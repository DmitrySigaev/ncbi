/* $Id$
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
* ===========================================================================*/
/*****************************************************************************

File name: blastkar.h

Author: Tom Madden

Contents: definitions and prototypes used by blastkar.c to calculate BLAST
	statistics.

******************************************************************************/

/* $Revision$ 
 * */
#ifndef __BLASTKAR__
#define __BLASTKAR__

#include <algo/blast/core/blast_def.h>
#include <algo/blast/core/blast_message.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
	Defines for the matrix 'preferences' (as specified by S. Altschul).
*/
#define BLAST_MATRIX_NOMINAL 0
#define BLAST_MATRIX_PREFERRED 1
#define BLAST_MATRIX_BEST 2


/****************************************************************************
For more accuracy in the calculation of K, set K_SUMLIMIT to 0.00001.
For high speed in the calculation of K, use a K_SUMLIMIT of 0.001
Note:  statistical significance is often not greatly affected by the value
of K, so high accuracy is generally unwarranted.
*****************************************************************************/
/* K_SUMLIMIT_DEFAULT == sumlimit used in BlastKarlinLHtoK() */
#define BLAST_KARLIN_K_SUMLIMIT_DEFAULT 0.01

/* LAMBDA_ACCURACY_DEFAULT == accuracy to which Lambda should be calc'd */
#define BLAST_KARLIN_LAMBDA_ACCURACY_DEFAULT    (1.e-5)

/* LAMBDA_ITER_DEFAULT == no. of iterations in LambdaBis = ln(accuracy)/ln(2)*/
#define BLAST_KARLIN_LAMBDA_ITER_DEFAULT        17

/* Initial guess for the value of Lambda in BlastKarlinLambdaNR */
#define BLAST_KARLIN_LAMBDA0_DEFAULT    0.5

#define BLAST_KARLIN_K_ITER_MAX 100
#define BLAST_SUMP_EPSILON_DEFAULT 0.002 /* accuracy for SumP calculations */

/* 
	Where are the BLAST matrices located?
*/
#define BLASTMAT_DIR "/usr/ncbi/blast/matrix"

#define BLASTNA_SIZE 16
#define BLASTAA_SIZE 26
/* Identifies the blastna alphabet, for use in blast only. */
#define BLASTNA_SEQ_CODE 99
#define BLASTAA_SEQ_CODE 11 /* == Seq_code_ncbistdaa */
#define NCBI4NA_SEQ_CODE 4  /* == Seq_code_ncbi4na */	

#define PSI_ALPHABET_SIZE  26 /* For PSI Blast this is the only 26 */

/*
	Maps the ncbi4na alphabet to blastna, an alphabet that blastn uses
	as the first four characters have the same representation as in
	ncbi2na.
*/
extern Uint1 NCBI4NA_TO_BLASTNA[BLASTNA_SIZE];
	
/*************************************************************************
	Structure to the Karlin-Blk parameters.

	This structure was (more or less) copied from the old
	karlin.h.
**************************************************************************/

typedef struct BLAST_KarlinBlk {
		double	Lambda; /* Lambda value used in statistics */
		double	K, logK; /* K value used in statistics */
		double	H; /* H value used in statistics */
		double	paramC;	/* for use in seed. */
	} BLAST_KarlinBlk;




/********************************************************************

	Structures relating to scoring or the BlastScoreBlk

********************************************************************/

/*
SCORE_MIN is (-2**31 + 1)/2 because it has been observed more than once that
a compiler did not properly calculate the quantity (-2**31)/2.  The list
of compilers which failed this operation have at least at some time included:
NeXT and a version of AIX/370's MetaWare High C R2.1r.
For this reason, SCORE_MIN is not simply defined to be LONG_MIN/2.
*/
#define BLAST_SCORE_MIN	INT2_MIN
#define BLAST_SCORE_MAX	INT2_MAX


#if defined(OS_DOS) || defined(OS_MAC)
#define BLAST_SCORE_1MIN (-100)
#define BLAST_SCORE_1MAX ( 100)
#else
#define BLAST_SCORE_1MIN (-10000)
#define BLAST_SCORE_1MAX ( 1000)
#endif
#define BLAST_SCORE_RANGE_MAX	(BLAST_SCORE_1MAX - BLAST_SCORE_1MIN)

