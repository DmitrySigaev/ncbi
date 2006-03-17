#ifndef NETSCHEDULE_JOB_STATUS__HPP
#define NETSCHEDULE_JOB_STATUS__HPP

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
 *   Net schedule job status states.
 *
 */

/// @file job_status.hpp
/// NetSchedule job status tracker. 
///
/// @internal

#include <util/bitset/ncbi_bitset.hpp>
#include <corelib/ncbimtx.hpp>

BEGIN_NCBI_SCOPE


/// In-Memory storage to track status of all jobs
/// Syncronized thread safe class
///
/// @internal
class CNetScheduler_JobStatusTracker
{
public:
    typedef bm::bvector<>     TBVector;
    typedef vector<TBVector*> TStatusStorage;

public:
    CNetScheduler_JobStatusTracker();
    ~CNetScheduler_JobStatusTracker();

    CNetScheduleClient::EJobStatus 
        GetStatus(unsigned int job_id) const;

    /// Set job status. (Controls status change logic)
    ///
    ///  Status switching rules ([] - means request ignored:
    ///  ePending   <- (eReturned, eRunning, "no status");
    ///  eRunning   <- (ePending, eReturned, [eCanceled])
    ///  eReturned  <- (eRunning, [eCanceled, eFailed])
    ///  eCanceled  <- (ePending, eRunning, eReturned) (ignored if job is ready)
    ///  eFailed    <- (eRunning, eReturned, [eCanceled])
    ///  eDone      <- (eRunning, eReturned, [eCanceled, eFailed])
    ///
    /// @return old status. Job may be already canceled (or failed)
    /// in this case status change is ignored (
    /// 
    CNetScheduleClient::EJobStatus
    ChangeStatus(unsigned int                   job_id, 
                 CNetScheduleClient::EJobStatus status);

    /// Add closed interval of ids to pending status
    void AddPendingBatch(unsigned job_id_from, unsigned job_id_to);

    /// Return job id (job is moved from pending to running)
    /// @return job id, 0 - no pending jobs
    unsigned GetPendingJob();

    /// Get pending job out of a certain candidate set
    /// Method cleans candidates if they are no longer available 
    /// for scheduling (it means job was already dispatched)
    /// 
    /// @param done_job_id
    ///     job id going from running to done status. Ignored if 0.
    /// @param need_db_update
    ///     (out) TRUE if database needs to be updated (for done job id). 
    ///     Update is not needed if database 
    /// @param job_id 
    ///     OUT job id (moved from pending to running)
    ///     job_id is taken out of candidates 
    ///     When 0 - means no pending candidates
    /// @return true - if there are any available pending jobs
    ///         false - queue has no pending jobs whatsoever 
    ///         (not just candidates)
    bool PutDone_GetPending(unsigned int done_job_id,
                            bool*        need_db_update,
                            bm::bvector<>* candidate_set,
                            unsigned*      job_id);

    /// Get any pending job, but it should NOT be in the unwanted list
    /// Presumed, that unwanted jobs are speculatively assigned to other
    /// worker nodes or postponed
    bool PutDone_GetPending(unsigned int         done_job_id,
                            bool*                need_db_update,
                            const bm::bvector<>& unwanted_jobs,
                            unsigned*            job_id);


    /// Set running job to done status, find and return pending job
    /// @param done_job_id
    ///     job id going from running to done status. Ignored if 0.
    /// @param need_db_update
    ///     (out) TRUE if database needs to be updated (for done job id). 
    ///     Update is not needed if database 
    /// @return job_id, 0 - no pending jobs
    unsigned PutDone_GetPending(unsigned int done_job_id,
                                bool*        need_db_update);

    /// Reschedule job without status check
    void ForceReschedule(unsigned job_id);

    /// Logical AND of candidates and pending jobs
    /// (candidate_set &= pending_set)
    void PendingIntersect(bm::bvector<>* candidate_set);

    /// Return job id (job is taken out of the regular job matrix)
    /// 0 - no pending jobs
    unsigned BorrowPendingJob();

    /// Return borrowed job to the specified status
    /// (pending or running)
    void ReturnBorrowedJob(unsigned int  job_id, 
                          CNetScheduleClient::EJobStatus status);

