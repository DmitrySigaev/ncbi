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
* Authors:  Paul Thiessen
*
* File Description:
*      new C++ PSSM construction
*
* ===========================================================================
*/

#ifndef CN3D_PSSM__HPP
#define CN3D_PSSM__HPP

#include <corelib/ncbistl.hpp>
#include <corelib/ncbistre.hpp>

#include <objects/scoremat/PssmWithParameters.hpp>

#include <blastkar.h>   // for BLAST_Matrix


BEGIN_SCOPE(Cn3D)

class BlockMultipleAlignment;

BLAST_Matrix * CreateBlastMatrix(const BlockMultipleAlignment *bma);
ncbi::objects::CPssmWithParameters * CreatePSSM(const BlockMultipleAlignment *bma);

void OutputPSSM(const BlockMultipleAlignment *bma, ncbi::CNcbiOstream& os);

END_SCOPE(Cn3D)

#endif // CN3D_PSSM__HPP

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.3  2005/11/01 02:44:07  thiessen
* fix GCC warnings; switch threader to C++ PSSMs
*
* Revision 1.2  2005/06/07 12:18:52  thiessen
* add PSSM export
*
* Revision 1.1  2005/03/08 17:22:31  thiessen
* apparently working C++ PSSM generation
*
*/
