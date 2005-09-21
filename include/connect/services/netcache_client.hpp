#ifndef CONN___NETCACHE_CLIENT__HPP
#define CONN___NETCACHE_CLIENT__HPP

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
 * File Description:
 *   Net cache client API.
 *
 */

/// @file netcache_client.hpp
/// NetCache client specs. 
///

#include <connect/connect_export.h>
#include <connect/ncbi_types.h>
#include <connect/ncbi_conn_reader_writer.hpp>
#include <connect/services/netservice_client.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/plugin_manager.hpp>
#include <corelib/ncbimisc.hpp>
#include <util/reader_writer.hpp>
#include <util/transmissionrw.hpp>



BEGIN_NCBI_SCOPE


class CSocket;

/** @addtogroup NetCacheClient
 *
 * @{
 */

/// Client API for NetCache server
///
/// After any Put, Get transactions connection socket
/// is closed (part of NetCache protocol implemenation)
///
/// @sa NetCache_ConfigureWithLB, CNetCacheClient_LB
///
class NCBI_XCONNECT_EXPORT CNetCacheClient : public CNetServiceClient
{
public:
    /// Construct the client without linking it to any particular
    /// server. Actual server (host and port) will be extracted from the
    /// BLOB key 
    ///
    /// @param client_name
    ///    local name of the client program 
    ///    (used for server access logging and authentication)
    CNetCacheClient(const string&  client_name);

    /// Construct client, working with the specified server (host and port)
    CNetCacheClient(const string&  host,
                    unsigned short port,
                    const string&  client_name);

    /// Construction.
    /// @param sock
    ///    Connected socket to the primary server. 
    ///    CNetCacheCleint does not take socket ownership and does not change 
    ///    communication parameters (like timeout)
    /// @param client_name
    ///    Identification name of the connecting client
    CNetCacheClient(CSocket*      sock,
                    const string& client_name);

    virtual ~CNetCacheClient();



    /// Put BLOB to server
    ///
    /// @param time_to_live
    ///    BLOB time to live value in seconds. 
    ///    0 - server side default is assumed.
    /// 
    /// Please note that time_to_live is controlled by the server-side
    /// parameter so if you set time_to_live higher than server-side value,
    /// server side TTL will be in effect.
    ///
    /// @return NetCache access key
    virtual 
    string PutData(const void*   buf,
                   size_t        size,
                   unsigned int  time_to_live = 0);

    /// Put BLOB to server
    ///
    /// @param key
    ///    NetCache key, if empty new key is created
    ///    
    /// @param time_to_live
    ///    BLOB time to live value in seconds. 
    ///    0 - server side default is assumed.
    /// 
    /// @return
    ///    IReader* (caller must delete this). 
    virtual
    IWriter* PutData(string* key, unsigned int  time_to_live = 0);

    /// Update an existing BLOB
    ///
    virtual 
    string PutData(const string& key,
                   const void*   buf,
                   size_t        size,
                   unsigned int  time_to_live = 0);

    /// Retrieve BLOB from server by key
    /// If BLOB not found method returns NULL
    //
    /// Caller is responsible for deletion of the IReader*
    /// IReader* MUST be deleted before calling any other
    /// method and before destruction of CNetCacheClient.
    ///
    /// @note 
    ///   IReader implementation used here is TCP/IP socket 
    ///   based, so when reading the BLOB please remember to check
    ///   IReader::Read return codes, it may not be able to read
    ///   the whole BLOB in one call because of network delays.
    ///
    /// @param key
    ///    BLOB key to read (returned by PutData)
    /// @param blob_size
    ///    Size of the BLOB
    /// @return
    ///    IReader* (caller must delete this). 
    ///    NULL means that BLOB was not found (expired).
    virtual 
    IReader* GetData(const string& key, 
                     size_t*       blob_size = 0);

    /// Status of GetData() call
    /// @sa GetData
    enum EReadResult {
        eReadComplete, ///< The whole BLOB has been read
        eNotFound,     ///< BLOB not found or error
        eReadPart      ///< Read part of the BLOB (buffer capacity)
    };

    /// Structure to read BLOB in memory
    struct SBlobData
    {
        AutoPtr<unsigned char, 
                ArrayDeleter<unsigned char> >  blob;      ///< blob data
        size_t                                 blob_size; ///< blob size

        SBlobData() : blob_size(0) {}
    };

    /// Retrieve BLOB from server by key
    /// This method retrives BLOB size, allocates memory and gets all
    /// the data from the server.
    ///
    /// Blob size and binary data is placed into blob_to_read structure.
    /// Do not use this method if you are not sure you have memory 
    /// to load the whole BLOB.
    /// 
    /// @return
    ///    eReadComplete if BLOB found (eNotFound otherwise)
    virtual
    EReadResult GetData(const string& key, SBlobData& blob_to_read);

