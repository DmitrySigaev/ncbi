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
*   DataSource for object manager
*
*/


#include <objects/objmgr/impl/data_source.hpp>
#include "annot_object.hpp"
#include <objects/objmgr/impl/handle_range_map.hpp>
#include <objects/objmgr/seq_vector.hpp>
#include <objects/general/Int_fuzz.hpp>
#include <objects/seq/Seqdesc.hpp>
#include <objects/seq/Seq_descr.hpp>
#include <objects/seq/Seq_ext.hpp>
#include <objects/seq/Seq_hist.hpp>
#include <objects/seq/Seg_ext.hpp>
#include <objects/seq/Ref_ext.hpp>
#include <objects/seq/Delta_ext.hpp>
#include <objects/seq/Delta_seq.hpp>
#include <objects/seq/Seq_literal.hpp>
#include <objects/seq/Seq_inst.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objects/seqset/Bioseq_set.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seqloc/Seq_point.hpp>
#include <objects/seqloc/Seq_loc_equiv.hpp>
#include <objects/seq/seqport_util.hpp>
#include <serial/iterator.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


CDataSource::CDataSource(CDataLoader& loader, CObjectManager& objmgr)
    : m_Loader(&loader), m_pTopEntry(0), m_ObjMgr(&objmgr)
{
    m_Loader->SetTargetDataSource(*this);
}


CDataSource::CDataSource(CSeq_entry& entry, CObjectManager& objmgr)
    : m_Loader(0), m_pTopEntry(&entry), m_ObjMgr(&objmgr),
      m_IndexedAnnot(false)
{
    x_AddToBioseqMap(entry, false, 0);
}


CDataSource::~CDataSource(void)
{
    // Find and drop each TSE
    while (m_Entries.size() > 0) {
        _ASSERT( !m_Entries.begin()->second->CounterLocked() );
        DropTSE(*(m_Entries.begin()->second->m_TSE));
    }
    if(m_Loader) delete m_Loader;
}


CTSE_Info* CDataSource::x_FindBestTSE(const CSeq_id_Handle& handle,
                                      const CScope::TRequestHistory& history) const
{
//### Don't forget to unlock TSE in the calling function!
    TTSESet* p_tse_set = 0;
    CMutexGuard ds_guard(m_DataSource_Mtx);
    TTSEMap::const_iterator tse_set = m_TSE_seq.find(handle);
    if ( tse_set == m_TSE_seq.end() ) {
        return 0;
    }
    p_tse_set = const_cast<TTSESet*>(&tse_set->second);
    if ( p_tse_set->size() == 1) {
        // There is only one TSE, no matter live or dead
        const CTSE_Info* info = *p_tse_set->begin();
        info->LockCounter();
        return const_cast<CTSE_Info*>(info);
    }
    // The map should not contain empty entries
    _ASSERT(p_tse_set->size() > 0);
    TTSESet live;
    const CTSE_Info* from_history = 0;
    iterate(TTSESet, tse, *p_tse_set) {
        // Check history
        CScope::TRequestHistory::const_iterator hst = history.find(*tse);
        if (hst != history.end()) {
            if ( from_history ) {
                THROW1_TRACE(runtime_error,
                    "CDataSource::x_FindBestTSE() -- Multiple history matches");
            }
            from_history = *tse;
        }
        // Find live TSEs
        if ( !(*tse)->m_Dead ) {
            // Make sure there is only one live TSE
            live.insert(*tse);
        }
    }
    // History is always the best choice
    if (from_history) {
        from_history->LockCounter();
        return const_cast<CTSE_Info*>(from_history);
    }

    // Check live
    if (live.size() == 1) {
        // There is only one live TSE -- ok to use it
        CTSE_Info* info = const_cast<CTSE_Info*>(live.begin()->GetPointer());
        info->LockCounter();
        return info;
    }
    else if ((live.size() == 0)  &&  m_Loader) {
        // No live TSEs -- try to select the best dead TSE
        CRef<CTSE_Info> best(m_Loader->ResolveConflict(handle, *p_tse_set));
        if ( best ) {
            const CTSE_Info* info = *p_tse_set->find(best);
            info->LockCounter();
            return const_cast<CTSE_Info*>(info);
        }
        THROW1_TRACE(runtime_error,
            "CDataSource::x_FindBestTSE() -- Multiple seq-id matches found");
    }
    // Multiple live TSEs -- try to resolve the conflict (the status of some
    // TSEs may change)
    CRef<CTSE_Info> best(m_Loader->ResolveConflict(handle, live));
    if ( best ) {
        const CTSE_Info* info = *p_tse_set->find(best);
        info->LockCounter();
        return const_cast<CTSE_Info*>(info);
    }
    THROW1_TRACE(runtime_error,
        "CDataSource::x_FindBestTSE() -- Multiple live entries found");
}


CBioseq_Handle CDataSource::GetBioseqHandle(CScope& scope,
                                            CSeqMatch_Info& info)
{
    // The TSE is locked by the scope, so, it can not be deleted.
    CSeq_entry* se = 0;
    {{
        //### CMutexGuard guard(sm_DataSource_Mtx);
        TBioseqMap::iterator found = info.m_TSE->
            m_BioseqMap.find(info.m_Handle);
        _ASSERT(found != info.m_TSE->m_BioseqMap.end());
        se = found->second->m_Entry;
    }}
    CBioseq_Handle h(info.m_Handle);
    h.x_ResolveTo(scope, *this, *se, *info.m_TSE);
    // Locked by BestResolve() in CScope::x_BestResolve()
    info.m_TSE->UnlockCounter();
    return h;
}


void CDataSource::FilterSeqid(TSeq_id_HandleSet& setResult,
                              TSeq_id_HandleSet& setSource) const
{
    _ASSERT(&setResult != &setSource);
    // for each handle
    TSeq_id_HandleSet::iterator itHandle;
    // CMutexGuard guard(m_DataSource_Mtx);
    for( itHandle = setSource.begin(); itHandle != setSource.end(); ) {
        // if it is in my map
        if (m_TSE_seq.find(*itHandle) != m_TSE_seq.end()) {
            //### The id handle is reported to be good, but it can be deleted
            //### by the next request!
            setResult.insert(*itHandle);
        }
        ++itHandle;
    }
}


const CBioseq& CDataSource::GetBioseq(const CBioseq_Handle& handle)
{
    // Bioseq core and TSE must be loaded if there exists a handle
    // Loader may be called to load descriptions (not included in core)
    if ( m_Loader ) {
        // Send request to the loader
        CHandleRangeMap hrm;
        hrm.AddRange(handle.m_Value, CHandleRange::TRange::GetWhole(), eNa_strand_unknown);
        m_Loader->GetRecords(hrm, CDataLoader::eBioseq);
    }
    //### CMutexGuard guard(sm_DataSource_Mtx);
    // the handle must be resolved to this data source
    _ASSERT(handle.m_DataSource == this);
    return handle.m_Entry->GetSeq();
}


const CSeq_entry& CDataSource::GetTSE(const CBioseq_Handle& handle)
{
    // Bioseq and TSE must be loaded if there exists a handle
    //### CMutexGuard guard(sm_DataSource_Mtx);
    _ASSERT(handle.m_DataSource == this);
    return *m_Entries[CRef<CSeq_entry>(handle.m_Entry)]->m_TSE;
}


CBioseq_Handle::TBioseqCore
CDataSource::GetBioseqCore(const CBioseq_Handle& handle)
{
    // Bioseq core and TSE must be loaded if there exists a handle --
    // just return the bioseq as-is.
    // the handle must be resolved to this data source
    _ASSERT(handle.m_DataSource == this);
    return CBioseq_Handle::TBioseqCore(&handle.m_Entry->GetSeq());
}


const CSeqMap& CDataSource::GetSeqMap(const CBioseq_Handle& handle)
{
    CMutexGuard guard(m_DataSource_Mtx);
    return x_GetSeqMap(handle);
}


CSeqMap& CDataSource::x_GetSeqMap(const CBioseq_Handle& handle)
{
    _ASSERT(handle.m_DataSource == this);
    // No need to lock anything since the TSE should be locked by the handle
    CBioseq_Handle::TBioseqCore core = GetBioseqCore(handle);
    //### Lock seq-maps to prevent duplicate seq-map creation
    CMutexGuard guard(m_DataSource_Mtx);    
    TSeqMaps::iterator found = m_SeqMaps.find(core);
    if (found == m_SeqMaps.end()) {
        // Call loader first
        if ( m_Loader ) {
            // Send request to the loader
            CHandleRangeMap hrm;
            hrm.AddRange(handle.m_Value, CHandleRange::TRange::GetWhole(), eNa_strand_unknown);
            m_Loader->GetRecords(hrm, CDataLoader::eBioseq); //### or eCore???
        }

        // Create sequence map
        x_CreateSeqMap(GetBioseq(handle), *handle.m_Scope);
        found = m_SeqMaps.find(core);
        if (found == m_SeqMaps.end()) {
            THROW1_TRACE(runtime_error,
                         "CDataSource::x_GetSeqMap() -- "
                         "Sequence map not found");
        }
    }
    _ASSERT(found != m_SeqMaps.end());
    return *found->second;
}

/*
const CSeqMap& CDataSource::GetResolvedSeqMap(const CBioseq_Handle& handle,
                                              CScope& scope)
{
    CMutexGuard guard(m_DataSource_Mtx);
    return x_CreateResolvedSeqMap(x_GetSeqMap(handle), scope);
}


const CSeqMap& CDataSource::GetResolvedSeqMap(const CBioseq_Handle& handle)
{
    return GetResolvedSeqMap(handle, *handle.m_Scope);
}
*/

bool CDataSource::GetSequence(const CBioseq_Handle& handle,
                              TSeqPos point,
                              SSeqData* seq_piece,
                              CScope& scope)
{
    //### CMutexGuard guard(sm_DataSource_Mtx);
    if (handle.m_DataSource != this  &&  handle.m_DataSource != 0) {
        // Resolved to a different data source
        return false;
    }
    CSeq_entry* entry = handle.m_Entry;
    CBioseq_Handle rhandle = handle; // resolved handle for local use
    if ( !entry ) {
        //### The TSE returns locked - unlock it
        CTSE_Info* info = x_FindBestTSE(rhandle.GetKey(), scope.m_History);
        CTSE_Guard guard(*info);
        if ( !info )
            return false;
        info->UnlockCounter();
        entry = info->m_BioseqMap[rhandle.GetKey()]->m_Entry;
        rhandle.x_ResolveTo(scope, *this, *entry, *info);
    }
    _ASSERT(entry->IsSeq());
    CBioseq& seq = entry->SetSeq();
    if ( seq.GetInst().IsSetSeq_data() ) {
        // Simple sequence -- just return seq-data
        seq_piece->dest_start = 0;
        seq_piece->src_start = 0;
        seq_piece->length = seq.GetInst().GetLength();
        seq_piece->src_data = &seq.GetInst().GetSeq_data();
        return true;
    }
    if ( seq.GetInst().IsSetExt() )
    {
        // Seq-ext: check the extension type, prepare the data
        CSeqMap& seqmap = x_GetSeqMap(rhandle);
        // Omit the last element - it is always eSeqEnd
        CSeqMap::CSegmentInfo seg = seqmap.FindSegment(point, &scope);
        /*
        if (seg.GetPosition() > point  ||
            seg.GetPosition() + seg.GetLength() - 1 < point) {
            // This may happen when the x_Resolve() was unable to
            // resolve some references before the point and the total
            // length of the sequence appears to be less than point.
            // Immitate a gap of length 1.
            seq_piece->dest_start = point;
            seq_piece->length = 1;
            seq_piece->src_data = 0;
            seq_piece->src_start = 0;
            return true;
        }
        */
        switch (seg.GetType()) {
        case CSeqMap::eSeqData:
            {
                seq_piece->dest_start = seg.GetPosition();
                seq_piece->src_start = 0;
                seq_piece->length = seg.GetLength();
                seq_piece->src_data = &seg.GetData();
                return true;
            }
        case CSeqMap::eSeqRef:
            {
                TSignedSeqPos shift = seg.GetRefPosition() - seg.GetPosition();
                if ( scope.x_GetSequence(seg.GetRefSeqid(),
                                         point + shift, seq_piece) ) {
                    TSeqPos xL = seg.GetLength();
                    TSignedSeqPos delta = seg.GetRefPosition() -
                        seq_piece->dest_start;
                    seq_piece->dest_start = seg.GetPosition();
                    if (delta < 0) {
                        // Got less then requested (delta is negative: -=)
                        seq_piece->dest_start -= delta;
                        xL += delta;
                    }
                    else {
                        // Got more than requested
                        seq_piece->src_start += delta;
                        seq_piece->length -= delta;
                    }
                    if (seq_piece->length > xL)
                        seq_piece->length = xL;
                    if ( seg.GetRefMinusStrand() &&
                         seq_piece->src_data != 0 ) {
                        // Convert data, update location
                        CSeq_data* tmp = new CSeq_data;
                        CSeqportUtil::ReverseComplement(
                            *seq_piece->src_data, tmp,
                            seq_piece->src_start, seq_piece->length);
                        seq_piece->src_start = 0;
                        seq_piece->src_data = tmp;
                    }
                    return true;
                }
                else {
                    seq_piece->dest_start = seg.GetPosition();
                    seq_piece->length = seg.GetLength();
                    seq_piece->src_data = 0;
                    seq_piece->src_start = 0;
                    return true;
                }
            }
        case CSeqMap::eSeqGap:
            {
                seq_piece->dest_start = seg.GetPosition();
                seq_piece->src_start = 0;
                seq_piece->length = seg.GetLength();
                seq_piece->src_data = 0;
                return true;
            }
#if 0
        case CSeqMap::eSeqEnd:
            {
                THROW1_TRACE(runtime_error,
                    "CDataSource::GetSequence() -- Attempt to read beyond sequence end");
            }
#endif
        }
    }
    return false;
}


