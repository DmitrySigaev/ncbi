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
 * Authors:  Maxim Didenko
 *
 * File Description:
 *    NetSchedule Worker Node implementation
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiexpt.hpp>
#include <connect/services/grid_debug_context.hpp>
#include "grid_thread_context.hpp"


BEGIN_NCBI_SCOPE

/// @internal
CGridThreadContext::CGridThreadContext(CWorkerNodeJobContext& job_context)
    : m_JobContext(NULL), m_RateControl(1)
{
    m_Reporter.reset(job_context.GetWorkerNode().CreateClient());
    m_Reader.reset(job_context.GetWorkerNode().CreateStorage());
    m_Writer.reset(job_context.GetWorkerNode().CreateStorage());
    m_ProgressWriter.reset(job_context.GetWorkerNode().CreateStorage());
    SetJobContext(job_context);
}
/// @internal
void CGridThreadContext::SetJobContext(CWorkerNodeJobContext& job_context)
{
    _ASSERT(!m_JobContext);
    m_JobContext = &job_context;
    job_context.SetThreadContext(this);
    CGridDebugContext* debug_context = CGridDebugContext::GetInstance();
    if (debug_context) {
        debug_context->DumpInput(m_JobContext->GetJobInput(), 
                                 m_JobContext->m_JobNumber);
    }
}
/// @internal
void CGridThreadContext::Reset()
{
    _ASSERT(m_JobContext);
    m_JobContext->SetThreadContext(NULL);
    m_JobContext = NULL;
 
}
/// @internal
CWorkerNodeJobContext& CGridThreadContext::GetJobContext()
{
    _ASSERT(m_JobContext);
    return *m_JobContext;
}

/// @internal
CNcbiIstream& CGridThreadContext::GetIStream(IBlobStorage::ELockMode mode)
{
    _ASSERT(m_JobContext);
    if (m_Reader.get()) {
        return m_Reader->GetIStream(m_JobContext->GetJobInput(),
                                    &m_JobContext->SetJobInputBlobSize(),
                                    mode);
    }
    NCBI_THROW(CBlobStorageException,
               eReader, "Reader is not set.");
}
/// @internal
CNcbiOstream& CGridThreadContext::GetOStream()
{
    _ASSERT(m_JobContext);
    if (m_Writer.get()) {
        return m_Writer->CreateOStream(m_JobContext->SetJobOutput());
    }
    NCBI_THROW(CBlobStorageException,
               eWriter, "Writer is not set.");
}
/// @internal
void CGridThreadContext::PutProgressMessage(const string& msg, bool send_immediately)
{
    _ASSERT(m_JobContext);
    if ( !send_immediately || !m_RateControl.Approve(CRequestRateControl::eErrCode))
        return;
    try {
        CGridDebugContext* debug_context = CGridDebugContext::GetInstance();
        string& blob_id = m_JobContext->SetJobProgressMsgKey();
        if (!debug_context || 
            debug_context->GetDebugMode() != CGridDebugContext::eGDC_Execute) {
            
            if (blob_id.empty() && m_Reporter.get()) {
                blob_id = m_Reporter->GetProgressMsg(m_JobContext->GetJobKey());
            }
            if (!blob_id.empty() && m_ProgressWriter.get()) {
                CNcbiOstream& os = m_ProgressWriter->CreateOStream(blob_id);
                os << msg;
                m_ProgressWriter->Reset();
            }
            else {
                ERR_POST("Couldn't send a progress message.");
            }
        }
        if (debug_context) {
            debug_context->DumpProgressMessage(m_JobContext->GetJobKey(),
                                               msg, 
                                               m_JobContext->m_JobNumber);
        }           
    } catch (exception& ex) {
        ERR_POST("Couldn't send a progress message: " << ex.what());
    }
}
/// @internal
void CGridThreadContext::SetJobRunTimeout(unsigned time_to_run)
{
    _ASSERT(m_JobContext);
    if (m_Reporter.get()) {
        try {
            m_Reporter->SetRunTimeout(m_JobContext->GetJobKey(), time_to_run);
        }
        catch(exception& ex) {
            ERR_POST("CWorkerNodeJobContext::SetJobRunTimeout : " 
                     << ex.what());
        } 
    }
    else {
        NCBI_THROW(CBlobStorageException,
                   eWriter, "Reporter is not set.");
    }
}