    /// Remove BLOB by key
    virtual 
    void Remove(const string& key);


    /// Returns communication error message or empty string.
    /// This method is used to deliver error message from
    /// IReader/IWriter because they sometimes cannot throw exceptions
    /// (intercepted by C++ stream library)
    ///
    /// This method can be called once to get error, repeated call 
    /// returns empty string
    virtual
    string GetCommErrMsg();


    /// Retrieve BLOB from server by key
    ///
    /// @note
    ///    Function waits for enough data to arrive.
    virtual 
    EReadResult GetData(const string&  key,
                        void*          buf, 
                        size_t         buf_size, 
                        size_t*        n_read    = 0,
                        size_t*        blob_size = 0);



    /// Return version string
    string ServerVersion();

protected:
    /// Shutdown the server daemon.
    ///
    /// @note
    ///  Protected to avoid a temptation to call it from time to time. :)
    void ShutdownServer();

    /// Turn server-side logging on(off)
    ///
    void Logging(bool on_off);
    /// Connects to server to make sure it is running.
    bool IsAlive();

    /// Print ini file
    void PrintConfig(CNcbiOstream& out);

    /// Print server statistics
    void PrintStat(CNcbiOstream& out);

protected:

    /// Set communication error message
    virtual
    void SetCommErrMsg(const string& msg);

    bool IsError(const char* str);

    void SendClientName();

    /// Makes string including authentication info(client name) and
    /// command
    void MakeCommandPacket(string* out_str, const string& cmd_str);

    /// If client is not already connected to the primary server it 
    /// tries to connect to the server specified in the BLOB key
    /// (all infomation is encoded in there)
    virtual 
    void CheckConnect(const string& key);

    /// @return netcache key
    void PutInitiate(string*       key,
                     unsigned      time_to_live,
                     unsigned      put_version = 0);

    void TransmitBuffer(const char* buf, size_t len);

    friend class CNetCache_WriterErrCheck;

private:
    CNetCacheClient(const CNetCacheClient&);
    CNetCacheClient& operator=(const CNetCacheClient&);
protected:
    string    m_Tmp;
    unsigned  m_PutVersion;
    string    m_CommErrMsg;  ///< communication err message
};



/// Client API for NetCache server.
///
/// The same API as provided by CNetCacheClient, 
/// only integrated with NCBI load balancer
///
/// Rebalancing is based on a combination of rebalance parameters,
/// when rebalance parameter is not zero and that parameter has been 
/// reached client connects to NCBI load balancing service (could be a 
/// daemon or a network instance) and obtains the most available server.
/// 
/// The intended use case for this client is long living programs like
/// services or fast CGIs or any other program running a lot of NetCache
/// requests.
///
/// @sa CNetCacheClient
///
class NCBI_XCONNECT_EXPORT CNetCacheClient_LB : public CNetCacheClient
{
public:

    typedef CNetCacheClient  TParent;

    /// Construct load-balanced client.
    ///
    ///
    /// @param client_name
    ///    Name of the client
    /// @param lb_service_name
    ///    Service name as listed in NCBI load balancer
    ///
    /// @param rebalance_time
    ///    Number of seconds after which client is rebalanced
    /// @param rebalance_requests
    ///    Number of requests before rebalancing. 
    /// @param rebalance_bytes
    ///    Read/Write bytes before rebalancing occures
    ///
    CNetCacheClient_LB(const string& client_name,
                       const string& lb_service_name,
                       unsigned int  rebalance_time     = 10,
                       unsigned int  rebalance_requests = 0,
                       unsigned int  rebalance_bytes    = 0);

    virtual ~CNetCacheClient_LB();

    /// Specifies when client use backup servers instead of LB provided
    /// services
    ///
    enum EServiceBackupMode {
        ENoBackup     = (0 << 0), ///< Do not use backup servers
        ENameNotFound = (1 << 0), ///< backup if LB does not find service name
        ENoConnection = (1 << 1), ///< backup if no TCP connection
        EFullBackup   = (ENameNotFound | ENoConnection)
    };

    typedef int TServiceBackupMode;

    /// Enable client to connect to predefined backup servers if service
    /// name is not available (load balancer is not available)
    /// (enabled by default)
    ///
    void EnableServiceBackup(TServiceBackupMode backup_mode = EFullBackup);


    virtual 
    string PutData(const void*   buf,
                   size_t        size,
                   unsigned int  time_to_live = 0);
    virtual 
    string PutData(const string& key,
                   const void*   buf,
                   size_t        size,
                   unsigned int  time_to_live = 0);
    virtual
    IWriter* PutData(string* key, unsigned int  time_to_live = 0);

