#ifndef NGALIGN_ALIGNMENT_SCORER__HPP
#define NGALIGN_ALIGNMENT_SCORER__HPP

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
 * Authors:  Nathan Bouk
 *
 * File Description:
 *
 */

#include <corelib/ncbistd.hpp>
#include <corelib/ncbiobj.hpp>
#include <objects/seqloc/Na_strand.hpp>
#include <hash_map.h>

#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objmgr/scope.hpp>
#include <objects/seqalign/Seq_align.hpp>
#include <objects/seqalign/Seq_align_set.hpp>
#include <objects/seqalign/Dense_seg.hpp>

#include <algo/blast/api/blast_types.hpp>
#include <algo/blast/api/blast_options_handle.hpp>
#include <algo/blast/api/sseqloc.hpp>

#include <algo/align/ngalign/ngalign_interface.hpp>


BEGIN_NCBI_SCOPE

BEGIN_SCOPE(objects)
    class CScope;
    class CSeq_align;
    class CSeq_align_set;
    class CSeq_id;
    class CDense_seg;
END_SCOPE(objects)

BEGIN_SCOPE(blast)
    class SSeqLoc;
    class CBlastOptionsHandle;
END_SCOPE(blast)



class CBlastScorer : public IAlignmentScorer
{
public:
    CBlastScorer() { ; }

    void ScoreAlignments(TAlignResultsRef Alignments, objects::CScope& Scope);
};


class CPctIdentScorer : public IAlignmentScorer
{
public:
    CPctIdentScorer() { ; }

    void ScoreAlignments(TAlignResultsRef Alignments, objects::CScope& Scope);
};


class CPctCoverageScorer : public IAlignmentScorer
{
public:
    CPctCoverageScorer() { ; }

    void ScoreAlignments(TAlignResultsRef Alignments, objects::CScope& Scope);
};


class CCommonComponentScorer : public IAlignmentScorer
{
public:
    CCommonComponentScorer() { ; }

    void ScoreAlignments(TAlignResultsRef Alignments, objects::CScope& Scope);

    void x_GetCompList(const objects::CSeq_id& Id,
                       list<CRef<objects::CSeq_id> >& CompIds,
                       objects::CScope& Scope);

    bool x_CompareCompLists(list<CRef<objects::CSeq_id> >& QueryIds,
                            list<CRef<objects::CSeq_id> >& SubjectIds);

};





END_NCBI_SCOPE

#endif
