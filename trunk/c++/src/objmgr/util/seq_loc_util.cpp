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
* Author:  Clifford Clausen, Aaron Ucko, Aleksey Grichenko
*
* File Description:
*   Seq-loc utilities requiring CScope
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbi_limits.hpp>

#include <serial/iterator.hpp>

#include <objects/seq/seq_id_mapper.hpp>
#include <objects/seq/seq_id_handle.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/seq/MolInfo.hpp>

#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seqloc/Packed_seqint.hpp>
#include <objects/seqloc/Seq_loc_mix.hpp>
#include <objects/seqloc/Seq_point.hpp>
#include <objects/seqloc/Seq_bond.hpp>
#include <objects/seqloc/Seq_loc_equiv.hpp>

#include <objmgr/util/seq_loc_util.hpp>
#include <objmgr/util/sequence.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/seqdesc_ci.hpp>
#include <objmgr/objmgr_exception.hpp>
#include <objmgr/seq_loc_mapper.hpp>
#include <objmgr/impl/handle_range_map.hpp>
#include <objmgr/impl/synonyms.hpp>
#include <objmgr/error_codes.hpp>

#include <util/range_coll.hpp>

#include <algorithm>


#define NCBI_USE_ERRCODE_X   ObjMgr_SeqUtil

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
    return hnd.GetBioseqLength();
}


TSeqPos GetLength(const CSeq_loc& loc, CScope* scope)
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
    case CSeq_loc::e_Bond:         // return number of points
        return loc.GetBond().IsSetB() + loc.GetBond().IsSetA();
    case CSeq_loc::e_not_set:      //can't calculate length
    case CSeq_loc::e_Feat:
    case CSeq_loc::e_Equiv:        // unless actually the same length...
    default:
        NCBI_THROW(CObjmgrUtilException, eUnknownLength,
            "Unable to determine length");
    }
}


namespace {
    struct SCoverageCollector {
        SCoverageCollector(const CSeq_loc& loc, CScope* scope)
            {
                Add(loc, scope);
            }
        void Add(const CSeq_loc& loc, CScope* scope)
            {
                switch (loc.Which()) {
                case CSeq_loc::e_Pnt:
                    Add(loc.GetPnt());
                    return;
                case CSeq_loc::e_Int:
                    Add(loc.GetInt());
                    return;
                case CSeq_loc::e_Null:
                case CSeq_loc::e_Empty:
                    return;
                case CSeq_loc::e_Whole:
                    AddWhole(loc.GetWhole(), scope);
                    return;
                case CSeq_loc::e_Mix:
                    Add(loc.GetMix(), scope);
                    return;
                case CSeq_loc::e_Packed_int:
                    Add(loc.GetPacked_int());
                    return;
                case CSeq_loc::e_Packed_pnt:
                    Add(loc.GetPacked_pnt());
                    return;
                case CSeq_loc::e_Bond:
                    Add(loc.GetBond().GetA());
                    if ( loc.GetBond().IsSetB() ) {
                        Add(loc.GetBond().GetB());
                    }
                    return;
                case CSeq_loc::e_not_set: //can't calculate coverage
                case CSeq_loc::e_Feat:
                case CSeq_loc::e_Equiv: // unless actually the same length...
                default:
                    NCBI_THROW(CObjmgrUtilException, eUnknownLength,
                               "Unable to determine coverage");
                }
            }
        void Add(const CSeq_id_Handle& idh, TSeqPos from, TSeqPos to)
            {
                m_Intervals[idh] += TRange(from, to);
            }
        void Add(const CSeq_id& id, TSeqPos from, TSeqPos to)
            {
                Add(CSeq_id_Handle::GetHandle(id), from, to);
            }
        void AddWhole(const CSeq_id& id, CScope* scope)
            {
                Add(id, 0, GetLength(id, scope)-1);
            }
        void Add(const CSeq_point& seq_pnt)
            {
                Add(seq_pnt.GetId(), seq_pnt.GetPoint(), seq_pnt.GetPoint());
            }
        void Add(const CSeq_interval& seq_int)
            {
                Add(seq_int.GetId(), seq_int.GetFrom(), seq_int.GetTo());
            }
        void Add(const CPacked_seqint& packed_int)
            {
                ITERATE ( CPacked_seqint::Tdata, it, packed_int.Get() ) {
                    Add(**it);
                }
            }
        void Add(const CPacked_seqpnt& packed_pnt)
            {
                CSeq_id_Handle idh =
                    CSeq_id_Handle::GetHandle(packed_pnt.GetId());
                ITERATE(CPacked_seqpnt::TPoints, it, packed_pnt.GetPoints()) {
                    Add(idh, *it, *it);
                }
            }
        void Add(const CSeq_loc_mix& mix, CScope* scope)
            {
                ITERATE ( CSeq_loc_mix::Tdata, it, mix.Get() ) {
                    Add(**it, scope);
                }
            }

        TSeqPos GetCoverage(void) const
            {
                TSeqPos coverage = 0;
                ITERATE ( TIntervals, it, m_Intervals ) {
                    ITERATE ( TRanges, it2, it->second ) {
                        coverage += it2->GetLength();
                    }
                }
                return coverage;
            }

    private:
        typedef CRange<TSeqPos> TRange;
        typedef CRangeCollection<TSeqPos> TRanges;
        typedef map<CSeq_id_Handle, TRanges> TIntervals;
        TIntervals m_Intervals;
    };
}


TSeqPos GetCoverage(const CSeq_loc& loc, CScope* scope)
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
    case CSeq_loc::e_Mix:
    case CSeq_loc::e_Packed_pnt:
    case CSeq_loc::e_Bond:
        return SCoverageCollector(loc, scope).GetCoverage();
    case CSeq_loc::e_not_set:      // can't calculate length
    case CSeq_loc::e_Feat:
    case CSeq_loc::e_Equiv:        // unless actually the same length...
    default:
        NCBI_THROW(CObjmgrUtilException, eUnknownLength,
            "Unable to determine length");
    }
}


