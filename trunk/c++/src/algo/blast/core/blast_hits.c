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

File name: blast_hits.c

Author: Ilya Dondoshansky

Contents: BLAST functions

Detailed Contents: 

        - BLAST functions for saving hits after the (preliminary) gapped 
          alignment

******************************************************************************
 * $Revision$
 * */

static char const rcsid[] = "$Id$";

#include <algo/blast/core/blast_options.h>
#include <algo/blast/core/blast_extend.h>
#include <algo/blast/core/blast_hits.h>
#include <algo/blast/core/blast_util.h>

void 
HSPListSetFrames(Uint1 program_number, BlastHSPList* hsp_list, 
                 Boolean is_ooframe)
{
   BlastHSP* hsp;
   Int4 index;

   if (!hsp_list)
      return;

   for (index=0; index<hsp_list->hspcnt; index++) {
      hsp = hsp_list->hsp_array[index];
      if (is_ooframe && program_number == blast_type_blastx) {
         /* Query offset is in mixed-frame coordinates */
         hsp->query.frame = hsp->query.offset % CODON_LENGTH + 1;
         if ((hsp->context % NUM_FRAMES) >= CODON_LENGTH)
            hsp->query.frame = -hsp->query.frame;
      } else {
         hsp->query.frame = BLAST_ContextToFrame(program_number, hsp->context);
      }
      
      /* Correct offsets in the edit block too */
      if (hsp->gap_info) {
         hsp->gap_info->frame1 = hsp->query.frame;
         hsp->gap_info->frame2 = hsp->subject.frame;
      }
   }
}

BlastHSP* BlastHSPFree(BlastHSP* hsp)
{
   if (!hsp)
      return NULL;
   hsp->gap_info = GapEditBlockDelete(hsp->gap_info);
   sfree(hsp);
   return NULL;
}

Int2 BLAST_GetNonSumStatsEvalue(Uint1 program, BlastQueryInfo* query_info,
        BlastHSPList* hsp_list, const BlastScoringOptions* score_options, 
        BlastScoreBlk* sbp)
{
   BlastHSP* hsp;
   BlastHSP** hsp_array;
   BLAST_KarlinBlk** kbp;
   Int4 hsp_cnt;
   Int4 index;
   
   if (hsp_list == NULL)
      return 1;
   
   if (score_options->gapped_calculation)
      kbp = sbp->kbp_gap_std;
   else
      kbp = sbp->kbp_std;
   
   hsp_cnt = hsp_list->hspcnt;
   hsp_array = hsp_list->hsp_array;
   for (index=0; index<hsp_cnt; index++) {
      hsp = hsp_array[index];

      ASSERT(hsp != NULL);

      /* Get effective search space from the score block, or from the 
         query information block, in order of preference */
      if (sbp->effective_search_sp) {
         hsp->evalue = BLAST_KarlinStoE_simple(hsp->score, kbp[hsp->context],
                          (double)sbp->effective_search_sp);
      } else {
         hsp->evalue = 
            BLAST_KarlinStoE_simple(hsp->score, kbp[hsp->context],
               (double)query_info->eff_searchsp_array[hsp->context]);
      }
   }
   
   return 0;
}

double PHIScoreToEvalue(Int4 score, BlastScoreBlk* sbp)
{
   double paramC = sbp->kbp[0]->paramC;
   double Lambda = sbp->kbp[0]->Lambda;
   Int8 pattern_space = sbp->effective_search_sp;
   
   return pattern_space*paramC*(1+Lambda*score)*exp(-Lambda*score);
}

void PHIGetEvalue(BlastHSPList* hsp_list, BlastScoreBlk* sbp)
{
   Int4 index;
   BlastHSP* hsp;

   for (index = 0; index < hsp_list->hspcnt; ++index) {
      hsp = hsp_list->hsp_array[index];
      hsp->evalue = PHIScoreToEvalue(hsp->score, sbp);
   }
}

Int2 BLAST_ReapHitlistByEvalue(BlastHSPList* hsp_list, 
        BlastHitSavingOptions* hit_options)
{
   BlastHSP* hsp;
   BlastHSP** hsp_array;
   Int4 hsp_cnt = 0;
   Int4 index;
   double cutoff;
   
   if (hsp_list == NULL)
      return 1;

   cutoff = hit_options->expect_value;

   hsp_array = hsp_list->hsp_array;
   for (index = 0; index < hsp_list->hspcnt; index++) {
      hsp = hsp_array[index];

      ASSERT(hsp != NULL);
      
      if (hsp->evalue > cutoff) {
         hsp_array[index] = BlastHSPFree(hsp_array[index]);
      } else {
         if (index > hsp_cnt)
            hsp_array[hsp_cnt] = hsp_array[index];
         hsp_cnt++;
      }
   }
      
   hsp_list->hspcnt = hsp_cnt;

   return 0;
}

/* See description in blast_hits.h */

Int4
BlastHSPArrayPurge (BlastHSP** hsp_array, Int4 hspcnt)

{
	Int4 index, index1;

	if (hspcnt == 0 || hsp_array == NULL)
		return 0;

	index1 = 0;
	for (index=0; index<hspcnt; index++)
	{
		if (hsp_array[index] != NULL)
		{
			hsp_array[index1] = hsp_array[index];
			index1++;
		}
	}

	for (index=index1; index<hspcnt; index++)
	{
		hsp_array[index] = NULL;
	}

	hspcnt = index1;

	return index1;
}

/** Calculate number of identities in an HSP.
 * @param query The query sequence [in]
 * @param subject The uncompressed subject sequence [in]
 * @param hsp All information about the HSP [in]
 * @param is_gapped Is this a gapped search? [in]
 * @param num_ident_ptr Number of identities [out]
 * @param align_length_ptr The alignment length, including gaps [out]
 */
static Int2
BlastHSPGetNumIdentical(Uint1* query, Uint1* subject, 
   BlastHSP* hsp, Boolean is_gapped, Int4* num_ident_ptr, 
   Int4* align_length_ptr)
{
   Int4 i, num_ident, align_length, q_off, s_off;
   Uint1* q,* s;
   GapEditBlock* gap_info;
   GapEditScript* esp;

   gap_info = hsp->gap_info;

   if (!gap_info && is_gapped)
      return -1;

   q_off = hsp->query.offset;
   s_off = hsp->subject.offset;

   if (!subject || !query)
      return -1;

   q = &query[q_off];
   s = &subject[s_off];

   num_ident = 0;
   align_length = 0;


   if (!gap_info) {
      /* Ungapped case */
      align_length = hsp->query.length; 
      for (i=0; i<align_length; i++) {
         if (*q++ == *s++)
            num_ident++;
      }
   } else {
      for (esp = gap_info->esp; esp; esp = esp->next) {
         align_length += esp->num;
         switch (esp->op_type) {
         case GAPALIGN_SUB:
            for (i=0; i<esp->num; i++) {
               if (*q++ == *s++)
                  num_ident++;
            }
            break;
         case GAPALIGN_DEL:
            s += esp->num;
            break;
         case GAPALIGN_INS:
            q += esp->num;
            break;
         default: 
            s += esp->num;
            q += esp->num;
            break;
         }
      }
   }

   *align_length_ptr = align_length;
   *num_ident_ptr = num_ident;
   return 0;
}

