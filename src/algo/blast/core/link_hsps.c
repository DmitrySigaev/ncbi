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

/** @file link_hsps.c
 * Functions to link with use of sum statistics
 */

static char const rcsid[] = 
    "$Id$";

#include <algo/blast/core/link_hsps.h>
#include <algo/blast/core/blast_util.h>


/** Methods used to "order" the HSP's. */
#define BLAST_NUMBER_OF_ORDERING_METHODS 2

typedef enum LinkOrderingMethod {
   BLAST_SMALL_GAPS = 0,
   BLAST_LARGE_GAPS
} LinkOrderingMethod;

/* Forward declaration */
struct LinkHSPStruct;

/** The following structure is used in "link_hsps" to decide between
 * two different "gapping" models.  Here link is used to hook up
 * a chain of HSP's, num is the number of links, and sum is the sum score.
 * Once the best gapping model has been found, this information is
 * transferred up to the LinkHSPStruct.  This structure should not be
 * used outside of the function Blast_EvenGapLinkHSPs.
 */
typedef struct BlastHSPLink {
   struct LinkHSPStruct* link[BLAST_NUMBER_OF_ORDERING_METHODS]; /**< Best 
                                               choice of HSP to link with */
   Int2 num[BLAST_NUMBER_OF_ORDERING_METHODS]; /**< number of HSP in the
                                                  ordering. */
   Int4 sum[BLAST_NUMBER_OF_ORDERING_METHODS]; /**< Sum-Score of HSP. */
   double xsum[BLAST_NUMBER_OF_ORDERING_METHODS]; /**< Sum-Score of HSP,
                                     multiplied by the appropriate Lambda. */
   Int4 changed; /**< Has the link been changed since previous access? */
} BlastHSPLink;

/** Structure containing all information internal to the process of linking
 * HSPs.
 */
typedef struct LinkHSPStruct {
   BlastHSP* hsp;      /**< Specific HSP this structure corresponds to */
   struct LinkHSPStruct* prev;         /**< Previous HSP in a set, if any */
   struct LinkHSPStruct* next;         /**< Next HSP in a set, if any */ 
   BlastHSPLink  hsp_link; /**< Auxiliary structure for keeping track of sum
                              scores, etc. */
   Boolean linked_set;     /**< Is this HSp part of a linked set? */
   Boolean start_of_chain; /**< If TRUE, this HSP starts a chain along the
                              "link" pointer. */
   Int4 linked_to;         /**< Where this HSP is linked to? */
   Int4 sumscore;          /**< Sumscore of a set of "linked" HSP's. */
   Int2 ordering_method;   /**< Which method (max or no max for gaps) was 
                              used for linking HSPs? */
   Int4 q_offset_trim;     /**< Start of trimmed hsp in query */
   Int4 q_end_trim;        /**< End of trimmed HSP in query */
   Int4 s_offset_trim;     /**< Start of trimmed hsp in subject */
   Int4 s_end_trim;        /**< End of trimmed HSP in subject */
} LinkHSPStruct;

/** Calculates e-value of a set of HSPs with sum statistics.
 * @param program_number Type of BLAST program [in]
 * @param sbp Scoring block with statistical parameters [in]
 * @param query_info Query information structure [in]
 * @param subject_length Subject sequence length [in]
 * @param link_hsp_params Parameters for linking HSPs [in]
 * @param head_hsp Set of HSPs with previously calculated sum score/evalue [in]
 * @param hsp New HSP candidate to join the set [in]
 * @param sum_score Score of all HSPs together [out]
 * @return E-value of the all HSPs together
 */
static double 
SumHSPEvalue(EBlastProgramType program_number, BlastScoreBlk* sbp, 
   BlastQueryInfo* query_info, Int4 subject_length, 
   const BlastLinkHSPParameters* link_hsp_params, 
   LinkHSPStruct* head_hsp, LinkHSPStruct* new_hsp, Int4* sumscore)
{
   double gap_prob, gap_decay_rate, sum_evalue, score_prime;
   Int2 num;
   Int4 subject_eff_length, query_eff_length, length_adjustment;
   Int4 context = head_hsp->hsp->context;
   double eff_searchsp;
   Blast_KarlinBlk* kbp = NULL;
   Int4 query_window_size;
   Int4 subject_window_size;

   gap_prob = link_hsp_params->gap_prob;
   gap_decay_rate = link_hsp_params->gap_decay_rate;

   num = head_hsp->hsp->num + new_hsp->hsp->num;

   length_adjustment = query_info->length_adjustments[context];

   subject_eff_length = 
      MAX((subject_length - length_adjustment), 1);
   if (program_number == eBlastTypeTblastn) 
      subject_eff_length /= 3;
   subject_eff_length = MAX(subject_eff_length, 1);
	
   query_eff_length = 
      MAX(BLAST_GetQueryLength(query_info, context) - length_adjustment, 1);
   
   *sumscore = MAX(new_hsp->hsp->score, new_hsp->sumscore) + 
      MAX(head_hsp->hsp->score, head_hsp->sumscore);

   /* Choose the appropriate Karlin block. */
   if (sbp->kbp_gap != NULL) 
      kbp = sbp->kbp_gap[context];
   else
      kbp = sbp->kbp[context];

   score_prime = (*sumscore) * kbp->Lambda;
   
   query_window_size = 
      link_hsp_params->overlap_size + link_hsp_params->gap_size + 1;
   subject_window_size = 
      link_hsp_params->overlap_size + link_hsp_params->longest_intron + 1;

   sum_evalue = 
       BLAST_UnevenGapSumE(kbp, query_window_size, subject_window_size,
          num, score_prime, query_eff_length, subject_eff_length,
          BLAST_GapDecayDivisor(gap_decay_rate, num));

   eff_searchsp = ((double) subject_eff_length) * query_eff_length;
   
   sum_evalue *= 
      ((double) query_info->eff_searchsp_array[context]) / eff_searchsp;

   return sum_evalue;
}

/** Sort the HSP's by starting position of the query.  Called by qsort.  
 *	The first function sorts in forward, the second in reverse order.
*/
static int
fwd_compare_hsps(const void* v1, const void* v2)
{
	BlastHSP* h1,* h2;
	LinkHSPStruct** hp1,** hp2;

	hp1 = (LinkHSPStruct**) v1;
	hp2 = (LinkHSPStruct**) v2;
	h1 = (*hp1)->hsp;
	h2 = (*hp2)->hsp;

	if (h1->context < h2->context)
      return -1;
   else if (h1->context > h2->context)
      return 1;

	if (h1->query.offset < h2->query.offset) 
		return -1;
	if (h1->query.offset > h2->query.offset) 
		return 1;
	/* Necessary in case both HSP's have the same query offset. */
	if (h1->subject.offset < h2->subject.offset) 
		return -1;
	if (h1->subject.offset > h2->subject.offset) 
		return 1;

	return 0;
}

