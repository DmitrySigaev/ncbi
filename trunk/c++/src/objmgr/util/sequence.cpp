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
* Author:  Clifford Clausen
*
* File Description:
*   Sequence utilities requiring CScope
*/

#include <serial/iterator.hpp>

#include <objects/objmgr/object_manager.hpp>
#include <objects/objmgr/scope.hpp>
#include <objects/objmgr/seq_vector.hpp>
#include <objects/objmgr/seq_vector_ci.hpp>
#include <objects/objmgr/seqdesc_ci.hpp>
#include <objects/objmgr/feat_ci.hpp>

#include <objects/general/Int_fuzz.hpp>

#include <objects/seq/Bioseq.hpp>
#include <objects/seq/Delta_ext.hpp>
#include <objects/seq/Delta_seq.hpp>
#include <objects/seq/MolInfo.hpp>
#include <objects/seq/Seg_ext.hpp>
#include <objects/seq/Seq_ext.hpp>
#include <objects/seq/Seq_inst.hpp>
#include <objects/seq/Seq_literal.hpp>

#include <objects/seqloc/Packed_seqpnt.hpp>
#include <objects/seqloc/Seq_bond.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_loc_equiv.hpp>
#include <objects/seqloc/Seq_loc_mix.hpp>
#include <objects/seqloc/Seq_point.hpp>

#include <objects/seqset/Seq_entry.hpp>

#include <objects/seqfeat/Cdregion.hpp>
#include <objects/seqfeat/Code_break.hpp>
#include <objects/seqfeat/Genetic_code.hpp>
#include <objects/seqfeat/Genetic_code_table.hpp>
#include <objects/seqfeat/Seq_feat.hpp>

#include <objects/util/sequence.hpp>
#include <util/strsearch.hpp>

#include <algorithm>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
BEGIN_SCOPE(sequence)

TSeqPos GetLength(const CSeq_id& id, CScope* scope)
{
    if ( !scope ) {
        return numeric_limits<TSeqPos>::max();
    }
    CBioseq_Handle hnd = scope->GetBioseqHandle(id);
    if ( !hnd ) {
        return numeric_limits<TSeqPos>::max();
    }
    CBioseq_Handle::TBioseqCore core = hnd.GetBioseqCore();
    return core->GetInst().IsSetLength() ? core->GetInst().GetLength() :
        numeric_limits<TSeqPos>::max();
}


TSeqPos GetLength(const CSeq_loc& loc, CScope* scope)
    THROWS((CNoLength))
{
    switch (loc.Which()) {
    case CSeq_loc::e_Pnt:
        return 1;
    case CSeq_loc::e_Int:
        return loc.GetInt().GetLength();
    case CSeq_loc::e_Null:
    case CSeq_loc::e_Empty:
        return 0;
    case CSeq_loc::e_Whole:
        return GetLength(loc.GetWhole(), scope);
    case CSeq_loc::e_Packed_int:
        return loc.GetPacked_int().GetLength();
    case CSeq_loc::e_Mix:
        return GetLength(loc.GetMix(), scope);
    case CSeq_loc::e_Packed_pnt:   // just a bunch of points
        return loc.GetPacked_pnt().GetPoints().size();
    case CSeq_loc::e_not_set:
    case CSeq_loc::e_Bond:         //can't calculate length
    case CSeq_loc::e_Feat:
    case CSeq_loc::e_Equiv:        // unless actually the same length...
    default:
        THROW0_TRACE(CNoLength());
    }
}


TSeqPos GetLength(const CSeq_loc_mix& mix, CScope* scope)
    THROWS((CNoLength))
{
    TSeqPos length = 0;

    ITERATE( CSeq_loc_mix::Tdata, i, mix.Get() ) {
        TSeqPos ret = GetLength((**i), scope);
        length += ret;
    }
    return length;
}


bool IsValid(const CSeq_point& pt, CScope* scope)
{
    if (static_cast<TSeqPos>(pt.GetPoint()) >=
         GetLength(pt.GetId(), scope) )
    {
        return false;
    }

    return true;
}


bool IsValid(const CPacked_seqpnt& pts, CScope* scope)
{
    typedef CPacked_seqpnt::TPoints TPoints;

    TSeqPos length = GetLength(pts.GetId(), scope);
    ITERATE (TPoints, it, pts.GetPoints()) {
        if (*it >= length) {
            return false;
        }
    }
    return true;
}


bool IsValid(const CSeq_interval& interval, CScope* scope)
{
    if (interval.GetFrom() > interval.GetTo() ||
        interval.GetTo() >= GetLength(interval.GetId(), scope))
    {
        return false;
    }

    return true;
}


bool IsSameBioseq (const CSeq_id& id1, const CSeq_id& id2, CScope* scope)
{
    // Compare CSeq_ids directly
    if (id1.Compare(id2) == CSeq_id::e_YES) {
        return true;
    }

    // Compare handles
    if ( scope ) {
        try {
            CBioseq_Handle hnd1 = scope->GetBioseqHandle(id1);
            CBioseq_Handle hnd2 = scope->GetBioseqHandle(id2);
            return hnd1  &&  hnd2  &&  (hnd1 == hnd2);
        } catch (runtime_error& e) {
            ERR_POST(e.what() << ": CSeq_id1: " << id1.DumpAsFasta()
                     << ": CSeq_id2: " << id2.DumpAsFasta());
            return false;
        }
    }

    return false;
}


bool IsOneBioseq(const CSeq_loc& loc, CScope* scope)
{
    try {
        GetId(loc, scope);
        return true;
    } catch (CNotUnique&) {
        return false;
    }
}


const CSeq_id& GetId(const CSeq_loc& loc, CScope* scope)
    THROWS((CNotUnique))
{
    CTypeConstIterator<CSeq_id> cur = ConstBegin(loc);
    CTypeConstIterator<CSeq_id> first = ConstBegin(loc);

    if (!first) {
        THROW0_TRACE(CNotUnique());
    }

    for (++cur; cur; ++cur) {
        if ( !IsSameBioseq(*cur, *first, scope) ) {
            THROW0_TRACE(CNotUnique());
        }
    }
    return *first;
}

inline
static ENa_strand s_GetStrand(const CSeq_loc& loc)
{
    switch (loc.Which()) {
    case CSeq_loc::e_Bond:
        return loc.GetBond().GetA().IsSetStrand() ?
            loc.GetBond().GetA().GetStrand() : eNa_strand_unknown;
    case CSeq_loc::e_Whole:
        return eNa_strand_both;
    case CSeq_loc::e_Int:
        return loc.GetInt().IsSetStrand() ? loc.GetInt().GetStrand() :
            eNa_strand_unknown;
    case CSeq_loc::e_Pnt:
        return loc.GetPnt().IsSetStrand() ? loc.GetPnt().GetStrand() :
            eNa_strand_unknown;
    case CSeq_loc::e_Packed_pnt:
        return loc.GetPacked_pnt().IsSetStrand() ?
            loc.GetPacked_pnt().GetStrand() : eNa_strand_unknown;
    case CSeq_loc::e_Packed_int:
    {
        ENa_strand strand = eNa_strand_unknown;
        bool strand_set = false;
        ITERATE(CPacked_seqint::Tdata, i, loc.GetPacked_int().Get()) {
            ENa_strand istrand = (*i)->IsSetStrand() ? (*i)->GetStrand() :
                eNa_strand_unknown;
            if (strand == eNa_strand_unknown  &&  istrand == eNa_strand_plus) {
                strand = eNa_strand_plus;
                strand_set = true;
            } else if (strand == eNa_strand_plus  &&
                istrand == eNa_strand_unknown) {
                istrand = eNa_strand_plus;
                strand_set = true;
            } else if (!strand_set) {
                strand = istrand;
                strand_set = true;
            } else if (istrand != strand) {
                return eNa_strand_other;
            }
        }
        return strand;
    }
    default:
        return eNa_strand_unknown;
    }
}


ENa_strand GetStrand(const CSeq_loc& loc, CScope* scope)
{
    if (!IsOneBioseq(loc, scope)) {
        return eNa_strand_unknown;
    }

    ENa_strand strand = eNa_strand_unknown, cstrand;
    bool strand_set = false;
    for (CSeq_loc_CI i(loc); i; ++i) {
        switch (i.GetSeq_loc().Which()) {
        case CSeq_loc::e_Mix:
        case CSeq_loc::e_Equiv:
            break;
        default:
            cstrand = s_GetStrand(i.GetSeq_loc());
            if (strand == eNa_strand_unknown  &&  cstrand == eNa_strand_plus) {
                strand = eNa_strand_plus;
                strand_set = true;
            } else if (strand == eNa_strand_plus  &&
                cstrand == eNa_strand_unknown) {
                cstrand = eNa_strand_plus;
                strand_set = true;
            } else if (!strand_set) {
                strand = cstrand;
                strand_set = true;
            } else if (cstrand != strand) {
                return eNa_strand_other;
            }
        }
    }
    return strand;
}


TSeqPos GetStart(const CSeq_loc& loc, CScope* scope)
    THROWS((CNotUnique))
{
    // Throw CNotUnique if loc does not represent one CBioseq
    GetId(loc, scope);

    CSeq_loc::TRange rg = loc.GetTotalRange();
    return rg.GetFrom();
}


TSeqPos GetStop(const CSeq_loc& loc, CScope* scope)
    THROWS((CNotUnique))
{
    // Throw CNotUnique if loc does not represent one CBioseq
    GetId(loc, scope);

    CSeq_loc::TRange rg = loc.GetTotalRange();
    return rg.GetTo();
}


/*
*******************************************************************************
                        Implementation of Compare()
*******************************************************************************
*/


ECompare s_Compare
(const CSeq_loc&,
 const CSeq_loc&,
 CScope*);

int s_RetA[5][5] = {
    { 0, 4, 2, 2, 4 },
    { 4, 1, 4, 1, 4 },
    { 2, 4, 2, 2, 4 },
    { 2, 1, 2, 3, 4 },
    { 4, 4, 4, 4, 4 }
};


int s_RetB[5][5] = {
    { 0, 1, 4, 1, 4 },
    { 1, 1, 4, 1, 1 },
    { 4, 4, 2, 2, 4 },
    { 1, 1, 4, 3, 4 },
    { 4, 1, 4, 4, 4 }
};


// Returns the complement of an ECompare value
inline
ECompare s_Complement(ECompare cmp)
{
    switch ( cmp ) {
    case eContains:
        return eContained;
    case eContained:
        return eContains;
    default:
        return cmp;
    }
}


// Compare two Whole sequences
inline
ECompare s_Compare
(const CSeq_id& id1,
 const CSeq_id& id2,
 CScope*        scope)
{
    // If both sequences from same CBioseq, they are the same
    if ( IsSameBioseq(id1, id2, scope) ) {
        return eSame;
    } else {
        return eNoOverlap;
    }
}


// Compare Whole sequence with a Bond
inline
ECompare s_Compare
(const CSeq_id&   id,
 const CSeq_bond& bond,
 CScope*          scope)
{
    unsigned int count = 0;

    // Increment count if bond point A is from same CBioseq as id
    if ( IsSameBioseq(id, bond.GetA().GetId(), scope) ) {
        ++count;
    }

    // Increment count if second point in bond is set and is from
    // same CBioseq as id.
    if (bond.IsSetB()  &&  IsSameBioseq(id, bond.GetB().GetId(), scope)) {
        ++count;
    }

    switch ( count ) {
    case 1:
        if ( bond.IsSetB() ) {
            // One of two bond points are from same CBioseq as id --> overlap
            return eOverlap;
        } else {
            // There is only one bond point set --> id contains bond
            return eContains;
        }
    case 2:
        // Both bond points are from same CBioseq as id --> id contains bond
        return eContains;
    default:
        // id and bond do not overlap
        return eNoOverlap;
    }
}


// Compare Whole sequence with an interval
inline
ECompare s_Compare
(const CSeq_id&       id,
 const CSeq_interval& interval,
 CScope*              scope)
{
    // If interval is from same CBioseq as id and interval is
    // [0, length of id-1], then they are the same. If interval is from same
    // CBioseq as id, but interval is not [0, length of id -1] then id
    // contains seqloc.
    if ( IsSameBioseq(id, interval.GetId(), scope) ) {
        if (interval.GetFrom() == 0  &&
            interval.GetTo()   == GetLength(id, scope) - 1) {
            return eSame;
        } else {
            return eContains;
        }
    } else {
        return eNoOverlap;
    }
}


