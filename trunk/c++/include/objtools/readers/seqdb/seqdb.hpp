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

USING_SCOPE(objects);


/////////////////////////////////////////////////////////////////////////////
///
/// CSeqDBIter --
///
/// Small class to iterate over a seqdb database.
///
/// This serves something of the same role for a CSeqDB object that a
/// vector iterator might serve in the standard template library.

class CSeqDB;

class CSeqDBIter {
public:
    typedef Uint4 TOID;
    
    virtual ~CSeqDBIter()
    {
        x_RetSeq();
    }
    
    CSeqDBIter & operator++(void);
    
    TOID GetOID(void)
    {
        return m_OID;
    }
    
    const char * GetData(void)
    {
        return m_Data;
    }
    
    Int4 GetLength(void)
    {
        return m_Length;
    }
    
    operator bool()
    {
        return m_Length != (Uint4)-1;
    }
    
    CSeqDBIter(const CSeqDBIter &);
    
    CSeqDBIter & operator =(const CSeqDBIter &);
    
private:
    inline void x_GetSeq(void);
    inline void x_RetSeq(void);
    
    friend class CSeqDB;
    
    CSeqDBIter(const CSeqDB *, Uint4 oid);
    
    const CSeqDB     * m_DB;
    TOID               m_OID;
    const char       * m_Data;
    Uint4              m_Length;
};

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
    
    CSeqDB(const string & dbname,
           char           prot_nucl,
           bool           use_mmap = true);
    
    /// Destructor.
    ///
    /// This will return resources acquired by this object, including
    /// any gotten by the GetSequence() call, whether or not they have
    /// been returned by RetSequence().
    ~CSeqDB();
    
    
    /// Returns the sequence length in base pairs or residues.
    Int4 GetSeqLength(TOID oid) const;
    
    /// Returns an unbiased, approximate sequence length.
    ///
    /// For protein DBs, this method is identical to GetSeqLength().
    /// In the nucleotide case, computing the exact length requires
    /// examination of the sequence data.  This method avoids doing
    /// that, returning an approximation ranging from L-3 to L+3
    /// (where L indicates the exact length), and unbiased on average.
    Int4 GetSeqLengthApprox(TOID oid) const;
    
    /// Get the ASN.1 header for the sequence.
    CRef<CBlast_def_line_set> GetHdr(TOID oid) const;
    
    /// Get a CBioseq of the sequence.
    CRef<CBioseq> GetBioseq(TOID                oid,
                            bool use_objmgr   = true,
                            bool insert_ctrlA = false) const;
    

    /// Get a pointer to raw sequence data.
    ///
    /// Get the raw sequence (strand data).  When done, resources
    /// should be returned with RetSequence.  This data pointed to
    /// by *buffer is in read-only memory (where supported).
    /// @return
    ///   In case of an error, -1 is returned; otherwise the return
    ///   value is the sequence length (in base pairs or residues).
    Int4 GetSequence(TOID oid, const char ** buffer) const;
    
    /// Get a pointer to sequence data with embedded ambiguities.
    ///
    /// In the protein case, this is identical to GetSequence().  In
    /// the nucleotide case, it stores 2 bases per byte instead of 4.
    /// The third parameter indicates the encoding for nucleotide
    /// data, either kSeqDBNucl4NA or kSeqDBNuclBlastNA, ignored if
    /// the sequence is a protein sequence.  When done, resources
    /// should be returned with RetSequence.
    /// @return
    ///   In case of an error, -1 is returned; otherwise the return
    ///   value is the sequence length (in base pairs or residues).
    Int4 GetAmbigSeq(TOID oid, const char ** buffer, Uint4 nucl_code) const;
    
    /// Get a pointer to sequence data with embedded ambiguities.
    ///
    /// This is like GetAmbigSeq(), but the allocated object should be
    /// deleted by the caller.  This is intended for users who are
    /// going to modify the sequence data, or are going to mix the
    /// data into a container with other data, and who are mixing data
    /// from multiple sources and want to free the data in the same
    /// way.  The fourth parameter should be given one of the values
    /// from EAllocStrategy; the corresponding method should be used
    /// to delete the object.  Note that "delete[]" should be used
    /// instead of "delete"
    /// @param oid
    ///   Ordinal ID.
    /// @param buffer
    ///   Address of a char pointer to access the sequence data.
    /// @param nucl_code
    ///   The NA encoding, kSeqDBNuclNcbiNA8 or kSeqDBNuclBlastNA8.
    /// @param alloc_strategy
    ///   Indicate which allocation strategy to use.
    Int4 GetAmbigSeqAlloc(TOID               oid,
                          char            ** buffer,
                          Uint4              nucl_code,
                          ESeqDBAllocType    strategy) const;
    
    /// Returns any resources associated with the sequence.
    /// 
    /// Note that if memory mapping was successful, the sequence data
    /// is in read only memory; also, this method has no effect.  If
    /// memory mapping failed, the sequence is probably in dynamically
    /// allocated memory and this method frees that memory.
    void RetSequence(const char ** buffer) const;
    
    /// Gets a list of sequence identifiers.
    /// 
    /// This returns the list of CSeq_id identifiers associated with
    /// the sequence specified by the given OID.
    list< CRef<CSeq_id> > GetSeqIDs(TOID oid) const;
    
    /// Returns the type of database opened - protein or nucleotide.
    /// 
    /// This uses the same constants as the constructor.
    char GetSeqType(void) const;
    
    /// Returns the database title.
    ///
    /// This is usually read from database volumes or alias files.  If
    /// multiple databases were passed to the constructor, this will
    /// be a concatenation of those databases' titles.
    string GetTitle(void) const;
    
    /// Returns the construction date of the database.
    /// 
    /// This is encoded in the database.  If multiple databases or
    /// multiple volumes were accessed, the first available date will
    /// be used.
    string GetDate(void) const;
    
    /// Returns the number of sequences available.
    Uint4 GetNumSeqs(void) const;
    
    /// Returns the sum of the lengths of all available sequences.
    ///
    /// This uses summary information stored in the database volumes
    /// or alias files.  It provides an exact value, without iterating
    /// over individual sequences.
    Uint8 GetTotalLength(void) const;
    
    /// Returns the length of the largest sequence in the database.
    ///
    /// This uses summary information stored in the database volumes
    /// or alias files.  This might be used to chose buffer sizes.
    Uint4 GetMaxLength(void) const;
    
    /// Returns a sequence iterator.
    ///
    /// This gets an iterator designed to allow traversal of the
    /// database from beginning to end.
    CSeqDBIter Begin(void) const;
    
    /// Restrict iterators to range of OIDs.
    ///
    /// This causes iteration using iterators or the CheckOrFindOID()
    /// method to skip OIDs outside of the range [first,last].
    void SetOIDRange(TOID first, TOID last);
    
    /// Find an included OID, incrementing next_oid if necessary.
    ///
    /// If the specified OID is not included in the set (i.e. the OID
    /// mask), the input parameter is incremented until one is found
    /// that is.  The user will probably want to increment between
    /// calls, if iterating over the db.
    /// @return
    ///   True if a valid OID was found, false otherwise.
    bool CheckOrFindOID(TOID & next_oid) const;
    
    /// Get list of database names.
    ///
    /// This returns the database name list used at construction.
    /// @return
    ///   List of database names.
    const string & GetDBNameList(void) const;
    
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

// Inline methods for CSeqDBIter

void CSeqDBIter::x_GetSeq(void)
{
    m_Length = m_DB->GetSequence(m_OID, & m_Data);
}

void CSeqDBIter::x_RetSeq(void)
{
    if (m_Data)
        m_DB->RetSequence(& m_Data);
}

END_NCBI_SCOPE

#endif // OBJTOOLS_READERS_SEQDB__SEQDB_HPP

