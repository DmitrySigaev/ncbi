/* $Id$
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
 * Author:   Author:  Cliff Clausen, Eugene Vasilchenko
 *
 * File Description:
 *   .......
 *
 * Remark:
 *   This code was originally generated by application DATATOOL
 *   using specifications from the ASN data definition file
 *   'seqloc.asn'.
 *
 * ===========================================================================
 */

#include <serial/enumvalues.hpp>
#include <objects/general/Int_fuzz.hpp>
#include <objects/seqloc/Seq_point.hpp>
#include <objects/seqloc/Seq_loc_equiv.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqfeat/Feat_id.hpp>

BEGIN_NCBI_SCOPE
BEGIN_objects_SCOPE // namespace ncbi::objects::


// destructor
CSeq_loc::~CSeq_loc(void)
{
}


inline
void x_Assign(CInt_fuzz& dst, const CInt_fuzz& src)
{
    switch ( src.Which() ) {
    case CInt_fuzz::e_not_set:
        dst.Reset();
        break;
    case CInt_fuzz::e_P_m:
        dst.SetP_m(src.GetP_m());
        break;
    case CInt_fuzz::e_Range:
        dst.SetRange().SetMin(src.GetRange().GetMin());
        dst.SetRange().SetMax(src.GetRange().GetMax());
        break;
    case CInt_fuzz::e_Pct:
        dst.SetPct(src.GetPct());
        break;
    case CInt_fuzz::e_Lim:
        dst.SetLim(src.GetLim());
        break;
    case CInt_fuzz::e_Alt:
        dst.SetAlt() = src.GetAlt();
        break;
    default:
        THROW1_TRACE(runtime_error, "Invalid Int-fuzz variant");
    }
}


inline
void x_Assign(CSeq_interval& dst, const CSeq_interval& src)
{
    dst.SetFrom(src.GetFrom());
    dst.SetTo(src.GetTo());
    if ( src.IsSetStrand() ) {
        dst.SetStrand(src.GetStrand());
    }
    else {
        dst.ResetStrand();
    }
    dst.SetId().Assign(src.GetId());
    if ( src.IsSetFuzz_from() ) {
        x_Assign(dst.SetFuzz_from(), src.GetFuzz_from());
    }
    else {
        dst.ResetFuzz_from();
    }
    if ( src.IsSetFuzz_to() ) {
        x_Assign(dst.SetFuzz_to(), src.GetFuzz_to());
    }
    else {
        dst.ResetFuzz_to();
    }
}


inline
void x_Assign(CSeq_point& dst, const CSeq_point& src)
{
    dst.SetPoint(src.GetPoint());
    if ( src.IsSetStrand() ) {
        dst.SetStrand(src.GetStrand());
    }
    else {
        dst.ResetStrand();
    }
    dst.SetId().Assign(src.GetId());
    if ( src.IsSetFuzz() ) {
        x_Assign(dst.SetFuzz(), src.GetFuzz());
    }
    else {
        dst.ResetFuzz();
    }
}


inline
void x_Assign(CPacked_seqint& dst, const CPacked_seqint& src)
{
    CPacked_seqint::Tdata& data = dst.Set();
    data.clear();
    ITERATE ( CPacked_seqint::Tdata, i, src.Get() ) {
        data.push_back(CRef<CSeq_interval>(new CSeq_interval));
        x_Assign(*data.back(), **i);
    }
}


inline
void x_Assign(CPacked_seqpnt& dst, const CPacked_seqpnt& src)
{
    if ( src.IsSetStrand() ) {
        dst.SetStrand(src.GetStrand());
    }
    else {
        dst.ResetStrand();
    }
    dst.SetId().Assign(src.GetId());
    if ( src.IsSetFuzz() ) {
        x_Assign(dst.SetFuzz(), src.GetFuzz());
    }
    else {
        dst.ResetFuzz();
    }
    dst.SetPoints() = src.GetPoints();
}


inline
void x_Assign(CSeq_bond& dst, const CSeq_bond& src)
{
    x_Assign(dst.SetA(), src.GetA());
    if ( src.IsSetB() ) {
        x_Assign(dst.SetB(), src.GetB());
    }
    else {
        dst.ResetB();
    }
}


