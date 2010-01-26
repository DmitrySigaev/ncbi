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
 *   Government have not placed any restriction on its use or reproduction.
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
 * Authors:  Maxim Didenko, Anatoliy Kuznetsov, Dmitry Kazimirov
 *
 * File Description:
 *    NetSchedule Worker Node implementation
 */

#include <ncbi_pch.hpp>

#include "grid_thread_context.hpp"
#include "netservice_params.hpp"
#include "balancing.hpp"

#include <connect/services/grid_globals.hpp>
#include <connect/services/error_codes.hpp>
#include <connect/services/grid_debug_context.hpp>
#include <connect/services/netschedule_api_expt.hpp>
#include <connect/services/grid_worker_app.hpp>
#include <connect/services/grid_control_thread.hpp>

#include <connect/ncbi_socket.hpp>

#include <util/thread_pool.hpp>

#include <corelib/ncbireg.hpp>
#include <corelib/ncbithr.hpp>
#include <corelib/ncbitime.hpp>
#include <corelib/ncbi_config.hpp>
#include <corelib/ncbi_process.hpp>
#include <corelib/ncbiexpt.hpp>
#include <corelib/ncbi_system.hpp>
#include <corelib/ncbi_safe_static.hpp>
#include <corelib/request_ctx.hpp>
#include <corelib/blob_storage.hpp>

#ifdef NCBI_OS_UNIX
#include <unistd.h>
#endif


#define NCBI_USE_ERRCODE_X   ConnServ_WorkerNode


BEGIN_NCBI_SCOPE

/////////////////////////////////////////////////////////////////////////////
//
IWorkerNodeJobWatcher::~IWorkerNodeJobWatcher()
{
}

/////////////////////////////////////////////////////////////////////////////
//
//     CWorkerNodeJobContext     --

namespace {

class CWorkerNodeCleanup : public IWorkerNodeCleanupEventSource
{
public:
    typedef set<IWorkerNodeCleanupEventListener*> TListeners;

    virtual void AddListener(IWorkerNodeCleanupEventListener* listener);
    virtual void RemoveListener(IWorkerNodeCleanupEventListener* listener);

    virtual void CallEventHandlers();

    void RemoveListeners(const TListeners& listeners);

protected:
    TListeners m_Listeners;
    CFastMutex m_ListenersLock;
};

void CWorkerNodeCleanup::AddListener(IWorkerNodeCleanupEventListener* listener)
{
    CFastMutexGuard g(m_ListenersLock);
    m_Listeners.insert(listener);
}

void CWorkerNodeCleanup::RemoveListener(
    IWorkerNodeCleanupEventListener* listener)
{
    CFastMutexGuard g(m_ListenersLock);
    m_Listeners.erase(listener);
}

void CWorkerNodeCleanup::CallEventHandlers()
{
    TListeners listeners;
    {
        CFastMutexGuard g(m_ListenersLock);
        listeners.swap(m_Listeners);
    }

    ITERATE(TListeners, it, listeners) {
        try {
            (*it)->HandleEvent(
                IWorkerNodeCleanupEventListener::eRegularCleanup);
            delete *it;
        }
        NCBI_CATCH_ALL_X(39, "Job clean-up error");
    }
}

void CWorkerNodeCleanup::RemoveListeners(
    const CWorkerNodeCleanup::TListeners& listeners)
{
    CFastMutexGuard g(m_ListenersLock);
    ITERATE(TListeners, it, listeners) {
        m_Listeners.erase(*it);
    }
}

class CWorkerNodeJobCleanup : public CWorkerNodeCleanup
{
public:
    CWorkerNodeJobCleanup(CWorkerNodeCleanup* worker_node_cleanup);

    virtual void AddListener(IWorkerNodeCleanupEventListener* listener);
    virtual void RemoveListener(IWorkerNodeCleanupEventListener* listener);

    virtual void CallEventHandlers();

private:
    CWorkerNodeCleanup* m_WorkerNodeCleanup;
};

CWorkerNodeJobCleanup::CWorkerNodeJobCleanup(
        CWorkerNodeCleanup* worker_node_cleanup) :
    m_WorkerNodeCleanup(worker_node_cleanup)
{
}

void CWorkerNodeJobCleanup::AddListener(
    IWorkerNodeCleanupEventListener* listener)
{
    CWorkerNodeCleanup::AddListener(listener);
    m_WorkerNodeCleanup->AddListener(listener);
}

void CWorkerNodeJobCleanup::RemoveListener(
    IWorkerNodeCleanupEventListener* listener)
{
    CWorkerNodeCleanup::RemoveListener(listener);
    m_WorkerNodeCleanup->RemoveListener(listener);
}

void CWorkerNodeJobCleanup::CallEventHandlers()
{
    {
        CFastMutexGuard g(m_ListenersLock);
        m_WorkerNodeCleanup->RemoveListeners(m_Listeners);
    }
    CWorkerNodeCleanup::CallEventHandlers();
}

} // end of anonymous namespace

CWorkerNodeJobContext::CWorkerNodeJobContext(CGridWorkerNode& worker_node,
                                             const CNetScheduleJob&    job,
                                             bool             log_requested)
    : m_WorkerNode(worker_node),
      m_LogRequested(log_requested),
      m_ThreadContext(NULL),
      m_CleanupEventSource(new CWorkerNodeJobCleanup(
        static_cast<CWorkerNodeCleanup*>(worker_node.GetCleanupEventSource())))
{
    Reset(job);
}

const string& CWorkerNodeJobContext::GetQueueName() const
{
    return m_WorkerNode.GetQueueName();
}
const string& CWorkerNodeJobContext::GetClientName() const
{
    return m_WorkerNode.GetClientName();
}

CNcbiIstream& CWorkerNodeJobContext::GetIStream(IBlobStorage::ELockMode mode)
{
    _ASSERT(m_ThreadContext);
    return m_ThreadContext->GetIStream(mode);
}
CNcbiOstream& CWorkerNodeJobContext::GetOStream()
{
    _ASSERT(m_ThreadContext);
    return m_ThreadContext->GetOStream();
}

void CWorkerNodeJobContext::CommitJob()
{
    CheckIfCanceled();
    m_JobCommitted = eDone;
}

