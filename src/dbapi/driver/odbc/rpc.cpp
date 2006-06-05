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
 * File Description:  ODBC RPC command
 *
 */

#include <ncbi_pch.hpp>
#include <dbapi/driver/odbc/interfaces.hpp>
#include <dbapi/driver/util/numeric_convert.hpp>

#include <stdio.h>

BEGIN_NCBI_SCOPE


/////////////////////////////////////////////////////////////////////////////
//
//  CODBC_RPCCmd::
//

CODBC_RPCCmd::CODBC_RPCCmd(CODBC_Connection* conn,
                           const string& proc_name,
                           unsigned int nof_params)
: CStatementBase(*conn)
, m_Query(proc_name)
, m_Params(nof_params)
, m_WasSent(false)
, m_HasFailed(false)
, m_Recompile(false)
, m_Res(0)
, m_RowCount(-1)
{
    string extra_msg = "Procedure Name: " + proc_name;
    SetDiagnosticInfo( extra_msg );

    return;
}


bool CODBC_RPCCmd::BindParam(const string& param_name,
                            CDB_Object* param_ptr, bool out_param)
{
    return m_Params.BindParam(CDB_Params::kNoParamNumber, param_name,
                              param_ptr, out_param);
}


bool CODBC_RPCCmd::SetParam(const string& param_name,
                           CDB_Object* param_ptr, bool out_param)
{
    return m_Params.SetParam(CDB_Params::kNoParamNumber, param_name,
                             param_ptr, out_param);
}


bool CODBC_RPCCmd::Send()
{
    Cancel();

    m_HasFailed = false;
    m_HasStatus = false;

    // make a language command
    string main_exec_query("declare @STpROCrETURNsTATUS int;\nexec @STpROCrETURNsTATUS=");
    main_exec_query+= m_Query;
    string param_result_query;

    CMemPot bindGuard;
    string q_str;

    if(m_Params.NofParams() > 0) {
        SQLLEN* indicator= (SQLLEN*)
                bindGuard.Alloc(m_Params.NofParams() * sizeof(SQLLEN));

        if (!x_AssignParams(q_str, main_exec_query, param_result_query,
                          bindGuard, indicator)) {
            ResetParams();
            m_HasFailed = true;

            string err_message = "cannot assign params" + GetDiagnosticInfo();
            DATABASE_DRIVER_ERROR( err_message, 420003 );
        }
    }

   if(m_Recompile) main_exec_query+= " with recompile";

   q_str+= main_exec_query + ";\nselect STpROCrETURNsTATUS=@STpROCrETURNsTATUS";
   if(!param_result_query.empty()) {
       q_str+= ";\nselect " + param_result_query;
   }

    switch(SQLExecDirect(GetHandle(), (SQLCHAR*)q_str.c_str(), SQL_NTS)) {
    case SQL_SUCCESS:
        m_hasResults= true;
        break;

    case SQL_NO_DATA:
        m_hasResults= true; /* this is a bug in SQLExecDirect it returns SQL_NO_DATA if
                               status result is the only result of RPC */
        m_RowCount= 0;
        break;

    case SQL_ERROR:
        ReportErrors();
        ResetParams();
        m_HasFailed = true;
        {
            string err_message = "SQLExecDirect failed" + GetDiagnosticInfo();
            DATABASE_DRIVER_ERROR( err_message, 420001 );
        }

    case SQL_SUCCESS_WITH_INFO:
        ReportErrors();
        m_hasResults= true;
        break;

    case SQL_STILL_EXECUTING:
        ReportErrors();
        ResetParams();
        m_HasFailed = true;
        {
            string err_message = "Some other query is executing on this connection" +
                GetDiagnosticInfo();
            DATABASE_DRIVER_ERROR( err_message, 420002 );
        }

    case SQL_INVALID_HANDLE:
        m_HasFailed= true;
        {
            string err_message = "The statement handler is invalid (memory corruption suspected)" +
                GetDiagnosticInfo();
            DATABASE_DRIVER_ERROR( err_message, 420004 );
        }

    default:
        ReportErrors();
        ResetParams();
        m_HasFailed = true;
        {
            string err_message = "Unexpected error" + GetDiagnosticInfo();
            DATABASE_DRIVER_ERROR( err_message, 420005 );
        }

    }
    m_WasSent = true;
    return true;
}


bool CODBC_RPCCmd::WasSent() const
{
    return m_WasSent;
}


