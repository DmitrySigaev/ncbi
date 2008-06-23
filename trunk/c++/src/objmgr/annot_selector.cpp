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
*
*/

#include <ncbi_pch.hpp>
#include <objmgr/annot_selector.hpp>
#include <objmgr/impl/tse_info.hpp>
#include <objmgr/impl/seq_entry_info.hpp>
#include <objmgr/impl/seq_annot_info.hpp>
#include <objmgr/impl/annot_type_index.hpp>

#include <objmgr/bioseq_handle.hpp>
#include <objmgr/seq_entry_handle.hpp>
#include <objmgr/seq_annot_handle.hpp>

#include <objects/seqset/Seq_entry.hpp>
#include <objects/seq/Seq_annot.hpp>

#include <algorithm>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

////////////////////////////////////////////////////////////////////
//
//  SAnnotSelector
//

SAnnotSelector::SAnnotSelector(TAnnotType annot,
                               TFeatType  feat,
                               bool       feat_product)
    : SAnnotTypeSelector(annot),
      m_FeatProduct(feat_product),
      m_ResolveDepth(kMax_Int),
      m_OverlapType(eOverlap_Intervals),
      m_ResolveMethod(eResolve_TSE),
      m_SortOrder(eSortOrder_Normal),
      m_LimitObjectType(eLimit_None),
      m_UnresolvedFlag(eIgnoreUnresolved),
      m_MaxSize(kMax_UInt),
      m_NoMapping(false),
      m_AdaptiveDepthFlags(kAdaptive_None),
      m_ExactDepth(false),
      m_ExcludeExternal(false),
      m_CollectSeq_annots(false),
      m_CollectTypes(false),
      m_CollectNames(false)
{
    if ( feat != CSeqFeatData::e_not_set ) {
        SetFeatType(feat);
    }
}


SAnnotSelector::SAnnotSelector(TFeatType feat,
                               bool      feat_product)
    : SAnnotTypeSelector(feat),
      m_FeatProduct(feat_product),
      m_ResolveDepth(kMax_Int),
      m_OverlapType(eOverlap_Intervals),
      m_ResolveMethod(eResolve_TSE),
      m_SortOrder(eSortOrder_Normal),
      m_LimitObjectType(eLimit_None),
      m_UnresolvedFlag(eIgnoreUnresolved),
      m_MaxSize(kMax_UInt),
      m_NoMapping(false),
      m_AdaptiveDepthFlags(kAdaptive_None),
      m_ExactDepth(false),
      m_ExcludeExternal(false),
      m_CollectSeq_annots(false),
      m_CollectTypes(false),
      m_CollectNames(false)
{
}


SAnnotSelector::SAnnotSelector(TFeatSubtype feat_subtype)
    : SAnnotTypeSelector(feat_subtype),
      m_FeatProduct(false),
      m_ResolveDepth(kMax_Int),
      m_OverlapType(eOverlap_Intervals),
      m_ResolveMethod(eResolve_TSE),
      m_SortOrder(eSortOrder_Normal),
      m_LimitObjectType(eLimit_None),
      m_UnresolvedFlag(eIgnoreUnresolved),
      m_MaxSize(kMax_UInt),
      m_NoMapping(false),
      m_AdaptiveDepthFlags(kAdaptive_None),
      m_ExactDepth(false),
      m_ExcludeExternal(false),
      m_CollectSeq_annots(false),
      m_CollectTypes(false),
      m_CollectNames(false)
{
}


SAnnotSelector::SAnnotSelector(const SAnnotSelector& sel)
{
    *this = sel;
}


