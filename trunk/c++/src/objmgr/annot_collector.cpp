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
*   Annotation collector for annot iterators
*
*/

#include <ncbi_pch.hpp>
#include <objmgr/impl/annot_collector.hpp>

#include <objmgr/scope.hpp>
#include <objmgr/bioseq_handle.hpp>
#include <objmgr/seq_entry_handle.hpp>
#include <objmgr/seq_annot_handle.hpp>
#include <objmgr/seq_map_ci.hpp>
#include <objmgr/impl/annot_object.hpp>
#include <objmgr/impl/tse_info.hpp>
#include <objmgr/impl/annot_type_index.hpp>
#include <objmgr/impl/tse_chunk_info.hpp>
#include <objmgr/impl/data_source.hpp>
#include <objmgr/impl/seq_annot_info.hpp>
#include <objmgr/impl/bioseq_set_info.hpp>
#include <objmgr/impl/handle_range_map.hpp>
#include <objmgr/impl/synonyms.hpp>
#include <objmgr/impl/seq_loc_cvt.hpp>
#include <objmgr/impl/seq_align_mapper.hpp>
#include <objmgr/impl/snp_annot_info.hpp>
#include <objmgr/impl/bioseq_info.hpp>
#include <objmgr/impl/scope_impl.hpp>
#include <objmgr/objmgr_exception.hpp>

#include <objects/seq/Bioseq.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objects/seqalign/Seq_align.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqres/Seq_graph.hpp>
#include <objects/seqfeat/SeqFeatData.hpp>
#include <objects/seqloc/Seq_loc_equiv.hpp>
#include <objects/seqloc/Seq_bond.hpp>

#include <serial/typeinfo.hpp>
#include <serial/objostr.hpp>
#include <serial/objostrasn.hpp>
#include <serial/serial.hpp>
#include <serial/serialutil.hpp>

#include <algorithm>
#include <typeinfo>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


/////////////////////////////////////////////////////////////////////////////
// CAnnotObject_Ref
/////////////////////////////////////////////////////////////////////////////


inline
CAnnotObject_Ref::CAnnotObject_Ref(const CAnnotObject_Info& object)
    : m_Object(&object.GetSeq_annot_Info()),
      m_AnnotObject_Index(object.GetSeq_annot_Info()
                          .GetAnnotObjectIndex(object)),
      m_ObjectType(eType_Seq_annot_Info),
      m_MappedFlags(0),
      m_MappedObjectType(eMappedObjType_not_set),
      m_MappedStrand(eNa_strand_unknown)
{
    if ( object.IsFeat() ) {
        const CSeq_feat& feat = *object.GetFeatFast();
        if ( feat.IsSetPartial() ) {
            SetPartial(feat.GetPartial());
        }
    }
}


inline
CAnnotObject_Ref::CAnnotObject_Ref(const CSeq_annot_SNP_Info& snp_annot,
                                   TSeqPos index)
    : m_Object(&snp_annot),
      m_AnnotObject_Index(index),
      m_ObjectType(eType_Seq_annot_SNP_Info),
      m_MappedFlags(0),
      m_MappedObjectType(eMappedObjType_not_set),
      m_MappedStrand(eNa_strand_unknown)
{
}


const CAnnotObject_Info& CAnnotObject_Ref::GetAnnotObject_Info(void) const
{
    return GetSeq_annot_Info().GetAnnotObject_Info(GetAnnotObjectIndex());
}


const SSNP_Info& CAnnotObject_Ref::GetSNP_Info(void) const
{
    return GetSeq_annot_SNP_Info().GetSNP_Info(GetAnnotObjectIndex());
}


bool CAnnotObject_Ref::IsFeat(void) const
{
    return GetObjectType() == eType_Seq_annot_SNP_Info ||
        (GetObjectType() == eType_Seq_annot_Info &&
         GetAnnotObject_Info().IsFeat());
}


bool CAnnotObject_Ref::IsGraph(void) const
{
    return GetObjectType() == eType_Seq_annot_Info &&
        GetAnnotObject_Info().IsGraph();
}


bool CAnnotObject_Ref::IsAlign(void) const
{
    return GetObjectType() == eType_Seq_annot_Info &&
        GetAnnotObject_Info().IsAlign();
}


const CSeq_feat& CAnnotObject_Ref::GetFeat(void) const
{
    return GetAnnotObject_Info().GetFeat();
}


const CSeq_graph& CAnnotObject_Ref::GetGraph(void) const
{
    return GetAnnotObject_Info().GetGraph();
}


const CSeq_align& CAnnotObject_Ref::GetAlign(void) const
{
    return GetAnnotObject_Info().GetAlign();
}


inline
void CAnnotObject_Ref::SetSNP_Point(const SSNP_Info& snp,
                                    CSeq_loc_Conversion* cvt)
{
    _ASSERT(GetObjectType() == eType_Seq_annot_SNP_Info);
    TSeqPos src_from = snp.GetFrom(), src_to = snp.GetTo();
    ENa_strand src_strand =
        snp.MinusStrand()? eNa_strand_minus: eNa_strand_plus;
    if ( !cvt ) {
        SetTotalRange(TRange(src_from, src_to));
        SetMappedSeq_id(const_cast<CSeq_id&>(GetSeq_annot_SNP_Info().                                             GetSeq_id()),
                        true);
        SetMappedStrand(src_strand);
        return;
    }

    cvt->Reset();
    if ( src_from == src_to ) {
        // point
        _VERIFY(cvt->ConvertPoint(src_from, src_strand));
    }
    else {
        // interval
        _VERIFY(cvt->ConvertInterval(src_from, src_to, src_strand));
    }
    cvt->SetMappedLocation(*this, CSeq_loc_Conversion::eLocation);
}


const CSeq_align&
CAnnotObject_Ref::GetMappedSeq_align(void) const
{
    if (m_MappedObjectType == eMappedObjType_Seq_loc_Conv_Set) {
        // Map the alignment, replace conv-set with the mapped align
        CSeq_loc_Conversion_Set& cvts =
            const_cast<CSeq_loc_Conversion_Set&>(
            *CTypeConverter<CSeq_loc_Conversion_Set>::
            SafeCast(m_MappedObject.GetPointer()));
        CRef<CSeq_align> dst;
        cvts.Convert(GetAlign(), &dst);
        const_cast<CAnnotObject_Ref&>(*this).
            SetMappedSeq_align(dst.GetPointerOrNull());
    }
    _ASSERT(m_MappedObjectType == eMappedObjType_Seq_align);
    return *CTypeConverter<CSeq_align>::
        SafeCast(m_MappedObject.GetPointer());
}


