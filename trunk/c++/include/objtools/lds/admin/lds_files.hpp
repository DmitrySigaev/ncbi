#ifndef LDS_FILES_HPP__
#define LDS_FILES_HPP__
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
 * File Description: Different operations on LDS File table
 *
 */

#include <corelib/ncbistd.hpp>

#include <objtools/lds/lds_db.hpp>
#include <objtools/lds/lds_set.hpp>
#include <objtools/lds/lds_expt.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


//////////////////////////////////////////////////////////////////
//
// SLDS_FileDB related methods.
//

class NCBI_LDS_EXPORT CLDS_File
{
public:
    CLDS_File(SLDS_FileDB& file_db)
    : m_FileDB(file_db),
      m_MaxRecId(0)
    {}

    // Scan the given directory, calculate timestamp and control sums for 
    // every file, update "File" database table.
    // Method returns set of row ids deleted from files, and set of row ids
    // to be updated.
    void SyncWithDir(const string& path, CLDS_Set* deleted, CLDS_Set* updated);

    // Scan the database, find the file
    bool DBFindFile(const string& path);

    // Delete all records with ids belonging to record_set
    void Delete(const CLDS_Set& record_set);

    void UpdateEntry(int    file_id, 
                     const  string& file_name,
                     Uint4  crc,
                     int    timestamp,
                     size_t file_size);

    // Return max record id. 0 if no record found.
    int FindMaxRecId();

private:
    CLDS_File(const CLDS_File&);
    CLDS_File& operator=(const CLDS_File&);

private:
    SLDS_FileDB&  m_FileDB;
    int           m_MaxRecId;
};


END_SCOPE(objects)
END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.5  2003/06/03 19:14:02  kuznets
 * Added lds dll export/import specifications
 *
 * Revision 1.4  2003/06/03 14:07:46  kuznets
 * Include paths changed to reflect the new directory structure
 *
 * Revision 1.3  2003/05/23 20:33:33  kuznets
 * Bulk changes in lds library, code reorganizations, implemented top level
 * objects read, metainformation persistance implemented for top level objects...
 *
 * Revision 1.2  2003/05/22 18:57:17  kuznets
 * Work in progress
 *
 * Revision 1.1  2003/05/22 13:24:45  kuznets
 * Initial revision
 *
 *
 * ===========================================================================
 */

#endif