void CWorkerNodeJobContext::CommitJobWithFailure(const string& err_msg)
{
    CheckIfCanceled();
    m_JobCommitted = eFailure;
    m_Job.error_msg = err_msg;
}

void CWorkerNodeJobContext::ReturnJob()
{
    CheckIfCanceled();
    m_JobCommitted = eReturn;
}

void CWorkerNodeJobContext::PutProgressMessage(const string& msg,
                                               bool send_immediately)
{
    _ASSERT(m_ThreadContext);
    CheckIfCanceled();
    m_ThreadContext->PutProgressMessage(msg, send_immediately);
}

void CWorkerNodeJobContext::SetJobRunTimeout(unsigned time_to_run)
{
    _ASSERT(m_ThreadContext);
    m_ThreadContext->SetJobRunTimeout(time_to_run);
}

void CWorkerNodeJobContext::JobDelayExpiration(unsigned runtime_inc)
{
    _ASSERT(m_ThreadContext);
    m_ThreadContext->JobDelayExpiration(runtime_inc);
}

CNetScheduleAdmin::EShutdownLevel
CWorkerNodeJobContext::GetShutdownLevel(void) const
{
    _ASSERT(m_ThreadContext);
    if (m_ThreadContext->IsJobCanceled())
        return CNetScheduleAdmin::eShutdownImmediate;
    return CGridGlobals::GetInstance().GetShutdownLevel();
}

void CWorkerNodeJobContext::CheckIfCanceled()
{
    if (IsCanceled()) {
        NCBI_THROW_FMT(CGridWorkerNodeException, eJobIsCanceled,
            "Job " << m_Job.job_id << " has been canceled");
    }
}

void CWorkerNodeJobContext::Reset(const CNetScheduleJob& job)
{
    m_Job = job;
    m_JobNumber = CGridGlobals::GetInstance().GetNewJobNumber();

    m_JobCommitted = eNotCommitted;
    m_InputBlobSize = 0;
    m_ExclusiveJob = m_Job.mask & CNetScheduleAPI::eExclusiveJob;
}

void CWorkerNodeJobContext::RequestExclusiveMode()
{
    if (!m_ExclusiveJob) {
        if (!m_WorkerNode.EnterExclusiveMode()) {
            NCBI_THROW(CGridWorkerNodeException,
                eExclusiveModeIsAlreadySet, "");
        }
        m_ExclusiveJob = true;
    }
}

size_t CWorkerNodeJobContext::GetMaxServerOutputSize() const
{
    return m_WorkerNode.GetServerOutputSize();
}

IWorkerNodeCleanupEventSource* CWorkerNodeJobContext::GetCleanupEventSource()
{
    return m_CleanupEventSource;
}


/////////////////////////////////////////////////////////////////////////////
//
///@internal
static void s_TlsCleanup(CGridThreadContext* p_value, void* /* data */ )
{
    delete p_value;
}
/// @internal
static CStaticTls<CGridThreadContext> s_tls;

///@internal
class CWorkerNodeRequest : public CStdRequest
{
public:
    CWorkerNodeRequest(auto_ptr<CWorkerNodeJobContext> context);

    virtual void Process();

private:
    void x_HandleProcessError(exception* ex = NULL);

    auto_ptr<CWorkerNodeJobContext> m_JobContext;
};


CWorkerNodeRequest::CWorkerNodeRequest(auto_ptr<CWorkerNodeJobContext> context)
  : m_JobContext(context)
{
}


void CGridThreadContext::x_HandleRunJobError(exception* ex /*= NULL*/)
{
    string msg = " Error in Job execution";
    if (ex) {
        msg += ": ";
        msg += ex->what();
    }
    ERR_POST_X(18, m_JobContext->GetJobKey() << msg);
    try {
        m_JobContext->m_Job.error_msg = ex ? ex->what() : "Unknown error";
        PutFailure();
    } catch (exception& ex1) {
        ERR_POST_X(19, "Failed to report exception: " <<
            m_JobContext->GetJobKey() << " " << ex1.what());
    } catch (...) {
        ERR_POST_X(20, "Failed to report exception while processing " <<
            m_JobContext->GetJobKey());
    }
}

static bool s_ReqEventsDisabled = false;

class CRequestStateGuard
{
public:
    CRequestStateGuard(CWorkerNodeJobContext& job_context);

    void RequestStart();
    void RequestStop();

    ~CRequestStateGuard();

private:
    CWorkerNodeJobContext& m_JobContext;
};

CRequestStateGuard::CRequestStateGuard(CWorkerNodeJobContext& job_context) :
    m_JobContext(job_context)
{
    CRequestContext& request_context = CDiagContext::GetRequestContext();

    request_context.SetRequestID((int) job_context.GetJobNumber());

    const CNetScheduleJob& job = job_context.GetJob();

    if (!job.client_ip.empty())
        request_context.SetClientIP(job.client_ip);

    if (!job.session_id.empty())
        request_context.SetSessionID(job.session_id);

    request_context.SetAppState(eDiagAppState_RequestBegin);

    if (!s_ReqEventsDisabled)
        GetDiagContext().PrintRequestStart().Print("jid", job.job_id);
}

inline void CRequestStateGuard::RequestStart()
{
    CDiagContext::GetRequestContext().SetAppState(eDiagAppState_Request);
}

inline void CRequestStateGuard::RequestStop()
{
    CDiagContext::GetRequestContext().SetAppState(eDiagAppState_RequestEnd);
}

CRequestStateGuard::~CRequestStateGuard()
{
    CRequestContext& request_context = CDiagContext::GetRequestContext();

    if (!request_context.IsSetRequestStatus())
        request_context.SetRequestStatus(
            m_JobContext.GetCommitStatus() == CWorkerNodeJobContext::eDone &&
                m_JobContext.GetJob().ret_code == 0 ? 200 : 500);

    switch (request_context.GetAppState()) {
    case eDiagAppState_Request:
        request_context.SetAppState(eDiagAppState_RequestEnd);
        /* FALL THROUGH */

    default:
        if (!s_ReqEventsDisabled)
            GetDiagContext().PrintRequestStop();
        request_context.SetAppState(eDiagAppState_NotSet);
        request_context.UnsetSessionID();
        request_context.UnsetClientIP();
    }
}

