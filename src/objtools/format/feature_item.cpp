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
* Author:  Aaron Ucko, NCBI
*          Mati Shomrat
*
* File Description:
*   new (early 2003) flat-file generator -- representation of features
*   (mainly of interest to implementors)
*
* ===========================================================================
*/
#include <corelib/ncbistd.hpp>
#include <serial/iterator.hpp>

#include <objects/seq/Bioseq.hpp>
#include <objects/seqfeat/Org_ref.hpp>
#include <objects/seqfeat/OrgName.hpp>
#include <objects/seqfeat/OrgMod.hpp>
#include <objects/seqfeat/Code_break.hpp>
#include <objects/seqfeat/Genetic_code.hpp>
#include <objects/seqfeat/Genetic_code_table.hpp>
#include <objects/seqfeat/Imp_feat.hpp>
#include <objects/seqfeat/RNA_ref.hpp>
#include <objects/seqfeat/Trna_ext.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_point.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seqloc/Packed_seqpnt.hpp>
#include <objects/seqloc/Textseq_id.hpp>
#include <objects/general/Object_id.hpp>

#include <objmgr/scope.hpp>
#include <objmgr/seqdesc_ci.hpp>
#include <objmgr/seq_vector.hpp>
#include <objmgr/seq_loc_mapper.hpp>
#include <objmgr/util/sequence.hpp>

#include <util/static_set.hpp>
#include <util/sequtil/sequtil.hpp>
#include <util/sequtil/sequtil_convert.hpp>

#include <algorithm>
#include <objtools/format/formatter.hpp>
#include <objtools/format/items/feature_item.hpp>
#include <objtools/format/context.hpp>
#include "utils.hpp"


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
USING_SCOPE(sequence);


// -- static functions

const CGb_qual* s_GetQual(const CSeq_feat& feat, const string& qual)
{
    ITERATE(CSeq_feat::TQual, it, feat.GetQual()) {
        if ( (*it)->CanGetQual()  &&
             NStr::CompareNocase((*it)->GetQual(), qual) == 0  &&
             (*it)->CanGetVal()  &&  !(*it)->GetVal().empty() ) {
            return *it;
        }
    }
    return 0;
}


static bool s_CheckQuals_cdregion(const CSeq_feat& feat, CFFContext& ctx)
{
    if ( !ctx.CheckCDSProductId() ) {
        return true;
    }

    // non-pseudo CDS must have /product
    bool pseudo = feat.CanGetPseudo()  &&  feat.GetPseudo();
    if ( !pseudo ) {
        const CGene_ref* grp = feat.GetGeneXref();
        if ( grp == 0 ) {
            CConstRef<CSeq_feat> gene = 
                GetOverlappingGene(feat.GetLocation(), ctx.GetScope());
            if ( gene ) {
                pseudo = gene->CanGetPseudo()  &&  gene->GetPseudo();
                if ( !pseudo ) {
                    grp = &(gene->GetData().GetGene());
                }
            }
        }
        if ( !pseudo  &&  grp != 0 ) {
            pseudo = grp->GetPseudo();
        }
    }

    bool just_stop = false;
    if ( feat.CanGetLocation() ) {
        const CSeq_loc& loc = feat.GetLocation();
        if ( loc.IsPartialLeft()  &&  !loc.IsPartialRight() ) {
            if ( GetLength(loc, &ctx.GetScope()) <= 5 ) {
                just_stop = true;
            }
        }
    }

    if ( pseudo ||  just_stop ) {
        return true;
    } 

    // make sure the product has a valid accession
    if ( feat.CanGetProduct() ) {
        const CSeq_id* id = 0;
        try {
            id = &(GetId(feat.GetProduct(), &ctx.GetScope()));
        } catch ( CException& ) {}
        if ( id != 0 ) {
            if ( (id->IsGi()  &&  id->GetGi() > 0) ||  id->IsLocal() ) {
                // !!! make sure the product has a valid accession
                return true;
            } else if ( id->IsGenbank()  ||  id->IsEmbl()    ||  id->IsDdbj()  ||
                        id->IsOther()    ||  id->IsPatent()  ||  
                        id->IsTpg()      ||  id->IsTpe()     ||  id->IsTpd() ) {
                const CTextseq_id* tsip = id->GetTextseq_Id();
                if ( tsip != 0  &&  tsip->CanGetAccession()  &&
                     ValidateAccession(tsip->GetAccession()) ) {
                    return true;
                }
            }
        }
    } else {  // no product
        if ( feat.CanGetExcept()  &&  feat.GetExcept()  &&
             feat.CanGetExcept_text() ) {
            if ( NStr::Find(feat.GetExcept_text(),
                    "rearrangement required for product") != NPOS ) {
                return true;
            }
        }
    }

    return false;
}


// conflict and old_sequence require a publication printable on the segment
static bool s_HasPub(const CSeq_feat& feat, CFFContext& ctx)
{
    if ( !feat.CanGetCit() ) {
        return false;
    }

    ITERATE(CBioseqContext::TReferences, it, ctx.GetReferences()) {
        if ( (*it)->Matches(feat.GetCit()) ) {
            return true;
        }
    }

    return false;
}


static bool s_CheckQuals_conflict(const CSeq_feat& feat, CFFContext& ctx)
{
    if ( !feat.CanGetCit() ) {
        // RefSeq allows conflict with accession in comment instead of sfp->cit
        if ( ctx.IsRefSeq()  &&
             feat.CanGetComment()  &&  !feat.GetComment().empty() ) {
            return true;
        }
    } else {
        return s_HasPub(feat, ctx);
    }

    return false;
}


static bool s_CheckQuals_old_seq(const CSeq_feat& feat, CFFContext& ctx)
{    
    return s_HasPub(feat, ctx);
}


static bool s_CheckQuals_gene(const CSeq_feat& feat, CFFContext& ctx)
{
    // gene requires /gene or /locus_tag, but desc or syn can be mapped to /gene
    const CSeqFeatData::TGene& gene = feat.GetData().GetGene();
    if ( (gene.CanGetLocus()      &&  !gene.GetLocus().empty())      ||
         (gene.CanGetLocus_tag()  &&  !gene.GetLocus_tag().empty())  ||
         (gene.CanGetDesc()       &&  !gene.GetDesc().empty())       ||
         (!gene.GetSyn().empty()  &&  !gene.GetSyn().front().empty()) ) {
        return true;
    }

    return false;
}


static bool s_CheckQuals_bind(const CSeq_feat& feat, CFFContext& ctx)
{
    // protein_bind or misc_binding require eFQ_bound_moiety
    return s_GetQual(feat, "bound_moiety") != 0;
}


static bool s_CheckQuals_mod_base(const CSeq_feat& feat, CFFContext& ctx)
{
    // modified_base requires eFQ_mod_base
    return s_GetQual(feat, "mod_base") != 0;
}


static bool s_CheckMandatoryQuals(const CSeq_feat& feat, CFFContext& ctx)
{
    switch ( feat.GetData().GetSubtype() ) {
    case CSeqFeatData::eSubtype_cdregion:
        {
            return s_CheckQuals_cdregion(feat, ctx);
        }
    case CSeqFeatData::eSubtype_conflict:
        {
            return s_CheckQuals_conflict(feat, ctx);
        }
    case CSeqFeatData::eSubtype_old_sequence:
        {
            return s_CheckQuals_old_seq(feat, ctx);
        }
    case CSeqFeatData::eSubtype_gene:
        {
            return s_CheckQuals_gene(feat, ctx);
        }
    case CSeqFeatData::eSubtype_protein_bind:
    case CSeqFeatData::eSubtype_misc_binding:
        {
            return s_CheckQuals_bind(feat, ctx);
        }
    case CSeqFeatData::eSubtype_modified_base:
        {
            return s_CheckQuals_mod_base(feat, ctx);
        }
    default:
        break;
    }

    return true;
}


static bool s_SkipFeature(const CSeq_feat& feat, CFFContext& ctx)
{
    CSeqFeatData::E_Choice type    = feat.GetData().Which();
    CSeqFeatData::ESubtype subtype = feat.GetData().GetSubtype();        
   
    if ( subtype == CSeqFeatData::eSubtype_pub              ||
         subtype == CSeqFeatData::eSubtype_non_std_residue  ||
         subtype == CSeqFeatData::eSubtype_biosrc           ||
         subtype == CSeqFeatData::eSubtype_rsite            ||
         subtype == CSeqFeatData::eSubtype_seq ) {
        return true;
    }
    
    // check feature customization flags
    if ( ctx.ValidateFeats()  &&
        (subtype == CSeqFeatData::eSubtype_bad  ||
         subtype == CSeqFeatData::eSubtype_virion) ) {
        return true;
    }
    
    if ( ctx.IsNa()  &&  subtype == CSeqFeatData::eSubtype_het ) {
        return true;
    }
    
    if ( ctx.HideImpFeats()  &&  type == CSeqFeatData::e_Imp ) {
        return true;
    }
    
    if ( ctx.HideSnpFeats()  &&  subtype == CSeqFeatData::eSubtype_variation ) {
        return true;
    }

    if ( ctx.HideExonFeats()  &&  subtype == CSeqFeatData::eSubtype_exon ) {
        return true;
    }

    if ( ctx.HideIntronFeats()  &&  subtype == CSeqFeatData::eSubtype_intron ) {
        return true;
    }

    if ( ctx.HideRemImpFeats()  &&  subtype == CSeqFeatData::eSubtype_imp ) {
        if ( subtype == CSeqFeatData::eSubtype_variation  ||
             subtype == CSeqFeatData::eSubtype_exon       ||
             subtype == CSeqFeatData::eSubtype_intron     ||
             subtype == CSeqFeatData::eSubtype_misc_feature ) {
            return true;
        }
    }

    // skip genes in DDBJ format
    if ( ctx.IsFormatDDBJ()  &&  type == CSeqFeatData::e_Gene ) {
        return true;
    }

    // To Do:
    // !!! supress full length comment features

    // if RELEASE mode, make sure we have all info to create mandatory quals.
    if ( ctx.NeedRequiredQuals() ) {
        return !s_CheckMandatoryQuals(feat, ctx);
    }

    return false;
}


// -- FeatureHeader