/** Sort the HSP's by starting position of the query.  Called by qsort.  
 *	The first function sorts in forward, the second in reverse order.
*/
static int
fwd_compare_hsps_transl(const void* v1, const void* v2)
{
	BlastHSP* h1,* h2;
	LinkHSPStruct** hp1,** hp2;
   Int4 context1, context2;

	hp1 = (LinkHSPStruct**) v1;
	hp2 = (LinkHSPStruct**) v2;
	h1 = (*hp1)->hsp;
	h2 = (*hp2)->hsp;

   context1 = h1->context/3;
   context2 = h2->context/3;

   if (context1 < context2)
      return -1;
   else if (context1 > context2)
      return 1;

	if (h1->query.offset < h2->query.offset) 
		return -1;
	if (h1->query.offset > h2->query.offset) 
		return 1;
	/* Necessary in case both HSP's have the same query offset. */
	if (h1->subject.offset < h2->subject.offset) 
		return -1;
	if (h1->subject.offset > h2->subject.offset) 
		return 1;

	return 0;
}

/* Comparison function based on end position in the query */
static int
end_compare_hsps(const void* v1, const void* v2)
{
	BlastHSP* h1,* h2;
	LinkHSPStruct** hp1,** hp2;

	hp1 = (LinkHSPStruct**) v1;
	hp2 = (LinkHSPStruct**) v2;
	h1 = (*hp1)->hsp;
	h2 = (*hp2)->hsp;

   if (h1->context < h2->context)
      return -1;
   else if (h1->context > h2->context)
      return 1;

	if (h1->query.end < h2->query.end) 
		return -1;
	if (h1->query.end > h2->query.end) 
		return 1;
	/* Necessary in case both HSP's have the same query end. */
	if (h1->subject.end < h2->subject.end) 
		return -1;
	if (h1->subject.end > h2->subject.end) 
		return 1;

	return 0;
}

static int
sumscore_compare_hsps(const void* v1, const void* v2)
{
	LinkHSPStruct* h1,* h2;
	LinkHSPStruct** hp1,** hp2;
   Int4 score1, score2;

	hp1 = (LinkHSPStruct**) v1;
	hp2 = (LinkHSPStruct**) v2;
	h1 = *hp1;
	h2 = *hp2;

	if (h1 == NULL || h2 == NULL)
		return 0;

   score1 = MAX(h1->sumscore, h1->hsp->score);
   score2 = MAX(h2->sumscore, h2->hsp->score);

	if (score1 < score2) 
		return 1;
	if (score1 > score2)
		return -1;

	return 0;
}

/* Find an HSP with offset closest, but not smaller/larger than a given one.
 */
static Int4 hsp_binary_search(LinkHSPStruct** hsp_array, Int4 size, 
                              Int4 offset, Boolean right)
{
   Int4 index, begin, end, coord;
   
   begin = 0;
   end = size;
   while (begin < end) {
      index = (begin + end) / 2;
      if (right) 
         coord = hsp_array[index]->hsp->query.offset;
      else
         coord = hsp_array[index]->hsp->query.end;
      if (coord >= offset) 
         end = index;
      else
         begin = index + 1;
   }

   return end;
}

static int
rev_compare_hsps(const void *v1, const void *v2)

{
	BlastHSP* h1,* h2;
	LinkHSPStruct** hp1,** hp2;

	hp1 = (LinkHSPStruct**) v1;
	hp2 = (LinkHSPStruct**) v2;
	h1 = (*hp1)->hsp;
	h2 = (*hp2)->hsp;
	
   if (h1->context < h2->context)
      return -1;
   else if (h1->context > h2->context)
      return 1;

	if (h1->query.offset < h2->query.offset) 
		return  1;
	if (h1->query.offset > h2->query.offset) 
		return -1;
	/* Necessary in case both HSP's have the same query offset. */
	if (h1->subject.offset < h2->subject.offset) 
		return 1;
	if (h1->subject.offset > h2->subject.offset) 
		return -1;
	return 0;
}

static int
rev_compare_hsps_transl(const void *v1, const void *v2)

{
	BlastHSP* h1,* h2;
	LinkHSPStruct** hp1,** hp2;
   Int4 context1, context2;

	hp1 = (LinkHSPStruct**) v1;
	hp2 = (LinkHSPStruct**) v2;
	h1 = (*hp1)->hsp;
	h2 = (*hp2)->hsp;
	
   context1 = h1->context/3;
   context2 = h2->context/3;

   if (context1 < context2)
      return -1;
   else if (context1 > context2)
      return 1;

	if (h1->query.offset < h2->query.offset) 
		return  1;
	if (h1->query.offset > h2->query.offset) 
		return -1;
	/* Necessary in case both HSP's have the same query offset. */
	if (h1->subject.offset < h2->subject.offset) 
		return 1;
	if (h1->subject.offset > h2->subject.offset) 
		return -1;
	return 0;
}

static int
rev_compare_hsps_tbn(const void *v1, const void *v2)

{
	BlastHSP* h1,* h2;
	LinkHSPStruct** hp1,** hp2;

	hp1 = (LinkHSPStruct**) v1;
	hp2 = (LinkHSPStruct**) v2;
	h1 = (*hp1)->hsp;
	h2 = (*hp2)->hsp;

   if (h1->context < h2->context)
      return -1;
   else if (h1->context > h2->context)
      return 1;

	if (SIGN(h1->subject.frame) != SIGN(h2->subject.frame))
	{
		if (h1->subject.frame > h2->subject.frame)
			return 1;
		else
			return -1;
	}

	if (h1->query.offset < h2->query.offset) 
		return  1;
	if (h1->query.offset > h2->query.offset) 
		return -1;
	if (h1->subject.offset < h2->subject.offset) 
		return  1;
	if (h1->subject.offset > h2->subject.offset) 
		return -1;
	return 0;
}

static int
rev_compare_hsps_tbx(const void *v1, const void *v2)

{
	BlastHSP* h1,* h2;
	LinkHSPStruct** hp1,** hp2;
   Int4 context1, context2;

	hp1 = (LinkHSPStruct**) v1;
	hp2 = (LinkHSPStruct**) v2;
	h1 = (*hp1)->hsp;
	h2 = (*hp2)->hsp;

   context1 = h1->context/3;
   context2 = h2->context/3;

   if (context1 < context2)
      return -1;
   else if (context1 > context2)
      return 1;
   
	if (SIGN(h1->subject.frame) != SIGN(h2->subject.frame))
	{
		if (h1->subject.frame > h2->subject.frame)
			return 1;
		else
			return -1;
	}

	if (h1->query.offset < h2->query.offset) 
		return  1;
	if (h1->query.offset > h2->query.offset) 
		return -1;
	if (h1->subject.offset < h2->subject.offset) 
		return  1;
	if (h1->subject.offset > h2->subject.offset) 
		return -1;
	return 0;
}

/** The helper array contains the info used frequently in the inner 
 * for loops of the HSP linking algorithm.
 * One array of helpers will be allocated for each thread.
 */
typedef struct LinkHelpStruct {
  LinkHSPStruct* ptr;
  Int4 q_off_trim;
  Int4 s_off_trim;
  Int4 sum[BLAST_NUMBER_OF_ORDERING_METHODS];
  Int4 maxsum1;
  Int4 next_larger;
} LinkHelpStruct;

