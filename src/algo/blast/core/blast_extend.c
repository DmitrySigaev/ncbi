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

File name: blast_extend.c

Author: Ilya Dondoshansky

Contents: Functions to initialize structures used for BLAST extension

******************************************************************************
 * $Revision$
 * */

static char const rcsid[] = "$Id$";

#include <algo/blast/core/blast_extend.h>
#include <algo/blast/core/blast_options.h>
#include <algo/blast/core/blast_lookup.h>
#include <algo/blast/core/mb_lookup.h>
#include <algo/blast/core/blast_util.h> /* for READDB_UNPACK_BASE macros */

#define MIN_INIT_HITLIST_SIZE 100

/* Description in blast_extend.h */
BlastInitHitList* BLAST_InitHitListNew(void)
{
   BlastInitHitList* init_hitlist = (BlastInitHitList*)
      calloc(1, sizeof(BlastInitHitList));

   init_hitlist->allocated = MIN_INIT_HITLIST_SIZE;

   init_hitlist->init_hsp_array = (BlastInitHSP*)
      malloc(MIN_INIT_HITLIST_SIZE*sizeof(BlastInitHSP));

   return init_hitlist;
}

BlastInitHitList* BLAST_InitHitListDestruct(BlastInitHitList* init_hitlist)
{
  sfree(init_hitlist->init_hsp_array);
  sfree(init_hitlist);
  return NULL;
}


/** Allocates memory for the BLAST_DiagTable*. This function also 
 * sets many of the parametes such as diag_array_length etc.
 * @param qlen Length of the query [in]
 * @param multiple_hits Specifies whether multiple hits method is used [in]
 * @param window_size The max. distance between two hits that are extended [in]
 * @return The allocated BLAST_DiagTable structure
*/
static BLAST_DiagTable*
BLAST_DiagTableNew (Int4 qlen, Boolean multiple_hits, Int4 window_size)

{
        BLAST_DiagTable* diag_table;
        Int4 diag_array_length;

        diag_table= (BLAST_DiagTable*) calloc(1, sizeof(BLAST_DiagTable));

        if (diag_table)
        {
                diag_array_length = 1;
                /* What power of 2 is just longer than the query? */
                while (diag_array_length < (qlen+window_size))
                {
                        diag_array_length = diag_array_length << 1;
                }
                /* These are used in the word finders to shift and mask
                rather than dividing and taking the remainder. */
                diag_table->diag_array_length = diag_array_length;
                diag_table->diag_mask = diag_array_length-1;
                diag_table->multiple_hits = multiple_hits;
                diag_table->offset = window_size;
                diag_table->window = window_size;
        }
        return diag_table;
}

/* Description in blast_extend.h */
Int2 BLAST_ExtendWordInit(BLAST_SequenceBlk* query,
   const BlastInitialWordOptions* word_options,
   Int8 dblen, Int4 dbseq_num, BLAST_ExtendWord** ewp_ptr)
{
   BLAST_ExtendWord* ewp;
   Int4 index, i;

   *ewp_ptr = ewp = (BLAST_ExtendWord*) calloc(1, sizeof(BLAST_ExtendWord));

   if (!ewp) {
      return -1;
   }

   if (word_options->container_type == eMbStacks) {
      double av_search_space;
      Int4 stack_size, num_stacks;
      MB_StackTable* stack_table;

      ewp->stack_table = stack_table = 
         (MB_StackTable*) malloc(sizeof(MB_StackTable));

      av_search_space = 
         ((double) query->length) * dblen / dbseq_num;
      num_stacks = MIN(1 + (Int4) (sqrt(av_search_space)/100), 500);
      stack_size = 5000/num_stacks;
      stack_table->stack_index = (Int4*) calloc(num_stacks, sizeof(Int4));
      stack_table->stack_size = (Int4*) malloc(num_stacks*sizeof(Int4));
      stack_table->estack = 
         (MbStack**) malloc(num_stacks*sizeof(MbStack*));
      for (index=0; index<num_stacks; index++) {
         stack_table->estack[index] = 
            (MbStack*) malloc(stack_size*sizeof(MbStack));
         stack_table->stack_size[index] = stack_size;
      }
      stack_table->num_stacks = num_stacks;
   } else {
      Boolean multiple_hits = (word_options->window_size > 0);
      Int4* buffer;
      BLAST_DiagTable* diag_table;

      ewp->diag_table = diag_table = 
         BLAST_DiagTableNew(query->length, multiple_hits, 
                            word_options->window_size);
      /* Allocate the buffer to be used for diagonal array. */
      buffer = 
         (Int4*) calloc(diag_table->diag_array_length, sizeof(DiagStruct));
      
      if (buffer == NULL)	{
         sfree(ewp);
         return -1;
      }
         
      diag_table->diag_array= (DiagStruct*) buffer;
      for(i=0;i<diag_table->diag_array_length;i++){
         diag_table->diag_array[i].diag_level=0;
         diag_table->diag_array[i].last_hit = -diag_table->window;
      }
   }

   *ewp_ptr = ewp;
   
   return 0;
}

