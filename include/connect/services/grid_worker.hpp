#ifndef CONNECT_SERVICES__GRID_WORKER_HPP
#define CONNECT_SERVICES__GRID_WORKER_HPP


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
 * Authors:  Maxim Didneko, Anatoliy Kuznetsov, Dmitry Kazimirov
 *
 * File Description:
 *   NetSchedule Worker Node Framework Interfaces.
 *
 */

/// @file grid_worker.hpp
/// Grid Framework specs.
///

#include <connect/services/netschedule_api.hpp>
#include <connect/services/netcache_api.hpp>
#include <connect/services/error_codes.hpp>

#include <connect/connect_export.h>

#include <util/thread_pool.hpp>

#include <corelib/ncbistre.hpp>
#include <corelib/ncbimisc.hpp>
#include <corelib/ncbitime.hpp>
#include <corelib/ncbireg.hpp>
#include <corelib/ncbithr.hpp>
#include <corelib/ncbiapp.hpp>

BEGIN_NCBI_SCOPE

/** @addtogroup NetScheduleClient
 *
 * @{
 */

class CArgs;
class IRegistry;
class CNcbiEnvironment;

class IWorkerNodeCleanupEventListener {
public:
    /// Event notifying of a safe clean-up point. It is generated for a
    /// job after the job is finished (or the worker node is shutting
    /// down). It is also generated for the whole worker node -- when
    /// it is shutting down.
    enum EWorkerNodeCleanupEvent {
        /// For jobs -- run from the same thread after Do() is done;
        /// for the whole WN -- run from a separate (clean-up) thread
        /// after all jobs are done and cleaned up, and the whole worker
        /// node is shutting down.
        eRegularCleanup,

        /// Called on emergency shutdown, always from a different (clean-up)
        /// thread, even for the jobs.
        eOnHardExit
    };

    virtual void HandleEvent(EWorkerNodeCleanupEvent cleanup_event) = 0;
    virtual ~IWorkerNodeCleanupEventListener() {}
};

/// Clean-up event source for the worker node. This interface provides
/// for subscribing for EWorkerNodeCleanupEvent. It is used by both
/// IWorkerNodeInitContext (for the global worker node clean-up) and
/// IWorkerNodeJob (for per-job clean-up).
class NCBI_XCONNECT_EXPORT IWorkerNodeCleanupEventSource : public CObject
{
public:
    virtual void AddListener(IWorkerNodeCleanupEventListener* listener) = 0;
    virtual void RemoveListener(IWorkerNodeCleanupEventListener* listener) = 0;

    virtual void CallEventHandlers() = 0;
};

/// Worker Node initialize context
///
/// An instance of a class which implements this interface
/// is passed to a constructor of a worker node job class
/// The worker node job class can use this class to get
/// configuration parameters.
///
class NCBI_XCONNECT_EXPORT IWorkerNodeInitContext
{
public:
    virtual ~IWorkerNodeInitContext() {}

    /// Get a config file registry
    ///
    virtual const IRegistry&        GetConfig() const = 0;

    /// Get command line arguments
    ///
    virtual const CArgs&            GetArgs() const = 0;

    /// Get environment variables
    ///
    virtual const CNcbiEnvironment& GetEnvironment() const = 0;

    /// Get interface for registering clean-up event listeners
    ///
    virtual IWorkerNodeCleanupEventSource* GetCleanupEventSource() const = 0;

    /// Get the shared NetCacheAPI object used by the worker node framework.
    ///
    virtual CNetCacheAPI GetNetCacheAPI() const = 0;
};

class CWorkerNodeJobContext;
class CWorkerNodeControlServer;
/// Worker Node Job interface.
///
/// This interface is a worker node workhorse.
/// Job is executed by method Do of this interface.
/// Worker node application may be configured to execute several jobs at once
/// using threads, so implementation must be thread safe.
///
/// @sa CWorkerNodeJobContext, IWorkerNodeInitContext
///
class IWorkerNodeJob : public CObject
{
public:
    virtual ~IWorkerNodeJob() {}
    /// Execute the job.
    ///
    /// Job is considered successfully done if the Do method calls
    /// CommitJob (see CWorkerNodeJobContext).
    /// If method does not call CommitJob job is considered unresolved
    /// and returned back to the queue.
    /// Method can throw an exception (derived from std::exception),
    /// in this case job is considered failed (error message will be
    /// redirected to the NetSchedule queue)
    ///
    /// @param context
    ///   Context where a job can get all required information
    ///   like input/output steams, the job key etc.
    ///
    /// @return
    ///   Job exit code
    ///
    virtual int Do(CWorkerNodeJobContext& context) = 0;
};

