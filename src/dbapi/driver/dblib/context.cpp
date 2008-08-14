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
 * File Description:  Driver for DBLib server
 *
 */

#include <ncbi_pch.hpp>

#include "dblib_utils.hpp"

#include <corelib/ncbimtx.hpp>

#include <corelib/plugin_manager_impl.hpp>
#include <corelib/plugin_manager_store.hpp>
#include <corelib/ncbi_safe_static.hpp>

// DO NOT DELETE this include !!!
#include <dbapi/driver/driver_mgr.hpp>

#include <dbapi/driver/dblib/interfaces.hpp>
#include <dbapi/driver/dblib/interfaces_p.hpp>

#include <dbapi/driver/util/numeric_convert.hpp>
#include <dbapi/error_codes.hpp>

#if defined(NCBI_OS_MSWIN)
#  include <winsock2.h>
#  include "../ncbi_win_hook.hpp"
#endif

#include <algorithm>
#include <stdio.h>

#define NCBI_USE_ERRCODE_X   Dbapi_Dblib_Context

BEGIN_NCBI_SCOPE


/////////////////////////////////////////////////////////////////////////////
inline
CDiagCompileInfo GetBlankCompileInfo(void)
{
    return CDiagCompileInfo();
}


/////////////////////////////////////////////////////////////////////////////
//
//  CDblibContextRegistry (Singleton)
//

class NCBI_DBAPIDRIVER_DBLIB_EXPORT CDblibContextRegistry
{
public:
    static CDblibContextRegistry& Instance(void);

    void Add(CDBLibContext* ctx);
    void Remove(CDBLibContext* ctx);
    void ClearAll(void);
    static void StaticClearAll(void);

    bool ExitProcessIsPatched(void) const
    {
        return m_ExitProcessPatched;
    }

private:
    CDblibContextRegistry(void);
    ~CDblibContextRegistry(void) throw();

    mutable CMutex          m_Mutex;
    vector<CDBLibContext*>  m_Registry;
    bool                    m_ExitProcessPatched;

    friend class CSafeStaticPtr<CDblibContextRegistry>;
};


/////////////////////////////////////////////////////////////////////////////
CDblibContextRegistry::CDblibContextRegistry(void) :
m_ExitProcessPatched(false)
{
#if defined(NCBI_OS_MSWIN) && defined(NCBI_DLL_BUILD)

    try {
        m_ExitProcessPatched = 
            NWinHook::COnExitProcess::Instance().Add(CDblibContextRegistry::StaticClearAll);
    } catch (const NWinHook::CWinHookException&) {
        // Just in case ...
        m_ExitProcessPatched = false;
    }

#endif
}

CDblibContextRegistry::~CDblibContextRegistry(void) throw()
{
    try {
        ClearAll();
    }
    NCBI_CATCH_ALL_X( 1, NCBI_CURRENT_FUNCTION )
}

CDblibContextRegistry&
CDblibContextRegistry::Instance(void)
{
    static CSafeStaticPtr<CDblibContextRegistry> instance;

    return instance.Get();
}

void
CDblibContextRegistry::Add(CDBLibContext* ctx)
{
    CMutexGuard mg(m_Mutex);

    vector<CDBLibContext*>::iterator it = find(m_Registry.begin(),
                                               m_Registry.end(),
                                               ctx);
    if (it == m_Registry.end()) {
        m_Registry.push_back(ctx);
    }
}

void
CDblibContextRegistry::Remove(CDBLibContext* ctx)
{
    CMutexGuard mg(m_Mutex);

    vector<CDBLibContext*>::iterator it = find(m_Registry.begin(),
                                              m_Registry.end(),
                                              ctx);

    if (it != m_Registry.end()) {
        m_Registry.erase(it);
        ctx->x_SetRegistry(NULL);
    }
}


void
CDblibContextRegistry::ClearAll(void)
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
CDblibContextRegistry::StaticClearAll(void)
{
    CDblibContextRegistry::Instance().ClearAll();
}

/////////////////////////////////////////////////////////////////////////////
//
//  CDBLibContext::
//