CFeatHeaderItem::CFeatHeaderItem(CFFContext& ctx) : CFlatItem(ctx)
{
    x_GatherInfo(ctx);
}


void CFeatHeaderItem::x_GatherInfo(CFFContext& ctx)
{
    if ( ctx.IsFormatFTable() ) {
        m_Id.Reset(ctx.GetPrimaryId());
    }
}


// -- FeatureItem

string CFeatureItem::GetKey(void) const
{
    CSeqFeatData::ESubtype subtype = m_Feat->GetData().GetSubtype();
    if ( m_Mapped == eMapped_from_prot  &&  GetContext().IsProt()  &&
         m_Feat->GetData().IsProt()) {
        if ( subtype == CSeqFeatData::eSubtype_preprotein         ||
             subtype == CSeqFeatData::eSubtype_mat_peptide_aa     ||
             subtype == CSeqFeatData::eSubtype_sig_peptide_aa     ||
             subtype == CSeqFeatData::eSubtype_transit_peptide_aa ) {
            return "Precursor";
        }
    }
    if ( GetContext().IsProt() ) {
        switch ( subtype ) {
        case CSeqFeatData::eSubtype_region:
            return "Region";
        case CSeqFeatData::eSubtype_bond:
            return "Bond";
        case CSeqFeatData::eSubtype_site:
            return "Site";
        default:
            break;
        }
    } else if ( subtype == CSeqFeatData::eSubtype_preprotein  &&
                !GetContext().IsRefSeq() ) {
        return "misc_feature";
    }

    // deal with unmappable impfeats
    if ( subtype == CSeqFeatData::eSubtype_imp  &&
        m_Feat->GetData().IsImp() ) {
        const CSeqFeatData::TImp& imp = m_Feat->GetData().GetImp();
        if ( imp.CanGetKey() ) {
            return imp.GetKey();
        }
    }

    return CFeatureItemBase::GetKey();
}


void CFeatureItem::x_GatherInfo(CFFContext& ctx)
{
    if ( s_SkipFeature(GetFeat(), ctx) ) {
        x_SetSkip();
        return;
    }
    m_Type = m_Feat->GetData().GetSubtype();
    x_AddQuals(ctx);
}


static bool s_HasNull(const CSeq_loc& loc)
{
    for ( CSeq_loc_CI it(loc, CSeq_loc_CI::eEmpty_Allow); it; ++it ) {
        if ( it.GetSeq_loc().IsNull() ) {
            return true;
        }
    }

    return false;
}


// constructor
CFeatureItemBase::CFeatureItemBase
(const CSeq_feat& feat,
 CFFContext& ctx,
 const CSeq_loc* loc) 
    :   CFlatItem(ctx), m_Feat(&feat), m_Loc(loc)
{
    if ( !m_Loc.Empty()  &&  ctx.IsSegmented()  &&  ctx.IsStyleMaster() ) {
        if ( m_Loc->IsMix() ) {
            // !!! correct the mapping of the location
            /*
            CSeq_loc_Mapper mapper(ctx.GetHandle());
            mapper.SetMergeAbutting();
            m_Loc = mapper.Map(*m_Loc);
            */
        }
    }
}



CRef<CFlatFeature> CFeatureItemBase::Format(void) const
{
    // extremely rough cut for now -- qualifiers still in progress!
    if (m_FF) {
        return m_FF;
    }

    m_FF.Reset(new CFlatFeature(GetKey(),
                                *new CFlatSeqLoc(GetLoc(), GetContext()), *m_Feat));
    x_FormatQuals();
    return m_FF;
}


static bool s_CheckFuzz(const CInt_fuzz& fuzz)
{
    return !(fuzz.IsLim()  &&  fuzz.GetLim() == CInt_fuzz::eLim_unk);
}


static bool s_LocIsFuzz(const CSeq_feat& feat, const CSeq_loc& loc)
{
    if ( feat.GetData().GetSubtype() == CSeqFeatData::eSubtype_imp  &&
         feat.GetData().IsImp() ) {  // unmappable impfeats
        const CSeqFeatData::TImp& imp = feat.GetData().GetImp();
        if ( imp.CanGetLoc() ) {
            const string& imploc = imp.GetLoc();
            if ( imploc.find('<') != NPOS  ||  imploc.find('>') != NPOS ) {
                return true;
            }
        }
    } else {    // any regular feature test location for fuzz
        for ( CSeq_loc_CI it(loc); it; ++it ) {
            const CSeq_loc& l = it.GetSeq_loc();
            switch ( l.Which() ) {
            case CSeq_loc::e_Pnt:
            {{
                if ( l.GetPnt().CanGetFuzz() ) {
                    if ( s_CheckFuzz(l.GetPnt().GetFuzz()) ) {
                        return true;
                    }
                }
                break;
            }}
            case CSeq_loc::e_Packed_pnt:
            {{
                if ( l.GetPacked_pnt().CanGetFuzz() ) {
                    if ( s_CheckFuzz(l.GetPacked_pnt().GetFuzz()) ) {
                        return true;
                    }
                }
                break;
            }}
            case CSeq_loc::e_Int:
            {{
                bool fuzz = false;
                if ( l.GetInt().CanGetFuzz_from() ) {
                    fuzz = s_CheckFuzz(l.GetInt().GetFuzz_from());
                }
                if ( !fuzz  &&  l.GetInt().CanGetFuzz_to() ) {
                    fuzz = s_CheckFuzz(l.GetInt().GetFuzz_to());
                }
                if ( fuzz ) {
                    return true;
                }
                break;
            }}
            default:
                break;
            }
        }
    }

    return false;
}


void CFeatureItem::x_AddQuals(CFFContext& ctx) const
{
    CScope&             scope = ctx.GetScope();
    const CSeqFeatData& data  = m_Feat->GetData();
    const CSeq_loc&     loc   = GetLoc();
    CSeqFeatData::E_Choice type = data.Which();
    CSeqFeatData::ESubtype subtype = data.GetSubtype();

    bool pseudo = m_Feat->CanGetPseudo() ? m_Feat->GetPseudo() : false;

    // add various common qualifiers...

    // partial
    if ( !(IsMappedFromCDNA()  &&  ctx.IsProt())  &&  !ctx.HideUnclassPartial() ) {
        if ( m_Feat->CanGetPartial()  &&  m_Feat->GetPartial() ) {
            if ( !s_LocIsFuzz(*m_Feat, loc) ) {
                int partial = SeqLocPartialCheck(loc, &scope);
                if ( partial == eSeqlocPartial_Complete ) {
                    x_AddQual(eFQ_partial, new CFlatBoolQVal(true));
                }
            }
        }
    }
    // comment
    if (m_Feat->IsSetComment()) {
        x_AddQual(eFQ_seqfeat_note, new CFlatStringQVal(m_Feat->GetComment()));
    }
    // dbxref
    if (m_Feat->IsSetDbxref()) {
        x_AddQual(eFQ_db_xref, new CFlatXrefQVal(m_Feat->GetDbxref(), &m_Quals));
    }
    // model_evidance and go quals
    if ( m_Feat->IsSetExt() ) {
        x_AddExtQuals(m_Feat->GetExt());
    }
    // evidence
    if ( !ctx.IsProt()  &&  m_Feat->IsSetExp_ev() ) {
        x_AddQual(eFQ_evidence, new CFlatExpEvQVal(m_Feat->GetExp_ev()));
    }
    // citation
    if (m_Feat->IsSetCit()) {
        x_AddQual(eFQ_citation, new CFlatPubSetQVal(m_Feat->GetCit()));
    }
    // exception
    x_AddExceptionQuals(ctx);

    if ( !data.IsGene() ) {
        const CGene_ref* grp = m_Feat->GetGeneXref();
        CConstRef<CSeq_feat> overlap_gene;
        if ( grp != 0  &&  grp->CanGetDb() ) {
            x_AddQual(eFQ_gene_xref, new CFlatXrefQVal(grp->GetDb()));
        }
        if ( grp == 0 ) {
            const CSeq_loc& gene_loc = (ctx.IsProt()  ||  !IsMapped()) ? 
                m_Feat->GetLocation() : GetLoc();
            overlap_gene = GetOverlappingGene(gene_loc, scope);
            if ( overlap_gene ) {
                if ( overlap_gene->CanGetComment() ) {
                    x_AddQual(eFQ_gene_note,
                        new CFlatStringQVal(overlap_gene->GetComment()));
                }
                if ( overlap_gene->CanGetPseudo()  &&  overlap_gene->GetPseudo() ) {
                    pseudo = true;
                }
                grp = &overlap_gene->GetData().GetGene();
                if ( grp != 0  &&  grp->IsSetDb() ) {
                    x_AddQual(eFQ_gene_xref, new CFlatXrefQVal(grp->GetDb()));
                } else if ( overlap_gene->IsSetDbxref() ) {
                    x_AddQual(eFQ_gene_xref, new CFlatXrefQVal(overlap_gene->GetDbxref()));
                }
            }
        }
        if ( grp != 0 ) {
            if (grp->CanGetPseudo()  &&  grp->GetPseudo() ) {
                pseudo = true;
            }
            if ( !grp->IsSuppressed()  &&
                (!overlap_gene  ||
                 subtype != CSeqFeatData::eSubtype_repeat_region) ) {
                x_AddQuals(*grp);
            }
            if ( subtype != CSeqFeatData::eSubtype_variation ) {
                if ( grp->IsSetAllele()  &&  !grp->GetAllele().empty() ) {
                    // now propagating /allele
                    x_AddQual(eFQ_gene_allele, new CFlatStringQVal(grp->GetAllele()));
                }
            }
        }
        if ( type != CSeqFeatData::e_Cdregion  &&  type !=  CSeqFeatData::e_Rna ) {
            x_RemoveQuals(eFQ_gene_xref);
        }
    }

    // specific fields set here...
    switch ( type ) {
    case CSeqFeatData::e_Gene:
        x_AddGeneQuals(*m_Feat, scope);
        break;
    case CSeqFeatData::e_Cdregion:
        x_AddCdregionQuals(*m_Feat, ctx, pseudo);
        break;
    case CSeqFeatData::e_Rna:
        x_AddRnaQuals(*m_Feat, ctx, pseudo);
        break;
    case CSeqFeatData::e_Prot:
        x_AddProtQuals(*m_Feat, ctx, pseudo);
        break;
    case CSeqFeatData::e_Region:
        x_AddRegionQuals(*m_Feat, ctx);
        break;
    // !!! case CSeqFeatData::e_XXX:
    default:
        break;
    }
    
    if ( pseudo ) {
        bool add_pseudo = true;

        // if RELEASE_MODE, check list of features that can have /pseudo
        if ( ctx.DropIllegalQuals()  &&  (data.IsRna()  ||  data.IsImp()) ) {
            switch ( subtype ) {
            case  CSeqFeatData::eSubtype_allele:
            case  CSeqFeatData::eSubtype_attenuator:
            case  CSeqFeatData::eSubtype_CAAT_signal:
            case  CSeqFeatData::eSubtype_conflict:
            case  CSeqFeatData::eSubtype_D_loop:
            case  CSeqFeatData::eSubtype_enhancer:
            case  CSeqFeatData::eSubtype_GC_signal:
            case  CSeqFeatData::eSubtype_iDNA:
            case  CSeqFeatData::eSubtype_intron:
            case  CSeqFeatData::eSubtype_LTR:
            case  CSeqFeatData::eSubtype_misc_binding:
            case  CSeqFeatData::eSubtype_misc_difference:
            case  CSeqFeatData::eSubtype_misc_recomb:
            case  CSeqFeatData::eSubtype_misc_RNA:
            case  CSeqFeatData::eSubtype_misc_signal:
            case  CSeqFeatData::eSubtype_misc_structure:
            case  CSeqFeatData::eSubtype_modified_base:
            case  CSeqFeatData::eSubtype_mutation:
            case  CSeqFeatData::eSubtype_old_sequence:
            case  CSeqFeatData::eSubtype_polyA_signal:
            case  CSeqFeatData::eSubtype_polyA_site:
            case  CSeqFeatData::eSubtype_precursor_RNA:
            case  CSeqFeatData::eSubtype_prim_transcript:
            case  CSeqFeatData::eSubtype_primer_bind:
            case  CSeqFeatData::eSubtype_protein_bind:
            case  CSeqFeatData::eSubtype_RBS:
            case  CSeqFeatData::eSubtype_repeat_region:
            case  CSeqFeatData::eSubtype_repeat_unit:
            case  CSeqFeatData::eSubtype_rep_origin:
            case  CSeqFeatData::eSubtype_satellite:
            case  CSeqFeatData::eSubtype_stem_loop:
            case  CSeqFeatData::eSubtype_STS:
            case  CSeqFeatData::eSubtype_TATA_signal:
            case  CSeqFeatData::eSubtype_terminator:
            case  CSeqFeatData::eSubtype_unsure:
            case  CSeqFeatData::eSubtype_variation:
            case  CSeqFeatData::eSubtype_3clip:
            case  CSeqFeatData::eSubtype_3UTR:
            case  CSeqFeatData::eSubtype_5clip:
            case  CSeqFeatData::eSubtype_5UTR:
            case  CSeqFeatData::eSubtype_10_signal:
            case  CSeqFeatData::eSubtype_35_signal:
                add_pseudo = false;
                break;
            default:
                break;
            }
        }
        if ( add_pseudo ) {
            x_AddQual(eFQ_pseudo, new CFlatBoolQVal(true));
        }
    }
    
    // now go through gbqual list
    if (m_Feat->IsSetQual()) {
        x_ImportQuals(m_Feat->GetQual());
    }

    // cleanup (drop illegal quals, duplicate information etc.)
    x_CleanQuals();
}


