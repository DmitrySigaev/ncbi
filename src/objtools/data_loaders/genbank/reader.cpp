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
 * ===========================================================================
 *
 *  Author:  Eugene Vasilchenko
 *
 *  File Description: Base data reader interface
 *
 */

#include <ncbi_pch.hpp>
#include <objtools/data_loaders/genbank/reader.hpp>
#include <objtools/data_loaders/genbank/dispatcher.hpp>
#include <objtools/data_loaders/genbank/request_result.hpp>
#include <objtools/data_loaders/genbank/processors.hpp>
#include <objtools/data_loaders/genbank/cache_manager.hpp>

#include <objmgr/objmgr_exception.hpp>
#include <algorithm>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

CReader::CReader(void)
    : m_Dispatcher(0), m_NumFreeConnections(0, 1000)
{
    m_NumConnections.Set(0);
    m_NextConnection.Set(0);
}


CReader::~CReader(void)
{
}


void CReader::SetInitialConnections(int max)
{
    if ( SetMaximumConnections(max) <= 0 ) {
        NCBI_THROW(CLoaderException, eConnectionFailed,
                   "Maximum connection is zero");
    }
    for ( int attempt = 1; ; ++attempt ) {
        try {
            CConn conn(this);
            x_ConnectAtSlot(conn);
            conn.Release();
            return;
        }
        catch ( CException& exc ) {
            LOG_POST(Warning <<
                     "CReader: cannot open initial connection: "<<exc.what());
            if ( attempt >= GetRetryCount() ) {
                // this is the last attempt to establish connection
                SetMaximumConnections(0);
                throw;
            }
        }
    }
}


int CReader::SetMaximumConnections(int max)
{
    int limit = GetMaximumConnectionsLimit();
    if ( max > limit ) {
        max = limit;
    }
    if ( max < 0 ) {
        max = 0;
    }
    int error_count = 0;
    while( GetMaximumConnections() < max ) {
        try {
            x_AddConnection();
            error_count = 0;
        }
        catch ( exception& exc ) {
            LOG_POST(Warning <<
                     "CReader: cannot add connection: "<<exc.what());
            if ( ++error_count >= GetRetryCount() ) {
                if ( max > 0 && GetMaximumConnections() == 0 ) {
                    throw;
                }
            }
        }
    }
    while( GetMaximumConnections() > max ) {
        x_RemoveConnection();
    }
    return GetMaximumConnections();
}


int CReader::GetMaximumConnections(void) const
{
    return m_NumConnections.Get();
}


int CReader::GetMaximumConnectionsLimit(void) const
{
    return 1;
}


void CReader::x_AddConnection(void)
{
    CMutexGuard guard(m_ConnectionsMutex);
    TConn conn = m_NextConnection.Add(1);
    x_AddConnectionSlot(conn);
    x_ReleaseConnection(conn, true);
    _VERIFY(m_NumConnections.Add(1) > 0);
}


void CReader::x_RemoveConnection(void)
{
    TConn conn = x_AllocConnection(true);
    CMutexGuard guard(m_ConnectionsMutex);
    _VERIFY(m_NumConnections.Add(-1) != TNCBIAtomicValue(-1));
    x_RemoveConnectionSlot(conn);
}


CReader::TConn CReader::x_AllocConnection(bool oldest)
{
    if ( GetMaximumConnections() <= 0 ) {
        NCBI_THROW(CLoaderException, eNoConnection, "no connection");
    }
    m_NumFreeConnections.Wait();
    CMutexGuard guard(m_ConnectionsMutex);
    TConn conn;
    if ( oldest ) {
        conn =  m_FreeConnections.back();
        m_FreeConnections.pop_back();
    }
    else {
        conn =  m_FreeConnections.front();
        m_FreeConnections.pop_front();
    }
    _ASSERT(find(m_FreeConnections.begin(), m_FreeConnections.end(), conn) ==
            m_FreeConnections.end());
    return conn;
}


