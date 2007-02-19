#ifndef BDB___BDB_DICT_STORE__HPP
#define BDB___BDB_DICT_STORE__HPP

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
 * Authors:  Mike DiCuccio
 *
 * File Description:
 *
 */

#include <bdb/bdb_env.hpp>
#include <bdb/bdb_split_blob.hpp>


BEGIN_NCBI_SCOPE


/////////////////////////////////////////////////////////////////////////////
///
/// Templatized candidate for dictionaries
/// This template defines the basic interface that must be supported by all
/// dictionaries.  Dictionaries do not need to inherit from this, so long as
/// this API is supported.
///

template <typename Key>
class CBDB_BlobDictionary
{
    Uint4 GetKey(const Key& key)
    {
        NCBI_THROW(CException, eUnknown,
                   "Key type not implemented");
    }

    Uint4 PutKey(const Key& key)
    {
        NCBI_THROW(CException, eUnknown,
                   "Key type not implemented");
    }
};


////////////////////////////////////////////////////////////////////////////
///
/// Pass-through for Uint4 key types
///

template<>
class CBDB_BlobDictionary<Uint4>
{
public:
    /// @name Required CBDB_BlobDictionary<> interface
    /// @{

    Uint4 GetKey(const Uint4& key) { return key; }
    Uint4 PutKey(const Uint4& key) { return key; }

    /// @}

    /// no-ops for compatibility
    void SetEnv(CBDB_Env& env) {}
    void Open(const char*, CBDB_RawFile::EOpenMode) {}
    bool IsOpen() const { return true; }
};


template<>
class CBDB_BlobDictionary<Int4>
{
public:
    /// @name Required CBDB_BlobDictionary<> interface
    /// @{

    Uint4 GetKey(const Int4& key) { return key; }
    Uint4 PutKey(const Int4& key) { return key; }

    /// @}

    /// no-ops for compatibility
    void SetEnv(CBDB_Env& env) {}
    void Open(const char*, CBDB_RawFile::EOpenMode) {}
    bool IsOpen() const { return true; }
};


////////////////////////////////////////////////////////////////////////////
///
/// Concrete dictionary for simple string
///

template<>
class CBDB_BlobDictionary<string> : public CBDB_File
{
public:
    CBDB_BlobDictionary<string>();

    /// @name Required CBDB_BlobDictionary<> interface
    /// @{

    Uint4  GetKey(const string& key);
    Uint4  PutKey(const string& key);

    /// @}

    /// retrieve the current key
    string GetCurrentKey() const;
    Uint4 GetCurrentUid() const;

    /// read a particular key's value
    EBDB_ErrCode Read (const string& key, Uint4* val);

    /// read the current key's value
    EBDB_ErrCode Read (Uint4* val);

    /// write a key/value pair to the store
    EBDB_ErrCode Write(const string& key, Uint4 val);

private:
    CBDB_FieldString m_Key;
    CBDB_FieldUint4  m_Uid;

    Uint4 m_MaxUid;
};


/////////////////////////////////////////////////////////////////////////////


template <class BV>
class CBDB_PersistentSplitStore
    : public CBDB_BlobSplitStore<BV, CBDB_BlobDeMuxPersistent>
{
public:
    typedef CBDB_BlobSplitStore<BV, CBDB_BlobDeMuxPersistent> TParent;
    CBDB_PersistentSplitStore(const string& demux_path)
        : TParent(new CBDB_BlobDeMuxPersistent(demux_path))
    {
    }
};


/////////////////////////////////////////////////////////////////////////////


template <typename Key,
          typename Dictionary = CBDB_BlobDictionary<Key>,
          typename BvStore = CBDB_PersistentSplitStore< bm::bvector<> > >
class CBDB_BlobDictStore
{
public:
    typedef Key        TKey;
    typedef Uint4      TKeyId;
    typedef Dictionary TDictionary;
    typedef BvStore    TBvStore;

    CBDB_BlobDictStore(const string& demux_path = kEmptyStr);
    CBDB_BlobDictStore(Dictionary& dict,
                       BvStore&    store,
                       EOwnership  own = eNoOwnership);
    ~CBDB_BlobDictStore();

    void SetEnv(CBDB_Env& env);
    void Open(const string& fname, CBDB_RawFile::EOpenMode mode);

    EBDB_ErrCode Read    (const Key& key, CBDB_RawFile::TBuffer& data);
    EBDB_ErrCode ReadById(TKeyId     key, CBDB_RawFile::TBuffer& data);
    EBDB_ErrCode Write(const Key& key, const CBDB_RawFile::TBuffer& data);
    EBDB_ErrCode Write(const Key& key, const void* data, size_t size);
    EBDB_ErrCode UpdateInsert(Uint4 uid,
                              const void* data,
                              size_t size);