static const string s_TrnaList[] = {
  "tRNA-Gap",
  "tRNA-Ala",
  "tRNA-Asx",
  "tRNA-Cys",
  "tRNA-Asp",
  "tRNA-Glu",
  "tRNA-Phe",
  "tRNA-Gly",
  "tRNA-His",
  "tRNA-Ile",
  "tRNA-Lys",
  "tRNA-Leu",
  "tRNA-Met",
  "tRNA-Asn",
  "tRNA-Pro",
  "tRNA-Gln",
  "tRNA-Arg",
  "tRNA-Ser",
  "tRNA-Thr",
  "tRNA-Sec",
  "tRNA-Val",
  "tRNA-Trp",
  "tRNA-OTHER",
  "tRNA-Tyr",
  "tRNA-Glx",
  "tRNA-TERM"
};


static const string& s_AaName(int aa)
{
    int shift = 0, idx = 255;

    if ( aa <= 74 ) {
        shift = 0;
    } else if (aa > 79) {
        shift = 2;
    } else {
        shift = 1;
    }
    if (aa != '*') {
        idx = aa - (64 + shift);
    } else {
        idx = 25;
    }
    if ( idx > 0 && idx < 26 ) {
        return s_TrnaList [idx];
    }
    return kEmptyStr;
}


static int s_ToIupacaa(int aa)
{
    vector<char> n(1, static_cast<char>(aa));
    vector<char> i;
    CSeqConvert::Convert(n, CSeqUtil::e_Ncbieaa, 0, 1, i, CSeqUtil::e_Iupacaa);
    return i.front();
}


void CFeatureItem::x_AddRnaQuals
(const CSeq_feat& feat,
 CFFContext& ctx,
 bool& pseudo) const
{
    const CRNA_ref& rna = feat.GetData().GetRna();

    if ( rna.CanGetPseudo()  &&  rna.GetPseudo() ) {
        pseudo = true;
    }

    CRNA_ref::TType rna_type = rna.CanGetType() ?
        rna.GetType() : CRNA_ref::eType_unknown;
    switch ( rna_type ) {
    case CRNA_ref::eType_tRNA:
    {
        if ( rna.CanGetExt() ) {
            const CRNA_ref::C_Ext& ext = rna.GetExt();
            switch ( ext.Which() ) {
            case CRNA_ref::C_Ext::e_Name:
            {
                // amino acid could not be parsed into structured form
                if ( !ctx.DropIllegalQuals() ) {
                    x_AddQual(eFQ_product,
                              new CFlatStringQVal(ext.GetName()));
                } else {
                    x_AddQual(eFQ_product,
                              new CFlatStringQVal("tRNA-OTHER"));
                }
                break;
            }
            case CRNA_ref::C_Ext::e_TRNA:
            {
                const CTrna_ext& trna = ext.GetTRNA();
                int aa = 0;
                if ( trna.CanGetAa()  &&  trna.GetAa().IsNcbieaa() ) {
                    aa = trna.GetAa().GetNcbieaa();
                } else {
                    // !!!
                    return;
                }
                if ( ctx.IupacaaOnly() ) {
                    aa = s_ToIupacaa(aa);
                }
                const string& aa_str = s_AaName(aa);
                if ( !aa_str.empty() ) {
                    x_AddQual(eFQ_product, new CFlatStringQVal(aa_str));
                    if ( trna.CanGetAnticodon()  &&  !aa_str.empty() ) {
                        x_AddQual(eFQ_anticodon,
                            new CFlatAnticodonQVal(trna.GetAnticodon(),
                                                   aa_str.substr(5, NPOS)));
                    }
                }
                if ( trna.IsSetCodon() ) {
                    x_AddQual(eFQ_trna_codons, new CFlatTrnaCodonsQVal(trna));
                }
                break;
            }
            default:
                break;
            } // end of internal switch
        }
        break;
    }
    case CRNA_ref::eType_mRNA:
    {
        try {
            if ( feat.CanGetProduct() ) {
                const CSeq_id& id = GetId(feat.GetProduct(), &ctx.GetScope());
                CBioseq_Handle prod = 
                    ctx.GetScope().GetBioseqHandleFromTSE(id, ctx.GetHandle());
                EFeatureQualifier slot = 
                    (ctx.IsRefSeq()  ||  ctx.IsModeDump()  ||  ctx.IsModeGBench()) ?
                    eFQ_transcript_id : eFQ_transcript_id_note;
                if ( prod ) {
                    x_AddProductIdQuals(prod, slot);
                } else {
                    x_AddQual(slot, new CFlatSeqIdQVal(id));
                    if ( id.IsGi() ) {
                        x_AddQual(eFQ_db_xref, new CFlatSeqIdQVal(id, true));
                    }
                }
            }
        } catch (CNotUnique&) {}
        if ( !pseudo  &&  ctx.ShowTranscript() ) {
            CSeqVector vec(feat.GetLocation(), ctx.GetScope());
            vec.SetCoding(CBioseq_Handle::eCoding_Iupac);
            string transcription;
            vec.GetSeqData(0, vec.size(), transcription);
            x_AddQual(eFQ_transcription, new CFlatStringQVal(transcription));
        }
        // intentional fall through
    }
    default:
        if ( rna.CanGetExt()  &&  rna.GetExt().IsName() ) {
            x_AddQual(eFQ_product, new CFlatStringQVal(rna.GetExt().GetName()));
        }
        break;
    } // end of switch
}


void CFeatureItem::x_AddGeneQuals(const CSeq_feat& gene, CScope& scope) const
{
    _ASSERT(gene.GetData().Which() == CSeqFeatData::e_Gene);

    x_AddQuals(gene.GetData().GetGene());
    CConstRef<CSeq_feat> operon =
        GetOverlappingOperon(gene.GetLocation(), scope);
    if ( operon ) {
        ITERATE (CSeq_feat::TQual, it, operon->GetQual()) {
            if ( (*it)->CanGetQual()  &&  (*it)->GetQual() == "operon"  &&
                 (*it)->CanGetVal() ) {
                x_AddQual(eFQ_operon, new CFlatStringQVal((*it)->GetVal()));
            }
        }
    }
}