SAnnotSelector& SAnnotSelector::operator=(const SAnnotSelector& sel)
{
    if ( this != &sel ) {
        static_cast<SAnnotTypeSelector&>(*this) = sel;
        m_FeatProduct = sel.m_FeatProduct;
        m_ResolveDepth = sel.m_ResolveDepth;
        m_OverlapType = sel.m_OverlapType;
        m_ResolveMethod = sel.m_ResolveMethod;
        m_SortOrder = sel.m_SortOrder;
        m_LimitObjectType = sel.m_LimitObjectType;
        m_UnresolvedFlag = sel.m_UnresolvedFlag;
        m_LimitObject = sel.m_LimitObject;
        m_LimitTSE = sel.m_LimitTSE;
        m_MaxSize = sel.m_MaxSize;
        m_IncludeAnnotsNames = sel.m_IncludeAnnotsNames;
        m_ExcludeAnnotsNames = sel.m_ExcludeAnnotsNames;
        m_NoMapping = sel.m_NoMapping;
        m_AdaptiveDepthFlags = sel.m_AdaptiveDepthFlags;
        m_ExactDepth = sel.m_ExactDepth;
        m_ExcludeExternal = sel.m_ExcludeExternal;
        m_CollectSeq_annots = sel.m_CollectSeq_annots;
        m_CollectTypes = sel.m_CollectTypes;
        m_CollectNames = sel.m_CollectNames;
        m_AdaptiveTriggers = sel.m_AdaptiveTriggers;
        m_ExcludedTSE = sel.m_ExcludedTSE;
        m_AnnotTypesBitset = sel.m_AnnotTypesBitset;
    }
    return *this;
}


SAnnotSelector::~SAnnotSelector(void)
{
}


SAnnotSelector& SAnnotSelector::SetLimitNone(void)
{
    m_LimitObjectType = eLimit_None;
    m_LimitObject.Reset();
    m_LimitTSE.Reset();
    return *this;
}


SAnnotSelector&
SAnnotSelector::SetLimitTSE(const CTSE_Handle& limit)
{
    if ( !limit )
        return SetLimitNone();
    
    m_LimitObjectType = eLimit_TSE_Info;
    m_LimitObject.Reset(&limit.x_GetTSE_Info());
    m_LimitTSE = limit;
    return *this;
}


SAnnotSelector&
SAnnotSelector::SetLimitTSE(const CSeq_entry_Handle& limit)
{
    return SetLimitTSE(limit.GetTSE_Handle());
}


SAnnotSelector&
SAnnotSelector::SetLimitSeqEntry(const CSeq_entry_Handle& limit)
{
    if ( !limit )
        return SetLimitNone();
    
    m_LimitObjectType = eLimit_Seq_entry_Info;
    m_LimitObject.Reset(&limit.x_GetInfo());
    m_LimitTSE = limit.GetTSE_Handle();
    return *this;
}


SAnnotSelector&
SAnnotSelector::SetLimitSeqAnnot(const CSeq_annot_Handle& limit)
{
    if ( !limit )
        return SetLimitNone();
    
    m_LimitObjectType = eLimit_Seq_annot_Info;
    m_LimitObject.Reset(&limit.x_GetInfo());
    m_LimitTSE = limit.GetTSE_Handle();
    return *this;
}


SAnnotSelector& SAnnotSelector::SetSearchExternal(const CTSE_Handle& tse)
{
    _ASSERT( tse );
    SetResolveTSE();
    SetLimitTSE(tse);
    SetSearchUnresolved();
    return *this;
}


SAnnotSelector& SAnnotSelector::SetSearchExternal(const CSeq_entry_Handle& se)
{
    _ASSERT( se );
    return SetSearchExternal(se.GetTSE_Handle());
}


SAnnotSelector& SAnnotSelector::SetSearchExternal(const CBioseq_Handle& seq)
{
    _ASSERT( seq );
    return SetSearchExternal(seq.GetTSE_Handle());
}


bool SAnnotSelector::x_Has(const TAnnotsNames& names, const CAnnotName& name)
{
    return find(names.begin(), names.end(), name) != names.end();
}


void SAnnotSelector::x_Add(TAnnotsNames& names, const CAnnotName& name)
{
    if ( !x_Has(names, name) ) {
        names.push_back(name);
    }
}


void SAnnotSelector::x_Del(TAnnotsNames& names, const CAnnotName& name)
{
    NON_CONST_ITERATE( TAnnotsNames, it, names ) {
        if ( *it == name ) {
            names.erase(it);
            return;
        }
    }
}


