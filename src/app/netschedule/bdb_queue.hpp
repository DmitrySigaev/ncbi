#ifndef NETSCHEDULE_BDB_QUEUE__HPP
#define NETSCHEDULE_BDB_QUEUE__HPP


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
 * Authors:  Anatoliy Kuznetsov
 *
 * File Description:
 *   Net schedule job status database.
 *
 */


/// @file bdb_queue.hpp
/// NetSchedule job status database. 
///
/// @internal


#include <corelib/ncbimtx.hpp>

#include <connect/netschedule_client.hpp>

#include <bdb/bdb_file.hpp>
#include <bdb/bdb_env.hpp>
#include <bdb/bdb_cursor.hpp>
#include <map>
#include <vector>

#include "job_status.hpp"
#include "queue_clean_thread.hpp"
#include "notif_thread.hpp"
#include "job_time_line.hpp"


BEGIN_NCBI_SCOPE

/// BDB table to store queue information
///
/// @internal
///
struct SQueueDB : public CBDB_File
{
    CBDB_FieldUint4        id;              ///< Job id

    CBDB_FieldInt4         status;          ///< Current job status
    CBDB_FieldUint4        time_submit;     ///< Job submit time
    CBDB_FieldUint4        time_run;        ///<     run time
    CBDB_FieldUint4        time_done;       ///<     result submission time
    CBDB_FieldUint4        timeout;         ///<     individual timeout
    CBDB_FieldUint4        run_timeout;     ///<     job run timeout

    CBDB_FieldUint4        subm_addr;       ///< netw BO (for notification)
    CBDB_FieldUint4        subm_port;       ///< notification port
    CBDB_FieldUint4        subm_timeout;    ///< notification timeout

    CBDB_FieldUint4        worker_node1;    ///< IP address of worker node 1
    CBDB_FieldUint4        worker_node2;    ///< reserved
    CBDB_FieldUint4        worker_node3;
    CBDB_FieldUint4        worker_node4;
    CBDB_FieldUint4        worker_node5;

    CBDB_FieldUint4        run_counter;     ///< Number of execution attempts
    CBDB_FieldInt4         ret_code;        ///< Return code

    CBDB_FieldString       input;           ///< Input data
    CBDB_FieldString       output;          ///< Result data

    CBDB_FieldString       err_msg;         ///< Error message (exception::what())

    CBDB_FieldString       cout;            ///< Reserved
    CBDB_FieldString       cerr;            ///< Reserved

    SQueueDB()
    {
        DisableNull(); 

        BindKey("id",      &id);

        BindData("status", &status);
        BindData("time_submit", &time_submit);
        BindData("time_run",    &time_run);
        BindData("time_done",   &time_done);
        BindData("timeout",     &timeout);
        BindData("run_timeout", &run_timeout);

        BindData("subm_addr",    &subm_addr);
        BindData("subm_port",    &subm_port);
        BindData("subm_timeout", &subm_timeout);

        BindData("worker_node1", &worker_node1);
        BindData("worker_node2", &worker_node2);
        BindData("worker_node3", &worker_node3);
        BindData("worker_node4", &worker_node4);
        BindData("worker_node5", &worker_node5);

        BindData("run_counter", &run_counter);
        BindData("ret_code",    &ret_code);

        BindData("input",  &input,  kNetScheduleMaxDataSize);
        BindData("output", &output, kNetScheduleMaxDataSize);

        BindData("err_msg", &err_msg, kNetScheduleMaxErrSize);

        BindData("cout",  &cout, kNetScheduleMaxDataSize);
        BindData("cerr",  &cerr, kNetScheduleMaxDataSize);
    }
};

/// Queue watcher description
///
/// @internal
///
struct SQueueListener
{
    unsigned int   host;         ///< host name (network BO)
    unsigned short port;         ///< Listening UDP port
    time_t         last_connect; ///< Last registration timestamp
    int            timeout;      ///< Notification expiration timeout

    SQueueListener(unsigned int   host_addr,
                   unsigned short port_number,
                   time_t         curr,
                   int            expiration_timeout)
    : host(host_addr),
      port(port_number),
      last_connect(curr),
      timeout(expiration_timeout)
    {}
};

/// Mutex protected Queue database with job status FSM
///
/// @internal
///
struct SLockedQueue
{
    SQueueDB                        db;               ///< Database
    auto_ptr<CBDB_FileCursor>       cur;              ///< DB cursor
    CFastMutex                      lock;             ///< db, cursor lock
    CNetScheduler_JobStatusTracker  status_tracker;   ///< status FSA

