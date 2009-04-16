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
* Author:  Sergey Sikorskiy
*
*/

#include <ncbi_pch.hpp>

#include <dbapi/driver/dbapi_driver_conn_mgr.hpp>
#include <dbapi/driver/impl/dbapi_impl_context.hpp>
#include <dbapi/driver/impl/dbapi_impl_connection.hpp>
#include <dbapi/driver/dbapi_driver_conn_params.hpp>

#include <util/resource_info.hpp>
#include <corelib/ncbifile.hpp>

#include <algorithm>
#include <set>


#if defined(NCBI_OS_MSWIN)
#  include <winsock2.h>
#endif


BEGIN_NCBI_SCOPE


NCBI_PARAM_DEF_EX(bool, dbapi, conn_use_encrypt_data, false, eParam_NoThread, NULL);


namespace impl
{

///////////////////////////////////////////////////////////////////////////
//  CDriverContext::
//

CDriverContext::CDriverContext(void) :
    m_LoginTimeout(0),
    m_Timeout(0),
    m_MaxTextImageSize(0),
    m_ClientEncoding(eEncoding_ISO8859_1)
{
    PushCntxMsgHandler    ( &CDB_UserHandler::GetDefault(), eTakeOwnership );
    PushDefConnMsgHandler ( &CDB_UserHandler::GetDefault(), eTakeOwnership );
}

CDriverContext::~CDriverContext(void)
{
    return;
}

void
CDriverContext::SetApplicationName(const string& app_name)
{
    CMutexGuard mg(m_CtxMtx);

    m_AppName = app_name;
}

string
CDriverContext::GetApplicationName(void) const
{
    CMutexGuard mg(m_CtxMtx);

    return m_AppName;
}

void
CDriverContext::SetHostName(const string& host_name)
{
    CMutexGuard mg(m_CtxMtx);

    m_HostName = host_name;
}

string
CDriverContext::GetHostName(void) const
{
    CMutexGuard mg(m_CtxMtx);

    return m_HostName;
}

unsigned int CDriverContext::GetLoginTimeout(void) const 
{ 
    CMutexGuard mg(m_CtxMtx);

    return m_LoginTimeout; 
}

bool CDriverContext::SetLoginTimeout (unsigned int nof_secs)
{
    CMutexGuard mg(m_CtxMtx);

    m_LoginTimeout = nof_secs;

    return true;
}

unsigned int CDriverContext::GetTimeout(void) const 
{ 
    CMutexGuard mg(m_CtxMtx);

    return m_Timeout; 
}


bool CDriverContext::SetTimeout(unsigned int nof_secs)
{
    bool success = true;
    CMutexGuard mg(m_CtxMtx);

    try {
        m_Timeout = nof_secs;
        
        // We do not have to update query/connection timeout in context
        // any more. Each connection can be updated explicitly now.
        // UpdateConnTimeout();
    } catch (...) {
        success = false;
    }

    return success;
}

bool CDriverContext::SetMaxTextImageSize(size_t nof_bytes)
{
    CMutexGuard mg(m_CtxMtx);

    m_MaxTextImageSize = nof_bytes;

    UpdateConnMaxTextImageSize();

    return true;
}

void CDriverContext::PushCntxMsgHandler(CDB_UserHandler* h,
                                         EOwnership ownership)
{
    CMutexGuard mg(m_CtxMtx);
    m_CntxHandlers.Push(h, ownership);
}

void CDriverContext::PopCntxMsgHandler(CDB_UserHandler* h)
{
    CMutexGuard mg(m_CtxMtx);
    m_CntxHandlers.Pop(h);
}

void CDriverContext::PushDefConnMsgHandler(CDB_UserHandler* h,
                                            EOwnership ownership)
{
    CMutexGuard mg(m_CtxMtx);
    m_ConnHandlers.Push(h, ownership);
}

void CDriverContext::PopDefConnMsgHandler(CDB_UserHandler* h)
{
    CMutexGuard mg(m_CtxMtx);
    m_ConnHandlers.Pop(h);

    // Remove this handler from all connections
    TConnPool::value_type con = NULL;
    ITERATE(TConnPool, it, m_NotInUse) {
        con = *it;
        con->PopMsgHandler(h);
    }

    ITERATE(TConnPool, it, m_InUse) {
        con = *it;
        con->PopMsgHandler(h);
    }
}


void CDriverContext::ResetEnvSybase(void)
{
    DEFINE_STATIC_MUTEX(env_mtx);

    CMutexGuard mg(env_mtx);
    CNcbiEnvironment env;

    // If user forces his own Sybase client path using $RESET_SYBASE
    // and $SYBASE -- use that unconditionally.
    try {
        if (!env.Get("SYBASE").empty()) {
            string reset = env.Get("RESET_SYBASE");
            if ( !reset.empty() && NStr::StringToBool(reset)) {
                return;
            }
        }
        // ...else try hardcoded paths 
    } catch (const CStringException&) {
        // Conversion error -- try hardcoded paths too
    }

    // User-set or default hardcoded path
    if ( CDir(NCBI_GetSybasePath()).CheckAccess(CDirEntry::fRead) ) {
        env.Set("SYBASE", NCBI_GetSybasePath());
        return;
    }

    // If NCBI_SetSybasePath() was used to set up the Sybase path, and it is
    // not right, then use the very Sybase client against which the code was
    // compiled
    if ( !NStr::Equal(NCBI_GetSybasePath(), NCBI_GetDefaultSybasePath())  &&
         CDir(NCBI_GetDefaultSybasePath()).CheckAccess(CDirEntry::fRead) ) {
        env.Set("SYBASE", NCBI_GetDefaultSybasePath());
    }

    // Else, well, use whatever $SYBASE there is
}


void CDriverContext::x_Recycle(CConnection* conn, bool conn_reusable)
{
    CMutexGuard mg(m_CtxMtx);

    TConnPool::iterator it = find(m_InUse.begin(), m_InUse.end(), conn);

    if (it != m_InUse.end()) {
        m_InUse.erase(it);
    }

    if ( conn_reusable ) {
        m_NotInUse.push_back(conn);
    } else {
        delete conn;
    }
}

void CDriverContext::CloseUnusedConnections(const string&   srv_name,
                                             const string&   pool_name)
{
    CMutexGuard mg(m_CtxMtx);

    TConnPool::value_type con;

    // close all connections first
    NON_CONST_ITERATE(TConnPool, it, m_NotInUse) {
        con = *it;

        if((!srv_name.empty()) && srv_name.compare(con->ServerName())) continue;
        if((!pool_name.empty()) && pool_name.compare(con->PoolName())) continue;

        it = --m_NotInUse.erase(it);
        delete con;
    }
}

unsigned int CDriverContext::NofConnections(const string& srv_name,
                                          const string& pool_name) const
{
    CMutexGuard mg(m_CtxMtx);

    if ( srv_name.empty() && pool_name.empty()) {
        return static_cast<unsigned int>(m_InUse.size() + m_NotInUse.size());
    }

    int n = 0;
    TConnPool::value_type con;

    ITERATE(TConnPool, it, m_NotInUse) {
        con = *it;
        if((!srv_name.empty()) && srv_name.compare(con->ServerName())) continue;
        if((!pool_name.empty()) && pool_name.compare(con->PoolName())) continue;
        ++n;
    }

    ITERATE(TConnPool, it, m_InUse) {
        con = *it;
        if((!srv_name.empty()) && srv_name.compare(con->ServerName())) continue;
        if((!pool_name.empty()) && pool_name.compare(con->PoolName())) continue;
        ++n;
    }

    return n;
}

CDB_Connection* CDriverContext::MakeCDBConnection(CConnection* connection)
{
    m_InUse.push_back(connection);

    return new CDB_Connection(connection);
}

CDB_Connection*
CDriverContext::MakePooledConnection(const CDBConnParams& params)
{
    if (params.GetParam("is_pooled") == "true") {
        CMutexGuard mg(m_CtxMtx);

        if (!m_NotInUse.empty()) {
            if (!params.GetParam("pool_name").empty()) {
                // use a pool name
                for (TConnPool::iterator it = m_NotInUse.begin(); it != m_NotInUse.end(); ) {
                    CConnection* t_con(*it);

                    // There is no pool name check here. We assume that a connection
                    // pool contains connections with appropriate server names only.
                    if (params.GetParam("pool_name").compare(t_con->PoolName()) == 0) {
                        it = m_NotInUse.erase(it);
                        if(t_con->Refresh()) {
                            /* Future development ...
                            if (!params.GetDatabaseName().empty()) {
                                return SetDatabase(MakeCDBConnection(t_con), params);
                            } else {
                                return MakeCDBConnection(t_con);
                            }
                            */
                            
                            return MakeCDBConnection(t_con);
                        }
                        else {
                            delete t_con;
                        }
                    }
                    else {
                        ++it;
                    }
                }
            }
            else {

                if ( params.GetServerName().empty() ) {
                    return NULL;
                }

                // try to use a server name
                for (TConnPool::iterator it = m_NotInUse.begin(); it != m_NotInUse.end(); ) {
                    CConnection* t_con(*it);

                    if (params.GetServerName().compare(t_con->ServerName()) == 0) {
                        it = m_NotInUse.erase(it);
                        if (t_con->Refresh()) {
                            /* Future development ...
                            if (!params.GetDatabaseName().empty()) {
                                return SetDatabase(MakeCDBConnection(t_con), params);
                            } else {
                                return MakeCDBConnection(t_con);
                            }
                            */

                            return MakeCDBConnection(t_con);
                        }
                        else {
                            delete t_con;
                        }
                    }
                    else {
                        ++it;
                    }
                }
            }
        }
    }

    if (params.GetParam("do_not_connect") == "true") {
        return NULL;
    }

    // Precondition check.
    if (params.GetServerName().empty() ||
            params.GetUserName().empty() ||
            params.GetPassword().empty()) {
        string err_msg("Insufficient info/credentials to connect.");

        if (params.GetServerName().empty()) {
            err_msg += " Server name has not been set.";
        }
        if (params.GetUserName().empty()) {
            err_msg += " User name has not been set.";
        }
        if (params.GetPassword().empty()) {
            err_msg += " Password has not been set.";
        }

        DATABASE_DRIVER_ERROR( err_msg, 200010 );
    }

    CConnection* t_con = MakeIConnection(params);

    return MakeCDBConnection(t_con);
}

void
CDriverContext::CloseAllConn(void)
{
    // close all connections first
    ITERATE(TConnPool, it, m_NotInUse) {
        delete *it;
    }
    m_NotInUse.clear();

    ITERATE(TConnPool, it, m_InUse) {
        (*it)->Close();
    }
}

void
CDriverContext::DeleteAllConn(void)
{
    // close all connections first
    ITERATE(TConnPool, it, m_NotInUse) {
        delete *it;
    }
    m_NotInUse.clear();

    ITERATE(TConnPool, it, m_InUse) {
        delete *it;
    }
    m_InUse.clear();
}


struct SLoginData
{
    string server_name;
    string user_name;
    string db_name;
    string password;