bool SAnnotSelector::IncludedAnnotName(const CAnnotName& name) const
{
    return x_Has(m_IncludeAnnotsNames, name);
}


bool SAnnotSelector::ExcludedAnnotName(const CAnnotName& name) const
{
    return x_Has(m_ExcludeAnnotsNames, name);
}


SAnnotSelector& SAnnotSelector::ResetAnnotsNames(void)
{
    m_IncludeAnnotsNames.clear();
    m_ExcludeAnnotsNames.clear();
    return *this;
}

namespace {
    void vector_erase(vector<CAnnotName>& v, const CAnnotName& name)
    {
        v.erase(remove(v.begin(), v.end(), name), v.end());
    }
}


SAnnotSelector& SAnnotSelector::ResetNamedAnnots(const CAnnotName& name)
{
    vector_erase(m_IncludeAnnotsNames, name);
    vector_erase(m_ExcludeAnnotsNames, name);
    return *this;
}


SAnnotSelector& SAnnotSelector::ResetNamedAnnots(const char* name)
{
    return ResetNamedAnnots(CAnnotName(name));
}


SAnnotSelector& SAnnotSelector::ResetUnnamedAnnots(void)
{
    return ResetNamedAnnots(CAnnotName());
}


SAnnotSelector& SAnnotSelector::AddNamedAnnots(const CAnnotName& name)
{
    x_Add(m_IncludeAnnotsNames, name);
    x_Del(m_ExcludeAnnotsNames, name);
    return *this;
}


SAnnotSelector& SAnnotSelector::AddNamedAnnots(const char* name)
{
    return AddNamedAnnots(CAnnotName(name));
}


SAnnotSelector& SAnnotSelector::AddUnnamedAnnots(void)
{
    return AddNamedAnnots(CAnnotName());
}


SAnnotSelector& SAnnotSelector::ExcludeNamedAnnots(const CAnnotName& name)
{
    x_Add(m_ExcludeAnnotsNames, name);
    x_Del(m_IncludeAnnotsNames, name);
    return *this;
}


SAnnotSelector& SAnnotSelector::ExcludeNamedAnnots(const char* name)
{
    return ExcludeNamedAnnots(CAnnotName(name));
}


SAnnotSelector& SAnnotSelector::ExcludeUnnamedAnnots(void)
{
    return ExcludeNamedAnnots(CAnnotName());
}


SAnnotSelector& SAnnotSelector::SetAllNamedAnnots(void)
{
    ResetAnnotsNames();
    ExcludeUnnamedAnnots();
    return *this;
}


SAnnotSelector& SAnnotSelector::SetDataSource(const string& source)
{
    if ( source.empty() ) {
        AddUnnamedAnnots();
    }
    return AddNamedAnnots(source);
}


SAnnotSelector&
SAnnotSelector::SetAdaptiveTrigger(const SAnnotTypeSelector& sel)
{
    ITERATE ( TAdaptiveTriggers, it, m_AdaptiveTriggers ) {
        if ( *it == sel ) {
            return *this;
        }
    }
    m_AdaptiveTriggers.push_back(sel);
    return *this;
}


SAnnotSelector&
SAnnotSelector::ExcludeTSE(const CTSE_Handle& tse)
{
    if ( !ExcludedTSE(tse) ) {
        m_ExcludedTSE.push_back(tse);
    }
    return *this;
}


SAnnotSelector&
SAnnotSelector::ExcludeTSE(const CSeq_entry_Handle& tse)
{
    return ExcludeTSE(tse.GetTSE_Handle());
}


SAnnotSelector&
SAnnotSelector::ResetExcludedTSE(void)
{
    m_ExcludedTSE.clear();
    return *this;
}


bool SAnnotSelector::ExcludedTSE(const CTSE_Handle& tse) const
{
    return find(m_ExcludedTSE.begin(), m_ExcludedTSE.end(), tse)
        != m_ExcludedTSE.end();
}


bool SAnnotSelector::ExcludedTSE(const CSeq_entry_Handle& tse) const
{
    return ExcludedTSE(tse.GetTSE_Handle());
}


