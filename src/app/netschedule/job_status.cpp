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
 * File Description: Network scheduler job status store
 *
 */
#include <ncbi_pch.hpp>
#include <corelib/ncbi_system.hpp>

#include <connect/services/netschedule_api.hpp>
#include <util/bitset/bmalgo.h>

#include "job_status.hpp"
#include "ns_gc_registry.hpp"


BEGIN_NCBI_SCOPE


CJobStatusTracker::CJobStatusTracker()
 : m_DoneCnt(0)
{
    // Note: one bit vector is not used - the corresponding job state became
    // obsolete and was deleted. The matrix though uses job statuses as indexes
    // for fast access, so that missed status vector is also created. The rest
    // of the code iterates only through the valid states.
    for (int i = 0; i < CNetScheduleAPI::eLastStatus; ++i) {
//        m_StatusStor.push_back(new TNSBitVector(bm::BM_GAP));
        m_StatusStor.push_back(new TNSBitVector());
    }
}


CJobStatusTracker::~CJobStatusTracker()
{
    for (int i = 0; i < CNetScheduleAPI::eLastStatus; ++i) {
        delete m_StatusStor[i];
    }
}


TJobStatus CJobStatusTracker::GetStatus(unsigned job_id) const
{
    CReadLockGuard      guard(m_Lock);

    for (size_t  k = 0; k < g_ValidJobStatusesSize; ++k) {
        const TNSBitVector &    bv = *m_StatusStor[g_ValidJobStatuses[k]];

        if (bv.get_bit(job_id))
            return g_ValidJobStatuses[k];
    }
    return CNetScheduleAPI::eJobNotFound;
}


unsigned int  CJobStatusTracker::CountStatus(TJobStatus status) const
{
    CReadLockGuard      guard(m_Lock);

    return m_StatusStor[(int)status]->count();
}


unsigned int
CJobStatusTracker::CountStatus(const vector<TJobStatus> &  statuses) const
{
    unsigned int    cnt = 0;
    CReadLockGuard  guard(m_Lock);

    for (vector<TJobStatus>::const_iterator  k = statuses.begin();
         k != statuses.end(); ++k)
        cnt += m_StatusStor[(int)(*k)]->count();

    return cnt;
}


vector<unsigned int>
CJobStatusTracker::GetJobCounters(const vector<TJobStatus> &  statuses) const
{
    vector<unsigned int>        counters;
    CReadLockGuard              guard(m_Lock);

    for (vector<TJobStatus>::const_iterator  k = statuses.begin();
            k != statuses.end(); ++k)
        counters.push_back(m_StatusStor[(int)(*k)]->count());

    return counters;
}


unsigned int  CJobStatusTracker::Count(void) const
{
    unsigned int    cnt = 0;
    CReadLockGuard  guard(m_Lock);

    for (size_t  k = 0; k < g_ValidJobStatusesSize; ++k)
        cnt += m_StatusStor[g_ValidJobStatuses[k]]->count();

    return cnt;
}


unsigned int  CJobStatusTracker::GetMinJobID(void) const
{
    unsigned int    id = 0;
    CReadLockGuard  guard(m_Lock);

    for (size_t  k = 0; k < g_ValidJobStatusesSize; ++k) {
        TNSBitVector &      bv = *m_StatusStor[g_ValidJobStatuses[k]];
        if (!bv.any())
            continue;
        if (id == 0)
            id = bv.get_first();
        else {
            unsigned int    first = bv.get_first();
            if (first < id)
                id = first;
        }
    }

    return id;
}


bool  CJobStatusTracker::AnyJobs(void) const
{
    CReadLockGuard  guard(m_Lock);

    for (size_t  k = 0; k < g_ValidJobStatusesSize; ++k)
        if (m_StatusStor[g_ValidJobStatuses[k]]->any())
            return true;
    return false;
}


bool  CJobStatusTracker::AnyJobs(TJobStatus  status) const
{
    CReadLockGuard      guard(m_Lock);

    return m_StatusStor[(int)status]->any();
}


bool  CJobStatusTracker::AnyJobs(const vector<TJobStatus> &  statuses) const
{
    CReadLockGuard  guard(m_Lock);

    for (vector<TJobStatus>::const_iterator  k = statuses.begin();
         k != statuses.end(); ++k)
        if (m_StatusStor[(int)(*k)]->any())
            return true;

    return false;
}



void CJobStatusTracker::StatusStatistics(TJobStatus                  status,
                                         TNSBitVector::statistics *  st) const
{
    _ASSERT(st);
    CReadLockGuard          guard(m_Lock);
    const TNSBitVector &    bv = *m_StatusStor[(int)status];

    bv.calc_stat(st);
}


