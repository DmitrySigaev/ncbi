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
* Author:  Christiam Camacho
*
* File Description:
*   Auxiliary setup functions for Blast objects interface
*
*/

#ifndef BLAST_SETUP__HPP
#define BLAST_SETUP__HPP

USING_NCBI_SCOPE;
USING_SCOPE(objects);

BEGIN_NCBI_SCOPE

/** Retrieves a sequence using the object manager
 * @param sl seqloc of the sequence to obtain [in]
 * @param encoding encoding for the sequence retrieved [in]
 * @param buflen length of the buffer returned [out]
 * @param scope Scope from which the sequences are retrieved [in]
 * @param strand strand to retrieve [in]
 * @param add_nucl_sentinel true to guard nucleotide sequence with sentinel 
 * bytes (ignored for protein sequences, which always have sentinels) [in]
 */
Uint1*
BLASTGetSequence(const CSeq_loc& sl, Uint1 encoding, int& buflen,
        CScope* scope, ENa_strand strand = eNa_strand_plus, 
        bool add_nucl_sentinel = true);

#if 0
// not used right now
/** Translates nucleotide query sequences to protein in the requested frame
 * @param nucl_seq forward (plus) strand of the nucleotide sequence [in]
 * @param nucl_seq_rev reverse (minus) strand of the nucleotide sequence [in]
 * @param nucl_length length of a single strand of the nucleotide sequence [in]
 * @param frame frame to translate, allowed values: 1,2,3,-1,-2,-3 [in]
 * @param translation buffer to hold the translation, should be allocated
 * outside this function [out]
 */
void
BLASTGetTranslation(const Uint1* nucl_seq, const Uint1* nucl_seq_rev, 
        const int nucl_length, const short frame, Uint1* translation);
#endif

// Should be moved to blast_util.c?
char*
BLASTFindGeneticCode(int genetic_code);

END_NCBI_SCOPE

/*
* ===========================================================================
*
* $Log$
* Revision 1.1  2003/07/10 18:34:19  camacho
* Initial revision
*
*
* ===========================================================================
*/

#endif  /* BLAST_SETUP__HPP */
