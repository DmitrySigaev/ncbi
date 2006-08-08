#ifndef DBAPI_DRIVER_ODBC___INTERFACES__HPP
#define DBAPI_DRIVER_ODBC___INTERFACES__HPP

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
 * File Description:  Driver for MS-SQL server (odbc version)
 *
 */

#include <dbapi/driver/public.hpp> // Kept for compatibility reasons ...
#include <dbapi/driver/impl/dbapi_impl_context.hpp>
#include <dbapi/driver/impl/dbapi_impl_connection.hpp>
#include <dbapi/driver/impl/dbapi_impl_cmd.hpp>
#include <dbapi/driver/impl/dbapi_impl_result.hpp>
#include <dbapi/driver/util/pointer_pot.hpp>
#include <dbapi/driver/util/parameters.hpp>

#ifdef NCBI_OS_MSWIN
#include <windows.h>
#endif

#if !defined(HAVE_LONG_LONG)  &&  defined(SIZEOF_LONG_LONG)
#  if SIZEOF_LONG_LONG != 0
#    define HAVE_LONG_LONG 1  // needed by UnixODBC
#  endif
#endif

#if defined(FTDS_IN_USE)
#  define HAVE_SQLGETPRIVATEPROFILESTRING 0
#elif defined(NCBI_OS_MSWIN)
#  define HAVE_SQLGETPRIVATEPROFILESTRING 1
#endif

#include <sql.h>
#include <sqlext.h>
#include <sqltypes.h>


BEGIN_NCBI_SCOPE

class CODBCContext;
class CODBC_Connection;
class CODBC_LangCmd;
class CODBC_RPCCmd;
class CODBC_CursorCmd;
class CODBC_BCPInCmd;
class CODBC_SendDataCmd;
class CODBC_RowResult;
class CODBC_ParamResult;
class CODBC_ComputeResult;
class CODBC_StatusResult;
class CODBCContextRegistry;
class CStatementBase;

/////////////////////////////////////////////////////////////////////////////
//
//  CODBC_Reporter::
//
class NCBI_DBAPIDRIVER_ODBC_EXPORT CODBC_Reporter
{
public:
    CODBC_Reporter(CDBHandlerStack* hs,
                   SQLSMALLINT ht,
                   SQLHANDLE h,
                   const CODBC_Reporter* parent_reporter = NULL);
    ~CODBC_Reporter(void);

public:
    void ReportErrors(void) const;
    void SetHandlerStack(CDBHandlerStack* hs) {
        m_HStack = hs;
    }
    void SetHandle(SQLHANDLE h) {
        m_Handle = h;
    }
    void SetHandleType(SQLSMALLINT ht) {
        m_HType = ht;
    }
    void SetExtraMsg(const string& em) {
        m_ExtraMsg = em;
    }
    string GetExtraMsg(void) const;

private:
    CODBC_Reporter(void);

private:
    CDBHandlerStack* m_HStack;
    SQLHANDLE m_Handle;
    SQLSMALLINT m_HType;
    const CODBC_Reporter* m_ParentReporter;
    string m_ExtraMsg;
};


/////////////////////////////////////////////////////////////////////////////
//
//  CODBCContext::
//

class NCBI_DBAPIDRIVER_ODBC_EXPORT CODBCContext : public impl::CDriverContext
{
    friend class CDB_Connection;

public:
    CODBCContext(SQLLEN version = SQL_OV_ODBC3,
                 int tds_version = 80,
                 bool use_dsn = false);
    virtual ~CODBCContext(void);

public:
    //
    // GENERIC functionality (see in <dbapi/driver/interfaces.hpp>)
    //

    virtual bool SetLoginTimeout (unsigned int nof_secs = 0);
    virtual bool SetTimeout      (unsigned int nof_secs = 0);
    virtual bool SetMaxTextImageSize(size_t nof_bytes);

    virtual bool IsAbleTo(ECapability cpb) const {return false;}


    //
    // ODBC specific functionality
    //

    // the following methods are optional (driver will use the default values
    // if not called), the values will affect the new connections only

    virtual void ODBC_SetPacketSize(SQLUINTEGER packet_size);

    virtual SQLHENV ODBC_GetContext(void) const;
    const CODBC_Reporter& GetReporter(void) const
    {
        return m_Reporter;
    }
    int GetTDSVersion(void) const
    {
        return m_TDSVersion;
    }

protected:
    virtual impl::CConnection* MakeIConnection(const SConnAttr& conn_attr);

private:
    SQLHENV         m_Context;
    SQLULEN         m_PacketSize;
    SQLUINTEGER     m_TextImageSize;
    CODBC_Reporter  m_Reporter;
    bool            m_UseDSN;
    CODBCContextRegistry* m_Registry;
    int             m_TDSVersion;

