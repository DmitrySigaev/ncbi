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
*   Seq-loc mapper
*
*/

#include <objmgr/seq_loc_mapper.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/impl/synonyms.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqfeat/Cdregion.hpp>
#include <objects/seqloc/Seq_loc_equiv.hpp>
#include <objects/seqloc/Seq_bond.hpp>
#include <objects/general/Int_fuzz.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


CMappingRange::CMappingRange(CSeq_id_Handle     src_id,
                             TSeqPos            src_from,
                             TSeqPos            src_length,
                             ENa_strand         src_strand,
                             const CSeq_id&     dst_id,
                             TSeqPos            dst_from,
                             ENa_strand         dst_strand)
    : m_Src_id_Handle(src_id),
      m_Src_from(src_from),
      m_Src_to(src_from + src_length - 1),
      m_Src_strand(src_strand),
      m_Dst_from(dst_from),
      m_Dst_strand(dst_strand),
      m_Reverse(!SameOrientation(src_strand, dst_strand))
{
    m_Dst_id.Reset(new CSeq_id);
    m_Dst_id->Assign(dst_id);
}


inline
bool CMappingRange::GoodSrcId(const CSeq_id& id) const
{
    return m_Src_id_Handle == id;
}


inline
CSeq_id& CMappingRange::GetDstId(void)
{
    return *m_Dst_id;
}


inline
bool CMappingRange::CanMap(TSeqPos from,
                           TSeqPos to,
                           bool is_set_strand,
                           ENa_strand strand)
{
    if (from > m_Src_to || to < m_Src_from) {
        return false;
    }
    return SameOrientation(is_set_strand ? strand : eNa_strand_unknown,
        m_Src_strand);
}


inline
TSeqPos CMappingRange::Map_Pos(TSeqPos pos) const
{
    _ASSERT(pos >= m_Src_from  &&  pos <= m_Src_to);
    if (!m_Reverse) {
        return m_Dst_from + pos - m_Src_from;
    }
    else {
        return m_Dst_from + m_Src_to - pos;
    }
}


inline
CMappingRange::TRange CMappingRange::Map_Range(TSeqPos from, TSeqPos to) const
{
    if (!m_Reverse) {
        return TRange(Map_Pos(from), Map_Pos(to));
    }
    else {
        return TRange(Map_Pos(to), Map_Pos(from));
    }
}


inline
bool CMappingRange::Map_Strand(bool is_set_strand,
                               ENa_strand src,
                               ENa_strand* dst) const
{
    _ASSERT(dst);
    if (is_set_strand) {
        *dst = m_Reverse ? Reverse(src) : src;
        return true;
    }
    if (m_Dst_strand != eNa_strand_unknown) {
        // Destination strand may be set for nucleotides
        // even if the source one is not set.
        *dst = m_Dst_strand;
        return true;
    }
    return false;
}


CSeq_loc_Mapper::CSeq_loc_Mapper(const CSeq_feat&  map_feat,
                                 EFeatMapDirection dir,
                                 CScope*           scope)
    : m_Scope(scope),
      m_MergeFlag(eMergeNone)
{
    _ASSERT(map_feat.IsSetProduct());
    int frame = 0;
    if (map_feat.GetData().IsCdregion()) {
        frame = map_feat.GetData().GetCdregion().GetFrame();
    }
    if (dir == eLocationToProduct) {
        x_Initialize(map_feat.GetLocation(), map_feat.GetProduct(), frame);
    }
    else {
        x_Initialize(map_feat.GetProduct(), map_feat.GetLocation(), frame);
    }
}


CSeq_loc_Mapper::CSeq_loc_Mapper(const CSeq_loc& source,
                                 const CSeq_loc& target,
                                 CScope* scope)
    : m_Scope(scope),
      m_MergeFlag(eMergeNone)
{
    x_Initialize(source, target);
}


CSeq_loc_Mapper::CSeq_loc_Mapper(const CSeq_align& /*map_align*/,
                                 const CSeq_id&    /*to_id*/,
                                 CScope*           scope)
    : m_Scope(scope),
      m_MergeFlag(eMergeNone)
{
    return;
}


