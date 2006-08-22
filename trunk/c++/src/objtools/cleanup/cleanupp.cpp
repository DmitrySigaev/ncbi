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
* Author:  Robert Smith
*
* File Description:
*   Implementation of private parts of basic cleanup.
*
*/
#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbistr.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objects/submit/Seq_submit.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/seqset/Bioseq_set.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seq/Seq_descr.hpp>
#include <objects/seq/Seqdesc.hpp>
#include <objects/seq/Pubdesc.hpp>
#include <objects/seq/Annot_descr.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_point.hpp>
#include <objects/pub/Pub_equiv.hpp>
#include <objects/pub/Pub.hpp>
#include <objects/seqfeat/BioSource.hpp>
#include <objects/seqblock/GB_block.hpp>
#include <objects/biblio/Cit_gen.hpp>
#include <serial/iterator.hpp>

#include <objects/seq/MolInfo.hpp>
#include <objects/seqfeat/Imp_feat.hpp>
#include <objects/general/Date.hpp>
#include <objmgr/util/sequence.hpp>
#include <objmgr/seq_annot_ci.hpp>
#include <objmgr/feat_ci.hpp>

#include <objmgr/scope.hpp>
#include <objmgr/seq_entry_handle.hpp>
#include <objmgr/bioseq_handle.hpp>
#include <objmgr/bioseq_set_handle.hpp>
#include <objmgr/seq_annot_handle.hpp>
#include <objmgr/seq_feat_handle.hpp>
#include <objmgr/bioseq_ci.hpp>

#include <objtools/cleanup/cleanup.hpp>
#include "../format/utils.hpp"
#include "cleanupp.hpp"
#include "cleanup_utils.hpp"

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

CCleanup_imp::CCleanup_imp(CRef<CCleanupChange> changes, CRef<CScope> scope, Uint4 options)
: m_Changes(changes), m_Options(options), m_Mode(eCleanup_GenBank), m_Scope (scope)
{
   
}


CCleanup_imp::~CCleanup_imp()
{
}


void CCleanup_imp::Setup(const CSeq_entry& se)
{
    /*** Set cleanup mode. ***/
    
    CConstRef<CBioseq> first_bioseq;
    switch (se.Which()) {
        case CSeq_entry::e_Seq:
        {
            first_bioseq.Reset(&se.GetSeq());
            break;            
        }
        case CSeq_entry::e_Set:
        {
            CTypeConstIterator<CBioseq> seq(ConstBegin(se.GetSet()));
            if (seq) {
                first_bioseq.Reset(&*seq);
            }
            break;            
        }
    }
    
    m_Mode = eCleanup_GenBank;
    if (first_bioseq) {
        ITERATE (CBioseq::TId, it, first_bioseq->GetId()) {
            const CSeq_id& id = **it;
            if (id.IsEmbl()  ||  id.IsTpe()) {
                m_Mode = eCleanup_EMBL;
            } else if (id.IsDdbj()  ||  id.IsTpd()) {
                m_Mode = eCleanup_DDBJ;
            } else if (id.IsSwissprot()) {
                m_Mode = eCleanup_SwissProt;
            }
        }        
    }
    
}


void CCleanup_imp::Finish(CSeq_entry& se)
{
    // cleanup for Cleanup.
    
}


void CCleanup_imp::ChangeMade(CCleanupChange::EChanges e)
{
    if (m_Changes) {
        m_Changes->SetChanged(e);
    }
}


void CCleanup_imp::BasicCleanup(CSeq_entry& se)
{
    Setup(se);
    switch (se.Which()) {
        case CSeq_entry::e_Seq:
            BasicCleanup(se.SetSeq());
            break;
        case CSeq_entry::e_Set:
            BasicCleanup(se.SetSet());
            break;
        case CSeq_entry::e_not_set:
        default:
            break;
    }
    Finish(se);
}


void CCleanup_imp::BasicCleanup(CSeq_submit& ss)
{
    // TODO Cleanup Submit-block.
    
    switch (ss.GetData().Which()) {
        case CSeq_submit::TData::e_Entrys:
            NON_CONST_ITERATE(CSeq_submit::TData::TEntrys, it, ss.SetData().SetEntrys()) {
                BasicCleanup(**it);
            }
            break;
        case CSeq_submit::TData::e_Annots:
            NON_CONST_ITERATE(CSeq_submit::TData::TAnnots, it, ss.SetData().SetAnnots()) {
                BasicCleanup(**it);
            }
            break;
        case CSeq_submit::TData::e_Delete:
        case CSeq_submit::TData::e_not_set:
        default:
            break;
    }
}


void CCleanup_imp::BasicCleanup(CBioseq_set& bss)
{
    if (bss.IsSetAnnot()) {
        NON_CONST_ITERATE (CBioseq_set::TAnnot, it, bss.SetAnnot()) {
            BasicCleanup(**it);
        }
    }
    if (bss.IsSetDescr()) {
        BasicCleanup(bss.SetDescr());
    }
    if (bss.IsSetSeq_set()) {
        // copies form BasicCleanup(CSeq_entry) to avoid recursing through it.
        NON_CONST_ITERATE (CBioseq_set::TSeq_set, it, bss.SetSeq_set()) {
            CSeq_entry& se = **it;
            switch (se.Which()) {
                case CSeq_entry::e_Seq:
                    BasicCleanup(se.SetSeq());
                    break;
                case CSeq_entry::e_Set:
                    BasicCleanup(se.SetSet());
                    break;
                case CSeq_entry::e_not_set:
                default:
                    break;
            }
        }
    }
}


void CCleanup_imp::BasicCleanup(CBioseq& bs)
{    
    if (bs.IsSetAnnot()) {
        NON_CONST_ITERATE (CBioseq::TAnnot, it, bs.SetAnnot()) {
            BasicCleanup(**it);
        }
    }
    if (bs.IsSetDescr()) {
        BasicCleanup(bs.SetDescr());
    }
}


void CCleanup_imp::BasicCleanup(CSeq_annot& sa)
{
    if (sa.IsSetData()  &&  sa.GetData().IsFtable()) {
        NON_CONST_ITERATE (CSeq_annot::TData::TFtable, it, sa.SetData().SetFtable()) {
            BasicCleanup(**it);
        }
    }
}


void CCleanup_imp::BasicCleanup(CSeq_descr& sdr)
{
    NON_CONST_ITERATE (CSeq_descr::Tdata, it, sdr.Set()) {
        BasicCleanup(**it);
    }
}


static bool s_SeqDescCommentCleanup( CSeqdesc::TComment& comment )
{
    bool changed = RemoveTrailingJunk(comment);
    if ( RemoveSpacesBetweenTildes(comment) ) {
        changed = true;
    }
    
    return changed;
};


