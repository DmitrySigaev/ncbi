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
 * File Description: NetCache-specific commands of the grid_cli application.
 *
 */

#include <ncbi_pch.hpp>

#include "grid_cli.hpp"

USING_NCBI_SCOPE;

void CGridCommandLineInterfaceApp::SetUp_NetCacheCmd(
    CGridCommandLineInterfaceApp::ENetCacheAPIClass api_class)
{
    if (m_Opts.auth.empty())
        m_Opts.auth = "netcache_control";

    static const string config_section("netcache_api");

    if (m_Opts.enable_mirroring)
        CNcbiApplication::Instance()->GetConfig().Set(
            config_section, "enable_mirroring", "true");

    switch (api_class) {
    case eNetCacheAPI:
        m_NetCacheAPI = CNetCacheAPI(m_Opts.nc_service, m_Opts.auth);
        if (!m_Opts.id.empty() && !m_Opts.nc_service.empty()) {
            string host, port;

            if (NStr::SplitInTwo(m_Opts.nc_service, ":", host, port))
                m_NetCacheAPI.GetService().StickToServer(
                    host, NStr::StringToInt(port));
            else {
                NCBI_THROW(CArgException, eInvalidArg,
                    "This operation requires the \"service_name\" "
                    "argument to be a host:port server address.");
            }
        }
        break;

    case eNetICacheClient:
        m_NetICacheClient = CNetICacheClient(m_Opts.nc_service,
            m_Opts.cache_name, m_Opts.auth);

        if (m_Opts.nc_service.empty()) {
            NCBI_THROW(CArgException, eNoValue, "\"--nc\" "
                "option is required in icache mode.");
        }
        break;

    default: // always eNetCacheAdmin
        m_NetCacheAdmin = CNetCacheAPI(m_Opts.nc_service,
            m_Opts.auth).GetAdmin();
    }
}

void CGridCommandLineInterfaceApp::ParseICacheKey(
    bool permit_empty_version, bool* version_is_defined)
{
    vector<string> key_parts;

    NStr::Tokenize(m_Opts.id, ",", key_parts);
    if (key_parts.size() != 3) {
        NCBI_THROW_FMT(CArgException, eInvalidArg,
            "Invalid ICache key specification \"" << m_Opts.id << "\" ("
            "expected a comma-separated key,version,subkey triplet).");
    }
    if (!key_parts[1].empty())
        m_Opts.icache_key.version = NStr::StringToInt(key_parts[1]);
    else if (permit_empty_version)
        *version_is_defined = false;
    else {
        NCBI_THROW(CArgException, eNoValue,
            "blob version parameter is missing");
    }
    m_Opts.icache_key.key = key_parts.front();
    m_Opts.icache_key.subkey = key_parts.back();
}

int CGridCommandLineInterfaceApp::Cmd_GetBlob()
{
    bool icache_mode = IsOptionSet(eCache);

    SetUp_NetCacheCmd(icache_mode ? eNetICacheClient : eNetCacheAPI);

    int reader_select = (IsOptionSet(ePassword) << 1) |
        (m_Opts.offset != 0 || m_Opts.size != 0);

    auto_ptr<IReader> reader;

    if (!icache_mode) {
        size_t blob_size = 0;
        switch (reader_select) {
        case 0: /* no special case */
            reader.reset(m_NetCacheAPI.GetReader(
                m_Opts.id,
                &blob_size,
                CNetCacheAPI::eCaching_Disable));
            break;
        case 1: /* use offset */
            reader.reset(m_NetCacheAPI.GetPartReader(
                m_Opts.id,
                m_Opts.offset,
                m_Opts.size,
                &blob_size,
                CNetCacheAPI::eCaching_Disable));
            break;
        case 2: /* use password */
            reader.reset(CNetCachePasswordGuard(m_NetCacheAPI,
                m_Opts.password)->GetReader(
                    m_Opts.id,
                    &blob_size,
                    CNetCacheAPI::eCaching_Disable));
            break;
        case 3: /* use password and offset */
            reader.reset(CNetCachePasswordGuard(m_NetCacheAPI,
                m_Opts.password)->GetPartReader(
                    m_Opts.id,
                    m_Opts.offset,
                    m_Opts.size,
                    &blob_size,
                    CNetCacheAPI::eCaching_Disable));
        }
    } else {
        bool version_is_defined = true;

        ParseICacheKey(reader_select == 0, &version_is_defined);

        ICache::EBlobValidity validity;
        switch (reader_select) {
        case 0: /* no special case */
            reader.reset(version_is_defined ?
                m_NetICacheClient.GetReadStream(
                    m_Opts.icache_key.key,
                    m_Opts.icache_key.version,
                    m_Opts.icache_key.subkey,
                    NULL,
                    CNetCacheAPI::eCaching_Disable) :
                m_NetICacheClient.GetReadStream(
                    m_Opts.icache_key.key,
                    m_Opts.icache_key.subkey,
                    &m_Opts.icache_key.version,
                    &validity));
            break;
        case 1: /* use offset */
            reader.reset(m_NetICacheClient.GetReadStreamPart(
                m_Opts.icache_key.key,
                m_Opts.icache_key.version,
                m_Opts.icache_key.subkey,
                m_Opts.offset,
                m_Opts.size,
                NULL,
                CNetCacheAPI::eCaching_Disable));
            break;
        case 2: /* use password */
            reader.reset(CNetICachePasswordGuard(m_NetICacheClient,
                m_Opts.password)->GetReadStream(
                    m_Opts.icache_key.key,
                    m_Opts.icache_key.version,
                    m_Opts.icache_key.subkey,
                    NULL,
                    CNetCacheAPI::eCaching_Disable));
            break;
        case 3: /* use password and offset */
            reader.reset(CNetICachePasswordGuard(m_NetICacheClient,
                m_Opts.password)->GetReadStreamPart(
                    m_Opts.icache_key.key,
                    m_Opts.icache_key.version,
                    m_Opts.icache_key.subkey,
                    m_Opts.offset,
                    m_Opts.size,
                    NULL,
                    CNetCacheAPI::eCaching_Disable));
        }
        if (!version_is_defined)
            NcbiCerr << "Blob version: " <<
                    m_Opts.icache_key.version << NcbiEndl <<
                "Blob validity: " << (validity == ICache::eValid ?
                    "valid" : "expired") << NcbiEndl;
    }
    if (!reader.get()) {
        NCBI_THROW(CNetCacheException, eBlobNotFound,
            "Cannot find the requested blob");
    }

    char buffer[16 * 1024];
    ERW_Result read_result;
    size_t bytes_read;

    while ((read_result = reader->Read(buffer,
            sizeof(buffer), &bytes_read)) == eRW_Success)
        fwrite(buffer, 1, bytes_read, m_Opts.output_stream);

    if (read_result != eRW_Eof) {
        ERR_POST("Error while sending data to the output stream");
        return 1;
    }

    return 0;
}

