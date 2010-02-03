#ifndef SEQ_LOC_MAPPER_BASE__HPP
#define SEQ_LOC_MAPPER_BASE__HPP

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
*   Seq-loc mapper base
*
*/

#include <corelib/ncbistd.hpp>
#include <corelib/ncbiobj.hpp>
#include <util/range.hpp>
#include <util/rangemap.hpp>
#include <objects/seqloc/Na_strand.hpp>
#include <objects/seqalign/Seq_align.hpp>
#include <objects/seqalign/Spliced_exon.hpp>
#include <objects/seq/seq_id_handle.hpp>
#include <objects/general/Int_fuzz.hpp>
#include <objects/seq/annot_mapper_exception.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


/** @addtogroup ObjectManagerCore
 *
 * @{
 */


class CSeq_id;
class CSeq_loc;
class CSeq_interval;
class CPacked_seqpnt;
class CSeq_loc_CI;
class CSeq_feat;
class CSeq_align;
class CSeq_align_Mapper_Base;
class CSeq_graph;


class NCBI_SEQ_EXPORT CMappingRange : public CObject
{
public:
    CMappingRange(CSeq_id_Handle    src_id,
                  TSeqPos           src_from,
                  TSeqPos           src_length,
                  ENa_strand        src_strand,
                  CSeq_id_Handle    dst_id,
                  TSeqPos           dst_from,
                  ENa_strand        dst_strand,
                  bool              ext_to = false);

    bool GoodSrcId(const CSeq_id& id) const;
    CRef<CSeq_id> GetDstId(void) const;
    const CSeq_id_Handle& GetDstIdHandle(void) const
        { return m_Dst_id_Handle; }

    typedef CRange<TSeqPos>    TRange;
    typedef CRef<CInt_fuzz>    TFuzz;
    typedef pair<TFuzz, TFuzz> TRangeFuzz;

    bool CanMap(TSeqPos    from,
                TSeqPos    to,
                bool       is_set_strand,
                ENa_strand strand) const;
    TSeqPos Map_Pos(TSeqPos pos) const;
    TRange Map_Range(TSeqPos           from,
                     TSeqPos           to,
                     const TRangeFuzz* fuzz = 0) const;
    bool Map_Strand(bool is_set_strand,
                    ENa_strand src,
                    ENa_strand* dst) const;
    TRangeFuzz Map_Fuzz(const TRangeFuzz& fuzz) const;

private:
    CInt_fuzz::ELim x_ReverseFuzzLim(CInt_fuzz::ELim lim) const;

    CSeq_id_Handle      m_Src_id_Handle;
    TSeqPos             m_Src_from;
    TSeqPos             m_Src_to;
    ENa_strand          m_Src_strand;
    CSeq_id_Handle      m_Dst_id_Handle;
    TSeqPos             m_Dst_from;
    ENa_strand          m_Dst_strand;
    bool                m_Reverse;
    bool                m_ExtTo;
    int                 m_Group;

    friend class CSeq_loc_Mapper_Base;
    //friend class CSeq_loc_Mapper;
    friend class CMappingRanges;
    friend class CSeq_align_Mapper_Base;
    //friend class CSeq_align_Mapper;
    friend struct CMappingRangeRef_Less;
    friend struct CMappingRangeRef_LessRev;
public:
    // Interface for CPairwiseAln converter
    TSeqPos GetSrc_from(void) const { return m_Src_from; }
    TSeqPos GetDst_from(void) const { return m_Dst_from; }
    TSeqPos GetLength(void) const { return m_Src_to - m_Src_from; }
    bool GetReverse(void) const { return m_Reverse; }
    int GetGroup(void) const { return m_Group; }
    void SetGroup(int grp) { m_Group = grp; }
};


class NCBI_SEQ_EXPORT CMappingRanges : public CObject
{
public:
    CMappingRanges(void);

    // Conversions
    typedef CMappingRange::TRange                        TRange;
    typedef CRangeMultimap<CRef<CMappingRange>, TSeqPos> TRangeMap;
    typedef TRangeMap::const_iterator                    TRangeIterator;
    typedef map<CSeq_id_Handle, TRangeMap>               TIdMap;
    typedef TIdMap::const_iterator                       TIdIterator;
    typedef vector< CRef<CMappingRange> >                TSortedMappings;

