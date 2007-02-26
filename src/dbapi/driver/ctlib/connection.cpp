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
 * File Description:  CTLib connection
 *
 */

#include <ncbi_pch.hpp>

#include "ctlib_utils.hpp"

#include <dbapi/driver/ctlib/interfaces.hpp>

#include <string.h>
#include <algorithm>

#if defined(NCBI_OS_MSWIN)
#  include <winsock2.h>
#  include <io.h>
inline int close(int fd)
{
    return _close(fd);
}
#else
#  include <unistd.h>
#endif


BEGIN_NCBI_SCOPE

#ifdef FTDS_IN_USE
BEGIN_SCOPE(ftds64_ctlib)
#endif

////////////////////////////////////////////////////////////////////////////
CTL_Connection::CTL_Connection(CTLibContext& cntx,
                               const I_DriverContext::SConnAttr& conn_attr) :
    impl::CConnection(cntx, false, conn_attr.reusable, conn_attr.pool_name),
    m_Cntx(&cntx),
    m_Handle(cntx, this)
{
    string extra_msg = " SERVER: " + conn_attr.srv_name + "; USER: " + conn_attr.user_name;
    SetExtraMsg(extra_msg);

    GetCTLibContext().Check(ct_callback(NULL,
                           x_GetSybaseConn(),
                           CS_SET,
                           CS_CLIENTMSG_CB,
                           (CS_VOID*) CTLibContext::CTLIB_cterr_handler));

    GetCTLibContext().Check(ct_callback(NULL,
                           x_GetSybaseConn(),
                           CS_SET,
                           CS_SERVERMSG_CB,
                           (CS_VOID*) CTLibContext::CTLIB_srverr_handler));

    char hostname[256];

    if(gethostname(hostname, 256)) {
      strcpy(hostname, "UNKNOWN");
    } else {
        hostname[255] = '\0';
    }


    if (GetCTLibContext().Check(ct_con_props(x_GetSybaseConn(),
                                CS_SET,
                                CS_USERNAME,
                                (void*) conn_attr.user_name.c_str(),
                                CS_NULLTERM,
                                NULL)) != CS_SUCCEED
        || GetCTLibContext().Check(ct_con_props(x_GetSybaseConn(),
                                   CS_SET,
                                   CS_PASSWORD,
                                   (void*) conn_attr.passwd.c_str(),
                                   CS_NULLTERM,
                                   NULL)) != CS_SUCCEED
        || GetCTLibContext().Check(ct_con_props(x_GetSybaseConn(),
                                   CS_SET,
                                   CS_APPNAME,
                                   (void*) GetCDriverContext().GetApplicationName().c_str(),
                                   CS_NULLTERM,
                                   NULL)) != CS_SUCCEED
        || GetCTLibContext().Check(ct_con_props(x_GetSybaseConn(),
                                   CS_SET,
                                   CS_HOSTNAME,
                                   (void*) hostname,
                                   CS_NULLTERM,
                                   NULL)) != CS_SUCCEED
        // Future development ...
//         || GetCTLibContext().Check(ct_con_props(x_GetSybaseConn(), CS_SET, CS_TDS_VERSION, &m_TDSVersion,
//                      CS_UNUSED, NULL)) != CS_SUCCEED
        )
    {
        DATABASE_DRIVER_ERROR( "Cannot connection's properties.", 100011 );
    }

    if (cntx.GetLocale()) {
        if (Check(ct_con_props(x_GetSybaseConn(),
                               CS_SET,
                               CS_LOC_PROP,
                               (void*) cntx.GetLocale(),
                               CS_UNUSED,
                               NULL)) != CS_SUCCEED
            ) {
            DATABASE_DRIVER_ERROR( "Cannot set a connection locale.", 100011 );
        }
    }
//     if ( !GetCDriverContext().GetHostName().empty() ) {
//         GetCTLibContext().Check(ct_con_props(x_GetSybaseConn(),
//                                 CS_SET,
//                                 CS_HOSTNAME,
//                                 (void*) GetCDriverContext().GetHostName().c_str(),
//                                 CS_NULLTERM,
//                                 NULL));
//     }

    if (cntx.GetPacketSize() > 0) {
        CS_INT packet_size = cntx.GetPacketSize();

        GetCTLibContext().Check(ct_con_props(x_GetSybaseConn(),
                                CS_SET,
                                CS_PACKETSIZE,
                                (void*) &packet_size,
                                CS_UNUSED,
                                NULL));
    }

#if defined(CS_RETRY_COUNT)
    if (cntx.GetLoginRetryCount() > 0) {
        CS_INT login_retry_count = cntx.GetLoginRetryCount();

        GetCTLibContext().Check(ct_con_props(x_GetSybaseConn(),
                                CS_SET,
                                CS_RETRY_COUNT,
                                (void*) &login_retry_count,
                                CS_UNUSED,
                                NULL));
    }
#endif

#if defined (CS_LOOP_DELAY)
    if (cntx.GetLoginLoopDelay() > 0) {
        CS_INT login_loop_delay = cntx.GetLoginLoopDelay();

        GetCTLibContext().Check(ct_con_props(x_GetSybaseConn(),
                                CS_SET,
                                CS_LOOP_DELAY,
                                (void*) &login_loop_delay,
                                CS_UNUSED,
                                NULL));
    }
#endif

    CS_BOOL flag = CS_TRUE;
    if ((conn_attr.mode & I_DriverContext::fBcpIn) != 0) {
        GetCTLibContext().Check(ct_con_props(x_GetSybaseConn(),
                                CS_SET,
                                CS_BULK_LOGIN,
                                &flag,
                                CS_UNUSED,
                                NULL));
        SetBCPable(true);
    }

    if ((conn_attr.mode & I_DriverContext::fPasswordEncrypted) != 0) {
        GetCTLibContext().Check(ct_con_props(x_GetSybaseConn(),
                                CS_SET,
                                CS_SEC_ENCRYPTION,
                                &flag,
                                CS_UNUSED,
                                NULL));
        SetSecureLogin(true);
    }

    if (!m_Handle.Open(conn_attr.srv_name)) {
        string err;

        err += "Cannot connect to the server '" + conn_attr.srv_name;
        err += "' as user '" + conn_attr.user_name + "'";
        DATABASE_DRIVER_ERROR( err, 100011 );
    }

    CTL_Connection* link = this;
    GetCTLibContext().Check(ct_con_props(x_GetSybaseConn(),
                            CS_SET,
                            CS_USERDATA,
                            &link,
                            (CS_INT) sizeof(link),
                            NULL));

    SetServerName(conn_attr.srv_name);
    SetUserName(conn_attr.user_name);
    SetPassword(conn_attr.passwd);
}


