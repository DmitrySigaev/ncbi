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
 * File Description: Network scheduler job status store
 *
 */
#include <ncbi_pch.hpp>

#include <connect/services/netschedule_client.hpp>

#include "job_status.hpp"

BEGIN_NCBI_SCOPE

CNetScheduler_JobStatusTracker::CNetScheduler_JobStatusTracker()
 : m_BorrowedIds(bm::BM_GAP),
   m_LastPending(0)
{
    for (int i = 0; i < CNetScheduleClient::eLastStatus; ++i) {
        bm::bvector<>* bv = new bm::bvector<>();
        m_StatusStor.push_back(bv);
    }
}

CNetScheduler_JobStatusTracker::~CNetScheduler_JobStatusTracker()
{
    for (TStatusStorage::size_type i = 0; i < m_StatusStor.size(); ++i) {
        TBVector* bv = m_StatusStor[i];
        delete bv;
    }
}

CNetScheduleClient::EJobStatus 
CNetScheduler_JobStatusTracker::GetStatus(unsigned int job_id) const
{
    CReadLockGuard guard(m_Lock);
    return GetStatusNoLock(job_id);
}

CNetScheduleClient::EJobStatus 
CNetScheduler_JobStatusTracker::GetStatusNoLock(unsigned int job_id) const
{
    for (int i = m_StatusStor.size()-1; i >= 0; --i) {
        const TBVector& bv = *m_StatusStor[i];
        bool b = bv[job_id];
        if (b) {
            return (CNetScheduleClient::EJobStatus)i;
        }
    }
    if (m_BorrowedIds[job_id]) {
        return CNetScheduleClient::ePending;
    }
    return CNetScheduleClient::eJobNotFound;
}

unsigned 
CNetScheduler_JobStatusTracker::CountStatus(
    CNetScheduleClient::EJobStatus status) const
{
    CReadLockGuard guard(m_Lock);

    const TBVector& bv = *m_StatusStor[(int)status];
    unsigned cnt = bv.count();

    if (status == CNetScheduleClient::ePending) {
        cnt += m_BorrowedIds.count();
    }
    return cnt;
}


void CNetScheduler_JobStatusTracker::Return2Pending()
{
    CWriteLockGuard guard(m_Lock);
    Return2PendingNoLock();
}

void CNetScheduler_JobStatusTracker::Return2PendingNoLock()
{
    TBVector& bv_ret = *m_StatusStor[(int)CNetScheduleClient::eReturned];
    if (!bv_ret.any())
        return;

    TBVector& bv_pen = *m_StatusStor[(int)CNetScheduleClient::ePending];

    bv_pen |= bv_ret;
    bv_ret.clear();
    bv_pen.count();
    m_LastPending = 0;
}


void 
CNetScheduler_JobStatusTracker::SetStatus(unsigned int  job_id, 
                        CNetScheduleClient::EJobStatus  status)
{
    CWriteLockGuard guard(m_Lock);

    for (TStatusStorage::size_type i = 0; i < m_StatusStor.size(); ++i) {
        TBVector& bv = *m_StatusStor[i];
        bv.set(job_id, (int)status == (int)i);
    }
    m_BorrowedIds.set(job_id, false);
}

void CNetScheduler_JobStatusTracker::ClearAll()
{
    CWriteLockGuard guard(m_Lock);

    for (TStatusStorage::size_type i = 0; i < m_StatusStor.size(); ++i) {
        TBVector& bv = *m_StatusStor[i];
        bv.clear(true);
    }
    m_BorrowedIds.clear(true);
}

void CNetScheduler_JobStatusTracker::FreeUnusedMem()
{
    CWriteLockGuard guard(m_Lock);

    for (TStatusStorage::size_type i = 0; i < m_StatusStor.size(); ++i) {
        TBVector& bv = *m_StatusStor[i];
        bv.optimize(0, TBVector::opt_free_0);
    }
    m_BorrowedIds.optimize(0, TBVector::opt_free_0);
}

void
CNetScheduler_JobStatusTracker::SetExactStatusNoLock(unsigned int job_id, 
                                   CNetScheduleClient::EJobStatus status,
                                                     bool         set_clear)
{
    TBVector& bv = *m_StatusStor[(int)status];
    bv.set(job_id, set_clear);

    if ((status == CNetScheduleClient::ePending) && 
        (job_id < m_LastPending)) {
        m_LastPending = job_id - 1;
    }
}