    const TIdMap& GetIdMap() const { return m_IdMap; }
    TIdMap& GetIdMap(void) { return m_IdMap; }

    void AddConversion(CRef<CMappingRange> cvt);
    CRef<CMappingRange> AddConversion(CSeq_id_Handle    src_id,
                                      TSeqPos           src_from,
                                      TSeqPos           src_length,
                                      ENa_strand        src_strand,
                                      CSeq_id_Handle    dst_id,
                                      TSeqPos           dst_from,
                                      ENa_strand        dst_strand,
                                      bool              ext_to = false);

    TRangeIterator BeginMappingRanges(CSeq_id_Handle id,
                                      TSeqPos        from,
                                      TSeqPos        to) const;

    // Set overall source orientation. The order of ranges is reversed
    // if reverse_src != reverse_dst.
    void SetReverseSrc(bool value = true) { m_ReverseSrc = value; };
    bool GetReverseSrc(void) const { return m_ReverseSrc; }
    // Set overall destination orientation. The order of ranges is reversed
    // if reverse_src != reverse_dst.
    void SetReverseDst(bool value = true) { m_ReverseDst = value; };
    bool GetReverseDst(void) const { return m_ReverseDst; }

private:
    TIdMap m_IdMap;

    // Mapping source and destination orientations
    bool   m_ReverseSrc;
    bool   m_ReverseDst;
};


/// Helper class for mapping graphs
class NCBI_SEQ_EXPORT CGraphRanges : public CObject
{
public:
    CGraphRanges(void) : m_Offset(0) {}

    typedef CRange<TSeqPos> TRange;
    typedef vector<TRange>  TGraphRanges;

    TSeqPos GetOffset(void) const { return m_Offset; }
    void SetOffset(TSeqPos offset) { m_Offset = offset; }
    void IncOffset(TSeqPos inc) { m_Offset += inc; }

    const TGraphRanges& GetRanges(void) const { return m_Ranges; }
    void AddRange(const TRange& rg)
    {
        if ( rg.Empty() ) {
            return;
        }
        TRange offset_rg = rg.IsWhole() ? rg :
            TRange(rg.GetFrom() + m_Offset, rg.GetTo() + m_Offset);
        m_Ranges.push_back(offset_rg);
        m_TotalRange.CombineWith(offset_rg);
    }

    const TRange& GetTotalRange(void) const { return m_TotalRange; }

private:
    TSeqPos      m_Offset; // Offset new ranges
    TGraphRanges m_Ranges;
    TRange       m_TotalRange;
};


/////////////////////////////////////////////////////////////////////////////
///
///  CSeq_loc_Mapper_Base --
///
///  Mapping locations and alignments between bioseqs through seq-locs,
///  features, alignments or between parts of segmented bioseqs.


class NCBI_SEQ_EXPORT CSeq_loc_Mapper_Base : public CObject
{
public:
    enum EFeatMapDirection {
        eLocationToProduct,
        eProductToLocation
    };

    /// options for interpretations of locations
    enum EMapOptions {
        /// Ignore internal dense-seg structure - map each
        /// dense-seg according to the total ranges involved
        fAlign_Dense_seg_TotalRange = 0x01,
        fAlign_Sparse_ToFirst       = 0x00, ///< Map to first-id
        fAlign_Sparse_ToSecond      = 0x02  ///< Map to second-id
    };
    typedef int TMapOptions;

    /// Spliced-seg row indexing constants.
    enum ESplicedRow {
        eSplicedRow_Prod = 0,
        eSplicedRow_Gen  = 1
    };

    /// Mapping through a pre-filled CMappipngRanges.
    /// @param mapping_ranges
    ///  CMappingRanges filled with the desired source and destination
    ///  ranges. Must be a heap object (will be stored in a CRef<>).
    CSeq_loc_Mapper_Base(CMappingRanges* mapping_ranges);

