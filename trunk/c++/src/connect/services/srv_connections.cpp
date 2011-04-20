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
 *
 */

#include <ncbi_pch.hpp>

#include "srv_rw.hpp"

#include <connect/services/srv_connections_expt.hpp>
#include <connect/services/error_codes.hpp>

#include <connect/ncbi_conn_exception.hpp>

#include <corelib/ncbi_system.hpp>


#define NCBI_USE_ERRCODE_X   ConnServ_Connection


BEGIN_NCBI_SCOPE

static const STimeout s_ZeroTimeout = {0, 0};

///////////////////////////////////////////////////////////////////////////
SNetServerMultilineCmdOutputImpl::~SNetServerMultilineCmdOutputImpl()
{
    if (!m_ReadCompletely) {
        m_Connection->Close();
        ERR_POST_X(7, "Multiline command output was not completely read");
    }
}

bool CNetServerMultilineCmdOutput::ReadLine(string& output)
{
    _ASSERT(!m_Impl->m_ReadCompletely);

    if (!m_Impl->m_FirstLineConsumed) {
        output = m_Impl->m_FirstOutputLine;
        m_Impl->m_FirstOutputLine = kEmptyStr;
        m_Impl->m_FirstLineConsumed = true;
    } else if (!m_Impl->m_NetCacheCompatMode)
        m_Impl->m_Connection->ReadCmdOutputLine(output);
    else {
        try {
            m_Impl->m_Connection->ReadCmdOutputLine(output);
        }
        catch (CNetSrvConnException& e) {
            if (e.GetErrCode() != CNetSrvConnException::eConnClosedByServer)
                throw;

            m_Impl->m_ReadCompletely = true;
            return false;
        }
    }

    if (output != "END")
        return true;
    else {
        m_Impl->m_ReadCompletely = true;
        return false;
    }
}

inline SNetServerConnectionImpl::SNetServerConnectionImpl(
    SNetServerImpl* server) :
        m_Server(server),
        m_NextFree(NULL)
{
    if (TServConn_UserLinger2::GetDefault())
        m_Socket.SetTimeout(eIO_Close, &s_ZeroTimeout);
}

void SNetServerConnectionImpl::DeleteThis()
{
    // Return this connection to the pool.
    if (m_Server->m_Service->m_PermanentConnection != eOff &&
            m_Socket.GetStatus(eIO_Open) == eIO_Success) {
        TFastMutexGuard guard(m_Server->m_FreeConnectionListLock);

        int upper_limit = TServConn_MaxConnPoolSize::GetDefault();

        if (upper_limit == 0 ||
                m_Server->m_FreeConnectionListSize < upper_limit) {
            m_NextFree = m_Server->m_FreeConnectionListHead;
            m_Server->m_FreeConnectionListHead = this;
            ++m_Server->m_FreeConnectionListSize;
            m_Server = NULL;
            return;
        }
    }

    // Could not return the connection to the pool, delete it.
    delete this;
}

void SNetServerConnectionImpl::ReadCmdOutputLine(string& result)
{
    EIO_Status io_st = m_Socket.ReadLine(result);

    switch (io_st)
    {
    case eIO_Success:
        break;
    case eIO_Timeout:
        Abort();
        NCBI_THROW(CNetSrvConnException, eReadTimeout,
            "Communication timeout reading from " +
                m_Server->m_Address.AsString());
        break;
    case eIO_Closed:
        Abort();
        NCBI_THROW(CNetSrvConnException, eConnClosedByServer,
            "Connection closed by " + m_Server->m_Address.AsString());
        break;
    default: // invalid socket or request, bailing out
        Abort();
        NCBI_THROW(CNetSrvConnException, eCommunicationError,
            "Communication error reading from " +
                m_Server->m_Address.AsString());
    }

    if (NStr::StartsWith(result, "OK:")) {
        result.erase(0, sizeof("OK:") - 1);
    } else if (NStr::StartsWith(result, "ERR:")) {
        result.erase(0, sizeof("ERR:") - 1);
        result = NStr::ParseEscapes(result);
        m_Server->m_Service->m_Listener->OnError(result, m_Server);
    }
}