inline
void x_Assign(CSeq_loc_mix& dst, const CSeq_loc_mix& src)
{
    CSeq_loc_mix::Tdata& data = dst.Set();
    data.clear();
    ITERATE ( CSeq_loc_mix::Tdata, i, src.Get() ) {
        data.push_back(CRef<CSeq_loc>(new CSeq_loc));
        data.back()->Assign(**i);
    }
}


inline
void x_Assign(CSeq_loc_equiv& dst, const CSeq_loc_equiv& src)
{
    CSeq_loc_equiv::Tdata& data = dst.Set();
    data.clear();
    ITERATE ( CSeq_loc_equiv::Tdata, i, src.Get() ) {
        data.push_back(CRef<CSeq_loc>(new CSeq_loc));
        data.back()->Assign(**i);
    }
}


void CSeq_loc::Assign(const CSerialObject& obj)
{
    if ( GetTypeInfo() == obj.GetThisTypeInfo() ) {
        const CSeq_loc& loc = static_cast<const CSeq_loc&>(obj);
        switch ( loc.Which() ) {
        case CSeq_loc::e_not_set:
            Reset();
            return;
        case CSeq_loc::e_Null:
            SetNull();
            return;
        case CSeq_loc::e_Empty:
            SetEmpty().Assign(loc.GetEmpty());
            return;
        case CSeq_loc::e_Whole:
            SetWhole().Assign(loc.GetWhole());
            return;
        case CSeq_loc::e_Int:
            x_Assign(SetInt(), loc.GetInt());
            return;
        case CSeq_loc::e_Pnt:
            x_Assign(SetPnt(), loc.GetPnt());
            return;
        case CSeq_loc::e_Packed_int:
            x_Assign(SetPacked_int(), loc.GetPacked_int());
            return;
        case CSeq_loc::e_Packed_pnt:
            x_Assign(SetPacked_pnt(), loc.GetPacked_pnt());
            return;
        case CSeq_loc::e_Mix:
            x_Assign(SetMix(), loc.GetMix());
            return;
        case CSeq_loc::e_Equiv:
            x_Assign(SetEquiv(), loc.GetEquiv());
            return;
        case CSeq_loc::e_Bond:
            x_Assign(SetBond(), loc.GetBond());
            return;
        case CSeq_loc::e_Feat:
            SetFeat().Assign(loc.GetFeat());
            return;
        }
    }
    CSerialObject::Assign(obj);
}


CSeq_loc::TRange CSeq_loc::x_UpdateTotalRange(void) const
{
    TRange range = m_TotalRangeCache;
    if ( range.GetFrom() == TSeqPos(kDirtyCache) ) {
        const CSeq_id* id = 0;
        range = m_TotalRangeCache = CalculateTotalRangeCheckId(id);
        m_IdCache = id;
    }
    return range;
}


void CSeq_loc::x_UpdateId(const CSeq_id*& total_id, const CSeq_id& id)
{
    if ( !total_id ) {
        total_id = &id;
    }
    else if ( !total_id->Equals(id) ) {
        THROW1_TRACE(runtime_error,
                     "CSeq_loc::CalculateTotalRange -- "
                     "can not combine multiple seq-ids");
    }
}