    /// Mapping through a feature, both location and product must be set.
    CSeq_loc_Mapper_Base(const CSeq_feat&  map_feat,
                         EFeatMapDirection dir);

    /// Mapping between two seq_locs.
    CSeq_loc_Mapper_Base(const CSeq_loc&   source,
                         const CSeq_loc&   target);

    /// Mapping through an alignment. Need to specify target ID or
    /// target row of the alignment. Any other ID is mapped to the
    /// target one. Only the first row matching target ID is used,
    /// all other rows are considered source.
    /// If the alignment is a spliced-seg, the merging is set to
    /// 'merge by segment' automatically.
    /// @sa SetMergeBySeg
    CSeq_loc_Mapper_Base(const CSeq_align& map_align,
                         const CSeq_id&    to_id,
                         TMapOptions       opts = 0);
    /// Sparse alignments require special row indexing since each
    /// row contains two seq-ids. Use options to specify mapping
    /// direction.
    /// If the alignment is a spliced-seg, the merging is set to
    /// 'merge by segment' automatically.
    /// @sa SetMergeBySeg
    CSeq_loc_Mapper_Base(const CSeq_align& map_align,
                         size_t            to_row,
                         TMapOptions       opts = 0);

    ~CSeq_loc_Mapper_Base(void);

    /// Intervals' merging mode
    /// MergeNone and MergeAbutting do not change the order of ranges
    /// in the destination seq-loc. No ranges will be merged if they
    /// are separated by any other sub-range.
    /// MergeContained and MergeAll sort ranges before sorting, so that
    /// any overlapping ranges can be merged.
    /// No merging
    CSeq_loc_Mapper_Base& SetMergeNone(void);
    /// Merge only abutting intervals, keep overlapping
    CSeq_loc_Mapper_Base& SetMergeAbutting(void);
    /// Merge only intervals from the same group (e.g. mapped through
    /// the same exon)
    CSeq_loc_Mapper_Base& SetMergeBySeg(void);
    /// Merge intervals only if one is completely covered by another
    CSeq_loc_Mapper_Base& SetMergeContained(void);
    /// Merge any abutting or overlapping intervals
    CSeq_loc_Mapper_Base& SetMergeAll(void);

    CSeq_loc_Mapper_Base& SetGapPreserve(void);
    CSeq_loc_Mapper_Base& SetGapRemove(void);

    /// Keep ranges which can not be mapped. Does not affect truncation
    /// of partially mapped ranges. By default nonmapping ranges are
    /// converted to NULL.
    CSeq_loc_Mapper_Base& KeepNonmappingRanges(void);
    CSeq_loc_Mapper_Base& TruncateNonmappingRanges(void);

    /// Check strands before mapping a range. By default strand is not
    /// checked and a range will be mapped even if its strand does not
    /// correspond to the strand of the mapping source.
    CSeq_loc_Mapper_Base& SetCheckStrand(bool value = true);

    /// Include source ranges in the mapped location. If turned
    /// on, the resulting seq-loc will be an equiv with the
    /// first sub-loc containing the usual mapped seq-loc, and
    /// the second one - the set of source locations used in the
    /// mapping.
    CSeq_loc_Mapper_Base& IncludeSourceLocs(bool value = true);

    /// Map seq-loc
    CRef<CSeq_loc>   Map(const CSeq_loc& src_loc);
    /// Map the whole alignment
    CRef<CSeq_align> Map(const CSeq_align& src_align);
    /// Map a single row of the alignment
    CRef<CSeq_align> Map(const CSeq_align& src_align,
                         size_t            row);
    /// Map seq-graph. This will map both location and data.
    CRef<CSeq_graph> Map(const CSeq_graph& src_graph);

    /// Check if the last mapping resulted in partial location
    bool             LastIsPartial(void);

    typedef vector<CSeq_id_Handle>        TSynonyms;

    // Collect synonyms for the given seq-id
    virtual void CollectSynonyms(const CSeq_id_Handle& id,
                                 TSynonyms&            synonyms) const;

protected:
    // Sequence type - to recalculate coordinates.
    enum ESeqType {
        eSeq_unknown = 0,
        eSeq_nuc = 1,
        eSeq_prot = 3
    };

