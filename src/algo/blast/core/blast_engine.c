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

File name: blast_engine.c

Author: Ilya Dondoshansky

Contents: High level BLAST functions

******************************************************************************/

static char const rcsid[] = "$Id$";

#include <blast_engine.h>
#include <aa_ungapped.h>
#include <blast_util.h>
#include <blast_gapalign.h>
#include <blast_traceback.h>

#if 0
extern OIDListPtr LIBCALL 
BlastGetVirtualOIDList PROTO((ReadDBFILEPtr rdfp_chain));
#endif

/** Deallocates all memory in BlastCoreAuxStruct */
static BlastCoreAuxStructPtr 
BlastCoreAuxStructFree(BlastCoreAuxStructPtr aux_struct)
{
   BlastExtendWordFree(aux_struct->ewp);
   BLAST_InitHitListDestruct(aux_struct->init_hitlist);
   BlastHSPListFree(aux_struct->hsp_list);
   MemFree(aux_struct->query_offsets);
   MemFree(aux_struct->subject_offsets);
   BlastSeqSrcFree(aux_struct->bssp);
   
   return (BlastCoreAuxStructPtr) MemFree(aux_struct);
}

/** Adjust HSP coordinates for out-of-frame gapped extension.
 * @param program One of blastx or tblastn [in]
 * @param init_hitlist List of hits after ungapped extension [in]
 * @param query_info Query information containing context offsets;
 *                   needed for blastx only [in]
 * @param subject_frame Frame of the subject sequence; tblastn only [in]
 * @param subject_length Length of the original nucleotide subject sequence;
 *                       tblastn only [in]
 */
void TranslateHSPsToDNAPCoord(Uint1 program, 
        BlastInitHitListPtr init_hitlist, BlastQueryInfoPtr query_info,
        Int2 subject_frame, Int4 subject_length)
{
   BlastInitHSPPtr init_hsp;
   Int4 index, context, frame;
   Int4Ptr context_offsets = query_info->context_offsets;
   
   for (index = 0; index < init_hitlist->total; ++index) {
      init_hsp = &init_hitlist->init_hsp_array[index];

      if (program == blast_type_blastx) {
         context = 
            BinarySearchInt4(init_hsp->q_off, context_offsets,
                             query_info->last_context+1);
         frame = context % 3;
      
         init_hsp->q_off = 
            (init_hsp->q_off - context_offsets[context]) * CODON_LENGTH + 
            context_offsets[context-frame] + frame;
         init_hsp->ungapped_data->q_start = 
            (init_hsp->ungapped_data->q_start - context_offsets[context]) 
            * CODON_LENGTH + context_offsets[context-frame] + frame;
      } else {
         if (subject_frame > 0) {
            init_hsp->s_off = 
               (init_hsp->s_off * CODON_LENGTH) + subject_frame - 1;
            init_hsp->ungapped_data->s_start = 
               (init_hsp->ungapped_data->s_start * CODON_LENGTH) + 
               subject_frame - 1;
         } else {
            init_hsp->s_off = (init_hsp->s_off * CODON_LENGTH) + 
               subject_length - subject_frame - 1;
            init_hsp->ungapped_data->s_start = 
               (init_hsp->ungapped_data->s_start * CODON_LENGTH) + 
               subject_length - subject_frame - 1;
         }
      }
   }
   return;
}

/** The core of the BLAST search: comparison between the (concatenated)
 * query against one subject sequence. Translation of the subject sequence
 * into 6 frames is done inside, if necessary. If subject sequence is 
 * too long, it can be split into several chunks. 
 */
