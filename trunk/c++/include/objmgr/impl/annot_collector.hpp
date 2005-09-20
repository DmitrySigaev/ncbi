#ifndef ANNOT_COLLECTOR__HPP
#define ANNOT_COLLECTOR__HPP

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
* Author: Aleksey Grichenko, Michael Kimelman, Eugene Vasilchenko
*
* File Description:
*   Annotation collector for annot iterators
*
*/

#include <corelib/ncbiobj.hpp>
#include <corelib/ncbimtx.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/annot_selector.hpp>
#include <objmgr/tse_handle.hpp>
#include <objmgr/impl/heap_scope.hpp>

#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqfeat/Seq_feat.hpp>

#include <set>
#include <vector>
#include <memory>

BEGIN_NCBI_SCOPE

BEGIN_SCOPE(objects)

class CHandleRangeMap;
class CHandleRange;
struct SAnnotObject_Index;
class CSeq_annot_SNP_Info;
struct SSNP_Info;
struct SIdAnnotObjs;
class CSeq_loc_Conversion;
class CSeq_loc_Conversion_Set;
class CSeq_feat_Handle;
class CAnnot_CI;
class CSeqMap_CI;

class NCBI_XOBJMGR_EXPORT CAnnotMapping_Info
{
public:
    enum FMappedFlags {
        fMapped_Partial      = 1<<0,
        fMapped_Product      = 1<<1,
        fMapped_Seq_point    = 1<<2,
        fMapped_Partial_from = 1<<3,
        fMapped_Partial_to   = 1<<4
    };

    enum EMappedObjectType {
        eMappedObjType_not_set,
        eMappedObjType_Seq_loc,
        eMappedObjType_Seq_id,
        eMappedObjType_Seq_feat,
        eMappedObjType_Seq_align,
        eMappedObjType_Seq_loc_Conv_Set
    };

    CAnnotMapping_Info(void);
    CAnnotMapping_Info(Int1 mapped_flags, Int1 mapped_type, Int1 mapped_strand);

    void Reset(void);

    Int1 GetMappedFlags(void) const;
    bool IsMapped(void) const;
    bool IsPartial(void) const;
    bool IsProduct(void) const;
    EMappedObjectType GetMappedObjectType(void) const;

    void SetPartial(bool value);
    void SetProduct(bool product);

    bool IsMappedLocation(void) const;
    bool IsMappedProduct(void) const;
    bool IsMappedPoint(void) const;

    typedef CRange<TSeqPos> TRange;

    TSeqPos GetFrom(void) const;
    TSeqPos GetToOpen(void) const;
    const TRange& GetTotalRange(void) const;
    void SetTotalRange(const TRange& range);

    ENa_strand GetMappedStrand(void) const;
    void SetMappedStrand(ENa_strand strand);

    const CSeq_loc& GetMappedSeq_loc(void) const;
    const CSeq_id& GetMappedSeq_id(void) const;
    const CSeq_feat& GetMappedSeq_feat(void) const;
    const CSeq_align& GetMappedSeq_align(const CSeq_align& orig) const;

    void SetMappedSeq_loc(CSeq_loc& loc);
    void SetMappedSeq_loc(CSeq_loc* loc);
    void SetMappedSeq_id(CSeq_id& id);
    void SetMappedPoint(bool point);
    void SetMappedPartial_from(void);
    void SetMappedPartial_to(void);
    void SetMappedSeq_id(CSeq_id& id, bool point);
    void SetMappedSeq_feat(CSeq_feat& feat);
    void SetMappedSeq_align(CSeq_align* align);
    void SetMappedSeq_align_Cvts(CSeq_loc_Conversion_Set& cvts);

    bool MappedSeq_locNeedsUpdate(void) const;
    void UpdateMappedSeq_loc(CRef<CSeq_loc>& loc) const;
    void UpdateMappedSeq_loc(CRef<CSeq_loc>&      loc,
                             CRef<CSeq_point>&    pnt_ref,
                             CRef<CSeq_interval>& int_ref) const;

    // Copy non-modified members from original feature
    // (all except partial flag and location/product, depending on mode.
    void InitializeMappedSeq_feat(const CSeq_feat& src, CSeq_feat& dst) const;