void CAnnotObject_Ref::SetMappedSeq_align_Cvts(CSeq_loc_Conversion_Set& cvts)
{
    _ASSERT(!IsMapped());
    m_MappedObject.Reset(&cvts);
    m_MappedObjectType = eMappedObjType_Seq_loc_Conv_Set;
}


void CAnnotObject_Ref::SetMappedSeq_align(CSeq_align* align)
{
    _ASSERT(m_MappedObjectType == eMappedObjType_Seq_loc_Conv_Set);
    m_MappedObject.Reset(align);
    m_MappedObjectType =
        align? eMappedObjType_Seq_align: eMappedObjType_not_set;
}


void CAnnotObject_Ref::UpdateMappedSeq_loc(CRef<CSeq_loc>& loc) const
{
    _ASSERT(MappedSeq_locNeedsUpdate());
    
    CSeq_id& id = const_cast<CSeq_id&>(GetMappedSeq_id());
    if ( !loc || !loc->ReferencedOnlyOnce() ) {
        loc.Reset(new CSeq_loc);
    }
    else {
        loc->InvalidateTotalRangeCache();
    }
    if ( IsMappedPoint() ) {
        CSeq_point& point = loc->SetPnt();
        point.SetId(id);
        point.SetPoint(m_TotalRange.GetFrom());
        if ( GetMappedStrand() != eNa_strand_unknown )
            point.SetStrand(GetMappedStrand());
        else
            point.ResetStrand();
    }
    else {
        CSeq_interval& interval = loc->SetInt();
        interval.SetId(id);
        interval.SetFrom(GetTotalRange().GetFrom());
        interval.SetTo(GetTotalRange().GetTo());
        if ( GetMappedStrand() != eNa_strand_unknown )
            interval.SetStrand(GetMappedStrand());
        else
            interval.ResetStrand();
    }
}


void CAnnotObject_Ref::UpdateMappedSeq_loc(CRef<CSeq_loc>& loc,
                                           CRef<CSeq_point>& pnt_ref,
                                           CRef<CSeq_interval>& int_ref) const
{
    _ASSERT(MappedSeq_locNeedsUpdate());

    CSeq_id& id = const_cast<CSeq_id&>(GetMappedSeq_id());
    if ( !loc || !loc->ReferencedOnlyOnce() ) {
        loc.Reset(new CSeq_loc);
    }
    else {
        loc->Reset();
        loc->InvalidateTotalRangeCache();
    }
    if ( IsMappedPoint() ) {
        if ( !pnt_ref || !pnt_ref->ReferencedOnlyOnce() ) {
            pnt_ref.Reset(new CSeq_point);
        }
        CSeq_point& point = *pnt_ref;
        loc->SetPnt(point);
        point.SetId(id);
        point.SetPoint(m_TotalRange.GetFrom());
        if ( GetMappedStrand() != eNa_strand_unknown )
            point.SetStrand(GetMappedStrand());
        else
            point.ResetStrand();
    }
    else {
        if ( !int_ref || !int_ref->ReferencedOnlyOnce() ) {
            int_ref.Reset(new CSeq_interval);
        }
        CSeq_interval& interval = *int_ref;
        loc->SetInt(interval);
        interval.SetId(id);
        interval.SetFrom(m_TotalRange.GetFrom());
        interval.SetTo(m_TotalRange.GetTo());
        if ( GetMappedStrand() != eNa_strand_unknown )
            interval.SetStrand(GetMappedStrand());
        else
            interval.ResetStrand();
    }
}


bool CAnnotObject_Ref::operator<(const CAnnotObject_Ref& ref) const
{
    if ( m_Object != ref.m_Object ) {
        return m_Object < ref.m_Object;
    }
    return GetAnnotObjectIndex() < ref.GetAnnotObjectIndex();
}


/////////////////////////////////////////////////////////////////////////////
// CAnnotObject_Ref comparision
/////////////////////////////////////////////////////////////////////////////

struct CAnnotObjectType_Less
{
    bool operator()(const CAnnotObject_Ref& x,
                    const CAnnotObject_Ref& y) const;

    static const CSeq_loc& GetLocation(const CAnnotObject_Ref& ref,
                                       const CSeq_feat& feat);
};


const CSeq_loc& CAnnotObjectType_Less::GetLocation(const CAnnotObject_Ref& ref,
                                                   const CSeq_feat& feat)
{
    if ( ref.GetMappedObjectType() == ref.eMappedObjType_Seq_loc &&
         !ref.IsProduct() ) {
        return ref.GetMappedSeq_loc();
    }
    else {
        return feat.GetLocation();
    }
}


bool CAnnotObjectType_Less::operator()(const CAnnotObject_Ref& x,
                                       const CAnnotObject_Ref& y) const
{
    // gather x annotation type
    const CAnnotObject_Info* x_info;
    CSeq_annot::C_Data::E_Choice x_annot_type;
    if ( !x.IsSNPFeat() ) {
        const CSeq_annot_Info& x_annot = x.GetSeq_annot_Info();
        x_info = &x_annot.GetAnnotObject_Info(x.GetAnnotObjectIndex());
        x_annot_type = x_info->GetAnnotType();
    }
    else {
        x_info = 0;
        x_annot_type = CSeq_annot::C_Data::e_Ftable;
    }

    // gather y annotation type
    const CAnnotObject_Info* y_info;
    CSeq_annot::C_Data::E_Choice y_annot_type;
    if ( !y.IsSNPFeat() ) {
        const CSeq_annot_Info& y_annot = y.GetSeq_annot_Info();
        y_info = &y_annot.GetAnnotObject_Info(y.GetAnnotObjectIndex());
        y_annot_type = y_info->GetAnnotType();
    }
    else {
        y_info = 0;
        y_annot_type = CSeq_annot::C_Data::e_Ftable;
    }

    // compare by annotation type (feature, align, graph)
    if ( x_annot_type != y_annot_type ) {
        return x_annot_type < y_annot_type;
    }

    if ( x_annot_type == CSeq_annot::C_Data::e_Ftable ) {
        // compare features by type
        if ( !x_info != !y_info ) {
            CSeqFeatData::E_Choice x_feat_type = CSeqFeatData::e_Imp;
            CSeqFeatData::ESubtype x_feat_subtype =
                CSeqFeatData::eSubtype_variation;
            if ( x_info ) {
                x_feat_type = x_info->GetFeatType();
                x_feat_subtype = x_info->GetFeatSubtype();
            }

            CSeqFeatData::E_Choice y_feat_type = CSeqFeatData::e_Imp;
            CSeqFeatData::ESubtype y_feat_subtype =
                CSeqFeatData::eSubtype_variation;
            if ( y_info ) {
                y_feat_type = y_info->GetFeatType();
                y_feat_subtype = y_info->GetFeatSubtype();
            }

            // one is simple SNP feature, another is some complex feature
            if ( x_feat_type != y_feat_type ) {
                int x_order = CSeq_feat::GetTypeSortingOrder(x_feat_type);
                int y_order = CSeq_feat::GetTypeSortingOrder(y_feat_type);
                if ( x_order != y_order ) {
                    return x_order < y_order;
                }
            }

            // non-variations first
            if ( x_feat_subtype != y_feat_subtype ) {
                return x_feat_subtype < y_feat_subtype;
            }

            // both are variations but one is simple and another is not.
            // required order: simple first.
            // if !x_info == true -> x - simple, y - complex -> return true
            // if !x_info == false -> x - complex, y - simple -> return false
            return !x_info;
        }

        if ( x_info ) {
            // both are complex features
            try {
                const CSeq_feat* x_feat = x_info->GetFeatFast();
                const CSeq_feat* y_feat = y_info->GetFeatFast();
                _ASSERT(x_feat && y_feat);
                int diff = x_feat->CompareNonLocation(*y_feat,
                                                      GetLocation(x, *x_feat),
                                                      GetLocation(y, *y_feat));
                if ( diff != 0 ) {
                    return diff < 0;
                }
            }
            catch ( exception& /*ignored*/ ) {
                // do not fail sort when compare function throws an exception
            }
        }
    }
    return x < y;
}