void CGridThreadContext::RunJobs(CWorkerNodeJobContext& job_context)
{
    _ASSERT(!m_JobContext);
    m_JobContext = &job_context;
    job_context.SetThreadContext(this);

    bool more_jobs;

    do {
        more_jobs = false;

        CGridDebugContext* debug_context = CGridDebugContext::GetInstance();
        if (debug_context) {
            debug_context->DumpInput(m_JobContext->GetJobInput(),
                m_JobContext->GetJobNumber());
        }
        m_JobContext->GetWorkerNode().x_NotifyJobWatcher(*m_JobContext,
            IWorkerNodeJobWatcher::eJobStarted);

        CRequestStateGuard request_state_guard(job_context);

        request_state_guard.RequestStart();

        CNetScheduleJob new_job;
        try {
            CRef<IWorkerNodeJob> job(GetJob());
            try {
                job_context.SetJobRetCode(job->Do(job_context));
            } catch (CGridWorkerNodeException& ex) {
                if (ex.GetErrCode() !=
                    CGridWorkerNodeException::eExclusiveModeIsAlreadySet)
                    throw;

                if (job_context.IsLogRequested()) {
                    LOG_POST_X(21, "Job " << job_context.GetJobKey() <<
                        " has been returned back to the queue "
                        "because it requested exclusive mode but "
                        "another job already has the exclusive status.");
                }
            }
            CloseStreams();
            unsigned try_count = 0;
            for (;;) {
                try {
                    switch (job_context.GetCommitStatus()) {
                        case CWorkerNodeJobContext::eDone:
                            more_jobs = PutResult(new_job);
                            break;

                        case CWorkerNodeJobContext::eFailure:
                            PutFailure();
                            break;

                        case CWorkerNodeJobContext::eNotCommitted:
                            if (!TWorkerNode_AllowImplicitJobReturn::
                                        GetDefault() &&
                                    job_context.GetShutdownLevel() ==
                                        CNetScheduleAdmin::eNoShutdown) {
                                job_context.m_Job.error_msg =
                                    "Job was not explicitly committed";
                                PutFailure();
                                break;
                            }
                            /* FALL THROUGH */

                        case CWorkerNodeJobContext::eReturn:
                            ReturnJob();
                            break;

                        case CWorkerNodeJobContext::eCanceled:
                            ERR_POST("Job " << job_context.GetJobKey() <<
                                " has been canceled");
                    }
                    break;
                } catch (CNetServiceException& ex) {
                    if (++try_count >= TServConn_ConnMaxRetries::GetDefault())
                        throw;
                    ERR_POST_X(22, "Communication Error : " << ex.what());
                    SleepMilliSec(s_GetRetryDelay());
                }
            }

            if (!CGridGlobals::GetInstance().IsShuttingDown())
                static_cast<CWorkerNodeJobCleanup*>(
                    job_context.GetCleanupEventSource())->CallEventHandlers();

            request_state_guard.RequestStop();
        }
        catch (exception& ex) {
            x_HandleRunJobError(&ex);
        }
        catch (...) {
            x_HandleRunJobError();
        }

        CloseStreams();

        _ASSERT(m_JobContext);
        m_JobContext->GetWorkerNode().x_NotifyJobWatcher(*m_JobContext,
            IWorkerNodeJobWatcher::eJobStopped);

        if (more_jobs)
            job_context.Reset(new_job);
    } while (more_jobs);

    m_JobContext->SetThreadContext(NULL);
    m_JobContext = NULL;
}

void CWorkerNodeRequest::x_HandleProcessError(exception* ex)
{
    string msg = " Error during job run";
    if (ex) {
        msg += ": ";
        msg += ex->what();
    }
    ERR_POST_X(23, msg);
    try {
        m_JobContext->GetWorkerNode().x_ReturnJob(m_JobContext->GetJobKey());
    } catch (exception& ex1) {
        ERR_POST_X(24, "Could not return job back to queue: " << ex1.what());
    } catch (...) {
        ERR_POST_X(25, "Could not return job back to queue.");
    }
}

void CWorkerNodeRequest::Process()
{
    try {
        CGridThreadContext* thread_context = s_tls.GetValue();
        if (!thread_context) {
            thread_context =
                new CGridThreadContext(m_JobContext->GetWorkerNode());
            s_tls.SetValue(thread_context, s_TlsCleanup);
        }
        thread_context->RunJobs(*m_JobContext);
    }
    catch (exception& ex) { x_HandleProcessError(&ex);  }
    catch (...) { x_HandleProcessError(); }
}

/////////////////////////////////////////////////////////////////////////////
//
//     CWorkerNodeJobWatchers
/// @internal

class CWorkerNodeJobWatchers : public IWorkerNodeJobWatcher
{
public:
    virtual ~CWorkerNodeJobWatchers() {};
    virtual void Notify(const CWorkerNodeJobContext& job, EEvent event)
    {
        NON_CONST_ITERATE(TCont, it, m_Watchers) {
            IWorkerNodeJobWatcher* watcher =
                const_cast<IWorkerNodeJobWatcher*>(it->first);
            watcher->Notify(job, event);
        }
    }

    void AttachJobWatcher(IWorkerNodeJobWatcher& job_watcher, EOwnership owner)
    {
        TCont::const_iterator it = m_Watchers.find(&job_watcher);
        if (it == m_Watchers.end()) {
            if (owner == eTakeOwnership)
                m_Watchers[&job_watcher] = AutoPtr<IWorkerNodeJobWatcher>(&job_watcher);
            else
                m_Watchers[&job_watcher] = AutoPtr<IWorkerNodeJobWatcher>();
        }
    }

private:
    typedef map<IWorkerNodeJobWatcher*, AutoPtr<IWorkerNodeJobWatcher> > TCont;
    TCont m_Watchers;
};

/////////////////////////////////////////////////////////////////////////////
//
//     CGridControleThread
/// @internal
class CGridControlThread : public CThread
{
public:
    CGridControlThread(CGridWorkerNode* worker_node,
        unsigned int start_port, unsigned int end_port) : m_Control(
            new CWorkerNodeControlServer(worker_node, start_port, end_port)) {}

    void Prepare() { m_Control->StartListening(); }

