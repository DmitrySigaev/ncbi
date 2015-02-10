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
 * Author: Rafael Sadyrov
 *
 */

#include <ncbi_pch.hpp>

#include "state.hpp"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#ifndef NCBI_OS_MSWIN
#  include <unistd.h>
#endif

BEGIN_NCBI_SCOPE

namespace
{

using namespace NImpl;

typedef CNetStorageObjectLoc TObjLoc;

class CROState : public IState
{
public:
    ERW_Result WriteImpl(const void*, size_t, size_t*);
};

class CWOState : public IState
{
public:
    ERW_Result ReadImpl(void*, size_t, size_t*);
    bool EofImpl();
};

class CRWNotFound : public IState
{
public:
    CRWNotFound(TObjLoc& object_loc)
        : m_ObjectLoc(object_loc)
    {}

    ERW_Result ReadImpl(void*, size_t, size_t*);
    ERW_Result WriteImpl(const void*, size_t, size_t*);
    bool EofImpl();

private:
    TObjLoc& m_ObjectLoc;
};

class CRONetCache : public CROState
{
public:
    typedef auto_ptr<IReader> TReaderPtr;

    void Set(TReaderPtr reader, size_t blob_size)
    {
        m_Reader = reader;
        m_BlobSize = blob_size;
        m_BytesRead = 0;
    }

    ERW_Result ReadImpl(void*, size_t, size_t*);
    bool EofImpl();
    void CloseImpl();

private:
    TReaderPtr m_Reader;
    size_t m_BlobSize;
    size_t m_BytesRead;
};

class CWONetCache : public CWOState
{
public:
    typedef auto_ptr<IEmbeddedStreamWriter> TWriterPtr;

    void Set(TWriterPtr writer)
    {
        m_Writer = writer;
    }

    ERW_Result WriteImpl(const void*, size_t, size_t*);
    void CloseImpl();
    void AbortImpl();

private:
    TWriterPtr m_Writer;
};

class CROFileTrack : public CROState
{
public:
    typedef CRef<SFileTrackRequest> TRequest;

    void Set(TRequest request)
    {
        m_Request = request;
    }

    ERW_Result ReadImpl(void*, size_t, size_t*);
    bool EofImpl();
    void CloseImpl();

private:
    TRequest m_Request;
};

class CWOFileTrack : public CWOState
{
public:
    typedef CRef<SFileTrackPostRequest> TRequest;

    void Set(TRequest request)
    {
        m_Request = request;
    }

    ERW_Result WriteImpl(const void*, size_t, size_t*);
    void CloseImpl();

private:
    TRequest m_Request;
};

typedef CNetStorageObjectLoc TObjLoc;

class CLocation : public ILocation
{
public:
    CLocation(TObjLoc& object_loc)
        : m_ObjectLoc(object_loc)
    {}

    virtual bool Init() { return true; }

protected:
    TObjLoc& m_ObjectLoc;
};

class CNotFound : public CLocation
{
public:
    CNotFound(TObjLoc& object_loc)
        : CLocation(object_loc),
          m_RW(object_loc)
    {}

    IState* StartRead(void*, size_t, size_t*, ERW_Result*);
    IState* StartWrite(const void*, size_t, size_t*, ERW_Result*);
    Uint8 GetSizeImpl();
    CNetStorageObjectInfo GetInfoImpl();
    bool ExistsImpl();
    void RemoveImpl();

private:
    CRWNotFound m_RW;
};

class CNetCache : public CLocation
{
public:
    CNetCache(TObjLoc& object_loc, SContext* context)
        : CLocation(object_loc),
          m_Context(context),
          m_Client(eVoid)
    {}

    CNetCache(TObjLoc& object_loc, SContext* context, bool ctx_client)
        : CLocation(object_loc),
          m_Context(context),
          m_Client(ctx_client ? context->icache_client : CNetICacheClient(eVoid))
    {}

    bool Init();
    IState* StartRead(void*, size_t, size_t*, ERW_Result*);
    IState* StartWrite(const void*, size_t, size_t*, ERW_Result*);
    Uint8 GetSizeImpl();
    CNetStorageObjectInfo GetInfoImpl();
    bool ExistsImpl();
    void RemoveImpl();

private:
    CRef<SContext> m_Context;
    CNetICacheClient m_Client;
    CRONetCache m_Read;
    CWONetCache m_Write;
};

class CFileTrack : public CLocation
{
public:
    CFileTrack(TObjLoc& object_loc, SContext* context)
        : CLocation(object_loc),
          m_Context(context)
    {}