CS_RETCODE
CTL_Connection::Check(CS_RETCODE rc)
{
    GetCTLExceptionStorage().Handle(GetMsgHandlers());

    return rc;
}


CS_RETCODE
CTL_Connection::Check(CS_RETCODE rc, const string& extra_msg)
{
    GetCTLExceptionStorage().Handle(GetMsgHandlers(), extra_msg);

    return rc;
}

CS_RETCODE CTL_Connection::CheckSFB(CS_RETCODE rc,
                                    const char* msg,
                                    unsigned int msg_num)
{
    switch (Check(rc)) {
    case CS_SUCCEED:
        break;
    case CS_FAIL:
#ifdef CS_BUSY
    case CS_BUSY:
#endif
        DATABASE_DRIVER_ERROR( "the connection is busy", 122002 );
    }

    return rc;
}


CS_INT
CTL_Connection::GetBLKVersion(void) const
{
    CS_INT blk_version = BLK_VERSION_100;

    _ASSERT(m_Cntx);
    switch (m_Cntx->GetTDSVersion()) {
    case CS_VERSION_100:
        blk_version = BLK_VERSION_100;
        break;
    case CS_VERSION_110: // Same as CS_VERSION_120
        blk_version = BLK_VERSION_110;
        break;
#ifdef CS_VERSION_125
    case CS_VERSION_125:
        blk_version = BLK_VERSION_125;
        break;
#endif
    }

    return blk_version;
}

bool CTL_Connection::IsAlive()
{
    CS_INT status;
    if (Check(ct_con_props(x_GetSybaseConn(),
                           CS_GET,
                           CS_CON_STATUS,
                           &status,
                           CS_UNUSED,
                           0))
        != CS_SUCCEED)
        return false;

    return
        (status & CS_CONSTAT_CONNECTED) != 0  &&
        (status & CS_CONSTAT_DEAD     ) == 0;
}