    unsigned short GetControlPort() { return m_Control->GetControlPort(); }
    void Stop() { if (m_Control.get()) m_Control->RequestShutdown(); }

protected:
    virtual void* Main(void)
    {
        m_Control->Run();
        return NULL;
    }
    virtual void OnExit(void)
    {
        CThread::OnExit();
        CGridGlobals::GetInstance().RequestShutdown(CNetScheduleAdmin::eShutdownImmediate);
        LOG_POST_X(46, "Control Thread has been stopped.");
    }

private:
    auto_ptr<CWorkerNodeControlServer> m_Control;
};

class CGridCleanupThread : public CThread
{
public:
    CGridCleanupThread(CGridWorkerNode* worker_node,
        IGridWorkerNodeApp_Listener* listener) :
            m_WorkerNode(worker_node),
            m_Listener(listener),
            m_Semaphore(0, 1)
    {
    }

    bool Wait(unsigned seconds) {return m_Semaphore.TryWait(seconds);}

protected:
    virtual void* Main();

private:
    CGridWorkerNode* m_WorkerNode;
    IGridWorkerNodeApp_Listener* m_Listener;
    CSemaphore m_Semaphore;
};

void* CGridCleanupThread::Main()
{
    m_WorkerNode->GetCleanupEventSource()->CallEventHandlers();
    m_Listener->OnGridWorkerStop();
    m_Semaphore.Post();

    return NULL;
}

/////////////////////////////////////////////////////////////////////////////
//
//     CWorkerNodeIdleThread      --
class CWorkerNodeIdleThread : public CThread
{
public:
    CWorkerNodeIdleThread(IWorkerNodeIdleTask*,
                          CGridWorkerNode& worker_node,
                          unsigned run_delay,
                          unsigned int auto_shutdown);

    void RequestShutdown()
    {
        m_ShutdownFlag = true;
        m_Wait1.Post();
        m_Wait2.Post();
    }
    void Schedule()
    {
        CFastMutexGuard guard(m_Mutext);
        m_AutoShutdownSW.Restart();
        if (m_StopFlag) {
            m_StopFlag = false;
            m_Wait1.Post();
        }
    }
    void Suspend()
    {
        CFastMutexGuard guard(m_Mutext);
        m_AutoShutdownSW.Restart();
        m_AutoShutdownSW.Stop();
        if (!m_StopFlag) {
            m_StopFlag = true;
            m_Wait2.Post();
        }
    }

    bool IsShutdownRequested() const { return m_ShutdownFlag; }


protected:
    virtual void* Main(void);
    virtual void OnExit(void);

    CWorkerNodeIdleTaskContext& GetContext();

private:

    unsigned int x_GetInterval() const
    {
        CFastMutexGuard guard(m_Mutext);
        return  m_AutoShutdown > 0 ?
                min( m_AutoShutdown - (unsigned int)m_AutoShutdownSW.Elapsed(), m_RunInterval )
                : m_RunInterval;
    }
    bool x_GetStopFlag() const
    {
        CFastMutexGuard guard(m_Mutext);
        return m_StopFlag;
    }
    bool x_IsAutoShutdownTime() const
    {
        CFastMutexGuard guard(m_Mutext);
        return m_AutoShutdown > 0 ? m_AutoShutdownSW.Elapsed() > m_AutoShutdown : false;
    }

    IWorkerNodeIdleTask* m_Task;
    CGridWorkerNode& m_WorkerNode;
    auto_ptr<CWorkerNodeIdleTaskContext> m_TaskContext;
    mutable CSemaphore  m_Wait1;
    mutable CSemaphore  m_Wait2;
    volatile bool       m_StopFlag;
    volatile bool       m_ShutdownFlag;
    unsigned int        m_RunInterval;
    unsigned int        m_AutoShutdown;
    CStopWatch          m_AutoShutdownSW;
    mutable CFastMutex  m_Mutext;

    CWorkerNodeIdleThread(const CWorkerNodeIdleThread&);
    CWorkerNodeIdleThread& operator=(const CWorkerNodeIdleThread&);
};

CWorkerNodeIdleThread::CWorkerNodeIdleThread(IWorkerNodeIdleTask* task,
                                             CGridWorkerNode& worker_node,
                                             unsigned run_delay,
                                             unsigned int auto_shutdown)
    : m_Task(task), m_WorkerNode(worker_node),
      m_Wait1(0,100000), m_Wait2(0,1000000),
      m_StopFlag(false), m_ShutdownFlag(false),
      m_RunInterval(run_delay),
      m_AutoShutdown(auto_shutdown), m_AutoShutdownSW(CStopWatch::eStart)
{
}
void* CWorkerNodeIdleThread::Main()
{
    while (!m_ShutdownFlag) {
        if ( x_IsAutoShutdownTime() ) {
            LOG_POST_X(47, "There are no more jobs to be done. Exiting.");
            CGridGlobals::GetInstance().RequestShutdown(CNetScheduleAdmin::eShutdownImmediate);
            break;
        }
        unsigned int interval = m_AutoShutdown > 0 ? min (m_RunInterval,m_AutoShutdown) : m_RunInterval;
        if (m_Wait1.TryWait(interval, 0)) {
            if (m_ShutdownFlag)
                continue;
            interval = x_GetInterval();
            if (m_Wait2.TryWait(interval, 0)) {
                continue;
            }
        }
        if (m_Task && !x_GetStopFlag()) {
            try {
                do {
                    if ( x_IsAutoShutdownTime() ) {
                        LOG_POST_X(48,
                            "There are no more jobs to be done. Exiting.");
                        CGridGlobals::GetInstance().RequestShutdown(
                            CNetScheduleAdmin::eShutdownImmediate);
                        m_ShutdownFlag = true;
                        break;
                    }
                    GetContext().Reset();
                    m_Task->Run(GetContext());
                } while( GetContext().NeedRunAgain() && !m_ShutdownFlag);
            } NCBI_CATCH_ALL_X(58,
                "CWorkerNodeIdleThread::Main: Idle Task failed");
        }
    }
    return 0;
}

void CWorkerNodeIdleThread::OnExit(void)
{
    LOG_POST_X(49, "Idle Thread has been stopped.");
}

CWorkerNodeIdleTaskContext& CWorkerNodeIdleThread::GetContext()
{
    if (!m_TaskContext.get())
        m_TaskContext.reset(new CWorkerNodeIdleTaskContext(*this));
    return *m_TaskContext;
}

