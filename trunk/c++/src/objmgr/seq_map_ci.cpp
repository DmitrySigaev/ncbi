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
* Authors:
*           Eugene Vasilchenko
*
* File Description:
*   Sequence map for the Object Manager. Describes sequence as a set of
*   segments of different types (data, reference, gap or end).
*
*/

#include <objects/objmgr/seq_map_ci.hpp>
#include <objects/objmgr/seq_map.hpp>
#include <objects/objmgr/impl/data_source.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

////////////////////////////////////////////////////////////////////
// CSeqMap_CI_SegmentInfo


bool CSeqMap_CI_SegmentInfo::x_Move(bool minusStrand, CScope* scope)
{
    const CSeqMap& seqMap = *m_SeqMap;
    size_t index = m_Index;
    const CSeqMap::CSegment& old_seg = seqMap.x_GetSegment(index);
    if ( !minusStrand ) {
        if ( old_seg.m_Position > m_LevelRangeEnd ||
             index == seqMap.x_GetSegmentsCount() )
            return false;
        m_Index = ++index;
        seqMap.x_GetSegmentLength(index, scope); // Update length of segment
        return seqMap.x_GetSegmentPosition(index, scope) < m_LevelRangeEnd;
    }
    else {
        if ( old_seg.m_Position + old_seg.m_Length < m_LevelRangePos ||
             index == 0 )
            return false;
        m_Index = --index;
        return old_seg.m_Position > m_LevelRangePos;
    }
}




////////////////////////////////////////////////////////////////////
// CSeqMap_CI


CSeqMap_CI::CSeqMap_CI(void)
    : m_Position(kInvalidSeqPos),
      m_Scope(0),
      m_MaxResolveCount(0),
      m_Flags(fDefaultFlags)
{
}

CSeqMap_CI::CSeqMap_CI(const CConstRef<CSeqMap>& seqMap, CScope* scope,
                       EPosition /*byPos*/, TSeqPos pos,
                       size_t maxResolveCount, TFlags flags)
    : m_Position(0),
      m_Scope(scope),
      m_MaxResolveCount(maxResolveCount),
      m_Flags(flags)
{
    x_Push(seqMap, 0, seqMap->GetLength(scope), false, pos);
    while ( !x_Found() ) {
        if ( !x_Push(pos - m_Position, m_MaxResolveCount > 0) ) {
            x_SettleNext();
            break;
        }
    }
}


CSeqMap_CI::CSeqMap_CI(const CConstRef<CSeqMap>& seqMap, CScope* scope,
                       EPosition /*byPos*/, TSeqPos pos,
                       ENa_strand strand,
                       size_t maxResolveCount, TFlags flags)
    : m_Position(0),
      m_Scope(scope),
      m_MaxResolveCount(maxResolveCount),
      m_Flags(flags)
{
    x_Push(seqMap, 0, seqMap->GetLength(scope),
           strand == eNa_strand_minus, pos);
    while ( !x_Found() ) {
        if ( !x_Push(pos - m_Position, m_MaxResolveCount > 0) ) {
            x_SettleNext();
            break;
        }
    }
}


CSeqMap_CI::CSeqMap_CI(const CConstRef<CSeqMap>& seqMap, CScope* scope,
                       EBegin /*toBegin*/,
                       size_t maxResolveCount, TFlags flags)
    : m_Position(0),
      m_Scope(scope),
      m_MaxResolveCount(maxResolveCount),
      m_Flags(flags)
{
    x_Push(seqMap, 0, seqMap->GetLength(scope), false, 0);
    x_SettleNext();
}


CSeqMap_CI::CSeqMap_CI(const CConstRef<CSeqMap>& seqMap, CScope* scope,
                       EEnd /*toEnd*/,
                       size_t maxResolveCount, TFlags flags)
    : m_Position(seqMap->GetLength(scope)),
      m_Length(0),
      m_Scope(scope),
      m_MaxResolveCount(maxResolveCount),
      m_Flags(flags)
{
    TSegmentInfo push;
    push.m_SeqMap = seqMap;
    push.m_LevelRangePos = 0;
    push.m_LevelRangeEnd = m_Position;
    push.m_MinusStrand = false;
    push.m_Index = seqMap->x_GetSegmentsCount();
    m_Stack.push_back(push);
}


CSeqMap_CI::CSeqMap_CI(const CConstRef<CSeqMap>& seqMap, CScope* scope,
                       TSeqPos from, TSeqPos length, ENa_strand strand,
                       EBegin /*toBegin*/,
                       size_t maxResolveCount, TFlags flags)
    : m_Position(0),
      m_Scope(scope),
      m_MaxResolveCount(maxResolveCount),
      m_Flags(flags)
{
    x_Push(seqMap, from, length, strand == eNa_strand_minus, 0);
    x_SettleNext();
}


CSeqMap_CI::~CSeqMap_CI(void)
{
}


