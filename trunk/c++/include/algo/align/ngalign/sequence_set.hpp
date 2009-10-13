#ifndef NGALIGN_SEQUENCE_SET__HPP
#define NGALIGN_SEQUENCE_SET__HPP

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

#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objmgr/scope.hpp>
#include <objects/seqalign/Seq_align.hpp>
#include <objects/seqalign/Seq_align_set.hpp>
#include <objects/seqalign/Dense_seg.hpp>

#include <algo/winmask/seq_masker.hpp>

#include <algo/blast/api/blast_types.hpp>
#include <algo/blast/api/blast_options_handle.hpp>
#include <algo/blast/api/sseqloc.hpp>
#include <algo/blast/api/query_data.hpp>
#include <algo/blast/api/uniform_search.hpp>
#include <algo/blast/api/local_db_adapter.hpp>
#include <algo/blast/api/objmgr_query_data.hpp>

#include <algo/align/ngalign/ngalign_interface.hpp>


BEGIN_NCBI_SCOPE

BEGIN_SCOPE(objects)
    class CScope;
    class CSeq_align;
    class CSeq_align_set;
    class CSeq_id;
END_SCOPE(objects)

BEGIN_SCOPE(blast)
    class SSeqLoc;
    class IQueryFactory;
    class CLocalDbAdapter;
    class CSearchDatabase;
END_SCOPE(blast)


class CBlastDbSet : public ISequenceSet
{
public:
    CBlastDbSet(const string& BlastDb);

    enum {
        eNoSoftFiltering = -1
    };

    void SetSoftFiltering(int Filter) { m_Filter = Filter; }

    CRef<blast::IQueryFactory> CreateQueryFactory(
            objects::CScope& Scope,
            const blast::CBlastOptionsHandle& BlastOpts);
    CRef<blast::IQueryFactory> CreateQueryFactory(
            objects::CScope& Scope,
            const blast::CBlastOptionsHandle& BlastOpts,
            const CAlignResultsSet& Alignments, int Threshold);
    CRef<blast::CLocalDbAdapter> CreateLocalDbAdapter(
            objects::CScope& Scope,
            const blast::CBlastOptionsHandle& BlastOpts);

protected:
    string m_BlastDb;
    int m_Filter;
};


class CSeqIdListSet : public ISequenceSet
{
public:
    CSeqIdListSet();

    list<CRef<objects::CSeq_id> >& SetIdList();
    void SetSeqMasker(CSeqMasker* SeqMasker);

    CRef<blast::IQueryFactory> CreateQueryFactory(
            objects::CScope& Scope, const blast::CBlastOptionsHandle& BlastOpts);
    CRef<blast::IQueryFactory> CreateQueryFactory(
            objects::CScope& Scope, const blast::CBlastOptionsHandle& BlastOpts,
            const CAlignResultsSet& Alignments, int Threshold);
    CRef<blast::CLocalDbAdapter> CreateLocalDbAdapter(
            objects::CScope& Scope, const blast::CBlastOptionsHandle& BlastOpts);

protected:
    list<CRef<objects::CSeq_id> > m_SeqIdList;
    CSeqMasker* m_SeqMasker;
};



class CFastaFileSet : public ISequenceSet
{
public:
    CFastaFileSet(CNcbiIstream* FastaStream);

    void EnableLowerCaseMasking(bool LowerCaseMasking);

    CRef<blast::IQueryFactory> CreateQueryFactory(
            objects::CScope& Scope, const blast::CBlastOptionsHandle& BlastOpts);
    CRef<blast::IQueryFactory> CreateQueryFactory(
            objects::CScope& Scope,
            const blast::CBlastOptionsHandle& BlastOpts,
            const CAlignResultsSet& Alignments, int Threshold);
    CRef<blast::CLocalDbAdapter> CreateLocalDbAdapter(
            objects::CScope& Scope, const blast::CBlastOptionsHandle& BlastOpts);

protected:
    CNcbiIstream* m_FastaStream;
    bool m_LowerCaseMasking;
};





END_NCBI_SCOPE

#endif
