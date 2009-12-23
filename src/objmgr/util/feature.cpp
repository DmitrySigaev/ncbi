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
* Author:  Clifford Clausen
*
* File Description:
*   Sequence utilities
*/

#include <ncbi_pch.hpp>
#include <serial/objistr.hpp>
#include <serial/serial.hpp>
#include <serial/iterator.hpp>
#include <serial/enumvalues.hpp>


#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/seq_vector.hpp>
#include <objmgr/feat_ci.hpp>
#include <objmgr/impl/handle_range_map.hpp>

#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqfeat/SeqFeatXref.hpp>
#include <objects/seqfeat/Imp_feat.hpp>
#include <objects/seqfeat/Prot_ref.hpp>
#include <objects/seqfeat/Gene_ref.hpp>
#include <objects/seqfeat/RNA_ref.hpp>
#include <objects/seqfeat/Org_ref.hpp>
#include <objects/seqfeat/Rsite_ref.hpp>
#include <objects/seqfeat/Trna_ext.hpp>
#include <objects/seqfeat/Cdregion.hpp>
#include <objects/seqfeat/Gb_qual.hpp>
#include <objects/seqfeat/BioSource.hpp>
#include <objects/seqfeat/SubSource.hpp>
#include <objects/seqfeat/Feat_id.hpp>

#include <objects/seq/Bioseq.hpp>
#include <objects/seq/seqport_util.hpp>
#include <objects/seq/IUPACaa.hpp>
#include <objects/seq/NCBIstdaa.hpp>
#include <objects/seq/NCBIeaa.hpp>
#include <objects/seq/NCBI8aa.hpp>
#include <objects/seq/Pubdesc.hpp>
#include <objects/seq/Heterogen.hpp>

#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_loc_mix.hpp>
#include <objects/seqfeat/SeqFeatData.hpp>

#include <objects/general/Dbtag.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/general/User_object.hpp>

#include <objects/pub/Pub_equiv.hpp>
#include <objects/pub/Pub.hpp>
#include <objects/pub/Pub_set.hpp>

#include <objmgr/util/feature.hpp>
#include <objmgr/util/sequence.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
BEGIN_SCOPE(feature)
USING_SCOPE(sequence);


// Appends a label onto "label" based on the type of feature       
void s_GetTypeLabel(const CSeq_feat& feat, string* label)
{    
    string tlabel;
    
    // Determine typelabel
    CSeqFeatData::ESubtype idx = feat.GetData().GetSubtype();
    if ( idx != CSeqFeatData::eSubtype_bad ) {
        tlabel = feat.GetData().GetKey();
        if (feat.GetData().Which() == CSeqFeatData::e_Imp  && 
            tlabel != "CDS") {
            tlabel = "[" + tlabel + "]";
        } else if (feat.GetData().Which() == CSeqFeatData::e_Region  && 
            feat.GetData().GetRegion() == "Domain"  &&  feat.IsSetComment() ) {
            tlabel = "Domain";
        }
    } else if (feat.GetData().Which() == CSeqFeatData::e_Imp) {
        tlabel = "[" + feat.GetData().GetImp().GetKey() + "]";
    } else {
        tlabel = "Unknown=0";
    }
    *label += tlabel;  
}


// Appends a label onto tlabel for a CSeqFeatData::e_Cdregion
inline
static void s_GetCdregionLabel
(const CSeq_feat& feat, 
 string*          tlabel,
 CScope*          scope)
{
    // Check that tlabel exists and that the feature data is Cdregion
    if (!tlabel  ||  !feat.GetData().IsCdregion()) {
        return;
    }
    
    const CGene_ref* gref = 0;
    const CProt_ref* pref = 0;
    
    // Look for CProt_ref object to create a label from
    if (feat.IsSetXref()) {
        ITERATE ( CSeq_feat::TXref, it, feat.GetXref()) {
            const CSeqFeatXref& xref = **it;
            if ( !xref.IsSetData() ) {
                continue;
            }

            switch (xref.GetData().Which()) {
            case CSeqFeatData::e_Prot:
                pref = &xref.GetData().GetProt();
                break;
            case CSeqFeatData::e_Gene:
                gref = &xref.GetData().GetGene();
                break;
            default:
                break;
            }
        }
    }
    
    // Try and create a label from a CProt_ref in CSeqFeatXref in feature
    if (pref) {
        pref->GetLabel(tlabel);
        return;
    }
    
    // Try and create a label from a CProt_ref in the feat product and
    // return if found 
    if (feat.IsSetProduct()  &&  scope) {
        try {
            const CSeq_id& id = GetId(feat.GetProduct(), scope);            
            CBioseq_Handle hnd = scope->GetBioseqHandle(id);
            if (hnd) {
                const CBioseq& seq = *hnd.GetCompleteBioseq();
            
                // Now look for a CProt_ref feature in seq and
                // if found call GetLabel() on the CProt_ref
                CTypeConstIterator<CSeqFeatData> it = ConstBegin(seq);
                for (;it; ++it) {
                    if (it->Which() == CSeqFeatData::e_Prot) {
                        it->GetProt().GetLabel(tlabel);
                        return;
                    }
                }
            }
        } catch (CObjmgrUtilException&) {}
    }
    
    // Try and create a label from a CGene_ref in CSeqFeatXref in feature
    if (gref) {
        gref->GetLabel(tlabel);
    }

    // check to see if the CDregion is just an open reading frame
    if (feat.GetData().GetCdregion().IsSetOrf()  &&
        feat.GetData().GetCdregion().GetOrf()) {
        string str("open reading frame: ");
        switch (feat.GetData().GetCdregion().GetFrame()) {
        case CCdregion::eFrame_not_set:
            str += "frame not set; ";
            break;
        case CCdregion::eFrame_one:
            str += "frame 1; ";
            break;
        case CCdregion::eFrame_two:
            str += "frame 2; ";
            break;
        case CCdregion::eFrame_three:
            str += "frame 3; ";
            break;
        }

        switch (sequence::GetStrand(feat.GetLocation(), scope)) {
        case eNa_strand_plus:
            str += "positive strand";
            break;
        case eNa_strand_minus:
            str += "negative strand";
            break;
        case eNa_strand_both:
            str += "both strands";
            break;
        case eNa_strand_both_rev:
            str += "both strands (reverse)";
            break;
        default:
            str += "strand unknown";
            break;
        }

        *tlabel += str;
    }


}


