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
 * File Description:  Driver for Sybase DBLib server
 *
 */

#include <dbapi/driver/public.hpp>
#include <dbapi/driver/util/parameters.hpp>
#include <dbapi/driver/util/handle_stack.hpp>
#include <dbapi/driver/util/pointer_pot.hpp>

#ifdef MS_DBLIB_IN_USE
#    define CDBLibContext       CMSDBLibContext
#    define CDBL_Connection     CMSDBL_Connection
#    define CDBL_LangCmd        CMSDBL_LangCmd
#    define CDBL_RPCCmd         CMSDBL_RPCCmd
#    define CDBL_CursorCmd      CMSDBL_CursorCmd
#    define CDBL_BCPInCmd       CMSDBL_BCPInCmd
#    define CDBL_SendDataCmd    CMSDBL_SendDataCmd
#    define CDBL_RowResult      CMSDBL_RowResult
#    define CDBL_ParamResult    CMSDBL_ParamResult
#    define CDBL_ComputeResult  CMSDBL_ComputeResult
#    define CDBL_StatusResult   CMSDBL_StatusResult
#    define CDBL_CursorResult   CMSDBL_CursorResult
#    define CDBL_BlobResult     CMSDBL_BlobResult
#    define CDBL_ITDescriptor   CMSDBL_ITDescriptor
#    define SDBL_ColDescr       CMSDBL_ColDescr
#endif // MS_DBLIB_IN_USE

#ifdef FTDS_IN_USE
#    include <dbapi/driver/ftds/ncbi_ftds_rename_sybdb.h>
#    include <cspublic.h>

#    define CDBLibContext       CTDSContext
#    define CDBL_Connection     CTDS_Connection
#    define CDBL_LangCmd        CTDS_LangCmd
#    define CDBL_RPCCmd         CTDS_RPCCmd
#    define CDBL_CursorCmd      CTDS_CursorCmd
#    define CDBL_BCPInCmd       CTDS_BCPInCmd
#    define CDBL_SendDataCmd    CTDS_SendDataCmd
#    define CDBL_RowResult      CTDS_RowResult
#    define CDBL_ParamResult    CTDS_ParamResult
#    define CDBL_ComputeResult  CTDS_ComputeResult
#    define CDBL_StatusResult   CTDS_StatusResult
#    define CDBL_CursorResult   CTDS_CursorResult
#    define CDBL_BlobResult     CTDS_BlobResult
#    define CDBL_ITDescriptor   CTDS_ITDescriptor
#    define SDBL_ColDescr       STDS_ColDescr

#    define DBLIB_dberr_handler TDS_dberr_handler
#    define DBLIB_dbmsg_handler TDS_dbmsg_handler

#    define DBLIB_SetApplicationName    TDS_SetApplicationName
#    define DBLIB_SetHostName           TDS_SetHostName
#    define DBLIB_SetPacketSize         TDS_SetPacketSize
#    define DBLIB_SetMaxNofConns        TDS_SetMaxNofConns

#endif // FTDS_IN_USE


#ifdef MS_DBLIB_IN_USE
    #include <windows.h>

    #define DBNTWIN32             /* must be defined before sqlfront.h */
    #include <sqlfront.h>         /* must be after windows.h */

    #if defined(_MSC_VER)  &&  (_MSC_VER > 1200)
        typedef const BYTE *LPCBYTE;  /* MSVC7 headers lucks typedef for LPCBYTE */
    #endif

    #include <sqldb.h>
#else
    #include <sybfront.h>
    #include <sybdb.h>
    #include <syberror.h>
#endif // MS_DBLIB_IN_USE



BEGIN_NCBI_SCOPE


#ifdef MS_DBLIB_IN_USE
#   define DBVERSION_UNKNOWN DBUNKNOWN
#   define DBVERSION_46 DBVER42
#   define DBVERSION_100 DBVER60
#   define DBCOLINFO    DBCOL
#endif

#ifdef FTDS_IN_USE
#    define DEFAULT_TDS_VERSION DBVERSION_UNKNOWN
#else
#    define DEFAULT_TDS_VERSION DBVERSION_46
#endif // FTDS_IN_USE

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
class CDBL_CursorResult;
class CDBL_BlobResult;


