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

#include <dbapi/driver/odbc/interfaces.hpp>
#include <string.h>
#ifdef HAVE_ODBCSS_H
#include <odbcss.h>
#endif


BEGIN_NCBI_SCOPE

static bool ODBC_xSendDataPrepare(SQLHSTMT cmd, CDB_ITDescriptor& descr_in,
                                  SQLINTEGER size, bool is_text, bool logit, 
						          SQLPOINTER id, CODBC_Reporter& rep);
static bool ODBC_xSendDataGetId(SQLHSTMT cmd, SQLPOINTER* id, 
                                CODBC_Reporter& rep);

CODBC_Connection::CODBC_Connection(CODBCContext* cntx, SQLHDBC con,
                                   bool reusable, const string& pool_name):
    m_Reporter(0, SQL_HANDLE_DBC, con)
{
    m_Link     = con;
    m_Context  = cntx;
    m_Reusable = reusable;
    m_Pool     = pool_name;
    m_Reporter.SetHandlerStack(&m_MsgHandlers);
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
    SQLHSTMT cmd;
    SQLRETURN r= SQLAllocHandle(SQL_HANDLE_STMT, m_Link, &cmd);

    if(r == SQL_ERROR) {
        m_Reporter.ReportErrors();
    }

    CODBC_LangCmd* lcmd = new CODBC_LangCmd(this, cmd, lang_query, nof_params);
    m_CMDs.Add(lcmd);
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
#if 0
    return 0;
#else
    SQLHSTMT cmd;
    SQLRETURN r= SQLAllocHandle(SQL_HANDLE_STMT, m_Link, &cmd);

    if(r == SQL_ERROR) {
        m_Reporter.ReportErrors();
    }

    CODBC_RPCCmd* rcmd = new CODBC_RPCCmd(this, cmd, rpc_name, nof_args);
    m_CMDs.Add(rcmd);
    return Create_RPCCmd(*rcmd);
#endif
}


CDB_BCPInCmd* CODBC_Connection::BCPIn(const string& table_name,
                                    unsigned int  nof_columns)
{
#ifdef NCBI_OS_UNIX
    return 0; // not implemented
#else
    if (!m_BCPable) {
        throw CDB_ClientEx(eDB_Error, 410003, "CODBC_Connection::BCPIn",
                           "No bcp on this connection");
    }

    CODBC_BCPInCmd* bcmd = new CODBC_BCPInCmd(this, m_Link, table_name, nof_columns);
    m_CMDs.Add(bcmd);
    return Create_BCPInCmd(*bcmd);
#endif
}


CDB_CursorCmd* CODBC_Connection::Cursor(const string& cursor_name,
                                      const string& query,
                                      unsigned int  nof_params,
                                      unsigned int  batch_size)
{
#if 0
    return 0;
#else
    SQLHSTMT cmd;
    SQLRETURN r= SQLAllocHandle(SQL_HANDLE_STMT, m_Link, &cmd);

    if(r == SQL_ERROR) {
        m_Reporter.ReportErrors();
    }

    CODBC_CursorCmd* ccmd = new CODBC_CursorCmd(this, cmd, cursor_name, query,
                                                nof_params);
    m_CMDs.Add(ccmd);
    return Create_CursorCmd(*ccmd);
#endif
}


CDB_SendDataCmd* CODBC_Connection::SendDataCmd(I_ITDescriptor& descr_in,
                                               size_t data_size, bool log_it)
{
    SQLHSTMT cmd;
    SQLRETURN r= SQLAllocHandle(SQL_HANDLE_STMT, m_Link, &cmd);

    if(r == SQL_ERROR) {
        m_Reporter.ReportErrors();
    }

    CODBC_SendDataCmd* sd_cmd = 
        new CODBC_SendDataCmd(this, cmd, 
                              (CDB_ITDescriptor&)descr_in,  
                              data_size, log_it);
    m_CMDs.Add(sd_cmd);
    return Create_SendDataCmd(*sd_cmd);
}


bool CODBC_Connection::SendData(I_ITDescriptor& desc, CDB_Image& img, bool log_it)
{
    SQLHSTMT cmd;
    SQLRETURN r= SQLAllocHandle(SQL_HANDLE_STMT, m_Link, &cmd);

    if(r == SQL_ERROR) {
        m_Reporter.ReportErrors();
    }

    CODBC_Reporter lrep(&m_MsgHandlers, SQL_HANDLE_STMT, cmd);
    SQLPOINTER p= (SQLPOINTER)2;
    SQLINTEGER s= img.Size();

    if((!ODBC_xSendDataPrepare(cmd, (CDB_ITDescriptor&)desc, s, false, log_it, p, lrep)) ||
       (!ODBC_xSendDataGetId(cmd, &p, lrep))) {
        throw CDB_ClientEx(eDB_Error, 410035, "CODBC_Connection::SendData",
                           "can not prepare a command");
    }

    return x_SendData(cmd, img, lrep);
    
}