void CFeatureItem::x_AddCdregionQuals
(const CSeq_feat& cds,
 CFFContext& ctx,
 bool& pseudo) const
{
    CScope& scope = ctx.GetScope();
    
    x_AddQuals(cds.GetData().GetCdregion());
    if ( ctx.IsProt()  &&  IsMappedFromCDNA() ) {
        x_AddQual(eFQ_coded_by, new CFlatSeqLocQVal(m_Feat->GetLocation()));
    } else {
        // protein qualifiers
        if ( m_Feat->CanGetProduct() ) {
            const CSeq_id* prot_id = 0;
            // protein id
            try {
                prot_id = &GetId(m_Feat->GetProduct(), &scope);
            } catch (CException&) {
                prot_id = 0;
            }
            
            if ( prot_id != 0 ) {
                CBioseq_Handle prot;
                if ( !ctx.AlwaysTranslateCDS() ) {
                    // by default only show /translation if product bioseq is within
                    // entity, but flag can override and force far /translation
                    prot = ctx.ShowFarTranslations() ? 
                        scope.GetBioseqHandle(*prot_id) : 
                        scope.GetBioseqHandleFromTSE(*prot_id, ctx.GetHandle());
                }
                const CProt_ref* pref = 0;
                if ( prot ) {
                    // Add protein quals (comment, note, names ...) 
                    pref = x_AddProteinQuals(prot);
                } else {
                    x_AddQual(eFQ_protein_id, new CFlatSeqIdQVal(*prot_id));
                    if ( prot_id->IsGi() ) {
                        x_AddQual(eFQ_db_xref, new CFlatSeqIdQVal(*prot_id, true));
                    }
                }
                
                // protein xref overrides names, but should not prevent /protein_id, etc.
                const CProt_ref* p = m_Feat->GetProtXref();
                if ( p != 0 ) {
                    pref = p;
                }
                if ( pref != 0 ) {
                    if ( !pref->GetName().empty() ) {
                        CProt_ref::TName names = pref->GetName();
                        x_AddQual(eFQ_cds_product, new CFlatStringQVal(names.front()));
                        names.pop_front();
                        if ( !names.empty() ) {
                            x_AddQual(eFQ_prot_names, new CFlatStringListQVal(names));
                        }
                    }
                    if ( pref->CanGetDesc() ) {
                        x_AddQual(eFQ_prot_desc, new CFlatStringQVal(pref->GetDesc()));
                    }
                    if ( !pref->GetActivity().empty() ) {
                        x_AddQual(eFQ_prot_activity,
                            new CFlatStringListQVal(pref->GetActivity()));
                    }
                    if ( !pref->GetEc().empty() ) {
                        x_AddQual(eFQ_prot_EC_number,
                            new CFlatStringListQVal(pref->GetEc()));
                    }
                }

                // translation
                if ( !pseudo ) {
                    string translation;
                    if ( (!prot  &&  ctx.TranslateIfNoProd())  ||
                        ctx.AlwaysTranslateCDS() ) {
                        CCdregion_translate::TranslateCdregion(translation, *m_Feat,
                            scope);
                    } else if ( prot ) {
                        TSeqPos len = GetLength(cds.GetProduct(), &scope);
                        if ( len > 0 ){
                            CSeqVector seqv = 
                                prot.GetSeqVector(CBioseq_Handle::eCoding_Ncbi);
                            if ( ctx.IupacaaOnly() ) {
                                seqv.SetCoding(CSeq_data::e_Iupacaa);
                            } else {
                                seqv.SetCoding(CSeq_data::e_Ncbieaa);
                            }
                            seqv.GetSeqData(0, seqv.size(), translation);
                        }
                    }
                    if ( !translation.empty() ) {
                        x_AddQual(eFQ_translation, new CFlatStringQVal(translation));
                    } else {
                        // !!! release mode error
                    }
                }
            }
        }
    }
}


const CProt_ref* CFeatureItem::x_AddProteinQuals(CBioseq_Handle& prot) const
{
    _ASSERT(prot);

    x_AddProductIdQuals(prot, eFQ_protein_id);

    CSeqdesc_CI comm(prot, CSeqdesc::e_Comment, 1);
    if ( comm  &&  !comm->GetComment().empty() ) {
        x_AddQual(eFQ_prot_comment, new CFlatStringQVal(comm->GetComment()));
    }

    CSeqdesc_CI mi(prot, CSeqdesc::e_Molinfo);
    if ( mi ) {
        CMolInfo::TTech prot_tech = mi->GetMolinfo().GetTech();
        if ( prot_tech >  CMolInfo::eTech_standard       &&
             prot_tech != CMolInfo::eTech_concept_trans  &&
             prot_tech != CMolInfo::eTech_concept_trans_a ) {
            if ( !GetTechString(prot_tech).empty() ) {
                x_AddQual(eFQ_prot_method, 
                    new CFlatStringQVal("Method: " + GetTechString(prot_tech)));
            }
        }
    }

    const CProt_ref* pref = 0;
    CFeat_CI prot_feat(prot, 0, 0, CSeqFeatData::e_Prot);
    if ( prot_feat ) {
        pref = &(prot_feat->GetData().GetProt());
        if ( prot_feat->IsSetComment() ) {
            if ( pref->GetProcessed() == CProt_ref::eProcessed_not_set  ||
                 pref->GetProcessed() == CProt_ref::eProcessed_preprotein ) {
                x_AddQual(eFQ_prot_note, 
                    new CFlatStringQVal(prot_feat->GetComment()));
            }
        }
    }

    return pref;
}


static const string s_ValidExceptionText[] = {
  "RNA editing",
  "reasons given in citation"
};


static const string s_ValidRefSeqExceptionText[] = {
  "RNA editing",
  "reasons given in citation",
  "ribosomal slippage",
  "ribosome slippage",
  "trans splicing",
  "trans-splicing",
  "alternative processing",
  "alternate processing",
  "artificial frameshift",
  "non-consensus splice site",
  "nonconsensus splice site",
  "rearrangement required for product",
  "modified codon recognition",
  "alternative start codon",
  "unclassified transcription discrepancy",
  "unclassified translation discrepancy"
};


static bool s_IsValidExceptionText(const string& text)
{
    CStaticArraySet<string> legal_text(s_ValidExceptionText,
        sizeof(s_ValidExceptionText) / sizeof(string));
    return legal_text.find(text) != legal_text.end();
}


static bool s_IsValidRefSeqExceptionText(const string& text)
{
    CStaticArraySet<string> legal_refseq_text(s_ValidRefSeqExceptionText,
        sizeof(s_ValidRefSeqExceptionText) / sizeof(string));
    return legal_refseq_text.find(text) != legal_refseq_text.end();
}


static void s_ParseException
(const string& original,
 string& except,
 string& note,
 CFFContext& ctx)
{
    if ( original.empty() ) {
        return;
    }

    except.erase();
    note.erase();

    if ( !ctx.DropIllegalQuals() ) {
        except = original;
        return;
    }

    list<string> l;
    NStr::Split(original, ",", l);
    NON_CONST_ITERATE (list<string>, it, l) {
        NStr::TruncateSpaces(*it);
    }

    list<string> except_list, note_list;

    ITERATE (list<string>, it, l) {
        if ( s_IsValidExceptionText(*it)  ||
             s_IsValidRefSeqExceptionText(*it) ) {
            except_list.push_back(*it);
        } else {
            note_list.push_back(*it);
        }
    }

    except = NStr::Join(except_list, ", ");
    note = NStr::Join(note_list, ", ");
}


void CFeatureItem::x_AddExceptionQuals(CFFContext& ctx) const
{
    string except_text, note_text;
    
    if ( m_Feat->CanGetExcept_text()  &&  !m_Feat->GetExcept_text().empty() ) {
        except_text = m_Feat->GetExcept_text();
    }
    
    // /exception currently legal only on cdregion
    if ( m_Feat->GetData().IsCdregion()  ||  !ctx.DropIllegalQuals() ) {
        // exception flag is set, but no exception text supplied
        if ( except_text.empty()  &&
             m_Feat->CanGetExcept()  &&  m_Feat->GetExcept() ) {
            // if no /exception text, use text in comment, remove from /note
            if ( x_HasQual(eFQ_seqfeat_note) ) {
                const CFlatStringQVal* qval = 
                    dynamic_cast<const CFlatStringQVal*>(x_GetQual(eFQ_seqfeat_note).first->second.GetPointerOrNull());
                if ( qval != 0 ) {
                    const string& seqfeat_note = qval->GetValue();
                    if ( !ctx.DropIllegalQuals()  ||
                        s_IsValidExceptionText(seqfeat_note) ) {
                        except_text = seqfeat_note;
                        x_RemoveQuals(eFQ_seqfeat_note);
                    }
                }
            } else {
                except_text = "No explanation supplied";
            }

            // if DropIllegalQuals is set, check CDS list here as well
            if ( ctx.DropIllegalQuals()  &&
                 !s_IsValidExceptionText(except_text) ) {
                except_text.erase();
            }
        }
        
        if ( ctx.DropIllegalQuals() ) {
            string except = except_text;
            s_ParseException(except, except_text, note_text, ctx);
        }
    } else if ( !except_text.empty() ) {
        note_text = except_text;
        except_text.erase();
    }

    if ( !except_text.empty() ) {
        x_AddQual(eFQ_exception, new CFlatStringQVal(except_text));
    }
    if ( !note_text.empty() ) {
        x_AddQual(eFQ_exception_note, new CFlatStringQVal(note_text));
    }
}


void CFeatureItem::x_AddProductIdQuals(CBioseq_Handle& prod, EFeatureQualifier slot) const
{
    if ( !prod ) {
        return;
    }

    const CBioseq::TId& ids = prod.GetBioseqCore()->GetId();
    if ( ids.empty() ) {
        return;
    }

    // the product id (transcript or protein) is set to the best id
    const CSeq_id* best = FindBestChoice(ids, CSeq_id::Score);
    if ( best != 0 ) {
        switch ( best->Which() ) {
        case CSeq_id::e_Genbank:
        case CSeq_id::e_Embl:
        case CSeq_id::e_Ddbj:
        case CSeq_id::e_Gi:
        case CSeq_id::e_Other:
        case CSeq_id::e_General:
        case CSeq_id::e_Tpg:
        case CSeq_id::e_Tpe:
        case CSeq_id::e_Tpd:
            // these are the only types we allow as product ids
            break;
        default:
            best = 0;
        }
    }
    if ( best == 0 ) {
        return;
    }
    x_AddQual(slot, new CFlatSeqIdQVal(*best));

    // all other ids are printed as db_xref
    ITERATE (CBioseq::TId, it, ids) {
        const CSeq_id& id = **it;
        if ( &id == best  &&  !id.IsGi() ) {
            // we've already done 'best'. 
            continue;
        }
        x_AddQual(eFQ_db_xref, new CFlatSeqIdQVal(id, id.IsGi()));
    }
}


