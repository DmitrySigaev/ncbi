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
*
*/

#include <objects/objmgr/bioseq_handle.hpp>
#include <objects/objmgr/impl/data_source.hpp>
#include <objects/objmgr/impl/tse_info.hpp>
#include <objects/objmgr/impl/handle_range.hpp>
#include <objects/objmgr/seq_vector.hpp>
#include <objects/objmgr/scope.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seqloc/Seq_point.hpp>
#include <objects/general/Object_id.hpp>
#include <serial/typeinfo.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


CBioseq_Handle::~CBioseq_Handle(void)
{
}


const CSeq_id* CBioseq_Handle::GetSeqId(void) const
{
    if (!m_Value) return 0;
    return m_Value.x_GetSeqId();
}


const CBioseq& CBioseq_Handle::GetBioseq(void) const
{
    return x_GetDataSource().GetBioseq(*this);
}


const CSeq_entry& CBioseq_Handle::GetTopLevelSeqEntry(void) const
{
    // Can not use m_TSE->m_TSE since the handle may be unresolved yet
    return x_GetDataSource().GetTSE(*this);
}


CBioseq_Handle::TBioseqCore CBioseq_Handle::GetBioseqCore(void) const
{
    return x_GetDataSource().GetBioseqCore(*this);
}


const CSeqMap& CBioseq_Handle::GetSeqMap(void) const
{
    return x_GetDataSource().GetSeqMap(*this);
}


CSeqVector CBioseq_Handle::GetSeqVector(EVectorCoding coding,
                                        ENa_strand strand) const
{
    return CSeqVector(GetSeqMap(), *m_Scope, coding, strand);
}


CSeqVector CBioseq_Handle::GetSeqVector(ENa_strand strand) const
{
    return CSeqVector(GetSeqMap(), *m_Scope, eCoding_Ncbi, strand);
}


CSeqVector CBioseq_Handle::GetSeqVector(EVectorCoding coding,
                                        EVectorStrand strand) const
{
    return CSeqVector(GetSeqMap(), *m_Scope, coding,
		      strand == eStrand_Minus?
		      eNa_strand_minus: eNa_strand_plus);
}


CSeqVector CBioseq_Handle::GetSeqVector(EVectorStrand strand) const
{
    return CSeqVector(GetSeqMap(), *m_Scope, eCoding_Ncbi,
		      strand == eStrand_Minus?
		      eNa_strand_minus: eNa_strand_plus);
}

#if 0
CSeqVector CBioseq_Handle::GetSeqVector(bool use_iupac_coding,
                                        bool plus_strand) const
{
    return GetSeqVector(use_iupac_coding ? eCoding_Iupac : eCoding_Ncbi,
                        plus_strand ? eNa_strand_plus : eNa_strand_minus);
}
#endif

bool CBioseq_Handle::x_IsSynonym(const CSeq_id& id) const
{
    if ( !(*this) )
        return false;
    CSeq_id_Handle h = CSeq_id_Mapper::GetSeq_id_Mapper().GetHandle(id);
    const CSynonymsSet* syns = m_Scope->x_GetSynonyms(m_Value);
    return syns->find(h) != syns->end();
}


CSeqVector CBioseq_Handle::GetSequenceView(const CSeq_loc& location,
                                           ESequenceViewMode mode,
                                           EVectorCoding coding,
                                           ENa_strand strand) const
{
    if ( mode != eViewConstructed )
	strand = eNa_strand_unknown;
    return CSeqVector(GetSeqMapByLocation(location, mode), *m_Scope,
		      coding, strand);
}

/*
CConstRef<CSeqMap>
CBioseq_Handle::CreateSeqMapForStrand(CConstRef<CSeqMap> seqMap,
                                      ENa_strand strand) const
{
    return CSeqMap::CreateSeqMapForStrand(seqMap, strand);

}


CConstRef<CSeqMap> CBioseq_Handle::GetSeqMapByStrand(ENa_strand strand) const
{
    return CreateSeqMapForStrand(CConstRef<CSeqMap>(&GetSeqMap()), strand);
}
*/

