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
 * Authors:  Victor Joukov
 *
 * File Description: Network cache daemon
 *
 */
#include <ncbi_pch.hpp>
#include <corelib/ncbi_param.hpp>

#include "message_handler.hpp"
#include "nc_handler.hpp"
#include "ic_handler.hpp"
#include "netcache_version.hpp"

// For emulation of CTransmissionReader
#include <util/util_exception.hpp>


BEGIN_NCBI_SCOPE

/////////////////////////////////////////////////////////////////////////////
// MessageHandler implementation
void MessageHandler::OnRead(void)
{
    CSocket &socket = GetSocket();
    char read_buf[kNetworkBufferSize];
    size_t n_read;

    if (!m_InRequest) {
        m_Stat.InitRequest();
        m_InRequest = true;
    }
    EIO_Status status;
    {{
        CTimeGuard time_guard(m_Stat.comm_elapsed);
        status = socket.Read(read_buf, sizeof(read_buf), &n_read);
        ++m_Stat.io_blocks;
    }}
    switch (status) {
    case eIO_Success:
        break;
    case eIO_Timeout:
        this->OnTimeout();
        return;
    case eIO_Closed:
        this->OnClose();
        return;
    default:
        // TODO: ??? OnError
        return;
    }
    int message_tail;
    char *buf_ptr = read_buf;
    for ( ;n_read > 0; ) {
        message_tail = this->CheckMessage(&m_Buffer, buf_ptr, n_read);
        // TODO: what should we do if message_tail > n_read?
        if (message_tail < 0) {
            return;
        } else {
            this->OnMessage(m_Buffer);
        }
        int consumed = n_read - message_tail;
        buf_ptr += consumed;
        n_read -= consumed;
    }
}


/////////////////////////////////////////////////////////////////////////////
// CNetCache_MessageHandler implementation
CNetCache_MessageHandler::CNetCache_MessageHandler(CNetCacheServer* server)
    : m_Server(server), m_SeenCR(false),
    m_ICHandler(new CICacheHandler(this)),
    m_NCHandler(new CNetCacheHandler(this)),
    m_InTransmission(false), m_DelayedWrite(false),
    m_LastHandler(0)
{
}


void CNetCache_MessageHandler::BeginReadTransmission()
{
    m_InTransmission = true;
    m_SigRead = 0;
    m_Signature = 0;
    m_ProcessMessage = &CNetCache_MessageHandler::ProcessMsgTransmission;
}


void CNetCache_MessageHandler::BeginDelayedWrite()
{
    m_DelayedWrite = true;
}


CNetCacheServer* CNetCache_MessageHandler::GetServer()
{
    return m_Server;
}


SNetCache_RequestStat* CNetCache_MessageHandler::GetStat()
{
    return &m_Stat;
}


const string* CNetCache_MessageHandler::GetAuth()
{
    return &m_Auth;
}


int CNetCache_MessageHandler::CheckMessage(BUF* buffer, const void *data, size_t size)
{
    if (!m_InTransmission)
        return Server_CheckLineMessage(buffer, data, size, m_SeenCR);
    unsigned start = 0;
    const unsigned char * msg = (const unsigned char *) data;
    if (size && m_SeenCR && msg[0] == '\n') {
        ++start;
    }
    m_SeenCR = false;
    for (size_t n = start; n < size; ++n) {
        if (m_SigRead < 4) {
            // Reading signature
            m_Signature = (m_Signature << 8) + msg[n];
            if (++m_SigRead == 4) {
                if (m_Signature == 0x01020304)
                    m_ByteSwap = false;
                else if (m_Signature == 0x04030201)
                    m_ByteSwap = true;
                else
                    NCBI_THROW(CUtilException, eWrongData,
                               "Cannot determine the byte order. Got: " +
                               NStr::UIntToString(m_Signature, 0, 16));
                // Signature read and verified, read chunk length
                m_LenRead = 0;
                m_Length = 0;
            }
        } else if (m_LenRead < 4) {
            // Reading length
            m_Length = (m_Length << 8) + msg[n];
            if (++m_LenRead == 4) {
                if (m_Length == 0xffffffff) {
                    _TRACE("Got end of transmission, tail is " << (size-n-1) << " buf size " << BUF_Size(*buffer));
                    m_InTransmission = false;
                    OnRequestEnd();
                    return size-n-1;
                }
                if (m_ByteSwap)
                    m_Length = CByteSwap::GetInt4((unsigned char*)&m_Length);
                // Length read, read data
                start = n + 1;
            }
        } else {
            unsigned chunk_len = min(m_Length, (unsigned)(size-start));
            _TRACE("Transmission buffer written " << chunk_len);
            BUF_Write(buffer, msg+start, chunk_len);
            m_Length -= chunk_len;
            n += chunk_len - 1;
            if (!m_Length) {
                // Read next length, in next CheckMessage invokation
                m_LenRead = 0;
                return size-n-1;
            }
        }
    }
    return -1;
}


