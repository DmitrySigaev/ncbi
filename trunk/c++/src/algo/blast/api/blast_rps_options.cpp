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
 * Authors:  Tom Madden
 *
 */

/// @file blast_rps_options.cpp
/// Implements the CBlastRPSOptionsHandle class.

#include <algo/blast/api/blast_rps_options.hpp>
#include <objects/seqloc/Na_strand.hpp>
#include "blast_setup.hpp"


/** @addtogroup AlgoBlast
 *
 * @{
 */


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(blast)

CBlastRPSOptionsHandle::CBlastRPSOptionsHandle(EAPILocality locality)
    : CBlastProteinOptionsHandle(locality)
{
    if (m_Opts->GetLocality() == CBlastOptions::eRemote) {
        return;
    }
    SetDefaults();
    m_Opts->SetProgram(eRPSBlast);
}

void
CBlastRPSOptionsHandle::SetLookupTableDefaults()
{
    m_Opts->SetLookupTableType(RPS_LOOKUP_TABLE);
}

void
CBlastRPSOptionsHandle::SetQueryOptionDefaults()
{
    SetFilterString("F");
    m_Opts->SetStrandOption(objects::eNa_strand_unknown);
}

END_SCOPE(blast)
END_NCBI_SCOPE


/* @} */


/*
 * ===========================================================================
 * $Log$
 * Revision 1.3  2004/04/16 14:27:47  papadopo
 * make this class a derived class of CBlastProteinOptionsHandle, that corresponds to the eRPSBlast program
 *
 * Revision 1.2  2004/03/19 15:13:34  camacho
 * Move to doxygen group AlgoBlast
 *
 * Revision 1.1  2004/03/10 14:52:13  madden
 * Options handle for RPSBlast searches
 *
 *
 * ===========================================================================
 */
