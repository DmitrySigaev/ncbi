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
 * File Description:  Driver for CTLib client library
 *
 */

#include <ncbi_pch.hpp>

#include "ctlib_utils.hpp"

#include <corelib/plugin_manager_impl.hpp>
#include <corelib/plugin_manager_store.hpp>
#include <corelib/ncbi_param.hpp>

// DO NOT DELETE this include !!!
#include <dbapi/driver/driver_mgr.hpp>

#include <dbapi/driver/ctlib/interfaces.hpp>
#include <dbapi/driver/util/pointer_pot.hpp>

#include <algorithm>

#if defined(NCBI_OS_MSWIN)
#  include <winsock2.h>
#  include "../ncbi_win_hook.hpp"
#else
#  include <unistd.h>
#endif


BEGIN_NCBI_SCOPE

#ifdef FTDS_IN_USE
BEGIN_SCOPE(ftds64_ctlib)
#endif

static const CDiagCompileInfo kBlankCompileInfo;


/////////////////////////////////////////////////////////////////////////////
//
//  CTLibContextRegistry (Singleton)
//

class NCBI_DBAPIDRIVER_CTLIB_EXPORT CTLibContextRegistry
{
public:
    static CTLibContextRegistry& Instance(void);

    void Add(CTLibContext* ctx);
    void Remove(CTLibContext* ctx);
    void ClearAll(void);
    static void StaticClearAll(void);

    bool ExitProcessIsPatched(void) const
    {
        return m_ExitProcessPatched;
    }

private:
    CTLibContextRegistry(void);
    ~CTLibContextRegistry(void) throw();

    mutable CMutex          m_Mutex;
    vector<CTLibContext*>   m_Registry;
    bool                    m_ExitProcessPatched;

    friend class CSafeStaticPtr<CTLibContextRegistry>;
};


/////////////////////////////////////////////////////////////////////////////
CTLibContextRegistry::CTLibContextRegistry(void) :
m_ExitProcessPatched(false)
{
#if defined(NCBI_OS_MSWIN)

    try {
        NWinHook::COnExitProcess::Instance().Add(CTLibContextRegistry::StaticClearAll);
        m_ExitProcessPatched = true;
    } catch (const NWinHook::CWinHookException&) {
        // Just in case ...
        m_ExitProcessPatched = false;
    }

#endif
}

CTLibContextRegistry::~CTLibContextRegistry(void) throw()
{
    try {
        ClearAll();
    }
    NCBI_CATCH_ALL( NCBI_CURRENT_FUNCTION )
}

CTLibContextRegistry&
CTLibContextRegistry::Instance(void)
{
    static CSafeStaticPtr<CTLibContextRegistry> instance;

    return instance.Get();
}

void
CTLibContextRegistry::Add(CTLibContext* ctx)
{
    CMutexGuard mg(m_Mutex);

    vector<CTLibContext*>::iterator it = find(m_Registry.begin(),
                                              m_Registry.end(),
                                              ctx);
    if (it == m_Registry.end()) {
        m_Registry.push_back(ctx);
    }
}

void
CTLibContextRegistry::Remove(CTLibContext* ctx)
{
    CMutexGuard mg(m_Mutex);

    vector<CTLibContext*>::iterator it = find(m_Registry.begin(),
                                              m_Registry.end(),
                                              ctx);

    if (it != m_Registry.end()) {
        m_Registry.erase(it);
        ctx->x_SetRegistry(NULL);
    }
}


void
CTLibContextRegistry::ClearAll(void)
{
    if (!m_Registry.empty())
    {
        CMutexGuard mg(m_Mutex);

        while ( !m_Registry.empty() ) {
            // x_Close will unregister and remove handler from the registry.
            m_Registry.back()->x_Close(false);
        }
    }
}

void
CTLibContextRegistry::StaticClearAll(void)
{
    CTLibContextRegistry::Instance().ClearAll();
}


/////////////////////////////////////////////////////////////////////////////
BEGIN_SCOPE(ctlib)

Connection::Connection(CTLibContext& context,
                       CTL_Connection* ctl_conn) :
    m_CTL_Context(&context),
    m_CTL_Conn(ctl_conn),
    m_Handle(NULL),
    m_IsAllocated(false),
    m_IsOpen(false)
{
    if (GetCTLContext().Check(ct_con_alloc(GetCTLContext().CTLIB_GetContext(),
                                   &m_Handle)) != CS_SUCCEED) {
        DATABASE_DRIVER_ERROR( "Cannot allocate a connection handle.", 100011 );
    }
    m_IsAllocated = true;
}


Connection::~Connection(void) throw()
{
    try {
        Drop();
    }
    NCBI_CATCH_ALL( NCBI_CURRENT_FUNCTION )
}