bool CODBC_Connection::SendData(I_ITDescriptor& desc, CDB_Text& txt, bool log_it)
{
    SQLHSTMT cmd;
    SQLRETURN r= SQLAllocHandle(SQL_HANDLE_STMT, m_Link, &cmd);

    if(r == SQL_ERROR) {
        m_Reporter.ReportErrors();
    }


    CODBC_Reporter lrep(&m_MsgHandlers, SQL_HANDLE_STMT, cmd);
    SQLPOINTER p= (SQLPOINTER)2;
    SQLINTEGER s= txt.Size();

    if((!ODBC_xSendDataPrepare(cmd, (CDB_ITDescriptor&)desc, s, true, log_it, p, lrep)) ||
       (!ODBC_xSendDataGetId(cmd, &p, lrep))) {
        throw CDB_ClientEx(eDB_Error, 410035, "CODBC_Connection::SendData",
                           "can not prepare a command");
    }

    return x_SendData(cmd, txt, lrep);
}


bool CODBC_Connection::Refresh()
{
    // close all commands first
    while(m_CMDs.NofItems() > 0) {
        CDB_BaseEnt* pCmd = static_cast<CDB_BaseEnt*> (m_CMDs.Get(0));
        delete pCmd;
        m_CMDs.Remove((int) 0);
    }

    return IsAlive();
}


const string& CODBC_Connection::ServerName() const
{
    return m_Server;
}


const string& CODBC_Connection::UserName() const
{
    return m_User;
}


const string& CODBC_Connection::Password() const
{
    return m_Passwd;
}


I_DriverContext::TConnectionMode CODBC_Connection::ConnectMode() const
{
    I_DriverContext::TConnectionMode mode = 0;
    if ( m_BCPable ) {
        mode |= I_DriverContext::fBcpIn;
    }
    if ( m_SecureLogin ) {
        mode |= I_DriverContext::fPasswordEncrypted;
    }
    return mode;
}


bool CODBC_Connection::IsReusable() const
{
    return m_Reusable;
}


const string& CODBC_Connection::PoolName() const
{
    return m_Pool;
}


I_DriverContext* CODBC_Connection::Context() const
{
    return const_cast<CODBCContext*> (m_Context);
}


void CODBC_Connection::PushMsgHandler(CDB_UserHandler* h)
{
    m_MsgHandlers.Push(h);
}


void CODBC_Connection::PopMsgHandler(CDB_UserHandler* h)
{
    m_MsgHandlers.Pop(h);
}


void CODBC_Connection::Release()
{
    m_BR = 0;
    // close all commands first
    while(m_CMDs.NofItems() > 0) {
        CDB_BaseEnt* pCmd = static_cast<CDB_BaseEnt*> (m_CMDs.Get(0));
        delete pCmd;
        m_CMDs.Remove((int) 0);
    }
}


CODBC_Connection::~CODBC_Connection()
{
    if (Refresh()) {
        switch(SQLDisconnect(m_Link)) {
        case SQL_SUCCESS_WITH_INFO:
        case SQL_ERROR:
            m_Reporter.ReportErrors();
        case SQL_SUCCESS:
            break;
        default:
            throw CDB_ClientEx(eDB_Error, 410009, "CODBC_Connection::~CODBC_Connection",
                               "SQLDisconnect failed (memory corruption suspected)");
            
        }
    }
    if(SQLFreeHandle(SQL_HANDLE_DBC, m_Link) == SQL_ERROR) {
        m_Reporter.ReportErrors();
    }
        
}


void CODBC_Connection::DropCmd(CDB_BaseEnt& cmd)
{
    m_CMDs.Remove(static_cast<TPotItem> (&cmd));
}


static bool ODBC_xSendDataPrepare(SQLHSTMT cmd, CDB_ITDescriptor& descr_in,
                                  SQLINTEGER size, bool is_text, bool logit, 
						          SQLPOINTER id, CODBC_Reporter& rep)
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
		switch(SQLSetStmtAttr(cmd, SQL_TEXTPTR_LOGGING, /*SQL_SOPT_SS_TEXTPTR_LOGGING,*/
			(SQLPOINTER)SQL_TL_OFF, SQL_IS_INTEGER)) {
		case SQL_SUCCESS_WITH_INFO:
		case SQL_ERROR:
			rep.ReportErrors();
		default: break;
		}
	}
