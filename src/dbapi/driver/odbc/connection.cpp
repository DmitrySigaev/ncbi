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
 * Author:  Vladimir Soussov
 *
 * File Description:  ODBC connection
 *
 */

#include <ncbi_pch.hpp>
#include <dbapi/driver/odbc/interfaces.hpp>
#include <string.h>
#ifdef HAVE_ODBCSS_H
#include <odbcss.h>
#endif


BEGIN_NCBI_SCOPE

static bool ODBC_xSendDataPrepare(CStatementBase& stmt,
                                  CDB_ITDescriptor& descr_in,
                                  SQLLEN size,
                                  bool is_text,
                                  bool logit,
                                  SQLPOINTER id,
                                  SQLLEN* ph);

static bool ODBC_xSendDataGetId(CStatementBase& stmt,
                                SQLPOINTER* id);

CODBC_Connection::CODBC_Connection(CODBCContext& cntx,
                                   SQLHDBC conn,
                                   bool reusable,
                                   const string& pool_name) :
    impl::CConnection(cntx, false, reusable, pool_name),
    m_Link(conn),
    m_Reporter(0, SQL_HANDLE_DBC, conn, &cntx.GetReporter())
{
    m_Reporter.SetHandlerStack(&GetMsgHandlers());
}


bool CODBC_Connection::IsAlive()
{
    SQLINTEGER status;
    SQLRETURN r= SQLGetConnectAttr(m_Link, SQL_ATTR_CONNECTION_DEAD, &status, SQL_IS_INTEGER, 0);

    return ((r == SQL_SUCCESS || r == SQL_SUCCESS_WITH_INFO) && (status == SQL_CD_FALSE));
}


CODBC_LangCmd* CODBC_Connection::xLangCmd(const string& lang_query,
                                          unsigned int  nof_params)
{
    string extra_msg = "SQL Command: \"" + lang_query + "\"";
    m_Reporter.SetExtraMsg( extra_msg );

    CODBC_LangCmd* lcmd = new CODBC_LangCmd(this, lang_query, nof_params);
    return lcmd;
}

CDB_LangCmd* CODBC_Connection::LangCmd(const string& lang_query,
                                     unsigned int  nof_params)
{
    return Create_LangCmd(*(xLangCmd(lang_query, nof_params)));
}


CDB_RPCCmd* CODBC_Connection::RPC(const string& rpc_name,
                                unsigned int  nof_args)
{
    string extra_msg = "RPC Command: " + rpc_name;
    m_Reporter.SetExtraMsg( extra_msg );

    CODBC_RPCCmd* rcmd = new CODBC_RPCCmd(this, rpc_name, nof_args);
    return Create_RPCCmd(*rcmd);
}


CDB_BCPInCmd* CODBC_Connection::BCPIn(const string& table_name,
                                    unsigned int  nof_columns)
{
#ifdef FTDS_IN_USE
    return NULL; // not implemented yet
#else
    if ( !IsBCPable() ) {
        string err_message = "No bcp on this connection" + GetDiagnosticInfo();
        DATABASE_DRIVER_ERROR( err_message, 410003 );
    }

    CODBC_BCPInCmd* bcmd = new CODBC_BCPInCmd(this, m_Link, table_name, nof_columns);
    return Create_BCPInCmd(*bcmd);
#endif
}


CDB_CursorCmd* CODBC_Connection::Cursor(const string& cursor_name,
                                      const string& query,
                                      unsigned int  nof_params,
                                      unsigned int  batch_size)
{
    string extra_msg = "Cursor Name: \"" + cursor_name + "\"; SQL Command: \""+
        query + "\"";
    m_Reporter.SetExtraMsg( extra_msg );

    CODBC_CursorCmd* ccmd = new CODBC_CursorCmd(this,
                                                cursor_name,
                                                query,
                                                nof_params);
    return Create_CursorCmd(*ccmd);
}


