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
 * File Description:  TDS RPC command
 *
 */

#include <ncbi_pch.hpp>
#include <dbapi/driver/ftds/interfaces.hpp>
#include <dbapi/driver/util/numeric_convert.hpp>


BEGIN_NCBI_SCOPE


/////////////////////////////////////////////////////////////////////////////
//
//  CTDS_RPCCmd::
//

CTDS_RPCCmd::CTDS_RPCCmd(CTDS_Connection* conn,
                         DBPROCESS* cmd,
                         const string& proc_name,
                         unsigned int nof_params) :
    CDBL_Cmd(conn, cmd),
    impl::CBaseCmd(conn, proc_name, nof_params),
    m_Res(0),
    m_Status(0)
{
    SetExecCntxInfo("RPC Command: " + proc_name);
}


bool CTDS_RPCCmd::Send()
{
    Cancel();

    SetHasFailed(false);

    if (!x_AssignOutputParams()) {
        dbfreebuf(GetCmd());
        CheckFunctCall();
        SetHasFailed();
        DATABASE_DRIVER_ERROR( "cannot assign the output params", 221001 );
    }

    string cmd= "execute " + GetQuery();
    if (Check(dbcmd(GetCmd(), (char*)(cmd.c_str()))) != SUCCEED) {
        dbfreebuf(GetCmd());
        CheckFunctCall();
        SetHasFailed();
        DATABASE_DRIVER_ERROR( "dbcmd failed", 221002 );
    }

    if (!x_AssignParams()) {
        SetHasFailed();
        DATABASE_DRIVER_ERROR( "Cannot assign the params", 221003 );
    }

    GetConnection().TDS_SetTimeout();

    if (Check(dbsqlsend(GetCmd())) != SUCCEED) {
        SetHasFailed();
        DATABASE_DRIVER_ERROR( "dbsqlsend failed", 221005 );
    }

    SetWasSent();
    m_Status = 0;
    return true;
}


bool CTDS_RPCCmd::Cancel()
{
    if (WasSent()) {
        if (m_Res) {
            delete m_Res;
            m_Res = 0;
        }
        SetWasSent(false);
        return Check(dbcancel(GetCmd())) == SUCCEED;
    }
    // GetQuery().erase();
    return true;
}


CDB_Result* CTDS_RPCCmd::Result()
{
    if (m_Res) {
        if(m_RowCount < 0) {
            m_RowCount = DBCOUNT(GetCmd());
        }
        delete m_Res;
        m_Res = 0;
    }

    if (!WasSent()) {
        DATABASE_DRIVER_ERROR( "you have to send a command first", 221010 );
    }

    if (m_Status == 0) {
        m_Status = 1;
        if (Check(dbsqlok(GetCmd())) != SUCCEED) {
            SetWasSent(false);
            SetHasFailed();
            DATABASE_DRIVER_ERROR( "dbsqlok failed", 221011 );
        }
    }

    if ((m_Status & 0x10) != 0) { // we do have a compute result
        m_Res = new CTDS_ComputeResult(GetConnection(), GetCmd(), &m_Status);
        m_RowCount = 1;
        return Create_Result(*m_Res);
    }

    while ((m_Status & 0x1) != 0) {
        RETCODE rc = Check(dbresults(GetCmd()));

        switch (rc) {
        case SUCCEED:
            if (DBCMDROW(GetCmd()) == SUCCEED) { // we may get rows in this result
                m_Res = new CTDS_RowResult(GetConnection(), GetCmd(), &m_Status);
                m_RowCount = -1;
                return Create_Result(*m_Res);
            } else {
                m_RowCount = DBCOUNT(GetCmd());
                continue;
            }
        case NO_MORE_RESULTS:
            m_Status = 2;
            break;
        default:
            SetHasFailed();
            DATABASE_DRIVER_WARNING( "error encountered in command execution", 221016 );
        }
        break;
    }

    // we've done with the row results at this point
    // let's look at return parameters and ret status
    if (m_Status == 2) {
        m_Status = 4;
        int n = Check(dbnumrets(GetCmd()));
        if (n > 0) {
            m_Res = new CTDS_ParamResult(GetConnection(), GetCmd(), n);
            m_RowCount = 1;
            return Create_Result(*m_Res);
        }
    }

    if (m_Status == 4) {
        m_Status = 6;
        if (Check(dbhasretstat(GetCmd()))) {
            m_Res = new CTDS_StatusResult(GetConnection(), GetCmd());
            m_RowCount = 1;
            return Create_Result(*m_Res);
        }
    }

    SetWasSent(false);
    return 0;
}


