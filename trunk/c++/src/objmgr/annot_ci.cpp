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

#include <objmgr/annot_ci.hpp>
#include <objmgr/seq_entry_handle.hpp>
#include <objmgr/seq_annot_handle.hpp>
#include <objmgr/impl/annot_object.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)



CAnnot_CI::CAnnot_CI(void)
{
}


CAnnot_CI::CAnnot_CI(const CAnnot_CI& iter)
    : CAnnotTypes_CI(iter)
{
    m_SeqAnnotSet = iter.m_SeqAnnotSet;
    if (iter) {
        m_Iterator = m_SeqAnnotSet.find(*iter.m_Iterator);
    }
    else {
        m_Iterator = m_SeqAnnotSet.end();
    }
}


CAnnot_CI::CAnnot_CI(CScope& scope, const CSeq_loc& loc,
                     const SAnnotSelector& sel)
    : CAnnotTypes_CI(CSeq_annot::C_Data::e_not_set,
                     scope, loc,
                     SAnnotSelector()
                     .SetNoMapping(true)
                     .SetSortOrder(SAnnotSelector::eSortOrder_None))
{
    x_Collect();
}



CAnnot_CI::CAnnot_CI(const CBioseq_Handle& bioseq,
                     TSeqPos start, TSeqPos stop,
                     const SAnnotSelector& sel)
    : CAnnotTypes_CI(CSeq_annot::C_Data::e_not_set,
                     bioseq, start, stop,
                     SAnnotSelector()
                     .SetNoMapping(true)
                     .SetSortOrder(SAnnotSelector::eSortOrder_None))
{
    x_Collect();
}


CAnnot_CI& CAnnot_CI::operator= (const CAnnot_CI& iter)
{
    CAnnotTypes_CI::operator=(iter);
    m_SeqAnnotSet.clear();
    m_SeqAnnotSet = iter.m_SeqAnnotSet;
    if (iter) {
        m_Iterator = m_SeqAnnotSet.find(*iter.m_Iterator);
    }
    else {
        m_Iterator = m_SeqAnnotSet.end();
    }
    return *this;
}


CAnnot_CI::CAnnot_CI(CScope& scope,
                     const CSeq_loc& loc,
                     SAnnotSelector::EOverlapType overlap_type,
                     SAnnotSelector::EResolveMethod resolve)
    : CAnnotTypes_CI(CSeq_annot::C_Data::e_not_set,
                     scope, loc,
                     SAnnotSelector()
                     .SetNoMapping(true)
                     .SetSortOrder(SAnnotSelector::eSortOrder_None)
                     .SetOverlapType(overlap_type)
                     .SetResolveMethod(resolve))
{
    x_Collect();
}


CAnnot_CI::CAnnot_CI(const CBioseq_Handle& bioseq, TSeqPos start, TSeqPos stop,
                     SAnnotSelector::EOverlapType overlap_type,
                     SAnnotSelector::EResolveMethod resolve)
    : CAnnotTypes_CI(CSeq_annot::C_Data::e_not_set,
                     bioseq, start, stop,
                     SAnnotSelector()
                     .SetNoMapping(true)
                     .SetSortOrder(SAnnotSelector::eSortOrder_None)
                     .SetOverlapType(overlap_type)
                     .SetResolveMethod(resolve))
{
    x_Collect();
}


CAnnot_CI::~CAnnot_CI(void)
{
    return;
}


void CAnnot_CI::x_Collect(void)
{
    while ( IsValid() ) {
        if (Get().GetObjectType() != CAnnotObject_Ref::eType_Seq_annot_Info) {
            Next();
            continue;
        }
        CSeq_annot_Handle h(GetScope(),
                            Get().GetAnnotObject_Info().GetSeq_annot_Info());
        m_SeqAnnotSet.insert(h);
        Next();
    }
    m_Iterator = m_SeqAnnotSet.begin();
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.30  2004/04/05 15:56:14  grichenk
* Redesigned CAnnotTypes_CI: moved all data and data collecting
* functions to CAnnotDataCollector. CAnnotTypes_CI is no more
* inherited from SAnnotSelector.
*
* Revision 1.29  2004/03/16 15:47:27  vasilche
* Added CBioseq_set_Handle and set of EditHandles
*
* Revision 1.28  2003/09/11 17:45:07  grichenk
* Added adaptive-depth option to annot-iterators.
*
* Revision 1.27  2003/08/27 14:29:52  vasilche
* Reduce object allocations in feature iterator.
*
* Revision 1.26  2003/08/22 15:00:49  grichenk
* Redesigned CAnnot_CI to iterate over seq-annots, containing
* given location.
*
* Revision 1.25  2003/08/15 15:22:00  grichenk
* Initial revision
*
*
* ===========================================================================
*/
