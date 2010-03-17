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

#include <ncbi_pch.hpp>
#include <algo/align/util/score_builder.hpp>
#include <algo/blast/api/blast_aux.hpp>
#include <algo/blast/api/blast_options_handle.hpp>
#include <algo/blast/core/blast_setup.h>
#include <algo/blast/core/blast_stat.h>
#include <algo/blast/core/blast_options.h>

#include <objmgr/util/sequence.hpp>
#include <objtools/alnmgr/alnvec.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqalign/Seq_align.hpp>
#include <objects/seqalign/Std_seg.hpp>
#include <objects/seqalign/Spliced_seg.hpp>
#include <objects/seqalign/Spliced_exon.hpp>
#include <objects/seqalign/Spliced_exon_chunk.hpp>
#include <objects/seqalign/Product_pos.hpp>
#include <objects/seqalign/Prot_pos.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
USING_SCOPE(blast);

void CScoreBuilder::x_Initialize(CBlastOptionsHandle& options)
{
    Int2 status;
    const CBlastOptions& opts = options.GetOptions();

    m_GapOpen = opts.GetGapOpeningCost();
    m_GapExtend = opts.GetGapExtensionCost();

    // alignments are either protein-protein or nucl-nucl

    m_BlastType = opts.GetProgram();
    if (m_BlastType == eBlastn || m_BlastType == eMegablast ||
                                m_BlastType == eDiscMegablast) {
        m_BlastType = eBlastn;
    } else {
        m_BlastType = eBlastp;
    }

    if (m_BlastType == eBlastn) {
        m_ScoreBlk = BlastScoreBlkNew(BLASTNA_SEQ_CODE, 1);
    } else {
        m_ScoreBlk = BlastScoreBlkNew(BLASTAA_SEQ_CODE, 1);
    }
    if (m_ScoreBlk == NULL) {
        NCBI_THROW(CException, eUnknown,
                "Failed to initialize blast score block");
    }

    // fill in the score matrix

    EBlastProgramType core_type = EProgramToEBlastProgramType(m_BlastType);
    BlastScoringOptions *score_options;
    BlastScoringOptionsNew(core_type, &score_options);
    BLAST_FillScoringOptions(score_options, core_type, TRUE,
                            opts.GetMismatchPenalty(),
                            opts.GetMatchReward(),
                            opts.GetMatrixName(),
                            m_GapOpen, m_GapExtend);
    status = Blast_ScoreBlkMatrixInit(core_type, score_options,
                                      m_ScoreBlk, NULL);
    score_options = BlastScoringOptionsFree(score_options);
    if (status) {
        NCBI_THROW(CException, eUnknown,
                "Failed to initialize score matrix");
    }

    // fill in Karlin blocks

    m_ScoreBlk->kbp_gap_std[0] = Blast_KarlinBlkNew();
    if (m_BlastType == eBlastn) {
        // the following computes the same Karlin blocks as blast
        // if the gap penalties are not large. When the penalties are
        // large the ungapped Karlin blocks are used, but these require
        // sequence data to be computed exactly. Instead we build an
        // ideal ungapped Karlin block to approximate the exact answer
        Blast_ScoreBlkKbpIdealCalc(m_ScoreBlk);
        status = Blast_KarlinBlkNuclGappedCalc(m_ScoreBlk->kbp_gap_std[0],
                                               m_GapOpen, m_GapExtend,
                                               m_ScoreBlk->reward,
                                               m_ScoreBlk->penalty,
                                               m_ScoreBlk->kbp_ideal,
                                               &(m_ScoreBlk->round_down),
                                               NULL);
    }
    else {
        status = Blast_KarlinBlkGappedCalc(m_ScoreBlk->kbp_gap_std[0],
                                           m_GapOpen, m_GapExtend,
                                           m_ScoreBlk->name, NULL);
    }
    if (status || m_ScoreBlk->kbp_gap_std[0] == NULL ||
        m_ScoreBlk->kbp_gap_std[0]->Lambda <= 0.0) {
        NCBI_THROW(CException, eUnknown,
                "Failed to initialize Karlin blocks");
    }
}