void CCleanup_imp::BasicCleanup(CSeqdesc& sd)
{
    switch (sd.Which()) {
        case CSeqdesc::e_Mol_type:
            break;
        case CSeqdesc::e_Modif:
            break;
        case CSeqdesc::e_Method:
            break;
        case CSeqdesc::e_Name:
            if (CleanString( sd.SetName() ) ) {
                ChangeMade(CCleanupChange::eTrimSpaces);
            }
            break;
        case CSeqdesc::e_Title:
            if (CleanString( sd.SetTitle() ) ) {
                ChangeMade(CCleanupChange::eTrimSpaces);
            }
            break;
        case CSeqdesc::e_Org:
            BasicCleanup(sd.SetOrg() );
            break;
        case CSeqdesc::e_Comment:
            if (s_SeqDescCommentCleanup( sd.SetComment() ) ) {
                ChangeMade(CCleanupChange::eTrimSpaces);
            }
            break;
        case CSeqdesc::e_Num:
            break;
        case CSeqdesc::e_Maploc:
            break;
        case CSeqdesc::e_Pir:
            break;
        case CSeqdesc::e_Genbank:
            BasicCleanup(sd.SetGenbank());
            break;
        case CSeqdesc::e_Pub:
            BasicCleanup(sd.SetPub());
            break;
        case CSeqdesc::e_Region:
            if (CleanString( sd.SetRegion() ) ) {
                ChangeMade(CCleanupChange::eTrimSpaces);
            }
            break;
        case CSeqdesc::e_User:
            BasicCleanup( sd.SetUser() );
            break;
        case CSeqdesc::e_Sp:
            break;
        case CSeqdesc::e_Dbxref:
            break;
        case CSeqdesc::e_Embl:
            // BasicCleanup( sd.SetEmbl() ); // no BasicCleanup( CEMBL_block& ) yet.
            break;
        case CSeqdesc::e_Create_date:
            break;
        case CSeqdesc::e_Update_date:
            break;
        case CSeqdesc::e_Prf:
            break;
        case CSeqdesc::e_Pdb:
            break;
        case CSeqdesc::e_Het:
            break;
        case CSeqdesc::e_Source:
            BasicCleanup(sd.SetSource());
            break;
        case CSeqdesc::e_Molinfo:
            break;
            
        default:
            break;
    }
}


// CGB_block data member cleanup
void CCleanup_imp::BasicCleanup(CGB_block& gbb)   
{
    //
    //  origin:
    //  append period if there isn't one already
    //
    if ( gbb.CanGetOrigin() ) {
        const CGB_block::TOrigin& origin = gbb.GetOrigin();
        if ( ! origin.empty() && ! NStr::EndsWith(origin, ".")) {
            gbb.SetOrigin() += '.';
            ChangeMade(CCleanupChange::eChangeOther);
        }
    }
}


// Basic Cleanup w/Object Manager Handles.
// cleanup fields that object manager cares about. (like changing feature types.)

void CCleanup_imp::Setup(const CSeq_entry_Handle& seh)
{
    /*** Set cleanup mode. ***/
    
    CBioseq_Handle first_bioseq;
    switch (seh.Which()) {
        case CSeq_entry::e_Seq :
        {
            first_bioseq = seh.GetSeq();
            break;            
        }
        case CSeq_entry::e_Set :
        {
            CBioseq_CI bs_i(seh.GetSet());
            if (bs_i) {
                first_bioseq = *bs_i;
            }
            break;            
        }
    }
    
    m_Mode = eCleanup_GenBank;
    if (first_bioseq  &&  first_bioseq.CanGetId()) {
        ITERATE (CBioseq_Handle::TId, it, first_bioseq.GetId()) {
            const CSeq_id& id = *(it->GetSeqId());
            if (id.IsEmbl()  ||  id.IsTpe()) {
                m_Mode = eCleanup_EMBL;
            } else if (id.IsDdbj()  ||  id.IsTpd()) {
                m_Mode = eCleanup_DDBJ;
            } else if (id.IsSwissprot()) {
                m_Mode = eCleanup_SwissProt;
            }
        }        
    }
    
}


void CCleanup_imp::Finish(CSeq_entry_Handle& seh)
{
    // cleanup for Cleanup.
    
}

void CCleanup_imp::BasicCleanup(CSeq_entry_Handle& seh)
{
    Setup(seh);
    CFeat_CI fi(seh);
    for (; fi; ++fi) {
        BasicCleanup(fi->GetSeq_feat_Handle());
    }
    // do the non-handle stuff
    BasicCleanup(const_cast<CSeq_entry&>(*seh.GetCompleteSeq_entry()));
    Finish(seh);
}


void CCleanup_imp::BasicCleanup(const CBioseq_Handle& bsh)
{
    CFeat_CI fi(bsh);
    for (; fi; ++fi) {
        BasicCleanup(fi->GetSeq_feat_Handle());
    }
    // do the non-handle stuff
    BasicCleanup(const_cast<CBioseq&>(*bsh.GetCompleteBioseq()));
}


void CCleanup_imp::BasicCleanup(CBioseq_set_Handle& bssh)
{
    CBioseq_CI bsi(bssh);
    for (; bsi; ++bsi) {
        BasicCleanup(*bsi);
    }
    // do the non-handle stuff
    BasicCleanup(const_cast<CBioseq_set&>(*bssh.GetCompleteBioseq_set()));
}


void CCleanup_imp::BasicCleanup(CSeq_annot_Handle& sah)
{
    CFeat_CI fi(sah);
    for (; fi; ++fi) {
        BasicCleanup(fi->GetSeq_feat_Handle());
    }
    // do the non-handle stuff
    BasicCleanup(const_cast<CSeq_annot&>(*sah.GetCompleteSeq_annot()));
}


