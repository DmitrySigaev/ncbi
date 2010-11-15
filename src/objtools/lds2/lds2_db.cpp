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
 * Author: Aleksey Grichenko
 *
 * File Description:  LDS v.2 database access.
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbifile.hpp>
#include <db/sqlite/sqlitewrapp.hpp>
#include <objtools/error_codes.hpp>
#include <objtools/lds2/lds2_expt.hpp>
#include <objtools/lds2/lds2_db.hpp>


#define NCBI_USE_ERRCODE_X Objtools_LDS2

BEGIN_NCBI_SCOPE

BEGIN_SCOPE(objects)


CLDS2_Database::CLDS2_Database(const string& db_file)
    : m_DbFile(db_file),
      m_DbFlags(CSQLITE_Connection::eDefaultFlags)
{
}


CLDS2_Database::~CLDS2_Database(void)
{
}


// Create db queries
// NOTE: The version of sqlite3 used by the toolkit does not support
// foreign keys. For this reason triggers are used to cascade
// deletions.
const char* kLDS2_CreateDB[] = {
    // files
    "create table `file` (" \
    "`file_id` integer primary key on conflict abort autoincrement," \
    "`file_name` varchar(2048) not null unique on conflict abort," \
    "`file_format` integer not null," \
    "`file_handler` varchar(100)," \
    "`file_size` integer(8)," \
    "`file_time` integer(8)," \
    "`file_crc` integer(4));",

    // chunks
    "create table `chunk` (" \
    "`file_id` integer(8) not null," \
    "`raw_pos` integer(8) not null," \
    "`stream_pos` integer(8) not null," \
    "`data_size` inteter not null," \
    "`data` blob default null);",

    // seq-id vs lds-id
    "create table `seq_id` (" \
    "`lds_id` integer primary key on conflict abort autoincrement," \
    "`txt_id` varchar(100) not null," \
    "`int_id` integer(8) default null);",

    // bioseqs by blob
    "create table `bioseq` (" \
    "`bioseq_id` integer primary key on conflict abort autoincrement," \
    "`blob_id` integer(8) not null);",

    // bioseqs by lds_id
    "create table `bioseq_id` (" \
    "`bioseq_id` integer(8) not null," \
    "`lds_id` integer(8) not null);",

    // blobs
    "create table `blob` (" \
    "`blob_id` integer primary key on conflict abort autoincrement," \
    "`blob_type` integer not null," \
    "`file_id` integer(8) not null," \
    "`file_pos` integer(8));",

    // annotations
    "create table `annot` (" \
    "`annot_id` integer primary key on conflict abort autoincrement," \
    "`annot_type` integer not null," \
    "`blob_id` integer(8) not null);",

    // annotations by lds_id
    "create table `annot_id` (" \
    "`annot_id` integer(8) not null," \
    "`lds_id` integer(8) not null," \
    "`external` boolean);",

    // delete files - cascade to blobls and chunks
    "create trigger `delete_file` before delete on `file` begin " \
    "delete from `blob` where `blob`.`file_id`=`old`.`file_id`;" \
    "delete from `chunk` where `chunk`.`file_id`=`old`.`file_id`;" \
    "end;",
    // delete bioseqs - cascade to bioseq_id
    "create trigger `delete_bioseq` before delete on `bioseq` begin " \
    "delete from `bioseq_id` where `bioseq_id`.`bioseq_id`=`old`.`bioseq_id`;" \
    "end;",
    // delete blobs - cascade to bioseq and annot
    "create trigger `delete_blob` before delete on `blob` begin " \
    "delete from `bioseq` where `bioseq`.`blob_id`=`old`.`blob_id`;" \
    "delete from `annot` where `annot`.`blob_id`=`old`.`blob_id`;" \
    "end;",
    // delete annotations - cascade to annot_id
    "create trigger `delete_annot` before delete on `annot` begin " \
    "delete from `annot_id` where `annot_id`.`annot_id`=`old`.`annot_id`;" \
    "end;"
};


