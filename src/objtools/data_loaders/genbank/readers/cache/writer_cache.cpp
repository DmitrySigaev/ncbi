/*  $Id$
 * ===========================================================================
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
 *  Author:  Eugene Vasilchenko, Anatoliy Kuznetsov
 *
 *  File Description: Cached writer for GenBank data loader
 *
 */
#include <ncbi_pch.hpp>
#include <objtools/data_loaders/genbank/readers/cache/writer_cache.hpp>
#include <objtools/data_loaders/genbank/readers/cache/writer_cache_entry.hpp>
#include <objtools/data_loaders/genbank/readers/cache/reader_cache_params.h>
#include <objtools/data_loaders/genbank/readers/readers.hpp> // for entry point
#include <objtools/data_loaders/genbank/request_result.hpp>

#include <util/cache/icache.hpp>

#include <util/rwstream.hpp>

#include <serial/objostrasnb.hpp>

#include <objmgr/objmgr_exception.hpp>

#include <corelib/plugin_manager_store.hpp>
#include <serial/serial.hpp>
#include <memory>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

CCacheWriter::CCacheWriter(ICache* blob_cache,
                           ICache* id_cache,
                           TOwnership own)
    : m_BlobCache(blob_cache), m_IdCache(id_cache), m_Own(own)
{
}


CCacheWriter::~CCacheWriter(void)
{
    if ( m_Own & fOwnIdCache ) {
        delete m_IdCache;
    }
    if ( m_Own & fOwnBlobCache ) {
        delete m_BlobCache;
    }
}


void CCacheWriter::SetBlobCache(ICache* blob_cache)
{
    m_BlobCache = blob_cache;
}


void CCacheWriter::SetIdCache(ICache* id_cache)
{
    m_IdCache = id_cache;
}


void CCacheWriter::SaveStringSeq_ids(CReaderRequestResult& result,
                                     const string& seq_id)
{
    if ( !m_IdCache) {
        return;
    }

    CLoadLockSeq_ids ids(result, seq_id);
    WriteSeq_ids(seq_id, ids);
}


void CCacheWriter::SaveStringGi(CReaderRequestResult& result,
                                const string& seq_id)
{
    if( !m_IdCache) {
        return;
    }

    CLoadLockSeq_ids ids(result, seq_id);
    if ( ids->IsLoadedGi() ) {
        int gi = ids->GetGi();
        m_IdCache->Store(seq_id,
                         0,
                         GetGiSubkey(),
                         &gi,
                         sizeof(gi));
    }
}


void CCacheWriter::SaveSeq_idSeq_ids(CReaderRequestResult& result,
                                     const CSeq_id_Handle& seq_id)
{
    if( !m_IdCache) {
        return;
    }

    CLoadLockSeq_ids ids(result, seq_id);
    WriteSeq_ids(GetIdKey(seq_id), ids);
}


void CCacheWriter::SaveSeq_idGi(CReaderRequestResult& result,
                                const CSeq_id_Handle& seq_id)
{
    if( !m_IdCache) {
        return;
    }

    CLoadLockSeq_ids ids(result, seq_id);
    if ( ids->IsLoadedGi() ) {
        int gi = ids->GetGi();
        m_IdCache->Store(GetIdKey(seq_id),
                         0,
                         GetGiSubkey(),
                         &gi,
                         sizeof(gi));
    }
}


void CCacheWriter::WriteSeq_ids(const string& key,
                                const CLoadLockSeq_ids& ids)
{
    if( !m_IdCache) {
        return;
    }

    if ( !ids.IsLoaded() ) {
        return;
    }

    try {
        auto_ptr<IWriter> writer
            (m_IdCache->GetWriteStream(key, 0, GetSeq_idsSubkey()));
        if ( !writer.get() ) {
            return;
        }

        {{
            CWStream w_stream(writer.get());
            CObjectOStreamAsnBinary obj_stream(w_stream);
            ITERATE ( CLoadInfoSeq_ids, it, *ids ) {
                obj_stream << *it->GetSeqId();
            }
        }}

        writer.reset();
    }
    catch ( ... ) {
        // In case of an error we need to remove incomplete data
        // from the cache.
        try {
            m_BlobCache->Remove(key);
        }
        catch ( exception& /*exc*/ ) {
            // ignored
        }
        // ignore cache write error - it doesn't affect application
    }
}


