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
* Author:  Colleen Bollin
*
* File Description:
*   Generate unique definition lines for a set of sequences using organism
*   descriptions and feature clauses.
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbimisc.hpp>
#include <objmgr/util/autodef.hpp>
#include <objmgr/seqdesc_ci.hpp>
#include <objmgr/bioseq_ci.hpp>
#include <objmgr/feat_ci.hpp>
#include <objmgr/util/feature.hpp>
#include <objmgr/util/sequence.hpp>

#include <objects/seq/Seq_descr.hpp>
#include <objects/seq/Seqdesc.hpp>
#include <objects/seq/Bioseq.hpp>

#include <serial/iterator.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

using namespace sequence;

CAutoDefFeatureClause::CAutoDefFeatureClause(CBioseq_Handle bh, const CSeq_feat& main_feat) 
                              : m_BH (bh),
                              m_MainFeat(main_feat)
{
    x_SetBiomol();
    m_ClauseList.clear();
    m_GeneName = "";
    m_AlleleName = "";
    m_Interval = "";
    m_IsAltSpliced = false;
    m_Pluralizable = false;
    m_ShowTypewordFirst = false;
    m_TypewordChosen = x_GetFeatureTypeWord(m_Typeword);
    m_Description = "";
    m_DescriptionChosen = false;
    m_ProductName = "";
    m_ProductNameChosen = false;
    
    CSeqFeatData::ESubtype subtype = m_MainFeat.GetData().GetSubtype();
    
    if (subtype == CSeqFeatData::eSubtype_gene) {
        m_GeneName = x_GetGeneName(m_MainFeat.GetData().GetGene());
        if (m_MainFeat.GetData().GetGene().CanGetAllele()) {
            m_AlleleName = m_MainFeat.GetData().GetGene().GetAllele();
        }
        if (m_MainFeat.CanGetPseudo() && m_MainFeat.GetPseudo()) {
            m_GeneIsPseudo = true;
        }
        m_HasGene = true;
    }
    
    m_ClauseLocation = new CSeq_loc();
    m_ClauseLocation->Add(m_MainFeat.GetLocation());
    
    if (subtype == CSeqFeatData::eSubtype_operon || IsGeneCluster()) {
        m_SuppressSubfeatures = true;
    }
}


CAutoDefFeatureClause::~CAutoDefFeatureClause()
{
}


CSeqFeatData::ESubtype CAutoDefFeatureClause::GetMainFeatureSubtype()
{
    return m_MainFeat.GetData().GetSubtype();
}


bool CAutoDefFeatureClause::IsTransposon()
{
    if (m_MainFeat.GetData().GetSubtype() != CSeqFeatData::eSubtype_repeat_region
        || NStr::IsBlank(m_MainFeat.GetNamedQual("transposon"))) {
        return false;
    } else {
        return true;
    }
}


bool CAutoDefFeatureClause::IsInsertionSequence()
{
    if (m_MainFeat.GetData().GetSubtype() != CSeqFeatData::eSubtype_repeat_region
        || NStr::IsBlank(m_MainFeat.GetNamedQual("insertion_seq"))) {
        return false;
    } else {
        return true;
    }
}


bool CAutoDefFeatureClause::IsEndogenousVirusSourceFeature ()
{
    if (m_MainFeat.GetData().GetSubtype() != CSeqFeatData::eSubtype_source
        || !m_MainFeat.GetData().GetBiosrc().CanGetSubtype()) {
        return false;
    }
    ITERATE (CBioSource::TSubtype, subSrcI, m_MainFeat.GetData().GetBiosrc().GetSubtype()) {
        if ((*subSrcI)->GetSubtype() == CSubSource::eSubtype_endogenous_virus_name) {
             return true;
        }
    }
    return false;
}


bool CAutoDefFeatureClause::IsGeneCluster ()
{
    if (m_MainFeat.GetData().GetSubtype() != CSeqFeatData::eSubtype_misc_feature
        || !m_MainFeat.CanGetComment()) {
        return false;
    }
    
    string comment = m_MainFeat.GetComment();
    if (NStr::Equal(comment, "gene cluster") || NStr::Equal(comment, "gene locus")) {
        return true;
    } else {
        return false;
    }
}


bool CAutoDefFeatureClause::IsRecognizedFeature()
{
    CSeqFeatData::ESubtype subtype = m_MainFeat.GetData().GetSubtype();
    if (subtype == CSeqFeatData::eSubtype_3UTR
        || subtype == CSeqFeatData::eSubtype_5UTR
        || subtype == CSeqFeatData::eSubtype_cdregion
        || subtype == CSeqFeatData::eSubtype_gene
        || subtype == CSeqFeatData::eSubtype_mRNA
        || subtype == CSeqFeatData::eSubtype_operon
        || subtype == CSeqFeatData::eSubtype_promoter
        || IsTransposon()
        || IsInsertionSequence()
        || IsIntergenicSpacer()
        || IsEndogenousVirusSourceFeature()
        || IsSatelliteClause()
        || IsGeneCluster()) {
        return true;
    } else {
        return false;
    }
}



void CAutoDefFeatureClause::x_SetBiomol()
{
    CSeqdesc_CI desc_iter(m_BH, CSeqdesc::e_Molinfo);
    for ( ;  desc_iter;  ++desc_iter) {
        if (desc_iter->GetMolinfo().IsSetBiomol()) {
            m_Biomol = desc_iter->GetMolinfo().GetBiomol();
        }
    }
}


