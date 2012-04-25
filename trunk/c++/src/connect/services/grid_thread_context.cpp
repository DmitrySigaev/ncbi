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
 * Authors:  Maxim Didenko, Dmitry Kazimirov
 *
 * File Description:
 *    NetSchedule Worker Node implementation
 */

#include <ncbi_pch.hpp>

#include "grid_thread_context.hpp"
#include "grid_debug_context.hpp"

#include <connect/services/grid_globals.hpp>
#include <connect/services/grid_globals.hpp>
#include <connect/services/grid_rw_impl.hpp>
#include <connect/services/error_codes.hpp>

#include <corelib/ncbiexpt.hpp>
#include <corelib/rwstream.hpp>
#include <corelib/ncbi_system.hpp>


#define NCBI_USE_ERRCODE_X   ConnServ_WorkerNode

#define RESOURCE_OVERUSE_EXIT_CODE 100


BEGIN_NCBI_SCOPE

/// @internal
CGridThreadContext::CGridThreadContext(CGridWorkerNode& node) :
    m_Worker(node),
    m_JobContext(NULL),
    m_NetScheduleExecutor(node.GetNSExecutor()),
    m_NetCacheAPI(node.GetNetCacheAPI()),
    m_MsgThrottler(1),
    m_StatusThrottler(1)
{
    long check_status_period = node.GetCheckStatusPeriod();
    m_CheckStatusPeriod = check_status_period > 1 ? check_status_period : 1;
    m_StatusThrottler.Reset(1, CTimeSpan(m_CheckStatusPeriod, 0));
}

/// @internal
CNcbiIstream& CGridThreadContext::GetIStream()
{
    _ASSERT(m_JobContext);
    if (m_NetCacheAPI) {
        IReader* reader =
               new CStringOrBlobStorageReader(m_JobContext->GetJobInput(),
                                              m_NetCacheAPI,
                                              &m_JobContext->SetJobInputBlobSize());
        m_RStream.reset(new CRStream(reader, 0,0,
                                     CRWStreambuf::fOwnReader
                                   | CRWStreambuf::fLeakExceptions));
        m_RStream->exceptions(IOS_BASE::badbit | IOS_BASE::failbit);
        return *m_RStream;
    }
    NCBI_THROW(CBlobStorageException,
               eReader, "Reader is not set.");
}
/// @internal
CNcbiOstream& CGridThreadContext::GetOStream()
{
    _ASSERT(m_JobContext);
    if (m_NetCacheAPI) {
        size_t max_data_size =
            m_JobContext->GetWorkerNode().GetServerOutputSize();
        m_Writer.reset(new CStringOrBlobStorageWriter(max_data_size,
            m_NetCacheAPI, m_JobContext->SetJobOutput()));
        m_WStream.reset(new CWStream(m_Writer.get(),
            0, 0, CRWStreambuf::fLeakExceptions));
        m_WStream->exceptions(IOS_BASE::badbit | IOS_BASE::failbit);
        return *m_WStream;
    }
    NCBI_THROW(CBlobStorageException,
               eWriter, "Writer is not set.");
}
/// @internal
void CGridThreadContext::PutProgressMessage(const string& msg, bool send_immediately)
{
    _ASSERT(m_JobContext);
    if (!send_immediately &&
        !m_MsgThrottler.Approve(CRequestRateControl::eErrCode))
        return;

    try {
        CGridDebugContext* debug_context = CGridDebugContext::GetInstance();
        if (!debug_context ||
            debug_context->GetDebugMode() != CGridDebugContext::eGDC_Execute) {

            if (m_JobContext->m_Job.progress_msg.empty() ) {
                m_NetScheduleExecutor.GetProgressMsg(m_JobContext->m_Job);
            }
            if (m_NetCacheAPI) {
                if (!m_JobContext->m_Job.progress_msg.empty())
                    m_NetCacheAPI.PutData(m_JobContext->m_Job.progress_msg,
                            msg.data(), msg.length());
                else {
                    m_JobContext->m_Job.progress_msg =
                            m_NetCacheAPI.PutData(msg.data(), msg.length());

                    m_NetScheduleExecutor.PutProgressMsg(m_JobContext->m_Job);
                }
            }
        }
        if (debug_context) {
            debug_context->DumpProgressMessage(m_JobContext->GetJobKey(),
                                               msg,
                                               m_JobContext->GetJobNumber());
        }
    } catch (exception& ex) {
        ERR_POST_X(6, "Couldn't send a progress message: " << ex.what());
    }
}
void CGridThreadContext::JobDelayExpiration(unsigned time_to_run)
{
    _ASSERT(m_JobContext);
    try {
        m_NetScheduleExecutor.JobDelayExpiration(m_JobContext->GetJobKey(), time_to_run);
    } catch(exception& ex) {
        ERR_POST_X(8, "CWorkerNodeJobContext::JobDelayExpiration : "
                       << ex.what());
    }
}