bool Connection::Drop(void)
{
    if (m_IsAllocated) {
        GetCTLConn().Check(ct_con_drop(m_Handle));
        m_IsAllocated = false;
    }

    return !m_IsAllocated;
}


const CTL_Connection&
Connection::GetCTLConn(void) const
{
    if (!m_CTL_Conn) {
        DATABASE_DRIVER_ERROR( "CTL_Connection wasn't assigned.", 100011 );
    }

    return *m_CTL_Conn;
}


CTL_Connection&
Connection::GetCTLConn(void)
{
    if (!m_CTL_Conn) {
        DATABASE_DRIVER_ERROR( "CTL_Connection wasn't assigned.", 100011 );
    }

    return *m_CTL_Conn;
}


bool Connection::Open(const string& srv_name)
{
    if (!IsOpen() || Close()) {
        CS_RETCODE rc;

        rc = GetCTLContext().Check(ct_connect(GetNativeHandle(),
                                const_cast<char*> (srv_name.c_str()),
                                CS_NULLTERM));

        if (rc == CS_SUCCEED) {
            m_IsOpen = true;
        }
    }

    return IsOpen();
}


bool Connection::Close(void)
{
    if (IsOpen()) {
        if (GetCTLConn().Check(ct_close(GetNativeHandle(), CS_UNUSED)) == CS_SUCCEED ||
            GetCTLConn().Check(ct_close(GetNativeHandle(), CS_FORCE_CLOSE)) == CS_SUCCEED) {
            m_IsOpen = false;
        }
    }

    return !IsOpen();
}


END_SCOPE(ctlib)


/////////////////////////////////////////////////////////////////////////////
//
//  CTLibContext::
//

CTLibContext::CTLibContext(bool reuse_context, CS_INT version) :
    m_Context(NULL),
    m_Locale(NULL),
    m_PacketSize(2048),
    m_LoginRetryCount(0),
    m_LoginLoopDelay(0),
    m_TDSVersion(version),
    m_Registry(NULL)
{
    DEFINE_STATIC_FAST_MUTEX(xMutex);
    CFastMutexGuard mg(xMutex);

    SetApplicationName("CTLibDriver");

    CS_RETCODE r = reuse_context ? Check(cs_ctx_global(version, &m_Context)) :
        Check(cs_ctx_alloc(version, &m_Context));
    if (r != CS_SUCCEED) {
        m_Context = 0;
        DATABASE_DRIVER_ERROR( "Cannot allocate a context", 100001 );
    }


    r = cs_loc_alloc(CTLIB_GetContext(), &m_Locale);
    if (r != CS_SUCCEED) {
        m_Locale = NULL;
    }

    CS_VOID*     cb;
    CS_INT       outlen;
    CPointerPot* p_pot = 0;

    // check if cs message callback is already installed
    r = Check(cs_config(CTLIB_GetContext(), CS_GET, CS_MESSAGE_CB, &cb, CS_UNUSED, &outlen));
    if (r != CS_SUCCEED) {
        m_Context = 0;
        DATABASE_DRIVER_ERROR( "cs_config failed", 100006 );
    }

    if (cb == (CS_VOID*)  CTLIB_cserr_handler) {
        // we did use this context already
        r = Check(cs_config(CTLIB_GetContext(), CS_GET, CS_USERDATA,
                      (CS_VOID*) &p_pot, (CS_INT) sizeof(p_pot), &outlen));
        if (r != CS_SUCCEED) {
            m_Context = 0;
            DATABASE_DRIVER_ERROR( "cs_config failed", 100006 );
        }
    }
    else {
        // this is a brand new context
        r = Check(cs_config(CTLIB_GetContext(), CS_SET, CS_MESSAGE_CB,
                      (CS_VOID*) CTLIB_cserr_handler, CS_UNUSED, NULL));
        if (r != CS_SUCCEED) {
            Check(cs_ctx_drop(CTLIB_GetContext()));
            m_Context = 0;
            DATABASE_DRIVER_ERROR( "Cannot install the cslib message callback", 100005 );
        }

        p_pot = new CPointerPot;
        r = Check(cs_config(CTLIB_GetContext(), CS_SET, CS_USERDATA,
                      (CS_VOID*) &p_pot, (CS_INT) sizeof(p_pot), NULL));
        if (r != CS_SUCCEED) {
            Check(cs_ctx_drop(CTLIB_GetContext()));
            m_Context = 0;
            delete p_pot;
            DATABASE_DRIVER_ERROR( "Cannot install the user data", 100007 );
        }

        r = Check(ct_init(CTLIB_GetContext(), version));
        if (r != CS_SUCCEED) {
            Check(cs_ctx_drop(CTLIB_GetContext()));
            m_Context = 0;
            delete p_pot;
            DATABASE_DRIVER_ERROR( "ct_init failed", 100002 );
        }

        r = Check(ct_callback(CTLIB_GetContext(), NULL, CS_SET, CS_CLIENTMSG_CB,
                        (CS_VOID*) CTLIB_cterr_handler));
        if (r != CS_SUCCEED) {
            Check(ct_exit(CTLIB_GetContext(), CS_FORCE_EXIT));
            Check(cs_ctx_drop(CTLIB_GetContext()));
            m_Context = 0;
            delete p_pot;
            DATABASE_DRIVER_ERROR( "Cannot install the client message callback", 100003 );
        }

        r = Check(ct_callback(CTLIB_GetContext(), NULL, CS_SET, CS_SERVERMSG_CB,
                        (CS_VOID*) CTLIB_srverr_handler));
        if (r != CS_SUCCEED) {
            Check(ct_exit(CTLIB_GetContext(), CS_FORCE_EXIT));
            Check(cs_ctx_drop(CTLIB_GetContext()));
            m_Context = 0;
            delete p_pot;
            DATABASE_DRIVER_ERROR( "Cannot install the server message callback", 100004 );
        }
        // PushCntxMsgHandler();
    }

    if ( p_pot ) {
        p_pot->Add((TPotItem) this);
    }

    m_Registry = &CTLibContextRegistry::Instance();
    x_AddToRegistry();
}


