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
* File Description:  Driver for CTLib server
*
*
*/

#include <dbapi/driver/ctlib/interfaces.hpp>
#include <dbapi/driver/util/numeric_convert.hpp>


BEGIN_NCBI_SCOPE


/////////////////////////////////////////////////////////////////////////////
//
//  C_DefCntxErrHandler::
//

class C_DefCntxErrHandler : public CDBUserHandler
{
public:
    virtual bool HandleIt(const CDB_Exception* ex);
};


static C_DefCntxErrHandler s_DefErrHandler;


bool C_DefCntxErrHandler::HandleIt(const CDB_Exception* ex)
{
    if ( !ex ) {
        return false;
    }

    cerr << "Sybase ";

    switch ( ex->Severity() ) {
    case eDB_Info:     cerr << "Info";             break;
    case eDB_Warning:  cerr << "Warning";          break;
    case eDB_Error:    cerr << "Error";            break;
    case eDB_Fatal:    cerr << "Fatal Error";      break;
    default:           cerr << "Unknown severity";
    }

    cerr << " ";

    switch ( ex->Type() ) {
    case CDB_Exception::eDS : {
        const CDB_DSEx& e = dynamic_cast<const CDB_DSEx&> (*ex);
        cerr << "Message (Err.code:" << e.ErrCode() << ") Data source: '"
             << e.OriginatedFrom() << "'\n<<<" << e.Message() << ">>>";
        break;
    }
    case CDB_Exception::eRPC : {
        const CDB_RPCEx& e = dynamic_cast<const CDB_RPCEx&> (*ex);
        cerr << "Message (Err code:" << e.ErrCode()
             << ") SQL/Open server: '" << e.OriginatedFrom()
             << "' Procedure: '" << e.ProcName()
             << "' Line: " << e.ProcLine() << endl
             << "<<<" << e.Message() << ">>>";
        break;
    }
    case CDB_Exception::eSQL : {
        const CDB_SQLEx& e = dynamic_cast<const CDB_SQLEx&> (*ex);
        cerr << "Message (Err.code=" << e.ErrCode()
             << ") SQL server: '" << e.OriginatedFrom()
             << "' SQLstate '" << e.SqlState()
             << "' Line: " << e.BatchLine() << endl
             << " <<<" << e.Message() << ">>>";
        break;
    }
    case CDB_Exception::eDeadlock : {
        const CDB_DeadlockEx& e = dynamic_cast<const CDB_DeadlockEx&> (*ex);
        cerr << "Message: SQL server: " << e.OriginatedFrom()
             << " <" << e.Message() << ">";
        break;
    }
    case CDB_Exception::eTimeout : {
        const CDB_TimeoutEx& e = dynamic_cast<const CDB_TimeoutEx&> (*ex);
        cerr << "Message: SQL server: " << e.OriginatedFrom()
             << " <" << e.Message() << ">";
        break;
    }
    case CDB_Exception::eClient : {
        const CDB_ClientEx& e = dynamic_cast<const CDB_ClientEx&> (*ex);
        cerr << "Message: Err code=" << e.ErrCode()
             <<" Source: " << e.OriginatedFrom() << " <" << e.Message() << ">";
        break;
    }
    case CDB_Exception::eMulti : {
        CDB_MultiEx& e = const_cast<CDB_MultiEx&>
            (dynamic_cast<const CDB_MultiEx&> (*ex));
        while ( s_DefErrHandler.HandleIt(e.Pop()) ) {
            continue;
        }
        break;
    }
    default: {
        cerr << "Message: Error code=" << ex->ErrCode()
             << " <" << ex->what() << ">";
    }
    }

    cerr << endl;
    return true;
}



/////////////////////////////////////////////////////////////////////////////
//
//  CTLibContext::
//


extern "C" {
    CS_RETCODE s_CTLIB_cserr_callback(CS_CONTEXT* context, CS_CLIENTMSG* msg)
    {
        return CTLibContext::CTLIB_cserr_handler(context, msg)
            ? CS_SUCCEED : CS_FAIL;
    }

    CS_RETCODE s_CTLIB_cterr_callback(CS_CONTEXT* context, CS_CONNECTION* con,
                                      CS_CLIENTMSG* msg)
    {
        return CTLibContext::CTLIB_cterr_handler(context, con, msg)
            ? CS_SUCCEED : CS_FAIL;
    }

    CS_RETCODE s_CTLIB_srverr_callback(CS_CONTEXT* context, CS_CONNECTION* con,
                                       CS_SERVERMSG* msg)
    {
        return CTLibContext::CTLIB_srverr_handler(context, con, msg)
            ? CS_SUCCEED : CS_FAIL;
    }

}


