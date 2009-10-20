#ifndef NETCACHED__HPP
#define NETCACHED__HPP

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
 * File Description: Network cache daemon
 *
 */

#include <connect/services/netcache_api_expt.hpp>

#include <connect/server.hpp>
#include <connect/server_monitor.hpp>

#include <corelib/ncbimtx.hpp>
#include <corelib/ncbi_config.hpp>

#include "smng_thread.hpp"
#include "nc_storage.hpp"
#include "nc_utils.hpp"


BEGIN_NCBI_SCOPE


class CNCServerStat;
class CNetCacheDApp;
class CNCMessageHandler;


struct SConstCharCompare
{
    bool operator() (const char* left, const char* right) const;
};


/// Helper class to maintain different instances of server statistics in
/// different threads.
class CNCServerStat_Getter : public CNCTlsObject<CNCServerStat_Getter,
                                                 CNCServerStat>
{
    typedef CNCTlsObject<CNCServerStat_Getter, CNCServerStat>  TBase;

public:
    CNCServerStat_Getter(void);
    ~CNCServerStat_Getter(void);

    /// Part of interface required by CNCTlsObject<>
    CNCServerStat* CreateTlsObject(void);
    static void DeleteTlsObject(void* obj_ptr);
};

/// Class collecting statistics about whole server.
class CNCServerStat
{
public:
    /// Add finished command
    ///
    /// @param cmd
    ///   Command name - should be string constant living through all time
    ///   application is running.
    /// @param cmd_span
    ///   Time command was executed
    /// @param state_spans
    ///   Array of times spent in each handler state during command execution
    static void AddFinishedCmd(const char*           cmd,
                               double                cmd_span,
                               const vector<double>& state_spans);
    /// Add closed connection
    ///
    /// @param conn_span
    ///   Time which connection stayed opened
    static void AddClosedConnection(double conn_span);
    /// Add opened connection
    static void AddOpenedConnection(void);
    /// Remove opened connection (after OnOverflow was called)
    static void RemoveOpenedConnection(void);
    /// Add connection that will be closed because of maximum number of
    /// connections exceeded.
    static void AddOverflowConnection(void);
    /// Add command terminated because of timeout
    static void AddTimedOutCommand(void);

    /// Print statistics
    static void Print(CPrintTextProxy& proxy);

private:
    friend class CNCServerStat_Getter;

    typedef map<const char*, Uint8,  SConstCharCompare>  TCmdsCountsMap;
    typedef map<const char*, double, SConstCharCompare>  TCmdsSpansMap;


    CNCServerStat(void);
    /// Collect all statistics from this instance to another
    void x_CollectTo(CNCServerStat* other);


    /// Mutex for working with statistics
    CSpinLock      m_ObjLock;
    /// Maximum time connection was opened
    double         m_MaxConnSpan;
    /// Number of connections closed
    Uint8          m_ClosedConns;
    /// Number of connections opened
    Uint8          m_OpenedConns;
    /// Number of connections closed because of maximum number of opened
    /// connections limit.
    Uint8          m_OverflowConns;
    /// Sum of times all connections stayed opened
    double         m_ConnsSpansSum;
    /// Maximum time one command was executed
    double         m_MaxCmdSpan;
    /// Maximum time of each command type executed
    TCmdsSpansMap  m_MaxCmdSpanByCmd;
    /// Total number of executed commands
    Uint8          m_CntCmds;
    /// Number of each command type executed
    TCmdsCountsMap m_CntCmdsByCmd;
    /// Sum of times all commands were executed
    double         m_CmdSpans;
    /// Sum of times each type of command was executed
    TCmdsSpansMap  m_CmdSpansByCmd;
    /// Sums of times handlers spent in every state
    vector<double> m_StatesSpansSums;
    /// Number of commands terminated because of command timeout
    Uint8          m_TimedOutCmds;

    /// Object differentiating statistics instances over threads
    static CNCServerStat_Getter sm_Getter;
    /// All instances of statistics used in application
    static CNCServerStat        sm_Instances[kNCMaxThreadsCnt];
};

/// Netcache server 
class CNetCacheServer : public CServer
{
public:
    /// Create server
    ///
    /// @param do_reinit
    ///   TRUE if reinitialization should be forced (was requested in command
    ///   line)
    CNetCacheServer(bool do_reinit);
    virtual ~CNetCacheServer();