CTLibContext::~CTLibContext()
{
    try {
        x_Close();

        if (m_Locale) {
            cs_loc_drop(CTLIB_GetContext(), m_Locale);
            m_Locale = NULL;
        }
    }
    NCBI_CATCH_ALL( NCBI_CURRENT_FUNCTION )
}


CS_RETCODE
CTLibContext::Check(CS_RETCODE rc) const
{
    // We need const_cast here because of someone's idea of non-const argument
    // of Handle()
    GetCTLExceptionStorage().Handle(const_cast<CDBHandlerStack&>(GetCtxHandlerStack()));

    return rc;
}


void
CTLibContext::x_AddToRegistry(void)
{
    if (m_Registry) {
        m_Registry->Add(this);
    }
}

void
CTLibContext::x_RemoveFromRegistry(void)
{
    if (m_Registry) {
        m_Registry->Remove(this);
    }
}

void
CTLibContext::x_SetRegistry(CTLibContextRegistry* registry)
{
    m_Registry = registry;
}

bool CTLibContext::SetLoginTimeout(unsigned int nof_secs)
{
    CMutexGuard mg(m_CtxMtx);

    I_DriverContext::SetLoginTimeout(nof_secs);

    CS_INT t_out = (CS_INT) GetLoginTimeout();
    t_out = (t_out == 0 ? CS_NO_LIMIT : t_out);

    return Check(ct_config(CTLIB_GetContext(),
                           CS_SET,
                           CS_LOGIN_TIMEOUT,
                           &t_out,
                           CS_UNUSED,
                           NULL)) == CS_SUCCEED;
}


bool CTLibContext::SetTimeout(unsigned int nof_secs)
{
    if (impl::CDriverContext::SetTimeout(nof_secs)) {
        CMutexGuard mg(m_CtxMtx);

        CS_INT t_out = (CS_INT) GetTimeout();
        t_out = (t_out == 0 ? CS_NO_LIMIT : t_out);

        return Check(ct_config(CTLIB_GetContext(),
                               CS_SET,
                               CS_TIMEOUT,
                               &t_out,
                               CS_UNUSED,
                               NULL)) == CS_SUCCEED;
    }

    return false;
}


bool CTLibContext::SetMaxTextImageSize(size_t nof_bytes)
{
    impl::CDriverContext::SetMaxTextImageSize(nof_bytes);

    CMutexGuard mg(m_CtxMtx);

    CS_INT ti_size = (CS_INT) GetMaxTextImageSize();
    return Check(ct_config(CTLIB_GetContext(),
                           CS_SET,
                           CS_TEXTLIMIT,
                           &ti_size,
                           CS_UNUSED,
                           NULL)) == CS_SUCCEED;
}


unsigned int
CTLibContext::GetLoginTimeout(void) const
{
    CS_INT t_out = 0;

    if (Check(ct_config(CTLIB_GetContext(),
                           CS_GET,
                           CS_LOGIN_TIMEOUT,
                           &t_out,
                           CS_UNUSED,
                           NULL)) == CS_SUCCEED) {
        return t_out;
    }

    return I_DriverContext::GetLoginTimeout();
}


unsigned int CTLibContext::GetTimeout(void) const
{
    CS_INT t_out = 0;

    if (Check(ct_config(CTLIB_GetContext(),
                        CS_GET,
                        CS_TIMEOUT,
                        &t_out,
                        CS_UNUSED,
                        NULL)) == CS_SUCCEED) {
        return t_out;
    }

    return I_DriverContext::GetTimeout();
}