/** Comparison callback function for sorting HSPs by e-value */
static int
score_compare_hsps(const void* v1, const void* v2)
{
   BlastHSP* h1,* h2;
   
   h1 = *((BlastHSP**) v1);
   h2 = *((BlastHSP**) v2);

   /* No need to check e-values, because for the same subject sequence e-value
      is always inverse proportional to score. However e-values are less 
      sensitive, since they both can be 0, when scores are large but 
      different. */
   if (h1->score > h2->score)
      return -1;
   else if (h1->score < h2->score)
      return 1;
   /* Tie-breakers: decreasing subject offsets; decreasing subject ends, 
      decreasing query offsets, decreasing query ends */
   else if (h1->subject.offset > h2->subject.offset)
      return -1;
   else if (h1->subject.offset < h2->subject.offset)
      return 1;
   else if (h1->subject.end > h2->subject.end)
      return -1;
   else if (h1->subject.end < h2->subject.end)
      return 1;
   else if (h1->query.offset > h2->query.offset)
      return -1;
   else if (h1->query.offset < h2->query.offset)
      return 1;
   else if (h1->query.end > h2->query.end)
      return -1;
   else if (h1->query.end < h2->query.end)
      return 1;

   return 0;
}

/** Comparison callback function for sorting HSPs by diagonal and flagging
 * the HSPs contained in or identical to other HSPs for future deletion.
*/
static int
diag_uniq_compare_hsps(const void* v1, const void* v2)
{
   BlastHSP* h1,* h2;
   BlastHSP** hp1,** hp2;
   
   hp1 = (BlastHSP**) v1;
   hp2 = (BlastHSP**) v2;
   h1 = *hp1;
   h2 = *hp2;
   
   if (h1==NULL && h2==NULL) return 0;
   else if (h1==NULL) return 1;
   else if (h2==NULL) return -1;
   
   /* Separate different queries and/or strands */
   if (h1->context < h2->context)
      return -1;
   else if (h1->context > h2->context)
      return 1;
   
   /* Check if one HSP is contained in the other, if so, 
      leave only the longer one, given it has lower evalue */
   if (h1->query.offset >= h2->query.offset && 
       h1->query.end <= h2->query.end &&  
       h1->subject.offset >= h2->subject.offset && 
       h1->subject.end <= h2->subject.end && 
       h1->evalue >= h2->evalue) { 
      (*hp1)->score = 0;
   } else if (h1->query.offset <= h2->query.offset &&  
              h1->query.end >= h2->query.end &&  
              h1->subject.offset <= h2->subject.offset && 
              h1->subject.end >= h2->subject.end && 
              h1->evalue <= h2->evalue) { 
      (*hp2)->score = 0;
   }
   
   return (h1->query.offset - h1->subject.offset) -
      (h2->query.offset - h2->subject.offset);
}

static int
null_compare_hsps(const void* v1, const void* v2)
{
   BlastHSP* h1,* h2;
   
   h1 = *((BlastHSP**) v1);
   h2 = *((BlastHSP**) v2);
   
   if ((h1 && h2) || (!h1 && !h2))
      return 0;
   else if (!h1) return 1;
   else return -1;
}

/** Are the two HSPs within a given diagonal distance of each other? */
#define MB_HSP_CLOSE(q1, q2, s1, s2, c) (ABS(((q1)-(s1)) - ((q2)-(s2))) < (c))
#define MIN_DIAG_DIST 60

/** Sort the HSPs in an HSP list by diagonal and remove redundant HSPs */
static Int2
BlastSortUniqHspArray(BlastHSPList* hsp_list)
{
   Int4 index, new_hspcnt, index1, q_off, s_off, q_end, s_end, index2;
   BlastHSP** hsp_array = hsp_list->hsp_array;
   Boolean shift_needed = FALSE;
   Int4 context;
   double evalue;

   qsort(hsp_array, hsp_list->hspcnt, sizeof(BlastHSP*), 
         diag_uniq_compare_hsps);
   /* Delete all HSPs that were flagged in qsort */
   for (index = 0, new_hspcnt = 0; index < hsp_list->hspcnt; ++index) {
      if (hsp_array[index]->score == 0) {
         hsp_array[index] = BlastHSPFree(hsp_array[index]);
      }
   }      
   /* Move all nulled out HSPs to the end */
   qsort(hsp_array, hsp_list->hspcnt, sizeof(BlastHSP*),
         null_compare_hsps);

   for (index=1, new_hspcnt=0; index<hsp_list->hspcnt; index++) {
      if (!hsp_array[index])
         break;

      q_off = hsp_array[index]->query.offset;
      s_off = hsp_array[index]->subject.offset;
      q_end = hsp_array[index]->query.end;
      s_end = hsp_array[index]->subject.end;
      evalue = hsp_array[index]->evalue;
      context = hsp_array[index]->context;
      for (index1 = new_hspcnt; index1 >= 0 && 
           hsp_array[index1]->context == context && new_hspcnt-index1 < 10 && 
           MB_HSP_CLOSE(q_off, hsp_array[index1]->query.offset,
                        s_off, hsp_array[index1]->subject.offset, 
                        MIN_DIAG_DIST);
           index1--) {
         if (q_off >= hsp_array[index1]->query.offset && 
             s_off >= hsp_array[index1]->subject.offset && 
             q_end <= hsp_array[index1]->query.end && 
             s_end <= hsp_array[index1]->subject.end &&
             evalue >= hsp_array[index1]->evalue) {
            hsp_array[index] = BlastHSPFree(hsp_array[index]);
            break;
         } else if (q_off <= hsp_array[index1]->query.offset && 
             s_off <= hsp_array[index1]->subject.offset && 
             q_end >= hsp_array[index1]->query.end && 
             s_end >= hsp_array[index1]->subject.end &&
             evalue <= hsp_array[index1]->evalue) {
            hsp_array[index1] = BlastHSPFree(hsp_array[index1]);
            shift_needed = TRUE;
         }
      }
      
      if (shift_needed) {
         while (index1 >= 0 && !hsp_array[index1])
            index1--;
         for (index2 = ++index1; index1 <= new_hspcnt; index1++) {
            if (hsp_array[index1])
               hsp_array[index2++] = hsp_array[index1];
         }
         new_hspcnt = index2 - 1;
         shift_needed = FALSE;
      }
      if (hsp_array[index] != NULL)
         hsp_array[++new_hspcnt] = hsp_array[index];
   }
   /* Set all HSP pointers that will not be used any more to NULL */
   memset(&hsp_array[new_hspcnt+1], 0, 
	  (hsp_list->hspcnt - new_hspcnt - 1)*sizeof(BlastHSP*));
   hsp_list->hspcnt = new_hspcnt + 1;

   return 0;
}