void CCleanup_imp::BasicCleanup(CSeq_loc& sl)
{
    if (sl.IsWhole()  &&  m_Scope) {
        // change the Seq-loc/whole to a Seq-loc/interval which covers the whole sequence.
        CSeq_id& id = sl.SetWhole();
        CBioseq_Handle bsh;
        try {
            bsh = m_Scope->GetBioseqHandle(id);
        } catch (...) { }
        if (bsh) {
            TSeqPos bs_len = bsh.GetBioseqLength();
            
            sl.SetInt().SetId(id);
            sl.SetInt().SetFrom(0);
            sl.SetInt().SetTo(bs_len - 1);
        }
    }
    
    switch (sl.Which()) {
    case CSeq_loc::e_Int :
        BasicCleanup(sl.SetInt());
        break;
    case CSeq_loc::e_Packed_int :
        {
            CSeq_loc::TPacked_int::Tdata& ints = sl.SetPacked_int().Set();
            NON_CONST_ITERATE(CSeq_loc::TPacked_int::Tdata, interval_it, ints) {
                BasicCleanup(**interval_it);
            }
            if (ints.size() == 1) {
                CRef<CSeq_interval> int_ref = ints.front();
                sl.SetInt(*int_ref);
                ChangeMade(CCleanupChange::eChangeSeqloc);
            }
        }
        break;
    case CSeq_loc::e_Pnt :
        {
            CSeq_loc::TPnt& pnt = sl.SetPnt();
            
            if (pnt.CanGetStrand()) {
                ENa_strand strand = pnt.GetStrand();
                if (strand == eNa_strand_both) {
                    pnt.SetStrand(eNa_strand_plus);
                    ChangeMade(CCleanupChange::eChangeStrand);
                } else if (strand == eNa_strand_both_rev) {
                    pnt.SetStrand(eNa_strand_minus);
                    ChangeMade(CCleanupChange::eChangeStrand);
                }                
            }
        }
        break;
    case CSeq_loc::e_Mix :
        typedef CSeq_loc::TMix::Tdata TMixList;
        // delete Null type Seq-locs from beginning and end of Mix list.
        TMixList& sl_list = sl.SetMix().Set();
        TMixList::iterator sl_it = sl_list.begin();
        while (sl_it != sl_list.end()) {
            if ((*sl_it)->IsNull()) {
                sl_it = sl_list.erase(sl_it);
                ChangeMade(CCleanupChange::eChangeSeqloc);
            } else {
                break;
            }
        }
        sl_it = sl_list.end();
        while (sl_it != sl_list.begin()) {
            --sl_it;
            if ( ! (*sl_it)->IsNull()) {
                break;
            }
        }
        ++sl_it;
        if (sl_it != sl_list.end()) {
            sl_list.erase(sl_it, sl_list.end());
            ChangeMade(CCleanupChange::eChangeSeqloc);            
        }

        NON_CONST_ITERATE(TMixList, sl_it, sl_list) {
            BasicCleanup(**sl_it);
        }
            
        if (sl_list.size() == 1) {
            CRef<CSeq_loc> only_sl = sl_list.front();
            sl.Assign(*only_sl);
            ChangeMade(CCleanupChange::eChangeSeqloc);
        }
        break;
    }
}


void CCleanup_imp::BasicCleanup(CSeq_interval& si)
{
    // Fix backwards intervals
    if (si.GetFrom() > si.GetTo()) {
        swap(si.SetFrom(), si.SetTo());
        ChangeMade(CCleanupChange::eChangeSeqloc);
    }
    // change bad strand values.
    if (si.CanGetStrand()) {
        ENa_strand strand = si.GetStrand();
        if (strand == eNa_strand_both) {
            si.SetStrand(eNa_strand_plus);
            ChangeMade(CCleanupChange::eChangeStrand);
        } else if (strand == eNa_strand_both_rev) {
            si.SetStrand(eNa_strand_minus);
            ChangeMade(CCleanupChange::eChangeStrand);
        }        
    }
}


// Extended Cleanup Methods
void CCleanup_imp::ExtendedCleanup(CSeq_entry_Handle seh)
{    
    switch (seh.Which()) {
        case CSeq_entry::e_Seq:
            ExtendedCleanup(seh.GetSeq());
            break;
        case CSeq_entry::e_Set:
            ExtendedCleanup(seh.GetSet());
            break;
        case CSeq_entry::e_not_set:
        default:
            break;
    }
}


void CCleanup_imp::ExtendedCleanup(CSeq_submit& ss)
{
    // TODO Cleanup Submit-block.
    
    switch (ss.GetData().Which()) {
        case CSeq_submit::TData::e_Entrys:
            NON_CONST_ITERATE(CSeq_submit::TData::TEntrys, it, ss.SetData().SetEntrys()) {
                CSeq_entry_Handle seh = m_Scope->GetSeq_entryHandle((**it));
                ExtendedCleanup(seh);
            }
            break;
        case CSeq_submit::TData::e_Annots:
            NON_CONST_ITERATE(CSeq_submit::TData::TAnnots, it, ss.SetData().SetAnnots()) {
                ExtendedCleanup(m_Scope->GetSeq_annotHandle(**it));
            }
            break;
        case CSeq_submit::TData::e_Delete:
        case CSeq_submit::TData::e_not_set:
        default:
            break;
    }
}


void CCleanup_imp::ExtendedCleanup(CBioseq_set_Handle bss)
{
    x_RecurseForDescriptors(bss, &ncbi::objects::CCleanup_imp::x_RemoveEmptyGenbankDesc);
    x_RecurseForSeqAnnots(bss, &ncbi::objects::CCleanup_imp::x_ConvertFullLenSourceFeatureToDescriptor);
    x_RecurseForSeqAnnots(bss, &ncbi::objects::CCleanup_imp::x_ConvertFullLenPubFeatureToDescriptor);    
    x_RecurseForSeqAnnots(bss, &ncbi::objects::CCleanup_imp::x_RemoveEmptyFeatures);
    x_MergeAdjacentAnnots(bss);
    x_RecurseForSeqAnnots(bss, &ncbi::objects::CCleanup_imp::x_ChangeImpFeatToCDS);
    x_RecurseForSeqAnnots(bss, &ncbi::objects::CCleanup_imp::x_ChangeImpFeatToProt);
    x_ExtendSingleGeneOnmRNA(bss);
    
    x_RecurseForDescriptors(bss, &ncbi::objects::CCleanup_imp::x_RemoveMultipleTitles);
    x_RecurseForDescriptors(bss, &ncbi::objects::CCleanup_imp::x_MergeMultipleDates);

    x_RecurseForSeqAnnots(bss, &ncbi::objects::CCleanup_imp::x_CorrectExceptText);
    x_RecurseDescriptorsForMerge(bss, &ncbi::objects::CCleanup_imp::x_IsCitSubPub, 
                                      &ncbi::objects::CCleanup_imp::x_CitSubsMatch);
    x_RecurseDescriptorsForMerge(bss, &ncbi::objects::CCleanup_imp::x_IsMergeableBioSource, 
                                      &ncbi::objects::CCleanup_imp::x_MergeDuplicateBioSources);
                                      
                                      
    RemoveEmptyFeaturesDescriptorsAndAnnots(bss);
    x_RecurseForSeqAnnots(bss, &ncbi::objects::CCleanup_imp::x_RemovePseudoProducts);
    
    CSeq_entry_EditHandle seh = bss.GetEditHandle().GetParentEntry();
    RenormalizeNucProtSets(bss);
    
    if (seh.IsSet()) {
        bss = seh.GetSet().GetEditHandle();
        x_ChangeGenBankBlocks (bss.GetParentEntry());
        x_RecurseForDescriptors(bss, &ncbi::objects::CCleanup_imp::x_CleanGenbankBlockStrings);
        x_RecurseForSeqAnnots(bss, &ncbi::objects::CCleanup_imp::x_MoveDbxrefs);
        x_RecurseDescriptorsForMerge(bss, &ncbi::objects::CCleanup_imp::x_IsMergeableBioSource, 
                                      &ncbi::objects::CCleanup_imp::x_MergeDuplicateBioSources);

        x_RecurseForDescriptors(bss, &ncbi::objects::CCleanup_imp::x_MolInfoUpdate);
        x_RecurseForDescriptors(bss, &ncbi::objects::CCleanup_imp::x_RemoveEmptyGenbankDesc);
        x_RecurseForSeqAnnots(bss, &ncbi::objects::CCleanup_imp::x_ConvertFullLenSourceFeatureToDescriptor);
        x_RecurseForSeqAnnots(bss, &ncbi::objects::CCleanup_imp::x_ConvertFullLenPubFeatureToDescriptor);    
        x_RecurseForSeqAnnots(bss, &ncbi::objects::CCleanup_imp::x_RemoveEmptyFeatures);
        x_MergeAdjacentAnnots(bss);
    
        x_RecurseForSeqAnnots(bss, &ncbi::objects::CCleanup_imp::x_CheckCodingRegionEnds);
    
        x_RecurseForSeqAnnots(bss, &ncbi::objects::CCleanup_imp::x_RemoveUnnecessaryGeneXrefs);
    } else {
        CBioseq_EditHandle bsh = seh.GetSeq().GetEditHandle();
        x_RecurseForDescriptors(bsh, &ncbi::objects::CCleanup_imp::x_CleanGenbankBlockStrings);
        x_ChangeGenBankBlocks (bsh.GetParentEntry());
        x_RecurseForSeqAnnots(bsh, &ncbi::objects::CCleanup_imp::x_MoveDbxrefs);
        x_RecurseDescriptorsForMerge(bsh, &ncbi::objects::CCleanup_imp::x_IsMergeableBioSource, 
                                      &ncbi::objects::CCleanup_imp::x_MergeDuplicateBioSources);

        x_RecurseForDescriptors(bsh, &ncbi::objects::CCleanup_imp::x_MolInfoUpdate);
        x_RecurseForDescriptors(bsh, &ncbi::objects::CCleanup_imp::x_RemoveEmptyGenbankDesc);
        x_RecurseForSeqAnnots(bsh, &ncbi::objects::CCleanup_imp::x_ConvertFullLenSourceFeatureToDescriptor);
        x_RecurseForSeqAnnots(bsh, &ncbi::objects::CCleanup_imp::x_ConvertFullLenPubFeatureToDescriptor);    
        x_RecurseForSeqAnnots(bsh, &ncbi::objects::CCleanup_imp::x_RemoveEmptyFeatures);
        x_MergeAdjacentAnnots(bsh);
    
        x_RecurseForSeqAnnots(bsh, &ncbi::objects::CCleanup_imp::x_CheckCodingRegionEnds);
    
        x_RecurseForSeqAnnots(bsh, &ncbi::objects::CCleanup_imp::x_RemoveUnnecessaryGeneXrefs);
    }
}