void SAnnotSelector::x_ClearAnnotTypesSet(void)
{
    m_AnnotTypesBitset.reset();
}


void SAnnotSelector::x_InitializeAnnotTypesSet(bool default_value)
{
    if ( m_AnnotTypesBitset.any() ) {
        return;
    }
    if ( default_value ) {
        m_AnnotTypesBitset.set();
    }
    else {
        m_AnnotTypesBitset.reset();
    }
    // Do not try to use flags from an uninitialized selector
    if (GetAnnotType() != CSeq_annot::C_Data::e_not_set) {
        // Copy current state to the set
        CAnnotType_Index::TIndexRange range =
            CAnnotType_Index::GetIndexRange(*this);
        for (size_t i = range.first; i < range.second; ++i) {
            m_AnnotTypesBitset.set(i);
        }
    }
}


SAnnotSelector& SAnnotSelector::IncludeAnnotType(TAnnotType type)
{
    if (GetAnnotType() == CSeq_annot::C_Data::e_not_set) {
        SAnnotTypeSelector::SetAnnotType(type);
    }
    else if ( !IncludedAnnotType(type) ) {
        x_InitializeAnnotTypesSet(false);
        CAnnotType_Index::TIndexRange range =
            CAnnotType_Index::GetAnnotTypeRange(type);
        for (size_t i = range.first; i < range.second; ++i) {
            m_AnnotTypesBitset.set(i);
        }
    }
    return *this;
}


SAnnotSelector& SAnnotSelector::ExcludeAnnotType(TAnnotType type)
{
    if (GetAnnotType() == CSeq_annot::C_Data::e_not_set
        ||  IncludedAnnotType(type)) {
        x_InitializeAnnotTypesSet(true);
        CAnnotType_Index::TIndexRange range =
            CAnnotType_Index::GetAnnotTypeRange(type);
        for (size_t i = range.first; i < range.second; ++i) {
            m_AnnotTypesBitset.reset(i);
        }
    }
    return *this;
}


SAnnotSelector& SAnnotSelector::IncludeFeatType(TFeatType type)
{
    if (GetAnnotType() == CSeq_annot::C_Data::e_not_set) {
        SAnnotTypeSelector::SetFeatType(type);
    }
    else if (!IncludedFeatType(type)) {
        x_InitializeAnnotTypesSet(false);
        ForceAnnotType(CSeq_annot::C_Data::e_Ftable);
        CAnnotType_Index::TIndexRange range =
            CAnnotType_Index::GetFeatTypeRange(type);
        for (size_t i = range.first; i < range.second; ++i) {
            m_AnnotTypesBitset.set(i);
        }
    }
    return *this;
}


SAnnotSelector& SAnnotSelector::ExcludeFeatType(TFeatType type)
{
    if (GetAnnotType() == CSeq_annot::C_Data::e_not_set
        ||  IncludedFeatType(type)) {
        x_InitializeAnnotTypesSet(true);
        ForceAnnotType(CSeq_annot::C_Data::e_Ftable);
        CAnnotType_Index::TIndexRange range =
            CAnnotType_Index::GetFeatTypeRange(type);
        for (size_t i = range.first; i < range.second; ++i) {
            m_AnnotTypesBitset.test(i);
        }
    }
    return *this;
}


SAnnotSelector& SAnnotSelector::IncludeFeatSubtype(TFeatSubtype subtype)
{
    if (GetAnnotType() == CSeq_annot::C_Data::e_not_set) {
        SAnnotTypeSelector::SetFeatSubtype(subtype);
    }
    else if ( !IncludedFeatSubtype(subtype) ) {
        x_InitializeAnnotTypesSet(false);
        ForceAnnotType(CSeq_annot::C_Data::e_Ftable);
        m_AnnotTypesBitset.set(CAnnotType_Index::GetSubtypeIndex(subtype));
    }
    return *this;
}