// Compare Whole sequence with a point
inline
ECompare s_Compare
(const CSeq_id&     id,
 const CSeq_point&  point,
 CScope*            scope)
{
    // If point from the same CBioseq as id, then id contains point
    if ( IsSameBioseq(id, point.GetId(), scope) ) {
        return eContains;
    } else {
        return eNoOverlap;
    }
}


// Compare Whole sequence with packed points
inline
ECompare s_Compare
(const CSeq_id&        id,
 const CPacked_seqpnt& packed,
 CScope*               scope)
{
    // If packed from same CBioseq as id, then id contains packed
    if ( IsSameBioseq(id, packed.GetId(), scope) ) {
        return eContains;
    } else {
        return eNoOverlap;
    }
}


// Compare an interval with a bond
inline
ECompare s_Compare
(const CSeq_interval& interval,
 const CSeq_bond&     bond,
 CScope*              scope)
{
    unsigned int count = 0;

    // Increment count if interval contains the first point of the bond
    if (IsSameBioseq(interval.GetId(), bond.GetA().GetId(), scope)  &&
        interval.GetFrom() <= bond.GetA().GetPoint()  &&
        interval.GetTo()   >= bond.GetA().GetPoint()) {
        ++count;
    }

    // Increment count if the second point of the bond is set and the
    // interval contains the second point.
    if (bond.IsSetB()  &&
        IsSameBioseq(interval.GetId(), bond.GetB().GetId(), scope)  &&
        interval.GetFrom() <= bond.GetB().GetPoint()  &&
        interval.GetTo()   >= bond.GetB().GetPoint()) {
        ++count;
    }

    switch ( count ) {
    case 1:
        if ( bond.IsSetB() ) {
            // The 2nd point of the bond is set
            return eOverlap;
        } else {
            // The 2nd point of the bond is not set
            return eContains;
        }
    case 2:
        // Both points of the bond are in the interval
        return eContains;
    default:
        return eNoOverlap;
    }
}


// Compare a bond with an interval
inline
ECompare s_Compare
(const CSeq_bond&     bond,
 const CSeq_interval& interval,
 CScope*              scope)
{
    // Just return the complement of comparing an interval with a bond
    return s_Complement(s_Compare(interval, bond, scope));
}


// Compare an interval to an interval
inline
ECompare s_Compare
(const CSeq_interval& intA,
 const CSeq_interval& intB,
 CScope*              scope)
{
    // If the intervals are not on the same bioseq, then there is no overlap
    if ( !IsSameBioseq(intA.GetId(), intB.GetId(), scope) ) {
        return eNoOverlap;
    }

    // Compare two intervals
    TSeqPos fromA = intA.GetFrom();
    TSeqPos toA   = intA.GetTo();
    TSeqPos fromB = intB.GetFrom();
    TSeqPos toB   = intB.GetTo();

    if (fromA == fromB  &&  toA == toB) {
        // End points are the same --> the intervals are the same.
        return eSame;
    } else if (fromA <= fromB  &&  toA >= toB) {
        // intA contains intB
        return eContains;
    } else if (fromA >= fromB  &&  toA <= toB) {
        // intB contains intA
        return eContained;
    } else if (fromA > toB  ||  toA < fromB) {
        // No overlap
        return eNoOverlap;
    } else {
        // The only possibility left is overlap
        return eOverlap;
    }
}


// Compare an interval and a point
inline
ECompare s_Compare
(const CSeq_interval& interval,
 const CSeq_point&    point,
 CScope*              scope)
{
    // If not from same CBioseq, then no overlap
    if ( !IsSameBioseq(interval.GetId(), point.GetId(), scope) ) {
        return eNoOverlap;
    }

    // If point in interval, then interval contains point
    TSeqPos pnt = point.GetPoint();
    if (interval.GetFrom() <= pnt  &&  interval.GetTo() >= pnt ) {
        return eContains;
    }

    // Only other possibility
    return eNoOverlap;
}


// Compare a point and an interval
inline
ECompare s_Compare
(const CSeq_point&    point,
 const CSeq_interval& interval,
 CScope*              scope)
{
    // The complement of comparing an interval with a point.
    return s_Complement(s_Compare(interval, point, scope));
}


// Compare a point with a point
inline
ECompare s_Compare
(const CSeq_point& pntA,
 const CSeq_point& pntB,
 CScope*           scope)
{
    // If points are on same bioseq and coordinates are the same, then same.
    if (IsSameBioseq(pntA.GetId(), pntB.GetId(), scope)  &&
        pntA.GetPoint() == pntB.GetPoint() ) {
        return eSame;
    }

    // Otherwise they don't overlap
    return eNoOverlap;
}


// Compare a point with packed point
inline
ECompare s_Compare
(const CSeq_point&     point,
 const CPacked_seqpnt& points,
 CScope*               scope)
{
    // If on the same bioseq, and any of the packed points are the
    // same as point, then the point is contained in the packed point
    if ( IsSameBioseq(point.GetId(), points.GetId(), scope) ) {
        TSeqPos pnt = point.GetPoint();

        // This loop will only be executed if points.GetPoints().size() > 0
        ITERATE(CSeq_loc::TPoints, it, points.GetPoints()) {
            if (pnt == *it) {
                return eContained;
            }
        }
    }

    // Otherwise, no overlap
    return eNoOverlap;
}


// Compare a point with a bond
inline
ECompare s_Compare
(const CSeq_point& point,
 const CSeq_bond&  bond,
 CScope*           scope)
{
    unsigned int count = 0;

    // If the first point of the bond is on the same CBioseq as point
    // and the point coordinates are the same, increment count by 1
    if (IsSameBioseq(point.GetId(), bond.GetA().GetId(), scope)  &&
        bond.GetA().GetPoint() == point.GetPoint()) {
        ++count;
    }

    // If the second point of the bond is set, the points are from the
    // same CBioseq, and the coordinates of the points are the same,
    // increment the count by 1.
    if (bond.IsSetB()  &&
        IsSameBioseq(point.GetId(), bond.GetB().GetId(), scope)  &&
        bond.GetB().GetPoint() == point.GetPoint()) {
        ++count;
    }

    switch ( count ) {
    case 1:
        if ( bond.IsSetB() ) {
            // The second point of the bond is set -- overlap
            return eOverlap;
            // The second point of the bond is not set -- same
        } else {
            return eSame;
        }
    case 2:
        // Unlikely case -- can happen if both points of the bond are the same
        return eSame;
    default:
        // All that's left is no overlap
        return eNoOverlap;
    }
}


// Compare packed point with an interval
inline
ECompare s_Compare
(const CPacked_seqpnt& points,
 const CSeq_interval&  interval,
 CScope*               scope)
{
    // If points and interval are from same bioseq and some points are
    // in interval, then overlap. If all points are in interval, then
    // contained. Else, no overlap.
    if ( IsSameBioseq(points.GetId(), interval.GetId(), scope) ) {
        bool    got_one    = false;
        bool    missed_one = false;
        TSeqPos from = interval.GetFrom();
        TSeqPos to   = interval.GetTo();

        // This loop will only be executed if points.GetPoints().size() > 0
        ITERATE(CSeq_loc::TPoints, it, points.GetPoints()) {
            if (from <= *it  &&  to >= *it) {
                got_one = true;
            } else {
                missed_one = true;
            }
            if (got_one  &&  missed_one) {
                break;
            }
        }

        if ( !got_one ) {
            return eNoOverlap;
        } else if ( missed_one ) {
            return eOverlap;
        } else {
            return eContained;
        }
    }

    return eNoOverlap;
}


// Compare two packed points
inline
ECompare s_Compare
(const CPacked_seqpnt& pntsA,
 const CPacked_seqpnt& pntsB,
 CScope*               scope)
{
    // If CPacked_seqpnts from different CBioseqs, then no overlap
    if ( !IsSameBioseq(pntsA.GetId(), pntsB.GetId(), scope) ) {
        return eNoOverlap;
    }

    const CSeq_loc::TPoints& pointsA = pntsA.GetPoints();
    const CSeq_loc::TPoints& pointsB = pntsB.GetPoints();

    // Check for equality. Note order of points is significant
    if (pointsA.size() == pointsB.size()) {
        CSeq_loc::TPoints::const_iterator iA = pointsA.begin();
        CSeq_loc::TPoints::const_iterator iB = pointsB.begin();
        bool check_containtment = false;
        // This loop will only be executed if pointsA.size() > 0
        while (iA != pointsA.end()) {
            if (*iA != *iB) {
                check_containtment = true;
                break;
            }
            ++iA;
            ++iB;
        }

        if ( !check_containtment ) {
            return eSame;
        }
    }

    // Check for containment
    size_t hits = 0;
    // This loop will only be executed if pointsA.size() > 0
    ITERATE(CSeq_loc::TPoints, iA, pointsA) {
        ITERATE(CSeq_loc::TPoints, iB, pointsB) {
            hits += (*iA == *iB) ? 1 : 0;
        }
    }
    if (hits == pointsA.size()) {
        return eContained;
    } else if (hits == pointsB.size()) {
        return eContains;
    } else if (hits == 0) {
        return eNoOverlap;
    } else {
        return eOverlap;
    }
}


// Compare packed point with bond
inline
ECompare s_Compare
(const CPacked_seqpnt& points,
 const CSeq_bond&      bond,
 CScope*               scope)
{
    // If all set bond points (can be just 1) are in points, then contains. If
    // one, but not all bond points in points, then overlap, else, no overlap
    const CSeq_point& bondA = bond.GetA();
    ECompare cmp = eNoOverlap;

    // Check for containment of the first residue in the bond
    if ( IsSameBioseq(points.GetId(), bondA.GetId(), scope) ) {
        TSeqPos pntA = bondA.GetPoint();

        // This loop will only be executed if points.GetPoints().size() > 0
        ITERATE(CSeq_loc::TPoints, it, points.GetPoints()) {
            if (pntA == *it) {
                cmp = eContains;
                break;
            }
        }
    }

    // Check for containment of the second residue of the bond if it exists
    if ( !bond.IsSetB() ) {
        return cmp;
    }

    const CSeq_point& bondB = bond.GetB();
    if ( !IsSameBioseq(points.GetId(), bondB.GetId(), scope) ) {
        return cmp;
    }

    TSeqPos pntB = bondB.GetPoint();
    // This loop will only be executed if points.GetPoints().size() > 0
    ITERATE(CSeq_loc::TPoints, it, points.GetPoints()) {
        if (pntB == *it) {
            if (cmp == eContains) {
                return eContains;
            } else {
                return eOverlap;
            }
        }
    }

    return cmp == eContains ? eOverlap : cmp;
}


// Compare a bond with a bond
inline
ECompare s_Compare
(const CSeq_bond& bndA,
 const CSeq_bond& bndB,
 CScope*          scope)
{
    // Performs two comparisons. First comparison is comparing the a points
    // of each bond and the b points of each bond. The second comparison
    // compares point a of bndA with point b of bndB and point b of bndA
    // with point a of bndB. The best match is used.
    ECompare cmp1, cmp2;
    int div = static_cast<int> (eSame);

    // Compare first points in each bond
    cmp1 = s_Compare(bndA.GetA(), bndB.GetA(), scope);

    // Compare second points in each bond if both exist
    if (bndA.IsSetB()  &&  bndB.IsSetB() ) {
        cmp2 = s_Compare(bndA.GetB(), bndB.GetB(), scope);
    } else {
        cmp2 = eNoOverlap;
    }
    int count1 = (static_cast<int> (cmp1) + static_cast<int> (cmp2)) / div;

    // Compare point A of bndA with point B of bndB (if it exists)
    if ( bndB.IsSetB() ) {
        cmp1 = s_Compare(bndA.GetA(), bndB.GetB(), scope);
    } else {
        cmp1 = eNoOverlap;
    }

    // Compare point B of bndA (if it exists) with point A of bndB.
    if ( bndA.IsSetB() ) {
        cmp2 = s_Compare(bndA.GetB(), bndB.GetA(), scope);
    } else {
        cmp2 = eNoOverlap;
    }
    int count2 = (static_cast<int> (cmp1) + static_cast<int> (cmp2)) / div;

    // Determine best match
    int count = (count1 > count2) ? count1 : count2;

    // Return based upon count in the obvious way
    switch ( count ) {
    case 1:
        if (!bndA.IsSetB()  &&  !bndB.IsSetB()) {
            return eSame;
        } else if ( !bndB.IsSetB() ) {
            return eContains;
        } else if ( !bndA.IsSetB() ) {
            return eContained;
        } else {
            return eOverlap;
        }
    case 2:
        return eSame;
    default:
        return eNoOverlap;
    }
}


