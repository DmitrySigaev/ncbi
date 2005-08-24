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
#include <corelib/plugin_manager.hpp>
#include <corelib/ncbi_system.hpp>
#include <corelib/ncbi_config.hpp>
#include <corelib/ncbimtx.hpp>

#include <util/thread_nonstop.hpp>
#include <util/bitset/ncbi_bitset.hpp>

#include <util/logrotate.hpp>

#include <connect/threaded_server.hpp>
#include <connect/ncbi_socket.hpp>
#include <connect/services/netschedule_client.hpp>
#include <connect/ncbi_conn_stream.hpp>

#include <bdb/bdb_expt.hpp>
#include <bdb/bdb_cursor.hpp>

#include "bdb_queue.hpp"
#include <db.h>

#if defined(NCBI_OS_UNIX)
# include <corelib/ncbi_os_unix.hpp>
# include <signal.h>
#endif

#include "job_time_line.hpp"
#include "access_list.hpp"


USING_NCBI_SCOPE;


#define NETSCHEDULED_VERSION \
    "NCBI NetSchedule server version=1.6.2  build " __DATE__ " " __TIME__

class CNetScheduleServer;
static CNetScheduleServer* s_netschedule_server = 0;

/// JS request types
///
/// @internal
///
typedef enum {
    eSubmitJob,
    eSubmitBatch,
    eCancelJob,
    eStatusJob,
    eGetJob,
    eWaitGetJob,
    ePutJobResult,
    ePutJobFailure,
    eReturnJob,
    eJobRunTimeout,
    eDropQueue,
    eDropJob,
    eGetProgressMsg,
    ePutProgressMsg,
    eJobExchange,

    eShutdown,
    eVersion,
    eLogging,
    eStatistics,
    eQuitSession,
    eError,
    eMonitor,
    eDumpQueue,
    eReloadConfig,
    eQList
} EJS_RequestType;

/// Request context
///
/// @internal
struct SJS_Request
{
    EJS_RequestType    req_type;
    char               input[kNetScheduleMaxDataSize];
    char               output[kNetScheduleMaxDataSize];
    char               progress_msg[kNetScheduleMaxDataSize];
    char               err[kNetScheduleMaxErrSize];
    string             job_key_str;
    unsigned int       jcount;
    unsigned int       job_id;
    unsigned int       job_return_code;
    unsigned int       port;
    unsigned int       timeout;

    string             err_msg;

    void Init()
    {
        input[0] = output[0] = progress_msg[0] = 0;
        job_key_str.erase(); err_msg.erase();
        jcount = job_id = job_return_code = port = timeout = 0;
    }
};


const unsigned kMaxMessageSize = kNetScheduleMaxErrSize * 4;

/// Thread specific data for threaded server
///
/// @internal
///
struct SThreadData
{
    char      request[kNetScheduleMaxErrSize * 4 + 1];

    string            auth;
    string            auth_prog;  // prog='...' from auth string
    CQueueClientInfo  auth_prog_info;    

    
    string      queue;
    string      answer;

    char        msg_buf[kMaxMessageSize];

    string      lmsg; /// < LOG message

    SJS_Request req;

    vector<SNS_BatchSubmitRec>  batch_subm_vec;
};

///@internal
static
void TlsCleanup(SThreadData* p_value, void* /* data */ )
{
    delete p_value;
}

/// @internal
static
CRef< CTls<SThreadData> > s_tls(new CTls<SThreadData>);



/// Log stream without creating backup names
///
/// @internal
class CNetScheduleLogStream : public CRotatingLogStream
{
public:
    typedef CRotatingLogStream TParent;
public:
    CNetScheduleLogStream(const string&    filename, 
                          CNcbiStreamoff   limit)
    : TParent(filename, limit)
    {}
protected:
    virtual string x_BackupName(string& name)
    {
        return kEmptyStr;
    }

};



/// NetScheduler threaded server
///
/// @internal
class CNetScheduleServer : public CThreadedServer
{
public:
    CNetScheduleServer(unsigned int    port,                       
                       CQueueDataBase* qdb,
                       unsigned        max_threads,
                       unsigned        init_threads,
                       unsigned        network_timeout,
                       bool            is_log); 

    virtual ~CNetScheduleServer();

    /// Take request code from the socket and process the request
    virtual void  Process(SOCK sock);
    virtual bool ShutdownRequested(void) { return m_Shutdown; }
        
    void SetShutdownFlag() { if (!m_Shutdown) m_Shutdown = true; }
    
    /// Override some parent parameters
    virtual void SetParams()
    {
        m_ThrdSrvAcceptTimeout.sec = 1;
        m_ThrdSrvAcceptTimeout.usec = 0;
    }

    void ProcessSubmit(CSocket&                sock,
                       SThreadData&            tdata,
                       CQueueDataBase::CQueue& queue);

    void ProcessSubmitBatch(CSocket&                sock,
                            SThreadData&            tdata,
                            CQueueDataBase::CQueue& queue);

    void ProcessCancel(CSocket&                sock,
                       SThreadData&            tdata,
                       CQueueDataBase::CQueue& queue);

    void ProcessStatus(CSocket&                sock,
                       SThreadData&            tdata,
                       CQueueDataBase::CQueue& queue);

    void ProcessGet(CSocket&                sock,
                    SThreadData&            tdata,
                    CQueueDataBase::CQueue& queue);

    void ProcessWaitGet(CSocket&                sock,
                        SThreadData&            tdata,
                        CQueueDataBase::CQueue& queue);

    void ProcessPut(CSocket&                sock,
                    SThreadData&            tdata,
                    CQueueDataBase::CQueue& queue);

    void ProcessJobExchange(CSocket&                sock,
                            SThreadData&            tdata,
                            CQueueDataBase::CQueue& queue);

    void ProcessMPut(CSocket&                sock,
                     SThreadData&            tdata,
                     CQueueDataBase::CQueue& queue);

    void ProcessMGet(CSocket&                sock,
                     SThreadData&            tdata,
                     CQueueDataBase::CQueue& queue);

    void ProcessPutFailure(CSocket&                sock,
                           SThreadData&            tdata,
                           CQueueDataBase::CQueue& queue);

    void ProcessDropQueue(CSocket&                sock,
                          SThreadData&            tdata,
                          CQueueDataBase::CQueue& queue);

    void ProcessReturn(CSocket&                sock,
                       SThreadData&            tdata,
                       CQueueDataBase::CQueue& queue);

    void ProcessJobRunTimeout(CSocket&                sock,
                              SThreadData&            tdata,
                              CQueueDataBase::CQueue& queue);

    void ProcessDropJob(CSocket&                sock,
                        SThreadData&            tdata,
                        CQueueDataBase::CQueue& queue);

    void ProcessStatistics(CSocket&                sock,
                           SThreadData&            tdata,
                           CQueueDataBase::CQueue& queue);

    void ProcessMonitor(CSocket&                sock,
                        SThreadData&            tdata,
                        CQueueDataBase::CQueue& queue);

    void ProcessReloadConfig(CSocket&                sock,
                             SThreadData&            tdata,
                             CQueueDataBase::CQueue& queue);


    void ProcessDump(CSocket&                sock,
                     SThreadData&            tdata,
                     CQueueDataBase::CQueue& queue);

    void ProcessShutdown(CSocket&                sock,
                         SThreadData&            tdata,
                         CQueueDataBase::CQueue& queue);


    void ProcessLog(CSocket&                sock,
                    SThreadData&            tdata);

    void SetAdminHosts(const string& host_names);

protected:
    virtual void ProcessOverflow(SOCK sock) 
    { 
        SOCK_Close(sock); 
        ERR_POST("ProcessOverflow!");
    }
    /// TRUE if logging is ON
    bool IsLog() const;

private:

    void ParseRequest(const char* reqstr, SJS_Request* req);

    void WriteMsg(CSocket&      sock, 
                  const char*   prefix, 
                  const char*   msg,
                  bool          comm_control = false);


private:
    /// Check if JOB id has the correct format
    bool x_CheckJobId(CSocket&          sock,
                       CNetSchedule_Key* job_id, 
                       const string&     job_key);

    void x_WriteBuf(CSocket& sock,
                    char*    buf,
                    size_t   bytes);

    /// Check if we have active thread data for this thread.
    /// Setup thread data if we don't.
    SThreadData* x_GetThreadData(); 

    void x_SetSocketParams(CSocket* sock);

    void x_MakeGetAnswer(const char* key_buf, 
                         SThreadData& tdata);

    void x_CreateLog();

    void x_MakeLogMessage(CSocket& sock, SThreadData& tdata);
private:
    /// Host name where server runs
    string             m_Host;
    bool               m_Shutdown; 
    /// Time to wait for the client (seconds)
    unsigned           m_InactivityTimeout;
    
    /// Accept timeout for threaded server
    STimeout           m_ThrdSrvAcceptTimeout;

    CQueueDataBase*    m_QueueDB;

    CTime              m_StartTime;

    CAtomicCounter         m_LogFlag;
    CNetScheduleLogStream  m_AccessLog;
    /// Quick local timer
    CFastLocalTime              m_LocalTimer;

