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
 * File Description:  LDS database maintanance implementation.
 *
 */

#include <objtools/lds/admin/lds_admin.hpp>
#include <objtools/lds/admin/lds_files.hpp>
#include <objtools/lds/admin/lds_object.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

CLDS_Management::CLDS_Management(CLDS_Database& db)
: m_lds_db(db)
{}


void CLDS_Management::SyncWithDir(const string& dir_name)
{
    CLDS_Set files_deleted;
    CLDS_Set files_updated;
    SLDS_TablesCollection& db = m_lds_db.GetTables();

    CLDS_File fl(db);
    fl.SyncWithDir(dir_name, &files_deleted, &files_updated);

    CLDS_Set objects_deleted;
    CLDS_Set annotations_deleted;

    CLDS_Object obj(db, m_lds_db.GetObjTypeMap());
    obj.DeleteCascadeFiles(files_deleted, 
                           &objects_deleted, &annotations_deleted);
    obj.UpdateCascadeFiles(files_updated);
}


CLDS_Database* 
CLDS_Management::OpenCreateDB(const string& dir_name,
                              const string& db_name,
                              bool*         is_created)
{
    CLDS_Database* db = new CLDS_Database(dir_name + "\\"+ db_name);
    try {
        db->Open();
        *is_created = false;
    } 
    catch (CBDB_ErrnoException& )
    {
        // Failed to open: file does not exists.
        // Force the construction

        CLDS_Management admin(*db);
        admin.Create();
        admin.SyncWithDir(dir_name);
    }
    *is_created = true;
    return db;
}

END_SCOPE(objects)
END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.3  2003/06/25 18:28:40  kuznets
 * + CLDS_Management::OpenCreateDB(...)
 *
 * Revision 1.2  2003/06/16 16:24:43  kuznets
 * Fixed #include paths (lds <-> lds_admin separation)
 *
 * Revision 1.1  2003/06/16 14:55:00  kuznets
 * lds splitted into "lds" and "lds_admin"
 *
 * ===========================================================================
*/
