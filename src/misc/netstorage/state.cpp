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

#include <connect/services/impl/netstorage_impl.hpp>

#include "state.hpp"


BEGIN_NCBI_SCOPE


namespace
{

using namespace NDirectNetStorageImpl;


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


class CLocation : public ILocation
{
public:
    CLocation(TObjLoc& object_loc)
        : m_ObjectLoc(object_loc)
    {}

    virtual bool Init() { return true; }
    virtual void SetLocator() = 0;

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

    void SetLocator();

    IState* StartRead(void*, size_t, size_t*, ERW_Result*);
    IState* StartWrite(const void*, size_t, size_t*, ERW_Result*);
    Uint8 GetSizeImpl();
    CNetStorageObjectInfo GetInfoImpl();
    bool ExistsImpl();
    void RemoveImpl();
    void SetExpirationImpl(const CTimeout&);

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

    bool Init();
    void SetLocator();

    IState* StartRead(void*, size_t, size_t*, ERW_Result*);
    IState* StartWrite(const void*, size_t, size_t*, ERW_Result*);
    Uint8 GetSizeImpl();
    CNetStorageObjectInfo GetInfoImpl();
    bool ExistsImpl();
    void RemoveImpl();
    void SetExpirationImpl(const CTimeout&);

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

    void SetLocator();