CConstRef<CSeqMap>
CBioseq_Handle::GetSeqMapByLocation(const CSeq_loc& loc,
				    ESequenceViewMode mode) const
{
    CConstRef<CSeqMap> ret;
    if ( mode == eViewConstructed ) {
        ret = CSeqMap::CreateSeqMapForSeq_loc(loc, &x_GetDataSource());
    }
    else {
        // Parse the location
        CHandleRange rlist;      // all intervals pointing to the sequence
        CSeq_loc_CI loc_it(loc);
        for ( ; loc_it; ++loc_it) {
            if ( !x_IsSynonym(loc_it.GetSeq_id()) )
                continue;
            rlist.AddRange(loc_it.GetRange(), loc_it.GetStrand());
        }

        // Make mode-dependent parsing of the range list
        CHandleRange mode_rlist; // processed intervals (merged, excluded)
        switch (mode) {
        case eViewMerged:
        {
            // Merge intervals from "rlist"
            ITERATE (CHandleRange, rit, rlist) {
                mode_rlist.MergeRange(rit->first, rit->second);
            }
            break;
        }
        case eViewExcluded:
        {
            // Exclude intervals from "rlist"
            TSeqPos last_from = 0;
            ITERATE (CHandleRange, rit, rlist) {
                if (last_from < rit->first.GetFrom()) {
                    mode_rlist.MergeRange(
                        CHandleRange::TRange(last_from, rit->first.GetFrom()-1),
                        eNa_strand_unknown);
                }
                if ( !rit->first.IsWholeTo() ) {
                    last_from = rit->first.GetTo()+1;
                }
                else {
                    last_from = CHandleRange::TRange::GetWholeTo();
                }
            }
            TSeqPos total_length = GetSeqMap().GetLength(m_Scope);
            if (last_from < total_length) {
                mode_rlist.MergeRange(
                    CHandleRange::TRange(last_from, total_length-1),
                    eNa_strand_unknown);
            }
            break;
        }
        }

        // Convert ranges to seq-loc
        CRef<CSeq_loc> view_loc(new CSeq_loc);
        ITERATE (CHandleRange, rit, mode_rlist) {
            CRef<CSeq_loc> seg_loc(new CSeq_loc);
            CRef<CSeq_id> id(new CSeq_id);
            id->Assign(CSeq_id_Mapper::GetSeq_id_Mapper().GetSeq_id(m_Value));
            seg_loc->SetInt().SetId(*id);
            seg_loc->SetInt().SetFrom(rit->first.GetFrom());
            seg_loc->SetInt().SetTo(rit->first.GetTo());
            view_loc->SetMix().Set().push_back(seg_loc);
        }
        ret = CSeqMap::CreateSeqMapForSeq_loc(*view_loc, &x_GetDataSource());
    }
    return ret;
}


void CBioseq_Handle::x_ResolveTo(
    CScope& scope, CBioseq_Info& bioseq)
{
    m_Scope = &scope;
    m_Bioseq_Info.Reset(&bioseq);
    m_TSE_Lock.Set(*m_Bioseq_Info->m_TSE_Info);
}

/*
const CSeqMap& CBioseq_Handle::CreateResolvedSeqMap(void) const
{
    return x_GetDataSource().GetResolvedSeqMap(*this);
}
*/

void CBioseq_Handle::AddAnnot(CSeq_annot& annot)
{
    _ASSERT(bool(m_Bioseq_Info)  &&  bool(m_Bioseq_Info->m_Entry));
    x_GetDataSource().AttachAnnot(*m_Bioseq_Info->m_Entry, annot);
}


void CBioseq_Handle::RemoveAnnot(const CSeq_annot& annot)
{
    _ASSERT(bool(m_Bioseq_Info)  &&  bool(m_Bioseq_Info->m_Entry));
    x_GetDataSource().RemoveAnnot(*m_Bioseq_Info->m_Entry, annot);
}


void CBioseq_Handle::ReplaceAnnot(const CSeq_annot& old_annot,
                                  CSeq_annot& new_annot)
{
    _ASSERT(bool(m_Bioseq_Info)  &&  bool(m_Bioseq_Info->m_Entry));
    x_GetDataSource().ReplaceAnnot(*m_Bioseq_Info->m_Entry, old_annot, new_annot);
}



