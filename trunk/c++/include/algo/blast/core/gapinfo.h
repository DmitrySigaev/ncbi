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

/** @file gapinfo.h
 * Structures definitions from gapxdrop.h in ncbitools 
 * @todo FIXME: doxygen comments
 */

#ifndef __GAPINFO__
#define __GAPINFO__

#ifdef __cplusplus
extern "C" {
#endif
#include <algo/blast/core/blast_def.h>

/** Operation types within the edit script*/
typedef enum EGapAlignOpType { 
   eGapAlignDel = 0, /**< Deletion: a gap in query */
   eGapAlignDel2 = 1,/**< Frame shift deletion of two nucleotides */
   eGapAlignDel1 = 2,/**< Frame shift deletion of one nucleotide */
   eGapAlignSub = 3, /**< Substitution */
   eGapAlignIns1 = 4,/**< Frame shift insertion of one nucleotide */
   eGapAlignIns2 = 5,/**< Frame shift insertion of two nucleotides */
   eGapAlignIns = 6, /**< Insertion: a gap in subject */
   eGapAlignDecline = 7 /**< Non-aligned region */
} EGapAlignOpType;

/** Edit script: linked list of correspondencies between two sequences */
typedef struct GapEditScript {
   EGapAlignOpType op_type;    /**< Type of operation */
   Int4 num;                   /**< Number of operations */
   struct GapEditScript* next; /**< Pointer to next link */
} GapEditScript;

typedef struct GapEditBlock {
    Int4 start1,  start2,       /* starts of alignments. */
        length1, length2,       /* total lengths of the sequences. */
        original_length1,	/* Untranslated lengths of the sequences. */
        original_length2;	
    Int2 frame1, frame2;	    /* frames of the sequences. */
    Boolean translate1,	translate2; /* are either of these be translated. */
    Boolean reverse;	/* reverse sequence 1 and 2 when producing SeqALign? */
    Boolean is_ooframe; /* Is this out_of_frame edit block? */
    Boolean discontinuous; /* Is this OK to produce discontinuous SeqAlign? */
    GapEditScript* esp;
} GapEditBlock;

/*
	Structure to keep memory for state structure.
*/
typedef struct GapStateArrayStruct {
	Int4 	length,		/* length of the state_array. */
		used;		/* how much of length is used. */
	Uint1* state_array;	/* array to be used. */
	struct GapStateArrayStruct* next;
} GapStateArrayStruct;

GapEditScript* 
GapEditScriptNew (GapEditScript* old);

GapEditScript* GapEditScriptDelete (GapEditScript* esp);

GapEditBlock* GapEditBlockNew (Int4 start1, Int4 start2);
GapEditBlock* GapEditBlockDelete (GapEditBlock* edit_block);
GapStateArrayStruct* 
GapStateFree(GapStateArrayStruct* state_struct);

/** Convert internal traceback integer array into a GapEditBlock structure.
 * @param S The traceback obtained from ALIGN [in]
 * @param M Length of alignment in query [in]
 * @param N Length of alignment in subject [in]
 * @param start1 Starting query offset [in]
 * @param start2 Starting subject offset [in]
 * @param edit_block The constructed edit block [out]
 */
Int2
BLAST_TracebackToGapEditBlock(Int4* S, Int4 M, Int4 N, Int4 start1,
                               Int4 start2, GapEditBlock** edit_block);

#ifdef __cplusplus
}
#endif
#endif /* !__GAPINFO__ */
