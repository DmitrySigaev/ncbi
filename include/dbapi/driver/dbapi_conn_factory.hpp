#ifndef DBAPI_CONN_FACTORY_HPP
#define DBAPI_CONN_FACTORY_HPP

/*  $Id$
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
* Author: Sergey Sikorskiy
*
* File Description:
*
*============================================================================
*/

#include <corelib/impl/ncbi_dbsvcmapper.hpp>
#include <dbapi/driver/dbapi_driver_conn_mgr.hpp>

BEGIN_NCBI_SCOPE

///////////////////////////////////////////////////////////////////////////////
/// CDBConnectionFactory
///

enum EDefaultMapping 
{
    eUseDefaultMapper,
    eNoMapping
};

class NCBI_DBAPIDRIVER_EXPORT CDBConnectionFactory : public IDBConnectionFactory
{
public:
    /// CDBConnectionFactory will take ownership of svr_name_policy.
    /// CDBConnectionFactory won't take ownership of registry.
    CDBConnectionFactory(IDBServiceMapper* svc_mapper, 
                         const IRegistry* registry = NULL,
                         EDefaultMapping def_mapping = eUseDefaultMapper);
    virtual ~CDBConnectionFactory(void);

    //
    virtual void Configure(const IRegistry* registry = NULL);
    
    IDBServiceMapper& GetServiceMapper(void);
    const IDBServiceMapper& GetServiceMapper(void) const;
    
    //
    unsigned int GetMaxNumOfConnAttempts(void) const;
    void SetMaxNumOfConnAttempts(unsigned int max_num);
    
    unsigned int GetMaxNumOfServerAlternatives(void) const;
    void SetMaxNumOfServerAlternatives(unsigned int max_num);
    
    unsigned int GetMaxNumOfDispatches(void) const;
    void SetMaxNumOfDispatches(unsigned int max_num);
    
    unsigned int GetConnectionTimeout(void) const;
    void SetConnectionTimeout(unsigned int timeout);
    
    unsigned int GetLoginTimeout(void) const;
    void SetLoginTimeout(unsigned int timeout);
    
protected:
    void ConfigureFromRegistry(const IRegistry* registry = NULL);
    virtual CDB_Connection* MakeDBConnection(
        I_DriverContext& ctx,
        const I_DriverContext::SConnAttr& conn_attr,
        IConnValidator* validator = NULL);
    
    //
    TSvrRef GetDispatchedServer(const string& service_name);
    void SetDispatchedServer(const string&  service_name, 
                             const TSvrRef& server);
    
    //
    unsigned int GetNumOfDispatches(const string& service_name);
    
private:
    // Methods
    CDB_Connection* DispatchServerName(
        I_DriverContext& ctx,
        const I_DriverContext::SConnAttr& conn_attr,
        IConnValidator* validator);
    
    CDB_Connection* MakeValidConnection(
        I_DriverContext& ctx,
        const I_DriverContext::SConnAttr& conn_attr,
        IConnValidator* validator) const;
    
    // Data types
    typedef map<string, TSvrRef>      TDispatchedSet;
    typedef map<string, unsigned int> TDispatchNumMap;
    
    // Data
    TDispatchedSet  m_DispatchedSet;
    TDispatchNumMap m_DispatchNumMap;
    
    mutable CFastMutex m_Mtx;

    auto_ptr<IDBServiceMapper> m_DBServiceMapper;
    // 0 means *none* (even do not try to connect)
    unsigned int m_MaxNumOfConnAttempts; 
    // 0 means *none* (even do not try to connect)
    // 1 means *try only one server* (give up strategy)
    unsigned int m_MaxNumOfServerAlternatives;
    // 0 means *unlimited*
    unsigned int m_MaxNumOfDispatches; 
    unsigned int m_ConnectionTimeout;
    unsigned int m_LoginTimeout;
};
    
///////////////////////////////////////////////////////////////////////////////
/// CDBGiveUpFactory
///