CTLibContext::CTLibContext(bool reuse_context, CS_INT version)
{
    m_Context         = 0;
    m_AppName         = "CTLibDriver";
    m_LoginRetryCount = 0;
    m_LoginLoopDelay  = 0;
    m_PacketSize      = 0;

    CS_RETCODE r = reuse_context ? cs_ctx_global(version, &m_Context) :
        cs_ctx_alloc(version, &m_Context);
    if (r != CS_SUCCEED) {
        m_Context = 0;
        throw CDB_ClientEx(eDB_Fatal, 100001, "CTLibContext::CTLibContext",
                           "Can not allocate a context");
    }

    CS_VOID*     cb;
    CS_INT       outlen;
    CPointerPot* p_pot = 0;

    // check if cs message callback is already installed
    r = cs_config(m_Context, CS_GET, CS_MESSAGE_CB, &cb, CS_UNUSED, &outlen);
    if (r != CS_SUCCEED) {
        m_Context = 0;
        throw CDB_ClientEx(eDB_Error, 100006, "CTLibContext::CTLibContext",
                           "cs_config failed");
    }

    if (cb == (CS_VOID*)  s_CTLIB_cserr_callback) {
        // we did use this context already
        r = cs_config(m_Context, CS_GET, CS_USERDATA,
                      (CS_VOID*) &p_pot, (CS_INT) sizeof(p_pot), &outlen);
        if (r != CS_SUCCEED) {
            m_Context = 0;
            throw CDB_ClientEx(eDB_Error, 100006, "CTLibContext::CTLibContext",
                               "cs_config failed");
        }
    }
    else {
        // this is a brand new context
        r = cs_config(m_Context, CS_SET, CS_MESSAGE_CB,
                      (CS_VOID*) s_CTLIB_cserr_callback, CS_UNUSED, NULL);
        if (r != CS_SUCCEED) {
            cs_ctx_drop(m_Context);
            m_Context = 0;
            throw CDB_ClientEx(eDB_Error, 100005, "CTLibContext::CTLibContext",
                               "Can not install the cslib message callback");
        }

        p_pot = new CPointerPot;
        r = cs_config(m_Context, CS_SET, CS_USERDATA,
                      (CS_VOID*) &p_pot, sizeof(p_pot), NULL);
        if (r != CS_SUCCEED) {
            cs_ctx_drop(m_Context);
            m_Context = 0;
            delete p_pot;
            throw CDB_ClientEx(eDB_Error, 100007, "CTLibContext::CTLibContext",
                               "Can not install the user data");
        }

        r = ct_init(m_Context, version);
        if (r != CS_SUCCEED) {
            cs_ctx_drop(m_Context);
            m_Context = 0;
            delete p_pot;	
            throw CDB_ClientEx(eDB_Error, 100002, "CTLibContext::CTLibContext",
                               "ct_init failed");
        }

        r = ct_callback(m_Context, NULL, CS_SET, CS_CLIENTMSG_CB,
                        (CS_VOID*) s_CTLIB_cterr_callback);
        if (r != CS_SUCCEED) {
            ct_exit(m_Context, CS_FORCE_EXIT);
            cs_ctx_drop(m_Context);
            m_Context = 0;
            delete p_pot;	    	
            throw CDB_ClientEx(eDB_Error, 100003, "CTLibContext::CTLibContext",
                               "Can not install the client message callback");
        }

        r = ct_callback(m_Context, NULL, CS_SET, CS_SERVERMSG_CB,
                        (CS_VOID*) s_CTLIB_srverr_callback);
        if (r != CS_SUCCEED) {
            ct_exit(m_Context, CS_FORCE_EXIT);
            cs_ctx_drop(m_Context);
            m_Context = 0;
            delete p_pot;	    	
            throw CDB_ClientEx(eDB_Error, 100004, "CTLibContext::CTLibContext",
                               "Can not install the server message callback");
        }
    }

    if ( p_pot ) {
        p_pot->Add((TPotItem) this);
    }

    PushCntxMsgHandler(&s_DefErrHandler);
    PushDefConnMsgHandler(&s_DefErrHandler);
}


