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

/** @file blast_parameters.c
 * These parameters are normally set by:
 *    1.) reading the options in blast_options.[ch] to find user preferences
 *    2.) making intelligent choices based upon the program, user preferences, and other
 *      data such as the sequences length.
 *
 *  NOTE: these parameters should be set by calls in algo/blast/core, prefereably to funcitons in 
 *        this file.  User preferences should be controlled by the structures and functions in 
 *        blast_options.[ch].  The parameters belong to algo/blast/core, the options belong to the user.
 *
 */

#ifndef SKIP_DOXYGEN_PROCESSING
static char const rcsid[] = 
    "$Id$";
#endif /* SKIP_DOXYGEN_PROCESSING */

#include <algo/blast/core/blast_parameters.h>
#include <algo/blast/core/blast_lookup.h>
#include <algo/blast/core/mb_lookup.h>
#include <algo/blast/core/phi_lookup.h>
#include <algo/blast/core/blast_rps.h>


/** Looks for a "valid" Karlin Blk within the list of blocks.
 * Valid means that Lambda, K, and H are not all -1.
 * Invalid blocks are often caused by completely masked sequences.
 * @param kbp_in array of Karlin blocks to be searched [in]
 * @param query_info information on number of queries [in]
 * @param kbp_ret the object to be pointed at [out]
 * @return zero on success, 1 if no valid block found
 */

static Int2
s_BlastFindValidKarlinBlk(Blast_KarlinBlk** kbp_in, const BlastQueryInfo* query_info, Blast_KarlinBlk** kbp_ret)
{
        Int4 index;   /* Look for the first valid kbp. */
        Int2 status=1;  /* 1 means no valid block found. */

        ASSERT(kbp_in && query_info && kbp_ret);

        for (index=query_info->first_context; index<=query_info->last_context; index++)
        {
             if (kbp_in[index]->Lambda > 0 && kbp_in[index]->K > 0 && kbp_in[index]->H > 0)
             {
                  *kbp_ret = kbp_in[index];
                  status = 0;
                  break;
             }
        }
        return status;
}

/** Determines optimal extension method given 1.) the type of
 * search (e.g., program, whether rps, discontig. mb etc.).
 * 2.) whether a flag is set specifying the AG stride option
 * if 2.) is true then the lookup table has been set up for AG stride
 *
 * @param lookup_wrap pointer to lookup table [in]
 * @return suggested extension method.  eMaxSeedExtensionMethod means 
 *     the input lookup table was not found. 
 */
ESeedExtensionMethod
s_GetBestExtensionMethod(const LookupTableWrap* lookup_wrap)
{
   ESeedExtensionMethod retval = eMaxSeedExtensionMethod;

   ASSERT(lookup_wrap);

   switch (lookup_wrap->lut_type) {
     case AA_LOOKUP_TABLE:
     case PHI_AA_LOOKUP:  
     case PHI_NA_LOOKUP:
     case RPS_LOOKUP_TABLE:
         retval = eRight;
         break;
     case NA_LOOKUP_TABLE:
         if (((BlastLookupTable*)lookup_wrap->lut)->ag_scanning_mode == TRUE)
               retval = eRightAndLeft;
         else
               retval = eRight;
         break;
     case MB_LOOKUP_TABLE:
         if (((BlastMBLookupTable*)lookup_wrap->lut)->ag_scanning_mode == TRUE)
               retval = eRightAndLeft;
         else if (((BlastMBLookupTable*)lookup_wrap->lut)->template_length > 0)
               retval = eUpdateDiag;   /* Used for discontiguous megablast. */
         else
               retval = eRight;
         break;
   }
   ASSERT(retval != eMaxSeedExtensionMethod);

   return retval;
}

BlastInitialWordParameters*
BlastInitialWordParametersFree(BlastInitialWordParameters* parameters)

{
	sfree(parameters);
	return NULL;

}