    /// List of admin stations allowed to enter
    CNetSchedule_AccessList    m_AdminHosts;
//    bm::bvector<>              m_AdminHosts;

};

/// @internal
static
bool s_WaitForReadSocket(CSocket& sock, unsigned time_to_wait)
{
    STimeout to = {time_to_wait, 0};
    EIO_Status io_st = sock.Wait(eIO_Read, &to);
    switch (io_st) {    
    case eIO_Success:
    case eIO_Closed: // following read will return EOF
        return true;  
    case eIO_Timeout:
        NCBI_THROW(CNetServiceException, eTimeout, "Socket Timeout expired");
    default:
        NCBI_THROW(CNetServiceException, 
                   eCommunicationError, "Socket Wait error.");
        return false;
    }
    return false;
}        




#define NS_RETURN_ERROR(err) \
    { req->req_type = eError; req->err_msg = err; return; }
#define NS_SKIPSPACE(x)  \
    while (*x && (*x == ' ' || *x == '\t')) { ++x; }
#define NS_CHECKEND(x, msg) \
    if (!*s) { req->req_type = eError; req->err_msg = msg; return; }
#define NS_CHECKSIZE(size, max_size) \
    if (unsigned(size) >= unsigned(max_size)) \
        { req->req_type = eError; req->err_msg = "Message too long"; return; }
#define NS_GETSTRING(x, str) \
    for (;*x && !(*x == ' ' || *x == '\t'); ++x) { str.push_back(*x); }





CNetScheduleServer::CNetScheduleServer(unsigned int    port,
                                       CQueueDataBase* qdb,
                                       unsigned        max_threads,
                                       unsigned        init_threads,
                                       unsigned        network_timeout,
                                       bool            is_log)
: CThreadedServer(port),
    m_Shutdown(false),
    m_InactivityTimeout(network_timeout),
    m_StartTime(CTime::eCurrent),
    m_AccessLog("ns_access.log", 100 * 1024 * 1024)
{
    m_QueueDB = qdb;
    char hostname[256];
    int status = SOCK_gethostname(hostname, sizeof(hostname));
    if (status == 0) {
        m_Host = hostname;
    }

    m_MaxThreads = max_threads ? max_threads : 25;
    m_InitThreads = init_threads ? 
        (init_threads < m_MaxThreads ? init_threads : 2)  : 10;
    m_QueueSize = m_MaxThreads * 2;

    s_netschedule_server = this;

    m_AcceptTimeout = &m_ThrdSrvAcceptTimeout;

    if (is_log) {
        m_LogFlag.Set(1);
    } else {
        m_LogFlag.Set(0);
    }
}

CNetScheduleServer::~CNetScheduleServer()
{
    delete m_QueueDB;
}

#define JS_CHECK_IO_STATUS(x) \
        switch (x)  { \
        case eIO_Success: \
            break; \
        case eIO_Timeout: \
            NCBI_THROW(CNetServiceException, \
                       eTimeout, "Communication timeout error"); \
        default: \
            NCBI_THROW(CNetServiceException, \
              eCommunicationError, "Communication error"); \
            return; \
        }; 


void CNetScheduleServer::Process(SOCK sock)
{
    EIO_Status io_st;
    CSocket socket;
    socket.Reset(sock, eTakeOwnership, eCopyTimeoutsFromSOCK);
    bool is_log = IsLog();
    SThreadData* tdata = x_GetThreadData();

    CNetScheduleMonitor* monitor = 0;

    try {
        x_SetSocketParams(&socket);

        // Process requests

        tdata->auth.erase();
        s_WaitForReadSocket(socket, m_InactivityTimeout);

        io_st = socket.ReadLine(tdata->auth);
        JS_CHECK_IO_STATUS(io_st)
        io_st = socket.ReadLine(tdata->queue);
        JS_CHECK_IO_STATUS(io_st)

        while (1) {

            size_t n_read;
            io_st = socket.ReadLine(tdata->request,
                                    sizeof(tdata->request),
                                    &n_read);
            if (io_st == eIO_Closed || io_st == eIO_Timeout) {
                return;
            }
            JS_CHECK_IO_STATUS(io_st)


            // Logging
            //

            if (is_log) {
                x_MakeLogMessage(socket, *tdata);
                m_AccessLog << tdata->lmsg;
            }

            unsigned peer_ha;
            socket.GetPeerAddress(&peer_ha, 0, eNH_NetworkByteOrder);

            CQueueDataBase::CQueue queue(*m_QueueDB, tdata->queue, peer_ha);

            monitor = queue.GetMonitor();
            if (monitor) {
                if (!monitor->IsMonitorActive()) {
                    monitor = 0;
                } else {
                    if (!is_log) {
                        x_MakeLogMessage(socket, *tdata);
                    }
                    monitor->SendString(tdata->lmsg);
                }
            }

            SJS_Request& req = tdata->req;
            req.Init();
            ParseRequest(tdata->request, &req);

            if (req.req_type == eQuitSession) {
                break;
            }



            // program version control:
            //
            // we want status request to be fast, skip version control 
            //
            if ((req.req_type != eStatusJob) && 
                    queue.IsVersionControl()) {

                // bypass for admin tools
                if (tdata->auth == "netschedule_control" ||
                    tdata->auth == "netschedule_admin") 
                {
                    goto end_version_control;
                }
                
                string::size_type pos; 
                pos = tdata->auth.find("prog='");
                if (pos == string::npos) {
                client_ver_err:
                    WriteMsg(socket, "ERR:", "CLIENT_VERSION");
                    return;
                } else {
                    string& prog = tdata->auth_prog;
                    prog.erase();
                    const char* prog_str = tdata->auth.c_str();
                    prog_str += pos + 6;
                    for(; *prog_str && *prog_str != '\''; ++prog_str) {
                        char ch = *prog_str;
                        prog.push_back(ch);
                    }
                    if (prog.empty()) {
                        goto client_ver_err;
                    }
                    try {
                        CQueueClientInfo& prog_info = tdata->auth_prog_info;
                        ParseVersionString(prog,
                                           &prog_info.client_name, 
                                           &prog_info.version_info);
                        bool match = queue.IsMatchingClient(prog_info);
                        if (!match)
                            goto client_ver_err;
                    }
                    catch (exception&)
                    {
                        goto client_ver_err;
                    }
                }
            }

end_version_control:

            switch (req.req_type) {
            case eSubmitJob:
                if (!queue.IsSubmitAllowed()) {
                    WriteMsg(socket, "ERR:", "OPERATION_ACCESS_DENIED");
                    break;
                }
                ProcessSubmit(socket, *tdata, queue);
                break;
            case eSubmitBatch:
                if (!queue.IsSubmitAllowed()) {
                    WriteMsg(socket, "ERR:", "OPERATION_ACCESS_DENIED");
                    break;
                }
                ProcessSubmitBatch(socket, *tdata, queue);
                break;
            case eCancelJob:
                if (!queue.IsSubmitAllowed()) {
                    WriteMsg(socket, "ERR:", "OPERATION_ACCESS_DENIED");
                    break;
                }
                ProcessCancel(socket, *tdata, queue);
                break;
            case eStatusJob:
                ProcessStatus(socket, *tdata, queue);
                break;
            case eGetJob:
                if (!queue.IsWorkerAllowed()) {
                    WriteMsg(socket, "ERR:", "OPERATION_ACCESS_DENIED");
                    break;
                }
                ProcessGet(socket, *tdata, queue);
                break;
            case eJobExchange:
                if (!queue.IsWorkerAllowed()) {
                    WriteMsg(socket, "ERR:", "OPERATION_ACCESS_DENIED");
                    break;
                }
                ProcessJobExchange(socket, *tdata, queue);
                break;
            case eWaitGetJob:
                if (!queue.IsWorkerAllowed()) {
                    WriteMsg(socket, "ERR:", "OPERATION_ACCESS_DENIED");
                    break;
                }
                ProcessWaitGet(socket, *tdata, queue);
                break;
            case ePutJobResult:
                ProcessPut(socket, *tdata, queue);
                break;
            case ePutJobFailure:
                ProcessPutFailure(socket, *tdata, queue);
                break;
            case eReturnJob:
                ProcessReturn(socket, *tdata, queue);
                break;
            case eJobRunTimeout:
                ProcessJobRunTimeout(socket, *tdata, queue);
                break;
            case eDropJob:
                if (!queue.IsSubmitAllowed()) {
                    WriteMsg(socket, "ERR:", "OPERATION_ACCESS_DENIED");
                    break;
                }
                ProcessDropJob(socket, *tdata, queue);
                break;
            case eGetProgressMsg:
                ProcessMGet(socket, *tdata, queue);
                break;
            case ePutProgressMsg:
                ProcessMPut(socket, *tdata, queue);
                break;
            case eShutdown:
                ProcessShutdown(socket, *tdata, queue);
                break;
            case eVersion:
                WriteMsg(socket, "OK:", NETSCHEDULED_VERSION);
                break;
            case eDropQueue:
                ProcessDropQueue(socket, *tdata, queue);
                break;
            case eLogging:
                ProcessLog(socket, *tdata);
                break;
            case eMonitor:
                ProcessMonitor(socket, *tdata, queue);
                break;
            case eDumpQueue:
                ProcessDump(socket, *tdata, queue);
                break;
            case eStatistics:
                ProcessStatistics(socket, *tdata, queue);
                break;
            case eReloadConfig:
                ProcessReloadConfig(socket, *tdata, queue);
                break;
            case eQList:
                {
                string qnames;
                ITERATE(CQueueCollection::TQueueMap, it, 
                        m_QueueDB->GetQueueCollection().GetMap()) {
                    qnames += it->first; qnames += ";";
                }
                WriteMsg(socket, "OK:", qnames.c_str());
                }
                break;
            case eError:
                WriteMsg(socket, "ERR:", tdata->req.err_msg.c_str());
                break;
            default:
                _ASSERT(0);
            } // switch
        } // while

        // read trailing input (see netcached.cpp for more comments on "why?")
        STimeout to; to.sec = to.usec = 0;
        socket.SetTimeout(eIO_Read, &to);
        socket.Read(NULL, 1024);
    } 
    catch (CNetScheduleException &ex)
    {
        if (ex.GetErrCode() == CNetScheduleException::eUnknownQueue) {
            WriteMsg(socket, "ERR:", "QUEUE_NOT_FOUND", false /*no control*/);
        } else {
            string err = NStr::PrintableString(ex.what());
            WriteMsg(socket, "ERR:", err.c_str(), false /*no control*/);
        }
        string msg = "Server error: ";
        msg += ex.what();
        msg += " Peer=";
        msg += socket.GetPeerAddress();
        msg += " ";
        msg += tdata->auth;

        ERR_POST(msg);

        msg = m_LocalTimer.GetLocalTime().AsString() + msg;
        if (monitor) {
            monitor->SendString(msg);
        }
    }
    catch (CNetServiceException &ex)
    {
        WriteMsg(socket, "ERR:", ex.what(), false /*no control*/);
        string msg = "Server error: ";
        msg += ex.what();
        msg += " Peer=";
        msg += socket.GetPeerAddress();
        msg += " ";
        msg += tdata->auth;

        ERR_POST(msg);

        msg = m_LocalTimer.GetLocalTime().AsString() + msg;
        if (monitor) {
            monitor->SendString(msg);
        }

    }
    catch (CBDB_ErrnoException& ex)
    {
        int err_no = ex.BDB_GetErrno();
        if (err_no == DB_RUNRECOVERY) {
            string msg = "Fatal Berkeley DB error: DB_RUNRECOVERY. ";
            msg += "Emergency shutdown initiated!";
            msg += ex.what();
            msg += " Peer=";
            msg += socket.GetPeerAddress();
            msg += " ";
            msg += tdata->auth;

            ERR_POST(msg);

            msg = m_LocalTimer.GetLocalTime().AsString() + msg;
            if (monitor) {
                monitor->SendString(msg);
            }

            SetShutdownFlag();
        } else {
            string msg = "BDB error:";
            msg += ex.what();
            msg += " Peer=";
            msg += socket.GetPeerAddress();
            msg += " ";
            msg += tdata->auth;

            ERR_POST(msg);

            msg = m_LocalTimer.GetLocalTime().AsString() + msg;
            if (monitor) {
                monitor->SendString(msg);
            }

            string err = "Internal database error:";
            err += ex.what();
            err = NStr::PrintableString(err);
            WriteMsg(socket, "ERR:", err.c_str(), false /*no control*/);
        }
    }
    catch (CBDB_Exception &ex)
    {
        string err = "Internal database (BDB) error:";
        err += ex.what();
        err = NStr::PrintableString(err);
        WriteMsg(socket, "ERR:", err.c_str(), false /*no control*/);

        string msg = "BDB error:";
        msg += ex.what();
        msg += " Peer=";
        msg += socket.GetPeerAddress();
        msg += " ";
        msg += tdata->auth;

        ERR_POST(msg);

        msg = m_LocalTimer.GetLocalTime().AsString() + msg;
        if (monitor) {
            monitor->SendString(msg);
        }
    }
    catch (exception& ex)
    {
        string msg = "Command execution error ";
        if (tdata) msg += tdata->request;
        msg += " ";
        msg += ex.what();
        string err = NStr::PrintableString(ex.what());
        WriteMsg(socket, "ERR:", err.c_str(), false /*no control*/);

        msg = "STL exception:";
        msg += ex.what();
        msg += " Peer=";
        msg += socket.GetPeerAddress();
        msg += " ";
        msg += tdata->auth;

        ERR_POST(msg);

        msg = m_LocalTimer.GetLocalTime().AsString() + msg;
        if (monitor) {
            monitor->SendString(msg);
        }
    }
}