bool CODBC_RPCCmd::Cancel()
{
    if (m_WasSent) {
        if (m_Res) {
            delete m_Res;
            m_Res = 0;
        }

        m_WasSent = false;

        if ( !Close() ) {
            return false;
        }
    }

    ResetParams();
    m_Query.erase();
    return true;
}


bool CODBC_RPCCmd::WasCanceled() const
{
    return !m_WasSent;
}


CDB_Result* CODBC_RPCCmd::Result()
{
    if (m_Res) {
        delete m_Res;
        m_Res = 0;
        m_hasResults= xCheck4MoreResults();
    }

    if ( !m_WasSent ) {
        string err_message = "a command has to be sent first" + GetDiagnosticInfo();
        DATABASE_DRIVER_ERROR( err_message, 420010 );
    }

    if(!m_hasResults) {
        m_WasSent= false;
        return 0;
    }

    SQLSMALLINT nof_cols= 0;
    char n_buff[64];

    while(m_hasResults) {
        switch(SQLNumResultCols(GetHandle(), &nof_cols)) {
        case SQL_SUCCESS_WITH_INFO:
            ReportErrors();

        case SQL_SUCCESS:
            break;

        case SQL_ERROR:
            ReportErrors();
            {
                string err_message = "SQLNumResultCols failed" + GetDiagnosticInfo();
                DATABASE_DRIVER_ERROR( err_message, 420011 );
            }
        default:
            {
                string err_message = "SQLNumResultCols failed (memory corruption suspected)" +
                    GetDiagnosticInfo();
                DATABASE_DRIVER_ERROR( err_message, 420012 );
            }
        }

        if(nof_cols < 1) { // no data in this result set
            SQLLEN rc;
            switch(SQLRowCount(GetHandle(), &rc)) {
                case SQL_SUCCESS_WITH_INFO:
                    ReportErrors();
                case SQL_SUCCESS: break;
                case SQL_ERROR:
                        ReportErrors();
                        {
                            string err_message = "SQLRowCount failed" + GetDiagnosticInfo();
                            DATABASE_DRIVER_ERROR( err_message, 420013 );
                        }
                default:
                    {
                        string err_message = "SQLRowCount failed (memory corruption suspected)" +
                            GetDiagnosticInfo();
                        DATABASE_DRIVER_ERROR( err_message, 420014 );
                    }
            }

            m_RowCount = rc;
            m_hasResults= xCheck4MoreResults();
            continue;
        }

        if(nof_cols == 1) { // it could be a status result
            SQLSMALLINT l;
            switch(SQLColAttribute(GetHandle(), 1, SQL_DESC_LABEL, n_buff, 64, &l, 0)) {
            case SQL_SUCCESS_WITH_INFO: ReportErrors();
            case SQL_SUCCESS:           break;
            case SQL_ERROR:
                ReportErrors();
                {
                    string err_message = "SQLColAttribute failed" + GetDiagnosticInfo();
                    DATABASE_DRIVER_ERROR( err_message, 420015 );
                }
            default:
                {
                    string err_message = "SQLColAttribute failed (memory corruption suspected)" +
                        GetDiagnosticInfo();
                    DATABASE_DRIVER_ERROR( err_message, 420016 );
                }
            }

            if(strcmp(n_buff, "STpROCrETURNsTATUS") == 0) {//this is a status result
                m_HasStatus= true;
                m_Res= new CODBC_StatusResult(*this);
            }
        }
        if(!m_Res) {
            if(m_HasStatus) {
                m_HasStatus= false;
                m_Res= new CODBC_ParamResult(*this, nof_cols);
            }
            else {
                m_Res = new CODBC_RowResult(*this, nof_cols, &m_RowCount);
            }
        }
        return Create_Result(*m_Res);
    }

    m_WasSent = false;
    return 0;
}


bool CODBC_RPCCmd::HasMoreResults() const
{
    return m_hasResults;
}

void CODBC_RPCCmd::DumpResults()
{
    CDB_Result* dbres;
    while(m_WasSent) {
        dbres= Result();
        if(dbres) {
            if(GetConnection().m_ResProc) {
                GetConnection().m_ResProc->ProcessResult(*dbres);
            }
            else {
                while(dbres->Fetch());
            }
            delete dbres;
        }
    }
}

bool CODBC_RPCCmd::HasFailed() const
{
    return m_HasFailed;
}


int CODBC_RPCCmd::RowCount() const
{
    return m_RowCount;
}


