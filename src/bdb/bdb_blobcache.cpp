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
 * File Description:  BDB libarary BLOB cache implementation.
 *
 */

#include <corelib/ncbitime.hpp>
#include <corelib/ncbifile.hpp>
#include <bdb/bdb_blobcache.hpp>
#include <bdb/bdb_cursor.hpp>
#include <corelib/ncbimtx.hpp>

#include <corelib/ncbitime.hpp>


BEGIN_NCBI_SCOPE

// Mutex to sync cache requests coming from different threads
// All requests are protected with one mutex
static CFastMutex x_BDB_BLOB_CacheMutex;


static void s_MakeOverflowFileName(string& buf,
                                   const string& path, 
                                   const string& blob_key,
                                   int           version)
{
    buf = path + blob_key + '_' + NStr::IntToString(version) + ".ov_";
}


class CBDB_BLOB_CacheIReader : public IReader
{
public:

    CBDB_BLOB_CacheIReader(CBDB_BLobStream* blob_stream)
    : m_BlobStream(blob_stream),
      m_OverflowFile(0)
    {}

    CBDB_BLOB_CacheIReader(CNcbiIfstream* overflow_file)
    : m_BlobStream(0),
      m_OverflowFile(overflow_file)
    {}

    virtual ~CBDB_BLOB_CacheIReader()
    {
        delete m_BlobStream;
        delete m_OverflowFile;
    }



    virtual ERW_Result Read(void*   buf, 
                            size_t  count,
                            size_t* bytes_read)
    {
        if (count == 0)
            return eRW_Success;

        // Check if BLOB is file based...
        if (m_OverflowFile) {
            CFastMutexGuard guard(x_BDB_BLOB_CacheMutex);

            m_OverflowFile->read((char*)buf, count);
            *bytes_read = m_OverflowFile->gcount();
            if (*bytes_read == 0) {
                return eRW_Eof;
            }
            return eRW_Success;
        }
        
        // Reading from the BDB stream

        size_t br;

        {{

        CFastMutexGuard guard(x_BDB_BLOB_CacheMutex);
        m_BlobStream->Read(buf, count, &br);

        }}

        if (bytes_read)
            *bytes_read = br;
        
        if (br == 0) 
            return eRW_Eof;
        return eRW_Success;
    }

    virtual ERW_Result PendingCount(size_t* /*count*/)
    {
        return eRW_NotImplemented;
    }


private:
    CBDB_BLOB_CacheIReader(const CBDB_BLOB_CacheIReader&);
    CBDB_BLOB_CacheIReader& operator=(const CBDB_BLOB_CacheIReader&);

private:
    CBDB_BLobStream* m_BlobStream;
    CNcbiIfstream*   m_OverflowFile;

};


static const int s_WriterBufferSize = 2 * (1024 * 1024);

class CBDB_BLOB_CacheIWriter : public IWriter
{
public:

    CBDB_BLOB_CacheIWriter(const char*         path,
                           const string&       blob_key,
                           int                 version,
                           CBDB_BLobStream*    blob_stream,
                           SBLOB_CacheDB&      blob_db,
                           SBLOB_Cache_AttrDB& attr_db)
    : m_Path(path),
      m_BlobKey(blob_key),
      m_Version(version),
      m_BlobStream(blob_stream),
      m_BlobDB(blob_db),
      m_AttrDB(attr_db),
      m_Buffer(0),
      m_BytesInBuffer(0),
      m_OverflowFile(0)
    {
        m_Buffer = new unsigned char[s_WriterBufferSize];
    }

    virtual ~CBDB_BLOB_CacheIWriter()
    {
        // Dumping the buffer
        CFastMutexGuard guard(x_BDB_BLOB_CacheMutex);

        if (m_Buffer) {
            LOG_POST(Info << "LC: Dumping BDB BLOB size=" << m_BytesInBuffer);
            m_BlobStream->Write(m_Buffer, m_BytesInBuffer);
            m_BlobDB.Sync();
            delete m_Buffer;
        }
        delete m_BlobStream;

        m_AttrDB.key = m_BlobKey.c_str();
        m_AttrDB.version = m_Version;
        m_AttrDB.overflow = 0;

        CTime time_stamp(CTime::eCurrent);
        m_AttrDB.time_stamp = (unsigned)time_stamp.GetTimeT();

        if (m_OverflowFile) {
            m_AttrDB.overflow = 1;
            delete m_OverflowFile;
        }
        m_AttrDB.UpdateInsert();
        m_AttrDB.Sync();
    }