    /// Check if server was asked to stop
    virtual bool ShutdownRequested(void);
    /// Get signal number by which server was asked to stop
    int GetSignalCode(void) const;
    /// Ask server to stop after receiving given signal
    void RequestShutdown(int signal = 0);
    
    // Check if session management turned on
    bool IsManagingSessions(void) const;
    /// Register new session
    ///
    /// @return
    ///   TRUE if session has been registered,
    ///   FALSE if session management cannot do it (shutdown)
    bool RegisterSession  (const string& host, unsigned int pid);
    /// Unregister session
    void UnregisterSession(const string& host, unsigned int pid);

    /// Get monitor for the server
    CServer_Monitor* GetServerMonitor(void);
    /// Get storage for the given cache name.
    /// If cache name is empty then it should be main storage for NetCache,
    /// otherwise it's storage for ICache implementation
    CNCBlobStorage* GetBlobStorage(const string& cache_name);
    /// Get next blob id to incorporate it into generated blob key
    int GetNextBlobId(void);
    /// Get configuration registry for the server
    const IRegistry& GetRegistry(void);
    /// Get server host
    const string& GetHost(void) const;
    /// Get server port
    unsigned int GetPort(void) const;
    /// Get inactivity timeout for each connection
    unsigned GetInactivityTimeout(void) const;
    /// Get timeout for each executed command
    unsigned GetCmdTimeout(void) const;
    /// Create CTime object in fast way (via CFastTime)
    CTime GetFastTime(void);

    /// Print full server statistics into stream
    void PrintServerStats(CNcbiIostream* ios);

private:
    /// Read integer configuration value from server's registry
    int    x_RegReadInt   (const IRegistry& reg,
                           const char*      value_name,
                           int              def_value);
    /// Read boolean configuration value from server's registry
    bool   x_RegReadBool  (const IRegistry& reg,
                           const char*      value_name,
                           bool             def_value);
    /// Read string configuration value from server's registry
    string x_RegReadString(const IRegistry& reg,
                           const char*      value_name,
                           const string&    def_value);

    /// Create all blob storage instances
    ///
    /// @param reg
    ///   Registry to read configuration for storages
    /// @param do_reinit
    ///   Flag if all storages should be forced to reinitialize
    void x_CreateStorages(const IRegistry& reg, bool do_reinit);

    /// Start session management thread
    void x_StartSessionManagement(unsigned int shutdown_timeout);
    /// Stop session management thread
    void x_StopSessionManagement (void);

    /// Print full server statistics into stream or diagnostics
    void x_PrintServerStats(CPrintTextProxy& proxy);


    typedef map<string, AutoPtr<CNCBlobStorage> >     TStorageMap;


    /// Host name where server runs
    string                         m_Host;
    /// Port where server runs
    unsigned                       m_Port;
    /// Time when this server instance was started
    CTime                          m_StartTime;
    // Some variable that should be here because of CServer requirements
    STimeout                       m_ServerAcceptTimeout;
    /// Flag that server received a shutdown request
    bool                           m_Shutdown;
    /// Signal which caused the shutdown request
    int                            m_Signal;
    /// Time to wait for the client on the connection (seconds)
    unsigned                       m_InactivityTimeout;
    /// Maximum time span which each command can work in
    unsigned                       m_CmdTimeout;
    /// Quick local timer
    CFastLocalTime                 m_FastTime;
    /// Map of strings to blob storages
    TStorageMap                    m_StorageMap;
    /// Counter for blob id
    CAtomicCounter                 m_BlobIdCounter;
    /// Session management thread
    CRef<CSessionManagementThread> m_SessionMngThread;
    /// Server monitor
    CServer_Monitor                m_Monitor;
};


/// NetCache daemon application
class CNetCacheDApp : public CNcbiApplication
{
protected:
    virtual void Init(void);
    virtual int  Run (void);
};



inline bool
SConstCharCompare::operator() (const char* left, const char* right) const
{
    return NStr::strcmp(left, right) < 0;
}


inline
CNCServerStat_Getter::CNCServerStat_Getter(void)
{
    TBase::Initialize();
}

inline
CNCServerStat_Getter::~CNCServerStat_Getter(void)
{
    TBase::Finalize();
}