/////////////////////////////////////////////////////////////////////////////
//
//     CWorkerNodeIdleTaskContext      --
CWorkerNodeIdleTaskContext::
CWorkerNodeIdleTaskContext(CWorkerNodeIdleThread& thread)
    : m_Thread(thread), m_RunAgain(false)
{
}
bool CWorkerNodeIdleTaskContext::IsShutdownRequested() const
{
    return m_Thread.IsShutdownRequested();
}
void CWorkerNodeIdleTaskContext::Reset()
{
    m_RunAgain = false;
}

void CWorkerNodeIdleTaskContext::RequestShutdown()
{
    m_Thread.RequestShutdown();
    CGridGlobals::GetInstance().
        RequestShutdown(CNetScheduleAdmin::eShutdownImmediate);
}

/////////////////////////////////////////////////////////////////////////////
//
//    CIdleWatcher
/// @internal
class CIdleWatcher : public IWorkerNodeJobWatcher
{
public:
    CIdleWatcher(CWorkerNodeIdleThread& idle)
        : m_Idle(idle), m_RunningJobs(0) {}
    virtual ~CIdleWatcher() {};
    virtual void Notify(const CWorkerNodeJobContext& job, EEvent event)
    {
        if (event == eJobStarted) {
            ++m_RunningJobs;
            m_Idle.Suspend();
        } else if (event == eJobStopped) {
            --m_RunningJobs;
            if (m_RunningJobs == 0)
                m_Idle.Schedule();
        }
    }

private:
    CWorkerNodeIdleThread& m_Idle;
    volatile int m_RunningJobs;
};


/////////////////////////////////////////////////////////////////////////////
//

CGridWorkerNode::CGridWorkerNode(
                               CNcbiApplication&          app,
                               IWorkerNodeJobFactory*     job_factory,
                               IBlobStorageFactory*       storage_factory,
                               INetScheduleClientFactory* client_factory) :
    m_JobFactory(job_factory),
    m_StorageFactory(storage_factory),
    m_ClientFactory(client_factory),
    m_MaxThreads(1),
    m_NSTimeout(30),
    m_CheckStatusPeriod(2),
    m_ExclusiveJobSemaphore(1, 1),
    m_IsProcessingExclusiveJob(false),
    m_UseEmbeddedStorage(false),
    m_CleanupEventSource(new CWorkerNodeCleanup()),
    m_Listener(new CGridWorkerNodeApp_Listener()),
    m_App(app),
    m_SingleThreadForced(false)
{
    if (!m_JobFactory.get())
        NCBI_THROW(CGridWorkerNodeException,
                 eJobFactoryIsNotSet, "The JobFactory is not set.");
}

CGridWorkerNode::~CGridWorkerNode()
{
}

void CGridWorkerNode::Init(bool default_merge_lines_value)
{
    IRWRegistry& reg = m_App.GetConfig();

    if (reg.GetBool("log", "merge_lines", default_merge_lines_value)) {
        SetDiagPostFlag(eDPF_PreMergeLines);
        SetDiagPostFlag(eDPF_MergeLines);
    }

    reg.Set(kNetScheduleAPIDriverName, "discover_low_priority_servers", "true");

    if (!m_StorageFactory.get())
        m_StorageFactory.reset(new CBlobStorageFactory(reg));
    if (!m_ClientFactory.get())
        m_ClientFactory.reset(new CNetScheduleClientFactory(reg));
}

const char* kServerSec = "server";

