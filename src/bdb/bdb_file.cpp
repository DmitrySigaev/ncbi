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
 * File Description:  BDB libarary file implementations.
 *
 */

#include <ncbi_pch.hpp>
#include <bdb/bdb_file.hpp>
#include <bdb/bdb_env.hpp>
#include <bdb/bdb_trans.hpp>

#include <db.h>

#ifdef verify
#undef verify
#endif

BEGIN_NCBI_SCOPE

const char CBDB_RawFile::kDefaultDatabase[] = "_table";
const int  CBDB_RawFile::kOpenFileMask      = 0664;



/// Auto-pointer style guard class for DB structure
///
/// @internal
///
class CDB_guard
{
public:
    CDB_guard(DB** db) : m_DB(db) {}
    ~CDB_guard()
    { 
        if (m_DB  &&  *m_DB) {
            (*m_DB)->close(*m_DB, 0);
            *m_DB = 0;
        }
    }
    void release() { m_DB = 0; }
private:
    DB** m_DB;
};

/////////////////////////////////////////////////////////////////////////////
//  CBDB_MultiRowBuffer::
//


CBDB_MultiRowBuffer::CBDB_MultiRowBuffer(size_t buf_size)
    : m_Data_DBT(new DBT)
    , m_Buf(new unsigned char[buf_size])
    , m_BufSize(buf_size)
    , m_BufPtr(0)
    , m_LastKey(0)
    , m_LastData(0)
    , m_LastKeyLen(0)
    , m_LastDataLen(0)
{
}

CBDB_MultiRowBuffer::~CBDB_MultiRowBuffer()
{
    delete [] (unsigned char*)(m_Buf);
    delete m_Data_DBT;
}

void CBDB_MultiRowBuffer::InitDBT()
{
    memset(m_Data_DBT, 0, sizeof(DBT));
    m_Data_DBT->data = m_Buf;
    m_Data_DBT->ulen = m_Data_DBT->size = m_BufSize;
    m_Data_DBT->flags = DB_DBT_USERMEM;
}

void CBDB_MultiRowBuffer::MultipleInit()
{
    DB_MULTIPLE_INIT(m_BufPtr, m_Data_DBT);
}


/////////////////////////////////////////////////////////////////////////////
//  CBDB_RawFile::
//



CBDB_RawFile::CBDB_RawFile(EDuplicateKeys dup_keys, EDBType db_type)
: m_DB_Type(db_type),
  m_DB(0),
  m_DBT_Key(0),
  m_DBT_Data(0),
  m_Env(0),
  m_Trans(0),
  m_TransAssociation(CBDB_Transaction::eFullAssociation),
  m_RecLen(0),
  m_H_ffactor(0),
  m_H_nelem(0),
  m_DB_Attached(false),
  m_ByteSwapped(false),
  m_RevSplitOff(false),
  m_CmpOverride(true),
  m_PageSize(0),
  m_CacheSize(256 * 1024),
  m_DuplicateKeys(dup_keys)
{
    if (m_DB_Type == eQueue)
    {
        dup_keys = eDuplicatesDisable;
    }

    try
    {
        m_DBT_Key = new DBT;
        m_DBT_Data = new DBT;
    }
    catch (...)
    {
        delete m_DBT_Key;
        delete m_DBT_Data;
        throw;
    }

    ::memset(m_DBT_Key,  0, sizeof(DBT));
    ::memset(m_DBT_Data, 0, sizeof(DBT));
}


CBDB_RawFile::~CBDB_RawFile()
{
    x_Close(eIgnoreError);
    delete m_DBT_Key;
    delete m_DBT_Data;

    // It's illegal to close a file involved in active transactions
    
    if ( m_Trans != 0 && 
        (m_TransAssociation == (int) CBDB_Transaction::eFullAssociation) &&
        (m_Trans->IsInProgress())) {
        _ASSERT(0);
        
        // If we are here we can try to communicate by throwing
        // an exception. It's illegal, but situation is bad enough already
        BDB_THROW(eTransInProgress, 
                  "Cannot close the file while transaction is in progress.");
    }
}


void CBDB_RawFile::Close()
{
    x_Close(eThrowOnError);
}

void CBDB_RawFile::Attach(CBDB_RawFile& bdb_file)
{
   Close();
   m_DB = bdb_file.m_DB;
   m_DB_Attached = true; 
}

void CBDB_RawFile::SetEnv(CBDB_Env& env)
{
    m_Env = &env;
}

DB_TXN* CBDB_RawFile::GetTxn()
{
    if (m_Trans)
        return m_Trans->GetTxn();
    return 0;
}

void CBDB_RawFile::x_Close(EIgnoreError close_mode)
{
    LOG_POST(Info << "Closing: " << m_FileName);
    if ( m_FileName.empty() )
        return;

    if (m_DB_Attached) {
        m_DB = 0;
        m_DB_Attached = false;
    }
    else
    if (m_DB) {
        int ret = m_DB->close(m_DB, 0);
        m_DB = 0;
        if (close_mode == eThrowOnError) {
            BDB_CHECK(ret, m_FileName.c_str());
        } else {
            if (ret != 0) {
                ERR_POST("Error when closing " << m_FileName);
            }
        }
    }

    m_FileName.erase();
    m_Database.erase();
}