bool CTLibContext::SetLoginTimeout(unsigned int nof_secs)
{
    CS_INT t_out = (CS_INT) nof_secs;
    return ct_config(m_Context, CS_SET,
                     CS_LOGIN_TIMEOUT, &t_out, CS_UNUSED, NULL) == CS_SUCCEED;
}


bool CTLibContext::SetTimeout(unsigned int nof_secs)
{
    CS_INT t_out = (CS_INT) nof_secs;
    return ct_config(m_Context, CS_SET,
                     CS_TIMEOUT, &t_out, CS_UNUSED, NULL) == CS_SUCCEED;
}


bool CTLibContext::SetMaxTextImageSize(size_t nof_bytes)
{
    CS_INT ti_size = (CS_INT) nof_bytes;
    return ct_config(m_Context, CS_SET,
                     CS_TEXTLIMIT, &ti_size, CS_UNUSED, NULL) == CS_SUCCEED;
}


CDB_Connection* CTLibContext::Connect(const string&   srv_name,
                                      const string&   user_name,
                                      const string&   passwd,
                                      TConnectionMode mode,
                                      bool            reusable,
                                      const string&   pool_name)
{
    if (reusable  &&  m_NotInUse.NofItems() > 0) {
        // try to get a connection from the pot
        if ( !pool_name.empty() ) {
            // use a pool name
            for (int i = m_NotInUse.NofItems();  i--; ) {
                CTL_Connection* t_con
                    = static_cast<CTL_Connection*> (m_NotInUse.Get(i));

                if (pool_name.compare(t_con->PoolName()) == 0) {
                    m_NotInUse.Remove(i);
                    m_InUse.Add((TPotItem) t_con);
                    t_con->Refresh();
                    return Create_Connection(*t_con);
                }
            }
        }

        if ( srv_name.empty() )
            return 0;

        // try to use a server name
        for (int i = m_NotInUse.NofItems();  i--; ) {
            CTL_Connection* t_con
                = static_cast<CTL_Connection*> (m_NotInUse.Get(i));

            if (srv_name.compare(t_con->ServerName()) == 0) {
                m_NotInUse.Remove(i);
                m_InUse.Add((TPotItem) t_con);
                t_con->Refresh();
                return Create_Connection(*t_con);
            }
        }
    }

    // new connection needed
    if (srv_name.empty()  ||  user_name.empty()  ||  passwd.empty()) {
        throw CDB_ClientEx(eDB_Error, 100010, "CTLibContext::Connect",
                           "You have to provide server name, user name and "
                           "password to connect to the server");
    }

    CS_CONNECTION* con = x_ConnectToServer(srv_name, user_name, passwd, mode);
    if (con == 0) {
        throw CDB_ClientEx(eDB_Error, 100011, "CTLibContext::Connect",
                           "Can not connect to the server");
    }

    CTL_Connection* t_con = new CTL_Connection(this, con, reusable, pool_name);
    t_con->m_MsgHandlers = m_ConnHandlers;

    m_InUse.Add((TPotItem) t_con);

    return Create_Connection(*t_con);
}


unsigned int CTLibContext::NofConnections(const string& srv_name) const
{
    if ( srv_name.empty() ) {
        return m_InUse.NofItems() + m_NotInUse.NofItems();
    }

    int n = 0;

    for (int i = m_NotInUse.NofItems(); i--;) {
        CTL_Connection* t_con
            = static_cast<CTL_Connection*> (m_NotInUse.Get(i));
        if (srv_name.compare(t_con->ServerName()) == 0)
            ++n;
    }

    for (int i = m_InUse.NofItems(); i--;) {
        CTL_Connection* t_con
            = static_cast<CTL_Connection*> (m_InUse.Get(i));
        if (srv_name.compare(t_con->ServerName()) == 0)
            ++n;
    }

    return n;
}


