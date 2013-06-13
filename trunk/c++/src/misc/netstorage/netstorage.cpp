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

#define THROW_IO_EXCEPTION(err_code, message, status) \
        NCBI_THROW_FMT(CIOException, err_code, message << IO_StatusStr(status));

struct SNetFileAPIImpl;

struct SNetStorageAPIImpl : public SNetStorageImpl
{
    SNetStorageAPIImpl(CNetICacheClient::TInstance icache_client,
            TNetStorageFlags default_flags);

    virtual CNetFile Create(TNetStorageFlags flags = 0);
    virtual CNetFile Open(const string& file_id, TNetStorageFlags flags = 0);
    virtual string Relocate(const string& file_id, TNetStorageFlags flags);
    virtual bool Exists(const string& file_id);
    virtual void Remove(const string& file_id);

    TNetStorageFlags GetDefaultFlags(TNetStorageFlags flags)
    {
        return flags != 0 ? flags : m_DefaultFlags;
    }

    string MoveFile(SNetFileAPIImpl* orig_file, SNetFileAPIImpl* new_file);

    CNetICacheClient m_NetICacheClient;
    TNetStorageFlags m_DefaultFlags;

    SFileTrackAPI m_FileTrackAPI;
};

SNetStorageAPIImpl::SNetStorageAPIImpl(
        CNetICacheClient::TInstance icache_client,
        TNetStorageFlags default_flags) :
    m_NetICacheClient(icache_client),
    m_DefaultFlags(default_flags)
{
}

enum ENetFileIOStatus {
    eNFS_Closed,
    eNFS_WritingToNetCache,
    eNFS_ReadingFromNetCache,
    eNFS_WritingToFileTrack,
    eNFS_ReadingFromFileTrack,
};

struct SNetFileAPIImpl : public SNetFileImpl
{
    SNetFileAPIImpl(SNetStorageAPIImpl* storage_impl,
            TNetStorageFlags flags,
            Uint8 random_number,
            CNetICacheClient::TInstance icache_client) :
        m_NetStorage(storage_impl),
        m_NetICacheClient(icache_client),
        m_FileID(flags, random_number),
        m_CurrentLocation(eNFL_Unknown),
        m_IOStatus(eNFS_Closed)
    {
        g_SetNetICacheParams(m_FileID, icache_client);
    }

    SNetFileAPIImpl(SNetStorageAPIImpl* storage_impl, const string& file_id) :
        m_NetStorage(storage_impl),
        m_NetICacheClient(eVoid),
        m_FileID(file_id),
        m_CurrentLocation(eNFL_Unknown),
        m_IOStatus(eNFS_Closed)
    {
    }

    SNetFileAPIImpl(SNetStorageAPIImpl* storage_impl,
            TNetStorageFlags flags,
            const string& domain_name,
            const string& unique_key,
            CNetICacheClient::TInstance icache_client) :
        m_NetStorage(storage_impl),
        m_NetICacheClient(icache_client),
        m_FileID(flags, domain_name, unique_key),
        m_CurrentLocation(eNFL_Unknown),
        m_IOStatus(eNFS_Closed)
    {
        g_SetNetICacheParams(m_FileID, icache_client);
    }

    SNetFileAPIImpl(SNetStorageAPIImpl* storage_impl,
            const CNetFileID& file_id) :
        m_NetStorage(storage_impl),
        m_NetICacheClient(eVoid),
        m_FileID(file_id),
        m_CurrentLocation(eNFL_Unknown),
        m_IOStatus(eNFS_Closed)
    {
    }

    virtual string GetID();
    virtual size_t Read(void* buffer, size_t buf_size);
    virtual bool Eof();
    virtual void Write(const void* buffer, size_t buf_size);
    virtual Uint8 GetSize();
    virtual CNetFileInfo GetInfo();
    virtual void Close();

    bool SetNetICacheClient();
    void DemandNetCache();

    const ENetFileLocation* GetPossibleFileLocations(); // For reading.
    void ChooseLocation(); // For writing.

    ERW_Result s_ReadFromNetCache(char* buf, size_t count, size_t* bytes_read);

    bool s_TryReadLocation(ENetFileLocation location, ERW_Result* rw_res,
            char* buf, size_t count, size_t* bytes_read);

    ERW_Result BeginOrContinueReading(
            void* buf, size_t count, size_t* bytes_read = 0);

    ERW_Result Write(const void* buf,
            size_t count, size_t* bytes_written = 0);