#endif

	switch(SQLPrepare(cmd, (SQLCHAR*)q.c_str(), SQL_NTS)) {
	case SQL_SUCCESS_WITH_INFO:
		rep.ReportErrors();
	case SQL_SUCCESS: break;
	case SQL_ERROR:
		rep.ReportErrors();
	default: return false;
	}
			
	SQLSMALLINT par_type, par_dig, par_null;
    SQLUINTEGER par_size;

#if 0
	switch(SQLNumParams(cmd, &par_dig)) {
	case SQL_SUCCESS: break;
	case SQL_SUCCESS_WITH_INFO:
	case SQL_ERROR:
		rep.ReportErrors();
	default: return false;
	}
#endif

	switch(SQLDescribeParam(cmd, 1, &par_type, &par_size, &par_dig, &par_null)){
	case SQL_SUCCESS_WITH_INFO:
		rep.ReportErrors();
	case SQL_SUCCESS: break;
	case SQL_ERROR:
		rep.ReportErrors();
	default: return false;
	}

    SQLINTEGER p= SQL_LEN_DATA_AT_EXEC(size);

    switch(SQLBindParameter(cmd, 1, SQL_PARAM_INPUT, 
                     is_text? SQL_C_CHAR : SQL_C_BINARY, par_type,
                     // is_text? SQL_LONGVARCHAR : SQL_LONGVARBINARY,
                     size, 0, id, 0, &p)) {
	case SQL_SUCCESS_WITH_INFO:
		rep.ReportErrors();
	case SQL_SUCCESS: break;
	case SQL_ERROR:
		rep.ReportErrors();
	default: return false;
	}
		

    
    switch(SQLExecute(cmd)) {
    case SQL_NEED_DATA:
        return true;
    case SQL_SUCCESS_WITH_INFO:
    case SQL_ERROR:
        rep.ReportErrors();
    default:
        return false;
    }
}

static bool ODBC_xSendDataGetId(SQLHSTMT cmd, SQLPOINTER* id, 
                                CODBC_Reporter& rep)
{
    switch(SQLParamData(cmd, id)) {
    case SQL_NEED_DATA:
        return true;
    case SQL_SUCCESS_WITH_INFO:
    case SQL_ERROR:
        rep.ReportErrors();
    default:
        return false;
    }
}

bool CODBC_Connection::x_SendData(SQLHSTMT cmd, CDB_Stream& stream, CODBC_Reporter& rep)
{
    char buff[1801];
    size_t s;

    while((s= stream.Read(buff, sizeof(buff))) != 0) {
        switch(SQLPutData(cmd, (SQLPOINTER)buff, (SQLINTEGER)s)) {
        case SQL_SUCCESS_WITH_INFO:
            rep.ReportErrors();
        case SQL_NEED_DATA:
			continue;
        case SQL_NO_DATA:
			return true;
        case SQL_SUCCESS:
            break;
        case SQL_ERROR:
            rep.ReportErrors();
        default:
            return false;
        }
    }
    switch(SQLParamData(cmd, (SQLPOINTER*)&s)) {
	case SQL_SUCCESS_WITH_INFO: rep.ReportErrors();
    case SQL_SUCCESS:           break;
	case SQL_NO_DATA:           return true;
    case SQL_NEED_DATA: 
	    throw CDB_ClientEx(eDB_Error, 410044, "CODBC_Connection::SendData",
                               "Not all the data were sent");
	case SQL_ERROR:             rep.ReportErrors();
	default:
		throw CDB_ClientEx(eDB_Error, 410045, "CODBC_Connection::SendData",
                           "SQLParamData failed");
	}
	
    for(;;) {
        switch(SQLMoreResults(cmd)) {
        case SQL_SUCCESS_WITH_INFO: rep.ReportErrors();
        case SQL_SUCCESS:           continue;
        case SQL_NO_DATA:           break;
        case SQL_ERROR:             
            rep.ReportErrors();
            throw CDB_ClientEx(eDB_Error, 410014, "CODBC_Connection::SendData",
                               "SQLMoreResults failed");
        default:
            throw CDB_ClientEx(eDB_Error, 410015, "CODBC_Connection::SendData",
                               "SQLMoreResults failed (memory corruption suspected)");
        }
		break;
    }
    return true;
}
        
