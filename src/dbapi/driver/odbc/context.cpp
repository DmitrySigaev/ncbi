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
 * File Description:  Driver for ODBC server
 *
 */

#include <ncbi_pch.hpp>

#include <corelib/ncbimtx.hpp>
#include <corelib/plugin_manager_impl.hpp>
#include <corelib/plugin_manager_store.hpp>
#include <corelib/ncbi_safe_static.hpp>

// DO NOT DELETE this include !!!
#include <dbapi/driver/driver_mgr.hpp>

#include <dbapi/driver/odbc/interfaces.hpp>
#include <dbapi/driver/util/numeric_convert.hpp>

#ifdef HAVE_ODBCSS_H
#include <odbcss.h>
#endif

BEGIN_NCBI_SCOPE

static const CDiagCompileInfo kBlankCompileInfo;

/////////////////////////////////////////////////////////////////////////////
//
//  CODBCContextRegistry (Singleton)
//

class CODBCContextRegistry
{
public:
    static CODBCContextRegistry& Instance(void);
    
    void Add(CODBCContext* ctx);
    void Remove(CODBCContext* ctx);
    void ClearAll(void);
    
private:
    CODBCContextRegistry(void);
    ~CODBCContextRegistry(void);
    
    mutable CMutex          m_Mutex;
    vector<CODBCContext*>   m_Registry;
    
    friend class CSafeStaticPtr<CODBCContextRegistry>;
};


CODBCContextRegistry::CODBCContextRegistry(void)
{
}

CODBCContextRegistry::~CODBCContextRegistry(void)
{
    ClearAll();
}

CODBCContextRegistry& 
CODBCContextRegistry::Instance(void)
{
    static CSafeStaticPtr<CODBCContextRegistry> instance;
    
    return *instance;
}

void 
CODBCContextRegistry::Add(CODBCContext* ctx)
{
    CMutexGuard mg(m_Mutex);
    
    vector<CODBCContext*>::iterator it = find(m_Registry.begin(), 
                                              m_Registry.end(), 
                                              ctx);
    if (it == m_Registry.end()) {
        m_Registry.push_back(ctx);
    }
}

void 
CODBCContextRegistry::Remove(CODBCContext* ctx)
{
    CMutexGuard mg(m_Mutex);
    
    m_Registry.erase(find(m_Registry.begin(), 
                          m_Registry.end(), 
                          ctx));
}

void 
CODBCContextRegistry::ClearAll(void)
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

/////////////////////////////////////////////////////////////////////////////
//
//  CODBC_Reporter::
//
CODBC_Reporter::CODBC_Reporter(CDBHandlerStack* hs, 
                               SQLSMALLINT ht, 
                               SQLHANDLE h,
                               const CODBC_Reporter* parent_reporter) 
: m_HStack(hs)
, m_HType(ht)
, m_Handle(h)
, m_ParentReporter(parent_reporter) 
{
}

CODBC_Reporter::~CODBC_Reporter(void)
{
}

string 
CODBC_Reporter::GetExtraMsg(void) const
{
    if ( m_ParentReporter != NULL ) {
        return " " + m_ExtraMsg + " " + m_ParentReporter->GetExtraMsg();
    }

    return " " + m_ExtraMsg;
}

