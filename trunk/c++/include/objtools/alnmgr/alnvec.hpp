#ifndef OBJECTS_ALNMGR___ALNVEC__HPP
#define OBJECTS_ALNMGR___ALNVEC__HPP

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
 *   Access to the actual aligned residues
 *
 */


#include <objects/alnmgr/alnmap.hpp>
#include <objects/objmgr/seq_vector.hpp>

BEGIN_NCBI_SCOPE
BEGIN_objects_SCOPE


// forward declarations
class CObjectManager;
class CScope;


class CAlnVec : public CAlnMap
{
    typedef CAlnMap                         Tparent;
    typedef map<TNumrow, CBioseq_Handle>    TBioseqHandleCache;
    typedef map<TNumrow, CRef<CSeqVector> > TSeqVectorCache;

public:
    // constructor
    CAlnVec(const CDense_seg& ds);
    CAlnVec(const CDense_seg& ds, TNumrow anchor);
    CAlnVec(const CDense_seg& ds, CScope& scope);
    CAlnVec(const CDense_seg& ds, TNumrow anchor, CScope& scope);

    // destructor
    ~CAlnVec(void);

    CScope& GetScope(void) const;

    string GetSeqString   (TNumrow row, TSeqPos from, TSeqPos to)        const;
    string GetSeqString   (TNumrow row, const CAlnMap::TRange& range)    const;
    string GetSegSeqString(TNumrow row, TNumseg seg, TNumseg offset = 0) const;

    const CBioseq_Handle& GetBioseqHandle(TNumrow row)                  const;
    CSeqVector::TResidue  GetResidue     (TNumrow row, TSeqPos aln_pos) const;

    // functions for manipulating the consensus sequence
    void    CreateConsensus     (void);
    bool    IsSetConsensus      (void) const;
    TNumrow GetConsensusRow     (void) const;

    // temporaries for conversion (see note below)
    static unsigned char FromIupac(unsigned char c);
    static unsigned char ToIupac  (unsigned char c);

protected:

    CSeqVector& x_GetSeqVector         (TNumrow row) const;
    CSeqVector& x_GetConsensusSeqVector(void)        const;

    mutable CRef<CObjectManager>    m_ObjMgr;
    mutable CRef<CScope>            m_Scope;
    mutable TBioseqHandleCache      m_BioseqHandlesCache;
    mutable TSeqVectorCache         m_SeqVectorCache;

    // our consensus sequence: a bioseq object and a vector of starts
    TNumrow                         m_ConsensusSeq;

private:
    // Prohibit copy constructor and assignment operator
    CAlnVec(const CAlnVec&);
    CAlnVec& operator=(const CAlnVec&);

};




/////////////////////////////////////////////////////////////////////////////
//  IMPLEMENTATION of INLINE functions
/////////////////////////////////////////////////////////////////////////////

inline
bool CAlnVec::IsSetConsensus(void) const
{
    return (m_ConsensusSeq != -1);
}

inline
CAlnVec::TNumrow CAlnVec::GetConsensusRow(void) const
{
    if ( !IsSetConsensus() ) {
        NCBI_THROW(CAlnException, eConsensusNotPresent,
                   "CAlnVec::GetConsensusRow(): "
                   "consensus sequence not present");
    }

    return m_ConsensusSeq;
}

inline 
CSeqVector::TResidue CAlnVec::GetResidue(TNumrow row, TSeqPos aln_pos) const
{
    return x_GetSeqVector(row)[GetSeqPosFromAlnPos(aln_pos, row)];
}


//
// these are a temporary work-around
// CSeqportUtil contains routines for converting sequence data from one
// format to another, but it places a requirement on the data: it must in
// a CSeq_data class.  I can get this for my data, but it is a bit of work
// (much more work than calling CSeqVector::GetSeqdata(), which, if I use the
// internal sequence vector, is guaranteed to be in IUPAC notation)
//
inline
unsigned char CAlnVec::FromIupac(unsigned char c)
{
    switch (c)
    {
    case 'A': return 0x01;
    case 'C': return 0x02;
    case 'M': return 0x03;
    case 'G': return 0x04;
    case 'R': return 0x05;
    case 'S': return 0x06;
    case 'V': return 0x07;
    case 'T': return 0x08;
    case 'W': return 0x09;
    case 'Y': return 0x0a;
    case 'H': return 0x0b;
    case 'K': return 0x0c;
    case 'D': return 0x0d;
    case 'B': return 0x0e;
    case 'N': return 0x0f;
    }

    return 0x00;
}

inline unsigned char CAlnVec::ToIupac(unsigned char c)
{
    const char *data = "-ACMGRSVTWYHKDBN";
    return ((c < 16) ? data[c] : 0);
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
 * Revision 1.8  2002/09/25 19:34:15  todorov
 * "un-inlined" x_GetSeqVector
 *
 * Revision 1.7  2002/09/25 18:16:26  dicuccio
 * Reworked computation of consensus sequence - this is now stored directly
 * in the underlying CDense_seg
 * Added exception class; currently used only on access of non-existent
 * consensus.
 *
 * Revision 1.6  2002/09/23 18:22:45  vakatov
 * Workaround an older MSVC++ compiler bug.
 * Get rid of "signed/unsigned" comp.warning, and some code prettification.
 *
 * Revision 1.5  2002/09/19 18:22:28  todorov
 * New function name for GetSegSeqString to avoid confusion
 *
 * Revision 1.4  2002/09/19 18:19:18  todorov
 * fixed unsigned to signed return type for GetConsensusSeq{Start,Stop}
 *
 * Revision 1.3  2002/09/05 19:31:18  dicuccio
 * - added ability to reference a consensus sequence for a given alignment
 * - added caching of CSeqVector (big performance win)
 * - many small bugs fixed
 *
 * Revision 1.2  2002/08/29 18:40:53  dicuccio
 * added caching mechanism for CSeqVector - this greatly improves speed in
 * accessing sequence data.
 *
 * Revision 1.1  2002/08/23 14:43:50  ucko
 * Add the new C++ alignment manager to the public tree (thanks, Kamen!)
 *
 *
 * ===========================================================================
 */

#endif  /* OBJECTS_ALNMGR___ALNVEC__HPP */