    virtual ERW_Result Write(const void* buf, size_t count,
                             size_t* bytes_written = 0)
    {
        if (count == 0)
            return eRW_Success;

        CFastMutexGuard guard(x_BDB_BLOB_CacheMutex);

        unsigned int new_buf_length = m_BytesInBuffer + count;

        if (m_Buffer) {
            // Filling the buffer while we can
            if (new_buf_length < s_WriterBufferSize) {
                ::memcpy(m_Buffer + m_BytesInBuffer, buf, count);
                m_BytesInBuffer = new_buf_length;
                *bytes_written = count;
                return eRW_Success;
            } else {
                // Buffer overflow. Writing to file.
                OpenOverflowFile();
                if (m_OverflowFile) {
                    if (m_BytesInBuffer) {
                        m_OverflowFile->write((char*)m_Buffer, 
                                              m_BytesInBuffer);
                        m_OverflowFile->write((char*)buf, count);
                        delete m_Buffer;
                        m_Buffer = 0;
                        *bytes_written = count;
                        return eRW_Success;
                    }
                }
            }
        }

        if (m_OverflowFile) {
            m_OverflowFile->write((char*)buf, count);
            *bytes_written = count;
            return eRW_Success;
        }

        return eRW_Success;
    }
private:
    void OpenOverflowFile()
    {
        m_OverflowFile = new CNcbiOfstream();
        string path;
        s_MakeOverflowFileName(path, m_Path, m_BlobKey, m_Version);
        LOG_POST(Info << "LC: Making overflow file " << path);
        m_OverflowFile->open(path.c_str(), 
                             IOS_BASE::out | 
                             IOS_BASE::trunc | 
                             IOS_BASE::binary);
        if (!m_OverflowFile->is_open()) {
            LOG_POST(Info << "LC Error:Cannot create overflow file " << path);
            delete m_OverflowFile;
            m_OverflowFile = 0;
        }
    }

private:
    CBDB_BLOB_CacheIWriter(const CBDB_BLOB_CacheIWriter&);
    CBDB_BLOB_CacheIWriter& operator=(const CBDB_BLOB_CacheIWriter&);

private:
    const char*           m_Path;
    string                m_BlobKey;
    int                   m_Version;
    CBDB_BLobStream*      m_BlobStream;

    SBLOB_CacheDB&        m_BlobDB;
    SBLOB_Cache_AttrDB&   m_AttrDB;

    unsigned char*        m_Buffer;
    unsigned int          m_BytesInBuffer;
    CNcbiOfstream*        m_OverflowFile;
};





CBDB_BLOB_Cache::CBDB_BLOB_Cache()
{
}

CBDB_BLOB_Cache::~CBDB_BLOB_Cache()
{
}

void CBDB_BLOB_Cache::Open(const char* path)
{
    m_Path = CDirEntry::AddTrailingPathSeparator(path);

    CFastMutexGuard guard(x_BDB_BLOB_CacheMutex);

    m_Env.OpenWithLocks(path);
    m_BlobDB.SetEnv(m_Env);
    m_AttrDB.SetEnv(m_Env);

    m_BlobDB.SetPageSize(32 * 1024);
    m_BlobDB.SetCacheSize(2 * s_WriterBufferSize);

    m_BlobDB.Open("lc_blob.db",      CBDB_RawFile::eReadWriteCreate);
    m_AttrDB.Open("lc_blob_attr.db", CBDB_RawFile::eReadWriteCreate);
}

void CBDB_BLOB_Cache::Store(const string& key,
                           int           version,
                           const void*   data,
                           size_t        size,
                           EKeepVersions keep_versions)
{
    {{

    CFastMutexGuard guard(x_BDB_BLOB_CacheMutex);

    m_BlobDB.key = key;
    m_BlobDB.version = version;

    if (m_BlobDB.Fetch() == eBDB_Ok) {   // BLOB exists, nothing to do
        return;
    }

    }}

    if (keep_versions == eDropAll || keep_versions == eDropOlder) {
        Purge(key, 0, keep_versions);
    }


    CFastMutexGuard guard(x_BDB_BLOB_CacheMutex);

    m_BlobDB.key = key;
    m_BlobDB.version = version;

    m_BlobDB.Insert(data, size);

    x_UpdateAccessTime(key, version);

    m_AttrDB.key = key;
    m_AttrDB.version = version;

    CTime time_stamp(CTime::eCurrent);
    m_AttrDB.time_stamp = (unsigned)time_stamp.GetTimeT();
    m_AttrDB.overflow = 0;

    m_AttrDB.UpdateInsert();
}