const CSeq_data& CSeqMap_CI::GetData(void) const
{
    if ( !*this ) {
        THROW1_TRACE(runtime_error,
                     "CSeqMap_CI::GetData: out of range");
    }
    if ( GetRefPosition() != 0 || GetRefMinusStrand() ) {
        THROW1_TRACE(runtime_error,
                     "CSeqMap_CI::GetData: non standard Seq_data: "
                     "use methods GetRefData/GetRefPosition/GetRefMinusStrand");
    }
    return GetRefData();
}


const CSeq_data& CSeqMap_CI::GetRefData(void) const
{
    if ( !*this ) {
        THROW1_TRACE(runtime_error,
                     "CSeqMap_CI::GetRefData: out of range");
    }
    return x_GetSeqMap().x_GetSeq_data(x_GetSegment());
}


CSeq_id_Handle CSeqMap_CI::GetRefSeqid(void) const
{
    if ( !*this ) {
        THROW1_TRACE(runtime_error,
                     "CSeqMap_CI::GetRefSeqid: out of range");
    }
    _ASSERT(x_GetSeqMap().m_Source);
    return CSeq_id_Mapper::GetSeq_id_Mapper().
        GetHandle(x_GetSeqMap().x_GetRefSeqid(x_GetSegment()));
}


TSeqPos CSeqMap_CI_SegmentInfo::GetRefPosition(void) const
{
    if ( !InRange() ) {
        THROW1_TRACE(runtime_error,
                     "CSeqMap_CI::GetRefPosition: out of range");
    }
    const CSeqMap::CSegment& seg = x_GetSegment();
    TSignedSeqPos skip;
    if ( !seg.m_RefMinusStrand ) {
        skip = m_LevelRangePos - seg.m_Position;
    }
    else {
        skip = (seg.m_Position + seg.m_Length) - m_LevelRangeEnd;
    }
    if ( skip < 0 )
        skip = 0;
    return seg.m_RefPosition + skip;
}


TSeqPos CSeqMap_CI_SegmentInfo::x_GetTopOffset(void) const
{
    TSignedSeqPos offset;
    if ( !m_MinusStrand ) {
        offset = min(x_GetLevelRealPos(), m_LevelRangeEnd) - m_LevelRangePos;
    }
    else {
        offset = m_LevelRangeEnd - max(x_GetLevelRealEnd(), m_LevelRangePos);
    }
    if ( offset < 0 )
        offset = 0;
    return offset;
}


TSeqPos CSeqMap_CI::x_GetTopOffset(void) const
{
    return x_GetSegmentInfo().x_GetTopOffset();
}


bool CSeqMap_CI::x_Push(TSeqPos pos, bool resolveExternal)
{
    const TSegmentInfo& info = x_GetSegmentInfo();
    const CSeqMap::CSegment& seg = info.x_GetSegment();
    CSeqMap::ESegmentType type = CSeqMap::ESegmentType(seg.m_SegType);
    if ( !(type == CSeqMap::eSeqSubMap ||
           type == CSeqMap::eSeqRef && resolveExternal) ) {
        return false;
    }
    x_Push(info.m_SeqMap->x_GetSubSeqMap(seg, GetScope(), resolveExternal),
           GetRefPosition(), GetLength(), GetRefMinusStrand(), pos);
    if ( type == CSeqMap::eSeqRef )
        --m_MaxResolveCount;
    return true;
}


void CSeqMap_CI::x_Push(const CConstRef<CSeqMap>& seqMap,
                        TSeqPos from, TSeqPos length,
                        bool minusStrand,
                        TSeqPos pos)
{
    TSegmentInfo push;
    push.m_SeqMap = seqMap;
    push.m_LevelRangePos = from;
    push.m_LevelRangeEnd = from + length;
    push.m_MinusStrand = minusStrand;
    TSeqPos findOffset = !minusStrand? pos: length - 1 - pos;
    push.m_Index = seqMap->x_FindSegment(from + findOffset, GetScope());
    if ( push.m_Index == size_t(-1) ) {
        _ASSERT(length == 0);
        push.m_Index = !minusStrand? seqMap->x_GetSegmentsCount(): 0;
    }
    else {
        _ASSERT(push.m_Index < seqMap->x_GetSegmentsCount());
    }
    // update length of current segment
    seqMap->x_GetSegmentLength(push.m_Index, GetScope());
    m_Stack.push_back(push);
    // update position
    m_Position += x_GetTopOffset();
    // update length
    m_Length = push.x_CalcLength();
}


bool CSeqMap_CI::x_Pop(void)
{
    if ( m_Stack.size() <= 1 ) {
        return false;
    }

    m_Position -= x_GetTopOffset();
    m_Stack.pop_back();
    if ( x_GetSegment().m_SegType == CSeqMap::eSeqRef ) {
        ++m_MaxResolveCount;
    }
    m_Length = x_GetSegmentInfo().x_CalcLength();
    return true;
}


