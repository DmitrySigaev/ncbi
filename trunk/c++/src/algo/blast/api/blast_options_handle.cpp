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

#include <algo/blast/api/blast_types.hpp>
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


/** @addtogroup AlgoBlast
 *
 * @{
 */

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(blast)

CBlastOptionsHandle::CBlastOptionsHandle(EAPILocality locality)
{
    m_Opts.Reset(new CBlastOptions(locality));
}

void
CBlastOptionsHandle::SetDefaults()
{
    if (m_Opts->GetLocality() == CBlastOptions::eRemote) {
        return;
    }
    
    SetLookupTableDefaults();
    SetQueryOptionDefaults();
    SetInitialWordOptionsDefaults();
    SetGappedExtensionDefaults();
    SetScoringOptionsDefaults();
    SetHitSavingOptionsDefaults();
    SetEffectiveLengthsOptionsDefaults();
    SetSubjectSequenceOptionsDefaults();
}

CBlastOptionsHandle*
CBlastOptionsFactory::Create(EProgram program, EAPILocality locality) THROWS((CBlastException))
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
        retval = new CBlastProteinOptionsHandle(locality);
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
        
    case ePSIblast:
        retval = new CPSIBlastOptionsHandle(locality);
        break;
        
    default:
        NCBI_THROW(CBlastException, eBadParameter,
        "CBlastOptionFactory cannot construct options handle: invalid program");
    }

    return retval;
}

END_SCOPE(blast)
END_NCBI_SCOPE


/* @} */


/*
 * ===========================================================================
 * $Log$
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