void CBDB_RawFile::Open(const string& filename,
                        const string& database,
                        EOpenMode     open_mode,
                        bool          support_dirty_read,
                        unsigned      rec_len)
{
    if ( !m_FileName.empty() )
        Close();

    const char * db;
    if (database.empty()) {
        db = 0; 
    } else {
        db = database.c_str();
    }


    x_Open(filename.c_str(), db, open_mode, support_dirty_read, rec_len);

    m_FileName = filename;
    if (db)
        m_Database = database;
    else
        m_Database = "";
}


void CBDB_RawFile::Reopen(EOpenMode open_mode, 
                          bool      support_dirty_read,
                          unsigned  rec_len)
{
    _ASSERT(!m_FileName.empty());

    if (m_DB_Attached) {
        BDB_THROW(eInvalidOperation, "Cannot reopen attached object");
    }

    int ret = m_DB->close(m_DB, 0);
    m_DB = 0;

    BDB_CHECK(ret, m_FileName.c_str());
    x_Open(m_FileName.c_str(),
           !m_Database.empty() ? m_Database.c_str() : 0,
           open_mode, support_dirty_read, rec_len);
}


void CBDB_RawFile::Remove(const string& filename, const string& database)
{
    const char* db_name;
    if (database.empty()) {
        db_name = 0; 
    } else {
        db_name = database.c_str();
    }

    if (m_DB_Attached) {
        BDB_THROW(eInvalidOperation, "Cannot remove attached object");
    }

    // temporary DB is used here, because BDB remove call invalidates the
    // DB argument redardless of the result.
    DB* db = 0;
    CDB_guard guard(&db);
    int ret = db_create(&db, m_Env ? m_Env->GetEnv() : 0, 0);
    BDB_CHECK(ret, 0);

    ret = db->remove(db, filename.c_str(), db_name, 0);
    guard.release();
    if (ret == ENOENT || ret == EINVAL)
        return;  // Non existent table cannot be removed

    BDB_CHECK(ret, filename.c_str());
}


unsigned int CBDB_RawFile::Truncate()
{
    _ASSERT(m_DB != 0);
    u_int32_t count;
    DB_TXN* txn = GetTxn();
    int ret = m_DB->truncate(m_DB,
                             txn,
                             &count,
                             0);

    BDB_CHECK(ret, FileName().c_str());
    return count;
}


/// Compact the database.  The target fill percent per page can be
/// supplied, to allow for known expansion
void CBDB_RawFile::Compact(ECompact compact_type,
                           int target_fill_pct)
{
#ifdef DB_COMPACT_FLAGS
    _ASSERT(m_DB != 0);

    u_int32_t flags = 0;
    if (compact_type == eCompactNoFree) {
        /// default
    } else if (compact_type == eCompactFreeExisting) {
        flags = DB_FREELIST_ONLY;
    } else if (compact_type == eCompactFreeAll) {
        flags = DB_FREE_SPACE;
    }

    target_fill_pct = max(target_fill_pct, 0);
    DB_TXN* txn = GetTxn();

    DB_COMPACT compact;
    memset(&compact, 0, sizeof(compact));
    compact.compact_fillpercent = target_fill_pct;
    compact.compact_timeout     = 0;

    int ret = m_DB->compact(m_DB, txn, NULL, NULL, &compact,
                            flags, NULL);
    BDB_CHECK(ret, FileName().c_str());

    LOG_POST(Info << "CBDB_RawFile::Compact(): "
             << compact.compact_pages_examine << " pages examined / "
             << compact.compact_pages_free << " pages freed / "
             << compact.compact_levels << " levels removed / "
             << compact.compact_pages_truncated << " pages truncated");
#endif
}


void CBDB_RawFile::SetCacheSize(unsigned int cache_size)
{
    m_CacheSize = cache_size;
    if (m_DB) {
        int ret = m_DB->set_cachesize(m_DB, 0, m_CacheSize, 1);
        BDB_CHECK(ret, 0);
    }
}


void CBDB_RawFile::RevSplitOff()
{
    m_RevSplitOff = true;
}

void CBDB_RawFile::SetTransaction(CBDB_Transaction* trans)
{
    if (m_Trans) {
        if (m_TransAssociation == (int) CBDB_Transaction::eFullAssociation) {
            m_Trans->RemoveFile(this);
        }
    }

    m_Trans = trans;
    if (m_Trans) {
        m_TransAssociation = m_Trans->GetAssociationMode();
        if (m_TransAssociation == (int) CBDB_Transaction::eFullAssociation) {
            m_Trans->AddFile(this);
        }
    }
}

void CBDB_RawFile::x_RemoveTransaction(CBDB_Transaction* trans)
{
    if (trans == m_Trans) {
        m_Trans = 0;
    }
}


