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

/** @file blast_fasta_input.hpp
 * Interface for FASTA files into blast sequence input
 */

#ifndef ALGO_BLAST_BLASTINPUT___BLAST_FASTA_INPUT__HPP
#define ALGO_BLAST_BLASTINPUT___BLAST_FASTA_INPUT__HPP

#include <algo/blast/blastinput/blast_input.hpp>

BEGIN_NCBI_SCOPE

/// Class representing a text file containing sequences
/// in fasta format
///
class CBlastFastaInputSource : public CBlastInputSource
{
public:

    /// Constructor
    /// @param objmgr Object Manager instance
    /// @param infile The file to read
    /// @param strand All SeqLoc types will have this strand assigned [in]
    /// @param lowercase If true, lowercase mask locations are generated
    ///                 for all input sequences [in]
    /// @param believe_defline If true, all sequences ID's are parsed;
    ///                 otherwise all sequences receive a local ID set
    ///                 to a monotonically increasing count value [in]
    /// @param from All sequence locations start at this offset [in]
    /// @param to All sequence locations end at this offset (end of sequence
    ///           if zero) [in]
    ///
    CBlastFastaInputSource(objects::CObjectManager& objmgr,
                   CNcbiIstream& infile,
                   objects::ENa_strand strand = objects::eNa_strand_other,
                   bool lowercase = false,
                   bool believe_defline = false,
                   TSeqPos from = 0,
                   TSeqPos to = 0);

    /// Destructor
    ///
    virtual ~CBlastFastaInputSource() {}

    /// Retrieve a single sequence (in an SSeqLoc container)
    ///
    virtual blast::SSeqLoc GetNextSSeqLoc();

    /// Retrieve a single sequence (in a CBlastSearchQuery container)
    ///
    virtual CRef<blast::CBlastSearchQuery> GetNextSequence();

    /// Signal whether there are any unread sequences left
    /// @return true if no unread sequences remaining
    ///
    virtual bool End();

    /// Configuration for the sequences to be read
    CBlastInputConfig m_Config;

private:
    CNcbiIstream& m_InputFile;  ///< reference to input file
    int m_Counter;              ///< counter for local sequence IDs

    /// Read a single sequence from file and convert to
    /// a Seq_loc
    /// @param lcase_mask If not null, the first element of the 
    ///           vector is filled with a Seq_loc that describes the
    ///           lowercase-masked regions in the query that was read in.
    ///           If there are no such locations, the Seq_loc is of type
    ///           'null', otherwise it is of type 'packed_seqint'
    /// @return The sequence in Seq_loc format
    ///
    CRef<objects::CSeq_loc> x_FastaToSeqLoc(
                          vector<CConstRef<objects::CSeq_loc> > *lcase_mask);
};

END_NCBI_SCOPE

#endif  /* ALGO_BLAST_BLASTINPUT___BLAST_FASTA_INPUT__HPP */