// Compare an interval with a whole sequence
inline
ECompare s_Compare
(const CSeq_interval& interval,
 const CSeq_id&       id,
 CScope*              scope)
{
    // Return the complement of comparing id with interval
    return s_Complement(s_Compare(id, interval, scope));
}


// Compare an interval with a packed point
inline
ECompare s_Compare
(const CSeq_interval&  interval,
 const CPacked_seqpnt& packed,
 CScope*               scope)
{
    // Return the complement of comparing packed points and an interval
    return s_Complement(s_Compare(packed, interval, scope));
}


// Compare a Packed Interval with one of Whole, Interval, Point,
// Packed Point, or Bond.
template <class T>
ECompare s_Compare
(const CPacked_seqint& intervals,
 const T&              slt,
 CScope*               scope)
{
    // Check that intervals is not empty
    if(intervals.Get().size() == 0) {
        return eNoOverlap;
    }

    ECompare cmp1 , cmp2;
    CSeq_loc::TIntervals::const_iterator it = intervals.Get().begin();
    cmp1 = s_Compare(**it, slt, scope);

    // This loop will be executed only if intervals.Get().size() > 1
    for (++it;  it != intervals.Get().end();  ++it) {
        cmp2 = s_Compare(**it, slt, scope);
        cmp1 = static_cast<ECompare> (s_RetA[cmp1][cmp2]);
    }
    return cmp1;
}


// Compare one of Whole, Interval, Point, Packed Point, or Bond with a
// Packed Interval.
template <class T>
ECompare s_Compare
(const T&              slt,
 const CPacked_seqint& intervals,
 CScope*               scope)
{
    // Check that intervals is not empty
    if(intervals.Get().size() == 0) {
        return eNoOverlap;
    }

    ECompare cmp1 , cmp2;
    CSeq_loc::TIntervals::const_iterator it = intervals.Get().begin();
    cmp1 = s_Compare(slt, **it, scope);

    // This loop will be executed only if intervals.Get().size() > 1
    for (++it;  it != intervals.Get().end();  ++it) {
        cmp2 = s_Compare(slt, **it, scope);
        cmp1 = static_cast<ECompare> (s_RetB[cmp1][cmp2]);
    }
    return cmp1;
}


// Compare a CSeq_loc and a CSeq_interval. Used by s_Compare_Help below
// when comparing list<CRef<CSeq_loc>> with list<CRef<CSeq_interval>>.
// By wrapping the CSeq_interval in a CSeq_loc, s_Compare(const CSeq_loc&,
// const CSeq_loc&, CScope*) can be called. This permits CPacked_seqint type
// CSeq_locs to be treated the same as CSeq_loc_mix and CSeq_loc_equiv type
// CSeq_locs
inline
ECompare s_Compare
(const CSeq_loc&      sl,
 const CSeq_interval& si,
 CScope*              scope)
{
    CSeq_loc nsl;
    nsl.SetInt(const_cast<CSeq_interval&>(si));
    return s_Compare(sl, nsl, scope);
}


// Compare a CSeq_interval and a CSeq_loc. Used by s_Compare_Help below
// when comparing TLocations with TIntervals. By
// wrapping the CSeq_interval in a CSeq_loc, s_Compare(const CSeq_loc&,
// const CSeq_loc&, CScope*) can be called. This permits CPacked_seqint type
// CSeq_locs to be treated the same as CSeq_loc_mix and CSeq_loc_equiv type
// CSeq_locs
inline
ECompare s_Compare
(const CSeq_interval& si,
 const CSeq_loc&      sl,
 CScope*              scope)
{
    CSeq_loc nsl;
    nsl.SetInt(const_cast<CSeq_interval&>(si));
    return s_Compare(nsl, sl, scope);
}


// Called by s_Compare() below for <CSeq_loc, CSeq_loc>,
// <CSeq_loc, CSeq_interval>, <CSeq_interval, CSeq_loc>, and
// <CSeq_interval, CSeq_interval>
template <class T1, class T2>
ECompare s_Compare_Help
(const list<CRef<T1> >& mec,
 const list<CRef<T2> >& youc,
 const CSeq_loc&        you,
 CScope*                scope)
{
    // If either mec or youc is empty, return eNoOverlap
    if(mec.size() == 0  ||  youc.size() == 0) {
        return eNoOverlap;
    }

    list<CRef<T1> >::const_iterator mit = mec.begin();
    list<CRef<T2> >::const_iterator yit = youc.begin();
    ECompare cmp1, cmp2;

    // Check for equality of the lists. Note, order of the lists contents are
    // significant
    if (mec.size() == youc.size()) {
        bool check_contained = false;

        for ( ;  mit != mec.end()  &&  yit != youc.end();  ++mit, ++yit) {
            if (s_Compare(**mit, ** yit, scope) != eSame) {
                check_contained = true;
                break;
            }
        }

        if ( !check_contained ) {
            return eSame;
        }
    }

    // Check if mec contained in youc
    mit = mec.begin();
    yit = youc.begin();
    bool got_one = false;
    while (mit != mec.end()  &&  yit != youc.end()) {
        cmp1 = s_Compare(**mit, **yit, scope);
        switch ( cmp1 ) {
        case eNoOverlap:
            ++yit;
            break;
        case eSame:
            ++mit;
            ++yit;
            got_one = true;
            break;
        case eContained:
            ++mit;
            got_one = true;
            break;
        default:
            got_one = true;
            // artificial trick -- just to get out the loop "prematurely"
            yit = youc.end();
        }
    }
    if (mit == mec.end()) {
        return eContained;
    }

    // Check if mec contains youc
    mit = mec.begin();
    yit = youc.begin();
    while (mit != mec.end()  &&  yit != youc.end() ) {
        cmp1 = s_Compare(**yit, **mit, scope);
        switch ( cmp1 ) {
        case eNoOverlap:
            ++mit;
            break;
        case eSame:
            ++mit;
            ++yit;
            got_one = true;
            break;
        case eContained:
            ++yit;
            got_one = true;
            break;
        default:
            got_one = true;
            // artificial trick -- just to get out the loop "prematurely"
            mit = mec.end();
        }
    }
    if (yit == youc.end()) {
        return eContains;
    }

    // Check for overlap of mec and youc
    if ( got_one ) {
        return eOverlap;
    }
    mit = mec.begin();
    cmp1 = s_Compare(**mit, you, scope);
    for (++mit;  mit != mec.end();  ++mit) {
        cmp2 = s_Compare(**mit, you, scope);
        cmp1 = static_cast<ECompare> (s_RetA[cmp1][cmp2]);
    }
    return cmp1;
}


inline
const CSeq_loc::TLocations& s_GetLocations(const CSeq_loc& loc)
{
    switch (loc.Which()) {
        case CSeq_loc::e_Mix:    return loc.GetMix().Get();
        case CSeq_loc::e_Equiv:  return loc.GetEquiv().Get();
        default: // should never happen, but the compiler doesn't know that...
            THROW1_TRACE(runtime_error,
                         "s_GetLocations: unsupported location type:"
                         + CSeq_loc::SelectionName(loc.Which()));
    }
}


// Compares two Seq-locs
ECompare s_Compare
(const CSeq_loc& me,
 const CSeq_loc& you,
 CScope*         scope)
{
    // Handle the case where me is one of e_Mix, e_Equiv, e_Packed_int
    switch ( me.Which() ) {
    case CSeq_loc::e_Mix:
    case CSeq_loc::e_Equiv: {
        const CSeq_loc::TLocations& pmc = s_GetLocations(me);
        switch ( you.Which() ) {
        case CSeq_loc::e_Mix:
        case CSeq_loc::e_Equiv: {
            const CSeq_loc::TLocations& pyc = s_GetLocations(you);
            return s_Compare_Help(pmc, pyc, you, scope);
        }
        case CSeq_loc::e_Packed_int: {
            const CSeq_loc::TIntervals& pyc = you.GetPacked_int().Get();
            return s_Compare_Help(pmc,pyc, you, scope);
        }
        case CSeq_loc::e_Null:
        case CSeq_loc::e_Empty:
        case CSeq_loc::e_Whole:
        case CSeq_loc::e_Int:
        case CSeq_loc::e_Pnt:
        case CSeq_loc::e_Packed_pnt:
        case CSeq_loc::e_Bond:
        case CSeq_loc::e_Feat: {
            //Check that pmc is not empty
            if(pmc.size() == 0) {
                return eNoOverlap;
            }

            CSeq_loc::TLocations::const_iterator mit = pmc.begin();
            ECompare cmp1, cmp2;
            cmp1 = s_Compare(**mit, you, scope);

            // This loop will only be executed if pmc.size() > 1
            for (++mit;  mit != pmc.end();  ++mit) {
                cmp2 = s_Compare(**mit, you, scope);
                cmp1 = static_cast<ECompare> (s_RetA[cmp1][cmp2]);
            }
            return cmp1;
        }
        default:
            return eNoOverlap;
        }
    }
    case CSeq_loc::e_Packed_int: {
        switch ( you.Which() ) {
        case CSeq_loc::e_Mix:
        case CSeq_loc::e_Equiv: {
            const CSeq_loc::TLocations& pyc = s_GetLocations(you);
            const CSeq_loc::TIntervals& pmc = me.GetPacked_int().Get();
            return s_Compare_Help(pmc,pyc, you, scope);
        }
        case CSeq_loc::e_Packed_int: {
            const CSeq_loc::TIntervals& mc = me.GetPacked_int().Get();
            const CSeq_loc::TIntervals& yc = you.GetPacked_int().Get();
            return s_Compare_Help(mc, yc, you, scope);
        }
        default:
            // All other cases are handled below
            break;
        }
    }
    default:
        // All other cases are handled below
        break;
    }

    // Note, at this point, me is not one of e_Mix or e_Equiv
    switch ( you.Which() ) {
    case CSeq_loc::e_Mix:
    case CSeq_loc::e_Equiv: {
        const CSeq_loc::TLocations& pyouc = s_GetLocations(you);

        // Check that pyouc is not empty
        if(pyouc.size() == 0) {
            return eNoOverlap;
        }

        CSeq_loc::TLocations::const_iterator yit = pyouc.begin();
        ECompare cmp1, cmp2;
        cmp1 = s_Compare(me, **yit, scope);

        // This loop will only be executed if pyouc.size() > 1
        for (++yit;  yit != pyouc.end();  ++yit) {
            cmp2 = s_Compare(me, **yit, scope);
            cmp1 = static_cast<ECompare> (s_RetB[cmp1][cmp2]);
        }
        return cmp1;
    }
    // All other cases handled below
    default:
        break;
    }

    switch ( me.Which() ) {
    case CSeq_loc::e_Null: {
        switch ( you.Which() ) {
        case CSeq_loc::e_Null:
            return eSame;
        default:
            return eNoOverlap;
        }
    }
    case CSeq_loc::e_Empty: {
        switch ( you.Which() ) {
        case CSeq_loc::e_Empty:
            if ( IsSameBioseq(me.GetEmpty(), you.GetEmpty(), scope) ) {
                return eSame;
            } else {
                return eNoOverlap;
            }
        default:
            return eNoOverlap;
        }
    }
    case CSeq_loc::e_Packed_int: {
        // Comparison of two e_Packed_ints is handled above
        switch ( you.Which() ) {
        case CSeq_loc::e_Whole:
            return s_Compare(me.GetPacked_int(), you.GetWhole(), scope);
        case CSeq_loc::e_Int:
            return s_Compare(me.GetPacked_int(), you.GetInt(), scope);
        case CSeq_loc::e_Pnt:
            return s_Compare(me.GetPacked_int(), you.GetPnt(), scope);
        case CSeq_loc::e_Packed_pnt:
            return s_Compare(me.GetPacked_int(), you.GetPacked_pnt(), scope);
        case CSeq_loc::e_Bond:
            return s_Compare(me.GetPacked_int(), you.GetBond(), scope);
        default:
            return eNoOverlap;
        }
    }
    case CSeq_loc::e_Whole: {
        switch ( you.Which() ) {
        case CSeq_loc::e_Whole:
            return s_Compare(me.GetWhole(), you.GetWhole(), scope);
        case CSeq_loc::e_Bond:
            return s_Compare(me.GetWhole(), you.GetBond(), scope);
        case CSeq_loc::e_Int:
            return s_Compare(me.GetWhole(), you.GetInt(), scope);
        case CSeq_loc::e_Pnt:
            return s_Compare(me.GetWhole(), you.GetPnt(), scope);
        case CSeq_loc::e_Packed_pnt:
            return s_Compare(me.GetWhole(), you.GetPacked_pnt(), scope);
        case CSeq_loc::e_Packed_int:
            return s_Compare(me.GetWhole(), you.GetPacked_int(), scope);
        default:
            return eNoOverlap;
        }
    }
    case CSeq_loc::e_Int: {
        switch ( you.Which() ) {
        case CSeq_loc::e_Whole:
            return s_Compare(me.GetInt(), you.GetWhole(), scope);
        case CSeq_loc::e_Bond:
            return s_Compare(me.GetInt(), you.GetBond(), scope);
        case CSeq_loc::e_Int:
            return s_Compare(me.GetInt(), you.GetInt(), scope);
        case CSeq_loc::e_Pnt:
            return s_Compare(me.GetInt(), you.GetPnt(), scope);
        case CSeq_loc::e_Packed_pnt:
            return s_Compare(me.GetInt(), you.GetPacked_pnt(), scope);
        case CSeq_loc::e_Packed_int:
            return s_Compare(me.GetInt(), you.GetPacked_int(), scope);
        default:
            return eNoOverlap;
        }
    }
    case CSeq_loc::e_Pnt: {
        switch ( you.Which() ) {
        case CSeq_loc::e_Whole:
            return s_Complement(s_Compare(you.GetWhole(), me.GetPnt(), scope));
        case CSeq_loc::e_Int:
            return s_Compare(me.GetPnt(), you.GetInt(), scope);
        case CSeq_loc::e_Pnt:
            return s_Compare(me.GetPnt(), you.GetPnt(), scope);
        case CSeq_loc::e_Packed_pnt:
            return s_Compare(me.GetPnt(), you.GetPacked_pnt(), scope);
        case CSeq_loc::e_Bond:
            return s_Compare(me.GetPnt(), you.GetBond(), scope);
        case CSeq_loc::e_Packed_int:
            return s_Compare(me.GetPnt(), you.GetPacked_int(), scope);
        default:
            return eNoOverlap;
        }
    }
    case CSeq_loc::e_Packed_pnt: {
        const CPacked_seqpnt& packed = me.GetPacked_pnt();
        switch ( you.Which() ) {
        case CSeq_loc::e_Whole:
            return s_Complement(s_Compare(you.GetWhole(), packed, scope));
        case CSeq_loc::e_Int:
            return s_Compare(packed, you.GetInt(), scope);
        case CSeq_loc::e_Pnt:
            return s_Complement(s_Compare(you.GetPnt(), packed, scope));
        case CSeq_loc::e_Packed_pnt:
            return s_Compare(packed, you.GetPacked_pnt(),scope);
        case CSeq_loc::e_Bond:
            return s_Compare(packed, you.GetBond(), scope);
        case CSeq_loc::e_Packed_int:
            return s_Compare(me.GetPacked_pnt(), you.GetPacked_int(), scope);
        default:
            return eNoOverlap;
        }
    }
    case CSeq_loc::e_Bond: {
        const CSeq_bond& bnd = me.GetBond();
        switch ( you.Which() ) {
        case CSeq_loc::e_Whole:
            return s_Complement(s_Compare(you.GetWhole(), bnd, scope));
        case CSeq_loc::e_Bond:
            return s_Compare(bnd, you.GetBond(), scope);
        case CSeq_loc::e_Int:
            return s_Compare(bnd, you.GetInt(), scope);
        case CSeq_loc::e_Packed_pnt:
            return s_Complement(s_Compare(you.GetPacked_pnt(), bnd, scope));
        case CSeq_loc::e_Pnt:
            return s_Complement(s_Compare(you.GetPnt(), bnd, scope));
        case CSeq_loc::e_Packed_int:
            return s_Complement(s_Compare(you.GetPacked_int(), bnd, scope));
        default:
            return eNoOverlap;
        }
    }
    default:
        return eNoOverlap;
    }
}


