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
#include <corelib/ncbicntr.hpp>

#include <connect/services/netschedule_client.hpp>

#include <bdb/bdb_file.hpp>
#include <bdb/bdb_env.hpp>
#include <bdb/bdb_cursor.hpp>
#include <map>
#include <vector>

#include <util/logrotate.hpp>

#include "job_status.hpp"
#include "queue_clean_thread.hpp"
#include "notif_thread.hpp"
#include "job_time_line.hpp"
#include "queue_vc.hpp"
#include "queue_monitor.hpp"
#include "access_list.hpp"
#include "nslb.hpp"

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

    CBDB_FieldUint4        worker_node1;    ///< IP of wnode 1 (netw BO)
    CBDB_FieldUint4        worker_node2;    ///<       wnode 2
    CBDB_FieldUint4        worker_node3;
    CBDB_FieldUint4        worker_node4;
    CBDB_FieldUint4        worker_node5;

    CBDB_FieldUint4        run_counter;     ///< Number of execution attempts
    CBDB_FieldInt4         ret_code;        ///< Return code

    CBDB_FieldUint4        time_lb_first_eval;  ///< First LB evaluation time

    CBDB_FieldString       input;           ///< Input data
    CBDB_FieldString       output;          ///< Result data

    CBDB_FieldString       err_msg;         ///< Error message (exception::what())
    CBDB_FieldString       progress_msg;    ///< Progress report message


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

        BindData("run_counter",        &run_counter);
        BindData("ret_code",           &ret_code);
        BindData("time_lb_first_eval", &time_lb_first_eval);

        BindData("input",  &input,  kNetScheduleMaxDataSize);
        BindData("output", &output, kNetScheduleMaxDataSize);

        BindData("err_msg", &err_msg, kNetScheduleMaxErrSize);
        BindData("progress_msg", &progress_msg, kNetScheduleMaxDataSize);

        BindData("cout",  &cout, kNetScheduleMaxDataSize);
        BindData("cerr",  &cerr, kNetScheduleMaxDataSize);
    }
};

/// Types of event notification subscribers
///
/// @internal
enum ENetScheduleListenerType
{
    eNS_Worker        = (1 << 0),  ///< Regular worker node
    eNS_BackupWorker  = (1 << 1)   ///< Backup worker node (second priority notifications)
};


/// @internal
typedef unsigned TNetScheduleListenerType;


/// Queue watcher (client) description. Client is predominantly a worker node
/// waiting for new jobs.
///
/// @internal
///
struct SQueueListener
{
    unsigned int               host;         ///< host name (network BO)
    unsigned short             udp_port;     ///< Listening UDP port
    time_t                     last_connect; ///< Last registration timestamp
    int                        timeout;      ///< Notification expiration timeout
    string                     auth;         ///< Authentication string
    TNetScheduleListenerType   client_type;  ///< Client type mask

    SQueueListener(unsigned int             host_addr,
                   unsigned short           udp_port_number,
                   time_t                   curr,
                   int                      expiration_timeout,
                   const string&            client_auth,
                   TNetScheduleListenerType ctype = eNS_Worker)
    : host(host_addr),
      udp_port(udp_port_number),
      last_connect(curr),
      timeout(expiration_timeout),
      auth(client_auth),
      client_type(ctype)
    {}
};

/// Runtime queue statistics
///
/// @internal
struct SQueueStatictics
{
    double   total_run_time; ///< Accumulated job running time
    unsigned run_count;      ///< Number of runs

    double   total_turn_around_time; ///< time from subm to completion

    SQueueStatictics() 
        : total_run_time(0.0), run_count(0), total_turn_around_time(0)
    {}
};

/// Batch submit record
///
/// @internal
struct SNS_BatchSubmitRec
{
    char input[kNetScheduleMaxDataSize];
};



/// @internal
enum ENSLB_RunDelayType
{
    eNSLB_Constant,   ///< Constant delay
    eNSLB_RunTimeAvg  ///< Running time based delay (averaged)
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

    /// Client program version control
    CQueueClientInfoList         program_version_list;

    /// Host access list for job submission
    CNetSchedule_AccessList      subm_hosts;
    /// Host access list for job execution (workers)
    CNetSchedule_AccessList      wnode_hosts;


    /// Queue monitor
    CNetScheduleMonitor          monitor;

    /// Database records when they are deleted can be dumped to an archive
    /// file for further processing
    CRotatingLogStream           rec_dump;
    CFastMutex                   rec_dump_lock;
    bool                         rec_dump_flag;

    mutable bool                 lb_flag;  ///< Load balancing flag
    CNSLB_Coordinator*           lb_coordinator;

    ENSLB_RunDelayType           lb_stall_delay_type;
    unsigned                     lb_stall_time;      ///< job delay (seconds)
    double                       lb_stall_time_mult; ///< stall coeff
    CFastMutex                   lb_stall_time_lock;

    SQueueStatictics             qstat;
    CFastMutex                   qstat_lock;