int CGridWorkerNode::Run()
{
    LOG_POST_X(50, GetJobFactory().GetJobVersion() << WN_BUILD_DATE);

    const IRegistry& reg = m_App.GetConfig();
    CConfig conf(reg);

    unsigned init_threads = 1;

    if (!m_SingleThreadForced) {
        string max_threads = reg.GetString(kServerSec, "max_threads", "auto");
        if (NStr::CompareNocase(max_threads, "auto") == 0)
            m_MaxThreads = GetCpuCount();
        else {
            try {
                m_MaxThreads = NStr::StringToUInt(max_threads);
            } catch (...) {
                m_MaxThreads = GetCpuCount();
                ERR_POST_X(51, "Could not convert [" << kServerSec <<
                    "] max_threads parameter to number.\n"
                    "Using \'auto\' option (" << m_MaxThreads << ").");
            }
        }
        init_threads =
            reg.GetInt(kServerSec, "init_threads", 1, 0, IRegistry::eReturn);
    }
    if (init_threads > m_MaxThreads)
        init_threads = m_MaxThreads;

    m_NSTimeout = reg.GetInt(kServerSec,
        "job_wait_timeout", 30, 0, IRegistry::eReturn);

    unsigned thread_pool_timeout = reg.GetInt(kServerSec,
        "thread_pool_timeout", 30, 0, IRegistry::eReturn);

    const CArgs& args = m_App.GetArgs();

    unsigned int start_port, end_port;

    string sport1, sport2;
    NStr::SplitInTwo(args["control_port"] ? args["control_port"].AsString() :
        reg.GetString(kServerSec, "control_port", "9300"), "-", sport1, sport2);
    start_port = NStr::StringToUInt(sport1);
    end_port = sport2.empty() ? start_port : NStr::StringToUInt(sport2);

    bool log_requested = reg.GetBool(kServerSec,
        "log", false, 0, IRegistry::eReturn);

    unsigned int idle_run_delay =
        reg.GetInt(kServerSec,"idle_run_delay",30,0,IRegistry::eReturn);

    unsigned int auto_shutdown =
        reg.GetInt(kServerSec,"auto_shutdown_if_idle",0,0,IRegistry::eReturn);

    unsigned int infinite_loop_time =
        reg.HasEntry(kServerSec, "infinite_loop_time") ?
        reg.GetInt(kServerSec, "infinite_loop_time", 0, 0, IRegistry::eReturn) :
        reg.GetInt(kServerSec, "infinit_loop_time", 0, 0, IRegistry::eReturn);

    bool reuse_job_object =
        reg.GetBool(kServerSec, "reuse_job_object", false, 0,
                    CNcbiRegistry::eReturn);

    unsigned int max_total_jobs =
        reg.GetInt(kServerSec,"max_total_jobs",0,0,IRegistry::eReturn);

    unsigned int max_failed_jobs =
        reg.GetInt(kServerSec,"max_failed_jobs",0,0,IRegistry::eReturn);

    bool is_daemon =
        reg.GetBool(kServerSec, "daemon", false, 0, CNcbiRegistry::eReturn);

    vector<string> vhosts;

    NStr::Tokenize(reg.GetString(kServerSec,
        "master_nodes", kEmptyStr), " ;,", vhosts);

    ITERATE(vector<string>, it, vhosts) {
        string host, port;
        NStr::SplitInTwo(NStr::TruncateSpaces(*it), ":", host, port);
        if (host.empty() || port.empty())
            continue;
        try {
            m_Masters.insert(SServerAddress(NStr::ToLower(host),
                (unsigned short) NStr::StringToUInt(port)));
        } catch(...) {}
    }

    vhosts.clear();

    NStr::Tokenize(reg.GetString(kServerSec,
        "admin_hosts", kEmptyStr), " ;,", vhosts);

    ITERATE(vector<string>, it, vhosts) {
        unsigned int ha = CSocketAPI::gethostbyname(*it);
        if (ha != 0)
            m_AdminHosts.insert(ha);
    }

    m_CheckStatusPeriod =
        reg.GetInt(kServerSec, "check_status_period", 2, 0, IRegistry::eReturn);

    if (reg.HasEntry(kServerSec,"wait_server_timeout")) {
        ERR_POST_X(52, "[" << kServerSec <<
            "] \"wait_server_timeout\" is not used anymore.\n"
            "Use [" << kNetScheduleAPIDriverName <<
            "] \"communication_timeout\" parameter instead.");
    }

    if (reg.HasEntry(kNetScheduleAPIDriverName, "use_embedded_storage"))
        m_UseEmbeddedStorage = reg.
            GetBool(kNetScheduleAPIDriverName, "use_embedded_storage", false, 0,
                    CNcbiRegistry::eReturn);
    else
        m_UseEmbeddedStorage = reg.
            GetBool(kNetScheduleAPIDriverName, "use_embedded_input", false, 0,
                    CNcbiRegistry::eReturn);

    CGridDebugContext::eMode debug_mode = CGridDebugContext::eGDC_NoDebug;
    string dbg_mode = reg.GetString("gw_debug", "mode", kEmptyStr);
    if (NStr::CompareNocase(dbg_mode, "gather")==0) {
        debug_mode = CGridDebugContext::eGDC_Gather;
    } else if (NStr::CompareNocase(dbg_mode, "execute")==0) {
        debug_mode = CGridDebugContext::eGDC_Execute;
    }
    if (debug_mode != CGridDebugContext::eGDC_NoDebug) {
        CGridDebugContext& debug_context =
            CGridDebugContext::Create(debug_mode, *m_StorageFactory);
        string run_name =
            reg.GetString("gw_debug", "run_name", m_App.GetProgramDisplayName());
        debug_context.SetRunName(run_name);
        if (debug_mode == CGridDebugContext::eGDC_Gather) {
            max_total_jobs =
                reg.GetInt("gw_debug","gather_nrequests",1,0,IRegistry::eReturn);
        } else if (debug_mode == CGridDebugContext::eGDC_Execute) {
            string files =
                reg.GetString("gw_debug", "execute_requests", kEmptyStr);
            max_total_jobs = 0;
            debug_context.SetExecuteList(files);
        }
        is_daemon = false;
    }

#if defined(NCBI_OS_UNIX)
    if (is_daemon) {
        LOG_POST_X(53, "Entering UNIX daemon mode...");
        bool daemon = CProcess::Daemonize("/dev/null",
                                          CProcess::fDontChroot |
                                          CProcess::fKeepStdin  |
                                          CProcess::fKeepStdout);
        if (!daemon) {
            return 0;
        }
    }
#endif

    AttachJobWatcher(CGridGlobals::GetInstance().GetJobsWatcher());

    CRef<CGridControlThread> control_thread(
        new CGridControlThread(this, start_port, end_port));

    control_thread->Prepare();

    m_RebalanceStrategy = CreateSimpleRebalanceStrategy(conf, "server");

    m_SharedNSClient = m_ClientFactory->CreateInstance();
    m_NSExecuter =
        m_SharedNSClient.GetExecuter(control_thread->GetControlPort());

    m_SharedNSClient.SetProgramVersion(m_JobFactory->GetJobVersion());

    CGridGlobals::GetInstance().SetReuseJobObject(reuse_job_object);
    CGridGlobals::GetInstance().GetJobsWatcher().SetMaxJobsAllowed(max_total_jobs);
    CGridGlobals::GetInstance().GetJobsWatcher().SetMaxFailuresAllowed(max_failed_jobs);
    CGridGlobals::GetInstance().GetJobsWatcher().SetInfinitLoopTime(infinite_loop_time);
    CGridGlobals::GetInstance().SetWorker(*this);

    IWorkerNodeIdleTask* task = NULL;
    if (idle_run_delay > 0)
        task = GetJobFactory().GetIdleTask();
    if (task || auto_shutdown > 0 ) {
        m_IdleThread.Reset(new CWorkerNodeIdleThread(task, *this,
                                                     task ? idle_run_delay : auto_shutdown,
                                                     auto_shutdown));
        m_IdleThread->Run();
        AttachJobWatcher(*(new CIdleWatcher(*m_IdleThread)), eTakeOwnership);
    }

    control_thread->Run();

    LOG_POST_X(54, "\n=================== NEW RUN : " <<
        CGridGlobals::GetInstance().GetStartTime().AsString() <<
            " ===================\n" <<
        GetJobFactory().GetJobVersion() << WN_BUILD_DATE << " is started.\n"
        "Waiting for control commands on " << CSocketAPI::gethostname() <<
            ":" << control_thread->GetControlPort() << "\n"
        "Queue name: " << GetQueueName() << "\n"
        "Maximum job threads: " << m_MaxThreads << "\n");

    try {
        GetNSExecuter().RegisterClient();
    } catch (CNetServiceException& ex) {
        // If the server does not understand this new command, ignore the error.
        if (ex.GetErrCode() != CNetServiceException::eCommunicationError ||
                NStr::Find(ex.what(), "Server error:Unknown request") == NPOS)
            throw;
    }

    m_Listener->OnGridWorkerStart();

    auto_ptr<CGridThreadContext> single_thread_context;
    auto_ptr<CStdPoolOfThreads> thread_pool;

    _ASSERT(m_MaxThreads > 0);

    if (m_MaxThreads <= 1)
        single_thread_context.reset(new CGridThreadContext(*this));
    else {
        thread_pool.reset(new CStdPoolOfThreads(m_MaxThreads, 0));
        try {
            thread_pool->Spawn(init_threads);
        }
        catch (exception& ex) {
            ERR_POST_X(26, ex.what());
            CGridGlobals::GetInstance().RequestShutdown(
                CNetScheduleAdmin::eShutdownImmediate);
            return 2;
        }
        catch (...) {
            ERR_POST_X(27, "Unknown error");
            CGridGlobals::GetInstance().RequestShutdown(
                CNetScheduleAdmin::eShutdownImmediate);
            return 3;
        }
    }

    CNetScheduleJob job;

    unsigned try_count = 0;
    while (!CGridGlobals::GetInstance().IsShuttingDown()) {
        try {

            if (m_MaxThreads > 1) {
                try {
                    thread_pool->WaitForRoom(thread_pool_timeout);
                } catch (CBlockingQueueException&) {
                    // threaded pool is busy
                    continue;
                }
            }

            if (x_GetNextJob(job)) {
                auto_ptr<CWorkerNodeJobContext>
                    job_context(new CWorkerNodeJobContext(*this,
                                                          job,
                                                          log_requested));
                job_context->Reset(job);
                if (m_MaxThreads > 1 ) {
                    CRef<CStdRequest> job_req(
                                    new CWorkerNodeRequest(job_context));
                    try {
                        thread_pool->AcceptRequest(job_req);
                    } catch (CBlockingQueueException& ex) {
                        ERR_POST_X(28, ex.what());
                        // that must not happen after CBlockingQueue is fixed
                        _ASSERT(0);
                        x_ReturnJob(job.job_id);
                    }
                } else {
                    try {
                        single_thread_context->RunJobs(*job_context);
                    } catch (...) {
                        x_ReturnJob(job_context->GetJobKey());
                        throw;
                    }
                }
            }
        } catch (CNetServiceException& ex) {
            ERR_POST_X(40, ex.what());
            if (++try_count >= TServConn_ConnMaxRetries::GetDefault()) {
                CGridGlobals::GetInstance().RequestShutdown(
                    CNetScheduleAdmin::eShutdownImmediate);
            } else {
                SleepMilliSec(s_GetRetryDelay());
                continue;
            }
        } catch (exception& ex) {
            if (TWorkerNode_StopOnJobErrors::GetDefault()) {
                ERR_POST_X(29, ex.what());
                CGridGlobals::GetInstance().RequestShutdown(
                    CNetScheduleAdmin::eShutdownImmediate);
            }
        } catch (...) {
            if (TWorkerNode_StopOnJobErrors::GetDefault()) {
                ERR_POST_X(30, "Unknown error");
                CGridGlobals::GetInstance().RequestShutdown(
                    CNetScheduleAdmin::eShutdownImmediate);
            }
        }
        try_count = 0;
    }
    LOG_POST_X(31, "Shutting down...");
    if (reg.GetBool(kServerSec,
            "force_exit", false, 0, CNcbiRegistry::eReturn)) {
        ERR_POST_X(45, "Force exit");
    } else if (m_MaxThreads > 1) {
        try {
            LOG_POST_X(32, "Stopping worker threads...");
            thread_pool->KillAllThreads(true);
            thread_pool.reset(0);
        } catch (exception& ex) {
            ERR_POST_X(33, "Could not stop worker threads: " << ex.what());
        } catch (...) {
            ERR_POST_X(34, "Could not stop worker threads: Unknown error");
        }
    }
    try {
        GetNSExecuter().UnRegisterClient();
    } catch (CNetServiceException& ex) {
        // if server does not understand this new command just ignore the error
        if (ex.GetErrCode() != CNetServiceException::eCommunicationError
            || NStr::Find(ex.what(),"Server error:Unknown request") == NPOS) {
            ERR_POST_X(35, "Could unregister from NetScehdule services: "
                       << ex.what());
        }
    } catch(exception& ex) {
        ERR_POST_X(36, "Could unregister from NetScehdule services: "
                   << ex.what());
    } catch(...) {
        ERR_POST_X(37, "Could unregister from NetScehdule services: "
            "Unknown error." );
    }

    LOG_POST_X(38, "Worker Node has been stopped.");


    CRef<CGridCleanupThread> cleanup_thread(
        new CGridCleanupThread(this, m_Listener.get()));

    cleanup_thread->Run();

    LOG_POST_X(55, "Stopping Control thread...");
    control_thread->Stop();
    control_thread->Join();

    CNcbiOstrstream os;
    CGridGlobals::GetInstance().GetJobsWatcher().Print(os);
    LOG_POST_X(56, string(CNcbiOstrstreamToString(os)));

    if (m_IdleThread) {
        if (!m_IdleThread->IsShutdownRequested()) {
            LOG_POST_X(57, "Stopping Idle thread...");
            m_IdleThread->RequestShutdown();
        }
        m_IdleThread->Join();
    }

    if (cleanup_thread->Wait(thread_pool_timeout)) {
        cleanup_thread->Join();
        LOG_POST_X(58, "Cleanup thread finished");
    } else {
        ERR_POST_X(59, "Clean-up thread timed out");
    }

    return 0;
}

