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

#include "srv_connections_impl.hpp"

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

bool CNetServerCmdOutput::ReadLine(std::string& output)
{
    _ASSERT(!m_Impl->m_ReadCompletely);

    if (!m_Impl->m_NetCacheCompatMode)
        output = m_Impl->m_Connection->ReadCmdOutputLine();
    else {
        try {
            output = m_Impl->m_Connection->ReadCmdOutputLine();
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
    SNetServerConnectionPool* pool) :
        m_ConnectionPool(pool),
        m_NextFree(NULL)
{
    if (TServConn_UserLinger2::GetDefault()) {
        STimeout zero = {0,0};
        m_Socket.SetTimeout(eIO_Close,&zero);
    }
}

void SNetServerConnectionImpl::Delete()
{
    m_ConnectionPool->Put(this); // may result in 'delete this'
}

std::string SNetServerConnectionImpl::ReadCmdOutputLine()
{
    string result;

    EIO_Status io_st = m_Socket.ReadLine(result);

    switch (io_st)
    {
    case eIO_Success:
        return result;
    case eIO_Timeout:
        Abort();
        NCBI_THROW(CNetSrvConnException, eReadTimeout,
            "Communication timeout reading from " +
            m_ConnectionPool->GetAddressAsString());
        break;
    default: // invalid socket or request, bailing out
        Abort();
        NCBI_THROW(CNetServiceException, eCommunicationError,
            "Communication error reading from " +
            m_ConnectionPool->GetAddressAsString());
    }

    if (NStr::StartsWith(result, "OK:")) {
        result.erase(0, sizeof("OK:") - 1);
    } else if (NStr::StartsWith(result, "ERR:")) {
        result.erase(0, sizeof("ERR:") - 1);
        result = NStr::ParseEscapes(result);
        m_ConnectionPool->m_EventListener->OnError(result, m_ConnectionPool);
    }
    return result;
}

bool SNetServerConnectionImpl::IsConnected()
{
    EIO_Status st = m_Socket.GetStatus(eIO_Open);
    if (st != eIO_Success)
        return false;

    STimeout zero = {0, 0}, tmo;
    const STimeout* tmp = m_Socket.GetTimeout(eIO_Read);
    if (tmp) {
        tmo = *tmp;
        tmp = &tmo;
    }
    if (m_Socket.SetTimeout(eIO_Read, &zero) != eIO_Success)
        return false;

    st = m_Socket.Read(0,1,0,eIO_ReadPeek);
    bool ret = false;
    switch (st) {
         case eIO_Closed:
            m_Socket.Close();
            return false;
         case eIO_Success:
            if (m_Socket.GetStatus(eIO_Read) != eIO_Success) {
                m_Socket.Close();
                break;
            }
         case eIO_Timeout:
            ret = true;
         default:
            break;
    }

    if (m_Socket.SetTimeout(eIO_Read, tmp) != eIO_Success)
        return false;

    return ret;
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

inline void SNetServerConnectionImpl::WriteLine(const string& line)
{
    // TODO change to "\n" when no old NS/NC servers remain.
    std::string str(line + "\r\n");

    const char* buf = str.data();
    size_t len = str.size();

    CheckConnect();

    while (len > 0) {
        size_t n_written;

        EIO_Status io_st = m_Socket.Write(buf, len, &n_written);

        if (io_st != eIO_Success) {
            Abort();

            CIO_Exception io_ex(DIAG_COMPILE_INFO, 0,
                (CIO_Exception::EErrCode) io_st, "IO error.");

            NCBI_THROW(CNetSrvConnException, eWriteFailure,
                "Failed to write to " +
                m_ConnectionPool->GetAddressAsString() +
                ": " + string(io_ex.what()));
        }
        len -= n_written;
        buf += n_written;
    }
}

// if wait_sec is set to 0 m_Timeout will be used
void SNetServerConnectionImpl::WaitForServer(unsigned int wait_sec)
{
    STimeout to = {wait_sec, 0};

    SNetServerConnectionPool* pool = m_ConnectionPool;

    if (wait_sec == 0)
        to = pool->m_Timeout;

    EIO_Status io_st = m_Socket.Wait(eIO_Read, &to);

    if (io_st == eIO_Timeout) {
        Abort();

        NCBI_THROW(CNetSrvConnException, eResponseTimeout,
            "No response from " + pool->GetAddressAsString());
    }
}

std::string CNetServerConnection::Exec(const string& cmd)
{
    m_Impl->WriteLine(cmd);
    m_Impl->WaitForServer();

    return m_Impl->ReadCmdOutputLine();
}

CNetServerCmdOutput CNetServerConnection::ExecMultiline(const std::string& cmd)
{
    m_Impl->WriteLine(cmd);
    m_Impl->WaitForServer();

    return new SNetServerCmdOutputImpl(m_Impl);
}

void SNetServerConnectionImpl::CheckConnect()
{
    if (IsConnected())
        return;

    SNetServerConnectionPool* pool = m_ConnectionPool;

    unsigned conn_repeats = 0;

    EIO_Status io_st;

    while ((io_st = m_Socket.Connect(pool->m_Host, pool->m_Port,
        &pool->m_Timeout, eOn)) != eIO_Success) {

        if (io_st == eIO_Unknown) {

            m_Socket.Close();

            // most likely this is an indication we have too many
            // open ports on the client side
            // (this kernel limitation manifests itself on Linux)
            if (++conn_repeats > pool->m_MaxRetries) {
                if (io_st != eIO_Success) {
                    CIO_Exception io_ex(DIAG_COMPILE_INFO,
                        0, (CIO_Exception::EErrCode)io_st, "IO error.");
                    NCBI_THROW(CNetSrvConnException, eConnectionFailure,
                        "Failed to connect to " + pool->GetAddressAsString() +
                            ": " + string(io_ex.what()));
                }
            }
            // give system a chance to recover

            SleepMilliSec(1000 * conn_repeats);
        } else {
            CIO_Exception io_ex(DIAG_COMPILE_INFO,  0, (CIO_Exception::EErrCode)io_st,  "IO error.");
            NCBI_THROW(CNetSrvConnException, eConnectionFailure,
                "Failed to connect to " + pool->GetAddressAsString() +
                ": " + string(io_ex.what()));
        }
    }

    m_Socket.SetDataLogging(eDefault);
    m_Socket.SetTimeout(eIO_ReadWrite, &pool->m_Timeout);
    m_Socket.DisableOSSendDelay();
    m_Socket.SetReuseAddress(eOn);

    if (pool->m_EventListener)
        pool->m_EventListener->OnConnected(this);
}

const string& CNetServerConnection::GetHost() const
{
    return m_Impl->m_ConnectionPool->m_Host;
}

unsigned int CNetServerConnection::GetPort() const
{
    return m_Impl->m_ConnectionPool->m_Port;
}


/*************************************************************************/
SNetServerConnectionPool::SNetServerConnectionPool(
    const string& host,
    unsigned short port,
    const STimeout& timeout,
    INetServerConnectionListener* listener) :
        m_Host(host),
        m_Port(port),
        m_Timeout(timeout),
        m_EventListener(listener),
        m_MaxRetries(TServConn_ConnMaxRetries::GetDefault()),
        m_FreeConnectionListHead(NULL),
        m_FreeConnectionListSize(0),
        m_PermanentConnection(eDefault)
{
}

inline void SNetServerConnectionPool::DeleteConnection(
    SNetServerConnectionImpl* impl)
{
    delete impl;
}

void SNetServerConnectionPool::Put(SNetServerConnectionImpl* impl)
{
    if (m_PermanentConnection != eOn)
        impl->Close();
    else
        if (impl->m_Socket.GetStatus(eIO_Open) == eIO_Success) {
            // we need to check if all data has been read.
            // if it has not then we have to close the connection
            STimeout to = {0, 0};

            if (impl->m_Socket.Wait(eIO_Read, &to) == eIO_Success) {
                impl->Abort();
                ERR_POST_X(8,
                    "Socket not fully read while returning to the pool");
            } else {
                TFastMutexGuard guard(m_FreeConnectionListLock);

                int upper_limit = TServConn_MaxConnPoolSize::GetDefault();

                if (upper_limit == 0 ||
                        m_FreeConnectionListSize < upper_limit) {
                    impl->m_NextFree = m_FreeConnectionListHead;
                    m_FreeConnectionListHead = impl;
                    ++m_FreeConnectionListSize;
                    impl->m_ConnectionPool = NULL;
                    return;
                }
            }
        }

    DeleteConnection(impl);
}

std::string SNetServerConnectionPool::GetAddressAsString() const
{
    std::string address =
        CSocketAPI::gethostbyaddr(CSocketAPI::gethostbyname(m_Host));

    address += ':';
    address += NStr::UIntToString(m_Port);

    return address;
}

SNetServerConnectionPool::~SNetServerConnectionPool()
{
    // No need to lock the mutex in the destructor.
    SNetServerConnectionImpl* impl = m_FreeConnectionListHead;
    while (impl != NULL) {
        SNetServerConnectionImpl* next_impl = impl->m_NextFree;
        DeleteConnection(impl);
        impl = next_impl;
    }
}

CNetServerConnection SNetServerConnectionPool::GetConnection()
{
    TFastMutexGuard guard(m_FreeConnectionListLock);

    SNetServerConnectionImpl* impl;

    if (m_FreeConnectionListSize == 0)
        impl = new SNetServerConnectionImpl(this);
    else {
        impl = m_FreeConnectionListHead;
        m_FreeConnectionListHead = impl->m_NextFree;
        --m_FreeConnectionListSize;
        impl->m_ConnectionPool = this;
    }

    return impl;
}

END_NCBI_SCOPE