    void SetAnnotObjectRange(const TRange& range, bool product);
    
    void Swap(CAnnotMapping_Info& info);

private:
    CRef<CObject>           m_MappedObject; // master sequence coordinates
    TRange                  m_TotalRange;
    Int1                    m_MappedFlags; // partial, product
    Int1                    m_MappedObjectType;
    Int1                    m_MappedStrand;
};


class NCBI_XOBJMGR_EXPORT CAnnotObject_Ref
{
public:
    typedef CRange<TSeqPos> TRange;
    typedef Int4            TIndex;

    CAnnotObject_Ref(void);
    CAnnotObject_Ref(const CAnnotObject_Info& object);
    CAnnotObject_Ref(const CSeq_annot_SNP_Info& snp_annot,
                     const SSNP_Info& snp_info);

    bool IsRegular(void) const;
    bool IsSNPFeat(void) const;

    const CSeq_annot_Info& GetSeq_annot_Info(void) const;
    const CSeq_annot_SNP_Info& GetSeq_annot_SNP_Info(void) const;
    TIndex GetAnnotIndex(void) const;

    const CAnnotObject_Info& GetAnnotObject_Info(void) const;
    const SSNP_Info& GetSNP_Info(void) const;

    bool IsFeat(void) const;
    bool IsGraph(void) const;
    bool IsAlign(void) const;
    const CSeq_feat& GetFeat(void) const;
    const CSeq_graph& GetGraph(void) const;
    const CSeq_align& GetAlign(void) const;

    CAnnotMapping_Info& GetMappingInfo(void) const;

    void SetSNP_Point(const SSNP_Info& snp, CSeq_loc_Conversion* cvt);

    void ResetLocation(void);
    bool operator<(const CAnnotObject_Ref& ref) const; // sort by object
    bool operator==(const CAnnotObject_Ref& ref) const; // sort by object
    bool operator!=(const CAnnotObject_Ref& ref) const; // sort by object

    void Swap(CAnnotObject_Ref& ref);

private:
    // states:
    // A. Regular annot:
    //    m_Object == CSeq_annot_Info*
    //    m_AnnotIndex >= 0, index of CAnnotObject_Info within CSeq_annot_Info
    // B. SNP table annot:
    //    m_Object == CSeq_annot_SNP_Info*
    //    m_AnnotIndex < 0, ==  (index of SSNP_Info) + kMin_I4

    CConstRef<CObject>         m_Object;
    TIndex                     m_AnnotIndex;
    mutable CAnnotMapping_Info m_MappingInfo;
};


class CCreatedFeat_Ref : public CObject
{
public:
    CCreatedFeat_Ref(void);
    ~CCreatedFeat_Ref(void);

    void ResetRefs(void);
    void ReleaseRefsTo(CRef<CSeq_feat>*     feat,
                       CRef<CSeq_loc>*      loc,
                       CRef<CSeq_point>*    point,
                       CRef<CSeq_interval>* interval);
    void ResetRefsFrom(CRef<CSeq_feat>*     feat,
                       CRef<CSeq_loc>*      loc,
                       CRef<CSeq_point>*    point,
                       CRef<CSeq_interval>* interval);

    CConstRef<CSeq_feat> MakeOriginalFeature(const CSeq_feat_Handle& feat_h);
    CConstRef<CSeq_loc>  MakeMappedLocation(const CAnnotMapping_Info& map_info);
    CConstRef<CSeq_feat> MakeMappedFeature(const CSeq_feat_Handle& orig_feat,
                                           const CAnnotMapping_Info& map_info,
                                           CSeq_loc& mapped_location);
private:
    CRef<CSeq_feat>      m_CreatedSeq_feat;
    CRef<CSeq_loc>       m_CreatedSeq_loc;
    CRef<CSeq_point>     m_CreatedSeq_point;
    CRef<CSeq_interval>  m_CreatedSeq_interval;
};


class CSeq_annot_Handle;


class CAnnotMappingCollector;