CTSE_Info* CDataSource::AddTSE(CSeq_entry& se, TTSESet* tse_set, bool dead)
{
    //### CMutexGuard guard(sm_DataSource_Mtx);
    return x_AddToBioseqMap(se, dead, tse_set);
}


CSeq_entry* CDataSource::x_FindEntry(const CSeq_entry& entry)
{
    CRef<CSeq_entry> ref(const_cast<CSeq_entry*>(&entry));
    //### CMutexGuard guard(sm_DataSource_Mtx);
    //### Lock the entries list to prevent "found" destruction
    CMutexGuard guard(m_DataSource_Mtx);
    TEntries::iterator found = m_Entries.find(ref);
    if (found == m_Entries.end())
        return 0;
    return const_cast<CSeq_entry*>(found->first.GetPointer());
}


bool CDataSource::AttachEntry(const CSeq_entry& parent, CSeq_entry& bioseq)
{
    //### CMutexGuard guard(sm_DataSource_Mtx);

    //### Lock the entry to prevent destruction or modification ???
    //### May need to find and lock the TSE_Info for this.
    CSeq_entry* found = x_FindEntry(parent);
    if ( !found ) {
        return false;
    }
    // Lock the TSE
    CTSE_Guard tse_guard(*m_Entries[CRef<CSeq_entry>(found)]);
    _ASSERT(found  &&  found->IsSet());

    found->SetSet().SetSeq_set().push_back(CRef<CSeq_entry>(&bioseq));

    // Re-parentize, update index
    CSeq_entry* top = found;
    while ( top->GetParentEntry() ) {
        top = top->GetParentEntry();
    }
    top->Parentize();
    // The "dead" parameter is not used here since the TSE_Info
    // structure must have been created already.
    x_IndexEntry(bioseq, *top, false, 0);
    return true;
}


bool CDataSource::AttachMap(const CSeq_entry& bioseq, CSeqMap& seqmap)
{
    //### CMutexGuard guard(sm_DataSource_Mtx);
    //### Lock the entry to prevent destruction or modification
    //### May need to lock the TSE instead.
    CSeq_entry* found = x_FindEntry(bioseq);
    if ( !found ) {
        return false;
    }
    // Lock the TSE
    CTSE_Guard tse_guard(*m_Entries[CRef<CSeq_entry>(found)]);

    CSeq_entry& entry = *found;
    _ASSERT(entry.IsSeq());
    m_SeqMaps[CBioseq_Handle::TBioseqCore(&entry.GetSeq())].Reset(&seqmap);
    return true;
}


#if 0
bool CDataSource::AttachSeqData(const CSeq_entry& bioseq,
                                const CDelta_ext& delta,
                                size_t index,
                                TSeqPos start,
                                TSeqPos length)
{
    vector<CSeqMap::CSegment> block;
    x_AppendDelta(delta, block);
    x_GetSeqMap(bioseq).AddBlock(block, index, start, length);
    return true;
}

bool CDataSource::AttachSeqData(const CSeq_entry& bioseq,
                                CDelta_seq& seq_seg,
                                TSeqPos start,
                                TSeqPos length)
{
/*
    The function should be used mostly by data loaders. If a segment of a
    bioseq is a reference to a whole sequence, the positioning mechanism may
    not work correctly, since the length of such reference is unknown. In most
    cases "whole" reference should be the only segment of a delta-ext.
*/
    // Get non-const reference to the entry
    //### CMutexGuard guard(sm_DataSource_Mtx);
    //### Lock the entry to prevent destruction or modification
    //### May need to lock the TSE instead.
    CSeq_entry* found = x_FindEntry(bioseq);
    if ( !found ) {
        return false;
    }
    // Lock the TSE
    CTSE_Guard tse_guard(*m_Entries[CRef<CSeq_entry>(found)]);

    CSeq_entry& entry = *found;
    _ASSERT( entry.IsSeq() );
    CSeq_inst& inst = entry.SetSeq().SetInst();
    if (start == 0  &&
        inst.IsSetLength()  &&
        inst.GetLength() == length  &&
        seq_seg.IsLiteral()  &&
        seq_seg.GetLiteral().IsSetSeq_data()) {
        // Non-segmented sequence, non-reference segment -- just add seq-data
        entry.SetSeq().SetInst().SetSeq_data(
            seq_seg.SetLiteral().SetSeq_data());
        return true;
    }
    // Possibly segmented sequence. Use delta-ext.
    _ASSERT( !inst.IsSetSeq_data() );
    CDelta_ext::Tdata& delta = inst.SetExt().SetDelta().Set();
    TSeqPosition ex_lower = 0; // the last of the existing points before start
    TSeqPosition ex_upper = 0; // the first of the existing points after start
    TSeqPosition ex_cur = 0; // current position while processing the sequence
    CDelta_ext::Tdata::iterator gap = delta.end();
    non_const_iterate(CDelta_ext::Tdata, dit, delta) {
        ex_lower = ex_cur;
        if ( (*dit)->IsLiteral() ) {
            if ( (*dit)->GetLiteral().IsSetSeq_data() ) {
                ex_cur += (*dit)->GetLiteral().GetLength();
                if (ex_cur <= start)
                    continue;
                // ex_cur > start
                // This is not good - we could not find the correct
                // match for the new segment start. The start should be
                // in a gap, not in a real literal.
                THROW1_TRACE(runtime_error,
                    "CDataSource::AttachSeqData() -- Can not insert segment into a literal");
            }
            else {
                // No data exist - treat it like a gap
                if ((*dit)->GetLiteral().GetLength() > 0) {
                    // Known length -- check if the starting point
                    // is whithin this gap.
                    ex_cur += (*dit)->GetLiteral().GetLength();
                    if (ex_cur < start)
                        continue;
                    // The new segment must be whithin this gap.
                    ex_upper = ex_cur;
                    gap = dit;
                    break;
                }
                // Gap of 0 or unknown length
                if (ex_cur == start) {
                    ex_upper = ex_cur;
                    gap = dit;
                    break;
                }
            }
        }
        else if ( (*dit)->IsLoc() ) {
            CSeqMap dummyseqmap; // Just to calculate the seq-loc length
            x_LocToSeqMap((*dit)->GetLoc(), ex_cur, dummyseqmap);
            if (ex_cur <= start) {
                continue;
            }
            // ex_cur > start
            THROW1_TRACE(runtime_error,
                "CDataSource::AttachSeqData() -- Segment position conflict");
        }
        else {
            THROW1_TRACE(runtime_error,
                "CDataSource::AttachSeqData() -- Invalid delta-seq type");
        }
    }
    if ( gap != delta.end() ) {
        // Found the correct gap
        if ( ex_upper > start ) {
            // Insert the new segment before the gap
            delta.insert(gap, CRef<CDelta_seq>(&seq_seg));
        }
        else if ( ex_lower < start ) {
            // Insert the new segment after the gap
            ++gap;
            delta.insert(gap, CRef<CDelta_seq>(&seq_seg));
        }
        else {
            // Split the gap, insert the new segment between the two gaps
            CRef<CDelta_seq> gap_dup(new CDelta_seq);
            gap_dup->Assign(**gap);
            if ((*gap)->GetLiteral().GetLength() > 0) {
                // Adjust gap lengths
                _ASSERT((*gap)->GetLiteral().GetLength() >= length);
                gap_dup->SetLiteral().SetLength(start - ex_lower);
                (*gap)->SetLiteral().SetLength(ex_upper - start - length);
            }
            // Insert new_seg before gap, and gap_dup before new_seg
            delta.insert(delta.insert(gap, CRef<CDelta_seq>(&seq_seg)), gap_dup);
        }
    }
    else {
        // No gap found -- The sequence must be empty
        _ASSERT(delta.size() == 0);
        delta.push_back(CRef<CDelta_seq>(&seq_seg));
    }
    // Replace seq-map for the updated bioseq if it was already created
    x_CreateSeqMap( entry.SetSeq() );
    return true;
}
#endif

bool CDataSource::AttachAnnot(const CSeq_entry& entry,
                           CSeq_annot& annot)
{
    //### CMutexGuard guard(sm_DataSource_Mtx);
    //### Lock the entry to prevent destruction or modification
    //### May need to lock the TSE instead. In this case also lock
    //### the entries list for a while.
    CSeq_entry* found = x_FindEntry(entry);
    if ( !found ) {
        return false;
    }

    CTSE_Info* tse = m_Entries[CRef<CSeq_entry>(found)];
    //### Lock the TSE !!!!!!!!!!!
    CTSE_Guard tse_guard(*tse);

    CBioseq_set::TAnnot* annot_list = 0;
    if ( found->IsSet() ) {
        annot_list = &found->SetSet().SetAnnot();
    }
    else {
        annot_list = &found->SetSeq().SetAnnot();
    }
    annot_list->push_back(CRef<CSeq_annot>(&annot));

    if ( !m_IndexedAnnot )
        return true; // skip annotations indexing

    switch ( annot.GetData().Which() ) {
        case CSeq_annot::C_Data::e_Ftable:
            {
                CRef<CSeq_annot_Info> annot_ref(
                    new CSeq_annot_Info(this, annot, &entry));
                iterate ( CSeq_annot::C_Data::TFtable, fi,
                    annot.GetData().GetFtable() ) {
                    x_MapFeature(**fi, *annot_ref, *tse);
                }
                break;
            }
        case CSeq_annot::C_Data::e_Align:
            {
                CRef<CSeq_annot_Info> annot_ref(
                    new CSeq_annot_Info(this, annot, &entry));
                iterate ( CSeq_annot::C_Data::TAlign, ai,
                    annot.GetData().GetAlign() ) {
                    x_MapAlign(**ai, *annot_ref, *tse);
                }
                break;
            }
        case CSeq_annot::C_Data::e_Graph:
            {
                CRef<CSeq_annot_Info> annot_ref(
                    new CSeq_annot_Info(this, annot, &entry));
                iterate ( CSeq_annot::C_Data::TGraph, gi,
                    annot.GetData().GetGraph() ) {
                    x_MapGraph(**gi, *annot_ref, *tse);
                }
                break;
            }
        default:
            {
                return false;
            }
        }
    return true;
}


bool CDataSource::RemoveAnnot(const CSeq_entry& entry, const CSeq_annot& annot)
{
    if ( m_Loader ) {
        THROW1_TRACE(runtime_error,
            "CDataSource::RemoveAnnot() -- can not modify a loaded entry");
    }
    CSeq_entry* found = x_FindEntry(entry);
    if ( !found ) {
        return false;
    }

    CTSE_Info* tse = m_Entries[CRef<CSeq_entry>(found)];
    //### Lock the TSE !!!!!!!!!!!
    CTSE_Guard tse_guard(*tse);
    x_DropAnnotMapRecursive(*tse->m_TSE);
    m_IndexedAnnot = false;
    tse->SetIndexed(false);

    if ( found->IsSet() ) {
        found->SetSet().SetAnnot().
            remove(CRef<CSeq_annot>(&const_cast<CSeq_annot&>(annot)));
        if (found->SetSet().SetAnnot().size() == 0) {
            found->SetSet().ResetAnnot();
        }
    }
    else {
        found->SetSeq().SetAnnot().
            remove(CRef<CSeq_annot>(&const_cast<CSeq_annot&>(annot)));
        if (found->SetSeq().SetAnnot().size() == 0) {
            found->SetSeq().ResetAnnot();
        }
    }
    return true;
}


bool CDataSource::ReplaceAnnot(const CSeq_entry& entry,
                               const CSeq_annot& old_annot,
                               CSeq_annot& new_annot)
{
    if ( !RemoveAnnot(entry, old_annot) )
        return false;
    return AttachAnnot(entry, new_annot);
}


CTSE_Info* CDataSource::x_AddToBioseqMap(CSeq_entry& entry,
                                         bool dead,
                                         TTSESet* tse_set)
{
    // Search for bioseqs, add each to map
    //### The entry is not locked here
    entry.Parentize();
    CTSE_Info* info = x_IndexEntry(entry, entry, dead, tse_set);
    // Do not index new TSEs -- wait for annotations request
    _ASSERT(!info->IsIndexed());  // Can not be indexed - it's a new one
    m_IndexedAnnot = false;       // reset the flag to enable indexing
    return info;
}


