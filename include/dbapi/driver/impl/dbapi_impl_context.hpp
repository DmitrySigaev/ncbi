#ifndef DBAPI_DRIVER_IMPL___DBAPI_IMPL_CONTEXT__HPP
#define DBAPI_DRIVER_IMPL___DBAPI_IMPL_CONTEXT__HPP

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

#include <corelib/ncbistd.hpp>
#include <corelib/ncbimtx.hpp>

#include <dbapi/driver/interfaces.hpp>
#include <dbapi/driver/util/handle_stack.hpp>

BEGIN_NCBI_SCOPE

class CDB_Connection;
class IConnValidator;
class CDB_UserHandler;
class CDB_Connection;

BEGIN_SCOPE(impl)

/////////////////////////////////////////////////////////////////////////////
// Forward declarations

class CConnection;

/////////////////////////////////////////////////////////////////////////////
///
///  CDriverContext::
///

class NCBI_DBAPIDRIVER_EXPORT CDriverContext : public I_DriverContext
{
    friend class impl::CConnection;

protected:
    CDriverContext(void);

public:
    virtual ~CDriverContext(void);

    virtual bool SetTimeout(unsigned int nof_secs = 0);
    virtual bool SetMaxTextImageSize(size_t nof_bytes);

    /// Create new connection to specified server (within this context).
    /// It is your responsibility to delete the returned connection object.
    /// reusable - controls connection pooling mechanism. If it is set to true
    /// then a connection will be added to a pool  of connections instead of
    /// closing.
    ///
    /// pool_name - name of a pool to which this connection is going to belong.
    ///
    /// srv_name, user_name and passwd may be set to empty string.
    ///
    /// If pool_name is provided then connection will be taken from a pool
    /// having this name if a pool is not empty.
    /// It is your responsibility to put connections with the same
    /// server/user/password values in a pool.
    /// If a pool name is not provided but a server name (srv_name) is provided
    /// instead then connection with the same name will be taken from a pool of
    /// connections if a pool is not empty.
    /// If a pool is empty then new connection will be created unless you passed
    /// mode = fDoNotConnect. In this case NULL will be returned.
    /// If you did not provide either a pool name or a server name then NULL will
    /// be returned.
    virtual CDB_Connection* Connect
    (const string&   srv_name,
     const string&   user_name,
     const string&   passwd,
     TConnectionMode mode,
     bool            reusable  = false,
     const string&   pool_name = kEmptyStr);

    /// Create new connection to specified server (within this context).
    /// It is your responsibility to delete the returned connection object.
    /// reusable - controls connection pooling mechanism. If it is set to true
    /// then a connection will be added to a pool  of connections instead of
    /// closing.
    ///
    /// pool_name - name of a pool to which this connection is going to belong.
    ///
    /// srv_name, user_name and passwd may be set to empty string.
    ///
    /// If pool_name is provided then connection will be taken from a pool
    /// having this name if a pool is not empty.
    /// It is your responsibility to put connections with the same
    /// server/user/password values in a pool.
    /// If a pool name is not provided but a server name (srv_name) is provided
    /// instead then connection with the same name will be taken from a pool of
    /// connections if a pool is not empty.
    /// If a pool is empty then new connection will be created unless you passed
    /// mode = fDoNotConnect. In this case NULL will be returned.
    /// If you did not provide either a pool name or a server name then NULL will
    /// be returned.
    CDB_Connection* ConnectValidated
    (const string&   srv_name,
     const string&   user_name,
     const string&   passwd,
     IConnValidator& validator,
     TConnectionMode mode      = 0,
     bool            reusable  = false,
     const string&   pool_name = kEmptyStr
     );

    /// Return number of currently open connections in this context.
    /// If "srv_name" is not NULL, then return # of conn. open to that server.
    virtual unsigned int NofConnections(const string& srv_name  = kEmptyStr,
                                        const string& pool_name = kEmptyStr)
        const;

