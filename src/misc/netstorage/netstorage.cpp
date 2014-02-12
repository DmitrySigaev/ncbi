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
 * Author:  Dmitry Kazimirov
 *
 * File Description:
 *   Implementation of the unified network blob storage API.
 *
 */

#include <ncbi_pch.hpp>

#include "filetrack.hpp"

#include <misc/netstorage/netstorage.hpp>
#include <misc/netstorage/error_codes.hpp>

#include <util/util_exception.hpp>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#ifndef NCBI_OS_MSWIN
#  include <unistd.h>
#endif

#define NCBI_USE_ERRCODE_X  NetStorage

#define RELOCATION_BUFFER_SIZE (128 * 1024)

BEGIN_NCBI_SCOPE

NCBI_PARAM_DEF(string, netstorage_api, backend_storage, "netcache, filetrack");

NCBI_PARAM_DEF(string, filetrack, site, "prod");
NCBI_PARAM_DEF(string, filetrack, api_key, kEmptyStr);

struct SNetStorageObjectAPIImpl;

struct SNetStorageAPIImpl : public SNetStorageImpl
{
    SNetStorageAPIImpl(CNetICacheClient::TInstance icache_client,
            TNetStorageFlags default_flags);

    virtual CNetStorageObject Create(TNetStorageFlags flags = 0);
    virtual CNetStorageObject Open(const string& object_loc,
            TNetStorageFlags flags = 0);
    virtual string Relocate(const string& object_loc, TNetStorageFlags flags);
    virtual bool Exists(const string& object_loc);
    virtual void Remove(const string& object_loc);

    TNetStorageFlags GetDefaultFlags(TNetStorageFlags flags)
    {
        return flags != 0 ? flags : m_DefaultFlags;
    }

    string MoveFile(SNetStorageObjectAPIImpl* orig_file,
            SNetStorageObjectAPIImpl* new_file);

    CCompoundIDPool m_CompoundIDPool;
    CNetICacheClient m_NetICacheClient;
    TNetStorageFlags m_DefaultFlags;
    TNetStorageFlags m_AvailableStorageMask;

    SFileTrackAPI m_FileTrackAPI;
};

SNetStorageAPIImpl::SNetStorageAPIImpl(
        CNetICacheClient::TInstance icache_client,
        TNetStorageFlags default_flags) :
    m_NetICacheClient(icache_client),
    m_DefaultFlags(default_flags),
    m_AvailableStorageMask(0)
{
    string backend_storage(TNetStorageAPI_BackendStorage::GetDefault());

    if (strstr(backend_storage.c_str(), "netcache") != NULL)
        m_AvailableStorageMask |= fNST_Fast;
    if (strstr(backend_storage.c_str(), "filetrack") != NULL)
        m_AvailableStorageMask |= fNST_Persistent;

    m_DefaultFlags &= m_AvailableStorageMask;

    if (m_DefaultFlags == 0)
        m_DefaultFlags = m_AvailableStorageMask;

    m_AvailableStorageMask |= fNST_Movable | fNST_Cacheable;
}

enum ENetStorageObjectIOStatus {
    eNFS_Closed,
    eNFS_WritingToNetCache,
    eNFS_ReadingFromNetCache,
    eNFS_WritingToFileTrack,
    eNFS_ReadingFromFileTrack,
};

class ITryLocation
{
public:
    virtual bool TryLocation(ENetStorageObjectLocation location) = 0;
    virtual ~ITryLocation() {}
};

struct SNetStorageObjectAPIImpl : public SNetStorageObjectImpl
{
    SNetStorageObjectAPIImpl(SNetStorageAPIImpl* storage_impl,
            TNetStorageFlags flags,
            Uint8 random_number,
            CNetICacheClient::TInstance icache_client) :
        m_NetStorage(storage_impl),
        m_NetICacheClient(icache_client),
        m_ObjectLoc(storage_impl->m_CompoundIDPool, flags, random_number,
                TFileTrack_Site::GetDefault().c_str()),
        m_CurrentLocation(eNFL_Unknown),
        m_IOStatus(eNFS_Closed)
    {
        g_SetNetICacheParams(m_ObjectLoc, icache_client);
    }

    SNetStorageObjectAPIImpl(SNetStorageAPIImpl* storage_impl,
            const string& object_loc) :
        m_NetStorage(storage_impl),
        m_NetICacheClient(eVoid),
        m_ObjectLoc(storage_impl->m_CompoundIDPool, object_loc),
        m_CurrentLocation(eNFL_Unknown),
        m_IOStatus(eNFS_Closed)
    {
    }

    SNetStorageObjectAPIImpl(SNetStorageAPIImpl* storage_impl,
            TNetStorageFlags flags,
            const string& domain_name,
            const string& unique_key,
            CNetICacheClient::TInstance icache_client) :
        m_NetStorage(storage_impl),
        m_NetICacheClient(icache_client),
        m_ObjectLoc(storage_impl->m_CompoundIDPool,
                flags, domain_name, unique_key,
                TFileTrack_Site::GetDefault().c_str()),
        m_CurrentLocation(eNFL_Unknown),
        m_IOStatus(eNFS_Closed)
    {
        g_SetNetICacheParams(m_ObjectLoc, icache_client);
    }

