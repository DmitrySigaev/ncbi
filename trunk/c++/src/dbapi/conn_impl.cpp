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
* ===========================================================================
*
* File Name:  $Id$
*
* Author:  Michael Kholodov
*
* File Description:   Connection implementation
*
*
* $Log$
* Revision 1.25  2004/03/12 16:27:09  sponomar
* correct nested querys
*
* Revision 1.23  2004/03/08 22:15:19  kholodov
* Added: 3 new Get...() methods internally
*
* Revision 1.22  2004/02/27 14:37:33  kholodov
* Modified: set collection replaced by list for listeners
*
* Revision 1.21  2003/11/18 17:00:01  kholodov
* Added: CloneConnection() method
*
* Revision 1.20  2003/03/07 21:21:15  kholodov
* Added: IsAlive() method
*
* Revision 1.19  2003/01/09 16:10:17  sapojnik
* CConnection::Connect -- do not attempt SetDbName() if connection failed
*
* Revision 1.18  2002/11/27 17:19:49  kholodov
* Added: Error output redirection to CToMultiExHandler object.
*
* Revision 1.17  2002/10/03 18:50:00  kholodov
* Added: additional TRACE diagnostics about object deletion
* Fixed: setting parameters in IStatement object is fully supported
* Added: IStatement::ExecuteLast() to execute the last statement with
* different parameters if any
*
* Revision 1.16  2002/09/30 20:45:34  kholodov
* Added: ForceSingle() method to enforce single connection used
*
* Revision 1.15  2002/09/23 18:25:10  kholodov
* Added: GetDataSource() method.
*
* Revision 1.14  2002/09/18 18:49:27  kholodov
* Modified: class declaration and Action method to reflect
* direct inheritance of CActiveObject from IEventListener
*
* Revision 1.13  2002/09/16 19:34:40  kholodov
* Added: bulk insert support
*
* Revision 1.12  2002/09/09 20:48:56  kholodov
* Added: Additional trace output about object life cycle
* Added: CStatement::Failed() method to check command status
*
* Revision 1.11  2002/06/24 19:10:03  kholodov
* Added more trace diagnostics
*
* Revision 1.10  2002/06/24 18:06:49  kholodov
* Added more detailed diagnostics on connections
*
* Revision 1.9  2002/06/21 14:42:31  kholodov
* Added: reporting connection deletions in debug mode
*
* Revision 1.8  2002/05/16 22:11:11  kholodov
* Improved: using minimum connections possible
*
* Revision 1.7  2002/04/15 19:08:55  kholodov
* Changed GetContext() -> GetDriverContext
*
* Revision 1.6  2002/02/08 22:43:10  kholodov
* Set/GetDataBase() renamed to Set/GetDatabase() respectively
*
* Revision 1.5  2002/02/08 21:29:54  kholodov
* SetDataBase() restored, connection cloning algorithm changed
*
* Revision 1.4  2002/02/08 17:47:34  kholodov
* Removed SetDataBase() method
*
* Revision 1.3  2002/02/08 17:38:26  kholodov
* Moved listener registration to parent objects
*
* Revision 1.2  2002/02/06 22:20:09  kholodov
* Connections are cloned for CallableStatement and Cursor
*
* Revision 1.1  2002/01/30 14:51:21  kholodov
* User DBAPI implementation, first commit
*
*
*
*
*/

#include <dbapi/driver/public.hpp>
//#include <dbapi/driver/interfaces.hpp>
//#include <dbapi/driver/exception.hpp>

#include "conn_impl.hpp"
#include "ds_impl.hpp"
#include "stmt_impl.hpp"
#include "cstmt_impl.hpp"
#include "cursor_impl.hpp"
#include "bulkinsert.hpp"
#include "err_handler.hpp"

BEGIN_NCBI_SCOPE

// Implementation
CConnection::CConnection(CDataSource* ds)
    : m_ds(ds), m_connection(0), m_connCounter(1), m_connUsed(false),
      m_modeMask(0), m_forceSingle(false), m_multiExH(0),
      m_stmt(0), m_cstmt(0), m_cursor(0), m_bulkInsert(0)
{
    _TRACE("Default connection " << (void *)this << " created...");
    SetIdent("CConnection");
}

CConnection::CConnection(CDB_Connection *conn, CDataSource* ds)
    : m_ds(ds), m_connection(conn), m_connCounter(-1), m_connUsed(false),
      m_modeMask(0), m_forceSingle(false), m_multiExH(0)
{
    _TRACE("Auxiliary connection " << (void *)this << " created...");
    SetIdent("CConnection");
}


void CConnection::SetMode(EConnMode mode)
{
    m_modeMask |= mode;
}

void CConnection::ResetMode(EConnMode mode)
{
    m_modeMask &= ~mode;
}

IDataSource* CConnection::GetDataSource()
{
    return m_ds;
}

unsigned int CConnection::GetModeMask()
{
    return m_modeMask;
}

void CConnection::ForceSingle(bool enable)
{
    m_forceSingle = enable;
}

