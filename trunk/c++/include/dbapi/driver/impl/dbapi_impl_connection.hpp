#ifndef DBAPI_DRIVER_IMPL___DBAPI_IMPL_CONNECTION__HPP
#define DBAPI_DRIVER_IMPL___DBAPI_IMPL_CONNECTION__HPP


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
 * Author:  Sergey Sikorskiy
 *
 * File Description:
 *
 */

#include <dbapi/driver/util/handle_stack.hpp>
#include <dbapi/driver/impl/dbapi_driver_utils.hpp>

BEGIN_NCBI_SCOPE

class CDB_Connection;

BEGIN_SCOPE(impl)

////////////////////////////////////////////////////////////////////////////
class CDriverContext;
class CLangCmd;
class CRPCCmd;
class CBCPInCmd;
class CCursorCmd;
class CSendDataCmd;
class CCommand;

/////////////////////////////////////////////////////////////////////////////
///
///  CConnection::
///

class NCBI_DBAPIDRIVER_EXPORT CConnection
{
    friend class impl::CDriverContext;
    friend class ncbi::CDB_Connection; // Because of AttachTo

public:
    CConnection(CDriverContext& dc,
                bool isBCPable = false,
                bool reusable = false,
                const string& pool_name = kEmptyStr,
                bool hasSecureLogin = false
                );
    virtual ~CConnection(void);

    CDB_ResultProcessor* GetResultProcessor(void) const
    {
        return m_ResProc;
    }

    CDriverContext& GetCDriverContext(void)
    {
        _ASSERT(m_DriverContext);
        return *m_DriverContext;
    }
    const CDriverContext& GetCDriverContext(void) const
    {
        _ASSERT(m_DriverContext);
        return *m_DriverContext;
    }

    bool IsMultibyteClientEncoding(void) const;
    EEncoding GetClientEncoding(void) const;

protected:
    /// Check out if connection is alive (this function doesn't ping the server,
    /// it just checks the status of connection which was set by the last
    /// i/o operation)
    virtual bool IsAlive(void) = 0;

    /// These methods:  LangCmd(), RPC(), BCPIn(), Cursor() and SendDataCmd()
    /// create and return a "command" object, register it for later use with
    /// this (and only this!) connection.
    /// On error, an exception will be thrown (they never return NULL!).
    /// It is the user's responsibility to delete the returned "command" object.

    /// Language command
    virtual CDB_LangCmd* LangCmd(const string& lang_query,
                                 unsigned int  nof_params = 0) = 0;
    /// Remote procedure call
    virtual CDB_RPCCmd* RPC(const string& rpc_name,
                            unsigned int  nof_args) = 0;
    /// "Bulk copy in" command
    virtual CDB_BCPInCmd* BCPIn(const string& table_name,
                                unsigned int  nof_columns) = 0;
    /// Cursor
    virtual CDB_CursorCmd* Cursor(const string& cursor_name,
                                  const string& query,
                                  unsigned int  nof_params,
                                  unsigned int  batch_size = 1) = 0;
    /// "Send-data" command
    virtual CDB_SendDataCmd* SendDataCmd(I_ITDescriptor& desc,
                                         size_t          data_size,
                                         bool            log_it = true) = 0;

    /// Shortcut to send text and image to the server without using the
    /// "Send-data" command (SendDataCmd)
    virtual bool SendData(I_ITDescriptor& desc, CDB_Text& txt,
                          bool log_it = true) = 0;
    virtual bool SendData(I_ITDescriptor& desc, CDB_Image& img,
                          bool log_it = true) = 0;

    /// Reset the connection to the "ready" state (cancel all active commands)
    virtual bool Refresh(void) = 0;

    /// Get the server name, user login name, and password
    virtual const string& ServerName(void) const;
    virtual const string& UserName(void) const;
    virtual const string& Password(void) const;

    /// Get the bitmask for the connection mode (BCP, secure login, ...)
    virtual I_DriverContext::TConnectionMode ConnectMode(void) const = 0;

    /// Check if this connection is a reusable one
    virtual bool IsReusable(void) const;

    /// Find out which connection pool this connection belongs to
    virtual const string& PoolName(void) const;

    /// Get pointer to the driver context
    I_DriverContext* Context(void) const;

    /// Put the message handler into message handler stack
    virtual void PushMsgHandler(CDB_UserHandler* h,
                                EOwnership ownership = eNoOwnership);

    /// Remove the message handler (and all above it) from the stack
    virtual void PopMsgHandler(CDB_UserHandler* h);

    CDB_ResultProcessor* SetResultProcessor(CDB_ResultProcessor* rp);

