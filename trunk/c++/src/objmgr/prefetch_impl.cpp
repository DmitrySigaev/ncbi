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

#include <ncbi_pch.hpp>
#include <objmgr/impl/prefetch_impl.hpp>
#include <corelib/ncbimtx.hpp>
#include <objmgr/bioseq_handle.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/impl/tse_info.hpp>
#include <objmgr/impl/bioseq_info.hpp>
#include <objmgr/impl/scope_impl.hpp>
#include <objmgr/impl/data_source.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

// NOTE: Max. value for semaphore must be prefetch depth + 1, because
// one extra-Post will be called when the token impl. is released.

CPrefetchToken_Impl::CPrefetchToken_Impl(const TIds& ids, unsigned int depth)
    : m_TokenCount(0),
      m_TSESemaphore(depth, max(depth+1, depth)),
      m_Non_locking(false)
{
    m_Ids = ids;
}


CPrefetchToken_Impl::~CPrefetchToken_Impl(void)
{
    return;
}


void CPrefetchToken_Impl::x_InitPrefetch(CScope& scope)
{
    m_TSEs.resize(m_Ids.size());
    m_CurrentId = 0;
    CRef<CDataSource> source(scope.GetImpl().GetFirstLoaderSource());
    if (!source) {
        return;
    }
    source->Prefetch(*this);
}


void CPrefetchToken_Impl::x_SetNon_locking(void)
{
    m_Non_locking = true;
}


void CPrefetchToken_Impl::AddResolvedId(size_t id_idx, TTSE_Lock tse)
{
    CFastMutexGuard guard(m_Lock);
    if ( m_Non_locking ) {
        m_TSESemaphore.Post();
        return;
    }
    if (m_Ids.empty()  ||  id_idx < m_CurrentId) {
        // Token has been cleaned or id already passed, do not lock the TSE
        return;
    }
    m_TSEs[id_idx] = tse;
    int count = ++m_TSEMap[tse];
    if (count > 1) {
        // One more ID found in a prefetched TSE
        m_TSESemaphore.Post();
    }
}


bool CPrefetchToken_Impl::IsEmpty(void) const
{
    CFastMutexGuard guard(m_Lock);
    return m_Ids.empty();
}


CPrefetchToken_Impl::operator bool(void) const
{
    CFastMutexGuard guard(m_Lock);
    return m_CurrentId < m_Ids.size();
}


CBioseq_Handle CPrefetchToken_Impl::NextBioseqHandle(CScope& scope)
{
    TTSE_Lock tse;
    CSeq_id_Handle id;
    {{
        CFastMutexGuard guard(m_Lock);
        // Can not call bool(*this) - creates deadlock
        _ASSERT(m_CurrentId < m_Ids.size());
        id = m_Ids[m_CurrentId];
        // Keep temporary TSE lock
        tse = m_TSEs[m_CurrentId];
        m_TSEs[m_CurrentId].Reset();
        ++m_CurrentId;
        if ( tse ) {
            TTSE_Map::iterator it = m_TSEMap.find(tse);
            if ( --(it->second) < 1 ) {
                m_TSEMap.erase(it);
                // Signal that next TSE or next token may be prefetched
                m_TSESemaphore.Post();
            }
        }
    }}
    return scope.GetBioseqHandle(id);
}


void CPrefetchToken_Impl::AddTokenReference(void)
{
    ++m_TokenCount;
}


void CPrefetchToken_Impl::RemoveTokenReference(void)
{
    if ( !(--m_TokenCount) ) {
        // No more tokens, reset the queue
        CFastMutexGuard guard(m_Lock);
        m_Ids.clear();
        m_TSEs.clear();
        m_CurrentId = 0;
        // Allow the thread to process next token
        m_TSESemaphore.Post();
    }
}


CPrefetchThread::CPrefetchThread(CDataSource& data_source)
    : m_DataSource(data_source),
      m_Stop(false)
{
    return;
}


CPrefetchThread::~CPrefetchThread(void)
{
    return;
}


void CPrefetchThread::AddRequest(CPrefetchToken_Impl& token)
{
    {{
        CFastMutexGuard guard(m_Lock);
        m_Queue.Put(Ref(&token));
    }}
}


void CPrefetchThread::Terminate(void)
{
    {{
        CFastMutexGuard guard(m_Lock);
        m_Stop = true;
    }}
    // Unlock the thread
    m_Queue.Put(CRef<CPrefetchToken_Impl>(0));
}


void* CPrefetchThread::Main(void)
{
    do {
        CRef<CPrefetchToken_Impl> token = m_Queue.Get();
        {{
            CFastMutexGuard guard(m_Lock);
            if (m_Stop) {
                return 0;
            }
            _ASSERT( token );
            if ( token->IsEmpty() ) {
                // Token may have been canceled
                continue;
            }
        }}
        bool release_token = false;
        for (size_t i = 0; ; ++i) {
            {{
                CFastMutexGuard guard(m_Lock);
                if (m_Stop) {
                    return 0;
                }
            }}
            CSeq_id_Handle id;
            token->m_TSESemaphore.Wait();
            {{
                // m_Ids may be cleaned up by the token, check size
                // on every iteration.
                CFastMutexGuard guard(token->m_Lock);
                i = max(i, token->m_CurrentId);
                if (i >= token->m_Ids.size()) {
                    // Can not release token now - mutex is still locked
                    release_token = true;
                    break;
                }
                id = token->m_Ids[i];
            }}
            try {
                SSeqMatch_DS match = m_DataSource.BestResolve(id);
                if ( match ) {
                    token->AddResolvedId(i, match.m_TSE_Lock);
                }
            } catch (...) {
                // BestResolve() failed, go to the next id.
            }
        }
        if (release_token) {
            token.Reset();
        }
    } while (true);
    return 0;
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.7  2004/12/22 15:56:24  vasilche
* Use typedefs for internal containers.
* Use SSeqMatch_DS instead of CSeqMatch_Info.
*
* Revision 1.6  2004/08/04 14:53:26  vasilche
* Revamped object manager:
* 1. Changed TSE locking scheme
* 2. TSE cache is maintained by CDataSource.
* 3. CObjectManager::GetInstance() doesn't hold CRef<> on the object manager.
* 4. Fixed processing of split data.
*
* Revision 1.5  2004/05/21 21:42:12  gorelenk
* Added PCH ncbi_pch.hpp
*
* Revision 1.4  2004/05/07 13:47:35  grichenk
* Removed single-id constructors.
* Added non-locking prefetch mode.
*
* Revision 1.3  2004/04/26 14:15:33  grichenk
* Catch exceptions in the prefetching thread
*
* Revision 1.2  2004/04/19 14:52:29  grichenk
* Added prefetch depth limit, redesigned prefetch queue.
*
* Revision 1.1  2004/04/16 13:30:34  grichenk
* Initial revision
*
*
* ===========================================================================
*/
