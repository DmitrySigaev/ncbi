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
 * Authors:  Maxim Didneko, Anatoliy Kuznetsov
 *
 * File Description:
 *   NetSchedule Worker Node Framework Interfaces.
 *
 */

/// @file grid_worker.hpp
/// NetSchedule Framework specs. 
///

#include <corelib/ncbistre.hpp>
#include <corelib/ncbimisc.hpp>
#include <corelib/ncbitime.hpp>
#include <corelib/ncbireg.hpp>
#include <corelib/ncbithr.hpp>
#include <corelib/blob_storage.hpp>
#include <connect/connect_export.h>
#include <connect/services/ns_client_factory.hpp>
#include <connect/services/ns_client_wrappers.hpp>
#include <util/thread_pool.hpp>

BEGIN_NCBI_SCOPE

/** @addtogroup NetScheduleClient
 *
 * @{
 */

class CArgs;
class IRegistry;
class CNcbiEnvironment;

/// Worker Node initialize context
///
/// An instance of a class which implements this interface
/// is passed to a constructor of a worker node job class
/// The worker node job class can use this class to get 
/// configuration parameters.
///
class IWorkerNodeInitContext
{
public:
    virtual ~IWorkerNodeInitContext() {}

    /// Get a config file regestry
    ///
    virtual const IRegistry&        GetConfig() const;

    /// Get command line arguments
    ///
    virtual const CArgs&            GetArgs() const;

    /// Get environment variables
    ///
    virtual const CNcbiEnvironment& GetEnvironment() const;
};

class CWorkerNodeJobContext;
class CWorkerNodeControlThread;
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
    /// Job is considered successfull if method calls 
    /// CommitJob (see CWorkerNodeJobContext).
    /// If method does not call CommitJob job is considered unresolved 
    /// and returned back to the queue.
    /// Method can throw an exception (derived from std::exception),
    /// in this case job is considered failed (error message will be
    /// redirected to the netschedule queue)
    ///
    /// @param context
    ///   Context where a job can get all requered information
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
/// Context in which a job is runnig, gives access to input and output
/// storage and some job control parameters.
///
/// @sa IWorkerNodeJob
///
class NCBI_XCONNECT_EXPORT CWorkerNodeJobContext
{
public:
    
    ~CWorkerNodeJobContext();

    /// Get a job key
    const string& GetJobKey() const        { return m_JobKey; }

    /// Get a job input string.
    ///
    /// This string can be used in two ways.
    /// 1. It can be an input data for a job (if that data is short)
    ///    If it is so don't use GetIStream method.
    /// 2. It holds a key to a data stored in an external data storage.
    ///    (NetCache)  In this case use GetIStream method to get a stream with 
    ///    an input data for the job
    ///
    const string& GetJobInput() const      { return m_JobInput; }

    /// Set a job's output. This string will be sent to
    /// the queue when job is done.
    /// 
    /// This method can be used to send a short data back to the client.
    /// To send a large result use GetOStream method. Don't call this 
    /// method after GetOStream method is called.
    ///
    void          SetJobOutput(const string& output) { m_JobOutput = output; }

    /// Get a stream with input data for a job. Stream is based on network
    /// data storage (NetCache). Size of the input data can be determined 
    /// using GetInputBlobSize. Will throw a CNetCacheStorageException 
    /// exception with code eBlocked if the mode parameter is set to 
    /// IBlobStorage::eLockNoWait and a blob is blocked by 
    /// another process.
    ///
    CNcbiIstream& GetIStream(IBlobStorage::ELockMode mode =
                             IBlobStorage::eLockWait);

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
    /// This method should be called at the end of IWorkerNodeJob::Do 
    /// method. If this method is not called the job's result will NOT be sent 
    /// to the queue (job is returned back to the NetSchedule queue 
    /// and re-executed in a while)
    ///
    void CommitJob()                       { m_JobCommitted = true; }

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
    CNetScheduleClient::EShutdownLevel GetShutdownLevel(void) const;

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
    /// finish in the specified timeframe (because of a failure) 
    /// it is going to be rescheduled
    ///
    /// Default value for the run timeout specified in the queue settings on 
    /// the server side.
    ///
    /// @param time_to_run
    ///    Time in seconds to finish the job. 0 means "queue default value".
    ///
    void SetJobRunTimeout(unsigned time_to_run);

