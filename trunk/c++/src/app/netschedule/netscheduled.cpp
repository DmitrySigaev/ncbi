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
 * File Description: Network scheduler daemon
 *
 */

#include <ncbi_pch.hpp>
#include <stdio.h>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbireg.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbistr.hpp>
#include <corelib/ncbimisc.hpp>
#include <corelib/ncbi_config.hpp>
#include <corelib/ncbimtx.hpp>

#include <corelib/ncbidiag.hpp>
#include <corelib/request_ctx.hpp> // FIXME: remove

#include <util/bitset/ncbi_bitset.hpp>

#include <connect/ncbi_core_cxx.hpp>
#include <connect/server.hpp>
#include <connect/ncbi_socket.hpp>
#include <connect/services/netservice_protocol_parser.hpp>
#include <connect/ncbi_conn_stream.hpp>

#include <bdb/bdb_expt.hpp>

#include "bdb_queue.hpp"
#include "ns_types.hpp"
#include "ns_util.hpp"
#include "job_status.hpp"
#include "access_list.hpp"
#include "background_host.hpp"
#include "worker_node.hpp"

#include "netschedule_version.hpp"

#define NETSCHEDULED_FEATURES \
    "fast_status=1;dyn_queues=1;tags=1;read_confirm=1;version=" NETSCHEDULED_VERSION

#if defined(NCBI_OS_UNIX)
# include <corelib/ncbi_process.hpp>
# include <signal.h>
#endif


USING_NCBI_SCOPE;

#define NETSCHEDULED_FULL_VERSION \
    "NCBI NetSchedule server Version " NETSCHEDULED_VERSION \
    " Storage version " NETSCHEDULED_STORAGE_VERSION \
    " Protocol version " NETSCHEDULED_PROTOCOL_VERSION \
    " build " __DATE__ " " __TIME__

class CNetScheduleServer;
static CNetScheduleServer* s_netschedule_server = 0;

struct SJS_Request
{
    string    input;
    string    output;
    string    affinity_token;
    string    job_key;
    int       job_return_code;
    unsigned  port;
    unsigned  timeout;
    unsigned  job_mask;
    unsigned  count;
    string    err_msg;
    string    param1;
    string    param2;
    string    param3;
    string    tags;

    void Init()
    {
        input.erase(); output.erase();
        affinity_token.erase();
        job_key.erase(); err_msg.erase();
        param1.erase(); param2.erase(); param3.erase();
        job_return_code = port = timeout = job_mask = 0;
    }
    void SetParamFields(map<string, string>& params)
    {
        typedef map<string, string> TStrMap;
        NON_CONST_ITERATE(TStrMap, it, params) {
            const string& key = it->first;
            string& val = it->second;
            if (key.size() == 0) {
                continue;
            }

            if (val.size() > kNetScheduleMaxDBDataSize - 1) {
                val.resize(kNetScheduleMaxDBDataSize - 1);
            }
            switch (key[0]) {
            case 'a':
                if (key == "aff") {
                    affinity_token = val;
                }
                else if (key == "affp") {
                    param1 = val;
                }
                else if (key == "action") {
                    param2 = val;
                }
                break;
            case 'c':
                if (key == "count") {
                    count = NStr::StringToUInt(val, NStr::fConvErr_NoThrow);
                }
                else if (key == "comment") {
                    param3 = val;
                }
                break;
            case 'e':
                if (key == "err_msg") {
                    err_msg = val;
                }
                break;
            case 'f':
                if (key == "fields") {
                    param3 = val;
                }
                break;
            case 'g':
                if (key == "guid") {
                    param1 = val;
                }
                break;
            case 'i':
                if (key == "input") {
                    input = val;
                }
                else if (key == "ip") {
                    param3 = val;
                }
                break;
            case 'j':
                if (key == "job_key") {
                    job_key = val;
                }
                else if (key == "job_return_code") {
                    job_return_code =
                            NStr::StringToInt(val, NStr::fConvErr_NoThrow);
                }
                break;
            case 'm':
                if (key == "msk") {
                    job_mask = NStr::StringToUInt(val, NStr::fConvErr_NoThrow);
                }
                break;
            case 'o':
                if (key == "output") {
                    output = val;
                }
                else if (key == "option") {
                    param1 = val;
                }
                break;
            case 'p':
                if (key == "port") {
                    port = NStr::StringToUInt(val, NStr::fConvErr_NoThrow);
                }
                else if (key == "progress_msg") {
                    param1 = val;
                }
                break;
            case 'q':
                if (key == "qname") {
                    param1 = val;
                }
                else if (key == "qclass") {
                    param2 = val;
                }
                break;
            case 's':
                if (key == "status") {
                    param1 = val;
                }
                else if (key == "select") {
                    param1 = val;
                }
                else if (key == "sid") {
                    param2 = val;
                }
                break;
            case 't':
                if (key == "timeout") {
                    timeout = NStr::StringToUInt(val, NStr::fConvErr_NoThrow);
                }
                else if (key == "tags") {
                    tags = val;
                }
                break;
            case 'w':
                if (key == "where") {
                    param1 = val;
                }
                break;
            default:
                break;
            }
        }
    }
};


class CNetScheduleServer;

//////////////////////////////////////////////////////////////////////////
// CNSRequestContextFactory
class CNSRequestContextFactory : public CRequestContextFactory
{
public:
    CNSRequestContextFactory(CNetScheduleServer* server);
    virtual CRequestContext* Get();
    virtual void Return(CRequestContext*);
private:
    CNetScheduleServer* m_Server;
};


//
const unsigned kMaxMessageSize = kNetScheduleMaxDBErrSize * 4;

//////////////////////////////////////////////////////////////////////////
/// ConnectionHandler for NetScheduler

class CNetScheduleHandler : public IServer_LineMessageHandler
{
public:
    CNetScheduleHandler(CNetScheduleServer* server);
    // MessageHandler protocol
    virtual EIO_Event GetEventsToPollFor(const CTime** alarm_time) const;
    virtual void OnOpen(void);
    virtual void OnWrite(void);
    virtual void OnClose(void) { }
    virtual void OnTimeout(void);
    virtual void OnOverflow(void);
    virtual void OnMessage(BUF buffer);

    void WriteMsg(const char*   prefix,
                  const string& msg = kEmptyStr,
                  bool          comm_control = false);
    void WriteOK(const string& msg = kEmptyStr);
    void WriteErr(const string& msg = kEmptyStr);

private:
    // Message processing phases
    void ProcessMsgAuth(BUF buffer);
    void ProcessMsgQueue(BUF buffer);
    void ProcessMsgRequest(BUF buffer);
    // Message processing for ProcessSubmitBatch phases
    void ProcessMsgBatchHeader(BUF buffer);
    void ProcessMsgBatchJob(BUF buffer);
    void ProcessMsgBatchSubmit(BUF buffer);

    // Monitoring
    /// Are we monitoring?
    bool IsMonitoring();
    /// Send string to monitor
    void MonitorPost(const string& msg);


public:
    enum ENSAccess {
        eNSAC_Queue         = 1 << 0,
        eNSAC_Worker        = 1 << 1,
        eNSAC_Submitter     = 1 << 2,
        eNSAC_Admin         = 1 << 3,
        eNSAC_QueueAdmin    = 1 << 4,
        eNSAC_Test          = 1 << 5,
        eNSAC_DynQueueAdmin = 1 << 6,
        eNSAC_DynClassAdmin = 1 << 7,
        eNSAC_AnyAdminMask  = eNSAC_Admin | eNSAC_QueueAdmin |
                              eNSAC_DynQueueAdmin | eNSAC_DynClassAdmin,
        // Combination of flags for client roles
        eNSCR_Any        = 0,
        eNSCR_Queue      = eNSAC_Queue,
        eNSCR_Worker     = eNSAC_Worker + eNSAC_Queue,
        eNSCR_Submitter  = eNSAC_Submitter + eNSAC_Queue,
        eNSCR_Admin      = eNSAC_Admin,
        eNSCR_QueueAdmin = eNSAC_QueueAdmin + eNSAC_Queue
    };
    typedef unsigned TNSClientRole;

    typedef void (CNetScheduleHandler::*FProcessor)(void);
    struct SCommandExtra {
        FProcessor    processor;
        TNSClientRole role;
    };
    typedef SNSProtoCmdDef<SCommandExtra>      SCommandMap;
    typedef SNSProtoParsedCmd<SCommandExtra>   SParsedCmd;
    typedef CNetServProtoParser<SCommandExtra> TProtoParser;

private:
    static SCommandMap sm_CommandMap[];
    static SCommandMap sm_BatchHeaderMap[];
    static SCommandMap sm_BatchEndMap[];
    static SNSProtoArgument sm_BatchArgs[];

    void ProcessError(const CException& ex);

    // Command processors
    void ProcessFastStatusS();
    void ProcessFastStatusW();
    void ProcessSubmit();
    void ProcessSubmitBatch();
    void ProcessBatchStart();
    void ProcessBatchSequenceEnd();
    void ProcessCancel();
    void ProcessStatus();
    void ProcessGetJob();
    void ProcessWaitGet();
    void ProcessPut();
    void ProcessJobExchange();
    void ProcessPutMessage();
    void ProcessGetMessage();
    void ProcessForceReschedule();
    void ProcessPutFailure();
    void ProcessDropQueue();
    void ProcessReturn();
    void ProcessJobRunTimeout();
    void ProcessJobDelayExpiration();
    void ProcessDropJob();
    void ProcessStatistics();
    void ProcessStatusSnapshot();
    void ProcessMonitor();
    void ProcessReloadConfig();
    void ProcessDump();
    void ProcessPrintQueue();
    void ProcessShutdown();
    void ProcessVersion();
    void ProcessRegisterClient();
    void ProcessUnRegisterClient();
    void ProcessQList();
    void ProcessLog();
    void ProcessQuitSession();
    void ProcessCreateQueue();
    void ProcessDeleteQueue();
    void ProcessQueueInfo();
    void ProcessQuery();
    void ProcessSelectQuery();
    void ProcessGetParam();
    void ProcessGetConfiguration();
    void ProcessReading();
    void ProcessConfirm();
    void ProcessReadFailed();
    void ProcessGetAffinityList();
    void ProcessInitWorkerNode();
    void ProcessClearWorkerNode();

    // Delayed output handlers
    void WriteProjection();

private:
    static
    void x_ParseTags(const string& strtags, TNSTagList& tags);
    static
    string x_SerializeBitVector(TNSBitVector& bv);
    static
    TNSBitVector x_DeserializeBitVector(const string& s);

    void x_MonitorJob(const CJob& job);


    bool x_CheckVersion(void);
    void x_CheckAccess(TNSClientRole role);
    void x_AccessViolationMessage(unsigned deficit, string& msg);
    string x_FormatErrorMessage(string header, string what);
    void x_WriteErrorToMonitor(string msg);
    // Moved from CNetScheduleServer
    void x_MakeLogMessage(string& lmsg);
    void x_MakeGetAnswer(const CJob& job);
    void x_StatisticsNew(const string& what, time_t curr);

    // Data
    string m_Request;
    string m_Answer;
    char   m_MsgBuffer[kMaxMessageSize];

    unsigned                    m_PeerAddr;
    // WorkerNodeInfo contains duplicates of m_AuthString and m_PeerAddr
    SWorkerNodeInfo             m_WorkerNodeInfo;
    string                      m_AuthString;
    CNetScheduleServer*         m_Server;
    string                      m_QueueName;
    auto_ptr<CQueue>            m_Queue;
    SJS_Request                 m_JobReq;
    // Incapabilities - that is combination of ENSAccess
    // rights, which can NOT be performed by this connection
    unsigned                    m_Incaps;
    unsigned                    m_Unreported;
    bool                        m_VersionControl;
    // Unique command number for relating command and reply
    unsigned                    m_CommandNumber;
    // Phase of connection - login, queue, command, batch submit etc.
    void (CNetScheduleHandler::*m_ProcessMessage)(BUF buffer);
    // Delayed output processor
    void (CNetScheduleHandler::*m_DelayedOutput)();

    // Batch submit data
    unsigned                    m_BatchSize;
    unsigned                    m_BatchPos;
    CStopWatch                  m_BatchStopWatch;
    vector<CJob>                m_BatchJobs;
    unsigned                    m_BatchSubmPort;
    unsigned                    m_BatchSubmTimeout;
    string                      m_BatchClientIP;
    string                      m_BatchClientSID;