CSeq_loc_Mapper::~CSeq_loc_Mapper(void)
{
    return;
}


int CSeq_loc_Mapper::x_CheckMolType(const CSeq_loc& loc,
                                    TSeqPos* total_length)
{
    int width = 0;
    *total_length = 0;
    for (CSeq_loc_CI src_it(loc); src_it; ++src_it) {
        *total_length += src_it.GetRange().GetLength();
        if ( !m_Scope ) {
            continue; // don't check sequence type;
        }
        CBioseq_Handle src_h = m_Scope->GetBioseqHandle(src_it.GetSeq_id());
        if ( !src_h ) {
            continue;
        }
        switch ( src_h.GetBioseqMolType() ) {
        case CSeq_inst::eMol_dna:
        case CSeq_inst::eMol_rna:
        case CSeq_inst::eMol_na:
            {
                if ( width != 3 ) {
                    if ( width ) {
                        // width already set to a different value
                        NCBI_THROW(CLocMapperException, eBadLocation,
                                   "Location contains different sequence types");
                    }
                    width = 3;
                }
                break;
            }
        case CSeq_inst::eMol_aa:
            {
                if ( width != 1 ) {
                    if ( width ) {
                        // width already set to a different value
                        NCBI_THROW(CLocMapperException, eBadLocation,
                                   "Location contains different sequence types");
                    }
                    width = 1;
                }
                break;
            }
        }
    }
    return width;
}


TSeqPos CSeq_loc_Mapper::x_GetRangeLength(const CSeq_loc_CI& it)
{
    if (it.IsWhole()  &&  IsReverse(it.GetStrand())) {
        // For reverse strand we need real interval length, not just "whole"
        CBioseq_Handle h;
        if (m_Scope) {
            h = m_Scope->GetBioseqHandle(it.GetSeq_id());
        }
        if ( !h ) {
            NCBI_THROW(CLocMapperException, eUnknownLength,
                       "Can not map from minus strand -- "
                       "unknown sequence length");
        }
        return h.GetBioseqLength();
    }
    else {
        return it.GetRange().GetLength();
    }
}


void CSeq_loc_Mapper::x_AddConversion(const CSeq_id& src_id,
                                      TSeqPos        src_start,
                                      ENa_strand     src_strand,
                                      const CSeq_id& dst_id,
                                      TSeqPos        dst_start,
                                      ENa_strand     dst_strand,
                                      TSeqPos        length)
{
    if (m_Scope) {
        CConstRef<CSynonymsSet> syns = m_Scope->GetSynonyms(src_id);
        ITERATE(CSynonymsSet, syn_it, *syns) {
            CRef<CMappingRange> cvt(new CMappingRange(
                CSynonymsSet::GetSeq_id_Handle(syn_it),
                src_start, length, src_strand,
                dst_id, dst_start, dst_strand));
            m_IdMap[cvt->m_Src_id_Handle].insert(TRangeMap::value_type(
                TRange(cvt->m_Src_from, cvt->m_Src_to), cvt));
        }
    }
    else {
        CRef<CMappingRange> cvt(new CMappingRange(
            CSeq_id_Handle::GetHandle(src_id),
            src_start, length, src_strand,
            dst_id, dst_start, dst_strand));
        m_IdMap[cvt->m_Src_id_Handle].insert(TRangeMap::value_type(
            TRange(cvt->m_Src_from, cvt->m_Src_to), cvt));
    }
}