    bool x_TryGetFileSizeFromLocation(ENetFileLocation location,
            Uint8* file_size);

    bool x_TryGetInfoFromLocation(ENetFileLocation location,
            CNetFileInfo* file_info);

    bool x_ExistsAtLocation(ENetFileLocation location);

    bool Exists();

    virtual ~SNetFileAPIImpl();

    CRef<SNetStorageAPIImpl,
            CNetComponentCounterLocker<SNetStorageAPIImpl> > m_NetStorage;
    CNetICacheClient m_NetICacheClient;

    CNetFileID m_FileID;
    ENetFileLocation m_CurrentLocation;
    ENetFileIOStatus m_IOStatus;

    auto_ptr<IEmbeddedStreamWriter> m_NetCacheWriter;
    auto_ptr<IReader> m_NetCacheReader;
    size_t m_NetCache_BlobSize;
    size_t m_NetCache_BytesRead;

    CRef<SFileTrackRequest> m_FileTrackRequest;
    CRef<SFileTrackPostRequest> m_FileTrackPostRequest;
};

void g_SetNetICacheParams(CNetFileID& file_id, CNetICacheClient icache_client)
{
    if (!icache_client)
        file_id.ClearNetICacheParams();
    else {
        CNetService service(icache_client.GetService());

        CNetServer icache_server(service.Iterate().GetServer());

        file_id.SetNetICacheParams(
                service.GetServiceName(), icache_client.GetCacheName(),
                icache_server.GetHost(), icache_server.GetPort()
#ifdef NCBI_GRID_XSITE_CONN_SUPPORT
                , service.IsUsingXSiteProxy()
#endif
                );
    }
}

bool SNetFileAPIImpl::SetNetICacheClient()
{
    if (!m_NetICacheClient) {
        if (m_FileID.GetFields() & fNFID_NetICache) {
            m_NetICacheClient = CNetICacheClient(m_FileID.GetNCServiceName(),
                    m_FileID.GetCacheName(), kEmptyStr);
#ifdef NCBI_GRID_XSITE_CONN_SUPPORT
            if (m_FileID.GetFields() & fNFID_AllowXSiteConn)
                m_NetICacheClient.GetService().AllowXSiteConnections();
#endif
        } else if (m_NetStorage->m_NetICacheClient)
            m_NetICacheClient = m_NetStorage->m_NetICacheClient;
        else
            return false;
    }
    return true;
}

void SNetFileAPIImpl::DemandNetCache()
{
    if (!SetNetICacheClient()) {
        NCBI_THROW(CNetStorageException, eInvalidArg,
                "The given storage preferences require a NetCache server.");
    }
}

static const ENetFileLocation s_TryNetCacheThenFileTrack[] =
        {eNFL_NetCache, eNFL_FileTrack, eNFL_NotFound};

static const ENetFileLocation s_NetCache[] =
        {eNFL_NetCache, eNFL_NotFound};

static const ENetFileLocation s_TryFileTrackThenNetCache[] =
        {eNFL_FileTrack, eNFL_NetCache, eNFL_NotFound};

static const ENetFileLocation s_FileTrack[] =
        {eNFL_FileTrack, eNFL_NotFound};