/// Default constructor
CScoreBuilder::CScoreBuilder()
    : m_ScoreBlk(0),
      m_EffectiveSearchSpace(0)
{
}

/// Constructor (uses blast program defaults)
CScoreBuilder::CScoreBuilder(EProgram blast_program)
    : m_EffectiveSearchSpace(0)
{
    CRef<CBlastOptionsHandle>
                        options(CBlastOptionsFactory::Create(blast_program));
    x_Initialize(*options);
}

/// Constructor (uses previously configured blast options)
CScoreBuilder::CScoreBuilder(CBlastOptionsHandle& options)
    : m_EffectiveSearchSpace(0)
{
    x_Initialize(options);
}

/// Destructor
CScoreBuilder::~CScoreBuilder()
{
    m_ScoreBlk = BlastScoreBlkFree(m_ScoreBlk);
}

///
/// calculate the length of our alignment
///
static size_t s_GetAlignmentLength(const CSeq_align& align,
                                   bool ungapped)
{
    size_t len = 0;
    switch (align.GetSegs().Which()) {
    case CSeq_align::TSegs::e_Denseg:
        {{
            const CDense_seg& ds = align.GetSegs().GetDenseg();
            if (ungapped) {
                for (CDense_seg::TNumseg i = 0;  i < ds.GetNumseg();  ++i) {
                    bool is_gap_seg = false;
                    for (CDense_seg::TDim j = 0;
                         !is_gap_seg  &&  j < ds.GetDim();  ++j) {
                        //int start = ds.GetStarts()[i * ds.GetDim() + j];
                        if (ds.GetStarts()[i * ds.GetDim() + j] == -1) {
                            is_gap_seg = true;
                        }
                    }
                    if ( !is_gap_seg ) {
                        len += ds.GetLens()[i];
                    }
                }
            } else {
                for (size_t i = 0;  i < ds.GetLens().size();  ++i) {
                    len += ds.GetLens()[i];
                }
            }
        }}
        break;

    case CSeq_align::TSegs::e_Disc:
        ITERATE (CSeq_align::TSegs::TDisc::Tdata, iter,
                 align.GetSegs().GetDisc().Get()) {
            len += s_GetAlignmentLength(**iter, ungapped);
        }
        break;

    case CSeq_align::TSegs::e_Std:
        {{
             /// pass 1:
             /// find total ranges
             vector<TSeqPos> sizes;

             size_t count = 0;
             ITERATE (CSeq_align::TSegs::TStd, iter, align.GetSegs().GetStd()) {
                 const CStd_seg& seg = **iter;

                 size_t i = 0;
                 ITERATE (CStd_seg::TLoc, it, seg.GetLoc()) {
                     if ((*it)->IsEmpty()) {
                         /// skip
                     } else {
                         if (sizes.empty()) {
                             sizes.resize(seg.GetDim(), 0);
                         } else if (sizes.size() != (size_t)seg.GetDim()) {
                             NCBI_THROW(CException, eUnknown,
                                        "invalid std-seg: "
                                        "inconsistent number of locs");
                         }
                         sizes[i] += sequence::GetLength(**it, NULL);
                     }

                     ++i;
                 }
                 ++count;
             }

             /// pass 2: determine shortest length
             vector<TSeqPos>::iterator iter = sizes.begin();
             vector<TSeqPos>::iterator smallest = iter;
             for (++iter;  iter != sizes.end();  ++iter) {
                 if (*iter < *smallest) {
                     smallest = iter;
                 }
             }
             return *smallest;
         }}
        break;

    case CSeq_align::TSegs::e_Spliced:
        ITERATE (CSpliced_seg::TExons, iter,
                 align.GetSegs().GetSpliced().GetExons()) {
            const CSpliced_exon& exon = **iter;
            if (exon.IsSetParts()) {
                ITERATE (CSpliced_exon::TParts, it, exon.GetParts()) {
                    const CSpliced_exon_chunk& chunk = **it;
                    switch (chunk.Which()) {
                    case CSpliced_exon_chunk::e_Match:
                        len += chunk.GetMatch();
                        break;

                    case CSpliced_exon_chunk::e_Mismatch:
                        len += chunk.GetMismatch();
                        break;

                    case CSpliced_exon_chunk::e_Diag:
                        len += chunk.GetDiag();
                        break;

                    case CSpliced_exon_chunk::e_Product_ins:
                        if ( !ungapped ) {
                            len += chunk.GetProduct_ins();
                        }
                        break;

                    case CSpliced_exon_chunk::e_Genomic_ins:
                        if ( !ungapped ) {
                            len += chunk.GetGenomic_ins();
                        }
                        break;

                    default:
                        break;
                    }
                }
            } else {
                size_t product_span = 0;
                if (exon.GetProduct_start().IsNucpos()) {
                    product_span =
                        exon.GetProduct_end().GetNucpos() -
                        exon.GetProduct_start().GetNucpos();
                } else if (exon.GetProduct_start().IsProtpos()) {
                    product_span =
                        exon.GetProduct_end().GetProtpos().GetAmin() -
                        exon.GetProduct_start().GetProtpos().GetAmin();
                } else {
                    NCBI_THROW(CException, eUnknown,
                               "Spliced-exon is neirther nuc nor prot");
                }

                size_t genomic_span =
                    exon.GetGenomic_end() - exon.GetGenomic_start();

                len += max(genomic_span, product_span);
            }
        }
        break;

    default:
        _ASSERT(false);
        break;
    }

    return len;
}