void CSeq_loc_Mapper::x_Initialize(const CSeq_loc& source,
                                   const CSeq_loc& target,
                                   int             frame)
{
    // Check sequence type or total location length to
    // adjust intervals according to character width.
    TSeqPos src_total_len;
    m_Src_width = x_CheckMolType(source, &src_total_len);
    TSeqPos dst_total_len;
    m_Dst_width = x_CheckMolType(target, &dst_total_len);

    // Convert frame to na-shift
    if (!m_Src_width  ||  !m_Dst_width) {
        // Try to detect types by lengths
        if (!src_total_len  ||  !dst_total_len) {
            NCBI_THROW(CLocMapperException, eBadLocation,
                       "Zero location length -- "
                       "unable to detect sequence type");
        }
        if (src_total_len == dst_total_len) {
            m_Src_width = 1;
            m_Dst_width = 1;
            _ASSERT(!frame);
        }
        // truncate incomplete left and right codons
        else if (src_total_len/3 == dst_total_len) {
            m_Src_width = 3;
            m_Dst_width = 1;
        }
        else if (dst_total_len/3 == src_total_len) {
            m_Src_width = 1;
            m_Dst_width = 3;
        }
        else {
            NCBI_THROW(CAnnotException, eBadLocation,
                       "Wrong location length -- "
                       "unable to detect sequence type");
        }
    }
    else {
        if (m_Src_width == m_Dst_width) {
            m_Src_width = 1;
            m_Dst_width = 1;
            _ASSERT(!frame);
        }
    }

    // Create conversions
    CSeq_loc_CI src_it(source);
    CSeq_loc_CI dst_it(target);
    TSeqPos src_start = src_it.GetRange().GetFrom()*m_Dst_width;
    TSeqPos src_len = x_GetRangeLength(src_it)*m_Dst_width;
    TSeqPos dst_start = dst_it.GetRange().GetFrom()*m_Src_width;
    TSeqPos dst_len = x_GetRangeLength(dst_it)*m_Src_width;
    TSeqPos src_pos = 0;
    TSeqPos dst_pos = 0;
    if ( frame ) {
        // ignore pre-frame range
        if (m_Src_width == 3) {
            src_start += frame - 1;
        }
        if (m_Dst_width == 3) {
            dst_start += frame - 1;
        }
    }
    while (src_it  &&  dst_it) {
        TSeqPos cvt_src_start = src_start;
        TSeqPos cvt_dst_start = dst_start;
        TSeqPos cvt_length;

        if (src_len == dst_len) {
            cvt_length = src_len;
            src_pos += cvt_length;
            dst_pos += cvt_length;
            src_len = 0;
            dst_len = 0;
        }
        else if (src_len > dst_len) {
            // reverse interval for minus strand
            if (IsReverse(src_it.GetStrand())) {
                cvt_src_start += src_len - dst_len;
            }
            else {
                src_start += dst_len;
            }
            cvt_length = dst_len;
            src_len -= cvt_length;
            src_pos += cvt_length;
            dst_pos += cvt_length;
            dst_len = 0;
        }
        else { // if (src_len < dst_len)
            // reverse interval for minus strand
            if ( IsReverse(dst_it.GetStrand()) ) {
                cvt_dst_start += dst_len - src_len;
            }
            else {
                dst_start += src_len;
            }
            cvt_length = src_len;
            dst_len -= cvt_length;
            dst_pos += cvt_length;
            src_pos += cvt_length;
            src_len = 0;
        }
        x_AddConversion(
            src_it.GetSeq_id(), cvt_src_start, src_it.GetStrand(),
            dst_it.GetSeq_id(), cvt_dst_start, dst_it.GetStrand(),
            cvt_length);
        if (src_len == 0  &&  ++src_it) {
            src_start = src_it.GetRange().GetFrom()*m_Dst_width;
            src_len = x_GetRangeLength(src_it)*m_Dst_width;
        }
        if (dst_len == 0  &&  ++dst_it) {
            dst_start = dst_it.GetRange().GetFrom()*m_Src_width;
            dst_len = x_GetRangeLength(dst_it)*m_Src_width;
        }
    }
}


CSeq_loc_Mapper::TRangeIterator
CSeq_loc_Mapper::x_BeginMappingRanges(CSeq_id_Handle id,
                                      TSeqPos from,
                                      TSeqPos to)
{
    TIdMap::iterator ranges = m_IdMap.find(id);
    if (ranges == m_IdMap.end()) {
        return TRangeIterator();
    }
    return ranges->second.begin(TRange(from, to));
}