void CConnection::Connect(const string& user,
                          const string& password,
                          const string& server,
                          const string& database)
{

    m_connection = m_ds->
        GetDriverContext()->Connect(server,
                                    user,
                                    password,
                                    m_modeMask,
                                    m_ds->IsPoolUsed());
    if(m_connection) {
        SetDbName(database);
    }

}

CConnection::~CConnection()
{
    if( IsAux() ) {
        _TRACE("Auxiliary connection " << (void*)this << " is being deleted...");
    } else {
        _TRACE("Default connection " << (void*)this << " is being deleted...");
    }
    Close();
    Notify(CDbapiDeletedEvent(this));
    _TRACE(GetIdent() << " " << (void*)this << " deleted.");
}


void CConnection::SetDatabase(const string& name)
{
  SetDbName(name);
}

string CConnection::GetDatabase()
{
    return m_database;
}

bool CConnection::IsAlive()
{
    if( m_connection != 0 )
        return m_connection->IsAlive();
    else
        return false;
}

void CConnection::SetDbName(const string& name,
                            CDB_Connection* conn)
{

    m_database = name;

    if( m_database.empty() )
        return;


    CDB_Connection* work = (conn == 0 ? m_connection : conn);
    string sql = "use " + m_database;
    CDB_LangCmd* cmd = work->LangCmd(sql.c_str());
    cmd->Send();
    while( cmd->HasMoreResults() ) {
        cmd->Result();
    }
    delete cmd;
}

CDB_Connection* CConnection::CloneCDB_Conn()
{
    CDB_Connection *temp = m_ds->
   GetDriverContext()->Connect(GetCDB_Connection()->ServerName(),
                               GetCDB_Connection()->UserName(),
                               GetCDB_Connection()->Password(),
                               m_modeMask,
                               true);
    _TRACE("CDB_Connection cloned");
    SetDbName(m_database, temp);
    return temp;
}

void CConnection::Close()
{
    delete m_connection;
    m_connection = 0;
}

CConnection* CConnection::Clone()
{
    CConnection *conn = new CConnection(CloneCDB_Conn(), m_ds);
    //conn->AddListener(m_ds);
    //m_ds->AddListener(conn);
    conn->SetDbName(GetDatabase());
    ++m_connCounter;
    return conn;
}

IConnection* CConnection::CloneConnection()
{
    CConnection *conn = new CConnection(m_ds);

    conn->m_modeMask = this->m_modeMask;
    conn->m_forceSingle = this->m_forceSingle;
    conn->m_connection = CloneCDB_Conn();
    conn->m_database = this->m_database;

    conn->AddListener(m_ds);
    m_ds->AddListener(conn);

    return conn;
}

// New part
IStatement* CConnection::GetStatement()
{
    if( m_connUsed ) 
        throw CDbapiException("CConnection::GetStatement(): Connection taken, cannot use this method");

    if( m_stmt == 0 ) {
        m_stmt = new CStatement(this);
        AddListener(m_stmt);
        m_stmt->AddListener(this);
    }
    return m_stmt;
}

ICallableStatement*
CConnection::GetCallableStatement(const string& proc,
                                  int nofArgs)
{
    if( m_connUsed ) 
        throw CDbapiException("CConnection::GetCallableStatement(): Connection taken, cannot use this method");

    if( m_cstmt == 0 ) {
        m_cstmt = new CCallableStatement(proc, nofArgs, this);
        AddListener(m_cstmt);
        m_cstmt->AddListener(this);
    }
    return m_cstmt;
}

ICursor* CConnection::GetCursor(const string& name,
                                const string& sql,
                                int nofArgs,
                                int batchSize)
{
    if( m_connUsed ) 
        throw CDbapiException("CConnection::GetCursor(): Connection taken, cannot use this method");

    if( m_cursor == 0 ) {
        m_cursor = new CCursor(name, sql, nofArgs, batchSize, this);
        AddListener(m_cursor);
        m_cursor->AddListener(this);
    }
    return m_cursor;
}

IBulkInsert* CConnection::GetBulkInsert(const string& table_name,
                                        unsigned int nof_cols)
{
    if( m_connUsed ) 
        throw CDbapiException("CConnection::GetBulkInsert(): Connection taken, cannot use this method");

    if( m_bulkInsert == 0 ) {
        m_bulkInsert = new CBulkInsert(table_name, nof_cols, this);
        AddListener(m_bulkInsert);
        m_bulkInsert->AddListener(this);
    }
    return m_bulkInsert;
}
// New part end


IStatement* CConnection::CreateStatement()
{
    if( m_stmt != 0 || m_cstmt != 0 || m_cursor != 0 || m_bulkInsert != 0 ) 
        throw CDbapiException("CConnection::CreateStatement(): Get...() methods used");
    
    CStatement *stmt = new CStatement(GetAuxConn());
    AddListener(stmt);
    stmt->AddListener(this);
    return stmt;
}

ICallableStatement*
CConnection::PrepareCall(const string& proc,
                         int nofArgs)
{
    if( m_stmt != 0 || m_cstmt != 0 || m_cursor != 0 || m_bulkInsert != 0 ) 
        throw CDbapiException("CConnection::CreateCallableStatement(): Get...() methods used");
    
    CCallableStatement *cstmt = new CCallableStatement(proc, nofArgs, GetAuxConn());
    AddListener(cstmt);
    cstmt->AddListener(this);
    return cstmt;
}