void CCacheWriter::SaveSeq_idBlob_ids(CReaderRequestResult& result,
                                      const CSeq_id_Handle& seq_id)
{
    if ( !m_IdCache) {
        return;
    }

    CLoadLockBlob_ids ids(result, seq_id);
    if ( !ids.IsLoaded() ) {
        return;
    }

    TIdCacheData data;
    data.push_back(IDS_MAGIC);
    data.push_back(ids->GetState());
    ITERATE ( CLoadInfoBlob_ids, it, *ids ) {
        const CBlob_id& id = it->first;
        const CBlob_Info& info = it->second;
        data.push_back(id.GetSat());
        data.push_back(id.GetSubSat());
        data.push_back(id.GetSatKey());
        data.push_back(info.GetContentsMask());
    }
    _ASSERT(data.size() % IDS_SIZE == IDS_HSIZE && data.front() == IDS_MAGIC);
    m_IdCache->Store(GetIdKey(seq_id),
                     0,
                     GetBlob_idsSubkey(),
                     &data.front(),
                     data.size() * sizeof(int));
}


void CCacheWriter::SaveBlobVersion(CReaderRequestResult& result,
                                   const TBlobId& blob_id,
                                   TBlobVersion version)
{
    if( !m_IdCache ) {
        return;
    }

    m_IdCache->Store(GetBlobKey(blob_id),
                     0,
                     GetBlobVersionSubkey(),
                     &version,
                     sizeof(version));
}


class CCacheBlobStream : public CWriter::CBlobStream
{
public:
    typedef int TVersion;

    CCacheBlobStream(IWriter* writer,
                     ICache* cache, const string& key,
                     TVersion version = 0, const string& subkey = kEmptyStr)
        : m_Writer(writer), m_Stream(new CWStream(writer)),
          m_Cache(cache), m_Key(key), m_Version(version), m_Subkey(subkey)
        {
        }
    ~CCacheBlobStream(void)
        {
            if ( m_Stream.get() ) {
                Abort();
            }
        }
    
    CNcbiOstream& operator*(void)
        {
            _ASSERT(m_Stream.get());
            return *m_Stream;
        }

    void Close(void)
        {
            *m_Stream << flush;
            if ( !*m_Stream ) {
                Abort();
                NCBI_THROW(CLoaderException, eLoaderFailed,
                           "cannot write into cache");
            }
            m_Stream.reset();
            m_Writer.reset();
        }

    void Abort(void)
        {
            m_Stream.reset();
            m_Writer.reset();
            Remove();
        }

    void Remove(void)
        {
            if ( m_Version == 0 && m_Subkey.empty() ) {
                m_Cache->Remove(m_Key);
            }
            else {
                m_Cache->Remove(m_Key, m_Version, m_Subkey);
            }
        }
    
private:
    auto_ptr<IWriter>   m_Writer;
    auto_ptr<CWStream>  m_Stream;
    ICache*             m_Cache;
    string              m_Key;
    TVersion            m_Version;
    string              m_Subkey;
};


CRef<CWriter::CBlobStream>
CCacheWriter::OpenBlobStream(CReaderRequestResult& result,
                             const TBlobId& blob_id,
                             const CProcessor& processor)
{
    if( !m_BlobCache ) {
        return null;
    }

    string key = GetBlobKey(blob_id);
    CLoadLockBlob blob(result, blob_id);
    TBlobVersion version = blob.GetBlobVersion();

    auto_ptr<IWriter> writer
        (m_BlobCache->GetWriteStream(key, version, GetBlobSubkey()));
    if ( !writer.get() ) {
        return null;
    }

    CRef<CBlobStream> stream(new CCacheBlobStream(writer.release(),
                                                  m_BlobCache, key));
    WriteProcessorTag(**stream, processor);
    return stream;
}