// returns enclosing location range
// the total range is meaningless if there are several seq-ids
// in the location
CSeq_loc::TRange CSeq_loc::CalculateTotalRangeCheckId(const CSeq_id*& id) const
{
    TRange total_range;
    switch ( Which() ) {
    case CSeq_loc::e_not_set:
    case CSeq_loc::e_Null:
    case CSeq_loc::e_Empty:
        {
            // Ignore empty locations
            total_range = TRange::GetEmpty();
            break;
        }
    case CSeq_loc::e_Whole:
        {
            x_UpdateId(id, GetWhole());
            total_range = TRange::GetWhole();
            break;
        }
    case CSeq_loc::e_Int:
        {
            const CSeq_interval& loc = GetInt();
            x_UpdateId(id, loc.GetId());
            total_range.Set(loc.GetFrom(), loc.GetTo());
            break;
        }
    case CSeq_loc::e_Pnt:
        {
            const CSeq_point& pnt = GetPnt();
            x_UpdateId(id, pnt.GetId());
            TSeqPos pos = pnt.GetPoint();
            total_range.Set(pos, pos);
            break;
        }
    case CSeq_loc::e_Packed_int:
        {
            // Check ID of each interval
            const CPacked_seqint& ints = GetPacked_int();
            total_range = TRange::GetEmpty();
            ITERATE ( CPacked_seqint::Tdata, ii, ints.Get() ) {
                const CSeq_interval& loc = **ii;
                x_UpdateId(id, loc.GetId());
                total_range += TRange(loc.GetFrom(), loc.GetTo());
            }
            break;
        }
    case CSeq_loc::e_Packed_pnt:
        {
            const CPacked_seqpnt& pnts = GetPacked_pnt();
            x_UpdateId(id, pnts.GetId());
            total_range = TRange::GetEmpty();
            ITERATE( CPacked_seqpnt::TPoints, pi, pnts.GetPoints() ) {
                TSeqPos pos = *pi;
                total_range += TRange(pos, pos);
            }
            break;
        }
    case CSeq_loc::e_Mix:
        {
            // Check ID of each sub-location.
            const CSeq_loc_mix& mix = GetMix();
            total_range = TRange::GetEmpty();
            ITERATE( CSeq_loc_mix::Tdata, li, mix.Get() ) {
                // instead of Get... to be able to check ID
                total_range += (*li)->GetTotalRangeCheckId(id);
            }
            break;
        }
    case CSeq_loc::e_Equiv:
/*
        {
            // Does it make any sense to GetTotalRange() from an equiv?
            total_range = TRange::GetEmpty();
            ITERATE(CSeq_loc_equiv::Tdata, li, GetEquiv().Get()) {
                total_range += (*li)->GetTotalRange();
            }
            break;
        }
*/
    case CSeq_loc::e_Bond:
/*
        {
            // Does it make any sense to GetTotalRange() from a bond?
            const CSeq_bond& loc = GetBond();
            TSeqPos pos = loc.GetA().GetPoint();
            total_range = TRange(pos, pos);
            if ( loc.IsSetB() ) {
                pos = loc.GetB().GetPoint();
                total_range += TRange(pos, pos);
            }
            break;
        }
*/
    case CSeq_loc::e_Feat:
    default:
        {
            THROW1_TRACE(runtime_error,
                         "CSeq_loc::CalculateTotalRange -- "
                         "unsupported location type");
        }
    }

    return total_range;
}


// CSeq_loc_CI implementation
CSeq_loc_CI::CSeq_loc_CI(void)
    : m_Location(0),
      m_EmptyFlag(eEmpty_Skip)
{
    m_CurLoc = m_LocList.end();
}


CSeq_loc_CI::CSeq_loc_CI(const CSeq_loc& loc, EEmptyFlag empty_flag)
    : m_Location(&loc),
      m_EmptyFlag(empty_flag)
{
    x_ProcessLocation(loc);
    m_CurLoc = m_LocList.begin();
}


CSeq_loc_CI::CSeq_loc_CI(const CSeq_loc_CI& iter)
{
    *this = iter;
}


CSeq_loc_CI::~CSeq_loc_CI()
{
}


CSeq_loc_CI& CSeq_loc_CI::operator= (const CSeq_loc_CI& iter)
{
    if (this == &iter)
        return *this;
    m_LocList.clear();
    m_Location = iter.m_Location;
    m_EmptyFlag = iter.m_EmptyFlag;
    ITERATE(TLocList, li, iter.m_LocList) {
        TLocList::iterator tmp = m_LocList.insert(m_LocList.end(), *li);
        if (iter.m_CurLoc == li)
            m_CurLoc = tmp;
    }
    return *this;
}


void CSeq_loc_CI::x_ThrowNotValid(const char* where) const
{
    string msg;
    msg += "CSeq_loc_CI::";
    msg += where;
    msg += " -- iterator is not valid";
    THROW1_TRACE(runtime_error, msg);
}