void CFeatureItem::x_AddRegionQuals(const CSeq_feat& feat, CFFContext& ctx) const
{
    const string& region = feat.GetData().GetRegion();
    if ( region.empty() ) {
        return;
    }

    if ( ctx.IsProt()  &&
         feat.GetData().GetSubtype() == CSeqFeatData::eSubtype_region ) {
        x_AddQual(eFQ_region_name, new CFlatStringQVal(region));
    } else {
        x_AddQual(eFQ_region, new CFlatStringQVal("Region: " + region));
    }
}


void CFeatureItem::x_AddExtQuals(const CSeq_feat::TExt& ext) const
{
    ITERATE (CUser_object::TData, it, ext.GetData()) {
        const CUser_field& field = **it;
        if ( !field.CanGetData() ) {
            continue;
        }
        if ( field.GetData().IsObject() ) {
            const CUser_object& obj = field.GetData().GetObject();
            x_AddExtQuals(obj);
            return;
        } else if ( field.GetData().IsObjects() ) {
            ITERATE (CUser_field::C_Data::TObjects, o, field.GetData().GetObjects()) {
                x_AddExtQuals(**o);
            }
            return;
        }
    }
    if ( ext.CanGetType()  &&  ext.GetType().IsStr() ) {
        const string& oid = ext.GetType().GetStr();
        if ( oid == "ModelEvidence" ) {
            x_AddQual(eFQ_modelev, new CFlatModelEvQVal(ext));
        } else if ( oid == "GeneOntology" ) {
            x_AddGoQuals(ext);
        }
    }
}


void CFeatureItem::x_AddGoQuals(const CUser_object& uo) const
{
    ITERATE (CUser_object::TData, uf_it, uo.GetData()) {
        const CUser_field& field = **uf_it;
        if ( field.IsSetLabel()  &&  field.GetLabel().IsStr() ) {
            const string& label = field.GetLabel().GetStr();
            EFeatureQualifier slot = eFQ_none;
            if ( label == "Process" ) {
                slot = eFQ_go_process;
            } else if ( label == "Component" ) {               
                slot = eFQ_go_component;
            } else if ( label == "Function" ) {
                slot = eFQ_go_function;
            }
            if ( slot == eFQ_none ) {
                continue;
            }

            ITERATE (CUser_field::TData::TFields, it, field.GetData().GetFields()) {
                if ( (*it)->GetData().IsFields() ) {
                    x_AddQual(slot, new CFlatGoQVal(**it));
                }
            }
        }
    }
}


void CFeatureItem::x_AddQuals(const CGene_ref& gene) const
{
    const string* locus = (gene.IsSetLocus()  &&  !gene.GetLocus().empty()) ?
        &gene.GetLocus() : 0;
    
    const string* desc = (gene.IsSetDesc() &&  !gene.GetDesc().empty()) ?
        &gene.GetDesc() : 0;
    const CGene_ref::TSyn* syn = (gene.IsSetSyn()  &&  !gene.GetSyn().empty()) ?
        &gene.GetSyn() : 0;
    const string* locus_tag = 
        (gene.IsSetLocus_tag()  &&  !gene.GetLocus_tag().empty()) ?
        &gene.GetLocus_tag() : 0;

    if ( locus ) {
        x_AddQual(eFQ_gene, new CFlatStringQVal(*locus));
        if ( locus_tag ) {
            x_AddQual(eFQ_locus_tag, new CFlatStringQVal(*locus_tag));
        }
        if ( desc ) {
            x_AddQual(eFQ_gene_desc, new CFlatStringQVal(*desc));
        }
        if ( syn ) {
            x_AddQual(eFQ_gene_syn, new CFlatStringListQVal(*syn));
        }
    } else if ( locus_tag ) {
        x_AddQual(eFQ_locus_tag, new CFlatStringQVal(*locus_tag));
        if ( desc ) {
            x_AddQual(eFQ_gene_desc, new CFlatStringQVal(*desc));
        }
        if ( syn ) {
            x_AddQual(eFQ_gene_syn, new CFlatStringListQVal(*syn));
        }
    } else if ( desc ) {
        x_AddQual(eFQ_gene, new CFlatStringQVal(*desc));
        if ( syn ) {
            x_AddQual(eFQ_gene_syn, new CFlatStringListQVal(*syn));
        }
    } else if ( syn ) {
        CGene_ref::TSyn syns = *syn;
        x_AddQual(eFQ_gene, new CFlatStringQVal(syns.front()));
        syns.pop_front();
        if ( !syns.empty() ) {
            x_AddQual(eFQ_gene_syn, new CFlatStringListQVal(syns));
        }
    }

    if ( gene.IsSetAllele()  &&  !gene.GetAllele().empty() ) {
        x_AddQual(eFQ_gene_allele, new CFlatStringQVal(gene.GetAllele()));
    }
    if ( gene.IsSetMaploc()  &&  !gene.GetMaploc().empty() ) {
        x_AddQual(eFQ_gene_map, new CFlatStringQVal(gene.GetMaploc()));
    }
    if ( gene.IsSetDb() ) {
        x_AddQual(eFQ_gene_xref, new CFlatXrefQVal(gene.GetDb()));
    }
    if ( gene.GetPseudo() ) {
        x_AddQual(eFQ_pseudo, new CFlatBoolQVal(true));
    }
}


void CFeatureItem::x_AddQuals(const CCdregion& cds) const
{
    CFFContext& ctx = GetContext();
    CScope& scope = ctx.GetHandle().GetScope();

    CCdregion::TFrame frame = cds.GetFrame();

    // code break
    if ( cds.IsSetCode_break() ) {
        // set selenocysteine quals
        ITERATE (CCdregion::TCode_break, it, cds.GetCode_break()) {
            if ( !(*it)->IsSetAa() ) {
                continue;
            }
            const CCode_break::C_Aa& cbaa = (*it)->GetAa();
            bool is_U = false;
            switch ( cbaa.Which() ) {
            case CCode_break::C_Aa::e_Ncbieaa:
                is_U = (cbaa.GetNcbieaa() == 'U');
                break;
            case CCode_break::C_Aa::e_Ncbi8aa:
                is_U = (cbaa.GetNcbieaa() == 'U');
            case CCode_break::C_Aa::e_Ncbistdaa:
                is_U = (cbaa.GetNcbieaa() == 24);
                break;
            default:
                break;
            }
            
            if ( is_U ) {
                if ( ctx.SelenocysteineToNote() ) {
                    x_AddQual(eFQ_selenocysteine_note,
                        new CFlatStringQVal("selenocysteine"));
                } else {
                    x_AddQual(eFQ_selenocysteine, new CFlatBoolQVal(true));
                }
                break;
            }
        }
    }

    // translation table
    if ( cds.CanGetCode() ) {
        int gcode = cds.GetCode().GetId();
        if ( gcode > 1 ) {
            x_AddQual(eFQ_transl_table, new CFlatIntQVal(gcode));
        }
    }

    if ( !(ctx.IsProt()  && IsMappedFromCDNA()) ) {
        // frame
        if ( frame == CCdregion::eFrame_not_set ) {
            frame = CCdregion::eFrame_one;
        }
        x_AddQual(eFQ_codon_start, new CFlatIntQVal(frame));

        // translation exception
        if ( cds.IsSetCode_break() ) {
            x_AddQual(eFQ_transl_except, 
                new CFlatCodeBreakQVal(cds.GetCode_break()));
        }
        
        // protein conflict
        static const string conflic_msg = 
            "Protein sequence is in conflict with the conceptual translation";
        bool has_prot = m_Feat->IsSetProduct()  &&
                        (GetLength(m_Feat->GetProduct(), &scope) > 0);
        if ( cds.CanGetConflict()  &&  cds.GetConflict()  &&  has_prot ) {
            x_AddQual(eFQ_prot_conflict, new CFlatStringQVal(conflic_msg));
        }
    } else {
        // frame
        if ( frame > 1 ) {
            x_AddQual(eFQ_codon_start, new CFlatIntQVal(frame));
        }
    }
}

void CFeatureItem::x_AddProtQuals
(const CSeq_feat& feat,
 CFFContext& ctx, 
 bool pseudo) const
{
    const CProt_ref& pref = feat.GetData().GetProt();
    CProt_ref::TProcessed processed = pref.GetProcessed();

    if ( ctx.IsNa()  ||  (ctx.IsProt()  &&  !IsMappedFromProt()) ) {
        if ( pref.IsSetName()  &&  !pref.GetName().empty() ) {
            CProt_ref::TName names = pref.GetName();
            x_AddQual(eFQ_product, new CFlatStringQVal(names.front()));
            names.pop_front();
            if ( !names.empty() ) {
                x_AddQual(eFQ_prot_names, new CFlatStringListQVal(names));
            }
        }
        if ( pref.CanGetDesc()  &&  !pref.GetDesc().empty() ) {
            if ( !ctx.IsProt() ) {
                x_AddQual(eFQ_prot_desc, new CFlatStringQVal(pref.GetDesc()));
            } else {
                x_AddQual(eFQ_prot_name, new CFlatStringQVal(pref.GetDesc()));
            }
        }
        if ( pref.IsSetActivity()  &&  !pref.GetActivity().empty() ) {
            if ( ctx.IsNa()  ||  processed != CProt_ref::eProcessed_mature ) {
                x_AddQual(eFQ_prot_activity, 
                    new CFlatStringListQVal(pref.GetActivity()));
            }
        }
        if ( feat.CanGetProduct() ) {
            CBioseq_Handle prot = 
                ctx.GetScope().GetBioseqHandle(feat.GetProduct());
            if ( prot ) {
                x_AddProductIdQuals(prot, eFQ_protein_id);
            } else {
                try {
                    const CSeq_id& prod_id = 
                        GetId(feat.GetProduct(), &ctx.GetScope());
                    if ( ctx.IsRefSeq()  ||  !ctx.ForGBRelease() ) {
                        x_AddQual(eFQ_protein_id, new CFlatSeqIdQVal(prod_id));
                    }
                } catch (CNotUnique&) {}
            }
        }
    } else { // protein feature on subpeptide bioseq
        x_AddQual(eFQ_derived_from, new CFlatSeqLocQVal(m_Feat->GetLocation()));
        // !!!  check precursor_comment
    }
    if ( !pseudo  &&  ctx.ShowPeptides() ) {
        if ( processed == CProt_ref::eProcessed_mature          ||
             processed == CProt_ref::eProcessed_signal_peptide  ||
             processed == CProt_ref::eProcessed_transit_peptide ) {
            CSeqVector pep(m_Feat->GetLocation(), ctx.GetScope());
            string peptide;
            pep.GetSeqData(pep.begin(), pep.end(), peptide);
            if ( !peptide.empty() ) {
                x_AddQual(eFQ_peptide, new CFlatStringQVal(peptide));
            }
        }
    }

    // cleanup
    if ( processed == CProt_ref::eProcessed_signal_peptide  ||
         processed == CProt_ref::eProcessed_transit_peptide ) {
        if ( !ctx.IsRefSeq() ) {
           // Only RefSeq allows product on signal or transit peptide
           x_RemoveQuals(eFQ_product);
        }
    }
    if ( processed == CProt_ref::eProcessed_preprotein  &&
         !ctx.IsRefSeq()  &&  !ctx.IsProt()  &&  
         feat.GetData().GetSubtype() == CSeqFeatData::eSubtype_preprotein ) {
        TQuals::iterator product = m_Quals.Find(eFQ_product);
        if (  product != m_Quals.end() ) {
            x_AddQual(eFQ_encodes, product->second);
            x_RemoveQuals(eFQ_product);
        }
    }
}


