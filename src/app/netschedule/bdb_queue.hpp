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
 * Authors:  Anatoliy Kuznetsov, Victor Joukov
 *
 * File Description:
 *   NetSchedule job status database.
 *
 */


/// @file bdb_queue.hpp
/// NetSchedule job status database. 
///
/// @internal


#include <corelib/ncbimtx.hpp>
#include <corelib/ncbicntr.hpp>

#include <connect/services/netschedule_api.hpp>
#include "job_status.hpp"
#include "queue_clean_thread.hpp"
#include "notif_thread.hpp"
#include "squeue.hpp"
#include "queue_vc.hpp"

BEGIN_NCBI_SCOPE


/// Batch submit record
///
/// @internal
///
struct SNS_SubmitRecord
{
    unsigned   job_id;
    char       input[kNetScheduleMaxDBDataSize];
    char       affinity_token[kNetScheduleMaxDBDataSize];
    unsigned   affinity_id;
    TNSTagList tags;
    unsigned   mask;

    SNS_SubmitRecord()
    {
        input[0] = affinity_token[0] = 0;
        affinity_id = job_id = mask = 0;
    }
};

class CQueueDataBase;

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

    /// Is the queue empty long enough to be deleted?
    bool IsExpired(void);

    unsigned int Submit(SNS_SubmitRecord* rec,
                        unsigned          host_addr = 0,
                        unsigned          port = 0,
                        unsigned          wait_timeout = 0,
                        const char*       progress_msg = 0);

    /// Submit job batch
    /// @return 
    ///    ID of the first job, second is first_id+1 etc.
    unsigned int SubmitBatch(vector<SNS_SubmitRecord>& batch,
                             unsigned      host_addr = 0,
                             unsigned      port = 0,
                             unsigned      wait_timeout = 0,
                             const char*   progress_msg = 0);

    void Cancel(unsigned int job_id);

    /// Move job to pending ignoring its current status
    void ForceReschedule(unsigned int job_id);

    void PutResult(unsigned int  job_id,
                   int           ret_code,
                   const char*   output);

    void PutResultGetJob(unsigned int   done_job_id,
                         int            ret_code,
                         const char*    output,
                         unsigned int   worker_node,
                         unsigned int*  job_id, 
                         char*          input,
                         const string&  client_name,
                         unsigned*      job_mask);


    void PutProgressMessage(unsigned int  job_id,
                            const char*   msg);
    
    void JobFailed(unsigned int  job_id,
                   const string& err_msg,
                   const string& output,
                   int           ret_code,
                   unsigned int  worker_node,
                   const string& client_name);

    void GetJobKey(char* key_buf, unsigned job_id,
                   const string& host, unsigned port);


    void GetJob(unsigned int   worker_node,
                unsigned int*  job_id, 
                char*          input,
                const string&  client_name,
                unsigned*      job_mask);

    void ReturnJob(unsigned int job_id);

    /// @param expected_status
    ///    If current status is different from expected try to
    ///    double read the database (to avoid races between nodes
    ///    and submitters)
    bool GetJobDescr(unsigned int job_id,
                     int*         ret_code,
                     char*        input,
                     char*        output,
                     char*        err_msg,
                     char*        progress_msg,
                     CNetScheduleAPI::EJobStatus expected_status 
                                        = CNetScheduleAPI::eJobNotFound);

    CNetScheduleAPI::EJobStatus 
    GetStatus(unsigned int job_id) const;

    /// count status snapshot for affinity token
    /// returns false if affinity token not found
    bool CountStatus(CJobStatusTracker::TStatusSummaryMap* status_map,
                     const char*                           affinity_token);


    /// Set job run-expiration timeout
    /// @param tm
    ///    Time worker node needs to execute the job (in seconds)
    void SetJobRunTimeout(unsigned job_id, unsigned tm);

    /// Prolong job expiration timeout
    /// @param tm
    ///    Time worker node needs to execute the job (in seconds)
    void JobDelayExpiration(unsigned job_id, unsigned tm);

    /// Delete if job is done and timeout expired
    ///
    /// @return TRUE if job has been deleted
    ///
    bool CheckDelete(unsigned int job_id);

    /// Delete batch_size jobs
    /// @return
    ///    Number of deleted jobs
    unsigned CheckDeleteBatch(unsigned batch_size,
                              CNetScheduleAPI::EJobStatus status);
    /// Actually delete batch jobs from jobs-to-delete vector
    /// @return
    ///    Number of deleted jobs
    unsigned DoDeleteBatch(unsigned batch_size);

    /// Delete all job ids already deleted (phisically) from the queue
    void ClearAffinityIdx(void);

    /// Remove all jobs
    void Truncate(void);

    /// Remove job from the queue
    void DropJob(unsigned job_id);

    /// @param host_addr
    ///    host address in network BO
    ///
    void RegisterNotificationListener(unsigned int   host_addr, 
                                      unsigned short udp_port,
                                      int            timeout,
                                      const string&  auth);
    void UnRegisterNotificationListener(unsigned int   host_addr, 
                                        unsigned short udp_port);

    /// Remove affinity association for a specified host
    ///
    /// @note Affinity is based on network host name and 
    /// program name (not UDP port). Presumed that we have one
    /// worker node instance per host.
    void ClearAffinity(unsigned int  host_addr,
                        const string& auth);

    void SetMonitorSocket(SOCK sock);

    /// Return monitor (no ownership transfer)
    CNetScheduleMonitor* GetMonitor(void);

    /// UDP notification to all listeners
    void NotifyListeners(bool unconditional=false);

    /// Check execution timeout.
    /// All jobs failed to execute, go back to pending
    void CheckExecutionTimeout(void);

    /// Check job expiration
    /// Returns new time if job not yet expired (or ended)
    /// 0 means job ended in any way.
    time_t CheckExecutionTimeout(unsigned job_id, time_t curr_time);

    unsigned CountStatus(CNetScheduleAPI::EJobStatus) const;

    void StatusStatistics(CNetScheduleAPI::EJobStatus status,
                          TNSBitVector::statistics* st) const;

    /// Count database records
    unsigned CountRecs(void);

    void PrintStat(CNcbiOstream & out);
    void PrintNodeStat(CNcbiOstream & out) const;
    void PrintSubmHosts(CNcbiOstream & out) const;
    void PrintWNodeHosts(CNcbiOstream & out) const;
    void PrintQueue(CNcbiOstream& out, 
                    CNetScheduleAPI::EJobStatus job_status,
                    const string& host,
                    unsigned      port);

    string ExecQuery(const string& query, const string& action,
                     const string& fields);
    typedef vector<vector<string> > TRecordSet;
    void x_ExecSelect(TRecordSet&         record_set,
                      SLockedQueue&       q,
                      const TNSBitVector* ids,
                      const string&       str_fields);
    void x_ExecSelectChunk(TRecordSet&           record_set,
                           SLockedQueue&         q,
                           const TNSBitVector*   ids,
                           const vector<int>&    field_nums,
                           const vector<string>* pos_to_tag);
    /// Queue dump
    void PrintJobDbStat(unsigned job_id, 
                        CNcbiOstream & out,
                        CNetScheduleAPI::EJobStatus status 
                                    = CNetScheduleAPI::eJobNotFound);
    /// Dump all job records
    void PrintAllJobDbStat(CNcbiOstream & out);

    void PrintJobStatusMatrix(CNcbiOstream & out);

    bool IsVersionControl() const;
    bool IsMatchingClient(const CQueueClientInfo& cinfo) const;
    /// Check if client is a configured submitter
    bool IsSubmitAllowed() const;
    /// Check if client is a configured worker node
    bool IsWorkerAllowed() const;

    typedef SLockedQueue::TStatEvent TStatEvent;
    double GetAverage(TStatEvent);

    // Service for CQueueDataBase
    /// Free unsued memory (status storage)
    void FreeUnusedMem(void)
        { x_GetLQueue()->status_tracker.FreeUnusedMem(); }

    /// All returned jobs come back to pending status
    void Returned2Pending(void)
        { x_GetLQueue()->status_tracker.Returned2Pending(); }