extern "C" {

#ifdef MS_DBLIB_IN_USE
    static int s_DBLIB_err_callback
    (DBPROCESS* dblink,   int   severity,
     int        dberr,    int   oserr,
     const char*  dberrstr, const char* oserrstr)
    {
        return CDBLibContext::DBLIB_dberr_handler
            (dblink, severity, dberr, oserr, dberrstr? dberrstr : "",
             oserrstr? oserrstr : "");
    }

    static int s_DBLIB_msg_callback
    (DBPROCESS* dblink,   DBINT msgno,
     int        msgstate, int   severity,
     const char*      msgtxt,   const char* srvname,
     const char*      procname, unsigned short   line)
    {
        CDBLibContext::DBLIB_dbmsg_handler
            (dblink, msgno,   msgstate, severity,
             msgtxt? msgtxt : "", srvname? srvname : "", procname? procname : "",
             line);
        return 0;
    }
#else
    static int CS_PUBLIC s_DBLIB_err_callback
    (DBPROCESS* dblink,   int   severity,
     int        dberr,    int   oserr,
     char*      dberrstr, char* oserrstr)
    {
        return CDBLibContext::DBLIB_dberr_handler
            (dblink, severity, dberr, oserr, dberrstr? dberrstr : "",
             oserrstr? oserrstr : "");
    }

    static int CS_PUBLIC s_DBLIB_msg_callback
    (DBPROCESS* dblink,   DBINT msgno,
     int        msgstate, int   severity,
     char*      msgtxt,   char* srvname,
     char*      procname, int   line)
    {
        CDBLibContext::DBLIB_dbmsg_handler
            (dblink, msgno,   msgstate, severity,
             msgtxt? msgtxt : "", srvname? srvname : "", procname? procname : "",
             line);
        return 0;
    }
#endif
}


DEFINE_STATIC_FAST_MUTEX(s_CtxMutex);
static CDBLibContext* g_pContext = NULL;


CDBLibContext::CDBLibContext(DBINT version)
: m_PacketSize( 0 )
, m_TDSVersion( version )
, m_BufferSize(1)
{
    SetApplicationName("DBLibDriver");

    CFastMutexGuard mg(s_CtxMutex);

    CHECK_DRIVER_ERROR(
        g_pContext != NULL,
        "You cannot use more than one dblib context "
        "concurrently",
        200000 );

    ResetEnvSybase();

    char hostname[256];
    if(gethostname(hostname, 256) == 0) {
        hostname[255] = '\0';
        SetHostName( hostname );
    }

#ifdef MS_DBLIB_IN_USE
    if (Check(dbinit()) == NULL || version == 31415)
#else
    if (Check(dbinit()) != SUCCEED)
#endif
    {
        DATABASE_DRIVER_ERROR( "dbinit failed", 200001 );
    }

#ifndef MS_DBLIB_IN_USE
    if (Check(dbsetversion(version)) != SUCCEED) {
        DATABASE_DRIVER_ERROR( "dbsetversion failed", 200001 );
    }
#endif

    Check(dberrhandle(s_DBLIB_err_callback));
    Check(dbmsghandle(s_DBLIB_msg_callback));

    g_pContext = this;

    m_Registry = &CDblibContextRegistry::Instance();
    x_AddToRegistry();
}

void
CDBLibContext::x_AddToRegistry(void)
{
    if (m_Registry) {
        m_Registry->Add(this);
    }
}

void
CDBLibContext::x_RemoveFromRegistry(void)
{
    if (m_Registry) {
        m_Registry->Remove(this);
    }
}

void
CDBLibContext::x_SetRegistry(CDblibContextRegistry* registry)
{
    m_Registry = registry;
}

int CDBLibContext::GetTDSVersion(void) const
{
    return m_TDSVersion;
}

bool CDBLibContext::SetLoginTimeout(unsigned int nof_secs)
{
    if (impl::CDriverContext::SetLoginTimeout(nof_secs)) {
        CFastMutexGuard ctx_mg(s_CtxMutex);
        return Check(dbsetlogintime(GetLoginTimeout())) == SUCCEED;
    }

    return false;
}


bool CDBLibContext::SetTimeout(unsigned int nof_secs)
{
    if (impl::CDriverContext::SetTimeout(nof_secs)) {
        CFastMutexGuard ctx_mg(s_CtxMutex);
        return Check(dbsettime(GetTimeout())) == SUCCEED;
    }

    return false;
}