    SQLHDBC x_ConnectToServer(const string&   srv_name,
                   const string&   usr_name,
                   const string&   passwd,
                   TConnectionMode mode);
    void xReportConError(SQLHDBC con);

    void x_AddToRegistry(void);
    void x_RemoveFromRegistry(void);
    void x_SetRegistry(CODBCContextRegistry* registry);
    void x_Close(bool delete_conn = true);
    static string x_MakeFreeTDSVersion(int version);


    friend class CODBCContextRegistry;
};



/////////////////////////////////////////////////////////////////////////////
//
//  CODBC_Connection::
//

class NCBI_DBAPIDRIVER_ODBC_EXPORT CODBC_Connection : public impl::CConnection
{
    friend class CStatementBase;
    friend class CODBCContext;
    friend class CDB_Connection;
    friend class CODBC_LangCmd;
    friend class CODBC_RPCCmd;
    friend class CODBC_CursorCmd;
    friend class CODBC_BCPInCmd;
    friend class CODBC_SendDataCmd;

protected:
    CODBC_Connection(CODBCContext& cntx,
                     SQLHDBC conn,
                     bool reusable,
                     const string& pool_name);
    virtual ~CODBC_Connection(void);

protected:
    virtual bool IsAlive(void);

    virtual CDB_LangCmd*     LangCmd     (const string&   lang_query,
                                          unsigned int    nof_params = 0);
    virtual CDB_RPCCmd*      RPC         (const string&   rpc_name,
                                          unsigned int    nof_args);
    virtual CDB_BCPInCmd*    BCPIn       (const string&   table_name,
                                          unsigned int    nof_columns);
    virtual CDB_CursorCmd*   Cursor      (const string&   cursor_name,
                                          const string&   query,
                                          unsigned int    nof_params,
                                          unsigned int    batch_size = 1);
    virtual CDB_SendDataCmd* SendDataCmd (I_ITDescriptor& desc,
                                          size_t          data_size,
                                          bool            log_it = true);

    virtual bool SendData(I_ITDescriptor& desc, CDB_Image& img,
                          bool log_it = true);
    virtual bool SendData(I_ITDescriptor& desc, CDB_Text&  txt,
                          bool log_it = true);
    virtual bool Refresh(void);
    virtual I_DriverContext::TConnectionMode ConnectMode(void) const;

    void ODBC_SetTimeout(SQLULEN nof_secs);
    void ODBC_SetTextImageSize(SQLULEN nof_bytes);

    CODBC_LangCmd* xLangCmd(const string&   lang_query,
                            unsigned int    nof_params = 0);

    // abort the connection
    // Attention: it is not recommended to use this method unless you absolutely have to.
    // The expected implementation is - close underlying file descriptor[s] without
    // destroing any objects associated with a connection.
    // Returns: true - if succeed
    //          false - if not
    virtual bool Abort(void);

    /// Close an open connection.
    /// Returns: true - if successfully closed an open connection.
    ///          false - if not
    virtual bool Close(void);

protected:
    string GetDiagnosticInfo(void) const
    {
        return m_Reporter.GetExtraMsg();
    }
    void ReportErrors(void)
    {
        m_Reporter.ReportErrors();
    }

private:
    bool x_SendData(CStatementBase& stmt, CDB_Stream& stream);

    SQLHDBC         m_Link;

    CODBC_Reporter  m_Reporter;
};


/////////////////////////////////////////////////////////////////////////////
class CStatementBase
{
public:
    CStatementBase(CODBC_Connection& conn);
    ~CStatementBase(void);

public:
    SQLHSTMT GetHandle(void) const
    {
        return m_Cmd;
    }
    CODBC_Connection& GetConnection(void)
    {
        return *m_ConnectPtr;
    }
    const CODBC_Connection& GetConnection(void) const
    {
        return *m_ConnectPtr;
    }

public:
    void SetDiagnosticInfo(const string& msg)
    {
        m_Reporter.SetExtraMsg( msg );
    }
    string GetDiagnosticInfo(void) const
    {
        return m_Reporter.GetExtraMsg();
    }
    void ReportErrors(void) const
    {
        m_Reporter.ReportErrors();
    }

public:
    // Do not throw exceptions in case of database errors.
    // Exception will be thrown in case of a logical error only.
    // Return false in case of a database error.
    bool CheckRC(int rc) const;
    bool Close(void) const
    {
        return CheckRC( SQLFreeStmt(m_Cmd, SQL_CLOSE) );
    }
    bool Unbind(void) const
    {
        return CheckRC( SQLFreeStmt(m_Cmd, SQL_UNBIND) );
    }
    bool ResetParams(void) const
    {
        return CheckRC( SQLFreeStmt(m_Cmd, SQL_RESET_PARAMS) );
    }

protected:
    SQLLEN  m_RowCount;
    bool    m_WasSent;
    bool    m_HasFailed;

private:
    CODBC_Connection* m_ConnectPtr;
    SQLHSTMT m_Cmd;
    CODBC_Reporter m_Reporter;
};