void CNetScheduleServer::ProcessSubmit(CSocket&                sock, 
                                       SThreadData&            tdata,
                                       CQueueDataBase::CQueue& queue)
{
    SJS_Request& req = tdata.req;
    unsigned client_address = 0;
    sock.GetPeerAddress(&client_address, 0, eNH_NetworkByteOrder);

    unsigned job_id =
        queue.Submit(req.input, 
                     client_address, 
                     req.port, req.timeout, 
                     req.progress_msg);

    char buf[1024];
    sprintf(buf, NETSCHEDULE_JOBMASK, 
                 job_id, m_Host.c_str(), unsigned(GetPort()));

    WriteMsg(sock, "OK:", buf);

    CNetScheduleMonitor* monitor = queue.GetMonitor();
    if (monitor->IsMonitorActive()) {
        string msg = "::ProcessSubmit ";
        msg += m_LocalTimer.GetLocalTime().AsString();
        msg += buf;
        msg += " ==> ";
        msg += sock.GetPeerAddress();
        msg += " ";
        msg += tdata.auth;

        monitor->SendString(msg);
    }

}

void CNetScheduleServer::ProcessSubmitBatch(CSocket&                sock,
                                            SThreadData&            tdata,
                                            CQueueDataBase::CQueue& queue)
{
    char    buf[kNetScheduleMaxDataSize * 4 + 1];
    size_t  n_read;

    WriteMsg(sock, "OK:", "Batch submit ready");

    unsigned client_address = 0;
    sock.GetPeerAddress(&client_address, 0, eNH_NetworkByteOrder);

    s_WaitForReadSocket(sock, m_InactivityTimeout);
    //SJS_Request& req = tdata.req;
    EIO_Status io_st;

    while (1) {
        unsigned batch_size = 0;

        // BTCH batch_size
        //
        {{
            io_st = sock.ReadLine(buf, sizeof(buf), &n_read);
            JS_CHECK_IO_STATUS(io_st)

            const char* s = buf;
            if (strncmp(s, "BTCH", 4) != 0) {
                if (strncmp(s, "ENDS", 4) == 0) {
                    break;
                }
                WriteMsg(sock, "ERR:", 
                        "Batch submit error: BATCH expected");
                return;
            }
            s+=5;
            NS_SKIPSPACE(s)

            batch_size = atoi(s);
        }}

        // read the batch body
        //

        CStopWatch sw1(true);

        tdata.batch_subm_vec.resize(batch_size);

        for (unsigned i = 0; i < batch_size; ++i) {
            s_WaitForReadSocket(sock, m_InactivityTimeout);
            
      

            io_st = sock.ReadLine(buf, sizeof(buf)-1, &n_read);
            JS_CHECK_IO_STATUS(io_st)

            if (*buf == 0) {  // something is wrong
                WriteMsg(sock, "ERR:", 
                        "Batch submit error: empty string");
                return;
            }

            const char* s = buf;
            if (*s !='"') {
                WriteMsg(sock, "ERR:", 
                        "Invalid batch submission, syntax error");
                return;
            }

            SNS_BatchSubmitRec& rec = tdata.batch_subm_vec[i];
            char *ptr = rec.input;
            for (++s; *s != '"'; ++s) {
                if (*s == 0) {
                    WriteMsg(sock, "ERR:",
                             "Batch submit error: unexpected end of string");
                    return;
                }
                *ptr++ = *s;
            } // for
            
            *ptr = 0;
        } // for batch_size

        s_WaitForReadSocket(sock, m_InactivityTimeout);

        io_st = sock.ReadLine(buf, sizeof(buf), &n_read);
        JS_CHECK_IO_STATUS(io_st)


        const char* s = buf;
        if (strncmp(s, "ENDB", 4) != 0) {
            WriteMsg(sock, "ERR:",
                        "Batch submit error: unexpected end of batch");
        }

        double comm_elapsed = sw1.Elapsed();

        // we have our batch now

        CStopWatch sw2(true);

        unsigned job_id =
            queue.SubmitBatch(tdata.batch_subm_vec, 
                              client_address, 
                              0, 0, 0);

        double db_elapsed = sw2.Elapsed();

        CNetScheduleMonitor* monitor = queue.GetMonitor();
        if (monitor->IsMonitorActive()) {
            string msg = "::ProcessSubmitBatch ";
            msg += m_LocalTimer.GetLocalTime().AsString();
            msg += buf;
            msg += " ==> ";
            msg += sock.GetPeerAddress();
            msg += " ";
            msg += tdata.auth;
            msg += " batch block size=";
            msg += NStr::UIntToString(batch_size);
            msg += " comm.time=";
            msg += NStr::DoubleToString(comm_elapsed, 4, NStr::fDoubleFixed);
            msg += " trans.time=";
            msg += NStr::DoubleToString(db_elapsed, 4, NStr::fDoubleFixed);

            monitor->SendString(msg);
        }

        {{
        char buf[1024];
        sprintf(buf, "%u %s %u", 
                    job_id, m_Host.c_str(), unsigned(GetPort()));

        WriteMsg(sock, "OK:", buf);
        }}

    } // while

    m_QueueDB->TransactionCheckPoint();
}