bool CDBLibContext::SetMaxTextImageSize(size_t nof_bytes)
{
    impl::CDriverContext::SetMaxTextImageSize(nof_bytes);

    char s[64];
    sprintf(s, "%lu", (unsigned long) GetMaxTextImageSize());

    CFastMutexGuard ctx_mg(s_CtxMutex);
#ifdef MS_DBLIB_IN_USE
    return Check(dbsetopt(0, DBTEXTLIMIT, s)) == SUCCEED;
#else
    return Check(dbsetopt(0, DBTEXTLIMIT, s, -1)) == SUCCEED;
#endif
}

impl::CConnection*
CDBLibContext::MakeIConnection(const CDBConnParams& params)
{
    return new CDBL_Connection(*this, params);
}


bool CDBLibContext::IsAbleTo(ECapability cpb) const
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


CDBLibContext::~CDBLibContext()
{
    try {
        x_Close();
    }
    NCBI_CATCH_ALL_X( 2, NCBI_CURRENT_FUNCTION )
}

void
CDBLibContext::x_Close(bool delete_conn)
{
    CMutexGuard mg(m_CtxMtx);

    if (g_pContext) {
        CFastMutexGuard ctx_mg(s_CtxMutex);

        if (g_pContext) {
            // Unregister itself before any other poeration
            // for sake of exception safety.
            g_pContext = NULL;
            x_RemoveFromRegistry();

            if (x_SafeToFinalize()) {
                // close all connections first
                try {
                    if (delete_conn) {
                        DeleteAllConn();
                    } else {
                        CloseAllConn();
                    }
                }
                NCBI_CATCH_ALL_X( 3, NCBI_CURRENT_FUNCTION )

#ifdef MS_DBLIB_IN_USE
                dbwinexit();
                CheckFunctCall();
#else
                try {
                    // This function can fail if we try to connect to MS SQL server
                    // using Sybase client.
                    dbexit();
                    CheckFunctCall();
                }
                NCBI_CATCH_ALL_X( 4, "dbexit() call failed. This usually happens when "
                    "Sybase client has been used to connect to a MS SQL Server." )
#endif
            }
        }
    } else {
        if (delete_conn && x_SafeToFinalize()) {
            DeleteAllConn();
        }
    }
}

bool CDBLibContext::x_SafeToFinalize(void) const
{
    if (m_Registry) {
#if defined(NCBI_OS_MSWIN)
        return m_Registry->ExitProcessIsPatched();
#endif
    }

    return true;
}

void CDBLibContext::DBLIB_SetApplicationName(const string& app_name)
{
    SetApplicationName( app_name );
}


void CDBLibContext::DBLIB_SetHostName(const string& host_name)
{
    SetHostName( host_name );
}


void CDBLibContext::DBLIB_SetPacketSize(int p_size)
{
    m_PacketSize = (short) p_size;
}


bool CDBLibContext::DBLIB_SetMaxNofConns(int n)
{
    CFastMutexGuard ctx_mg(s_CtxMutex);
    return Check(dbsetmaxprocs(n)) == SUCCEED;
}