// Create the indexes.
const char* kLDS2_CreateDBIdx[] = {
    // files
    // index files by name, prevents duplicate names
    "create unique index `idx_filename` on `file` (`file_name`);",

    // chunks
    // index by stream_pos
    "create index `idx_stream_pos` on `chunk` (`stream_pos`);",

    // seq-id vs lds-id
    // index for reverse lookup, prevents duplicate ids
    "create unique index `idx_int_id` on `seq_id` (`int_id`);",
    "create unique index `idx_txt_id` on `seq_id` (`txt_id`);",

    // bioseqs by blob
    // reverse index
    "create index `idx_blob_id` on `bioseq` (`blob_id`);",

    // bioseqs by lds_id
    // index by bioseq_id
    "create index `idx_bioseq_id` on `bioseq_id` (`bioseq_id`);",
    // index by lds_id
    "create index `idx_bioseq_lds_id` on `bioseq_id` (`lds_id`);",

    // blobs
    // index by file_id
    "create index `idx_blob_file_id` on `blob` (`file_id`);",

    // annotations
    // index by blob_id
    "create index `idx_annot_blob_id` on `annot` (`blob_id`);",

    // annotations by lds_id
    // index by annot_id
    "create index `idx_annot_id` on `annot_id` (`annot_id`);",
    // index by lds_id
    "create index `idx_annot_lds_id` on `annot_id` (`lds_id`);",
    // index 'external' column
    "create index `idx_external` on `annot_id` (`external`);"
};


// Drop the indexes.
const char* kLDS2_DropDBIdx[] = {
    "drop index if exists `idx_filename`;",
    "drop index if exists `idx_stream_pos`;",
    "drop index if exists `idx_int_id`;",
    "drop index if exists `idx_txt_id`;",
    "drop index if exists `idx_blob_id`;",
    "drop index if exists `idx_bioseq_id`;",
    "drop index if exists `idx_bioseq_lds_id`;",
    "drop index if exists `idx_blob_file_id`;",
    "drop index if exists `idx_annot_blob_id`;",
    "drop index if exists `idx_annot_id`;",
    "drop index if exists `idx_annot_lds_id`;",
    "drop index if exists `idx_external`;"
};


