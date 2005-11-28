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
*/

/// @file blast_seqalign.hpp
/// Utility function to convert internal BLAST result structures into
/// objects::CSeq_align_set objects.

#ifndef ALGO_BLAST_API___BLAST_SEQALIGN__HPP
#define ALGO_BLAST_API___BLAST_SEQALIGN__HPP

#include <corelib/ncbistd.hpp>

#include <algo/blast/api/blast_types.hpp>
#include <algo/blast/core/blast_hits.h>
#include <algo/blast/api/blast_seqinfosrc.hpp>
#include <objects/seqalign/Seq_align.hpp>
#include <objects/seqalign/Seq_align_set.hpp>

/** @addtogroup AlgoBlast
 *
 * @{
 */

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
BEGIN_SCOPE(blast)

/// Forward declaration
class ILocalQueryData; 

/// Remaps Seq-align offsets relative to the query Seq-loc. 
/// Since the query strands were already taken into account when CSeq_align 
/// was created, only start position shifts in the CSeq_loc's are relevant in 
/// this function. 
/// @param sar Seq-align for a given query [in] [out]
/// @param query The query Seq-loc [in]
void
RemapToQueryLoc(CRef<CSeq_align> sar, const CSeq_loc & query);

/// Constructs an empty Seq-align-set containing an empty discontinuous
/// seq-align, and appends it to a previously constructed Seq-align-set.
/// @param sas Pointer to a Seq-align-set, to which new object should be 
///            appended (if not NULL).
/// @return Resulting Seq-align-set. 
CSeq_align_set*
CreateEmptySeq_align_set(CSeq_align_set* sas);

CRef<CSeq_align>
BLASTHspListToSeqAlign(EBlastProgramType program, 
                       BlastHSPList* hsp_list, 
                       const CSeq_id *query_id, 
                       const CSeq_id *subject_id,
                       Int4 query_length, 
                       Int4 subject_length,
                       bool is_ooframe);

CRef<CSeq_align>
BLASTUngappedHspListToSeqAlign(EBlastProgramType program, 
                               BlastHSPList* hsp_list, 
                               const CSeq_id *query_id, 
                               const CSeq_id *subject_id, 
                               Int4 query_length, 
                               Int4 subject_length);

/// Convert traceback output into Seq-align format.
/// 
/// This converts the traceback stage output into a standard
/// Seq-align.  The result_type argument indicates whether the
/// seqalign is for a database search or a sequence comparison
/// (eDatabaseSearch or eSequenceComparison).  The seqinfo_src
/// argument is used to translate oids into Seq-ids.
/// 
/// @param hsp_results
///   Results of a traceback search. [in]
/// @param local_data
///   The queries used to perform the search. [in]
/// @param seqinfo_src
///   Provides sequence identifiers and meta-data. [in]
/// @param program
///   The type of search done. [in]
/// @param gapped
///   True if this was a gapped search. [in]
/// @param oof_mode
///   True if out-of-frame matches are allowed. [in]
/// @param result_type
///   Specify how to arrange the results in the return value. [in]

TSeqAlignVector
LocalBlastResults2SeqAlign(BlastHSPResults   * hsp_results,
                           ILocalQueryData   & local_data,
                           const IBlastSeqInfoSrc& seqinfo_src,
                           EBlastProgramType   program,
                           bool                gapped,
                           bool                oof_mode,
                           EResultType         result_type = eDatabaseSearch);

END_SCOPE(blast)
END_NCBI_SCOPE

/* @} */