/** Compute the default cutoff expect value for ungapped extensions
 * @param program The blast program type
 * @return The default per-program expect value
 */
static double 
s_GetCutoffEvalue(EBlastProgramType program)
{
   switch(program) {
   case eBlastTypeBlastn:
      return CUTOFF_E_BLASTN;
   case eBlastTypeBlastp: 
   case eBlastTypeRpsBlast: 
      return CUTOFF_E_BLASTP;
   case eBlastTypeBlastx: 
      return CUTOFF_E_BLASTX;
   case eBlastTypeTblastn:
   case eBlastTypeRpsTblastn:
      return CUTOFF_E_TBLASTN;
   case eBlastTypeTblastx:
      return CUTOFF_E_TBLASTX;
   case eBlastTypeUndefined:
   default:
      abort(); /* should never happen */
   }
   return 0.0;
}

Int2
BlastInitialWordParametersNew(EBlastProgramType program_number, 
   const BlastInitialWordOptions* word_options, 
   const BlastHitSavingParameters* hit_params, 
   const LookupTableWrap* lookup_wrap,
   BlastScoreBlk* sbp, 
   BlastQueryInfo* query_info, 
   Uint4 subject_length,
   BlastInitialWordParameters* *parameters)
{
   Blast_KarlinBlk* kbp_std;
   Int2 status = 0;
   const int kQueryLenForStacks = 50;  /* Use stacks rather than diag for 
                                     any query longer than this.  Only for blastn. */

   /* If parameters pointer is NULL, there is nothing to fill, 
      so don't do anything */
   if (!parameters)
      return 0;

   ASSERT(word_options);
   ASSERT(sbp);
   ASSERT(sbp->kbp_std[query_info->first_context]);
   if (s_BlastFindValidKarlinBlk(sbp->kbp_std, query_info, &kbp_std) != 0)
         return -1;

   *parameters = (BlastInitialWordParameters*) 
      calloc(1, sizeof(BlastInitialWordParameters));

   (*parameters)->options = (BlastInitialWordOptions *) word_options;

   (*parameters)->x_dropoff_init = (Int4)
      ceil(sbp->scale_factor * word_options->x_dropoff * NCBIMATH_LN2/
           kbp_std->Lambda);

   if (program_number == eBlastTypeBlastn &&
      (query_info->contexts[query_info->last_context].query_offset +
            query_info->contexts[query_info->last_context].query_length) > kQueryLenForStacks)
       (*parameters)->container_type = eWordStacks;
   else
       (*parameters)->container_type = eDiagArray;

   (*parameters)->extension_method = 
                 s_GetBestExtensionMethod(lookup_wrap);

   status = BlastInitialWordParametersUpdate(program_number,
               hit_params, sbp, query_info,
               subject_length, *parameters);

   return status;
}