    IState* StartRead(void*, size_t, size_t*, ERW_Result*);
    IState* StartWrite(const void*, size_t, size_t*, ERW_Result*);
    Uint8 GetSizeImpl();
    CNetStorageObjectInfo GetInfoImpl();
    bool ExistsImpl();
    void RemoveImpl();

private:
    CRef<SContext> m_Context;
    CROFileTrack m_Read;
    CWOFileTrack m_Write;
};

ERW_Result CROState::WriteImpl(const void*, size_t, size_t*)
{
    NCBI_THROW_FMT(CNetStorageException, eInvalidArg,
            "Invalid file status: cannot write while reading.");
}

ERW_Result CWOState::ReadImpl(void*, size_t, size_t*)
{
    NCBI_THROW_FMT(CNetStorageException, eInvalidArg,
            "Invalid file status: cannot read while writing.");
}

bool CWOState::EofImpl()
{
    NCBI_THROW_FMT(CNetStorageException, eInvalidArg,
            "Invalid file status: cannot check EOF status while writing.");
}

ERW_Result CRWNotFound::ReadImpl(void*, size_t, size_t*)
{
    NCBI_THROW_FMT(CNetStorageException, eNotExists,
            "Cannot open \"" << m_ObjectLoc.GetLocator() << "\" for reading.");
}

ERW_Result CRWNotFound::WriteImpl(const void*, size_t, size_t*)
{
    NCBI_THROW_FMT(CNetStorageException, eNotExists,
            "Cannot open \"" << m_ObjectLoc.GetLocator() << "\" for writing.");
}

bool CRWNotFound::EofImpl()
{
    return false;
}

ERW_Result CRONetCache::ReadImpl(void* buf, size_t count, size_t* bytes_read)
{
    size_t bytes_read_local = 0;
    ERW_Result rw_res = g_ReadFromNetCache(m_Reader.get(),
            reinterpret_cast<char*>(buf), count, &bytes_read_local);
    m_BytesRead += bytes_read_local;
    if (bytes_read != NULL)
        *bytes_read = bytes_read_local;
    return rw_res;
}

bool CRONetCache::EofImpl()
{
    return m_BytesRead >= m_BlobSize;
}

void CRONetCache::CloseImpl()
{
    m_Reader.reset();
}

ERW_Result CWONetCache::WriteImpl(const void* buf, size_t count, size_t* bytes_written)
{
    return m_Writer->Write(buf, count, bytes_written);
}

void CWONetCache::CloseImpl()
{
    m_Writer->Close();
    m_Writer.reset();
}

void CWONetCache::AbortImpl()
{
    m_Writer->Abort();
    m_Writer.reset();
}

ERW_Result CROFileTrack::ReadImpl(void* buf, size_t count, size_t* bytes_read)
{
    return m_Request->Read(buf, count, bytes_read);
}

bool CROFileTrack::EofImpl()
{
    return m_Request->m_HTTPStream.eof();
}

void CROFileTrack::CloseImpl()
{
    m_Request->FinishDownload();
    m_Request.Reset();
}

ERW_Result CWOFileTrack::WriteImpl(const void* buf, size_t count, size_t* bytes_written)
{
    m_Request->Write(buf, count, bytes_written);
    return eRW_Success;
}

void CWOFileTrack::CloseImpl()
{
    m_Request->FinishUpload();
    m_Request.Reset();
}

IState* CNotFound::StartRead(void* buf, size_t count,
        size_t* bytes_read, ERW_Result* result)
{
    _ASSERT(result);
    *result = m_RW.ReadImpl(buf, count, bytes_read);
    return &m_RW;
}

IState* CNotFound::StartWrite(const void* buf, size_t count,
        size_t* bytes_written, ERW_Result* result)
{
    _ASSERT(result);
    *result = m_RW.WriteImpl(buf, count, bytes_written);
    return &m_RW;
}

Uint8 CNotFound::GetSizeImpl()
{
    NCBI_THROW_FMT(CNetStorageException, eNotExists,
            "NetStorageObject \"" << m_ObjectLoc.GetLocator() <<
            "\" could not be found in any of the designated locations.");
}

CNetStorageObjectInfo CNotFound::GetInfoImpl()
{
    return g_CreateNetStorageObjectInfo(m_ObjectLoc.GetLocator(),
            eNFL_NotFound, &m_ObjectLoc, 0, NULL);
}

bool CNotFound::ExistsImpl()
{
    return false;
}

void CNotFound::RemoveImpl()
{
}

bool CNetCache::Init()
{
    if (!m_Client) {
        if (m_ObjectLoc.GetLocation() == eNFL_NetCache) {
            m_Client = CNetICacheClient(m_ObjectLoc.GetNCServiceName(),
                    m_ObjectLoc.GetAppDomain(), kEmptyStr);
#ifdef NCBI_GRID_XSITE_CONN_SUPPORT
            if (m_ObjectLoc.IsXSiteProxyAllowed())
                m_Client.GetService().AllowXSiteConnections();
#endif
        } else if (m_Context->icache_client)
            m_Client = m_Context->icache_client;
        else
            return false;
    }
    return true;
}

IState* CNetCache::StartRead(void* buf, size_t count,
        size_t* bytes_read, ERW_Result* result)
{
    _ASSERT(result);

    size_t blob_size;
    CRONetCache::TReaderPtr reader(m_Client.GetReadStream(
                m_ObjectLoc.GetICacheKey(), 0, kEmptyStr, &blob_size,
                (nc_caching_mode = CNetCacheAPI::eCaching_Disable,
                nc_cache_name = m_ObjectLoc.GetAppDomain())));

    if (!reader.get()) {
        return NULL;
    }

    m_Read.Set(reader, blob_size);
    *result =  m_Read.ReadImpl(buf, count, bytes_read);
    return &m_Read;
}

IState* CNetCache::StartWrite(const void* buf, size_t count,
        size_t* bytes_written, ERW_Result* result)
{
    _ASSERT(result);

    CWONetCache::TWriterPtr writer(m_Client.GetNetCacheWriter(
                m_ObjectLoc.GetICacheKey(), 0, kEmptyStr,
                nc_cache_name = m_ObjectLoc.GetAppDomain()));

    if (!writer.get()) {
        return NULL;
    }

    m_Write.Set(writer);
    *result = m_Write.WriteImpl(buf, count, bytes_written);

    CNetService service(m_Client.GetService());
    m_ObjectLoc.SetLocation_NetCache(
            service.GetServiceName(), 0, 0,
#ifdef NCBI_GRID_XSITE_CONN_SUPPORT
            service.IsUsingXSiteProxy() ? true :
#endif
                    false);

    return &m_Write;
}

Uint8 CNetCache::GetSizeImpl()
{
    return m_Client.GetBlobSize(
            m_ObjectLoc.GetICacheKey(), 0, kEmptyStr,
            nc_cache_name = m_ObjectLoc.GetAppDomain());
}

CNetStorageObjectInfo CNetCache::GetInfoImpl()
{
    CNetServerMultilineCmdOutput output = m_Client.GetBlobInfo(
            m_ObjectLoc.GetICacheKey(), 0, kEmptyStr,
            nc_cache_name = m_ObjectLoc.GetAppDomain());

    CJsonNode blob_info = CJsonNode::NewObjectNode();
    string line, key, val;

    while (output.ReadLine(line))
        if (NStr::SplitInTwo(line, ": ", key, val, NStr::fSplit_ByPattern))
            blob_info.SetByKey(key, CJsonNode::GuessType(val));

    CJsonNode size_node(blob_info.GetByKeyOrNull("Size"));

    Uint8 blob_size = size_node && size_node.IsInteger() ?
            (Uint8) size_node.AsInteger() : GetSizeImpl();

    return g_CreateNetStorageObjectInfo(m_ObjectLoc.GetLocator(), eNFL_NetCache,
            &m_ObjectLoc, blob_size, blob_info);
}

bool CNetCache::ExistsImpl()
{
    LOG_POST(Trace << "Cheching existence in NetCache " << m_ObjectLoc.GetLocator());
    return m_Client.HasBlob(m_ObjectLoc.GetICacheKey(),
            kEmptyStr, nc_cache_name = m_ObjectLoc.GetAppDomain());
}

void CNetCache::RemoveImpl()
{
    LOG_POST(Trace << "Trying to remove from NetCache " << m_ObjectLoc.GetLocator());
    m_Client.RemoveBlob(m_ObjectLoc.GetICacheKey(), 0, kEmptyStr,
            nc_cache_name = m_ObjectLoc.GetAppDomain());
}

IState* CFileTrack::StartRead(void* buf, size_t count,
        size_t* bytes_read, ERW_Result* result)
{
    _ASSERT(result);

    CROFileTrack::TRequest request = m_Context->filetrack_api.StartDownload(&m_ObjectLoc);

    try {
        m_Read.Set(request);
        *result = m_Read.ReadImpl(buf, count, bytes_read);
        return &m_Read;
    }
    catch (CNetStorageException& e) {
        request.Reset();

        if (e.GetErrCode() != CNetStorageException::eNotExists) {
            throw;
        }
    }

    return NULL;
}

IState* CFileTrack::StartWrite(const void* buf, size_t count,
        size_t* bytes_written, ERW_Result* result)
{
    _ASSERT(result);

    CWOFileTrack::TRequest request = m_Context->filetrack_api.StartUpload(&m_ObjectLoc);
    m_Write.Set(request);
    *result = m_Write.WriteImpl(buf, count, bytes_written);
    m_ObjectLoc.SetLocation_FileTrack(TFileTrack_Site::GetDefault().c_str());
    return &m_Write;
}

Uint8 CFileTrack::GetSizeImpl()
{
    return (Uint8) m_Context->filetrack_api.GetFileInfo(
            &m_ObjectLoc).GetInteger("size");
}

CNetStorageObjectInfo CFileTrack::GetInfoImpl()
{
    CJsonNode file_info_node =
            m_Context->filetrack_api.GetFileInfo(&m_ObjectLoc);

    Uint8 file_size = 0;

    CJsonNode size_node = file_info_node.GetByKeyOrNull("size");

    if (size_node)
        file_size = (Uint8) size_node.AsInteger();

    return g_CreateNetStorageObjectInfo(m_ObjectLoc.GetLocator(),
            eNFL_FileTrack, &m_ObjectLoc, file_size, file_info_node);
}

bool CFileTrack::ExistsImpl()
{
    LOG_POST(Trace << "Cheching existence in FileTrack " << m_ObjectLoc.GetLocator());
    return !m_Context->filetrack_api.GetFileInfo(&m_ObjectLoc).GetBoolean("deleted");
}

void CFileTrack::RemoveImpl()
{
    LOG_POST(Trace << "Trying to remove from FileTrack " << m_ObjectLoc.GetLocator());
    m_Context->filetrack_api.Remove(&m_ObjectLoc);
}

class CSelector : public ISelector
{
public:
    CSelector(const TObjLoc&, SContext*, TNetStorageFlags);