bool CTDS_RPCCmd::HasMoreResults() const
{
    return WasSent();
}


int CTDS_RPCCmd::RowCount() const
{
    return (m_RowCount < 0)? DBCOUNT(GetCmd()) : m_RowCount;
}


CTDS_RPCCmd::~CTDS_RPCCmd()
{
    try {
        DetachInterface();

        GetConnection().DropCmd(*this);

        Cancel();
    }
    NCBI_CATCH_ALL( NCBI_CURRENT_FUNCTION )
}


bool CTDS_RPCCmd::x_AddParamValue(string& cmd, const CDB_Object& param)
{
    static const char s_hexnum[] = "0123456789ABCDEF";
    char val_buffer[16*1024];

    if (!param.IsNULL()) {
        switch (param.GetType()) {
        case eDB_Int: {
            const CDB_Int& val = dynamic_cast<const CDB_Int&> (param);
            sprintf(val_buffer, "%d\n", val.Value());
            break;
        }
        case eDB_SmallInt: {
            const CDB_SmallInt& val = dynamic_cast<const CDB_SmallInt&>(param);
            sprintf(val_buffer, "%d\n", (int) val.Value());
            break;
        }
        case eDB_TinyInt: {
            const CDB_TinyInt& val = dynamic_cast<const CDB_TinyInt&> (param);
            sprintf(val_buffer, "%d\n", (int) val.Value());
            break;
        }
        case eDB_BigInt: {
            const CDB_BigInt& val = dynamic_cast<const CDB_BigInt&> (param);
            sprintf(val_buffer, "%lld\n", val.Value());
            break;
        }
        case eDB_Char: {
            const CDB_Char& val = dynamic_cast<const CDB_Char&> (param);
            const char* c = val.Value(); // No more than 255 chars
            size_t i = 0;
            val_buffer[i++] = '\'';
            while (*c) {
                if (*c == '\'')
                    val_buffer[i++] = '\'';
                val_buffer[i++] = *c++;
            }
            val_buffer[i++] = '\'';
            val_buffer[i++] = '\n';
            val_buffer[i] = '\0';
            break;
        }
        case eDB_VarChar: {
            const CDB_VarChar& val = dynamic_cast<const CDB_VarChar&> (param);
            const char* c = val.Value();
            size_t i = 0;
            val_buffer[i++] = '\'';
            while (*c) {
                if (*c == '\'')
                    val_buffer[i++] = '\'';
                val_buffer[i++] = *c++;
            }
            val_buffer[i++] = '\'';
            val_buffer[i++] = '\n';
            val_buffer[i] = '\0';
            break;
        }
        case eDB_LongChar: {
            const CDB_LongChar& val = dynamic_cast<const CDB_LongChar&> (param);
            const char* c = val.Value();
            size_t i = 0;
            val_buffer[i++] = '\'';
            while (*c && (i < sizeof(val_buffer) - 4)) {
                if (*c == '\'')
                    val_buffer[i++] = '\'';
                val_buffer[i++] = *c++;
            }
            if(*c != '\0') return false;
            val_buffer[i++] = '\'';
            val_buffer[i++] = '\n';
            val_buffer[i] = '\0';
            break;
        }
        case eDB_Binary: {
            const CDB_Binary& val = dynamic_cast<const CDB_Binary&> (param);
            const unsigned char* c = (const unsigned char*) val.Value();
            size_t i = 0, size = val.Size();
            val_buffer[i++] = '0';
            val_buffer[i++] = 'x';
            for (size_t j = 0; j < size; j++) {
                val_buffer[i++] = s_hexnum[c[j] >> 4];
                val_buffer[i++] = s_hexnum[c[j] & 0x0F];
            }
            val_buffer[i++] = '\n';
            val_buffer[i++] = '\0';
            break;
        }
        case eDB_VarBinary: {
            const CDB_VarBinary& val =
                dynamic_cast<const CDB_VarBinary&> (param);
            const unsigned char* c = (const unsigned char*) val.Value();
            size_t i = 0, size = val.Size();
            val_buffer[i++] = '0';
            val_buffer[i++] = 'x';
            for (size_t j = 0; j < size; j++) {
                val_buffer[i++] = s_hexnum[c[j] >> 4];
                val_buffer[i++] = s_hexnum[c[j] & 0x0F];
            }
            val_buffer[i++] = '\n';
            val_buffer[i++] = '\0';
            break;
        }
        case eDB_LongBinary: {
            const CDB_LongBinary& val =
                dynamic_cast<const CDB_LongBinary&> (param);
            const unsigned char* c = (const unsigned char*) val.Value();
            size_t i = 0, size = val.DataSize();
            if(size*2 > sizeof(val_buffer) - 4) return false;
            val_buffer[i++] = '0';
            val_buffer[i++] = 'x';
            for (size_t j = 0; j < size; j++) {
                val_buffer[i++] = s_hexnum[c[j] >> 4];
                val_buffer[i++] = s_hexnum[c[j] & 0x0F];
            }
            val_buffer[i++] = '\n';
            val_buffer[i++] = '\0';
            break;
        }
        case eDB_Float: {
            const CDB_Float& val = dynamic_cast<const CDB_Float&> (param);
            sprintf(val_buffer, "%E\n", (double) val.Value());
            break;
        }
        case eDB_Double: {
            const CDB_Double& val = dynamic_cast<const CDB_Double&> (param);
            sprintf(val_buffer, "%E\n", val.Value());
            break;
        }
        case eDB_SmallDateTime: {
            const CDB_SmallDateTime& val =
                dynamic_cast<const CDB_SmallDateTime&> (param);
            sprintf(val_buffer, "'%s'\n",
            val.Value().AsString("M/D/Y h:m").c_str());
            break;
        }
        case eDB_DateTime: {
            const CDB_DateTime& val =
                dynamic_cast<const CDB_DateTime&> (param);
            sprintf(val_buffer, "'%s:%.3d'\n",
            val.Value().AsString("M/D/Y h:m:s").c_str(),
            (int)(val.Value().NanoSecond()/1000000));
            break;
        }
        default:
            return false; // dummy for compiler
        }
        cmd += val_buffer;
    } else {
        cmd += "NULL";
    }

    return true;
}


