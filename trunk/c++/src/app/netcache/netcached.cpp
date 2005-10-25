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
 * File Description: Network cache daemon
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

#include <util/cache/icache.hpp>
#include <util/cache/icache_clean_thread.hpp>
#include <util/logrotate.hpp>
#include <util/transmissionrw.hpp>

#include <bdb/bdb_blobcache.hpp>

#include <connect/threaded_server.hpp>
#include <connect/ncbi_socket.hpp>
#include <connect/ncbi_conn_stream.hpp>
#include <connect/ncbi_conn_reader_writer.hpp>
#include <connect/services/netcache_client.hpp>

#if defined(NCBI_OS_UNIX)
# include <corelib/ncbi_os_unix.hpp>
# include <signal.h>
#endif

#define NETCACHED_VERSION \
      "NCBI NetCache server version=1.3.5  " __DATE__ " " __TIME__


USING_NCBI_SCOPE;

//const unsigned int kNetCacheBufSize = 64 * 1024;
const unsigned int kObjectTimeout = 60 * 60; ///< Default timeout in seconds

/// General purpose NetCache Mutex
DEFINE_STATIC_FAST_MUTEX(x_NetCacheMutex);

/// Mutex to guard vector of busy IDs 
DEFINE_STATIC_FAST_MUTEX(x_NetCacheMutex_ID);






/// Class guards the BLOB id, guarantees exclusive access to the object
///
/// @internal
class CIdBusyGuard
{
public:
    CIdBusyGuard(bm::bvector<>* id_set)
        : m_IdSet(id_set), m_Id(0)
    {}

    CIdBusyGuard(bm::bvector<>* id_set, 
                 unsigned int   id,
                 unsigned       timeout)
        : m_IdSet(id_set)
    {
        Lock(id, timeout);
    }

    bool IsLocked(unsigned int  id)
    {
        CFastMutexGuard guard(x_NetCacheMutex_ID);
        return (*m_IdSet)[id];
    }

    void Lock(unsigned int   id,
              unsigned       timeout)
    {
        _ASSERT(id);
        unsigned cnt = 0; unsigned sleep_ms = 10;
        while (true) {
            {{
            CFastMutexGuard guard(x_NetCacheMutex_ID);
            if (!(*m_IdSet)[id]) {
                m_IdSet->set(id);
                break;
            }
            }}
            cnt += sleep_ms;
            if (cnt > timeout * 1000) {
                string err_msg("Failed to lock BLOB object ");
                err_msg.append("timeout=");
                err_msg.append(NStr::UIntToString(timeout));
                NCBI_THROW(CNetServiceException, eTimeout, err_msg);
            }
            SleepMilliSec(sleep_ms);
        } // while
        m_Id = id;
    }

    /// Returns false if lock was not successfull 
    /// (when timeout == 0 i.e. no-locking mode)
    bool Lock(const string& blob_key, unsigned timeout)
    {
        unsigned cnt = 0; unsigned sleep_ms = 10;
        unsigned id = 0;
        while (true) {
            if (id == 0) {
                id = CNetCache_Key::GetBlobId(blob_key);
            }

            {{
            CFastMutexGuard guard(x_NetCacheMutex_ID);

            if ((*m_IdSet)[id] == false) {
                m_IdSet->set(id);
                break;
            }
            }}

            if (timeout == 0) {
                return false;
            }

            cnt += sleep_ms;
            if (cnt > timeout * 1000) {
                string err_msg("Failed to lock BLOB object ");
                err_msg.append("timeout=");
                err_msg.append(NStr::UIntToString(timeout));

                NCBI_THROW(CNetServiceException, eTimeout, err_msg);
            }
            SleepMilliSec(sleep_ms);
        } // while
        m_Id = id;
        return true;
    }

    void LockNewId(unsigned*  max_id, unsigned timeout)
    {
        unsigned cnt = 0; unsigned sleep_ms = 10;
        unsigned id = 0;
        while (true) {
            {{
            CFastMutexGuard guard(x_NetCacheMutex_ID);

            if (id == 0) {
                id = ++(*max_id);
            }

            if (!(*m_IdSet)[id]) {
                m_IdSet->set(id);
                break;
            }
            }}
            cnt += sleep_ms;
            if (cnt > timeout * 1000) {
                NCBI_THROW(CNetServiceException, 
                           eTimeout, "Failed to lock new BLOB object");
            }
            SleepMilliSec(sleep_ms);
        } // while
        m_Id = id;
    }


    ~CIdBusyGuard()
    {
        Release();
    }

    void Release()
    {
        if (m_Id) {
            CFastMutexGuard guard(x_NetCacheMutex_ID);
            m_IdSet->set(m_Id, false);
            m_Id = 0;
        }
    }

    unsigned GetId() const { return m_Id; }

private:
    CIdBusyGuard(const CIdBusyGuard&);
    CIdBusyGuard& operator=(const CIdBusyGuard&);
private:
    bm::bvector<>*   m_IdSet;
    unsigned int     m_Id;
};


/// @internal
///
/// Netcache server side request statistics
///
struct NetCache_RequestStat
{
    CTime        conn_time;    ///< request incoming time in seconds
    unsigned     req_code;     ///< 'P' put, 'G' get
    size_t       blob_size;    ///< BLOB size
    double       elapsed;      ///< total time in seconds to process request
    double       comm_elapsed; ///< time spent reading/sending data
    string       peer_address; ///< Client's IP address
    unsigned     io_blocks;    ///< Number of IO blocks translated
};

/// @internal
class CNetCacheLogStream : public CRotatingLogStream
{
public:
    typedef CRotatingLogStream TParent;
public:
    CNetCacheLogStream(const string&    filename, 
                       CNcbiStreamoff   limit)
    : TParent(filename, limit)
    {}
protected:
    virtual string x_BackupName(string& name)
    {
        return kEmptyStr;
    }

};

/// @internal
///
/// Netcache logger
///
class CNetCache_Logger
{
public:
    CNetCache_Logger(const string&    filename, 
                     CNcbiStreamoff   limit)
      : m_Log(filename, limit)
    {}

    void Put(const string& auth, 
             const NetCache_RequestStat& stat,
             const string&  blob_key)
    {
        string msg, tmp;
        msg += auth;
        msg += ';';
        msg += stat.peer_address;
        msg += ';';
        msg += stat.conn_time.AsString();
        msg += ';';
        msg += (char)stat.req_code;
        msg += ';';
        NStr::UInt8ToString(tmp, stat.blob_size);
        msg += tmp;
        msg += ';';
        msg += NStr::DoubleToString(stat.elapsed, 5);
        msg += ';';
        msg += NStr::DoubleToString(stat.comm_elapsed, 5);
        msg += ';';
        msg += blob_key;
        msg += "\n";

        m_Log << msg;

        if (stat.req_code == 'V') {
            m_Log << NcbiFlush;
        }
    }
    void Rotate() { m_Log.Rotate(); }
private:
    CNetCacheLogStream m_Log;
};

class CNetCacheServer;

static CNetCacheServer* s_netcache_server = 0;

/// NC requests
///
/// @internal
typedef enum {
    eError,
    ePut,
    ePut2,   ///< PUT v.2 transmission protocol
    eGet,
    eShutdown,
    eVersion,
    eRemove,
    eLogging,
    eGetConfig,
    eGetStat,
    eIsLock
} ENC_RequestType;


/// Request context
///
/// @internal
struct SNC_Request
{
    ENC_RequestType req_type;
    unsigned int    timeout;
    string          req_id;
    string          err_msg;
    bool            no_lock;

    void Init() 
    {
        req_type = eError;
        timeout = 0;
        req_id.erase(); err_msg.erase();
        no_lock = false;
    }
};