void CODBC_Reporter::ReportErrors(void) const
{
    SQLINTEGER NativeError;
    SQLSMALLINT MsgLen;
    SQLCHAR SqlState[6];
    SQLCHAR Msg[1024];
    string err_msg;

    if( !m_HStack ) {
        return;
    }

    memset(Msg, 0, sizeof(Msg));

    for(SQLSMALLINT i= 1; i < 128; i++) {
        int rc = SQLGetDiagRec(m_HType, m_Handle, i, SqlState, &NativeError,
                             Msg, sizeof(Msg), &MsgLen);
        string err_msg( reinterpret_cast<const char*>(Msg) );
        err_msg += GetExtraMsg();

        switch( rc ) {
        case SQL_SUCCESS:
            if(strncmp((const char*)SqlState, "HYT", 3) == 0) { // timeout

                CDB_TimeoutEx to(kBlankCompileInfo,
                                 0,
                                 err_msg.c_str(),
                                 NativeError);

                m_HStack->PostMsg(&to);
            }
            else if(strncmp((const char*)SqlState, "40001", 5) == 0) { // deadlock
                CDB_DeadlockEx dl(kBlankCompileInfo,
                                  0,
                                  err_msg.c_str());
                m_HStack->PostMsg(&dl);
            }
            else if(NativeError != 5701 && NativeError != 5703){
                CDB_SQLEx se(kBlankCompileInfo,
                             0,
                             err_msg.c_str(),
                             (NativeError == 0 ? eDiag_Info : eDiag_Warning),
                             NativeError,
                             (const char*)SqlState,
                             0);
                m_HStack->PostMsg(&se);
            }
            continue;

        case SQL_NO_DATA: break;

        case SQL_SUCCESS_WITH_INFO:
            {
                string err_msg( "Message is too long to be retrieved" );
                err_msg += GetExtraMsg();

                CDB_DSEx dse(kBlankCompileInfo,
                             0,
                             err_msg.c_str(),
                             eDiag_Warning,
                             777);
                m_HStack->PostMsg(&dse);
            }
            continue;

        default:
            {
                string err_msg( "SQLGetDiagRec failed (memory corruption suspected" );
                err_msg += GetExtraMsg();

                CDB_ClientEx ce(kBlankCompileInfo,
                                0,
                                err_msg.c_str(),
                                eDiag_Warning,
                                420016);
                m_HStack->PostMsg(&ce);
            }
            break;
        }
        break;
    }
}

/////////////////////////////////////////////////////////////////////////////
//
//  CODBCContext::
//

CODBCContext::CODBCContext(SQLLEN version, bool use_dsn)
: m_PacketSize(0)
, m_LoginTimeout(0)
, m_Timeout(0)
, m_TextImageSize(0)
, m_Reporter(0, SQL_HANDLE_ENV, 0)
, m_UseDSN(use_dsn)
, m_Registry(&CODBCContextRegistry::Instance())
{
    DEFINE_STATIC_FAST_MUTEX(xMutex);
    CFastMutexGuard mg(xMutex);

    if(SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &m_Context) != SQL_SUCCESS) {
        string err_message = "Cannot allocate a context" + m_Reporter.GetExtraMsg();
        DATABASE_DRIVER_ERROR( err_message, 400001 );
    }

    m_Reporter.SetHandle(m_Context);
    m_Reporter.SetHandlerStack(&m_CntxHandlers);

    SQLSetEnvAttr(m_Context, SQL_ATTR_ODBC_VERSION, (SQLPOINTER)version, 0);

    x_AddToRegistry();
}

void 
CODBCContext::x_AddToRegistry(void)
{
    if (m_Registry) {
        m_Registry->Add(this);
    }
}

void 
CODBCContext::x_RemoveFromRegistry(void)
{
    if (m_Registry) {
        m_Registry->Remove(this);
    }
}

void 
CODBCContext::x_SetRegistry(CODBCContextRegistry* registry)
{
    m_Registry = registry;
}


bool CODBCContext::SetLoginTimeout(unsigned int nof_secs)
{
    m_LoginTimeout = (SQLULEN)nof_secs;
    return true;
}


bool CODBCContext::SetTimeout(unsigned int nof_secs)
{
    CFastMutexGuard mg(m_Mtx);
    
    m_Timeout = (SQLULEN)nof_secs;

    ITERATE(TConnPool, it, m_NotInUse) {
        CODBC_Connection* t_con
            = dynamic_cast<CODBC_Connection*>(*it);
        if (!t_con) continue;

        t_con->ODBC_SetTimeout(m_Timeout);
    }

    ITERATE(TConnPool, it, m_InUse) {
        CODBC_Connection* t_con
            = dynamic_cast<CODBC_Connection*>(*it);
        if (!t_con) continue;

        t_con->ODBC_SetTimeout(m_Timeout);
    }

    return true;
}


bool CODBCContext::SetMaxTextImageSize(size_t nof_bytes)
{
    CFastMutexGuard mg(m_Mtx);
    
    m_TextImageSize = (SQLUINTEGER) nof_bytes;

    ITERATE(TConnPool, it, m_NotInUse) {
        CODBC_Connection* t_con
            = dynamic_cast<CODBC_Connection*>(*it);
        if (!t_con) continue;

        t_con->ODBC_SetTextImageSize(m_TextImageSize);
    }

    ITERATE(TConnPool, it, m_InUse) {
        CODBC_Connection* t_con
            = dynamic_cast<CODBC_Connection*>(*it);
        if (!t_con) continue;

        t_con->ODBC_SetTextImageSize(m_TextImageSize);
    }

    return true;
}