    /// TRUE if we have pending jobs
    bool AnyPending() const;

    /// Returned jobs come back to pending area
    void Return2Pending();


    /// Get first job id from DONE status
    unsigned int GetFirstDone() const;

    /// Get first job in the specified status
    unsigned int GetFirst(CNetScheduleClient::EJobStatus  status) const;

    /// Set job status without logic control.
    /// @param status
    ///     Status to set (all other statuses are cleared)
    ///     Non existing status code clears all statuses
    void SetStatus(unsigned int                    job_id, 
                   CNetScheduleClient::EJobStatus  status);


    /// Set job status without any protection 
    void SetExactStatusNoLock(
        unsigned int                         job_id, 
        CNetScheduleClient::EJobStatus       status,
                              bool           set_clear);

    /// Return number of jobs in specified status
    unsigned CountStatus(CNetScheduleClient::EJobStatus status) const;

    void StatusStatistics(CNetScheduleClient::EJobStatus status,
                          TBVector::statistics*          st) const;

    /// Specified status is OR-ed with the target vector
    void StatusSnapshot(CNetScheduleClient::EJobStatus status,
                        TBVector*                      bv) const;

    static
    bool IsCancelCode(CNetScheduleClient::EJobStatus status)
    {
        return (status == CNetScheduleClient::eCanceled) ||
               (status == CNetScheduleClient::eFailed);
    }

    /// Clear status storage
    ///
    /// @param bv
    ///    If not NULL all ids from the matrix are OR-ed with this vector 
    ///    (bv is not cleared)
    void ClearAll(TBVector* bv = 0);

    /// Free unused memory buffers
    void FreeUnusedMem();

    void PrintStatusMatrix(CNcbiOstream& out) const;


protected:

    CNetScheduleClient::EJobStatus 
        GetStatusNoLock(unsigned int job_id) const;

    /// Check if job is in specified status
    /// @return -1 if no, status value otherwise
    CNetScheduleClient::EJobStatus 
    IsStatusNoLock(unsigned int job_id, 
        CNetScheduleClient::EJobStatus st1,
        CNetScheduleClient::EJobStatus st2 = CNetScheduleClient::eJobNotFound,
        CNetScheduleClient::EJobStatus st3 = CNetScheduleClient::eJobNotFound
        ) const;

    /// Check if job is in specified status and clear it
    /// @return -1 if no, status value otherwise
    CNetScheduleClient::EJobStatus 
    ClearIfStatusNoLock(unsigned int job_id, 
        CNetScheduleClient::EJobStatus st1,
        CNetScheduleClient::EJobStatus st2 = CNetScheduleClient::eJobNotFound,
        CNetScheduleClient::EJobStatus st3 = CNetScheduleClient::eJobNotFound
        ) const;


    void ReportInvalidStatus(unsigned int    job_id, 
             CNetScheduleClient::EJobStatus  status,
             CNetScheduleClient::EJobStatus  old_status);
    void x_SetClearStatusNoLock(unsigned int job_id,
             CNetScheduleClient::EJobStatus  status,
             CNetScheduleClient::EJobStatus  old_status);

    void Return2PendingNoLock();
    unsigned int GetPendingJobNoLock();
    void FreeUnusedMemNoLock();
    void IncDoneJobs();

    /// Transfer job to done status 
    void PutDone_NoLock(unsigned int done_job_id,
                        bool*        need_db_update);

private:
    CNetScheduler_JobStatusTracker(const CNetScheduler_JobStatusTracker&);
    CNetScheduler_JobStatusTracker& 
        operator=(const CNetScheduler_JobStatusTracker&);
private:

    TStatusStorage          m_StatusStor;
    mutable CRWLock         m_Lock;
    /// Pending Ids extracted out
    TBVector                m_BorrowedIds; 
    /// Last pending id
    bm::id_t                m_LastPending;
    /// Done jobs counter
    unsigned                m_DoneCnt;
};


/// @internal
class CNetSchedule_JS_BorrowGuard
{
public:
    CNetSchedule_JS_BorrowGuard(
        CNetScheduler_JobStatusTracker& strack,
        unsigned int                    job_id,
        CNetScheduleClient::EJobStatus  old_status = CNetScheduleClient::ePending)
    : m_Tracker(strack),
      m_OldStatus(old_status),
       m_JobId(job_id)
    {
        _ASSERT(job_id);
    }