    SNetStorageObjectAPIImpl(SNetStorageAPIImpl* storage_impl,
            const CNetStorageObjectLoc& object_loc) :
        m_NetStorage(storage_impl),
        m_NetICacheClient(eVoid),
        m_ObjectLoc(object_loc),
        m_CurrentLocation(eNFL_Unknown),
        m_IOStatus(eNFS_Closed)
    {
    }

    virtual string GetLoc();
    virtual size_t Read(void* buffer, size_t buf_size);
    virtual bool Eof();
    virtual void Write(const void* buffer, size_t buf_size);
    virtual Uint8 GetSize();
    virtual string GetAttribute(const string& attr_name);
    virtual void SetAttribute(const string& attr_name,
            const string& attr_value);
    virtual CNetStorageObjectInfo GetInfo();
    virtual void Close();

    bool SetNetICacheClient();
    void DemandNetCache();

    CNetServer GetNetCacheServer();

    const ENetStorageObjectLocation* GetPossibleFileLocations(); // For reading.
    void ChooseLocation(); // For writing.

    ERW_Result s_ReadFromNetCache(char* buf, size_t count, size_t* bytes_read);

    bool s_TryReadLocation(ENetStorageObjectLocation location,
            ERW_Result* rw_res, char* buf, size_t count, size_t* bytes_read);

    ERW_Result BeginOrContinueReading(
            void* buf, size_t count, size_t* bytes_read = 0);

    ERW_Result Write(const void* buf,
            size_t count, size_t* bytes_written = 0);

    void Locate();
    bool LocateAndTry(ITryLocation* try_object);

    bool x_TryGetFileSizeFromLocation(ENetStorageObjectLocation location,
            Uint8* file_size);

    bool x_TryGetInfoFromLocation(ENetStorageObjectLocation location,
            CNetStorageObjectInfo* file_info);

    bool x_ExistsAtLocation(ENetStorageObjectLocation location);

    bool Exists();
    void Remove();

    virtual ~SNetStorageObjectAPIImpl();

    CRef<SNetStorageAPIImpl,
            CNetComponentCounterLocker<SNetStorageAPIImpl> > m_NetStorage;
    CNetICacheClient m_NetICacheClient;

    CNetStorageObjectLoc m_ObjectLoc;
    ENetStorageObjectLocation m_CurrentLocation;
    ENetStorageObjectIOStatus m_IOStatus;

    auto_ptr<IEmbeddedStreamWriter> m_NetCacheWriter;
    auto_ptr<IReader> m_NetCacheReader;
    size_t m_NetCache_BlobSize;
    size_t m_NetCache_BytesRead;

    CRef<SFileTrackRequest> m_FileTrackRequest;
    CRef<SFileTrackPostRequest> m_FileTrackPostRequest;
};

void g_SetNetICacheParams(CNetStorageObjectLoc& object_loc,
        CNetICacheClient icache_client)
{
    if (!icache_client)
        object_loc.ClearNetICacheParams();
    else {
        CNetService service(icache_client.GetService());

        CNetServer icache_server(service.Iterate().GetServer());

        object_loc.SetNetICacheParams(
                service.GetServiceName(), icache_client.GetCacheName(),
                icache_server.GetHost(), icache_server.GetPort()
#ifdef NCBI_GRID_XSITE_CONN_SUPPORT
                , service.IsUsingXSiteProxy()
#endif
                );
    }
}

bool SNetStorageObjectAPIImpl::SetNetICacheClient()
{
    if (!m_NetICacheClient) {
        if (m_ObjectLoc.GetFields() & fNFID_NetICache) {
            m_NetICacheClient = CNetICacheClient(m_ObjectLoc.GetNCServiceName(),
                    m_ObjectLoc.GetCacheName(), kEmptyStr);
#ifdef NCBI_GRID_XSITE_CONN_SUPPORT
            if (m_ObjectLoc.GetFields() & fNFID_AllowXSiteConn)
                m_NetICacheClient.GetService().AllowXSiteConnections();
#endif
        } else if (m_NetStorage->m_NetICacheClient)
            m_NetICacheClient = m_NetStorage->m_NetICacheClient;
        else
            return false;
    }
    return true;
}

void SNetStorageObjectAPIImpl::DemandNetCache()
{
    if (!SetNetICacheClient()) {
        NCBI_THROW(CNetStorageException, eInvalidArg,
                "The given storage preferences require a NetCache server.");
    }
}

CNetServer SNetStorageObjectAPIImpl::GetNetCacheServer()
{
    return (m_ObjectLoc.GetFields() & fNFID_NetICache) == 0 ? CNetServer() :
            m_NetICacheClient.GetService().GetServer(m_ObjectLoc.GetNetCacheIP(),
                    m_ObjectLoc.GetNetCachePort());
}

static const ENetStorageObjectLocation s_TryNetCacheThenFileTrack[] =
        {eNFL_NetCache, eNFL_FileTrack, eNFL_NotFound};

static const ENetStorageObjectLocation s_NetCache[] =
        {eNFL_NetCache, eNFL_NotFound};

static const ENetStorageObjectLocation s_TryFileTrackThenNetCache[] =
        {eNFL_FileTrack, eNFL_NetCache, eNFL_NotFound};

static const ENetStorageObjectLocation s_FileTrack[] =
        {eNFL_FileTrack, eNFL_NotFound};