    // Get molecule type.
    virtual ESeqType GetSeqType(const CSeq_id_Handle& idh) const;

    // Get sequence length for the given seq-id
    virtual TSeqPos GetSequenceLength(const CSeq_id& id);

    // Create CSeq_align_Mapper, add any necessary arguments
    virtual CSeq_align_Mapper_Base*
        InitAlignMapper(const CSeq_align& src_align);

    // Initialization methods
    void x_InitializeFeat(const CSeq_feat&  map_feat,
                          EFeatMapDirection dir);
    // Optional frame is used for cd-region only
    void x_InitializeLocs(const CSeq_loc& source,
                          const CSeq_loc& target,
                          int             frame = 0);
    void x_InitializeAlign(const CSeq_align& map_align,
                           const CSeq_id&    to_id,
                           TMapOptions       opts);
    void x_InitializeAlign(const CSeq_align& map_align,
                           size_t            to_row,
                           TMapOptions       opts);

    // Create target-to-target mapping to avoid truncation of ranges
    // already on the target sequence(s).
    void x_PreserveDestinationLocs(void);

    void x_NextMappingRange(const CSeq_id&   src_id,
                            TSeqPos&         src_start,
                            TSeqPos&         src_len,
                            ENa_strand       src_strand,
                            const CSeq_id&   dst_id,
                            TSeqPos&         dst_start,
                            TSeqPos&         dst_len,
                            ENa_strand       dst_strand,
                            const CInt_fuzz* fuzz_from = 0,
                            const CInt_fuzz* fuzz_to = 0);

    void x_MapSeq_loc(const CSeq_loc& src_loc);

    // Convert collected ranges into seq-loc and push into destination mix.
    void x_PushRangesToDstMix(void);

    typedef CMappingRange::TRange           TRange;
    typedef CMappingRanges::TRangeMap       TRangeMap;
    typedef CMappingRanges::TRangeIterator  TRangeIterator;
    typedef CMappingRanges::TSortedMappings TSortedMappings;

    // List and map of target ranges to construct target-to-target mapping
    typedef list<TRange>                    TDstRanges;
    typedef map<CSeq_id_Handle, TDstRanges> TDstIdMap;
    typedef vector<TDstIdMap>               TDstStrandMap;

    // Destination locations arranged by ID/range
    typedef CRef<CInt_fuzz>                 TFuzz;
    typedef pair<TFuzz, TFuzz>              TRangeFuzz;

    struct SMappedRange {
        SMappedRange(void) : group(0) {}
        SMappedRange(const TRange&      rg,
                     const TRangeFuzz&  fz,
                     int                grp = 0)
            : range(rg), fuzz(fz), group(grp) {}

        TRange      range;
        TRangeFuzz  fuzz;
        int         group;

        bool operator<(const SMappedRange& rg) const
            {
                return range < rg.range;
            }
    };
    //typedef pair<TRange, TRangeFuzz>             TRangeWithFuzz;
    typedef list<SMappedRange>                   TMappedRanges;
    // 0 = not set, any other index = na_strand + 1
    typedef vector<TMappedRanges>                TRangesByStrand;
    typedef map<CSeq_id_Handle, TRangesByStrand> TRangesById;
    typedef map<CSeq_id_Handle, ESeqType>        TSeqTypeById;

    typedef CSeq_align::C_Segs::TDendiag         TDendiag;
    typedef CSeq_align::C_Segs::TStd             TStd;

private:
    CSeq_loc_Mapper_Base(const CSeq_loc_Mapper_Base&);
    CSeq_loc_Mapper_Base& operator=(const CSeq_loc_Mapper_Base&);

    friend class CSeq_align_Mapper_Base;

    enum EMergeFlags {
        eMergeNone,      // no merging
        eMergeAbutting,  // merge only abutting intervals, keep overlapping
        eMergeContained, // merge if one is contained in another
        eMergeBySeg,     // merge ranges by mapping group (e.g. exon)
        eMergeAll        // merge both abutting and overlapping intervals
    };
    enum EGapFlags {
        eGapPreserve,    // Leave gaps as-is
        eGapRemove       // Remove gaps (NULLs)
    };