bool CAutoDefFeatureClause::x_GetFeatureTypeWord(string &typeword)
{
    string qual, comment;
  
    CSeqFeatData::ESubtype subtype = m_MainFeat.GetData().GetSubtype();
    switch (subtype) {
        case CSeqFeatData::eSubtype_exon:
            typeword = "exon";
            return true;
            break;
        case CSeqFeatData::eSubtype_intron:
            typeword = "intron";
            return true;
            break;
        case CSeqFeatData::eSubtype_D_loop:
            typeword = "D-loop";
            return true;
            break;
        case CSeqFeatData::eSubtype_LTR:
            typeword = "LTR";
            return true;
            break;
        case CSeqFeatData::eSubtype_3UTR:
            typeword = "3' UTR";
            return true;
            break;
        case CSeqFeatData::eSubtype_5UTR:
            typeword = "5' UTR";
            return true;
            break;
        case CSeqFeatData::eSubtype_operon:
            typeword = "operon";
            return true;
            break;
        case CSeqFeatData::eSubtype_repeat_region:
            //if has insertion_seq gbqual
            if (IsInsertionSequence()) {
                typeword = "insertion sequence";
                return true;
            }
            qual = m_MainFeat.GetNamedQual("endogenous_virus");
            if (!NStr::IsBlank(qual)) {
                typeword = "endogenous virus";
                return true;
            }
            if (IsTransposon()) {
                typeword = "transposon";
                return true;
            }
            break;
        case CSeqFeatData::eSubtype_misc_feature:
            if (m_MainFeat.CanGetComment()) {
                comment = m_MainFeat.GetComment();
                if (NStr::StartsWith(comment, "control region", NStr::eNocase)) {
                    typeword = "control region";
                    return true;
                }
            }
            break;
        case CSeqFeatData::eSubtype_source:
            if (IsEndogenousVirusSourceFeature()) {
                typeword = "endogenous virus";
                return true;
            }
            break;
    }
    
    if (m_Biomol == CMolInfo::eBiomol_genomic) {
        if (m_MainFeat.CanGetPseudo() && m_MainFeat.IsSetPseudo()) {
            typeword = "pseudogene";
            return true;
        } else {
            typeword = "gene";
            return true;
        }
    } else if (subtype == CSeqFeatData::eSubtype_rRNA 
               || subtype == CSeqFeatData::eSubtype_snoRNA
               || subtype == CSeqFeatData::eSubtype_snRNA) {
        return false;
    } else if (subtype == CSeqFeatData::eSubtype_precursor_RNA) {
        typeword = "precursor RNA";
        return true;
    } else if (m_Biomol == CMolInfo::eBiomol_mRNA) {
        if (m_MainFeat.CanGetPseudo() && m_MainFeat.IsSetPseudo()) {
            typeword = "pseudogene mRNA";
        } else {
            typeword = "mRNA";
        }
        return true;
    } else if (m_Biomol == CMolInfo::eBiomol_pre_RNA) {
        if (m_MainFeat.CanGetPseudo() && m_MainFeat.IsSetPseudo()) {
            typeword = "pseudogene precursor mRNA";
        } else {
            typeword = "precursor mRNA";
        }
        return true;
    } else if (m_Biomol == CMolInfo::eBiomol_other_genetic) {
        typeword = "gene";
        return true;
    }
    typeword = "";
    return true;
}


bool CAutoDefFeatureClause::x_ShowTypewordFirst(string typeword)
{
    if (NStr::Equal(typeword, "")) {
        return false;
    } else if (NStr::EqualNocase(typeword, "exon")
               || NStr::EqualNocase(typeword, "intron")
               || NStr::EqualNocase(typeword, "transposon")
               || NStr::EqualNocase(typeword, "insertion sequence")
               || NStr::EqualNocase(typeword, "endogenous virus")
               || NStr::EqualNocase(typeword, "retrotransposon")             
               || NStr::EqualNocase(typeword, "P-element")
               || NStr::EqualNocase(typeword, "transposable element")
               || NStr::EqualNocase(typeword, "integron")
               || NStr::EqualNocase(typeword, "superintegron")
               || NStr::EqualNocase(typeword, "MITE")) {
        return true;
    } else {
        return false;
    }
}


bool CAutoDefFeatureClause::x_FindNoncodingFeatureKeywordProduct (string comment, string keyword, string &product_name)
{
    if (NStr::IsBlank(comment) || NStr::IsBlank(keyword)) {
        return false;
    }
    unsigned int start_pos = 0;
    unsigned int accession_pos;
    
    while (start_pos != NCBI_NS_STD::string::npos) {
        start_pos = NStr::Find(comment, keyword, start_pos);
        if (start_pos != NCBI_NS_STD::string::npos) {
            accession_pos = NStr::Find(comment, "GenBank Accession Number", start_pos);
            if (accession_pos == 0) {
                product_name = comment.substr(start_pos + keyword.length());
                // truncate at first semicolon
                unsigned int end = NStr::Find(product_name, ";");
                if (end != NCBI_NS_STD::string::npos) {
                    product_name = product_name.substr(0, end);
                }
                // remove sequence from end of product name if found
                if (NStr::EndsWith(product_name, " sequence")) {
                    product_name = product_name.substr(0, product_name.length() - 9);
                }
                // add "-like" if not present
                if (!NStr::EndsWith(product_name, "-like")) {
                    product_name += "-like";
                }
                return true;
            } else {
                start_pos += keyword.length();
            }
        }
    }
    return false;
}


bool CAutoDefFeatureClause::x_GetNoncodingProductFeatProduct (string &product_name)
{
    if (m_MainFeat.GetData().GetSubtype() != CSeqFeatData::eSubtype_misc_feature
        || !m_MainFeat.CanGetComment()) {
        return false;
    }
    string comment = m_MainFeat.GetComment();
    unsigned int start_pos = NStr::Find(comment, "nonfunctional ");
    if (start_pos != NCBI_NS_STD::string::npos) {
        unsigned int sep_pos = NStr::Find (comment, " due to ", start_pos);
        if (sep_pos != NCBI_NS_STD::string::npos) {
            product_name = comment.substr(start_pos + 14, sep_pos - start_pos - 14);
            return true;
        }
    }
    if (x_FindNoncodingFeatureKeywordProduct (comment, "similar to ", product_name)) {
        return true;
    } else if (x_FindNoncodingFeatureKeywordProduct (comment, "contains ", product_name)) {
        return true;
    } else {
        return false;
    }
}

bool CAutoDefFeatureClause::IsNoncodingProductFeat() 
{
    string product_name;
    return x_GetNoncodingProductFeatProduct(product_name);
}


