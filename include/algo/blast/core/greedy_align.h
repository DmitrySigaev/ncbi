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

File name: greedy_align.h

Author: Ilya Dondoshansky

Contents: Copy of mbalign.h from ncbitools library

******************************************************************************
 * $Revision$
 * */
#ifndef _GREEDY_H_
#define _GREEDY_H_

#include <blast_def.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef Uint4 edit_op_t; /* 32 bits */
typedef struct {
    edit_op_t *op;                  /* array of edit operations */
    Uint4 size, num;         /* size of allocation, number in use */
    edit_op_t last;                 /* most recent operation added */
} MBGapEditScript;

MBGapEditScript *MBGapEditScriptFree(MBGapEditScript *es);
MBGapEditScript *MBGapEditScriptNew(void);
MBGapEditScript *MBGapEditScriptAppend(MBGapEditScript *es, MBGapEditScript *et);

enum {
    EDIT_OP_MASK = 0x3,
    EDIT_OP_ERR  = 0x0,
    EDIT_OP_INS  = 0x1,
    EDIT_OP_DEL  = 0x2,
    EDIT_OP_REP  = 0x3
};

enum {         /* half of the (fixed) match score */
    ERROR_FRACTION=2,  /* 1/this */
    MAX_SPACE=1000000,
    sC = 0, sI = 1, sD = 2, LARGE=100000000
};

#define ICEIL(x,y) ((((x)-1)/(y))+1)

/* ----- pool allocator ----- */
typedef struct ThreeVal {
    Int4 I, C, D;
} ThreeVal,* ThreeValPtr;

typedef struct MBSpace {
    ThreeValPtr space_array;
    Int4 used, size;
    struct MBSpace *next;
} MBSpace, *MBSpacePtr;

#define EDIT_VAL(op) (op >> 2)

#define EDIT_OPC(op) (op & EDIT_OP_MASK)

MBSpacePtr MBSpaceNew(void);
void MBSpaceFree(MBSpacePtr sp);

typedef struct GreedyAlignMem {
   Int4Ptr* flast_d;
   Int4Ptr max_row_free;
   ThreeValPtr* flast_d_affine;
   Int4Ptr uplow_free;
   MBSpacePtr space;
} GreedyAlignMem,* GreedyAlignMemPtr;

Int4 
BLAST_GreedyAlign (const Uint1Ptr s1, Int4 len1,
			     const Uint1Ptr s2, Int4 len2,
			     Boolean reverse, Int4 xdrop_threshold, 
			     Int4 match_cost, Int4 mismatch_cost,
			     Int4Ptr e1, Int4Ptr e2, GreedyAlignMemPtr abmp, 
			     MBGapEditScript *S, Uint1 rem);
Int4 
BLAST_AffineGreedyAlign (const Uint1Ptr s1, Int4 len1,
				  const Uint1Ptr s2, Int4 len2,
				  Boolean reverse, Int4 xdrop_threshold, 
				  Int4 match_cost, Int4 mismatch_cost,
				  Int4 gap_open, Int4 gap_extend,
				  Int4Ptr e1, Int4Ptr e2, 
				  GreedyAlignMemPtr abmp, 
				  MBGapEditScript *S, Uint1 rem);

#ifdef __cplusplus
}
#endif
#endif /* _GREEDY_H_ */