///
/// calculate mismatches and identities in a seq-align
///
static void s_GetCountIdentityMismatch(CScope& scope, const CSeq_align& align,
                                       int* identities, int* mismatches)
{
    _ASSERT(identities  &&  mismatches);
    switch (align.GetSegs().Which()) {
    case CSeq_align::TSegs::e_Denseg:
        {{
            ///
            /// shortcut: if 'num_ident' is present, we trust it
            ///
            int num_ident = 0;
            if (align.GetNamedScore("num_ident", num_ident)) {
                size_t len     = s_GetAlignmentLength(align, true);
                *identities += num_ident;
                *mismatches += (len - num_ident);
                break;
            }

            const CDense_seg& ds = align.GetSegs().GetDenseg();
            CAlnVec vec(ds, scope);
            for (int seg = 0;  seg < vec.GetNumSegs();  ++seg) {
                vector<string> data;
                for (int i = 0;  i < vec.GetNumRows();  ++i) {
                    TSeqPos start = vec.GetStart(i, seg);
                    if (start == (TSeqPos)(-1)) {
                        /// we compute ungapped identities
                        /// gap on at least one row, so we skip this segment
                        break;
                    }
                    TSeqPos stop  = vec.GetStop(i, seg);
                    if (start == stop) {
                        break;
                    }

                    data.push_back(string());
                    vec.GetSeqString(data.back(), i, start, stop);
                }

                if (data.size() == (size_t)ds.GetDim()) {
                    for (size_t a = 0;  a < data[0].size();  ++a) {
                        bool is_mismatch = false;
                        for (size_t b = 1;  b < data.size();  ++b) {
                            if (data[b][a] != data[0][a]) {
                                is_mismatch = true;
                                break;
                            }
                        }

                        if (is_mismatch) {
                            ++(*mismatches);
                        } else {
                            ++(*identities);
                        }
                    }
                }
            }
        }}
        break;

    case CSeq_align::TSegs::e_Disc:
        {{
            ITERATE (CSeq_align::TSegs::TDisc::Tdata, iter,
                     align.GetSegs().GetDisc().Get()) {
                s_GetCountIdentityMismatch(scope, **iter,
                                           identities, mismatches);
            }
        }}
        break;

    case CSeq_align::TSegs::e_Std:
        {{
            ///
            /// shortcut: if 'num_ident' is present, we trust it
            ///
            int num_ident = 0;
            if (align.GetNamedScore("num_ident", num_ident)) {
                size_t len     = s_GetAlignmentLength(align, true);
                *identities = num_ident;
                *mismatches = (len - num_ident);
                break;
            }
         }}

        NCBI_THROW(CException, eUnknown,
                   "identity + mismatch function unimplemented for std-seg");
        break;

    case CSeq_align::TSegs::e_Spliced:
        {{
             ITERATE (CSpliced_seg::TExons, iter,
                      align.GetSegs().GetSpliced().GetExons()) {
                 const CSpliced_exon& exon = **iter;
                 if (exon.IsSetParts()) {
                     ITERATE (CSpliced_exon::TParts, it, exon.GetParts()) {
                         const CSpliced_exon_chunk& chunk = **it;
                         switch (chunk.Which()) {
                         case CSpliced_exon_chunk::e_Match:
                             *identities += chunk.GetMatch();
                             break;
                         case CSpliced_exon_chunk::e_Mismatch:
                             *mismatches += chunk.GetMismatch();
                             break;

                         default:
                             break;
                         }
                     }
                 } else {
                     *identities +=
                         exon.GetGenomic_end() - exon.GetGenomic_start() + 1;
                 }
             }
         }}
        break;

    default:
        _ASSERT(false);
        break;
    }
}