Boolean ReevaluateHSPWithAmbiguities(BlastHSP* hsp, 
           Uint1* query_start, Uint1* subject_start, 
           const BlastHitSavingOptions* hit_options, 
           const BlastScoringOptions* score_options, 
           BlastQueryInfo* query_info, BlastScoreBlk* sbp)
{
   Int4 sum, score, gap_open, gap_extend;
   Int4** matrix;
   Uint1* query,* subject;
   Uint1* new_q_start,* new_s_start,* new_q_end,* new_s_end;
   Int4 index;
   Int2 factor = 1;
   Uint1 mask = 0x0f;
   GapEditScript* esp,* last_esp = NULL,* prev_esp,* first_esp = NULL;
   Boolean delete_hsp;
   double searchsp_eff;
   Int4 last_esp_num = 0;
   Int4 align_length;
   BLAST_KarlinBlk* kbp;
   Boolean gapped_calculation = score_options->gapped_calculation;

   /* NB: this function is called only for BLASTn, so we know where the 
      Karlin block is */
   kbp = sbp->kbp_std[hsp->context];
   searchsp_eff = (double)query_info->eff_searchsp_array[hsp->context];

   if (score_options->gap_open == 0 && score_options->gap_extend == 0) {
      if (score_options->reward % 2 == 1) 
         factor = 2;
      gap_open = 0;
      gap_extend = 
         (score_options->reward - 2*score_options->penalty) * factor / 2;
   } else {
      gap_open = score_options->gap_open;
      gap_extend = score_options->gap_extend;
   }

   matrix = sbp->matrix;

   query = query_start + hsp->query.offset; 
   subject = subject_start + hsp->subject.offset;
   score = 0;
   sum = 0;
   new_q_start = new_q_end = query;
   new_s_start = new_s_end = subject;
   index = 0;
   
   if (!gapped_calculation) {
      for (index = 0; index < hsp->subject.length; ++index) {
         sum += factor*matrix[*query & mask][*subject];
         query++;
         subject++;
         if (sum < 0) {
            if (BLAST_KarlinStoE_simple(score, kbp, searchsp_eff)
                > hit_options->expect_value) {
               /* Start from new offset */
               new_q_start = query;
               new_s_start = subject;
               score = sum = 0;
            } else {
               /* Stop here */
               break;
            }
         } else if (sum > score) {
            /* Remember this point as the best scoring end point */
            score = sum;
            new_q_end = query;
            new_s_end = subject;
         }
      }
   } else {
      esp = hsp->gap_info->esp;
      prev_esp = NULL;
      last_esp = first_esp = esp;
      last_esp_num = 0;
      
      while (esp) {
         if (esp->op_type == GAPALIGN_SUB) {
            sum += factor*matrix[*query & mask][*subject];
            query++;
            subject++;
            index++;
         } else if (esp->op_type == GAPALIGN_DEL) {
            sum -= gap_open + gap_extend * esp->num;
            subject += esp->num;
            index += esp->num;
         } else if (esp->op_type == GAPALIGN_INS) {
            sum -= gap_open + gap_extend * esp->num;
            query += esp->num;
            index += esp->num;
         }
         
         if (sum < 0) {
            if (BLAST_KarlinStoE_simple(score, kbp, searchsp_eff)
                > hit_options->expect_value) {
               /* Start from new offset */
               new_q_start = query;
               new_s_start = subject;
               score = sum = 0;
               if (index < esp->num) {
                  esp->num -= index;
                  first_esp = esp;
                  index = 0;
               } else {
                  first_esp = esp->next;
               }
               /* Unlink the bad part of the esp chain 
                  so it can be freed later */
               if (prev_esp)
                  prev_esp->next = NULL;
               last_esp = NULL;
            } else {
               /* Stop here */
               break;
            }
         } else if (sum > score) {
            /* Remember this point as the best scoring end point */
            score = sum;
            last_esp = esp;
            last_esp_num = index;
            new_q_end = query;
            new_s_end = subject;
         }
         if (index >= esp->num) {
            index = 0;
            prev_esp = esp;
            esp = esp->next;
         }
      } /* loop on edit scripts */
   } /* if (gapped_calculation) */

   score /= factor;
   
   delete_hsp = FALSE;
   hsp->score = score;
   hsp->evalue = 
      BLAST_KarlinStoE_simple(score, kbp, searchsp_eff);
   if (hsp->evalue > hit_options->expect_value) {
      delete_hsp = TRUE;
   } else {
      hsp->query.length = new_q_end - new_q_start;
      hsp->subject.length = new_s_end - new_s_start;
      hsp->query.offset = new_q_start - query_start;
      hsp->query.end = hsp->query.offset + hsp->query.length;
      hsp->subject.offset = new_s_start - subject_start;
      hsp->subject.end = hsp->subject.offset + hsp->subject.length;
      if (gapped_calculation) {
         /* Make corrections in edit block and free any parts that
            are no longer needed */
         if (first_esp != hsp->gap_info->esp) {
            GapEditScriptDelete(hsp->gap_info->esp);
            hsp->gap_info->esp = first_esp;
         }
         if (last_esp->next != NULL) {
            GapEditScriptDelete(last_esp->next);
            last_esp->next = NULL;
         }
         last_esp->num = last_esp_num;
      }
      BlastHSPGetNumIdentical(query_start, subject_start, hsp, 
         gapped_calculation, &hsp->num_ident, &align_length);
      /* Check if this HSP passes the percent identity test */
      if (((double)hsp->num_ident) / align_length * 100 < 
          hit_options->percent_identity)
         delete_hsp = TRUE;
   }
   
   if (delete_hsp) { /* This HSP is now below the cutoff */
      if (gapped_calculation && first_esp != NULL && 
          first_esp != hsp->gap_info->esp)
         GapEditScriptDelete(first_esp);
   }
  
   return delete_hsp;
} 

/** Reevaluate all HSPs in an HSP list, using ambiguity information. 
 * This is/can only done either for an ungapped search, or if traceback is 
 * already available.
 * Subject sequence is uncompressed and saved here. Number of identities is
 * calculated for each HSP along the way. 
 * @param hsp_list The list of HSPs for one subject sequence [in] [out]
 * @param query_blk The query sequence [in]
 * @param subject_blk The subject sequence [in] [out]
 * @param hit_options The options related to saving hits [in]
 * @param query_info Auxiliary query information [in]
 * @param sbp The statistical information [in]
 * @param score_options The scoring options [in]
 * @param bssp The BLAST database structure (for retrieving uncompressed
 *             sequence) [in]
 */
static Int2 
ReevaluateHSPListWithAmbiguities(BlastHSPList* hsp_list,
   BLAST_SequenceBlk* query_blk, BLAST_SequenceBlk* subject_blk, 
   const BlastHitSavingOptions* hit_options, BlastQueryInfo* query_info, 
   BlastScoreBlk* sbp, const BlastScoringOptions* score_options, 
   const BlastSeqSrc* bssp)
{
   BlastHSP** hsp_array,* hsp;
   Uint1* query_start,* subject_start = NULL;
   Int4 index, context, hspcnt;
   Boolean purge, delete_hsp;
   Int2 status = 0;
   GetSeqArg seq_arg;
   Boolean gapped_calculation = score_options->gapped_calculation;

   if (!hsp_list)
      return status;

   hspcnt = hsp_list->hspcnt;
   hsp_array = hsp_list->hsp_array;
   memset((void*) &seq_arg, 0, sizeof(seq_arg));

   /* In case of no traceback, return without doing anything */
   if (!hsp_list->traceback_done && gapped_calculation) {
      if (hsp_list->hspcnt > 1)
         status = BlastSortUniqHspArray(hsp_list);
      return status;
   }

   if (hsp_list->hspcnt == 0)
      /* All HSPs have been deleted */
      return status;

   /* Retrieve the unpacked subject sequence and save it in the 
      sequence_start element of the subject structure.
      NB: for the BLAST 2 Sequences case, the uncompressed sequence must 
      already be there */
   if (bssp) {
      seq_arg.oid = subject_blk->oid;
      seq_arg.encoding = BLASTNA_ENCODING;
      BLASTSeqSrcGetSequence(bssp, (void*) &seq_arg);

      subject_blk->sequence_start = seq_arg.seq->sequence_start;
      subject_blk->sequence = seq_arg.seq->sequence_start + 1;
   }
   /* The sequence in blastna encoding is now stored in sequence_start */
   subject_start = subject_blk->sequence_start + 1;

   purge = FALSE;
   for (index=0; index<hspcnt; index++) {
      if (hsp_array[index] == NULL)
         continue;
      else
         hsp = hsp_array[index];

      context = hsp->context;

      query_start = query_blk->sequence + query_info->context_offsets[context];

      delete_hsp = 
         ReevaluateHSPWithAmbiguities(hsp, query_start, subject_start,
            hit_options, score_options, query_info, sbp);
      
      if (delete_hsp) { /* This HSP is now below the cutoff */
         hsp_array[index] = BlastHSPFree(hsp_array[index]);
         purge = TRUE;
      }
   }
    
   if (purge) {
      index = BlastHSPArrayPurge(hsp_array, hspcnt);
      hsp_list->hspcnt = index;	
   }
   
   /* Check for HSP inclusion once more */
   if (hsp_list->hspcnt > 1)
      status = BlastSortUniqHspArray(hsp_list);

   BlastSequenceBlkFree(seq_arg.seq);
   subject_blk->sequence = NULL;

   return status;
}


