#ifndef NETCACHE__NC_STORAGE_BLOB__HPP
#define NETCACHE__NC_STORAGE_BLOB__HPP
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
 * Authors:  Pavel Ivanov
 */

#include <corelib/ncbitime.hpp>
#include <corelib/ncbimtx.hpp>
#include <corelib/obj_pool.hpp>

#include "nc_db_info.hpp"
#include "nc_utils.hpp"


BEGIN_NCBI_SCOPE


class CNCBlobStorage;
class CNCBlobAccessor;


///
class CNCBlobVerManager : public INCBlockedOpListener
{
public:
    ///
    static CNCBlobVerManager* Get(Uint2         slot,
                                  const string& key,
                                  SNCCacheData* cache_data,
                                  bool          for_new_version);
    ///
    void Release(void);

    ///
    ENCBlockingOpResult  GetCurVersion(CRef<SNCBlobVerData>* ver_data,
                                       INCBlockedOpListener* listener);
    ///
    CRef<SNCBlobVerData> CreateNewVersion(void);

    ///
    void FinalizeWriting(SNCBlobVerData* ver_data);
    ///
    void DeleteVersion(const SNCBlobVerData* ver_data);

public:
    ///
    void ReleaseVerData(const SNCBlobVerData* ver_data);

private:
    CNCBlobVerManager(const CNCBlobVerManager&);
    CNCBlobVerManager& operator= (const CNCBlobVerManager&);

    ///
    static CNCBlobVerManager* sx_LockCacheData(SNCCacheData* cache_data);
    ///
    static void sx_UnlockCacheData(SNCCacheData*      cache_data,
                                   CNCBlobVerManager* new_mgr);
    ///
    CNCBlobVerManager(Uint2 slot, const string& key, SNCCacheData* cache_data);
    virtual ~CNCBlobVerManager(void);
    ///
    virtual void OnBlockedOpFinish(void);

    ///
    unsigned int x_IncRef(void);
    ///
    unsigned int x_DecRef(void);
    ///
    unsigned int x_GetRef(void);
    ///
    void x_SetFlag(unsigned int flag, bool value);
    ///
    bool x_IsFlagSet(unsigned int flag);

    ///
    void x_ReadCurVersion(void);
    ///
    void x_DeleteBlobKey(void);
    ///
    void x_RestoreBlobKey(void);


    ///
    enum {
        kRefCntMask     = 0x0000FFFF,
        kFlagsMask      = 0xFFFF0000,

        fCleaningMgr    = 0x00010000,
        fDeletingKey    = 0x00040000,
        fNeedRestoreKey = 0x00080000
    };


    ///
    unsigned int volatile   m_State;
    Uint2                   m_Slot;
    ///
    SNCCacheData*           m_CacheData;
    ///
    string                  m_Key;
    ///
    CRef<SNCBlobVerData>    m_CurVersion;
    ///
    CNCLongOpTrigger        m_CurVerTrigger;
};


/// Type of access to NetCache blob
enum ENCAccessType {
    eNCRead,        ///< Read meta information only
    eNCReadData,    ///< Read blob data
    eNCCreate,      ///< Create blob or re-write its contents
    eNCCopyCreate,
    eNCDelete,      ///< Delete the blob
    eNCGCDelete
};


/// Object holding lock on NetCache blob.
/// Object takes care of blob creation when necessary. When it is destroyed
/// lock for the blob is released. Lock can also be released by explicit call
/// of ReleaseLock() method.
class CNCBlobAccessor
{
public:
    ///
    ENCBlockingOpResult ObtainMetaInfo(INCBlockedOpListener* listener);
    /// Check if blob exists.
    /// Method can be called only after lock is acquired.
    bool IsBlobExists(void) const;
    /// Check if password provided for accessing the blob was correct
    bool IsAuthorized(void) const;
    TNCBlobId GetBlobId(void) const;
    /// Get key of the blob.
    /// Method can be called only after lock is acquired.
    const string& GetBlobKey       (void) const;
    Uint2         GetBlobSlot      (void) const;
    /// Get size of the blob.
    /// Method can be called only after lock is acquired.
    /// If blob doesn't exist then value is undefined.
    Uint8         GetBlobSize      (void) const;
    Uint8         GetSizeRead      (void) const;
    /// Check if blob is already expired but not yet deleted by GC.
    /// Method can be called only after lock is acquired.
    /// If blob doesn't exist then value is undefined.
    bool          IsBlobExpired    (void) const;
    bool          IsVerExpired     (void) const;
    /// Get type of access this holder was created for
    ENCAccessType GetAccessType    (void) const;