CDB_SendDataCmd* CODBC_Connection::SendDataCmd(I_ITDescriptor& descr_in,
                                               size_t data_size, bool log_it)
{
    CODBC_SendDataCmd* sd_cmd =
        new CODBC_SendDataCmd(this,
                              (CDB_ITDescriptor&)descr_in,
                              data_size,
                              log_it);
    return Create_SendDataCmd(*sd_cmd);
}


bool CODBC_Connection::SendData(I_ITDescriptor& desc, CDB_Image& img, bool log_it)
{
    CStatementBase stmt(*this);

    SQLPOINTER  p = (SQLPOINTER)2;
    SQLLEN      s = img.Size();
    SQLLEN      ph;

    if((!ODBC_xSendDataPrepare(stmt, (CDB_ITDescriptor&)desc, s, false, log_it, p, &ph)) ||
       (!ODBC_xSendDataGetId(stmt, &p ))) {
        string err_message = "Cannot prepare a command" + GetDiagnosticInfo();
        DATABASE_DRIVER_ERROR( err_message, 410035 );
    }

    return x_SendData(stmt, img);

}


bool CODBC_Connection::SendData(I_ITDescriptor& desc, CDB_Text& txt, bool log_it)
{
    CStatementBase stmt(*this);

    SQLPOINTER  p = (SQLPOINTER)2;
    SQLLEN      s = txt.Size();
    SQLLEN      ph;

    if((!ODBC_xSendDataPrepare(stmt, (CDB_ITDescriptor&)desc, s, true, log_it, p, &ph)) ||
       (!ODBC_xSendDataGetId(stmt, &p))) {
        string err_message = "Cannot prepare a command" + GetDiagnosticInfo();
        DATABASE_DRIVER_ERROR( err_message, 410035 );
    }

    return x_SendData(stmt, txt);
}


bool CODBC_Connection::Refresh()
{
    // close all commands first
    DeleteAllCommands();

    return IsAlive();
}


I_DriverContext::TConnectionMode CODBC_Connection::ConnectMode() const
{
    I_DriverContext::TConnectionMode mode = 0;
    if ( IsBCPable() ) {
        mode |= I_DriverContext::fBcpIn;
    }
    if ( HasSecureLogin() ) {
        mode |= I_DriverContext::fPasswordEncrypted;
    }
    return mode;
}


CODBC_Connection::~CODBC_Connection()
{
    try {
        Close();
    }
    NCBI_CATCH_ALL( NCBI_CURRENT_FUNCTION )
}


bool CODBC_Connection::Abort()
{
    SQLDisconnect(m_Link);
    return false;
}

bool CODBC_Connection::Close(void)
{
    if (Refresh()) {
        switch(SQLDisconnect(m_Link)) {
        case SQL_SUCCESS_WITH_INFO:
        case SQL_ERROR:
            ReportErrors();
        case SQL_SUCCESS:
            break;
        default:
            {
                string err_message = "SQLDisconnect failed (memory corruption suspected)" + GetDiagnosticInfo();
                DATABASE_DRIVER_ERROR( err_message, 410009 );
            }
        }

        if(SQLFreeHandle(SQL_HANDLE_DBC, m_Link) == SQL_ERROR) {
            ReportErrors();
        }

        m_Link = NULL;

        return true;
    }

    return false;
}