    SLockedQueue(const string& queue_name) 
        : timeout(3600), 
          notif_timeout(7), 
          last_notif(0), 
          q_notif("NCBI_JSQ_"),
          run_time_line(0),

          rec_dump("jsqd_"+queue_name+".dump", 10 * (1024 * 1024)),
          rec_dump_flag(false),
          lb_flag(false),
          lb_coordinator(0),
          lb_stall_delay_type(eNSLB_Constant),
          lb_stall_time(6),
          lb_stall_time_mult(1.0)
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
        delete lb_coordinator;
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
    TQueueMap              m_QMap;
    mutable CRWLock        m_Lock;
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

    /// @param path
    ///    Path to the database
    /// @param cache_ram_size
    ///    Size of database cache
    /// @param max_locks
    ///    Number of locks and lock objects
    /// @param log_mem_size
    ///    Size of in-memory LOG (when 0 log is put to disk)
    ///    In memory LOG is not durable, put it to memory
    ///    only if you need better performance 
    void Open(const string& path, 
              unsigned      cache_ram_size,
              unsigned      max_locks,
              unsigned      log_mem_size);

    void ReadConfig(const IRegistry& reg, unsigned* min_run_timeout);

    void MountQueue(const string& queue_name,
                    int           timeout,
                    int           notif_timeout,
                    int           run_timeout,
                    int           run_timeout_precision,
                    const string& program_name);
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
        /// @param client_host_addr - 0 means internal use
        CQueue(CQueueDataBase& db, 
               const string&   queue_name,
               unsigned        client_host_addr);

        unsigned int Submit(const char*   input,
                            unsigned      host_addr = 0,
                            unsigned      port = 0,
                            unsigned      wait_timeout = 0,
                            const char*   progress_msg = 0);

        /// Submit job batch
        /// @return 
        ///    ID of the first job, second is first_id+1
        unsigned int SubmitBatch(vector<SNS_BatchSubmitRec> & batch,
                                 unsigned      host_addr = 0,
                                 unsigned      port = 0,
                                 unsigned      wait_timeout = 0,
                                 const char*   progress_msg = 0);

        void Cancel(unsigned int job_id);
        void PutResult(unsigned int  job_id,
                       int           ret_code,
                       const char*   output);

        void PutResultGetJob(unsigned int   done_job_id,
                             int            ret_code,
                             const char*    output,
                             char*          key_buf,
                             unsigned int   worker_node,
                             unsigned int*  job_id, 
                             char*          input,
                             const string&  host,
                             unsigned       port);


        void PutProgressMessage(unsigned int  job_id,
                                const char*   msg);
        
        void JobFailed(unsigned int  job_id,
                       const string& err_msg);

        /// Get job with load balancing
        void GetJobLB(unsigned int   worker_node,
                      unsigned int*  job_id, 
                      char*          input);

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