static LinkHSPStruct* LinkHSPStructReset(LinkHSPStruct* lhsp)
{
   BlastHSP* hsp;

   if (!lhsp) {
      lhsp = (LinkHSPStruct*) calloc(1, sizeof(LinkHSPStruct));
      lhsp->hsp = (BlastHSP*) calloc(1, sizeof(BlastHSP));
   } else {
      if (!lhsp->hsp) {
         hsp = (BlastHSP*) calloc(1, sizeof(BlastHSP));
      } else {
         hsp = lhsp->hsp;
         memset(hsp, 0, sizeof(BlastHSP));
      }
      memset(lhsp, 0, sizeof(LinkHSPStruct));
      lhsp->hsp = hsp;
   }
   return lhsp;
}

static Int2
Blast_EvenGapLinkHSPs(EBlastProgramType program_number, BlastHSPList* hsp_list, 
   BlastQueryInfo* query_info, Int4 subject_length,
   BlastScoreBlk* sbp, const BlastLinkHSPParameters* link_hsp_params,
   Boolean gapped_calculation)
{
	LinkHSPStruct* H,* H2,* best[2],* first_hsp,* last_hsp,** hp_frame_start;
	LinkHSPStruct* hp_start = NULL;
   BlastHSP* hsp;
   BlastHSP** hsp_array;
	Blast_KarlinBlk** kbp;
	Int4 maxscore, cutoff[2];
	Boolean linked_set, ignore_small_gaps;
	double gap_decay_rate, gap_prob, prob[2];
	Int4 index, index1, num_links, frame_index;
   LinkOrderingMethod ordering_method;
   Int4 num_query_frames, num_subject_frames;
	Int4 *hp_frame_number;
	Int4 window_size, trim_size;
   Int4 number_of_hsps, total_number_of_hsps;
   Int4 query_length, length_adjustment;
   Int4 subject_length_orig = subject_length;
	LinkHSPStruct* link;
	Int4 H2_index,H_index;
	Int4 i;
 	Int4 path_changed;  /* will be set if an element is removed that may change an existing path */
 	Int4 first_pass, use_current_max; 
	LinkHelpStruct *lh_helper=0;
   Int4 lh_helper_size;
	Int4 query_context; /* AM: to support query concatenation. */
   Boolean translated_query;
   LinkHSPStruct** link_hsp_array;

	if (hsp_list == NULL)
		return -1;

   hsp_array = hsp_list->hsp_array;

   lh_helper_size = MAX(1024,hsp_list->hspcnt+5);
   lh_helper = (LinkHelpStruct *) 
      calloc(lh_helper_size, sizeof(LinkHelpStruct));

	if (gapped_calculation) 
		kbp = sbp->kbp_gap;
	else
		kbp = sbp->kbp;

	total_number_of_hsps = hsp_list->hspcnt;

	number_of_hsps = total_number_of_hsps;

   /* For convenience, include overlap size into the gap size */
	window_size = link_hsp_params->gap_size + link_hsp_params->overlap_size + 1;
   trim_size = (link_hsp_params->overlap_size + 1) / 2;
	gap_prob = link_hsp_params->gap_prob;
	gap_decay_rate = link_hsp_params->gap_decay_rate;

	translated_query = (program_number == eBlastTypeBlastx || 
                       program_number == eBlastTypeTblastx);
   if (program_number == eBlastTypeTblastn ||
       program_number == eBlastTypeTblastx)
      num_subject_frames = NUM_STRANDS;
   else
      num_subject_frames = 1;

   link_hsp_array = 
      (LinkHSPStruct**) malloc(total_number_of_hsps*sizeof(LinkHSPStruct*));
   for (index = 0; index < total_number_of_hsps; ++index) {
      link_hsp_array[index] = (LinkHSPStruct*) calloc(1, sizeof(LinkHSPStruct));
      link_hsp_array[index]->hsp = hsp_array[index];
   }

   /* Sort by (reverse) position. */
   if (translated_query) {
      qsort(link_hsp_array,total_number_of_hsps,sizeof(LinkHSPStruct*), 
            rev_compare_hsps_tbx);
   } else {
      qsort(link_hsp_array,total_number_of_hsps,sizeof(LinkHSPStruct*), 
            rev_compare_hsps_tbn);
   }

   cutoff[0] = link_hsp_params->cutoff_small_gap;
   cutoff[1] = link_hsp_params->cutoff_big_gap;
   
   ignore_small_gaps = (cutoff[0] == 0);
   
   /* If query is nucleotide, it has 2 strands that should be separated. */
   if (translated_query || program_number == eBlastTypeBlastn)
      num_query_frames = NUM_STRANDS*query_info->num_queries;
   else
      num_query_frames = query_info->num_queries;
   
   hp_frame_start = 
       calloc(num_subject_frames*num_query_frames, sizeof(LinkHSPStruct*));
   hp_frame_number = calloc(num_subject_frames*num_query_frames, sizeof(Int4));

   /* hook up the HSP's */
   hp_frame_start[0] = link_hsp_array[0];

   /* Put entries from different strands into separate 'query_frame's. */
   {
      Int4 cur_frame=0;
      Int4 strand_factor = (translated_query ? 3 : 1);
      for (index=0;index<number_of_hsps;index++) 
      {
        H=link_hsp_array[index];
        H->start_of_chain = FALSE;
        hp_frame_number[cur_frame]++;

        H->prev= index ? link_hsp_array[index-1] : NULL;
        H->next= index<(number_of_hsps-1) ? link_hsp_array[index+1] : NULL;
        if (H->prev != NULL && 
            ((H->hsp->context/strand_factor) != 
	     (H->prev->hsp->context/strand_factor) ||
             (SIGN(H->hsp->subject.frame) != SIGN(H->prev->hsp->subject.frame))))
        { /* If frame switches, then start new list. */
           hp_frame_number[cur_frame]--;
           hp_frame_number[++cur_frame]++;
           hp_frame_start[cur_frame] = H;
           H->prev->next = NULL;
           H->prev = NULL;
        }
      }
      num_query_frames = cur_frame+1;
   }

   /* trim_size is the maximum amount q.offset can differ from 
      q.offset_trim */
   /* This is used to break out of H2 loop early */
   for (index=0;index<number_of_hsps;index++) 
   {
      H = link_hsp_array[index];
		hsp = H->hsp;
		H->q_offset_trim = hsp->query.offset + 
         MIN(((hsp->query.length)/4), trim_size);
		H->q_end_trim = hsp->query.end - 
         MIN(((hsp->query.length)/4), trim_size);
		H->s_offset_trim = 
         hsp->subject.offset + 
         MIN(((hsp->subject.length)/4), trim_size);
		H->s_end_trim = 
         hsp->subject.end - 
         MIN(((hsp->subject.length)/4), trim_size);
   }	    

	for (frame_index=0; frame_index<num_query_frames; frame_index++)
	{
      hp_start = LinkHSPStructReset(hp_start);
      hp_start->next = hp_frame_start[frame_index];
      hp_frame_start[frame_index]->prev = hp_start;
      number_of_hsps = hp_frame_number[frame_index];
      query_context = hp_start->next->hsp->context;
      length_adjustment = query_info->length_adjustments[query_context];
      query_length = BLAST_GetQueryLength(query_info, query_context);
      query_length = MAX(query_length - length_adjustment, 1);
      subject_length = subject_length_orig; /* in nucleotides even for tblast[nx] */
      /* If subject is translated, length adjustment is given in nucleotide
         scale. */
      if (program_number == eBlastTypeTblastn || 
          program_number == eBlastTypeTblastx)
      {
         length_adjustment /= CODON_LENGTH;
         subject_length /= CODON_LENGTH;
      }
      subject_length = MAX(subject_length - length_adjustment, 1);

      lh_helper[0].ptr = hp_start;
      lh_helper[0].q_off_trim = 0;
      lh_helper[0].s_off_trim = 0;
      lh_helper[0].maxsum1  = -10000;
      lh_helper[0].next_larger  = 0;
      
      /* lh_helper[0]  = empty     = additional end marker
       * lh_helper[1]  = hsp_start = empty entry used in original code
       * lh_helper[2]  = hsp_array->next = hsp_array[0]
       * lh_helper[i]  = ... = hsp_array[i-2] (for i>=2) 
       */
      first_pass=1;    /* do full search */
      path_changed=1;
      for (H=hp_start->next; H!=NULL; H=H->next) 
         H->hsp_link.changed=1;

      while (number_of_hsps > 0)
      {
         Int4 max[3];
         max[0]=max[1]=max[2]=-10000;
         /* Initialize the 'best' parameter */
         best[0] = best[1] = NULL;
         
         
         /* See if we can avoid recomputing all scores:
          *  - Find the max paths (based on old scores). 
          *  - If no paths were changed by removal of nodes (ie research==0) 
          *    then these max paths are still the best.
          *  - else if these max paths were unchanged, then they are still the best.
          */
         use_current_max=0;
         if (!first_pass){
            Int4 max0,max1;
            /* Find the current max sums */
            if(!ignore_small_gaps){
               max0 = -cutoff[0];
               max1 = -cutoff[1];
               for (H=hp_start->next; H!=NULL; H=H->next) {
                  Int4 sum0=H->hsp_link.sum[0];
                  Int4 sum1=H->hsp_link.sum[1];
                  if(sum0>=max0)
                  {
                     max0=sum0;
                     best[0]=H;
                  }
                  if(sum1>=max1)
                  {
                     max1=sum1;
                     best[1]=H;
                  }
               }
            } else {
               maxscore = -cutoff[1];
               for (H=hp_start->next; H!=NULL; H=H->next) {
                  Int4  sum=H->hsp_link.sum[1];
                  if(sum>=maxscore)
                  {
                     maxscore=sum;
                     best[1]=H;
                  }
               }
            }
            if(path_changed==0){
               /* No path was changed, use these max sums. */
               use_current_max=1;
            }
            else{
               /* If max path hasn't chaged, we can use it */
               /* Walk down best, give up if we find a removed item in path */
               use_current_max=1;
               if(!ignore_small_gaps){
                  for (H=best[0]; H!=NULL; H=H->hsp_link.link[0]) 
                     if (H->linked_to==-1000) {use_current_max=0; break;}
               }
               if(use_current_max)
                  for (H=best[1]; H!=NULL; H=H->hsp_link.link[1]) 
                     if (H->linked_to==-1000) {use_current_max=0; break;}
               
            }
         }

         /* reset helper_info */
         /* Inside this while loop, the linked list order never changes 
          * So here we initialize an array of commonly used info, 
          * and in this loop we access these arrays instead of the actual list 
          */
         if(!use_current_max){
            for (H=hp_start,H_index=1; H!=NULL; H=H->next,H_index++) {
               Int4 s_frame = H->hsp->subject.frame;
               Int4 s_off_t = H->s_offset_trim;
               Int4 q_off_t = H->q_offset_trim;
               lh_helper[H_index].ptr = H;
               lh_helper[H_index].q_off_trim = q_off_t;
               lh_helper[H_index].s_off_trim = s_off_t;
               for(i=0;i<BLAST_NUMBER_OF_ORDERING_METHODS;i++)
                  lh_helper[H_index].sum[i] = H->hsp_link.sum[i];
               max[SIGN(s_frame)+1]=
                  MAX(max[SIGN(s_frame)+1],H->hsp_link.sum[1]);
               lh_helper[H_index].maxsum1 =max[SIGN(s_frame)+1];					   
               
               /* set next_larger to link back to closest entry with a sum1 
                  larger than this */
               {
                  Int4 cur_sum=lh_helper[H_index].sum[1];
                  Int4 prev = H_index-1;
                  Int4 prev_sum = lh_helper[prev].sum[1];
                  while((cur_sum>=prev_sum) && (prev>0)){
                     prev=lh_helper[prev].next_larger;
                     prev_sum = lh_helper[prev].sum[1];
                  }
                  lh_helper[H_index].next_larger = prev;
               }
               H->linked_to = 0;
            }
            
            lh_helper[1].maxsum1 = -10000;
            
            /****** loop iter for index = 0  **************************/
            if(!ignore_small_gaps)
            {
               index=0;
               maxscore = -cutoff[index];
               H_index = 2;
               for (H=hp_start->next; H!=NULL; H=H->next,H_index++) 
               {
                  Int4 H_hsp_num=0;
                  Int4 H_hsp_sum=0;
                  double H_hsp_xsum=0.0;
                  LinkHSPStruct* H_hsp_link=NULL;
                  if (H->hsp->score > cutoff[index]) {
                     Int4 H_query_etrim = H->q_end_trim;
                     Int4 H_sub_etrim = H->s_end_trim;
                     Int4 H_q_et_gap = H_query_etrim+window_size;
                     Int4 H_s_et_gap = H_sub_etrim+window_size;
                     
                     /* We only walk down hits with the same frame sign */
                     /* for (H2=H->prev; H2!=NULL; H2=H2->prev,H2_index--) */
                     for (H2_index=H_index-1; H2_index>1; H2_index=H2_index-1) 
                     {
                        Int4 b1,b2,b4,b5;
                        Int4 q_off_t,s_off_t,sum;
                        
                        /* s_frame = lh_helper[H2_index].s_frame; */
                        q_off_t = lh_helper[H2_index].q_off_trim;
                        s_off_t = lh_helper[H2_index].s_off_trim;
                        
                        /* combine tests to reduce mispredicts -cfj */
                        b1 = q_off_t <= H_query_etrim;
                        b2 = s_off_t <= H_sub_etrim;
                        sum = lh_helper[H2_index].sum[index];
                        
                        
                        b4 = ( q_off_t > H_q_et_gap ) ;
                        b5 = ( s_off_t > H_s_et_gap ) ;
                        
                        /* list is sorted by q_off, so q_off should only increase.
                         * q_off_t can only differ from q_off by trim_size
                         * So once q_off_t is large enough (ie it exceeds limit 
                         * by trim_size), we can stop.  -cfj 
                         */
                        if(q_off_t > (H_q_et_gap+trim_size))
                           break;
                        
                        if (b1|b2|b5|b4) continue;
                        
                        if (sum>H_hsp_sum) 
                        {
                           H2=lh_helper[H2_index].ptr; 
                           H_hsp_num=H2->hsp_link.num[index];
                           H_hsp_sum=H2->hsp_link.sum[index];
                           H_hsp_xsum=H2->hsp_link.xsum[index];
                           H_hsp_link=H2;
                        }
                     } /* end for H2... */
                  }
                  { 
                     Int4 score=H->hsp->score;
                     double new_xsum = H_hsp_xsum + (score*(kbp[H->hsp->context]->Lambda));
                     Int4 new_sum = H_hsp_sum + (score - cutoff[index]);
                     
                     H->hsp_link.sum[index] = new_sum;
                     H->hsp_link.num[index] = H_hsp_num+1;
                     H->hsp_link.link[index] = H_hsp_link;
                     lh_helper[H_index].sum[index] = new_sum;
                     if (new_sum >= maxscore) 
                     {
                        maxscore=new_sum;
                        best[index]=H;
                     }
                     H->hsp_link.xsum[index] = new_xsum;
                     if(H_hsp_link)
                        ((LinkHSPStruct*)H_hsp_link)->linked_to++;
                  }
               } /* end for H=... */
            }
            /****** loop iter for index = 1  **************************/
            index=1;
            maxscore = -cutoff[index];
            H_index = 2;
            for (H=hp_start->next; H!=NULL; H=H->next,H_index++) 
            {
               Int4 H_hsp_num=0;
               Int4 H_hsp_sum=0;
               double H_hsp_xsum=0.0;
               LinkHSPStruct* H_hsp_link=NULL;
               
               H->hsp_link.changed=1;
               H2 = H->hsp_link.link[index];
               if ((!first_pass) && ((H2==0) || (H2->hsp_link.changed==0)))
               {
                  /* If the best choice last time has not been changed, then 
                     it is still the best choice, so no need to walk down list.
                  */
                  if(H2){
                     H_hsp_num=H2->hsp_link.num[index];
                     H_hsp_sum=H2->hsp_link.sum[index];
                     H_hsp_xsum=H2->hsp_link.xsum[index];
                  }
                  H_hsp_link=H2;
                  H->hsp_link.changed=0;
               } else if (H->hsp->score > cutoff[index]) {
                  Int4 H_query_etrim = H->q_end_trim;
                  Int4 H_sub_etrim = H->s_end_trim;
                  

                  /* Here we look at what was the best choice last time, if it's
                   * still around, and set this to the initial choice. By
                   * setting the best score to a (potentially) large value
                   * initially, we can reduce the number of hsps checked. 
                   */
                  
                  /* Currently we set the best score to a value just less than
                   * the real value. This is not really necessary, but doing
                   * this ensures that in the case of a tie, we make the same
                   * selection the original code did.
                   */
                  
                  if(!first_pass&&H2&&H2->linked_to>=0){
                     if(1){
                        /* We set this to less than the real value to keep the
                           original ordering in case of ties. */
                        H_hsp_sum=H2->hsp_link.sum[index]-1;
                     }else{
                        H_hsp_num=H2->hsp_link.num[index];
                        H_hsp_sum=H2->hsp_link.sum[index];
                        H_hsp_xsum=H2->hsp_link.xsum[index];
                        H_hsp_link=H2;
                     }
                  }
                  
                  /* We now only walk down hits with the same frame sign */
                  /* for (H2=H->prev; H2!=NULL; H2=H2->prev,H2_index--) */
                  for (H2_index=H_index-1; H2_index>1;)
                  {
                     Int4 b0,b1,b2;
                     Int4 q_off_t,s_off_t,sum,next_larger;
                     LinkHelpStruct * H2_helper=&lh_helper[H2_index];
                     sum = H2_helper->sum[index];
                     next_larger = H2_helper->next_larger;
                     
                     s_off_t = H2_helper->s_off_trim;
                     q_off_t = H2_helper->q_off_trim;
                     
                     b0 = sum <= H_hsp_sum;
                     
                     /* Compute the next H2_index */
                     H2_index--;
                     if(b0){	 /* If this sum is too small to beat H_hsp_sum, advance to a larger sum */
                        H2_index=next_larger;
                     }

                     /* combine tests to reduce mispredicts -cfj */
                     b1 = q_off_t <= H_query_etrim;
                     b2 = s_off_t <= H_sub_etrim;
                     
                     if(0) if(H2_helper->maxsum1<=H_hsp_sum)break;
                     
                     if (!(b0|b1|b2) )
                     {
                        H2 = H2_helper->ptr;
                        
                        H_hsp_num=H2->hsp_link.num[index];
                        H_hsp_sum=H2->hsp_link.sum[index];
                        H_hsp_xsum=H2->hsp_link.xsum[index];
                        H_hsp_link=H2;
                     }
                  } /* end for H2_index... */
               } /* end if(H->score>cuttof[]) */
               { 
                  Int4 score=H->hsp->score;
                  double new_xsum = 
                     H_hsp_xsum + (score*(kbp[H->hsp->context]->Lambda));
                  Int4 new_sum = H_hsp_sum + (score - cutoff[index]);
                  
                  H->hsp_link.sum[index] = new_sum;
                  H->hsp_link.num[index] = H_hsp_num+1;
                  H->hsp_link.link[index] = H_hsp_link;
                  lh_helper[H_index].sum[index] = new_sum;
                  lh_helper[H_index].maxsum1 = MAX(lh_helper[H_index-1].maxsum1, new_sum);
                  /* Update this entry's 'next_larger' field */
                  {
                     Int4 cur_sum=lh_helper[H_index].sum[1];
                     Int4 prev = H_index-1;
                     Int4 prev_sum = lh_helper[prev].sum[1];
                     while((cur_sum>=prev_sum) && (prev>0)){
                        prev=lh_helper[prev].next_larger;
                        prev_sum = lh_helper[prev].sum[1];
                     }
                     lh_helper[H_index].next_larger = prev;
                  }
                  
                  if (new_sum >= maxscore) 
                  {
                     maxscore=new_sum;
                     best[index]=H;
                  }
                  H->hsp_link.xsum[index] = new_xsum;
                  if(H_hsp_link)
                     ((LinkHSPStruct*)H_hsp_link)->linked_to++;
               }
            }
            path_changed=0;
            first_pass=0;
         }
         /********************************/
         if (!ignore_small_gaps)
         {
            /* Select the best ordering method.
               First we add back in the value cutoff[index] * the number
               of links, as this was subtracted out for purposes of the
               comparison above. */
            best[0]->hsp_link.sum[0] +=
               (best[0]->hsp_link.num[0])*cutoff[0];

            prob[0] = BLAST_SmallGapSumE(kbp[query_context],
                         window_size,
                         best[0]->hsp_link.num[0], best[0]->hsp_link.xsum[0],
                         query_length, subject_length,
                         BLAST_GapDecayDivisor(gap_decay_rate,
                                              best[0]->hsp_link.num[0]) );

            /* Adjust the e-value because we are performing multiple tests */
            if( best[0]->hsp_link.num[0] > 1 ) {
              if( gap_prob == 0 || (prob[0] /= gap_prob) > INT4_MAX ) {
                prob[0] = INT4_MAX;
              }
            }

            prob[1] = BLAST_LargeGapSumE(kbp[query_context],
                         best[1]->hsp_link.num[1],
                         best[1]->hsp_link.xsum[1],
                         query_length, subject_length,
                         BLAST_GapDecayDivisor(gap_decay_rate,
                                              best[1]->hsp_link.num[1]));

            if( best[1]->hsp_link.num[1] > 1 ) {
              if( 1 - gap_prob == 0 || (prob[1] /= 1 - gap_prob) > INT4_MAX ) {
                prob[1] = INT4_MAX;
              }
            }
            ordering_method =
               prob[0]<=prob[1] ? BLAST_SMALL_GAPS : BLAST_LARGE_GAPS;
         }
         else
         {
            /* We only consider the case of big gaps. */
            best[1]->hsp_link.sum[1] +=
               (best[1]->hsp_link.num[1])*cutoff[1];

            prob[1] = BLAST_LargeGapSumE(kbp[query_context],
                         best[1]->hsp_link.num[1],
                         best[1]->hsp_link.xsum[1],
                         query_length, subject_length,
                         BLAST_GapDecayDivisor(gap_decay_rate,
                                              best[1]->hsp_link.num[1]));
            ordering_method = BLAST_LARGE_GAPS;
         }

         best[ordering_method]->start_of_chain = TRUE;
         
         /* AM: Support for query concatenation. */
         prob[ordering_method] *= 
            ((double)query_info->eff_searchsp_array[query_context] /
             ((double)subject_length*query_length));
         
         best[ordering_method]->hsp->evalue = prob[ordering_method];
         
         /* remove the links that have been ordered already. */
         if (best[ordering_method]->hsp_link.link[ordering_method])
            linked_set = TRUE;
         else
            linked_set = FALSE;

         if (best[ordering_method]->linked_to>0) path_changed=1;
         for (H=best[ordering_method]; H!=NULL;
              H=H->hsp_link.link[ordering_method]) 
         {
            if (H->linked_to>1) path_changed=1;
            H->linked_to=-1000;
            H->hsp_link.changed=1;
            /* record whether this is part of a linked set. */
            H->linked_set = linked_set;
            H->ordering_method = ordering_method;
            H->hsp->evalue = prob[ordering_method];
            if (H->next)
               (H->next)->prev=H->prev;
            if (H->prev)
               (H->prev)->next=H->next;
            number_of_hsps--;
         }
         
      } /* end while num_hsps... */
	} /* end for frame_index ... */

   sfree(hp_frame_start);
   sfree(hp_frame_number);
   sfree(hp_start->hsp);
   sfree(hp_start);

   if (translated_query) {
      qsort(link_hsp_array,total_number_of_hsps,sizeof(LinkHSPStruct*), 
            rev_compare_hsps_transl);
      qsort(link_hsp_array, total_number_of_hsps,sizeof(LinkHSPStruct*), 
            fwd_compare_hsps_transl);
   } else {
      qsort(link_hsp_array,total_number_of_hsps,sizeof(LinkHSPStruct*), 
            rev_compare_hsps);
      qsort(link_hsp_array, total_number_of_hsps,sizeof(LinkHSPStruct*), 
            fwd_compare_hsps);
   }

   /* Sort by starting position. */
   

	for (index=0, last_hsp=NULL;index<total_number_of_hsps; index++) 
	{
		H = link_hsp_array[index];
		H->prev = NULL;
		H->next = NULL;
	}

   /* hook up the HSP's. */
	first_hsp = NULL;
	for (index=0, last_hsp=NULL;index<total_number_of_hsps; index++) 
   {
		H = link_hsp_array[index];

      /* If this is not a single piece or the start of a chain, then Skip it. */
      if (H->linked_set == TRUE && H->start_of_chain == FALSE)
			continue;

      /* If the HSP has no "link" connect the "next", otherwise follow the "link"
         chain down, connecting them with "next" and "prev". */
		if (last_hsp == NULL)
			first_hsp = H;
		H->prev = last_hsp;
		ordering_method = H->ordering_method;
		if (H->hsp_link.link[ordering_method] == NULL)
		{
         /* Grab the next HSP that is not part of a chain or the start of a chain */
         /* The "next" pointers are not hooked up yet in HSP's further down array. */
         index1=index;
         H2 = index1<(total_number_of_hsps-1) ? link_hsp_array[index1+1] : NULL;
         while (H2 && H2->linked_set == TRUE && 
                H2->start_of_chain == FALSE)
         {
            index1++;
		     	H2 = index1<(total_number_of_hsps-1) ? link_hsp_array[index1+1] : NULL;
         }
         H->next= H2;
		}
		else
		{
			/* The first one has the number of links correct. */
			num_links = H->hsp_link.num[ordering_method];
			link = H->hsp_link.link[ordering_method];
			while (link)
			{
				H->hsp->num = num_links;
				H->sumscore = H->hsp_link.sum[ordering_method];
				H->next = (LinkHSPStruct*) link;
				H->prev = last_hsp;
				last_hsp = H;
				H = H->next;
				if (H != NULL)
				    link = H->hsp_link.link[ordering_method];
				else
				    break;
			}
			/* Set these for last link in chain. */
			H->hsp->num = num_links;
			H->sumscore = H->hsp_link.sum[ordering_method];
         /* Grab the next HSP that is not part of a chain or the start of a chain */
         index1=index;
         H2 = index1<(total_number_of_hsps-1) ? link_hsp_array[index1+1] : NULL;
         while (H2 && H2->linked_set == TRUE && 
                H2->start_of_chain == FALSE)
		   {
            index1++;
            H2 = index1<(total_number_of_hsps-1) ? link_hsp_array[index1+1] : NULL;
			}
         H->next= H2;
			H->prev = last_hsp;
		}
		last_hsp = H;
	}
	
   /* The HSP's may be in a different order than they were before, 
      but first_hsp contains the first one. */
   for (index = 0, H = first_hsp; index < hsp_list->hspcnt; index++) {
      hsp_list->hsp_array[index] = H->hsp;
      /* Free the wrapper structure */
      H2 = H->next;
      sfree(H);
      H = H2;
   }
   sfree(link_hsp_array);
   sfree(lh_helper);

   return 0;
}

