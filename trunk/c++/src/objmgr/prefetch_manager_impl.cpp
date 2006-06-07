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
* Author: Eugene Vasilchenko
*
* File Description:
*   Prefetch implementation
*
*/

#include <ncbi_pch.hpp>
#include <objmgr/impl/prefetch_manager_impl.hpp>
#include <objmgr/prefetch_manager.hpp>
#include <objmgr/objmgr_exception.hpp>
#include <corelib/ncbithr.hpp>
#include <corelib/ncbi_system.hpp>
#include <corelib/ncbi_safe_static.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


class CPrefetchManager_Impl;


class CPrefetchThread :
    public CThreadInPool<CPrefetchRequest>,
    public SPrefetchTypes
{
    typedef CThreadInPool<CPrefetchRequest> TParent;
public:
    CPrefetchThread(TPool* pool, ERunMode mode = eNormal) 
        : CThreadInPool<CPrefetchRequest>(pool, mode), m_CanceledAll(false)
        {
        }

    static CPrefetchThread* GetCurrentThread(void)
        {
            return dynamic_cast<CPrefetchThread*>(CThread::GetCurrentThread());
        }

    static CPrefetchManager::EState GetCurrentTokenState(void)
        {
            CPrefetchThread* thread = GetCurrentThread();
            if ( !thread ) {
                return CPrefetchManager::eInvalid;
            }
            return thread->m_CurrentToken.GetState();
        }
    static void SetCurrentToken(const CPrefetchToken& token)
        {
            CPrefetchThread* thread = GetCurrentThread();
            CMutexGuard guard(thread->m_Mutex);
            thread->m_CurrentToken = token;
        }
    static void ResetCurrentToken(void)
        {
            SetCurrentToken(CPrefetchToken());
        }

    virtual void ProcessRequest(TItemHandle handle);
    virtual void ProcessRequest(const CPrefetchRequest& req);

    void CancelAll(void);

private:
    mutable CMutex  m_Mutex;
    bool            m_CanceledAll;
    CPrefetchToken  m_CurrentToken;
};



CPrefetchRequest::CPrefetchRequest(CObjectFor<CMutex>* state_mutex,
                                   IPrefetchAction* action,
                                   IPrefetchListener* listener)
    : m_StateMutex(state_mutex),
      m_QueueItem(0),
      m_Action(action),
      m_Listener(listener),
      m_State(eQueued),
      m_Progress(0)
{
}


CPrefetchRequest::~CPrefetchRequest(void)
{
}


void CPrefetchRequest::SetListener(IPrefetchListener* listener)
{
    CMutexGuard guard(m_StateMutex->GetData());
    if ( m_Listener ) {
        NCBI_THROW(CObjMgrException, eOtherError,
                   "CPrefetchToken::SetListener: listener already set");
    }
    m_Listener = listener;
}


bool CPrefetchRequest::x_SetState(EState state)
{
    if ( state != m_State ) {
        if ( IsDone() ) {
            return false;
        }
        m_State = state;
        if ( m_Listener ) {
            m_Listener->PrefetchNotify(CPrefetchToken(this), state);
        }
        if ( IsDone() ) {
            m_QueueItem = 0;
        }
    }
    return true;
}


CPrefetchToken::EState
CPrefetchRequest::SetState(EState state)
{
    if ( IsDone() ) {
        if ( m_State == state ) {
            return state;
        }
        NCBI_THROW(CObjMgrException, eOtherError,
                   "CPrefetchToken::SetState: already done");
    }
    CMutexGuard guard(m_StateMutex->GetData());
    EState old_state = m_State;
    if ( !x_SetState(state) ) {
        NCBI_THROW(CObjMgrException, eOtherError,
                   "CPrefetchToken::SetState: already done");
    }
    return old_state;
}


bool CPrefetchRequest::TrySetState(EState state)
{
    if ( IsDone() ) {
        return m_State == state;
    }
    CMutexGuard guard(m_StateMutex->GetData());
    return x_SetState(state);
}


CPrefetchToken::TProgress
CPrefetchRequest::SetProgress(TProgress progress)
{
    CMutexGuard guard(m_StateMutex->GetData());
    if ( m_State != eStarted ) {
        NCBI_THROW(CObjMgrException, eOtherError,
                   "CPrefetchToken::SetProgress: not processing");
    }
    TProgress old_progress = m_Progress;
    if ( progress != old_progress ) {
        m_Progress = progress;
        if ( m_Listener ) {
            m_Listener->PrefetchNotify(CPrefetchToken(this), eAdvanced);
        }
    }
    return old_progress;
}


void CPrefetchRequest::Cancel(void)
{
    TrySetState(eCanceled);
}


CPrefetchManager_Impl::CPrefetchManager_Impl(void)
    : CPoolOfThreads<CPrefetchRequest>(3, kMax_Int),
      m_CanceledAll(false),
      m_StateMutex(new CObjectFor<CMutex>())
{
}