ECompare Compare
(const CSeq_loc& loc1,
 const CSeq_loc& loc2,
 CScope*         scope)
{
    return s_Compare(loc1, loc2, scope);
}


void ChangeSeqId(CSeq_id* id, bool best, CScope* scope)
{
    // Return if no scope
    if (!scope) {
        return;
    }

    // Get CBioseq represented by *id
    CBioseq_Handle::TBioseqCore seq =
        scope->GetBioseqHandle(*id).GetBioseqCore();

    // Get pointer to the best/worst id of *seq
    const CSeq_id* tmp_id;
    if (!best) {
        tmp_id = FindBestChoice(seq->GetId(), CSeq_id::BestRank).GetPointer();
    } else {
        tmp_id = FindBestChoice(seq->GetId(), CSeq_id::WorstRank).GetPointer();
    }

    // Change the contents of *id to that of *tmp_id
    id->Reset();
    id->Assign(*tmp_id);
}


void ChangeSeqLocId(CSeq_loc* loc, bool best, CScope* scope)
{
    if (!scope) {
        return;
    }

    for (CTypeIterator<CSeq_id> id(Begin(*loc)); id; ++id) {
        ChangeSeqId(&(*id), best, scope);
    }
}


bool BadSeqLocSortOrder
(const CBioseq&, //  seq,
 const CSeq_loc& loc,
 CScope*         scope)
{
    ENa_strand strand = GetStrand(loc, scope);
    if (strand == eNa_strand_unknown  ||  strand == eNa_strand_other) {
        return false;
    }
    
    // Check that loc segments are in order
    CSeq_loc::TRange last_range;
    bool first = true;
    for (CSeq_loc_CI lit(loc); lit; ++lit) {
        if (first) {
            last_range = lit.GetRange();
            first = false;
            continue;
        }
        if (strand == eNa_strand_minus) {
            if (last_range.GetTo() < lit.GetRange().GetTo()) {
                return true;
            }
        } else {
            if (last_range.GetFrom() > lit.GetRange().GetFrom()) {
                return true;
            }
        }
        last_range = lit.GetRange();
    }
    return false;
}


ESeqLocCheck SeqLocCheck(const CSeq_loc& loc, CScope* scope)
{
    ESeqLocCheck rtn = eSeqLocCheck_ok;

    ENa_strand strand = GetStrand(loc, scope);
    if (strand == eNa_strand_unknown  ||  strand == eNa_strand_other) {
        rtn = eSeqLocCheck_warning;
    }

    CTypeConstIterator<CSeq_loc> lit(ConstBegin(loc));
    for (;lit; ++lit) {
        switch (lit->Which()) {
        case CSeq_loc::e_Int:
            if (!IsValid(lit->GetInt(), scope)) {
                rtn = eSeqLocCheck_error;
            }
            break;
        case CSeq_loc::e_Packed_int:
        {
            CTypeConstIterator<CSeq_interval> sit(ConstBegin(*lit));
            for(;sit; ++sit) {
                if (!IsValid(*sit, scope)) {
                    rtn = eSeqLocCheck_error;
                    break;
                }
            }
            break;
        }
        case CSeq_loc::e_Pnt:
            if (!IsValid(lit->GetPnt(), scope)) {
                rtn = eSeqLocCheck_error;
            }
            break;
        case CSeq_loc::e_Packed_pnt:
            if (!IsValid(lit->GetPacked_pnt(), scope)) {
                rtn = eSeqLocCheck_error;
            }
            break;
        default:
            break;
        }
    }
    return rtn;
}


// XXX - handle fuzz?
CRef<CSeq_loc> SourceToProduct(const CSeq_feat& feat,
                               const CSeq_loc& source_loc, TS2PFlags flags,
                               CScope* scope, int* frame)
{
    SRelLoc::TFlags rl_flags = 0;
    if (flags & fS2P_NoMerge) {
        rl_flags |= SRelLoc::fNoMerge;
    }
    SRelLoc rl(feat.GetLocation(), source_loc, scope, rl_flags);
    _ASSERT(!rl.m_Ranges.empty());
    rl.m_ParentLoc.Reset(&feat.GetProduct());
    if (feat.GetData().IsCdregion()) {
        // 3:1 ratio
        const CCdregion& cds         = feat.GetData().GetCdregion();
        int              base_frame  = cds.GetFrame();
        if (base_frame > 0) {
            --base_frame;
        }
        if (frame) {
            *frame = 3 - (rl.m_Ranges.front()->GetFrom() + 2 - base_frame) % 3;
        }
        TSeqPos prot_length;
        try {
            prot_length = GetLength(feat.GetProduct(), scope);
        } catch (CNoLength) {
            prot_length = numeric_limits<TSeqPos>::max();
        }
        NON_CONST_ITERATE (SRelLoc::TRanges, it, rl.m_Ranges) {
            (*it)->SetFrom(((*it)->GetFrom() - base_frame) / 3);
            (*it)->SetTo  (((*it)->GetTo()   - base_frame) / 3);
            if ((flags & fS2P_AllowTer)  &&  (*it)->GetTo() == prot_length) {
                --(*it)->SetTo();
            }
        }
    } else {
        if (frame) {
            *frame = 0; // not applicable; explicitly zero
        }
    }

    return rl.Resolve(scope, rl_flags);
}


// XXX - handle fuzz?
CRef<CSeq_loc> ProductToSource(const CSeq_feat& feat, const CSeq_loc& prod_loc,
                               TP2SFlags flags, CScope* scope)
{
    SRelLoc rl(feat.GetProduct(), prod_loc, scope);
    _ASSERT(!rl.m_Ranges.empty());
    rl.m_ParentLoc.Reset(&feat.GetLocation());
    if (feat.GetData().IsCdregion()) {
        // 3:1 ratio
        const CCdregion& cds        = feat.GetData().GetCdregion();
        int              base_frame = cds.GetFrame();
        if (base_frame > 0) {
            --base_frame;
        }
        TSeqPos nuc_length, prot_length;
        try {
            nuc_length = GetLength(feat.GetLocation(), scope);
        } catch (CNoLength) {
            nuc_length = numeric_limits<TSeqPos>::max();
        }
        try {
            prot_length = GetLength(feat.GetProduct(), scope);
        } catch (CNoLength) {
            prot_length = numeric_limits<TSeqPos>::max();
        }
        NON_CONST_ITERATE(SRelLoc::TRanges, it, rl.m_Ranges) {
            TSeqPos from, to;
            if ((flags & fP2S_Extend)  &&  (*it)->GetFrom() == 0) {
                from = 0;
            } else {
                from = (*it)->GetFrom() * 3 + base_frame;
            }
            if ((flags & fP2S_Extend)  &&  (*it)->GetTo() == prot_length - 1) {
                to = nuc_length - 1;
            } else {
                to = (*it)->GetTo() * 3 + base_frame + 2;
            }
            (*it)->SetFrom(from);
            (*it)->SetTo  (to);
        }
    }

    return rl.Resolve(scope);
}


TSeqPos LocationOffset(const CSeq_loc& outer, const CSeq_loc& inner,
                       EOffsetType how, CScope* scope)
{
    SRelLoc rl(outer, inner, scope);
    if (rl.m_Ranges.empty()) {
        return (TSeqPos)-1;
    }
    bool want_reverse = false;
    {{
        bool outer_is_reverse = IsReverse(GetStrand(outer, scope));
        switch (how) {
        case eOffset_FromStart:
            want_reverse = false;
            break;
        case eOffset_FromEnd:
            want_reverse = true;
            break;
        case eOffset_FromLeft:
            want_reverse = outer_is_reverse;
            break;
        case eOffset_FromRight:
            want_reverse = !outer_is_reverse;
            break;
        }
    }}
    if (want_reverse) {
        return GetLength(outer, scope) - rl.m_Ranges.back()->GetTo();
    } else {
        return rl.m_Ranges.front()->GetFrom();
    }
}


