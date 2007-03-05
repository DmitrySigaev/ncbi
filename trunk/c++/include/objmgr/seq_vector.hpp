#ifndef SEQ_VECTOR__HPP
#define SEQ_VECTOR__HPP

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
* Author: Aleksey Grichenko, Michael Kimelman, Eugene Vasilchenko
*
* File Description:
*   Sequence data container for object manager
*
*/

#include <objmgr/bioseq_handle.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/seq_map.hpp>
#include <objmgr/seq_vector_ci.hpp>

BEGIN_NCBI_SCOPE

/** @addtogroup ObjectManagerSequenceRep
 *
 * @{
 */

class CRandom;

BEGIN_SCOPE(objects)

class CScope;
class CSeq_loc;
class CSeqMap;
class CSeqVector_CI;
class CNcbi2naRandomizer;

/////////////////////////////////////////////////////////////////////////////
///
///  CSeqVector --
///
///  Provide sequence data in the selected coding

class NCBI_XOBJMGR_EXPORT CSeqVector : public CObject, public CSeqVectorTypes
{
public:
    typedef CBioseq_Handle::EVectorCoding EVectorCoding;
    typedef CSeqVector_CI const_iterator;

    CSeqVector(void);
    CSeqVector(const CBioseq_Handle& bioseq,
               EVectorCoding coding = CBioseq_Handle::eCoding_Ncbi,
               ENa_strand strand = eNa_strand_unknown);
    CSeqVector(const CSeqMap& seqMap, CScope& scope,
               EVectorCoding coding = CBioseq_Handle::eCoding_Ncbi,
               ENa_strand strand = eNa_strand_unknown);
    CSeqVector(const CSeqMap& seqMap, const CTSE_Handle& top_tse,
               EVectorCoding coding = CBioseq_Handle::eCoding_Ncbi,
               ENa_strand strand = eNa_strand_unknown);
    CSeqVector(const CSeq_loc& loc, CScope& scope,
               EVectorCoding coding = CBioseq_Handle::eCoding_Ncbi,
               ENa_strand strand = eNa_strand_unknown);
    CSeqVector(const CSeq_loc& loc, const CTSE_Handle& top_tse,
               EVectorCoding coding = CBioseq_Handle::eCoding_Ncbi,
               ENa_strand strand = eNa_strand_unknown);
    CSeqVector(const CSeqVector& vec);

    virtual ~CSeqVector(void);

    CSeqVector& operator= (const CSeqVector& vec);

    bool empty(void) const;
    TSeqPos size(void) const;
    /// 0-based array of residues
    TResidue operator[] (TSeqPos pos) const;

    /// true if sequence at 0-based position 'pos' has gap
    bool IsInGap(TSeqPos pos) const;

    /// Fill the buffer string with the sequence data for the interval
    /// [start, stop).
    void GetSeqData(TSeqPos start, TSeqPos stop, string& buffer) const;
    void GetSeqData(const const_iterator& start,
                    const const_iterator& stop,
                    string& buffer) const;

    typedef CSeq_inst::TMol TMol;

    TMol GetSequenceType(void) const;
    bool IsProtein(void) const;
    bool IsNucleotide(void) const;

    CScope& GetScope(void) const;
    const CSeqMap& GetSeqMap(void) const;
    ENa_strand GetStrand(void) const;

    /// Target sequence coding. CSeq_data::e_not_set -- do not
    /// convert sequence (use GetCoding() to check the real coding).
    TCoding GetCoding(void) const;
    void SetCoding(TCoding coding);
    /// Set coding to either Iupacaa or Iupacna depending on molecule type
    void SetIupacCoding(void);
    /// Set coding to either Ncbi8aa or Ncbi8na depending on molecule type
    void SetNcbiCoding(void);
    /// Set coding to either Iupac or Ncbi8xx
    void SetCoding(EVectorCoding coding);

    /// Return gap symbol corresponding to the selected coding
    TResidue GetGapChar(ECaseConversion case_cvt = eCaseConversion_none) const;

    bool CanGetRange(TSeqPos from, TSeqPos to) const;
    bool CanGetRange(const const_iterator& from,
                     const const_iterator& to) const;

    const_iterator begin(void) const;
    const_iterator end(void) const;

    /// Randomization of ambiguities and gaps in ncbi2na coding
    void SetRandomizeAmbiguities(void);
    void SetRandomizeAmbiguities(Uint4 seed);
    void SetRandomizeAmbiguities(CRandom& random_gen);
    void SetNoAmbiguities(void);

private:

    friend class CBioseq_Handle;
    friend class CSeqVector_CI;

    void x_InitSequenceType(void);
    CSeqVector_CI& x_GetIterator(TSeqPos pos) const;
    CSeqVector_CI* x_CreateIterator(TSeqPos pos) const;
    void x_InitRandomizer(CRandom& random_gen);

    CHeapScope               m_Scope;
    CConstRef<CSeqMap>       m_SeqMap;
    CTSE_Handle              m_TSE;
    TSeqPos                  m_Size;
    TMol                     m_Mol;
    ENa_strand               m_Strand;
    TCoding                  m_Coding;
    CRef<CNcbi2naRandomizer> m_Randomizer;

    mutable CSeqVector_CI    m_Iterator;
};


const size_t kRandomizerPosMask = 0x3f;
const size_t kRandomDataSize    = kRandomizerPosMask + 1;


/////////////////////////////////////////////////////////////////////////////
///
///  CNcbi2naRandomizer --
///

class NCBI_XOBJMGR_EXPORT CNcbi2naRandomizer : public CObject
{
public:
    // If seed == 0 then use random number for seed
    CNcbi2naRandomizer(CRandom& gen);
    ~CNcbi2naRandomizer(void);

    typedef char* TData;

    void RandomizeData(TData data,    // cache to be randomized
                       size_t count,  // number of bases in the cache
                       TSeqPos pos);  // sequence pos of the cache

private:
    CNcbi2naRandomizer(const CNcbi2naRandomizer&);
    CNcbi2naRandomizer& operator=(const CNcbi2naRandomizer&);

    // First value in each row indicates ambiguity (1) or
    // normal base (0)
    typedef char        TRandomData[kRandomDataSize + 1];
    typedef TRandomData TRandomTable[16];

    TRandomTable m_RandomTable;
};


/////////////////////////////////////////////////////////////////////
//
//  Inline methods
//
/////////////////////////////////////////////////////////////////////


inline
CSeqVector::TResidue CSeqVector::operator[] (TSeqPos pos) const
{
    return *m_Iterator.SetPos(pos);
}


inline
bool CSeqVector::IsInGap(TSeqPos pos) const
{
    return m_Iterator.SetPos(pos).IsInGap();
}


inline
bool CSeqVector::empty(void) const
{
    return m_Size == 0;
}


inline
TSeqPos CSeqVector::size(void) const
{
    return m_Size;
}


inline
CSeqVector_CI CSeqVector::begin(void) const
{
    return CSeqVector_CI(*this, 0);
}


inline
CSeqVector_CI CSeqVector::end(void) const
{
    return CSeqVector_CI(*this, size());
}


inline
CSeqVector::TCoding CSeqVector::GetCoding(void) const
{
    return m_Coding;
}

inline
CSeqVector::TResidue CSeqVector::GetGapChar(ECaseConversion case_cvt) const
{
    return sx_GetGapChar(GetCoding(), case_cvt);
}

inline
const CSeqMap& CSeqVector::GetSeqMap(void) const
{
    return *m_SeqMap;
}

inline
CScope& CSeqVector::GetScope(void) const
{
    return m_Scope;
}

inline
ENa_strand CSeqVector::GetStrand(void) const
{
    return m_Strand;
}


inline
bool CSeqVector::CanGetRange(const const_iterator& from,
                             const const_iterator& to) const
{
    return CanGetRange(from.GetPos(), to.GetPos());
}


inline
CSeqVector::TMol CSeqVector::GetSequenceType(void) const
{
    return m_Mol;
}


inline
bool CSeqVector::IsProtein(void) const
{
    return CSeq_inst::IsAa(GetSequenceType());
}


inline
bool CSeqVector::IsNucleotide(void) const
{
    return CSeq_inst::IsNa(GetSequenceType());
}


inline
void CSeqVector::GetSeqData(TSeqPos start, TSeqPos stop, string& buffer) const
{
    m_Iterator.GetSeqData(start, stop, buffer);
}


inline
void CSeqVector::GetSeqData(const const_iterator& start,
                            const const_iterator& stop,
                            string& buffer) const
{
    m_Iterator.GetSeqData(start.GetPos(), stop.GetPos(), buffer);
}


/* @} */


END_SCOPE(objects)
END_NCBI_SCOPE

#endif  // SEQ_VECTOR__HPP
