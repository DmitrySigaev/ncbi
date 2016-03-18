/*  $Id$
 * =========================================================================
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
 * =========================================================================
 *
 * Author:  Colleen Bollin
 *
 * File Description:
 *   class for storing results for Discrepancy Report
 *
 * $Log:$ 
 */

#include <ncbi_pch.hpp>

#include "discrepancy_core.hpp"
#include <misc/discrepancy/report_object.hpp>
#include <objects/general/Date.hpp>
#include <objects/general/Dbtag.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/general/User_object.hpp>
#include <objects/misc/sequence_util_macros.hpp>
#include <objects/pub/Pub_equiv.hpp>
#include <objects/seq/Pubdesc.hpp>
#include <objects/seq/Seqdesc.hpp>
#include <objects/seqfeat/Org_ref.hpp>
#include <objects/seqloc/Packed_seqpnt.hpp>
#include <objects/seqloc/Seq_bond.hpp>
#include <objects/seqloc/Seq_loc_equiv.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seqloc/Seq_loc_mix.hpp>
#include <objects/seqloc/Seq_point.hpp>
#include <objmgr/seq_vector.hpp>
#include <objmgr/util/feature.hpp>
#include <objmgr/util/sequence.hpp>
#include <objtools/edit/apply_object.hpp>

BEGIN_NCBI_SCOPE;
BEGIN_SCOPE(NDiscrepancy)

USING_NCBI_SCOPE;
USING_SCOPE(objects);

CConstRef<CSeq_id> GetBestId(const CBioseq&, CSeq_id::E_Choice);

void CReportObject::SetText(CScope& scope)
{
    if (m_Bioseq) {
        CBioseq_Handle bsh = scope.GetBioseqHandle(*m_Bioseq);
        m_Text = GetTextObjectDescription(bsh);
        GetBestId(*bsh.GetCompleteBioseq(), CSeq_id::e_Genbank)->GetLabel(&m_ShortName, CSeq_id::eContent);
    }
    else if (m_Seq_feat) {
        m_Text = GetTextObjectDescription(*m_Seq_feat, scope);
    }
    else if (m_Seqdesc) {
        m_Text = GetTextObjectDescription(*m_Seqdesc, scope);
    }
    else if (m_Bioseq_set) {
        CBioseq_set_Handle bssh = scope.GetBioseq_setHandle(*m_Bioseq_set);
        m_Text = GetTextObjectDescription(bssh);
    }

    if (!NStr::IsBlank(m_Filename)) {
        m_Text = m_Filename + ":" + m_Text;
    }
}


void CReportObject::SetFeatureTable(CScope& scope)
{
    m_FeatureTable = "feature table format display of object";
}


void CReportObject::SetXML(CScope& scope)
{
    m_XML = "xml description of object";
}


void CReportObject::Format(CScope& scope)
{
    SetText(scope);
    SetFeatureTable(scope);
    SetXML(scope);
}


string GetLocusTagForFeature(const CSeq_feat& seq_feat, CScope& scope)
{
    string tag(kEmptyStr);
    if (seq_feat.GetData().IsGene()) {
        const CGene_ref& gene = seq_feat.GetData().GetGene();
        tag =  (gene.CanGetLocus_tag()) ? gene.GetLocus_tag() : kEmptyStr;
    } else {
        const CGene_ref* gene = seq_feat.GetGeneXref();
        if (gene) {
            tag = (gene->CanGetLocus_tag()) ? gene->GetLocus_tag() : kEmptyStr;
        }
        else {
            CConstRef<CSeq_feat> 
            gene= sequence::GetBestOverlappingFeat(seq_feat.GetLocation(),
                                                    CSeqFeatData::e_Gene,
				                    sequence::eOverlap_Contained,
                                                    scope);
            if (gene.NotEmpty()) {
                tag = (gene->GetData().GetGene().CanGetLocus_tag()) ? gene->GetData().GetGene().GetLocus_tag() : kEmptyStr;
            }
        }
    }
  
    return tag;
}


string GetProduct(const CProt_ref& prot_ref)
{
    string rval = "";
    if (prot_ref.CanGetName() && !prot_ref.GetName().empty()) {
        rval = prot_ref.GetName().front();
    }
    return rval;
}


