/* $Id$
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
 * Author:  Robert G. Smith
 *
 * File Description:
 *   Implementation of BasicCleanup for Seq-feat and sub-objects.
 *
 */

#include <ncbi_pch.hpp>
#include "cleanup_utils.hpp"
#include <objects/general/Object_id.hpp>
#include <objects/general/Dbtag.hpp>
#include <objects/seqfeat/Cdregion.hpp>
#include <objects/seqfeat/Genetic_code.hpp>
#include <objects/seqfeat/Gb_qual.hpp>
#include <objects/seqfeat/Imp_feat.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqfeat/SeqFeatXref.hpp>
#include <objects/seqfeat/BioSource.hpp>
#include <objects/seqfeat/SubSource.hpp>
#include <objects/seqfeat/Org_ref.hpp>
#include <util/static_map.hpp>

#include <objects/seqfeat/RNA_ref.hpp>
#include <objects/seqfeat/Code_break.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/general/User_object.hpp>
#include <objects/general/User_field.hpp>
#include <objects/seq/seqport_util.hpp>
#include <objects/seq/Seqdesc.hpp>
#include <objects/seq/MolInfo.hpp>
#include <vector>
#include <objmgr/feat_ci.hpp>
#include <objmgr/util/sequence.hpp>
#include <objmgr/seq_vector.hpp>
#include <objmgr/seqdesc_ci.hpp>
#include <objmgr/bioseq_ci.hpp>

#include "cleanupp.hpp"


BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::



// ==========================================================================
//                             CSeq_feat Cleanup section
// ==========================================================================

void CCleanup_imp::x_CleanupExcept_text(string& except_text)
{
    if (NStr::Find(except_text, "ribosome slippage") == NPOS     &&
        NStr::Find(except_text, "trans splicing") == NPOS        &&
        NStr::Find(except_text, "alternate processing") == NPOS  &&
        NStr::Find(except_text, "adjusted for low quality genome") == NPOS  &&
        NStr::Find(except_text, "non-consensus splice site") == NPOS) {
        return;
    }

    vector<string> exceptions;
    NStr::Tokenize(except_text, ",", exceptions);

    NON_CONST_ITERATE(vector<string>, it, exceptions) {
        string& text = *it;
        NStr::TruncateSpacesInPlace(text);
        if (!text.empty()) {
            if (text == "ribosome slippage") {
                text = "ribosomal slippage";
            } else if (text == "trans splicing") {
                text = "trans-splicing";
            } else if (text == "alternate processing") {
                text = "alternative processing";
            } else if (text == "adjusted for low quality genome") {
                text = "adjusted for low-quality genome";
            } else if (text == "non-consensus splice site") {
                text = "nonconsensus splice site";
            }
        }
    }
    except_text = NStr::Join(exceptions, ",");
}


// === Gene

static bool s_IsEmptyGeneRef(const CGene_ref& gref)
{
    return (!gref.IsSetLocus()  &&  !gref.IsSetAllele()  &&
        !gref.IsSetDesc()  &&  !gref.IsSetMaploc()  &&  !gref.IsSetDb()  &&
        !gref.IsSetSyn()  &&  !gref.IsSetLocus_tag());
}


// === Site

typedef pair<const string, CSeqFeatData::TSite>  TSiteElem;
static const TSiteElem sc_site_map[] = {
    TSiteElem("acetylation", CSeqFeatData::eSite_acetylation),
    TSiteElem("active", CSeqFeatData::eSite_active),
    TSiteElem("amidation", CSeqFeatData::eSite_amidation),
    TSiteElem("binding", CSeqFeatData::eSite_binding),
    TSiteElem("blocked", CSeqFeatData::eSite_blocked),
    TSiteElem("cleavage", CSeqFeatData::eSite_cleavage),
    TSiteElem("dna binding", CSeqFeatData::eSite_dna_binding),
    TSiteElem("dna-binding", CSeqFeatData::eSite_dna_binding),
    TSiteElem("gamma carboxyglutamic acid", CSeqFeatData::eSite_gamma_carboxyglutamic_acid),
    TSiteElem("gamma-carboxyglutamic-acid", CSeqFeatData::eSite_gamma_carboxyglutamic_acid),
    TSiteElem("glycosylation", CSeqFeatData::eSite_glycosylation),
    TSiteElem("hydroxylation", CSeqFeatData::eSite_hydroxylation),
    TSiteElem("inhibit", CSeqFeatData::eSite_inhibit),
    TSiteElem("lipid binding", CSeqFeatData::eSite_lipid_binding),
    TSiteElem("lipid-binding", CSeqFeatData::eSite_lipid_binding),
    TSiteElem("metal binding", CSeqFeatData::eSite_metal_binding),
    TSiteElem("metal-binding", CSeqFeatData::eSite_metal_binding),
    TSiteElem("methylation", CSeqFeatData::eSite_methylation),
    TSiteElem("modified", CSeqFeatData::eSite_modified),
    TSiteElem("mutagenized", CSeqFeatData::eSite_mutagenized),
    TSiteElem("myristoylation", CSeqFeatData::eSite_myristoylation),
    TSiteElem("np binding", CSeqFeatData::eSite_np_binding),
    TSiteElem("np-binding", CSeqFeatData::eSite_np_binding),
    TSiteElem("oxidative deamination", CSeqFeatData::eSite_oxidative_deamination),
    TSiteElem("oxidative-deamination", CSeqFeatData::eSite_oxidative_deamination),
    TSiteElem("phosphorylation", CSeqFeatData::eSite_phosphorylation),
    TSiteElem("pyrrolidone carboxylic acid", CSeqFeatData::eSite_pyrrolidone_carboxylic_acid),
    TSiteElem("pyrrolidone-carboxylic-acid", CSeqFeatData::eSite_pyrrolidone_carboxylic_acid),
    TSiteElem("signal peptide", CSeqFeatData::eSite_signal_peptide),
    TSiteElem("signal-peptide", CSeqFeatData::eSite_signal_peptide),
    TSiteElem("sulfatation", CSeqFeatData::eSite_sulfatation),
    TSiteElem("transit peptide", CSeqFeatData::eSite_transit_peptide),
    TSiteElem("transit-peptide", CSeqFeatData::eSite_transit_peptide),
    TSiteElem("transmembrane region", CSeqFeatData::eSite_transmembrane_region),
    TSiteElem("transmembrane-region", CSeqFeatData::eSite_transmembrane_region)
};
typedef CStaticArrayMap<const string, CSeqFeatData::TSite>   TSiteMap;
static const TSiteMap sc_SiteMap(sc_site_map, sizeof(sc_site_map));