void CGridWorkerNode::RequestShutdown()
{
    CGridGlobals::GetInstance().
        RequestShutdown(CNetScheduleAdmin::eShutdownImmediate);
}


void CGridWorkerNode::AttachJobWatcher(IWorkerNodeJobWatcher& job_watcher,
                                           EOwnership owner)
{
    if (!m_JobWatchers.get())
        m_JobWatchers.reset(new CWorkerNodeJobWatchers);
    m_JobWatchers->AttachJobWatcher(job_watcher, owner);
};

void CGridWorkerNode::SetListener(IGridWorkerNodeApp_Listener* listener)
{
    m_Listener.reset(listener ? listener : new CGridWorkerNodeApp_Listener());
}


void CGridWorkerNode::x_NotifyJobWatcher(const CWorkerNodeJobContext& job,
    IWorkerNodeJobWatcher::EEvent event)
{
    if (m_JobWatchers.get() != NULL) {
        CFastMutexGuard guard(m_JobWatcherMutex);
        m_JobWatchers->Notify(job, event);
    }
}

bool CGridWorkerNode::x_GetNextJob(CNetScheduleJob& job)
{
    bool job_exists = false;

    CGridDebugContext* debug_context = CGridDebugContext::GetInstance();

    if (debug_context &&
        debug_context->GetDebugMode() == CGridDebugContext::eGDC_Execute) {
        job_exists = debug_context->GetNextJob(job.job_id, job.input);
        if (!job_exists) {
            CGridGlobals::GetInstance().
                RequestShutdown(CNetScheduleAdmin::eNormalShutdown);
        }
    } else {
        if (!x_AreMastersBusy()) {
            SleepSec(m_NSTimeout);
            return false;
        }

        if (!WaitForExclusiveJobToFinish())
            return false;

        job_exists = GetNSExecuter().WaitJob(job, m_NSTimeout);

        if (job_exists && job.mask & CNetScheduleAPI::eExclusiveJob)
            job_exists = EnterExclusiveModeOrReturnJob(job);
    }
    if (job_exists && CGridGlobals::GetInstance().IsShuttingDown()) {
        x_ReturnJob(job.job_id);
        return false;
    }
    return job_exists;
}

