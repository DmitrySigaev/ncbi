#ifndef DBAPI___BLOBCACHE__HPP
#define DBAPI___BLOBCACHE__HPP

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
 * Author: Anatoliy Kuznetsov
 *
 * File Description:  DBAPI based ICache interface
 *
 */

/// @file bdb_blobcache.hpp
/// ICache interface implementation on top of Berkeley DB

#include <corelib/ncbiexpt.hpp>
#include <util/cache/icache.hpp>
#include <dbapi/dbapi.hpp>

BEGIN_NCBI_SCOPE

/** @addtogroup DbAPI
 *
 * @{
 */

/// Register NCBI_EntryPoint_DBAPI_BlobCache
void DBAPI_Register_Cache(void);

/// DBAPI ICache exception

class CDBAPI_ICacheException : public CException
{
public:
    enum EErrCode {
        eCannotInitCache,
        eConnectionError,
        eInvalidDirectory,
        eStreamClosed,
        eCannotCreateBLOB,
        eCannotReadBLOB,
        eTempFileIOError
    };

    virtual const char* GetErrCodeString(void) const
    {
        switch (GetErrCode())
        {
        case eCannotInitCache:   return "eCannotInitCache";
        case eConnectionError:   return "eConnectionError";
        case eInvalidDirectory:  return "eInvalidDirectory";
        case eStreamClosed:      return "eStreamClosed";
        case eCannotCreateBLOB:  return "eCannotCreateBLOB";
        case eCannotReadBLOB:    return "eCannotReadBLOB";
        case eTempFileIOError:   return "eTempFileIOError";
        default:                 return  CException::GetErrCodeString();
        }
    }

    NCBI_EXCEPTION_DEFAULT(CDBAPI_ICacheException, CException);
};






/// DBAPI cache implementation.
///
/// Class implements ICache (BLOB cache) interface using DBAPI (RDBMS)
/// This implementation stores BLOBs in the RDBMS and intended to play
/// role of an enterprise wide BLOB storage
///
/// (tested with MSSQL)

class NCBI_DBAPI_CACHE_EXPORT CDBAPI_Cache : public ICache
{
public:
    CDBAPI_Cache();
    virtual ~CDBAPI_Cache();

    /// Open cache. Does not take IConnection ownership.
    /// Connection should be already logged into the destination server
    ///
    /// Implementation uses temporary files to keep local BLOBs.
    /// If you temp directory is not specified system default is used
    ///
    /// @param conn
    ///    Established database connection
    /// @param temp_dir
    ///    Directory name to keep temp files
    ///    (auto created if does not exist)
    /// @param temp_prefix
    ///    Temp file prefix
    void Open(IConnection*  conn,
              const string& temp_dir = kEmptyStr,
              const string& temp_prefix = kEmptyStr);

    /// Open cache. Creates it's own connection.
    void Open(const string& driver,
              const string& server,
              const string& database,
              const string& login,
              const string& password,
              const string& temp_dir = kEmptyStr,
              const string& temp_prefix = kEmptyStr);


    IConnection* GetConnection() { return m_Conn; }

    /// @return Size of the intermidiate BLOB memory buffer
    unsigned GetMemBufferSize() const { return m_MemBufferSize; }

    /// Set size of the intermidiate BLOB memory buffer
    void SetMemBufferSize(unsigned int buf_size);


    // ICache interface

    virtual void SetTimeStampPolicy(TTimeStampFlags policy,
                                    unsigned int    timeout,
                                    unsigned int    max_timeout = 0);
    virtual bool IsOpen() const { return m_Conn != 0; }
    virtual TTimeStampFlags GetTimeStampPolicy() const;
    virtual int GetTimeout() const;
    virtual void SetVersionRetention(EKeepVersions policy);
    virtual EKeepVersions GetVersionRetention() const;
    virtual void Store(const string&  key,
                       int            version,
                       const string&  subkey,
                       const void*    data,
                       size_t         size,
                       unsigned int   time_to_live = 0,
                       const string&  owner = kEmptyStr);
    virtual size_t GetSize(const string&  key,
                           int            version,
                           const string&  subkey);

    virtual bool Read(const string& key,
                      int           version,
                      const string& subkey,
                      void*         buf,
                      size_t        buf_size);
    virtual bool HasBlobs(const string&  key,
                          const string&  subkey);

    virtual IReader* GetReadStream(const string&  key,
                                   int            version,
                                   const string&  subkey);
    virtual void GetBlobAccess(const string&     key,
                               int               version,
                               const string&     subkey,
                               SBlobAccessDescr*  blob_descr);
    virtual void GetBlobOwner(const string&  key,
                              int            version,
                              const string&  subkey,
                              string*        owner);

    /// Specifics of this IWriter implementation is that IWriter::Flush here
    /// cannot be called twice, because it finalises transaction
    /// Also you cannot call Write after Flush...
    /// All this is because MSSQL (and Sybase) wants to know exact
    /// BLOB size before writing it to the database
    /// Effectively IWriter::Flush in this case works as "Close"...
    ///
    virtual IWriter* GetWriteStream(const string&   key,
                                    int             version,
                                    const string&   subkey,
                                    unsigned int    time_to_live = 0,
                                    const string&   owner = kEmptyStr);

    virtual void Remove(const string& key);

    virtual void Remove(const string&    key,
                        int              version,
                        const string&    subkey);

    virtual time_t GetAccessTime(const string&  key,
                                 int            version,
                                 const string&  subkey);

