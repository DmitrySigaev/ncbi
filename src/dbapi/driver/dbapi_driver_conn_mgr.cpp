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
 *  ===========================================================================
 *
 * Author:  Sergey Sikorskiy
 *
 * File Description:
 *
 */

#include <ncbi_pch.hpp>

#include <dbapi/driver/dbapi_driver_conn_mgr.hpp>
#include <corelib/ncbi_safe_static.hpp>
#include <set>
#include <map>
#include <dbapi/driver/public.hpp>

BEGIN_NCBI_SCOPE

///////////////////////////////////////////////////////////////////////////////
class CDefaultConnectPolicy : public IDBConnectionFactory
{
public:
    virtual ~CDefaultConnectPolicy(void);

public:
    virtual void Configure(const IRegistry* registry = NULL);
    virtual CDB_Connection* MakeDBConnection(
        I_DriverContext& ctx,
        const I_DriverContext::SConnAttr& conn_attr,
        IConnValidator* validator = NULL);
};

CDefaultConnectPolicy::~CDefaultConnectPolicy(void)
{
}

void
CDefaultConnectPolicy::Configure(const IRegistry*)
{
    // Do-nothing ...
}

CDB_Connection*
CDefaultConnectPolicy::MakeDBConnection(
    I_DriverContext& ctx,
    const I_DriverContext::SConnAttr& conn_attr,
    IConnValidator* validator)
{
    auto_ptr<CDB_Connection> conn(CtxMakeConnection(ctx, conn_attr));

    if (conn.get() &&
        validator &&
        validator->Validate(*conn) == IConnValidator::eInvalidConn) {
        return NULL;
    }
    return conn.release();
}

///////////////////////////////////////////////////////////////////////////////
IConnValidator::~IConnValidator(void)
{
}

///////////////////////////////////////////////////////////////////////////////
IDBConnectionFactory::IDBConnectionFactory(void)
{
}

IDBConnectionFactory::~IDBConnectionFactory(void)
{
}

///////////////////////////////////////////////////////////////////////////////
CDbapiConnMgr::CDbapiConnMgr(void)
{
    m_ConnectFactory.Reset( new CDefaultConnectPolicy() );
}

CDbapiConnMgr::~CDbapiConnMgr(void)
{
}

CDbapiConnMgr&
CDbapiConnMgr::Instance(void)
{
    static CSafeStaticPtr<CDbapiConnMgr> instance;

    return instance.Get();
}

END_NCBI_SCOPE

