#ifndef DBAPI_DRIVER_DBLIB___INTERFACES__HPP
#define DBAPI_DRIVER_DBLIB___INTERFACES__HPP

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

#ifdef NCBI_OS_MSWIN ////////////////////////////////////////

#define DBNTWIN32          // must identify operating system environment
#include "windows.h"
#include <sqlfront.h>
#include <sqldb.h>     // DB-LIB header file (should always be included)

#define SYBINT1 SQLINT1
#define SYBINT2 SQLINT2
#define SYBINT4 SQLINT4
#define SYBIMAGE     SQLIMAGE
#define SYBTEXT      SQLTEXT
#define SYBBINARY    SQLBINARY
#define SYBDATETIME  SQLDATETIME
#define SYBDATETIME4 SQLDATETIM4
#define SYBNUMERIC   SQLNUMERIC
#define SYBCHAR      SQLCHAR
#define SYBBIT       SQLBIT
#define SYBDECIMAL   SQLDECIMAL
#define SYBREAL      SQLFLT4
#define SYBFLT8      SQLFLT8

#define SYBETIME     SQLETIME
#define SYBEFCON     SQLECONNFB
#define SYBECONN     SQLECONN

// DBSETLENCRYPT
#define DBDATETIME4  DBDATETIM4
#define DBCOLINFO    DBCOL
#define DBTYPEINFO   DBTYPEDEFS

#define DBVERSION_UNKNOWN DBUNKNOWN

#define CS_SUCCEED 0
#define INT_TIMEOUT 1
#define CS_INT Int4

#define DBDATETIME4_days(x) ((x)->numdays)
#define DBDATETIME4_mins(x) ((x)->nummins)
#define DBNUMERIC_val(x) ((x)->val)

#else

#define DBDATETIME4_days(x) ((x)->days)
#define DBDATETIME4_mins(x) ((x)->minutes)
#define DBNUMERIC_val(x) ((x)->array)

#endif NCBI_OS_MSWIN ////////////////////////////////////////


#include <dbapi/driver/public.hpp>
#include <dbapi/driver/util/parameters.hpp>
#include <dbapi/driver/util/handle_stack.hpp>
#include <dbapi/driver/util/pointer_pot.hpp>

#ifndef NCBI_OS_MSWIN ////////////////////////////////////////
#include <sybfront.h>
#include <sybdb.h>
#include <syberror.h>
#endif NCBI_OS_MSWIN ////////////////////////////////////////

BEGIN_NCBI_SCOPE

class CDBLibContext;
class CDBL_Connection;
class CDBL_LangCmd;
class CDBL_RPCCmd;
class CDBL_CursorCmd;
class CDBL_BCPInCmd;
class CDBL_SendDataCmd;
class CDBL_RowResult;
class CDBL_ParamResult;
class CDBL_ComputeResult;
class CDBL_StatusResult;
class CDBL_BlobResult;


const unsigned int kDBLibMaxNameLen = 128 + 4;


/////////////////////////////////////////////////////////////////////////////
//
//  CDBLibContext::
//

class CDBLibContext : public I_DriverContext
{
    friend class CDB_Connection;

public:
    CDBLibContext(DBINT version = DBVERSION_UNKNOWN);

    //
    // GENERIC functionality (see in <dbapi/driver/interfaces.hpp>)
    //

    virtual bool SetLoginTimeout (unsigned int nof_secs = 0);
    virtual bool SetTimeout      (unsigned int nof_secs = 0);
    virtual bool SetMaxTextImageSize(size_t nof_bytes);

    virtual CDB_Connection* Connect(const string&   srv_name,
                                    const string&   user_name,
                                    const string&   passwd,
                                    TConnectionMode mode,
                                    bool            reusable  = false,
                                    const string&   pool_name = kEmptyStr);

    virtual unsigned int NofConnections(const string& srv_name = kEmptyStr)
        const;

    virtual ~CDBLibContext();


    //
    // DBLIB specific functionality
    //

    // the following methods are optional (driver will use the default values if not called)
    // the values will affect the new connections only

