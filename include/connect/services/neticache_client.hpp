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
 * Authors:  Anatoliy Kuznetsov
 *
 * File Description:
 *   Network client for ICache (NetCache).
 *
 */

/// @file neticache_client.hpp
/// NetCache ICache client specs. 
///

#include <connect/connect_export.h>
#include <connect/ncbi_types.h>
#include <connect/ncbi_conn_reader_writer.hpp>
#include <connect/services/netservice_client.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/plugin_manager.hpp>
#include <corelib/ncbimisc.hpp>
#include <corelib/version.hpp>

#include <util/reader_writer.hpp>
#include <util/transmissionrw.hpp>
#include <util/cache/icache.hpp>


BEGIN_NCBI_SCOPE

class NCBI_NET_CACHE_EXPORT CNetICacheClient : public CNetServiceClient,
                                              public ICache
{
public:
    CNetICacheClient();
    CNetICacheClient(const string&  host,
                     unsigned short port,
                     const string&  cache_name,
                     const string&  client_name);

    virtual ~CNetICacheClient();

    void SetConnectionParams(const string&  host,
                             unsigned short port,
                             const string&  cache_name,
                             const string&  client_name);

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
                               BlobAccessDescr*  blob_descr);
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

protected:
    void CheckConnect();
    void MakeCommandPacket(string* out_str, 
                           const string& cmd_str) const;
    void AddKVS(string*          out_str, 
                const string&    key,
                int              version,
                const string&    subkey) const;

    void TrimPrefix(string* str) const;
    void CheckOK(string* str) const;

protected:
    string      m_CacheName;
    size_t      m_BlobSize;
};


extern NCBI_NET_CACHE_EXPORT const char* kNetICacheDriverName;

extern "C"
{

NCBI_NET_CACHE_EXPORT
void NCBI_EntryPoint_xcache_netcache(
     CPluginManager<ICache>::TDriverInfoList&   info_list,
     CPluginManager<ICache>::EEntryPointRequest method);


} // extern C


END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.4  2006/01/09 16:39:25  vakatov
 * Heed the warning -- added EOL at EOF
 *
 * Revision 1.3  2006/01/05 17:38:30  kuznets
 * Implemented plugin manager entry point
 *
 * Revision 1.2  2006/01/04 19:04:58  kuznets
 * Cleanup
 *
 * Revision 1.1  2006/01/03 15:35:57  kuznets
 * Added network ICache client
 *
 *
 * ===========================================================================
 */


#endif  /* CONNECT_SERVICES___NETICACHE_CLIENT__HPP */