void CNetScheduleServer::ProcessCancel(CSocket&                sock, 
                                       SThreadData&            tdata,
                                       CQueueDataBase::CQueue& queue)
{
    SJS_Request& req = tdata.req;

    unsigned job_id = CNetSchedule_GetJobId(req.job_key_str);
    queue.Cancel(job_id);
    WriteMsg(sock, "OK:", "");
}

void CNetScheduleServer::ProcessDropJob(CSocket&                sock, 
                                        SThreadData&            tdata,
                                        CQueueDataBase::CQueue& queue)
{
    SJS_Request& req = tdata.req;

    unsigned job_id = CNetSchedule_GetJobId(req.job_key_str);
    queue.DropJob(job_id);
    WriteMsg(sock, "OK:", "");
}


void CNetScheduleServer::ProcessJobRunTimeout(CSocket&                sock, 
                                              SThreadData&            tdata,
                                              CQueueDataBase::CQueue& queue)
{
    SJS_Request& req = tdata.req;

    unsigned job_id = CNetSchedule_GetJobId(req.job_key_str);
    queue.SetJobRunTimeout(job_id, req.timeout);
    WriteMsg(sock, "OK:", "");
}

void CNetScheduleServer::ProcessStatus(CSocket&                sock, 
                                       SThreadData&            tdata,
                                       CQueueDataBase::CQueue& queue)
{
    SJS_Request& req = tdata.req;

    unsigned job_id = CNetSchedule_GetJobId(req.job_key_str);

    CNetScheduleClient::EJobStatus status = queue.GetStatus(job_id);
    char szBuf[kNetScheduleMaxDataSize * 2];
    int st = (int) status;

    if (status == CNetScheduleClient::eDone) {
        int ret_code;
        bool b = queue.GetJobDescr(job_id, &ret_code, 
                             tdata.req.input,
                             tdata.req.output,
                             0, 0);

        if (b) {
            sprintf(szBuf, 
                    "%i %i \"%s\" \"\" \"%s\"", 
                    st, ret_code, 
                    tdata.req.output,
                    tdata.req.input);
            WriteMsg(sock, "OK:", szBuf);
            return;
        } else {
            st = (int)CNetScheduleClient::eJobNotFound;
        }
    } 
    else if (status == CNetScheduleClient::eFailed) {
        bool b = queue.GetJobDescr(job_id, 0, 
                             tdata.req.input,
                             0,
                             tdata.req.err,
                             0);

        if (b) {
            sprintf(szBuf, 
                    "%i %i \"\" \"%s\" \"%s\"", 
                    st, 0, 
                    tdata.req.err,
                    tdata.req.input);
            WriteMsg(sock, "OK:", szBuf);
            return;
        } else {
            st = (int)CNetScheduleClient::eJobNotFound;
        }
    }

    sprintf(szBuf, "%i", st);
    WriteMsg(sock, "OK:", szBuf);
}

void CNetScheduleServer::ProcessMGet(CSocket&                sock, 
                                     SThreadData&            tdata,
                                     CQueueDataBase::CQueue& queue)
{
    SJS_Request& req = tdata.req;

    unsigned job_id = CNetSchedule_GetJobId(req.job_key_str);
    int ret_code;
    bool b = queue.GetJobDescr(job_id, &ret_code, 
                               0, 0, 0, tdata.req.progress_msg);

    if (b) {
        WriteMsg(sock, "OK:", tdata.req.progress_msg);
    } else {
        WriteMsg(sock, "ERR:", "Job not found");
    }
}

void CNetScheduleServer::ProcessMPut(CSocket&                sock, 
                                     SThreadData&            tdata,
                                     CQueueDataBase::CQueue& queue)
{
    SJS_Request& req = tdata.req;

    unsigned job_id = CNetSchedule_GetJobId(req.job_key_str);

    queue.PutProgressMessage(job_id, tdata.req.progress_msg);
    WriteMsg(sock, "OK:", "");
}




void CNetScheduleServer::x_MakeGetAnswer(const char*   key_buf,
                                         SThreadData&  tdata)
{
    tdata.answer = key_buf;
    tdata.answer.append(" \"");
    tdata.answer.append(tdata.req.input);
    tdata.answer.append("\"");
}


void CNetScheduleServer::ProcessGet(CSocket&                sock, 
                                    SThreadData&            tdata,
                                    CQueueDataBase::CQueue& queue)
{
    SJS_Request& req = tdata.req;
    unsigned job_id;
    unsigned client_address;
    sock.GetPeerAddress(&client_address, 0, eNH_NetworkByteOrder);

    char key_buf[1024];
    queue.GetJob(key_buf,
                 client_address, &job_id, req.input, m_Host, GetPort());

    if (job_id) {
        x_MakeGetAnswer(key_buf, tdata);
        WriteMsg(sock, "OK:", tdata.answer.c_str());
    } else {
        WriteMsg(sock, "OK:", "");
    }

    CNetScheduleMonitor* monitor = queue.GetMonitor();
    if (monitor->IsMonitorActive() && job_id) {
        string msg = "::ProcessGet ";
        msg += m_LocalTimer.GetLocalTime().AsString();
        msg += tdata.answer;
        msg += " ==> ";
        msg += sock.GetPeerAddress();
        msg += " ";
        msg += tdata.auth;

        monitor->SendString(msg);
    }

    if (req.port) {  // unregister notification
        sock.GetPeerAddress(&client_address, 0, eNH_NetworkByteOrder);
        queue.RegisterNotificationListener(
            client_address, req.port, 0, tdata.auth);
   }
}

void 
CNetScheduleServer::ProcessJobExchange(CSocket&                sock,
                                       SThreadData&            tdata,
                                       CQueueDataBase::CQueue& queue)
{
    SJS_Request& req = tdata.req;
    unsigned job_id;
    unsigned client_address;
    sock.GetPeerAddress(&client_address, 0, eNH_NetworkByteOrder);

    char key_buf[1024];

    unsigned done_job_id;
    if (!req.job_key_str.empty()) {
        done_job_id = CNetSchedule_GetJobId(req.job_key_str);
    } else {
        done_job_id = 0;
    }
    queue.PutResultGetJob(done_job_id, req.job_return_code, req.output,
                          key_buf, client_address, &job_id,
                          req.input, m_Host, GetPort());

    if (job_id) {
        x_MakeGetAnswer(key_buf, tdata);
        WriteMsg(sock, "OK:", tdata.answer.c_str());
    } else {
        WriteMsg(sock, "OK:", "");
    }

    CNetScheduleMonitor* monitor = queue.GetMonitor();
    if (monitor->IsMonitorActive() && job_id) {
        string msg = "::ProcessJobExchange ";
        msg += m_LocalTimer.GetLocalTime().AsString();
        msg += tdata.answer;
        msg += " ==> ";
        msg += sock.GetPeerAddress();
        msg += " ";
        msg += tdata.auth;

        monitor->SendString(msg);
    }
}

void CNetScheduleServer::ProcessWaitGet(CSocket&                sock,
                                        SThreadData&            tdata,
                                        CQueueDataBase::CQueue& queue)
{
    SJS_Request& req = tdata.req;
    unsigned job_id;
    unsigned client_address;
    sock.GetPeerAddress(&client_address, 0, eNH_NetworkByteOrder);

    char key_buf[1024];
    queue.GetJob(key_buf,
                 client_address, &job_id, req.input, m_Host, GetPort());
    if (job_id) {
        x_MakeGetAnswer(key_buf, tdata);
        WriteMsg(sock, "OK:", tdata.answer.c_str());
        return;
    }

    // job not found, initiate waiting mode

    WriteMsg(sock, "OK:", kEmptyStr.c_str());

    sock.GetPeerAddress(&client_address, 0, eNH_NetworkByteOrder);
    queue.RegisterNotificationListener(
        client_address, req.port, req.timeout, tdata.auth);

}

void CNetScheduleServer::ProcessPutFailure(CSocket&                sock, 
                                           SThreadData&            tdata,
                                           CQueueDataBase::CQueue& queue)
{
    SJS_Request& req = tdata.req;

    unsigned job_id = CNetSchedule_GetJobId(req.job_key_str);
    queue.JobFailed(job_id, req.err_msg);
    WriteMsg(sock, "OK:", kEmptyStr.c_str());
}