const ENetStorageObjectLocation*
        SNetStorageObjectAPIImpl::GetPossibleFileLocations()
{
    TNetStorageFlags flags = m_ObjectLoc.GetStorageFlags();

    if (flags == 0)
        // Just guessing.
        return SetNetICacheClient() ? s_TryNetCacheThenFileTrack : s_FileTrack;

    if (flags & fNST_Movable) {
        if (!SetNetICacheClient())
            return s_FileTrack;

        return flags & fNST_Fast ? s_TryNetCacheThenFileTrack :
                s_TryFileTrackThenNetCache;
    }

    if (flags & fNST_Persistent)
        return s_FileTrack;

    DemandNetCache();
    return s_NetCache;
}

void SNetStorageObjectAPIImpl::ChooseLocation()
{
    TNetStorageFlags flags = m_ObjectLoc.GetStorageFlags();

    if (flags == 0)
        flags = (SetNetICacheClient() ? fNST_Fast : fNST_Persistent) &
                m_NetStorage->m_AvailableStorageMask;

    if (flags & fNST_Persistent)
        m_CurrentLocation = eNFL_FileTrack;
    else {
        DemandNetCache();
        m_CurrentLocation = eNFL_NetCache;
    }
}

ERW_Result SNetStorageObjectAPIImpl::s_ReadFromNetCache(char* buf,
        size_t count, size_t* bytes_read)
{
    size_t iter_bytes_read;
    size_t total_bytes_read = 0;
    ERW_Result rw_res = eRW_Success;

    while (count > 0) {
        rw_res = m_NetCacheReader->Read(buf, count, &iter_bytes_read);
        if (rw_res == eRW_Success) {
            total_bytes_read += iter_bytes_read;
            buf += iter_bytes_read;
            count -= iter_bytes_read;
        } else if (rw_res == eRW_Eof)
            break;
        else {
            NCBI_THROW(CNetStorageException, eIOError,
                    "I/O error while reading NetCache BLOB");
        }
    }

    m_NetCache_BytesRead += total_bytes_read;

    if (bytes_read != NULL)
        *bytes_read = total_bytes_read;

    return rw_res;
}

bool SNetStorageObjectAPIImpl::s_TryReadLocation(
        ENetStorageObjectLocation location,
        ERW_Result* rw_res, char* buf, size_t count, size_t* bytes_read)
{
    if (location == eNFL_NetCache) {
        m_NetCacheReader.reset(m_NetICacheClient.GetReadStream(
                m_ObjectLoc.GetUniqueKey(), 0, kEmptyStr,
                &m_NetCache_BlobSize,
                (nc_caching_mode = CNetCacheAPI::eCaching_Disable,
                nc_server_to_use = GetNetCacheServer())));

        if (m_NetCacheReader.get() != NULL) {
            m_NetCache_BytesRead = 0;
            *rw_res = s_ReadFromNetCache(buf, count, bytes_read);
            m_CurrentLocation = eNFL_NetCache;
            m_IOStatus = eNFS_ReadingFromNetCache;
            return true;
        }
    } else { /* location == eNFL_FileTrack */
        m_FileTrackRequest =
                m_NetStorage->m_FileTrackAPI.StartDownload(&m_ObjectLoc);

        try {
            *rw_res = m_FileTrackRequest->Read(buf, count, bytes_read);
            m_CurrentLocation = eNFL_FileTrack;
            m_IOStatus = eNFS_ReadingFromFileTrack;
            return true;
        }
        catch (CNetStorageException& e) {
            m_FileTrackRequest.Reset();

            if (e.GetErrCode() != CNetStorageException::eNotExists)
                throw;
        }
    }

    return false;
}

ERW_Result SNetStorageObjectAPIImpl::BeginOrContinueReading(
        void* buf, size_t count, size_t* bytes_read)
{
    switch (m_IOStatus) {
    case eNFS_Closed:
        switch (m_CurrentLocation) {
        case eNFL_Unknown:
            {
                ERW_Result rw_res = eRW_Success;
                const ENetStorageObjectLocation* location =
                        GetPossibleFileLocations();

                do
                    if (s_TryReadLocation(*location, &rw_res,
                            reinterpret_cast<char*>(buf), count, bytes_read))
                        return rw_res;
                while (*++location != eNFL_NotFound);
            }
            m_CurrentLocation = eNFL_NotFound;

        case eNFL_NotFound:
            break;

        default:
            {
                ERW_Result rw_res = eRW_Success;
                if (s_TryReadLocation(m_CurrentLocation, &rw_res,
                        reinterpret_cast<char*>(buf), count, bytes_read))
                    return rw_res;
            }
        }

        NCBI_THROW_FMT(CNetStorageException, eNotExists,
                "Cannot open \"" << m_ObjectLoc.GetLoc() << "\" for reading.");

    case eNFS_ReadingFromNetCache:
        return s_ReadFromNetCache(reinterpret_cast<char*>(buf),
                count, bytes_read);

    case eNFS_ReadingFromFileTrack:
        m_FileTrackRequest->Read(buf, count, bytes_read);
        break;

    default: /* eNFS_WritingToNetCache or eNFS_WritingToFileTrack */
        NCBI_THROW_FMT(CNetStorageException, eInvalidArg,
                "Invalid file status: cannot read while writing.");
    }

    return eRW_Success;
}

