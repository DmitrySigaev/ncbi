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
 *
 * Author: Ilya Dondoshansky
 *
 */

/** @file blast_gapalign.c
 * Functions to perform gapped alignment
 */

static char const rcsid[] = 
    "$Id$";

#include <algo/blast/core/blast_options.h>
#include <algo/blast/core/blast_def.h>
#include <algo/blast/core/blast_gapalign.h>
#include <algo/blast/core/blast_util.h> /* for NCBI2NA_UNPACK_BASE macros */
#include <algo/blast/core/blast_setup.h>
#include <algo/blast/core/greedy_align.h>
#include "blast_gapalign_pri.h"

static Int2 BLAST_DynProgNtGappedAlignment(BLAST_SequenceBlk* query_blk, 
   BLAST_SequenceBlk* subject_blk, BlastGapAlignStruct* gap_align, 
   const BlastScoringParameters* score_params, BlastInitHSP* init_hsp);
static Int4 BLAST_AlignPackedNucl(Uint1* B, Uint1* A, Int4 N, Int4 M, 
   Int4* pej, Int4* pei, BlastGapAlignStruct* gap_align,
   const BlastScoringParameters* score_params, Boolean reverse_sequence);

static Int2 BLAST_ProtGappedAlignment(EBlastProgramType program, 
   BLAST_SequenceBlk* query_in, BLAST_SequenceBlk* subject_in,
   BlastGapAlignStruct* gap_align,
   const BlastScoringParameters* score_params, BlastInitHSP* init_hsp);

/** Auxiliary structure for dynamic programming gapped extension */
typedef struct BlastGapDP {
  Int4 best;
  Int4 best_gap;
  Int4 best_decline;
} BlastGapDP;

/** Structure containing all internal traceback information. */
typedef struct GapData {
  Int4* sapp;                 /* Current script append ptr */
  Int4  last;
} GapData;

/** Append "Delete k" op */
#define DEL_(k) \
data.last = (data.last < 0) ? (data.sapp[-1] -= (k)) : (*data.sapp++ = -(k));

/** Append "Insert k" op */
#define INS_(k) \
data.last = (data.last > 0) ? (data.sapp[-1] += (k)) : (*data.sapp++ = (k));

/** Append "Replace" op */
#define REP_ \
{data.last = *data.sapp++ = 0;}

/* Divide by two to prevent underflows. */
#define MININT INT4_MIN/2
#define REPP_ \
{*data.sapp++ = MININT; data.last = 0;}

/** Are the two HSPs within a given number of diagonals from each other? */
#define MB_HSP_CLOSE(q1, q2, s1, s2, c) \
(ABS((q1-s1) - (q2-s2)) < c)

/** Is one HSP contained in a diagonal strip around another? */
#define MB_HSP_CONTAINED(qo1,qo2,qe2,so1,so2,se2,c) \
(qo1>=qo2 && qo1<=qe2 && so1>=so2 && so1<=se2 && \
MB_HSP_CLOSE(qo1,qo2,so1,so2,c))

/** TRUE if c is between a and b; f between d and f. Determines if the
 * coordinates are already in an HSP that has been evaluated. 
 */
#define CONTAINED_IN_HSP(a,b,c,d,e,f) (((a <= c && b >= c) && (d <= f && e >= f)) ? TRUE : FALSE)

/** Callback for sorting HSPs by starting offset in query */ 
static int
query_offset_compare_hsps(const void* v1, const void* v2)
{
	BlastHSP* h1,* h2;
	BlastHSP** hp1,** hp2;

	hp1 = (BlastHSP**) v1;
	hp2 = (BlastHSP**) v2;
	h1 = *hp1;
	h2 = *hp2;

	if (h1 == NULL || h2 == NULL)
		return 0;

   /* If these are from different contexts, don't compare offsets */
   if (h1->context < h2->context) 
      return -1;
   if (h1->context > h2->context)
      return 1;

	if (h1->query.offset < h2->query.offset)
		return -1;
	if (h1->query.offset > h2->query.offset)
		return 1;

	return 0;
}

/** Callback for sorting HSPs by ending offset in query */
static int
query_end_compare_hsps(const void* v1, const void* v2)
{
	BlastHSP* h1,* h2;
	BlastHSP** hp1,** hp2;

	hp1 = (BlastHSP**) v1;
	hp2 = (BlastHSP**) v2;
	h1 = *hp1;
	h2 = *hp2;

	if (h1 == NULL || h2 == NULL)
		return 0;

   /* If these are from different contexts, don't compare offsets */
   if (h1->context < h2->context) 
      return -1;
   if (h1->context > h2->context)
      return 1;

	if (h1->query.end < h2->query.end)
		return -1;
	if (h1->query.end > h2->query.end)
		return 1;

	return 0;
}

/** Callback for sorting HSPs in order to move NULL HSPs to the end of
 * the array.
 */
static int
null_compare_hsps(const void* v1, const void* v2)
{
	BlastHSP* h1,* h2;
	BlastHSP** hp1,** hp2;

	hp1 = (BlastHSP**) v1;
	hp2 = (BlastHSP**) v2;
	h1 = *hp1;
	h2 = *hp2;
        
        /* NULL HSPs are moved to the end */
	if (!h1 && !h2)
	   return 0;
	else if (!h1)
           return 1;
        else if (!h2)
           return -1;

	return 0;
}

/** Check the gapped alignments for an overlap of two different alignments.
 * A sufficient overlap is when two alignments have the same start values
 * of have the same final values. 
 * @param hsp_array Pointer to an array of BlastHSP structures [in]
 * @param hsp_count The size of the hsp_array [in]
 * @return The number of valid alignments remaining. 
*/
static Int4
CheckGappedAlignmentsForOverlap(BlastHSP* *hsp_array, Int4 hsp_count)

{
   Int4 index = 0;
   Int4 increment = 1;
   Int4 retval = 0;

   if (*hsp_array == NULL || hsp_count == 0)
      return 0;
   
   qsort(hsp_array, hsp_count, sizeof(BlastHSP*), query_offset_compare_hsps);
   while (index < hsp_count-increment) {
      /* Check if both HSP's start on or end on the same digonal. */
      if (hsp_array[index+increment] == NULL) {
         increment++;
         continue;
      }
      
      if (hsp_array[index] && hsp_array[index]->query.offset == hsp_array[index+increment]->query.offset &&
          hsp_array[index]->subject.offset == hsp_array[index+increment]->subject.offset &&
          hsp_array[index]->context == hsp_array[index+increment]->context)
      {
         if (hsp_array[index]->score > hsp_array[index+increment]->score) {
            sfree(hsp_array[index+increment]);
            increment++;
         } else {
            sfree(hsp_array[index]);
            index++;
            increment = 1;
         }
      } else {
         index++;
         increment = 1;
      }
   }
   
   qsort(hsp_array, hsp_count, sizeof(BlastHSP*), query_end_compare_hsps);
   index=0;
   increment=1;
   while (index < hsp_count-increment)
   { /* Check if both HSP's start on or end on the same digonal. */
      if (hsp_array[index+increment] == NULL)
      {
         increment++;
         continue;
      }
      
      if (hsp_array[index] &&
          hsp_array[index]->query.end == hsp_array[index+increment]->query.end &&
          hsp_array[index]->subject.end == hsp_array[index+increment]->subject.end &&
          hsp_array[index]->context == hsp_array[index+increment]->context)
      {
         if (hsp_array[index]->score > hsp_array[index+increment]->score) {
            sfree(hsp_array[index+increment]);
            increment++;
         } else	{
            sfree(hsp_array[index]);
            index++;
            increment = 1;
         }
      } else {
         index++;
         increment = 1;
      }
   }

   qsort(hsp_array,hsp_count,sizeof(BlastHSP*), null_compare_hsps);
   
   for (index=0; index<hsp_count; index++)
   {
      if (hsp_array[index] != NULL)
         retval++;
   }

   return retval;
}

/** Retrieve the state structure corresponding to a given length
 * @param head Pointer to the first element of the state structures 
 *        array [in]
 * @param length The length for which the state structure has to be 
 *        found [in]
 * @return The found or created state structure
 */
#define	CHUNKSIZE	2097152
static GapStateArrayStruct*
GapGetState(GapStateArrayStruct** head, Int4 length)

{
   GapStateArrayStruct*	retval,* var,* last;
   Int4	chunksize = MAX(CHUNKSIZE, length + length/3);

   length += length/3;	/* Add on about 30% so the end will get reused. */
   retval = NULL;
   if (*head == NULL) {
      retval = (GapStateArrayStruct*) 
         malloc(sizeof(GapStateArrayStruct));
      retval->state_array = (Uint1*) malloc(chunksize*sizeof(Uint1));
      retval->length = chunksize;
      retval->used = 0;
      retval->next = NULL;
      *head = retval;
   } else {
      var = *head;
      last = *head;
      while (var) {
         if (length < (var->length - var->used)) {
            retval = var;
            break;
         } else if (var->used == 0) { 
            /* If it's empty and too small, replace. */
            sfree(var->state_array);
            var->state_array = (Uint1*) malloc(chunksize*sizeof(Uint1));
            var->length = chunksize;
            retval = var;
            break;
         }
         last = var;
         var = var->next;
      }
      
      if (var == NULL)
      {
         retval = (GapStateArrayStruct*) malloc(sizeof(GapStateArrayStruct));
         retval->state_array = (Uint1*) malloc(chunksize*sizeof(Uint1));
         retval->length = chunksize;
         retval->used = 0;
         retval->next = NULL;
         last->next = retval;
      }
   }

#ifdef ERR_POST_EX_DEFINED
   if (retval->state_array == NULL)
      ErrPostEx(SEV_ERROR, 0, 0, "state array is NULL");
#endif
		
   return retval;

}

/** Remove a state from a state structure */
static Boolean
GapPurgeState(GapStateArrayStruct* state_struct)
{
   while (state_struct)
   {
      /*
	memset(state_struct->state_array, 0, state_struct->used);
      */
      state_struct->used = 0;
      state_struct = state_struct->next;
   }
   
   return TRUE;
}

/** Greatest common divisor */
static Int4 gcd(Int4 a, Int4 b)
{
    Int4 c;
    if (a < b) {
        c = a;
        a = b; b = c;
    }
    while ((c = a % b) != 0) {
        a = b; b = c;
    }

    return b;
}

/** Divide 3 numbers by their greatest common divisor
 * @return The GCD
 */
static Int4 gdb3(Int4* a, Int4* b, Int4* c)
{
    Int4 g;
    if (*b == 0) g = gcd(*a, *c);
    else g = gcd(*a, gcd(*b, *c));
    if (g > 1) {
        *a /= g;
        *b /= g;
        *c /= g;
    }
    return g;
}

/** Deallocate the memory for greedy gapped alignment */
static SGreedyAlignMem* BLAST_GreedyAlignsfree(SGreedyAlignMem* gamp)
{
   if (gamp->flast_d) {
      sfree(gamp->flast_d[0]);
      sfree(gamp->flast_d);
   } else {
      if (gamp->flast_d_affine) {
         sfree(gamp->flast_d_affine[0]);
         sfree(gamp->flast_d_affine);
      }
      sfree(gamp->uplow_free);
   }
   sfree(gamp->max_row_free);
   if (gamp->space)
      MBSpaceFree(gamp->space);
   sfree(gamp);
   return NULL;
}

/** Allocate memory for the greedy gapped alignment algorithm
 * @param score_params Parameters related to scoring [in]
 * @param ext_params Options and parameters related to the extension [in]
 * @param max_dbseq_length The length of the longest sequence in the 
 *        database [in]
 * @return The allocated SGreedyAlignMem structure
 */
static SGreedyAlignMem* 
BLAST_GreedyAlignMemAlloc(const BlastScoringParameters* score_params,
		       const BlastExtensionParameters* ext_params,
		       Int4 max_dbseq_length)
{
#define ERROR_FRACTION 2    /* N.B.: This value should match the value of
                               ERROR_FRACTION in the anonymous enum in
                               greedy_align.c */
#define ICEIL(x,y) ((((x)-1)/(y))+1)    /* FIXME: duplicated from greedy_align.c */
   SGreedyAlignMem* gamp;
   Int4 max_d, max_d_1, Xdrop, d_diff, max_cost, gd, i;
   Int4 reward, penalty, gap_open, gap_extend;
   Int4 Mis_cost, GE_cost;
   Boolean do_traceback;
   
   if (score_params == NULL || ext_params == NULL) 
      return NULL;
   
   do_traceback = 
      (ext_params->options->ePrelimGapExt != eGreedyExt);

   if (score_params->reward % 2 == 1) {
      reward = 2*score_params->reward;
      penalty = -2*score_params->penalty;
      Xdrop = 2*ext_params->gap_x_dropoff;
      gap_open = 2*score_params->gap_open;
      gap_extend = 2*score_params->gap_extend;
   } else {
      reward = score_params->reward;
      penalty = -score_params->penalty;
      Xdrop = ext_params->gap_x_dropoff;
      gap_open = score_params->gap_open;
      gap_extend = score_params->gap_extend;
   }

   if (gap_open == 0 && gap_extend == 0)
      gap_extend = reward / 2 + penalty;

   max_d = (Int4) (max_dbseq_length / ERROR_FRACTION + 1);

   gamp = (SGreedyAlignMem*) calloc(1, sizeof(SGreedyAlignMem));

   if (score_params->gap_open==0 && score_params->gap_extend==0) {
      d_diff = ICEIL(Xdrop+reward/2, penalty+reward);
   
      gamp->flast_d = (Int4**) malloc((max_d + 2) * sizeof(Int4*));
      if (gamp->flast_d == NULL) {
         sfree(gamp);
         return NULL;
      }
      gamp->flast_d[0] = 
         (Int4*) malloc((max_d + max_d + 6) * sizeof(Int4) * 2);
      if (gamp->flast_d[0] == NULL) {
#ifdef ERR_POST_EX_DEFINED
	 ErrPostEx(SEV_WARNING, 0, 0, 
              "Failed to allocate %ld bytes for greedy alignment", 
              (max_d + max_d + 6) * sizeof(Int4) * 2);
#endif
         BLAST_GreedyAlignsfree(gamp);
         return NULL;
      }

      gamp->flast_d[1] = gamp->flast_d[0] + max_d + max_d + 6;
      gamp->flast_d_affine = NULL;
      gamp->uplow_free = NULL;
   } else {
      gamp->flast_d = NULL;
      Mis_cost = reward + penalty;
      GE_cost = gap_extend+reward/2;
      max_d_1 = max_d;
      max_d *= GE_cost;
      max_cost = MAX(Mis_cost, gap_open+GE_cost);
      gd = gdb3(&Mis_cost, &gap_open, &GE_cost);
      d_diff = ICEIL(Xdrop+reward/2, gd);
      gamp->uplow_free = (Int4*) calloc(2*(max_d+1+max_cost), sizeof(Int4));
      gamp->flast_d_affine = (SThreeVal**) 
	 malloc((MAX(max_d, max_cost) + 2) * sizeof(SThreeVal*));
      if (!gamp->uplow_free || !gamp->flast_d_affine) {
         BLAST_GreedyAlignsfree(gamp);
         return NULL;
      }
      gamp->flast_d_affine[0] = (SThreeVal*)
	 calloc((2*max_d_1 + 6) , sizeof(SThreeVal) * (max_cost+1));
      for (i = 1; i <= max_cost; i++)
	 gamp->flast_d_affine[i] = 
	    gamp->flast_d_affine[i-1] + 2*max_d_1 + 6;
      if (!gamp->flast_d_affine || !gamp->flast_d_affine[0])
         BLAST_GreedyAlignsfree(gamp);
   }
   gamp->max_row_free = (Int4*) malloc(sizeof(Int4) * (max_d + 1 + d_diff));

   if (do_traceback)
      gamp->space = MBSpaceNew();
   if (!gamp->max_row_free || (do_traceback && !gamp->space))
      /* Failure in one of the memory allocations */
      BLAST_GreedyAlignsfree(gamp);

   return gamp;
}

/* Documented in blast_gapalign.h */
BlastGapAlignStruct* 
BLAST_GapAlignStructFree(BlastGapAlignStruct* gap_align)
{
   if (!gap_align)
      return NULL;

   GapEditBlockDelete(gap_align->edit_block);
   if (gap_align->greedy_align_mem)
      BLAST_GreedyAlignsfree(gap_align->greedy_align_mem);
   GapStateFree(gap_align->state_struct);

   sfree(gap_align);
   return NULL;
}