void CODBC_RPCCmd::SetRecompile(bool recompile)
{
    m_Recompile = recompile;
}


void CODBC_RPCCmd::Release()
{
    m_BR = 0;

    Cancel();

    GetConnection().DropCmd(*this);

    delete this;
}


CODBC_RPCCmd::~CODBC_RPCCmd()
{
    try {
        if (m_BR) {
            *m_BR = 0;
        }

        Cancel();
    }
    NCBI_CATCH_ALL( NCBI_CURRENT_FUNCTION )
}


bool CODBC_RPCCmd::x_AssignParams(string& cmd, string& q_exec, string& q_select,
                                   CMemPot& bind_guard, SQLLEN* indicator)
{
    char p_nm[16], tbuf[32];
    // check if we do have a named parameters (first named - all named)
    bool param_named= !m_Params.GetParamName(0).empty();

    for (unsigned int n = 0; n < m_Params.NofParams(); n++) {
        if(m_Params.GetParamStatus(n) == 0) continue;
        const string& name  =  m_Params.GetParamName(n);
        CDB_Object&   param = *m_Params.GetParam(n);
        const char*   type;

        switch (param.GetType()) {
        case eDB_Int: {
            CDB_Int& val = dynamic_cast<CDB_Int&> (param);
            type = "int";
            indicator[n]= 4;
            SQLBindParameter(GetHandle(), n+1, SQL_PARAM_INPUT, SQL_C_SLONG,
                             SQL_INTEGER, 4, 0, val.BindVal(), 4, indicator+n);
            break;
        }
        case eDB_SmallInt: {
            CDB_SmallInt& val = dynamic_cast<CDB_SmallInt&> (param);
            type = "smallint";
            indicator[n]= 2;
            SQLBindParameter(GetHandle(), n+1, SQL_PARAM_INPUT, SQL_C_SSHORT,
                             SQL_SMALLINT, 2, 0, val.BindVal(), 2, indicator+n);
            break;
        }
        case eDB_TinyInt: {
            CDB_TinyInt& val = dynamic_cast<CDB_TinyInt&> (param);
            type = "tinyint";
            indicator[n]= 1;
            SQLBindParameter(GetHandle(), n+1, SQL_PARAM_INPUT, SQL_C_UTINYINT,
                             SQL_TINYINT, 1, 0, val.BindVal(), 1, indicator+n);
            break;
        }
        case eDB_BigInt: {
            CDB_BigInt& val = dynamic_cast<CDB_BigInt&> (param);
            type = "numeric";
            indicator[n]= 8;
            SQLBindParameter(GetHandle(), n+1, SQL_PARAM_INPUT, SQL_C_SBIGINT,
                             SQL_NUMERIC, 18, 0, val.BindVal(), 18, indicator+n);

            break;
        }
        case eDB_Char: {
            CDB_Char& val = dynamic_cast<CDB_Char&> (param);
            type= "varchar(255)";
            indicator[n]= SQL_NTS;
            SQLBindParameter(GetHandle(), n+1, SQL_PARAM_INPUT, SQL_C_CHAR,
                             SQL_VARCHAR, 255, 0, (void*)val.Value(), 256, indicator+n);
            break;
        }
        case eDB_VarChar: {
            CDB_VarChar& val = dynamic_cast<CDB_VarChar&> (param);
            type = "varchar(255)";
            indicator[n]= SQL_NTS;
            SQLBindParameter(GetHandle(), n+1, SQL_PARAM_INPUT, SQL_C_CHAR,
                             SQL_VARCHAR, 255, 0, (void*)val.Value(), 256, indicator+n);
            break;
        }
        case eDB_LongChar: {
            CDB_LongChar& val = dynamic_cast<CDB_LongChar&> (param);
            sprintf(tbuf,"varchar(%d)", val.Size());
            type= tbuf;
            indicator[n]= SQL_NTS;
            SQLBindParameter(GetHandle(), n+1, SQL_PARAM_INPUT, SQL_C_CHAR,
                             SQL_VARCHAR, val.Size(), 0, (void*)val.Value(), val.Size(), indicator+n);
            break;
        }
        case eDB_Binary: {
            CDB_Binary& val = dynamic_cast<CDB_Binary&> (param);
            type = "varbinary(255)";
            indicator[n]= val.Size();
            SQLBindParameter(GetHandle(), n+1, SQL_PARAM_INPUT, SQL_C_BINARY,
                             SQL_VARBINARY, 255, 0, (void*)val.Value(), 255, indicator+n);
            break;
        }
        case eDB_VarBinary: {
            CDB_VarBinary& val = dynamic_cast<CDB_VarBinary&> (param);
            type = "varbinary(255)";
            indicator[n]= val.Size();
            SQLBindParameter(GetHandle(), n+1, SQL_PARAM_INPUT, SQL_C_BINARY,
                             SQL_VARBINARY, 255, 0, (void*)val.Value(), 255, indicator+n);
            break;
        }
        case eDB_LongBinary: {
            CDB_LongBinary& val = dynamic_cast<CDB_LongBinary&> (param);
            sprintf(tbuf,"varbinary(%d)", val.Size());
            type= tbuf;
            indicator[n]= val.DataSize();
            SQLBindParameter(GetHandle(), n+1, SQL_PARAM_INPUT, SQL_C_BINARY,
                             SQL_VARBINARY, val.Size(), 0, (void*)val.Value(), val.Size(), indicator+n);
            break;
        }
        case eDB_Float: {
            CDB_Float& val = dynamic_cast<CDB_Float&> (param);
            type = "real";
            indicator[n]= 4;
            SQLBindParameter(GetHandle(), n+1, SQL_PARAM_INPUT, SQL_C_FLOAT,
                             SQL_REAL, 4, 0, val.BindVal(), 4, indicator+n);
            break;
        }
        case eDB_Double: {
            CDB_Double& val = dynamic_cast<CDB_Double&> (param);
            type = "float";
            indicator[n]= 8;
            SQLBindParameter(GetHandle(), n+1, SQL_PARAM_INPUT, SQL_C_DOUBLE,
                             SQL_FLOAT, 8, 0, val.BindVal(), 8, indicator+n);
            break;
        }
        case eDB_SmallDateTime: {
            CDB_SmallDateTime& val = dynamic_cast<CDB_SmallDateTime&> (param);
            type = "smalldatetime";
            SQL_TIMESTAMP_STRUCT* ts= 0;
            if(!val.IsNULL()) {
                ts= (SQL_TIMESTAMP_STRUCT*)bind_guard.Alloc(sizeof(SQL_TIMESTAMP_STRUCT));
                const CTime& t= val.Value();
                ts->year= t.Year();
                ts->month= t.Month();
                ts->day= t.Day();
                ts->hour= t.Hour();
                ts->minute= t.Minute();
                ts->second= 0;
                ts->fraction= 0;
                indicator[n]= sizeof(SQL_TIMESTAMP_STRUCT);
            }
            SQLBindParameter(GetHandle(), n+1, SQL_PARAM_INPUT, SQL_C_TYPE_TIMESTAMP,
                             SQL_TYPE_TIMESTAMP, 16, 0, (void*)ts, sizeof(SQL_TIMESTAMP_STRUCT),
                             indicator+n);
            break;
        }
        case eDB_DateTime: {
            CDB_DateTime& val = dynamic_cast<CDB_DateTime&> (param);
            type = "datetime";
            SQL_TIMESTAMP_STRUCT* ts= 0;
            if(!val.IsNULL()) {
                ts= (SQL_TIMESTAMP_STRUCT*)bind_guard.Alloc(sizeof(SQL_TIMESTAMP_STRUCT));
                const CTime& t= val.Value();
                ts->year= t.Year();
                ts->month= t.Month();
                ts->day= t.Day();
                ts->hour= t.Hour();
                ts->minute= t.Minute();
                ts->second= t.Second();
                ts->fraction= t.NanoSecond()/1000000;
                ts->fraction*= 1000000; /* MSSQL has a bug - it cannot handle fraction of msecs */
                indicator[n]= sizeof(SQL_TIMESTAMP_STRUCT);
            }
            SQLBindParameter(GetHandle(), n+1, SQL_PARAM_INPUT, SQL_C_TYPE_TIMESTAMP,
                             SQL_TYPE_TIMESTAMP, 23, 3, ts, sizeof(SQL_TIMESTAMP_STRUCT),
                             indicator+n);
            break;
        }
        default:
            return false;
        }

        q_exec+= n? ',':' ';

        if(!param_named) {
            sprintf(p_nm, "@pR%d", n);
            q_exec+= p_nm;
            cmd+= "declare ";
            cmd+= p_nm;
            cmd+= ' ';
            cmd+= type;
            cmd+= ";select ";
            cmd+= p_nm;
            cmd+= " = ?;";
        }
        else {
            q_exec+= name+'='+name;
            cmd += "declare " + name + ' ' + type + ";select " + name + " = ?;";
        }

        if(param.IsNULL()) {
            indicator[n]= SQL_NULL_DATA;
        }

        if ((m_Params.GetParamStatus(n) & CDB_Params::fOutput) != 0) {
            q_exec+= " output";
            const char* p_name= param_named? name.c_str() : p_nm;
            if(!q_select.empty()) q_select+= ',';
            q_select.append(p_name+1);
            q_select+= '=';
            q_select+= p_name;
        }

    }
    return true;
}