EIO_Event CNetCache_MessageHandler::GetEventsToPollFor(const CTime**) const
{
    if (m_DelayedWrite) return eIO_Write;
    return eIO_Read;
}


void CNetCache_MessageHandler::OnOpen(void)
{
    CSocket& socket = GetSocket();
    socket.DisableOSSendDelay();
    STimeout to = {m_Server->GetInactivityTimeout(), 0};
    socket.SetTimeout(eIO_ReadWrite, &to);

    m_Stat.InitSession(m_Server->GetTimer().GetLocalTime(),
                       socket.GetPeerAddress());
                    
    m_ProcessMessage = &CNetCache_MessageHandler::ProcessMsgAuth;
    m_DelayedWrite = false;
}


void CNetCache_MessageHandler::OnRead(void)
{
    CCounterGuard pending_req_guard(m_Server->GetRequestCounter());
    MessageHandler::OnRead();
}


void CNetCache_MessageHandler::OnWrite(void)
{
    CCounterGuard pending_req_guard(m_Server->GetRequestCounter());
    if (!m_DelayedWrite) return;
    if (!m_LastHandler) {
        // There MUST be m_LastHandler set, so it is apparently
        // an internal logic error
        ERR_POST(Error << "No m_LastHandler set while got OnWrite");
        m_DelayedWrite = false;
        return;
    }
    bool res = m_LastHandler->ProcessWrite();
    m_DelayedWrite = m_DelayedWrite && res;
    if (!m_DelayedWrite) OnRequestEnd();
}


void CNetCache_MessageHandler::OnClose(void)
{
}


void CNetCache_MessageHandler::OnTimeout(void)
{
}


void CNetCache_MessageHandler::OnOverflow(void)
{
}


void CNetCache_MessageHandler::OnMessage(BUF buffer)
{
    if (m_Server->ShutdownRequested()) {
        WriteMsg("ERR:",
            "NetSchedule server is shutting down. Session aborted.");
        return;
    }

    // TODO: move exception handling here
    // try {
        (this->*m_ProcessMessage)(buffer);
    // }
}


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


void CNetCache_MessageHandler::ProcessMsgAuth(BUF buffer)
{
    s_ReadBufToString(buffer, m_Auth);
    m_ProcessMessage = &CNetCache_MessageHandler::ProcessMsgRequest;
}


void CNetCache_MessageHandler::ProcessSM(CSocket& socket, string& req)
{
    // SMR host pid     -- registration
    // or
    // SMU host pid     -- unregistration
    //
    if (!m_Server->IsManagingSessions()) {
        WriteMsg("ERR:", "Server does not support sessions ");
        return;
    }

    if (req[0] == 'S' && req[1] == 'M') {
        bool reg;
        if (req[2] == 'R') {
            reg = true;
        } else
        if (req[2] == 'U') {
            reg = false;
        } else {
            goto err;
        }
        req.erase(0, 3);
        NStr::TruncateSpacesInPlace(req, NStr::eTrunc_Begin);
        string host, port_str;
        bool split = NStr::SplitInTwo(req, " ", host, port_str);
        if (!split) {
            goto err;
        }
        unsigned port = NStr::StringToUInt(port_str);

        if (!port || host.empty()) {
            goto err;
        }

        if (reg) {
            m_Server->RegisterSession(host, port);
        } else {
            m_Server->UnRegisterSession(host, port);
        }
        WriteMsg("OK:", "");
    } else {
        err:
        WriteMsg("ERR:", "Invalid request ");
    }
}