/* Documented in blast_gapalign.h */
Int2
BLAST_GapAlignStructNew(const BlastScoringParameters* score_params, 
   const BlastExtensionParameters* ext_params, 
   Uint4 max_subject_length,
   BlastScoreBlk* sbp, BlastGapAlignStruct** gap_align_ptr)
{
   Int2 status = 0;
   BlastGapAlignStruct* gap_align;

   /* If pointer to output structure is NULL, just don't do anything */ 
   if (!gap_align_ptr)
      return 0;

   if (!gap_align_ptr || !sbp || !score_params || !ext_params)
      return -1;

   gap_align = (BlastGapAlignStruct*) calloc(1, sizeof(BlastGapAlignStruct));

   *gap_align_ptr = gap_align;

   gap_align->sbp = sbp;

   gap_align->gap_x_dropoff = ext_params->gap_x_dropoff;

   if (ext_params->options->ePrelimGapExt != eDynProgExt) {
      max_subject_length = MIN(max_subject_length, MAX_DBSEQ_LEN);
      gap_align->greedy_align_mem = 
         BLAST_GreedyAlignMemAlloc(score_params, ext_params, 
                                max_subject_length);
      if (!gap_align->greedy_align_mem)
         gap_align = BLAST_GapAlignStructFree(gap_align);
   }

   if (!gap_align)
      return -1;

   gap_align->positionBased = (sbp->posMatrix != NULL);

   return status;
}

/** Low level function to perform dynamic programming gapped extension 
 * with traceback.
 * @param A The query sequence [in]
 * @param B The subject sequence [in]
 * @param M Maximal extension length in query [in]
 * @param N Maximal extension length in subject [in]
 * @param S The traceback information from previous extension [in]
 * @param a_offset Resulting starting offset in query [out]
 * @param b_offset Resulting starting offset in subject [out]
 * @param sapp The traceback information [out]
 * @param gap_align Structure holding various information and allocated 
 *        memory for the gapped alignment [in]
 * @param score_params Parameters related to scoring [in]
 * @param query_offset The starting offset in query [in]
 * @param reversed Has the sequence been reversed? Used for psi-blast [in]
 * @param reverse_sequence Do reverse the sequence [in]
 * @return The best alignment score found.
*/

#define SCRIPT_SUB      0x00
#define SCRIPT_INS      0x01
#define SCRIPT_DEL      0x02
#define SCRIPT_DECLINE  0x03

#define SCRIPT_OP_MASK  0x03

#define SCRIPT_OPEN_GAP 0x04
#define SCRIPT_INS_ROW  0x10
#define SCRIPT_DEL_ROW  0x20
#define SCRIPT_INS_COL  0x40
#define SCRIPT_DEL_COL  0x80

Int4
ALIGN_EX(Uint1* A, Uint1* B, Int4 M, Int4 N, Int4* S, Int4* a_offset, 
	Int4* b_offset, Int4** sapp, BlastGapAlignStruct* gap_align, 
	const BlastScoringParameters* score_params, Int4 query_offset, 
        Boolean reversed, Boolean reverse_sequence)
	
{ 
    /* See SEMI_G_ALIGN_EX for more general comments on 
       what this code is doing; comments in this function
       only apply to the traceback computations */

    BlastGapDP* score_array;
    Int4 i; 
    Int4 a_index;
    Int4 b_index, b_size, first_b_index, last_b_index, b_increment;
    Uint1* b_ptr;
  
    Int4 gap_open;
    Int4 gap_extend;
    Int4 gap_open_extend;
    Int4 decline_penalty;
    Int4 x_dropoff;
    Int4 best_score;
  
    Int4* *matrix;
    Int4* matrix_row;
  
    Int4 score;
    Int4 score_gap_row;
    Int4 score_gap_col;
    Int4 score_decline;
    Int4 next_score;
    Int4 next_score_decline;
  
    GapData data;                       /* traceback variables */
    GapStateArrayStruct* state_struct;
    Uint1** edit_script;
    Uint1* edit_script_row;
    Int4 *edit_start_offset;
    Int4 orig_b_index;
    Uint1 script, next_script, script_row, script_col;
    Int4 align_len;

    matrix = gap_align->sbp->matrix;
    *a_offset = 0;
    *b_offset = 0;
    gap_open = score_params->gap_open;
    gap_extend = score_params->gap_extend;
    gap_open_extend = gap_open + gap_extend;
    decline_penalty = score_params->decline_align;
    x_dropoff = gap_align->gap_x_dropoff;
  
    if (x_dropoff < gap_open_extend)
        x_dropoff = gap_open_extend;
  
    if(N <= 0 || M <= 0) 
        return 0;
  
    /* Initialize traceback information. edit_script[] is
       a 2-D array which is filled in row by row as the 
       dynamic programming takes place */

    data.sapp = S;
    data.last = 0;
    *sapp = S;
    GapPurgeState(gap_align->state_struct);

    /* Conceptually, traceback requires a byte array of size
       MxN. To save on memory, each row of the array is allocated
       separately. edit_script[i] points to the storage reserved
       for row i, and edit_start_offset[i] gives the offset into
       the B sequence corresponding to element 0 of edit_script[i] */

    edit_script = (Uint1**) malloc(sizeof(Uint1*)*(M+1));
    edit_start_offset = (Int4*) malloc(sizeof(Int4)*(M+1));

    /* allocate storage for the first row of the traceback
       array. Because row elements correspond to gaps in A,
       the alignment can only go x_dropoff/gap_extend positions
       at most before failing the X dropoff criterion */

    if (gap_extend > 0)
        state_struct = GapGetState(&gap_align->state_struct, 
                                   x_dropoff / gap_extend + 5);
    else
        state_struct = GapGetState(&gap_align->state_struct, N + 3);
    edit_script[0] = state_struct->state_array;
    edit_start_offset[0] = 0;
    edit_script_row = state_struct->state_array;

    score_array = (BlastGapDP*)malloc((N + 2) * sizeof(BlastGapDP));
    score = -gap_open_extend;
    score_array[0].best = 0;
    score_array[0].best_gap = -gap_open_extend;
    score_array[0].best_decline = -gap_open_extend - decline_penalty;
  
    for (i = 1; i <= N; i++) {
        if (score < -x_dropoff) 
            break;

        score_array[i].best = score;
        score_array[i].best_gap = score - gap_open_extend; 
        score_array[i].best_decline = score - gap_open_extend - decline_penalty;
        score -= gap_extend;
        edit_script_row[i] = SCRIPT_INS;
    }
    state_struct->used = i + 1;
  
    b_size = i;
    best_score = 0;
    first_b_index = 0;
    if (reverse_sequence)
        b_increment = -1;
    else
        b_increment = 1;
  
    for (a_index = 1; a_index <= M; a_index++) {

        /* Set up the next row of the edit script; this involves
           allocating memory for the row, then pointing to it.
           It is not necessary to allocate space for offsets less
           than first_b_index (since the inner loop will never 
           look at them) and the alignment can go at most
           x_dropoff/gap_extend positions to the right beyond b_size
           before failing the X dropoff test. */

        if (gap_extend > 0)
            state_struct = GapGetState(&gap_align->state_struct, 
                          b_size + x_dropoff / gap_extend - first_b_index + 5);
        else
            state_struct = GapGetState(&gap_align->state_struct, 
                          N + 3 - first_b_index);
        edit_script[a_index] = state_struct->state_array + 
                                state_struct->used + 1;
        edit_start_offset[a_index] = first_b_index;

        /* the inner loop assumes the current traceback
           row begins at offset zero of B */

        edit_script_row = edit_script[a_index] - first_b_index;
        orig_b_index = first_b_index;

        if (!(gap_align->positionBased)) {
            if(reverse_sequence)
                matrix_row = matrix[ A[ M - a_index ] ];
            else
                matrix_row = matrix[ A[ a_index ] ];
        }
        else {
            if(reversed || reverse_sequence)
                matrix_row = gap_align->sbp->posMatrix[M - a_index];
            else
                matrix_row = gap_align->sbp->posMatrix[a_index + query_offset];
        }

        if(reverse_sequence)
            b_ptr = &B[N - first_b_index];
        else
            b_ptr = &B[first_b_index];

        score = MININT;
        score_gap_row = MININT;
        score_decline = MININT;
        last_b_index = first_b_index;

        for (b_index = first_b_index; b_index < b_size; b_index++) {

            b_ptr += b_increment;
            score_gap_col = score_array[b_index].best_gap;
            next_score = score_array[b_index].best + matrix_row[ *b_ptr ];
            next_score_decline = score_array[b_index].best_decline;

            /* script, script_row and script_col contain the
               actions specified by the dynamic programming.
               when the inner loop has finished, 'script' con-
               tains all of the actions to perform, and is
               written to edit_script[a_index][b_index]. Otherwise,
               this inner loop is exactly the same as the one
               in SEMI_G_ALIGN_EX() */

            if (score_decline > score) {
                script = SCRIPT_DECLINE;
                score = score_decline;
            }
            else {
                script = SCRIPT_SUB;
            }

            if (score_gap_col < score_decline) {
                score_gap_col = score_decline;
                script_col = SCRIPT_DEL_COL;
            }
            else {
                script_col = SCRIPT_INS_COL;
                if (score < score_gap_col) {
                    script = SCRIPT_DEL;
                    score = score_gap_col;
                }
            }

            if (score_gap_row < score_decline) {
                score_gap_row = score_decline;
                script_row = SCRIPT_DEL_ROW;
            }
            else {
                script_row = SCRIPT_INS_ROW;
                if (score < score_gap_row) {
                    script = SCRIPT_INS;
                    score = score_gap_row;
                }
            }

            if (best_score - score > x_dropoff) {

                if (first_b_index == b_index)
                    first_b_index++;
                else
                    score_array[b_index].best = MININT;
            }
            else {
                last_b_index = b_index;
                if (score > best_score) {
                    best_score = score;
                    *a_offset = a_index;
                    *b_offset = b_index;
                }

                score_gap_row -= gap_extend;
                score_gap_col -= gap_extend;
                if (score_gap_col < (score - gap_open_extend)) {
                    score_array[b_index].best_gap = score - gap_open_extend;
                }
                else {
                    score_array[b_index].best_gap = score_gap_col;
                    script += script_col;
                }

                if (score_gap_row < (score - gap_open_extend)) 
                    score_gap_row = score - gap_open_extend;
                else 
                    script += script_row;

                if (score_decline < (score - gap_open)) {
                    score_array[b_index].best_decline = score - gap_open - decline_penalty;
                }
                else {
                    score_array[b_index].best_decline = score_decline - decline_penalty;
                    script += SCRIPT_OPEN_GAP;
                }
                score_array[b_index].best = score;
            }

            score = next_score;
            score_decline = next_score_decline;
            edit_script_row[b_index] = script;
        }
  
        if (first_b_index == b_size) {
            a_index++;
            break;
        }

        if (last_b_index < b_size - 1) {
            b_size = last_b_index + 1;
        }
        else {
            while (score_gap_row >= (best_score - x_dropoff) && b_size <= N) {
                score_array[b_size].best = score_gap_row;
                score_array[b_size].best_gap = score_gap_row - gap_open_extend;
                score_array[b_size].best_decline = score_gap_row - gap_open -
                                                        decline_penalty;
                score_gap_row -= gap_extend;
                edit_script_row[b_size] = SCRIPT_INS;
                b_size++;
            }
        }

        if (b_size <= N) {
            score_array[b_size].best = MININT;
            score_array[b_size].best_gap = MININT;
            score_array[b_size].best_decline = MININT;
            b_size++;
        }
        state_struct->used += MAX(b_index, b_size) - orig_b_index + 1;
    }
    
    /* Pick the optimal path through the now complete
       edit_script[][]. This is equivalent to flattening 
       the 2-D array into a 1-D list of actions. */

    a_index = *a_offset;
    b_index = *b_offset;
    script = 0;
    edit_script_row = (Uint1 *)malloc(a_index + b_index);

    for (i = 0; a_index > 0 || b_index > 0; i++) {
        /* Retrieve the next action to perform. Rows of
           the traceback array do not necessarily start
           at offset zero of B, so a correction is needed
           to point to the correct position */

        next_script = 
            edit_script[a_index][b_index - edit_start_offset[a_index]];

        switch(script) {
        case SCRIPT_INS:
            script = next_script & SCRIPT_OP_MASK;
            if (next_script & SCRIPT_INS_ROW)
                script = SCRIPT_INS;
            else if (next_script & SCRIPT_DEL_ROW)
                script = SCRIPT_DECLINE;
            break;

        case SCRIPT_DEL:
            script = next_script & SCRIPT_OP_MASK;
            if (next_script & SCRIPT_INS_COL)
                script = SCRIPT_DEL;
            else if (next_script & SCRIPT_DEL_COL)
                script = SCRIPT_DECLINE;
            break;

        case SCRIPT_DECLINE:
            script = next_script & SCRIPT_OP_MASK;
            if (next_script & SCRIPT_OPEN_GAP)
                script = SCRIPT_DECLINE;
            break;

        default:
            script = next_script & SCRIPT_OP_MASK;
            break;
        }

        if (script == SCRIPT_INS)
            b_index--;
        else if (script == SCRIPT_DEL)
            a_index--;
        else {
            a_index--;
            b_index--;
        }
        edit_script_row[i] = script;
    }

    /* Traceback proceeded backwards through edit_script, 
       so the output traceback information is written 
       in reverse order */

    align_len = i;
    for (i--; i >= 0; i--) {
        if (edit_script_row[i] == SCRIPT_SUB) 
            REP_
        else if (edit_script_row[i] == SCRIPT_INS) 
            INS_(1)
        else if (edit_script_row[i] == SCRIPT_DECLINE) 
            REPP_                     
        else 
            DEL_(1)      
    }
      
    sfree(edit_start_offset);
    sfree(edit_script_row);
    sfree(edit_script);
    sfree(score_array);
    align_len -= data.sapp - S;
    if (align_len > 0)
        memset(data.sapp, 0, align_len);
    *sapp = data.sapp;

    return best_score;
}

/** Low level function to perform gapped extension in one direction with 
 * or without traceback.
 * @param A The query sequence [in]
 * @param B The subject sequence [in]
 * @param M Maximal extension length in query [in]
 * @param N Maximal extension length in subject [in]
 * @param S The traceback information from previous extension [in]
 * @param a_offset Resulting starting offset in query [out]
 * @param b_offset Resulting starting offset in subject [out]
 * @param score_only Only find the score, without saving traceback [in]
 * @param sapp The traceback information [out]
 * @param gap_align Structure holding various information and allocated 
 *        memory for the gapped alignment [in]
 * @param score_params Parameters related to scoring [in]
 * @param query_offset The starting offset in query [in]
 * @param reversed Has the sequence been reversed? Used for psi-blast [in]
 * @param reverse_sequence Do reverse the sequence [in]
 * @return The best alignment score found.
 */