string GetProductForCDS(const CSeq_feat& cds, CScope& scope)
{
    // use protein xref if available
    const CProt_ref* prot = cds.GetProtXref();
    if (prot) {
        return GetProduct(*prot);
    }
    
    // should be the longest protein feature on the bioseq pointed by the cds.GetProduct()
    if (cds.IsSetProduct()) {
        CConstRef <CSeq_feat> prot_seq_feat = sequence::GetBestOverlappingFeat(cds.GetProduct(),
                                                CSeqFeatData::e_Prot, 
                                                sequence::eOverlap_Contains,
                                                scope);
        if (prot_seq_feat && prot_seq_feat->GetData().IsProt()) {
            return GetProduct(prot_seq_feat->GetData().GetProt());
        }
    }
    return (kEmptyStr);
}


void GetSeqFeatLabel(const CSeq_feat& seq_feat, string& label)
{
     label = kEmptyStr;
     
     feature::GetLabel(seq_feat, &label, feature::fFGL_Content);
     size_t pos;
     if (!label.empty() && (string::npos != (pos = label.find("-")) ) ) {
          label = CTempString(label).substr(pos+1);
     }
     string number = "/number=";
     if (!label.empty() 
            && (seq_feat.GetData().GetSubtype() == CSeqFeatData::eSubtype_exon 
                   || seq_feat.GetData().GetSubtype() 
                              == CSeqFeatData::eSubtype_intron)
            && (string::npos != (pos = label.find(number)))) {
          label = label.substr(pos + number.size());
          if (label.find("exon") == 0 || label.find("intron") == 0) { // pos
             label = label.substr(0, label.find(' '));
          }
     }
}


CConstRef<CSeq_id> GetBestId(const CBioseq& bioseq, CSeq_id::E_Choice choice)
{
    const CBioseq::TId& seq_id_ls = bioseq.GetId();
    CConstRef<CSeq_id> best_seq_id;
    int best_score = 99999;
    ITERATE (CBioseq::TId, it, seq_id_ls) {
        if (choice == (*it)->Which()) {
            return *it;
        } else if (best_score > (*it)->BaseBestRankScore()) {
            best_seq_id = *it;
            best_score = (*it)->BaseBestRankScore();
        }
    }
    return best_seq_id;
}


void UpgradeSeqLocId(CSeq_point& pnt, CScope& scope) 
{
    if (pnt.IsSetId()) {
        CBioseq_Handle bsh = scope.GetBioseqHandle(pnt.GetId());
        if (bsh) {
            CConstRef<CSeq_id> best_id = GetBestId(*(bsh.GetCompleteBioseq()), CSeq_id::e_Genbank);
            if (best_id) {
                pnt.SetId().Assign(*best_id);
            }
        }
    }
}


void UpgradeSeqLocId(CSeq_interval& interval, CScope& scope)
{
    if (interval.IsSetId()) {
        CBioseq_Handle bsh = scope.GetBioseqHandle(interval.GetId());
        if (bsh) {
            CConstRef<CSeq_id> best_id = GetBestId(*(bsh.GetCompleteBioseq()), CSeq_id::e_Genbank);
            if (best_id) {
                interval.SetId().Assign(*best_id);
            }
        }
    }
}


void UpgradeSeqLocId(CSeq_loc& loc, CScope& scope)
{
    switch (loc.Which()) {
        case CSeq_loc::e_Bond:
            if (loc.GetBond().IsSetA()) {
                UpgradeSeqLocId(loc.SetBond().SetA(), scope);
            }
            if (loc.GetBond().IsSetB()) {
                UpgradeSeqLocId(loc.SetBond().SetB(), scope);
            }
            break;
        case CSeq_loc::e_Equiv:
            NON_CONST_ITERATE (CSeq_loc::TEquiv::Tdata, it, loc.SetEquiv().Set()) {
                UpgradeSeqLocId(**it, scope);
            }
            break;
        case CSeq_loc::e_Int:
            UpgradeSeqLocId(loc.SetInt(), scope);
            break;

        case CSeq_loc::e_Mix:
            NON_CONST_ITERATE (CSeq_loc::TMix::Tdata, it, loc.SetMix().Set()) {
                UpgradeSeqLocId(**it, scope);
            }
            break;
        case CSeq_loc::e_Packed_int:
            NON_CONST_ITERATE (CSeq_loc::TPacked_int::Tdata, it, loc.SetPacked_int().Set()) {
                UpgradeSeqLocId(**it, scope);
            }
            break;
        case CSeq_loc::e_Packed_pnt:
            if (loc.GetPacked_pnt().IsSetId()) {
                CBioseq_Handle bsh = scope.GetBioseqHandle(loc.GetPacked_pnt().GetId());
                if (bsh) {
                    CConstRef<CSeq_id> best_id = GetBestId(*(bsh.GetCompleteBioseq()), CSeq_id::e_Genbank);
                    if (best_id) {
                        loc.SetPacked_pnt().SetId().Assign(*best_id);
                    }
                }
            }
            break;
        case CSeq_loc::e_Pnt:
            UpgradeSeqLocId(loc.SetPnt(), scope);
            break;
        case CSeq_loc::e_Whole:
            {{
                CBioseq_Handle bsh = scope.GetBioseqHandle(loc.GetPacked_pnt().GetId());
                if (bsh) {
                    CConstRef<CSeq_id> best_id = GetBestId(*(bsh.GetCompleteBioseq()), CSeq_id::e_Genbank);
                    if (best_id) {
                        loc.SetWhole().Assign(*best_id);
                    }
                }
            }}
            break;
        case CSeq_loc::e_Null:
        case CSeq_loc::e_Empty:
        case CSeq_loc::e_Feat:
        case CSeq_loc::e_not_set:
            break;
    }
}