void CNetScheduleServer::ProcessPut(CSocket&                sock, 
                                    SThreadData&            tdata,
                                    CQueueDataBase::CQueue& queue)
{
    SJS_Request& req = tdata.req;

    unsigned job_id = CNetSchedule_GetJobId(req.job_key_str);
    queue.PutResult(job_id, req.job_return_code, req.output);
    WriteMsg(sock, "OK:", kEmptyStr.c_str());
}


void CNetScheduleServer::ProcessReturn(CSocket&                sock, 
                                       SThreadData&            tdata,
                                       CQueueDataBase::CQueue& queue)
{
    SJS_Request& req = tdata.req;

    unsigned job_id = CNetSchedule_GetJobId(req.job_key_str);
    queue.ReturnJob(job_id);
    WriteMsg(sock, "OK:", kEmptyStr.c_str());
}


void CNetScheduleServer::ProcessDropQueue(CSocket&                sock, 
                                          SThreadData&            tdata,
                                          CQueueDataBase::CQueue& queue)
{
    queue.Truncate();
    WriteMsg(sock, "OK:", kEmptyStr.c_str());
}

void CNetScheduleServer::ProcessMonitor(CSocket&                sock, 
                                        SThreadData&            tdata,
                                        CQueueDataBase::CQueue& queue)
{
    sock.DisableOSSendDelay(false);
    WriteMsg(sock, "OK:", NETSCHEDULED_VERSION);
    string started = "Started: " + m_StartTime.AsString();
    WriteMsg(sock, "OK:", started.c_str());
    sock.SetOwnership(eNoOwnership);
    queue.SetMonitorSocket(sock.GetSOCK());
}


void CNetScheduleServer::ProcessDump(CSocket&                sock, 
                                     SThreadData&            tdata,
                                     CQueueDataBase::CQueue& queue)
{
    WriteMsg(sock, "OK:", NETSCHEDULED_VERSION);
    SJS_Request& req = tdata.req;

    if (req.job_key_str.empty()) {
        WriteMsg(sock, "OK:", "[Job status matrix]:");

        SOCK sk = sock.GetSOCK();
        sock.SetOwnership(eNoOwnership);
        sock.Reset(0, eTakeOwnership, eCopyTimeoutsToSOCK);

        CConn_SocketStream ios(sk);  // sock is being passed and used exclusively

        queue.PrintJobStatusMatrix(ios);

        ios << "OK:[Job DB]:\n";
        queue.PrintAllJobDbStat(ios);
        ios << "OK:END";
    } else {
        unsigned job_id = CNetSchedule_GetJobId(req.job_key_str);

        CNetScheduleClient::EJobStatus status = queue.GetStatus(job_id);
        
        string st_str = CNetScheduleClient::StatusToString(status);
        st_str = "[Job status matrix]:" + st_str;
        WriteMsg(sock, "OK:", st_str.c_str());

        SOCK sk = sock.GetSOCK();
        sock.SetOwnership(eNoOwnership);
        sock.Reset(0, eTakeOwnership, eCopyTimeoutsToSOCK);

        CConn_SocketStream ios(sk);

        ios << "OK:[Job DB]:\n";
        queue.PrintJobDbStat(job_id, ios);
        ios << "OK:END";
    }
}

void CNetScheduleServer::ProcessReloadConfig(CSocket&                sock, 
                                             SThreadData&            tdata,
                                             CQueueDataBase::CQueue& queue)
{
    CNcbiApplication* app = CNcbiApplication::Instance();

    bool reloaded = app->ReloadConfig(CMetaRegistry::fReloadIfChanged);
    if (reloaded) {
        unsigned tmp;
        m_QueueDB->ReadConfig(app->GetConfig(), &tmp);
    }
    WriteMsg(sock, "OK:", "");
}



void CNetScheduleServer::ProcessStatistics(CSocket&                sock, 
                                           SThreadData&            tdata,
                                           CQueueDataBase::CQueue& queue)
{
    sock.DisableOSSendDelay(false);

    WriteMsg(sock, "OK:", NETSCHEDULED_VERSION);
    string started = "Started: " + m_StartTime.AsString();
    WriteMsg(sock, "OK:", started.c_str());

    for (int i = CNetScheduleClient::ePending; 
         i < CNetScheduleClient::eLastStatus; ++i) {
             CNetScheduleClient::EJobStatus st = 
                                (CNetScheduleClient::EJobStatus) i;
             unsigned count = queue.CountStatus(st);
             string st_name = CNetScheduleClient::StatusToString(st);
             st_name += ": ";
             st_name += NStr::UIntToString(count);
             WriteMsg(sock, "OK:", st_name.c_str());
    } // for

    unsigned db_recs = queue.CountRecs();
    string recs = "Records:";
    recs += NStr::UIntToString(db_recs);
    WriteMsg(sock, "OK:", recs.c_str());
    WriteMsg(sock, "OK:", "[Database statistics]:");

    {{
        CNcbiOstrstream ostr;
        queue.PrintStat(ostr);
        ostr << ends;

        char* stat_str = ostr.str();
//        size_t os_str_len = ostr.pcount()-1;
//        stat_str[os_str_len] = 0;
        try {
            WriteMsg(sock, "OK:", stat_str);
        } catch (...) {
            ostr.freeze(false);
            throw;
        }
        ostr.freeze(false);
    }}

    WriteMsg(sock, "OK:", "[Worker node statistics]:");

    {{
        CNcbiOstrstream ostr;
        queue.PrintNodeStat(ostr);
        ostr << ends;

        char* stat_str = ostr.str();
//        size_t os_str_len = ostr.pcount()-1;
//        stat_str[os_str_len] = 0;
        try {
            WriteMsg(sock, "OK:", stat_str);
        } catch (...) {
            ostr.freeze(false);
            throw;
        }
        ostr.freeze(false);
    }}

    {{
        WriteMsg(sock, "OK:", "[Configured job submitters]:");
        CNcbiOstrstream ostr;
        queue.PrintSubmHosts(ostr);
        ostr << ends;

        char* stat_str = ostr.str();
        try {
            WriteMsg(sock, "OK:", stat_str);
        } catch (...) {
            ostr.freeze(false);
            throw;
        }
        ostr.freeze(false);
    }}

    {{
        WriteMsg(sock, "OK:", "[Configured workers]:");
        CNcbiOstrstream ostr;
        queue.PrintWNodeHosts(ostr);
        ostr << ends;

        char* stat_str = ostr.str();
        try {
            WriteMsg(sock, "OK:", stat_str);
        } catch (...) {
            ostr.freeze(false);
            throw;
        }
        ostr.freeze(false);
    }}

/*
    {{
        SOCK sk = sock.GetSOCK();
        sock.SetOwnership(eNoOwnership);
        sock.Reset(0, eTakeOwnership, eCopyTimeoutsToSOCK);

        CConn_SocketStream ios(sk);

        WriteMsg(sock, "OK:", "[Configured job submitters]:");
        queue.PrintSubmHosts(ios);

        WriteMsg(sock, "OK:", "[Configured workers]:");
        queue.PrintWNodeHosts(ios);
    }}
*/

    WriteMsg(sock, "OK:", "END");
}

void CNetScheduleServer::ProcessLog(CSocket&                sock, 
                                    SThreadData&            tdata)
{
    const char* str = tdata.req.job_key_str.c_str();
    if (NStr::strcasecmp(str, "ON")==0) {
        bool is_log = IsLog();
        m_LogFlag.Set(1);
        if (!is_log) {
            m_LogFlag.Set(1);
            m_AccessLog.Rotate();
        }
        LOG_POST("Logging turned ON");
    } else {
        if (NStr::strcasecmp(str, "OFF")==0) {
            m_LogFlag.Set(0);
            LOG_POST("Logging turned OFF");
        } else {
            WriteMsg(sock, "ERR:", "Incorrect LOG syntax");
        }
    }
    WriteMsg(sock, "OK:", "");
}

void CNetScheduleServer::ProcessShutdown(CSocket&                sock,
                                         SThreadData&            tdata,
                                         CQueueDataBase::CQueue& queue)
{
    string admin_host = sock.GetPeerAddress();
    size_t pos = admin_host.find_first_of(':');
    if (pos != string::npos) {
        admin_host = admin_host.substr(0, pos);
    }
    unsigned ha;
    if (!m_AdminHosts.IsRestrictionSet()) { // no control
        goto process_shutdown;
    }
    ha = CSocketAPI::gethostbyname(admin_host);

    if (m_AdminHosts.IsAllowed(ha)) {
    process_shutdown:
        string msg = "Shutdown request... ";
        msg += admin_host;
        msg += " ";
        msg += CTime(CTime::eCurrent).AsString();
        LOG_POST(Info << msg);
        SetShutdownFlag();
    } else {
        WriteMsg(sock, "ERR:", "Shutdown access denied.");
        LOG_POST(Warning << "Shutdown request denied: " << admin_host);
    }
}

void CNetScheduleServer::x_WriteBuf(CSocket& sock,
                                    char*    buf,
                                    size_t   bytes)
{
    do {
        size_t n_written;
        EIO_Status io_st;
        io_st = sock.Write(buf, bytes, &n_written);
        switch (io_st) {
        case eIO_Success:
            break;
        case eIO_Timeout:
            NCBI_THROW(CNetServiceException, 
                       eTimeout, "Communication timeout error");
            break;
        default:
            NCBI_THROW(CNetServiceException, 
              eCommunicationError, "Communication error (cannot send data)");
            break;
        } // switch
        bytes -= n_written;
        buf += n_written;
    } while (bytes > 0);
}