/** This is a copy of a static function from ncbimisc.c.
 * Turns array into a heap with respect to a given comparison function.
 */
static void
heapify (char* base0, char* base, char* lim, char* last, size_t width, int (*compar )(const void*, const void* ))
{
   size_t i;
   char   ch;
   char* left_son,* large_son;
   
   left_son = base0 + 2*(base-base0) + width;
   while (base <= lim) {
      if (left_son == last)
         large_son = left_son;
      else
         large_son = (*compar)(left_son, left_son+width) >= 0 ?
            left_son : left_son+width;
      if ((*compar)(base, large_son) < 0) {
         for (i=0; i<width; ++i) {
            ch = base[i];
            base[i] = large_son[i];
            large_son[i] = ch;
         }
         base = large_son;
         left_son = base0 + 2*(base-base0) + width;
      } else
         break;
   }
}

/** The first part of qsort: create heap only, without sorting it
 * @param b An array [in] [out]
 * @param nel Number of elements in b [in]
 * @param width The size of each element [in]
 * @param compar Callback to compare two heap elements [in]
 */
static void CreateHeap (void* b, size_t nel, size_t width, 
   int (*compar )(const void*, const void* ))   
{
   char*    base = (char*)b;
   size_t i;
   char*    base0 = (char*)base,* lim,* basef;
   
   if (nel < 2)
      return;
   
   lim = &base[((nel-2)/2)*width];
   basef = &base[(nel-1)*width];
   i = nel/2;
   for (base = &base0[(i - 1)*width]; i > 0; base = base - width) {
      heapify(base0, base, lim, basef, width, compar);
      i--;
   }
}

/** Callback for sorting hsp lists by their best evalue/score;
 * It is assumed that the HSP arrays in each hit list are already sorted by 
 * e-value/score.
*/
static int
evalue_compare_hsp_lists(const void* v1, const void* v2)
{
   BlastHSPList* h1,* h2;
   
   h1 = *(BlastHSPList**) v1;
   h2 = *(BlastHSPList**) v2;
   
   /* If any of the HSP lists is empty, it is considered "worse" than the 
      other, unless the other is also empty. */
   if (h1->hspcnt == 0 && h2->hspcnt == 0)
      return 0;
   else if (h1->hspcnt == 0)
      return 1;
   else if (h2->hspcnt == 0)
      return -1;

   if (h1->hsp_array[0]->evalue < h2->hsp_array[0]->evalue)
      return -1;
   if (h1->hsp_array[0]->evalue > h2->hsp_array[0]->evalue)
      return 1;
   
   if (h1->hsp_array[0]->score > h2->hsp_array[0]->score)
      return -1;
   if (h1->hsp_array[0]->score < h2->hsp_array[0]->score)
      return 1;
   
   /* In case of equal best E-values and scores, order will be determined
      by ordinal ids of the subject sequences */
   if (h1->oid > h2->oid)
      return -1;
   if (h1->oid < h2->oid)
      return 1;

   return 0;
}

#define MIN_HSP_ARRAY_SIZE 100
BlastHSPList* BlastHSPListNew()
{
   BlastHSPList* hsp_list = (BlastHSPList*) calloc(1, sizeof(BlastHSPList));
   hsp_list->hsp_array = (BlastHSP**) 
      calloc(MIN_HSP_ARRAY_SIZE, sizeof(BlastHSP*));
   hsp_list->hsp_max = INT4_MAX;
   hsp_list->allocated = MIN_HSP_ARRAY_SIZE;

   return hsp_list;
}

BlastHSPList* BlastHSPListFree(BlastHSPList* hsp_list)
{
   Int4 index;

   if (!hsp_list)
      return hsp_list;

   for (index = 0; index < hsp_list->hspcnt; ++index) {
      BlastHSPFree(hsp_list->hsp_array[index]);
   }
   sfree(hsp_list->hsp_array);

   sfree(hsp_list);
   return NULL;
}

/** Given a BlastHitList* with a heapified HSP list array, remove
 *  the worst scoring HSP list and insert the new HSP list in the heap
 * @param hit_list Contains all HSP lists for a given query [in] [out]
 * @param hsp_list A new HSP list to be inserted into the hit list [in]
 */
static void InsertBlastHSPListInHeap(BlastHitList* hit_list, 
                                     BlastHSPList* hsp_list)
{
      BlastHSPListFree(hit_list->hsplist_array[0]);
      hit_list->hsplist_array[0] = hsp_list;
      if (hit_list->hsplist_count >= 2) {
         heapify((char*)hit_list->hsplist_array, (char*)hit_list->hsplist_array, 
                 (char*)&hit_list->hsplist_array[(hit_list->hsplist_count-1)/2],
                 (char*)&hit_list->hsplist_array[hit_list->hsplist_count-1],
                 sizeof(BlastHSPList*), evalue_compare_hsp_lists);
      }
      hit_list->worst_evalue = 
         hit_list->hsplist_array[0]->hsp_array[0]->evalue;
      hit_list->low_score = hit_list->hsplist_array[0]->hsp_array[0]->score;
}

/** Insert a new HSP list into the hit list.
 * Before capacity of the hit list is reached, just add to the end;
 * After that, store in a heap, to ensure efficient insertion and deletion.
 * The heap order is reverse, with worst e-value on top, for convenience
 * of deletion.
 * @param hit_list Contains all HSP lists saved so far [in] [out]
 * @param hsp_list A new HSP list to be inserted into the hit list [in]
*/
static Int2 BLAST_UpdateHitList(BlastHitList* hit_list, 
                                BlastHSPList* hsp_list)
{
   if (hit_list->hsplist_count < hit_list->hsplist_max) {
      /* If the array of HSP lists for this query is not yet allocated, 
         do it here */
      if (!hit_list->hsplist_array)
         hit_list->hsplist_array = (BlastHSPList**)
            malloc(hit_list->hsplist_max*sizeof(BlastHSPList*));
      /* Just add to the end; sort later */
      hit_list->hsplist_array[hit_list->hsplist_count++] = hsp_list;
      hit_list->worst_evalue = 
         MAX(hsp_list->hsp_array[0]->evalue, hit_list->worst_evalue);
      hit_list->low_score = 
         MIN(hsp_list->hsp_array[0]->score, hit_list->low_score);
   } else if ((hsp_list->hsp_array[0]->evalue > hit_list->worst_evalue) ||
              ((hsp_list->hsp_array[0]->evalue == hit_list->worst_evalue) &&
               (hsp_list->hsp_array[0]->score <= hit_list->low_score))) {
      /* This hit list is less significant than any of those already saved;
         discard it */
      BlastHSPListFree(hsp_list);
   } else {
      if (!hit_list->heapified) {
         CreateHeap(hit_list->hsplist_array, hit_list->hsplist_count, 
                    sizeof(BlastHSPList*), evalue_compare_hsp_lists);
         hit_list->heapified = TRUE;
      }
      InsertBlastHSPListInHeap(hit_list, hsp_list);
   }

   return 0;
}

/** Allocate memory for a hit list of a given size.
 * @param hitlist_size Size of the hit list (number of HSP lists) [in]
 */
static BlastHitList* BLAST_HitListNew(Int4 hitlist_size)
{
   BlastHitList* new_hitlist = (BlastHitList*) 
      calloc(1, sizeof(BlastHitList));
   new_hitlist->hsplist_max = hitlist_size;
   new_hitlist->low_score = INT4_MAX;
   return new_hitlist;
}