    virtual 
    IReader* GetData(const string& key, 
                            size_t*       blob_size = 0);
    virtual 
    void Remove(const string& key);
    virtual 
    EReadResult GetData(const string&  key,
                        void*          buf, 
                        size_t         buf_size, 
                        size_t*        n_read    = 0,
                        size_t*        blob_size = 0);
    virtual
    EReadResult GetData(const string& key, SBlobData& blob_to_read);

protected:
    virtual 
    void CheckConnect(const string& key);
private:
    CNetCacheClient_LB(const CNetCacheClient_LB&);
    CNetCacheClient_LB& operator=(const CNetCacheClient_LB&);

private:
    string   m_LB_ServiceName;

    unsigned int  m_RebalanceTime;
    unsigned int  m_RebalanceRequests;
    unsigned int  m_RebalanceBytes;

    time_t               m_LastRebalanceTime;
    unsigned int         m_Requests;
    unsigned int         m_RWBytes;
    bool                 m_StickToHost;
    TServiceBackupMode   m_ServiceBackup;
};

NCBI_DECLARE_INTERFACE_VERSION(CNetCacheClient,  "xnetcache", 1, 1, 0);


/// NetCache internal exception
///
class CNetCacheException : public CNetServiceException
{
public:
    typedef CNetServiceException TParent;
    enum EErrCode {
        ///< If client is not allowed to run this operation
        eAuthenticationError,
        ///< BLOB key corruption or version mismatch
        eKeyFormatError,
        ///< Server side error
        eServerError
    };

    virtual const char* GetErrCodeString(void) const
    {
        switch (GetErrCode())
        {
        case eAuthenticationError: return "eAuthenticationError";
        case eKeyFormatError:      return "eKeyFormatError";
        case eServerError:         return "eServerError";
        default:                   return CException::GetErrCodeString();
        }
    }

    NCBI_EXCEPTION_DEFAULT(CNetCacheException, CNetServiceException);
};

/// Meaningful information encoded in the NetCache key
///
/// @sa CNetCache_ParseBlobKey
///
class NCBI_XCONNECT_EXPORT CNetCache_Key
{
public:
    /// Create the key out of string
    /// @sa ParseBlobKey()
    CNetCache_Key(const string& key_str);

    /// Parse blob key string into a CNetCache_Key structure
    void ParseBlobKey(const string& key_str);

    /// Generate blob key string
    ///
    /// Please note that "id" is an integer issued by the NetCache server.
    /// Clients should not use this function with custom ids.
    /// Otherwise it may disrupt the interserver communication.
    static
    void GenerateBlobKey(string*        key,
                         unsigned int   id,
                         const string&  host,
                         unsigned short port);

    /// Parse blob key, extract id
    static 
    unsigned int GetBlobId(const string& key_str);
public:
    string       prefix;    ///< Key prefix
    unsigned     version;   ///< Key version
    unsigned     id;        ///< BLOB id
    string       hostname;  ///< server name
    unsigned     port;      ///< TCP/IP port number
};



/// IReader/IWriter implementation 
///
/// @internal
///
class CNetCacheSock_RW : public CSocketReaderWriter
{
public:
    CNetCacheSock_RW(CSocket* sock);
    virtual ~CNetCacheSock_RW();

    /// Take socket ownership
    void OwnSocket();

    /// Access to CSocketReaderWriter::m_Sock
    CSocket& GetSocket() { _ASSERT(m_Sock); return *m_Sock; }
};

/// IWriter with error checking
///
/// @internal
class CNetCache_WriterErrCheck : public CTransmissionWriter
{
public:
    typedef CTransmissionWriter TParent;
public:
    CNetCache_WriterErrCheck(CNetCacheSock_RW* wrt, 
                             EOwnership        own_writer,
                             CNetCacheClient&  parent);
    virtual ~CNetCache_WriterErrCheck();

    virtual 
    ERW_Result Write(const void* buf,
                     size_t      count,
                     size_t*     bytes_written = 0);

    virtual 
    ERW_Result Flush(void);
protected:
    void CheckInputMessage();
protected:
    CNetCacheSock_RW*  m_RW;
    CNetCacheClient&   m_NC_Client;
};


extern NCBI_XCONNECT_EXPORT const char* kNetCacheDriverName;


//extern "C" {

void NCBI_XCONNECT_EXPORT NCBI_EntryPoint_xnetcache(
     CPluginManager<CNetCacheClient>::TDriverInfoList&   info_list,
     CPluginManager<CNetCacheClient>::EEntryPointRequest method);


//} // extern C

/* @} */