    // For projection writer
    auto_ptr<TNSBitVector>      m_SelectedIds;
    SFieldsDescription          m_FieldDescr;

    /// Quick local timer
    CFastLocalTime              m_LocalTimer;

    // Due to processing of one logical request can span
    // multiple threads, but goes on in a single handler,
    // we use manual request context switching, thus replacing
    // default per-thread mechanism.
    CRef<CNSRequestContextFactory> m_RequestContextFactory;

    /// Parser for incoming commands
    TProtoParser                m_ReqParser;

}; // CNetScheduleHandler

//////////////////////////////////////////////////////////////////////////
/// CNetScheduleConnectionFactory::
class CNetScheduleConnectionFactory : public IServer_ConnectionFactory
{
public:
    CNetScheduleConnectionFactory(CNetScheduleServer* server) :
        m_Server(server)
    {
    }
    IServer_ConnectionHandler* Create(void)
    {
        return new CNetScheduleHandler(m_Server);
    }
private:
    CNetScheduleServer* m_Server;
};


//////////////////////////////////////////////////////////////////////////
/// Host for background threads
class CNetScheduleBackgroundHost : public CBackgroundHost
{
public:
    CNetScheduleBackgroundHost(CNetScheduleServer* server) :
      m_Server(server)
    {}
    virtual void ReportError(ESeverity severity,
        const string& what);
    virtual bool ShouldRun();
private:
    CNetScheduleServer* m_Server;
};


//////////////////////////////////////////////////////////////////////////
/// Parameters for server
struct SNS_Parameters : SServer_Parameters
{
    bool reinit;
    unsigned short port;
    unsigned short udp_port;

    bool use_hostname;
    unsigned network_timeout;

    bool is_daemon;
    bool is_log;

    string admin_hosts;

    void Read(const IRegistry& reg, const string& sname);

    unsigned GetNumParams() const;
    string GetParamName(unsigned n) const;
    string GetParamValue(unsigned n) const;
};


void SNS_Parameters::Read(const IRegistry& reg, const string& sname)
{
#define GetIntNoErr(name, dflt) \
    reg.GetInt(sname, name, dflt, 0, IRegistry::eReturn)
#define GetBoolNoErr(name, dflt) \
    reg.GetBool(sname, name, dflt, 0, IRegistry::eReturn)

    reinit = GetBoolNoErr("reinit", false);
    max_connections = GetIntNoErr("max_connections", 100);
    max_threads = GetIntNoErr("max_threads", 25);
    init_threads = GetIntNoErr("init_threads", 10);
    if (init_threads > max_threads)
        init_threads = max_threads;

    port = (unsigned short) GetIntNoErr("port", 9100);

    int u_port = GetIntNoErr("udp_port", 0);
    if (u_port == 0) {
        u_port = port + 1;
        LOG_POST(Info << "UDP notification port: " << u_port);
    }
    if (u_port < 1024 || u_port > 65535) {
        LOG_POST(Error << "Invalid UDP port value: " << u_port
                       << ". Notification will be disabled.");
        u_port = -1;
    }
    if (u_port < 0) {
        LOG_POST(Info << "UDP notification disabled.");
    }
    udp_port = u_port > 0 ? (unsigned short) u_port : 0;

    use_hostname = GetBoolNoErr("use_hostname", false);
    network_timeout = GetIntNoErr("network_timeout", 10);
    if (network_timeout == 0) {
        LOG_POST(Warning <<
            "INI file sets 0 sec. network timeout. Assume 10 seconds.");
        network_timeout =  10;
    }

    is_log = GetBoolNoErr("log", false);
    is_daemon = GetBoolNoErr("daemon", false);

    admin_hosts = reg.GetString(sname, "admin_host", kEmptyStr);
}


unsigned SNS_Parameters::GetNumParams() const
{
    return 11;
}


string SNS_Parameters::GetParamName(unsigned n) const
{
    switch (n) {
    case 0:  return "reinit";
    case 1:  return "max_connections";
    case 2:  return "max_threads";
    case 3:  return "init_threads";
    case 4:  return "port";
    case 5:  return "udp_port";
    case 6:  return "use_hostname";
    case 7:  return "network_timeout";
    case 8:  return "log";
    case 9:  return "daemon";
    case 10: return "admin_host";
    default: return "";
    }
}


string SNS_Parameters::GetParamValue(unsigned n) const
{
    switch (n) {
    case 0:  return NStr::BoolToString(reinit);
    case 1:  return NStr::UIntToString(max_connections);
    case 2:  return NStr::UIntToString(max_threads);
    case 3:  return NStr::UIntToString(init_threads);
    case 4:  return NStr::UIntToString(port);
    case 5:  return NStr::UIntToString(udp_port);
    case 6:  return NStr::BoolToString(use_hostname);
    case 7:  return NStr::UIntToString(network_timeout);
    case 8:  return NStr::BoolToString(is_log);
    case 9:  return NStr::BoolToString(is_daemon);
    case 10: return admin_hosts;
    default: return "";
    }
}

struct CRequestContextPoolFactory
{
    static CRequestContext* Create() {
        CRequestContext* rc = new CRequestContext;
        rc->AddReference();
        return rc;
    }
    static void Delete(CRequestContext* rc) {
        if (rc) rc->RemoveReference();
    }
};


//////////////////////////////////////////////////////////////////////////
/// NetScheduler threaded server
///
/// @internal
class CNetScheduleServer : public CServer
{
public:
    CNetScheduleServer();
    virtual ~CNetScheduleServer();

    void AddDefaultListener(IServer_ConnectionFactory* factory);
    void SetNSParameters(const SNS_Parameters& new_params);

    virtual bool ShutdownRequested(void)
    {
        return m_Shutdown;
    }

    void SetQueueDB(CQueueDataBase* qdb)
    {
        delete m_QueueDB;
        m_QueueDB = qdb;
    }

    void SetShutdownFlag(int signum = 0)
    {
        if (!m_Shutdown) {
            m_Shutdown = true;
            m_SigNum = signum;
        }
    }

    //////////////////////////////////////////////////////////////////
    /// For Handlers
    /// TRUE if logging is ON
    bool IsLog() const { return m_LogFlag.Get() != 0; }
    void SetLogging(bool flag) {
        m_LogFlag.Set(flag);
    }
    unsigned GetCommandNumber() { return m_AtomicCommandNumber.Add(1); }
    CRequestContext* GetRequestContextFromPool()
        { return m_RequestContextPool.Get(); }
    void ReturnRequestContextToPool(CRequestContext* req_ctx)
        { m_RequestContextPool.Return(req_ctx); }

    // Queue handling
    unsigned Configure(const IRegistry& reg) {
        return m_QueueDB->Configure(reg);
    }
    CQueue* OpenQueue(const string& name, unsigned peer_addr) {
        return m_QueueDB->OpenQueue(name, peer_addr);
    }
    void CreateQueue(const string& qname, const string& qclass,
                    const string& comment) {
        m_QueueDB->CreateQueue(qname, qclass, comment);
    }
    void DeleteQueue(const string& qname) {
        m_QueueDB->DeleteQueue(qname);
    }
    void QueueInfo(const string& qname, int& kind,
        string* qclass, string* comment) {
        m_QueueDB->QueueInfo(qname, kind, qclass, comment);
    }
    string GetQueueNames(const string& sep) const {
        return m_QueueDB->GetQueueNames(sep);
    }

    unsigned GetInactivityTimeout(void) { return m_InactivityTimeout; }
    string& GetHost() { return m_Host; }
    unsigned GetPort() { return m_Port; }
    unsigned GetHostNetAddr() { return m_HostNetAddr; }
    const CTime& GetStartTime(void) const { return m_StartTime; }
    bool AdminHostValid(unsigned host) const {
        return !m_AdminHosts.IsRestrictionSet() ||
                m_AdminHosts.IsAllowed(host);
    }

    CBackgroundHost& GetBackgroundHost() { return m_BackgroundHost; }

protected:
    virtual void Exit();

private:
    void x_CreateLog();

private:
    /// API for background threads
    CNetScheduleBackgroundHost m_BackgroundHost;
    /// Host name where server runs
    string                  m_Host;
    unsigned                m_Port;
    unsigned                m_HostNetAddr;
    bool                    m_Shutdown;
    int                     m_SigNum;  ///< Shutdown signal number
    /// Time to wait for the client (seconds)
    unsigned                m_InactivityTimeout;
    CQueueDataBase*         m_QueueDB;
    CTime                   m_StartTime;
    CAtomicCounter          m_LogFlag;
    /// Quick local timer
    CFastLocalTime          m_LocalTimer;
    /// Pool of RequestContexts
    CResourcePool<CRequestContext, CFastMutex, CRequestContextPoolFactory>
        m_RequestContextPool;

    /// List of admin stations
    CNetSchedule_AccessList m_AdminHosts;

    CAtomicCounter          m_AtomicCommandNumber;
};


static void s_BufReadHelper(void* data, void* ptr, size_t size)
{
    ((string*) data)->append((const char *) ptr, size);
}


static void s_ReadBufToString(BUF buf, string& str)
{
    str.erase();
    size_t size = BUF_Size(buf);
    str.reserve(size);
    BUF_PeekAtCB(buf, 0, s_BufReadHelper, &str, size);
    BUF_Read(buf, NULL, size);
}

//////////////////////////////////////////////////////////////////////////
// CNSRequestContextFactory implementation
CNSRequestContextFactory::CNSRequestContextFactory(CNetScheduleServer* server)
: m_Server(server)
{
}


CRequestContext* CNSRequestContextFactory::Get()
{
    CRequestContext* req_ctx = m_Server->GetRequestContextFromPool();
    req_ctx->SetRequestID(CRequestContext::GetNextRequestID());
    return req_ctx;
}


void CNSRequestContextFactory::Return(CRequestContext* req_ctx)
{
    m_Server->ReturnRequestContextToPool(req_ctx);
}


//////////////////////////////////////////////////////////////////////////
// CNetScheduleBackgroundHost implementation
void CNetScheduleBackgroundHost::ReportError(ESeverity severity,
                                             const string& what)
{
    if (severity == CBackgroundHost::eFatal)
        m_Server->SetShutdownFlag();
}


bool CNetScheduleBackgroundHost::ShouldRun()
{
    return true;
}


//////////////////////////////////////////////////////////////////////////
// ConnectionHandler implementation

CNetScheduleHandler::CNetScheduleHandler(CNetScheduleServer* server)
    : m_PeerAddr(0), m_Server(server),
      m_Incaps(~0L), m_Unreported(~0L), m_VersionControl(false),
      m_RequestContextFactory(new CNSRequestContextFactory(server)),
      m_ReqParser(sm_CommandMap)
{
}


EIO_Event CNetScheduleHandler::GetEventsToPollFor(const CTime** /*alarm_time*/) const
{
    if (m_DelayedOutput) return eIO_Write;
    return eIO_Read;
}


void CNetScheduleHandler::OnOpen(void)
{
    CSocket& socket = GetSocket();
    socket.DisableOSSendDelay();
    STimeout to = {m_Server->GetInactivityTimeout(), 0};
    socket.SetTimeout(eIO_ReadWrite, &to);

    socket.GetPeerAddress(&m_PeerAddr, 0, eNH_NetworkByteOrder);
    // always use localhost(127.0*) address for clients coming from
    // the same net address (sometimes it can be 127.* or full address)
    if (m_PeerAddr == m_Server->GetHostNetAddr()) {
        m_PeerAddr = CSocketAPI::GetLoopbackAddress();
    }
    m_WorkerNodeInfo.host = m_PeerAddr;

    m_ProcessMessage = &CNetScheduleHandler::ProcessMsgAuth;
    m_DelayedOutput = NULL;
}


void CNetScheduleHandler::OnWrite()
{
    if (m_DelayedOutput)
        (this->*m_DelayedOutput)();
}


void CNetScheduleHandler::OnTimeout()
{
//    ERR_POST("OnTimeout!");
}


void CNetScheduleHandler::OnOverflow()
{
//    ERR_POST("OnOverflow!");
}

class CRequestContextGuard
{
public:
    CRequestContextGuard()
    {
        m_Ctx = &CDiagContext::GetRequestContext();
    }
    ~CRequestContextGuard()
    {
        CRequestContext* ctx = &CDiagContext::GetRequestContext();
        if (ctx == m_Ctx) return;
        CDiagContext::SetRequestContext(m_Ctx);
    }
private:
    CRequestContext*        m_Ctx;
};