CTLibContext::~CTLibContext()
{
    if ( !m_Context ) {
        return;
    }

    // close all connections first
    for (int i = m_NotInUse.NofItems();  i--; ) {
        CTL_Connection* t_con = static_cast<CTL_Connection*>(m_NotInUse.Get(i));
        delete t_con;
    }

    for (int i = m_InUse.NofItems();  i--; ) {
        CTL_Connection* t_con = static_cast<CTL_Connection*> (m_InUse.Get(i));
        delete t_con;
    }

    CS_INT       outlen;
    CPointerPot* p_pot = 0;

    if (cs_config(m_Context, CS_GET, CS_USERDATA,
                  (void*) &p_pot, (CS_INT) sizeof(p_pot), &outlen) == CS_SUCCEED
        &&  p_pot != 0) {
        p_pot->Remove(this);
        if (p_pot->NofItems() == 0) { // this is a last driver for this context
            delete p_pot;
            if (ct_exit(m_Context, CS_UNUSED) != CS_SUCCEED) {
                ct_exit(m_Context, CS_FORCE_EXIT);
            }
            cs_ctx_drop(m_Context);
        }
    }
}


bool CTLibContext::CTLIB_cserr_handler(CS_CONTEXT* context, CS_CLIENTMSG* msg)
{
    CS_INT       outlen;
    CPointerPot* p_pot = 0;
    CS_RETCODE   r;

    r = cs_config(context, CS_GET, CS_USERDATA,
                  (void*) &p_pot, (CS_INT) sizeof(p_pot), &outlen);

    if (r == CS_SUCCEED  &&  p_pot != 0  &&  p_pot->NofItems() > 0) {
        CTLibContext* drv = (CTLibContext*) p_pot->Get(0);
        EDBSeverity sev = eDB_Error;
        if (msg->severity == CS_SV_INFORM) {
            sev = eDB_Info;
        }
        else if (msg->severity == CS_SV_FATAL) {
            sev = eDB_Fatal;
        }

        CDB_ClientEx ex(sev, msg->msgnumber, "cslib", msg->msgstring);
        drv->m_CntxHandlers.PostMsg(&ex);
    }
    else if (msg->severity != CS_SV_INFORM) {
        // nobody can be informed, so put it to stderr
        cerr << "CSLIB error handler detects the following error" << endl
             << msg->msgstring << endl;
    }

    return true;
}


bool CTLibContext::CTLIB_cterr_handler(CS_CONTEXT* context, CS_CONNECTION* con,
                                       CS_CLIENTMSG* msg)
{
    CS_INT           outlen;
    CPointerPot*        p_pot = 0;
    CTL_Connection*  link = 0;
    CDBHandlerStack* hs;

    if (con != 0  &&
        ct_con_props(con, CS_GET, CS_USERDATA,
                     (void*) &link, (CS_INT) sizeof(link),
                     &outlen) == CS_SUCCEED  &&  link != 0) {
        hs = &link->m_MsgHandlers;
    }
    else if (cs_config(context, CS_GET, CS_USERDATA,
                       (void*) &p_pot, (CS_INT) sizeof(p_pot),
                       &outlen) == CS_SUCCEED  &&
             p_pot != 0  &&  p_pot->NofItems() > 0) {
        CTLibContext* drv = (CTLibContext*) p_pot->Get(0);
        hs = &drv->m_CntxHandlers;
    }
    else {
        if (msg->severity != CS_SV_INFORM) {
            // nobody can be informed, let's put it in stderr
            cerr << "CTLIB error handler detects the following error" << endl
                 << "Severity:" << msg->severity
                 << " Msg # " << msg->msgnumber << endl
                 << msg->msgstring << endl;
            if (msg->osstringlen > 1) {
                cerr << "OS # "    << msg->osnumber
                     << " OS msg " << msg->osstring << endl;
            }
            if (msg->sqlstatelen > 1  &&
                (msg->sqlstate[0] != 'Z'  ||  msg->sqlstate[1] != 'Z')) {
                cerr << "SQL: " << msg->sqlstate << endl;
            }
        }
        return true;
    }


    switch (msg->severity) {
    case CS_SV_INFORM: {
        CDB_ClientEx info(eDB_Info,
                          (int) msg->msgnumber, "ctlib", msg->msgstring);
        hs->PostMsg(&info);
        break;
    }
    case CS_SV_RETRY_FAIL: {
        CDB_TimeoutEx to((int) msg->msgnumber, "ctlib", msg->msgstring);
        hs->PostMsg(&to);
        break;
    }
    case CS_SV_CONFIG_FAIL:
    case CS_SV_API_FAIL:
    case CS_SV_INTERNAL_FAIL: {
        CDB_ClientEx err(eDB_Error,
                         (int) msg->msgnumber, "ctlib", msg->msgstring);
        hs->PostMsg(&err);
        break;
    }
    default: {
        CDB_ClientEx ftl(eDB_Fatal,
                         (int) msg->msgnumber, "ctlib", msg->msgstring);
        hs->PostMsg(&ftl);
        break;
    }
    }

    return true;
}