CRef<CWriter::CBlobStream>
CCacheWriter::OpenChunkStream(CReaderRequestResult& result,
                              const TBlobId& blob_id,
                              TChunkId chunk_id,
                              const CProcessor& processor)
{
    if( !m_BlobCache ) {
        return null;
    }

    CLoadLockBlob blob(result, blob_id);
    string key = GetBlobKey(blob_id);
    TBlobVersion version = blob.GetBlobVersion();
    string subkey = GetChunkSubkey(chunk_id);

    auto_ptr<IWriter> writer
        (m_BlobCache->GetWriteStream(key, version, subkey));
    if ( !writer.get() ) {
        return null;
    }

    CRef<CBlobStream> stream(new CCacheBlobStream(writer.release(),
                                                  m_BlobCache, key,
                                                  version, subkey));
    WriteProcessorTag(**stream, processor);
    return stream;
}


void CCacheWriter::SaveBlob(CReaderRequestResult& result,
                            const TBlobId& blob_id,
                            const CProcessor& processor,
                            CRef<CByteSource> byte_source)
{
    if( !m_BlobCache ) {
        return;
    }

    _ASSERT(byte_source);

    string key = GetBlobKey(blob_id);
    CLoadLockBlob blob(result, blob_id);
    TBlobVersion version = blob.GetBlobVersion();

    try {
        auto_ptr<IWriter> writer(
            m_BlobCache->GetWriteStream(key, version, GetBlobSubkey()));
        if ( !writer.get() ) {
            return;
        }

        {{
            CWStream stream(writer.get());
            WriteProcessorTag(stream, processor);
            WriteBytes(stream, byte_source);
        }}

        writer.reset();
        
        // everything is fine
        return;
    }
    catch ( ... ) {
        // In case of an error we need to remove incomplete BLOB
        // from the cache.
        try {
            m_BlobCache->Remove(key);
        }
        catch ( exception& /*exc*/ ) {
            // ignored
        }
        // ignore cache write error - it doesn't affect application
    }
    
}


void CCacheWriter::SaveStateAndBlob(CReaderRequestResult& result,
                                    const TBlobId& blob_id,
                                    const CProcessor& processor,
                                    CRef<CByteSource> byte_source)
{
    if( !m_BlobCache ) {
        return;
    }

    string key = GetBlobKey(blob_id);
    CLoadLockBlob blob(result, blob_id);
    TBlobState blob_state = blob.GetBlobState();
    TBlobVersion version = blob.GetBlobVersion();

    _ASSERT((blob_state&CBioseq_Handle::fState_no_data) && !byte_source ||
            !(blob_state&CBioseq_Handle::fState_no_data) && byte_source);

    try {
        auto_ptr<IWriter> writer(
            m_BlobCache->GetWriteStream(key, version, GetBlobSubkey()));
        if ( !writer.get() ) {
            return;
        }

        {{
            CWStream stream(writer.get());
            WriteProcessorTag(stream, processor);
            WriteInt(stream, blob_state);
            if ( byte_source ) {
                WriteBytes(stream, byte_source);
            }
        }}

        writer.reset();
        
        // everything is fine
        return;
    }
    catch ( ... ) {
        // In case of an error we need to remove incomplete BLOB
        // from the cache.
        try {
            m_BlobCache->Remove(key);
        }
        catch ( exception& /*exc*/ ) {
            // ignored
        }
        // ignore cache write error - it doesn't affect application
    }
    
}


void CCacheWriter::SaveSNPBlob(CReaderRequestResult& result,
                               const TBlobId& blob_id,
                               const CConstObjectInfo& root,
                               const TSNP_InfoMap& snps)
{
    if( !m_BlobCache ) 
        return;

    string key = GetBlobKey(blob_id);
    CLoadLockBlob blob(result, blob_id);
    TBlobVersion version = blob.GetBlobVersion();

    try {
        auto_ptr<IWriter> writer
            (m_BlobCache->GetWriteStream(key, version,
                                         GetSeqEntryWithSNPSubkey()));
        if ( !writer.get() ) {
            return;
        }

        {{
            CWStream stream(writer.get());
            CSeq_annot_SNP_Info_Reader::Write(stream, root, snps);
        }}
        writer.reset();
        // everything is fine
        return;
    }
    catch ( ... ) {
        // In case of an error we need to remove incomplete BLOB
        // from the cache.
        try {
            m_BlobCache->Remove(key);
        }
        catch ( exception& /*exc*/ ) {
            // ignored
        }
    }
}