static const char* s_LDS2_SQL[] = {
    // eSt_GetFileNames
    "select `file_name` from `file`;",
    // eSt_GetFileInfoByName
    "select `file_id`, `file_format`, `file_handler`, `file_size`, "
    "`file_time`, `file_crc` from `file` where `file_name`=?1;",
    // eSt_GetFileInfoById
    "select `file_name`, `file_format`, `file_handler`, `file_size`, "
    "`file_time`, `file_crc` from `file` where `file_id`=?1;",
    // eSt_GetLdsSeqIdForIntId
    "select `lds_id` from `seq_id` where `int_id`=?1;",
    // eSt_GetLdsSeqIdForTxtId
    "select `lds_id` from `seq_id` where `txt_id`=?1;",
    // eSt_GetBioseqIdForIntId
    "select `bioseq_id`.`bioseq_id` from `seq_id` "
    "inner join `bioseq_id` using(`lds_id`) where `seq_id`.`int_id`=?1;",
    // eSt_GetBioseqIdForTxtId
    "select `bioseq_id`.`bioseq_id` from `seq_id` "
    "inner join `bioseq_id` using(`lds_id`) where `seq_id`.`txt_id`=?1;",
    // eSt_GetSynonyms
    "select distinct `lds_id` from `bioseq_id` where `bioseq_id`=?1;",
    // eSt_GetBlobInfo
    "select distinct `blob_id`, `blob_type`, `file_id`, `file_pos` "
    "from `blob` where `blob_id`=?1;",
    // eSt_GetBioseqForIntId
    "select distinct `blob_id`, `blob_type`, `file_id`, `file_pos` "
    "from `blob` inner join `bioseq` using(`blob_id`) "
    "inner join `bioseq_id` using(`bioseq_id`) "
    "inner join `seq_id` using(`lds_id`) where `seq_id`.`int_id`=?1;",
    // eSt_GetBioseqForTxtId
    "select distinct `blob_id`, `blob_type`, `file_id`, `file_pos` "
    "from `blob` inner join `bioseq` using(`blob_id`) "
    "inner join `bioseq_id` using(`bioseq_id`) "
    "inner join `seq_id` using(`lds_id`) where `seq_id`.`txt_id`=?1;",
    // eSt_GetAnnotBlobsByIntId
    "select distinct `blob_id`, `blob_type`, `file_id`, `file_pos` "
    "from `blob` inner join `annot` using(`blob_id`) "
    "inner join `annot_id` using(`annot_id`) "
    "inner join `seq_id` using(`lds_id`) where "
    "`annot_id`.`external`=?2 and `seq_id`.`int_id`=?1;",
    // eSt_GetAnnotBlobsAllByIntId
    "select distinct `blob_id`, `blob_type`, `file_id`, `file_pos` "
    "from `blob` inner join `annot` using(`blob_id`) "
    "inner join `annot_id` using(`annot_id`) "
    "inner join `seq_id` using(`lds_id`) where "
    "`seq_id`.`int_id`=?1;",
    // eSt_GetAnnotBlobsByTxtId
    "select distinct `blob_id`, `blob_type`, `file_id`, `file_pos` "
    "from `blob` inner join `annot` using(`blob_id`) "
    "inner join `annot_id` using(`annot_id`) "
    "inner join `seq_id` using(`lds_id`) where "
    "`annot_id`.`external`=?2 and `seq_id`.`txt_id`=?1;",
    // eSt_GetAnnotBlobsAllByTxtId
    "select distinct `blob_id`, `blob_type`, `file_id`, `file_pos` "
    "from `blob` inner join `annot` using(`blob_id`) "
    "inner join `annot_id` using(`annot_id`) "
    "inner join `seq_id` using(`lds_id`) where "
    "`seq_id`.`txt_id`=?1;",

    // eSt_AddFile
    "insert into `file` "
    "(`file_name`, `file_format`, `file_handler`, `file_size`, "
    "`file_time`, `file_crc`) values (?1, ?2, ?3, ?4, ?5, ?6);",
    // eSt_AddLdsId
    "insert into `seq_id` (`txt_id`, `int_id`) values (?1, ?2);",
    // eSt_AddBlob
    "insert into `blob` (`blob_type`, `file_id`, `file_pos`) "
    "values (?1, ?2, ?3);",
    // eSt_AddBioseqToBlob
    "insert into `bioseq` (`blob_id`) values (?1);",
    // eSt_AddBioseqIds
    "insert into `bioseq_id` (`bioseq_id`, `lds_id`) values (?1, ?2);",
    // eSt_AddAnnotToBlob
    "insert into `annot` (`annot_type`, `blob_id`) values (?1, ?2);",
    // eSt_AddAnnotIds
    "insert into `annot_id` (`annot_id`, `lds_id`, `external`) "
    "values (?1, ?2, ?3);",
    // eSt_DeleteFileByName
    "delete from `file` where `file_name`=?1;",
    // eSt_DeleteFileById
    "delete from `file` where `file_id`=?1;",

    // eSt_AddChunk
    "insert into `chunk` "
    "(`file_id`, `raw_pos`, `stream_pos`, `data_size`, `data`) "
    "values (?1, ?2, ?3, ?4, ?5);",
    // eSt_FindChunk
    "select `raw_pos`, `stream_pos`, `data_size`, `data` from `chunk` "
    "where `file_id`=?1 and `stream_pos`<=?2 order by `stream_pos` desc "
    "limit 1;"
};


void CLDS2_Database::sx_DbConn_Cleanup(SLDS2_DbConnection* value,
                                       void* /*cleanup_data*/)
{
    delete value;
}


CLDS2_Database::SLDS2_DbConnection::SLDS2_DbConnection(void)
    : Statements(CLDS2_Database::eSt_StatementsCount)
{
}


CLDS2_Database::SLDS2_DbConnection&
CLDS2_Database::x_GetDbConnection(void) const
{
    if ( !m_DbConn ) {
        m_DbConn.Reset(new TDbConnectionsTls);
    }
    SLDS2_DbConnection* db_conn = m_DbConn->GetValue();
    if ( !db_conn ) {
        auto_ptr<SLDS2_DbConnection> conn_ptr(new SLDS2_DbConnection);
        db_conn = conn_ptr.get();
        m_DbConn->SetValue(conn_ptr.release(), sx_DbConn_Cleanup, 0);
    }
    return *db_conn;
}


void CLDS2_Database::x_ResetDbConnection(void)
{
    m_DbConn.Reset();
}