END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.33  2003/03/12 20:09:33  grichenk
* Redistributed members between CBioseq_Handle, CBioseq_Info and CTSE_Info
*
* Revision 1.32  2003/03/11 15:51:06  kuznets
* iterate -> ITERATE
*
* Revision 1.31  2003/02/05 17:59:17  dicuccio
* Moved formerly private headers into include/objects/objmgr/impl
*
* Revision 1.30  2003/01/29 22:03:46  grichenk
* Use single static CSeq_id_Mapper instead of per-OM model.
*
* Revision 1.29  2003/01/23 19:33:57  vasilche
* Commented out obsolete methods.
* Use strand argument of CSeqVector instead of creation reversed seqmap.
* Fixed ordering operators of CBioseqHandle to be consistent.
*
* Revision 1.28  2003/01/22 20:11:54  vasilche
* Merged functionality of CSeqMapResolved_CI to CSeqMap_CI.
* CSeqMap_CI now supports resolution and iteration over sequence range.
* Added several caches to CScope.
* Optimized CSeqVector().
* Added serveral variants of CBioseqHandle::GetSeqVector().
* Tried to optimize annotations iterator (not much success).
* Rewritten CHandleRange and CHandleRangeMap classes to avoid sorting of list.
*
* Revision 1.27  2002/12/26 20:55:17  dicuccio
* Moved seq_id_mapper.hpp, tse_info.hpp, and bioseq_info.hpp -> include/ tree
*
* Revision 1.26  2002/12/26 16:39:24  vasilche
* Object manager class CSeqMap rewritten.
*
* Revision 1.25  2002/12/06 15:36:00  grichenk
* Added overlap type for annot-iterators
*
* Revision 1.24  2002/11/08 22:15:51  grichenk
* Added methods for removing/replacing annotations
*
* Revision 1.23  2002/11/08 19:43:35  grichenk
* CConstRef<> constructor made explicit
*
* Revision 1.22  2002/09/03 21:27:01  grichenk
* Replaced bool arguments in CSeqVector constructor and getters
* with enums.
*
* Revision 1.21  2002/07/10 16:49:29  grichenk
* Removed commented reference to old CDataSource mutex
*
* Revision 1.20  2002/07/08 20:51:01  grichenk
* Moved log to the end of file
* Replaced static mutex (in CScope, CDataSource) with the mutex
* pool. Redesigned CDataSource data locking.
*
* Revision 1.19  2002/06/12 14:39:02  grichenk
* Renamed enumerators
*
* Revision 1.18  2002/06/06 21:00:42  clausen
* Added include for scope.hpp
*
* Revision 1.17  2002/05/31 17:53:00  grichenk
* Optimized for better performance (CTSE_Info uses atomic counter,
* delayed annotations indexing, no location convertions in
* CAnnot_Types_CI if no references resolution is required etc.)
*
* Revision 1.16  2002/05/24 14:57:12  grichenk
* SerialAssign<>() -> CSerialObject::Assign()
*
* Revision 1.15  2002/05/21 18:39:30  grichenk
* CBioseq_Handle::GetResolvedSeqMap() -> CreateResolvedSeqMap()
*
* Revision 1.14  2002/05/06 03:28:46  vakatov
* OM/OM1 renaming
*
* Revision 1.13  2002/05/03 21:28:08  ucko
* Introduce T(Signed)SeqPos.
*
* Revision 1.12  2002/04/29 16:23:28  grichenk
* GetSequenceView() reimplemented in CSeqVector.
* CSeqVector optimized for better performance.
*
* Revision 1.11  2002/04/23 19:01:07  grichenk
* Added optional flag to GetSeqVector() and GetSequenceView()
* for switching to IUPAC encoding.
*
* Revision 1.10  2002/04/22 20:06:59  grichenk
* +GetSequenceView(), +x_IsSynonym()
*
* Revision 1.9  2002/04/11 12:07:30  grichenk
* Redesigned CAnnotTypes_CI to resolve segmented sequences correctly.
*
* Revision 1.8  2002/03/19 19:16:28  gouriano
* added const qualifier to GetTitle and GetSeqVector
*
* Revision 1.7  2002/03/15 18:10:07  grichenk
* Removed CRef<CSeq_id> from CSeq_id_Handle, added
* key to seq-id map th CSeq_id_Mapper
*
* Revision 1.6  2002/03/04 15:08:44  grichenk
* Improved CTSE_Info locks
*
* Revision 1.5  2002/02/21 19:27:05  grichenk
* Rearranged includes. Added scope history. Added searching for the
* best seq-id match in data sources and scopes. Updated tests.
*
* Revision 1.4  2002/01/28 19:44:49  gouriano
* changed the interface of BioseqHandle: two functions moved from Scope
*
* Revision 1.3  2002/01/23 21:59:31  grichenk
* Redesigned seq-id handles and mapper
*
* Revision 1.2  2002/01/16 16:25:56  gouriano
* restructured objmgr
*
* Revision 1.1  2002/01/11 19:06:17  gouriano
* restructured objmgr
*
*
* ===========================================================================
*/