const unsigned int kDBLibMaxNameLen = 128 + 4;

#ifdef FTDS_IN_USE
const unsigned int kTDSMaxNameLen = kDBLibMaxNameLen;
#endif

/////////////////////////////////////////////////////////////////////////////
///
///  CDBLibContext::
///

class NCBI_DBAPIDRIVER_DBLIB_EXPORT CDBLibContext : public I_DriverContext
{
    friend class CDB_Connection;

public:
    CDBLibContext(DBINT version = DEFAULT_TDS_VERSION);
    virtual ~CDBLibContext(void);

public:
    ///
    /// GENERIC functionality (see in <dbapi/driver/interfaces.hpp>)
    ///

    virtual bool SetLoginTimeout (unsigned int nof_secs = 0);
    virtual bool SetTimeout      (unsigned int nof_secs = 0);
    virtual bool SetMaxTextImageSize(size_t nof_bytes);

    virtual bool IsAbleTo(ECapability cpb) const;

    ///
    /// DBLIB specific functionality
    ///

    /// the following methods are optional (driver will use the default
    /// values if not called)
    /// the values will affect the new connections only

    /// Deprecated. Use SetApplicationName instead.
    virtual void DBLIB_SetApplicationName(const string& a_name);
    /// Deprecated. Use SetHostName instead.
    virtual void DBLIB_SetHostName(const string& host_name);
    virtual void DBLIB_SetPacketSize(int p_size);
    virtual bool DBLIB_SetMaxNofConns(int n);

public:
#ifdef FTDS_IN_USE
    // Function name keept same as with ftds.
    unsigned int TDS_GetTimeout(void) {
        return m_Timeout;
    }
#endif

public:
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

    void SetClientCharset(const char* charset) const;
    CDBLibContext* GetContext(void) const;

public:
    bool ConnectedToMSSQLServer(void) const;
    int GetTDSVersion(void) const;
    
protected:
    virtual I_Connection* MakeIConnection(const SConnAttr& conn_attr);
    
private:
    short                 m_PacketSize;
    LOGINREC*             m_Login;
    int                   m_TDSVersion;

#ifdef FTDS_IN_USE
    unsigned int          m_LoginTimeout; // inherited from ftds
    unsigned int          m_Timeout; // inherited from ftds
#endif    

    DBPROCESS* x_ConnectToServer(const string&   srv_name,
                                 const string&   user_name,
                                 const string&   passwd,
                                 TConnectionMode mode);
};



/////////////////////////////////////////////////////////////////////////////
///
///  CTL_Connection::
///

class NCBI_DBAPIDRIVER_DBLIB_EXPORT CDBL_Connection : public I_Connection
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
    virtual ~CDBL_Connection(void);

protected:
    virtual bool IsAlive(void);

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
    virtual bool Refresh(void);
    virtual const string& ServerName(void) const;
    virtual const string& UserName(void)   const;
    virtual const string& Password(void)   const;
    virtual I_DriverContext::TConnectionMode ConnectMode(void) const;
    virtual bool IsReusable(void) const;
    virtual const string& PoolName(void) const;
    virtual I_DriverContext* Context(void) const;
    virtual void PushMsgHandler(CDB_UserHandler* h);
    virtual void PopMsgHandler (CDB_UserHandler* h);
    virtual CDB_ResultProcessor* SetResultProcessor(CDB_ResultProcessor* rp);
    virtual void Release(void);

    void DropCmd(CDB_BaseEnt& cmd);

    /// abort the connection
    /// Attention: it is not recommended to use this method unless you absolutely have to.
    /// The expected implementation is - close underlying file descriptor[s] without
    /// destroing any objects associated with a connection.
    /// Returns: true - if succeed
    ///          false - if not
    virtual bool Abort(void);

private:
    bool x_SendData(I_ITDescriptor& desc, CDB_Stream& img, bool log_it = true);
    I_ITDescriptor* x_GetNativeITDescriptor(const CDB_ITDescriptor& descr_in);
    RETCODE x_Results(DBPROCESS* pLink);

#ifdef FTDS_IN_USE
    /// Function name keept same as with ftds.
    void TDS_SetTimeout(void);
#endif    

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
    CDB_ResultProcessor* m_ResProc;
};



