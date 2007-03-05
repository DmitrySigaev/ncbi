#ifndef LDS_ADMIN_HPP__
#define LDS_ADMIN_HPP__
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
 * File Description: LDS data management(admin) include file.
 *
 */

#include <objtools/lds/lds.hpp>
#include <objtools/lds/lds_object.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


//////////////////////////////////////////////////////////////////
//
// LDS database maintanance(modification).
//

class NCBI_LDS_EXPORT NCBI_DEPRECATED CLDS_Management
{
public:
    CLDS_Management(CLDS_Database& db);

    /// Create the database. If the LDS database already exists all data will
    /// be cleaned up.
    void Create();

    /// Recursive scanning of directories in LDS database
    enum ERecurse {
        eDontRecurse,
        eRecurseSubDirs
    };

    /// CRC32 calculation mode
    enum EComputeControlSum {
        eNoControlSum,
        eComputeControlSum
    };

    /// Duplicate ids handling
    enum EDuplicateId {
        eCheckDuplicates,
        eIgnoreDuplicates
    };


    /// Syncronize LDS database content with directory. 
    /// The main workhorse function.
    /// Function will do format guessing, files parsing, etc
    /// Current implementation can work with only one directory.
    /// If file is removed all relevant objects are cleaned up too.
    /// @param dir_name
    ///   directory to syncronize with
    /// @param recurse
    ///   subdirectories recursion mode
    /// @param control_sum
    ///   control sum computation
    void SyncWithDir(const string&      dir_name, 
                     ERecurse           recurse = eRecurseSubDirs,
                     EComputeControlSum control_sum = eComputeControlSum,
                     EDuplicateId       dup_control = eIgnoreDuplicates);


    /// Function tries to open the database, if the database does not exist
    /// creates it and loads all the data.
    /// Return pointer on new CLDS_Database object 
    /// (caller is responsible for the destruction).
    /// is_created parameter receives TRUE if the database was created by 
    /// the method.
    static CLDS_Database* 
        OpenCreateDB(const string&      dir_name,
                     const string&      db_name,
                     bool*              is_created,
                     ERecurse           recurse = eRecurseSubDirs,
                     EComputeControlSum control_sum = eComputeControlSum,
                     EDuplicateId       dup_control = eIgnoreDuplicates);


    /// Function tries to open the database, if the database does not exist
    /// creates it and loads all the data.
    /// Return pointer on new CLDS_Database object 
    /// (caller is responsible for the destruction).
    /// is_created parameter receives TRUE if the database was created by 
    /// the method.
    static CLDS_Database* 
        OpenCreateDB(const string&      source_dir,
                     const string&      db_dir,
                     const string&      db_name,
                     bool*              is_created,
                     ERecurse           recurse = eRecurseSubDirs,
                     EComputeControlSum control_sum = eComputeControlSum,
                     EDuplicateId       dup_control = eIgnoreDuplicates);

    /// Open the database. Throws an exception if the database cannot be open
    static CLDS_Database* 
        OpenDB(const string& dir_name,
               const string& db_name);

private:
    CLDS_Database&   m_lds_db;        // LDS database object
};

END_SCOPE(objects)
END_NCBI_SCOPE

#endif