string GetSeqLocDescription(const CSeq_loc& loc, CScope& scope)
{
    string location = "";    

    CRef<CSeq_loc> cpy(new CSeq_loc());
    cpy->Assign(loc);
    UpgradeSeqLocId(*cpy, scope);
    cpy->GetLabel(&location);
    return location;
}

void CReportObject::GetTextObjectDescription(const CSeq_feat& seq_feat, CScope& scope, string &label, string &location, string &locus_tag)
{
    location = GetSeqLocDescription(seq_feat.GetLocation(), scope);
    label = seq_feat.GetData().GetKey();
    locus_tag = GetLocusTagForFeature (seq_feat, scope);
}

string CReportObject::GetTextObjectDescription(const CSeq_feat& seq_feat, CScope& scope, const string& product)
{
    string location;
    string label;
    string locus_tag;
    GetTextObjectDescription(seq_feat, scope, label, location, locus_tag);
    string rval = label + "\t" + product + "\t" + location + "\t" + locus_tag; // + "\n";
    return rval;
}

void CReportObject::GetTextObjectDescription(const CSeq_feat& seq_feat, CScope& scope, string &label, string &context, string &location, string &locus_tag)
{
    if ( seq_feat.GetData().IsProt()) {
        CConstRef <CBioseq> bioseq = sequence::GetBioseqFromSeqLoc(seq_feat.GetLocation(), scope).GetCompleteBioseq();
        if (bioseq) {
            const CSeq_feat* cds = sequence::GetCDSForProduct(*bioseq, &scope);
            if (cds) {
                context = GetProduct(seq_feat.GetData().GetProt());
                GetTextObjectDescription(*cds, scope, label, location, locus_tag);
                return;
            }
        }
    }
    
    GetTextObjectDescription(seq_feat, scope, label, location, locus_tag);   
    context.clear();
    if (seq_feat.GetData().IsCdregion()) { 
        context = GetProductForCDS(seq_feat, scope);
        if (NStr::IsBlank(context)) {
            GetSeqFeatLabel(seq_feat, context);
        }
    } else if (seq_feat.GetData().IsPub()) {
        seq_feat.GetData().GetPub().GetPub().GetLabel(&context);
    } else if (seq_feat.GetData().IsGene()) {
        if (seq_feat.GetData().GetGene().CanGetLocus() &&
            !NStr::IsBlank(seq_feat.GetData().GetGene().GetLocus())) {
            context = seq_feat.GetData().GetGene().GetLocus(); 
        } else if (seq_feat.GetData().GetGene().CanGetDesc()) {
            context = seq_feat.GetData().GetGene().GetDesc(); 
        }
    } 
    else GetSeqFeatLabel(seq_feat, context);
}

string CReportObject::GetTextObjectDescription(const CSeq_feat& seq_feat, CScope& scope)
{
    string location;
    string label;
    string context;
    string locus_tag;
    GetTextObjectDescription(seq_feat, scope, label, context, location, locus_tag);

    string rval = label + "\t" + context + "\t" + location + "\t" + locus_tag;
    return rval;
}