bool SNetStorageObjectAPIImpl::Eof()
{
    switch (m_IOStatus) {
    case eNFS_Closed:
        return false;

    case eNFS_ReadingFromNetCache:
        return m_NetCache_BytesRead >= m_NetCache_BlobSize;

    case eNFS_ReadingFromFileTrack:
        return m_FileTrackRequest->m_HTTPStream.eof();

    default: /* eNFS_WritingToNetCache or eNFS_WritingToFileTrack */
        break;
    }

    NCBI_THROW_FMT(CNetStorageException, eInvalidArg,
            "Invalid file status: cannot check EOF status while writing.");
}

ERW_Result SNetStorageObjectAPIImpl::Write(const void* buf, size_t count,
        size_t* bytes_written)
{
    switch (m_IOStatus) {
    case eNFS_Closed:
        if (m_CurrentLocation == eNFL_Unknown)
            ChooseLocation();

        if (m_CurrentLocation == eNFL_NetCache) {
            m_NetCacheWriter.reset(m_NetICacheClient.GetNetCacheWriter(
                    m_ObjectLoc.GetUniqueKey(), 0, kEmptyStr,
                            nc_blob_ttl = (m_ObjectLoc.GetFields() & fNFID_TTL ?
                                    (unsigned) m_ObjectLoc.GetTTL() : 0)));

            m_IOStatus = eNFS_WritingToNetCache;

            return m_NetCacheWriter->Write(buf, count, bytes_written);
        } else {
            m_FileTrackPostRequest =
                    m_NetStorage->m_FileTrackAPI.StartUpload(&m_ObjectLoc);

            m_IOStatus = eNFS_WritingToFileTrack;

            m_FileTrackPostRequest->Write(buf, count, bytes_written);
        }
        break;

    case eNFS_WritingToNetCache:
        return m_NetCacheWriter->Write(buf, count, bytes_written);

    case eNFS_WritingToFileTrack:
        m_FileTrackPostRequest->Write(buf, count, bytes_written);
        break;

    default: /* eNFS_ReadingToNetCache or eNFS_ReadingFromFileTrack */
        NCBI_THROW_FMT(CNetStorageException, eInvalidArg,
                "Invalid file status: cannot write while reading.");
    }

    return eRW_Success;
}

struct SCheckForExistence : public ITryLocation
{
    SCheckForExistence(SNetStorageObjectAPIImpl* netstorage_object_api) :
        m_NetStorageObjectAPI(netstorage_object_api)
    {
    }

    virtual bool TryLocation(ENetStorageObjectLocation location);

    CRef<SNetStorageObjectAPIImpl, CNetComponentCounterLocker<
            SNetStorageObjectAPIImpl> > m_NetStorageObjectAPI;
};

bool SCheckForExistence::TryLocation(ENetStorageObjectLocation location)
{
    try {
        if (location == eNFL_NetCache) {
            if (m_NetStorageObjectAPI->m_NetICacheClient.HasBlob(
                    m_NetStorageObjectAPI->m_ObjectLoc.GetUniqueKey(), kEmptyStr,
                    nc_server_to_use =
                            m_NetStorageObjectAPI->GetNetCacheServer()))
                return true;
        } else { /* location == eNFL_FileTrack */
            if (!m_NetStorageObjectAPI->m_NetStorage->
                    m_FileTrackAPI.GetFileInfo(
                    &m_NetStorageObjectAPI->m_ObjectLoc).GetBoolean("deleted"))
                return true;
        }
    }
    catch (CException& e) {
        LOG_POST(Trace << e);
    }

    return false;
}

void SNetStorageObjectAPIImpl::Locate()
{
    SCheckForExistence check_for_existence(this);

    switch (m_CurrentLocation) {
    case eNFL_Unknown:
        {
            const ENetStorageObjectLocation* location =
                    GetPossibleFileLocations();

            do
                if (check_for_existence.TryLocation(*location)) {
                    m_CurrentLocation = *location;
                    return;
                }
            while (*++location != eNFL_NotFound);
        }
        m_CurrentLocation = eNFL_NotFound;

    case eNFL_NotFound:
        {
            NCBI_THROW_FMT(CNetStorageException, eNotExists,
                    "NetStorageObject \"" << m_ObjectLoc.GetLoc() <<
                    "\" could not be found.");
        }
        break;

    default:
        break;
    }
}

bool SNetStorageObjectAPIImpl::LocateAndTry(ITryLocation* try_object)
{
    switch (m_CurrentLocation) {
    case eNFL_Unknown:
        {
            const ENetStorageObjectLocation* location =
                    GetPossibleFileLocations();

            do
                if (try_object->TryLocation(*location))
                    return true;
            while (*++location != eNFL_NotFound);
        }
        m_CurrentLocation = eNFL_NotFound;

    case eNFL_NotFound:
        break;

    default:
        {
            CNetStorageObjectInfo file_info;
            if (try_object->TryLocation(m_CurrentLocation))
                return true;
        }
    }

    return false;
}

bool SNetStorageObjectAPIImpl::x_TryGetFileSizeFromLocation(
        ENetStorageObjectLocation location,
        Uint8* file_size)
{
    try {
        if (location == eNFL_NetCache) {
            *file_size = m_NetICacheClient.GetBlobSize(
                    m_ObjectLoc.GetUniqueKey(), 0, kEmptyStr,
                            nc_server_to_use = GetNetCacheServer());

            m_CurrentLocation = eNFL_NetCache;
            return true;
        } else { /* location == eNFL_FileTrack */
            *file_size = (Uint8) m_NetStorage->m_FileTrackAPI.GetFileInfo(
                    &m_ObjectLoc).GetInteger("size");

            m_CurrentLocation = eNFL_FileTrack;
            return true;
        }
    }
    catch (CException& e) {
        LOG_POST(Trace << e);
    }

    return false;
}