int CDBLibContext::DBLIB_dberr_handler(DBPROCESS*    dblink,
                                       int           severity,
                                       int           dberr,
                                       int           /*oserr*/,
                                       const string& dberrstr,
                                       const string& /*oserrstr*/)
{
    string server_name;
    string user_name;
    string message = dberrstr;

    CDBL_Connection* link = dblink ?
        reinterpret_cast<CDBL_Connection*> (dbgetuserdata(dblink)) : 0;

    if ( link ) {
        server_name = link->ServerName();
        user_name = link->UserName();
    }

    switch (dberr) {
    case SYBETIME:
    case SYBEFCON:
    case SYBECONN:
        {
            CDB_TimeoutEx ex(GetBlankCompileInfo(),
                             0,
                             message,
                             dberr);

            ex.SetServerName(server_name);
            ex.SetUserName(user_name);
            ex.SetSybaseSeverity(severity);

            GetDBLExceptionStorage().Accept(ex);
        }
        return INT_TIMEOUT;

#ifdef FTDS_IN_USE
    case 50000:
        {
            CDB_TruncateEx ex(GetBlankCompileInfo(),
                              0,
                              message,
                              dberr);

            ex.SetServerName(server_name);
            ex.SetUserName(user_name);
            ex.SetSybaseSeverity(severity);

            GetDBLExceptionStorage().Accept(ex);

            return INT_CANCEL;
        }
#endif

    default:
        if(dberr == 1205) {
            CDB_DeadlockEx ex(GetBlankCompileInfo(),
                              0,
                              message);

            ex.SetServerName(server_name);
            ex.SetUserName(user_name);
            ex.SetSybaseSeverity(severity);

            GetDBLExceptionStorage().Accept(ex);

            return INT_CANCEL;
        }

        break;
    }


    switch (severity) {
    case EXINFO:
    case EXUSER:
        {
            CDB_ClientEx ex(GetBlankCompileInfo(),
                            0,
                            message,
                            eDiag_Info,
                            dberr);

            ex.SetServerName(server_name);
            ex.SetUserName(user_name);
            ex.SetSybaseSeverity(severity);

            GetDBLExceptionStorage().Accept(ex);
        }
        break;
    case EXNONFATAL:
    case EXCONVERSION:
    case EXSERVER:
    case EXPROGRAM:
        if ( dberr != 20018 ) {
            CDB_ClientEx ex(GetBlankCompileInfo(),
                            0,
                            message,
                            eDiag_Error,
                            dberr);

            ex.SetServerName(server_name);
            ex.SetUserName(user_name);
            ex.SetSybaseSeverity(severity);

            GetDBLExceptionStorage().Accept(ex);
        }
        break;
    case EXTIME:
        {
            CDB_TimeoutEx ex(GetBlankCompileInfo(),
                             0,
                             message,
                             dberr);

            ex.SetServerName(server_name);
            ex.SetUserName(user_name);
            ex.SetSybaseSeverity(severity);

            GetDBLExceptionStorage().Accept(ex);
        }
        return INT_TIMEOUT;
    default:
        {
            CDB_ClientEx ex(GetBlankCompileInfo(),
                            0,
                            message,
                            eDiag_Critical,
                            dberr);

            ex.SetServerName(server_name);
            ex.SetUserName(user_name);
            ex.SetSybaseSeverity(severity);

            GetDBLExceptionStorage().Accept(ex);
        }
        break;
    }
    return INT_CANCEL;
}


void CDBLibContext::DBLIB_dbmsg_handler(DBPROCESS*    dblink,
                                        DBINT         msgno,
                                        int           /*msgstate*/,
                                        int           severity,
                                        const string& msgtxt,
                                        const string& srvname,
                                        const string& procname,
                                        int           line)
{
    string server_name;
    string user_name;
    string message = msgtxt;

    if (msgno == 5701 || msgno == 5703 || msgno == 5704)
        return;

    CDBL_Connection* link = dblink ?
        reinterpret_cast<CDBL_Connection*>(dbgetuserdata(dblink)) : 0;

    if ( link ) {
        server_name = link->ServerName();
        user_name = link->UserName();
    } else {
        server_name = srvname;
    }

    if (msgno == 1205/*DEADLOCK*/) {
        CDB_DeadlockEx ex(GetBlankCompileInfo(),
                          0,
                          message);

        ex.SetServerName(server_name);
        ex.SetUserName(user_name);
        ex.SetSybaseSeverity(severity);

        GetDBLExceptionStorage().Accept(ex);
    }
    else if (msgno == 1708  ||  msgno == 1771) {
        // "Maximum row size exceeds allowable width. It is being rounded down to 32767 bytes."
        // "Row size (32767 bytes) could exceed row size limit, which is 1962 bytes."
        ERR_POST_X(6, Warning << msgtxt);
    }
    else {
        EDiagSev sev =
            severity <  10 ? eDiag_Info :
            severity == 10 ? eDiag_Warning :
            severity <  16 ? eDiag_Error : eDiag_Critical;

        if (!procname.empty()) {
            CDB_RPCEx ex(GetBlankCompileInfo(),
                         0,
                         message,
                         sev,
                         msgno,
                         procname,
                         line);

            ex.SetServerName(server_name);
            ex.SetUserName(user_name);
            ex.SetSybaseSeverity(severity);

            GetDBLExceptionStorage().Accept(ex);
        } else {
            CDB_DSEx ex(GetBlankCompileInfo(),
                        0,
                        message,
                        sev,
                        msgno);

            ex.SetServerName(server_name);
            ex.SetUserName(user_name);
            ex.SetSybaseSeverity(severity);

            GetDBLExceptionStorage().Accept(ex);
        }
    }
}


