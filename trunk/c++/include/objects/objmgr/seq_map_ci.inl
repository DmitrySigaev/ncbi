#if !defined(OBJECTS_OBJMGR___SEQ_MAP_CI__INL)
#define OBJECTS_OBJMGR___SEQ_MAP_CI__INL

/* $Id$
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
 * Author: Eugene Vasilchenko
 *
 * File Description:
 *   Inline methods of CSeqMap class.
 *
 */


/////////////////////////////////////////////////////////////////////
//  CSeqMap_CI_SegmentInfo


inline
const CSeqMap& CSeqMap_CI_SegmentInfo::x_GetSeqMap(void) const
{
    return *m_SeqMap;
}


inline
size_t CSeqMap_CI_SegmentInfo::x_GetIndex(void) const
{
    return m_Index;
}


inline
const CSeqMap::CSegment& CSeqMap_CI_SegmentInfo::x_GetSegment(void) const
{
    return x_GetSeqMap().x_GetSegment(x_GetIndex());
}


inline
CSeqMap_CI_SegmentInfo::CSeqMap_CI_SegmentInfo(void)
    : m_SeqMap(0), m_Index(kInvalidSeqPos),
      m_LevelRangePos(kInvalidSeqPos), m_LevelRangeEnd(kInvalidSeqPos)
{
}

/*
inline
CSeqMap_CI_SegmentInfo::CSeqMap_CI_SegmentInfo(CConstRef<CSeqMap> seqMap, size_t index)
    : m_SeqMap(seqMap), m_Index(index),
      m_LevelRangePos(0), m_LevelRangeEnd(seqMap->x_GetSegmentLength(index))
{
}
*/

/////////////////////////////////////////////////////////////////////
//  CSeqMap_CI


inline
const CSeqMap_CI::TSegmentInfo& CSeqMap_CI::x_GetSegmentInfo(void) const
{
    return m_Stack.back();
}


inline
CSeqMap_CI::TSegmentInfo& CSeqMap_CI::x_GetSegmentInfo(void)
{
    return m_Stack.back();
}


inline
const CSeqMap& CSeqMap_CI::x_GetSeqMap(void) const
{
    return x_GetSegmentInfo().x_GetSeqMap();
}


inline
size_t CSeqMap_CI::x_GetIndex(void) const
{
    return x_GetSegmentInfo().x_GetIndex();
}


inline
const CSeqMap::CSegment& CSeqMap_CI::x_GetSegment(void) const
{
    return x_GetSegmentInfo().x_GetSegment();
}


inline
CScope* CSeqMap_CI::GetScope(void) const
{
    return m_Scope;
}


inline
TSeqPos CSeqMap_CI_SegmentInfo::x_GetLevelRealPos(void) const
{
    return x_GetSegment().m_Position;
}


inline
TSeqPos CSeqMap_CI_SegmentInfo::x_GetLevelRealEnd(void) const
{
    const CSeqMap::CSegment& seg = x_GetSegment();
    return seg.m_Position + seg.m_Length;
}


inline
TSeqPos CSeqMap_CI_SegmentInfo::x_GetLevelPos(void) const
{
    return max(m_LevelRangePos, x_GetLevelRealPos());
}


inline
TSeqPos CSeqMap_CI_SegmentInfo::x_GetLevelEnd(void) const
{
    return min(m_LevelRangeEnd, x_GetLevelRealEnd());
}


inline
TSeqPos CSeqMap_CI_SegmentInfo::x_GetSkipBefore(void) const
{
    TSignedSeqPos skip = m_LevelRangePos - x_GetLevelRealPos();
    if ( skip < 0 )
        skip = 0;
    return skip;
}


inline
TSeqPos CSeqMap_CI_SegmentInfo::x_GetSkipAfter(void) const
{
    TSignedSeqPos skip = x_GetLevelRealEnd() - m_LevelRangeEnd;
    if ( skip < 0 )
        skip = 0;
    return skip;
}


inline
TSeqPos CSeqMap_CI_SegmentInfo::x_CalcLength(void) const
{
    return x_GetLevelEnd() - x_GetLevelPos();
}


