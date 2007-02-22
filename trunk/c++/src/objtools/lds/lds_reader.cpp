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
 * File Description:  LDS reader implementation.
 *
 */
#include <ncbi_pch.hpp>
#include <corelib/ncbifile.hpp>

#include <util/format_guess.hpp>
#include <bdb/bdb_cursor.hpp>

#include <serial/objistr.hpp>

#include <objects/seq/Bioseq.hpp>
#include <objects/seqalign/Seq_align.hpp>
#include <objects/seqset/Bioseq_set.hpp>

#include <objmgr/util/obj_sniff.hpp>

#include <objtools/readers/fasta.hpp>
#include <objtools/lds/lds_reader.hpp>
#include <objtools/lds/lds_db.hpp>
#include <objtools/lds/lds_expt.hpp>

#include <objtools/lds/lds_query.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


CRef<CSeq_entry> 
LDS_LoadTSE(CLDS_Database& lds_db, 
            int            object_id,
            bool           trace_to_top)
{
    const map<string, int>& type_map = lds_db.GetObjTypeMap();

    CLDS_Query query(lds_db);
    CLDS_Query::SObjectDescr obj_descr = 
        query.GetObjectDescr(type_map, object_id, trace_to_top);

    if (!obj_descr.is_object || obj_descr.id <= 0) {
        return CRef<CSeq_entry>();
    }

    CNcbiIfstream in(obj_descr.file_name.c_str(), 
                     IOS_BASE::in | IOS_BASE::binary);
    if (!in.is_open()) {
        string msg = "Cannot open file:";
        msg.append(obj_descr.file_name);
        LDS_THROW(eFileNotFound, msg);
    }

    switch (obj_descr.format) {
    case CFormatGuess::eFasta:
        in.seekg(obj_descr.pos);
        {{
        CStreamLineReader lr(in);
        CFastaReader      fr(lr, 
                             CFastaReader::fAssumeNuc |
                             CFastaReader::fOneSeq    |
                             CFastaReader::fParseRawID);
        return fr.ReadOneSeq();
        }}
    case CFormatGuess::eTextASN:
    case CFormatGuess::eXml:
    case CFormatGuess::eBinaryASN:
        {
        in.seekg(obj_descr.pos);
        auto_ptr<CObjectIStream> 
          is(CObjectIStream::Open(FormatGuess2Serial(obj_descr.format), in));
        if (obj_descr.type_str == "Bioseq") {
            //
            // If object is a bare Bioseq: read it and 
            // construct a Seq_entry on it
            //
            CRef<CBioseq> bioseq(new CBioseq());
            is->Read(ObjectInfo(*bioseq));
            CRef<CSeq_entry> seq_entry(new CSeq_entry());
            seq_entry->SetSeq(*bioseq);
            return seq_entry;
        } else 
        if (obj_descr.type_str == "Seq-entry") {
            CRef<CSeq_entry> seq_entry(new CSeq_entry());
            is->Read(ObjectInfo(*seq_entry));
            return seq_entry;
        } else 
        if (obj_descr.type_str == "Bioseq-set") {
            CRef<CBioseq_set> bioseq_set(new CBioseq_set());
            is->Read(ObjectInfo(*bioseq_set));

            CRef<CSeq_entry> seq_entry(new CSeq_entry());
            CSeq_entry::TSet& s = *bioseq_set;
            seq_entry->SetSet(s);
            return seq_entry;
        }
        else {
            LDS_THROW(eInvalidDataType, "Non Seq-entry object type");
        }

        }
        break;
    default:
        LDS_THROW(eNotImplemented, "Not implemeneted yet.");
    }

}


CRef<CSeq_annot> LDS_LoadAnnot(SLDS_TablesCollection& lds_db, 
                               const CLDS_Query::SObjectDescr& obj_descr)
{
    CNcbiIfstream in(obj_descr.file_name.c_str(), 
                     IOS_BASE::in | IOS_BASE::binary);
    if (!in.is_open()) {
        string msg = "Cannot open file:";
        msg.append(obj_descr.file_name);
        LDS_THROW(eFileNotFound, msg);
    }

    switch (obj_descr.format) {
    case CFormatGuess::eTextASN:
    case CFormatGuess::eXml:
    case CFormatGuess::eBinaryASN:
        {
        in.seekg(obj_descr.pos);
        auto_ptr<CObjectIStream> 
          is(CObjectIStream::Open(FormatGuess2Serial(obj_descr.format), in));
        if (obj_descr.type_str == "Seq-annot") {
            //
            // If object is a bare Bioseq: read it and 
            // construct a Seq_entry on it
            //
            CRef<CSeq_annot> annot(new CSeq_annot());
            is->Read(ObjectInfo(*annot));
            return annot;
        } else 
        if (obj_descr.type_str == "Seq-align") {
            CRef<CSeq_align> align(new CSeq_align());
            is->Read(ObjectInfo(*align));
            CRef<CSeq_annot> annot(new CSeq_annot());
            annot->SetData().SetAlign().push_back(align);
            return annot;
        } else {
            LDS_THROW(eInvalidDataType, 
                      "Non Seq-aanot compatible object type");
        }

        }
        break;
    default:
        LDS_THROW(eNotImplemented, "Invalid file format");
    }
    
}

END_SCOPE(objects)
END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.11  2005/10/20 15:34:08  kuznets
 * Implemented duplicate id check
 *
 * Revision 1.10  2005/10/12 12:18:15  kuznets
 * Use 64-bit file sizes and offsets
 *
 * Revision 1.9  2005/09/30 13:55:21  kuznets
 * Load only one entry from fasta file, not all sequences
 *
 * Revision 1.8  2005/01/13 17:38:20  kuznets
 * LoadTSE added trace to top level entry option
 *
 * Revision 1.7  2005/01/11 17:59:54  kuznets
 * Added missing code to read bioseq-set
 *
 * Revision 1.6  2004/05/21 21:42:55  gorelenk
 * Added PCH ncbi_pch.hpp
 *
 * Revision 1.5  2003/07/14 19:47:01  kuznets
 * + new annotation reader
 *
 * Revision 1.4  2003/07/10 20:10:09  kuznets
 * Code clean up
 *
 * Revision 1.3  2003/06/23 18:57:31  kuznets
 * LDS_LoadTSE corrected to read XML serialization format.
 *
 * Revision 1.2  2003/06/09 18:06:33  kuznets
 * CSeq_entry reader changed to handle top level CBioseqs.
 *
 * Revision 1.1  2003/06/06 20:02:34  kuznets
 * Initial revision
 *
 *
 * ===========================================================================
 */
