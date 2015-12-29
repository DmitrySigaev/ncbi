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
 * Authors:  Maxim Didenko, Dmitry Kazimirov
 *
 * File Description:  NetSchedule worker node sample
 *
 */

#include <ncbi_pch.hpp>

#include "exec_helpers.hpp"

#include <connect/services/grid_globals.hpp>
#include <connect/services/remote_app.hpp>

#include <corelib/ncbiapp.hpp>
#include <corelib/ncbifile.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbiexec.hpp>

#include <vector>
#include <algorithm>

USING_NCBI_SCOPE;


static void s_SetParam(const CRemoteAppRequest& request,
                       CRemoteAppResult& result)
{
    result.SetStdOutErrFileNames(request.GetStdOutFileName(),
                                 request.GetStdErrFileName(),
                                 request.GetStdOutErrStorageType());
}

class CAppEnvHolder
{
public:
    CAppEnvHolder(const CRemoteAppLauncher& remote_app_launcher) :
        m_RemoteAppLauncher(remote_app_launcher)
    {}

    const vector<const char*>& GetEnv(const CWorkerNodeJobContext&);

private:
    const CRemoteAppLauncher& m_RemoteAppLauncher;
    list<string> m_EnvValues;
    list<string> m_CtxEnvValues;
    vector<const char*> m_Env;
};

const vector<const char*>& CAppEnvHolder::GetEnv(
        const CWorkerNodeJobContext& context)
{
    if (m_Env.empty()) {
        const CRemoteAppLauncher::TEnvMap& added_env =
                m_RemoteAppLauncher.GetAddedEnv();

        ITERATE(CRemoteAppLauncher::TEnvMap, it, added_env) {
            m_EnvValues.push_back(it->first + "=" +it->second);
        }
        list<string> names;
        const CNcbiEnvironment& env = m_RemoteAppLauncher.GetLocalEnv();
        env.Enumerate(names);
        ITERATE(list<string>, it, names) {
            if (added_env.find(*it) == added_env.end())
                m_EnvValues.push_back(*it + "=" + env.Get(*it));
        }
    } else {
        m_Env.clear();
    }

    m_CtxEnvValues.clear();
    m_CtxEnvValues.push_back("NCBI_NS_SERVICE=" +
            context.GetWorkerNode().GetServiceName());

    m_CtxEnvValues.push_back("NCBI_NS_QUEUE=" + context.GetQueueName());

    const CNetScheduleJob& job = context.GetJob();

    m_CtxEnvValues.push_back("NCBI_NS_JID=" + job.job_id);
    m_CtxEnvValues.push_back("NCBI_JOB_AFFINITY=" + job.affinity);

    if (!job.client_ip.empty()) {
        m_CtxEnvValues.push_back("NCBI_LOG_CLIENT_IP=" + job.client_ip);
    }

    if (!job.session_id.empty()) {
        m_CtxEnvValues.push_back("NCBI_LOG_SESSION_ID=" + job.session_id);
    }

    if (!job.page_hit_id.empty()) {
        m_CtxEnvValues.push_back("NCBI_LOG_HIT_ID=" + job.page_hit_id);
    }

    ITERATE(list<string>, it, m_CtxEnvValues) {
        m_Env.push_back(it->c_str());
    }

    ITERATE(list<string>, it, m_EnvValues) {
        m_Env.push_back(it->c_str());
    }

    m_Env.push_back(NULL);
    return m_Env;
}

///////////////////////////////////////////////////////////////////////

/// The remote_app NetSchedule job.
///
/// This class performs demarshalling of the command line arguments
/// and then executes the specified program.
///
class CRemoteAppJob : public IWorkerNodeJob
{
public:
    CRemoteAppJob(const IWorkerNodeInitContext& context,
            const CRemoteAppLauncher& remote_app_launcher);

    virtual ~CRemoteAppJob() {}

    int Do(CWorkerNodeJobContext& context);

private:
    CNetCacheAPI m_NetCacheAPI;
    const CRemoteAppLauncher& m_RemoteAppLauncher;
    CAppEnvHolder m_AppEnvHolder;
};

