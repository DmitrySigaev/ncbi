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


class NCBI_XALNMGR_EXPORT CAlnVec : public CAlnMap
{
    typedef CAlnMap                         Tparent;
    typedef CSeqVector::TResidue            TResidue;
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

    // GetSeqString methods
    // in seq coords
    string& GetSeqString   (string& buffer,
                            TNumrow row,
                            TSeqPos seq_from, TSeqPos seq_to)             const;
    string& GetSeqString   (string& buffer,
                            TNumrow row,
                            const CAlnMap::TRange& seq_rng)               const;
    string& GetSegSeqString(string& buffer, 
                            TNumrow row, 
                            TNumseg seg, TNumseg offset = 0) const;
    // in aln coords
    string& GetAlnSeqString(string& buffer,
                            TNumrow row, 
                            const CAlnMap::TSignedRange& aln_rng)         const;

    const CBioseq_Handle& GetBioseqHandle(TNumrow row)                  const;
    TResidue              GetResidue     (TNumrow row, TSeqPos aln_pos) const;


    // gap character could be explicitely set otherwise taken from seqvector
    void     SetGapChar(TResidue gap_char);
    void     UnsetGapChar();
    bool     IsSetGapChar()                const;
    TResidue GetGapChar(TNumrow row)       const;

    // end character is ' ' by default
    void     SetEndChar(TResidue gap_char);
    bool     IsSetEndChar()                const;
    TResidue GetEndChar()                  const;

    // functions for manipulating the consensus sequence
    void    CreateConsensus     (void);
    bool    IsSetConsensus      (void) const;
    TNumrow GetConsensusRow     (void) const;

    // utilities
    int CalculateScore(TNumrow row1, TNumrow row2);

    // static utilities
    static int  CalculateScore(const string& s1, const string& s2,
                               bool s1_is_prot, bool s2_is_prot);
    
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

    TResidue m_GapChar;
    bool     m_set_GapChar;
    TResidue m_EndChar;
    bool     m_set_EndChar;
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
    if (aln_pos > GetAlnStop()) {
        return (TResidue) 0; // out of range
    }
    TSegTypeFlags type = GetSegType(row, GetSeg(aln_pos));
    if (type & fSeq) {
        return x_GetSeqVector(row)[GetSeqPosFromAlnPos(row, aln_pos)];
    } else {
        if (type & fNoSeqOnLeft  ||  type & fNoSeqOnRight) {
            return GetEndChar();
        } else {
            return GetGapChar(row);
        }
    }
}


inline
string& CAlnVec::GetSeqString(string& buffer,
                              TNumrow row,
                              TSeqPos seq_from, TSeqPos seq_to) const
{
    x_GetSeqVector(row).GetSeqData(seq_from, seq_to + 1, buffer);
    return buffer;
}


inline
string& CAlnVec::GetSegSeqString(string& buffer,
                                 TNumrow row,
                                 TNumseg seg, int offset) const
{
    x_GetSeqVector(row).GetSeqData(GetStart(row, seg, offset),
                                   GetStop (row, seg, offset) + 1,
                                   buffer);
    return buffer;
}


inline
string& CAlnVec::GetSeqString(string& buffer,
                              TNumrow row,
                              const CAlnMap::TRange& seq_rng) const
{
    x_GetSeqVector(row).GetSeqData(seq_rng.GetFrom(), seq_rng.GetTo() + 1, buffer);
    return buffer;
}


inline
void CAlnVec::SetGapChar(TResidue gap_char)
{
    m_GapChar = gap_char;
    m_set_GapChar = true;
}

inline
void CAlnVec::UnsetGapChar()
{
    m_set_GapChar = false;
}

inline
bool CAlnVec::IsSetGapChar() const
{
    return m_set_GapChar;
}

inline
CSeqVector::TResidue CAlnVec::GetGapChar(TNumrow row) const
{
    if (IsSetGapChar()) {
        return m_GapChar;
    } else {
        return x_GetSeqVector(row).GetGapChar();
    }
}

inline
void CAlnVec::SetEndChar(TResidue end_char)
{
    m_EndChar = end_char;
    m_set_EndChar = true;
}

inline
bool CAlnVec::IsSetEndChar() const
{
    return m_set_EndChar;
}

inline
CSeqVector::TResidue CAlnVec::GetEndChar() const
{
    if (IsSetEndChar()) {
        return m_EndChar;
    } else {
        return ' ';
    }
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
 * Revision 1.17  2003/01/23 21:30:50  todorov
 * Removing the original, inefficient GetXXXString methods
 *
 * Revision 1.16  2003/01/23 16:31:56  todorov
 * Added calc score methods
 *
 * Revision 1.15  2003/01/22 22:54:04  todorov
 * Removed an obsolete function
 *
 * Revision 1.14  2003/01/17 18:17:29  todorov
 * Added a better-performing set of GetXXXString methods
 *
 * Revision 1.13  2003/01/16 20:46:30  todorov
 * Added Gap/EndChar set flags
 *
 * Revision 1.12  2002/12/26 12:38:08  dicuccio
 * Added Win32 export specifiers
 *
 * Revision 1.11  2002/10/21 19:15:12  todorov
 * added GetAlnSeqString
 *
 * Revision 1.10  2002/10/04 17:31:36  todorov
 * Added gap char and end char
 *
 * Revision 1.9  2002/09/27 17:01:16  todorov
 * changed order of params for GetSeqPosFrom{Seq,Aln}Pos
 *
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