void CCleanup_imp::ExtendedCleanup(CBioseq_Handle bsh)
{
    x_RecurseForDescriptors(bsh, &ncbi::objects::CCleanup_imp::x_RemoveEmptyGenbankDesc);
    x_RecurseForSeqAnnots(bsh, &ncbi::objects::CCleanup_imp::x_ConvertFullLenSourceFeatureToDescriptor);
    x_RecurseForSeqAnnots(bsh, &ncbi::objects::CCleanup_imp::x_ConvertFullLenPubFeatureToDescriptor);    
    x_RecurseForSeqAnnots(bsh, &ncbi::objects::CCleanup_imp::x_RemoveEmptyFeatures); 
    x_MergeAdjacentAnnots(bsh);
    x_RecurseForSeqAnnots(bsh, &ncbi::objects::CCleanup_imp::x_ChangeImpFeatToCDS);
    x_RecurseForSeqAnnots(bsh, &ncbi::objects::CCleanup_imp::x_ChangeImpFeatToProt);
    x_ExtendSingleGeneOnmRNA(bsh);
    
    x_RecurseForDescriptors(bsh, &ncbi::objects::CCleanup_imp::x_RemoveMultipleTitles);
    x_RecurseForDescriptors(bsh, &ncbi::objects::CCleanup_imp::x_MergeMultipleDates);

    x_RecurseForSeqAnnots(bsh, &ncbi::objects::CCleanup_imp::x_CorrectExceptText);
    
    x_RecurseDescriptorsForMerge(bsh, &ncbi::objects::CCleanup_imp::x_IsCitSubPub, 
                                      &ncbi::objects::CCleanup_imp::x_CitSubsMatch);
    x_RecurseDescriptorsForMerge(bsh, &ncbi::objects::CCleanup_imp::x_IsMergeableBioSource, 
                                      &ncbi::objects::CCleanup_imp::x_MergeDuplicateBioSources);
    RemoveEmptyFeaturesDescriptorsAndAnnots(bsh);
    
    x_ChangeGenBankBlocks (bsh.GetParentEntry());
    x_RecurseForDescriptors(bsh, &ncbi::objects::CCleanup_imp::x_CleanGenbankBlockStrings);
    x_RecurseForSeqAnnots(bsh, &ncbi::objects::CCleanup_imp::x_MoveDbxrefs);
    x_RecurseDescriptorsForMerge(bsh, &ncbi::objects::CCleanup_imp::x_IsMergeableBioSource, 
                                      &ncbi::objects::CCleanup_imp::x_MergeDuplicateBioSources);

    x_RecurseForDescriptors(bsh, &ncbi::objects::CCleanup_imp::x_MolInfoUpdate);
    x_RecurseForDescriptors(bsh, &ncbi::objects::CCleanup_imp::x_RemoveEmptyGenbankDesc);
    x_RecurseForSeqAnnots(bsh, &ncbi::objects::CCleanup_imp::x_ConvertFullLenSourceFeatureToDescriptor);
    x_RecurseForSeqAnnots(bsh, &ncbi::objects::CCleanup_imp::x_ConvertFullLenPubFeatureToDescriptor);    
    x_RecurseForSeqAnnots(bsh, &ncbi::objects::CCleanup_imp::x_RemoveEmptyFeatures); 
    x_MergeAdjacentAnnots(bsh);
    
    x_RecurseForSeqAnnots(bsh, &ncbi::objects::CCleanup_imp::x_CheckCodingRegionEnds);        

    x_RecurseForSeqAnnots(bsh, &ncbi::objects::CCleanup_imp::x_RemoveUnnecessaryGeneXrefs);
}


void CCleanup_imp::ExtendedCleanup(CSeq_annot_Handle sa)
{
    x_ConvertFullLenSourceFeatureToDescriptor(sa);
    x_ConvertFullLenPubFeatureToDescriptor(sa);    
    x_RemoveEmptyFeatures(sa);

    x_ChangeImpFeatToCDS(sa);
    x_ChangeImpFeatToProt(sa);
   
    x_CorrectExceptText(sa);
    
    x_RemovePseudoProducts(sa);   
    
    x_MoveDbxrefs(sa);
 
    x_CheckCodingRegionEnds(sa);
    
    x_RemoveUnnecessaryGeneXrefs (sa);
}


