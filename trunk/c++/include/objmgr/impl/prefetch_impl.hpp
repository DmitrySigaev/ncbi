#ifndef PREFETCH_IMPL__HPP
#define PREFETCH_IMPL__HPP

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
* Author: Aleksey Grichenko, Eugene Vasilchenko
*
* File Description:
*   Prefetch implementation
*
*/

#include <corelib/ncbistd.hpp>
#include <corelib/ncbiobj.hpp>
#include <corelib/ncbithr.hpp>
#include <objmgr/data_loader.hpp>
#include <vector>
#include <list>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CScope;
class CSeq_id;
class CSeq_id_Handle;
class CBioseq_Handle;
class CPrefetchThread;

class CPrefetchToken_Impl : public CObject
{
public:
    typedef vector<CSeq_id_Handle> TIds;
    typedef CDataLoader::TTSE_Lock TTSE_Lock;

    ~CPrefetchToken_Impl(void);

private:
    friend class CPrefetchToken;
    friend class CPrefetchThread;

    CPrefetchToken_Impl(CScope& scope, const CSeq_id& id);
    CPrefetchToken_Impl(CScope& scope, const CSeq_id_Handle& id);
    CPrefetchToken_Impl(CScope& scope, const TIds& ids);

    void x_InitPrefetch(CScope& scope);

    // Hide copy methods
    CPrefetchToken_Impl(const CPrefetchToken_Impl&);
    CPrefetchToken_Impl& operator=(const CPrefetchToken_Impl&);

    operator bool(void) const;
    CBioseq_Handle NextBioseqHandle(CScope& scope);

    void AddTokenReference(void);
    void RemoveTokenReference(void);

    // Called by fetching function when id is loaded
    void AddResolvedId(size_t id_idx, TTSE_Lock tse);

    typedef vector<TTSE_Lock> TFetchedTSEs;

    int            m_TokenCount;  // Number of tokens referencing this impl
    TIds           m_Ids;         // requested ids in the original order
    size_t         m_CurrentId;   // next id to return
    TFetchedTSEs   m_TSEs;        // loaded TSEs
    mutable CFastMutex m_Lock;
};


class CPrefetchThread : public CThread
{
public:
    CPrefetchThread(CDataSource& data_source);

    // Add request to the fetcher queue
    void AddRequest(CPrefetchToken_Impl& token);

    // Stop the thread (since Exit() can not be called from
    // data loader's destructor).
    void Terminate(void);

protected:
    virtual void* Main(void);
    // protected destructor as required by CThread
    ~CPrefetchThread(void);

private:
    // Using list to be able to delete elements
    typedef list<CRef<CPrefetchToken_Impl> > TPrefetchQueue;

    CDataSource&   m_DataSource;
    TPrefetchQueue m_Queue;
    CFastMutex     m_Lock;
    CSemaphore     m_Semaphore; // Queue signal
    bool           m_Stop;      // used to stop the thread
};


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.1  2004/04/16 13:30:34  grichenk
* Initial revision
*
*
* ===========================================================================
*/

#endif  // PREFETCH_IMPL__HPP