bool CAutoDefFeatureClause::IsIntergenicSpacer()
{
    CSeqFeatData::ESubtype subtype = m_MainFeat.GetData().GetSubtype();
    if ((subtype != CSeqFeatData::eSubtype_misc_feature && subtype != CSeqFeatData::eSubtype_otherRNA)
        || !m_MainFeat.CanGetComment()) {
        return false;
    }
    string comment = m_MainFeat.GetComment();
    if (NStr::Find(comment, "intergenic spacer") != NCBI_NS_STD::string::npos) {
        return true;
    } else {
        return false;
    }
}


string CAutoDefFeatureClause::x_GetGeneName(const CGene_ref& gref)
{
#if 1
    if (gref.IsSuppressed()) {
        return "";
    } else if (gref.CanGetLocus() && !NStr::IsBlank(gref.GetLocus())) {
        return gref.GetLocus();
    } else {
        return "";
    }
#else
    if (m_MainFeat.GetData().GetSubtype() != CSeqFeatData::eSubtype_gene
        || m_MainFeat.GetData().GetGene().IsSuppressed()) {
        return "";
    } else if (m_MainFeat.GetData().GetGene().CanGetLocus()
               && !NStr::IsBlank(m_MainFeat.GetData().GetGene().GetLocus())) {
        return m_MainFeat.GetData().GetGene().GetLocus();
    } else if (!m_SuppressLocusTag
               && m_MainFeat.GetData().GetGene().CanGetLocus_tag()
               && !NStr::IsBlank(m_MainFeat.GetData().GetGene().GetLocus_tag())) {
        return m_MainFeat.GetData().GetGene().GetLocus_tag();
    } else if (m_MainFeat.GetData().GetGene().CanGetDesc()
               && !NStr::IsBlank(m_MainFeat.GetData().GetGene().GetDesc())) {
        return m_MainFeat.GetData().GetGene().GetDesc();
    } else if (m_MainFeat.GetData().GetGene().CanGetSyn()
               && m_MainFeat.GetData().GetGene().IsSetSyn()) {
        return m_MainFeat.GetData().GetGene().GetSyn().front();
    } else {
        return "";
    }
#endif        
}


/* Frequently the product associated with a feature is listed as part of the
 * description of the feature in the definition line.  This function determines
 * the name of the product associated with this specific feature.  Some
 * features will be listed with the product of a feature that is associated
 * with the feature being described - this function does not look at other
 * features to determine a product name.
 * If the feature is a misc_feat with particular keywords in the comment,
 * the product will be determined based on the contents of the comment.
 * If the feature is a CDS and is marked as pseudo, the product will be
 * determined based on the contents of the comment.
 * If the feature is a gene and has different strings in the description than
 * in the locus or locus tag, the description will be used as the product for
 * the gene.
 * If none of the above conditions apply, the sequence indexing context label
 * will be used to obtain the product name for the feature.
 */
bool CAutoDefFeatureClause::x_GetProductName(string &product_name)
{
    CSeqFeatData::ESubtype subtype = m_MainFeat.GetData().GetSubtype();
    
    if (subtype == CSeqFeatData::eSubtype_misc_feature && x_GetNoncodingProductFeatProduct(product_name)) {
        return true;
    } else if (subtype == CSeqFeatData::eSubtype_cdregion 
               && m_MainFeat.CanGetPseudo() 
               && m_MainFeat.IsSetPseudo()
               && m_MainFeat.CanGetComment()) {
        string comment = m_MainFeat.GetComment();
        if (!NStr::IsBlank(comment)) {
            unsigned int pos = NStr::Find(comment, ";");
            if (pos != NCBI_NS_STD::string::npos) {
                comment = comment.substr(0, pos);
            }
            product_name = comment;
            return true;
        }
    } else if (subtype == CSeqFeatData::eSubtype_gene) {
        if (m_MainFeat.GetData().GetGene().CanGetDesc()
            && !NStr::Equal(m_MainFeat.GetData().GetGene().GetDesc(),
                            m_GeneName)) {
            product_name = m_MainFeat.GetData().GetGene().GetDesc();
            return true;
        } else {
            return false;
        }
    } else {
        string label;
        
        if (m_MainFeat.CanGetProduct()) {
            CConstRef<CSeq_feat> prot = GetBestOverlappingFeat(m_MainFeat.GetProduct(),
                                                               CSeqFeatData::e_Prot,
                                                               eOverlap_Simple,
                                                               m_BH.GetScope());
            if (prot) {
                feature::GetLabel(*prot, &label, feature::eContent);
            }
        }
        
        if (NStr::IsBlank(label)) {                    
            feature::GetLabel(m_MainFeat, &label, feature::eContent);
        }
        if (subtype == CSeqFeatData::eSubtype_tRNA) {
            if (NStr::Equal(label, "tRNA-Xxx")) {
                label = "tRNA-OTHER";
            } else {
                label = "tRNA-" + label;
            }
            return true;
        } else if ((subtype == CSeqFeatData::eSubtype_cdregion && !NStr::Equal(label, "CDS"))
                   || (subtype == CSeqFeatData::eSubtype_mRNA && !NStr::Equal(label, "mRNA"))
                   || (subtype != CSeqFeatData::eSubtype_cdregion && subtype != CSeqFeatData::eSubtype_mRNA)) {
        } else {
            label = "";
        }
        
        // remove unwanted "mRNA-" tacked onto label for mRNA features
        if (subtype == CSeqFeatData::eSubtype_mRNA && NStr::StartsWith(label, "mRNA-")) {
            label = label.substr(5);
        }
        
        if (!NStr::IsBlank(label)) {
            unsigned int pos = NStr::Find(label, ";");
            if (pos != NCBI_NS_STD::string::npos) {
                label = label.substr(0, pos);
            }
        }
        if (!NStr::IsBlank(label)) {
            product_name = label;
            return true;
        } else {
            product_name = "";
            return false;
        }        
    }
    return false;
}