struct CAnnotObject_Less
{
    // Compare CRef-s: both must be features
    bool operator()(const CAnnotObject_Ref& x,
                    const CAnnotObject_Ref& y) const
        {
            {
                // smallest left extreme first
                TSeqPos x_from = x.GetFrom();
                TSeqPos y_from = y.GetFrom();
                if ( x_from != y_from ) {
                    return x_from < y_from;
                }
            }
            {
                // longest feature first
                TSeqPos x_to = x.GetToOpen();
                TSeqPos y_to = y.GetToOpen();
                if ( x_to != y_to ) {
                    return x_to > y_to;
                }
            }
            return type_less(x, y);
        }
    CAnnotObjectType_Less type_less;
};


struct CAnnotObject_LessReverse
{
    // Compare CRef-s: both must be features
    bool operator()(const CAnnotObject_Ref& x,
                    const CAnnotObject_Ref& y) const
        {
            {
                // largest right extreme first
                TSeqPos x_to = x.GetToOpen();
                TSeqPos y_to = y.GetToOpen();
                if ( x_to != y_to ) {
                    return x_to > y_to;
                }
            }
            {
                // longest feature first
                TSeqPos x_from = x.GetFrom();
                TSeqPos y_from = y.GetFrom();
                if ( x_from != y_from ) {
                    return x_from < y_from;
                }
            }
            return type_less(x, y);
        }
    CAnnotObjectType_Less type_less;
};


/////////////////////////////////////////////////////////////////////////////
// CAnnot_Collector, CAnnotMappingCollector
/////////////////////////////////////////////////////////////////////////////


class CAnnotMappingCollector
{
public:
    typedef map<CAnnotObject_Ref,
                CRef<CSeq_loc_Conversion_Set> > TAnnotMappingSet;
    // Set of annotations for complex remapping
    TAnnotMappingSet              m_AnnotMappingSet;
    // info of limit object
    CConstRef<CObject>            m_LimitObjectInfo;
};


CAnnot_Collector::CAnnot_Collector(const SAnnotSelector& selector,
                                   CScope&               scope)
    : m_Selector(selector),
      m_Scope(scope),
      m_MappingCollector(new CAnnotMappingCollector)
{
    return;
}

CAnnot_Collector::~CAnnot_Collector(void)
{
    x_Clear();
}


inline
size_t CAnnot_Collector::x_GetAnnotCount(void) const
{
    return m_AnnotSet.size() +
        (m_MappingCollector.get() ?
        m_MappingCollector->m_AnnotMappingSet.size() : 0);
}


void CAnnot_Collector::x_Clear(void)
{
    m_AnnotSet.clear();
    if ( m_MappingCollector.get() ) {
        m_MappingCollector.reset();
    }
    m_TSE_LockSet.clear();
    m_Scope = CHeapScope();
}


void CAnnot_Collector::x_Initialize(const CBioseq_Handle& bioseq,
                                    TSeqPos               start,
                                    TSeqPos               stop)
{
    try {
        if ( start == 0 && stop == 0 ) {
            stop = bioseq.GetBioseqLength()-1;
        }
        CHandleRangeMap master_loc;
        master_loc.AddRange(bioseq.GetSeq_id_Handle(),
                            CHandleRange::TRange(start, stop),
                            eNa_strand_unknown);
        x_Initialize(master_loc);
    } catch (...) {
        // clear all members - GCC 3.0.4 does not do it
        x_Clear();
        throw;
    }
}


void CAnnot_Collector::x_Initialize(const CHandleRangeMap& master_loc)
{
    try {
        if ( !m_Selector.m_LimitObject ) {
            m_Selector.m_LimitObjectType = SAnnotSelector::eLimit_None;
        }
        if ( m_Selector.m_LimitObjectType != SAnnotSelector::eLimit_None ) {
            x_GetTSE_Info();
        }

        bool found = x_Search(master_loc, 0);
        bool deeper = !(found && m_Selector.m_AdaptiveDepth) &&
            m_Selector.m_ResolveMethod != SAnnotSelector::eResolve_None  &&
            m_Selector.m_ResolveDepth > 0;
        if ( deeper ) {
            ITERATE ( CHandleRangeMap::TLocMap, idit, master_loc.GetMap() ) {
                //### Check for eLoadedOnly
                CBioseq_Handle bh = m_Scope->GetBioseqHandle(idit->first,
                    CScope::eGetBioseq_All);
                if ( !bh ) {
                    if (m_Selector.m_IdResolving ==
                        SAnnotSelector::eFailUnresolved) {
                        // resolve by Seq-id only
                        NCBI_THROW(CAnnotException, eFindFailed,
                                   "Cannot resolve master id");
                    }
                    continue; // Do not crash - just skip unresolvable IDs
                }

                CRef<CSeq_loc> master_loc_empty(new CSeq_loc);
                master_loc_empty->SetEmpty(
                    const_cast<CSeq_id&>(*idit->first.GetSeqId()));
                
                CHandleRange::TRange idrange =
                    idit->second.GetOverlappingRange();
                const CSeqMap& seqMap = bh.GetSeqMap();
                CSeqMap_CI smit(seqMap.FindResolved(idrange.GetFrom(),
                                                    m_Scope,
                                                    m_Selector.m_ResolveDepth-1,
                                                    CSeqMap::fFindRef));
                while ( smit && smit.GetPosition() < idrange.GetToOpen() ) {
                    _ASSERT(smit.GetType() == CSeqMap::eSeqRef);
                    if ( m_Selector.m_ResolveMethod ==
                        SAnnotSelector::eResolve_TSE &&
                        !m_Scope->GetBioseqHandleFromTSE(smit.GetRefSeqid(),
                                                         bh) ) {
                        smit.Next(false);
                        continue;
                    }
                    found = x_SearchMapped(smit,
                                           *master_loc_empty,
                                           idit->first,
                                           idit->second);
                    deeper = !(found && m_Selector.m_AdaptiveDepth);
                    smit.Next(deeper);
                }
            }
        }
        NON_CONST_ITERATE(CAnnotMappingCollector::TAnnotMappingSet, amit,
            m_MappingCollector->m_AnnotMappingSet) {
            CAnnotObject_Ref annot_ref = amit->first;
            amit->second->Convert(annot_ref,
                m_Selector.m_FeatProduct ? CSeq_loc_Conversion::eProduct :
                CSeq_loc_Conversion::eLocation);
            m_AnnotSet.push_back(annot_ref);
        }
        m_MappingCollector->m_AnnotMappingSet.clear();
        x_Sort();
        m_MappingCollector.reset();
    }
    catch (...) {
        // clear all members - GCC 3.0.4 does not do it
        x_Clear();
        throw;
    }
}