I_Connection* 
CODBCContext::MakeIConnection(const SConnAttr& conn_attr)
{
    CFastMutexGuard mg(m_Mtx);
    
    SQLHDBC con = x_ConnectToServer(conn_attr.srv_name, 
                                    conn_attr.user_name, 
                                    conn_attr.passwd, 
                                    conn_attr.mode);
    
    if (!con) {
        string err;
        
        err += "Cannot connect to the server '" + conn_attr.srv_name;
        err += "' as user '" + conn_attr.user_name + "'" + m_Reporter.GetExtraMsg();
        DATABASE_DRIVER_ERROR( err, 100011 );
    }

    CODBC_Connection* t_con = new CODBC_Connection(this, 
                                                   con, 
                                                   conn_attr.reusable, 
                                                   conn_attr.pool_name);
    t_con->m_MsgHandlers = m_ConnHandlers;
    t_con->m_Server      = conn_attr.srv_name;
    t_con->m_User        = conn_attr.user_name;
    t_con->m_Passwd      = conn_attr.passwd;
    t_con->m_BCPable     = (conn_attr.mode & fBcpIn) != 0;
    t_con->m_SecureLogin = (conn_attr.mode & fPasswordEncrypted) != 0;
    
    return t_con;
}

CODBCContext::~CODBCContext()
{
    try {
        x_Close();
    }
    NCBI_CATCH_ALL( kEmptyStr )
}

void
CODBCContext::x_Close(bool delete_conn)
{
    if ( m_Context ) {
        CFastMutexGuard mg(m_Mtx);
        if (m_Context) {
            // close all connections first
            if (delete_conn) {
                DeleteAllConn();
            } else {
                CloseAllConn();
            }

            int rc = SQLFreeHandle(SQL_HANDLE_ENV, m_Context);
            switch( rc ) {
            case SQL_INVALID_HANDLE:
            case SQL_ERROR:
                m_Reporter.ReportErrors();
                break;
            case SQL_SUCCESS_WITH_INFO:
                m_Reporter.ReportErrors();
            case SQL_SUCCESS:
                break;
            default:
                m_Reporter.ReportErrors();
                break;
            };

            m_Context = NULL;
            x_RemoveFromRegistry();
        }
    }
}


void CODBCContext::ODBC_SetPacketSize(SQLUINTEGER packet_size)
{
    CFastMutexGuard mg(m_Mtx);
    
    m_PacketSize = (SQLULEN)packet_size;
}


SQLHENV CODBCContext::ODBC_GetContext() const
{
    return m_Context;
}


SQLHDBC CODBCContext::x_ConnectToServer(const string&   srv_name,
                                        const string&   user_name,
                                        const string&   passwd,
                                        TConnectionMode mode)
{
    // This is a private method. We do not have to make it thread-safe.
    // This will be made in public methods.

    SQLHDBC con;
    SQLRETURN r;

    r = SQLAllocHandle(SQL_HANDLE_DBC, m_Context, &con);
    if((r != SQL_SUCCESS) && (r != SQL_SUCCESS_WITH_INFO))
        return 0;

    if(m_Timeout) {
        SQLSetConnectAttr(con, SQL_ATTR_CONNECTION_TIMEOUT, (SQLPOINTER)m_Timeout, 0);
    }

    if(m_LoginTimeout) {
        SQLSetConnectAttr(con, SQL_ATTR_LOGIN_TIMEOUT, (SQLPOINTER)m_LoginTimeout, 0);
    }

    if(m_PacketSize) {
        SQLSetConnectAttr(con, SQL_ATTR_PACKET_SIZE, (SQLPOINTER)m_PacketSize, 0);
    }

#ifdef SQL_COPT_SS_BCP
    if((mode & fBcpIn) != 0) {
        SQLSetConnectAttr(con, SQL_COPT_SS_BCP, (SQLPOINTER) SQL_BCP_ON, SQL_IS_INTEGER);
    }
#endif


    string extra_msg = " SERVER: " + srv_name + "; USER: " + user_name;
    m_Reporter.SetExtraMsg( extra_msg );
        
    if(!m_UseDSN) {
#ifdef NCBI_OS_MSWIN
      string connect_str("DRIVER={SQL Server};SERVER=");
#else
      string connect_str("DSN=");
#endif
        connect_str+= srv_name;
        connect_str+= ";UID=";
        connect_str+= user_name;
        connect_str+= ";PWD=";
        connect_str+= passwd;
        
        r = SQLDriverConnect(con, 0, (SQLCHAR*) connect_str.c_str(), SQL_NTS,
                             0, 0, 0, SQL_DRIVER_NOPROMPT);
    }
    else {
        r = SQLConnect(con, (SQLCHAR*) srv_name.c_str(), SQL_NTS,
                      (SQLCHAR*) user_name.c_str(), SQL_NTS,
                      (SQLCHAR*) passwd.c_str(), SQL_NTS);
    }

    switch(r) {
    case SQL_SUCCESS_WITH_INFO:
        xReportConError(con);
    case SQL_SUCCESS: return con;

    case SQL_ERROR:
        xReportConError(con);
        SQLFreeHandle(SQL_HANDLE_DBC, con);
        break;
    default:
        m_Reporter.ReportErrors();
        break;
    }

    return 0;
}