/////////////////////////////////////////////////////////////////////////////
///
///  CDBL_LangCmd::
///

class NCBI_DBAPIDRIVER_DBLIB_EXPORT CDBL_LangCmd : public I_LangCmd
{
    friend class CDBL_Connection;
    
protected:
    CDBL_LangCmd(CDBL_Connection* conn, DBPROCESS* cmd,
                 const string& lang_query, unsigned int nof_params);
    virtual ~CDBL_LangCmd(void);

protected:
    virtual bool More(const string& query_text);
    virtual bool BindParam(const string& param_name, CDB_Object* param_ptr);
    virtual bool SetParam(const string& param_name, CDB_Object* param_ptr);
    virtual bool Send(void);
    virtual bool WasSent(void) const;
    virtual bool Cancel(void);
    virtual bool WasCanceled(void) const;
    virtual CDB_Result* Result(void);
    virtual bool HasMoreResults(void) const;
    virtual bool HasFailed(void) const;
    virtual int  RowCount(void) const;
    virtual void DumpResults(void);
    virtual void Release(void);

private:
    I_Result* GetResultSet(void) const;
    void SetResultSet(I_Result* res);
    void ClearResultSet(void);
    
private:
    bool x_AssignParams(void);

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
///
///  CTL_RPCCmd::
///

class NCBI_DBAPIDRIVER_DBLIB_EXPORT CDBL_RPCCmd : public I_RPCCmd
{
    friend class CDBL_Connection;
    
protected:
    CDBL_RPCCmd(CDBL_Connection* con, DBPROCESS* cmd,
                const string& proc_name, unsigned int nof_params);
    ~CDBL_RPCCmd(void);

protected:
    virtual bool BindParam(const string& param_name, CDB_Object* param_ptr,
                           bool out_param = false);
    virtual bool SetParam(const string& param_name, CDB_Object* param_ptr,
                          bool out_param = false);
    virtual bool Send(void);
    virtual bool WasSent(void) const;
    virtual bool Cancel(void);
    virtual bool WasCanceled(void) const;
    virtual CDB_Result* Result(void);
    virtual bool HasMoreResults(void) const;
    virtual bool HasFailed(void) const ;
    virtual int  RowCount(void) const;
    virtual void DumpResults(void);
    virtual void SetRecompile(bool recompile = true);
    virtual void Release(void);

private:
    I_Result* GetResultSet(void) const;
    void SetResultSet(I_Result* res);
    void ClearResultSet(void);
    
private:
    bool x_AssignParams(char* param_buff);
    
#ifdef FTDS_IN_USE
    bool x_AddParamValue(string& cmd, const CDB_Object& param);
    bool x_AssignOutputParams(void);
    bool x_AssignParams(void);
#endif

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
///
///  CDBL_CursorCmd::
///

class NCBI_DBAPIDRIVER_DBLIB_EXPORT CDBL_CursorCmd : public I_CursorCmd
{
    friend class CDBL_Connection;
    
protected:
    CDBL_CursorCmd(CDBL_Connection* con, DBPROCESS* cmd,
                   const string& cursor_name, const string& query,
                   unsigned int nof_params);
    virtual ~CDBL_CursorCmd(void);

protected:
    virtual bool BindParam(const string& param_name, CDB_Object* pVal);
    virtual CDB_Result* Open(void);
    virtual bool Update(const string& table_name, const string& upd_query);
    virtual bool UpdateTextImage(unsigned int item_num, CDB_Stream& data,
                 bool log_it = true);
    virtual CDB_SendDataCmd* SendDataCmd(unsigned int item_num, size_t size,
                     bool log_it = true);
    virtual bool Delete(const string& table_name);
    virtual int  RowCount(void) const;
    virtual bool Close(void);
    virtual void Release(void);

private:
    CDBL_CursorResult* GetResultSet(void) const;
    void SetResultSet(CDBL_CursorResult* res);
    void ClearResultSet(void);
    
private:
    bool x_AssignParams(void);
    I_ITDescriptor* x_GetITDescriptor(unsigned int item_num);