Boolean BLAST_SaveInitialHit(BlastInitHitList* init_hitlist, 
                  Int4 q_off, Int4 s_off, BlastUngappedData* ungapped_data) 
{
   BlastInitHSP* match_array;
   Int4 num, num_avail;

   num = init_hitlist->total;
   num_avail = init_hitlist->allocated;

   match_array = init_hitlist->init_hsp_array;
   if (num>=num_avail) {
      if (init_hitlist->do_not_reallocate) 
         return FALSE;
      num_avail *= 2;
      match_array = (BlastInitHSP*) 
         realloc(match_array, num_avail*sizeof(BlastInitHSP));
      if (!match_array) {
         init_hitlist->do_not_reallocate = TRUE;
         return FALSE;
      } else {
         init_hitlist->allocated = num_avail;
         init_hitlist->init_hsp_array = match_array;
      }
   }

   match_array[num].q_off = q_off;
   match_array[num].s_off = s_off;
   match_array[num].ungapped_data = ungapped_data;

   init_hitlist->total++;
   return TRUE;
}

/** Traditional Mega BLAST initial word extension
 * @param query The query sequence [in]
 * @param subject The subject sequence [in]
 * @param lookup Lookup table structure [in]
 * @param word_params The parameters related to initial word extension [in]
 * @param matrix the substitution matrix for ungapped extension [in]
 * @param ewp The structure containing word extension information [in]
 * @param q_off The offset in the query sequence [in]
 * @param s_off The offset in the subject sequence [in]
 * @param init_hitlist The structure containing information about all 
 *                     initial hits [in] [out]
 */