bool CTDS_RPCCmd::x_AssignOutputParams()
{
    char buffer[64];
    for (unsigned int n = 0; n < GetParams().NofParams(); n++) {
        if ((GetParams().GetParamStatus(n) & CDB_Params::fOutput) == 0)
            continue;

        const string& name  =  GetParams().GetParamName(n);
        CDB_Object&   param = *GetParams().GetParam(n);
        const char*   type;
        string        cmd;

        if (name.length() > kDBLibMaxNameLen)
            return false;

        switch (param.GetType()) {
        case eDB_Int:
            type = "int";
            break;
        case eDB_SmallInt:
            type = "smallint";
            break;
        case eDB_TinyInt:
            type = "tinyint";
            break;
        case eDB_BigInt:
            type = "numeric";
            break;
        case eDB_Char:
        case eDB_VarChar:
            type = "varchar(255)";
            break;
        case eDB_LongChar: {
            CDB_LongChar& lc = dynamic_cast<CDB_LongChar&> (param);
            sprintf(buffer, "varchar(%d)", lc.Size());
            type= buffer;
            break;
        }
        case eDB_Binary:
        case eDB_VarBinary:
            type = "varbinary(255)";
            break;
        case eDB_LongBinary: {
            CDB_LongBinary& lb = dynamic_cast<CDB_LongBinary&> (param);
            sprintf(buffer, "varbinary(%d)", lb.Size());
            type= buffer;
            break;
        }
        case eDB_Float:
            type = "real";
            break;
        case eDB_Double:
            type = "float";
            break;
        case eDB_SmallDateTime:
            type = "smalldatetime";
            break;
        case eDB_DateTime:
            type = "datetime";
            break;
        default:
            return false;
        }

        cmd += "declare " + name + ' ' + type + '\n';
        if (!param.IsNULL()) {
            cmd += "select " + name + " = ";
            x_AddParamValue(cmd, param);
            cmd+= '\n';
        }

        if (Check(dbcmd(GetCmd(), (char*) cmd.c_str())) != SUCCEED)
            return false;
    }
    return true;
}

bool CTDS_RPCCmd::x_AssignParams()
{
    for (unsigned int i = 0;  i < GetParams().NofParams();  i++) {
      if(!GetParams().GetParamStatus(i)) continue;
        CDB_Object& param = *GetParams().GetParam(i);
        bool is_output =
            (GetParams().GetParamStatus(i) & CDB_Params::fOutput) != 0;
        const string& name  = GetParams().GetParamName(i);
        string cmd= i? ", " : " ";

        if(!name.empty()) {
            cmd+= name + '=';
            if(is_output) {
                cmd+= name + " out";
            }
            else {
                x_AddParamValue(cmd, param);
            }
        }
        else {
            x_AddParamValue(cmd, param);
        }
        if (Check(dbcmd(GetCmd(), (char*) cmd.c_str())) != SUCCEED)
            return false;
    }
    return true;
}


END_NCBI_SCOPE