    ILocation* First();
    ILocation* Next();
    string Locator();

private:
    ILocation* Top();

    TObjLoc m_ObjectLoc;
    CNotFound m_NotFound;
    CNetCache m_NetCache;
    CFileTrack m_FileTrack;
    stack<CLocation*> m_Locations;
};

CSelector::CSelector(const TObjLoc& loc, SContext* context, TNetStorageFlags flags)
    : m_ObjectLoc(loc),
        m_NotFound(m_ObjectLoc),
        m_NetCache(m_ObjectLoc, context),
        m_FileTrack(m_ObjectLoc, context)
{
    _ASSERT(context);

    if (flags) {
        m_ObjectLoc.ResetLocation(TFileTrack_Site::GetDefault().c_str());
        m_ObjectLoc.SetStorageAttrFlags(flags);
    } else {
        flags = m_ObjectLoc.GetStorageAttrFlags();
    }

    // The order does matter:
    // First, primary locations
    // Then, secondary locations
    // After, all other locations that have not yet been used
    // And finally, the 'not found' location

    ENetStorageObjectLocation location = m_ObjectLoc.GetLocation();
    bool primary_nc = location == eNFL_NetCache;
    bool primary_ft = location == eNFL_FileTrack;
    bool secondary_nc = flags & (fNST_NetCache | fNST_Fast);
    bool secondary_ft = flags & (fNST_FileTrack | fNST_Persistent);
    LOG_POST(Trace << "location: " << location << ", flags: " << flags);

    m_Locations.push(&m_NotFound);

    if (!primary_nc && !secondary_nc && (flags & fNST_Movable)) {
        LOG_POST(Trace << "NetCache (movable)");
        m_Locations.push(&m_NetCache);
    }

    if (!primary_ft && !secondary_ft && (flags & fNST_Movable)) {
        LOG_POST(Trace << "FileTrack (movable)");
        m_Locations.push(&m_FileTrack);
    }

    if (!primary_nc && secondary_nc) {
        LOG_POST(Trace << "NetCache (flag)");
        m_Locations.push(&m_NetCache);
    }

    if (!primary_ft && secondary_ft) {
        LOG_POST(Trace << "FileTrack (flag)");
        m_Locations.push(&m_FileTrack);
    }

    if (primary_nc) {
        LOG_POST(Trace << "NetCache (location)");
        m_Locations.push(&m_NetCache);
    }
    
    if (primary_ft) {
        LOG_POST(Trace << "FileTrack (location)");
        m_Locations.push(&m_FileTrack);
    }
}

ILocation* CSelector::First()
{
    return m_Locations.size() ? Top() : NULL;
}

ILocation* CSelector::Next()
{
    if (m_Locations.size() > 1) {
        m_Locations.pop();
        return Top();
    }
   
    return NULL;
}

ILocation* CSelector::Top()
{
    _ASSERT(m_Locations.size());
    CLocation* location = m_Locations.top();
    _ASSERT(location);
    return location->Init() ? location : NULL;
}

string CSelector::Locator()
{
    return m_ObjectLoc.GetLocation() != eNFL_Unknown ?
        m_ObjectLoc.GetLocator() : kEmptyStr;
}

// TODO:
// Reconsider, default_flags should not be always used,
// otherwise location is not used at all
inline TNetStorageFlags PurifyFlags(SContext* context,
        TNetStorageFlags flags)
{
    _ASSERT(context);
    flags &= context->valid_flags_mask;
    return flags ? flags : context->default_flags;
}

Uint8 GetRandomNumber(SContext* context)
{
    _ASSERT(context);
#ifndef NCBI_OS_MSWIN
    Uint8 random_number = 0;

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

    return random_number >>= 8;
#else
    return context->filetrack_api.GetRandom();
#endif // NCBI_OS_MSWIN
}

}