inline
bool CSeqMap_CI_SegmentInfo::GetRefMinusStrand(void) const
{
    return x_GetSegment().m_RefMinusStrand ^ m_MinusStrand;
}


inline
TSeqPos CSeqMap_CI::GetPosition(void) const
{
    return m_Position;
}


inline
TSeqPos CSeqMap_CI::GetLength(void) const
{
    return m_Length;
}


inline
TSeqPos CSeqMap_CI::GetEndPosition(void) const
{
    return m_Position + m_Length;
}


inline
CSeqMap_CI::operator bool(void) const
{
    const CSeqMap_CI_SegmentInfo& info = m_Stack.front();
    return m_Position < info.m_LevelRangeEnd - info.m_LevelRangePos;
}


inline
TSeqPos CSeqMap_CI::GetRefPosition(void) const
{
    return x_GetSegmentInfo().GetRefPosition();
}


inline
bool CSeqMap_CI::GetRefMinusStrand(void) const
{
    return x_GetSegmentInfo().GetRefMinusStrand();
}


inline
TSeqPos CSeqMap_CI::GetRefEndPosition(void) const
{
    return GetRefPosition() + GetLength();
}

/*
inline
TSeqPos CSeqMap_CI::x_GetPositionInSeqMap(void) const
{
    return x_GetSeqMap()->x_GetSegmentPosition(x_GetIndex(), GetScope());
}
*/

inline
bool CSeqMap_CI::operator==(const CSeqMap_CI& seg) const
{
    return
        GetPosition() == seg.GetPosition() &&
        m_Stack.size() == seg.m_Stack.size() &&
        x_GetIndex() == seg.x_GetIndex();
}


inline
bool CSeqMap_CI::operator<(const CSeqMap_CI& seg) const
{
    return
        GetPosition() < seg.GetPosition() ||
        GetPosition() == seg.GetPosition() && 
        (m_Stack.size() < seg.m_Stack.size() ||
         m_Stack.size() == seg.m_Stack.size() && x_GetIndex() < seg.x_GetIndex());
}


inline
bool CSeqMap_CI::operator>(const CSeqMap_CI& seg) const
{
    return
        GetPosition() > seg.GetPosition() ||
        GetPosition() == seg.GetPosition() && 
        (m_Stack.size() > seg.m_Stack.size() ||
         m_Stack.size() == seg.m_Stack.size() && x_GetIndex() > seg.x_GetIndex());
}


inline
bool CSeqMap_CI::operator!=(const CSeqMap_CI& seg) const
{
    return !(*this == seg);
}


inline
bool CSeqMap_CI::operator<=(const CSeqMap_CI& seg) const
{
    return !(*this > seg);
}


inline
bool CSeqMap_CI::operator>=(const CSeqMap_CI& seg) const
{
    return !(*this < seg);
}


inline
CSeqMap_CI& CSeqMap_CI::operator++(void)
{
    Next();
    return *this;
}


inline
CSeqMap_CI CSeqMap_CI::operator++(int)
{
    CSeqMap_CI old(*this);
    Next();
    return old;
}


inline
CSeqMap_CI& CSeqMap_CI::operator--(void)
{
    Prev();
    return *this;
}


inline
CSeqMap_CI CSeqMap_CI::operator--(int)
{
    CSeqMap_CI old(*this);
    Prev();
    return old;
}


/*
 * ===========================================================================
 * $Log$
 * Revision 1.2  2003/01/22 20:11:53  vasilche
 * Merged functionality of CSeqMapResolved_CI to CSeqMap_CI.
 * CSeqMap_CI now supports resolution and iteration over sequence range.
 * Added several caches to CScope.
 * Optimized CSeqVector().
 * Added serveral variants of CBioseqHandle::GetSeqVector().
 * Tried to optimize annotations iterator (not much success).
 * Rewritten CHandleRange and CHandleRangeMap classes to avoid sorting of list.
 *
 * Revision 1.1  2002/12/26 16:39:22  vasilche
 * Object manager class CSeqMap rewritten.
 *
 *
 * ===========================================================================
 */

#endif