/// Thread specific data for threaded server
///
/// @internal
///
struct SNC_ThreadData
{
    string      request;                        ///< request string
    string      auth;                           ///< Authentication string
    SNC_Request req;                            ///< parsed request
    AutoPtr<char, ArrayDeleter<char> >  buffer; ///< operation buffer

    SNC_ThreadData(unsigned int size)
        : buffer(new char[size + 256]) 
    {}
};


///@internal
static
void TlsCleanup(SNC_ThreadData* p_value, void* /* data */ )
{
    delete p_value;
}

/// @internal
static
CRef< CTls<SNC_ThreadData> > s_tls(new CTls<SNC_ThreadData>);


/// Netcache threaded server
///
/// @internal
class CNetCacheServer : public CThreadedServer
{
public:
    CNetCacheServer(unsigned int     port,
                    ICache*          cache,
                    unsigned         max_threads,
                    unsigned         init_threads,
                    unsigned         network_timeout,
                    bool             is_log,
                    const IRegistry& reg) 
        : CThreadedServer(port),
          m_MaxId(0),
          m_Cache(cache),
          m_Shutdown(false),
          m_InactivityTimeout(network_timeout),
          m_LogFlag(is_log),
          m_TLS_Size(64 * 1024),
          m_Reg(reg)
    {
        char hostname[256];
        int status = SOCK_gethostname(hostname, sizeof(hostname));
        if (status == 0) {
            m_Host = hostname;
        }

        m_MaxThreads = max_threads ? max_threads : 25;
        m_InitThreads = init_threads ? 
            (init_threads < m_MaxThreads ? init_threads : 2)  : 10;
        m_QueueSize = m_MaxThreads + 2;

        x_CreateLog();

        s_netcache_server = this;
        m_AcceptTimeout = &m_ThrdSrvAcceptTimeout;
    }

    virtual ~CNetCacheServer() {}

    unsigned int GetTLS_Size() const { return m_TLS_Size; }
    void SetTLS_Size(unsigned int size) { m_TLS_Size = size; }

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

protected:
    virtual void ProcessOverflow(SOCK sock) 
    {
        const char* msg = "ERR:Server busy";
        
        size_t n_written;
        SOCK_Write(sock, (void *)msg, 16, &n_written, eIO_WritePlain);
        SOCK_Close(sock); 
        ERR_POST("ProcessOverflow! Server is busy.");
    }

private:

    /// Process "PUT" request
    void ProcessPut(CSocket&              sock, 
                    SNC_Request&          req,
                    SNC_ThreadData&       tdata,
                    NetCache_RequestStat& stat);

    /// Process "PUT2" request
    void ProcessPut2(CSocket&              sock, 
                     SNC_Request&          req,
                     SNC_ThreadData&       tdata,
                     NetCache_RequestStat& stat);

    /// Process "GET" request
    void ProcessGet(CSocket&              sock, 
                    const SNC_Request&    req,
                    SNC_ThreadData&       tdata,
                    NetCache_RequestStat& stat);

    /// Process "VERSION" request
    void ProcessVersion(CSocket& sock, const SNC_Request& req);

    /// Process "LOG" request
    void ProcessLog(CSocket& sock, const SNC_Request& req);

    /// Process "REMOVE" request
    void ProcessRemove(CSocket& sock, const SNC_Request& req);

    /// Process "SHUTDOWN" request
    void ProcessShutdown();

    /// Process "GETCONF" request
    void ProcessGetConfig(CSocket& sock);

    /// Process "GETSTAT" request
    void ProcessGetStat(CSocket& sock, const SNC_Request& req);

    /// Process "ISLK" request
    void ProcessIsLock(CSocket& sock, const SNC_Request& req);


    /// Returns FALSE when socket is closed or cannot be read
    bool ReadStr(CSocket& sock, string* str);

    /// Read buffer from the socket.
    // bool ReadBuffer(CSocket& sock, char* buf, size_t* buffer_length);

    /// TRUE return means we have EOF in the socket (no more data is coming)
    bool ReadBuffer(CSocket& sock, 
                    char*    buf, 
                    size_t   buf_size,
                    size_t*  read_length);

    /// TRUE return means we have EOF in the socket (no more data is coming)
    bool ReadBuffer(CSocket& sock,
                    IReader* rdr, 
                    char*    buf, 
                    size_t   buf_size,
                    size_t*  read_length);

    void ParseRequest(const string& reqstr, SNC_Request* req);

    /// Reply to the client
    void WriteMsg(CSocket& sock, 
                  const string& prefix, const string& msg);

    /// Generate unique system-wide request id
    void GenerateRequestId(const SNC_Request& req, 
                           string*            id_str,
                           unsigned int*      transaction_id);


    /// Get logger instance
    CNetCache_Logger* GetLogger();
    /// TRUE if logging is ON
    bool IsLog() const;

private:
    bool x_CheckBlobId(CSocket&       sock,
                       CNetCache_Key* blob_id, 
                       const string&  blob_key);

    void x_WriteBuf(CSocket& sock,
                    char*    buf,
                    size_t   bytes);

    void x_CreateLog();

    void x_PrintStatistics(CNcbiOstream& ios, const SBDB_CacheStatistics& cs);
    void x_PrintStatisticsHistogram(
                          CNcbiOstream&                                   ios, 
                          const SBDB_CacheStatistics::TBlobSizeHistogram& hist);

    /// Check if we have active thread data for this thread.
    /// Setup thread data if we don't.
    SNC_ThreadData* x_GetThreadData(); 

    /// Register protocol error (statistics)
    void x_RegisterProtocolErr(ENC_RequestType   req_type,
                               const string&     auth);
    void x_RegisterInternalErr(ENC_RequestType   req_type,
                               const string&     auth);
    void x_RegisterNoBlobErr(ENC_RequestType   req_type,
                               const string&     auth);

    /// Register exception error (statistics)
    void x_RegisterException(ENC_RequestType           req_type,
                             const string&             auth,
                             const CNetCacheException& ex);

    /// Register exception error (statistics)
    void x_RegisterException(ENC_RequestType           req_type,
                             const string&             auth,
                             const CNetServiceException& ex);

private:
    /// Host name where server runs
    string             m_Host;
    /// ID counter
    unsigned           m_MaxId;
    /// Set of ids in use (PUT)
    bm::bvector<>      m_UsedIds;
    ICache*            m_Cache;
    /// Flags that server received a shutdown request
    volatile bool      m_Shutdown; 
    /// Time to wait for the client (seconds)
    unsigned           m_InactivityTimeout;
    /// Log writer
    auto_ptr<CNetCache_Logger>  m_Logger;
    /// Logging ON/OFF
    bool                        m_LogFlag;
    unsigned int                m_TLS_Size;
    
    /// Accept timeout for threaded server
    STimeout                     m_ThrdSrvAcceptTimeout;
    /// Quick local timer
    CFastLocalTime               m_LocalTimer;
    /// Configuration
    const IRegistry&             m_Reg;
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
        NCBI_THROW(CNetServiceException, eTimeout, kEmptyStr);
    default:
        NCBI_THROW(CNetServiceException, 
                   eCommunicationError, "Socket Wait error.");
        return false;
    }
    return false;
}        


