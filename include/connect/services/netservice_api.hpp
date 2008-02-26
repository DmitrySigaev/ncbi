#ifndef CONNECT_SERVICES___NETSERVICE_API__HPP
#define CONNECT_SERVICES___NETSERVICE_API__HPP

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
 * Authors:  Maxim Didenko
 *
 * File Description:
 *   Network client for ICache (NetCache).
 *
 */

#include <connect/services/srv_discovery.hpp>

BEGIN_NCBI_SCOPE

class CNetSrvAuthenticator;

class NCBI_XCONNECT_EXPORT CNetServiceAPI_Base
{
public:
    CNetServiceAPI_Base(const string& service_name, const string& client_name);
    virtual ~CNetServiceAPI_Base();

    typedef pair<string, unsigned short> TServerAddress;
    typedef map<TServerAddress,
        CNetServerConnectionPool*> TServerAddressToConnectionPool;
    typedef vector<TServerAddress> TDiscoveredServers;

    CNetServerConnection GetBest(const TServerAddress* backup = NULL,
        const string& hit = "");
    CNetServerConnection GetSpecific(const string& host, unsigned int port);

    void SetCommunicationTimeout(const STimeout& to);
    const STimeout& GetCommunicationTimeout() const;

    void SetCreateSocketMaxRetries(unsigned int retries);
    unsigned int GetCreateSocketMaxRetries() const;

    void PermanentConnection(ESwitch type);

    bool IsLoadBalanced() const { return m_IsLoadBalanced; }

    template<class Pred>
    bool Find(Pred func) {
        x_Rebalance();
        CReadLockGuard g(m_ServersLock);
        TDiscoveredServers servers_copy(m_Servers);
        g.Release();
        // pick a random pivot element, so we do not always
        // fetch jobs using the same lookup order and some servers do
        // not get equally "milked"
        // also get random list lookup direction
        unsigned servers_size = servers_copy.size();
        unsigned pivot = rand() % servers_size;
        unsigned pivot_plus_servers_size = pivot + servers_size;

        for (unsigned i = pivot; i < pivot_plus_servers_size; ++i) {
            unsigned j = i % servers_size;
            const TServerAddress& srv = servers_copy[j];

            CNetServerConnection conn =
                x_FindOrCreateConnectionPool(srv).GetConnection();

            if (func(conn, i == pivot_plus_servers_size - 1))
                return true;
        }
        return false;
    }

    template<class Func>
    Func ForEach(Func func) {
        x_Rebalance();
        CReadLockGuard g(m_ServersLock);
        TDiscoveredServers servers_copy(m_Servers);
        g.Release();
        NON_CONST_ITERATE(TDiscoveredServers, it, servers_copy) {
            CNetServerConnection conn =
                x_FindOrCreateConnectionPool(*it).GetConnection();

            func(conn);
        }
        return func;
    }

    void DiscoverLowPriorityServers(ESwitch on_off);

    /// Connection management options
    enum EConnectionMode {
        /// Close connection after each call (default).
        /// This mode frees server side resources, but reconnection can be
        /// costly because of the network overhead
        eCloseConnection,

        /// Keep connection open.
        /// This mode occupies server side resources(session thread),
        /// use this mode very carefully
        eKeepConnection
    };
    /// Returns current connection mode
    /// @sa SetConnMode
    EConnectionMode GetConnMode() const;

    /// Set connection mode
    /// @sa GetConnMode
    void SetConnMode(EConnectionMode conn_mode);

    const string& GetClientName() const { return m_ClientName; }
    const string& GetServiceName() const { return m_ServiceName; }

    void SetRebalanceStrategy( IRebalanceStrategy* strategy,
                               EOwnership owner = eTakeOwnership);

    string WaitResponse(CNetServerConnection conn) const;
    string SendCmdWaitResponse(CNetServerConnection conn,
        const string& cmd) const;

    //protected:
    void CheckServerOK(string& response) const;

    //protected:
    static void TrimErr(string& err_msg);


    class ISink
    {
    public:
        virtual ~ISink() {};
        virtual CNcbiOstream& GetOstream(CNetServerConnection conn) = 0;
        virtual void EndOfData(CNetServerConnection /* conn */) = 0;
    };
    enum EStreamCollectorType {
        eSendCmdWaitResponse,
        ePrintServerOut
    };
    void x_CollectStreamOutput(const string& cmd, ISink& sink, EStreamCollectorType type);
    void PrintServerOut(CNetServerConnection conn,
        CNcbiOstream& out) const;

protected:
    enum ETrimErr {
        eNoTrimErr,
        eTrimErr
    };

    virtual void ProcessServerError(string& response, ETrimErr trim_err) const;

    string GetConnectionInfo(CNetServerConnection& conn) const;


private:
    friend class CNetServiceAuthenticator;
    void DoAuthenticate(CNetServerConnection& conn) const {
        x_SendAuthetication(conn);
    }
    virtual void x_SendAuthetication(CNetServerConnection& conn) const = 0;

    string m_ServiceName;
    string m_ClientName;

    auto_ptr<INetServerConnectionListener> m_Authenticator;
    EConnectionMode m_ConnMode;
    IRebalanceStrategy* m_RebalanceStrategy;
    auto_ptr<IRebalanceStrategy> m_RebalanceStrategyGuard;

    CNetServiceAPI_Base(const CNetServiceAPI_Base&);
    CNetServiceAPI_Base& operator=(const CNetServiceAPI_Base&);

    TDiscoveredServers m_Servers;
    CRWLock m_ServersLock;
    mutable TServerAddressToConnectionPool m_ServerAddressToConnectionPool;
    mutable CFastMutex m_ConnectionMutex;

    bool    m_IsLoadBalanced;
    ESwitch m_DiscoverLowPriorityServers;

    STimeout                          m_Timeout;
    unsigned int                      m_MaxRetries;
    ESwitch                           m_PermanentConnection;

    CNetServerConnectionPool&
        x_FindOrCreateConnectionPool(const TServerAddress& srv) const;

    void x_Rebalance();
};


END_NCBI_SCOPE

#endif  /* CONNECT_SERVICES___NETSERVICE_API__HPP */