struct SLegalImport {
    const char*       m_Name;
    EFeatureQualifier m_Value;

    operator string(void) const { return m_Name; }
};


void CFeatureItem::x_ImportQuals(const CSeq_feat::TQual& quals) const
{
    static const SLegalImport kLegalImports[] = {
        // Must be in case-insensitive alphabetical order!
#define DO_IMPORT(x) { #x, eFQ_##x }
        DO_IMPORT(allele),
        DO_IMPORT(bound_moiety),
        DO_IMPORT(clone),
        DO_IMPORT(codon),
        DO_IMPORT(cons_splice),
        DO_IMPORT(direction),
        DO_IMPORT(EC_number),
        DO_IMPORT(frequency),
        DO_IMPORT(function),
        DO_IMPORT(insertion_seq),
        DO_IMPORT(label),
        DO_IMPORT(map),
        DO_IMPORT(mod_base),
        DO_IMPORT(number),
        DO_IMPORT(organism),
        DO_IMPORT(PCR_conditions),
        DO_IMPORT(phenotype),
        { "product", eFQ_product_quals },
        DO_IMPORT(replace),
        DO_IMPORT(rpt_family),
        DO_IMPORT(rpt_type),
        DO_IMPORT(rpt_unit),
        DO_IMPORT(standard_name),
        DO_IMPORT(transposon),
        DO_IMPORT(usedin)
#undef DO_IMPORT
    };
    static const SLegalImport* kLegalImportsEnd
        = kLegalImports + sizeof(kLegalImports)/sizeof(SLegalImport);

    ITERATE (CSeq_feat::TQual, it, quals) {
        const string&       name = (*it)->GetQual();
        const SLegalImport* li   = lower_bound(kLegalImports, kLegalImportsEnd,
                                               name, PNocase());
        EFeatureQualifier   slot = eFQ_illegal_qual;
        if (li != kLegalImportsEnd && !NStr::CompareNocase(li->m_Name,name)) {
            slot = li->m_Value;
        }
        switch (slot) {
        case eFQ_codon:
        case eFQ_cons_splice:
        case eFQ_direction:
        case eFQ_mod_base:
        case eFQ_number:
        case eFQ_rpt_type:
        case eFQ_rpt_unit:
        case eFQ_usedin:
            // XXX -- each of these should really get its own class
            // (to verify correct syntax)
            x_AddQual(slot, new CFlatStringQVal((*it)->GetVal(),
                CFlatQual::eUnquoted));
            break;
        case eFQ_label:
            x_AddQual(slot, new CFlatLabelQVal((*it)->GetVal()));
            break;
        case eFQ_illegal_qual:
            x_AddQual(slot, new CFlatIllegalQVal(**it));
            break;
        default:
            // XXX - should split off EC_number and replace
            // (to verify correct syntax)
            x_AddQual(slot, new CFlatStringQVal((*it)->GetVal()));
            break;
        }
    }
}


void CFeatureItem::x_FormatQuals(void) const
{
    m_FF->SetQuals().reserve(m_Quals.Size());
    CFlatFeature::TQuals& qvec = m_FF->SetQuals();

#define DO_QUAL(x) x_FormatQual(eFQ_##x, #x, m_FF->SetQuals())
    DO_QUAL(partial);
    DO_QUAL(gene);

    DO_QUAL(locus_tag);
    DO_QUAL(gene_syn_refseq);

    x_FormatQual(eFQ_gene_allele, "allele", qvec);

    DO_QUAL(operon);

    DO_QUAL(product);

    x_FormatQual(eFQ_prot_EC_number, "EC_number", qvec);
    x_FormatQual(eFQ_prot_activity,  "function", qvec);

    DO_QUAL(standard_name);
    DO_QUAL(coded_by);
    DO_QUAL(derived_from);

    x_FormatQual(eFQ_prot_name, "name", qvec);
    DO_QUAL(region_name);
    DO_QUAL(bond_type);
    DO_QUAL(site_type);
    DO_QUAL(sec_str_type);
    DO_QUAL(heterogen);

    if ( !GetContext().GoQualsToNote() ) {
        DO_QUAL(go_component);
        DO_QUAL(go_function);
        DO_QUAL(go_process);
    }

    x_FormatNoteQuals();
        
    DO_QUAL(citation);

    DO_QUAL(number);

    DO_QUAL(pseudo);
    DO_QUAL(selenocysteine);

    DO_QUAL(codon_start);

    DO_QUAL(anticodon);
    DO_QUAL(bound_moiety);
    DO_QUAL(clone);
    DO_QUAL(cons_splice);
    DO_QUAL(direction);
    DO_QUAL(function);
    DO_QUAL(evidence);
    DO_QUAL(exception);
    DO_QUAL(frequency);
    DO_QUAL(EC_number);
    x_FormatQual(eFQ_gene_map,    "map", qvec);
    DO_QUAL(allele);
    DO_QUAL(map);
    DO_QUAL(mod_base);
    DO_QUAL(PCR_conditions);
    DO_QUAL(phenotype);
    DO_QUAL(rpt_family);
    DO_QUAL(rpt_type);
    DO_QUAL(rpt_unit);
    DO_QUAL(insertion_seq);
    DO_QUAL(transposon);
    DO_QUAL(usedin);

    // extra imports, actually...
    x_FormatQual(eFQ_illegal_qual, "illegal", qvec);

    DO_QUAL(replace);

    DO_QUAL(transl_except);
    DO_QUAL(transl_table);
    DO_QUAL(codon);
    DO_QUAL(organism);
    DO_QUAL(label);
    x_FormatQual(eFQ_cds_product, "product", qvec);
    DO_QUAL(protein_id);
    DO_QUAL(transcript_id);
    DO_QUAL(db_xref);
    x_FormatQual(eFQ_gene_xref, "db_xref", qvec);
    DO_QUAL(translation);
    DO_QUAL(transcription);
    DO_QUAL(peptide);
#undef DO_QUAL
}


void CFeatureItem::x_FormatNoteQuals(void) const
{
    CFlatFeature::TQuals qvec;

#define DO_NOTE(x) x_FormatNoteQual(eFQ_##x, #x, qvec)
    x_FormatNoteQual(eFQ_transcript_id_note, "tscpt_id_note", qvec);
    DO_NOTE(gene_desc);
    DO_NOTE(gene_syn);
    DO_NOTE(trna_codons);
    DO_NOTE(prot_desc);
    DO_NOTE(prot_note);
    DO_NOTE(prot_comment);
    DO_NOTE(prot_method);
    DO_NOTE(figure);
    DO_NOTE(maploc);
    DO_NOTE(prot_conflict);
    DO_NOTE(prot_missing);
    DO_NOTE(seqfeat_note);
    DO_NOTE(exception_note);
    DO_NOTE(region);
    DO_NOTE(selenocysteine_note);
    DO_NOTE(prot_names);
    DO_NOTE(bond);
    DO_NOTE(site);
    DO_NOTE(rrna_its);
    DO_NOTE(xtra_prod_quals);
    DO_NOTE(modelev);
    if ( GetContext().GoQualsToNote() ) {
        DO_NOTE(go_component);
        DO_NOTE(go_function);
        DO_NOTE(go_process);
    }
#undef DO_NOTE

    string notestr;
    const string* suffix = &kEmptyStr;

    CFlatFeature::TQuals::const_iterator last = qvec.end();
    CFlatFeature::TQuals::const_iterator end = last--;
    CFlatFeature::TQuals::const_iterator it = qvec.begin();
    bool add_period = false;
    for ( ; it != end; ++it ) {
        add_period = false;
        string note = (*it)->GetValue();
        if ( NStr::EndsWith(note, ".")  &&  !NStr::EndsWith(note, "...") ) {
            note.erase(note.length() - 1);
            add_period = true;
        }
        const string& prefix = *suffix + (*it)->GetPrefix();
        JoinNoRedund(notestr, prefix, note);
        suffix = &(*it)->GetSuffix();
    }

    if ( !notestr.empty() ) {
        NStr::TruncateSpaces(notestr);
        if ( add_period  &&  !NStr::EndsWith(notestr, ".") ) {
            notestr += ".";
        }
        CRef<CFlatQual> note(new CFlatQual("note", 
            ExpandTildes(notestr, eTilde_newline)));
        m_FF->SetQuals().push_back(note);
    }
}