impl::CConnection*
CTLibContext::MakeIConnection(const SConnAttr& conn_attr)
{
    return new CTL_Connection(*this, conn_attr);
}


bool CTLibContext::IsAbleTo(ECapability cpb) const
{
    switch(cpb) {
    case eBcp:
    case eReturnITDescriptors:
    case eReturnComputeResults:
        return true;
    default:
        break;
    }

    return false;
}


void
CTLibContext::x_Close(bool delete_conn)
{
    CMutexGuard mg(m_CtxMtx);

    if ( CTLIB_GetContext() ) {
        if (x_SafeToFinalize()) {
            if (delete_conn) {
                DeleteAllConn();
            } else {
                CloseAllConn();
            }
        }

        CS_INT       outlen;
        CPointerPot* p_pot = 0;

        if (Check(cs_config(CTLIB_GetContext(),
                        CS_GET,
                        CS_USERDATA,
                        (void*) &p_pot,
                        (CS_INT) sizeof(p_pot),
                        &outlen)) == CS_SUCCEED
            &&  p_pot != 0) {
            p_pot->Remove(this);
            if (p_pot->NofItems() == 0) {
                // this is a last driver for this context
                delete p_pot;

                if (x_SafeToFinalize()) {
                    if (Check(ct_exit(CTLIB_GetContext(),
                                        CS_UNUSED)) != CS_SUCCEED) {
                        Check(ct_exit(CTLIB_GetContext(),
                                        CS_FORCE_EXIT));
                    }
                    Check(cs_ctx_drop(CTLIB_GetContext()));
                }
            }
        }

        m_Context = NULL;
        x_RemoveFromRegistry();
    } else {
        if (delete_conn && x_SafeToFinalize()) {
            DeleteAllConn();
        }
    }
}

bool CTLibContext::x_SafeToFinalize(void) const
{
    if (m_Registry) {
#if defined(NCBI_OS_MSWIN)
        return m_Registry->ExitProcessIsPatched();
#endif
    }

    return true;
}

void CTLibContext::CTLIB_SetApplicationName(const string& a_name)
{
    SetApplicationName( a_name );
}


void CTLibContext::CTLIB_SetHostName(const string& host_name)
{
    SetHostName( host_name );
}


void CTLibContext::CTLIB_SetPacketSize(CS_INT packet_size)
{
    m_PacketSize = packet_size;
}


void CTLibContext::CTLIB_SetLoginRetryCount(CS_INT n)
{
    m_LoginRetryCount = n;
}


void CTLibContext::CTLIB_SetLoginLoopDelay(CS_INT nof_sec)
{
    m_LoginLoopDelay = nof_sec;
}


CS_CONTEXT* CTLibContext::CTLIB_GetContext() const
{
    return m_Context;
}


CS_RETCODE CTLibContext::CTLIB_cserr_handler(CS_CONTEXT* context, CS_CLIENTMSG* msg)
{
    EDiagSev sev = eDiag_Error;

    if (msg->severity == CS_SV_INFORM) {
        sev = eDiag_Info;
    }
    else if (msg->severity == CS_SV_FATAL) {
        sev = eDiag_Critical;
    }

    CDB_ClientEx ex(
        kBlankCompileInfo,
        0, msg->msgstring,
        sev,
        msg->msgnumber
        );

    ex.SetSybaseSeverity(msg->severity);

    GetCTLExceptionStorage().Accept(ex);

    return CS_SUCCEED;
}