/////////////////////////////////////////////////////////////////////////////
//
//  CODBC_LangCmd::
//

class NCBI_DBAPIDRIVER_ODBC_EXPORT CODBC_LangCmd :
    public CStatementBase,
    public impl::CBaseCmd
{
    friend class CODBC_Connection;
    friend class CODBC_CursorCmd;
    friend class CODBC_CursorResult;

protected:
    CODBC_LangCmd(
        CODBC_Connection* conn,
        const string& lang_query,
        unsigned int nof_params
        );

public:
    virtual ~CODBC_LangCmd(void);

protected:
    virtual bool Send(void);
    virtual bool WasSent(void) const;
    virtual bool Cancel(void);
    virtual bool WasCanceled(void) const;
    virtual CDB_Result* Result(void);
    virtual bool HasMoreResults(void) const;
    virtual bool HasFailed(void) const;
    virtual int  RowCount(void) const;
    virtual void DumpResults(void);

private:
    bool x_AssignParams(string& cmd, CMemPot& bind_guard, SQLLEN* indicator);
    bool xCheck4MoreResults(void);

    CODBC_RowResult*  m_Res;
    bool              m_hasResults;
};


/////////////////////////////////////////////////////////////////////////////
//
//  CODBC_RPCCmd::
//

class NCBI_DBAPIDRIVER_ODBC_EXPORT CODBC_RPCCmd :
    public CStatementBase,
    public impl::CBaseCmd
{
    friend class CODBC_Connection;

protected:
    CODBC_RPCCmd(CODBC_Connection* conn,
                 const string& proc_name,
                 unsigned int nof_params);
    virtual ~CODBC_RPCCmd(void);

protected:
    virtual bool Send(void);
    virtual bool WasSent(void) const;
    virtual bool Cancel(void);
    virtual bool WasCanceled(void) const;
    virtual CDB_Result* Result(void);
    virtual bool HasMoreResults(void) const;
    virtual bool HasFailed(void) const;
    virtual int  RowCount(void) const;
    virtual void DumpResults(void);

private:
    bool x_AssignParams(string& cmd, string& q_exec, string& q_select,
        CMemPot& bind_guard, SQLLEN* indicator);
    bool xCheck4MoreResults(void);

    bool              m_HasStatus;
    bool              m_hasResults;
    impl::CResult*    m_Res;
};


/////////////////////////////////////////////////////////////////////////////
//
//  CODBC_CursorCmd::
//

class NCBI_DBAPIDRIVER_ODBC_EXPORT CODBC_CursorCmd :
    public CStatementBase,
    public impl::CCursorCmd
{
    friend class CODBC_Connection;

protected:
    CODBC_CursorCmd(CODBC_Connection* conn,
                    const string& cursor_name,
                    const string& query,
                    unsigned int nof_params);
    virtual ~CODBC_CursorCmd(void);

protected:
    virtual bool BindParam(const string& param_name, CDB_Object* param_ptr,
                           bool out_param = false);
    virtual CDB_Result* Open(void);
    virtual bool Update(const string& table_name, const string& upd_query);
    virtual bool UpdateTextImage(unsigned int item_num, CDB_Stream& data,
                 bool log_it = true);
    virtual CDB_SendDataCmd* SendDataCmd(unsigned int item_num, size_t size,
                     bool log_it = true);
    virtual bool Delete(const string& table_name);
    virtual int  RowCount(void) const;
    virtual bool Close(void);

private:
    bool x_AssignParams(bool just_declare = false);
    CDB_ITDescriptor* x_GetITDescriptor(unsigned int item_num);

    CODBC_LangCmd           m_CursCmd;
    auto_ptr<CODBC_LangCmd> m_LCmd;