static void 
ConnectLinkHSPStructs(LinkHSPStruct** linkhsp_array, Int4 hspcnt)
{
   Int4 index, index1, i;
   LinkHSPStruct* linkhsp;

   qsort(linkhsp_array, hspcnt, sizeof(LinkHSPStruct*), 
         sumscore_compare_hsps);

   linkhsp = linkhsp_array[0];
   for (index=0; index<hspcnt; linkhsp = linkhsp->next) {
      if (linkhsp->linked_set) {
         index1 = linkhsp->hsp->num;
         for (i=1; i < index1; i++, linkhsp = linkhsp->next) {
            linkhsp->next->hsp->evalue = linkhsp->hsp->evalue; 
            linkhsp->next->hsp->num = linkhsp->hsp->num;
            linkhsp->next->sumscore = linkhsp->sumscore;
         }
      } 
      while (++index < hspcnt)
         if (!linkhsp_array[index]->linked_set ||
             linkhsp_array[index]->start_of_chain)
            break;
      if (index == hspcnt) {
         linkhsp->next = NULL;
         break;
      }
      linkhsp->next = linkhsp_array[index];
   }
}

static void 
AddHSPToLinkedSet(LinkHSPStruct** head_hsp_ptr, LinkHSPStruct* new_hsp, 
                  Int4 sumscore, double evalue, Boolean reverse_link)
{
   LinkHSPStruct* var_hsp = NULL;
   int i;
   LinkHSPStruct* head_hsp = *head_hsp_ptr;

   if (!new_hsp || !head_hsp_ptr)
      return;

   head_hsp = *head_hsp_ptr;

   if (!reverse_link) {
      head_hsp->start_of_chain = TRUE;
      head_hsp->sumscore = sumscore;
      head_hsp->hsp->evalue = evalue;
      new_hsp->start_of_chain = FALSE;
      if (head_hsp->linked_set) 
         for (var_hsp = head_hsp, i=1; i<head_hsp->hsp->num; 
              var_hsp = var_hsp->next, i++);
      else 
         var_hsp = head_hsp;
      var_hsp->next = new_hsp;
      new_hsp->prev = var_hsp;
      head_hsp->hsp->num += new_hsp->hsp->num;
   } else {
      new_hsp->next = head_hsp;
      head_hsp->prev = new_hsp;
      if (new_hsp->linked_set) {
         for (var_hsp = new_hsp; 
              var_hsp->start_of_chain == FALSE; 
              var_hsp = var_hsp->prev);
      } else
         var_hsp = new_hsp;
      var_hsp->start_of_chain = TRUE;
      var_hsp->sumscore = sumscore;
      var_hsp->hsp->evalue = evalue;
      var_hsp->hsp->num += head_hsp->hsp->num;
      head_hsp->start_of_chain = FALSE;
   }
   
   head_hsp->linked_set = new_hsp->linked_set = TRUE;
   if (reverse_link)
      *head_hsp_ptr = var_hsp;
}

