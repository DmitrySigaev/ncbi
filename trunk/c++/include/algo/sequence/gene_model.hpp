#ifndef ALGO_SEQUENCE___GENE_MODEL__HPP
#define ALGO_SEQUENCE___GENE_MODEL__HPP

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
 * Authors:  Mike DiCuccio
 *
 * File Description:
 *
 */

#include <corelib/ncbistd.hpp>
#include <corelib/ncbiobj.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
    class CScope;
    class CSeq_feat;
    class CSeq_align;
    class CSeq_annot;
    class CBioseq_set;
END_SCOPE(objects)

class NCBI_XALGOSEQ_EXPORT CFeatureGenerator
{
public:
    NCBI_DEPRECATED
    CFeatureGenerator(CRef<objects::CScope> scope);

    CFeatureGenerator(objects::CScope& scope);
    ~CFeatureGenerator();

    enum EGeneModelCreateFlags {
        fCreateGene          = 0x001,
        fCreateMrna          = 0x002,
        fCreateCdregion      = 0x004,
        fPromoteAllFeatures  = 0x008,
        fPropagateOnly       = 0x010,
        fForceTranslateCds   = 0x020,
        fForceTranscribeMrna = 0x040,
        fDensegAsExon        = 0x080,
        fGenerateLocalIds    = 0x100,

        fDefaults = fCreateGene | fCreateMrna | fCreateCdregion | fGenerateLocalIds
    };
    typedef int TFeatureGeneratorFlags;
    static const TSeqPos kDefaultMinIntron = 200;
    static const TSeqPos kDefaultAllowedUnaligned = 10;

    void SetFlags(TFeatureGeneratorFlags);
    TFeatureGeneratorFlags GetFlags() const;
    void SetMinIntron(TSeqPos);
    void SetAllowedUnaligned(TSeqPos);

    /// Clean an alignment according to our best guess of its biological
    /// representation.  Cleaning involves adjusting segments to satisfy our
    /// expectations of partial exonic alignments and account for unaligned
    /// parts. Eg. stitching small gaps, trimming to codon boundaries.
    CConstRef<objects::CSeq_align>
    CleanAlignment(const objects::CSeq_align& align);

    /// Convert an alignment to an annotation.
    /// This will optionally promote all features through the alignment
    /// and create product sequences
    /// Returns mRNA feature
    CRef<objects::CSeq_feat> ConvertAlignToAnnot(const objects::CSeq_align& align,
                             objects::CSeq_annot& annot,
                             objects::CBioseq_set& seqs,
                             int gene_id = 0,
                             const objects::CSeq_feat* cdregion_on_mrna = NULL);

    void ConvertAlignToAnnot(const list< CRef<objects::CSeq_align> > &aligns,
                             objects::CSeq_annot &annot,
                             objects::CBioseq_set &seqs);

    /// Correctly mark exceptions on a feature
    ///
    void SetFeatureExceptions(objects::CSeq_feat& feat,
                              const objects::CSeq_align* align = NULL);

    /// Mark the correct partial states for a set of features
    ///
    void SetPartialFlags(CRef<objects::CSeq_feat> gene_feat,
                         CRef<objects::CSeq_feat> mrna_feat,
                         CRef<objects::CSeq_feat> cds_feat);

    /// Recompute the correct partial states for all features in this annotation
    void RecomputePartialFlags(objects::CSeq_annot& annot);

private:
    struct SImplementation;
    auto_ptr<SImplementation> m_impl;
};


class NCBI_XALGOSEQ_EXPORT CGeneModel
{
public:
    enum EGeneModelCreateFlags {
        fCreateGene          = CFeatureGenerator::fCreateGene,
        fCreateMrna          = CFeatureGenerator::fCreateMrna,
        fCreateCdregion      = CFeatureGenerator::fCreateCdregion,
        fForceTranslateCds   = CFeatureGenerator::fForceTranslateCds,
        fForceTranscribeMrna = CFeatureGenerator::fForceTranscribeMrna,

        fDefaults = fCreateGene | fCreateMrna | fCreateCdregion
    };
    typedef int TGeneModelCreateFlags;

    /// Create a gene model from an alignment
    /// this will optionally promote all features through the alignment
    NCBI_DEPRECATED
    static void CreateGeneModelFromAlign(const objects::CSeq_align& align,
                                         objects::CScope& scope,
                                         objects::CSeq_annot& annot,
                                         objects::CBioseq_set& seqs,
                                         TGeneModelCreateFlags flags = fDefaults,
                                         TSeqPos allowed_unaligned = 10);

    NCBI_DEPRECATED
    static void CreateGeneModelsFromAligns(const list< CRef<objects::CSeq_align> > &aligns,
                                           objects::CScope& scope,
                                           objects::CSeq_annot& annot,
                                           objects::CBioseq_set& seqs,
                                           TGeneModelCreateFlags flags = fDefaults,
                                           TSeqPos allowed_unaligned = 10);

    /// Correctly mark exceptions on a feature
    ///
    NCBI_DEPRECATED
    static void SetFeatureExceptions(objects::CSeq_feat& feat,
                                     objects::CScope& scope,
                                     const objects::CSeq_align* align = NULL);

    NCBI_DEPRECATED
    static void SetPartialFlags(objects::CScope& scope,
                                CRef<objects::CSeq_feat> gene_feat,
                                CRef<objects::CSeq_feat> mrna_feat,
                                CRef<objects::CSeq_feat> cds_feat);

    NCBI_DEPRECATED
    static void RecomputePartialFlags(objects::CScope& scope,
                                      objects::CSeq_annot& annot);
};

END_NCBI_SCOPE

#endif  // ALGO_SEQUENCE___GENE_MODEL__HPP