CDB_LangCmd* CTL_Connection::LangCmd(const string& lang_query,
                                     unsigned int  nof_params)
{
    string extra_msg = "SQL Command: \"" + lang_query + "\"";
    SetExtraMsg(extra_msg);

    CTL_LangCmd* lcmd = new CTL_LangCmd(
        this,
        lang_query,
        nof_params
        );

    return Create_LangCmd(*lcmd);
}


CDB_RPCCmd* CTL_Connection::RPC(const string& rpc_name,
                                unsigned int  nof_args)
{
    string extra_msg = "RPC Command: " + rpc_name;
    SetExtraMsg(extra_msg);

    CTL_RPCCmd* rcmd = new CTL_RPCCmd(
        this,
        rpc_name,
        nof_args
        );

    return Create_RPCCmd(*rcmd);
}


CDB_BCPInCmd* CTL_Connection::BCPIn(const string& table_name,
                                    unsigned int  nof_columns)
{
    CHECK_DRIVER_ERROR( !IsBCPable(), "No bcp on this connection", 110003 );

    string extra_msg = "BCP Table: " + table_name;
    SetExtraMsg(extra_msg);

    CTL_BCPInCmd* bcmd = new CTL_BCPInCmd(
        this,
        table_name,
        nof_columns
        );

    return Create_BCPInCmd(*bcmd);
}


CDB_CursorCmd* CTL_Connection::Cursor(const string& cursor_name,
                                      const string& query,
                                      unsigned int  nof_params,
                                      unsigned int  batch_size)
{
    string extra_msg = "Cursor Name: \"" + cursor_name + "\"; SQL Command: \""+
        query + "\"";
    SetExtraMsg(extra_msg);

    CTL_CursorCmd* ccmd = new CTL_CursorCmd(
        this,
        cursor_name,
        query,
        nof_params,
        batch_size
        );

    return Create_CursorCmd(*ccmd);
}


CDB_SendDataCmd* CTL_Connection::SendDataCmd(I_ITDescriptor& descr_in,
                                             size_t data_size,
                                             bool log_it)
{

    CTL_SendDataCmd* sd_cmd = new CTL_SendDataCmd(this,
                                                  descr_in,
                                                  data_size,
                                                  log_it
                                                  );
    return Create_SendDataCmd(*sd_cmd);
}


bool CTL_Connection::SendData(I_ITDescriptor& desc, CDB_Image& img,
                              bool log_it)
{
    return x_SendData(desc, img, log_it);
}


bool CTL_Connection::SendData(I_ITDescriptor& desc, CDB_Text& txt,
                              bool log_it)
{
    return x_SendData(desc, txt, log_it);
}


bool CTL_Connection::Refresh()
{
    // close all commands first
    DeleteAllCommands();

    // cancel all pending commands
    if (Check(ct_cancel(x_GetSybaseConn(), 0, CS_CANCEL_ALL) != CS_SUCCEED))
        return false;

    // check the connection status
    CS_INT status;
    if (Check(ct_con_props(x_GetSybaseConn(),
                           CS_GET,
                           CS_CON_STATUS,
                           &status,
                           CS_UNUSED,
                           0))
        != CS_SUCCEED)
        return false;

    return
        (status & CS_CONSTAT_CONNECTED) != 0  &&
        (status & CS_CONSTAT_DEAD     ) == 0;
}


I_DriverContext::TConnectionMode CTL_Connection::ConnectMode() const
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


CTL_Connection::~CTL_Connection()
{
    try {
        Close();
    }
    NCBI_CATCH_ALL( NCBI_CURRENT_FUNCTION )
}