void CNetScheduleHandler::OnMessage(BUF buffer)
{
    if (m_Server->ShutdownRequested()) {
        WriteErr("NetSchedule server is shutting down. Session aborted.");
        return;
    }
    CRequestContextGuard guard;
    try {
        (this->*m_ProcessMessage)(buffer);
    }
    catch (CNetScheduleException &ex)
    {
        WriteErr(string(ex.GetErrCodeString()) + ":" + ex.GetMsg());
        string msg = x_FormatErrorMessage("Server error", ex.what());
        ERR_POST(msg);
        x_WriteErrorToMonitor(msg);
        throw;
    }
    catch (CNSProtoParserException &ex)
    {
        WriteErr(string(ex.GetErrCodeString()) + ":" + ex.GetMsg());
        string msg = x_FormatErrorMessage("Server error", ex.what());
        ERR_POST(msg);
        x_WriteErrorToMonitor(msg);
        throw;
    }
    catch (CNetServiceException &ex)
    {
        WriteErr(string(ex.GetErrCodeString()) + ":" + ex.GetMsg());
        string msg = x_FormatErrorMessage("Server error", ex.what());
        ERR_POST(msg);
        x_WriteErrorToMonitor(msg);
        throw;
    }
    catch (CBDB_ErrnoException& ex)
    {
        if (ex.IsRecovery()) {
            string msg = x_FormatErrorMessage(
                "Fatal Berkeley DB error: DB_RUNRECOVERY. "
                "Emergency shutdown initiated!", ex.what());
            ERR_POST(msg);
            x_WriteErrorToMonitor(msg);
            m_Server->SetShutdownFlag();
        } else {
            string msg = x_FormatErrorMessage("BDB error", ex.what());
            ERR_POST(msg);
            x_WriteErrorToMonitor(msg);

            string err = "eInternalError:Internal database error - ";
            err += ex.what();
            err = NStr::PrintableString(err);
            WriteErr(err);
        }
        throw;
    }
    catch (CBDB_Exception &ex)
    {
        string err = "eInternalError:Internal database (BDB) error - ";
        err += ex.what();
        WriteErr(NStr::PrintableString(err));

        string msg = x_FormatErrorMessage("BDB error", ex.what());
        ERR_POST(msg);
        x_WriteErrorToMonitor(msg);
        throw;
    }
    catch (exception& ex)
    {
        string err = "eInternalError:Internal error - ";
        err += ex.what();
        WriteErr(NStr::PrintableString(err));

        string msg = x_FormatErrorMessage("STL exception", ex.what());
        ERR_POST(msg);
        x_WriteErrorToMonitor(msg);
        throw;
    }
}


string CNetScheduleHandler::x_FormatErrorMessage(string header, string what)
{
    string msg(header);
    CSocket& socket = GetSocket();
    msg += ": ";
    msg += what;
    msg += " Peer=";
    msg += socket.GetPeerAddress();
    msg += " ";
    msg += m_AuthString;
    msg += m_Request;
    return msg;
}

void CNetScheduleHandler::x_WriteErrorToMonitor(string msg)
{
    if (IsMonitoring()) {
        MonitorPost(m_LocalTimer.GetLocalTime().AsString() + msg);
    }
}


void CNetScheduleHandler::ProcessMsgAuth(BUF buffer)
{
    s_ReadBufToString(buffer, m_AuthString);

    if (m_Server->AdminHostValid(m_PeerAddr) &&
        (m_AuthString == "netschedule_control" ||
         m_AuthString == "netschedule_admin"))
    {
        // TODO: queue admin should be checked in ProcessMsgQueue,
        // when queue info is available
        m_Incaps &= ~(eNSAC_Admin | eNSAC_QueueAdmin);
    }
    m_WorkerNodeInfo.auth = m_AuthString;
    m_ProcessMessage = &CNetScheduleHandler::ProcessMsgQueue;
}


void CNetScheduleHandler::ProcessMsgQueue(BUF buffer)
{
    s_ReadBufToString(buffer, m_QueueName);

    if (m_QueueName != "noname" && !m_QueueName.empty()) {
        m_Queue.reset(m_Server->OpenQueue(m_QueueName, m_PeerAddr));
        m_Incaps &= ~eNSAC_Queue;
        if (m_Queue->IsWorkerAllowed())
            m_Incaps &= ~eNSAC_Worker;
        if (m_Queue->IsSubmitAllowed())
            m_Incaps &= ~eNSAC_Submitter;
        if (m_Queue->IsVersionControl())
            m_VersionControl = true;
    }

    m_ProcessMessage = &CNetScheduleHandler::ProcessMsgRequest;
}


// Workhorse method
void CNetScheduleHandler::ProcessMsgRequest(BUF buffer)
{
    bool is_log = m_Server->IsLog();

    s_ReadBufToString(buffer, m_Request);

    m_CommandNumber = m_Server->GetCommandNumber();

    // Logging
    if (is_log || IsMonitoring()) {
        string lmsg;
        x_MakeLogMessage(lmsg);
        if (is_log) {
            NCBI_NS_NCBI::CNcbiDiag(eDiag_Info, eDPF_Log).GetRef()
                << lmsg
                << NCBI_NS_NCBI::Endm;
        }
        if (IsMonitoring()) {
            lmsg += "\n";
            MonitorPost(lmsg);
        }
    }

    m_JobReq.Init();
    SParsedCmd cmd;
    try {
        cmd = m_ReqParser.ParseCommand(m_Request);
    }
    catch (const CNSProtoParserException& ex) {
        // Rewrite error in usual terms
        // To prevent stall due to possible very long message (which
        // quotes original, arbitrarily long user input) we truncate it here
        // to the value long enough for diagnostics.
        string msg = ex.GetMsg().substr(0, 64);
        WriteErr("eProtocolSyntaxError:" + msg);
        return;
    }

    const SCommandExtra& extra = cmd.command->extra;

    x_CheckAccess(extra.role);
    m_JobReq.SetParamFields(cmd.params);

    if (extra.processor == &CNetScheduleHandler::ProcessQuitSession) {
        ProcessQuitSession();
        return;
    }

    // program version control
    if (m_VersionControl &&
        // we want status request to be fast, skip version control
        (extra.processor != &CNetScheduleHandler::ProcessStatus) &&
        // bypass for admin tools
        (m_Incaps & eNSAC_Admin)) {
        if (!x_CheckVersion()) {
            WriteErr("eInvalidClientOrVersion:");
            CSocket& socket = GetSocket();
            socket.Close();
            return;
        }
        // One check per session is enough
        m_VersionControl = false;
    }
    (this->*extra.processor)();
}


bool CNetScheduleHandler::x_CheckVersion()
{
    string::size_type pos;
    pos = m_AuthString.find("prog='");
    if (pos == string::npos) {
        return false;
    }
    string auth_prog;
    const char* prog_str = m_AuthString.c_str();
    prog_str += pos + 6;
    for (; *prog_str && *prog_str != '\''; ++prog_str) {
        char ch = *prog_str;
        auth_prog.push_back(ch);
    }
    if (auth_prog.empty()) {
        return false;
    }
    try {
        CQueueClientInfo auth_prog_info;
        ParseVersionString(auth_prog,
                           &auth_prog_info.client_name,
                           &auth_prog_info.version_info);
        if (!m_Queue->IsMatchingClient(auth_prog_info))
            return false;
    }
    catch (exception&)
    {
        return false;
    }
    return true;
}

void CNetScheduleHandler::x_CheckAccess(TNSClientRole role)
{
    unsigned deficit = role & m_Incaps;
    if (!deficit) return;
    if (deficit & eNSAC_DynQueueAdmin) {
        // check that we have admin rights on queue m_JobReq.param1
        deficit &= ~eNSAC_DynQueueAdmin;
    }
    if (deficit & eNSAC_DynClassAdmin) {
        // check that we have admin rights on class m_JobReq.param2
        deficit &= ~eNSAC_DynClassAdmin;
    }
    if (!deficit) return;
    bool deny =
        (deficit & eNSAC_AnyAdminMask)  ||  // for any admin access
        (m_Incaps & eNSAC_Queue)        ||  // or if no queue
        m_Queue->IsDenyAccessViolations();  // or if queue configured so
    bool report =
        !(m_Incaps & eNSAC_Queue)         &&  // only if there is a queue
        m_Queue->IsLogAccessViolations()  &&  // so configured
        (m_Unreported & deficit);             // and we did not report it yet
    if (report) {
        m_Unreported &= ~deficit;
        CSocket& socket = GetSocket();
        string msg = "Unauthorized access from: ";
        msg += socket.GetPeerAddress();
        msg += " ";
        msg += m_AuthString;
        x_AccessViolationMessage(deficit, msg);
        LOG_POST(Warning << msg);
    } else {
        m_Unreported = ~0L;
    }
    if (deny) {
        string msg = "Access denied:";
        x_AccessViolationMessage(deficit, msg);
        NCBI_THROW(CNetScheduleException, eAccessDenied, msg);
    }
}


void CNetScheduleHandler::x_AccessViolationMessage(unsigned deficit,
                                                   string& msg)
{
    if (deficit & eNSAC_Queue)
        msg += " queue required";
    if (deficit & eNSAC_Worker)
        msg += " worker node privileges required";
    if (deficit & eNSAC_Submitter)
        msg += " submitter privileges required";
    if (deficit & eNSAC_Admin)
        msg += " admin privileges required";
    if (deficit & eNSAC_QueueAdmin)
        msg += " queue admin privileges required";
}


void CNetScheduleHandler::x_ParseTags(const string& strtags, TNSTagList& tags)
{
    list<string> tokens;
    NStr::Split(NStr::ParseEscapes(strtags), "\t",
        tokens, NStr::eNoMergeDelims);
    tags.clear();
    for (list<string>::iterator it = tokens.begin(); it != tokens.end(); ++it) {
        string key(*it); ++it;
        if (it != tokens.end()){
            tags.push_back(TNSTag(key, *it));
        } else {
            tags.push_back(TNSTag(key, kEmptyStr));
            break;
        }
    }
}


string CNetScheduleHandler::x_SerializeBitVector(TNSBitVector& bv)
{
    TNSBitVector::statistics st;
    bv.optimize(0, TNSBitVector::opt_compress, &st);
    size_t dst_size = st.max_serialize_mem * 4 / 3 + 1;
    size_t src_size = st.max_serialize_mem;
    unsigned char* buf = new unsigned char[src_size + dst_size];
    size_t size = bm::serialize(bv, buf);
    size_t src_read, dst_written, line_len;
    line_len = 0; // no line feeds in encoded value
    unsigned char* encode_buf = buf+src_size;
    BASE64_Encode(buf, size, &src_read, encode_buf, dst_size, &dst_written, &line_len);
    delete [] buf;
    return string((const char*) encode_buf, dst_written);
}


TNSBitVector CNetScheduleHandler::x_DeserializeBitVector(const string& s)
{
    size_t src_read, dst_written;
    size_t dst_size =  s.size();
    vector<unsigned char> dst_buf(dst_size, 0);
    BASE64_Decode(s.data(),  s.size(), &src_read,
                  &dst_buf[0], dst_size, &dst_written);
    TNSBitVector bv;
    bm::deserialize(bv, &dst_buf[0]);
    return bv;
}


void CNetScheduleHandler::x_MonitorJob(const CJob& job)
{
    string msg;

    msg += string("id: ") + NStr::IntToString(job.GetId());
    msg += string(" input: ") + job.GetInput();
    msg += string(" aff: ") + job.GetAffinityToken();
    msg += string(" aff_id: ") + NStr::IntToString(job.GetAffinityId());
    msg += string(" mask: ") + NStr::IntToString(job.GetMask());
    msg += string(" tags: ");
    ITERATE(TNSTagList, it, job.GetTags()) {
        msg += it->first + "=" + it->second + "; ";
    }
    MonitorPost(msg);
}

//////////////////////////////////////////////////////////////////////////
// Process* methods for processing commands

void CNetScheduleHandler::ProcessFastStatusS()
{
    unsigned job_id = CNetScheduleKey(m_JobReq.job_key).id;

    TJobStatus status = m_Queue->GetStatus(job_id);
    // TODO: update timestamp
    WriteOK(NStr::IntToString((int) status));
}


void CNetScheduleHandler::ProcessFastStatusW()
{
    unsigned job_id = CNetScheduleKey(m_JobReq.job_key).id;

    TJobStatus status = m_Queue->GetStatus(job_id);
    WriteOK(NStr::IntToString((int) status));
    m_Queue->RegisterWorkerNodeVisit(m_WorkerNodeInfo);
}