class CGridWorkerNode;
class CGridThreadContext;
class CWorkerNodeRequest;

/// Worker Node job context
///
/// Context in which a job is running, gives access to input and output
/// storage and some job control parameters.
///
/// @sa IWorkerNodeJob
///
class NCBI_XCONNECT_EXPORT CWorkerNodeJobContext
{
public:
    /// Get the associated job structure to access all of its fields.
    const CNetScheduleJob& GetJob() const {return m_Job;}

    /// Get a job key
    const string& GetJobKey() const        { return m_Job.job_id; }

    /// Get a job input string.
    ///
    /// This string can be used in two ways.
    /// 1. It can be an input data for a job (if that data is short)
    ///    If it is so don't use GetIStream method.
    /// 2. It holds a key to a data stored in an external data storage.
    ///    (NetCache)  In this case use GetIStream method to get a stream with
    ///    an input data for the job
    ///
    const string& GetJobInput() const      { return m_Job.input; }

    /// Set a job's output. This string will be sent to
    /// the queue when job is done.
    ///
    /// This method can be used to send a short data back to the client.
    /// To send a large result use GetOStream method. Don't call this
    /// method after GetOStream method is called.
    ///
    void          SetJobOutput(const string& output) { m_Job.output = output; }

    /// Set the return code of the job.
    ///
    void          SetJobRetCode(int ret_code) { m_Job.ret_code = ret_code; }

    /// Get a stream with input data for a job. Stream is based on network
    /// data storage (NetCache). Size of the input data can be determined
    /// using GetInputBlobSize.
    ///
    CNcbiIstream& GetIStream();

    /// Get the size of an input stream
    ///
    size_t        GetInputBlobSize() const { return m_InputBlobSize; }

    /// Put progress message
    ///
    void          PutProgressMessage(const string& msg,
                                     bool send_immediately = false);

    /// Get a stream where a job can write its result
    ///
    CNcbiOstream& GetOStream();

    /// Confirm that a job is done and result is ready to be sent
    /// back to the client.
    ///
    /// This method should be called at the end of the
    /// IWorkerNodeJob::Do() method.
    ///
    void CommitJob();

    /// Confirm that a job is finished, but an error has happened
    /// during its execution.
    ///
    /// This method should be called at the end of the
    /// IWorkerNodeJob::Do() method.
    ///
    void CommitJobWithFailure(const string& err_msg);

    /// Schedule the job for return.
    ///
    /// This method should be called before exiting the IWorkerNodeJob::Do()
    /// method.  The job will be returned back to the NetSchedule queue
    /// and re-executed after a while.
    ///
    void ReturnJob();

    /// Check if node application shutdown was requested.
    ///
    /// If job takes a long time it should periodically call
    /// this method during the execution and check if node shutdown was
    /// requested and gracefully finish its work.
    ///
    /// Shutdown level eNormal means job can finish the job and exit,
    /// eImmidiate means node should exit immediately
    /// (unfinished jobs are returned back to the queue)
    ///
    CNetScheduleAdmin::EShutdownLevel GetShutdownLevel(void) const;

    /// Get a name of a queue where this node is connected to.
    ///
    const string& GetQueueName() const;

    /// Get a node name
    ///
    const string& GetClientName() const;

    /// Set job execution timeout
    ///
    /// When node picks up the job for execution it may evaluate what time it
    /// takes for computation and report it to the queue. If job does not
    /// finish in the specified time frame (because of a failure)
    /// it is going to be rescheduled
    ///
    /// Default value for the run timeout specified in the queue settings on
    /// the server side.
    ///
    /// @param time_to_run
    ///    Time in seconds to finish the job. 0 means "queue default value".
    ///
    void SetJobRunTimeout(unsigned time_to_run);

    /// Increment job execution timeout
    ///
    /// When node picks up the job for execution it may periodically
    /// communicate to the server that job is still alive and
    /// prolong job execution timeout, so job server does not try to
    /// reschedule.
    ///
    ///
    /// @param runtime_inc
    ///    Estimated time in seconds(from the current moment) to
    ///    finish the job.
    ///
    /// @sa SetRunTimeout
    void JobDelayExpiration(unsigned runtime_inc);

    /// Check if logging was requested in config file
    ///
    bool IsLogRequested(void) const { return m_LogRequested; }

    /// Instruct the system that this job requires all system's resources
    /// If this method is call, the node will not accept any other jobs
    /// until this one is done. In the event if the mode has already been
    /// requested by another job this job will be returned back to the queue.
    void RequestExclusiveMode();