void CNetCacheServer::Process(SOCK sock)
{
    SNC_ThreadData* tdata = x_GetThreadData();
    ENC_RequestType req_type = eError;
    NetCache_RequestStat    stat;
    stat.blob_size = stat.io_blocks = 0;

    CSocket socket;
    socket.Reset(sock, eTakeOwnership, eCopyTimeoutsFromSOCK);

    try {
        tdata->auth.erase();
        
        CStopWatch              sw(false); // OFF by default
        bool is_log = IsLog();

        if (is_log) {
            stat.conn_time = m_LocalTimer.GetLocalTime();
            stat.comm_elapsed = 0;
            stat.peer_address = socket.GetPeerAddress();
                       
            // get only host name
            string::size_type offs = stat.peer_address.find_first_of(":");
            if (offs != string::npos) {
                stat.peer_address.erase(offs, stat.peer_address.length());
            }

            sw.Start();
        }


        // Set socket parameters

        socket.DisableOSSendDelay();
        STimeout to = {m_InactivityTimeout, 0};
        socket.SetTimeout(eIO_ReadWrite , &to);

        tdata->req.Init();
        s_WaitForReadSocket(socket, m_InactivityTimeout);

        if (ReadStr(socket, &(tdata->auth))) {
            
            s_WaitForReadSocket(socket, m_InactivityTimeout);

            if (ReadStr(socket, &(tdata->request))) {                
                ParseRequest(tdata->request, &(tdata->req));

                switch ((req_type = tdata->req.req_type)) {
                case ePut:
                    stat.req_code = 'P';
                    ProcessPut(socket, tdata->req, *tdata, stat);
                    break;
                case ePut2:
                    stat.req_code = 'P';
                    ProcessPut2(socket, tdata->req, *tdata, stat);
                    break;
                case eGet:
                    stat.req_code = 'G';
                    ProcessGet(socket, tdata->req, *tdata, stat);
                    break;
                case eShutdown:
                    stat.req_code = 'S';
                    ProcessShutdown();
                    break;
                case eGetConfig:
                    stat.req_code = 'C';
                    ProcessGetConfig(socket);
                    break;
                case eGetStat:
                    stat.req_code = 'T';
                    ProcessGetStat(socket, tdata->req);
                    break;
                case eVersion:
                    stat.req_code = 'V';
                    ProcessVersion(socket, tdata->req);
                    break;
                case eRemove:
                    stat.req_code = 'R';
                    ProcessRemove(socket, tdata->req);
                    break;
                case eLogging:
                    stat.req_code = 'L';
                    ProcessLog(socket, tdata->req);
                    break;
                case eIsLock:
                    stat.req_code = 'K';
                    ProcessIsLock(socket, tdata->req);
                    break;
                case eError:
                    WriteMsg(socket, "ERR:", tdata->req.err_msg);
                    x_RegisterProtocolErr(eError, tdata->auth);
                    break;
                default:
                    _ASSERT(0);
                } // switch
            }
        }

        // cleaning up the input wire, in case if there is some
        // trailing input.
        // i don't want to know wnat is it, but if i don't read it
        // some clients will receive PEER RESET error trying to read
        // servers answer.
        //
        // I'm reading fixed length sized buffer, so any huge ill formed 
        // request still will end up with an error on the client side
        // (considered a client's fault (DDOS attempt))
        to.sec = to.usec = 0;
        socket.SetTimeout(eIO_Read, &to);
        socket.Read(NULL, 1024);

        //
        // Logging.
        //
        if (is_log) {
            stat.elapsed = sw.Elapsed();
            CNetCache_Logger* lg = GetLogger();
            _ASSERT(lg);
            lg->Put(tdata->auth, stat, tdata->req.req_id);
        }
    } 
    catch (CNetCacheException &ex)
    {
        ERR_POST("NC Server error: " << ex.what() 
                 << " client="       << tdata->auth
                 << " request='"     << tdata->request << "'"
                 << " peer="         << socket.GetPeerAddress()
                 << " blobsize="     << stat.blob_size
                 << " io blocks="    << stat.io_blocks
                 );
        x_RegisterException(req_type, tdata->auth, ex);
    }
    catch (CNetServiceException& ex)
    {
        ERR_POST("Server error: " << ex.what() 
                 << " client="    << tdata->auth
                 << " request='"  << tdata->request << "'"
                 << " peer="      << socket.GetPeerAddress()
                 << " blobsize="     << stat.blob_size
                 << " io blocks="    << stat.io_blocks
                 );
        x_RegisterException(req_type, tdata->auth, ex);
    }
    catch (exception& ex)
    {
        ERR_POST("Execution error in command "
                 << ex.what() << " client=" << tdata->auth
                 << " request='" << tdata->request << "'"
                 << " peer="     << socket.GetPeerAddress()
                 << " blobsize="     << stat.blob_size
                 << " io blocks="    << stat.io_blocks
                 );
        x_RegisterInternalErr(req_type, tdata->auth);
    }
}

void CNetCacheServer::ProcessShutdown()
{    
    SetShutdownFlag();
}

void CNetCacheServer::ProcessVersion(CSocket& sock, const SNC_Request& req)
{
    WriteMsg(sock, "OK:", NETCACHED_VERSION); 
}

void CNetCacheServer::ProcessGetConfig(CSocket& sock)
{
    SOCK sk = sock.GetSOCK();
    sock.SetOwnership(eNoOwnership);
    sock.Reset(0, eTakeOwnership, eCopyTimeoutsToSOCK);

    CConn_SocketStream ios(sk);
    m_Reg.Write(ios);
}

void CNetCacheServer::ProcessGetStat(CSocket& sock, const SNC_Request& req)
{
    SOCK sk = sock.GetSOCK();
    sock.SetOwnership(eNoOwnership);
    sock.Reset(0, eTakeOwnership, eCopyTimeoutsToSOCK);

    CConn_SocketStream ios(sk);

    CBDB_Cache* bdb_cache = dynamic_cast<CBDB_Cache*>(m_Cache);
    if (!bdb_cache) {
        return;
    }
    bdb_cache->Lock();

    try {
    const SBDB_CacheStatistics& cs = bdb_cache->GetStatistics();

    // print statistics

    ios << "[bdb_stat]" << "\n\n";

    x_PrintStatistics(ios, cs);

    ios << "\n" << "[bdb_stat_hist]" << "\n\n";

    x_PrintStatisticsHistogram(ios, cs.blob_size_hist);

    ios << "\n\n";


    bool owner_stat = bdb_cache->IsOwnerStatistics();

    if (owner_stat) {
        const CBDB_Cache::TOwnerStatistics& ost = 
            bdb_cache->GetOwnerStatistics();

        ITERATE(CBDB_Cache::TOwnerStatistics, it, ost) {
            const string& oname = it->first;
            ios << "[bdb_stat_" 
                << (oname.empty() ? "noname" : oname) << "]" << "\n\n";
            x_PrintStatistics(ios, it->second);

            ios << "\n\n";

            ios << "[bdb_stat_" 
                << (oname.empty() ? "noname" : oname) << "_hist]" << "\n\n";

            x_PrintStatisticsHistogram(ios, it->second.blob_size_hist);
        }
    }


    } catch(...) {
        bdb_cache->Unlock();
        throw;
    }
    bdb_cache->Unlock();
}


void CNetCacheServer::x_PrintStatistics(CNcbiOstream&               ios, 
                                        const SBDB_CacheStatistics& cs)
{
    ios << "blobs_stored_total = "   << cs.blobs_stored_total   << "\n";
    ios << "blobs_overflow_total = " << cs.blobs_overflow_total << "\n";
    ios << "blobs_updates_total = " << cs.blobs_updates_total << "\n";
    ios << "blobs_never_read_total = " << cs.blobs_never_read_total << "\n";
    ios << "blobs_read_total = " << cs.blobs_read_total << "\n";
    ios << "blobs_expl_deleted_total = " << cs.blobs_expl_deleted_total << "\n";
    ios << "blobs_purge_deleted_total = " << cs.blobs_purge_deleted_total << "\n";
    ios << "blobs_size_total = " << unsigned(cs.blobs_size_total) << "\n";
    ios << "blob_size_max_total = " << cs.blob_size_max_total << "\n";
    ios << "blobs_db = " << cs.blobs_db << "\n";
    ios << "blobs_size_db = " << unsigned(cs.blobs_size_db) << "\n";
    ios << "err_protocol = " << cs.err_protocol << "\n";
    ios << "err_communication = " << cs.err_communication << "\n";
    ios << "err_internal = " << cs.err_internal << "\n";
    ios << "err_no_blob = " << cs.err_no_blob << "\n";
    ios << "err_blob_get = " << cs.err_blob_get << "\n";
    ios << "err_blob_put = " << cs.err_blob_put << "\n";
}