///
/// calculate the percent identity
/// we also return the count of identities and mismatches
///
static void s_GetPercentIdentity(CScope& scope, const CSeq_align& align,
                                 int* identities,
                                 int* mismatches,
                                 double* pct_identity)
{
    size_t count_aligned = s_GetAlignmentLength(align, true /* ungapped */);
    s_GetCountIdentityMismatch(scope, align, identities, mismatches);
    if (count_aligned) {
        *pct_identity = 100.0f * double(*identities) / count_aligned;
    } else {
        *pct_identity = 0;
    }
}




///
/// calculate the length of our alignment
///
static size_t s_GetCoveredBases(const CSeq_align& align,
                                int row)
{
    size_t len = 0;
    switch (align.GetSegs().Which()) {
    case CSeq_align::TSegs::e_Denseg:
        {{
             const CDense_seg& ds = align.GetSegs().GetDenseg();
             for (CDense_seg::TNumseg i = 0;  i < ds.GetNumseg();  ++i) {
                 int count_covered = 0;
                 for (CDense_seg::TDim d = 0; d < ds.GetDim(); d++) {
                     if (ds.GetStarts()[(i * ds.GetDim()) + d] != -1) {
                         ++count_covered;
                     }
                 }
                 if (ds.GetStarts()[(i * ds.GetDim()) + row] != -1  &&
                     count_covered >= 2) {
                     len += ds.GetLens()[i];
                 }
             }
         }}
        break;

    case CSeq_align::TSegs::e_Disc:
        ITERATE (CSeq_align::TSegs::TDisc::Tdata, iter,
                 align.GetSegs().GetDisc().Get()) {
            len += s_GetCoveredBases(**iter, row);
        }
        break;

    case CSeq_align::TSegs::e_Std:
        {{
             ITERATE (CSeq_align::TSegs::TStd, iter, align.GetSegs().GetStd()) {
                 const CStd_seg& seg = **iter;

                 const CSeq_loc& loc = *seg.GetLoc()[row];
                 if (loc.IsEmpty()) {
                     /// skip
                 } else {
                     len += sequence::GetLength(loc, NULL);
                 }
             }
         }}
        break;

    case CSeq_align::TSegs::e_Spliced:
        {{
            ITERATE (CSpliced_seg::TExons, iter,
                     align.GetSegs().GetSpliced().GetExons()) {
                const CSpliced_exon& exon = **iter;
                if (exon.IsSetParts()) {
                    ITERATE (CSpliced_exon::TParts, it, exon.GetParts()) {
                        const CSpliced_exon_chunk& chunk = **it;
                        if (chunk.IsMatch()) {
                            len += chunk.GetMatch();
                        } else if (chunk.IsMismatch()) {
                            len += chunk.GetMismatch();
                        } else if (chunk.IsDiag()) {
                            len += chunk.GetDiag();
                        } else if (row == 0  &&  chunk.IsProduct_ins()) {
                            len += chunk.GetProduct_ins();
                        } else if (row == 1  &&  chunk.IsGenomic_ins()) {
                            len += chunk.GetGenomic_ins();
                        }
                    }
                } else if (row == 0) {
                    if (exon.GetProduct_end().IsNucpos()) {
                        len += exon.GetProduct_end().GetNucpos() -
                            exon.GetProduct_start().GetNucpos() + 1;
                    } else {
                        len += exon.GetProduct_end().GetProtpos().GetAmin() -
                            exon.GetProduct_start().GetProtpos().GetAmin() + 1;
                    }
                } else if (row == 1) {
                    len += exon.GetGenomic_end() - exon.GetGenomic_start() + 1;
                }
            }
        }}
        break;

    default:
        _ASSERT(false);
        break;
    }

    return len;
}


