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
 * Author: Ilya Dondoshansky
 *
 */

/** @file lookup_wrap.c
 * Wrapper for different flavors of lookup tables allowing a uniform interface in the code.
 * The wrapper (LookupTableWrap) contains an unsigned byte specifying the type of lookup 
 * table as well as a void pointer pointing to the actual lookup table.  Examples of different 
 * types of lookup tables are those for protein queries, the "standard" nucleotide one, the 
 * megablast lookup table, etc.
 */

#ifndef SKIP_DOXYGEN_PROCESSING
static char const rcsid[] = 
    "$Id$";
#endif /* SKIP_DOXYGEN_PROCESSING */

#include <algo/blast/core/lookup_wrap.h>
#include <algo/blast/core/blast_aalookup.h>
#include <algo/blast/core/blast_nalookup.h>
#include <algo/blast/core/phi_lookup.h>
#include <algo/blast/core/blast_filter.h>
#include <algo/blast/core/lookup_util.h>
#include <algo/blast/core/blast_rps.h>
#include <algo/blast/core/blast_encoding.h>

Int2 LookupTableWrapInit(BLAST_SequenceBlk* query, 
        const LookupTableOptions* lookup_options,	
        BlastSeqLoc* lookup_segments, BlastScoreBlk* sbp, 
        LookupTableWrap** lookup_wrap_ptr, const BlastRPSInfo *rps_info,
        Blast_Message* *error_msg)
{
   Int2 status = 0;
   LookupTableWrap* lookup_wrap;

   if (error_msg)
      *error_msg = NULL;

   /* Construct the lookup table. */
   *lookup_wrap_ptr = lookup_wrap = 
      (LookupTableWrap*) calloc(1, sizeof(LookupTableWrap));
   lookup_wrap->lut_type = lookup_options->lut_type;

   switch ( lookup_options->lut_type ) {
   case eAaLookupTable:
       {
       Int4** matrix = NULL;
       Boolean has_pssm = FALSE;
       if (sbp->psi_matrix && sbp->psi_matrix->pssm) {
           matrix = sbp->psi_matrix->pssm->data;
           has_pssm = TRUE;
       } else {
           matrix = sbp->matrix->data;
       }
       BlastAaLookupTableNew(lookup_options, (BlastAaLookupTable* *)
                             &lookup_wrap->lut);
       ((BlastAaLookupTable*)lookup_wrap->lut)->use_pssm = has_pssm;
       BlastAaLookupIndexQuery( (BlastAaLookupTable*) lookup_wrap->lut, matrix, 
                                 query, lookup_segments, 0);
       BlastAaLookupFinalize((BlastAaLookupTable*) lookup_wrap->lut);
       }
      break;

   case eIndexedMBLookupTable:
      /* for indexed megablast, lookup table data is initialized
         in the API layer, not here */
      lookup_wrap->lut = NULL;
      break;

   case eNaLookupTable:
   case eMBLookupTable:
      {
          Int4 lut_width;
          Int4 num_table_entries = EstimateNumTableEntries(lookup_segments);
    
          lookup_wrap->lut_type = BlastChooseNaLookupTable(
                                     lookup_options, num_table_entries,
                                     &lut_width);
    
          if (lookup_wrap->lut_type == eMBLookupTable) {
             BlastMBLookupTableNew(query, lookup_segments, 
                               (BlastMBLookupTable* *) &(lookup_wrap->lut), 
                               lookup_options, num_table_entries, lut_width);
          }
          else {
             BlastNaLookupTableNew(query, lookup_segments,
                            (BlastNaLookupTable* *) &(lookup_wrap->lut), 
                             lookup_options, lut_width);
          }
      }
      break;

   case ePhiLookupTable: case ePhiNaLookupTable:
       {
           const Boolean kIsDna = 
                          (lookup_options->lut_type == ePhiNaLookupTable);
           status = SPHIPatternSearchBlkNew(lookup_options->phi_pattern, kIsDna, sbp,
                             (SPHIPatternSearchBlk* *) &(lookup_wrap->lut),
                             error_msg);
           break;
       }

   case eRPSLookupTable:
       {
           BlastRPSLookupTable *lookup;
           Int4 alphabet_size;
           RPSLookupTableNew(rps_info, (BlastRPSLookupTable* *)
                                        (&lookup_wrap->lut));

           /* if the alphabet size from the RPS database is too
              small, mask all unsupported query letters */
           lookup = (BlastRPSLookupTable*)(lookup_wrap->lut);
           alphabet_size = lookup->alphabet_size;
           if (alphabet_size < BLASTAA_SIZE)
               Blast_MaskUnsupportedAA(query, alphabet_size);
           break;
       }
   } /* end switch */

   return status;
}

LookupTableWrap* LookupTableWrapFree(LookupTableWrap* lookup)
{
   if (!lookup)
       return NULL;

   if (lookup->lut_type == eMBLookupTable) {
      lookup->lut = (void*) 
         BlastMBLookupTableDestruct((BlastMBLookupTable*)lookup->lut);
   } else if( lookup->lut_type == eIndexedMBLookupTable ) {
       lookup->lut = NULL;
   } else if (lookup->lut_type == ePhiLookupTable || 
              lookup->lut_type == ePhiNaLookupTable) {
       lookup->lut = (void*)
           SPHIPatternSearchBlkFree((SPHIPatternSearchBlk*)lookup->lut);
   } else if (lookup->lut_type == eRPSLookupTable) {
      lookup->lut = (void*) 
         RPSLookupTableDestruct((BlastRPSLookupTable*)lookup->lut);
   } else if (lookup->lut_type == eNaLookupTable) {
      lookup->lut = (void*) 
         BlastNaLookupTableDestruct((BlastNaLookupTable*)lookup->lut);
   } else {
      lookup->lut = (void*) 
         BlastAaLookupTableDestruct((BlastAaLookupTable*)lookup->lut);
   }
   sfree(lookup);
   return NULL;
}

Int4 GetOffsetArraySize(LookupTableWrap* lookup)
{
   Int4 offset_array_size;

   switch (lookup->lut_type) {
   case eMBLookupTable:
      offset_array_size = OFFSET_ARRAY_SIZE + 
         ((BlastMBLookupTable*)lookup->lut)->longest_chain;
      break;
   case eAaLookupTable: 
      offset_array_size = OFFSET_ARRAY_SIZE + 
         ((BlastAaLookupTable*)lookup->lut)->longest_chain;
      break;
   case eNaLookupTable:
      offset_array_size = OFFSET_ARRAY_SIZE + 
         ((BlastNaLookupTable*)lookup->lut)->longest_chain;
      break;
   default:
      offset_array_size = OFFSET_ARRAY_SIZE;
      break;
   }
   return offset_array_size;
}
