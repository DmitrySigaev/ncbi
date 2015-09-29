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
 *   Government have not placed any restriction on its use or reproduction.
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
 * File Description: Miscellaneous commands of the grid_cli application
 *                   (the ones that are not specific to NetCache, NetSchedule,
 *                   or worker nodes).
 *
 */

#include <ncbi_pch.hpp>

#include "ns_cmd_impl.hpp"
#include "util.hpp"

#include <connect/services/netstorage_impl.hpp>

USING_NCBI_SCOPE;

#define TOKEN_TYPE__NETCACHE_BLOB_KEY "NetCacheBlobKey"
#define TOKEN_TYPE__NETSCHEDULE_JOB_KEY "NetScheduleJobKey"
#define TOKEN_TYPE__NETSTORAGEOBJECT_LOC "NetStorageObjectLoc"

void s_AddBlobMeta(CJsonNode& node, const CNetCacheKey& key)
{
    if (key.GetVersion() != 3) {
        const string server_host(g_NetService_TryResolveHost(key.GetHost()));
        node.SetString("server_host", server_host);
        node.SetInteger("server_port", key.GetPort());
    } else {
        node.SetInteger("server_address_crc32", key.GetHostPortCRC32());
    }

    node.SetInteger("id", key.GetId());

    CTime generation_time;
    generation_time.SetTimeT(key.GetCreationTime());
    node.SetString("key_generation_time", generation_time.AsString());
    node.SetInteger("random", key.GetRandomPart());

    const string service(key.GetServiceName());

    if (!service.empty()) {
        node.SetString("service_name", service);
    } else {
        node.SetNull("service_name");
    }
}

CJsonNode s_WhatIs(const string& id, CCompoundIDPool::TInstance id_pool)
{
    try {
        CNetStorageObjectLoc object_loc(id_pool, id);

        CJsonNode object_loc_info(CJsonNode::NewObjectNode());

        object_loc_info.SetString("type", TOKEN_TYPE__NETSTORAGEOBJECT_LOC);

        object_loc.ToJSON(object_loc_info);
        return object_loc_info;
    }
    catch (CCompoundIDException&) {
    }
    catch (CNetStorageException&) {
    }

    CNetCacheKey nc_key;

    if (CNetCacheKey::ParseBlobKey(id.c_str(), id.length(), &nc_key, id_pool)) {
        CJsonNode result(CJsonNode::NewObjectNode());
        result.SetString("type", TOKEN_TYPE__NETCACHE_BLOB_KEY);
        result.SetInteger("key_version", nc_key.GetVersion());
        s_AddBlobMeta(result, nc_key);
        return result;
    }
    
    CNetScheduleKey ns_key;

    if (ns_key.ParseJobKey(id, id_pool)) {
        CJobInfoToJSON job_info_to_json;

        CJsonNode job_info_node(job_info_to_json.GetRootNode());

        job_info_node.SetString("type", TOKEN_TYPE__NETSCHEDULE_JOB_KEY);
        job_info_node.SetInteger("key_version", ns_key.version);

        job_info_to_json.ProcessJobMeta(ns_key);
        return job_info_node;
    }

    return NULL;
}

int CGridCommandLineInterfaceApp::Cmd_WhatIs()
{
    if (m_Opts.output_format == eJSON) {
        CJsonNode result(s_WhatIs(m_Opts.id, m_CompoundIDPool));

        if (result) {
            g_PrintJSON(stdout, result);
            return 0;
        }
    } else {
        try {
            CNetStorageObjectLoc object_loc(m_CompoundIDPool, m_Opts.id);

            CJsonNode object_loc_info(CJsonNode::NewObjectNode());

            object_loc_info.SetString("type", TOKEN_TYPE__NETSTORAGEOBJECT_LOC);

            object_loc.ToJSON(object_loc_info);

            g_PrintJSON(stdout, object_loc_info);

            return 0;
        }
        catch (CCompoundIDException&) {
        }
        catch (CNetStorageException&) {
        }

        CNetCacheKey nc_key;
        CNetScheduleKey ns_key;

        if (CNetCacheKey::ParseBlobKey(m_Opts.id.c_str(),
            m_Opts.id.length(), &nc_key, m_CompoundIDPool)) {
            printf("type: " TOKEN_TYPE__NETCACHE_BLOB_KEY "\nkey_version: %u\n",
                    nc_key.GetVersion());
            PrintBlobMeta(nc_key);

            if (m_Opts.output_format == eHumanReadable)
                printf("\nTo retrieve blob attributes from the server, use\n"
                        GRID_APP_NAME " blobinfo %s\n", m_Opts.id.c_str());
            return 0;
        } else if (ns_key.ParseJobKey(m_Opts.id, m_CompoundIDPool)) {
            printf("type: " TOKEN_TYPE__NETSCHEDULE_JOB_KEY "\n"
                    "key_version: %u\n",
                    ns_key.version);

            CPrintJobInfo print_job_info;

            print_job_info.ProcessJobMeta(ns_key);

            printf("\nTo retrieve job attributes from the server, use\n"
                    GRID_APP_NAME " jobinfo %s\n", m_Opts.id.c_str());
            return 0;
        }
    }

    fprintf(stderr, "Unable to recognize the specified token.\n");
    return 3;
}

