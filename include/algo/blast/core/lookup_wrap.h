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

/** @file lookup_wrap.h
 * Wrapper for all lookup tables used in BLAST
 */

#ifndef __LOOKUP_WRAP__
#define __LOOKUP_WRAP__

#include <algo/blast/core/blast_def.h>
#include <algo/blast/core/blast_options.h>
#include <algo/blast/core/blast_rps.h>
#include <algo/blast/core/blast_stat.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Wrapper structure for different types of BLAST lookup tables */
typedef struct LookupTableWrap {
   Uint1 lut_type; /**< What kind of a lookup table it is? */
   void* lut; /**< Pointer to the actual lookup table structure */
} LookupTableWrap;

/** Create the lookup table for all query words.
 * @param query The query sequence [in]
 * @param lookup_options What kind of lookup table to build? [in]
 * @param lookup_segments Locations on query to be used for lookup table
 *                        construction [in]
 * @param sbp Scoring block containing matrix [in]
 * @param lookup_wrap_ptr The initialized lookup table [out]
 * @param rps_info Structure containing RPS blast setup information [in]
 */
Int2 LookupTableWrapInit(BLAST_SequenceBlk* query, 
        const LookupTableOptions* lookup_options,	
        ListNode* lookup_segments, BlastScoreBlk* sbp, 
        LookupTableWrap** lookup_wrap_ptr, RPSInfo *rps_info);

/** Deallocate memory for the lookup table */
LookupTableWrap* LookupTableWrapFree(LookupTableWrap* lookup);

#define OFFSET_ARRAY_SIZE 4096

/** Determine the size of the offsets arrays to be filled by
 * the ScanSubject function.
 */
Int4 GetOffsetArraySize(LookupTableWrap* lookup);

#ifdef __cplusplus
}
#endif
#endif /* !__LOOKUP_WRAP__ */