string CReportObject::GetTextObjectDescription(const CSeqdesc& sd, CScope& scope)
{
    string desc = GetTextObjectDescription(sd);
    CRef<CScope> s(&scope);
    CSeq_entry_Handle seh = edit::GetSeqEntryForSeqdesc (s, sd);

    if (seh) {
        string label;
        if (seh.IsSeq()) {
            seh.GetSeq().GetCompleteBioseq()->GetLabel(&label, CBioseq::eContent);
        } else if (seh.IsSet()) {
            seh.GetSet().GetCompleteBioseq_set()->GetLabel(&label, CBioseq_set::eContent);
        }
        if (!NStr::IsBlank(label)) {
            desc = label + ":" + desc;
        }
    }
    return desc;
}


string CReportObject::GetTextObjectDescription(const CSeqdesc& sd)
{
    string label(kEmptyStr);
    switch (sd.Which()) 
    {
        case CSeqdesc::e_Comment: return (sd.GetComment());
        case CSeqdesc::e_Region: return (sd.GetRegion());
        case CSeqdesc::e_Het: return (sd.GetHet());
        case CSeqdesc::e_Title: return (sd.GetTitle());
        case CSeqdesc::e_Name: return (sd.GetName());
        case CSeqdesc::e_Create_date:
            sd.GetCreate_date().GetDate(&label);
            break;
        case CSeqdesc::e_Update_date:
            sd.GetUpdate_date().GetDate(&label);
            break;
        case CSeqdesc::e_Org:
          {
            const COrg_ref& org = sd.GetOrg();
            label = (org.CanGetTaxname() ? org.GetTaxname() 
                       : (org.CanGetCommon()? org.GetCommon(): kEmptyStr));
          }
          break;
        case CSeqdesc::e_Pub:
            sd.GetPub().GetPub().GetLabel(&label);
            break;
        case CSeqdesc::e_User:
          {
            const CUser_object& uop = sd.GetUser();
            label = (uop.CanGetClass()? uop.GetClass() 
                 : (uop.GetType().IsStr()? uop.GetType().GetStr(): kEmptyStr));
          }
          break;
        case CSeqdesc::e_Method:
            label = (ENUM_METHOD_NAME(EGIBB_method)()->FindName(sd.GetMethod(), true));
            break;
        case CSeqdesc::e_Mol_type:
            label = (ENUM_METHOD_NAME(EGIBB_mol)()->FindName(sd.GetMol_type(), true));
            break;
        case CSeqdesc::e_Modif:
            ITERATE (list <EGIBB_mod>, it, sd.GetModif()) {
                label 
                  += ENUM_METHOD_NAME(EGIBB_mod)()->FindName(*it, true) + ", ";
            }
            label = label.substr(0, label.size()-2);
            break;
        case CSeqdesc::e_Source:
          {
            const COrg_ref& org = sd.GetSource().GetOrg();
            label = (org.CanGetTaxname() ? org.GetTaxname() 
                       : (org.CanGetCommon()? org.GetCommon(): kEmptyStr));
          }  
          break;
        case CSeqdesc::e_Maploc:
            sd.GetMaploc().GetLabel(&label);
            break; 
        case CSeqdesc::e_Molinfo:
            sd.GetMolinfo().GetLabel(&label);
            break;
        case CSeqdesc::e_Dbxref:
            sd.GetDbxref().GetLabel(&label);
            break;
        default:
            break; 
    }
    return label;
}


// label for Bioseq includes "best" ID, plus length, plus number of
// non-ATGC characters (if any), plus number of gaps (if any)
string CReportObject::GetTextObjectDescription(CBioseq_Handle bsh)
{
    string rval;

    CConstRef<CSeq_id> id = GetBestId(*(bsh.GetCompleteBioseq()), CSeq_id::e_Genbank);

    id->GetLabel(&rval, CSeq_id::eContent);

    rval += " (length " + NStr::NumericToString(bsh.GetInst_Length());

    size_t num_other = 0;
    size_t num_gap = 0;
    CSeqVector vec = bsh.GetSeqVector(CBioseq_Handle::eCoding_Iupac);
    CSeqVector::const_iterator vi = vec.begin();
    while (vi != vec.end()) {
        if (*vi == 'A' || *vi == 'T' || *vi == 'G' || *vi == 'C') {
            // normal
        } else if (vi.IsInGap()) {
            num_gap++;
        } else {
            num_other++;
        }
        ++vi;
    }

    if (num_other > 0 && bsh.IsNa()) {
        rval += ", " + NStr::NumericToString(num_other) + " other";
    }

    if (num_gap > 0) {
        rval += ", " + NStr::NumericToString(num_gap) + " gap";
    }
    
    rval += ")";
    return rval;
}