// === Prot

static const string uninf_names[] = {
    "peptide", "putative", "signal", "signal peptide", "signal-peptide",
    "signal_peptide", "transit", "transit peptide", "transit-peptide",
    "transit_peptide", "unknown", "unnamed"
};
typedef CStaticArraySet<string, PNocase> TUninformative;
static const TUninformative sc_UninfNames(uninf_names, sizeof(uninf_names));

static bool s_IsInformativeName(const string& name)
{
    return sc_UninfNames.find(name) == sc_UninfNames.end();
}



// === Imp

void CCleanup_imp::x_AddReplaceQual(CSeq_feat& feat, const string& str)
{
    if (!NStr::EndsWith(str, ')')) {
        return;
    }

    SIZE_TYPE start = str.find_first_of('\"');
    if (start != NPOS) {
        SIZE_TYPE end = str.find_first_of('\"', start + 1);
        if (end != NPOS) {
            feat.AddQualifier("replace", str.substr(start + 1, end));
        }
    }
}

typedef pair<const string, CRNA_ref::TType> TRnaTypePair;
static const TRnaTypePair sc_rna_type_map[] = {
    TRnaTypePair("mRNA", CRNA_ref::eType_premsg),
    TRnaTypePair("misc_RNA", CRNA_ref::eType_other),
    TRnaTypePair("precursor_RNA", CRNA_ref::eType_premsg),
    TRnaTypePair("rRNA", CRNA_ref::eType_tRNA),
    TRnaTypePair("scRNA", CRNA_ref::eType_scRNA),
    TRnaTypePair("snRNA", CRNA_ref::eType_snRNA),
    TRnaTypePair("snoRNA", CRNA_ref::eType_snoRNA),
    TRnaTypePair("tRNA", CRNA_ref::eType_mRNA)
};
typedef CStaticArrayMap<const string, CRNA_ref::TType> TRnaTypeMap;
static const TRnaTypeMap sc_RnaTypeMap(sc_rna_type_map, sizeof(sc_rna_type_map));


// === Seq-feat.data

void CCleanup_imp::BasicCleanup(CSeqFeatData& data) 
{    
    // basic localized cleanup of kinds of CSeqFeatData.
    switch (data.Which()) {
        case CSeqFeatData::e_Gene:
            BasicCleanup(data.SetGene());
            break;
        case CSeqFeatData::e_Org:
            BasicCleanup(data.SetOrg());
            break;
        case CSeqFeatData::e_Cdregion:
            break;
        case CSeqFeatData::e_Prot:
            BasicCleanup(data.SetProt());
            break;
        case CSeqFeatData::e_Rna:
            BasicCleanup(data.SetRna());
            break;
        case CSeqFeatData::e_Pub:
            BasicCleanup(data.SetPub());
            break;
        case CSeqFeatData::e_Seq:
            break;
        case CSeqFeatData::e_Imp:
            BasicCleanup(data.SetImp());
            break;
        case CSeqFeatData::e_Region:
            break;
        case CSeqFeatData::e_Comment:
            break;
        case CSeqFeatData::e_Bond:
            break;
        case CSeqFeatData::e_Site:
            break;
        case CSeqFeatData::e_Rsite:
            break;
        case CSeqFeatData::e_User:
            BasicCleanup(data.SetUser());
            break;
        case CSeqFeatData::e_Txinit:
            break;
        case CSeqFeatData::e_Num:
            break;
        case CSeqFeatData::e_Psec_str:
            break;
        case CSeqFeatData::e_Non_std_residue:
            break;
        case CSeqFeatData::e_Het:
            break;
        case CSeqFeatData::e_Biosrc:
            BasicCleanup(data.SetBiosrc());
            break;
        default:
            break;
    }
}