static Int4 SEMI_G_ALIGN_EX(Uint1* A, Uint1* B, Int4 M, Int4 N,
   Int4* S, Int4* a_offset, Int4* b_offset, Boolean score_only, Int4** sapp,
   BlastGapAlignStruct* gap_align, const BlastScoringParameters* score_params, 
   Int4 query_offset, Boolean reversed, Boolean reverse_sequence)
{
    BlastGapDP* score_array;            /* sequence pointers and indices */
    Int4 i; 
    Int4 a_index;
    Int4 b_index, b_size, first_b_index, last_b_index, b_increment;
    Uint1* b_ptr;
  
    Int4 gap_open;              /* alignment penalty variables */
    Int4 gap_extend;
    Int4 gap_open_extend;
    Int4 decline_penalty;
    Int4 x_dropoff;
  
    Int4* *matrix;              /* pointers to the score matrix */
    Int4* matrix_row;
  
    Int4 score;                 /* score tracking variables */
    Int4 score_gap_row;
    Int4 score_gap_col;
    Int4 score_decline;
    Int4 next_score;
    Int4 next_score_decline;
    Int4 best_score;
  
    if (!score_only) {
        return ALIGN_EX(A, B, M, N, S, a_offset, b_offset, sapp, gap_align, 
                      score_params, query_offset, reversed, reverse_sequence);
    }
    
    /* do initialization and sanity-checking */

    matrix = gap_align->sbp->matrix;
    *a_offset = 0;
    *b_offset = 0;
    gap_open = score_params->gap_open;
    gap_extend = score_params->gap_extend;
    gap_open_extend = gap_open + gap_extend;
    decline_penalty = score_params->decline_align;
    x_dropoff = gap_align->gap_x_dropoff;
  
    if (x_dropoff < gap_open_extend)
        x_dropoff = gap_open_extend;
  
    if(N <= 0 || M <= 0) 
        return 0;
  
    /* Allocate and fill in the auxiliary 
       bookeeping structures (one struct
       per letter of B) */

    score_array = (BlastGapDP*)malloc((N + 2) * sizeof(BlastGapDP));
    score = -gap_open_extend;
    score_array[0].best = 0;
    score_array[0].best_gap = -gap_open_extend;
    score_array[0].best_decline = -gap_open_extend - decline_penalty;
  
    for (i = 1; i <= N; i++) {
        if (score < -x_dropoff) 
            break;

        score_array[i].best = score;
        score_array[i].best_gap = score - gap_open_extend; 
        score_array[i].best_decline = score - gap_open_extend - decline_penalty;
        score -= gap_extend;
    }
  
    /* The inner loop below examines letters of B from 
       index 'first_b_index' to 'b_size' */

    b_size = i;
    best_score = 0;
    first_b_index = 0;
    if (reverse_sequence)
        b_increment = -1;
    else
        b_increment = 1;
  
    for (a_index = 1; a_index <= M; a_index++) {
        /* pick out the row of the score matrix 
           appropriate for A[a_index] */

        if (!(gap_align->positionBased)) {
            if(reverse_sequence)
                matrix_row = matrix[ A[ M - a_index ] ];
            else
                matrix_row = matrix[ A[ a_index ] ];
        }
        else {
            if(reversed || reverse_sequence)
                matrix_row = gap_align->sbp->posMatrix[M - a_index];
            else 
                matrix_row = gap_align->sbp->posMatrix[a_index + query_offset];
        }

        if(reverse_sequence)
            b_ptr = &B[N - first_b_index];
        else
            b_ptr = &B[first_b_index];

        /* initialize running-score variables */
        score = MININT;
        score_gap_row = MININT;
        score_decline = MININT;
        last_b_index = first_b_index;

        for (b_index = first_b_index; b_index < b_size; b_index++) {

            b_ptr += b_increment;
            score_gap_col = score_array[b_index].best_gap;
            next_score = score_array[b_index].best + matrix_row[ *b_ptr ];
            next_score_decline = score_array[b_index].best_decline;

            /* decline the alignment if that improves the score */

            score = MAX(score, score_decline);
            
            /* decline the best row score if that improves it;
               if not, make it the new high score if it's
               an improvement */

            if (score_gap_col < score_decline)
                score_gap_col = score_decline;
            else if (score < score_gap_col)
                score = score_gap_col;

            /* decline the best column score if that improves it;
               if not, make it the new high score if it's
               an improvement */

            if (score_gap_row < score_decline)
                score_gap_row = score_decline;
            else if (score < score_gap_row)
                score = score_gap_row;

            if (best_score - score > x_dropoff) {

                /* the current best score failed the X-dropoff
                   criterion. Note that this does not stop the
                   inner loop, only forces future iterations to
                   skip this column of B. 

                   Also, if the very first letter of B that was
                   tested failed the X dropoff criterion, make
                   sure future inner loops start one letter to 
                   the right */

                if (b_index == first_b_index)
                    first_b_index++;
                else
                    score_array[b_index].best = MININT;
            }
            else {
                last_b_index = b_index;
                if (score > best_score) {
                    best_score = score;
                    *a_offset = a_index;
                    *b_offset = b_index;
                }

                /* If starting a gap at this position will improve
                   the best row, column, or declined alignment score, 
                   update them to reflect that. */

                score_gap_row -= gap_extend;
                score_gap_col -= gap_extend;
                score_array[b_index].best_gap = MAX(score - gap_open_extend,
                                                    score_gap_col);
                score_gap_row = MAX(score - gap_open_extend, score_gap_row);

                score_array[b_index].best_decline = 
                        MAX(score_decline, score - gap_open) - decline_penalty;
                score_array[b_index].best = score;
            }

            score = next_score;
            score_decline = next_score_decline;
        }
  
        /* Finish aligning if the best scores for all positions
           of B will fail the X-dropoff test, i.e. the inner loop 
           bounds have converged to each other */

        if (first_b_index == b_size)
            break;

        if (last_b_index < b_size - 1) {
            /* This row failed the X-dropoff test earlier than
               the last row did; just shorten the loop bounds
               before doing the next row */

            b_size = last_b_index + 1;
        }
        else {
            /* The inner loop finished without failing the X-dropoff
               test; initialize extra bookkeeping structures until
               the X dropoff test fails or we run out of letters in B. 
               The next inner loop will have larger bounds */

            while (score_gap_row >= (best_score - x_dropoff) && b_size <= N) {
                score_array[b_size].best = score_gap_row;
                score_array[b_size].best_gap = score_gap_row - gap_open_extend;
                score_array[b_size].best_decline = score_gap_row - gap_open -
                                                        decline_penalty;
                score_gap_row -= gap_extend;
                b_size++;
            }
        }

        if (b_size <= N) {
            score_array[b_size].best = MININT;
            score_array[b_size].best_gap = MININT;
            score_array[b_size].best_decline = MININT;
            b_size++;
        }
    }
    
    sfree(score_array);
    return best_score;
}

/** Low level function to perform gapped extension with out-of-frame
 * gapping with traceback 
 * @param A The query sequence [in]
 * @param B The subject sequence [in]
 * @param M Maximal extension length in query [in]
 * @param N Maximal extension length in subject [in]
 * @param S The traceback information from previous extension [in]
 * @param a_offset Resulting starting offset in query [out]
 * @param b_offset Resulting starting offset in subject [out]
 * @param sapp The traceback information [out]
 * @param gap_align Structure holding various information and allocated 
 *        memory for the gapped alignment [in]
 * @param score_params Parameters related to scoring [in]
 * @param query_offset The starting offset in query [in]
 * @param reversed Has the sequence been reversed? Used for psi-blast [in]
 * @return The best alignment score found.
 */

#define SCRIPT_GAP_IN_A                 0
#define SCRIPT_AHEAD_ONE_FRAME          1
#define SCRIPT_AHEAD_TWO_FRAMES         2
#define SCRIPT_NEXT_IN_FRAME            3
#define SCRIPT_NEXT_PLUS_ONE_FRAME      4
#define SCRIPT_NEXT_PLUS_TWO_FRAMES     5
#define SCRIPT_GAP_IN_B                 6
#define SCRIPT_OOF_OP_MASK              0x07

#define SCRIPT_OOF_OPEN_GAP             0x10
#define SCRIPT_EXTEND_GAP_IN_A          0x20
#define SCRIPT_EXTEND_GAP_IN_B          0x40

static Int4 OOF_ALIGN(Uint1* A, Uint1* B, Int4 M, Int4 N,
   Int4* S, Int4* a_offset, Int4* b_offset, Int4** sapp, 
   BlastGapAlignStruct* gap_align, const BlastScoringParameters* score_params,
   Int4 query_offset, Boolean reversed)
{
    BlastGapDP* score_array;            /* sequence pointers and indices */
    Int4 i, increment; 
    Int4 a_index;
    Int4 b_index, b_size, first_b_index, last_b_index;
  
    Int4 gap_open;              /* alignment penalty variables */
    Int4 gap_extend;
    Int4 gap_open_extend;
    Int4 shift_penalty;
    Int4 x_dropoff;
  
    Int4* *matrix;              /* pointers to the score matrix */
    Int4* matrix_row;
  
    Int4 score;                 /* score tracking variables */
    Int4 score_row1; 
    Int4 score_row2; 
    Int4 score_row3;
    Int4 score_gap_col; 
    Int4 score_col1; 
    Int4 score_col2; 
    Int4 score_col3;
    Int4 score_other_frame1; 
    Int4 score_other_frame2; 
    Int4 best_score;
  
    GapData data;                       /* traceback variables */
    GapStateArrayStruct* state_struct;
    Uint1** edit_script;
    Uint1* edit_script_row;
    Int4 orig_b_index;
    Int1 script, next_script;
    Int4 align_len;
    Int4 *edit_start_offset;

    /* do initialization and sanity-checking */

    matrix = gap_align->sbp->matrix;
    *a_offset = 0;
    *b_offset = -2;
    gap_open = score_params->gap_open;
    gap_extend = score_params->gap_extend;
    gap_open_extend = gap_open + gap_extend;
    shift_penalty = score_params->shift_pen;
    x_dropoff = gap_align->gap_x_dropoff;
  
    if (x_dropoff < gap_open_extend)
        x_dropoff = gap_open_extend;
  
    if(N <= 0 || M <= 0) 
        return 0;
  
    /* Initialize traceback information. edit_script[] is
       a 2-D array which is filled in row by row as the 
       dynamic programming takes place */

    data.sapp = S;
    data.last = 0;
    *sapp = S;
    GapPurgeState(gap_align->state_struct);

    /* Conceptually, traceback requires a byte array of size
       MxN. To save on memory, each row of the array is allocated
       separately. edit_script[i] points to the storage reserved
       for row i, and edit_start_offset[i] gives the offset into
       the B sequence corresponding to element 0 of edit_script[i] */

    edit_script = (Uint1**) malloc(sizeof(Uint1*)*(M+1));
    edit_start_offset = (Int4*) malloc(sizeof(Int4)*(M+1));

    /* allocate storage for the first row of the traceback array */

    state_struct = GapGetState(&gap_align->state_struct, N + 5);
    edit_script[0] = state_struct->state_array;
    edit_start_offset[0] = 0;
    edit_script_row = state_struct->state_array;

    /* Allocate and fill in the auxiliary 
       bookeeping structures (one struct
       per letter of B) */

    score_array = (BlastGapDP*)calloc((N + 4), sizeof(BlastGapDP));
    score = -gap_open_extend;
    score_array[0].best = 0;
    score_array[0].best_gap = -gap_open_extend;
  
    for (i = 3; i <= N + 2; i += 3) {
        score_array[i].best = score;
        score_array[i].best_gap = score - gap_open_extend; 
        edit_script_row[i] = SCRIPT_GAP_IN_B;

        score_array[i-1].best = MININT;
        score_array[i-1].best_gap = MININT;
        edit_script_row[i-1] = SCRIPT_GAP_IN_B;

        score_array[i-2].best = MININT;
        score_array[i-2].best_gap = MININT;
        edit_script_row[i-2] = SCRIPT_GAP_IN_B;

        if (score < -x_dropoff) 
            break;
        score -= gap_extend;
    }
  
    /* The inner loop below examines letters of B from 
       index 'first_b_index' to 'b_size' */

    b_size = i - 2;
    state_struct->used = b_size + 1;
    score_array[b_size].best = MININT;
    score_array[b_size].best_gap = MININT;

    best_score = 0;
    first_b_index = 0;
    if (reversed) {
        increment = -1;
    }
    else {
        /* Allow for a backwards frame shift */
        B -= 2;
        increment = 1;
    }
  
    for (a_index = 1; a_index <= M; a_index++) {

        /* Set up the next row of the edit script; this involves
           allocating memory for the row, then pointing to it.
           It is not necessary to allocate space for offsets less
           than first_b_index (since the inner loop will never 
           look at them) */

        state_struct = GapGetState(&gap_align->state_struct, 
                                    N + 5 - first_b_index);

        edit_script[a_index] = state_struct->state_array + 
                                state_struct->used + 1;
        edit_start_offset[a_index] = first_b_index;

        /* the inner loop assumes the current traceback
           row begins at offset zero of B */

        edit_script_row = edit_script[a_index] - first_b_index;
        orig_b_index = first_b_index;

        if (!(gap_align->positionBased)) {
            matrix_row = matrix[ A[ a_index * increment ] ];
        }
        else {
            if(reversed)
                matrix_row = gap_align->sbp->posMatrix[M - a_index];
            else 
                matrix_row = gap_align->sbp->posMatrix[a_index + query_offset];
        }

        score = MININT;
        score_row1 = MININT; 
        score_row2 = MININT; 
        score_row3 = MININT;
        score_gap_col = MININT; 
        score_col1 = MININT; 
        score_col2 = MININT; 
        score_col3 = MININT;
        score_other_frame1 = MININT; 
        score_other_frame2 = MININT; 
        last_b_index = first_b_index;
        b_index = first_b_index;

        /* The inner loop is identical to that of OOF_SEMI_G_ALIGN,
           except that traceback operations are sprinkled throughout. */

        while (b_index < b_size) {

            /* FRAME 0 */

            score = MAX(score_other_frame1, score_other_frame2) - shift_penalty;
            score = MAX(score, score_col1);
            if (score == score_col1) {
                script = SCRIPT_NEXT_IN_FRAME;
            }
            else if (score + shift_penalty == score_other_frame1) {
                if (score_other_frame1 == score_col2)
                    script = SCRIPT_AHEAD_TWO_FRAMES;
                else
                    script = SCRIPT_NEXT_PLUS_TWO_FRAMES;
            }
            else {
                if (score_other_frame2 == score_col3)
                    script = SCRIPT_AHEAD_ONE_FRAME;
                else
                    script = SCRIPT_NEXT_PLUS_ONE_FRAME;
            }
            score += matrix_row[ B[ b_index * increment ] ];

            score_other_frame1 = MAX(score_col1, score_array[b_index].best);
            score_col1 = score_array[b_index].best;
            score_gap_col = score_array[b_index].best_gap;

            if (score < MAX(score_gap_col, score_row1)) {
                if (score_gap_col > score_row1) {
                    score = score_gap_col;
                    script = SCRIPT_OOF_OPEN_GAP | SCRIPT_GAP_IN_A;
                }
                else {
                    score = score_row1;
                    script = SCRIPT_OOF_OPEN_GAP | SCRIPT_GAP_IN_B;
                }

                if (best_score - score > x_dropoff) {
                    if (first_b_index == b_index) 
                        first_b_index = b_index + 1;
                    else
                        score_array[b_index].best = MININT;
                }
                else {
                    last_b_index = b_index + 1;
                    score_array[b_index].best = score;
                    score_array[b_index].best_gap = score_gap_col - gap_extend;
                    score_row1 -= gap_extend;
                }
            }
            else {
                if (best_score - score > x_dropoff) {
                    if (first_b_index == b_index) 
                        first_b_index = b_index + 1;
                    else
                        score_array[b_index].best = MININT;
                }
                else {
                    last_b_index = b_index + 1;
                    score_array[b_index].best = score;
                    if (score > best_score) {
                        best_score = score;
                        *a_offset = a_index;
                        *b_offset = b_index;
                    }

                    score -= gap_open_extend;
                    score_row1 -= gap_extend;
                    if (score > score_row1)
                        score_row1 = score;
                    else
                        script |= SCRIPT_EXTEND_GAP_IN_B;

                    score_gap_col -= gap_extend;
                    if (score < score_gap_col) {
                        score_array[b_index].best_gap = score_gap_col;
                        script |= SCRIPT_EXTEND_GAP_IN_A;
                    }
                    else {
                        score_array[b_index].best_gap = score;
                    }
                }
            }

            edit_script_row[b_index] = script;
            if (++b_index >= b_size) {
                score = score_row1;
                score_row1 = score_row2;
                score_row2 = score_row3;
                score_row3 = score;
                break;
            }

            /* FRAME 1 */

            score = MAX(score_other_frame1, score_other_frame2) - shift_penalty;
            score = MAX(score, score_col2);
            if (score == score_col2) {
                script = SCRIPT_NEXT_IN_FRAME;
            }
            else if (score + shift_penalty == score_other_frame1) {
                if (score_other_frame1 == score_col1)
                    script = SCRIPT_AHEAD_ONE_FRAME;
                else
                    script = SCRIPT_NEXT_PLUS_ONE_FRAME;
            }
            else {
                if (score_other_frame2 == score_col3)
                    script = SCRIPT_AHEAD_TWO_FRAMES;
                else
                    script = SCRIPT_NEXT_PLUS_TWO_FRAMES;
            }
            score += matrix_row[ B[ b_index * increment ] ];
            score_other_frame2 = MAX(score_col2, score_array[b_index].best);
            score_col2 = score_array[b_index].best;
            score_gap_col = score_array[b_index].best_gap;

            if (score < MAX(score_gap_col, score_row2)) {
                score = MAX(score_gap_col, score_row2);
                if (best_score - score > x_dropoff) {
                    if (first_b_index == b_index) 
                        first_b_index = b_index + 1;
                    else
                        score_array[b_index].best = MININT;
                }
                else {
                    if (score == score_gap_col)
                        script = SCRIPT_OOF_OPEN_GAP | SCRIPT_GAP_IN_A;
                    else 
                        script = SCRIPT_OOF_OPEN_GAP | SCRIPT_GAP_IN_B;

                    last_b_index = b_index + 1;
                    score_array[b_index].best = score;
                    score_array[b_index].best_gap = score_gap_col - gap_extend;
                    score_row2 -= gap_extend;
                }
            }
            else {
                if (best_score - score > x_dropoff) {
                    if (first_b_index == b_index) 
                        first_b_index = b_index + 1;
                    else
                        score_array[b_index].best = MININT;
                }
                else {
                    last_b_index = b_index + 1;
                    score_array[b_index].best = score;
                    if (score > best_score) {
                        best_score = score;
                        *a_offset = a_index;
                        *b_offset = b_index;
                    }
                    score -= gap_open_extend;
                    score_row2 -= gap_extend;
                    if (score > score_row2)
                        score_row2 = score;
                    else
                        script |= SCRIPT_EXTEND_GAP_IN_B;

                    score_gap_col -= gap_extend;
                    if (score < score_gap_col) {
                        score_array[b_index].best_gap = score_gap_col;
                        script |= SCRIPT_EXTEND_GAP_IN_A;
                    }
                    else {
                        score_array[b_index].best_gap = score;
                    }
                }
            }

            edit_script_row[b_index] = script;
            ++b_index;
            if (b_index >= b_size) {
                score = score_row2;
                score_row2 = score_row1;
                score_row1 = score_row3;
                score_row3 = score;
                break;
            }

            /* FRAME 2 */

            score = MAX(score_other_frame1, score_other_frame2) - shift_penalty;
            score = MAX(score, score_col3);
            if (score == score_col3) {
                script = SCRIPT_NEXT_IN_FRAME;
            }
            else if (score + shift_penalty == score_other_frame1) {
                if (score_other_frame1 == score_col1)
                    script = SCRIPT_AHEAD_TWO_FRAMES;
                else
                    script = SCRIPT_NEXT_PLUS_TWO_FRAMES;
            }
            else {
                if (score_other_frame2 == score_col2)
                    script = SCRIPT_AHEAD_ONE_FRAME;
                else
                    script = SCRIPT_NEXT_PLUS_ONE_FRAME;
            }
            score += matrix_row[ B[ b_index * increment ] ];
            score_other_frame1 = score_other_frame2;
            score_other_frame2 = MAX(score_col3, score_array[b_index].best);
            score_col3 = score_array[b_index].best;
            score_gap_col = score_array[b_index].best_gap;

            if (score < MAX(score_gap_col, score_row3)) {
                score = MAX(score_gap_col, score_row3);
                if (best_score - score > x_dropoff) {
                    if (first_b_index == b_index) 
                        first_b_index = b_index + 1;
                    else
                        score_array[b_index].best = MININT;
                }
                else {
                    if (score == score_gap_col)
                        script = SCRIPT_OOF_OPEN_GAP | SCRIPT_GAP_IN_A;
                    else 
                        script = SCRIPT_OOF_OPEN_GAP | SCRIPT_GAP_IN_B;

                    last_b_index = b_index + 1;
                    score_array[b_index].best = score;
                    score_array[b_index].best_gap = score_gap_col - gap_extend;
                    score_row3 -= gap_extend;
                }
            }
            else {
                if (best_score - score > x_dropoff) {
                    if (first_b_index == b_index) 
                        first_b_index = b_index + 1;
                    else
                        score_array[b_index].best = MININT;
                }
                else {
                    last_b_index = b_index + 1;
                    score_array[b_index].best = score;
                    if (score > best_score) {
                        best_score = score;
                        *a_offset = a_index;
                        *b_offset = b_index;
                    }
                    score -= gap_open_extend;
                    score_row3 -= gap_extend;
                    if (score > score_row3)
                        score_row3 = score;
                    else
                        script |= SCRIPT_EXTEND_GAP_IN_B;

                    score_gap_col -= gap_extend;
                    if (score < score_gap_col) {
                        score_array[b_index].best_gap = score_gap_col;
                        script |= SCRIPT_EXTEND_GAP_IN_A;
                    }
                    else {
                        score_array[b_index].best_gap = score;
                    }
                }
            }
            edit_script_row[b_index] = script;
            b_index++;
        }
  
        /* Finish aligning if the best scores for all positions
           of B will fail the X-dropoff test, i.e. the inner loop 
           bounds have converged to each other */

        if (first_b_index == b_size)
            break;

        if (last_b_index < b_size) {
            /* This row failed the X-dropoff test earlier than
               the last row did; just shorten the loop bounds
               before doing the next row */

            b_size = last_b_index;
        }
        else {
            /* The inner loop finished without failing the X-dropoff
               test; initialize extra bookkeeping structures until
               the X dropoff test fails or we run out of letters in B. 
               The next inner loop will have larger bounds. 
             
               Keep initializing extra structures until the X dropoff
               test fails in all frames for this row */

            score = MAX(score_row1, MAX(score_row2, score_row3));
            last_b_index = b_size + (score - (best_score - x_dropoff)) /
                                                    gap_extend;
            last_b_index = MIN(last_b_index, N + 2);

            while (b_size < last_b_index) {
                score_array[b_size].best = score_row1;
                score_array[b_size].best_gap = score_row1 - gap_open_extend;
                score_row1 -= gap_extend;
                edit_script_row[b_size] = SCRIPT_OOF_OPEN_GAP | SCRIPT_GAP_IN_B;
                if (++b_size > last_b_index)
                    break;

                score_array[b_size].best = score_row2;
                score_array[b_size].best_gap = score_row2 - gap_open_extend;
                score_row2 -= gap_extend;
                edit_script_row[b_size] = SCRIPT_OOF_OPEN_GAP | SCRIPT_GAP_IN_B;
                if (++b_size > last_b_index)
                    break;

                score_array[b_size].best = score_row3;
                score_array[b_size].best_gap = score_row3 - gap_open_extend;
                score_row3 -= gap_extend;
                edit_script_row[b_size] = SCRIPT_OOF_OPEN_GAP | SCRIPT_GAP_IN_B;
                if (++b_size > last_b_index)
                    break;
            }
        }

        /* chop off the best score in each frame */
        last_b_index = MIN(b_size + 4, N + 3);
        while (b_size < last_b_index) {
            score_array[b_size].best = MININT;
            score_array[b_size].best_gap = MININT;
            b_size++;
        }

        state_struct->used += MAX(b_index, b_size) - orig_b_index + 1;
    }

    a_index = *a_offset;
    b_index = *b_offset;
    script = 1;
    align_len = 0;
    edit_script_row = (Uint1 *)malloc(a_index + b_index);

    while (a_index > 0 || b_index > 0) {
        /* Retrieve the next action to perform. Rows of
           the traceback array do not necessarily start
           at offset zero of B, so a correction is needed
           to point to the correct position */

        next_script = 
            edit_script[a_index][b_index - edit_start_offset[a_index]];

        switch (script) {
        case SCRIPT_GAP_IN_A:
            script = next_script & SCRIPT_OOF_OP_MASK;
            if (next_script & (SCRIPT_OOF_OPEN_GAP | SCRIPT_EXTEND_GAP_IN_A))
                script = SCRIPT_GAP_IN_A;
            break;
        case SCRIPT_GAP_IN_B:
            script = next_script & SCRIPT_OOF_OP_MASK;
            if (next_script & (SCRIPT_OOF_OPEN_GAP | SCRIPT_EXTEND_GAP_IN_B))
                script = SCRIPT_GAP_IN_B;
            break;
        default:
            script = next_script & SCRIPT_OOF_OP_MASK;
            break;
        }

        if (script == SCRIPT_GAP_IN_B) {
            b_index -= 3;
        }
        else {
            b_index -= script;
            a_index--;
        }

        edit_script_row[align_len++] = script;
    }

    /* Traceback proceeded backwards through edit_script, 
       so the output traceback information is written 
       in reverse order */

    for (align_len--; align_len >= 0; align_len--) 
        *data.sapp++ = edit_script_row[align_len];
      
    sfree(edit_start_offset);
    sfree(edit_script_row);
    sfree(edit_script);
    sfree(score_array);
    *sapp = data.sapp;

    if (!reversed)
        *b_offset -= 2;
    return best_score;
}

