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

/// @file blast_options_factory.cpp

#include <algo/blast/api/blast_nucl_options.hpp>
#include <objects/seqloc/Na_strand.hpp>
#include "blast_setup.hpp"

/** @addtogroup Miscellaneous
 *
 * @{
 */


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(blast)

CBlastNucleotideOptionsHandle::CBlastNucleotideOptionsHandle()
{
    SetDefaults();
}

void
CBlastNucleotideOptionsHandle::SetDefaults()
{
    SetTraditionalMegablastDefaults();
    m_Opts->SetProgram(eBlastn);
}

void
CBlastNucleotideOptionsHandle::SetTraditionalBlastnDefaults()
{
    SetLookupTableDefaults();
    SetQueryOptionDefaults();
    SetInitialWordOptionsDefaults();
    SetGappedExtensionDefaults();
    SetScoringOptionsDefaults();
    SetHitSavingOptionsDefaults();
    SetEffectiveLengthsOptionsDefaults();
}

void
CBlastNucleotideOptionsHandle::SetTraditionalMegablastDefaults()
{
    SetMBLookupTableDefaults();
    SetQueryOptionDefaults();
    SetMBInitialWordOptionsDefaults();
    SetGappedExtensionDefaults();
    SetMBScoringOptionsDefaults();
    SetHitSavingOptionsDefaults();
    SetEffectiveLengthsOptionsDefaults();
}

void 
CBlastNucleotideOptionsHandle::SetLookupTableDefaults()
{
    SetLookupTableType(NA_LOOKUP_TABLE);
    SetWordSize(BLAST_WORDSIZE_NUCL);
    m_Opts->SetWordThreshold(BLAST_WORD_THRESHOLD_BLASTN);
    SetAlphabetSize(BLASTNA_SIZE);

    unsigned int stride = CalculateBestStride(GetWordSize(), 
                                              GetVariableWordSize(), 
                                              GetLookupTableType());
    SetScanStep(stride);
}

void 
CBlastNucleotideOptionsHandle::SetMBLookupTableDefaults()
{
    SetLookupTableType(MB_LOOKUP_TABLE);
    SetWordSize(BLAST_WORDSIZE_MEGABLAST);
    m_Opts->SetWordThreshold(BLAST_WORD_THRESHOLD_MEGABLAST);
    m_Opts->SetMBMaxPositions(INT4_MAX);
    SetAlphabetSize(BLASTNA_SIZE);

    // Note: stride makes no sense in the context of eRightAndLeft extension 
    // method
    unsigned int stride = CalculateBestStride(GetWordSize(), 
                                              GetVariableWordSize(), 
                                              GetLookupTableType());
    SetScanStep(stride);
}

void
CBlastNucleotideOptionsHandle::SetQueryOptionDefaults()
{
    SetFilterString("D");
    SetStrandOption(objects::eNa_strand_both);
}

void
CBlastNucleotideOptionsHandle::SetInitialWordOptionsDefaults()
{
    SetXDropoff(BLAST_UNGAPPED_X_DROPOFF_NUCL);
    SetWindowSize(BLAST_WINDOW_SIZE_NUCL);
    SetSeedContainerType(eDiagArray);
    SetSeedExtensionMethod(eRightAndLeft);
    SetVariableWordSize(false);
    SetUngappedExtension();
}

void
CBlastNucleotideOptionsHandle::SetMBInitialWordOptionsDefaults()
{
    SetXDropoff(BLAST_UNGAPPED_X_DROPOFF_NUCL);
    SetWindowSize(BLAST_WINDOW_SIZE_NUCL);
    SetSeedContainerType(eMbStacks);
    SetSeedExtensionMethod(eRight);
    SetVariableWordSize(false);
    SetUngappedExtension();
}

void
CBlastNucleotideOptionsHandle::SetGappedExtensionDefaults()
{
    SetGapXDropoff(BLAST_GAP_X_DROPOFF_NUCL);
    SetGapXDropoffFinal(BLAST_GAP_X_DROPOFF_FINAL_NUCL);
    SetGapTrigger(BLAST_GAP_TRIGGER_NUCL);
    SetGapExtnAlgorithm(EXTEND_DYN_PROG);
}


void
CBlastNucleotideOptionsHandle::SetScoringOptionsDefaults()
{
    SetMatrixName(NULL);
    SetMatrixPath(FindMatrixPath(GetMatrixName(), false).c_str());
    SetGapOpeningCost(BLAST_GAP_OPEN_NUCL);
    SetGapExtensionCost(BLAST_GAP_EXTN_NUCL);
    SetMatchReward(BLAST_REWARD);
    SetMismatchPenalty(BLAST_PENALTY);
    SetGappedMode();

    // set out-of-frame options to invalid? values
    m_Opts->SetOutOfFrameMode(false);
    m_Opts->SetFrameShiftPenalty(INT2_MAX);
    m_Opts->SetDecline2AlignPenalty(INT2_MAX);
}

void
CBlastNucleotideOptionsHandle::SetMBScoringOptionsDefaults()
{
    SetMatrixName(NULL);
    SetMatrixPath(FindMatrixPath(GetMatrixName(), false).c_str());
    SetGapOpeningCost(BLAST_GAP_OPEN_MEGABLAST);
    SetGapExtensionCost(BLAST_GAP_EXTN_MEGABLAST);
    SetMatchReward(BLAST_REWARD);
    SetMismatchPenalty(BLAST_PENALTY);
    SetGappedMode();

    // set out-of-frame options to invalid? values
    m_Opts->SetOutOfFrameMode(false);
    m_Opts->SetFrameShiftPenalty(INT2_MAX);
    m_Opts->SetDecline2AlignPenalty(INT2_MAX);
}
void
CBlastNucleotideOptionsHandle::SetHitSavingOptionsDefaults()
{
    SetHitlistSize(500);
    SetEvalueThreshold(BLAST_EXPECT_VALUE);
    SetPercentIdentity(0);
    // set some default here, allow INT4MAX to mean infinity
    SetMaxNumHspPerSequence(0); 
    // this is never used... altough it could be calculated
    //m_Opts->SetTotalHspLimit(FIXME);

    SetCutoffScore(0); // will be calculated based on evalue threshold,
    // effective lengths and Karlin-Altschul params in BLAST_Cutoffs_simple
    // and passed to the engine in the params structure

    // not applicable
    m_Opts->SetRequiredStart(0);
    m_Opts->SetRequiredEnd(0);
}

void
CBlastNucleotideOptionsHandle::SetEffectiveLengthsOptionsDefaults()
{
    SetDbLength(0);
    SetDbSeqNum(1);
    SetEffectiveSearchSpace(0);
    SetUseRealDbSize();
}

void
CBlastNucleotideOptionsHandle::SetSubjectSequenceOptionsDefaults()
{}

END_SCOPE(blast)
END_NCBI_SCOPE


/* @} */


/*
 * ===========================================================================
 * $Log$
 * Revision 1.1  2003/11/26 18:23:59  camacho
 * +Blast Option Handle classes
 *
 * ===========================================================================
 */