    // queue parameters
    int                             timeout;       ///< Result exp. timeout
    int                             notif_timeout; ///< Notification interval

    // List of active worker node listeners waiting for pending jobs

    typedef vector<SQueueListener*> TListenerList;

    TListenerList                wnodes;       ///< worker node listeners
    time_t                       last_notif;   ///< last notification time
    CRWLock                      wn_lock;      ///< wnodes locker
    string                       q_notif;      ///< Queue notification message

    // Timeline object to control job execution timeout
    CJobTimeLine*                run_time_line;
    int                          run_timeout;
    CRWLock                      rtl_lock;      ///< run_time_line locker

    // datagram notification socket 
    // (used to notify worker nodes and waiting clients)
    CDatagramSocket              udp_socket;    ///< UDP notification socket
    CFastMutex                   us_lock;       ///< UDP socket lock

    SLockedQueue(const string& queue_name) 
        : timeout(3600), 
          notif_timeout(7), 
          last_notif(0), 
          q_notif("NCBI_JSQ_"),
          run_time_line(0)
    {
        _ASSERT(!queue_name.empty());
        q_notif.append(queue_name);
    }

    ~SLockedQueue()
    {
        NON_CONST_ITERATE(TListenerList, it, wnodes) {
            SQueueListener* node = *it;
            delete node;
        }
        delete run_time_line;
    }
};

/// Queue database manager
///
/// @internal
///
class CQueueCollection
{
public:
    typedef map<string, SLockedQueue*> TQueueMap;

public:
    CQueueCollection();
    ~CQueueCollection();

    void Close();

    SLockedQueue& GetLockedQueue(const string& qname);
    bool QueueExists(const string& qname) const;

    /// Collection takes ownership of queue
    void AddQueue(const string& name, SLockedQueue* queue);

    const TQueueMap& GetMap() const { return m_QMap; }

private:
    CQueueCollection(const CQueueCollection&);
    CQueueCollection& operator=(const CQueueCollection&);
private:
    TQueueMap      m_QMap;
};



/// Top level queue database.
/// (Thread-Safe, syncronized.)
///
/// @internal
///
class CQueueDataBase
{
public:
    CQueueDataBase();
    ~CQueueDataBase();

    void Open(const string& path, unsigned cache_ram_size);
    void MountQueue(const string& queue_name,
                    int           timeout,
                    int           notif_timeout,
                    int           run_timeout,
                    int           run_timeout_precision);
    void Close();
    bool QueueExists(const string& qname) const 
                { return m_QueueCollection.QueueExists(qname); }

    /// Remove old jobs
    void Purge();
    void StopPurge();
    void RunPurgeThread();
    void StopPurgeThread();

    /// Notify all listeners
    void NotifyListeners();
    void RunNotifThread();
    void StopNotifThread();

    void CheckExecutionTimeout();
    void RunExecutionWatcherThread(unsigned run_delay);
    void StopExecutionWatcherThread();


    void SetUdpPort(unsigned short port) { m_UdpPort = port; }
    unsigned short GetUdpPort() const { return m_UdpPort; }

    /// Main queue entry point
    ///
    /// @internal
    ///
    class CQueue
    {
    public:
        CQueue(CQueueDataBase& db, const string& queue_name);

        unsigned int Submit(const string& input,
                            unsigned      host_addr = 0,
                            unsigned      port = 0,
                            unsigned      wait_timeout = 0);

        void Cancel(unsigned int job_id);
        void PutResult(unsigned int  job_id,
                       int           ret_code,
                       const char*   output);
        
        void JobFailed(unsigned int  job_id,
                       const string& err_msg);

        void GetJob(unsigned int   worker_node,
                    unsigned int*  job_id, 
                    char*          input);
        // Get job and generate key
        void GetJob(char* key_buf, 
                    unsigned int   worker_node,
                    unsigned int*  job_id, 
                    char*          input,
                    const string&  host,
                    unsigned       port
                    );
        void ReturnJob(unsigned int job_id);

        // Get output info for compeleted job
        bool GetOutput(unsigned int job_id,
                       int*         ret_code,
                       char*        output);

        // Get output for failed job
        bool GetErrMsg(unsigned int job_id,
                       char*        err_msg);

        CNetScheduleClient::EJobStatus 
        GetStatus(unsigned int job_id) const;


        /// Set job run-expiration timeout
        /// @param tm
        ///    Time worker node needs to execute the job (in seconds)
        void SetJobRunTimeout(unsigned job_id, unsigned tm);