inline
static void s_GetRnaRefLabelFromComment
(const CSeq_feat& feat, 
 string*          label,
 ELabelType       label_type,
 const string*    type_label)
{
    if (feat.IsSetComment()  &&  !feat.GetComment().empty()) {
        if (label_type == eContent  &&  type_label != 0
            &&  feat.GetComment().find(*type_label) == string::npos) {
            *label += *type_label + "-" + feat.GetComment();
        } else {
            *label += feat.GetComment();
        }
    } else if (type_label) {
        *label += *type_label;
    }
}


// Appends a label onto "label" for a CRNA_ref
inline
static void s_GetRnaRefLabel
(const CSeq_feat& feat, 
 string*          label,
 ELabelType       label_type,
 const string*    type_label)
{
    // Check that label exists and that feature data is type RNA-ref
    if (!label  ||  !feat.GetData().IsRna()) {
        return;
    }
    
    const CRNA_ref& rna = feat.GetData().GetRna();
    
    // Append the feature comment, the type label, or both  and return 
    // if Ext is not set
    if (!rna.IsSetExt()) {
        s_GetRnaRefLabelFromComment(feat, label, label_type, type_label);
        return;
    }
    
    // Append a label based on the type of the type of the ext of the
    // CRna_ref
    string tmp_label;
    switch (rna.GetExt().Which()) {
    case CRNA_ref::C_Ext::e_not_set:
        s_GetRnaRefLabelFromComment(feat, label, label_type, type_label);
        break;
    case CRNA_ref::C_Ext::e_Name:
        tmp_label = rna.GetExt().GetName();
        if (label_type == eContent  &&  type_label != 0  &&
                tmp_label.find(*type_label) == string::npos) {
            *label += *type_label + "-" + tmp_label;
        } else if (!tmp_label.empty()) {
            *label += tmp_label;
        } else if (type_label) {
            *label += *type_label;
        }
        break;
    case CRNA_ref::C_Ext::e_TRNA:
    {
        if ( !rna.GetExt().GetTRNA().IsSetAa() ) {
            s_GetRnaRefLabelFromComment(feat, label, label_type, type_label);
            break;                
        }
        try {
            CTrna_ext::C_Aa::E_Choice aa_code_type = 
                rna.GetExt().GetTRNA().GetAa().Which();
            int aa_code;
            CSeq_data in_seq, out_seq;
            string str_aa_code;
            switch (aa_code_type) {
            case CTrna_ext::C_Aa::e_Iupacaa:        
                // Convert an e_Iupacaa code to an Iupacaa3 code for the label
                aa_code = rna.GetExt().GetTRNA().GetAa().GetIupacaa();
                str_aa_code = CSeqportUtil::GetCode(CSeq_data::e_Iupacaa,
                                                    aa_code); 
                in_seq.SetIupacaa().Set() = str_aa_code;
                CSeqportUtil::Convert(in_seq, &out_seq,
                                      CSeq_data::e_Ncbistdaa);
                if (out_seq.GetNcbistdaa().Get().size()) {
                    aa_code = out_seq.GetNcbistdaa().Get()[0];
                    tmp_label = CSeqportUtil::GetIupacaa3(aa_code);
                } else {
                    s_GetRnaRefLabelFromComment(feat, label, label_type,
                                                type_label);
                }
                break;
            case CTrna_ext::C_Aa::e_Ncbieaa:
                // Convert an e_Ncbieaa code to an Iupacaa3 code for the label
                aa_code = rna.GetExt().GetTRNA().GetAa().GetNcbieaa();
                str_aa_code = CSeqportUtil::GetCode(CSeq_data::e_Ncbieaa,
                                                    aa_code);
                in_seq.SetNcbieaa().Set() = str_aa_code;
                CSeqportUtil::Convert(in_seq, &out_seq,
                                      CSeq_data::e_Ncbistdaa);
                if (out_seq.GetNcbistdaa().Get().size()) {
                    aa_code = out_seq.GetNcbistdaa().Get()[0];
                    tmp_label = CSeqportUtil::GetIupacaa3(aa_code);
                } else {
                    s_GetRnaRefLabelFromComment(feat, label, label_type,
                                                type_label);
                }
                break;
            case CTrna_ext::C_Aa::e_Ncbi8aa:
                // Convert an e_Ncbi8aa code to an Iupacaa3 code for the label
                aa_code = rna.GetExt().GetTRNA().GetAa().GetNcbi8aa();
                tmp_label = CSeqportUtil::GetIupacaa3(aa_code);
                break;
            case CTrna_ext::C_Aa::e_Ncbistdaa:
                // Convert an e_Ncbistdaa code to an Iupacaa3 code for the label
                aa_code = rna.GetExt().GetTRNA().GetAa().GetNcbistdaa();
                tmp_label = CSeqportUtil::GetIupacaa3(aa_code);
                break;
            default:
                break;
            }
        
            // Append to label, depending on ELabelType
            if (label_type == eContent  &&  type_label != 0) {
                *label += *type_label + "-" + tmp_label;
            } else if (!tmp_label.empty()) {
                *label += tmp_label;
            } else if (type_label) {
                *label += *type_label;
            }
        } catch (CSeqportUtil::CBadIndex&) {
            // fall back to comment (if any)
            s_GetRnaRefLabelFromComment(feat, label, label_type, type_label);
        }
        
        break;
    }}
}


