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
 * Authors:  Maxim Didenko, Anatoliy Kuznetsov, Dmitry Kazimirov
 *
 * File Description:
 *    NetSchedule Worker Node implementation
 */

#include <ncbi_pch.hpp>

#include "wn_commit_thread.hpp"
#include "wn_cleanup.hpp"
#include "grid_worker_impl.hpp"
#include "netschedule_api_impl.hpp"

#include <connect/services/grid_globals.hpp>
#include <connect/services/grid_worker_app.hpp>
#include <connect/services/grid_rw_impl.hpp>
#include <connect/services/ns_job_serializer.hpp>
#include <corelib/rwstream.hpp>


#define NCBI_USE_ERRCODE_X   ConnServ_WorkerNode

BEGIN_NCBI_SCOPE

/////////////////////////////////////////////////////////////////////////////
//

void COfflineJobContext::PutProgressMessage(const string& msg,
        bool send_immediately)
{
}

CNetScheduleAdmin::EShutdownLevel COfflineJobContext::GetShutdownLevel()
{
    return CGridGlobals::GetInstance().GetShutdownLevel();
}

void COfflineJobContext::JobDelayExpiration(unsigned runtime_inc)
{
}

void COfflineJobContext::x_RunJob()
{
    m_RequestContext->SetRequestID((int) GetJobNumber());
    m_RequestContext->SetAppState(eDiagAppState_RequestBegin);

    CRequestContextSwitcher request_state_guard(m_RequestContext);

    if (g_IsRequestStartEventEnabled())
        GetDiagContext().PrintRequestStart().Print("jid", m_Job.job_id);

    m_RequestContext->SetAppState(eDiagAppState_Request);

    try {
        SetJobRetCode(m_WorkerNode.GetJobProcessor()->Do(*this));
    }
    catch (CGridWorkerNodeException& ex) {
        switch (ex.GetErrCode()) {
        case CGridWorkerNodeException::eJobIsCanceled:
            x_SetCanceled();
            break;

        case CGridWorkerNodeException::eExclusiveModeIsAlreadySet:
            if (IsLogRequested()) {
                LOG_POST_X(21, "Job " << GetJobKey() <<
                    " will be returned back to the queue "
                    "because it requested exclusive mode while "
                    "another job already has the exclusive status.");
            }
            if (!IsJobCommitted())
                ReturnJob();
            break;

        default:
            ERR_POST_X(62, ex);
            if (!IsJobCommitted())
                ReturnJob();
        }
    }
    catch (exception& e) {
        ERR_POST_X(18, "job" << GetJobKey() << " failed: " << e.what());
        if (!IsJobCommitted())
            CommitJobWithFailure(e.what());
    }

    CloseStreams();

    if (m_WorkerNode.IsExclusiveMode() && m_ExclusiveJob)
        m_WorkerNode.LeaveExclusiveMode();

    if (!m_OutputDirName.empty()) {
        CNetScheduleJobSerializer job_serializer(m_Job, m_CompoundIDPool);

        switch (GetCommitStatus()) {
        case eDone:
            job_serializer.SaveJobOutput(CNetScheduleAPI::eDone,
                    m_OutputDirName, m_NetCacheAPI);
            break;

        case eNotCommitted:
            CommitJobWithFailure("Job was not explicitly committed");
            /* FALL THROUGH */

        case eFailure:
            job_serializer.SaveJobOutput(CNetScheduleAPI::eFailed,
                    m_OutputDirName, m_NetCacheAPI);
            break;

        default: // eReturn, eRescheduled, eCanceled - results won't be saved
            break;
        }
    }

    x_PrintRequestStop();

    delete this;
}

int CGridWorkerNode::OfflineRun()
{
    x_WNCoreInit();

    m_QueueEmbeddedOutputSize = numeric_limits<size_t>().max();

    const CArgs& args = m_App.GetArgs();

    string input_dir_name(args["offline-input-dir"].AsString());
    string output_dir_name;

    if (args["offline-output-dir"])
        output_dir_name = args["offline-output-dir"].AsString();

    CDir input_dir(input_dir_name);

    auto_ptr<CDir::TEntries> dir_contents(input_dir.GetEntriesPtr(kEmptyStr,
            CDir::fIgnoreRecursive | CDir::fIgnorePath));

    if (dir_contents.get() == NULL) {
        ERR_POST("Cannot read input directory '" << input_dir_name << '\'');
        return 4;
    }

    m_Listener->OnGridWorkerStart();

    x_StartWorkerThreads();

    LOG_POST("Reading job input files (*.in)...");

    string path;
    if (!input_dir_name.empty())
        path = CDirEntry::AddTrailingPathSeparator(input_dir_name);

    unsigned job_count = 0;

    ITERATE(CDir::TEntries, it, *dir_contents) {
        CDirEntry& dir_entry = **it;

        string name = dir_entry.GetName();

        if (!NStr::EndsWith(name, ".in"))
            continue;

        dir_entry.Reset(CDirEntry::MakePath(path, name));

        if (dir_entry.IsFile()) {
            try {
                auto_ptr<CWorkerNodeJobContext> job_context(
                        new COfflineJobContext(*this, output_dir_name,
                        m_NetScheduleAPI.GetCompoundIDPool()));

                job_context->Reset();

                CNetScheduleJobSerializer job_serializer(job_context->GetJob(),
                        m_NetScheduleAPI.GetCompoundIDPool());

                job_serializer.LoadJobInput(dir_entry.GetPath());

                for (;;) {
                    try {
                        m_ThreadPool->WaitForRoom(m_ThreadPoolTimeout);
                        break;
                    }
                    catch (CBlockingQueueException&) {
                        if (CGridGlobals::GetInstance().IsShuttingDown())
                            break;
                    }
                }

                if (CGridGlobals::GetInstance().IsShuttingDown())
                    break;

                try {
                    m_ThreadPool->AcceptRequest(CRef<CStdRequest>(
                            new CWorkerNodeRequest(job_context.get())));
                } catch (CBlockingQueueException& ex) {
                    ERR_POST(ex);
                    // that must not happen after CBlockingQueue is fixed
                    _ASSERT(0);
                }

                job_context.release();

                if (CGridGlobals::GetInstance().IsShuttingDown())
                    break;
            } catch (exception& ex) {
                ERR_POST(ex.what());
                if (TWorkerNode_StopOnJobErrors::GetDefault())
                    CGridGlobals::GetInstance().RequestShutdown(
                            CNetScheduleAdmin::eShutdownImmediate);
            }
            ++job_count;
        }
    }

    x_StopWorkerThreads();

    if (job_count == 1) {
        LOG_POST("Processed 1 job input file.");
    } else {
        LOG_POST("Processed " << job_count << " job input files.");
    }

    return x_WNCleanUp();
}

END_NCBI_SCOPE