inline CNCServerStat*
CNCServerStat_Getter::CreateTlsObject(void)
{
    return &CNCServerStat::sm_Instances[
                                     g_GetNCThreadIndex() % kNCMaxThreadsCnt];
}

inline void
CNCServerStat_Getter::DeleteTlsObject(void*)
{}


inline void
CNCServerStat::AddClosedConnection(double conn_span)
{
    CNCServerStat* stat = sm_Getter.GetObjPtr();
    stat->m_ObjLock.Lock();
    ++stat->m_ClosedConns;
    stat->m_ConnsSpansSum += conn_span;
    stat->m_MaxConnSpan    = max(stat->m_MaxConnSpan, conn_span);
    stat->m_ObjLock.Unlock();
}

inline void
CNCServerStat::AddOpenedConnection(void)
{
    CNCServerStat* stat = sm_Getter.GetObjPtr();
    stat->m_ObjLock.Lock();
    ++stat->m_OpenedConns;
    stat->m_ObjLock.Unlock();
}

inline void
CNCServerStat::RemoveOpenedConnection(void)
{
    CNCServerStat* stat = sm_Getter.GetObjPtr();
    stat->m_ObjLock.Lock();
    --stat->m_OpenedConns;
    stat->m_ObjLock.Unlock();
}

inline void
CNCServerStat::AddOverflowConnection(void)
{
    CNCServerStat* stat = sm_Getter.GetObjPtr();
    stat->m_ObjLock.Lock();
    ++stat->m_OverflowConns;
    stat->m_ObjLock.Unlock();
}

inline void
CNCServerStat::AddTimedOutCommand(void)
{
    CNCServerStat* stat = sm_Getter.GetObjPtr();
    stat->m_ObjLock.Lock();
    ++stat->m_TimedOutCmds;
    stat->m_ObjLock.Unlock();
}



inline bool
CNetCacheServer::ShutdownRequested(void)
{
    return m_Shutdown;
}

inline int
CNetCacheServer::GetSignalCode(void) const
{
    return m_Signal;
}

inline int
CNetCacheServer::GetNextBlobId(void)
{
    return int(m_BlobIdCounter.Add(1));
}

inline unsigned int
CNetCacheServer::GetCmdTimeout(void) const
{
    return m_CmdTimeout;
}

inline void
CNetCacheServer::RequestShutdown(int sig)
{
    if (!m_Shutdown) {
        m_Shutdown = true;
        m_Signal = sig;
    }
}

inline bool
CNetCacheServer::IsManagingSessions(void) const
{
    return !m_SessionMngThread.Empty();
}

inline bool
CNetCacheServer::RegisterSession(const string& host, unsigned int pid)
{
    if (m_SessionMngThread.Empty())
        return false;

    return m_SessionMngThread->RegisterSession(host, pid);
}

inline void
CNetCacheServer::UnregisterSession(const string& host, unsigned int pid)
{
    if (m_SessionMngThread.Empty())
        return;

    m_SessionMngThread->UnregisterSession(host, pid);
}

inline CServer_Monitor*
CNetCacheServer::GetServerMonitor(void)
{
    return &m_Monitor;
}

inline const IRegistry&
CNetCacheServer::GetRegistry(void)
{
    return CNcbiApplication::Instance()->GetConfig();
}

inline const string&
CNetCacheServer::GetHost(void) const
{
    return m_Host;
}

inline unsigned int
CNetCacheServer::GetPort(void) const
{
    return m_Port;
}

inline unsigned int
CNetCacheServer::GetInactivityTimeout(void) const
{
    return m_InactivityTimeout;
}

inline CTime
CNetCacheServer::GetFastTime(void)
{
    return m_FastTime.GetLocalTime();
}

inline CNCBlobStorage*
CNetCacheServer::GetBlobStorage(const string& cache_name)
{
    TStorageMap::iterator it = m_StorageMap.find(cache_name);
    if (it == m_StorageMap.end()) {
        return NULL;
    }
    return it->second.get();
}

inline void
CNetCacheServer::PrintServerStats(CNcbiIostream* ios)
{
    CPrintTextProxy proxy(CPrintTextProxy::ePrintStream, ios);
    x_PrintServerStats(proxy);
}

END_NCBI_SCOPE

#endif /* NETCACHED__HPP */