void CAnnot_Collector::x_Initialize(void)
{
    try {
        // Limit must be set, resolving is obsolete
        _ASSERT(m_Selector.m_LimitObjectType != SAnnotSelector::eLimit_None);
        _ASSERT(m_Selector.m_LimitObject);
        _ASSERT(m_Selector.m_ResolveMethod == SAnnotSelector::eResolve_None);
        x_GetTSE_Info();

        x_SearchAll();
        x_Sort();
        m_MappingCollector.reset();
    }
    catch (...) {
        // clear all members - GCC 3.0.4 does not do it
        x_Clear();
        throw;
    }
}


void CAnnot_Collector::x_Sort(void)
{
    _ASSERT(m_MappingCollector->m_AnnotMappingSet.empty());
    switch ( m_Selector.m_SortOrder ) {
    case SAnnotSelector::eSortOrder_Normal:
        sort(m_AnnotSet.begin(), m_AnnotSet.end(), CAnnotObject_Less());
        break;
    case SAnnotSelector::eSortOrder_Reverse:
        sort(m_AnnotSet.begin(), m_AnnotSet.end(), CAnnotObject_LessReverse());
        break;
    default:
        // do nothing
        break;
    }
}


inline
bool
CAnnot_Collector::x_MatchLimitObject(const CAnnotObject_Info& object_info) const
{
    if ( m_Selector.m_LimitObjectType == SAnnotSelector::eLimit_Seq_entry_Info ) {
        const CSeq_entry_Info* info = &object_info.GetSeq_entry_Info();
        _ASSERT(m_MappingCollector->m_LimitObjectInfo);
        _ASSERT(info);
        for ( ;; ) {
            if ( info == m_MappingCollector->m_LimitObjectInfo.GetPointer() ) {
                return true;
            }
            if ( !info->HasParent_Info() ) {
                return false;
            }
            info = &info->GetParentSeq_entry_Info();
        }
    }
    else if ( m_Selector.m_LimitObjectType ==
        SAnnotSelector::eLimit_Seq_annot_Info ) {
        const CSeq_annot_Info* info = &object_info.GetSeq_annot_Info();
        _ASSERT(m_MappingCollector->m_LimitObjectInfo);
        return info == m_MappingCollector->m_LimitObjectInfo.GetPointer();
    }
    return true;
}


inline
bool CAnnot_Collector::x_NeedSNPs(void) const
{
    if ( m_Selector.GetAnnotType() != CSeq_annot::C_Data::e_not_set ) {
        if ( m_Selector.GetAnnotType() != CSeq_annot::C_Data::e_Ftable ) {
            return false;
        }
        if ( !m_Selector.IncludedFeatSubtype(
            CSeqFeatData::eSubtype_variation) ) {
            return false;
        }
        else if ( !m_Selector.IncludedFeatType(CSeqFeatData::e_Imp) ) {
            return false;
        }
    }
    return true;
}


inline
bool CAnnot_Collector::x_MatchLocIndex(const SAnnotObject_Index& index) const
{
    return index.m_AnnotObject_Info->IsAlign()  ||
        m_Selector.m_FeatProduct == (index.m_AnnotLocationIndex == 1);
}


inline
bool CAnnot_Collector::x_MatchRange(const CHandleRange&       hr,
                                    const CRange<TSeqPos>&    range,
                                    const SAnnotObject_Index& index) const
{
    if ( m_Selector.m_OverlapType == SAnnotSelector::eOverlap_Intervals ) {
        if ( index.m_HandleRange ) {
            if ( !hr.IntersectingWith(*index.m_HandleRange) ) {
                return false;
            }
        }
        else {
            if ( !hr.IntersectingWith(range) ) {
                return false;
            }
        }
    }
    if ( !x_MatchLocIndex(index) ) {
        return false;
    }
    return true;
}


