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

#include <objects/seqloc/Seq_loc.hpp>

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
// class CSeq_align_Mapper;

class NCBI_XOBJMGR_EXPORT CAnnotObject_Ref
{
public:
    typedef CRange<TSeqPos> TRange;

    CAnnotObject_Ref(void);
    CAnnotObject_Ref(const CAnnotObject_Info& object);
    CAnnotObject_Ref(const CSeq_annot_SNP_Info& snp_info, TSeqPos index);
    ~CAnnotObject_Ref(void);

    enum EObjectType {
        eType_null,
        eType_Seq_annot_Info,
        eType_Seq_annot_SNP_Info
    };

    enum FMappedFlags {
        fMapped_Partial   = 1<<0,
        fMapped_Product   = 1<<1,
        fMapped_Seq_point = 1<<2
    };

    enum EMappedObjectType {
        eMappedObjType_not_set,
        eMappedObjType_Seq_loc,
        eMappedObjType_Seq_id,
        eMappedObjType_Seq_align,
        eMappedObjType_Seq_loc_Conv_Set
    };

    EObjectType GetObjectType(void) const;

    const CSeq_annot_Info& GetSeq_annot_Info(void) const;
    const CSeq_annot_SNP_Info& GetSeq_annot_SNP_Info(void) const;

    const CAnnotObject_Info& GetAnnotObject_Info(void) const;
    const SSNP_Info& GetSNP_Info(void) const;

    EMappedObjectType GetMappedObjectType(void) const;

    TSeqPos GetFrom(void) const;
    TSeqPos GetToOpen(void) const;
    const TRange& GetTotalRange(void) const;

    bool IsPartial(void) const;
    bool IsProduct(void) const;
    bool IsMappedPoint(void) const;
    void SetProduct(bool product);

    ENa_strand GetMappedStrand(void) const;

    bool IsFeat(void) const;
    bool IsSNPFeat(void) const;
    bool IsGraph(void) const;
    bool IsAlign(void) const;
    const CSeq_feat& GetFeat(void) const;
    const CSeq_graph& GetGraph(void) const;
    const CSeq_align& GetAlign(void) const;

    bool IsMapped(void) const;
    bool IsMappedLocation(void) const;
    bool IsMappedProduct(void) const;

    bool MappedSeq_locNeedsUpdate(void) const;

    const CSeq_loc& GetMappedSeq_loc(void) const;
    const CSeq_id& GetMappedSeq_id(void) const;
    const CSeq_align& GetMappedSeq_align(void) const;

    unsigned int GetAnnotObjectIndex(void) const;

    void UpdateMappedSeq_loc(CRef<CSeq_loc>& loc) const;
    void UpdateMappedSeq_loc(CRef<CSeq_loc>&      loc,
                             CRef<CSeq_point>&    pnt_ref,
                             CRef<CSeq_interval>& int_ref) const;

    void SetAnnotObjectRange(const TRange& range, bool product);
    void SetSNP_Point(const SSNP_Info& snp, CSeq_loc_Conversion* cvt);

    void SetPartial(bool value);
    void SetMappedSeq_loc(CSeq_loc& loc);
    void SetMappedSeq_loc(CSeq_loc* loc);
    void SetMappedSeq_id(CSeq_id& id);
    void SetMappedPoint(bool point);
    void SetMappedSeq_id(CSeq_id& id, bool point);
    void SetMappedSeq_align(CSeq_align* align);
    void SetMappedSeq_align_Cvts(CSeq_loc_Conversion_Set& cvts);
    void SetTotalRange(const TRange& range);
    void SetMappedStrand(ENa_strand strand);

    void ResetLocation(void);
    bool operator<(const CAnnotObject_Ref& ref) const; // sort by object

private:
    CConstRef<CObject>      m_Object;
    CRef<CObject>           m_MappedObject; // master sequence coordinates
    TRange                  m_TotalRange;
    unsigned int            m_AnnotObject_Index;
    Int1                    m_ObjectType; // EObjectType
    Int1                    m_MappedFlags; // partial, product
    Int1                    m_MappedObjectType;
    Int1                    m_MappedStrand;
};


class CSeq_annot_Handle;


class CAnnotMappingCollector;


class CAnnot_Collector : public CObject
{
public:
    CAnnot_Collector(const SAnnotSelector& selector,
                        CScope&               scope);
    ~CAnnot_Collector(void);

private:
    CAnnot_Collector(const CAnnot_Collector&);
    CAnnot_Collector& operator= (const CAnnot_Collector&);

    typedef CConstRef<CTSE_Info> TTSE_Lock;
    typedef set<TTSE_Lock>       TTSE_LockSet;
    typedef vector<CAnnotObject_Ref> TAnnotSet;

    const TAnnotSet& GetAnnotSet(void) const;
    CScope& GetScope(void) const;