void CGridWorkerNode::x_ReturnJob(const string& job_key)
{
    CGridDebugContext* debug_context = CGridDebugContext::GetInstance();
    if (!debug_context || debug_context->GetDebugMode() != CGridDebugContext::eGDC_Execute) {
         GetNSExecuter().ReturnJob(job_key);
    }
}

void CGridWorkerNode::x_FailJob(const string& job_key, const string& reason)
{
    CGridDebugContext* debug_context = CGridDebugContext::GetInstance();
    if (!debug_context ||
            debug_context->GetDebugMode() != CGridDebugContext::eGDC_Execute) {
         CNetScheduleJob job(job_key);
         job.error_msg = reason;
         GetNSExecuter().PutFailure(job);
    }
}

size_t CGridWorkerNode::GetServerOutputSize() const
{
    return IsEmeddedStorageUsed() ?
        GetNSClient().GetServerParams().max_output_size : 0;
}

bool CGridWorkerNode::IsHostInAdminHostsList(const string& host) const
{
    if (m_AdminHosts.empty())
        return true;
    unsigned int ha = CSocketAPI::gethostbyname(host);
    if (m_AdminHosts.find(ha) != m_AdminHosts.end())
        return true;
    unsigned int ha_lh = CSocketAPI::gethostbyname("");
    if (ha == ha_lh) {
        ha = CSocketAPI::gethostbyname("localhost");
        if (m_AdminHosts.find(ha) != m_AdminHosts.end())
            return true;
    }
    return false;
}

bool CGridWorkerNode::x_AreMastersBusy() const
{
    ITERATE(set<SServerAddress>, it, m_Masters) {
        STimeout tmo = {0, 500};
        CSocket socket(it->host, it->port, &tmo, eOff);
        if (socket.GetStatus(eIO_Open) != eIO_Success)
            continue;

        CNcbiOstrstream os;
        os << GetJobVersion() << endl;
        os << GetNSClient().GetQueueName()  <<";"
           << GetNSClient().GetService().GetServiceName();
        os << endl;
        os << "GETLOAD" << ends;
        if (socket.Write(os.str(), os.pcount()) != eIO_Success) {
            os.freeze(false);
            continue;
        }
        os.freeze(false);
        string reply;
        if (socket.ReadLine(reply) != eIO_Success)
            continue;
        if (NStr::StartsWith(reply, "ERR:")) {
            string msg;
            NStr::Replace(reply, "ERR:", "", msg);
            ERR_POST_X(43, "Worker Node at " << it->AsString() <<
                " returned error: " << msg);
        } else if (NStr::StartsWith(reply, "OK:")) {
            string msg;
            NStr::Replace(reply, "OK:", "", msg);
            try {
                int load = NStr::StringToInt(msg);
                if (load > 0)
                    return false;
            } catch (...) {}
        } else {
            ERR_POST_X(44, "Worker Node at " << it->AsString() <<
                " returned unknown reply: " << reply);
        }
    }
    return true;
}

bool CGridWorkerNode::IsTimeToRebalance()
{
    if (!m_SharedNSClient.GetService().IsLoadBalanced() ||
            TWorkerNode_DoNotRebalance::GetDefault())
        return false;

    m_RebalanceStrategy->OnResourceRequested();
    return m_RebalanceStrategy->NeedRebalance();
}

bool CGridWorkerNode::EnterExclusiveMode()
{
    if (m_ExclusiveJobSemaphore.TryWait()) {
        _ASSERT(!m_IsProcessingExclusiveJob);

        m_IsProcessingExclusiveJob = true;
        return true;
    }
    return false;
}

bool CGridWorkerNode::EnterExclusiveModeOrReturnJob(
    CNetScheduleJob& exclusive_job)
{
    _ASSERT(exclusive_job.mask & CNetScheduleAPI::eExclusiveJob);

    if (!EnterExclusiveMode()) {
        x_ReturnJob(exclusive_job.job_id);
        return false;
    }
    return true;
}

void CGridWorkerNode::LeaveExclusiveMode()
{
    _ASSERT(m_IsProcessingExclusiveJob);

    m_IsProcessingExclusiveJob = false;
    m_ExclusiveJobSemaphore.Post();
}

bool CGridWorkerNode::WaitForExclusiveJobToFinish()
{
    if (m_ExclusiveJobSemaphore.TryWait(m_NSTimeout)) {
        m_ExclusiveJobSemaphore.Post();
        return true;
    }
    return false;
}

void CGridWorkerNode::DisableDefaultRequestEventLogging()
{
    s_ReqEventsDisabled = true;
}

IWorkerNodeCleanupEventSource* CGridWorkerNode::GetCleanupEventSource()
{
    return m_CleanupEventSource;
}

END_NCBI_SCOPE