CNetScheduleClient::EJobStatus 
CNetScheduler_JobStatusTracker::ChangeStatus(unsigned int  job_id, 
                           CNetScheduleClient::EJobStatus  status)
{
    CWriteLockGuard guard(m_Lock);
    CNetScheduleClient::EJobStatus old_status = 
                                CNetScheduleClient::eJobNotFound;

    switch (status) {

    case CNetScheduleClient::ePending:
        old_status = GetStatusNoLock(job_id);
        if (old_status == CNetScheduleClient::eJobNotFound) { // new job
            SetExactStatusNoLock(job_id, status, true);
            break;
        }
        if ((old_status == CNetScheduleClient::eReturned) ||
            (old_status == CNetScheduleClient::eRunning)) {
            x_SetClearStatusNoLock(job_id, status, old_status);
            break;
        }
        ReportInvalidStatus(job_id, status, old_status);
        break;

    case CNetScheduleClient::eRunning:
        old_status = IsStatusNoLock(job_id, 
                                    CNetScheduleClient::ePending,
                                    CNetScheduleClient::eReturned,
                                    CNetScheduleClient::eCanceled);
        if ((int)old_status >= 0) {
            if (IsCancelCode(old_status)) {
                break;
            }
            x_SetClearStatusNoLock(job_id, status, old_status);
            break;
        }

        ReportInvalidStatus(job_id, status, old_status);
        break;

    case CNetScheduleClient::eReturned:
        old_status = IsStatusNoLock(job_id, 
                                    CNetScheduleClient::eRunning,
                                    CNetScheduleClient::eCanceled,
                                    CNetScheduleClient::eFailed);
        if ((int)old_status >= 0) {
            if (IsCancelCode(old_status)) {
                break;
            }
            x_SetClearStatusNoLock(job_id, status, old_status);
            break;
        }
        old_status = IsStatusNoLock(job_id, 
                                    CNetScheduleClient::eReturned,
                                    CNetScheduleClient::eDone);
        if ((int)old_status >= 0) {
            break;
        }
        ReportInvalidStatus(job_id, status, old_status);
        break;

    case CNetScheduleClient::eCanceled:
        old_status = IsStatusNoLock(job_id, 
                                    CNetScheduleClient::ePending,
                                    CNetScheduleClient::eRunning,
                                    CNetScheduleClient::eReturned);
        if ((int)old_status >= 0) {
            x_SetClearStatusNoLock(job_id, status, old_status);
            break;
        }
        // in this case (failed, done) we just do nothing.
        old_status = CNetScheduleClient::eCanceled;
        break;

    case CNetScheduleClient::eFailed:
        old_status = IsStatusNoLock(job_id, 
                                    CNetScheduleClient::eRunning,
                                    CNetScheduleClient::eReturned);
        if ((int)old_status >= 0) {
            x_SetClearStatusNoLock(job_id, status, old_status);
            break;
        }

        old_status = CNetScheduleClient::eFailed;
        break;

    case CNetScheduleClient::eDone:

        old_status = ClearIfStatusNoLock(job_id,
                                    CNetScheduleClient::eRunning,
                                    CNetScheduleClient::ePending,
                                    CNetScheduleClient::eReturned);
        if (old_status != CNetScheduleClient::eJobNotFound) {
            break;
        }
        old_status = IsStatusNoLock(job_id, 
                                    CNetScheduleClient::eCanceled,
                                    CNetScheduleClient::eFailed);
        if (IsCancelCode(old_status)) {
            break;
        }

/*
        old_status = IsStatusNoLock(job_id, 
                                    CNetScheduleClient::eCanceled,
                                    CNetScheduleClient::eFailed);
        if (IsCancelCode(old_status)) {
            break;
        }
        old_status = IsStatusNoLock(job_id,
                                    CNetScheduleClient::eRunning,
                                    CNetScheduleClient::ePending,
                                    CNetScheduleClient::eReturned);
        if ((int)old_status >= 0) {
            x_SetClearStatusNoLock(job_id, status, old_status);
            break;
        }
*/
        ReportInvalidStatus(job_id, status, old_status);
        break;    
        
    default:
        _ASSERT(0);
    }

    return old_status;
}

void 
CNetScheduler_JobStatusTracker::AddPendingBatch(
                                    unsigned job_id_from, unsigned job_id_to)
{
    CWriteLockGuard guard(m_Lock);

    TBVector& bv = *m_StatusStor[(int)CNetScheduleClient::ePending];

    bv.set_range(job_id_from, job_id_to);
}