///
/// calculate the percent coverage
///
static void s_GetPercentCoverage(CScope& scope, const CSeq_align& align,
                                 double* pct_coverage)
{
    size_t covered_bases = s_GetCoveredBases(align, 0);
    size_t seq_len = 0;
    if (align.GetSegs().IsSpliced()  &&
        align.GetSegs().GetSpliced().IsSetPoly_a()) {
        seq_len = align.GetSegs().GetSpliced().GetPoly_a();

        if (align.GetSegs().GetSpliced().IsSetProduct_strand()  &&
            align.GetSegs().GetSpliced().GetProduct_strand() == eNa_strand_minus) {
            if (align.GetSegs().GetSpliced().IsSetProduct_length()) {
                seq_len = align.GetSegs().GetSpliced().GetProduct_length() - seq_len;
            } else {
                CBioseq_Handle bsh = scope.GetBioseqHandle(align.GetSeq_id(0));
                seq_len = bsh.GetBioseqLength() - seq_len;
            }
        }
    } else {
        CBioseq_Handle bsh = scope.GetBioseqHandle(align.GetSeq_id(0));
        seq_len = bsh.GetBioseqLength();
    }

    if (covered_bases) {
        *pct_coverage = 100.0f * double(covered_bases) / double(seq_len);
    } else {
        *pct_coverage = 0;
    }
}


/////////////////////////////////////////////////////////////////////////////


double CScoreBuilder::GetPercentIdentity(CScope& scope,
                                         const CSeq_align& align)
{
    int identities = 0;
    int mismatches = 0;
    double pct_identity = 0;
    s_GetPercentIdentity(scope, align, &identities, &mismatches, &pct_identity);
    return pct_identity;
}


double CScoreBuilder::GetPercentCoverage(CScope& scope,
                                         const CSeq_align& align)
{
    double pct_coverage = 0;
    s_GetPercentCoverage(scope, align, &pct_coverage);
    return pct_coverage;
}


int CScoreBuilder::GetIdentityCount(CScope& scope, const CSeq_align& align)
{
    int identities = 0;
    int mismatches = 0;
    s_GetCountIdentityMismatch(scope, align, &identities, &mismatches);
    return identities;
}


int CScoreBuilder::GetMismatchCount(CScope& scope, const CSeq_align& align)
{
    int identities = 0;
    int mismatches = 0;
    s_GetCountIdentityMismatch(scope, align, &identities,&mismatches);
    return mismatches;
}


void CScoreBuilder::GetMismatchCount(CScope& scope, const CSeq_align& align,
                                     int& identities, int& mismatches)
{
    identities = 0;
    mismatches = 0;
    s_GetCountIdentityMismatch(scope, align, &identities, &mismatches);
}


int CScoreBuilder::GetGapBaseCount(const CSeq_align& align)
{
    return align.GetTotalGapCount();
}


int CScoreBuilder::GetGapCount(const CSeq_align& align)
{
    return align.GetNumGapOpenings();
}


TSeqPos CScoreBuilder::GetAlignLength(const CSeq_align& align, bool ungapped)
{
    return s_GetAlignmentLength(align, ungapped);
}


/////////////////////////////////////////////////////////////////////////////

static const unsigned char reverse_4na[16] = {0, 8, 4, 0, 2, 0, 0, 0, 1};