void CNetCacheServer::x_PrintStatisticsHistogram(
                        CNcbiOstream&                                   ios, 
                        const SBDB_CacheStatistics::TBlobSizeHistogram& hist)
{
    // find the histogram's end

    SBDB_CacheStatistics::TBlobSizeHistogram::const_iterator hist_end = 
        hist.end();

    ITERATE(SBDB_CacheStatistics::TBlobSizeHistogram, it, hist) {
        if (it->second > 0) {
            hist_end = it;
        }
    }

    SBDB_CacheStatistics::TBlobSizeHistogram::const_iterator it = 
        hist.begin();

    for (; it != hist.end(); ++it) {
        ios << "size  = " << it->first  << "\n";
        ios << "count = " << it->second << "\n";
        if (it == hist_end) {
            break;
        }
    }
}



void CNetCacheServer::ProcessRemove(CSocket& sock, const SNC_Request& req)
{
    const string& req_id = req.req_id;

    if (req_id.empty()) {
        WriteMsg(sock, "ERR:", "BLOB id is empty.");
        return;
    }

    CNetCache_Key blob_id(req_id);
    if (!x_CheckBlobId(sock, &blob_id, req_id))
        return;

    CIdBusyGuard guard(&m_UsedIds, blob_id.id, m_InactivityTimeout);

    m_Cache->Remove(req_id);
}

void CNetCacheServer::x_WriteBuf(CSocket& sock,
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
            {
            string msg = "Communication timeout error (cannot send)";
            msg.append(" IO block size=");
            msg.append(NStr::UIntToString(bytes));
            
            NCBI_THROW(CNetServiceException, eTimeout, msg);
            }
            break;
        case eIO_Closed:
            {
            string msg = 
                "Communication error: peer closed connection (cannot send)";
            msg.append(" IO block size=");
            msg.append(NStr::UIntToString(bytes));
            
            NCBI_THROW(CNetServiceException, eCommunicationError, msg);
            }
            break;
        case eIO_Interrupt:
            {
            string msg = 
                "Communication error: interrupt signal (cannot send)";
            msg.append(" IO block size=");
            msg.append(NStr::UIntToString(bytes));
            
            NCBI_THROW(CNetServiceException, eCommunicationError, msg);
            }
            break;
        case eIO_InvalidArg:
            {
            string msg = 
                "Communication error: invalid argument (cannot send)";
            msg.append(" IO block size=");
            msg.append(NStr::UIntToString(bytes));
            
            NCBI_THROW(CNetServiceException, eCommunicationError, msg);
            }
            break;
        default:
            {
            string msg = "Communication error (cannot send)";
            msg.append(" IO block size=");
            msg.append(NStr::UIntToString(bytes));
            
            NCBI_THROW(CNetServiceException, eCommunicationError, msg);
            }
            break;
        } // switch
        bytes -= n_written;
        buf += n_written;
    } while (bytes > 0);
}


void CNetCacheServer::ProcessGet(CSocket&               sock, 
                                 const SNC_Request&     req,
                                 SNC_ThreadData&        tdata,
                                 NetCache_RequestStat&  stat)
{
    const string& req_id = req.req_id;

    if (req_id.empty()) {
        WriteMsg(sock, "ERR:", "BLOB id is empty.");
        x_RegisterProtocolErr(req.req_type, tdata.auth);
        return;
    }

    CIdBusyGuard guard(&m_UsedIds);
    if (req.no_lock) {
        bool lock_accuired = guard.Lock(req_id, 0);
        if (!lock_accuired) {  // BLOB is locked by someone else
            WriteMsg(sock, "ERR:", "BLOB locked by another client");
            return;
        }
    } else {
        guard.Lock(req_id, m_InactivityTimeout);
    }
    char* buf = tdata.buffer.get();

    ICache::BlobAccessDescr ba_descr;
    buf += 100;
    ba_descr.buf = buf;
    ba_descr.buf_size = GetTLS_Size() - 100;

    m_Cache->GetBlobAccess(req_id, 0, kEmptyStr, &ba_descr);
    if (ba_descr.blob_size == 0) { // not found
        if (ba_descr.reader == 0) {
blob_not_found:
            string msg = "BLOB not found. ";
            msg += req_id;
            WriteMsg(sock, "ERR:", msg);
        } else {
            WriteMsg(sock, "OK:", "BLOB found. SIZE=0");
        }
        x_RegisterNoBlobErr(req.req_type, tdata.auth);
        return;
    }

    stat.blob_size = ba_descr.blob_size;

    if (ba_descr.reader == 0) {  // all in buffer
        string msg("OK:BLOB found. SIZE=");
        string sz;
        NStr::UIntToString(sz, ba_descr.blob_size);
        msg += sz;

        const char* msg_begin = msg.c_str();
        const char* msg_end = msg_begin + msg.length();

        for (; msg_end >= msg_begin; --msg_end) {
            --buf;
            *buf = *msg_end;
            ++ba_descr.blob_size;
        }

        // translate BLOB fragment to the network
        CStopWatch  sw(true);

        x_WriteBuf(sock, buf, ba_descr.blob_size);

        stat.comm_elapsed += sw.Elapsed();
        ++stat.io_blocks;

        return;

    } // inline BLOB


    // re-translate reader to the network

    auto_ptr<IReader> rdr(ba_descr.reader);
    if (!rdr.get()) {
        goto blob_not_found;
    }
    size_t blob_size = ba_descr.blob_size;

    bool read_flag = false;
    
    buf = tdata.buffer.get();

    size_t bytes_read;
    do {
        ERW_Result io_res = rdr->Read(buf, GetTLS_Size(), &bytes_read);
        if (io_res == eRW_Success && bytes_read) {
            if (!read_flag) {
                read_flag = true;
                string msg("BLOB found. SIZE=");
                string sz;
                NStr::UIntToString(sz, blob_size);
                msg += sz;
                WriteMsg(sock, "OK:", msg);
            }

            // translate BLOB fragment to the network
            CStopWatch  sw(true);

            x_WriteBuf(sock, buf, bytes_read);

            stat.comm_elapsed += sw.Elapsed();
            ++stat.io_blocks;

        } else {
            break;
        }
    } while(1);
    if (!read_flag) {
        goto blob_not_found;
    }

}

