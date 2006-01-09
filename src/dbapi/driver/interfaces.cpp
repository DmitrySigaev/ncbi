/* $Id$
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
 *  ===========================================================================
 *
 * Author:  Vladimir Soussov
 *
 * File Description:  Data Server public interfaces
 *
 */

#include <ncbi_pch.hpp>
#include <dbapi/driver/interfaces.hpp>
#include <dbapi/driver/public.hpp>
#include <dbapi/driver/dbapi_driver_conn_mgr.hpp>


BEGIN_NCBI_SCOPE


////////////////////////////////////////////////////////////////////////////
// C_ITDescriptorGuard
C_ITDescriptorGuard::C_ITDescriptorGuard(I_ITDescriptor* d) 
{
    m_D = d;
}

C_ITDescriptorGuard::~C_ITDescriptorGuard(void) 
{
    if ( m_D )
        delete m_D;
}
    
////////////////////////////////////////////////////////////////////////////
//  I_ITDescriptor::
//

I_ITDescriptor::~I_ITDescriptor(void)
{
    return;
}


////////////////////////////////////////////////////////////////////////////
//  CDB_BaseEnt::
//

CDB_BaseEnt::CDB_BaseEnt(void) 
{
    m_BR = 0;
}

void CDB_BaseEnt::Release(void)
{
    m_BR = 0;
}

CDB_BaseEnt::~CDB_BaseEnt(void)
{
    if (m_BR)
        *m_BR = 0;
}

CDB_Result* CDB_BaseEnt::Create_Result(I_Result& result)
{
    return new CDB_Result(&result);
}


////////////////////////////////////////////////////////////////////////////
//  I_BaseCmd::
//  I_LangCmd::
//  I_RPCCmd::
//  I_BCPInCmd::
//  I_CursorCmd::
//  I_SendDataCmd::
//

I_BaseCmd::I_BaseCmd(void)
{
}

I_BaseCmd::~I_BaseCmd(void)
{
    return;
}

I_LangCmd::I_LangCmd(void)
{
}

I_LangCmd::~I_LangCmd(void)
{
    return;
}

I_RPCCmd::I_RPCCmd(void)
{
}

I_RPCCmd::~I_RPCCmd(void)
{
    return;
}

I_BCPInCmd::I_BCPInCmd(void)
{
}

I_BCPInCmd::~I_BCPInCmd(void)
{
    return;
}

I_CursorCmd::I_CursorCmd(void)
{
}

I_CursorCmd::~I_CursorCmd(void)
{
    return;
}

I_SendDataCmd::I_SendDataCmd(void)
{
}

I_SendDataCmd::~I_SendDataCmd(void)
{
    return;
}


////////////////////////////////////////////////////////////////////////////
//  I_Result::
//

I_Result::I_Result(void)
{
}

I_Result::~I_Result(void)
{
    return;
}


////////////////////////////////////////////////////////////////////////////
//  I_DriverContext::SConnAttr::
//

I_DriverContext::SConnAttr::SConnAttr(void)
: mode(fDoNotConnect)
{
}

////////////////////////////////////////////////////////////////////////////
//  I_DriverContext::
//

I_DriverContext::I_DriverContext(void)
{
    PushCntxMsgHandler    ( &CDB_UserHandler::GetDefault() );
    PushDefConnMsgHandler ( &CDB_UserHandler::GetDefault() );
}

I_DriverContext::~I_DriverContext(void)
{
    return;
}

void I_DriverContext::PushCntxMsgHandler(CDB_UserHandler* h)
{
    CFastMutexGuard mg(m_Mtx);
    m_CntxHandlers.Push(h);
}

void I_DriverContext::PopCntxMsgHandler(CDB_UserHandler* h)
{
    CFastMutexGuard mg(m_Mtx);
    m_CntxHandlers.Pop(h);
}

void I_DriverContext::PushDefConnMsgHandler(CDB_UserHandler* h)
{
    CFastMutexGuard mg(m_Mtx);
    m_ConnHandlers.Push(h);
}

void I_DriverContext::PopDefConnMsgHandler(CDB_UserHandler* h)
{
    CFastMutexGuard mg(m_Mtx);
    m_ConnHandlers.Pop(h);
}