bool CAutoDefFeatureClause::x_GetExonDescription(string &description)
{
    CSeqFeatData::ESubtype subtype = m_MainFeat.GetData().GetSubtype();
    string label;
    
    feature::GetLabel(m_MainFeat, &label, feature::eContent);

    if ((subtype == CSeqFeatData::eSubtype_exon && NStr::Equal(label, "exon"))
        || (subtype == CSeqFeatData::eSubtype_intron && NStr::Equal(label, "intron"))
        || (subtype != CSeqFeatData::eSubtype_exon && subtype != CSeqFeatData::eSubtype_intron)) {
        description = "";
        return false;
    } else {
        description = label;
        return true;
    }
}


bool CAutoDefFeatureClause::x_GetDescription(string &description)
{
    CSeqFeatData::ESubtype subtype = m_MainFeat.GetData().GetSubtype();

    description = "";
    if (subtype == CSeqFeatData::eSubtype_exon || subtype == CSeqFeatData::eSubtype_intron) {
        return x_GetExonDescription(description);
    } else if (NStr::Equal(m_Typeword, "insertion sequence")) {
        description = m_MainFeat.GetNamedQual("insertion_seq");
        if (NStr::Equal(description, "unnamed")
            || NStr::IsBlank(description)) {
            description = "";
            return false;
        } else {
            return true;
        }
    } else if (subtype == CSeqFeatData::eSubtype_repeat_region
               && NStr::Equal(m_Typeword, "endogenous virus")) {
        description = m_MainFeat.GetNamedQual("endogenous_virus");
        if (NStr::Equal(description, "unnamed")
            || NStr::IsBlank(description)) {
            description = "";
            return false;
        } else {
            return true;
        }
    } else if (subtype == CSeqFeatData::eSubtype_source
               && NStr::Equal(m_Typeword, "endogenous virus")) {
        if (m_MainFeat.GetData().GetBiosrc().CanGetSubtype()) {
            ITERATE (CBioSource::TSubtype, subSrcI, m_MainFeat.GetData().GetBiosrc().GetSubtype()) {
                if ((*subSrcI)->GetSubtype() == CSubSource::eSubtype_endogenous_virus_name) {
                    description = (*subSrcI)->GetName();
                    if (NStr::Equal(description, "unnamed")
                        || NStr::IsBlank(description)) {
                        description = "";
                    } else {
                        return true;
                    }                    
                }
            }
        }
        return false;
    } else if (NStr::Equal(m_Typeword, "control region")
               || NStr::Equal(m_Typeword, "D loop")
               || subtype == CSeqFeatData::eSubtype_3UTR
               || subtype == CSeqFeatData::eSubtype_5UTR) {
        return false;
    } else if (subtype == CSeqFeatData::eSubtype_LTR) {
        if (m_MainFeat.CanGetComment()) {
            string comment = m_MainFeat.GetComment();
            if (NStr::StartsWith(comment,"LTR ")) {
                comment = comment.substr(4);
            }
            description = comment;
        }
        if (NStr::IsBlank(description)) {
            return false;
        } else {
            return true;
        }
    } else {
        if (!m_ProductNameChosen) {
            m_ProductNameChosen = x_GetProductName(m_ProductName);
        }
        
        if (!NStr::IsBlank(m_GeneName) && !NStr::IsBlank(m_ProductName)) {
            description = m_ProductName + " (" + m_GeneName + ")";
        } else if (!NStr::IsBlank(m_GeneName)) {
            description = m_GeneName;
        } else if (!NStr::IsBlank(m_ProductName)) {
            description = m_ProductName;
        }
        if (NStr::IsBlank(description)) {
            return false;
        } else {
            return true;
        }
   }
}


bool CAutoDefFeatureClause::IsSatelliteClause() 
{
    if (m_MainFeat.GetData().GetSubtype() == CSeqFeatData::eSubtype_repeat_region
        && m_MainFeat.CanGetComment()) {
        string comment = m_MainFeat.GetComment();
        if (NStr::StartsWith(comment, "satellite")
            || NStr::StartsWith(comment, "microsatellite")) {
            string qual = m_MainFeat.GetNamedQual("rpt_type");
            if (NStr::Equal(qual, "tandem")) {
                return true;
            }
        }
    }
    return false;
}


/* This function calculates the "interval" for a clause in the definition
 * line.  The interval could be an empty string, it could indicate whether
 * the location of the feature is partial or complete and whether or not
 * the feature is a CDS, the interval could be a description of the
 * subfeatures of the clause, or the interval could be a combination of the
 * last two items if the feature is a CDS.
 */
bool CAutoDefFeatureClause::x_GetGenericInterval (string &interval)
{
    unsigned int k;
    
    interval = "";
    if (m_IsUnknown) {
        return false;
    }
    
    CSeqFeatData::ESubtype subtype = m_MainFeat.GetData().GetSubtype();
    if (subtype == CSeqFeatData::eSubtype_exon && m_IsAltSpliced) {
        interval = "alternatively spliced";
        return true;
    }
    
    if (IsSatelliteClause() || subtype == CSeqFeatData::eSubtype_promoter || subtype == CSeqFeatData::eSubtype_operon) {
        return false;
    }
    
    bool has_3UTR = false;
    unsigned int num_non3UTRclauses = 0;
    
    if (!m_SuppressSubfeatures) {
        // label subclauses
        // check to see if 3'UTR is present, and whether there are any other features
        for (k = 0; k < m_ClauseList.size(); k++) {
            m_ClauseList[k]->Label();
            if (m_ClauseList[k]->GetMainFeatureSubtype() == CSeqFeatData::eSubtype_3UTR) {
                has_3UTR = true;
            } else {
                num_non3UTRclauses++;
            }
        }
    
        // label any subclauses
        if (num_non3UTRclauses > 0) {
            bool suppress_final_and = true;
            if ((subtype == CSeqFeatData::eSubtype_cdregion && ! m_ClauseInfoOnly) || has_3UTR) {
                suppress_final_and = true;
            }
        
            // ConsolidateClauses
        
            // create subclause list for interval
            interval += ListClauses(false, suppress_final_and);
        
            interval += ", ";
            if (has_3UTR && subtype == CSeqFeatData::eSubtype_cdregion && ! m_ClauseInfoOnly) {
                interval += "and 3' UTR";
            }
        }
    }
    
    if (IsPartial()) {
        interval += "partial ";
    } else {
        interval += "complete ";
    }
    
    if (subtype == CSeqFeatData::eSubtype_cdregion
        && (!m_MainFeat.CanGetPseudo() || !m_MainFeat.GetPseudo())) {
        interval += "cds";
        if (m_IsAltSpliced) {
            interval += ", alternatively spliced";
        }
    } else {
        interval += "sequence";
        string product_name;
        if (m_IsAltSpliced && x_GetNoncodingProductFeatProduct (product_name)) {
            interval += ", alternatively spliced";
        }
    }

    if (has_3UTR && (subtype != CSeqFeatData::eSubtype_cdregion || num_non3UTRclauses == 0)) {
        /* tack UTR3 on at end of clause */
        interval += ", and 3' UTR";
    } 
  
    return true;
} 


