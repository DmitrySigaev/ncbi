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


#include <objects/objmgr/tse_info.hpp>

#include "annot_object.hpp"

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


////////////////////////////////////////////////////////////////////
//
//  CTSE_Info::
//
//    General information and indexes for top level seq-entries
//


CTSE_Info::CTSE_Info(void)
    : m_Dead(false)
{
    Set(0);
}


CTSE_Info::~CTSE_Info(void)
{
}


void CTSE_Info::CounterOverflow(void) const
{
    THROW1_TRACE(runtime_error,
                 "CTSE_Info::Lock() -- TSE lock counter overflow");
}


void CTSE_Info::CounterUnderflow(void) const
{
    THROW1_TRACE(runtime_error,
                 "CTSE_Info::Unlock() -- The TSE is not locked");
}


void CTSE_Info::DebugDump(CDebugDumpContext ddc, unsigned int depth) const
{
    ddc.SetFrame("CTSE_Info");
    CObject::DebugDump( ddc, depth);

    ddc.Log("m_TSE", m_TSE.GetPointer(),0);
    ddc.Log("m_Dead", m_Dead);
    if (depth == 0) {
        DebugDumpValue(ddc, "m_BioseqMap.size()", m_BioseqMap.size());
        DebugDumpValue(ddc, "m_AnnotMap_ByInt.size()",  m_AnnotMap_ByInt.size());
        DebugDumpValue(ddc, "m_AnnotMap_ByTotal.size()",  m_AnnotMap_ByTotal.size());
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
            for (it=m_AnnotMap_ByInt.begin(); it!=m_AnnotMap_ByInt.end(); ++it) {
                string member_name = "m_AnnotMap_ByInt[ " +
                    (it->first).AsString() +" ]";
                if (depth2 == 0) {
                    member_name += "size()";
                    DebugDumpValue(ddc2, member_name, (it->second).size());
                } else {
/*
                    // CRangeMultimap
                    CDebugDumpContext ddc3(ddc2, member_name);
                    iterate ( TRangeMap, itrm, it->second ) {
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
        { //--- m_AnnotMap_ByTotal
            DebugDumpValue(ddc, "m_AnnotMap_ByTotal.type",
                "map<CSeq_id_Handle, CRangeMultimap<CRef<CAnnotObject>,"
                "CRange<TSeqPos>::position_type>>");

            CDebugDumpContext ddc2(ddc,"m_AnnotMap_ByTotal");
            TAnnotMap::const_iterator it;
            for (it=m_AnnotMap_ByTotal.begin(); it!=m_AnnotMap_ByTotal.end(); ++it) {
                string member_name = "m_AnnotMap_ByTotal[ " +
                    (it->first).AsString() +" ]";
                if (depth2 == 0) {
                    member_name += "size()";
                    DebugDumpValue(ddc2, member_name, (it->second).size());
                } else {
                    // CRangeMultimap
                    /*
                    CDebugDumpContext ddc3(ddc2, member_name);
                    iterate ( TRangeMap, itrm, it->second ) {
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
    DebugDumpValue(ddc, "CMutableAtomicCounter::Get()", Get());
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
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