// Appends a label to tlabel for a CImp_feat. A return value of true indicates 
// that the label was created for a CImp_feat key = "Site-ref" 
inline
static bool s_GetImpLabel
(const CSeq_feat&      feat, 
 string*               tlabel,
 ELabelType            label_type,
 const string*         type_label)
{
    // Return if tlablel does not exist or feature data is not Imp-feat
    if (!tlabel  ||  !feat.GetData().IsImp()) {
        return false;
    }
    
    const string& key = feat.GetData().GetImp().GetKey();
    bool empty = true;
    
    // If the key is Site-ref
    if (NStr::EqualNocase(key, "Site-ref")) {
        if (feat.IsSetCit()) {
            // Create label based on Pub-set
            feat.GetCit().GetLabel(tlabel);
            return true;
        }
    // else if the key is not Site-ref
    } else if (label_type == eContent) {
        // If the key is CDS
        if (NStr::EqualNocase(key, "CDS")) {
            *tlabel += "[CDS]";
        // else if the key is repeat_unit or repeat_region
        } else if (NStr::EqualNocase(key, "repeat_unit")  ||
                   NStr::EqualNocase(key, "repeat_region")) {
            if (feat.IsSetQual()) {
                // Loop thru the feature qualifiers
                ITERATE( CSeq_feat::TQual, it, feat.GetQual()) {
                    // If qualifier qual is rpt_family append qualifier val
                    if (NStr::EqualNocase((*it)->GetQual(),"rpt_family")) { 
                        *tlabel += (*it)->GetVal();
                        empty = false;
                        break;
                    }
                }
            }
            
            // If nothing has been appended yet
            if (empty) {
                *tlabel += type_label ? *type_label : string("");
            }
        // else if the key is STS
        } else if (NStr::EqualNocase(key, "STS")) {
            if (feat.IsSetQual()) {
                ITERATE( CSeq_feat::TQual, it, feat.GetQual()) {
                    if (NStr::EqualNocase((*it)->GetQual(),"standard_name"))
                    { 
                           *tlabel = (*it)->GetVal();
                           empty = false;
                           break;
                    }
                }
            }
            
            // If nothing has been appended yet
            if (empty) {
                if (feat.IsSetComment()) {
                    size_t pos = feat.GetComment().find(";");
                    if (pos == string::npos) {
                        *tlabel += feat.GetComment();
                    } else {
                        *tlabel += feat.GetComment().substr(0, pos);
                    } 
                } else {
                    *tlabel += type_label ? *type_label : string("");
                }
            }
        // else if the key is misc_feature
        } else if (NStr::EqualNocase(key, "misc_feature")) {
            if (feat.IsSetQual()) {
                // Look for a single qualifier qual in order of preference 
                // "standard_name", "function", "number", any and
                // append to tlabel and return if found
                ITERATE(CSeq_feat::TQual, it, feat.GetQual()) {
                    if (NStr::EqualNocase((*it)->GetQual(),"standard_name")) {
                        *tlabel += (*it)->GetVal();
                        return false;
                    }
                }
                ITERATE(CSeq_feat::TQual, it, feat.GetQual()) {
                    if (NStr::EqualNocase((*it)->GetQual(), "function")) {
                        *tlabel += (*it)->GetVal();
                        return false;
                    }
                }
                ITERATE(CSeq_feat::TQual, it, feat.GetQual()) {
                    if (NStr::EqualNocase((*it)->GetQual(), "number")) {
                        *tlabel += (*it)->GetVal();
                        return false;
                    }
                }
                ITERATE(CSeq_feat::TQual, it, feat.GetQual()) {
                    *tlabel += (*it)->GetVal();
                    return false;
                }
                // Append type_label if there is one
                if (empty) {
                    *tlabel += type_label ? *type_label : string("");
                    return false;
                }
            }
        }
    } 
    return false;                
}

 
// Return a label based on the content of the feature
void s_GetContentLabel
(const CSeq_feat&      feat,
 string*               label,
 const string*         type_label,
 ELabelType            label_type, 
 CScope*               scope)
{
    string tlabel;
    
    // Get a content label dependent on the type of the feature data
    switch (feat.GetData().Which()) {
    case CSeqFeatData::e_Gene:
        feat.GetData().GetGene().GetLabel(&tlabel);
        break;
    case CSeqFeatData::e_Org:
        feat.GetData().GetOrg().GetLabel(&tlabel);
        break;
    case CSeqFeatData::e_Cdregion:
        s_GetCdregionLabel(feat, &tlabel, scope);
        break;
    case CSeqFeatData::e_Prot:
        feat.GetData().GetProt().GetLabel(&tlabel);
        break;
    case CSeqFeatData::e_Rna:
        s_GetRnaRefLabel(feat, &tlabel, label_type, type_label);
        break;  
    case CSeqFeatData::e_Pub:
        feat.GetData().GetPub().GetPub().GetLabel(&tlabel); 
        break;
    case CSeqFeatData::e_Seq:
        break;
    case CSeqFeatData::e_Imp:
        if (s_GetImpLabel(feat, &tlabel, label_type, type_label)) {
            *label += tlabel;
            return;
        }
        break;
    case CSeqFeatData::e_Region:
        if (feat.GetData().GetRegion().find("Domain") != string::npos  && 
                feat.IsSetComment()) {
            tlabel += feat.GetComment();
        } else {
            tlabel += feat.GetData().GetRegion();
        }
        break;
    case CSeqFeatData::e_Comment:
        tlabel += feat.IsSetComment() ? feat.GetComment() : string("");
        break;
    case CSeqFeatData::e_Bond:
        // Get the ASN string name for the enumerated EBond type
        tlabel += CSeqFeatData::GetTypeInfo_enum_EBond()
            ->FindName(feat.GetData().GetBond(), true);
        break;
    case CSeqFeatData::e_Site:
        // Get the ASN string name for the enumerated ESite type
        tlabel += CSeqFeatData::GetTypeInfo_enum_ESite()
            ->FindName(feat.GetData().GetSite(), true);
        break;
    case CSeqFeatData::e_Rsite:
        switch (feat.GetData().GetRsite().Which()) {
        case CRsite_ref::e_Str:
            tlabel += feat.GetData().GetRsite().GetStr();
            break;
        case CRsite_ref::e_Db:
            tlabel += feat.GetData().GetRsite().GetDb().GetTag().IsStr() ?
                feat.GetData().GetRsite().GetDb().GetTag().GetStr() : 
                string("?");
            break;
        default:
            break;
        }
        break;
    case CSeqFeatData::e_User:
        if (feat.GetData().GetUser().IsSetClass()) {
            tlabel += feat.GetData().GetUser().GetClass();
        } else if (feat.GetData().GetUser().GetType().IsStr()) {
            tlabel += feat.GetData().GetUser().GetType().GetStr();
        }
    case CSeqFeatData::e_Txinit:
        break;
    case CSeqFeatData::e_Num:
        break;
    case CSeqFeatData::e_Psec_str:
        tlabel += CSeqFeatData::GetTypeInfo_enum_EPsec_str()
            ->FindName(feat.GetData().GetPsec_str(), true);
        break;    
    case CSeqFeatData::e_Non_std_residue:
        tlabel += feat.GetData().GetNon_std_residue();
        break;
    case CSeqFeatData::e_Het:
        tlabel += feat.GetData().GetHet().Get();
        break;        
    case CSeqFeatData::e_Biosrc:
        {{
            const CBioSource& biosrc = feat.GetData().GetBiosrc();
            string str;
            if (biosrc.IsSetSubtype()) {
                ITERATE (CBioSource::TSubtype, iter, biosrc.GetSubtype()) {
                    if ( !str.empty() ) {
                        str += "; ";
                    }
                    (*iter)->GetLabel(&str);
                }
            }
            if (str.empty()) {
                feat.GetData().GetBiosrc().GetOrg().GetLabel(&str);
            } else {
                str += " (";
                feat.GetData().GetBiosrc().GetOrg().GetLabel(&str);
                str += ")";
            }
            tlabel += str;
        }}
        break;        
    default:
        break;
    }
    
    // Return if a label has been calculated above
    if (!tlabel.empty()) {
        *label += tlabel;
        return;
    }
    
    // Put Seq-feat qual into label
    if (feat.IsSetQual()) {
        string prefix("/");
        ITERATE(CSeq_feat::TQual, it, feat.GetQual()) {
            tlabel += prefix + (**it).GetQual();
            prefix = " ";
            if (!(**it).GetVal().empty()) {
                tlabel += "=" + (**it).GetVal();
            }
        }
    }
    
    // Put Seq-feat comment into label
    if (feat.IsSetComment()) {
        if (tlabel.empty()) {
            tlabel = feat.GetComment();
        } else {
            tlabel += "; " + feat.GetComment();
        }
    }
    
    *label += tlabel;
}


