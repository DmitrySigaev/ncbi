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

#include "netservice_api_impl.hpp"

#include <connect/services/srv_connections_expt.hpp>
#include <connect/services/netservice_api_expt.hpp>
#include <connect/services/netservice_params.hpp>
#include <connect/services/error_codes.hpp>

#include <connect/ncbi_conn_exception.hpp>

#include <corelib/ncbi_system.hpp>


#define NCBI_USE_ERRCODE_X   ConnServ_Connection


BEGIN_NCBI_SCOPE

///////////////////////////////////////////////////////////////////////////
SNetServerCmdOutputImpl::~SNetServerCmdOutputImpl()
{
    if (!m_ReadCompletely) {
        m_Connection->Close();
        ERR_POST_X(7, "Multiline command output was not completely read");
    }
}

bool CNetServerCmdOutput::ReadLine(string& output)
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
        catch (CNetServiceException& e) {
            if (e.GetErrCode() != CNetServiceException::eCommunicationError)
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
    if (TServConn_UserLinger2::GetDefault()) {
        STimeout zero = {0,0};
        m_Socket.SetTimeout(eIO_Close,&zero);
    }
}

void SNetServerConnectionImpl::Delete()
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
            m_Server->GetAddressAsString());
        break;
    default: // invalid socket or request, bailing out
        Abort();
        NCBI_THROW(CNetServiceException, eCommunicationError,
            "Communication error reading from " +
            m_Server->GetAddressAsString());
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
                "Failed to write to " + m_Server->GetAddressAsString() +
                ": " + string(io_ex.what()));
        }
        len -= n_written;
        buf += n_written;
    }
}

void SNetServerConnectionImpl::WaitForServer()
{
    EIO_Status io_st = m_Socket.Wait(eIO_Read, &m_Server->m_Service->m_Timeout);

    if (io_st == eIO_Timeout) {
        Abort();

        NCBI_THROW(CNetSrvConnException, eResponseTimeout,
            "No response from " + m_Server->GetAddressAsString());
    }
}

string CNetServerConnection::Exec(const string& cmd)
{
    m_Impl->WriteLine(cmd);
    m_Impl->WaitForServer();

    string output;
    m_Impl->ReadCmdOutputLine(output);

    return output;
}

CNetServerCmdOutput CNetServerConnection::ExecMultiline(const string& cmd)
{
    return new SNetServerCmdOutputImpl(m_Impl, Exec(cmd));
}

const string& CNetServerConnection::GetHost() const
{
    return m_Impl->m_Server->m_Host;
}

unsigned int CNetServerConnection::GetPort() const
{
    return m_Impl->m_Server->m_Port;
}


/*************************************************************************/
SNetServerImplReal::SNetServerImplReal(const string& host,
    unsigned short port) : SNetServerImpl(host, port)
{
    m_FreeConnectionListHead = NULL;
    m_FreeConnectionListSize = 0;
}

void SNetServerImpl::Delete()
{
    // Before resetting the m_Service pointer, verify that nobody
    // has acquired a reference to this server object yet (between
    // the time the reference counter went to zero, and the
    // current moment when m_Service is about to be reset).

    CFastMutexGuard g(m_Service->m_ConnectionMutex);

    if (m_Refs.Get() == 0)
        m_Service = NULL;
}

string SNetServerImpl::GetAddressAsString() const
{
    string address =
        CSocketAPI::gethostbyaddr(CSocketAPI::gethostbyname(m_Host));

    address += ':';
    address += NStr::UIntToString(m_Port);

    return address;
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
    return m_Impl->m_Host;
}

unsigned short CNetServer::GetPort() const
{
    return m_Impl->m_Port;
}

CNetServerConnection CNetServer::Connect()
{
    CNetServerConnection conn;

    {{
        TFastMutexGuard guard(m_Impl->m_FreeConnectionListLock);

        if (m_Impl->m_FreeConnectionListSize > 0) {
            // Get an existing connection object from the connection pool.
            conn = m_Impl->m_FreeConnectionListHead;

            m_Impl->m_FreeConnectionListHead = conn->m_NextFree;
            --m_Impl->m_FreeConnectionListSize;
            conn->m_Server = m_Impl;
        } else {
            // Pool is empty; create a new connection.
            guard.Release();

            conn = new SNetServerConnectionImpl(m_Impl);
        }
    }}

    CSocket& conn_socket = conn->m_Socket;

    // Check if the socket is already connected.
    if (conn_socket.GetStatus(eIO_Open) == eIO_Success) {
        STimeout old_timeout;

        const STimeout* old_timeout_ptr = conn_socket.GetTimeout(eIO_Read);

        if (old_timeout_ptr != NULL) {
            old_timeout = *old_timeout_ptr;
            old_timeout_ptr = &old_timeout;
        }

        static const STimeout zero_timeout = {0, 0};

        if (conn_socket.SetTimeout(eIO_Read, &zero_timeout) == eIO_Success) {
            switch (conn_socket.Read(NULL, 1, NULL)) {
            case eIO_Success:
                ERR_POST_X(8, "Protocol error: a socket from the pool "
                    "still has input data in it, reconnecting");
                break;

            case eIO_Timeout:
                if (conn_socket.SetTimeout(eIO_Read,
                        old_timeout_ptr) == eIO_Success)
                    return conn;
                /* FALL THROUGH */

            default:
                break;
            }
        }

        conn_socket.Close();
    }

    // The socket must be closed at this point.
    _ASSERT(conn_socket.GetStatus(eIO_Open) != eIO_Success);

    unsigned attempt = 0;

    EIO_Status io_st;

    while ((io_st = conn_socket.Connect(m_Impl->m_Host, m_Impl->m_Port,
        &m_Impl->m_Service->m_Timeout, eOn)) != eIO_Success) {
        conn_socket.Close();

        string message = "Could not connect to " +
            m_Impl->GetAddressAsString();
        message += ": ";
        message += IO_StatusStr(io_st);

        if (++attempt > TServConn_ConnMaxRetries::GetDefault()) {
            NCBI_THROW(CNetSrvConnException, eConnectionFailure, message);
        }

        LOG_POST(Warning << message << ", reconnecting: attempt " <<
            attempt << " of " << TServConn_ConnMaxRetries::GetDefault());

        SleepMilliSec(TServConn_RetryDelay::GetDefault());
    }

    conn_socket.SetDataLogging(eDefault);
    conn_socket.SetTimeout(eIO_ReadWrite, &m_Impl->m_Service->m_Timeout);
    conn_socket.DisableOSSendDelay();
    conn_socket.SetReuseAddress(eOn);

    m_Impl->m_Service->m_Listener->OnConnected(conn);

    return conn;
}

END_NCBI_SCOPE