static Int2
MB_ExtendInitialHit(BLAST_SequenceBlk* query, 
   BLAST_SequenceBlk* subject, LookupTableWrap* lookup,
   BlastInitialWordParameters* word_params, 
   Int4** matrix, BLAST_ExtendWord* ewp, Int4 q_off, Int4 s_off,
   BlastInitHitList* init_hitlist) 
{
   Int4 index, index1, step;
   MBLookupTable* mb_lt = (MBLookupTable*) lookup->lut;
   MbStack* estack;
   Int4 diag, stack_top;
   Int4 window, word_extra_length, scan_step;
   Boolean new_hit, hit_ready, two_hits, do_ungapped_extension;
   BLAST_DiagTable* diag_table = ewp->diag_table;
   MB_StackTable* stack_table = ewp->stack_table;
   BlastUngappedData* ungapped_data = NULL;
   BlastInitialWordOptions* word_options = word_params->options;
   Int4 s_pos;

   window = word_options->window_size;

   word_extra_length = 
      mb_lt->word_length - COMPRESSION_RATIO*mb_lt->compressed_wordsize;
   scan_step = mb_lt->scan_step;
   two_hits = (window > 0);
   do_ungapped_extension = word_options->ungapped_extension;

   if (diag_table) {
      DiagStruct* diag_array = diag_table->diag_array;
      DiagStruct* diag_array_elem;
      Int4 diag_mask;
      Int4 offset, s_pos;

      diag_mask = diag_table->diag_mask;
      offset = diag_table->offset;

      s_pos = s_off + offset;
      diag = (s_off + diag_table->diag_array_length - q_off) & diag_mask;
      diag_array_elem = &diag_array[diag];
      step = s_pos - diag_array_elem->last_hit;
      if (step <= 0)
         return 0;

      hit_ready = FALSE;
      if (!two_hits) {
         /* Single hit version */
         new_hit = (step > scan_step);
         hit_ready = (new_hit && (word_extra_length == 0)) || 
            (!new_hit && 
             (step + diag_array_elem->diag_level == word_extra_length));
      } else {
         /* Two hit version */
         if (diag_array_elem->diag_level > word_extra_length) {
            /* Previous hit already saved */
            new_hit = (step > scan_step);
         } else {
            new_hit = (step > window);
            hit_ready = (diag_array_elem->diag_level == word_extra_length) &&
               !new_hit;
         }
      }

      if (hit_ready) {
         if (do_ungapped_extension) {
            /* Perform ungapped extension */
            BlastnWordUngappedExtend(query, subject, matrix, q_off, s_off, 
               word_params->cutoff_score, -word_params->x_dropoff, 
               &ungapped_data);
            s_pos = ungapped_data->length - 1 + ungapped_data->s_start 
               + diag_table->offset;
            diag_array_elem->diag_level += 
               s_pos - diag_array_elem->last_hit;
            diag_array_elem->last_hit = s_pos;
         } else {
            ungapped_data = NULL;
            diag_array_elem->diag_level += step;
            diag_array_elem->last_hit = s_pos;
         }
         if (!ungapped_data || 
             ungapped_data->score >= word_params->cutoff_score) {
            BLAST_SaveInitialHit(init_hitlist, q_off, s_off, ungapped_data);
         } else {
            sfree(ungapped_data);
            /* Set diag_level to 0, indicating that any hit after this will 
               be new */
            diag_array_elem->diag_level = 0;
         }
      } else if (step > 0) {
         /* First hit in the 2-hit case or a direct extension of the 
            previous hit - update the last hit information only */
         if (new_hit)
            diag_array_elem->diag_level = 0;
         else
            diag_array_elem->diag_level += step;
         diag_array_elem->last_hit = s_pos;
      }
      
   } else {
      /* Use stacks instead of the diagonal array */
      index1 = (s_off - q_off) % stack_table->num_stacks;
      if (index1<0)
         index1 += stack_table->num_stacks;
      estack = stack_table->estack[index1];
      stack_top = stack_table->stack_index[index1] - 1;
      
      for (index = 0; index <= stack_top; ) {
         step = s_off - estack[index].level;
         if (estack[index].diag == s_off - q_off) {
            if (step <= 0) {
               stack_table->stack_index[index1] = stack_top + 1;
               return 0;
            }
            hit_ready = FALSE;
            if (!two_hits) {
               /* Single hit version */
               new_hit = (step > scan_step);
               hit_ready = (!new_hit && 
                  (step + estack[index].length == word_extra_length)) ||
                  (new_hit && (word_extra_length == 0));
            } else {
               /* Two hit version */
               if (estack[index].length > word_extra_length) {
                  /* Previous hit already saved */
                  new_hit = (step > scan_step);
               } else {
                  new_hit = (step > window);
                  hit_ready = (estack[index].length == word_extra_length) &&
                     !new_hit;
               }
            }
            if (hit_ready) {
               if (do_ungapped_extension) {
                  /* Perform ungapped extension */
                  BlastnWordUngappedExtend(query, subject, matrix, 
                      q_off, s_off, word_params->cutoff_score, 
                      -word_params->x_dropoff, &ungapped_data);
                  s_pos = ungapped_data->length - 1 + 
                     ungapped_data->s_start;
                  estack[index].length += s_pos - estack[index].level;
                  estack[index].level = s_pos;
               } else {
                  ungapped_data = NULL;
                  estack[index].length += step;
                  estack[index].level = s_off;
               }
               if (!ungapped_data || 
                   ungapped_data->score >= word_params->cutoff_score) {
                  BLAST_SaveInitialHit(init_hitlist, q_off, s_off, 
                                       ungapped_data);
               } else {
                  sfree(ungapped_data);
                  /* Set hit length back to 0 after ungapped extension 
                     failure */
                  estack[index].length = 0;
               }
            } else {
               /* First hit in the 2-hit case or a direct extension of the 
                  previous hit - update the last hit information only */
               if (new_hit)
                  estack[index].length = 0;
               else
                  estack[index].length += step;
               estack[index].level = s_off;
            }

            /* In case the size of this stack changed */
            stack_table->stack_index[index1] = stack_top + 1;	 
            return 0;
         } else if (step <= scan_step || (step <= window && 
                      estack[index].length >= word_extra_length)) {
            /* Hit from a different diagonal, and it can be continued */
            index++;
         } else {
            /* Hit from a different diagonal that does not continue: remove
               it from the stack */
            estack[index] = estack[stack_top];
            --stack_top;
         }
      }

      /* Need an extra slot on the stack for this hit */
      if (++stack_top >= stack_table->stack_size[index1]) {
         /* Stack about to overflow - reallocate memory */
         MbStack* ptr;
         if (!(ptr = (MbStack*)realloc(estack,
                     2*stack_table->stack_size[index1]*sizeof(MbStack)))) {
            return 1;
         } else {
            stack_table->stack_size[index1] *= 2;
            estack = stack_table->estack[index1] = ptr;
         }
      }
      /* Start a new hit */
      estack[stack_top].diag = s_off - q_off;
      estack[stack_top].level = s_off;
      estack[stack_top].length = 0;
      stack_table->stack_index[index1] = stack_top + 1;
      /* Save the hit if it already qualifies */
      if (!two_hits && (word_extra_length == 0)) {
         if (do_ungapped_extension) {
            /* Perform ungapped extension */
            BlastnWordUngappedExtend(query, subject, matrix, q_off, s_off, 
               word_params->cutoff_score, -word_params->x_dropoff, 
               &ungapped_data);
            estack[stack_top].level = ungapped_data->length - 1 + 
               ungapped_data->s_start;
            estack[stack_top].length = ungapped_data->length;
         } else {
            ungapped_data = NULL;
         }
         if (!ungapped_data || 
             ungapped_data->score >= word_params->cutoff_score) {
            BLAST_SaveInitialHit(init_hitlist, q_off, s_off, 
                                 ungapped_data);
         } else {
            sfree(ungapped_data);
            /* Set hit length back to 0 after ungapped extension 
               failure */
            estack[index].length = 0;
         }
      }
   }

   return 0;
}

/** Update the word extension structure after scanning of each subject sequence
 * @param ewp The structure holding word extension information [in] [out]
 * @param subject_length The length of the subject sequence that has just been
 *        processed [in]
 */
