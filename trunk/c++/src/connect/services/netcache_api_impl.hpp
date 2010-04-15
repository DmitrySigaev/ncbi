#ifndef CONNECT_SERVICES___NETCACHE_API_IMPL__HPP
#define CONNECT_SERVICES___NETCACHE_API_IMPL__HPP

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
 * Author:  Dmitry Kazimirov
 *
 * File Description:
 */

#include "netservice_api_impl.hpp"
#include "netcache_rw.hpp"

#include <connect/services/netcache_api.hpp>

BEGIN_NCBI_SCOPE

class NCBI_XCONNECT_EXPORT CNetCacheServerListener :
    public INetServerConnectionListener
{
public:
    CNetCacheServerListener(const string& client_name) : m_Auth(client_name) {}

private:
    virtual void OnConnected(CNetServerConnection::TInstance conn);
    virtual void OnError(const string& err_msg, SNetServerImpl* server);

private:
    string m_Auth;
};

struct NCBI_XCONNECT_EXPORT SNetCacheAPIImpl : public CNetObject
{
    SNetCacheAPIImpl(CConfig* config, const string& section,
        const string& service, const string& client_name,
        const string& lbsm_affinity_name);

    IReader* GetReadStream(
        const CNetServer::SExecResult& exec_result,
        size_t* blob_size);

    static CNetCacheAPI::EReadResult ReadBuffer(
        IReader& reader,
        unsigned char* buf_ptr,
        size_t buf_size,
        size_t* n_read,
        size_t blob_size);

    CNetServer GetServer(const string& bid);
    CNetServerConnection InitiatePutCmd(string* key, unsigned time_to_live);

    void WriteBuffer(
        SNetServerConnectionImpl* conn_impl,
        CNetCacheWriter::EServerResponseType response_type,
        const char* buf_ptr,
        size_t buf_size);

    void AppendClientIPSessionIDPassword(string* cmd);
    string MakeCmd(const char* cmd);
    string MakeCmd(const char* cmd_base, const string& key);

    CNetService m_Service;

    CNetObjectRef<CNetCacheServerListener> m_Listener;

    string m_Password;
};

struct SNetCacheAdminImpl : public CNetObject
{
    SNetCacheAdminImpl(SNetCacheAPIImpl* nc_api_impl);

    CNetCacheAPI m_API;
};

inline SNetCacheAdminImpl::SNetCacheAdminImpl(SNetCacheAPIImpl* nc_api_impl) :
    m_API(nc_api_impl)
{
}

END_NCBI_SCOPE

#endif  /* CONNECT_SERVICES___NETCACHE_API_IMPL__HPP */