Uint8 SNetStorageObjectAPIImpl::GetSize()
{
    switch (m_CurrentLocation) {
    case eNFL_Unknown:
        {
            Uint8 file_size;
            const ENetStorageObjectLocation* location =
                    GetPossibleFileLocations();

            do
                if (x_TryGetFileSizeFromLocation(*location, &file_size))
                    return file_size;
            while (*++location != eNFL_NotFound);
        }
        m_CurrentLocation = eNFL_NotFound;

    case eNFL_NotFound:
        break;

    default:
        {
            Uint8 file_size;

            if (x_TryGetFileSizeFromLocation(m_CurrentLocation, &file_size))
                return file_size;
        }
    }

    NCBI_THROW_FMT(CNetStorageException, eNotExists,
            "NetStorageObject \"" << m_ObjectLoc.GetLoc() <<
            "\" could not be found in any of the designated locations.");
}

struct STryGetAttr : public ITryLocation
{
    STryGetAttr(SNetStorageObjectAPIImpl* netstorage_object_api,
            const string& attr_name) :
        m_NetStorageObjectAPI(netstorage_object_api),
        m_AttrName(attr_name)
    {
    }

    virtual bool TryLocation(ENetStorageObjectLocation location);

    CRef<SNetStorageObjectAPIImpl, CNetComponentCounterLocker<
            SNetStorageObjectAPIImpl> > m_NetStorageObjectAPI;
    string m_AttrName;
    string m_AttrValue;
};

bool STryGetAttr::TryLocation(ENetStorageObjectLocation location)
{
    try {
        if (location == eNFL_NetCache) {
            string attr_blob_id =
                m_NetStorageObjectAPI->m_ObjectLoc.GetUniqueKey() + "_attr_";
            attr_blob_id += m_AttrName;

            size_t blob_size;

            auto_ptr<IReader> reader(m_NetStorageObjectAPI->m_NetICacheClient.
                    GetReadStream(attr_blob_id, 0, kEmptyStr, &blob_size,
                    nc_server_to_use =
                            m_NetStorageObjectAPI->GetNetCacheServer()));

            if (reader.get() == NULL)
                return false;

            m_AttrValue.resize(blob_size);

            if (reader->Read(const_cast<char*>(m_AttrValue.data()),
                    blob_size) != eRW_Success) {
                ERR_POST("Error while retrieving the \"" <<
                        m_AttrName << "\" attribute value");
                return false;
            }
        } else { /* location == eNFL_FileTrack */
            m_AttrValue = m_NetStorageObjectAPI->m_NetStorage->m_FileTrackAPI.
                    GetFileAttribute(
                            &m_NetStorageObjectAPI->m_ObjectLoc, m_AttrName);
        }

        return true;
    }
    catch (CException& e) {
        LOG_POST(Trace << e);
    }

    return false;
}

string SNetStorageObjectAPIImpl::GetAttribute(const string& attr_name)
{
    STryGetAttr try_get_attr(this, attr_name);

    if (LocateAndTry(&try_get_attr))
        return try_get_attr.m_AttrValue;

    NCBI_THROW_FMT(CNetStorageException, eNotExists,
            "Failed to retrieve attribute \"" << attr_name <<
            "\" of \"" << m_ObjectLoc.GetLoc() << "\".");
}

void SNetStorageObjectAPIImpl::SetAttribute(const string& attr_name,
        const string& attr_value)
{
    Locate();
    if (m_CurrentLocation == eNFL_NetCache) {
        string attr_blob_id = m_ObjectLoc.GetUniqueKey() + "_attr_";
        attr_blob_id += attr_name;

        auto_ptr<IEmbeddedStreamWriter> writer(
                m_NetICacheClient.GetNetCacheWriter(attr_blob_id, 0, kEmptyStr,
                        nc_server_to_use = GetNetCacheServer()));
        writer->Write(attr_value.data(), attr_value.length());
        writer->Close();
    } else { /* location == eNFL_FileTrack */
        m_NetStorage->m_FileTrackAPI.SetFileAttribute(
                &m_ObjectLoc, attr_name, attr_value);
    }
}

