#ifndef ALGO_BLAST_API___MULTISEQ_SRC__HPP
#define ALGO_BLAST_API___MULTISEQ_SRC__HPP

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
* Author:  Ilya Dondoshansky
*
*/

/// @file multiseq_src.hpp
/// Implementation of the BlastSeqSrc interface for a vector of sequence 
/// locations.


#include <algo/blast/core/blast_seqsrc.h>
#include <algo/blast/core/blast_def.h>
#include <algo/blast/core/blast_message.h>
#include <algo/blast/api/blast_types.hpp>
#include <algo/blast/api/blast_exception.hpp>

/** @addtogroup Miscellaneous
 *
 * @{
 */

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
//USING_SCOPE(blast);
BEGIN_SCOPE(blast)

/// Encapsulates the arguments needed to initialize multi-sequence source.
typedef struct MultiSeqSrcNewArgs {
    TSeqLocVector seq_vector;
    EProgram program;
} MultiSeqSrcNewArgs;

/// Contains information about all sequences in a set
class MultiSeqInfo : public CObject 
{
public: 
    /// Constructor from a vector of sequence location/scope pairs and a 
    /// BLAST program type.
    MultiSeqInfo(const TSeqLocVector& seq_vector, EProgram program);
    ~MultiSeqInfo();
    /// Setter and getter functions for the private fields
    Uint4 GetMaxLength();
    void SetMaxLength(Uint4 val);
    Uint4 GetAvgLength();
    void SetAvgLength(Uint4 val);
    bool GetIsProtein();
    Uint4 GetNumSeqs();
    BLAST_SequenceBlk* GetSeqBlk(int index);
    void* GetSeqId(int index);
    void* GetSeqLoc(int index);
    Blast_Message* GetErrorMessage();
private:
    /// Passed from outside - not owned:
    TSeqLocVector m_vSeqVector; ///< Vector of sequence locations
    /// Internal fields
    bool m_ibIsProt; ///< Are these sequences protein or nucleotide? 
    vector<BLAST_SequenceBlk*> m_ivSeqBlkVec; ///< Vector of sequence blocks
    unsigned int m_iMaxLength; ///< Length of the longest sequence in this set
    unsigned int m_iAvgLength; ///< Average length of sequences in this set
    Blast_Message* m_icErrMsg; ///< Error message in case of set up failure
};

inline Uint4 MultiSeqInfo::GetMaxLength()
{
    return m_iMaxLength;
}

inline void MultiSeqInfo::SetMaxLength(Uint4 length)
{
    m_iMaxLength = length;
}

inline Uint4 MultiSeqInfo::GetAvgLength()
{
    return m_iAvgLength;
}

inline void MultiSeqInfo::SetAvgLength(Uint4 length)
{
    m_iAvgLength = length;
}

inline bool MultiSeqInfo::GetIsProtein()
{
    return m_ibIsProt;
}

inline Uint4 MultiSeqInfo::GetNumSeqs()
{
    return m_ivSeqBlkVec.size();
}

inline BLAST_SequenceBlk* MultiSeqInfo::GetSeqBlk(int index)
{
    return m_ivSeqBlkVec[index];
}

inline Blast_Message* MultiSeqInfo::GetErrorMessage()
{
    return m_icErrMsg;
}

/** Multi-sequence source constructor 
 * @param seq_src BlastSeqSrc structure (already allocated) to populate [in]
 * @param args Pointer to MultiSeqSrcNewArgs structure above [in]
 * @return Updated bssp structure (with all function pointers initialized
 */
BlastSeqSrc* MultiSeqSrcNew(BlastSeqSrc* seq_src, void* args);

/** Initialize the sequence source structure.
 * @param seq_vector Vector of sequence locations [in]
 * @param program Type of BLAST to be performed [in]
 */
BlastSeqSrc* 
MultiSeqSrcInit(const TSeqLocVector& seq_vector, EProgram program);

/** Multi sequence source destructor: frees its internal data structure and the
 * BlastSeqSrc structure itself.
 * @param seq_src BlastSeqSrc structure to free [in]
 * @return NULL
 */
BlastSeqSrc* MultiSeqSrcFree(BlastSeqSrc* seq_src);

END_SCOPE(blast)
END_NCBI_SCOPE

/* @} */

/*
 * ===========================================================================
 * $Log$
 * Revision 1.2  2004/03/15 18:34:19  dondosha
 * Made doxygen comments and top #ifndef adhere to toolkit standard
 *
 * ===========================================================================
 */

#endif /* ALGO_BLAST_API___MULTISEQ_SRC__HPP */