/** Deallocate memory for the hit list */
static BlastHitList* BLAST_HitListFree(BlastHitList* hitlist)
{
   Int4 index;

   if (!hitlist)
      return NULL;

   for (index = 0; index < hitlist->hsplist_count; ++index)
      BlastHSPListFree(hitlist->hsplist_array[index]);
   sfree(hitlist->hsplist_array);
   sfree(hitlist);
   return NULL;
}

static BlastHSPList* BlastHSPListDup(BlastHSPList* hsp_list)
{
   BlastHSPList* new_hsp_list = (BlastHSPList*) 
      BlastMemDup(hsp_list, sizeof(BlastHSPList));
   new_hsp_list->hsp_array = (BlastHSP**) 
      BlastMemDup(hsp_list->hsp_array, hsp_list->allocated*sizeof(BlastHSP*));
   new_hsp_list->allocated = hsp_list->allocated;

   return new_hsp_list;
}

Int2 BLAST_SaveHitlist(Uint1 program, BLAST_SequenceBlk* query,
        BLAST_SequenceBlk* subject, BlastHSPResults* results, 
        BlastHSPList* hsp_list, BlastHitSavingParameters* hit_parameters, 
        BlastQueryInfo* query_info, BlastScoreBlk* sbp, 
        const BlastScoringOptions* score_options, const BlastSeqSrc* bssp)
{
   Int2 status = 0;
   BlastHSPList** hsp_list_array;
   BlastHSP* hsp;
   Int4 index;
   BlastHitSavingOptions* hit_options = hit_parameters->options;
   Uint1 context_factor;

   if (!hsp_list)
      return 0;

   if (program == blast_type_blastn) {
      status = ReevaluateHSPListWithAmbiguities(hsp_list, query, subject, 
                  hit_options, query_info, sbp, score_options, bssp);
      context_factor = 2;
   } else if (program == blast_type_blastx || 
              program == blast_type_tblastx) {
      context_factor = 6;
   } else {
      context_factor = 1;
   }

   /* Sort the HSPs by e-value */
   if (hsp_list->hspcnt > 1) {
      qsort(hsp_list->hsp_array, hsp_list->hspcnt, sizeof(BlastHSP*), 
               score_compare_hsps);
   }

   /* Rearrange HSPs into multiple hit lists if more than one query */
   if (results->num_queries > 1) {
      BlastHSPList* tmp_hsp_list;
      hsp_list_array = calloc(results->num_queries, sizeof(BlastHSPList*));
      for (index = 0; index < hsp_list->hspcnt; index++) {
         hsp = hsp_list->hsp_array[index];
         tmp_hsp_list = hsp_list_array[hsp->context/context_factor];

         if (!tmp_hsp_list) {
            hsp_list_array[hsp->context/context_factor] = tmp_hsp_list = 
               BlastHSPListNew();
            tmp_hsp_list->oid = hsp_list->oid;
            tmp_hsp_list->traceback_done = hsp_list->traceback_done;
         }

         if (!tmp_hsp_list || tmp_hsp_list->do_not_reallocate) {
            tmp_hsp_list = NULL;
         } else if (tmp_hsp_list->hspcnt >= tmp_hsp_list->allocated) {
            BlastHSP** new_hsp_array;
            Int4 new_size = 
               MIN(2*tmp_hsp_list->allocated, tmp_hsp_list->hsp_max);
            if (new_size == tmp_hsp_list->hsp_max)
               tmp_hsp_list->do_not_reallocate = TRUE;
            
            new_hsp_array = realloc(tmp_hsp_list->hsp_array, 
                                    new_size*sizeof(BlastHSP*));
            if (!new_hsp_array) {
               tmp_hsp_list->do_not_reallocate = TRUE;
               tmp_hsp_list = NULL;
            } else {
               tmp_hsp_list->hsp_array = new_hsp_array;
               tmp_hsp_list->allocated = new_size;
            }
         }
         if (tmp_hsp_list) {
            tmp_hsp_list->hsp_array[tmp_hsp_list->hspcnt++] = hsp;
         } else {
            /* Cannot add more HSPs; free the memory */
            hsp_list->hsp_array[index] = BlastHSPFree(hsp);
         }
      }

      /* Insert the hit list(s) into the appropriate places in the results 
         structure */
      for (index = 0; index < results->num_queries; index++) {
         if (hsp_list_array[index]) {
            if (!results->hitlist_array[index]) {
               results->hitlist_array[index] = 
                  BLAST_HitListNew(hit_options->prelim_hitlist_size);
            }
            BLAST_UpdateHitList(results->hitlist_array[index], 
                                hsp_list_array[index]);
         }
      }
      sfree(hsp_list_array);
      /* All HSPs from the hsp_list structure are now moved to the results 
         structure, so set the HSP count back to 0 */
      hsp_list->hspcnt = 0;
      BlastHSPListFree(hsp_list);
   } else if (hsp_list->hspcnt > 0) {
      /* Single query; save the HSP list directly into the results 
         structure */
      if (!results->hitlist_array[0]) {
         results->hitlist_array[0] = 
            BLAST_HitListNew(hit_options->prelim_hitlist_size);
      }
      BLAST_UpdateHitList(results->hitlist_array[0], 
                          hsp_list);
   }
   
   return status; 
}

Int2 BLAST_ResultsInit(Int4 num_queries, BlastHSPResults** results_ptr)
{
   BlastHSPResults* results;
   Int2 status = 0;

   results = (BlastHSPResults*) malloc(sizeof(BlastHSPResults));

   results->num_queries = num_queries;
   results->hitlist_array = (BlastHitList**) 
      calloc(num_queries, sizeof(BlastHitList*));
   
   *results_ptr = results;
   return status;
}

BlastHSPResults* BLAST_ResultsFree(BlastHSPResults* results)
{
   Int4 index;

   if (!results)
       return NULL;

   for (index = 0; index < results->num_queries; ++index)
      BLAST_HitListFree(results->hitlist_array[index]);
   sfree(results->hitlist_array);
   sfree(results);
   return NULL;
}

static void BlastHitListPurge(BlastHitList* hit_list)
{
   Int4 index, hsplist_count;
   
   if (!hit_list)
      return;
   
   hsplist_count = hit_list->hsplist_count;
   for (index = 0; index < hsplist_count && 
           hit_list->hsplist_array[index]->hspcnt > 0; ++index);
   
   hit_list->hsplist_count = index;
   /* Free all empty HSP lists */
   for ( ; index < hsplist_count; ++index) {
      BlastHSPListFree(hit_list->hsplist_array[index]);
   }
}

Int2 BLAST_SortResults(BlastHSPResults* results)
{
   Int4 index;
   BlastHitList* hit_list;

   for (index = 0; index < results->num_queries; ++index) {
      hit_list = results->hitlist_array[index];
      if (hit_list && hit_list->hsplist_count > 1) {
         qsort(hit_list->hsplist_array, hit_list->hsplist_count, 
                  sizeof(BlastHSPList*), evalue_compare_hsp_lists);
      }
      BlastHitListPurge(hit_list);
   }
   return 0;
}

void AdjustOffsetsInHSPList(BlastHSPList* hsp_list, Int4 offset)
{
   Int4 index;
   BlastHSP* hsp;

   for (index=0; index<hsp_list->hspcnt; index++) {
      hsp = hsp_list->hsp_array[index];
      hsp->subject.offset += offset;
      hsp->subject.end += offset;
      hsp->subject.gapped_start += offset;
      if (hsp->gap_info)
         hsp->gap_info->start2 += offset;
   }
}

