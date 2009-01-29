#ifndef CONN___NETCACHE_API__HPP
#define CONN___NETCACHE_API__HPP

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
 * Authors:  Anatoliy Kuznetsov, Maxim Didenko
 *
 * File Description:
 *   Net cache client API.
 *
 */

/// @file netcache_client.hpp
/// NetCache client specs.
///

#include "netcache_admin.hpp"

#include <connect/services/netservice_api.hpp>
#include <connect/services/netcache_api_expt.hpp>
#include <connect/services/netcache_key.hpp>

#include <util/simple_buffer.hpp>

#include <corelib/plugin_manager.hpp>
#include <corelib/reader_writer.hpp>


BEGIN_NCBI_SCOPE


/** @addtogroup NetCacheClient
 *
 * @{
 */


struct SNetCacheAPIImpl;

/// Client API for NetCache server
///
/// After any Put, Get transactions connection socket
/// is closed (a part of the NetCache protocol implementation).
///
///
class NCBI_XCONNECT_EXPORT CNetCacheAPI
{
    NET_COMPONENT(NetCacheAPI);

    explicit CNetCacheAPI(const std::string& client_name);

    /// Construct client, working with the specified service
    CNetCacheAPI(const std::string& service_name,
        const std::string& client_name,
        const std::string& lbsm_affinity_name = kEmptyStr);

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
    IWriter* PutData(string* key, unsigned int  time_to_live = 0);

    /// Update an existing BLOB
    ///
    string PutData(const string& key,
                   const void*   buf,
                   size_t        size,
                   unsigned int  time_to_live = 0);

    /// Check if the BLOB identified by the key "key" exists.
    ///
    /// @param key
    ///    Key of the BLOB to check for existence.
    ///
    /// @return
    ///    True, if the BLOB exists; false otherwise.
    bool HasBlob(const string& key);

    /// Returns the size of the BLOB identified by the "key" parameter.
    ///
    /// @param key
    ///    The key of the BLOB the size of which to be returned.
    ///
    /// @return
    ///    Size of the BLOB in bytes.
    size_t GetBlobSize(const string& key);

    /// BLOB locking mode
    enum ELockMode {
        eLockWait,   ///< waits for BLOB to become available
        eLockNoWait  ///< throws an exception immediately if BLOB locked
    };

    /// Retrieve BLOB from server by key
    /// If BLOB not found method returns NULL
    //
    /// Caller is responsible for deletion of the IReader*
    /// IReader* MUST be deleted before destruction of CNetCacheAPI.
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
    /// @param lock_mode
    ///    Blob locking mode
    /// @return
    ///    IReader* (caller must delete this).
    ///    NULL means that BLOB was not found (expired).
    IReader* GetData(const string& key,
                     size_t*       blob_size = 0,
                     ELockMode     lock_mode = eLockWait);

    /// Status of GetData() call
    /// @sa GetData
    enum EReadResult {
        eReadComplete, ///< The whole BLOB has been read
        eNotFound,     ///< BLOB not found or error
        eReadPart      ///< Read part of the BLOB (buffer capacity)
    };

    /// Retrieve BLOB from server by key
    ///
    /// @note
    ///    Function waits for enough data to arrive.
    EReadResult GetData(const string&  key,
                        void*          buf,
                        size_t         buf_size,
                        size_t*        n_read    = 0,
                        size_t*        blob_size = 0);

    /// Retrieve BLOB from server by key
    /// This method retrieves BLOB size, allocates memory and gets all
    /// the data from the server.
    ///
    /// Blob size and binary data is placed into blob_to_read structure.
    /// Do not use this method if you are not sure you have memory
    /// to load the whole BLOB.
    ///
    /// @return
    ///    eReadComplete if BLOB found (eNotFound otherwise)
    EReadResult GetData(const string& key, CSimpleBuffer& buffer);


    /// NetCache server locks BLOB so only one client can
    /// work with one BLOB at a time. Method returns TRUE
    /// if BLOB is locked at the moment.
    ///
    /// @return TRUE if BLOB exists and locked by another client
    /// FALSE if BLOB not found or not locked
    bool IsLocked(const string& key);

    /// Retrieve BLOB's owner information as registered by the server
    string GetOwner(const string& key);

    /// Remove BLOB by key
    void Remove(const string& key);


    CNetCacheAdmin GetAdmin();

    CNetService GetService();

    void SetCommunicationTimeout(const STimeout& to)
        {GetService().SetCommunicationTimeout(to);}

    /// Connection management options
    enum EConnectionMode {
        /// Close connection after each call.
        /// This mode frees server side resources, but reconnection can be
        /// costly because of the network overhead
        eCloseConnection,

        /// Keep connection open (default).
        /// This mode occupies server side resources(session thread),
        /// use this mode very carefully
        eKeepConnection
    };

    /// Set connection mode
    /// @sa GetConnMode
    void SetConnMode(EConnectionMode conn_mode)
    {
        GetService().SetPermanentConnection(
            conn_mode == eCloseConnection ? eOff : eOn);
    }
};

NCBI_DECLARE_INTERFACE_VERSION(SNetCacheAPIImpl,  "xnetcacheapi", 1, 1, 0);

extern NCBI_XCONNECT_EXPORT const char* kNetCacheAPIDriverName;

//extern "C" {

void NCBI_XCONNECT_EXPORT NCBI_EntryPoint_xnetcacheapi(
     CPluginManager<SNetCacheAPIImpl>::TDriverInfoList&   info_list,
     CPluginManager<SNetCacheAPIImpl>::EEntryPointRequest method);


//} // extern C


/* @} */


END_NCBI_SCOPE

#endif  /* CONN___NETCACHE_API__HPP */