bool CSeq_loc_Mapper::x_MapInterval(const CSeq_id&   src_id,
                                    TRange           src_rg,
                                    bool             is_set_strand,
                                    ENa_strand       src_strand)
{
    bool res = false;
    if (m_Dst_width == 3) {
        src_rg = TRange(src_rg.GetFrom()*3, src_rg.GetTo()*3 + 2);
    }

    TRangeIterator mit = x_BeginMappingRanges(
        CSeq_id_Handle::GetHandle(src_id),
        src_rg.GetFrom(), src_rg.GetTo());
    for ( ; mit; ++mit) {
        CMappingRange& cvt = *mit->second;
        if ( !cvt.CanMap(src_rg.GetFrom(), src_rg.GetTo(),
            is_set_strand, src_strand) ) {
            continue;
        }
        TSeqPos from = src_rg.GetFrom();
        TSeqPos to = src_rg.GetTo();
        if (from < cvt.m_Src_from) {
            from = cvt.m_Src_from;
            m_Partial = true;
        }
        if ( to > cvt.m_Src_to ) {
            to = cvt.m_Src_to;
            m_Partial = true;
        }
        if ( from > to ) {
            continue;
        }
        TRange rg = cvt.Map_Range(from, to);
        ENa_strand dst_strand;
        bool is_set_dst_strand = cvt.Map_Strand(is_set_strand,
            src_strand, &dst_strand);
        x_GetMappedRanges(CSeq_id_Handle::GetHandle(*cvt.m_Dst_id),
            is_set_dst_strand ? int(dst_strand) + 1 : 0).push_back(rg);
        res = true;
    }
    return res;
}


void CSeq_loc_Mapper::x_PushLocToDstMix(CRef<CSeq_loc> loc)
{
    _ASSERT(loc);
    if ( !m_Dst_loc  ||  !m_Dst_loc->IsMix() ) {
        CRef<CSeq_loc> tmp = m_Dst_loc;
        m_Dst_loc.Reset(new CSeq_loc);
        m_Dst_loc->SetMix();
        if ( tmp ) {
            m_Dst_loc->SetMix().Set().push_back(tmp);
        }
    }
    CSeq_loc_mix::Tdata& mix = m_Dst_loc->SetMix().Set();
    if ( loc->IsNull()  &&  mix.size() > 0  &&  (*mix.rbegin())->IsNull() ) {
        // do not create duplicate NULLs
        return;
    }
    mix.push_back(loc);
}


inline
void CSeq_loc_Mapper::x_PushRangesToDstMix(void)
{
    if (m_MappedLocs.size() == 0) {
        return;
    }
    // Push everything already mapped to the destination mix
    CRef<CSeq_loc> loc = x_GetMappedSeq_loc();
    if ( !m_Dst_loc ) {
        m_Dst_loc = loc;
        return;
    }
    if ( !loc->IsNull() ) {
        x_PushLocToDstMix(loc);
    }
}


CRef<CSeq_loc> CSeq_loc_Mapper::Map(const CSeq_loc& src_loc)
{
    m_Dst_loc.Reset();
    m_Partial = false; // reset for each location
    x_MapSeq_loc(src_loc);
    x_PushRangesToDstMix();
    x_OptimizeSeq_loc(m_Dst_loc);
    return m_Dst_loc;
}