void CODBC_Connection::ODBC_SetTimeout(SQLUINTEGER nof_secs) {}
void CODBC_Connection::ODBC_SetTextImageSize(SQLUINTEGER nof_bytes) {}


/////////////////////////////////////////////////////////////////////////////
//
//  CODBC_SendDataCmd::
//

CODBC_SendDataCmd::CODBC_SendDataCmd(CODBC_Connection* conn, 
                                     SQLHSTMT cmd, 
                                     CDB_ITDescriptor& descr,
                                     size_t nof_bytes, bool logit) :
    m_Connect(conn), m_Cmd(cmd), m_Bytes2go(nof_bytes),
    m_Reporter(&conn->m_MsgHandlers, SQL_HANDLE_STMT, cmd)
{
    SQLPOINTER p= (SQLPOINTER)1;
    if((!ODBC_xSendDataPrepare(cmd, descr, (SQLINTEGER)nof_bytes,
                              false, logit, p, m_Reporter)) ||
       (!ODBC_xSendDataGetId(cmd, &p, m_Reporter))) {
        throw CDB_ClientEx(eDB_Error, 410035, "CODBC_SendDataCmd::CODBC_SendDataCm",
                           "can not prepare a command");
    }   
}

size_t CODBC_SendDataCmd::SendChunk(const void* chunk_ptr, size_t nof_bytes)
{
    if(nof_bytes > m_Bytes2go) nof_bytes= m_Bytes2go;
    if(nof_bytes < 1) return 0;

    switch(SQLPutData(m_Cmd, (SQLPOINTER)chunk_ptr, (SQLINTEGER)nof_bytes)) {
    case SQL_SUCCESS_WITH_INFO:
        m_Reporter.ReportErrors();
    case SQL_NEED_DATA:
    case SQL_NO_DATA:
    case SQL_SUCCESS:
        m_Bytes2go-= nof_bytes;
        if(m_Bytes2go == 0) break;
        return nof_bytes;
    case SQL_ERROR:
        m_Reporter.ReportErrors();
    default:
        return 0;
    }

    size_t s;
    switch(SQLParamData(m_Cmd, (SQLPOINTER*)&s)) {
	case SQL_SUCCESS_WITH_INFO: m_Reporter.ReportErrors();
    case SQL_SUCCESS:           break;
	case SQL_NO_DATA:           break;
    case SQL_NEED_DATA: 
	    throw CDB_ClientEx(eDB_Error, 410044, "CODBC_Connection::SendChunk",
                               "Not all the data were sent");
	case SQL_ERROR:             m_Reporter.ReportErrors();
	default:
		throw CDB_ClientEx(eDB_Error, 410045, "CODBC_Connection::SendChunk",
                           "SQLParamData failed");
	}

    for(;;) {
        switch(SQLMoreResults(m_Cmd)) {
        case SQL_SUCCESS_WITH_INFO: m_Reporter.ReportErrors();
        case SQL_SUCCESS:           continue;
        case SQL_NO_DATA:           break;
        case SQL_ERROR:             
            m_Reporter.ReportErrors();
            throw CDB_ClientEx(eDB_Error, 410014, "CODBC_SendDataCmd::SendChunk",
                               "SQLMoreResults failed");
        default:
            throw CDB_ClientEx(eDB_Error, 410015, "CODBC_SendDataCmd::SendChunk",
                               "SQLMoreResults failed (memory corruption suspected)");
        }
		break;
    }
    return nof_bytes;
}

void CODBC_SendDataCmd::Release()
{
    m_BR = 0;
    if (m_Bytes2go > 0) {
        xCancel();
        m_Bytes2go = 0;
    }
    m_Connect->DropCmd(*this);
    delete this;
}


CODBC_SendDataCmd::~CODBC_SendDataCmd()
{
    if (m_Bytes2go > 0)
        xCancel();
    if (m_BR)
        *m_BR = 0;
    SQLFreeHandle(SQL_HANDLE_STMT, m_Cmd);
}

void CODBC_SendDataCmd::xCancel()
{
    switch(SQLFreeStmt(m_Cmd, SQL_CLOSE)) {
    case SQL_SUCCESS_WITH_INFO: m_Reporter.ReportErrors();
    case SQL_SUCCESS:           break;
    case SQL_ERROR:             m_Reporter.ReportErrors();
    default:                    return;
    }
    SQLFreeStmt(m_Cmd, SQL_RESET_PARAMS);
}

END_NCBI_SCOPE



/*
 * ===========================================================================
 * $Log$
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