void I_DriverContext::x_Recycle(I_Connection* conn, bool conn_reusable)
{
    CFastMutexGuard mg(m_Mtx);

    m_InUse.Remove((TPotItem) conn);

    if ( conn_reusable ) {
        m_NotInUse.Add((TPotItem) conn);
    } else {
        delete conn;
    }
}

void I_DriverContext::CloseUnusedConnections(const string&   srv_name,
                                             const string&   pool_name)
{
    CFastMutexGuard mg(m_Mtx);

    I_Connection* con;
    
    // close all connections first
    for (int i = m_NotInUse.NofItems();  i--; ) {
        con = (I_Connection*)(m_NotInUse.Get(i));
        if((!srv_name.empty()) && srv_name.compare(con->ServerName())) continue;
        if((!pool_name.empty()) && pool_name.compare(con->PoolName())) continue;

        m_NotInUse.Remove(i);
        delete con;
    }
}

unsigned int I_DriverContext::NofConnections(const string& srv_name,
                                          const string& pool_name) const
{

    if ( srv_name.empty() && pool_name.empty()) {
        return m_InUse.NofItems() + m_NotInUse.NofItems();
    }

    CFastMutexGuard mg(m_Mtx);
    int n = 0;
    I_Connection* con;

    for (int i = m_NotInUse.NofItems(); i--;) {
        con= (I_Connection*) (m_NotInUse.Get(i));
        if((!srv_name.empty()) && srv_name.compare(con->ServerName())) continue;
        if((!pool_name.empty()) && pool_name.compare(con->PoolName())) continue;
        ++n;
    }

    for (int i = m_InUse.NofItems(); i--;) {
        con= (I_Connection*) (m_InUse.Get(i));
        if((!srv_name.empty()) && srv_name.compare(con->ServerName())) continue;
        if((!pool_name.empty()) && pool_name.compare(con->PoolName())) continue;
        ++n;
    }

    return n;
}

CDB_Connection* I_DriverContext::Create_Connection(I_Connection& connection)
{
    m_InUse.Add((TPotItem) &connection);

    return new CDB_Connection(&connection);
}

void 
I_DriverContext::SetApplicationName(const string& app_name)
{
    m_AppName = app_name;
}

const string& 
I_DriverContext::GetApplicationName(void) const
{
    return m_AppName;
}

void 
I_DriverContext::SetHostName(const string& host_name)
{
    m_HostName = host_name;
}

const string& 
I_DriverContext::GetHostName(void) const
{
    return m_HostName;
}

I_Connection* 
I_DriverContext::MakePooledConnection(const SConnAttr& conn_attr)
{
    if (conn_attr.reusable  &&  m_NotInUse.NofItems() > 0) {
        // try to get a connection from the pot
        if ( !conn_attr.pool_name.empty() ) {
            // use a pool name
            for (int i = m_NotInUse.NofItems();  --i; ) {
                CDB_Connection* t_con
                    = static_cast<CDB_Connection*> (m_NotInUse.Get(i));

                // There is no pool name check here. We assume that a connection
                // pool contains connections with appropriate server names only.
                if (conn_attr.pool_name.compare(t_con->PoolName()) == 0) {
                    m_NotInUse.Remove(i);
                    if(t_con->Refresh()) {
                        return Create_Connection(*t_con);
                    }
                    delete t_con;
                }
            }
        }
        else {

            if ( conn_attr.srv_name.empty() )
                return NULL;

            // try to use a server name
            for (int i = m_NotInUse.NofItems();  --i; ) {
                CDB_Connection* t_con
                    = static_cast<CDB_Connection*> (m_NotInUse.Get(i));

                if (conn_attr.srv_name.compare(t_con->ServerName()) == 0) {
                    m_NotInUse.Remove(i);
                    if(t_con->Refresh()) {
                        return Create_Connection(*t_con);
                    }
                    delete t_con;
                }
            }
        }
    }

    if((conn_attr.mode & fDoNotConnect) != 0) {
        return NULL;
    }
    
    I_Connection* t_con = MakeConnection(conn_attr);
    
    return t_con;
}

