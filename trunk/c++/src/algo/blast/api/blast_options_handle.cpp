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

#include <algo/blast/api/blast_types.hpp>
#include <algo/blast/api/blast_exception.hpp>
#include <algo/blast/api/blast_options_handle.hpp>
#include <algo/blast/api/blast_prot_options.hpp>
#include <algo/blast/api/blastx_options.hpp>
#include <algo/blast/api/tblastn_options.hpp>
#include <algo/blast/api/tblastx_options.hpp>
#include <algo/blast/api/blast_nucl_options.hpp>
#include <algo/blast/api/disc_nucl_options.hpp>


/** @addtogroup Miscellaneous
 *
 * @{
 */

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(blast)

CBlastOptionsHandle::CBlastOptionsHandle()
{
    m_Opts.Reset(new CBlastOptions);
}

void
CBlastOptionsHandle::SetDefaults()
{
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
CBlastOptionsFactory::Create(EProgram program) THROWS((CBlastException))
{
    CBlastOptionsHandle* retval = NULL;

    switch (program) {
    case eBlastn:
	{
		CBlastNucleotideOptionsHandle* opts = 
			new CBlastNucleotideOptionsHandle();
		opts->SetTraditionalBlastnDefaults();
		retval = opts;
        break;
	}

    case eBlastp:
        retval = new CBlastProteinOptionsHandle();
        break;

    case eBlastx:
        retval = new CBlastxOptionsHandle();
        break;

    case eTblastn:
        retval = new CTBlastnOptionsHandle();
        break;

    case eTblastx:
        retval = new CTBlastxOptionsHandle();
        break;

    case eMegablast:
	{
		CBlastNucleotideOptionsHandle* opts = 
			new CBlastNucleotideOptionsHandle();
		opts->SetTraditionalMegablastDefaults();
		retval = opts;
        break;
	}

    case eDiscMegablast:
        retval = new CDiscNucleotideOptionsHandle();
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
 * Revision 1.1  2003/11/26 18:23:59  camacho
 * +Blast Option Handle classes
 *
 * ===========================================================================
 */