    Dictionary& GetDictionary() { return *m_Dict; }
    BvStore&    GetBvStore()    { return *m_Store; }

protected:
    Dictionary* m_Dict;
    BvStore*    m_Store;

private:
    auto_ptr<Dictionary> m_DictOwned;
    auto_ptr<BvStore>    m_StoreOwned;
};


/////////////////////////////////////////////////////////////////////////////


template <typename Key, typename Dictionary, typename BvStore>
inline CBDB_BlobDictStore<Key, Dictionary, BvStore>
::CBDB_BlobDictStore(const string& demux_path)
    : m_Dict(NULL)
    , m_Store(NULL)
    , m_DictOwned(new Dictionary)
    , m_StoreOwned(new BvStore(demux_path))
{
    m_Dict = m_DictOwned.get();
    m_Store = m_StoreOwned.get();
}


template <typename Key, typename Dictionary, typename BvStore>
inline CBDB_BlobDictStore<Key, Dictionary, BvStore>
::CBDB_BlobDictStore(Dictionary& generator,
                     BvStore&    store,
                     EOwnership  own)
    : m_Dict(NULL)
    , m_Store(NULL)
{
    if (own == eTakeOwnership) {
        m_DictOwned.reset(&generator);
        m_StoreOwned.reset(&store);
    }
    m_Dict = &generator;
    m_Store = &store;
}


template <typename Key, typename Dictionary, typename BvStore>
inline
CBDB_BlobDictStore<Key, Dictionary, BvStore>::~CBDB_BlobDictStore()
{
}

template <typename Key, typename Dictionary, typename BvStore>
inline
void CBDB_BlobDictStore<Key, Dictionary, BvStore>::SetEnv(CBDB_Env& env)
{
    m_Dict->SetEnv(env);
    m_Store->SetEnv(env);
}


template <typename Key, typename Dictionary, typename BvStore>
inline
void CBDB_BlobDictStore<Key, Dictionary, BvStore>::Open(const string& fname,
                                                        CBDB_RawFile::EOpenMode mode)
{
    /// open our dictionary
    if ( !m_Dict->IsOpen() ) {
        m_Dict->Open((fname + ".dict").c_str(), mode);
    }
    m_Store->Open(fname, mode);
}


template <typename Key, typename Dictionary, typename BvStore>
inline EBDB_ErrCode
CBDB_BlobDictStore<Key, Dictionary, BvStore>::Read(const Key& key,
                                                   CBDB_RawFile::TBuffer& data)
{
    TKeyId key_id = m_Dict->GetKey(key);
    if ( !key_id ) {
        return eBDB_NotFound;
    }
    return ReadById(key_id, data);
}


template <typename Key, typename Dictionary, typename BvStore>
inline EBDB_ErrCode
CBDB_BlobDictStore<Key, Dictionary, BvStore>::ReadById(TKeyId key_id,
                                                       CBDB_RawFile::TBuffer& data)
{
    return m_Store->ReadRealloc(key_id, data);
}


template <typename Key, typename Dictionary, typename BvStore>
inline EBDB_ErrCode
CBDB_BlobDictStore<Key, Dictionary, BvStore>::Write(const Key& key,
                                                    const CBDB_RawFile::TBuffer& data)
{
    return Write(key, &data[0], data.size());
}


template <typename Key, typename Dictionary, typename BvStore>
inline EBDB_ErrCode
CBDB_BlobDictStore<Key, Dictionary, BvStore>::Write(const Key& key,
                                                    const void* data,
                                                    size_t size)
{
    TKeyId key_id = m_Dict->GetKey(key);
    if ( !key_id ) {
        key_id = m_Dict->PutKey(key);
        if ( !key_id ) {
            NCBI_THROW(CException, eUnknown,
                       "Failed to insert key value");
        }
    }
    return UpdateInsert(key_id, data, size);
}


template <typename Key, typename Dictionary, typename BvStore>
inline EBDB_ErrCode
CBDB_BlobDictStore<Key, Dictionary, BvStore>::UpdateInsert(Uint4 uid,
                                                           const void* data,
                                                           size_t size)
{
    return m_Store->UpdateInsert(uid, data, size);
}


END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.1  2006/12/14 13:11:31  dicuccio
 * Added components for creating split blob stores with arbitrarily defined key
 * spaces (including multi-component keys)
 *
 * Revision 1.4  2006/12/05 19:33:43  dicuccio
 * Added bv split store shim to call Save() in the dtor
 *
 * Revision 1.3  2006/12/04 12:54:17  dicuccio
 * Added template parameter for underlying BV store
 *
 * Revision 1.2  2006/12/01 13:15:11  dicuccio
 * Various clean-ups.  Permit a dictionary to be passed in from the outside,
 * pre-opened.  Use standard buffer types
 *
 * Revision 1.1  2006/11/21 13:52:39  dicuccio
 * Large-scale reorganization of indexing.  Support multiple index types, split
 * dictionary-based index stores.
 *
 * ===========================================================================
 */

#endif  // BDB___BDB_DICT_STORE__HPP