private:
    // Transitional - for support of CQueue iterators
    friend class CQueueIterator;
    CQueue(const CQueueDataBase& db) :
        m_Db(const_cast<CQueueDataBase&>(db)), // HACK! - legitimate here,
                                               // because iterator access is
                                               // const, and this constructor
                                               // is for CQueueIterator only
        m_ClientHostAddr(0),
        m_QueueDbAccessCounter(0) {}

    void x_Assume(CRef<SLockedQueue> slq) { m_LQueue = slq; }

    /// Find the listener if it is registered
    /// @return NULL if not found
    ///
    SLockedQueue::TListenerList::iterator 
    x_FindListener(unsigned int host_addr, unsigned short udp_port);

    /// Find the pending job.
    /// This method takes into account jobs available
    /// in the job status matrix and current 
    /// worker node affinity association
    ///
    /// Sync.Locks: affinity map lock, main queue lock, status matrix
    ///
    /// @return job_id
    unsigned 
    x_FindPendingJob(const string&  client_name,
                     unsigned       client_addr);

    /// Get job with load balancing
    void x_GetJobLB(unsigned int   worker_node,
                    unsigned int*  job_id, 
                    char*          input,
                    unsigned*      job_mask);

    time_t x_ComputeExpirationTime(unsigned time_run, 
                                   unsigned run_timeout) const; 


    /// db should be already positioned
    void x_PrintJobDbStat(SQueueDB&     db, 
                          CNcbiOstream& out,
                          const char*   fld_separator = "\n",
                          bool          print_fname = true);

    void x_PrintShortJobDbStat(SQueueDB&     db, 
                               const string& host,
                               unsigned port,
                               CNcbiOstream& out,
                               const char*   fld_separator = "\t");

    /// Delete record using positioned cursor, dumps content
    /// to text file if necessary
    void x_DeleteDBRec(SQueueDB& db, 
                       CBDB_FileCursor& cur);

    void x_AssignSubmitRec(SQueueDB&     db,
                           const SNS_SubmitRecord* rec,
                           time_t        time_submit,
                           unsigned      host_addr,
                           unsigned      port,
                           unsigned      wait_timeout,
                           const char*   progress_msg);

    void x_AssignJobInfoRec(SJobInfoDB& db, const SNS_SubmitRecord* rec);

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
                                char*                input,
                                unsigned*            aff_id,
                                unsigned*            job_mask);

    SQueueDB*        x_GetLocalDb();
    CBDB_FileCursor* x_GetLocalCursor(CBDB_Transaction& trans);

    /// Update the affinity index (no locking)
    void x_AddToAffIdx_NoLock(unsigned aff_id, 
                              unsigned job_id_from,
                              unsigned job_id_to = 0);

    void x_AddToAffIdx_NoLock(const vector<SNS_SubmitRecord>& batch);

    /// Read queue affinity index, retrieve all jobs, with
    /// given set of affinity ids
    ///
    /// @param aff_id_set
    ///     set of affinity ids to read
    /// @param job_candidates
    ///     OUT set of jobs associated with specified affinities
    ///
    void x_ReadAffIdx_NoLock(const TNSBitVector& aff_id_set,
                             TNSBitVector*       job_candidates);

    void x_ReadAffIdx_NoLock(unsigned            aff_id,
                             TNSBitVector*       job_candidates);

    void x_Count(TStatEvent event)
        { x_GetLQueue()->CountEvent(event); }

    CRef<SLockedQueue> x_GetLQueue(void);
    const CRef<SLockedQueue> x_GetLQueue(void) const;

    void x_AddToTimeLine(unsigned job_id, time_t curr);
    void x_RemoveFromTimeLine(unsigned job_id);
    void x_TimeLineExchange(unsigned remove_job_id, 
                            unsigned add_job_id,
                            time_t   curr);

