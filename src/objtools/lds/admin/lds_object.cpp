/*  $Id$
 * ===========================================================================
 *
 *                            PUBLIC DOMAIN NOTICE
 *               National Center for Biotechnology Information
 *
 *  This software/database is a "United States Government Work" under the
 *  terms of the United States Copyright Act.  It was written as part of
 *  the author's official duties as a United States Government employee and
 *  thus cannot be copyrighted.  This software/database is freely available
 *  to the public for use. The National Library of Medicine and the U.S.
 *  Government have not placed any restriction on its use or reproduction.
 *
 *  Although all reasonable efforts have been taken to ensure the accuracy
 *  and reliability of the software and data, the NLM and the U.S.
 *  Government do not and cannot warrant the performance or results that
 *  may be obtained by using this software or data. The NLM and the U.S.
 *  Government disclaim all warranties, express or implied, including
 *  warranties of performance, merchantability or fitness for any particular
 *  purpose.
 *
 *  Please cite the author in any work or product based on this material.
 *
 * ===========================================================================
 *
 * Author: Anatoliy Kuznetsov
 *
 * File Description:  CLDS_Object implementation.
 *
 */


#include <objects/seq/Bioseq.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <objects/seqalign/Seq_align.hpp>
#include <objects/seqset/Seq_entry.hpp>

#include <bdb/bdb_cursor.hpp>

#include <objtools/readers/fasta.hpp>
#include <objtools/lds/admin/lds_object.hpp>
#include <objtools/lds/lds_set.hpp>
#include <objtools/lds/lds_util.hpp>

#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/util/sequence.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


CLDS_Object::CLDS_Object(SLDS_TablesCollection& db, 
                         const map<string, int>& obj_map)
: m_db(db),
  m_ObjTypeMap(obj_map),
  m_MaxObjRecId(0),
  m_MaxAnnRecId(0),
  m_TSE_Manager(0),
  m_Scope(0)
{}


CLDS_Object::~CLDS_Object()
{
    delete m_Scope;
    delete m_TSE_Manager;
}

void CLDS_Object::DeleteCascadeFiles(const CLDS_Set& file_ids, 
                                     CLDS_Set* objects_deleted,
                                     CLDS_Set* annotations_deleted)
{
    if (file_ids.empty())
        return;

    //
    //  Delete records from "object" table
    //
    {{
    CBDB_FileCursor cur(m_db.object_db); 
    cur.SetCondition(CBDB_FileCursor::eFirst); 
    while (cur.Fetch() == eBDB_Ok) { 
        int fid = m_db.object_db.file_id;
        if (fid && LDS_SetTest(file_ids, fid)) {
            int object_attr_id = m_db.object_db.object_attr_id;
            
            if (object_attr_id) {  // delete dependent object attr
                m_db.object_attr_db.object_attr_id = object_attr_id;
                m_db.object_attr_db.Delete();
            }
            int object_id = m_db.object_db.object_id;

            objects_deleted->insert(object_id);
            m_db.object_db.Delete();
        }
    }

    }}

    //
    // Delete "annot2obj"
    //
    {{
    CBDB_FileCursor cur(m_db.annot2obj_db); 
    cur.SetCondition(CBDB_FileCursor::eFirst); 
    while (cur.Fetch() == eBDB_Ok) { 
        int object_id = m_db.annot2obj_db.object_id;
        if (object_id && LDS_SetTest(*objects_deleted, object_id)) {
            m_db.annot2obj_db.Delete();
        }
    }

    }}

    //
    // Delete "annotation"
    //
    {{
    CBDB_FileCursor cur(m_db.annot_db); 
    cur.SetCondition(CBDB_FileCursor::eFirst); 
    while (cur.Fetch() == eBDB_Ok) { 
        int fid = m_db.object_db.file_id;
        if (fid && LDS_SetTest(file_ids, fid)) {
            int annot_id = m_db.annot_db.annot_id;
            annotations_deleted->insert(annot_id);
            m_db.annot_db.Delete();
        }
    }

    }}

}