CTSE_Info* CDataSource::x_IndexEntry(CSeq_entry& entry, CSeq_entry& tse,
                                     bool dead,
                                     TTSESet* tse_set)
{
    CTSE_Info* tse_info = 0;
    // Lock indexes to prevent duplicate indexing
    CMutexGuard entries_guard(m_DataSource_Mtx);    

    TEntries::iterator found_tse = m_Entries.find(CRef<CSeq_entry>(&tse));
    if (found_tse == m_Entries.end()) {
        // New TSE info
        tse_info = new CTSE_Info;
        tse_info->m_TSE = &tse;
        tse_info->m_Dead = dead;
        // Do not lock TSE if there is no tse_set -- none will unlock it
        if (tse_set) {
            tse_info->LockCounter();
        }
    }
    else {
        // existing TSE info
        tse_info = found_tse->second;
    }
    _ASSERT(tse_info);
    CTSE_Guard tse_guard(*tse_info);
     
    m_Entries[CRef<CSeq_entry>(&entry)] = tse_info;
    if ( entry.IsSeq() ) {
        CBioseq* seq = &entry.SetSeq();
        CBioseq_Info* info = new CBioseq_Info(entry);
        iterate ( CBioseq::TId, id, seq->GetId() ) {
            // Find the bioseq index
            CSeq_id_Handle key = GetSeq_id_Mapper().GetHandle(**id);
            TTSESet& seq_tse_set = m_TSE_seq[key];
            TTSESet::iterator tse_it = seq_tse_set.begin();
            for ( ; tse_it != seq_tse_set.end(); ++tse_it) {
                if (const_cast< CRef<CSeq_entry>& >((*tse_it)->m_TSE) == &tse)
                    break;
            }
            if (tse_it == seq_tse_set.end()) {
                seq_tse_set.insert(CRef<CTSE_Info>(tse_info));
            }
            else {
                tse_info = const_cast<CTSE_Info*>(tse_it->GetPointer());
            }
            TBioseqMap::iterator found = tse_info->m_BioseqMap.find(key);
            if ( found != tse_info->m_BioseqMap.end() ) {
                // No duplicate bioseqs in the same TSE
                string sid1, sid2, si_conflict;
                {{
                    CNcbiOstrstream os;
                    iterate (CBioseq::TId, id_it, found->second->m_Entry->GetSeq().GetId()) {
                        os << (*id_it)->DumpAsFasta() << " | ";
                    }
                    sid1 = CNcbiOstrstreamToString(os);
                }}
                {{
                    CNcbiOstrstream os;
                    iterate (CBioseq::TId, id_it, seq->GetId()) {
                        os << (*id_it)->DumpAsFasta() << " | ";
                    }
                    sid2 = CNcbiOstrstreamToString(os);
                }}
                {{
                    CNcbiOstrstream os;
                    os << (*id)->DumpAsFasta();
                    si_conflict = CNcbiOstrstreamToString(os);
                }}
                THROW1_TRACE(runtime_error,
                    " duplicate Bioseq id '" + si_conflict + "' present in" +
                    "\n  seq1: " + sid1 +
                    "\n  seq2: " + sid2);
            }
            else {
                // Add new seq-id synonym
                info->m_Synonyms.insert(key);
            }
        }
        iterate ( CBioseq_Info::TSynonyms, syn, info->m_Synonyms ) {
            tse_info->m_BioseqMap.insert
                (TBioseqMap::value_type(*syn,
                                        CRef<CBioseq_Info>(info)));
        }
    }
    else {
        iterate ( CBioseq_set::TSeq_set, it, entry.GetSet().GetSeq_set() ) {
            x_IndexEntry(const_cast<CSeq_entry&>(**it), tse, dead, 0);
        }
    }
    if (tse_set) {
        tse_set->insert(CRef<CTSE_Info>(tse_info));
    }
    return tse_info;
}


void CDataSource::x_AddToAnnotMap(CSeq_entry& entry, CTSE_Info* info)
{
    // The entry must be already in the m_Entries map
    // Lock indexes
    CMutexGuard guard(m_DataSource_Mtx);    

    CTSE_Info* tse = info ? info : m_Entries[CRef<CSeq_entry>(&entry)];
    CTSE_Guard tse_guard(*tse);
    tse->SetIndexed(true);
    const CBioseq::TAnnot* annot_list = 0;
    if ( entry.IsSeq() ) {
        if ( entry.GetSeq().IsSetAnnot() ) {
            annot_list = &entry.GetSeq().GetAnnot();
        }
    }
    else {
        if ( entry.GetSet().IsSetAnnot() ) {
            annot_list = &entry.GetSet().GetAnnot();
        }
        iterate ( CBioseq_set::TSeq_set, it, entry.GetSet().GetSeq_set() ) {
            x_AddToAnnotMap(const_cast<CSeq_entry&>(**it), tse);
        }
    }
    if ( !annot_list ) {
        return;
    }
    iterate ( CBioseq::TAnnot, ai, *annot_list ) {
        switch ( (*ai)->GetData().Which() ) {
        case CSeq_annot::C_Data::e_Ftable:
            {
                CRef<CSeq_annot_Info> annot_ref(
                    new CSeq_annot_Info(this, **ai, &entry));
                iterate ( CSeq_annot::C_Data::TFtable, it,
                    (*ai)->GetData().GetFtable() ) {
                    x_MapFeature(**it, *annot_ref, *tse);
                }
                break;
            }
        case CSeq_annot::C_Data::e_Align:
            {
                CRef<CSeq_annot_Info> annot_ref(
                    new CSeq_annot_Info(this, **ai, &entry));
                iterate ( CSeq_annot::C_Data::TAlign, it,
                    (*ai)->GetData().GetAlign() ) {
                    x_MapAlign(**it, *annot_ref, *tse);
                }
                break;
            }
        case CSeq_annot::C_Data::e_Graph:
            {
                CRef<CSeq_annot_Info> annot_ref(
                    new CSeq_annot_Info(this, **ai, &entry));
                iterate ( CSeq_annot::C_Data::TGraph, it,
                    (*ai)->GetData().GetGraph() ) {
                    x_MapGraph(**it, *annot_ref, *tse);
                }
                break;
            }
        }
    }
}


void PrintSeqMap(const string& id, const CSeqMap& smap)
{
#if _DEBUG && 0
    _TRACE("CSeqMap("<<id<<"):");
    iterate ( CSeqMap, it, smap ) {
        switch ( it.GetType() ) {
        case CSeqMap::eSeqGap:
            _TRACE("    gap: "<<it.GetLength());
            break;
        case CSeqMap::eSeqData:
            _TRACE("    data: "<<it.GetLength());
            break;
        case CSeqMap::eSeqRef:
            _TRACE("    ref: "<<it.GetRefSeqid().AsString()<<' '<<it.GetRefPosition()<<' '<<it.GetLength()<<' '<<it.GetRefMinusStrand());
            break;
        default:
            _TRACE("    bad: "<<it.GetType()<<' '<<it.GetLength());
            break;
        }
    }
    _TRACE("end of CSeqMap "<<id);
#endif
}

#if 0
void CDataSource::x_AppendDelta(const CDelta_ext& delta,
                                vector<CSeqMap::CSegment>& block)
{
    const CDelta_ext::Tdata& data = delta.Get();
    iterate ( CDelta_ext::Tdata, it, data ) {
        switch ( (*it)->Which() ) {
        case CDelta_seq::e_Loc:
            x_AppendLoc((*it)->GetLoc(), block);
            break;
        case CDelta_seq::e_Literal:
        {
            const CSeq_literal& literal = (*it)->GetLiteral();
            if ( literal.IsSetSeq_data() ) {
                block.push_back(CSeqMap::CSegment(literal.GetSeq_data(),
                                                  literal.GetLength()));
            }
            else {
                // No data exist - treat it like a gap
                block.push_back(CSeqMap::CSegment(CSeqMap::eSeqGap,
                                                  literal.GetLength())); //???
            }
            break;
        }
        }
    }
}
#endif


void CDataSource::x_CreateSeqMap(const CBioseq& seq, CScope& scope)
{
    //### Make sure the bioseq is not deleted while creating the seq-map
    CConstRef<CBioseq> guard(&seq);
    CConstRef<CSeqMap> seqMap =
        CSeqMap::CreateSeqMapForBioseq(const_cast<CBioseq&>(seq), this);
    m_SeqMaps[CBioseq_Handle::TBioseqCore(&seq)]
        .Reset(const_cast<CSeqMap*>(seqMap.GetPointer()));
}

#if 0
const CSeqMap& CDataSource::x_CreateResolvedSeqMap(const CSeqMap& smap,
                                                   CScope& scope)
{
    // Create destination map (not stored in the indexes,
    // must be deleted by user or user's CRef).
    vector<CSeqMap::CSegment> segments;
    iterate ( CSeqMap, it, smap ) {
        if ( it.GetType() == CSeqMap::eSeqRef ) {
            // Resolve references
            //segments.push_back(CSeqMap::CSegment(it.Get(), scope));
            x_AppendResolved(it.GetRefSeqid(),
                             it.GetRefPosition(),
                             it.GetLength(),
                             it.GetRefMinusStrand(),
                             segments, scope);
        }
        else {
            //segments.push_back(it.x_GetSegment());
        }
    }
    return *new CSeqMap(segments);
}
#endif
#if 0
void CDataSource::x_LocToSeqMap(const CSeq_loc& loc,
                                TSeqPos& pos,
                                CSeqMap& seqmap)
{
    switch ( loc.Which() ) {
    case CSeq_loc::e_not_set:
    case CSeq_loc::e_Null:
    case CSeq_loc::e_Empty:
        {
            // Add gap
            seqmap.Add(CSeqMap::eSeqGap, pos, 0); //???
            return;
        }
    case CSeq_loc::e_Whole:
        {
            // Reference to the whole sequence - do not check its
            // length, use 0 instead.
            CSeqMap::CSegmentInfo* seg = new CSeqMap::CSegmentInfo(
                CSeqMap::eSeqRef, pos, 0, false);
            seg->m_RefPos = 0;
            seg->m_RefSeq = GetSeq_id_Mapper().GetHandle(loc.GetWhole());
            seqmap.Add(*seg);
            return;
        }
    case CSeq_loc::e_Int:
        {
            bool minus_strand = loc.GetInt().IsSetStrand()  &&
                loc.GetInt().GetStrand() == eNa_strand_minus; //???
            CSeqMap::CSegmentInfo* seg = new CSeqMap::CSegmentInfo(
                CSeqMap::eSeqRef, pos, loc.GetInt().GetLength(),
                minus_strand);
            seg->m_RefSeq =
                GetSeq_id_Mapper().GetHandle(loc.GetInt().GetId());
            seg->m_RefPos = min(loc.GetInt().GetFrom(), loc.GetInt().GetTo());
            seg->m_Length = loc.GetInt().GetLength();
            seqmap.Add(*seg);
            pos += loc.GetInt().GetLength();
            return;
        }
    case CSeq_loc::e_Pnt:
        {
            bool minus_strand = loc.GetPnt().IsSetStrand()  &&
                loc.GetPnt().GetStrand() == eNa_strand_minus; //???
            CSeqMap::CSegmentInfo* seg = new CSeqMap::CSegmentInfo(
                CSeqMap::eSeqRef, pos, 1, minus_strand);
            seg->m_RefSeq =
                GetSeq_id_Mapper().GetHandle(loc.GetPnt().GetId());
            seg->m_RefPos = loc.GetPnt().GetPoint();
            seg->m_Length = 1;
            seqmap.Add(*seg);
            pos++;
            return;
        }
    case CSeq_loc::e_Packed_int:
        {
            iterate ( CPacked_seqint::Tdata, ii, loc.GetPacked_int().Get() ) {
                bool minus_strand = (*ii)->IsSetStrand()  &&
                    (*ii)->GetStrand() == eNa_strand_minus; //???
                CSeqMap::CSegmentInfo* seg = new CSeqMap::CSegmentInfo(
                    CSeqMap::eSeqRef, pos, (*ii)->GetLength(), minus_strand);
                seg->m_RefSeq =
                    GetSeq_id_Mapper().GetHandle((*ii)->GetId());
                seg->m_RefPos = min((*ii)->GetFrom(), (*ii)->GetTo());
                seg->m_Length = (*ii)->GetLength();
                seqmap.Add(*seg);
                pos += (*ii)->GetLength();
            }
            return;
        }
    case CSeq_loc::e_Packed_pnt:
        {
            bool minus_strand = loc.GetPacked_pnt().IsSetStrand()  &&
                loc.GetPacked_pnt().GetStrand() == eNa_strand_minus; //???
            iterate ( CPacked_seqpnt::TPoints, pi,
                loc.GetPacked_pnt().GetPoints() ) {
                CSeqMap::CSegmentInfo* seg = new CSeqMap::CSegmentInfo(
                    CSeqMap::eSeqRef, pos, 1, minus_strand);
                seg->m_RefSeq =
                    GetSeq_id_Mapper().GetHandle(loc.GetPacked_pnt().GetId());
                seg->m_RefPos = *pi;
                seg->m_Length = 1;
                seqmap.Add(*seg);
                pos++;
            }
            return;
        }
    case CSeq_loc::e_Mix:
        {
            iterate ( CSeq_loc_mix::Tdata, li, loc.GetMix().Get() ) {
                x_LocToSeqMap(**li, pos, seqmap);
            }
            return;
        }
    case CSeq_loc::e_Equiv:
        {
            iterate ( CSeq_loc_equiv::Tdata, li, loc.GetEquiv().Get() ) {
                //### Is this type allowed here?
                x_LocToSeqMap(**li, pos, seqmap);
            }
            return;
        }
    case CSeq_loc::e_Bond:
        {
            THROW1_TRACE(runtime_error,
                "CDataSource::x_LocToSeqMap() -- e_Bond is not allowed as a reference type");
        }
    case CSeq_loc::e_Feat:
        {
            THROW1_TRACE(runtime_error,
                "CDataSource::x_LocToSeqMap() -- e_Feat is not allowed as a reference type");
        }
    }
}

