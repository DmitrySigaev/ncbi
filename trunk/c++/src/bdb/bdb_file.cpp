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

#include <bdb/bdb_file.hpp>

BEGIN_NCBI_SCOPE

const char CBDB_RawFile::kDefaultDatabase[] = "_table";
const int  CBDB_RawFile::kOpenFileMask      = 0664;

extern "C"
{


int bdb_uint_cmp(DB*, const DBT* val1, const DBT* val2)
{
    unsigned int v1, v2;
    ::memcpy(&v1, val1->data, sizeof(unsigned));
    ::memcpy(&v2, val2->data, sizeof(unsigned));
    return (v1 < v2) ? -1
                     : ((v2 < v1) ? 1 : 0);
}


int bdb_int_cmp(DB*, const DBT* val1, const DBT* val2)
{
    int v1, v2;
    ::memcpy(&v1, val1->data, sizeof(int));
    ::memcpy(&v2, val2->data, sizeof(int));
    return (v1 < v2) ? -1
                     : ((v2 < v1) ? 1 : 0);
}


int bdb_cmp(DB* db, const DBT* val1, const DBT* val2)
{
    const CBDB_BufferManager* fbuf1 =
          static_cast<const CBDB_BufferManager*> (db->app_private);

    _ASSERT(fbuf1);

    const char* p1 = static_cast<const char*> (val1->data);
    const char* p2 = static_cast<const char*> (val2->data);

    for (unsigned int i = 0;  i < fbuf1->FieldCount();  ++i) {
        const CBDB_Field& fld1 = fbuf1->GetField(i);
        int ret = fld1.Compare(p1, p2);
        if ( ret )
            return ret;

        p1 += fld1.GetDataLength(p1);
        p2 += fld1.GetDataLength(p2);
    }

    return 0;
}


} // extern "C"



/////////////////////////////////////////////////////////////////////////////
//  CBDB_RawFile::
//



CBDB_RawFile::CBDB_RawFile()
: m_DB(0),
  m_PageSize(0),
  m_CacheSize(256 * 1024)
{
    ::memset(&m_DBT_Key,  0, sizeof(m_DBT_Key));
    ::memset(&m_DBT_Data, 0, sizeof(m_DBT_Data));
}


CBDB_RawFile::~CBDB_RawFile()
{
    x_Close(eIgnoreError);
}


void CBDB_RawFile::Close()
{
    x_Close(eThrowOnError);
}


void CBDB_RawFile::x_Close(ECloseMode close_mode)
{
    if ( m_FileName.empty() )
        return;

    if (m_DB) {
        int ret = m_DB->close(m_DB, 0);
        m_DB = 0;
        if (close_mode == eThrowOnError) {
            BDB_CHECK(ret, m_FileName.c_str());
        }
    }

    m_FileName.erase();
    m_Database.erase();
}


void CBDB_RawFile::Open(const char* filename,
                        const char* database,
                        EOpenMode   open_mode)
{
    if ( !m_FileName.empty() )
        Close();

    if (!database  ||  !*database)
        database = kDefaultDatabase;

    x_Open(filename, database, open_mode);

    m_FileName = filename;
    m_Database = database;
}


void CBDB_RawFile::Reopen(EOpenMode open_mode)
{
    _ASSERT(!m_FileName.empty());
    int ret = m_DB->close(m_DB, 0);
    m_DB = 0;
    BDB_CHECK(ret, m_FileName.c_str());
    x_Open(m_FileName.c_str(),
           !m_Database.empty() ? m_Database.c_str() : 0,
           open_mode);
}


void CBDB_RawFile::Remove(const char* filename, const char* database)
{
    if (!database  ||  !*database)
        database = kDefaultDatabase;

    // temporary DB is used here, because BDB remove call invalidates the
    // DB argument redardless of the result.
    DB* db = 0;
	CDB_guard guard(&db);
    int ret = db_create(&db, 0, 0);
    BDB_CHECK(ret, 0);

    ret = db->remove(db, filename, database, 0);
    guard.release();
    if (ret == ENOENT)
        return;  // Non existent table cannot be removed

    BDB_CHECK(ret, filename);
}


