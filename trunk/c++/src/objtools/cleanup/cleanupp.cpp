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



static void s_SeqDescCommentCleanup( CSeqdesc::TComment& comment )
{
    //  Remove trailing periods:
    if ( ! comment.empty() && (comment[ comment.size() -1 ] == '.') ) {
        comment = comment.substr( 0, comment.size() -1 );
    }
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
            CleanString( sd.SetName() );
            break;
        case CSeqdesc::e_Title:
            CleanString( sd.SetTitle() );
            break;
        case CSeqdesc::e_Org:
            BasicCleanup(sd.SetOrg() );
            break;
        case CSeqdesc::e_Comment:
            s_SeqDescCommentCleanup( sd.SetComment() );
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
            CleanString( sd.SetRegion() );
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
    
    x_RecurseForDescriptors(bss, &ncbi::objects::CCleanup_imp::x_RemoveMultipleTitles);
    x_RecurseForDescriptors(bss, &ncbi::objects::CCleanup_imp::x_MergeMultipleDates);

    x_RecurseForSeqAnnots(bss, &ncbi::objects::CCleanup_imp::x_CorrectExceptText);
    x_RecurseForDescriptors(bss, &ncbi::objects::CCleanup_imp::x_MergeEquivalentCitSubs);
    x_RecurseForDescriptors(bss, &ncbi::objects::CCleanup_imp::x_MergeDuplicateBioSources);
    RemoveEmptyFeaturesDescriptorsAndAnnots(bss);
    
    RenormalizeNucProtSets(bss);
    
    x_RecurseForDescriptors(bss, &ncbi::objects::CCleanup_imp::x_CleanGenbankBlockStrings);

    x_RecurseForSeqAnnots(bss, &ncbi::objects::CCleanup_imp::x_MoveDbxrefs);
    x_RecurseForDescriptors(bss, &ncbi::objects::CCleanup_imp::x_MergeDuplicateBioSources);

    x_RecurseForDescriptors(bss, &ncbi::objects::CCleanup_imp::x_MolInfoUpdate);
    x_RecurseForDescriptors(bss, &ncbi::objects::CCleanup_imp::x_RemoveEmptyGenbankDesc);
    x_RecurseForSeqAnnots(bss, &ncbi::objects::CCleanup_imp::x_ConvertFullLenSourceFeatureToDescriptor);
    x_RecurseForSeqAnnots(bss, &ncbi::objects::CCleanup_imp::x_ConvertFullLenPubFeatureToDescriptor);    
    x_RecurseForSeqAnnots(bss, &ncbi::objects::CCleanup_imp::x_RemoveEmptyFeatures);
    x_MergeAdjacentAnnots(bss);
}



void CCleanup_imp::ExtendedCleanup(CBioseq_Handle bsh)
{
    x_RecurseForDescriptors(bsh, &ncbi::objects::CCleanup_imp::x_RemoveEmptyGenbankDesc);
    x_RecurseForSeqAnnots(bsh, &ncbi::objects::CCleanup_imp::x_ConvertFullLenSourceFeatureToDescriptor);
    x_RecurseForSeqAnnots(bsh, &ncbi::objects::CCleanup_imp::x_ConvertFullLenPubFeatureToDescriptor);    
    x_RecurseForSeqAnnots(bsh, &ncbi::objects::CCleanup_imp::x_RemoveEmptyFeatures); 
    x_MergeAdjacentAnnots(bsh);
    x_RecurseForDescriptors(bsh, &ncbi::objects::CCleanup_imp::x_RemoveMultipleTitles);
    x_RecurseForDescriptors(bsh, &ncbi::objects::CCleanup_imp::x_MergeMultipleDates);

    x_RecurseForSeqAnnots(bsh, &ncbi::objects::CCleanup_imp::x_CorrectExceptText);
    
    x_RecurseForDescriptors(bsh, &ncbi::objects::CCleanup_imp::x_MergeEquivalentCitSubs);
    x_RecurseForDescriptors(bsh, &ncbi::objects::CCleanup_imp::x_MergeDuplicateBioSources);
    RemoveEmptyFeaturesDescriptorsAndAnnots(bsh);
    
    x_RecurseForDescriptors(bsh, &ncbi::objects::CCleanup_imp::x_CleanGenbankBlockStrings);

    x_RecurseForSeqAnnots(bsh, &ncbi::objects::CCleanup_imp::x_MoveDbxrefs);
    x_RecurseForDescriptors(bsh, &ncbi::objects::CCleanup_imp::x_MergeDuplicateBioSources);

    x_RecurseForDescriptors(bsh, &ncbi::objects::CCleanup_imp::x_MolInfoUpdate);
    x_RecurseForDescriptors(bsh, &ncbi::objects::CCleanup_imp::x_RemoveEmptyGenbankDesc);
    x_RecurseForSeqAnnots(bsh, &ncbi::objects::CCleanup_imp::x_ConvertFullLenSourceFeatureToDescriptor);
    x_RecurseForSeqAnnots(bsh, &ncbi::objects::CCleanup_imp::x_ConvertFullLenPubFeatureToDescriptor);    
    x_RecurseForSeqAnnots(bsh, &ncbi::objects::CCleanup_imp::x_RemoveEmptyFeatures); 
    x_MergeAdjacentAnnots(bsh);    
}


