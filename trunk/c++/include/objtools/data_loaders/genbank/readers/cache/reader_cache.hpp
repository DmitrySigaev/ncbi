#ifndef READER_CACHE__HPP_INCLUDED
#define READER_CACHE__HPP_INCLUDED

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
*  Author:  Eugene Vasilchenko, Anatoliy Kuznetsov
*
*  File Description: Cached extension of data reader from ID1
*
*/

#include <objtools/data_loaders/genbank/reader.hpp>

#include <vector>

BEGIN_NCBI_SCOPE

class IBLOB_Cache;
class IIntCache;
class ICache;
class IReader;
class IWriter;
class CByteSource;

BEGIN_SCOPE(objects)

class CSeq_id;
class CID2_Reply_Data;
class CLoadLockSeq_ids;

/// structure for common cache reader&writer implementation
struct SCacheInfo
{
    typedef vector<int> TIdCacheData;

    static const int    IDS_MAGIC;
    static const size_t IDS_HSIZE;
    static const size_t IDS_SIZE;

    //////////////////////////////////////////////////////////////////
    // Keys manipulation methods:

    /// Return BLOB cache key string based on Sat() and SatKey()
    static string GetBlobKey(const CBlob_id& blob_id);

    /// BLOB cache subkeys:
    static const char* GetBlobSubkey(void);
    static const char* GetSeqEntrySubkey(void);
    static const char* GetSeqEntryWithSNPSubkey(void);
    static const char* GetSNPTableSubkey(void);
    static const char* GetSkeletonSubkey(void);
    static const char* GetSplitInfoSubkey(void);
    static string GetChunkSubkey(int chunk_id);

    /// Return Id cache key string based on CSeq_id of gi
    static string GetIdKey(const CSeq_id& id);
    static string GetIdKey(const CSeq_id_Handle& id);
    static string GetIdKey(int gi);

    /// Id cache subkeys:
    // Seq-id/gi -> blob_id (4*N ints)
    static const char* GetBlob_idsSubkey(void);
    // Seq-id -> gi (1 int)
    static const char* GetGiSubkey(void);      
    // Seq-id -> list of Seq-id, binary ASN.1
    static const char* GetSeq_idsSubkey(void);
    // blob_id -> blob version (1 int)
    static const char* GetBlobVersionSubkey(void);
};


class NCBI_XREADER_CACHE_EXPORT CCacheReader : public CReader,
                                               public SCacheInfo
{
public:
    enum EOwnership {
        fOwnNone      = 0,
        fOwnIdCache   = 1 << 1,    // own the underlying id ICache
        fOwnBlobCache = 1 << 2,    // own the underlying blob ICache
        fOwnAll       = fOwnIdCache | fOwnBlobCache
    };
    typedef int TOwnership;     // bitwise OR of EOwnership

    CCacheReader(ICache* blob_cache = 0,
                 ICache* id_cache = 0,
                 TOwnership own = fOwnNone);
    ~CCacheReader();


    //////////////////////////////////////////////////////////////////
    // Setup methods:

    void SetBlobCache(ICache* blob_cache);
    void SetIdCache(ICache* id_cache);

    //////////////////////////////////////////////////////////////////
    // Overloaded loading methods:
    bool LoadStringSeq_ids(CReaderRequestResult& result,
                           const string& seq_id);
    bool LoadSeq_idSeq_ids(CReaderRequestResult& result,
                           const CSeq_id_Handle& seq_id);
    bool LoadSeq_idGi(CReaderRequestResult& result,
                      const CSeq_id_Handle& seq_id);
    bool LoadSeq_idBlob_ids(CReaderRequestResult& result,
                            const CSeq_id_Handle& seq_id);
    bool LoadBlobVersion(CReaderRequestResult& result,
                         const TBlobId& blob_id);

    bool LoadBlob(CReaderRequestResult& result,
                  const TBlobId& blob_id);
    bool LoadChunk(CReaderRequestResult& result,
                   const TBlobId& blob_id, TChunkId chunk_id);

    bool ReadSeq_ids(const string& key, CLoadLockSeq_ids& ids);

    int GetRetryCount(void) const;
    bool MayBeSkippedOnErrors(void) const;
    int GetMaximumConnectionsLimit(void) const;

protected:
    void x_Connect(TConn conn);
    void x_Disconnect(TConn conn);

    bool x_LoadIdCache(const string& key,
                       const string& subkey,
                       TIdCacheData& data);

private:
    ICache* m_BlobCache;
    ICache* m_IdCache;
    TOwnership m_Own;

private:
    // to prevent copying
    CCacheReader(const CCacheReader& );
    CCacheReader& operator=(const CCacheReader&);
};


END_SCOPE(objects)
END_NCBI_SCOPE

#endif // READER_CACHE__HPP_INCLUDED