static Int2 BlastNaExtendWordExit(BLAST_ExtendWord* ewp, Int4 subject_length)
{
   BLAST_DiagTable* diag_table;
   Int4 diag_array_length, i;

   if (!ewp)
      return -1;

   diag_table = ewp->diag_table;
      
   if (diag_table->offset >= INT4_MAX/2) {
      diag_array_length = diag_table->diag_array_length;
      for(i=0;i<diag_array_length;i++) {
	 diag_table->diag_array[i].diag_level=0;
	 diag_table->diag_array[i].last_hit = -diag_table->window;
      }
   }
      
   if (diag_table->offset < INT4_MAX/2)	{
      diag_table->offset += subject_length + diag_table->window;
   } else {
      diag_table->offset = 0;
   }

   return 0;
}

/** Update the MegaBlast word extension structure after scanning of each 
 *  subject sequence.
 * @param ewp The structure holding word extension information [in] [out]
 * @param subject_length The length of the subject sequence that has just been
 *        processed [in]
 */
static Int2 MB_ExtendWordExit(BLAST_ExtendWord* ewp, Int4 subject_length)
{
   if (!ewp)
      return -1;

   if (ewp->diag_table) {
      return BlastNaExtendWordExit(ewp, subject_length);
   } else { 
      memset(ewp->stack_table->stack_index, 0, 
             ewp->stack_table->num_stacks*sizeof(Int4));
   }

   return 0;
}

/** Pack 4 sequence bytes into a one byte integer, assuming sequence contains
 *  no ambiguities.
 */
#define	PACK_WORD(q) ((q[0]<<6) + (q[1]<<4) + (q[2]<<2) + q[3]) 

/** Compare a given number of bytes of an compressed subject sequence with
 *  the non-compressed query sequence.
 * @param q Pointer to the first byte to be compared in the query sequence [in]
 * @param s Pointer to the first byte to be compared in the subject 
 *        sequence [in]
 * @param extra_bytes Number of compressed bytes to compare [in]
 * @return TRUE if sequences are identical, FALSE if mismatch is found.
 */
static NCBI_INLINE Boolean BlastNaCompareExtraBytes(Uint1* q, Uint1* s, 
                                               Int4 extra_bytes)
{
   Int4 index;
   
   for (index = 0; index < extra_bytes; ++index) {
      if (*s++ != PACK_WORD(q))
	 return FALSE;
      q += COMPRESSION_RATIO;
   }
   return TRUE;
}

/** Perform mini extension (up to max_left <= 4 bases) to the left; 
 * @param q Pointer to the query base right after the ones to be extended [in]
 * @param s Pointer to a byte in the compressed subject sequence that is to be 
 *        tested for extension [in]
 * @param max_left Maximal number of bits to compare [in]
 * @return Number of matched bases
*/
static NCBI_INLINE Uint1 
BlastNaMiniExtendLeft(Uint1* q, const Uint1* s, Uint1 max_left)
{
   Uint1 left = 0;

   for (left = 0; left < max_left; ++left) {
      if (READDB_UNPACK_BASE_N(*s, left) != *--q) {
	 break;
      }
   }
   return left;
}

/** Perform mini extension (up to max_right <= 4 bases) to the right; 
 * @param q Pointer to the start of the extension in the query [in]
 * @param s Pointer to a byte in the compressed subject sequence that is to be 
 *        tested for extension [in]
 * @param max_right Maximal number of bits to compare [in]
 * @return Number of matched bases
*/
static NCBI_INLINE Uint1 
BlastNaMiniExtendRight(Uint1* q, const Uint1* s, Uint1 max_right)
{
   Uint1 right;
   Uint1 index = 3;
   
   for (right = 0; right < max_right; ++right, --index) {
      if (READDB_UNPACK_BASE_N(*s, index) != *q++) {
	 break;
      }
   }
   return right;
}