static bool ODBC_xSendDataPrepare(// CODBC_Connection& conn,
                                  CStatementBase& stmt,
                                  CDB_ITDescriptor& descr_in,
                                  SQLLEN size,
                                  bool is_text,
                                  bool logit,
                                  SQLPOINTER id,
                                  SQLLEN* ph)
{
    string q= "update ";
    q+= descr_in.TableName();
    q+= " set ";
    q+= descr_in.ColumnName();
    q+= "=? where ";
    q+= descr_in.SearchConditions();
    //q+= " ;\nset rowcount 0";

#ifdef SQL_TEXTPTR_LOGGING
    if(!logit) {
        switch(SQLSetStmtAttr(stmt.GetHandle(), SQL_TEXTPTR_LOGGING, /*SQL_SOPT_SS_TEXTPTR_LOGGING,*/
            (SQLPOINTER)SQL_TL_OFF, SQL_IS_INTEGER)) {
        case SQL_SUCCESS_WITH_INFO:
        case SQL_ERROR:
            stmt.ReportErrors();
        default: break;
        }
    }
#endif

    switch(SQLPrepare(stmt.GetHandle(), (SQLCHAR*)q.c_str(), SQL_NTS)) {
    case SQL_SUCCESS_WITH_INFO:
        stmt.ReportErrors();
    case SQL_SUCCESS: break;
    case SQL_ERROR:
        stmt.ReportErrors();
    default: return false;
    }

    SQLSMALLINT par_type, par_dig, par_null;
    SQLULEN par_size;

#if 0
    switch(SQLNumParams(stmt.GetHandle(), &par_dig)) {
    case SQL_SUCCESS: break;
    case SQL_SUCCESS_WITH_INFO:
    case SQL_ERROR:
        stmt.ReportErrors();
    default: return false;
    }
#endif

    switch(SQLDescribeParam(stmt.GetHandle(), 1, &par_type, &par_size, &par_dig, &par_null)){
    case SQL_SUCCESS_WITH_INFO:
        stmt.ReportErrors();
    case SQL_SUCCESS: break;
    case SQL_ERROR:
        stmt.ReportErrors();
    default: return false;
    }

    *ph = SQL_LEN_DATA_AT_EXEC(size);

    switch(SQLBindParameter(stmt.GetHandle(), 1, SQL_PARAM_INPUT,
                     is_text? SQL_C_CHAR : SQL_C_BINARY, par_type,
                     // is_text? SQL_LONGVARCHAR : SQL_LONGVARBINARY,
                     size, 0, id, 0, ph)) {
    case SQL_SUCCESS_WITH_INFO:
        stmt.ReportErrors();
    case SQL_SUCCESS: break;
    case SQL_ERROR:
        stmt.ReportErrors();
    default:
        return false;
    }



    switch(SQLExecute(stmt.GetHandle())) {
    case SQL_NEED_DATA:
        return true;
    case SQL_SUCCESS_WITH_INFO:
    case SQL_ERROR:
        stmt.ReportErrors();
    default:
        return false;
    }
}

static bool ODBC_xSendDataGetId(CStatementBase& stmt,
                                SQLPOINTER* id)
{
    switch(SQLParamData(stmt.GetHandle(), id)) {
    case SQL_NEED_DATA:
        return true;
    case SQL_SUCCESS_WITH_INFO:
    case SQL_ERROR:
        stmt.ReportErrors();
    default:
        return false;
    }
}