int CRemoteAppJob::Do(CWorkerNodeJobContext& context)
{
    if (context.IsLogRequested()) {
        LOG_POST(Note << context.GetJobKey() << " is received.");
    }

    CRemoteAppRequest m_Request(m_NetCacheAPI);
    CRemoteAppResult m_Result(m_NetCacheAPI);

    try {
        m_Request.Deserialize(context.GetIStream());
    }
    catch (exception&) {
        ERR_POST("Cannot deserialize remote_app job");
        context.CommitJobWithFailure("Error while "
                "unpacking remote_app arguments");
        return -1;
    }

    s_SetParam(m_Request, m_Result);
    size_t output_size = context.GetWorkerNode().GetServerOutputSize();
    if (output_size == 0) {
        // NetSchedule internal storage is not supported; all
        // output will be stored in NetCache anyway, so it can
        // be put into one blob.
        output_size = kMax_UInt;
    } else {
        // Empiric estimation of the maximum output size
        // (reduction by 10%).
        output_size = output_size - output_size / 10;
    }
    m_Result.SetMaxInlineSize(output_size);

    if (context.IsLogRequested()) {
        if (!m_Request.GetInBlobIdOrData().empty()) {
            LOG_POST(Note << context.GetJobKey()
                << " Input data: " << m_Request.GetInBlobIdOrData());
        }
        LOG_POST(Note << context.GetJobKey()
            << " Args: " << m_Request.GetCmdLine());
        if (!m_Request.GetStdOutFileName().empty()) {
            LOG_POST(Note << context.GetJobKey()
                << " StdOutFile: " << m_Request.GetStdOutFileName());
        }
        if (!m_Request.GetStdErrFileName().empty()) {
            LOG_POST(Note << context.GetJobKey()
                << " StdErrFile: " << m_Request.GetStdErrFileName());
        }
    }

    vector<string> args;
    TokenizeCmdLine(m_Request.GetCmdLine(), args);


    int ret = -1;
    bool finished_ok = false;
    try {
        vector<const char*> env(m_AppEnvHolder.GetEnv(context));

        finished_ok = m_RemoteAppLauncher.ExecRemoteApp(args,
                                    m_Request.GetStdInForRead(),
                                    m_Result.GetStdOutForWrite(),
                                    m_Result.GetStdErrForWrite(),
                                    ret,
                                    context,
                                    m_Request.GetAppRunTimeout(),
                                    &env[0]);
    } catch (...) {
        m_Request.Reset();
        m_Result.Reset();
        throw;
    }

    m_Result.SetRetCode(ret);
    m_Result.Serialize(context.GetOStream());

    if (!finished_ok) {
        if (!context.IsJobCommitted())
            context.CommitJobWithFailure("Job has been canceled");
    } else
        if (m_RemoteAppLauncher.MustFailNoRetries(ret))
            context.CommitJobWithFailure(
                    "Exited with return code " + NStr::IntToString(ret) +
                    " - will not be rerun",
                    true /* no retries */);
        else if (ret == 0 || m_RemoteAppLauncher.GetNonZeroExitAction() ==
                CRemoteAppLauncher::eDoneOnNonZeroExit)
            context.CommitJob();
        else if (m_RemoteAppLauncher.GetNonZeroExitAction() ==
                CRemoteAppLauncher::eReturnOnNonZeroExit)
            context.ReturnJob();
        else
            context.CommitJobWithFailure(
                    "Exited with return code " + NStr::IntToString(ret));

    if (context.IsLogRequested()) {
        LOG_POST(Note << "Job " << context.GetJobKey() <<
                " is " << context.GetCommitStatusDescription(
                        context.GetCommitStatus()) <<
                ". Exit code: " << ret);
        if (!m_Result.GetErrBlobIdOrData().empty()) {
            LOG_POST(Note << context.GetJobKey() << " Err data: " <<
                m_Result.GetErrBlobIdOrData());
        }
    }
    m_Request.Reset();
    m_Result.Reset();
    return ret;
}