int CScoreBuilder::GetBlastScore(CScope& scope,
                                 const CSeq_align& align)
{
    if ( !align.GetSegs().IsDenseg() ) {
        NCBI_THROW(CException, eUnknown,
            "CScoreBuilder::GetBlastScore(): "
            "only dense-seg alignments are supported");
    }

    if (m_ScoreBlk == 0) {
        NCBI_THROW(CException, eUnknown,
               "Blast scoring parameters have not been specified");
    }

    int computed_score = 0;
    const CDense_seg& ds = align.GetSegs().GetDenseg();
    CAlnVec vec(ds, scope);
    CBioseq_Handle bsh1 = vec.GetBioseqHandle(0);
    CBioseq_Handle bsh2 = vec.GetBioseqHandle(1);
    CSeqVector vec1(bsh1);
    CSeqVector vec2(bsh2);
    CSeq_inst::TMol mol1 = vec1.GetSequenceType();
    CSeq_inst::TMol mol2 = vec2.GetSequenceType();

    int gap_open = m_GapOpen;
    int gap_extend = m_GapExtend;

    if (mol1 == CSeq_inst::eMol_aa && mol2 == CSeq_inst::eMol_aa) {

        Int4 **matrix = m_ScoreBlk->matrix->data;

        if (m_BlastType != eBlastp) {
            NCBI_THROW(CException, eUnknown,
                        "Protein scoring parameters required");
        }

        for (CAlnVec::TNumseg seg_idx = 0;
                        seg_idx < vec.GetNumSegs();  ++seg_idx) {

            TSignedSeqPos start1 = vec.GetStart(0, seg_idx);
            TSignedSeqPos start2 = vec.GetStart(1, seg_idx);
            TSeqPos seg_len = vec.GetLen(seg_idx);

            if (start1 == -1 || start2 == -1) {
                computed_score -= gap_open + gap_extend * seg_len;
                continue;
            }

            // @todo FIXME the following assumes ncbistdaa format

            for (TSeqPos pos = 0;  pos < seg_len;  ++pos) {
                unsigned char c1 = vec1[start1 + pos];
                unsigned char c2 = vec2[start2 + pos];
                computed_score += matrix[c1][c2];
            }
        }
    }
    else if (CSeq_inst::IsNa(mol1) && CSeq_inst::IsNa(mol2)) {

        int match = m_ScoreBlk->reward;
        int mismatch = m_ScoreBlk->penalty; // assumed negative

        if (m_BlastType != eBlastn) {
            NCBI_THROW(CException, eUnknown,
                        "Nucleotide scoring parameters required");
        }

        bool scaled_up = false;
        if (gap_open == 0 && gap_extend == 0) { // possible with megablast
            match *= 2;
            mismatch *= 2;
            gap_extend = match / 2 - mismatch;
            scaled_up = true;
        }

        int strand1 = vec.StrandSign(0);
        int strand2 = vec.StrandSign(1);

        for (CAlnVec::TNumseg seg_idx = 0;
                        seg_idx < vec.GetNumSegs();  ++seg_idx) {

            TSignedSeqPos start1 = vec.GetStart(0, seg_idx);
            TSignedSeqPos start2 = vec.GetStart(1, seg_idx);
            TSeqPos seg_len = vec.GetLen(seg_idx);

            if (start1 == -1 || start2 == -1) {
                computed_score -= gap_open + gap_extend * seg_len;
                continue;
            }

            // @todo FIXME encoding assumed to be ncbi4na, without
            // ambiguity charaters

            if (strand1 > strand2) {
                for (TSeqPos pos = 0;  pos < seg_len;  ++pos) {
                    unsigned char c1 = vec1[start1 + pos];
                    unsigned char c2 = vec2[start2 + seg_len - 1 - pos];
                    computed_score +=  (c1 == reverse_4na[c2]) ?
                                        match : mismatch;
                }
            }
            else if (strand1 < strand2) {
                for (TSeqPos pos = 0;  pos < seg_len;  ++pos) {
                    unsigned char c1 = vec1[start1 + seg_len - 1 - pos];
                    unsigned char c2 = vec2[start2 + pos];
                    computed_score += (reverse_4na[c1] == c2) ?
                                        match : mismatch;
                }
            }
            else {
                for (TSeqPos pos = 0;  pos < seg_len;  ++pos) {
                    unsigned char c1 = vec1[start1 + pos];
                    unsigned char c2 = vec2[start2 + pos];
                    computed_score += (c1 == c2) ? match : mismatch;
                }
            }
        }

        if (scaled_up)
            computed_score /= 2;
    }
    else {
        NCBI_THROW(CException, eUnknown,
                    "pairwise alignment contains unsupported "
                    "or mismatched molecule types");
    }

    return computed_score;
}