Int2
BlastInitialWordParametersUpdate(EBlastProgramType program_number, 
   const BlastHitSavingParameters* hit_params, BlastScoreBlk* sbp, 
   BlastQueryInfo* query_info, Uint4 subj_length,
   BlastInitialWordParameters* parameters)
{
   Blast_KarlinBlk* kbp;
   Boolean gapped_calculation = TRUE;
   Int4 cutoff_s = 0;
   Int4 gap_trigger = 0;

   ASSERT(sbp);
   ASSERT(hit_params);
   ASSERT(query_info);

   /* kbp_gap is only non-NULL for gapped searches! */
   if (sbp->kbp_gap) {
      if (s_BlastFindValidKarlinBlk(sbp->kbp_gap, query_info, &kbp) != 0)
         return -1;
   } else {
      if (s_BlastFindValidKarlinBlk(sbp->kbp_std, query_info, &kbp) != 0)
         return -1;
      gapped_calculation = FALSE;
   }

   if (sbp->kbp_std)
   { /* We calculate the gap_trigger value here and use it as a cutoff to save ungapped alignments in a gapped
        search as well as a maximum value for the ungapped alignments in an ungapped search.  
        gap_trigger (at 22 bits) seems like a good value for both of these.  Ungapped blastn is an exception (see
        comment below) and it's not clear that this should be.  */
      Blast_KarlinBlk* kbp_ungap;
      const BlastInitialWordOptions* kOptions = parameters->options;
      if (s_BlastFindValidKarlinBlk(sbp->kbp_std, query_info, &kbp_ungap) != 0)
          return -1;
      gap_trigger = (Int4) ((kOptions->gap_trigger*NCBIMATH_LN2 + kbp_ungap->logK) / kbp_ungap->Lambda);
   }

   ASSERT(kbp);

   if (!gapped_calculation || program_number == eBlastTypeBlastn) {
      double cutoff_e = s_GetCutoffEvalue(program_number);
      Int4 concat_qlen =
          query_info->contexts[query_info->last_context].query_offset +
          query_info->contexts[query_info->last_context].query_length;
      Int4 avg_qlen = concat_qlen / (query_info->last_context + 1);
      double gap_decay_rate = 0.0;
      
      /* FIXME, hit_params->link_hsp_params is NULL for gapped blastn so is gap_decay_rate. */
      if (hit_params && hit_params->link_hsp_params)
          gap_decay_rate = hit_params->link_hsp_params->gap_decay_rate;
   
       /* include the length of reverse complement for blastn searchs. */
       if (program_number == eBlastTypeBlastn)
          avg_qlen *= 2;
   
       BLAST_Cutoffs(&cutoff_s, &cutoff_e, kbp, MIN(subj_length, (Uint4) avg_qlen)*subj_length,
                    TRUE, gap_decay_rate);

       if (program_number != eBlastTypeBlastn)  /* Perform this check for compatiability with the old code */
          cutoff_s = MIN(gap_trigger, cutoff_s);
   } else {
      cutoff_s = gap_trigger;
   }

   if (sbp->scale_factor > 0)
      cutoff_s *= (Int4)sbp->scale_factor;

   parameters->cutoff_score = MIN(hit_params->cutoff_score_max, cutoff_s);
   
   /* Note that x_dropoff_init stays constant throughout the search.
      The cutoff_score and x_dropoff parameters may be updated multiple times, 
      if every subject sequence is treated individually. Hence we need to know 
      the original value of x_dropoff in order to take the minimum. */
   if (parameters->x_dropoff_init != 0 && parameters->cutoff_score != 0) {
      parameters->x_dropoff = 
         MIN(parameters->x_dropoff_init, parameters->cutoff_score);
   } else if (parameters->cutoff_score != 0) {
      parameters->x_dropoff = parameters->cutoff_score;
   }

   return 0;
}


Int2 BlastExtensionParametersNew(EBlastProgramType program_number, 
        const BlastExtensionOptions* options, BlastScoreBlk* sbp,
        BlastQueryInfo* query_info, BlastExtensionParameters* *parameters)
{
   Blast_KarlinBlk* kbp=NULL;
   Blast_KarlinBlk* kbp_gap=NULL;
   BlastExtensionParameters* params;

   /* If parameters pointer is NULL, there is nothing to fill, 
      so don't do anything */
   if (!parameters)
      return 0;

       
 
   if (sbp->kbp) {
      if (s_BlastFindValidKarlinBlk(sbp->kbp, query_info, &kbp) != 0)
         return -1;
   } else {
      /* The Karlin block is not found, can't do any calculations */
      *parameters = NULL;
      return -1;
   }

   *parameters = params = (BlastExtensionParameters*) 
      calloc(1, sizeof(BlastExtensionParameters));

   params->options = (BlastExtensionOptions *) options;

   /* Set gapped X-dropoffs only if it is a gapped search. */
   if (sbp->kbp_gap) {
      if (s_BlastFindValidKarlinBlk(sbp->kbp_gap, query_info, &kbp_gap) != 0)
         return -1;
      params->gap_x_dropoff = 
         (Int4) (options->gap_x_dropoff*NCBIMATH_LN2 / kbp_gap->Lambda);
      params->gap_x_dropoff_final = (Int4) 
         (options->gap_x_dropoff_final*NCBIMATH_LN2 / kbp_gap->Lambda);
   }
   
   if (sbp->scale_factor > 1.0) {
      params->gap_x_dropoff *= (Int4)sbp->scale_factor;
      params->gap_x_dropoff_final *= (Int4)sbp->scale_factor;
   }
   return 0;
}