/// @internal
void CGridThreadContext::PutResult(int ret_code)
{
    _ASSERT(m_JobContext);
    CGridDebugContext* debug_context = CGridDebugContext::GetInstance();
    if (!debug_context || 
        debug_context->GetDebugMode() != CGridDebugContext::eGDC_Execute) {

        if (m_Reporter.get()) {
            m_Reporter->PutResult(m_JobContext->GetJobKey(),
                                  ret_code,
                                  m_JobContext->GetJobOutput());
        }
    }
    m_JobContext->GetWorkerNode().m_JobsSucceed.Add(1);
}
/// @internal
void CGridThreadContext::ReturnJob()
{
    _ASSERT(m_JobContext);
    CGridDebugContext* debug_context = CGridDebugContext::GetInstance();
    if (!debug_context || 
        debug_context->GetDebugMode() != CGridDebugContext::eGDC_Execute) {

        if (m_Reporter.get()) {
            int ret_code;
            string output;
            CNetScheduleClient::EJobStatus status = 
                m_Reporter->GetStatus(m_JobContext->GetJobKey(), 
                                      &ret_code, 
                                      &output);
            if (status == CNetScheduleClient::eJobNotFound || 
                status == CNetScheduleClient::eFailed ||
                status == CNetScheduleClient::eDone)  {
                m_JobContext->GetWorkerNode().m_JobsLost.Add(1);
            } else if (status == CNetScheduleClient::eCanceled) {
                m_JobContext->GetWorkerNode().m_JobsCanceled.Add(1);
            } else {
                m_Reporter->ReturnJob(m_JobContext->GetJobKey());
                m_JobContext->GetWorkerNode().m_JobsReturned.Add(1);
            }
            return;
        }
    }
    m_JobContext->GetWorkerNode().m_JobsReturned.Add(1);
}
/// @internal
void CGridThreadContext::PutFailure(const string& msg)
{
    _ASSERT(m_JobContext);
    CGridDebugContext* debug_context = CGridDebugContext::GetInstance();
    if (!debug_context || 
        debug_context->GetDebugMode() != CGridDebugContext::eGDC_Execute) {

        if (m_Reporter.get()) {
            m_Reporter->PutFailure(m_JobContext->GetJobKey(),msg);
        }
    }
    m_JobContext->GetWorkerNode().m_JobsFailed.Add(1);
}
bool CGridThreadContext::IsJobCanceled()
{
    _ASSERT(m_JobContext);
    CGridDebugContext* debug_context = CGridDebugContext::GetInstance();
    if (!debug_context || 
        debug_context->GetDebugMode() != CGridDebugContext::eGDC_Execute) {

        if (m_Reporter.get()) {
            int ret_code;
            string output;
            CNetScheduleClient::EJobStatus status = 
                m_Reporter->GetStatus(m_JobContext->GetJobKey(), 
                                      &ret_code, 
                                      &output);
            if (status == CNetScheduleClient::eJobNotFound || 
                status == CNetScheduleClient::eCanceled || 
                status == CNetScheduleClient::eFailed ||
                status == CNetScheduleClient::eDone)
                return true;
        }
    }
    return false;
}

/// @internal
bool CGridThreadContext::IsJobCommitted() const
{
    _ASSERT(m_JobContext);
    return m_JobContext->IsJobCommitted();
}
/// @internal
IWorkerNodeJob* CGridThreadContext::CreateJob()
{
    _ASSERT(m_JobContext);
    return m_JobContext->GetWorkerNode().CreateJob();
}

void CGridThreadContext::CloseStreams()
{
    _ASSERT(m_JobContext);
    m_Reader->Reset();
    m_Writer->Reset();
    m_ProgressWriter->Reset();
    m_RateControl.Reset(1);

    CGridDebugContext* debug_context = CGridDebugContext::GetInstance();
    if (debug_context) {
        debug_context->DumpOutput(m_JobContext->GetJobKey(),
                                  m_JobContext->GetJobOutput(), 
                                  m_JobContext->m_JobNumber);
    }
}

END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 6.11  2005/12/20 17:26:22  didenko
 * Reorganized netschedule storage facility.
 * renamed INetScheduleStorage to IBlobStorage and moved it to corelib
 * renamed INetScheduleStorageFactory to IBlobStorageFactory and moved it to corelib
 * renamed CNetScheduleNSStorage_NetCache to CBlobStorage_NetCache and moved it
 * to separate files
 * Moved CNetScheduleClientFactory to separate files
 *
 * Revision 6.10  2005/10/26 16:37:44  didenko
 * Added for non-blocking read for netschedule storage
 *
 * Revision 6.9  2005/09/30 14:58:56  didenko
 * Added optional parameter to PutProgressMessage methods which allows
 * sending progress messages regardless of the rate control.
 *
 * Revision 6.8  2005/05/27 14:46:06  didenko
 * Fixed a worker node statistics
 *
 * Revision 6.7  2005/05/23 17:58:57  didenko
 * Changed "grid_debug_context.hpp" to <connect/services/grid_debug_context.hpp>
 *
 * Revision 6.6  2005/05/11 18:57:39  didenko
 * Added worker node statictics
 *
 * Revision 6.5  2005/05/10 14:13:10  didenko
 * Added CloseStreams method
 *
 * Revision 6.4  2005/05/06 14:43:14  didenko
 * Minor fix
 *
 * Revision 6.3  2005/05/06 13:08:06  didenko
 * Added check for a job cancelation in the GetShoutdownLevel method
 *
 * Revision 6.2  2005/05/05 15:57:35  didenko
 * Minor fixes
 *
 * Revision 6.1  2005/05/05 15:18:51  didenko
 * Added debugging facility to worker nodes
 *
 * ===========================================================================
 */
 
