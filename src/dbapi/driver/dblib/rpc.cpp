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
 * File Description:  DBLib RPC command
 *
 */

#include <dbapi/driver/dblib/interfaces.hpp>
#include <dbapi/driver/util/numeric_convert.hpp>


BEGIN_NCBI_SCOPE


/////////////////////////////////////////////////////////////////////////////
//
//  CDBL_RPCCmd::
//

CDBL_RPCCmd::CDBL_RPCCmd(CDBL_Connection* con, DBPROCESS* cmd,
                         const string& proc_name, unsigned int nof_params) :
    m_Connect(con), m_Cmd(cmd), m_Query(proc_name), m_Params(nof_params),
    m_WasSent(false), m_HasFailed(false), m_Recompile(false), m_Res(0),
    m_RowCount(-1), m_Status(0)
{
}


bool CDBL_RPCCmd::BindParam(const string& param_name,
                            CDB_Object* param_ptr, bool out_param)
{
    return m_Params.BindParam(CDB_Params::kNoParamNumber, param_name,
                              param_ptr, out_param);
}


bool CDBL_RPCCmd::SetParam(const string& param_name,
                           CDB_Object* param_ptr, bool out_param)
{
    return m_Params.SetParam(CDB_Params::kNoParamNumber, param_name,
                             param_ptr, out_param);
}


bool CDBL_RPCCmd::Send()
{
    if (m_WasSent)
        Cancel();

    m_HasFailed = false;

    if (dbrpcinit(m_Cmd, (char*) m_Query.c_str(),
                  m_Recompile ? DBRPCRECOMPILE : 0) != SUCCEED) {
        m_HasFailed = true;
        throw CDB_ClientEx(eDB_Error, 221001, "CDBL_RPCCmd::Send",
                           "dbrpcinit failed");
    }

    char param_buff[2048]; // maximal page size
    if (!x_AssignParams(param_buff)) {
        m_HasFailed = true;
        throw CDB_ClientEx(eDB_Error, 221003, "CDBL_RPCCmd::Send",
                           "can not assign the params");
    }
    if (dbrpcsend(m_Cmd) != SUCCEED) {
        m_HasFailed = true;
        throw CDB_ClientEx(eDB_Error, 221005, "CDBL_RPCCmd::Send",
                           "dbrpcsend failed");
    }

    m_WasSent = true;
    m_Status = 0;
    return true;
}


bool CDBL_RPCCmd::WasSent() const
{
    return m_WasSent;
}


bool CDBL_RPCCmd::Cancel()
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


bool CDBL_RPCCmd::WasCanceled() const
{
    return !m_WasSent;
}