class NCBI_XOBJMGR_EXPORT CAnnot_Collector : public CObject
{
public:
    typedef SAnnotSelector::TAnnotType TAnnotType;
    typedef vector<CAnnotObject_Ref>   TAnnotSet;

    CAnnot_Collector(CScope& scope);
    ~CAnnot_Collector(void);

private:
    CAnnot_Collector(const CAnnot_Collector&);
    CAnnot_Collector& operator= (const CAnnot_Collector&);


    const TAnnotSet& GetAnnotSet(void) const;
    CScope& GetScope(void) const;
    CSeq_annot_Handle GetAnnot(const CAnnotObject_Ref& ref) const;

    const SAnnotSelector& GetSelector(void);
    CScope::EGetBioseqFlag GetGetFlag(void) const;
    bool CanResolveId(const CSeq_id_Handle& idh, const CBioseq_Handle& bh);

    void x_Clear(void);
    void x_Initialize(const SAnnotSelector& selector,
                      const CBioseq_Handle& bioseq,
                      const CRange<TSeqPos>& range,
                      ENa_strand strand);
    void x_Initialize(const SAnnotSelector& selector,
                      const CHandleRangeMap& master_loc);
    void x_Initialize(const SAnnotSelector& selector);
    void x_GetTSE_Info(void);
    bool x_SearchMapped(const CSeqMap_CI&     seg,
                        CSeq_loc&             master_loc_empty,
                        const CSeq_id_Handle& master_id,
                        const CHandleRange&   master_hr);
    bool x_SearchLoc(const CHandleRangeMap& loc,
                     CSeq_loc_Conversion*   cvt,
                     const CTSE_Handle*     using_tse,
                     bool top_level = false);
    bool x_SearchTSE(const CTSE_Handle&    tse,
                     const CSeq_id_Handle& id,
                     const CHandleRange&   hr,
                     CSeq_loc_Conversion*  cvt);
    void x_SearchObjects(const CTSE_Handle&    tse,
                         const SIdAnnotObjs*   objs,
                         CReadLockGuard&       guard,
                         const CAnnotName&     name,
                         const CSeq_id_Handle& id,
                         const CHandleRange&   hr,
                         CSeq_loc_Conversion*  cvt);
    void x_SearchRange(const CTSE_Handle&    tse,
                       const SIdAnnotObjs*   objs,
                       CReadLockGuard&       guard,
                       const CAnnotName&     name,
                       const CSeq_id_Handle& id,
                       const CHandleRange&   hr,
                       CSeq_loc_Conversion*  cvt,
                       size_t                from_idx,
                       size_t                to_idx);
    void x_SearchAll(void);
    void x_SearchAll(const CSeq_entry_Info& entry_info);
    void x_SearchAll(const CSeq_annot_Info& annot_info);
    void x_Sort(void);
    
    void x_AddObjectMapping(CAnnotObject_Ref&    object_ref,
                            CSeq_loc_Conversion* cvt,
                            unsigned int         loc_index);
    void x_AddObject(CAnnotObject_Ref& object_ref);
    void x_AddObject(CAnnotObject_Ref&    object_ref,
                     CSeq_loc_Conversion* cvt,
                     unsigned int         loc_index);

    // Release all locked resources TSE etc
    void x_ReleaseAll(void);

    bool x_NeedSNPs(void) const;
    bool x_MatchLimitObject(const CAnnotObject_Info& annot_info) const;
    bool x_MatchRange(const CHandleRange&       hr,
                      const CRange<TSeqPos>&    range,
                      const SAnnotObject_Index& index) const;
    bool x_MatchLocIndex(const SAnnotObject_Index& index) const;

    bool x_NoMoreObjects(void) const;

    void x_AddPostMappings(void);
    void x_AddTSE(const CTSE_Handle& tse);
    void x_AddAnnot(const CAnnotObject_Ref& ref);

    // Set of processed annot-locs to avoid duplicates
    typedef set< CConstRef<CSeq_loc> >   TAnnotLocsSet;
    typedef map<const CTSE_Info*, CTSE_Handle> TTSE_LockMap;
    typedef map<const CSeq_annot_Info*, CSeq_annot_Handle> TAnnotLockMap;