void CBDB_RawFile::x_CreateDB(unsigned rec_len)
{
    _ASSERT(m_DB == 0);
    _ASSERT(!m_DB_Attached);

    CDB_guard guard(&m_DB);

    int ret = db_create(&m_DB, m_Env ? m_Env->GetEnv() : 0, 0);
    BDB_CHECK(ret, 0);

    if (m_DB_Type == eBtree && m_CmpOverride) {
        SetCmp(m_DB);
    }
    if (m_DB_Type == eHash && m_CmpOverride) {
        SetHash(m_DB);
    }

    if ( m_PageSize ) {
        ret = m_DB->set_pagesize(m_DB, m_PageSize);
        BDB_CHECK(ret, 0);
    } else {
        m_DB->get_pagesize(m_DB, &m_PageSize);
    }

    if (!m_Env) {
        ret = m_DB->set_cachesize(m_DB, 0, m_CacheSize, 1);
        BDB_CHECK(ret, 0);
    }

    if (DuplicatesAllowed()) {
        ret = m_DB->set_flags(m_DB, DB_DUP);

        BDB_CHECK(ret, 0);
    }

    if (m_RevSplitOff) {
        ret = m_DB->set_flags(m_DB, DB_REVSPLITOFF);
        BDB_CHECK(ret, 0);
    }

    switch (m_DB_Type)
    {
    case eQueue:
        _ASSERT(rec_len);
        m_RecLen = rec_len;
        ret = m_DB->set_re_len(m_DB, rec_len);
        BDB_CHECK(ret, 0);
        break;
    case eHash:
        if (m_H_ffactor) {
            int ret = m_DB->set_h_ffactor(m_DB, m_H_ffactor);
            BDB_CHECK(ret, FileName().c_str());
        }
        if (m_H_nelem) {
            int ret = m_DB->set_h_nelem(m_DB, m_H_nelem);
            BDB_CHECK(ret, FileName().c_str());
        }
        break;
    default:
        break;
    }

    guard.release();
}


void CBDB_RawFile::x_Open(const char* filename,
                          const char* database,
                          EOpenMode   open_mode,
                          bool        support_dirty_read,
                          unsigned    rec_len)
{   
    int ret;    
    if (m_DB == 0) {
        x_CreateDB(rec_len);
    }

    if (open_mode == eCreate) {
        Remove(filename, database ? database : "");
        x_Create(filename, database);
    }
    else {
        u_int32_t open_flags;

        switch (open_mode)
        {
        case eReadOnly:
            open_flags = DB_RDONLY;
            break;
        case eCreate:
            open_flags = DB_CREATE;
            break;
        default:
            open_flags = 0;
            break;
        }

        DB_TXN* txn = 0; // GetTxn();

        if (m_Env) {
            if (m_Env->IsTransactional()) {
                open_flags |= DB_THREAD | DB_AUTO_COMMIT;
            }
        }

        if (support_dirty_read) {
            open_flags |= DB_DIRTY_READ;
        }


        DBTYPE db_type = DB_BTREE;
        switch (m_DB_Type) {
        case eBtree:
            break;
        case eQueue:
            db_type = DB_QUEUE;
            m_RecLen = rec_len;
            break;
        default:
            _ASSERT(0);
        }

        ret = m_DB->open(m_DB,
                         txn,
                         filename,
                         database,             // database name
                         db_type,
                         open_flags,
                         kOpenFileMask
                        );
        if ( ret ) {
            if (open_mode == eCreate || 
                open_mode == eReadWriteCreate)
            {
                x_Create(filename, database);
            }
            else {
                m_DB->close(m_DB, 0);
                m_DB = 0;
                BDB_CHECK(ret, filename);
            }
        } else {
            // file opened succesfully, check if it needs
            // a byte swapping (different byteorder)

            int isswapped;
            ret = m_DB->get_byteswapped(m_DB, &isswapped);
            BDB_CHECK(ret, filename);

            m_ByteSwapped = (isswapped!=0);
            if (m_ByteSwapped) {
                // re-open the file
                m_DB->close(m_DB, 0);
                m_DB = 0;

                x_SetByteSwapped(m_ByteSwapped);
                x_CreateDB(rec_len);

                ret = m_DB->open(m_DB,
                                 txn,
                                 filename,
                                 database, // database name
                                 db_type,
                                 open_flags,
                                 kOpenFileMask);
                BDB_CHECK(ret, filename);

            }

        }
    } // else open_mode == Create

    m_OpenMode = open_mode;

}


void CBDB_RawFile::SetPageSize(unsigned int page_size)
{
    _ASSERT(m_DB == 0); // we can set page size only before opening the file
    if (((page_size - 1) & page_size) != 0) {
        BDB_THROW(eInvalidValue, "Page size must be power of 2");
    }
    m_PageSize = page_size;
}

unsigned int CBDB_RawFile::GetPageSize() const
{
    _ASSERT(m_DB == 0); // we can set page size only before opening the file
    return m_PageSize;
}

void CBDB_RawFile::Sync()
{
    int ret = m_DB->sync(m_DB, 0);
    BDB_CHECK(ret, FileName().c_str());
}


// check BDB version 4.3 changed DB->stat signature

#ifndef BDB_USE_NEW_STAT
#if DB_VERSION_MAJOR >= 4 
    #if DB_VERSION_MINOR >= 3
        #define BDB_USE_NEW_STAT
    #endif
#endif
#endif
    

unsigned CBDB_RawFile::CountRecs()
{
    DB_BTREE_STAT* stp;
#ifdef BDB_USE_NEW_STAT
    CBDB_Transaction* trans = GetTransaction();
    DB_TXN* txn = trans ? trans->GetTxn() : 0;
    int ret = m_DB->stat(m_DB, txn, &stp, 0);
#else
    int ret = m_DB->stat(m_DB, &stp, 0);
#endif

    BDB_CHECK(ret, FileName().c_str());
    u_int32_t rc = stp->bt_ndata;

    ::free(stp);

    return rc;
}

