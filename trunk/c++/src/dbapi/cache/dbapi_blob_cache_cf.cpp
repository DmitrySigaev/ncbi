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
 * Author: Anatoliy Kuznetsov
 *
 * File Description:  Class factory for DBAPI based ICache interface
 *
 */

#include <ncbi_pch.hpp>
#include <dbapi/cache/dbapi_blob_cache.hpp>
#include <corelib/plugin_manager_impl.hpp>
#include <corelib/ncbistr.hpp>

BEGIN_NCBI_SCOPE

const string kDBAPI_BlobCacheDriverName("dbapi_blob_cache");


/// Class factory for DBAPI BLOB cache
///
/// @internal
///

class CDBAPI_BlobCacheCF : 
    public CSimpleClassFactoryImpl<ICache, CDBAPI_Cache>
{
public:
    typedef CSimpleClassFactoryImpl<ICache, CDBAPI_Cache> TParent;

public:
    CDBAPI_BlobCacheCF() 
      : CSimpleClassFactoryImpl<ICache, CDBAPI_Cache>(
                                        kDBAPI_BlobCacheDriverName, -1)
    {}

    /// Create instance of TDriver
    virtual 
    ICache* CreateInstance(
                   const string&    driver  = kEmptyStr,
                   CVersionInfo     version = NCBI_INTERFACE_VERSION(ICache),
                   const TPluginManagerParamTree* params = 0) const;
};



// List of parameters accepted by the CF

static const string kCFParam_connection    = "connection";

static const string kCFParam_temp_dir      = "temp_dir";
static const string kCFParam_temp_prefix   = "temp_prefix";

static const string kCFParam_driver         = "driver";
static const string kCFParam_server         = "server";
static const string kCFParam_database       = "database";

static const string kCFParam_login         = "login";
static const string kCFParam_login_default = "cwrite";

static const string kCFParam_password         = "password";
static const string kCFParam_password_default = "allowed";

static const string kCFParam_mem_size      = "mem_size";


////////////////////////////////////////////////////////////////////////////
//
//  CDBAPI_BlobCacheCF


ICache* CDBAPI_BlobCacheCF::CreateInstance(
           const string&                  driver,
           CVersionInfo                   version,
           const TPluginManagerParamTree* params) const
{
    auto_ptr<CDBAPI_Cache> drv;
    if (driver.empty() || driver == m_DriverName) {
        if (version.Match(NCBI_INTERFACE_VERSION(ICache)) 
                            != CVersionInfo::eNonCompatible) {
            drv.reset(new CDBAPI_Cache());
        }
    } else {
        return 0;
    }

    // cache configuration

    const string& conn_str =
        GetParam(params, kCFParam_connection, false, kEmptyStr);

    const string& temp_dir =
        GetParam(params, kCFParam_temp_dir, false, kEmptyStr);
    const string& temp_prefix =
        GetParam(params, kCFParam_temp_prefix, false, kEmptyStr);

    if (!conn_str.empty()) {
        const void* ptr = NStr::StringToPtr(conn_str);
        IConnection* conn = (IConnection*)ptr;
        drv->Open(conn, temp_dir, temp_prefix);
    } else {
        const string& driver = 
            GetParam(params, kCFParam_driver, true, kEmptyStr);
        const string& server = 
            GetParam(params, kCFParam_server, true, kEmptyStr);
        const string& database = 
            GetParam(params, kCFParam_database, true, kEmptyStr);

        const string& login = 
            GetParam(params, kCFParam_login, false, kCFParam_login_default);
        const string& password = 
            GetParam(params, 
                     kCFParam_password, false, kCFParam_password_default);

        drv->Open(driver, server, database, 
                  login, password, 
                  temp_dir, temp_prefix);

    }

    const string& mem_size_str =
        GetParam(params, kCFParam_mem_size, false, kEmptyStr);
    unsigned mem_size = NStr::StringToUInt(mem_size_str);
    if (mem_size) {
        drv->SetMemBufferSize(mem_size);
    }

    return drv.release();
}



void NCBI_EntryPoint_DBAPI_BlobCache(
     CPluginManager<ICache>::TDriverInfoList&   info_list,
     CPluginManager<ICache>::EEntryPointRequest method)
{
    CHostEntryPointImpl<CDBAPI_BlobCacheCF>::NCBI_EntryPointImpl(
                                                            info_list,
                                                            method);
}

END_NCBI_SCOPE



/*
 * ===========================================================================
 * $Log$
 * Revision 1.1  2004/07/26 14:04:22  kuznets
 * Initial revision
 *
 *
 * ===========================================================================
 */
