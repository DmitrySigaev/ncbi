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
#include <objmgr/seq_map.hpp>
#include <objmgr/seq_vector_ci.hpp>
#include <objects/seq/Seq_data.hpp>
#include <vector>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CScope;
class CSeq_loc;
class CSeqMap;
class CSeq_data;
class CSeqVector_CI;

// Sequence data
struct NCBI_XOBJMGR_EXPORT SSeqData {
    TSeqPos              length;      /// Length of the sequence data piece
    TSeqPos              dest_start;  /// Starting pos in the dest. Bioseq
    TSeqPos              src_start;   /// Starting pos in the source Bioseq
    CConstRef<CSeq_data> src_data;    /// Source sequence data
};

class NCBI_XOBJMGR_EXPORT CSeqVector : public CObject
{
public:
    typedef CSeqVector_CI::TResidue TResidue;
    typedef CSeqVector_CI::TCoding  TCoding;
    typedef CBioseq_Handle::EVectorCoding EVectorCoding;

    CSeqVector(void);
    CSeqVector(const CSeqMap& seqMap, CScope& scope,
               EVectorCoding coding = CBioseq_Handle::eCoding_Ncbi,
               ENa_strand strand = eNa_strand_unknown);
    CSeqVector(CConstRef<CSeqMap> seqMap, CScope& scope,
               EVectorCoding coding = CBioseq_Handle::eCoding_Ncbi,
               ENa_strand strand = eNa_strand_unknown);
    CSeqVector(const CSeqVector& vec);

    virtual ~CSeqVector(void);

    CSeqVector& operator= (const CSeqVector& vec);

    TSeqPos size(void) const;
    // 0-based array of residues
    TResidue operator[] (TSeqPos pos) const;

    // Fill the buffer string with the sequence data for the interval
    // [start, stop).
    void GetSeqData(TSeqPos start, TSeqPos stop, string& buffer) const;

    enum ESequenceType {
        eType_not_set,  // temporary before initialization
        eType_na,
        eType_aa,
        eType_unknown   // cannot be determined (no data)
    };
    ESequenceType GetSequenceType(void) const;

    // Target sequence coding. CSeq_data::e_not_set -- do not
    // convert sequence (use GetCoding() to check the real coding).
    TCoding GetCoding(void) const;
    void SetCoding(TCoding coding);
    // Set coding to either Iupacaa or Iupacna depending on molecule type
    void SetIupacCoding(void);
    // Set coding to either Ncbi8aa or Ncbi8na depending on molecule type
    void SetNcbiCoding(void);
    // Set coding to either Iupac or Ncbi8xx
    void SetCoding(EVectorCoding coding);

    // Return gap symbol corresponding to the selected coding
    TResidue GetGapChar(void) const;

private:
    enum {
        kTypeUnknown = 1 << 7
    };

    friend class CBioseq_Handle;
    friend class CSeqVector_CI;

    TCoding x_GetCoding(TCoding cacheCoding, TCoding dataCoding) const;
    TCoding x_UpdateCoding(void) const;
    void x_InitSequenceType(void);
    TResidue x_GetGapChar(TCoding coding) const;
    const CSeqMap& x_GetSeqMap(void) const;
    CSeqVector_CI& x_GetIterator(void) const;

    static const char* sx_GetConvertTable(TCoding src, TCoding dst);
    static const char* sx_GetComplementTable(TCoding coding);

    CConstRef<CSeqMap>    m_SeqMap;
    CScope*               m_Scope;
    TCoding               m_Coding;
    ENa_strand            m_Strand;

    mutable ESequenceType m_SequenceType;
    mutable auto_ptr<CSeqVector_CI> m_Iterator;
};


/////////////////////////////////////////////////////////////////////
//
//  Inline methods
//
/////////////////////////////////////////////////////////////////////


inline
CSeqVector::TResidue CSeqVector::operator[] (TSeqPos pos) const
{
    CSeqVector_CI& it = x_GetIterator();
    TSeqPos old_pos = it.GetPos();
    if (pos == old_pos+1) {
        ++it;
    }
    else if (pos == old_pos-1) {
        --it;
    }
    else if (pos != old_pos) {
        it.SetPos(pos);
    }
    return *it;
}

inline
CSeqVector::TCoding CSeqVector::GetCoding(void) const
{
    TCoding coding = m_Coding;
    return int(coding) & kTypeUnknown? x_UpdateCoding(): coding;
}

inline
CSeqVector::TResidue CSeqVector::GetGapChar(void) const
{
    return x_GetGapChar(GetCoding());
}

inline
const CSeqMap& CSeqVector::x_GetSeqMap(void) const
{
    return *m_SeqMap;
}

