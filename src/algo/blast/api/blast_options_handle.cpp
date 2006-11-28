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

/// @file blast_options_handle.cpp
/// Implementation for the CBlastOptionsHandle and the
/// CBlastOptionsFactory classes.

#include <ncbi_pch.hpp>
#include <algo/blast/api/blast_exception.hpp>
#include <algo/blast/api/blast_options_handle.hpp>
#include <algo/blast/api/blast_prot_options.hpp>
#include <algo/blast/api/blastx_options.hpp>
#include <algo/blast/api/tblastn_options.hpp>
#include <algo/blast/api/rpstblastn_options.hpp>
#include <algo/blast/api/tblastx_options.hpp>
#include <algo/blast/api/blast_nucl_options.hpp>
#include <algo/blast/api/disc_nucl_options.hpp>
#include <algo/blast/api/blast_rps_options.hpp>
#include <algo/blast/api/blast_advprot_options.hpp>
#include <algo/blast/api/psiblast_options.hpp>
#include <algo/blast/api/phiblast_nucl_options.hpp>
#include <algo/blast/api/phiblast_prot_options.hpp>

/** @addtogroup AlgoBlast
 *
 * @{
 */

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(blast)

CBlastOptionsHandle::CBlastOptionsHandle(EAPILocality locality)
    : m_DefaultsMode(false)
{
    m_Opts.Reset(new CBlastOptions(locality));
}

void
CBlastOptionsHandle::SetDefaults()
{
    if (m_Opts->GetLocality() != CBlastOptions::eRemote) {
        m_Opts->SetDefaultsMode(true);
        SetLookupTableDefaults();
        SetQueryOptionDefaults();
        SetInitialWordOptionsDefaults();
        SetGappedExtensionDefaults();
        SetScoringOptionsDefaults();
        SetHitSavingOptionsDefaults();
        SetEffectiveLengthsOptionsDefaults();
        SetSubjectSequenceOptionsDefaults();
        m_Opts->SetDefaultsMode(false);
    }
    SetRemoteProgramAndService_Blast3();
}

bool
CBlastOptionsHandle::Validate() const
{
    return m_Opts->Validate();
}

CBlastOptionsHandle*
CBlastOptionsFactory::Create(EProgram program, EAPILocality locality)
{
    CBlastOptionsHandle* retval = NULL;

    switch (program) {
    case eBlastn:
	{
		CBlastNucleotideOptionsHandle* opts = 
			new CBlastNucleotideOptionsHandle(locality);
		opts->SetTraditionalBlastnDefaults();
		retval = opts;
        break;
	}

    case eBlastp:
        retval = new CBlastAdvancedProteinOptionsHandle(locality);
        break;

    case eBlastx:
        retval = new CBlastxOptionsHandle(locality);
        break;

    case eTblastn:
        retval = new CTBlastnOptionsHandle(locality);
        break;

    case eTblastx:
        retval = new CTBlastxOptionsHandle(locality);
        break;

    case eMegablast:
	{
		CBlastNucleotideOptionsHandle* opts = 
			new CBlastNucleotideOptionsHandle(locality);
		opts->SetTraditionalMegablastDefaults();
		retval = opts;
        break;
	}

    case eDiscMegablast:
        retval = new CDiscNucleotideOptionsHandle(locality);
        break;

    case eRPSBlast:
        retval = new CBlastRPSOptionsHandle(locality);
        break;

    case eRPSTblastn:
        retval = new CRPSTBlastnOptionsHandle(locality);
        break;
        
    case ePSIBlast:
        retval = new CPSIBlastOptionsHandle(locality);
        break;

    case ePHIBlastp:
        retval = new CPHIBlastProtOptionsHandle(locality);
        break;        

    case ePHIBlastn:
        retval = new CPHIBlastNuclOptionsHandle(locality);
        break;

    case eBlastNotSet:
        NCBI_THROW(CBlastException, eInvalidArgument,
                   "eBlastNotSet may not be used as argument");
        break;

    default:
        abort();    // should never happen
    }

    return retval;
}

END_SCOPE(blast)
END_NCBI_SCOPE


/* @} */


/*
 * ===========================================================================
 * $Log$
 * Revision 1.18  2006/11/28 13:28:31  madden
 * Throw exception in Create if eBlastNotSet passed in
 *
 * Revision 1.17  2006/11/02 18:14:47  madden
 * For eBlastp create CBlastAdvancedProteinOptionsHandle
 *
 * Revision 1.16  2006/05/08 16:48:02  bealer
 * - Defaults mode / eBoth changes.
 *
 * Revision 1.15  2005/09/28 18:23:07  camacho
 * Rearrangement of headers/functions to segregate object manager dependencies.
 *
 * Revision 1.14  2005/07/07 16:32:11  camacho
 * Revamping of BLAST exception classes and error codes
 *
 * Revision 1.13  2005/07/06 17:47:50  camacho
 * Doxygen and other minor fixes
 *
 * Revision 1.12  2005/05/26 14:36:00  dondosha
 * Added cases for PHI BLAST options handles
 *
 * Revision 1.11  2005/05/24 14:08:11  madden
 * Add include for blast_advprot_options.hpp
 *
 * Revision 1.10  2005/05/09 20:08:48  bealer
 * - Add program and service strings to CBlastOptions for remote blast.
 * - New CBlastOptionsHandle constructor for CRemoteBlast.
 * - Prohibit copy construction/assignment for CRemoteBlast.
 * - Code in each BlastOptionsHandle derived class to set program+service.
 *
 * Revision 1.9  2005/03/31 13:45:35  camacho
 * BLAST options API clean-up
 *
 * Revision 1.8  2004/05/21 21:41:02  gorelenk
 * Added PCH ncbi_pch.hpp
 *
 * Revision 1.7  2004/05/17 20:19:55  ucko
 * Trivial compilation fixes.
 *
 * Revision 1.6  2004/05/17 19:42:23  bealer
 * - Add PSIBlast to factory.
 *
 * Revision 1.5  2004/04/16 14:26:39  papadopo
 * add handle construction for translated RPS blast
 *
 * Revision 1.4  2004/03/17 19:40:06  camacho
 * Correct @file doxygen directive
 *
 * Revision 1.3  2004/03/10 14:53:06  madden
 * Add case of eRPSBlast
 *
 * Revision 1.2  2004/01/16 21:51:35  bealer
 * - Changes for Blast4 API
 *
 * Revision 1.1  2003/11/26 18:23:59  camacho
 * +Blast Option Handle classes
 *
 * ===========================================================================
 */