bool CODBC_Connection::x_SendData(CStatementBase& stmt,
                                  CDB_Stream& stream)
{
    char buff[1801];
    size_t s;

    while(( s = stream.Read(buff, sizeof(buff))) != 0 ) {
        switch(SQLPutData(stmt.GetHandle(), (SQLPOINTER)buff, (SQLINTEGER)s)) {
        case SQL_SUCCESS_WITH_INFO:
            stmt.ReportErrors();
        case SQL_NEED_DATA:
            continue;
        case SQL_NO_DATA:
            return true;
        case SQL_SUCCESS:
            break;
        case SQL_ERROR:
            stmt.ReportErrors();
        default:
            return false;
        }
    }
    switch(SQLParamData(stmt.GetHandle(), (SQLPOINTER*)&s)) {
    case SQL_SUCCESS_WITH_INFO: stmt.ReportErrors();
    case SQL_SUCCESS:           break;
    case SQL_NO_DATA:           return true;
    case SQL_NEED_DATA:
        {
            string err_message = "Not all the data were sent" + GetDiagnosticInfo();
            DATABASE_DRIVER_ERROR( err_message, 410044 );
        }
    case SQL_ERROR:             stmt.ReportErrors();
    default:
        {
            string err_message = "SQLParamData failed" + GetDiagnosticInfo();
            DATABASE_DRIVER_ERROR( err_message, 410045 );
        }
    }

    for(;;) {
        switch(SQLMoreResults( stmt.GetHandle() )) {
        case SQL_SUCCESS_WITH_INFO: stmt.ReportErrors();
        case SQL_SUCCESS:           continue;
        case SQL_NO_DATA:           break;
        case SQL_ERROR:
            {
                stmt.ReportErrors();
                string err_message = "SQLMoreResults failed" + GetDiagnosticInfo();
                DATABASE_DRIVER_ERROR( err_message, 410014 );
            }
        default:
            {
                string err_message = "SQLMoreResults failed (memory corruption suspected)" + GetDiagnosticInfo();
                DATABASE_DRIVER_ERROR( err_message, 410015 );
            }
        }
        break;
    }
    return true;
}

void CODBC_Connection::ODBC_SetTimeout(SQLULEN nof_secs) {}
void CODBC_Connection::ODBC_SetTextImageSize(SQLULEN nof_bytes) {}


/////////////////////////////////////////////////////////////////////////////
CStatementBase::CStatementBase(CODBC_Connection& conn) :
    m_RowCount(-1),
    m_WasSent(false),
    m_HasFailed(false),
    m_ConnectPtr(&conn),
    m_Reporter(&conn.GetMsgHandlers(), SQL_HANDLE_STMT, NULL, &conn.m_Reporter)
{
    SQLRETURN rc = SQLAllocHandle(SQL_HANDLE_STMT, conn.m_Link, &m_Cmd);

    if(rc == SQL_ERROR) {
        conn.ReportErrors();
    }
    m_Reporter.SetHandle(m_Cmd);
}

CStatementBase::~CStatementBase(void)
{
    try {
        SQLRETURN rc = SQLFreeHandle(SQL_HANDLE_STMT, m_Cmd);
        if(rc != SQL_SUCCESS) {
            ReportErrors();
        }
    }
    NCBI_CATCH_ALL( NCBI_CURRENT_FUNCTION )
}

bool
CStatementBase::CheckRC(int rc) const
{
    switch (rc)
    {
    case SQL_SUCCESS:
        return true;
    case SQL_SUCCESS_WITH_INFO:
    case SQL_ERROR:
        ReportErrors();
        break;
    case SQL_INVALID_HANDLE:
        {
            string err_message = "Invalid handle" + GetDiagnosticInfo();
            DATABASE_DRIVER_ERROR( err_message, 0 );
        }
        break;
    }

    return false;
}

/////////////////////////////////////////////////////////////////////////////
//
//  CODBC_SendDataCmd::
//

CODBC_SendDataCmd::CODBC_SendDataCmd(CODBC_Connection* conn,
                                     CDB_ITDescriptor& descr,
                                     size_t nof_bytes,
                                     bool logit) :
    CStatementBase(*conn),
    impl::CSendDataCmd(nof_bytes)
{
    SQLPOINTER p = (SQLPOINTER)1;
    if((!ODBC_xSendDataPrepare(*this, descr, (SQLINTEGER)nof_bytes,
                              false, logit, p, &m_ParamPH)) ||
       (!ODBC_xSendDataGetId(*this, &p))) {

        string err_message = "Cannot prepare a command" + GetDiagnosticInfo();
        DATABASE_DRIVER_ERROR( err_message, 410035 );
    }
}