ICursor* CConnection::CreateCursor(const string& name,
                                   const string& sql,
                                   int nofArgs,
                                   int batchSize)
{
    if( m_stmt != 0 || m_cstmt != 0 || m_cursor != 0 || m_bulkInsert != 0 ) 
        throw CDbapiException("CConnection::CreateCursor(): Get...() methods used");
    
    CCursor *cur = new CCursor(name, sql, nofArgs, batchSize, GetAuxConn());
    AddListener(cur);
    cur->AddListener(this);
    return cur;
}

IBulkInsert* CConnection::CreateBulkInsert(const string& table_name,
                                           unsigned int nof_cols)
{
    if( m_stmt != 0 || m_cstmt != 0 || m_cursor != 0 || m_bulkInsert != 0 ) 
        throw CDbapiException("CConnection::CreateBulkInsert(): Get...() methods used");
    
    CBulkInsert *bcp = new CBulkInsert(table_name, nof_cols, GetAuxConn());
    AddListener(bcp);
    bcp->AddListener(this);
    return bcp;
}

void CConnection::Action(const CDbapiEvent& e)
{
    _TRACE(GetIdent() << " " << (void*)this << ": '" << e.GetName()
           << "' received from " << e.GetSource()->GetIdent());

    if(dynamic_cast<const CDbapiClosedEvent*>(&e) != 0 ) {
        if( m_connCounter > 1 ) {
            --m_connCounter;
            _TRACE("Server: " << GetCDB_Connection()->ServerName()
                   <<", connections left: " << m_connCounter);
        }
        else
            m_connUsed = false;

        CStatement *stmt;
        CCallableStatement *cstmt;
        CCursor *cursor;
        CBulkInsert *bulkInsert;
        if( (stmt = dynamic_cast<CStatement*>(e.GetSource())) != 0 ) {
            if( stmt == m_stmt ) {
                _TRACE("CConnection: Clearing cached statement " << (void*)m_stmt); 
                m_stmt = 0;
            }
        }
        else if( (cstmt = dynamic_cast<CCallableStatement*>(e.GetSource())) != 0 ) {
            if( cstmt == m_cstmt ) {
                _TRACE("CConnection: Clearing cached callable statement " << (void*)m_cstmt); 
                m_cstmt = 0;
            }
        }
        else if( (cursor = dynamic_cast<CCursor*>(e.GetSource())) != 0 ) {
            if( cursor == m_cursor ) {
                _TRACE("CConnection: Clearing cached cursor " << (void*)m_cursor); 
                m_cursor = 0;
            }
        }
        else if( (bulkInsert = dynamic_cast<CBulkInsert*>(e.GetSource())) != 0 ) {
            if( bulkInsert == m_bulkInsert ) {
                _TRACE("CConnection: Clearing cached bulkinsert " << (void*)m_bulkInsert); 
                m_bulkInsert = 0;
            }
        }
    }
    if(dynamic_cast<const CDbapiDeletedEvent*>(&e) != 0 ) {
        RemoveListener(e.GetSource());
        if(dynamic_cast<CDataSource*>(e.GetSource()) != 0 ) {
            delete this;
        }
    }
}

CConnection* CConnection::GetAuxConn()
{
    if( m_connCounter < 0 )
        return 0;

    CConnection *conn = this;
    if( m_connUsed && m_forceSingle ) {
        throw CDbapiException("GetAuxConn(): Extra connections not permitted");
    }
    if( m_connUsed ) {
        conn = Clone();
        _TRACE("GetAuxConn(): Server: " << GetCDB_Connection()->ServerName()
               << ", open aux connection, total: " << m_connCounter);
    }
    else {
        m_connUsed = true;

        _TRACE("GetAuxconn(): server: " << GetCDB_Connection()->ServerName()
               << ", no aux connections necessary, using default...");
    }

    return conn;

}

void CConnection::MsgToEx(bool v)
{
    if( !v ) {
        // Clear the previous handlers if present
        GetCDB_Connection()->PopMsgHandler(GetHandler());
    }
    else {
        GetCDB_Connection()->PushMsgHandler(GetHandler());
    }
}

CToMultiExHandler* CConnection::GetHandler()
{
    if(m_multiExH == 0 ) {
        m_multiExH = new CToMultiExHandler;
    }
    return m_multiExH;
}

CDB_MultiEx* CConnection::GetErrorAsEx()
{
    return GetHandler()->GetMultiEx();
}

string CConnection::GetErrorInfo()
{
    CNcbiOstrstream out;
    CDB_UserHandler_Stream h(&out);
    h.HandleIt(GetHandler()->GetMultiEx());
    return CNcbiOstrstreamToString(out);
}

/*
void CConnection::DeleteConn(CConnection* conn)
{
    if( m_connCounter > 1) {
        delete conn;
        --m_connCounter;
    }

    _TRACE("Connection deleted, total left: " << m_connCounter);
    return;
}
*/
END_NCBI_SCOPE