void CSeq_loc_Mapper::x_MapSeq_loc(const CSeq_loc& src_loc)
{
    switch ( src_loc.Which() ) {
    case CSeq_loc::e_not_set:
    case CSeq_loc::e_Feat:
    case CSeq_loc::e_Null:
    {
        x_PushRangesToDstMix();
        CRef<CSeq_loc> loc(new CSeq_loc);
        loc->Assign(src_loc);
        x_PushLocToDstMix(loc);
        break;
    }
    case CSeq_loc::e_Empty:
    {
        bool res = false;
        TRangeIterator mit = x_BeginMappingRanges(
            CSeq_id_Handle::GetHandle(src_loc.GetEmpty()),
            TRange::GetWhole().GetFrom(),
            TRange::GetWhole().GetTo());
        for ( ; mit; ++mit) {
            CMappingRange& cvt = *mit->second;
            if ( cvt.GoodSrcId(src_loc.GetEmpty()) ) {
                x_GetMappedRanges(
                    CSeq_id_Handle::GetHandle(src_loc.GetEmpty()), 0)
                    .push_back(TRange::GetEmpty());
                res = true;
                break;
            }
        }
        if ( !res ) {
            m_Partial = true;
        }
        break;
    }
    case CSeq_loc::e_Whole:
    {
        const CSeq_id& src_id = src_loc.GetWhole();
        // Convert to the allowed master seq interval
        TSeqPos src_to = kInvalidSeqPos;
        if ( m_Scope ) {
            CBioseq_Handle bh = m_Scope->GetBioseqHandle(src_id);
            src_to = bh ? bh.GetBioseqLength() : kInvalidSeqPos;
        }
        bool res = x_MapInterval(src_id, TRange(0, src_to),
            false, eNa_strand_unknown);
        if ( !res ) {
            m_Partial = true;
        }
        break;
    }
    case CSeq_loc::e_Int:
    {
        const CSeq_interval& src_int = src_loc.GetInt();
        x_MapInterval(src_int.GetId(),
            TRange(src_int.GetFrom(), src_int.GetTo()),
            src_int.IsSetStrand(),
            src_int.IsSetStrand() ? src_int.GetStrand() : eNa_strand_unknown);
        break;
    }
    case CSeq_loc::e_Pnt:
    {
        const CSeq_point& pnt = src_loc.GetPnt();
        x_MapInterval(pnt.GetId(),
            TRange(pnt.GetPoint(), pnt.GetPoint()),
            pnt.IsSetStrand(),
            pnt.IsSetStrand() ? pnt.GetStrand() : eNa_strand_unknown);
        break;
    }
    case CSeq_loc::e_Packed_int:
    {
        const CPacked_seqint::Tdata& src_ints = src_loc.GetPacked_int().Get();
        ITERATE ( CPacked_seqint::Tdata, i, src_ints ) {
            const CSeq_interval& si = **i;
            x_MapInterval(si.GetId(),
                TRange(si.GetFrom(), si.GetTo()),
                si.IsSetStrand(),
                si.IsSetStrand() ? si.GetStrand() : eNa_strand_unknown);
        }
        break;
    }
    case CSeq_loc::e_Packed_pnt:
    {
        const CPacked_seqpnt& src_pack_pnts = src_loc.GetPacked_pnt();
        const CPacked_seqpnt::TPoints& src_pnts = src_pack_pnts.GetPoints();
        bool res = false;
        ITERATE ( CPacked_seqpnt::TPoints, i, src_pnts ) {
            x_MapInterval(
                src_pack_pnts.GetId(),
                TRange(*i, *i), src_pack_pnts.IsSetStrand(),
                src_pack_pnts.IsSetStrand() ?
                src_pack_pnts.GetStrand() : eNa_strand_unknown);
        }
        break;
    }
    case CSeq_loc::e_Mix:
    {
        x_PushRangesToDstMix();
        CRef<CSeq_loc> prev = m_Dst_loc;
        m_Dst_loc.Reset();
        const CSeq_loc_mix::Tdata& src_mix = src_loc.GetMix().Get();
        ITERATE ( CSeq_loc_mix::Tdata, i, src_mix ) {
            x_MapSeq_loc(**i);
        }
        x_PushRangesToDstMix();
        CRef<CSeq_loc> mix = m_Dst_loc;
        m_Dst_loc = prev;
        x_OptimizeSeq_loc(mix);
        x_PushLocToDstMix(mix);
        break;
    }
    case CSeq_loc::e_Equiv:
    {
        x_PushRangesToDstMix();
        CRef<CSeq_loc> prev = m_Dst_loc;
        m_Dst_loc.Reset();
        const CSeq_loc_equiv::Tdata& src_equiv = src_loc.GetEquiv().Get();
        CRef<CSeq_loc> equiv(new CSeq_loc);
        equiv->SetEquiv();
        ITERATE ( CSeq_loc_equiv::Tdata, i, src_equiv ) {
            x_MapSeq_loc(**i);
            x_PushRangesToDstMix();
            x_OptimizeSeq_loc(m_Dst_loc);
            equiv->SetEquiv().Set().push_back(m_Dst_loc);
            m_Dst_loc.Reset();
        }
        m_Dst_loc = prev;
        x_PushLocToDstMix(equiv);
        break;
    }
    case CSeq_loc::e_Bond:
    {
        x_PushRangesToDstMix();
        CRef<CSeq_loc> prev = m_Dst_loc;
        m_Dst_loc.Reset();
        const CSeq_bond& src_bond = src_loc.GetBond();
        CRef<CSeq_loc> dst_loc(new CSeq_loc);
        dst_loc->SetBond();
        CRef<CSeq_loc> pntA;
        CRef<CSeq_loc> pntB;
        bool resA = x_MapInterval(src_bond.GetA().GetId(),
            TRange(src_bond.GetA().GetPoint(), src_bond.GetA().GetPoint()),
            src_bond.GetA().IsSetStrand(),
            src_bond.GetA().IsSetStrand() ?
            src_bond.GetA().GetStrand() : eNa_strand_unknown);
        if ( resA ) {
            pntA = x_GetMappedSeq_loc();
            _ASSERT(pntA);
            if ( !pntA->IsPnt() ) {
                NCBI_THROW(CLocMapperException, eBadLocation,
                           "Bond point A can not be mapped to a point");
            }
        }
        else {
            pntA.Reset(new CSeq_loc);
            pntA->SetPnt().Assign(src_bond.GetA());
        }
        bool resB = false;
        if ( src_bond.IsSetB() ) {
            resB = x_MapInterval(src_bond.GetB().GetId(),
                TRange(src_bond.GetB().GetPoint(), src_bond.GetB().GetPoint()),
                src_bond.GetB().IsSetStrand(),
                src_bond.GetB().IsSetStrand() ?
                src_bond.GetB().GetStrand() : eNa_strand_unknown);
        }
        if ( resB ) {
            pntB = x_GetMappedSeq_loc();
            _ASSERT(pntB);
            if ( !pntB->IsPnt() ) {
                NCBI_THROW(CLocMapperException, eBadLocation,
                           "Bond point B can not be mapped to a point");
            }
        }
        else {
            pntB.Reset(new CSeq_loc);
            pntB->SetPnt().Assign(src_bond.GetB());
        }
        m_Dst_loc = prev;
        if ( resA || resB ) {
            CSeq_bond& dst_bond = dst_loc->SetBond();
            dst_bond.SetA(pntA->SetPnt());
            if ( src_bond.IsSetB() ) {
                dst_bond.SetB(pntB->SetPnt());
            }
            x_PushLocToDstMix(dst_loc);
        }
        m_Partial |= (!resA || !resB);
        break;
    }
    default:
        NCBI_THROW(CAnnotException, eBadLocation,
                   "Unsupported location type");
    }
}