    CDBL_Connection*   m_Connect;
    DBPROCESS*         m_Cmd;
    string             m_Name;
    CDB_LangCmd*       m_LCmd;
    string             m_Query;
    CDB_Params         m_Params;
    bool               m_IsOpen;
    bool               m_HasFailed;
    bool               m_IsDeclared;
    CDBL_CursorResult* m_Res;
    int                m_RowCount;
};



/////////////////////////////////////////////////////////////////////////////
///
///  CDBL_BCPInCmd::
///

class NCBI_DBAPIDRIVER_DBLIB_EXPORT CDBL_BCPInCmd : public I_BCPInCmd
{
    friend class CDBL_Connection;
    
protected:
    CDBL_BCPInCmd(CDBL_Connection* con, DBPROCESS* cmd,
                  const string& table_name, unsigned int nof_columns);
    ~CDBL_BCPInCmd(void);

protected:
    virtual bool Bind(unsigned int column_num, CDB_Object* param_ptr);
    virtual bool SendRow(void);
    virtual bool CompleteBatch(void);
    virtual bool Cancel(void);
    virtual bool CompleteBCP(void);
    virtual void Release(void);

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
///
///  CDBL_SendDataCmd::
///

class NCBI_DBAPIDRIVER_DBLIB_EXPORT CDBL_SendDataCmd : public I_SendDataCmd 
{
    friend class CDBL_Connection;
    
protected:
    CDBL_SendDataCmd(CDBL_Connection* con, DBPROCESS* cmd, size_t nof_bytes);
    ~CDBL_SendDataCmd(void);

protected:
    virtual size_t SendChunk(const void* chunk_ptr, size_t nof_bytes);
    virtual void   Release(void);

private:
    CDBL_Connection* m_Connect;
    DBPROCESS*       m_Cmd;
    size_t           m_Bytes2go;
};



/////////////////////////////////////////////////////////////////////////////
///
///  SDBL_ColDescr::
///

struct SDBL_ColDescr
{
    DBINT      max_length;
    EDB_Type   data_type;
    string     col_name;
};



/////////////////////////////////////////////////////////////////////////////
///
///  CDBL_RowResult::
///

class NCBI_DBAPIDRIVER_DBLIB_EXPORT CDBL_RowResult : public I_Result
{
    friend class CDBL_LangCmd;
    friend class CDBL_RPCCmd;
    friend class CDBL_Connection;
    
protected:
    CDBL_RowResult(DBPROCESS* cmd, unsigned int* res_status,
                   bool need_init = true);
    virtual ~CDBL_RowResult(void);

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
    virtual bool            SkipItem(void);

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
///
///  CDBL_BlobResult::
///

class NCBI_DBAPIDRIVER_DBLIB_EXPORT CDBL_BlobResult : public I_Result
{
    friend class CDBL_LangCmd;
    friend class CDBL_RPCCmd;
    friend class CDBL_Connection;
    
protected:
    CDBL_BlobResult(DBPROCESS* cmd);
    virtual ~CDBL_BlobResult(void);

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
    virtual bool            SkipItem(void);

    // data
    DBPROCESS*    m_Cmd;
    int           m_CurrItem;
    bool          m_EOR;
    int           m_CmdNum;
    char          m_Buff[2048];
    SDBL_ColDescr m_ColFmt;
    int           m_BytesInBuffer;
    int           m_ReadedBytes;
};



/////////////////////////////////////////////////////////////////////////////
///
///  CDBL_ParamResult::
///

class NCBI_DBAPIDRIVER_DBLIB_EXPORT CDBL_ParamResult : public CDBL_RowResult
{
    friend class CDBL_LangCmd;
    friend class CDBL_RPCCmd;
    friend class CDBL_Connection;
    
protected:
    CDBL_ParamResult(DBPROCESS* cmd, int nof_params);
    virtual ~CDBL_ParamResult(void);
    
protected:
    virtual EDB_ResType     ResultType(void) const;
    virtual bool            Fetch(void);
    virtual CDB_Object*     GetItem(CDB_Object* item_buff = 0);
    virtual size_t          ReadItem(void* buffer, size_t buffer_size,
                                     bool* is_null = 0);
    virtual I_ITDescriptor* GetImageOrTextDescriptor(void);