unsigned int CBDB_RawFile::Truncate()
{
    _ASSERT(m_DB != 0);
    u_int32_t count;
    int ret = m_DB->truncate(m_DB,
                             0,       // DB_TXN*
                             &count,
                             0);

    BDB_CHECK(ret, FileName().c_str());
    return count;
}


void CBDB_RawFile::x_CreateDB()
{
    _ASSERT(m_DB == 0);

	CDB_guard guard(&m_DB);

    int ret = db_create(&m_DB, 0, 0);
    BDB_CHECK(ret, 0);

    SetCmp(m_DB);

    if ( m_PageSize ) {
        ret = m_DB->set_pagesize(m_DB, m_PageSize);
        BDB_CHECK(ret, 0);
    }

    ret = m_DB->set_cachesize(m_DB, 0, m_CacheSize, 1);
    BDB_CHECK(ret, 0);
    guard.release();
}


void CBDB_RawFile::x_Open(const char* filename,
                                 const char* database,
                                 EOpenMode   open_mode)
{
    if (m_DB == 0) {
        x_CreateDB();
    }

    if (open_mode == eCreate) {
        Remove(filename, database);
        x_Create(filename, database);
    }
    else {
        int ret = m_DB->open(m_DB,
                             0,                    // DB_TXN*
                             filename,
                             database,             // database name
                             DB_BTREE,
                             (u_int32_t) open_mode,
                             kOpenFileMask
                             );
        if ( ret ) {
            if (open_mode == eReadWrite) {
                x_Create(filename, database);
            }
            else {
                m_DB->close(m_DB, 0);
                m_DB = 0;
                BDB_CHECK(ret, filename);
            }
        }
    } // else open_mode == Create
}


void CBDB_RawFile::SetPageSize(unsigned int page_size)
{
    _ASSERT(m_DB == 0); // we can set page size only before opening the file
    if (((page_size - 1) & page_size) != 0) {
        BDB_THROW(eInvalidValue, "Page size must be power of 2");
    }
    m_PageSize = page_size;
}


void CBDB_RawFile::x_Create(const char* filename, const char* database)
{
    int ret = m_DB->open(m_DB,
                         0,               // DB_TXN*
                         filename,
                         database,        // database name
                         DB_BTREE,
                         DB_CREATE,
                         kOpenFileMask);
    if ( ret ) {
        m_DB->close(m_DB, 0);
        m_DB = 0;
        BDB_CHECK(ret, filename);
    }
}


/////////////////////////////////////////////////////////////////////////////
//
//  CBDB_File::
//


CBDB_File::CBDB_File()
    : CBDB_RawFile(),
      m_KeyBuf(new CBDB_BufferManager),
      m_BufsAttached(false),
      m_BufsCreated(false)
{
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
        auto_ptr<CBDB_BufferManager> dbuf(new CBDB_BufferManager);
        m_DataBuf = dbuf;
        m_DataBuf->SetNullable();
    }

    m_DataBuf->Bind(data_field);
    if ( buf_size )
        data_field->SetBufferSize(buf_size);
    if (is_nullable == eNullable)
        data_field->SetNullable();
}


void CBDB_File::Open(const char* filename,
                     const char* database,
                     EOpenMode   open_mode)
{
    if ( IsOpen() )
        Close();

    CBDB_RawFile::Open(filename, database, open_mode);

    if (!m_BufsAttached  &&  !m_BufsCreated) {
        m_KeyBuf->Construct();
        if ( m_DataBuf.get() ) {
			m_DataBuf->Construct();
            m_DataBuf->SetAllNull();
		}
        m_BufsCreated = 1;
    }
    m_DB->app_private = (void*) m_KeyBuf.get();

}


