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
 * File Description:  CTLib RPC command
 *
 */

#include <dbapi/driver/ctlib/interfaces.hpp>


BEGIN_NCBI_SCOPE


/////////////////////////////////////////////////////////////////////////////
//
//  CTL_RPCCmd::
//

CTL_RPCCmd::CTL_RPCCmd(CTL_Connection* conn, CS_COMMAND* cmd,
                       const string& proc_name, unsigned int nof_params)
    : m_Connect(conn),
      m_Cmd(cmd),
      m_Query(proc_name),
      m_Params(nof_params)
{
    m_WasSent     = false;
    m_HasFailed   = false;
    m_Recompile   = false;
    m_Res         = 0;
    m_RowCount    = -1;
}


bool CTL_RPCCmd::BindParam(const string& param_name, CDB_Object* param_ptr,
                           bool out_param)
{
    return m_Params.BindParam(CDB_Params::kNoParamNumber, param_name,
                              param_ptr, out_param);
}


bool CTL_RPCCmd::SetParam(const string& param_name, CDB_Object* param_ptr,
                          bool out_param)
{
    return m_Params.SetParam(CDB_Params::kNoParamNumber, param_name,
                             param_ptr, out_param);
}


bool CTL_RPCCmd::Send()
{
    if ( m_WasSent ) {
        Cancel();
    }

    m_HasFailed = false;

    switch ( ct_command(m_Cmd, CS_RPC_CMD,
                        const_cast<char*> (m_Query.c_str()), CS_NULLTERM,
                        m_Recompile ? CS_RECOMPILE : CS_UNUSED) ) {
    case CS_SUCCEED:
        break;
    case CS_FAIL:
        m_HasFailed = true;
        throw CDB_ClientEx(eDB_Fatal, 121001, "CTL_RPCCmd::Send",
                           "ct_command failed");
    case CS_BUSY:
        throw CDB_ClientEx(eDB_Error, 121002, "CTL_RPCCmd::Send",
                           "the connection is busy");
    }

    if ( !x_AssignParams() ) {
        m_HasFailed = true;
        throw CDB_ClientEx(eDB_Error, 121003, "CTL_RPCCmd::Send",
                           "cannot assign the params");
    }

    switch ( ct_send(m_Cmd) ) {
    case CS_SUCCEED:
        break;
    case CS_FAIL:
        m_HasFailed = true;
        if (ct_cancel(0, m_Cmd, CS_CANCEL_ALL) != CS_SUCCEED) {
            // we need to close this connection
            throw CDB_ClientEx(eDB_Fatal, 121004, "CTL_RPCCmd::Send",
                               "Unrecoverable crash of ct_send. "
                               "Connection must be closed");
        }
        throw CDB_ClientEx(eDB_Error, 121005, "CTL_RPCCmd::Send",
                           "ct_send failed");
    case CS_CANCELED:
        throw CDB_ClientEx(eDB_Error, 121006, "CTL_RPCCmd::Send",
                           "command was canceled");
    case CS_BUSY:
        throw CDB_ClientEx(eDB_Error, 121007, "CTL_RPCCmd::Send",
                           "connection has another request pending");
    case CS_PENDING:
    default:
        m_WasSent = true;
        return false;
    }

    m_WasSent = true;
    return true;
}


bool CTL_RPCCmd::WasSent() const
{
    return m_WasSent;
}


bool CTL_RPCCmd::Cancel()
{
    if ( !m_WasSent ) {
        return true;
    }

    if ( m_Res ) {
        delete m_Res;
        m_Res = 0;
    }

    switch ( ct_cancel(0, m_Cmd, CS_CANCEL_ALL) ) {
    case CS_SUCCEED:
        m_WasSent = false;
        return true;
    case CS_FAIL:
        throw CDB_ClientEx(eDB_Error, 121008, "CTL_RPCCmd::Cancel",
                           "ct_cancel failed");
    case CS_BUSY:
        throw CDB_ClientEx(eDB_Error, 121009, "CTL_RPCCmd::Cancel",
                           "connection has another request pending");
    default:
        return false;
    }
}


bool CTL_RPCCmd::WasCanceled() const
{
    return !m_WasSent;
}


