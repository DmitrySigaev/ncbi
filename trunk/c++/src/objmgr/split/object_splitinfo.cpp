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
* Author:  Eugene Vasilchenko
*
* File Description:
*   Application for splitting blobs withing ID1 cache
*
* ===========================================================================
*/

#include <objmgr/split/object_splitinfo.hpp>

#include <serial/serial.hpp>

#include <objects/seq/Seq_annot.hpp>
#include <objects/seq/Annotdesc.hpp>
#include <objects/seq/Annot_descr.hpp>
#include <objects/seq/Bioseq.hpp>

#include <objects/seqset/Bioseq_set.hpp>

#include <objects/seqalign/Seq_align.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqres/Seq_graph.hpp>

#include <objmgr/split/asn_sizer.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

/////////////////////////////////////////////////////////////////////////////
// CSeq_annot_SplitInfo
/////////////////////////////////////////////////////////////////////////////


static CAsnSizer s_Sizer;


void CLocObjects_SplitInfo::Add(const CAnnotObject_SplitInfo& obj)
{
    m_Objects.push_back(obj);
    m_Location.Add(obj.m_Location);
    m_Size += obj.m_Size;
}


CNcbiOstream& CLocObjects_SplitInfo::Print(CNcbiOstream& out) const
{
    return out << m_Size;
}


CSeq_annot_SplitInfo::CSeq_annot_SplitInfo(void)
    : m_Id(0)
{
}


CSeq_annot_SplitInfo::CSeq_annot_SplitInfo(const CSeq_annot_SplitInfo& base,
                                           const CLocObjects_SplitInfo& objs)
    : m_Id(base.m_Id),
      m_Src_annot(base.m_Src_annot),
      m_Name(base.m_Name),
      m_LandmarkObjects(objs),
      m_Size(objs.m_Size),
      m_Location(objs.m_Location)
{
}


CAnnotName CSeq_annot_SplitInfo::GetName(const CSeq_annot& annot)
{
    CAnnotName ret;
    if ( annot.IsSetDesc() ) {
        string name;
        ITERATE ( CSeq_annot::TDesc::Tdata, it, annot.GetDesc().Get() ) {
            const CAnnotdesc& desc = **it;
            if ( desc.Which() == CAnnotdesc::e_Name ) {
                name = desc.GetName();
                break;
            }
        }
        ret.SetNamed(name);
    }
    return ret;
}


size_t CSeq_annot_SplitInfo::CountAnnotObjects(const CSeq_annot& annot)
{
    switch ( annot.GetData().Which() ) {
    case CSeq_annot::C_Data::e_Ftable:
        return annot.GetData().GetFtable().size();
    case CSeq_annot::C_Data::e_Align:
        return annot.GetData().GetAlign().size();
    case CSeq_annot::C_Data::e_Graph:
        return annot.GetData().GetGraph().size();
    }
    return 0;
}


void CSeq_annot_SplitInfo::SetSeq_annot(int id,
                                        const CSeq_annot& annot,
                                        const SSplitterParams& params)
{
    m_Id = id;
    s_Sizer.Set(annot, params);
    m_Size = CSize(s_Sizer);

    double ratio = m_Size.GetRatio();
    _ASSERT(!m_Src_annot);
    m_Src_annot.Reset(&annot);
    _ASSERT(!m_Name.IsNamed());
    m_Name = GetName(annot);
    switch ( annot.GetData().Which() ) {
    case CSeq_annot::TData::e_Ftable:
        ITERATE(CSeq_annot::C_Data::TFtable, it, annot.GetData().GetFtable()) {
            Add(CAnnotObject_SplitInfo(**it, ratio));
        }
        break;
    case CSeq_annot::TData::e_Align:
        ITERATE(CSeq_annot::C_Data::TAlign, it, annot.GetData().GetAlign()) {
            Add(CAnnotObject_SplitInfo(**it, ratio));
        }
        break;
    case CSeq_annot::TData::e_Graph:
        ITERATE(CSeq_annot::C_Data::TGraph, it, annot.GetData().GetGraph()) {
            Add(CAnnotObject_SplitInfo(**it, ratio));
        }
        break;
    }
}


