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
 
#include <dbapi/driver/ftds/interfaces.hpp>
#include <dbapi/driver/util/numeric_convert.hpp>


BEGIN_NCBI_SCOPE


/////////////////////////////////////////////////////////////////////////////
//
//  CTDS_RPCCmd::
//

CTDS_RPCCmd::CTDS_RPCCmd(CTDS_Connection* con, DBPROCESS* cmd,
                         const string& proc_name, unsigned int nof_params) :
    m_Connect(con), m_Cmd(cmd), m_Query(proc_name), m_Params(nof_params),
    m_WasSent(false), m_HasFailed(false), m_Recompile(false), m_Res(0),
    m_RowCount(-1), m_Status(0)
{
    return;
}


bool CTDS_RPCCmd::BindParam(const string& param_name,
                            CDB_Object* param_ptr, bool out_param)
{
    return m_Params.BindParam(CDB_Params::kNoParamNumber, param_name,
                              param_ptr, out_param);
}


bool CTDS_RPCCmd::SetParam(const string& param_name,
                           CDB_Object* param_ptr, bool out_param)
{
    return m_Params.SetParam(CDB_Params::kNoParamNumber, param_name,
                             param_ptr, out_param);
}


bool CTDS_RPCCmd::Send()
{
    if (m_WasSent)
        Cancel();

    m_HasFailed = false;

    if (!x_AssignOutputParams()) {
        dbfreebuf(m_Cmd);
        m_HasFailed = true;
        throw CDB_ClientEx(eDB_Error, 221001, "CTDS_RPCCmd::Send",
                           "cannot assign the output params");
    }

    string cmd= "execute " + m_Query;
    if (dbcmd(m_Cmd, (char*)(cmd.c_str())) != SUCCEED) {
        dbfreebuf(m_Cmd);
        m_HasFailed = true;
        throw CDB_ClientEx(eDB_Fatal, 221002, "CTDS_RPCCmd::Send",
                           "dbcmd failed");
    }

    if (!x_AssignParams()) {
        m_HasFailed = true;
        throw CDB_ClientEx(eDB_Error, 221003, "CTDS_RPCCmd::Send",
                           "can not assign the params");
    }

    m_Connect->TDS_SetTimeout();

    if (dbsqlsend(m_Cmd) != SUCCEED) {
        m_HasFailed = true;
        throw CDB_ClientEx(eDB_Error, 221005, "CTDS_RPCCmd::Send",
                           "dbsqlsend failed");
    }

    m_WasSent = true;
    m_Status = 0;
    return true;
}


bool CTDS_RPCCmd::WasSent() const
{
    return m_WasSent;
}


bool CTDS_RPCCmd::Cancel()
{
    if (m_WasSent) {
        if (m_Res) {
            delete m_Res;
            m_Res = 0;
        }
        m_WasSent = false;
        return dbcancel(m_Cmd) == SUCCEED;
    }
    // m_Query.erase();
    return true;
}


bool CTDS_RPCCmd::WasCanceled() const
{
    return !m_WasSent;
}


CDB_Result* CTDS_RPCCmd::Result()
{
    if (m_Res) {
        if(m_RowCount < 0) {
            m_RowCount = DBCOUNT(m_Cmd);
        }
        delete m_Res;
        m_Res = 0;
    }

    if (!m_WasSent) {
        throw CDB_ClientEx(eDB_Error, 221010, "CTDS_RPCCmd::Result",
                           "you have to send a command first");
    }

    if (m_Status == 0) {
        m_Status = 1;
        if (dbsqlok(m_Cmd) != SUCCEED) {
            m_WasSent = false;
            m_HasFailed = true;
            throw CDB_ClientEx(eDB_Error, 221011, "CTDS_RPCCmd::Result",
                               "dbsqlok failed");
        }
    }

    if ((m_Status & 0x10) != 0) { // we do have a compute result
        m_Res = new CTDS_ComputeResult(m_Cmd, &m_Status);
        m_RowCount = 1;
        return Create_Result(*m_Res);
    }

    while ((m_Status & 0x1) != 0) {
        switch (dbresults(m_Cmd)) {
        case SUCCEED:
            if (DBCMDROW(m_Cmd) == SUCCEED) { // we may get rows in this result
                m_Res = new CTDS_RowResult(m_Cmd, &m_Status);
                m_RowCount = -1;
                return Create_Result(*m_Res);
            } else {
                m_RowCount = DBCOUNT(m_Cmd);
                continue;
            }
        case NO_MORE_RESULTS:
            m_Status = 2;
            break;
        default:
            m_HasFailed = true;
            throw CDB_ClientEx(eDB_Warning, 221016, "CTDS_RPCCmd::Result",
                               "error encountered in command execution");
        }
        break;
    }

    // we've done with the row results at this point
    // let's look at return parameters and ret status
    if (m_Status == 2) {
        m_Status = 4;
        int n = dbnumrets(m_Cmd);
        if (n > 0) {
            m_Res = new CTDS_ParamResult(m_Cmd, n);
            m_RowCount = 1;
            return Create_Result(*m_Res);
        }
    }

    if (m_Status == 4) {
        m_Status = 6;
        if (dbhasretstat(m_Cmd)) {
            m_Res = new CTDS_StatusResult(m_Cmd);
            m_RowCount = 1;
            return Create_Result(*m_Res);
        }
    }

    m_WasSent = false;
    return 0;
}