void CBDB_RawFile::PrintStat(CNcbiOstream & out)
{
    DB_BTREE_STAT* stp;
#ifdef BDB_USE_NEW_STAT
    CBDB_Transaction* trans = GetTransaction();
    DB_TXN* txn = trans ? trans->GetTxn() : 0;
    int ret = m_DB->stat(m_DB, txn, &stp, 0);
#else
    int ret = m_DB->stat(m_DB, &stp, 0);
#endif

    BDB_CHECK(ret, FileName().c_str());

    out << FileName().c_str() << NcbiEndl;
    out << "bt_version    : " << stp->bt_version    << NcbiEndl
        << "bt_nkeys      : " << stp->bt_nkeys      << NcbiEndl
        << "bt_ndata      : " << stp->bt_ndata      << NcbiEndl
        << "bt_pagesize   : " << stp->bt_pagesize   << NcbiEndl
        << "bt_levels     : " << stp->bt_levels     << NcbiEndl
        << "bt_int_pg     : " << stp->bt_int_pg     << NcbiEndl
        << "bt_leaf_pg    : " << stp->bt_leaf_pg    << NcbiEndl
        << "bt_dup_pg     : " << stp->bt_dup_pg     << NcbiEndl
        << "bt_over_pg    : " << stp->bt_over_pg    << NcbiEndl
#ifdef BDB_USE_NEW_STAT
        << "bt_empty_pg   : " << stp->bt_empty_pg   << NcbiEndl
#endif
        << "bt_free       : " << stp->bt_free       << NcbiEndl
        << "bt_int_pgfree : " << stp->bt_int_pgfree << NcbiEndl
        << "bt_leaf_pgfree: " << stp->bt_leaf_pgfree<< NcbiEndl
        << "bt_dup_pgfree : " << stp->bt_dup_pgfree << NcbiEndl
        << "bt_over_pgfree: " << stp->bt_over_pgfree<< NcbiEndl
    ;

    ::free(stp);
}


void CBDB_RawFile::x_Create(const char* filename, const char* database)
{
    _ASSERT(!m_DB_Attached);
    u_int32_t open_flags = DB_CREATE;

    DB_TXN* txn = 0; //GetTxn();

    if (m_Env) {
        if (m_Env->IsTransactional()) {
            open_flags |= DB_THREAD | DB_AUTO_COMMIT;
        }
    }

    DBTYPE db_type = DB_BTREE;
    switch (m_DB_Type) {
    case eBtree:
        break;
    case eQueue:
        db_type = DB_QUEUE;
        break;
    case eHash:
        db_type = DB_HASH;
        break;
    default:
        _ASSERT(0);
    }

    int ret = m_DB->open(m_DB,
                         txn,
                         filename,
                         database,        // database name
                         db_type,
                         open_flags,
                         kOpenFileMask);
    if ( ret ) {
        m_DB->close(m_DB, 0);
        m_DB = 0;
        BDB_CHECK(ret, filename);
    }
}


DBC* CBDB_RawFile::CreateCursor(CBDB_Transaction* trans, 
                                unsigned int      flags) const
{
    DBC* cursor;

    if (!m_DB) {
        BDB_THROW(eInvalidValue, "Cannot create cursor for unopen file.");
    }

    DB_TXN* txn = 0; // GetTxn();
    if (trans) {
        txn = trans->GetTxn();
    }

    int ret = m_DB->cursor(m_DB,
                           txn,
                           &cursor,
                           flags);
    BDB_CHECK(ret, FileName().c_str());
    return cursor;
}

void CBDB_RawFile::x_SetByteSwapped(bool bswp)
{
    m_ByteSwapped = bswp;
}


void CBDB_RawFile::SetHashFillFactor(unsigned h_ffactor)
{
    _ASSERT(m_DB_Type == eHash);
    m_H_ffactor = h_ffactor;
}

void CBDB_RawFile::SetHashNelem(unsigned h_nelem)
{
    _ASSERT(m_DB_Type == eHash);
    m_H_nelem = h_nelem;
}

void CBDB_RawFile::SetHash(DB* db)
{
    _ASSERT(m_DB_Type == eHash);
    int ret = db->set_h_hash(db, BDB_Hash);
    BDB_CHECK(ret, 0);
}


/////////////////////////////////////////////////////////////////////////////
//
//  CBDB_File::
//


CBDB_File::CBDB_File(EDuplicateKeys dup_keys, EDBType db_type)
    : CBDB_RawFile(dup_keys, db_type),
      m_KeyBuf(new CBDB_BufferManager),
      m_BufsAttached(false),
      m_BufsCreated(false),
      m_DataBufDisabled(false),
      m_LegacyString(false),
      m_OwnFields(false),
      m_DisabledNull(false),
      m_PrefixCompress(false)
{
}

void CBDB_File::SetFieldOwnership(bool own_fields) 
{ 
    m_OwnFields = own_fields; 
    
    m_KeyBuf->SetFieldOwnership(own_fields);
    if (m_DataBuf.get() != 0) {
        m_DataBuf->SetFieldOwnership(own_fields);
    }
}

void CBDB_File::x_ConstructKeyBuf()
{
	m_KeyBuf.reset(new CBDB_BufferManager);
    m_KeyBuf->SetLegacyStringsCheck(m_LegacyString);
    m_KeyBuf->SetFieldOwnership(m_OwnFields);
}		

void CBDB_File::x_ConstructDataBuf()
{
	m_DataBuf.reset(new CBDB_BufferManager);
    if (!m_DisabledNull) {
        m_DataBuf->SetNullable();
    }
    m_DataBuf->SetLegacyStringsCheck(m_LegacyString);
    m_DataBuf->SetFieldOwnership(m_OwnFields);
}		