BlastExtensionParameters*
BlastExtensionParametersFree(BlastExtensionParameters* parameters)
{
  sfree(parameters);
  return NULL;
}


BlastScoringParameters*
BlastScoringParametersFree(BlastScoringParameters* parameters)
{
	sfree(parameters);
	return NULL;
}


Int2
BlastScoringParametersNew(const BlastScoringOptions* score_options, 
                          BlastScoreBlk* sbp, 
                          BlastScoringParameters* *parameters)
{
   BlastScoringParameters *params;
   double scale_factor;

   if (score_options == NULL)
      return 1;

   *parameters = params = (BlastScoringParameters*) 
                        calloc(1, sizeof(BlastScoringParameters));
   if (params == NULL)
      return 2;

   params->options = (BlastScoringOptions *)score_options;
   scale_factor = sbp->scale_factor;
   params->scale_factor = scale_factor;
   params->reward = score_options->reward;
   params->penalty = score_options->penalty;
   params->gap_open = score_options->gap_open * (Int4)scale_factor;
   params->gap_extend = score_options->gap_extend * (Int4)scale_factor;
   params->decline_align = score_options->decline_align * (Int4)scale_factor;
   params->shift_pen = score_options->shift_pen * (Int4)scale_factor;
   return 0;
}


BlastEffectiveLengthsParameters*
BlastEffectiveLengthsParametersFree(BlastEffectiveLengthsParameters* parameters)

{
	sfree(parameters);

	return NULL;
}

Int2 
BlastEffectiveLengthsParametersNew(const BlastEffectiveLengthsOptions* options, 
                               Int8 db_length, Int4 num_seqs,
                               BlastEffectiveLengthsParameters* *parameters)
{
   *parameters = (BlastEffectiveLengthsParameters*) 
      calloc(1, sizeof(BlastEffectiveLengthsParameters));
   (*parameters)->options = (BlastEffectiveLengthsOptions*) options;
   (*parameters)->real_db_length = db_length;
   (*parameters)->real_num_seqs = num_seqs;
   return 0;
}

BlastLinkHSPParameters* 
BlastLinkHSPParametersFree(BlastLinkHSPParameters* parameters)
{
   sfree(parameters);
   return NULL;
}

Int2 BlastLinkHSPParametersNew(EBlastProgramType program_number, 
                               Boolean gapped_calculation,
                               BlastLinkHSPParameters** link_hsp_params)
{
   BlastLinkHSPParameters* params;

   if (!link_hsp_params)
      return -1;

   params = (BlastLinkHSPParameters*)
      calloc(1, sizeof(BlastLinkHSPParameters));

   if (program_number == eBlastTypeBlastn || !gapped_calculation) {
      params->gap_prob = BLAST_GAP_PROB;
      params->gap_decay_rate = BLAST_GAP_DECAY_RATE;
   } else {
      params->gap_prob = BLAST_GAP_PROB_GAPPED;
      params->gap_decay_rate = BLAST_GAP_DECAY_RATE_GAPPED;
   }
   params->gap_size = BLAST_GAP_SIZE;
   params->overlap_size = BLAST_OVERLAP_SIZE;

   *link_hsp_params = params;
   return 0;
}

