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

File name: blast_filter.h

Author: Ilya Dondoshansky

Contents: BLAST filtering functions.

Detailed Contents: 

******************************************************************************
 * $Revision$
 * */
#ifndef __BLAST_FILTER__
#define __BLAST_FILTER__

#ifdef __cplusplus
extern "C" {
#endif

#include <blast_def.h>

/** Create and initialize a new sequence interval.
 * @param from Start of the interval [in]
 * @param to End of the interval [in]
 * @return Pointer to the allocated BlastSeqLoc structure.
 */
BlastSeqLocPtr BlastSeqLocNew(Int4 from, Int4 to);

/** Deallocate a BlastSeqLoc structure */
BlastSeqLocPtr BlastSeqLocFree(BlastSeqLocPtr loc);

/** Deallocate memory for a list of BlastMask structures */
BlastMaskPtr BlastMaskFree(BlastMaskPtr mask_loc);

/** Convert a SeqLoc of type int or packed_int to a BlastMask structure.
 * @param mask_slp The SeqLoc to be converted [in]
 * @param index The ordinal number of the sequence to be assigned to the new 
 *              BlastMask
 * @return Pointer to the allocated BlastMask structure.
 */
BlastMaskPtr BlastMaskFromSeqLoc(SeqLocPtr mask_slp, Int4 index);

/** Convert a BlastMask list to a list of SeqLocs, used for formatting 
 * BLAST results.
 */
SeqLocPtr BlastMaskToSeqLoc(BlastMaskPtr mask_loc, SeqLocPtr slp);

/** Go through all mask locations in one sequence, 
 * combine any that overlap. Deallocate the memory for the locations that 
 * were on the list, produce a new (merged) list of locations. 
 * @param mask_loc The list of masks to be merged [in] 
 * @param mask_loc_out The new (merged) list of masks. [out]
*/
Int2
CombineMaskLocations(BlastSeqLocPtr mask_loc, BlastSeqLocPtr *mask_loc_out);

/** This function takes the list of mask locations (i.e., regions that 
 * should not be searched or not added to lookup table) and makes up a set 
 * of DoubleIntPtr's that should be searched (that is, takes the 
 * complement). If the entire sequence is filtered, then a DoubleInt is 
 * created and both of its elements (i1 and i2) are set to -1 to indicate 
 * this. 
 * If any of the mask_loc's is NULL, a DoubleInt for full span of the 
 * respective query sequence is created.
 * @param program_number Type of BLAST program [in]
 * @param query_info The query information structure [in]
 * @param mask_loc All mask locations [in]
 * @param complement_mask Linked list of DoubleIntPtrs with offsets. [out]
*/
Int2 
BLAST_ComplementMaskLocations(Uint1 program_number, 
   BlastQueryInfoPtr query_info, BlastMaskPtr mask_loc, 
   BlastSeqLocPtr *complement_mask);

/** Runs filtering functions, according to the string "instructions", on the
 * SeqLocPtr. Should combine all SeqLocs so they are non-redundant.
 * @param program_number Type of BLAST program [in]
 * @param sequence The sequence or part of the sequence to be filtered [in]
 * @param length Length of the (sub)sequence [in]
 * @param offset Offset into the full sequence [in]
 * @param instructions String of instructions to filtering functions. [in]
 * @param mask_at_hash If TRUE masking is done while making the lookup table
 *                     only. [out] 
 * @param seqloc_retval Resulting locations for filtered region. [out]
*/
Int2
BlastSetUp_Filter(Uint1 program_number, Uint1Ptr sequence, Int4 length, 
   Int4 offset, CharPtr instructions, BoolPtr mask_at_hash, 
   BlastSeqLocPtr *seqloc_retval);

/** Duplicate masks in 6 frames for each nucleotide sequence, converting
 * all masks' coordinates from DNA to protein.
 * @param mask_loc_ptr Masks list to be modified [in] [out]
 * @param slp List of nucleotide query SeqLoc's [in]
 */
Int2 BlastMaskDNAToProtein(BlastMaskPtr PNTR mask_loc_ptr, SeqLocPtr slp);

/** Convert all masks' protein coordinates to nucleotide.
 * @param mask_loc_ptr Masks list to be modified [in] [out]
 * @param slp List of nucleotide query SeqLoc's [in]
 */
Int2 BlastMaskProteinToDNA(BlastMaskPtr PNTR mask_loc_ptr, SeqLocPtr slp);

#ifdef __cplusplus
}
#endif
#endif /* !__BLAST_FILTER__ */
