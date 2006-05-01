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
* Author: Maxim Didenko
*
* File Description:
*
*/


#include <ncbi_pch.hpp>

#include <objmgr/impl/tse_info.hpp>
#include <objmgr/impl/tse_assigner.hpp>
#include <objmgr/impl/bioseq_set_info.hpp>
#include <objmgr/impl/bioseq_info.hpp>
#include <objmgr/impl/seq_annot_info.hpp>
#include <objmgr/impl/data_source.hpp>

#include <objmgr/seq_map.hpp>
#include <objmgr/objmgr_exception.hpp>

#include <objects/seq/Seq_literal.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

CBioseq_Info& ITSE_Assigner::x_GetBioseq(CTSE_Info& tse_info,
                                         const TBioseqId& place_id)
{
    return tse_info.x_GetBioseq(place_id);
}


CBioseq_set_Info& ITSE_Assigner::x_GetBioseq_set(CTSE_Info& tse_info,
                                                 TBioseq_setId place_id)
{
    return tse_info.x_GetBioseq_set(place_id);
}


CBioseq_Base_Info& ITSE_Assigner::x_GetBase(CTSE_Info& tse_info,
                                            const TPlace& place)
{
    if ( place.first ) {
        return x_GetBioseq(tse_info, place.first);
    }
    else {
        return x_GetBioseq_set(tse_info, place.second);
    }
}


CBioseq_Info& ITSE_Assigner::x_GetBioseq(CTSE_Info& tse_info,
                                         const TPlace& place)
{
    if ( place.first ) {
        return x_GetBioseq(tse_info, place.first);
    }
    else {
        NCBI_THROW(CObjMgrException, eOtherError,
                   "Bioseq-set id where gi is expected");
    }
}


CBioseq_set_Info& ITSE_Assigner::x_GetBioseq_set(CTSE_Info& tse_info,
                                                 const TPlace& place)
{
    if ( place.first ) {
        NCBI_THROW(CObjMgrException, eOtherError,
                   "Gi where Bioseq-set id is expected");
    }
    else {
        return x_GetBioseq_set(tse_info, place.second);
    }
}
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
CTSE_Default_Assigner::CTSE_Default_Assigner() 
{
}

CTSE_Default_Assigner::~CTSE_Default_Assigner()
{
}

void CTSE_Default_Assigner::AddDescInfo(CTSE_Info& tse, 
                                        const TDescInfo& info, 
                                        TChunkId chunk_id)
{
    x_GetBase(tse, info.second)
              .x_AddDescrChunkId(info.first, chunk_id);
}

void CTSE_Default_Assigner::AddAnnotPlace(CTSE_Info& tse, 
                                          const TPlace& place, 
                                          TChunkId chunk_id)
{
    x_GetBase(tse, place)
              .x_AddAnnotChunkId(chunk_id);
}
void CTSE_Default_Assigner::AddBioseqPlace(CTSE_Info& tse, 
                                           TBioseq_setId place_id, 
                                           TChunkId chunk_id)
{
    if ( place_id == kTSE_Place_id ) {
        tse.x_SetBioseqChunkId(chunk_id);
    }
    else {
        x_GetBioseq_set(tse, place_id).x_AddBioseqChunkId(chunk_id);
    }
}
void CTSE_Default_Assigner::AddSeq_data(CTSE_Info& tse,
                                        const TLocationSet& locations,
                                        CTSE_Chunk_Info& chunk)
{
    CBioseq_Info* last_bioseq = 0, *bioseq;
    ITERATE ( TLocationSet, it, locations ) {
        const CSeq_id_Handle& loc_id = it->first;
        int gi = loc_id.GetGi();
        bioseq = &x_GetBioseq(tse, it->first);
        if (bioseq != last_bioseq) {
            // Do not add duplicate chunks to the same bioseq
            bioseq->x_AddSeq_dataChunkId(chunk.GetChunkId());
        }
        last_bioseq = bioseq;

        CSeqMap& seq_map = const_cast<CSeqMap&>(bioseq->GetSeqMap());
        seq_map.SetRegionInChunk(chunk,
                                 it->second.GetFrom(),
                                 it->second.GetLength());
    }
}

void CTSE_Default_Assigner::AddAssemblyInfo(CTSE_Info& tse, 
                                            const TAssemblyInfo& info, 
                                            TChunkId chunk_id)
{
    x_GetBioseq(tse, info)
                .x_AddAssemblyChunkId(chunk_id);
}