void CCleanup_imp::BasicCleanup(CSeq_feat& feat, CSeqFeatData& data) 
{
    // change things in the feat based on what is in data and vice versa.
    // does not call any other BasicCleanup routine.

    switch (data.Which()) {
    case CSeqFeatData::e_Gene:
        {
            CSeqFeatData::TGene& gene = data.SetGene();
            
            // remove feat.comment if equal to gene.locus
            if (gene.IsSetLocus()  &&  feat.IsSetComment()) {
                if (feat.GetComment() == gene.GetLocus()) {
                    feat.ResetComment();
                }
            }
                
            // move Gene-ref.db to the Seq-feat.dbxref
            if (gene.IsSetDb()) {
                copy(gene.GetDb().begin(), gene.GetDb().end(), back_inserter(feat.SetDbxref()));
                gene.ResetDb();
            }
            
            // move db_xref from gene xrefs to feature
            if (feat.IsSetXref()) {
                CSeq_feat::TXref& xrefs = feat.SetXref();
                CSeq_feat::TXref::iterator it = xrefs.begin();
                while (it != xrefs.end()) {
                    CSeqFeatXref& xref = **it;
                    if (xref.IsSetData()  &&  xref.GetData().IsGene()) {
                        CGene_ref& gref = xref.SetData().SetGene();
                        if (gref.IsSetDb()) {
                            copy(gref.GetDb().begin(), gref.GetDb().end(), 
                                 back_inserter(feat.SetDbxref()));
                            gref.ResetDb();
                        }
                        // remove gene xref if it has no values set
                        if (s_IsEmptyGeneRef(gref)) {
                            it = xrefs.erase(it);
                        } else {
                            ++it;
                        }
                    }
                }
            }
        }
        break;
    case CSeqFeatData::e_Org:
        break;
    case CSeqFeatData::e_Cdregion:
        break;
    case CSeqFeatData::e_Prot:
        {
            CSeq_feat::TData::TProt& prot = data.SetProt();
            
            if (prot.IsSetProcessed()  &&  prot.IsSetName()) {
                CProt_ref::TProcessed processed = prot.GetProcessed();
                CProt_ref::TName& name = prot.SetName();
                if (processed == CProt_ref::eProcessed_signal_peptide  ||
                    processed == CProt_ref::eProcessed_transit_peptide) {
                    CProt_ref::TName::iterator it = name.begin();
                    while (it != name.end()) {
                        if (!feat.IsSetComment()) {
                            if (NStr::Find(*it, "putative") != NPOS  ||
                                NStr::Find(*it, "put. ") != NPOS) {
                                feat.SetComment("putative");
                            }
                        }
                        // remove uninformative names
                        if (!s_IsInformativeName(*it)) {
                            it = name.erase(it);
                        } else {
                            ++it;
                        }
                    }
                }
            }
                
            // move Prot-ref.db to Seq-feat.dbxref
            if (prot.IsSetDb()) {
                copy(prot.GetDb().begin(), prot.GetDb().end(),
                     back_inserter(feat.SetDbxref()));
                prot.ResetDb();
            }
        }
        break;
    case CSeqFeatData::e_Rna:
        break;
    case CSeqFeatData::e_Pub:
        break;
    case CSeqFeatData::e_Seq:
        break;
    case CSeqFeatData::e_Imp:
        {
            CSeqFeatData::TImp& imp = data.SetImp();
            
            if (imp.IsSetLoc()  &&  (NStr::Find(imp.GetLoc(), "replace") != NPOS)) {
                x_AddReplaceQual(feat, imp.GetLoc());
                imp.ResetLoc();
            }
            
            if (imp.IsSetKey()) {
                const CImp_feat::TKey& key = imp.GetKey();
                
                if (key == "CDS") {
                    if ( ! (m_Mode == eCleanup_EMBL  ||  m_Mode == eCleanup_DDBJ) ) {
                        data.SetCdregion();
                        //s_CleanupCdregion(feat);
                    }
                } else if (!imp.IsSetLoc()  ||  NStr::IsBlank(imp.GetLoc())) {
                    TRnaTypeMap::const_iterator rna_type_it = sc_RnaTypeMap.find(key);
                    if (rna_type_it != sc_RnaTypeMap.end()) {
                        CSeqFeatData::TRna& rna = data.SetRna();
                        rna.SetType(rna_type_it->second);
                        BasicCleanup(rna);
                    } else {
                        // !!! need to find protein bioseq without object manager
                    }
                }
            }
        }
        break;
    case CSeqFeatData::e_Region:
        {            
            string &region = data.SetRegion();
            CleanString(region);
            ConvertDoubleQuotes(region);
            if (region.empty()) {
                feat.SetData().SetComment();
            }
        }
        break;
    case CSeqFeatData::e_Comment:
        break;
    case CSeqFeatData::e_Bond:
        break;
    case CSeqFeatData::e_Site:
        {
            CSeqFeatData::TSite site = data.GetSite();
            if (feat.IsSetComment()  &&
                (site == CSeqFeatData::TSite(0)  ||  site == CSeqFeatData::eSite_other)) {
                const string& comment = feat.GetComment();
                ITERATE (TSiteMap, it, sc_SiteMap) {
                    if (NStr::StartsWith(comment, it->first, NStr::eNocase)) {
                        feat.SetData().SetSite(it->second);
                        if (NStr::IsBlank(comment, it->first.length())  ||
                            NStr::EqualNocase(comment, it->first.length(), NPOS, " site")) {
                            feat.ResetComment();
                        }
                    }
                }
            }
        }
        break;
    case CSeqFeatData::e_Rsite:
        break;
    case CSeqFeatData::e_User:
        break;
    case CSeqFeatData::e_Txinit:
        break;
    case CSeqFeatData::e_Num:
        break;
    case CSeqFeatData::e_Psec_str:
        break;
    case CSeqFeatData::e_Non_std_residue:
        break;
    case CSeqFeatData::e_Het:
        break;
    case CSeqFeatData::e_Biosrc:
        break;
    default:
        break;
    }
}


// === Seq-feat.dbxref


struct SDbtagCompare
{
    // is dbt1 < dbt2
    bool operator()(const CRef<CDbtag>& dbt1, const CRef<CDbtag>& dbt2) {
        return dbt1->Compare(*dbt2) < 0;
    }
};


struct SDbtagEqual
{
    // is dbt1 < dbt2
    bool operator()(const CRef<CDbtag>& dbt1, const CRef<CDbtag>& dbt2) {
        return dbt1->Compare(*dbt2) == 0;
    }
};


struct SGb_QualCompare
{
    // is q1 < q2
    bool operator()(const CRef<CGb_qual>& q1, const CRef<CGb_qual>& q2) {
        return (q1->Compare(*q2) < 0);
    }
};


struct SGb_QualEqual
{
    // is q1 == q2
    bool operator()(const CRef<CGb_qual>& q1, const CRef<CGb_qual>& q2) {
        return (q1->Compare(*q2) == 0);
    }
};