    /// Check if logging was requested in config file
    ///
    bool IsLogRequested(void) const { return m_LogRequested; }

private:    
    friend class CGridThreadContext;
    void SetThreadContext(CGridThreadContext*);
    const string& GetJobOutput() const { return m_JobOutput; }
    string& SetJobOutput()             { return m_JobOutput; }
    string& SetJobProgressMsgKey()     { return m_ProgressMsgKey; }
    size_t& SetJobInputBlobSize()      { return m_InputBlobSize; }
    
    friend class CWorkerNodeRequest;
    CGridWorkerNode& GetWorkerNode()   { return m_WorkerNode; }
    bool IsJobCommitted() const        { return m_JobCommitted; }   
    unsigned int GetJobNumber() const  { return m_JobNumber; }

    /// Only a CGridWorkerNode can create an instance of this class
    friend class CGridWorkerNode;
    CWorkerNodeJobContext(CGridWorkerNode&   worker_node,
                          const string&      job_key,
                          const string&      job_input,
                          unsigned int       job_nubmer,
                          bool               log_requested);

    void Reset(const string& job_key,
               const string& job_input,
               unsigned int  job_nubmer);

    CGridWorkerNode&     m_WorkerNode;
    string               m_JobKey;
    string               m_JobInput;
    string               m_JobOutput;
    string               m_ProgressMsgKey;
    bool                 m_JobCommitted;
    size_t               m_InputBlobSize;
    bool                 m_LogRequested;
    unsigned int         m_JobNumber;
    CGridThreadContext*  m_ThreadContext;

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
};

/// Worker Node Idle Task Interaface
///
///  @sa IWorkerNodeJobFactory, CWorkerNodeIdleTaskContext
///
class IWorkerNodeIdleTask 
{
public:
    virtual ~IWorkerNodeIdleTask() {};
    
    /// Do an Idle Taks here. 
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

