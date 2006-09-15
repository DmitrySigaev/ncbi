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

#include <dbapi/driver/impl/dbapi_impl_context.hpp>
#include <dbapi/driver/impl/dbapi_impl_connection.hpp>

#include <dbapi/driver/dbapi_driver_conn_mgr.hpp>

#include <algorithm>

#if defined(NCBI_OS_MSWIN)
#  include <winsock2.h>
#endif

BEGIN_NCBI_SCOPE

BEGIN_SCOPE(impl)

///////////////////////////////////////////////////////////////////////////
//  CDriverContext::
//

CDriverContext::CDriverContext(void) :
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

bool CDriverContext::SetTimeout(unsigned int nof_secs)
{
    bool success = true;
    CFastMutexGuard mg(m_Mtx);

    try {
        if (I_DriverContext::SetTimeout(nof_secs)) {
            UpdateConnTimeout();
        }
    } catch (...) {
        success = false;
    }

    return success;
}

bool CDriverContext::SetMaxTextImageSize(size_t nof_bytes)
{
    CFastMutexGuard mg(m_Mtx);

    m_MaxTextImageSize = nof_bytes;

    UpdateConnMaxTextImageSize();

    return true;
}

void CDriverContext::PushCntxMsgHandler(CDB_UserHandler* h,
                                         EOwnership ownership)
{
    CFastMutexGuard mg(m_Mtx);
    m_CntxHandlers.Push(h, ownership);
}

void CDriverContext::PopCntxMsgHandler(CDB_UserHandler* h)
{
    CFastMutexGuard mg(m_Mtx);
    m_CntxHandlers.Pop(h);
}

void CDriverContext::PushDefConnMsgHandler(CDB_UserHandler* h,
                                            EOwnership ownership)
{
    CFastMutexGuard mg(m_Mtx);
    m_ConnHandlers.Push(h, ownership);
}

void CDriverContext::PopDefConnMsgHandler(CDB_UserHandler* h)
{
    CFastMutexGuard mg(m_Mtx);
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

void CDriverContext::x_Recycle(CConnection* conn, bool conn_reusable)
{
    CFastMutexGuard mg(m_Mtx);

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
    CFastMutexGuard mg(m_Mtx);

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
    CFastMutexGuard mg(m_Mtx);

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
CDriverContext::MakePooledConnection(const SConnAttr& conn_attr)
{
    if (conn_attr.reusable && !m_NotInUse.empty()) {
        // try to get a connection from the pot
        if (!conn_attr.pool_name.empty()) {
            // use a pool name
            NON_CONST_ITERATE(TConnPool, it, m_NotInUse) {
                CConnection* t_con(*it);

                // There is no pool name check here. We assume that a connection
                // pool contains connections with appropriate server names only.
                if (conn_attr.pool_name.compare(t_con->PoolName()) == 0) {
                    it = --m_NotInUse.erase(it);
                    if(t_con->Refresh()) {
                        return MakeCDBConnection(t_con);
                    }
                    else {
                        delete t_con;
                    }
                }
            }
        }
        else {

            if ( conn_attr.srv_name.empty() ) {
                return NULL;
            }

            // try to use a server name
            NON_CONST_ITERATE(TConnPool, it, m_NotInUse) {
                CConnection* t_con(*it);

                if (conn_attr.srv_name.compare(t_con->ServerName()) == 0) {
                    it = --m_NotInUse.erase(it);
                    if(t_con->Refresh()) {
                        return MakeCDBConnection(t_con);
                    }
                    else {
                        delete t_con;
                    }
                }
            }
        }
    }

    if ((conn_attr.mode & fDoNotConnect) != 0) {
        return NULL;
    }

    // Precondition check.
    if (conn_attr.srv_name.empty() ||
        conn_attr.user_name.empty() ||
        conn_attr.passwd.empty()) {
        string err_msg("Insufficient info/credentials to connect.");

        if (conn_attr.srv_name.empty()) {
            err_msg += " Server name has not been set.";
        }
        if (conn_attr.user_name.empty()) {
            err_msg += " User name has not been set.";
        }
        if (conn_attr.passwd.empty()) {
            err_msg += " Password has not been set.";
        }

        DATABASE_DRIVER_ERROR( err_msg, 200010 );
    }

    CConnection* t_con = MakeIConnection(conn_attr);

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

CDB_Connection*
CDriverContext::Connect(const string&   srv_name,
                         const string&   user_name,
                         const string&   passwd,
                         TConnectionMode mode,
                         bool            reusable,
                         const string&   pool_name)
{
    SConnAttr conn_attr;

    conn_attr.srv_name = srv_name;
    conn_attr.user_name = user_name;
    conn_attr.passwd = passwd;
    conn_attr.mode = mode;
    conn_attr.reusable = reusable;
    conn_attr.pool_name = pool_name;

    CDB_Connection* t_con =
        CDbapiConnMgr::Instance().GetConnectionFactory()->MakeDBConnection(
            *this,
            conn_attr);

    if((!t_con && (mode & fDoNotConnect) != 0)) {
        return NULL;
    }

    if (!t_con) {
        string err;

        err += "Cannot connect to the server '" + srv_name;
        err += "' as user '" + user_name + "'";
        DATABASE_DRIVER_ERROR( err, 100011 );
    }

    // return Create_Connection(*t_con);
    return t_con;
}

CDB_Connection*
CDriverContext::ConnectValidated(const string&   srv_name,
                                  const string&   user_name,
                                  const string&   passwd,
                                  IConnValidator& validator,
                                  TConnectionMode mode,
                                  bool            reusable,
                                  const string&   pool_name)
{
    SConnAttr conn_attr;

    conn_attr.srv_name = srv_name;
    conn_attr.user_name = user_name;
    conn_attr.passwd = passwd;
    conn_attr.mode = mode;
    conn_attr.reusable = reusable;
    conn_attr.pool_name = pool_name;

    validator.DoNotDeleteThisObject();
    CDB_Connection* t_con =
        CDbapiConnMgr::Instance().GetConnectionFactory()->MakeDBConnection(
            *this,
            conn_attr,
            &validator);

    if((!t_con && (mode & fDoNotConnect) != 0)) {
        return NULL;
    }

    if (!t_con) {
        string err;

        err += "Cannot connect to the server '" + srv_name;
        err += "' as user '" + user_name + "'";
        DATABASE_DRIVER_ERROR( err, 100011 );
    }

    return t_con;
}

void CDriverContext::DestroyConnImpl(CConnection* impl)
{
    if (impl) {
        impl->ReleaseInterface();
        x_Recycle(impl, impl->IsReusable());
    }
}

bool CDriverContext::ConnectedToMSSQLServer(void) const
{
    return false;
}

void CDriverContext::SetClientCharset(const string& charset)
{
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
    // Do not protect this method. It is protected.

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

END_SCOPE(impl)

END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.4  2006/09/15 19:20:35  ssikorsk
 * Translate exceptions into return code in CDriverContext::SetTimeout.
 *
 * Revision 1.3  2006/09/13 19:51:58  ssikorsk
 * Implemented SetTimeout, SetMaxTextImageSize with CDriverContext;
 * Implemented SetClientCharset, UpdateConnTimeout, UpdateConnMaxTextImageSize
 * with CDriverContext;
 * Implemented class CWinSock;
 *
 * Revision 1.2  2006/07/12 20:35:22  ucko
 * #include <algorithm> for find()
 *
 * Revision 1.1  2006/07/12 16:29:30  ssikorsk
 * Separated interface and implementation of CDB classes.
 *
 * ===========================================================================
 */

