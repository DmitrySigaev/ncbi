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
#include <connect/connect_export.h>
#include <connect/services/netschedule_client.hpp>
#include <connect/services/netschedule_storage.hpp>
#include <util/thread_pool.hpp>

BEGIN_NCBI_SCOPE

class CWorkerNodeJobContext;

/// Worker Node Job interface
/// 
/// @sa CWorkerNodeJobContext
///
class IWorkerNodeJob
{
public:
    /// Execute the real job
    ///
    /// @param context
    ///   Context where a job can get all requered information
    ///   like input/output steams, the job key etc.
    ///
    virtual int Do(CWorkerNodeJobContext& context) = 0;
};

class CGridWorkerNode;
class CWorkerNodeRequest;
/// Worker Node Job Context
///
/// Context in which a job is runnig
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
    ///    In this case use GetIStream method to get a stream with 
    ///    an input data for the job
    ///
    const string& GetJobInput() const      { return m_JobInput; }

    /// Set a job's output. This string will be sent to
    /// a NetScehule server when job is done.
    /// 
    /// This method can be used to send a short data back to the client.
    /// To send a large result use GetOStream method. Don't call this 
    /// method after GetOStream method is called.
    ///
    void          SetJobOutput(const string& output) { m_JobOutput = output; }

    /// Get a stream with input data for a job
    ///
    CNcbiIstream& GetIStream();

    /// Get the size of an input stream
    size_t        GetInputBlobSize() const { return m_InputBlobSize; }

    /// Get a stream where a job can write its result
    ///
    CNcbiOstream& GetOStream();

    /// Confirm that a job is done and a result is ready to be sent 
    /// back to the client. 
    ///
    /// This method should be called at the end of Do method of your job.
    /// If this method is not called the job's result will not be sent to
    /// the client and a job will be returned back to the NetSchedule queue
    ///
    void CommitJob()                       { m_JobStatus = true; }

    /// Check if shutdown was requested.
    ///
    /// If a job takes a long time to do its task it should call 
    /// this method during it execution and check if node shutdown was 
    /// requested and if so gracefully finish its work.
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

private:    
    friend CWorkerNodeRequest;  
    void Reset();
    CGridWorkerNode& GetWorkerNode() { return m_WorkerNode; }
    bool IsJobCommited() const       { return m_JobStatus; }

    /// Only a CGridWorkerNode can create an instance of this class
    friend CGridWorkerNode;
    CWorkerNodeJobContext(CGridWorkerNode& worker_node,
                          const string&    job_key,
                          const string&    job_input);

    CGridWorkerNode&     m_WorkerNode;
    string               m_JobKey;
    string               m_JobInput;
    string               m_JobOutput;
    bool                 m_JobStatus;
    size_t               m_InputBlobSize;

    INetScheduleStorage* m_Reader;
    INetScheduleStorage* m_Writer;
    CNetScheduleClient*  m_Reporter;

    /// The copy constructor and the assignment operator
    /// are prohibited
    CWorkerNodeJobContext(const CWorkerNodeJobContext&);
    CWorkerNodeJobContext& operator=(const CWorkerNodeJobContext&);
};

/// Worker Node Job Factory interface
///
/// @sa IWorkerNodeJob
///
class IWorkerNodeJobFactory
{
public:
    /// Create a job
    ///
    virtual IWorkerNodeJob* CreateJob(void) = 0;

    /// Get the job version
    ///
    virtual string GetJobVersion(void) const = 0;
};

/// NetSchedule Storage Factory interafce
///
/// @sa INetScheduleStorage
///
class INetScheduleStorageFactory
{
public:
    /// Create a NetSchedule storage
    ///
    virtual INetScheduleStorage* CreateStorage(void) = 0;
};

/// NetSchedule Client Factory interface
///
/// @sa CNetScheduleClient
///
class INetScheduleClientFactory
{
public:
    /// Create a NetSchedule client
    ///
    virtual CNetScheduleClient* CreateClient(void) = 0;
};