CDB_Result* CDBL_RPCCmd::Result()
{
    if (m_Res) {
        delete m_Res;
        m_Res = 0;
    }

    if (!m_WasSent) {
        throw CDB_ClientEx(eDB_Error, 221010, "CDBL_RPCCmd::Result",
                           "you have to send a command first");
    }

    if (m_Status == 0) {
        m_Status = 1;
        if (dbsqlok(m_Cmd) != SUCCEED) {
            m_WasSent = false;
            m_HasFailed = true;
            throw CDB_ClientEx(eDB_Error, 221011, "CDBL_RPCCmd::Result",
                               "dbsqlok failed");
        }
    }

    if ((m_Status & 0x10) != 0) { // we do have a compute result
        m_Res = new CDBL_ComputeResult(m_Cmd, &m_Status);
        return Create_Result(*m_Res);
    }

    while ((m_Status & 0x1) != 0) {
        switch (dbresults(m_Cmd)) {
        case SUCCEED:
            if (DBCMDROW(m_Cmd) == SUCCEED) { // we may get rows in this result
                m_Res = new CDBL_RowResult(m_Cmd, &m_Status);
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
            throw CDB_ClientEx(eDB_Warning, 221016, "CDBL_RPCCmd::Result",
                               "error encountered in command execution");
        }
        break;
    }

    // we've done with the row results at this point
    // let's look at return parameters and ret status
    if (m_Status == 2) {
        m_Status = 3;
        int n = dbnumrets(m_Cmd);
        if (n > 0) {
            m_Res = new CDBL_ParamResult(m_Cmd, n);
            return Create_Result(*m_Res);
        }
    }

    if (m_Status == 3) {
        m_Status = 4;
        if (dbhasretstat(m_Cmd)) {
            m_Res = new CDBL_StatusResult(m_Cmd);
            return Create_Result(*m_Res);
        }
    }

    m_WasSent = false;
    return 0;
}


bool CDBL_RPCCmd::HasMoreResults() const
{
    return m_WasSent;
}


bool CDBL_RPCCmd::HasFailed() const
{
    return m_HasFailed;
}


int CDBL_RPCCmd::RowCount() const
{
    return m_RowCount;
}


void CDBL_RPCCmd::SetRecompile(bool recompile)
{
    m_Recompile = recompile;
}


void CDBL_RPCCmd::Release()
{
    m_BR = 0;
    if (m_WasSent) {
        Cancel();
        m_WasSent = false;
    }
    m_Connect->DropCmd(*this);
    delete this;
}


CDBL_RPCCmd::~CDBL_RPCCmd()
{
    if (m_BR)
        *m_BR = 0;
    if (m_WasSent)
        Cancel();
}


bool CDBL_RPCCmd::x_AssignParams(char* param_buff)
{
    RETCODE r;

    for (unsigned int i = 0; i < m_Params.NofParams(); i++) {
        CDB_Object& param = *m_Params.GetParam(i);
        BYTE status =
            (m_Params.GetParamStatus(i) & CDB_Params::fOutput)
            ? DBRPCRETURN : 0;
        bool is_null = param.IsNULL();

        switch (param.GetType()) {
        case eDB_Int: {
            CDB_Int& val = dynamic_cast<CDB_Int&> (param);
            r = dbrpcparam(m_Cmd, (char*) m_Params.GetParamName(i).c_str(),
                           status, SYBINT4, -1,
                           is_null ? 0 : -1, (BYTE*) val.BindVal());
            break;
        }
        case eDB_SmallInt: {
            CDB_SmallInt& val = dynamic_cast<CDB_SmallInt&> (param);
            r = dbrpcparam(m_Cmd, (char*) m_Params.GetParamName(i).c_str(),
                           status, SYBINT2, -1,
                           is_null ? 0 : -1, (BYTE*) val.BindVal());
            break;
        }
        case eDB_TinyInt: {
            CDB_TinyInt& val = dynamic_cast<CDB_TinyInt&> (param);
            r = dbrpcparam(m_Cmd, (char*) m_Params.GetParamName(i).c_str(),
                           status, SYBINT1, -1,
                           is_null ? 0 : -1, (BYTE*) val.BindVal());
            break;
        }
        case eDB_BigInt: {
            CDB_BigInt& val = dynamic_cast<CDB_BigInt&> (param);
            DBNUMERIC* v = (DBNUMERIC*) param_buff;
            Int8 v8 = val.Value();
            if (longlong_to_numeric(v8, 18, DBNUMERIC_val(v)) == 0)
                return false;
            r = dbrpcparam(m_Cmd, (char*) m_Params.GetParamName(i).c_str(),
                           status, SYBNUMERIC, -1,
                           is_null ? 0 : -1, (BYTE*) v);
            param_buff = (char*) (v + 1);
            break;
        }
        case eDB_Char: {
            CDB_Char& val = dynamic_cast<CDB_Char&> (param);
            r = dbrpcparam(m_Cmd, (char*) m_Params.GetParamName(i).c_str(),
                           status, SYBCHAR, -1,
                           is_null ? 0 : (DBINT) val.Size(),
                           (BYTE*) val.Value());
            break;
        }
        case eDB_VarChar: {
            CDB_VarChar& val = dynamic_cast<CDB_VarChar&> (param);
            r = dbrpcparam(m_Cmd, (char*) m_Params.GetParamName(i).c_str(),
                           status, SYBCHAR, -1,
                           is_null ? 0 : (DBINT) val.Size(),
                           (BYTE*) val.Value());
        }
        break;
        case eDB_Binary: {
            CDB_Binary& val = dynamic_cast<CDB_Binary&> (param);
            r = dbrpcparam(m_Cmd, (char*) m_Params.GetParamName(i).c_str(),
                           status, SYBBINARY, -1,
                           is_null ? 0 : (DBINT) val.Size(),
                           (BYTE*) val.Value());
            break;
        }
        case eDB_VarBinary: {
            CDB_VarBinary& val = dynamic_cast<CDB_VarBinary&> (param);
            r = dbrpcparam(m_Cmd, (char*) m_Params.GetParamName(i).c_str(),
                           status, SYBBINARY, -1,
                           is_null ? 0 : (DBINT) val.Size(),
                           (BYTE*) val.Value());
        }
        break;
        case eDB_Float: {
            CDB_Float& val = dynamic_cast<CDB_Float&> (param);
            r = dbrpcparam(m_Cmd, (char*) m_Params.GetParamName(i).c_str(),
                           status, SYBREAL, -1,
                           is_null ? 0 : -1, (BYTE*) val.BindVal());
            break;
        }
        case eDB_Double: {
            CDB_Double& val = dynamic_cast<CDB_Double&> (param);
            r = dbrpcparam(m_Cmd, (char*) m_Params.GetParamName(i).c_str(),
                           status, SYBFLT8, -1,
                           is_null ? 0 : -1, (BYTE*) val.BindVal());
            break;
        }
        case eDB_SmallDateTime: {
            CDB_SmallDateTime& val = dynamic_cast<CDB_SmallDateTime&> (param);
            DBDATETIME4* dt        = (DBDATETIME4*) param_buff;
            DBDATETIME4_days(dt)   = val.GetDays();
            DBDATETIME4_mins(dt)   = val.GetMinutes();
            r = dbrpcparam(m_Cmd, (char*) m_Params.GetParamName(i).c_str(),
                           status, SYBDATETIME4, -1,
                           is_null ? 0 : -1, (BYTE*) dt);
            param_buff = (char*) (dt + 1);
            break;
        }
        case eDB_DateTime: {
            CDB_DateTime& val = dynamic_cast<CDB_DateTime&> (param);
            DBDATETIME* dt = (DBDATETIME*) param_buff;
            dt->dtdays     = val.GetDays();
            dt->dttime     = val.Get300Secs();
            r = dbrpcparam(m_Cmd, (char*) m_Params.GetParamName(i).c_str(),
                           status, SYBDATETIME, -1,
                           is_null ? 0 : -1, (BYTE*) dt);
            param_buff = (char*) (dt + 1);
            break;
        }
        default:
            return false;
        }
        if (r != SUCCEED)
            return false;
    }
    return true;
}


END_NCBI_SCOPE



/*
 * ===========================================================================
 * $Log$
 * Revision 1.4  2002/01/03 15:46:23  sapojnik
 * ported to MS SQL (about 12 'ifdef NCBI_OS_MSWIN' in 6 files)
 *
 * Revision 1.3  2001/10/24 16:39:32  lavr
 * Explicit casts (where necessary) to eliminate 64->32 bit compiler warnings
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