typedef struct BLAST_ScoreFreq {
    Int4	score_min, score_max;
    Int4	obs_min, obs_max;
    double	score_avg;
    double* sprob0,* sprob;
} BLAST_ScoreFreq;

#define BLAST_MATRIX_SIZE 32

typedef struct BLASTMatrixStructure {
    Int4 *matrix[BLAST_MATRIX_SIZE];
    Int4 long_matrix[BLAST_MATRIX_SIZE*BLAST_MATRIX_SIZE];
} BLASTMatrixStructure;

typedef struct BlastScoreBlk {
	Boolean		protein_alphabet; /* TRUE if alphabet_code is for a 
protein alphabet (e.g., ncbistdaa etc.), FALSE for nt. alphabets. */
	Uint1		alphabet_code;	/* NCBI alphabet code. */
	Int2 		alphabet_size;  /* size of alphabet. */
	Int2 		alphabet_start;  /* numerical value of 1st letter. */
	BLASTMatrixStructure* matrix_struct;	/* Holds info about matrix. */
	Int4 **matrix;  /* Substitution matrix */
	Int4 **posMatrix;  /* Sub matrix for position depend BLAST. */
   double karlinK; /* Karlin-Altschul parameter associated with posMatrix */
	Int2	mat_dim1, mat_dim2;	/* dimensions of matrix. */
	Int4 *maxscore; /* Max. score for each letter */
	Int4	loscore, hiscore; /* Min. & max. substitution scores */
	Int4	penalty, reward; /* penalty and reward for blastn. */
	Boolean		read_in_matrix; /* If TRUE, matrix is read in, otherwise
					produce one from penalty and reward above. */
	BLAST_ScoreFreq** sfp;	/* score frequencies. */
	double **posFreqs; /*matrix of position specific frequencies*/
	/* kbp & kbp_gap are ptrs that should be set to kbp_std, kbp_psi, etc. */
	BLAST_KarlinBlk** kbp; 	/* Karlin-Altschul parameters. */
	BLAST_KarlinBlk** kbp_gap; /* K-A parameters for gapped alignments. */
	/* Below are the Karlin-Altschul parameters for non-position based ('std')
	and position based ('psi') searches. */
	BLAST_KarlinBlk **kbp_std,	
                    **kbp_psi,	
                    **kbp_gap_std,
                    **kbp_gap_psi;
	BLAST_KarlinBlk* 	kbp_ideal;	/* Ideal values (for query with average database composition). */
	Int2 number_of_contexts;	/* Used by sfp and kbp, how large are these*/
	Int2		matid;		/* id number of matrix. */
	char* 	name;		/* name of matrix. */
	Uint1* 	ambiguous_res;	/* Array of ambiguous res. (e.g, 'X', 'N')*/
	Int2		ambig_size,	/* size of array above. */
			ambig_occupy;	/* How many occupied? */
	ListNode*	comments;	/* Comments about matrix. */
/**** Andy's modification ****/
	Int4    	query_length;   /* the length of the query. */
/**** end Andy's modification ****/
	Int4	length_adjustment; /* length to trim query/db sequence by. */
	Int4	effective_query_length; /* shortened query length. */
	Int8	effective_db_length;	/* trimmed db length */
	Int8	effective_search_sp;	/* product of above two */
} BlastScoreBlk;

/* Used for communicating between BLAST and other applications. */
typedef struct BLAST_Matrix {
    Boolean is_prot;	/* Matrix is for proteins */
    char* name;		/* Name of Matrix (i.e., BLOSUM62). */
    /* Position-specific BLAST rows and columns are different, otherwise they are the
    alphabet length. */
    Int4	rows,		/* query length + 1 for PSSM. */
        columns;	/* alphabet size in all cases (26). */
    Int4** matrix;
    double ** posFreqs;
    double karlinK;
    Int4** original_matrix;
} BLAST_Matrix;

typedef struct BLAST_ResComp {
    Uint1	alphabet_code;
    Int4*	comp; 	/* composition of alphabet, array starts at beginning of alphabet. */
    Int4*   comp0;	/* Same array as above, starts at zero. */
} BLAST_ResComp;

typedef struct BLAST_ResFreq {
    Uint1		alphabet_code;
    double* prob;	/* probs, (possible) non-zero offset. */
    double* prob0; /* probs, zero offset. */
} BLAST_ResFreq;

