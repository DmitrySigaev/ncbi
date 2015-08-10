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

#include <list>
#include <map>


BEGIN_NCBI_SCOPE


////////////////////////////////////////////////////////////////////////////////
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

struct SNetScheduleServerProperties : public INetServerProperties
{
    SNetScheduleServerProperties() :
        affs_synced(false)
    {
    }

    CFastMutex m_Mutex;

    string ns_node;
    string ns_session;

    bool affs_synced;
};

class CNetScheduleConfigLoader
{
    class CErrorSuppressor;

public:
    CNetScheduleConfigLoader(
            const CTempString& qinf2_prefix,
            const CTempString& qinf2_section,
            const CTempString& getp2_prefix,
            const CTempString& getp2_section);

    CConfig* Get(SNetScheduleAPIImpl* impl, CConfig* config, string& section);

protected:
    virtual bool Transform(const CTempString& prefix, string& name);

private:
    typedef CNetScheduleAPI::TQueueParams TParams;
    CConfig* Parse(const TParams& params, const CTempString& prefix);

    const CTempString m_Qinf2Prefix;
    const CTempString m_Qinf2Section;
    const CTempString m_Getp2Prefix;
    const CTempString m_Getp2Section;
};

class CNetScheduleOwnConfigLoader : public CNetScheduleConfigLoader
{
public:
    CNetScheduleOwnConfigLoader();

protected:
    bool Transform(const CTempString& prefix, string& name);
};

class CNetScheduleServerListener : public INetServerConnectionListener
{
public:
    CNetScheduleServerListener() :
        m_ClientType(CNetScheduleAPI::eCT_Auto),
        m_WorkerNodeCompatMode(false)
    {
    }

    void SetAuthString(SNetScheduleAPIImpl* impl);

    bool NeedToSubmitAffinities(SNetServerImpl* server_impl);
    void SetAffinitiesSynced(SNetServerImpl* server_impl, bool affs_synced);

    CRef<SNetScheduleServerProperties> x_GetServerProperties(
            SNetServerImpl* server_impl);

    virtual CRef<INetServerProperties> AllocServerProperties();

    virtual void OnInit(CObject* api_impl,
        CConfig* config, const string& config_section);
    virtual void OnConnected(CNetServerConnection& connection);
    virtual void OnError(const string& err_msg, CNetServer& server);
    virtual void OnWarning(const string& warn_msg, CNetServer& server);

    string m_Auth;
    CRef<INetEventHandler> m_EventHandler;

    CFastMutex m_ServerByNodeMutex;
    typedef map<string, SNetServerInPool*> TServerByNode;
    TServerByNode m_ServerByNode;

    // Make sure the worker node does not attempt to submit its
    // preferred affinities from two threads.
    CFastMutex m_AffinitySubmissionMutex;

    CNetScheduleAPI::EClientType m_ClientType;
    bool m_WorkerNodeCompatMode;
};

// Structure that governs NetSchedule server notifications.
struct SServerNotifications
{
    SServerNotifications() :
        m_NotificationSemaphore(0, 1),
        m_Interrupted(false)
    {
    }

    bool Wait(const CDeadline& deadline)
    {
        return m_NotificationSemaphore.TryWait(deadline.GetRemainingTime());
    }

    void InterruptWait();

    void RegisterServer(const string& ns_node);

    bool GetNextNotification(string* ns_node);

private:
    void x_ClearInterruptFlag()
    {
        if (m_Interrupted) {
            m_Interrupted = false;
            m_NotificationSemaphore.TryWait();
        }
    }
    // Semaphore that the worker node or the job reader can wait on.
    // If count=0 then m_ReadyServers is empty and m_Interrupted=false;
    // if count=1 then at least one server is ready or m_Interrupted=true.
    CSemaphore m_NotificationSemaphore;
    // Protection against concurrent access to m_ReadyServers.
    CFastMutex m_Mutex;
    // A set of NetSchedule node IDs of the servers that are ready.
    // (i.e. the servers have sent notifications).
    typedef set<string> TReadyServers;
    TReadyServers m_ReadyServers;
    // This flag is set when the wait must be interrupted.
    bool m_Interrupted;
};

struct SNetScheduleNotificationThread : public CThread
{
    SNetScheduleNotificationThread(SNetScheduleAPIImpl* ns_api);

    enum ENotificationType {
        eNT_GetNotification,
        eNT_ReadNotification,
        eNT_Unknown,
    };
    ENotificationType CheckNotification(string* ns_node);

    virtual void* Main();

    unsigned short GetPort() const {return m_UDPPort;}

    const string& GetMessage() const {return m_Message;}

    void PrintPortNumber();

    void CmdAppendPortAndTimeout(string* cmd, const CDeadline& deadline);

    SNetScheduleAPIImpl* m_API;