    // Check types of all sequences referenced by the location,
    // calculate the total length of the location, return true
    // if types are known for all sequences.
    // Set seqtype to the detected sequence type or to unknown
    // if the type can not be detected or there are multiple types.
    bool x_CheckSeqTypes(const CSeq_loc& loc,
                         ESeqType&       seqtype,
                         TSeqPos&        len) const;
    // Try to find at least one known sequence type in the location
    // and force it for all other sequences if any.
    ESeqType x_ForceSeqTypes(const CSeq_loc& loc) const;

    void x_AdjustSeqTypesToProt(const CSeq_id_Handle& idh);

    // Get sequence length, try to get the real length for
    // reverse strand, do not use "whole".
    TSeqPos x_GetRangeLength(const CSeq_loc_CI& it);

    void x_AddConversion(const CSeq_id& src_id,
                         TSeqPos        src_start,
                         ENa_strand     src_strand,
                         const CSeq_id& dst_id,
                         TSeqPos        dst_start,
                         ENa_strand     dst_strand,
                         TSeqPos        length,
                         bool           ext_right);

    void x_InitAlign(const CDense_diag& diag, size_t to_row);
    void x_InitAlign(const CDense_seg& denseg, size_t to_row,
                     TMapOptions opts);
    void x_InitAlign(const CStd_seg& sseg, size_t to_row);
    void x_InitAlign(const CPacked_seg& pseg, size_t to_row);
    void x_InitSpliced(const CSpliced_seg& spliced,
                       const CSeq_id&      to_id);
    void x_InitSpliced(const CSpliced_seg& spliced, ESplicedRow to_row);
    void x_InitSparse(const CSparse_seg& sparse, int to_row,
                      TMapOptions opts);

    void x_IterateExonParts(const CSpliced_exon::TParts& parts,
                            ESplicedRow                  to_row,
                            const CSeq_id&               gen_id,
                            TSeqPos&                     gen_start,
                            TSeqPos&                     gen_len,
                            ENa_strand                   gen_strand,
                            const CSeq_id&               prod_id,
                            TSeqPos&                     prod_start,
                            TSeqPos&                     prod_len,
                            ENa_strand                   prod_strand);
    static TSeqPos sx_GetExonPartLength(const CSpliced_exon_chunk& part);

    bool x_MapNextRange(const TRange&     src_rg,
                        bool              is_set_strand,
                        ENa_strand        src_strand,
                        const TRangeFuzz& src_fuzz,
                        TSortedMappings&  mappings,
                        size_t            cvt_idx,
                        TSeqPos*          last_src_to);
    bool x_MapInterval(const CSeq_id&   src_id,
                       TRange           src_rg,
                       bool             is_set_strand,
                       ENa_strand       src_strand,
                       TRangeFuzz       orig_fuzz);
    void x_SetLastTruncated(void);

    void x_PushLocToDstMix(CRef<CSeq_loc> loc);

    // Map alignment. If row == -1, map all rows.
    CRef<CSeq_align> x_MapSeq_align(const CSeq_align& src_align,
                                    size_t*           row);

    // Access mapped ranges, check vector size
    TMappedRanges& x_GetMappedRanges(const CSeq_id_Handle& id,
                                     size_t                strand_idx) const;
    void x_PushMappedRange(const CSeq_id_Handle& id,
                           size_t                strand_idx,
                           const TRange&         range,
                           const TRangeFuzz&     fuzz,
                           int                   group);
    void x_PushSourceRange(const CSeq_id_Handle& idh,
                           size_t                src_strand,
                           size_t                dst_strand,
                           const TRange&         range);

    CRef<CSeq_loc> x_RangeToSeq_loc(const CSeq_id_Handle& idh,
                                    TSeqPos               from,
                                    TSeqPos               to,
                                    size_t                strand_idx,
                                    TRangeFuzz            rg_fuzz);