void CSeq_loc_CI::x_ProcessLocation(const CSeq_loc& loc)
{
    switch ( loc.Which() ) {
    case CSeq_loc::e_not_set:
    case CSeq_loc::e_Null:
    case CSeq_loc::e_Empty:
        {
            if (m_EmptyFlag == eEmpty_Allow) {
                SLoc_Info info;
                info.m_Id.Reset(new CSeq_id);
                info.m_Range = TRange::GetEmpty();
                info.m_Loc = &loc;
                m_LocList.push_back(info);
            }
            return;
        }
    case CSeq_loc::e_Whole:
        {
            SLoc_Info info;
            info.m_Id = &loc.GetWhole();
            info.m_Range = TRange::GetWhole();
            info.m_Loc = &loc;
            m_LocList.push_back(info);
            return;
        }
    case CSeq_loc::e_Int:
        {
            SLoc_Info info;
            info.m_Id = &loc.GetInt().GetId();
            info.m_Range.Set(loc.GetInt().GetFrom(), loc.GetInt().GetTo());
            if ( loc.GetInt().IsSetStrand() )
                info.m_Strand = loc.GetInt().GetStrand();
            info.m_Loc = &loc;
            m_LocList.push_back(info);
            return;
        }
    case CSeq_loc::e_Pnt:
        {
            SLoc_Info info;
            info.m_Id = &loc.GetPnt().GetId();
            info.m_Range.Set(loc.GetPnt().GetPoint(), loc.GetPnt().GetPoint());
            if ( loc.GetPnt().IsSetStrand() )
                info.m_Strand = loc.GetPnt().GetStrand();
            info.m_Loc = &loc;
            m_LocList.push_back(info);
            return;
        }
    case CSeq_loc::e_Packed_int:
        {
            ITERATE ( CPacked_seqint::Tdata, ii, loc.GetPacked_int().Get() ) {
                SLoc_Info info;
                info.m_Id = &(*ii)->GetId();
                info.m_Range.Set((*ii)->GetFrom(), (*ii)->GetTo());
                if ( (*ii)->IsSetStrand() )
                    info.m_Strand = (*ii)->GetStrand();
                info.m_Loc = &loc;
                m_LocList.push_back(info);
            }
            return;
        }
    case CSeq_loc::e_Packed_pnt:
        {
            ITERATE ( CPacked_seqpnt::TPoints, pi, loc.GetPacked_pnt().GetPoints() ) {
                SLoc_Info info;
                info.m_Id = &loc.GetPacked_pnt().GetId();
                info.m_Range.Set(*pi, *pi);
                if ( loc.GetPacked_pnt().IsSetStrand() )
                    info.m_Strand = loc.GetPacked_pnt().GetStrand();
                info.m_Loc = &loc;
                m_LocList.push_back(info);
            }
            return;
        }
    case CSeq_loc::e_Mix:
        {
            ITERATE(CSeq_loc_mix::Tdata, li, loc.GetMix().Get()) {
                x_ProcessLocation(**li);
            }
            return;
        }
    case CSeq_loc::e_Equiv:
        {
            ITERATE(CSeq_loc_equiv::Tdata, li, loc.GetEquiv().Get()) {
                x_ProcessLocation(**li);
            }
            return;
        }
    case CSeq_loc::e_Bond:
        {
            SLoc_Info infoA;
            infoA.m_Id = &loc.GetBond().GetA().GetId();
            infoA.m_Range.Set(loc.GetBond().GetA().GetPoint(),
                              loc.GetBond().GetA().GetPoint());
            if ( loc.GetBond().GetA().IsSetStrand() )
                infoA.m_Strand = loc.GetBond().GetA().GetStrand();
            infoA.m_Loc = &loc;
            m_LocList.push_back(infoA);
            if ( loc.GetBond().IsSetB() ) {
                SLoc_Info infoB;
                infoB.m_Id = &loc.GetBond().GetB().GetId();
                infoB.m_Range.Set(loc.GetBond().GetB().GetPoint(),
                                  loc.GetBond().GetB().GetPoint());
                if ( loc.GetBond().GetB().IsSetStrand() )
                    infoB.m_Strand = loc.GetBond().GetB().GetStrand();
                infoB.m_Loc = &loc;
                m_LocList.push_back(infoB);
            }
            return;
        }
    case CSeq_loc::e_Feat:
    default:
        {
            THROW1_TRACE(runtime_error,
                         "CSeq_loc_CI -- unsupported location type");
        }
    }
}