CS_RETCODE CTLibContext::CTLIB_cterr_handler(CS_CONTEXT* context, CS_CONNECTION* con,
                                       CS_CLIENTMSG* msg)
{
    CS_INT          outlen;
    CPointerPot*    p_pot = 0;
    CTL_Connection* link = 0;
    string          message;
    string          server_name;
    string          user_name;

    if ( msg->msgstring ) {
        message.append( msg->msgstring );
    }

    // Retrieve CDBHandlerStack ...
    if (con != 0  &&
        ct_con_props(con,
                     CS_GET,
                     CS_USERDATA,
                     (void*) &link,
                     (CS_INT) sizeof(link),
                     &outlen ) == CS_SUCCEED  &&  link != 0) {
        server_name = link->ServerName();
        user_name = link->UserName();
    }
    else if (cs_config(context,
                       CS_GET,
                       CS_USERDATA,
                       (void*) &p_pot,
                       (CS_INT) sizeof(p_pot),
                       &outlen ) == CS_SUCCEED  &&
             p_pot != 0  &&  p_pot->NofItems() > 0) {
        // CTLibContext* drv = (CTLibContext*) p_pot->Get(0);
    }
    else {
        if (msg->severity != CS_SV_INFORM) {
            ostrstream err_str;

            // nobody can be informed, let's put it in stderr
            err_str << "CTLIB error handler detects the following error" << endl
                    << "Severity:" << msg->severity << err_str << " Msg # "
                    << msg->msgnumber << endl;
            err_str << msg->msgstring << endl;

            if (msg->osstringlen > 1) {
                err_str << "OS # "    << msg->osnumber
                        << " OS msg " << msg->osstring << endl;
            }

            if (msg->sqlstatelen > 1  &&
                (msg->sqlstate[0] != 'Z'  ||  msg->sqlstate[1] != 'Z')) {
                err_str << "SQL: " << msg->sqlstate << endl;
            }

            ERR_POST((string)CNcbiOstrstreamToString(err_str));
        }

        return CS_SUCCEED;
    }

    // Process the message ...
    switch (msg->severity) {
    case CS_SV_INFORM: {
        CDB_ClientEx ex( kBlankCompileInfo,
                           0,
                           message,
                           eDiag_Info,
                           msg->msgnumber);

        ex.SetServerName(server_name);
        ex.SetUserName(user_name);
        ex.SetSybaseSeverity(msg->severity);

        GetCTLExceptionStorage().Accept(ex);

        break;
    }
    case CS_SV_RETRY_FAIL: {
        CDB_TimeoutEx ex(
            kBlankCompileInfo,
            0,
            message,
            msg->msgnumber);

        ex.SetServerName(server_name);
        ex.SetUserName(user_name);
        ex.SetSybaseSeverity(msg->severity);

        GetCTLExceptionStorage().Accept(ex);

        if( con ) {
            CS_INT status;
            if((ct_con_props(con,
                             CS_GET,
                             CS_LOGIN_STATUS,
                             (CS_VOID*)&status,
                             CS_UNUSED,
                             NULL) != CS_SUCCEED) ||
                (!status)) {
                return CS_FAIL;
            }

            if(ct_cancel(con, (CS_COMMAND*)0, CS_CANCEL_ATTN) != CS_SUCCEED) {
                return CS_FAIL;
            }
        }
        else {
            return CS_FAIL;
        }

        break;
    }
    case CS_SV_CONFIG_FAIL:
    case CS_SV_API_FAIL:
    case CS_SV_INTERNAL_FAIL: {
        CDB_ClientEx ex( kBlankCompileInfo,
                          0,
                          message,
                          eDiag_Error,
                          msg->msgnumber);

        ex.SetServerName(server_name);
        ex.SetUserName(user_name);
        ex.SetSybaseSeverity(msg->severity);

        GetCTLExceptionStorage().Accept(ex);

        break;
    }
    default: {
        CDB_ClientEx ex(
            kBlankCompileInfo,
            0,
            message,
            eDiag_Critical,
            msg->msgnumber);

        ex.SetServerName(server_name);
        ex.SetUserName(user_name);
        ex.SetSybaseSeverity(msg->severity);

        GetCTLExceptionStorage().Accept(ex);

        break;
    }
    }

    return CS_SUCCEED;
}