static Int2
BLAST_SearchEngineCore(Uint1 program_number, BLAST_SequenceBlkPtr query, 
   BlastQueryInfoPtr query_info, BLAST_SequenceBlkPtr subject, 
   LookupTableWrapPtr lookup, BlastGapAlignStructPtr gap_align, 
   const BlastScoringOptionsPtr score_options, 
   BlastInitialWordParametersPtr word_params, 
   BlastExtensionParametersPtr ext_params, 
   BlastHitSavingParametersPtr hit_params, 
   const PSIBlastOptionsPtr psi_options, 
   const BlastDatabaseOptionsPtr db_options,
   BlastReturnStatPtr return_stats,
   BlastCoreAuxStructPtr aux_struct,
   BlastHSPListPtr PNTR hsp_list_out)
{
   BlastInitHitListPtr init_hitlist = aux_struct->init_hitlist;
   BlastHSPListPtr hsp_list = aux_struct->hsp_list;
   BLAST_ExtendWordPtr ewp = aux_struct->ewp;
   Uint4Ptr query_offsets = aux_struct->query_offsets;
   Uint4Ptr subject_offsets = aux_struct->subject_offsets;
   Uint1Ptr translation_buffer;
   Int4Ptr frame_offsets;
   Int4 num_chunks, chunk, total_subject_length, offset;
   BlastHitSavingOptionsPtr hit_options = hit_params->options;
   BlastHSPListPtr combined_hsp_list;
   Int2 status = 0;
   Boolean translated_subject;
   Int2 context, first_context, last_context;
   Int4 orig_length = subject->length, prot_length = 0;
   Uint1Ptr orig_sequence = subject->sequence;

   translated_subject = (program_number == blast_type_tblastn
                         || program_number == blast_type_tblastx
                         || program_number == blast_type_psitblastn);

   if (translated_subject) {
      first_context = 0;
      last_context = 5;
      if (score_options->is_ooframe) {
         BLAST_GetAllTranslations(orig_sequence, NCBI2NA_ENCODING,
            orig_length, db_options->gen_code_string, &translation_buffer,
            &frame_offsets, &subject->oof_sequence);
      } else {
         BLAST_GetAllTranslations(orig_sequence, NCBI2NA_ENCODING,
            orig_length, db_options->gen_code_string, &translation_buffer,
            &frame_offsets, NULL);
      }
   } else if (program_number == blast_type_blastn) {
      first_context = 1;
      last_context = 1;
   } else {
      first_context = 0;
      last_context = 0;
   }

   *hsp_list_out = NULL;

   /* Loop over frames of the subject sequence */
   for (context=first_context; context<=last_context; context++) {
      if (translated_subject) {
         subject->frame = BLAST_ContextToFrame(blast_type_blastx, context);
         subject->sequence = 
            translation_buffer + frame_offsets[context] + 1;
         subject->length = 
           frame_offsets[context+1] - frame_offsets[context] - 1;
      } else {
         subject->frame = context;
      }
     
      /* Split subject sequence into chunks if it is too long */
      num_chunks = (subject->length - DBSEQ_CHUNK_OVERLAP) / 
         (MAX_DBSEQ_LEN - DBSEQ_CHUNK_OVERLAP) + 1;
      offset = 0;
      total_subject_length = subject->length;
      combined_hsp_list = NULL;
      
      for (chunk = 0; chunk < num_chunks; ++chunk) {
         subject->length = MIN(total_subject_length - offset, 
                               MAX_DBSEQ_LEN);
         
         init_hitlist->total = 0;
         
         return_stats->db_hits +=
            aux_struct->WordFinder(subject, query, lookup, 
               gap_align->sbp->matrix, word_params, ewp, query_offsets, 
               subject_offsets, OFFSET_ARRAY_SIZE, init_hitlist);
            
         if (init_hitlist->total == 0)
            continue;
         
         return_stats->init_extends += init_hitlist->total;
         
         if (score_options->gapped_calculation) {
            if (score_options->is_ooframe) {
               /* Convert query offsets in all HSPs into the mixed-frame  
                  coordinates */
               TranslateHSPsToDNAPCoord(program_number, init_hitlist, 
                  query_info, subject->frame, orig_length);
               if (translated_subject) {
                  prot_length = subject->length;
                  subject->length = 2*orig_length + 1;
               }
            }

            aux_struct->GetGappedScore(program_number, query, subject, 
               gap_align, score_options, ext_params, hit_params, 
               init_hitlist, &hsp_list);
            if (score_options->is_ooframe && translated_subject)
               subject->length = prot_length;
         } else {
            BLAST_GetUngappedHSPList(init_hitlist, subject,
                                     hit_params->options, &hsp_list);
         }

         if (hsp_list->hspcnt == 0)
            continue;
         
         return_stats->good_init_extends += hsp_list->hspcnt;
         
         /* The subject ordinal id is not yet filled in this HSP list */
         hsp_list->oid = subject->oid;
         
         /* Multiple contexts - adjust all HSP offsets to the individual 
            query coordinates; also assign frames, except in case of
            out-of-frame gapping. */
         BLAST_AdjustQueryOffsets(program_number, hsp_list, query_info, 
                                  score_options->is_ooframe);
         
#ifdef DO_LINK_HSPS
         if (hit_options->do_sum_stats == TRUE)
            status = BLAST_LinkHsps(hsp_list);
         else
#endif
            /* Calculate e-values for all HSPs */
            status = 
               BLAST_GetNonSumStatsEvalue(program_number, query_info, 
                  hsp_list, hit_options, gap_align->sbp);
         
         /* Discard HSPs that don't pass the e-value test */
         status = BLAST_ReapHitlistByEvalue(hsp_list, hit_options);
         
         AdjustOffsetsInHSPList(hsp_list, offset);
         /* Allow merging of HSPs either if traceback is already 
            available, or if it is an ungapped search */
         if (MergeHSPLists(hsp_list, &combined_hsp_list, offset,
             (hsp_list->traceback_done || !hit_options->is_gapped), FALSE)) {
            /* HSPs from this list are moved elsewhere, reset count to 0 */
            hsp_list->hspcnt = 0;
         }
         offset += subject->length - DBSEQ_CHUNK_OVERLAP;
         subject->sequence += 
            (subject->length - DBSEQ_CHUNK_OVERLAP)/COMPRESSION_RATIO;
      } /* End loop on chunks of subject sequence */
      
      MergeHSPLists(combined_hsp_list, hsp_list_out, 0, 
                    FALSE, TRUE);
   } /* End loop on frames */
      
   /* Restore the original contents of the subject block */
   subject->length = orig_length;
   subject->sequence = orig_sequence;

   return status;
}