    const string& GetJobOutput() const { return m_Job.output; }

    CNetScheduleAPI::TJobMask GetJobMask() const { return m_Job.mask; }

    size_t GetMaxServerOutputSize() const;

    unsigned int GetJobNumber() const  { return m_JobNumber; }

    enum ECommitStatus {
        eDone,
        eFailure,
        eReturn,
        eNotCommitted,
        eCanceled
    };

    bool IsJobCommitted() const    { return m_JobCommitted != eNotCommitted; }
    ECommitStatus GetCommitStatus() const    { return m_JobCommitted; }

    bool IsCanceled() const { return m_JobCommitted == eCanceled; }

    IWorkerNodeCleanupEventSource* GetCleanupEventSource();

    CGridWorkerNode& GetWorkerNode()   { return m_WorkerNode; }

private:
    friend class CGridThreadContext;
    void SetThreadContext(CGridThreadContext* ctxt) { m_ThreadContext = ctxt; }
    string& SetJobOutput()             { return m_Job.output; }
    size_t& SetJobInputBlobSize()      { return m_InputBlobSize; }
    bool IsJobExclusive() const        { return m_ExclusiveJob; }
    const string& GetErrMsg() const    { return m_Job.error_msg; }

    friend class CWorkerNodeRequest;
    /// Only a CGridWorkerNode can create an instance of this class
    friend class CGridWorkerNode;
    CWorkerNodeJobContext(CGridWorkerNode&   worker_node,
                          const CNetScheduleJob& job,
                          bool               log_requested);

    void SetCanceled() { m_JobCommitted = eCanceled; }
    void CheckIfCanceled();

    void Reset(const CNetScheduleJob& job);

    CGridWorkerNode&     m_WorkerNode;
    CNetScheduleJob      m_Job;
    ECommitStatus        m_JobCommitted;
    size_t               m_InputBlobSize;
    bool                 m_LogRequested;
    unsigned int         m_JobNumber;
    CGridThreadContext*  m_ThreadContext;
    bool                 m_ExclusiveJob;

    CRef<IWorkerNodeCleanupEventSource> m_CleanupEventSource;

    /// The copy constructor and the assignment operator
    /// are prohibited
    CWorkerNodeJobContext(const CWorkerNodeJobContext&);
    CWorkerNodeJobContext& operator=(const CWorkerNodeJobContext&);
};

class CWorkerNodeIdleThread;
/// Worker Node Idle Task Context
///
class NCBI_XCONNECT_EXPORT CWorkerNodeIdleTaskContext
{
public:

    void RequestShutdown();
    bool IsShutdownRequested() const;

    void SetRunAgain() { m_RunAgain = true; }
    bool NeedRunAgain() const { return m_RunAgain; }

    void Reset();

private:
    friend class CWorkerNodeIdleThread;
    CWorkerNodeIdleTaskContext(CWorkerNodeIdleThread& thread);

    CWorkerNodeIdleThread& m_Thread;
    bool m_RunAgain;

private:
    CWorkerNodeIdleTaskContext(const CWorkerNodeIdleTaskContext&);
    CWorkerNodeIdleTaskContext& operator=(const CWorkerNodeIdleTaskContext&);
};

/// Worker Node Idle Task Interface
///
///  @sa IWorkerNodeJobFactory, CWorkerNodeIdleTaskContext
///
class IWorkerNodeIdleTask
{
public:
    virtual ~IWorkerNodeIdleTask() {};

    /// Do the Idle task here.
    /// It should not take a lot time, because while it is running
    /// no real jobs will be processed.
    virtual void Run(CWorkerNodeIdleTaskContext&) = 0;
};

/// Worker Node Job Factory interface
///
/// @sa IWorkerNodeJob
///
class IWorkerNodeJobFactory
{
public:
    virtual ~IWorkerNodeJobFactory() {}
    /// Create a job
    ///
    virtual IWorkerNodeJob* CreateInstance(void) = 0;

    /// Initialize a worker node factory
    ///
    virtual void Init(const IWorkerNodeInitContext& context) {}

    /// Get the job version
    ///
    virtual string GetJobVersion(void) const = 0;

    /// Get the Idle task
    ///
    virtual IWorkerNodeIdleTask* GetIdleTask() { return NULL; }
};

template <typename TWorkerNodeJob>
class CSimpleJobFactory : public IWorkerNodeJobFactory
{
public:
    virtual void Init(const IWorkerNodeInitContext& context)
    {
        m_WorkerNodeInitContext = &context;
    }
    virtual IWorkerNodeJob* CreateInstance(void)
    {
        return new TWorkerNodeJob(*m_WorkerNodeInitContext);
    }

private:
    const IWorkerNodeInitContext* m_WorkerNodeInitContext;
};