void CNetCacheServer::ProcessPut(CSocket&              sock, 
                                 SNC_Request&          req,
                                 SNC_ThreadData&       tdata,
                                 NetCache_RequestStat& stat)
{
    string& rid = req.req_id;
//    CNetCache_Key blob_id;

    CIdBusyGuard guard(&m_UsedIds);

    if (!req.req_id.empty()) {  // UPDATE request
        guard.Lock(req.req_id, m_InactivityTimeout * 2);
    } else {
        guard.LockNewId(&m_MaxId, m_InactivityTimeout);
        unsigned int id = guard.GetId();
        CNetCache_Key::GenerateBlobKey(&rid, id, m_Host, GetPort());
        //CNetCache_GenerateBlobKey(&rid, id, m_Host, GetPort());
    }


    WriteMsg(sock, "ID:", rid);

    auto_ptr<IWriter> iwrt;

    // Reconfigure socket for no-timeout operation

    STimeout to;
    to.sec = to.usec = 0;
    sock.SetTimeout(eIO_Read, &to);
    char* buf = tdata.buffer.get();
    size_t buf_size = GetTLS_Size();

    bool not_eof;

    do {
        size_t nn_read;

        CStopWatch  sw(true);

        not_eof = ReadBuffer(sock, buf, buf_size, &nn_read);

        if (nn_read == 0 && !not_eof) {
            m_Cache->Store(rid, 0, kEmptyStr, 
                           buf, nn_read, req.timeout, tdata.auth);
            break;
        }

        stat.comm_elapsed += sw.Elapsed();
        stat.blob_size += nn_read;

        if (nn_read) {
            if (iwrt.get() == 0) { // first read

                if (not_eof == false) { // complete read
                    m_Cache->Store(rid, 0, kEmptyStr, 
                                   buf, nn_read, req.timeout, tdata.auth);
                    return;
                }

                iwrt.reset(
                    m_Cache->GetWriteStream(rid, 0, kEmptyStr, 
                                            req.timeout, tdata.auth));
            }
            size_t bytes_written;
            ERW_Result res = 
                iwrt->Write(buf, nn_read, &bytes_written);
            if (res != eRW_Success) {
                WriteMsg(sock, "ERR:", "Server I/O error");
                x_RegisterInternalErr(req.req_type, tdata.auth);
                return;
            }
        } // if (nn_read)

    } while (not_eof);

    if (iwrt.get()) {
        iwrt->Flush();
        iwrt.reset(0);
    }
}


void CNetCacheServer::ProcessPut2(CSocket&              sock, 
                                  SNC_Request&          req,
                                  SNC_ThreadData&       tdata,
                                  NetCache_RequestStat& stat)
{
    string& rid = req.req_id;

    CIdBusyGuard guard(&m_UsedIds);

    if (!req.req_id.empty()) {  // UPDATE request
        guard.Lock(req.req_id, m_InactivityTimeout);
    } else {
        guard.LockNewId(&m_MaxId, m_InactivityTimeout);
        unsigned int id = guard.GetId();
        CNetCache_Key::GenerateBlobKey(&rid, id, m_Host, GetPort());
    }


    WriteMsg(sock, "ID:", rid);


    auto_ptr<IWriter> iwrt;
    char* buf = tdata.buffer.get();
    size_t buf_size = GetTLS_Size();

    bool not_eof;

    CSocketReaderWriter  comm_reader(&sock, eNoOwnership);
    CTransmissionReader  transm_reader(&comm_reader, eNoOwnership);

    do {
        size_t nn_read = 0;

        CStopWatch  sw(true);

        not_eof = ReadBuffer(sock, &transm_reader, buf, buf_size, &nn_read);

        stat.comm_elapsed += sw.Elapsed();
        stat.blob_size += nn_read;
        

        if (nn_read == 0 && !not_eof) {
            m_Cache->Store(rid, 0, kEmptyStr, 
                            buf, nn_read, req.timeout, tdata.auth);
            break;
        }

        if (nn_read) {
            if (iwrt.get() == 0) { // first read

                if (not_eof == false) { // complete read
                    m_Cache->Store(rid, 0, kEmptyStr, 
                                   buf, nn_read, req.timeout, tdata.auth);
                    break;
                }

                iwrt.reset(
                    m_Cache->GetWriteStream(rid, 0, kEmptyStr, 
                                            req.timeout, tdata.auth));
            }
            size_t bytes_written;
            ERW_Result res = 
                iwrt->Write(buf, nn_read, &bytes_written);
            if (res != eRW_Success) {
                WriteMsg(sock, "Err:", "Server I/O error");
                x_RegisterInternalErr(req.req_type, tdata.auth);
                return;
            }
        } // if (nn_read)

    } while (not_eof);

    if (iwrt.get()) {
        iwrt->Flush();
        iwrt.reset(0);
    }

}

void CNetCacheServer::ProcessIsLock(CSocket& sock, const SNC_Request& req)
{
    const string& req_id = req.req_id;

    if (req_id.empty()) {
        WriteMsg(sock, "ERR:", "BLOB id is empty.");
        return;
    }
    CNetCache_Key blob_id(req_id);
    if (!x_CheckBlobId(sock, &blob_id, req_id))
        return;

    CIdBusyGuard guard(&m_UsedIds);
    if (guard.IsLocked(blob_id.id)) {
        WriteMsg(sock, "OK:", "1");
    } else {
        WriteMsg(sock, "OK:", "0");
    }

}



void CNetCacheServer::WriteMsg(CSocket&       sock, 
                               const string&  prefix, 
                               const string&  msg)
{
    string err_msg(prefix);
    err_msg.append(msg);
    err_msg.append("\r\n");
    size_t n_written;
    /* EIO_Status io_st = */
        sock.Write(err_msg.c_str(), err_msg.length(), &n_written);
}

void CNetCacheServer::ParseRequest(const string& reqstr, SNC_Request* req)
{
    const char* s = reqstr.c_str();

    if (strncmp(s, "PUT2", 4) == 0) {
        req->req_type = ePut2;
        req->timeout = 0;
        req->req_id.erase();

        s += 4;
        goto put_args_parse;

    } // PUT2

    if (strncmp(s, "PUT", 3) == 0) {
        req->req_type = ePut;
        req->timeout = 0;
        req->req_id.erase();

        s += 3;
put_args_parse:
        while (*s && isspace((unsigned char)(*s))) {
            ++s;
        }

        if (*s) {  // timeout value
            int time_out = atoi(s);
            if (time_out > 0) {
                req->timeout = time_out;
            }
        }
        while (*s && isdigit((unsigned char)(*s))) {
            ++s;
        }
        while (*s && isspace((unsigned char)(*s))) {
            ++s;
        }
        req->req_id = s;

        return;
    } // PUT

    if (strncmp(s, "GETCONF", 7) == 0) {
        req->req_type = eGetConfig;
        s += 7;
        return;
    } // GETCONF

    if (strncmp(s, "GETSTAT", 7) == 0) {
        req->req_type = eGetStat;
        s += 7;
        return;
    } // GETCONF

    if (strncmp(s, "ISLK", 4) == 0) {
        req->req_type = eIsLock;
        s += 4;

parse_blob_id:
        while (*s && isspace((unsigned char)(*s))) {
            ++s;
        }

        req->req_id = s;
        return;
    }

    if (strncmp(s, "GET", 3) == 0) {
        req->req_type = eGet;
        s += 3;

        // parse blob id
        while (*s && isspace((unsigned char)(*s))) {
            ++s;
        }

        req->req_id.erase();

        // skip blob id
        while (*s && !isspace((unsigned char)(*s))) {
            char ch = *s;
            req->req_id.append(1, ch);
            ++s;
        }

        if (!*s) {
            return;
        }

        // skip whitespace
        while (*s && !isspace((unsigned char)(*s))) {
            ++s;
        }

        if (!*s) {
            return;
        }

        // NW modificator (no wait request)
        if (s[0] == 'N' && s[1] == 'W') {
            req->no_lock = true;
        }

        return;
    } // GET

    if (strncmp(s, "REMOVE", 3) == 0) {
        req->req_type = eRemove;
        s += 6;
        goto parse_blob_id;
    } // REMOVE

    if (strncmp(s, "SHUTDOWN", 7) == 0) {
        req->req_type = eShutdown;
        return;
    } // SHUTDOWN

    if (strncmp(s, "VERSION", 7) == 0) {
        req->req_type = eVersion;
        return;
    } // VERSION

    if (strncmp(s, "LOG", 3) == 0) {
        req->req_type = eLogging;
        s += 3;
        goto parse_blob_id;  // "ON/OFF" instead of blob_id in this case
    } // LOG


    req->req_type = eError;
    req->err_msg = "Unknown request";
}