void CReader::x_ReleaseConnection(TConn conn, bool oldest)
{
    CMutexGuard guard(m_ConnectionsMutex);
    _ASSERT(find(m_FreeConnections.begin(), m_FreeConnections.end(), conn) ==
            m_FreeConnections.end());
    if ( oldest ) {
        m_FreeConnections.push_back(conn);
    }
    else {
        m_FreeConnections.push_front(conn);
    }
    m_NumFreeConnections.Post(1);
}


void CReader::x_AbortConnection(TConn conn)
{
    CMutexGuard guard(m_ConnectionsMutex);
    try {
        x_DisconnectAtSlot(conn);
    }
    catch ( exception& exc ) {
        ERR_POST("CReader: cannot reuse connection: "<<exc.what());
        // cannot reuse connection number, allocate new one
        try {
            x_RemoveConnectionSlot(conn);
        }
        catch ( ... ) {
            // ignore
        }
        _VERIFY(m_NumConnections.Add(-1) != TNCBIAtomicValue(-1));
        x_AddConnection();
        return;
    }
    x_ReleaseConnection(conn, true);
}


void CReader::x_DisconnectAtSlot(TConn conn)
{
    LOG_POST(Warning << "CReader:"
             " GenBank connection failed: reconnecting...");
    x_RemoveConnectionSlot(conn);
    x_AddConnectionSlot(conn);
}


int CReader::GetConst(const string& ) const
{
    return 0;
}


int CReader::GetRetryCount(void) const
{
    return 3;
}


bool CReader::MayBeSkippedOnErrors(void) const
{
    return false;
}


bool CReader::LoadSeq_idGi(CReaderRequestResult& result,
                           const CSeq_id_Handle& seq_id)
{
    CLoadLockSeq_ids ids(result, seq_id);
    if ( ids->IsLoadedGi() || ids.IsLoaded() ) {
        return true;
    }
    return LoadSeq_idSeq_ids(result, seq_id);
}


bool CReader::LoadBlobs(CReaderRequestResult& result,
                        const string& seq_id,
                        TContentsMask mask)
{
    CLoadLockSeq_ids ids(result, seq_id);
    if ( !ids.IsLoaded() ) {
        if ( !LoadStringSeq_ids(result, seq_id) && !ids.IsLoaded() ) {
            return false;
        }
        if ( !ids.IsLoaded() ) {
            return true;
        }
    }
    if ( ids->size() == 1 ) {
        m_Dispatcher->LoadBlobs(result, *ids->begin(), mask);
    }
    return true;
}


bool CReader::LoadBlobs(CReaderRequestResult& result,
                        const CSeq_id_Handle& seq_id,
                        TContentsMask mask)
{
    CLoadLockBlob_ids ids(result, seq_id);
    if ( !ids.IsLoaded() ) {
        if ( !LoadSeq_idBlob_ids(result, seq_id) && !ids.IsLoaded() ) {
            return false;
        }
        if ( !ids.IsLoaded() ) {
            return true;
        }
    }
    m_Dispatcher->LoadBlobs(result, ids, mask);
    return true;
}


bool CReader::LoadBlobs(CReaderRequestResult& result,
                        CLoadLockBlob_ids blobs,
                        TContentsMask mask)
{
    int loaded_count = 0;
    ITERATE ( CLoadInfoBlob_ids, it, *blobs ) {
        const CBlob_Info& info = it->second;
        if ( (info.GetContentsMask() & mask) != 0 ) {
            CLoadLockBlob blob(result, *it->first);
            if ( blob.IsLoaded() ) {
                continue;
            }
            if ( LoadBlob(result, *it->first) ) {
                ++loaded_count;
            }
        }
    }
    return loaded_count > 0;
}


bool CReader::LoadChunk(CReaderRequestResult& /*result*/,
                        const TBlobId& /*blob_id*/,
                        TChunkId /*chunk_id*/)
{
    return false;
}