    // data
    bool m_1stFetch;
};


/////////////////////////////////////////////////////////////////////////////
///
///  CDBL_ComputeResult::
///

class NCBI_DBAPIDRIVER_DBLIB_EXPORT CDBL_ComputeResult : public CDBL_RowResult
{
    friend class CDBL_LangCmd;
    friend class CDBL_RPCCmd;
    friend class CDBL_Connection;
    
protected:
    CDBL_ComputeResult(DBPROCESS* cmd, unsigned int* res_stat);
    virtual ~CDBL_ComputeResult(void);
    
protected:
    virtual EDB_ResType     ResultType(void) const;
    virtual bool            Fetch(void);
    virtual int             CurrentItemNo(void) const;
    virtual CDB_Object*     GetItem(CDB_Object* item_buff = 0);
    virtual size_t          ReadItem(void* buffer, size_t buffer_size,
                                     bool* is_null = 0);
    virtual I_ITDescriptor* GetImageOrTextDescriptor(void);

    // data
    int  m_ComputeId;
    bool m_1stFetch;
};


/////////////////////////////////////////////////////////////////////////////
///
///  CDBL_StatusResult::
///

class NCBI_DBAPIDRIVER_DBLIB_EXPORT CDBL_StatusResult : public I_Result
{
    friend class CDBL_LangCmd;
    friend class CDBL_RPCCmd;
    friend class CDBL_Connection;
    
protected:
    CDBL_StatusResult(DBPROCESS* cmd);
    virtual ~CDBL_StatusResult(void);
    
protected:
    virtual EDB_ResType     ResultType(void) const;
    virtual unsigned int    NofItems(void) const;
    virtual const char*     ItemName    (unsigned int item_num) const;
    virtual size_t          ItemMaxSize (unsigned int item_num) const;
    virtual EDB_Type        ItemDataType(unsigned int item_num) const;
    virtual bool            Fetch(void);
    virtual int             CurrentItemNo(void) const ;
    virtual int             GetColumnNum(void) const;
    virtual CDB_Object*     GetItem(CDB_Object* item_buff = 0);
    virtual size_t          ReadItem(void* buffer, size_t buffer_size,
                                     bool* is_null = 0);
    virtual I_ITDescriptor* GetImageOrTextDescriptor(void);
    virtual bool            SkipItem(void);