void SNetServerConnectionImpl::Close()
{
    m_Socket.Close();
}

void SNetServerConnectionImpl::Abort()
{
    m_Socket.Abort();
}

SNetServerConnectionImpl::~SNetServerConnectionImpl()
{
    Close();
}

void SNetServerConnectionImpl::WriteLine(const string& line)
{
    // TODO change to "\n" when no old NS/NC servers remain.
    string str(line + "\r\n");

    const char* buf = str.data();
    size_t len = str.size();

    while (len > 0) {
        size_t n_written;

        EIO_Status io_st = m_Socket.Write(buf, len, &n_written);

        if (io_st != eIO_Success) {
            Abort();

            CIO_Exception io_ex(DIAG_COMPILE_INFO, 0,
                (CIO_Exception::EErrCode) io_st, "IO error.");

            NCBI_THROW(CNetSrvConnException, eWriteFailure,
                "Failed to write to " + m_Server->m_Address.AsString() +
                ": " + string(io_ex.what()));
        }
        len -= n_written;
        buf += n_written;
    }
}

string CNetServerConnection::Exec(const string& cmd)
{
    m_Impl->WriteLine(cmd);

    string output;
    m_Impl->ReadCmdOutputLine(output);

    return output;
}

CNetServerMultilineCmdOutput::CNetServerMultilineCmdOutput(
    const CNetServer::SExecResult& exec_result) :
        m_Impl(new SNetServerMultilineCmdOutputImpl(exec_result))
{
}

/*************************************************************************/
SNetServerImplReal::SNetServerImplReal(const string& host,
    unsigned short port) : SNetServerImpl(host, port)
{
    m_FreeConnectionListHead = NULL;
    m_FreeConnectionListSize = 0;

    ResetThrottlingParameters();
}

void SNetServerImpl::DeleteThis()
{
    SNetServiceImpl* service_impl = m_Service;

    if (service_impl == NULL)
        return;

    // Before resetting the m_Service pointer, verify that no other object
    // has acquired a reference to this server object yet (between
    // the time the reference counter went to zero, and the
    // current moment when m_Service is about to be reset).
    CFastMutexGuard g(service_impl->m_ServerMutex);

    if (!Referenced())
        m_Service = NULL;
}

SNetServerImplReal::~SNetServerImplReal()
{
    // No need to lock the mutex in the destructor.
    SNetServerConnectionImpl* impl = m_FreeConnectionListHead;
    while (impl != NULL) {
        SNetServerConnectionImpl* next_impl = impl->m_NextFree;
        delete impl;
        impl = next_impl;
    }
}

string CNetServer::GetHost() const
{
    return m_Impl->m_Address.host;
}

unsigned short CNetServer::GetPort() const
{
    return m_Impl->m_Address.port;
}

CNetServerConnection SNetServerImpl::GetConnectionFromPool()
{
    for (;;) {
        TFastMutexGuard guard(m_FreeConnectionListLock);

        if (m_FreeConnectionListSize == 0)
            return NULL;

        // Get an existing connection object from the connection pool.
        CNetServerConnection conn = m_FreeConnectionListHead;

        m_FreeConnectionListHead = conn->m_NextFree;
        --m_FreeConnectionListSize;
        conn->m_Server = this;

        guard.Release();

        // Check if the socket is already connected.
        if (conn->m_Socket.GetStatus(eIO_Open) != eIO_Success)
            continue;

        SSOCK_Poll conn_socket_poll_struct = {
            /* sock     */ conn->m_Socket.GetSOCK(),
            /* event    */ eIO_ReadWrite
            /* revent   */ // [out]
        };

        if (SOCK_Poll(1, &conn_socket_poll_struct, &s_ZeroTimeout, NULL) ==
                eIO_Success && conn_socket_poll_struct.revent == eIO_Write)
            return conn;

        conn->m_Socket.Close();
    }
}