END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.41  2005/09/21 18:21:08  kuznets
 * Made a class for key manipulation service functions
 *
 * Revision 1.40  2005/09/21 16:12:01  kuznets
 * Added comment
 *
 * Revision 1.39  2005/08/24 15:57:12  kuznets
 * Improved comments
 *
 * Revision 1.38  2005/08/11 19:32:56  kuznets
 * Added communication error from IWriter
 *
 * Revision 1.37  2005/08/11 17:51:02  kuznets
 * Added IWriter implementation with transmission checking
 *
 * Revision 1.36  2005/08/09 16:00:47  kuznets
 * Added GetData(), allocating memory for BLOB (C++ style)
 *
 * Revision 1.35  2005/08/01 16:52:26  kuznets
 * +PrintStat()
 *
 * Revision 1.34  2005/07/27 18:13:59  kuznets
 * PrintConfig()
 *
 * Revision 1.33  2005/05/12 18:34:52  vakatov
 * Minor warning heeded
 *
 * Revision 1.32  2005/04/19 14:17:13  kuznets
 * Alternative put version
 *
 * Revision 1.31  2005/04/07 13:51:02  kuznets
 * NetCache_ConfigureWithLB() removed from the public interface
 *
 * Revision 1.30  2005/04/04 18:12:36  kuznets
 * Detailed parametrization of service backup
 *
 * Revision 1.29  2005/04/04 16:41:51  kuznets
 * Backup service (when LB unavailable) location made optional
 *
 * Revision 1.28  2005/03/28 15:31:37  didenko
 * Made destructors virtual
 *
 * Revision 1.27  2005/03/22 21:42:50  didenko
 * Got rid of warnning on Sun WorkShop
 *
 * Revision 1.26  2005/03/22 18:54:18  kuznets
 * Changed project tree layout
 *
 * Revision 1.25  2005/03/21 16:53:31  didenko
 * + creating from PluginManager
 *
 * Revision 1.24  2005/02/15 15:20:05  kuznets
 * IsAlive made protected
 *
 * Revision 1.23  2005/02/07 13:00:41  kuznets
 * Part of functionality moved to netservice_client.hpp(cpp)
 *
 * Revision 1.22  2005/01/28 19:25:16  kuznets
 * Exception method inlined
 *
 * Revision 1.21  2005/01/28 14:55:06  kuznets
 * GetCommunicatioTimeout() declared const
 *
 * Revision 1.20  2005/01/28 14:46:38  kuznets
 * Code clean-up, added PutData returning IWriter
 *
 * Revision 1.19  2005/01/19 12:21:10  kuznets
 * +CNetCacheClient_LB
 *
 * Revision 1.18  2005/01/05 17:45:34  kuznets
 * Removed default client name
 *
 * Revision 1.17  2004/12/27 16:30:30  kuznets
 * + logging control function
 *
 * Revision 1.16  2004/12/20 13:13:14  kuznets
 * +NetCache_ConfigureWithLB
 *
 * Revision 1.15  2004/12/16 17:32:45  kuznets
 * + methods to change comm.timeouts
 *
 * Revision 1.14  2004/12/15 19:04:40  kuznets
 * Code cleanup
 *
 * Revision 1.13  2004/11/16 19:26:39  kuznets
 * Improved documentation
 *
 * Revision 1.12  2004/11/04 21:49:51  kuznets
 * CheckConnect() fixed
 *
 * Revision 1.11  2004/11/02 17:29:55  kuznets
 * Implemented reconnection mode and no-default server mode
 *
 * Revision 1.10  2004/11/01 16:02:17  kuznets
 * GetData now returns BLOB size
 *
 * Revision 1.9  2004/11/01 14:39:30  kuznets
 * Implemented BLOB update
 *
 * Revision 1.8  2004/10/28 16:16:09  kuznets
 * +CNetCacheClient::Remove()
 *
 * Revision 1.7  2004/10/27 14:16:45  kuznets
 * BLOB key parser moved from netcached
 *
 * Revision 1.6  2004/10/25 14:36:03  kuznets
 * New methods IsAlive(), ServerVersion()
 *
 * Revision 1.5  2004/10/22 13:51:01  kuznets
 * Documentation chnages
 *
 * Revision 1.4  2004/10/08 12:30:34  lavr
 * Cosmetics
 *
 * Revision 1.3  2004/10/05 19:01:59  kuznets
 * Implemented ShutdownServer()
 *
 * Revision 1.2  2004/10/05 18:17:58  kuznets
 * comments, added GetData
 *
 * Revision 1.1  2004/10/04 18:44:33  kuznets
 * Initial revision
 *
 *
 * ===========================================================================
 */


#endif  /* CONN___NETCACHE_CLIENT__HPP */