unsigned int CNetScheduler_JobStatusTracker::GetPendingJob()
{
    TBVector& bv = 
        *m_StatusStor[(int)CNetScheduleClient::ePending];

    bm::id_t job_id;
    CWriteLockGuard guard(m_Lock);

    for (int i = 0; i < 2; ++i) {
        job_id = bv.extract_next(m_LastPending);
        if (job_id) {
            TBVector& bv2 = *m_StatusStor[(int)CNetScheduleClient::eRunning];
            bv2.set(job_id, true);
/*
            x_SetClearStatusNoLock(job_id,
                                   CNetScheduleClient::eRunning,
                                   CNetScheduleClient::ePending);
*/
            m_LastPending = job_id;
            break;
        } else {
            Return2PendingNoLock();
        }
/*
        if (bv.any()) {
            job_id = bv.get_first();
            if (job_id) {
                x_SetClearStatusNoLock(job_id, 
                                    CNetScheduleClient::eRunning, 
                                    CNetScheduleClient::ePending);
                return job_id;
            }
        }        
        Return2PendingNoLock();
*/
    }
    return job_id;
}

unsigned int CNetScheduler_JobStatusTracker::BorrowPendingJob()
{
    TBVector& bv = 
        *m_StatusStor[(int)CNetScheduleClient::ePending];

    bm::id_t job_id;
    CWriteLockGuard guard(m_Lock);

    for (int i = 0; i < 2; ++i) {
        job_id = bv.get_next(m_LastPending);
        if (job_id) {
            m_BorrowedIds.set(job_id, true);
            m_LastPending = job_id;
            bv.set(job_id, false);
            break;
        } else {
            Return2PendingNoLock();
        }
/*
        if (bv.any()) {
            job_id = bv.get_first();
            if (job_id) {
                m_BorrowedIds.set(job_id, true);
                return job_id;
            }
        }        
        Return2PendingNoLock();
*/
    }
    return job_id;
}

void CNetScheduler_JobStatusTracker::ReturnBorrowedJob(unsigned int  job_id, 
                                                       CNetScheduleClient::EJobStatus status)
{
    CWriteLockGuard guard(m_Lock);
    if (!m_BorrowedIds[job_id]) {
        ReportInvalidStatus(job_id, status, CNetScheduleClient::ePending);
        return;
    }
    m_BorrowedIds.set(job_id, false);
    SetExactStatusNoLock(job_id, status, true /*set status*/);
}


bool CNetScheduler_JobStatusTracker::AnyPending() const
{
    const TBVector& bv = 
        *m_StatusStor[(int)CNetScheduleClient::ePending];

    CReadLockGuard guard(m_Lock);
    return bv.any();
}

unsigned int CNetScheduler_JobStatusTracker::GetFirstDone() const
{
    return GetFirst(CNetScheduleClient::eDone);
}

unsigned int 
CNetScheduler_JobStatusTracker::GetFirst(
                        CNetScheduleClient::EJobStatus  status) const
{
    const TBVector& bv = *m_StatusStor[(int)status];

    CReadLockGuard guard(m_Lock);
    unsigned int job_id = bv.get_first();
    return job_id;
}


void 
CNetScheduler_JobStatusTracker::x_SetClearStatusNoLock(
                                            unsigned int job_id,
                          CNetScheduleClient::EJobStatus status,
                          CNetScheduleClient::EJobStatus old_status)
{
    SetExactStatusNoLock(job_id, status, true);
    SetExactStatusNoLock(job_id, old_status, false);
}

void 
CNetScheduler_JobStatusTracker::ReportInvalidStatus(unsigned int job_id, 
                         CNetScheduleClient::EJobStatus          status,
                         CNetScheduleClient::EJobStatus      old_status)
{
    string msg = "Job status cannot be changed. ";
    msg += "Old status ";
    msg += CNetScheduleClient::StatusToString(old_status);
    msg += ". New status ";
    msg += CNetScheduleClient::StatusToString(status);
    NCBI_THROW(CNetScheduleException, eInvalidJobStatus, msg);
}


CNetScheduleClient::EJobStatus 
CNetScheduler_JobStatusTracker::IsStatusNoLock(unsigned int job_id, 
        CNetScheduleClient::EJobStatus st1,
        CNetScheduleClient::EJobStatus st2,
        CNetScheduleClient::EJobStatus st3
        ) const
{
    if (st1 == CNetScheduleClient::eJobNotFound) {
        return CNetScheduleClient::eJobNotFound;;
    } else {
        const TBVector& bv = *m_StatusStor[(int)st1];
        if (bv[job_id]) {
            return st1;
        }
    }

    if (st2 == CNetScheduleClient::eJobNotFound) {
        return CNetScheduleClient::eJobNotFound;;
    } else {
        const TBVector& bv = *m_StatusStor[(int)st2];
        if (bv[job_id]) {
            return st2;
        }
    }

    if (st3 == CNetScheduleClient::eJobNotFound) {
        return CNetScheduleClient::eJobNotFound;;
    } else {
        const TBVector& bv = *m_StatusStor[(int)st3];
        if (bv[job_id]) {
            return st3;
        }
    }


    return CNetScheduleClient::eJobNotFound;
}

