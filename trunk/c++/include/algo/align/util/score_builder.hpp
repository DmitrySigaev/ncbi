#ifndef ALGO_ALIGN_UTIL___SCORE_BUILDER__HPP
#define ALGO_ALIGN_UTIL___SCORE_BUILDER__HPP

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

#include <corelib/ncbiobj.hpp>
#include <algo/blast/api/blast_types.hpp>

struct BlastScoreBlk; // C structure

BEGIN_NCBI_SCOPE

BEGIN_SCOPE(blast)
    class CBlastOptionsHandle;
END_SCOPE(blast)

BEGIN_SCOPE(objects)

class CScope;
class CSeq_align;

class NCBI_XALGOALIGN_EXPORT CScoreBuilder
{
public:

    CScoreBuilder();
    CScoreBuilder(enum blast::EProgram program_type);
    CScoreBuilder(blast::CBlastOptionsHandle& options);
    ~CScoreBuilder();

    enum EScoreType {
        //< typical blast 'score'
        eScore_Blast,

        //< blast 'bit_score' score
        eScore_Blast_BitScore,

        //< blast 'e_value' score
        eScore_Blast_EValue,

        //< count of ungapped identities as 'num_ident'
        eScore_IdentityCount,

        //< count of ungapped identities as 'num_mismatch'
        eScore_MismatchCount,

        //< percent identity as 'pct_identity', range 0.0-100.0
        //< this will also create 'num_ident' and 'num_mismatch'
        eScore_PercentIdentity,

        //< percent coverage of query as 'pct_coverage', range 0.0-100.0
        eScore_PercentCoverage
    };

    /// @name Functions to add scores directly to Seq-aligns
    /// @{

    void AddScore(CScope& scope, CSeq_align& align,
                  EScoreType score);
    void AddScore(CScope& scope, list< CRef<CSeq_align> >& aligns,
                  EScoreType score);

    /// @}

    /// @name Functions to compute scores without adding
    /// @{

    /// Compute ungapped percent identity (range 0-100)
    double GetPercentIdentity(CScope& scope, const CSeq_align& align);

    /// Compute percent coverage of the query (sequence 0) (range 0-100)
    double GetPercentCoverage(CScope& scope, const CSeq_align& align);

    /// Compute the number of identities in the alignment
    int GetIdentityCount  (CScope& scope, const CSeq_align& align);

    /// Compute the number of mismatches in the alignment
    int GetMismatchCount  (CScope& scope, const CSeq_align& align);
    void GetMismatchCount  (CScope& scope, const CSeq_align& align,
                            int& identities, int& mismatches);

    /// Compute the BLAST score of the alignment
    int GetBlastScore     (CScope& scope, const CSeq_align& align);

    /// Compute the BLAST bit score
    double GetBlastBitScore(CScope& scope, const CSeq_align& align);

    /// Compute the BLAST e-value
    double GetBlastEValue (CScope& scope, const CSeq_align& align);

    /// Compute the number of gaps in the alignment
    int GetGapCount       (const CSeq_align& align);

    /// Compute the number of gap bases in the alignment (= length of all gap
    /// segments)
    int GetGapBaseCount   (const CSeq_align& align);

    /// Compute the length of the alignment (= length of all segments, gaps +
    /// aligned)
    TSeqPos GetAlignLength(const CSeq_align& align, bool ungapped=false);

    /// @}

    /// @name Functions for configuring blast scores
    /// @{

    void SetEffectiveSearchSpace(Int8 searchsp) // required for blast e-values
    {
        m_EffectiveSearchSpace = searchsp;
    }

    /// @}

private:

    struct BlastScoreBlk *m_ScoreBlk;
    enum blast::EProgram m_BlastType;
    int m_GapOpen;
    int m_GapExtend;
    Int8 m_EffectiveSearchSpace;

    void x_Initialize(blast::CBlastOptionsHandle& options);
};



END_SCOPE(objects)
END_NCBI_SCOPE

#endif  // ALGO_ALIGN_UTIL___SCORE_BUILDER__HPP
