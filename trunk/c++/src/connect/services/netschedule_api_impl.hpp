#ifndef CONN_SERVICES___NETSCHEDULE_API_IMPL__HPP
#define CONN_SERVICES___NETSCHEDULE_API_IMPL__HPP

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
 * Authors:  Anatoliy Kuznetsov, Maxim Didenko, Victor Joukov, Dmitry Kazimirov
 *
 * File Description:
 *   NetSchedule client API implementation details.
 *
 */

#include "netservice_api_impl.hpp"

#include <connect/services/netschedule_api.hpp>
#include <connect/services/error_codes.hpp>


BEGIN_NCBI_SCOPE


////////////////////////////////////////////////////////////////////////////////////////
//

#define SERVER_PARAMS_ASK_MAX_COUNT 100

bool g_ParseGetJobResponse(CNetScheduleJob& job, const string& response);

inline string g_MakeBaseCmd(const string& cmd_name, const string& job_key)
{
    string cmd(cmd_name);
    cmd += ' ';
    cmd += job_key;
    return cmd;
}

class CNetScheduleServerListener : public INetServerConnectionListener
{
public:
    CNetScheduleServerListener() : m_WorkerNodeCompatMode(false) {}

    void SetAuthString(SNetScheduleAPIImpl* impl);

    virtual void OnInit(CObject* api_impl,
        CConfig* config, const string& config_section);
    virtual void OnConnected(CNetServerConnection::TInstance conn);
    virtual void OnError(const string& err_msg, SNetServerImpl* server);
    virtual void OnWarning(const string& warn_msg, SNetServerImpl* server);

    string m_Auth;
    CRef<CNetScheduleAPI::IEventHandler> m_EventHandler;

    typedef map<string, SNetServerInPool*> TNodeIdToServerMap;
    TNodeIdToServerMap m_ServerByNSNodeId;
    CFastMutex m_FastMutex;

    bool m_WorkerNodeCompatMode;
};

struct SNetScheduleAPIImpl : public CObject
{
    SNetScheduleAPIImpl(CConfig* config, const string& section,
        const string& service_name, const string& client_name,
        const string& queue_name);

    // Special constructor for CNetScheduleAPI::GetServer().
    SNetScheduleAPIImpl(SNetServerInPool* server, SNetScheduleAPIImpl* parent);

    CNetScheduleServerListener* GetListener()
    {
        return static_cast<CNetScheduleServerListener*>(
                m_Service->m_Listener.GetPointer());
    }

    string x_SendJobCmdWaitResponse(const string& cmd_name, const string& job_key)
    {
        string cmd(g_MakeBaseCmd(cmd_name, job_key));
        g_AppendClientIPAndSessionID(cmd);
        return GetServer(job_key).ExecWithRetry(cmd).response;
    }

    CNetScheduleAPI::EJobStatus GetJobStatus(const string& cmd,
            const string& job_key, time_t* job_exptime);

    const CNetScheduleAPI::SServerParams& GetServerParams();

    CNetServer GetServer(const string& job_key)
    {
        CNetScheduleKey nskey(job_key);
        return m_Service->GetServer(g_NetService_gethostbyname(nskey.host),
                nskey.port);
    }

    static void VerifyJobGroupAlphabet(const string& job_group)
    {
        g_VerifyAlphabet(job_group, "job group name", eCC_BASE64_PI);
    }

    static void VerifyAuthTokenAlphabet(const string& auth_token)
    {
        g_VerifyAlphabet(auth_token, "security token", eCC_BASE64_PI);
    }

    static void VerifyAffinityAlphabet(const string& affinity)
    {
        g_VerifyAlphabet(affinity, "affinity token", eCC_BASE64_PI);
    }

    CNetService m_Service;

    static CNetScheduleExceptionMap sm_ExceptionMap;
    string m_Queue;
    string m_ProgramVersion;
    string m_ClientNode;
    string m_ClientSession;

    typedef map<string, string> TAuthParams;
    TAuthParams m_AuthParams;

    auto_ptr<CNetScheduleAPI::SServerParams> m_ServerParams;
    long m_ServerParamsAskCount;
    CFastMutex m_FastMutex;

    CNetScheduleExecutor::EJobAffinityPreference m_AffinityPreference;
    bool m_UseEmbeddedStorage;
};


struct SNetScheduleSubmitterImpl : public CObject
{
    SNetScheduleSubmitterImpl(CNetScheduleAPI::TInstance ns_api_impl);

    string SubmitJobImpl(CNetScheduleJob& job, unsigned short udp_port,
            unsigned wait_time, CNetServer* server = NULL);

    void FinalizeRead(const char* cmd_start,
        const char* cmd_name,
        const string& job_id,
        const string& auth_token,
        const string& error_message);

    CNetScheduleAPI m_API;
};

inline SNetScheduleSubmitterImpl::SNetScheduleSubmitterImpl(
    CNetScheduleAPI::TInstance ns_api_impl) :
    m_API(ns_api_impl)
{
}

struct SNetScheduleExecutorImpl : public CObject
{
    SNetScheduleExecutorImpl(CNetScheduleAPI::TInstance ns_api_impl) :
        m_API(ns_api_impl),
        m_AffinityPreference(ns_api_impl->m_AffinityPreference),
        m_WorkerNodeMode(false)
    {
    }

    void ExecWithOrWithoutRetry(const string& job_key, const string& cmd);

    enum EChangeAffAction {
        eAddAffs,
        eDeleteAffs
    };
    int AppendAffinityTokens(string& cmd,
            const vector<string>* affs, EChangeAffAction action);

    CNetScheduleAPI m_API;

    CNetScheduleExecutor::EJobAffinityPreference m_AffinityPreference;

    CNetScheduleNotificationHandler m_NotificationHandler;

    CFastMutex m_PreferredAffMutex;
    set<string> m_PreferredAffinities;
    bool m_WorkerNodeMode;
};

struct SNetScheduleAdminImpl : public CObject
{
    SNetScheduleAdminImpl(CNetScheduleAPI::TInstance ns_api_impl);

    typedef map<pair<string, unsigned int>, string> TIDsMap;

    CNetScheduleAPI m_API;
};

inline SNetScheduleAdminImpl::SNetScheduleAdminImpl(
    CNetScheduleAPI::TInstance ns_api_impl) :
    m_API(ns_api_impl)
{
}

END_NCBI_SCOPE


#endif  /* CONN_SERVICES___NETSCHEDULE_API_IMPL__HPP */