void CNetScheduleHandler::ProcessSubmit()
{
    CJob job;
    job.SetInput(NStr::ParseEscapes(m_JobReq.input));
    job.SetAffinityToken(NStr::ParseEscapes(m_JobReq.affinity_token));
    job.SetTags(m_JobReq.tags);
    job.SetMask(m_JobReq.job_mask);
    job.SetSubmAddr(m_PeerAddr);
    job.SetSubmPort(m_JobReq.port);
    job.SetSubmTimeout(m_JobReq.timeout);
    job.SetProgressMsg(m_JobReq.param1);
    job.SetClientIP(m_JobReq.param3);
    job.SetClientSID(m_JobReq.param2);

    unsigned job_id = m_Queue->Submit(job);
                        
    string str_job_id(CNetScheduleKey(job_id,
        m_Server->GetHost(), m_Server->GetPort()));
    WriteOK(str_job_id);

    if (IsMonitoring()) {
        CSocket& socket = GetSocket();
        string msg = "::ProcessSubmit ";
        msg += m_LocalTimer.GetLocalTime().AsString();
        msg += str_job_id;
        msg += " ==> ";
        msg += socket.GetPeerAddress();
        msg += " ";
        msg += m_AuthString;

        MonitorPost(msg);
        x_MonitorJob(job);
    }
}


void CNetScheduleHandler::ProcessSubmitBatch()
{
    m_BatchSubmPort    = m_JobReq.port;
    m_BatchSubmTimeout = m_JobReq.timeout;
    m_BatchClientIP    = m_JobReq.param3;
    m_BatchClientSID   = m_JobReq.param2;
    WriteOK("Batch submit ready");
    m_ProcessMessage = &CNetScheduleHandler::ProcessMsgBatchHeader;
}


CNetScheduleHandler::SCommandMap CNetScheduleHandler::sm_BatchHeaderMap[] = {
    { "BTCH", &CNetScheduleHandler::ProcessBatchStart, 0,
        { { "size", eNSPT_Int, eNSPA_Required } } },
    { "ENDS", &CNetScheduleHandler::ProcessBatchSequenceEnd, 0 },
    { NULL }
};

CNetScheduleHandler::SCommandMap CNetScheduleHandler::sm_BatchEndMap[] = {
    { "ENDB" },
    { NULL }
};

SNSProtoArgument CNetScheduleHandler::sm_BatchArgs[] = {
    { "input", eNSPT_Str, eNSPA_Required },
    { "affp",  eNSPT_Id,  eNSPA_Optional | fNSPA_Or | fNSPA_Match, "" },
    { "aff",   eNSPT_Str, eNSPA_Optional, "" },
    { "msk",   eNSPT_Int, eNSPA_Optional, "0" },
    { "tags",  eNSPT_Str, eNSPA_Optional, "" },
    { NULL }
};


// Message processors for ProcessSubmitBatch
void CNetScheduleHandler::ProcessMsgBatchHeader(BUF buffer)
{
    // Expecting BTCH size | ENDS
    char cmd[256];
    size_t msg_size = BUF_Read(buffer, cmd, sizeof(cmd)-1);
    cmd[msg_size] = 0;
    m_ReqParser.SetCommandMap(sm_BatchHeaderMap);
    try {
        try {
            SParsedCmd parsed = m_ReqParser.ParseCommand(cmd);
            const string& size_str = parsed.params["size"];
            if (!size_str.empty()) {
                m_BatchSize = NStr::StringToInt(size_str);
            }
            (this->*parsed.command->extra.processor)();
        }
        catch (const CException&) {
            BUF_Read(buffer, 0, BUF_Size(buffer));
            WriteErr("eProtocolSyntaxError:"
                     "Batch submit error - BATCH expected");
            ProcessBatchSequenceEnd();
        }
        m_ReqParser.SetCommandMap(sm_CommandMap);
    }
    catch (...) {
        m_ReqParser.SetCommandMap(sm_CommandMap);
        throw;
    }
}


void CNetScheduleHandler::ProcessBatchStart()
{
    m_BatchPos = 0;
    m_BatchStopWatch.Restart();
    m_BatchJobs.resize(m_BatchSize);
    m_ProcessMessage = &CNetScheduleHandler::ProcessMsgBatchJob;
    if (IsMonitoring()) {
        CSocket& socket = GetSocket();
        string msg = "::ProcessMsgBatchHeader ";
        msg += m_LocalTimer.GetLocalTime().AsString();
        msg += " ";
        msg += socket.GetPeerAddress();
        msg += " ";
        msg += m_AuthString;
    }
}


void CNetScheduleHandler::ProcessBatchSequenceEnd()
{
    m_BatchJobs.clear();
    m_ProcessMessage = &CNetScheduleHandler::ProcessMsgRequest;
    WriteOK();
}


void CNetScheduleHandler::ProcessMsgBatchJob(BUF buffer)
{
    // Expecting:
    // "input" [affp|aff="affinity_token"] [msk=1]
    //         [tags="key1\tval1\tkey2\t\tkey3\tval3"]
    string msg;
    s_ReadBufToString(buffer, msg);

    CJob& job = m_BatchJobs[m_BatchPos];

    map<string, string> params;
    try {
        m_ReqParser.ParseArguments(msg, sm_BatchArgs, &params);
    }
    catch (const CException&) {
        WriteErr("eProtocolSyntaxError:"
                 "Invalid batch submission, syntax error");
        m_ProcessMessage = &CNetScheduleHandler::ProcessMsgRequest;
    }

    m_JobReq.SetParamFields(params);
    job.SetInput(NStr::ParseEscapes(m_JobReq.input));
    if (m_JobReq.param1 == "match") {
        // We reuse jobs from m_BatchJobs, so we always should reset this
        // field. We should call SetAffinityToken before SetAffinityId, because
        // it clears out id field of the job.
        job.SetAffinityToken("");
        job.SetAffinityId(kMax_I4);
    } else {
        job.SetAffinityToken(NStr::ParseEscapes(m_JobReq.affinity_token));
    }
    job.SetTags(m_JobReq.tags);

    job.SetMask(m_JobReq.job_mask);
    job.SetSubmAddr(m_PeerAddr);
    job.SetSubmPort(m_BatchSubmPort);
    job.SetSubmTimeout(m_BatchSubmTimeout);
    job.SetClientIP(m_BatchClientIP);
    job.SetClientSID(m_BatchClientSID);

    if (++m_BatchPos >= m_BatchSize)
        m_ProcessMessage = &CNetScheduleHandler::ProcessMsgBatchSubmit;
}


class CRequestContextSubmitGuard
{
public:
    CRequestContextSubmitGuard(CNetScheduleServer* server,
                               const string& client_ip,
                               const string& session_id)
      : m_Server(server)
    {
        m_Ctx = &CDiagContext::GetRequestContext();
        CRequestContext* ctx = m_Server->GetRequestContextFromPool();
        if (!client_ip.empty())
            ctx->SetClientIP(client_ip);
        ctx->SetSessionID(session_id);
        ctx->SetRequestID(CRequestContext::GetNextRequestID());
        CDiagContext::SetRequestContext(ctx);
    }
    ~CRequestContextSubmitGuard()
    {
        CRequestContext* ctx = &CDiagContext::GetRequestContext();
        if (ctx == m_Ctx) return;
        CDiagContext::SetRequestContext(m_Ctx);
        m_Server->ReturnRequestContextToPool(ctx);
    }
private:
    CNetScheduleServer* m_Server;
    CRequestContext*    m_Ctx;
};


void CNetScheduleHandler::ProcessMsgBatchSubmit(BUF buffer)
{
    // Expecting ENDB
    char cmd[256];
    size_t msg_size = BUF_Read(buffer, cmd, sizeof(cmd)-1);
    cmd[msg_size] = 0;

    m_ReqParser.SetCommandMap(sm_BatchEndMap);
    try {
        try {
            m_ReqParser.ParseCommand(cmd);
        }
        catch (const CException&) {
            BUF_Read(buffer, 0, BUF_Size(buffer));
            WriteErr("eProtocolSyntaxError:"
                     "Batch submit error - unexpected end of batch");
        }
        m_ReqParser.SetCommandMap(sm_CommandMap);
    }
    catch (...) {
        m_ReqParser.SetCommandMap(sm_CommandMap);
        throw;
    }

    double comm_elapsed = m_BatchStopWatch.Elapsed();

    // we have our batch now
    CRequestContextSubmitGuard req_guard(m_Server,
        m_BatchClientIP, m_BatchClientSID);
    CStopWatch sw(CStopWatch::eStart);
    unsigned job_id =
        m_Queue->SubmitBatch(m_BatchJobs);
    double db_elapsed = sw.Elapsed();

    if (IsMonitoring()) {
        ITERATE(vector<CJob>, it, m_BatchJobs) {
            x_MonitorJob(*it);
        }
        CSocket& socket = GetSocket();
        string msg = "::ProcessMsgSubmitBatch ";
        msg += m_LocalTimer.GetLocalTime().AsString();
        msg += " ==> ";
        msg += socket.GetPeerAddress();
        msg += " ";
        msg += m_AuthString;
        msg += " batch block size=";
        msg += NStr::UIntToString(m_BatchSize);
        msg += " comm.time=";
        msg += NStr::DoubleToString(comm_elapsed, 4, NStr::fDoubleFixed);
        msg += " trans.time=";
        msg += NStr::DoubleToString(db_elapsed, 4, NStr::fDoubleFixed);

        MonitorPost(msg);
    }

    {{
        char buf[1024];
        sprintf(buf, "%u %s %u",
                job_id, m_Server->GetHost().c_str(),
                unsigned(m_Server->GetPort()));

        WriteOK(buf);
    }}

    m_ProcessMessage = &CNetScheduleHandler::ProcessMsgBatchHeader;
}


bool CNetScheduleHandler::IsMonitoring()
{
    return (m_Incaps & eNSAC_Queue) || m_Queue->IsMonitoring();
}


void CNetScheduleHandler::MonitorPost(const string& msg)
{
    if (m_Incaps & eNSAC_Queue)
        return;
    m_Queue->MonitorPost(msg);
}


void CNetScheduleHandler::ProcessCancel()
{
    unsigned job_id = CNetScheduleKey(m_JobReq.job_key).id;
    m_Queue->Cancel(job_id);
    WriteOK();
}


void CNetScheduleHandler::ProcessForceReschedule()
{
    unsigned job_id = CNetScheduleKey(m_JobReq.job_key).id;
    m_Queue->ForceReschedule(job_id);
    WriteOK();
}


void CNetScheduleHandler::ProcessDropJob()
{
    unsigned job_id = CNetScheduleKey(m_JobReq.job_key).id;
    m_Queue->DropJob(job_id);
    WriteOK();
}


void CNetScheduleHandler::ProcessJobRunTimeout()
{
    unsigned job_id = CNetScheduleKey(m_JobReq.job_key).id;
    if (IsMonitoring()) {
        CTime tmp_t(CTime::eCurrent);
        string msg = tmp_t.AsString();
        msg += " OBSOLETE CQueue::SetJobRunTimeout: Job id=";
        msg += NStr::IntToString(job_id);

        MonitorPost(msg);
    }
    LOG_POST(Warning << "Obsolete API SetRunTimeout called");
    NCBI_THROW(CNetScheduleException,
        eObsoleteCommand, "Use API JobDelayExpiration (cmd JDEX) instead");
}


void CNetScheduleHandler::ProcessJobDelayExpiration()
{
    unsigned job_id = CNetScheduleKey(m_JobReq.job_key).id;
    m_Queue->JobDelayExpiration(m_WorkerNodeInfo, job_id, m_JobReq.timeout);
    WriteOK();
}


void CNetScheduleHandler::ProcessStatusSnapshot()
{
    CJobStatusTracker::TStatusSummaryMap st_map;

    bool aff_exists = m_Queue->CountStatus(&st_map,
        NStr::ParseEscapes(m_JobReq.affinity_token));
    if (!aff_exists) {
        WriteErr(string("eProtocolSyntaxError:Unknown affinity token \"")
                 + m_JobReq.affinity_token + "\"");
        return;
    }
    ITERATE(CJobStatusTracker::TStatusSummaryMap, it, st_map) {
        string st_str = CNetScheduleAPI::StatusToString(it->first);
        st_str.push_back(' ');
        st_str.append(NStr::UIntToString(it->second));
        WriteOK(st_str);
    }
    WriteOK("END");
}