void CNetScheduleServer::ParseRequest(const char* reqstr, SJS_Request* req)
{
    // Request formats and types:
    //
    // 1. SUBMIT "NCID_01_1..." ["Progress msg"] [udp_port notif_wait] 
    // 2. CANCEL JSID_01_1
    // 3. STATUS JSID_01_1
    // 4. GET udp_port
    // 5. PUT JSID_01_1 EndStatus "NCID_01_2..."
    // 6. RETURN JSID_01_1
    // 7. SHUTDOWN
    // 8. VERSION
    // 9. LOG [ON/OFF]
    // 10.STAT
    // 11.QUIT 
    // 12.DROPQ
    // 13.WGET udp_port_number timeout
    // 14.JRTO JSID_01_1 timeout
    // 15.DROJ JSID_01_1
    // 16.FPUT JSID_01_1 "error message"
    // 17.MPUT JSID_01_1 "error message"
    // 18.MGET JSID_01_1
    // 19.MONI
    // 20.DUMP [JSID_01_1]
    // 21.RECO
    // 22.QLST
    // 23.BSUB
    // 24.JXCG [JSID_01_1 EndStatus "NCID_01_2..."]

    const char* s = reqstr;

    switch (*s) {

    case 'S':
        if (strncmp(s, "STATUS", 6) == 0) {
            req->req_type = eStatusJob;
            s += 6;
            NS_SKIPSPACE(s)
            NS_GETSTRING(s, req->job_key_str)
            
            if (req->job_key_str.empty()) {
                NS_RETURN_ERROR("Misformed STATUS request")
            }
            return;
        }


        if (strncmp(s, "SUBMIT", 6) == 0) {
            req->req_type = eSubmitJob;
            s += 6;
            NS_SKIPSPACE(s)
            if (*s !='"') {
                NS_RETURN_ERROR("Misformed SUBMIT request")
            }
            char *ptr = req->input;
            for (++s; *s != '"'; ++s) {
                NS_CHECKEND(s, "Misformed SUBMIT request")
                NS_CHECKSIZE(ptr-req->input, kNetScheduleMaxDataSize);
                *ptr++ = *s;
            }
            ++s;
            *ptr = 0;

            NS_SKIPSPACE(s)

            // optional progress message

            if (*s == '"') {
                ptr = req->progress_msg;

                for (++s; *s != '"'; ++s) {
                    NS_CHECKEND(s, "Misformed SUBMIT request")
                    NS_CHECKSIZE(ptr-req->progress_msg, 
                                 kNetScheduleMaxDataSize);
                    *ptr++ = *s;
                }
                ++s;
                *ptr = 0;

                NS_SKIPSPACE(s)
            }

            // optional UDP notification parameters

            if (!*s) {
                return;
            }

            int port = atoi(s);
            if (port > 0) {
                req->port = (unsigned)port;
            }

            for (; *s && isdigit((unsigned char)(*s)); ++s) {}

            NS_SKIPSPACE(s)

            if (!*s) {
                return;
            }

            int timeout = atoi(s);
            if (timeout > 0) {
                req->timeout = timeout;
            }

            return;
        }

        if (strncmp(s, "STAT", 4) == 0) {
            req->req_type = eStatistics;
            return;
        }

        if (strncmp(s, "SHUTDOWN", 8) == 0) {
            req->req_type = eShutdown;
            return;
        }

        break;

    case 'G':
        if (strncmp(s, "GET", 3) == 0) {
            req->req_type = eGetJob;
            
            s += 3;
            NS_SKIPSPACE(s)

            if (*s) {
                int port = atoi(s);
                if (port > 0) {
                    req->port = (unsigned)port;
                }
            }
            return;
        }

        break;
    case 'M':
        if (strncmp(s, "MPUT", 4) == 0) {
            req->req_type = ePutProgressMsg;
            s += 4;

            NS_SKIPSPACE(s)
            NS_GETSTRING(s, req->job_key_str)
            NS_SKIPSPACE(s)

            if (*s !='"') {
                NS_RETURN_ERROR("Misformed MPUT request")
            }
            char *ptr = req->progress_msg;
            for (++s; *s; ++s) {
                if (*s == '"' && *(s-1) != '\\') break;
                NS_CHECKEND(s, "Misformed MPUT request")
                NS_CHECKSIZE(ptr-req->progress_msg, kNetScheduleMaxDataSize);
                *ptr++ = *s;            
            }
            *ptr = 0;

            return;
        }
        if (strncmp(s, "MGET", 4) == 0) {
            req->req_type = eGetProgressMsg;
            s += 4;

            NS_SKIPSPACE(s)
            if (*s != 0) {
                NS_GETSTRING(s, req->job_key_str)
            }
            return;
        }
        if (strncmp(s, "MONI", 4) == 0) {
            req->req_type = eMonitor;
            return;
        }

        break;

    case 'J':
        if (strncmp(s, "JXCG", 4) == 0) {
            req->req_type = eJobExchange;
            s += 4;
            NS_SKIPSPACE(s)
            if (*s == 0) {
                req->job_key_str.erase();
                return;
            }
            goto parse_put_params;
        }
        if (strncmp(s, "JRTO", 4) == 0) {
            req->req_type = eJobRunTimeout;
            
            s += 4;
            NS_SKIPSPACE(s)

            NS_CHECKEND(s, "Misformed job run timeout request")

            NS_GETSTRING(s, req->job_key_str)
            
            if (req->job_key_str.empty()) {
                NS_RETURN_ERROR("Misformed job run timeout request")
            }
            NS_SKIPSPACE(s)

            NS_CHECKEND(s, "Misformed job run timeout request")

            int timeout = atoi(s);
            if (timeout < 0) {
                NS_RETURN_ERROR(
                    "Invalid job run timeout request: incorrect timeout value")
            }
            req->timeout = timeout;

            return;
        }
        break;

    case 'P':
        if (strncmp(s, "PUT", 3) == 0) {
            req->req_type = ePutJobResult;
            s += 3;
            NS_SKIPSPACE(s)
            parse_put_params:
            NS_GETSTRING(s, req->job_key_str)

            NS_SKIPSPACE(s)

            // return code
            if (*s) {
                req->job_return_code = atoi(s);
                // skip digits
                for (; isdigit((unsigned char)(*s)); ++s) {}
                if (!isspace((unsigned char)(*s))) {
                    goto put_format_error;
                }
            } else {
            put_format_error:
                NS_RETURN_ERROR("Misformed PUT request. Missing job return code.")
            }

            // output information
            NS_SKIPSPACE(s)
            
            if (*s !='"') {
                NS_RETURN_ERROR("Misformed PUT request")
            }
            char *ptr = req->output;
            for (++s; *s; ++s) {
                if (*s == '"' && *(s-1) != '\\') break;
                NS_CHECKEND(s, "Misformed PUT request")
                NS_CHECKSIZE(ptr-req->output, kNetScheduleMaxDataSize);
                *ptr++ = *s;            
            }
            *ptr = 0;

            return;
        }

        break;
    case 'W':
        if (strncmp(s, "WGET", 4) == 0) {
            req->req_type = eWaitGetJob;
            
            s += 4;
            NS_SKIPSPACE(s)

            NS_CHECKEND(s, "Misformed WGET request")

            int port = atoi(s);
            if (port > 0) {
                req->port = (unsigned)port;
            }

            for (; *s && isdigit((unsigned char)(*s)); ++s) {}

            NS_CHECKEND(s, "Misformed WGET request")
            NS_SKIPSPACE(s)
            NS_CHECKEND(s, "Misformed WGET request")

            int timeout = atoi(s);
            if (timeout <= 0) {
                timeout = 60;
            }
            req->timeout = timeout;
            
            return;
        }
        break;
    case 'C':
        if (strncmp(s, "CANCEL", 6) == 0) {
            req->req_type = eCancelJob;
            s += 6;
            NS_SKIPSPACE(s)
            NS_GETSTRING(s, req->job_key_str)
            
            if (req->job_key_str.empty()) {
                NS_RETURN_ERROR("Misformed CANCEL request")
            }
            return;
        }

        break;
    case 'D':
        if (strncmp(s, "DROJ", 4) == 0) {
            req->req_type = eDropJob;
            s += 4;
            NS_SKIPSPACE(s)
            NS_GETSTRING(s, req->job_key_str)
            
            if (req->job_key_str.empty()) {
                NS_RETURN_ERROR("Misformed drop job request")
            }
            return;
        }

        if (strncmp(s, "DROPQ", 4) == 0) {
            req->req_type = eDropQueue;
            return;
        }

        if (strncmp(s, "DUMP", 4) == 0) {
            req->req_type = eDumpQueue;
            s += 4;

            NS_SKIPSPACE(s)
            NS_GETSTRING(s, req->job_key_str)
            return;
        }

        break;
    case 'F':
        if (strncmp(s, "FPUT", 4) == 0) {
            req->req_type = ePutJobFailure;
            s += 4;
            NS_SKIPSPACE(s)
            NS_GETSTRING(s, req->job_key_str)

            if (req->job_key_str.empty()) {
                NS_RETURN_ERROR("Misformed put error request")
            }

            if (!*s) return;
            NS_SKIPSPACE(s)
            if (!*s) return;

            if (*s !='"') {
                NS_RETURN_ERROR("Misformed PUT error request")
            }
            for (++s; *s; ++s) {
                if (*s == '"' && *(s-1) != '\\') break;
                NS_CHECKEND(s, "Misformed PUT error request")
                req->err_msg.push_back(*s);
            }
            
            return;
        }
        break;

    case 'R':
        if (strncmp(s, "RETURN", 6) == 0) {
            req->req_type = eReturnJob;
            s += 6;
            NS_SKIPSPACE(s)
            NS_CHECKEND(s, "Misformed job return request")
            NS_GETSTRING(s, req->job_key_str)

            return;
        }
        if (strncmp(s, "RECO", 4) == 0) {
            req->req_type = eReloadConfig;
            return;
        }

        break;
    case 'Q':
        if (strncmp(s, "QUIT", 4) == 0) {
            req->req_type = eQuitSession;
            return;
        }
        if (strncmp(s, "QLST", 4) == 0) {
            req->req_type = eQList;
            return;
        }

        break;
    case 'V':
        if (strncmp(s, "VERSION", 7) == 0) {
            req->req_type = eVersion;
            return;
        }
        break;
    case 'L':
        if (strncmp(s, "LOG", 3) == 0) {
            req->req_type = eLogging;
            s += 3;
            NS_SKIPSPACE(s)
            NS_CHECKEND(s, "Misformed LOG request")
            NS_GETSTRING(s, req->job_key_str)
            return;
        } // LOG
        break;
    case 'B':
        if (strncmp(s, "BSUB", 4) == 0) {
            req->req_type = eSubmitBatch;
            s += 4;
            return;
        }
        break;
    default:
        req->req_type = eError;
        req->err_msg = "Unknown request";
        return;
    } // switch

    req->req_type = eError;
    req->err_msg = "Unknown request";
}