bool CReader::LoadChunks(CReaderRequestResult& result,
                         const TBlobId& blob_id,
                         const TChunkIds& chunk_ids)
{
    bool ret = false;
    ITERATE(TChunkIds, id, chunk_ids) {
        ret |= LoadChunk(result, blob_id, *id);
    }
    return ret;
}


bool CReader::LoadBlobSet(CReaderRequestResult& result,
                          const TSeqIds& seq_ids)
{
    bool ret = false;
    ITERATE(TSeqIds, id, seq_ids) {
        ret |= LoadBlobs(result, *id, fBlobHasCore);
    }
    return ret;
}


void CReader::SetAndSaveNoBlob(CReaderRequestResult& result,
                               const TBlobId& blob_id,
                               TChunkId chunk_id)
{
    CLoadLockBlob blob(result, blob_id);
    SetAndSaveNoBlob(result, blob_id, chunk_id, blob);
}


void CReader::SetAndSaveNoBlob(CReaderRequestResult& result,
                               const TBlobId& blob_id,
                               TChunkId chunk_id,
                               CLoadLockBlob& blob)
{
    blob.SetBlobState(CBioseq_Handle::fState_no_data);
    CWriter* writer = m_Dispatcher->GetWriter(result, CWriter::eBlobWriter);
    if ( writer ) {
        const CProcessor_St_SE* prc =
            dynamic_cast<const CProcessor_St_SE*>
            (&m_Dispatcher->GetProcessor(CProcessor::eType_St_Seq_entry));
        if ( prc ) {
            prc->SaveNoBlob(result, blob_id, chunk_id, blob, writer);
        }
    }
    CProcessor::SetLoaded(result, blob_id, chunk_id, blob);
}


void CReader::SetAndSaveStringSeq_ids(CReaderRequestResult& result,
                                      const string& seq_id) const
{
    CLoadLockSeq_ids seq_ids(result, seq_id);
    SetAndSaveStringSeq_ids(result, seq_id, seq_ids);
}


void CReader::SetAndSaveStringGi(CReaderRequestResult& result,
                                 const string& seq_id,
                                 int gi) const
{
    CLoadLockSeq_ids seq_ids(result, seq_id);
    SetAndSaveStringGi(result, seq_id, seq_ids, gi);
}


void CReader::SetAndSaveSeq_idSeq_ids(CReaderRequestResult& result,
                                      const CSeq_id_Handle& seq_id) const
{
    CLoadLockSeq_ids seq_ids(result, seq_id);
    SetAndSaveSeq_idSeq_ids(result, seq_id, seq_ids);
}


void CReader::SetAndSaveSeq_idGi(CReaderRequestResult& result,
                                 const CSeq_id_Handle& seq_id,
                                 int gi) const
{
    CLoadLockSeq_ids seq_ids(result, seq_id);
    SetAndSaveSeq_idGi(result, seq_id, seq_ids, gi);
}


void CReader::SetAndSaveSeq_idBlob_ids(CReaderRequestResult& result,
                                       const CSeq_id_Handle& seq_id) const
{
    CLoadLockBlob_ids blob_ids(result, seq_id);
    SetAndSaveSeq_idBlob_ids(result, seq_id, blob_ids);
}


void CReader::SetAndSaveBlobVersion(CReaderRequestResult& result,
                                    const TBlobId& blob_id,
                                    TBlobVersion version) const
{
    CLoadLockBlob blob(result, blob_id);
    SetAndSaveBlobVersion(result, blob_id, blob, version);
}


void CReader::SetAndSaveStringSeq_ids(CReaderRequestResult& result,
                                      const string& seq_id,
                                      CLoadLockSeq_ids& seq_ids) const
{
    if ( seq_ids.IsLoaded() ) {
        return;
    }
    seq_ids.SetLoaded();
    CWriter *writer = m_Dispatcher->GetWriter(result, CWriter::eIdWriter);
    if( writer ) {
        writer->SaveStringSeq_ids(result, seq_id);
    }
}