CSQLITE_Connection& CLDS2_Database::x_GetConn(void) const
{
    SLDS2_DbConnection& db_conn = x_GetDbConnection();
    if ( !db_conn.Connection.get() ) {
        if ( m_DbFile.empty() ) {
            LDS2_THROW(eInvalidDbFile, "Empty database file name.");
        }
        db_conn.Connection.reset(new CSQLITE_Connection(m_DbFile,
            CSQLITE_Connection::fExternalMT |
            CSQLITE_Connection::eDefaultFlags |
            CSQLITE_Connection::fVacuumManual |
            CSQLITE_Connection::fJournalMemory |
            CSQLITE_Connection::fSyncOff
            ));
    }
    return *db_conn.Connection;
}


CSQLITE_Statement& CLDS2_Database::x_GetStatement(EStatement st) const
{
    SLDS2_DbConnection& db_conn = x_GetDbConnection();
    _ASSERT((size_t)st < db_conn.Statements.size());
    AutoPtr<CSQLITE_Statement>& ptr = db_conn.Statements[st];
    if ( !ptr.get() ) {
        ptr.reset(new CSQLITE_Statement(&x_GetConn(), s_LDS2_SQL[st]));
    }
    else {
        ptr->Reset();
    }
    return *ptr;
}


void CLDS2_Database::x_ExecuteSqls(const char* sqls[], size_t len)
{
    CSQLITE_Connection& conn = x_GetConn();
    for (size_t i = 0; i < len; i++) {
        conn.ExecuteSql(sqls[i]);
    }
}


void CLDS2_Database::Create(void)
{
    LOG_POST_X(1, Info << "LDS2: Creating database " <<  m_DbFile);

    // Close the connection if any
    x_ResetDbConnection();

    // Delete all data
    CFile dbf(m_DbFile);
    if ( dbf.Exists() ) {
        dbf.Remove();
    }

    // Initialize connection and create tables:
    x_ExecuteSqls(kLDS2_CreateDB,
        sizeof(kLDS2_CreateDB)/sizeof(kLDS2_CreateDB[0]));
}


void CLDS2_Database::SetSQLiteFlags(int flags)
{
    m_DbFlags = flags;
    x_ResetDbConnection();
}


void CLDS2_Database::Open(void)
{
    x_GetConn();
}


void CLDS2_Database::GetFileNames(TStringSet& files) const
{
    CSQLITE_Statement& st = x_GetStatement(eSt_GetFileNames);
    while ( st.Step() ) {
        files.insert(st.GetString(0));
    }
    st.Reset();
}


void CLDS2_Database::AddFile(SLDS2_File& info)
{
    LOG_POST_X(2, Info << "LDS2: Adding file " << info.name);
    CSQLITE_Statement& st = x_GetStatement(eSt_AddFile);
    st.Bind(1, info.name);
    st.Bind(2, info.format);
    st.Bind(3, info.handler);
    st.Bind(4, info.size);
    st.Bind(5, info.time);
    st.Bind(6, info.crc);
    st.Execute();
    info.id = st.GetLastInsertedRowid();
    st.Reset();
}


void CLDS2_Database::UpdateFile(SLDS2_File& info)
{
    LOG_POST_X(3, Info << "LDS2: Updating file " << info.name);
    DeleteFile(info.id);
    AddFile(info);
}


void CLDS2_Database::DeleteFile(const string& file_name)
{
    LOG_POST_X(4, Info << "LDS2: Deleting file " << file_name);
    CSQLITE_Statement& st = x_GetStatement(eSt_DeleteFileByName);
    st.Bind(1, file_name);
    st.Execute();
    st.Reset();
}


void CLDS2_Database::DeleteFile(Int8 file_id)
{
    LOG_POST_X(4, Info << "LDS2: Deleting file " << file_id);
    CSQLITE_Statement& st = x_GetStatement(eSt_DeleteFileById);
    st.Bind(1, file_id);
    st.Execute();
    st.Reset();
}