bool CODBC_RPCCmd::xCheck4MoreResults()
{
    switch(SQLMoreResults(GetHandle())) {
    case SQL_SUCCESS_WITH_INFO: ReportErrors();
    case SQL_SUCCESS:           return true;
    case SQL_NO_DATA:           return false;
    case SQL_ERROR:
        ReportErrors();
        {
            string err_message = "SQLMoreResults failed" + GetDiagnosticInfo();
            DATABASE_DRIVER_ERROR( err_message, 420014 );
        }
    default:
        {
            string err_message = "SQLMoreResults failed (memory corruption suspected)" +
                GetDiagnosticInfo();
            DATABASE_DRIVER_ERROR( err_message, 420015 );
        }
    }
}

END_NCBI_SCOPE



/*
 * ===========================================================================
 * $Log$
 * Revision 1.21  2006/06/05 18:10:07  ssikorsk
 * Revamp code to use methods Cancel and Close more efficient.
 *
 * Revision 1.20  2006/06/02 19:37:40  ssikorsk
 * + NCBI_CATCH_ALL( NCBI_CURRENT_FUNCTION )
 *
 * Revision 1.19  2006/02/28 14:27:30  ssikorsk
 * Replaced int/SQLINTEGER variables with SQLLEN where needed.
 *
 * Revision 1.18  2006/02/28 14:00:47  ssikorsk
 * Fixed argument type misuse (like SQLINTEGER and SQLLEN) for vc8-x64 sake.
 *
 * Revision 1.17  2006/02/22 15:15:51  ssikorsk
 * *** empty log message ***
 *
 * Revision 1.16  2005/11/28 13:22:59  ssikorsk
 * Report SQL statement and database connection parameters in case
 * of an error in addition to a server error message.
 *
 * Revision 1.15  2005/11/02 16:46:21  ssikorsk
 * Pass context information with an error message of a database exception.
 *
 * Revision 1.14  2005/11/02 12:58:38  ssikorsk
 * Report extra information in exceptions and error messages.
 *
 * Revision 1.13  2005/09/19 14:19:05  ssikorsk
 * Use NCBI_CATCH_ALL macro instead of catch(...)
 *
 * Revision 1.12  2005/09/15 11:00:02  ssikorsk
 * Destructors do not throw exceptions any more.
 *
 * Revision 1.11  2005/04/04 13:03:57  ssikorsk
 * Revamp of DBAPI exception class CDB_Exception
 *
 * Revision 1.10  2005/02/15 16:07:51  ssikorsk
 * Fixed a bug with GetRowCount plus SELECT statement
 *
 * Revision 1.9  2004/05/17 21:16:06  gorelenk
 * Added include of PCH ncbi_pch.hpp
 *
 * Revision 1.8  2004/01/27 18:00:07  soussov
 * patches the SQLExecDirect bug
 *
 * Revision 1.7  2003/11/07 17:14:20  soussov
 * work around the odbc bug. It can not handle properly the fractions of msecs
 *
 * Revision 1.6  2003/11/06 20:33:32  soussov
 * fixed bug in DateTime bindings
 *
 * Revision 1.5  2003/06/05 16:02:04  soussov
 * adds code for DumpResults and for the dumped results processing
 *
 * Revision 1.4  2003/05/16 20:26:44  soussov
 * adds code to skip parameter if it was not set
 *
 * Revision 1.3  2003/05/08 20:30:24  soussov
 * CDB_LongChar CDB_LongBinary added
 *
 * Revision 1.2  2003/05/05 20:48:47  ucko
 * +<stdio.h> for sprintf
 *
 * Revision 1.1  2002/06/18 22:06:25  soussov
 * initial commit
 *
 *
 * ===========================================================================
 */