/** Comparison callback for sorting HSPs by diagonal */
static int
diag_compare_hsps(const void* v1, const void* v2)
{
   BlastHSP* h1,* h2;

   h1 = *((BlastHSP**) v1);
   h2 = *((BlastHSP**) v2);
   
   return (h1->query.offset - h1->subject.offset) - 
      (h2->query.offset - h2->subject.offset);
}

#define OVERLAP_DIAG_CLOSE 10
/** Merge the two HSPs if they intersect.
 * @param hsp1 The first HSP [in]
 * @param hsp2 The second HSP [in]
 * @param start The starting offset of the subject coordinates where the 
 *               intersection is possible [in]
*/
static Boolean
BLAST_MergeHsps(BlastHSP* hsp1, BlastHSP* hsp2, Int4 start)
{
   BLASTHSPSegment* segments1,* segments2,* new_segment1,* new_segment2;
   GapEditScript* esp1,* esp2,* esp;
   Int4 end = start + DBSEQ_CHUNK_OVERLAP - 1;
   Int4 min_diag, max_diag, num1, num2, dist = 0, next_dist = 0;
   Int4 diag1_start, diag1_end, diag2_start, diag2_end;
   Int4 index;
   Uint1 intersection_found;
   Uint1 op_type = 0;

   if (!hsp1->gap_info || !hsp2->gap_info) {
      /* Assume that this is an ungapped alignment, hence simply compare 
         diagonals. Do not merge if they are on different diagonals */
      if (diag_compare_hsps(&hsp1, &hsp2) == 0 &&
          hsp1->query.end >= hsp2->query.offset) {
         hsp1->query.end = hsp2->query.end;
         hsp1->subject.end = hsp2->subject.end;
         hsp1->query.length = hsp1->query.end - hsp1->query.offset;
         hsp1->subject.length = hsp1->subject.end - hsp1->subject.offset;
         return TRUE;
      } else
         return FALSE;
   }
   /* Find whether these HSPs have an intersection point */
   segments1 = (BLASTHSPSegment*) calloc(1, sizeof(BLASTHSPSegment));
   
   esp1 = hsp1->gap_info->esp;
   esp2 = hsp2->gap_info->esp;
   
   segments1->q_start = hsp1->query.offset;
   segments1->s_start = hsp1->subject.offset;
   while (segments1->s_start < start) {
      if (esp1->op_type == GAPALIGN_INS)
         segments1->q_start += esp1->num;
      else if (segments1->s_start + esp1->num < start) {
         if (esp1->op_type == GAPALIGN_SUB) { 
            segments1->s_start += esp1->num;
            segments1->q_start += esp1->num;
         } else if (esp1->op_type == GAPALIGN_DEL)
            segments1->s_start += esp1->num;
      } else 
         break;
      esp1 = esp1->next;
   }
   /* Current esp is the first segment within the overlap region */
   segments1->s_end = segments1->s_start + esp1->num - 1;
   if (esp1->op_type == GAPALIGN_SUB)
      segments1->q_end = segments1->q_start + esp1->num - 1;
   else
      segments1->q_end = segments1->q_start;
   
   new_segment1 = segments1;
   
   for (esp = esp1->next; esp; esp = esp->next) {
      new_segment1->next = (BLASTHSPSegment*)
         calloc(1, sizeof(BLASTHSPSegment));
      new_segment1->next->q_start = new_segment1->q_end + 1;
      new_segment1->next->s_start = new_segment1->s_end + 1;
      new_segment1 = new_segment1->next;
      if (esp->op_type == GAPALIGN_SUB) {
         new_segment1->q_end += esp->num - 1;
         new_segment1->s_end += esp->num - 1;
      } else if (esp->op_type == GAPALIGN_INS) {
         new_segment1->q_end += esp->num - 1;
         new_segment1->s_end = new_segment1->s_start;
      } else {
         new_segment1->s_end += esp->num - 1;
         new_segment1->q_end = new_segment1->q_start;
      }
   }
   
   /* Now create the second segments list */
   
   segments2 = (BLASTHSPSegment*) calloc(1, sizeof(BLASTHSPSegment));
   segments2->q_start = hsp2->query.offset;
   segments2->s_start = hsp2->subject.offset;
   segments2->q_end = segments2->q_start + esp2->num - 1;
   segments2->s_end = segments2->s_start + esp2->num - 1;
   
   new_segment2 = segments2;
   
   for (esp = esp2->next; esp && new_segment2->s_end < end; 
        esp = esp->next) {
      new_segment2->next = (BLASTHSPSegment*)
         calloc(1, sizeof(BLASTHSPSegment));
      new_segment2->next->q_start = new_segment2->q_end + 1;
      new_segment2->next->s_start = new_segment2->s_end + 1;
      new_segment2 = new_segment2->next;
      if (esp->op_type == GAPALIGN_INS) {
         new_segment2->s_end = new_segment2->s_start;
         new_segment2->q_end = new_segment2->q_start + esp->num - 1;
      } else if (esp->op_type == GAPALIGN_DEL) {
         new_segment2->s_end = new_segment2->s_start + esp->num - 1;
         new_segment2->q_end = new_segment2->q_start;
      } else if (esp->op_type == GAPALIGN_SUB) {
         new_segment2->s_end = new_segment2->s_start + esp->num - 1;
         new_segment2->q_end = new_segment2->q_start + esp->num - 1;
      }
   }
   
   new_segment1 = segments1;
   new_segment2 = segments2;
   intersection_found = 0;
   num1 = num2 = 0;
   while (new_segment1 && new_segment2 && !intersection_found) {
      if (new_segment1->s_end < new_segment2->s_start || 
          new_segment1->q_end < new_segment2->q_start) {
         new_segment1 = new_segment1->next;
         num1++;
         continue;
      }
      if (new_segment2->s_end < new_segment1->s_start || 
          new_segment2->q_end < new_segment1->q_start) {
         new_segment2 = new_segment2->next;
         num2++;
         continue;
      }
      diag1_start = new_segment1->s_start - new_segment1->q_start;
      diag2_start = new_segment2->s_start - new_segment2->q_start;
      diag1_end = new_segment1->s_end - new_segment1->q_end;
      diag2_end = new_segment2->s_end - new_segment2->q_end;
      
      if (diag1_start == diag1_end && diag2_start == diag2_end &&  
          diag1_start == diag2_start) {
         /* Both segments substitutions, on same diagonal */
         intersection_found = 1;
         dist = new_segment2->s_end - new_segment1->s_start + 1;
         break;
      } else if (diag1_start != diag1_end && diag2_start != diag2_end) {
         /* Both segments gaps - must intersect */
         intersection_found = 3;

         dist = new_segment2->s_end - new_segment1->s_start + 1;
         op_type = GAPALIGN_INS;
         next_dist = new_segment2->q_end - new_segment1->q_start - dist + 1;
         if (new_segment2->q_end - new_segment1->q_start < dist) {
            dist = new_segment2->q_end - new_segment1->q_start + 1;
            op_type = GAPALIGN_DEL;
            next_dist = new_segment2->s_end - new_segment1->s_start - dist + 1;
         }
         break;
      } else if (diag1_start != diag1_end) {
         max_diag = MAX(diag1_start, diag1_end);
         min_diag = MIN(diag1_start, diag1_end);
         if (diag2_start >= min_diag && diag2_start <= max_diag) {
            intersection_found = 2;
            dist = diag2_start - min_diag + 1;
            if (new_segment1->s_end == new_segment1->s_start)
               next_dist = new_segment2->s_end - new_segment1->s_end + 1;
            else
               next_dist = new_segment2->q_end - new_segment1->q_end + 1;
            break;
         }
      } else if (diag2_start != diag2_end) {
         max_diag = MAX(diag2_start, diag2_end);
         min_diag = MIN(diag2_start, diag2_end);
         if (diag1_start >= min_diag && diag1_start <= max_diag) {
            intersection_found = 2;
            next_dist = max_diag - diag1_start + 1;
            if (new_segment2->s_end == new_segment2->s_start)
               dist = new_segment2->s_start - new_segment1->s_start + 1;
            else
               dist = new_segment2->q_start - new_segment1->q_start + 1;
            break;
         }
      }
      if (new_segment1->s_end <= new_segment2->s_end) {
         new_segment1 = new_segment1->next;
         num1++;
      } else {
         new_segment2 = new_segment2->next;
         num2++;
      }
   }

   if (intersection_found) {
      esp = NULL;
      for (index = 0; index < num1-1; index++)
         esp1 = esp1->next;
      for (index = 0; index < num2-1; index++) {
         esp = esp2;
         esp2 = esp2->next;
      }
      if (intersection_found < 3) {
         if (num1 > 0)
            esp1 = esp1->next;
         if (num2 > 0) {
            esp = esp2;
            esp2 = esp2->next;
         }
      }
      switch (intersection_found) {
      case 1:
         esp1->num = dist;
         esp1->next = esp2->next;
         esp2->next = NULL;
         break;
      case 2:
         esp1->num = dist;
         esp2->num = next_dist;
         esp1->next = esp2;
         if (esp)
            esp->next = NULL;
         break;
      case 3:
         esp1->num += dist;
         esp2->op_type = op_type;
         esp2->num = next_dist;
         esp1->next = esp2;
         if (esp)
            esp->next = NULL;
         break;
      default: break;
      }
      hsp1->query.end = hsp2->query.end;
      hsp1->subject.end = hsp2->subject.end;
      hsp1->query.length = hsp1->query.end - hsp1->query.offset;
      hsp1->subject.length = hsp1->subject.end - hsp1->subject.offset;
   }

   return (Boolean) intersection_found;
}