/*
* ===========================================================================
*
* $Log$
* Revision 1.33  2005/11/28 17:15:29  bealer
* - Doxyg.
*
* Revision 1.32  2005/11/09 20:56:26  camacho
* Refactorings to allow CPsiBl2Seq to produce Seq-aligns in the same format
* as CBl2Seq and reduce redundant code.
*
* Revision 1.31  2005/09/28 21:03:28  camacho
* Further rearrangement of headers/functions to segregate object manager dependencies.
*
* Revision 1.30  2005/09/28 20:11:09  camacho
* Added missing export specifier
*
* Revision 1.29  2005/09/28 18:23:08  camacho
* Rearrangement of headers/functions to segregate object manager dependencies.
*
* Revision 1.28  2005/07/21 17:17:28  bealer
* - OMF version of seqalign generation.
*
* Revision 1.27  2005/07/20 20:43:25  bealer
* - Minor constness change.
*
* Revision 1.26  2005/05/26 14:36:24  dondosha
* Added declaration of PHIBlast_Results2CSeqAlign
*
* Revision 1.25  2005/04/06 21:06:18  dondosha
* Use EBlastProgramType instead of EProgram in non-user-exposed functions
*
* Revision 1.24  2004/10/06 14:55:31  dondosha
* Use IBlastSeqInfoSrc interface in BLAST_Results2CSeqAlign; added Blast_RemapToSubjectLoc function
*
* Revision 1.23  2004/07/19 13:56:02  dondosha
* Pass subject SSeqLoc directly to BLAST_OneSubjectResults2CSeqAlign instead of BlastSeqSrc
*
* Revision 1.22  2004/06/07 21:34:55  dondosha
* Use 2 booleans for gapped and out-of-frame mode instead of scoring options in function arguments
*
* Revision 1.21  2004/06/07 18:26:29  dondosha
* Bit scores are now filled in HSP lists, so BlastScoreBlk is no longer needed when results are converted to seqalign
*
* Revision 1.20  2004/03/19 19:22:55  camacho
* Move to doxygen group AlgoBlast, add missing CVS logs at EOF
*
* Revision 1.19  2004/03/15 19:58:55  dondosha
* Added BLAST_OneSubjectResults2CSeqalign function to retrieve single subject results from BlastHSPResults
*
* Revision 1.18  2003/12/03 16:43:47  dondosha
* Renamed BlastMask to BlastMaskLoc, BlastResults to BlastHSPResults
*
* Revision 1.17  2003/11/26 18:36:45  camacho
* Renaming blast_option*pp -> blast_options*pp
*
* Revision 1.16  2003/10/30 21:40:36  dondosha
* Removed unneeded extra argument from BLAST_Results2CSeqAlign
*
* Revision 1.15  2003/08/19 20:24:17  dondosha
* Added TSeqAlignVector type as a return type for results-to-seqalign functions and input for formatting
*
* Revision 1.14  2003/08/19 13:45:21  dicuccio
* Removed 'USING_SCOPE(objects)'.  Changed #include guards to be standards
* compliant.  Added 'objects::' where necessary.
*
* Revision 1.13  2003/08/18 20:58:56  camacho
* Added blast namespace, removed *__.hpp includes
*
* Revision 1.12  2003/08/18 17:07:41  camacho
* Introduce new SSeqLoc structure (replaces pair<CSeq_loc, CScope>).
* Change in function to read seqlocs from files.
*
* Revision 1.11  2003/08/15 15:54:55  dondosha
* Pass seqloc-scope pairs to results2seqalign conversion functions
*
* Revision 1.10  2003/08/12 19:18:45  dondosha
* Use TSeqLocVector type in functions
*
* Revision 1.9  2003/08/11 19:55:04  camacho
* Early commit to support query concatenation and the use of multiple scopes.
* Compiles, but still needs work.
*
* Revision 1.8  2003/08/11 15:18:50  dondosha
* Added algo/blast/core to all #included headers
*
* Revision 1.7  2003/08/11 13:58:51  dicuccio
* Added export specifiers.  Fixed problem with unimplemented private copy ctor
* (truly make unimplemented)
*
* Revision 1.6  2003/08/08 19:42:14  dicuccio
* Compilation fixes: #include file relocation; fixed use of 'list' and 'vector'
* as variable names
*
* Revision 1.5  2003/08/01 17:40:56  dondosha
* Use renamed functions and structures from local blastkar.h
*
* Revision 1.4  2003/07/31 19:45:33  camacho
* Eliminate Ptr notation
*
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

#endif  /* ALGO_BLAST_API___BLAST_SEQALIGN__HPP */