    /// Get an Idle task
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
            LOG_POST("Idle tast is not created: " << ex.what());
        } catch (...) {
            LOG_POST("Idle tast is not created: Unknown error");
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


/// Grid Worker Node
/// 
/// It gets jobs from a NetSchedule server and runs them simultaneously
/// in the different threads (thread pool is used).
///
class NCBI_XCONNECT_EXPORT CGridWorkerNode
{
public:

    /// Construct a worker node using class factories
    ///
    CGridWorkerNode(IWorkerNodeJobFactory&      job_creator,
                    IBlobStorageFactory&        ns_storage_creator,
                    INetScheduleClientFactory&  ns_client_creator,
                    IWorkerNodeJobWatcher*      job_wather);

    virtual ~CGridWorkerNode();

    /// Set a UDP port to listen to input jobs
    ///
    void SetListeningPort(unsigned int udp_port) { m_UdpPort = udp_port; }
    unsigned int GetListeningPort() const { return m_UdpPort; }

    /// Set the maximum threads running simultaneously
    ///
    void SetMaxThreads(unsigned int max_threads) 
                      { m_MaxThreads = max_threads; }

    /// Get the maximum threads running simultaneously
    //
    unsigned int GetMaxThreads() const { return m_MaxThreads; }

    void SetInitThreads(unsigned int init_threads) 
                      { m_InitThreads = init_threads; }

    void SetNSTimeout(unsigned int timeout) { m_NSTimeout = timeout; }
    void SetThreadsPoolTimeout(unsigned int timeout)
                      { m_ThreadsPoolTimeout = timeout; }

    void ActivateServerLog(bool on_off) 
                      { m_LogRequested = on_off; }

    void AcivatePermanentConnection(bool on_off);

    void SetMasterWorkerNodes(const string& hosts);
    void SetAdminHosts(const string& hosts);
    bool IsHostInAdminHostsList(const string& host) const;

    void SetUseEmbeddedInput(bool on_off) { m_UseEmbeddedInput = on_off; }
    bool IsEmeddedInputUsed() const { return m_UseEmbeddedInput; }
    /// Start jobs execution.
    ///
    void Start();

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
        return m_JobFactory.GetJobVersion();
    }

    /// Get a Connection Info
    ///
    string GetConnectionInfo() const;

    void PutOnHold(bool hold);
    bool IsOnHold() const;
   
private:
    IWorkerNodeJobFactory&       m_JobFactory;
    IBlobStorageFactory&         m_NSStorageFactory;
    INetScheduleClientFactory&   m_NSClientFactory;
    IWorkerNodeJobWatcher*       m_JobWatcher;

    auto_ptr<INSCWrapper>        m_NSReadClient;
    CFastMutex                   m_SharedClientMutex;
    auto_ptr<CNetScheduleClient> m_SharedNSClient;

    auto_ptr<CStdPoolOfThreads>  m_ThreadsPool;
    unsigned int                 m_UdpPort;
    unsigned int                 m_MaxThreads;
    unsigned int                 m_InitThreads;
    unsigned int                 m_NSTimeout;
    unsigned int                 m_ThreadsPoolTimeout;
    CFastMutex                   m_NSClientFactoryMutex;
    mutable CFastMutex           m_JobFactoryMutex;
    CFastMutex                   m_StorageFactoryMutex;
    CFastMutex                   m_JobWatcherMutex;
    bool                         m_LogRequested;
    volatile bool                m_OnHold;
    CSemaphore                   m_HoldSem;
    mutable CFastMutex           m_HoldMutex;
    bool                         m_UseEmbeddedInput;

    friend class CGridThreadContext;
    IWorkerNodeJob* CreateJob()
    {
        CFastMutexGuard guard(m_JobFactoryMutex);
        return m_JobFactory.CreateInstance();
    }
    IBlobStorage* CreateStorage()
    {
        CFastMutexGuard guard(m_StorageFactoryMutex);
        return  m_NSStorageFactory.CreateInstance();
    }
    INSCWrapper* CreateClient();

    void x_NotifyJobWatcher(const CWorkerNodeJobContext& job,
                            IWorkerNodeJobWatcher::EEvent event)
    {
        if (m_JobWatcher) {
            CFastMutexGuard guard(m_JobWatcherMutex);
            m_JobWatcher->Notify(job, event);
        }
    }

    struct SHost {
        SHost(string h, unsigned int p) : host(h), port(p) {}
        bool operator== (const SHost& h) const 
        { return host == h.host && port == h.port; }
        bool operator< (const SHost& h) const 
        { return (host == h.host ? (port < h.port) : (host < h.host)); }
        
        string host;
        unsigned int port;
    };
    set<SHost> m_Masters;
    set<unsigned int> m_AdminHosts;

    bool x_GetNextJob(string& job_key, string& input);

    friend class CWorkerNodeRequest;
    void x_ReturnJob(const string& job_key);
    bool x_CreateNSReadClient();
    bool x_AreMastersBusy() const;

    CGridWorkerNode(const CGridWorkerNode&);
    CGridWorkerNode& operator=(const CGridWorkerNode&);

};

/* @} */


END_NCBI_SCOPE

#define WN_BUILD_DATE " build " __DATE__ " " __TIME__