void CAutoDefFeatureClause::Label()
{
    if (!m_TypewordChosen) {
        m_TypewordChosen = x_GetFeatureTypeWord(m_Typeword);
        m_ShowTypewordFirst = x_ShowTypewordFirst(m_Typeword);
        m_Pluralizable = true;
    }
    if (!m_ProductNameChosen) {
        m_ProductNameChosen = x_GetProductName(m_ProductName);
    }
    if (!m_DescriptionChosen) {
        m_DescriptionChosen = x_GetDescription(m_Description);
    }
    
    x_GetGenericInterval (m_Interval);

}


sequence::ECompare CAutoDefFeatureClause::CompareLocation(const CSeq_loc& loc)
{
    return sequence::Compare(loc, *m_ClauseLocation, &(m_BH.GetScope()));
}


bool CAutoDefFeatureClause::SameStrand(const CSeq_loc& loc)
{
    ENa_strand loc_strand = loc.GetStrand();
    ENa_strand this_strand = m_ClauseLocation->GetStrand();
    
    if ((loc_strand == eNa_strand_minus && this_strand != eNa_strand_minus)
        || (loc_strand != eNa_strand_minus && this_strand == eNa_strand_minus)) {
        return false;
    } else {
        return true;
    }
    
}

bool CAutoDefFeatureClause::IsPartial()
{
    if (m_ClauseLocation->IsPartialStart(eExtreme_Biological)
        || m_ClauseLocation->IsPartialStop(eExtreme_Biological)) {
        return true;
    } else {
        return false;
    }
}


CRef<CSeq_loc> CAutoDefFeatureClause::GetLocation()
{
    return m_ClauseLocation;
}


void CAutoDefFeatureClause::AddToOtherLocation(CRef<CSeq_loc> loc)
{
    loc->Add(*m_ClauseLocation);
}


void CAutoDefFeatureClause::AddToLocation(CRef<CSeq_loc> loc)
{
    m_ClauseLocation->Add(*loc);
}


/* This function searches this list for clauses to which this mRNA should
 * apply.  This is not taken care of by the GroupAllClauses function
 * because when an mRNA is added to a CDS, the product for the clause is
 * replaced and the location for the clause is expanded, rather than simply
 * adding the mRNA as an additional feature in the list, and because an 
 * mRNA can apply to more than one clause, while other features should 
 * really only belong to one clause.
 */
bool CAutoDefFeatureClause::AddmRNA (CAutoDefFeatureClause_Base *mRNAClause)
{
    bool used_mRNA = false;
    string clause_product, mRNA_product;
    
    if (mRNAClause == NULL || ! mRNAClause->SameStrand(*m_ClauseLocation)) {
        return false;
    }
    
    CSeqFeatData::ESubtype subtype = m_MainFeat.GetData().GetSubtype();
    sequence::ECompare loc_compare = mRNAClause->CompareLocation(*m_ClauseLocation);
    
    if (subtype == CSeqFeatData::eSubtype_cdregion
        && m_ProductNameChosen
        && NStr::Equal(m_ProductName, mRNAClause->GetProductName())
        && (loc_compare == sequence::eContained || loc_compare == sequence::eSame)) {
        m_HasmRNA = true;
        mRNAClause->AddToOtherLocation(m_ClauseLocation);
        used_mRNA = true;
    } else if ((subtype == CSeqFeatData::eSubtype_cdregion || subtype == CSeqFeatData::eSubtype_gene)
               && !m_ProductNameChosen
               && (loc_compare == sequence::eContained
                   || loc_compare == sequence::eContains
                   || loc_compare == sequence::eSame)) {
        m_HasmRNA = true;
        mRNAClause->AddToOtherLocation(m_ClauseLocation);
        used_mRNA = true;
        m_ProductName = mRNAClause->GetProductName();
        m_ProductNameChosen = true;
    }       
    
  return used_mRNA;
}


bool CAutoDefFeatureClause::x_MatchGene(const CGene_ref& gref)
{
    if (!m_HasGene 
        || !NStr::Equal(x_GetGeneName(gref), m_GeneName)) {
        return false;
    }
    
    if (NStr::IsBlank(m_AlleleName)) {
        if (gref.CanGetAllele() && !NStr::IsBlank(gref.GetAllele())) {
            return false;
        } else {
            return true;
        }
    } else {
        if (!gref.CanGetAllele() || !NStr::Equal(m_AlleleName, gref.GetAllele())) {
            return false;
        } else {
            return true;
        }
    }          
}


/* This function searches this list for clauses to which this gene should
 * apply.  This is not taken care of by the GroupAllClauses function
 * because genes are added to clauses as a GeneRefPtr instead of as an
 * additional feature in the list, and because a gene can apply to more
 * than one clause, while other features should really only belong to
 * one clause.
 */