void CDBLibContext::CheckFunctCall(void)
{
    GetDBLExceptionStorage().Handle(GetCtxHandlerStack(), GetExtraMsg());
}

///////////////////////////////////////////////////////////////////////
// Driver manager related functions
//

///////////////////////////////////////////////////////////////////////////////
#if defined(MS_DBLIB_IN_USE)

class CDbapiMSDblibCF2 : public CSimpleClassFactoryImpl<I_DriverContext, CDBLibContext>
{
public:
    typedef CSimpleClassFactoryImpl<I_DriverContext, CDBLibContext> TParent;

public:
    CDbapiMSDblibCF2(void);
    ~CDbapiMSDblibCF2(void);

public:
    virtual TInterface*
    CreateInstance(
        const string& driver  = kEmptyStr,
        CVersionInfo version =
        NCBI_INTERFACE_VERSION(I_DriverContext),
        const TPluginManagerParamTree* params = 0) const;

};

CDbapiMSDblibCF2::CDbapiMSDblibCF2(void)
    : TParent( "msdblib", 0 )
{
    return ;
}

CDbapiMSDblibCF2::~CDbapiMSDblibCF2(void)
{
    return ;
}

CDbapiMSDblibCF2::TInterface*
CDbapiMSDblibCF2::CreateInstance(
    const string& driver,
    CVersionInfo version,
    const TPluginManagerParamTree* params) const
{
    TImplementation* drv = NULL;

    if ( !driver.empty()  &&  driver != m_DriverName ) {
        return 0;
    }
    if (version.Match(NCBI_INTERFACE_VERSION(I_DriverContext))
                        != CVersionInfo::eNonCompatible) {
        // Mandatory parameters ....
        DBINT version = DBVERSION_46;

        drv = new CDBLibContext(version);
    }
    return drv;
}

void
NCBI_EntryPoint_xdbapi_msdblib(
    CPluginManager<I_DriverContext>::TDriverInfoList&   info_list,
    CPluginManager<I_DriverContext>::EEntryPointRequest method)
{
    CHostEntryPointImpl<CDbapiMSDblibCF2>::NCBI_EntryPointImpl( info_list, method );
}

NCBI_DBAPIDRIVER_DBLIB_EXPORT
void
DBAPI_RegisterDriver_MSDBLIB(void)
{
    RegisterEntryPoint<I_DriverContext>( NCBI_EntryPoint_xdbapi_msdblib );
}

#elif defined(FTDS_IN_USE)

#define NCBI_FTDS 8

///////////////////////////////////////////////////////////////////////////////
// Version-specific driver name and DLL entry point
#if defined(NCBI_FTDS)
#  if   NCBI_FTDS == 7
#    define NCBI_FTDS_DRV_NAME    "ftds7"
#    define NCBI_FTDS_ENTRY_POINT DBAPI_E_ftds7
#  elif NCBI_FTDS == 8
#    define NCBI_FTDS_DRV_NAME    "ftds8"
#    define NCBI_FTDS_ENTRY_POINT DBAPI_E_ftds8
#  elif
#    error "This version of FreeTDS is not supported"
#  endif
#endif


class CDbapiFtdsCFBase : public CSimpleClassFactoryImpl<I_DriverContext, CDBLibContext>
{
public:
    typedef CSimpleClassFactoryImpl<I_DriverContext, CDBLibContext> TParent;

public:
    CDbapiFtdsCFBase(const string& driver_name);
    ~CDbapiFtdsCFBase(void);

public:
    virtual TInterface*
    CreateInstance(
        const string& driver  = kEmptyStr,
        CVersionInfo version =
        NCBI_INTERFACE_VERSION(I_DriverContext),
        const TPluginManagerParamTree* params = 0) const;

};

CDbapiFtdsCFBase::CDbapiFtdsCFBase(const string& driver_name)
    : TParent(driver_name, 1)
{
    return ;
}