    CDatagramSocket m_UDPSocket;
    unsigned short m_UDPPort;

    char m_Buffer[1024];
    string m_Message;

    bool m_StopThread;

    SServerNotifications m_GetNotifications;
    SServerNotifications m_ReadNotifications;
};

struct SNetScheduleAPIImpl : public CObject
{
    SNetScheduleAPIImpl(CConfig* config, const string& section,
        const string& service_name, const string& client_name,
        const string& queue_name);

    // Special constructor for CNetScheduleAPI::GetServer().
    SNetScheduleAPIImpl(SNetServerInPool* server, SNetScheduleAPIImpl* parent);

    ~SNetScheduleAPIImpl();

    CNetScheduleServerListener* GetListener()
    {
        return static_cast<CNetScheduleServerListener*>(
                m_Service->m_Listener.GetPointer());
    }

    string x_ExecOnce(const string& cmd_name, const CNetScheduleJob& job)
    {
        string cmd(g_MakeBaseCmd(cmd_name, job.job_id));
        g_AppendClientIPSessionIDHitID(cmd);

        CNetServer::SExecResult exec_result;

        GetServer(job)->ConnectAndExec(cmd, false, exec_result);

        return exec_result.response;
    }

    CNetScheduleAPI::EJobStatus GetJobStatus(const string& cmd,
            const CNetScheduleJob& job, time_t* job_exptime,
            ENetScheduleQueuePauseMode* pause_mode);

    const CNetScheduleAPI::SServerParams& GetServerParams();

    typedef CNetScheduleAPI::TQueueParams TQueueParams;
    void GetQueueParams(const string& queue_name, TQueueParams& queue_params);
    void GetQueueParams(TQueueParams& queue_params);

    CNetServer GetServer(const string& job_key)
    {
        CNetScheduleKey nskey(job_key, m_CompoundIDPool);
        return m_Service.GetServer(nskey.host, nskey.port);
    }

    CNetServer GetServer(const CNetScheduleJob& job)
    {
        return job.server != NULL ? job.server : GetServer(job.job_id);
    }

    bool GetServerByNode(const string& ns_node, CNetServer* server);

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

    static void VerifyQueueNameAlphabet(const string& queue_name);

    void AllocNotificationThread();
    void StartNotificationThread();

    // Unregister client-listener. After this call, the
    // server will not try to send any notification messages or
    // maintain job affinity for the client.
    void x_ClearNode();

    CNetService m_Service;

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
    list<string> m_AffinityList;
    list<string> m_AffinityLadder;

    string m_JobGroup;
    unsigned m_JobTtl;

    bool m_UseEmbeddedStorage;

    CCompoundIDPool m_CompoundIDPool;

    CFastMutex m_NotificationThreadMutex;
    CRef<SNetScheduleNotificationThread> m_NotificationThread;
    CAtomicCounter_WithAutoInit m_NotificationThreadStartStopCounter;
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
        m_JobGroup(ns_api_impl->m_JobGroup),
        m_WorkerNodeMode(false)
    {
        copy(ns_api_impl->m_AffinityList.begin(),
             ns_api_impl->m_AffinityList.end(),
             inserter(m_PreferredAffinities, m_PreferredAffinities.end()));
    }

    void ClaimNewPreferredAffinity(CNetServer orig_server,
        const string& affinity);
    string MkSETAFFCmd();
    bool ExecGET(SNetServerImpl* server,
            const string& get_cmd, CNetScheduleJob& job);
    bool x_GetJobWithAffinityList(SNetServerImpl* server,
            const CDeadline* timeout,
            CNetScheduleJob& job,
            CNetScheduleExecutor::EJobAffinityPreference affinity_preference,
            const string& affinity_list);
    bool x_GetJobWithAffinityLadder(SNetServerImpl* server,
            const CDeadline& timeout,
            CNetScheduleJob& job);

    void ExecWithOrWithoutRetry(const CNetScheduleJob& job, const string& cmd);

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

    string m_JobGroup;

    // True when this object is used by a real
    // worker node application (CGridWorkerNode).
    bool m_WorkerNodeMode;
};

class CNetScheduleGETCmdListener : public INetServerExecListener
{
public:
    CNetScheduleGETCmdListener(SNetScheduleExecutorImpl* executor) :
        m_Executor(executor)
    {
    }

    virtual void OnExec(CNetServerConnection::TInstance conn_impl,
            const string& cmd);

    SNetScheduleExecutorImpl* m_Executor;
};

namespace NNetScheduleGetJob
{
    enum EState {
        eWorking,
        eRestarted,
        eStopped
    };

    enum EResult {
        eJob,
        eAgain,
        eInterrupt,
        eNoJobs
    };

    struct SEntry
    {
        SServerAddress server_address;
        CDeadline deadline;
        bool more_jobs;

