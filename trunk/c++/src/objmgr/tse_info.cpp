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
* Author: Aleksey Grichenko
*
* File Description:
*   TSE info -- entry for data source seq-id to TSE map
*
*/


#include <objects/objmgr/impl/tse_info.hpp>

#include <objects/objmgr/impl/annot_object.hpp>
#include <objects/objmgr/annot_selector.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


////////////////////////////////////////////////////////////////////
//
//  CTSE_Info::
//
//    General information and indexes for top level seq-entries
//


CTSE_Info::CTSE_Info(void)
    : m_DataSource(0), m_Dead(false)
{
}


CTSE_Info::~CTSE_Info(void)
{
}


inline
CTSE_Info::TAnnotSelectorKey
CTSE_Info::x_GetAnnotSelectorKey(const SAnnotSelector& sel)
{
    _ASSERT(size_t(sel.GetAnnotChoice()) < 0x100);
    _ASSERT(size_t(sel.GetFeatChoice()) < 0x100);
    _ASSERT(size_t(sel.GetFeatProduct()) < 0x10000);
    return (sel.GetAnnotChoice()) |
        (sel.GetFeatChoice() << 8) |
        (sel.GetFeatProduct() << 16);
}

const CTSE_Info::TRangeMap*
CTSE_Info::x_GetRangeMap(const CSeq_id_Handle& id,
                         const SAnnotSelector& sel) const
{
    TAnnotMap::const_iterator amit = m_AnnotMap.find(id);
    if (amit == m_AnnotMap.end()) {
        return 0;
    }
    
    
    TAnnotSelectorMap::const_iterator sit =
        amit->second.find(x_GetAnnotSelectorKey(sel));
    if ( sit == amit->second.end() ) {
        return 0;
    }

    return &sit->second;
}


CTSE_Info::TRangeMap& CTSE_Info::x_SetRangeMap(TAnnotSelectorMap& selMap,
                                               const SAnnotSelector& sel)
{
    return selMap[x_GetAnnotSelectorKey(sel)];
}


CTSE_Info::TRangeMap& CTSE_Info::x_SetRangeMap(const CSeq_id_Handle& id,
                                               const SAnnotSelector& sel)
{
    return x_SetRangeMap(m_AnnotMap[id], sel);
}


void CTSE_Info::x_DropRangeMap(TAnnotSelectorMap& selMap,
                               const SAnnotSelector& sel)
{
    selMap.erase(x_GetAnnotSelectorKey(sel));
}


void CTSE_Info::x_DropRangeMap(const CSeq_id_Handle& id,
                               const SAnnotSelector& sel)
{
    x_DropRangeMap(m_AnnotMap[id], sel);
}