void CCleanup_imp::x_RecurseForSeqAnnots (CBioseq_Handle bs, RecurseSeqAnnot pmf)
{
    CBioseq_EditHandle bseh = bs.GetEditHandle();
    
    CSeq_annot_CI annot_it(bseh.GetSeq_entry_Handle(), CSeq_annot_CI::eSearch_entry);
    for(; annot_it; ++annot_it) {
        (this->*pmf)(*annot_it);
    }   
}


void CCleanup_imp::x_RecurseForSeqAnnots (CBioseq_set_Handle bss, RecurseSeqAnnot pmf)
{
    CBioseq_set_EditHandle bseh = bss.GetEditHandle();
    
    CSeq_annot_CI annot_it(bseh.GetParentEntry(), CSeq_annot_CI::eSearch_entry);
    for(; annot_it; ++annot_it) {
        (this->*pmf)(*annot_it);
    }   


    // now operate on members of set
    if (bss.GetCompleteBioseq_set()->IsSetSeq_set()) {
       CConstRef<CBioseq_set> b = bss.GetCompleteBioseq_set();
       list< CRef< CSeq_entry > > set = (*b).GetSeq_set();
       
       ITERATE (list< CRef< CSeq_entry > >, it, set) {
            switch ((**it).Which()) {
                case CSeq_entry::e_Seq:
                    x_RecurseForSeqAnnots(m_Scope->GetBioseqHandle((**it).GetSeq()), pmf);
                    break;
                case CSeq_entry::e_Set:
                    x_RecurseForSeqAnnots(m_Scope->GetBioseq_setHandle((**it).GetSet()), pmf);
                    break;
                case CSeq_entry::e_not_set:
                default:
                    break;
            }
        }
    }
}


bool IsEmpty (CGene_ref& gene_ref)
{
    if ((!gene_ref.CanGetLocus() || NStr::IsBlank (gene_ref.GetLocus()))
        && (!gene_ref.CanGetAllele() || NStr::IsBlank (gene_ref.GetAllele()))
        && (!gene_ref.CanGetDesc() || NStr::IsBlank (gene_ref.GetDesc()))
        && (!gene_ref.CanGetMaploc() || NStr::IsBlank (gene_ref.GetMaploc()))
        && (!gene_ref.CanGetLocus_tag() || NStr::IsBlank (gene_ref.GetLocus_tag()))
        && (!gene_ref.CanGetDb() || gene_ref.GetDb().size() == 0)
        && (!gene_ref.CanGetSyn() || gene_ref.GetSyn().size() == 0)) {
        return true;
    } else {
        return false;
    }   
}


bool IsEmpty (CSeq_feat& sf)
{
    if (sf.CanGetData() && sf.SetData().IsGene()) {
        if (IsEmpty(sf.SetData().SetGene())) {
            if (!sf.CanGetComment() || NStr::IsBlank (sf.GetComment())) {
                return true;
            } else {
                // convert this feature to a misc_feat
                sf.SetData().Reset();
                sf.SetData().SetImp().SetKey("misc_feature");
                sf.SetData().InvalidateSubtype();
            }
        }
    } else if (sf.CanGetData() && sf.SetData().IsProt()) {
        CProt_ref& prot = sf.SetData().SetProt();
        if (prot.CanGetProcessed()) {
            unsigned int processed = sf.SetData().GetProt().GetProcessed();
            
            if (processed != CProt_ref::eProcessed_signal_peptide
                && processed != CProt_ref::eProcessed_transit_peptide
                && (!prot.CanGetName() || prot.GetName().size() == 0)
                && sf.CanGetComment()
                && NStr::StartsWith (sf.GetComment(), "putative")) {
                prot.SetName ().push_back(sf.GetComment());
                sf.ResetComment();
            }

            if (processed == CProt_ref::eProcessed_mature && (!prot.CanGetName() || prot.GetName().size() == 0)) {
                prot.SetName().push_back("unnamed");
            }
            
            if (processed != CProt_ref::eProcessed_signal_peptide
                && processed != CProt_ref::eProcessed_transit_peptide
                && (!prot.CanGetName() 
                    || prot.GetName().size() == 0 
                    || NStr::IsBlank(prot.GetName().front()))
                && (!prot.CanGetDesc() || NStr::IsBlank(prot.GetDesc()))
                && (!prot.CanGetEc() || prot.GetEc().size() == 0)
                && (!prot.CanGetActivity() || prot.GetActivity().size() == 0)
                && (!prot.CanGetDb() || prot.GetDb().size() == 0)) {
                return true;
            }
        }
    } else if (sf.CanGetData() && sf.SetData().IsComment()) {
        if (!sf.CanGetComment() || NStr::IsBlank (sf.GetComment())) {
            return true;
        }
    }
    return false;
}


void CCleanup_imp::x_ExtendedCleanStrings (CSeqdesc& sd)
{  
    switch (sd.Which()) {
        case CSeqdesc::e_Mol_type:
        case CSeqdesc::e_Method:
        case CSeqdesc::e_Modif:
        case CSeqdesc::e_Num:
        case CSeqdesc::e_Maploc:
        case CSeqdesc::e_Pir:
        case CSeqdesc::e_User:
        case CSeqdesc::e_Sp:
        case CSeqdesc::e_Dbxref:
        case CSeqdesc::e_Embl:
        case CSeqdesc::e_Create_date:
        case CSeqdesc::e_Update_date:
        case CSeqdesc::e_Prf:
        case CSeqdesc::e_Pdb:
        case CSeqdesc::e_Het:
        case CSeqdesc::e_Molinfo:
            // nothing to clean
            return;
            break;
        case CSeqdesc::e_Name:
            CleanVisString(sd.SetName());
            break;
        case CSeqdesc::e_Title:
            CleanVisString(sd.SetTitle());
            break;
        case CSeqdesc::e_Org:
            x_ExtendedCleanStrings (sd.SetOrg());
            break;
        case CSeqdesc::e_Genbank:
            x_CleanGenbankBlockStrings(sd.SetGenbank());
            break;
        case CSeqdesc::e_Region:
            CleanVisString(sd.SetRegion());
            break;
        case CSeqdesc::e_Comment:
            TrimSpacesAndJunkFromEnds(sd.SetComment(), true);
            if (OnlyPunctuation (sd.SetComment())) {
                sd.SetComment("");
            }
            break;
        case CSeqdesc::e_Pub:
            if (sd.GetPub().CanGetComment()) {
                if (IsOnlinePub(sd.GetPub())) {
                    NStr::TruncateSpacesInPlace (sd.SetPub().SetComment(), NStr::eTrunc_Both);
                } else {
                    CleanVisString(sd.SetPub().SetComment());
                }
                if (NStr::IsBlank(sd.GetPub().GetComment())) {
                    sd.SetPub().ResetComment();
                }
            }
            break;
        case CSeqdesc::e_Source:
            x_ExtendedCleanSubSourceList (sd.SetSource());
            if (sd.SetSource().CanGetOrg()) {
                x_ExtendedCleanStrings (sd.SetSource().SetOrg());
            }
            break;
        default:
            break;                     
    }
    
}


