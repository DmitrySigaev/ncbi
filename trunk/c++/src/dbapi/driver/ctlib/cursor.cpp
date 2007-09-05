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
 * File Description:  CTLib cursor command
 *
 */

#include <ncbi_pch.hpp>
#include <dbapi/driver/ctlib/interfaces.hpp>


BEGIN_NCBI_SCOPE

#ifdef FTDS_IN_USE
namespace ftds64_ctlib
{
#endif


/////////////////////////////////////////////////////////////////////////////
//
//  CTL_CursorCmd::
//

CTL_CursorCmd::CTL_CursorCmd(CTL_Connection& conn,
                             const string& cursor_name,
                             const string& query,
                             unsigned int nof_params,
                             unsigned int fetch_size
                             )
: CTL_Cmd(conn)
, impl::CBaseCmd(conn, cursor_name, query, nof_params)
, m_FetchSize(fetch_size)
{
    string extra_msg = "Cursor Name: \"" + cursor_name + "\"; SQL Command: \""+
        query + "\"";
    SetExecCntxInfo(extra_msg);
}


CS_RETCODE
CTL_CursorCmd::CheckSFB(CS_RETCODE rc, const char* msg, unsigned int msg_num)
{
    try {
        rc = Check(rc);
    } catch (...) {
        SetHasFailed();
        throw;
    }

    switch (rc) {
    case CS_SUCCEED:
        break;
    case CS_FAIL:
        SetHasFailed();
        DATABASE_DRIVER_ERROR( msg, msg_num );
#ifdef CS_BUSY
    case CS_BUSY:
        DATABASE_DRIVER_ERROR( "the connection is busy", 122002 );
#endif
    }

    return rc;
}


CS_RETCODE
CTL_CursorCmd::CheckSFBCP(CS_RETCODE rc, const char* msg, unsigned int msg_num)
{
    try {
        rc = Check(rc);
    } catch (...) {
        SetHasFailed();
        throw;
    }

    switch (rc) {
    case CS_SUCCEED:
        break;
    case CS_FAIL:
        SetHasFailed();
        DATABASE_DRIVER_ERROR( msg, msg_num );
#ifdef CS_BUSY
    case CS_BUSY:
        DATABASE_DRIVER_ERROR( "the connection is busy", 122002 );
#endif
    case CS_CANCELED:
        DATABASE_DRIVER_ERROR( "command was canceled", 122008 );
    case CS_PENDING:
        DATABASE_DRIVER_ERROR( "connection has another request pending", 122007 );
    }

    return rc;
}


bool
CTL_CursorCmd::ProcessResults(void)
{
    // process the results
    for (;;) {
        CS_INT res_type;

        if (CheckSFBCP(ct_results(x_GetSybaseCmd(), &res_type),
                       "ct_result failed", 122045) == CS_END_RESULTS) {
            return true;
        }

        if (ProcessResultInternal(res_type)) {
            continue;
        }

        switch ( res_type ) {
        case CS_CMD_SUCCEED:
        case CS_CMD_DONE: // done with this command
            continue;
        case CS_CMD_FAIL: // the command has failed
            SetHasFailed();
            while(Check(ct_results(x_GetSybaseCmd(), &res_type)) == CS_SUCCEED)
            {
                continue;
            }
            DATABASE_DRIVER_WARNING( "The server encountered an error while "
                               "executing a command", 122049 );
        default:
            continue;
        }
    }

    return false;
}


CDB_Result*
CTL_CursorCmd::OpenCursor()
{
    // need to close it first
    CloseCursor();

    if (!CursorIsDeclared()) {
        SetHasFailed(false);

        CheckSFB(ct_cursor(x_GetSybaseCmd(), CS_CURSOR_DECLARE,
                           const_cast<char*> (GetCmdName().c_str()), CS_NULLTERM,
                           const_cast<char*> (GetQuery().c_str()), CS_NULLTERM,
                           CS_UNUSED),
                 "ct_cursor(DECLARE) failed", 122001);

        if (GetParams().NofParams() > 0) {
            // we do have the parameters
            // check if query is a select statement or a function call
            if (GetQuery().find("select") != string::npos  ||
                GetQuery().find("SELECT") != string::npos) {
                // this is a select
                SetHasFailed(!x_AssignParams(true));
                CHECK_DRIVER_ERROR( HasFailed(), "Cannot declare the params." +
                                    GetDbgInfo(), 122003 );
            }
        }

        if (m_FetchSize > 1) {
            CheckSFB(ct_cursor(x_GetSybaseCmd(), CS_CURSOR_ROWS, 0, CS_UNUSED,
                               0, CS_UNUSED, (CS_INT) m_FetchSize),
                     "ct_cursor(ROWS) failed", 122004);
        }

        // Execute command ...
        // Current inplementation of FreeTDS ctlib won't send CS_CURSOR_DECLARE
        // command. So, error processing can be done only after CS_CURSOR_OPEN
        // was sent.
        {
            // send this command
            CheckSFBCP(ct_send(x_GetSybaseCmd()), "ct_send failed", 122006);

            // Check results of send ...
            ProcessResults();
        }

        SetCursorDeclared();
    }

    SetHasFailed(false);

    // open cursor
    CheckSFB(ct_cursor(x_GetSybaseCmd(), CS_CURSOR_OPEN, 0, CS_UNUSED, 0, CS_UNUSED,
                       CursorIsDeclared() ? CS_RESTORE_OPEN : CS_UNUSED),
             "ct_cursor(open) failed", 122005);

    if (GetParams().NofParams() > 0) {
        // we do have parameters
        SetHasFailed(!x_AssignParams(false));
        CHECK_DRIVER_ERROR( HasFailed(), "Cannot assign the params." + GetDbgInfo(), 122003 );
    }

    // send this command
    CheckSFBCP(ct_send(x_GetSybaseCmd()), "ct_send failed", 122006);

    // Process results ....
    for (;;) {
        CS_INT res_type;

        try {
            if (CheckSFBCP(ct_results(x_GetSybaseCmd(), &res_type),
                        "ct_result failed", 122013) == CS_END_RESULTS) {
                return NULL;
            }
        } catch (...) {
            // We have to fech out all pending  results ...
            while (ct_results(x_GetSybaseCmd(), &res_type) == CS_SUCCEED) {
                continue;
            }
            throw;
        }

        switch ( res_type ) {
        case CS_CMD_SUCCEED:
        case CS_CMD_DONE:
            // done with this command -- check the number of affected rows
            GetRowCount(&m_RowCount);

            continue;
        case CS_CMD_FAIL:
            // the command has failed -- check the number of affected rows
            GetRowCount(&m_RowCount);
            SetHasFailed();
            while (Check(ct_results(x_GetSybaseCmd(), &res_type)) == CS_SUCCEED) {
                continue;
            }
            DATABASE_DRIVER_WARNING( "The server encountered an error while "
                               "executing a command", 122016 );
        case CS_CURSOR_RESULT:
            // Cursor can be set open only after processing of ct_send, which does
            // actual job.
            SetCursorOpen();

            SetResult(MakeCursorResult());
            break;
        default:
            continue;
        }

        return Create_Result(static_cast<impl::CResult&>(GetResult()));
    }
}


bool CTL_CursorCmd::Update(const string& table_name, const string& upd_query)
{
    if (!CursorIsOpen()) {
        return false;
    }

    CheckSFB(ct_cursor(x_GetSybaseCmd(), CS_CURSOR_UPDATE,
                       const_cast<char*> (table_name.c_str()), CS_NULLTERM,
                       const_cast<char*> (upd_query.c_str()),  CS_NULLTERM,
                       CS_UNUSED),
             "ct_cursor(update) failed", 122030);

    // send this command
    CheckSFBCP(ct_send(x_GetSybaseCmd()), "ct_send failed", 122032);

    // process results
    return ProcessResults();
}

I_ITDescriptor* CTL_CursorCmd::x_GetITDescriptor(unsigned int item_num)
{
    if(!CursorIsOpen() || !HaveResult()) {
        return 0;
    }

    while ( static_cast<unsigned int>(GetResult().CurrentItemNo()) < item_num ) {
        if(!GetResult().SkipItem()) return 0;
    }

    I_ITDescriptor* desc= 0;
    if(static_cast<unsigned int>(GetResult().CurrentItemNo()) == item_num) {
        desc = GetResult().GetImageOrTextDescriptor();
    }
    else {
        auto_ptr<CTL_ITDescriptor> dsc(new CTL_ITDescriptor);

        bool rc = (Check(ct_data_info(x_GetSybaseCmd(), CS_GET, item_num+1, &dsc->m_Desc))
                   != CS_SUCCEED);

        CHECK_DRIVER_ERROR(
            rc,
            "ct_data_info failed." + GetDbgInfo(),
            130010 );
        desc = dsc.release();
    }
    return desc;
}

bool CTL_CursorCmd::UpdateTextImage(unsigned int item_num, CDB_Stream& data,
                    bool log_it)
{
    I_ITDescriptor* desc= x_GetITDescriptor(item_num);
    auto_ptr<I_ITDescriptor> d_guard(desc);

    return (desc) ? x_SendData(*desc, data, log_it) : false;
}

CDB_SendDataCmd* CTL_CursorCmd::SendDataCmd(unsigned int item_num, size_t size,
                        bool log_it)
{
    I_ITDescriptor* desc= x_GetITDescriptor(item_num);
    auto_ptr<I_ITDescriptor> d_guard(desc);

    return (desc) ? ConnSendDataCmd(*desc, size, log_it) : 0;
}

bool CTL_CursorCmd::Delete(const string& table_name)
{
    if (!CursorIsOpen()) {
        return false;
    }

    CheckSFB(ct_cursor(x_GetSybaseCmd(), CS_CURSOR_DELETE,
                       const_cast<char*> (table_name.c_str()), CS_NULLTERM,
                       0, CS_UNUSED, CS_UNUSED),
             "ct_cursor(delete) failed", 122040);

    // send this command
    CheckSFBCP(ct_send(x_GetSybaseCmd()), "ct_send failed", 122042);

    // process the results
    return ProcessResults();
}


int CTL_CursorCmd::RowCount() const
{
    return m_RowCount;
}


bool CTL_CursorCmd::CloseCursor()
{
    if (!CursorIsOpen()) {
        return false;
    }

    DeleteResult();

    CheckSFB(ct_cursor(x_GetSybaseCmd(),
                       CS_CURSOR_CLOSE,
                       0,
                       CS_UNUSED,
                       0,
                       CS_UNUSED,
                       CS_UNUSED),
             "ct_cursor(close) failed", 122020);

    // send this command
    CheckSFBCP(ct_send(x_GetSybaseCmd()), "ct_send failed", 122022);

    // Process results ...
    bool result = ProcessResults();

    // An exception can be thrown in ProcessResults, so, we set the flag after
    // calling of ProcessResults.
    SetCursorOpen(!result);

    return result;
}


void
CTL_CursorCmd::CloseForever(void)
{
    if (x_GetSybaseCmd()) {

        // ????
        DetachInterface();

        CloseCursor();

        if (CursorIsDeclared()) {
            // deallocate the cursor
            switch ( Check(ct_cursor(x_GetSybaseCmd(), CS_CURSOR_DEALLOC,
                               0, CS_UNUSED, 0, CS_UNUSED, CS_UNUSED)) ) {
            case CS_SUCCEED:
                break;
            case CS_FAIL:
                // SetHasFailed();
                //throw CDB_ClientEx(eDiag_Fatal, 122050, "::~CTL_CursorCmd",
                //                   "ct_cursor(dealloc) failed");
#ifdef CS_BUSY
            case CS_BUSY:
                //throw CDB_ClientEx(eDiag_Error, 122051, "::~CTL_CursorCmd",
                //                   "the connection is busy");
#endif
                DropSybaseCmd();
                return;
            }

            // send this command
            switch ( Check(ct_send(x_GetSybaseCmd())) ) {
            case CS_SUCCEED:
                break;
            case CS_FAIL:
                // SetHasFailed();
                // throw CDB_ClientEx(eDiag_Error, 122052, "::~CTL_CursorCmd",
                //                   "ct_send failed");
            case CS_CANCELED:
                // throw CDB_ClientEx(eDiag_Error, 122053, "::~CTL_CursorCmd",
                //                   "command was canceled");
#ifdef CS_BUSY
            case CS_BUSY:
#endif
            case CS_PENDING:
                // throw CDB_ClientEx(eDiag_Error, 122054, "::~CTL_CursorCmd",
                //                   "connection has another request pending");
                DropSybaseCmd();
                return;
            }

            // process the results
            try {
                ProcessResults();
            } catch(CDB_ClientEx const&)
            {
                // Just ignore ...
                _ASSERT(false);
            }

        }

        DropSybaseCmd();
    }
}


CTL_CursorCmd::~CTL_CursorCmd()
{
    try {
        DetachInterface();

        DropCmd(*this);

        CloseForever();
    }
    NCBI_CATCH_ALL( NCBI_CURRENT_FUNCTION )
}


bool CTL_CursorCmd::x_AssignParams(bool declare_only)
{
    CS_DATAFMT param_fmt;
    memset(&param_fmt, 0, sizeof(param_fmt));
    param_fmt.namelen = CS_NULLTERM;
    param_fmt.status  = CS_INPUTVALUE;

    for (unsigned int i = 0;  i < GetParams().NofParams();  i++) {
        if(GetParams().GetParamStatus(i) == 0) continue;

        CDB_Object&   param = *GetParams().GetParam(i);
        const string& param_name = GetParams().GetParamName(i);
        CS_SMALLINT   indicator = (!declare_only  &&  param.IsNULL()) ? -1 : 0;

        if ( !AssignCmdParam(param, param_name, param_fmt, indicator, declare_only) ) {
            return false;
        }
    }

    return true;
}


/////////////////////////////////////////////////////////////////////////////
//
//  CTL_CursorCmdExpl::
//

CTL_CursorCmdExpl::CTL_CursorCmdExpl(CTL_Connection& conn,
        const string& cursor_name,
        const string& query,
        unsigned int nof_params,
        unsigned int fetch_size
        )
: CTL_Cmd(conn)
, impl::CBaseCmd(conn, cursor_name, query, nof_params)
, m_FetchSize(fetch_size)
{
    string extra_msg = "Cursor Name: \"" + cursor_name + "\"; SQL Command: \""+
        query + "\"";
    SetExecCntxInfo(extra_msg);

    m_CursSql =
        "declare " + cursor_name + " cursor for " + query
        ;
}


CDB_Result*
CTL_CursorCmdExpl::OpenCursor()
{
    // need to close it first
    CloseCursor();

    SetHasFailed(false);

    // declare the cursor
    try {
        auto_ptr<CDB_LangCmd> stmt(GetConnection().LangCmd(m_CursSql));

        stmt->Send();
        stmt->DumpResults();
    } catch (const CDB_Exception& e) {
        string err_message = "Failed to declare cursor." + GetDbgInfo();
        DATABASE_DRIVER_ERROR_EX( e, err_message, 422001 );
    }

    SetCursorDeclared();

    try {
        auto_ptr<CDB_LangCmd> stmt(GetConnection().LangCmd("open " + GetCmdName()));

        stmt->Send();
        stmt->DumpResults();
    } catch (const CDB_Exception& e) {
        string err_message = "Failed to open cursor." + GetDbgInfo();
        DATABASE_DRIVER_ERROR_EX( e, err_message, 422002 );
    }

    SetCursorOpen();

    m_LCmd.reset(GetConnection().xLangCmd("fetch " + GetCmdName()));
    m_Res.reset(new CTL_CursorResultExpl(*m_LCmd));

    return Create_Result(*m_Res);
}


bool CTL_CursorCmdExpl::Update(const string& table_name, const string& upd_query)
{
    if (!CursorIsOpen())
        return false;

    try {
        m_LCmd->Cancel();

        string buff = upd_query + " where current of " + GetCmdName();

        auto_ptr<CDB_LangCmd> cmd( GetConnection().LangCmd(buff) );
        cmd->Send();
        cmd->DumpResults();
    } catch (const CDB_Exception& e) {
        string err_message = "Update failed." + GetDbgInfo();
        DATABASE_DRIVER_ERROR_EX( e, err_message, 422004 );
    }

    return true;
}

I_ITDescriptor* CTL_CursorCmdExpl::x_GetITDescriptor(unsigned int item_num)
{
    if(!CursorIsOpen() || m_Res.get() == 0 || m_LCmd.get() == 0) {
        return NULL;
    }

    string cond = "current of " + GetCmdName();

    // Not ready yet ...
    // return m_LCmd->m_Res->GetImageOrTextDescriptor(item_num, cond);
    return NULL;
}

bool CTL_CursorCmdExpl::UpdateTextImage(unsigned int item_num, CDB_Stream& data,
                    bool log_it)
{
    // Not ready yet ...
    // CDB_ITDescriptor* desc= x_GetITDescriptor(item_num);
    CDB_ITDescriptor* desc = NULL; 

    if(desc == NULL) {
        return false;
    }

    auto_ptr<I_ITDescriptor> g((I_ITDescriptor*)desc);

    m_LCmd->Cancel();

    return (data.GetType() == eDB_Text)?
        GetConnection().SendData(*desc, (CDB_Text&)data, log_it) :
        GetConnection().SendData(*desc, (CDB_Image&)data, log_it);
}

CDB_SendDataCmd* CTL_CursorCmdExpl::SendDataCmd(unsigned int item_num, size_t size,
                        bool log_it)
{
    // Not ready yet ...
    // CDB_ITDescriptor* desc= x_GetITDescriptor(item_num);
    CDB_ITDescriptor* desc = NULL; 

    if(desc == NULL) {
        return NULL;
    }

    auto_ptr<I_ITDescriptor> g((I_ITDescriptor*)desc);

    m_LCmd->Cancel();

    return GetConnection().SendDataCmd((I_ITDescriptor&)*desc, size, log_it);
}

bool CTL_CursorCmdExpl::Delete(const string& table_name)
{
    if (!CursorIsOpen())
        return false;

    try {
        m_LCmd->Cancel();

        string buff = "delete " + table_name + " where current of " + GetCmdName();

        auto_ptr<CDB_LangCmd> cmd(GetConnection().LangCmd(buff));
        cmd->Send();
        cmd->DumpResults();
    } catch (const CDB_Exception& e) {
        string err_message = "Update failed." + GetDbgInfo();
        DATABASE_DRIVER_ERROR_EX( e, err_message, 422004 );
    }

    return true;
}


int CTL_CursorCmdExpl::RowCount() const
{
    return m_RowCount;
}


bool CTL_CursorCmdExpl::CloseCursor()
{
    if (!CursorIsOpen())
        return false;

    m_Res.reset();
    m_LCmd.reset();

    if (CursorIsOpen()) {
        string buff = "close " + GetCmdName();
        try {
            auto_ptr<CTL_LangCmd> cmd(GetConnection().xLangCmd(buff));

            cmd->Send();
            cmd->DumpResults();
        } catch (const CDB_Exception& e) {
            string err_message = "Failed to close cursor." + GetDbgInfo();
            DATABASE_DRIVER_ERROR_EX( e, err_message, 422003 );
        }

        SetCursorOpen(false);
    }

    if (CursorIsDeclared()) {
        string buff = "deallocate " + GetCmdName();

        try {
            auto_ptr<CTL_LangCmd> cmd(GetConnection().xLangCmd(buff));

            cmd->Send();
            cmd->DumpResults();
        } catch (const CDB_Exception& e) {
            string err_message = "Failed to deallocate cursor." + GetDbgInfo();
            DATABASE_DRIVER_ERROR_EX( e, err_message, 422003 );
        }

        SetCursorDeclared(false);
    }

    return true;
}


CTL_CursorCmdExpl::~CTL_CursorCmdExpl()
{
    try {
        DetachInterface();

        DropCmd(*this);

        // CloseForever();
        CloseCursor();
    }
    NCBI_CATCH_ALL( NCBI_CURRENT_FUNCTION )
}


#ifdef FTDS_IN_USE
} // namespace ftds64_ctlib
#endif

END_NCBI_SCOPE


