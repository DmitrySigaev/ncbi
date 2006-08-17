#ifndef OBJTOOLS_WRITERS_WRITEDB__WRITEDB_HPP
#define OBJTOOLS_WRITERS_WRITEDB__WRITEDB_HPP

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

/// @file writedb.hpp
/// Defines BLAST database construction classes.
///
/// Defines classes:
///     CWriteDB
///
/// Implemented for: UNIX, MS-Windows

#include <objtools/writers/writedb/writedb_error.hpp>
#include <objects/blastdb/Blast_def_line.hpp>
#include <objects/blastdb/Blast_def_line_set.hpp>
#include <objects/seq/seq__.hpp>

#include <objmgr/bioseq_handle.hpp>


BEGIN_NCBI_SCOPE

/// Include definitions from the objects namespace.
USING_SCOPE(objects);


/// CWriteDB
///
/// User interface class for blast databases.
///
/// This class provides the top-level interface class for BLAST
/// database users.  It defines access to the database component by
/// calling methods on objects which represent the various database
/// files, such as the index, header, sequence, and alias files.

class NCBI_XOBJWRITE_EXPORT CWriteDB : public CObject
{
public:
    /// Sequence types.
    enum ESeqType {
        /// Protein database.
        eProtein,
        
        /// Nucleotide database.
        eNucleotide
    };
    
    /// Whether and what kind of indices to build.
    enum EIndexType {
        /// Build a database without any indices.
        eNoIndex,
        
        /// Use only simple accessions in the string index.
        eSparseIndex,
        
        /// Use several forms of each Seq-id in the string index.
        eFullIndex
    };
    
    //
    // Setup
    //
    
    /// Constructor
    /// 
    /// Starts construction of a blast database.
    /// 
    /// @param dbname
    ///   A list of database or alias names, seperated by spaces
    /// @param seqtype
    ///   Specify eProtein, eNucleotide, or eUnknown.
    /// @param title
    ///   The database title.
    /// @param itype
    ///   Indicates the type of indices to build if specified.
    CWriteDB(const string & dbname,
             ESeqType       seqtype,
             const string & title,
             EIndexType     itype = eFullIndex);
    
    /// Destructor.
    ///
    /// This will return resources acquired by this object, and call Close()
    /// if it has not already been called.
    ~CWriteDB();
    
    //
    // Adding data
    //
    
    // Each new sequence is started when the client calls one of the
    // AddSequence() methods.  This can optionally be followed by one
    // or more calls to Set...() methods or AddDefline(), to add or
    // change other data.  The accumulated data for the sequence is
    // combined and written when the sequence after it is started
    // (with another AddSequence() call), or when Close() is called.
    
    /// Add a sequence as a CBioseq.
    /// 
    /// This adds the sequence data in the specified CBioseq to the
    /// database.  If the CBioseq contains deflines, they will also be
    /// used unless there is a call to SetDeflines() or AddDefline().
    /// Note that the CBioseq will be held by CWriteDB at least until
    /// the next sequence is provided.  If this method is used, the
    /// CBioseq is expected to contain sequence data accessible via
    /// GetInst().GetSeq_data().  If this might not be true, it may be
    /// better to use the version of this function that also takes a
    /// CSeqVector.
    /// 
    /// @param bs
    ///   The sequence and related data expressed as a CBioseq.
    void AddSequence(const CBioseq & bs);
    
    /// Add a sequence as a CBioseq.
    /// 
    /// This adds the sequence data in the specified CSeqVector, and
    /// the meta data in the specified CBioseq, to the database.  If
    /// the CBioseq contains deflines, they will also be used unless
    /// there is a call to SetDeflines() or AddDefline().  Note that
    /// the CBioseq will be held by CWriteDB at least until the next
    /// sequence is provided.  This version will use the CSeqVector if
    /// the sequence data is not found in the CBioseq.
    /// 
    /// @param bs
    ///   A CBioseq containing meta data for the sequence.
    /// @param sv
    ///   The sequence data for the sequence.
    void AddSequence(const CBioseq & bs, CSeqVector & sv);
    
    /// Add a sequence as a CBioseq.
    /// 
    /// This adds the sequence found in the given CBioseq_Handle to
    /// the database.
    /// 
    /// @param bs
    ///   The sequence and related data expressed as a CBioseq.
    void AddSequence(const CBioseq_Handle & bsh);
    
