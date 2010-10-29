#ifndef CONNECT_SERVICES___SRV_CONNECTIONS_IMPL__HPP
#define CONNECT_SERVICES___SRV_CONNECTIONS_IMPL__HPP

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
 * Authors:  Dmitry Kazimirov
 *
 * File Description:
 *
 */

#include "netservice_params.hpp"

#include <connect/services/netservice_api.hpp>

#include <corelib/ncbimtx.hpp>

BEGIN_NCBI_SCOPE

struct SNetServerMultilineCmdOutputImpl : public CObject
{
    SNetServerMultilineCmdOutputImpl(
        const CNetServer::SExecResult& exec_result) :
            m_Connection(exec_result.conn),
            m_FirstOutputLine(exec_result.response),
            m_FirstLineConsumed(false),
            m_NetCacheCompatMode(false),
            m_ReadCompletely(false)
    {
    }

    virtual ~SNetServerMultilineCmdOutputImpl();

    void SetNetCacheCompatMode() { m_NetCacheCompatMode = true; }

    CNetServerConnection m_Connection;

    string m_FirstOutputLine;
    bool m_FirstLineConsumed;

    bool m_NetCacheCompatMode;
    bool m_ReadCompletely;
};

struct SNetServerImpl;

struct SNetServerConnectionImpl : public CObject
{
    SNetServerConnectionImpl(SNetServerImpl* pool);

    // Return this connection to the pool.
    virtual void DeleteThis();

    void WriteLine(const string& line);
    void ReadCmdOutputLine(string& result);

    void Close();
    void Abort();

    virtual ~SNetServerConnectionImpl();

    // The server this connection is connected to.
    CNetServer m_Server;
    SNetServerConnectionImpl* m_NextFree;

    CSocket m_Socket;
};


class INetServerConnectionListener : public CObject
{
public:
    virtual void OnInit(CObject* api_impl,
        CConfig* config, const string& config_section) = 0;
    virtual void OnConnected(CNetServerConnection::TInstance conn) = 0;
    virtual void OnError(const string& err_msg, SNetServerImpl* server) = 0;
};


struct SNetServerImpl : public CObject
{
    // Special constructor for making search images
    // for searching in TNetServerSet.
    SNetServerImpl(const string& host, unsigned short port) :
        m_Address(host, port)
    {
    }

    // Releases a reference to the parent service object,
    // and if that was the last reference, the service object
    // will be deleted, which in turn will lead to this server
    // object being deleted too (along with all other server
    // objects that the parent service object may contain).
    virtual void DeleteThis();

    CNetServerConnection GetConnectionFromPool();
    CNetServerConnection Connect();
    void ConnectAndExec(const string& cmd,
        CNetServer::SExecResult& exec_result);

    void RegisterConnectionEvent(bool failure);
    void CheckIfThrottled();
    void ResetThrottlingParameters();

    // A smart pointer to the NetService object
    // that contains this NetServer.
    CNetService m_Service;

    SServerAddress m_Address;

    unsigned m_DiscoveryIteration;

    SNetServerConnectionImpl* m_FreeConnectionListHead;
    int m_FreeConnectionListSize;
    CFastMutex m_FreeConnectionListLock;

    int m_NumberOfSuccessiveFailures;
    bool m_ConnectionFailureRegister[CONNECTION_ERROR_HISTORY_MAX];
    int m_ConnectionFailureRegisterIndex;
    int m_ConnectionFailureCounter;
    bool m_Throttled;
    string m_ThrottleMessage;
    CTime m_ThrottledUntil;
    CFastMutex m_ThrottleLock;
};

struct SNetServerImplReal : public SNetServerImpl
{
    SNetServerImplReal(const string& host, unsigned short port);

    virtual ~SNetServerImplReal();
};

END_NCBI_SCOPE

#endif  /* CONNECT_SERVICES___SRV_CONNECTIONS_IMPL__HPP */