    /// These methods to allow the children of CConnection to create
    /// various command-objects
    CDB_LangCmd*     Create_LangCmd     (CBaseCmd&     lang_cmd    );
    CDB_RPCCmd*      Create_RPCCmd      (CBaseCmd&     rpc_cmd     );
    CDB_BCPInCmd*    Create_BCPInCmd    (CBaseCmd&     bcpin_cmd   );
    CDB_CursorCmd*   Create_CursorCmd   (CCursorCmd&   cursor_cmd  );
    CDB_SendDataCmd* Create_SendDataCmd (CSendDataCmd& senddata_cmd);

    /// abort the connection
    /// Attention: it is not recommended to use this method unless you absolutely have to.
    /// The expected implementation is - close underlying file descriptor[s] without
    /// destroing any objects associated with a connection.
    /// Returns: true - if succeed
    ///          false - if not
    virtual bool Abort(void) = 0;

    /// Close an open connection.
    /// Returns: true - if successfully closed an open connection.
    ///          false - if not
    virtual bool Close(void) = 0;

    virtual void SetTimeout(size_t nof_secs);
    virtual void SetTextImageSize(size_t nof_bytes);


protected:
    void Release(void);
    static CDB_Result* Create_Result(impl::CResult& result);

    const CDBHandlerStack& GetMsgHandlers(void) const
    {
        return m_MsgHandlers;
    }
    CDBHandlerStack& GetMsgHandlers(void)
    {
        return m_MsgHandlers;
    }

    void DropCmd(impl::CCommand& cmd);
    void DeleteAllCommands(void);


    //
    void AttachTo(CDB_Connection* interface);
    void ReleaseInterface(void);

    //
    void DetachResultProcessor(void);

    //
    void SetServerName(const string& name)
    {
        m_Server = name;
    }
    void SetUserName(const string& name)
    {
        m_User = name;
    }
    void SetPassword(const string& pswd)
    {
        m_Passwd = pswd;
    }
    void SetPoolName(const string& name)
    {
        m_Pool = name;
    }
    void SetReusable(bool flag = true)
    {
        m_Reusable = flag;
    }
    void SetBCPable(bool flag = true)
    {
        m_BCPable = flag;
    }
    bool IsBCPable(void) const
    {
        return m_BCPable;
    }
    void SetSecureLogin(bool flag = true)
    {
        m_SecureLogin = flag;
    }
    bool HasSecureLogin(void) const
    {
        return m_SecureLogin;
    }

private:
    typedef deque<impl::CCommand*> TCommandList;

    CDriverContext*                 m_DriverContext;
    CDBHandlerStack                 m_MsgHandlers;
    TCommandList                    m_CMDs;
    CInterfaceHook<CDB_Connection>  m_Interface;
    CDB_ResultProcessor*            m_ResProc;

    string  m_Server;
    string  m_User;
    string  m_Passwd;
    string  m_Pool;
    bool    m_Reusable;
    bool    m_BCPable;
    bool    m_SecureLogin;
};

END_SCOPE(impl)

END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.10  2006/10/26 18:13:53  ssikorsk
 * + CConnection::GetClientEncoding()
 *
 * Revision 1.9  2006/10/23 22:01:50  ssikorsk
 * Added IsMultibyteClientEncoding()
 *
 * Revision 1.8  2006/10/02 19:57:33  ssikorsk
 * Changed visibility of GetCDriverContext to public.
 *
 * Revision 1.7  2006/09/13 19:22:21  ssikorsk
 * Added methods SetTimeout and SetTextImageSize.
 *
 * Revision 1.6  2006/07/18 15:46:00  ssikorsk
 * LangCmd, RPCCmd, and BCPInCmd have common base class impl::CBaseCmd now.
 *
 * Revision 1.5  2006/07/12 19:42:54  ssikorsk
 * - #include <dbapi/driver/public.hpp> Forward declaration of CDB_Connection is enough.
 *
 * Revision 1.4  2006/07/12 19:15:17  ucko
 * Disambiguate friend declarations, and add corresponding top-level
 * predeclarations, for the sake of GCC 4.x.
 *
 * Revision 1.3  2006/07/12 19:10:43  ssikorsk
 * + #include <dbapi/driver/public.hpp> (Necessary for MIPS).
 *
 * Revision 1.2  2006/07/12 18:55:28  ssikorsk
 * Moved implementations of DetachInterface and AttachTo into cpp for MIPS sake.
 *
 * Revision 1.1  2006/07/12 16:28:48  ssikorsk
 * Separated interface and implementation of CDB classes.
 *
 * ===========================================================================
 */


#endif  /* DBAPI_DRIVER_IMPL___DBAPI_IMPL_CONNECTION__HPP */
