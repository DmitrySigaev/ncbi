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
* Author: Aleksey Grichenko, Michael Kimelman
*
* File Description:
*   Sequence data container for object manager
*
*/

#include <objects/objmgr/bioseq_handle.hpp>
#include <objects/seq/Seq_data.hpp>
#include <util/range.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

// Sequence data
struct SSeqData {
    TSeqPos              length;      /// Length of the sequence data piece
    TSeqPos              dest_start;  /// Starting pos in the dest. Bioseq
    TSeqPos              src_start;   /// Starting pos in the source Bioseq
    CConstRef<CSeq_data> src_data;    /// Source sequence data
};

class CScope;
class CSeq_loc;
class CSeqMap;

class NCBI_XOBJMGR_EXPORT CSeqVector : public CObject
{
public:
    typedef unsigned char         TResidue;
    typedef CSeq_data::E_Choice   TCoding;

    CSeqVector(const CSeqVector& vec);
    virtual ~CSeqVector(void);

    CSeqVector& operator= (const CSeqVector& vec);

    TSeqPos size(void) const;
    // 0-based array of residues
    TResidue operator[] (TSeqPos pos);

    // Fill the buffer string with the sequence data for the interval
    // [start, stop).
    void GetSeqData(TSeqPos start, TSeqPos stop, string& buffer);

    // Target sequence coding. CSeq_data::e_not_set -- do not
    // convert sequence (use GetCoding() to check the real coding).
    TCoding GetCoding(void) const;
    void SetCoding(TCoding coding);
    // Set coding to either Iupacaa or Iupacna depending on molecule type
    void SetIupacCoding(void);

    // Return gap symbol corresponding to the selected coding
    TResidue GetGapChar(void) const;

private:
    friend class CBioseq_Handle;

    // Created by CBioseq_Handle only.
    CSeqVector(const CBioseq_Handle& handle,
        CBioseq_Handle::EVectorCoding coding,  // Set coding to IUPACna or IUPACaa?
        CBioseq_Handle::EVectorStrand strand,  // Use plus strand coordinates?
        CScope& scope,
        CConstRef<CSeq_loc> view_loc);         // Restrict visible area with a seq-loc

    // Process seq-loc, create visible area ranges
    void x_SetVisibleArea(const CSeq_loc& view_loc);
    // Calculate sequence and visible area size
    TSeqPos x_GetTotalSize(void) const;
    TSeqPos x_GetVisibleSize(void) const;

    void x_UpdateVisibleRange(TSeqPos pos);
    void x_UpdateSeqData(TSeqPos pos);
    // Get residue assuming the data in m_CurrentData are valid
    TResidue x_GetResidue(TSeqPos pos);

    // Fill the buffer with as many characters as possible with the single
    // data cache. Characters starting from the "start" position are added
    // to the buffer; the last character added is at or before "stop" position;
    // "start" is moved to the next position after the last character added.
    void x_GetCacheForInterval(TSeqPos& start, TSeqPos stop, string& buffer);

    // Single visible interval
    typedef CRange<TSeqPos> TRange;
    // Interval with the plus strand flag (true = plus)
    typedef pair<TRange, bool> TRangeWithStrand;
    // Map visible interval end (not start!) to an interval with strand.
    // E.g. if there is only one visible interval [20..30], it will be
    // mapped with 10 (=30-20) key.
    typedef map<TSeqPos, TRangeWithStrand> TRanges;

    CScope*            m_Scope;
    CBioseq_Handle     m_Handle;
    bool               m_PlusStrand;
    SSeqData           m_CurData;
    CConstRef<CSeqMap> m_SeqMap;
    mutable TSeqPos    m_Size;
    string             m_CachedData;
    TSeqPos            m_CachedPos;
    TSeqPos            m_CachedLen;
    TCoding            m_Coding;
    mutable TRanges    m_Ranges;    // Set of visible ranges
    mutable TSeqPos    m_RangeSize; // Visible area size
    TRanges::const_iterator m_SelRange;  // Selected range from the visible area
    // Current visible range limits -- for faster checks in []
    TSeqPos            m_CurFrom;   // visible segment start
    TSeqPos            m_CurTo;     // visible segment end
    // End of visible segment on the original sequence
    TSeqPos            m_OrgTo;

    static const TSeqPos kPosUnknown;
};


/////////////////////////////////////////////////////////////////////
//
//  Inline methods
//
/////////////////////////////////////////////////////////////////////


inline
TSeqPos CSeqVector::size(void) const
{
    if (m_RangeSize == kPosUnknown)
        x_GetVisibleSize();
    return m_RangeSize;
}

inline
CSeqVector::TCoding CSeqVector::GetCoding(void) const
{
    if (m_Coding != CSeq_data::e_not_set) {
        return m_Coding;
    }
    else if ( m_CurData.src_data ) {
        return m_CurData.src_data->Which();
    }
    // No coding specified
    return CSeq_data::e_not_set;
}

inline
void CSeqVector::SetCoding(TCoding coding)
{
    if (m_Coding == coding) return;
    m_Coding = coding;
    // Reset cached data
    m_CachedLen = 0;
    m_CachedData = "";
}

inline
CSeqVector::TResidue CSeqVector::operator[] (TSeqPos pos)
{
    // Force size calculation
    TSeqPos seq_size = size();
    // Convert position to destination strand
    if ( !m_PlusStrand ) {
        pos = seq_size - pos - 1;
    }

    if (pos >= m_CurTo  ||  pos < m_CurFrom) {
        x_UpdateVisibleRange(pos);
    }

    // Recalculate position from visible area to the whole sequence
    pos += m_OrgTo - m_CurTo;
    // Check and update current data segment
    if (m_CurData.dest_start > pos  ||
        m_CurData.dest_start+m_CurData.length <= pos) {
        x_UpdateSeqData(pos);
    }
    // Get the residue
    return x_GetResidue(pos);
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
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