    const SAnnotSelector*            m_Selector;
    CHeapScope                       m_Scope;
    // TSE set to keep all the TSEs locked
    TTSE_LockMap                     m_TSE_LockMap;
    TAnnotLockMap                    m_AnnotLockMap;
    auto_ptr<CAnnotMappingCollector> m_MappingCollector;
    // Set of all the annotations found
    TAnnotSet                        m_AnnotSet;

    // Temporary objects to be re-used by iterators
    CRef<CCreatedFeat_Ref>  m_CreatedOriginal;
    CRef<CCreatedFeat_Ref>  m_CreatedMapped;
    auto_ptr<TAnnotLocsSet> m_AnnotLocsSet;

    friend class CAnnotTypes_CI;
    friend class CMappedFeat;
    friend class CMappedGraph;
    friend class CAnnot_CI;
};


/////////////////////////////////////////////////////////////////////////////
// CAnnotMapping_Info
/////////////////////////////////////////////////////////////////////////////


inline
CAnnotMapping_Info::CAnnotMapping_Info(void)
    : m_MappedFlags(0),
      m_MappedObjectType(eMappedObjType_not_set),
      m_MappedStrand(eNa_strand_unknown)
{
}


inline
CAnnotMapping_Info::CAnnotMapping_Info(Int1 mapped_flags,
                                       Int1 mapped_type,
                                       Int1 mapped_strand)
    : m_MappedFlags(mapped_flags),
      m_MappedObjectType(mapped_type),
      m_MappedStrand(mapped_strand)
{
}


inline
TSeqPos CAnnotMapping_Info::GetFrom(void) const
{
    return m_TotalRange.GetFrom();
}


inline
TSeqPos CAnnotMapping_Info::GetToOpen(void) const
{
    return m_TotalRange.GetToOpen();
}


inline
const CAnnotMapping_Info::TRange&
CAnnotMapping_Info::GetTotalRange(void) const
{
    return m_TotalRange;
}


inline
void CAnnotMapping_Info::SetTotalRange(const TRange& range)
{
    m_TotalRange = range;
}


inline
bool CAnnotMapping_Info::IsPartial(void) const
{
    return (m_MappedFlags & fMapped_Partial) != 0;
}


inline
bool CAnnotMapping_Info::IsProduct(void) const
{
    return (m_MappedFlags & fMapped_Product) != 0;
}


inline
ENa_strand CAnnotMapping_Info::GetMappedStrand(void) const
{
    return ENa_strand(m_MappedStrand);
}


inline
void CAnnotMapping_Info::SetMappedStrand(ENa_strand strand)
{
    _ASSERT(IsMapped());
    m_MappedStrand = strand;
}


inline
Int1 CAnnotMapping_Info::GetMappedFlags(void) const
{
    return m_MappedFlags;
}


inline
CAnnotMapping_Info::EMappedObjectType
CAnnotMapping_Info::GetMappedObjectType(void) const
{
    return EMappedObjectType(m_MappedObjectType);
}


inline
bool CAnnotMapping_Info::IsMapped(void) const
{
    _ASSERT((GetMappedObjectType() == eMappedObjType_not_set) ==
            !m_MappedObject);
    return GetMappedObjectType() != eMappedObjType_not_set;
}


inline
bool CAnnotMapping_Info::MappedSeq_locNeedsUpdate(void) const
{
    return GetMappedObjectType() == eMappedObjType_Seq_id;
}


inline
bool CAnnotMapping_Info::IsMappedLocation(void) const
{
    return IsMapped() && !IsProduct();
}


inline
bool CAnnotMapping_Info::IsMappedProduct(void) const
{
    return IsMapped() && IsProduct();
}


inline
const CSeq_loc& CAnnotMapping_Info::GetMappedSeq_loc(void) const
{
    if (GetMappedObjectType() == eMappedObjType_Seq_feat) {
        return IsProduct() ? GetMappedSeq_feat().GetProduct()
            : GetMappedSeq_feat().GetLocation();
    }
    _ASSERT(GetMappedObjectType() == eMappedObjType_Seq_loc);
    return static_cast<const CSeq_loc&>(*m_MappedObject);
}