CPrefetchManager_Impl::~CPrefetchManager_Impl(void)
{
    CRef<CPrefetchManager_Impl> ref(this); // to avoid double destruction
    try {
        KillAllThreads();
    }
    catch(...) {
        // Just to be sure that we will not throw from the destructor.
    }
    ref.Release();
}


CPrefetchToken CPrefetchManager_Impl::AddAction(TPriority priority,
                                                IPrefetchAction* action,
                                                IPrefetchListener* listener)
{
    CPrefetchToken token(AcceptRequest(CPrefetchRequest(m_StateMutex,
                                                        action,
                                                        listener),
                                       priority));
    _ASSERT(token.m_QueueItem);
    _ASSERT(!token.x_GetImpl().m_QueueItem);
    token.x_GetImpl().m_QueueItem = token.m_QueueItem.GetNCPointer();
    _ASSERT(token.x_GetImpl().m_QueueItem);
    return token;
}


CPrefetchManager_Impl::TThread* CPrefetchManager_Impl::NewThread(ERunMode mode)
{
    return new CPrefetchThread(this, mode);
}


void CPrefetchManager_Impl::CancelQueue(void)
{
    for ( ;; ) {
        TItemHandle handle;
        try {
            handle = m_Queue.GetHandle(0);
        }
        catch ( CBlockingQueueException& ) {
            break;
        }
        CPrefetchToken(handle).Cancel();
    }
}


void CPrefetchManager_Impl::KillAllThreads(void)
{
    {{
        CMutexGuard guard(GetMutex());
        m_MaxThreads = 0;
        m_CanceledAll = true;
        ITERATE ( TThreads, it, m_Threads ) {
            dynamic_cast<CPrefetchThread&>(it->GetNCObject()).CancelAll();
        }
    }}
    CancelQueue();

    int n;
    {{
        CMutexGuard guard(GetMutex());
        n = m_Threads.size();
    }}
    vector<CPrefetchToken> kill_tokens;
    while ( n ) {
        try {
            kill_tokens.push_back(AddAction(0, 0, 0));
            --n;
        }
        catch (CBlockingQueueException&) { // guard against races
        }
    }
    CPrefetchThread* thread = CPrefetchThread::GetCurrentThread();
    ITERATE(TThreads, it, m_Threads) {
        if ( *it != thread ) {
            it->GetNCObject().Join();
        }
    }
    m_Threads.clear();
}


void CPrefetchManager_Impl::Register(TThread& thread)
{
    CMutexGuard guard(GetMutex());
    if (m_MaxThreads > 0) {
        m_Threads.insert(CRef<TThread>(&thread));
    }
}


void CPrefetchManager_Impl::UnRegister(TThread& thread)
{
    CMutexGuard guard(GetMutex());
    if (m_MaxThreads > 0) {
        m_Threads.erase(CRef<TThread>(&thread));
    }
}


class CSaveCurrentToken
{
public:
    CSaveCurrentToken(const CPrefetchToken& token)
        {
            CPrefetchThread::SetCurrentToken(token);
        }
    ~CSaveCurrentToken(void)
        {
            CPrefetchThread::ResetCurrentToken();
        }
private:
    CSaveCurrentToken(const CSaveCurrentToken&);
    void operator=(const CSaveCurrentToken&);
};

/*
CPrefetchToken CPrefetchRequest::GetCurrentToken(void)
{
    CPrefetchThread* thread = CPrefetchThread::GetCurrentThread();
    return thread? thread->GetCurrentToken(): CPrefetchToken();
}
*/

CPrefetchManager::EState CPrefetchManager::GetCurrentTokenState(void)
{
    return CPrefetchThread::GetCurrentTokenState();
}


bool CPrefetchManager::IsActive(void)
{
    switch ( GetCurrentTokenState() ) {
    case eCanceled:
        NCBI_THROW(CPrefetchCanceled, eCanceled, "canceled");
    case eInvalid:
        return false;
    default:
        return true;
    }
}


void CPrefetchThread::CancelAll(void)
{
    CMutexGuard guard(m_Mutex);
    m_CanceledAll = true;
    if ( m_CurrentToken ) {
        m_CurrentToken.Cancel();
    }
}


void CPrefetchThread::ProcessRequest(TItemHandle handle)
{
    CPrefetchToken token(handle);
    CPrefetchRequest& impl(token.x_GetImpl());
    try {
        IPrefetchAction* action = impl.GetAction();
        if ( !action ) {
            token.Cancel();
            Exit(0);
        }
        if ( m_CanceledAll ) {
            token.Cancel();
        }
        if ( impl.TrySetState(eStarted) ) {
            CSaveCurrentToken save(token);
            bool ok = action && action->Execute(token);
            impl.TrySetState(ok? eCompleted: eFailed);
        }
    }
    catch ( CPrefetchCanceled& /* ignored */ ) {
        token.Cancel();
    }
    catch ( CException& /* exc */ ) {
        impl.TrySetState(eFailed);
    }
}


void CPrefetchThread::ProcessRequest(const CPrefetchRequest& )
{
}


END_SCOPE(objects)
END_NCBI_SCOPE