    CRef<CSeq_loc> x_GetMappedSeq_loc(void);
    void x_OptimizeSeq_loc(CRef<CSeq_loc>& loc) const;

    bool x_ReverseRangeOrder(void) const;

    void x_Map_PackedInt_Element(const CSeq_interval& si);
    void x_Map_PackedPnt_Element(const CPacked_seqpnt& pp, TSeqPos p);

    EMergeFlags          m_MergeFlag;
    EGapFlags            m_GapFlag;
    bool                 m_KeepNonmapping;
    bool                 m_CheckStrand; // Check strands before mapping
    bool                 m_IncludeSrcLocs;

    mutable TRangesById  m_MappedLocs;
    CRef<CSeq_loc>       m_SrcLocs;

    // Collecting ranges for mapped graph
    CRef<CGraphRanges>   m_GraphRanges;

protected:
    // Sources may have different types, e.g. in an alignment
    mutable TSeqTypeById m_SeqTypes;
    bool                 m_Partial;
    bool                 m_LastTruncated;
    CRef<CMappingRanges> m_Mappings;
    CRef<CSeq_loc>       m_Dst_loc;
    TDstStrandMap        m_DstRanges;
    int                  m_CurrentGroup;

public:
    // Initialize the mapper with default values
    CSeq_loc_Mapper_Base(void);

    // Methods for getting sequence types, use cached types (m_SeqTypes)
    // if possible.
    ESeqType GetSeqTypeById(const CSeq_id_Handle& idh) const;
    ESeqType GetSeqTypeById(const CSeq_id& id) const;
    // Methods for setting sequence types. May be used to populate the
    // cache before mapping huge alignments if the types are already
    // known.
    void SetSeqTypeById(const CSeq_id_Handle& idh, ESeqType seqtype) const;
    void SetSeqTypeById(const CSeq_id& id, ESeqType seqtype) const;

    // Get sequence width.
    int GetWidthById(const CSeq_id_Handle& idh) const;
    int GetWidthById(const CSeq_id& id) const;

    // Get mapping ranges.
    const CMappingRanges& GetMappingRanges(void) const { return *m_Mappings; }
};


struct CMappingRangeRef_Less
{
    bool operator()(const CRef<CMappingRange>& x,
                    const CRef<CMappingRange>& y) const;
};


struct CMappingRangeRef_LessRev
{
    bool operator()(const CRef<CMappingRange>& x,
                    const CRef<CMappingRange>& y) const;
};


inline
bool CMappingRangeRef_Less::operator()(const CRef<CMappingRange>& x,
                                       const CRef<CMappingRange>& y) const
{
    // Leftmost first
    if (x->m_Src_from != y->m_Src_from) {
        return x->m_Src_from < y->m_Src_from;
    }
    // Longest first
    if (x->m_Src_to != y->m_Src_to) {
        return x->m_Src_to > y->m_Src_to;
    }
    return x < y;
}


inline
bool CMappingRangeRef_LessRev::operator()(const CRef<CMappingRange>& x,
                                          const CRef<CMappingRange>& y) const
{
    // Rightmost first
    if (x->m_Src_to != y->m_Src_to) {
        return x->m_Src_to > y->m_Src_to;
    }
    // Longest first
    if (x->m_Src_from != y->m_Src_from) {
        return x->m_Src_from < y->m_Src_from;
    }
    return x > y;
}


inline
bool CMappingRange::GoodSrcId(const CSeq_id& id) const
{
    return m_Src_id_Handle == id;
}


inline
CRef<CSeq_id> CMappingRange::GetDstId(void) const
{
    return m_Dst_id_Handle ?
        Ref(&const_cast<CSeq_id&>(*m_Dst_id_Handle.GetSeqId())) :
        CRef<CSeq_id>(0);
}


inline
CSeq_loc_Mapper_Base& CSeq_loc_Mapper_Base::SetMergeNone(void)
{
    m_MergeFlag = eMergeNone;
    return *this;
}


inline
CSeq_loc_Mapper_Base& CSeq_loc_Mapper_Base::SetMergeAbutting(void)
{
    m_MergeFlag = eMergeAbutting;
    return *this;
}