void CAnnot_Collector::x_GetTSE_Info(void)
{
    // only one TSE is needed
    _ASSERT(m_TSE_LockSet.empty());
    _ASSERT(m_Selector.m_LimitObjectType != SAnnotSelector::eLimit_None);
    _ASSERT(m_Selector.m_LimitObject);
    
    TTSE_Lock tse_info;
    switch ( m_Selector.m_LimitObjectType ) {
    case SAnnotSelector::eLimit_TSE:
    {
        const CSeq_entry* object = CTypeConverter<CSeq_entry>::
            SafeCast(m_Selector.m_LimitObject.GetPointer());
        CSeq_entry_Handle handle = m_Scope->GetSeq_entryHandle(*object);
        if ( !handle ) {
            NCBI_THROW(CAnnotException, eLimitError,
                       "CAnnot_Collector::x_GetTSE_Info: "
                       "unknown top level Seq-entry");
        }
        const CTSE_Info& info = handle.x_GetInfo().GetTSE_Info();
        tse_info.Reset(&info);

        // normalize TSE -> TSE_Info
        m_Selector.m_LimitObjectType = SAnnotSelector::eLimit_TSE_Info;
        m_Selector.m_LimitObject.Reset(&info);
        m_MappingCollector->m_LimitObjectInfo.Reset(&info);
        break;
    }
    case SAnnotSelector::eLimit_Seq_entry:
    {
        const CSeq_entry* object = CTypeConverter<CSeq_entry>::
            SafeCast(m_Selector.m_LimitObject.GetPointer());
        CSeq_entry_Handle handle = m_Scope->GetSeq_entryHandle(*object);
        if ( !handle ) {
            NCBI_THROW(CAnnotException, eLimitError,
                       "CAnnot_Collector::x_GetTSE_Info: "
                       "unknown Seq-entry");
        }
        const CSeq_entry_Info& info = handle.x_GetInfo();
        tse_info.Reset(&info.GetTSE_Info());

        // normalize Seq_entry -> Seq_entry_Info
        m_Selector.m_LimitObjectType = SAnnotSelector::eLimit_Seq_entry_Info;
        m_Selector.m_LimitObject.Reset(&info);
        m_MappingCollector->m_LimitObjectInfo.Reset(&info);
        break;
    }
    case SAnnotSelector::eLimit_Seq_annot:
    {
        const CSeq_annot* object = CTypeConverter<CSeq_annot>::
            SafeCast(m_Selector.m_LimitObject.GetPointer());
        CSeq_annot_Handle handle = m_Scope->GetSeq_annotHandle(*object);
        if ( !handle ) {
            NCBI_THROW(CAnnotException, eLimitError,
                       "CAnnot_Collector::x_GetTSE_Info: "
                       "unknown Seq-annot");
        }
        const CSeq_annot_Info& info = handle.x_GetInfo();
        tse_info.Reset(&info.GetTSE_Info());

        // normalize Seq_annot -> Seq_annot_Info
        m_Selector.m_LimitObjectType = SAnnotSelector::eLimit_Seq_annot_Info;
        m_Selector.m_LimitObject.Reset(&info);
        m_MappingCollector->m_LimitObjectInfo.Reset(&info);
        break;
    }
    case SAnnotSelector::eLimit_TSE_Info:
    {
        const CTSE_Info* info = CTypeConverter<CTSE_Info>::
            SafeCast(m_Selector.m_LimitObject.GetPointer());
        tse_info.Reset(info);
        m_MappingCollector->m_LimitObjectInfo.Reset(info);
        break;
    }
    case SAnnotSelector::eLimit_Seq_entry_Info:
    {
        const CSeq_entry_Info* info = CTypeConverter<CSeq_entry_Info>::
            SafeCast(m_Selector.m_LimitObject.GetPointer());
        tse_info.Reset(&info->GetTSE_Info());
        m_MappingCollector->m_LimitObjectInfo.Reset(info);
        break;
    }
    case SAnnotSelector::eLimit_Seq_annot_Info:
    {
        const CSeq_annot_Info* info = CTypeConverter<CSeq_annot_Info>::
            SafeCast(m_Selector.m_LimitObject.GetPointer());
        tse_info.Reset(&info->GetTSE_Info());
        m_MappingCollector->m_LimitObjectInfo.Reset(info);
        break;
    }
    default:
        // no limit object -> do nothing
        break;
    }
    _ASSERT(m_MappingCollector->m_LimitObjectInfo);
    _ASSERT(tse_info);
    tse_info->UpdateAnnotIndex();
    //if ( !IsSetAnnotsNames() || x_MatchAnnotName(*tse_info) ) {
        m_TSE_LockSet.insert(tse_info);
    //}
}


static CSeqFeatData::ESubtype s_DefaultAdaptiveTriggers[] = {
    CSeqFeatData::eSubtype_gene,
    CSeqFeatData::eSubtype_cdregion,
    CSeqFeatData::eSubtype_mRNA
};


bool CAnnot_Collector::x_Search(const TTSE_Lock&      tse_lock,
                                const CSeq_id_Handle& id,
                                const CHandleRange&   hr,
                                CSeq_loc_Conversion*  cvt)
{
    bool found = false;
    const CTSE_Info& tse = *tse_lock;

    CTSE_Info::TAnnotReadLockGuard guard(tse.m_AnnotObjsLock);

    if (cvt) {
        cvt->SetSrcId(id);
    }
    // Skip excluded TSEs
    //if ( ExcludedTSE(tse) ) {
    //continue;
    //}

    if ( m_Selector.m_AdaptiveDepth && tse.ContainsSeqid(id) ) {
        const SIdAnnotObjs* objs = tse.x_GetUnnamedIdObjects(id);
        if ( objs ) {
            vector<char> indexes;
            if ( m_Selector.m_AdaptiveTriggers.empty() ) {
                const size_t count =
                    sizeof(s_DefaultAdaptiveTriggers)/
                    sizeof(s_DefaultAdaptiveTriggers[0]);
                for ( int i = count - 1; i >= 0; --i ) {
                    CSeqFeatData::ESubtype subtype =
                        s_DefaultAdaptiveTriggers[i];
                    size_t index = CAnnotType_Index::GetSubtypeIndex(subtype);
                    if ( index ) {
                        indexes.resize(max(indexes.size(), index + 1));
                        indexes[index] = 1;
                    }
                }
            }
            else {
                ITERATE ( SAnnotSelector::TAdaptiveTriggers, it,
                    m_Selector.m_AdaptiveTriggers ) {
                    pair<size_t, size_t> idxs =
                        CAnnotType_Index::GetIndexRange(*it);
                    indexes.resize(max(indexes.size(), idxs.second));
                    for ( size_t i = idxs.first; i < idxs.second; ++i ) {
                        indexes[i] = 1;
                    }
                }
            }
            for ( size_t index = 0; index < indexes.size(); ++index ) {
                if ( !indexes[index] ) {
                    continue;
                }
                if ( index >= objs->m_AnnotSet.size() ) {
                    break;
                }
                if ( !objs->m_AnnotSet[index].empty() ) {
                    found = true;
                    break;
                }
            }
        }
    }
    
    if ( !m_Selector.m_IncludeAnnotsNames.empty() ) {
        // only 'included' annots
        ITERATE ( SAnnotSelector::TAnnotsNames, iter,
            m_Selector.m_IncludeAnnotsNames ) {
            _ASSERT(!m_Selector.ExcludedAnnotName(*iter)); // consistency check
            const SIdAnnotObjs* objs = tse.x_GetIdObjects(*iter, id);
            if ( objs ) {
                x_Search(tse, objs, guard, *iter, id, hr, cvt);
            }
        }
    }
    else {
        // all annots, skipping 'excluded'
        ITERATE (CTSE_Info::TNamedAnnotObjs, iter, tse.m_NamedAnnotObjs) {
            if ( m_Selector.ExcludedAnnotName(iter->first) ) {
                continue;
            }
            const SIdAnnotObjs* objs =
                tse.x_GetIdObjects(iter->second, id);
            if ( objs ) {
                x_Search(tse, objs, guard, iter->first, id, hr, cvt);
            }
        }
    }
    return found;
}