bool CTL_Connection::x_SendData(I_ITDescriptor& descr_in,
                                CDB_Stream& img,
                                bool log_it)
{
    CS_INT size = (CS_INT) img.Size();
    if ( !size )
        return false;

    I_ITDescriptor* p_desc = 0;

    // check what type of descriptor we've got
    if(descr_in.DescriptorType() != CTL_ITDESCRIPTOR_TYPE_MAGNUM) {
        // this is not a native descriptor
        p_desc = x_GetNativeITDescriptor
            (dynamic_cast<CDB_ITDescriptor&> (descr_in));
        if(p_desc == 0)
            return false;
    }


    auto_ptr<I_ITDescriptor> d_guard(p_desc);
    CS_COMMAND* cmd = NULL;

    CheckSFB(
        ct_cmd_alloc(
            x_GetSybaseConn(),
            &cmd
            ),
        "ct_cmd_alloc failed", 110001
        );

    if (Check(ct_command(cmd, CS_SEND_DATA_CMD, 0, CS_UNUSED, CS_COLUMN_DATA))
        != CS_SUCCEED) {
        Check(ct_cmd_drop(cmd));
        DATABASE_DRIVER_ERROR( "ct_command failed", 110031 );
    }

    CTL_ITDescriptor& desc = p_desc ?
        dynamic_cast<CTL_ITDescriptor&> (*p_desc) :
            dynamic_cast<CTL_ITDescriptor&> (descr_in);
    // desc->m_Desc.datatype = CS_TEXT_TYPE;
    desc.m_Desc.total_txtlen  = size;
    desc.m_Desc.log_on_update = log_it ? CS_TRUE : CS_FALSE;

    if (Check(ct_data_info(cmd, CS_SET, CS_UNUSED, &desc.m_Desc)) != CS_SUCCEED) {
        Check(ct_cancel(0, cmd, CS_CANCEL_ALL));
        Check(ct_cmd_drop(cmd));
        return false;
    }

    while (size > 0) {
        char   buff[1800];
        CS_INT n_read = (CS_INT) img.Read(buff, sizeof(buff));
        if ( !n_read ) {
            Check(ct_cancel(0, cmd, CS_CANCEL_ALL));
            Check(ct_cmd_drop(cmd));
            DATABASE_DRIVER_ERROR( "Text/Image data corrupted", 110032 );
        }
        if (Check(ct_send_data(cmd, buff, n_read)) != CS_SUCCEED) {
            Check(ct_cancel(0, cmd, CS_CANCEL_CURRENT));
            Check(ct_cmd_drop(cmd));
            DATABASE_DRIVER_ERROR( "ct_send_data failed", 110033 );
        }
        size -= n_read;
    }

    if (Check(ct_send(cmd)) != CS_SUCCEED) {
        Check(ct_cancel(0, cmd, CS_CANCEL_CURRENT));
        Check(ct_cmd_drop(cmd));
        DATABASE_DRIVER_ERROR( "ct_send failed", 110034 );
    }

    for (;;) {
        CS_INT res_type;
        switch ( Check(ct_results(cmd, &res_type)) ) {
        case CS_SUCCEED: {
            if (x_ProcessResultInternal(cmd, res_type)) {
                continue;
            }

            switch (res_type) {
            case CS_COMPUTE_RESULT:
            case CS_CURSOR_RESULT:
            case CS_PARAM_RESULT:
            case CS_ROW_RESULT:
            case CS_STATUS_RESULT: {
                CS_RETCODE ret_code;
                while ((ret_code = Check(ct_fetch(cmd, CS_UNUSED, CS_UNUSED,
                                            CS_UNUSED, 0))) == CS_SUCCEED) {
                    continue;
                }
                if (ret_code != CS_END_DATA) {
                    Check(ct_cmd_drop(cmd));
                    DATABASE_DRIVER_ERROR( "ct_fetch failed", 110036 );
                }
                break;
            }
            case CS_CMD_FAIL:
                Check(ct_cmd_drop(cmd));
                DATABASE_DRIVER_ERROR( "command failed", 110037 );
            default:
                break;
            }
            continue;
        }
        case CS_END_RESULTS: {
            Check(ct_cmd_drop(cmd));
            return true;
        }
        default: {
            if (Check(ct_cancel(0, cmd, CS_CANCEL_ALL)) != CS_SUCCEED) {
                // we need to close this connection
                Check(ct_cmd_drop(cmd));
                DATABASE_DRIVER_ERROR( "Unrecoverable crash of ct_result. "
                                   "Connection must be closed", 110033 );
            }
            Check(ct_cmd_drop(cmd));
            DATABASE_DRIVER_ERROR( "ct_result failed", 110034 );
        }
        }
    }
}