// BasicCleanup
void CCleanup_imp::BasicCleanup(CSeq_feat& f)
{
    if (!f.IsSetData()) {
        return;
    }

    CLEAN_STRING_MEMBER(f, Comment);
    if (f.IsSetComment()  &&  f.GetComment() == ".") {
        f.ResetComment();
    }
    CLEAN_STRING_MEMBER(f, Title);
    CLEAN_STRING_MEMBER(f, Except_text);
    if (f.IsSetExcept_text()) {
        x_CleanupExcept_text(f.SetExcept_text());
    }

    BasicCleanup(f.SetData());
    BasicCleanup(f, f.SetData());

    if (f.IsSetDbxref()) {
        CSeq_feat::TDbxref& dbxref = f.SetDbxref();
        
        // dbxrefs cleanup
        CSeq_feat::TDbxref::iterator it = dbxref.begin();
        while (it != dbxref.end()) {
            if (it->Empty()) {
                it = dbxref.erase(it);
                continue;
            }
            BasicCleanup(**it);
            
            ++it;
        }
        
        // sort/unique db_xrefs
        stable_sort(dbxref.begin(), dbxref.end(), SDbtagCompare());
        it = unique(dbxref.begin(), dbxref.end(), SDbtagEqual());
        dbxref.erase(it, dbxref.end());
    }
    if (f.IsSetQual()) {
        
        CSeq_feat::TQual::iterator it = f.SetQual().begin();
        CSeq_feat::TQual::iterator it_end = f.SetQual().end();
        while (it != it_end) {
            CGb_qual& gb_qual = **it;
            // clean up this qual.
            BasicCleanup(gb_qual);
            // cleanup the feature given this qual.
            if (BasicCleanup(f, gb_qual)) {
                it = f.SetQual().erase(it);
                it_end = f.SetQual().end();
            } else {
                ++it;
            }
        }
        // expand out all combined quals.
        x_ExpandCombinedQuals(f.SetQual());
        
        // sort/uniquequalsdb_xrefs
        CSeq_feat::TQual& quals = f.SetQual();
        stable_sort(quals.begin(), quals.end(), SGb_QualCompare());
        CSeq_feat::TQual::iterator erase_it = unique(quals.begin(), quals.end(), SGb_QualEqual());
        quals.erase(erase_it, quals.end());
    }
}

// ==========================================================================
//                             end of Seq_feat cleanup section
// ==========================================================================




void CCleanup_imp::BasicCleanup(CGene_ref& gene_ref)
{
    CLEAN_STRING_MEMBER(gene_ref, Locus);
    CLEAN_STRING_MEMBER(gene_ref, Allele);
    CLEAN_STRING_MEMBER(gene_ref, Desc);
    CLEAN_STRING_MEMBER(gene_ref, Maploc);
    CLEAN_STRING_MEMBER(gene_ref, Locus_tag);
    CLEAN_STRING_LIST(gene_ref, Syn);
    
    // remove synonyms equal to locus
    if (gene_ref.IsSetLocus()  &&  gene_ref.IsSetSyn()) {
        const CGene_ref::TLocus& locus = gene_ref.GetLocus();
        CGene_ref::TSyn& syns = gene_ref.SetSyn();
        
        CGene_ref::TSyn::iterator it = syns.begin();
        while (it != syns.end()) {
            if (locus == *it) {
                it = syns.erase(it);
            } else {
                ++it;
            }
        }
    }
}


// perform basic cleanup functionality (trim spaces from strings etc.)
void CCleanup_imp::BasicCleanup(CProt_ref& prot_ref)
{
    CLEAN_STRING_MEMBER(prot_ref, Desc);
    CLEAN_STRING_LIST(prot_ref, Name);
    CLEAN_STRING_LIST(prot_ref, Ec);
    CLEAN_STRING_LIST(prot_ref, Activity);
    
    if (prot_ref.IsSetProcessed()  &&  !prot_ref.IsSetName()) {
        CProt_ref::TProcessed processed = prot_ref.GetProcessed();
        if (processed == CProt_ref::eProcessed_preprotein  ||  
            processed == CProt_ref::eProcessed_mature) {
            prot_ref.SetName().push_back("unnamed");
        }
    }
}


// RNA_ref basic cleanup 
void CCleanup_imp::BasicCleanup(CRNA_ref& rr)
{
    if (rr.IsSetExt()) {
        CRNA_ref::TExt& ext = rr.SetExt();
        switch (ext.Which()) {
            case CRNA_ref::TExt::e_Name:
                {
                    static const string rRNA = " rRNA";
                    static const string kRibosomalrRna = " ribosomal rRNA";
                    
                    _ASSERT(rr.IsSetExt()  &&  rr.GetExt().IsName());
                    
                    string& name = rr.SetExt().SetName();
                    CleanString(name);
                    
                    if (name.empty()) {
                        rr.ResetExt();
                    } else if (rr.IsSetType()) {
                        switch (rr.GetType()) {
                            case CRNA_ref::eType_rRNA:
                            {{
                                size_t len = name.length();
                                if (len >= rRNA.length()                       &&
                                    NStr::EndsWith(name, rRNA, NStr::eNocase)  &&
                                    !NStr::EndsWith(name, kRibosomalrRna, NStr::eNocase)) {
                                    name.replace(len - rRNA.length(), name.size(), kRibosomalrRna);
                                }
                                break;
                            }}
                            case CRNA_ref::eType_tRNA:
                            {{
                                // !!!
                                break;
                            }}
                            case CRNA_ref::eType_other:
                            {{
                                if (NStr::EqualNocase(name, "its1")) {
                                    name = "internal transcribed spacer 1";
                                } else if (NStr::EqualNocase(name, "its2")) {
                                    name = "internal transcribed spacer 2";
                                } else if (NStr::EqualNocase(name, "its3")) {
                                    name = "internal transcribed spacer 3";
                                } else if (NStr::EqualNocase(name, "its")) {
                                    name = "internal transcribed spacer";
                                    break;
                                }
                            }}
                            default:
                                break;
                        }
                    }
                }
                break;
            case CRNA_ref::TExt::e_TRNA:
                break;
            default:
                break;
        }
    }
}



