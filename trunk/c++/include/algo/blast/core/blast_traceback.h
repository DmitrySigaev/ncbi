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

File name: blast_traceback.h

Author: Ilya Dondoshansky

Contents: Functions to do gapped alignment with traceback and/or convert 
          results to the SeqAlign form

******************************************************************************
 * $Revision$
 * */
#ifndef __BLAST_TRACEBACK__
#define __BLAST_TRACEBACK__

#ifdef __cplusplus
extern "C" {
#endif

#include <algo/blast/core/blast_seqsrc.h>
#include <algo/blast/core/blast_gapalign.h>

/** Given the preliminary alignment results from a database search, redo 
 * the gapped alignment with traceback, if it has not yet been done.
 * @param program_number Type of the BLAST program [in]
 * @param results Results of this BLAST search [in] [out]
 * @param query The query sequence [in]
 * @param query_info Information about the query [in]
 * @param rdfp BLAST database structure [in]
 * @param gap_align The auxiliary structure for gapped alignment [in]
 * @param score_options The scoring related options [in]
 * @param ext_params Gapped extension parameters [in]
 * @param hit_params Parameters for saving hits [in]
 * @param db_options Options containing database genetic code string [in]
 * @param psi_options Options specific to PSI BLAST [in]
 */
Int2 BLAST_ComputeTraceback(Uint1 program_number, BlastHSPResults* results, 
        BLAST_SequenceBlk* query, BlastQueryInfo* query_info, 
        const BlastSeqSrc* bssp, BlastGapAlignStruct* gap_align,
        const BlastScoringOptions* score_options,
        const BlastExtensionParameters* ext_params,
        BlastHitSavingParameters* hit_params,
        const BlastDatabaseOptions* db_options,
        const PSIBlastOptions* psi_options);

/** Given the preliminary alignment results from a two sequences search
 * (possibly with multiple query sequences), redo the gapped alignment
 * with traceback, if it has not yet been done.
 * @param program_number Type of the BLAST program [in]
 * @param results Results of this BLAST search [in] [out]
 * @param query The query sequence [in]
 * @param query_info Information about the query [in]
 * @param subject The subject sequence [in]
 * @param gap_align The auxiliary structure for gapped alignment [in]
 * @param score_options The scoring related options [in]
 * @param ext_params Gapped extension parameters [in]
 * @param hit_params Parameters for saving hits [in]
 * @param db_options Options containing database genetic code string [in]
 */
Int2 BLAST_TwoSequencesTraceback(Uint1 program_number, 
        BlastHSPResults* results, BLAST_SequenceBlk* query, 
        BlastQueryInfo* query_info, BLAST_SequenceBlk* subject, 
        BlastGapAlignStruct* gap_align, 
        const BlastScoringOptions* score_options,
        BlastExtensionParameters* ext_params,
        BlastHitSavingParameters* hit_params,
        const BlastDatabaseOptions* db_options);

#ifdef __cplusplus
}
#endif
#endif /* !__BLAST_TRACEBACK__ */