void CNetScheduleHandler::ProcessStatus()
{
    unsigned job_id = CNetScheduleKey(m_JobReq.job_key).id;

    TJobStatus status = m_Queue->GetStatus(job_id);
    int ret_code = 0;
    string input, output, error;

    bool res = m_Queue->GetJobDescr(job_id, &ret_code,
                                    &input, &output,
                                    &error, 0, status);
    if (!res) status = CNetScheduleAPI::eJobNotFound;
    string buf = NStr::IntToString((int) status);
    if (status != CNetScheduleAPI::eJobNotFound) {
            buf += " ";
            buf += NStr::IntToString(ret_code);
            buf += " \""   + NStr::PrintableString(output) +
                   "\" \"" + NStr::PrintableString(error) +
                   "\" \"" + NStr::PrintableString(input) +
                   "\"";
    }
    WriteOK(buf);
}


void CNetScheduleHandler::ProcessGetMessage()
{
    unsigned job_id = CNetScheduleKey(m_JobReq.job_key).id;
    string progress_msg;

    if (m_Queue->GetJobDescr(job_id, 0, 0, 0, 0,
                             &progress_msg)) {
        WriteOK(progress_msg);
    } else {
        WriteErr("Job not found");
    }
}


void CNetScheduleHandler::ProcessPutMessage()
{
    unsigned job_id = CNetScheduleKey(m_JobReq.job_key).id;

    if(m_Queue->PutProgressMessage(job_id, m_JobReq.param1))
        WriteOK();
    else
        WriteErr("Job not found");
}


void CNetScheduleHandler::ProcessGetJob()
{
    if (m_JobReq.port)
        m_WorkerNodeInfo.port = m_JobReq.port;
    // See note in PutResultGetJob below
    m_Queue->RegisterWorkerNodeVisit(m_WorkerNodeInfo);

    list<string> aff_list;
    NStr::Split(NStr::ParseEscapes(m_JobReq.affinity_token),
                "\t,", aff_list, NStr::eNoMergeDelims);
    CJob job;
    m_Queue->GetJob(m_WorkerNodeInfo,
        m_RequestContextFactory.GetPointer(), &aff_list, &job);

    unsigned job_id = job.GetId();
    if (job_id) {
        x_MakeGetAnswer(job);
        WriteOK(m_Answer);
    } else {
        WriteOK();
    }

    if (job_id && IsMonitoring()) {
        CSocket& socket = GetSocket();
        string msg = "::ProcessGetJob ";
        msg += m_LocalTimer.GetLocalTime().AsString();
        msg += m_Answer;
        msg += " ==> ";
        msg += socket.GetPeerAddress();
        msg += " ";
        msg += m_AuthString;

        MonitorPost(msg);
    }

    // We don't call UnRegisterNotificationListener here
    // because for old worker nodes it finishes the session
    // and cleans up all the information, affinity association
    // included.
    if (m_JobReq.port)  // unregister notification
        m_Queue->RegisterNotificationListener(
            m_WorkerNodeInfo, m_JobReq.port, 0);
}


void
CNetScheduleHandler::ProcessJobExchange()
{
    list<string> aff_list;
    NStr::Split(NStr::ParseEscapes(m_JobReq.affinity_token),
                "\t,", aff_list, NStr::eNoMergeDelims);

    unsigned done_job_id;
    if (!m_JobReq.job_key.empty()) {
        done_job_id = CNetScheduleKey(m_JobReq.job_key).id;
    } else {
        done_job_id = 0;
    }

    // PutResultGetJob (and for the same reason, GetJob) must ensure
    // existence of a valid worker node record before actual assigning of
    // a job to the node
    m_Queue->RegisterWorkerNodeVisit(m_WorkerNodeInfo);

    CJob job;
    string output = NStr::ParseEscapes(m_JobReq.output);
    m_Queue->PutResultGetJob(m_WorkerNodeInfo,
                             done_job_id,
                             m_JobReq.job_return_code,
                             &output,
                             // GetJob params
                             m_RequestContextFactory.GetPointer(),
                             &aff_list,
                             &job);

    unsigned job_id = job.GetId();
    if (job_id) {
        x_MakeGetAnswer(job);
        WriteOK(m_Answer);
    } else {
        WriteOK();
    }

    if (IsMonitoring()) {
        CSocket& socket = GetSocket();
        string msg = "::ProcessJobExchange ";
        msg += m_LocalTimer.GetLocalTime().AsString();
        msg += " ";
        msg += m_Answer;
        msg += " ==> ";
        msg += socket.GetPeerAddress();
        msg += " ";
        msg += m_AuthString;

        MonitorPost(msg);
    }
}


void CNetScheduleHandler::ProcessWaitGet()
{
    if (m_JobReq.port)
        m_WorkerNodeInfo.port = m_JobReq.port;
    // See note in PutResultGetJob above
    m_Queue->RegisterWorkerNodeVisit(m_WorkerNodeInfo);

    list<string> aff_list;
    NStr::Split(NStr::ParseEscapes(m_JobReq.affinity_token),
                "\t,", aff_list, NStr::eNoMergeDelims);

    CJob job;
    m_Queue->GetJob(m_WorkerNodeInfo,
                    m_RequestContextFactory.GetPointer(),
                    &aff_list,
                    &job);

    unsigned job_id = job.GetId();
    if (job_id) {
        x_MakeGetAnswer(job);
        WriteOK(m_Answer);
        return;
    }

    // job not found, initiate waiting mode
    WriteOK();

    m_Queue->RegisterNotificationListener(
        m_WorkerNodeInfo, m_JobReq.port, m_JobReq.timeout);
}


void CNetScheduleHandler::ProcessRegisterClient()
{
    if (m_JobReq.port)
        m_WorkerNodeInfo.port = m_JobReq.port;
    m_Queue->RegisterNotificationListener(
        m_WorkerNodeInfo, m_JobReq.port, 0);
    WriteOK();
}


void CNetScheduleHandler::ProcessUnRegisterClient()
{
    m_Queue->UnRegisterNotificationListener(m_WorkerNodeInfo);
    WriteOK();
}

void CNetScheduleHandler::ProcessQList()
{
    WriteOK(m_Server->GetQueueNames(";"));
}


void CNetScheduleHandler::ProcessError(const CException& ex) {
    WriteErr(ex.GetMsg());
}


void CNetScheduleHandler::ProcessQuitSession(void)
{
    // read trailing input (see netcached.cpp for more comments on "why?")
    CSocket& socket = GetSocket();
    STimeout to; to.sec = to.usec = 0;
    socket.SetTimeout(eIO_Read, &to);
    socket.Read(NULL, 1024);
    socket.Close();
}


void CNetScheduleHandler::ProcessPutFailure()
{
    unsigned job_id = CNetScheduleKey(m_JobReq.job_key).id;
    m_Queue->FailJob(m_WorkerNodeInfo, job_id,
                     NStr::ParseEscapes(m_JobReq.err_msg), 
                     NStr::ParseEscapes(m_JobReq.output),
                     m_JobReq.job_return_code);
    WriteOK();
}


void CNetScheduleHandler::ProcessPut()
{
    unsigned job_id = CNetScheduleKey(m_JobReq.job_key).id;
    m_Queue->PutResult(m_WorkerNodeInfo,
                       job_id, m_JobReq.job_return_code, 
                       NStr::ParseEscapes(m_JobReq.output));
    WriteOK();
}


void CNetScheduleHandler::ProcessReturn()
{
    unsigned job_id = CNetScheduleKey(m_JobReq.job_key).id;
    m_Queue->ReturnJob(m_WorkerNodeInfo, job_id);
    WriteOK();
}


void CNetScheduleHandler::ProcessDropQueue()
{
    m_Queue->Truncate();
    WriteOK();
}


void CNetScheduleHandler::ProcessMonitor()
{
    CSocket& socket = GetSocket();
    socket.DisableOSSendDelay(false);
    WriteOK(NETSCHEDULED_FULL_VERSION);
    string started = "Started: " + m_Server->GetStartTime().AsString();
    WriteOK(started);
    // Socket is released from regular scheduling here - it is
    // write only for monitor since this moment.
    m_Queue->SetMonitorSocket(socket);
}


void CNetScheduleHandler::ProcessPrintQueue()
{
    // TODO this method can not support session, because socket is closed at
    // the end.
    TJobStatus job_status = CNetScheduleAPI::StringToStatus(m_JobReq.param1);

    if (job_status == CNetScheduleAPI::eJobNotFound) {
        string err_msg = "Status unknown: " + m_JobReq.param1;
        WriteErr(err_msg);
        return;
    }

    CSocket& socket = GetSocket();
    SOCK sock = socket.GetSOCK();
    socket.SetOwnership(eNoOwnership);
    socket.Reset(0, eTakeOwnership, eCopyTimeoutsToSOCK);

    CConn_SocketStream ios(sock);  // sock is being passed and used exclusively

    m_Queue->PrintQueue(ios,
                        job_status,
                        m_Server->GetHost(),
                        m_Server->GetPort());

    ios << "OK:END" << endl;
}


void CNetScheduleHandler::ProcessDump()
{
    // TODO this method can not support session, because socket is closed at
    // the end.
    CSocket& socket = GetSocket();
    SOCK sock = socket.GetSOCK();
    socket.SetOwnership(eNoOwnership);
    socket.Reset(0, eTakeOwnership, eCopyTimeoutsToSOCK);

    CConn_SocketStream ios(sock);

    ios << "OK:" << NETSCHEDULED_FULL_VERSION << endl;

    if (m_JobReq.job_key.empty()) {
        ios << "OK:" << "[Job status matrix]:";

        m_Queue->PrintJobStatusMatrix(ios);

        ios << "OK:[Job DB]:" << endl;
        m_Queue->PrintAllJobDbStat(ios);
    } else {
        try {
            unsigned job_id = CNetScheduleKey(m_JobReq.job_key).id;

            TJobStatus status = m_Queue->GetStatus(job_id);

            string st_str = CNetScheduleAPI::StatusToString(status);
            ios << "OK:" << "[Job status matrix]:" << st_str;
            ios << "OK:[Job DB]:" << endl;
            m_Queue->PrintJobDbStat(job_id, ios);
        }
        catch (CException&)
        {
            // dump by status
            TJobStatus
                job_status = CNetScheduleAPI::StringToStatus(
                    m_JobReq.job_key);

            if (job_status == CNetScheduleAPI::eJobNotFound) {
                ios << "ERR:" << "Status unknown: " << m_JobReq.job_key;
                return;
            }
        }
    }
    ios << "OK:END" << endl;
}

void CNetScheduleHandler::ProcessReloadConfig()
{
    CNcbiApplication* app = CNcbiApplication::Instance();

    bool reloaded = app->ReloadConfig(CMetaRegistry::fReloadIfChanged);
    if (reloaded)
        m_Server->Configure(app->GetConfig());
    WriteOK();
}


void CNetScheduleHandler::x_StatisticsNew(const string& what,
                                          time_t curr)
{
    CSocket& socket = GetSocket();
    if (what == "WNODE") {
        CNcbiOstrstream ostr;
        m_Queue->PrintWorkerNodeStat(ostr, curr, eWNF_WithNodeId);
        ostr << ends;

        char* stat_str = ostr.str();
        try {
            WriteMsg("OK:", stat_str, true);
        } catch (...) {
            ostr.freeze(false);
            throw;
        }
        ostr.freeze(false);
    }
    if (what == "JOBS") {
        unsigned total = 0;
        for (int i = CNetScheduleAPI::ePending;
            i < CNetScheduleAPI::eLastStatus; ++i) {
                TJobStatus st = TJobStatus(i);
                unsigned count = m_Queue->CountStatus(st);
                total += count;

                string st_str = CNetScheduleAPI::StatusToString(st);
                st_str += ": ";
                st_str += NStr::UIntToString(count);

                WriteMsg("OK:", st_str, true);
        } // for
        WriteMsg("OK:", "Total: " + NStr::UIntToString(total), true);
    }
}