/** Greedy algorithm to link HSPs with uneven gaps.
 * Sorts HSPs by score. Starting with the highest scoring HSP, finds
 * an HSP that produces the best sum e-value when added to the HSP set under 
 * consideration. The neighboring HSPs in a set must have endpoints within a
 * window of each other on the protein axis, and within the longest allowed 
 * intron length on the nucleotide axis. When no more HSPs can be added to the
 * highest scoring set, the next highest scoring HSP is considered that is not 
 * yet part of any set.
 * @param program Type of BLAST program (blastx or tblastn) [in]
 * @param hsp_list Structure containing all HSPs for a given subject [in] [out]
 * @param query_info Query information, including effective lengths [in]
 * @param subject_length Subject sequence length [in]
 * @param sbp Scoring and statistical parameters [in]
 * @param link_hsp_params Parameters for linking HSPs [in]
 */
static Int2
Blast_UnevenGapLinkHSPs(EBlastProgramType program, BlastHSPList* hsp_list, 
   BlastQueryInfo* query_info, Int4 subject_length, BlastScoreBlk* sbp, 
   const BlastLinkHSPParameters* link_hsp_params)
{
   BlastHSP** hsp_array;
   LinkHSPStruct** score_hsp_array;  /* an array of HSPs sorted by decreasing 
                                        score */
   LinkHSPStruct** offset_hsp_array; /* an array of HSPs sorted by increasing
                                        query offset */
   LinkHSPStruct** end_hsp_array;    /* an array of HSPs sorted by increasing
                                        query endpoint */
   LinkHSPStruct* lhsp,* head_hsp,* best_hsp,* var_hsp;
   Int4 hspcnt, index, index1, i;
   double best_evalue, evalue;
   Int4 sumscore, best_sumscore = 0;
   Boolean reverse_link;
   Int4 longest_intron;
   LinkHSPStruct** link_hsp_array;
   Int4 gap_size, overlap_size;

   longest_intron = link_hsp_params->longest_intron; /* Maximal gap size in
                                                        subject */
	gap_size = link_hsp_params->gap_size; /* Maximal gap size in query */
	overlap_size = link_hsp_params->overlap_size; /* Maximal overlap size in
                                                    query or subject */

   hspcnt = hsp_list->hspcnt;
   hsp_array = hsp_list->hsp_array;

   if (program == eBlastTypeBlastx) {
      BlastSeg seg;
      /* Create a temporary hsp_array with query and subject switched.
      * Also adjust context numbers so they distinguish HSPs from different
      * queries, but not different frames, since frame distinction is moved
      * to the subject side.
      */
      for (index = 0; index < hspcnt; ++index) {
         seg = hsp_array[index]->query;
         hsp_array[index]->query = hsp_list->hsp_array[index]->subject;
         hsp_array[index]->subject = seg;
         hsp_array[index]->context = 
            Blast_GetQueryIndexFromContext(hsp_array[index]->context, program);
      }
   }

   /* Set up an array of HSP structure wrappers */
   link_hsp_array = (LinkHSPStruct**) malloc(hspcnt*sizeof(LinkHSPStruct*));
   for (index = 0; index < hspcnt; ++index) {
      link_hsp_array[index] = (LinkHSPStruct*) calloc(1, sizeof(LinkHSPStruct));
      link_hsp_array[index]->hsp = hsp_array[index];
      link_hsp_array[index]->linked_set = FALSE;
      hsp_array[index]->num = 1;
   }

   score_hsp_array = (LinkHSPStruct**) malloc(hspcnt*sizeof(LinkHSPStruct*));
   offset_hsp_array = (LinkHSPStruct**) malloc(hspcnt*sizeof(LinkHSPStruct*));
   end_hsp_array = (LinkHSPStruct**) malloc(hspcnt*sizeof(LinkHSPStruct*));

   memcpy(score_hsp_array, link_hsp_array, hspcnt*sizeof(LinkHSPStruct*));
   memcpy(offset_hsp_array, link_hsp_array, hspcnt*sizeof(LinkHSPStruct*));
   memcpy(end_hsp_array, link_hsp_array, hspcnt*sizeof(LinkHSPStruct*));
   qsort(offset_hsp_array, hspcnt, sizeof(LinkHSPStruct*), fwd_compare_hsps);
   qsort(end_hsp_array, hspcnt, sizeof(LinkHSPStruct*), end_compare_hsps);

   qsort(score_hsp_array, hspcnt, sizeof(LinkHSPStruct*), 
            sumscore_compare_hsps);
      
   /* head_hsp is set to NULL whenever there is no current linked set that is
      being worked on. */
   head_hsp = NULL;
   for (index = 0; index < hspcnt && score_hsp_array[index]; ) {
      if (!head_hsp) {
         /* Find the highest scoring HSP that is not yet part of a linked set. */
         while (index<hspcnt && score_hsp_array[index] && 
                score_hsp_array[index]->linked_set)
            index++;
         if (index==hspcnt)
            break;
         head_hsp = score_hsp_array[index];
      }
      best_evalue = head_hsp->hsp->evalue;
      best_hsp = NULL;
      reverse_link = FALSE;
      
      if (head_hsp->linked_set)
         for (var_hsp = head_hsp, i=1; i<head_hsp->hsp->num; 
              var_hsp = var_hsp->next, i++);
      else
         var_hsp = head_hsp;

      /* Look for closest HSP to the right that starts within the overlap size
       * from where this HSP set ends.
       */
      index1 = hsp_binary_search(offset_hsp_array, hspcnt,
                                 var_hsp->hsp->query.end - overlap_size, TRUE);
      /* Check all HSPs that start in the window around this HSP set's end. */
      while (index1 < hspcnt && 
             offset_hsp_array[index1]->hsp->query.offset <= 
             var_hsp->hsp->query.end + gap_size) {
         lhsp = offset_hsp_array[index1++];
         /* If this is already part of a linked set, disregard it */
         if (lhsp == var_hsp || lhsp == head_hsp || 
             (lhsp->linked_set && !lhsp->start_of_chain))
            continue;
         /* Check if the subject coordinates are consistent with query. Also
            make sure the HSPs to be linked are on the same strand. */
         if (SIGN(lhsp->hsp->subject.frame) != SIGN(var_hsp->hsp->subject.frame) ||
             (lhsp->hsp->subject.offset < 
             var_hsp->hsp->subject.end - overlap_size) || 
             (lhsp->hsp->subject.offset > 
              var_hsp->hsp->subject.end + longest_intron))
            continue;
         /* Check if the e-value for the new combined HSP set is better than for
            the previously obtained set. */
         if ((evalue = SumHSPEvalue(program, sbp, query_info, subject_length, 
                                    link_hsp_params, head_hsp, lhsp, &sumscore)) < 
             MIN(best_evalue, lhsp->hsp->evalue)) {
            best_hsp = lhsp;
            best_evalue = evalue;
            best_sumscore = sumscore;
         }
      }

      /* Look for farthest HSP to the left that ends within a window from 
       * where this HSP set starts.
       */
      index1 = hsp_binary_search(end_hsp_array, hspcnt,
                                 head_hsp->hsp->query.offset - gap_size, FALSE);
      /* Check all HSPs that end in the window around this HSP set's start. */ 
      while (index1 < hspcnt && end_hsp_array[index1]->hsp->query.end <= 
             head_hsp->hsp->query.offset + overlap_size) {
         lhsp = end_hsp_array[index1++];

         /* Check if the subject coordinates are consistent with query. Also
            make sure the HSPs are on the same strand. */
         if (lhsp == head_hsp || 
             SIGN(lhsp->hsp->subject.frame) != SIGN(head_hsp->hsp->subject.frame) ||
             lhsp->hsp->subject.end > 
             head_hsp->hsp->subject.offset + overlap_size || 
             lhsp->hsp->subject.end < 
             head_hsp->hsp->subject.offset - longest_intron)
            continue;
         if (lhsp->linked_set) {
            for (var_hsp = lhsp, i=1; var_hsp->start_of_chain == FALSE; 
                 var_hsp = var_hsp->prev, i++);
            if (i<var_hsp->hsp->num || var_hsp == head_hsp)
               continue;
         } else {
            var_hsp = lhsp;
         }
         /* Check if the e-value for the new combined HSP set is better than for
            the previously obtained set. */
         if ((evalue = SumHSPEvalue(program, sbp, query_info, subject_length, 
                          link_hsp_params, var_hsp, head_hsp, &sumscore)) < 
             MIN(var_hsp->hsp->evalue, best_evalue)) {
            best_hsp = lhsp;
            best_evalue = evalue;
            best_sumscore = sumscore;
            reverse_link = TRUE;
         }
      }
         
      /* Link the new HSP to the set, if it qualified. */
      if (best_hsp) {
         AddHSPToLinkedSet(&head_hsp, best_hsp, best_sumscore, best_evalue, 
                           reverse_link);
      } else {
         head_hsp = NULL;
         ++index;
      }
   }
  
   ConnectLinkHSPStructs(score_hsp_array, hspcnt);

   head_hsp = score_hsp_array[0];

   sfree(score_hsp_array);
   sfree(offset_hsp_array);
   sfree(end_hsp_array);

   if (program == eBlastTypeBlastx) {
      BlastSeg seg;
      /* Substitute the HSP pointers in the original hsp_array by the new ones, 
       * switching back subject and query. Also free the structures and array 
       * that are no longer needed.
       */
      for (var_hsp = head_hsp; var_hsp; var_hsp = var_hsp->next) {
         seg = var_hsp->hsp->query;
         var_hsp->hsp->query = var_hsp->hsp->subject;
         var_hsp->hsp->subject = seg;
         /* Also restore the correct context number. */
         var_hsp->hsp->context = var_hsp->hsp->context * NUM_FRAMES +
            FrameToContext(var_hsp->hsp->query.frame);
      }
   }

   /* Place HSPs in the original HSP array in their new order */
   for (index=0; head_hsp && index<hspcnt; index++) {
      hsp_list->hsp_array[index] = head_hsp->hsp;
      var_hsp = head_hsp->next;
      sfree(head_hsp);
      head_hsp = var_hsp;
   }

   sfree(link_hsp_array);
   return 0;
}