/** Low level function to perform gapped extension with out-of-frame
 * gapping with or without traceback.
 * @param A The query sequence [in]
 * @param B The subject sequence [in]
 * @param M Maximal extension length in query [in]
 * @param N Maximal extension length in subject [in]
 * @param S The traceback information from previous extension [in]
 * @param a_offset Resulting starting offset in query [out]
 * @param b_offset Resulting starting offset in subject [out]
 * @param score_only Only find the score, without saving traceback [in]
 * @param sapp the traceback information [out]
 * @param gap_align Structure holding various information and allocated 
 *        memory for the gapped alignment [in]
 * @param score_params Parameters related to scoring [in]
 * @param query_offset The starting offset in query [in]
 * @param reversed Has the sequence been reversed? Used for psi-blast [in]
 * @return The best alignment score found.
 */
static Int4 OOF_SEMI_G_ALIGN(Uint1* A, Uint1* B, Int4 M, Int4 N,
   Int4* S, Int4* a_offset, Int4* b_offset, Boolean score_only, Int4** sapp, 
   BlastGapAlignStruct* gap_align, const BlastScoringParameters* score_params,
   Int4 query_offset, Boolean reversed)
{
    BlastGapDP* score_array;            /* sequence pointers and indices */
    Int4 i, increment; 
    Int4 a_index;
    Int4 b_index, b_size, first_b_index, last_b_index;
  
    Int4 gap_open;              /* alignment penalty variables */
    Int4 gap_extend;
    Int4 gap_open_extend;
    Int4 shift_penalty;
    Int4 x_dropoff;
  
    Int4* *matrix;              /* pointers to the score matrix */
    Int4* matrix_row;
  
    Int4 score;                 /* score tracking variables */
    Int4 score_row1; 
    Int4 score_row2; 
    Int4 score_row3;
    Int4 score_gap_col; 
    Int4 score_col1; 
    Int4 score_col2; 
    Int4 score_col3;
    Int4 score_other_frame1; 
    Int4 score_other_frame2; 
    Int4 best_score;
  
    if (!score_only) {
        return OOF_ALIGN(A, B, M, N, S, a_offset, b_offset, sapp, gap_align, 
                      score_params, query_offset, reversed);
    }
    
    /* do initialization and sanity-checking */

    matrix = gap_align->sbp->matrix;
    *a_offset = 0;
    *b_offset = -2;
    gap_open = score_params->gap_open;
    gap_extend = score_params->gap_extend;
    gap_open_extend = gap_open + gap_extend;
    shift_penalty = score_params->shift_pen;
    x_dropoff = gap_align->gap_x_dropoff;
  
    if (x_dropoff < gap_open_extend)
        x_dropoff = gap_open_extend;
  
    if(N <= 0 || M <= 0) 
        return 0;
  
    /* Allocate and fill in the auxiliary 
       bookeeping structures (one struct
       per letter of B) */

    score_array = (BlastGapDP*)calloc((N + 5), sizeof(BlastGapDP));
    score = -gap_open_extend;
    score_array[0].best = 0;
    score_array[0].best_gap = -gap_open_extend;
  
    for (i = 3; i <= N + 2; i += 3) {
        score_array[i].best = score;
        score_array[i].best_gap = score - gap_open_extend; 
        score_array[i-1].best = MININT;
        score_array[i-1].best_gap = MININT;
        score_array[i-2].best = MININT;
        score_array[i-2].best_gap = MININT;
        if (score < -x_dropoff) 
            break;
        score -= gap_extend;
    }
  
    /* The inner loop below examines letters of B from 
       index 'first_b_index' to 'b_size' */

    b_size = i - 2;
    score_array[b_size].best = MININT;
    score_array[b_size].best_gap = MININT;

    best_score = 0;
    first_b_index = 0;
    if (reversed) {
        increment = -1;
    }
    else {
        /* Allow for a backwards frame shift */
        B -= 2;
        increment = 1;
    }
  
    for (a_index = 1; a_index <= M; a_index++) {

        /* XXX Why is this here? */
        score_array[2].best = MININT;
        score_array[2].best_gap = MININT;

        /* pick out the row of the score matrix 
           appropriate for A[a_index] */

        if (!(gap_align->positionBased)) {
            matrix_row = matrix[ A[ a_index * increment ] ];
        }
        else {
            if(reversed)
                matrix_row = gap_align->sbp->posMatrix[M - a_index];
            else 
                matrix_row = gap_align->sbp->posMatrix[a_index + query_offset];
        }

        /* initialize running-score variables */
        score = MININT;
        score_row1 = MININT; 
        score_row2 = MININT; 
        score_row3 = MININT;
        score_gap_col = MININT; 
        score_col1 = MININT; 
        score_col2 = MININT; 
        score_col3 = MININT;
        score_other_frame1 = MININT; 
        score_other_frame2 = MININT; 
        last_b_index = first_b_index;
        b_index = first_b_index;

        while (b_index < b_size) {

            /* FRAME 0 */

            /* Pick the best score among all frames */
            score = MAX(score_other_frame1, score_other_frame2) - shift_penalty;
            score = MAX(score, score_col1) + 
                                matrix_row[ B[ b_index * increment ] ];
            score_other_frame1 = MAX(score_col1, score_array[b_index].best);
            score_col1 = score_array[b_index].best;
            score_gap_col = score_array[b_index].best_gap;

            /* Use the row and column scores if they improve
               the score overall */

            if (score < MAX(score_gap_col, score_row1)) {
                score = MAX(score_gap_col, score_row1);
                if (best_score - score > x_dropoff) {

                   /* the current best score failed the X-dropoff
                      criterion. Note that this does not stop the
                      inner loop, only forces future iterations to
                      skip this column of B. 
   
                      Also, if the very first letter of B that was
                      tested failed the X dropoff criterion, make
                      sure future inner loops start one letter to 
                      the right */

                    if (first_b_index == b_index) 
                        first_b_index = b_index + 1;
                    else
                        score_array[b_index].best = MININT;
                }
                else {
                    /* update the row and column running scores */
                    last_b_index = b_index + 1;
                    score_array[b_index].best = score;
                    score_array[b_index].best_gap = score_gap_col - gap_extend;
                    score_row1 -= gap_extend;
                }
            }
            else {
                if (best_score - score > x_dropoff) {

                   /* the current best score failed the X-dropoff
                      criterion. */

                    if (first_b_index == b_index) 
                        first_b_index = b_index + 1;
                    else
                        score_array[b_index].best = MININT;
                }
                else {
                    /* The current best score exceeds the
                       row and column scores, and thus may
                       improve on the current optimal score */

                    last_b_index = b_index + 1;
                    score_array[b_index].best = score;
                    if (score > best_score) {
                        best_score = score;
                        *a_offset = a_index;
                        *b_offset = b_index;
                    }

                    /* compute the best scores that include gaps
                       or gap extensions */

                    score -= gap_open_extend;
                    score_row1 -= gap_extend;
                    score_row1 = MAX(score, score_row1);
                    score_array[b_index].best_gap = MAX(score, 
                                                  score_gap_col - gap_extend);
                }
            }

            /* If this was the last letter of B checked, rotate
               the row scores so that code beyond the inner loop
               works correctly */

            if (++b_index >= b_size) {
                score = score_row1;
                score_row1 = score_row2;
                score_row2 = score_row3;
                score_row3 = score;
                break;
            }

            /* FRAME 1 */

            /* This code, and that for Frame 2, are essentially the
               same as the preceeding code. The only real difference
               is the updating of the other_frame best scores */

            score = MAX(score_other_frame1, score_other_frame2) - shift_penalty;
            score = MAX(score, score_col2) + 
                                matrix_row[ B[ b_index * increment ] ];
            score_other_frame2 = MAX(score_col2, score_array[b_index].best);
            score_col2 = score_array[b_index].best;
            score_gap_col = score_array[b_index].best_gap;

            if (score < MAX(score_gap_col, score_row2)) {
                score = MAX(score_gap_col, score_row2);
                if (best_score - score > x_dropoff) {
                    if (first_b_index == b_index) 
                        first_b_index = b_index + 1;
                    else
                        score_array[b_index].best = MININT;
                }
                else {
                    last_b_index = b_index + 1;
                    score_array[b_index].best = score;
                    score_array[b_index].best_gap = score_gap_col - gap_extend;
                    score_row2 -= gap_extend;
                }
            }
            else {
                if (best_score - score > x_dropoff) {
                    if (first_b_index == b_index) 
                        first_b_index = b_index + 1;
                    else
                        score_array[b_index].best = MININT;
                }
                else {
                    last_b_index = b_index + 1;
                    score_array[b_index].best = score;
                    if (score > best_score) {
                        best_score = score;
                        *a_offset = a_index;
                        *b_offset = b_index;
                    }
                    score -= gap_open_extend;
                    score_row2 -= gap_extend;
                    score_row2 = MAX(score, score_row2);
                    score_array[b_index].best_gap = MAX(score, 
                                                  score_gap_col - gap_extend);
                }
            }

            if (++b_index >= b_size) {
                score = score_row2;
                score_row2 = score_row1;
                score_row1 = score_row3;
                score_row3 = score;
                break;
            }

            /* FRAME 2 */

            score = MAX(score_other_frame1, score_other_frame2) - shift_penalty;
            score = MAX(score, score_col3) + 
                                matrix_row[ B[ b_index * increment ] ];
            score_other_frame1 = score_other_frame2;
            score_other_frame2 = MAX(score_col3, score_array[b_index].best);
            score_col3 = score_array[b_index].best;
            score_gap_col = score_array[b_index].best_gap;

            if (score < MAX(score_gap_col, score_row3)) {
                score = MAX(score_gap_col, score_row3);
                if (best_score - score > x_dropoff) {
                    if (first_b_index == b_index) 
                        first_b_index = b_index + 1;
                    else
                        score_array[b_index].best = MININT;
                }
                else {
                    last_b_index = b_index + 1;
                    score_array[b_index].best = score;
                    score_array[b_index].best_gap = score_gap_col - gap_extend;
                    score_row3 -= gap_extend;
                }
            }
            else {
                if (best_score - score > x_dropoff) {
                    if (first_b_index == b_index) 
                        first_b_index = b_index + 1;
                    else
                        score_array[b_index].best = MININT;
                }
                else {
                    last_b_index = b_index + 1;
                    score_array[b_index].best = score;
                    if (score > best_score) {
                        best_score = score;
                        *a_offset = a_index;
                        *b_offset = b_index;
                    }
                    score -= gap_open_extend;
                    score_row3 -= gap_extend;
                    score_row3 = MAX(score, score_row3);
                    score_array[b_index].best_gap = MAX(score, 
                                                  score_gap_col - gap_extend);
                }
            }
            b_index++;
        }
  
        /* Finish aligning if the best scores for all positions
           of B will fail the X-dropoff test, i.e. the inner loop 
           bounds have converged to each other */

        if (first_b_index == b_size)
            break;

        if (last_b_index < b_size) {
            /* This row failed the X-dropoff test earlier than
               the last row did; just shorten the loop bounds
               before doing the next row */

            b_size = last_b_index;
        }
        else {
            /* The inner loop finished without failing the X-dropoff
               test; initialize extra bookkeeping structures until
               the X dropoff test fails or we run out of letters in B. 
               The next inner loop will have larger bounds. 
             
               Keep initializing extra structures until the X dropoff
               test fails in all frames for this row */

            score = MAX(score_row1, MAX(score_row2, score_row3));
            last_b_index = b_size + (score - (best_score - x_dropoff)) /
                                                    gap_extend;
            last_b_index = MIN(last_b_index, N+2);

            while (b_size < last_b_index) {
                score_array[b_size].best = score_row1;
                score_array[b_size].best_gap = score_row1 - gap_open_extend;
                score_row1 -= gap_extend;
                if (++b_size > last_b_index)
                    break;

                score_array[b_size].best = score_row2;
                score_array[b_size].best_gap = score_row2 - gap_open_extend;
                score_row2 -= gap_extend;
                if (++b_size > last_b_index)
                    break;

                score_array[b_size].best = score_row3;
                score_array[b_size].best_gap = score_row3 - gap_open_extend;
                score_row3 -= gap_extend;
                if (++b_size > last_b_index)
                    break;
            }
        }

        /* chop off the best score in each frame */
        last_b_index = MIN(b_size + 4, N + 3);
        while (b_size < last_b_index) {
            score_array[b_size].best = MININT;
            score_array[b_size].best_gap = MININT;
            b_size++;
        }
    }

    if (!reversed) {
        /* The sequence was shifted, so length should be adjusted as well */
        *b_offset -= 2;
    }
    sfree(score_array);
    return best_score;
}