NCBI_PARAM_DECL(double, server, log_threshold);
static NCBI_PARAM_TYPE(server, log_threshold) s_LogThreshold;
NCBI_PARAM_DEF(double, server, log_threshold, 1.0);

void CNetCache_MessageHandler::ProcessMsgRequest(BUF buffer)
{
    CSocket& socket = GetSocket();

    bool close_socket = false;
    string request;
    s_ReadBufToString(buffer, request);
    try {
        {{
        if (request.length() < 2) { 
            WriteMsg("ERR:", "Invalid request");
            m_Server->RegisterProtocolErr(
                SBDB_CacheUnitStatistics::eErr_Unknown, request);
            return;
        }

        // check if it is NC or IC
        _TRACE("Processing request " << request.substr(0, 100));
        m_Stat.request = request;
        if (request[0] == 'I' && request[1] == 'C') {
            // ICache request
            m_LastHandler = m_ICHandler.get();
            m_ICHandler->SetSocket(&socket);
            m_ICHandler->ProcessRequest(request, m_Stat);
        } else if (request[0] == 'A' && request[1] == '?') {
            // Alive?
            WriteMsg("OK:", "");
        } else if (request[0] == 'S' && request[1] == 'M') {
            // Session management
            if (1) m_Stat.type = "Session";
            ProcessSM(socket, request);
            // need to disconnect after reg-unreg
            close_socket = true;
        } else if (request[0] == 'O' && request[1] == 'K') {
        } else if (request == "MONI") {
            socket.DisableOSSendDelay(false);
            WriteMsg("OK:", "Monitor for " NETCACHED_VERSION "\n");
            m_Server->SetMonitorSocket(socket);
        } else {
            // NetCache request
            m_LastHandler = m_NCHandler.get();
            m_NCHandler->SetSocket(&socket);
            m_NCHandler->ProcessRequest(request, m_Stat);
        }
        }}
        if (!m_InTransmission && !m_DelayedWrite) {
            OnRequestEnd();
        }

        // Monitoring
        //
        if (IsMonitoring()) {
            string msg, tmp;
            msg += m_Auth;
            msg += "\"";
            msg += request;
            msg += "\" ";
            msg += m_Stat.peer_address;
            msg += "\n\t";
            msg += "ConnTime=" + m_Stat.conn_time.AsString();
            msg += " BLOB size=";
            NStr::UInt8ToString(tmp, m_Stat.blob_size);
            msg += tmp;
            msg += " elapsed=";
            msg += NStr::DoubleToString(m_Stat.elapsed, 5);
            msg += " comm.elapsed=";
            msg += NStr::DoubleToString(m_Stat.comm_elapsed, 5);
            msg += "\n\t";
            msg += m_Stat.type + ":" + m_Stat.details;

            MonitorPost(msg);
        }

        if (close_socket) {
            socket.Close();
        }
    } 
    catch (CNetCacheException &ex)
    {
        ReportException(ex, "NC Server error: ", request);
        m_Server->RegisterException(m_Stat.op_code, m_Auth, ex);
    }
    catch (CNetServiceException& ex)
    {
        ReportException(ex, "NC Service exception: ", request);
        m_Server->RegisterException(m_Stat.op_code, m_Auth, ex);
    }
    catch (CBDB_ErrnoException& ex)
    {
        if (ex.IsRecovery()) {
            string msg = "Fatal Berkeley DB error: DB_RUNRECOVERY. " 
                         "Emergency shutdown initiated!";
            ERR_POST(msg);
            if (IsMonitoring()) {
                MonitorPost(msg);
            }
            m_Server->SetShutdownFlag();
        } else {
            ReportException(ex, "NC BerkeleyDB error: ", request);
            m_Server->RegisterInternalErr(m_Stat.op_code, m_Auth);
        }
        throw;
    }
    catch (CException& ex)
    {
        ReportException(ex, "NC CException: ", request);
        m_Server->RegisterInternalErr(m_Stat.op_code, m_Auth);
    }
    catch (exception& ex)
    {
        ReportException(ex, "NC std::exception: ", request);
        m_Server->RegisterInternalErr(m_Stat.op_code, m_Auth);
    }
}