    unsigned int            m_FetchSize;
    auto_ptr<impl::CResult> m_Res;
};




/////////////////////////////////////////////////////////////////////////////
//
//  CODBC_BCPInCmd::
//
// This class is not implemented yet ...

class NCBI_DBAPIDRIVER_ODBC_EXPORT CODBC_BCPInCmd :
    public CStatementBase,
    public impl::CBaseCmd
{
    friend class CODBC_Connection;

protected:
    CODBC_BCPInCmd(CODBC_Connection* conn,
                   SQLHDBC cmd,
                   const string& table_name,
                   unsigned int nof_columns);
    virtual ~CODBC_BCPInCmd(void);

protected:
    virtual bool Bind(unsigned int column_num, CDB_Object* param_ptr);
    virtual bool Send(void);
    virtual bool WasSent(void) const;
    virtual bool CommitBCPTrans(void);
    virtual bool Cancel(void);
    virtual bool WasCanceled(void) const;
    virtual bool EndBCP(void);
    virtual bool HasFailed(void) const;
    virtual int  RowCount(void) const;

private:
    bool x_AssignParams(void* p);

    SQLHDBC m_Cmd;
    bool    m_WasBound;
    bool    m_HasTextImage;
};


/////////////////////////////////////////////////////////////////////////////
//
//  CODBC_SendDataCmd::
//

class NCBI_DBAPIDRIVER_ODBC_EXPORT CODBC_SendDataCmd :
    public CStatementBase,
    public impl::CSendDataCmd
{
    friend class CODBC_Connection;

protected:
    CODBC_SendDataCmd(CODBC_Connection* conn,
                      CDB_ITDescriptor& descr,
                      size_t nof_bytes,
                      bool logit);
    virtual ~CODBC_SendDataCmd(void);

protected:
    virtual size_t SendChunk(const void* chunk_ptr, size_t nof_bytes);
    virtual bool   Cancel(void);


private:
    void xCancel(void);

    SQLLEN  m_ParamPH;
};


/////////////////////////////////////////////////////////////////////////////
//
//  CODBC_RowResult::
//

class NCBI_DBAPIDRIVER_ODBC_EXPORT CODBC_RowResult : public impl::CResult
{
    friend class CODBC_LangCmd;
    friend class CODBC_RPCCmd;
    friend class CODBC_CursorCmd;
    friend class CODBC_Connection;

protected:
    CODBC_RowResult(
        CStatementBase& stmt,
        SQLSMALLINT nof_cols,
        SQLLEN* row_count
        );
    virtual ~CODBC_RowResult(void);

protected:
    virtual EDB_ResType     ResultType(void) const;
    virtual unsigned int    NofItems(void) const;
    virtual const char*     ItemName    (unsigned int item_num) const;
    virtual size_t          ItemMaxSize (unsigned int item_num) const;
    virtual EDB_Type        ItemDataType(unsigned int item_num) const;
    virtual bool            Fetch(void);
    virtual int             CurrentItemNo(void) const;
    virtual int             GetColumnNum(void) const;
    virtual CDB_Object*     GetItem(CDB_Object* item_buf = 0);
    virtual size_t          ReadItem(void* buffer, size_t buffer_size,
                                     bool* is_null = 0);
    virtual I_ITDescriptor* GetImageOrTextDescriptor(void);
    CDB_ITDescriptor* GetImageOrTextDescriptor(int item_no,
                                               const string& cond);
    virtual bool            SkipItem(void);

    int xGetData(SQLSMALLINT target_type, SQLPOINTER buffer,
        SQLINTEGER buffer_size);
    CDB_Object* xLoadItem(CDB_Object* item_buf);
    CDB_Object* xMakeItem(void);

protected:
    SQLHSTMT GetHandle(void) const
    {
        return m_Stmt.GetHandle();
    }
    void ReportErrors(void)
    {
        m_Stmt.ReportErrors();
    }
    string GetDiagnosticInfo(void) const
    {
        return m_Stmt.GetDiagnosticInfo();
    }
    void Close(void)
    {
        m_Stmt.Close();
    }

private:
    CStatementBase&   m_Stmt;
    int               m_CurrItem;
    bool              m_EOR;
    unsigned int      m_NofCols;
    unsigned int      m_CmdNum;
#define ODBC_COLUMN_NAME_SIZE 80
    typedef struct t_SODBC_ColDescr {
        SQLCHAR     ColumnName[ODBC_COLUMN_NAME_SIZE];
        SQLULEN     ColumnSize;
        SQLSMALLINT DataType;
        SQLSMALLINT DecimalDigits;
    } SODBC_ColDescr;