void CDataSource::x_DataToSeqMap(const CSeq_data& data,
                                 TSeqPos& pos, TSeqPos len,
                                 CSeqMap& seqmap)
{
    //### Search for gaps in the data
    CSeqMap::CSegmentInfo* seg = new CSeqMap::CSegmentInfo(
        CSeqMap::eSeqData, pos, len, false);
    // Assume starting position on the referenced sequence is 0
    seg->m_RefData = &data;
    seg->m_RefPos = 0;
    seg->m_Length = len;
    seqmap.Add(*seg);
    pos += len;
}
#endif
#if 0
inline
void CDataSource::x_AppendRef(const CSeq_id_Handle& ref,
                              TSeqPos from,
                              TSeqPos length,
                              bool minus_strand,
                              vector<CSeqMap::CSegment>& segments)
{
    segments.push_back(CSeqMap::CSegment(ref, from, length, minus_strand));
}


inline
void CDataSource::x_AppendRef(const CSeq_id_Handle& ref,
                              TSeqPos from,
                              TSeqPos length,
                              ENa_strand strand,
                              vector<CSeqMap::CSegment>& segments)
{
    segments.push_back(CSeqMap::CSegment(ref, from, length, strand));
}


inline
void CDataSource::x_AppendRef(const CSeq_id_Handle& ref,
                              const CSeq_interval& interval,
                              vector<CSeqMap::CSegment>& segments)
{
    x_AppendRef(ref,
                interval.GetFrom(),
                interval.GetLength(),
                interval.GetStrand(),
                segments);
}


inline
void CDataSource::x_AppendRef(const CSeq_interval& interval,
                              vector<CSeqMap::CSegment>& segments)
{
    x_AppendRef(GetSeq_id_Mapper().GetHandle(interval.GetId()), interval, segments);
}


void CDataSource::x_AppendLoc(const CSeq_loc& loc,
                              vector<CSeqMap::CSegment>& segments)
{
    switch ( loc.Which() ) {
    case CSeq_loc::e_not_set:
    case CSeq_loc::e_Null:
    case CSeq_loc::e_Empty:
        // Add gap
        segments.push_back(CSeqMap::CSegment(CSeqMap::eSeqGap, 0)); //???
        break;
    case CSeq_loc::e_Whole:
    {
        // Reference to the whole sequence - do not check its
        // length, use 0 instead.
        const CSeq_id& ref = loc.GetWhole();
        segments.push_back(CSeqMap::CSegment(GetSeq_id_Mapper().GetHandle(ref)));
        break;
    }
    case CSeq_loc::e_Int:
        x_AppendRef(loc.GetInt(), segments);
        break;
    case CSeq_loc::e_Pnt:
    {
        const CSeq_point& point = loc.GetPnt();
        x_AppendRef(GetSeq_id_Mapper().GetHandle(point.GetId()),
                    point.GetPoint(),
                    1,
                    point.GetStrand(),
                    segments);
        break;
    }
    case CSeq_loc::e_Packed_int:
    {
        const CPacked_seqint::Tdata& data = loc.GetPacked_int().Get();
        iterate ( CPacked_seqint::Tdata, it, data ) {
            x_AppendRef(**it, segments);
        }
        break;
    }
    case CSeq_loc::e_Packed_pnt:
    {
        const CPacked_seqpnt& points = loc.GetPacked_pnt();
        CSeq_id_Handle seqRef = GetSeq_id_Mapper().GetHandle(points.GetId());
        ENa_strand strand = points.GetStrand();
        const CPacked_seqpnt::TPoints& data = points.GetPoints();
        iterate ( CPacked_seqpnt::TPoints, it, data ) {
            x_AppendRef(seqRef, *it, 1, strand, segments);
        }
        break;
    }
    case CSeq_loc::e_Mix:
    {
        const CSeq_loc_mix::Tdata& data = loc.GetMix().Get();
        iterate ( CSeq_loc_mix::Tdata, it, data ) {
            x_AppendLoc(**it, segments);
        }
        break;
    }
    case CSeq_loc::e_Equiv:
    {
        const CSeq_loc_equiv::Tdata& data = loc.GetEquiv().Get();
        iterate ( CSeq_loc_equiv::Tdata, it, data ) {
            //### Is this type allowed here?
            x_AppendLoc(**it, segments);
        }
        break;
    }
    case CSeq_loc::e_Bond:
        THROW1_TRACE(runtime_error,
                     "CDataSource::x_AppendLoc() -- "
                     "e_Bond is not allowed as a reference type");
    case CSeq_loc::e_Feat:
        THROW1_TRACE(runtime_error,
                     "CDataSource::x_AppendLoc() -- "
                     "e_Feat is not allowed as a reference type");
    }
}
#endif

void CDataSource::x_MapFeature(const CSeq_feat& feat,
                               CSeq_annot_Info& annot,
                               CTSE_Info& tse)
{
    // Lock indexes
    CMutexGuard guard(m_DataSource_Mtx);    

    // Create annotation object. It will split feature location
    // to a handle-ranges map.
    CRef<CAnnotObject_Info> aobj(new CAnnotObject_Info(annot, feat));
    m_AnnotObjectMap.insert(TAnnotObjectMap::value_type(&feat, aobj));
    // Iterate handles
    iterate ( CHandleRangeMap::TLocMap,
              mapit, aobj->GetRangeMap().GetMap() ) {
        m_TSE_ref[mapit->first].insert(CRef<CTSE_Info>(&tse));

        CTSE_Info::TAnnotSelectorMap& selMapTotal =
            tse.m_AnnotMap_ByTotal[mapit->first];
        CTSE_Info::TAnnotSelectorMap& selMapInt =
            tse.m_AnnotMap_ByInt[mapit->first];
        CTSE_Info::TRangeMap::value_type
            ins(mapit->second.GetOverlappingRange(), aobj);
        SAnnotSelector ss[2];
        ss[0] = SAnnotSelector(CSeq_annot::C_Data::e_Ftable,
                               feat.GetData().Which());
        ss[1] = SAnnotSelector(CSeq_annot::C_Data::e_Ftable);
        for ( int i = 0; i < 2; ++i ) {
            selMapTotal[ss[i]].insert(ins);
        }
        iterate (CHandleRange::TRanges, rgit, mapit->second) {
            CTSE_Info::TRangeMap::value_type ins(rgit->first, aobj);
            for ( int i = 0; i < 2; ++i ) {
                selMapInt[ss[i]].insert(ins);
            }
        }
    }
    if ( aobj->GetProductMap() ) {
        iterate ( CHandleRangeMap::TLocMap,
                  mapit, aobj->GetProductMap()->GetMap() ) {
            m_TSE_ref[mapit->first].insert(CRef<CTSE_Info>(&tse));
            
            CTSE_Info::TAnnotSelectorMap& selMapTotal =
                tse.m_AnnotMap_ByTotal[mapit->first];
            CTSE_Info::TAnnotSelectorMap& selMapInt =
                tse.m_AnnotMap_ByInt[mapit->first];
            CTSE_Info::TRangeMap::value_type
                ins(mapit->second.GetOverlappingRange(), aobj);

            SAnnotSelector ss[2];
            ss[0] = SAnnotSelector(CSeq_annot::C_Data::e_Ftable,
                                   feat.GetData().Which(),
                                   true);
            ss[1] = SAnnotSelector(CSeq_annot::C_Data::e_Ftable,
                                   CSeqFeatData::e_not_set,
                                   true);
            for ( int i = 0; i < 2; ++i ) {
                selMapTotal[ss[i]].insert(ins);
            }
            iterate (CHandleRange::TRanges, rgit, mapit->second) {
                CTSE_Info::TRangeMap::value_type ins(rgit->first, aobj);
                for ( int i = 0; i < 2; ++i ) {
                    selMapInt[ss[i]].insert(ins);
                }
            }
        }
    }
}


void CDataSource::x_MapAlign(const CSeq_align& align,
                             CSeq_annot_Info& annot,
                             CTSE_Info& tse)
{
    // Lock indexes
    CMutexGuard guard(m_DataSource_Mtx);    

    // Create annotation object. It will process the align locations
    CRef<CAnnotObject_Info> aobj(new CAnnotObject_Info(annot, align));
    m_AnnotObjectMap.insert(TAnnotObjectMap::value_type(&align, aobj));
    // Iterate handles
    iterate ( CHandleRangeMap::TLocMap,
              mapit, aobj->GetRangeMap().GetMap() ) {
        m_TSE_ref[mapit->first].insert(CRef<CTSE_Info>(&tse));

        CTSE_Info::TAnnotSelectorMap& selMapTotal =
            tse.m_AnnotMap_ByTotal[mapit->first];
        CTSE_Info::TAnnotSelectorMap& selMapInt =
            tse.m_AnnotMap_ByInt[mapit->first];
        CTSE_Info::TRangeMap::value_type ins
            (mapit->second.GetOverlappingRange(), CRef<CAnnotObject_Info>(aobj));

        selMapTotal[SAnnotSelector(CSeq_annot::C_Data::e_Align)].insert(ins);
        iterate (CHandleRange::TRanges, rgit, mapit->second) {
            CTSE_Info::TRangeMap::value_type ins
                (rgit->first, CRef<CAnnotObject_Info>(aobj));
            selMapInt[SAnnotSelector(CSeq_annot::C_Data::e_Align)].
                insert(ins);
        }
    }
}


void CDataSource::x_MapGraph(const CSeq_graph& graph,
                             CSeq_annot_Info& annot,
                             CTSE_Info& tse)
{
    // Lock indexes
    CMutexGuard guard(m_DataSource_Mtx);    

    // Create annotation object. It will split graph location
    // to a handle-ranges map.
    CRef<CAnnotObject_Info> aobj(new CAnnotObject_Info(annot, graph));
    m_AnnotObjectMap.insert(TAnnotObjectMap::value_type(&graph, aobj));
    // Iterate handles
    iterate ( CHandleRangeMap::TLocMap,
        mapit, aobj->GetRangeMap().GetMap() ) {
        m_TSE_ref[mapit->first].insert(CRef<CTSE_Info>(&tse));

        CTSE_Info::TAnnotSelectorMap& selMapTotal =
            tse.m_AnnotMap_ByTotal[mapit->first];
        CTSE_Info::TAnnotSelectorMap& selMapInt =
            tse.m_AnnotMap_ByInt[mapit->first];
        CTSE_Info::TRangeMap::value_type ins
            (mapit->second.GetOverlappingRange(), CRef<CAnnotObject_Info>(aobj));

        selMapTotal[SAnnotSelector(CSeq_annot::C_Data::e_Graph)].insert(ins);
        iterate (CHandleRange::TRanges, rgit, mapit->second) {
            CTSE_Info::TRangeMap::value_type ins
                (rgit->first, CRef<CAnnotObject_Info>(aobj));
            selMapInt[SAnnotSelector(CSeq_annot::C_Data::e_Graph)].
                insert(ins);
        }
    }
}