Int8 CLDS2_Database::x_GetLdsSeqId(const CSeq_id_Handle& id)
{
    CSQLITE_Statement* st = NULL;
    if ( id.IsGi() ) {
        // Try to use integer index
        st = &x_GetStatement(eSt_GetLdsSeqIdForIntId);
        st->Bind(1, id.GetGi());
    }
    else {
        // Use text index
        st = &x_GetStatement(eSt_GetLdsSeqIdForTxtId);
        st->Bind(1, id.AsString());
    }
    if ( st->Step() ) {
        Int8 ret = st->GetInt8(0);
        st->Reset();
        return ret;
    }

    // Id not in the database yet -- add new entry.
    st = &x_GetStatement(eSt_AddLdsSeqId);
    st->Bind(1, id.AsString());
    if ( id.IsGi() ) {
        st->Bind(2, id.GetGi());
    }
    else {
        // HACK: reset GI to null if not available.
        st->Bind(2, (void*)NULL, 0);
    }
    st->Execute();
    Int8 ret = st->GetLastInsertedRowid();
    st->Reset();
    return ret;
}


Int8 CLDS2_Database::AddBlob(Int8                   file_id,
                             SLDS2_Blob::EBlobType  blob_type,
                             Int8                   file_pos)
{
    CSQLITE_Statement& st = x_GetStatement(eSt_AddBlob);
    st.Bind(1, blob_type);
    st.Bind(2, file_id);
    st.Bind(3, file_pos);
    st.Execute();
    Int8 blob_id = st.GetLastInsertedRowid();
    st.Reset();
    return blob_id;
}


typedef vector<Int8> TLdsIds;


Int8 CLDS2_Database::AddBioseq(Int8 blob_id, const TSeqIdSet& ids)
{
    // Convert ids to lds-ids
    TLdsIds lds_ids;
    ITERATE(TSeqIdSet, id, ids) {
        Int8 lds_id = x_GetLdsSeqId(*id);
        lds_ids.push_back(lds_id);
    }

    CSQLITE_Statement& st1 = x_GetStatement(eSt_AddBioseqToBlob);

    // Create new bioseq
    st1.SetSql("insert into `bioseq` (`blob_id`) values (?1);");
    st1.Bind(1, blob_id);
    st1.Execute();
    Int8 bioseq_id = st1.GetLastInsertedRowid();
    st1.Reset();

    // Link bioseq to its seq-ids
    CSQLITE_Statement& st2 = x_GetStatement(eSt_AddBioseqIds);
    ITERATE(TLdsIds, lds_id, lds_ids) {
        st2.Bind(1, bioseq_id);
        st2.Bind(2, *lds_id);
        st2.Execute();
        st2.Reset();
    }

    return bioseq_id;
}


Int8 CLDS2_Database::AddAnnot(Int8                      blob_id,
                              SLDS2_Annot::EAnnotType   annot_type,
                              const TAnnotRefSet&       refs)
{
    // Convert ids to lds-ids
    typedef pair<Int8, bool> TLdsAnnotRef;
    typedef vector<TLdsAnnotRef> TLdsAnnotRefSet;

    TLdsAnnotRefSet lds_refs;
    ITERATE(TAnnotRefSet, ref, refs) {
        Int8 lds_id = x_GetLdsSeqId(ref->first);
        lds_refs.push_back(TLdsAnnotRef(lds_id, ref->second));
    }

    CSQLITE_Statement& st1 = x_GetStatement(eSt_AddAnnotToBlob);

    // Create new bioseq
    st1.Bind(1, annot_type);
    st1.Bind(2, blob_id);
    st1.Execute();
    Int8 annot_id = st1.GetLastInsertedRowid();
    st1.Reset();

    // Link annot to its seq-ids
    CSQLITE_Statement& st2 = x_GetStatement(eSt_AddAnnotIds);
    ITERATE(TLdsAnnotRefSet, ref, lds_refs) {
        st2.Bind(1, annot_id);
        st2.Bind(2, ref->first);
        st2.Bind(3, ref->second);
        st2.Execute();
        st2.Reset();
    }

    return annot_id;
}


Int8 CLDS2_Database::GetBioseqId(const CSeq_id_Handle& idh) const
{
    CSQLITE_Statement* st = NULL;
    if ( idh.IsGi() ) {
        st = &x_GetStatement(eSt_GetBioseqIdForIntId);
        st->Bind(1, idh.GetGi());
    }
    else {
        st = &x_GetStatement(eSt_GetBioseqIdForTxtId);
        st->Bind(1, idh.AsString());
    }
    Int8 bioseq_id = 0;
    if ( !st->Step() )
    {
        st->Reset();
        return 0;
    }
    bioseq_id = st->GetInt8(0);
    // Reset on conflict
    if ( st->Step() ) {
        bioseq_id = -1;
    }
    st->Reset();
    return bioseq_id;
}