    /// Initially set current position in the blob to start reading from
    void SetPosition(Uint8 pos);
    ENCBlockingOpResult ObtainFirstData(INCBlockedOpListener* listener);
    size_t ReadData(void* buffer, size_t buf_size);
    int GetBlobTTL(void) const;
    /// Set blob's timeout after last access before it will be deleted.
    /// Method can be called only after lock is acquired, if blob exists and
    /// lock is acquired for eNCDelete or eNCCreate access.
    void SetBlobTTL(int ttl);
    int GetVersionTTL(void) const;
    void SetVersionTTL(int ttl);
    int GetBlobVersion(void) const;
    void SetBlobVersion(int ver);
    Uint8 GetBlobCreateTime(void) const;
    void SetBlobCreateTime(Uint8 create_time);
    int GetBlobDeadTime(void) const;
    int GetVerDeadTime(void) const;
    void SetBlobDeadTime(int dead_time);
    void SetVerDeadTime(int dead_time);
    Uint8 GetCreateServer(void) const;
    TNCBlobId GetCreateId(void) const;
    void SetCreateServer(Uint8 create_server, TNCBlobId create_id, Uint2 slot);
    void SetBlobSlot(Uint2 slot);
    const string& GetPassword(void) const;
    void SetPassword(CTempString password);
    bool ReplaceBlobInfo(const SNCBlobVerData& new_info);
    void WriteData(const void* data, size_t size);
    void Finalize(void);
    /// Delete the blob.
    /// Method can be called only after lock is acquired and
    /// lock is acquired for eNCDelete or eNCCreate access. If blob doesn't exist
    /// or was already deleted by call to this method then method is no-op.
    void DeleteBlob(void);

    /// Release blob lock.
    /// No other method can be called after call to this one.
    void Release(void);

public:
    // For internal use only

    /// Create holder of blob lock bound to the given NetCache storage.
    /// Lock holder is yet not usable after construction until Prepare()
    /// and InitializeLock() are called.
    CNCBlobAccessor(void);
    /// Add holder to the list with provided head
    void AddToList(CNCBlobAccessor*& list_head);
    /// Remove holder from the list with provided head
    void RemoveFromList(CNCBlobAccessor*& list_head);

    /// Prepare lock for the blob identified by key, subkey and version.
    /// Method only initializes necessary variables, actual acquiring of the
    /// lock happens in InitializeLock() method.
    /// If access is eNCRead and storage's IsChangeTimeOnRead() returns TRUE
    /// then holder will change blob's access time when lock is released. If
    /// access is eNCCreate then blob will be automatically deleted
    /// when lock is released if blob wasn't properly finalized.
    ///
    /// @param key
    ///   Key of the blob
    /// @param access
    ///   Required access to the blob
    void Prepare(const string& key,
                 const string& password,
                 Uint2         slot,
                 ENCAccessType access_type);
    /// Initialize and acquire the lock.
    /// Should be called only after Prepare().
    void Initialize(SNCCacheData* cache_data);
    /// Check if lock was initialized and process of lock acquiring was started
    bool IsInitialized(void) const;
    void Deinitialize(void);

private:
    CNCBlobAccessor(const CNCBlobAccessor&);
    CNCBlobAccessor& operator= (const CNCBlobAccessor&);

    ///
    static bool sx_IsOnlyOneChunk(SNCBlobVerData* ver_data);
    ///
    NCBI_NORETURN void x_DelCorruptedVersion(void);
    ///
    void x_ReadChunkData(TNCChunkId chunk_id, CNCBlobBuffer* buffer);
    ///
    void x_ReadSingleChunk(void);
    ///
    void x_ReadChunkIds(void);
    ///
    void x_ReadNextChunk(void);


    /// Previous holder in double-linked list of holders
    CNCBlobAccessor*        m_PrevAccessor;
    /// Next holder in list of holders
    CNCBlobAccessor*        m_NextAccessor;
    /// Type of access requested for the blob
    ENCAccessType           m_AccessType;
    ///
    string                  m_BlobKey;
    /// Password that was used for access to the blob
    string                  m_Password;
    Uint2                   m_BlobSlot;
    ///
    CNCBlobVerManager*      m_VerManager;
    /// 
    CRef<SNCBlobVerData>    m_CurData;
    /// 
    CRef<SNCBlobVerData>    m_NewData;
    ///
    bool                    m_Initialized;
    ///
    INCBlockedOpListener*   m_InitListener;
    ///
    Uint4                   m_CurChunk;
    /// Current position of reading/writing inside blob's chunk
    size_t                  m_ChunkPos;
    ///
    Uint8                   m_SizeRead;
    ///
    CRef<CNCBlobBuffer>     m_Buffer;
};



inline
CNCBlobAccessor::CNCBlobAccessor(void)
    : m_PrevAccessor(NULL),
      m_NextAccessor(NULL)
{}

inline void
CNCBlobAccessor::AddToList(CNCBlobAccessor*& list_head)
{
    m_NextAccessor = list_head;
    if (list_head)
        list_head->m_PrevAccessor = this;
    list_head = this;
}

inline void
CNCBlobAccessor::RemoveFromList(CNCBlobAccessor*& list_head)
{
    if (m_PrevAccessor) {
        m_PrevAccessor->m_NextAccessor = m_NextAccessor;
    }
    else {
        _ASSERT(list_head == this);
        list_head = m_NextAccessor;
    }
    if (m_NextAccessor) {
        m_NextAccessor->m_PrevAccessor = m_PrevAccessor;
    }
    m_NextAccessor = m_PrevAccessor = NULL;
}