void CLDS_Object::UpdateCascadeFiles(const CLDS_Set& file_ids)
{
    if (file_ids.empty())
        return;

    CLDS_Set objects_deleted;
    CLDS_Set annotations_deleted;
    DeleteCascadeFiles(file_ids, &objects_deleted, &annotations_deleted);


    ITERATE(CLDS_Set, it, file_ids) {
        int fid = *it;
        m_db.file_db.file_id = fid;

        if (m_db.file_db.Fetch() == eBDB_Ok) {
            string fname = m_db.file_db.file_name;
            CFormatGuess::EFormat format = 
                (CFormatGuess::EFormat)(int)m_db.file_db.format;
    
            LOG_POST(Info << "<< Updating file >>: " << fname);

            UpdateFileObjects(fid, fname, format);
        }
    } // ITERATE
}


void CLDS_Object::UpdateFileObjects(int file_id, 
                                    const string& file_name, 
                                    CFormatGuess::EFormat format)
{
    FindMaxObjRecId();
    FindMaxAnnRecId();

    if (format == CFormatGuess::eBinaryASN ||
        format == CFormatGuess::eTextASN ||
        format == CFormatGuess::eXml) {

        LOG_POST(Info << "Scanning file: " << file_name);

        CLDS_CoreObjectsReader sniffer;
        ESerialDataFormat stream_format = FormatGuess2Serial(format);

        auto_ptr<CObjectIStream> 
          input(CObjectIStream::Open(file_name, stream_format));
        sniffer.Probe(*input);

        CLDS_CoreObjectsReader::TObjectVector& obj_vector 
                                                = sniffer.GetObjectsVector();

        for (unsigned int i = 0; i < obj_vector.size(); ++i) {
            CLDS_CoreObjectsReader::SObjectDetails* obj_info = &obj_vector[i];
            // If object is not in the database yet.
            if (obj_info->ext_id == 0) {
                SaveObject(file_id, &sniffer, obj_info);
            }
        }

    } else if ( format == CFormatGuess::eFasta ){
        CNcbiIfstream input(file_name.c_str(), 
                            IOS_BASE::in | IOS_BASE::binary);

        SFastaFileMap fmap;
    
        ReadFastaFileMap(&fmap, input);
        int type_id;
        {{
        map<string, int>::const_iterator it = m_ObjTypeMap.find("FastaEntry");
        _ASSERT(it != m_ObjTypeMap.end());
        type_id = it->second;
        }}

        SFastaFileMap::TMapVector::const_iterator it;
        for (it = fmap.file_map.begin(); it < fmap.file_map.end(); ++it) {
            SaveObject(file_id, 
                       it->seq_id, 
                       it->description, 
                       it->stream_offset,
                       type_id);
        }
        
    } else {
        LOG_POST(Info << "Unsupported file format: " << file_name);
    }


}


int CLDS_Object::SaveObject(int file_id,
                            const string& seq_id,
                            const string& description,
                            size_t offset,
                            int type_id)
{
    ++m_MaxObjRecId;

    m_db.object_attr_db.object_attr_id = m_MaxObjRecId;
    m_db.object_attr_db.object_title = description;
    EBDB_ErrCode err = m_db.object_attr_db.Insert();
    BDB_CHECK(err, "LDS::ObjectAttribute");

    m_db.object_db.object_id = m_MaxObjRecId;
    m_db.object_db.file_id = file_id;
    m_db.object_db.seqlist_id = 0;
    m_db.object_db.object_type = type_id;
    m_db.object_db.file_offset = offset;
    m_db.object_db.object_attr_id = m_MaxObjRecId;
    m_db.object_db.TSE_object_id = 0;
    m_db.object_db.parent_object_id = 0;
    
    string ups = seq_id; 
    NStr::ToUpper(ups);
    m_db.object_db.primary_seqid = ups;

    LOG_POST(Info << "Saving Fasta object: " << seq_id);

    err = m_db.object_db.Insert();
    BDB_CHECK(err, "LDS::Object");

    return m_MaxObjRecId;
}