// Append a string representation of a CSeq_id to label
inline
void s_GetLabel(const CSeq_id& id, string* label)
{
    CNcbiOstrstream os;
    id.WriteAsFasta(os);
    *label += CNcbiOstrstreamToString(os);
}


// Append to label info for a CSeq_point
inline
const CSeq_id* s_GetLabel
(const CSeq_point& pnt,
 const CSeq_id*    last_id,
 string*           label)
{
    // If CSeq_id does not match last_id, then append id to label
    if ( !last_id  ||  !last_id->Match(pnt.GetId()) ) {
        s_GetLabel(pnt.GetId(), label);
        *label += ":";
    }

    // Add strand info to label
    *label += GetTypeInfo_enum_ENa_strand()
        ->FindName(pnt.GetStrand(), true);

    if (pnt.IsSetFuzz()) {
        // Append Fuzz info to label
        pnt.GetFuzz().GetLabel(label, pnt.GetPoint());
    } else {
        // Append 1 based point to label
        *label += NStr::IntToString(pnt.GetPoint()+1);
    }

    // update last_id
    last_id = &pnt.GetId();
    return last_id;
}


// Append to label info for CSeq_interval
inline
const CSeq_id* s_GetLabel
(const CSeq_interval& itval,
 const CSeq_id*       last_id,
 string*              label)
{
    if (!last_id || !last_id->Match(itval.GetId())) {
        s_GetLabel(itval.GetId(), label);
        *label += ":";
    }
    last_id = &itval.GetId();
    if (itval.IsSetStrand()) {
        *label += GetTypeInfo_enum_ENa_strand()
            ->FindName(itval.GetStrand(), true);
    }
    if (itval.GetStrand() == eNa_strand_minus ||
        itval.GetStrand() == eNa_strand_both_rev) {
        if (itval.IsSetFuzz_to()) {
            itval.GetFuzz_to().GetLabel(label, itval.GetTo(), false);
        } else {
            *label += NStr::IntToString(itval.GetTo()+1);
        }
        *label += "-";
        if (itval.IsSetFuzz_from()) {
            itval.GetFuzz_from().GetLabel(label, itval.GetFrom());
        } else {
            *label += NStr::IntToString(itval.GetFrom()+1);
        }
    } else {
        if (itval.IsSetFuzz_from()) {
            itval.GetFuzz_from().GetLabel
                (label, itval.GetFrom(), false);
        } else {
            *label += NStr::IntToString(itval.GetFrom()+1);
        }
        *label += "-";
        if (itval.IsSetFuzz_to()) {
            itval.GetFuzz_to().GetLabel(label, itval.GetTo());
        } else {
            *label += NStr::IntToString(itval.GetTo()+1);
        }
    }
    return last_id;
}


// Forward declaration
const CSeq_id* s_GetLabel
(const CSeq_loc& loc,
 const CSeq_id*  last_id,
 string*         label,
 bool            first = false);


// Appends to label info for each CSeq_loc in a list of CSeq_locs
inline
const CSeq_id* s_GetLabel
(const list<CRef<CSeq_loc> >&  loc_list,
 const CSeq_id*                last_id,
 string*                       label)
{
    bool first = true;
    ITERATE (list<CRef<CSeq_loc> >, it, loc_list) {

        // Append to label for each CSeq_loc in list
        last_id = s_GetLabel(**it, last_id, label, first);
        first = false;
    }

    return last_id;
}