bool CAutoDefFeatureClause::AddGene (CAutoDefFeatureClause_Base *gene_clause) 
{
    bool used_gene = false;
    
    if (gene_clause == NULL || gene_clause->GetMainFeatureSubtype() != CSeqFeatData::eSubtype_gene) {
        return false;
    }
    
    CSeqFeatData::ESubtype subtype = m_MainFeat.GetData().GetSubtype();
    sequence::ECompare loc_compare = gene_clause->CompareLocation(*m_ClauseLocation);
    string noncoding_product_name;
    
    // only add gene to certain other types of clauses
    if (subtype != CSeqFeatData::eSubtype_cdregion
        && subtype != CSeqFeatData::eSubtype_rRNA
        && subtype != CSeqFeatData::eSubtype_tRNA
        && subtype != CSeqFeatData::eSubtype_misc_RNA
        && subtype != CSeqFeatData::eSubtype_snRNA
        && subtype != CSeqFeatData::eSubtype_snoRNA
        && subtype != CSeqFeatData::eSubtype_precursor_RNA
        && !x_GetNoncodingProductFeatProduct(noncoding_product_name)) {
        return false;
    }

    // find overlapping gene for this feature    
//    CConstRef<CSeq_feat> overlap = GetOverlappingGene(*m_ClauseLocation, m_BH.GetScope());
    
    if (m_HasGene) {
//        if (overlap && x_MatchGene(overlap->GetData().GetGene())) {
//            used_gene = true;
//        }            
    } else if ((loc_compare == sequence::eContained || loc_compare == sequence::eSame)
                && gene_clause->SameStrand(*m_ClauseLocation)) {
        used_gene = true;
        m_HasGene = true;
        m_GeneName = gene_clause->GetGeneName();
        m_AlleleName = gene_clause->GetAlleleName();
        m_GeneIsPseudo = gene_clause->GetGeneIsPseudo();
    }
    
    if (used_gene && ! m_ProductNameChosen) {
        Label();
        if (!m_ProductNameChosen) {
            m_ProductNameChosen = true;
            m_ProductName = gene_clause->GetProductName();
        }
    }
    
    return used_gene;
}


bool CAutoDefFeatureClause::OkToGroupUnderByType(CAutoDefFeatureClause_Base *parent_clause)
{
    bool ok_to_group = false;
    
    if (parent_clause == NULL) {
        return false;
    }
    CSeqFeatData::ESubtype subtype = m_MainFeat.GetData().GetSubtype();
    CSeqFeatData::ESubtype parent_subtype = parent_clause->GetMainFeatureSubtype();
    
    if (subtype == CSeqFeatData::eSubtype_exon || subtype == CSeqFeatData::eSubtype_intron) {
        if (parent_subtype == CSeqFeatData::eSubtype_cdregion
            || parent_subtype == CSeqFeatData::eSubtype_D_loop
            || parent_subtype == CSeqFeatData::eSubtype_mRNA
            || parent_subtype == CSeqFeatData::eSubtype_gene
            || parent_subtype == CSeqFeatData::eSubtype_operon
            || parent_clause->IsNoncodingProductFeat()
            || parent_clause->IsEndogenousVirusSourceFeature()
            || parent_clause->IsGeneCluster()) {
            ok_to_group = true;
        }
    } else if (subtype == CSeqFeatData::eSubtype_promoter) {
        if (parent_subtype == CSeqFeatData::eSubtype_cdregion
            || parent_subtype == CSeqFeatData::eSubtype_mRNA
            || parent_subtype == CSeqFeatData::eSubtype_gene
            || parent_subtype == CSeqFeatData::eSubtype_operon
            || parent_clause->IsEndogenousVirusSourceFeature()
            || parent_clause->IsGeneCluster()) {
            ok_to_group = true;
        }
    } else if (subtype == CSeqFeatData::eSubtype_cdregion) {
        if (parent_subtype == CSeqFeatData::eSubtype_mRNA
            || parent_clause->IsInsertionSequence()
            || parent_clause->IsTransposon()
            || parent_clause->IsEndogenousVirusSourceFeature()
            || parent_subtype == CSeqFeatData::eSubtype_operon
            || parent_clause->IsGeneCluster()) {
            ok_to_group = true;
        }
    } else if (IsInsertionSequence()
               || subtype == CSeqFeatData::eSubtype_gene
               || IsTransposon()
               || IsNoncodingProductFeat()
               || subtype == CSeqFeatData::eSubtype_operon
               || IsGeneCluster()
               || IsIntergenicSpacer()) {
        if (parent_clause->IsTransposon()
            || parent_clause->IsInsertionSequence()
            || parent_clause->IsEndogenousVirusSourceFeature()
            || parent_subtype == CSeqFeatData::eSubtype_operon
            || parent_clause->IsGeneCluster()) {
            ok_to_group = true;
        }
    } else if (subtype == CSeqFeatData::eSubtype_3UTR 
               || subtype == CSeqFeatData::eSubtype_5UTR
               || subtype == CSeqFeatData::eSubtype_LTR) {
        if (parent_subtype == CSeqFeatData::eSubtype_cdregion
            || parent_subtype == CSeqFeatData::eSubtype_mRNA
            || parent_subtype == CSeqFeatData::eSubtype_gene
            || parent_clause->IsEndogenousVirusSourceFeature()
            || parent_subtype == CSeqFeatData::eSubtype_operon
            || parent_clause->IsGeneCluster()) {
            ok_to_group = true;
        }
    }
         
    return ok_to_group;
}