string CReportObject::GetTextObjectDescription(CBioseq_set_Handle bssh)
{
    CNcbiOstrstream result_strm;

    CBioseq_set::EClass bioseq_set_class =
        GET_FIELD_OR_DEFAULT(bssh, Class, CBioseq_set::eClass_not_set);
    if( bioseq_set_class == CBioseq_set::eClass_segset ||
        bioseq_set_class == CBioseq_set::eClass_nuc_prot )
    {
        switch(bioseq_set_class) {
        case CBioseq_set::eClass_segset:
            result_strm << "ss|";
            break;
        case CBioseq_set::eClass_nuc_prot:
            result_strm << "np|";
            break;
        default:
            _TROUBLE;
        }

        // get representative bioseq from bioseq-set
        CBioseq_CI bioseq_ci(bssh);
        if( ! bioseq_ci ) {
            return "(EMPTY BIOSEQ-SET)";
        }
        const CBioseq_Handle & main_bsh = *bioseq_ci;
        CConstRef<CSeq_id> best_seq_id = GetBestId(
            *main_bsh.GetCompleteBioseq(), CSeq_id::e_Genbank);
        _ASSERT(best_seq_id);  // a Bioseq must have at least one Seq-id
        {
            string seq_id_str;
            best_seq_id->GetLabel(&seq_id_str, CSeq_id::eContent);
            result_strm << seq_id_str;
        }

    } else {
        // get first child of bssh (which could be a bioseq or bioseq-set)
        CSeq_entry_CI direct_child_ci(bssh); // NOT recursive
        if( ! direct_child_ci ) {
            // no child, so this is the best we can do
            return "BioseqSet";
        }

        result_strm << "Set containing ";
        if( direct_child_ci->IsSeq() ) {
            CConstRef<CSeq_id> best_seq_id = GetBestId(
                *direct_child_ci->GetSeq().GetCompleteBioseq(),
                CSeq_id::e_Genbank);
            _ASSERT(best_seq_id);  // a Bioseq must have at least one Seq-id
            {
                string seq_id_str;
                best_seq_id->GetLabel(&seq_id_str, CSeq_id::eContent);
                result_strm << seq_id_str;
            }
        } else {
            _ASSERT(direct_child_ci->IsSet());
            result_strm << GetTextObjectDescription(direct_child_ci->GetSet());
        }
    }

    return (string)CNcbiOstrstreamToString(result_strm);
}


void CReportObject::DropReference()
{
    m_Bioseq.Reset();
    m_Seq_feat.Reset();
    m_Seqdesc.Reset();
    m_Bioseq_set.Reset();
}


void CReportObject::DropReference(CScope& scope) 
{
    if (!m_Bioseq && !m_Seq_feat && !m_Seqdesc && !m_Bioseq_set) {
        return;
    }
    if (NStr::IsBlank(m_Text)) {
        SetText(scope);
    }
    if (NStr::IsBlank(m_FeatureTable)) {
        SetFeatureTable(scope);
    }
    if (NStr::IsBlank(m_XML)) {
        SetXML(scope);
    }
}


bool CReportObjPtr::operator<(const CReportObjPtr& other) const
{
    if (P == other.P) {
        return false;
    }
    const CReportObject& A = (const CReportObject&)*P;
    const CReportObject& B = (const CReportObject&)*other.P;
    if (A.m_Bioseq || B.m_Bioseq) {
        return A.m_Bioseq < B.m_Bioseq;
    }
    if (A.m_Seq_feat || B.m_Seq_feat) {
        return A.m_Seq_feat < B.m_Seq_feat;
    }
    if (A.m_Seqdesc || B.m_Seqdesc) {
        return A.m_Seqdesc < B.m_Seqdesc;
    }
    if (A.m_Bioseq_set || B.m_Bioseq_set) {
        return A.m_Bioseq_set < B.m_Bioseq_set;
    }
    if (A.m_Filename != B.m_Filename) {
        return A.m_Filename < B.m_Filename;
    }
    if (A.m_Text != B.m_Text) {
        return A.m_Text < B.m_Text;
    }
    return false;
}


END_SCOPE(NDiscrepancy)
END_NCBI_SCOPE