/** TRUE if c is between a and b; f between d and f. Determines if the
 * coordinates are already in an HSP that has been evaluated. 
 */
#define CONTAINED_IN_HSP(a,b,c,d,e,f) (((a <= c && b >= c) && (d <= f && e >= f)) ? TRUE : FALSE)

/** Checks if the first HSP is contained in the second. */
static Boolean BLASTHspContained(BlastHSP* hsp1, BlastHSP* hsp2)
{
   Boolean hsp_start_is_contained=FALSE, hsp_end_is_contained=FALSE;

   if (hsp1->score > hsp2->score || 
       SIGN(hsp2->query.frame) != SIGN(hsp1->query.frame) || 
       SIGN(hsp2->subject.frame) != SIGN(hsp1->subject.frame))
      return FALSE;

   if (CONTAINED_IN_HSP(hsp2->query.offset, hsp2->query.end, hsp1->query.offset, hsp2->subject.offset, hsp2->subject.end, hsp1->subject.offset) == TRUE) {
      hsp_start_is_contained = TRUE;
   }
   if (CONTAINED_IN_HSP(hsp2->query.offset, hsp2->query.end, hsp1->query.end, hsp2->subject.offset, hsp2->subject.end, hsp1->subject.end) == TRUE) {
      hsp_end_is_contained = TRUE;
   }
   
   return (hsp_start_is_contained && hsp_end_is_contained);
}


/** Combine two HSP lists, without altering the individual HSPs, and without 
 * reallocating the HSP array. 
 * @param hsp_list New HSP list [in]
 * @param combined_hsp_list Old HSP list, to which new HSPs are added [in] [out]
 * @param new_hspcnt How many HSPs to save in the combined list? The extra ones
 *                   are freed. The best scoring HSPs are saved. This argument
 *                   cannot be greater than the allocated size of the 
 *                   combined list's HSP array. [in]
 */
static void 
CombineHSPListsByScore(BlastHSPList* hsp_list, BlastHSPList* combined_hsp_list,
                       Int4 new_hspcnt)
{
   Int4 index, index1, index2;
   BlastHSP** new_hsp_array;

   ASSERT(new_hspcnt <= combined_hsp_list->allocated);

   if (new_hspcnt >= hsp_list->hspcnt + combined_hsp_list->hspcnt) {
      /* All HSPs from both arrays are saved */
      for (index=combined_hsp_list->hspcnt, index1=0; 
           index1<hsp_list->hspcnt; index1++) {
         if (hsp_list->hsp_array[index1] != NULL)
            combined_hsp_list->hsp_array[index++] = hsp_list->hsp_array[index1];
      }
   } else {
      /* Not all HSPs are be saved; sort both arrays by score and save only
         the new_hspcnt best ones. 
         For the merged set of HSPs, allocate array the same size as in the 
         old HSP list. */
      new_hsp_array = (BlastHSP**) 
         malloc(combined_hsp_list->allocated*sizeof(BlastHSP*));
      qsort(combined_hsp_list->hsp_array, combined_hsp_list->hspcnt, 
           sizeof(BlastHSP*), score_compare_hsps);
      qsort(hsp_list->hsp_array, hsp_list->hspcnt, sizeof(BlastHSP*),
            score_compare_hsps);
      index1 = index2 = 0;
      for (index = 0; index < combined_hsp_list->allocated; ++index) {
         if (index1 < combined_hsp_list->hspcnt &&
             (index2 >= hsp_list->hspcnt ||
             (combined_hsp_list->hsp_array[index1]->score >= 
             hsp_list->hsp_array[index2]->score))) {
            new_hsp_array[index] = combined_hsp_list->hsp_array[index1];
            ++index1;
         } else {
            new_hsp_array[index] = hsp_list->hsp_array[index2];
            ++index2;
         }
      }
      /* Free the extra HSPs that could not be saved */
      for ( ; index1 < combined_hsp_list->hspcnt; ++index1) {
         combined_hsp_list->hsp_array[index1] = 
            BlastHSPFree(combined_hsp_list->hsp_array[index1]);
      }
      for ( ; index2 < hsp_list->hspcnt; ++index2) {
         hsp_list->hsp_array[index2] = 
            BlastHSPFree(hsp_list->hsp_array[index2]);
      }
      /* Point combined_hsp_list's HSP array to the new one */
      sfree(combined_hsp_list->hsp_array);
      combined_hsp_list->hsp_array = new_hsp_array;
   }
   
   combined_hsp_list->hspcnt = index;
   /* Second HSP list now does not own any HSPs */
   hsp_list->hspcnt = 0;
}

Int2 AppendHSPList(BlastHSPList* hsp_list,
        BlastHSPList** combined_hsp_list_ptr, Int4 hsp_num_max)
{
   BlastHSPList* combined_hsp_list = *combined_hsp_list_ptr;
   BlastHSP** new_hsp_array;
   Int4 new_hspcnt;

   if (!hsp_list || hsp_list->hspcnt == 0)
      return 0;

   /* If no previous HSP list, just return a copy of the new one. */
   if (!combined_hsp_list) {
      *combined_hsp_list_ptr = BlastHSPListDup(hsp_list);
      hsp_list->hspcnt = 0;
      return 0;
   }
   /* Just append new list to the end of the old list, in case of 
      multiple frames of the subject sequence */
   new_hspcnt = MIN(combined_hsp_list->hspcnt + hsp_list->hspcnt, 
                    hsp_num_max);
   if (new_hspcnt > combined_hsp_list->allocated && 
       !combined_hsp_list->do_not_reallocate) {
      Int4 new_allocated = MIN(2*new_hspcnt, hsp_num_max);
      new_hsp_array = (BlastHSP**) 
         realloc(combined_hsp_list->hsp_array, 
                 new_allocated*sizeof(BlastHSP*));
      
      if (new_hsp_array) {
         combined_hsp_list->allocated = new_allocated;
         combined_hsp_list->hsp_array = new_hsp_array;
      } else {
         combined_hsp_list->do_not_reallocate = TRUE;
         new_hspcnt = combined_hsp_list->allocated;
      }
   }
   if (combined_hsp_list->allocated == hsp_num_max)
      combined_hsp_list->do_not_reallocate = TRUE;

   CombineHSPListsByScore(hsp_list, combined_hsp_list, new_hspcnt);
   return 0;
}