void CODBCContext::xReportConError(SQLHDBC con)
{
    m_Reporter.SetHandleType(SQL_HANDLE_DBC);
    m_Reporter.SetHandle(con);
    m_Reporter.ReportErrors();
    m_Reporter.SetHandleType(SQL_HANDLE_ENV);
    m_Reporter.SetHandle(m_Context);
}


/////////////////////////////////////////////////////////////////////////////
//
//  Miscellaneous
//


///////////////////////////////////////////////////////////////////////
// Driver manager related functions
//

I_DriverContext* ODBC_CreateContext(const map<string,string>* attr = 0)
{
    SQLINTEGER version= SQL_OV_ODBC3;
    bool use_dsn= false;
    map<string,string>::const_iterator citer;

    if(attr) {
        string vers;
        citer = attr->find("version");
        if (citer != attr->end()) {
            vers = citer->second;
        }
        if(vers.find("3") != string::npos)
            version= SQL_OV_ODBC3;
        else if(vers.find("2") != string::npos)
            version= SQL_OV_ODBC2;
        citer = attr->find("use_dsn");
        if (citer != attr->end()) {
            use_dsn = citer->second == "true";
        }
    }

    CODBCContext* cntx=  new CODBCContext(version, use_dsn);
    if(cntx && attr) {
        string page_size;
        citer = attr->find("packet");
        if (citer != attr->end()) {
            page_size = citer->second;
        }
        if(!page_size.empty()) {
    SQLUINTEGER s= atoi(page_size.c_str());
    cntx->ODBC_SetPacketSize(s);
      }
    }
    return cntx;
}



///////////////////////////////////////////////////////////////////////////////
const string kDBAPI_ODBC_DriverName("odbc");

class CDbapiOdbcCF2 : public CSimpleClassFactoryImpl<I_DriverContext, CODBCContext>
{
public:
    typedef CSimpleClassFactoryImpl<I_DriverContext, CODBCContext> TParent;

public:
    CDbapiOdbcCF2(void);
    ~CDbapiOdbcCF2(void);

public:
    virtual TInterface*
    CreateInstance(
        const string& driver  = kEmptyStr,
        CVersionInfo version =
        NCBI_INTERFACE_VERSION(I_DriverContext),
        const TPluginManagerParamTree* params = 0) const;

};

CDbapiOdbcCF2::CDbapiOdbcCF2(void)
    : TParent( kDBAPI_ODBC_DriverName, 0 )
{
    return ;
}

CDbapiOdbcCF2::~CDbapiOdbcCF2(void)
{
    return ;
}

CDbapiOdbcCF2::TInterface*
CDbapiOdbcCF2::CreateInstance(
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
        SQLINTEGER odbc_version = SQL_OV_ODBC3;
        bool use_dsn = false;

        // Optional parameters ...
        int page_size = 0;

        if ( params != NULL ) {
            typedef TPluginManagerParamTree::TNodeList_CI TCIter;
            typedef TPluginManagerParamTree::TValueType   TValue;

            // Get parameters ...
            TCIter cit = params->SubNodeBegin();
            TCIter cend = params->SubNodeEnd();

            for (; cit != cend; ++cit) {
                const TValue& v = (*cit)->GetValue();

                if ( v.id == "use_dsn" ) {
                    use_dsn = (v.value != "false");
                } else if ( v.id == "version" ) {
                    int value = NStr::StringToInt( v.value );

                    switch ( value ) {
                    case 3 :
                        odbc_version = SQL_OV_ODBC3;
                        break;
                    case 2 :
                        odbc_version = SQL_OV_ODBC3;
                        break;
                    }
                } else if ( v.id == "packet" ) {
                    page_size = NStr::StringToInt( v.value );
                }
            }
        }

        // Create a driver ...
        drv = new CODBCContext( odbc_version, use_dsn );

        // Set parameters ...
        if ( page_size ) {
            drv->ODBC_SetPacketSize( page_size );
        }
    }
    return drv;
}