/* Description in blast_extend.h */
Boolean
BlastnWordUngappedExtend(BLAST_SequenceBlk* query, 
   BLAST_SequenceBlk* subject, Int4** matrix, 
   Int4 q_off, Int4 s_off, Int4 cutoff, Int4 X, 
   BlastUngappedData** ungapped_data)
{
	Uint1* q;
	Int4 sum, score;
	Uint1 ch;
	Uint1* subject0,* sf,* q_beg,* q_end,* s_end,* s,* start;
	Int2 remainder, base;
	Int4 q_avail, s_avail;

   base = 3 - (s_off % 4);

	subject0 = subject->sequence;
   q_avail = query->length - q_off;
   s_avail = subject->length - s_off;

	q = q_beg = q_end = query->sequence + q_off;
	s = s_end = subject0 + s_off/COMPRESSION_RATIO;
	if (q_off < s_off) {
      start = subject0 + (s_off-q_off)/COMPRESSION_RATIO;
      remainder = 3 - ((s_off-q_off)%COMPRESSION_RATIO);
	} else {
      start = subject0;
      remainder = 3;
	}

	/* Find where positive scoring starts & ends within the word hit */
	score = 0;
   sum = 0;

	/* extend to the left */
	do {
      if (base == 3) {
         s--;
         base = 0;
      } else
         base++;
      ch = *s;
      if ((sum += matrix[*--q][READDB_UNPACK_BASE_N(ch, base)]) > 0) {
         q_beg = q;
         score += sum;
         sum = 0;
      } else if (sum < X)
         break;
	} while ((s > start) || (s == start && base < remainder));
   
   if (score >= cutoff && !ungapped_data) 
      return FALSE;
   
	if (ungapped_data) {
      *ungapped_data = (BlastUngappedData*) 
         malloc(sizeof(BlastUngappedData));
	   (*ungapped_data)->q_start = q_beg - query->sequence;
	   (*ungapped_data)->s_start = 
	      s_off - (q_off - (*ungapped_data)->q_start);
	}

   if (q_avail < s_avail) {
      sf = subject0 + (s_off + q_avail)/COMPRESSION_RATIO;
      remainder = 3 - ((s_off + q_avail)%COMPRESSION_RATIO);
   } else {
      sf = subject0 + (subject->length)/COMPRESSION_RATIO;
      remainder = 3 - ((subject->length)%COMPRESSION_RATIO);
   }
	/* extend to the right */
	q = q_end;
	s = s_end;
	sum = 0;
   base = 3 - (s_off % COMPRESSION_RATIO);

	while (s < sf || (s == sf && base > remainder)) {
      ch = *s;
      if ((sum += matrix[*q++][READDB_UNPACK_BASE_N(ch, base)]) > 0) {
         q_end = q;
         score += sum;
         sum = 0;
      } else if (sum < X)
         break;
      if (base == 0) {
         base = 3;
         s++;
      } else
         base--;
	}
   
	if (ungapped_data) {
	   (*ungapped_data)->length = q_end - q_beg;
	   (*ungapped_data)->score = score;
      (*ungapped_data)->frame = 0;
	}

   return (score < cutoff);
}

/** Perform ungapped extension given an offset pair, and save the initial 
 * hit information if the hit qualifies. This function assumes that the
 * exact match has already been extended to the word size parameter.
 * @param query The query sequence [in]
 * @param subject The subject sequence [in]
 * @param lookup Lookup table structure [in]
 * @param word_params The parameters related to initial word extension [in]
 * @param matrix the substitution matrix for ungapped extension [in]
 * @param ewp The structure containing word extension information [in]
 * @param q_off The offset in the query sequence [in]
 * @param s_end The offset in the subject sequence where this hit ends [in]
 * @param s_off The offset in the subject sequence [in]
 * @param init_hitlist The structure containing information about all 
 *                     initial hits [in] [out]
 */
static Int2
BlastnExtendInitialHit(BLAST_SequenceBlk* query, 
   BLAST_SequenceBlk* subject, LookupTableWrap* lookup,
   BlastInitialWordParameters* word_params, 
   Int4** matrix, BLAST_ExtendWord* ewp, Int4 q_off, Int4 s_end,
   Int4 s_off, BlastInitHitList* init_hitlist)
{
   Int4 diag, real_diag;
   Int4 s_pos, last_hit;
   BLAST_DiagTable*     diag_table;
   BlastUngappedData* ungapped_data;
   BlastInitialWordOptions* word_options = word_params->options;
   Int4 window_size = word_options->window_size;
   Boolean hit_ready;
   Boolean new_hit = FALSE, second_hit = FALSE;
   Int4 min_step, step;
   DiagStruct* diag_array_elem;
   Boolean do_ungapped_extension = word_options->ungapped_extension;

   diag_table = ewp->diag_table;

   diag = s_off + diag_table->diag_array_length - q_off;
   real_diag = diag & diag_table->diag_mask;
   diag_array_elem = &diag_table->diag_array[real_diag];
   s_pos = s_end + diag_table->offset;
   last_hit = diag_array_elem->last_hit;
   step = s_pos - last_hit;

   if (step <= 0)
      /* This is a hit on a diagonal that has already been explored 
         further down */
      return 0;

   min_step = 0;
   if (!do_ungapped_extension) {
      if (lookup->lut_type == MB_LOOKUP_TABLE) {
         MBLookupTable* lut = (MBLookupTable*)lookup->lut;
         if(!lut->discontiguous)
            min_step = lut->scan_step;
      } else {
         min_step = ((LookupTable*)lookup->lut)->scan_step;
      }
   }

   if (window_size == 0 ||
       (diag_array_elem->diag_level == 1)) {
      /* Single hit version or previous hit was already a second hit */
      new_hit = (step > min_step);
   } else {
      /* Previous hit was the first hit */
      new_hit = (step > window_size);
      second_hit = (step > 0 && step <= window_size);
   }

   hit_ready = ((window_size == 0) && new_hit) || second_hit;

   if (hit_ready) {
      if (word_options->ungapped_extension) {
         /* Perform ungapped extension */
         BlastnWordUngappedExtend(query, subject, matrix, q_off, s_off, 
            word_params->cutoff_score, -word_params->x_dropoff, 
            &ungapped_data);
      
         diag_array_elem->last_hit = 
            ungapped_data->length + ungapped_data->s_start 
            + diag_table->offset;
      } else {
         ungapped_data = NULL;
         diag_array_elem->last_hit = s_pos;
      }
      if (!ungapped_data || 
          ungapped_data->score >= word_params->cutoff_score) {
         BLAST_SaveInitialHit(init_hitlist, q_off, s_off, ungapped_data);
         /* Set diag_level to 1, indicating that this hit has already been 
            saved */
         diag_array_elem->diag_level = 1;
      } else {
         sfree(ungapped_data);
         /* Set diag_level to 0, indicating that any hit after this will 
            be new */
         diag_array_elem->diag_level = 0;
      }
   } else {
      /* First hit in the 2-hit case or a direct extension of the previous 
         hit - update the last hit information only */
      diag_array_elem->last_hit = s_pos;
      if (new_hit)
         diag_array_elem->diag_level = 0;
   }
   return 0;
}