/// Crid Worker Node
/// 
/// This call does all the magic.
/// It gets jobs from a NetSchedule server and runs them simultaneously
/// in the different threads.
///
class NCBI_XCONNECT_EXPORT CGridWorkerNode
{
public:

    /// Construct a worker node with given factories
    ///
    CGridWorkerNode(INetScheduleClientFactory& ns_client_creator, 
                    INetScheduleStorageFactory& ns_storage_creator,
                    IWorkerNodeJobFactory& job_creator);

    virtual ~CGridWorkerNode();

    /// Set a UDP port to listen to input jobs
    ///
    void SetListeningPort(unsigned int udp_port) { m_UdpPort = udp_port; }

    /// Set the maximum threads running simultaneously
    ///
    void SetMaxThreads(unsigned int max_threads) 
                      { m_MaxThreads = max_threads; }

    /// Set the maximum number of jobs ready to execution
    ///                    
    void SetQueueSize(unsigned int queue_size) { m_QueueSize = queue_size; }

    void SetInitThreads(unsigned int init_threads) 
                       { m_InitThreads = init_threads; }

    void SetNSTimeout(unsigned int timeout) { m_NSTimeout = timeout; }
    void SetThreadsPoolTimeout(unsigned int timeout)
                              { m_ThreadsPoolTimeout = timeout; }

    /// Start jobs execution.
    ///
    void Start();

    /// Request node shutdown
    void RequestShutdown(CNetScheduleClient::EShutdownLevel level) 
                        { m_ShutdownLevel = level; }


    /// Check if shutdown was requested.
    ///
    CNetScheduleClient::EShutdownLevel GetShutdownLevel(void) 
                        { return m_ShutdownLevel; }

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
        CMutexGuard guard(m_JobFactoryMutex);
        return m_JobFactory.GetJobVersion();
    }

private:
    INetScheduleClientFactory&   m_NSClientFactory;
    INetScheduleStorageFactory&  m_NSStorageFactory;
    IWorkerNodeJobFactory&       m_JobFactory;

    auto_ptr<CNetScheduleClient> m_NSReadClient;
    auto_ptr<CStdPoolOfThreads>  m_ThreadsPool;
    unsigned int                 m_UdpPort;
    unsigned int                 m_MaxThreads;
    unsigned int                 m_QueueSize;
    unsigned int                 m_InitThreads;
    unsigned int                 m_NSTimeout;
    unsigned int                 m_ThreadsPoolTimeout;
    CMutex                       m_NSClientFactoryMutex;
    mutable CMutex               m_JobFactoryMutex;
    CMutex                       m_StorageFactoryMutex;
    volatile CNetScheduleClient::EShutdownLevel m_ShutdownLevel;

    friend CWorkerNodeRequest;
    auto_ptr<IWorkerNodeJob> CreateJob()
    {
        CMutexGuard guard(m_JobFactoryMutex);
        return 
            auto_ptr<IWorkerNodeJob>(m_JobFactory.CreateJob());
    }
    auto_ptr<INetScheduleStorage> CreateStorage()
    {
        CMutexGuard guard(m_StorageFactoryMutex);
        return 
            auto_ptr<INetScheduleStorage>(m_NSStorageFactory.CreateStorage());
    }
    auto_ptr<CNetScheduleClient> CreateClient()
    {
        CMutexGuard guard(m_NSClientFactoryMutex);
        return 
            auto_ptr<CNetScheduleClient>(m_NSClientFactory.CreateClient());
    }

    CGridWorkerNode(const CGridWorkerNode&);
    CGridWorkerNode& operator=(const CGridWorkerNode&);

};



/////////////////////////////////////////////////////////////////////////////

END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.1  2005/03/22 20:17:55  didenko
 * Initial version
 *
 * ===========================================================================
 */


#endif //CONNECT_SERVICES__GRID_WOKER_HPP