bool TestForStrands(ENa_strand strand1, ENa_strand strand2)
{
    // Check strands. Overlapping rules for strand:
    //   - equal strands overlap
    //   - "both" overlaps with any other
    //   - "unknown" overlaps with any other except "minus"
    return strand1 == strand2
        || strand1 == eNa_strand_both
        || strand2 == eNa_strand_both
        || (strand1 == eNa_strand_unknown  && strand2 != eNa_strand_minus)
        || (strand2 == eNa_strand_unknown  && strand1 != eNa_strand_minus);
}


bool TestForIntervals(CSeq_loc_CI it1, CSeq_loc_CI it2, bool minus_strand)
{
    // Check intervals one by one
    while ( it1  &&  it2 ) {
        if ( !TestForStrands(it1.GetStrand(), it2.GetStrand()) ) {
            return false;
        }
        if ( minus_strand ) {
            if (it1.GetRange().GetFrom()  !=  it2.GetRange().GetFrom() ) {
                // The last interval from loc2 may be shorter than the
                // current interval from loc1
                if (it1.GetRange().GetFrom() > it2.GetRange().GetFrom()  ||
                    ++it2) {
                    return false;
                }
                break;
            }
        }
        else {
            if (it1.GetRange().GetTo()  !=  it2.GetRange().GetTo() ) {
                // The last interval from loc2 may be shorter than the
                // current interval from loc1
                if (it1.GetRange().GetTo() < it2.GetRange().GetTo()  ||
                    ++it2) {
                    return false;
                }
                break;
            }
        }
        // Go to the next interval start
        if ( !(++it2) ) {
            break;
        }
        if ( !(++it1) ) {
            return false; // loc1 has not enough intervals
        }
        if ( minus_strand ) {
            if (it1.GetRange().GetTo() != it2.GetRange().GetTo()) {
                return false;
            }
        }
        else {
            if (it1.GetRange().GetFrom() != it2.GetRange().GetFrom()) {
                return false;
            }
        }
    }
    return true;
}


int TestForOverlap(const CSeq_loc& loc1,
                   const CSeq_loc& loc2,
                   EOverlapType type)
{
    CRange<TSeqPos> rg1 = loc1.GetTotalRange();
    CRange<TSeqPos> rg2 = loc2.GetTotalRange();

    ENa_strand strand1 = GetStrand(loc1);
    ENa_strand strand2 = GetStrand(loc2);
    if ( !TestForStrands(strand1, strand2) )
        return -1;
    switch (type) {
    case eOverlap_Simple:
        {
            if ( rg1.GetTo() >= rg2.GetFrom()  &&
                rg1.GetFrom() <= rg2.GetTo() ) {
                return abs(int(rg2.GetFrom() - rg1.GetFrom())) +
                    abs(int(rg1.GetTo() - rg2.GetTo()));
            }
            return -1;
        }
    case eOverlap_Contained:
        {
            if ( rg1.GetFrom() <= rg2.GetFrom()  &&
                rg1.GetTo() >= rg2.GetTo() ) {
                return (rg2.GetFrom() - rg1.GetFrom()) +
                    (rg1.GetTo() - rg2.GetTo());
            }
            return -1;
        }
    case eOverlap_Contains:
        {
            if ( rg2.GetFrom() <= rg1.GetFrom()  &&
                rg2.GetTo() >= rg1.GetTo() ) {
                return (rg1.GetFrom() - rg2.GetFrom()) +
                    (rg2.GetTo() - rg1.GetTo());
            }
            return -1;
        }
    case eOverlap_Subset:
        {
            // loc1 should contain loc2
            if ( Compare(loc1, loc2) != eContains ) {
                return -1;
            }
            return GetLength(loc1) - GetLength(loc2);
        }
    case eOverlap_CheckIntervals:
        {
            if ( rg1.GetFrom() > rg2.GetTo()  ||
                rg1.GetTo() < rg2.GetTo() ) {
                return -1;
            }
            // Check intervals' boundaries
            CSeq_loc_CI it1(loc1);
            CSeq_loc_CI it2(loc2);
            if (!it1  ||  !it2) {
                break;
            }
            // check case when strand is minus
            if (it2.GetStrand() == eNa_strand_minus) {
                // The first interval should be treated as the last one
                // for minuns strands.
                TSeqPos loc2end = it2.GetRange().GetTo();
                TSeqPos loc2start = it2.GetRange().GetFrom();
                // Find the first interval in loc1 intersecting with loc2
                for ( ; it1  &&  it1.GetRange().GetTo() >= loc2start; ++it1) {
                    if (it1.GetRange().GetTo() >= loc2end  &&
                        TestForIntervals(it1, it2, true)) {
                        return GetLength(loc1) - GetLength(loc2);
                    }
                }
            }
            else {
                TSeqPos loc2start = it2.GetRange().GetFrom();
                TSeqPos loc2end = it2.GetRange().GetTo();
                // Find the first interval in loc1 intersecting with loc2
                for ( ; it1  &&  it1.GetRange().GetFrom() <= loc2end; ++it1) {
                    if (it1.GetRange().GetFrom() <= loc2start  &&
                        TestForIntervals(it1, it2, false)) {
                        return GetLength(loc1) - GetLength(loc2);
                    }
                }
            }
            return -1;
        }
    case eOverlap_Interval:
        {
            return (Compare(loc1, loc2) == eNoOverlap) ? -1
                : abs(int(rg2.GetFrom() - rg1.GetFrom())) +
                abs(int(rg1.GetTo() - rg2.GetTo()));
        }
    }
    return -1;
}


CConstRef<CSeq_feat> GetBestOverlappingFeat(const CSeq_loc& loc,
                                            CSeqFeatData::E_Choice feat_type,
                                            EOverlapType overlap_type,
                                            CScope& scope)
{
    bool revert_locations = false;
    SAnnotSelector::EOverlapType annot_overlap_type;
    switch (overlap_type) {
    case eOverlap_Simple:
    case eOverlap_Contained:
    case eOverlap_Contains:
        // Require total range overlap
        annot_overlap_type = SAnnotSelector::eOverlap_TotalRange;
        break;
    case eOverlap_Subset:
    case eOverlap_CheckIntervals:
    case eOverlap_Interval:
        revert_locations = true;
        // there's no break here - proceed to "default"
    default:
        // Require intervals overlap
        annot_overlap_type = SAnnotSelector::eOverlap_Intervals;
        break;
    }

    CConstRef<CSeq_feat> feat_ref;
    int diff = -1;

    CFeat_CI feat_it(scope, loc, feat_type,
        annot_overlap_type,
        CFeat_CI::eResolve_TSE, // ???
        CFeat_CI::e_Location);
    for ( ; feat_it; ++feat_it) {
        // treat subset as a special case
        int cur_diff = ( !revert_locations ) ?
            TestForOverlap(loc, feat_it->GetLocation(), overlap_type) :
            TestForOverlap(feat_it->GetLocation(), loc, overlap_type);
        if (cur_diff < 0)
            continue;
        if ( cur_diff < diff  ||  diff < 0 ) {
            diff = cur_diff;
            feat_ref = &feat_it->GetMappedFeature();
        }
    }
    return feat_ref;
}


CConstRef<CSeq_feat> GetBestOverlappingFeat(const CSeq_loc& loc,
                                            CSeqFeatData::ESubtype feat_type,
                                            EOverlapType overlap_type,
                                            CScope& scope)
{
    bool revert_locations = false;
    SAnnotSelector::EOverlapType annot_overlap_type;
    switch (overlap_type) {
    case eOverlap_Simple:
    case eOverlap_Contained:
    case eOverlap_Contains:
        // Require total range overlap
        annot_overlap_type = SAnnotSelector::eOverlap_TotalRange;
        break;
    case eOverlap_Subset:
    case eOverlap_CheckIntervals:
    case eOverlap_Interval:
        revert_locations = true;
        // there's no break here - proceed to "default"
    default:
        // Require intervals overlap
        annot_overlap_type = SAnnotSelector::eOverlap_Intervals;
        break;
    }

    CConstRef<CSeq_feat> feat_ref;
    int diff = -1;

    for ( CFeat_CI feat_it(scope, loc, CSeqFeatData::e_not_set,
                           annot_overlap_type,
                           CFeat_CI::eResolve_TSE, // ???
                           CFeat_CI::e_Location); feat_it; ++feat_it ) {
        if ( feat_it->GetData().GetSubtype() != feat_type )
            continue;
        int cur_diff = ( !revert_locations ) ?
            TestForOverlap(loc, feat_it->GetLocation(), overlap_type) :
            TestForOverlap(feat_it->GetLocation(), loc, overlap_type);
        if (cur_diff < 0)
            continue;
        if ( cur_diff < diff  ||  diff < 0 ) {
            diff = cur_diff;
            feat_ref = &feat_it->GetMappedFeature();
        }
    }
    return feat_ref;
}