int CLDS_Object::SaveObject(int file_id, 
                            CLDS_CoreObjectsReader* sniffer,
                            CLDS_CoreObjectsReader::SObjectDetails* obj_info)
{
    int top_level_id, parent_id;

    _ASSERT(obj_info->ext_id == 0);  // Making sure the object is not in the DB yet

    if (obj_info->is_top_level) {
        top_level_id = parent_id = 0;
    } else {
        // Find the direct parent
        {{

        CLDS_CoreObjectsReader::SObjectDetails* parent_obj_info 
                    = sniffer->FindObjectInfo(obj_info->parent_offset);
        _ASSERT(parent_obj_info);
        if (parent_obj_info->ext_id == 0) { // not yet in the database
            // Recursively save the parent
            parent_id = SaveObject(file_id, sniffer, parent_obj_info);
        } else {
            parent_id = parent_obj_info->ext_id;
        }

        }}

        // Find the top level grand parent
        {{

        CLDS_CoreObjectsReader::SObjectDetails* top_obj_info 
                    = sniffer->FindObjectInfo(obj_info->top_level_offset);
        _ASSERT(top_obj_info);
        if (top_obj_info->ext_id == 0) { // not yet in the database
            // Recursively save the parent
            top_level_id = SaveObject(file_id, sniffer, top_obj_info);
        } else {
            top_level_id = top_obj_info->ext_id;
        }

        }}

    }

    const string& type_name = obj_info->info.GetTypeInfo()->GetName();

    map<string, int>::const_iterator it = m_ObjTypeMap.find(type_name);
    if (it == m_ObjTypeMap.end()) {
        LOG_POST(Info << "Unrecognized type: " << type_name);
        return 0;                
    }
    int type_id = it->second;


    string id_str;
    string molecule_title;
    bool is_object = IsObject(*obj_info, &id_str, &molecule_title);
    if (is_object) {
        string all_seq_id; // Space separated list of seq_ids
        const CBioseq* bioseq = CType<CBioseq>().Get(obj_info->info);
        if (bioseq) {
            const CBioseq::TId&  id_list = bioseq->GetId();
            ITERATE(CBioseq::TId, it, id_list) {
                const CSeq_id* seq_id = *it;
                if (seq_id) {
                    all_seq_id.append(seq_id->AsFastaString());
                    all_seq_id.append(" ");
                }
            }
        }

        m_db.object_db.primary_seqid = NStr::ToUpper(id_str);

        ++m_MaxObjRecId;

        obj_info->ext_id = m_MaxObjRecId; // Keep external id for the next scan

        m_db.object_attr_db.object_attr_id = m_MaxObjRecId;
        m_db.object_attr_db.object_title = molecule_title;
        m_db.object_attr_db.seq_ids = NStr::ToUpper(all_seq_id);
        EBDB_ErrCode err = m_db.object_attr_db.Insert();
        BDB_CHECK(err, "LDS::ObjectAttr");

        m_db.object_db.object_id = m_MaxObjRecId;
        m_db.object_db.file_id = file_id;
        m_db.object_db.seqlist_id = 0;  // TODO:
        m_db.object_db.object_type = type_id;
        m_db.object_db.file_offset = obj_info->offset;
        m_db.object_db.object_attr_id = m_MaxObjRecId; 
        m_db.object_db.TSE_object_id = top_level_id;
        m_db.object_db.parent_object_id = parent_id;

//        LOG_POST(Info << "Saving object: " << type_name << " " << id_str);

        err = m_db.object_db.Insert();
        BDB_CHECK(err, "LDS::Object");

    } else {
        
        ++m_MaxAnnRecId;
        
        obj_info->ext_id = m_MaxAnnRecId; // Keep external id for the next scan
                
        m_db.annot_db.annot_id = m_MaxAnnRecId;
        m_db.annot_db.file_id = file_id;
        m_db.annot_db.annot_type = type_id;
        m_db.annot_db.file_offset = obj_info->offset;

//        LOG_POST(Info << "Saving annotation: " << type_name << " " << id_str);

        EBDB_ErrCode err = m_db.annot_db.Insert();
        BDB_CHECK(err, "LDS::Annotation");

        m_db.annot2obj_db.object_id = parent_id;
        m_db.annot2obj_db.annot_id = m_MaxAnnRecId;

        err = m_db.annot2obj_db.Insert();
        BDB_CHECK(err, "LDS::Annot2Obj");

    }

    return obj_info->ext_id;
}