static Int2 
FillReturnXDropoffsInfo(BlastReturnStatPtr return_stats, 
   BlastInitialWordParametersPtr word_params, 
   BlastExtensionParametersPtr ext_params)
{
   if (!return_stats)
      return -1;

   return_stats->x_drop_ungapped = word_params->x_dropoff;
   return_stats->x_drop_gap = ext_params->gap_x_dropoff;
   return_stats->x_drop_gap_final = ext_params->gap_x_dropoff_final;
   return_stats->gap_trigger = ext_params->gap_trigger;

   return 0;
}

Int2 BLAST_CalcEffLengths (Uint1 program_number, 
   const BlastScoringOptionsPtr scoring_options,
   const BlastEffectiveLengthsOptionsPtr eff_len_options, 
   const BLAST_ScoreBlkPtr sbp, BlastQueryInfoPtr query_info)
{
   Nlm_FloatHi alpha, beta; /*alpha and beta for new scoring system */
   Int4 min_query_length;   /* lower bound on query length. */
   Int4 length_adjustment;  /* length adjustment for current iteration. */
   Int4 last_length_adjustment;/* length adjustment in previous iteration.*/
   Int4 index;		/* loop index. */
   Int4	db_num_seqs;	/* number of sequences in database. */
   Int8	db_length;	/* total length of database. */
   BLAST_KarlinBlkPtr *kbp_ptr; /* Array of Karlin block pointers */
   BLAST_KarlinBlkPtr kbp; /* Karlin-Blk pointer from ScoreBlk. */
   Int4 query_length;   /* length of an individual query sequence */
   Int8 effective_length, effective_db_length; /* effective lengths of 
                                                  query and database */
   Int8 effective_search_space; /* Effective search space for a given 
                                   sequence/strand/frame */
   Int2 i; /* Iteration index for calculating length adjustment */
   Uint1 num_strands;

   if (sbp == NULL || eff_len_options == NULL)
      return 1;

   /* use values in BlastEffectiveLengthsOptionsPtr */
   db_length = eff_len_options->db_length;
   if (program_number == blast_type_tblastn || 
       program_number == blast_type_tblastx)
      db_length = db_length/3;	
   
   db_num_seqs = eff_len_options->dbseq_num;
   
   if (program_number != blast_type_blastn) {
      if (scoring_options->gapped_calculation) {
         getAlphaBeta(sbp->name,&alpha,&beta,TRUE, 
            scoring_options->gap_open, scoring_options->gap_extend);
      }
   }
   
   if (scoring_options->gapped_calculation && 
       program_number != blast_type_blastn) 
      kbp_ptr = sbp->kbp_gap_std;
   else
      kbp_ptr = sbp->kbp_std; 
   
   if (program_number == blast_type_blastn)
      num_strands = 2;
   else
      num_strands = 1;

   for (index = query_info->first_context; 
        index <= query_info->last_context; ) {
      if (eff_len_options->searchsp_eff) {
         effective_search_space = eff_len_options->searchsp_eff;
      } else {
         query_length = BLAST_GetQueryLength(query_info, index);
         /* Use the correct Karlin block. For blastn, two identical Karlin
            blocks are allocated for each sequence (one per strand), but we
            only need one of them.
         */
         kbp = kbp_ptr[index];
         length_adjustment = 0;
         last_length_adjustment = 0;
         
         min_query_length = (Int4) (1/(kbp->K));

         for (i=0; i<5; i++) {
            if (program_number != blast_type_blastn && 
                scoring_options->gapped_calculation) {
               length_adjustment = Nlm_Nint((((kbp->logK)+log((Nlm_FloatHi)(query_length-last_length_adjustment)*(Nlm_FloatHi)MAX(db_num_seqs, db_length-db_num_seqs*last_length_adjustment)))*alpha/kbp->Lambda) + beta);
            } else {
               length_adjustment = (Int4) ((kbp->logK+log((Nlm_FloatHi)(query_length-last_length_adjustment)*(Nlm_FloatHi)MAX(1, db_length-db_num_seqs*last_length_adjustment)))/(kbp->H));
            }

            if (length_adjustment >= query_length-min_query_length) {
               length_adjustment = query_length-min_query_length;
               break;
            }
            
            if (ABS(last_length_adjustment-length_adjustment) <= 1)
               break;
            last_length_adjustment = length_adjustment;
         }
         
         effective_length = 
            MAX(query_length - length_adjustment, min_query_length);
         effective_db_length = MAX(1, db_length - db_num_seqs*length_adjustment);
         
         effective_search_space = effective_length * effective_db_length;
      }
      for (i = 0; i < num_strands; ++i) {
         query_info->eff_searchsp_array[index] = effective_search_space;
         ++index;
      }
   }

   return 0;
}