/* Description in blast_extend.h */
Int4 BlastNaWordFinder(BLAST_SequenceBlk* subject, 
		       BLAST_SequenceBlk* query,
		       LookupTableWrap* lookup_wrap,
		       Int4** matrix,
		       BlastInitialWordParameters* word_params,
		       BLAST_ExtendWord* ewp,
		       Uint4* q_offsets,
		       Uint4* s_offsets,
		       Int4 max_hits,
		       BlastInitHitList* init_hitlist)
{
   LookupTable* lookup = (LookupTable*) lookup_wrap->lut;
   Uint1* s_start = subject->sequence;
   Uint1* abs_start = s_start;
   Int4 i;
   Uint1* s,* s_end;
   Uint1* q_start = query->sequence;
   Int4 hitsfound, total_hits = 0;
   Uint4 word_size, compressed_wordsize, reduced_word_length;
   Uint4 extra_bytes_needed;
   Uint2 extra_bases, left, right;
   Uint1* q;
   Int4 start_offset, last_start, next_start, last_end;
   Uint1 max_bases;

   word_size = COMPRESSION_RATIO*lookup->wordsize;
   last_start = subject->length - word_size;
   s_end = subject->sequence + subject->length/COMPRESSION_RATIO;
   start_offset = 0;

   compressed_wordsize = lookup->reduced_wordsize;
   extra_bytes_needed = lookup->wordsize - compressed_wordsize;
   reduced_word_length = COMPRESSION_RATIO*compressed_wordsize;
   extra_bases = lookup->word_length - word_size;
   last_end = subject->length - word_size;

   while (start_offset <= last_start) {
      /* Pass the last word ending offset */
      next_start = last_end;
      hitsfound = BlastNaScanSubject(lookup_wrap, subject, start_offset, 
                     q_offsets, s_offsets, max_hits, &next_start); 
      
      total_hits += hitsfound;
      for (i = 0; i < hitsfound; ++i) {
         /* Here it is guaranteed that subject offset is divisible by 4,
            because we only extend to the right, so scanning stride must be
            equal to 4. */
         s = abs_start + (s_offsets[i])/COMPRESSION_RATIO;
         q = q_start + q_offsets[i];
	    
         /* Check for extra bytes if required for longer words. */
         if (extra_bytes_needed && 
             !BlastNaCompareExtraBytes(q+reduced_word_length, 
                 s+compressed_wordsize, extra_bytes_needed))
            continue;
         /* mini extension to the left */
         max_bases = 
            MIN(COMPRESSION_RATIO, MIN(q_offsets[i], s_offsets[i]));
         left = BlastNaMiniExtendLeft(q, s-1, max_bases);

         /* mini extension to the right */
         max_bases =
            MIN(COMPRESSION_RATIO, 
                MIN(subject->length - s_offsets[i] - lookup->wordsize,
                    query->length - q_offsets[i] - word_size));

         if (max_bases > 0) {
            s += lookup->wordsize;
            q += word_size;
            right = BlastNaMiniExtendRight(q, s, max_bases);
         }

         if (left + right >= extra_bases) {
            /* Check if this diagonal has already been explored. */
            BlastnExtendInitialHit(query, subject, lookup_wrap, 
               word_params, matrix, ewp, q_offsets[i], 
               s_offsets[i] + word_size + right, 
               s_offsets[i], init_hitlist);
         }
      }
      start_offset = next_start;
   }

   BlastNaExtendWordExit(ewp, subject->length);
   return total_hits;
} 

/** Extend an exact match in both directions up to the provided 
 * maximal length. 
 * @param q_start Pointer to the start of the extension in query [in]
 * @param s_start Pointer to the start of the extension in subject [in]
 * @param max_bases_left At most how many bases to extend to the left [in]
 * @param max_bases_right At most how many bases to extend to the right [in]
 * @param max_length The length of the required exact match [in]
 * @param extend_partial_byte Should partial byte extension be perfomed?[in]
 * @param extended_right How far has extension succeeded to the right? [out]
 * @return TRUE if extension successful 
 */