// Imp_feat cleanup
void CCleanup_imp::BasicCleanup(CImp_feat& imf)
{
    CLEAN_STRING_MEMBER(imf, Key);
    CLEAN_STRING_MEMBER(imf, Loc);
    CLEAN_STRING_MEMBER(imf, Descr);
    
    if (imf.IsSetKey()) {
        const CImp_feat::TKey& key = imf.GetKey();
        if (key == "allele"  ||  key == "mutation") {
            imf.SetKey("variation");
        }
    }
}


// Extended Cleanup methods

// changes "reasons cited in publication" in feature exception text
// to "reasons given in citation"
void CCleanup_imp::x_CorrectExceptText (string& except_text)
{
    if (NStr::Equal(except_text, "reasons cited in publication")) {
        except_text = "reasons given in citation";
    }
}


void CCleanup_imp::x_CorrectExceptText(CSeq_feat& feat)
{
    if (feat.IsSetExcept_text()) {
        x_CorrectExceptText(feat.SetExcept_text());
    }    
}


void CCleanup_imp::x_CorrectExceptText (CSeq_annot_Handle sa)
{
    if (sa.IsFtable()) {
        CFeat_CI feat_ci(sa);
        while (feat_ci) {
            if (feat_ci->IsSetExcept_text()) {
                x_CorrectExceptText(const_cast<CSeq_feat &> (feat_ci->GetOriginalFeature()));
            }
            ++feat_ci;                
        }
    }

}


// move GenBank qualifiers named "db_xref" on a feature to real dbxrefs
// Was SeqEntryMoveDbxrefs in C Toolkit
void CCleanup_imp::x_MoveDbxrefs(CSeq_feat& feat)
{
    if (feat.CanGetQual()) {
        CSeq_feat::TQual& current_list = feat.SetQual();
        CSeq_feat::TQual new_list;
        new_list.clear();
        
        ITERATE (CSeq_feat::TQual, it, current_list) {            
            if ((*it)->CanGetQual() && NStr::Equal((*it)->GetQual(), "db_xref")) {
                CRef<CDbtag> tag(new CDbtag());
                string qval = (*it)->GetVal();
                unsigned int pos = NStr::Find (qval, ":");
                if (pos == NCBI_NS_STD::string::npos) {
                    tag->SetDb("?");
                    tag->SetTag().Select(CObject_id::e_Str);
                    tag->SetTag().SetStr(qval);
                } else {
                    tag->SetDb(qval.substr(0, pos));
                    tag->SetTag().Select(CObject_id::e_Str);
                    tag->SetTag().SetStr(qval.substr(pos));
                }
                feat.SetDbxref().push_back(tag);
            } else {
                new_list.push_back (*it);
            }
        }
        current_list.clear();

        ITERATE (CSeq_feat::TQual, it, new_list) {
            current_list.push_back(*it);
        }
        if (current_list.size() == 0) {
            feat.ResetQual();
        }        
    }
}


void CCleanup_imp::x_MoveDbxrefs (CSeq_annot_Handle sa)
{
    if (sa.IsFtable()) {
        CFeat_CI feat_ci(sa);
        while (feat_ci) {
            x_MoveDbxrefs(const_cast<CSeq_feat &> (feat_ci->GetOriginalFeature()));
            ++feat_ci;                
        }
    }

}


void CCleanup_imp::x_ExtendedCleanStrings (CGene_ref& gene_ref)
{
    EXTENDED_CLEAN_STRING_MEMBER (gene_ref, Locus);
    EXTENDED_CLEAN_STRING_MEMBER (gene_ref, Allele);
    EXTENDED_CLEAN_STRING_MEMBER (gene_ref, Desc);
    EXTENDED_CLEAN_STRING_MEMBER (gene_ref, Maploc);
    EXTENDED_CLEAN_STRING_MEMBER (gene_ref, Locus_tag);
    EXTENDED_CLEAN_STRING_LIST (gene_ref, Syn);
    if (gene_ref.CanGetSyn()) {
        CleanVisStringList (gene_ref.SetSyn());
        if (gene_ref.GetSyn().size() == 0) {
            gene_ref.ResetSyn();
        }
    }
}


void CCleanup_imp::x_ExtendedCleanStrings (CProt_ref& prot_ref)
{
    EXTENDED_CLEAN_STRING_MEMBER (prot_ref, Desc);
    EXTENDED_CLEAN_STRING_LIST (prot_ref, Name);
    EXTENDED_CLEAN_STRING_LIST (prot_ref, Ec);
    EXTENDED_CLEAN_STRING_LIST (prot_ref, Activity);
}


void CCleanup_imp::x_ExtendedCleanStrings (CRNA_ref& rr)
{
    if (rr.IsSetExt() && rr.GetExt().IsName()) {
        CleanVisString (rr.SetExt().SetName());
        if (NStr::IsBlank(rr.GetExt().GetName())) {
            rr.ResetExt();
        }
    }
}


void CCleanup_imp::x_ExtendedCleanStrings (CSeq_feat& feat)
{
    EXTENDED_CLEAN_STRING_MEMBER (feat, Comment);
    EXTENDED_CLEAN_STRING_MEMBER (feat, Title);

    switch (feat.GetData().Which()) {
        case CSeqFeatData::e_Bond:
        case CSeqFeatData::e_Site:
        case CSeqFeatData::e_Psec_str:
        case CSeqFeatData::e_Comment:
        case CSeqFeatData::e_Cdregion:
        case CSeqFeatData::e_Seq:
        case CSeqFeatData::e_Rsite:
        case CSeqFeatData::e_User:
        case CSeqFeatData::e_Txinit:
        case CSeqFeatData::e_Num:
        case CSeqFeatData::e_Non_std_residue:
        case CSeqFeatData::e_Het:
            // nothing to do
            break;
        case CSeqFeatData::e_Gene:
            x_ExtendedCleanStrings (feat.SetData().SetGene());
            break;
        case CSeqFeatData::e_Org:
            x_ExtendedCleanStrings (feat.SetData().SetOrg());
            break;
        case CSeqFeatData::e_Prot:
            x_ExtendedCleanStrings (feat.SetData().SetProt());
            break;
        case CSeqFeatData::e_Rna:
            x_ExtendedCleanStrings (feat.SetData().SetRna());
            break;
        case CSeqFeatData::e_Pub:
            x_ExtendedCleanStrings (feat.SetData().SetPub());
            break;
        case CSeqFeatData::e_Imp:
            x_ExtendedCleanStrings (feat.SetData().SetImp());
            break;
        case CSeqFeatData::e_Region:
        {
            string &region = feat.SetData().SetRegion();
            CleanVisString(region);
            if (region.empty()) {
                feat.ResetData();
                feat.SetData().SetComment();
            }
        }
            break;
        case CSeqFeatData::e_Biosrc:
            x_ExtendedCleanSubSourceList(feat.SetData().SetBiosrc());
            if (feat.GetData().GetBiosrc().CanGetOrg()) {
                x_ExtendedCleanStrings(feat.SetData().SetBiosrc().SetOrg());
            }
            break;
        default:
            break;
    }  
}