void CJobStatusTracker::SetStatus(unsigned    job_id, TJobStatus  status)
{
    TJobStatus              old_status = CNetScheduleAPI::eJobNotFound;
    CWriteLockGuard         guard(m_Lock);

    for (size_t  k = 0; k < g_ValidJobStatusesSize; ++k) {
        TNSBitVector &      bv = *m_StatusStor[g_ValidJobStatuses[k]];

        if (bv.get_bit(job_id)) {
            if (old_status != CNetScheduleAPI::eJobNotFound)
                NCBI_THROW(CNetScheduleException, eInternalError,
                           "State matrix was damaged, more than one status "
                           "active for job " + NStr::NumericToString(job_id));
            old_status = g_ValidJobStatuses[k];

            if (status != g_ValidJobStatuses[k])
                bv.set_bit(job_id, false);
        } else {
            if (status == g_ValidJobStatuses[k])
                bv.set_bit(job_id, true);
        }
    }
}


void CJobStatusTracker::AddPendingJob(unsigned int  job_id)
{
    CWriteLockGuard         guard(m_Lock);
    m_StatusStor[(int) CNetScheduleAPI::ePending]->set_bit(job_id, true);
}


void CJobStatusTracker::Erase(unsigned  job_id)
{
    SetStatus(job_id, CNetScheduleAPI::eJobNotFound);
}


void CJobStatusTracker::ClearAll(TNSBitVector *  bv)
{
    CWriteLockGuard         guard(m_Lock);

    for (size_t  k = 0; k < g_ValidJobStatusesSize; ++k) {
        TNSBitVector &      bv1 = *m_StatusStor[g_ValidJobStatuses[k]];

        *bv |= bv1;
        bv1.clear(true);
    }
}


void CJobStatusTracker::OptimizeMem()
{
    for (size_t  k = 0; k < g_ValidJobStatusesSize; ++k) {
        TNSBitVector &      bv = *m_StatusStor[g_ValidJobStatuses[k]];
        {{
            CWriteLockGuard     guard(m_Lock);
            bv.optimize(0, TNSBitVector::opt_free_0);
        }}
    }
}


void CJobStatusTracker::SetExactStatusNoLock(unsigned   job_id,
                                             TJobStatus status,
                                             bool       set_clear)
{
    TNSBitVector &      bv = *m_StatusStor[(int)status];
    bv.set(job_id, set_clear);
}


void CJobStatusTracker::AddPendingBatch(unsigned  job_id_from,
                                        unsigned  job_id_to)
{
    CWriteLockGuard     guard(m_Lock);
    m_StatusStor[(int) CNetScheduleAPI::ePending]->set_range(job_id_from,
                                                             job_id_to);
}


unsigned int
CJobStatusTracker::GetJobByStatus(TJobStatus            status,
                                  const TNSBitVector &  unwanted_jobs,
                                  const TNSBitVector &  restrict_jobs,
                                  bool                  restricted) const
{
    TNSBitVector &              bv = *m_StatusStor[(int)status];
    TNSBitVector::enumerator    en;
    CReadLockGuard              guard(m_Lock);

    for (en = bv.first(); en.valid(); ++en) {
        unsigned int    job_id = *en;
        if (unwanted_jobs.get_bit(job_id))
            continue;
        if (restricted) {
            if (restrict_jobs.get_bit(job_id))
                return job_id;
        } else {
            return job_id;
        }
    }
    return 0;
}


unsigned int
CJobStatusTracker::GetJobByStatus(const vector<TJobStatus> &   statuses,
                                  const TNSBitVector &         unwanted_jobs,
                                  const TNSBitVector &         restrict_jobs,
                                  bool                         restricted) const
{
    TNSBitVector                jobs;
    TNSBitVector::enumerator    en;
    CReadLockGuard              guard(m_Lock);

    for (vector<TJobStatus>::const_iterator  k = statuses.begin();
         k != statuses.end(); ++k)
        jobs |= *m_StatusStor[(int)(*k)];

    for (en = jobs.first(); en.valid(); ++en) {
        unsigned int    job_id = *en;
        if (unwanted_jobs.get_bit(job_id))
            continue;
        if (restricted) {
            if (restrict_jobs.get_bit(job_id))
                return job_id;
        } else {
            return job_id;
        }
    }
    return 0;
}


void
CJobStatusTracker::GetJobs(const vector<TJobStatus> &  statuses,
                           TNSBitVector &  jobs) const
{
    CReadLockGuard      guard(m_Lock);

    for (vector<TJobStatus>::const_iterator  k = statuses.begin();
         k != statuses.end(); ++k)
        jobs |= *m_StatusStor[(int)(*k)];
}