CS_RETCODE CTLibContext::CTLIB_srverr_handler(CS_CONTEXT* context,
                                        CS_CONNECTION* con,
                                        CS_SERVERMSG* msg)
{
    if (
        /* (msg->severity == 0  &&  msg->msgnumber == 0)  ||*/
        // commented out because nobody remember why it is there and PubSeqOS does
        // send messages with 0 0 that need to be processed
        msg->msgnumber == 5701 ||
        msg->msgnumber == 5703 ||
        msg->msgnumber == 5704) {
        return CS_SUCCEED;
    }

    CS_INT          outlen;
    CPointerPot*    p_pot = 0;
    CTL_Connection* link = 0;
    string          message;
    string          server_name;
    string          user_name;

    if (con != 0  &&  ct_con_props(con, CS_GET, CS_USERDATA,
                                   (void*) &link, (CS_INT) sizeof(link),
                                   &outlen) == CS_SUCCEED  &&
        link != 0) {
        server_name = link->ServerName();
        user_name = link->UserName();
    }
    else if (cs_config(context, CS_GET,
                       CS_USERDATA,
                       (void*) &p_pot,
                       (CS_INT) sizeof(p_pot),
                       &outlen) == CS_SUCCEED  &&
             p_pot != 0  &&  p_pot->NofItems() > 0) {

        // CTLibContext* drv = (CTLibContext*) p_pot->Get(0);
        server_name = string(msg->svrname, msg->svrnlen);
    }
    else {
        ostrstream err_str;

        err_str << "Message from the server ";

        if (msg->svrnlen > 0) {
            err_str << "<" << msg->svrname << "> ";
        }

        err_str << "msg # " << msg->msgnumber
                << " severity: " << msg->severity << endl;

        if (msg->proclen > 0) {
            err_str << "Proc: " << msg->proc << " line: " << msg->line << endl;
        }

        if (msg->sqlstatelen > 1  &&
            (msg->sqlstate[0] != 'Z'  ||  msg->sqlstate[1] != 'Z')) {
            err_str << "SQL: " << msg->sqlstate << endl;
        }

        err_str << msg->text << endl;

        ERR_POST((string)CNcbiOstrstreamToString(err_str));

        return CS_SUCCEED;
    }

    if ( msg->text ) {
        message += msg->text;
    }

    if (msg->msgnumber == 1205 /*DEADLOCK*/) {
        CDB_DeadlockEx ex(kBlankCompileInfo,
                          0,
                          message);

        ex.SetServerName(server_name);
        ex.SetUserName(user_name);
        ex.SetSybaseSeverity(msg->severity);

        GetCTLExceptionStorage().Accept(ex);
    }
    else {
        EDiagSev sev =
            msg->severity <  10 ? eDiag_Info :
            msg->severity == 10 ? (msg->msgnumber == 0 ? eDiag_Info : eDiag_Warning) :
            msg->severity <  16 ? eDiag_Error : eDiag_Critical;

        if (msg->proclen > 0) {
            CDB_RPCEx ex(kBlankCompileInfo,
                          0,
                          message,
                          sev,
                          (int) msg->msgnumber,
                          msg->proc,
                          (int) msg->line);

            ex.SetServerName(server_name);
            ex.SetUserName(user_name);
            ex.SetSybaseSeverity(msg->severity);

            GetCTLExceptionStorage().Accept(ex);
        }
        else if (msg->sqlstatelen > 1  &&
                 (msg->sqlstate[0] != 'Z'  ||  msg->sqlstate[1] != 'Z')) {
            CDB_SQLEx ex(kBlankCompileInfo,
                          0,
                          message,
                          sev,
                          (int) msg->msgnumber,
                          (const char*) msg->sqlstate,
                          (int) msg->line);

            ex.SetServerName(server_name);
            ex.SetUserName(user_name);
            ex.SetSybaseSeverity(msg->severity);

            GetCTLExceptionStorage().Accept(ex);
        }
        else {
            CDB_DSEx ex(kBlankCompileInfo,
                        0,
                        message,
                        sev,
                        (int) msg->msgnumber);

            ex.SetServerName(server_name);
            ex.SetUserName(user_name);
            ex.SetSybaseSeverity(msg->severity);

            GetCTLExceptionStorage().Accept(ex);
        }
    }

    return CS_SUCCEED;
}


void CTLibContext::SetClientCharset(const string& charset)
{
    impl::CDriverContext::SetClientCharset(charset);

    if ( !GetClientCharset().empty() ) {
        cs_locale(CTLIB_GetContext(),
                  CS_SET,
                  m_Locale,
                  CS_SYB_CHARSET,
                  (CS_CHAR*) GetClientCharset().c_str(),
                  CS_UNUSED,
                  NULL);
    }
}

// Tunable version of TDS protocol to use

#if !defined(NCBI_CTLIB_TDS_VERSION)
#    define NCBI_CTLIB_TDS_VERSION 125
#endif

#define NCBI_CTLIB_TDS_FALLBACK_VERSION 110


NCBI_PARAM_DECL  (int, ctlib, TDS_VERSION);
NCBI_PARAM_DEF_EX(int, ctlib, TDS_VERSION,
                  NCBI_CTLIB_TDS_VERSION,  // default TDS version
                  eParam_NoThread,
                  CTLIB_TDS_VERSION);
typedef NCBI_PARAM_TYPE(ctlib, TDS_VERSION) TCtlibTdsVersion;


CS_INT GetCtlibTdsVersion(int version)
{
    if (version == 0) {
        version = TCtlibTdsVersion::GetDefault();
    }

    switch ( version ) {
    case 42:
    case 46:
    case 70:
    case 80:
        return version;
    case 100:
        return CS_VERSION_100;
    case 110:
        return CS_VERSION_110;
#ifdef CS_VERSION_120
    case 120:
        return CS_VERSION_120;
#endif
#ifdef CS_VERSION_125
    case 125:
        return CS_VERSION_125;
#endif
    }

    int fallback_version = (version == NCBI_CTLIB_TDS_VERSION) ?
        NCBI_CTLIB_TDS_FALLBACK_VERSION : NCBI_CTLIB_TDS_VERSION;

    ERR_POST(Warning <<
             "The version " << version << " of TDS protocol for "
             "the DBAPI CTLib driver is not supported. Falling back to "
             "the TDS protocol version " << fallback_version << ".");

    return GetCtlibTdsVersion(fallback_version);
}



///////////////////////////////////////////////////////////////////////
// Driver manager related functions
//