CDB_Result* CTL_RPCCmd::Result()
{
    if ( m_Res ) {
        delete m_Res;
        m_Res = 0;
    }

    if ( !m_WasSent ) {
        throw CDB_ClientEx(eDB_Error, 121010, "CTL_RPCCmd::Result",
                           "you need to send a command first");
    }

    for (;;) {
        CS_INT res_type;

        switch ( ct_results(m_Cmd, &res_type) ) {
        case CS_SUCCEED:
            break;
        case CS_END_RESULTS:
            m_WasSent = false;
            return 0;
        case CS_FAIL:
            m_HasFailed = true;
            if (ct_cancel(0, m_Cmd, CS_CANCEL_ALL) != CS_SUCCEED) {
                // we need to close this connection
                throw CDB_ClientEx(eDB_Fatal, 121012, "CTL_RPCCmd::Result",
                                   "Unrecoverable crash of ct_result. "
                                   "Connection must be closed");
            }
            m_WasSent = false;
            throw CDB_ClientEx(eDB_Error, 121013, "CTL_RPCCmd::Result",
                               "ct_result failed");
        case CS_CANCELED:
            m_WasSent = false;
            throw CDB_ClientEx(eDB_Error, 121011, "CTL_RPCCmd::Result",
                               "your command has been canceled");
        case CS_BUSY:
            throw CDB_ClientEx(eDB_Error, 121014, "CTL_RPCCmd::Result",
                               "connection has another request pending");
        default:
            throw CDB_ClientEx(eDB_Error, 121015, "CTL_RPCCmd::Result",
                               "your request is pending");
        }

        switch ( res_type ) {
        case CS_CMD_SUCCEED:
        case CS_CMD_DONE: // done with this command
            // check the number of affected rows
            g_CTLIB_GetRowCount(m_Cmd, &m_RowCount);
            continue;
        case CS_CMD_FAIL: // the command has failed
            g_CTLIB_GetRowCount(m_Cmd, &m_RowCount);
            m_HasFailed = true;
            throw CDB_ClientEx(eDB_Warning, 121016, "CTL_RPCCmd::Result",
                               "The server encountered an error while "
                               "executing a command");
        case CS_ROW_RESULT:
            m_Res = new CTL_RowResult(m_Cmd);
            break;
        case CS_PARAM_RESULT:
            m_Res = new CTL_ParamResult(m_Cmd);
            break;
        case CS_COMPUTE_RESULT:
            m_Res = new CTL_ComputeResult(m_Cmd);
            break;
        case CS_STATUS_RESULT:
            m_Res = new CTL_StatusResult(m_Cmd);
            break;
        case CS_COMPUTEFMT_RESULT:
            throw CDB_ClientEx(eDB_Info, 121017, "CTL_RPCCmd::Result",
                               "CS_COMPUTEFMT_RESULT has arrived");
        case CS_ROWFMT_RESULT:
            throw CDB_ClientEx(eDB_Info, 121018, "CTL_RPCCmd::Result",
                               "CS_ROWFMT_RESULT has arrived");
        case CS_MSG_RESULT:
            throw CDB_ClientEx(eDB_Info, 121019, "CTL_RPCCmd::Result",
                               "CS_MSG_RESULT has arrived");
        default:
            throw CDB_ClientEx(eDB_Warning, 121020, "CTL_RPCCmd::Result",
                               "Unexpected result type has arrived");
        }

        return Create_Result(*m_Res);
    }
}


bool CTL_RPCCmd::HasMoreResults() const
{
    return m_WasSent;
}


bool CTL_RPCCmd::HasFailed() const
{
    return m_HasFailed;
}


int CTL_RPCCmd::RowCount() const
{
    return m_RowCount;
}


void CTL_RPCCmd::SetRecompile(bool recompile)
{
    m_Recompile = recompile;
}


void CTL_RPCCmd::Release()
{
    m_BR = 0;
    if ( m_WasSent) {
        try {
            Cancel();
        } catch (CDB_Exception& e) {}
        m_WasSent = false;
    }

    m_Connect->DropCmd(*this);
    delete this;
}


CTL_RPCCmd::~CTL_RPCCmd()
{
    if ( m_BR ) {
        *m_BR = 0;
    }

    if ( m_WasSent ) {
        try {
            Cancel();
        } catch (CDB_Exception& e) {}
    }

    if (ct_cmd_drop(m_Cmd) != CS_SUCCEED) {
        throw CDB_ClientEx(eDB_Fatal, 121021, "CTL_RPCCmd::~CTL_RPCCmd",
                           "ct_cmd_drop failed");
    }
}


bool CTL_RPCCmd::x_AssignParams()
{
    CS_DATAFMT param_fmt;
    memset(&param_fmt, 0, sizeof(param_fmt));
    param_fmt.namelen = CS_NULLTERM;

    for (unsigned int i = 0;  i < m_Params.NofParams();  i++) {

        CDB_Object&   param      = *m_Params.GetParam(i);
        const string& param_name = m_Params.GetParamName(i);
        CS_SMALLINT   indicator  = param.IsNULL() ? -1 : 0;

        param_fmt.status =
            ((m_Params.GetParamStatus(i) & CDB_Params::fOutput) == 0)
            ? CS_INPUTVALUE : CS_RETURN;

        if ( !g_CTLIB_AssignCmdParam(m_Cmd, param, param_name, param_fmt,
                                     indicator, false/*!declare_only*/) ) {
            return false;
        }
    }

    return true;
}


END_NCBI_SCOPE



/*
 * ===========================================================================
 * $Log$
 * Revision 1.3  2002/09/16 19:40:03  soussov
 * add try catch when canceling in Release method
 *
 * Revision 1.2  2001/11/06 17:59:55  lavr
 * Formatted uniformly as the rest of the library
 *
 * Revision 1.1  2001/09/21 23:40:02  vakatov
 * -----  Initial (draft) revision.  -----
 * This is a major revamp (by Denis Vakatov, with help from Vladimir Soussov)
 * of the DBAPI "driver" libs originally written by Vladimir Soussov.
 * The revamp involved massive code shuffling and grooming, numerous local
 * API redesigns, adding comments and incorporating DBAPI to the C++ Toolkit.
 *
 * ===========================================================================
 */