bool CLDS_Object::IsObject(const CLDS_CoreObjectsReader::SObjectDetails& parse_info,
                           string* object_str_id,
                           string* object_title)
{
    *object_title = "";
    *object_str_id = "";

    if (parse_info.is_top_level) {
        delete m_Scope; m_Scope = 0;
        delete m_TSE_Manager; m_TSE_Manager = 0;

        CSeq_entry* seq_entry = CType<CSeq_entry>().Get(parse_info.info);

        if (seq_entry) {
            m_TSE_Manager = new CObjectManager;
            m_Scope = new CScope(*m_TSE_Manager);

            m_Scope->AddTopLevelSeqEntry(*seq_entry);
            return true;
        }
    }

    const CBioseq* bioseq = CType<CBioseq>().Get(parse_info.info);
    if (bioseq) {
        const CSeq_id* seq_id = bioseq->GetFirstId();
        if (seq_id) {
            *object_str_id = seq_id->AsFastaString();
        } else {
            *object_str_id = "";
        }

        if (m_Scope) { // we are under OM here
            CBioseq_Handle bio_handle = m_Scope->GetBioseqHandle(*bioseq);
            if (bio_handle) {
                *object_title = sequence::GetTitle(bio_handle);
                //LOG_POST(Info << "object title: " << *object_title);
            } else {
                // the last resort
                bioseq->GetLabel(object_title, CBioseq::eBoth);
            }
            
        } else {  // non-OM controlled object
            bioseq->GetLabel(object_title, CBioseq::eBoth);
        }
    } else {
        const CSeq_annot* annot = 
            CType<CSeq_annot>().Get(parse_info.info);
        if (annot) {
            *object_str_id = "";
            return false;
        } else {
            const CSeq_align* align =  
                CType<CSeq_align>().Get(parse_info.info);
            if (align) {
                *object_str_id = "";
                return false;
            }
        }
    }
    return true;
}


int CLDS_Object::FindMaxObjRecId()
{
    if (m_MaxObjRecId) {
        return m_MaxObjRecId;
    }

    LDS_GETMAXID(m_MaxObjRecId, m_db.object_db, object_id);
    return m_MaxObjRecId;
}


int CLDS_Object::FindMaxAnnRecId()
{
    if (m_MaxAnnRecId) {
        return m_MaxAnnRecId;
    }

    LDS_GETMAXID(m_MaxAnnRecId, m_db.object_db, object_id);
    return m_MaxObjRecId;
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.8  2003/06/26 16:22:15  kuznets
 * Uppercased all sequence ids before writing into the database.
 *
 * Revision 1.7  2003/06/16 16:24:43  kuznets
 * Fixed #include paths (lds <-> lds_admin separation)
 *
 * Revision 1.6  2003/06/16 15:40:21  kuznets
 * Fixed a bug with collecting all seq_ids from a bioseq
 *
 * Revision 1.5  2003/06/13 16:00:30  kuznets
 * Improved work with sequence ids. Now it keeps all sequence ids bioseq has
 *
 * Revision 1.4  2003/06/10 19:00:32  kuznets
 * Code clean-up
 *
 * Revision 1.3  2003/06/06 20:03:54  kuznets
 * Reflecting new location of fasta reader
 *
 * Revision 1.2  2003/06/04 16:38:45  kuznets
 * Implemented OM-based bioseq title extraction (should work better than
 * CBioseq::GetTitle())
 *
 * Revision 1.1  2003/06/03 14:13:25  kuznets
 * Initial revision
 *
 *
 * ===========================================================================
 */