int SeqLocPartialCheck(const CSeq_loc& loc, CScope* scope)
{
    unsigned int retval = 0;
    if (!scope) {
        return retval;
    }

    // Find first and last Seq-loc
    const CSeq_loc *first = 0, *last = 0;
    for ( CSeq_loc_CI loc_iter(loc); loc_iter; ++loc_iter ) {
        if ( first == 0 ) {
            first = &(loc_iter.GetSeq_loc());
        }
        last = &(loc_iter.GetSeq_loc());
    }
    if (!first) {
        return retval;
    }

    CSeq_loc_CI i2(loc);
    for ( ; i2; ++i2 ) {
        const CSeq_loc* slp = &(i2.GetSeq_loc());
        switch (slp->Which()) {
        case CSeq_loc::e_Null:
            if (slp == first) {
                retval |= eSeqlocPartial_Start;
            } else if (slp == last) {
                retval |= eSeqlocPartial_Stop;
            } else {
                retval |= eSeqlocPartial_Internal;
            }
            break;
        case CSeq_loc::e_Int:
            {
                const CSeq_interval& itv = slp->GetInt();
                if (itv.IsSetFuzz_from()) {
                    const CInt_fuzz& fuzz = itv.GetFuzz_from();
                    if (fuzz.Which() == CInt_fuzz::e_Lim) {
                        CInt_fuzz::ELim lim = fuzz.GetLim();
                        if (lim == CInt_fuzz::eLim_gt) {
                            retval |= eSeqlocPartial_Limwrong;
                        } else if (lim == CInt_fuzz::eLim_lt  ||
                            lim == CInt_fuzz::eLim_unk) {
                            if (itv.IsSetStrand()  &&
                                itv.GetStrand() == eNa_strand_minus) {
                                if (slp == last) {
                                    retval |= eSeqlocPartial_Stop;
                                } else {
                                    retval |= eSeqlocPartial_Internal;
                                }
                                if (itv.GetFrom() != 0) {
                                    if (slp == last) {
                                        retval |= eSeqlocPartial_Nostop;
                                    } else {
                                        retval |= eSeqlocPartial_Nointernal;
                                    }
                                }
                            } else {
                                if (slp == first) {
                                    retval |= eSeqlocPartial_Start;
                                } else {
                                    retval |= eSeqlocPartial_Internal;
                                }
                                if (itv.GetFrom() != 0) {
                                    if (slp == first) {
                                        retval |= eSeqlocPartial_Nostart;
                                    } else {
                                        retval |= eSeqlocPartial_Nointernal;
                                    }
                                }
                            }
                        }
                    }
                }
                
                if (itv.IsSetFuzz_to()) {
                    const CInt_fuzz& fuzz = itv.GetFuzz_to();
                    CInt_fuzz::ELim lim = fuzz.IsLim() ? 
                        fuzz.GetLim() : CInt_fuzz::eLim_unk;
                    if (lim == CInt_fuzz::eLim_lt) {
                        retval |= eSeqlocPartial_Limwrong;
                    } else if (lim == CInt_fuzz::eLim_gt  ||
                        lim == CInt_fuzz::eLim_unk) {
                        CBioseq_Handle hnd =
                            scope->GetBioseqHandle(itv.GetId());
                        bool miss_end = false;
                        if ( hnd ) {
                            CBioseq_Handle::TBioseqCore bc = hnd.GetBioseqCore();
                            
                            if (itv.GetTo() != bc->GetInst().GetLength() - 1) {
                                miss_end = true;
                            }
                        }
                        if (itv.IsSetStrand()  &&
                            itv.GetStrand() == eNa_strand_minus) {
                            if (slp == first) {
                                retval |= eSeqlocPartial_Start;
                            } else {
                                retval |= eSeqlocPartial_Internal;
                            }
                            if (miss_end) {
                                if (slp == first /* was last */) {
                                    retval |= eSeqlocPartial_Nostart;
                                } else {
                                    retval |= eSeqlocPartial_Nointernal;
                                }
                            }
                        } else {
                            if (slp == last) {
                                retval |= eSeqlocPartial_Stop;
                            } else {
                                retval |= eSeqlocPartial_Internal;
                            }
                            if (miss_end) {
                                if (slp == last) {
                                    retval |= eSeqlocPartial_Nostop;
                                } else {
                                    retval |= eSeqlocPartial_Nointernal;
                                }
                            }
                        }
                    }
                }
            }
            break;
        case CSeq_loc::e_Pnt:
            if (slp->GetPnt().IsSetFuzz()) {
                const CInt_fuzz& fuzz = slp->GetPnt().GetFuzz();
                if (fuzz.Which() == CInt_fuzz::e_Lim) {
                    CInt_fuzz::ELim lim = fuzz.GetLim();
                    if (lim == CInt_fuzz::eLim_gt  ||
                        lim == CInt_fuzz::eLim_lt  ||
                        lim == CInt_fuzz::eLim_unk) {
                        if (slp == first) {
                            retval |= eSeqlocPartial_Start;
                        } else if (slp == last) {
                            retval |= eSeqlocPartial_Stop;
                        } else {
                            retval |= eSeqlocPartial_Internal;
                        }
                    }
                }
            }
            break;
        case CSeq_loc::e_Packed_pnt:
            if (slp->GetPacked_pnt().IsSetFuzz()) {
                const CInt_fuzz& fuzz = slp->GetPacked_pnt().GetFuzz();
                if (fuzz.Which() == CInt_fuzz::e_Lim) {
                    CInt_fuzz::ELim lim = fuzz.GetLim();
                    if (lim == CInt_fuzz::eLim_gt  ||
                        lim == CInt_fuzz::eLim_lt  ||
                        lim == CInt_fuzz::eLim_unk) {
                        if (slp == first) {
                            retval |= eSeqlocPartial_Start;
                        } else if (slp == last) {
                            retval |= eSeqlocPartial_Stop;
                        } else {
                            retval |= eSeqlocPartial_Internal;
                        }
                    }
                }
            }
            break;
        case CSeq_loc::e_Whole:
        {
            // Get the Bioseq referred to by Whole
            CBioseq_Handle bsh = scope->GetBioseqHandle(slp->GetWhole());
            if ( !bsh ) {
                break;
            }
            // Check for CMolInfo on the biodseq
            CSeqdesc_CI di( bsh, CSeqdesc::e_Molinfo );
            if ( !di ) {
                // If no CSeq_descr, nothing can be done
                break;
            }
            // First try to loop through CMolInfo
            const CMolInfo& mi = di->GetMolinfo();
            if (!mi.IsSetCompleteness()) {
                continue;
            }
            switch (mi.GetCompleteness()) {
            case CMolInfo::eCompleteness_no_left:
                if (slp == first) {
                    retval |= eSeqlocPartial_Start;
                } else {
                    retval |= eSeqlocPartial_Internal;
                }
                break;
            case CMolInfo::eCompleteness_no_right:
                if (slp == last) {
                    retval |= eSeqlocPartial_Stop;
                } else {
                    retval |= eSeqlocPartial_Internal;
                }
                break;
            case CMolInfo::eCompleteness_partial:
                retval |= eSeqlocPartial_Other;
                break;
            case CMolInfo::eCompleteness_no_ends:
                retval |= eSeqlocPartial_Start;
                retval |= eSeqlocPartial_Stop;
                break;
            default:
                break;
            }
            break;
        }
        default:
            break;
        }
    }
    return retval;
}


END_SCOPE(sequence)


void CFastaOstream::Write(const CBioseq_Handle& handle,
                          const CSeq_loc* location)
{
    WriteTitle(handle);
    WriteSequence(handle, location);
}


void CFastaOstream::WriteTitle(const CBioseq_Handle& handle)
{
    m_Out << '>' << CSeq_id::GetStringDescr(handle.GetBioseq(),
                                            CSeq_id::eFormat_FastA)
          << ' ' << sequence::GetTitle(handle) << NcbiEndl;
}


// XXX - replace with SRelLoc?
struct SGap {
    SGap(TSeqPos start, TSeqPos length) : m_Start(start), m_Length(length) { }
    TSeqPos GetEnd(void) const { return m_Start + m_Length - 1; }

    TSeqPos m_Start, m_Length;
};
typedef list<SGap> TGaps;


static bool s_IsGap(const CSeq_loc& loc, CScope& scope)
{
    if (loc.IsNull()) {
        return true;
    }

    CTypeConstIterator<CSeq_id> id(loc);
    CBioseq_Handle handle = scope.GetBioseqHandle(*id);
    if (handle  &&  handle.GetBioseqCore()->GetInst().GetRepr()
        == CSeq_inst::eRepr_virtual) {
        return true;
    }

    return false; // default
}


static TGaps s_FindGaps(const CSeq_ext& ext, CScope& scope)
{
    TSeqPos pos = 0;
    TGaps   gaps;

    switch (ext.Which()) {
    case CSeq_ext::e_Seg:
        ITERATE (CSeg_ext::Tdata, it, ext.GetSeg().Get()) {
            TSeqPos length = sequence::GetLength(**it, &scope);
            if (s_IsGap(**it, scope)) {
                gaps.push_back(SGap(pos, length));
            }
            pos += length;
        }
        break;

    case CSeq_ext::e_Delta:
        ITERATE (CDelta_ext::Tdata, it, ext.GetDelta().Get()) {
            switch ((*it)->Which()) {
            case CDelta_seq::e_Loc:
            {
                const CSeq_loc& loc = (*it)->GetLoc();
                TSeqPos length = sequence::GetLength(loc, &scope);
                if (s_IsGap(loc, scope)) {
                    gaps.push_back(SGap(pos, length));
                }
                pos += length;
                break;
            }

            case CDelta_seq::e_Literal:
            {
                const CSeq_literal& lit    = (*it)->GetLiteral();
                TSeqPos             length = lit.GetLength();
                if ( !lit.IsSetSeq_data() ) {
                    gaps.push_back(SGap(pos, length));
                }
                pos += length;
                break;
            }

            default:
                ERR_POST(Warning << "CFastaOstream::WriteSequence: "
                         "unsupported Delta-seq selection "
                         << CDelta_seq::SelectionName((*it)->Which()));
                break;
            }
        }

    default:
        break;
    }

    return gaps;
}


static TGaps s_AdjustGaps(const TGaps& gaps, const CSeq_loc& location)
{
    // assume location matches handle
    const TSeqPos         kMaxPos = numeric_limits<TSeqPos>::max();
    TSeqPos               pos     = 0;
    TGaps::const_iterator gap_it  = gaps.begin();
    TGaps                 adjusted_gaps;
    SGap                  new_gap(kMaxPos, 0);

    for (CSeq_loc_CI loc_it(location);  loc_it  &&  gap_it != gaps.end();
         pos += loc_it.GetRange().GetLength(), ++loc_it) {
        CSeq_loc_CI::TRange range = loc_it.GetRange();

        if (new_gap.m_Start != kMaxPos) {
            // in progress
            if (gap_it->GetEnd() < range.GetFrom()) {
                adjusted_gaps.push_back(new_gap);
                new_gap.m_Start = kMaxPos;
                ++gap_it;
            } else if (gap_it->GetEnd() <= range.GetTo()) {
                new_gap.m_Length += gap_it->GetEnd() - range.GetFrom() + 1;
                adjusted_gaps.push_back(new_gap);
                new_gap.m_Start = kMaxPos;
                ++gap_it;
            } else {
                new_gap.m_Length += range.GetLength();
                continue;
            }
        }

        while (gap_it->GetEnd() < range.GetFrom()) {
            ++gap_it; // skip
        }

        if (gap_it->m_Start <= range.GetFrom()) {
            if (gap_it->GetEnd() <= range.GetTo()) {
                adjusted_gaps.push_back
                    (SGap(pos, gap_it->GetEnd() - range.GetFrom() + 1));
                ++gap_it;
            } else {
                new_gap.m_Start  = pos;
                new_gap.m_Length = range.GetLength();
                continue;
            }
        }

        while (gap_it->m_Start <= range.GetTo()) {
            TSeqPos pos2 = pos + gap_it->m_Start - range.GetFrom();
            if (gap_it->GetEnd() <= range.GetTo()) {
                adjusted_gaps.push_back(SGap(pos2, gap_it->m_Length));
                ++gap_it;
            } else {
                new_gap.m_Start  = pos2;
                new_gap.m_Length = range.GetTo() - gap_it->m_Start + 1;
            }
        }
    }

    if (new_gap.m_Start != kMaxPos) {
        adjusted_gaps.push_back(new_gap);
    }

    return adjusted_gaps;
}


void CFastaOstream::WriteSequence(const CBioseq_Handle& handle,
                                  const CSeq_loc* location)
{
    const CBioseq&   seq  = handle.GetBioseq();
    const CSeq_inst& inst = seq.GetInst();
    if ( !(m_Flags & eAssembleParts)  &&  !inst.IsSetSeq_data() ) {
        return;
    }

    CSeqVector v = (location
                    ? handle.GetSequenceView(*location,
                                             CBioseq_Handle::eViewMerged,
                                             CBioseq_Handle::eCoding_Iupac)
                    : handle.GetSeqVector(CBioseq_Handle::eCoding_Iupac));
    bool is_na = inst.GetMol() != CSeq_inst::eMol_aa;
    // autodetection is sometimes broken (!)
    v.SetCoding(is_na ? CSeq_data::e_Iupacna : CSeq_data::e_Iupacaa);

    TSeqPos              size = v.size();
    TSeqPos              pos  = 0;
    CSeqVector::TResidue gap  = v.GetGapChar();
    string               buffer;
    TGaps                gaps;
    CScope&              scope   = handle.GetScope();
    const TSeqPos        kMaxPos = numeric_limits<TSeqPos>::max();

    if ( !inst.IsSetSeq_data()  &&  inst.IsSetExt() ) {
        gaps = s_FindGaps(inst.GetExt(), scope);
        if (location) {
            gaps = s_AdjustGaps(gaps, *location);
        }
    }
    gaps.push_back(SGap(kMaxPos, 0));

    while (pos < size) {
        unsigned int limit = min(m_Width,
                                 min(size, gaps.front().m_Start) - pos);
        v.GetSeqData(pos, pos + limit, buffer);
        pos += limit;
        if (limit > 0) {
            m_Out << buffer << '\n';
        }
        if (pos == gaps.front().m_Start) {
            if (m_Flags & eInstantiateGaps) {
                for (TSeqPos l = gaps.front().m_Length; l > 0; l -= m_Width) {
                    m_Out << string(min(l, m_Width), gap) << '\n';
                }
            } else {
                m_Out << "-\n";
            }
            pos += gaps.front().m_Length;
            gaps.pop_front();
        }
    }
    m_Out << NcbiFlush;
}


void CFastaOstream::Write(CSeq_entry& entry, const CSeq_loc* location)
{
    CObjectManager om;
    CScope         scope(om);

    scope.AddTopLevelSeqEntry(entry);
    for (CTypeConstIterator<CBioseq> it(entry);  it;  ++it) {
        if ( !SkipBioseq(*it) ) {
            CBioseq_Handle h = scope.GetBioseqHandle(*it->GetId().front());
            Write(h, location);
        }
    }
}


void CFastaOstream::Write(CBioseq& seq, const CSeq_loc* location)
{
    CSeq_entry entry;
    entry.SetSeq(seq);
    Write(entry, location);
}


void CCdregion_translate::ReadSequenceByLocation (string& seq,
                                                  CBioseq_Handle& bsh,
                                                  const CSeq_loc& loc)