void CFeatureItem::x_FormatQual
(EFeatureQualifier slot,
 const string& name,
 CFlatFeature::TQuals& qvec,
 IFlatQVal::TFlags flags) const
{
    pair<TQCI, TQCI> range = const_cast<const TQuals&>(m_Quals).GetQuals(slot);
    for (TQCI it = range.first;  it != range.second;  ++it) {
        it->second->Format(qvec, name, GetContext(), flags);
    }
}


void CFeatureItem::x_CleanQuals(void) const
{
    // /gene same as feature.comment will suppress /note
    if ( m_Feat->CanGetComment()  &&  x_HasQual(eFQ_gene) ) {
        const IFlatQVal* qval = m_Quals.Find(eFQ_gene)->second;
        const CFlatStringQVal* strval = 
            dynamic_cast<const CFlatStringQVal*>(qval);
        if ( strval != 0  &&  
             NStr::EqualNocase(strval->GetValue(), m_Feat->GetComment()) ) {
            x_RemoveQuals(eFQ_seqfeat_note);
        }
    }
}


/////////////////////////////////////////////////////////////////////////////
//   Source Feature
/////////////////////////////////////////////////////////////////////////////

void CSourceFeatureItem::x_AddQuals(CFFContext& ctx) const
{
    const CSeqFeatData& data = m_Feat->GetData();
    _ASSERT(data.IsOrg()  ||  data.IsBiosrc());
    // add various generic qualifiers...
    x_AddQual(eSQ_mol_type,
              new CFlatMolTypeQVal(ctx.GetBiomol(), ctx.GetMol()));
    if (m_Feat->IsSetComment()) {
        x_AddQual(eSQ_seqfeat_note, new CFlatStringQVal(m_Feat->GetComment()));
    }
    if (m_Feat->IsSetTitle()) {
        x_AddQual(eSQ_label, new CFlatLabelQVal(m_Feat->GetTitle()));
    }
    if (m_Feat->IsSetCit()) {
        x_AddQual(eSQ_citation, new CFlatPubSetQVal(m_Feat->GetCit()));
    }
    if (m_Feat->IsSetDbxref()) {
        x_AddQual(eSQ_org_xref, new CFlatXrefQVal(m_Feat->GetDbxref()));
    }
    // add qualifiers from biosource fields
    x_AddQuals(data.GetBiosrc(), ctx);
}


static ESourceQualifier s_OrgModToSlot(const COrgMod& om)
{
    switch ( om.GetSubtype() ) {
#define CASE_ORGMOD(x) case COrgMod::eSubtype_##x:  return eSQ_##x;
        CASE_ORGMOD(strain);
        CASE_ORGMOD(substrain);
        CASE_ORGMOD(type);
        CASE_ORGMOD(subtype);
        CASE_ORGMOD(variety);
        CASE_ORGMOD(serotype);
        CASE_ORGMOD(serogroup);
        CASE_ORGMOD(serovar);
        CASE_ORGMOD(cultivar);
        CASE_ORGMOD(pathovar);
        CASE_ORGMOD(chemovar);
        CASE_ORGMOD(biovar);
        CASE_ORGMOD(biotype);
        CASE_ORGMOD(group);
        CASE_ORGMOD(subgroup);
        CASE_ORGMOD(isolate);
        CASE_ORGMOD(common);
        CASE_ORGMOD(acronym);
        CASE_ORGMOD(dosage);
    case COrgMod::eSubtype_nat_host:  return eSQ_spec_or_nat_host;
        CASE_ORGMOD(sub_species);
        CASE_ORGMOD(specimen_voucher);
        CASE_ORGMOD(authority);
        CASE_ORGMOD(forma);
        CASE_ORGMOD(forma_specialis);
        CASE_ORGMOD(ecotype);
        CASE_ORGMOD(synonym);
        CASE_ORGMOD(anamorph);
        CASE_ORGMOD(teleomorph);
        CASE_ORGMOD(breed);
        CASE_ORGMOD(gb_acronym);
        CASE_ORGMOD(gb_anamorph);
        CASE_ORGMOD(gb_synonym);
        CASE_ORGMOD(old_lineage);
        CASE_ORGMOD(old_name);
#undef CASE_ORGMOD
    case COrgMod::eSubtype_other:  return eSQ_orgmod_note;
    default:                       return eSQ_none;
    }
}


void CSourceFeatureItem::x_AddQuals(const COrg_ref& org, CFFContext& ctx) const
{
    string taxname, common;
    if ( org.IsSetTaxname() ) {
        taxname = org.GetTaxname();
    }
    if ( taxname.empty()  &&  ctx.NeedOrganismQual() ) {
        taxname = "unknown";
        if ( org.IsSetCommon() ) {
            common = org.GetCommon();
        }
    }
    if ( !taxname.empty() ) {
        x_AddQual(eSQ_organism, new CFlatStringQVal(taxname));
    }
    if ( !common.empty() ) {
        x_AddQual(eSQ_common_name, new CFlatStringQVal(common));
    }
    if ( org.CanGetOrgname() ) {
        ITERATE (COrgName::TMod, it, org.GetOrgname().GetMod()) {
            ESourceQualifier slot = s_OrgModToSlot(**it);
            if (slot != eSQ_none) {
                x_AddQual(slot, new CFlatOrgModQVal(**it));
            }
        }
    }
    if ( !WasDesc() ) {
        x_AddQual(eSQ_unstructured, new CFlatStringListQVal(org.GetMod()));
    }
    if ( org.IsSetDb() ) {
        x_AddQual(eSQ_db_xref, new CFlatXrefQVal(org.GetDb()));
    }
}


static ESourceQualifier s_SubSourceToSlot(const CSubSource& ss)
{
    switch (ss.GetSubtype()) {
#define DO_SS(x) case CSubSource::eSubtype_##x:  return eSQ_##x;
        DO_SS(chromosome);
        DO_SS(map);
        DO_SS(clone);
        DO_SS(subclone);
        DO_SS(haplotype);
        DO_SS(genotype);
        DO_SS(sex);
        DO_SS(cell_line);
        DO_SS(cell_type);
        DO_SS(tissue_type);
        DO_SS(clone_lib);
        DO_SS(dev_stage);
        DO_SS(frequency);
        DO_SS(germline);
        DO_SS(rearranged);
        DO_SS(lab_host);
        DO_SS(pop_variant);
        DO_SS(tissue_lib);
        DO_SS(plasmid_name);
        DO_SS(transposon_name);
        DO_SS(insertion_seq_name);
        DO_SS(plastid_name);
        DO_SS(country);
        DO_SS(segment);
        DO_SS(endogenous_virus_name);
        DO_SS(transgenic);
        DO_SS(environmental_sample);
        DO_SS(isolation_source);
#undef DO_SS
    case CSubSource::eSubtype_other:  return eSQ_subsource_note;
    default:                          return eSQ_none;
    }
}


void CSourceFeatureItem::x_AddQuals(const CBioSource& src, CFFContext& ctx) const
{
    // add qualifiers from Org_ref field
    if ( src.CanGetOrg() ) {
        x_AddQuals(src.GetOrg(), ctx);
    }
    x_AddQual(eSQ_focus, new CFlatBoolQVal(src.IsSetIs_focus()));

    
    bool insertion_seq_name = false,
         plasmid_name = false,
         transposon_name = false;
    ITERATE (CBioSource::TSubtype, it, src.GetSubtype()) {
        ESourceQualifier slot = s_SubSourceToSlot(**it);
        if ( slot == eSQ_insertion_seq_name ) {
            insertion_seq_name = true;
        } else if ( slot == eSQ_plasmid_name ) {
            plasmid_name = true;
        } else if ( slot == eSQ_transposon_name ) {
            transposon_name = true;
        }
        if (slot != eSQ_none) {
            x_AddQual(slot, new CFlatSubSourceQVal(**it));
        }
    }

    // some qualifiers are flags in genome and names in subsource,
    // print once with name
    CBioSource::TGenome genome = src.GetGenome();
    CRef<CFlatOrganelleQVal> organelle(new CFlatOrganelleQVal(genome));
    if ( (insertion_seq_name  &&  genome == CBioSource::eGenome_insertion_seq)  ||
         (plasmid_name  &&  genome == CBioSource::eGenome_plasmid)  ||
         (transposon_name  &&  genome == CBioSource::eGenome_transposon) ) {
        organelle.Reset();
    }
    if ( organelle ) {
        x_AddQual(eSQ_organelle, organelle);
    }

    if ( !WasDesc()  &&  m_Feat->CanGetComment() ) {
        x_AddQual(eSQ_seqfeat_note, new CFlatStringQVal(m_Feat->GetComment()));
    }
}


void CSourceFeatureItem::x_FormatQuals(void) const
{
    m_FF->SetQuals().reserve(m_Quals.Size());
    CFlatFeature::TQuals& qvec = m_FF->SetQuals();

#define DO_QUAL(x) x_FormatQual(eSQ_##x, #x, qvec)
    DO_QUAL(organism);

    DO_QUAL(organelle);

    DO_QUAL(mol_type);

    DO_QUAL(strain);
    x_FormatQual(eSQ_substrain, "sub_strain", qvec);
    DO_QUAL(variety);
    DO_QUAL(serotype);
    DO_QUAL(serovar);
    DO_QUAL(cultivar);
    DO_QUAL(isolate);
    DO_QUAL(isolation_source);
    x_FormatQual(eSQ_spec_or_nat_host, "specific_host", qvec);
    DO_QUAL(sub_species);
    DO_QUAL(specimen_voucher);

    DO_QUAL(db_xref);
    x_FormatQual(eSQ_org_xref, "db_xref", qvec);

    DO_QUAL(chromosome);

    DO_QUAL(segment);

    DO_QUAL(map);
    DO_QUAL(clone);
    x_FormatQual(eSQ_subclone, "sub_clone", qvec);
    DO_QUAL(haplotype);
    DO_QUAL(sex);
    DO_QUAL(cell_line);
    DO_QUAL(cell_type);
    DO_QUAL(tissue_type);
    DO_QUAL(clone_lib);
    DO_QUAL(dev_stage);
    DO_QUAL(frequency);

    DO_QUAL(germline);
    DO_QUAL(rearranged);
    DO_QUAL(transgenic);
    DO_QUAL(environmental_sample);

    DO_QUAL(lab_host);
    DO_QUAL(pop_variant);
    DO_QUAL(tissue_lib);

    x_FormatQual(eSQ_plasmid_name,       "plasmid", qvec);
    x_FormatQual(eSQ_transposon_name,    "transposon", qvec);
    x_FormatQual(eSQ_insertion_seq_name, "insertion_seq", qvec);

    DO_QUAL(country);

    DO_QUAL(focus);

    if ( !GetContext().SrcQualsToNote() ) {
        // some note qualifiers appear as regular quals in GBench or Dump mode
        x_FormatGBNoteQuals();
    }

    DO_QUAL(sequenced_mol);
    DO_QUAL(label);
    DO_QUAL(usedin);
    DO_QUAL(citation);
#undef DO_QUAL

    // Format the rest of the note quals (ones that weren't formatted above)
    // as a single note qualifier
    x_FormatNoteQuals();
}