I_DriverContext* CTLIB_CreateContext(const map<string,string>* attr = 0)
{
    bool reuse_context = true;
#ifdef FTDS_IN_USE
    int  tds_version   = 80;
#else
    int  tds_version   = NCBI_CTLIB_TDS_VERSION;
#endif

    if ( attr ) {
        map<string,string>::const_iterator citer = attr->find("reuse_context");
        if ( citer != attr->end() ) {
            reuse_context = (citer->second != "false");
        }

        citer = attr->find("version");
        if (citer != attr->end())
            tds_version = NStr::StringToInt(citer->second);
    }

    CTLibContext* cntx = new CTLibContext(reuse_context,
                                          GetCtlibTdsVersion(tds_version));

    if ( cntx && attr ) {
        string page_size;
        map<string,string>::const_iterator citer = attr->find("packet");
        if ( citer != attr->end() ) {
            page_size = citer->second;
        }
        if ( !page_size.empty() ) {
            CS_INT s= atoi(page_size.c_str());
            cntx->CTLIB_SetPacketSize(s);
        }
        string prog_name;
        citer = attr->find("prog_name");
        if ( citer != attr->end() ) {
            prog_name = citer->second;
        }
        if ( !prog_name.empty() ) {
            cntx->CTLIB_SetApplicationName(prog_name);
        }
        string host_name;
        citer = attr->find("host_name");
        if ( citer != attr->end() ) {
            host_name = citer->second;
        }
        if ( !host_name.empty() ) {
            cntx->CTLIB_SetHostName(host_name);
        }
    }
    return cntx;
}

///////////////////////////////////////////////////////////////////////////////
class CDbapiCtlibCFBase : public CSimpleClassFactoryImpl<I_DriverContext, CTLibContext>
{
public:
    typedef CSimpleClassFactoryImpl<I_DriverContext, CTLibContext> TParent;

public:
    CDbapiCtlibCFBase(const string& driver_name);
    ~CDbapiCtlibCFBase(void);

public:
    virtual TInterface*
    CreateInstance(
        const string& driver  = kEmptyStr,
        CVersionInfo version =
        NCBI_INTERFACE_VERSION(I_DriverContext),
        const TPluginManagerParamTree* params = 0) const;

};

CDbapiCtlibCFBase::CDbapiCtlibCFBase(const string& driver_name)
    : TParent( driver_name, 0 )
{
    return ;
}

CDbapiCtlibCFBase::~CDbapiCtlibCFBase(void)
{
    return ;
}

CDbapiCtlibCFBase::TInterface*
CDbapiCtlibCFBase::CreateInstance(
    const string& driver,
    CVersionInfo version,
    const TPluginManagerParamTree* params) const
{
    TImplementation* drv = 0;
    if ( !driver.empty()  &&  driver != m_DriverName ) {
        return 0;
    }
    if (version.Match(NCBI_INTERFACE_VERSION(I_DriverContext))
                        != CVersionInfo::eNonCompatible) {
        // Mandatory parameters ....
        bool reuse_context = true;

#ifdef FTDS_IN_USE
        int  tds_version   = 80;
#else
        int  tds_version   = NCBI_CTLIB_TDS_VERSION;
#endif

        // Optional parameters ...
        CS_INT page_size = 0;
        string prog_name;
        string host_name;
        string client_charset;

        if ( params != NULL ) {
            typedef TPluginManagerParamTree::TNodeList_CI TCIter;
            typedef TPluginManagerParamTree::TValueType   TValue;

            // Get parameters ...
            TCIter cit  = params->SubNodeBegin();
            TCIter cend = params->SubNodeEnd();

            for (; cit != cend; ++cit) {
                const TValue& v = (*cit)->GetValue();

                if ( v.id == "reuse_context" ) {
                    reuse_context = (v.value != "false");
                } else if ( v.id == "version" ) {
                    tds_version = NStr::StringToInt( v.value );
                } else if ( v.id == "packet" ) {
                    page_size = NStr::StringToInt( v.value );
                } else if ( v.id == "prog_name" ) {
                    prog_name = v.value;
                } else if ( v.id == "host_name" ) {
                    host_name = v.value;
                } else if ( v.id == "client_charset" ) {
                    client_charset = v.value;
                }
            }
        }

        // Create a driver ...
        drv = new CTLibContext(reuse_context,
                               GetCtlibTdsVersion(tds_version));

        // Set parameters ...
        if ( page_size ) {
            drv->CTLIB_SetPacketSize( page_size );
        }

        if ( !prog_name.empty() ) {
            drv->CTLIB_SetApplicationName( prog_name );
        }

        if ( !host_name.empty() ) {
            drv->CTLIB_SetHostName( host_name );
        }

        if ( !client_charset.empty() ) {
            drv->SetClientCharset( client_charset );
        }
    }
    return drv;
}