    SAnnotSelector& GetSelector(void);

    void x_Clear(void);
    void x_Initialize(const CBioseq_Handle& bioseq,
                      TSeqPos start, TSeqPos stop);
    void x_Initialize(const CHandleRangeMap& master_loc);
    void x_Initialize(void);
    void x_GetTSE_Info(void);
    bool x_SearchMapped(const CSeqMap_CI&     seg,
                        CSeq_loc&             master_loc_empty,
                        const CSeq_id_Handle& master_id,
                        const CHandleRange&   master_hr,
                        TSeqPos               master_shift);
    bool x_Search(const CHandleRangeMap& loc,
                  CSeq_loc_Conversion*   cvt);
/*
    bool x_Search(const CSeq_id_Handle& id,
                  const CBioseq_Handle& bh,
                  const CHandleRange& hr,
                  CSeq_loc_Conversion* cvt);
*/
    bool x_Search(const TTSE_Lock&      tse_lock,
                  const CSeq_id_Handle& id,
                  const CHandleRange&   hr,
                  CSeq_loc_Conversion*  cvt);
    void x_Search(const CTSE_Info&      tse_info,
                  const SIdAnnotObjs*   objs,
                  CReadLockGuard&       guard,
                  const CAnnotName&     name,
                  const CSeq_id_Handle& id,
                  const CHandleRange&   hr,
                  CSeq_loc_Conversion*  cvt);
    void x_SearchRange(const CTSE_Info&      tse_info,
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
    
    bool x_AddObjectMapping(CAnnotObject_Ref&    object_ref,
                            CSeq_loc_Conversion* cvt,
                            unsigned int         loc_index);
    bool x_AddObject(CAnnotObject_Ref& object_ref);
    bool x_AddObject(CAnnotObject_Ref&    object_ref,
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

    size_t x_GetAnnotCount(void) const;

    // Set of processed annot-locs to avoid duplicates
    typedef set< CConstRef<CSeq_loc> > TAnnotLocsSet;

    SAnnotSelector                   m_Selector;
    CHeapScope                       m_Scope;
    // TSE set to keep all the TSEs locked
    TTSE_LockSet                     m_TSE_LockSet;
    auto_ptr<CAnnotMappingCollector> m_MappingCollector;
    // Set of all the annotations found
    TAnnotSet                        m_AnnotSet;

    // Temporary objects to be re-used by iterators
    CRef<CSeq_feat>      m_CreatedOriginalSeq_feat;
    CRef<CSeq_point>     m_CreatedOriginalSeq_point;
    CRef<CSeq_interval>  m_CreatedOriginalSeq_interval;
    CRef<CSeq_feat>      m_CreatedMappedSeq_feat;
    CRef<CSeq_loc>       m_CreatedMappedSeq_loc;
    CRef<CSeq_point>     m_CreatedMappedSeq_point;
    CRef<CSeq_interval>  m_CreatedMappedSeq_interval;
    auto_ptr<TAnnotLocsSet> m_AnnotLocsSet;

    friend class CAnnotTypes_CI;
    friend class CMappedFeat;
    friend class CMappedGraph;
};


/////////////////////////////////////////////////////////////////////////////
// CAnnotObject_Ref
/////////////////////////////////////////////////////////////////////////////

inline
CAnnotObject_Ref::CAnnotObject_Ref(void)
    : m_AnnotObject_Index(0),
      m_ObjectType(eType_null),
      m_MappedFlags(0),
      m_MappedObjectType(CSeq_loc::e_not_set),
      m_MappedStrand(eNa_strand_unknown)
{
}


inline
CAnnotObject_Ref::~CAnnotObject_Ref(void)
{
}


inline
CAnnotObject_Ref::EObjectType CAnnotObject_Ref::GetObjectType(void) const
{
    return EObjectType(m_ObjectType);
}


inline
TSeqPos CAnnotObject_Ref::GetFrom(void) const
{
    return m_TotalRange.GetFrom();
}


inline
TSeqPos CAnnotObject_Ref::GetToOpen(void) const
{
    return m_TotalRange.GetToOpen();
}


inline
const CAnnotObject_Ref::TRange& CAnnotObject_Ref::GetTotalRange(void) const
{
    return m_TotalRange;
}


inline
void CAnnotObject_Ref::SetTotalRange(const TRange& range)
{
    m_TotalRange = range;
}


inline
unsigned int CAnnotObject_Ref::GetAnnotObjectIndex(void) const
{
    return m_AnnotObject_Index;
}


inline
const CSeq_annot_Info& CAnnotObject_Ref::GetSeq_annot_Info(void) const
{
    _ASSERT(GetObjectType() == eType_Seq_annot_Info);
    return reinterpret_cast<const CSeq_annot_Info&>(*m_Object);
}


inline
const CSeq_annot_SNP_Info& CAnnotObject_Ref::GetSeq_annot_SNP_Info(void) const
{
    _ASSERT(GetObjectType() == eType_Seq_annot_SNP_Info);
    return reinterpret_cast<const CSeq_annot_SNP_Info&>(*m_Object);
}


inline
bool CAnnotObject_Ref::IsPartial(void) const
{
    return (m_MappedFlags & fMapped_Partial) != 0;
}


inline
bool CAnnotObject_Ref::IsProduct(void) const
{
    return (m_MappedFlags & fMapped_Product) != 0;
}


inline
ENa_strand CAnnotObject_Ref::GetMappedStrand(void) const
{
    return ENa_strand(m_MappedStrand);
}


inline
void CAnnotObject_Ref::SetMappedStrand(ENa_strand strand)
{
    _ASSERT(IsMapped());
    m_MappedStrand = strand;
}


inline
CAnnotObject_Ref::EMappedObjectType
CAnnotObject_Ref::GetMappedObjectType(void) const
{
    return EMappedObjectType(m_MappedObjectType);
}


inline
bool CAnnotObject_Ref::IsMapped(void) const
{
    _ASSERT((GetMappedObjectType() == eMappedObjType_not_set) ==
            !m_MappedObject);
    return GetMappedObjectType() != eMappedObjType_not_set;

}


inline
bool CAnnotObject_Ref::MappedSeq_locNeedsUpdate(void) const
{
    return GetMappedObjectType() == eMappedObjType_Seq_id;

}


inline
bool CAnnotObject_Ref::IsMappedLocation(void) const
{
    return IsMapped() && !IsProduct();
}


inline
bool CAnnotObject_Ref::IsMappedProduct(void) const
{
    return IsMapped() && IsProduct();
}


inline
const CSeq_loc& CAnnotObject_Ref::GetMappedSeq_loc(void) const
{
    _ASSERT(GetMappedObjectType() == eMappedObjType_Seq_loc);
    return static_cast<const CSeq_loc&>(*m_MappedObject);
}


inline
const CSeq_id& CAnnotObject_Ref::GetMappedSeq_id(void) const
{
    _ASSERT(GetMappedObjectType() == eMappedObjType_Seq_id);
    return static_cast<const CSeq_id&>(*m_MappedObject);
}


inline
void CAnnotObject_Ref::SetMappedSeq_loc(CSeq_loc* loc)
{
    _ASSERT(!IsMapped());
    m_MappedObject.Reset(loc);
    m_MappedObjectType = loc? eMappedObjType_Seq_loc: eMappedObjType_not_set;
}


inline
void CAnnotObject_Ref::SetMappedSeq_loc(CSeq_loc& loc)
{
    _ASSERT(!IsMapped());
    m_MappedObject.Reset(&loc);
    m_MappedObjectType = eMappedObjType_Seq_loc;
}


inline
void CAnnotObject_Ref::SetMappedSeq_id(CSeq_id& id)
{
    _ASSERT(!IsMapped());
    m_MappedObject.Reset(&id);
    m_MappedObjectType = eMappedObjType_Seq_id;
}


inline
void CAnnotObject_Ref::SetMappedPoint(bool point)
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
bool CAnnotObject_Ref::IsMappedPoint(void) const
{
    _ASSERT(GetMappedObjectType() == eMappedObjType_Seq_id);
    return (m_MappedFlags & fMapped_Seq_point) != 0;
}


inline
void CAnnotObject_Ref::SetMappedSeq_id(CSeq_id& id, bool point)
{
    SetMappedSeq_id(id);
    SetMappedPoint(point);
}


inline
bool CAnnotObject_Ref::IsSNPFeat(void) const
{
    return GetObjectType() == eType_Seq_annot_SNP_Info;
}


inline
void CAnnotObject_Ref::SetPartial(bool partial)
{
    if ( partial ) {
        m_MappedFlags |= fMapped_Partial;
    }
    else {
        m_MappedFlags &= ~fMapped_Partial;
    }
}


inline
void CAnnotObject_Ref::SetProduct(bool product)
{
    if ( product ) {
        m_MappedFlags |= fMapped_Product;
    }
    else {
        m_MappedFlags &= ~fMapped_Product;
    }
}


inline
void CAnnotObject_Ref::SetAnnotObjectRange(const TRange& range, bool product)
{
    m_TotalRange = range;
    SetProduct(product);
}


inline
void CAnnotObject_Ref::ResetLocation(void)
{
    m_TotalRange = TRange::GetEmpty();
    m_MappedObject.Reset();
    m_MappedObjectType = eMappedObjType_not_set;
    m_MappedStrand = eNa_strand_unknown;
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
SAnnotSelector& CAnnot_Collector::GetSelector(void)
{
    return m_Selector;
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
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