TSeqPos GetLength(const CSeq_loc_mix& mix, CScope* scope)
{
    TSeqPos length = 0;

    ITERATE( CSeq_loc_mix::Tdata, i, mix.Get() ) {
        TSeqPos ret = GetLength((**i), scope);
        if (ret < numeric_limits<TSeqPos>::max()) {
            length += ret;
        }
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


bool IsSameBioseq(const CSeq_id& id1, const CSeq_id& id2, CScope* scope,
                  CScope::EGetBioseqFlag get_flag)
{
    return IsSameBioseq(CSeq_id_Handle::GetHandle(id1),
                        CSeq_id_Handle::GetHandle(id2),
                        scope, get_flag);
}


bool IsSameBioseq(const CSeq_id_Handle& id1, const CSeq_id_Handle& id2, CScope* scope,
                  CScope::EGetBioseqFlag get_flag)
{
    // Compare CSeq_ids directly
    if (id1 == id2) {
        return true;
    }

    // Compare handles
    if (scope != NULL) {
        try {
            CBioseq_Handle hnd1 = scope->GetBioseqHandle(id1, get_flag);
            CBioseq_Handle hnd2 = scope->GetBioseqHandle(id2, get_flag);
            return hnd1  &&  hnd2  &&  (hnd1 == hnd2);
        } catch (CException& e) {
            ERR_POST_X(1, e.what() << ": CSeq_id1: " << id1.GetSeqId()->DumpAsFasta()
                       << ": CSeq_id2: " << id2.GetSeqId()->DumpAsFasta());
        }
    }

    return false;
}


static const CSeq_id* s_GetId(const CSeq_loc& loc, CScope* scope,
                              string* msg = NULL)
{
    const CSeq_id* sip = NULL;
    if (msg != NULL) {
        msg->erase();
    }

    for (CSeq_loc_CI it(loc, CSeq_loc_CI::eEmpty_Allow); it; ++it) {
        const CSeq_id& id = it.GetSeq_id();
        if (id.Which() == CSeq_id::e_not_set) {
            continue;
        }
        if (sip == NULL) {
            sip = &id;
        } else {
            if (!IsSameBioseq(*sip, id, scope)) {
                if (msg != NULL) {
                    *msg = "Location contains segments on more than one bioseq.";
                }
                sip = NULL;
                break;
            }
        }
    }

    if (sip == NULL  &&  msg != NULL  &&  msg->empty()) {
        *msg = "Location contains no IDs.";
    }

    return sip;
}


const CSeq_id& GetId(const CSeq_loc& loc, CScope* scope)
{
    string msg;
    const CSeq_id* sip = s_GetId(loc, scope, &msg);

    if (sip == NULL) {
        NCBI_THROW(CObjmgrUtilException, eNotUnique, msg);
    }

    return *sip;
}


CSeq_id_Handle GetIdHandle(const CSeq_loc& loc, CScope* scope)
{
    CSeq_id_Handle retval;

    try {
        if (!loc.IsNull()) {
            retval = CSeq_id_Handle::GetHandle(GetId(loc, scope));
        }
    } catch (CObjmgrUtilException&) {}

    return retval;
}


bool IsOneBioseq(const CSeq_loc& loc, CScope* scope)
{
    return s_GetId(loc, scope) != NULL;
}


inline
static ENa_strand s_GetStrand(const CSeq_loc& loc)
{
    switch (loc.Which()) {
    case CSeq_loc::e_Bond:
        {
            const CSeq_bond& bond = loc.GetBond();
            ENa_strand a_strand = bond.GetA().IsSetStrand() ?
                bond.GetA().GetStrand() : eNa_strand_unknown;
            ENa_strand b_strand = eNa_strand_unknown;
            if ( bond.IsSetB() ) {
                b_strand = bond.GetB().IsSetStrand() ?
                    bond.GetB().GetStrand() : eNa_strand_unknown;
            }

            if ( a_strand == eNa_strand_unknown  &&
                 b_strand != eNa_strand_unknown ) {
                a_strand = b_strand;
            } else if ( a_strand != eNa_strand_unknown  &&
                        b_strand == eNa_strand_unknown ) {
                b_strand = a_strand;
            }

            return (a_strand != b_strand) ? eNa_strand_other : a_strand;
        }
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
    case CSeq_loc::e_Mix:
    {
        ENa_strand strand = eNa_strand_unknown;
        bool strand_set = false;
        ITERATE(CSeq_loc_mix::Tdata, it, loc.GetMix().Get()) {
            if ((*it)->IsNull()  ||  (*it)->IsEmpty()) {
                continue;
            }
            ENa_strand istrand = GetStrand(**it);
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
    switch (loc.Which()) {
    case CSeq_loc::e_Int:
        if (loc.GetInt().IsSetStrand()) {
            return loc.GetInt().GetStrand();
        }
        break;

    case CSeq_loc::e_Whole:
        return eNa_strand_both;

    case CSeq_loc::e_Pnt:
        if (loc.GetPnt().IsSetStrand()) {
            return loc.GetPnt().GetStrand();
        }
        break;

    case CSeq_loc::e_Packed_pnt:
        return loc.GetPacked_pnt().IsSetStrand() ?
            loc.GetPacked_pnt().GetStrand() : eNa_strand_unknown;

    default:
        if (!IsOneBioseq(loc, scope)) {
            return eNa_strand_unknown;  // multiple bioseqs
        } else {
            return s_GetStrand(loc);
        }
    }

    /// default to unknown strand
    return eNa_strand_unknown;
}


TSeqPos GetStart(const CSeq_loc& loc, CScope* scope, ESeqLocExtremes ext)
{
    // Throw CObjmgrUtilException if loc does not represent one CBioseq
    GetId(loc, scope);

    return loc.GetStart(ext);
}


TSeqPos GetStop(const CSeq_loc& loc, CScope* scope, ESeqLocExtremes ext)
{
    // Throw CObjmgrUtilException if loc does not represent one CBioseq
    GetId(loc, scope);

    if (loc.IsWhole()  &&  scope != NULL) {
        CBioseq_Handle seq = GetBioseqFromSeqLoc(loc, *scope);
        if (seq) {
            return seq.GetBioseqLength() - 1;
        }
    }
    return loc.GetStop(ext);
}


void ChangeSeqId(CSeq_id* id, bool best, CScope* scope)
{
    // Return if no scope
    if (!scope  ||  !id) {
        return;
    }

    // Get CBioseq represented by *id
    CBioseq_Handle::TBioseqCore seq =
        scope->GetBioseqHandle(*id).GetBioseqCore();

    // Get pointer to the best/worst id of *seq
    const CSeq_id* tmp_id;
    if (best) {
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
(const CBioseq_Handle& bsh,
 const CSeq_loc& loc)
{
    try {
        CSeq_loc_Mapper mapper (bsh, CSeq_loc_Mapper::eSeqMap_Up);
        CConstRef<CSeq_loc> mapped_loc = mapper.Map(loc);
        if (!mapped_loc) {
            return false;
        }
        
        // Check that loc segments are in order
        CSeq_loc::TRange last_range;
        bool first = true;
        for (CSeq_loc_CI lit(*mapped_loc); lit; ++lit) {
            if (first) {
                last_range = lit.GetRange();
                first = false;
                continue;
            }
            if (lit.GetStrand() == eNa_strand_minus) {
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
    } catch (CException) {
        // exception will be thrown if references far sequence and not remote fetching
    }
    return false;
}


bool BadSeqLocSortOrder
(const CBioseq&  seq,
 const CSeq_loc& loc,
 CScope*         scope)
{
    if (scope) {
        return BadSeqLocSortOrder (scope->GetBioseqHandle(seq), loc);
    } else {
        return false;
    }
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


void SeqIntPartialCheck(const CSeq_interval& itv,
                        unsigned int& retval,
                        bool is_first,
                        bool is_last,
                        CScope& scope)
{
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
                    if ( is_last ) {
                        retval |= eSeqlocPartial_Stop;
                    } else {
                        retval |= eSeqlocPartial_Internal;
                    }
                    if (itv.GetFrom() != 0) {
                        if ( is_last ) {
                            retval |= eSeqlocPartial_Nostop;
                        } else {
                            retval |= eSeqlocPartial_Nointernal;
                        }
                    }
                } else {
                    if ( is_first ) {
                        retval |= eSeqlocPartial_Start;
                    } else {
                        retval |= eSeqlocPartial_Internal;
                    }
                    if (itv.GetFrom() != 0) {
                        if ( is_first ) {
                            retval |= eSeqlocPartial_Nostart;
                        } else {
                            retval |= eSeqlocPartial_Nointernal;
                        }
                    }
                }
            }
        } else if (fuzz.Which() == CInt_fuzz::e_Range) {
            // range
            if (itv.IsSetStrand()  &&   itv.GetStrand() == eNa_strand_minus) {
                if (is_last) {
                    retval |= eSeqlocPartial_Stop;
                } 
            } else {
                if (is_first) {
                    retval |= eSeqlocPartial_Start;
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
                scope.GetBioseqHandle(itv.GetId());
            bool miss_end = false;
            if ( hnd ) {                            
                if (itv.GetTo() != hnd.GetBioseqLength() - 1) {
                    miss_end = true;
                }
            }
            if (itv.IsSetStrand()  &&
                itv.GetStrand() == eNa_strand_minus) {
                if ( is_first ) {
                    retval |= eSeqlocPartial_Start;
                } else {
                    retval |= eSeqlocPartial_Internal;
                }
                if (miss_end) {
                    if ( is_first /* was last */) {
                        retval |= eSeqlocPartial_Nostart;
                    } else {
                        retval |= eSeqlocPartial_Nointernal;
                    }
                }
            } else {
                if ( is_last ) {
                    retval |= eSeqlocPartial_Stop;
                } else {
                    retval |= eSeqlocPartial_Internal;
                }
                if ( miss_end ) {
                    if ( is_last ) {
                        retval |= eSeqlocPartial_Nostop;
                    } else {
                        retval |= eSeqlocPartial_Nointernal;
                    }
                }
            }
        }
    }
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
            first = &(loc_iter.GetEmbeddingSeq_loc());
        }
        last = &(loc_iter.GetEmbeddingSeq_loc());
    }
    if (!first) {
        return retval;
    }

    CSeq_loc_CI i2(loc, CSeq_loc_CI::eEmpty_Allow);
    for ( ; i2; ++i2 ) {
        const CSeq_loc* slp = &(i2.GetEmbeddingSeq_loc());
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
                SeqIntPartialCheck(slp->GetInt(), retval,
                    slp == first, slp == last, *scope);
            }
            break;
        case CSeq_loc::e_Packed_int:
            {
                const CPacked_seqint::Tdata& ints = slp->GetPacked_int().Get();
                const CSeq_interval* first_int =
                    ints.empty() ? 0 : ints.front().GetPointer();
                const CSeq_interval* last_int =
                    ints.empty() ? 0 : ints.back().GetPointer();
                ITERATE(CPacked_seqint::Tdata, it, ints) {
                    SeqIntPartialCheck(**it, retval,
                        slp == first  &&  *it == first_int,
                        slp == last  &&  *it == last_int,
                        *scope);
                    ++i2;
                }
                break;
            }
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
        if ( !i2 ) {
            break;
        }
    }
    return retval;
}


/////////////////////////////////////////////////////////////////////
//
//  Implementation of SeqLocRevCmp()
//

static CSeq_interval* s_SeqIntRevCmp(const CSeq_interval& i,
                                     CScope* /* scope */)
{
    CRef<CSeq_interval> rev_int(new CSeq_interval);
    rev_int->Assign(i);
    
    ENa_strand s = i.CanGetStrand() ? i.GetStrand() : eNa_strand_unknown;
    rev_int->SetStrand(Reverse(s));

    return rev_int.Release();
}


static CSeq_point* s_SeqPntRevCmp(const CSeq_point& pnt,
                                  CScope* /* scope */)
{
    CRef<CSeq_point> rev_pnt(new CSeq_point);
    rev_pnt->Assign(pnt);
    
    ENa_strand s = pnt.CanGetStrand() ? pnt.GetStrand() : eNa_strand_unknown;
    rev_pnt->SetStrand(Reverse(s));

    return rev_pnt.Release();
}


CSeq_loc* SeqLocRevCmp(const CSeq_loc& loc, CScope* scope)
{
    CRef<CSeq_loc> rev_loc(new CSeq_loc);

    switch ( loc.Which() ) {

    // -- reverse is the same.
    case CSeq_loc::e_Null:
    case CSeq_loc::e_Empty:
    case CSeq_loc::e_Whole:
        rev_loc->Assign(loc);
        break;

    // -- just reverse the strand
    case CSeq_loc::e_Int:
        rev_loc->SetInt(*s_SeqIntRevCmp(loc.GetInt(), scope));
        break;
    case CSeq_loc::e_Pnt:
        rev_loc->SetPnt(*s_SeqPntRevCmp(loc.GetPnt(), scope));
        break;
    case CSeq_loc::e_Packed_pnt:
        rev_loc->SetPacked_pnt().Assign(loc.GetPacked_pnt());
        rev_loc->SetPacked_pnt().SetStrand(Reverse(GetStrand(loc, scope)));
        break;

    // -- possibly more than one sequence
    case CSeq_loc::e_Packed_int:
    {
        // reverse each interval and store them in reverse order
        typedef CRef< CSeq_interval > TInt;
        CPacked_seqint& pint = rev_loc->SetPacked_int();
        ITERATE (CPacked_seqint::Tdata, it, loc.GetPacked_int().Get()) {
            pint.Set().push_front(TInt(s_SeqIntRevCmp(**it, scope)));
        }
        break;
    }
    case CSeq_loc::e_Mix:
    {
        // reverse each location and store them in reverse order
        typedef CRef< CSeq_loc > TLoc;
        CSeq_loc_mix& mix = rev_loc->SetMix();
        ITERATE (CSeq_loc_mix::Tdata, it, loc.GetMix().Get()) {
            mix.Set().push_front(TLoc(SeqLocRevCmp(**it, scope)));
        }
        break;
    }
    case CSeq_loc::e_Equiv:
    {
        // reverse each location (no need to reverse order)
        typedef CRef< CSeq_loc > TLoc;
        CSeq_loc_equiv& e = rev_loc->SetEquiv();
        ITERATE (CSeq_loc_equiv::Tdata, it, loc.GetEquiv().Get()) {
            e.Set().push_back(TLoc(SeqLocRevCmp(**it, scope)));
        }
        break;
    }

    case CSeq_loc::e_Bond:
    {
        CSeq_bond& bond = rev_loc->SetBond();
        bond.SetA(*s_SeqPntRevCmp(loc.GetBond().GetA(), scope));
        if ( loc.GetBond().CanGetB() ) {
            bond.SetA(*s_SeqPntRevCmp(loc.GetBond().GetB(), scope));
        }
    }
        
    // -- not supported
    case CSeq_loc::e_Feat:
    default:
        NCBI_THROW(CException, eUnknown,
            "CSeq_loc::SeqLocRevCmp -- unsupported location type");
    }

    return rev_loc.Release();
}


/////////////////////////////////////////////////////////////////////
//
//  Implementation of Compare()
//


typedef pair<TSeqPos, TSeqPos> TRangeInfo;
typedef list<TRangeInfo> TRangeInfoList;
typedef map<CSeq_id_Handle, TRangeInfoList> TRangeInfoMap;
typedef map<CSeq_id_Handle, CSeq_id_Handle> TSynMap;


// Convert the seq-loc to TRangeInfos. The id map is used to
// normalize ids.
void s_SeqLocToRangeInfoMap(const CSeq_loc& loc,
                            TRangeInfoMap&  infos,
                            TSynMap&        syns,
                            CScope*         scope)
{
    CSeq_loc_CI it(loc,
        CSeq_loc_CI::eEmpty_Skip, CSeq_loc_CI::eOrder_Positional);
    for ( ; it; ++it) {
        TRangeInfo info;
        if ( it.IsWhole() ) {
            info.first = 0;
            info.second = GetLength(it.GetSeq_id(), scope);
        }
        else {
            info.first = it.GetRange().GetFrom();
            info.second = it.GetRange().GetToOpen();
        }
        CSeq_id_Handle id = it.GetSeq_id_Handle();
        TSynMap::const_iterator syn_it = syns.find(id);
        if (syn_it != syns.end()) {
            // known id
            id = syn_it->second;
        }
        else {
            // Unknown id, try to find a match
            bool found_id = false;
            ITERATE(TSynMap, sit, syns) {
                if ( IsSameBioseq(sit->first, id, scope) ) {
                    // Found a synonym
                    CSeq_id_Handle map_to = sit->second;
                    syns[id] = map_to;
                    id = map_to;
                    found_id = true;
                    break;
                }
            }
            if ( !found_id ) {
                // Store the new id
                syns[id] = id;
            }
        }
        infos[id].push_back(info);
    }
    NON_CONST_ITERATE(TRangeInfoMap, it, infos) {
        it->second.sort();
    }
}


ECompare Compare(const CSeq_loc& me,
                 const CSeq_loc& you,
                 CScope*         scope)
{
    TRangeInfoMap me_infos, you_infos;
    TSynMap syns;
    s_SeqLocToRangeInfoMap(me, me_infos, syns, scope);
    s_SeqLocToRangeInfoMap(you, you_infos, syns, scope);

    // Check if locs are equal. The ranges are sorted now, so the original
    // order is not important.
    if (me_infos.size() == you_infos.size()) {
        bool equal = true;
        TRangeInfoMap::const_iterator mid_it = me_infos.begin();
        TRangeInfoMap::const_iterator yid_it = you_infos.begin();
        for ( ; mid_it != me_infos.end(); ++mid_it, ++yid_it) {
            _ASSERT(yid_it != you_infos.end());
            if (mid_it->first != yid_it->first  ||
                mid_it->second.size() != yid_it->second.size()) {
                equal = false;
                break;
            }
            TRangeInfoList::const_iterator mit = mid_it->second.begin();
            TRangeInfoList::const_iterator yit = yid_it->second.begin();
            for ( ; mit != mid_it->second.end(); ++mit, ++yit) {
                _ASSERT(yit != yid_it->second.end());
                if (*mit != *yit) {
                    equal = false;
                    break;
                }
            }
            if ( !equal ) break;
        }
        if ( equal ) {
            return eSame;
        }
    }

    // Check if 'me' is contained or overlapping with 'you'.
    bool me_contained = true;
    bool overlap = false;

    ITERATE(TRangeInfoMap, mid_it, me_infos) {
        TRangeInfoMap::const_iterator yid_it = you_infos.find(mid_it->first);
        if (yid_it == you_infos.end()) {
            // The id is missing from 'you'.
            me_contained = false;
            if ( overlap ) {
                break; // nothing else to check
            }
            continue; // check for overlap
        }
        ITERATE(TRangeInfoList, mit, mid_it->second) {
            // current range is contained in 'you'?
            bool mit_contained = false;
            ITERATE(TRangeInfoList, yit, yid_it->second) {
                if (yit->second > mit->first  &&  yit->first < mit->second) {
                    overlap = true;
                    if (yit->first <= mit->first  &&  yit->second >= mit->second) {
                        mit_contained = true;
                        break;
                    }
                }
            }
            if ( !mit_contained ) {
                me_contained = false;
                if ( overlap ) break; // nothing else to check
            }
        }
        if (!me_contained  &&  overlap) {
            break; // nothing else to check
        }
    }

    // Reverse check: if 'you' is contained in 'me'.
    bool you_contained = true;

    ITERATE(TRangeInfoMap, yid_it, you_infos) {
        TRangeInfoMap::const_iterator mid_it = me_infos.find(yid_it->first);
        if (mid_it == me_infos.end()) {
            // The id is missing from 'me'.
            you_contained = false;
            break; // nothing else to check
        }
        ITERATE(TRangeInfoList, yit, yid_it->second) {
            // current range is contained in 'me'?
            bool yit_contained = false;
            ITERATE(TRangeInfoList, mit, mid_it->second) {
                if (mit->first <= yit->first  &&  mit->second >= yit->second) {
                    yit_contained = true;
                    break;
                }
            }
            if ( !yit_contained ) {
                you_contained = false;
                break;
            }
        }
        if ( !you_contained ) {
            break; // nothing else to check
        }
    }

    // Always prefere 'eContains' over 'eContained'.
    if ( you_contained ) {
        return eContains;
    }
    if ( me_contained ) {
        return eContained;
    }
    return overlap ? eOverlap : eNoOverlap;
}


/////////////////////////////////////////////////////////////////////
//
//  Implementation of TestForOverlap()
//

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
        if ( !TestForStrands(it1.GetStrand(), it2.GetStrand())  ||
             !it1.GetSeq_id().Equals(it2.GetSeq_id())) {
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


inline
Int8 AbsInt8(Int8 x)
{
    return x < 0 ? -x : x;
}


// Checks for eContained.
// May be used for eContains with reverse order of ranges.
inline
Int8 x_GetRangeDiff(const CHandleRange& hrg1,
                    const CHandleRange& hrg2,
                    CHandleRange::ETotalRangeFlags strand_flag)
{
    CRange<TSeqPos> rg1 =
        hrg1.GetOverlappingRange(strand_flag);
    CRange<TSeqPos> rg2 =
        hrg2.GetOverlappingRange(strand_flag);
    if ( rg2.Empty() ) {
        return rg1.GetLength();
    }
    if ( rg1.GetFrom() <= rg2.GetFrom()  &&
        rg1.GetTo() >= rg2.GetTo() ) {
        return Int8(rg2.GetFrom() - rg1.GetFrom()) +
            Int8(rg1.GetTo() - rg2.GetTo());
    }
    return -1;
}


Int8 x_TestForOverlap_MultiSeq(const CSeq_loc& loc1,
                              const CSeq_loc& loc2,
                              EOverlapType type)
{
    // Special case of TestForOverlap() - multi-sequences locations
    typedef CRange<Int8> TRange8;
    switch (type) {
    case eOverlap_Simple:
        {
            // Find all intersecting intervals
            Int8 diff = 0;
            for (CSeq_loc_CI li1(loc1); li1; ++li1) {
                for (CSeq_loc_CI li2(loc2); li2; ++li2) {
                    if ( !li1.GetSeq_id().Equals(li2.GetSeq_id()) ) {
                        continue;
                    }
                    // Compare strands
                    if ( IsReverse(li1.GetStrand()) !=
                        IsReverse(li2.GetStrand()) ) {
                        continue;
                    }
                    CRange<TSeqPos> rg1 = li1.GetRange();
                    CRange<TSeqPos> rg2 = li2.GetRange();
                    if ( rg1.GetTo() >= rg2.GetFrom()  &&
                         rg1.GetFrom() <= rg2.GetTo() ) {
                        diff += AbsInt8(Int8(rg2.GetFrom()) - Int8(rg1.GetFrom())) +
                            AbsInt8(Int8(rg1.GetTo()) - Int8(rg2.GetTo()));
                    }
                }
            }
            return diff ? diff : -1;
        }
    case eOverlap_Contained:
        {
            // loc2 is contained in loc1
            CHandleRangeMap rm1, rm2;
            rm1.AddLocation(loc1);
            rm2.AddLocation(loc2);
            Int8 diff = 0;
            CHandleRangeMap::const_iterator it2 = rm2.begin();
            for ( ; it2 != rm2.end(); ++it2) {
                CHandleRangeMap::const_iterator it1 =
                    rm1.GetMap().find(it2->first);
                if (it1 == rm1.end()) {
                    // loc2 has region on a sequence not present in loc1
                    return -1;
                }
                Int8 diff_plus = x_GetRangeDiff(it1->second,
                                                it2->second,
                                                CHandleRange::eStrandPlus);
                if (diff_plus < 0) {
                    return -1;
                }
                diff += diff_plus;
                Int8 diff_minus = x_GetRangeDiff(it1->second,
                                                 it2->second,
                                                 CHandleRange::eStrandMinus);
                if (diff_minus < 0) {
                    // It is possible that we are comparing location on
                    // plus strand to both/unknown strand. In this case
                    // the negative diff_minus should be ignored.
                    CRange<TSeqPos> rg1_minus =
                        it1->second.GetOverlappingRange(CHandleRange::eStrandMinus);
                    CRange<TSeqPos> rg2_plus =
                        it2->second.GetOverlappingRange(CHandleRange::eStrandPlus);
                    CRange<TSeqPos> rg2_minus =
                        it2->second.GetOverlappingRange(CHandleRange::eStrandMinus);
                    if (rg1_minus.Empty()  &&  rg2_plus == rg2_minus) {
                        // use diff_plus instead of diff_minus
                        diff_minus = diff_plus;
                    }
                    else {
                        return -1;
                    }
                }
                diff += diff_minus;
            }
            return diff;
        }
    case eOverlap_Contains:
        {
            // loc2 contains loc1
            CHandleRangeMap rm1, rm2;
            rm1.AddLocation(loc1);
            rm2.AddLocation(loc2);
            Int8 diff = 0;
            CHandleRangeMap::const_iterator it1 = rm1.begin();
            for ( ; it1 != rm1.end(); ++it1) {
                CHandleRangeMap::const_iterator it2 =
                    rm2.GetMap().find(it1->first);
                if (it2 == rm2.end()) {
                    // loc1 has region on a sequence not present in loc2
                    return -1;
                }
                Int8 diff_plus = x_GetRangeDiff(it2->second,
                                                it1->second,
                                                CHandleRange::eStrandPlus);
                if (diff_plus < 0) {
                    return -1;
                }
                diff += diff_plus;
                Int8 diff_minus = x_GetRangeDiff(it2->second,
                                                 it1->second,
                                                 CHandleRange::eStrandMinus);
                if (diff_minus < 0) {
                    return -1;
                }
                diff += diff_minus;
            }
            return diff;
        }
    case eOverlap_Subset:
    case eOverlap_SubsetRev:
    case eOverlap_CheckIntervals:
    case eOverlap_CheckIntRev:
    case eOverlap_Interval:
        {
            // For this types the function should not be called
            NCBI_THROW(CObjmgrUtilException, eNotImplemented,
                "TestForOverlap() -- error processing multi-ID seq-loc");
        }
    }
    return -1;
}


Int8 x_TestForOverlap_MultiStrand(const CSeq_loc& loc1,
                                  const CSeq_loc& loc2,
                                  EOverlapType type,
                                  CScope* scope)
{
    switch (type) {
    case eOverlap_Interval:
        {
            if (Compare(loc1, loc2, scope) == eNoOverlap) {
                return -1;
            }
            // Proceed to eSimple case to calculate diff by strand
        }
    case eOverlap_Simple:
        {
            CHandleRangeMap hrm1;
            CHandleRangeMap hrm2;
            // Each range map should have single ID
            hrm1.AddLocation(loc1);
            _ASSERT(hrm1.GetMap().size() == 1);
            hrm2.AddLocation(loc2);
            _ASSERT(hrm2.GetMap().size() == 1);
            if (hrm1.begin()->first != hrm2.begin()->first) {
                // Different IDs
                return -1;
            }
            Int8 diff_plus = -1;
            Int8 diff_minus = -1;
            {{
                // Plus strand
                CRange<TSeqPos> rg1 = hrm1.begin()->second.
                    GetOverlappingRange(CHandleRange::eStrandPlus);
                CRange<TSeqPos> rg2 = hrm2.begin()->second.
                    GetOverlappingRange(CHandleRange::eStrandPlus);
                if ( rg1.GetTo() >= rg2.GetFrom()  &&
                    rg1.GetFrom() <= rg2.GetTo() ) {
                    diff_plus = AbsInt8(rg2.GetFrom() - rg1.GetFrom()) +
                        AbsInt8(rg1.GetTo() - rg2.GetTo());
                }
            }}
            {{
                // Minus strand
                CRange<TSeqPos> rg1 = hrm1.begin()->second.
                    GetOverlappingRange(CHandleRange::eStrandMinus);
                CRange<TSeqPos> rg2 = hrm2.begin()->second.
                    GetOverlappingRange(CHandleRange::eStrandMinus);
                if ( rg1.GetTo() >= rg2.GetFrom()  &&
                    rg1.GetFrom() <= rg2.GetTo() ) {
                    diff_minus = AbsInt8(rg2.GetFrom() - rg1.GetFrom()) +
                        AbsInt8(rg1.GetTo() - rg2.GetTo());
                }
            }}
            if (diff_plus < 0  &&  diff_minus < 0) {
                return -1;
            }
            return (diff_plus < 0 ? 0 : diff_plus) +
                (diff_minus < 0 ? 0 : diff_minus);
        }
    case eOverlap_Contained:
    case eOverlap_Contains:
        {
            return x_TestForOverlap_MultiSeq(loc1, loc2, type);
        }
    case eOverlap_CheckIntervals:
    case eOverlap_CheckIntRev:
    default:
        {
            // For this types the function should not be called
            NCBI_THROW(CObjmgrUtilException, eNotImplemented,
                "TestForOverlap() -- error processing multi-strand seq-loc");
        }
    }
    return -1;
}


int TestForOverlap(const CSeq_loc& loc1,
                   const CSeq_loc& loc2,
                   EOverlapType type,
                   TSeqPos circular_len,
                   CScope* scope)
{
    Int8 ret = TestForOverlap64(loc1, loc2, type, circular_len, scope);
    return ret <= kMax_Int ? int(ret) : kMax_Int;
}


Int8 TestForOverlap64(const CSeq_loc& loc1,
                      const CSeq_loc& loc2,
                      EOverlapType type,
                      TSeqPos circular_len,
                      CScope* scope)
{
    const CSeq_loc* ploc1 = &loc1;
    const CSeq_loc* ploc2 = &loc2;
    typedef CRange<Int8> TRange8;
    CRange<TSeqPos> int_rg1, int_rg2;
    TRange8 rg1, rg2;
    bool multi_seq = false;
    try {
        int_rg1 = ploc1->GetTotalRange();
        int_rg2 = ploc2->GetTotalRange();
    }
    catch (exception&) {
        // Can not use total range for multi-sequence locations
        if (type == eOverlap_Simple  ||
            type == eOverlap_Contained  ||
            type == eOverlap_Contains) {
            // Can not process circular multi-id locations
            if (circular_len != 0  &&  circular_len != kInvalidSeqPos) {
                throw;
            }
            return x_TestForOverlap_MultiSeq(*ploc1, *ploc2, type);
        }
        multi_seq = true;
    }
    if ( scope && ploc1->IsWhole() ) {
        CBioseq_Handle h1 = scope->GetBioseqHandle(ploc1->GetWhole());
        if ( h1 ) {
            int_rg1.Set(0, h1.GetBioseqLength() - 1);
        }
    }
    if ( scope && ploc2->IsWhole() ) {
        CBioseq_Handle h2 = scope->GetBioseqHandle(ploc2->GetWhole());
        if ( h2 ) {
            int_rg2.Set(0, h2.GetBioseqLength() - 1);
        }
    }
    rg1.Set(int_rg1.GetFrom(), int_rg1.GetTo());
    rg2.Set(int_rg2.GetFrom(), int_rg2.GetTo());

    ENa_strand strand1 = GetStrand(*ploc1);
    ENa_strand strand2 = GetStrand(*ploc2);
    if ( !TestForStrands(strand1, strand2) ) {
        // Subset and CheckIntervals don't use total ranges
        if (type != eOverlap_Subset  &&
            type != eOverlap_SubsetRev  &&
            type != eOverlap_CheckIntervals  &&
            type != eOverlap_CheckIntRev) {
            if ( strand1 == eNa_strand_other  ||
                strand2 == eNa_strand_other ) {
                return x_TestForOverlap_MultiStrand(*ploc1, *ploc2, type, scope);
            }
            return -1;
        }
        else {
            if ( strand1 != eNa_strand_other && strand2 != eNa_strand_other ) {
                // singular but incompatible strands
                return -1;
            }
            // there is a possibility of multiple strands that needs to be
            // checked too
        }
    }
    switch (type) {
    case eOverlap_Simple:
        {
            if (circular_len != kInvalidSeqPos) {
                Int8 from1 = ploc1->GetStart(eExtreme_Positional);
                Int8 from2 = ploc2->GetStart(eExtreme_Positional);
                Int8 to1 = ploc1->GetStop(eExtreme_Positional);
                Int8 to2 = ploc2->GetStop(eExtreme_Positional);
                if (from1 > to1) {
                    if (from2 > to2) {
                        // Both locations are circular and must intersect at 0
                        return AbsInt8(from2 - from1) + AbsInt8(to1 - to2);
                    }
                    else {
                        // Only the first location is circular, rg2 may be used
                        // for the second one.
                        Int8 loc_len =
                            rg2.GetLength() +
                            Int8(ploc1->GetCircularLength(circular_len));
                        if (from1 < rg2.GetFrom()  ||  to1 > rg2.GetTo()) {
                            // loc2 is completely in loc1
                            return loc_len - 2*rg2.GetLength();
                        }
                        if (from1 < rg2.GetTo()) {
                            return loc_len - (rg2.GetTo() - from1);
                        }
                        if (rg2.GetFrom() < to1) {
                            return loc_len - (to1 - rg2.GetFrom());
                        }
                        return -1;
                    }
                }
                else if (from2 > to2) {
                    // Only the second location is circular
                    Int8 loc_len =
                        rg1.GetLength() +
                        Int8(ploc2->GetCircularLength(circular_len));
                    if (from2 < rg1.GetFrom()  ||  to2 > rg1.GetTo()) {
                        // loc2 is completely in loc1
                        return loc_len - 2*rg1.GetLength();
                    }
                    if (from2 < rg1.GetTo()) {
                        return loc_len - (rg1.GetTo() - from2);
                    }
                    if (rg1.GetFrom() < to2) {
                        return loc_len - (to2 - rg1.GetFrom());
                    }
                    return -1;
                }
                // Locations are not circular, proceed to normal calculations
            }
            if ( rg1.GetTo() >= rg2.GetFrom()  &&
                rg1.GetFrom() <= rg2.GetTo() ) {
                return AbsInt8(rg2.GetFrom() - rg1.GetFrom()) +
                    AbsInt8(rg1.GetTo() - rg2.GetTo());
            }
            return -1;
        }
    case eOverlap_Contained:
        {
            if (circular_len != kInvalidSeqPos) {
                Int8 from1 = ploc1->GetStart(eExtreme_Positional);
                Int8 from2 = ploc2->GetStart(eExtreme_Positional);
                Int8 to1 = ploc1->GetStop(eExtreme_Positional);
                Int8 to2 = ploc2->GetStop(eExtreme_Positional);
                if (from1 > to1) {
                    if (from2 > to2) {
                        return (from1 <= from2  &&  to1 >= to2) ?
                            (from2 - from1) + (to1 - to2) : -1;
                    }
                    else {
                        if (rg2.GetFrom() >= from1  ||  rg2.GetTo() <= to1) {
                            return Int8(ploc1->GetCircularLength(circular_len)) -
                                rg2.GetLength();
                        }
                        return -1;
                    }
                }
                else if (from2 > to2) {
                    // Non-circular location can not contain a circular one
                    return -1;
                }
            }
            if ( rg1.GetFrom() <= rg2.GetFrom()  &&
                rg1.GetTo() >= rg2.GetTo() ) {
                return (rg2.GetFrom() - rg1.GetFrom()) +
                    (rg1.GetTo() - rg2.GetTo());
            }
            return -1;
        }
    case eOverlap_Contains:
        {
            if (circular_len != kInvalidSeqPos) {
                Int8 from1 = ploc1->GetStart(eExtreme_Positional);
                Int8 from2 = ploc2->GetStart(eExtreme_Positional);
                Int8 to1 = ploc1->GetStop(eExtreme_Positional);
                Int8 to2 = ploc2->GetStop(eExtreme_Positional);
                if (from1 > to1) {
                    if (from2 > to2) {
                        return (from2 <= from1  &&  to2 >= to1) ?
                            (from1 - from2) + (to2 - to1) : -1;
                    }
                    else {
                        // Non-circular location can not contain a circular one
                        return -1;
                    }
                }
                else if (from2 > to2) {
                    if (rg1.GetFrom() >= from2  ||  rg1.GetTo() <= to2) {
                        return Int8(ploc2->GetCircularLength(circular_len)) -
                            rg1.GetLength();
                    }
                    return -1;
                }
            }
            if ( rg2.GetFrom() <= rg1.GetFrom()  &&
                rg2.GetTo() >= rg1.GetTo()) {
                return (rg1.GetFrom() - rg2.GetFrom()) + 
                    (rg2.GetTo() - rg1.GetTo());
            }
            return -1;
        }
    case eOverlap_SubsetRev:
        swap(ploc1, ploc2);
        // continue to eOverlap_Subset case
    case eOverlap_Subset:
        {
            ECompare cmp = Compare(*ploc1, *ploc2, scope);
            if ( cmp == eSame ) {
                return 0;
            }
            if ( cmp != eContains ) {
                return -1;
            }
            return Int8(GetCoverage(*ploc2, scope)) -
                Int8(GetCoverage(*ploc1, scope));
        }
    case eOverlap_CheckIntRev:
        swap(ploc1, ploc2);
        // Go on to the next case
    case eOverlap_CheckIntervals:
        {
            if ( !multi_seq  &&
                (rg1.GetFrom() > rg2.GetTo()  || rg1.GetTo() < rg2.GetFrom()) ) {
                return -1;
            }
            // Check intervals' boundaries
            CSeq_loc_CI it1(*ploc1);
            CSeq_loc_CI it2(*ploc2);
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
                        return Int8(GetLength(*ploc1, scope)) -
                            Int8(GetLength(*ploc2, scope));
                    }
                }
            }
            else {
                TSeqPos loc2start = it2.GetRange().GetFrom();
                //TSeqPos loc2end = it2.GetRange().GetTo();
                // Find the first interval in loc1 intersecting with loc2
                for ( ; it1  /*&&  it1.GetRange().GetFrom() <= loc2end*/; ++it1) {
                    if (it1.GetSeq_id().Equals(it2.GetSeq_id())  &&
                        it1.GetRange().GetFrom() <= loc2start  &&
                        TestForIntervals(it1, it2, false)) {
                        return Int8(GetLength(*ploc1, scope)) -
                            Int8(GetLength(*ploc2, scope));
                    }
                }
            }
            return -1;
        }
    case eOverlap_Interval:
        {
            return (Compare(*ploc1, *ploc2, scope) == eNoOverlap) ? -1
                : AbsInt8(rg2.GetFrom() - rg1.GetFrom()) +
                AbsInt8(rg1.GetTo() - rg2.GetTo());
        }
    }
    return -1;
}


/////////////////////////////////////////////////////////////////////
//
//  Seq-loc operations
//

class CDefaultSynonymMapper : public ISynonymMapper
{
public:
    CDefaultSynonymMapper(CScope* scope);
    virtual ~CDefaultSynonymMapper(void);

    virtual CSeq_id_Handle GetBestSynonym(const CSeq_id& id);

private:
    typedef map<CSeq_id_Handle, CSeq_id_Handle> TSynonymMap;

    CRef<CSeq_id_Mapper> m_IdMapper;
    TSynonymMap          m_SynMap;
    CScope*              m_Scope;
};


CDefaultSynonymMapper::CDefaultSynonymMapper(CScope* scope)
    : m_IdMapper(CSeq_id_Mapper::GetInstance()),
      m_Scope(scope)
{
    return;
}


CDefaultSynonymMapper::~CDefaultSynonymMapper(void)
{
    return;
}


CSeq_id_Handle CDefaultSynonymMapper::GetBestSynonym(const CSeq_id& id)
{
    CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(id);
    if ( !m_Scope  ||  id.Which() == CSeq_id::e_not_set ) {
        return idh;
    }
    TSynonymMap::iterator id_syn = m_SynMap.find(idh);
    if (id_syn != m_SynMap.end()) {
        return id_syn->second;
    }
    CSeq_id_Handle best;
    int best_rank = kMax_Int;
    CConstRef<CSynonymsSet> syn_set = m_Scope->GetSynonyms(idh);
    ITERATE(CSynonymsSet, syn_it, *syn_set) {
        CSeq_id_Handle synh = syn_set->GetSeq_id_Handle(syn_it);
        int rank = synh.GetSeqId()->BestRankScore();
        if (rank < best_rank) {
            best = synh;
            best_rank = rank;
        }
    }
    if ( !best ) {
        // Synonyms set was empty
        m_SynMap[idh] = idh;
        return idh;
    }
    ITERATE(CSynonymsSet, syn_it, *syn_set) {
        m_SynMap[syn_set->GetSeq_id_Handle(syn_it)] = best;
    }
    return best;
}


class CDefaultLengthGetter : public ILengthGetter
{
public:
    CDefaultLengthGetter(CScope* scope);
    virtual ~CDefaultLengthGetter(void);

    virtual TSeqPos GetLength(const CSeq_id& id);

protected:
    CScope*              m_Scope;
};


CDefaultLengthGetter::CDefaultLengthGetter(CScope* scope)
    : m_Scope(scope)
{
    return;
}


CDefaultLengthGetter::~CDefaultLengthGetter(void)
{
    return;
}


TSeqPos CDefaultLengthGetter::GetLength(const CSeq_id& id)
{
    if (id.Which() == CSeq_id::e_not_set) {
        return 0;
    }
    CBioseq_Handle bh;
    if ( m_Scope ) {
        bh = m_Scope->GetBioseqHandle(id);
    }
    if ( !bh ) {
        NCBI_THROW(CException, eUnknown,
            "Can not get length of whole location");
    }
    return bh.GetBioseqLength();
}


CRef<CSeq_loc> Seq_loc_Merge(const CSeq_loc&    loc,
                             CSeq_loc::TOpFlags flags,
                             CScope*            scope)
{
    CDefaultSynonymMapper syn_mapper(scope);
    return loc.Merge(flags, &syn_mapper);
}


CRef<CSeq_loc> Seq_loc_Add(const CSeq_loc&    loc1,
                           const CSeq_loc&    loc2,
                           CSeq_loc::TOpFlags flags,
                           CScope*            scope)
{
    CDefaultSynonymMapper syn_mapper(scope);
    return loc1.Add(loc2, flags, &syn_mapper);
}


CRef<CSeq_loc> Seq_loc_Subtract(const CSeq_loc&    loc1,
                                const CSeq_loc&    loc2,
                                CSeq_loc::TOpFlags flags,
                                CScope*            scope)
{
    CDefaultSynonymMapper syn_mapper(scope);
    CDefaultLengthGetter len_getter(scope);
    return loc1.Subtract(loc2, flags, &syn_mapper, &len_getter);
}


END_SCOPE(sequence)
END_SCOPE(objects)
END_NCBI_SCOPE