BlastScoreBlk* BlastScoreBlkNew (Uint1 alphabet, Int2 number_of_contexts);

BlastScoreBlk* BlastScoreBlkFree (BlastScoreBlk* sbp);

Int2 BLAST_ScoreSetAmbigRes (BlastScoreBlk* sbp, char ambiguous_res);


Int2 BLAST_ScoreBlkFill (BlastScoreBlk* sbp, char* string, Int4 length, Int2 context_number);
 
/** This function fills in the BlastScoreBlk structure.  
 * Tasks are:
 *	-read in the matrix
 *	-set maxscore
 * @param sbp Scoring block [in] [out]
 * @param matrix_path Full path to the matrix in the directory structure [in]
*/
Int2 BLAST_ScoreBlkMatFill (BlastScoreBlk* sbp, char* matrix);
 
/*
	Functions taken from the OLD karlin.c
*/

BLAST_KarlinBlk* BLAST_KarlinBlkCreate (void);

Int2 BLAST_KarlinBlkGappedCalc (BLAST_KarlinBlk* kbp, Int4 gap_open, 
        Int4 gap_extend, Int4 decline_align, char* matrix_name, 
        Blast_Message** error_return);
Int2 BLAST_KarlinBlkStandardCalc(BlastScoreBlk* sbp, Int2 context_start, 
                                 Int2 context_end);

/*
        Attempts to fill KarlinBlk for given gap opening, extensions etc.
        Will return non-zero status if that fails.

        return values:  -1 if matrix_name is NULL;
                        1 if matrix not found
                        2 if matrix found, but open, extend etc. values not supported.
*/
Int2 BLAST_KarlinkGapBlkFill(BLAST_KarlinBlk* kbp, Int4 gap_open, Int4 gap_extend, Int4 decline_align, char* matrix_name);

/* Prints a messages about the allowed matrices, BlastKarlinkGapBlkFill should return 1 before this is called. */
char* BLAST_PrintMatrixMessage(const char *matrix);

/* Prints a messages about the allowed open etc values for the given matrix, 
BlastKarlinkGapBlkFill should return 2 before this is called. */
char* BLAST_PrintAllowedValues(const char *matrix, Int4 gap_open, Int4 gap_extend, Int4 decline_align);

double BLAST_KarlinStoE_simple (Int4 S, BLAST_KarlinBlk* kbp, double  searchsp);

/** Calculate the cutoff score from the expected number of HSPs or vice versa.
 * @param S The calculated score [in] [out]
 * @param E The calculated e-value [in] [out]
 * @param kbp The Karlin-Altschul statistical parameters [in]
 * @param searchsp The effective search space [in]
 * @param dodecay Use gap decay feature? [in]
 * @param gap_decay_rate Gap decay rate to use, if dodecay is set [in]
 */
Int2 BLAST_Cutoffs (Int4 *S, double* E, BLAST_KarlinBlk* kbp, 
                    double searchsp, Boolean dodecay, double gap_decay_rate);

/* Functions to calculate SumE (for large and small gaps). */
double BLAST_SmallGapSumE (BLAST_KarlinBlk* kbp, Int4 gap, double gap_prob, double gap_decay_rate, Int2 num,  double xsum, Int4 query_length, Int4 subject_length);

double BLAST_UnevenGapSumE (BLAST_KarlinBlk* kbp, Int4 p_gap, Int4 n_gap, double gap_prob, double gap_decay_rate, Int2 num,  double xsum, Int4 query_length, Int4 subject_length);

double BLAST_LargeGapSumE (BLAST_KarlinBlk* kbp, double gap_prob, double gap_decay_rate, Int2 num,  double xsum, Int4 query_length, Int4 subject_length);

/*
Obtains arrays of the allowed opening and extension penalties for gapped BLAST for
the given matrix.  Also obtains arrays of Lambda, K, and H.  The pref_flags field is
used for display purposes, with the defines: BLAST_MATRIX_NOMINAL, BLAST_MATRIX_PREFERRED, and
BLAST_MATRIX_BEST.

Any of these fields that
are not required should be set to NULL.  The Int2 return value is the length of the
arrays.
*/

void BLAST_GetAlphaBeta (char* matrixName, double *alpha,
double *beta, Boolean gapped, Int4 gap_open, Int4 gap_extend);

#ifdef __cplusplus
}
#endif
#endif /* !__BLASTKAR__ */