/** Setup of the auxiliary BLAST structures; 
 * also calculates internally used parameters from options. 
 * @param program_number blastn, blastp, blastx, etc. [in]
 * @param scoring_options options for scoring. [in]
 * @param eff_len_options  used to calc. eff len. [in]
 * @param lookup_wrap Lookup table, already constructed. [in]
 * @param word_options options for initial word finding. [in]
 * @param ext_options options for gapped extension. [in]
 * @param hit_options options for saving hits. [in]
 * @param query The query sequence block [in]
 * @param query_info The query information block [in]
 * @param sbp Contains scoring information. [in]
 * @param subject_length Length of the subject sequence in two 
 *                       sequences case [in]
 * @param gap_align Gapped alignment information and allocated memory [out]
 * @param word_params Parameters for initial word processing [out]
 * @param ext_params Parameters for gapped extension [out]
 * @param hit_params Parameters for saving hits [out]
 * @param aux_struct_ptr Placeholder joining various auxiliary memory 
 *                       structures [out]
 */
static Int2 
BLAST_SetUpAuxStructures(Uint1 program_number,
   const BlastSeqSrcNewInfoPtr bssn_info,
   const BlastScoringOptionsPtr scoring_options,
   const BlastEffectiveLengthsOptionsPtr eff_len_options,
   LookupTableWrapPtr lookup_wrap,	
   const BlastInitialWordOptionsPtr word_options,
   const BlastExtensionOptionsPtr ext_options,
   const BlastHitSavingOptionsPtr hit_options,
   BLAST_SequenceBlkPtr query, BlastQueryInfoPtr query_info, 
   BLAST_ScoreBlkPtr sbp, Uint4 subject_length, 
   BlastGapAlignStructPtr PNTR gap_align, 
   BlastInitialWordParametersPtr PNTR word_params,
   BlastExtensionParametersPtr PNTR ext_params,
   BlastHitSavingParametersPtr PNTR hit_params,
   BlastCoreAuxStructPtr PNTR aux_struct_ptr)
{
   Int2 status = 0;
   Boolean blastp = (lookup_wrap->lut_type == AA_LOOKUP_TABLE);
   Boolean mb_lookup = (lookup_wrap->lut_type == MB_LOOKUP_TABLE);
   Boolean ag_blast = (Boolean)
      (word_options->extend_word_method & EXTEND_WORD_AG);
   Int4 offset_array_size = 0;
   BLAST_ExtendWordPtr ewp;
   BlastCoreAuxStructPtr aux_struct;
   Uint4 max_subject_length;

   *aux_struct_ptr = aux_struct = (BlastCoreAuxStructPtr)
      MemNew(sizeof(BlastCoreAuxStruct));

   /* Initialize the BlastSeqSrc */
   if (bssn_info) {
      aux_struct->bssp = BlastSeqSrcNew(bssn_info);
      max_subject_length = BLASTSeqSrcGetMaxSeqLen(aux_struct->bssp);
   } else {
      max_subject_length = subject_length;
   }

   if ((status = 
      BLAST_ExtendWordInit(query, word_options, eff_len_options->db_length, 
                           eff_len_options->dbseq_num, &ewp)) != 0)
      return status;
      
   if ((status = BLAST_CalcEffLengths(program_number, scoring_options, 
                    eff_len_options, sbp, query_info)) != 0)
      return status;

   BlastExtensionParametersNew(program_number, ext_options, sbp, query_info, 
                               ext_params);

   BlastHitSavingParametersNew(program_number, hit_options, NULL, sbp, 
                               query_info, hit_params);

   BlastInitialWordParametersNew(program_number, word_options, *hit_params, 
      *ext_params, sbp, query_info, eff_len_options, word_params);

   if ((status = BLAST_GapAlignStructNew(scoring_options, *ext_params, 1, 
                    max_subject_length, query->length, program_number, sbp,
                    gap_align))) {
      return status;
   }

   aux_struct->ewp = ewp;

   /* pick which gapped alignment algorithm to use */
   if (ext_options->algorithm_type == EXTEND_DYN_PROG)
     aux_struct->GetGappedScore = BLAST_GetGappedScore;
   else
     aux_struct->GetGappedScore = BLAST_MbGetGappedScore;

   if (mb_lookup) {
      aux_struct->WordFinder = MB_WordFinder;
      offset_array_size = OFFSET_ARRAY_SIZE + 
         ((MBLookupTablePtr)lookup_wrap->lut)->longest_chain;
   } else {
      if (blastp) {
         aux_struct->WordFinder = BlastAaWordFinder;
      } else if (ag_blast) {
         aux_struct->WordFinder = BlastNaWordFinder_AG;
      } else {
         aux_struct->WordFinder = BlastNaWordFinder;
      }
      offset_array_size = OFFSET_ARRAY_SIZE + 
         ((LookupTablePtr)lookup_wrap->lut)->longest_chain;
   }

   aux_struct->query_offsets = 
      (Uint4Ptr) Malloc(offset_array_size * sizeof(Uint4));
   aux_struct->subject_offsets = 
      (Uint4Ptr) Malloc(offset_array_size * sizeof(Uint4));

   aux_struct->init_hitlist = BLAST_InitHitListNew();
   aux_struct->hsp_list = BlastHSPListNew();

   return status;
}