void CSourceFeatureItem::x_FormatGBNoteQuals(void) const
{
    _ASSERT(!GetContext().SrcQualsToNote());
    CFlatFeature::TQuals& qvec = m_FF->SetQuals();

#define DO_QUAL(x) x_FormatQual(eSQ_##x, #x, qvec)
    DO_QUAL(type);
    DO_QUAL(subtype);
    DO_QUAL(serogroup);
    DO_QUAL(pathovar);
    DO_QUAL(chemovar);
    DO_QUAL(biovar);
    DO_QUAL(biotype);
    DO_QUAL(group);
    DO_QUAL(subgroup);
    DO_QUAL(common);
    DO_QUAL(acronym);
    DO_QUAL(dosage);
    
    DO_QUAL(authority);
    DO_QUAL(forma);
    DO_QUAL(forma_specialis);
    DO_QUAL(ecotype);
    DO_QUAL(synonym);
    DO_QUAL(anamorph);
    DO_QUAL(teleomorph);
    DO_QUAL(breed);
    
    DO_QUAL(genotype);
    x_FormatQual(eSQ_plastid_name, "plastid", qvec);
    
    x_FormatQual(eSQ_endogenous_virus_name, "endogenous_virus", qvec);

    x_FormatQual(eSQ_zero_orgmod, "?", qvec);
    x_FormatQual(eSQ_one_orgmod,  "?", qvec);
    x_FormatQual(eSQ_zero_subsrc, "?", qvec);
#undef DO_QUAL
}


void CSourceFeatureItem::x_FormatNoteQuals() const
{
    CFlatFeature::TQuals qvec;

#define DO_NOTE(x) x_FormatNoteQual(eSQ_##x, #x, qvec)
    if (m_WasDesc) {
        x_FormatNoteQual(eSQ_seqfeat_note, "note", qvec);
        DO_NOTE(orgmod_note);
        DO_NOTE(subsource_note);
    } else {
        DO_NOTE(unstructured);
    }

    if ( GetContext().SrcQualsToNote() ) {
        DO_NOTE(type);
        DO_NOTE(subtype);
        DO_NOTE(serogroup);
        DO_NOTE(pathovar);
        DO_NOTE(chemovar);
        DO_NOTE(biovar);
        DO_NOTE(biotype);
        DO_NOTE(group);
        DO_NOTE(subgroup);
        DO_NOTE(common);
        DO_NOTE(acronym);
        DO_NOTE(dosage);
        
        DO_NOTE(authority);
        DO_NOTE(forma);
        DO_NOTE(forma_specialis);
        DO_NOTE(ecotype);
        DO_NOTE(synonym);
        DO_NOTE(anamorph);
        DO_NOTE(teleomorph);
        DO_NOTE(breed);
        
        DO_NOTE(genotype);
        x_FormatNoteQual(eSQ_plastid_name, "plastid", qvec);
        
        x_FormatNoteQual(eSQ_endogenous_virus_name, "endogenous_virus", qvec);
    }

    x_FormatNoteQual(eSQ_common_name, "common", qvec);

    if ( GetContext().SrcQualsToNote() ) {
        x_FormatNoteQual(eSQ_zero_orgmod, "?", qvec);
        x_FormatNoteQual(eSQ_one_orgmod,  "?", qvec);
        x_FormatNoteQual(eSQ_zero_subsrc, "?", qvec);
    }
#undef DO_NOTE

    string notestr;
    const string* suffix = &kEmptyStr;

    if ( GetSource().CanGetGenome()  &&  
        GetSource().GetGenome() == CBioSource::eGenome_extrachrom ) {
        static const string kEOL = "\n";
        notestr += "extrachromosomal";
        suffix = &kEOL;
    }

    CFlatFeature::TQuals::const_iterator last = qvec.end();
    CFlatFeature::TQuals::const_iterator end = last--;
    CFlatFeature::TQuals::const_iterator it = qvec.begin();
    for ( ; it != end; ++it ) {
        string note = (*it)->GetValue();
        if ( NStr::EndsWith(note, ".")  &&  !NStr::EndsWith(note, "...") ) {
            note.erase(note.length() - 1);
        }
        const string& prefix = *suffix + (*it)->GetPrefix();
        JoinNoRedund(notestr, prefix, note);
        suffix = &(*it)->GetSuffix();
    }
    if ( !notestr.empty() ) {
        CRef<CFlatQual> note(new CFlatQual("note", notestr));
        m_FF->SetQuals().push_back(note);
    }
}


CSourceFeatureItem::CSourceFeatureItem
(const CBioSource& src,
 TRange range,
 CFFContext& ctx)
    : CFeatureItemBase(*new CSeq_feat, ctx),
      m_WasDesc(true)
{
    CSeq_feat& feat = const_cast<CSeq_feat&>(*m_Feat);
    feat.SetData().SetBiosrc(const_cast<CBioSource&>(src));
    if ( range.IsWhole() ) {
        feat.SetLocation().SetWhole(*ctx.GetPrimaryId());
    } else {
        CSeq_interval& ival = feat.SetLocation().SetInt();
        ival.SetFrom(range.GetFrom());
        ival.SetTo(range.GetTo());
        ival.SetId(*ctx.GetPrimaryId());
    }

    x_GatherInfo(ctx);
}


void CSourceFeatureItem::x_FormatQual
(ESourceQualifier slot,
 const string& name,
 CFlatFeature::TQuals& qvec,
 IFlatQVal::TFlags flags) const
{
    pair<TQCI, TQCI> range = const_cast<const TQuals&>(m_Quals).GetQuals(slot);
    for (TQCI it = range.first;  it != range.second;  ++it) {
        it->second->Format(qvec, name, GetContext(),
                           flags | IFlatQVal::fIsSource);
    }
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ===========================================================================
*
* $Log$
* Revision 1.15  2004/03/31 16:00:12  ucko
* C(Source)FeatureItem::x_FormatQual: make sure to call the const
* version of GetQuals to fix WorkShop build errors.
*
* Revision 1.14  2004/03/30 20:27:09  shomrat
* Separated quals container from feature class
*
* Revision 1.13  2004/03/25 20:37:41  shomrat
* Use handles
*
* Revision 1.12  2004/03/18 15:37:57  shomrat
* Fixes to note qual formatting
*
* Revision 1.11  2004/03/10 21:30:18  shomrat
* + product tRNA qual; limit Seq-is types for product ids
*
* Revision 1.10  2004/03/10 16:22:18  shomrat
* Fixed product-id qualifiers
*
* Revision 1.9  2004/03/08 21:02:18  shomrat
* + Exception quals gathering; fixed product id gathering
*
* Revision 1.8  2004/03/08 15:45:46  shomrat
* Added pseudo qualifier
*
* Revision 1.7  2004/03/05 18:44:48  shomrat
* fixes to qualifiers collection and formatting
*
* Revision 1.6  2004/02/19 18:07:26  shomrat
* HideXXXFeat() => HideXXXFeats()
*
* Revision 1.5  2004/02/11 22:50:35  shomrat
* override GetKey
*
* Revision 1.4  2004/02/11 16:56:42  shomrat
* changes in qualifiers gathering and formatting
*
* Revision 1.3  2004/01/14 16:12:20  shomrat
* uncommenetd code
*
* Revision 1.2  2003/12/18 17:43:33  shomrat
* context.hpp moved
*
* Revision 1.1  2003/12/17 20:20:52  shomrat
* Initial Revision (adapted from flat lib)
*
* Revision 1.10  2003/10/17 20:58:41  ucko
* Don't assume coding-region features have their "product" fields set.
*
* Revision 1.9  2003/10/16 20:21:53  ucko
* Fix a copy-and-paste error in CFeatureItem::x_AddQuals
*
* Revision 1.8  2003/10/08 21:11:12  ucko
* Add a couple of accessors to CFeatureItemBase for the GFF/GTF formatter.
*
* Revision 1.7  2003/07/22 18:04:13  dicuccio
* Fixed access of unset optional variables
*
* Revision 1.6  2003/06/02 16:06:42  dicuccio
* Rearranged src/objects/ subtree.  This includes the following shifts:
*     - src/objects/asn2asn --> arc/app/asn2asn
*     - src/objects/testmedline --> src/objects/ncbimime/test
*     - src/objects/objmgr --> src/objmgr
*     - src/objects/util --> src/objmgr/util
*     - src/objects/alnmgr --> src/objtools/alnmgr
*     - src/objects/flat --> src/objtools/flat
*     - src/objects/validator --> src/objtools/validator
*     - src/objects/cddalignview --> src/objtools/cddalignview
* In addition, libseq now includes six of the objects/seq... libs, and libmmdb
* replaces the three libmmdb? libs.
*
* Revision 1.5  2003/04/24 16:15:58  vasilche
* Added missing includes and forward class declarations.
*
* Revision 1.4  2003/03/21 18:49:17  ucko
* Turn most structs into (accessor-requiring) classes; replace some
* formerly copied fields with pointers to the original data.
*
* Revision 1.3  2003/03/11 15:37:51  kuznets
* iterate -> ITERATE
*
* Revision 1.2  2003/03/10 22:01:36  ucko
* Change SLegalImport::m_Name from string to const char* (needed by MSVC).
*
* Revision 1.1  2003/03/10 16:39:09  ucko
* Initial check-in of new flat-file generator
*
*
* ===========================================================================
*/