#define NCBI_DECLARE_WORKERNODE_FACTORY(TWorkerNodeJob, Version)         \
class TWorkerNodeJob##Factory : public CSimpleJobFactory<TWorkerNodeJob> \
{                                                                        \
public:                                                                  \
    virtual string GetJobVersion() const                                 \
    {                                                                    \
        return #TWorkerNodeJob " version " #Version;                     \
    }                                                                    \
}

template <typename TWorkerNodeJob, typename TWorkerNodeIdleTask>
class CSimpleJobFactoryEx : public IWorkerNodeJobFactory
{
public:
    virtual void Init(const IWorkerNodeInitContext& context)
    {
        m_WorkerNodeInitContext = &context;
        try {
            m_IdleTask.reset(new TWorkerNodeIdleTask(*m_WorkerNodeInitContext));
        } catch (exception& ex) {
            LOG_POST_XX(ConnServ_WorkerNode, 16,
                        "Idle task is not created: " << ex.what());
        } catch (...) {
            LOG_POST_XX(ConnServ_WorkerNode, 17,
                        "Idle task is not created: Unknown error");
        }
    }
    virtual IWorkerNodeJob* CreateInstance(void)
    {
        return new TWorkerNodeJob(*m_WorkerNodeInitContext);
    }
    virtual IWorkerNodeIdleTask* GetIdleTask() { return m_IdleTask.get(); }

private:
    const IWorkerNodeInitContext* m_WorkerNodeInitContext;
    auto_ptr<TWorkerNodeIdleTask> m_IdleTask;
};

#define NCBI_DECLARE_WORKERNODE_FACTORY_EX(TWorkerNodeJob,TWorkerNodeIdleTask, Version) \
class TWorkerNodeJob##FactoryEx                                          \
    : public CSimpleJobFactoryEx<TWorkerNodeJob, TWorkerNodeIdleTask>    \
{                                                                        \
public:                                                                  \
    virtual string GetJobVersion() const                                 \
    {                                                                    \
        return #TWorkerNodeJob " version " #Version;                     \
    }                                                                    \
}



/// Jobs watcher interface
class NCBI_XCONNECT_EXPORT IWorkerNodeJobWatcher
{
public:
    enum EEvent {
        eJobStarted,
        eJobStopped,
        eJobFailed,
        eJobSucceed,
        eJobReturned,
        eJobCanceled,
        eJobLost
    };

    virtual ~IWorkerNodeJobWatcher();

    virtual void Notify(const CWorkerNodeJobContext& job, EEvent event) = 0;

};

class CWorkerNodeJobWatchers;
class CWorkerNodeIdleThread;
class IGridWorkerNodeApp_Listener;
class CWNJobsWatcher;
class CSimpleRebalanceStrategy;
/// Grid Worker Node
///
/// It gets jobs from a NetSchedule server and runs them simultaneously
/// in the different threads (thread pool is used).
///
/// @note
/// Worker node application is parameterized using INI file settings.
/// Please read the sample ini file for more information.
///
class NCBI_XCONNECT_EXPORT CGridWorkerNode
{
public:
    /// Construct a worker node using class factories
    ///
    CGridWorkerNode(CNcbiApplication& app,
        IWorkerNodeJobFactory* job_factory);

    virtual ~CGridWorkerNode();

    void Init(bool default_merge_lines_value);

    /// Start job execution loop.
    ///
    int Run();

    void RequestShutdown();

    void ForceSingleThread() { m_SingleThreadForced = true; }

    void AttachJobWatcher(IWorkerNodeJobWatcher& job_watcher,
                          EOwnership owner = eNoOwnership);

    void SetListener(IGridWorkerNodeApp_Listener* listener);

    IWorkerNodeJobFactory&      GetJobFactory() { return *m_JobFactory; }

    /// Get the maximum threads running simultaneously
    //
    unsigned int GetMaxThreads() const { return m_MaxThreads; }

    bool IsHostInAdminHostsList(const string& host) const;

    bool IsEmeddedStorageUsed() const { return m_UseEmbeddedStorage; }
    unsigned int GetCheckStatusPeriod() const { return m_CheckStatusPeriod; }
    size_t GetServerOutputSize() const;

    /// Get a name of a queue where this node is connected to.
    ///
    const string& GetQueueName() const;

    /// Get a node name
    ///
    const string& GetClientName() const;