{
    // get vector of sequence under location
    CSeqVector seqv = bsh.GetSequenceView (loc,
                                           CBioseq_Handle::eViewConstructed,
                                           CBioseq_Handle::eCoding_Iupac);
    seqv.GetSeqData(0, seqv.size() - 1, seq);
}

void CCdregion_translate::TranslateCdregion (string& prot,
                                             CBioseq_Handle& bsh,
                                             const CSeq_loc& loc,
                                             const CCdregion& cdr,
                                             bool include_stop,
                                             bool remove_trailing_X)

{
    // clear contents of result string
    prot.erase();

    // copy bases from coding region location
    string bases = "";
    ReadSequenceByLocation (bases, bsh, loc);

    // calculate offset from frame parameter
    int offset = 0;
    if (cdr.IsSetFrame ()) {
        switch (cdr.GetFrame ()) {
            case CCdregion::eFrame_two :
                offset = 1;
                break;
            case CCdregion::eFrame_three :
                offset = 2;
                break;
            default :
                break;
        }
    }

    int dnalen = bases.size () - offset;
    if (dnalen < 1) return;

    // pad bases string if last codon is incomplete
    bool incomplete_last_codon = false;
    int mod = dnalen % 3;
    if (mod == 1) {
        bases += "NN";
        incomplete_last_codon = true;
    } else if (mod == 2) {
        bases += "N";
        incomplete_last_codon = true;
    }

    // resize output protein translation string
    prot.resize ((dnalen) / 3);

    // get appropriate translation table
    const CTrans_table & tbl = (cdr.IsSetCode () ?
                                CGen_code_table::GetTransTable (cdr.GetCode ()) :
                                CGen_code_table::GetTransTable (1));

    // main loop through bases
    unsigned int i, j, state;
    for (i = offset, j = 0, state = 0; i < bases.size (); j++) {

        // loop through one codon at a time
        for (unsigned int k = 0; k < 3; i++, k++) {
            char ch = bases [i];
            state = tbl.NextCodonState (state, ch);
        }

        // save translated amino acid
        prot [j] = tbl.GetCodonResidue (state);
    }

    // if complete at 5' end, require valid start codon
    if (offset == 0  &&  (! loc.IsPartialLeft ())) {
        state = tbl.SetCodonState (bases [offset], bases [offset + 1], bases [offset + 2]);
        prot [0] = tbl.GetStartResidue (state);
    }

    // code break substitution
    if (cdr.IsSetCode_break ()) {
        SIZE_TYPE protlen = prot.size ();
        ITERATE (CCdregion::TCode_break, code_break, cdr.GetCode_break ()) {
            const CRef <CCode_break> brk = *code_break;
            const CSeq_loc& cbk_loc = brk->GetLoc ();
            TSeqPos seq_pos = sequence::LocationOffset (loc, cbk_loc, sequence::eOffset_FromStart, &bsh.GetScope ());
            seq_pos -= offset;
            i = seq_pos / 3;
            if (i >= 0 && i < protlen) {
              const CCode_break::C_Aa& c_aa = brk->GetAa ();
              if (c_aa.IsNcbieaa ()) {
                prot [i] = c_aa.GetNcbieaa ();
              }
            }
        }
    }

    // optionally truncate at first terminator
    if (! include_stop) {
        SIZE_TYPE protlen = prot.size ();
        for (i = 0; i < protlen; i++) {
            if (prot [i] == '*') {
                prot.resize (i);
                return;
            }
        }
    }

    // if padding was needed, trim ambiguous last residue
    if (incomplete_last_codon) {
        int protlen = prot.size ();
        if (protlen > 0 && prot [protlen - 1] == 'X') {
            protlen--;
            prot.resize (protlen);
        }
    }

    // optionally remove trailing X on 3' partial coding region
    if (remove_trailing_X) {
        int protlen = prot.size ();
        while (protlen > 0 && prot [protlen - 1] == 'X') {
            protlen--;
        }
        prot.resize (protlen);
    }
}


SRelLoc::SRelLoc(const CSeq_loc& parent, const CSeq_loc& child, CScope* scope,
                 SRelLoc::TFlags flags)
    : m_ParentLoc(&parent)
{
    typedef CSeq_loc::TRange TRange0;
    for (CSeq_loc_CI cit(child);  cit;  ++cit) {
        const CSeq_id& cseqid  = cit.GetSeq_id();
        TRange0        crange  = cit.GetRange();
        if (crange.IsWholeTo()  &&  scope) {
            // determine actual end
            crange.SetToOpen(sequence::GetLength(cit.GetSeq_id(), scope));
        }
        ENa_strand     cstrand = cit.GetStrand();
        TSeqPos        pos     = 0;
        for (CSeq_loc_CI pit(parent);  pit;  ++pit) {
            if ( !sequence::IsSameBioseq(cseqid, pit.GetSeq_id(), scope) ) {
                continue;
            }
            TRange0 prange = pit.GetRange();
            if (prange.IsWholeTo()  &&  scope) {
                // determine actual end
                prange.SetToOpen(sequence::GetLength(pit.GetSeq_id(), scope));
            }
            CRef<TRange> intersection(new TRange);
            intersection->SetFrom(max(prange.GetFrom(), crange.GetFrom()));
            intersection->SetTo  (min(prange.GetTo(),   crange.GetTo()));
            if (intersection->GetFrom() <= intersection->GetTo()) {
                if ( !SameOrientation(cstrand, pit.GetStrand()) ) {
                    ERR_POST(Warning
                             << "SRelLoc::SRelLoc:"
                             " parent and child have opposite orientations");
                }
                if (IsReverse(cstrand)) { // both strands reverse
                    TSeqPos sigma = pos + prange.GetTo();
                    TSeqPos from0 = intersection->GetFrom();
                    intersection->SetFrom(sigma - intersection->GetTo());
                    intersection->SetTo  (sigma - from0);
                } else { // both strands forward
                    TSignedSeqPos delta = pos - prange.GetFrom();
                    intersection->SetFrom(intersection->GetFrom() + delta);
                    intersection->SetTo  (intersection->GetTo()   + delta);
                }
                // add to m_Ranges, combining with the previous
                // interval if possible
                if ( !(flags & fNoMerge)  &&  !m_Ranges.empty()
                    && m_Ranges.back()->GetTo() == intersection->GetFrom() - 1)
                {
                    m_Ranges.back()->SetTo(intersection->GetTo());
                } else {
                    m_Ranges.push_back(intersection);
                }
            }
            pos += prange.GetLength();
        }
    }
}


// Bother trying to merge?
CRef<CSeq_loc> SRelLoc::Resolve(CScope* scope, SRelLoc::TFlags /* flags */)
    const
{
    typedef CSeq_loc::TRange TRange0;
    CRef<CSeq_loc> result(new CSeq_loc);
    CSeq_loc_mix&  mix = result->SetMix();
    ITERATE (TRanges, it, m_Ranges) {
        _ASSERT((*it)->GetFrom() <= (*it)->GetTo());
        TSeqPos pos = 0, start = (*it)->GetFrom();
        bool    keep_going = true;
        for (CSeq_loc_CI pit(*m_ParentLoc);  pit;  ++pit) {
            TRange0 prange = pit.GetRange();
            if (prange.IsWholeTo()  &&  scope) {
                // determine actual end
                prange.SetToOpen(sequence::GetLength(pit.GetSeq_id(), scope));
            }
            TSeqPos length = prange.GetLength();
            if (start >= pos  &&  start < pos + length) {
                TSeqPos from, to;
                if (IsReverse(pit.GetStrand())) {
                    TSeqPos sigma = pos + prange.GetTo();
                    from = sigma - (*it)->GetTo();
                    to   = sigma - start;
                    if (from < prange.GetFrom()  ||  from > sigma) {
                        from = prange.GetFrom();
                        keep_going = true;
                    } else {
                        keep_going = false;
                    }
                } else {
                    TSignedSeqPos delta = prange.GetFrom() - pos;
                    from = start          + delta;
                    to   = (*it)->GetTo() + delta;
                    if (to > prange.GetTo()) {
                        to = prange.GetTo();
                        keep_going = true;
                    } else {
                        keep_going = false;
                    }
                }
                if (from == to) {
                    // just a point
                    CRef<CSeq_loc> loc(new CSeq_loc);
                    CSeq_point& point = loc->SetPnt();
                    point.SetPoint(from);
                    point.SetStrand(pit.GetStrand());
                    point.SetId().Assign(pit.GetSeq_id());
                    mix.Set().push_back(loc);
                } else {
                    CRef<CSeq_loc> loc(new CSeq_loc);
                    CSeq_interval& ival = loc->SetInt();
                    ival.SetFrom(from);
                    ival.SetTo(to);
                    ival.SetStrand(pit.GetStrand());
                    ival.SetId().Assign(pit.GetSeq_id());
                    mix.Set().push_back(loc);
                }
                if (keep_going) {
                    start = pos + length;
                } else {
                    break;
                }
            }
            pos += length;
        }
        if (keep_going) {
            TSeqPos total_length;
            string  label;
            m_ParentLoc->GetLabel(&label);
            try {
                total_length = sequence::GetLength(*m_ParentLoc, scope);
                ERR_POST(Warning << "SRelLoc::Resolve: Relative position "
                         << start << " exceeds length (" << total_length
                         << ") of parent location " << label);
            } catch (sequence::CNoLength) {
                ERR_POST(Warning << "SRelLoc::Resolve: Relative position "
                         << start
                         << " exceeds length (?\?\?) of parent location "
                         << label);
            }            
        }
    }
    // clean up output
    switch (mix.Get().size()) {
    case 0:
        result->SetNull();
        break;
    case 1:
        result.Reset(mix.Set().front());
        break;
    default:
        break;
    }
    return result;
}


//============================================================================//
//                                 SeqSearch                                  //
//============================================================================//

// Public:
// =======

// Constructors and Destructors:
CSeqSearch::CSeqSearch(IClient *client, bool allow_mismatch) :
    m_AllowOneMismatch(allow_mismatch),
    m_MaxPatLen(0),
    m_Client(client)
{
    InitializeMaps();
}


CSeqSearch::~CSeqSearch(void) {}


// Add nucleotide pattern or restriction site to sequence search.
// Uses ambiguity codes, e.g., R = A and G, H = A, C and T
void CSeqSearch::AddNucleotidePattern
(const string& name,
const string& pat, 
int cut_site,
int overhang)
{
    string pattern = pat;
    NStr::TruncateSpaces(pattern);
    NStr::ToUpper(pattern);

    // reverse complement pattern to see if it is symetrical
    string rcomp = ReverseComplement(pattern);

    bool symmetric = (pattern == rcomp);

    ENa_strand strand = symmetric ? eNa_strand_both : eNa_strand_plus;

    AddNucleotidePattern(name, pat, cut_site, overhang, strand);
    if ( !symmetric ) {
        AddNucleotidePattern(name, rcomp, pat.length() - cut_site, 
                             overhang, eNa_strand_minus);
    }
}


// Program passes each character in turn to finite state machine.
int CSeqSearch::Search
(int current_state,
 char ch,
 int position,
 int length)
{
    if ( !m_Client ) return 0;

    // on first character, populate state transition table
    if ( !m_Fsa.IsPrimed() ) {
        m_Fsa.Prime();
    }
    
    int next_state = m_Fsa.GetNextState(current_state, ch);
    
    // report any matches at current state to the client object
    if ( m_Fsa.IsMatchFound(next_state) ) {
        ITERATE( vector<CMatchInfo>, it, m_Fsa.GetMatches(next_state) ) {
	  //const CMatchInfo& match = *it;
            int start = position - it->GetPattern().length() + 1;    

            // prevent multiple reports of patterns for circular sequences.
            if ( start < length ) {
                m_Client->MatchFound(*it, start);
            }
        }
    }

    return next_state;
}


// Search an entire bioseq.
void CSeqSearch::Search(const CBioseq_Handle& bsh)
{
    if ( !bsh ) return;
    if ( !m_Client ) return;  // no one to report to, so why search at all.

    CSeqVector seq_vec = bsh.GetSeqVector(CBioseq_Handle::eCoding_Iupac);
    size_t seq_len = seq_vec.size();
    size_t search_len = seq_len;

    CSeq_inst::ETopology topology = bsh.GetBioseq().GetInst().GetTopology();    
    if ( topology == CSeq_inst::eTopology_circular ) {
        search_len += m_MaxPatLen - 1;
    }
    
    int state = m_Fsa.GetInitialState();

    for ( size_t i = 0; i < search_len; ++i ) {
        state = Search(state, seq_vec[i % seq_len], i, seq_len);
    }
}