void CDataSource::PopulateTSESet(CHandleRangeMap& loc,
                                 TTSESet& tse_set,
                                 CSeq_annot::C_Data::E_Choice sel,
                                 CScope& scope)
{
/*
Iterate each id from "loc". Find all TSEs with references to the id.
Put TSEs, containing the sequence itself to "selected_with_seq" and
TSEs without the sequence to "selected_with_ref". In "selected_with_seq"
try to find TSE, referenced by the history (if more than one found, report
error), if not found, search for a live TSE (if more than one found,
report error), if not found, check if there is only one dead TSE in the set.
Otherwise report error.
From "selected_with_ref" select only live TSEs and dead TSEs, referenced by
the history.
The resulting set will contain 0 or 1 TSE with the sequence, all live TSEs
without the sequence but with references to the id and all dead TSEs
(with references and without the sequence), referenced by the history.
*/
    //### Lock all TSEs found, unlock all filtered out in the end.
    TTSESet loaded_tse_set;
    TTSESet tmp_tse_set;
    if ( m_Loader ) {
        // Send request to the loader
        switch ( sel ) {
        case CSeq_annot::C_Data::e_Ftable:
            m_Loader->GetRecords(loc, CDataLoader::eFeatures, &loaded_tse_set);
            break;
        case CSeq_annot::C_Data::e_Align:
            //### Need special flag for alignments
            m_Loader->GetRecords(loc, CDataLoader::eAll);
            break;
        case CSeq_annot::C_Data::e_Graph:
            m_Loader->GetRecords(loc, CDataLoader::eGraph);
            break;
        }
    }
    // Index all annotations if not indexed yet
    if ( !m_IndexedAnnot ) {
        non_const_iterate (TEntries, tse_it, m_Entries) {
            //### Lock TSE so that another thread can not index it too
            CTSE_Guard guard(*tse_it->second);
            if ( !tse_it->second->IsIndexed() )
                x_AddToAnnotMap(*tse_it->second->m_TSE, tse_it->second);
        }
        m_IndexedAnnot = true;
    }
    x_ResolveLocationHandles(loc, scope.m_History);
    TTSESet non_history;
    CMutexGuard guard(m_DataSource_Mtx);    
    iterate(CHandleRangeMap::TLocMap, hit, loc.GetMap()) {
        // Search for each seq-id handle from loc in the indexes
        TTSEMap::const_iterator tse_it = m_TSE_ref.find(hit->first);
        if (tse_it == m_TSE_ref.end())
            continue; // No this seq-id in the TSE
        _ASSERT(tse_it->second.size() > 0);
        TTSESet selected_with_ref;
        TTSESet selected_with_seq;
        iterate(TTSESet, tse, tse_it->second) {
            if ((*tse)->m_BioseqMap.find(hit->first) !=
                (*tse)->m_BioseqMap.end()) {
                selected_with_seq.insert(*tse); // with sequence
            }
            else
                selected_with_ref.insert(*tse); // with reference
        }

        CRef<CTSE_Info> unique_from_history;
        CRef<CTSE_Info> unique_live;
        iterate (TTSESet, with_seq, selected_with_seq) {
            if (scope.m_History.find(*with_seq) != scope.m_History.end()) {
                if ( unique_from_history ) {
                    THROW1_TRACE(runtime_error,
                        "CDataSource::PopulateTSESet() -- "
                        "Ambiguous request: multiple history matches");
                }
                unique_from_history = *with_seq;
            }
            else if ( !unique_from_history ) {
                if ((*with_seq)->m_Dead)
                    continue;
                if ( unique_live ) {
                    THROW1_TRACE(runtime_error,
                        "CDataSource::PopulateTSESet() -- "
                        "Ambiguous request: multiple live TSEs");
                }
                unique_live = *with_seq;
            }
        }
        if ( unique_from_history ) {
            tmp_tse_set.insert(unique_from_history);
        }
        else if ( unique_live ) {
            non_history.insert(unique_live);
        }
        else if (selected_with_seq.size() == 1) {
            non_history.insert(*selected_with_seq.begin());
        }
        else if (selected_with_seq.size() > 1) {
            //### Try to resolve the conflict with the help of loader
            THROW1_TRACE(runtime_error,
                "CDataSource::PopulateTSESet() -- "
                "Ambigous request: multiple TSEs found");
        }
        iterate(TTSESet, tse, selected_with_ref) {
            if ( !(*tse)->m_Dead  ||  scope.m_History.find(*tse) != scope.m_History.end()) {
                // Select only TSEs present in the history and live TSEs
                // Different sets for in-history and non-history TSEs for
                // the future filtering.
                if (scope.m_History.find(*tse) != scope.m_History.end()) {
                    tmp_tse_set.insert(*tse); // in-history TSE
                }
                else {
                    non_history.insert(*tse); // non-history TSE
                }
            }
        }
    }
    // Filter out TSEs not in the history yet and conflicting with any
    // history TSE. The conflict may be caused by any seq-id, even not
    // mentioned in the searched location.
    bool conflict;
    iterate (TTSESet, tse_it, non_history) {
        conflict = false;
        // Check each seq-id from the current TSE
        iterate (CTSE_Info::TBioseqMap, seq_it, (*tse_it)->m_BioseqMap) {
            iterate (CScope::TRequestHistory, hist_it, scope.m_History) {
                conflict = (*hist_it)->m_BioseqMap.find(seq_it->first) !=
                    (*hist_it)->m_BioseqMap.end();
                if ( conflict ) break;
            }
            if ( conflict ) break;
        }
        if ( !conflict ) {
            // No conflicts found -- add the TSE to the resulting set
            tmp_tse_set.insert(*tse_it);
        }
    }
    // Unlock unused TSEs
    iterate (TTSESet, lit, loaded_tse_set) {
        if (tmp_tse_set.find(*lit) == tmp_tse_set.end()) {
            (*lit)->UnlockCounter();
        }
    }
    // Lock used TSEs loaded before this call (the scope does not know
    // which TSE should be unlocked, so it unlocks all of them).
    iterate (TTSESet, lit, tmp_tse_set) {
        TTSESet::iterator loaded_it = loaded_tse_set.find(*lit);
        if (loaded_it == loaded_tse_set.end()) {
            (*lit)->LockCounter();
        }
        tse_set.insert(*lit);
    }
}


void CDataSource::x_ResolveLocationHandles(CHandleRangeMap& loc,
                                           const CScope::TRequestHistory& history) const
{
    CHandleRangeMap tmp;
    iterate ( CHandleRangeMap::TLocMap, it, loc.GetMap() ) {
        CSeq_id_Handle idh = it->first;
        //### The TSE returns locked, unlock it
        CTSE_Info* tse_info = x_FindBestTSE(it->first, history);
        if ( !tse_info ) {
            // Try to find the best matching id (not exactly equal)
            const CSeq_id& id = GetSeq_id_Mapper().GetSeq_id(idh);
            TSeq_id_HandleSet hset;
            GetSeq_id_Mapper().GetMatchingHandles(id, hset);
            iterate(TSeq_id_HandleSet, hit, hset) {
                if ( tse_info  &&  idh.IsBetter(*hit) )
                    continue;
                CTSE_Info* tmp_tse = x_FindBestTSE(*hit, history);
                if ( tmp_tse ) {
                    if (tse_info != 0) {
                        tse_info->UnlockCounter();
                    }
                    tse_info = tmp_tse;
                    idh = *hit;
                }
            }
        }
        // At this point the tse_info (if not null) should be locked
        if (tse_info != 0) {
            TBioseqMap::const_iterator info =
                tse_info->m_BioseqMap.find(idh);
            if (info == tse_info->m_BioseqMap.end()) {
                // Just copy the existing range map
                tmp.AddRanges(it->first, it->second);
            }
            else {
                // Create range list for each synonym of a seq_id
                const CBioseq_Info::TSynonyms& syn = info->second->m_Synonyms;
                iterate ( CBioseq_Info::TSynonyms, syn_it, syn ) {
                    tmp.AddRanges(*syn_it, it->second);
                }
            }
            tse_info->UnlockCounter();
        }
        else {
            // Just copy the existing range map
            tmp.AddRanges(it->first, it->second);
        }
    }
    loc = tmp;
}


bool CDataSource::DropTSE(const CSeq_entry& tse)
{
    // Allow to drop top-level seq-entries only
    _ASSERT(tse.GetParentEntry() == 0);

    CRef<CSeq_entry> ref(const_cast<CSeq_entry*>(&tse));

    // Lock indexes
    CMutexGuard guard(m_DataSource_Mtx);    

    TEntries::iterator found = m_Entries.find(ref);
    if (found == m_Entries.end())
        return false;
    if ( found->second->CounterLocked() )
        return false; // Not really dropped, although found
    if ( m_Loader )
        m_Loader->DropTSE(found->first);
    x_DropEntry(const_cast<CSeq_entry&>(*found->first));
    return true;
}


void CDataSource::x_DropEntry(CSeq_entry& entry)
{
    CSeq_entry* tse = &entry;
    while ( tse->GetParentEntry() ) {
        tse = tse->GetParentEntry();
    }
    if ( entry.IsSeq() ) {
        CBioseq& seq = entry.SetSeq();
        CSeq_id_Handle key;
        iterate ( CBioseq::TId, id, seq.GetId() ) {
            // Find TSE and bioseq positions
            key = GetSeq_id_Mapper().GetHandle(**id);
            TTSEMap::iterator tse_set = m_TSE_seq.find(key);
            _ASSERT(tse_set != m_TSE_seq.end());
            //### No need to lock the TSE since the whole DS is locked by DropTSE()
            //### CMutexGuard guard(sm_TSESet_MP.GetMutex(&tse_set->second));
            TTSESet::iterator tse_it = tse_set->second.begin();
            for ( ; tse_it != tse_set->second.end(); ++tse_it) {
                if (const_cast<CSeq_entry*>((*tse_it)->m_TSE.GetPointer()) ==
                    const_cast<CSeq_entry*>(tse))
                    break;
            }
            _ASSERT(tse_it != tse_set->second.end());
            TBioseqMap::iterator found =
                const_cast<TBioseqMap&>((*tse_it)->m_BioseqMap).find(key);
            _ASSERT( found !=
                     const_cast<TBioseqMap&>((*tse_it)->m_BioseqMap).end() );
            const_cast<TBioseqMap&>((*tse_it)->m_BioseqMap).erase(found);
            // Remove TSE index for the bioseq (the TSE may still be
            // in other id indexes).
            tse_set->second.erase(tse_it);
            if (tse_set->second.size() == 0) {
                m_TSE_seq.erase(tse_set);
            }
        }
        TSeqMaps::iterator map_it =
            m_SeqMaps.find(CBioseq_Handle::TBioseqCore(&seq));
        if (map_it != m_SeqMaps.end()) {
            m_SeqMaps.erase(map_it);
        }
        x_DropAnnotMap(entry);
    }
    else {
        iterate ( CBioseq_set::TSeq_set, it, entry.GetSet().GetSeq_set() ) {
            x_DropEntry(const_cast<CSeq_entry&>(**it));
        }
        x_DropAnnotMap(entry);
    }
    m_Entries.erase(CRef<CSeq_entry>(&entry));
}


void CDataSource::x_DropAnnotMapRecursive(const CSeq_entry& entry)
{
    if ( entry.IsSet() ) {
        iterate ( CBioseq_set::TSeq_set, it, entry.GetSet().GetSeq_set() ) {
            x_DropAnnotMapRecursive(**it);
        }
    }
    x_DropAnnotMap(entry);
}


void CDataSource::x_DropAnnotMap(const CSeq_entry& entry)
{
    const CBioseq::TAnnot* annot_list = 0;
    if ( entry.IsSeq() ) {
        if ( entry.GetSeq().IsSetAnnot() ) {
            annot_list = &entry.GetSeq().GetAnnot();
        }
    }
    else {
        if ( entry.GetSet().IsSetAnnot() ) {
            annot_list = &entry.GetSet().GetAnnot();
        }
        // Do not iterate sub-trees -- they will be iterated by
        // the calling function.
    }
    if ( !annot_list ) {
        return;
    }
    iterate ( CBioseq::TAnnot, ai, *annot_list ) {
        switch ( (*ai)->GetData().Which() ) {
        case CSeq_annot::C_Data::e_Ftable:
            {
                iterate ( CSeq_annot::C_Data::TFtable, it,
                    (*ai)->GetData().GetFtable() ) {
                    x_DropFeature(**it, **ai, &entry);
                }
                break;
            }
        case CSeq_annot::C_Data::e_Align:
            {
                iterate ( CSeq_annot::C_Data::TAlign, it,
                    (*ai)->GetData().GetAlign() ) {
                    x_DropAlign(**it, **ai, &entry);
                }
                break;
            }
        case CSeq_annot::C_Data::e_Graph:
            {
                iterate ( CSeq_annot::C_Data::TGraph, it,
                    (*ai)->GetData().GetGraph() ) {
                    x_DropGraph(**it, **ai, &entry);
                }
                break;
            }
        }
    }
}