    // data
    int    m_Val;
    size_t m_Offset;
    bool   m_1stFetch;
};


/////////////////////////////////////////////////////////////////////////////
///
///  CDBL_CursorResult::
///

class NCBI_DBAPIDRIVER_DBLIB_EXPORT CDBL_CursorResult : public I_Result
{
    friend class CDBL_CursorCmd;
    
protected:
    CDBL_CursorResult(CDB_LangCmd* cmd);
    virtual ~CDBL_CursorResult(void);
    
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

private:
    CDB_Result* GetResultSet(void) const;
    void SetResultSet(CDB_Result* res);
    void ClearResultSet(void);
    void DumpResultSet(void);
    void FetchAllResultSet(void);
    
private:
    // data
    CDB_LangCmd* m_Cmd;
    CDB_Result*  m_Res;
};



/////////////////////////////////////////////////////////////////////////////
///
///  CDBL_ITDescriptor::
///

#if defined(MS_DBLIB_IN_USE)
#    define CDBL_ITDESCRIPTOR_TYPE_MAGNUM 0xd01
#    define CMSDBL_ITDESCRIPTOR_TYPE_MAGNUM CDBL_ITDESCRIPTOR_TYPE_MAGNUM
#elif defined(FTDS_IN_USE)
#    define CDBL_ITDESCRIPTOR_TYPE_MAGNUM 0xf00
#    define CTDS_ITDESCRIPTOR_TYPE_MAGNUM CDBL_ITDESCRIPTOR_TYPE_MAGNUM
#else
#    define CDBL_ITDESCRIPTOR_TYPE_MAGNUM 0xd00
#endif

class NCBI_DBAPIDRIVER_DBLIB_EXPORT CDBL_ITDescriptor : public I_ITDescriptor
{
    friend class CDBL_RowResult;
    friend class CDBL_BlobResult;
    friend class CDBL_Connection;
    friend class CDBL_CursorCmd;
    
protected:
    CDBL_ITDescriptor(DBPROCESS* m_link, int col_num);
    CDBL_ITDescriptor(DBPROCESS* m_link, const CDB_ITDescriptor& inp_d);

public:
    virtual ~CDBL_ITDescriptor(void);

public:
    virtual int DescriptorType(void) const;
    
private:
#ifndef FTDS_IN_USE
    bool x_MakeObjName(DBCOLINFO* col_info);
#endif

protected:
    // data
    string   m_ObjName;
    DBBINARY m_TxtPtr[DBTXPLEN];
    DBBINARY m_TimeStamp[DBTXTSLEN];
    bool     m_TxtPtr_is_NULL;
    bool     m_TimeStamp_is_NULL;
};


/////////////////////////////////////////////////////////////////////////////
#if defined(MS_DBLIB_IN_USE)

extern NCBI_DBAPIDRIVER_DBLIB_EXPORT const string kDBAPI_MSDBLIB_DriverName;

extern "C"
{

NCBI_DBAPIDRIVER_DBLIB_EXPORT
void
NCBI_EntryPoint_xdbapi_msdblib(
    CPluginManager<I_DriverContext>::TDriverInfoList&   info_list,
    CPluginManager<I_DriverContext>::EEntryPointRequest method);

} // extern C

#elif defined(FTDS_IN_USE)

// Uncomment a line below if you want to simulate a previous ftds driver logic.
// #define FTDS_LOGIC 

extern NCBI_DBAPIDRIVER_DBLIB_EXPORT const string kDBAPI_FTDS_DriverName;

extern "C"
{

NCBI_DBAPIDRIVER_DBLIB_EXPORT
void
NCBI_EntryPoint_xdbapi_ftds(
    CPluginManager<I_DriverContext>::TDriverInfoList&   info_list,
    CPluginManager<I_DriverContext>::EEntryPointRequest method);

NCBI_DBAPIDRIVER_DBLIB_EXPORT
void
NCBI_EntryPoint_xdbapi_ftds63(
    CPluginManager<I_DriverContext>::TDriverInfoList&   info_list,
    CPluginManager<I_DriverContext>::EEntryPointRequest method);

} // extern C 

#else

extern NCBI_DBAPIDRIVER_DBLIB_EXPORT const string kDBAPI_DBLIB_DriverName;

extern "C"
{

NCBI_DBAPIDRIVER_DBLIB_EXPORT
void
NCBI_EntryPoint_xdbapi_dblib(
    CPluginManager<I_DriverContext>::TDriverInfoList&   info_list,
    CPluginManager<I_DriverContext>::EEntryPointRequest method);

} // extern C

#endif

inline
I_Result* 
CDBL_LangCmd::GetResultSet(void) const
{
    return m_Res;
}

inline
void 
CDBL_LangCmd::SetResultSet(I_Result* res)
{
    m_Res = res;
}

inline
void 
CDBL_LangCmd::ClearResultSet(void)
{
    delete m_Res;
    m_Res = NULL;
}

inline
CDBL_CursorResult* 
CDBL_CursorCmd::GetResultSet(void) const
{
    return m_Res;
}

inline
void 
CDBL_CursorCmd::SetResultSet(CDBL_CursorResult* res)
{
    m_Res = res;
}

inline
void 
CDBL_CursorCmd::ClearResultSet(void)
{
    delete m_Res;
    m_Res = NULL;
}

inline
CDB_Result* 
CDBL_CursorResult::GetResultSet(void) const
{
    return m_Res;
}

inline
void 
CDBL_CursorResult::SetResultSet(CDB_Result* res)
{
    m_Res = res;
}

inline
void 
CDBL_CursorResult::ClearResultSet(void)
{
   delete m_Res;
    m_Res = NULL;
}

inline
void 
CDBL_CursorResult::DumpResultSet(void)
{
    SetResultSet( m_Cmd->Result() );
    FetchAllResultSet();
}

inline
void
CDBL_CursorResult::FetchAllResultSet(void)
{
    if (GetResultSet()) {
        while (GetResultSet()->Fetch())
            continue;
        ClearResultSet();
    }
}

inline
I_Result* 
CDBL_RPCCmd::GetResultSet(void) const
{
    return m_Res;
}

inline
void 
CDBL_RPCCmd::SetResultSet(I_Result* res)
{
    m_Res = res;
}

inline
void 
CDBL_RPCCmd::ClearResultSet(void)
{
    delete m_Res;
    m_Res = NULL;
}


END_NCBI_SCOPE


#endif  /* DBAPI_DRIVER_DBLIB___INTERFACES__HPP */



/*
 * ===========================================================================
 * $Log$
 * Revision 1.31  2006/01/23 13:14:04  ssikorsk
 * Renamed CDBLibContext::MakeConnection to MakeIConnection.
 *
 * Revision 1.30  2006/01/03 18:55:30  ssikorsk
 * Replace method Connect with MakeConnection.
 *
 * Revision 1.29  2005/12/06 19:21:52  ssikorsk
 * Added private methods GetResultSet/SetResultSet/ClearResultSet
 * to all *command* classes
 *
 * Revision 1.28  2005/10/31 12:16:00  ssikorsk
 * Merged msdblib/interfaces.hpp into dblib/interfaces.hpp
 *
 * Revision 1.27  2005/10/20 12:54:35  ssikorsk
 * - CDBLibContext::m_AppName
 * - CDBLibContext::m_HostName
 *
 * Revision 1.26  2005/09/14 14:10:00  ssikorsk
 * Add ConnectedToMSSQLServer and GetTDSVersion methods to the CDBLibContext class
 *
 * Revision 1.25  2005/09/07 11:00:07  ssikorsk
 * Added GetColumnNum method
 *
 * Revision 1.24  2005/08/09 14:57:26  ssikorsk
 * Added the FTDS_LOGIC define to be able to turn on an old ftds driver logic
 *
 * Revision 1.23  2005/07/20 13:49:19  ssikorsk
 * Removed ftds7 code. Declared constants and defines from ftds to reach full compatibility.
 *
 * Revision 1.22  2005/07/20 12:33:04  ssikorsk
 * Merged ftds/interfaces.hpp into dblib/interfaces.hpp
 *
 * Revision 1.21  2005/07/07 15:43:20  ssikorsk
 * Integrated interfaces with FreeTDS v0.63
 *
 * Revision 1.20  2005/03/01 15:21:52  ssikorsk
 * Database driver manager revamp to use "core" CPluginManager
 *
 * Revision 1.19  2005/02/23 21:33:31  soussov
 * Adds Abort() method to connection
 *
 * Revision 1.18  2003/07/17 20:42:02  soussov
 * connections pool improvements
 *
 * Revision 1.17  2003/06/05 15:55:34  soussov
 * adds DumpResults method for LangCmd and RPC, SetResultProcessor method for Connection interface
 *
 * Revision 1.16  2003/04/01 20:27:17  vakatov
 * Temporarily rollback to R1.14 -- until more backward-incompatible
 * changes (in CException) are ready to commit (to avoid breaking the
 * compatibility twice).
 *
 * Revision 1.14  2003/02/13 15:43:00  ivanov
 * Added export specifier NCBI_DBAPIDRIVER_DBLIB_EXPORT for class definitions
 *
 * Revision 1.13  2002/07/02 16:03:28  soussov
 * splitting Sybase dblib and MS dblib
 *
 * Revision 1.12  2002/06/21 14:33:19  soussov
 * defines version for microsoft
 *
 * Revision 1.11  2002/06/19 16:46:31  soussov
 * changes default version from unknown to 46
 *
 * Revision 1.10  2002/05/29 22:04:43  soussov
 * Makes BlobResult read ahead
 *
 * Revision 1.9  2002/03/28 00:39:49  vakatov
 * CDBL_CursorCmd::  use CDBL_CursorResult rather than I_Result (to fix access)
 *
 * Revision 1.8  2002/03/26 15:26:23  soussov
 * new image/text operations added
 *
 * Revision 1.7  2002/01/14 20:26:15  sapojnik
 * new SampleDBAPI_Blob, etc.
 *
 * Revision 1.6  2002/01/08 18:09:39  sapojnik
 * Syabse to MSSQL name translations moved to interface_p.hpp
 *
 * Revision 1.5  2002/01/08 15:58:00  sapojnik
 * Moved to const_syb2ms.hpp: Sybase dblib constants translations to
 * Microsoft-compatible ones
 *
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