void CNetScheduleHandler::ProcessStatistics()
{
    CSocket& socket = GetSocket();
    socket.DisableOSSendDelay(false);
    const string& what = m_JobReq.param1;
    time_t curr = time(0);

    if (!what.empty() && what != "ALL") {
        x_StatisticsNew(what, curr);
        WriteOK("END");
        return;
    }

    WriteOK(NETSCHEDULED_FULL_VERSION);
    string started = "Started: " + m_Server->GetStartTime().AsString();
    WriteOK(started);

    string load_str = "Load: jobs dispatched: ";
    load_str +=
        NStr::DoubleToString(m_Queue->GetAverage(SLockedQueue::eStatGetEvent));
    load_str += "/sec, jobs complete: ";
    load_str +=
        NStr::DoubleToString(m_Queue->GetAverage(SLockedQueue::eStatPutEvent));
    load_str += "/sec, DB locks: ";
    load_str +=
        NStr::DoubleToString(m_Queue->GetAverage(SLockedQueue::eStatDBLockEvent));
    load_str += "/sec, Job DB writes: ";
    load_str +=
        NStr::DoubleToString(m_Queue->GetAverage(SLockedQueue::eStatDBWriteEvent));
    load_str += "/sec";
    WriteOK(load_str);

    for (int i = CNetScheduleAPI::ePending;
         i < CNetScheduleAPI::eLastStatus; ++i) {
        TJobStatus st = TJobStatus(i);
        unsigned count = m_Queue->CountStatus(st);


        string st_str = CNetScheduleAPI::StatusToString(st);
        st_str += ": ";
        st_str += NStr::UIntToString(count);

        WriteMsg("OK:", st_str, true);

        if (m_JobReq.param1 == "ALL") {
            TNSBitVector::statistics bv_stat;
            m_Queue->StatusStatistics(st, &bv_stat);
            st_str = "   bit_blk=";
            st_str.append(NStr::UIntToString(bv_stat.bit_blocks));
            st_str += "; gap_blk=";
            st_str.append(NStr::UIntToString(bv_stat.gap_blocks));
            st_str += "; mem_used=";
            st_str.append(NStr::UIntToString(bv_stat.memory_used));
            WriteOK(st_str);
        }
    } // for

    /*
    if (m_JobReq.param1 == "ALL") {

        unsigned db_recs = m_Queue->CountRecs();
        string recs = "Records:";
        recs += NStr::UIntToString(db_recs);
        WriteOK(recs);
        WriteOK("[Database statistics]:");

        {{
            CNcbiOstrstream ostr;
            m_Queue->PrintStat(ostr);
            ostr << ends;

            char* stat_str = ostr.str();
    //        size_t os_str_len = ostr.pcount()-1;
    //        stat_str[os_str_len] = 0;
            try {
                WriteOK(stat_str);
            } catch (...) {
                ostr.freeze(false);
                throw;
            }
            ostr.freeze(false);
        }}
    }
    */

    if (m_JobReq.param1 == "ALL") {
        WriteOK("[Berkeley DB Mutexes]:");
        {{
            CNcbiOstrstream ostr;
            m_Queue->PrintMutexStat(ostr);
            ostr << ends;

            char* stat_str = ostr.str();
            try {
                WriteMsg("OK:", stat_str, true);
            } catch (...) {
                ostr.freeze(false);
                throw;
            }
            ostr.freeze(false);
        }}

        WriteOK("[Berkeley DB Locks]:");
        {{
            CNcbiOstrstream ostr;
            m_Queue->PrintLockStat(ostr);
            ostr << ends;

            char* stat_str = ostr.str();
            try {
                WriteMsg("OK:", stat_str, true);
            } catch (...) {
                ostr.freeze(false);
                throw;
            }
            ostr.freeze(false);
        }}

        WriteOK("[Berkeley DB Memory Usage]:");
        {{
            CNcbiOstrstream ostr;
            m_Queue->PrintMemStat(ostr);
            ostr << ends;

            char* stat_str = ostr.str();
            try {
                WriteMsg("OK:", stat_str, true);
            } catch (...) {
                ostr.freeze(false);
                throw;
            }
            ostr.freeze(false);
        }}

        WriteOK("[BitVector block pool]:");

        {{
            const TBlockAlloc::TBucketPool::TBucketVector& bv =
                TBlockAlloc::GetPoolVector();
            size_t pool_vec_size = bv.size();
            string tmp_str = "Pool vector size: ";
            tmp_str.append(NStr::UIntToString(pool_vec_size));
            WriteOK(tmp_str);
            for (size_t i = 0; i < pool_vec_size; ++i) {
                const TBlockAlloc::TBucketPool::TResourcePool* rp =
                    TBlockAlloc::GetPool(i);
                if (rp) {
                    size_t pool_size = rp->GetSize();
                    if (pool_size) {
                        tmp_str = "Pool [ ";
                        tmp_str.append(NStr::UIntToString(i));
                        tmp_str.append("] = ");
                        tmp_str.append(NStr::UIntToString(pool_size));

                        WriteOK(tmp_str);
                    }
                }
            }
        }}
    }

    WriteOK("[Worker node statistics]:");

    {{
        CNcbiOstrstream ostr;
        m_Queue->PrintWorkerNodeStat(ostr, curr);
        ostr << ends;

        char* stat_str = ostr.str();
        try {
            WriteMsg("OK:", stat_str, true);
        } catch (...) {
            ostr.freeze(false);
            throw;
        }
        ostr.freeze(false);
    }}

    {{
        WriteOK("[Configured job submitters]:");
        CNcbiOstrstream ostr;
        m_Queue->PrintSubmHosts(ostr);
        ostr << ends;

        char* stat_str = ostr.str();
        try {
            WriteMsg("OK:", stat_str, true);
        } catch (...) {
            ostr.freeze(false);
            throw;
        }
        ostr.freeze(false);
    }}

    {{
        WriteOK("[Configured workers]:");
        CNcbiOstrstream ostr;
        m_Queue->PrintWNodeHosts(ostr);
        ostr << ends;

        char* stat_str = ostr.str();
        try {
            WriteMsg("OK:", stat_str, true);
        } catch (...) {
            ostr.freeze(false);
            throw;
        }
        ostr.freeze(false);
    }}

    WriteOK("END");
}


void CNetScheduleHandler::ProcessLog()
{
    const char* str = m_JobReq.param1.c_str();
    if (NStr::strcasecmp(str, "ON") == 0) {
        m_Server->SetLogging(true);
        LOG_POST("Logging turned ON");
    } else if (NStr::strcasecmp(str, "OFF") == 0) {
        m_Server->SetLogging(false);
        LOG_POST("Logging turned OFF");
    } else {
        WriteErr("eProtocolSyntaxError:Incorrect LOG syntax");
    }
    WriteOK();
}


void CNetScheduleHandler::ProcessShutdown()
{
    CSocket& socket = GetSocket();
    string admin_host = socket.GetPeerAddress();
    string msg = "Shutdown request... ";
    msg += admin_host;
    msg += " ";
    msg += CTime(CTime::eCurrent).AsString();
    LOG_POST(Info << msg);
    m_Server->SetShutdownFlag();
    WriteOK();
}


void CNetScheduleHandler::ProcessVersion()
{
    WriteOK(NETSCHEDULED_FULL_VERSION);
}


void CNetScheduleHandler::ProcessCreateQueue()
{
    const string& qname  = m_JobReq.param1;
    const string& qclass = m_JobReq.param2;
    const string& comment = NStr::ParseEscapes(m_JobReq.param3);
    m_Server->CreateQueue(qname, qclass, comment);
    WriteOK();
}

void CNetScheduleHandler::ProcessDeleteQueue()
{
    const string& qname = m_JobReq.param1;
    m_Server->DeleteQueue(qname);
    WriteOK();
}


void CNetScheduleHandler::ProcessQueueInfo()
{
    const string& qname = m_JobReq.param1;
    int kind;
    string qclass;
    string comment;
    m_Server->QueueInfo(qname, kind, &qclass, &comment);
    WriteOK(NStr::IntToString(kind) + "\t" + qclass + "\t\"" +
            NStr::PrintableString(comment) + "\"");
}


void CNetScheduleHandler::ProcessQuery()
{
    string result_str;

    string query = NStr::ParseEscapes(m_JobReq.param1);

    list<string> fields;
    m_SelectedIds.reset(m_Queue->ExecSelect(query, fields));

    string& action = m_JobReq.param2;

    if (!fields.empty()) {
        WriteErr(string("eProtocolSyntaxError:") +
                 "SELECT is not allowed");
        m_SelectedIds.release();
        return;
    } else if (action == "SLCT") {
        // Execute 'projection' phase
        string str_fields(NStr::ParseEscapes(m_JobReq.param3));
        // Split fields
        NStr::Split(str_fields, "\t,", fields, NStr::eNoMergeDelims);
        m_Queue->PrepareFields(m_FieldDescr, fields);
        m_DelayedOutput = &CNetScheduleHandler::WriteProjection;
        return;
    } else if (action == "COUNT") {
        result_str = NStr::IntToString(m_SelectedIds->count());
    } else if (action == "IDS") {
        result_str = x_SerializeBitVector(*m_SelectedIds);
    } else if (action == "DROP") {
    } else if (action == "FRES") {
    } else if (action == "CNCL") {
    } else {
        WriteErr(string("eProtocolSyntaxError:") +
                 "Unknown action " + action);
        m_SelectedIds.release();
        return;
    }
    WriteOK(result_str);
    m_SelectedIds.release();
}


void CNetScheduleHandler::ProcessSelectQuery()
{
    string result_str;

    string query = NStr::ParseEscapes(m_JobReq.param1);

    list<string> fields;
    m_SelectedIds.reset(m_Queue->ExecSelect(query, fields));

    if (fields.empty()) {
        WriteErr(string("eProtocolSyntaxError:") +
                 "SQL-like select is expected");
        m_SelectedIds.release();
    } else {
        m_Queue->PrepareFields(m_FieldDescr, fields);
        m_DelayedOutput = &CNetScheduleHandler::WriteProjection;
    }
}


/*
static bool Less(const CQueue::TRecord& elem1, const CQueue::TRecord& elem2)
{
    int size = min(elem1.size(), elem2.size());
    for (int i = 0; i < size; ++i) {
        // try to convert both elements to integer
        const string& p1 = elem1[i];
        const string& p2 = elem2[i];
        try {
            int i1 = NStr::StringToInt(p1);
            int i2 = NStr::StringToInt(p2);
            if (i1 < i2) return true;
            if (i1 > i2) return false;
        } catch (CStringException&) {
            if (p1 < p2) return true;
            if (p1 > p2) return false;
        }
    }
    if (elem1.size() < elem2.size()) return true;
    return false;
}
*/


void CNetScheduleHandler::WriteProjection()
{
    unsigned chunk_size = 10000;

    unsigned n;
    TNSBitVector chunk;
    unsigned job_id = 0;
    for (n = 0; n < chunk_size &&
                (job_id = m_SelectedIds->extract_next(job_id));
         ++n)
    {
        chunk.set(job_id);
    }
    if (n > 0) {
        SQueueDescription qdesc;
        qdesc.host = m_Server->GetHost().c_str();
        qdesc.port = m_Server->GetPort();
        CQueue::TRecordSet record_set;
        m_Queue->ExecProject(record_set, chunk, m_FieldDescr);
        /*
        sort(record_set.begin(), record_set.end(), Less);

        list<string> string_set;
        ITERATE(TRecordSet, it, record_set) {
            string_set.push_back(NStr::Join(*it, "\t"));
        }
        list<string> out_set;
        unique_copy(string_set.begin(), string_set.end(),
            back_insert_iterator<list<string> > (out_set));
        out_set.push_front(NStr::IntToString(out_set.size()));
        result_str = NStr::Join(out_set, "\n");
        */
        ITERATE(CQueue::TRecordSet, it, record_set) {
            const CQueue::TRecord& rec = *it;
            string str_rec;
            for (unsigned i = 0; i < rec.size(); ++i) {
                if (i) str_rec += '\t';
                const string& field = rec[i];
                if (m_FieldDescr.formatters[i])
                    str_rec += m_FieldDescr.formatters[i](field, &qdesc);
                else
                    str_rec += field;
            }
            WriteOK(str_rec);
        }
    }
    if (!m_SelectedIds->any()) {
        WriteOK("END");
        m_SelectedIds.release();
        m_DelayedOutput = NULL;
    }
}


void CNetScheduleHandler::ProcessGetParam()
{
    string res("max_input_size=");
    res += NStr::IntToString(m_Queue->GetMaxInputSize());
    res += ";max_output_size=";
    res += NStr::IntToString(m_Queue->GetMaxOutputSize());
    res += ";";
    res += NETSCHEDULED_FEATURES;
    WriteOK(res);
}