    virtual void DBLIB_SetApplicationName(const string& a_name);
    virtual void DBLIB_SetHostName(const string& host_name);
    virtual void DBLIB_SetPacketSize(int p_size);
    virtual bool DBLIB_SetMaxNofConns(int n);
    static  int  DBLIB_dberr_handler(DBPROCESS*    dblink,   int     severity,
                                     int           dberr,    int     oserr,
                                     const string& dberrstr,
                                     const string& oserrstr);
    static  void DBLIB_dbmsg_handler(DBPROCESS*    dblink,   DBINT   msgno,
                                     int           msgstate, int     severity,
                                     const string& msgtxt,
                                     const string& srvname,
                                     const string& procname,
                                     int           line);

private:
    static CDBLibContext* m_pDBLibContext;
    string                m_AppName;
    string                m_HostName;
    short                 m_PacketSize;
    LOGINREC*             m_Login;

    DBPROCESS* x_ConnectToServer(const string&   srv_name,
                                 const string&   user_name,
                                 const string&   passwd,
                                 TConnectionMode mode);
};



/////////////////////////////////////////////////////////////////////////////
//
//  CTL_Connection::
//

class CDBL_Connection : public I_Connection
{
    friend class CDBLibContext;
    friend class CDB_Connection;
    friend class CDBL_LangCmd;
    friend class CDBL_RPCCmd;
    friend class CDBL_CursorCmd;
    friend class CDBL_BCPInCmd;
    friend class CDBL_SendDataCmd;

protected:
    CDBL_Connection(CDBLibContext* cntx, DBPROCESS* con,
                    bool reusable, const string& pool_name);

    virtual bool IsAlive();

    virtual CDB_LangCmd*     LangCmd(const string&       lang_query,
                                     unsigned int        nof_params = 0);
    virtual CDB_RPCCmd*      RPC(const string&           rpc_name,
                                 unsigned int            nof_args);
    virtual CDB_BCPInCmd*    BCPIn(const string&         table_name,
                                   unsigned int          nof_columns);
    virtual CDB_CursorCmd*   Cursor(const string&        cursor_name,
                                    const string&        query,
                                    unsigned int         nof_params,
                                    unsigned int         batch_size = 1);
    virtual CDB_SendDataCmd* SendDataCmd(I_ITDescriptor& desc,
                                         size_t          data_size,
                                         bool            log_it = true);

    virtual bool SendData(I_ITDescriptor& desc, CDB_Image& img,
                          bool log_it = true);
    virtual bool SendData(I_ITDescriptor& desc, CDB_Text&  txt,
                          bool log_it = true);
    virtual bool Refresh();
    virtual const string& ServerName() const;
    virtual const string& UserName()   const;
    virtual const string& Password()   const;
    virtual I_DriverContext::TConnectionMode ConnectMode() const;
    virtual bool IsReusable() const;
    virtual const string& PoolName() const;
    virtual I_DriverContext* Context() const;
    virtual void PushMsgHandler(CDB_UserHandler* h);
    virtual void PopMsgHandler (CDB_UserHandler* h);
    virtual void Release();

    virtual ~CDBL_Connection();

    void DropCmd(CDB_BaseEnt& cmd);

private:
    bool x_SendData(I_ITDescriptor& desc, CDB_Stream& img, bool log_it = true);

    DBPROCESS*      m_Link;
    CDBLibContext*  m_Context;
    CPointerPot     m_CMDs;
    CDBHandlerStack m_MsgHandlers;
    string          m_Server;
    string          m_User;
    string          m_Passwd;
    string          m_Pool;
    bool            m_Reusable;
    bool            m_BCPAble;
    bool            m_SecureLogin;
};



/////////////////////////////////////////////////////////////////////////////
//
//  CDBL_LangCmd::
//

class CDBL_LangCmd : public I_LangCmd
{
    friend class CDBL_Connection;
protected:
    CDBL_LangCmd(CDBL_Connection* conn, DBPROCESS* cmd,
                 const string& lang_query, unsigned int nof_params);

    virtual bool More(const string& query_text);
    virtual bool BindParam(const string& param_name, CDB_Object* param_ptr);
    virtual bool SetParam(const string& param_name, CDB_Object* param_ptr);
    virtual bool Send();
    virtual bool WasSent() const;
    virtual bool Cancel();
    virtual bool WasCanceled() const;
    virtual CDB_Result* Result();
    virtual bool HasMoreResults() const;
    virtual bool HasFailed() const;
    virtual int  RowCount() const;
    virtual void Release();

    virtual ~CDBL_LangCmd();

private:
    bool x_AssignParams();