#if THREADS_IMPLEMENTED
static Boolean 
BLAST_GetDbChunk(ReadDBFILEPtr rdfp, Int4Ptr start, Int4Ptr stop, 
   Int4Ptr id_list, Int4Ptr id_list_number, BlastThrInfoPtr thr_info)
{
    Boolean done=FALSE;
    OIDListPtr virtual_oidlist = NULL;
    *id_list_number = 0;
    
    NlmMutexLockEx(&thr_info->db_mutex);
    if (thr_info->realdb_done) {
        if ((virtual_oidlist = BlastGetVirtualOIDList(rdfp))) {
           /* Virtual database.   Create id_list using mask file */
           Int4 gi_end       = 0;
	    
           thr_info->final_db_seq = 
              MIN(thr_info->final_db_seq, virtual_oidlist->total);
	    
           gi_end = thr_info->final_db_seq;

           if (thr_info->oid_current < gi_end) {
              Int4 oidindex  = 0;
              Int4 gi_start  = thr_info->oid_current;
              Int4 bit_start = gi_start % MASK_WORD_SIZE;
              Int4 gi;
              
              for(gi = gi_start; (gi < gi_end) && 
                     (oidindex < thr_info->db_chunk_size);) {
                 Int4 bit_end = ((gi_end - gi) < MASK_WORD_SIZE) ? 
                    (gi_end - gi) : MASK_WORD_SIZE;
                 Int4 bit;
                 
                 Uint4 mask_index = gi / MASK_WORD_SIZE;
                 Uint4 mask_word  = 
                    Nlm_SwapUint4(virtual_oidlist->list[mask_index]);
                 
                 if ( mask_word ) {
                    for(bit = bit_start; bit<bit_end; bit++) {
                       Uint4 bitshift = (MASK_WORD_SIZE-1)-bit;
                       
                       if ((mask_word >> bitshift) & 1) {
                          id_list[ oidindex++ ] = (gi - bit_start) + bit;
                       }
                    }
                 }
                 
                 gi += bit_end - bit_start;
                 bit_start = 0;
              }
              
              thr_info->oid_current = gi;
              *id_list_number = oidindex;
           } else {
              done = TRUE;
           }
        } else {
           done = TRUE;
        }
    } else {
       int real_readdb_entries;
       int total_readdb_entries;
       int final_real_seq;
       
       real_readdb_entries  = readdb_get_num_entries_total_real(rdfp);
       total_readdb_entries = readdb_get_num_entries_total(rdfp);
       final_real_seq = MIN( real_readdb_entries, thr_info->final_db_seq );
       
       /* we have real database with start/stop specified */
       if (thr_info->db_mutex) {
          *start = thr_info->db_chunk_last;
          if (thr_info->db_chunk_last < final_real_seq) {
             *stop = MIN((thr_info->db_chunk_last + 
                          thr_info->db_chunk_size), final_real_seq);
          } else {/* Already finished. */
             *stop = thr_info->db_chunk_last;
             
             /* Change parameters for oidlist processing. */
             thr_info->realdb_done  = TRUE;
          }
          thr_info->db_chunk_last = *stop;
       } else {
          if (*stop != final_real_seq) {
             done = FALSE;
             *start = thr_info->last_db_seq;
             *stop  = final_real_seq;
          } else {
             thr_info->realdb_done = TRUE;
             
             if (total_readdb_entries == real_readdb_entries) {
                done = TRUE;
             } else {
                thr_info->oid_current = final_real_seq;
             }
          }
       }
    }
    
    NlmMutexUnlock(thr_info->db_mutex);
    return done;
}

