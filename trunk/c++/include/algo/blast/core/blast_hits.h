/* $Id$
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
 * Author:  Ilya Dondoshansky
 *
 */

/** @file blast_hits.h
 * Structures and API used for saving BLAST hits
 */

#ifndef __BLAST_HITS__
#define __BLAST_HITS__

#include <algo/blast/core/blast_options.h>
#include <algo/blast/core/gapinfo.h>
#include <algo/blast/core/blast_seqsrc.h>

#ifdef __cplusplus
extern "C" {
#endif

/** One sequence segment within an HSP */
typedef struct BlastSeg {
   Int2 frame;  /**< Translation frame */
   Int4 offset; /**< Start of hsp */
   Int4 length; /**< Length of hsp */
   Int4 end;    /**< End of HSP */
   Int4 gapped_start;/**< Where the gapped extension started. */
} BlastSeg;

/** Structure holding all information about an HSP */
typedef struct BlastHSP {
   Int4 score;           /**< This HSP's raw score */
   Int4 num_ident;       /**< Number of identical base pairs in this HSP */
   double bit_score;     /**< Bit score, calculated from score */
   double evalue;        /**< This HSP's e-value */
   BlastSeg query;       /**< Query sequence info. */
   BlastSeg subject;     /**< Subject sequence info. */
   Int4     context;     /**< Context number of query */
   GapEditBlock* gap_info;/**< ALL gapped alignment is here */
   Int4 num;             /**< How many HSP's make up this (sum) segment? */
   Uint4 pattern_length; /**< Length of pattern occurrence in this HSP, in 
                            PHI BLAST */
} BlastHSP;

/** The structure to hold all HSPs for a given sequence after the gapped 
 *  alignment.
 */
typedef struct BlastHSPList {
   Int4 oid;/**< The ordinal id of the subject sequence this HSP list is for */
   Int4 query_index; /**< Index of the query which this HSPList corresponds to.
                        Set to 0 if not applicable */
   BlastHSP** hsp_array; /**< Array of pointers to individual HSPs */
   Int4 hspcnt; /**< Number of HSPs saved */
   Int4 allocated; /**< The allocated size of the hsp_array */
   Int4 hsp_max; /**< The maximal number of HSPs allowed to be saved */
   Boolean do_not_reallocate; /**< Is reallocation of the hsp_array allowed? */
} BlastHSPList;

/** The structure to contain all BLAST results for one query sequence */
typedef struct BlastHitList {
   Int4 hsplist_count; /**< Filled size of the HSP lists array */
   Int4 hsplist_max; /**< Maximal allowed size of the HSP lists array */
   double worst_evalue; /**< Highest of the best e-values among the HSP 
                           lists */
   Int4 low_score; /**< The lowest of the best scores among the HSP lists */
   Boolean heapified; /**< Is this hit list already heapified? */
   BlastHSPList** hsplist_array; /**< Array of HSP lists for individual
                                          database hits */
} BlastHitList;

/** The structure to contain all BLAST results, for multiple queries */
typedef struct BlastHSPResults {
   Int4 num_queries; /**< Number of query sequences */
   BlastHitList** hitlist_array; /**< Array of results for individual
                                          query sequences */
} BlastHSPResults;


/** By how much should the chunks of a subject sequence overlap if it is 
    too long and has to be split */
#define DBSEQ_CHUNK_OVERLAP 100

/********************************************************************************

The following section has four sets of functions (or "APIs"), manipulating with
the following structures:
1. BlastHSP, which is the basic unit to record one alignment.  
2. BlastHSPList, which is a list of BlastHSP's for one database sequence. 
3. BlastHitList, which contains all HSPList's for a given query. 
4. BlastHSPResults, which is a list of BlastHitList's for multiple queries.

 The naming conventions for the functions are the following:

1.) All routines start with "Blast_"

2.) After "Blast_" comes the structure being manipulated, that should be either 
    HSP (all capitals all the time!), HSPList (exactly this capitalization), 
    HitList (capital H and L, all others lower-case), or HSPResults.

3.) finally the task being done, e.g., "Free", "New", "Init".

********************************************************************************/
/********************************************************************************
          HSP API
********************************************************************************/

/** Deallocate memory for an HSP structure */
NCBI_XBLAST_EXPORT
BlastHSP* Blast_HSPFree(BlastHSP* hsp);

/** Allocate and zeros out memory for an HSP structure */
NCBI_XBLAST_EXPORT
BlastHSP* Blast_HSPNew(void);

/** Allocates BlastHSP and inits with information from input.
 * structure
 * @param query_start Start of query alignment [in]
 * @param query_end End of query alignment [in]
 * @param subject_start Start of subject alignment [in]
 * @param subject_end End of subject alignment [in]
 * @param query_gapped_start Where gapped alignment started on query [in]
 * @param subject_gapped_start Where gapped alignment started on subject [in]
 * @param query_context The index of the query containing this HSP [in]
 * @param subject_frame Subject frame: -3..3 for translated sequence, 
 *        1 for blastn, 0 for blastp [in]
 * @param score score of alignment [in]
 * @param gap_edit Will be transferred to HSP and nulled out 
 *    if a traceback was not calculated may be NULL [in] [out]
 * @param ret_hsp allocated and filled in BlastHSP [out]
 */
NCBI_XBLAST_EXPORT
Int2
Blast_HSPInit(Int4 query_start, Int4 query_end, Int4 subject_start, 
              Int4 subject_end,
              Int4 query_gapped_start, Int4 subject_gapped_start,
              Int4 query_context, Int2 subject_frame, Int4 score,
              GapEditBlock* *gap_edit, BlastHSP** ret_hsp);


/** Modifies the HSP data after the final gapped alignment.
 *   Input includes only data that likely needs modification.
 * @param query_start start of alignment on query [in]
 * @param query_end end of alignment on query [in]
 * @param subject_start start of alignment on subject [in]
 * @param subject_end end of alignment on subject [in]
 * @param score New score for this HSP [in]
 * @param gap_edit traceback from final gapped alignment [in] [out]
 * @param hsp Original HSP from the preliminary stage [in] [out]
 */
NCBI_XBLAST_EXPORT
Int2
Blast_HSPReset(Int4 query_start, Int4 query_end, Int4 subject_start, 
               Int4 subject_end, Int4 score, GapEditBlock* *gap_edit, 
               BlastHSP* hsp);

/** Calculate e-value for an HSP found by PHI BLAST.
 * @param hsp An HSP found by PHI BLAST [in]
 * @param sbp Scoring block with statistical parameters [in]
 */
NCBI_XBLAST_EXPORT
void Blast_HSPPHIGetEvalue(BlastHSP* hsp, BlastScoreBlk* sbp);

/** Reevaluate the HSP's score, e-value and percent identity after taking
 * into account the ambiguity information. Needed for blastn only, either
 * after a greedy gapped extension, or for ungapped search.
 * @param hsp The HSP structure [in] [out]
 * @param query_start Pointer to the start of the query sequence [in]
 * @param subject_start Pointer to the start of the subject sequence [in]
 * @param hit_options Hit saving options with e-value cut-off [in]
 * @param score_params Scoring parameters [in]
 * @param query_info Query information structure, containing effective search
 *                   space(s) [in]
 * @param sbp Score block with Karlin-Altschul parameters [in]
 * @return Should this HSP be deleted after the score reevaluation?
 */
NCBI_XBLAST_EXPORT
Boolean Blast_HSPReevaluateWithAmbiguities(BlastHSP* hsp, 
           Uint1* query_start, Uint1* subject_start, 
           const BlastHitSavingOptions* hit_options, 
           const BlastScoringParameters* score_params, 
           BlastQueryInfo* query_info, BlastScoreBlk* sbp);

/** Calculate number of identities in an HSP.
 * @param query The query sequence [in]
 * @param subject The uncompressed subject sequence [in]
 * @param hsp All information about the HSP [in]
 * @param is_gapped Is this a gapped search? [in]
 * @param num_ident_ptr Number of identities [out]
 * @param align_length_ptr The alignment length, including gaps [out]
 */
NCBI_XBLAST_EXPORT
Int2
Blast_HSPGetNumIdentities(Uint1* query, Uint1* subject, BlastHSP* hsp, 
                          Boolean is_gapped, Int4* num_ident_ptr, 
                          Int4* align_length_ptr);

/** Calculate number of identities in an HSP for an out-of-frame alignment.
 * @param query The query sequence [in]
 * @param subject The uncompressed subject sequence [in]
 * @param hsp All information about the HSP [in]
 * @param program BLAST program (blastx or tblastn) [in]
 * @param num_ident_ptr Number of identities [out]
 * @param align_length_ptr The alignment length, including gaps [out]
 */
NCBI_XBLAST_EXPORT
Int2
Blast_HSPGetOOFNumIdentities(Uint1* query, Uint1* subject, BlastHSP* hsp, 
                             EBlastProgramType program, Int4* num_ident_ptr, 
                             Int4* align_length_ptr);

/** Calculate length of an HSP as length in query plus length of gaps in 
 * query. If gap information is unavailable, return maximum between length in
 * query and in subject.
 * @param hsp An HSP structure [in]
 * @param length Length of this HSP [out]
 * @param gaps Total number of gaps in this HSP [out]
 * @param gap_opens Number of gap openings in this HSP [out] 
 */
NCBI_XBLAST_EXPORT
void Blast_HSPCalcLengthAndGaps(BlastHSP* hsp, Int4* length,
                                Int4* gaps, Int4* gap_opens);

/** Adjust HSP endpoint offsets according to strand/frame; return values in
 * 1-offset coordinates instead of internal 0-offset.
 * @param hsp An HSP structure [in]
 * @param q_start Start of alignment in query [out]
 * @param q_end End of alignment in query [out]
 * @param s_start Start of alignment in subject [out]
 * @param s_end End of alignment in subject [out]
 */
NCBI_XBLAST_EXPORT
void 
Blast_HSPGetAdjustedOffsets(BlastHSP* hsp, Int4* q_start, Int4* q_end,
                            Int4* s_start, Int4* s_end);

/********************************************************************************
          HSPList API
********************************************************************************/

/** Deallocate memory for an HSP list structure 
 *  as well as all it's components.
 * @param hsp_list the BlastHSPList to be freed [in]. 
*/
NCBI_XBLAST_EXPORT
BlastHSPList* Blast_HSPListFree(BlastHSPList* hsp_list);

/** Creates HSP list structure with a default size HSP array 
 * @param hsp_max the maximum number of HSP's that can ever be
 *    saved at once [in].
*/
NCBI_XBLAST_EXPORT
BlastHSPList* Blast_HSPListNew(Int4 hsp_max);

/** Saves HSP information into a BlastHSPList structure
 * @param hsp_list Structure holding all HSPs with full gapped alignment 
 *        information [in] [out]
 * @param hsp The new HSP to be inserted into the HSPList [in]
*/
NCBI_XBLAST_EXPORT
Int2
Blast_HSPListSaveHSP(BlastHSPList* hsp_list, BlastHSP* hsp);

/** Assign frames in all HSPs in the HSP list.
 * @param program_number Type of BLAST program [in]
 * @param hsp_list List of HSPs for one subject sequence [in] [out]
 * @param is_ooframe Is out-of-frame gapping used? [in]
*/
NCBI_XBLAST_EXPORT
void 
Blast_HSPListSetFrames(EBlastProgramType program_number, BlastHSPList* hsp_list, 
                 Boolean is_ooframe);

/** Calculate the expected values for all HSPs in a hit list, without using 
 * the sum statistics. In case of multiple queries, the offsets are assumed 
 * to be already adjusted to individual query coordinates, and the contexts 
 * are set for each HSP.
 * @param query_info Auxiliary query information - needed only for effective
 *                   search space calculation if it is not provided [in]
 * @param hsp_list List of HSPs for one subject sequence [in] [out]
 * @param gapped_calculation Is this for a gapped or ungapped search? [in]
 * @param sbp Structure containing statistical information [in]
 * @param gap_decay_rate Adjustment parameter to compensate for the effects of
 * performing multiple tests when linking HSPs. No adjustment is made if 0. [in]
 */
NCBI_XBLAST_EXPORT
Int2 Blast_HSPListGetEvalues(BlastQueryInfo* query_info,
        BlastHSPList* hsp_list, Boolean gapped_calculation, 
        BlastScoreBlk* sbp, double gap_decay_rate);

/** Calculate e-values for a PHI BLAST HSP list.
 * @param hsp_list HSP list found by PHI BLAST [in] [out]
 * @param sbp Scoring block with statistical parameters [in]
 */
NCBI_XBLAST_EXPORT
void Blast_HSPListPHIGetEvalues(BlastHSPList* hsp_list, BlastScoreBlk* sbp);

/** Calculate bit scores from raw scores in an HSP list.
 * @param hsp_list List of HSPs [in] [out]
 * @param gapped_calculation Is this a gapped search? [in]
 * @param sbp Scoring block with statistical parameters [in]
 */
NCBI_XBLAST_EXPORT
Int2 Blast_HSPListGetBitScores(BlastHSPList* hsp_list, 
                               Boolean gapped_calculation, BlastScoreBlk* sbp);

/** Discard the HSPs above the e-value threshold from the HSP list 
 * @param hsp_list List of HSPs for one subject sequence [in] [out]
 * @param hit_options Options block containing the e-value cut-off [in]
*/
NCBI_XBLAST_EXPORT
Int2 Blast_HSPListReapByEvalue(BlastHSPList* hsp_list, 
                               BlastHitSavingOptions* hit_options);

/** Cleans out the NULLed out HSP's from the HSP array that
 * is part of the BlastHSPList.
 * @param hsp_list Contains array of pointers to HSP structures [in]
 * @return status of function call.
*/
NCBI_XBLAST_EXPORT
Int2
Blast_HSPListPurgeNullHSPs(BlastHSPList* hsp_list);

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
 * @param score_params Parameters related to scoring [in]
 * @param seq_src The BLAST database structure (for retrieving uncompressed
 *             sequence) [in]
 */
NCBI_XBLAST_EXPORT
Int2 
Blast_HSPListReevaluateWithAmbiguities(BlastHSPList* hsp_list,
   BLAST_SequenceBlk* query_blk, BLAST_SequenceBlk* subject_blk, 
   const BlastHitSavingOptions* hit_options, BlastQueryInfo* query_info, 
   BlastScoreBlk* sbp, const BlastScoringParameters* score_params, 
   const BlastSeqSrc* seq_src);

/** Append one HSP list to the other. Discard lower scoring HSPs if there is
 * not enough space to keep all.
 * @param hsp_list New list of HSPs [in]
 * @param combined_hsp_list_ptr Pointer to the combined list of HSPs, possibly
 *                              containing previously saved HSPs [in] [out]
 * @param hsp_num_max Maximal allowed number of HSPs to save (unlimited if INT4_MAX) [in]
 * @return Status: 0 on success, -1 on failure.
 */ 
NCBI_XBLAST_EXPORT
Int2 Blast_HSPListAppend(BlastHSPList* hsp_list, 
                         BlastHSPList** combined_hsp_list_ptr, 
                         Int4 hsp_num_max);

/** Merge an HSP list from a chunk of the subject sequence into a previously
 * computed HSP list.
 * @param hsp_list Contains HSPs from the new chunk [in]
 * @param combined_hsp_list_ptr Contains HSPs from previous chunks [in] [out]
 * @param hsp_num_max Maximal allowed number of HSPs to save (unlimited if INT4_MAX) [in]
 * @param start Offset where the current subject chunk starts [in]
 * @param merge_hsps Should the overlapping HSPs be merged into one? [in]
 * @return 0 if HSP lists have been merged successfully, -1 otherwise.
 */
NCBI_XBLAST_EXPORT
Int2 Blast_HSPListsMerge(BlastHSPList* hsp_list, 
                   BlastHSPList** combined_hsp_list_ptr, 
                   Int4 hsp_num_max, Int4 start, Boolean merge_hsps);

/** Adjust subject offsets in an HSP list if only part of the subject sequence
 * was searched. Used when long subject sequence is split into more manageable
 * chunks.
 * @param hsp_list List of HSPs from a chunk of a subject sequence [in]
 * @param offset Offset where the chunk starts [in]
 */
NCBI_XBLAST_EXPORT
void Blast_HSPListAdjustOffsets(BlastHSPList* hsp_list, Int4 offset);

/** Sort the HSPs in an HSP list by diagonal and remove redundant HSPs. */
NCBI_XBLAST_EXPORT
Int2
Blast_HSPListUniqSort(BlastHSPList* hsp_list);

/********************************************************************************
          HitList API.
********************************************************************************/

/** Allocate memory for a hit list of a given size.
 * @param hitlist_size Size of the hit list (number of HSP lists) [in]
 */
NCBI_XBLAST_EXPORT
BlastHitList* Blast_HitListNew(Int4 hitlist_size);

/** Deallocate memory for the hit list */
NCBI_XBLAST_EXPORT
BlastHitList* Blast_HitListFree(BlastHitList* hitlist);

/** Deallocate memory for every HSP list on BlastHitList,
 *  as well as all their components.
 * @param hitlist contains the BlastHSPList array to be freed [in/out]. 
*/
NCBI_XBLAST_EXPORT
Int2 Blast_HitListHSPListsFree(BlastHitList* hitlist);

/** Insert a new HSP list into the hit list.
 * Before capacity of the hit list is reached, just add to the end;
 * After that, store in a heap, to ensure efficient insertion and deletion.
 * The heap order is reverse, with worst e-value on top, for convenience
 * of deletion.
 * @param hit_list Contains all HSP lists saved so far [in] [out]
 * @param hsp_list A new HSP list to be inserted into the hit list [in]
*/
NCBI_XBLAST_EXPORT
Int2 Blast_HitListUpdate(BlastHitList* hit_list, BlastHSPList* hsp_list);

/********************************************************************************
          HSPResults API.
********************************************************************************/

/** Initialize the results structure.
 * @param num_queries Number of query sequences to allocate results structure
 *                    for [in]
 * @param results_ptr The allocated structure [out]
 */
NCBI_XBLAST_EXPORT
Int2 Blast_HSPResultsInit(Int4 num_queries, BlastHSPResults** results_ptr);

/** Deallocate memory for BLAST results */
NCBI_XBLAST_EXPORT
BlastHSPResults* Blast_HSPResultsFree(BlastHSPResults* results);

/** Sort each hit list in the BLAST results by best e-value */
NCBI_XBLAST_EXPORT
Int2 Blast_HSPResultsSortByEvalue(BlastHSPResults* results);
/** Sort each hit list in the BLAST results by best e-value, in reverse
    order. */
NCBI_XBLAST_EXPORT
Int2 Blast_HSPResultsReverseSort(BlastHSPResults* results);

/** Reverse order of HSP lists in each hit list in the BLAST results. 
 * This allows to return HSP lists from the end of the arrays when reading
 * from a collector HSP stream.
 */
NCBI_XBLAST_EXPORT
Int2 Blast_HSPResultsReverseOrder(BlastHSPResults* results);

/** Blast_HSPResultsSaveRPSHSPList
 *  Save the HSPs from an HSPList obtained on the preliminary stage of 
 * RPS BLAST to appropriate places in the results structure. Input HSPList
 * contains HSPs from a single query, but from all RPS BLAST database 
 * sequences. 
 * @param program The type of BLAST search [in]
 * @param results The structure holding results for all queries [in] [out]
 * @param hsp_list The results for the current subject sequence; in case of 
 *                 multiple queries, offsets are still in the concatenated 
 *                 sequence coordinates [in]
 * @param hit_options The options related to saving hits [in]
 */
NCBI_XBLAST_EXPORT
Int2 Blast_HSPResultsSaveRPSHSPList(EBlastProgramType program, BlastHSPResults* results, 
        BlastHSPList* hsp_list, const BlastHitSavingOptions* hit_options);

/** Blast_HSPResultsSaveHSPList
 *  Save the current HSP list to appropriate places in the results structure.
 * The input HSPList contains HSPs from a single BLAST database sequence, but
 * possibly from multiple queries.
 * @param program The type of BLAST search [in]
 * @param results The structure holding results for all queries [in] [out]
 * @param hsp_list The results for the current subject sequence; in case of 
 *                 multiple queries, offsets are still in the concatenated 
 *                 sequence coordinates [in]
 * @param hit_options The options related to saving hits [in]
 */
NCBI_XBLAST_EXPORT
Int2 Blast_HSPResultsSaveHSPList(EBlastProgramType program, BlastHSPResults* results, 
        BlastHSPList* hsp_list, const BlastHitSavingOptions* hit_options);

/** Blast_HSPResultsSaveHSPList
 * Insert an HSP list to the appropriate place in the results structure.
 * All HSPs in this list must be from the same query, and the query index
 * must be set in the BlastHSPList input structure.
 * @param results The structure holding results for all queries [in] [out]
 * @param hsp_list The results for one query-subject sequence pair. [in]
 * @param hitlist_size Maximal allowed hit list size. [in]
 */
NCBI_XBLAST_EXPORT
Int2 Blast_HSPResultsInsertHSPList(BlastHSPResults* results, 
        BlastHSPList* hsp_list, Int4 hitlist_size);

#ifdef __cplusplus
}
#endif
#endif /* !__BLAST_HITS__ */