void CCleanup_imp::x_ExtendedCleanStrings (CSeq_descr& sdr)
{
    CSeq_descr::Tdata& current_list = sdr.Set();
    NON_CONST_ITERATE (CSeq_descr::Tdata, it, sdr.Set()) {
        x_ExtendedCleanStrings(**it);
        // NOTE: At this point in the C Toolkit, we check for empty
        // descriptors and remove them.
        // We need a definition for "emptiness" for each of the
        // descriptor types.
    }
}


void CCleanup_imp::x_ExtendedCleanStrings (CSeq_annot_Handle sah)
{
    if (sah.IsFtable()) {
        CFeat_CI feat_ci(sah);
        while (feat_ci) {
            x_ExtendedCleanStrings (const_cast< CSeq_feat& > (feat_ci->GetOriginalFeature()));
            // NOTE: At this point in the C Toolkit, we check for empty features and remove them.
            // We need a definition for "emptiness" for each of the feature types.
            ++feat_ci;
        }
    }        
}

// Was GetRidOfEmptyFeatsDescCallback in the C Toolkit
void CCleanup_imp::RemoveEmptyFeaturesDescriptorsAndAnnots (CBioseq_Handle bs)
{
    CBioseq_EditHandle bseh = bs.GetEditHandle();
    
    // combine adjacent annotations
    vector<CSeq_annot_EditHandle> sah; // copy annot handles to not to break iterator while moving
    CSeq_annot_CI annot_it(bseh.GetSeq_entry_Handle(), CSeq_annot_CI::eSearch_entry);
    for(; annot_it; ++annot_it) {
        x_ExtendedCleanStrings (*annot_it);
        if ((*annot_it).IsFtable()) {
            CFeat_CI feat_ci((*annot_it));
            if (!feat_ci) {
                // add annot_it to list of annotations to remove
                sah.push_back((*annot_it).GetEditHandle());
            }
        }
    }
    // move annots from one place to another without copying their contents
    ITERATE ( vector<CSeq_annot_EditHandle>, it, sah ) {
        (*it).Remove();
    }
    
    x_ExtendedCleanStrings (bseh.SetDescr());
    
}


void CCleanup_imp::RemoveEmptyFeaturesDescriptorsAndAnnots (CBioseq_set_Handle bs)
{
    CBioseq_set_EditHandle bseh = bs.GetEditHandle();
    
    // combine adjacent annotations
    vector<CSeq_annot_EditHandle> sah; // copy annot handles to not to break iterator while moving
    CSeq_annot_CI annot_it(bseh.GetParentEntry(), CSeq_annot_CI::eSearch_entry);
    for(; annot_it; ++annot_it) {
        x_ExtendedCleanStrings (*annot_it);
        if ((*annot_it).IsFtable()) {
            CFeat_CI feat_ci((*annot_it));
            if (!feat_ci) {
                // add annot_it to list of annotations to remove
                sah.push_back((*annot_it).GetEditHandle());
            }
        }
    }
    // move annots from one place to another without copying their contents
    ITERATE ( vector<CSeq_annot_EditHandle>, it, sah ) {
        (*it).Remove();
    }
    
    x_ExtendedCleanStrings (bseh.SetDescr());

    if (bs.GetCompleteBioseq_set()->IsSetSeq_set()) {
       CConstRef<CBioseq_set> b = bs.GetCompleteBioseq_set();
       list< CRef< CSeq_entry > > set = (*b).GetSeq_set();
       
       ITERATE (list< CRef< CSeq_entry > >, it, set) {
            switch ((**it).Which()) {
                case CSeq_entry::e_Seq:
                    RemoveEmptyFeaturesDescriptorsAndAnnots(m_Scope->GetBioseqHandle((**it).GetSeq()));
                    break;
                case CSeq_entry::e_Set:
                    RemoveEmptyFeaturesDescriptorsAndAnnots(m_Scope->GetBioseq_setHandle((**it).GetSet()));
                    break;
                case CSeq_entry::e_not_set:
                default:
                    break;
            }
        }
    }

}


void CCleanup_imp::x_RemoveEmptyFeatures (CSeq_annot_Handle sa) 
{
    if (sa.IsFtable()) {
        CFeat_CI feat_ci(sa);
        while (feat_ci) {
            const CSeq_feat &cf = feat_ci->GetOriginalFeature();
            if (IsEmpty(const_cast<CSeq_feat &>(cf))) {
                feat_ci->GetSeq_feat_Handle().Remove();
            }
            ++feat_ci;
        }    
    }
}


// combine CSeq_annot objects if two adjacent objects in the list are both
// feature table annotations, both have no id, no name, no db, and no description
// Was MergeAdjacentAnnotsCallback in C Toolkit

void CCleanup_imp::x_MergeAdjacentAnnots (CBioseq_Handle bs)
{
    CBioseq_EditHandle bseh = bs.GetEditHandle();
    
    // combine adjacent annotations
    vector<CSeq_annot_EditHandle> sah; // copy annot handles to not to break iterator while moving
    CSeq_annot_EditHandle prev;
    bool prev_can_merge = false;
    CSeq_annot_CI annot_it(bseh.GetSeq_entry_Handle(), CSeq_annot_CI::eSearch_entry);
    for(; annot_it; ++annot_it) {
        if ((*annot_it).IsFtable()
            && (! (*annot_it).Seq_annot_CanGetId() 
                || (*annot_it).Seq_annot_GetId().size() == 0)
            && (! (*annot_it).Seq_annot_CanGetName() 
                || NStr::IsBlank((*annot_it).Seq_annot_GetName()))
            && (! (*annot_it).Seq_annot_CanGetDb()
                || (*annot_it).Seq_annot_GetDb() == 0)
            && (! (*annot_it).Seq_annot_CanGetDesc()
                || (*annot_it).Seq_annot_GetDesc().Get().size() == 0)) {
            if (prev_can_merge) {
                // merge features from annot_it into prev
                CFeat_CI feat_ci((*annot_it));
                while (feat_ci) {
                    prev.AddFeat(feat_ci->GetOriginalFeature());
                    ++feat_ci;
                }
                // add annot_it to list of annotations to remove
                sah.push_back((*annot_it).GetEditHandle());
            } else {
                prev = (*annot_it).GetEditHandle();
            }
            prev_can_merge = true;
        } else {
            prev_can_merge = false;
        }
    }
    // move annots from one place to another without copying their contents
    ITERATE ( vector<CSeq_annot_EditHandle>, it, sah ) {
        (*it).Remove();
    }
   
}