    /// Get a job version
    ///
    string GetJobVersion() const
    {
        CFastMutexGuard guard(m_JobFactoryMutex);
        return m_JobFactory->GetJobVersion();
    }

    /// Get a Connection Info
    ///
    const string& GetServiceName() const;

    CNetCacheAPI GetNetCacheAPI() const { return m_NetCacheAPI; }
    CNetScheduleAPI GetNetScheduleAPI() const { return m_NetScheduleAPI; }
    CNetScheduleExecuter GetNSExecuter() const { return m_NSExecuter; }

    bool IsTimeToRebalance();

    bool EnterExclusiveMode();
    bool EnterExclusiveModeOrReturnJob(CNetScheduleJob& exclusive_job);
    void LeaveExclusiveMode();
    bool IsExclusiveMode();
    bool WaitForExclusiveJobToFinish();

    /// Disable the automatic logging of request-start and
    /// request-stop events by the framework itself.
    static void DisableDefaultRequestEventLogging();

    IWorkerNodeCleanupEventSource* GetCleanupEventSource();

private:
    auto_ptr<IWorkerNodeJobFactory>      m_JobFactory;
    auto_ptr<CWorkerNodeJobWatchers>     m_JobWatchers;

    CNetCacheAPI m_NetCacheAPI;
    CNetScheduleAPI m_NetScheduleAPI;
    CNetScheduleExecuter m_NSExecuter;

    unsigned int                 m_MaxThreads;
    unsigned int                 m_NSTimeout;
    mutable CFastMutex           m_JobFactoryMutex;
    CFastMutex                   m_StorageFactoryMutex;
    CFastMutex                   m_JobWatcherMutex;
    unsigned int                 m_CheckStatusPeriod;
    CNetObjectRef<CSimpleRebalanceStrategy> m_RebalanceStrategy;
    CSemaphore                   m_ExclusiveJobSemaphore;
    bool                         m_IsProcessingExclusiveJob;
    bool                         m_UseEmbeddedStorage;

    CRef<IWorkerNodeCleanupEventSource> m_CleanupEventSource;

    friend class CGridThreadContext;
    IWorkerNodeJob* CreateJob()
    {
        CFastMutexGuard guard(m_JobFactoryMutex);
        return m_JobFactory->CreateInstance();
    }
    friend class CWorkerNodeJobContext;

    void x_NotifyJobWatcher(const CWorkerNodeJobContext& job,
                            IWorkerNodeJobWatcher::EEvent event);

    set<SServerAddress> m_Masters;
    set<unsigned int> m_AdminHosts;

    bool x_GetNextJob(CNetScheduleJob& job);

    friend class CWNJobsWatcher;
    friend class CWorkerNodeRequest;
    void x_ReturnJob(const string& job_key);
    void x_FailJob(const string& job_key, const string& reason);
    bool x_AreMastersBusy() const;

    auto_ptr<IWorkerNodeInitContext> m_WorkerNodeInitContext;

    CRef<CWorkerNodeIdleThread>  m_IdleThread;

    auto_ptr<IGridWorkerNodeApp_Listener> m_Listener;

    CNcbiApplication& m_App;
    bool m_SingleThreadForced;
};

inline const string& CGridWorkerNode::GetQueueName() const
{
    return GetNetScheduleAPI().GetQueueName();
}

inline const string& CGridWorkerNode::GetClientName() const
{
    return GetNetScheduleAPI().GetService().GetClientName();
}

inline const string& CGridWorkerNode::GetServiceName() const
{
    return GetNetScheduleAPI().GetService().GetServiceName();
}

inline bool CGridWorkerNode::IsExclusiveMode()
{
    return m_IsProcessingExclusiveJob;
}


class NCBI_XCONNECT_EXPORT CGridWorkerNodeException : public CException
{
public:
    enum EErrCode {
        eJobIsCanceled,
        eJobFactoryIsNotSet,
        eExclusiveModeIsAlreadySet
    };

    virtual const char* GetErrCodeString(void) const
    {
        switch (GetErrCode())
        {
        case eJobIsCanceled:             return "eJobIsCanceled";
        case eJobFactoryIsNotSet:        return "eJobFactoryIsNotSet";
        case eExclusiveModeIsAlreadySet: return "eExclusiveModeIsAlreadySet";
        default:                         return CException::GetErrCodeString();
        }
    }

    NCBI_EXCEPTION_DEFAULT(CGridWorkerNodeException, CException);
};


/* @} */


END_NCBI_SCOPE

#define WN_BUILD_DATE " build " __DATE__ " " __TIME__ " (Framework version: 4.0.0)"

#endif //CONNECT_SERVICES__GRID_WOKER_HPP