CSeq_loc_Mapper::TMappedRanges&
CSeq_loc_Mapper::x_GetMappedRanges(const CSeq_id_Handle& id,
                                   int strand_idx) const
{
    TRangesByStrand& str_vec = m_MappedLocs[id];
    if (str_vec.size() <= strand_idx) {
        str_vec.resize(strand_idx + 1);
    }
    return str_vec[strand_idx];
}


CRef<CSeq_loc> CSeq_loc_Mapper::x_RangeToSeq_loc(const CSeq_id_Handle& idh,
                                                 TSeqPos from,
                                                 TSeqPos to,
                                                 int strand_idx)
{
    if ( m_Src_width == 3 ) {
        // Do not convert if destination is nuc. and this is
        // convertion from a protein.
        from = from/3;
        to = to/3;
    }

    CRef<CSeq_loc> loc(new CSeq_loc);
    if (from == to) {
        // point
        loc->SetPnt().SetId().Assign(*idh.GetSeqId());
        loc->SetPnt().SetPoint(from);
        if (strand_idx > 0) {
            loc->SetPnt().SetStrand(ENa_strand(strand_idx - 1));
        }
    }
    else {
        // interval
        loc->SetInt().SetId().Assign(*idh.GetSeqId());
        loc->SetInt().SetFrom(from);
        loc->SetInt().SetTo(to);
        if (strand_idx > 0) {
            loc->SetInt().SetStrand(ENa_strand(strand_idx - 1));
        }
    }
    return loc;
}