bool CTLibContext::CTLIB_srverr_handler(CS_CONTEXT* context, CS_CONNECTION* con,
                                        CS_SERVERMSG* msg)
{
    if ((msg->severity == 0  &&  msg->msgnumber == 0)  ||
        msg->msgnumber == 5701  ||  msg->msgnumber == 5703) {
        return true;
    }

    CS_INT           outlen;
    CPointerPot*     p_pot = 0;
    CTL_Connection*  link = 0;
    CDBHandlerStack* hs;

    if (con != 0  &&  ct_con_props(con, CS_GET, CS_USERDATA,
                                   (void*) &link, (CS_INT) sizeof(link),
                                   &outlen) == CS_SUCCEED  &&
        link != 0) {
        hs = &link->m_MsgHandlers;
    }
    else if (cs_config(context, CS_GET, CS_USERDATA,
                       (void*) &p_pot, (CS_INT) sizeof(p_pot),
                       &outlen) == CS_SUCCEED  &&
             p_pot != 0  &&  p_pot->NofItems() > 0) {
        CTLibContext* drv = (CTLibContext*) p_pot->Get(0);
        hs = &drv->m_CntxHandlers;
    }
    else {
        cerr << "Message from the server ";
        if (msg->svrnlen > 0) {
            cerr << "<" << msg->svrname << "> ";
        }
        cerr << "msg # " << msg->msgnumber
             << " severity: " << msg->severity << endl;
        if (msg->proclen > 0) {
            cerr << "Proc: " << msg->proc << " line: " << msg->line << endl;
        }
        if (msg->sqlstatelen > 1  &&
            (msg->sqlstate[0] != 'Z'  ||  msg->sqlstate[1] != 'Z')) {
            cerr << "SQL: " << msg->sqlstate << endl;
        }
        cerr << msg->text << endl;
        return true;
    }

    if (msg->msgnumber == 1205 /*DEADLOCK*/) {
        CDB_DeadlockEx dl(msg->svrname, msg->text);
        hs->PostMsg(&dl);
    }
    else {
        EDBSeverity sev =
            (msg->severity < 10) ? eDB_Info :
            (msg->severity = 10) ? eDB_Warning :
            (msg->severity < 16) ? eDB_Error :
            (msg->severity > 16) ? eDB_Fatal :
            eDB_Unknown;

        if (msg->proclen > 0) {
            CDB_RPCEx rpc(sev, (int) msg->msgnumber, msg->svrname, msg->text,
                          msg->proc, (int) msg->line);
            hs->PostMsg(&rpc);
        }
        else if (msg->sqlstatelen > 1  &&
                 (msg->sqlstate[0] != 'Z'  ||  msg->sqlstate[1] != 'Z')) {
            CDB_SQLEx sql(sev, (int) msg->msgnumber, msg->svrname, msg->text,
                          (const char*) msg->sqlstate, (int) msg->line);
            hs->PostMsg(&sql);
        }
        else {
            CDB_DSEx m(sev, (int) msg->msgnumber, msg->svrname, msg->text);
            hs->PostMsg(&m);
        }
    }

    return true;
}