bool SNetStorageObjectAPIImpl::x_TryGetInfoFromLocation(
        ENetStorageObjectLocation location,
        CNetStorageObjectInfo* file_info)
{
    if (location == eNFL_NetCache) {
        try {
            CNetServerMultilineCmdOutput output = m_NetICacheClient.GetBlobInfo(
                    m_ObjectLoc.GetUniqueKey(), 0, kEmptyStr,
                    nc_server_to_use = GetNetCacheServer());

            CJsonNode blob_info = CJsonNode::NewObjectNode();
            string line;
            string key, val;
            Uint8 blob_size = 0;
            bool got_blob_size = false;

            while (output.ReadLine(line)) {
                if (!NStr::SplitInTwo(line, ": ",
                        key, val, NStr::fSplit_ByPattern))
                    continue;

                blob_info.SetByKey(key, CJsonNode::GuessType(val));

                if (key == "Size") {
                    blob_size = NStr::StringToUInt8(val,
                            NStr::fConvErr_NoThrow);
                    got_blob_size = blob_size != 0 || errno == 0;
                }
            }

            if (!got_blob_size)
                blob_size = m_NetICacheClient.GetSize(
                        m_ObjectLoc.GetUniqueKey(), 0, kEmptyStr);

            *file_info = CNetStorageObjectInfo(m_ObjectLoc.GetLoc(),
                    eNFL_NetCache, m_ObjectLoc, blob_size, blob_info);

            m_CurrentLocation = eNFL_NetCache;
            return true;
        }
        catch (CException& e) {
            LOG_POST(Trace << e);
        }
    } else { /* location == eNFL_FileTrack */
        try {
            CJsonNode file_info_node =
                    m_NetStorage->m_FileTrackAPI.GetFileInfo(&m_ObjectLoc);

            Uint8 file_size = 0;

            CJsonNode size_node = file_info_node.GetByKeyOrNull("size");

            if (size_node)
                file_size = (Uint8) size_node.AsInteger();

            *file_info = CNetStorageObjectInfo(m_ObjectLoc.GetLoc(),
                    eNFL_FileTrack, m_ObjectLoc, file_size, file_info_node);

            m_CurrentLocation = eNFL_FileTrack;
            return true;
        }
        catch (CException& e) {
            LOG_POST(Trace << e);
        }
    }

    return false;
}

CNetStorageObjectInfo SNetStorageObjectAPIImpl::GetInfo()
{
    switch (m_CurrentLocation) {
    case eNFL_Unknown:
        {
            CNetStorageObjectInfo file_info;
            const ENetStorageObjectLocation* location =
                    GetPossibleFileLocations();

            do
                if (x_TryGetInfoFromLocation(*location, &file_info))
                    return file_info;
            while (*++location != eNFL_NotFound);
        }
        m_CurrentLocation = eNFL_NotFound;

    case eNFL_NotFound:
        break;

    default:
        {
            CNetStorageObjectInfo file_info;
            if (x_TryGetInfoFromLocation(m_CurrentLocation, &file_info))
                return file_info;
        }
    }

    return CNetStorageObjectInfo(m_ObjectLoc.GetLoc(),
            eNFL_NotFound, m_ObjectLoc, 0, NULL);
}

bool SNetStorageObjectAPIImpl::x_ExistsAtLocation(
        ENetStorageObjectLocation location)
{
    if (location == eNFL_NetCache) {
        try {
            if (!m_NetICacheClient.HasBlob(m_ObjectLoc.GetUniqueKey(),
                    kEmptyStr, nc_server_to_use = GetNetCacheServer()))
                return false;

            m_CurrentLocation = eNFL_NetCache;
            return true;
        }
        catch (CException& e) {
            LOG_POST(Trace << e);
        }
    } else { /* location == eNFL_FileTrack */
        try {
            if (m_NetStorage->m_FileTrackAPI.GetFileInfo(
                    &m_ObjectLoc).GetBoolean("deleted"))
                return false;

            m_CurrentLocation = eNFL_FileTrack;
            return true;
        }
        catch (CException& e) {
            LOG_POST(Trace << e);
        }
    }

    return false;
}

bool SNetStorageObjectAPIImpl::Exists()
{
    switch (m_CurrentLocation) {
    case eNFL_Unknown:
        {
            const ENetStorageObjectLocation* location =
                    GetPossibleFileLocations();

            do
                if (x_ExistsAtLocation(*location))
                    return true;
            while (*++location != eNFL_NotFound);
        }
        m_CurrentLocation = eNFL_NotFound;

    case eNFL_NotFound:
        break;

    default:
        return x_ExistsAtLocation(m_CurrentLocation);
    }

    return false;
}

void SNetStorageObjectAPIImpl::Remove()
{
    if (m_CurrentLocation == eNFL_Unknown ||
            m_CurrentLocation == eNFL_FileTrack)
        try {
            m_NetStorage->m_FileTrackAPI.Remove(&m_ObjectLoc);
        }
        catch (CException& e) {
            LOG_POST(Trace << e);
        }

    if (m_CurrentLocation == eNFL_Unknown ||
            m_CurrentLocation == eNFL_NetCache)
        try {
            m_NetICacheClient.RemoveBlob(m_ObjectLoc.GetUniqueKey(), 0,
                    kEmptyStr, nc_server_to_use = GetNetCacheServer());
        }
        catch (CException& e) {
            LOG_POST(Trace << e);
        }

    m_CurrentLocation = eNFL_NotFound;
}

void SNetStorageObjectAPIImpl::Close()
{
    switch (m_IOStatus) {
    default: /* case eNFS_Closed: */
        break;

    case eNFS_WritingToNetCache:
        m_IOStatus = eNFS_Closed;
        m_NetCacheWriter->Close();
        m_NetCacheWriter.reset();
        break;

    case eNFS_ReadingFromNetCache:
        m_IOStatus = eNFS_Closed;
        m_NetCacheReader.reset();
        break;

    case eNFS_WritingToFileTrack:
        m_IOStatus = eNFS_Closed;
        m_FileTrackPostRequest->FinishUpload();
        m_FileTrackPostRequest.Reset();
        break;

    case eNFS_ReadingFromFileTrack:
        m_IOStatus = eNFS_Closed;
        m_FileTrackRequest->FinishDownload();
        m_FileTrackRequest.Reset();
    }
}

