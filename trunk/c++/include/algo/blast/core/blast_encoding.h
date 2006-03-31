#ifndef ALGO_BLAST_CORE___BLAST_ENCODING__H
#define ALGO_BLAST_CORE___BLAST_ENCODING__H

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

/** Different types of sequence encodings for sequence retrieval from the 
 * BLAST database 
 */
typedef enum { 
    eBlastEncodingProtein       = 0, /**< NCBIstdaa */
    eBlastEncodingNucleotide    = 1, /**< Special encoding for preliminary 
                                       stage of BLAST: permutation of NCBI4na.
                                       A.k.a.: BLASTNA encoding
                                      */
    eBlastEncodingNcbi4na       = 2, /**< NCBI4na */
    eBlastEncodingNcbi2na       = 3, /**< NCBI2na */
    eBlastEncodingError         = 255 /**< Error value for encoding */
} EBlastEncoding;

/* Nucleotide encodings */

/** Translates between ncbi4na and blastna. The first four elements
 *	of this array match ncbi2na. */
extern const Uint1 NCBI4NA_TO_BLASTNA[];

/** Translates between blastna and ncbi4na. */
extern const Uint1 BLASTNA_TO_NCBI4NA[];

/** Translates between iupacna and blastna. */
extern const Uint1 IUPACNA_TO_BLASTNA[];

/** Translates between iupacna and ncbi4na. */
extern const Uint1 IUPACNA_TO_NCBI4NA[];

/** Translates between ncbieaa and ncbistdaa. */
extern const Uint1 AMINOACID_TO_NCBISTDAA[];

#define BLAST2NA_SIZE 4     /**< Size of compressed nucleic acid alphabet */
#define BLASTNA_SIZE 16     /**< Size of nucleic acid alphabet */
#define BLASTAA_SIZE 26     /**< Size of aminoacid alphabet */


#define BLASTNA_SEQ_CODE 99 /**< Identifies the blastna alphabet, for use in 
                                blast only. */
#define BLASTAA_SEQ_CODE 11 /**< == Seq_code_ncbistdaa */
#define NCBI4NA_SEQ_CODE 4  /**< == Seq_code_ncbi4na */	

/// Sentinel byte for protein sequences
extern const Uint1 kProtSentinel;
/// Sentinel nibble for nucleotide sequences
extern const Uint1 kNuclSentinel;

#ifdef __cplusplus
}
#endif

/* @} */


/*
 * ===========================================================================
 * $Log$
 * Revision 1.8  2006/03/31 17:31:49  camacho
 * Added constants for sentinel values
 *
 * Revision 1.7  2005/12/19 16:19:18  papadopo
 * add define for size of ncbi2na alphabet
 *
 * Revision 1.6  2005/05/10 16:07:35  camacho
 * Changed *_ENCODING #defines to EBlastEncoding enumeration
 *
 * Revision 1.5  2004/11/24 16:00:39  dondosha
 * Added and/or fixed doxygen comments
 *
 * Revision 1.4  2004/06/21 12:53:54  camacho
 * Remove PSI_ALPHABET_SIZE as it is identical to BLASTAA_SIZE
 *
 * Revision 1.3  2004/04/09 14:48:05  camacho
 * Added doxygen comments
 *
 * Revision 1.2  2004/04/07 19:05:44  camacho
 * Minor fix in #ifdef guards
 *
 * Revision 1.1  2004/04/07 03:10:20  camacho
 * Initial revision
 *
 * ===========================================================================
 */

#endif  /* ALGO_BLAST_CORE___BLAST_ENCODING__H */