void CCleanup_imp::x_ExtendedCleanStrings (CImp_feat& imf)
{
    EXTENDED_CLEAN_STRING_MEMBER (imf, Key);
    EXTENDED_CLEAN_STRING_MEMBER (imf, Loc);
    EXTENDED_CLEAN_STRING_MEMBER (imf, Descr);
}


CSeq_feat_Handle GetSeq_feat_Handle(CScope& scope, const CSeq_feat& feat)
{
    SAnnotSelector sel(feat.GetData().GetSubtype());
    sel.SetResolveAll().SetNoMapping().SetSortOrder(sel.eSortOrder_None);
    for (CFeat_CI mf(scope, feat.GetLocation(), sel); mf; ++mf) {
        if (mf->GetOriginalFeature().Equals(feat)) {
            return mf->GetSeq_feat_Handle();
        }
    }
    return CSeq_feat_Handle();
}


// Was CdEndCheck in C Toolkit
// Attempts to adjust the length of a coding region so that it will translate to
// the specified product
// returns true if 
bool CCleanup_imp::x_CheckCodingRegionEnds (CSeq_feat& orig_feat)
{
    if (!orig_feat.IsSetData() 
        || orig_feat.GetData().Which() != CSeqFeatData::e_Cdregion
        || !orig_feat.CanGetProduct()) {
        return false;
    }
    
    CSeq_feat_Handle ofh = GetSeq_feat_Handle(*m_Scope, orig_feat);
            
    if (ofh.GetSeq_feat().IsNull()) {
        return false;
    }
    
    CRef<CSeq_feat> feat(new CSeq_feat);
    feat->Assign(orig_feat);
    
    const CCdregion& crp = feat->GetData().GetCdregion();    

    unsigned int feat_len = sequence::GetLength(feat->GetLocation(), m_Scope);
    unsigned int frame_adjusted_len = feat_len;
    if (crp.CanGetFrame()) {
        if (crp.GetFrame() == 2) {
            frame_adjusted_len -= 1;
        } else if (crp.GetFrame() == 3) {
            frame_adjusted_len -= 2;
        }
    }
    
    unsigned int remainder = frame_adjusted_len % 3;
    unsigned int translation_len = frame_adjusted_len / 3;
    CBioseq_Handle product;
    
    try {
        product = m_Scope->GetBioseqHandle(feat->GetProduct());
    } catch (...) {
        return false;
    }
    
    if (product.GetBioseqLength() + 1 == translation_len && remainder == 0) {
        return false;
    }

    if (crp.CanGetCode_break()) {
        ITERATE (list< CRef< CCode_break > >, it, crp.GetCode_break()) {
            int pos1 = INT_MAX;
            int pos2 = -10;
            int pos = 0;
        
            for(CSeq_loc_CI loc_it((*it)->GetLoc()); loc_it; ++loc_it) {
                pos = sequence::LocationOffset(feat->GetLocation(), loc_it.GetSeq_loc(), 
                                               sequence::eOffset_FromStart, m_Scope);
                if (pos < pos1) {
                    pos1 = pos;
                }
                
                pos = sequence::LocationOffset(feat->GetLocation(), loc_it.GetSeq_loc(),
                                               sequence::eOffset_FromEnd, m_Scope);
                if (pos > pos2)
                    pos2 = pos;
            }
        
            pos = pos2 - pos1;
            if (pos >= 0 && pos <= 1 && pos2 == feat_len - 1) {
                return false;
            }
        }

    }
    
    // create a copy of the feature location called new_location
    CRef<CSeq_loc> new_location(new CSeq_loc);
    new_location->Assign(feat->GetLocation());
    
    // adjust the last piece of new_location to be the right length to
    // generate the desired protein length
    CSeq_loc_CI loc_it(*new_location);
    CSeq_loc_CI last_it = loc_it;
    
    while (loc_it) {
        last_it = loc_it;
        ++loc_it;
    }
    
    if (!last_it || !last_it.GetSeq_loc().IsInt()) {
        return false;
    }
    
    CBioseq_Handle last_seq = m_Scope->GetBioseqHandle(last_it.GetSeq_loc());
    
	switch (remainder)
	{
		case 0:
			remainder = 3;
			break;
		case 1:
			remainder = 2;
			break;
		case 2:
			remainder = 1;
			break;
	}

    TSeqPos old_from = last_it.GetRange().GetFrom();
	TSeqPos old_to = last_it.GetRange().GetTo();

    if (last_it.GetStrand() == eNa_strand_minus) {
		if (old_from < remainder) {
			return false;
		} else if (last_it.GetFuzzFrom() != NULL) {
		    return false;
		} else {
		    (const_cast <CSeq_loc& > (last_it.GetSeq_loc())).SetInt().SetFrom (old_from - remainder);
		}
	}
	else
	{
	    if (old_to >= last_seq.GetBioseqLength() - remainder) {
	        return false;
	    } else if (last_it.GetFuzzTo() != NULL) {
	        return false;
	    } else {
		    (const_cast <CSeq_loc& > (last_it.GetSeq_loc())).SetInt().SetTo (old_to + remainder);
	    }
	}

    // get new protein sequence by translating the coding region
    CBioseq_Handle nuc_bsh = m_Scope->GetBioseqHandle(feat->GetLocation());
    string data;
    CCdregion_translate::TranslateCdregion(data, nuc_bsh,
                                           *new_location,
                                           crp,
                                           true,
                                           true);

    // if the translation is the wrong length, give up
	if (data.length() != (frame_adjusted_len + remainder) / 3) {
	    return false;
	}

    // if the translation doesn't end with stop codon, give up
    if (!NStr::Equal(data.substr(data.length() - 1), "*")) {
        return false;
    }
    
    // get existing protein data
    string prot_buffer;
    product.GetSeqVector(CBioseq_Handle::eCoding_Iupac).GetSeqData(0, product.GetBioseqLength() - 1, prot_buffer);
    // if the translation doesn't match the existing protein, give up
    if (!NStr::Equal(data.substr(0, data.length() - 2), prot_buffer)) {
        return false;
    }

    // fix location for overlapping gene
    const CGene_ref* grp = feat->GetGeneXref();
    if (grp == NULL) { // NOTE - in C Toolkit also do this if grp is not suppressed
        CConstRef<CSeq_feat> cr = sequence::GetOverlappingGene(feat->GetLocation(), *m_Scope);
        if (!cr.IsNull()) {        
            CSeq_feat_Handle fh = GetSeq_feat_Handle(*m_Scope, *cr);
            
            if (!fh.GetSeq_feat().IsNull()) {
                CRef<CSeq_feat> gene_feat(new CSeq_feat);
            
                gene_feat->Assign(*cr);
                sequence::ECompare loc_compare = sequence::Compare(*new_location, gene_feat->GetLocation(), m_Scope);

                if (loc_compare != sequence::eContained && loc_compare != sequence::eSame) {
                    CSeq_loc& gene_loc = gene_feat->SetLocation();
            
                    CSeq_loc_CI tmp(gene_loc);
                    bool has_nulls = false;
                    while (tmp && !has_nulls) {
                        if (tmp.GetSeq_loc().IsNull()) {
                            has_nulls = true;
                        }
                        ++tmp;
                    }
                    CRef<CSeq_loc> new_gene_loc = sequence::Seq_loc_Add(gene_loc, *new_location, 
                                                        CSeq_loc::fMerge_SingleRange, m_Scope);
                                                
                    new_gene_loc->SetPartialStart (new_location->IsPartialStart(eExtreme_Biological) | gene_loc.IsPartialStart(eExtreme_Biological), eExtreme_Biological);
                    new_gene_loc->SetPartialStop (new_location->IsPartialStop(eExtreme_Biological) | gene_loc.IsPartialStop(eExtreme_Biological), eExtreme_Biological);
                    // Note - C version pushes gene location to segset parts         
  
                    gene_feat->SetLocation (*new_gene_loc);          
                    fh.Replace(*gene_feat);
                }        
            }
		}
	}
	
    // fix location of coding region
    feat->SetLocation(*new_location);
    
    ofh.Replace(*feat);
    return true;
}