/** Callback function for a sorting of initial HSPs by diagonal */
static int
diag_compare_match(const void* v1, const void* v2)
{
   BlastInitHSP* h1,* h2;

   h1 = *((BlastInitHSP**) v1);
   h2 = *((BlastInitHSP**) v2);
   
   if (!h1 || !h2)
      return 0;

   return (h1->q_off - h1->s_off) - (h2->q_off - h2->s_off);
}

/** Find the HSP offsets relative to the individual query sequence instead of
 * the concatenated sequence.
 * @param query Query sequence block [in]
 * @param query_info Query information structure, including context offsets 
 *                   array [in]
 * @param init_hsp Initial HSP [in] [out]
 * @param query_out Optional: query sequence block with modified sequence 
 *                  pointer and sequence length [out]
 * @param init_hsp_out Optional: Pointer to initial HSP structure to hold
 *                     adjusted offsets; the input init_hsp will be modified if
 *                     this parameter is NULL [out]
 * @param context_out Which context this HSP belongs to? [out]
 */
static void 
GetRelativeCoordinates(const BLAST_SequenceBlk* query, 
                       BlastQueryInfo* query_info, 
                       BlastInitHSP* init_hsp, BLAST_SequenceBlk* query_out,
                       BlastInitHSP* init_hsp_out, Int4* context_out)
{
   Int4 context;
   Int4 query_start, query_length;

   context = BSearchInt4(init_hsp->q_off, query_info->context_offsets, 
                         query_info->last_context+2);
   if (query && query->oof_sequence) {
      /* Out-of-frame blastx case: all frames of the same parity are mixed
         together in a special sequence. */
      Int4 mixed_frame_context = context - context%CODON_LENGTH;
      query_start = query_info->context_offsets[mixed_frame_context];
      query_length = 
         query_info->context_offsets[mixed_frame_context+CODON_LENGTH] - 
         query_start - 1;
   } else {
      query_start = query_info->context_offsets[context];
      query_length = BLAST_GetQueryLength(query_info, context);
   }

   if (query && query_out) {
      if (query->oof_sequence) {
         query_out->sequence = NULL;
         query_out->oof_sequence = query->oof_sequence + query_start;
      } else {
         query_out->sequence = query->sequence + query_start;
         query_out->oof_sequence = NULL;
      }
      query_out->length = query_length;
   }

   if (init_hsp_out) {
      init_hsp_out->q_off = init_hsp->q_off - query_start;
      init_hsp_out->s_off = init_hsp->s_off;
      if (init_hsp->ungapped_data) {
         if (!init_hsp_out->ungapped_data) {
            init_hsp_out->ungapped_data = (BlastUngappedData*) 
               BlastMemDup(init_hsp->ungapped_data, sizeof(BlastUngappedData));
         } else {
            memcpy(init_hsp_out->ungapped_data, init_hsp->ungapped_data,
                   sizeof(BlastUngappedData));
         }
         init_hsp_out->ungapped_data->q_start = 
            init_hsp->ungapped_data->q_start - query_start;
      }
   } else {
      init_hsp->q_off -= query_start;
      if (init_hsp->ungapped_data)
         init_hsp->ungapped_data->q_start -= query_start;
   }

   *context_out = context;
}

Int2 BLAST_MbGetGappedScore(EBlastProgramType program_number, 
             BLAST_SequenceBlk* query, BlastQueryInfo* query_info, 
			    BLAST_SequenceBlk* subject,
			    BlastGapAlignStruct* gap_align,
			    const BlastScoringParameters* score_params, 
			    const BlastExtensionParameters* ext_params,
			    const BlastHitSavingParameters* hit_params,
			    BlastInitHitList* init_hitlist,
			    BlastHSPList** hsp_list_ptr, BlastGappedStats* gapped_stats)
{
   const BlastExtensionOptions* ext_options = ext_params->options;
   Int4 index, i;
   Boolean delete_hsp;
   BlastInitHSP* init_hsp;
   BlastInitHSP** init_hsp_array;
   BlastHSPList* hsp_list;
   BlastHSP* hsp;
   const BlastHitSavingOptions* hit_options = hit_params->options;
   BLAST_SequenceBlk query_tmp;
   Int4 context;

   if (*hsp_list_ptr == NULL)
      *hsp_list_ptr = hsp_list = Blast_HSPListNew(hit_options->hsp_num_max);
   else 
      hsp_list = *hsp_list_ptr;

   init_hsp_array = (BlastInitHSP**) 
     malloc(init_hitlist->total*sizeof(BlastInitHSP*));
   for (index=0; index<init_hitlist->total; ++index)
     init_hsp_array[index] = &init_hitlist->init_hsp_array[index];

   qsort(init_hsp_array, init_hitlist->total, 
            sizeof(BlastInitHSP*), diag_compare_match);

   for (index=0; index<init_hitlist->total; index++) {
      init_hsp = init_hsp_array[index];
      /* Change query coordinates to relative in the initial HSP right here */
      GetRelativeCoordinates(query, query_info, init_hsp, &query_tmp, NULL, 
                             &context);
      delete_hsp = FALSE;
      for (i = hsp_list->hspcnt - 1; i >= 0; i--) {
         hsp = hsp_list->hsp_array[i];
         if (context != hsp->context)
            continue;
         if (!MB_HSP_CLOSE(init_hsp->q_off, hsp->query.offset, 
                           init_hsp->s_off, hsp->subject.offset, MB_DIAG_NEAR))
             break;

         /* Do not extend an HSP already contained in another HSP, unless
            its ungapped score is higher than that HSP's gapped score,
            which indicates wrong starting offset for previously extended HSP.
         */
         if (MB_HSP_CONTAINED(init_hsp->q_off, hsp->query.offset, 
                hsp->query.end, init_hsp->s_off, 
                hsp->subject.offset, hsp->subject.end, MB_DIAG_CLOSE) &&
             (!init_hsp->ungapped_data || 
              init_hsp->ungapped_data->score < hsp->score)) 
         {
               delete_hsp = TRUE;
               break;
         }
      }
      if (!delete_hsp) {
         Boolean good_hit = TRUE;
         Int4 hsp_length;

         if (gapped_stats)
            ++gapped_stats->extensions;

         BLAST_GreedyGappedAlignment(query_tmp.sequence, 
            subject->sequence, query_tmp.length, subject->length, gap_align, 
            score_params, init_hsp->q_off, init_hsp->s_off, (Boolean) TRUE, 
            (Boolean) (ext_options->ePrelimGapExt == eGreedyWithTracebackExt));

         /* Take advantage of an opportunity to easily check whether this 
            hit passes the percent identity and minimal length criteria. */
         hsp_length = 
            MIN(gap_align->query_stop-gap_align->query_start, 
                gap_align->subject_stop-gap_align->subject_start) + 1;
         if (hsp_length < hit_options->min_hit_length || 
             gap_align->percent_identity < hit_options->percent_identity)
            good_hit = FALSE;
               
         if (good_hit && gap_align->score >= hit_options->cutoff_score) {
            /* gap_align contains alignment endpoints; init_hsp contains 
               the offsets to start the alignment from, if traceback is to 
               be performed later */
            BlastHSP* new_hsp;
            Blast_HSPInit(gap_align->query_start, gap_align->query_stop,
                           gap_align->subject_start, gap_align->subject_stop, 
                           init_hsp->q_off, init_hsp->s_off, 
                           context, 1, gap_align->score,
                           &(gap_align->edit_block), &new_hsp);
            Blast_HSPListSaveHSP(hsp_list, new_hsp);
         }
      }
   }

   sfree(init_hsp_array);

   return 0;
}

/** Auxiliary function to transform one style of edit script into 
 *  another. 
 */
static GapEditScript*
MBToGapEditScript (MBGapEditScript* ed_script)
{
    /* Moved from greedy_align.h because it's only needed in this function */
#define EDIT_VAL(op) (op >> 2)
#define EDIT_OPC(op) (op & 0x3) /* EDIT_OP_MASK == 0x3 */

   GapEditScript* esp_start = NULL,* esp,* esp_prev = NULL;
   Uint4 i;

   if (ed_script==NULL || ed_script->num == 0)
      return NULL;

   for (i=0; i<ed_script->num; i++) {
      esp = (GapEditScript*) calloc(1, sizeof(GapEditScript));
      esp->num = EDIT_VAL(ed_script->op[i]);
      switch (EDIT_OPC(ed_script->op[i])) {
      case 1: 
         esp->op_type = eGapAlignDel; break;
      case 2:
         esp->op_type = eGapAlignIns; break;
      case 3:
         esp->op_type = eGapAlignSub; break;
      default:
         fprintf(stderr, "op_type = 3\n"); break;
      }
      if (i==0)
         esp_start = esp_prev = esp;
      else {
         esp_prev->next = esp;
         esp_prev = esp;
      }
   }

   return esp_start;

}

/** Fills the BlastGapAlignStruct structure with the results of a gapped 
 * extension.
 * @param gap_align the initialized gapped alignment structure [in] [out]
 * @param q_start The starting offset in query [in]
 * @param s_start The starting offset in subject [in]
 * @param q_end The ending offset in query [in]
 * @param s_end The ending offset in subject [in]
 * @param query_length Length of the query sequence [in]
 * @param subject_length Length of the subject sequence [in]
 * @param score The alignment score [in]
 * @param esp The edit script containing the traceback information [in]
 */
static Int2
BLAST_GapAlignStructFill(BlastGapAlignStruct* gap_align, Int4 q_start, 
   Int4 s_start, Int4 q_end, Int4 s_end, Uint4 query_length, 
   Uint4 subject_length, Int4 score, GapEditScript* esp)
{
   gap_align->query_start = q_start;
   gap_align->query_stop = q_end;
   gap_align->subject_start = s_start;
   gap_align->subject_stop = s_end;
   gap_align->score = score;

   if (esp) {
      gap_align->edit_block = GapEditBlockNew(q_start, s_start);
      gap_align->edit_block->start1 = q_start;
      gap_align->edit_block->start2 = s_start;
      gap_align->edit_block->length1 = query_length;
      gap_align->edit_block->length2 = subject_length;
      gap_align->edit_block->original_length1 = query_length;
      gap_align->edit_block->original_length2 = subject_length;
      gap_align->edit_block->frame1 = gap_align->edit_block->frame2 = 1;
      gap_align->edit_block->reverse = 0;
      gap_align->edit_block->esp = esp;
   }
   return 0;
}

Int2 
BLAST_GreedyGappedAlignment(Uint1* query, Uint1* subject, 
   Int4 query_length, Int4 subject_length, BlastGapAlignStruct* gap_align,
   const BlastScoringParameters* score_params, 
   Int4 q_off, Int4 s_off, Boolean compressed_subject, Boolean do_traceback)
{
   Uint1* q;
   Uint1* s;
   Int4 score;
   Int4 X;
   Int4 q_avail, s_avail;
   /* Variables for the call to MegaBlastGreedyAlign */
   Int4 q_ext_l, q_ext_r, s_ext_l, s_ext_r;
   MBGapEditScript *ed_script_fwd=NULL, *ed_script_rev=NULL;
   Uint1 rem;
   GapEditScript* esp = NULL;
   
   q_avail = query_length - q_off;
   s_avail = subject_length - s_off;
   
   q = query + q_off;
   if (!compressed_subject) {
      s = subject + s_off;
      rem = 4; /* Special value to indicate that sequence is already
                  uncompressed */
   } else {
      s = subject + s_off/4;
      rem = s_off % 4;
   }

   /* Find where positive scoring starts & ends within the word hit */
   score = 0;
   
   X = gap_align->gap_x_dropoff;
   
   if (do_traceback) {
      ed_script_fwd = MBGapEditScriptNew();
      ed_script_rev = MBGapEditScriptNew();
   }
   
   /* extend to the right */
   score = BLAST_AffineGreedyAlign(s, s_avail, q, q_avail, FALSE, X,
              score_params->reward, -score_params->penalty, 
              score_params->gap_open, score_params->gap_extend,
              &s_ext_r, &q_ext_r, gap_align->greedy_align_mem, 
              ed_script_fwd, rem);

   if (compressed_subject)
      rem = 0;

   /* extend to the left */
   score += BLAST_AffineGreedyAlign(subject, s_off, 
               query, q_off, TRUE, X, 
               score_params->reward, -score_params->penalty, 
               score_params->gap_open, score_params->gap_extend, 
               &s_ext_l, &q_ext_l, gap_align->greedy_align_mem, 
               ed_script_rev, rem);

   /* In basic case the greedy algorithm returns number of 
      differences, hence we need to convert it to score */
   if (score_params->gap_open==0 && score_params->gap_extend==0) {
      /* Take advantage of an opportunity to easily calculate percent 
         identity, to avoid parsing the traceback later */
      gap_align->percent_identity = 
         100*(1 - ((double)score) / MIN(q_ext_l+q_ext_r, s_ext_l+s_ext_r));
      score = 
         (q_ext_r + s_ext_r + q_ext_l + s_ext_l)*score_params->reward/2 - 
         score*(score_params->reward - score_params->penalty);
   } else if (score_params->reward % 2 == 1) {
      score /= 2;
   }

   if (do_traceback) {
      MBGapEditScriptAppend(ed_script_rev, ed_script_fwd);
      esp = MBToGapEditScript(ed_script_rev);
   }
   
   MBGapEditScriptFree(ed_script_fwd);
   MBGapEditScriptFree(ed_script_rev);
   
   BLAST_GapAlignStructFill(gap_align, q_off-q_ext_l, 
      s_off-s_ext_l, q_off+q_ext_r, 
      s_off+s_ext_r, query_length, subject_length, score, esp);
   return 0;
}

