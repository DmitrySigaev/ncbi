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

File name: blast_options.h

Author: Tom Madden

Contents: BLAST options

Detailed Contents: 

	- Options to be used for different tasks of the BLAST search

******************************************************************************
 * $Revision$
 * */

#ifndef __BLASTOPTIONS__
#define __BLASTOPTIONS__

#include <ncbi.h>
#include <blast_def.h>
#include <blast_message.h>
#include <objloc.h>


#ifdef __cplusplus
extern "C" {
#endif

/** Some default values (used when creating blast options block and for
 * command-line program defaults. When changing these defaults, please
 * remember to update the defaults in the command-line programs 
 */

/** "window" between hits to trigger an extension. */
#define BLAST_WINDOW_SIZE_PROT 40
#define BLAST_WINDOW_SIZE_NUCL 0
#define BLAST_WINDOW_SIZE_MEGABLAST 0

/** length of word to trigger an extension. */
#define BLAST_WORDSIZE_PROT 3
#define BLAST_WORDSIZE_NUCL 11
#define BLAST_WORDSIZE_MEGABLAST 28

/** Protein gap costs are the defaults for the BLOSUM62 scoring matrix.
 * More gap costs are listed in BLASTOptionSetGapParams 
 */

/** cost for the existence of a gap.*/
#define BLAST_GAP_OPEN_PROT 11
#define BLAST_GAP_OPEN_NUCL 5
#define BLAST_GAP_OPEN_MEGABLAST 0

/** cost to extend a gap. */
#define BLAST_GAP_EXTN_PROT 1
#define BLAST_GAP_EXTN_NUCL 2
#define BLAST_GAP_EXTN_MEGABLAST 0

/** neighboring word score thresholds */
#define BLAST_WORD_THRESHOLD_BLASTP 11
#define BLAST_WORD_THRESHOLD_BLASTN 0
#define BLAST_WORD_THRESHOLD_BLASTX 12
#define BLAST_WORD_THRESHOLD_TBLASTN 13
#define BLAST_WORD_THRESHOLD_TBLASTX 13
#define BLAST_WORD_THRESHOLD_MEGABLAST 0

/** dropoff for ungapped extension */
#define BLAST_UNGAPPED_X_DROPOFF_PROT 7
#define BLAST_UNGAPPED_X_DROPOFF_NUCL 20

/** dropoff for gapped extension */
#define BLAST_GAP_X_DROPOFF_PROT 15
#define BLAST_GAP_X_DROPOFF_NUCL 30
#define BLAST_GAP_X_DROPOFF_GREEDY 30
#define BLAST_GAP_X_DROPOFF_TBLASTX 0

/** minimal score for triggering gapped extension */
#define BLAST_GAP_TRIGGER_PROT 22.0
#define BLAST_GAP_TRIGGER_NUCL 25.0 

/** dropoff for the final gapped extension with traceback */
#define BLAST_GAP_X_DROPOFF_FINAL_PROT 25
#define BLAST_GAP_X_DROPOFF_FINAL_NUCL 50
#define BLAST_GAP_X_DROPOFF_FINAL_TBLASTX 0

/** reward and penalty only apply to blastn/megablast */
#define BLAST_PENALTY -3
#define BLAST_REWARD 1

/** expect value cutoff */
#define BLAST_EXPECT_VALUE 10.0

/** Types of the lookup table */
#define MB_LOOKUP_TABLE 1
#define NA_LOOKUP_TABLE 2
#define AA_LOOKUP_TABLE 3

/** Options needed to construct a lookup table 
 * Also needed: query sequence and query length.
 */
typedef struct LookupTableOptions {
   CharPtr matrixname;  /**< Amino acid substitution matrix */
   Int4 threshold; /**< Score threshold for putting words in a lookup table */
   Int4 lut_type; /**< What kind of lookup table to construct? E.g. blastn 
                     allows for traditional and megablast style lookup table */
   Int2 word_size; /**< Determines the size of the lookup table */
   Int4 alphabet_size; /**< Size of the alphabet */
   Int2 mb_template_length; /**< Length of the discontiguous words */
   Uint1 mb_template_type; /**< Type of a discontiguous word template */
   Int4 max_positions; /**< Max number of positions per word (MegaBlast only);
                         no restriction if 0 */
   Uint1 scan_step; /**< Step at which database sequence should be parsed */
   Uint1 program_number;	/* number for blastn, blastp, etc. */
} LookupTableOptions, *LookupTableOptionsPtr;

/** Options required for setting up the query sequence */
typedef struct QuerySetUpOptions {
   CharPtr filter_string; /**< Parseable string that determines the filtering
                             options */
   SeqLocPtr lcase_mask; /**< Lower case masked locations on the query */
   Uint1 strand_option; /**< In blastn: which strand to search: 1 = forward;
                           2 = reverse; 3 = both */
} QuerySetUpOptions, *QuerySetUpOptionsPtr;

/** Options needed for initial word finding and processing */
typedef struct BlastInitialWordOptions {
   Int4 window_size; /**< Maximal allowed distance between 2 hits in case 2 
                        hits are required to trigger the extension */
   Int4 extend_word_method; /**< What to do with the initial words? E.g. for
                              blastn: do mini-extension to the word size; in
                              megablast: keep word information on stacks, 
                              etc. */
   FloatHi x_dropoff; /**< X-dropoff value (in bits) for the ungapped 
                         extension */
} BlastInitialWordOptions, *BlastInitialWordOptionsPtr;

/** Parameter block that contains a pointer to BlastInitialWordOptions
 * and parsed values for those options that require it 
 * (in this case x_dropoff).
 */
typedef struct BlastInitialWordParameters {
   BlastInitialWordOptionsPtr options; /**< The original (unparsed) options. */
   Int4 x_dropoff; /**< Raw X-dropoff value for the ungapped extension */
   Int4 cutoff_score; /**< Cutoff score for saving ungapped hits. */
} BlastInitialWordParameters, *BlastInitialWordParametersPtr;
	
/** Options used for gapped extension 
 *  These include:
 *  a. Penalties for various types of gapping;
 *  b. Drop-off values for the extension algorithms tree exploration;
 *  c. Parameters identifying what kind of extension algorithm(s) should 
 *     be used.
 */
typedef struct BlastExtensionOptions {
   FloatHi gap_x_dropoff; /**< X-dropoff value for gapped extension (in bits) */
   FloatHi gap_x_dropoff_final;/**< X-dropoff value for the final gapped 
                                  extension (in bits) */
   FloatHi gap_trigger;/**< Score in bits for starting gapped extension */
   Int4 algorithm_type; /**< E.g. for blastn: dynamic programming; 
                           greedy without traceback; greedy with traceback */
   Uint1 program_number; /**< Number for blastn, blastp, etc. */
} BlastExtensionOptions, *BlastExtensionOptionsPtr;

typedef struct BlastExtensionParameters {
   BlastExtensionOptionsPtr options;
   Int4 gap_x_dropoff; /**< X-dropoff value for gapped extension (raw) */
   Int4 gap_x_dropoff_final;/**< X-dropoff value for the final gapped 
                               extension (raw) */
   FloatHi gap_trigger; /**< Minimal raw score for starting gapped extension */
} BlastExtensionParameters, PNTR BlastExtensionParametersPtr;

/** Options used when evaluating and saving hits
 *  These include: 
 *  a. Restrictions on the number of hits to be saved;
 *  b. Restrictions on the quality and positions of hits to be saved;
 *  c. Parameters used to evaluate the quality of hits.
 */
typedef struct BlastHitSavingOptions {
   Int4 hitlist_size;/**< Maximal number of database sequences to save hits 
                        for */
   Int4 hsp_num_max; /**< Maximal number of HSPs to save for one database 
                        sequence */
   Int4 total_hsp_limit; /**< Maximal total number of HSPs to keep */
   Int4 hsp_range_max; /**< Maximal number of HSPs to save in a region: 
                          used for culling only */
   Boolean perform_culling;/**< Perform culling of hit lists by keeping at 
                              most a certain number of HSPs in a range */
   Int4 required_start;  /**< Start of the region required to be part of the
                            alignment */
   Int4 required_end;    /**< End of the region required to be part of the
                            alignment */
   FloatHi expect_value; /**< The expect value cut-off threshold for an HSP, or
                            a combined hit if sum statistics is used */
   FloatHi original_expect_value; /**< Needed for PSI-BLAST??? */
   FloatHi single_hsp_evalue; /**< When sum statistics is used, the largest 
                                 e-value allowed for an individual HSP */
   Int4 cutoff_score; /**< The (raw) score cut-off threshold */
   Int4 single_hsp_score; /**< The score cut-off for a single HSP when sum
                             statistics is used */
   FloatHi percent_identity; /**< The percent identity cut-off threshold */
   Boolean do_sum_stats; /**< Should sum statistics be used to combine HSPs? */
   Int4 longest_intron; /**< The longest distance between HSPs allowed for
                           combining via sum statistics with uneven gaps */
   Boolean is_neighboring;/**< Neighboring has different hit saving criteria */
   Boolean is_gapped;	/**< gapping is used. */
   Uint1 program_number; /**< Number for blastn, blastp, etc. */
   Uint1 handle_results_method; /**< Formatting results on the fly if set to 
                                   non-zero */
} BlastHitSavingOptions, *BlastHitSavingOptionsPtr;

/** Parameter block that contains a pointer to BlastHitSavingOptions
 * and parsed values for those options that require it
 * (in this case expect value).
 */
typedef struct BlastHitSavingParameters {
   BlastHitSavingOptionsPtr options; /**< The original (unparsed) options. */
   int (LIBCALLBACK *handle_results)PROTO((VoidPtr query, VoidPtr subject, 
        VoidPtr hsp_list, VoidPtr hit_options, VoidPtr query_info, 
        VoidPtr sbp, VoidPtr rdfp));
   /**< Callback for formatting results on the fly for each subject sequence */
} BlastHitSavingParameters, *BlastHitSavingParametersPtr;
	

/** Scoring options block 
 *  Used to produce the BlastScoreBlk structure
 *  This structure may be needed for lookup table construction (proteins only),
 *  and for evaluating alignments. 
 */
typedef struct BlastScoringOptions {
   CharPtr matrix;   /**< Name of the matrix containing all scores: needed for
                        finding neighboring words */
   Int2 reward;      /**< Reward for a match */
   Int2 penalty;     /**< Penalty for a mismatch */
   Boolean gapped_calculation; /**< Will a gapped extension be performed? */
   Int4 gap_open;    /**< Extra penalty for starting a gap */
   Int4 gap_extend;  /**< Penalty for each gap residue */
   Int4 shift_pen;   /**< Penalty for shifting a frame in out-of-frame 
                        gapping */
   Int4 decline_align; /**< Cost for declining alignment */
   Boolean is_ooframe; /**< Should out-of-frame gapping be used in a translated
                          search? */
   Uint1 program_number;	/* Number for blastn, blastp, etc. */
} BlastScoringOptions, *BlastScoringOptionsPtr;

/** Options for setting up effective lengths and search spaces.  
 * All values will be the real (correct) values unless the user has specified an override.
 */
typedef struct BlastEffectiveLengthsOptions {
   Int8 db_length;    /**< Database length used for statistical
                         calculations */
   Int4 dbseq_num;    /**< Number of database sequences used for
                           statistical calculations */
   Int8 searchsp_eff; /**< Search space used for statistical
                           calculations */
   Boolean use_real_db_size; /**< Use real database size instead of virtual
                                database size for statistical calculations */
} BlastEffectiveLengthsOptions, *BlastEffectiveLengthsOptionsPtr;

/** Options used in protein BLAST only (PSI, PHI, RPS and translated BLAST)
 *  Some of these possibly should be transfered elsewhere  
 */
typedef struct ProteinBlastOptions {
   FloatHi ethresh;       /**< PSI-BLAST */
   Int4 maxNumPasses;     /**< PSI-BLAST */
   Int4 pseudoCountConst; /**< PSI-BLAST */
   Boolean composition_based_stat;/**< PSI-BLAST */
   FloatHi scalingFactor; /**< Scaling factor used when constructing PSSM for
                             RPS-BLAST */
   Boolean use_best_align; /**< Use only alignments chosen by user for PSSM
                              computation: WWW PSI-BLAST only */
   Boolean smith_waterman;  /**< PSI-BLAST */
   Boolean discontinuous;   /**< PSI-BLAST */
   Boolean isPatternSearch; /**< PHI-BLAST */
   CharPtr phi_pattern;    /**< PHI-BLAST */
   Int4 max_num_patterns; /**< PHI-BLAST */
   Boolean is_rps_blast;    /**< RPS-BLAST */
   Int4 genetic_code;     /**< blastx, tblastx */
   Int4 db_genetic_code;  /**< tblastn, tblastx */
} ProteinBlastOptions, *ProteinBlastOptionsPtr;

/** Options used to create the ReadDBFILE structure 
 *  Include database name and various information for restricting the database
 *  to a subset.
 *  THESE SHOULD PROBABLY BE MOVED TO A DIFFERENT HEADER FILE!!!
 */
typedef struct BlastDatabaseSetUpOptions {
   CharPtr database; /**< Name of the database */
   CharPtr gifile;   /**< File to get a gi list from: REMOVE? */
   ValNodePtr gilist; /**< A list of gis this database should be restricted to:
                         REMOVE? */
   CharPtr entrez_query;/**< An Entrez query to get a OID list from: REMOVE? */
   Int4 first_db_seq; /**< The first ordinal id number (OID) to search */
   Int4 final_db_seq; /**< The last OID to search */
} BlastDatabaseSetUpOptions, *BlastDatabaseSetUpOptionsPtr;

/** Options for formatting BLAST results 
 */
typedef struct BlastFormattingOptions {
   Uint1 align_view;
   Uint4 align_options;
   Uint4 print_options;
   Boolean believe_query;
   Boolean html;
   FILE *outfp;
   Int4 number_of_descriptions;
   Int4 number_of_alignments;
   Boolean ungapped; /**< Should this be here????? */
} BlastFormattingOptions, *BlastFormattingOptionsPtr;

/********************************************************************************

	Functions to create options blocks with default values
	and free them after use.

*********************************************************************************/

/** Deallocate memory for QuerySetUpOptions. */
QuerySetUpOptionsPtr BlastQuerySetUpOptionsFree(QuerySetUpOptionsPtr options);


/** Allocate memory for QuerySetUpOptions and fill with default values.  
 * @param program Program name (blastn, blastp, etc.) [in]
 * @param filter_string Parsable string of filtering options [in] 
 * @param strand_option which strand to search [in]
 * @param options The options that have are being returned [out]
*/
Int2 BlastQuerySetUpOptionsNew(const Char *program, CharPtr filter_string,
        Uint1 strand_option, QuerySetUpOptionsPtr *options);

/** Deallocate memory for BlastInitialWordOptions. */
BlastInitialWordOptionsPtr
BlastInitialWordOptionsFree(BlastInitialWordOptionsPtr options);

/** Allocate memory for BlastInitialWordOptions and fill with default values.
 * @param program Program name (blastn, blastp, etc.) [in]
 * @param greedy Settings should assume greedy alignments [in]
 * @param window_size Size of a largest window between 2 words for the two-hit
 *                    version [in]
 * @param variable_wordsize Will only full bytes of the compressed sequence be
 *        checked in initial word extension (blastn only)? [in]
 * @param ag_blast Is AG BLAST approach used for scanning the database 
 *                 (blastn only)? [in]
 * @param mb_lookup Is Mega BLAST (12-mer based) lookup table used? [in]
 * @param xdrop_ungapped The value of the X-dropoff for ungapped extensions [in]
 * @param options The options that have are being returned [out] 
*/
Int2
BlastInitialWordOptionsNew(const Char *program, Boolean greedy, 
   Int4 window_size, Boolean variable_wordsize, Boolean ag_blast,
   Boolean mb_lookup, FloatHi xdrop_ungapped, 
   BlastInitialWordOptionsPtr *options);

/** Deallocate memory for BlastInitialWordParameters. */
BlastInitialWordParametersPtr
BlastInitialWordParametersFree(BlastInitialWordParametersPtr parameters);

/** Allocate memory for BlastInitialWordParameters and set x_dropoff.
 * @param word_options The initial word options [in]
 * @param hit_options The hit saving options (needed to calculate cutoff score 
 *                    for ungapped extensions) [in]
 * @param ext_params Extension parameters (containing gap trigger value) [in]
 * @param sbp Statistical (Karlin-Altschul) information [in]
 * @param query_info Query information [in]
 * @param parameters Resulting parameters [out]
*/
Int2
BlastInitialWordParametersNew(BlastInitialWordOptionsPtr options, 
   BlastHitSavingOptionsPtr hit_options, BlastExtensionParametersPtr ext_params, 
   BLAST_ScoreBlkPtr sbp, BlastQueryInfoPtr query_info, 
   BlastInitialWordParametersPtr *parameters);

/** Deallocate memory for BlastExtensionOptions. */
BlastExtensionOptionsPtr
BlastExtensionOptionsFree(BlastExtensionOptionsPtr options);

/** Allocate memory for BlastExtensionOptions and fill with default values.
 * @param program Program name (blastn, blastp, etc.) [in]
 * @param greedy Settings should assume greedy alignments [in]
 * @param x_dropoff X-dropoff parameter value for preliminary gapped 
 *                  extensions [in]
 * @param x_dropoff_final X-dropoff parameter value for final gapped 
 *                        extensions with traceback [in]
 * @param options The options that are being returned [out]
*/
Int2
BlastExtensionOptionsNew(const Char *program, Boolean greedy, 
   FloatHi x_dropoff, FloatHi x_dropoff_final, 
   BlastExtensionOptionsPtr *options);

/** Validate contents of BlastExtensionOptions.
 * @param options Options to be validated [in]
 * @param blast_msg Describes any validation problems found [out]
*/
Int2 BlastExtensionOptionsValidate(BlastExtensionOptionsPtr options, Blast_MessagePtr *blast_msg);

/** Calculate the raw values for the X-dropoff parameters 
 * @param blast_program Program name [in]
 * @param options Already allocated extension options [in]
 * @param sbp Structure containing statistical information [in]
 * @param parameters Extension parameters [out]
 */
Int2 BlastExtensionParametersNew(CharPtr blast_program, 
        BlastExtensionOptionsPtr options, 
        BLAST_ScoreBlkPtr sbp, BlastExtensionParametersPtr *parameters);

/** Deallocate memory for BlastExtensionParameters. */
BlastExtensionParametersPtr
BlastExtensionParametersFree(BlastExtensionParametersPtr parameters);


/**  Deallocate memory for BlastScoringOptions. */
BlastScoringOptionsPtr BlastScoringOptionsFree(BlastScoringOptionsPtr options);

/** Allocate memory for BlastScoringOptions and fill with default values. 
 * @param program Program name (blastn, blastp, etc.) [in]
 * @param is_megablast Megablast (instead of blastn) if TRUE [in]
 * @param penalty Mismatch penalty score (blastn only) [in]
 * @param reward Match reward score (blastn only) [in]
 * @param matrix Name of the BLAST matrix (all except blastn) [in]
 * @param gap_open Extra cost for opening a gap [in]
 * @param gap_extend Cost of a gap [in]
 * @param options The options that are being returned [out]
*/
Int2 BlastScoringOptionsNew(const Char *program, Boolean is_megablast, 
        Int4 penalty, Int4 reward, CharPtr matrix, Int4 gap_open, 
        Int4 gap_extend, BlastScoringOptionsPtr *options);

/** Validate contents of BlastScoringOptions.
 * @param options Options to be validated [in]
 * @param blast_msg Describes any validation problems found [out]
*/
Int2
BlastScoringOptionsValidate( BlastScoringOptionsPtr options, Blast_MessagePtr *blast_msg);

/** Deallocate memory for BlastEffectiveLengthsOptionsPtr. */
BlastEffectiveLengthsOptionsPtr 
BlastEffectiveLengthsOptionsFree(BlastEffectiveLengthsOptionsPtr options);

/** Allocate memory for BlastEffectiveLengthsOptionsPtr and fill with 
 * default values. 
 * @param database BLAST database [in]
 * @param is_protein Protein if TRUE [in]
 * @param dbseq_num Number of sequences in the database (if zero real value will be used) [in]
 * @param db_length Total length of the database (if zero real value will be used) [in]
 * @param searchsp_eff Effective search space (if zero real value will be used) [in]
 * @param options The options that are being returned [out]
 */
Int2 BlastEffectiveLengthsOptionsNew(CharPtr database, Boolean is_protein,
        Int4 dbseq_num, Int8 db_length, Int8 searchsp_eff, 
        BlastEffectiveLengthsOptionsPtr *options);

/** Allocate memory for lookup table options and fill with default values.
 * @param program Program name (blastn, blastp, etc.) [in]
 * @param is_megablast Megablast (instead of blastn) if TRUE [in]
 * @param threshold Threshold value for finding neighboring words [in]
 * @param word_size Number of matched residues in an initial word [in]
 * @param ag_blast Is AG BLAST approach to database scanning used? [in]
 * @param variable_wordsize Are only full bytes of a compressed sequence 
 *        checked to find initial words? [in]
 * @param options The options that are being returned [out]
 */
Int2 LookupTableOptionsNew(const Char *program, Boolean is_megablast, 
        Int4 threshold, Int2 word_size, Boolean ag_blast, 
        Boolean variable_wordsize, LookupTableOptionsPtr *options);

/*******************************************************************************
        Deallocates memory for LookupTableOptionsPtr
*********************************************************************************/
LookupTableOptionsPtr
LookupTableOptionsFree(LookupTableOptionsPtr options);

/** Validate LookupTableOptions.
 * @param options The options that have are being returned [in]
 * @param blast_msg The options that have are being returned [out]
*/

Int2
LookupTableOptionsValidate(LookupTableOptionsPtr options,  Blast_MessagePtr *blast_msg);

/** Deallocate memory for BlastHitSavingOptions. */
BlastHitSavingOptionsPtr
BlastHitSavingOptionsFree(BlastHitSavingOptionsPtr options);

/** Validate BlastHitSavingOptions
 * @param options The options that have are being returned [in]
 * @param blast_msg The options that have are being returned [out]
*/

Int2
BlastHitSavingOptionsValidate(BlastHitSavingOptionsPtr options, Blast_MessagePtr *blast_msg);

/*************************************************************************

	Deallocate memory for BlastHitSavingParameterPtr.

*************************************************************************/
/** Allocate memory for BlastHitSavingOptions.
 * @param program Program name (blastn, blastp, etc.) [in]
 * @param is_gapped Specifies that search is gapped [in]
 * @param evalue The expected value threshold [in]
 * @param hitlist_size How many database sequences to save per query? [in]
 * @param options The options that are being returned [out]
*/
Int2 BlastHitSavingOptionsNew(const Char *program, Boolean is_gapped, 
        FloatHi evalue, Int4 hitlist_size, BlastHitSavingOptionsPtr *options);

/** Deallocate memory for BlastHitSavingOptionsPtr. */
BlastHitSavingParametersPtr
BlastHitSavingParametersFree(BlastHitSavingParametersPtr parameters);

/** Allocate memory for BlastInitialWordParameters and set x_dropoff. 
 * @param options The given hit saving options [in]
 * @param handle_results Callback function for printing results on the fly [in]
 * @param parameters Resulting parameters [out]
 */
Int2 BlastHitSavingParametersNew(BlastHitSavingOptionsPtr options, 
        int handle_results(VoidPtr, VoidPtr, VoidPtr, VoidPtr, VoidPtr, VoidPtr, 
                VoidPtr), BlastHitSavingParametersPtr *parameters);

/** Initialize the formatting options structure.
 * @param program Name of the BLAST program [in]
 * @param file_out Name of the file where output is printed [in]
 * @param num_descriptions Number of definition lines to report per query [in]
 * @param num_alignments Number of alignments to show per query [in]
 * @param align_view What kind of formatted output to show? [in]
 * @param format_options_ptr The initialized structure [out]
*/
Int2 BlastFormattingOptionsNew(CharPtr program, CharPtr file_out, 
        Int4 num_descriptions, Int4 num_alignments, Int4 align_view,
        BlastFormattingOptionsPtr PNTR format_options_ptr);

/** Deallocate memory for formatting options. In particular,
 * close the output file.
 */
BlastFormattingOptionsPtr 
BlastFormattingOptionsFree(BlastFormattingOptionsPtr format_options);

#ifdef __cplusplus
}
#endif
#endif /* !__BLASTOPTIONS__ */