CNetServerConnection SNetServerImpl::Connect()
{
    CNetServerConnection conn = new SNetServerConnectionImpl(this);

    EIO_Status io_st = conn->m_Socket.Connect(m_Address.host, m_Address.port,
        &m_Service->m_Timeout, fSOCK_LogOff | fSOCK_KeepAlive);

    if (io_st != eIO_Success) {
        conn->m_Socket.Close();

        string message = "Could not connect to " + m_Address.AsString();
        message += ": ";
        message += IO_StatusStr(io_st);

        NCBI_THROW(CNetSrvConnException, eConnectionFailure, message);
    }

    RegisterConnectionEvent(false);

    conn->m_Socket.SetDataLogging(eDefault);
    conn->m_Socket.SetTimeout(eIO_ReadWrite, &m_Service->m_Timeout);
    conn->m_Socket.DisableOSSendDelay();
    conn->m_Socket.SetReuseAddress(eOn);

    m_Service->m_Listener->OnConnected(conn);

    return conn;
}

void SNetServerImpl::ConnectAndExec(const string& cmd,
    CNetServer::SExecResult& exec_result)
{
    CheckIfThrottled();

    // Silently reconnect if the connection was taken
    // from the pool and it was closed by the server
    // due to inactivity.
    while ((exec_result.conn = GetConnectionFromPool()) != NULL) {
        try {
            exec_result.response = exec_result.conn.Exec(cmd);
            return;
        }
        catch (CNetSrvConnException& e) {
            CException::TErrCode err_code = e.GetErrCode();
            if (err_code != CNetSrvConnException::eWriteFailure &&
                err_code != CNetSrvConnException::eConnClosedByServer)
            {
                RegisterConnectionEvent(true);
                throw;
            }
        }
    }

    try {
        exec_result.conn = Connect();
        exec_result.response = exec_result.conn.Exec(cmd);
    }
    catch (CNetSrvConnException&) {
        RegisterConnectionEvent(true);
        throw;
    }
}

void SNetServerImpl::RegisterConnectionEvent(bool failure)
{
    if (m_Service->m_ServerThrottlePeriod > 0) {
        CFastMutexGuard guard(m_ThrottleLock);

        if (m_Service->m_MaxSubsequentConnectionFailures > 0) {
            if (failure)
                ++m_NumberOfSuccessiveFailures;
            else if (m_NumberOfSuccessiveFailures <
                    m_Service->m_MaxSubsequentConnectionFailures)
                m_NumberOfSuccessiveFailures = 0;
        }

        if (m_Service->m_ReconnectionFailureThresholdNumerator > 0) {
            if (m_ConnectionFailureRegister[
                    m_ConnectionFailureRegisterIndex] != failure) {
                if ((m_ConnectionFailureRegister[
                        m_ConnectionFailureRegisterIndex] = failure) == false)
                    --m_ConnectionFailureCounter;
                else
                    ++m_ConnectionFailureCounter;
            }

            if (++m_ConnectionFailureRegisterIndex >=
                    m_Service->m_ReconnectionFailureThresholdDenominator)
                m_ConnectionFailureRegisterIndex = 0;
        }
    }
}