inline
const CSeq_id& CAnnotMapping_Info::GetMappedSeq_id(void) const
{
    _ASSERT(GetMappedObjectType() == eMappedObjType_Seq_id);
    return static_cast<const CSeq_id&>(*m_MappedObject);
}


inline
const CSeq_feat& CAnnotMapping_Info::GetMappedSeq_feat(void) const
{
    _ASSERT(GetMappedObjectType() == eMappedObjType_Seq_feat);
    return static_cast<const CSeq_feat&>(*m_MappedObject);
}


inline
void CAnnotMapping_Info::SetMappedSeq_loc(CSeq_loc* loc)
{
    _ASSERT(!IsMapped());
    m_MappedObject.Reset(loc);
    m_MappedObjectType = loc ?
        eMappedObjType_Seq_loc : eMappedObjType_not_set;
}


inline
void CAnnotMapping_Info::SetMappedSeq_loc(CSeq_loc& loc)
{
    _ASSERT(!IsMapped());
    m_MappedObject.Reset(&loc);
    m_MappedObjectType = eMappedObjType_Seq_loc;
}


inline
void CAnnotMapping_Info::SetMappedSeq_id(CSeq_id& id)
{
    _ASSERT(!IsMapped());
    m_MappedObject.Reset(&id);
    m_MappedObjectType = eMappedObjType_Seq_id;
}


inline
void CAnnotMapping_Info::SetMappedPoint(bool point)
{
    _ASSERT(GetMappedObjectType() == eMappedObjType_Seq_id);
    if ( point ) {
        m_MappedFlags |= fMapped_Seq_point;
    }
    else {
        m_MappedFlags &= ~fMapped_Seq_point;
    }
}


inline
void CAnnotMapping_Info::SetMappedPartial_from(void)
{
    _ASSERT(GetMappedObjectType() == eMappedObjType_Seq_id);
    m_MappedFlags |= fMapped_Partial_from;
}


inline
void CAnnotMapping_Info::SetMappedPartial_to(void)
{
    _ASSERT(GetMappedObjectType() == eMappedObjType_Seq_id);
    m_MappedFlags |= fMapped_Partial_to;
}


inline
bool CAnnotMapping_Info::IsMappedPoint(void) const
{
    _ASSERT(GetMappedObjectType() == eMappedObjType_Seq_id);
    return (m_MappedFlags & fMapped_Seq_point) != 0;
}


inline
void CAnnotMapping_Info::SetMappedSeq_id(CSeq_id& id, bool point)
{
    SetMappedSeq_id(id);
    SetMappedPoint(point);
}


inline
void CAnnotMapping_Info::SetPartial(bool partial)
{
    if ( partial ) {
        m_MappedFlags |= fMapped_Partial;
    }
    else {
        m_MappedFlags &= ~fMapped_Partial;
    }
}


inline
void CAnnotMapping_Info::SetProduct(bool product)
{
    if ( product ) {
        m_MappedFlags |= fMapped_Product;
    }
    else {
        m_MappedFlags &= ~fMapped_Product;
    }
}


inline
void CAnnotMapping_Info::SetAnnotObjectRange(const TRange& range, bool product)
{
    m_TotalRange = range;
    SetProduct(product);
}


inline
void CAnnotMapping_Info::Swap(CAnnotMapping_Info& info)
{
    m_MappedObject.Swap(info.m_MappedObject);
    swap(m_TotalRange, info.m_TotalRange);
    swap(m_MappedFlags, info.m_MappedFlags);
    swap(m_MappedObjectType, info.m_MappedObjectType);
    swap(m_MappedStrand, info.m_MappedStrand);
}


/////////////////////////////////////////////////////////////////////////////
// CAnnotObject_Ref
/////////////////////////////////////////////////////////////////////////////


inline
CAnnotObject_Ref::CAnnotObject_Ref(void)
    : m_AnnotIndex(0)
{
}


inline
CAnnotObject_Ref::TIndex CAnnotObject_Ref::GetAnnotIndex(void) const
{
    return m_AnnotIndex;
}


inline
bool CAnnotObject_Ref::IsRegular(void) const
{
    return m_AnnotIndex >= 0;
}