void CNetCache_MessageHandler::OnRequestEnd()
{
    m_Stat.EndRequest();
    m_InRequest = false;
    if (m_Server->IsLogForced() ||
        (m_Server->IsLog() && m_Stat.elapsed > s_LogThreshold.Get())) {
#ifdef _DEBUG
        /*
        string trace;
        ITERATE(vector<STraceEvent>, it, m_Stat.events) {
            trace += NStr::DoubleToString(it->time_mark, 6);
            trace += " ";
            trace += it->message;
            trace += "\n";
        }
        */
#endif
#ifdef REPORT_UNACCOUNTED
        int unaccounted = (int)
            ((m_Stat.elapsed - m_Stat.comm_elapsed - m_Stat.db_elapsed)
                / m_Stat.elapsed * 100.0);
#endif
        unsigned pending_requests = m_Server->GetRequestCounter().Get();
        ostrstream msg;
        msg << m_Auth
            << " peer=" << m_Stat.peer_address
            << " request='" << m_Stat.request << "'"
            << " blob=" << m_Stat.blob_id
            << " blobsize=" << m_Stat.blob_size
            << " io blocks=" << m_Stat.io_blocks
            << " pending=" << pending_requests
            << " req time=" << NStr::DoubleToString(m_Stat.elapsed, 5) 
            << " comm time=" << NStr::DoubleToString(m_Stat.comm_elapsed, 5)
//            << " lock wait time=" << stat.lock_elapsed
            << " db time=" << NStr::DoubleToString(m_Stat.db_elapsed, 5)
//            << " db hits=" << stat.db_accesses
#ifdef REPORT_UNACCOUNTED
            << " unaccounted=" << unaccounted
#endif
#ifdef _DEBUG
//            << "\n" << trace
#endif
        ;
        LOG_POST(Error << (string) CNcbiOstrstreamToString(msg));
    }
}


void CNetCache_MessageHandler::MonitorException(const std::exception& ex,
                                                const string& comment,
                                                const string& request)
{
    CSocket& socket = GetSocket();
    string msg = comment;
    msg.append(ex.what());
    msg.append(" client="); msg.append(m_Auth);
    msg.append(" request='"); msg.append(request); msg.append("'");
    msg.append(" peer="); msg.append(socket.GetPeerAddress());
    msg.append(" blobsize=");
        msg.append(NStr::UIntToString(m_Stat.blob_size));
    msg.append(" io blocks=");
        msg.append(NStr::UIntToString(m_Stat.io_blocks));
    MonitorPost(msg);
}


void CNetCache_MessageHandler::ReportException(const std::exception& ex,
                                               const string& comment,
                                               const string& request)
{
    CSocket& socket = GetSocket();
    ERR_POST(Error << ex.what()
        << " client=" << m_Auth
        << " request='" << request << "'"
        << " peer=" << socket.GetPeerAddress()
        << " blobsize=" << NStr::UIntToString(m_Stat.blob_size)
        << " io blocks=" << NStr::UIntToString(m_Stat.io_blocks));
    if (IsMonitoring()) {
        MonitorException(ex, comment, request);
    }
}


void CNetCache_MessageHandler::ReportException(const CException& ex,
                                               const string& comment,
                                               const string& request)
{
    CSocket& socket = GetSocket();
    LOG_POST(Error << ex
        << " client=" << m_Auth
        << " request='" << request << "'"
        << " peer=" << socket.GetPeerAddress()
        << " blobsize=" << NStr::UIntToString(m_Stat.blob_size)
        << " io blocks=" << NStr::UIntToString(m_Stat.io_blocks));
    if (IsMonitoring()) {
        MonitorException(ex, comment, request);
    }
}


void CNetCache_MessageHandler::ProcessMsgTransmission(BUF buffer)
{
    if (m_LastHandler) {
        string data;
        s_ReadBufToString(buffer, data);
        bool res = m_LastHandler->ProcessTransmission(
            data.data(),
            data.size(),
            m_InTransmission ?
                CNetCache_RequestHandler::eTransmissionMoreBuffers :
                CNetCache_RequestHandler::eTransmissionLastBuffer);
        m_InTransmission = m_InTransmission && res;
    }
    if (!m_InTransmission) {
        m_ProcessMessage = &CNetCache_MessageHandler::ProcessMsgRequest;
        OnRequestEnd();
    }
}


END_NCBI_SCOPE