inline bool
CNCBlobAccessor::IsInitialized(void) const
{
    return m_Initialized;
}

inline bool
CNCBlobAccessor::IsBlobExists(void) const
{
    return m_CurData.NotNull();
}

inline bool
CNCBlobAccessor::IsAuthorized(void) const
{
    return !IsBlobExists()  ||  m_CurData->password == m_Password;
}

inline const string&
CNCBlobAccessor::GetBlobKey(void) const
{
    return m_BlobKey;
}

inline Uint2
CNCBlobAccessor::GetBlobSlot(void) const
{
    return m_BlobSlot;
}

inline Uint8
CNCBlobAccessor::GetBlobSize(void) const
{
    return m_NewData.NotNull()? m_NewData->size: m_CurData->size;
}

inline Uint8
CNCBlobAccessor::GetSizeRead(void) const
{
    return m_SizeRead;
}

inline int
CNCBlobAccessor::GetBlobVersion(void) const
{
    return m_CurData->blob_ver;
}

inline int
CNCBlobAccessor::GetBlobTTL(void) const
{
    return m_CurData->ttl;
}

inline bool
CNCBlobAccessor::IsBlobExpired(void) const
{
    return m_CurData->dead_time < int(time(NULL));
}

inline bool
CNCBlobAccessor::IsVerExpired(void) const
{
    return m_CurData->ver_expire <= int(time(NULL));
}

inline void
CNCBlobAccessor::SetBlobCreateTime(Uint8 create_time)
{
    m_NewData->create_time = create_time;
    m_NewData->dead_time = (create_time >> 32) + m_NewData->ttl;
}

inline Uint8
CNCBlobAccessor::GetBlobCreateTime(void) const
{
    return m_NewData.NotNull()? m_NewData->create_time:
           (m_CurData.NotNull()? m_CurData->create_time: 0);
}

inline int
CNCBlobAccessor::GetBlobDeadTime(void) const
{
    return m_CurData->dead_time;
}

inline int
CNCBlobAccessor::GetVerDeadTime(void) const
{
    return m_CurData->ver_expire;
}

inline void
CNCBlobAccessor::SetBlobDeadTime(int dead_time)
{
    if (m_NewData.NotNull()) {
        m_NewData->dead_time = dead_time;
    }
    else {
        m_CurData->dead_time = dead_time;
        m_CurData->need_write = true;
    }
}

inline int
CNCBlobAccessor::GetVersionTTL(void) const
{
    return m_NewData.NotNull()? m_NewData->ver_ttl: m_CurData->ver_ttl;
}

inline void
CNCBlobAccessor::SetVersionTTL(int ttl)
{
    m_NewData->ver_ttl = ttl;
}

inline void
CNCBlobAccessor::SetBlobTTL(int ttl)
{
    m_NewData->ttl = ttl;
}

inline void
CNCBlobAccessor::SetBlobVersion(int ver)
{
    m_NewData->blob_ver = ver;
}

inline void
CNCBlobAccessor::SetVerDeadTime(int dead_time)
{
    if (m_NewData.NotNull()) {
        m_NewData->ver_expire = dead_time;
    }
    else {
        m_CurData->ver_expire = dead_time;
        m_CurData->need_write = true;
    }
}

inline void
CNCBlobAccessor::SetCreateServer(Uint8      create_server,
                                 TNCBlobId  create_id,
                                 Uint2      slot)
{
    m_NewData->create_server = create_server;
    m_NewData->create_id = create_id;
    m_NewData->slot = slot;
}

inline Uint8
CNCBlobAccessor::GetCreateServer(void) const
{
    return m_NewData.NotNull()? m_NewData->create_server:
           (m_CurData.NotNull()? m_CurData->create_server: 0);
}

inline TNCBlobId
CNCBlobAccessor::GetCreateId(void) const
{
    return m_CurData.NotNull()? m_CurData->create_id: 0;
}

inline void
CNCBlobAccessor::SetBlobSlot(Uint2 slot)
{
    m_NewData->slot = slot;
}

inline const string&
CNCBlobAccessor::GetPassword(void) const
{
    return m_CurData->password;
}

inline void
CNCBlobAccessor::SetPassword(CTempString password)
{
    m_NewData->password = password;
}

inline ENCAccessType
CNCBlobAccessor::GetAccessType(void) const
{
    return m_AccessType;
}

inline void
CNCBlobAccessor::SetPosition(Uint8 pos)
{
    _ASSERT(IsBlobExists()  &&  m_AccessType == eNCReadData);
    _ASSERT(!m_Buffer  &&  pos <= m_CurData->size);

    m_CurChunk = Uint4(pos / kNCMaxBlobChunkSize);
    m_ChunkPos = size_t(pos % kNCMaxBlobChunkSize);
}

inline TNCBlobId
CNCBlobAccessor::GetBlobId(void) const
{
    return m_NewData.NotNull()? m_NewData->coords.blob_id: m_CurData->coords.blob_id;
}

END_NCBI_SCOPE

#endif /* NETCACHE__NC_STORAGE_BLOB__HPP */