void GetLabel
(const CSeq_feat&    feat,
 string*             label,
 ELabelType          label_type,
 CScope*             scope)
{
 
    // Ensure that label exists
    if (!label) {
        return;
    }
    
    // Get the type label
    string type_label;
    s_GetTypeLabel(feat, &type_label);
    
    // Append the type label and return if content label not required
    if (label_type == eBoth) {
        *label += type_label + ": ";
    } else if (label_type == eType) {
        *label += type_label;
        return;
    }
    
    // Append the content label
    size_t label_len = label->size();
    s_GetContentLabel(feat, label, &type_label, label_type, scope);
    
    // If there is no content label, append the type label
    if (label->size() == label_len  &&  label_type == eContent) {
        *label += type_label;
    }
}


void CFeatIdRemapper::Reset(void)
{
    m_IdMap.clear();
}


size_t CFeatIdRemapper::GetFeatIdsCount(void) const
{
    return m_IdMap.size();
}


int CFeatIdRemapper::RemapId(int old_id, const CTSE_Handle& tse)
{
    TFullId key(old_id, tse);
    int& new_id = m_IdMap[key];
    if ( !new_id ) {
        new_id = m_IdMap.size();
    }
    return new_id;
}


bool CFeatIdRemapper::RemapId(CFeat_id& id, const CTSE_Handle& tse)
{
    bool mapped = false;
    if ( id.IsLocal() ) {
        CObject_id& local = id.SetLocal();
        if ( local.IsId() ) {
            int old_id = local.GetId();
            int new_id = RemapId(old_id, tse);
            if ( new_id != old_id ) {
                mapped = true;
                local.SetId(new_id);
            }
        }
    }
    return mapped;
}


bool CFeatIdRemapper::RemapId(CFeat_id& id, const CFeat_CI& feat_it)
{
    bool mapped = false;
    if ( id.IsLocal() ) {
        CObject_id& local = id.SetLocal();
        if ( local.IsId() ) {
            int old_id = local.GetId();
            int new_id = RemapId(old_id, feat_it.GetAnnot().GetTSE_Handle());
            if ( new_id != old_id ) {
                mapped = true;
                local.SetId(new_id);
            }
        }
    }
    return mapped;
}


bool CFeatIdRemapper::RemapIds(CSeq_feat& feat, const CTSE_Handle& tse)
{
    bool mapped = false;
    if ( feat.IsSetId() ) {
        if ( RemapId(feat.SetId(), tse) ) {
            mapped = true;
        }
    }
    if ( feat.IsSetXref() ) {
        NON_CONST_ITERATE ( CSeq_feat::TXref, it, feat.SetXref() ) {
            CSeqFeatXref& xref = **it;
            if ( xref.IsSetId() && RemapId(xref.SetId(), tse) ) {
                mapped = true;
            }
        }
    }
    return mapped;
}


CRef<CSeq_feat> CFeatIdRemapper::RemapIds(const CFeat_CI& feat_it)
{
    CRef<CSeq_feat> feat(SerialClone(feat_it->GetMappedFeature()));
    if ( feat->IsSetId() ) {
        RemapId(feat->SetId(), feat_it);
    }
    if ( feat->IsSetXref() ) {
        NON_CONST_ITERATE ( CSeq_feat::TXref, it, feat->SetXref() ) {
            CSeqFeatXref& xref = **it;
            if ( xref.IsSetId() ) {
                RemapId(xref.SetId(), feat_it);
            }
        }
    }
    return feat;
}


bool CFeatComparatorByLabel::Less(const CSeq_feat& f1,
                                  const CSeq_feat& f2,
                                  CScope* scope)
{
    string l1, l2;
    GetLabel(f1, &l1, eBoth, scope);
    GetLabel(f2, &l2, eBoth, scope);
    return l1 < l2;
}


CMappedFeat MapSeq_feat(const CSeq_feat_Handle& feat,
                        const CBioseq_Handle& master_seq,
                        const CRange<TSeqPos>& range)
{
    SAnnotSelector sel(feat.GetFeatSubtype());
    sel.SetExactDepth();
    sel.SetResolveAll();
    CSeq_annot_Handle annot = feat.GetAnnot();
    sel.SetLimitSeqAnnot(annot);
    sel.SetSourceLoc(feat.GetOriginalSeq_feat()->GetLocation());
    for ( size_t depth = 0; depth < 10; ++depth ) {
        sel.SetResolveDepth(depth);
        for ( CFeat_CI it(master_seq, range, sel); it; ++it ) {
            if ( it->GetSeq_feat_Handle() == feat ) {
                return *it;
            }
        }
    }
    NCBI_THROW(CObjMgrException, eFindFailed,
               "MapSeq_feat: feature not found");
}


NCBI_XOBJUTIL_EXPORT
CMappedFeat MapSeq_feat(const CSeq_feat_Handle& feat,
                        const CSeq_id_Handle& master_id,
                        const CRange<TSeqPos>& range)
{
    CBioseq_Handle master_seq = feat.GetScope().GetBioseqHandle(master_id);
    if ( !master_seq ) {
        NCBI_THROW(CObjmgrUtilException, eBadLocation,
                   "MapSeq_feat: master sequence not found");
    }
    return MapSeq_feat(feat, master_seq, range);
}