    CDBL_Connection* m_Connect;
    DBPROCESS*       m_Cmd;
    string           m_Query;
    CDB_Params       m_Params;
    bool             m_WasSent;
    bool             m_HasFailed;
    I_Result*        m_Res;
    int              m_RowCount;
    unsigned int     m_Status;
};



/////////////////////////////////////////////////////////////////////////////
//
//  CTL_RPCCmd::
//

class CDBL_RPCCmd : public I_RPCCmd
{
    friend class CDBL_Connection;
protected:
    CDBL_RPCCmd(CDBL_Connection* con, DBPROCESS* cmd,
                const string& proc_name, unsigned int nof_params);

    virtual bool BindParam(const string& param_name, CDB_Object* param_ptr,
                           bool out_param = false);
    virtual bool SetParam(const string& param_name, CDB_Object* param_ptr,
                          bool out_param = false);
    virtual bool Send();
    virtual bool WasSent() const;
    virtual bool Cancel();
    virtual bool WasCanceled() const;
    virtual CDB_Result* Result();
    virtual bool HasMoreResults() const;
    virtual bool HasFailed() const ;
    virtual int  RowCount() const;
    virtual void SetRecompile(bool recompile = true);
    virtual void Release();

    ~CDBL_RPCCmd();

private:
    bool x_AssignParams(char* param_buff);

    CDBL_Connection* m_Connect;
    DBPROCESS*       m_Cmd;
    string           m_Query;
    CDB_Params       m_Params;
    bool             m_WasSent;
    bool             m_HasFailed;
    bool             m_Recompile;
    I_Result*        m_Res;
    int              m_RowCount;
    unsigned int     m_Status;
};



/////////////////////////////////////////////////////////////////////////////
//
//  CDBL_CursorCmd::
//

class CDBL_CursorCmd : public I_CursorCmd {
    friend class CDBL_Connection;
protected:
    CDBL_CursorCmd(CDBL_Connection* con, DBPROCESS* cmd,
                   const string& cursor_name, const string& query,
                   unsigned int nof_params);

    virtual bool BindParam(const string& param_name, CDB_Object* pVal);
    virtual CDB_Result* Open();
    virtual bool Update(const string& table_name, const string& upd_query);
    virtual bool Delete(const string& table_name);
    virtual int  RowCount() const;
    virtual bool Close();
    virtual void Release();

    virtual ~CDBL_CursorCmd();

private:
    bool x_AssignParams();

    CDBL_Connection* m_Connect;
    DBPROCESS*       m_Cmd;
    string           m_Name;
    CDB_LangCmd*     m_LCmd;
    string           m_Query;
    CDB_Params       m_Params;
    bool             m_IsOpen;
    bool             m_HasFailed;
    bool             m_IsDeclared;
    I_Result*        m_Res;
    int              m_RowCount;
};



/////////////////////////////////////////////////////////////////////////////
//
//  CDBL_BCPInCmd::
//

class CDBL_BCPInCmd : public I_BCPInCmd
{
    friend class CDBL_Connection;
protected:
    CDBL_BCPInCmd(CDBL_Connection* con, DBPROCESS* cmd,
                  const string& table_name, unsigned int nof_columns);

    virtual bool Bind(unsigned int column_num, CDB_Object* param_ptr);
    virtual bool SendRow();
    virtual bool CompleteBatch();
    virtual bool Cancel();
    virtual bool CompleteBCP();
    virtual void Release();

    ~CDBL_BCPInCmd();

private:
    bool x_AssignParams(void* pb);

    CDBL_Connection* m_Connect;
    DBPROCESS*       m_Cmd;
    CDB_Params       m_Params;
    bool             m_WasSent;
    bool             m_HasFailed;
    bool             m_HasTextImage;
    bool             m_WasBound;
};



/////////////////////////////////////////////////////////////////////////////
//
//  CDBL_SendDataCmd::
//

class CDBL_SendDataCmd : public I_SendDataCmd {
    friend class CDBL_Connection;
protected:
    CDBL_SendDataCmd(CDBL_Connection* con, DBPROCESS* cmd, size_t nof_bytes);

    virtual size_t SendChunk(const void* chunk_ptr, size_t nof_bytes);
    virtual void   Release();

    ~CDBL_SendDataCmd();

private:
    CDBL_Connection* m_Connect;
    DBPROCESS*       m_Cmd;
    size_t           m_Bytes2go;
};



