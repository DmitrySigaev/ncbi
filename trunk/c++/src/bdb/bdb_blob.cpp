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
 * File Description:  BDB libarary BLOB implementations.
 *
 */

#include <bdb/bdb_blob.hpp>

BEGIN_NCBI_SCOPE


/////////////////////////////////////////////////////////////////////////////
//  CBDB_BLobFile::
//

CBDB_BLobFile::CBDB_BLobFile() 
{ 
    DisableDataBufProcessing(); 
}


EBDB_ErrCode CBDB_BLobFile::Fetch()
{
    return Fetch(0, 0, eReallocForbidden);
}

EBDB_ErrCode CBDB_BLobFile::Fetch(void**       buf, 
                                  size_t       buf_size, 
                                  EReallocMode allow_realloc)
{
    m_DBT_Data.data = buf ? *buf : 0;
    m_DBT_Data.ulen = buf_size;
    m_DBT_Data.size = 0;

    if (allow_realloc == eReallocForbidden) {
        m_DBT_Data.flags = DB_DBT_USERMEM;
    } else {
        if (m_DBT_Data.data == 0) {
            m_DBT_Data.flags = DB_DBT_MALLOC;
        } else {
            m_DBT_Data.flags = DB_DBT_REALLOC;
        }
    }

    EBDB_ErrCode ret;
    try {

        ret = CBDB_File::Fetch();

        if ( buf )
            *buf = m_DBT_Data.data;

    } catch(...) {
        ::memset(&m_DBT_Data, 0, sizeof(m_DBT_Data));
        throw;
    }


    if (ret == eBDB_NotFound)
        return eBDB_NotFound;

    return eBDB_Ok;
}


EBDB_ErrCode CBDB_BLobFile::GetData(void* buf, size_t size)
{
    return Fetch(&buf, size, eReallocForbidden);
}


EBDB_ErrCode CBDB_BLobFile::Insert(const void* data, size_t size)
{
    m_DBT_Data.data = const_cast<void*> (data);
    m_DBT_Data.size = m_DBT_Data.ulen = size;

    EBDB_ErrCode ret = CBDB_File::Insert();
    return ret;
}



/////////////////////////////////////////////////////////////////////////////
//  CBDB_LobFile::
//


CBDB_LobFile::CBDB_LobFile()
 : m_LobKey(0)
{
    m_DBT_Key.data = &m_LobKey;
    m_DBT_Key.size = sizeof(m_LobKey);
    m_DBT_Key.ulen = sizeof(m_LobKey);
    m_DBT_Key.flags = DB_DBT_USERMEM;
}


void CBDB_LobFile::SetCmp(DB* db)
{
    int ret = db->set_bt_compare(db, bdb_uint_cmp);
    BDB_CHECK(ret, 0);
}


EBDB_ErrCode CBDB_LobFile::Insert(unsigned int lob_id,
                                  const void*  data,
                                  size_t       size)
{
    _ASSERT(lob_id);
    _ASSERT(size);
    _ASSERT(m_DB);

    // paranoia check
    _ASSERT(m_DBT_Key.data == &m_LobKey);
    _ASSERT(m_DBT_Key.size == sizeof(m_LobKey));

    m_LobKey = lob_id;

    m_DBT_Data.data = const_cast<void*> (data);
    m_DBT_Data.size = m_DBT_Data.ulen = size;

    int ret = m_DB->put(m_DB,
                        0,     // DB_TXN*
                        &m_DBT_Key,
                        &m_DBT_Data,
                        DB_NOOVERWRITE | DB_NODUPDATA
                        );
    if (ret == DB_KEYEXIST)
        return eBDB_KeyDup;
    BDB_CHECK(ret, FileName().c_str());
    return eBDB_Ok;
}



EBDB_ErrCode CBDB_LobFile::Fetch(unsigned int lob_id,
                                 void**       buf,
                                 size_t       buf_size,
                                 EReallocMode allow_realloc)
{
    _ASSERT(lob_id);
    _ASSERT(m_DB);

    // paranoia check
    _ASSERT(m_DBT_Key.data  == &m_LobKey);
    _ASSERT(m_DBT_Key.size  == sizeof(m_LobKey));
    _ASSERT(m_DBT_Key.ulen  == sizeof(m_LobKey));
    _ASSERT(m_DBT_Key.flags == DB_DBT_USERMEM);

    m_LobKey = lob_id;

    // Here we attempt to read only key value and get information
    // about LOB size. In this case get operation fails with ENOMEM
    // error message (ignored)

    m_DBT_Data.data = buf ? *buf : 0;
    m_DBT_Data.ulen = buf_size;
    m_DBT_Data.size = 0;

    if (m_DBT_Data.data == 0  &&  m_DBT_Data.ulen != 0) {
        _ASSERT(0);
    }

    if (allow_realloc == eReallocForbidden) {
        m_DBT_Data.flags = DB_DBT_USERMEM;
    } else {
        if (m_DBT_Data.data == 0) {
            m_DBT_Data.flags = DB_DBT_MALLOC;
        } else {
            m_DBT_Data.flags = DB_DBT_REALLOC;
        }
    }

    int ret = m_DB->get(m_DB,
                        0,          // DB_TXN*
                        &m_DBT_Key,
                        &m_DBT_Data,
                        0
                        );

    if (ret == DB_NOTFOUND)
        return eBDB_NotFound;

    if (ret == ENOMEM) {
        if (m_DBT_Data.data == 0)
            return eBDB_Ok;  // to be retrieved later using GetData()
    }

    BDB_CHECK(ret, FileName().c_str());

    if ( buf )
        *buf = m_DBT_Data.data;

    return eBDB_Ok;
}



EBDB_ErrCode CBDB_LobFile::GetData(void* buf, size_t size)
{
    _ASSERT(m_LobKey);
    _ASSERT(m_DB);
    _ASSERT(size >= m_DBT_Data.size);
    _ASSERT(m_DBT_Data.size);

    // paranoia check
    _ASSERT(m_DBT_Key.data == &m_LobKey);
    _ASSERT(m_DBT_Key.size == sizeof(m_LobKey));
    _ASSERT(m_DBT_Key.ulen == sizeof(m_LobKey));
    _ASSERT(m_DBT_Key.flags == DB_DBT_USERMEM);

    m_DBT_Data.data  = buf;
    m_DBT_Data.ulen  = size;
    m_DBT_Data.flags = DB_DBT_USERMEM;

    int ret = m_DB->get(m_DB,
                        0,          // DB_TXN*
                        &m_DBT_Key,
                        &m_DBT_Data,
                        0
                        );

    if (ret == DB_NOTFOUND)
        return eBDB_NotFound;

    BDB_CHECK(ret, FileName().c_str());
    return eBDB_Ok;
}



END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.5  2003/05/08 13:43:40  kuznets
 * Bug fix.
 *
 * Revision 1.4  2003/05/05 20:15:32  kuznets
 * Added CBDB_BLobFile
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