void CCacheWriter::SaveSNPTable(CReaderRequestResult& result,
                                const TBlobId& blob_id,
                                const CSeq_annot_SNP_Info& snp_info)
{
    if( !m_BlobCache ) {
        return;
    }

    cout << "CCacheWriter::SaveSNPTable()" << endl;

    string key = GetBlobKey(blob_id);
    CLoadLockBlob blob(result, blob_id);
    TBlobVersion version = blob.GetBlobVersion();

    try {
        auto_ptr<IWriter> writer
            (m_BlobCache->GetWriteStream(key, version, GetSNPTableSubkey()));
        if ( !writer.get() ) {
            return;
        }
        {{
            CWStream stream(writer.get());
            CSeq_annot_SNP_Info_Reader::Write(stream, snp_info);
        }}
        writer.reset();
    }
    catch ( exception& exc ) {
        ERR_POST("CId1Cache:StoreSNPTable: "
                 "Exception: "<< key << ": " << exc.what());
        try {
            m_BlobCache->Remove(key);
        }
        catch ( exception& /*exc*/ ) {
            // ignored
        }

    }
}

void CCacheWriter::SaveChunk(CReaderRequestResult& result,
                             const TBlobId& blob_id, 
                             TChunkId chunk_id)
{
}


bool CCacheWriter::CanWrite(EType type) const
{
    return type == eIdWriter ? m_IdCache : m_BlobCache;
}


END_SCOPE(objects)


void GenBankWriters_Register_Cache(void)
{
    RegisterEntryPoint<objects::CWriter>(NCBI_EntryPoint_CacheWriter);
}


/// Class factory for Cache writer
///
/// @internal
///
class CCacheWriterCF : 
    public CSimpleClassFactoryImpl<objects::CWriter, objects::CCacheWriter>
{
public:
    typedef 
      CSimpleClassFactoryImpl<objects::CWriter, objects::CCacheWriter> TParent;
public:
    CCacheWriterCF()
        : TParent(NCBI_GBLOADER_WRITER_CACHE_DRIVER_NAME, 0) {}
    ~CCacheWriterCF() {}


    ICache* CreateCache(const TPluginManagerParamTree* params,
                        const string& section) const
        {
            typedef CPluginManager<ICache> TCacheManager;
            typedef CPluginManagerStore::CPMMaker<ICache> TCacheManagerStore;
            CRef<TCacheManager> CacheManager(TCacheManagerStore::Get());
            _ASSERT(CacheManager);
            const TPluginManagerParamTree* cache_params = params ?
                params->FindSubNode(section) : 0;
            return CacheManager->CreateInstanceFromKey
                (cache_params,
                 NCBI_GBLOADER_WRITER_CACHE_PARAM_DRIVER);
        }

    objects::CWriter* 
    CreateInstance(const string& driver  = kEmptyStr,
                   CVersionInfo version =
                   NCBI_INTERFACE_VERSION(objects::CWriter),
                   const TPluginManagerParamTree* params = 0) const
    {
        if ( !driver.empty()  &&  driver != m_DriverName ) 
            return 0;
        
        if (version.Match(NCBI_INTERFACE_VERSION(objects::CWriter)) 
                            != CVersionInfo::eNonCompatible) {
            auto_ptr<ICache> id_cache
                (CreateCache(params,
                             NCBI_GBLOADER_WRITER_CACHE_PARAM_ID_SECTION));
            auto_ptr<ICache> blob_cache
                (CreateCache(params,
                             NCBI_GBLOADER_WRITER_CACHE_PARAM_BLOB_SECTION));
            if ( blob_cache.get()  ||  id_cache.get() ) 
                return new objects::CCacheWriter(blob_cache.release(),
                                                 id_cache.release(),
                                                 objects::CCacheWriter::fOwnAll);
        }
        return 0;
    }
};


void NCBI_EntryPoint_CacheWriter(
     CPluginManager<objects::CWriter>::TDriverInfoList&   info_list,
     CPluginManager<objects::CWriter>::EEntryPointRequest method)
{
    CHostEntryPointImpl<CCacheWriterCF>::NCBI_EntryPointImpl(info_list, method);
}


void NCBI_EntryPoint_xwriter_cache(
     CPluginManager<objects::CWriter>::TDriverInfoList&   info_list,
     CPluginManager<objects::CWriter>::EEntryPointRequest method)
{
    NCBI_EntryPoint_CacheWriter(info_list, method);
}


END_NCBI_SCOPE