///////////////////////////////////////////////////////////////////////////////
void
NCBI_EntryPoint_xdbapi_odbc(
    CPluginManager<I_DriverContext>::TDriverInfoList&   info_list,
    CPluginManager<I_DriverContext>::EEntryPointRequest method)
{
    CHostEntryPointImpl<CDbapiOdbcCF2>::NCBI_EntryPointImpl( info_list, method );
}

NCBI_DBAPIDRIVER_ODBC_EXPORT
void
DBAPI_RegisterDriver_ODBC(void)
{
    RegisterEntryPoint<I_DriverContext>( NCBI_EntryPoint_xdbapi_odbc );
}

///////////////////////////////////////////////////////////////////////////////
NCBI_DBAPIDRIVER_ODBC_EXPORT
void DBAPI_RegisterDriver_ODBC(I_DriverMgr& mgr)
{
    mgr.RegisterDriver("odbc", ODBC_CreateContext);
    DBAPI_RegisterDriver_ODBC();
}

void DBAPI_RegisterDriver_ODBC_old(I_DriverMgr& mgr)
{
    DBAPI_RegisterDriver_ODBC( mgr );
}

extern "C" {
    NCBI_DBAPIDRIVER_ODBC_EXPORT
    void* DBAPI_E_odbc()
    {
        return (void*)DBAPI_RegisterDriver_ODBC_old;
    }
}


END_NCBI_SCOPE



