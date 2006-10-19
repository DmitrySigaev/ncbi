#ifndef OBJTOOLS_ALNMGR___PAIRWISE_ALN__HPP
#define OBJTOOLS_ALNMGR___PAIRWISE_ALN__HPP
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
* Authors:  Kamen Todorov, NCBI
*
* File Description:
*   Pairwise and query-anchored alignments
*
* ===========================================================================
*/


#include <corelib/ncbistd.hpp>
#include <corelib/ncbiobj.hpp>

#include <util/align_range.hpp>
#include <util/align_range_coll>


BEGIN_NCBI_SCOPE
USING_SCOPE(objects);


/// A pair of sequences
template <TSeq>
class CPairwiseAln : public CObject
{
public:
    CPairwiseAln(const TSeq& first_seq,
                 const TSeq& second_seq,
                 bool negative_strand = false,
                 TAlnRngColl* aln_rng_coll = NULL)
        m_First(first_seq),
        m_Second(second_seq),
        m_AlnRngColl(aln_rng_coll), 
        m_NegativeStrand(negative_strand)
    {}

    ~CPairwiseAln()
    {
        delete m_AlnRngColl;
    }

    typedef TSignedSeqPos TPos;
    typedef CAlignRange<TPos> TAlnRng;
    typedef CAlignRangeCollection<TAlnRng> TAlnRngColl;

    TAlnRngColl& GetAlnRngColl() {
        if (m_AlnRngColl) {
            return *m_AlnRngColl;
        } else {
            NCBI_THROW(CException, eUnknown,
                       "Align range collection does not exist.");
        }
    }

    const TSeq& GetFirst() {
        return m_First;
    }

    const TSeq& GetSecond() {
        return m_Second;
    }

private:
    typedef SAlignTools::TSignedRange TSignedRange;

    TSeq         m_First;
    TSeq         m_Second;

    TAlnRngColl* m_AlnRngColl;     //< sequence mapping to the alignment
    TSignedRange m_SecondRange;    //< range of the segments on the sequence
    bool         m_NegativeStrand;  
};


/// Pair of rows
typedef CPairwiseAln<int> CRowAln;


/// Query-anchored alignment can be 2 or multi-dimentional
class CAnchoredAln : public CObject
{
    CConstRef<CSeq_align> m_SeqAlign;
    
    typedef vector<CConstRef<CSeq_id> > TRowIdVector;
    TRowIdVector m_RowIdVector;

    typedef vector<CRowAln> TRowAlnContainer;
};



END_NCBI_SCOPE


/*
* ===========================================================================
*
* $Log$
* Revision 1.2  2006/10/19 17:11:05  todorov
* Minor refactoring.
*
* Revision 1.1  2006/10/17 19:59:36  todorov
* Initial revision
*
* ===========================================================================
*/

#endif  // OBJTOOLS_ALNMGR___PAIRWISE_ALN__HPP