void CBDB_File::BindKey(const char* field_name,
                        CBDB_Field* key_field,
                        size_t      buf_size)
{
    _ASSERT(!IsOpen());
    _ASSERT(m_KeyBuf.get());
    _ASSERT(key_field);

    key_field->SetName(field_name);
    m_KeyBuf->Bind(key_field);
    if ( buf_size )
        key_field->SetBufferSize(buf_size);
}


void CBDB_File::BindData(const char* field_name,
                         CBDB_Field* data_field,
                         size_t      buf_size,
                         ENullable   is_nullable)
{
    _ASSERT(!IsOpen());
    _ASSERT(data_field);

    data_field->SetName(field_name);

    if (m_DataBuf.get() == 0) {  // data buffer is not yet created 
		x_ConstructDataBuf();
    }

    m_DataBuf->Bind(data_field);
    if ( buf_size > 0) {
        data_field->SetBufferSize(buf_size);
    }
    if (is_nullable == eNullable && !m_DisabledNull) {
        data_field->SetNullable();
    }
}


void CBDB_File::Open(const string& filename,
                     const string& database,
                     EOpenMode     open_mode,
                     bool          support_dirty_read,
                     unsigned      rec_len)
{
    if ( IsOpen() )
        Close();
    x_CheckConstructBuffers();

    if (m_DB_Type == eQueue) {
        DisableDataPacking();
        if (m_DataBuf.get()) {
            rec_len = m_DataBuf->ComputeBufferSize();
        }
    }

    CBDB_RawFile::Open(filename, database, 
                        open_mode, support_dirty_read, rec_len);

    m_DB->app_private = (void*) m_KeyBuf.get();

}


void CBDB_File::Reopen(EOpenMode open_mode, bool support_dirty_read)
{
    unsigned rec_len = 0;
    if (m_DB_Type == eQueue) {
        if (m_DataBuf.get()) {
            rec_len = m_DataBuf->ComputeBufferSize();
        }
    }
    CBDB_RawFile::Reopen(open_mode, support_dirty_read, rec_len);
    m_DB->app_private = (void*) m_KeyBuf.get();
    if ( m_DataBuf.get() ) {
        m_DataBuf->SetAllNull();
    }
    bool byte_swapped = IsByteSwapped();
    m_KeyBuf->SetByteSwapped(byte_swapped);
    if (m_DataBuf.get()) {
        m_DataBuf->SetByteSwapped(byte_swapped);
    }
}


void CBDB_File::Attach(CBDB_File& db_file)
{
    CBDB_RawFile::Attach(db_file);
    x_CheckConstructBuffers();
    SetLegacyStringsCheck(db_file.m_LegacyString);
}

void CBDB_File::SetLegacyStringsCheck(bool value) 
{ 
    m_LegacyString = value; 
    if (m_KeyBuf.get()) {
        m_KeyBuf->SetLegacyStringsCheck(value);
    }
    if (m_DataBuf.get()) {
        m_DataBuf->SetLegacyStringsCheck(value);
    }
}

void CBDB_File::x_SetByteSwapped(bool bswp)
{
    CBDB_RawFile::x_SetByteSwapped(bswp);
    m_KeyBuf->SetByteSwapped(bswp);
    if (m_DataBuf.get()) {
        m_DataBuf->SetByteSwapped(bswp);
    }
}

void CBDB_File::Verify(const char* filename, 
                       const char* database, 
                       FILE* backup)
{
    if (m_DB == 0) {
        x_CreateDB(0);
    }
    x_CheckConstructBuffers();
    m_DB->app_private = (void*) m_KeyBuf.get();

    /*int ret = */
    m_DB->verify(m_DB, filename, database, backup, backup ? DB_SALVAGE: 0);
}

// v 4.3.xx introduced new error code DB_BUFFER_SMALL
#if DB_VERSION_MAJOR >= 4 
    #if DB_VERSION_MINOR >= 3
        #define BDB_CHECK_BUFFER_SMALL
    #endif
#endif


EBDB_ErrCode CBDB_File::x_Fetch(unsigned int flags)
{
    x_StartRead();

    DB_TXN* txn = GetTxn();

    int ret = m_DB->get(m_DB,
                        txn,
                        m_DBT_Key,
                        m_DBT_Data,
                        flags);
                        
    if (ret == DB_NOTFOUND) {
        return eBDB_NotFound;
    }
    
    // Disable error reporting for custom m_DBT_data management

# ifdef BDB_CHECK_BUFFER_SMALL
    if ((ret == ENOMEM || ret == DB_BUFFER_SMALL) 
           && m_DataBufDisabled && m_DBT_Data->data == 0) {
        ret = 0;
    }
# else
    if (ret == ENOMEM && m_DataBufDisabled && m_DBT_Data->data == 0) {
        ret = 0;
    }
# endif
    BDB_CHECK(ret, FileName().c_str());

    x_EndRead();
    return eBDB_Ok;
}

EBDB_ErrCode CBDB_File::FetchForUpdate()
{
    return x_Fetch(DB_RMW);
}