double CScoreBuilder::GetBlastBitScore(CScope& scope,
                                       const CSeq_align& align)
{
    int raw_score = GetBlastScore(scope, align);
    Blast_KarlinBlk *kbp = m_ScoreBlk->kbp_gap_std[0];

    if (m_BlastType == eBlastn && m_ScoreBlk->round_down)
        raw_score &= ~1;

    return (raw_score * kbp->Lambda - kbp->logK) / NCBIMATH_LN2;
}


double CScoreBuilder::GetBlastEValue(CScope& scope,
                                     const CSeq_align& align)
{
    if (m_EffectiveSearchSpace == 0) {
        NCBI_THROW(CException, eUnknown,
               "E-value calculation requires search space to be specified");
    }

    int raw_score = GetBlastScore(scope, align);
    Blast_KarlinBlk *kbp = m_ScoreBlk->kbp_gap_std[0];

    if (m_BlastType == eBlastn && m_ScoreBlk->round_down)
        raw_score &= ~1;

    return BLAST_KarlinStoE_simple(raw_score, kbp, m_EffectiveSearchSpace);
}

/////////////////////////////////////////////////////////////////////////////

void CScoreBuilder::AddScore(CScope& scope, CSeq_align& align,
                             EScoreType score)
{
    switch (score) {
    case eScore_Blast:
        {{
            align.SetNamedScore(CSeq_align::eScore_Score,
                                GetBlastScore(scope, align));
        }}
        break;

    case eScore_Blast_BitScore:
        {{
            align.SetNamedScore(CSeq_align::eScore_BitScore,
                                GetBlastBitScore(scope, align));
        }}
        break;

    case eScore_Blast_EValue:
        {{
            align.SetNamedScore(CSeq_align::eScore_EValue,
                                GetBlastEValue(scope, align));
        }}
        break;

    case eScore_MismatchCount:
        {{
            int identities = 0;
            int mismatches = 0;
            s_GetCountIdentityMismatch(scope, align, &identities, &mismatches);
            align.SetNamedScore(CSeq_align::eScore_MismatchCount, mismatches);
        }}
        break;

    case eScore_IdentityCount:
        {{
            int identities = 0;
            int mismatches = 0;
            s_GetCountIdentityMismatch(scope, align, &identities, &mismatches);
            align.SetNamedScore(CSeq_align::eScore_IdentityCount, identities);
        }}
        break;

    case eScore_PercentIdentity:
        {{
            int identities      = 0;
            int mismatches      = 0;
            double pct_identity = 0;
            s_GetPercentIdentity(scope, align,
                                 &identities, &mismatches, &pct_identity);
            align.SetNamedScore(CSeq_align::eScore_PercentIdentity, pct_identity);
            align.SetNamedScore(CSeq_align::eScore_IdentityCount,   identities);
            align.SetNamedScore(CSeq_align::eScore_MismatchCount,   mismatches);
        }}
        break;
    case eScore_PercentCoverage:
        {{
            double pct_coverage = 0;
            s_GetPercentCoverage(scope, align, &pct_coverage);
            align.SetNamedScore("pct_coverage", pct_coverage);
        }}
        break;
    }
}


void CScoreBuilder::AddScore(CScope& scope,
                             list< CRef<CSeq_align> >& aligns, EScoreType score)
{
    NON_CONST_ITERATE (list< CRef<CSeq_align> >, iter, aligns) {
        try {
            AddScore(scope, **iter, score);
        }
        catch (CException& e) {
            LOG_POST(Error
                << "CScoreBuilder::AddScore(): error computing score: "
                << e.GetMsg());
        }
    }
}





END_NCBI_SCOPE
