#ifndef OBJTOOLS_WRITERS_WRITEDB__WRITEDB_IMPL_HPP
#define OBJTOOLS_WRITERS_WRITEDB__WRITEDB_IMPL_HPP

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

/// @file writedb_impl.hpp
/// Defines implementation class of WriteDB.
///
/// Defines classes:
///     CWriteDBHeader
///
/// Implemented for: UNIX, MS-Windows

#include <objects/seq/seq__.hpp>
#include <objects/blastdb/blastdb__.hpp>
#include "writedb_volume.hpp"

#include <objmgr/bioseq_handle.hpp>
#include <objmgr/seq_vector.hpp>

BEGIN_NCBI_SCOPE

/// Import definitions from the objects namespace.
USING_SCOPE(objects);

/// CWriteDB_Impl class
/// 
/// This manufactures blast database header files from input data.

class CWriteDB_Impl {
public:
    /// Whether and what kind of indices to build.
    typedef EWriteDBIndexType EIndexType;
    
    // Setup and control
    
    /// Constructor.
    /// @param dbname Name of the database to create.
    /// @param protein True for protein, false for nucleotide.
    /// @param title Title string for volumes and alias file.
    /// @param indices Type of indexing to do for string IDs.
    CWriteDB_Impl(const string & dbname,
                  bool           protein,
                  const string & title,
                  EIndexType     indices);
    
    /// Destructor.
    ~CWriteDB_Impl();
    
    /// Close the file and flush any remaining data to disk.
    void Close();
    
    // Sequence Data
    
    /// Add a new sequence as raw sequence and ambiguity data.
    /// 
    /// A new sequence record is started, and data from any previous
    /// sequence is combined and written to disk.  Each sequence needs
    /// sequence data and header data.  This method takes sequence
    /// data in the form of seperated sequence data and compressed
    /// ambiguities packed in the blast database disk format.  It is
    /// intended for efficiently copying sequences from sources that
    /// provide this format, such as CSeqDBExpert().  If this method
    /// is used for protein data, the ambiguities string should be
    /// empty.  If this method is used, header data must also be
    /// specified with a call to SetDeflines().
    ///
    /// @param sequence Sequence data in blast db disk format.
    /// @param ambiguities Ambiguity data in blast db disk format.
    void AddSequence(const CTempString & sequence,
                     const CTempString & ambiguities);
    
    /// Add a new sequence as a CBioseq.
    /// 
    /// A new sequence record is started, and data from any previous
    /// sequence is combined and written to disk.  Each sequence needs
    /// sequence data and header data.  This method can extract both
    /// from the provided CBioseq.  If other header data is preferred,
    /// SetDeflines() can be called after this method to replace the
    /// header data from the CBioseq.  Note that CBioseqs from some
    /// sources are not guaranteed to contain sequence data; if this
    /// might be the case, consider the versions of AddSequence that
    /// take either CBioseq_Handle or CBioseq and CSeqVector.  In
    /// order to use this method, sequence data should be accessible
    /// from bs.GetInst().GetSeq_data().  (Note: objects provided to
    /// WriteDB will be kept alive until the next AddSequence call.)
    ///
    /// @param bs Bioseq containing sequence and header data.
    void AddSequence(const CBioseq & bs);
    
    /// Add a new sequence as a CBioseq_Handle.
    /// 
    /// A new sequence record is started, and data from any previous
    /// sequence is combined and written to disk.  Each sequence needs
    /// sequence data and header data.  This method can extract both
    /// from the provided CBioseq_Handle.  If other header data is
    /// preferred, SetDeflines() can be called after this method to
    /// replace the header data from the CBioseq.  (Note: objects
    /// provided to WriteDB will be kept alive until the next
    /// AddSequence call.)
    /// 
    /// @param bsh Bioseq_Handle for sequence to add.
    void AddSequence(const CBioseq_Handle & bsh);
    
    /// Add a new sequence as a CBioseq_Handle.
    /// 
    /// A new sequence record is started, and data from any previous
    /// sequence is combined and written to disk.  Each sequence needs
    /// sequence data and header data.  This method will extract
    /// header data from the provided CBioseq.  If the CBioseq
    /// contains sequence data, it will be used; otherwise sequence
    /// data will be fetched from the provided CSeqVector.  If other
    /// header data is preferred, SetDeflines() can be called after
    /// this method.  (Note: objects provided to WriteDB will be kept
    /// alive until the next AddSequence call.)
    /// 
    /// @param bs Bioseq_Handle for header and sequence data.
    /// @param sv CSeqVector for sequence data.
    void AddSequence(const CBioseq & bs, CSeqVector & sv);
    