DBT* CBDB_File::CloneDBT_Key()
{
    x_StartRead();
    x_EndRead();

    DBT* dbt = new DBT;
    ::memset(dbt,  0, sizeof(DBT));

    // Clone the "data" area (needs to be properly deleted!)
    if (m_DBT_Key->ulen) {
        dbt->size = m_DBT_Key->size;
        dbt->ulen = m_DBT_Key->ulen;
        unsigned char* p = (unsigned char*)malloc(dbt->ulen);
        ::memcpy(p, m_DBT_Key->data, m_DBT_Key->size);
        dbt->data = p;
        dbt->flags = DB_DBT_USERMEM;
    }
    return dbt;
}

void CBDB_File::DestroyDBT_Clone(DBT* dbt)
{
    unsigned char* p = (unsigned char*)dbt->data;
    free(p); dbt->data = NULL;
    delete dbt;
}


EBDB_ErrCode CBDB_File::Insert(EAfterWrite write_flag)
{
    CheckNullDataConstraint();

    unsigned int flags;
    if (DuplicatesAllowed()) {
        flags = 0;
    } else {
        flags = /*DB_NODUPDATA |*/ DB_NOOVERWRITE;
    }
    return x_Write(flags, write_flag);
}

unsigned CBDB_File::Append(EAfterWrite write_flag)
{
    unsigned int flags = DB_APPEND;
    x_Write(flags, write_flag);
    unsigned rec_id;
    memcpy(&rec_id, m_DBT_Key->data, sizeof(rec_id));
    return rec_id;
}


EBDB_ErrCode CBDB_File::UpdateInsert(EAfterWrite write_flag)
{
    CheckNullDataConstraint();
    return x_Write(0, write_flag);
}


EBDB_ErrCode CBDB_File::Delete(EIgnoreError on_error)
{
    m_KeyBuf->PrepareDBT_ForWrite(m_DBT_Key);
    DB_TXN* txn = GetTxn();

    int ret = m_DB->del(m_DB,
                        txn,
                        m_DBT_Key,
                        0);
    if (on_error != eIgnoreError) {
        BDB_CHECK(ret, FileName().c_str());
    }
    Discard();
    return eBDB_Ok;
}


void CBDB_File::Discard()
{
    m_KeyBuf->ArrangePtrsUnpacked();
    if ( m_DataBuf.get() ) {
        m_DataBuf->ArrangePtrsUnpacked();
        m_DataBuf->SetAllNull();
    }
}


/// @internal
size_t
BDB_compare_prefix(DB* dbp, const DBT* a, const DBT* b)
{
	size_t cnt, len;
	char* p1, *p2;

    cnt = 1; len = a->size > b->size ? b->size : a->size; 
    p1 = (char*)a->data, p2 = (char*)b->data;
    for (;len--; ++p1, ++p2, ++cnt) { 
        if (*p1 != *p2) {
            return (cnt);
        }
    }
    if (a->size < b->size) return (a->size + 1); 
    if (b->size < a->size) return (b->size + 1); 
    return (b->size); 
}


void CBDB_File::SetCmp(DB* db)
{
    _ASSERT(m_DB_Type == eBtree);
    BDB_CompareFunction func = m_KeyBuf->GetCompareFunction();
    _ASSERT(func);
    int ret = db->set_bt_compare(db, func);
    BDB_CHECK(ret, 0);

    if (m_PrefixCompress) {
        ret = m_DB->set_bt_prefix(m_DB, BDB_compare_prefix);
        BDB_CHECK(ret, 0);
    }
}



CBDB_File::TUnifiedFieldIndex 
CBDB_File::GetFieldIdx(const string& name) const
{
    int fidx = 0;
    if (m_KeyBuf.get()) {
        fidx = m_KeyBuf->GetFieldIndex(name);
        if (fidx >= 0) {    //  field name found
            return BDB_GetUFieldIdx(fidx, true /*key*/);
        }
    }

    if (m_DataBuf.get()) {
        fidx = m_DataBuf->GetFieldIndex(name);
        if (fidx >= 0) {    //  field name found
            return BDB_GetUFieldIdx(fidx, false /*non-key*/);
        }
    }
    return 0;
}

const CBDB_Field& CBDB_File::GetField(TUnifiedFieldIndex idx) const
{
    _ASSERT(idx != 0);

    const CBDB_BufferManager* buffer;

    if (idx < 0) { // key buffer
        idx = -idx;
        --idx;
        buffer = m_KeyBuf.get();
    } else {  // data buffer
        --idx;
        buffer = m_DataBuf.get();
    }

    _ASSERT(buffer);

    const CBDB_Field& fld = buffer->GetField(idx);
    return fld;
}


CBDB_Field& CBDB_File::GetField(TUnifiedFieldIndex idx)
{
    _ASSERT(idx != 0);

    CBDB_BufferManager* buffer;

    if (idx < 0) {     // key buffer
        idx = -idx;
        --idx;
        buffer = m_KeyBuf.get();
    } else {          // data buffer
        --idx;
        buffer = m_DataBuf.get();
    }
    _ASSERT(buffer);

    CBDB_Field& fld = buffer->GetField(idx);
    return fld;
}

void CBDB_File::DisableDataPacking()
{
    if (m_DataBuf.get()) {
        m_DataBuf->SetPackable(false); // disable packing
    }
}

void CBDB_File::CopyFrom(const CBDB_File& dbf)
{
    const CBDB_BufferManager* src_key  = dbf.GetKeyBuffer();
    const CBDB_BufferManager* src_data = dbf.GetDataBuffer();
	
    CBDB_BufferManager* key  = GetKeyBuffer();
    CBDB_BufferManager* data = GetDataBuffer();
	
	key->CopyFrom(*src_key);
	if (data) {
		data->CopyFrom(*src_data);
	}
}

