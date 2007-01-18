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
 * Author:  Anton Butanayev
 *
 * File Description:
 *    Driver for MySQL server
 *
 */

#include <ncbi_pch.hpp>

#include <corelib/plugin_manager_impl.hpp>
#include <corelib/plugin_manager_store.hpp>

// DO NOT DELETE this include !!!
#include <dbapi/driver/driver_mgr.hpp>

#include <dbapi/driver/mysql/interfaces.hpp>


BEGIN_NCBI_SCOPE


CMySQLContext::CMySQLContext()
{
}


CMySQLContext::~CMySQLContext()
{
}


bool CMySQLContext::IsAbleTo(ECapability /*cpb*/) const
{
    return false;
}


impl::CConnection*
CMySQLContext::MakeIConnection(const SConnAttr& conn_attr)
{
    return new CMySQL_Connection(*this, conn_attr.srv_name, conn_attr.user_name,
                                 conn_attr.passwd);
}


///////////////////////////////////////////////////////////////////////
// DriverManager related functions
//

static I_DriverContext* MYSQL_CreateContext(const map<string,string>* /*attr*/)
{
    return new CMySQLContext();
}

///////////////////////////////////////////////////////////////////////////////
const string kDBAPI_MYSQL_DriverName("mysql");

class CDbapiMySqlCF2 : public CSimpleClassFactoryImpl<I_DriverContext, CMySQLContext>
{
public:
    typedef CSimpleClassFactoryImpl<I_DriverContext, CMySQLContext> TParent;

public:
    CDbapiMySqlCF2(void);
    ~CDbapiMySqlCF2(void);

};

CDbapiMySqlCF2::CDbapiMySqlCF2(void)
    : TParent( kDBAPI_MYSQL_DriverName, 0 )
{
    return ;
}

CDbapiMySqlCF2::~CDbapiMySqlCF2(void)
{
    return ;
}

///////////////////////////////////////////////////////////////////////////////
void
NCBI_EntryPoint_xdbapi_mysql(
    CPluginManager<I_DriverContext>::TDriverInfoList&   info_list,
    CPluginManager<I_DriverContext>::EEntryPointRequest method)
{
    CHostEntryPointImpl<CDbapiMySqlCF2>::NCBI_EntryPointImpl( info_list, method );
}

NCBI_DBAPIDRIVER_MYSQL_EXPORT
void
DBAPI_RegisterDriver_MYSQL(void)
{
    RegisterEntryPoint<I_DriverContext>( NCBI_EntryPoint_xdbapi_mysql );
}

///////////////////////////////////////////////////////////////////////////////
NCBI_DBAPIDRIVER_MYSQL_EXPORT
void DBAPI_RegisterDriver_MYSQL(I_DriverMgr& mgr)
{
    mgr.RegisterDriver("mysql", MYSQL_CreateContext);
    DBAPI_RegisterDriver_MYSQL();
}

void DBAPI_RegisterDriver_MYSQL_old(I_DriverMgr& mgr)
{
    DBAPI_RegisterDriver_MYSQL(mgr);
}

extern "C" {
    NCBI_DBAPIDRIVER_MYSQL_EXPORT
    void* DBAPI_E_mysql()
    {
        return (void*) DBAPI_RegisterDriver_MYSQL_old;
    }
}


END_NCBI_SCOPE