void CLDS2_Database::GetSynonyms(const CSeq_id_Handle& idh, TLdsIdSet& ids)
{
    Int8 bioseq_id = GetBioseqId(idh);
    if (bioseq_id <= 0) {
        // No such id or conflict
        return;
    }
    CSQLITE_Statement& st = x_GetStatement(eSt_GetSynonyms);
    st.Bind(1, bioseq_id);
    while ( st.Step() ) {
        ids.insert(st.GetInt8(0));
    }
    st.Reset();
}


CSQLITE_Statement&
CLDS2_Database::x_InitGetBioseqsSql(const CSeq_id_Handle& idh) const
{
    CSQLITE_Statement* st = NULL;
    if ( idh.IsGi() ) {
        st = &x_GetStatement(eSt_GetBioseqForIntId);
        st->Bind(1, idh.GetGi());
    }
    else {
        st = &x_GetStatement(eSt_GetBioseqForTxtId);
        st->Bind(1, idh.AsString());
    }
    _ASSERT(st);
    return *st;
}


SLDS2_Blob CLDS2_Database::GetBlobInfo(const CSeq_id_Handle& idh)
{
    CSQLITE_Statement& st = x_InitGetBioseqsSql(idh);
    if ( !st.Step() ) {
        st.Reset();
        return SLDS2_Blob();
    }
    SLDS2_Blob info;
    info.id = st.GetInt8(0);
    info.type = SLDS2_Blob::EBlobType(st.GetInt(1));
    info.file_id = st.GetInt8(2);
    info.file_pos = st.GetInt8(3);
    if ( st.Step() ) {
        st.Reset();
        // Conflict - return empty blob
        return SLDS2_Blob();
    }
    st.Reset();
    return info;
}


SLDS2_Blob CLDS2_Database::GetBlobInfo(Int8 blob_id)
{
    SLDS2_Blob info;
    if (blob_id <= 0) {
        return info;
    }
    CSQLITE_Statement& st = x_GetStatement(eSt_GetBlobInfo);
    st.Bind(1, blob_id);
    if ( !st.Step() ) {
        return info;
    }
    info.id = st.GetInt8(0);
    info.type = SLDS2_Blob::EBlobType(st.GetInt(1));
    info.file_id = st.GetInt8(2);
    info.file_pos = st.GetInt8(3);
    _ASSERT( !st.Step() );
    st.Reset();
    return info;
}


SLDS2_File CLDS2_Database::GetFileInfo(const string& file_name) const
{
    SLDS2_File info(file_name);

    CSQLITE_Statement& st = x_GetStatement(eSt_GetFileInfoByName);
    st.Bind(1, file_name);
    if ( st.Step() ) {
        info.id = st.GetInt8(0);
        info.format = SLDS2_File::TFormat(st.GetInt(1));
        info.handler = st.GetString(2);
        info.size = st.GetInt8(3);
        info.time = st.GetInt8(4);
        info.crc = st.GetInt(5);
    }
    st.Reset();
    return info;
}


SLDS2_File CLDS2_Database::GetFileInfo(Int8 file_id)
{
    SLDS2_File info;
    if (file_id <= 0) {
        return info;
    }
    CSQLITE_Statement& st = x_GetStatement(eSt_GetFileInfoById);
    st.Bind(1, file_id);
    if ( st.Step() ) {
        info.id = file_id;
        info.name = st.GetString(0);
        info.format = SLDS2_File::TFormat(st.GetInt(1));
        info.handler = st.GetString(2);
        info.size = st.GetInt8(3);
        info.time = st.GetInt8(4);
        info.crc = st.GetInt(5);
    }
    st.Reset();
    return info;
}


void CLDS2_Database::GetBioseqBlobs(const CSeq_id_Handle& idh,
                                    TBlobSet&             blobs)
{
    CSQLITE_Statement& st = x_InitGetBioseqsSql(idh);
    while ( st.Step() ) {
        SLDS2_Blob info;
        info.id = st.GetInt8(0);
        info.type = SLDS2_Blob::EBlobType(st.GetInt(1));
        info.file_id = st.GetInt8(2);
        info.file_pos = st.GetInt8(3);
        blobs.push_back(info);
    }
    st.Reset();
}


