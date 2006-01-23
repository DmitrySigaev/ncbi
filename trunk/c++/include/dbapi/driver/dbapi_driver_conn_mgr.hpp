#ifndef DBAPI_DRIVER_CONN_MGR_HPP
#define DBAPI_DRIVER_CONN_MGR_HPP


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
 * File Description:  Connection manager
 *
 */

#include <corelib/ncbiobj.hpp>
#include <dbapi/driver/interfaces.hpp>

BEGIN_NCBI_SCOPE


///////////////////////////////////////////////////////////////////////////////
// Forward declaration

class IRegistry;
template <typename T> class CSafeStaticPtr;


///////////////////////////////////////////////////////////////////////////////
/// IConnValidator
///

class NCBI_DBAPIDRIVER_EXPORT IConnValidator : public CObject
{
public:
    enum EConnStatus {eValidConn, eInvalidConn};
    
    virtual ~IConnValidator(void);
    
    virtual EConnStatus Validate(CDB_Connection& conn) = 0;
};

///////////////////////////////////////////////////////////////////////////////
/// IDBConnectionFactory
///

class NCBI_DBAPIDRIVER_EXPORT IDBConnectionFactory : public CObject
{
public:
    /// IDBConnectionFactory will take ownership of validator if there is any.
    IDBConnectionFactory(void);
    virtual ~IDBConnectionFactory(void);

    /// Configure connection policy using registry.
    virtual void Configure(const IRegistry* registry = NULL) = 0;
    
protected:
    /// Create new connection object for the given context 
    /// and connection attributes.
    virtual CDB_Connection* MakeDBConnection
    (I_DriverContext&                  ctx,
     const I_DriverContext::SConnAttr& conn_attr,
     IConnValidator* validator = NULL) = 0;
    
    /// Helper method to provide access to a protected method in I_DriverContext
    /// for child classses.
    static CDB_Connection* CtxMakeConnection
    (I_DriverContext&                  ctx,
     const I_DriverContext::SConnAttr& conn_attr);
    
private:    
    // Friends
    friend class I_DriverContext;
};


///////////////////////////////////////////////////////////////////////////////
/// CDbapiConnMgr
///

class NCBI_DBAPIDRIVER_EXPORT CDbapiConnMgr
{
public:
    /// Get access to the class instance.
    static CDbapiConnMgr& Instance(void);
    
    /// Set up a connection factory.
    void SetConnectionFactory(IDBConnectionFactory* factory)
    {
        m_ConnectFactory.Reset(factory);
    }
    
    /// Retrieve a connection factory.
    CRef<IDBConnectionFactory> GetConnectionFactory(void) const
    {
        return m_ConnectFactory;
    }
    
private:
    CDbapiConnMgr(void);
    ~CDbapiConnMgr(void);
    
    CRef<IDBConnectionFactory> m_ConnectFactory;
    
    // Friends
    friend class CSafeStaticPtr<CDbapiConnMgr>;
};


///////////////////////////////////////////////////////////////////////////////
inline
CDB_Connection* 
IDBConnectionFactory::CtxMakeConnection
(I_DriverContext&                  ctx,
 const I_DriverContext::SConnAttr& conn_attr)
{
    return ctx.MakePooledConnection(conn_attr);
}

END_NCBI_SCOPE



/*
 * ===========================================================================
 * $Log$
 * Revision 1.2  2006/01/23 13:21:45  ssikorsk
 * Added interface IConnValidator;
 * Renamed IDBCannectionFactory::MakeConnection to MakeDBConnection;
 * Added IConnValidator as a default argument to IDBCannectionFactory::MakeDBConnection;
 * Changed return types of IDBCannectionFactory::MakeDBConnection and
 *     IDBCannectionFactory::CtxMakeConnection to CDB_Connection*;
 *
 * Revision 1.1  2006/01/03 19:25:12  ssikorsk
 * Declaration of the IDBConnectionFactory interface.
 * CDbapiConnMgr singleton to manage life time of
 * IDBConnectionFactory implementations.
 *
 * ===========================================================================
 */

#endif  /* DBAPI_DRIVER_CONN_MGR_HPP */