/// @internal
void CGridThreadContext::PutResult()
{
    _ASSERT(m_JobContext);
    CGridDebugContext* debug_context = CGridDebugContext::GetInstance();
    if (!debug_context ||
        debug_context->GetDebugMode() != CGridDebugContext::eGDC_Execute) {

        Uint8 total_memory_limit = m_Worker.GetTotalMemoryLimit();
        if (total_memory_limit > 0) {  // memory check requested
            size_t memory_usage;
            if (!GetMemoryUsage(&memory_usage, 0, 0)) {
                ERR_POST("Could not check self memory usage" );
            } else if (memory_usage > total_memory_limit) {
                ERR_POST(Warning << "Memory usage (" << memory_usage <<
                    ") is above the configured limit (" <<
                    total_memory_limit << ")");

                CGridGlobals::GetInstance().RequestShutdown(
                    CNetScheduleAdmin::eNormalShutdown,
                        RESOURCE_OVERUSE_EXIT_CODE);
            }
        }

        int total_time_limit = m_Worker.GetTotalTimeLimit();
        if (total_time_limit > 0 &&  // time check requested
                time(0) > m_Worker.GetStartupTime() + total_time_limit)
            CGridGlobals::GetInstance().RequestShutdown(
                CNetScheduleAdmin::eNormalShutdown, RESOURCE_OVERUSE_EXIT_CODE);

        m_NetScheduleExecutor.PutResult(m_JobContext->m_Job);

        if (m_Worker.IsExclusiveMode() && m_JobContext->IsJobExclusive())
            m_Worker.LeaveExclusiveMode();
    }
    m_JobContext->GetWorkerNode()
        .x_NotifyJobWatcher(*m_JobContext,
                            IWorkerNodeJobWatcher::eJobSucceed);
}
/// @internal
void CGridThreadContext::ReturnJob()
{
    _ASSERT(m_JobContext);
    CGridDebugContext* debug_context = CGridDebugContext::GetInstance();
    if (!debug_context ||
        debug_context->GetDebugMode() != CGridDebugContext::eGDC_Execute) {

        IWorkerNodeJobWatcher::EEvent event =
            IWorkerNodeJobWatcher::eJobReturned;

        switch (m_NetScheduleExecutor.GetJobStatus(m_JobContext->GetJobKey())) {
        case CNetScheduleAPI::eJobNotFound:
            event = IWorkerNodeJobWatcher::eJobLost;
            break;
        case CNetScheduleAPI::eFailed:
            event = IWorkerNodeJobWatcher::eJobFailed;
            break;
        case CNetScheduleAPI::eDone:
            event = IWorkerNodeJobWatcher::eJobSucceed;
            break;
        case CNetScheduleAPI::eCanceled:
            event = IWorkerNodeJobWatcher::eJobCanceled;
            break;
        case CNetScheduleAPI::eRunning:
            {
                const CNetScheduleJob& job = m_JobContext->GetJob();
                m_NetScheduleExecutor.ReturnJob(job.job_id, job.auth_token);
            }
            break;
        default:
            break;
        }
        m_JobContext->GetWorkerNode().x_NotifyJobWatcher(*m_JobContext, event);
        return;
    }
    m_JobContext->GetWorkerNode()
        .x_NotifyJobWatcher(*m_JobContext,
                            IWorkerNodeJobWatcher::eJobReturned);
}
/// @internal
void CGridThreadContext::PutFailure()
{
    _ASSERT(m_JobContext);
    CGridDebugContext* debug_context = CGridDebugContext::GetInstance();
    if (!debug_context ||
        debug_context->GetDebugMode() != CGridDebugContext::eGDC_Execute) {

        m_NetScheduleExecutor.PutFailure(m_JobContext->m_Job);
    }
    m_JobContext->GetWorkerNode().x_NotifyJobWatcher(*m_JobContext,
        IWorkerNodeJobWatcher::eJobFailed);
}