void CDataSource::x_DropFeature(const CSeq_feat& feat,
                                const CSeq_annot& annot,
                                const CSeq_entry* entry)
{
    // Get annot object to iterate all seq-id handles
    TAnnotObjectMap::iterator found = m_AnnotObjectMap.find(&feat);
    if (found == m_AnnotObjectMap.end())
        return;
    CRef<CAnnotObject_Info> aobj(found->second);
    // Iterate id handles
    iterate ( CHandleRangeMap::TLocMap,
              mapit, aobj->GetRangeMap().GetMap() ) {
        // Find TSEs containing references to the id
        // The whole DS should be locked by DropTSE()
        TTSEMap::iterator tse_set = m_TSE_ref.find(mapit->first);
        if (tse_set == m_TSE_ref.end())
            continue; // The referenced ID is not currently loaded

        SAnnotSelector ss[2];
        ss[0] = SAnnotSelector(CSeq_annot::C_Data::e_Ftable,
                               feat.GetData().Which());
        ss[1] = SAnnotSelector(CSeq_annot::C_Data::e_Ftable);
        CRange<TSeqPos> range = mapit->second.GetOverlappingRange();

        // Find the TSE containing the feature
        for ( TTSESet::iterator tse_info = tse_set->second.begin();
              tse_info != tse_set->second.end(); ++tse_info) {
            CTSE_Info::TAnnotMap& amt =
                const_cast<TAnnotMap&>((*tse_info)->m_AnnotMap_ByTotal);
            CTSE_Info::TAnnotMap& ami =
                const_cast<TAnnotMap&>((*tse_info)->m_AnnotMap_ByInt);
            TAnnotMap::iterator annot_itt = amt.find(mapit->first);
            TAnnotMap::iterator annot_iti = ami.find(mapit->first);
            if ( annot_itt != amt.end() ) {
                for ( int i = 0; i < 2; ++i ) {
                    CTSE_Info::TAnnotSelectorMap::iterator sel_it =
                        annot_itt->second.find(ss[i]);
                    if ( sel_it == annot_itt->second.end() )
                        continue;
                    TRangeMap::iterator rg = sel_it->second.begin(range);
                    for ( ; rg != sel_it->second.end(); ++rg) {
                        if (rg->second->IsFeat()  &&
                            &(rg->second->GetFeat()) == &feat)
                            break;
                    }
                    if (rg == sel_it->second.end())
                        continue;
                    // Delete the feature from all indexes
                    sel_it->second.erase(rg);
                    if (sel_it->second.empty()) {
                        annot_itt->second.erase(sel_it);
                    }
                }
            }
            if ( annot_iti != ami.end() ) {
                for ( int i = 0; i < 2; ++i ) {
                    CTSE_Info::TAnnotSelectorMap::iterator sel_it =
                        annot_iti->second.find(ss[i]);
                    if ( sel_it == annot_iti->second.end() )
                        continue;
                    TRangeMap::iterator rg = sel_it->second.begin(range);
                    for ( ; rg != sel_it->second.end(); ++rg) {
                        if (rg->second->IsFeat()  &&
                            &(rg->second->GetFeat()) == &feat)
                            break;
                    }
                    if (rg == sel_it->second.end())
                        continue;
                    // Delete the feature from all indexes
                    sel_it->second.erase(rg);
                    if (sel_it->second.empty()) {
                        annot_iti->second.erase(sel_it);
                    }
                }
            }
            if (annot_itt->second.empty()  &&  annot_iti->second.empty()) {
                amt.erase(annot_itt);
                ami.erase(annot_iti);
                tse_set->second.erase(tse_info);
            }
            if (tse_set->second.empty()) {
                m_TSE_ref.erase(tse_set);
            }
            break;
        }
    }
    if ( aobj->GetProductMap() ) {
        // Repeat it for product if any
        iterate ( CHandleRangeMap::TLocMap,
                  mapit, aobj->GetProductMap()->GetMap() ) {
            // Find TSEs containing references to the id
            // The whole DS should be locked by DropTSE()
            TTSEMap::iterator tse_set = m_TSE_ref.find(mapit->first);
            if (tse_set == m_TSE_ref.end())
                continue; // The referenced ID is not currently loaded

            SAnnotSelector ss[2];
            ss[0] = SAnnotSelector(CSeq_annot::C_Data::e_Ftable,
                                   feat.GetData().Which(),
                                   true);
            ss[1] = SAnnotSelector(CSeq_annot::C_Data::e_Ftable,
                                   CSeqFeatData::e_not_set,
                                   true);
            CRange<TSeqPos> range = mapit->second.GetOverlappingRange();
            
            // Find the TSE containing the feature
            for ( TTSESet::iterator tse_info = tse_set->second.begin();
                  tse_info != tse_set->second.end(); ++tse_info) {
                CTSE_Info::TAnnotMap& amt =
                    const_cast<TAnnotMap&>((*tse_info)->m_AnnotMap_ByTotal);
                CTSE_Info::TAnnotMap& ami =
                    const_cast<TAnnotMap&>((*tse_info)->m_AnnotMap_ByInt);
                TAnnotMap::iterator annot_itt = amt.find(mapit->first);
                TAnnotMap::iterator annot_iti = ami.find(mapit->first);
                if (annot_itt != amt.end()) {
                    for ( int i = 0; i < 2; ++i ) {
                        CTSE_Info::TAnnotSelectorMap::iterator sel_it =
                            annot_itt->second.find(ss[i]);
                        if ( sel_it == annot_itt->second.end() )
                            continue;
                        TRangeMap::iterator rg = sel_it->second.begin(range);
                        for ( ; rg != sel_it->second.end(); ++rg) {
                            if (rg->second->IsFeat()  &&
                                &(rg->second->GetFeat()) == &feat)
                                break;
                        }
                        if (rg == sel_it->second.end())
                            continue;
                        // Delete the feature from all indexes
                        sel_it->second.erase(rg);
                        if (sel_it->second.empty()) {
                            annot_itt->second.erase(sel_it);
                        }
                    }
                }
                if (annot_iti != ami.end()) {
                    for ( int i = 0; i < 2; ++i ) {
                        CTSE_Info::TAnnotSelectorMap::iterator sel_it =
                            annot_iti->second.find(ss[i]);
                        if ( sel_it == annot_iti->second.end() )
                            continue;
                        TRangeMap::iterator rg = sel_it->second.begin(range);
                        for ( ; rg != sel_it->second.end(); ++rg) {
                            if (rg->second->IsFeat()  &&
                                &(rg->second->GetFeat()) == &feat)
                                break;
                        }
                        if (rg == sel_it->second.end())
                            continue;
                        // Delete the feature from all indexes
                        sel_it->second.erase(rg);
                        if (sel_it->second.empty()) {
                            annot_iti->second.erase(sel_it);
                        }
                    }
                }
                if (annot_itt->second.empty()  &&  annot_iti->second.empty()) {
                    amt.erase(annot_itt);
                    ami.erase(annot_iti);
                    tse_set->second.erase(tse_info);
                }
                if (tse_set->second.empty()) {
                    m_TSE_ref.erase(tse_set);
                }
                break;
            }
        }
    }
    m_AnnotObjectMap.erase(found);
}


void CDataSource::x_DropAlign(const CSeq_align& align,
                              const CSeq_annot& annot,
                              const CSeq_entry* entry)
{
    // Get annot object to iterate all seq-id handles
    TAnnotObjectMap::iterator found = m_AnnotObjectMap.find(&align);
    if (found == m_AnnotObjectMap.end())
        return;
    CRef<CAnnotObject_Info> aobj(found->second);
    // Iterate handles
    iterate ( CHandleRangeMap::TLocMap,
              mapit, aobj->GetRangeMap().GetMap() ) {
        // Find TSEs containing references to the id
        // The whole DS should be locked by DropTSE()
        TTSEMap::iterator tse_set = m_TSE_ref.find(mapit->first);
        if (tse_set == m_TSE_ref.end())
            continue; // The referenced ID is not currently loaded

        // Find the TSE containing the align
        for ( TTSESet::iterator tse_info = tse_set->second.begin();
              tse_info != tse_set->second.end(); ++tse_info) {
            CTSE_Info::TAnnotMap& amt =
                const_cast<TAnnotMap&>((*tse_info)->m_AnnotMap_ByTotal);
            CTSE_Info::TAnnotMap& ami =
                const_cast<TAnnotMap&>((*tse_info)->m_AnnotMap_ByInt);
            TAnnotMap::iterator annot_itt = amt.find(mapit->first);
            TAnnotMap::iterator annot_iti = ami.find(mapit->first);
            if (annot_itt != amt.end()) {
                CTSE_Info::TAnnotSelectorMap::iterator sel_it =
                    annot_itt->second.find(SAnnotSelector(CSeq_annot::C_Data::e_Align));
                if ( sel_it != annot_itt->second.end() ) {
                    TRangeMap::iterator rg = sel_it->second.begin(
                        mapit->second.GetOverlappingRange());
                    for ( ; rg != sel_it->second.end(); ++rg) {
                        if (rg->second->IsAlign()  &&
                            &(rg->second->GetAlign()) == &align)
                            break;
                    }
                    if (rg == sel_it->second.end())
                        continue;
                    // Delete the align from all indexes
                    sel_it->second.erase(rg);
                    if ( sel_it->second.empty() ) {
                        annot_itt->second.erase(sel_it);
                    }
                }
            }
            if (annot_iti != amt.end()) {
                CTSE_Info::TAnnotSelectorMap::iterator sel_it =
                    annot_iti->second.find(SAnnotSelector(CSeq_annot::C_Data::e_Align));
                if ( sel_it != annot_iti->second.end() ) {
                    TRangeMap::iterator rg = sel_it->second.begin(
                        mapit->second.GetOverlappingRange());
                    for ( ; rg != sel_it->second.end(); ++rg) {
                        if (rg->second->IsAlign()  &&
                            &(rg->second->GetAlign()) == &align)
                            break;
                    }
                    if (rg == sel_it->second.end())
                        continue;
                    // Delete the align from all indexes
                    sel_it->second.erase(rg);
                    if ( sel_it->second.empty() ) {
                        annot_iti->second.erase(sel_it);
                    }
                }
            }
            if (annot_itt->second.empty()  &&  annot_iti->second.empty()) {
                amt.erase(annot_itt);
                ami.erase(annot_iti);
                tse_set->second.erase(tse_info);
            }
            if (tse_set->second.empty()) {
                m_TSE_ref.erase(tse_set);
            }
            break;
        }
    }
    m_AnnotObjectMap.erase(found);
}


void CDataSource::x_DropGraph(const CSeq_graph& graph,
                              const CSeq_annot& annot,
                              const CSeq_entry* entry)
{
    // Get annot object to iterate all seq-id handles
    TAnnotObjectMap::iterator found = m_AnnotObjectMap.find(&graph);
    if (found == m_AnnotObjectMap.end())
        return;
    CRef<CAnnotObject_Info> aobj(found->second);
    // Iterate handles
    iterate ( CHandleRangeMap::TLocMap,
              mapit, aobj->GetRangeMap().GetMap() ) {
        // Find TSEs containing references to the id
        // The whole DS should be locked by DropTSE()
        TTSEMap::iterator tse_set = m_TSE_ref.find(mapit->first);
        if (tse_set == m_TSE_ref.end())
            continue; // The referenced ID is not currently loaded

        // Find the TSE containing the graph
        for ( TTSESet::iterator tse_info = tse_set->second.begin();
              tse_info != tse_set->second.end(); ++tse_info) {
            CTSE_Info::TAnnotMap& amt =
                const_cast<TAnnotMap&>((*tse_info)->m_AnnotMap_ByTotal);
            CTSE_Info::TAnnotMap& ami =
                const_cast<TAnnotMap&>((*tse_info)->m_AnnotMap_ByInt);
            TAnnotMap::iterator annot_itt = amt.find(mapit->first);
            TAnnotMap::iterator annot_iti = ami.find(mapit->first);
            if (annot_itt != amt.end()) {
                CTSE_Info::TAnnotSelectorMap::iterator sel_it =
                    annot_itt->second.find(SAnnotSelector(CSeq_annot::C_Data::e_Graph));
                if ( sel_it != annot_itt->second.end() ) {
                    TRangeMap::iterator rg = sel_it->second.begin(
                        mapit->second.GetOverlappingRange());
                    for ( ; rg != sel_it->second.end(); ++rg) {
                        if (rg->second->IsGraph()  &&
                            &(rg->second->GetGraph()) == &graph)
                            break;
                    }
                    if (rg == sel_it->second.end())
                        continue;
                    // Delete the graph from all indexes
                    sel_it->second.erase(rg);
                    if ( sel_it->second.empty() ) {
                        annot_itt->second.erase(sel_it);
                    }
                }
            }
            if (annot_iti != ami.end()) {
                CTSE_Info::TAnnotSelectorMap::iterator sel_it =
                    annot_iti->second.find(SAnnotSelector(CSeq_annot::C_Data::e_Graph));
                if ( sel_it != annot_iti->second.end() ) {
                    TRangeMap::iterator rg = sel_it->second.begin(
                        mapit->second.GetOverlappingRange());
                    for ( ; rg != sel_it->second.end(); ++rg) {
                        if (rg->second->IsGraph()  &&
                            &(rg->second->GetGraph()) == &graph)
                            break;
                    }
                    if (rg == sel_it->second.end())
                        continue;
                    // Delete the graph from all indexes
                    sel_it->second.erase(rg);
                    if ( sel_it->second.empty() ) {
                        annot_iti->second.erase(sel_it);
                    }
                }
            }
            if (annot_itt->second.empty()  &&  annot_iti->second.empty()) {
                amt.erase(annot_itt);
                ami.erase(annot_iti);
                tse_set->second.erase(tse_info);
            }
            if (tse_set->second.empty()) {
                m_TSE_ref.erase(tse_set);
            }
            break;
        }
    }
    m_AnnotObjectMap.erase(found);
}


void CDataSource::x_CleanupUnusedEntries(void)
{
    // Lock indexes
    CMutexGuard guard(m_DataSource_Mtx);    

    bool broken = true;
    while ( broken ) {
        broken = false;
        iterate(TEntries, it, m_Entries) {
            if ( !it->second->CounterLocked() ) {
                //### Lock the entry and check again
                DropTSE(*it->first);
                broken = true;
                break;
            }
        }
    }
}


