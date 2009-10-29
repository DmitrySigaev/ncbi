#ifndef CONNECT_SERVICES__NETSERVICE_PARAMS_HPP
#define CONNECT_SERVICES__NETSERVICE_PARAMS_HPP

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
 * File Description:
 *   Declarations of the configuration parameters for connect/services.
 *
 * Authors:
 *   Dmitry Kazimirov
 *
 */

#include <connect/ncbi_types.h>

#include <corelib/ncbi_param.hpp>

#define RETRY_DELAY_DEFAULT 1.0
#define COMMUNICATION_TIMEOUT_DEFAULT 12.0
#define THROTTLE_RELAXATION_PERIOD_DEFAULT 0.0
#define THROTTLE_BY_SUBSEQUENT_CONNECTION_FAILURES_DEFAULT 0
#define THROTTLE_BY_CONNECTION_ERROR_RATE_DEFAULT "0/0"
#define THROTTLE_HOLD_UNTIL_ACTIVE_IN_LB_DEFAULT false
#define THROTTLE_FORCED_REBALANCE_DEFAULT 3.0
#define CONNECTION_ERROR_HISTORY_MAX 128
#define MAX_CONNECTION_TIME_DEFAULT 0.0

BEGIN_NCBI_SCOPE

NCBI_PARAM_DECL(string, netservice_api, communication_timeout);
typedef NCBI_PARAM_TYPE(netservice_api, communication_timeout)
    TServConn_CommTimeout;

NCBI_PARAM_DECL(unsigned int, netservice_api, connection_max_retries);
typedef NCBI_PARAM_TYPE(netservice_api, connection_max_retries)
    TServConn_ConnMaxRetries;

NCBI_PARAM_DECL(string, netservice_api, retry_delay);
typedef NCBI_PARAM_TYPE(netservice_api, retry_delay)
    TServConn_RetryDelay;

NCBI_PARAM_DECL(bool, netservice_api, use_linger2);
typedef NCBI_PARAM_TYPE(netservice_api, use_linger2)
    TServConn_UserLinger2;

NCBI_PARAM_DECL(int, netservice_api, max_find_lbname_retries);
typedef NCBI_PARAM_TYPE(netservice_api, max_find_lbname_retries)
    TServConn_MaxFineLBNameRetries;

NCBI_PARAM_DECL(string, netcache_api, fallback_server);
typedef NCBI_PARAM_TYPE(netcache_api, fallback_server)
    TCGI_NetCacheFallbackServer;

NCBI_PARAM_DECL(string, netcache_client, fallback_servers);
typedef NCBI_PARAM_TYPE(netcache_client, fallback_servers)
    TCGI_NetCacheFallbackServers;

NCBI_PARAM_DECL(bool, netcache_api, use_hasb_fallback);
typedef NCBI_PARAM_TYPE(netcache_api, use_hasb_fallback)
    TCGI_NetCacheUseHasbFallback;

NCBI_PARAM_DECL(int, netservice_api, max_connection_pool_size);
typedef NCBI_PARAM_TYPE(netservice_api, max_connection_pool_size)
    TServConn_MaxConnPoolSize;

// Worker node-specific parameters
NCBI_PARAM_DECL(bool, server, stop_on_job_errors);
typedef NCBI_PARAM_TYPE(server, stop_on_job_errors)
    TWorkerNode_StopOnJobErrors;

NCBI_PARAM_DECL(bool, server, allow_implicit_job_return);
typedef NCBI_PARAM_TYPE(server, allow_implicit_job_return)
    TWorkerNode_AllowImplicitJobReturn;

NCBI_PARAM_DECL(bool, server, do_not_rebalance);
typedef NCBI_PARAM_TYPE(server, do_not_rebalance)
    TWorkerNode_DoNotRebalance;

#define SECONDS_DOUBLE_TO_MS_UL(seconds) ((unsigned long) (seconds * 1000.0))

NCBI_XCONNECT_EXPORT unsigned long s_SecondsToMilliseconds(
    const string& seconds, unsigned long default_value);
NCBI_XCONNECT_EXPORT STimeout s_GetDefaultCommTimeout();
NCBI_XCONNECT_EXPORT void s_SetDefaultCommTimeout(const STimeout& tm);
NCBI_XCONNECT_EXPORT unsigned long s_GetRetryDelay();


END_NCBI_SCOPE

#endif  /* CONNECT_SERVICES__NETSERVICE_PARAMS_HPP */