/*
 * ===========================================================================
 * $Log$
 * Revision 1.46  2006/04/05 14:33:16  ssikorsk
 * Improved CODBCContext::x_Close
 *
 * Revision 1.45  2006/03/15 19:56:37  ssikorsk
 * Replaced "static auto_ptr" with "static CSafeStaticPtr".
 *
 * Revision 1.44  2006/03/09 20:37:25  ssikorsk
 * Utilized method I_DriverContext:: CloseAllConn.
 *
 * Revision 1.43  2006/03/06 22:14:59  ssikorsk
 * Added CODBCContext::Close
 *
 * Revision 1.42  2006/03/02 17:20:58  ssikorsk
 * Report errors with SQLFreeHandle(SQL_HANDLE_ENV, m_Context)
 *
 * Revision 1.41  2006/02/28 15:14:30  ssikorsk
 * Replaced argument type SQLINTEGER on SQLLEN where needed.
 *
 * Revision 1.40  2006/02/28 15:00:45  ssikorsk
 * Use larger type (SQLLEN) instead of SQLINTEGER where it needs to be converted to a pointer.
 *
 * Revision 1.39  2006/02/22 15:15:51  ssikorsk
 * *** empty log message ***
 *
 * Revision 1.38  2006/02/17 17:58:47  ssikorsk
 * Initialize Connection::m_BCPable value using connection mode.
 *
 * Revision 1.37  2006/02/09 12:03:19  ssikorsk
 * Set severity level of error messages with native error num == 0 to informational.
 *
 * Revision 1.36  2006/02/07 17:24:01  ssikorsk
 * Added an extra space prior server name in the regular exception string.
 *
 * Revision 1.35  2006/02/01 13:59:19  ssikorsk
 * Report server's and user's names in case of a failed connection attempt.
 *
 * Revision 1.34  2006/01/23 13:42:13  ssikorsk
 * Renamed CODBCContext::MakeConnection to MakeIConnection;
 * Removed connection attribut checking from this method;
 *
 * Revision 1.33  2006/01/04 12:28:35  ssikorsk
 * Fix compilation issues
 *
 * Revision 1.32  2006/01/03 19:02:44  ssikorsk
 * Implement method MakeConnection.
 *
 * Revision 1.31  2005/11/28 13:22:59  ssikorsk
 * Report SQL statement and database connection parameters in case
 * of an error in addition to a server error message.
 *
 * Revision 1.30  2005/11/02 16:46:21  ssikorsk
 * Pass context information with an error message of a database exception.
 *
 * Revision 1.29  2005/11/02 15:59:47  ucko
 * Revert previous change in favor of supplying empty compilation info
 * via a static constant.
 *
 * Revision 1.28  2005/11/02 15:38:07  ucko
 * Replace CDiagCompileInfo() with DIAG_COMPILE_INFO, as the latter
 * automatically fills in some useful information and is less likely to
 * confuse compilers into thinking they're looking at function prototypes.
 *
 * Revision 1.27  2005/11/02 13:37:08  ssikorsk
 * Fixed file merging problems.
 *
 * Revision 1.26  2005/11/02 13:30:34  ssikorsk
 * Do not report function name, file name and line number in case of SQL Server errors.
 *
 * Revision 1.25  2005/11/02 12:58:38  ssikorsk
 * Report extra information in exceptions and error messages.
 *
 * Revision 1.24  2005/10/27 16:48:49  grichenk
 * Redesigned CTreeNode (added search methods),
 * removed CPairTreeNode.
 *
 * Revision 1.23  2005/09/19 14:19:05  ssikorsk
 * Use NCBI_CATCH_ALL macro instead of catch(...)
 *
 * Revision 1.22  2005/09/15 11:00:02  ssikorsk
 * Destructors do not throw exceptions any more.
 *
 * Revision 1.21  2005/06/07 16:22:51  ssikorsk
 * Included <dbapi/driver/driver_mgr.hpp> to make CDllResolver_Getter<I_DriverContext> explicitly visible.
 *
 * Revision 1.20  2005/04/04 13:03:57  ssikorsk
 * Revamp of DBAPI exception class CDB_Exception
 *
 * Revision 1.19  2005/03/21 14:08:30  ssikorsk
 * Fixed the 'version' of a databases protocol parameter handling
 *
 * Revision 1.18  2005/03/02 21:19:20  ssikorsk
 * Explicitly call a new RegisterDriver function from the old one
 *
 * Revision 1.17  2005/03/02 19:29:54  ssikorsk
 * Export new RegisterDriver function on Windows
 *
 * Revision 1.16  2005/03/01 15:22:55  ssikorsk
 * Database driver manager revamp to use "core" CPluginManager
 *
 * Revision 1.15  2004/12/20 19:17:33  ssikorsk
 * Refactoring of dbapi/driver/samples
 *
 * Revision 1.14  2004/05/17 21:16:05  gorelenk
 * Added include of PCH ncbi_pch.hpp
 *
 * Revision 1.13  2004/04/07 13:41:48  gorelenk
 * Added export prefix to implementations of DBAPI_E_* functions.
 *
 * Revision 1.12  2004/03/17 19:25:38  gorelenk
 * Added NCBI_DBAPIDRIVER_ODBC_EXPORT export prefix for definition of
 * DBAPI_RegisterDriver_ODBC .
 *
 * Revision 1.11  2003/11/14 20:46:51  soussov
 * implements DoNotConnect mode
 *
 * Revision 1.10  2003/10/27 17:00:53  soussov
 * adds code to prevent the return of broken connection from the pool
 *
 * Revision 1.9  2003/07/21 21:58:08  soussov
 * fixes bug whith pool name mismatch in Connect()
 *
 * Revision 1.8  2003/07/18 19:20:34  soussov
 * removes SetPacketSize function
 *
 * Revision 1.7  2003/07/17 20:47:10  soussov
 * connections pool improvements
 *
 * Revision 1.6  2003/05/08 20:48:33  soussov
 * adding datadirect-odbc type of connecting string. Rememner that you need ODBCINI environment variable to make it works
 *
 * Revision 1.5  2003/05/05 20:48:29  ucko
 * Check HAVE_ODBCSS_H; fix some typos in an error message.
 *
 * Revision 1.4  2003/04/01 21:51:17  soussov
 * new attribute 'packet=XXX' (where XXX is a packet size) added to ODBC_CreateContext
 *
 * Revision 1.3  2002/07/03 21:48:50  soussov
 * adds DSN support if needed
 *
 * Revision 1.2  2002/07/02 20:52:54  soussov
 * adds RegisterDriver for ODBC
 *
 * Revision 1.1  2002/06/18 22:06:24  soussov
 * initial commit
 *
 *
 * ===========================================================================
 */