bool CNetCacheServer::ReadBuffer(CSocket& sock,
                                 IReader* rdr, 
                                 char*    buf, 
                                 size_t   buf_size,
                                 size_t*  read_length)
{
    *read_length = 0;
    size_t nn_read = 0;

    bool ret_flag = true;

    while (ret_flag) {
        if (!s_WaitForReadSocket(sock, m_InactivityTimeout)) {
            break;
        }

        ERW_Result io_res = rdr->Read(buf, buf_size, &nn_read);
        switch (io_res) 
        {
        case eRW_Success:
            break;
        case eRW_Timeout:
            if (*read_length == 0) {
                NCBI_THROW(CNetServiceException, eTimeout, "IReader timeout");
            }
            break;
        case eRW_Eof:
            ret_flag = false;
            break;
        default: // invalid socket or request
            NCBI_THROW(CNetServiceException, eCommunicationError, kEmptyStr);

        } // switch
        *read_length += nn_read;
        buf_size -= nn_read;

        if (buf_size <= 10) {  // buffer too small to read again
            break;
        }
        buf += nn_read;
    }

    return ret_flag;  // false means we hit "eIO_Closed"

}


bool CNetCacheServer::ReadBuffer(CSocket& sock, 
                                 char*    buf, 
                                 size_t   buf_size,
                                 size_t*  read_length)
{
    *read_length = 0;
    size_t nn_read = 0;

    bool ret_flag = true;

    while (ret_flag) {
        if (!s_WaitForReadSocket(sock, m_InactivityTimeout)) {
            break;
        }

        EIO_Status io_st = sock.Read(buf, buf_size, &nn_read);
        *read_length += nn_read;
        switch (io_st) 
        {
        case eIO_Success:
            break;
        case eIO_Timeout:
            if (*read_length == 0) {
                NCBI_THROW(CNetServiceException, eTimeout, kEmptyStr);
            }
            break;
        case eIO_Closed:
            ret_flag = false;
            break;
        default: // invalid socket or request
            NCBI_THROW(CNetServiceException, eCommunicationError, kEmptyStr);
        };
        buf_size -= nn_read;

        if (buf_size <= 10) {  // buffer too small to read again
            break;
        }
        buf += nn_read;
    }

    return ret_flag;  // false means we hit "eIO_Closed"

}



bool CNetCacheServer::ReadStr(CSocket& sock, string* str)
{
    _ASSERT(str);

    str->erase();
    char ch;
    EIO_Status io_st;

    char szBuf[1024] = {0,};
    unsigned str_len = 0;
    size_t n_read = 0;

    for (bool flag = true; flag; ) {
        io_st = sock.Read(szBuf, 256, &n_read, eIO_ReadPeek);
        switch (io_st) 
        {
        case eIO_Success:
            flag = false;
            break;
        case eIO_Timeout:
            NCBI_THROW(CNetServiceException, eTimeout, kEmptyStr);
            break;
        default: // invalid socket or request, bailing out
            return false;
        };
    }

    for (str_len = 0; str_len < n_read; ++str_len) {
        ch = szBuf[str_len];
        if (ch == 0)
            break;
        if (ch == '\n' || ch == '\r') {
            // analyse next char for \r\n sequence
            ++str_len;
            ch = szBuf[str_len];
            if (ch == '\n' || ch == '\r') {} else {--str_len;}
            break;
        }
        *str += ch;
    }

    if (str_len == 0) {
        return false;
    }
    io_st = sock.Read(szBuf, str_len + 1);
    return true;
}


void CNetCacheServer::GenerateRequestId(const SNC_Request& req, 
                                        string*            id_str,
                                        unsigned int*      transaction_id)
{
    long id;
    {{
    CFastMutexGuard guard(x_NetCacheMutex);
    id = ++m_MaxId;
    }}
    *transaction_id = id;

    CNetCache_Key::GenerateBlobKey(id_str, id, m_Host, GetPort());
}


bool CNetCacheServer::x_CheckBlobId(CSocket&       sock,
                                    CNetCache_Key* blob_id, 
                                    const string&  blob_key)
{
    try {
        //CNetCache_ParseBlobKey(blob_id, blob_key);
        if (blob_id->version != 1     ||
            blob_id->hostname.empty() ||
            blob_id->id == 0          ||
            blob_id->port == 0) 
        {
            NCBI_THROW(CNetCacheException, eKeyFormatError, kEmptyStr);
        }
    } 
    catch (CNetCacheException& )
    {
        WriteMsg(sock, "ERR:", "BLOB id format error.");
        return false;
    }
    return true;
}

void CNetCacheServer::ProcessLog(CSocket&  sock, const SNC_Request&  req)
{
    const char* str = req.req_id.c_str();
    if (NStr::strcasecmp(str, "ON")==0) {
        CFastMutexGuard guard(x_NetCacheMutex);
        if (!m_LogFlag) {
            m_LogFlag = true;
            m_Logger->Rotate();
        }
    }
    if (NStr::strcasecmp(str, "OFF")==0) {
        CFastMutexGuard guard(x_NetCacheMutex);
        m_LogFlag = false;
    }
}


CNetCache_Logger* CNetCacheServer::GetLogger()
{
    CFastMutexGuard guard(x_NetCacheMutex);

    return m_Logger.get();
}

bool CNetCacheServer::IsLog() const
{
    CFastMutexGuard guard(x_NetCacheMutex);
    return m_LogFlag;
}

void CNetCacheServer::x_CreateLog()
{
    CFastMutexGuard guard(x_NetCacheMutex);

    if (m_Logger.get()) {
        return; // nothing to do
    }
    m_Logger.reset(
        new CNetCache_Logger("netcached.log", 100 * 1024 * 1024));
}

SNC_ThreadData* CNetCacheServer::x_GetThreadData()
{
    SNC_ThreadData* tdata = s_tls->GetValue();
    if (tdata) {
    } else {
        tdata = new SNC_ThreadData(GetTLS_Size());
        s_tls->SetValue(tdata, TlsCleanup);
    }
    return tdata;
}

static
SBDB_CacheStatistics::EErrGetPut s_GetStatType(ENC_RequestType   req_type)
{
    SBDB_CacheStatistics::EErrGetPut op;
    switch (req_type) {
    case ePut:
    case ePut2:
        op = SBDB_CacheStatistics::eErr_Put;
        break;
    case eGet:
        op = SBDB_CacheStatistics::eErr_Get;
        break;
    default:
        op = SBDB_CacheStatistics::eErr_Unknown;
        break;
    }
    return op;
}

void CNetCacheServer::x_RegisterProtocolErr(
                               ENC_RequestType   req_type,
                               const string&     auth)
{
    CBDB_Cache* bdb_cache = dynamic_cast<CBDB_Cache*>(m_Cache);
    if (!bdb_cache) {
        return;
    }
    SBDB_CacheStatistics::EErrGetPut op = s_GetStatType(req_type);
    bdb_cache->RegisterProtocolError(op, auth);
}

void CNetCacheServer::x_RegisterInternalErr(
                               ENC_RequestType   req_type,
                               const string&     auth)
{
    CBDB_Cache* bdb_cache = dynamic_cast<CBDB_Cache*>(m_Cache);
    if (!bdb_cache) {
        return;
    }
    SBDB_CacheStatistics::EErrGetPut op = s_GetStatType(req_type);
    bdb_cache->RegisterInternalError(op, auth);
}