private:
    CQueue(const CQueue&);
    CQueue& operator=(const CQueue&);

private:
    // Per queue data
    CQueueDataBase&    m_Db;      ///< Parent structure reference
    mutable CFastMutex m_LQueueLock;
    CWeakRef<SLockedQueue> m_LQueue;
    unsigned           m_ClientHostAddr;

    // Per client data
    /// For private DB
    unsigned        m_QueueDbAccessCounter;
    /// Private database (for long running sessions)
    auto_ptr<SQueueDB>         m_QueueDB;
    /// Private cursor
    auto_ptr<CBDB_FileCursor>  m_QueueDB_Cursor;

}; // CQueue


/// Queue database manager
///
/// @internal
///
class CQueueIterator;
class CQueueCollection
{
public:
    typedef map<string, CRef<SLockedQueue> > TQueueMap;
    typedef CQueueIterator iterator;
    typedef CQueueIterator const_iterator; // HACK

public:
    CQueueCollection(const CQueueDataBase& db);
    ~CQueueCollection();

    void Close();

    CRef<SLockedQueue> GetLockedQueue(const string& qname) const;
    bool QueueExists(const string& qname) const;

    /// Collection takes ownership of queue
    SLockedQueue& AddQueue(const string& name, SLockedQueue* queue);
    // Remove queue from collection, notifying every interested party
    // through week reference mechanism.
    bool RemoveQueue(const string& name);

    CQueueIterator begin() const;
    CQueueIterator end() const;

    size_t GetSize() const { return m_QMap.size(); }

private:
    CQueueCollection(const CQueueCollection&);
    CQueueCollection& operator=(const CQueueCollection&);
private:
    friend class CQueueIterator;
    const CQueueDataBase&  m_QueueDataBase;
    TQueueMap              m_QMap;
    mutable CRWLock        m_Lock;
};