void CSeq_loc_Mapper::x_OptimizeSeq_loc(CRef<CSeq_loc>& loc)
{
    if ( !loc ) {
        loc.Reset(new CSeq_loc);
        loc->SetNull();
        return;
    }
    switch (loc->Which()) {
    case CSeq_loc::e_not_set:
    case CSeq_loc::e_Feat:
    case CSeq_loc::e_Null:
    case CSeq_loc::e_Empty:
    case CSeq_loc::e_Whole:
    case CSeq_loc::e_Int:
    case CSeq_loc::e_Pnt:
    case CSeq_loc::e_Equiv:
    case CSeq_loc::e_Bond:
    case CSeq_loc::e_Packed_int:  // pack ?
    case CSeq_loc::e_Packed_pnt:  // pack ?
        return;
    case CSeq_loc::e_Mix:
    {
        switch ( loc->GetMix().Get().size() ) {
        case 0:
            loc->SetNull();
            break;
        case 1:
        {
            CRef<CSeq_loc> single = *loc->SetMix().Set().begin();
            loc = single;
            break;
        }
        default:
            break;
        }
        break;
    }
    default:
        NCBI_THROW(CAnnotException, eBadLocation,
                   "Unsupported location type");
    }
}


CRef<CSeq_loc> CSeq_loc_Mapper::x_GetMappedSeq_loc(void)
{
    CRef<CSeq_loc> dst_loc(new CSeq_loc);
    CSeq_loc_mix::Tdata& dst_mix = dst_loc->SetMix().Set();
    NON_CONST_ITERATE(TRangesById, id_it, m_MappedLocs) {
        if ( !id_it->first ) {
            CRef<CSeq_loc> null_loc(new CSeq_loc);
            null_loc->SetNull();
            dst_mix.push_back(null_loc);
            continue;
        }
        for (int str = 0; str < id_it->second.size(); ++str) {
            if (id_it->second[str].size() == 0) {
                continue;
            }
            TSeqPos from = kInvalidSeqPos;
            TSeqPos to = kInvalidSeqPos;
            id_it->second[str].sort();
            NON_CONST_ITERATE(TMappedRanges, rg_it, id_it->second[str]) {
                if ( rg_it->Empty() ) {
                    // Empty seq-loc
                    CRef<CSeq_loc> loc(new CSeq_loc);
                    loc->SetEmpty().Assign(*id_it->first.GetSeqId());
                    dst_mix.push_back(loc);
                    continue;
                }
                if (to == kInvalidSeqPos) {
                    from = rg_it->GetFrom();
                    to = rg_it->GetTo();
                    continue;
                }
                if (m_MergeFlag == eMergeAbutting) {
                    if (rg_it->GetFrom() == to + 1) {
                        to = rg_it->GetTo();
                        continue;
                    }
                }
                if (m_MergeFlag == eMergeAll) {
                    if (rg_it->GetFrom() <= to + 1) {
                        to = rg_it->GetTo();
                        continue;
                    }
                }
                // Add new interval or point
                dst_mix.push_back(x_RangeToSeq_loc(id_it->first, from, to, str));
                from = rg_it->GetFrom();
                to = rg_it->GetTo();
            }
            // last interval or point
            dst_mix.push_back(x_RangeToSeq_loc(id_it->first, from, to, str));
        }
    }
    m_MappedLocs.clear();
    x_OptimizeSeq_loc(dst_loc);
    return dst_loc;
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.3  2004/03/11 18:48:02  shomrat
* skip if empty
*
* Revision 1.2  2004/03/11 04:54:48  grichenk
* Removed inline
*
* Revision 1.1  2004/03/10 16:22:29  grichenk
* Initial revision
*
*
* ===========================================================================
*/