NCBI_XOBJUTIL_EXPORT
CMappedFeat MapSeq_feat(const CSeq_feat_Handle& feat,
                        const CBioseq_Handle& master_seq)
{
    return MapSeq_feat(feat, master_seq, CRange<TSeqPos>::GetWhole());
}


NCBI_XOBJUTIL_EXPORT
CMappedFeat MapSeq_feat(const CSeq_feat_Handle& feat,
                        const CSeq_id_Handle& master_id)
{
    CBioseq_Handle master_seq = feat.GetScope().GetBioseqHandle(master_id);
    if ( !master_seq ) {
        NCBI_THROW(CObjmgrUtilException, eBadLocation,
                   "MapSeq_feat: master sequence not found");
    }
    return MapSeq_feat(feat, master_seq);
}


namespace {
    // Checks if the location has mixed strands or wrong order of intervals
    static
    bool sx_IsIrregularLocation(const CSeq_loc& loc)
    {
        try {
            // simple locations are regular
            if ( !loc.IsMix() ) {
                return false;
            }
            
            if ( !loc.GetId() ) {
                // multiple ids locations are irregular
                return true;
            }
            
            ENa_strand strand = loc.GetStrand();
            if ( strand == eNa_strand_other ) {
                // mixed strands
                return true;
            }

            bool plus_strand = !IsReverse(strand);
            TSeqPos pos = plus_strand? 0: kInvalidSeqPos;
            bool stop = false;
            
            const CSeq_loc_mix& mix = loc.GetMix();
            ITERATE ( CSeq_loc_mix::Tdata, it, mix.Get() ) {
                const CSeq_loc& loc1 = **it;
                if ( sx_IsIrregularLocation(loc1) ) {
                    return true;
                }
                CRange<TSeqPos> range = loc1.GetTotalRange();
                if ( range.Empty() ) {
                    continue;
                }
                if ( stop ) {
                    return true;
                }
                if ( plus_strand ) {
                    if ( range.GetFrom() < pos ) {
                        return true;
                    }
                    pos = range.GetTo()+1;
                    stop = pos == 0;
                }
                else {
                    if ( range.GetTo() > pos ) {
                        return true;
                    }
                    pos = range.GetFrom();
                    stop = pos == 0;
                    --pos;
                }
            }
            
            return false;
        }
        catch ( CException& ) {
            // something's wrong -> irregular
            return true;
        }
    }


    static
    CSeqFeatData::ESubtype sx_GetParentType(CSeqFeatData::ESubtype subtype)
    {
        switch ( subtype ) {
        case CSeqFeatData::eSubtype_operon:
        case CSeqFeatData::eSubtype_gap:
            // operon and gap features do not inherit anything
            return CSeqFeatData::eSubtype_bad;
        case CSeqFeatData::eSubtype_gene:
            // Gene features can inherit operon by overlap (CONTAINED_WITHIN)
            return CSeqFeatData::eSubtype_operon;
        case CSeqFeatData::eSubtype_mat_peptide:
        case CSeqFeatData::eSubtype_sig_peptide:
            return CSeqFeatData::eSubtype_prot;
        case CSeqFeatData::eSubtype_cdregion:
            return CSeqFeatData::eSubtype_mRNA;
        case CSeqFeatData::eSubtype_prot:
            return CSeqFeatData::eSubtype_cdregion;
        default:
            return CSeqFeatData::eSubtype_gene;
        }
    }


    static
    CMappedFeat sx_GetParentByRef(const CMappedFeat& feat,
                                  CSeqFeatData::ESubtype parent_type)
    {
        if ( !feat.IsSetXref() ) {
            return CMappedFeat();
        }

        CTSE_Handle tse = feat.GetAnnot().GetTSE_Handle();
        const CSeq_feat::TXref& xrefs = feat.GetXref();
        ITERATE ( CSeq_feat::TXref, it, xrefs ) {
            const CSeqFeatXref& xref = **it;
            if ( xref.IsSetId() ) {
                const CFeat_id& id = xref.GetId();
                if ( id.IsLocal() ) {
                    const CObject_id& obj_id = id.GetLocal();
                    if ( obj_id.IsId() ) {
                        CSeq_feat_Handle feat1 =
                            tse.GetFeatureWithId(parent_type, obj_id.GetId());
                        if ( feat1 ) {
                            return feat1;
                        }
                    }
                }
            }
            if ( parent_type == CSeqFeatData::eSubtype_gene &&
                 xref.IsSetData() ) {
                const CSeqFeatData& data = xref.GetData();
                if ( data.IsGene() ) {
                    CSeq_feat_Handle feat1 = tse.GetGeneByRef(data.GetGene());
                    if ( feat1 ) {
                        return feat1;
                    }
                }
            }
        }
        return CMappedFeat();
    }


    static inline
    bool sx_ByProduct(CSeqFeatData::ESubtype feature_type)
    {
        return feature_type == CSeqFeatData::eSubtype_prot;
    }


    static inline
    bool sx_InheritParent(CSeqFeatData::ESubtype parent_type)
    {
        return parent_type != CSeqFeatData::eSubtype_gene;
    }

    
    static
    CMappedFeat sx_GetParentByOverlap(const CMappedFeat& feat,
                                      bool by_product,
                                      CSeqFeatData::ESubtype parent_type)
    {
        CMappedFeat best_parent;

        const CSeq_loc& c_loc = feat.GetLocation();

        // find best suitable parent by overlap score
        EOverlapType overlap_type = eOverlap_Contained;
        if ( parent_type == CSeqFeatData::eSubtype_gene &&
             sx_IsIrregularLocation(c_loc) ) {
            // LOCATION_SUBSET if bad order or mixed strand
            // otherwise CONTAINED_WITHIN
            overlap_type = eOverlap_Subset;
        }
    
        Int8 best_overlap = kMax_I8;
        SAnnotSelector sel(parent_type);
        sel.SetByProduct(by_product);
        for (CFeat_CI it(feat.GetScope(), c_loc, sel); it; ++it) {
            Int8 overlap = TestForOverlap64(it->GetLocation(),
                                            c_loc,
                                            overlap_type,
                                            kInvalidSeqPos,
                                            &feat.GetScope());
            if ( overlap >= 0 && overlap < best_overlap ) {
                best_parent = *it;
                best_overlap = overlap;
            }
        }
        return best_parent;
    }
}