CDbapiFtdsCFBase::~CDbapiFtdsCFBase(void)
{
    return ;
}

CDbapiFtdsCFBase::TInterface*
CDbapiFtdsCFBase::CreateInstance(
    const string& driver,
    CVersionInfo version,
    const TPluginManagerParamTree* params) const
{
    TImplementation* drv = 0;
    if ( !driver.empty() && driver != m_DriverName ) {
        return 0;
    }
    if (version.Match(NCBI_INTERFACE_VERSION(I_DriverContext))
                        != CVersionInfo::eNonCompatible) {
        // Mandatory parameters ....
        DBINT db_version = DBVERSION_UNKNOWN;

        // Optional parameters ...
        CS_INT page_size = 0;

        string client_charset;

        if ( params != NULL ) {
            typedef TPluginManagerParamTree::TNodeList_CI TCIter;
            typedef TPluginManagerParamTree::TValueType   TValue;

            // Get parameters ...
            TCIter cit = params->SubNodeBegin();
            TCIter cend = params->SubNodeEnd();

            for (; cit != cend; ++cit) {
                const TValue& v = (*cit)->GetValue();

                if ( v.id == "reuse_context" ) {
                    bool reuse_context = (v.value != "false");
                    CFastMutexGuard mg(s_CtxMutex);

                    if ( reuse_context && g_pContext ) {
                        return g_pContext;
                    }
                } else if ( v.id == "version" ) {
                    int value = NStr::StringToInt( v.value );

                    switch ( value ) {
                    case 42 :
                        db_version = DBVERSION_42;
                        break;
                    case 46 :
                        db_version = DBVERSION_46;
                        break;
                    case 70 :
                        db_version = DBVERSION_70;
                        break;
                    case 80 :
                        db_version = DBVERSION_80;
                        break;
                    case 100 :
                        db_version = DBVERSION_100;
                        break;
                    }
                } else if ( v.id == "packet" ) {
                    page_size = NStr::StringToInt( v.value );
                } else if ( v.id == "client_charset" ) {
                    client_charset = v.value;
                }
            }
        }

        // Create a driver ...
        drv = new CDBLibContext( db_version );

        // Set parameters ...
        if ( page_size ) {
            drv->DBLIB_SetPacketSize( page_size );
        }

        // Set client's charset ...
        if ( !client_charset.empty() ) {
            drv->SetClientCharset( client_charset.c_str() );
        }
    }
    return drv;
}

class CDbapiFtdsCF : public CDbapiFtdsCFBase
{
public:
    CDbapiFtdsCF(void)
    : CDbapiFtdsCFBase("ftds")
    {
    }
};

class CDbapiFtdsCF63 : public CDbapiFtdsCFBase
{
public:
    CDbapiFtdsCF63(void)
    : CDbapiFtdsCFBase("ftds63")
    {
    }
};

class CDbapiFtdsCF64 : public CDbapiFtdsCFBase
{
public:
    CDbapiFtdsCF64(void)
    : CDbapiFtdsCFBase("ftds_dblib")
    {
    }
};

///////////////////////////////////////////////////////////////////////////////
void
NCBI_EntryPoint_xdbapi_ftds(
    CPluginManager<I_DriverContext>::TDriverInfoList&   info_list,
    CPluginManager<I_DriverContext>::EEntryPointRequest method)
{
    CHostEntryPointImpl<CDbapiFtdsCF>::NCBI_EntryPointImpl( info_list, method );
}

void
NCBI_EntryPoint_xdbapi_ftds63(
    CPluginManager<I_DriverContext>::TDriverInfoList&   info_list,
    CPluginManager<I_DriverContext>::EEntryPointRequest method)
{
    CHostEntryPointImpl<CDbapiFtdsCF63>::NCBI_EntryPointImpl( info_list, method );
}

void
NCBI_EntryPoint_xdbapi_ftds_dblib(
    CPluginManager<I_DriverContext>::TDriverInfoList&   info_list,
    CPluginManager<I_DriverContext>::EEntryPointRequest method)
{
    CHostEntryPointImpl<CDbapiFtdsCF64>::NCBI_EntryPointImpl( info_list, method );
}