    virtual void Purge(time_t           access_timeout,
                       EKeepVersions    keep_last_version = eDropAll);

    virtual void Purge(const string&    key,
                       const string&    subkey,
                       time_t           access_timeout,
                       EKeepVersions    keep_last_version = eDropAll);

    virtual bool SameCacheParams(const TCacheParams* params) const;
    virtual string GetCacheName(void) const
        {
            return m_Conn->GetDatabase();
        }

private:

    /// Update BLOB storage, return TRUE if it was updated,
    /// FALSE if there is no record to update
    /// If data == 0 or size == 0 method adds an empty BLOB
    /// (or updates to make it empty)
    bool x_UpdateBlob(IStatement&    stmt,
                      const string&  key,
                      int            version,
                      const string&  subkey,
                      const void*    data,
                      size_t         size);

    /// Update access time attributes for the BLOB
    static
    void x_UpdateAccessTime(IStatement&    stmt,
                            const string&  key,
                            int            version,
                            const string&  subkey,
                            int            timestamp_flag);

    /// Return FALSE if timestamp cannot be retrived
    bool x_RetrieveTimeStamp(IStatement&    stmt,
                             const string&  key,
                             int            version,
                             const string&  subkey,
                             int&           timestamp);

    bool x_CheckTimestampExpired(int timestamp) const;

    void x_TruncateDB();

    /// Delete all BLOBs without corresponding attribute records
    void x_CleanOrphantBlobs(IStatement&    stmt);

private:
    CDBAPI_Cache(const CDBAPI_Cache&);
    CDBAPI_Cache& operator=(const CDBAPI_Cache&);

public:
    static
    void UpdateAccessTime(IStatement&    stmt,
                          const string&  key,
                          int            version,
                          const string&  subkey,
                          int            timestamp_flag)
    {
        x_UpdateAccessTime(stmt, key, version, subkey, timestamp_flag);
    }

private:
    IConnection*            m_Conn;         ///< db connection
    bool                    m_OwnConnection;///< Connection ownership flag
    TTimeStampFlags         m_TimeStampFlag;///< Time stamp flag
    unsigned int            m_Timeout;      ///< Timeout expiration policy
    unsigned int            m_MaxTimeout;   ///< Maximum timeout
    EKeepVersions           m_VersionFlag;  ///< Version retention policy
    string                  m_TempDir;      ///< Directory for temp files
    string                  m_TempPrefix;   ///< Temp prefix
    unsigned int            m_MemBufferSize;///< Size of temp. buffer for BLOBs
};

/* @} */

extern NCBI_DBAPI_CACHE_EXPORT const string kDBAPI_BlobCacheDriverName;

extern "C"
{

NCBI_DBAPI_CACHE_EXPORT
void NCBI_EntryPoint_DBAPI_BlobCache(
     CPluginManager<ICache>::TDriverInfoList&   info_list,
     CPluginManager<ICache>::EEntryPointRequest method);


NCBI_DBAPI_CACHE_EXPORT
void NCBI_EntryPoint_xcache_dbapi(
     CPluginManager<ICache>::TDriverInfoList&   info_list,
     CPluginManager<ICache>::EEntryPointRequest method);

} // extern C


END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.19  2006/01/11 15:28:03  kuznets
 * dbapi_blob_cache.hpp
 *
 * Revision 1.18  2005/11/28 15:28:42  kuznets
 * +GetBlobOwner()
 *
 * Revision 1.17  2005/08/01 16:50:40  kuznets
 * Added BLOB's owner
 *
 * Revision 1.16  2005/06/30 16:54:32  grichenk
 * Moved cache ownership to GB loader. Improved cache sharing.
 * Added CGBDataLoader::PurgeCache().
 *
 * Revision 1.15  2005/05/12 15:49:49  grichenk
 * Share bdb cache between reader and writer
 *
 * Revision 1.14  2005/02/22 15:25:37  kuznets
 * +HasBlob()
 *
 * Revision 1.13  2004/12/22 21:02:53  grichenk
 * BDB and DBAPI caches split into separate libs.
 * Added entry point registration, fixed driver names.
 *
 * Revision 1.12  2004/12/22 14:34:56  kuznets
 * +GetBlobAccess()
 *
 * Revision 1.11  2004/11/03 17:54:50  kuznets
 * All time related parameters made unsigned
 *
 * Revision 1.10  2004/11/03 17:08:46  kuznets
 * ICache revision2. Add individual timeouts
 *
 * Revision 1.9  2004/08/19 13:02:28  dicuccio
 * Dropped unnecessary export specifier on exceptions
 *
 * Revision 1.8  2004/08/10 16:56:10  grichenk
 * Fixed dll export declarations, moved entry points to cpp.
 *
 * Revision 1.7  2004/07/29 16:45:11  kuznets
 * Removed conflicting entry point
 *
 * Revision 1.6  2004/07/27 14:04:30  kuznets
 * +IsOpen
 *
 * Revision 1.5  2004/07/26 17:04:21  kuznets
 * + plugin manager entry points
 *
 * Revision 1.4  2004/07/26 14:05:39  kuznets
 * + Open with all connection parameters
 *
 * Revision 1.3  2004/07/21 19:04:33  kuznets
 * Added functions to change the size of the memory buffer
 *
 * Revision 1.2  2004/07/19 16:11:51  kuznets
 * + Remove for key,version,subkey
 *
 * Revision 1.1  2004/07/19 14:57:58  kuznets
 * Initial revision
 *
 *
 * ===========================================================================
 */

#endif