void CDataSource::x_UpdateTSEStatus(CSeq_entry& tse, bool dead)
{
    TEntries::iterator tse_it = m_Entries.find(CRef<CSeq_entry>(&tse));
    _ASSERT(tse_it != m_Entries.end());
    CTSE_Guard guard(*tse_it->second);
    tse_it->second->m_Dead = dead;
}


CSeqMatch_Info CDataSource::BestResolve(const CSeq_id& id, CScope& scope)
{
    //### Lock all TSEs found, unlock all filtered out in the end.
    TTSESet loaded_tse_set;
    if ( m_Loader ) {
        // Send request to the loader
        //### Need a better interface to request just a set of IDs
        CSeq_id_Handle idh = GetSeq_id_Mapper().GetHandle(id);
        CHandleRangeMap hrm;
        hrm.AddRange(idh, CHandleRange::TRange::GetWhole(), eNa_strand_unknown);
        m_Loader->GetRecords(hrm, CDataLoader::eBioseqCore, &loaded_tse_set);
    }
    CSeqMatch_Info match;
    CSeq_id_Handle idh = GetSeq_id_Mapper().GetHandle(id, true);
    if ( !idh ) {
        return match; // The seq-id is not even mapped yet
    }
    CTSE_Info* tse = x_FindBestTSE(idh, scope.m_History);
    if ( !tse ) {
        // Try to find the best matching id (not exactly equal)
        TSeq_id_HandleSet hset;
        GetSeq_id_Mapper().GetMatchingHandles(id, hset);
        iterate(TSeq_id_HandleSet, hit, hset) {
            if ( tse  &&  idh.IsBetter(*hit) )
                continue;
            CTSE_Info* tmp_tse = x_FindBestTSE(*hit, scope.m_History);
            if ( tmp_tse ) {
                if (tse != 0) {
                    tse->UnlockCounter();
                }
                tse = tmp_tse;
                idh = *hit;
            }
        }
    }
    if (tse != 0) {
        match = CSeqMatch_Info(idh, *tse, *this);
        tse->UnlockCounter();
    }
    bool just_loaded = false;
    iterate (TTSESet, lit, loaded_tse_set) {
        if (lit->GetPointer() != tse) {
            (*lit)->UnlockCounter();
        }
        else {
            just_loaded = true;
        }
    }
    if ( !just_loaded  &&  match ) {
        match.m_TSE->LockCounter(); // will be unlocked by the scope
    }
    return match;
}


string CDataSource::GetName(void) const
{
    if ( m_Loader )
        return m_Loader->GetName();
    else
        return kEmptyStr;
}

#if 0
void CDataSource::x_ResolveMapSegment(const CSeq_id_Handle& rh,
                                      TSeqPos start, TSeqPos len,
                                      CSeqMap& dmap, TSeqPos& dpos,
                                      TSeqPos dstop,
                                      CScope& scope)
{
    CBioseq_Handle rbsh = scope.GetBioseqHandle(
        GetSeq_id_Mapper().GetSeq_id(rh));
    if ( !rbsh ) {
        THROW1_TRACE(runtime_error,
            "CDataSource::x_ResolveMapSegment() -- "
            "Can not resolve reference.");
    }
    // This tricky way of getting the seq-map is used to obtain
    // the non-const reference.

    CSeqMap& rmap = rbsh.x_GetDataSource().x_GetSeqMap(rbsh);
    // Resolve the reference map up to the end of the referenced region
    rmap.x_Resolve(start + len, scope);
    size_t rseg = rmap.x_FindSegment(start, &scope);

    auto_ptr<CMutexGuard> guard(new CMutexGuard(m_DataSource_Mtx));    
    // Get enough segments to cover the "start+length" range on the
    // destination sequence
    while (dpos < dstop  &&  len > 0) {
        // Get and adjust the segment start
        TSeqPos rstart = rmap[rseg].m_Position;
        TSeqPos rlength = rmap[rseg].m_Length;
        if (rstart < start) {
            rlength -= start - rstart;
            rstart = start;
        }
        if (rstart + rlength > start + len)
            rlength = start + len - rstart;
        switch (rmap[rseg].m_SegType) {
        case CSeqMap::eSeqData:
            {
                // Put the final reference
                CSeqMap::CSegmentInfo* new_seg = new CSeqMap::CSegmentInfo(
                    CSeqMap::eSeqRef, dpos, rlength, rmap[rseg].m_MinusStrand);
                new_seg->m_RefPos = rstart;
                new_seg->m_RefSeq = rh; //rmap[rseg].m_RefSeq;
                dmap.Add(*new_seg);
                dpos += rlength;
                break;
            }
        case CSeqMap::eSeqGap:
        case CSeqMap::eSeqEnd:
            {
                // Copy gaps and literals
                dmap.Add(rmap[rseg].m_SegType, dpos, rlength,
                    rmap[rseg].m_MinusStrand);
                dpos += rlength;
                break;
            }
        case CSeqMap::eSeqRef:
            {
                // Resolve multi-level references
                TSignedSeqPos rshift
                    = rmap[rseg].m_RefPos - rmap[rseg].m_Position;
                // Unlock mutex to prevent dead-locks
                guard.reset();
                x_ResolveMapSegment(rmap[rseg].m_RefSeq,
                    rstart + rshift, rlength,
                    dmap, dpos, dstop, scope);
                guard.reset(new CMutexGuard(m_DataSource_Mtx));    
                break;
            }
        }
        start += rlength;
        len -= rlength;
        if (++rseg >= rmap.size())
            break;
    }
}
#endif
#if 0
void CDataSource::x_AppendResolved(const CSeq_id_Handle& seqid,
                                   TSeqPos ref_pos,
                                   TSeqPos ref_len,
                                   bool minus_strand,
                                   vector<CSeqMap::CSegment>& segments,
                                   CScope& scope)
{
    _TRACE("x_AppendResolved("<<seqid.AsString()<<','<<ref_pos<<','<<ref_len<<','<<minus_strand<<')');
    CBioseq_Handle rbsh =
        scope.GetBioseqHandle(GetSeq_id_Mapper().GetSeq_id(seqid));
    if ( !rbsh ) {
        THROW1_TRACE(runtime_error,
                     "CDataSource::x_AppendResolved() -- "
                     "Can not resolve sequence reference.");
    }
    const CSeqMap& rmap = rbsh.GetSeqMap();
    PrintSeqMap(GetSeq_id_Mapper().GetHandle(*rbsh.GetSeqId()).AsString(), rmap);
    TSeqPos ref_end = ref_pos + ref_len;
    for ( CSeqMap::const_iterator it = rmap.find(minus_strand? ref_end-1: ref_pos);; ) {
        // intersection of current segment and referenced region
        TSeqPos pos = max(ref_pos, it.GetPosition());
        TSeqPos end = min(ref_pos+ref_len, it.GetPosition()+it.GetLength());
        if ( end <= pos ) // no intersection -> end of region
            break;
        TSeqPos shift = minus_strand?
            ref_pos+ref_len-end:
            pos - it.GetPosition();
        TSeqPos len = end - pos;
        _TRACE("  segment: "<<it.GetPosition()<<' '<<it.GetLength()<<"  pos="<<pos<<" end="<<end<<" len="<<len<<" shift="<<shift);
        switch ( it.GetType() ) {
        case CSeqMap::eSeqData:
            x_AppendRef(seqid, pos, len,
                        it.GetRefMinusStrand() ^ minus_strand, segments);
            break;
        case CSeqMap::eSeqRef:
            x_AppendResolved(it.GetRefSeqid(),
                             it.GetRefPosition() + shift, len,
                             it.GetRefMinusStrand() ^ minus_strand,
                             segments, scope);
            break;
        default:
            segments.push_back(CSeqMap::CSegment(it.GetType(), len));
            break;
        }
        if ( minus_strand ) {
            // go to previous segment
            if ( pos == ref_pos ) // no more
                break;
            --it;
        }
        else {
            // go to next segment
            if ( end == ref_end ) // no more
                break;
            ++it;
        }
    }

#if 0
    auto_ptr<CMutexGuard> guard(new CMutexGuard(m_DataSource_Mtx));    
    // Get enough segments to cover the "start+length" range on the
    // destination sequence
    while (dpos < dstop  &&  len > 0) {
        // Get and adjust the segment start
        TSeqPos rstart = rmap[rseg].m_Position;
        TSeqPos rlength = rmap[rseg].m_Length;
        if (rstart < start) {
            rlength -= start - rstart;
            rstart = start;
        }
        if (rstart + rlength > start + len)
            rlength = start + len - rstart;
        switch (rmap[rseg].m_SegType) {
        case CSeqMap::eSeqData:
        {
            x_AppendRef(rh,
                        rstart,
                        rlength,
                        rmap[rseg].m_MinusStrand,
                        segments);
            break;
        }
        case CSeqMap::eSeqGap:
        case CSeqMap::eSeqEnd:
        {
            // Copy gaps and literals
            segments.push_back(CSeqMap::CSegment(rmap[rseg].m_SegType,
                                                 rlength));
            // rmap[rseg].m_MinusStrand ??
            break;
        }
        case CSeqMap::eSeqRef:
        {
            // Resolve multi-level references
            TSignedSeqPos rshift
                = rmap[rseg].m_RefPos - rmap[rseg].m_Position;
            // Unlock mutex to prevent dead-locks
            guard.reset();
            x_AppendResolved(rmap[rseg].m_RefSeq,
                             rstart + rshift, rlength,
                             dmap, dpos, dstop, scope);
            guard.reset(new CMutexGuard(m_DataSource_Mtx));    
            break;
        }
        }
        start += rlength;
        len -= rlength;
        if (++rseg >= rmap.size())
            break;
    }
#endif
}
#endif

bool CDataSource::IsSynonym(const CSeq_id& id1, CSeq_id& id2) const
{
    CSeq_id_Handle h1 = GetSeq_id_Mapper().GetHandle(id1);
    CSeq_id_Handle h2 = GetSeq_id_Mapper().GetHandle(id2);

    CMutexGuard guard(m_DataSource_Mtx);    
    TTSEMap::const_iterator tse_set = m_TSE_seq.find(h1);
    if (tse_set == m_TSE_seq.end())
        return false; // Could not find id1 in the datasource
    non_const_iterate (TTSESet, tse_it, const_cast<TTSESet&>(tse_set->second) ) {
        const CBioseq_Info& bioseq =
            *const_cast<TBioseqMap&>((*tse_it)->m_BioseqMap)[h1];
        if (bioseq.m_Synonyms.find(h2) != bioseq.m_Synonyms.end())
            return true;
    }
    return false;
}


bool CDataSource::GetTSEHandles(CScope& scope,
                                const CSeq_entry& entry,
                                set<CBioseq_Handle>& handles,
                                CSeq_inst::EMol filter)
{
    // Find TSE_Info
    CRef<CSeq_entry> ref(const_cast<CSeq_entry*>(&entry));
    CMutexGuard guard(m_DataSource_Mtx);
    TEntries::iterator found = m_Entries.find(ref);
    if (found == m_Entries.end()  ||
        found->second->m_TSE.GetPointer() != &entry)
        return false; // entry not found or not a TSE

    // One lock for the whole bioseq iterator
    scope.x_AddToHistory(*found->second);

    // Get all bioseq handles from the TSE
    // Store seq-entries in the map to prevent duplicates
    typedef map<CSeq_entry*, CBioseq_Info*> TEntryToInfo;
    TEntryToInfo bioseq_map;
    // Populate the map
    non_const_iterate (CTSE_Info::TBioseqMap, bit, found->second->m_BioseqMap) {
        if (filter != CSeq_inst::eMol_not_set) {
            // Filter sequences
            _ASSERT(bit->second->m_Entry->IsSeq());
            if (bit->second->m_Entry->GetSeq().GetInst().GetMol() != filter)
                continue;
        }
        bioseq_map[const_cast<CSeq_entry*>(bit->second->m_Entry.GetPointer())] =
            bit->second;
    }
    // Convert each map entry into bioseq handle
    iterate (TEntryToInfo, eit, bioseq_map) {
        CBioseq_Handle h(*eit->second->m_Synonyms.begin());
        h.x_ResolveTo(scope, *this, *eit->first, *found->second);
        handles.insert(h);
    }
    return true;
}