Int2 
BlastLinkHSPParametersUpdate(const BlastInitialWordParameters* word_params,
                             const BlastHitSavingParameters* hit_params,
                             Boolean gapped_calculation)
{
   if (!word_params || !hit_params)
      return -1;
   if (!hit_params->link_hsp_params)
      return 0;

   if (gapped_calculation) {
      /* FIXME, is this correct?? */
      hit_params->link_hsp_params->cutoff_small_gap = word_params->cutoff_score;
   } else {
      /* For all ungapped programs other than blastn, this value will be 
         recalculated anyway in CalculateLinkHSPCutoffs, but for blastn 
         this will be the final value. */
      hit_params->link_hsp_params->cutoff_small_gap = word_params->cutoff_score;
   }

   return 0;
}

BlastHitSavingParameters*
BlastHitSavingParametersFree(BlastHitSavingParameters* parameters)

{
   if (parameters) {
      sfree(parameters->link_hsp_params);
      sfree(parameters);
   }
   return NULL;
}


Int2
BlastHitSavingParametersNew(EBlastProgramType program_number, 
   const BlastHitSavingOptions* options, 
   BlastScoreBlk* sbp, BlastQueryInfo* query_info, 
   Int4 avg_subj_length,
   BlastHitSavingParameters* *parameters)
{
   Boolean gapped_calculation = TRUE;
   Int2 status = 0;
   BlastHitSavingParameters* params;
   Boolean do_sum_stats = FALSE;

   /* If parameters pointer is NULL, there is nothing to fill, 
      so don't do anything */
   if (!parameters)
      return 0;

   *parameters = NULL;

   ASSERT(options);
   ASSERT(sbp);
   
   if (!sbp->kbp_gap)
      gapped_calculation = FALSE;

   /* If sum statistics use is forced by the options, 
      set it in the paramters */
   if (options->do_sum_stats == eSumStatsTrue) {
      do_sum_stats = TRUE;
   } else if (options->do_sum_stats == eSumStatsNotSet) {
      /* By default, sum statistics is used for all translated searches 
       * (except RPS BLAST), and for all ungapped searches.
       */
      if (!gapped_calculation ||  
          (program_number == eBlastTypeBlastx) ||
          (program_number == eBlastTypeTblastn) ||
          (program_number == eBlastTypeTblastx))
         do_sum_stats = TRUE;
   }
  
   if (do_sum_stats && gapped_calculation && avg_subj_length <= 0)
       return 1;
       

   /* If parameters have not yet been created, allocate and fill all
      parameters that are constant throughout the search */
   *parameters = params = (BlastHitSavingParameters*) 
      calloc(1, sizeof(BlastHitSavingParameters));

   if (params == NULL)
      return 1;

   params->options = (BlastHitSavingOptions *) options;


   if (do_sum_stats) {
      BlastLinkHSPParametersNew(program_number, gapped_calculation,
                                &params->link_hsp_params);

      if(program_number == eBlastTypeBlastx  ||
         program_number == eBlastTypeTblastn) {
          /* The program may use Blast_UnevenGapLinkHSPs find significant
             collections of distinct alignments */
          Int4 max_protein_gap; /* the largest gap permitted in the
                               * translated sequence */

          max_protein_gap = (options->longest_intron - 2)/3;
          if(gapped_calculation) {
              if(options->longest_intron == 0) {
                  /* a zero value of longest_intron invokes the
                   * default behavior, which for gapped calculation is
                   * to set longest_intron to a predefined value. */
                  params->link_hsp_params->longest_intron =
                      (DEFAULT_LONGEST_INTRON - 2) / 3;
              } else if(max_protein_gap <= 0) {
                  /* A nonpositive value of max_protein_gap disables linking */
                  params->link_hsp_params =
                      BlastLinkHSPParametersFree(params->link_hsp_params);
              } else { /* the value of max_protein_gap is positive */
                  params->link_hsp_params->longest_intron = max_protein_gap;
              }
          } else { /* This is an ungapped calculation. */
              /* For ungapped calculations, we preserve the old behavior
               * of the longest_intron parameter to maintain
               * backward-compatibility with older versions of BLAST. */
              params->link_hsp_params->longest_intron =
                MAX(max_protein_gap, 0);
          }
      }
   }

   status = BlastHitSavingParametersUpdate(program_number, sbp, query_info, avg_subj_length, params);

   return status;
}