void CCleanup_imp::x_CheckCodingRegionEnds (CSeq_annot_Handle sa)
{
    if (sa.IsFtable()) {
        CFeat_CI feat_ci(sa);
        while (feat_ci) {
            x_CheckCodingRegionEnds(const_cast<CSeq_feat &> (feat_ci->GetOriginalFeature()));
            ++feat_ci;                
        }
    }
}


bool s_IsmRNA(CBioseq_Handle bsh)
{
    bool is_mRNA = false;
    for (CSeqdesc_CI desc(bsh, CSeqdesc::e_Molinfo); desc && !is_mRNA; ++desc) {
        if (desc->GetMolinfo().CanGetBiomol()
            && desc->GetMolinfo().GetBiomol() == CMolInfo::eBiomol_mRNA) {
            is_mRNA = true;
        }
    }
    return is_mRNA;
}


// Was ExtendSingleGeneOnMRNA in C Toolkit
// Will change the location of a gene on an mRNA sequence to
// cover the entire sequence, as long as there is only one gene
// present and zero or one coding regions present.
void CCleanup_imp::x_ExtendSingleGeneOnmRNA (CBioseq_Handle bsh)
{
    if (!bsh.CanGetId() || !bsh.CanGetDescr()) {
        return;
    }

    if (!s_IsmRNA(bsh)) {
        return;
    }
    
    int num_genes = 0;
    int num_cdss = 0;
    
    CFeat_CI gene_it;
    
    for (CFeat_CI feat_ci(bsh); num_genes < 2 && num_cdss < 2; ++feat_ci) {
        if (feat_ci->GetFeatType() == CSeqFeatData::e_Gene) {
            num_genes ++;
            if (num_genes == 1) {
                gene_it = feat_ci;
            }
        } else if (feat_ci->GetFeatType() == CSeqFeatData::e_Cdregion) {
            num_cdss++;
        }
    }
    
    if (num_genes == 1 && num_cdss < 2) {
        CBioseq_Handle gene_bsh = m_Scope->GetBioseqHandle(gene_it->GetLocation());
        if (gene_bsh == bsh) {
            CSeq_feat_Handle fh = GetSeq_feat_Handle(*m_Scope, gene_it->GetOriginalFeature());
        
            if (!fh.GetSeq_feat().IsNull()) {
                CRef<CSeq_feat> new_gene(new CSeq_feat);
                new_gene->Assign(gene_it->GetOriginalFeature());
                CRef<CSeq_loc> new_loc(new CSeq_loc);
        
                CRef<CSeq_id> new_id(new CSeq_id);
                new_id->Assign(*(new_gene->GetLocation().GetId()));
                new_loc->SetInt().SetId(*new_id);
                new_loc->SetInt().SetFrom(0);
                new_loc->SetInt().SetTo(bsh.GetBioseqLength() - 1);
                new_loc->SetInt().SetStrand(new_gene->GetLocation().GetStrand());
                new_gene->SetLocation(*new_loc);
            
                fh.Replace(*new_gene);
            }
        }
    }  
}