inline
bool CAnnotObject_Ref::IsSNPFeat(void) const
{
    return m_AnnotIndex < 0;
}


inline
const CSeq_annot_Info& CAnnotObject_Ref::GetSeq_annot_Info(void) const
{
    _ASSERT(IsRegular());
    return reinterpret_cast<const CSeq_annot_Info&>(*m_Object);
}


inline
const CSeq_annot_SNP_Info& CAnnotObject_Ref::GetSeq_annot_SNP_Info(void) const
{
    _ASSERT(IsSNPFeat());
    return reinterpret_cast<const CSeq_annot_SNP_Info&>(*m_Object);
}


inline
bool CAnnotObject_Ref::operator<(const CAnnotObject_Ref& ref) const
{
    return (m_Object < ref.m_Object  ||
            m_Object == ref.m_Object && m_AnnotIndex < ref.m_AnnotIndex);
}


inline
bool CAnnotObject_Ref::operator==(const CAnnotObject_Ref& ref) const
{
    return (m_Object == ref.m_Object  &&
            m_AnnotIndex == ref.m_AnnotIndex);
}


inline
bool CAnnotObject_Ref::operator!=(const CAnnotObject_Ref& ref) const
{
    return (m_Object != ref.m_Object  ||
            m_AnnotIndex != ref.m_AnnotIndex);
}


inline
CAnnotMapping_Info& CAnnotObject_Ref::GetMappingInfo(void) const
{
    return m_MappingInfo;
}


/////////////////////////////////////////////////////////////////////////////
// CAnnot_Collector
/////////////////////////////////////////////////////////////////////////////


inline
const CAnnot_Collector::TAnnotSet&
CAnnot_Collector::GetAnnotSet(void) const
{
    return m_AnnotSet;
}


inline
CScope& CAnnot_Collector::GetScope(void) const
{
    return m_Scope;
}


inline
const SAnnotSelector& CAnnot_Collector::GetSelector(void)
{
    return *m_Selector;
}


inline
CScope::EGetBioseqFlag CAnnot_Collector::GetGetFlag(void) const
{
    switch (m_Selector->m_ResolveMethod) {
    case SAnnotSelector::eResolve_All:
        return CScope::eGetBioseq_All;
    default:
        // Do not load new TSEs
        return CScope::eGetBioseq_Loaded;
    }
}


inline
void CAnnotObject_Ref::Swap(CAnnotObject_Ref& ref)
{
    m_Object.Swap(ref.m_Object);
    swap(m_AnnotIndex, ref.m_AnnotIndex);
    m_MappingInfo.Swap(ref.m_MappingInfo);
}

END_SCOPE(objects)
END_NCBI_SCOPE


BEGIN_STD_SCOPE
inline
void swap(NCBI_NS_NCBI::objects::CAnnotMapping_Info& info1,
		  NCBI_NS_NCBI::objects::CAnnotMapping_Info& info2)
{
    info1.Swap(info2);
}


inline
void swap(NCBI_NS_NCBI::objects::CAnnotObject_Ref& ref1,
		  NCBI_NS_NCBI::objects::CAnnotObject_Ref& ref2)
{
    ref1.Swap(ref2);
}

