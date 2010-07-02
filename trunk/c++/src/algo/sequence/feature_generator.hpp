#ifndef ALGO_SEQUENCE___FEAT_GEN__HPP
#define ALGO_SEQUENCE___FEAT_GEN__HPP

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
 * Authors:  Vyacheslav Chetvernin
 *
 * File Description:
 *
 */

#include <corelib/ncbistd.hpp>
#include <corelib/ncbiobj.hpp>

#include <algo/sequence/gene_model.hpp>
#include <objects/seqalign/Spliced_seg.hpp>
#include <objects/seqloc/Na_strand.hpp>
#include <objmgr/seq_loc_mapper.hpp>

BEGIN_NCBI_SCOPE

BEGIN_SCOPE(objects)
//     class CScope;
//     class CSeq_feat;
//    class CSeq_align;
//     class CSeq_annot;
//     class CBioseq_set;
END_SCOPE(objects)

USING_SCOPE(objects);

BEGIN_SCOPE()

struct SExon {
    TSignedSeqPos prod_from;
    TSignedSeqPos prod_to;
    TSignedSeqPos genomic_from;
    TSignedSeqPos genomic_to;
};

END_SCOPE();

struct CFeatureGenerator::SImplementation {
    SImplementation(CRef<objects::CScope> scope);
    ~SImplementation();

    CRef<objects::CScope> m_scope;
    TFeatureGeneratorFlags m_flags;
    TSeqPos m_min_intron;
    TSeqPos m_allowed_unaligned;

    void StitchSmallHoles(objects::CSeq_align& align);
    void TrimHolesToCodons(objects::CSeq_align& align);

    void ConvertAlignToAnnot(const objects::CSeq_align& align,
                             objects::CSeq_annot& annot,
                             objects::CBioseq_set& seqs);
    void SetFeatureExceptions(objects::CSeq_feat& feat,
                              const objects::CSeq_align* align);

    TSignedSeqPos GetCdsStart(const objects::CSeq_id& seqid);

    void TrimLeftExon(int trim_amount,
                      vector<SExon>::reverse_iterator left_edge,
                      vector<SExon>::reverse_iterator& exon_it,
                      objects::CSpliced_seg::TExons::reverse_iterator& spl_exon_it,
                      objects::ENa_strand product_strand,
                      objects::ENa_strand genomic_strand);
    void TrimRightExon(int trim_amount,
                       vector<SExon>::iterator& exon_it,
                       vector<SExon>::iterator right_edge,
                       objects::CSpliced_seg::TExons::iterator& spl_exon_it,
                       objects::ENa_strand product_strand,
                       objects::ENa_strand genomic_strand);
private:

    struct SMapper
    {
    public:

        SMapper(const CSeq_align& aln, CScope& scope,
                TSeqPos allowed_unaligned = 10,
                CSeq_loc_Mapper::TMapOptions opts = 0);

        const CSeq_loc& GetRnaLoc();
        CSeq_align::TDim GetGenomicRow() const;
        CSeq_align::TDim GetRnaRow() const;
        CRef<CSeq_loc> Map(const CSeq_loc& loc);
        void IncludeSourceLocs(bool b = true);
        void SetMergeNone();

    private:

        /// This has special logic to set partialness based on alignment
        /// properties
        /// In addition, we need to interpret partial exons correctly
        CRef<CSeq_loc> x_GetLocFromSplicedExons(const CSeq_align& aln) const;

        const CSeq_align& m_aln;
        CScope& m_scope;
        CRef<CSeq_loc_Mapper> m_mapper;
        CSeq_align::TDim m_genomic_row;
        CRef<CSeq_loc> rna_loc;
        TSeqPos m_allowed_unaligned;
    };

    CRef<CSeq_feat> x_CreateMrnaFeature(const CBioseq_Handle& handle, CRef<CSeq_loc> loc, const CTime& time, size_t model_num, CBioseq_set& seqs, const CSeq_id& rna_id);
    CRef<CSeq_feat> x_CreateGeneFeature(const CBioseq_Handle& handle, SMapper& mapper, CRef<CSeq_feat> mrna_feat, CRef<CSeq_loc> loc, const CSeq_id& genomic_id);
    CRef<CSeq_feat> x_CreateCdsFeature(const CBioseq_Handle& handle, const CSeq_align& align, CRef<CSeq_loc> loc, const CTime& time, size_t model_num, CBioseq_set& seqs, CSeq_loc_Mapper::TMapOptions opts, CRef<CSeq_feat> gene_feat);
    void x_SetPartialWhereNeeded(CRef<CSeq_feat> mrna_feat, CRef<CSeq_feat> cds_feat, CRef<CSeq_feat> gene_feat);
    void x_CopyAdditionalFeatures(const CBioseq_Handle& handle, SMapper& mapper, CSeq_annot& annot);
};

END_NCBI_SCOPE

#endif  // ALGO_SEQUENCE___FEAT_GEN__HPP