NCBI_XOBJUTIL_EXPORT
CMappedFeat GetParentFeature(const CMappedFeat& feat)
{
    CMappedFeat best_parent;

    CSeqFeatData::ESubtype current_type = feat.GetFeatSubtype();
    while ( current_type != CSeqFeatData::eSubtype_bad ) {
        CSeqFeatData::ESubtype parent_type = sx_GetParentType(current_type);
        if ( parent_type == CSeqFeatData::eSubtype_bad ) {
            // no parent is possible
            break;
        }

        best_parent = sx_GetParentByRef(feat, parent_type);
        if ( best_parent ) {
            // found by Xref
            break;
        }

        bool by_product = sx_ByProduct(current_type);
        best_parent = sx_GetParentByOverlap(feat, by_product, parent_type);
        if ( best_parent ) {
            // parent is found by overlap
            break;
        }

        if ( by_product || !sx_InheritParent(parent_type) ) {
            // no way to link proteins without cdregion
            break;
        }
        current_type = parent_type;
    }
    return best_parent;
}


CFeatTree::CFeatTree(void)
    : m_AssignedParents(0)
{
}


CFeatTree::CFeatTree(CFeat_CI it)
    : m_AssignedParents(0)
{
    AddFeatures(it);
}


CFeatTree::~CFeatTree(void)
{
}


void CFeatTree::AddFeatures(CFeat_CI it)
{
    for ( ; it; ++it ) {
        AddFeature(*it);
    }
}


void CFeatTree::AddFeature(const CMappedFeat& feat)
{
    m_InfoMap[feat.GetSeq_feat_Handle()].m_Feat = feat;
}


CFeatTree::CFeatInfo& CFeatTree::x_GetInfo(const CMappedFeat& feat)
{
    return x_GetInfo(feat.GetSeq_feat_Handle());
}


CFeatTree::CFeatInfo& CFeatTree::x_GetInfo(const CSeq_feat_Handle& feat)
{
    TInfoMap::iterator it = m_InfoMap.find(feat);
    if ( it == m_InfoMap.end() ) {
        NCBI_THROW(CObjMgrException, eFindFailed,
                   "CFeatTree: feature not found");
    }
    return it->second;
}


CFeatTree::CFeatInfo* CFeatTree::x_FindInfo(const CSeq_feat_Handle& feat)
{
    TInfoMap::iterator it = m_InfoMap.find(feat);
    if ( it == m_InfoMap.end() ) {
        return 0;
    }
    return &it->second;
}


namespace {
    struct SFeatRangeInfo {
        CSeq_id_Handle m_Id;
        CRange<TSeqPos> m_Range;
        CFeatTree::CFeatInfo* m_Info;
        
        // min start coordinate for all entries after this
        TSeqPos m_MinFrom;

        SFeatRangeInfo(CFeatTree::CFeatInfo& info, bool by_product = false)
            : m_Info(&info)
            {
                if ( by_product ) {
                    m_Id = info.m_Feat.GetProductId();
                    if ( m_Id ) {
                        m_Range = info.m_Feat.GetProductTotalRange();
                    }
                }
                else {
                    m_Id = info.m_Feat.GetLocationId();
                    if ( m_Id ) {
                        m_Range = info.m_Feat.GetLocationTotalRange();
                    }
                }
            }
        SFeatRangeInfo(CFeatTree::CFeatInfo& info,
                       CHandleRangeMap::const_iterator it)
            : m_Id(it->first),
              m_Range(it->second.GetOverlappingRange()),
              m_Info(&info)
            {
            }
    };
    struct PLessByStart {
        // sort first by start coordinate, then by end coordinate
        bool operator()(const SFeatRangeInfo& a, const SFeatRangeInfo& b) const
            {
                return a.m_Id < b.m_Id || a.m_Id == b.m_Id &&
                    a.m_Range < b.m_Range;
            }
    };
    struct PLessByEnd {
        // sort first by end coordinate, then by start coordinate
        bool operator()(const SFeatRangeInfo& a, const SFeatRangeInfo& b) const
            {
                return a.m_Id < b.m_Id || a.m_Id == b.m_Id &&
                    (a.m_Range.GetToOpen() < b.m_Range.GetToOpen() ||
                     a.m_Range.GetToOpen() == b.m_Range.GetToOpen() &&
                     a.m_Range.GetFrom() < b.m_Range.GetFrom());
            }
    };

    void s_AddRanges(vector<SFeatRangeInfo>& rr,
                     CFeatTree::CFeatInfo& info,
                     const CSeq_loc& loc)
    {
        CHandleRangeMap hrmap;
        hrmap.AddLocation(loc);
        ITERATE ( CHandleRangeMap, it, hrmap ) {
            SFeatRangeInfo range_info(info, it);
            rr.push_back(range_info);
        }
    }
}


void CFeatTree::x_CollectNeeded(TParentInfoMap& pinfo_map)
{
    // collect all necessary parent candidates
    NON_CONST_ITERATE ( TInfoMap, it, m_InfoMap ) {
        CFeatInfo& info = it->second;
        CSeqFeatData::ESubtype feat_type = info.m_Feat.GetFeatSubtype();
        SFeatSet& feat_set = pinfo_map[feat_type];
        if ( feat_set.m_NeedAll && !feat_set.m_CollectedAll ) {
            feat_set.m_All.push_back(&info);
        }
    }
    // mark collected 
    NON_CONST_ITERATE ( TParentInfoMap, it, pinfo_map ) {
        SFeatSet& feat_set = *it;
        if ( feat_set.m_NeedAll && !feat_set.m_CollectedAll ) {
            feat_set.m_CollectedAll = true;
        }
    }
}


