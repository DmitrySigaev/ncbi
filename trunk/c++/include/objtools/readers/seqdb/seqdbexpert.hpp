#ifndef OBJTOOLS_READERS_SEQDB__SEQDB_EXPERT_HPP
#define OBJTOOLS_READERS_SEQDB__SEQDB_EXPERT_HPP

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
 * Author:  Kevin Bealer
 *
 */

/// @file seqdb_expert.hpp
/// Defines `expert' version of CSeqDB interfaces.
///
/// Defines classes:
///     CSeqDBExpert
///
/// Implemented for: UNIX, MS-Windows

#include <objtools/readers/seqdb/seqdb.hpp>

BEGIN_NCBI_SCOPE

/// Include definitions from the objects namespace.
USING_SCOPE(objects);


/// CSeqDBExpert
///
/// User interface class for blast databases, including experimental
/// and advanced code for expert NCBI use.

class NCBI_XOBJREAD_EXPORT CSeqDBExpert : public CSeqDB {
public:
    /// Short Constructor
    /// 
    /// This version of the constructor assumes memory mapping and
    /// that the entire possible OID range will be included.
    /// 
    /// @param dbname
    ///   A list of database or alias names, seperated by spaces
    /// @param seqtype
    ///   Specify eProtein, eNucleotide, or eUnknown.
    /// @param gilist
    ///   The database will be filtered by this GI list if non-null.
    CSeqDBExpert(const string & dbname,
                 ESeqType seqtype,
                 CSeqDBGiList * gilist = 0);
    
    /// Constructor with MMap Flag and OID Range.
    ///
    /// If the oid_end value is specified as zero, or as a value
    /// larger than the number of OIDs, it will be adjusted to the
    /// number of OIDs in the database.  Specifying 0,0 for the start
    /// and end will cause inclusion of the entire database.  This
    /// version of the constructor is obsolete because the sequence
    /// type is specified as a character (eventually only the ESeqType
    /// version will exist).
    /// 
    /// @param dbname
    ///   A list of database or alias names, seperated by spaces.
    /// @param seqtype
    ///   Specify eProtein, eNucleotide, or eUnknown.
    /// @param oid_begin
    ///   Iterator will skip OIDs less than this value.  Only OIDs
    ///   found in the OID lists (if any) will be returned.
    /// @param oid_end
    ///   Iterator will return up to (but not including) this OID.
    /// @param use_mmap
    ///   If kSeqDBMMap is specified (the default), memory mapping is
    ///   attempted.  If kSeqDBNoMMap is specified, or memory mapping
    ///   fails, this platform does not support it, the less efficient
    ///   read and write calls are used instead.
    /// @param gi_list
    ///   The database will be filtered by this GI list if non-null.
    CSeqDBExpert(const string & dbname,
                 ESeqType       seqtype,
                 int            oid_begin,
                 int            oid_end,
                 bool           use_mmap,
                 CSeqDBGiList * gi_list = 0);
    
    /// Null Constructor
    /// 
    /// This version of the constructor does not open a specific blast
    /// database.  This is provided for cases where the application
    /// only needs 'global' resources like the taxonomy database.
    CSeqDBExpert();
    
    /// Destructor.
    ///
    /// This will return resources acquired by this object, including
    /// any gotten by the GetSequence() call, whether or not they have
    /// been returned by RetSequence().
    ~CSeqDBExpert();
};

END_NCBI_SCOPE

#endif // OBJTOOLS_READERS_SEQDB__SEQDB_EXPERT_HPP