void
CJobStatusTracker::GetJobs(CNetScheduleAPI::EJobStatus  status,
                           TNSBitVector &  jobs) const
{
    CReadLockGuard      guard(m_Lock);
    jobs = *m_StatusStor[(int)status];
}


TNSBitVector
CJobStatusTracker::GetOutdatedPendingJobs(CNSPreciseTime          timeout,
                                          const CJobGCRegistry &  gc_registry) const
{
    static CNSPreciseTime   s_LastTimeout = kTimeZero;
    static unsigned int     s_LastCheckedJobID = 0;
    static const size_t     kMaxCandidates = 100;

    TNSBitVector            result;

    if (timeout == kTimeZero)
        return result;  // Not configured

    size_t                  count = 0;
    const TNSBitVector &    pending_jobs = *m_StatusStor[(int) CNetScheduleAPI::ePending];
    CNSPreciseTime          limit = CNSPreciseTime::Current() - timeout;
    CReadLockGuard          guard(m_Lock);

    if (s_LastTimeout != timeout) {
        s_LastTimeout = timeout;
        s_LastCheckedJobID = 0;
    }

    TNSBitVector::enumerator    en(pending_jobs.first());
    for (; en.valid() && count < kMaxCandidates; ++en, ++count) {
        unsigned int    job_id = *en;
        if (job_id <= s_LastCheckedJobID) {
            result.set_bit(job_id, true);
            continue;
        }
        if (gc_registry.GetPreciseSubmitTime(job_id) < limit) {
            s_LastCheckedJobID = job_id;
            result.set_bit(job_id, true);
            continue;
        }

        // No more old jobs
        break;
    }

    return result;
}


TNSBitVector
CJobStatusTracker::GetOutdatedReadVacantJobs(CNSPreciseTime          timeout,
                                             const TNSBitVector &    read_jobs,
                                             const CJobGCRegistry &  gc_registry) const
{
    static CNSPreciseTime   s_LastTimeout = kTimeZero;
    static unsigned int     s_LastCheckedJobID = 0;
    static const size_t     kMaxCandidates = 100;

    TNSBitVector            result;

    if (timeout == kTimeZero)
        return result;  // Not configured

    size_t                  count = 0;
    CNSPreciseTime          limit = CNSPreciseTime::Current() - timeout;
    CReadLockGuard          guard(m_Lock);
    TNSBitVector            candidates;

    candidates = *m_StatusStor[(int) CNetScheduleAPI::eDone] |
                 *m_StatusStor[(int) CNetScheduleAPI::eFailed] |
                 *m_StatusStor[(int) CNetScheduleAPI::eCanceled];

    // Exclude jobs which have been read or in a process of reading
    candidates -= read_jobs;

    if (s_LastTimeout != timeout) {
        s_LastTimeout = timeout;
        s_LastCheckedJobID = 0;
    }

    TNSBitVector::enumerator    en(candidates.first());
    for (; en.valid() && count < kMaxCandidates; ++en, ++count) {
        unsigned int    job_id = *en;
        if (job_id <= s_LastCheckedJobID) {
            result.set_bit(job_id, true);
            continue;
        }
        if (gc_registry.GetPreciseReadVacantTime(job_id) < limit) {
            s_LastCheckedJobID = job_id;
            result.set_bit(job_id, true);
            continue;
        }

        // No more old jobs
        break;
    }

    return result;
}


bool CJobStatusTracker::AnyPending() const
{
    const TNSBitVector &    bv = *m_StatusStor[(int) CNetScheduleAPI::ePending];
    CReadLockGuard          guard(m_Lock);

    return bv.any();
}


unsigned CJobStatusTracker::GetNext(TJobStatus status, unsigned job_id) const
{
    const TNSBitVector &    bv = *m_StatusStor[(int)status];
    CReadLockGuard          guard(m_Lock);

    return bv.get_next(job_id);
}


void CJobStatusTracker::x_IncDoneJobs(void)
{
    ++m_DoneCnt;
    if (m_DoneCnt == 65535 * 2) {
        m_DoneCnt = 0;
        {{
            TNSBitVector& bv = *m_StatusStor[(int) CNetScheduleAPI::eDone];
            bv.optimize(0, TNSBitVector::opt_free_01);
        }}
        {{
            TNSBitVector& bv = *m_StatusStor[(int) CNetScheduleAPI::ePending];
            bv.optimize(0, TNSBitVector::opt_free_0);
        }}
    }
}

END_NCBI_SCOPE