Int2 MergeHSPLists(BlastHSPList* hsp_list, 
                   BlastHSPList** combined_hsp_list_ptr,
                   Int4 hsp_num_max, Int4 start, Boolean merge_hsps)
{
   BlastHSPList* combined_hsp_list = *combined_hsp_list_ptr;
   BlastHSP* hsp,** hspp1,** hspp2;
   Int4 index, index1;
   Int4 hspcnt1, hspcnt2, new_hspcnt = 0;
   BlastHSP** new_hsp_array;
   Int4* index_array1, *index_array2;
   
   if (!hsp_list || hsp_list->hspcnt == 0)
      return 0;

   /* If no previous HSP list, just return a copy of the new one. */
   if (!combined_hsp_list) {
      *combined_hsp_list_ptr = BlastHSPListDup(hsp_list);
      hsp_list->hspcnt = 0;
      return 0;
   }

   /* Merge the two HSP lists for successive chunks of the subject sequence */
   hspcnt1 = hspcnt2 = 0;
   hspp1 = (BlastHSP**) 
      calloc(combined_hsp_list->hspcnt, sizeof(BlastHSP*));
   hspp2 = (BlastHSP**) calloc(hsp_list->hspcnt, sizeof(BlastHSP*));
   index_array1 = (Int4*) calloc(combined_hsp_list->hspcnt, sizeof(Int4));
   index_array2 = (Int4*) calloc(hsp_list->hspcnt, sizeof(Int4));

   for (index=0; index<combined_hsp_list->hspcnt; index++) {
      hsp = combined_hsp_list->hsp_array[index];
      if (hsp->subject.end > start) {
         index_array1[hspcnt1] = index;
         hspp1[hspcnt1++] = hsp;
      } else
         new_hspcnt++;
   }
   for (index=0; index<hsp_list->hspcnt; index++) {
      hsp = hsp_list->hsp_array[index];
      if (hsp->subject.offset < start + DBSEQ_CHUNK_OVERLAP) {
         index_array2[hspcnt2] = index;
         hspp2[hspcnt2++] = hsp;
      } else
         new_hspcnt++;
   }

   qsort(hspp1, hspcnt1, sizeof(BlastHSP*), diag_compare_hsps);
   qsort(hspp2, hspcnt2, sizeof(BlastHSP*), diag_compare_hsps);

   for (index=0; index<hspcnt1; index++) {
      for (index1=0; index1<hspcnt2; index1++) {
         if (hspp2[index1] && 
             hspp2[index1]->query.frame == hspp1[index]->query.frame &&
             hspp2[index1]->subject.frame == hspp1[index]->subject.frame &&
             ABS(diag_compare_hsps(&hspp1[index], &hspp2[index1])) < 
             OVERLAP_DIAG_CLOSE) {
            if (merge_hsps) {
               if (BLAST_MergeHsps(hspp1[index], hspp2[index1], start)) {
                  /* Point the corresponding element of the full first HSP
                     array to the new HSP */
                  combined_hsp_list->hsp_array[index_array1[index]] = hspp1[index];
                  /* Free the corresponding element of the full second 
                     HSP array. */
                  hsp_list->hsp_array[index_array2[index1]] = 
                     hspp2[index1] = BlastHSPFree(hspp2[index1]);
                  break;
               }
            } else { /* No gap information available */
               if (BLASTHspContained(hspp1[index], hspp2[index1])) {
                  sfree(hspp1[index]);
                  /* Point the corresponding element of the full first HSP
                     array to the new HSP; free the element of the second
                     array. */
                  combined_hsp_list->hsp_array[index_array1[index]] = hspp2[index1];
                  hsp_list->hsp_array[index_array2[index1]] = 
                     hspp2[index1] = NULL;
               } else if (BLASTHspContained(hspp2[index1], hspp1[index])) {
                  /* Just free the corresponding element of the second 
                     HSP array */
                  hsp_list->hsp_array[index_array2[index1]] = 
                     hspp2[index1] = BlastHSPFree(hspp2[index1]);
               }
            }
         }
      }
   }

   new_hspcnt += hspcnt1;
   for (index=0; index<hspcnt2; index++) {
      if (hspp2[index] != NULL)
         new_hspcnt++;
   }
   
   sfree(hspp1);
   sfree(hspp2);
   sfree(index_array1);
   sfree(index_array2);
   
   /* If there is a restriction on the number of HSPs to keep, the new number of
      HSPs might have to be reduced */
   new_hspcnt = MIN(new_hspcnt, hsp_num_max);

   if (new_hspcnt >= combined_hsp_list->allocated-1 && 
       combined_hsp_list->do_not_reallocate == FALSE) {
      Int4 new_allocated = MIN(2*new_hspcnt, hsp_num_max);
      if (new_allocated > combined_hsp_list->allocated) {
         new_hsp_array = (BlastHSP**) 
            realloc(combined_hsp_list->hsp_array, 
                    new_allocated*sizeof(BlastHSP*));
         if (new_hsp_array == NULL) {
            combined_hsp_list->do_not_reallocate = TRUE; 
         } else {
            combined_hsp_list->hsp_array = new_hsp_array;
            combined_hsp_list->allocated = new_allocated;
         }
      } else {
         combined_hsp_list->do_not_reallocate = TRUE;
      }
      new_hspcnt = MIN(new_hspcnt, combined_hsp_list->allocated);
   }

   CombineHSPListsByScore(hsp_list, combined_hsp_list, new_hspcnt);

   return 0;
}

void RPSUpdateResults(BlastHSPResults *final_result,
                      BlastHSPResults *init_result)
{
   Int4 i, j, k;
   BlastHitList **hitlist;
   BlastHSPList **hsplist;
   BlastHSP **hsp;

   /* Allocate the (one) hitlist of the final result,
      if it hasn't been allocated already */

   if (!final_result->hitlist_array[0])
      final_result->hitlist_array[0] = BLAST_HitListNew(500);

   /* For each DB sequence... */

   hitlist = init_result->hitlist_array;
   for (i = 0; i < init_result->num_queries; i++) {

       if (hitlist[i] == NULL) 
           continue;

       /* DB sequence i has HSPLists; for each HSPList 
          (corresponding to a distinct DB sequence)... */

       hsplist = hitlist[i]->hsplist_array;
       for (j = 0; j < hitlist[i]->hsplist_count; j++) {

           /* Change the OID to make this HSPList correspond to
              a distinct DB sequence */
           
           hsplist[j]->oid = i;

           /* For each HSP in list j, there are no distinct 
              contexts/frames anymore */

           hsp = hsplist[j]->hsp_array;
           for (k = 0; k < hsplist[j]->hspcnt; k++)
               hsp[k]->context = 0;

           /* Add the modified HSPList to the new set of results */

           if (hsplist[j]->hspcnt)
               BLAST_UpdateHitList(final_result->hitlist_array[0], 
                                   hsplist[j]);
           else
               hsplist[j] = BlastHSPListFree(hsplist[j]);
       }

       /* Remove the original hit list (all of its contents
          have been moved) */

       hitlist[i]->hsplist_count = 0;
       init_result->hitlist_array[i] = BLAST_HitListFree(hitlist[i]);
   }
}
