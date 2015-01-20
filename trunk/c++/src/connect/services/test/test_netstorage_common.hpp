#ifndef TEST_NETSTORAGE_COMMON__HPP
#define TEST_NETSTORAGE_COMMON__HPP

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
 * Authors:  Dmitry Kazimirov, Rafael Sadyrov
 *
 * File Description:  Simple test for the NetStorage API
 *      (implemented via RPC to NetStorage servers).
 *
 */

#include <boost/type_traits/integral_constant.hpp>

#include <corelib/ncbi_system.hpp>
#include <connect/services/netstorage.hpp>

BEGIN_NCBI_SCOPE


typedef boost::integral_constant<bool, true> TAttrTesting;


#define APP_NAME                    "test_netstorage_rpc"

NCBI_PARAM_DECL(string, netstorage, service_name);
typedef NCBI_PARAM_TYPE(netstorage, service_name) TNetStorage_ServiceName;

NCBI_PARAM_DECL(string, netcache, service_name);
typedef NCBI_PARAM_TYPE(netcache, service_name) TNetCache_ServiceName;

NCBI_PARAM_DECL(string, netcache, cache_name);
typedef NCBI_PARAM_TYPE(netcache, cache_name) TNetCache_CacheName;


template <class TNetStorage>
inline TNetStorage g_GetNetStorage()
{
    string nst_service(TNetStorage_ServiceName::GetDefault());
    string nc_service(TNetCache_ServiceName::GetDefault());
    string nc_cache(TNetCache_CacheName::GetDefault());
    string init_string(
            "nst="     + nst_service +
            "&nc="     + nc_service +
            "&cache="  + nc_cache +
            "&client="   APP_NAME);
    return CNetStorage(init_string);
}

template <>
inline CNetStorageByKey g_GetNetStorage<CNetStorageByKey>()
{
    string nst_service(TNetStorage_ServiceName::GetDefault());
    string nc_service(TNetCache_ServiceName::GetDefault());
    string nc_cache(TNetCache_CacheName::GetDefault());
    string init_string(
            "nst="     + nst_service +
            "&nc="     + nc_service +
            "&cache="  + nc_cache +
            "&client="   APP_NAME
            "&domain=" + nc_cache);
    return CNetStorageByKey(init_string);
}

inline void g_Sleep()
{
    SleepSec(1UL);
}

END_NCBI_SCOPE

#endif