// Transposons, insertion sequences, and endogenous virii
// take subfeatures regardless of whether the subfeature is
// on the same strand.
// Gene Clusters can optionally take subfeatures on either
// strand (gene_cluster_opp_strand is flag).
// Promoters will match up to features that are adjacent.
// Any feature on an mRNA sequence groups locationally.
// All other feature matches must be that the feature to
// go into the clause must fit inside the location of the
// other clause.
bool CAutoDefFeatureClause::OkToGroupUnderByLocation(CAutoDefFeatureClause_Base *parent_clause, bool gene_cluster_opp_strand)
{
    if (parent_clause == NULL) {
        return false;
    }
    
    if (m_Biomol == CMolInfo::eBiomol_mRNA) {
        return true;
    }
    
    sequence::ECompare loc_compare = parent_clause->CompareLocation(*m_ClauseLocation);
    
    if (loc_compare == sequence::eContained || loc_compare == sequence::eSame) {
        if (parent_clause->SameStrand(*m_ClauseLocation)) {
            return true;
        } else if (parent_clause->IsTransposon()
                   || parent_clause->IsInsertionSequence()
                   || parent_clause->IsEndogenousVirusSourceFeature()
                   || (parent_clause->IsGeneCluster() && gene_cluster_opp_strand)) {
            return true;
        }
    } else if (m_MainFeat.GetData().GetSubtype() == CSeqFeatData::eSubtype_promoter
               && parent_clause->SameStrand(*m_ClauseLocation)) {
        unsigned int promoter_stop = sequence::GetStop(*m_ClauseLocation, &(m_BH.GetScope()), eExtreme_Biological);
        unsigned int parent_start = sequence::GetStart(*(parent_clause->GetLocation()), &(m_BH.GetScope()), eExtreme_Biological);
        if (m_ClauseLocation->GetStrand() == eNa_strand_minus) {
            if (promoter_stop == parent_start + 1) {
                return true;
            }
        } else if (promoter_stop + 1 == parent_start) {
            return true;
        }
    }
    return false;
}


CAutoDefFeatureClause_Base *CAutoDefFeatureClause::FindBestParentClause(CAutoDefFeatureClause_Base * subclause, bool gene_cluster_opp_strand)
{
    CAutoDefFeatureClause_Base *best_parent;
    
    if (subclause == NULL || subclause == this) {
        return NULL;
    }
    
    best_parent = CAutoDefFeatureClause_Base::FindBestParentClause(subclause, gene_cluster_opp_strand);
    
    if (subclause->OkToGroupUnderByLocation(this, gene_cluster_opp_strand)
        && subclause->OkToGroupUnderByType(this)) {
        if (best_parent == NULL || best_parent->CompareLocation(*m_ClauseLocation) == sequence::eContained) {
            best_parent = this;
        }
    }
    return best_parent;
}


static string transposon_keywords [] = {
  "retrotransposon",
  "P-element",
  "transposable element",
  "integron",
  "superintegron",
  "MITE"
};
  

CAutoDefTransposonClause::CAutoDefTransposonClause(CBioseq_Handle bh, const CSeq_feat& main_feat)
                  : CAutoDefFeatureClause(bh, main_feat)
{
    string transposon_name = m_MainFeat.GetNamedQual("transposon");
    bool   found_keyword = false;

    m_Pluralizable = true;

    if (NStr::IsBlank(transposon_name)) {
        m_Description = "unnamed";
        m_ShowTypewordFirst = false;
        m_Typeword = "transposon";
    } else {
        for (unsigned int k = 0; k < sizeof (transposon_keywords) / sizeof (string) && !found_keyword; k++) {
            if (NStr::StartsWith(transposon_name, transposon_keywords[k])) {
                m_Typeword = transposon_keywords[k];
                if (NStr::Equal(transposon_name, transposon_keywords[k])) {
                    m_ShowTypewordFirst = false;
                    m_Description = "unnamed";
                } else {
                    m_ShowTypewordFirst = true;
                    m_Description = transposon_name.substr(transposon_keywords[k].length());
                    NStr::TruncateSpacesInPlace(m_Description);
                }
                found_keyword = true;
            } else if (NStr::EndsWith(transposon_name, transposon_keywords[k])) {
                m_Typeword = transposon_keywords[k];
                m_ShowTypewordFirst = false;
                m_Description = transposon_name.substr(0, transposon_name.length() - transposon_keywords[k].length());
                NStr::TruncateSpacesInPlace(m_Description);
                found_keyword = true;
            }
        }
        if (!found_keyword) {
            m_ShowTypewordFirst = true;
            m_Typeword = "transposon";
            m_Description = transposon_name;
        }
    }
    m_DescriptionChosen = true;
    m_TypewordChosen = true;
    m_ProductName = "";
    m_ProductNameChosen = true;    
}


CAutoDefTransposonClause::~CAutoDefTransposonClause()
{
}


void CAutoDefTransposonClause::Label()
{
    x_GetGenericInterval (m_Interval);
}


CAutoDefSatelliteClause::CAutoDefSatelliteClause(CBioseq_Handle bh, const CSeq_feat& main_feat)
                  : CAutoDefFeatureClause(bh, main_feat)
{
    string comment = m_MainFeat.GetComment();
    unsigned int pos = NStr::Find(comment, ";");
    if (pos != NCBI_NS_STD::string::npos) {
        comment = comment.substr(0, pos);
    }
    m_Description = comment;
    m_DescriptionChosen = true;
    m_Typeword = "sequence";
    m_TypewordChosen = true;
}


CAutoDefSatelliteClause::~CAutoDefSatelliteClause()
{
}


void CAutoDefSatelliteClause::Label()
{
    x_GetGenericInterval(m_Interval);
}


CAutoDefPromoterClause::CAutoDefPromoterClause(CBioseq_Handle bh, const CSeq_feat& main_feat)
                  : CAutoDefFeatureClause(bh, main_feat)
{
    m_Description = "";
    m_DescriptionChosen = true;
    m_Typeword = "promoter region";
    m_TypewordChosen = true;
    m_Interval = "";
}


CAutoDefPromoterClause::~CAutoDefPromoterClause()
{
}


void CAutoDefPromoterClause::Label()
{
}


/* This class produces the default definition line label for a misc_feature 
 * that has the word "intergenic spacer" in the comment.  If the comment starts
 * with the word "contains", "contains" is ignored.  If "intergenic spacer"
 * appears first in the comment (or first after the word "contains", the text
 * after the words "intergenic spacer" but before the first semicolon (if any)
 * appear after the words "intergenic spacer" in the definition line.  If there
 * are words after "contains" or at the beginning of the comment before the words
 * "intergenic spacer", this text will appear in the definition line before the words
 * "intergenic spacer".
 */