#define BLAST_DB_CHUNK_SIZE 1024
static BlastThrInfoPtr BLAST_ThrInfoNew(ReadDBFILEPtr rdfp)
{
   BlastThrInfoPtr thr_info;
   
   thr_info = MemNew(sizeof(BlastThrInfo));
   thr_info->db_chunk_size = BLAST_DB_CHUNK_SIZE;
   thr_info->final_db_seq = readdb_get_num_entries_total(rdfp);
   
   return thr_info;
}
#endif

#define BLAST_DB_CHUNK_SIZE 1024
static BlastThrInfoPtr BLAST_ThrInfoNew(Int4 last_oid2search)
{
   BlastThrInfoPtr thr_info;
   
   thr_info = (BlastThrInfoPtr) MemNew(sizeof(BlastThrInfo));
   thr_info->db_chunk_size = BLAST_DB_CHUNK_SIZE;
   thr_info->final_db_seq = last_oid2search;
   
   return thr_info;
}

static void BLAST_ThrInfoFree(BlastThrInfoPtr thr_info)
{
    if (thr_info == NULL)
	return;
#if THREADS_IMPLEMENTED
    NlmMutexDestroy(thr_info->db_mutex);
    NlmMutexDestroy(thr_info->results_mutex);
    NlmMutexDestroy(thr_info->callback_mutex);
#endif
    MemFree(thr_info);
    
    return;
}