void CFeatTree::x_AssignParentsByRef(TFeatArray& features,
                                     CSeqFeatData::ESubtype parent_type)
{
    if ( features.empty() || parent_type == CSeqFeatData::eSubtype_bad ) {
        return;
    }
    TFeatArray::iterator dst = features.begin();
    ITERATE ( TFeatArray, it, features ) {
        CFeatInfo& info = **it;
        if ( info.IsSetParent() ) {
            continue;
        }
        if ( !info.m_Feat.IsSetXref() ) {
            *dst++ = *it;
            continue;
        }
        CTSE_Handle tse = info.GetTSE();
        const CSeq_feat::TXref& xrefs = info.m_Feat.GetXref();
        CFeatInfo* gene_parent = 0;
        ITERATE ( CSeq_feat::TXref, xit, xrefs ) {
            const CSeqFeatXref& xref = **xit;
            if ( xref.IsSetId() ) {
                const CFeat_id& id = xref.GetId();
                if ( !id.IsLocal() ) {
                    continue;
                }
                const CObject_id& obj_id = id.GetLocal();
                if ( obj_id.IsId() ) {
                    vector<CSeq_feat_Handle> ff =
                        tse.GetFeaturesWithId(parent_type, obj_id.GetId());
                    ITERATE ( vector<CSeq_feat_Handle>, fit, ff ) {
                        CFeatInfo* p_info = x_FindInfo(*fit);
                        if ( p_info ) {
                            x_SetParent(info, *p_info);
                            break;
                        }
                        else {
                            x_SetNoParent(info);
                            break;
                        }
                    }
                    if ( info.IsSetParent() ) {
                        break;
                    }
                }
            }
            if ( parent_type == CSeqFeatData::eSubtype_gene &&
                 !gene_parent && xref.IsSetData() ) {
                const CSeqFeatData& data = xref.GetData();
                if ( data.IsGene() ) {
                    vector<CSeq_feat_Handle> ff =
                        tse.GetGenesByRef(data.GetGene());
                    ITERATE ( vector<CSeq_feat_Handle>, fit, ff ) {
                        CFeatInfo* p_info = x_FindInfo(*fit);
                        if ( p_info ) {
                            gene_parent = p_info;
                            break;
                        }
                    }
                }
            }
        }
        if ( !info.IsSetParent() ) {
            if ( gene_parent ) {
                x_SetParent(info, *gene_parent);
            }
            else {
                *dst++ = *it;
            }
        }
    }
    features.erase(dst, features.end());
}


void CFeatTree::x_AssignParentsByOverlap(TFeatArray& features,
                                         bool by_product,
                                         TFeatArray& parents)
{
    if ( features.empty() || parents.empty() ) {
        return;
    }

    typedef vector<SFeatRangeInfo> TRangeArray;
    TRangeArray pp, cc;

    // collect parents parameters
    ITERATE ( TFeatArray, it, parents ) {
        CFeatInfo& feat_info = **it;
        SFeatRangeInfo range_info(feat_info, by_product);
        if ( range_info.m_Id ) {
            pp.push_back(range_info);
        }
        else {
            s_AddRanges(pp, feat_info,
                        by_product?
                        feat_info.m_Feat.GetProduct():
                        feat_info.m_Feat.GetLocation());
        }
    }
    sort(pp.begin(), pp.end(), PLessByEnd());

    // collect children parameters
    ITERATE ( TFeatArray, it, features ) {
        CFeatInfo& feat_info = **it;
        SFeatRangeInfo range_info(feat_info);
        if ( range_info.m_Id ) {
            cc.push_back(range_info);
        }
        else {
            s_AddRanges(cc, feat_info, feat_info.m_Feat.GetLocation());
        }
    }
    sort(cc.begin(), cc.end(), PLessByStart());

    CSeqFeatData::ESubtype parent_type = parents[0]->m_Feat.GetFeatSubtype();
    // assign parents in single scan over both lists
    {{
        TRangeArray::iterator pi = pp.begin();
        TRangeArray::iterator ci = cc.begin();
        for ( ; ci != cc.end(); ) {
            // skip all parents with Seq-ids smaller than first child
            while ( pi != pp.end() && pi->m_Id < ci->m_Id ) {
                ++pi;
            }
            if ( pi == pp.end() ) { // no more parents
                break;
            }
            const CSeq_id_Handle& cur_id = pi->m_Id;
            if ( ci->m_Id < cur_id || !ci->m_Id ) {
                // skip all children with Seq-ids smaller than first parent
                do {
                    ++ci;
                } while ( ci != cc.end() && (ci->m_Id < cur_id || !ci->m_Id) );
                continue;
            }

            // find end of Seq-id parents
            TRangeArray::iterator pe = pi;
            while ( pe != pp.end() && pe->m_Id == cur_id ) {
                ++pe;
            }

            {{
                // update parents' m_MinFrom on the Seq-id
                TRangeArray::iterator i = pe;
                TSeqPos min_from = (--i)->m_Range.GetFrom();
                i->m_MinFrom = min_from;
                while ( i != pi ) {
                    min_from = min(min_from, (--i)->m_Range.GetFrom());
                    i->m_MinFrom = min_from;
                }
            }}

            // scan all Seq-id children
            for ( ; ci != cc.end() && pi != pe && ci->m_Id == cur_id; ++ci ) {
                // child parameters
                CFeatInfo& info = *ci->m_Info;
                const CSeq_loc& c_loc = info.m_Feat.GetLocation();
                EOverlapType overlap_type = eOverlap_Contained;
                if ( parent_type == CSeqFeatData::eSubtype_gene &&
                     sx_IsIrregularLocation(c_loc) ) {
                    // LOCATION_SUBSET if bad order or mixed strand
                    // otherwise CONTAINED_WITHIN
                    overlap_type = eOverlap_Subset;
                }

                // skip non-overlapping parents
                while ( pi != pe &&
                        pi->m_Range.GetToOpen() < ci->m_Range.GetFrom() ) {
                    ++pi;
                }
            
                // scan parent candidates
                for ( TRangeArray::iterator pc = pi;
                      pc != pe && pc->m_MinFrom < ci->m_Range.GetToOpen();
                      ++pc ) {
                    if ( !pc->m_Range.IntersectingWith(ci->m_Range) ) {
                        continue;
                    }
                    const CMappedFeat& p_feat = pc->m_Info->m_Feat;
                    const CSeq_loc& p_loc =
                        by_product?
                        p_feat.GetProduct():
                        p_feat.GetLocation();
                    Int8 overlap = TestForOverlap64(p_loc,
                                                    c_loc,
                                                    overlap_type,
                                                    kInvalidSeqPos,
                                                    &p_feat.GetScope());
                    if ( overlap >= 0 && overlap < info.m_ParentOverlap ) {
                        info.m_ParentOverlap = overlap;
                        info.m_Parent = pc->m_Info;
                    }
                }
            }
            // skip remaining Seq-id children
            for ( ; ci != cc.end() && ci->m_Id == cur_id; ++ci ) {
            }
        }
    }}
    // assign found parents
    TFeatArray::iterator dst = features.begin();
    ITERATE ( TFeatArray, it, features ) {
        CFeatInfo& info = **it;
        if ( !info.IsSetParent() ) {
            if ( info.m_Parent && info.m_ParentOverlap < kMax_I8 ) {
                // assign best parent
                CFeatInfo* p_info = info.m_Parent;
                info.m_Parent = 0;
                x_SetParent(info, *p_info);
            }
            else {
                // store for future processing
                *dst++ = &info;
            }
        }
    }
    // remove all features with assigned parents
    features.erase(dst, features.end());
}