bool CSeq_annot_SplitInfo::IsLandmark(const CAnnotObject_SplitInfo& obj) const
{
    if ( obj.m_ObjectType != CSeq_annot::C_Data::e_Ftable ) {
        return false;
    }
    const CObject& annot = *obj.m_Object;
    const CSeq_feat& feat = dynamic_cast<const CSeq_feat&>(annot);
    switch ( feat.GetData().GetSubtype() ) {
    case CSeqFeatData::eSubtype_gene:
        return true;
    default:
        return false;
    }
}


void CSeq_annot_SplitInfo::Add(const CAnnotObject_SplitInfo& obj)
{
    if ( IsLandmark(obj) ) {
        m_LandmarkObjects.Add(obj);
    }
    else {
        CSeq_id_Handle idh = obj.m_Location.GetSingleId();
        if ( idh ) {
            m_SimpleLocObjects[idh].Add(obj);
        }
        else {
            m_ComplexLocObjects.Add(obj);
        }
    }
    m_Location.Add(obj.m_Location);
}


CNcbiOstream& CSeq_annot_SplitInfo::Print(CNcbiOstream& out) const
{
    string name;
    if ( m_Name.IsNamed() ) {
        name = " \"" + m_Name.GetName() + "\"";
    }
    out << "Seq-annot" << name << ":";

    size_t lines = 0;

    if ( m_LandmarkObjects.size() ) {
        out << "\nLandmark: " << m_LandmarkObjects;
        ++lines;
    }

    if ( m_ComplexLocObjects.size() ) {
        out << "\n Complex: " << m_ComplexLocObjects;
        ++lines;
    }

    ITERATE ( TSimpleLocObjects, it, m_SimpleLocObjects ) {
        out << "\n  Simple: " << it->second;
        ++lines;
    }

    if ( lines > 1 ) {
        out << "\n   Total: " << m_Size;
    }
    return out << NcbiEndl;
}


CAnnotObject_SplitInfo::CAnnotObject_SplitInfo(const CSeq_feat& obj,
                                               double ratio)
    : m_ObjectType(CSeq_annot::C_Data::e_Ftable),
      m_Object(&obj),
      m_Size(s_Sizer.GetAsnSize(obj), ratio)
{
    m_Location.Add(obj);
}


CAnnotObject_SplitInfo::CAnnotObject_SplitInfo(const CSeq_graph& obj,
                                               double ratio)
    : m_ObjectType(CSeq_annot::C_Data::e_Graph),
      m_Object(&obj),
      m_Size(s_Sizer.GetAsnSize(obj), ratio)
{
    m_Location.Add(obj);
}


CAnnotObject_SplitInfo::CAnnotObject_SplitInfo(const CSeq_align& obj,
                                               double ratio)
    : m_ObjectType(CSeq_annot::C_Data::e_Align),
      m_Object(&obj),
      m_Size(s_Sizer.GetAsnSize(obj), ratio)
{
    m_Location.Add(obj);
}


CBioseq_SplitInfo::CBioseq_SplitInfo(void)
    : m_Id(0)
{
}


CBioseq_SplitInfo::~CBioseq_SplitInfo(void)
{
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.4  2004/01/07 17:36:27  vasilche
* Moved id2_split headers to include/objmgr/split.
* Fixed include path to genbank.
*
* Revision 1.3  2003/12/01 18:37:10  vasilche
* Separate different annotation types in split info to reduce memory usage.
*
* Revision 1.2  2003/11/26 23:04:59  vasilche
* Removed extra semicolons after BEGIN_SCOPE and END_SCOPE.
*
* Revision 1.1  2003/11/12 16:18:31  vasilche
* First implementation of ID2 blob splitter withing cache.
*
* ===========================================================================
*/