Int2
BlastHitSavingParametersUpdate(EBlastProgramType program_number, 
   BlastScoreBlk* sbp, BlastQueryInfo* query_info, 
   Int4 avg_subject_length, BlastHitSavingParameters* params)
{
   BlastHitSavingOptions* options;
   Blast_KarlinBlk* kbp;
   double scale_factor = sbp->scale_factor;
   Boolean gapped_calculation;
   Boolean do_sum_stats = FALSE;

   ASSERT(params);
   ASSERT(query_info);

   if (params->link_hsp_params)
        do_sum_stats = TRUE;

   options = params->options;

   /* Scoring options are not available here, but we can determine whether
      this is a gapped or ungapped search by checking whether gapped
      Karlin blocks have been set. */
   gapped_calculation = (sbp->kbp_gap != NULL);
   if (gapped_calculation) {
      if (s_BlastFindValidKarlinBlk(sbp->kbp_gap, query_info, &kbp) != 0)
         return -1;
   } else {
      if (s_BlastFindValidKarlinBlk(sbp->kbp, query_info, &kbp) != 0)
         return -1;
   }

   /* Calculate cutoffs based on the current effective lengths information */
   if (options->cutoff_score > 0) {
      params->cutoff_score_max = params->cutoff_score = options->cutoff_score * (Int4) sbp->scale_factor;
   } else if (!options->phi_align) {
      Int4 context = query_info->first_context;
      Int8 searchsp = query_info->contexts[context].eff_searchsp;
      double evalue = options->expect_value;

      /* translated RPS searches must scale the search space down */
      if (program_number == eBlastTypeRpsTblastn)   /* FIXME, why only rpstblastn?  */
         searchsp = searchsp / NUM_FRAMES;

      /* Get cutoff_score for specified evalue. */
      params->cutoff_score_max = 0;  
      BLAST_Cutoffs(&(params->cutoff_score_max), &evalue, kbp, searchsp, FALSE, 0);

      params->cutoff_score = 0;
      if (do_sum_stats && gapped_calculation)
      {
         double evalue_hsp=1.0;
         Int4 concat_qlen =
             query_info->contexts[query_info->last_context].query_offset +
             query_info->contexts[query_info->last_context].query_length - 1;
         Int4 avg_qlen = concat_qlen / (query_info->last_context + 1);
         BLAST_Cutoffs(&params->cutoff_score, &evalue_hsp, kbp, (Int8) MIN(avg_qlen, avg_subject_length)* (Int8)avg_subject_length, 
               TRUE, params->link_hsp_params->gap_decay_rate);
         /* Assure that cutoff_score for one HSP is no larger than cutoff_score for specified expect value. */
         params->cutoff_score = MIN(params->cutoff_score, params->cutoff_score_max);
      }
      else 
         params->cutoff_score = params->cutoff_score_max;
     
      params->cutoff_score *= (Int4) scale_factor;
      params->cutoff_score_max *= (Int4) scale_factor;
   } else {  /* phi-blast */
      params->cutoff_score = 0;
      params->cutoff_score_max = 0;
   }

   ASSERT(params->cutoff_score_max >= params->cutoff_score);

   return 0;
}

/** machine epsilon assumed by CalculateLinkHSPCutoffs */
#define MY_EPS 1.0e-9