const ENetFileLocation* SNetFileAPIImpl::GetPossibleFileLocations()
{
    TNetStorageFlags flags = m_FileID.GetStorageFlags();

    if (flags == 0)
        // Just guessing.
        return SetNetICacheClient() ? s_NetCache : s_FileTrack;

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

void SNetFileAPIImpl::ChooseLocation()
{
    TNetStorageFlags flags = m_FileID.GetStorageFlags();

    if (flags == 0)
        m_CurrentLocation = SetNetICacheClient() ?
                eNFL_NetCache : eNFL_FileTrack;
    else if (flags & fNST_Persistent)
        m_CurrentLocation = eNFL_FileTrack;
    else {
        DemandNetCache();
        m_CurrentLocation = eNFL_NetCache;
    }
}

ERW_Result SNetFileAPIImpl::s_ReadFromNetCache(char* buf,
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

bool SNetFileAPIImpl::s_TryReadLocation(ENetFileLocation location,
        ERW_Result* rw_res, char* buf, size_t count, size_t* bytes_read)
{
    if (location == eNFL_NetCache) {
        CNetServer ic_server;

        if (m_FileID.GetFields() & fNFID_NetICache)
            ic_server = m_NetICacheClient.GetService().GetServer(
                    m_FileID.GetNetCacheIP(), m_FileID.GetNetCachePort());

        m_NetCacheReader.reset(m_NetICacheClient.GetReadStream(
                m_FileID.GetUniqueKey(), 0, kEmptyStr,
                &m_NetCache_BlobSize,
                (nc_caching_mode = CNetCacheAPI::eCaching_Disable,
                nc_server_to_use = ic_server)));

        if (m_NetCacheReader.get() != NULL) {
            m_NetCache_BytesRead = 0;
            *rw_res = s_ReadFromNetCache(buf, count, bytes_read);
            m_CurrentLocation = eNFL_NetCache;
            m_IOStatus = eNFS_ReadingFromNetCache;
            return true;
        }
    } else { /* location == eNFL_FileTrack */
        m_FileTrackRequest =
                m_NetStorage->m_FileTrackAPI.StartDownload(&m_FileID);

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

ERW_Result SNetFileAPIImpl::BeginOrContinueReading(
        void* buf, size_t count, size_t* bytes_read)
{
    switch (m_IOStatus) {
    case eNFS_Closed:
        switch (m_CurrentLocation) {
        case eNFL_Unknown:
            {
                ERW_Result rw_res = eRW_Success;
                const ENetFileLocation* location = GetPossibleFileLocations();

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
                "Cannot open \"" << m_FileID.GetID() << "\" for reading.");

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

bool SNetFileAPIImpl::Eof()
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

ERW_Result SNetFileAPIImpl::Write(const void* buf, size_t count,
        size_t* bytes_written)
{
    switch (m_IOStatus) {
    case eNFS_Closed:
        if (m_CurrentLocation == eNFL_Unknown)
            ChooseLocation();

        if (m_CurrentLocation == eNFL_NetCache) {
            m_NetCacheWriter.reset(m_NetICacheClient.GetNetCacheWriter(
                    m_FileID.GetUniqueKey(), 0, kEmptyStr,
                            nc_blob_ttl = (m_FileID.GetFields() & fNFID_TTL ?
                                    (unsigned) m_FileID.GetTTL() : 0)));

            m_IOStatus = eNFS_WritingToNetCache;

            return m_NetCacheWriter->Write(buf, count, bytes_written);
        } else {
            m_FileTrackPostRequest =
                    m_NetStorage->m_FileTrackAPI.StartUpload(&m_FileID);

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

bool SNetFileAPIImpl::x_TryGetFileSizeFromLocation(ENetFileLocation location,
        Uint8* file_size)
{
    try {
        if (location == eNFL_NetCache) {
            CNetServer ic_server;

            if (m_FileID.GetFields() & fNFID_NetICache)
                ic_server = m_NetICacheClient.GetService().GetServer(
                        m_FileID.GetNetCacheIP(), m_FileID.GetNetCachePort());

            *file_size = m_NetICacheClient.GetBlobSize(
                    m_FileID.GetUniqueKey(), 0, kEmptyStr,
                            nc_server_to_use = ic_server);

            m_CurrentLocation = eNFL_NetCache;
            return true;
        } else { /* location == eNFL_FileTrack */
            *file_size = (Uint8) m_NetStorage->m_FileTrackAPI.GetFileInfo(
                    &m_FileID).GetInteger("size");

            m_CurrentLocation = eNFL_FileTrack;
            return true;
        }
    }
    catch (CException& e) {
        LOG_POST(Trace << e);
    }

    return false;
}

Uint8 SNetFileAPIImpl::GetSize()
{
    switch (m_CurrentLocation) {
    case eNFL_Unknown:
        {
            Uint8 file_size;
            const ENetFileLocation* location = GetPossibleFileLocations();

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
            "NetFile \"" << m_FileID.GetID() <<
            "\" could not be found in any of the designated locations.");
}

bool SNetFileAPIImpl::x_TryGetInfoFromLocation(ENetFileLocation location,
        CNetFileInfo* file_info)
{
    if (location == eNFL_NetCache) {
        CNetServer ic_server;

        if (m_FileID.GetFields() & fNFID_NetICache)
            ic_server = m_NetICacheClient.GetService().GetServer(
                    m_FileID.GetNetCacheIP(), m_FileID.GetNetCachePort());

        try {
            CNetServerMultilineCmdOutput output = m_NetICacheClient.GetBlobInfo(
                    m_FileID.GetUniqueKey(), 0, kEmptyStr,
                    nc_server_to_use = ic_server);

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
                        m_FileID.GetUniqueKey(), 0, kEmptyStr);

            *file_info = CNetFileInfo(eNFL_NetCache,
                    m_FileID, blob_size, blob_info);

            m_CurrentLocation = eNFL_NetCache;
            return true;
        }
        catch (CException& e) {
            LOG_POST(Trace << e);
        }
    } else { /* location == eNFL_FileTrack */
        try {
            CJsonNode file_info_node =
                    m_NetStorage->m_FileTrackAPI.GetFileInfo(&m_FileID);

            Uint8 file_size = 0;

            CJsonNode size_node = file_info_node.GetByKeyOrNull("size");

            if (size_node)
                file_size = (Uint8) size_node.AsInteger();

            *file_info = CNetFileInfo(eNFL_FileTrack,
                    m_FileID, file_size, file_info_node);

            m_CurrentLocation = eNFL_FileTrack;
            return true;
        }
        catch (CException& e) {
            LOG_POST(Trace << e);
        }
    }

    return false;
}

CNetFileInfo SNetFileAPIImpl::GetInfo()
{
    switch (m_CurrentLocation) {
    case eNFL_Unknown:
        {
            CNetFileInfo file_info;
            const ENetFileLocation* location = GetPossibleFileLocations();

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
            CNetFileInfo file_info;
            if (x_TryGetInfoFromLocation(m_CurrentLocation, &file_info))
                return file_info;
        }
    }

    return CNetFileInfo(eNFL_NotFound, m_FileID, 0, NULL);
}

bool SNetFileAPIImpl::x_ExistsAtLocation(ENetFileLocation location)
{
    if (location == eNFL_NetCache) {
        CNetServer ic_server;

        if (m_FileID.GetFields() & fNFID_NetICache)
            ic_server = m_NetICacheClient.GetService().GetServer(
                    m_FileID.GetNetCacheIP(), m_FileID.GetNetCachePort());

        try {
            if (!m_NetICacheClient.HasBlob(m_FileID.GetUniqueKey(),
                    kEmptyStr, nc_server_to_use = ic_server))
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
                    &m_FileID).GetBoolean("deleted"))
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

bool SNetFileAPIImpl::Exists()
{
    switch (m_CurrentLocation) {
    case eNFL_Unknown:
        {
            const ENetFileLocation* location = GetPossibleFileLocations();

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

void SNetFileAPIImpl::Close()
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

SNetFileAPIImpl::~SNetFileAPIImpl()
{
    try {
        Close();
    }
    NCBI_CATCH_ALL("Error while implicitly closing a NetStorage file.");
}

string SNetFileAPIImpl::GetID()
{
    return m_FileID.GetID();
}

size_t SNetFileAPIImpl::Read(void* buffer, size_t buf_size)
{
    size_t bytes_read;

    BeginOrContinueReading(buffer, buf_size, &bytes_read);

    return bytes_read;
}

void SNetFileAPIImpl::Write(const void* buffer, size_t buf_size)
{
    Write(buffer, buf_size, NULL);
}

CNetFile SNetStorageAPIImpl::Create(TNetStorageFlags flags)
{
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

    CRef<SNetFileAPIImpl, CNetComponentCounterLocker<SNetFileAPIImpl> >
            netfile(new SNetFileAPIImpl(this, GetDefaultFlags(flags),
                    random_number, m_NetICacheClient));

    return netfile.GetPointerOrNull();
}

CNetFile SNetStorageAPIImpl::Open(const string& file_id, TNetStorageFlags flags)
{
    CRef<SNetFileAPIImpl, CNetComponentCounterLocker<SNetFileAPIImpl> >
        netfile(new SNetFileAPIImpl(this, file_id));

    if (flags != 0)
        netfile->m_FileID.SetStorageFlags(flags);

    return netfile.GetPointerOrNull();
}

string SNetStorageAPIImpl::MoveFile(SNetFileAPIImpl* orig_file,
        SNetFileAPIImpl* new_file)
{
    new_file->ChooseLocation();

    if (new_file->m_CurrentLocation == eNFL_NetCache &&
            (new_file->m_FileID.GetFields() & fNFID_NetICache) == 0)
        g_SetNetICacheParams(new_file->m_FileID, m_NetICacheClient);

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

        if (orig_file->m_CurrentLocation == eNFL_NetCache)
            orig_file->m_NetICacheClient.Remove(
                    orig_file->m_FileID.GetUniqueKey(), 0, kEmptyStr);

        // TODO Delete orig_file from FileTrack.
    }

    return new_file->m_FileID.GetID();
}

string SNetStorageAPIImpl::Relocate(const string& file_id,
        TNetStorageFlags flags)
{
    if (flags == 0)
        return file_id;

    CRef<SNetFileAPIImpl, CNetComponentCounterLocker<SNetFileAPIImpl> >
            orig_file(new SNetFileAPIImpl(this, file_id));

    if (orig_file->m_FileID.GetStorageFlags() == flags)
        return file_id;

    CRef<SNetFileAPIImpl, CNetComponentCounterLocker<SNetFileAPIImpl> >
            new_file(new SNetFileAPIImpl(this, orig_file->m_FileID));

    new_file->m_FileID.SetStorageFlags(flags);

    return MoveFile(orig_file, new_file);
}

bool SNetStorageAPIImpl::Exists(const string& file_id)
{
    CRef<SNetFileAPIImpl, CNetComponentCounterLocker<SNetFileAPIImpl> >
            net_file(new SNetFileAPIImpl(this, file_id));

    return net_file->Exists();
}

void SNetStorageAPIImpl::Remove(const string& /*key*/)
{
    NCBI_THROW(CNetStorageException, eInvalidArg, "Method not implemented");
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

    virtual CNetFile Open(const string& unique_key,
            TNetStorageFlags flags = 0);
    virtual string Relocate(const string& unique_key,
            TNetStorageFlags flags, TNetStorageFlags old_flags = 0);
    virtual bool Exists(const string& key, TNetStorageFlags flags = 0);
    virtual void Remove(const string& key, TNetStorageFlags flags = 0);

    CRef<SNetStorageAPIImpl,
        CNetComponentCounterLocker<SNetStorageAPIImpl> > m_APIImpl;

    string m_DomainName;
};

CNetFile SNetStorageByKeyAPIImpl::Open(const string& unique_key,
        TNetStorageFlags flags)
{
    return new SNetFileAPIImpl(m_APIImpl, m_APIImpl->GetDefaultFlags(flags),
            m_DomainName, unique_key, m_APIImpl->m_NetICacheClient);
}

string SNetStorageByKeyAPIImpl::Relocate(const string& unique_key,
        TNetStorageFlags flags, TNetStorageFlags old_flags)
{
    CRef<SNetFileAPIImpl, CNetComponentCounterLocker<SNetFileAPIImpl> >
            orig_file(new SNetFileAPIImpl(m_APIImpl, old_flags,
                    m_DomainName, unique_key, m_APIImpl->m_NetICacheClient));

    CRef<SNetFileAPIImpl, CNetComponentCounterLocker<SNetFileAPIImpl> >
            new_file(new SNetFileAPIImpl(m_APIImpl, old_flags,
                    m_DomainName, unique_key, m_APIImpl->m_NetICacheClient));

    return m_APIImpl->MoveFile(orig_file, new_file);
}

bool SNetStorageByKeyAPIImpl::Exists(const string& key, TNetStorageFlags flags)
{
    CRef<SNetFileAPIImpl, CNetComponentCounterLocker<SNetFileAPIImpl> >
            net_file(new SNetFileAPIImpl(m_APIImpl,
                    m_APIImpl->GetDefaultFlags(flags),
                            m_DomainName, key, m_APIImpl->m_NetICacheClient));

    return net_file->Exists();
}

void SNetStorageByKeyAPIImpl::Remove(const string& key, TNetStorageFlags flags)
{
    NCBI_THROW(CNetStorageException, eInvalidArg, "Method not implemented");
}

CNetStorage g_CreateNetStorage(CNetICacheClient::TInstance icache_client,
        TNetStorageFlags default_flags)
{
    return new SNetStorageAPIImpl(icache_client, default_flags);
}

CNetStorage g_CreateNetStorage(TNetStorageFlags default_flags)
{
    return new SNetStorageAPIImpl(NULL, default_flags);
}

CNetStorageByKey g_CreateNetStorageByKey(
        CNetICacheClient::TInstance icache_client,
        const string& domain_name, TNetStorageFlags default_flags)
{
    return new SNetStorageByKeyAPIImpl(icache_client,
            domain_name, default_flags);
}

CNetStorageByKey g_CreateNetStorageByKey(const string& domain_name,
        TNetStorageFlags default_flags)
{
    return new SNetStorageByKeyAPIImpl(NULL, domain_name, default_flags);
}

END_NCBI_SCOPE