/////////////////////////////////////////////////////////////////////////////
//
//  SDBL_ColDescr::
//

struct SDBL_ColDescr
{
    DBINT      max_length;
    EDB_Type   data_type;
    string     col_name;
};



/////////////////////////////////////////////////////////////////////////////
//
//  CDBL_RowResult::
//

class CDBL_RowResult : public I_Result
{
    friend class CDBL_LangCmd;
    friend class CDBL_RPCCmd;
protected:
    CDBL_RowResult(DBPROCESS* cmd, unsigned int* res_status,
                   bool need_init = true);

    virtual EDB_ResType     ResultType() const;
    virtual unsigned int    NofItems() const;
    virtual const char*     ItemName    (unsigned int item_num) const;
    virtual size_t          ItemMaxSize (unsigned int item_num) const;
    virtual EDB_Type        ItemDataType(unsigned int item_num) const;
    virtual bool            Fetch();
    virtual int             CurrentItemNo() const;
    virtual CDB_Object*     GetItem(CDB_Object* item_buf = 0);
    virtual size_t          ReadItem(void* buffer, size_t buffer_size,
                                     bool* is_null = 0);
    virtual I_ITDescriptor* GetImageOrTextDescriptor();
    virtual bool            SkipItem();

    virtual ~CDBL_RowResult();

    // data
    DBPROCESS*     m_Cmd;
    int            m_CurrItem;
    bool           m_EOR;
    unsigned int   m_NofCols;
    int            m_CmdNum;
    unsigned int*  m_ResStatus;
    size_t         m_Offset;
    SDBL_ColDescr* m_ColFmt;
};



/////////////////////////////////////////////////////////////////////////////
//
//  CDBL_BlobResult::
//

class CDBL_BlobResult : public I_Result
{
    friend class CDBL_LangCmd;
    friend class CDBL_RPCCmd;
protected:
    CDBL_BlobResult(DBPROCESS* cmd);

    virtual EDB_ResType     ResultType() const;
    virtual unsigned int    NofItems() const;
    virtual const char*     ItemName    (unsigned int item_num) const;
    virtual size_t          ItemMaxSize (unsigned int item_num) const;
    virtual EDB_Type        ItemDataType(unsigned int item_num) const;
    virtual bool            Fetch();
    virtual int             CurrentItemNo() const;
    virtual CDB_Object*     GetItem(CDB_Object* item_buf = 0);
    virtual size_t          ReadItem(void* buffer, size_t buffer_size,
                                     bool* is_null = 0);
    virtual I_ITDescriptor* GetImageOrTextDescriptor();
    virtual bool            SkipItem();

    virtual ~CDBL_BlobResult();

    // data
    DBPROCESS*    m_Cmd;
    int           m_CurrItem;
    bool          m_EOR;
    int           m_CmdNum;
    char          m_Buff[2048];
    SDBL_ColDescr m_ColFmt;
};



/////////////////////////////////////////////////////////////////////////////
//
//  CDBL_ParamResult::
//  CDBL_ComputeResult::
//  CDBL_StatusResult::
//  CDBL_CursorResult::
//

class CDBL_ParamResult : public CDBL_RowResult
{
    friend class CDBL_LangCmd;
    friend class CDBL_RPCCmd;
protected:
    CDBL_ParamResult(DBPROCESS* cmd, int nof_params);
    virtual EDB_ResType     ResultType() const;
    virtual bool            Fetch();
    virtual CDB_Object*     GetItem(CDB_Object* item_buff = 0);
    virtual size_t          ReadItem(void* buffer, size_t buffer_size,
                                     bool* is_null = 0);
    virtual I_ITDescriptor* GetImageOrTextDescriptor();

    virtual ~CDBL_ParamResult();

    // data
    bool m_1stFetch;
};


class CDBL_ComputeResult : public CDBL_RowResult
{
    friend class CDBL_LangCmd;
    friend class CDBL_RPCCmd;
protected:
    CDBL_ComputeResult(DBPROCESS* cmd, unsigned int* res_stat);
    virtual EDB_ResType     ResultType() const;
    virtual bool            Fetch();
    virtual int             CurrentItemNo() const;
    virtual CDB_Object*     GetItem(CDB_Object* item_buff = 0);
    virtual size_t          ReadItem(void* buffer, size_t buffer_size,
                                     bool* is_null = 0);
    virtual I_ITDescriptor* GetImageOrTextDescriptor();