SNetStorageObjectAPIImpl::~SNetStorageObjectAPIImpl()
{
    try {
        Close();
    }
    NCBI_CATCH_ALL("Error while implicitly closing a NetStorage file.");
}

string SNetStorageObjectAPIImpl::GetLoc()
{
    return m_ObjectLoc.GetLoc();
}

size_t SNetStorageObjectAPIImpl::Read(void* buffer, size_t buf_size)
{
    size_t bytes_read;

    BeginOrContinueReading(buffer, buf_size, &bytes_read);

    return bytes_read;
}

void SNetStorageObjectAPIImpl::Write(const void* buffer, size_t buf_size)
{
    Write(buffer, buf_size, NULL);
}

CNetStorageObject SNetStorageAPIImpl::Create(TNetStorageFlags flags)
{
    // Restrict to available storage backends.
    flags &= m_AvailableStorageMask;

    Uint8 random_number = 0;

#ifndef NCBI_OS_MSWIN
    int urandom_fd = open("/dev/urandom", O_RDONLY);
    if (urandom_fd < 0) {
        NCBI_USER_THROW_FMT("Cannot open /dev/urandom: " <<
                strerror(errno));
    }

    char* read_buffer = reinterpret_cast<char*>(&random_number) + 1;
    ssize_t bytes_read;
    ssize_t bytes_remain = sizeof(random_number) - 2;
    do {
        bytes_read = read(urandom_fd, read_buffer, bytes_remain);
        if (bytes_read < 0 && errno != EINTR) {
            NCBI_USER_THROW_FMT("Error while reading "
                    "/dev/urandom: " << strerror(errno));
        }
        read_buffer += bytes_read;
    } while ((bytes_remain -= bytes_read) > 0);

    close(urandom_fd);

    random_number >>= 8;
#else
    random_number = m_FileTrackAPI.GetRandom();
#endif // NCBI_OS_MSWIN

    CRef<SNetStorageObjectAPIImpl, CNetComponentCounterLocker<
            SNetStorageObjectAPIImpl> > netstorage_object(
                    new SNetStorageObjectAPIImpl(this, GetDefaultFlags(flags),
                    random_number, m_NetICacheClient));

    return netstorage_object.GetPointerOrNull();
}

CNetStorageObject SNetStorageAPIImpl::Open(
        const string& object_loc, TNetStorageFlags flags)
{
    flags &= m_AvailableStorageMask;

    CRef<SNetStorageObjectAPIImpl, CNetComponentCounterLocker<
            SNetStorageObjectAPIImpl> > netstorage_object(
                    new SNetStorageObjectAPIImpl(this, object_loc));

    if (flags != 0)
        netstorage_object->m_ObjectLoc.SetStorageFlags(flags);

    return netstorage_object.GetPointerOrNull();
}

string SNetStorageAPIImpl::MoveFile(SNetStorageObjectAPIImpl* orig_file,
        SNetStorageObjectAPIImpl* new_file)
{
    new_file->ChooseLocation();

    if (new_file->m_CurrentLocation == eNFL_NetCache &&
            (new_file->m_ObjectLoc.GetFields() & fNFID_NetICache) == 0)
        g_SetNetICacheParams(new_file->m_ObjectLoc, m_NetICacheClient);

    char buffer[RELOCATION_BUFFER_SIZE];

    // Use Read() to detect the current location of orig_file.
    size_t bytes_read;

    orig_file->BeginOrContinueReading(buffer, sizeof(buffer), &bytes_read);

    if (orig_file->m_CurrentLocation != new_file->m_CurrentLocation) {
        for (;;) {
            new_file->Write(buffer, bytes_read, NULL);
            if (orig_file->Eof())
                break;
            orig_file->BeginOrContinueReading(
                    buffer, sizeof(buffer), &bytes_read);
        }

        new_file->Close();
        orig_file->Close();

        orig_file->Remove();
    }

    return new_file->m_ObjectLoc.GetLoc();
}

string SNetStorageAPIImpl::Relocate(const string& object_loc,
        TNetStorageFlags flags)
{
    flags &= m_AvailableStorageMask;

    if (flags == 0)
        return object_loc;

    CRef<SNetStorageObjectAPIImpl, CNetComponentCounterLocker<
            SNetStorageObjectAPIImpl> >
                    orig_file(new SNetStorageObjectAPIImpl(this, object_loc));

    if (orig_file->m_ObjectLoc.GetStorageFlags() == flags)
        return object_loc;

    CRef<SNetStorageObjectAPIImpl, CNetComponentCounterLocker<
            SNetStorageObjectAPIImpl> >
                    new_file(new SNetStorageObjectAPIImpl(this,
                                orig_file->m_ObjectLoc));

    new_file->m_ObjectLoc.SetStorageFlags(flags);

    return MoveFile(orig_file, new_file);
}

bool SNetStorageAPIImpl::Exists(const string& object_loc)
{
    CRef<SNetStorageObjectAPIImpl, CNetComponentCounterLocker<
            SNetStorageObjectAPIImpl> >
                    net_file(new SNetStorageObjectAPIImpl(this, object_loc));

    return net_file->Exists();
}