namespace NImpl
{

SContext::SContext(const string& domain, CNetICacheClient client, TNetStorageFlags flags)
    : icache_client(client),
      default_flags(flags),
      valid_flags_mask(~0U),
      app_domain(domain)
{
    string backend_storage(TNetStorageAPI_BackendStorage::GetDefault());

    if (strstr(backend_storage.c_str(), "netcache") == NULL)
        valid_flags_mask &= ~fNST_NetCache;
    if (strstr(backend_storage.c_str(), "filetrack") == NULL)
        valid_flags_mask &= ~fNST_FileTrack;

    default_flags &= valid_flags_mask;

    if (default_flags == 0)
        default_flags = valid_flags_mask & fNST_AnyLoc;

    if (app_domain.empty() && icache_client) {
        app_domain = icache_client.GetCacheName();
    }
}

ISelector::Ptr ISelector::Create(SContext* context, TNetStorageFlags flags)
{
    flags = PurifyFlags(context, flags);
    return Ptr(new CSelector(TObjLoc(context->compound_id_pool,
                    flags, context->app_domain, GetRandomNumber(context),
                    TFileTrack_Site::GetDefault().c_str()), context, flags));
}

ISelector::Ptr ISelector::CreateFromLoc(SContext* context, const string& object_loc, TNetStorageFlags flags)
{
    flags = PurifyFlags(context, flags);
    return Ptr(new CSelector(TObjLoc(context->compound_id_pool,
                    object_loc, flags), context, flags));
}

ISelector::Ptr ISelector::CreateFromKey(SContext* context, const string& key, TNetStorageFlags flags)
{
    flags = PurifyFlags(context, flags);
    return Ptr(new CSelector(TObjLoc(context->compound_id_pool,
                    flags, context->app_domain, key,
                    TFileTrack_Site::GetDefault().c_str()), context, flags));
}

ISelector::Ptr ISelector::Create(SContext* context, TNetStorageFlags flags,
        const string& service, Int8 id)
{
    flags = PurifyFlags(context, flags);
    TObjLoc loc(context->compound_id_pool,
                    flags, context->app_domain,
                    GetRandomNumber(context),
                    TFileTrack_Site::GetDefault().c_str());
    loc.SetServiceName(service);
    loc.SetObjectID(id);
    return Ptr(new CSelector(loc, context, flags));
}

ISelector::Ptr ISelector::Create(SContext* context, TNetStorageFlags flags,
        const string& service)
{
    flags = PurifyFlags(context, flags);
    TObjLoc loc(context->compound_id_pool,
                    flags, context->app_domain,
                    GetRandomNumber(context),
                    TFileTrack_Site::GetDefault().c_str());
    loc.SetServiceName(service);
    return Ptr(new CSelector(loc, context, flags));
}

}

NCBI_PARAM_DEF(string, netstorage_api, backend_storage, "netcache, filetrack");

END_NCBI_SCOPE