        SEntry(const SServerAddress& a, bool j = true) :
            server_address(a),
            deadline(0, 0),
            more_jobs(j)
        {
        }

        bool operator==(const SEntry& rhs) const
        {
            return server_address == rhs.server_address;
        }
    };
};

template <class TImpl>
class CNetScheduleGetJob
{
    typedef list<SServerAddress> TServers;

public:
    typedef NNetScheduleGetJob::SEntry SEntry;
    typedef list<SEntry> TTimeline;

    CNetScheduleGetJob(TImpl& impl) :
        m_Impl(impl),
        m_DiscoveryAction(SServerAddress(0, 0), false)
    {
        m_ImmediateActions.push_back(m_DiscoveryAction);
    }

    // TODO: This can be replaced by lambda after we migrate to C++11
    struct SEntryHasMoreJobs
    {
        TImpl& impl;
        SEntryHasMoreJobs(TImpl& i) : impl(i) {}
        bool operator()(const SEntry& entry) { return impl.MoreJobs(entry); }
    };

    NNetScheduleGetJob::EResult GetJobImmediately(
            CNetScheduleJob& job,
            CNetScheduleAPI::EJobStatus* job_status)
    {
        for (;;) {
            NNetScheduleGetJob::EState state = m_Impl.CheckState();

            if (state == NNetScheduleGetJob::eStopped) {
                return NNetScheduleGetJob::eInterrupt;
            }
            
            if (state == NNetScheduleGetJob::eRestarted) {
                // Restart (rediscover all servers)
                m_ScheduledActions.clear();
                m_ImmediateActions.clear();
                m_ImmediateActions.push_back(m_DiscoveryAction);
            }

            if (m_ImmediateActions.empty()) {
                break;
            }

            SEntry timeline_entry(m_ImmediateActions.front());

            if (timeline_entry == m_DiscoveryAction) {
                NextDiscoveryIteration();
                timeline_entry.deadline = CDeadline(m_Impl.m_Timeout, 0);
                MoveHead(m_ImmediateActions, m_ScheduledActions);
            } else {
                try {
                    if (m_Impl.CheckEntry(timeline_entry, job, job_status)) {
                        // A job has been returned; add the server to
                        // immediate actions because there can be more
                        // jobs in the queue.
                        return NNetScheduleGetJob::eJob;
                    } else {
                        // No job has been returned by this server;
                        // query the server later.
                        timeline_entry.deadline = CDeadline(m_Impl.m_Timeout, 0);
                        MoveHead(m_ImmediateActions, m_ScheduledActions);
                    }
                }
                catch (CNetSrvConnException& e) {
                    // Because a connection error has occurred, do not
                    // put this server back to the timeline.
                    m_ImmediateActions.pop_front();
                    LOG_POST(Warning << e.GetMsg());
                }
            }

            // Check all servers that have timeout expired
            while (!m_ScheduledActions.empty() &&
                    m_ScheduledActions.front().deadline.GetRemainingTime().IsZero()) {
                MoveHead(m_ScheduledActions, m_ImmediateActions);
            }

            // Check if there's a notification in the UDP socket.
            while (CNetServer server = m_Impl.ReadNotifications()) {
                MoveToImmediateActions(server);
            }
        }

        return NNetScheduleGetJob::eAgain;
    }

    NNetScheduleGetJob::EResult GetJob(
            const CDeadline& deadline,
            CNetScheduleJob& job,
            CNetScheduleAPI::EJobStatus* job_status)
    {
        for (;;) {
            NNetScheduleGetJob::EResult ret = GetJobImmediately(job, job_status);

            if (ret != NNetScheduleGetJob::eAgain) {
                return ret;
            }

            // If MoreJobs() returned false for all entries of m_ScheduledActions
            if (find_if(m_ScheduledActions.begin(), m_ScheduledActions.end(),
                        SEntryHasMoreJobs(m_Impl)) == m_ScheduledActions.end()) {
                return NNetScheduleGetJob::eNoJobs;
            }

            if (deadline.IsExpired())
                return NNetScheduleGetJob::eAgain;

            // At least, the discovery action must be there
            _ASSERT(!m_ScheduledActions.empty());

            // There's still time. Wait for notifications and query the servers.
            CDeadline next_event_time = m_ScheduledActions.front().deadline;
            bool last_wait = deadline < next_event_time;
            if (last_wait) next_event_time = deadline;

            if (CNetServer server = m_Impl.WaitForNotifications(next_event_time)) {
                do {
                    MoveToImmediateActions(server);
                } while (server = m_Impl.ReadNotifications());
            } else if (last_wait) {
                return NNetScheduleGetJob::eAgain;
            } else {
                MoveHead(m_ScheduledActions, m_ImmediateActions);
            }
        }
    }