void CCleanup_imp::x_ExtendSingleGeneOnmRNA (CBioseq_set_Handle bssh)
{
    // only perform this operation if the set contains one and only one mRNA sequence,
    // one and only one gene, and zero or one coding regions.

    int num_mRNA = 0;
    CBioseq_Handle first_mRNA;
    
    for (CBioseq_CI bioseq_ci(bssh); bioseq_ci && num_mRNA < 2; ++bioseq_ci) {
        if (s_IsmRNA(*bioseq_ci)) {
            num_mRNA++;
            if (num_mRNA == 1) {
                first_mRNA = m_Scope->GetBioseqHandle(bioseq_ci->GetId().front());
            }
        }
    }
    
    if (num_mRNA != 1) {
        return;
    }
    
    int num_genes = 0;
    int num_cdss = 0;
    
    CFeat_CI gene_it;
    
    for (CFeat_CI feat_ci(bssh.GetParentEntry()); feat_ci && num_genes < 2 && num_cdss < 2; ++feat_ci) {
        if (feat_ci->GetFeatType() == CSeqFeatData::e_Gene) {
            num_genes ++;
            if (num_genes == 1) {
                gene_it = feat_ci;
            }
        } else if (feat_ci->GetFeatType() == CSeqFeatData::e_Cdregion) {
            num_cdss++;
        }
    }
    
    if (num_genes == 1 && num_cdss < 2) {
        CBioseq_Handle gene_bsh = m_Scope->GetBioseqHandle(gene_it->GetLocation());
        if (gene_bsh == first_mRNA) {
            CSeq_feat_Handle fh = GetSeq_feat_Handle(*m_Scope, gene_it->GetOriginalFeature());
        
            if (!fh.GetSeq_feat().IsNull()) {
                CRef<CSeq_feat> new_gene(new CSeq_feat);
                new_gene->Assign(gene_it->GetOriginalFeature());
                CRef<CSeq_loc> new_loc(new CSeq_loc);
        
                CRef<CSeq_id> new_id(new CSeq_id);
                new_id->Assign(*(new_gene->GetLocation().GetId()));
                new_loc->SetInt().SetId(*new_id);
                new_loc->SetInt().SetFrom(0);
                new_loc->SetInt().SetTo(gene_bsh.GetBioseqLength() - 1);
                new_loc->SetInt().SetStrand(new_gene->GetLocation().GetStrand());
                new_gene->SetLocation(*new_loc);
            
                fh.Replace(*new_gene);
            }
        }
    }  
}


END_objects_SCOPE // namespace ncbi::objects::

END_NCBI_SCOPE

/*
 * ===========================================================================
 *
 * $Log$
 * Revision 1.17  2006/07/11 14:38:28  bollin
 * aadded a step to ExtendedCleanup to extend the only gene found on the only
 * mRNA sequence in the set where there are zero or one coding region features
 * in the set so that the gene covers the entire mRNA sequence
 *
 * Revision 1.16  2006/07/11 14:19:30  ucko
 * Don't bother clear()ing newly allocated strings, particularly given
 * that GCC 2.95 only accepts erase() anyway.
 *
 * Revision 1.15  2006/07/10 19:01:57  bollin
 * added step to extend coding region to cover missing portion of a stop codon,
 * will also adjust the location of the overlapping gene if necessary.
 *
 * Revision 1.14  2006/07/05 16:43:34  bollin
 * added step to ExtendedCleanup to clean features and descriptors
 * and remove empty feature table seq-annots
 *
 * Revision 1.13  2006/07/03 18:45:00  bollin
 * changed methods in ExtendedCleanup for correcting exception text and moving
 * dbxrefs to use edit handles
 *
 * Revision 1.12  2006/06/28 15:23:03  bollin
 * added step to move db_xref GenBank Qualifiers to real dbxrefs to Extended Cleanup
 *
 * Revision 1.11  2006/06/27 14:30:59  bollin
 * added step for correcting exception text to ExtendedCleanup
 *
 * Revision 1.10  2006/05/17 18:18:08  ucko
 * Fix compilation error introduced in previous revision -- don't compare
 * iterators to NULL, as it is neither necessary nor portable to do so.
 *
 * Revision 1.9  2006/05/17 17:39:36  bollin
 * added parsing and cleanup of anticodon qualifiers on tRNA features and
 * transl_except qualifiers on coding region features
 *
 * Revision 1.8  2006/04/19 19:59:59  ludwigf
 * FIXED: Comparision routines for GB qualifiers.
 *
 * Revision 1.7  2006/04/18 14:32:36  rsmith
 * refactoring
 *
 * Revision 1.6  2006/04/17 17:03:12  rsmith
 * Refactoring. Get rid of cleanup-mode parameters.
 *
 * Revision 1.5  2006/03/29 19:40:13  rsmith
 * Consolidate BasicCleanup(CRNA_ref) stuff.
 *
 * Revision 1.4  2006/03/29 16:36:56  rsmith
 * Move all gbqual stuff to its own file.
 * Lots of other refactoring.
 *
 * Revision 1.3  2006/03/23 18:33:32  rsmith
 * Separate straight BasicCleanup of SFData and GBQuals from more complicated
 * changes to various parts of the Seq_feat.
 *
 * Revision 1.2  2006/03/20 14:21:25  rsmith
 * move Biosource related cleanup to its own file.
 *
 * Revision 1.1  2006/03/14 20:21:50  rsmith
 * Move BasicCleanup functionality from objects to objtools/cleanup
 *
 *
 * ===========================================================================
 */