        /// Delete if job is done and timeout expired
        ///
        /// @return TRUE if job has been deleted
        ///
        bool CheckDelete(unsigned int job_id);

        /// Delete batch_size jobs
        /// @return
        ///    Number of deleted jobs
        unsigned CheckDeleteBatch(unsigned batch_size,
                                  CNetScheduleClient::EJobStatus status);

        /// Remove all jobs
        void Truncate();

        /// Remove job from the queue
        void DropJob(unsigned job_id);

        /// Free unsued memory (status storage)
        void FreeUnusedMem() { m_LQueue.status_tracker.FreeUnusedMem(); }

        /// All returned jobs come back to pending status
        void Return2Pending() { m_LQueue.status_tracker.Return2Pending(); }

        /// @param host_addr
        ///    host address in network BO
        ///
        void RegisterNotificationListener(unsigned int    host_addr, 
                                          unsigned short  port,
                                          int             timeout);

        /// UDP notification to all listeners
        void NotifyListeners();

        /// Check execution timeout.
        /// All jobs failed to execute, go back to pending
        void CheckExecutionTimeout();

        /// Check job expiration
        /// Returns new time if job not yet expired (or ended)
        /// 0 means job ended in any way.
        time_t CheckExecutionTimeout(unsigned job_id, time_t curr_time);

    private:
        CBDB_FileCursor* GetCursor(CBDB_Transaction& trans);

        void RemoveFromTimeLine(unsigned job_id);

        time_t x_ComputeExpirationTime(unsigned time_run, 
                                       unsigned run_timeout) const; 

    private:
        CQueue(const CQueue&);
        CQueue& operator=(const CQueue&);

    private:
        CQueueDataBase& m_Db;      ///< Parent structure reference
        SLockedQueue&   m_LQueue;  
    };



protected:
    /// get next job id (counter increment)
    unsigned int GetNextId();

    friend class CQueue;
private:
    CBDB_Env*                       m_Env;
    string                          m_Path;
    string                          m_Name;

    CQueueCollection                m_QueueCollection;

    unsigned int                    m_MaxId;   ///< job id counter
    CFastMutex                      m_IdLock;

    bm::bvector<>                   m_UsedIds; /// id access locker
    CRef<CJobQueueCleanerThread>    m_PurgeThread;

    bool                 m_StopPurge;         ///< Purge stop flag
    CFastMutex           m_PurgeLock;
    unsigned int         m_PurgeLastId;       ///< m_MaxId at last Purge
    unsigned int         m_PurgeSkipCnt;      ///< Number of purge skipped
    unsigned int         m_DeleteChkPointCnt; ///< trans. checkpnt counter
    unsigned int         m_FreeStatusMemCnt;  ///< Free memory counter
    time_t               m_LastR2P;           ///< Return 2 Pending timestamp
    unsigned short       m_UdpPort;           ///< UDP notification port      

    CRef<CJobNotificationThread>             m_NotifThread;
    CRef<CJobQueueExecutionWatcherThread>    m_ExeWatchThread;
};



END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.13  2005/03/17 20:37:07  kuznets
 * Implemented FPUT
 *
 * Revision 1.12  2005/03/15 20:14:30  kuznets
 * Implemented notification to client waiting for job
 *
 * Revision 1.11  2005/03/15 14:52:39  kuznets
 * Better datagram socket management, DropJob implemenetation
 *
 * Revision 1.10  2005/03/10 14:19:57  kuznets
 * Implemented individual run timeouts
 *
 * Revision 1.9  2005/03/09 17:37:17  kuznets
 * Added node notification thread and execution control timeline
 *
 * Revision 1.8  2005/03/04 12:06:41  kuznets
 * Implenyed UDP callback to clients
 *
 * Revision 1.7  2005/02/28 12:24:17  kuznets
 * New job status Returned, better error processing and queue management
 *
 * Revision 1.6  2005/02/23 19:16:38  kuznets
 * Implemented garbage collection thread
 *
 * Revision 1.5  2005/02/22 16:13:00  kuznets
 * Performance optimization
 *
 * Revision 1.4  2005/02/14 17:57:41  kuznets
 * Fixed a bug in queue procesing
 *
 * Revision 1.3  2005/02/10 19:58:45  kuznets
 * +GetOutput()
 *
 * Revision 1.2  2005/02/09 18:56:08  kuznets
 * private->public fix
 *
 * Revision 1.1  2005/02/08 16:42:55  kuznets
 * Initial revision
 *
 *
 * ===========================================================================
 */

#endif /* NETSCHEDULE_BDB_QUEUE__HPP */

