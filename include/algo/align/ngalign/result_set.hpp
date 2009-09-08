#ifndef NGALIGN_RESULT_SET__HPP
#define NGALIGN_RESULT_SET__HPP

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

#include <algo/align/util/align_filter.hpp>



BEGIN_NCBI_SCOPE

BEGIN_SCOPE(objects)
    class CScope;
    class CSeq_align;
    class CSeq_align_set;
    class CSeq_id;
END_SCOPE(objects)

BEGIN_SCOPE(blast)
    class SSeqLoc;
    class CSearchResultSet;
    class CSearchResults;
END_SCOPE(blast)



class CQuerySet : public CObject
{
public:
    typedef map<string, CRef<objects::CSeq_align_set> > TSubjectToAlignSet;

    CQuerySet(const blast::CSearchResults& Results);
    CQuerySet(const objects::CSeq_align_set& Results);

    TSubjectToAlignSet& Get() { return m_SubjectMap; }
    const TSubjectToAlignSet& Get() const { return m_SubjectMap; }

    CRef<objects::CSeq_align_set> ToSeqAlignSet() const;
    CRef<objects::CSeq_align_set> ToBestSeqAlignSet() const;

    CConstRef<objects::CSeq_id> GetQueryId() const { return m_QueryId; }

    void Insert(CRef<CQuerySet> QuerySet);
    void Insert(const objects::CSeq_align_set& AlignSet);

    // gets the rank of the best (lowest) ranked alignment in this query set
    int GetBestRank() const;

private:

    TSubjectToAlignSet m_SubjectMap;
    CRef<objects::CSeq_id> m_QueryId;

    bool x_AlreadyContains(const objects::CSeq_align_set& Set,
                           const objects::CSeq_align& New) const;
};


class CAlignResultsSet : public CObject
{
public:
    typedef map<string, CRef<CQuerySet> > TQueryToSubjectSet;

    CAlignResultsSet() { ; }
    CAlignResultsSet(const blast::CSearchResultSet& BlastResults);

    TQueryToSubjectSet& Get() { return m_QueryMap; }
    const TQueryToSubjectSet& Get() const { return m_QueryMap; }

    bool QueryExists(const objects::CSeq_id& Id) const;
    CRef<CQuerySet> GetQuerySet(const objects::CSeq_id& Id);
    CConstRef<CQuerySet> GetQuerySet(const objects::CSeq_id& Id) const;


    CRef<objects::CSeq_align_set> ToSeqAlignSet() const;
    CRef<objects::CSeq_align_set> ToBestSeqAlignSet() const;

    void Insert(CRef<CQuerySet> QuerySet);
    void Insert(CRef<CAlignResultsSet> AlignSet);
    void Insert(const blast::CSearchResultSet& BlastResults);

    size_t size() const { return m_QueryMap.size(); }
    bool empty() const { return m_QueryMap.empty(); }

private:

    TQueryToSubjectSet m_QueryMap;

};





END_NCBI_SCOPE

#endif