    ~CNetSchedule_JS_BorrowGuard()
    {
        if (m_JobId) {
            m_Tracker.ReturnBorrowedJob(m_JobId, m_OldStatus);
        }
    }
    
    void ReturnToStatus(CNetScheduleClient::EJobStatus status)
    {
        if (m_JobId) {
            m_Tracker.ReturnBorrowedJob(m_JobId, status);
            m_JobId = 0;
        }
    }

private:
    CNetSchedule_JS_BorrowGuard(const CNetSchedule_JS_BorrowGuard&);
    CNetSchedule_JS_BorrowGuard& operator=(const CNetSchedule_JS_BorrowGuard&);
private:
    CNetScheduler_JobStatusTracker&  m_Tracker;
    CNetScheduleClient::EJobStatus   m_OldStatus;
    unsigned int                     m_JobId;
};

/// @internal
class CNetSchedule_JS_Guard
{
public:
    CNetSchedule_JS_Guard(
        CNetScheduler_JobStatusTracker& strack,
        unsigned int                    job_id,
        CNetScheduleClient::EJobStatus  status)
     : m_Tracker(strack),
       m_OldStatus(strack.ChangeStatus(job_id, status)),
       m_JobId(job_id)
    {
        _ASSERT(job_id);
    }

    ~CNetSchedule_JS_Guard()
    {
        // roll back to the old status
        if (m_OldStatus >= -1) {
            m_Tracker.SetStatus(m_JobId, 
              (CNetScheduleClient::EJobStatus)m_OldStatus);
        } 
    }

    void Release() { m_OldStatus = -2; }

    CNetScheduleClient::EJobStatus GetOldStatus() const
    {
        return (CNetScheduleClient::EJobStatus) m_OldStatus;
    }


private:
    CNetSchedule_JS_Guard(const CNetSchedule_JS_Guard&);
    CNetSchedule_JS_Guard& operator=(CNetSchedule_JS_Guard&);
private:
    CNetScheduler_JobStatusTracker&  m_Tracker;
    int                              m_OldStatus;
    unsigned int                     m_JobId;
};


END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.17  2006/03/17 14:25:29  kuznets
 * Force reschedule (to re-try failed jobs)
 *
 * Revision 1.16  2006/03/13 16:01:36  kuznets
 * Fixed queue truncation (transaction log overflow). Added commands to print queue selectively
 *
 * Revision 1.15  2006/02/21 14:44:57  kuznets
 * Bug fixes, improvements in statistics
 *
 * Revision 1.14  2006/02/09 17:07:41  kuznets
 * Various improvements in job scheduling with respect to affinity
 *
 * Revision 1.13  2006/02/06 14:10:29  kuznets
 * Added job affinity
 *
 * Revision 1.12  2005/08/26 12:36:10  kuznets
 * Performance optimization
 *
 * Revision 1.11  2005/08/22 14:01:58  kuznets
 * Added JobExchange command
 *
 * Revision 1.10  2005/08/18 19:16:31  kuznets
 * Performance optimization
 *
 * Revision 1.9  2005/08/18 16:24:32  kuznets
 * Optimized job retrival out o the bit matrix
 *
 * Revision 1.8  2005/08/15 13:29:50  kuznets
 * Implemented batch job submission
 *
 * Revision 1.7  2005/07/14 13:12:56  kuznets
 * Added load balancer
 *
 * Revision 1.6  2005/05/04 19:09:43  kuznets
 * Added queue dumping
 *
 * Revision 1.5  2005/03/09 17:37:17  kuznets
 * Added node notification thread and execution control timeline
 *
 * Revision 1.4  2005/03/04 12:06:41  kuznets
 * Implenyed UDP callback to clients
 *
 * Revision 1.3  2005/02/28 12:24:17  kuznets
 * New job status Returned, better error processing and queue management
 *
 * Revision 1.2  2005/02/23 19:16:38  kuznets
 * Implemented garbage collection thread
 *
 * Revision 1.1  2005/02/08 16:42:55  kuznets
 * Initial revision
 *
 *
 * ===========================================================================
 */

#endif /* NETSCHEDULE_JOB_STATUS__HPP */