// Private:
// ========

// translation finite state machine base codes - ncbi4na
enum EBaseCode {
    eBase_gap = 0,
        eBase_A,      /* A    */
        eBase_C,      /* C    */
        eBase_M,      /* AC   */
        eBase_G,      /* G    */
        eBase_R,      /* AG   */
        eBase_S,      /* CG   */
        eBase_V,      /* ACG  */
        eBase_T,      /* T    */
        eBase_W,      /* AT   */
        eBase_Y,      /* CT   */
        eBase_H,      /* ACT  */
        eBase_K,      /* GT   */
        eBase_D,      /* AGT  */
        eBase_B,      /* CGT  */
        eBase_N       /* ACGT */
};


map<unsigned char, int> CSeqSearch::sm_CharToEnum;
map<int, unsigned char> CSeqSearch::sm_EnumToChar;
map<char, char>         CSeqSearch::sm_Complement;


void CSeqSearch::InitializeMaps(void)
{
    int c, i;
    static const string iupacna_alphabet      = "-ACMGRSVTWYHKDBN";
    static const string comp_iupacna_alphabet = "-TGKCYSBAWRDMHVN";

    if ( sm_CharToEnum.empty() ) {
        // illegal characters map to eBase_gap (0)
        for ( c = 0; c < 256; ++c ) {
            sm_CharToEnum[c] = eBase_gap;
        }
        
        // map iupacna alphabet to EBaseCode
        for (i = eBase_gap; i <= eBase_N; ++i) {
            c = iupacna_alphabet[i];
            sm_CharToEnum[c] = (EBaseCode)i;
            c = tolower(c);
            sm_CharToEnum[c] = (EBaseCode)i;
        }
        sm_CharToEnum ['U'] = eBase_T;
        sm_CharToEnum ['u'] = eBase_T;
        sm_CharToEnum ['X'] = eBase_N;
        sm_CharToEnum ['x'] = eBase_N;
        
        // also map ncbi4na alphabet to EBaseCode
        for (c = eBase_gap; c <= eBase_N; ++c) {
            sm_CharToEnum[c] = (EBaseCode)c;
        }
    }
    
    // map EBaseCode to iupacna alphabet
    if ( sm_EnumToChar.empty() ) { 
        for (i = eBase_gap; i <= eBase_N; ++i) {
            sm_EnumToChar[i] = iupacna_alphabet[i];
        }
    }

    // initialize table to convert character to complement character
    if ( sm_Complement.empty() ) {
        int len = iupacna_alphabet.length();
        for ( i = 0; i < len; ++i ) {
            sm_Complement.insert(make_pair(iupacna_alphabet[i], comp_iupacna_alphabet[i]));
        }
    }
}


string CSeqSearch::ReverseComplement(const string& pattern) const
{
    size_t len = pattern.length();
    string rcomp = pattern;
    rcomp.resize(len);

    // calculate the complement
    for ( size_t i = 0; i < len; ++i ) {
        rcomp[i] = sm_Complement[pattern[i]];
    }

    // reverse the complement
    reverse(rcomp.begin(), rcomp.end());

    return rcomp;
}


void CSeqSearch::AddNucleotidePattern
(const string& name,
const string& pattern, 
int cut_site,
int overhang,
ENa_strand strand)
{
    size_t pat_len = pattern.length();
    string temp;
    temp.resize(pat_len);
    CMatchInfo info(name,
                    CNcbiEmptyString::Get(), 
                    cut_site, 
                    overhang, 
                    strand);

    if ( pat_len > m_MaxPatLen ) {
        m_MaxPatLen = pat_len;
    }

    ExpandPattern(pattern, temp, 0, pat_len, info);
}


void CSeqSearch::ExpandPattern
(const string& pattern,
string& temp,
int position,
int pat_len,
CMatchInfo& info)
{
    static EBaseCode expension[] = { eBase_A, eBase_C, eBase_G, eBase_T };

    if ( position < pat_len ) {
        int code = sm_CharToEnum[(int)pattern[position]];
        
        for ( int i = 0; i < 4; ++i ) {
            if ( code & expension[i] ) {
                temp[position] = sm_EnumToChar[expension[i]];
                ExpandPattern(pattern, temp, position + 1, pat_len, info);
            }
        }
    } else { // recursion base
        // when position reaches pattern length, store one expanded string.
        info.m_Pattern = temp;
        m_Fsa.AddWord(info.m_Pattern, info);

        if ( m_AllowOneMismatch ) {
            char ch;
            // put 'N' at every position if a single mismatch is allowed.
            for ( int i = 0; i < pat_len; ++i ) {
                ch = temp[i];
                temp[i] = 'N';

                info.m_Pattern = temp;
                m_Fsa.AddWord(pattern, info);

                // restore proper character, go on to put N in next position.
                temp[i] = ch;
            }
        }
    }
}




END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ===========================================================================
* $Log$
* Revision 1.54  2003/06/02 18:53:32  dicuccio
* Fixed bungled commit
*
* Revision 1.52  2003/05/27 19:44:10  grichenk
* Added CSeqVector_CI class
*
* Revision 1.51  2003/05/15 19:27:02  shomrat
* Compare handle only if both valid; Check IsLim before GetLim
*
* Revision 1.50  2003/05/09 15:37:00  ucko
* Take const CBioseq_Handle references in CFastaOstream::Write et al.
*
* Revision 1.49  2003/05/06 19:34:36  grichenk
* Fixed GetStrand() (worked fine only for plus and unknown strands)
*
* Revision 1.48  2003/05/01 17:55:17  ucko
* Fix GetLength(const CSeq_id&, CScope*) to return ...::max() rather
* than throwing if it can't resolve the ID to a handle.
*
* Revision 1.47  2003/04/24 16:15:58  vasilche
* Added missing includes and forward class declarations.
*
* Revision 1.46  2003/04/16 19:44:26  grichenk
* More fixes to TestForOverlap() and GetStrand()
*
* Revision 1.45  2003/04/15 20:11:21  grichenk
* Fixed strands problem in TestForOverlap(), replaced type
* iterator with container iterator in s_GetStrand().
*
* Revision 1.44  2003/04/03 19:03:17  grichenk
* Two more cases to revert locations in GetBestOverlappingFeat()
*
* Revision 1.43  2003/04/02 16:58:59  grichenk
* Change location and feature in GetBestOverlappingFeat()
* for eOverlap_Subset.
*
* Revision 1.42  2003/03/25 22:00:20  grichenk
* Added strand checking to TestForOverlap()
*
* Revision 1.41  2003/03/18 21:48:35  grichenk
* Removed obsolete class CAnnot_CI
*
* Revision 1.40  2003/03/11 16:00:58  kuznets
* iterate -> ITERATE
*
* Revision 1.39  2003/02/19 16:25:14  grichenk
* Check strands in GetBestOverlappingFeat()
*
* Revision 1.38  2003/02/14 15:41:00  shomrat
* Minor implementation changes in SeqLocPartialTest
*
* Revision 1.37  2003/02/13 14:35:40  grichenk
* + eOverlap_Contains
*
* Revision 1.36  2003/02/10 15:54:01  grichenk
* Use CFeat_CI->GetMappedFeature() and GetOriginalFeature()
*
* Revision 1.35  2003/02/06 22:26:27  vasilche
* Use CSeq_id::Assign().
*
* Revision 1.34  2003/02/06 20:59:16  shomrat
* Bug fix in SeqLocPartialCheck
*
* Revision 1.33  2003/01/22 21:02:23  ucko
* Fix stupid logic error in SRelLoc's constructor; change LocationOffset
* to return -1 rather than crashing if the locations don't intersect.
*
* Revision 1.32  2003/01/22 20:15:02  vasilche
* Removed compiler warning.
*
* Revision 1.31  2003/01/22 18:17:09  ucko
* SRelLoc::SRelLoc: change intersection to a CRef, so we don't have to
* worry about it going out of scope while still referenced (by m_Ranges).
*
* Revision 1.30  2003/01/08 20:43:10  ucko
* Adjust SRelLoc to use (ID-less) Seq-intervals for ranges, so that it
* will be possible to add support for fuzz and strandedness/orientation.
*
* Revision 1.29  2002/12/30 19:38:35  vasilche
* Optimized CGenbankWriter::WriteSequence.
* Implemented GetBestOverlappingFeat() with CSeqFeatData::ESubtype selector.
*
* Revision 1.28  2002/12/26 21:45:29  grichenk
* + GetBestOverlappingFeat()
*
* Revision 1.27  2002/12/26 21:17:06  dicuccio
* Minor tweaks to avoid compiler warnings in MSVC (remove unused variables)
*
* Revision 1.26  2002/12/20 17:14:18  kans
* added SeqLocPartialCheck
*
* Revision 1.25  2002/12/19 20:24:55  grichenk
* Updated usage of CRange<>
*
* Revision 1.24  2002/12/10 16:56:41  ucko
* CFastaOstream::WriteTitle: restore leading > accidentally dropped in R1.19.
*
* Revision 1.23  2002/12/10 16:34:45  kans
* implement code break processing in TranslateCdregion
*
* Revision 1.22  2002/12/09 20:38:41  ucko
* +sequence::LocationOffset
*
* Revision 1.21  2002/12/06 15:36:05  grichenk
* Added overlap type for annot-iterators
*
* Revision 1.20  2002/12/04 15:38:22  ucko
* SourceToProduct, ProductToSource: just check whether the feature is a coding
* region rather than attempting to determine molecule types; drop s_DeduceMol.
*
* Revision 1.19  2002/11/26 15:14:04  dicuccio
* Changed CFastaOStream::WriteTitle() to make use of CSeq_id::GetStringDescr().
*
* Revision 1.18  2002/11/25 21:24:46  grichenk
* Added TestForOverlap() function.
*
* Revision 1.17  2002/11/18 19:59:23  shomrat
* Add CSeqSearch - a nucleotide search utility
*
* Revision 1.16  2002/11/12 20:00:25  ucko
* +SourceToProduct, ProductToSource, SRelLoc
*
* Revision 1.15  2002/10/23 19:22:39  ucko
* Move the FASTA reader from objects/util/sequence.?pp to
* objects/seqset/Seq_entry.?pp because it doesn't need the OM.
*
* Revision 1.14  2002/10/23 18:23:59  ucko
* Clean up #includes.
* Add a FASTA reader (known to compile, but not otherwise tested -- take care)
*
* Revision 1.13  2002/10/23 16:41:12  clausen
* Fixed warning in WorkShop53
*
* Revision 1.12  2002/10/23 15:33:50  clausen
* Fixed local variable scope warnings
*
* Revision 1.11  2002/10/08 12:35:37  clausen
* Fixed bugs in GetStrand(), ChangeSeqId() & SeqLocCheck()
*
* Revision 1.10  2002/10/07 17:11:16  ucko
* Fix usage of ERR_POST (caught by KCC)
*
* Revision 1.9  2002/10/03 18:44:09  clausen
* Removed extra whitespace
*
* Revision 1.8  2002/10/03 16:33:55  clausen
* Added functions needed by validate
*
* Revision 1.7  2002/09/13 15:30:43  ucko
* Change resize(0) to erase() at Denis's suggestion.
*
* Revision 1.6  2002/09/13 14:28:34  ucko
* string::clear() doesn't exist under g++ 2.95, so use resize(0) instead. :-/
*
* Revision 1.5  2002/09/12 21:39:13  kans
* added CCdregion_translate and CCdregion_translate
*
* Revision 1.4  2002/09/03 21:27:04  grichenk
* Replaced bool arguments in CSeqVector constructor and getters
* with enums.
*
* Revision 1.3  2002/08/27 21:41:15  ucko
* Add CFastaOstream.
*
* Revision 1.2  2002/06/07 16:11:09  ucko
* Move everything into the "sequence" namespace.
* Drop the anonymous-namespace business; "sequence" should provide
* enough protection, and anonymous namespaces ironically interact poorly
* with WorkShop's caching when rebuilding shared libraries.
*
* Revision 1.1  2002/06/06 18:41:41  clausen
* Initial version
*
* ===========================================================================
*/