void CNetCacheServer::x_RegisterNoBlobErr(ENC_RequestType   req_type,
                                          const string&     auth)
{
    CBDB_Cache* bdb_cache = dynamic_cast<CBDB_Cache*>(m_Cache);
    if (!bdb_cache) {
        return;
    }
    SBDB_CacheStatistics::EErrGetPut op = s_GetStatType(req_type);
    bdb_cache->RegisterNoBlobError(op, auth);
}

void CNetCacheServer::x_RegisterException(ENC_RequestType           req_type,
                                          const string&             auth,
                                          const CNetServiceException& ex)
{
    CBDB_Cache* bdb_cache = dynamic_cast<CBDB_Cache*>(m_Cache);
    if (!bdb_cache) {
        return;
    }
    SBDB_CacheStatistics::EErrGetPut op = s_GetStatType(req_type);
    switch (ex.GetErrCode()) {
    case CNetServiceException::eTimeout:
    case CNetServiceException::eCommunicationError:
        bdb_cache->RegisterCommError(op, auth);
        break;
    default:
        ERR_POST("Unknown err code in CNetCacheServer::x_RegisterException");
        bdb_cache->RegisterInternalError(op, auth);
        break;
    } // switch
}


void CNetCacheServer::x_RegisterException(ENC_RequestType           req_type,
                                          const string&             auth,
                                          const CNetCacheException& ex)
{
    CBDB_Cache* bdb_cache = dynamic_cast<CBDB_Cache*>(m_Cache);
    if (!bdb_cache) {
        return;
    }
    SBDB_CacheStatistics::EErrGetPut op = s_GetStatType(req_type);

    switch (ex.GetErrCode()) {
    case CNetCacheException::eAuthenticationError:
    case CNetCacheException::eKeyFormatError:
        bdb_cache->RegisterProtocolError(op, auth);
        break;
    case CNetCacheException::eServerError:
        bdb_cache->RegisterInternalError(op, auth);
        break;
    default:
        ERR_POST("Unknown err code in CNetCacheServer::x_RegisterException");
        bdb_cache->RegisterInternalError(op, auth);
        break;
    } // switch
}


///////////////////////////////////////////////////////////////////////


/// NetCache daemon application
///
/// @internal
///
class CNetCacheDApp : public CNcbiApplication
{
public:
    void Init(void);
    int Run(void);
private:
    CRef<CCacheCleanerThread>  m_PurgeThread;
    /// Error message logging
    auto_ptr<CNetCacheLogStream> m_ErrLog;
};

void CNetCacheDApp::Init(void)
{
    SetDiagPostLevel(eDiag_Info);
    SetDiagPostFlag(eDPF_DateTime);

    // Setup command line arguments and parameters
        
    // Create command-line argument descriptions class
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // Specify USAGE context
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "netcached");

    arg_desc->AddFlag("reinit", "Recreate the storage directory.");

    SetupArgDescriptions(arg_desc.release());
}


/// @internal
extern "C" 
void Threaded_Server_SignalHandler( int )
{
    if (s_netcache_server && 
        (!s_netcache_server->ShutdownRequested()) ) {
        s_netcache_server->SetShutdownFlag();
        LOG_POST("Interrupt signal. Shutdown requested.");
    }
}



int CNetCacheDApp::Run(void)
{
    CBDB_Cache* bdb_cache = 0;
    CArgs args = GetArgs();

    LOG_POST(NETCACHED_VERSION);

    try {
        const CNcbiRegistry& reg = GetConfig();

        CConfig conf(reg);
        
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
        m_ErrLog.reset(new CNetCacheLogStream("nc_err.log", 25 * 1024 * 1024));
        // All errors redirected to rotated log
        // from this moment on the server is silent...
        SetDiagStream(m_ErrLog.get());

        const CConfig::TParamTree* param_tree = conf.GetTree();
        const TPluginManagerParamTree* bdb_tree = 
            param_tree->FindSubNode(kBDBCacheDriverName);


        auto_ptr<ICache> cache;

        if (bdb_tree) {

            CConfig bdb_conf((CConfig::TParamTree*)bdb_tree, eNoOwnership);
            const string& db_path = 
                bdb_conf.GetString("netcached", "path", 
                                    CConfig::eErr_Throw, kEmptyStr);
            bool reinit =
            reg.GetBool("server", "reinit", false, 0, CNcbiRegistry::eReturn);

            if (args["reinit"] || reinit) {  // Drop the database directory
                CDir dir(db_path);
                dir.Remove();
            }
            

            LOG_POST("Initializing BDB cache");

            typedef CPluginManager<ICache> TCachePM;
            CPluginManager<ICache> pm_cache;
            pm_cache.RegisterWithEntryPoint(NCBI_EntryPoint_xcache_bdb);

            ICache* ic;

            try {
                ic = 
                    pm_cache.CreateInstance(
                        kBDBCacheDriverName,
                        TCachePM::GetDefaultDrvVers(),
                        bdb_tree);
            } 
            catch (CBDB_Exception& ex)
            {
                bool drop_db = reg.GetBool("server", "drop_db", 
                                           true, 0, CNcbiRegistry::eReturn);
                if (drop_db) {
                    LOG_POST("Error initializing BDB ICache interface.");
                    LOG_POST(ex.what());
                    LOG_POST("Database directory will be droppped.");

                    CDir dir(db_path);
                    dir.Remove();

                    ic = 
                      pm_cache.CreateInstance(
                        kBDBCacheDriverName,
                        TCachePM::GetDefaultDrvVers(),
                        bdb_tree);

                } else {
                    throw;
                }
            }

            bdb_cache = dynamic_cast<CBDB_Cache*>(ic);
            cache.reset(ic);

            if (bdb_cache) {
                bdb_cache->CleanLog();
            }

        } else {
            string msg = 
                "Configuration error. Cannot init storage. Driver name:";
            msg += kBDBCacheDriverName;
            ERR_POST(msg);
            return 1;
        }

        // cache storage media has been created

        if (!cache->IsOpen()) {
            ERR_POST("Configuration error. Cache not open.");
            return 1;
        }

        int port = 
            reg.GetInt("server", "port", 9000, 0, CNcbiRegistry::eReturn);

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
        } else {
            LOG_POST("Network IO timeout " << network_timeout);
        }

        bool is_log =
            reg.GetBool("server", "log", false, 0, CNcbiRegistry::eReturn);
        unsigned tls_size =
            reg.GetInt("server", "tls_size", 64 * 1024, 0, CNcbiRegistry::eReturn);

        auto_ptr<CNetCacheServer> thr_srv(
            new CNetCacheServer(port, 
                                cache.get(), 
                                max_threads, 
                                init_threads,
                                network_timeout,
                                is_log,
                                reg));

        thr_srv->SetTLS_Size(tls_size);

        LOG_POST(Info << "Running server on port " << port);
        thr_srv->Run();
    }
    catch (CBDB_ErrnoException& ex)
    {
        NcbiCerr << "Error: DBD errno exception:" << ex.what();
        return 1;
    }
    catch (CBDB_LibException& ex)
    {
        NcbiCerr << "Error: DBD library exception:" << ex.what();
        return 1;
    }

    return 0;
}





int main(int argc, const char* argv[])
{

    return CNetCacheDApp().AppMain(argc, argv, 0, eDS_Default, "netcached.ini");
}