SAnnotSelector& SAnnotSelector::ExcludeFeatSubtype(TFeatSubtype subtype)
{
    if (GetAnnotType() == CSeq_annot::C_Data::e_not_set
        ||  IncludedFeatSubtype(subtype)) {
        x_InitializeAnnotTypesSet(true);
        ForceAnnotType(CSeq_annot::C_Data::e_Ftable);
        m_AnnotTypesBitset.reset(CAnnotType_Index::GetSubtypeIndex(subtype));
    }
    return *this;
}


SAnnotSelector& SAnnotSelector::ForceAnnotType(TAnnotType type)
{
    if ( type == CSeq_annot::C_Data::e_Ftable ) {
        // Remove all non-feature types from the list
        if ( m_AnnotTypesBitset.any() ) {
            CAnnotType_Index::TIndexRange range =
                CAnnotType_Index::GetAnnotTypeRange(type);
            for (size_t i = 0; i < range.first; ++i) {
                m_AnnotTypesBitset.reset(i);
            }
            for (size_t i = range.second; i < m_AnnotTypesBitset.size(); ++i) {
                m_AnnotTypesBitset.reset(i);
            }
        }
        else {
            SetAnnotType(type);
        }
    }
    else if ( type != CSeq_annot::C_Data::e_not_set ) {
        // Force the type
        SetAnnotType(type);
    }
    return *this;
}


bool SAnnotSelector::IncludedAnnotType(TAnnotType type) const
{
    if ( m_AnnotTypesBitset.any() ) {
        CAnnotType_Index::TIndexRange range =
            CAnnotType_Index::GetAnnotTypeRange(type);
        for (size_t i = range.first; i < range.second; ++i) {
            if ( m_AnnotTypesBitset.test(i) ) {
                return true;
            }
        }
        return false;
    }
    return GetAnnotType() == CSeq_annot::C_Data::e_not_set
        || GetAnnotType() == type;
}


bool SAnnotSelector::IncludedFeatType(TFeatType type) const
{
    if ( m_AnnotTypesBitset.any() ) {
        CAnnotType_Index::TIndexRange range =
            CAnnotType_Index::GetFeatTypeRange(type);
        for (size_t i = range.first; i < range.second; ++i) {
            if ( m_AnnotTypesBitset.test(i) ) {
                return true;
            }
        }
        return false;
    }
    // Make sure features are selected
    return GetAnnotType() == CSeq_annot::C_Data::e_not_set ||
        GetAnnotType() == CSeq_annot::C_Data::e_Ftable &&
        (GetFeatType() == CSeqFeatData::e_not_set ||
         GetFeatType() == type);
}


bool SAnnotSelector::IncludedFeatSubtype(TFeatSubtype subtype) const
{
    if ( m_AnnotTypesBitset.any() ) {
        return m_AnnotTypesBitset
            .test(CAnnotType_Index::GetSubtypeIndex(subtype));
    }
    // Make sure features are selected
    return GetAnnotType() == CSeq_annot::C_Data::e_not_set ||
        GetAnnotType() == CSeq_annot::C_Data::e_Ftable &&
        (GetFeatType() == CSeqFeatData::e_not_set ||
         subtype == CSeqFeatData::eSubtype_any ||
         GetFeatSubtype() == subtype ||
         (GetFeatSubtype() == CSeqFeatData::eSubtype_any &&
          GetFeatType() == CSeqFeatData::GetTypeFromSubtype(subtype)));
}


bool SAnnotSelector::MatchType(const CAnnotObject_Info& annot_info) const
{
    if (annot_info.GetFeatSubtype() != CSeqFeatData::eSubtype_any) {
        return IncludedFeatSubtype(annot_info.GetFeatSubtype());
    }
    if (annot_info.GetFeatType() != CSeqFeatData::e_not_set) {
        return IncludedFeatType(annot_info.GetFeatType());
    }
    return IncludedAnnotType(annot_info.GetAnnotType());
}


void SAnnotSelector::CheckLimitObjectType(void) const
{
    if ( !m_LimitObject ) {
        m_LimitObjectType = eLimit_None;
    }
}


END_SCOPE(objects)
END_NCBI_SCOPE
