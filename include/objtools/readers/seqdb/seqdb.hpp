#ifndef OBJTOOLS_READERS_SEQDB__SEQDB_HPP
#define OBJTOOLS_READERS_SEQDB__SEQDB_HPP

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

/// @file seqdb.hpp
/// Defines BLAST database access classes.
///
/// Defines classes:
///     CSeqDB
///     CSeqDBSequence
///
/// Implemented for: UNIX, MS-Windows


#include <objtools/readers/seqdb/seqdbcommon.hpp>
#include <objects/blastdb/Blast_def_line.hpp>
#include <objects/blastdb/Blast_def_line_set.hpp>
#include <objects/general/Dbtag.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/seqloc/Seq_id.hpp>

BEGIN_NCBI_SCOPE

USING_NCBI_SCOPE;
USING_SCOPE(objects);


/////////////////////////////////////////////////////////////////////////////
///
/// CSeqDB --
///
/// User interface class for blast databases.
///
/// This class provides the top-level interface class for BLAST
/// database users.  It defines access to the database component by
/// calling methods on objects which represent the various database
/// files, such as the index, header, sequence, and alias files.

class CSeqDB : public CObject {
public:
    /// Sequence type accepted and returned for OID indexes.
    typedef Uint4 TOID;
    
    /// Constructor.
    ///
    /// @param dbpath
    ///   Path to the blast databases, sometimes found in environment
    ///   variable BLASTDB.
    /// @param dbname
    ///   A list of database or alias names, seperated by spaces.
    /// @param prot_nucl
    ///   Either kSeqTypeProt for a protein database, or kSeqTypeNucl
    ///   for nucleotide.  These can also be specified as 'p' or 'n'.
    /// @param use_mmap
    ///   If kSeqDBMMap is specified (the default), memory mapping is
    ///   attempted.  If kSeqDBNoMMap is specified, or memory mapping
    ///   fails, this platform does not support it, the less efficient
    ///   read and write calls are used instead.
    
    CSeqDB(const string & dbpath,
           const string & dbname,
           char           prot_nucl,
           bool           use_mmap = true);
    
    /// Destructor.
    ///
    /// This will return resources acquired by this object, including
    /// any gotten by the GetSequence() call, whether or not they have
    /// been returned by RetSequence().
    ~CSeqDB();
    
    
    /// Returns the sequence length in base pairs or residues.
    Int4 GetSeqLength(TOID oid);
    
    /// Returns an unbiased, approximate sequence length.
    ///
    /// For protein DBs, this method is identical to GetSeqLength().
    /// In the nucleotide case, computing the exact length requires
    /// examination of the sequence data.  This method avoids doing
    /// that, returning an approximation ranging from L-3 to L+3
    /// (where L indicates the exact length), and unbiased on average.
    Int4 GetSeqLengthApprox(TOID oid);
    
    /// Get the ASN.1 header for the sequence.
    CRef<CBlast_def_line_set> GetHdr(TOID oid);
    
    /// Get a CBioseq of the sequence.
    CRef<CBioseq> GetBioseq(TOID                oid,
                            bool use_objmgr   = true,
                            bool insert_ctrlA = false);
    
    /// Get a pointer to raw sequence data.
    ///
    /// Get the raw sequence (strand data).  When done, resources
    /// should be returned with RetSequence.  This data pointed to
    /// by *buffer is in read-only memory (where supported).
    /// @return
    ///   In case of an error, -1 is returned; otherwise the return
    ///   value is the sequence length (in base pairs or residues).
    Int4 GetSequence(TOID oid, const char ** buffer);
    
    /// Returns any resources associated with the sequence.
    /// 
    /// Note that if memory mapping was successful, the sequence data
    /// is in read only memory; also, this method has no effect.  If
    /// memory mapping failed, the sequence is probably in dynamically
    /// allocated memory and this method frees that memory.
    void RetSequence(const char ** buffer);
    
    /// Returns the type of database opened - protein or nucleotide.
    /// 
    /// This uses the same constants as the constructor.
    char GetSeqType(void);
    
    /// Returns the database title.
    ///
    /// This is usually read from database volumes or alias files.  If
    /// multiple databases were passed to the constructor, this will
    /// be a concatenation of those databases' titles.
    string GetTitle(void);
    
    /// Returns the construction date of the database.
    /// 
    /// This is encoded in the database.  If multiple databases or
    /// multiple volumes were accessed, the first available date will
    /// be used.
    string GetDate(void);
    
    /// Returns the number of sequences available.
    Uint4 GetNumSeqs(void);
    
    /// Returns the sum of the lengths of all available sequences.
    ///
    /// This uses summary information stored in the database volumes
    /// or alias files.  It provides an exact value, without iterating
    /// over individual sequences.
    Uint8 GetTotalLength(void);
    
    /// Returns the length of the largest sequence in the database.
    ///
    /// This uses summary information stored in the database volumes
    /// or alias files.  This might be used to chose buffer sizes.
    Uint4 GetMaxLength(void);
    
private:
    /// Implementation details are hidden.  (See seqdbimpl.hpp).
    class CSeqDBImpl * m_Impl;
};


/////////////////////////////////////////////////////////////////////////////
///
/// CSeqDBSequence --
///
/// Small class to manage return of sequence data.
///
/// The CSeqDB class requires that sequences be returned at some point
/// after they are gotten.  This class provides that service via the
/// destructor.  It also insures that the database itself stays around
/// for at least the duration of its lifetime, by holding a CRef<> to
/// that object.  CSeqDB::GetSequence may be used directly to avoid
/// the small overhead of this class, provided care is taken to call
/// CSeqDB::RetSequence.  The data referred to by this object is not
/// modifyable, and is memory mapped (read only) where supported.

class CSeqDBSequence {
public:
    CSeqDBSequence(CSeqDB * db, Uint4 oid)
        : m_DB    (db),
          m_Data  (0),
          m_Length(0)
    {
        m_Length = m_DB->GetSequence(oid, & m_Data);
    }
    
    ~CSeqDBSequence()
    {
        if (m_Data) {
            m_DB->RetSequence(& m_Data);
        }
    }
    
    const char * GetData(void)
    {
        return m_Data;
    }
    
    Uint4 GetLength(void)
    {
        return m_Length;
    }
    
private:
    // Prevent copy for now - would be easy to fix.
    CSeqDBSequence(const CSeqDBSequence &);
    CSeqDBSequence & operator=(const CSeqDBSequence &);
    
    CRef<CSeqDB> m_DB;
    const char * m_Data;
    Uint4        m_Length;
};


END_NCBI_SCOPE

#endif // OBJTOOLS_READERS_SEQDB__SEQDB_HPP