    SODBC_ColDescr* m_ColFmt;
    SQLLEN* const   m_RowCountPtr;
};



/////////////////////////////////////////////////////////////////////////////
//
//  CODBC_ParamResult::
//  CODBC_ComputeResult::
//  CODBC_StatusResult::
//  CODBC_CursorResult::
//

class NCBI_DBAPIDRIVER_ODBC_EXPORT CODBC_StatusResult :  public CODBC_RowResult
{
    friend class CODBC_RPCCmd;
    friend class CODBC_Connection;

protected:
    CODBC_StatusResult(CStatementBase& stmt);
    virtual ~CODBC_StatusResult(void);

protected:
    virtual EDB_ResType ResultType(void) const;
};

class NCBI_DBAPIDRIVER_ODBC_EXPORT CODBC_ParamResult :  public CODBC_RowResult
{
    friend class CODBC_RPCCmd;
    friend class CODBC_Connection;

protected:
    CODBC_ParamResult(
        CStatementBase& stmt,
        SQLSMALLINT nof_cols
        );
    virtual ~CODBC_ParamResult(void);

protected:
    virtual EDB_ResType ResultType(void) const;
};

class NCBI_DBAPIDRIVER_ODBC_EXPORT CODBC_CursorResult : public impl::CResult
{
    friend class CODBC_CursorCmd;

protected:
    CODBC_CursorResult(CODBC_LangCmd* cmd);
    virtual ~CODBC_CursorResult(void);

protected:
    virtual EDB_ResType     ResultType(void) const;
    virtual unsigned int    NofItems(void) const;
    virtual const char*     ItemName    (unsigned int item_num) const;
    virtual size_t          ItemMaxSize (unsigned int item_num) const;
    virtual EDB_Type        ItemDataType(unsigned int item_num) const;
    virtual bool            Fetch(void);
    virtual int             CurrentItemNo(void) const;
    virtual int             GetColumnNum(void) const;
    virtual CDB_Object*     GetItem(CDB_Object* item_buff = 0);
    virtual size_t          ReadItem(void* buffer, size_t buffer_size,
                                     bool* is_null = 0);
    virtual I_ITDescriptor* GetImageOrTextDescriptor(void);
    virtual bool            SkipItem(void);

protected:
    string GetDiagnosticInfo(void) const
    {
        return m_Cmd->GetDiagnosticInfo();
    }
    CDB_Result* GetCDBResultPtr(void) const
    {
        return m_Res;
    }
    CDB_Result& GetCDBResult(void)
    {
        _ASSERT(m_Res);
        return *m_Res;
    }
    void SetCDBResultPtr(CDB_Result* res)
    {
        _ASSERT(res);
        m_Res = res;
    }

protected:
    // data
    CODBC_LangCmd* m_Cmd;
    CDB_Result*  m_Res;
    bool m_EOR;
};


END_NCBI_SCOPE


#endif  /* DBAPI_DRIVER_ODBC___INTERFACES__HPP */



