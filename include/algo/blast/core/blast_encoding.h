#ifndef ALGO_BLAST_CORE___BLAST_LIST__H
#define ALGO_BLAST_CORE___BLAST_LIST__H

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
 */

/** @file blast_encoding.h
 *  Declarations of static arrays used to define some NCBI encodings to be used
 *  in a toolkit independent manner by the BLAST engine.
 */

#include <algo/blast/core/ncbi_std.h>

/** @addtogroup AlgoBlast
 *
 * @{
 */

#ifdef __cplusplus
extern "C" {
#endif

/* Nucleotide encodings */

/** Translates between ncbi4na and blastna. The first four elements
 *	of this array match ncbi2na. */
extern const Uint1 NCBI4NA_TO_BLASTNA[];
extern const Uint1 BLASTNA_TO_NCBI4NA[];
extern const Uint1 IUPACNA_TO_BLASTNA[];
extern const Uint1 IUPACNA_TO_NCBI4NA[];

/* Protein encodings */
extern const Uint1 AMINOACID_TO_NCBISTDAA[];

#define BLASTNA_SIZE 16
#define BLASTAA_SIZE 26

/* Identifies the blastna alphabet, for use in blast only. */
#define BLASTNA_SEQ_CODE 99
#define BLASTAA_SEQ_CODE 11 /* == Seq_code_ncbistdaa */
#define NCBI4NA_SEQ_CODE 4  /* == Seq_code_ncbi4na */	

/* FIXME: Should this be removed since it's the same as BLASTAA_SIZE ? */
#define PSI_ALPHABET_SIZE  26 /* For PSI Blast this is the only 26 */

#ifdef __cplusplus
}
#endif

/* @} */


/*
 * ===========================================================================
 * $Log$
 * Revision 1.1  2004/04/07 03:10:20  camacho
 * Initial revision
 *
 * ===========================================================================
 */

#endif  /* ALGO_BLAST_CORE___BLAST_LIST__H */