    /// Add a sequence as raw data.
    /// 
    /// This adds a sequence provided as raw sequence data.  The raw
    /// data must be (and is assumed to be) encoded correctly for the
    /// format of database being produced.  For protein databases, the
    /// ambiguities string should be empty (and is thus optional).  If
    /// this version of AddSequence() is used, the user must also
    /// provide one or more deflines with SetDeflines() or
    /// AddDefline() calls.
    /// 
    /// @param sequence
    ///   The sequence data as a string of bytes.
    /// @param ambiguities
    ///   The ambiguity data as a string of bytes.
    void AddSequence(const CTempString & sequence,
                     const CTempString & ambiguities = "");
    
    /// Set the PIG to be used for the sequence.
    /// 
    /// For proteins, this sets the PIG of the protein sequence.
    /// 
    /// @param pig
    ///   PIG identifier as an integer.
    void SetPig(int pig);
    
    /// Set the deflines to be used for the sequence.
    /// 
    /// This method sets all the deflines at once as a complete set,
    /// overriding any deflines provided by AddSequence().  If this
    /// method is used with the CBioseq version of AddSequence, it
    /// replaces the deflines found in the CBioseq.
    /// 
    /// @param deflines
    ///   Deflines to use for this sequence.
    void SetDeflines(const CBlast_def_line_set & deflines);
    
    /// Adds defline record for this sequence.
    /// 
    /// The specified meta-data is used to build and append a blast
    /// defline.  This should be called once per defline, but may be
    /// called more than once per sequence.  If this method is used
    /// with the CBioseq version of AddSequence, the deflines provided
    /// with this method replace the deflines found in the CBioseq.
    /// 
    /// @param ids   One or more sequence identifiers.
    /// @param title The defline title.
    /// @param taxid The taxonomic id, or zero for none.
    /// @param membs Sets the memberships for this defline.
    /// @param links Sets linkouts for this defline.
    void AddDefline(const vector< CRef<CSeq_id> > & ids,
                    const CTempString             & title,
                    int                             taxid,
                    const vector<int>             & membs,
                    const vector<int>             & links);
    
    //
    // Output
    //
    
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
    
    /// Close the Database.
    ///
    /// Flush all data to disk and close any open files.
    void Close();
    
    //
    // Controls
    //
    
    // The blast volume format has internal limits for these fields;
    // these are called 'hard limits' here.  If the value specified
    // here exceeds that limit, it will be silently reduced.  Limits
    // are applied simultaneously; creation of a new volume is
    // triggered as soon as any of the limits is reached (unless the
    // current volume is empty).
    
    /// Set maximum size for output files.
    ///
    /// The provided size is applied as a limit on the size of output
    /// files.  If adding a sequence would cause any output file to
    /// exceed this size, the volume is closed and a new volume is
    /// started (unless the current volume is empty, in which case the
    /// size limit is ignored and a one-sequence volume is created).
    /// The default value is 2^30-1.  There is also a hard limit
    /// required by the database format.
    ///
    /// @param sz Maximum size in bytes of any volume component file.
    void SetMaxFileSize(Uint8 sz);
    
    /// Set maximum letters for output volumes.
    ///
    /// The provided size is applied as a limit on the size of output
    /// volumes.  If adding a sequence would cause a volume to exceed
    /// this many protein or nucleotide letters (*not* bytes), the
    /// volume is closed and a new volume is started (unless the
    /// volume is currently empty).  There is no default, but there is
    /// a hard limit required by the format definition.  Ambiguity
    /// encoding is not counted toward this limit.
    ///
    /// @param letters Maximum letters to pack in one volume.
    void SetMaxVolumeLetters(Uint8 letters);
    
    /// Extract Deflines From Bioseq.
    /// 
    /// Deflines are extracted from the CBioseq and returned to the
    /// user.  The caller can then modify or inspect the deflines, and
    /// apply them to a sequence with SetDeflines().
    /// 
    /// @param bioseq
    /// @return A set of deflines for this CBioseq.
    static CRef<CBlast_def_line_set>
    ExtractBioseqDeflines(const CBioseq & bs);
    
    /// Set letters that should not be used in sequences.
    /// 
    /// This method specifies letters that should not be used in the
    /// resulting database.  The masked letters are expected to be
    /// specified in an IUPAC (alphabetic) encoding, and will be
    /// replaced by 'X' (for protein) when the sequences are packed.
    /// This method should be called before any sequences are added.
    /// This method only works with protein (the motivating case
    /// cannot happen with nucleotide).
    /// 
    /// @param masked Letters to disinclude.
    void SetMaskedLetters(const string & masked);
    
protected:
    /// Implementation object.
    struct CWriteDB_Impl * m_Impl;
};


END_NCBI_SCOPE

#endif // OBJTOOLS_WRITERS_WRITEDB__WRITEDB_HPP