inline
CSeq_loc_Mapper_Base& CSeq_loc_Mapper_Base::SetMergeBySeg(void)
{
    m_MergeFlag = eMergeBySeg;
    return *this;
}


inline
CSeq_loc_Mapper_Base& CSeq_loc_Mapper_Base::SetMergeContained(void)
{
    m_MergeFlag = eMergeContained;
    return *this;
}


inline
CSeq_loc_Mapper_Base& CSeq_loc_Mapper_Base::SetMergeAll(void)
{
    m_MergeFlag = eMergeAll;
    return *this;
}


inline
CSeq_loc_Mapper_Base& CSeq_loc_Mapper_Base::SetGapPreserve(void)
{
    m_GapFlag = eGapPreserve;
    return *this;
}


inline
CSeq_loc_Mapper_Base& CSeq_loc_Mapper_Base::SetGapRemove(void)
{
    m_GapFlag = eGapRemove;
    return *this;
}


inline
CSeq_loc_Mapper_Base& CSeq_loc_Mapper_Base::SetCheckStrand(bool value)
{
    m_CheckStrand = value;
    return *this;
}


inline
bool CSeq_loc_Mapper_Base::LastIsPartial(void)
{
    return m_Partial;
}


inline
CSeq_loc_Mapper_Base& CSeq_loc_Mapper_Base::KeepNonmappingRanges(void)
{
    m_KeepNonmapping = true;
    return *this;
}


inline
CSeq_loc_Mapper_Base& CSeq_loc_Mapper_Base::TruncateNonmappingRanges(void)
{
    m_KeepNonmapping = false;
    return *this;
}


inline
CSeq_loc_Mapper_Base& CSeq_loc_Mapper_Base::IncludeSourceLocs(bool value)
{
    m_IncludeSrcLocs = value;
    return *this;
}


inline
bool CSeq_loc_Mapper_Base::x_ReverseRangeOrder(void) const
{
    if (m_MergeFlag == eMergeContained  || m_MergeFlag == eMergeAll) {
        // Sorting discards the original order, no need to check
        // m_ReverseSrc
        return m_Mappings->GetReverseDst();
    }
    return m_Mappings->GetReverseSrc() != m_Mappings->GetReverseDst();
}


inline
CRef<CSeq_align> CSeq_loc_Mapper_Base::Map(const CSeq_align& src_align)
{
    return x_MapSeq_align(src_align, 0);
}


inline
CRef<CSeq_align> CSeq_loc_Mapper_Base::Map(const CSeq_align& src_align,
                                           size_t            row)
{
    return x_MapSeq_align(src_align, &row);
}


inline
CSeq_loc_Mapper_Base::ESeqType
CSeq_loc_Mapper_Base::GetSeqTypeById(const CSeq_id_Handle& idh) const
{
    TSeqTypeById::const_iterator it = m_SeqTypes.find(idh);
    if (it != m_SeqTypes.end()) {
        return it->second;
    }
    return GetSeqType(idh);
}


inline
CSeq_loc_Mapper_Base::ESeqType
CSeq_loc_Mapper_Base::GetSeqTypeById(const CSeq_id& id) const
{
    return GetSeqTypeById(CSeq_id_Handle::GetHandle(id));
}


inline
void CSeq_loc_Mapper_Base::SetSeqTypeById(const CSeq_id& id,
                                          ESeqType       seqtype) const
{
    SetSeqTypeById(CSeq_id_Handle::GetHandle(id), seqtype);
}


inline
int CSeq_loc_Mapper_Base::GetWidthById(const CSeq_id_Handle& idh) const
{
    return (GetSeqTypeById(idh) == eSeq_prot) ? 3 : 1;
}


inline
int CSeq_loc_Mapper_Base::GetWidthById(const CSeq_id& id) const
{
    return GetWidthById(CSeq_id_Handle::GetHandle(id));
}


/* @} */


END_SCOPE(objects)
END_NCBI_SCOPE

#endif  // SEQ_LOC_MAPPER_BASE__HPP