void CCleanup_imp::x_MergeAdjacentAnnots (CBioseq_set_Handle bs)
{
    CBioseq_set_EditHandle bseh = bs.GetEditHandle();
    
    // combine adjacent annotations
    vector<CSeq_annot_EditHandle> sah; // copy annot handles to not to break iterator while moving
    CSeq_annot_EditHandle prev;
    bool prev_can_merge = false;
    CSeq_annot_CI annot_it(bseh.GetParentEntry(), CSeq_annot_CI::eSearch_entry);
    for(; annot_it; ++annot_it) {
        if ((*annot_it).IsFtable()
            && (! (*annot_it).Seq_annot_CanGetId() 
                || (*annot_it).Seq_annot_GetId().size() == 0)
            && (! (*annot_it).Seq_annot_CanGetName() 
                || NStr::IsBlank((*annot_it).Seq_annot_GetName()))
            && (! (*annot_it).Seq_annot_CanGetDb()
                || (*annot_it).Seq_annot_GetDb() == 0)
            && (! (*annot_it).Seq_annot_CanGetDesc()
                || (*annot_it).Seq_annot_GetDesc().Get().size() == 0)) {
            if (prev_can_merge) {
                // merge features from annot_it into prev
                CFeat_CI feat_ci((*annot_it));
                while (feat_ci) {
                    prev.AddFeat(feat_ci->GetOriginalFeature());
                    ++feat_ci;
                }
                // add annot_it to list of annotations to remove
                sah.push_back((*annot_it).GetEditHandle());
            } else {
                prev = (*annot_it).GetEditHandle();
            }
            prev_can_merge = true;
        } else {
            prev_can_merge = false;
        }
    }
    // move annots from one place to another without copying their contents
    ITERATE ( vector<CSeq_annot_EditHandle>, it, sah ) {
        (*it).Remove();
    }

    // now operate on members of set
    if (bs.GetCompleteBioseq_set()->IsSetSeq_set()) {
       CConstRef<CBioseq_set> b = bs.GetCompleteBioseq_set();
       list< CRef< CSeq_entry > > set = (*b).GetSeq_set();
       
       ITERATE (list< CRef< CSeq_entry > >, it, set) {
            switch ((**it).Which()) {
                case CSeq_entry::e_Seq:
                    x_MergeAdjacentAnnots(m_Scope->GetBioseqHandle((**it).GetSeq()));
                    break;
                case CSeq_entry::e_Set:
                    x_MergeAdjacentAnnots(m_Scope->GetBioseq_setHandle((**it).GetSet()));
                    break;
                case CSeq_entry::e_not_set:
                default:
                    break;
            }
        }
    }
   
}


// If the feature location is exactly the entire length of the Bioseq that the feature is 
// located on, convert the feature to a descriptor on the same Bioseq.
// Was ConvertFullLenSourceFeatToDesc in C Toolkit
void CCleanup_imp::x_ConvertFullLenFeatureToDescriptor (CSeq_annot_Handle sa, CSeqFeatData::E_Choice choice)
{
    if (sa.IsFtable()) {
        CFeat_CI feat_ci(sa);
        while (feat_ci) {
            const CSeq_feat& cf = feat_ci->GetOriginalFeature();
            if (cf.CanGetData() && cf.GetData().Which()  == choice) {
                // Create a location that covers the entire sequence and do
                // a comparison.  Can't just check for the location type 
                // of the feature to be "whole" because an interval could
                // start at 0 and end at the end of the Bioseq.
                CRef<CSeq_loc> loc(new CSeq_loc);
                loc->SetWhole().Assign(*(cf.GetLocation().GetId()));
                if (sequence::Compare(*loc, cf.GetLocation(), m_Scope) == sequence::eSame) {
                    CBioseq_Handle bh = m_Scope->GetBioseqHandle(cf.GetLocation());                
                    CRef<CSeqdesc> desc(new CSeqdesc);
             
                    if (choice == CSeqFeatData::e_Biosrc) {
                        desc->Select(CSeqdesc::e_Source);
                        desc->SetSource(const_cast< CBioSource& >(cf.GetData().GetBiosrc()));
                    } else if (choice == CSeqFeatData::e_Pub) {
                        desc->Select(CSeqdesc::e_Pub);
                        desc->SetPub(const_cast< CPubdesc& >(cf.GetData().GetPub()));
                    }
             
                    CBioseq_EditHandle eh = bh.GetEditHandle();
                    eh.AddSeqdesc(*desc);
                    (*feat_ci).GetSeq_feat_Handle().Remove();
                }
            }
            ++feat_ci;                
        }
    }
}


void CCleanup_imp::x_ConvertFullLenSourceFeatureToDescriptor (CSeq_annot_Handle sah)
{
    x_ConvertFullLenFeatureToDescriptor(sah, CSeqFeatData::e_Biosrc);
}


void CCleanup_imp::x_ConvertFullLenPubFeatureToDescriptor (CSeq_annot_Handle sah)
{
    x_ConvertFullLenFeatureToDescriptor(sah, CSeqFeatData::e_Pub);
}