    virtual ~CDBL_ComputeResult();

    // data
    int  m_ComputeId;
    bool m_1stFetch;
};


class CDBL_StatusResult : public I_Result
{
    friend class CDBL_LangCmd;
    friend class CDBL_RPCCmd;
protected:
    CDBL_StatusResult(DBPROCESS* cmd);
    virtual EDB_ResType     ResultType() const;
    virtual unsigned int    NofItems() const;
    virtual const char*     ItemName    (unsigned int item_num) const;
    virtual size_t          ItemMaxSize (unsigned int item_num) const;
    virtual EDB_Type        ItemDataType(unsigned int item_num) const;
    virtual bool            Fetch();
    virtual int             CurrentItemNo() const ;
    virtual CDB_Object*     GetItem(CDB_Object* item_buff = 0);
    virtual size_t          ReadItem(void* buffer, size_t buffer_size,
                                     bool* is_null = 0);
    virtual I_ITDescriptor* GetImageOrTextDescriptor();
    virtual bool            SkipItem();

    virtual ~CDBL_StatusResult();

    // data
    int    m_Val;
    size_t m_Offset;
    bool   m_1stFetch;
};


class CDBL_CursorResult : public I_Result
{
    friend class CDBL_CursorCmd;
protected:
    CDBL_CursorResult(CDB_LangCmd* cmd);
    virtual EDB_ResType     ResultType() const;
    virtual unsigned int    NofItems() const;
    virtual const char*     ItemName    (unsigned int item_num) const;
    virtual size_t          ItemMaxSize (unsigned int item_num) const;
    virtual EDB_Type        ItemDataType(unsigned int item_num) const;
    virtual bool            Fetch();
    virtual int             CurrentItemNo() const;
    virtual CDB_Object*     GetItem(CDB_Object* item_buff = 0);
    virtual size_t          ReadItem(void* buffer, size_t buffer_size,
                                     bool* is_null = 0);
    virtual I_ITDescriptor* GetImageOrTextDescriptor();
    virtual bool            SkipItem();

    virtual ~CDBL_CursorResult();

    // data
    CDB_LangCmd* m_Cmd;
    CDB_Result*  m_Res;
};



/////////////////////////////////////////////////////////////////////////////
//
//  CDBL_ITDescriptor::
//

class CDBL_ITDescriptor : public I_ITDescriptor
{
    friend class CDBL_RowResult;
    friend class CDBL_BlobResult;
    friend class CDBL_Connection;
public:
    virtual ~CDBL_ITDescriptor();

#ifndef NCBI_OS_MSWIN ////////////////////////////////////////
private:
    bool x_MakeObjName(DBCOLINFO* col_info);
#endif NCBI_OS_MSWIN ////////////////////////////////////////

protected:
    CDBL_ITDescriptor(DBPROCESS* m_link, int col_num);

    // data
    string   m_ObjName;
    DBBINARY m_TxtPtr[DBTXPLEN];
    DBBINARY m_TimeStamp[DBTXTSLEN];
    bool     m_TxtPtr_is_NULL;
    bool     m_TimeStamp_is_NULL;
};


END_NCBI_SCOPE


#endif  /* DBAPI_DRIVER_DBLIB___INTERFACES__HPP */



/*
 * ===========================================================================
 * $Log$
 * Revision 1.4  2002/01/03 17:07:53  sapojnik
 * fixing CR/LF mixup
 *
 * Revision 1.3  2002/01/03 15:46:24  sapojnik
 * ported to MS SQL (about 12 'ifdef NCBI_OS_MSWIN' in 6 files)
 *
 * Revision 1.2  2001/10/24 16:36:05  lavr
 * Type of CDBL_*Result::m_Offset changed to 'size_t'
 *
 * Revision 1.1  2001/10/22 15:17:54  lavr
 * This is a major revamp (by Anton Lavrentiev, with help from Vladimir
 * Soussov and Denis Vakatov) of the DBAPI "driver" libs originally
 * written by Vladimir Soussov. The revamp follows the one of CTLib
 * driver, and it involved massive code shuffling and grooming, numerous
 * local API redesigns, adding comments and incorporating DBAPI to
 * the C++ Toolkit.
 *
 * ===========================================================================
 */
