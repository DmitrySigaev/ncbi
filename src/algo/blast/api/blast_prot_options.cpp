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
 * Authors:  Christiam Camacho
 *
 */

/// @file blast_prot_options.cpp
/// Implements the CBlastProteinOptionsHandle class.

#include <algo/blast/core/blast_encoding.h>
#include <algo/blast/api/blast_prot_options.hpp>
#include <objects/seqloc/Na_strand.hpp>
#include "blast_setup.hpp"


/** @addtogroup AlgoBlast
 *
 * @{
 */


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(blast)

CBlastProteinOptionsHandle::CBlastProteinOptionsHandle(EAPILocality locality)
    : CBlastOptionsHandle(locality)
{
    SetDefaults();
    m_Opts->SetProgram(eBlastp);
}

void 
CBlastProteinOptionsHandle::SetLookupTableDefaults()
{
    m_Opts->SetLookupTableType(AA_LOOKUP_TABLE);
    SetWordSize(BLAST_WORDSIZE_PROT);
    SetWordThreshold(BLAST_WORD_THRESHOLD_BLASTP);
    SetAlphabetSize(BLASTAA_SIZE);
}

void
CBlastProteinOptionsHandle::SetQueryOptionDefaults()
{
    SetFilterString("S");
    m_Opts->SetStrandOption(objects::eNa_strand_unknown);
}

void
CBlastProteinOptionsHandle::SetInitialWordOptionsDefaults()
{
    SetXDropoff(BLAST_UNGAPPED_X_DROPOFF_PROT);
    SetWindowSize(BLAST_WINDOW_SIZE_PROT);
    // FIXME: extend_word_method is missing
    m_Opts->SetUngappedExtension();
}

void
CBlastProteinOptionsHandle::SetGappedExtensionDefaults()
{
    SetGapXDropoff(BLAST_GAP_X_DROPOFF_PROT);
    SetGapXDropoffFinal(BLAST_GAP_X_DROPOFF_FINAL_PROT);
    SetGapTrigger(BLAST_GAP_TRIGGER_PROT);
    m_Opts->SetGapExtnAlgorithm(EXTEND_DYN_PROG);
}


void
CBlastProteinOptionsHandle::SetScoringOptionsDefaults()
{
    const char * mname = "BLOSUM62";
    SetMatrixName(mname);
    SetMatrixPath(FindMatrixPath(mname, true).c_str());
    SetGapOpeningCost(BLAST_GAP_OPEN_PROT);
    SetGapExtensionCost(BLAST_GAP_EXTN_PROT);
    SetGappedMode();

    // set invalid values for options that are not applicable
    m_Opts->SetOutOfFrameMode(false);
    m_Opts->SetFrameShiftPenalty(INT2_MAX);
    m_Opts->SetDecline2AlignPenalty(INT2_MAX);
    //m_Opts->SetMatchReward(0);
    //m_Opts->SetMismatchPenalty(0);
}

void
CBlastProteinOptionsHandle::SetHitSavingOptionsDefaults()
{
    SetHitlistSize(500);
    SetPrelimHitlistSize(550);
    SetEvalueThreshold(BLAST_EXPECT_VALUE);
    SetPercentIdentity(0);
    m_Opts->SetSumStatisticsMode(false);
    // set some default here, allow INT4MAX to mean infinity
    SetMaxNumHspPerSequence(0); 
    // this is never used... altough it could be calculated
    //SetTotalHspLimit(FIXME);

    SetCutoffScore(0); // will be calculated based on evalue threshold,
    // effective lengths and Karlin-Altschul params in BLAST_Cutoffs_simple
    // and passed to the engine in the params structure

    // not applicable
    m_Opts->SetRequiredStart(0);
    m_Opts->SetRequiredEnd(0);
}

void
CBlastProteinOptionsHandle::SetEffectiveLengthsOptionsDefaults()
{
    SetDbLength(0);
    SetDbSeqNum(1);
    SetEffectiveSearchSpace(0);
    SetUseRealDbSize();
}

void
CBlastProteinOptionsHandle::SetSubjectSequenceOptionsDefaults()
{}

END_SCOPE(blast)
END_NCBI_SCOPE


/* @} */


/*
 * ===========================================================================
 * $Log$
 * Revision 1.7  2004/04/07 03:06:15  camacho
 * Added blast_encoding.[hc], refactoring blast_stat.[hc]
 *
 * Revision 1.6  2004/03/19 15:13:34  camacho
 * Move to doxygen group AlgoBlast
 *
 * Revision 1.5  2004/02/18 00:35:48  dondosha
 * Typo fix in previous commit
 *
 * Revision 1.4  2004/02/17 23:53:31  dondosha
 * Added setting of preliminary hitlist size
 *
 * Revision 1.3  2004/01/17 02:38:30  ucko
 * Initialize variables with = rather than () when possible to avoid
 * confusing MSVC's parser.
 *
 * Revision 1.2  2004/01/16 21:49:26  bealer
 * - Add locality flag for Blast4 API
 *
 * Revision 1.1  2003/11/26 18:24:00  camacho
 * +Blast Option Handle classes
 *
 * ===========================================================================
 */