/** Performs dynamic programming style gapped extension, given an initial 
 * HSP (offset pair), two sequence blocks and scoring and extension options.
 * Note: traceback is not done in this function.
 * @param query_blk The query sequence [in]
 * @param subject_blk The subject sequence [in]
 * @param gap_align The auxiliary structure for gapped alignment [in]
 * @param score_params Parameters related to scoring [in]
 * @param init_hsp The initial HSP that needs to be extended [in]
*/
static Int2 BLAST_DynProgNtGappedAlignment(BLAST_SequenceBlk* query_blk, 
   BLAST_SequenceBlk* subject_blk, BlastGapAlignStruct* gap_align, 
   const BlastScoringParameters* score_params, BlastInitHSP* init_hsp)
{
   Boolean found_start, found_end;
   Int4 q_length=0, s_length=0, score_right, score_left, 
      private_q_start, private_s_start;
   Uint1 offset_adjustment;
   Uint1* query,* subject;
   
   found_start = FALSE;
   found_end = FALSE;

   query = query_blk->sequence;
   subject = subject_blk->sequence;
   score_left = 0;
   /* If subject offset is not at the start of a full byte, 
      BLAST_AlignPackedNucl won't work, so shift the alignment start
      to the left */
   offset_adjustment = 
      COMPRESSION_RATIO - (init_hsp->s_off % COMPRESSION_RATIO);
   q_length = init_hsp->q_off + offset_adjustment;
   s_length = init_hsp->s_off + offset_adjustment;
   if (q_length != 0 && s_length != 0) {
      found_start = TRUE;
      score_left = BLAST_AlignPackedNucl(query, subject, q_length, s_length, 
                      &private_q_start, &private_s_start, gap_align, 
                      score_params, TRUE);
      if (score_left < 0) 
         return -1;
      gap_align->query_start = q_length - private_q_start;
      gap_align->subject_start = s_length - private_s_start;
   }

   score_right = 0;
   if (q_length < query_blk->length && 
       s_length < subject_blk->length)
   {
      found_end = TRUE;
      score_right = BLAST_AlignPackedNucl(query+q_length-1, 
         subject+(s_length+3)/COMPRESSION_RATIO - 1, 
         query_blk->length-q_length, 
         subject_blk->length-s_length, &(gap_align->query_stop),
         &(gap_align->subject_stop), gap_align, score_params, FALSE);
      if (score_right < 0) 
         return -1;
      gap_align->query_stop += q_length;
      gap_align->subject_stop += s_length;
   }

   if (found_start == FALSE) {
      /* Start never found */
      gap_align->query_start = init_hsp->q_off;
      gap_align->subject_start = init_hsp->s_off;
   }

   if (found_end == FALSE) {
      gap_align->query_stop = init_hsp->q_off - 1;
      gap_align->subject_stop = init_hsp->s_off - 1;
   }

   gap_align->score = score_right+score_left;

   return 0;
}

/* Aligns two nucleotide sequences, one (A) should be packed in the
 * same way as the BLAST databases, the other (B) should contain one
 * basepair/byte. Traceback is not done in this function.
 * @param B The query sequence [in]
 * @param A The subject sequence [in]
 * @param N Maximal extension length in query [in]
 * @param M Maximal extension length in subject [in]
 * @param b_offset Resulting starting offset in query [out]
 * @param a_offset Resulting starting offset in subject [out]
 * @param gap_align The auxiliary structure for gapped alignment [in]
 * @param score_params Parameters related to scoring [in]
 * @param reverse_sequence Reverse the sequence.
 * @return The best alignment score found.
*/
static Int4 BLAST_AlignPackedNucl(Uint1* B, Uint1* A, Int4 N, Int4 M, 
	Int4* b_offset, Int4* a_offset, 
        BlastGapAlignStruct* gap_align,
        const BlastScoringParameters* score_params, 
        Boolean reverse_sequence)
{ 
    BlastGapDP* score_array;            /* sequence pointers and indices */
    Int4 i; 
    Int4 a_index, a_base_pair;
    Int4 b_index, b_size, first_b_index, last_b_index, b_increment;
    Uint1* b_ptr;
  
    Int4 gap_open;              /* alignment penalty variables */
    Int4 gap_extend;
    Int4 gap_open_extend;
    Int4 decline_penalty;
    Int4 x_dropoff;
  
    Int4* *matrix;              /* pointers to the score matrix */
    Int4* matrix_row;
  
    Int4 score;                 /* score tracking variables */
    Int4 score_gap_row;
    Int4 score_gap_col;
    Int4 score_decline;
    Int4 next_score;
    Int4 next_score_decline;
    Int4 best_score;
  
    /* do initialization and sanity-checking */

    matrix = gap_align->sbp->matrix;
    *a_offset = 0;
    *b_offset = 0;
    gap_open = score_params->gap_open;
    gap_extend = score_params->gap_extend;
    gap_open_extend = gap_open + gap_extend;
    decline_penalty = score_params->decline_align;
    x_dropoff = gap_align->gap_x_dropoff;
  
    if (x_dropoff < gap_open_extend)
        x_dropoff = gap_open_extend;
  
    if(N <= 0 || M <= 0) 
        return 0;
  
    /* Allocate and fill in the auxiliary 
       bookeeping structures (one struct
       per letter of B) */

    score_array = (BlastGapDP*)malloc((N + 2) * sizeof(BlastGapDP));
    score = -gap_open_extend;
    score_array[0].best = 0;
    score_array[0].best_gap = -gap_open_extend;
    score_array[0].best_decline = -gap_open_extend - decline_penalty;
  
    for (i = 1; i <= N; i++) {
        if (score < -x_dropoff) 
            break;

        score_array[i].best = score;
        score_array[i].best_gap = score - gap_open_extend; 
        score_array[i].best_decline = score - gap_open_extend - decline_penalty;
        score -= gap_extend;
    }
  
    /* The inner loop below examines letters of B from 
       index 'first_b_index' to 'b_size' */

    b_size = i;
    best_score = 0;
    first_b_index = 0;
    if (reverse_sequence)
        b_increment = -1;
    else
        b_increment = 1;
  
    for (a_index = 1; a_index <= M; a_index++) {

        /* pick out the row of the score matrix 
           appropriate for A[a_index] */

        if(reverse_sequence) {
            a_base_pair = NCBI2NA_UNPACK_BASE(A[(M-a_index)/4], 
                                               ((a_index-1)%4));
            matrix_row = matrix[a_base_pair];
        } 
        else {
            a_base_pair = NCBI2NA_UNPACK_BASE(A[1+((a_index-1)/4)], 
                                               (3-((a_index-1)%4)));
            matrix_row = matrix[a_base_pair];
        }

        if(reverse_sequence)
            b_ptr = &B[N - first_b_index];
        else
            b_ptr = &B[first_b_index];

        /* initialize running-score variables */
        score = MININT;
        score_gap_row = MININT;
        score_decline = MININT;
        last_b_index = first_b_index;

        for (b_index = first_b_index; b_index < b_size; b_index++) {

            b_ptr += b_increment;
            score_gap_col = score_array[b_index].best_gap;
            next_score = score_array[b_index].best + matrix_row[ *b_ptr ];
            next_score_decline = score_array[b_index].best_decline;

            /* decline the alignment if that improves the score */

            score = MAX(score, score_decline);
            
            /* decline the best row score if that improves it;
               if not, make it the new high score if it's
               an improvement */

            if (score_gap_col < score_decline)
                score_gap_col = score_decline;
            else if (score < score_gap_col)
                score = score_gap_col;

            /* decline the best column score if that improves it;
               if not, make it the new high score if it's
               an improvement */

            if (score_gap_row < score_decline)
                score_gap_row = score_decline;
            else if (score < score_gap_row)
                score = score_gap_row;

            if (best_score - score > x_dropoff) {

                /* the current best score failed the X-dropoff
                   criterion. Note that this does not stop the
                   inner loop, only forces future iterations to
                   skip this column of B. 

                   Also, if the very first letter of B that was
                   tested failed the X dropoff criterion, make
                   sure future inner loops start one letter to 
                   the right */

                if (b_index == first_b_index)
                    first_b_index++;
                else
                    score_array[b_index].best = MININT;
            }
            else {
                last_b_index = b_index;
                if (score > best_score) {
                    best_score = score;
                    *a_offset = a_index;
                    *b_offset = b_index;
                }

                /* If starting a gap at this position will improve
                   the best row, column, or declined alignment score, 
                   update them to reflect that. */

                score_gap_row -= gap_extend;
                score_gap_col -= gap_extend;
                score_array[b_index].best_gap = MAX(score - gap_open_extend,
                                                    score_gap_col);
                score_gap_row = MAX(score - gap_open_extend, score_gap_row);

                score_array[b_index].best_decline = 
                        MAX(score_decline, score - gap_open) - decline_penalty;
                score_array[b_index].best = score;
            }

            score = next_score;
            score_decline = next_score_decline;
        }
  
        /* Finish aligning if the best scores for all positions
           of B will fail the X-dropoff test, i.e. the inner loop 
           bounds have converged to each other */

        if (first_b_index == b_size)
            break;

        if (last_b_index < b_size - 1) {
            /* This row failed the X-dropoff test earlier than
               the last row did; just shorten the loop bounds
               before doing the next row */

            b_size = last_b_index + 1;
        }
        else {
            /* The inner loop finished without failing the X-dropoff
               test; initialize extra bookkeeping structures until
               the X dropoff test fails or we run out of letters in B. 
               The next inner loop will have larger bounds */

            while (score_gap_row >= (best_score - x_dropoff) && b_size <= N) {
                score_array[b_size].best = score_gap_row;
                score_array[b_size].best_gap = score_gap_row - gap_open_extend;
                score_array[b_size].best_decline = score_gap_row - gap_open -
                                                        decline_penalty;
                score_gap_row -= gap_extend;
                b_size++;
            }
        }

        if (b_size <= N) {
            score_array[b_size].best = MININT;
            score_array[b_size].best_gap = MININT;
            score_array[b_size].best_decline = MININT;
            b_size++;
        }
    }
    
    sfree(score_array);
    return best_score;
}

/** Callback for sorting initial HSPs by score. */
static int
score_compare_match(const void* v1, const void* v2)

{
	BlastInitHSP* h1,* h2;

	h1 = *(BlastInitHSP**) v1;
	h2 = *(BlastInitHSP**) v2;

	if (h1 == NULL || h2 == NULL || 
            !h1->ungapped_data || !h2->ungapped_data)
		return 0;

	if (h1->ungapped_data->score < h2->ungapped_data->score) 
		return 1;
	if (h1->ungapped_data->score > h2->ungapped_data->score)
		return -1;

   /* Tie breaks: starting offset in subject; then length
    * (equivalent to ending offset in subject).
    */
   if (h1->ungapped_data->s_start < h2->ungapped_data->s_start)
      return 1;
   if (h1->ungapped_data->s_start > h2->ungapped_data->s_start )
      return -1;

   if (h1->ungapped_data->length < h2->ungapped_data->length)
      return 1;
   if (h1->ungapped_data->length > h2->ungapped_data->length)
      return -1;

	return 0;
}

#define HSP_MAX_WINDOW 11

Int4 
BlastGetStartForGappedAlignment (Uint1* query, Uint1* subject,
   const BlastScoreBlk* sbp, Uint4 q_start, Uint4 q_length, 
   Uint4 s_start, Uint4 s_length)
{
    Int4 index1, max_offset, score, max_score, hsp_end;
    Uint1* query_var,* subject_var;
    Boolean positionBased = (sbp->posMatrix != NULL);
    
    if (q_length <= HSP_MAX_WINDOW) {
        max_offset = q_start + q_length/2;
        return max_offset;
    }

    hsp_end = q_start + HSP_MAX_WINDOW;
    query_var = query + q_start;
    subject_var = subject + s_start;
    score=0;
    for (index1=q_start; index1<hsp_end; index1++) {
        if (!(positionBased))
            score += sbp->matrix[*query_var][*subject_var];
        else
            score += sbp->posMatrix[index1][*subject_var];
        query_var++; subject_var++;
    }
    max_score = score;
    max_offset = hsp_end - 1;
    hsp_end = q_start + MIN(q_length, s_length);
    for (index1=q_start + HSP_MAX_WINDOW; index1<hsp_end; index1++) {
        if (!(positionBased)) {
            score -= sbp->matrix[*(query_var-HSP_MAX_WINDOW)][*(subject_var-HSP_MAX_WINDOW)];
            score += sbp->matrix[*query_var][*subject_var];
        } else {
            score -= sbp->posMatrix[index1-HSP_MAX_WINDOW][*(subject_var-HSP_MAX_WINDOW)];
            score += sbp->posMatrix[index1][*subject_var];
        }
        if (score > max_score) {
            max_score = score;
            max_offset = index1;
        }
        query_var++; subject_var++;
    }
    if (max_score > 0)
       max_offset -= HSP_MAX_WINDOW/2;
    else 
       max_offset = q_start;

    return max_offset;
}

static Boolean 
Blast_GappedScorePrelimTest(EBlastProgramType program_number, 
        BLAST_SequenceBlk* query, BlastQueryInfo* query_info, 
        BLAST_SequenceBlk* subject, 
        BlastGapAlignStruct* gap_align,
        const BlastScoringParameters* score_params,
        const BlastExtensionParameters* ext_params,
        const BlastHitSavingParameters* hit_params,
        BlastInitHSP** init_hsp_array, Int4 init_hsp_count,
        BlastGappedStats* gapped_stats)
{
   BlastInitHSP* init_hsp = NULL;
   BlastInitHSP init_hsp_tmp;
   Int4 index;
   BLAST_SequenceBlk query_tmp;
   Int4 context;
   Int4 **orig_pssm;
   Boolean further_process = FALSE;
   Int4 gap_trigger;
   Int4 cutoff_score;
   Boolean is_prot;
   Int4 max_offset;
   Int2 status = 0;

   gap_trigger = ext_params->gap_trigger;
   cutoff_score = hit_params->cutoff_score;
   is_prot = (program_number != eBlastTypeBlastn);
   orig_pssm = gap_align->sbp->posMatrix;

   qsort(init_hsp_array, init_hsp_count,
         sizeof(BlastInitHSP*), score_compare_match);

   /* If no initial HSP passes the e-value threshold so far, check if any 
      would do after gapped alignment, and exit if none are found. 
      Only attempt to extend initial HSPs whose scores are already above 
      gap trigger */
   if (init_hsp_array[0]->ungapped_data && 
       init_hsp_array[0]->ungapped_data->score < cutoff_score) {
      init_hsp_tmp.ungapped_data = NULL;
      for (index=0; index<init_hsp_count; index++) {
         init_hsp = init_hsp_array[index];

         if (init_hsp->ungapped_data && 
             init_hsp->ungapped_data->score < gap_trigger)
            break;

         if (gapped_stats) {
            ++gapped_stats->extra_extensions;
            ++gapped_stats->extensions;
         }

         /* Don't modify initial HSP's coordinates here, because it will be 
            done again if further processing is required */
         GetRelativeCoordinates(query, query_info, init_hsp, &query_tmp, 
                                &init_hsp_tmp, &context);
         if (orig_pssm)
            gap_align->sbp->posMatrix = orig_pssm + 
                                query_info->context_offsets[context];

         if(is_prot && !score_params->options->is_ooframe) {
            max_offset = 
               BlastGetStartForGappedAlignment(query_tmp.sequence, 
                  subject->sequence, gap_align->sbp,
                  init_hsp_tmp.ungapped_data->q_start,
                  init_hsp_tmp.ungapped_data->length,
                  init_hsp_tmp.ungapped_data->s_start,
                  init_hsp_tmp.ungapped_data->length);
            init_hsp_tmp.s_off += max_offset - init_hsp_tmp.q_off;
            init_hsp_tmp.q_off = max_offset;
         }

         if (is_prot) {
            status =  
               BLAST_ProtGappedAlignment(program_number, &query_tmp, 
                  subject, gap_align, score_params, &init_hsp_tmp);
         } else {
            status = 
               BLAST_DynProgNtGappedAlignment(&query_tmp, subject, 
                  gap_align, score_params, &init_hsp_tmp);
         }
         if (status) {
            further_process = FALSE;
            break;
         }
         if (gap_align->score >= cutoff_score) {
            further_process = TRUE;
            break;
         }
      }
      sfree(init_hsp_tmp.ungapped_data);
   } else {
      index = 0;
      further_process = TRUE;
      if (gapped_stats)
         ++gapped_stats->seqs_ungapped_passed;
   }

   if (!further_process) {
      /* Free the ungapped data */
      for (index = 0; index < init_hsp_count; ++index) {
            sfree(init_hsp_array[index]->ungapped_data);
      }
      sfree(init_hsp_array);
      gap_align->sbp->posMatrix = orig_pssm;
   } else if (index > 0) { /* Sort again, if necessary */
      qsort(init_hsp_array, init_hsp_count,
               sizeof(BlastInitHSP*), score_compare_match);
   }

   return further_process;
}

Int2 BLAST_GetGappedScore (EBlastProgramType program_number, 
        BLAST_SequenceBlk* query, BlastQueryInfo* query_info, 
        BLAST_SequenceBlk* subject, 
        BlastGapAlignStruct* gap_align,
        const BlastScoringParameters* score_params,
        const BlastExtensionParameters* ext_params,
        const BlastHitSavingParameters* hit_params,
        BlastInitHitList* init_hitlist,
        BlastHSPList** hsp_list_ptr, BlastGappedStats* gapped_stats)