void CBDB_File::Reopen(EOpenMode open_mode)
{
    CBDB_RawFile::Reopen(open_mode);
    m_DB->app_private = (void*) m_KeyBuf.get();
    if ( m_DataBuf.get() ) {
        m_DataBuf->SetAllNull();
    }
}


EBDB_ErrCode CBDB_File::Fetch()
{
    x_StartRead();

    int ret = m_DB->get(m_DB,
                        0,     // DB_TXN*
                        &m_DBT_Key,
                        &m_DBT_Data,
                        0
                        );
    if (ret == DB_NOTFOUND)
        return eBDB_NotFound;
    BDB_CHECK(ret, FileName().c_str());

    x_EndRead();
    return eBDB_Ok;
}


EBDB_ErrCode CBDB_File::Insert(EAfterWrite write_flag)
{
    CheckNullDataConstraint();
    return x_Write(DB_NOOVERWRITE, write_flag);
}


EBDB_ErrCode CBDB_File::Delete()
{
    m_KeyBuf->PrepareDBT_ForWrite(&m_DBT_Key);
    int ret = m_DB->del(m_DB,
                        0,           // DB_TXN*
                        &m_DBT_Key,
                        0);
    BDB_CHECK(ret, FileName().c_str());
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


void CBDB_File::SetCmp(DB* db)
{
    int ret = db->set_bt_compare(db, bdb_cmp);
    BDB_CHECK(ret, 0);
}


DBC* CBDB_File::CreateCursor() const
{
    DBC* cursor;
    int ret = m_DB->cursor(m_DB,
                           0,        // DB_TXN*
                           &cursor,
                           0);
    BDB_CHECK(ret, FileName().c_str());
    return cursor;
}


EBDB_ErrCode CBDB_File::ReadCursor(DBC* dbc, unsigned int bdb_flag)
{
    x_StartRead();

    int ret = dbc->c_get(dbc,
                         &m_DBT_Key,
                         &m_DBT_Data,
                         bdb_flag);

    if (ret == DB_NOTFOUND)
        return eBDB_NotFound;

    BDB_CHECK(ret, FileName().c_str());
    x_EndRead();
    return eBDB_Ok;
}


void CBDB_File::x_StartRead()
{
    m_KeyBuf->Pack();
    m_KeyBuf->PrepareDBT_ForRead(&m_DBT_Key);
    if ( m_DataBuf.get() ) {
        m_DataBuf->PrepareDBT_ForRead(&m_DBT_Data);
    }
    else {
        m_DBT_Data.size  = 0;
        m_DBT_Data.flags = 0;
        m_DBT_Data.data  = 0;
    }
}


void CBDB_File::x_EndRead()
{
    m_KeyBuf->ArrangePtrsPacked();
    if ( m_DataBuf.get() )
        m_DataBuf->ArrangePtrsPacked();
}


EBDB_ErrCode CBDB_File::x_Write(unsigned int flags, EAfterWrite write_flag)
{
    m_KeyBuf->PrepareDBT_ForWrite(&m_DBT_Key);

    if ( m_DataBuf.get() ) {
        m_DataBuf->PrepareDBT_ForWrite(&m_DBT_Data);
    }

    int ret = m_DB->put(m_DB,
                        0,     // DB_TXN*
                        &m_DBT_Key,
                        &m_DBT_Data,
                        flags
                        );
    if (ret == DB_KEYEXIST)
        return eBDB_KeyDup;

    BDB_CHECK(ret, FileName().c_str());

    if (write_flag == eDiscardData) {
        Discard();
    }

    return eBDB_Ok;
}



END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.4  2003/04/30 20:25:42  kuznets
 * Bug fix
 *
 * Revision 1.3  2003/04/29 19:07:22  kuznets
 * Cosmetics..
 *
 * Revision 1.2  2003/04/28 14:51:55  kuznets
 * #include directives changed to conform the NCBI policy
 *
 * Revision 1.1  2003/04/24 16:34:30  kuznets
 * Initial revision
 *
 * ===========================================================================
 */