void CGridThreadContext::PutFailureAndIgnoreErrors(const char* error_message)
{
    try {
        m_JobContext->m_Job.error_msg = error_message;
        PutFailure();
    } catch (exception& e) {
        ERR_POST_X(19, "Failed to report exception: " <<
            m_JobContext->GetJobKey() << " " << e.what());
    }
}

bool CGridThreadContext::IsJobCanceled()
{
    _ASSERT(m_JobContext);
    CGridDebugContext* debug_context = CGridDebugContext::GetInstance();
    if (!debug_context ||
        debug_context->GetDebugMode() != CGridDebugContext::eGDC_Execute) {

        if (m_StatusThrottler.Approve(CRequestRateControl::eErrCode)) {
            switch (m_NetScheduleExecutor.GetJobStatus(
                        m_JobContext->GetJobKey())) {

            case CNetScheduleAPI::eRunning:
                break;

            case CNetScheduleAPI::eCanceled:
                m_JobContext->SetCanceled();
                /* FALL THROUGH */

            default:
                return true;
            }
        }
    }
    return false;
}

/// @internal
IWorkerNodeJob* CGridThreadContext::GetJob()
{
    _ASSERT(m_JobContext);
    IWorkerNodeJob* ret = m_Job.GetPointer();
    if (!ret) {
        try {
            if (!CGridGlobals::GetInstance().ReuseJobObject())
                ret = m_JobContext->GetWorkerNode().CreateJob();
            else {
                m_Job.Reset(m_JobContext->GetWorkerNode().CreateJob());
                ret = m_Job.GetPointer();
            }
        } catch (...) {
            ERR_POST_X(9, "Could not create an instance of a job class." );
            CGridGlobals::GetInstance().
                RequestShutdown(CNetScheduleAdmin::eShutdownImmediate);
            throw;
        }
    }
    if (!ret) {
        CGridGlobals::GetInstance().
            RequestShutdown(CNetScheduleAdmin::eShutdownImmediate);
        NCBI_THROW(CException, eInvalid,
                   "Could not create an instance of a job class.");
    }
    return ret;
}

void CGridThreadContext::CloseStreams()
{
    _ASSERT(m_JobContext);

    m_MsgThrottler.Reset(1);
    m_StatusThrottler.Reset(1, CTimeSpan(m_CheckStatusPeriod, 0));

    m_RStream.reset();
    m_WStream.reset();
    if (m_Writer.get() != NULL) {
        m_Writer->Close();
        m_Writer.reset();
    }

    CGridDebugContext* debug_context = CGridDebugContext::GetInstance();
    if (debug_context) {
        debug_context->DumpOutput(m_JobContext->GetJobKey(),
                                  m_JobContext->GetJobOutput(),
                                  m_JobContext->GetJobNumber());
    }
}

END_NCBI_SCOPE