void CNetScheduleHandler::ProcessGetConfiguration()
{
    unsigned nParams;
    nParams = m_Queue->GetNumParams();
    for (unsigned n = 0; n < nParams; ++n) {
        string s = m_Queue->GetParamName(n);
        s += '=';
        s += m_Queue->GetParamValue(n);
        WriteOK(s);
    }
    WriteOK("END");
}


void CNetScheduleHandler::ProcessReading()
{
    unsigned read_id;
    TNSBitVector jobs;
    m_Queue->ReadJobs(m_PeerAddr,
                      m_JobReq.count, m_JobReq.timeout, read_id, jobs);
    WriteOK(NStr::UIntToString(read_id) + " " + NS_EncodeBitVector(jobs));
}


void CNetScheduleHandler::ProcessConfirm()
{
    TNSBitVector jobs = NS_DecodeBitVector(m_JobReq.output);
    m_Queue->ConfirmJobs(m_JobReq.count, jobs);
    WriteOK();
}


void CNetScheduleHandler::ProcessReadFailed()
{
    TNSBitVector jobs = NS_DecodeBitVector(m_JobReq.output);
//    m_JobReq.err_msg; we still don't (and probably, won't) use this
    m_Queue->FailReadingJobs(m_JobReq.count, jobs);
    WriteOK();
}


void CNetScheduleHandler::ProcessGetAffinityList()
{
    WriteOK(m_Queue->GetAffinityList(m_WorkerNodeInfo));
}


void CNetScheduleHandler::ProcessInitWorkerNode()
{
    if (!m_WorkerNodeInfo.node_id.empty())
        m_Queue->ClearWorkerNode(m_WorkerNodeInfo.node_id);
    m_WorkerNodeInfo.port    = m_JobReq.port;
    m_WorkerNodeInfo.node_id = m_JobReq.param1.substr(0, kMaxWorkerNodeIdSize);
    m_Queue->InitWorkerNode(m_WorkerNodeInfo);
    WriteOK();
}


void CNetScheduleHandler::ProcessClearWorkerNode()
{
    m_Queue->ClearWorkerNode(m_JobReq.param1);
    WriteOK();
}


//////////////////////////////////////////////////////////////////////////

/// NetSchedule command parser
///
/// @internal
///

CNetScheduleHandler::SCommandMap CNetScheduleHandler::sm_CommandMap[] = {
    // SST job_key : id -- submitter fast status, changes timestamp
    { "SST",      &CNetScheduleHandler::ProcessFastStatusS, eNSCR_Submitter,
        { { "job_key", eNSPT_Id, eNSPA_Required } } },
    // WST job_key : id -- worker node fast status, does not change timestamp
    { "WST",      &CNetScheduleHandler::ProcessFastStatusW, eNSCR_Worker,
        { { "job_key", eNSPT_Id, eNSPA_Required } } },
    // SUBMIT input : str [ progress_msg : str ] [ port : uint [ timeout : uint ]]
    //        [ affinity_token : keystr(aff) ] [ job_mask : keyint(msk) ]
    //        [ tags : keystr(tags) ]
    { "SUBMIT",   &CNetScheduleHandler::ProcessSubmit, eNSCR_Submitter,
        { { "input",        eNSPT_Str, eNSPA_Required },        // input
          { "progress_msg", eNSPT_Str, eNSPA_Optional },        // param1
          { "port",         eNSPT_Int, eNSPA_Optional },
          { "timeout",      eNSPT_Int, eNSPA_Optional },
          { "aff",          eNSPT_Str, eNSPA_Optional, "" },    // affinity_token
          { "msk",          eNSPT_Int, eNSPA_Optional, "0" },
          { "tags",         eNSPT_Str, eNSPA_Optional, "" },    // tags
          { "ip",           eNSPT_Str, eNSPA_Optional, "" },    // param3
          { "sid",          eNSPT_Str, eNSPA_Optional, "" } } },// param2
    // CANCEL job_key : id
    { "CANCEL",   &CNetScheduleHandler::ProcessCancel, eNSCR_Submitter,
        { { "job_key", eNSPT_Id, eNSPA_Required } } },
    // STATUS job_key : id
    { "STATUS",   &CNetScheduleHandler::ProcessStatus, eNSCR_Queue,
        { { "job_key", eNSPT_Id, eNSPA_Required } } },
    // GET [ port : int ] [affinity_list : keystr(aff) ]
    { "GET",      &CNetScheduleHandler::ProcessGetJob, eNSCR_Worker,
        { { "port", eNSPT_Id,  eNSPA_Optional },
          { "aff",  eNSPT_Str, eNSPA_Optional, "" } } },
    // PUT job_key : id  job_return_code : int  output : str
    { "PUT",      &CNetScheduleHandler::ProcessPut, eNSCR_Worker,
        { { "job_key",         eNSPT_Id,  eNSPA_Required },
          { "job_return_code", eNSPT_Id,  eNSPA_Required },
          { "output",          eNSPT_Str, eNSPA_Required } } },
    // RETURN job_key : id
    { "RETURN",   &CNetScheduleHandler::ProcessReturn, eNSCR_Worker,
        { { "job_key", eNSPT_Id, eNSPA_Required } } },
    { "SHUTDOWN", &CNetScheduleHandler::ProcessShutdown, eNSCR_Admin },
    { "VERSION",  &CNetScheduleHandler::ProcessVersion, eNSCR_Any },
    // LOG option : id -- "ON" or "OFF"
    { "LOG",      &CNetScheduleHandler::ProcessLog, eNSCR_Any, // ?? Admin
        { { "option", eNSPT_Id, eNSPA_Required } } },
    // STAT [ option : id ] -- "ALL"
    { "STAT",     &CNetScheduleHandler::ProcessStatistics, eNSCR_Queue,
        { { "option", eNSPT_Id, eNSPA_Optional } } },
    { "QUIT",     &CNetScheduleHandler::ProcessQuitSession, eNSCR_Any },
    { "DROPQ",    &CNetScheduleHandler::ProcessDropQueue, eNSCR_QueueAdmin },
    // WGET port : uint  timeout : uint
    //      [affinity_list : keystr(aff) ]
    { "WGET",     &CNetScheduleHandler::ProcessWaitGet, eNSCR_Worker,
        { { "port",    eNSPT_Int, eNSPA_Required },
          { "timeout", eNSPT_Int, eNSPA_Required },
          { "aff",     eNSPT_Str, eNSPA_Optional, "" } } },
    // JRTO job_key : id  timeout : uint     OBSOLETE, throws exception
    { "JRTO",     &CNetScheduleHandler::ProcessJobRunTimeout, eNSCR_Worker,
        { { "job_key", eNSPT_Id,  eNSPA_Required },
          { "timeout", eNSPT_Int, eNSPA_Required } } },
    // DROJ job_key : id
    { "DROJ",     &CNetScheduleHandler::ProcessDropJob, eNSCR_Submitter,
        { { "job_key", eNSPT_Id, eNSPA_Required } } },
    // FPUT job_key : id  err_msg : str  output : str  job_return_code : int
    { "FPUT",     &CNetScheduleHandler::ProcessPutFailure, eNSCR_Worker,
        { { "job_key",         eNSPT_Id,  eNSPA_Required },
          { "err_msg",         eNSPT_Str, eNSPA_Required },
          { "output",          eNSPT_Str, eNSPA_Required },
          { "job_return_code", eNSPT_Int, eNSPA_Required } } },
    // MPUT job_key : id  progress_msg : str
    { "MPUT",     &CNetScheduleHandler::ProcessPutMessage, eNSCR_Queue,
        { { "job_key",      eNSPT_Id, eNSPA_Required },
          { "progress_msg", eNSPT_Str, eNSPA_Required } } },
    // MGET job_key : id
    { "MGET",     &CNetScheduleHandler::ProcessGetMessage, eNSCR_Queue,
        { { "job_key", eNSPT_Id, eNSPA_Required } } },
    { "MONI",     &CNetScheduleHandler::ProcessMonitor, eNSCR_Queue },
    // DUMP [ job_key : id ]
    { "DUMP",     &CNetScheduleHandler::ProcessDump, eNSCR_Queue,
        { { "job_key", eNSPT_Id, eNSPA_Optional } } },
    { "RECO",     &CNetScheduleHandler::ProcessReloadConfig, eNSCR_Any }, // ?? Admin
    { "QLST",     &CNetScheduleHandler::ProcessQList, eNSCR_Any },
    { "BSUB",     &CNetScheduleHandler::ProcessSubmitBatch, eNSCR_Submitter,
        { { "port",         eNSPT_Int, eNSPA_Optional },
          { "timeout",      eNSPT_Int, eNSPA_Optional },
          { "ip",           eNSPT_Str, eNSPA_Optional, "" },
          { "sid",          eNSPT_Str, eNSPA_Optional, "" } } },
    // JXCG [ job_key : id [ job_return_code : int [ output : str ] ] ]
    //      [affinity_list : keystr(aff) ]
    { "JXCG",     &CNetScheduleHandler::ProcessJobExchange, eNSCR_Worker,
        { { "job_key",         eNSPT_Id,  eNSPA_Optchain },
          { "job_return_code", eNSPT_Int, eNSPA_Optchain },
          { "output",          eNSPT_Str, eNSPA_Optional },
          { "aff",             eNSPT_Str, eNSPA_Optional, "" } } },
    // REGC port : uint
    { "REGC",     &CNetScheduleHandler::ProcessRegisterClient, eNSCR_Worker,
        { { "port", eNSPT_Int, eNSPA_Required } } },
    // URGC port : uint
    { "URGC",     &CNetScheduleHandler::ProcessUnRegisterClient, eNSCR_Worker,
        { { "port", eNSPT_Int, eNSPA_Required } } },
    // QPRT status : id
    { "QPRT",     &CNetScheduleHandler::ProcessPrintQueue, eNSCR_Queue,
        { { "status", eNSPT_Id, eNSPA_Required } } },
    // FRES job_key : id
    { "FRES",     &CNetScheduleHandler::ProcessForceReschedule, eNSCR_Submitter,
        { { "job_key", eNSPT_Id, eNSPA_Required } } },
    // JDEX job_key : id timeout : uint
    { "JDEX",     &CNetScheduleHandler::ProcessJobDelayExpiration, eNSCR_Worker,
        { { "job_key", eNSPT_Id,  eNSPA_Required },
          { "timeout", eNSPT_Int, eNSPA_Required } } },
    // STSN [ affinity_token : keystr(aff) ]
    { "STSN",     &CNetScheduleHandler::ProcessStatusSnapshot, eNSCR_Queue,
        { { "aff", eNSPT_Str, eNSPA_Optional, "" } } },
    // QCRE qname : id  qclass : id [ comment : str ]
    { "QCRE",     &CNetScheduleHandler::ProcessCreateQueue, eNSAC_DynClassAdmin,
        { { "qname",   eNSPT_Id,  eNSPA_Required },
          { "qclass",  eNSPT_Id,  eNSPA_Required },
          { "comment", eNSPT_Str, eNSPA_Optional } } },
    // QDEL qname : id
    { "QDEL",     &CNetScheduleHandler::ProcessDeleteQueue, eNSAC_DynQueueAdmin,
        { { "qname", eNSPT_Id, eNSPA_Required } } },
    { "QINF",     &CNetScheduleHandler::ProcessQueueInfo, eNSCR_Any,
        { { "qname", eNSPT_Id, eNSPA_Required } } },
    { "QERY",     &CNetScheduleHandler::ProcessQuery, eNSCR_Queue,
        { { "where",  eNSPT_Str, eNSPA_Required },
          { "action", eNSPT_Id,  eNSPA_Optional, "COUNT" },
          { "fields", eNSPT_Str, eNSPA_Optional } } },
    { "QSEL",     &CNetScheduleHandler::ProcessSelectQuery, eNSCR_Queue,
        { { "select", eNSPT_Str, eNSPA_Required } } },
    { "GETP",     &CNetScheduleHandler::ProcessGetParam, eNSCR_Queue },
    { "GETC",     &CNetScheduleHandler::ProcessGetConfiguration, eNSCR_Queue },
    // READ limit : int [ timeout : int ] -> group : int jobs : str (encoded_vec)
    { "READ",     &CNetScheduleHandler::ProcessReading, eNSCR_Submitter,
        { { "count",   eNSPT_Int, eNSPA_Required },
          { "timeout", eNSPT_Int, eNSPA_Optional, "0" } } },
    // CFRM group : int jobs : str
    { "CFRM",     &CNetScheduleHandler::ProcessConfirm, eNSCR_Submitter,
        { { "count",  eNSPT_Int, eNSPA_Required },
          { "output", eNSPT_Str, eNSPA_Required } } },
    // FRED group : int jobs : str [ message : str ]
    { "FRED",     &CNetScheduleHandler::ProcessReadFailed, eNSCR_Submitter,
        { { "count",   eNSPT_Int, eNSPA_Required },
          { "output",  eNSPT_Str, eNSPA_Required },
          { "err_msg", eNSPT_Str, eNSPA_Optional } } },
    // AFLS
    { "AFLS",     &CNetScheduleHandler::ProcessGetAffinityList, eNSCR_Worker },
    // INIT port : uint id : string
    { "INIT",     &CNetScheduleHandler::ProcessInitWorkerNode, eNSCR_Worker,
        { { "port", eNSPT_Int, eNSPA_Required },
          { "guid", eNSPT_Str, eNSPA_Required } } },
    // CLRN id : string
    { "CLRN",     &CNetScheduleHandler::ProcessClearWorkerNode, eNSCR_Any,
        { { "guid", eNSPT_Str, eNSPA_Required } } },
    { NULL },
};


