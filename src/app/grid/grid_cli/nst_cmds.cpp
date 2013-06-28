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
 * File Description: FileTrack interface.
 *
 */

#include <ncbi_pch.hpp>

#include <connect/services/netstorage_impl.hpp>

#include <misc/netstorage/netstorage.hpp>

#include "grid_cli.hpp"
#include "util.hpp"

USING_NCBI_SCOPE;

void CGridCommandLineInterfaceApp::SetUp_NetStorageCmd()
{
    switch (IsOptionExplicitlySet(eNetCache, OPTION_N(0)) |
            IsOptionExplicitlySet(eCache, OPTION_N(1)))
    {
    case OPTION_N(0) + OPTION_N(1):
        if (!IsOptionSet(eNetStorage)) {
            m_NetICacheClient = CNetICacheClient(m_Opts.nc_service,
                    m_Opts.cache_name, m_Opts.auth);
#ifdef NCBI_GRID_XSITE_CONN_SUPPORT
            if (IsOptionExplicitlySet(eAllowXSiteConn))
                m_NetICacheClient.GetService().AllowXSiteConnections();
#endif
        }
        break;

    case OPTION_N(0):
        NCBI_THROW(CArgException, eNoArg, "'--" NETCACHE_OPTION
                "' requires '--" CACHE_OPTION "'.");

    case OPTION_N(1):
        NCBI_THROW(CArgException, eNoArg, "'--" CACHE_OPTION
                "' requires '--" NETCACHE_OPTION "'.");
    }

    if (!IsOptionSet(eNetStorage))
        m_NetStorage = g_CreateNetStorage(m_NetICacheClient,
                m_Opts.netstorage_flags);
    else {
        string init_string = "nst=" + NStr::URLEncode(m_Opts.nst_service);

        if (IsOptionExplicitlySet(eNetCache)) {
            init_string += "&nc=";
            init_string += NStr::URLEncode(m_Opts.nc_service);
            init_string += "&cache=";
            init_string += NStr::URLEncode(m_Opts.cache_name);
        }

        string auth;

        if (IsOptionSet(eAuth)) {
            auth = m_Opts.auth;
        } else {
            string user, host;
            g_GetUserAndHost(&user, &host);
            auth = user + '@';
            auth += host;
        }

        init_string += "&client=";
        init_string += NStr::URLEncode(auth);

        m_NetStorage = CNetStorage(init_string, m_Opts.netstorage_flags);
    }
}

int CGridCommandLineInterfaceApp::Cmd_Upload()
{
    SetUp_NetStorageCmd();

    CNetFile netfile(IsOptionSet(eOptionalID) ?
            m_NetStorage.Open(m_Opts.id, m_Opts.netstorage_flags) :
            m_NetStorage.Create(m_Opts.netstorage_flags));

    if (IsOptionSet(eInput))
        netfile.Write(m_Opts.input);
    else {
        char buffer[IO_BUFFER_SIZE];
        size_t bytes_read;

        while ((bytes_read = fread(buffer, 1,
                sizeof(buffer), m_Opts.input_stream)) > 0) {
            netfile.Write(buffer, bytes_read);
            if (feof(m_Opts.input_stream))
                break;
        }
    }

    netfile.Close();

    if (!IsOptionSet(eOptionalID))
        PrintLine(netfile.GetID());

    return 0;
}

int CGridCommandLineInterfaceApp::Cmd_Download()
{
    SetUp_NetStorageCmd();

    CNetFile netfile(m_NetStorage.Open(m_Opts.id, m_Opts.netstorage_flags));

    char buffer[IO_BUFFER_SIZE];
    size_t bytes_read;

    while (!netfile.Eof()) {
        bytes_read = netfile.Read(buffer, sizeof(buffer));
        fwrite(buffer, 1, bytes_read, m_Opts.output_stream);
    }

    netfile.Close();

    return 0;
}

int CGridCommandLineInterfaceApp::Cmd_Relocate()
{
    SetUp_NetStorageCmd();

    PrintLine(m_NetStorage.Relocate(m_Opts.id, m_Opts.netstorage_flags));

    return 0;
}

int CGridCommandLineInterfaceApp::Cmd_MkFileID()
{
    SetUp_NetStorageCmd();

    auto_ptr<CNetFileID> file_id;

    switch (IsOptionSet(eOptionalID, OPTION_N(0)) |
            IsOptionSet(eFileKey, OPTION_N(1)) |
            IsOptionSet(eNamespace, OPTION_N(2))) {
    case OPTION_N(0):
        file_id.reset(new CNetFileID(m_Opts.id));
        break;

    case OPTION_N(1) + OPTION_N(2):
        file_id.reset(new CNetFileID(m_Opts.netstorage_flags,
                m_Opts.app_domain, m_Opts.id));
        break;

    case 0:
        fprintf(stderr, GRID_APP_NAME " mkfileid: either a "
                "file ID or a combination of '--"
                FILE_KEY_OPTION "' and '--" NAMESPACE_OPTION
                "' must be specified.\n");
        return 2;

    case OPTION_N(1):
        fprintf(stderr, GRID_APP_NAME " mkfileid: '--" FILE_KEY_OPTION
                "' requires '--" NAMESPACE_OPTION "'.\n");
        return 2;

    case OPTION_N(2):
        fprintf(stderr, GRID_APP_NAME " mkfileid: '--" NAMESPACE_OPTION
                "' requires '--" FILE_KEY_OPTION "'.\n");
        return 2;

    default:
        fprintf(stderr, GRID_APP_NAME " mkfileid: file ID cannot "
                "be combined with either '--" FILE_KEY_OPTION
                "' or '--" NAMESPACE_OPTION "'.\n");
        return 2;
    }

    if (IsOptionSet(eNetCache))
        g_SetNetICacheParams(*file_id, m_NetICacheClient);

    if (m_Opts.netstorage_flags != 0)
        file_id->SetStorageFlags(m_Opts.netstorage_flags);

    if (IsOptionSet(eTTL))
        file_id->SetTTL(m_Opts.ttl);

    PrintLine(file_id->GetID());

    return 0;
}

int CGridCommandLineInterfaceApp::Cmd_NetFileInfo()
{
    SetUp_NetStorageCmd();

    CNetFile netfile(m_NetStorage.Open(m_Opts.id, m_Opts.netstorage_flags));

    g_PrintJSON(stdout, netfile.GetInfo().ToJSON());

    return 0;
}

int CGridCommandLineInterfaceApp::Cmd_RemoveNetFile()
{
    SetUp_NetStorageCmd();

    m_NetStorage.Remove(m_Opts.id);

    return 0;
}