// collapse nuc-prot sets with only one seq-entry
void CCleanup_imp::RenormalizeNucProtSets (CBioseq_set_Handle bsh)
{
    CConstRef<CBioseq_set> b = bsh.GetCompleteBioseq_set();
    if (bsh.CanGetClass() && b->IsSetSeq_set()) {
        unsigned int bclass = bsh.GetClass();

        list< CRef< CSeq_entry > > set = (*b).GetSeq_set();

        if (bclass == CBioseq_set::eClass_genbank
            || bclass == CBioseq_set::eClass_mut_set
            || bclass == CBioseq_set::eClass_pop_set
            || bclass == CBioseq_set::eClass_phy_set
            || bclass == CBioseq_set::eClass_eco_set
            || bclass == CBioseq_set::eClass_wgs_set
            || bclass == CBioseq_set::eClass_gen_prod_set) {
       
            ITERATE (list< CRef< CSeq_entry > >, it, set) {
                if ((**it).Which() == CSeq_entry::e_Set) {
                    RenormalizeNucProtSets (m_Scope->GetBioseq_setHandle((**it).GetSet()));
                }
            }
        } else if (bclass == CBioseq_set::eClass_nuc_prot && set.size() == 1) {
            // collapse nuc-prot set
            CSeq_entry_EditHandle seh = bsh.GetEditHandle().GetParentEntry();
            
            seh.CollapseSet();
            if (seh.IsSetDescr()) {
                CSeq_descr::Tdata remove_list;

                remove_list.clear();
                
                if (seh.IsSeq()) {
                    CBioseq_EditHandle eh = seh.GetSeq().GetEditHandle();
                    x_RemoveMultipleTitles(eh.SetDescr(), remove_list);
                    for (CSeq_descr::Tdata::iterator it1 = remove_list.begin();
                        it1 != remove_list.end(); ++it1) { 
                        eh.RemoveSeqdesc(**it1);
                    }        
                } else if (seh.IsSet()) {
                    CBioseq_set_EditHandle eh = seh.GetSet().GetEditHandle();
                    x_RemoveMultipleTitles(eh.SetDescr(), remove_list);
                    for (CSeq_descr::Tdata::iterator it1 = remove_list.begin();
                        it1 != remove_list.end(); ++it1) { 
                        eh.RemoveSeqdesc(**it1);
                    }        
                }
            }
        }
    }
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
 * ===========================================================================
 *
 * $Log$
 * Revision 1.43  2006/08/22 12:10:50  rsmith
 * Better Seqdesc:Comment and Seqloc cleanup.
 *
 * Revision 1.42  2006/08/03 18:13:46  rsmith
 * BasicCleanup(CSeq_loc)
 *
 * Revision 1.41  2006/08/03 12:04:01  bollin
 * moved descriptor extended cleanup methods to cleanup_desc.cpp
 *
 * Revision 1.40  2006/07/27 19:02:58  rsmith
 * Handle versions of Basic cleanup call non-handle stuff as well.
 *
 * Revision 1.39  2006/07/26 19:37:50  rsmith
 * add cleanup w/handles and reporting
 *
 * Revision 1.38  2006/07/26 17:12:41  bollin
 * added method to remove redundant genbank block information
 *
 * Revision 1.37  2006/07/25 20:07:13  bollin
 * added step to ExtendedCleanup to remove unnecessary gene xrefs
 *
 * Revision 1.36  2006/07/25 16:51:23  bollin
 * fixed bug in x_RemovePseudoProducts
 * implemented more efficient method for removing descriptors
 *
 * Revision 1.35  2006/07/25 14:36:47  bollin
 * added method to ExtendedCleanup to remove products on coding regions marked
 * as pseudo.
 *
 * Revision 1.34  2006/07/18 16:43:43  bollin
 * added x_RecurseDescriptorsForMerge and changed the ExtendedCleanup functions
 * for merging duplicate BioSources and equivalent CitSubs to use the new function
 *
 * Revision 1.33  2006/07/13 17:10:56  rsmith
 * report if changes made
 *
 * Revision 1.32  2006/07/11 14:38:28  bollin
 * aadded a step to ExtendedCleanup to extend the only gene found on the only
 * mRNA sequence in the set where there are zero or one coding region features
 * in the set so that the gene covers the entire mRNA sequence
 *
 * Revision 1.31  2006/07/10 19:01:57  bollin
 * added step to extend coding region to cover missing portion of a stop codon,
 * will also adjust the location of the overlapping gene if necessary.
 *
 * Revision 1.30  2006/07/06 17:28:48  bollin
 * corrected bug in RenormalizeNucProtSets
 *
 * Revision 1.29  2006/07/06 17:16:37  bollin
 * fixed bug in RenormalizeNucProtSets
 *
 * Revision 1.28  2006/07/06 15:16:04  bollin
 * do not create unassigned publication comments
 *
 * Revision 1.27  2006/07/05 16:43:34  bollin
 * added step to ExtendedCleanup to clean features and descriptors
 * and remove empty feature table seq-annots
 *
 * Revision 1.26  2006/07/03 18:45:00  bollin
 * changed methods in ExtendedCleanup for correcting exception text and moving
 * dbxrefs to use edit handles
 *
 * Revision 1.25  2006/07/03 17:48:04  bollin
 * changed RemoveEmptyFeatures method to use edit handles
 *
 * Revision 1.24  2006/07/03 17:02:32  bollin
 * converted steps for ExtendedCleanup that convert full length pub and source
 * features to descriptors to use edit handles
 *
 * Revision 1.23  2006/07/03 15:27:38  bollin
 * rewrote x_MergeAddjacentAnnots to use edit handles
 *
 * Revision 1.22  2006/07/03 14:51:11  bollin
 * corrected compiler errors
 *
 * Revision 1.21  2006/07/03 12:33:48  bollin
 * use edit handles instead of operating directly on the data
 * added step to renormalize nuc-prot sets to ExtendedCleanup
 *
 * Revision 1.20  2006/06/28 15:23:03  bollin
 * added step to move db_xref GenBank Qualifiers to real dbxrefs to Extended Cleanup
 *
 * Revision 1.19  2006/06/28 13:22:39  bollin
 * added step to merge duplicate biosources to ExtendedCleanup
 *
 * Revision 1.18  2006/06/27 18:43:02  bollin
 * added step for merging equivalent cit-sub publications to ExtendedCleanup
 *
 * Revision 1.17  2006/06/27 14:30:59  bollin
 * added step for correcting exception text to ExtendedCleanup
 *
 * Revision 1.16  2006/06/27 13:18:47  bollin
 * added steps for converting full length pubs and sources to descriptors to
 * Extended Cleanup
 *
 * Revision 1.15  2006/06/22 18:16:01  bollin
 * added step to merge adjacent Seq-Annots to ExtendedCleanup
 *
 * Revision 1.14  2006/06/22 17:47:52  ucko
 * Correctly capitalize Date.hpp.
 *
 * Revision 1.13  2006/06/22 17:29:15  bollin
 * added method to merge multiple create dates to Extended Cleanup
 *
 * Revision 1.12  2006/06/22 15:39:00  bollin
 * added step for removing multiple titles to Extended Cleanup
 *
 * Revision 1.11  2006/06/22 13:59:58  bollin
 * provide explicit conversion from EGIBB_mol to CMolInfo::EBiomol
 * removes compiler warnings
 *
 * Revision 1.10  2006/06/22 13:28:29  bollin
 * added step to remove empty features to ExtendedCleanup
 *
 * Revision 1.9  2006/06/21 17:21:28  bollin
 * added cleanup of GenbankBlock descriptor strings to ExtendedCleanup
 *
 * Revision 1.8  2006/06/21 14:12:59  bollin
 * added step for removing empty GenBank block descriptors to ExtendedCleanup
 *
 * Revision 1.7  2006/06/20 19:43:39  bollin
 * added MolInfoUpdate to ExtendedCleanup
 *
 * Revision 1.6  2006/05/17 17:39:36  bollin
 * added parsing and cleanup of anticodon qualifiers on tRNA features and
 * transl_except qualifiers on coding region features
 *
 * Revision 1.5  2006/04/18 14:32:36  rsmith
 * refactoring
 *
 * Revision 1.4  2006/04/17 17:03:12  rsmith
 * Refactoring. Get rid of cleanup-mode parameters.
 *
 * Revision 1.3  2006/03/29 16:33:43  rsmith
 * Cleanup strings in Seqdesc. Move Pub functions to cleanup_pub.cpp
 *
 * Revision 1.2  2006/03/20 14:22:15  rsmith
 * add cleanup for CSeq_submit
 *
 * Revision 1.1  2006/03/14 20:21:50  rsmith
 * Move BasicCleanup functionality from objects to objtools/cleanup
 *
 *
 * ===========================================================================
 */