CRemoteAppJob::CRemoteAppJob(const IWorkerNodeInitContext& context,
        const CRemoteAppLauncher& remote_app_launcher) :
    m_NetCacheAPI(context.GetNetCacheAPI()),
    m_RemoteAppLauncher(remote_app_launcher),
    m_AppEnvHolder(remote_app_launcher)
{
    CGridGlobals::GetInstance().SetReuseJobObject(true);
}

class CRemoteAppIdleTask : public IWorkerNodeIdleTask
{
public:
    CRemoteAppIdleTask(const string& idle_app_cmd) : m_AppCmd(idle_app_cmd)
    {
    }
    virtual ~CRemoteAppIdleTask() {}

    virtual void Run(CWorkerNodeIdleTaskContext& context)
    {
        if (!m_AppCmd.empty())
            CExec::System(m_AppCmd.c_str());
    }
private:
    string m_AppCmd;
};

class CRemoteAppListener : public CRemoteAppBaseListener
{
public:
    CRemoteAppListener(const TLauncherPtr& launcher)
        : CRemoteAppBaseListener(launcher) {}

    virtual void OnInit(CNcbiApplication* app);
};

void CRemoteAppListener::OnInit(CNcbiApplication* app)
{
    CNcbiRegistry& reg = app->GetConfig();

    if (!reg.HasEntry("log", "merge_lines"))
        reg.Set("log", "merge_lines", "true");
}


/////////////////////////////////////////////////////////////////////////////

#define GRID_APP_NAME "remote_app"

class CRemoteAppJobFactory : public IWorkerNodeJobFactory
{
public:
    virtual void Init(const IWorkerNodeInitContext& context)
    {
        m_RemoteAppLauncher.reset(
                new CRemoteAppLauncher("remote_app", context.GetConfig()));

        CFile file(m_RemoteAppLauncher->GetAppPath());

        if (!file.Exists()) {
            NCBI_THROW_FMT(CException, eInvalid, "File \"" <<
                    m_RemoteAppLauncher->GetAppPath() <<
                    "\" does not exists.");
        }

        if (!CRemoteAppLauncher::CanExec(file)) {
            NCBI_THROW_FMT(CException, eInvalid, "File \"" <<
                    m_RemoteAppLauncher->GetAppPath() <<
                    "\" is not executable.");
        }

        m_WorkerNodeInitContext = &context;
        string idle_app_cmd = context.GetConfig().GetString("remote_app",
                "idle_app_cmd", kEmptyStr);
        if (!idle_app_cmd.empty())
            m_IdleTask.reset(new CRemoteAppIdleTask(idle_app_cmd));
    }
    virtual IWorkerNodeJob* CreateInstance(void)
    {
        return new CRemoteAppJob(*m_WorkerNodeInitContext, *m_RemoteAppLauncher);
    }
    virtual IWorkerNodeIdleTask* GetIdleTask() { return m_IdleTask.get(); }

    virtual string GetJobVersion() const
    {
        return GRID_APP_VERSION_INFO;
    }
    virtual string GetAppName() const
    {
        return GRID_APP_NAME;
    }
    virtual string GetAppVersion() const
    {
        _ASSERT(m_RemoteAppLauncher.get());
        return m_RemoteAppLauncher->GetAppVersion(GRID_APP_VERSION);
    }

    CRemoteAppListener* CreateListener() const
    {
        return new CRemoteAppListener(m_RemoteAppLauncher);
    }

private:
    auto_ptr<CRemoteAppLauncher> m_RemoteAppLauncher;
    const IWorkerNodeInitContext* m_WorkerNodeInitContext;
    auto_ptr<CRemoteAppIdleTask> m_IdleTask;
};

int main(int argc, const char* argv[])
{
    GRID_APP_CHECK_VERSION_ARGS();
    return Main<CRemoteAppJobFactory, CRemoteAppListener>(argc, argv);
}