{
   SSeqRange* helper = NULL;
   Int4 index, index1, next_offset;
   BlastInitHSP* init_hsp = NULL;
   BlastHSP* hsp1 = NULL;
   Int4 q_start, s_start, q_end, s_end;
   Boolean is_prot;
   Int4 max_offset;
   Int2 status = 0;
   BlastInitHSP** init_hsp_array = NULL;
   BlastHSPList* hsp_list = NULL;
   const BlastHitSavingOptions* hit_options = hit_params->options;
   BLAST_SequenceBlk query_tmp;
   Int4 context;
   Int4 **orig_pssm;

   if (!query || !subject || !gap_align || !score_params || !ext_params ||
       !hit_params || !init_hitlist || !hsp_list_ptr)
      return 1;

   if (init_hitlist->total == 0)
      return 0;

   is_prot = (program_number != eBlastTypeBlastn);
   orig_pssm = gap_align->sbp->posMatrix;

   if (*hsp_list_ptr == NULL)
      *hsp_list_ptr = hsp_list = Blast_HSPListNew(hit_options->hsp_num_max);
   else 
      hsp_list = *hsp_list_ptr;

   init_hsp_array = (BlastInitHSP**) 
      malloc(init_hitlist->total*sizeof(BlastInitHSP*));

   for (index = 0; index < init_hitlist->total; ++index)
      init_hsp_array[index] = &init_hitlist->init_hsp_array[index];


   if (!Blast_GappedScorePrelimTest(program_number, query, query_info, 
           subject, gap_align, score_params, ext_params, hit_params, 
           init_hsp_array, init_hitlist->total, gapped_stats))
      return 0;

   /* helper contains most frequently used information to speed up access. */
   helper = (SSeqRange*) malloc((init_hitlist->total)*sizeof(SSeqRange));

   for (index=0; index<init_hitlist->total; index++)
   {
      Boolean delete_hsp = FALSE;  /* Set if HSP is contained within another. */
      init_hsp = init_hsp_array[index];

      /* Now adjust the initial HSP's coordinates. */
      GetRelativeCoordinates(query, query_info, init_hsp, &query_tmp, 
                             NULL, &context);

      if (orig_pssm)
         gap_align->sbp->posMatrix = orig_pssm + 
                                query_info->context_offsets[context];

      /* This prefetches this value for the test below. */
      next_offset = init_hsp->q_off;

      if (!init_hsp->ungapped_data) {
         q_start = q_end = next_offset;
         s_start = s_end = init_hsp->s_off;
      } else {
         q_start = init_hsp->ungapped_data->q_start;
         q_end = q_start + init_hsp->ungapped_data->length;
         s_start = init_hsp->ungapped_data->s_start;
         s_end = s_start + init_hsp->ungapped_data->length;
      }

      hsp1 = NULL;
      for (index1=0; index1<hsp_list->hspcnt; index1++)
      {
         delete_hsp = FALSE;
         hsp1 = hsp_list->hsp_array[index1];

         if (hsp1->context != context)
            continue;
         /* Check with the helper array whether further
            tests are warranted.  Having only two ints
            in the helper array speeds up access. */
         if (helper[index1].left <= next_offset &&
             helper[index1].right >= next_offset)
         {
            if (hit_options->min_diag_separation > 0)
            {
               if (MB_HSP_CONTAINED(q_start, hsp1->query.offset, hsp1->query.end,
                   s_start, hsp1->subject.offset, hsp1->subject.end, hit_options->min_diag_separation) &&
                   (!init_hsp->ungapped_data ||
                    init_hsp->ungapped_data->score <= hsp1->score))
               {
                     delete_hsp = TRUE;
                     break;
               }
            } 
            else 
            {
               Boolean hsp_start_is_contained=FALSE, hsp_end_is_contained=FALSE;
               
               if (CONTAINED_IN_HSP(hsp1->query.offset, hsp1->query.end, q_start,
                   hsp1->subject.offset, hsp1->subject.end, s_start) &&
                   (!init_hsp->ungapped_data || (SIGN(hsp1->query.frame) ==
                       SIGN(init_hsp->ungapped_data->frame))))
               {
                     hsp_start_is_contained = TRUE;
               }

               if (hsp_start_is_contained && (CONTAINED_IN_HSP(hsp1->query.offset,
                   hsp1->query.end, q_end, hsp1->subject.offset,
                   hsp1->subject.end, s_end)))
               {
                  hsp_end_is_contained = TRUE;
               }

               if (hsp_start_is_contained && hsp_end_is_contained &&
                   (!init_hsp->ungapped_data ||
                    init_hsp->ungapped_data->score <= hsp1->score))
               {
                  delete_hsp = TRUE;
                  break;
               }
            }
         }
      }
      
      if (!delete_hsp)
      {
         BlastHSP* new_hsp;

         if (gapped_stats) {
            ++gapped_stats->extensions;
         }
 
         if(is_prot && !score_params->options->is_ooframe) {
            max_offset = 
               BlastGetStartForGappedAlignment(query_tmp.sequence, 
                  subject->sequence, gap_align->sbp,
                  init_hsp->ungapped_data->q_start,
                  init_hsp->ungapped_data->length,
                  init_hsp->ungapped_data->s_start,
                  init_hsp->ungapped_data->length);
            init_hsp->s_off += max_offset - init_hsp->q_off;
            init_hsp->q_off = max_offset;
         }

         if (is_prot) {
            status =  BLAST_ProtGappedAlignment(program_number, &query_tmp, 
                         subject, gap_align, score_params, init_hsp);
         } else {
            status = BLAST_DynProgNtGappedAlignment(&query_tmp, subject, 
                         gap_align, score_params, init_hsp);
         }

         if (status) {
            sfree(init_hsp_array);
            gap_align->sbp->posMatrix = orig_pssm;
            return status;
         }

         Blast_HSPInit(gap_align->query_start, gap_align->query_stop,
                       gap_align->subject_start, gap_align->subject_stop, 
                       init_hsp->q_off, init_hsp->s_off, 
                       context, subject->frame, gap_align->score,
                       &(gap_align->edit_block), &new_hsp);
         Blast_HSPListSaveHSP(hsp_list, new_hsp);

         /* Fill in the helper structure. */
         helper[hsp_list->hspcnt - 1].left = gap_align->query_start;
         helper[hsp_list->hspcnt - 1].right = gap_align->query_stop;
      }
      /* Free ungapped data here - it's no longer needed */
      sfree(init_hsp->ungapped_data);
   }   

   sfree(init_hsp_array);
   sfree(helper);
   gap_align->sbp->posMatrix = orig_pssm;
      
   /* Remove any HSPs that share a starting or ending diagonal
      with a higher-scoring HSP. Do not perform this step with RPS
      blast, because the alignments can change during the traceback
      and thus may not share any diagonals later */

   if (is_prot && program_number != eBlastTypeRpsBlast) {
      hsp_list->hspcnt = 
         CheckGappedAlignmentsForOverlap(hsp_list->hsp_array, 
            hsp_list->hspcnt);
   }
   *hsp_list_ptr = hsp_list;
   return status;
}

/** Out-of-frame gapped alignment.
 * @param query Query sequence [in]
 * @param subject Subject sequence [in]
 * @param q_off Offset in query [in]
 * @param s_off Offset in subject [in]
 * @param S Start of the traceback buffer [in]
 * @param private_q_start Extent of alignment in query [out]
 * @param private_s_start Extent of alignment in subject [out]
 * @param score_only Return score only, without traceback [in]
 * @param sapp End of the traceback buffer [out]
 * @param gap_align Gapped alignment information and preallocated 
 *                  memory [in] [out]
 * @param score_params Scoring parameters [in]
 * @param psi_offset Starting position in PSI-BLAST matrix [in]
 * @param reversed Direction of the extension [in]
 * @param switch_seq Sequences need to be switched for blastx, 
 *                   but not for tblastn [in]
 */
static Int4 
OOF_SEMI_G_ALIGN_EX(Uint1* query, Uint1* subject, Int4 q_off, 
   Int4 s_off, Int4* S, Int4* private_q_start, Int4* private_s_start, 
   Boolean score_only, Int4** sapp, BlastGapAlignStruct* gap_align, 
   const BlastScoringParameters* score_params, Int4 psi_offset, 
   Boolean reversed, Boolean switch_seq)
{
   if (switch_seq) {
      return OOF_SEMI_G_ALIGN(subject, query, s_off, q_off, S, 
                private_s_start, private_q_start, score_only, sapp, 
                gap_align, score_params, psi_offset, reversed);
   } else {
      return OOF_SEMI_G_ALIGN(query, subject, q_off, s_off, S,
                private_q_start, private_s_start, score_only, sapp, 
                gap_align, score_params, psi_offset, reversed);
   }
}

#define MAX_SUBJECT_OFFSET 90000
#define MAX_TOTAL_GAPS 3000

void 
AdjustSubjectRange(Int4* subject_offset_ptr, Int4* subject_length_ptr, 
		   Int4 query_offset, Int4 query_length, Int4* start_shift)
{
   Int4 s_offset;
   Int4 subject_length = *subject_length_ptr;
   Int4 max_extension_left, max_extension_right;
   
   /* If subject sequence is not too long, leave everything as is */
   if (subject_length < MAX_SUBJECT_OFFSET) {
      *start_shift = 0;
      return;
   }

   s_offset = *subject_offset_ptr;
   /* Maximal extension length is the remaining length in the query, plus 
      an estimate of a maximal total number of gaps. */
   max_extension_left = query_offset + MAX_TOTAL_GAPS;
   max_extension_right = query_length - query_offset + MAX_TOTAL_GAPS;

   if (s_offset <= max_extension_left) {
      *start_shift = 0;
   } else {
      *start_shift = s_offset - max_extension_left;
      *subject_offset_ptr = max_extension_left;
   }

   *subject_length_ptr = 
      MIN(subject_length, s_offset + max_extension_right) - *start_shift;
}

/** Performs gapped extension for protein sequences, given two
 * sequence blocks, scoring and extension options, and an initial HSP 
 * with information from the previously performed ungapped extension
 * @param program BLAST program [in]
 * @param query_blk The query sequence block [in]
 * @param subject_blk The subject sequence block [in]
 * @param gap_align The auxiliary structure for gapped alignment [in]
 * @param score_params Parameters related to scoring [in]
 * @param init_hsp The initial HSP information [in]
 */
static Int2 BLAST_ProtGappedAlignment(EBlastProgramType program, 
   BLAST_SequenceBlk* query_blk, BLAST_SequenceBlk* subject_blk, 
   BlastGapAlignStruct* gap_align,
   const BlastScoringParameters* score_params, BlastInitHSP* init_hsp)
{
   Boolean found_start, found_end;
   Int4 q_length=0, s_length=0, score_right, score_left;
   Int4 private_q_start, private_s_start;
   Uint1* query=NULL,* subject=NULL;
   Boolean switch_seq = FALSE;
   Int4 query_length = query_blk->length;
   Int4 subject_length = subject_blk->length;
   Int4 subject_shift = 0;
   BlastScoringOptions *score_options = score_params->options;
    
   if (gap_align == NULL)
      return FALSE;
   
   if (score_options->is_ooframe) {
      q_length = init_hsp->q_off;
      s_length = init_hsp->s_off;

      if (program == eBlastTypeBlastx) {
         subject = subject_blk->sequence + s_length;
         query = query_blk->oof_sequence + CODON_LENGTH + q_length;
         query_length -= CODON_LENGTH - 1;
         switch_seq = TRUE;
      } else if (program == eBlastTypeTblastn ||
                 program == eBlastTypeRpsTblastn) {
         subject = subject_blk->oof_sequence + CODON_LENGTH + s_length;
         query = query_blk->sequence + q_length;
         subject_length -= CODON_LENGTH - 1;
      }
   } else {
      q_length = init_hsp->q_off + 1;
      s_length = init_hsp->s_off + 1;
      query = query_blk->sequence;
      subject = subject_blk->sequence;
   }

   AdjustSubjectRange(&s_length, &subject_length, q_length, query_length, 
                      &subject_shift);

   found_start = FALSE;
   found_end = FALSE;
    
   /* Looking for "left" score */
   score_left = 0;
   if (q_length != 0 && s_length != 0) {
      found_start = TRUE;
      if(score_options->is_ooframe) {
         score_left = OOF_SEMI_G_ALIGN_EX(query, subject, q_length, s_length,
               NULL, &private_q_start, &private_s_start, TRUE, NULL, 
               gap_align, score_params, q_length, TRUE, switch_seq);
      } else {
         score_left = SEMI_G_ALIGN_EX(query, subject+subject_shift, q_length, 
            s_length, NULL,
            &private_q_start, &private_s_start, TRUE, NULL, gap_align, 
            score_params, init_hsp->q_off, FALSE, TRUE);
      }
        
      gap_align->query_start = q_length - private_q_start;
      gap_align->subject_start = s_length - private_s_start + subject_shift;
      
   }

   score_right = 0;
   if (q_length < query_length && s_length < subject_length) {
      found_end = TRUE;
      if(score_options->is_ooframe) {
         score_right = OOF_SEMI_G_ALIGN_EX(query-1, subject-1, 
            query_length-q_length+1, subject_length-s_length+1,
            NULL, &(gap_align->query_stop), &(gap_align->subject_stop), 
            TRUE, NULL, gap_align, 
            score_params, q_length, FALSE, switch_seq);
         gap_align->query_stop += q_length;
         gap_align->subject_stop += s_length + subject_shift;
      } else {
         score_right = SEMI_G_ALIGN_EX(query+init_hsp->q_off,
            subject+init_hsp->s_off, query_length-q_length, 
            subject_length-s_length, NULL, &(gap_align->query_stop), 
            &(gap_align->subject_stop), TRUE, NULL, gap_align, 
            score_params, init_hsp->q_off, FALSE, FALSE);
         /* Make end offsets point to the byte after the end of the 
            alignment */
         gap_align->query_stop += init_hsp->q_off + 1;
         gap_align->subject_stop += init_hsp->s_off + 1;
      }
   }
   
   if (found_start == FALSE) {	/* Start never found */
      gap_align->query_start = init_hsp->q_off;
      gap_align->subject_start = init_hsp->s_off;
   }
   
   if (found_end == FALSE) {
      gap_align->query_stop = init_hsp->q_off - 1;
      gap_align->subject_stop = init_hsp->s_off - 1;
      
      if(score_options->is_ooframe) {  /* Do we really need this ??? */
         gap_align->query_stop++;
         gap_align->subject_stop++;
      }
   }
   
   gap_align->score = score_right+score_left;

   return 0;
}

Int2 
BLAST_TracebackToGapEditBlock(Int4* S, Int4 M, Int4 N, Int4 start1, 
                              Int4 start2, GapEditBlock** edit_block)
{

  Int4 i, j, op, number_of_subs, number_of_decline;
  GapEditScript* e_script;

  if (S == NULL)
     return -1;

  i = j = op = number_of_subs = number_of_decline = 0;

  *edit_block = GapEditBlockNew(start1, start2);
  (*edit_block)->esp = e_script = GapEditScriptNew(NULL);

  while(i < M || j < N) 
  {
	op = *S;
	if (op != MININT && number_of_decline > 0) 
	{
               e_script->op_type = eGapAlignDecline;
               e_script->num = number_of_decline;
               e_script = GapEditScriptNew(e_script);
		number_of_decline = 0;
	}
        if (op != 0 && number_of_subs > 0) 
	{
                        e_script->op_type = eGapAlignSub;
                        e_script->num = number_of_subs;
                        e_script = GapEditScriptNew(e_script);
                        number_of_subs = 0;
        }      
	if (op == 0) {
		i++; j++; number_of_subs++;
	} else if (op == MININT) {
		i++; j++;
		number_of_decline++; 
	}	
	else
	{
		if(op > 0) 
		{
			e_script->op_type = eGapAlignDel;
			e_script->num = op;
			j += op;
			if (i < M || j < N)
                                e_script = GapEditScriptNew(e_script);
		}
		else
		{
			e_script->op_type = eGapAlignIns;
			e_script->num = ABS(op);
			i += ABS(op);
			if (i < M || j < N)
                                e_script = GapEditScriptNew(e_script);
		}
    	}
	S++;
  }

  if (number_of_subs > 0)
  {
	e_script->op_type = eGapAlignSub;
	e_script->num = number_of_subs;
  } else if (number_of_decline > 0) {
        e_script->op_type = eGapAlignDecline;
        e_script->num = number_of_decline;
  }

  return 0;
}

/** Converts a OOF traceback from the gapped alignment to a GapEditBlock.
 * This function is for out-of-frame gapping:
 * index1 is for the protein, index2 is for the nucleotides.
 * 0: deletion of a dna codon, i.e dash aligned with a protein letter.
 * 1: one nucleotide vs a protein, deletion of 2 nuc.
 * 2: 2 nucleotides aligned with a protein, i.e deletion of one nuc.
 * 3: substitution, 3 nucleotides vs an amino acid.
 * 4: 4 nuc vs an amino acid.
 * 5: 5 nuc vs an amino acid.
 * 6: a codon aligned with a dash. opposite of 0.
 */