bool CSeqMap_CI::x_TopNext(void)
{
    TSegmentInfo& top = x_GetSegmentInfo();
    m_Position += m_Length;
    if ( !top.x_Move(top.m_MinusStrand, GetScope()) ) {
        m_Length = 0;
        return false;
    }
    else {
        m_Length = x_GetSegmentInfo().x_CalcLength();
        return true;
    }
}


bool CSeqMap_CI::x_TopPrev(void)
{
    TSegmentInfo& top = x_GetSegmentInfo();
    if ( !top.x_Move(!top.m_MinusStrand, GetScope()) ) {
        m_Length = 0;
        return false;
    }
    else {
        m_Length = x_GetSegmentInfo().x_CalcLength();
        m_Position -= m_Length;
        return true;
    }
}


bool CSeqMap_CI::x_Next(bool resolveExternal)
{
    if ( x_Push(0, resolveExternal) ) {
        return true;
    }
    do {
        if ( x_TopNext() )
            return true;
    } while ( x_Pop() );
    return false;
}


bool CSeqMap_CI::x_Prev(void)
{
    if ( !x_TopPrev() )
        return x_Pop();
    while ( x_Push(m_Length-1, m_MaxResolveCount > 0) ) {
    }
    return true;
}


bool CSeqMap_CI::x_Found(void) const
{
    switch ( x_GetSegment().m_SegType ) {
    case CSeqMap::eSeqRef:
        return (m_Flags & fFindRef) != 0 || m_MaxResolveCount <= 0;
    case CSeqMap::eSeqData:
        return (m_Flags & fFindData) != 0;
    case CSeqMap::eSeqGap:
        return (m_Flags & fFindGap) != 0;
    case CSeqMap::eSeqSubMap:
        return false; // always skip submaps
    default:
        return true;
    }
}


bool CSeqMap_CI::x_SettleNext(void)
{
    while ( !x_Found() ) {
        if ( !x_Next(m_MaxResolveCount > 0) )
            return false;
    }
    return true;
}


bool CSeqMap_CI::x_SettlePrev(void)
{
    while ( !x_Found() ) {
        if ( !x_Prev() )
            return false;
    }
    return true;
}


bool CSeqMap_CI::Next(bool resolveCurrentExternal)
{
    return x_Next(resolveCurrentExternal && m_MaxResolveCount > 0) &&
        x_SettleNext();
}


bool CSeqMap_CI::Prev(void)
{
    return x_Prev() && x_SettlePrev();
}


void CSeqMap_CI::SetFlags(TFlags flags)
{
    m_Flags = flags;
}


END_SCOPE(objects)
END_NCBI_SCOPE


/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.13  2003/05/23 16:32:21  vasilche
* Fixed backward traversal of CSeqMap_CI.
*
* Revision 1.12  2003/05/20 20:36:14  vasilche
* Added FindResolved() with strand argument.
*
* Revision 1.11  2003/05/20 15:44:38  vasilche
* Fixed interaction of CDataSource and CDataLoader in multithreaded app.
* Fixed some warnings on WorkShop.
* Added workaround for memory leak on WorkShop.
*
* Revision 1.10  2003/02/27 16:29:19  vasilche
* Fixed lost features from first segment.
*
* Revision 1.9  2003/02/25 14:48:29  vasilche
* Removed performance warning on Windows.
*
* Revision 1.8  2003/02/24 18:57:22  vasilche
* Make feature gathering in one linear pass using CSeqMap iterator.
* Do not use feture index by sub locations.
* Sort features at the end of gathering in one vector.
* Extracted some internal structures and classes in separate header.
* Delay creation of mapped features.
*
* Revision 1.7  2003/02/11 19:26:18  vasilche
* Fixed CSeqMap_CI with ending NULL segment.
*
* Revision 1.6  2003/02/05 17:59:17  dicuccio
* Moved formerly private headers into include/objects/objmgr/impl
*
* Revision 1.5  2003/02/05 15:55:26  vasilche
* Added eSeqEnd segment at the beginning of seq map.
* Added flags to CSeqMap_CI to stop on data, gap, or references.
*
* Revision 1.4  2003/01/29 22:03:46  grichenk
* Use single static CSeq_id_Mapper instead of per-OM model.
*
* Revision 1.3  2003/01/24 20:14:08  vasilche
* Fixed processing zero length references.
*
* Revision 1.2  2003/01/22 20:11:54  vasilche
* Merged functionality of CSeqMapResolved_CI to CSeqMap_CI.
* CSeqMap_CI now supports resolution and iteration over sequence range.
* Added several caches to CScope.
* Optimized CSeqVector().
* Added serveral variants of CBioseqHandle::GetSeqVector().
* Tried to optimize annotations iterator (not much success).
* Rewritten CHandleRange and CHandleRangeMap classes to avoid sorting of list.
*
* Revision 1.1  2002/12/26 16:39:24  vasilche
* Object manager class CSeqMap rewritten.
*
*
* ===========================================================================
*/