        bool GetJobDescr(unsigned int job_id,
                         int*         ret_code,
                         char*        input,
                         char*        output,
                         char*        err_msg,
                         char*        progress_msg);

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
                                          unsigned short  udp_port,
                                          int             timeout,
                                          const string&   auth);

        void SetMonitorSocket(SOCK sock);

        /// Return monitor (no ownership transfer)
        CNetScheduleMonitor* GetMonitor() { return &m_LQueue.monitor; }

        /// UDP notification to all listeners
        void NotifyListeners();

        /// Check execution timeout.
        /// All jobs failed to execute, go back to pending
        void CheckExecutionTimeout();

        /// Check job expiration
        /// Returns new time if job not yet expired (or ended)
        /// 0 means job ended in any way.
        time_t CheckExecutionTimeout(unsigned job_id, time_t curr_time);

        unsigned CountStatus(CNetScheduleClient::EJobStatus) const;

        /// Count database records
        unsigned CountRecs();

        void PrintStat(CNcbiOstream & out);
        void PrintNodeStat(CNcbiOstream & out) const;
        void PrintSubmHosts(CNcbiOstream & out) const;
        void PrintWNodeHosts(CNcbiOstream & out) const;

        void PrintJobDbStat(unsigned job_id, CNcbiOstream & out);
        /// Dump all job records
        void PrintAllJobDbStat(CNcbiOstream & out);

        void PrintJobStatusMatrix(CNcbiOstream & out);

        bool IsVersionControl() const
        {
            return m_LQueue.program_version_list.IsConfigured();
        }

        bool IsMatchingClient(const CQueueClientInfo& cinfo) const
        {
            return m_LQueue.program_version_list.IsMatchingClient(cinfo);
        }

        /// Check if client is a configured submitter
        bool IsSubmitAllowed() const
        {
            return (m_ClientHostAddr == 0) || 
                   m_LQueue.subm_hosts.IsAllowed(m_ClientHostAddr);
        }

        /// Check if client is a configured worker node
        bool IsWorkerAllowed() const
        {
            return (m_ClientHostAddr == 0) || 
                   m_LQueue.wnode_hosts.IsAllowed(m_ClientHostAddr);
        }

    private:
        CBDB_FileCursor* GetCursor(CBDB_Transaction& trans);

        void RemoveFromTimeLine(unsigned job_id);

        time_t x_ComputeExpirationTime(unsigned time_run, 
                                       unsigned run_timeout) const; 


        /// db should be already positioned
        void x_PrintJobDbStat(SQueueDB&     db, 
                              CNcbiOstream& out,
                              const char*   fld_separator = "\n",
                              bool          print_fname = true);

        /// Delete record using positioned cursor, dumps content
        /// to text file if necessary
        void x_DeleteDBRec(SQueueDB&  db, 
                           CBDB_FileCursor& cur);

        void x_AssignSubmitRec(unsigned      job_id,
                               const char*   input,
                               unsigned      host_addr,
                               unsigned      port,
                               unsigned      wait_timeout,
                               const char*   progress_msg);

        /// Info on how to notify job submitter
        struct SSubmitNotifInfo
        {
            unsigned subm_addr;
            unsigned subm_port;
            unsigned subm_timeout;
            unsigned time_submit;
            unsigned time_run;
        };

        /// @return TRUE if job record has been found and updated
        bool x_UpdateDB_PutResultNoLock(SQueueDB&            db,
                                        time_t               curr,
                                        CBDB_FileCursor&     cur,
                                        unsigned             job_id,
                                        int                  ret_code,
                                        const char*          output,
                                        SSubmitNotifInfo*    subm_info);


        enum EGetJobUpdateStatus
        {
            eGetJobUpdate_Ok,
            eGetJobUpdate_NotFound,
            eGetJobUpdate_JobStopped,  
            eGetJobUpdate_JobFailed  
        };
        EGetJobUpdateStatus x_UpdateDB_GetJobNoLock(
                                    SQueueDB&            db,
                                    time_t               curr,
                                    CBDB_FileCursor&     cur,
                                    CBDB_Transaction&    trans,
                                    unsigned int         worker_node,
                                    unsigned             job_id,
                                    char*                input);

    private:
        CQueue(const CQueue&);
        CQueue& operator=(const CQueue&);

    private:
        CQueueDataBase& m_Db;      ///< Parent structure reference
        SLockedQueue&   m_LQueue;  
        unsigned        m_ClientHostAddr;
    };

    const CQueueCollection& GetQueueCollection() const 
    { 
        return m_QueueCollection; 
    }

    /// Force transaction checkpoint
    void TransactionCheckPoint();


protected:
    /// get next job id (counter increment)
    unsigned int GetNextId();

    /// Returns first id for the batch
    unsigned int GetNextIdBatch(unsigned count);

    friend class CQueue;
private:
    CBDB_Env*                       m_Env;
    string                          m_Path;
    string                          m_Name;

    CQueueCollection                m_QueueCollection;

    CAtomicCounter                  m_IdCounter;

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
 * Revision 1.33  2005/08/22 14:01:58  kuznets
 * Added JobExchange command
 *
 * Revision 1.32  2005/08/18 16:24:32  kuznets
 * Optimized job retrival out o the bit matrix
 *
 * Revision 1.31  2005/08/15 13:29:50  kuznets
 * Implemented batch job submission
 *
 * Revision 1.30  2005/07/25 16:14:31  kuznets
 * Revisited LB parameters, added options to compute job stall delay as fraction of AVG runtime
 *
 * Revision 1.29  2005/07/14 13:12:56  kuznets
 * Added load balancer
 *
 * Revision 1.28  2005/06/21 16:00:22  kuznets
 * Added archival dump of all deleted records
 *
 * Revision 1.27  2005/06/20 13:31:08  kuznets
 * Added access control for job submitters and worker nodes
 *
 * Revision 1.26  2005/05/16 16:21:26  kuznets
 * Added available queues listing
 *
 * Revision 1.25  2005/05/12 18:37:33  kuznets
 * Implemented config reload
 *
 * Revision 1.24  2005/05/04 19:09:43  kuznets
 * Added queue dumping
 *
 * Revision 1.23  2005/05/02 14:44:40  kuznets
 * Implemented remote monitoring
 *
 * Revision 1.22  2005/04/28 17:40:26  kuznets
 * Added functions to rack down forgotten nodes
 *
 * Revision 1.21  2005/04/20 15:59:33  kuznets
 * Progress message to Submit
 *
 * Revision 1.20  2005/04/19 19:34:05  kuznets
 * Adde progress report messages
 *
 * Revision 1.19  2005/04/11 13:53:25  kuznets
 * Code cleanup
 *
 * Revision 1.18  2005/04/06 12:39:55  kuznets
 * + client version control
 *
 * Revision 1.17  2005/03/29 16:48:28  kuznets
 * Use atomic counter for job id assignment
 *
 * Revision 1.16  2005/03/22 19:02:54  kuznets
 * Reflecting chnages in connect layout
 *
 * Revision 1.15  2005/03/22 16:14:49  kuznets
 * +PrintStat()
 *
 * Revision 1.14  2005/03/21 13:07:28  kuznets
 * Added some statistical functions
 *
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