inline
CSeqVector_CI& CSeqVector::x_GetIterator(void) const
{
    if ( !m_Iterator.get() ) {
        m_Iterator.reset(new CSeqVector_CI(*this, 0));
    }
    return *m_Iterator;
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.39  2003/06/24 19:46:41  grichenk
* Changed cache from vector<char> to char*. Made
* CSeqVector::operator[] inline.
*
* Revision 1.38  2003/06/13 19:40:14  grichenk
* Removed _ASSERT() from x_GetSeqMap()
*
* Revision 1.37  2003/06/13 17:22:26  grichenk
* Check if seq-map is not null before using it
*
* Revision 1.36  2003/06/12 18:38:47  vasilche
* Added default constructor of CSeqVector.
*
* Revision 1.35  2003/06/11 19:32:53  grichenk
* Added molecule type caching to CSeqMap, simplified
* coding and sequence type calculations in CSeqVector.
*
* Revision 1.34  2003/06/02 16:01:36  dicuccio
* Rearranged include/objects/ subtree.  This includes the following shifts:
*     - include/objects/alnmgr --> include/objtools/alnmgr
*     - include/objects/cddalignview --> include/objtools/cddalignview
*     - include/objects/flat --> include/objtools/flat
*     - include/objects/objmgr/ --> include/objmgr/
*     - include/objects/util/ --> include/objmgr/util/
*     - include/objects/validator --> include/objtools/validator
*
* Revision 1.33  2003/05/27 19:44:04  grichenk
* Added CSeqVector_CI class
*
* Revision 1.32  2003/05/20 15:44:37  vasilche
* Fixed interaction of CDataSource and CDataLoader in multithreaded app.
* Fixed some warnings on WorkShop.
* Added workaround for memory leak on WorkShop.
*
* Revision 1.31  2003/05/05 21:00:27  vasilche
* Fix assignment of empty CSeqVector.
*
* Revision 1.30  2003/04/29 19:51:12  vasilche
* Fixed interaction of Data Loader garbage collector and TSE locking mechanism.
* Made some typedefs more consistent.
*
* Revision 1.29  2003/04/24 16:12:37  vasilche
* Object manager internal structures are splitted more straightforward.
* Removed excessive header dependencies.
*
* Revision 1.28  2003/02/27 20:56:03  vasilche
* kTypeUnknown bit made fitting in 8 bit integer.
*
* Revision 1.27  2003/02/25 14:48:06  vasilche
* Added Win32 export modifier to object manager classes.
*
* Revision 1.26  2003/02/20 17:01:20  vasilche
* Changed value of kTypeUnknown to fit in 16 bit enums on Mac.
*
* Revision 1.25  2003/02/06 19:05:39  vasilche
* Fixed old cache data copying.
* Delayed sequence type (protein/dna) resolution.
*
* Revision 1.24  2003/01/23 19:33:57  vasilche
* Commented out obsolete methods.
* Use strand argument of CSeqVector instead of creation reversed seqmap.
* Fixed ordering operators of CBioseqHandle to be consistent.
*
* Revision 1.23  2003/01/22 20:11:53  vasilche
* Merged functionality of CSeqMapResolved_CI to CSeqMap_CI.
* CSeqMap_CI now supports resolution and iteration over sequence range.
* Added several caches to CScope.
* Optimized CSeqVector().
* Added serveral variants of CBioseqHandle::GetSeqVector().
* Tried to optimize annotations iterator (not much success).
* Rewritten CHandleRange and CHandleRangeMap classes to avoid sorting of list.
*
* Revision 1.22  2003/01/03 19:45:44  dicuccio
* Replaced kPosUnknwon with kInvalidSeqPos (non-static variable; work-around for
* MSVC)
*
* Revision 1.21  2002/12/26 20:51:36  dicuccio
* Added Win32 export specifier
*
* Revision 1.20  2002/12/26 16:39:22  vasilche
* Object manager class CSeqMap rewritten.
*
* Revision 1.19  2002/10/03 13:45:37  grichenk
* CSeqVector::size() made const
*
* Revision 1.18  2002/09/03 21:26:58  grichenk
* Replaced bool arguments in CSeqVector constructor and getters
* with enums.
*
* Revision 1.17  2002/07/08 20:50:56  grichenk
* Moved log to the end of file
* Replaced static mutex (in CScope, CDataSource) with the mutex
* pool. Redesigned CDataSource data locking.
*
* Revision 1.16  2002/05/31 17:52:58  grichenk
* Optimized for better performance (CTSE_Info uses atomic counter,
* delayed annotations indexing, no location convertions in
* CAnnot_Types_CI if no references resolution is required etc.)
*
* Revision 1.15  2002/05/17 17:14:50  grichenk
* +GetSeqData() for getting a range of characters from a seq-vector
*
* Revision 1.14  2002/05/09 14:16:29  grichenk
* sm_SizeUnknown -> kPosUnknown, minor fixes for unsigned positions
*
* Revision 1.13  2002/05/06 03:30:36  vakatov
* OM/OM1 renaming
*
* Revision 1.12  2002/05/03 21:28:02  ucko
* Introduce T(Signed)SeqPos.
*
* Revision 1.11  2002/05/03 18:36:13  grichenk
* Fixed members initialization
*
* Revision 1.10  2002/04/30 14:32:51  ucko
* Have size() return int in keeping with its actual behavior; should cut
* down on warnings about truncation of 64-bit integers.
*
* Revision 1.9  2002/04/29 16:23:25  grichenk
* GetSequenceView() reimplemented in CSeqVector.
* CSeqVector optimized for better performance.
*
* Revision 1.8  2002/04/25 16:37:19  grichenk
* Fixed gap coding, added GetGapChar() function
*
* Revision 1.7  2002/04/23 19:01:06  grichenk
* Added optional flag to GetSeqVector() and GetSequenceView()
* for switching to IUPAC encoding.
*
* Revision 1.6  2002/02/21 19:27:00  grichenk
* Rearranged includes. Added scope history. Added searching for the
* best seq-id match in data sources and scopes. Updated tests.
*
* Revision 1.5  2002/02/15 20:36:29  gouriano
* changed implementation of HandleRangeMap
*
* Revision 1.4  2002/01/28 19:45:34  gouriano
* changed the interface of BioseqHandle: two functions moved from Scope
*
* Revision 1.3  2002/01/23 21:59:29  grichenk
* Redesigned seq-id handles and mapper
*
* Revision 1.2  2002/01/16 16:26:36  gouriano
* restructured objmgr
*
* Revision 1.1  2002/01/11 19:04:04  gouriano
* restructured objmgr
*
*
* ===========================================================================
*/

#endif  // SEQ_VECTOR__HPP