void CCleanup_imp::ExtendedCleanup(CSeq_annot_Handle sa)
{
    x_ConvertFullLenSourceFeatureToDescriptor(sa);
    x_ConvertFullLenPubFeatureToDescriptor(sa);    
    x_RemoveEmptyFeatures(sa);
   
    x_CorrectExceptText(sa);
    
    x_MoveDbxrefs(sa);
    
}


void CCleanup_imp::x_RecurseForDescriptors (CBioseq_Handle bs, RecurseDescriptor pmf)
{
    if (bs.IsSetDescr()) {
        CBioseq_EditHandle eh = bs.GetEditHandle();    
        (this->*pmf)(eh.SetDescr());
    }
}


void CCleanup_imp::x_RecurseForDescriptors (CBioseq_set_Handle bss, RecurseDescriptor pmf)
{
    if (bss.IsSetDescr()) {
        CBioseq_set_EditHandle eh = bss.GetEditHandle();
        (this->*pmf)(eh.SetDescr());
        if (eh.SetDescr().Set().empty()) {
            eh.ResetDescr();
        }
    }

    if (bss.GetCompleteBioseq_set()->IsSetSeq_set()) {
       CBioseq_set_EditHandle eh = bss.GetEditHandle();
       CConstRef<CBioseq_set> b = bss.GetCompleteBioseq_set();
       list< CRef< CSeq_entry > > set = (*b).GetSeq_set();
       
       ITERATE (list< CRef< CSeq_entry > >, it, set) {
            switch ((**it).Which()) {
                case CSeq_entry::e_Seq:
                    x_RecurseForDescriptors(m_Scope->GetBioseqHandle((**it).GetSeq()), pmf);
                    break;
                case CSeq_entry::e_Set:
                    x_RecurseForDescriptors(m_Scope->GetBioseq_setHandle((**it).GetSet()), pmf);
                    break;
                case CSeq_entry::e_not_set:
                default:
                    break;
            }
        }
    }
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


//   For any Bioseq or BioseqSet that has a MolInfo descriptor,
//1) If the Bioseq or BioseqSet also has a MolType descriptor, if the MolInfo biomol value is not set 
//   and the value from the MolType descriptor is MOLECULE_TYPE_GENOMIC, MOLECULE_TYPE_PRE_MRNA, 
//   MOLECULE_TYPE_MRNA, MOLECULE_TYPE_RRNA, MOLECULE_TYPE_TRNA, MOLECULE_TYPE_SNRNA, MOLECULE_TYPE_SCRNA, 
//   MOLECULE_TYPE_PEPTIDE, MOLECULE_TYPE_OTHER_GENETIC_MATERIAL, MOLECULE_TYPE_GENOMIC_MRNA_MIX, or 255, 
//   the value from the MolType descriptor will be used to set the MolInfo biomol value.  The MolType descriptor
//   will be removed, whether its value was copied to the MolInfo descriptor or not.
//2) If the Bioseq or BioseqSet also has a Method descriptor, if the MolInfo technique value has not been set and 
//   the Method descriptor value is �concept_trans�, �seq_pept�, �both�, �seq_pept_overlap�,  �seq_pept_homol�, 
//   �concept_transl_a�, or �other�, then the Method descriptor value will be used to set the MolInfo technique value.  
//   The Method descriptor will be removed, whether its value was copied to the MolInfo descriptor or not.
void CCleanup_imp::x_MolInfoUpdate(CSeq_descr& sdr)
{
    bool has_molinfo = false;
    CMolInfo::TBiomol biomol = CMolInfo::eBiomol_unknown;
    CMolInfo::TTech   tech = CMolInfo::eTech_unknown;
    bool changed = false;
    
    NON_CONST_ITERATE (CSeq_descr::Tdata, it, sdr.Set()) {
        if ((*it)->Which() == CSeqdesc::e_Molinfo) {
            if (!has_molinfo) {
                has_molinfo = true;
                const CMolInfo& mol_info = (*it)->GetMolinfo();
                if (mol_info.CanGetBiomol()) {
                    biomol = mol_info.GetBiomol();
                }
                if (mol_info.CanGetTech()) {
                    tech = mol_info.GetTech();
                }
            }          
        } else if ((*it)->Which() == CSeqdesc::e_Mol_type) {
            if (biomol == CMolInfo::eBiomol_unknown) {
                CSeqdesc::TMol_type mol_type = (*it)->GetMol_type();
                switch (mol_type) {
                    case eGIBB_mol_unknown:
                        break;
                    case eGIBB_mol_genomic:
                        biomol = CMolInfo::eBiomol_genomic;
                        break;
                    case eGIBB_mol_pre_mRNA:
                        biomol = CMolInfo::eBiomol_pre_RNA;
                        break;
                    case eGIBB_mol_mRNA:
                        biomol = CMolInfo::eBiomol_mRNA;
                        break;
                    case eGIBB_mol_rRNA:
                        biomol = CMolInfo::eBiomol_rRNA;
                        break;
                    case eGIBB_mol_tRNA:
                        biomol = CMolInfo::eBiomol_tRNA;
                        break;
                    case eGIBB_mol_snRNA:
                        biomol = CMolInfo::eBiomol_snRNA;
                        break;
                    case eGIBB_mol_scRNA:
                        biomol = CMolInfo::eBiomol_scRNA;
                        break;
                    case eGIBB_mol_peptide:
                        biomol = CMolInfo::eBiomol_peptide;
                        break;
                    case eGIBB_mol_other_genetic:
                        biomol = CMolInfo::eBiomol_other_genetic;
                        break;
                    case eGIBB_mol_genomic_mRNA:
                        biomol = CMolInfo::eBiomol_genomic_mRNA;
                        break;
                    case eGIBB_mol_other:
                        biomol = CMolInfo::eBiomol_other;
                        break;
                }
            }
        } else if ((*it)->Which() == CSeqdesc::e_Method) {
            if (tech == CMolInfo::eTech_unknown) {
                CSeqdesc::TMethod method = (*it)->GetMethod();
                switch (method) {
                    case eGIBB_method_concept_trans:
                        tech = CMolInfo::eTech_concept_trans;
                        break;
                    case eGIBB_method_seq_pept:
                        tech = CMolInfo::eTech_seq_pept;
                        break;
                    case eGIBB_method_both:
                        tech = CMolInfo::eTech_both;
                        break;
                    case eGIBB_method_seq_pept_overlap:
                        tech = CMolInfo::eTech_seq_pept_overlap;
                        break;
                    case eGIBB_method_seq_pept_homol:
                        tech = CMolInfo::eTech_seq_pept_homol;
                        break;
                    case eGIBB_method_concept_trans_a:
                        tech = CMolInfo::eTech_concept_trans_a;
                        break;
                    case eGIBB_method_other:
                        tech = CMolInfo::eTech_other;
                        break;
                }
            }
        }
            
    }
    if (!has_molinfo) {
        return;
    }    
    
    NON_CONST_ITERATE (CSeq_descr::Tdata, it, sdr.Set()) {
        if ((*it)->Which() == CSeqdesc::e_Molinfo) {
            CSeqdesc::TMolinfo& mi = (*it)->SetMolinfo();
            if (biomol != CMolInfo::eBiomol_unknown) {
                if (!mi.CanGetBiomol() || mi.GetBiomol() != biomol) {
                    changed = true;
                    mi.SetBiomol (biomol);
                }
            }
            if (tech != CMolInfo::eTech_unknown) {
                if (!mi.CanGetTech() || mi.GetTech() != tech) {
                    changed = true;
                    mi.SetTech (tech);
                }
            }
            break;
        }
    }

    bool found = true;
    while (found) {
        found = false;
        NON_CONST_ITERATE (CSeq_descr::Tdata, it, sdr.Set()) {
            if ((*it)->Which() == CSeqdesc::e_Method || (*it)->Which() == CSeqdesc::e_Mol_type) {
                sdr.Set().erase(it);
                found = true;
                break;
            }
        }        
    }
}


bool IsEmpty (CGB_block& block) 
{
    if ((!block.CanGetExtra_accessions() || block.GetExtra_accessions().size() == 0)
        && (!block.CanGetSource() || NStr::IsBlank(block.GetSource()))
        && (!block.CanGetKeywords() || block.GetKeywords().size() == 0)
        && (!block.CanGetOrigin() || NStr::IsBlank(block.GetOrigin()))
        && (!block.CanGetDate() || NStr::IsBlank (block.GetDate()))
        && (!block.CanGetEntry_date())
        && (!block.CanGetDiv() || NStr::IsBlank(block.GetDiv()))
        && (!block.CanGetTaxonomy() || NStr::IsBlank(block.GetTaxonomy()))) {
        return true;
    } else {
        return false;
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


// Was CleanupGenbankCallback in C Toolkit
// removes taxonomy
// remove Genbank Block descriptors when
//      if (gbp->extra_accessions == NULL && gbp->source == NULL &&
//         gbp->keywords == NULL && gbp->origin == NULL &&
//         gbp->date == NULL && gbp->entry_date == NULL &&
//         gbp->div == NULL && gbp->taxonomy == NULL) {

void CCleanup_imp::x_RemoveEmptyGenbankDesc(CSeq_descr& sdr)
{
    bool found = true;
    
    while (found) {
        found = false;
        NON_CONST_ITERATE (CSeq_descr::Tdata, it, sdr.Set()) {
            if ((*it)->Which() == CSeqdesc::e_Genbank) {
                CGB_block& block = (*it)->SetGenbank();
                block.ResetTaxonomy();
                (*it)->SetGenbank(block);
                if (IsEmpty(block)) {
                    sdr.Set().erase(it);
                    found = true;
                    break;
                }
            }
        }
    }
}


// was EntryCheckGBBlock in C Toolkit
// cleans strings for the GenBankBlock, removes descriptor if block is now empty
void CCleanup_imp::x_CleanGenbankBlockStrings (CGB_block& block)
{
    // clean extra accessions
    if (block.CanGetExtra_accessions()) {
        CleanVisStringList(block.SetExtra_accessions());
        if (block.GetExtra_accessions().size() == 0) {
            block.ResetExtra_accessions();
        }
    }
                
    // clean keywords
    if (block.CanGetKeywords()) {
        CleanVisStringList(block.SetKeywords());
        if (block.GetKeywords().size() == 0) {
            block.ResetKeywords();
        }
    }
                
    // clean source
    if (block.CanGetSource()) {
        string source = block.GetSource();
        CleanVisString (source);
        if (NStr::IsBlank (source)) {
            block.ResetSource();
        } else {
            block.SetSource (source);
        }
    }
    // clean origin
    if (block.CanGetOrigin()) {
        string origin = block.GetOrigin();
        CleanVisString (origin);
        if (NStr::IsBlank (origin)) {
            block.ResetOrigin();
        } else {
            block.SetOrigin(origin);
        }
    }
    //clean date
    if (block.CanGetDate()) {
        string date = block.GetDate();
        CleanVisString (date);
        if (NStr::IsBlank (date)) {
            block.ResetDate();
        } else {
            block.SetDate(date);
        }
    }
    //clean div
    if (block.CanGetDiv()) {
        string div = block.GetDiv();
        CleanVisString (div);
        if (NStr::IsBlank (div)) {
            block.ResetDiv();
        } else {
            block.SetDiv(div);
        }
    }
    //clean taxonomy
    if (block.CanGetTaxonomy()) {
        string tax = block.GetTaxonomy();
        if (NStr::IsBlank (tax)) {
            block.ResetTaxonomy();
        } else {
            block.SetTaxonomy(tax);
        }
    }
}


void CCleanup_imp::x_CleanGenbankBlockStrings (CSeq_descr& sdr)
{
    bool found = true;
    
    while (found) {
        found = false;
        NON_CONST_ITERATE (CSeq_descr::Tdata, it, sdr.Set()) {
            if ((*it)->Which() == CSeqdesc::e_Genbank) {
                x_CleanGenbankBlockStrings((*it)->SetGenbank());
                
                if (IsEmpty((*it)->SetGenbank())) {
                    sdr.Set().erase(it);
                    found = true;
                    break;
                }
            }
        }
    }
}


// removes all title descriptors except the last one
// Was RemoveMultipleTitles in C Toolkit
void CCleanup_imp::x_RemoveMultipleTitles (CSeq_descr& sdr)
{
    int num_titles = 0;
    CSeq_descr::Tdata& current_list = sdr.Set();
    CSeq_descr::Tdata new_list;
    
    new_list.clear();
    
    while (current_list.size() > 0) {
        CRef< CSeqdesc > sd = current_list.back();
        current_list.pop_back();
        if ((*sd).Which() == CSeqdesc::e_Title ) {
            if (num_titles == 0) {
                num_titles ++;
                new_list.push_front(sd);
            }
        } else {
            new_list.push_front(sd);
        }
    }
    
    while (new_list.size() > 0) {
        CRef< CSeqdesc > sd = new_list.front();
        new_list.pop_front();
        current_list.push_back (sd);
    }
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


// Retain the most recent create date, remove the others
// Was MergeMultipleDates in C Toolkit
void CCleanup_imp::x_MergeMultipleDates (CSeq_descr& sdr)
{
    CSeq_descr::Tdata& current_list = sdr.Set();
    CSeq_descr::Tdata new_list;    
    CSeq_descr::Tdata dates_list;    
    int num_dates = 0;
    
    new_list.clear();
    dates_list.clear();
    
    NON_CONST_ITERATE (CSeq_descr::Tdata, it, sdr.Set()) {
        if ((**it).Which() == CSeqdesc::e_Create_date ) {
            if (dates_list.size() > 0) {
                if ((*(dates_list.back())).GetCreate_date().Compare ((**it).GetCreate_date()) == CDate::eCompare_before) {
                    dates_list.pop_back();
                    dates_list.push_back(*it);
                }
            } else {
                dates_list.push_back(*it);
            }
            num_dates ++;
        }
    }
    if (dates_list.size() == 0 || num_dates == 1) {
        return;
    }
    
    num_dates = 0;
    while (current_list.size() > 0) {
        CRef< CSeqdesc > sd = current_list.front();
        current_list.pop_front();
        if ((*sd).Which() == CSeqdesc::e_Create_date ) {
            if (num_dates == 0
                && (*(dates_list.back())).GetCreate_date().Compare (sd->GetCreate_date()) == CDate::eCompare_same) {
                new_list.push_back(sd);
                num_dates ++;
            }
        } else {
            new_list.push_back(sd);
        }
                
    }
    
    while (new_list.size() > 0) {
        CRef< CSeqdesc > sd = new_list.front();
        new_list.pop_front();
        current_list.push_back (sd);
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
                if ((**it).Which() == CSeq_entry::e_Seq) {
                    RenormalizeNucProtSets (m_Scope->GetBioseq_setHandle((**it).GetSet()));
                }
            }
        } else if (bclass == CBioseq_set::eClass_nuc_prot && set.size() == 1) {
            // collapse nuc-prot set
            CBioseq_set_EditHandle bseh = bsh.GetEditHandle();
            
            bseh.GetParentEntry().CollapseSet();
            x_RemoveMultipleTitles(bseh.SetDescr());
        }
    }
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
 * ===========================================================================
 *
 * $Log$
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