bool CAnnot_Collector::x_AddObjectMapping(CAnnotObject_Ref&    object_ref,
                                          CSeq_loc_Conversion* cvt,
                                          unsigned int         loc_index)
{
    _ASSERT(object_ref.GetAnnotObject_Info().GetMultiIdFlags()
            || cvt->IsPartial()
            || object_ref.IsAlign() );
    object_ref.ResetLocation();
    CRef<CSeq_loc_Conversion_Set>& mapping_set =
        m_MappingCollector->m_AnnotMappingSet[object_ref];
    if ( !mapping_set ) {
        mapping_set.Reset(new CSeq_loc_Conversion_Set(m_Scope));
    }
    CRef<CSeq_loc_Conversion> cvt_copy(new CSeq_loc_Conversion(*cvt));
    mapping_set->Add(*cvt_copy, loc_index);
    return x_GetAnnotCount() >= m_Selector.m_MaxSize;
}


inline
bool CAnnot_Collector::x_AddObject(CAnnotObject_Ref& object_ref)
{
    m_AnnotSet.push_back(object_ref);
    return x_GetAnnotCount() >= m_Selector.m_MaxSize;
}


inline
bool CAnnot_Collector::x_AddObject(CAnnotObject_Ref&    object_ref,
                                   CSeq_loc_Conversion* cvt,
                                   unsigned int         loc_index)
{
    // Always map aligns through conv. set
    return ( cvt && (cvt->IsPartial() || object_ref.IsAlign()) )?
        x_AddObjectMapping(object_ref, cvt, loc_index)
        : x_AddObject(object_ref);
}


void CAnnot_Collector::x_Search(const CTSE_Info&      tse,
                                const SIdAnnotObjs*   objs,
                                CReadLockGuard&       guard,
                                const CAnnotName&     annot_name,
                                const CSeq_id_Handle& id,
                                const CHandleRange&   hr,
                                CSeq_loc_Conversion*  cvt)
{
    if (m_Selector.m_AnnotTypesSet.size() == 0) {
        pair<size_t, size_t> range =
            CAnnotType_Index::GetIndexRange(m_Selector, *objs);
        if ( range.first < range.second ) {
            x_SearchRange(tse, objs, guard, annot_name, id, hr, cvt,
                range.first, range.second);
        }
    }
    else {
        pair<size_t, size_t> range(0, 0);
        bool last_bit = false;
        bool cur_bit;
        for (size_t idx = 0; idx < objs->m_AnnotSet.size(); ++idx) {
            cur_bit = m_Selector.m_AnnotTypesSet[idx];
            if (!last_bit  &&  cur_bit) {
                // open range
                range.first = idx;
            }
            else if (last_bit  &&  !cur_bit) {
                // close and search range
                range.second = idx;
                x_SearchRange(tse, objs, guard, annot_name, id, hr, cvt,
                    range.first, range.second);
            }
            last_bit = cur_bit;
        }
        if (last_bit) {
            // search to the end of annot set
            x_SearchRange(tse, objs, guard, annot_name, id, hr, cvt,
                range.first, objs->m_AnnotSet.size());
        }
    }

    if ( x_NeedSNPs() ) {
        CHandleRange::TRange range = hr.GetOverlappingRange();
        ITERATE ( CTSE_Info::TSNPSet, snp_annot_it, objs->m_SNPSet ) {
            const CSeq_annot_SNP_Info& snp_annot = **snp_annot_it;
            CSeq_annot_SNP_Info::const_iterator snp_it =
                snp_annot.FirstIn(range);
            if ( snp_it != snp_annot.end() ) {
                m_TSE_LockSet.insert(ConstRef(&tse));
                TSeqPos index = snp_it - snp_annot.begin() - 1;
                do {
                    ++index;
                    const SSNP_Info& snp = *snp_it;
                    if ( snp.NoMore(range) ) {
                        break;
                    }
                    if ( snp.NotThis(range) ) {
                        continue;
                    }

                    CAnnotObject_Ref annot_ref(snp_annot, index);
                    annot_ref.SetSNP_Point(snp, cvt);
                    if ( x_AddObject(annot_ref, cvt, 0) ) {
                        return;
                    }
                } while ( ++snp_it != snp_annot.end() );
            }
        }
    }
}