static Boolean 
BlastNaExactMatchExtend(Uint1* q_start, Uint1* s_start, 
   Uint4 max_bases_left, Uint4 max_bases_right, Uint4 max_length, 
   Boolean extend_partial_byte, Uint4* extended_right)
{
   Uint4 length;
   Uint1* q,* s;
   
   *extended_right = 0;

   length = 0;

   /* Extend to the right; start from the firstt byte (it must be the 
      first one that's guaranteed to match by the lookup table hit). */

   q = q_start;
   s = s_start;
   while (length < max_length && max_bases_right >= COMPRESSION_RATIO) {
      if (*s != PACK_WORD(q))
         break;
      length += COMPRESSION_RATIO;
      ++s;
      q += COMPRESSION_RATIO;
      max_bases_right -= COMPRESSION_RATIO;
   }
   if (extend_partial_byte) {
      if (max_bases_right > 0) {
         length += BlastNaMiniExtendRight(q, s, 
                      (Uint1) MIN(max_bases_right, COMPRESSION_RATIO));
      }
   }

   *extended_right = length;

   if (length >= max_length)
      return TRUE;

   if (max_bases_left < max_length - length)
      return FALSE;
   else
      max_bases_left = max_length - length;

   /* Extend to the left; start with the byte just before the first. */
   q = q_start - COMPRESSION_RATIO;
   s = s_start - 1;
   while (length < max_length && max_bases_left >= COMPRESSION_RATIO) {
      if (*s != PACK_WORD(q))
         break;
      length += COMPRESSION_RATIO;
      --s;
      q -= COMPRESSION_RATIO;
      max_bases_left -= COMPRESSION_RATIO;
   }
   if (length >= max_length)
      return TRUE;
   if (extend_partial_byte && max_bases_left > 0) {
      length += BlastNaMiniExtendLeft(q+COMPRESSION_RATIO, s, 
                   (Uint1) MIN(max_bases_left, COMPRESSION_RATIO));
   }

   return (length >= max_length);
}

/* Description in blast_extend.h */
Int4 MB_WordFinder(BLAST_SequenceBlk* subject,
		   BLAST_SequenceBlk* query,
		   LookupTableWrap* lookup,
		   Int4** matrix, 
		   BlastInitialWordParameters* word_params,
		   BLAST_ExtendWord* ewp,
		   Uint4* q_offsets,
		   Uint4* s_offsets,
		   Int4 max_hits,
		   BlastInitHitList* init_hitlist)
{
   BlastInitialWordOptions* word_options = word_params->options;
   /* Pointer to the beginning of the first word of the subject sequence */
   MBLookupTable* mb_lt = (MBLookupTable*) lookup->lut;
   Uint1* s_start,* q_start,* q,* s;
   Int4 hitsfound=0;
   Int4 hit_counter=0, index;
   Int4 start_offset, next_start, last_start, last_end;
   Uint4 word_length;
   Uint4 max_bases_left, max_bases_right;
   Int4 query_length = query->length;
   Int4 subject_length = subject->length;
   Boolean ag_blast, variable_wordsize;
   Uint4 extended_right;
   Uint4 q_off, s_off; 
   Uint1 remainder;

   s_start = subject->sequence;
   q_start = query->sequence;
   word_length = mb_lt->word_length;
   ag_blast = (Boolean) (word_options->extension_method == eRightAndLeft);
   variable_wordsize = (Boolean) word_options->variable_wordsize;

   start_offset = 0;
   if (mb_lt->discontiguous) {
      last_start = subject_length - mb_lt->template_length;
      last_end = last_start + mb_lt->word_length;
   } else {
      last_end = subject_length;
      if (ag_blast) {
         last_start = 
            last_end - COMPRESSION_RATIO*mb_lt->compressed_wordsize;
      } else {
         last_start = last_end - mb_lt->word_length;
      }
   }

   /* start_offset points to the beginning of the word */
   while ( start_offset <= last_start ) {
      /* Set the last argument's value to the end of the last word,
         without the extra bases for the discontiguous case */
      next_start = last_end;
      if (mb_lt->discontiguous) {
         hitsfound = MB_DiscWordScanSubject(lookup, subject, start_offset,
            q_offsets, s_offsets, max_hits, &next_start);
      } else if (!ag_blast) {
         hitsfound = MB_ScanSubject(lookup, subject, start_offset, 
	    q_offsets, s_offsets, max_hits, &next_start);
      } else {
         hitsfound = MB_AG_ScanSubject(lookup, subject, start_offset, 
	    q_offsets, s_offsets, max_hits, &next_start);
      }
      if (ag_blast) {
         for (index = 0; index < hitsfound; ++index) {
            /* Adjust offsets to the start of the next full byte in the
               subject sequence */
            remainder = (-s_offsets[index])%COMPRESSION_RATIO;
            q_off = q_offsets[index] + remainder;
            s_off = s_offsets[index] + remainder;
            s = s_start + s_off/COMPRESSION_RATIO;
            q = q_start + q_off;
	    
            max_bases_left = 
               MIN(word_length, MIN(q_off, s_off));
            max_bases_right = MIN(word_length, 
               MIN(query_length-q_off, subject_length-s_off));
                                                
            if (BlastNaExactMatchExtend(q, s, max_bases_left, 
                   max_bases_right, word_length, !variable_wordsize, 
                   &extended_right))
            {
               /* Check if this diagonal has already been explored and save
                  the hit if needed. */
               BlastnExtendInitialHit(query, subject, lookup,
                  word_params, matrix, ewp, q_offsets[index], 
                  s_off + extended_right, s_offsets[index], 
                  init_hitlist);
            }
         }
      } else {
         for (index = 0; index < hitsfound; ++index) {
            MB_ExtendInitialHit(query, subject, lookup, word_params,
               matrix, ewp, q_offsets[index], s_offsets[index], init_hitlist);
         }
      }
      /* next_start returned from the ScanSubject points to the beginning
         of the word */
      start_offset = next_start;
      hit_counter += hitsfound;
   }

   MB_ExtendWordExit(ewp, subject_length);

   return hit_counter;
}