size_t CBDB_BLOB_Cache::GetSize(const string& key,
                               int            version)
{
    CFastMutexGuard guard(x_BDB_BLOB_CacheMutex);

    m_AttrDB.key = key;
    m_AttrDB.version = version;

    EBDB_ErrCode ret = m_AttrDB.Fetch();
    if (ret != eBDB_Ok) {
        return 0;
    }

    int overflow = m_AttrDB.overflow;

    if (overflow) {
        string path;
        s_MakeOverflowFileName(path, m_Path, key, version);
        CFile entry(path);

        if (entry.Exists()) {
            return (size_t) entry.GetLength();
        }

    }

    // Regular inline BLOB

    m_BlobDB.key = key;
    m_BlobDB.version = version;
    ret = m_BlobDB.Fetch();

    if (ret != eBDB_Ok) {
        return 0;
    }
    return m_BlobDB.LobSize();


}


bool CBDB_BLOB_Cache::Read(const string& key, 
                           int           version, 
                           void*         buf, 
                           size_t        buf_size)
{
    CFastMutexGuard guard(x_BDB_BLOB_CacheMutex);

    m_AttrDB.key = key;
    m_AttrDB.version = version;

    EBDB_ErrCode ret = m_AttrDB.Fetch();
    if (ret != eBDB_Ok) {
        return false;
    }

    int overflow = m_AttrDB.overflow;

    if (overflow) {
        string path;
        s_MakeOverflowFileName(path, m_Path, key, version);

        auto_ptr<CNcbiIfstream>  overflow_file(new CNcbiIfstream());
        overflow_file->open(path.c_str(), IOS_BASE::in | IOS_BASE::binary);
        if (!overflow_file->is_open()) {
            return false;
        }
        overflow_file->read((char*)buf, buf_size);
    }


    m_BlobDB.key = key;
    m_BlobDB.version = version;
    ret = m_BlobDB.Fetch();
    if (ret != eBDB_Ok) {
        return false;
    }
    ret = m_BlobDB.GetData(buf, buf_size);
    if (ret != eBDB_Ok) {
        return false;
    }

    x_UpdateAccessTime(key, version);

    return true;
}


void CBDB_BLOB_Cache::Remove(const string& key)
{
    Purge(key, 0, eDropAll);
}


void CBDB_BLOB_Cache::x_UpdateAccessTime(const string&  key,
                                         int            version)
{
    m_AttrDB.key = key;
    m_AttrDB.version = version;

    EBDB_ErrCode ret = m_AttrDB.Fetch();
    if (ret != eBDB_Ok) {
        m_AttrDB.overflow = 0;
    }

    CTime time_stamp(CTime::eCurrent);
    m_AttrDB.time_stamp = (unsigned)time_stamp.GetTimeT();

    m_AttrDB.UpdateInsert();
}


time_t CBDB_BLOB_Cache::GetAccessTime(const string& key,
                                      int           version)
{
    CFastMutexGuard guard(x_BDB_BLOB_CacheMutex);

    m_AttrDB.key = key;
    m_AttrDB.version = version;

    EBDB_ErrCode ret = m_AttrDB.Fetch();
    if (ret == eBDB_Ok) {
        time_t t = m_AttrDB.time_stamp;
        return t;
    }

    return 0;
}

void CBDB_BLOB_Cache::Purge(time_t           access_time,
                            EKeepVersions    keep_last_version)
{
    CFastMutexGuard guard(x_BDB_BLOB_CacheMutex);

    if (keep_last_version == eDropAll && access_time == 0) {
        LOG_POST(Info << "CBDB_BLOB_Cache:: cache truncated");
        m_BlobDB.Truncate();
        m_AttrDB.Truncate();

        // Scan the directory, delete overflow BLOBs
        CDir dir(m_Path);
        string ext;
        string ov_("ov_");
        if (dir.Exists()) {
            CDir::TEntries  content(dir.GetEntries());
            ITERATE(CDir::TEntries, it, content) {
                if (!(*it)->IsFile()) {
                    if (ext == ov_) {
                        (*it)->Remove();
                    }
                }
            }            
        }

        return;
    }

    // Search the records

    CBDB_FileCursor cur(m_AttrDB);
    cur.SetCondition(CBDB_FileCursor::eFirst);

    while (cur.Fetch() == eBDB_Ok) {
        time_t t = m_AttrDB.time_stamp;
        int version = m_AttrDB.version;
        const char* key = m_AttrDB.key;
        int overflow = m_AttrDB.overflow;

        if (t < access_time) {
            x_DropBLOB(key, version, overflow);
        }
    }
}