    TImpl& m_Impl;

private:
    static void MoveHead(TTimeline& from, TTimeline& to)
    {
        to.splice(to.end(), from, from.begin());
    }

    static void Filter(TTimeline& timeline, TServers& servers)
    {
        TTimeline new_timeline;

        for (TTimeline::const_iterator i = timeline.begin();
                i != timeline.end(); ++i) {
            const SServerAddress& address(i->server_address);
            TServers::iterator j = find(servers.begin(), servers.end(), address);

            if (j != servers.end()) {
                new_timeline.push_back(*i);
                servers.erase(j);
            }
        }

        timeline.swap(new_timeline);
    }

    void MoveToImmediateActions(SNetServerImpl* server_impl)
    {
        SEntry entry(server_impl->m_ServerInPool->m_Address);

        TTimeline::iterator i = find(m_ScheduledActions.begin(),
                m_ScheduledActions.end(), entry);

        // Server was postponed, move to immediate
        if (i != m_ScheduledActions.end()) {
            m_ImmediateActions.splice(m_ImmediateActions.end(),
                    m_ScheduledActions, i);
            return;
        }

        TTimeline::iterator j = find(m_ImmediateActions.begin(),
                m_ImmediateActions.end(), entry);

        // It's new server, add to immediate
        if (i == m_ImmediateActions.end()) {
            m_ImmediateActions.push_back(entry);
        }
    }

    void NextDiscoveryIteration()
    {
        TServers servers;

        for (CNetServiceIterator it =
                m_Impl.m_API.GetService().Iterate(
                    CNetService::eIncludePenalized); it; ++it) {
            servers.push_back((*it)->m_ServerInPool->m_Address);
        }

        // Keep up to date servers
        Filter(m_ImmediateActions, servers);
        Filter(m_ScheduledActions, servers);

        // Add newly discovered servers
        for (TServers::const_iterator i = servers.begin();
                i != servers.end(); ++i) {
            m_ImmediateActions.push_back(*i);
        }
    }

    TTimeline m_ImmediateActions, m_ScheduledActions;
    SEntry m_DiscoveryAction;
};

const unsigned s_Timeout = 10;

struct SNetScheduleJobReaderImpl : public CObject
{
    SNetScheduleJobReaderImpl(CNetScheduleAPI::TInstance ns_api_impl,
            const string& group, const string& affinity) :
        m_Impl(ns_api_impl, group, affinity),
        m_Timeline(m_Impl)
    {
    }

    void x_StartNotificationThread()
    {
        m_Impl.m_API->StartNotificationThread();
    }

    void SetJobGroup(const string& group_name);
    void SetAffinity(const string& affinity);
    CNetScheduleJobReader::EReadNextJobResult ReadNextJob(
        CNetScheduleJob* job,
        CNetScheduleAPI::EJobStatus* job_status,
        const CTimeout* timeout);
    void InterruptReading();

private:
    class CImpl
    {
    public:
        CImpl(CNetScheduleAPI::TInstance ns_api_impl,
                const string& group, const string& affinity) :
            m_API(ns_api_impl),
            m_Timeout(s_Timeout),
            m_JobGroup(group),
            m_Affinity(affinity),
            m_MoreJobs(false)
        {
            SNetScheduleAPIImpl::VerifyJobGroupAlphabet(group);
            SNetScheduleAPIImpl::VerifyAffinityAlphabet(affinity);
        }

        NNetScheduleGetJob::EState CheckState();
        CNetServer ReadNotifications();
        CNetServer WaitForNotifications(const CDeadline& deadline);
        bool MoreJobs(const NNetScheduleGetJob::SEntry& entry);
        bool CheckEntry(
                NNetScheduleGetJob::SEntry& entry,
                CNetScheduleJob& job,
                CNetScheduleAPI::EJobStatus* job_status);

        CNetScheduleAPI m_API;
        const unsigned m_Timeout;
        string m_JobGroup;
        string m_Affinity;

    private:
        CNetServer x_ProcessReadJobNotifications();
        bool x_ReadJob(SNetServerImpl* server,
                const CDeadline& timeout,
                CNetScheduleJob& job,
                CNetScheduleAPI::EJobStatus* job_status,
                bool* no_more_jobs);

        bool m_MoreJobs;
    };

    CImpl m_Impl;
    CNetScheduleGetJob<CImpl> m_Timeline;
};

struct SNetScheduleAdminImpl : public CObject
{
    SNetScheduleAdminImpl(CNetScheduleAPI::TInstance ns_api_impl) :
        m_API(ns_api_impl)
    {
    }

    typedef map<pair<string, unsigned int>, string> TIDsMap;

    CNetScheduleAPI m_API;
};

END_NCBI_SCOPE


#endif  /* CONN_SERVICES___NETSCHEDULE_API_IMPL__HPP */