// Builds a label based upon a CSeq_loc and all embedded CSeq_locs
const CSeq_id* s_GetLabel
(const CSeq_loc& loc,
 const CSeq_id*  last_id,
 string*         label,
 bool            first)
{
    // Ensure label is not null
    if (!label) {
        return last_id;
    }

    // Put a comma separator if necessary
    if (!first) {
        *label += ", ";
    }

    // Loop through embedded CSeq_locs and create a label, depending on
    // type of CSeq_loc
    switch (loc.Which()) {
    case CSeq_loc::e_Null:
        *label += "~";
        break;
    case CSeq_loc::e_Empty:
        *label += "{";
        s_GetLabel(loc.GetEmpty(), label);
        last_id = &loc.GetEmpty();
        *label += "}";
        break;
    case CSeq_loc::e_Whole:
        s_GetLabel(loc.GetWhole(), label);
        last_id = &loc.GetWhole();
        break;
    case CSeq_loc::e_Int:
        last_id = s_GetLabel(loc.GetInt(), last_id, label);
        break;
    case CSeq_loc::e_Packed_int:
    {
        *label += "(";
        bool first = true;
        ITERATE(list<CRef<CSeq_interval> >, it, loc.GetPacked_int().Get()) {
            if (!first) {
                *label += ", ";
            }
            first = false;
            last_id = s_GetLabel(**it, last_id, label);
        }
        *label += ")";
        break;
    }
    case CSeq_loc::e_Pnt:
        last_id = s_GetLabel(loc.GetPnt(), last_id, label);
        break;
    case CSeq_loc::e_Packed_pnt:
        if (!loc.GetPacked_pnt().GetPoints().empty()) {
            *label += "PackSeqPnt";
        }
        last_id = &loc.GetPacked_pnt().GetId();
        break;
    case CSeq_loc::e_Mix:
        *label += "[";
        last_id = s_GetLabel(loc.GetMix().Get(), last_id, label);
        *label += "]";
        break;
    case CSeq_loc::e_Equiv:
        *label += "[";
        last_id = s_GetLabel(loc.GetEquiv().Get(), last_id, label);
        *label += "]";
        break;
    case CSeq_loc::e_Bond:
        last_id = s_GetLabel(loc.GetBond().GetA(), last_id, label);
        *label += "=";
        if (loc.GetBond().IsSetB()) {
            last_id = s_GetLabel(loc.GetBond().GetB(), last_id, label);
        } else {
            *label += "?";
        }
        break;
    case CSeq_loc::e_Feat:
        *label += "(feat)";
        break;
    default:
        *label += "(?\?)";
        break;
    }
    return last_id;
}

bool CSeq_loc::IsPartialLeft (void) const

{
    switch (Which ()) {

        case CSeq_loc::e_Null :
            return true;

        case CSeq_loc::e_Int :
            return GetInt ().IsPartialLeft ();

        case CSeq_loc::e_Pnt :
            return GetPnt ().IsPartialLeft ();

        case CSeq_loc::e_Mix :
            return GetMix ().IsPartialLeft ();

        default :
            break;
    }

    return false;
}

bool CSeq_loc::IsPartialRight (void) const

{
    switch (Which ()) {

        case CSeq_loc::e_Null :
            return true;

        case CSeq_loc::e_Int :
            return GetInt ().IsPartialRight ();

        case CSeq_loc::e_Pnt :
            return GetPnt ().IsPartialRight ();

        case CSeq_loc::e_Mix :
            return GetMix ().IsPartialRight ();

        default :
            break;
    }

    return false;
}

// Appends a label suitable for display (e.g., error messages)
// label must point to an existing string object
// Method just returns if label is null
void CSeq_loc::GetLabel(string* label) const
{
    s_GetLabel(*this, 0, label, true);
}


bool CSeq_loc::Equals(const CSerialObject& object) const
{
    if ( typeid(object) != typeid(*this) ) {
        ERR_POST(Fatal <<
            "CSeq_loc::Assign() -- Assignment of incompatible types: " <<
            typeid(*this).name() << " = " << typeid(object).name());
    }
    return CSerialObject::Equals(object);
}


END_objects_SCOPE // namespace ncbi::objects::
END_NCBI_SCOPE