void CLDS2_Database::GetAnnotBlobs(const CSeq_id_Handle& idh,
                                   TAnnotChoice          choice,
                                   TBlobSet&             blobs)
{
    CSQLITE_Statement* st = NULL;
    if ( idh.IsGi() ) {
        st = &x_GetStatement(choice != fAnnot_All
            ? eSt_GetAnnotBlobsByIntId : eSt_GetAnnotBlobsAllByIntId);
        st->Bind(1, idh.GetGi());
    }
    else {
        st = &x_GetStatement(choice != fAnnot_All
            ? eSt_GetAnnotBlobsByTxtId : eSt_GetAnnotBlobsAllByTxtId);
        st->Bind(1, idh.AsString());
    }

    if (choice != fAnnot_All) {
        st->Bind(2, (choice & fAnnot_External) != 0);
    }

    while ( st->Step() ) {
        SLDS2_Blob info;
        info.id = st->GetInt8(0);
        info.type = SLDS2_Blob::EBlobType(st->GetInt(1));
        info.file_id = st->GetInt8(2);
        info.file_pos = st->GetInt8(3);
        blobs.push_back(info);
    }
    st->Reset();
}


void CLDS2_Database::AddChunk(const SLDS2_File&  file_info,
                              const SLDS2_Chunk& chunk_info)
{
    CSQLITE_Statement& st = x_GetStatement(eSt_AddChunk);
    st.Bind(1, file_info.id);
    st.Bind(2, chunk_info.raw_pos);
    st.Bind(3, chunk_info.stream_pos);
    st.Bind(4, chunk_info.data_size);
    st.Bind(5, chunk_info.data, chunk_info.data_size);
    st.Execute();
    st.Reset();
}


bool CLDS2_Database::FindChunk(const SLDS2_File& file_info,
                               SLDS2_Chunk&      chunk_info,
                               Int8              stream_pos)
{
    CSQLITE_Statement& st = x_GetStatement(eSt_FindChunk);
    st.Bind(1, file_info.id);
    st.Bind(2, stream_pos);
    if ( !st.Step() ) {
        st.Reset();
        return false;
    }
    chunk_info.raw_pos = st.GetInt8(0);
    chunk_info.stream_pos = st.GetInt8(1);
    chunk_info.DeleteData(); // just in case someone reuses the same object
    chunk_info.data_size = st.GetInt(2);
    if ( chunk_info.data_size ) {
        chunk_info.InitData(chunk_info.data_size);
        chunk_info.data_size =
            st.GetBlob(3, chunk_info.data, chunk_info.data_size);
    }
    st.Reset();
    return true;
}


void CLDS2_Database::BeginUpdate(void)
{
    x_ExecuteSqls(kLDS2_DropDBIdx,
        sizeof(kLDS2_DropDBIdx)/sizeof(kLDS2_DropDBIdx[0]));
    CSQLITE_Connection& conn = x_GetConn();
    conn.ExecuteSql("begin transaction;");
}


void CLDS2_Database::EndUpdate(void)
{
    CSQLITE_Connection& conn = x_GetConn();
    conn.ExecuteSql("end transaction;");
    x_ExecuteSqls(kLDS2_CreateDBIdx,
        sizeof(kLDS2_CreateDBIdx)/sizeof(kLDS2_CreateDBIdx[0]));
    Analyze();
}


void CLDS2_Database::CancelUpdate(void)
{
    CSQLITE_Connection& conn = x_GetConn();
    conn.ExecuteSql("rollback transaction;");
    x_ExecuteSqls(kLDS2_CreateDBIdx,
        sizeof(kLDS2_CreateDBIdx)/sizeof(kLDS2_CreateDBIdx[0]));
}


void CLDS2_Database::Analyze(void)
{
    CSQLITE_Connection& conn = x_GetConn();
    // Analyze the database indices. This can make queries about
    // 10-100 times faster. The method should be called after any
    // updates.
    conn.ExecuteSql("analyze;");
}


void CLDS2_Database::BeginRead(void)
{
    x_GetConn().ExecuteSql("begin transaction;");
}


void CLDS2_Database::EndRead(void)
{
    x_GetConn().ExecuteSql("end transaction;");
}


END_SCOPE(objects)
END_NCBI_SCOPE
