#ifndef OBJECTS_ALNMGR___ALNMIX__HPP
#define OBJECTS_ALNMGR___ALNMIX__HPP

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
* Author:  Kamen Todorov, NCBI
*
* File Description:
*   Alignment merger
*
*/

#include <objects/alnmgr/alnvec.hpp>
#include <objects/seqalign/Seq_align.hpp>

BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::


// forward declarations
class CScope;
class CObjectManager;

class CAlnMixSegment;
class CAlnMixSeq;
class CAlnMixMatch;

class CAlnMix : public CObject
{
public:

    typedef vector< CConstRef< CDense_seg > > TConstDSs;
    

    // constructor
    CAlnMix(void);
    CAlnMix(CScope& scope);

    // destructor
    ~CAlnMix(void);

    void Add(const CDense_seg& ds);

    enum EMergeFlags {
        fGen2EST              = 0x01, // otherwise Nucl2Nucl
        fTruncateOverlaps     = 0x02, // otherwise put on separate rows
        fNegativeStrand       = 0x04,
        fTryOtherMethodOnFail = 0x08,
        fGapJoin              = 0x10
    };
    typedef int TMergeFlags; // binary OR of EMergeFlags
    void Merge(TMergeFlags flags = 0);

    CScope&            GetScope(void)  const;
    const CDense_seg&  GetDenseg(void) const;

private:
    // Prohibit copy constructor and assignment operator
    CAlnMix(const CAlnMix& value);
    CAlnMix& operator=(const CAlnMix& value);

    typedef vector<CAlnMixSeq*>                    TSeqs;
    typedef map<CBioseq_Handle, CRef<CAlnMixSeq> > TBioseqHandleMap;
    typedef vector<CRef<CAlnMixMatch> >            TMatches;
    typedef vector<CAlnMixSegment*>                TSegments;

    void x_InitBlosum62Map     (void);
    void x_CreateScope         (void);
    void x_Merge               (void);
    bool x_SecondRowFits       (const CAlnMixMatch * match) const;
    void x_CreateRowsVector    (void);
    void x_CreateSegmentsVector(void);
    void x_CreateDenseg        (void);
    int  x_CalculateScore      (const string& s1, const string& s2,
                                bool s1_is_prot, bool s2_is_prot);
    void x_ConsolidateGaps     (TSegments& gapped_segs);


    static bool x_CompareAlnSeqScores  (const CAlnMixSeq* aln_seq1,
                                        const CAlnMixSeq* aln_seq2);
    static bool x_CompareAlnMatchScores(const CAlnMixMatch* aln_match1,
                                        const CAlnMixMatch* aln_match2);
    static bool x_CompareAlnSegIndexes (const CAlnMixSegment* aln_seg1,
                                        const CAlnMixSegment* aln_seg2);

    CRef<CObjectManager>        m_ObjMgr;
    mutable CRef<CScope>        m_Scope;
    TConstDSs                   m_InputDSs;
    CRef<CDense_seg>            m_DS;
    TMergeFlags                 m_MergeFlags;
    TSeqs                       m_Seqs;
    TMatches                    m_Matches;
    TSegments                   m_Segments;
    TSeqs                       m_Rows;
    list<CRef<CAlnMixSeq> >     m_ExtraRows;
    bool                        m_SingleRefseq;
    map<char, map<char, int> >  m_Blosum62Map;
    TBioseqHandleMap            m_BioseqHandles;

};


///////////////////////////////////////////////////////////
///////////////////// Helper Classes //////////////////////
///////////////////////////////////////////////////////////

class CAlnMixSeq : public CObject
{
public:
    CAlnMixSeq(void) 
        : m_DS_Count(0),
          m_Score(0),
          m_Factor(1),
          m_RefBy(0),
          m_RefseqCandidate(false),
          m_ExtraRow(0)
    {};

    typedef map<TSeqPos, CRef<CAlnMixSegment> > TStarts;

    int                   m_DS_Count;
    const CBioseq_Handle* m_BioseqHandle;
    int                   m_Score;
    bool                  m_IsAA;
    int                   m_Factor;
    bool                  m_PositiveStrand;
    TStarts               m_Starts;
    CAlnMixSeq *          m_RefBy;
    bool                  m_RefseqCandidate;
    CAlnMixSeq *          m_ExtraRow;
    int                   m_RowIndex;
    int                   m_SeqIndex;
    TStarts::iterator     m_StartIt;

    CSeqVector& GetSeqVector(void) {
        if ( !m_SeqVector ) {
            m_SeqVector = new CSeqVector
                (m_BioseqHandle->GetSeqVector
                 (CBioseq_Handle::eCoding_Iupac,
                  CBioseq_Handle::eStrand_Plus));
        }
        return *m_SeqVector;
    }
private:
    CRef<CSeqVector> m_SeqVector;
};


class CAlnMixSegment : public CObject
{
public:
    typedef map<CAlnMixSeq*, CAlnMixSeq::TStarts::iterator> TStartIterators;
        
    TSeqPos         m_Len;
    TStartIterators m_StartIts;
    int             m_Index1;
    int             m_Index2;
};


class CAlnMixMatch : public CObject
{
public:
    CAlnMixMatch(void)
        : m_Score(0), m_Start1(0), m_Start2(0),
          m_Len(0), m_StrandsDiffer(false)
    {};
        
    int           m_Score;
    CAlnMixSeq    * m_AlnSeq1, * m_AlnSeq2;
    TSeqPos       m_Start1, m_Start2, m_Len;
    bool          m_StrandsDiffer;
};


///////////////////////////////////////////////////////////
///////////////////// inline methods //////////////////////
///////////////////////////////////////////////////////////

inline
CScope& CAlnMix::GetScope() const
{
    return const_cast<CScope&>(*m_Scope);
}


inline
const CDense_seg& CAlnMix::GetDenseg() const
{
    if (!m_DS) {
        NCBI_THROW(CAlnException, eMergeFailure,
                   "CAlnMix::GetDenseg(): "
                   "Dense_seg is not available until after Merge()");
    }
    return *m_DS;
}


///////////////////////////////////////////////////////////
////////////////// end of inline methods //////////////////
///////////////////////////////////////////////////////////

END_objects_SCOPE // namespace ncbi::objects::

END_NCBI_SCOPE

/*
* ===========================================================================
*
* $Log$
* Revision 1.8  2002/12/19 00:06:55  todorov
* Added optional consolidation of segments that are gapped on the query.
*
* Revision 1.7  2002/12/18 03:47:05  todorov
* created an algorithm for mixing alignments that share a single sequence.
*
* Revision 1.6  2002/11/08 19:43:33  grichenk
* CConstRef<> constructor made explicit
*
* Revision 1.5  2002/11/04 21:28:57  grichenk
* Fixed usage of const CRef<> and CRef<> constructor
*
* Revision 1.4  2002/10/25 20:02:20  todorov
* new fTryOtherMethodOnFail flag
*
* Revision 1.3  2002/10/24 21:31:33  todorov
* adding Dense-segs instead of Seq-aligns
*
* Revision 1.2  2002/10/08 18:02:09  todorov
* changed the aln lst input param
*
* Revision 1.1  2002/08/23 14:43:50  ucko
* Add the new C++ alignment manager to the public tree (thanks, Kamen!)
*
*
* ===========================================================================
*/

#endif // OBJECTS_ALNMGR___ALNMIX__HPP
