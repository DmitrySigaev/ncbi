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
* Author: Aleksey Grichenko, Eugene Vasilchenko
*
* File Description:
*   Object manager iterators
*
*/

#include <objmgr/align_ci.hpp>

#include <objects/seqalign/Seq_align.hpp>

#include <objmgr/impl/annot_object.hpp>
#include <objmgr/impl/seq_loc_cvt.hpp>
#include <objmgr/impl/seq_align_mapper.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)



CAlign_CI::CAlign_CI(CScope& scope,
                     const CSeq_loc& loc,
                     SAnnotSelector::EOverlapType overlap_type,
                     SAnnotSelector::EResolveMethod resolve)
    : CAnnotTypes_CI(CSeq_annot::C_Data::e_Align,
                     scope, loc,
                     overlap_type,
                     resolve)
{
}


CAlign_CI::CAlign_CI(const CBioseq_Handle& bioseq, TSeqPos start, TSeqPos stop,
                     SAnnotSelector::EOverlapType overlap_type,
                     SAnnotSelector::EResolveMethod resolve)
    : CAnnotTypes_CI(CSeq_annot::C_Data::e_Align,
                     bioseq, start, stop,
                     overlap_type,
                     resolve)
{
}


CAlign_CI::~CAlign_CI(void)
{
}


CAlign_CI& CAlign_CI::operator++ (void)
{
    Next();
    m_MappedAlign.Reset();
    return *this;
}


CAlign_CI& CAlign_CI::operator-- (void)
{
    Prev();
    m_MappedAlign.Reset();
    return *this;
}


const CSeq_align& CAlign_CI::operator* (void) const
{
    const CAnnotObject_Ref& annot = Get();
    _ASSERT(annot.IsAlign());
    if (!m_MappedAlign) {
        if ( annot.IsMapped() ) {
            m_MappedAlign = annot.GetMappedSeq_align_Mapper().GetDstAlign();
        }
        else {
            m_MappedAlign.Reset(&annot.GetAlign());
        }
    }
    return *m_MappedAlign;
}


const CSeq_align* CAlign_CI::operator-> (void) const
{
    const CAnnotObject_Ref& annot = Get();
    _ASSERT(annot.IsAlign());
    if (!m_MappedAlign) {
        if ( annot.IsMapped() ) {
            m_MappedAlign = annot.GetMappedSeq_align_Mapper().GetDstAlign();
        }
        else {
            m_MappedAlign.Reset(&annot.GetAlign());
        }
    }
    return m_MappedAlign.GetPointer();
}


const CSeq_align& CAlign_CI::GetOriginalSeq_align(void) const
{
    const CAnnotObject_Ref& annot = Get();
    _ASSERT(annot.IsAlign());
    return annot.GetAlign();
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.24  2004/04/05 15:56:14  grichenk
* Redesigned CAnnotTypes_CI: moved all data and data collecting
* functions to CAnnotDataCollector. CAnnotTypes_CI is no more
* inherited from SAnnotSelector.
*
* Revision 1.23  2004/03/30 15:42:33  grichenk
* Moved alignment mapper to separate file, added alignment mapping
* to CSeq_loc_Mapper.
*
* Revision 1.22  2004/03/16 15:47:27  vasilche
* Added CBioseq_set_Handle and set of EditHandles
*
* Revision 1.21  2004/01/29 15:44:46  vasilche
* Fixed mapped align when it's not mapped.
*
* Revision 1.20  2004/01/28 20:54:35  vasilche
* Fixed mapping of annotations.
*
* Revision 1.19  2004/01/23 16:14:47  grichenk
* Implemented alignment mapping
*
* Revision 1.18  2003/08/14 20:05:19  vasilche
* Simple SNP features are stored as table internally.
* They are recreated when needed using CFeat_CI.
*
* Revision 1.17  2003/06/02 16:06:37  dicuccio
* Rearranged src/objects/ subtree.  This includes the following shifts:
*     - src/objects/asn2asn --> arc/app/asn2asn
*     - src/objects/testmedline --> src/objects/ncbimime/test
*     - src/objects/objmgr --> src/objmgr
*     - src/objects/util --> src/objmgr/util
*     - src/objects/alnmgr --> src/objtools/alnmgr
*     - src/objects/flat --> src/objtools/flat
*     - src/objects/validator --> src/objtools/validator
*     - src/objects/cddalignview --> src/objtools/cddalignview
* In addition, libseq now includes six of the objects/seq... libs, and libmmdb
* replaces the three libmmdb? libs.
*
* Revision 1.16  2003/04/24 16:12:38  vasilche
* Object manager internal structures are splitted more straightforward.
* Removed excessive header dependencies.
*
* Revision 1.15  2003/03/18 21:48:30  grichenk
* Removed obsolete class CAnnot_CI
*
* Revision 1.14  2003/02/24 18:57:22  vasilche
* Make feature gathering in one linear pass using CSeqMap iterator.
* Do not use feture index by sub locations.
* Sort features at the end of gathering in one vector.
* Extracted some internal structures and classes in separate header.
* Delay creation of mapped features.
*
* Revision 1.13  2003/02/13 14:34:34  grichenk
* Renamed CAnnotObject -> CAnnotObject_Info
* + CSeq_annot_Info and CAnnotObject_Ref
* Moved some members of CAnnotObject to CSeq_annot_Info
* and CAnnotObject_Ref.
* Added feat/align/graph to CAnnotObject_Info map
* to CDataSource.
*
* Revision 1.12  2002/12/24 15:42:45  grichenk
* CBioseqHandle argument to annotation iterators made const
*
* Revision 1.11  2002/12/06 15:35:59  grichenk
* Added overlap type for annot-iterators
*
* Revision 1.10  2002/11/04 21:29:11  grichenk
* Fixed usage of const CRef<> and CRef<> constructor
*
* Revision 1.9  2002/07/08 20:51:00  grichenk
* Moved log to the end of file
* Replaced static mutex (in CScope, CDataSource) with the mutex
* pool. Redesigned CDataSource data locking.
*
* Revision 1.8  2002/05/06 03:28:46  vakatov
* OM/OM1 renaming
*
* Revision 1.7  2002/05/03 21:28:08  ucko
* Introduce T(Signed)SeqPos.
*
* Revision 1.6  2002/04/05 21:26:19  grichenk
* Enabled iteration over annotations defined on segments of a
* delta-sequence.
*
* Revision 1.5  2002/03/05 16:08:13  grichenk
* Moved TSE-restriction to new constructors
*
* Revision 1.4  2002/03/04 15:07:47  grichenk
* Added "bioseq" argument to CAnnotTypes_CI constructor to iterate
* annotations from a single TSE.
*
* Revision 1.3  2002/02/21 19:27:04  grichenk
* Rearranged includes. Added scope history. Added searching for the
* best seq-id match in data sources and scopes. Updated tests.
*
* Revision 1.2  2002/01/16 16:25:56  gouriano
* restructured objmgr
*
* Revision 1.1  2002/01/11 19:06:15  gouriano
* restructured objmgr
*
*
* ===========================================================================
*/