void CTSE_Info::DebugDump(CDebugDumpContext ddc, unsigned int depth) const
{
    ddc.SetFrame("CTSE_Info");
    CObject::DebugDump( ddc, depth);

    ddc.Log("m_TSE", m_TSE.GetPointer(),0);
    ddc.Log("m_Dead", m_Dead);
    if (depth == 0) {
        DebugDumpValue(ddc, "m_BioseqMap.size()", m_BioseqMap.size());
        DebugDumpValue(ddc, "m_AnnotMap.size()",  m_AnnotMap.size());
    } else {
        unsigned int depth2 = depth-1;
        { //--- m_BioseqMap
            DebugDumpValue(ddc, "m_BioseqMap.type",
                "map<CSeq_id_Handle, CRef<CBioseq_Info>>");
            CDebugDumpContext ddc2(ddc,"m_BioseqMap");
            TBioseqMap::const_iterator it;
            for (it=m_BioseqMap.begin(); it!=m_BioseqMap.end(); ++it) {
                string member_name = "m_BioseqMap[ " +
                    (it->first).AsString() +" ]";
                ddc2.Log(member_name, (it->second).GetPointer(),depth2);
            }
        }
        { //--- m_AnnotMap_ByInt
            DebugDumpValue(ddc, "m_AnnotMap_ByInt.type",
                "map<CSeq_id_Handle, CRangeMultimap<CRef<CAnnotObject>,"
                "CRange<TSeqPos>::position_type>>");

            CDebugDumpContext ddc2(ddc,"m_AnnotMap_ByInt");
            TAnnotMap::const_iterator it;
            for (it=m_AnnotMap.begin(); it!=m_AnnotMap.end(); ++it) {
                string member_name = "m_AnnotMap[ " +
                    (it->first).AsString() +" ]";
                if (depth2 == 0) {
                    member_name += "size()";
                    DebugDumpValue(ddc2, member_name, (it->second).size());
                } else {
/*
                    // CRangeMultimap
                    CDebugDumpContext ddc3(ddc2, member_name);
                    ITERATE( TRangeMap, itrm, it->second ) {
                        // CRange as string
                        string rg;
                        if (itrm->first.Empty()) {
                            rg += "empty";
                        } else if (itrm->first.IsWhole()) {
                            rg += "whole";
                        } else if (itrm->first.IsWholeTo()) {
                            rg += "unknown";
                        } else {
                            rg +=
                                NStr::UIntToString(itrm->first.GetFrom()) +
                                "..." +
                                NStr::UIntToString(itrm->first.GetTo());
                        }
                        string rm_name = member_name + "[ " + rg + " ]";
                        // CAnnotObject
                        ddc3.Log(rm_name, itrm->second, depth2-1);
                    }
*/
                }
            }
        }
    }
    // DebugDumpValue(ddc, "CMutableAtomicCounter::Get()", Get());
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.22  2003/03/21 19:22:51  grichenk
* Redesigned TSE locking, replaced CTSE_Lock with CRef<CTSE_Info>.
*
* Revision 1.21  2003/03/12 20:09:34  grichenk
* Redistributed members between CBioseq_Handle, CBioseq_Info and CTSE_Info
*
* Revision 1.20  2003/03/11 15:51:06  kuznets
* iterate -> ITERATE
*
* Revision 1.19  2003/03/10 16:55:17  vasilche
* Cleaned SAnnotSelector structure.
* Added shortcut when features are limited to one TSE.
*
* Revision 1.18  2003/03/05 20:56:43  vasilche
* SAnnotSelector now holds all parameters of annotation iterators.
*
* Revision 1.17  2003/02/25 20:10:40  grichenk
* Reverted to single total-range index for annotations
*
* Revision 1.16  2003/02/24 18:57:22  vasilche
* Make feature gathering in one linear pass using CSeqMap iterator.
* Do not use feture index by sub locations.
* Sort features at the end of gathering in one vector.
* Extracted some internal structures and classes in separate header.
* Delay creation of mapped features.
*
* Revision 1.15  2003/02/05 17:59:17  dicuccio
* Moved formerly private headers into include/objects/objmgr/impl
*
* Revision 1.14  2003/02/04 21:46:32  grichenk
* Added map of annotations by intervals (the old one was
* by total ranges)
*
* Revision 1.13  2003/01/29 17:45:03  vasilche
* Annotaions index is split by annotation/feature type.
*
* Revision 1.12  2003/01/22 20:11:54  vasilche
* Merged functionality of CSeqMapResolved_CI to CSeqMap_CI.
* CSeqMap_CI now supports resolution and iteration over sequence range.
* Added several caches to CScope.
* Optimized CSeqVector().
* Added serveral variants of CBioseqHandle::GetSeqVector().
* Tried to optimize annotations iterator (not much success).
* Rewritten CHandleRange and CHandleRangeMap classes to avoid sorting of list.
*
* Revision 1.11  2002/12/26 20:55:18  dicuccio
* Moved seq_id_mapper.hpp, tse_info.hpp, and bioseq_info.hpp -> include/ tree
*
* Revision 1.10  2002/12/26 16:39:24  vasilche
* Object manager class CSeqMap rewritten.
*
* Revision 1.9  2002/10/18 19:12:40  grichenk
* Removed mutex pools, converted most static mutexes to non-static.
* Protected CSeqMap::x_Resolve() with mutex. Modified code to prevent
* dead-locks.
*
* Revision 1.8  2002/07/10 16:50:33  grichenk
* Fixed bug with duplicate and uninitialized atomic counters
*
* Revision 1.7  2002/07/08 20:51:02  grichenk
* Moved log to the end of file
* Replaced static mutex (in CScope, CDataSource) with the mutex
* pool. Redesigned CDataSource data locking.
*
* Revision 1.6  2002/07/01 15:32:30  grichenk
* Fixed 'unused variable depth3' warning
*
* Revision 1.5  2002/05/31 17:53:00  grichenk
* Optimized for better performance (CTSE_Info uses atomic counter,
* delayed annotations indexing, no location convertions in
* CAnnot_Types_CI if no references resolution is required etc.)
*
* Revision 1.4  2002/05/29 21:21:13  gouriano
* added debug dump
*
* Revision 1.3  2002/03/14 18:39:13  gouriano
* added mutex for MT protection
*
* Revision 1.2  2002/02/21 19:27:06  grichenk
* Rearranged includes. Added scope history. Added searching for the
* best seq-id match in data sources and scopes. Updated tests.
*
* Revision 1.1  2002/02/07 21:25:05  grichenk
* Initial revision
*
*
* ===========================================================================
*/