Int4 
BLAST_DatabaseSearchEngine(Uint1 program_number, 
   const BLAST_SequenceBlkPtr query, const BlastQueryInfoPtr query_info,
   const BlastSeqSrcNewInfoPtr bssn_info,  BLAST_ScoreBlkPtr sbp,
   const BlastScoringOptionsPtr score_options, 
   LookupTableWrapPtr lookup_wrap,
   const BlastInitialWordOptionsPtr word_options, 
   const BlastExtensionOptionsPtr ext_options, 
   const BlastHitSavingOptionsPtr hit_options,
   const BlastEffectiveLengthsOptionsPtr eff_len_options,
   const PSIBlastOptionsPtr psi_options, 
   const BlastDatabaseOptionsPtr db_options,
   BlastResultsPtr results, BlastReturnStatPtr return_stats)
{
   BlastCoreAuxStructPtr aux_struct = NULL;
   BlastThrInfoPtr thr_info = NULL;
   Int4 oid; /* Subject ordinal id in the database */
   Int4Ptr oid_list = NULL;
   Int4 start = 0, stop = 0, oid_list_length = 0, index;
   Boolean use_oid_list = FALSE;
   BlastHSPListPtr hsp_list; 
   BlastInitialWordParametersPtr word_params;
   BlastExtensionParametersPtr ext_params;
   BlastHitSavingParametersPtr hit_params;
   BlastGapAlignStructPtr gap_align;
   GetSeqArg seq_arg = { NULLB };
   Int2 status = 0;
   
   if ((status = 
       BLAST_SetUpAuxStructures(program_number, bssn_info,
          score_options, eff_len_options, lookup_wrap, word_options, 
          ext_options, hit_options, query, query_info, sbp, 
          0, &gap_align, &word_params, &ext_params, 
          &hit_params, &aux_struct)) != 0)
      return status;

   FillReturnXDropoffsInfo(return_stats, word_params, ext_params);

   /* FIXME: will only work for full databases */
   /*thr_info = BLAST_ThrInfoNew(eff_len_options->dbseq_num);*/
   stop = eff_len_options->dbseq_num;

#if THREADS_IMPLEMENTED
   while (!BLAST_GetDbChunk(rdfp, &start, &stop, oid_list, 
                            &oid_list_length, thr_info)) {
      use_oid_list = (oid_list && (oid_list_length > 0));
      if (use_oid_list) {
         start = 0;
         stop = oid_list_length;
      }
#endif
   
   /* iterate over all subject sequences */
   for (index = start; index < stop; index++) {
      seq_arg.oid = (use_oid_list ? oid_list[index] : index);
      seq_arg.encoding = BLASTP_ENCODING; /* shouldn't use a different encoding?*/
      BlastSequenceBlkClean(seq_arg.seq);
      if (BLASTSeqSrcGetSequence(aux_struct->bssp, (void*) &seq_arg) < 0)
          continue;
 
      BLAST_SearchEngineCore(program_number, query, query_info,
         seq_arg.seq, lookup_wrap, gap_align, score_options, word_params, 
         ext_params, hit_params, psi_options, db_options,
         return_stats, aux_struct, &hsp_list);
 
      if (hsp_list && hsp_list->hspcnt > 0) {
         return_stats->prelim_gap_passed += hsp_list->hspcnt;
         /* Save the HSPs into a hit list */
         BLAST_SaveHitlist(program_number, query, seq_arg.seq, results, 
            hsp_list, hit_params, query_info, gap_align->sbp, 
            score_options, aux_struct->bssp, thr_info);
      }
         /*BlastSequenceBlkClean(subject);*/
   }
#if THREADS_IMPLEMENTED
   }    /* end while!(BLAST_GetDbChunk...) */
#endif

   BLAST_ThrInfoFree(thr_info); /* CC: Is this really needed? */
   oid_list = (Int4Ptr) MemFree(oid_list);
   BlastSequenceBlkFree(seq_arg.seq);

   /* Now sort the hit lists for all queries */
   BLAST_SortResults(results);

   if (hit_options->is_gapped) {
      status = 
         BLAST_ComputeTraceback(program_number, results, query, query_info,
            aux_struct->bssp, gap_align, score_options, ext_params, hit_params,
            db_options);
   }

   /* Do not destruct score block here */
   gap_align->sbp = NULL;
   BLAST_GapAlignStructFree(gap_align);
   BlastCoreAuxStructFree(aux_struct);

   hit_params = (BlastHitSavingParametersPtr) MemFree(hit_params);
   ext_params = (BlastExtensionParametersPtr) MemFree(ext_params);
   word_params = (BlastInitialWordParametersPtr) MemFree(word_params);

   return status;
}