/*
 * =============================================================================
 * $Log$
 * Revision 6.30  2003/06/18 16:00:07  vasilche
 * Fixed GetTotalRange() in multithreaded app.
 *
 * Revision 6.29  2003/06/13 17:21:20  grichenk
 * Added seq-id caching for single-id seq-locs
 *
 * Revision 6.28  2003/04/02 15:17:47  grichenk
 * Added flag for CSeq_loc_CI to skip/include empty locations.
 *
 * Revision 6.27  2003/03/11 15:55:44  kuznets
 * iterate -> ITERATE
 *
 * Revision 6.26  2003/02/24 18:52:13  vasilche
 * Added clearing of old data in assign.
 *
 * Revision 6.25  2003/02/06 22:23:29  vasilche
 * Added CSeq_id::Assign(), CSeq_loc::Assign().
 * Added int CSeq_id::Compare() (not safe).
 * Added caching of CSeq_loc::GetTotalRange().
 *
 * Revision 6.24  2003/02/04 15:15:15  grichenk
 * Overrided Assign() for CSeq_loc and CSeq_id
 *
 * Revision 6.23  2003/01/24 20:11:33  vasilche
 * Fixed trigraph warning on GCC.
 *
 * Revision 6.22  2003/01/22 20:17:33  vasilche
 * Optimized CSeq_loc::GetTotalRange().
 *
 * Revision 6.21  2002/12/30 19:37:02  vasilche
 * Rewrote CSeq_loc::GetTotalRange() to avoid using CSeq_loc_CI -
 * it's too expensive.
 *
 * Revision 6.20  2002/12/23 17:19:27  grichenk
 * Added GetSeq_loc() to CSeq_loc_CI
 *
 * Revision 6.19  2002/12/19 20:24:54  grichenk
 * Updated usage of CRange<>
 *
 * Revision 6.18  2002/12/06 15:36:04  grichenk
 * Added overlap type for annot-iterators
 *
 * Revision 6.17  2002/10/03 20:22:50  ucko
 * Drop duplicate default arg. spec. for s_GetLabel.
 *
 * Revision 6.16  2002/10/03 18:53:03  clausen
 * Removed extra whitespace
 *
 * Revision 6.15  2002/10/03 16:36:09  clausen
 * Added GetLabel()
 *
 * Revision 6.14  2002/09/12 21:19:02  kans
 * added IsPartialLeft and IsPartialRight
 *
 * Revision 6.13  2002/06/06 20:35:28  clausen
 * Moved methods using object manager to objects/util
 *
 * Revision 6.12  2002/05/31 13:33:02  grichenk
 * GetLength() -- return 0 for e_Null locations
 *
 * Revision 6.11  2002/05/06 03:39:12  vakatov
 * OM/OM1 renaming
 *
 * Revision 6.10  2002/05/03 21:28:18  ucko
 * Introduce T(Signed)SeqPos.
 *
 * Revision 6.9  2002/04/22 20:08:31  grichenk
 * Redesigned GetTotalRange() using CSeq_loc_CI
 *
 * Revision 6.8  2002/04/17 15:39:08  grichenk
 * Moved CSeq_loc_CI to the seq-loc library
 *
 * Revision 6.7  2002/01/16 18:56:32  grichenk
 * Removed CRef<> argument from choice variant setter, updated sources to
 * use references instead of CRef<>s
 *
 * Revision 6.6  2002/01/10 18:21:26  clausen
 * Added IsOneBioseq, GetStart, and GetId
 *
 * Revision 6.5  2001/10/22 11:40:32  clausen
 * Added Compare() implementation
 *
 * Revision 6.4  2001/01/03 18:59:09  vasilche
 * Added missing include.
 *
 * Revision 6.3  2001/01/03 16:39:05  vasilche
 * Added CAbstractObjectManager - stub for object manager.
 * CRange extracted to separate file.
 *
 * Revision 6.2  2000/12/26 17:28:55  vasilche
 * Simplified and formatted code.
 *
 * Revision 6.1  2000/11/17 21:35:10  vasilche
 * Added GetLength() method to CSeq_loc class.
 *
 *
 * ===========================================================================
*/