I_ITDescriptor* CTL_Connection::x_GetNativeITDescriptor
(const CDB_ITDescriptor& descr_in)
{
    string q= "set rowcount 1\nupdate ";
    q+= descr_in.TableName();
    q+= " set ";
    q+= descr_in.ColumnName();
    q+= "=NULL where ";
    q+= descr_in.SearchConditions();
    q+= " \nselect ";
    q+= descr_in.ColumnName();
    q+= " from ";
    q+= descr_in.TableName();
    q+= " where ";
    q+= descr_in.SearchConditions();
    q+= " \nset rowcount 0";

    CDB_LangCmd* lcmd= LangCmd(q, 0);
    bool rc = !lcmd->Send();
    CHECK_DRIVER_ERROR( rc, "Cannot send the language command", 110035 );

    CDB_Result* res;
    I_ITDescriptor* descr= 0;

    while(lcmd->HasMoreResults()) {
        res= lcmd->Result();
        if(res == 0) continue;
        if((res->ResultType() == eDB_RowResult) && (descr == 0)) {
            while(res->Fetch()) {
                //res->ReadItem(&i, 0);
                descr= res->GetImageOrTextDescriptor();
                if(descr) break;
            }
        }
        delete res;
    }
    delete lcmd;

    return descr;
}

bool CTL_Connection::Abort()
{
#ifdef CS_ENDPOINT
    int fd = -1;

    if(Check(ct_con_props(x_GetSybaseConn(),
                          CS_GET,
                          CS_ENDPOINT,
                          &fd,
                          CS_UNUSED,
                          0)) == CS_SUCCEED) {
        if(fd >= 0) {
            close(fd);
            return true;
        }
    }
#endif

    return false;
}

bool CTL_Connection::Close(void)
{
    if (m_Handle.IsOpen()) {
        return (!Refresh() || (m_Handle.Close() && m_Handle.Drop()));
    }

    return false;
}

bool
CTL_Connection::x_ProcessResultInternal(CS_COMMAND* cmd, CS_INT res_type)
{
    if(GetResultProcessor()) {
        auto_ptr<impl::CResult> res;
        switch (res_type) {
        case CS_ROW_RESULT:
            res.reset(new CTL_RowResult(cmd, *this));
            break;
        case CS_PARAM_RESULT:
            res.reset(new CTL_ParamResult(cmd, *this));
            break;
        case CS_COMPUTE_RESULT:
            res.reset(new CTL_ComputeResult(cmd, *this));
            break;
        case CS_STATUS_RESULT:
            res.reset(new CTL_StatusResult(cmd, *this));
            break;
        }

        if(res.get()) {
            auto_ptr<CDB_Result> dbres(Create_Result(*res));
            GetResultProcessor()->ProcessResult(*dbres);
            return true;
        }
    }

    return false;
}


/////////////////////////////////////////////////////////////////////////////
//
//  CTL_SendDataCmd::
//


CTL_SendDataCmd::CTL_SendDataCmd(CTL_Connection* conn,
                                 I_ITDescriptor& descr_in,
                                 size_t nof_bytes,
                                 bool log_it
                                 )
: CTL_Cmd(conn)
, impl::CSendDataCmd(nof_bytes)
{
    CHECK_DRIVER_ERROR(!nof_bytes, "wrong (zero) data size", 110092);

    I_ITDescriptor* p_desc = NULL;

    // check what type of descriptor we've got
    if(descr_in.DescriptorType() != CTL_ITDESCRIPTOR_TYPE_MAGNUM) {
        // this is not a native descriptor
        p_desc = GetConnection().x_GetNativeITDescriptor(
            dynamic_cast<CDB_ITDescriptor&> (descr_in)
            );
        if(p_desc == 0) {
            DATABASE_DRIVER_ERROR( "ct_command failedCannot retrieve I_ITDescriptor.", 110093 );
        }
    }

    auto_ptr<I_ITDescriptor> d_guard(p_desc);

    if (Check(ct_command(x_GetSybaseCmd(),
                         CS_SEND_DATA_CMD,
                         0,
                         CS_UNUSED,
                         CS_COLUMN_DATA
                         )
              )
        != CS_SUCCEED) {
        DATABASE_DRIVER_ERROR( "ct_command failed", 110093 );
    }

    CTL_ITDescriptor& desc = p_desc ? dynamic_cast<CTL_ITDescriptor&>(*p_desc) :
        dynamic_cast<CTL_ITDescriptor&> (descr_in);
    // desc.m_Desc.datatype   = CS_TEXT_TYPE;
    desc.m_Desc.total_txtlen  = (CS_INT)nof_bytes;
    desc.m_Desc.log_on_update = log_it ? CS_TRUE : CS_FALSE;

    if (Check(ct_data_info(x_GetSybaseCmd(),
                           CS_SET,
                           CS_UNUSED,
                           &desc.m_Desc
                           )
              ) != CS_SUCCEED) {
        Check(ct_cancel(0, x_GetSybaseCmd(), CS_CANCEL_ALL));
        DATABASE_DRIVER_ERROR( "ct_data_info failed", 110093 );
    }
}


