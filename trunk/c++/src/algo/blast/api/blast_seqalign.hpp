/*  $Id$
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
* Author:  Christiam Camacho
*
* File Description:
*   Utility function to convert internal BLAST result structures into
*   CSeq_align_set objects
*
*/

#ifndef BLASTSEQALIGN__HPP
#define BLASTSEQALIGN__HPP

#include <corelib/ncbistd.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqalign/seqalign__.hpp>

// NewBlast includes
#include <blast_hits.h>
#include <gapinfo.h>

// Blast++ includes
#include <BlastOption.hpp>

USING_NCBI_SCOPE;
USING_SCOPE(objects);

#ifndef BLAST_SMALL_GAPS
#define BLAST_SMALL_GAPS 0
#endif

/** Converts BlastResults structure into CSeq_align_set class (handles 
 * query concatenation).
 * @param results results from running the BLAST algorithm [in]
 * @param prog type of BLAST program [in]
 * @param query_seqids list of sequence identifiers (to allow query
 * concatenation) [in]
 * @param bssp handle to BLAST Sequence Source ADT [in]
 * @param subject_seqid sequence identifier (for 2 sequences search) [in]
 * @param score_options contains scoring options [in]
 * @param sbp scoring and statistical information [in]
 * @return set of CSeq_align objects
 */
CRef<CSeq_align_set>
BLAST_Results2CppSeqAlign(const BlastResults* results, 
        CBlastOption::EProgram prog,
        vector< CConstRef<CSeq_id> >& query_seqids, 
        const BlastSeqSrcPtr bssp, 
        CConstRef<CSeq_id>& subject_seqid,
        const BlastScoringOptionsPtr score_options, 
        const BLAST_ScoreBlkPtr sbp);

/*
* ===========================================================================
*
* $Log$
* Revision 1.3  2003/07/25 22:12:46  camacho
* Use BLAST Sequence Source to retrieve sequence identifier
*
* Revision 1.2  2003/07/23 21:28:23  camacho
* Use new local gapinfo structures
*
* Revision 1.1  2003/07/10 18:34:19  camacho
* Initial revision
*
*
* ===========================================================================
*/

#endif  /* BLASTSEQALIGN__HPP */