    /// Add message handler "h" to process 'context-wide' (not bound
    /// to any particular connection) error messages.
    virtual void PushCntxMsgHandler(CDB_UserHandler* h,
                            EOwnership ownership = eNoOwnership);

    /// Remove message handler "h" and all handlers above it in the stack
    virtual void PopCntxMsgHandler(CDB_UserHandler* h);

    /// Add `per-connection' err.message handler "h" to the stack of default
    /// handlers which are inherited by all newly created connections.
    virtual void PushDefConnMsgHandler(CDB_UserHandler* h,
                               EOwnership ownership = eNoOwnership);

    /// Remove `per-connection' mess. handler "h" and all above it in the stack.
    virtual void PopDefConnMsgHandler(CDB_UserHandler* h);

    /// close reusable deleted connections for specified server and/or pool
    void CloseUnusedConnections(const string& srv_name  = kEmptyStr,
                                const string& pool_name = kEmptyStr);

    virtual bool ConnectedToMSSQLServer(void) const;

    const CDBHandlerStack& GetConnHandlerStack(void) const
    {
        return m_ConnHandlers;
    }

    virtual void SetClientCharset(const string& charset);
    const string& GetClientCharset(void) const
    {
        return m_ClientCharset;
    }
    EEncoding GetClientEncoding(void) const
    {
        return m_ClientEncoding;
    }
    bool IsMultibyteClientEncoding(void) const
    {
        return (GetClientEncoding() == eEncoding_UTF8);
    }

    size_t GetMaxTextImageSize(void) const
    {
        return m_MaxTextImageSize;
    }

protected:
    typedef list<CConnection*> TConnPool;

    mutable CFastMutex m_Mtx;

    // To allow children of CDriverContext to create CDB_Connection
    CDB_Connection* MakeCDBConnection(CConnection* connection);
    CDB_Connection* MakePooledConnection(const SConnAttr& conn_attr);
    virtual CConnection* MakeIConnection(const SConnAttr& conn_attr) = 0;
    void DestroyConnImpl(CConnection* impl);

    void CloseAllConn(void);
    void DeleteAllConn(void);

    const CDBHandlerStack& GetCtxHandlerStack(void) const
    {
        return m_CntxHandlers;
    }
    CDBHandlerStack& GetCtxHandlerStack(void)
    {
        return m_CntxHandlers;
    }

    void UpdateConnMaxTextImageSize(void) const;
    void UpdateConnTimeout(void) const;


private:
    /// Used connections
    TConnPool       m_NotInUse;
    /// Unused(reserve) connections
    TConnPool       m_InUse;

    /// Stack of `per-context' err.message handlers
    CDBHandlerStack m_CntxHandlers;
    /// Stacks of `per-connection' err.message handlers
    CDBHandlerStack m_ConnHandlers;

    string          m_ClientCharset;
    size_t          m_MaxTextImageSize;
    EEncoding       m_ClientEncoding;

    /// Return unused connection "conn" to the driver context for future
    /// reuse (if "conn_reusable" is TRUE) or utilization
    void x_Recycle(CConnection* conn, bool conn_reusable);
};


class NCBI_DBAPIDRIVER_EXPORT CWinSock
{
protected:
    CWinSock(void);
    ~CWinSock(void);
};

END_SCOPE(impl)

END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.3  2006/10/23 22:00:57  ssikorsk
 * + IsMultibyteClientEncoding()
 *
 * Revision 1.2  2006/09/13 19:25:21  ssikorsk
 * Added new class CWinSock;
 * Added members m_ClientCharset, m_MaxTextImageSize, and m_ClientEncoding to CDriverContext;
 *
 * Revision 1.1  2006/07/12 16:28:48  ssikorsk
 * Separated interface and implementation of CDB classes.
 *
 * ===========================================================================
 */


#endif  /* DBAPI_DRIVER_IMPL___DBAPI_IMPL_CONTEXT__HPP */