class CQueueIterator
{
public:
    CQueueIterator(const CQueueDataBase& db,
        CQueueCollection::TQueueMap::const_iterator iter, CRWLock* lock);
    // MSVC8 (Studio 2005) can not (or does not want to) perform
    // copy constructor optimization on return, thus it needs
    // explicit copy constructor because CQueue does not have it.
    CQueueIterator(const CQueueIterator& rhs);
    ~CQueueIterator();
    CQueue& operator*();
    const string GetName();
    void operator++();
    bool operator==(const CQueueIterator& rhs) {
        return m_Iter == rhs.m_Iter;
    }
    bool operator!=(const CQueueIterator& rhs) {
        return m_Iter != rhs.m_Iter;
    }
private:
    const CQueueDataBase&                       m_QueueDataBase;
    CQueueCollection::TQueueMap::const_iterator m_Iter;
    CQueue                                      m_Queue;
    mutable CRWLock*                            m_Lock;
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
    /// @param max_trans
    ///    Maximum number of active transactions
    void Open(const string& path, 
              unsigned      cache_ram_size,
              unsigned      max_locks,
              unsigned      log_mem_size,
              unsigned      max_trans);

    void Configure(const IRegistry& reg, unsigned* min_run_timeout);

    typedef SLockedQueue::TQueueKind TQueueKind;
    void MountQueue(const string& qname,
                    const string& qclass,
                    TQueueKind    kind,
                    const SQueueParameters& params);

    void CreateQueue(const string& qname, const string& qclass,
                     const string& comment = "");
    void DeleteQueue(const string& qname);
    void QueueInfo(const string& qname, int& kind,
                   string* qclass, string* comment);
    void GetQueueNames(string *list, const string& sep) const;

    void UpdateQueueParameters(const string& qname,
                               const SQueueParameters& params);
    void UpdateQueueLBParameters(const string& qname,
                                 const SQueueParameters& params);
    void Close(void);
    bool QueueExists(const string& qname) const 
                { return m_QueueCollection.QueueExists(qname); }

    /// Remove old jobs
    void Purge(void);
    void StopPurge(void);
    void RunPurgeThread(void);
    void StopPurgeThread(void);

    /// Notify all listeners
    void NotifyListeners(void);
    void RunNotifThread(void);
    void StopNotifThread(void);

    void CheckExecutionTimeout(void);
    void RunExecutionWatcherThread(unsigned run_delay);
    void StopExecutionWatcherThread(void);


    void SetUdpPort(unsigned short port) { m_UdpPort = port; }
    unsigned short GetUdpPort(void) const { return m_UdpPort; }

    /// Force transaction checkpoint
    void TransactionCheckPoint();

protected:
    /// get next job id (counter increment)
    unsigned int GetNextId();

    /// Returns first id for the batch
    unsigned int GetNextIdBatch(unsigned count);

private:
    friend class CQueue;
    unsigned x_PurgeUnconditional(unsigned batch_size);
    void x_Returned2Pending(void);
    void x_OptimizeStatusMatrix(void);
    void x_OptimizeAffinity(void);
    bool x_CheckStopPurge(void);
    void x_CleanParamMap(void);

    CBDB_Env*                       m_Env;
    string                          m_Path;
    string                          m_Name;

    mutable CFastMutex              m_ConfigureLock;
    typedef map<string, SQueueParameters*> TQueueParamMap;
    TQueueParamMap                  m_QueueParamMap;
    SQueueDescriptionDB             m_QueueDescriptionDB;
    CQueueCollection                m_QueueCollection;

    TNSBitVector                    m_UsedIds; /// id access locker
    CRef<CJobQueueCleanerThread>    m_PurgeThread;

    bool                 m_StopPurge;         ///< Purge stop flag
    CFastMutex           m_PurgeLock;
    unsigned int         m_DeleteChkPointCnt; ///< trans. checkpnt counter
    unsigned int         m_FreeStatusMemCnt;  ///< Free memory counter
    time_t               m_LastFreeMem;       ///< time of the last memory opt
    time_t               m_LastR2P;           ///< Return 2 Pending timestamp
    unsigned short       m_UdpPort;           ///< UDP notification port      

    CRef<CJobNotificationThread>             m_NotifThread;
    CRef<CJobQueueExecutionWatcherThread>    m_ExeWatchThread;

}; // CQueueDataBase


END_NCBI_SCOPE

#endif /* NETSCHEDULE_BDB_QUEUE__HPP */