/// Helper class
/// This policy will give up without trying to connect to alternative db servers.
class NCBI_DBAPIDRIVER_EXPORT CDBGiveUpFactory : public CDBConnectionFactory
{
public:
    CDBGiveUpFactory(IDBServiceMapper* svc_mapper, 
                     const IRegistry* registry = NULL,
                     EDefaultMapping def_mapping = eUseDefaultMapper);
    virtual ~CDBGiveUpFactory(void);
};

///////////////////////////////////////////////////////////////////////////////
/// CDBRedispatchFactory
///

/// Helper class
/// This policy will redispatch every time after a successful dispatch.
class NCBI_DBAPIDRIVER_EXPORT CDBRedispatchFactory : public CDBConnectionFactory
{
public:
    CDBRedispatchFactory(IDBServiceMapper* svc_mapper, 
                         const IRegistry* registry = NULL,
                         EDefaultMapping def_mapping = eUseDefaultMapper);
    virtual ~CDBRedispatchFactory(void);
};


///////////////////////////////////////////////////////////////////////////////
/// CConnValidatorCoR
///
/// IConnValidator adaptor which implements the chain of responsibility
/// pattern
///

class NCBI_DBAPIDRIVER_EXPORT CConnValidatorCoR : public IConnValidator
{
public:
    CConnValidatorCoR(void);
    virtual ~CConnValidatorCoR(void);
    
    virtual EConnStatus Validate(CDB_Connection& conn);
    
    void Push(const CRef<IConnValidator>& validator);
    void Pop(void);
    CRef<IConnValidator> Top(void) const;
    bool Empty(void) const;
    
protected:
    typedef vector<CRef<IConnValidator> > TValidators;
    
    mutable CFastMutex m_Mtx;
    TValidators        m_Validators;
};

///////////////////////////////////////////////////////////////////////////////
class NCBI_DBAPIDRIVER_EXPORT CTrivialConnValidator : public IConnValidator
{
public:
    enum EValidateAttr {
        eKeepModifiedConnection = 0,
        eRestoreDefaultDB = 1
    };
    
    CTrivialConnValidator(const string& db_name, 
                          EValidateAttr attr = eRestoreDefaultDB);
    virtual ~CTrivialConnValidator(void);
    
    virtual EConnStatus Validate(CDB_Connection& conn);
    
    const string& GetDBName(void) const
    {
        return m_DBName;
    }
    
private:
    const string        m_DBName;
    const EValidateAttr m_Attr;
};



/////////////////////////////////////////////////////////////////////////////
inline
IDBServiceMapper& 
CDBConnectionFactory::GetServiceMapper(void)
{
    return *m_DBServiceMapper;
}

inline
const IDBServiceMapper& 
CDBConnectionFactory::GetServiceMapper(void) const
{
    return *m_DBServiceMapper;
}

inline
unsigned int 
CDBConnectionFactory::GetMaxNumOfConnAttempts(void) const
{
    return m_MaxNumOfConnAttempts;
}

inline
unsigned int 
CDBConnectionFactory::GetMaxNumOfServerAlternatives(void) const
{
    return m_MaxNumOfServerAlternatives;
}

inline
unsigned int 
CDBConnectionFactory::GetMaxNumOfDispatches(void) const
{
    return m_MaxNumOfDispatches;
}

inline
unsigned int 
CDBConnectionFactory::GetConnectionTimeout(void) const
{
    return m_ConnectionTimeout;
}

inline
unsigned int 
CDBConnectionFactory::GetLoginTimeout(void) const
{
    return m_LoginTimeout;
}

END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.2  2006/01/23 13:26:28  ssikorsk
 * Added method GetServiceMapper to CDBConnectionFactory;
 * Added classes CConnValidatorCoR and CTrivialConnValidator
 *     as an implementation of the IConnValidator interface.
 *
 * Revision 1.1  2006/01/03 19:32:22  ssikorsk
 * Added CDBConnectionFactory, CDBRedispatchFactory and CDBGiveUpFactory
 * as implementations of the IDBConnectionFactory interface.
 *
 * ===========================================================================
 */

#endif // DBAPI_CONN_FACTORY_HPP