/*
 * ===========================================================================
 * $Log$
 * Revision 1.40  2006/08/08 15:43:10  ssikorsk
 * + #define HAVE_SQLGETPRIVATEPROFILESTRING
 *
 * Revision 1.39  2006/07/31 22:18:28  ssikorsk
 * Added forward declaration of CStatementBase;
 * Define HAVE_LONG_LONG on base of SIZEOF_LONG_LONG;
 *
 * Revision 1.38  2006/07/31 15:48:34  ssikorsk
 * Minor changes to add support for the FreeTDS odbc driver.
 *
 * Revision 1.37  2006/07/25 13:50:51  ssikorsk
 * Moved declaration of NCBI_EntryPoint_xdbapi_odbc into odbc_utils.hpp.
 *
 * Revision 1.36  2006/07/19 14:09:55  ssikorsk
 * Refactoring of CursorCmd.
 *
 * Revision 1.35  2006/07/18 15:46:00  ssikorsk
 * LangCmd, RPCCmd, and BCPInCmd have common base class impl::CBaseCmd now.
 *
 * Revision 1.34  2006/07/12 17:10:40  ssikorsk
 * Fixed return type of MakeIConnection.
 *
 * Revision 1.33  2006/07/12 16:28:49  ssikorsk
 * Separated interface and implementation of CDB classes.
 *
 * Revision 1.32  2006/06/05 21:01:34  ssikorsk
 * Moved method Release from CODBC_Connection to I_Connection.
 *
 * Revision 1.31  2006/06/05 19:07:21  ssikorsk
 * Declared C...Cmd::Release deprecated.
 *
 * Revision 1.30  2006/06/05 18:04:17  ssikorsk
 * Added method Cancel to CODBC_SendDataCmd.
 *
 * Revision 1.29  2006/06/05 14:36:28  ssikorsk
 * Removed members m_MsgHandlers and m_CMDs from CODBC_Connection.
 *
 * Revision 1.28  2006/06/02 19:34:09  ssikorsk
 * Added methods GetCDBResultPtr, GetCDBResult and SetCDBResultPtr to the
 * CODBC_CursorResult class.
 *
 * Revision 1.27  2006/05/31 16:55:31  ssikorsk
 * Replaced CPointerPot with deque<CDB_BaseEnt*>
 *
 * Revision 1.26  2006/05/15 19:39:20  ssikorsk
 * Added EOwnership argument to method PushMsgHandler.
 *
 * Revision 1.25  2006/04/05 14:22:56  ssikorsk
 * Added CODBC_Connection::Close
 *
 * Revision 1.24  2006/03/06 22:14:23  ssikorsk
 * Added CODBCContext::Close
 *
 * Revision 1.23  2006/02/28 15:13:59  ssikorsk
 * Replaced argument type SQLINTEGER on SQLLEN where needed.
 *
 * Revision 1.22  2006/02/28 15:00:16  ssikorsk
 * Use larger type (SQLLEN) instead of SQLINTEGER where it needs to be converted to a pointer.
 *
 * Revision 1.21  2006/02/28 14:26:35  ssikorsk
 * Replaced int/SQLINTEGER members with SQLLEN where needed.
 *
 * Revision 1.20  2006/02/28 13:59:38  ssikorsk
 * Fixed argument type misuse (like SQLINTEGER and SQLLEN) for vc8-x64 sake.
 *
 * Revision 1.19  2006/01/23 13:17:11  ssikorsk
 * Renamed CODBCContext::MakeConnection to MakeIConnection.
 *
 * Revision 1.18  2006/01/03 18:56:03  ssikorsk
 * Replace method Connect with MakeConnection.
 *
 * Revision 1.17  2005/12/01 14:33:32  ssikorsk
 * Private method CODBC_Connection::x_SendData takes
 * statement instead of connection as a parameter now.
 *
 * Revision 1.16  2005/11/28 13:54:58  ssikorsk
 * Report SQL statement and database connection parameters in case
 * of an error in addition to a server error message.
 *
 * Revision 1.15  2005/11/02 16:38:59  ssikorsk
 * + CODBC_Reporter member to CODBC_CursorResult and CODBC_CursorCmd.
 *
 * Revision 1.14  2005/11/02 12:59:35  ssikorsk
 * Report extra information in exceptions and error messages.
 *
 * Revision 1.13  2005/10/20 12:58:47  ssikorsk
 * Code reformatting
 *
 * Revision 1.12  2005/09/07 11:00:08  ssikorsk
 * Added GetColumnNum method
 *
 * Revision 1.11  2005/03/01 15:21:52  ssikorsk
 * Database driver manager revamp to use "core" CPluginManager
 *
 * Revision 1.10  2005/02/23 21:36:24  soussov
 * Adds Abort() method to connection
 *
 * Revision 1.9  2005/02/15 17:57:19  ssikorsk
 * Fixed RowCountNum() with a SELECT statement
 *
 * Revision 1.8  2004/12/21 22:16:04  soussov
 * fixes bug in SendDataCmd
 *
 * Revision 1.7  2003/07/17 20:43:17  soussov
 * connections pool improvements
 *
 * Revision 1.6  2003/06/06 18:43:15  soussov
 * Removes SetPacketSize()
 *
 * Revision 1.5  2003/06/05 15:57:02  soussov
 * adds DumpResults method for LangCmd and RPC, SetResultProcessor method for Connection interface
 *
 * Revision 1.4  2003/05/05 20:45:51  ucko
 * Lowercase header names for compatibility with Unix.
 *
 * Revision 1.3  2003/02/13 15:43:44  ivanov
 * Added export specifier NCBI_DBAPIDRIVER_ODBC_EXPORT for class definitions
 *
 * Revision 1.2  2002/07/03 21:48:08  soussov
 * adds DSN support if needed
 *
 * Revision 1.1  2002/06/18 22:00:53  soussov
 * initial commit
 *
 * ===========================================================================
 */
