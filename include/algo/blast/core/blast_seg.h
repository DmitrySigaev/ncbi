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

/** @file blast_seg.h
 * SEG filtering functions. @todo FIXME: should this be combined with
 * blast_filter/dust? Needs doxygen documentation and comments
 */

#ifndef __BLAST_SEG__
#define __BLAST_SEG__

#include <algo/blast/core/ncbi_std.h>
#include <algo/blast/core/blast_def.h>


#ifdef __cplusplus
extern "C" {
#endif

/** Structure to hold parameters for seg search.
 */
typedef struct SegParameters
  {
   Int4 window;         /**< initial window size to trigger further work. */
   double locut;        
   double hicut;
   Int4 period;
   Int4 hilenmin;
   Boolean overlaps;	/* merge overlapping pieces if TRUE. */
   Int4 maxtrim;
   Int4 maxbogus;
  } SegParameters;

/** Allocated SeqParameter struct for proteins and fills with default values.
 * @return pointer to SegParameters
 */
NCBI_XBLAST_EXPORT
SegParameters* SegParametersNewAa (void);

/** Free SegParameters structure
 * @param sparamsp object to be freed [in]
 */
NCBI_XBLAST_EXPORT
void SegParametersFree(SegParameters* sparamsp);

/** Runs seg on a protein sequence in ncbistdaa.
 * @param sequence the protein residues in ncbistdaa [in]
 * @param length number of redidues [in]
 * @param offset amount to shift over resulting locations 
 *    (if full sequence not passed in) [in]
 * @param sparamsp the seg parameters created with SegParametersNewAa [in]
 * @param seg_locs resulting locations for filtering [out]
 * @return zero on success
 */
NCBI_XBLAST_EXPORT
Int2 SeqBufferSeg (Uint1* sequence, Int4 length, Int4 offset,
                   SegParameters* sparamsp, BlastSeqLoc** seg_locs);

#ifdef __cplusplus
}
#endif
#endif /* !__BLAST_FILTER__ */