size_t CTL_SendDataCmd::SendChunk(const void* pChunk, size_t nof_bytes)
{
    CHECK_DRIVER_ERROR(
        !pChunk  ||  !nof_bytes,
        "wrong (zero) arguments",
        190000 );

    if ( !m_Bytes2go )
        return 0;

    if (nof_bytes > m_Bytes2go)
        nof_bytes = m_Bytes2go;

    if (Check(ct_send_data(x_GetSybaseCmd(), (void*) pChunk, (CS_INT) nof_bytes)) != CS_SUCCEED){
        DATABASE_DRIVER_ERROR( "ct_send_data failed", 190001 );
    }

    m_Bytes2go -= nof_bytes;
    if ( m_Bytes2go )
        return nof_bytes;

    if (Check(ct_send(x_GetSybaseCmd())) != CS_SUCCEED) {
        Check(ct_cancel(0, x_GetSybaseCmd(), CS_CANCEL_CURRENT));
        DATABASE_DRIVER_ERROR( "ct_send failed", 190004 );
    }

    for (;;) {
        CS_INT res_type;
        switch ( Check(ct_results(x_GetSybaseCmd(), &res_type)) ) {
        case CS_SUCCEED: {
            if (ProcessResultInternal(res_type)) {
                continue;
            }

            switch ( res_type ) {
            case CS_COMPUTE_RESULT:
            case CS_CURSOR_RESULT:
            case CS_PARAM_RESULT:
            case CS_ROW_RESULT:
            case CS_STATUS_RESULT:
                {
                    CS_RETCODE ret_code;
                    while ((ret_code = Check(ct_fetch(x_GetSybaseCmd(),
                                                      CS_UNUSED,
                                                      CS_UNUSED,
                                                      CS_UNUSED, 0))) ==
                           CS_SUCCEED);
                    if (ret_code != CS_END_DATA) {
                        DATABASE_DRIVER_ERROR( "ct_fetch failed", 190006 );
                    }
                    break;
                }
            case CS_CMD_FAIL:
                Check(ct_cancel(NULL, x_GetSybaseCmd(), CS_CANCEL_ALL));
                DATABASE_DRIVER_ERROR( "command failed", 190007 );
            default:
                break;
            }
            continue;
        }
        case CS_END_RESULTS:
            return nof_bytes;
        default:
            if (Check(ct_cancel(0, x_GetSybaseCmd(), CS_CANCEL_ALL)) != CS_SUCCEED) {
                DATABASE_DRIVER_ERROR( "Unrecoverable crash of ct_result. "
                                   "Connection must be closed", 190002 );
            }
            DATABASE_DRIVER_ERROR( "ct_result failed", 190003 );
        }
    }
}


bool CTL_SendDataCmd::Cancel(void)
{
    if ( m_Bytes2go ) {
        Check(ct_cancel(0, x_GetSybaseCmd(), CS_CANCEL_ALL));
        m_Bytes2go = 0;
        return true;
    }

    return false;
}


CDB_Result*
CTL_SendDataCmd::CreateResult(impl::CResult& result)
{
    return Create_Result(result);
}


CTL_SendDataCmd::~CTL_SendDataCmd()
{
    try {
        DetachInterface();

        Cancel();

        DropCmd(*this);

        Close();
    }
    NCBI_CATCH_ALL( NCBI_CURRENT_FUNCTION )
}


void
CTL_SendDataCmd::Close(void)
{
    if (x_GetSybaseCmd()) {
        if ( m_Bytes2go )
            Check(ct_cancel(0, x_GetSybaseCmd(), CS_CANCEL_ALL));

        // ????
        DetachInterface();

        Check(ct_cmd_drop(x_GetSybaseCmd()));

        SetSybaseCmd(NULL);
    }
}

#ifdef FTDS_IN_USE
END_SCOPE(ftds64_ctlib)
#endif

END_NCBI_SCOPE