void CBDB_File::DuplicateStructure(const CBDB_File& dbf)
{
    const CBDB_BufferManager* src_key  = dbf.GetKeyBuffer();
    const CBDB_BufferManager* src_data = dbf.GetDataBuffer();
	
	_ASSERT(src_key);
	
	x_ConstructKeyBuf();
	m_KeyBuf->DuplicateStructureFrom(*src_key);
	
	if (src_data) {
		x_ConstructDataBuf();
		m_DataBuf->DuplicateStructureFrom(*src_data);
	} else {
		m_DataBuf.reset(0);
	}
}

EBDB_ErrCode CBDB_File::ReadCursor(DBC* dbc, unsigned int bdb_flag)
{
    x_StartRead();

    if (m_DataBufDisabled) {
        m_DBT_Data->size  = 0;
        m_DBT_Data->flags = 0;
        m_DBT_Data->data  = 0;        
    }

    int ret = dbc->c_get(dbc,
                         m_DBT_Key,
                         m_DBT_Data,
                         bdb_flag);

    switch (ret) {
    case DB_NOTFOUND:
        return eBDB_NotFound;
    case DB_KEYEMPTY:
        // record has been deleted
        return eBDB_KeyEmpty;
    }

    BDB_CHECK(ret, FileName().c_str());
    x_EndRead();
    return eBDB_Ok;
}

EBDB_ErrCode CBDB_File::ReadCursor(DBC*         dbc, 
                                   unsigned int bdb_flag,
                                   TBuffer*     buf)
{
    _ASSERT(buf);
    
    if (buf->size() == 0) {
        buf->resize(1024);
    } 
    if (buf->size() < buf->capacity()) {
        buf->resize(buf->capacity());
    }

    x_StartRead();
    m_DBT_Data->data = &((*buf)[0]);
    m_DBT_Data->ulen = buf->size();
    m_DBT_Data->size = 0;
    m_DBT_Data->flags = DB_DBT_USERMEM;

    int ret = dbc->c_get(dbc,
                         m_DBT_Key,
                         m_DBT_Data,
                         bdb_flag);

    switch (ret) {
    case DB_NOTFOUND:
        buf->resize_mem(0);
        return eBDB_NotFound;
    case DB_KEYEMPTY:
        // record has been deleted
        buf->resize_mem(0);
        return eBDB_KeyEmpty;
    case DB_BUFFER_SMALL:
        buf->resize_mem(m_DBT_Data->size);
        return this->ReadCursor(dbc, DB_CURRENT, buf);
        break;       
    } // switch

    BDB_CHECK(ret, FileName().c_str());
    buf->resize(m_DBT_Data->size);
    x_EndRead();

    return eBDB_Ok;
}

EBDB_ErrCode CBDB_File::ReadCursor(DBC*         dbc, 
                                   unsigned int bdb_flag,
                                   void**       buf, 
                                   size_t       buf_size, 
                                   EReallocMode allow_realloc)
{
    x_StartRead();
    m_DBT_Data->data = buf ? *buf : 0;
    m_DBT_Data->ulen = (unsigned)buf_size;
    m_DBT_Data->size = 0;

    if (allow_realloc == eReallocForbidden) {
        m_DBT_Data->flags = DB_DBT_USERMEM;
    } else {
        if (m_DBT_Data->data == 0) {
            m_DBT_Data->flags = DB_DBT_MALLOC;
        } else {
            m_DBT_Data->flags = DB_DBT_REALLOC;
        }
    }

    int ret = dbc->c_get(dbc,
                         m_DBT_Key,
                         m_DBT_Data,
                         bdb_flag);

    if ( buf )
        *buf = m_DBT_Data->data;

    switch (ret) {
    case DB_NOTFOUND:
        return eBDB_NotFound;
    case DB_KEYEMPTY:
        // record has been deleted
        return eBDB_KeyEmpty;
    }

    BDB_CHECK(ret, FileName().c_str());
    x_EndRead();
    return eBDB_Ok;
}

EBDB_ErrCode CBDB_File::ReadCursor(DBC*         dbc, 
                                   unsigned int bdb_flag,
                                   CBDB_MultiRowBuffer*  multirow_buf,
                                   bool                  multirow_only)
{
	int ret;
    if (multirow_buf == 0) {
        return ReadCursor(dbc, bdb_flag);
    }

    // Something sits in the memory buffer already, get the next record
    //
    if (multirow_buf->m_BufPtr != 0) {
        DB_MULTIPLE_KEY_NEXT(
            multirow_buf->m_BufPtr,
            multirow_buf->m_Data_DBT,
            multirow_buf->m_LastKey,  multirow_buf->m_LastKeyLen,
            multirow_buf->m_LastData, multirow_buf->m_LastDataLen);

        if (multirow_buf->m_BufPtr != 0) {
            goto read_epilog;
        }
        if (multirow_only) {
            return eBDB_MultiRowEnd;
        }
    } else {
        if (multirow_only) {
            return eBDB_MultiRowEnd;
        }
    }


    // read prolog actions
    m_KeyBuf->Pack();
    m_KeyBuf->PrepareDBT_ForRead(m_DBT_Key);

    multirow_buf->InitDBT();

    // Cursor read
    ret = dbc->c_get(dbc,
                     m_DBT_Key,
                     multirow_buf->m_Data_DBT,
                     bdb_flag | DB_MULTIPLE_KEY);
    switch (ret) {
    case DB_NOTFOUND:
        return eBDB_NotFound;
    case DB_KEYEMPTY:
        // record has been deleted
        return eBDB_KeyEmpty;
    }

    BDB_CHECK(ret, FileName().c_str());


    // Get the first record out of the fetching buffer
    //
    multirow_buf->MultipleInit();

    DB_MULTIPLE_KEY_NEXT(
            multirow_buf->m_BufPtr,
            multirow_buf->m_Data_DBT,
            multirow_buf->m_LastKey,  multirow_buf->m_LastKeyLen,
            multirow_buf->m_LastData, multirow_buf->m_LastDataLen);
    if (multirow_buf->m_BufPtr == 0) {
        return eBDB_NotFound;
    }

    // Read epilog (copy things into field buffer)
    //
read_epilog:
    m_KeyBuf->CopyPackedFrom(multirow_buf->m_LastKey,  
                             multirow_buf->m_LastKeyLen);
    if ( m_DataBuf.get() ) {
        m_DataBuf->CopyPackedFrom(multirow_buf->m_LastData,  
                                  multirow_buf->m_LastDataLen);
    }
    return eBDB_Ok;
}



