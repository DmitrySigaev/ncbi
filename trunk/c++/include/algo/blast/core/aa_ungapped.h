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
* ===========================================================================

*/

#ifndef AA_UNGAPPED__H
#define AA_UNGAPPED__H

#include <algo/blast/core/blast_def.h>
#include <algo/blast/core/aa_lookup.h>
#include <algo/blast/core/blast_extend.h>

#ifdef _cplusplus
extern "C" {
#endif

/** Scan a subject sequence for word hits
 *
 * @param subject the subject sequence [in]
 * @param query the query sequence [in]
 * @param lookup the lookup table [in]
 * @param matrix the substitution matrix [in]
 * @param word_params word parameters, needed for cutoff and dropoff [in]
 * @param ewp extend parameters, needed for diagonal tracking [in]
 * @param query_offsets array for storing query offsets [out]
 * @param subject_offsets array for storing subject offsets [out]
 * @param offset_array_size the number of elements in each offset array [in]
 * @param init_hitlist hsps resulting from the ungapped extension [out]
 * @return the number of hits found 
 */
Int4 BlastAaWordFinder(BLAST_SequenceBlk* subject,
		       BLAST_SequenceBlk* query,
		       LookupTableWrap* lookup,
		       Int4** matrix,
		       BlastInitialWordParameters* word_params,
		       BLAST_ExtendWord* ewp,
		       Uint4* query_offsets,
		       Uint4* subject_offsets,
		       Int4 offset_array_size,
		       BlastInitHitList* init_hitlist);

/** Scan a subject sequence for word hits and trigger two-hit extensions.
 *
 * @param subject the subject sequence [in]
 * @param query the query sequence [in]
 * @param lookup_wrap the lookup table [in]
 * @param diag the diagonal array structure [in/out]
 * @param matrix the substitution matrix [in]
 * @param cutoff cutoff score for saving ungapped HSPs [in]
 * @param dropoff x dropoff [in]
 * @param query_offsets array for storing query offsets [out]
 * @param subject_offsets array for storing subject offsets [out]
 * @param array_size the number of elements in each offset array [in]
 * @param ungapped_hsps hsps resulting from the ungapped extension [out]
 * @return the number of hits found 
 */

Int4 BlastAaWordFinder_TwoHit(const BLAST_SequenceBlk* subject,
			      const BLAST_SequenceBlk* query,
			      const LookupTableWrap* lookup_wrap,
			      BLAST_DiagTable* diag,
			      Int4 ** matrix,
			      Int4 cutoff,
			      Int4 dropoff,
			      Uint4 * query_offsets,
			      Uint4 * subject_offsets,
			      Int4 array_size,
                              BlastInitHitList* ungapped_hsps);

/** Scan a subject sequence for word hits and trigger one-hit extensions.
 *
 * @param subject the subject sequence
 * @param query the query sequence
 * @param lookup_wrap the lookup table
 * @param diag the diagonal array structure
 * @param matrix the substitution matrix [in]
 * @param cutoff cutoff score for saving ungapped HSPs [in]
 * @param dropoff x dropoff [in]
 * @param query_offsets array for storing query offsets
 * @param subject_offsets array for storing subject offsets
 * @param array_size the number of elements in each offset array
 * @param ungapped_hsps hsps resulting from the ungapped extensions [out]
 * @return the number of hits found
 */

Int4 BlastAaWordFinder_OneHit(const BLAST_SequenceBlk* subject,
			      const BLAST_SequenceBlk* query,
			      const LookupTableWrap* lookup_wrap,
			      BLAST_DiagTable* diag,
			      Int4 ** matrix,
			      Int4 cutoff,
			      Int4 dropoff,
			      Uint4 * query_offsets,
			      Uint4 * subject_offsets,
			      Int4 array_size,
                              BlastInitHitList* ungapped_hsps);

/**
 * Beginning at s_off and q_off in the subject and query, respectively,
 * extend to the right until the cumulative score becomes negative or
 * drops by at least dropoff.
 *
 * @param matrix the substitution matrix [in]
 * @param subject subject sequence [in]
 * @param query query sequence [in]
 * @param s_off subject offset [in]
 * @param q_off query offset [in]
 * @param dropoff the X dropoff parameter [in]
 * @param displacement the length of the extension [out]
 * @return The score of the extension
 */

  Int4 BlastAaExtendRight(Int4 ** matrix,
			const BLAST_SequenceBlk* subject,
			const BLAST_SequenceBlk* query,
			Int4 s_off,
			Int4 q_off,
			Int4 dropoff,
			Int4* displacement);


/**
 * Beginning at s_off and q_off in the subject and query, respectively,
 * extend to the left until the cumulative score becomes negative or
 * drops by at least dropoff.
 *
 * @param matrix the substitution matrix [in]
 * @param subject subject sequence [in]
 * @param query query sequence [in]
 * @param s_off subject offset [in]
 * @param q_off query offset [in]
 * @param dropoff the X dropoff parameter [in]
 * @param displacement the length of the extension [out]
 * @return The score of the extension
 */

Int4 BlastAaExtendLeft(Int4 ** matrix,
		       const BLAST_SequenceBlk* subject,
		       const BLAST_SequenceBlk* query,
		       Int4 s_off,
		       Int4 q_off,
		       Int4 dropoff,
		       Int4* displacement);


/** Perform a one-hit extension. Beginning at the specified hit,
 * extend to the left, then extend to the right. 
 *
 * @param matrix the substitution matrix [in]
 * @param subject subject sequence [in]
 * @param query query sequence [in]
 * @param s_off subject offset [in]
 * @param q_off query offset [in]
 * @param dropoff X dropoff parameter [in]
 * @param hsp_q the offset in the query where the HSP begins [out]
 * @param hsp_s the offset in the subject where the HSP begins [out]
 * @param hsp_len the length of the HSP [out]
 * @return the score of the hsp.
 */

Int4 BlastAaExtendOneHit(Int4 ** matrix,
                         const BLAST_SequenceBlk* subject,
                         const BLAST_SequenceBlk* query,
                         Int4 s_off,
                         Int4 q_off,
                         Int4 dropoff,
			 Int4* hsp_q,
			 Int4* hsp_s,
			 Int4* hsp_len);
                         
/** Perform a two-hit extension. Given two hits L and R, begin
 * at R and extend to the left. If we do not reach L, abort the extension.
 * Otherwise, begin at R and extend to the right.
 *
 * @param matrix the substitution matrix [in]
 * @param subject subject sequence [in]
 * @param query query sequence [in]
 * @param s_left_off left subject offset [in]
 * @param s_right_off right subject offset [in]
 * @param q_right_off right query offset [in]
 * @param dropoff X dropoff parameter [in]
 * @param hsp_q the offset in the query where the HSP begins [out]
 * @param hsp_s the offset in the subject where the HSP begins [out]
 * @param hsp_len the length of the HSP [out]
 * @return the score of the hsp.
 */

Int4 BlastAaExtendTwoHit(Int4 ** matrix,
                         const BLAST_SequenceBlk* subject,
                         const BLAST_SequenceBlk* query,
                         Int4 s_left_off,
                         Int4 s_right_off,
                         Int4 q_right_off,
                         Int4 dropoff,
			 Int4* hsp_q,
			 Int4* hsp_s,
			 Int4* hsp_len);

/** Create a new diagonal array structure.
  * @param diag handle to diagonal array structure [in/modified]
  * @param window_size the window size for the two-hit heuristic
  * @param longest_seq length of the longest subject or query sequence
  */

Int4 DiagNew(BLAST_DiagTable* * diag, Int4 window_size, Int4 longest_seq);

/** Update the offset for use with a new sequence.
  * @param diag pointer to the diagonal array structure [in]
  * @param length length of the new sequence [in]
  */
  
Int4 DiagUpdate(BLAST_DiagTable* diag, Int4 length);

/** Reset the diagonal array structure. Used when offset has wrapped around.
  * @param diag pointer to the diagonal array structure [in]
  */
Int4 DiagClear(BLAST_DiagTable* diag);

/** Free the diagonal array structure.
  * @param diag pointer to the diagonal array structure
  */
Int4 DiagFree(BLAST_DiagTable* diag);

/** Update the level of a given diagonal.
  * @param diag pointer to the diagonal array structure [in]
  * @param query_offset offset in the query [in]
  * @param subject_offset offset in the subject [in]
  * @param subject_extension how far we've extended along the subject [in]
  */

/*Int4 DiagUpdateLevel(BLAST_DiagTable* diag, Int4 query_offset, Int4 subject_offset, Int4 subject_extension);
*/

#ifdef _cplusplus
}
#endif

#endif /* AA_UNGAPPED__H */
