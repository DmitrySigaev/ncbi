/* $Id$
* ===========================================================================
*
*                            PUBLIC DOMAIN NOTICE
*               National Center for Biotechnology Information
*
*  This software/database is a "United States Government Work" under the
*  terms of the United States Copyright Act.  It was written as part of
*  the author's offical duties as a United States Government employee and
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
* ===========================================================================*/

/*****************************************************************************

File name: blast_util.h

Author: Ilya Dondoshansky

Contents: Various auxiliary BLAST utility functions

Detailed Contents: 

******************************************************************************
 * $Revision$
 * */
#ifndef __BLAST_UTIL__
#define __BLAST_UTIL__

#ifdef __cplusplus
extern "C" {
#endif

#include <readdb.h>
#include <blast_def.h>

/** Different types of sequence encodings for sequence retrieval from the 
 * BLAST database 
 */
#define BLASTP_ENCODING 0
#define BLASTNA_ENCODING 1
#define NCBI4NA_ENCODING 2
#define ERROR_ENCODING 255

/** Retrieve a sequence from the BLAST database
 * @param db BLAST database [in]
 * @param seq_ptr Pointer to sequence buffer [out]
 * @param oid Ordinal id of the sequence to be retrieved [in]
 * @param encoding In what encoding should the sequence be retrieved? [in]
 */ 
void
MakeBlastSequenceBlk(ReadDBFILEPtr db, BLAST_SequenceBlkPtr PNTR seq_ptr,
                     Int4 oid, Uint1 encoding);

/** Deallocate memory only for the sequence in the sequence block */
Int2 BlastSequenceBlkClean(BLAST_SequenceBlkPtr seq_blk);
   
/** Deallocate memory for a sequence block */
BLAST_SequenceBlkPtr BlastSequenceBlkFree(BLAST_SequenceBlkPtr seq_blk);

/** Set number for a given program type.  Return is zero on success.
 * @param program string name of program [in]
 * @param number number of program [out]
*/
Int2 BlastProgram2Number(const Char *program, Uint1 *number);

/** Return string name for program given a number.  Return is zero on success.
 * @param number number of program [in]
 * @param program string name of program (memory should be deallocated by called) [out]
*/
Int2 BlastNumber2Program(Uint1 number, CharPtr *program);

/** Allocates memory for *sequence_blk and then populates it.
 * @param buffer start of sequence [in]
 * @param length query sequence length [in]
 * @param context context number [in]
 * @param frame frame information [in]
 * @param seq_blk SequenceBlk to be allocated and filled in [out]
 * @param buffer_allocated Is the buffer allocated? If yes, 'sequence_start' is
 *        the start of the sequence, otherwise it is 'sequence'. [in]
*/
Int2
BlastSetUp_SeqBlkNew (const Uint1Ptr buffer, Int4 length, Int2 context,
	const Int4Ptr frame, BLAST_SequenceBlkPtr *seq_blk, 
        Boolean buffer_allocated);

/** Deallocate the sequence block.
 * @param seq_blk The sequence block to be freed [in]
 * @return NULL
 */
BLAST_SequenceBlkPtr 
BLAST_SequenceBlkDestruct(BLAST_SequenceBlkPtr seq_blk);

#define CODON_LENGTH 3

/** GetTranslation to get the translation of the nucl. sequence in the
 * appropriate frame and with the appropriate GeneticCode.
 * The function return an allocated CharPtr, the caller must delete this.
 * The first and last spaces of this CharPtr contain NULLB's.
 * @param query_seq Forward strand of the nucleotide sequence [in]
 * @param query_seq_rev Reverse strand of the nucleotide sequence [in]
 * @param nt_length Length of the nucleotide sequence [in]
 * @param frame What frame to translate into? [in]
 * @param length Length of the translated sequence [out]
 * @param genetic_code Genetic code to use for translation [in]
 * @return The translated sequence
*/
Uint1Ptr LIBCALL
BLAST_GetTranslation(Uint1Ptr query_seq, Uint1Ptr query_seq_rev, 
   Int4 nt_length, Int2 frame, Int4Ptr length, CharPtr genetic_code);

/** Make a temporary protein BioseqPtr to use with seg.
*/
BioseqPtr
BLAST_MakeTempProteinBioseq (Uint1Ptr sequence, Int4 length, Uint1 alphabet);


/** Given a GI, read the sequence from the database and fill out a
 *  BLAST_SequenceBlk structure.
 * @param db the database to read from, assumed to already be open.
 * @param gi the gi of the sequence to read
 * @param seq pointer to the sequence block to fill out
 */
Int4 MakeBlastSequenceBlkFromGI(ReadDBFILEPtr db, Int4 gi, BLAST_SequenceBlkPtr seq);


/** Given an ordinal id, read the sequence from the database and fill out a
 *  BLAST_SequenceBlk structure.
 * @param db the database to read from, assumed to already be open.
 * @param oid the ordinal id of the sequence to read
 * @param seq pointer to the sequence block to fill out
 */
Int4 MakeBlastSequenceBlkFromOID(ReadDBFILEPtr db, Int4 oid, BLAST_SequenceBlkPtr seq);

/** Given a file containing sequence(s) in fasta format,
 * read a sequence and fill out a BLAST_SequenceBlk structure.
 *
 * @param fasta_fp the file to read from, assumed to already be open
 * @param seq pointer to the sequence block to fill out
 * @return Zero if successful, one on any error.
 */
Int4 MakeBlastSequenceBlkFromFasta(FILE *fasta_fp, BLAST_SequenceBlkPtr seq);

/** Translate a nucleotide sequence without ambiguity codes.
 * This is used for the first-pass translation of the database.
 * The genetic code to be used is determined by the translation_table
 * This function translates a packed (ncbi2na) nucl. alphabet.  It
 * views a basepair as being in one of four sets of 2-bits:
 * |0|1|2|3||0|1|2|3||0|1|2|3||...
 *
 * 1st byte | 2 byte | 3rd byte...
 *
 * A codon that starts at the beginning of the above sequence starts in
 * state "0" and includes basepairs 0, 1, and 2.  The next codon, in the
 * same frame, after that starts in state "3" and includes 3, 0, and 1.
 *
 *** Optimization:
 * changed the single main loop to 
 * - advance to state 0, 
 * - optimized inner loop does two (3 byte->4 codon) translation per iteration
 *   (loads are moved earlier so they can be done in advance.)
 * - do remainder
 *
 * @param translation The translation table [in]
 * @param length Length of the nucleotide sequence [in]
 * @param nt_seq The original nucleotide sequence [in]
 * @param frame What frame to translate to? [in]
 * @param prot_seq Preallocated buffer for the (translated) protein sequence, 
 *                 with NULLB sentinels on either end. [out]
*/
Int4 LIBCALL
BLAST_TranslateCompressedSequence(Uint1Ptr translation, Int4 length, 
   Uint1Ptr nt_seq, Int2 frame, Uint1Ptr prot_seq);

/** Reverse a nucleotide sequence in the blastna encoding, adding sentinel 
 * bytes on both ends.
 * @param sequence Forward strand of the sequence [in]
 * @param length Length of the sequence plus 1 for the sentinel byte [in]
 * @param rev_sequence_ptr Reverse strand of the sequence [out]
 */
Int2 GetReverseNuclSequence(Uint1Ptr sequence, Int4 length, 
                            Uint1Ptr PNTR rev_sequence_ptr);

#ifdef __cplusplus
}
#endif
#endif /* !__BLAST_UTIL__ */