Int4 
BLAST_TwoSequencesEngine(Uint1 program_number, 
   const BLAST_SequenceBlkPtr query, const BlastQueryInfoPtr query_info, 
   const BLAST_SequenceBlkPtr subject, 
   BLAST_ScoreBlkPtr sbp, const BlastScoringOptionsPtr score_options, 
   LookupTableWrapPtr lookup_wrap,
   const BlastInitialWordOptionsPtr word_options, 
   const BlastExtensionOptionsPtr ext_options, 
   const BlastHitSavingOptionsPtr hit_options, 
   const BlastEffectiveLengthsOptionsPtr eff_len_options,
   const PSIBlastOptionsPtr psi_options, 
   const BlastDatabaseOptionsPtr db_options,
   BlastResultsPtr results, BlastReturnStatPtr return_stats)
{
   BlastCoreAuxStructPtr aux_struct = NULL;
   BlastHSPListPtr hsp_list; 
   BlastHitSavingParametersPtr hit_params; 
   BlastInitialWordParametersPtr word_params; 
   BlastExtensionParametersPtr ext_params;
   BlastGapAlignStructPtr gap_align;
   Int2 status = 0;

   if (!subject)
      return -1;

   if ((status = 
        BLAST_SetUpAuxStructures(program_number, NULL, score_options, 
           eff_len_options, lookup_wrap, word_options, ext_options, 
           hit_options, query, query_info, 
           sbp, subject->length, &gap_align, &word_params, &ext_params, 
           &hit_params, &aux_struct)) != 0)
      return status;

   FillReturnXDropoffsInfo(return_stats, word_params, ext_params);

   BLAST_SearchEngineCore(program_number, query, query_info, subject, 
      lookup_wrap, gap_align, score_options, word_params, ext_params, 
      hit_params, psi_options, db_options, return_stats, aux_struct, 
      &hsp_list);

   if (hsp_list && hsp_list->hspcnt > 0) {
      return_stats->prelim_gap_passed += hsp_list->hspcnt;
      /* Save the HSPs into a hit list */
      BLAST_SaveHitlist(program_number, query, subject, results, 
         hsp_list, hit_params, query_info, gap_align->sbp, 
         score_options, NULL, NULL);
   }
   BlastCoreAuxStructFree(aux_struct);
   word_params = (BlastInitialWordParametersPtr) MemFree(word_params);

   if (hit_options->is_gapped) {
      status = 
         BLAST_TwoSequencesTraceback(program_number, results, query, 
            query_info, subject, gap_align, score_options, ext_params, 
            hit_params, db_options);
   }

   /* Do not destruct score block here */
   gap_align->sbp = NULL;
   BLAST_GapAlignStructFree(gap_align);

   ext_params = (BlastExtensionParametersPtr) MemFree(ext_params);
   hit_params = (BlastHitSavingParametersPtr) MemFree(hit_params);

   return status;
}

Int2 LookupTableWrapInit(BLAST_SequenceBlkPtr query, 
        const LookupTableOptionsPtr lookup_options,	
        ValNodePtr lookup_segments, BLAST_ScoreBlkPtr sbp, 
        LookupTableWrapPtr PNTR lookup_wrap_ptr)
{
   LookupTableWrapPtr lookup_wrap;

   /* Construct the lookup table. */
   *lookup_wrap_ptr = lookup_wrap = 
      (LookupTableWrapPtr) MemNew(sizeof(LookupTableWrap));
   lookup_wrap->lut_type = lookup_options->lut_type;

   switch ( lookup_options->lut_type ) {
   case AA_LOOKUP_TABLE:
      BlastAaLookupNew(lookup_options, (LookupTablePtr *)
                       &lookup_wrap->lut);
      BlastAaLookupIndexQueries( (LookupTablePtr) lookup_wrap->lut,
                                 sbp->matrix, query, lookup_segments, 1);
      _BlastAaLookupFinalize((LookupTablePtr) lookup_wrap->lut);
      break;
   case MB_LOOKUP_TABLE:
      MB_LookupTableNew(query, lookup_segments, 
         (MBLookupTablePtr *) &(lookup_wrap->lut), lookup_options);
      break;
   case NA_LOOKUP_TABLE:
      LookupTableNew(lookup_options, 
         (LookupTablePtr *) &(lookup_wrap->lut), FALSE);
	    
      BlastNaLookupIndexQuery((LookupTablePtr) lookup_wrap->lut, query,
                              lookup_segments);
      _BlastAaLookupFinalize((LookupTablePtr) lookup_wrap->lut);
      break;
   default:
      {
         /* FIXME - emit error condition here */
      }
   } /* end switch */

   return 0;
}