CS_CONNECTION* CTLibContext::x_ConnectToServer(const string&   srv_name,
                                               const string&   user_name,
                                               const string&   passwd,
                                               TConnectionMode mode)
{
    CS_CONNECTION* con;
    if (ct_con_alloc(m_Context, &con) != CS_SUCCEED)
        return 0;

    if (ct_con_props(con, CS_SET, CS_USERNAME, (void*) user_name.c_str(),
                     CS_NULLTERM, NULL) != CS_SUCCEED  ||
        ct_con_props(con, CS_SET, CS_PASSWORD, (void*) passwd.c_str(),
                     CS_NULLTERM, NULL) != CS_SUCCEED  ||
        ct_con_props(con, CS_SET, CS_APPNAME, (void*) m_AppName.c_str(),
                     CS_NULLTERM, NULL) != CS_SUCCEED) {
        ct_con_drop(con);
        return 0;
    }

    if ( !m_HostName.empty() ) {
        ct_con_props(con, CS_SET, CS_HOSTNAME,
                     (void*) m_HostName.c_str(), CS_NULLTERM, NULL);
    }

    if (m_PacketSize > 0) {
        ct_con_props(con, CS_SET, CS_PACKETSIZE,
                     (void*) &m_PacketSize, CS_UNUSED, NULL);
    }

    if (m_LoginRetryCount > 0) {
        ct_con_props(con, CS_SET, CS_RETRY_COUNT,
                     (void*) &m_LoginRetryCount, CS_UNUSED, NULL);
    }

    if (m_LoginLoopDelay > 0) {
        ct_con_props(con, CS_SET, CS_LOOP_DELAY,
                     (void*) &m_LoginLoopDelay, CS_UNUSED, NULL);
    }

    CS_BOOL flag = CS_TRUE;
    if ((mode & fBcpIn) != 0) {
        ct_con_props(con, CS_SET, CS_BULK_LOGIN, &flag, CS_UNUSED, NULL);
    }
    if ((mode & fPasswordEncrypted) != 0) {
        ct_con_props(con, CS_SET, CS_SEC_ENCRYPTION, &flag, CS_UNUSED, NULL);
    }

    if (ct_connect(con, const_cast<char*> (srv_name.c_str()), CS_NULLTERM)
        != CS_SUCCEED) {
        ct_con_drop(con);
        return 0;
    }

    return con;
}




/////////////////////////////////////////////////////////////////////////////
//
//  Miscellaneous
//

void g_CTLIB_GetRowCount(CS_COMMAND* cmd, int* cnt)
{
    CS_INT n;
    CS_INT outlen;
    if (cnt  &&
        ct_res_info(cmd, CS_ROW_COUNT, &n, CS_UNUSED, &outlen) == CS_SUCCEED  &&
        n >= 0  &&  n != CS_NO_COUNT)
        {
            *cnt = (int) n;
        }
}