void CDataSource::DebugDump(CDebugDumpContext ddc, unsigned int depth) const
{
    ddc.SetFrame("CDataSource");
    CObject::DebugDump( ddc, depth);

    ddc.Log("m_Loader", m_Loader,0);
    ddc.Log("m_pTopEntry", m_pTopEntry.GetPointer(),0);
    ddc.Log("m_ObjMgr", m_ObjMgr,0);

    if (depth == 0) {
        DebugDumpValue(ddc, "m_Entries.size()", m_Entries.size());
        DebugDumpValue(ddc, "m_TSE_seq.size()", m_TSE_seq.size());
        DebugDumpValue(ddc, "m_TSE_ref.size()", m_TSE_ref.size());
        DebugDumpValue(ddc, "m_SeqMaps.size()", m_SeqMaps.size());
    } else {
        DebugDumpValue(ddc, "m_Entries.type",
            "map<CRef<CSeq_entry>,CRef<CTSE_Info>>");
        DebugDumpPairsCRefCRef(ddc, "m_Entries",
            m_Entries.begin(), m_Entries.end(), depth);

        { //---  m_TSE_seq
            unsigned int depth2 = depth-1;
            DebugDumpValue(ddc, "m_TSE_seq.type",
                "map<CSeq_id_Handle,set<CRef<CTSE_Info>>>");
            CDebugDumpContext ddc2(ddc,"m_TSE_seq");
            TTSEMap::const_iterator it;
            for ( it = m_TSE_seq.begin(); it != m_TSE_seq.end(); ++it) {
                string member_name = "m_TSE_seq[ " +
                    (it->first).AsString() +" ]";
                if (depth2 == 0) {
                    member_name += ".size()";
                    DebugDumpValue(ddc2, member_name, (it->second).size());
                } else {
                    DebugDumpRangeCRef(ddc2, member_name,
                        (it->second).begin(), (it->second).end(), depth2);
                }
            }
        }

        { //---  m_TSE_ref
            unsigned int depth2 = depth-1;
            DebugDumpValue(ddc, "m_TSE_ref.type",
                "map<CSeq_id_Handle,set<CRef<CTSE_Info>>>");
            CDebugDumpContext ddc2(ddc,"m_TSE_ref");
            TTSEMap::const_iterator it;
            for ( it = m_TSE_ref.begin(); it != m_TSE_ref.end(); ++it) {
                string member_name = "m_TSE_ref[ " +
                    (it->first).AsString() +" ]";
                if (depth2 == 0) {
                    member_name += ".size()";
                    DebugDumpValue(ddc2, member_name, (it->second).size());
                } else {
                    DebugDumpRangeCRef(ddc2, member_name,
                        (it->second).begin(), (it->second).end(), depth2);
                }
            }
        }

        DebugDumpValue(ddc, "m_SeqMaps.type",
            "map<const CBioseq*,CRef<CSeqMap>>");
        DebugDumpPairsCRefCRef(ddc, "m_SeqMaps",
            m_SeqMaps.begin(), m_SeqMaps.end(), depth);
    }
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.82  2003/02/24 14:51:11  grichenk
* Minor improvements in annot indexing
*
* Revision 1.81  2003/02/13 14:34:34  grichenk
* Renamed CAnnotObject -> CAnnotObject_Info
* + CSeq_annot_Info and CAnnotObject_Ref
* Moved some members of CAnnotObject to CSeq_annot_Info
* and CAnnotObject_Ref.
* Added feat/align/graph to CAnnotObject_Info map
* to CDataSource.
*
* Revision 1.80  2003/02/05 17:59:17  dicuccio
* Moved formerly private headers into include/objects/objmgr/impl
*
* Revision 1.79  2003/02/04 22:01:26  grichenk
* Fixed annotations loading/indexing order
*
* Revision 1.78  2003/02/04 21:46:32  grichenk
* Added map of annotations by intervals (the old one was
* by total ranges)
*
* Revision 1.77  2003/01/29 22:03:46  grichenk
* Use single static CSeq_id_Mapper instead of per-OM model.
*
* Revision 1.76  2003/01/29 17:45:02  vasilche
* Annotaions index is split by annotation/feature type.
*
* Revision 1.75  2003/01/22 20:11:54  vasilche
* Merged functionality of CSeqMapResolved_CI to CSeqMap_CI.
* CSeqMap_CI now supports resolution and iteration over sequence range.
* Added several caches to CScope.
* Optimized CSeqVector().
* Added serveral variants of CBioseqHandle::GetSeqVector().
* Tried to optimize annotations iterator (not much success).
* Rewritten CHandleRange and CHandleRangeMap classes to avoid sorting of list.
*
* Revision 1.74  2002/12/26 16:39:24  vasilche
* Object manager class CSeqMap rewritten.
*
* Revision 1.73  2002/12/20 20:54:24  grichenk
* Added optional location/product switch to CFeat_CI
*
* Revision 1.72  2002/12/19 20:16:39  grichenk
* Fixed locations on minus strand
*
* Revision 1.71  2002/12/06 15:36:00  grichenk
* Added overlap type for annot-iterators
*
* Revision 1.70  2002/11/08 22:15:51  grichenk
* Added methods for removing/replacing annotations
*
* Revision 1.69  2002/11/08 21:03:30  ucko
* CConstRef<> now requires an explicit constructor.
*
* Revision 1.68  2002/11/04 21:29:12  grichenk
* Fixed usage of const CRef<> and CRef<> constructor
*
* Revision 1.67  2002/10/18 19:12:40  grichenk
* Removed mutex pools, converted most static mutexes to non-static.
* Protected CSeqMap::x_Resolve() with mutex. Modified code to prevent
* dead-locks.
*
* Revision 1.66  2002/10/16 20:44:16  ucko
* MIPSpro: use #pragma instantiate rather than template<>, as the latter
* ended up giving rld errors for CMutexPool_Base<*>::sm_Pool.  (Sigh.)
*
* Revision 1.65  2002/10/02 17:58:23  grichenk
* Added sequence type filter to CBioseq_CI
*
* Revision 1.64  2002/09/30 20:01:19  grichenk
* Added methods to support CBioseq_CI
*
* Revision 1.63  2002/09/16 19:59:36  grichenk
* Fixed getting reference to a gap on minus strand
*
* Revision 1.62  2002/09/10 19:55:49  grichenk
* Throw exception when unable to create resolved seq-map
*
* Revision 1.61  2002/08/09 14:58:50  ucko
* Restrict template <> to MIPSpro for now, as it also leads to link
* errors with Compaq's compiler.  (Sigh.)
*
* Revision 1.60  2002/08/08 19:51:16  ucko
* Omit EMPTY_TEMPLATE for GCC and KCC, as it evidently leads to link errors(!)
*
* Revision 1.59  2002/08/08 14:28:00  ucko
* Add EMPTY_TEMPLATE to explicit instantiations.
*
* Revision 1.58  2002/08/07 18:22:48  ucko
* Explicitly instantiate CMutexPool_Base<{CDataSource,TTSESet}>::sm_Pool
*
* Revision 1.57  2002/07/25 15:01:51  grichenk
* Replaced non-const GetXXX() with SetXXX()
*
* Revision 1.56  2002/07/08 20:51:01  grichenk
* Moved log to the end of file
* Replaced static mutex (in CScope, CDataSource) with the mutex
* pool. Redesigned CDataSource data locking.
*
* Revision 1.55  2002/07/01 15:44:51  grichenk
* Fixed typos
*
* Revision 1.54  2002/07/01 15:40:58  grichenk
* Fixed 'tse_set' warning.
* Removed duplicate call to tse_set.begin().
* Fixed version resolving for annotation iterators.
* Fixed strstream bug in KCC.
*
* Revision 1.53  2002/06/28 17:30:42  grichenk
* Duplicate seq-id: ERR_POST() -> THROW1_TRACE() with the seq-id
* list.
* Fixed x_CleanupUnusedEntries().
* Fixed a bug with not found sequences.
* Do not copy bioseq data in GetBioseqCore().
*
* Revision 1.52  2002/06/21 15:12:15  grichenk
* Added resolving seq-id to the best version
*
* Revision 1.51  2002/06/12 14:39:53  grichenk
* Performance improvements
*
* Revision 1.50  2002/06/04 17:18:33  kimelman
* memory cleanup :  new/delete/Cref rearrangements
*
* Revision 1.49  2002/05/31 17:53:00  grichenk
* Optimized for better performance (CTSE_Info uses atomic counter,
* delayed annotations indexing, no location convertions in
* CAnnot_Types_CI if no references resolution is required etc.)
*
* Revision 1.48  2002/05/29 21:21:13  gouriano
* added debug dump
*
* Revision 1.47  2002/05/28 18:00:43  gouriano
* DebugDump added
*
* Revision 1.46  2002/05/24 14:57:12  grichenk
* SerialAssign<>() -> CSerialObject::Assign()
*
* Revision 1.45  2002/05/21 18:57:25  grichenk
* Fixed annotations dropping
*
* Revision 1.44  2002/05/21 18:40:50  grichenk
* Fixed annotations droppping
*
* Revision 1.43  2002/05/14 20:06:25  grichenk
* Improved CTSE_Info locking by CDataSource and CDataLoader
*
* Revision 1.42  2002/05/13 15:28:27  grichenk
* Fixed seqmap for virtual sequences
*
* Revision 1.41  2002/05/09 14:18:15  grichenk
* More TSE conflict resolving rules for annotations
*
* Revision 1.40  2002/05/06 03:28:46  vakatov
* OM/OM1 renaming
*
* Revision 1.39  2002/05/03 21:28:09  ucko
* Introduce T(Signed)SeqPos.
*
* Revision 1.38  2002/05/02 20:42:37  grichenk
* throw -> THROW1_TRACE
*
* Revision 1.37  2002/04/22 20:05:08  grichenk
* Fixed minor bug in GetSequence()
*
* Revision 1.36  2002/04/19 18:02:47  kimelman
* add verify to catch coredump
*
* Revision 1.35  2002/04/17 21:09:14  grichenk
* Fixed annotations loading
* +IsSynonym()
*
* Revision 1.34  2002/04/11 18:45:39  ucko
* Pull in extra headers to make KCC happy.
*
* Revision 1.33  2002/04/11 12:08:21  grichenk
* Fixed GetResolvedSeqMap() implementation
*
* Revision 1.32  2002/04/05 21:23:08  grichenk
* More duplicate id warnings fixed
*
* Revision 1.31  2002/04/05 20:27:52  grichenk
* Fixed duplicate identifier warning
*
* Revision 1.30  2002/04/04 21:33:13  grichenk
* Fixed GetSequence() for sequences with unresolved segments
*
* Revision 1.29  2002/04/03 18:06:47  grichenk
* Fixed segmented sequence bugs (invalid positioning of literals
* and gaps). Improved CSeqVector performance.
*
* Revision 1.27  2002/03/28 14:02:31  grichenk
* Added scope history checks to CDataSource::x_FindBestTSE()
*
* Revision 1.26  2002/03/22 17:24:12  gouriano
* loader-related fix in DropTSE()
*
* Revision 1.25  2002/03/21 21:39:48  grichenk
* garbage collector bugfix
*
* Revision 1.24  2002/03/20 21:24:59  gouriano
* *** empty log message ***
*
* Revision 1.23  2002/03/15 18:10:08  grichenk
* Removed CRef<CSeq_id> from CSeq_id_Handle, added
* key to seq-id map th CSeq_id_Mapper
*
* Revision 1.22  2002/03/11 21:10:13  grichenk
* +CDataLoader::ResolveConflict()
*
* Revision 1.21  2002/03/07 21:25:33  grichenk
* +GetSeq_annot() in annotation iterators
*
* Revision 1.20  2002/03/06 19:37:19  grichenk
* Fixed minor bugs and comments
*
* Revision 1.19  2002/03/05 18:44:55  grichenk
* +x_UpdateTSEStatus()
*
* Revision 1.18  2002/03/05 16:09:10  grichenk
* Added x_CleanupUnusedEntries()
*
* Revision 1.17  2002/03/04 15:09:27  grichenk
* Improved MT-safety. Added live/dead flag to CDataSource methods.
*
* Revision 1.16  2002/02/28 20:53:31  grichenk
* Implemented attaching segmented sequence data. Fixed minor bugs.
*
* Revision 1.15  2002/02/25 21:05:29  grichenk
* Removed seq-data references caching. Increased MT-safety. Fixed typos.
*
* Revision 1.14  2002/02/21 19:27:05  grichenk
* Rearranged includes. Added scope history. Added searching for the
* best seq-id match in data sources and scopes. Updated tests.
*
* Revision 1.13  2002/02/20 20:23:27  gouriano
* corrected FilterSeqid()
*
* Revision 1.12  2002/02/15 20:35:38  gouriano
* changed implementation of HandleRangeMap
*
* Revision 1.11  2002/02/12 19:41:42  grichenk
* Seq-id handles lock/unlock moved to CSeq_id_Handle 'ctors.
*
* Revision 1.10  2002/02/07 21:27:35  grichenk
* Redesigned CDataSource indexing: seq-id handle -> TSE -> seq/annot
*
* Revision 1.9  2002/02/06 21:46:11  gouriano
* *** empty log message ***
*
* Revision 1.8  2002/02/05 21:46:28  gouriano
* added FindSeqid function, minor tuneup in CSeq_id_mapper
*
* Revision 1.7  2002/01/30 22:09:28  gouriano
* changed CSeqMap interface
*
* Revision 1.6  2002/01/29 17:45:00  grichenk
* Added seq-id handles locking
*
* Revision 1.5  2002/01/28 19:44:49  gouriano
* changed the interface of BioseqHandle: two functions moved from Scope
*
* Revision 1.4  2002/01/23 21:59:31  grichenk
* Redesigned seq-id handles and mapper
*
* Revision 1.3  2002/01/18 15:56:23  gouriano
* changed TSeqMaps definition
*
* Revision 1.2  2002/01/16 16:25:57  gouriano
* restructured objmgr
*
* Revision 1.1  2002/01/11 19:06:18  gouriano
* restructured objmgr
*
*
* ===========================================================================
*/