size_t CODBC_SendDataCmd::SendChunk(const void* chunk_ptr, size_t nof_bytes)
{
    if(nof_bytes > m_Bytes2go) nof_bytes= m_Bytes2go;
    if(nof_bytes < 1) return 0;

    switch(SQLPutData(GetHandle(), (SQLPOINTER)chunk_ptr, (SQLINTEGER)nof_bytes)) {
    case SQL_SUCCESS_WITH_INFO:
        ReportErrors();
    case SQL_NEED_DATA:
    case SQL_NO_DATA:
    case SQL_SUCCESS:
        m_Bytes2go-= nof_bytes;
        if(m_Bytes2go == 0) break;
        return nof_bytes;
    case SQL_ERROR:
        ReportErrors();
    default:
        return 0;
    }

    SQLPOINTER s= (SQLPOINTER)1;
    switch(SQLParamData(GetHandle(), (SQLPOINTER*)&s)) {
    case SQL_SUCCESS_WITH_INFO: ReportErrors();
    case SQL_SUCCESS:           break;
    case SQL_NO_DATA:           break;
    case SQL_NEED_DATA:
        {
            string err_message = "Not all the data were sent" + GetDiagnosticInfo();
            DATABASE_DRIVER_ERROR( err_message, 410044 );
        }
    case SQL_ERROR:             ReportErrors();
    default:
        {
            string err_message = "SQLParamData failed" + GetDiagnosticInfo();
            DATABASE_DRIVER_ERROR( err_message, 410045 );
        }
    }

    for(;;) {
        switch(SQLMoreResults(GetHandle())) {
        case SQL_SUCCESS_WITH_INFO: ReportErrors();
        case SQL_SUCCESS:           continue;
        case SQL_NO_DATA:           break;
        case SQL_ERROR:
            {
                ReportErrors();
                string err_message = "SQLMoreResults failed" + GetDiagnosticInfo();
                DATABASE_DRIVER_ERROR( err_message, 410014 );
            }
        default:
            {
                string err_message = "SQLMoreResults failed (memory corruption suspected)" + GetDiagnosticInfo();
                DATABASE_DRIVER_ERROR( err_message, 410015 );
            }
        }
        break;
    }
    return nof_bytes;
}

bool CODBC_SendDataCmd::Cancel(void)
{
    if (m_Bytes2go > 0) {
        xCancel();
        m_Bytes2go = 0;
        return true;
    }

    return false;
}

CODBC_SendDataCmd::~CODBC_SendDataCmd()
{
    try {
        DetachInterface();

        GetConnection().DropCmd(*this);

        Cancel();
    }
    NCBI_CATCH_ALL( NCBI_CURRENT_FUNCTION )
}

void CODBC_SendDataCmd::xCancel()
{
    if ( !Close() ) {
        return;
    }
    ResetParams();
}

END_NCBI_SCOPE



