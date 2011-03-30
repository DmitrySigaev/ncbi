#ifndef CONN___NETCACHE_ADMIN__HPP
#define CONN___NETCACHE_ADMIN__HPP

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
 * Authors:  Dmitry Kazimirov
 *
 * File Description:
 *   Administrative API for NetCache.
 *
 */

/// @file netcache_admin.hpp
/// Administrative API for NetCache.
///

#include "netcomponent.hpp"


BEGIN_NCBI_SCOPE


/** @addtogroup NetCacheClient
 *
 * @{
 */


struct SNetCacheAdminImpl;

class NCBI_XCONNECT_EXPORT CNetCacheAdmin
{
    NCBI_NET_COMPONENT(NetCacheAdmin);

    /// Shutdown the server daemon.
    ///
    /// @note
    ///  Protected to avoid a temptation to call it from time to time. :)
    void ShutdownServer();

    /// Reload configuration parameters from the same source.
    void ReloadServerConfig();

    /// Drop the specified database and then create it anew.
    void Reinitialize(const string& cache_name = kEmptyStr);

    /// Print contents of the configuration file
    void PrintConfig(CNcbiOstream& output_stream);

    /// Print server statistics
    void PrintStat(CNcbiOstream& output_stream);

    // Print server health information
    void PrintHealth(CNcbiOstream& output_stream);

    void GetServerVersion(CNcbiOstream& output_stream);

    string GetServerVersion();
};

/* @} */


END_NCBI_SCOPE

#endif  /* CONN___NETCACHE_ADMIN__HPP */