Int2 
BLAST_LinkHsps(EBlastProgramType program_number, BlastHSPList* hsp_list, 
   BlastQueryInfo* query_info, Int4 subject_length,
   BlastScoreBlk* sbp, const BlastLinkHSPParameters* link_hsp_params,
   Boolean gapped_calculation)
{
	if (hsp_list && hsp_list->hspcnt > 0)
	{
	   Int4 index;
	   /* Remove any information on number of linked HSPs from previous
	      linking. */
	   for (index = 0; index < hsp_list->hspcnt; ++index)
	      hsp_list->hsp_array[index]->num = 0;

      /* Link up the HSP's for this hsp_list. */
      if (link_hsp_params->longest_intron <= 0)
      {
         Blast_EvenGapLinkHSPs(program_number, hsp_list, query_info, 
			       subject_length, sbp, link_hsp_params, 
			       gapped_calculation);
         /* The HSP's may be in a different order than they were before, 
            but hsp contains the first one. */
      } else {
         /* Calculate individual HSP e-values first - they'll be needed to
            compare with sum e-values. Use decay rate to compensate for 
            multiple tests. */
         Blast_HSPListGetEvalues(query_info, hsp_list, 
            gapped_calculation, sbp, link_hsp_params->gap_decay_rate);
         
         Blast_UnevenGapLinkHSPs(program_number, hsp_list, query_info, 
                                 subject_length, sbp, link_hsp_params);
      }
	}

	return 0;
}

