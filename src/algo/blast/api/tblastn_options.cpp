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

/// @file tblastn_options.cpp
/// Implements the CTBlastnOptionsHandle class.

#include <algo/blast/api/tblastn_options.hpp>
#include "blast_setup.hpp"

/** @addtogroup AlgoBlast
 *
 * @{
 */


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(blast)

CTBlastnOptionsHandle::CTBlastnOptionsHandle(EAPILocality locality)
    : CBlastProteinOptionsHandle(locality)
{
    if (m_Opts->GetLocality() == CBlastOptions::eRemote) {
        return;
    }
    SetDefaults();
    m_Opts->SetProgram(eTblastn);
}

void 
CTBlastnOptionsHandle::SetLookupTableDefaults()
{
    CBlastProteinOptionsHandle::SetLookupTableDefaults();
    SetWordThreshold(BLAST_WORD_THRESHOLD_TBLASTN);
}

void
CTBlastnOptionsHandle::SetScoringOptionsDefaults()
{
    CBlastProteinOptionsHandle::SetScoringOptionsDefaults();
}

void
CTBlastnOptionsHandle::SetHitSavingOptionsDefaults()
{
    CBlastProteinOptionsHandle::SetHitSavingOptionsDefaults();
    m_Opts->SetSumStatisticsMode();
}

void
CTBlastnOptionsHandle::SetSubjectSequenceOptionsDefaults()
{
    SetDbGeneticCode(BLAST_GENETIC_CODE);
}

END_SCOPE(blast)
END_NCBI_SCOPE


/* @} */


/*
 * ===========================================================================
 * $Log$
 * Revision 1.4  2004/03/19 15:13:34  camacho
 * Move to doxygen group AlgoBlast
 *
 * Revision 1.3  2004/01/16 21:54:51  bealer
 * - Blast4 API changes.
 *
 * Revision 1.2  2003/12/03 16:42:33  dondosha
 * SetDbGeneticCode takes care of setting both integer and string now
 *
 * Revision 1.1  2003/11/26 18:24:01  camacho
 * +Blast Option Handle classes
 *
 * ===========================================================================
 */