void CReader::SetAndSaveStringGi(CReaderRequestResult& result,
                                 const string& seq_id,
                                 CLoadLockSeq_ids& seq_ids,
                                 int gi) const
{
    if ( seq_ids->IsLoadedGi() ) {
        return;
    }
    seq_ids->SetLoadedGi(gi);
    CWriter *writer = m_Dispatcher->GetWriter(result, CWriter::eIdWriter);
    if( writer ) {
        writer->SaveStringGi(result, seq_id);
    }
}


void CReader::SetAndSaveSeq_idSeq_ids(CReaderRequestResult& result,
                                      const CSeq_id_Handle& seq_id,
                                      CLoadLockSeq_ids& seq_ids) const
{
    if ( seq_ids.IsLoaded() ) {
        return;
    }
    if ( seq_ids->empty() ) {
        seq_ids->SetState(seq_ids->GetState() |
                          CBioseq_Handle::fState_no_data);
    }
    seq_ids.SetLoaded();
    if (seq_ids->GetState() & CBioseq_Handle::fState_no_data) {
        SetAndSaveSeq_idBlob_ids(result, seq_id);
    }
    CWriter *writer = m_Dispatcher->GetWriter(result, CWriter::eIdWriter);
    if( writer ) {
        writer->SaveSeq_idSeq_ids(result, seq_id);
    }
}


void CReader::SetAndSaveSeq_idGi(CReaderRequestResult& result,
                                 const CSeq_id_Handle& seq_id,
                                 CLoadLockSeq_ids& seq_ids,
                                 int gi) const
{
    if ( seq_ids->IsLoadedGi() ) {
        return;
    }
    seq_ids->SetLoadedGi(gi);
    CWriter *writer = m_Dispatcher->GetWriter(result, CWriter::eIdWriter);
    if( writer ) {
        writer->SaveSeq_idGi(result, seq_id);
    }
}


void CReader::SetAndSaveSeq_idBlob_ids(CReaderRequestResult& result,
                                       const CSeq_id_Handle& seq_id,
                                       CLoadLockBlob_ids& blob_ids) const
{
    if ( blob_ids.IsLoaded() ) {
        return;
    }
    if ( blob_ids->empty() ) {
        blob_ids->SetState(blob_ids->GetState() |
                           CBioseq_Handle::fState_no_data);
    }
    blob_ids.SetLoaded();
    CWriter *writer = m_Dispatcher->GetWriter(result, CWriter::eIdWriter);
    if( writer ) {
        writer->SaveSeq_idBlob_ids(result, seq_id);
    }
}


void CReader::SetAndSaveBlobVersion(CReaderRequestResult& result,
                                    const TBlobId& blob_id,
                                    CLoadLockBlob& blob,
                                    TBlobVersion version) const
{
    if ( blob.IsSetBlobVersion() && blob.GetBlobVersion() == version ) {
        return;
    }
    blob.SetBlobVersion(version);
    CWriter *writer = m_Dispatcher->GetWriter(result, CWriter::eIdWriter);
    if( writer ) {
        writer->SaveBlobVersion(result, blob_id, version);
    }
}


int CReader::ReadInt(CNcbiIstream& stream)
{
    int value;
    stream.read(reinterpret_cast<char*>(&value), sizeof(value));
    if ( stream.gcount() != sizeof(value) ) {
        NCBI_THROW(CLoaderException, eLoaderFailed,
                   "cannot read value");
    }
    return value;
}


void CReader::InitializeCache(CReaderCacheManager& /*cache_manager*/,
                              const TPluginManagerParamTree* /*params*/)
{
}


void CReader::ResetCache(void)
{
}


CReaderCacheManager::CReaderCacheManager(void)
{
}


CReaderCacheManager::~CReaderCacheManager(void)
{
}


CReaderCacheManager::SReaderCacheInfo::SReaderCacheInfo(ICache& cache, ECacheType cache_type)
    : m_Cache(&cache),
      m_Type(cache_type)
{
}

CReaderCacheManager::SReaderCacheInfo::~SReaderCacheInfo(void)
{
}

END_SCOPE(objects)
END_NCBI_SCOPE