int CGridCommandLineInterfaceApp::Cmd_PutBlob()
{
    bool icache_mode = IsOptionSet(eCache);

    SetUp_NetCacheCmd(icache_mode ? eNetICacheClient : eNetCacheAPI);

    auto_ptr<IEmbeddedStreamWriter> writer;

    if (!icache_mode) {
        writer.reset(IsOptionSet(ePassword) ?
            CNetCachePasswordGuard(m_NetCacheAPI,
                m_Opts.password)->PutData(&m_Opts.id, m_Opts.ttl) :
            m_NetCacheAPI.PutData(&m_Opts.id, m_Opts.ttl));
    } else {
        ParseICacheKey();

        writer.reset(IsOptionSet(ePassword) ?
            CNetICachePasswordGuard(m_NetICacheClient,
                m_Opts.password)->GetNetCacheWriter(
                    m_Opts.icache_key.key,
                    m_Opts.icache_key.version,
                    m_Opts.icache_key.subkey,
                    m_Opts.ttl) :
            m_NetICacheClient.GetNetCacheWriter(
                m_Opts.icache_key.key,
                m_Opts.icache_key.version,
                m_Opts.icache_key.subkey,
                m_Opts.ttl));
    }

    if (!writer.get()) {
        NCBI_USER_THROW_FMT("Cannot create blob stream");
    }

    char buffer[16 * 1024];
    size_t bytes_read;
    size_t bytes_written;

    while ((bytes_read = fread(buffer, 1,
            sizeof(buffer), m_Opts.input_stream)) > 0) {
        if (writer->Write(buffer, bytes_read, &bytes_written) !=
                eRW_Success || bytes_written != bytes_read) {
            NCBI_USER_THROW_FMT("Error while writing to NetCache");
        }
        if (feof(m_Opts.input_stream))
            break;
    }

    writer->Close();

    if (!icache_mode)
        NcbiCout << m_Opts.id << NcbiEndl;

    return 0;
}

int CGridCommandLineInterfaceApp::Cmd_BlobInfo()
{
    bool icache_mode = IsOptionSet(eCache);

    SetUp_NetCacheCmd(icache_mode ? eNetICacheClient : eNetCacheAPI);

    if (!icache_mode)
        m_NetCacheAPI.PrintBlobInfo(m_Opts.id);
    else
        m_NetICacheClient.PrintBlobInfo(m_Opts.icache_key.key,
            m_Opts.icache_key.version, m_Opts.icache_key.subkey);

    return 0;
}

int CGridCommandLineInterfaceApp::Cmd_RemoveBlob()
{
    bool icache_mode = IsOptionSet(eCache);

    SetUp_NetCacheCmd(icache_mode ? eNetICacheClient : eNetCacheAPI);

    if (!icache_mode)
        if (IsOptionSet(ePassword))
            CNetCachePasswordGuard(m_NetCacheAPI,
                m_Opts.password)->Remove(m_Opts.id);
        else
            m_NetCacheAPI.Remove(m_Opts.id);
    else
        if (IsOptionSet(ePassword))
            CNetICachePasswordGuard(m_NetICacheClient,
                m_Opts.password)->Remove(m_Opts.icache_key.key,
                    m_Opts.icache_key.version, m_Opts.icache_key.subkey);
        else
            m_NetICacheClient.Remove(m_Opts.icache_key.key,
                m_Opts.icache_key.version, m_Opts.icache_key.subkey);

    return 0;
}

int CGridCommandLineInterfaceApp::Cmd_ReinitNetCache()
{
    bool icache_mode = IsOptionSet(eCache);

    SetUp_NetCacheCmd(eNetCacheAdmin);

    if (icache_mode)
        m_NetCacheAdmin.Reinitialize(m_Opts.cache_name);
    else
        m_NetCacheAdmin.Reinitialize();

    return 0;
}