void CNetScheduleServer::WriteMsg(CSocket&       sock, 
                                  const char*    prefix, 
                                  const char*    msg,
                                  bool           comm_control)
{
    SThreadData*   tdata = x_GetThreadData();
    if (tdata == 0) {
        LOG_POST(Error << "Cannot deliver message (no TLS buffer):"
                       << prefix << msg);
        return;
    }
    size_t msg_length = 0;
    char* ptr = tdata->msg_buf;

    for (; *prefix; ++prefix) {
        *ptr++ = *prefix;
        ++msg_length;
    }
    for (; *msg; ++msg) {
        *ptr++ = *msg;
        ++msg_length;
        if (msg_length >= kMaxMessageSize) {
            LOG_POST(Error << "Message too large:" << msg);
            _ASSERT(0);
            return;
        }
    }
    *ptr++ = '\r';
    *ptr++ = '\n';
    msg_length = ptr - tdata->msg_buf;

    size_t n_written;
    EIO_Status io_st = 
        sock.Write(tdata->msg_buf, msg_length, &n_written);
    if (comm_control && io_st) {
        NCBI_THROW(CNetServiceException, 
                   eCommunicationError, "Socket write error.");
    }

}

bool CNetScheduleServer::IsLog() const 
{
    return m_LogFlag.Get() != 0;
}



bool CNetScheduleServer::x_CheckJobId(CSocket&          sock,
                                      CNetSchedule_Key* job_id, 
                                      const string&     job_key)
{
    try {
        CNetSchedule_ParseJobKey(job_id, job_key);
        if (job_id->version != 1     ||
            job_id->hostname.empty() ||
            job_id->id == 0          ||
            job_id->port == 0) 
        {
            NCBI_THROW(CNetScheduleException, eKeyFormatError, kEmptyStr);
        }
    } 
    catch (CNetScheduleException&)
    {
        string err = "JOB id format error. (";
        err += job_key;
        err += ")";
        WriteMsg(sock, "ERR:", err.c_str());
        return false;
    }
    return true;
}

SThreadData* CNetScheduleServer::x_GetThreadData()
{
    SThreadData* tdata = s_tls->GetValue();
    if (tdata) {
    } else {
        tdata = new SThreadData();
        s_tls->SetValue(tdata, TlsCleanup);
    }
    return tdata;
}

void CNetScheduleServer::x_SetSocketParams(CSocket* sock)
{
    sock->DisableOSSendDelay();
    STimeout to = {m_InactivityTimeout, 0};
    sock->SetTimeout(eIO_ReadWrite, &to);
}

void CNetScheduleServer::x_MakeLogMessage(CSocket& sock, SThreadData& tdata)
{
    CTime conn_tm = m_LocalTimer.GetLocalTime();
    string peer = sock.GetPeerAddress();
    // get only host name
    string::size_type offs = peer.find_first_of(":");
    if (offs != string::npos) {
        peer.erase(offs, peer.length());
    }

    string& lmsg = tdata.lmsg;
    lmsg.erase();

    lmsg += peer;
    lmsg += ';';
    lmsg += conn_tm.AsString();
    lmsg += ';';
    lmsg += tdata.auth;
    lmsg += ';';
    lmsg += tdata.queue;
    lmsg += ';';
    lmsg += tdata.request;
    lmsg += "\n";
}

void CNetScheduleServer::SetAdminHosts(const string& host_names)
{
    m_AdminHosts.SetHosts(host_names);
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
    auto_ptr<CRotatingLogStream> m_ErrLog;
};

void CNetScheduleDApp::Init(void)
{
    SetDiagPostLevel(eDiag_Info);
    SetDiagPostFlag(eDPF_DateTime);

    // Setup command line arguments and parameters
        
    // Create command-line argument descriptions class
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // Specify USAGE context
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "netscheduled");

    arg_desc->AddFlag("reinit", "Recreate the storage directory.");

    SetupArgDescriptions(arg_desc.release());
}


/// @internal
extern "C" void Threaded_Server_SignalHandler( int )
{
    if (s_netschedule_server && 
        (!s_netschedule_server->ShutdownRequested()) ) {
        s_netschedule_server->SetShutdownFlag();
        LOG_POST("Interrupt signal. Shutdown requested.");
    }
}