void SNetServerImpl::CheckIfThrottled()
{
    if (m_Service->m_ServerThrottlePeriod > 0) {
        CFastMutexGuard guard(m_ThrottleLock);

        if (m_Throttled) {
            CTime current_time(GetFastLocalTime());
            if (current_time >= m_ThrottledUntil) {
                if (!m_Service->m_ThrottleUntilDiscoverable) {
                    ResetThrottlingParameters();
                    return;
                } else if (m_Service->m_ForceRebalanceAfterThrottleWithin > 0) {
                    CTime discovery_validity_time(m_Service->
                        m_RebalanceStrategy->GetLastRebalanceTime());

                    discovery_validity_time.AddSecond(m_Service->
                        m_ForceRebalanceAfterThrottleWithin);

                    if (discovery_validity_time < current_time) {
                        // Force service discovery and rebalancing.
                        CFastMutexGuard discovery_mutex_lock(
                            m_Service->m_DiscoveryMutex);
                        ITERATE(TActualServiceDiscoveryIteration, it,
                                m_ActualServiceDiscoveryIteration) {
                            // Force discovery:
                            ++it->first->m_LatestDiscoveryIteration;
                            m_Service->DiscoverServersIfNeeded(it->first);
                            if (it->first->m_LatestDiscoveryIteration ==
                                    it->second) {
                                ResetThrottlingParameters();
                                return;
                            }
                        }
                    }
                } else {
                    CFastMutexGuard discovery_mutex_lock(
                        m_Service->m_DiscoveryMutex);
                    ITERATE(TActualServiceDiscoveryIteration, it,
                            m_ActualServiceDiscoveryIteration) {
                        if (it->first->m_LatestDiscoveryIteration ==
                                it->second) {
                            ResetThrottlingParameters();
                            return;
                        }
                    }
                }
            }
            NCBI_THROW(CNetSrvConnException, eServerThrottle,
                m_ThrottleMessage);
        }

        if (m_Service->m_MaxSubsequentConnectionFailures > 0 &&
            m_NumberOfSuccessiveFailures >=
                m_Service->m_MaxSubsequentConnectionFailures) {
            m_Throttled = true;
            m_ThrottleMessage = "Server " + m_Address.AsString();
            m_ThrottleMessage += " has reached the maximum number "
                "of connection failures in a row";
        }

        if (m_Service->m_ReconnectionFailureThresholdNumerator > 0 &&
            m_ConnectionFailureCounter >=
                m_Service->m_ReconnectionFailureThresholdNumerator) {
            m_Throttled = true;
            m_ThrottleMessage = "Connection to server " + m_Address.AsString();
            m_ThrottleMessage += " aborted as it is considered bad/overloaded";
        }

        if (m_Throttled) {
            m_ThrottledUntil.SetCurrent();
            m_ThrottledUntil.AddSecond(m_Service->m_ServerThrottlePeriod);
            NCBI_THROW(CNetSrvConnException, eServerThrottle,
                m_ThrottleMessage);
        }
    }
}

void SNetServerImpl::ResetThrottlingParameters()
{
    m_NumberOfSuccessiveFailures = 0;
    memset(m_ConnectionFailureRegister, 0, sizeof(m_ConnectionFailureRegister));
    m_ConnectionFailureCounter = m_ConnectionFailureRegisterIndex = 0;
    m_Throttled = false;
}

CNetServer::SExecResult CNetServer::ExecWithRetry(const string& cmd)
{
    CNetServer::SExecResult exec_result;

    CTime max_query_time(GetFastLocalTime());
    max_query_time.AddNanoSecond(m_Impl->m_Service->m_MaxQueryTime * 1000000);

    unsigned attempt = 0;

    for (;;) {
        try {
            m_Impl->ConnectAndExec(cmd, exec_result);
            return exec_result;
        }
        catch (CNetSrvConnException& e) {
            if (++attempt > TServConn_ConnMaxRetries::GetDefault() ||
                    e.GetErrCode() == CNetSrvConnException::eServerThrottle)
                throw;

            if (m_Impl->m_Service->m_MaxQueryTime > 0 &&
                    max_query_time <= GetFastLocalTime()) {
                LOG_POST(Error << "Timeout (max_query_time=" <<
                    m_Impl->m_Service->m_MaxQueryTime <<
                    "); cmd=" << cmd <<
                    "; exception=" << e.GetMsg());
                throw;
            }

            LOG_POST(Warning << e.what() << ", reconnecting: attempt " <<
                attempt << " of " << TServConn_ConnMaxRetries::GetDefault());

            SleepMilliSec(s_GetRetryDelay());
        }
    }
}

IReader* CNetServer::ExecRead(const string& cmd, string* response)
{
    SExecResult exec_result(ExecWithRetry(cmd));

    if (response != NULL)
        *response = exec_result.response;

    return new CNetServerReader(exec_result);
}

IEmbeddedStreamWriter* CNetServer::ExecWrite(
    const string& cmd, string* response)
{
    SExecResult exec_result(ExecWithRetry(cmd));

    if (response != NULL)
        *response = exec_result.response;

    return new CNetServerWriter(exec_result);
}

END_NCBI_SCOPE
