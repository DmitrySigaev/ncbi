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
* Author: Aleksey Grichenko
*
* File Description:
*       CHandleRange:
*       Internal class to be used instead of CSeq_loc
*       for better performance.
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.7  2002/05/24 14:58:55  grichenk
* Fixed Empty() for unsigned intervals
* SerialAssign<>() -> CSerialObject::Assign()
* Improved performance for eResolve_None case
*
* Revision 1.6  2002/05/09 14:18:55  grichenk
* Fixed "unused variable" warnings
*
* Revision 1.5  2002/04/22 20:05:35  grichenk
* +MergeRange()
*
* Revision 1.4  2002/02/21 19:27:05  grichenk
* Rearranged includes. Added scope history. Added searching for the
* best seq-id match in data sources and scopes. Updated tests.
*
* Revision 1.3  2002/02/15 20:35:38  gouriano
* changed implementation of HandleRangeMap
*
* Revision 1.2  2002/01/23 21:59:31  grichenk
* Redesigned seq-id handles and mapper
*
* Revision 1.1  2002/01/11 19:06:19  gouriano
* restructured objmgr
*
*
* ===========================================================================
*/

#include "handle_range.hpp"


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

////////////////////////////////////////////////////////////////////
//
//
//


CHandleRange::CHandleRange(const CSeq_id_Handle& handle)
    : m_Handle(handle)
{
}


CHandleRange::CHandleRange(const CHandleRange& hrange)
{
    *this = hrange;
}


CHandleRange::~CHandleRange(void)
{
}


CHandleRange& CHandleRange::operator= (const CHandleRange& hrange)
{
    m_Handle = hrange.m_Handle;
    m_Ranges.assign(hrange.m_Ranges.begin(), hrange.m_Ranges.end());
    return *this;
}


bool operator< (CHandleRange::TRangeWithStrand r1,
                CHandleRange::TRangeWithStrand r2)
{
    if (r1.first < r2.first) 
        return true;
    else if (r1.first == r2.first)
        return r1.second < r2.second;
    else
        return false;
}


void CHandleRange::AddRange(TRange range, ENa_strand strand)
{
    m_Ranges.push_back(TRanges::value_type(range, strand));
    m_Ranges.sort();
}


void CHandleRange::MergeRange(TRange range, ENa_strand /*strand*/)
{
    TRange mrg = range;
    non_const_iterate ( TRanges, it, m_Ranges ) {
        // Find intersecting intervals, discard strand information
        // Also merge adjacent ranges, prevent merging whole-to + whole-from
        if ( it->first.IntersectingWith(mrg)  ||
            (it->first.GetFrom() == mrg.GetTo()+1 && !mrg.IsWholeTo()) ||
            (it->first.GetTo()+1 == mrg.GetFrom() && !mrg.IsWholeFrom()) ) {
            // Remove the intersecting interval, update the merged range.
            // We assume that WholeFrom is less than any non-whole value
            // and WholeTo is greater than any non-whole value.
            mrg += it->first;
            it = m_Ranges.erase(it);
        }
    }
    m_Ranges.push_back(TRangeWithStrand(mrg, eNa_strand_unknown));
    m_Ranges.sort();
}


CHandleRange::TRange CHandleRange::GetOverlappingRange(void) const
{
    TRange range = TRange::GetEmpty();
    iterate ( TRanges, it, m_Ranges ) {
        x_CombineRanges(range, it->first);
    }
    return range;
}


bool CHandleRange::IntersectingWith(const CHandleRange& hloc) const
{
    if ( hloc.m_Handle != m_Handle )
        return false;
    //### Optimize this
    iterate ( TRanges, it1, hloc.GetRanges() ) {
        iterate ( TRanges, it2, m_Ranges ) {
            if ( x_IntersectingStrands(it1->second, it2->second) )
                return it1->first.IntersectingWith(it2->first);
        }
    }
    return false;
}


void CHandleRange::x_CombineRanges(TRange& dest, const TRange& src)
{
    if ( src.Empty() ) {
        return;
    }
    if ( !dest.IsWholeFrom() ) {
        if (dest.GetFrom() > src.GetFrom()  ||
            src.IsWholeFrom()  ||
            dest.IsEmptyFrom()) {
            dest.SetFrom(src.GetFrom());
        }
    }
    if ( !dest.IsWholeTo() ) {
        if (dest.GetTo() < src.GetTo()  ||
            src.IsWholeTo()  ||
            dest.IsEmptyTo()) {
            dest.SetTo(src.GetTo());
        }
    }
}


bool CHandleRange::x_IntersectingStrands(ENa_strand str1, ENa_strand str2)
{
    return
        str1 == eNa_strand_unknown // str1 includes anything
        ||
        str2 == eNa_strand_unknown // str2 includes anything
        ||
        str1 == str2;              // accept only equal strands
    //### Not sure about "eNa_strand_both includes eNa_strand_plus" etc.
}


END_SCOPE(objects)
END_NCBI_SCOPE