int CNetScheduleDApp::Run(void)
{
    LOG_POST(NETSCHEDULED_VERSION);
    
    CArgs args = GetArgs();

    try {
        const CNcbiRegistry& reg = GetConfig();

#if defined(NCBI_OS_UNIX)
        bool is_daemon =
            reg.GetBool("server", "daemon", false, 0, CNcbiRegistry::eReturn);
        if (is_daemon) {
            LOG_POST("Entering UNIX daemon mode...");
            bool daemon = Daemonize(0, fDaemon_DontChroot);
            if (!daemon) {
                return 0;
            }

        }
        
     // attempt to get server gracefully shutdown on signal
     signal( SIGINT, Threaded_Server_SignalHandler);
     signal( SIGTERM, Threaded_Server_SignalHandler);    
#endif
                   
        m_ErrLog.reset(new CNetScheduleLogStream("ns_err.log", 25 * 1024 * 1024));     
        // All errors redirected to rotated log
        // from this moment on the server is silent...
        SetDiagStream(m_ErrLog.get());
        

        CConfig conf(reg);

        const CConfig::TParamTree* param_tree = conf.GetTree();
        const TPluginManagerParamTree* bdb_tree = 
            param_tree->FindSubNode("bdb");

        if (!bdb_tree) {
            ERR_POST("BDB initialization section not found");
            return 1;
        }

        // Storage initialization

        
        CConfig bdb_conf((CConfig::TParamTree*)bdb_tree, eNoOwnership);
        const string& db_path = 
            bdb_conf.GetString("netschedule", "path", 
                                CConfig::eErr_Throw, kEmptyStr);
        bool reg_reinit =
            reg.GetBool("server", "reinit", false, 0, IRegistry::eReturn);

        if (args["reinit"] || reg_reinit) {  // Drop the database directory
            CDir dir(db_path);
            dir.Remove();
            LOG_POST(Info << "Reinintialization. " << db_path 
                          << " removed. \n");
        }

        unsigned mem_size = (unsigned)
            bdb_conf.GetDataSize("netschedule", "mem_size", 
                                 CConfig::eErr_NoThrow, 0);
        int max_locks = 
            bdb_conf.GetInt("netschedule", "max_locks", 
                                 CConfig::eErr_NoThrow, 0);

        unsigned log_mem_size = (unsigned)
            bdb_conf.GetDataSize("netschedule", "log_mem_size", 
                                 CConfig::eErr_NoThrow, 0);
        LOG_POST(Info << "Mounting database at: " << db_path);

        auto_ptr<CQueueDataBase> qdb(new CQueueDataBase());
        qdb->Open(db_path, mem_size, max_locks, log_mem_size);


        int port = 
            reg.GetInt("server", "port", 9100, 0, CNcbiRegistry::eReturn);

        int udp_port = 
            reg.GetInt("server", "udp_port", 0, 0, CNcbiRegistry::eReturn);
        if (udp_port == 0) {
            udp_port = port + 1;
            LOG_POST(Info << "UDP notification port: " << udp_port);
        }
        if (udp_port < 1024 || udp_port > 65535) {
            LOG_POST(Error << "Invalid UDP port value: " << udp_port 
                           << ". Notification will be disabled.");
            udp_port = -1;
        }
        if (udp_port < 0) {
            LOG_POST(Info << "UDP notification disabled. ");
        }
        if (udp_port > 0) {
            qdb->SetUdpPort((unsigned short) udp_port);
        }



        string qname = "noname";
        LOG_POST(Info << "Mounting queue: " << qname);
        qdb->MountQueue(qname, 3600, 7, 3600, 1800, kEmptyStr); // default queue


        // Scan and mount queues
        unsigned min_run_timeout = 3600;

        qdb->ReadConfig(reg, &min_run_timeout);

        LOG_POST(Info << "Running execution control every " 
                      << min_run_timeout << " seconds. ");
        min_run_timeout = min_run_timeout >= 0 ? min_run_timeout : 2;
        
        
        qdb->RunExecutionWatcherThread(min_run_timeout);
        qdb->RunPurgeThread();
        qdb->RunNotifThread();


        unsigned max_threads =
            reg.GetInt("server", "max_threads", 25, 0, CNcbiRegistry::eReturn);
        unsigned init_threads =
            reg.GetInt("server", "init_threads", 5, 0, CNcbiRegistry::eReturn);
        unsigned network_timeout =
            reg.GetInt("server", "network_timeout", 10, 0, CNcbiRegistry::eReturn);
        if (network_timeout == 0) {
            LOG_POST(Warning << 
                "INI file sets 0 sec. network timeout. Assume 10 seconds.");
            network_timeout =  10;
        }

        bool is_log =
            reg.GetBool("server", "log", false, 0, CNcbiRegistry::eReturn);

        auto_ptr<CNetScheduleServer> thr_srv(
            new CNetScheduleServer(port, 
                                   qdb.release(),
                                   max_threads, 
                                   init_threads,
                                   network_timeout,
                                   is_log));

        string admin_hosts =
            reg.GetString("server", "admin_host", kEmptyStr);
        if (!admin_hosts.empty()) {
            thr_srv->SetAdminHosts(admin_hosts);
        }

        
        LOG_POST(Info << "Running server on port " << port);

        thr_srv->Run();

        LOG_POST("NetSchedule server stoped.");

    }
    catch (CBDB_ErrnoException& ex)
    {
        LOG_POST(Error << "Error: DBD errno exception:" << ex.what());
        return 1;
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

     return 
        CNetScheduleDApp().AppMain(argc, argv, 0, 
                           eDS_Default, "netscheduled.ini");
}

/*
 * ===========================================================================
 * $Log$
 * Revision 1.55  2005/08/24 18:17:59  kuznets
 * Incremented program version
 *
 * Revision 1.54  2005/08/24 16:57:56  kuznets
 * Optimization of command parsing
 *
 * Revision 1.53  2005/08/22 14:01:58  kuznets
 * Added JobExchange command
 *
 * Revision 1.52  2005/08/17 14:24:37  kuznets
 * Improved err checking
 *
 * Revision 1.51  2005/08/16 13:45:26  kuznets
 * Added monitoring for batch submissions
 *
 * Revision 1.50  2005/08/15 17:39:48  kuznets
 * Use C-style ReadLine for better performance
 *
 * Revision 1.49  2005/08/15 13:29:46  kuznets
 * Implemented batch job submission
 *
 * Revision 1.48  2005/08/10 19:20:46  kuznets
 * Set connection accept timeout 1sec
 *
 * Revision 1.47  2005/07/14 13:12:56  kuznets
 * Added load balancer
 *
 * Revision 1.46  2005/06/27 15:52:20  kuznets
 * Minor change in statistics
 *
 * Revision 1.45  2005/06/20 13:31:08  kuznets
 * Added access control for job submitters and worker nodes
 *
 * Revision 1.44  2005/06/03 16:27:36  lavr
 * Explicit (unsigned char) casts in ctype routines
 *
 * Revision 1.43  2005/05/17 14:25:00  ucko
 * Tweak ProcessShutdown to avoid jumping over a variable declaration.
 *
 * Revision 1.42  2005/05/17 13:52:08  kuznets
 * Restrictions (optional) on server shutdown
 *
 * Revision 1.41  2005/05/16 16:21:26  kuznets
 * Added available queues listing
 *
 * Revision 1.40  2005/05/12 18:37:33  kuznets
 * Implemented config reload
 *
 * Revision 1.39  2005/05/12 13:11:16  kuznets
 * Added netschedule_admin to the list of admin tools
 *
 * Revision 1.38  2005/05/06 13:07:32  kuznets
 * Fixed bug in cleaning database
 *
 * Revision 1.37  2005/05/05 16:07:15  kuznets
 * Added individual job dumping
 *
 * Revision 1.36  2005/05/04 19:09:43  kuznets
 * Added queue dumping
 *
 * Revision 1.35  2005/05/03 19:46:00  kuznets
 * Fixed bug in remote monitoring
 *
 * Revision 1.34  2005/05/02 14:44:40  kuznets
 * Implemented remote monitoring
 *
 * Revision 1.33  2005/04/28 17:40:26  kuznets
 * Added functions to rack down forgotten nodes
 *
 * Revision 1.32  2005/04/27 18:12:16  kuznets
 * Logging improved
 *
 * Revision 1.31  2005/04/27 15:00:18  kuznets
 * Improved error messaging
 *
 * Revision 1.30  2005/04/26 18:31:04  kuznets
 * Corrected bug in daemonization
 *
 * Revision 1.29  2005/04/25 18:20:59  kuznets
 * Added daeminization.
 *
 * Revision 1.28  2005/04/21 13:38:05  kuznets
 * Version increment
 *
 * Revision 1.27  2005/04/20 15:59:33  kuznets
 * Progress message to Submit
 *
 * Revision 1.26  2005/04/19 19:34:12  kuznets
 * Adde progress report messages
 *
 * Revision 1.25  2005/04/11 13:53:51  kuznets
 * Implemented logging
 *
 * Revision 1.24  2005/04/06 12:39:55  kuznets
 * + client version control
 *
 * Revision 1.23  2005/03/31 19:28:34  kuznets
 * Corrected use of prep macros
 *
 * Revision 1.22  2005/03/31 19:18:29  kuznets
 * Added build date to version string" netscheduled.cpp
 *
 * Revision 1.21  2005/03/29 16:48:54  kuznets
 * Removed dead code
 *
 * Revision 1.20  2005/03/24 16:42:25  kuznets
 * Fixed bug in command parser
 *
 * Revision 1.19  2005/03/22 19:02:54  kuznets
 * Reflecting chnages in connect layout
 *
 * Revision 1.18  2005/03/22 16:16:10  kuznets
 * Statistics improvement: added database information
 *
 * Revision 1.17  2005/03/21 13:08:24  kuznets
 * + STAT command support
 *
 * Revision 1.16  2005/03/17 20:37:07  kuznets
 * Implemented FPUT
 *
 * Revision 1.15  2005/03/17 16:05:21  kuznets
 * Sensible error message if timeout expired
 *
 * Revision 1.14  2005/03/17 14:07:19  kuznets
 * Fixed bug in request parsing
 *
 * Revision 1.13  2005/03/15 20:14:30  kuznets
 * Implemented notification to client waiting for job
 *
 * Revision 1.12  2005/03/15 14:52:39  kuznets
 * Better datagram socket management, DropJob implemenetation
 *
 * Revision 1.11  2005/03/10 14:19:57  kuznets
 * Implemented individual run timeouts
 *
 * Revision 1.10  2005/03/09 17:37:16  kuznets
 * Added node notification thread and execution control timeline
 *
 * Revision 1.9  2005/03/04 12:06:41  kuznets
 * Implenyed UDP callback to clients
 *
 * Revision 1.8  2005/02/28 12:24:17  kuznets
 * New job status Returned, better error processing and queue management
 *
 * Revision 1.7  2005/02/23 19:16:38  kuznets
 * Implemented garbage collection thread
 *
 * Revision 1.6  2005/02/22 16:13:00  kuznets
 * Performance optimization
 *
 * Revision 1.5  2005/02/14 17:57:41  kuznets
 * Fixed a bug in queue procesing
 *
 * Revision 1.4  2005/02/14 14:42:52  kuznets
 * Overloaded ProcessOverflow to better detect queue shortage
 *
 * Revision 1.3  2005/02/10 19:59:40  kuznets
 * Implemented GET and PUT
 *
 * Revision 1.2  2005/02/09 18:56:42  kuznets
 * Implemented SUBMIT, CANCEL, STATUS, SHUTDOWN
 *
 * Revision 1.1  2005/02/08 16:42:55  kuznets
 * Initial revision
 *
 *
 * ===========================================================================
 */