int CGridCommandLineInterfaceApp::Cmd_Login()
{
    if (m_Opts.app_uid.empty()) {
        fputs("Application UID cannot be empty.\n", stderr);
        return 2;
    }
    if (m_Opts.app_uid == DEFAULT_APP_UID) {
        fputs("Application UID '" DEFAULT_APP_UID
                "' is reserved and cannot be used.\n", stderr);
        return 2;
    }

    CCompoundID cid(m_CompoundIDPool.NewID(eCIC_GenericID));

    cid.AppendLabel(LOGIN_TOKEN_APP_UID_FIELD);
    cid.AppendString(m_Opts.app_uid);

    if (IsOptionSet(eAuth)) {
        cid.AppendLabel(LOGIN_TOKEN_AUTH_FIELD);
        cid.AppendString(m_Opts.auth);
    }

    string user, host;
    g_GetUserAndHost(&user, &host);
    cid.AppendLabel(LOGIN_TOKEN_USER_FIELD);
    cid.AppendString(user);
    cid.AppendLabel(LOGIN_TOKEN_HOST_FIELD);
    cid.AppendHost(host);

    if (IsOptionSet(eNetCache)) {
        cid.AppendLabel(LOGIN_TOKEN_NETCACHE_FIELD);
        cid.AppendServiceName(m_Opts.nc_service);
    }
    if (IsOptionSet(eCache)) {
        cid.AppendLabel(LOGIN_TOKEN_ICACHE_NAME_FIELD);
        cid.AppendDatabaseName(m_Opts.cache_name);
    }
    if (IsOptionSet(eEnableMirroring)) {
        cid.AppendLabel(LOGIN_TOKEN_ENABLE_MIRRORING);
        cid.AppendBoolean(true);
    }

    if (IsOptionSet(eNetSchedule)) {
        cid.AppendLabel(LOGIN_TOKEN_NETSCHEDULE_FIELD);
        cid.AppendServiceName(m_Opts.ns_service);
    }
    if (IsOptionSet(eQueue)) {
        cid.AppendLabel(LOGIN_TOKEN_QUEUE_FIELD);
        cid.AppendDatabaseName(m_Opts.queue);
    }

    cid.AppendLabel(LOGIN_TOKEN_SESSION_PID_FIELD);
    cid.AppendID((Uint8) CProcess::GetCurrentPid());

    cid.AppendLabel(LOGIN_TOKEN_SESSION_TIMESTAMP_FIELD);
    cid.AppendCurrentTime();

    cid.AppendLabel(LOGIN_TOKEN_SESSION_UID_FIELD);
    cid.AppendString(GetDiagContext().GetStringUID());

#ifdef NCBI_GRID_XSITE_CONN_SUPPORT
    if (IsOptionSet(eAllowXSiteConn)) {
        cid.AppendLabel(LOGIN_TOKEN_ALLOW_XSITE_CONN);
        cid.AppendBoolean(true);
    }
#endif

    if (IsOptionSet(eNoConnRetries)) {
        cid.AppendLabel(LOGIN_TOKEN_NO_CONN_RETRIES);
        cid.AppendBoolean(true);
    }

    if (IsOptionSet(eFileTrackSite)) {
        cid.AppendLabel(LOGIN_TOKEN_FILETRACK_SITE);
        cid.AppendString(
                CDirectNetStorageRegistry::CFileTrack::GetSite(GetConfig()));
    }
    if (IsOptionSet(eFileTrackAPIKey)) {
        cid.AppendLabel(LOGIN_TOKEN_FILETRACK_API_KEY);
        cid.AppendString(
                CDirectNetStorageRegistry::CFileTrack::GetKey(GetConfig()));
    }

    PrintLine(cid.ToString());

    return 0;
}