void SNetStorageAPIImpl::Remove(const string& object_loc)
{
    CRef<SNetStorageObjectAPIImpl, CNetComponentCounterLocker<
            SNetStorageObjectAPIImpl> >
                    net_file(new SNetStorageObjectAPIImpl(this, object_loc));

    return net_file->Remove();
}

struct SNetStorageByKeyAPIImpl : public SNetStorageByKeyImpl
{
    SNetStorageByKeyAPIImpl(CNetICacheClient::TInstance icache_client,
            const string& domain_name, TNetStorageFlags default_flags) :
        m_APIImpl(new SNetStorageAPIImpl(icache_client, default_flags)),
        m_DomainName(domain_name)
    {
        if (domain_name.empty()) {
            if (icache_client != NULL)
                m_DomainName = CNetICacheClient(icache_client).GetCacheName();
            else {
                NCBI_THROW_FMT(CNetStorageException, eInvalidArg,
                        "Domain name cannot be empty.");
            }
        }
    }

    virtual CNetStorageObject Open(const string& unique_key,
            TNetStorageFlags flags = 0);
    virtual string Relocate(const string& unique_key,
            TNetStorageFlags flags, TNetStorageFlags old_flags = 0);
    virtual bool Exists(const string& key, TNetStorageFlags flags = 0);
    virtual void Remove(const string& key, TNetStorageFlags flags = 0);

    CRef<SNetStorageAPIImpl,
        CNetComponentCounterLocker<SNetStorageAPIImpl> > m_APIImpl;

    string m_DomainName;
};

CNetStorageObject SNetStorageByKeyAPIImpl::Open(const string& unique_key,
        TNetStorageFlags flags)
{
    return new SNetStorageObjectAPIImpl(m_APIImpl,
            m_APIImpl->GetDefaultFlags(flags) &
                    m_APIImpl->m_AvailableStorageMask,
            m_DomainName, unique_key, m_APIImpl->m_NetICacheClient);
}

string SNetStorageByKeyAPIImpl::Relocate(const string& unique_key,
        TNetStorageFlags flags, TNetStorageFlags old_flags)
{
    CRef<SNetStorageObjectAPIImpl, CNetComponentCounterLocker<
            SNetStorageObjectAPIImpl> >
                    orig_file(new SNetStorageObjectAPIImpl(m_APIImpl, old_flags,
                            m_DomainName, unique_key,
                            m_APIImpl->m_NetICacheClient));

    CRef<SNetStorageObjectAPIImpl, CNetComponentCounterLocker<
            SNetStorageObjectAPIImpl> >
                    new_file(new SNetStorageObjectAPIImpl(m_APIImpl,
                            flags & m_APIImpl->m_AvailableStorageMask,
                            m_DomainName, unique_key,
                            m_APIImpl->m_NetICacheClient));

    return m_APIImpl->MoveFile(orig_file, new_file);
}

bool SNetStorageByKeyAPIImpl::Exists(const string& key, TNetStorageFlags flags)
{
    CRef<SNetStorageObjectAPIImpl, CNetComponentCounterLocker<
            SNetStorageObjectAPIImpl> >
                    net_file(new SNetStorageObjectAPIImpl(m_APIImpl,
                            m_APIImpl->GetDefaultFlags(flags),
                            m_DomainName, key, m_APIImpl->m_NetICacheClient));

    return net_file->Exists();
}

void SNetStorageByKeyAPIImpl::Remove(const string& key, TNetStorageFlags flags)
{
    CRef<SNetStorageObjectAPIImpl, CNetComponentCounterLocker<
            SNetStorageObjectAPIImpl> >
                    net_file(new SNetStorageObjectAPIImpl(m_APIImpl,
                            m_APIImpl->GetDefaultFlags(flags),
                            m_DomainName, key, m_APIImpl->m_NetICacheClient));

    net_file->Remove();
}

CNetStorage g_CreateNetStorage(CNetICacheClient::TInstance icache_client,
        TNetStorageFlags default_flags)
{
    if (icache_client != NULL)
        return new SNetStorageAPIImpl(icache_client, default_flags);
    else {
        CNetICacheClient new_ic_client(CNetICacheClient::eAppRegistry);

        return new SNetStorageAPIImpl(new_ic_client, default_flags);
    }
}

CNetStorage g_CreateNetStorage(TNetStorageFlags default_flags)
{
    return new SNetStorageAPIImpl(
            CNetICacheClient(CNetICacheClient::eAppRegistry), default_flags);
}

CNetStorageByKey g_CreateNetStorageByKey(
        CNetICacheClient::TInstance icache_client,
        const string& domain_name, TNetStorageFlags default_flags)
{
    if (icache_client != NULL)
        return new SNetStorageByKeyAPIImpl(icache_client,
                domain_name, default_flags);
    else {
        CNetICacheClient new_ic_client(CNetICacheClient::eAppRegistry);

        return new SNetStorageByKeyAPIImpl(new_ic_client,
                domain_name, default_flags);
    }
}

CNetStorageByKey g_CreateNetStorageByKey(const string& domain_name,
        TNetStorageFlags default_flags)
{
    return new SNetStorageByKeyAPIImpl(
            CNetICacheClient(CNetICacheClient::eAppRegistry),
            domain_name, default_flags);
}

CNetStorageObject g_CreateNetStorageObject(
        CNetStorage::TInstance netstorage_api,
        Int8 object_loc,
        TNetStorageFlags flags)
{
    return NULL;
}

END_NCBI_SCOPE