EBDB_ErrCode CBDB_File::WriteCursor(DBC* dbc, unsigned int bdb_flag, 
                                    EAfterWrite write_flag)
{
    CheckNullDataConstraint();
    return x_Write(bdb_flag, write_flag, dbc);
}

EBDB_ErrCode CBDB_File::WriteCursor(const void* data, 
                                    size_t      size, 
                                    DBC* dbc, 
                                    unsigned int bdb_flag, 
                                    EAfterWrite write_flag)
{
    if (!m_DataBufDisabled) {
        BDB_THROW(eInvalidOperation, "BLOB operation on non BLOB table");
    }

    m_DBT_Data->data = const_cast<void*> (data);
    m_DBT_Data->size = m_DBT_Data->ulen = (unsigned)size;

    return x_Write(bdb_flag, write_flag, dbc);
}


EBDB_ErrCode CBDB_File::DeleteCursor(DBC* dbc, EIgnoreError on_error)
{
    int ret = dbc->c_del(dbc,0);

    if (on_error != CBDB_File::eIgnoreError) {
        BDB_CHECK(ret, FileName().c_str());
    }

    return eBDB_Ok;
}

void CBDB_File::x_CheckConstructBuffers()
{
    if (!m_BufsAttached  &&  !m_BufsCreated) {
        if (m_KeyBuf->FieldCount() == 0) {
            BDB_THROW(eInvalidValue, "Empty BDB key (no fields defined).");
        }
        
        m_KeyBuf->Construct();
        if ( m_DataBuf.get() ) {
            m_DataBuf->Construct();
            m_DataBuf->SetAllNull();
        }
        m_BufsCreated = 1;
    }
}

void CBDB_File::x_StartRead()
{
    m_KeyBuf->Pack();
    m_KeyBuf->PrepareDBT_ForRead(m_DBT_Key);

    if (!m_DataBufDisabled) {
        if ( m_DataBuf.get() ) {
            m_DataBuf->PrepareDBT_ForRead(m_DBT_Data);
        }
        else {
            m_DBT_Data->size  = 0;
            m_DBT_Data->flags = 0;
            m_DBT_Data->data  = 0;
        }
    }
}


void CBDB_File::x_EndRead()
{
    m_KeyBuf->SetDBT_Size(m_DBT_Key->size);
    m_KeyBuf->ArrangePtrsPacked();
    if ( m_DataBuf.get() ) {
        m_DataBuf->SetDBT_Size(m_DBT_Data->size);
        m_DataBuf->ArrangePtrsPacked();
    }
}


EBDB_ErrCode CBDB_File::x_Write(unsigned int flags, 
                                EAfterWrite write_flag, 
                                DBC * dbc)
{
    m_KeyBuf->PrepareDBT_ForWrite(m_DBT_Key);

    if (!m_DataBufDisabled) {
        if ( m_DataBuf.get() ) {
            m_DataBuf->PrepareDBT_ForWrite(m_DBT_Data);
        }
    }

    int ret=0;
    if(dbc) {
        ret = dbc->c_put(dbc,
                         m_DBT_Key,
                         m_DBT_Data,
                         flags
            );
        
    } else {
        DB_TXN* txn = GetTxn();
    
        ret = m_DB->put(m_DB,
                        txn,
                        m_DBT_Key,
                        m_DBT_Data,
                        flags
            );
    }
    
    if (ret == DB_KEYEXIST)
        return eBDB_KeyDup;

    BDB_CHECK(ret, FileName().c_str());

    if (write_flag == eDiscardData) {
        Discard();
    }

    return eBDB_Ok;
}



/////////////////////////////////////////////////////////////////////////////
//
//  CBDB_IdFile::
//

CBDB_IdFile::CBDB_IdFile() 
: CBDB_File()
{
    BindKey("id", &IdKey);
}

void CBDB_IdFile::SetCmp(DB* /* db */)
{
    BDB_CompareFunction func = BDB_Int4Compare;
    if (IsByteSwapped()) {
        func = BDB_ByteSwap_Int4Compare;
    }

    _ASSERT(func);
    int ret = m_DB->set_bt_compare(m_DB, func);
    BDB_CHECK(ret, 0);
}


END_NCBI_SCOPE
