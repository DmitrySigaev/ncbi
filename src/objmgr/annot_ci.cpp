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

#include <objmgr/impl/annot_object.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)



CAnnot_CI::CAnnot_CI(CScope& scope,
                     const CSeq_loc& loc,
                     SAnnotSelector::EOverlapType overlap_type,
                     EResolveMethod resolve,
                     const CSeq_entry* entry)
    : CAnnotTypes_CI(scope, loc,
          SAnnotSelector(CSeq_annot::C_Data::e_not_set),
          overlap_type, resolve, entry)
{
    return;
}


CAnnot_CI::CAnnot_CI(const CBioseq_Handle& bioseq, TSeqPos start, TSeqPos stop,
                     SAnnotSelector::EOverlapType overlap_type,
                     EResolveMethod resolve, const CSeq_entry* entry)
    : CAnnotTypes_CI(bioseq, start, stop,
          SAnnotSelector(CSeq_annot::C_Data::e_not_set),
          overlap_type, resolve, entry)
{
    return;
}


CAnnot_CI::~CAnnot_CI(void)
{
    return;
}


/*
const CSeq_align& CAlign_CI::operator* (void) const
{
    const CAnnotObject_Ref& annot = Get();
    _ASSERT(annot.Get().IsAlign());
    return annot.Get().GetAlign();
}


const CSeq_align* CAlign_CI::operator-> (void) const
{
    const CAnnotObject_Ref& annot = Get();
    _ASSERT(annot.Get().IsAlign());
    return &annot.Get().GetAlign();
}
*/

END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.25  2003/08/15 15:22:00  grichenk
* Initial revision
*
*
* ===========================================================================
*/