void CAnnot_Collector::x_SearchRange(const CTSE_Info&      tse,
                                     const SIdAnnotObjs*   objs,
                                     CReadLockGuard&       guard,
                                     const CAnnotName&     annot_name,
                                     const CSeq_id_Handle& id,
                                     const CHandleRange&   hr,
                                     CSeq_loc_Conversion*  cvt,
                                     size_t                from_idx,
                                     size_t                to_idx)
{
    _ASSERT(objs);

    CHandleRange::TRange range = hr.GetOverlappingRange();

    m_TSE_LockSet.insert(ConstRef(&tse));

    for ( size_t index = from_idx; index < to_idx; ++index ) {
        size_t start_size = m_AnnotSet.size(); // for rollback
        
        const CTSE_Info::TRangeMap& rmap = objs->m_AnnotSet[index];
        if ( rmap.empty() ) {
            continue;
        }

        for ( CTSE_Info::TRangeMap::const_iterator aoit(rmap.begin(range));
              aoit; ++aoit ) {
            const CAnnotObject_Info& annot_info =
                *aoit->second.m_AnnotObject_Info;

            if ( annot_info.IsChunkStub() ) {
                const CTSE_Chunk_Info& chunk = annot_info.GetChunk_Info();
                if ( chunk.NotLoaded() ) {
                    // New annot objects are to be loaded,
                    // so we'll need to restart scan of current range.

                    // Forget already found object
                    // as they will be found again:
                    m_AnnotSet.resize(start_size);

                    CAnnotName name(annot_name);

                    // Release lock for tse update:
                    guard.Release();
                        
                    // Load the stub:
                    const_cast<CTSE_Chunk_Info&>(chunk).Load();

                    // Acquire the lock again:
                    guard.Guard(tse.m_AnnotObjsLock);

                    // Reget range map pointer as it may change:
                    objs = tse.x_GetIdObjects(name, id);
                    _ASSERT(objs);

                    // Restart this index again:
                    --index;
                    break;
                }
                else {
                    // Skip chunk stub
                    continue;
                }
            }

            if ( annot_info.IsLocs() ) {
                const CSeq_loc& ref_loc = annot_info.GetLocs();

                // Check if the stub has been already processed
                if ( m_AnnotLocsSet.get() ) {
                    CConstRef<CSeq_loc> ploc(&ref_loc);
                    TAnnotLocsSet::const_iterator found =
                        m_AnnotLocsSet->find(ploc);
                    if (found != m_AnnotLocsSet->end()) {
                        continue;
                    }
                }
                else {
                    m_AnnotLocsSet.reset(new TAnnotLocsSet);
                }
                m_AnnotLocsSet->insert(ConstRef(&ref_loc));

                // Search annotations on the referenced location
                const CSeq_id* ref_id = 0;
                ref_loc.CheckId(ref_id);
                _ASSERT(ref_id);
                _ASSERT( ref_loc.IsInt() );
                bool reverse_ref = ref_loc.GetInt().IsSetStrand() ?
                    IsReverse(ref_loc.GetInt().GetStrand()) : false;
                CSeq_id_Handle ref_idh = CSeq_id_Handle::GetHandle(*ref_id);
                if ( m_Selector.m_ResolveMethod ==
                    SAnnotSelector::eResolve_TSE &&
                    !tse.FindBioseq(ref_idh) ) {
                    continue;
                }
                CHandleRange::TRange ref_range = ref_loc.GetTotalRange();

                CRef<CSeq_loc> master_loc_empty(new CSeq_loc);
                master_loc_empty->SetEmpty(
                    const_cast<CSeq_id&>(*id.GetSeqId()));
                CHandleRange::TRange master_range = range & aoit->first;
                TSeqPos start = master_range.GetFrom() - aoit->first.GetFrom();
                TSeqPos end = master_range.GetTo() - aoit->first.GetFrom();

                CHandleRangeMap ref_rmap;
                CHandleRange::TRange search_range(
                    ref_range.GetFrom() + start, ref_range.GetFrom() + end);
                ref_rmap.AddRanges(ref_idh).AddRange(search_range, eNa_strand_unknown);
                bool found = false;
                if (m_Selector.m_NoMapping) {
                    found = x_Search(ref_rmap, 0);
                }
                else {
                    CRef<CSeq_loc_Conversion> locs_cvt(new CSeq_loc_Conversion(
                            *master_loc_empty,
                            id,
                            aoit->first,
                            ref_idh,
                            ref_range.GetFrom(),
                            reverse_ref,
                            m_Scope));
                    if ( cvt ) {
                        locs_cvt->CombineWith(*cvt);
                    }
                    found = x_Search(ref_rmap, &*locs_cvt);
                }
                bool deeper = !(found && m_Selector.m_AdaptiveDepth);
                continue;
            }

            _ASSERT(m_Selector.MatchType(annot_info));

            if ( !x_MatchLimitObject(annot_info) ) {
                continue;
            }
                
            if ( !x_MatchRange(hr, aoit->first, aoit->second) ) {
                continue;
            }

            CAnnotObject_Ref annot_ref(annot_info);
            if (!cvt  &&  annot_info.GetMultiIdFlags()) {
                // Create self-conversion, add to conversion set
                CRef<CSeq_loc_Conversion> cvt_ref
                    (new CSeq_loc_Conversion(id, m_Scope));
                if (x_AddObjectMapping(annot_ref,
                    &*cvt_ref, aoit->second.m_AnnotLocationIndex)) {
                    return;
                }
            }
            else {
                if (cvt  &&  !annot_ref.IsAlign() ) {
                    cvt->Convert
                        (annot_ref,
                         m_Selector.m_FeatProduct ?
                         CSeq_loc_Conversion::eProduct :
                         CSeq_loc_Conversion::eLocation);
                }
                else {
                    annot_ref.SetAnnotObjectRange(aoit->first,
                                                  m_Selector.m_FeatProduct);
                }
                if ( x_AddObject(annot_ref, cvt,
                    aoit->second.m_AnnotLocationIndex) ) {
                    return;
                }
            }
        }
    }
}


bool CAnnot_Collector::x_Search(const CHandleRangeMap& loc,
                                CSeq_loc_Conversion*   cvt)
{
    bool found = false;
    ITERATE ( CHandleRangeMap, idit, loc ) {
        if ( idit->second.Empty() ) {
            continue;
        }
        if ( m_Selector.m_LimitObjectType == SAnnotSelector::eLimit_None ) {
            // any data source
            CConstRef<CScope_Impl::TAnnotRefMap> tse_map =
                m_Scope->GetTSESetWithAnnots(idit->first);
            if (!tse_map) {
                if (m_Selector.m_IdResolving == SAnnotSelector::eFailUnresolved) {
                    NCBI_THROW(CAnnotException, eFindFailed,
                                "Cannot find id synonyms");
                }
                continue;
            }
            ITERATE(CScope_Impl::TTSE_LockMap, tse_it, tse_map->GetData()) {
                ITERATE(TSeq_id_HandleSet, id_it, tse_it->second) {
                    found |= x_Search(tse_it->first,
                        *id_it, idit->second, cvt);
                }
            }
        }
        else {
            // Search in the limit objects
            CBioseq_Handle bh = m_Scope->GetBioseqHandle(idit->first,
                CScope::eGetBioseq_All);
            TSeq_id_HandleSet idh_set;
            if (bh) {
                CSeq_id_Mapper& mapper = *CSeq_id_Mapper::GetInstance();
                ITERATE(CBioseq_Handle::TId, syn_it, bh.GetId()) {
                    if (mapper.HaveReverseMatch(*syn_it)) {
                        mapper.GetReverseMatchingHandles(*syn_it, idh_set);
                    }
                    else {
                        idh_set.insert(*syn_it);
                    }
                }
            }
            else {
                idh_set.insert(idit->first);
            }
            ITERATE(TSeq_id_HandleSet, id_it, idh_set) {
                ITERATE(TTSE_LockSet, tse_it, m_TSE_LockSet) {
                    found = x_Search(*tse_it,
                        *id_it, idit->second, cvt) || found;
                }
            }
        }
    }
    return found;
}


void CAnnot_Collector::x_SearchAll(void)
{
    _ASSERT(m_Selector.m_LimitObjectType != SAnnotSelector::eLimit_None);
    _ASSERT(m_Selector.m_LimitObject);
    _ASSERT(m_MappingCollector->m_LimitObjectInfo);
    if ( m_TSE_LockSet.empty() ) {
        // data source name not matched
        return;
    }
    switch ( m_Selector.m_LimitObjectType ) {
    case SAnnotSelector::eLimit_TSE_Info:
        x_SearchAll(*CTypeConverter<CTSE_Info>::
                    SafeCast(m_MappingCollector->
                    m_LimitObjectInfo.GetPointer()));
        break;
    case SAnnotSelector::eLimit_Seq_entry_Info:
        x_SearchAll(*CTypeConverter<CSeq_entry_Info>::
                    SafeCast(m_MappingCollector->
                    m_LimitObjectInfo.GetPointer()));
        break;
    case SAnnotSelector::eLimit_Seq_annot_Info:
        x_SearchAll(*CTypeConverter<CSeq_annot_Info>::
                    SafeCast(m_MappingCollector->
                    m_LimitObjectInfo.GetPointer()));
        break;
    default:
        // no limit object -> do nothing
        break;
    }
}