void CNetScheduleHandler::WriteMsg(const char*   prefix,
                                   const string& msg,
                                   bool          comm_control)
{
    CSocket& socket = GetSocket();
    size_t msg_length = 0;
    const char* buf_ptr;
    string buffer;
    size_t prefix_size = strlen(prefix);

    if (prefix_size + msg.size() + 2 > kMaxMessageSize) {
        buffer = prefix;
        buffer += msg;
        buffer += "\r\n";
        msg_length = buffer.size();
        buf_ptr = buffer.data();
    } else {
        char* ptr = m_MsgBuffer;
        strcpy(ptr, prefix);
        ptr += prefix_size;
        size_t msg_size = msg.size();
        memcpy(ptr, msg.data(), msg_size);
        ptr += msg_size;
        *ptr++ = '\r';
        *ptr++ = '\n';
        msg_length = ptr - m_MsgBuffer;
        buf_ptr = m_MsgBuffer;
    }

    if (m_Server->IsLog()) {
        string lmsg;
        lmsg = string("ANS:") + NStr::IntToString(m_CommandNumber);
        lmsg += ';';
        lmsg += m_AuthString;
        lmsg += ';';
        lmsg += m_QueueName;
        lmsg += ';';
        size_t log_length = min(msg_length, (size_t) 1024);
        bool shortened = log_length < msg_length;
        while (log_length > 0 && (buf_ptr[log_length-1] == '\n' ||
                                  buf_ptr[log_length-1] == '\r')) {
                log_length--;
        }
        lmsg += string(buf_ptr, log_length);
        if (shortened) lmsg += "...";
        NCBI_NS_NCBI::CNcbiDiag(eDiag_Info, eDPF_Log).GetRef()
            << lmsg
            << NCBI_NS_NCBI::Endm;
    }

    size_t n_written;
    EIO_Status io_st =
        socket.Write(buf_ptr, msg_length, &n_written);
    if (comm_control && io_st) {
        NCBI_THROW(CNetServiceException,
                   eCommunicationError, "Socket write error.");
    }
}


void CNetScheduleHandler::WriteOK(const string& msg)
{
    WriteMsg("OK:", msg);
}


void CNetScheduleHandler::WriteErr(const string& msg)
{
    WriteMsg("ERR:", msg);
}


void CNetScheduleHandler::x_MakeLogMessage(string& lmsg)
{
    CSocket& socket = GetSocket();
    string peer = socket.GetPeerAddress();
    // get only host name
    string::size_type offs = peer.find_first_of(":");
    if (offs != string::npos) {
        peer.erase(offs, peer.length());
    }

    lmsg = string("REQ:") + NStr::IntToString(m_CommandNumber) + ';' + peer + ';';
    lmsg += m_AuthString;
    lmsg += ';';
    lmsg += m_QueueName;
    lmsg += ';';
    lmsg += m_Request;
}


void CNetScheduleHandler::x_MakeGetAnswer(const CJob& job)
{
    string& answer = m_Answer;
    answer = CNetScheduleKey(job.GetId(),
        m_Server->GetHost(), m_Server->GetPort());
    answer.append(" \"");
    answer.append(NStr::PrintableString(job.GetInput()));
    answer.append("\"");

    // We can re-use old jout and jerr job parameters for affinity and
    // session id/client ip respectively.
    answer.append(" \"");
    answer.append(NStr::PrintableString(job.GetAffinityToken()));
    answer.append("\" \"");
    answer.append(job.GetClientIP() + " " + job.GetClientSID());
    answer.append("\""); // to keep compat. with jout & jerr

    answer.append(" ");
    answer.append(NStr::UIntToString(job.GetMask()));
}


//////////////////////////////////////////////////////////////////////////
// CNetScheduleServer implementation
CNetScheduleServer::CNetScheduleServer()
:   m_BackgroundHost(this),
    m_Port(0),
    m_HostNetAddr(0),
    m_Shutdown(false),
    m_SigNum(0),
    m_InactivityTimeout(0),
    m_QueueDB(0),
    m_StartTime(CTime::eCurrent)
{
    m_AtomicCommandNumber.Set(1);
    s_netschedule_server = this;
}


CNetScheduleServer::~CNetScheduleServer()
{
    delete m_QueueDB;
}


void CNetScheduleServer::AddDefaultListener(IServer_ConnectionFactory* factory)
{
    // port must be set before listening
    _ASSERT(m_Port);
    AddListener(factory, m_Port);
}


void CNetScheduleServer::SetNSParameters(const SNS_Parameters& params)
{
    CServer::SetParameters(params);
    m_Port = params.port;
    m_HostNetAddr = CSocketAPI::gethostbyname(kEmptyStr);
    if (params.use_hostname) {
        m_Host = CSocketAPI::gethostname();
    } else {
        NS_FormatIPAddress(m_HostNetAddr, m_Host);
    }

    m_InactivityTimeout = params.network_timeout;
    m_LogFlag.Set(params.is_log ? 1 : 0);
    m_AdminHosts.SetHosts(params.admin_hosts);
}


void CNetScheduleServer::Exit()
{
    m_QueueDB->Close();
}


///////////////////////////////////////////////////////////////////////


/// NetSchedule daemon application
///
/// @internal
///
class CNetScheduleDApp : public CNcbiApplication
{
public:
    CNetScheduleDApp()
        : CNcbiApplication()
    {}
    void Init(void);
    int Run(void);
private:
    STimeout m_ServerAcceptTimeout;
};


void CNetScheduleDApp::Init(void)
{
    SetDiagPostLevel(eDiag_Info);
    SetDiagPostFlag(eDPF_DateTime);

    // Convert multi-line diagnostic messages into one-line ones by default.
    SetDiagPostFlag(eDPF_PreMergeLines);
    SetDiagPostFlag(eDPF_MergeLines);


    // Setup command line arguments and parameters

    // Create command-line argument descriptions class
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // Specify USAGE context
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "netscheduled");

    arg_desc->AddFlag("reinit",       "Recreate the storage directory.");
    arg_desc->AddFlag("version-full", "Version");

    SetupArgDescriptions(arg_desc.release());
    //CONNECT_Init(&GetConfig());
    SOCK_InitializeAPI();
}


/// @internal
extern "C" void Threaded_Server_SignalHandler(int signum)
{
    if (s_netschedule_server &&
        (!s_netschedule_server->ShutdownRequested()) ) {
        s_netschedule_server->SetShutdownFlag(signum);
    }
}


int CNetScheduleDApp::Run(void)
{
    const CArgs& args = GetArgs();

    if (args["version-full"]) {
        printf(NETSCHEDULED_FULL_VERSION "\n");
        return 0;
    }

    LOG_POST(NETSCHEDULED_FULL_VERSION);

    const CNcbiRegistry& reg = GetConfig();

    try {
#if defined(NCBI_OS_UNIX)
        // attempt to get server gracefully shutdown on signal
        signal( SIGINT, Threaded_Server_SignalHandler);
        signal( SIGTERM, Threaded_Server_SignalHandler);
#endif


        // [bdb] section
        SNSDBEnvironmentParams bdb_params;

        if (!bdb_params.Read(reg, "bdb")) {
            ERR_POST("Failed to read BDB initialization section");
            return 1;
        }

        {{
            string str_params;
            unsigned nParams = bdb_params.GetNumParams();
            for (unsigned n = 0; n < nParams; ++n) {
                if (n > 0) str_params += ';';
                str_params += bdb_params.GetParamName(n);
                str_params += '=';
                str_params += bdb_params.GetParamValue(n);
            }
            LOG_POST(Info << "Effective [bdb] parameters: " << str_params);
        }}

        // [server] section
        SNS_Parameters params;
        params.Read(reg, "server");

        {{
            string str_params;
            unsigned nParams = params.GetNumParams();
            for (unsigned n = 0; n < nParams; ++n) {
                if (n > 0) str_params += ';';
                str_params += params.GetParamName(n);
                str_params += '=';
                str_params += params.GetParamValue(n);
            }
            LOG_POST(Info << "Effective [server] parameters: " << str_params);
        }}

        bool reinit = params.reinit || args["reinit"];

        m_ServerAcceptTimeout.sec = 1;
        m_ServerAcceptTimeout.usec = 0;
        params.accept_timeout  = &m_ServerAcceptTimeout;

        auto_ptr<CNetScheduleServer> server(
            new CNetScheduleServer());

        server->SetNSParameters(params);

        // Use port passed through parameters
        server->AddDefaultListener(new CNetScheduleConnectionFactory(&*server));

        server->StartListening();

        NcbiCout << "Server listening on port " << params.port << NcbiEndl;
        LOG_POST(Info << "Server listening on port " << params.port);

        // two transactions per thread should be enough
        bdb_params.max_trans = params.max_threads * 2;

        auto_ptr<CQueueDataBase> qdb(
            new CQueueDataBase(server->GetBackgroundHost()));

        NcbiCout << "Mounting database at " << bdb_params.db_path << NcbiEndl;
        LOG_POST(Info << "Mounting database at " << bdb_params.db_path);

        qdb->Open(bdb_params, reinit);

        if (params.udp_port > 0) {
            qdb->SetUdpPort(params.udp_port);
        }

#if defined(NCBI_OS_UNIX)
        if (params.is_daemon) {
            LOG_POST("Entering UNIX daemon mode...");
            bool daemon = CProcess::Daemonize(0, CProcess::fDontChroot);
            if (!daemon) {
                return 0;
            }

        }

#else
        params.is_daemon = false;
#endif

        // [queue_*], [qclass_*] and [queues] sections
        // Scan and mount queues
        unsigned min_run_timeout = qdb->Configure(reg);

        LOG_POST(Info << "Running execution control every " 
                      << min_run_timeout << " seconds");
        min_run_timeout = min_run_timeout > 0 ? min_run_timeout : 2;


        qdb->RunExecutionWatcherThread(min_run_timeout);
        qdb->RunPurgeThread();
        qdb->RunNotifThread();

        server->SetQueueDB(qdb.release());

        if (!params.is_daemon)
            NcbiCout << "Server started" << NcbiEndl;
        LOG_POST(Info << "Server started");

        server->Run();

        if (!params.is_daemon)
            NcbiCout << "Server stopped" << NcbiEndl;
        LOG_POST("Server stopped");

    }
    catch (CBDB_ErrnoException& ex)
    {
        LOG_POST(Error << "Error: DBD errno exception:" << ex.what());
        return ex.BDB_GetErrno();
    }
    catch (CBDB_LibException& ex)
    {
        LOG_POST(Error << "Error: DBD library exception:" << ex.what());
        return 1;
    }
    catch (exception& ex)
    {
        LOG_POST(Error << "Error: STD exception:" << ex.what());
        return 1;
    }

    return 0;
}


int main(int argc, const char* argv[])
{
    CDiagContext::SetOldPostFormat(false);
    CRequestContext::SetDefaultAutoIncRequestIDOnPost(true);
    // Main thread request context already created, so is not affected
    // by just set default, so set it manually.
    CDiagContext::GetRequestContext().SetAutoIncRequestIDOnPost(true);
    return CNetScheduleDApp().AppMain(argc, argv, NULL, eDS_ToStdlog);
}