/*
 * ===========================================================================
 * $Log$
 * Revision 1.39  2006/03/15 17:30:11  didenko
 * Added ability to use embedded NetSchedule job's storage as a job's input/output data instead of using it as a NetCache blob key. This reduces network traffic and increases job submittion speed.
 *
 * Revision 1.38  2006/03/07 17:19:37  didenko
 * Perfomance and error handling improvements
 *
 * Revision 1.37  2006/02/27 14:50:20  didenko
 * Redone an implementation of IBlobStorage interface based on NetCache as a plugin
 *
 * Revision 1.36  2006/02/15 20:27:45  didenko
 * Added new optional config parameter "reuse_job_object" which allows
 * reusing IWorkerNodeJob objects in the jobs' threads instead of
 * creating a new object for each job.
 *
 * Revision 1.35  2006/02/15 17:15:41  didenko
 * Added ReqeustShutdown method to worker node idle task context
 *
 * Revision 1.34  2006/02/15 15:19:03  didenko
 * Implemented an optional possibility for a worker node to have a permanent connection
 * to a NetSchedule server.
 * To expedite the job exchange between a worker node and a NetSchedule server,
 * a call to CNetScheduleClient::PutResult method is replaced to a
 * call to CNetScheduleClient::PutResultGetJob method.
 *
 * Revision 1.33  2006/02/01 17:17:26  didenko
 * Added missing export specifier
 *
 * Revision 1.32  2006/02/01 16:39:01  didenko
 * Added Idle Task facility to the Grid Worker Node Framework
 *
 * Revision 1.31  2006/01/18 17:47:42  didenko
 * Added JobWatchers mechanism
 * Reimplement worker node statistics as a JobWatcher
 * Added JobWatcher for diag stream
 * Fixed a problem with PutProgressMessage method of CWorkerNodeThreadContext class
 *
 * Revision 1.30  2005/12/20 17:26:22  didenko
 * Reorganized netschedule storage facility.
 * renamed INetScheduleStorage to IBlobStorage and moved it to corelib
 * renamed INetScheduleStorageFactory to IBlobStorageFactory and moved it to corelib
 * renamed CNetScheduleNSStorage_NetCache to CBlobStorage_NetCache and moved it
 * to separate files
 * Moved CNetScheduleClientFactory to separate files
 *
 * Revision 1.29  2005/10/26 16:37:44  didenko
 * Added for non-blocking read for netschedule storage
 *
 * Revision 1.28  2005/09/30 14:58:56  didenko
 * Added optional parameter to PutProgressMessage methods which allows
 * sending progress messages regardless of the rate control.
 *
 * Revision 1.27  2005/07/19 20:10:54  ucko
 * Predeclare CGridThreadContext for the sake of G++ 4.
 *
 * Revision 1.26  2005/05/27 14:46:06  didenko
 * Fixed a worker node statistics
 *
 * Revision 1.25  2005/05/27 12:51:59  didenko
 * Made GetStatistics a public static function
 *
 * Revision 1.24  2005/05/19 15:15:23  didenko
 * Added admin_hosts parameter to worker nodes configurations
 *
 * Revision 1.23  2005/05/16 14:20:55  didenko
 * Added master/slave dependances between worker nodes.
 *
 * Revision 1.22  2005/05/12 18:14:33  didenko
 * Cosmetics
 *
 * Revision 1.21  2005/05/12 16:18:49  ucko
 * Use portable DECLARE_CLASS_STATIC_MUTEX macro.
 *
 * Revision 1.20  2005/05/12 14:52:05  didenko
 * Added a worker node build time and start time to the statistic
 *
 * Revision 1.19  2005/05/11 18:57:39  didenko
 * Added worker node statictics
 *
 * Revision 1.18  2005/05/05 15:18:51  didenko
 * Added debugging facility to worker nodes
 *
 * Revision 1.17  2005/05/03 19:54:17  didenko
 * Don't start the threads pool then a worker node is running in a single threaded mode.
 *
 * Revision 1.16  2005/04/28 18:46:55  didenko
 * Added Request rate control to PutProgressMessage method
 *
 * Revision 1.15  2005/04/27 15:16:29  didenko
 * Added rotating log
 * Added optional deamonize
 *
 * Revision 1.14  2005/04/21 20:15:52  didenko
 * Added some comments
 *
 * Revision 1.13  2005/04/21 19:53:19  didenko
 * Minor fix
 *
 * Revision 1.12  2005/04/21 19:38:52  didenko
 * Fixed version generation
 *
 * Revision 1.11  2005/04/21 19:10:01  didenko
 * Added IWorkerNodeInitContext
 * Added some convenient macros
 *
 * Revision 1.10  2005/04/20 19:25:59  didenko
 * Added support for progress messages passing from a worker node to a client
 *
 * Revision 1.9  2005/04/19 18:58:52  didenko
 * Added Init method to CGridWorker
 *
 * Revision 1.8  2005/04/07 16:46:28  didenko
 * + Program Version checking
 *
 * Revision 1.7  2005/03/28 16:49:00  didenko
 * Added virtual desturctors to all new interfaces to prevent memory leaks
 *
 * Revision 1.6  2005/03/25 16:24:58  didenko
 * Classes restructure
 *
 * Revision 1.5  2005/03/23 21:26:04  didenko
 * Class Hierarchy restructure
 *
 * Revision 1.4  2005/03/23 13:10:32  kuznets
 * documented and doxygenized
 *
 * Revision 1.3  2005/03/22 21:42:50  didenko
 * Got rid of warnning on Sun WorkShop
 *
 * Revision 1.2  2005/03/22 20:35:56  didenko
 * Make it compile under CGG
 *
 * Revision 1.1  2005/03/22 20:17:55  didenko
 * Initial version
 *
 * ===========================================================================
 */


#endif //CONNECT_SERVICES__GRID_WOKER_HPP