/*
 * ===========================================================================
 * $Log$
 * Revision 1.67  2005/10/25 18:11:54  kuznets
 * Implemented no-wait-lock mode for GET
 *
 * Revision 1.66  2005/10/25 14:29:59  kuznets
 * Implemented BLOB lock detection
 *
 * Revision 1.65  2005/09/21 18:22:21  kuznets
 * Reflected changes in client API
 *
 * Revision 1.64  2005/08/10 19:19:14  kuznets
 * Set accept timeout 1sec
 *
 * Revision 1.63  2005/08/08 17:45:13  kuznets
 * Improved error logging
 *
 * Revision 1.62  2005/08/08 14:55:34  kuznets
 * Improved statistics and logging
 *
 * Revision 1.61  2005/08/01 16:53:10  kuznets
 * Added BDB statistics
 *
 * Revision 1.60  2005/07/28 12:53:35  ssikorsk
 * Replaced TInterfaceVersion traits with GetDefaultDrvVers() method call.
 *
 * Revision 1.59  2005/07/27 18:16:18  kuznets
 * Added GETCONF command
 *
 * Revision 1.58  2005/07/05 13:25:22  kuznets
 * Improved error messaging
 *
 * Revision 1.57  2005/06/03 16:27:15  lavr
 * Explicit (unsigned char) casts in ctype routines
 *
 * Revision 1.56  2005/05/02 17:42:57  kuznets
 * Added support of reinit flag in the ini file
 *
 * Revision 1.55  2005/04/26 18:53:50  kuznets
 * Added optional daemonization
 *
 * Revision 1.54  2005/04/25 16:42:59  kuznets
 * Fixed err message transmission
 *
 * Revision 1.53  2005/04/25 16:35:08  kuznets
 * Send a short error message when we hit thread pool overflow
 *
 * Revision 1.52  2005/04/25 15:37:58  kuznets
 * Improved logging
 *
 * Revision 1.51  2005/04/19 14:19:09  kuznets
 * New protocol for puit blob
 *
 * Revision 1.50  2005/03/31 19:31:15  kuznets
 * Corrected use of preprocessor
 *
 * Revision 1.49  2005/03/31 19:16:49  kuznets
 * Added build date to the version string
 *
 * Revision 1.48  2005/03/24 20:20:00  kuznets
 * Suppressed communication error (carries no information anyway)
 *
 * Revision 1.47  2005/03/24 01:23:44  vakatov
 * Fix accidental mix-up of 'flags' vs 'action' arg in calls to
 * CNcbiRegistry::Get*()
 *
 * Revision 1.46  2005/03/22 18:55:18  kuznets
 * Reflecting changes in connect library layout
 *
 * Revision 1.45  2005/03/21 20:12:05  kuznets
 * Fixed race condition with setting accept timeout
 *
 * Revision 1.44  2005/03/17 21:34:24  kuznets
 * Use CFastLocalTime
 *
 * Revision 1.43  2005/02/17 16:15:27  kuznets
 * Added ProcessOverflow with ERR_POST
 *
 * Revision 1.42  2005/02/15 15:16:50  kuznets
 * Rotated log stream, overloaded x_BackupName to self-rotate
 *
 * Revision 1.41  2005/02/09 19:36:39  kuznets
 * Restored accidentally removed SetTimeout()
 *
 * Revision 1.40  2005/02/09 19:34:08  kuznets
 * Trail buffer read using socket.Read(NULL, ...)
 *
 * Revision 1.39  2005/02/09 13:36:14  kuznets
 * Limit log size to 100M, rotate log every time its turned ON
 *
 * Revision 1.38  2005/02/07 18:55:40  kuznets
 * REmoved port number from the connection log
 *
 * Revision 1.37  2005/02/07 13:06:37  kuznets
 * Logging improvements
 *
 * Revision 1.36  2005/01/24 17:21:40  vasilche
 * Removed redundant comparison "bool != false".
 *
 * Revision 1.35  2005/01/05 15:34:51  kuznets
 * Fast shutdown through low small accept timeout, restored signal procesing
 *
 * Revision 1.34  2005/01/04 18:55:30  kuznets
 * Commented out signal handlers
 *
 * Revision 1.33  2005/01/04 17:33:34  kuznets
 * Added graceful shutdown on SIGTERM(unix)
 *
 * Revision 1.32  2005/01/03 14:29:51  kuznets
 * Improved logging
 *
 * Revision 1.31  2004/12/29 15:35:37  kuznets
 * Fixed bug in comm. protocol
 *
 * Revision 1.29  2004/12/27 19:14:07  kuznets
 * Use NStr::strcasecmp instead of stricmp
 *
 * Revision 1.28  2004/12/27 16:31:32  kuznets
 * Implemented server side logging
 *
 * Revision 1.27  2004/12/22 21:02:53  grichenk
 * BDB and DBAPI caches split into separate libs.
 * Added entry point registration, fixed driver names.
 *
 * Revision 1.26  2004/12/22 14:36:13  kuznets
 * Performance optimization (ProcessGet)
 *
 * Revision 1.25  2004/11/08 16:02:53  kuznets
 * BLOB timeout passed to ICache
 *
 * Revision 1.24  2004/11/01 16:16:02  kuznets
 * Use NStr instead of itoa
 *
 * Revision 1.23  2004/11/01 16:03:39  kuznets
 * Added blob size to GET response
 *
 * Revision 1.22  2004/11/01 14:40:24  kuznets
 * Implemented BLOB update, fixed bug in object locking
 *
 * Revision 1.21  2004/10/28 16:14:42  kuznets
 * Implemented REMOVE
 *
 * Revision 1.20  2004/10/27 17:08:57  kuznets
 * Purge thread has been delegated to CBDB_Cache
 *
 * Revision 1.19  2004/10/27 14:18:02  kuznets
 * BLOB key parser moved from netcached
 *
 * Revision 1.18  2004/10/26 14:21:41  kuznets
 * New parameter network_timeout
 *
 * Revision 1.17  2004/10/26 13:36:27  kuznets
 * new startup flag -reinit and drop_db ini parameter
 *
 * Revision 1.16  2004/10/25 16:06:18  kuznets
 * Better timeout handling, use larger network bufers, VERSION command
 *
 * Revision 1.15  2004/10/21 17:21:42  kuznets
 * Reallocated buffer replaced with TLS data
 *
 * Revision 1.14  2004/10/21 15:51:21  kuznets
 * removed unused variable
 *
 * Revision 1.13  2004/10/21 15:06:10  kuznets
 * New parameter run_purge_thread
 *
 * Revision 1.12  2004/10/21 13:39:27  kuznets
 * New parameters max_threads, init_threads
 *
 * Revision 1.11  2004/10/20 21:21:02  ucko
 * Make CNetCacheServer::ERequestType public so that struct Request can
 * make use of it when building with WorkShop.
 *
 * Revision 1.10  2004/10/20 14:50:22  kuznets
 * Code cleanup
 *
 * Revision 1.9  2004/10/20 13:45:37  kuznets
 * Disabled TCP/IP delay on write
 *
 * Revision 1.8  2004/10/19 18:20:26  kuznets
 * Use ReadPeek to avoid net delays
 *
 * Revision 1.7  2004/10/19 15:53:36  kuznets
 * Code cleanup
 *
 * Revision 1.6  2004/10/18 13:46:57  kuznets
 * Removed common buffer (was shared between threads)
 *
 * Revision 1.5  2004/10/15 14:34:46  kuznets
 * Fine tuning of networking, cleaning thread interruption, etc
 *
 * Revision 1.4  2004/10/04 12:33:24  kuznets
 * Complete implementation of GET/PUT
 *
 * Revision 1.3  2004/09/23 16:37:21  kuznets
 * Reflected changes in ncbi_config.hpp
 *
 * Revision 1.2  2004/09/23 14:17:36  kuznets
 * Fixed compilation bug
 *
 * Revision 1.1  2004/09/23 13:57:01  kuznets
 * Initial revision
 *
 *
 * ===========================================================================
 */