CDB_Connection* 
I_DriverContext::Connect(const string&   srv_name,
                         const string&   user_name,
                         const string&   passwd,
                         TConnectionMode mode,
                         bool            reusable,
                         const string&   pool_name)
{
    CFastMutexGuard mg(m_Mtx);

    SConnAttr conn_attr;

    conn_attr.srv_name = srv_name;
    conn_attr.user_name = user_name;
    conn_attr.passwd = passwd;
    conn_attr.mode = mode;
    conn_attr.reusable = reusable;
    conn_attr.pool_name = pool_name;

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
        
    I_Connection* t_con = 
        CDbapiConnMgr::Instance().GetConnectionFactory()->MakeConnection(*this, conn_attr);
    
    if((!t_con && (mode & fDoNotConnect) != 0)) {
        return NULL;
    }

    CHECK_DRIVER_ERROR(t_con == NULL, 
                       "Cannot connect to the server '" + srv_name + "'", 
                       100011 );
    
    return Create_Connection(*t_con);
}

////////////////////////////////////////////////////////////////////////////
//  I_Connection::
//

CDB_LangCmd* I_Connection::Create_LangCmd(I_LangCmd&lang_cmd)
{
    return new CDB_LangCmd(&lang_cmd);
}

CDB_RPCCmd* I_Connection::Create_RPCCmd(I_RPCCmd&rpc_cmd)
{
    return new CDB_RPCCmd(&rpc_cmd);
}

CDB_BCPInCmd* I_Connection::Create_BCPInCmd(I_BCPInCmd& bcpin_cmd)
{
    return new CDB_BCPInCmd(&bcpin_cmd);
}

CDB_CursorCmd* I_Connection::Create_CursorCmd(I_CursorCmd& cursor_cmd)
{
    return new CDB_CursorCmd(&cursor_cmd);
}

CDB_SendDataCmd* I_Connection::Create_SendDataCmd(I_SendDataCmd& senddata_cmd)
{
    return new CDB_SendDataCmd(&senddata_cmd);
}


I_Connection::I_Connection(void)
{
}

I_Connection::~I_Connection(void)
{
    return;
}


////////////////////////////////////////////////////////////////////////////
//  I_DriverMgr::
//

I_DriverMgr::I_DriverMgr(void)
{
}

I_DriverMgr::~I_DriverMgr(void)
{
    return;
}


END_NCBI_SCOPE



/*
 * ===========================================================================
 * $Log$
 * Revision 1.12  2006/01/09 19:19:21  ssikorsk
 * Validate server name, user name and password before connection for all drivers.
 *
 * Revision 1.11  2006/01/03 18:59:52  ssikorsk
 * Implement I_DriverContext::MakePooledConnection and
 * I_DriverContext::Connect.
 *
 * Revision 1.10  2005/10/20 13:03:20  ssikorsk
 * Implemented:
 * I_DriverContext::SetApplicationName
 * I_DriverContext::GetApplicationName
 * I_DriverContext::SetHostName
 * I_DriverContext::GetHostName
 *
 * Revision 1.9  2004/05/17 21:11:38  gorelenk
 * Added include of PCH ncbi_pch.hpp
 *
 * Revision 1.8  2003/07/17 20:44:57  soussov
 * connections pool improvements
 *
 * Revision 1.7  2002/01/20 07:21:02  vakatov
 * I_DriverMgr:: -- added virtual destructor
 *
 * Revision 1.6  2001/11/06 17:59:53  lavr
 * Formatted uniformly as the rest of the library
 *
 * Revision 1.5  2001/10/01 20:09:29  vakatov
 * Introduced a generic default user error handler and the means to
 * alternate it. Added an auxiliary error handler class
 * "CDB_UserHandler_Stream".
 * Moved "{Push/Pop}{Cntx/Conn}MsgHandler()" to the generic code
 * (in I_DriverContext).
 *
 * Revision 1.4  2001/09/27 20:08:32  vakatov
 * Added "DB_" (or "I_") prefix where it was missing
 *
 * Revision 1.3  2001/09/26 23:23:29  vakatov
 * Moved the err.message handlers' stack functionality (generic storage
 * and methods) to the "abstract interface" level.
 *
 * Revision 1.2  2001/09/24 19:48:49  vakatov
 * + implementation for CDB_BaseEnt::Create_Result
 *
 * Revision 1.1  2001/09/21 23:39:59  vakatov
 * -----  Initial (draft) revision.  -----
 * This is a major revamp (by Denis Vakatov, with help from Vladimir Soussov)
 * of the DBAPI "driver" libs originally written by Vladimir Soussov.
 * The revamp involved massive code shuffling and grooming, numerous local
 * API redesigns, adding comments and incorporating DBAPI to the C++ Toolkit.
 *
 * ===========================================================================
 */