    IState* StartRead(void*, size_t, size_t*, ERW_Result*);
    IState* StartWrite(const void*, size_t, size_t*, ERW_Result*);
    Uint8 GetSizeImpl();
    CNetStorageObjectInfo GetInfoImpl();
    bool ExistsImpl();
    void RemoveImpl();
    void SetExpirationImpl(const CTimeout&);

private:
    CRef<SContext> m_Context;
    CROFileTrack m_Read;
    CWOFileTrack m_Write;
};


ERW_Result CROState::WriteImpl(const void*, size_t, size_t*)
{
    NCBI_THROW_FMT(CNetStorageException, eInvalidArg,
            "Invalid file status: cannot write while reading.");
    return eRW_Error; // Not reached
}


ERW_Result CWOState::ReadImpl(void*, size_t, size_t*)
{
    NCBI_THROW_FMT(CNetStorageException, eInvalidArg,
            "Invalid file status: cannot read while writing.");
    return eRW_Error; // Not reached
}


bool CWOState::EofImpl()
{
    NCBI_THROW_FMT(CNetStorageException, eInvalidArg,
            "Invalid file status: cannot check EOF status while writing.");
    return true; // Not reached
}


ERW_Result CRWNotFound::ReadImpl(void*, size_t, size_t*)
{
    NCBI_THROW_FMT(CNetStorageException, eNotExists,
            "Cannot open \"" << m_ObjectLoc.GetLocator() << "\" for reading.");
    return eRW_Error; // Not reached
}


ERW_Result CRWNotFound::WriteImpl(const void*, size_t, size_t*)
{
    NCBI_THROW_FMT(CNetStorageException, eNotExists,
            "Cannot open \"" << m_ObjectLoc.GetLocator() << "\" for writing.");
    return eRW_Error; // Not reached
}


bool CRWNotFound::EofImpl()
{
    return false;
}


ERW_Result CRONetCache::ReadImpl(void* buf, size_t count, size_t* bytes_read)
{
    size_t bytes_read_local = 0;
    ERW_Result rw_res = m_Reader->Read(buf, count, &bytes_read_local);
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


void CNotFound::SetLocator()
{
    NCBI_THROW_FMT(CNetStorageException, eNotExists,
            "Cannot open \"" << m_ObjectLoc.GetLocator() << "\" for writing.");
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
    return 0; // Not reached
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
    NCBI_THROW_FMT(CNetStorageException, eNotExists,
            "NetStorageObject \"" << m_ObjectLoc.GetLocator() <<
            "\" could not be found in any of the designated locations.");
}


void CNotFound::SetExpirationImpl(const CTimeout&)
{
    NCBI_THROW_FMT(CNetStorageException, eNotExists,
            "NetStorageObject \"" << m_ObjectLoc.GetLocator() <<
            "\" could not be found in any of the designated locations.");
}


bool CNetCache::Init()
{
    if (!m_Client) {
        if (m_ObjectLoc.GetLocation() == eNFL_NetCache) {
            m_Client = CNetICacheClient(m_ObjectLoc.GetNCServiceName(),
                    m_ObjectLoc.GetAppDomain(), kEmptyStr);
            m_Client.SetFlags(ICache::fBestReliability);
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


void CNetCache::SetLocator()
{
    CNetService service(m_Client.GetService());
    m_ObjectLoc.SetLocation_NetCache(
            service.GetServiceName(), 0, 0,
#ifdef NCBI_GRID_XSITE_CONN_SUPPORT
            service.IsUsingXSiteProxy() ? true :
#endif
                    false);
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
    SetLocator();
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
    CJsonNode blob_info = CJsonNode::NewObjectNode();

    try {
        CNetServerMultilineCmdOutput output = m_Client.GetBlobInfo(
                m_ObjectLoc.GetICacheKey(), 0, kEmptyStr,
                nc_cache_name = m_ObjectLoc.GetAppDomain());

        string line, key, val;

        while (output.ReadLine(line))
            if (NStr::SplitInTwo(line, ": ", key, val, NStr::fSplit_ByPattern))
                blob_info.SetByKey(key, CJsonNode::GuessType(val));
    }
    catch (CNetCacheException& e) {
        if (e.GetErrCode() == CNetCacheException::eBlobNotFound) {
            NCBI_RETHROW_FMT(e, CNetStorageException, eNotExists, e.GetMsg());
        }
    }

    CJsonNode size_node(blob_info.GetByKeyOrNull("Size"));

    Uint8 blob_size = size_node && size_node.IsInteger() ?
            (Uint8) size_node.AsInteger() : GetSizeImpl();

    return g_CreateNetStorageObjectInfo(m_ObjectLoc.GetLocator(), eNFL_NetCache,
            &m_ObjectLoc, blob_size, blob_info);
}


// Cannot use ExistsImpl() directly from other methods,
// as otherwise it would get into thrown exception instead of those methods
#define NC_EXISTS_IMPL                                                      \
    if (!m_Client.HasBlob(m_ObjectLoc.GetICacheKey(),                       \
                kEmptyStr, nc_cache_name = m_ObjectLoc.GetAppDomain())) {   \
        /* Have to throw to let other locations try */                      \
        NCBI_THROW_FMT(CNetStorageException, eNotExists,                    \
            "NetStorageObject \"" << m_ObjectLoc.GetLocator() <<            \
            "\" does not exist in NetCache.");                              \
    }


bool CNetCache::ExistsImpl()
{
    LOG_POST(Trace << "Checking existence in NetCache " << m_ObjectLoc.GetLocator());
    NC_EXISTS_IMPL;
    return true;
}


void CNetCache::RemoveImpl()
{
    LOG_POST(Trace << "Trying to remove from NetCache " << m_ObjectLoc.GetLocator());

    // NetCache returns OK on removing already-removed/non-existent blobs,
    // so have to check for existence first and throw if not
    NC_EXISTS_IMPL;
    m_Client.RemoveBlob(m_ObjectLoc.GetICacheKey(), 0, kEmptyStr,
            nc_cache_name = m_ObjectLoc.GetAppDomain());
}


struct SIClient : public CNetICacheClient
{
    SIClient(CNetICacheClient::TInstance impl) : CNetICacheClient(impl) {}

    void ProlongBlobLifetime(const string& key, const CTimeout& ttl)
    {
        x_ProlongBlobLifetime(key, ttl.GetAsDouble());
    }
};

void CNetCache::SetExpirationImpl(const CTimeout& ttl)
{
    NC_EXISTS_IMPL;
    SIClient(m_Client).ProlongBlobLifetime(m_ObjectLoc.GetICacheKey(), ttl);
}


void CFileTrack::SetLocator()
{
    m_ObjectLoc.SetLocation_FileTrack(m_Context->filetrack_api.config.site);
}


IState* CFileTrack::StartRead(void* buf, size_t count,
        size_t* bytes_read, ERW_Result* result)
{
    _ASSERT(result);

    CROFileTrack::TRequest request = m_Context->filetrack_api.StartDownload(m_ObjectLoc);

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

    // If exists, delete the file first
    // TODO: Replace this with post upload file replacement
    // in SFileTrackPostRequest::FinishUpload (filetrack_file_id != unique_key)
    // after FileTrack implements that feature (FT-85).
    bool exists = false;
    try { exists = ExistsImpl(); } catch (...) { }
    if (exists) RemoveImpl();

    CWOFileTrack::TRequest request = m_Context->filetrack_api.StartUpload(m_ObjectLoc);
    m_Write.Set(request);
    *result = m_Write.WriteImpl(buf, count, bytes_written);
    SetLocator();
    return &m_Write;
}


Uint8 CFileTrack::GetSizeImpl()
{
    return (Uint8) m_Context->filetrack_api.GetFileInfo(
            m_ObjectLoc).GetInteger("size");
}


CNetStorageObjectInfo CFileTrack::GetInfoImpl()
{
    CJsonNode file_info_node =
            m_Context->filetrack_api.GetFileInfo(m_ObjectLoc);

    Uint8 file_size = 0;

    CJsonNode size_node = file_info_node.GetByKeyOrNull("size");

    if (size_node)
        file_size = (Uint8) size_node.AsInteger();

    return g_CreateNetStorageObjectInfo(m_ObjectLoc.GetLocator(),
            eNFL_FileTrack, &m_ObjectLoc, file_size, file_info_node);
}


// Cannot use ExistsImpl() directly from other methods,
// as otherwise it would get into thrown exception instead of those methods
#define FT_EXISTS_IMPL                                              \
    if (m_Context->filetrack_api.GetFileInfo(m_ObjectLoc).          \
            GetBoolean("deleted")) {                                \
        /* Have to throw to let other locations try */              \
        NCBI_THROW_FMT(CNetStorageException, eNotExists,            \
            "NetStorageObject \"" << m_ObjectLoc.GetLocator() <<    \
            "\" does not exist in FileTrack.");                     \
    }


bool CFileTrack::ExistsImpl()
{
    LOG_POST(Trace << "Checking existence in FileTrack " << m_ObjectLoc.GetLocator());
    FT_EXISTS_IMPL;
    return true;
}


void CFileTrack::RemoveImpl()
{
    LOG_POST(Trace << "Trying to remove from FileTrack " << m_ObjectLoc.GetLocator());
    m_Context->filetrack_api.Remove(m_ObjectLoc);
}


void CFileTrack::SetExpirationImpl(const CTimeout&)
{
    // By default objects in FileTrack do not have expiration,
    // so checking only object existence
    FT_EXISTS_IMPL;
    NCBI_THROW_FMT(CNetStorageException, eNotSupported,
            "SetExpiration() is not supported for FileTrack");
}


class CSelector : public ISelector
{
public:
    CSelector(const TObjLoc&, SContext*);
    CSelector(const TObjLoc&, SContext*, TNetStorageFlags);

    ILocation* First();
    ILocation* Next();
    const TObjLoc& Locator();
    void SetLocator();
    ISelector* Clone(TNetStorageFlags);

private:
    void InitLocations(ENetStorageObjectLocation, TNetStorageFlags);
    CLocation* Top();

    TObjLoc m_ObjectLoc;
    CRef<SContext> m_Context;
    CNotFound m_NotFound;
    CNetCache m_NetCache;
    CFileTrack m_FileTrack;
    stack<CLocation*> m_Locations;
};


CSelector::CSelector(const TObjLoc& loc, SContext* context)
    : m_ObjectLoc(loc),
        m_Context(context),
        m_NotFound(m_ObjectLoc),
        m_NetCache(m_ObjectLoc, m_Context),
        m_FileTrack(m_ObjectLoc, m_Context)
{
    InitLocations(m_ObjectLoc.GetLocation(), m_ObjectLoc.GetStorageAttrFlags());
}

CSelector::CSelector(const TObjLoc& loc, SContext* context, TNetStorageFlags flags)
    : m_ObjectLoc(loc),
        m_Context(context),
        m_NotFound(m_ObjectLoc),
        m_NetCache(m_ObjectLoc, m_Context),
        m_FileTrack(m_ObjectLoc, m_Context)
{
    InitLocations(eNFL_Unknown, flags);
}

void CSelector::InitLocations(ENetStorageObjectLocation location,
        TNetStorageFlags flags)
{
    // The order does matter:
    // First, primary locations
    // Then, secondary locations
    // After, all other locations that have not yet been used
    // And finally, the 'not found' location

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

    // No real locations, only CNotFound
    if (m_Locations.size() == 1) {
        NCBI_THROW_FMT(CNetStorageException, eInvalidArg,
                "No storages available for locator=\"" <<
                m_ObjectLoc.GetLocator() << "\" and flags=" << flags);
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


CLocation* CSelector::Top()
{
    _ASSERT(m_Locations.size());
    CLocation* location = m_Locations.top();
    _ASSERT(location);
    return location->Init() ? location : NULL;
}


const TObjLoc& CSelector::Locator()
{
    return m_ObjectLoc;
}


void CSelector::SetLocator()
{
    if (CLocation* l = Top()) {
        l->SetLocator();
    } else {
        NCBI_THROW_FMT(CNetStorageException, eNotExists,
                "Cannot open \"" << m_ObjectLoc.GetLocator() << "\" for writing.");
    }
}


ISelector* CSelector::Clone(TNetStorageFlags flags)
{
    flags = m_Context->DefaultFlags(flags);
    return new CSelector(TObjLoc(m_Context->compound_id_pool,
                    m_ObjectLoc.GetLocator(), flags), m_Context, flags);
}


}


namespace NDirectNetStorageImpl
{


CNetICacheClient s_GetICClient(const SCombinedNetStorageConfig& c)
{
    if (c.nc_service.empty() || c.app_domain.empty() || c.client_name.empty()) {
        return eVoid;
    }

    return CNetICacheClient(c.nc_service, c.app_domain, c.client_name);
}


SContext::SContext(const SCombinedNetStorageConfig& config, TNetStorageFlags flags)
    : icache_client(s_GetICClient(config)),
      filetrack_api(config.ft),
      default_flags(flags),
      app_domain(config.app_domain)
{
    Init();
}


SContext::SContext(const string& domain, CNetICacheClient client,
        TNetStorageFlags flags, CCompoundIDPool::TInstance id_pool,
        const SFileTrackConfig& ft_config)
    : icache_client(client),
      filetrack_api(ft_config),
      compound_id_pool(id_pool ? CCompoundIDPool(id_pool) : CCompoundIDPool()),
      default_flags(flags),
      app_domain(domain)
{
    Init();
}


void SContext::Init()
{
    const TNetStorageFlags backend_storage =
        (icache_client                     ? fNST_NetCache  : 0) |
        (!filetrack_api.config.key.empty() ? fNST_FileTrack : 0);

    // If there were specific underlying storages requested
    if (TNetStorageLocFlags(default_flags)) {
        // Reduce storages to the ones that are available
        default_flags &= (backend_storage | fNST_AnyAttr);
    } else {
        // Use all available underlying storages
        default_flags |= backend_storage;
    }

    if (TNetStorageLocFlags(default_flags)) {
        if (app_domain.empty() && icache_client) {
            app_domain = icache_client.GetCacheName();
        }
    } else {
        // If no available underlying storages left
        NCBI_THROW_FMT(CNetStorageException, eInvalidArg,
                "No storages available when using backend_storage=\"" <<
                backend_storage << "\" and flags=" << default_flags);
    }

    if (icache_client) {
        icache_client.SetFlags(ICache::fBestReliability);
    }
}


ISelector* SContext::Create(TNetStorageFlags flags)
{
    flags = DefaultFlags(flags);
    return new CSelector(TObjLoc(compound_id_pool,
                    flags, app_domain, GetRandomNumber(),
                    filetrack_api.config.site), this, flags);
}


ISelector* SContext::Create(const string& object_loc)
{
    return new CSelector(TObjLoc(compound_id_pool, object_loc), this);
}


ISelector* SContext::Create(TNetStorageFlags flags,
        const string& service, Int8 id)
{
    flags = DefaultFlags(flags);
    TObjLoc loc(compound_id_pool, flags, app_domain,
            GetRandomNumber(), filetrack_api.config.site);
    loc.SetServiceName(service);
    if (id) loc.SetObjectID(id);
    return new CSelector(loc, this, flags);
}


ISelector* SContext::Create(TNetStorageFlags flags, const string& key)
{
    flags = DefaultFlags(flags);
    return new CSelector(TObjLoc(compound_id_pool,
                    flags, app_domain, key,
                    filetrack_api.config.site), this, flags);
}


}


SCombinedNetStorageConfig::EMode
SCombinedNetStorageConfig::GetMode(const string& value)
{
    if (NStr::CompareNocase(value, "direct") == 0)
        return eServerless;
    else
        return eDefault;
}


void SCombinedNetStorageConfig::ParseArg(const string& name,
        const string& value)
{
    if (name == "mode")
        mode = GetMode(value);
    if (name == "ft_site")
        ft.site = SFileTrackConfig::GetSite(value);
    else if (name == "ft_key")
        ft.key = NStr::URLDecode(value);
    else
        SNetStorage::SConfig::ParseArg(name, value);
}


END_NCBI_SCOPE