void CAnnot_Collector::x_SearchAll(const CSeq_entry_Info& entry_info)
{
    {{
        CConstRef<CBioseq_Base_Info> base = entry_info.m_Contents;
        // Collect all annotations from the entry
        ITERATE( CBioseq_Base_Info::TAnnot, ait, base->GetAnnot() ) {
            x_SearchAll(**ait);
            if ( x_GetAnnotCount() >= m_Selector.m_MaxSize )
                return;
        }
    }}

    if ( entry_info.IsSet() ) {
        CConstRef<CBioseq_set_Info> set(&entry_info.GetSet());
        // Collect annotations from all children
        ITERATE( CBioseq_set_Info::TSeq_set, cit, set->GetSeq_set() ) {
            x_SearchAll(**cit);
            if ( x_GetAnnotCount() >= m_Selector.m_MaxSize )
                return;
        }
    }
}


void CAnnot_Collector::x_SearchAll(const CSeq_annot_Info& annot_info)
{
    // Collect all annotations from the annot
    ITERATE ( SAnnotObjects_Info::TObjectInfos, aoit,
              annot_info.m_ObjectInfos.GetInfos() ) {
        if ( !m_Selector.MatchType(*aoit) ) {
            continue;
        }
        CAnnotObject_Ref annot_ref(*aoit);
        if ( x_AddObject(annot_ref) ) {
            return;
        }
    }

    if ( x_NeedSNPs() && annot_info.x_HasSNP_annot_Info() ) {
        const CSeq_annot_SNP_Info& snp_annot =
            annot_info.x_GetSNP_annot_Info();
        TSeqPos index = 0;
        ITERATE ( CSeq_annot_SNP_Info, snp_it, snp_annot ) {
            const SSNP_Info& snp = *snp_it;
            CAnnotObject_Ref annot_ref(snp_annot, index);
            annot_ref.SetSNP_Point(snp, 0);
            if ( x_AddObject(annot_ref) ) {
                return;
            }
            ++index;
        }
    }
}


bool CAnnot_Collector::x_SearchMapped(const CSeqMap_CI&     seg,
                                      CSeq_loc&             master_loc_empty,
                                      const CSeq_id_Handle& master_id,
                                      const CHandleRange&   master_hr)
{
    CHandleRange::TOpenRange master_seg_range(
        seg.GetPosition(),
        seg.GetEndPosition());
    CHandleRange::TOpenRange ref_seg_range(seg.GetRefPosition(),
                                           seg.GetRefEndPosition());
    bool reversed = seg.GetRefMinusStrand();
    TSignedSeqPos shift;
    if ( !reversed ) {
        shift = ref_seg_range.GetFrom() - master_seg_range.GetFrom();
    }
    else {
        shift = ref_seg_range.GetTo() + master_seg_range.GetFrom();
    }
    CSeq_id_Handle ref_id = seg.GetRefSeqid();
    CHandleRangeMap ref_loc;
    {{ // translate master_loc to ref_loc
        CHandleRange& hr = ref_loc.AddRanges(ref_id);
        ITERATE ( CHandleRange, mlit, master_hr ) {
            CHandleRange::TOpenRange range = master_seg_range & mlit->first;
            if ( !range.Empty() ) {
                ENa_strand strand = mlit->second;
                if ( !reversed ) {
                    range.SetOpen(range.GetFrom() + shift,
                                  range.GetToOpen() + shift);
                }
                else {
                    strand = Reverse(strand);
                    range.Set(shift - range.GetTo(), shift - range.GetFrom());
                }
                hr.AddRange(range, strand);
            }
        }
        if ( hr.Empty() )
            return false;
    }}

    if (m_Selector.m_NoMapping) {
        return x_Search(ref_loc, 0);
    }
    else {
        CRef<CSeq_loc_Conversion> cvt(new CSeq_loc_Conversion(master_loc_empty,
                                                              master_id,
                                                              seg,
                                                              ref_id,
                                                              m_Scope));
        return x_Search(ref_loc, &*cvt);
    }
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.16  2004/07/19 17:41:59  grichenk
* Added strand processing in annot.locs
*
* Revision 1.15  2004/07/19 14:24:00  grichenk
* Simplified and fixed mapping through annot.locs
*
* Revision 1.14  2004/07/15 16:51:30  vasilche
* Fixed segment shift in Seq-annot.locs processing.
*
* Revision 1.13  2004/07/14 16:04:03  grichenk
* Fixed range when searching through annot-locs
*
* Revision 1.12  2004/07/12 15:05:32  grichenk
* Moved seq-id mapper from xobjmgr to seq library
*
* Revision 1.11  2004/06/10 16:21:27  grichenk
* Changed CSeq_id_Mapper singleton type to pointer, GetSeq_id_Mapper
* returns CRef<> which is locked by CObjectManager.
*
* Revision 1.10  2004/06/08 14:24:25  grichenk
* Restricted depth of mapping through annot-locs
*
* Revision 1.9  2004/06/07 17:01:17  grichenk
* Implemented referencing through locs annotations
*
* Revision 1.8  2004/06/03 18:33:48  grichenk
* Modified annot collector to better resolve synonyms
* and matching IDs. Do not add id to scope history when
* collecting annots. Exclude TSEs with bioseqs from data
* source's annot index.
*
* Revision 1.7  2004/05/26 14:29:20  grichenk
* Redesigned CSeq_align_Mapper: preserve non-mapping intervals,
* fixed strands handling, improved performance.
*
* Revision 1.6  2004/05/21 21:42:12  gorelenk
* Added PCH ncbi_pch.hpp
*
* Revision 1.5  2004/05/10 18:26:37  grichenk
* Fixed 'not used' warnings
*
* Revision 1.4  2004/05/03 17:01:03  grichenk
* Invalidate total range before reusing seq-loc.
*
* Revision 1.3  2004/04/13 21:14:27  vasilche
* Fixed wrong order of object deletion causing "tse is locked" error.
*
* Revision 1.2  2004/04/13 15:59:35  grichenk
* Added CScope::GetBioseqHandle() with id resolving flag.
*
* Revision 1.1  2004/04/05 15:54:26  grichenk
* Initial revision
*
*
* ===========================================================================
*/