CAutoDefIntergenicSpacerClause::CAutoDefIntergenicSpacerClause(CBioseq_Handle bh, const CSeq_feat& main_feat)
                  : CAutoDefFeatureClause(bh, main_feat)
{
    m_Typeword = "intergenic spacer";
    m_TypewordChosen = true;
    m_ShowTypewordFirst = false;
    m_Pluralizable = false;
    
    string comment = m_MainFeat.GetComment();
    
    if (NStr::StartsWith(comment, "contains ")) {
        comment = comment.substr(9);
    }
    
    if (NStr::StartsWith(comment, "intergenic spacer")) {
        comment = comment.substr(17);
        if (NStr::IsBlank(comment)) {
            m_ShowTypewordFirst = false;
            m_Description = "";
            m_DescriptionChosen = true;
        } else {
            NStr::TruncateSpacesInPlace(comment);
            if (NStr::StartsWith(comment, "and ")) {
                m_Description = "";
                m_DescriptionChosen = true;
                m_ShowTypewordFirst = false;
            } else {
                unsigned int pos = NStr::Find(comment, ";");
                if (pos != NCBI_NS_STD::string::npos) {
                    comment = comment.substr(0, pos);
                }
                m_Description = comment;
                m_DescriptionChosen = true;
                m_ShowTypewordFirst = true;
            }
        }
    } else {
        unsigned int pos = NStr::Find(comment, "intergenic spacer");
        if (pos != NCBI_NS_STD::string::npos) {
            m_Description = comment.substr(0, pos);
            NStr::TruncateSpacesInPlace(m_Description);
            m_DescriptionChosen = true;
            m_ShowTypewordFirst = false;
        }
    }
    x_GetGenericInterval(m_Interval);        
}


CAutoDefIntergenicSpacerClause::~CAutoDefIntergenicSpacerClause()
{
}


void CAutoDefIntergenicSpacerClause::Label()
{
}


CAutoDefParsedIntergenicSpacerClause::CAutoDefParsedIntergenicSpacerClause(CBioseq_Handle bh, const CSeq_feat &main_feat, 
                                                                           string description, bool is_first, bool is_last)
                                                                           : CAutoDefIntergenicSpacerClause(bh, main_feat)
{
    if (!NStr::IsBlank(description)) {
        m_Description = description;
    }
    // adjust partialness of location
    bool partial5 = m_ClauseLocation->IsPartialStart(eExtreme_Biological) && is_first;
    bool partial3 = m_ClauseLocation->IsPartialStop(eExtreme_Biological) && is_last;
    m_ClauseLocation->SetPartialStart(partial5, eExtreme_Biological);
    m_ClauseLocation->SetPartialStop(partial3, eExtreme_Biological);
}


CAutoDefParsedIntergenicSpacerClause::~CAutoDefParsedIntergenicSpacerClause()
{
}


CAutoDefParsedtRNAClause::~CAutoDefParsedtRNAClause()
{
}


CAutoDefParsedClause::CAutoDefParsedClause(CBioseq_Handle bh, const CSeq_feat &main_feat, bool is_first, bool is_last)
                                       : CAutoDefFeatureClause (bh, main_feat)
{
    // adjust partialness of location
    bool partial5 = m_ClauseLocation->IsPartialStart(eExtreme_Biological) && is_first;
    bool partial3 = m_ClauseLocation->IsPartialStop(eExtreme_Biological) && is_last;
    m_ClauseLocation->SetPartialStart(partial5, eExtreme_Biological);
    m_ClauseLocation->SetPartialStop(partial3, eExtreme_Biological);
}

CAutoDefParsedClause::~CAutoDefParsedClause()
{
}


CAutoDefParsedtRNAClause::CAutoDefParsedtRNAClause(CBioseq_Handle bh, const CSeq_feat &main_feat, 
                                                   string gene_name, string product_name, 
                                                   bool is_first, bool is_last)
                                                   : CAutoDefParsedClause (bh, main_feat, is_first, is_last)
{
    m_Typeword = "gene";
    m_TypewordChosen = true;
    m_GeneName = gene_name;
    m_ProductName = product_name;
    m_ProductNameChosen = true;
}


CAutoDefGeneClusterClause::CAutoDefGeneClusterClause(CBioseq_Handle bh, const CSeq_feat& main_feat)
                  : CAutoDefFeatureClause(bh, main_feat)
{
    m_Pluralizable = false;
    m_ShowTypewordFirst = false;
    string comment = m_MainFeat.GetComment();
    
    unsigned int pos = NStr::Find(comment, "gene cluster");
    if (pos == NCBI_NS_STD::string::npos) {
        pos = NStr::Find(comment, "gene locus");
        m_Typeword = "gene locus";
        m_TypewordChosen = true;
    } else {
        m_Typeword = "gene cluster";
        m_TypewordChosen = true;
    }
    
    if (pos != NCBI_NS_STD::string::npos) {
        comment = comment.substr(0, pos);
    }
    NStr::TruncateSpacesInPlace(comment);
    m_Description = comment;
    m_DescriptionChosen = true;
    m_SuppressSubfeatures = true;
}


CAutoDefGeneClusterClause::~CAutoDefGeneClusterClause()
{
}


void CAutoDefGeneClusterClause::Label()
{
    x_GetGenericInterval(m_Interval);        
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ===========================================================================
* $Log$
* Revision 1.5  2006/04/18 20:13:58  bollin
* added option to suppress transposon and insertion sequence subfeaures
* corrected bug in CAutoDefFeatureClause::SameStrand
*
* Revision 1.4  2006/04/18 16:56:16  bollin
* added support for parsing misc_RNA features
*
* Revision 1.3  2006/04/18 01:04:59  ucko
* Don't bother clear()ing freshly allocated strings, particularly given
* that it would have been necessary to call erase() instead for
* compatibility with GCC 2.95.
*
* Revision 1.2  2006/04/17 17:39:37  ucko
* Fix capitalization of header filenames.
*
* Revision 1.1  2006/04/17 16:25:05  bollin
* files for automatically generating definition lines, using a combination
* of modifiers to make definition lines unique within a set and listing the
* relevant features on the sequence.
*
*
* ===========================================================================
*/