///////////////////////////////////////////////////////////////////////////////
class CDbapiCtlibCF_Sybase : public CDbapiCtlibCFBase
{
public:
    CDbapiCtlibCF_Sybase(void)
    : CDbapiCtlibCFBase("ctlib")
    {
    }
};


class CDbapiCtlibCF_ftds64_ctlib : public CDbapiCtlibCFBase
{
public:
    CDbapiCtlibCF_ftds64_ctlib(void)
    : CDbapiCtlibCFBase("ftds64_ctlib")
    {
    }
};


#ifdef FTDS_IN_USE

class CDbapiCtlibCF_ftds : public CDbapiCtlibCFBase
{
public:
    CDbapiCtlibCF_ftds(void)
    : CDbapiCtlibCFBase("ftds")
    {
    }
};

#endif


#ifdef FTDS_IN_USE
END_SCOPE(ftds64_ctlib)
#endif


///////////////////////////////////////////////////////////////////////////////
void
NCBI_EntryPoint_xdbapi_ctlib(
    CPluginManager<I_DriverContext>::TDriverInfoList&   info_list,
    CPluginManager<I_DriverContext>::EEntryPointRequest method)
{
#ifdef FTDS_IN_USE
    CHostEntryPointImpl<ftds64_ctlib::CDbapiCtlibCF_Sybase>::NCBI_EntryPointImpl( info_list, method );
#else
    CHostEntryPointImpl<CDbapiCtlibCF_Sybase>::NCBI_EntryPointImpl( info_list, method );
#endif
}

void
NCBI_EntryPoint_xdbapi_ftds64_ctlib(
    CPluginManager<I_DriverContext>::TDriverInfoList&   info_list,
    CPluginManager<I_DriverContext>::EEntryPointRequest method)
{
#ifdef FTDS_IN_USE
    CHostEntryPointImpl<ftds64_ctlib::CDbapiCtlibCF_ftds64_ctlib>::NCBI_EntryPointImpl( info_list, method );
#else
    CHostEntryPointImpl<CDbapiCtlibCF_ftds64_ctlib>::NCBI_EntryPointImpl( info_list, method );
#endif
}

NCBI_DBAPIDRIVER_CTLIB_EXPORT
void
DBAPI_RegisterDriver_CTLIB(void)
{
    RegisterEntryPoint<I_DriverContext>( NCBI_EntryPoint_xdbapi_ctlib );
    RegisterEntryPoint<I_DriverContext>( NCBI_EntryPoint_xdbapi_ftds64_ctlib );
}

///////////////////////////////////////////////////////////////////////////////
NCBI_DBAPIDRIVER_CTLIB_EXPORT
void DBAPI_RegisterDriver_CTLIB(I_DriverMgr& mgr)
{
#ifdef FTDS_IN_USE
    mgr.RegisterDriver("ctlib", ftds64_ctlib::CTLIB_CreateContext);
#else
    mgr.RegisterDriver("ctlib", CTLIB_CreateContext);
#endif
    DBAPI_RegisterDriver_CTLIB();
}


void DBAPI_RegisterDriver_CTLIB_old(I_DriverMgr& mgr)
{
    DBAPI_RegisterDriver_CTLIB( mgr );
}


extern "C" {
    NCBI_DBAPIDRIVER_CTLIB_EXPORT
    void* DBAPI_E_ctlib()
    {
        return (void*)DBAPI_RegisterDriver_CTLIB_old;
    }
}


///////////////////////////////////////////////////////////////////////////////
#ifdef FTDS_IN_USE

void
NCBI_EntryPoint_xdbapi_ftds(
    CPluginManager<I_DriverContext>::TDriverInfoList&   info_list,
    CPluginManager<I_DriverContext>::EEntryPointRequest method)
{
#ifdef FTDS_IN_USE
    CHostEntryPointImpl<ftds64_ctlib::CDbapiCtlibCF_ftds>::NCBI_EntryPointImpl( info_list, method );
#else
    CHostEntryPointImpl<CDbapiCtlibCF_ftds>::NCBI_EntryPointImpl( info_list, method );
#endif
}

NCBI_DBAPIDRIVER_CTLIB_EXPORT
void
DBAPI_RegisterDriver_FTDS(void)
{
    RegisterEntryPoint<I_DriverContext>( NCBI_EntryPoint_xdbapi_ftds );
}


NCBI_DBAPIDRIVER_CTLIB_EXPORT
void
DBAPI_RegisterDriver_FTDS(I_DriverMgr& mgr)
{
#ifdef FTDS_IN_USE
    mgr.RegisterDriver("ftds", ftds64_ctlib::CTLIB_CreateContext);
#else
    mgr.RegisterDriver("ftds", CTLIB_CreateContext);
#endif
    DBAPI_RegisterDriver_FTDS();
}

#endif

END_NCBI_SCOPE