CNetScheduleClient::EJobStatus 
CNetScheduler_JobStatusTracker::ClearIfStatusNoLock(unsigned int job_id, 
    CNetScheduleClient::EJobStatus st1,
    CNetScheduleClient::EJobStatus st2,
    CNetScheduleClient::EJobStatus st3
    ) const
{
    if (st1 == CNetScheduleClient::eJobNotFound) {
        return CNetScheduleClient::eJobNotFound;
    } else {
        TBVector& bv = *m_StatusStor[(int)st1];
        if (bv.set_bit(job_id, false)) {
            return st1;
        }
    }

    if (st2 == CNetScheduleClient::eJobNotFound) {
        return CNetScheduleClient::eJobNotFound;
    } else {
        TBVector& bv = *m_StatusStor[(int)st2];
        if (bv.set_bit(job_id, false)) {
            return st2;
        }
    }

    if (st3 == CNetScheduleClient::eJobNotFound) {
        return CNetScheduleClient::eJobNotFound;
    } else {
        TBVector& bv = *m_StatusStor[(int)st3];
        if (bv.set_bit(job_id, false)) {
            return st3;
        }
    }


    return CNetScheduleClient::eJobNotFound;
}


void 
CNetScheduler_JobStatusTracker::PrintStatusMatrix(CNcbiOstream& out) const
{
    CReadLockGuard guard(m_Lock);
    for (size_t i = CNetScheduleClient::ePending; 
                i < m_StatusStor.size(); ++i) {
        out << "status:" 
            << CNetScheduleClient::StatusToString(
                        (CNetScheduleClient::EJobStatus)i) << "\n\n";
        const TBVector& bv = *m_StatusStor[i];
        TBVector::enumerator en(bv.first());
        for (int cnt = 0;en.valid(); ++en, ++cnt) {
            out << *en << ", ";
            if (cnt % 10 == 0) {
                out << "\n";
            }
        } // for
        out << "\n\n";
        if (!out.good()) break;
    } // for

    out << "status: Borrowed pending" << "\n\n";
    TBVector::enumerator en(m_BorrowedIds.first());
    for (int cnt = 0;en.valid(); ++en, ++cnt) {
        out << *en << ", ";
        if (cnt % 10 == 0) {
            out << "\n";
        }
    } // for
    out << "\n\n";
    
}

END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.19  2005/08/18 19:16:31  kuznets
 * Performance optimization
 *
 * Revision 1.18  2005/08/18 18:04:47  kuznets
 * GetPendingJob() performance optimization
 *
 * Revision 1.17  2005/08/18 16:40:01  kuznets
 * Fixed bug in GetPendingJob()
 *
 * Revision 1.16  2005/08/18 16:24:32  kuznets
 * Optimized job retrival out o the bit matrix
 *
 * Revision 1.15  2005/08/15 13:29:46  kuznets
 * Implemented batch job submission
 *
 * Revision 1.14  2005/07/14 13:12:56  kuznets
 * Added load balancer
 *
 * Revision 1.13  2005/06/20 15:36:25  kuznets
 * Let job go from pending to done (rescheduling)
 *
 * Revision 1.12  2005/05/04 19:09:43  kuznets
 * Added queue dumping
 *
 * Revision 1.11  2005/05/02 14:44:40  kuznets
 * Implemented remote monitoring
 *
 * Revision 1.10  2005/04/27 14:59:46  kuznets
 * Improved error messaging
 *
 * Revision 1.9  2005/03/22 19:02:54  kuznets
 * Reflecting chnages in connect layout
 *
 * Revision 1.8  2005/03/09 17:37:16  kuznets
 * Added node notification thread and execution control timeline
 *
 * Revision 1.7  2005/03/04 12:06:41  kuznets
 * Implenyed UDP callback to clients
 *
 * Revision 1.6  2005/02/28 12:24:17  kuznets
 * New job status Returned, better error processing and queue management
 *
 * Revision 1.5  2005/02/23 19:16:38  kuznets
 * Implemented garbage collection thread
 *
 * Revision 1.4  2005/02/22 16:13:00  kuznets
 * Performance optimization
 *
 * Revision 1.3  2005/02/14 17:57:41  kuznets
 * Fixed a bug in queue procesing
 *
 * Revision 1.2  2005/02/11 14:45:29  kuznets
 * Fixed compilation issue (GCC)
 *
 * Revision 1.1  2005/02/08 16:42:55  kuznets
 * Initial revision
 *
 *
 * ===========================================================================
 */