/* Description in blast_extend.h */
Int4 BlastNaWordFinder_AG(BLAST_SequenceBlk* subject, 
			  BLAST_SequenceBlk* query,
			  LookupTableWrap* lookup_wrap, 
			  Int4** matrix,
			  BlastInitialWordParameters* word_params,
			  BLAST_ExtendWord* ewp,
			  Uint4* q_offsets,
			  Uint4* s_offsets,
			  Int4 max_hits,
			  BlastInitHitList* init_hitlist)
{
   BlastInitialWordOptions* word_options = word_params->options;
   LookupTable* lookup = (LookupTable*) lookup_wrap->lut;
   Uint1* s_start = subject->sequence;
   Uint1* q_start = query->sequence;
   Int4 index;
   Uint1* q;
   Uint1* s;
   Int4 query_length = query->length;
   Int4 subject_length = subject->length;
   Int4 hitsfound, total_hits = 0;
   Uint4 word_length;
   Int4 start_offset, end_offset, next_start;
   Uint1 max_bases_left, max_bases_right;
   Boolean variable_wordsize = word_options->variable_wordsize;
   Uint4 extended_right;
   Uint4 q_off, s_off; /* Adjusted offsets */
   Uint1 remainder; 

   word_length = lookup->word_length;
   start_offset = 0;
   end_offset = subject_length - COMPRESSION_RATIO*lookup->reduced_wordsize;

   /* start_offset points to the beginning of the word; end_offset is the
      beginning of the last word */
   while (start_offset <= end_offset) {
      hitsfound = BlastNaScanSubject_AG(lookup_wrap, subject, start_offset, 
                     q_offsets, s_offsets, max_hits, &next_start); 
      
      total_hits += hitsfound;
      for (index = 0; index < hitsfound; ++index) {
         remainder = (-s_offsets[index])%COMPRESSION_RATIO;
         q_off = q_offsets[index] + remainder;
         s_off = s_offsets[index] + remainder;
         s = s_start + s_off/COMPRESSION_RATIO;
         q = q_start + q_off;
	    
         max_bases_left = 
            MIN(word_length, MIN(q_off, s_off));
         max_bases_right = MIN(word_length, 
            MIN(query_length-q_off, subject_length-s_off));
         
         if (BlastNaExactMatchExtend(q, s, max_bases_left, max_bases_right,
                word_length, !variable_wordsize, &extended_right)) 
         {
            /* Check if this diagonal has already been explored. */
            BlastnExtendInitialHit(query, subject, lookup_wrap, 
               word_params, matrix, ewp, q_offsets[index], 
               s_off + extended_right, s_offsets[index], init_hitlist);
         }
      }
      start_offset = next_start;
   }

   BlastNaExtendWordExit(ewp, subject_length);
   return total_hits;
} 

/** Deallocate memory for the diagonal table structure */
static BLAST_DiagTable* BlastDiagTableFree(BLAST_DiagTable* diag_table)
{
   if (diag_table) {
      sfree(diag_table->diag_array);
      sfree(diag_table);
   }
   return NULL;
}

/** Deallocate memory for the stack table structure */
static MB_StackTable* MBStackTableFree(MB_StackTable* stack_table)
{
   Int4 index;

   if (!stack_table)
      return NULL;

   for (index = 0; index < stack_table->num_stacks; ++index)
      sfree(stack_table->estack[index]);
   sfree(stack_table->estack);
   sfree(stack_table->stack_index);
   sfree(stack_table->stack_size);
   sfree(stack_table);
   return NULL;
}

BLAST_ExtendWord* BlastExtendWordFree(BLAST_ExtendWord* ewp)
{
   BlastDiagTableFree(ewp->diag_table);
   MBStackTableFree(ewp->stack_table);
   sfree(ewp);
   return NULL;
}

void 
BlastSaveInitHsp(BlastInitHitList* ungapped_hsps, Int4 q_start, Int4 s_start, 
   Int4 q_off, Int4 s_off, Int4 len, Int4 score)
{
  BlastUngappedData* ungapped_data = NULL;

  ungapped_data = (BlastUngappedData*) malloc(sizeof(BlastUngappedData));

  ungapped_data->q_start = q_start;
  ungapped_data->s_start = s_start;
  ungapped_data->length  = len;
  ungapped_data->score   = score;
  ungapped_data->frame   = 0;

  BLAST_SaveInitialHit(ungapped_hsps, q_off, s_off, ungapped_data);

  return;
}
