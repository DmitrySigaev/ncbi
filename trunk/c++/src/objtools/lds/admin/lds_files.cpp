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
 * File Description:  LDS Files implementation.
 *
 */

#include <corelib/ncbifile.hpp>
#include <corelib/ncbitime.hpp>

#include <util/checksum.hpp>
#include <util/format_guess.hpp>

#include <bdb/bdb_cursor.hpp>

#include <objtools/lds/admin/lds_files.hpp>
#include <objtools/lds/lds_set.hpp>
#include <objtools/lds/lds_util.hpp>
#include <objtools/lds/lds_query.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


void CLDS_File::SyncWithDir(const string& path, 
                            CLDS_Set* deleted, 
                            CLDS_Set* updated)
{
    CDir dir(path);
    if (!dir.Exists()) {
        string err("Directory is not found or access denied:");
        err.append(path);

        LDS_THROW(eFileNotFound, err);
    }

    CLDS_Query lds_query(m_db);

    set<string> files;

    // Scan the directory, compare it against File table

    CDir::TEntries  content(dir.GetEntries());
    ITERATE(CDir::TEntries, i, content) {

        if (!(*i)->IsFile()) {
            continue;
        }

        CTime modification;
        string entry = (*i)->GetPath();
        string name = (*i)->GetName();
        string ext = (*i)->GetExt();
        (*i)->GetTime(&modification);
        time_t tm = modification.GetTimeT();
        CFile fl(entry);
        size_t file_size = fl.GetLength();

        if (ext == ".db") {
            continue; // Berkeley DB file, no need to index it.
        }

        bool found = lds_query.FindFile(entry);

        files.insert(entry);

        if (!found) {  // new file arrived
            CFormatGuess fg;
            Uint4 crc = ComputeFileCRC32(entry);
            CFormatGuess::EFormat format = fg.Format(entry);

            FindMaxRecId();
            ++m_MaxRecId;

            m_FileDB.file_id = m_MaxRecId;
            m_FileDB.file_name = entry.c_str();
            m_FileDB.format = format;
            m_FileDB.time_stamp = tm;
            m_FileDB.CRC = crc;
            m_FileDB.file_size = file_size;

            m_FileDB.Insert();

            updated->insert(m_MaxRecId);

            LOG_POST(Info << "New LDS file found: " << entry);

            continue;
        }

        if (tm != m_FileDB.time_stamp || file_size != m_FileDB.file_size) {
            updated->insert(m_FileDB.file_id);
            UpdateEntry(m_FileDB.file_id, entry, 0, tm, file_size);
        } else {
            Uint4 crc = ComputeFileCRC32(entry);
            if (crc != m_FileDB.CRC) {
                updated->insert(m_FileDB.file_id);
                UpdateEntry(m_FileDB.file_id, entry, crc, tm, file_size);
            }
        }

    } // ITERATE


    // Scan the database, find deleted files

    CBDB_FileCursor cur(m_FileDB);
    cur.SetCondition(CBDB_FileCursor::eFirst);
    while (cur.Fetch() == eBDB_Ok) {
        string fname(m_FileDB.file_name);
        set<string>::const_iterator fit = files.find(fname);
        if (fit == files.end()) { // not found
            deleted->insert(m_FileDB.file_id);

            LOG_POST(Info << "LDS: File removed: " << fname);
        }
    } // while

    Delete(*deleted);
}


void CLDS_File::Delete(const CLDS_Set& record_set)
{
    ITERATE(CLDS_Set, it, record_set) {
        m_FileDB.file_id = *it;
        m_FileDB.Delete();
    }
}


void CLDS_File::UpdateEntry(int    file_id, 
                            const  string& file_name,
                            Uint4  crc,
                            int    time_stamp,
                            size_t file_size)
{
    if (!crc) {
        crc = ComputeFileCRC32(file_name);
    }

    m_FileDB.file_id = file_id;
    if (m_FileDB.Fetch() != eBDB_Ok) {
        LDS_THROW(eRecordNotFound, "Files record not found");
    }

    // Re-evalute the file format

    CFormatGuess fg;
    CFormatGuess::EFormat format = fg.Format(file_name);

    m_FileDB.format = format;
    m_FileDB.time_stamp = time_stamp;
    m_FileDB.CRC = crc;
    m_FileDB.file_size = file_size;

    EBDB_ErrCode err = m_FileDB.UpdateInsert();
    BDB_CHECK(err, "LDS::File");

    LOG_POST(Info << "LDS: file update: " << file_name);

}


int CLDS_File::FindMaxRecId()
{
    if (m_MaxRecId) {
        return m_MaxRecId;
    }
    LDS_GETMAXID(m_MaxRecId, m_FileDB, file_id);
    return m_MaxRecId;
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.4  2003/07/02 12:07:42  dicuccio
 * Fix for implicit conversion/assignment in gcc
 *
 * Revision 1.3  2003/06/16 16:24:43  kuznets
 * Fixed #include paths (lds <-> lds_admin separation)
 *
 * Revision 1.2  2003/06/16 14:55:00  kuznets
 * lds splitted into "lds" and "lds_admin"
 *
 * Revision 1.1  2003/06/03 14:13:25  kuznets
 * Initial revision
 *
 *
 * ===========================================================================
 */

