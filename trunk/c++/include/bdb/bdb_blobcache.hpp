#ifndef BDB___BLOBCACHE__HPP
#define BDB___BLOBCACHE__HPP

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
 * Authors:  Anatoliy Kuznetsov
 *
 * File Description: BerkeleyDB based local BLOB cache. 
 *
 */

/// @file bdb_blobcache.hpp
/// IBLOB_Cache interface implemented on top of Berkeley DB


#include <util/cache/blob_cache.hpp>
#include <util/cache/int_cache.hpp>
#include <bdb/bdb_file.hpp>
#include <bdb/bdb_blob.hpp>
#include <bdb/bdb_env.hpp>

BEGIN_NCBI_SCOPE

/** @addtogroup BDB_BLOB_Cache
 *
 * @{
 */

/// BLOB storage file structure

struct NCBI_BDB_EXPORT SBLOB_CacheDB : public CBDB_BLobFile
{
    CBDB_FieldString       key;
    CBDB_FieldInt4         version;

    SBLOB_CacheDB()
    {
        BindKey("key",     &key);
        BindKey("version", &version);
    }
};


/// BLOB attributes file structure

struct NCBI_BDB_EXPORT SBLOB_Cache_AttrDB : public CBDB_File
{
    CBDB_FieldString       key;
    CBDB_FieldInt4         version;
    CBDB_FieldInt4         time_stamp;
    CBDB_FieldInt4         overflow;

    SBLOB_Cache_AttrDB()
    {
        BindKey("key",     &key);
        BindKey("version", &version);

        BindData("time_stamp", &time_stamp);
        BindData("overflow",   &overflow);
    }
};

/// Int cache file structure

struct NCBI_BDB_EXPORT SIntCacheDB : public CBDB_BLobFile
{
    CBDB_FieldInt4     key1;
    CBDB_FieldInt4     key2;
    CBDB_FieldUint4    time_stamp;

    SIntCacheDB()
    {
        BindKey("key1",       &key1);
        BindKey("key2",       &key2);
        BindKey("time_stamp", &time_stamp);
    }
};


/// Int cache implementation
///
/// Class implements IIntCache interface

class NCBI_BDB_EXPORT CBDB_IntCache : public IIntCache
{
public:
    CBDB_IntCache(SIntCacheDB& cache_db);
    virtual ~CBDB_IntCache();

    // IIntCache interface

    virtual void Store(int key1, int key2, const vector<int>& value);
    virtual size_t GetSize(int key1, int key2);
    virtual bool Read(int key1, int key2, vector<int>& value);
    virtual void Remove(int key1, int key2);
    virtual void SetExpirationTime(time_t expiration_timeout);
    virtual void Purge(time_t           time_point,
                       EKeepVersions    keep_last_version = eDropAll) ;

private:
    SIntCacheDB&   m_IntCacheDB;
    time_t         m_ExpirationTime;
};

/// BDB cache implementation.
///
/// Class implements IBLOB_Cache interface using local Berkeley DB
/// database.

class NCBI_BDB_EXPORT CBDB_BLOB_Cache : public IBLOB_Cache
{
public:
    CBDB_BLOB_Cache();
    virtual ~CBDB_BLOB_Cache();

    void Open(const char* cache_path);

    IIntCache* GetIntCache() { return &m_IntCacheInstance; }

    // IBLOB_Cache interface

    virtual void Store(const string& key,
                       int           version,
                       const void*   data,
                       size_t        size,
                       EKeepVersions keep_versions = eDropOlder);

    virtual size_t GetSize(const string& key,
                           int           version);

    virtual bool Read(const string& key, 
                      int           version, 
                      void*         buf, 
                      size_t        buf_size);

    virtual IReader* GetReadStream(const string& key, 
                                   int   version);

    virtual IWriter* GetWriteStream(const string&    key, 
                                    int              version,
                                    EKeepVersions    keep_versions = eDropOlder);

    virtual void Remove(const string& key);

    virtual time_t GetAccessTime(const string& key,
                                 int           version);

    virtual void Purge(time_t           access_time,
                       EKeepVersions keep_last_version = eDropAll);

    virtual void Purge(const string&    key,
                       time_t           access_time,
                       EKeepVersions keep_last_version = eDropAll);
private:
    void x_UpdateAccessTime(const string&    key,
                            int              version);

    void x_DropBLOB(const char*    key,
                    int            version,
                    int            overflow);

private:
    CBDB_BLOB_Cache(const CBDB_BLOB_Cache&);
    CBDB_BLOB_Cache& operator=(const CBDB_BLOB_Cache);
private:
    string                  m_Path;    //!< Path to storage

    CBDB_Env                m_Env;        //!< Common environment for cache DBs
    SBLOB_CacheDB           m_BlobDB;     //!< In database BLOB storage
    SBLOB_Cache_AttrDB      m_AttrDB;     //!< BLOB attributes storage
    
    SIntCacheDB             m_IntCacheDB; //!< Int cache storage
    CBDB_IntCache           m_IntCacheInstance; //!< Interface instance
};


/* @} */


END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.8  2003/10/21 11:56:41  kuznets
 * Fixed bug with non-updated Int cache timestamp.
 *
 * Revision 1.7  2003/10/16 19:27:04  kuznets
 * Added Int cache (AKA id resolution cache)
 *
 * Revision 1.6  2003/10/15 18:12:49  kuznets
 * Implemented new cache architecture based on combination of BDB tables
 * and plain files. Fixes the performance degradation in Berkeley DB
 * when it has to work with a lot of overflow pages.
 *
 * Revision 1.5  2003/10/02 20:14:16  kuznets
 * Minor code cleanup
 *
 * Revision 1.4  2003/10/01 20:49:41  kuznets
 * Changed several function prototypes (were marked as pure virual)
 *
 * Revision 1.3  2003/09/29 15:46:18  kuznets
 * Fixed warning (Sun WorkShop)
 *
 * Revision 1.2  2003/09/26 20:54:37  kuznets
 * Documentaion change
 *
 * Revision 1.1  2003/09/24 14:29:42  kuznets
 * Initial revision
 *
 *
 * ===========================================================================
 */

#endif