    /// Add or replace header data.
    /// 
    /// This method replaces any stored header data for the current
    /// sequence with the provided CBlast_def_line_set.  Header data
    /// can be constructed directly by the caller, or extracted from
    /// an existing CBioseq using ExtractBioseqDeflines (see below).
    /// Once it is in the correct form, it can be attached to the
    /// sequence with this method.  (Note: objects provided to WriteDB
    /// will be kept alive until the next AddSequence call.)
    ///
    /// @param deflines Header data for the most recent sequence.
    void SetDeflines(const CBlast_def_line_set & deflines);
    
    /// Set the PIG identifier of this sequence.
    /// 
    /// For protein sequences, this sets the PIG identifier.  PIG ids
    /// are per-sequence, so it will only be attached to the first
    /// defline in the set.
    /// 
    /// @param pig PIG identifier as an integer.
    void SetPig(int pig);
    
    // Options
    
    /// Set the maximum size for any file in the database.
    /// 
    /// This method sets the maximum size for any file in a database
    /// volume.  If adding a sequence would cause any file in the
    /// generated database to exceed this size, the current volume is
    /// ended and a new volume is started.  This is not a strict
    /// limit, inasmuch as it always puts at least one sequence in
    /// each volume regardless of that sequence's size.
    /// 
    /// @param sz Maximum file size (in bytes).
    void SetMaxFileSize(Uint8 sz);
    
    /// Set the maximum letters in one volume.
    ///
    /// This method sets the maximum number of sequence letters per
    /// database volume.  If adding a sequence would cause the volume
    /// to have more than this many letters, the current volume is
    /// ended and a new volume is started.  This is not a strict
    /// limit, inasmuch as it always puts at least one sequence in
    /// each volume regardless of that sequence's size.
    ///
    /// @param sz Maximum sequence letters per volume.
    void SetMaxVolumeLetters(Uint8 sz);
    
    /// Extract deflines from a CBioseq.
    ///
    /// Given a CBioseq, this method extracts and returns header info
    /// as a defline set.  The deflines will not be applied to the
    /// current sequence unless passed to SetDeflines.  The expected
    /// use of this method is in cases where the caller has a CBioseq
    /// or CBioseq_Handle but wishes to examine and/or change the
    /// deflines before passing them to CWriteDB.  Some elements of
    /// the CBioseq may be shared by the returned defline set, notably
    /// the Seq-ids.
    ///
    /// @param bs Bioseq from which to construct the defline set.
    /// @return The blast defline set.
    static CRef<CBlast_def_line_set>
    ExtractBioseqDeflines(const CBioseq & bs);
    
    /// Set bases that should not be used in sequences.
    /// 
    /// This method specifies nucelotide or protein bases that should
    /// not be used in the resulting database.  The bases in question
    /// will be replaced with N (for nucleotide) or X (for protein).
    /// The input data is expected to be specified in the appropriate
    /// 'alphabetic' encoding (either IUPACAA and IUPACNA).
    /// 
    /// @param masked
    void SetMaskedLetters(const string & masked);
    
    /// List Volumes
    ///
    /// Returns the base names of all volumes constructed by this
    /// class; the returned list may not be complete until Close() has
    /// been called.
    ///
    /// @param vols
    ///   The set of volumes produced by this class.
    void ListVolumes(vector<string> & vols);
    
    /// List Filenames
    ///
    /// Returns a list of the files constructed by this class; the
    /// returned list may not be complete until Close() has been
    /// called.
    ///
    /// @param files
    ///   The set of resolved database path names.
    void ListFiles(vector<string> & files);
    
private:
    // Configuration
    
    string       m_Dbname;           ///< Database base name.
    bool         m_Protein;          ///< True if DB is protein.
    string       m_Title;            ///< Title field of database.
    string       m_Date;             ///< Time stamp (for all volumes.)
    Uint8        m_MaxFileSize;      ///< Maximum size of any file.
    Uint8        m_MaxVolumeLetters; ///< Max letters per volume.
    EIndexType   m_Indices;          ///< Indexing mode.
    bool         m_Closed;           ///< True if database has been closed.
    string       m_MaskedLetters;    ///< Masked protein letters (IUPAC).
    string       m_MaskByte;         ///< Byte that replaced masked letters.
    vector<bool> m_MaskLookup;       ///< Maps (blast-aa) byte to maskedness.
    
    // Functions
    
    /// Flush accumulated sequence data to volume.
    void x_Publish();
    
    /// Compute name of alias file produced.
    string x_MakeAliasName();
    
    /// Flush accumulated sequence data to volume.
    void x_MakeAlias();
    
    /// Clear sequence data from last sequence.
    void x_ResetSequenceData();
    
    /// Convert and compute final data formats.
    void x_CookData();
    
    /// Convert header data into usable forms.
    void x_CookHeader();
    
    /// Collect ids for ISAM files.
    void x_CookIds();
    