NCBI_DBAPIDRIVER_DBLIB_EXPORT
void
DBAPI_RegisterDriver_FTDS(void)
{
    RegisterEntryPoint<I_DriverContext>( NCBI_EntryPoint_xdbapi_ftds );
    RegisterEntryPoint<I_DriverContext>( NCBI_EntryPoint_xdbapi_ftds63 );
    RegisterEntryPoint<I_DriverContext>( NCBI_EntryPoint_xdbapi_ftds_dblib );
}

///////////////////////////////////////////////////////////////////////////////
// NOTE:  we define a generic ("ftds") driver name here -- in order to
//        provide a default, but also to prevent from accidental linking
//        of more than one version of FreeTDS driver to the same application

#else

///////////////////////////////////////////////////////////////////////////////
class CDbapiDblibCF2 : public CSimpleClassFactoryImpl<I_DriverContext, CDBLibContext>
{
public:
    typedef CSimpleClassFactoryImpl<I_DriverContext, CDBLibContext> TParent;

public:
    CDbapiDblibCF2(void);
    ~CDbapiDblibCF2(void);

public:
    virtual TInterface*
    CreateInstance(
        const string& driver  = kEmptyStr,
        CVersionInfo version =
        NCBI_INTERFACE_VERSION(I_DriverContext),
        const TPluginManagerParamTree* params = 0) const;

};

CDbapiDblibCF2::CDbapiDblibCF2(void)
    : TParent( "dblib", 0 )
{
    return ;
}

CDbapiDblibCF2::~CDbapiDblibCF2(void)
{
    return ;
}

CDbapiDblibCF2::TInterface*
CDbapiDblibCF2::CreateInstance(
    const string& driver,
    CVersionInfo version,
    const TPluginManagerParamTree* params) const
{
    auto_ptr<TImplementation> drv;

    if ( !driver.empty() && driver != m_DriverName ) {
        return 0;
    }

    if (version.Match(NCBI_INTERFACE_VERSION(I_DriverContext))
                        != CVersionInfo::eNonCompatible) {
        // Mandatory parameters ....
        DBINT db_version = DBVERSION_46;

        // Optional parameters ...
        CS_INT page_size = 0;
        string client_charset;

        if ( params != NULL ) {
            typedef TPluginManagerParamTree::TNodeList_CI TCIter;
            typedef TPluginManagerParamTree::TValueType   TValue;

            // Get parameters ...
            TCIter cit = params->SubNodeBegin();
            TCIter cend = params->SubNodeEnd();

            for (; cit != cend; ++cit) {
                const TValue& v = (*cit)->GetValue();

                if ( v.id == "reuse_context" ) {
                    bool reuse_context = (v.value != "false");
                    CFastMutexGuard mg(s_CtxMutex);

                    if ( reuse_context && g_pContext ) {
                        return g_pContext;
                    }
                } else if ( v.id == "version" ) {
                    int value = NStr::StringToInt( v.value );
                    switch ( value ) {
                    case 46 :
                        db_version = DBVERSION_46;
                        break;
                    case 100 :
                        db_version = DBVERSION_100;
                    }
                } else if ( v.id == "packet" ) {
                    page_size = NStr::StringToInt( v.value );
                } else if ( v.id == "client_charset" ) {
                    client_charset = v.value;
                }
            }
        }

        // Create a driver ...
        drv.reset(new CDBLibContext(db_version));

        // Set parameters ...
        if ( page_size ) {
            drv->DBLIB_SetPacketSize( page_size );
        }

        // Set client's charset ...
        if ( !client_charset.empty() ) {
            drv->SetClientCharset( client_charset.c_str() );
        }
    }

    return drv.release();
}

///////////////////////////////////////////////////////////////////////////////
void
NCBI_EntryPoint_xdbapi_dblib(
    CPluginManager<I_DriverContext>::TDriverInfoList&   info_list,
    CPluginManager<I_DriverContext>::EEntryPointRequest method)
{
    CHostEntryPointImpl<CDbapiDblibCF2>::NCBI_EntryPointImpl( info_list, method );
}

NCBI_DBAPIDRIVER_DBLIB_EXPORT
void
DBAPI_RegisterDriver_DBLIB(void)
{
    RegisterEntryPoint<I_DriverContext>( NCBI_EntryPoint_xdbapi_dblib );
}

#endif

END_NCBI_SCOPE



