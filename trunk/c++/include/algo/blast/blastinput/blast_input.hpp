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
 * Author:  Jason Papadopoulos
 *
 */

/** @file blast_input.hpp
 * Interface for converting sources of sequence data into
 * blast sequence input
 */

#ifndef ALGO_BLAST_BLASTINPUT___BLAST_INPUT__HPP
#define ALGO_BLAST_BLASTINPUT___BLAST_INPUT__HPP

#include <corelib/ncbistd.hpp>
#include <algo/blast/api/sseqloc.hpp>

BEGIN_NCBI_SCOPE

/// Class that centralizes the configuration data for
/// sequences to be converted
///
class CBlastInputConfig {

public:

    /// Constructor
    /// @param strand All SeqLoc types will have this strand assigned;
    ///             If set to 'other', the strand will be set to 'unknown'
    ///             for protein sequences and 'both' for nucleotide [in]
    /// @param lowercase If true, lowercase mask locations are generated
    ///                 for all input sequences [in]
    /// @param believe_defline If true, all sequences ID's are parsed;
    ///                 otherwise all sequences receive a local ID set
    ///                 to a monotonically increasing count value [in]
    /// @param from All sequence locations start at this offset [in]
    /// @param to All sequence locations end at this offset (end of sequence
    ///           if zero) [in]
    ///
    CBlastInputConfig(objects::ENa_strand strand = objects::eNa_strand_other,
                     bool lowercase = false,
                     bool believe_defline = false,
                     TSeqPos from = 0,
                     TSeqPos to = 0)
        : m_Strand(strand), m_LowerCaseMask(lowercase),
          m_BelieveDeflines(believe_defline), 
          m_From(from), m_To(to) {}

    /// Destructor
    ///
    ~CBlastInputConfig() {}

    /// Set the strand to a specified value
    /// @param strand The strand value
    ///
    void SetStrand(objects::ENa_strand strand) { m_Strand = strand; }

    /// Retrieve the current strand value
    /// @return the strand
    objects::ENa_strand GetStrand() const { return m_Strand; }

    /// Turn lowercase masking on/off
    /// @param mask boolean to toggle lowercase masking
    ///
    void SetLowercaseMask(bool mask) { m_LowerCaseMask = mask; }

    /// Retrieve lowercase mask status
    /// @return boolean to toggle lowercase masking
    ///
    bool GetLowercaseMask() const { return m_LowerCaseMask; }

    /// Turn parsing of sequence IDs on/off
    /// @param believe boolean to toggle parsing of seq IDs
    ///
    void SetBelieveDeflines(bool believe) { m_BelieveDeflines = believe; }

    /// Retrieve current sequence ID parsing status
    /// @return boolean to toggle parsing of seq IDs
    ///
    bool GetBelieveDeflines() const { return m_BelieveDeflines; }

    /// Set start offset of all sequences
    /// @param from Start offset
    ///
    void SetFrom(TSeqPos from) { m_From = from; }

    /// Get current start offset of all sequences
    /// @return Current start offset
    ///
    TSeqPos GetFrom() const { return m_From; }

    /// Set end offset of all sequences
    /// @param to Start offset
    ///
    void SetTo(TSeqPos to) { m_To = to; }

    /// Get current end offset of all sequences
    /// @return Current end offset
    ///
    TSeqPos GetTo() const { return m_To; }

private:
    objects::ENa_strand m_Strand;  ///< strand to assign to sequences
    bool m_LowerCaseMask;          ///< whether to save lowercase mask locs
    bool m_BelieveDeflines;        ///< whether to parse sequence IDs
    TSeqPos m_From;                ///< sequence start offsets
    TSeqPos m_To;                  ///< sequence end offsets
};


/// Base class representing a source of biological sequences
///
class CBlastInputSource : public CObject
{
public:
    /// Constructor
    /// @param objmgr Object Manager instance
    ///
    CBlastInputSource(objects::CObjectManager& objmgr);

    /// Destructor
    ///
    virtual ~CBlastInputSource() {}

    /// Retrieve a single sequence (in an SSeqLoc container)
    ///
    virtual blast::SSeqLoc GetNextSSeqLoc() = 0;

    /// Retrieve a single sequence (in a CBlastSearchQuery container)
    ///
    virtual CRef<blast::CBlastSearchQuery> GetNextSequence() = 0;

    /// Signal whether there are any unread sequence left
    /// @return true if no unread sequences remaining
    ///
    virtual bool End() = 0;

protected:
    objects::CObjectManager& m_ObjMgr;  ///< object manager instance
    CRef<objects::CScope> m_Scope;      ///< scope instance (local to object)
};


/// Generalized converter from an abstract source of
/// biological sequence data to collections of blast input
///
class NCBI_BLAST_EXPORT CBlastInput : public CObject
{
public:
    /// Constructor
    /// @param source Pointer to abstract source of sequences
    /// @param batch_size A hint specifying how many letters should
    ///               be in a batch of converted sequences
    ///
    CBlastInput(CBlastInputSource *source, int batch_size = kMax_Int)
        : m_Source(source), m_BatchSize(batch_size) {}

    /// Destructor
    ///
    ~CBlastInput() {}

    /// Read and convert all the sequences from the source
    /// @return The converted sequences
    ///
    blast::TSeqLocVector GetAllSeqLocs();

    /// Read and convert all the sequences from the source
    /// @return The converted sequences
    ///
    CRef<blast::CBlastQueryVector> GetAllSeqs();

    /// Read and convert the next batch of sequences
    /// @return The next batch of sequence. The size of the batch is
    ///        either all remaining sequences, or the size of sufficiently
    ///        many whole sequences whose combined size exceeds m_BatchSize,
    ///        whichever is smaller
    ///
    blast::TSeqLocVector GetNextSeqLocBatch();

    /// Read and convert the next batch of sequences
    /// @return The next batch of sequence. The size of the batch is
    ///        either all remaining sequences, or the size of sufficiently
    ///        many whole sequences whose combined size exceeds m_BatchSize,
    ///        whichever is smaller
    ///
    CRef<blast::CBlastQueryVector> GetNextSeqBatch();

    /// Set the target size of a batch of sequences
    /// @param batch_size The desired total size of all the 
    ///                 sequences in future batches
    ///                  
    void SetBatchSize(TSeqPos batch_size) { m_BatchSize = batch_size; }

    /// Retrieve the target size of a batch of sequences
    /// @return The current batch size
    ///                  
    TSeqPos GetBatchSize() const { return m_BatchSize; }

private:
    CBlastInputSource *m_Source;  ///< pointer to source of sequences
    TSeqPos m_BatchSize;          ///< total size of one block of sequences
};

END_NCBI_SCOPE

#endif  /* ALGO_BLAST_BLASTINPUT___BLAST_INPUT__HPP */