void CFeatTree::x_AssignParents(void)
{
    if ( m_AssignedParents >= m_InfoMap.size() ) {
        return;
    }
    
    // collect all features without assigned parent
    TParentInfoMap pinfo_map(CSeqFeatData::eSubtype_max+1);
    size_t new_count = 0;
    NON_CONST_ITERATE ( TInfoMap, it, m_InfoMap ) {
        CFeatInfo& info = it->second;
        if ( info.IsSetParent() ) {
            continue;
        }
        CSeqFeatData::ESubtype feat_type = info.m_Feat.GetFeatSubtype();
        CSeqFeatData::ESubtype parent_type = sx_GetParentType(feat_type);
        if ( parent_type == CSeqFeatData::eSubtype_bad ) {
            // no parent
            x_SetNoParent(info);
        }
        else {
            SFeatSet& feat_set = pinfo_map[feat_type];
            if ( feat_set.m_New.empty() ) {
                feat_set.m_FeatType = feat_type;
                feat_set.m_ParentType = parent_type;
                pinfo_map[parent_type].m_NeedAll = true;
            }
            feat_set.m_New.push_back(&info);
            ++new_count;
        }
    }
    if ( new_count == 0 ) { // no work to do
        return;
    }
    // collect all necessary parent candidates
    x_CollectNeeded(pinfo_map);
    // assign parents for each parent type
    NON_CONST_ITERATE ( TParentInfoMap, it1, pinfo_map ) {
        SFeatSet& feat_set = *it1;
        if ( feat_set.m_New.empty() ) {
            // no work to do
            continue;
        }
        CSeqFeatData::ESubtype current_type = feat_set.m_FeatType;
        while ( current_type != CSeqFeatData::eSubtype_bad ) {
            CSeqFeatData::ESubtype parent_type=sx_GetParentType(current_type);
            if ( parent_type == CSeqFeatData::eSubtype_bad ) {
                // no parent is possible
                break;
            }
            x_AssignParentsByRef(feat_set.m_New, parent_type);
            if ( feat_set.m_New.empty() ) {
                break;
            }
            if ( !pinfo_map[parent_type].m_CollectedAll ) {
                pinfo_map[parent_type].m_NeedAll = true;
                x_CollectNeeded(pinfo_map);
            }
            _ASSERT(pinfo_map[parent_type].m_CollectedAll);
            bool by_product = sx_ByProduct(current_type);
            x_AssignParentsByOverlap(feat_set.m_New,
                                     by_product,
                                     pinfo_map[parent_type].m_All);
            if ( feat_set.m_New.empty() ) {
                break;
            }
            if ( by_product || !sx_InheritParent(parent_type) ) {
                // no way to link proteins without cdregion
                break;
            }
            current_type = parent_type;
        }
        // all remaining features are without parent
        ITERATE ( TFeatArray, it, feat_set.m_New ) {
            x_SetNoParent(**it);
        }
    }

    m_AssignedParents = m_InfoMap.size();
}


void CFeatTree::x_SetParent(CFeatInfo& info, CFeatInfo& parent)
{
    _ASSERT(!info.IsSetParent());
    _ASSERT(!info.m_Parent);
    _ASSERT(!parent.m_IsSetChildren);
    parent.m_Children.push_back(&info);
    info.m_Parent = &parent;
    info.m_IsSetParent = true;
}


void CFeatTree::x_SetNoParent(CFeatInfo& info)
{
    _ASSERT(!info.IsSetParent());
    _ASSERT(!info.m_Parent);
    m_RootInfo.m_Children.push_back(&info);
    info.m_IsSetParent = true;
}


CFeatTree::CFeatInfo* CFeatTree::x_GetParent(CFeatInfo& info)
{
    if ( !info.IsSetParent() ) {
        x_AssignParents();
    }
    return info.m_Parent;
}


const CFeatTree::TChildren& CFeatTree::x_GetChildren(CFeatInfo& info)
{
    x_AssignParents();
    return info.m_Children;
}


CMappedFeat CFeatTree::GetParent(const CMappedFeat& feat)
{
    CMappedFeat ret;
    CFeatInfo* info = x_GetParent(x_GetInfo(feat));
    if ( info ) {
        ret = info->m_Feat;
    }
    return ret;
}


CMappedFeat CFeatTree::GetParent(const CMappedFeat& feat,
                                 CSeqFeatData::E_Choice type)
{
    CMappedFeat parent = GetParent(feat);
    while ( parent && parent.GetFeatType() != type ) {
        parent = GetParent(parent);
    }
    return parent;
}


CMappedFeat CFeatTree::GetParent(const CMappedFeat& feat,
                                 CSeqFeatData::ESubtype subtype)
{
    CMappedFeat parent = GetParent(feat);
    while ( parent && parent.GetFeatSubtype() != subtype ) {
        parent = GetParent(parent);
    }
    return parent;
}


vector<CMappedFeat> CFeatTree::GetChildren(const CMappedFeat& feat)
{
    vector<CMappedFeat> children;
    GetChildrenTo(feat, children);
    return children;
}


void CFeatTree::GetChildrenTo(const CMappedFeat& feat,
                              vector<CMappedFeat>& children)
{
    children.clear();
    const TChildren* infos;
    if ( feat ) {
        infos = &x_GetChildren(x_GetInfo(feat));
    }
    else {
        x_AssignParents();
        infos = &m_RootInfo.m_Children;
    }
    children.reserve(infos->size());
    ITERATE ( TChildren, it, *infos ) {
        children.push_back((*it)->m_Feat);
    }
}


CFeatTree::CFeatInfo::CFeatInfo(void)
    : m_IsSetParent(false),
      m_IsSetChildren(false),
      m_Parent(0),
      m_ParentOverlap(kMax_I8)
{
}


CFeatTree::CFeatInfo::~CFeatInfo(void)
{
}


const CTSE_Handle& CFeatTree::CFeatInfo::GetTSE(void) const
{
    return m_Feat.GetAnnot().GetTSE_Handle();
}


END_SCOPE(feature)
END_SCOPE(objects)
END_NCBI_SCOPE