bool CTDS_RPCCmd::HasMoreResults() const
{
    return m_WasSent;
}

void CTDS_RPCCmd::DumpResults()
{
    CDB_Result* dbres;
    while(m_WasSent) {
        dbres= Result();
        if(dbres) {
            if(m_Connect->m_ResProc) {
                m_Connect->m_ResProc->ProcessResult(*dbres);
            }
            else {
                while(dbres->Fetch());
            }
            delete dbres;
        }
    }
}

bool CTDS_RPCCmd::HasFailed() const
{
    return m_HasFailed;
}


int CTDS_RPCCmd::RowCount() const
{
    return (m_RowCount < 0)? DBCOUNT(m_Cmd) : m_RowCount;
}


void CTDS_RPCCmd::SetRecompile(bool recompile)
{
    m_Recompile = recompile;
}


void CTDS_RPCCmd::Release()
{
    m_BR = 0;
    if (m_WasSent) {
        Cancel();
        m_WasSent = false;
    }
    m_Connect->DropCmd(*this);
    delete this;
}


CTDS_RPCCmd::~CTDS_RPCCmd()
{
    if (m_BR)
        *m_BR = 0;
    if (m_WasSent)
        Cancel();
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
    for (unsigned int n = 0; n < m_Params.NofParams(); n++) {
        if ((m_Params.GetParamStatus(n) & CDB_Params::fOutput) == 0)
            continue;

        const string& name  =  m_Params.GetParamName(n);
        CDB_Object&   param = *m_Params.GetParam(n);
        const char*   type;
        string        cmd;

        if (name.length() > kTDSMaxNameLen)
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

        if (dbcmd(m_Cmd, (char*) cmd.c_str()) != SUCCEED)
            return false;
    }
    return true;
}

bool CTDS_RPCCmd::x_AssignParams()
{
    for (unsigned int i = 0;  i < m_Params.NofParams();  i++) {
      if(!m_Params.GetParamStatus(i)) continue;
        CDB_Object& param = *m_Params.GetParam(i);
        bool is_output =
            (m_Params.GetParamStatus(i) & CDB_Params::fOutput) != 0;
        const string& name  = m_Params.GetParamName(i);
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
        if (dbcmd(m_Cmd, (char*) cmd.c_str()) != SUCCEED)
            return false;
    }
    return true;
}


END_NCBI_SCOPE



/*
 * ===========================================================================
 * $Log$
 * Revision 1.11  2003/08/13 18:04:05  soussov
 * fixes copy/paste bug for DateTime
 *
 * Revision 1.10  2003/06/05 16:01:40  soussov
 * adds code for DumpResults and for the dumped results processing
 *
 * Revision 1.9  2003/04/29 21:15:03  soussov
 * new datatypes CDB_LongChar and CDB_LongBinary added
 *
 * Revision 1.8  2002/11/20 20:36:09  soussov
 * fixed bug in parameters assignment
 *
 * Revision 1.7  2002/07/22 20:11:07  soussov
 * fixes the RowCount calculations
 *
 * Revision 1.6  2002/02/22 22:12:45  soussov
 * fixes bug with return params result
 *
 * Revision 1.5  2002/01/14 20:38:49  soussov
 * timeout support for tds added
 *
 * Revision 1.4  2001/12/18 19:29:08  soussov
 * adds conversion from nanosecs to milisecs for datetime args
 *
 * Revision 1.3  2001/12/18 16:42:44  soussov
 * fixes bug in datetime argument processing
 *
 * Revision 1.2  2001/12/13 23:40:53  soussov
 * changes double quotes to single quotes in SQL queries
 *
 * Revision 1.1  2001/10/25 00:39:22  vakatov
 * Initial revision
 *
 * Revision 1.2  2001/10/22 16:28:02  lavr
 * Default argument values removed
 * (mistakenly left while moving code from header files)
 *
 * Revision 1.1  2001/10/22 15:19:56  lavr
 * This is a major revamp (by Anton Lavrentiev, with help from Vladimir
 * Soussov and Denis Vakatov) of the DBAPI "driver" libs originally
 * written by Vladimir Soussov. The revamp follows the one of CTLib
 * driver, and it involved massive code shuffling and grooming, numerous
 * local API redesigns, adding comments and incorporating DBAPI to
 * the C++ Toolkit.
 *
 * ===========================================================================
 */