/*
 * ===========================================================================
 * $Log$
 * Revision 1.34  2006/07/31 21:35:21  ssikorsk
 * Disable BCP for the ftds64_odbc driver temporarily.
 *
 * Revision 1.33  2006/07/18 15:47:59  ssikorsk
 * LangCmd, RPCCmd, and BCPInCmd have common base class impl::CBaseCmd now.
 *
 * Revision 1.32  2006/07/12 17:11:11  ssikorsk
 * Fixed compilation isssues.
 *
 * Revision 1.31  2006/07/12 16:29:31  ssikorsk
 * Separated interface and implementation of CDB classes.
 *
 * Revision 1.30  2006/06/09 19:59:22  ssikorsk
 * Fixed CDB_BaseEnt garbage collector logic.
 *
 * Revision 1.29  2006/06/05 21:07:25  ssikorsk
 * Deleted CODBC_Connection::Release;
 * Replaced "m_BR = 0" with "CDB_BaseEnt::Release()";
 *
 * Revision 1.28  2006/06/05 19:10:06  ssikorsk
 * Moved logic from C...Cmd::Release into dtor.
 *
 * Revision 1.27  2006/06/05 18:10:07  ssikorsk
 * Revamp code to use methods Cancel and Close more efficient.
 *
 * Revision 1.26  2006/06/05 15:05:50  ssikorsk
 * Revamp code to use GetMsgHandlers() and DeleteAllCommands() instead of
 * m_MsgHandlers and m_CMDs.
 *
 * Revision 1.25  2006/06/05 14:44:32  ssikorsk
 * Moved methods PushMsgHandler, PopMsgHandler and DropCmd into I_Connection.
 *
 * Revision 1.24  2006/06/02 19:37:40  ssikorsk
 * + NCBI_CATCH_ALL( NCBI_CURRENT_FUNCTION )
 *
 * Revision 1.23  2006/05/31 16:56:11  ssikorsk
 * Replaced CPointerPot with deque<CDB_BaseEnt*>
 *
 * Revision 1.22  2006/05/15 19:39:30  ssikorsk
 * Added EOwnership argument to method PushMsgHandler.
 *
 * Revision 1.21  2006/04/05 14:32:51  ssikorsk
 * Implemented CODBC_Connection::Close
 *
 * Revision 1.20  2006/02/28 15:14:30  ssikorsk
 * Replaced argument type SQLINTEGER on SQLLEN where needed.
 *
 * Revision 1.19  2006/02/28 15:00:45  ssikorsk
 * Use larger type (SQLLEN) instead of SQLINTEGER where it needs to be converted to a pointer.
 *
 * Revision 1.18  2006/02/28 14:27:30  ssikorsk
 * Replaced int/SQLINTEGER variables with SQLLEN where needed.
 *
 * Revision 1.17  2006/02/28 14:00:47  ssikorsk
 * Fixed argument type misuse (like SQLINTEGER and SQLLEN) for vc8-x64 sake.
 *
 * Revision 1.16  2006/02/17 18:00:29  ssikorsk
 * Set CODBC_Connection::m_BCPable to false in ctor.
 *
 * Revision 1.15  2005/12/01 14:40:32  ssikorsk
 * Staticfunctions ODBC_xSendDataPrepare and ODBC_xSendDataGetId take
 * statement instead of connection as a parameter now.
 *
 * Revision 1.14  2005/11/28 13:22:59  ssikorsk
 * Report SQL statement and database connection parameters in case
 * of an error in addition to a server error message.
 *
 * Revision 1.13  2005/11/02 16:46:21  ssikorsk
 * Pass context information with an error message of a database exception.
 *
 * Revision 1.12  2005/11/02 12:58:38  ssikorsk
 * Report extra information in exceptions and error messages.
 *
 * Revision 1.11  2005/09/19 14:19:05  ssikorsk
 * Use NCBI_CATCH_ALL macro instead of catch(...)
 *
 * Revision 1.10  2005/09/15 11:00:02  ssikorsk
 * Destructors do not throw exceptions any more.
 *
 * Revision 1.9  2005/08/09 20:41:28  soussov
 * adds SQLDisconnect to Abort
 *
 * Revision 1.8  2005/04/04 13:03:57  ssikorsk
 * Revamp of DBAPI exception class CDB_Exception
 *
 * Revision 1.7  2005/02/23 21:40:55  soussov
 * Adds Abort() method to connection
 *
 * Revision 1.6  2004/12/21 22:17:16  soussov
 * fixes bug in SendDataCmd
 *
 * Revision 1.5  2004/05/17 21:16:05  gorelenk
 * Added include of PCH ncbi_pch.hpp
 *
 * Revision 1.4  2003/06/05 16:02:04  soussov
 * adds code for DumpResults and for the dumped results processing
 *
 * Revision 1.3  2003/05/05 20:47:45  ucko
 * Check HAVE_ODBCSS_H; regardless, disable BCP on Unix, since even
 * DataDirect's implementation lacks the relevant bcp_* functions.
 *
 * Revision 1.2  2002/07/05 20:47:56  soussov
 * fixes bug in SendData
 *
 *
 * ===========================================================================
 */