    /// Convert sequence data into usable forms.
    void x_CookSequence();
    
    /// Replace masked input letters with m_MaskByte value.
    void x_MaskSequence();
    
    /// Get binary version of deflines from 'user' data in Bioseq.
    ///
    /// Some CBioseq objects (e.g. those from CSeqDB) have an ASN.1
    /// octet array containing a binary ASN.1 version of the blast
    /// defline set for the sequence.  This method looks for that data
    /// and returns it if found.  If not found, it returns an empty
    /// string.
    ///
    /// @param bioseq Bioseq from which to fetch header. [in]
    /// @param binhdr Header data as binary ASN.1. [out]
    static void x_GetBioseqBinaryHeader(const CBioseq & bioseq,
                                        string        & binhdr);
    
    /// Construct deflines from a CBioseq and other meta-data.
    ///
    /// This method builds deflines from various data found in the
    /// Bioseq, along with other meta data (like the PIG and
    /// membership and linkout lists.)
    ///
    /// @param bioseq Defline data will be built from this. [in]
    /// @param deflines A defline set will be returned here. [out]
    /// @param membits Membership bits for each defline. [in]
    /// @param linkout Linkout bits for each defline. [in]
    /// @param pig PIG to attach to a protein sequence. [in]
    static void
    x_BuildDeflinesFromBioseq(const CBioseq                  & bioseq,
                              CConstRef<CBlast_def_line_set> & deflines, 
                              const vector< vector<int> >    & membits,
                              const vector< vector<int> >    & linkout,
                              int                              pig);
    
    /// Extract a defline set from a binary ASN.1 blob.
    /// @param bin_hdr Binary ASN.1 encoding of defline set. [in]
    /// @param deflines Defline set. [out]
    static void
    x_SetDeflinesFromBinary(const string                   & bin_hdr,
                            CConstRef<CBlast_def_line_set> & deflines);
    
    /// Returns true if we have unwritten sequence data.
    bool x_HaveSequence() const;
    
    /// Records that we now have unwritten sequence data.
    void x_SetHaveSequence();
    
    /// Records that we no longer have unwritten sequence data.
    void x_ClearHaveSequence();
    
    /// Get deflines from a CBioseq and other meta-data.
    ///
    /// This method extracts binary ASN.1 deflines from a CBioseq if
    /// possible, and otherwise builds deflines from various data
    /// found in the Bioseq, along with other meta data (like the PIG
    /// and membership and linkout lists.)  It returns the result as
    /// a blast defline set.  If a binary version of the headers is
    /// computed during this method, it will be returned in bin_hdr.
    ///
    /// @param bioseq Defline data will be built from this. [in]
    /// @param deflines A defline set will be returned here. [out]
    /// @param bin_hdr Binary header data may be returned here. [out]
    /// @param membits Membership bits for each defline. [in]
    /// @param linkout Linkout bits for each defline. [in]
    /// @param pig PIG to attach to a protein sequence. [in]
    static void x_ExtractDeflines(CConstRef<CBioseq>             & bioseq,
                                  CConstRef<CBlast_def_line_set> & deflines,
                                  string                         & bin_hdr,
                                  const vector< vector<int> >    & membbits,
                                  const vector< vector<int> >    & linkouts,
                                  int                              pig);
    
    //
    // Accumulated sequence data.
    //
    
    /// Bioseq object for next sequence to write.
    CConstRef<CBioseq> m_Bioseq;
    
    /// SeqVector for next sequence to write.
    CSeqVector m_SeqVector;
    
    /// Deflines to write as header.
    CConstRef<CBlast_def_line_set> m_Deflines;
    
    /// Ids for next sequence to write, for use during ISAM construction.
    vector< CRef<CSeq_id> > m_Ids;
    
    /// Linkout bits - outer vector is per-defline, inner is bits.
    vector< vector<int> > m_Linkouts;
    
    /// Membership bits - outer vector is per-defline, inner is bits.
    vector< vector<int> > m_Memberships;
    
    /// PIG to attach to headers for protein sequences.
    int m_Pig;
    
    /// True if we have a sequence to write.
    bool m_HaveSequence;
    
    // Cooked
    
    /// Sequence data in format that will be written to disk.
    string m_Sequence;
    
    /// Ambiguities in format that will be written to disk.
    string m_Ambig;
    
    /// Binary header in format that will be written to disk.
    string m_BinHdr;
    
    // Volumes
    
    /// This volume is currently accepting sequences.
    CRef<CWriteDB_Volume> m_Volume;
    
    /// List of all volumes so far, up to and including m_Volume.
    vector< CRef<CWriteDB_Volume> > m_VolumeList;
};

END_NCBI_SCOPE


#endif // OBJTOOLS_WRITERS_WRITEDB__WRITEDB_IMPL_HPP