END_STD_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.33  2005/09/20 15:45:35  vasilche
* Feature editing API.
* Annotation handles remember annotations by index.
*
* Revision 1.32  2005/08/23 17:04:02  vasilche
* Use CAnnotObject_Info pointer instead of annotation index in annot handles.
*
* Revision 1.31  2005/07/19 20:12:23  ucko
* Predeclare CSeqMap_CI for the sake of G++ 4.
*
* Revision 1.30  2005/06/22 14:07:42  vasilche
* Added constructor from CBioseq_Handle, CRange, and strand.
* Moved constructors out of inline section.
*
* Revision 1.29  2005/04/11 17:51:38  grichenk
* Fixed m_CollectSeq_annots initialization.
* Avoid copying SAnnotSelector in CAnnotTypes_CI.
*
* Revision 1.28  2005/04/05 13:42:51  vasilche
* Optimized annotation iterator from CBioseq_Handle.
*
* Revision 1.27  2005/03/28 20:40:44  jcherry
* Added export specifiers
*
* Revision 1.26  2005/03/17 17:52:27  grichenk
* Added flag to SAnnotSelector for skipping multiple SNPs from the same
* seq-annot. Optimized CAnnotCollector::GetAnnot().
*
* Revision 1.25  2005/03/03 18:49:12  didenko
* Added Swap methods and std::swap functions for
* CAnnotMapping_Info and CAnnotObject_Ref classes
*
* Revision 1.24  2005/02/24 19:13:34  grichenk
* Redesigned CMappedFeat not to hold the whole annot collector.
*
* Revision 1.23  2005/02/16 18:51:30  vasilche
* Fixed feature iteration by location in fresh new scope.
*
* Revision 1.22  2005/01/06 16:41:31  grichenk
* Removed deprecated methods
*
* Revision 1.21  2004/12/22 15:56:19  vasilche
* Added CTSE_Handle.
* Renamed x_Search() methods to avoid name clash.
* Allow used TSE linking.
*
* Revision 1.20  2004/10/29 16:29:47  grichenk
* Prepared to remove deprecated methods, added new constructors.
*
* Revision 1.19  2004/10/27 19:29:23  vasilche
* Reset partial flag in CAnnotObject_Ref::ResetLocation().
* Several methods of CAnnotObject_Ref made non-inline.
*
* Revision 1.18  2004/10/26 15:46:59  vasilche
* Fixed processing of partial intervals in feature mapping.
*
* Revision 1.17  2004/10/08 14:18:34  grichenk
* Moved MakeMappedXXXX methods to CAnnotCollector,
* fixed mapped feature initialization bug.
*
* Revision 1.16  2004/09/30 15:03:41  grichenk
* Fixed segments resolving
*
* Revision 1.15  2004/09/27 14:35:46  grichenk
* +Flag for handling unresolved IDs (search/ignore/fail)
* +Selector method for external annotations search
*
* Revision 1.14  2004/08/17 14:31:46  grichenk
* operators <, == and != made inline
*
* Revision 1.13  2004/08/17 03:28:20  grichenk
* Added operator !=()
*
* Revision 1.12  2004/08/16 18:00:40  grichenk
* Added detection of circular locations, improved annotation
* indexing by strand.
*
* Revision 1.11  2004/08/04 14:53:26  vasilche
* Revamped object manager:
* 1. Changed TSE locking scheme
* 2. TSE cache is maintained by CDataSource.
* 3. CObjectManager::GetInstance() doesn't hold CRef<> on the object manager.
* 4. Fixed processing of split data.
*
* Revision 1.10  2004/07/19 14:24:00  grichenk
* Simplified and fixed mapping through annot.locs
*
* Revision 1.9  2004/06/16 11:38:22  dicuccio
* Replaced forward declaration for CReadGuard with #include for
* <corelib/ncbimtx.hpp>
*
* Revision 1.8  2004/06/07 17:01:17  grichenk
* Implemented referencing through locs annotations
*
* Revision 1.7  2004/06/03 18:33:48  grichenk
* Modified annot collector to better resolve synonyms
* and matching IDs. Do not add id to scope history when
* collecting annots. Exclude TSEs with bioseqs from data
* source's annot index.
*
* Revision 1.6  2004/05/26 14:29:20  grichenk
* Redesigned CSeq_align_Mapper: preserve non-mapping intervals,
* fixed strands handling, improved performance.
*
* Revision 1.5  2004/05/11 17:45:24  grichenk
* Removed declarations for SetMappedIndex() and SetMappedSeq_align_Mapper()
*
* Revision 1.4  2004/05/10 18:26:37  grichenk
* Fixed 'not used' warnings
*
* Revision 1.3  2004/04/13 21:14:27  vasilche
* Fixed wrong order of object deletion causing "tse is locked" error.
*
* Revision 1.2  2004/04/07 13:20:17  grichenk
* Moved more data from iterators to CAnnot_Collector
*
* Revision 1.1  2004/04/05 15:54:25  grichenk
* Initial revision
*
*
* ===========================================================================
*/

#endif  // ANNOT_COLLECTOR__HPP