void CBDB_BLOB_Cache::Purge(const string&    key,
                            time_t           access_time,
                            EKeepVersions    keep_last_version)
{
    CFastMutexGuard guard(x_BDB_BLOB_CacheMutex);

    // Search the records

    CBDB_FileCursor cur(m_AttrDB);
    cur.SetCondition(CBDB_FileCursor::eEQ);

    cur.From << key;

    while (cur.Fetch() == eBDB_Ok) {
        time_t t = m_AttrDB.time_stamp;

        const char* key_str = m_AttrDB.key;
        int version = m_AttrDB.version;
        int overflow = m_AttrDB.overflow;

        if (access_time == 0 || 
            keep_last_version == eDropAll ||
            t < access_time) {
            x_DropBLOB(key_str, version, overflow);
        }
    }    
}


IReader* CBDB_BLOB_Cache::GetReadStream(const string& key, 
                                        int   version)
{
    CFastMutexGuard guard(x_BDB_BLOB_CacheMutex);

    m_AttrDB.key = key;
    m_AttrDB.version = version;

    EBDB_ErrCode ret = m_AttrDB.Fetch();
    if (ret != eBDB_Ok) {
        return 0;
    }

    int overflow = m_AttrDB.overflow;

    // Check if it's an overflow BLOB (external file)

    if (overflow) {
        string path;
        s_MakeOverflowFileName(path, m_Path, key, version);
        auto_ptr<CNcbiIfstream>  overflow_file(new CNcbiIfstream());
        overflow_file->open(path.c_str(), IOS_BASE::in | IOS_BASE::binary);
        if (!overflow_file->is_open()) {
            return 0;
        }
        return new CBDB_BLOB_CacheIReader(overflow_file.release());

    }

    // Inline BLOB, reading from BDB storage

    m_BlobDB.key = key;
    m_BlobDB.version = version;
    
    ret = m_BlobDB.Fetch();
    if (ret != eBDB_Ok) {
        return 0;
    }
    CBDB_BLobStream* bstream = m_BlobDB.CreateStream();
    return new CBDB_BLOB_CacheIReader(bstream);

}

IWriter* CBDB_BLOB_Cache::GetWriteStream(const string&    key, 
                                         int              version,
                                         EKeepVersions    keep_versions)
{
    if (keep_versions == eDropAll || keep_versions == eDropOlder) {
        Purge(key, 0, keep_versions);
    }

    CFastMutexGuard guard(x_BDB_BLOB_CacheMutex);

    m_BlobDB.key = key;
    m_BlobDB.version = version;

    CBDB_BLobStream* bstream = m_BlobDB.CreateStream();
    return 
        new CBDB_BLOB_CacheIWriter(m_Path.c_str(), 
                                   key, 
                                   version,
                                   bstream, 
                                   m_BlobDB,
                                   m_AttrDB);
}

void CBDB_BLOB_Cache::x_DropBLOB(const char*    key,
                                 int            version,
                                 int            overflow)
{
    if (overflow == 1) {
        string path;
        s_MakeOverflowFileName(path, m_Path, key, version);

        CDirEntry entry(path);
        if (entry.Exists()) {
            entry.Remove();
        }
    }
    m_BlobDB.key = key;
    m_BlobDB.version = version;

    m_BlobDB.Delete(CBDB_RawFile::eIgnoreError);

    m_AttrDB.key = key;
    m_AttrDB.version = version;

    m_AttrDB.Delete(CBDB_RawFile::eIgnoreError);
}


END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.10  2003/10/16 12:08:16  ucko
 * Address GCC 2.95 errors about missing sprintf declaration, and avoid
 * possible buffer overflows, by rewriting s_MakeOverflowFileName to use
 * C++ strings.
 *
 * Revision 1.9  2003/10/16 00:30:57  ucko
 * ios_base -> IOS_BASE (should fix GCC 2.95 build)
 *
 * Revision 1.8  2003/10/15 18:39:13  kuznets
 * Fixed minor incompatibility with the C++ language.
 *
 * Revision 1.7  2003/10/15 18:13:16  kuznets
 * Implemented new cache architecture based on combination of BDB tables
 * and plain files. Fixes the performance degradation in Berkeley DB
 * when it has to work with a lot of overflow pages.
 *
 * Revision 1.6  2003/10/06 16:24:19  kuznets
 * Fixed bug in Purge function
 * (truncated cache files with some parameters combination).
 *
 * Revision 1.5  2003/10/02 20:13:25  kuznets
 * Minor code cleanup
 *
 * Revision 1.4  2003/09/29 16:26:34  kuznets
 * Reflected ERW_Result rename + cleaned up 64-bit compilation
 *
 * Revision 1.3  2003/09/29 15:45:17  kuznets
 * Minor warning fixed
 *
 * Revision 1.2  2003/09/24 15:59:45  kuznets
 * Reflected changes in IReader/IWriter <util/reader_writer.hpp>
 *
 * Revision 1.1  2003/09/24 14:30:17  kuznets
 * Initial revision
 *
 *
 * ===========================================================================
 */