    SLoginData(const string& sn, const string& un,
               const string& dn, const string& pass)
        : server_name(sn), user_name(un), db_name(dn), password(pass)
    {}

    bool operator< (const SLoginData& right) const
    {
        if (server_name != right.server_name)
            return server_name < right.server_name;
        else if (user_name != right.user_name)
            return user_name < right.user_name;
        else if (db_name != right.db_name)
            return db_name < right.db_name;
        else
            return password < right.password;
    }
};


static void
s_TransformLoginData(string& server_name, string& user_name,
                     string& db_name,     string& password)
{
    if (!TDbapi_ConnUseEncryptData::GetDefault())
        return;

    string app_name = CNcbiApplication::Instance()->GetProgramDisplayName();
    set<SLoginData> visited;
    CNcbiResourceInfoFile res_file(CNcbiResourceInfoFile::GetDefaultFileName());

    visited.insert(SLoginData(server_name, user_name, db_name, password));
    for (;;) {
        string res_name = app_name;
        if (!user_name.empty()) {
            res_name += "/";
            res_name += user_name;
        }
        if (!server_name.empty()) {
            res_name += "@";
            res_name += server_name;
        }
        if (!db_name.empty()) {
            res_name += ":";
            res_name += db_name;
        }
        const CNcbiResourceInfo& info
                               = res_file.GetResourceInfo(res_name, password);
        if (!info)
            break;

        password = info.GetValue();
        typedef multimap<string, string>  TExtraMap;
        typedef TExtraMap::const_iterator TExtraMapIt;
        const TExtraMap& extra = info.GetExtraValues().GetPairs();

        TExtraMapIt it = extra.find("server");
        if (it != extra.end())
            server_name = it->second;
        it = extra.find("username");
        if (it != extra.end())
            user_name = it->second;
        it = extra.find("database");
        if (it != extra.end())
            db_name = it->second;

        if (!visited.insert(
                SLoginData(server_name, user_name, db_name, password)).second)
        {
            DATABASE_DRIVER_ERROR(
                   "Circular dependency inside resources info file.", 100012);
        }
    }
}


CDB_Connection* 
CDriverContext::MakeConnection(const CDBConnParams& params)
{
    string server_name = params.GetServerName();
    string user_name = params.GetUserName();
    string db_name = params.GetDatabaseName();
    string password = params.GetPassword();
    s_TransformLoginData(server_name, user_name, db_name, password);
    CMakeConnActualParams act_params(params, server_name, user_name,
                                     db_name, password);

    CDB_Connection* t_con =
        CDbapiConnMgr::Instance().GetConnectionFactory()->MakeDBConnection(
            *this,
            act_params);

    if((!t_con && act_params.GetParam("do_not_connect") == "true")) {
        return NULL;
    }

    if (!t_con) {
        string err;

        err += "Cannot connect to the server '" + act_params.GetServerName();
        err += "' as user '" + act_params.GetUserName() + "'";
        DATABASE_DRIVER_ERROR( err, 100011 );
    }

    // Set database ...
    t_con->SetDatabaseName(act_params.GetDatabaseName());

    return t_con;
}

void CDriverContext::DestroyConnImpl(CConnection* impl)
{
    if (impl) {
        impl->ReleaseInterface();
        x_Recycle(impl, impl->IsReusable());
    }
}

void CDriverContext::SetClientCharset(const string& charset)
{
    CMutexGuard mg(m_CtxMtx);

    m_ClientCharset = charset;
    m_ClientEncoding = eEncoding_Unknown;

    if (NStr::CompareNocase(charset.c_str(), "UTF-8") == 0 ||
        NStr::CompareNocase(charset.c_str(), "UTF8") == 0) {
        m_ClientEncoding = eEncoding_UTF8;
    } else if (NStr::CompareNocase(charset.c_str(), "Ascii") == 0) {
        m_ClientEncoding = eEncoding_Ascii;
    } else if (NStr::CompareNocase(charset.c_str(), "ISO8859_1") == 0 ||
               NStr::CompareNocase(charset.c_str(), "ISO8859-1") == 0
               ) {
        m_ClientEncoding = eEncoding_ISO8859_1;
    } else if (NStr::CompareNocase(charset.c_str(), "Windows_1252") == 0 ||
               NStr::CompareNocase(charset.c_str(), "Windows-1252") == 0) {
        m_ClientEncoding = eEncoding_Windows_1252;
    }
}

void CDriverContext::UpdateConnTimeout(void) const
{
    // Do not protect this method. It is already protected.

    ITERATE(TConnPool, it, m_NotInUse) {
        CConnection* t_con = *it;
        if (!t_con) continue;

        t_con->SetTimeout(GetTimeout());
    }

    ITERATE(TConnPool, it, m_InUse) {
        CConnection* t_con = *it;
        if (!t_con) continue;

        t_con->SetTimeout(GetTimeout());
    }
}


void CDriverContext::UpdateConnMaxTextImageSize(void) const
{
    // Do not protect this method. It is protected.

    ITERATE(TConnPool, it, m_NotInUse) {
        CConnection* t_con = *it;
        if (!t_con) continue;

        t_con->SetTextImageSize(GetMaxTextImageSize());
    }

    ITERATE(TConnPool, it, m_InUse) {
        CConnection* t_con = *it;
        if (!t_con) continue;

        t_con->SetTextImageSize(GetMaxTextImageSize());
    }
}


///////////////////////////////////////////////////////////////////////////
CWinSock::CWinSock(void)
{
#if defined(NCBI_OS_MSWIN)
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(1, 1), &wsaData) != 0)
    {
        DATABASE_DRIVER_ERROR( "winsock initialization failed", 200001 );
    }
#endif
}

CWinSock::~CWinSock(void)
{
#if defined(NCBI_OS_MSWIN)
        WSACleanup();
#endif
}

} // namespace impl

END_NCBI_SCOPE


