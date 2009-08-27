#ifndef CONNECT_SERVICES___NETICACHE_CLIENT__HPP
#define CONNECT_SERVICES___NETICACHE_CLIENT__HPP

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
 * Authors:  Anatoliy Kuznetsov, Dmitry Kazimirov
 *
 * File Description:
 *   Network client for ICache (NetCache).
 *
 */

/// @file neticache_client.hpp
/// NetCache ICache client specs.
///

#include <connect/services/netservice_api.hpp>
#include <connect/services/srv_discovery.hpp>

#include <connect/ncbi_socket.hpp>
#include <connect/ncbi_core_cxx.hpp>

#include <util/resource_pool.hpp>
#include <util/cache/icache.hpp>

#include <corelib/request_control.hpp>
#include <corelib/plugin_manager_store.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbimtx.hpp>


BEGIN_NCBI_SCOPE


/** @addtogroup NetCacheClient
 *
 * @{
 */

/// Client to NetCache server (implements ICache interface)
///
/// @note This implementation is thread safe and synchronized
///
class NCBI_NET_CACHE_EXPORT CNetICacheClient : virtual protected CConnIniter,
                                               public ICache
{
public:
    /// Construct the client without connecting to any particular
    /// server. Actual server (host and port) will be extracted from the
    /// blob key.
    CNetICacheClient();

    CNetICacheClient(const string&  host,
                     unsigned short port,
                     const string&  cache_name,
                     const string&  client_name);

    CNetICacheClient(const string& lb_service_name,
                     const string& cache_name,
                     const string& client_name,
                     const string& lbsm_affinity_name = kEmptyStr);

    virtual ~CNetICacheClient();

    void SetConnectionParams(const string&  host,
                             unsigned short port,
                             const string&  cache_name,
                             const string&  client_name);

    void SetConnectionParams(CNetServiceDiscovery* service_discovery,
                             IRebalanceStrategy* rebalance_strategy,
                             const string& cache_name,
                             const string& client_name);

    /// Return socket to the socket pool
    /// @note thread sync. method
    virtual
    void ReturnSocket(CSocket* sock, const string& blob_comments);

    /// Send session registration command
    void RegisterSession(unsigned pid);
    /// Send session unregistration command
    void UnRegisterSession(unsigned pid);

    // ICache interface implementation

    virtual void SetTimeStampPolicy(TTimeStampFlags policy,
                                    unsigned int    timeout,
                                    unsigned int    max_timeout = 0);
    virtual TTimeStampFlags GetTimeStampPolicy() const;
    virtual int GetTimeout() const;
    virtual bool IsOpen() const;
    virtual void SetVersionRetention(EKeepVersions policy);
    virtual EKeepVersions GetVersionRetention() const;
    virtual void Store(const string&  key,
                       int            version,
                       const string&  subkey,
                       const void*    data,
                       size_t         size,
                       unsigned int   time_to_live = 0,
                       const string&  owner = kEmptyStr);
    virtual size_t GetSize(const string&  key,
                           int            version,
                           const string&  subkey);
    virtual void GetBlobOwner(const string&  key,
                              int            version,
                              const string&  subkey,
                              string*        owner);
    virtual bool Read(const string& key,
                      int           version,
                      const string& subkey,
                      void*         buf,
                      size_t        buf_size);
    virtual IReader* GetReadStream(const string&  key,
                                   int            version,
                                   const string&  subkey);

    virtual void GetBlobAccess(const string&     key,
                               int               version,
                               const string&     subkey,
                               SBlobAccessDescr*  blob_descr);
    virtual IWriter* GetWriteStream(const string&    key,
                                    int              version,
                                    const string&    subkey,
                                    unsigned int     time_to_live = 0,
                                    const string&    owner = kEmptyStr);
    virtual void Remove(const string& key);
    virtual void Remove(const string&    key,
                        int              version,
                        const string&    subkey);
    virtual time_t GetAccessTime(const string&  key,
                                 int            version,
                                 const string&  subkey);
    virtual bool HasBlobs(const string&  key,
                          const string&  subkey);
    virtual void Purge(time_t           access_timeout,
                       EKeepVersions    keep_last_version = eDropAll);

    virtual void Purge(const string&    key,
                       const string&    subkey,
                       time_t           access_timeout,
                       EKeepVersions    keep_last_version = eDropAll);

    virtual bool SameCacheParams(const TCacheParams* params) const;
    virtual string GetCacheName(void) const;

    /// Set communication timeout default for all new connections
    static
        void SetDefaultCommunicationTimeout(const STimeout& to);

    /// Set communication timeout (ReadWrite)
    void SetCommunicationTimeout(const STimeout& to);
    STimeout& SetCommunicationTimeout();
    STimeout  GetCommunicationTimeout() const;

    void RestoreHostPort();

    /// Set socket (connected to the server)
    ///
    /// @param sock
    ///    Connected socket to the server.
    ///    Communication timeouts of the socket won't be changed
    /// @param own
    ///    Socket ownership
    ///
    void SetSocket(CSocket* sock, EOwnership own = eTakeOwnership);

    /// Detach and return current socket.
    /// Caller is responsible for deletion.
    CSocket* DetachSocket();

    /// Set client name comment (like LB service name).
    /// May be sent to server for better logging.
    void SetClientNameComment(const string& comment)
    {
        m_ClientNameComment = comment;
    }

    const string& GetClientNameComment() const
    {
        return m_ClientNameComment;
    }
    const string& GetClientName() const { return m_ClientName; }

    /// Get socket out of the socket pool (if there are sockets available)
    /// @note thread sync. method
    CSocket* GetPoolSocket();

    const string& GetHost() const { return m_Host; }
    unsigned short GetPort() const { return m_Port; }

protected:
    bool ReadStr(CSocket& sock, string* str);
    void WriteStr(const char* str, size_t len);
    void CreateSocket(const string& hostname, unsigned port);
    void WaitForServer(unsigned wait_sec=0);

    /// @internal
    class CSockGuard
    {
    public:
        CSockGuard(CSocket& sock) : m_Sock(&sock) {}
        CSockGuard(CSocket* sock) : m_Sock(sock) {}
        ~CSockGuard() { if (m_Sock) m_Sock->Close(); }
        /// Dismiss the guard (no disconnect)
        void Release() { m_Sock = 0; }
    private:
        CSockGuard(const CSockGuard&);
        CSockGuard& operator=(const CSockGuard&);
    private:
        CSocket*    m_Sock;
    };

protected:
    CSocket*                m_Sock;
    string                  m_Host;
    unsigned short          m_Port;
    EOwnership              m_OwnSocket;
    string                  m_ClientName;
    STimeout                m_Timeout;
    string                  m_ClientNameComment;
    string                  m_Tmp;                 ///< Temporary string

private:
    CResourcePool<CSocket>  m_SockPool;
    CFastMutex              m_SockPool_Lock;

protected:
    /// Connect to server
    /// Function returns true if connection has been re-established
    /// false if connection has been established before and
    /// throws an exception if it cannot establish connection
    bool CheckConnect();

    string MakeCommandPacket(const string& cmd_str,
                             bool          connected) const;
    void AddKVS(string*          out_str,
                const string&    key,
                int              version,
                const string&    subkey) const;

    virtual
    void TrimPrefix(string* str) const;
    virtual
    void CheckOK(string* str) const;

    IReader* GetReadStream_NoLock(const string&  key,
                                  int            version,
                                  const string&  subkey);

private:
    /// Check if server output starts with "ERR:", throws an exception
    /// "OK:", "ERR:" prefixes are getting trimmed
    ///
    /// @return false if Blob not found error detected
    static bool x_CheckErrTrim(string& answer);

    CNetICacheClient(const CNetICacheClient&);
    CNetICacheClient& operator=(const CNetICacheClient&);

protected:
    CNetObjectRef<IRebalanceStrategy> m_RebalanceStrategy;
    CNetObjectRef<CNetServiceDiscovery> m_ServiceDiscovery;

    string              m_CacheName;
    size_t              m_BlobSize;
    CRequestRateControl m_Throttler;
    mutable CFastMutex  m_Lock;     ///< Client access lock
};

extern NCBI_NET_CACHE_EXPORT const char* kNetICacheDriverName;

extern "C"
{

NCBI_NET_CACHE_EXPORT
void NCBI_EntryPoint_xcache_netcache(
     CPluginManager<ICache>::TDriverInfoList&   info_list,
     CPluginManager<ICache>::EEntryPointRequest method);

NCBI_NET_CACHE_EXPORT
void Cache_RegisterDriver_NetCache(void);

} // extern C

/* @} */


END_NCBI_SCOPE

#endif  /* CONNECT_SERVICES___NETICACHE_CLIENT__HPP */