/* FIXME, move to blast_engine.c and make private?  */
void
CalculateLinkHSPCutoffs(EBlastProgramType program, BlastQueryInfo* query_info, 
   BlastScoreBlk* sbp, BlastLinkHSPParameters* link_hsp_params, 
   const BlastInitialWordParameters* word_params,
   Int8 db_length, Int4 subject_length)
{
    double gap_prob, gap_decay_rate, x_variable, y_variable;
    Blast_KarlinBlk* kbp;
    Int4 expected_length, window_size, query_length;
    Int8 search_sp;
	Int4 concat_qlen;
    Boolean translated_subject = (program == eBlastTypeTblastn || 
                                  program == eBlastTypeRpsTblastn || 
                                  program == eBlastTypeTblastx);

    if (!link_hsp_params)
        return;

    /* Do this for the first context, should this be changed?? */
    kbp = sbp->kbp[query_info->first_context];
    window_size
        = link_hsp_params->gap_size + link_hsp_params->overlap_size + 1;
    gap_prob = link_hsp_params->gap_prob = BLAST_GAP_PROB;
    gap_decay_rate = link_hsp_params->gap_decay_rate;
    /* Use average query length */
    
    concat_qlen =
        query_info->contexts[query_info->last_context].query_offset +
        query_info->contexts[query_info->last_context].query_length - 1;
    
    query_length = concat_qlen / (query_info->last_context + 1);
    
    if (translated_subject) {
        /* Lengths in subsequent calculations should be on the protein scale */
        subject_length /= CODON_LENGTH;
        db_length /= CODON_LENGTH;
    }
    
    /* Subtract off the expected score. */
   expected_length = BLAST_Nint(log(kbp->K*((double) query_length)*
                                    ((double) subject_length))/(kbp->H));
   query_length = query_length - expected_length;

   subject_length = subject_length - expected_length;
   query_length = MAX(query_length, 1);
   subject_length = MAX(subject_length, 1);

   /* If this is a database search, use database length, else the single 
      subject sequence length */
   if (db_length > subject_length) {
      y_variable = log((double) (db_length)/(double) subject_length)*(kbp->K)/
         (gap_decay_rate);
   } else {
      y_variable = log((double) (subject_length + expected_length)/
                       (double) subject_length)*(kbp->K)/(gap_decay_rate);
   }
   search_sp = ((Int8) query_length)* ((Int8) subject_length);
   x_variable = 0.25*y_variable*((double) search_sp);

   /* To use "small" gaps the query and subject must be "large" compared to
      the gap size. If small gaps may be used, then the cutoff values must be
      adjusted for the "bayesian" possibility that both large and small gaps 
      are being checked for. */

   if (search_sp > 8*window_size*window_size) {
      x_variable /= (1.0 - gap_prob + MY_EPS);
      link_hsp_params->cutoff_big_gap = 
         (Int4) floor((log(x_variable)/kbp->Lambda)) + 1;
      x_variable = y_variable*(window_size*window_size);
      x_variable /= (gap_prob + MY_EPS);
      link_hsp_params->cutoff_small_gap = 
         MAX(word_params->cutoff_score, 
             (Int4) floor((log(x_variable)/kbp->Lambda)) + 1);
   } else {
      link_hsp_params->cutoff_big_gap = 
         (Int4) floor((log(x_variable)/kbp->Lambda)) + 1;
      /* The following is equivalent to forcing small gap rule to be ignored
         when linking HSPs. */
      link_hsp_params->gap_prob = 0;
      link_hsp_params->cutoff_small_gap = 0;
   }	

   link_hsp_params->cutoff_big_gap *= (Int4)sbp->scale_factor;
   link_hsp_params->cutoff_small_gap *= (Int4)sbp->scale_factor;
}


/*
 * ===========================================================================
 *
 * $Log$
 * Revision 1.3  2005/01/13 15:14:43  madden
 * Set params->cutoff_score_max to zero before calling BLASTCutoffs in BlastHitSavingParametersUpdate, prevents last value from overriding new calculation
 *
 * Revision 1.2  2005/01/10 13:22:34  madden
 * Add function s_GetBestExtensionMethod to decide on optimal extension method, also set container type based upon search type and query length
 *
 * Revision 1.1  2004/12/29 13:31:27  madden
 * New files for Blast parameters (separated from blast_options.[ch])
 *
 *
 * ===========================================================================
 */