static Int2
BLAST_OOFTracebackToGapEditBlock(Int4* S, Int4 q_length, 
   Int4 s_length, Int4 q_start, Int4 s_start, EBlastProgramType program, 
   GapEditBlock** edit_block_ptr)
{
    register Int4 current_val, last_val, number, index1, index2;
    Int4 M, N;
    GapEditScript* e_script;
    GapEditBlock* edit_block;
    
    if (S == NULL)
        return -1;
    
    number = 0;
    index1 = 0;
    index2 = 0;
    
    *edit_block_ptr = edit_block = GapEditBlockNew(q_start, s_start);
    edit_block->is_ooframe = TRUE;
    if (program == eBlastTypeBlastx) {
        M = s_length;
        N = q_length;
    } else {
        M = q_length;
        N = s_length;
    }

    edit_block->esp = e_script = GapEditScriptNew(NULL);
    
    last_val = *S;
    
    /* index1, M - index and length of protein, 
       index2, N - length of the nucleotide */
    
    for(index1 = 0, index2 = 0; index1 < M && index2 < N; S++, number++) {
        
        current_val = *S;
        /* New script element will be created for any new frameshift
           region  0,3,6 will be collected in a single segment */
        if (current_val != last_val || (current_val%3 != 0 && 
                                        edit_block->esp != e_script)) {
            e_script->num = number;
            e_script = GapEditScriptNew(e_script);
            
            /* if(last_val%3 != 0 && current_val%3 == 0) */
            if(last_val%3 != 0 && current_val == eGapAlignSub) 
                /* 1, 2, 4, 5 vs. 0, 3, 6*/                
                number = 1;
            else
                number = 0;
        }
        
        last_val = current_val;
        
        /* for out_of_frame == TRUE - we have op_type == S parameter */
        e_script->op_type = current_val;
        
        if(current_val != eGapAlignIns) {
            index1++;
            index2 += current_val;
        } else {
            index2 += 3;
        }
    }
    /* Get the last one. */    
    e_script->num = number;

    return 0;
}

Int2 BLAST_GappedAlignmentWithTraceback(EBlastProgramType program, Uint1* query, 
        Uint1* subject, BlastGapAlignStruct* gap_align, 
        const BlastScoringParameters* score_params,
        Int4 q_start, Int4 s_start, Int4 query_length, Int4 subject_length)
{
    Boolean found_start, found_end;
    Int4 score_right, score_left, private_q_length, private_s_length, tmp;
    Int4 q_length, s_length;
    Int4 prev;
    Int4* tback,* tback1,* p = NULL,* q;
    Boolean is_ooframe = score_params->options->is_ooframe;
    Int2 status = 0;
    Boolean switch_seq = FALSE;
    
    if (gap_align == NULL)
        return -1;
    
    found_start = FALSE;
    found_end = FALSE;
    
    q_length = query_length;
    s_length = subject_length;
    if (is_ooframe) {
       /* The mixed frame sequence is shifted to the 3rd position, so its 
          maximal available length for extension is less by 2 than its 
          total length. */
       switch_seq = (program == eBlastTypeBlastx);
       if (switch_seq) {
          q_length -= CODON_LENGTH - 1;
       } else {
          s_length -= CODON_LENGTH - 1;
       }
    }

    tback = tback1 = (Int4*)
       malloc((s_length + q_length)*sizeof(Int4));

    score_left = 0; prev = 3;
    found_start = TRUE;
        
    if(is_ooframe) {
       /* NB: Left extension does not include starting point corresponding
          to the offset pair; the right extension does. */
       score_left =
          OOF_SEMI_G_ALIGN_EX(query+q_start, subject+s_start, q_start, 
             s_start, tback, &private_q_length, &private_s_length, FALSE,
             &tback1, gap_align, score_params, q_start, TRUE, switch_seq);
       gap_align->query_start = q_start - private_q_length;
       gap_align->subject_start = s_start - private_s_length;
    } else {        
       /* NB: The left extension includes the starting point 
          [q_start,s_start]; the right extension does not. */
       score_left = 
          SEMI_G_ALIGN_EX(query, subject, q_start+1, s_start+1, tback, 
             &private_q_length, &private_s_length, FALSE, &tback1, 
             gap_align, score_params, q_start, FALSE, TRUE);
       gap_align->query_start = q_start - private_q_length + 1;
       gap_align->subject_start = s_start - private_s_length + 1;
    }
    
    for(p = tback, q = tback1-1; p < q; p++, q--)  {
       tmp = *p;
       *p = *q;
       *q = tmp;
    }
        
    if(is_ooframe){
       for (prev = 3, p = tback; p < tback1; p++) {
          if (*p == 0 || *p ==  6) continue;
          tmp = *p; *p = prev; prev = tmp;
       }
    }
    
    score_right = 0;

    if ((q_start < q_length) && (s_start < s_length)) {
       found_end = TRUE;
       if(is_ooframe) {
          score_right = 
             OOF_SEMI_G_ALIGN_EX(query+q_start-1, subject+s_start-1, 
                q_length-q_start, s_length-s_start, 
                tback1, &private_q_length, &private_s_length, FALSE,
                &tback1, gap_align, score_params, q_start, FALSE, switch_seq);
            if (prev != 3 && p) {
                while (*p == 0 || *p == 6) p++;
                *p = prev+*p-3;
            }
        } else {
            score_right = 
               SEMI_G_ALIGN_EX(query+q_start, subject+s_start, 
                  q_length-q_start-1, s_length-s_start-1, 
                  tback1, &private_q_length, &private_s_length, FALSE, 
                  &tback1, gap_align, score_params, q_start, FALSE, FALSE);
        }

        gap_align->query_stop = q_start + private_q_length + 1;
        gap_align->subject_stop = s_start + private_s_length + 1;
    }
    
    if (found_start == FALSE) {	/* Start never found */
        gap_align->query_start = q_start;
        gap_align->subject_start = s_start;
    }
    
    if (found_end == FALSE) {
        gap_align->query_stop = q_start - 1;
        gap_align->subject_stop = s_start - 1;
    }

    if(is_ooframe) {
        status = BLAST_OOFTracebackToGapEditBlock(tback, 
           gap_align->query_stop-gap_align->query_start, 
           gap_align->subject_stop-gap_align->subject_start, 
           gap_align->query_start, gap_align->subject_start, 
           program, &gap_align->edit_block);
    } else {
        status = BLAST_TracebackToGapEditBlock(tback, 
           gap_align->query_stop-gap_align->query_start, 
           gap_align->subject_stop-gap_align->subject_start, 
           gap_align->query_start, gap_align->subject_start,
           &gap_align->edit_block);
    }

    gap_align->edit_block->length1 = query_length;
    gap_align->edit_block->length2 = subject_length;
#if 0 /** How can these be assigned???? This data is now not in gap_align. */
    gap_align->edit_block->translate1 = gap_align->translate1;
    gap_align->edit_block->translate2 = gap_align->translate2;
    gap_align->edit_block->discontinuous = gap_align->discontinuous;
#endif

    sfree(tback);

    gap_align->score = score_right+score_left;
    
    return status;
}

static Int4 
GetPatternLengthFromGapAlignStruct(BlastGapAlignStruct* gap_align)
{
   /* Kludge: pattern lengths are saved in the output structure member 
      query_stop. Probably should be changed??? */
   return gap_align->query_stop;
}

Int2 PHIGappedAlignmentWithTraceback(Uint1* query, Uint1* subject, 
        BlastGapAlignStruct* gap_align, 
        const BlastScoringParameters* score_params,
        Int4 q_start, Int4 s_start, Int4 query_length, Int4 subject_length)
{
    Boolean found_end;
    Int4 score_right, score_left, private_q_length, private_s_length, tmp;
    Int4* tback,* tback1,* p = NULL,* q;
    Int2 status = 0;
    Int4 pat_length;
    
    if (gap_align == NULL)
        return -1;
    
    found_end = FALSE;
    
    tback = tback1 = (Int4*)
       malloc((subject_length + query_length)*sizeof(Int4));

    score_left = 0;
        
    score_left = 
       SEMI_G_ALIGN_EX(query, subject, q_start, s_start, tback, 
           &private_q_length, &private_s_length, FALSE, &tback1, 
          gap_align, score_params, q_start, FALSE, TRUE);
    gap_align->query_start = q_start - private_q_length;
    gap_align->subject_start = s_start - private_s_length;
    
    for(p = tback, q = tback1-1; p < q; p++, q--)  {
       tmp = *p;
       *p = *q;
       *q = tmp;
    }
        
    pat_length = GetPatternLengthFromGapAlignStruct(gap_align);
    memset(tback1, 0, pat_length*sizeof(Int4));
    tback1 += pat_length;

    score_right = 0;

    q_start += pat_length - 1;
    s_start += pat_length - 1;

    if ((q_start < query_length) && (s_start < subject_length)) {
       found_end = TRUE;
       score_right = 
          SEMI_G_ALIGN_EX(query+q_start, subject+s_start, 
             query_length-q_start-1, subject_length-s_start-1, 
             tback1, &private_q_length, &private_s_length, FALSE, 
             &tback1, gap_align, score_params, q_start, FALSE, FALSE);

       gap_align->query_stop = q_start + private_q_length + 1;
       gap_align->subject_stop = s_start + private_s_length + 1; 
    }
    
    if (found_end == FALSE) {
        gap_align->query_stop = q_start;
        gap_align->subject_stop = s_start;
    }

    status = BLAST_TracebackToGapEditBlock(tback, 
                gap_align->query_stop-gap_align->query_start, 
                gap_align->subject_stop-gap_align->subject_start, 
                gap_align->query_start, gap_align->subject_start,
                &gap_align->edit_block);

    gap_align->edit_block->length1 = query_length;
    gap_align->edit_block->length2 = subject_length;

    sfree(tback);

    gap_align->score = score_right+score_left;
    
    return status;
}

Int2 BLAST_GetUngappedHSPList(BlastInitHitList* init_hitlist,
        BlastQueryInfo* query_info, BLAST_SequenceBlk* subject, 
        const BlastHitSavingOptions* hit_options, 
        BlastHSPList** hsp_list_ptr)
{
   BlastHSPList* hsp_list = NULL;
   Int4 index;
   BlastInitHSP* init_hsp;
   Int4 context;

   /* The BlastHSPList structure can be allocated and passed from outside */
   if (*hsp_list_ptr != NULL)
      hsp_list = *hsp_list_ptr;

   if (!init_hitlist) {
      if (!hsp_list)
         *hsp_list_ptr = NULL;
      else
         hsp_list->hspcnt = 0;
      return 0;
   }

   for (index = 0; index < init_hitlist->total; ++index) {
      BlastHSP* new_hsp;
      BlastUngappedData* ungapped_data=NULL;
      init_hsp = &init_hitlist->init_hsp_array[index];
      if (!init_hsp->ungapped_data) 
         continue;

      /* Adjust the initial HSP's coordinates in case of concatenated 
         multiple queries/strands/frames */
      GetRelativeCoordinates(NULL, query_info, init_hsp, NULL, 
                             NULL, &context);
      if (!hsp_list) {
         hsp_list = Blast_HSPListNew(hit_options->hsp_num_max);
         *hsp_list_ptr = hsp_list;
      }
      ungapped_data = init_hsp->ungapped_data;
      Blast_HSPInit(ungapped_data->q_start, ungapped_data->length+ungapped_data->q_start,
                    ungapped_data->s_start, ungapped_data->length+ungapped_data->s_start,
                    init_hsp->q_off, init_hsp->s_off, 
                    context, subject->frame, ungapped_data->score,
                    NULL, &new_hsp);
      Blast_HSPListSaveHSP(hsp_list, new_hsp);
   }
   return 0;
}

/** Performs gapped extension for PHI BLAST, given two
 * sequence blocks, scoring and extension options, and an initial HSP 
 * with information from the previously performed ungapped extension
 * @param query_blk The query sequence block [in]
 * @param subject_blk The subject sequence block [in]
 * @param gap_align The auxiliary structure for gapped alignment [in]
 * @param score_params Parameters related to scoring [in]
 * @param init_hsp The initial HSP information [in]
 */
static Int2 
PHIGappedAlignment(BLAST_SequenceBlk* query_blk, 
                   BLAST_SequenceBlk* subject_blk, 
                   BlastGapAlignStruct* gap_align,
                   const BlastScoringParameters* score_params, 
                   BlastInitHSP* init_hsp)
{
   Boolean found_start, found_end;
   Int4 q_length=0, s_length=0, score_right, score_left;
   Int4 private_q_start, private_s_start;
   Uint1* query,* subject;
    
   if (gap_align == NULL)
      return FALSE;
   
   q_length = init_hsp->q_off;
   s_length = init_hsp->s_off;
   query = query_blk->sequence;
   subject = subject_blk->sequence;

   found_start = FALSE;
   found_end = FALSE;
    
   /* Looking for "left" score */
   score_left = 0;
   if (q_length != 0 && s_length != 0) {
      found_start = TRUE;
      score_left = 
         SEMI_G_ALIGN_EX(query, subject, q_length, s_length, NULL,
            &private_q_start, &private_s_start, TRUE, NULL, gap_align, 
            score_params, init_hsp->q_off, FALSE, TRUE);
        
      gap_align->query_start = q_length - private_q_start + 1;
      gap_align->subject_start = s_length - private_s_start + 1;
      
   } else {
      q_length = init_hsp->q_off;
      s_length = init_hsp->s_off;
   }

   /* Pattern itself is not included in the gapped alignment */
   q_length += init_hsp->ungapped_data->length - 1;
   s_length += init_hsp->ungapped_data->length - 1;

   score_right = 0;
   if (init_hsp->q_off < query_blk->length && 
       init_hsp->s_off < subject_blk->length) {
      found_end = TRUE;
      score_right = SEMI_G_ALIGN_EX(query+q_length,
         subject+s_length, query_blk->length-q_length, 
         subject_blk->length-s_length, NULL, &(gap_align->query_stop), 
         &(gap_align->subject_stop), TRUE, NULL, gap_align, 
         score_params, q_length, FALSE, FALSE);
      gap_align->query_stop += q_length;
      gap_align->subject_stop += s_length;
   }
   
   if (found_start == FALSE) {	/* Start never found */
      gap_align->query_start = init_hsp->q_off;
      gap_align->subject_start = init_hsp->s_off;
   }
   
   if (found_end == FALSE) {
      gap_align->query_stop = init_hsp->q_off - 1;
      gap_align->subject_stop = init_hsp->s_off - 1;
   }
   
   gap_align->score = score_right+score_left;

   return 0;
}

Int2 PHIGetGappedScore (EBlastProgramType program_number, 
        BLAST_SequenceBlk* query, BlastQueryInfo* query_info, 
        BLAST_SequenceBlk* subject, 
        BlastGapAlignStruct* gap_align,
        const BlastScoringParameters* score_params,
        const BlastExtensionParameters* ext_params,
        const BlastHitSavingParameters* hit_params,
        BlastInitHitList* init_hitlist,
        BlastHSPList** hsp_list_ptr, BlastGappedStats* gapped_stats)

{
   BlastHSPList* hsp_list;
   BlastInitHSP** init_hsp_array;
   BlastInitHSP* init_hsp;
   Int4 index;
   Int2 status = 0;
   BlastHitSavingOptions* hit_options;
   BLAST_SequenceBlk query_tmp;
   Int4 context;

   if (!query || !subject || !gap_align || !score_params || !ext_params ||
       !hit_params || !init_hitlist || !hsp_list_ptr)
      return 1;

   if (init_hitlist->total == 0)
      return 0;

   hit_options = hit_params->options;
   if (*hsp_list_ptr == NULL)
      *hsp_list_ptr = hsp_list = Blast_HSPListNew(hit_options->hsp_num_max);
   else 
      hsp_list = *hsp_list_ptr;

   init_hsp_array = (BlastInitHSP**) 
      malloc(init_hitlist->total*sizeof(BlastInitHSP*));

   for (index = 0; index < init_hitlist->total; ++index)
      init_hsp_array[index] = &init_hitlist->init_hsp_array[index];

   for (index=0; index<init_hitlist->total; index++)
   {
      BlastHSP* new_hsp;
      init_hsp = init_hsp_array[index];

      if (gapped_stats)
         ++gapped_stats->extensions;

      /* Adjust the initial HSP's coordinates to ones relative to an 
         individual query sequence */
      GetRelativeCoordinates(query, query_info, init_hsp, &query_tmp, 
                             NULL, &context);
      status =  PHIGappedAlignment(&query_tmp, subject, gap_align, 
                                   score_params, init_hsp);

      if (status) {
         sfree(init_hsp_array);
         return status;
      }

      Blast_HSPInit(gap_align->query_start, gap_align->query_stop,
                    gap_align->subject_start, gap_align->subject_stop, 
                    init_hsp->q_off, init_hsp->s_off, 
                    context, subject->frame, gap_align->score,
                    &(gap_align->edit_block), &new_hsp);
      new_hsp->pattern_length = init_hsp->ungapped_data->length;
      Blast_HSPListSaveHSP(hsp_list, new_hsp);

      /* Free ungapped data here - it's no longer needed */
      sfree(init_hsp->ungapped_data);
   }   

   sfree(init_hsp_array);

   *hsp_list_ptr = hsp_list;
   return status;
}