bool g_CTLIB_AssignCmdParam(CS_COMMAND*   cmd,
                            CDB_Object&   param,
                            const string& param_name,
                            CS_DATAFMT&   param_fmt,
                            CS_SMALLINT   indicator,
                            bool          declare_only)
{
    {{
        size_t l = param_name.copy(param_fmt.name, CS_MAX_NAME-1);
        param_fmt.name[l] = '\0';
    }}


    CS_RETCODE ret_code;

    switch ( param.GetType() ) {
    case eDB_Int: {
        CDB_Int& par = dynamic_cast<CDB_Int&> (param);
        param_fmt.datatype = CS_INT_TYPE;
        if ( declare_only )
            break;

        CS_INT value = (CS_INT) par.Value();
        ret_code = ct_param(cmd, &param_fmt,
                            (CS_VOID*) &value, CS_UNUSED, indicator);
        break;
    }
    case eDB_SmallInt: {
        CDB_SmallInt& par = dynamic_cast<CDB_SmallInt&> (param);
        param_fmt.datatype = CS_SMALLINT_TYPE;
        if ( declare_only )
            break;

        CS_SMALLINT value = (CS_SMALLINT) par.Value();
        ret_code = ct_param(cmd, &param_fmt,
                            (CS_VOID*) &value, CS_UNUSED, indicator);
        break;
    }
    case eDB_TinyInt: {
        CDB_TinyInt& par = dynamic_cast<CDB_TinyInt&> (param);
        param_fmt.datatype = CS_TINYINT_TYPE;
        if ( declare_only )
            break;

        CS_TINYINT value = (CS_TINYINT) par.Value();
        ret_code = ct_param(cmd, &param_fmt,
                            (CS_VOID*) &value, CS_UNUSED, indicator);
        break;
    }
    case eDB_BigInt: {
        CDB_BigInt& par = dynamic_cast<CDB_BigInt&> (param);
        param_fmt.datatype = CS_NUMERIC_TYPE;
        if ( declare_only )
            break;

        CS_NUMERIC value;
        Int8 v8 = par.Value();
        if (longlong_to_numeric(v8, 18, 0, &value) == 0)
            return false;

        ret_code = ct_param(cmd, &param_fmt,
                            (CS_VOID*) &value, CS_UNUSED, indicator);
        break;
    }
    case eDB_Char: {
        CDB_Char& par = dynamic_cast<CDB_Char&> (param);
        param_fmt.datatype = CS_CHAR_TYPE;
        if ( declare_only )
            break;

        ret_code = ct_param(cmd, &param_fmt, (CS_VOID*) par.Value(),
                            (CS_INT) par.Size(), indicator);
        break;
    }
    case eDB_VarChar: {
        CDB_VarChar& par = dynamic_cast<CDB_VarChar&> (param);
        param_fmt.datatype = CS_CHAR_TYPE;
        if ( declare_only )
            break;

        ret_code = ct_param(cmd, &param_fmt, (CS_VOID*) par.Value(),
                            (CS_INT) par.Size(), indicator);
        break;
    }
    case eDB_Binary: {
        CDB_Binary& par = dynamic_cast<CDB_Binary&> (param);
        param_fmt.datatype = CS_BINARY_TYPE;
        if ( declare_only )
            break;

        ret_code = ct_param(cmd, &param_fmt, (CS_VOID*) par.Value(),
                            (CS_INT) par.Size(), indicator);
        break;
    }
    case eDB_VarBinary: {
        CDB_VarBinary& par = dynamic_cast<CDB_VarBinary&> (param);
        param_fmt.datatype = CS_BINARY_TYPE;
        if ( declare_only )
            break;

        ret_code = ct_param(cmd, &param_fmt, (CS_VOID*) par.Value(),
                            (CS_INT) par.Size(), indicator);
        break;
    }
    case eDB_Float: {
        CDB_Float& par = dynamic_cast<CDB_Float&> (param);
        param_fmt.datatype = CS_REAL_TYPE;
        if ( declare_only )
            break;

        CS_REAL value = (CS_REAL) par.Value();
        ret_code = ct_param(cmd, &param_fmt,
                            (CS_VOID*) &value, CS_UNUSED, indicator);
        break;
    }
    case eDB_Double: {
        CDB_Double& par = dynamic_cast<CDB_Double&> (param);
        param_fmt.datatype = CS_FLOAT_TYPE;
        if ( declare_only )
            break;

        CS_FLOAT value = (CS_FLOAT) par.Value();
        ret_code = ct_param(cmd, &param_fmt,
                            (CS_VOID*) &value, CS_UNUSED, indicator);
        break;
    }
    case eDB_SmallDateTime: {
        CDB_SmallDateTime& par = dynamic_cast<CDB_SmallDateTime&> (param);
        param_fmt.datatype = CS_DATETIME4_TYPE;
        if ( declare_only )
            break;

        CS_DATETIME4 dt;
        dt.days    = par.GetDays();
        dt.minutes = par.GetMinutes();
        ret_code = ct_param(cmd, &param_fmt,
                            (CS_VOID*) &dt, CS_UNUSED, indicator);
        break;
    }
    case eDB_DateTime: {
        CDB_DateTime& par = dynamic_cast<CDB_DateTime&> (param);
        param_fmt.datatype = CS_DATETIME_TYPE;
        if ( declare_only )
            break;

        CS_DATETIME dt;
        dt.dtdays = par.GetDays();
        dt.dttime = par.Get300Secs();
        ret_code = ct_param(cmd, &param_fmt,
                            (CS_VOID*) &dt, CS_UNUSED, indicator);
        break;
    }
    default: {
        return false;
    }
    }

    if ( declare_only ) {
        ret_code = ct_param(cmd, &param_fmt, 0, CS_UNUSED, 0);
    }

    return (ret_code == CS_SUCCEED);
}


END_NCBI_SCOPE



/*
 * ===========================================================================
 * $Log$
 * Revision 1.2  2001/09/26 23:23:31  vakatov
 * Moved the err.message handlers' stack functionality (generic storage
 * and methods) to the "abstract interface" level.
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