void CTSE_Default_Assigner::UpdateAnnotIndex(CTSE_Info& tse, 
                                             CTSE_Chunk_Info& chunk)
{
    CDataSource::TAnnotLockWriteGuard guard1;
    if( tse.HasDataSource() )
        guard1.Guard(tse.GetDataSource());
    CTSE_Info::TAnnotLockWriteGuard guard2(tse.GetAnnotLock());          
    chunk.x_UpdateAnnotIndex(tse);
}

    // loading results
void CTSE_Default_Assigner::LoadDescr(CTSE_Info& tse, 
                                      const TPlace& place, 
                                      const CSeq_descr& descr)
{
    x_GetBase(tse, place).AddSeq_descr(descr);
}

void CTSE_Default_Assigner::LoadAnnot(CTSE_Info& tse,
                                      const TPlace& place, 
                                      CRef<CSeq_annot> annot)
{
    CRef<CSeq_annot_Info> annot_info;
    {{
        CDataSource::TMainLock::TWriteLockGuard guard;
        if( tse.HasDataSource() )
            guard.Guard(tse.GetDataSource().GetMainLock());
        annot_info.Reset(x_GetBase(tse, place).AddAnnot(*annot));
    }}
    {{
        CDataSource::TAnnotLockWriteGuard guard;
        if( tse.HasDataSource() )
            guard.Guard(tse.GetDataSource());
        tse.UpdateAnnotIndex(*annot_info);
    }}
}

void CTSE_Default_Assigner::LoadBioseq(CTSE_Info& tse,
                                       const TPlace& place, 
                                       CRef<CSeq_entry> entry)
{
    {{
        CDataSource::TMainLock::TWriteLockGuard guard;
        if( tse.HasDataSource() )
            guard.Guard(tse.GetDataSource().GetMainLock());
        if (place == TPlace(CSeq_id_Handle(), kTSE_Place_id)) {
            CRef<CSeq_entry_Info> entry_info(new CSeq_entry_Info(*entry));
            tse.x_SetObject(*entry_info, 0); //???
        }
        else {
            x_GetBioseq_set(tse, place).AddEntry(*entry);
        }
    }}

}

void CTSE_Default_Assigner::LoadSequence(CTSE_Info& tse, 
                                         const TPlace& place, 
                                         TSeqPos pos, 
                                         const TSequence& sequence)
{
    CSeqMap& seq_map = const_cast<CSeqMap&>(x_GetBioseq(tse, place).GetSeqMap());;
    ITERATE ( TSequence, it, sequence ) {
        const CSeq_literal& literal = **it;
        seq_map.LoadSeq_data(pos, literal.GetLength(), literal.GetSeq_data());
        pos += literal.GetLength();
    }
}

void CTSE_Default_Assigner::LoadAssembly(CTSE_Info& tse,
                                         const TBioseqId& seq_id,
                                         const TAssembly& assembly)
{
    x_GetBioseq(tse, seq_id).SetInst_Hist_Assembly(assembly);
}

void CTSE_Default_Assigner::LoadSeq_entry(CTSE_Info& tse,
                                          CSeq_entry& entry, 
                                          CTSE_SetObjectInfo* set_info)
{
    tse.SetSeq_entry(entry, set_info);
}


/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////


END_SCOPE(objects)
END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.8  2006/05/01 16:56:45  didenko
 * Attach SeqEntry edit command revamp
 *
 * Revision 1.7  2006/01/25 18:59:04  didenko
 * Redisgned bio objects edit facility
 *
 * Revision 1.6  2005/11/15 15:54:31  vasilche
 * Replaced CTSE_SNP_InfoMap with CTSE_SetObjectInfo to allow additional info.
 *
 * Revision 1.5  2005/08/31 19:36:44  didenko
 * Reduced the number of objects copies which are being created while doing PatchSeqIds
 *
 * Revision 1.4  2005/08/31 14:47:14  didenko
 * Changed the object parameter type for LoadAnnot and LoadBioseq methods
 *
 * Revision 1.3  2005/08/29 16:15:01  didenko
 * Modified default implementation of ITSE_Assigner in a way that it can be used as base class for
 * the user's implementations of this interface
 *
 * Revision 1.2  2005/08/25 15:37:29  didenko
 * Added call to InvalidateCache() method when the CSeq_los is bieng patched
 *
 * Revision 1.1  2005/08/25 14:05:37  didenko
 * Restructured TSE loading process
 *
 *
 * ===========================================================================
 */
