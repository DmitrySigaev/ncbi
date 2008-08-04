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
#include <objtools/edit/autodef.hpp>
#include <corelib/ncbimisc.hpp>
#include <objmgr/seqdesc_ci.hpp>
#include <objmgr/bioseq_ci.hpp>
#include <objmgr/feat_ci.hpp>
#include <objmgr/util/feature.hpp>
#include <objmgr/util/sequence.hpp>

#include <objects/seq/Seq_descr.hpp>
#include <objects/seq/Seqdesc.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/seqfeat/RNA_ref.hpp>

#include <serial/iterator.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

            
CAutoDef::CAutoDef()
    : m_FeatureListType(eListAllFeatures),
      m_MiscFeatRule(eDelete),
      m_SpecifyNuclearProduct(false),
      m_ProductFlag(CBioSource::eGenome_unknown),
      m_AltSpliceFlag(false),
      m_SuppressAltSplicePhrase(false),
      m_SuppressLocusTags(false),
      m_GeneOppStrand(false),
      m_RemoveTransposonAndInsertionSequenceSubfeatures(false),
      m_KeepExons(false),
      m_KeepIntrons(false),
      m_KeepPromoters(false),
      m_KeepLTRs(false),
      m_Keep3UTRs(false),
      m_Keep5UTRs(false),
      m_Cancelled(false)
{
    m_SuppressedFeatures.clear();
}


CAutoDef::~CAutoDef()
{
}

void CAutoDef::AddSources (CSeq_entry_Handle se)
{
    
    // add sources to modifier combination groups
    CBioseq_CI seq_iter(se, CSeq_inst::eMol_na);
    for ( ; seq_iter; ++seq_iter ) {
        for (CSeqdesc_CI dit((*seq_iter), CSeqdesc::e_Source); dit;  ++dit) {
            const CBioSource& bsrc = dit->GetSource();
            m_OrigModCombo.AddSource(bsrc);
        }
    }

    // set default exclude_sp values
    m_OrigModCombo.SetExcludeSpOrgs (m_OrigModCombo.GetDefaultExcludeSp());
}


void CAutoDef::AddSources (CBioseq_Handle bh) 
{
    for (CSeqdesc_CI dit(bh, CSeqdesc::e_Source); dit;  ++dit) {
        const CBioSource& bsrc = dit->GetSource();
        m_OrigModCombo.AddSource (bsrc);
    }

    // set default exclude_sp values
    m_OrigModCombo.SetExcludeSpOrgs(m_OrigModCombo.GetDefaultExcludeSp());
}


void CAutoDef::x_SortModifierListByRank(TModifierIndexVector &index_list, CAutoDefSourceDescription::TAvailableModifierVector &modifier_list) 
{
    unsigned int k, j, tmp;
    if (index_list.size() < 2) {
        return;
    }
    for (k = 0; k < index_list.size() - 1; k++) {
        for (j = k + 1; j < index_list.size(); j++) { 
            if (modifier_list[index_list[k]].GetRank() > modifier_list[index_list[j]].GetRank()) {
                 tmp = index_list[k];
                 index_list[k] = index_list[j];
                 index_list[j] = tmp;
             }
         }
     }
}


void CAutoDef::x_GetModifierIndexList(TModifierIndexVector &index_list, CAutoDefSourceDescription::TAvailableModifierVector &modifier_list)
{
    unsigned int k;
    TModifierIndexVector remaining_list;

    index_list.clear();
    remaining_list.clear();
    
    // note - required modifiers should be removed from the list
    
    // first, look for all_present and all_unique modifiers
    for (k = 0; k < modifier_list.size(); k++) {
        if (modifier_list[k].AllPresent() && modifier_list[k].AllUnique()) {
            index_list.push_back(k);
        } else if (modifier_list[k].AnyPresent()) {
            remaining_list.push_back(k);
        }
    }
    x_SortModifierListByRank(index_list, modifier_list);
    x_SortModifierListByRank(remaining_list, modifier_list);
    
    for (k = 0; k < remaining_list.size(); k++) {
        index_list.push_back(remaining_list[k]);
    }
}


bool CAutoDef::x_IsOrgModRequired(unsigned int mod_type)
{
    return false;
}


bool CAutoDef::x_IsSubSrcRequired(unsigned int mod_type)
{
    if (mod_type == CSubSource::eSubtype_endogenous_virus_name
        || mod_type == CSubSource::eSubtype_plasmid_name
        || mod_type == CSubSource::eSubtype_transgenic) {
        return true;
    } else {
        return false;
    }
}


unsigned int CAutoDef::GetNumAvailableModifiers()
{
    CAutoDefSourceDescription::TAvailableModifierVector modifier_list;
    modifier_list.clear();
    m_OrigModCombo.GetAvailableModifiers (modifier_list);

    unsigned int num_present = 0;
    for (unsigned int k = 0; k < modifier_list.size(); k++) {
        if (modifier_list[k].AnyPresent()) {
            num_present++;
        }
    }
    return num_present;    
}


CAutoDefModifierCombo * CAutoDef::FindBestModifierCombo()
{
    CAutoDefModifierCombo *best = NULL;
    TModifierComboVector  combo_list;

    combo_list.clear();
    combo_list.push_back (new CAutoDefModifierCombo(&m_OrigModCombo));


    TModifierComboVector tmp, add_list;
    TModifierComboVector::iterator it;
    CAutoDefSourceDescription::TModifierVector mod_list, mods_to_try;
    bool stop = false;
    unsigned int  k;

    mod_list.clear();

    if (combo_list[0]->GetMaxInGroup() == 1) {
        stop = true;
    }

    while (!stop) {
        stop = true;
        it = combo_list.begin();
        add_list.clear();
        while (it != combo_list.end()) {
            tmp = (*it)->ExpandByAllPresent ();
            if (!tmp.empty()) {
                stop = false;
                for (k = 0; k < tmp.size(); k++) {
                    add_list.push_back (new CAutoDefModifierCombo(tmp[k]));
                }
                it = combo_list.erase (it);
            } else {
                it++;
            }
            tmp.clear();
        }
        for (k = 0; k < add_list.size(); k++) {
            combo_list.push_back (new CAutoDefModifierCombo(add_list[k]));
        }
        add_list.clear();
        std::sort (combo_list.begin(), combo_list.end());
        if (combo_list[0]->GetMaxInGroup() == 1) {
            stop = true;
        }
    }

    ITERATE (CAutoDefSourceDescription::TModifierVector, it, combo_list[0]->GetModifiers()) {
        mod_list.push_back (CAutoDefSourceModifierInfo(*it));
    }

    best = combo_list[0];
    combo_list[0] = NULL;
    for (k = 1; k < combo_list.size(); k++) {
       delete combo_list[k];
       combo_list[k] = NULL;
    }
    return best;
}


CAutoDefModifierCombo* CAutoDef::GetAllModifierCombo()
{
    CAutoDefModifierCombo *newm = new CAutoDefModifierCombo(&m_OrigModCombo);
        
    // set all modifiers in combo
    CAutoDefSourceDescription::TAvailableModifierVector modifier_list;
    
    // first, get the list of modifiers that are available
    modifier_list.clear();
    newm->GetAvailableModifiers (modifier_list);
    
    // add any modifier not already in the combo to the combo
    for (unsigned int k = 0; k < modifier_list.size(); k++) {
        if (modifier_list[k].AnyPresent()) {
            if (modifier_list[k].IsOrgMod()) {
                COrgMod::ESubtype subtype = modifier_list[k].GetOrgModType();
                if (!newm->HasOrgMod(subtype)) {
                    newm->AddOrgMod(subtype);
                } 
            } else {
                CSubSource::ESubtype subtype = modifier_list[k].GetSubSourceType();
                if (!newm->HasSubSource(subtype)) {
                    newm->AddSubsource(subtype);
                }
            }
        }
    }
    return newm;
}


CAutoDefModifierCombo* CAutoDef::GetEmptyCombo()
{
    CAutoDefModifierCombo *newm = new CAutoDefModifierCombo(&m_OrigModCombo);
        
    return newm;
}


string CAutoDef::GetOneSourceDescription(CBioseq_Handle bh)
{
    CAutoDefModifierCombo *best = FindBestModifierCombo();
    if (best == NULL) {
        return "";
    }
    
    for (CSeqdesc_CI dit(bh, CSeqdesc::e_Source); dit;  ++dit) {
        const CBioSource& bsrc = dit->GetSource();
        return best->GetSourceDescriptionString(bsrc);
    }
    return "";
}



CAutoDefParsedtRNAClause *CAutoDef::x_tRNAClauseFromNote(CBioseq_Handle bh, const CSeq_feat& cf, const CSeq_loc& mapped_loc, string comment, bool is_first, bool is_last) 
{
    string product_name = "";
    string gene_name = "";
    
    /* tRNA name must start with "tRNA-" and be followed by one uppercase letter and
     * two lowercase letters.
     */
    if (!NStr::StartsWith(comment, "tRNA-")
        || comment.length() < 8
        || !isalpha(comment.c_str()[5]) || !isupper(comment.c_str()[5])
        || !isalpha(comment.c_str()[6]) || !islower(comment.c_str()[6])
        || !isalpha(comment.c_str()[7]) || !islower(comment.c_str()[7])) {
        return NULL;
    }
    product_name = comment.substr(0, 8);
    comment = comment.substr(8);
    NStr::TruncateSpacesInPlace(comment);

    /* gene name must be in parentheses, start with letters "trn",
     * and end with one uppercase letter.
     */    
    if (comment.length() < 6
        || !NStr::StartsWith(comment, "(trn" )
        || !isalpha(comment.c_str()[4])
        || !isupper(comment.c_str()[4])
        || comment.c_str()[5] != ')' ) {
        return NULL;
    }
    gene_name = comment.substr(1, 4);
    comment = comment.substr(6);
    NStr::TruncateSpacesInPlace(comment);
         
    return new CAutoDefParsedtRNAClause(bh, cf, mapped_loc, gene_name, product_name, is_first, is_last);

}        


const int kParsedTrnaGene = 1;
const int kParsedTrnaSpacer = 2;
bool CAutoDef::x_AddIntergenicSpacerFeatures(CBioseq_Handle bh, const CSeq_feat& cf, const CSeq_loc& mapped_loc, CAutoDefFeatureClause_Base &main_clause, bool suppress_locus_tags)
{
    CSeqFeatData::ESubtype subtype = cf.GetData().GetSubtype();
    if ((subtype != CSeqFeatData::eSubtype_misc_feature && subtype != CSeqFeatData::eSubtype_otherRNA)
        || !cf.CanGetComment()) {
        return false;
    }
    string comment = cf.GetComment();
    string::size_type pos = NStr::Find(comment, "intergenic spacer");
    if (pos == NCBI_NS_STD::string::npos) {
        return false;
    }
    
    // ignore "contains " at beginning of comment
    if (NStr::StartsWith(comment, "contains ")) {
        comment = comment.substr(9);
    }
    
    // get rid of any leading or trailing spaces
    NStr::TruncateSpacesInPlace(comment);

    // ignore anything after the first semicolon
    pos = NStr::Find(comment, ";");
    if (pos != NCBI_NS_STD::string::npos) {
        comment = comment.substr(0, pos);
    }

    pos = NStr::Find (comment, " and ");
    // insert comma for parsing
    if (pos != NCBI_NS_STD::string::npos && pos > 0 && !NStr::StartsWith (comment.substr(pos - 1), ",")) {
        comment = comment.substr(0, pos - 1) + "," + comment.substr(pos);
    }

    vector<string> parts;
    NStr::Tokenize(comment, ",", parts, NStr::eMergeDelims );
    if ( parts.empty() ) {
        return false;
    }
    
    bool bad_phrase = false;
    bool alternating = true;
    bool names_correct = true;
    int last_type = 0;

    vector<CAutoDefFeatureClause *> clause_list;

    CAutoDefParsedtRNAClause *gene = NULL;

    for (int j = 0; j < parts.size() && alternating && names_correct && !bad_phrase; j++) {
        NStr::TruncateSpacesInPlace(parts[j]);
        if (NStr::StartsWith (parts[j], "and ")) {
            parts[j] = parts[j].substr(4);
        }
        if (NStr::EndsWith (parts[j], " intergenic spacer")) {
          // must be a spacer
            string spacer_description = parts[j].substr(0, parts[j].length() - 18);
            CAutoDefFeatureClause *spacer = new CAutoDefParsedIntergenicSpacerClause(bh, cf, mapped_loc, spacer_description, j == 0, j == parts.size() - 1);
            if (spacer == NULL) {
                bad_phrase = true;
            } else {
                if (last_type == kParsedTrnaSpacer) {
                    alternating = false;
                } else if (last_type == kParsedTrnaGene) {
                    // spacer names and gene names must agree
                    string gene_name = gene->GetGeneName();
                    string description = spacer->GetDescription();
                    if (!NStr::StartsWith (description, gene_name + "-")) {
                        names_correct = false;
                    }
                }
                spacer->SetSuppressLocusTag(suppress_locus_tags);
                clause_list.push_back (spacer);
                last_type = kParsedTrnaSpacer;
            }
        } else {
            gene = x_tRNAClauseFromNote(bh, cf, mapped_loc, parts[j], j == 0, j == parts.size() - 1);
            if (gene == NULL) {
                bad_phrase = true;
            } else {
                // must alternate between genes and spacers
                if (last_type == kParsedTrnaGene) {
                    alternating = false;
                } else if (last_type == kParsedTrnaSpacer) {
                    // spacer names and gene names must agree
                    string gene_name = gene->GetGeneName();
                    if (!NStr::EndsWith (parts[j - 1], "-" + gene_name + " intergenic spacer")) {
                        names_correct = false;
                    }
                }
                gene->SetSuppressLocusTag(suppress_locus_tags);
                clause_list.push_back (gene);
                last_type = kParsedTrnaGene;
            }
        }
    }

    if (!bad_phrase && alternating && names_correct) {
        for (int i = 0; i < clause_list.size(); i++) {
            main_clause.AddSubclause (clause_list[i]);
        }
        return true;
    } else {
        for (int i = 0; i < clause_list.size(); i++) {
            delete clause_list[i];
        }
        return false;
    }
}
    

// Some misc_RNA clauses have a comment that actually lists multiple
// features.  These functions create a clause for each element in the
// comment.
static string misc_words [] = {
  "internal transcribed spacer",
  "external transcribed spacer",
  "ribosomal RNA intergenic spacer",
  "ribosomal RNA",
  "intergenic spacer"
};

typedef enum {
  MISC_RNA_WORD_INTERNAL_SPACER = 0,
  MISC_RNA_WORD_EXTERNAL_SPACER,
  MISC_RNA_WORD_RNA_INTERGENIC_SPACER,
  MISC_RNA_WORD_RNA,
  MISC_RNA_WORD_INTERGENIC_SPACER,
  NUM_MISC_RNA_WORDS
} MiscWord;

static string separators [] = {
  ", and ",
  " and ",
  ", ",
  "; "
};

#define num_separators 3


bool CAutoDef::x_AddMiscRNAFeatures(CBioseq_Handle bh, const CSeq_feat& cf, const CSeq_loc& mapped_loc, CAutoDefFeatureClause_Base &main_clause, bool suppress_locus_tags)
{
    string product = "";
    string::size_type pos;
    bool is_first = true;
    
    if (cf.GetData().Which() == CSeqFeatData::e_Rna) {
        product = cf.GetNamedQual("product");
        if (NStr::IsBlank(product)
            && cf.GetData().GetRna().GetExt().Which() == CRNA_ref::C_Ext::e_Name) {
            product = cf.GetData().GetRna().GetExt().GetName();
        }
    }

    if ((NStr::Equal (product, "misc_RNA") || NStr::IsBlank (product)) && cf.CanGetComment()) {
        product = cf.GetComment();
    }
    if (NStr::IsBlank(product)) {
        return false;
    }
    
    if (NStr::StartsWith(product, "contains ")) {
        product = product.substr(9);
    }
    
    pos = NStr::Find(product, "spacer");
    if (pos == NCBI_NS_STD::string::npos) {
        return false;
    }
    
    while (!NStr::IsBlank(product)) {
        string this_label = product;
        string::size_type first_separator = NCBI_NS_STD::string::npos;
        string::size_type separator_len = 0;
        for (unsigned int i = 0; i < 4; i++) {
            pos = NStr::Find(product, separators[i]);
            if (pos != NCBI_NS_STD::string::npos 
                && (first_separator == NCBI_NS_STD::string::npos
                    || pos < first_separator)) {
                first_separator = pos;
                separator_len = separators[i].length();
            }
        }
        
        if (first_separator != NCBI_NS_STD::string::npos) {
            this_label = product.substr(0, first_separator);
            product = product.substr(first_separator + separator_len);
        } else {
            product = "";
        }
    
        // find first of the recognized words to occur in the string
        string::size_type first_word = NCBI_NS_STD::string::npos;
        unsigned int word_id = 0;
    
        for (unsigned int i = 0; i < NUM_MISC_RNA_WORDS && first_word == NCBI_NS_STD::string::npos; i++) {
            first_word = NStr::Find (this_label, misc_words[i]);
            if (first_word != NCBI_NS_STD::string::npos) {
                word_id = i;
            }
        }
        if (first_word == NCBI_NS_STD::string::npos) {
            continue;
        }
        
        // check to see if any other clauses are present
        bool is_last = true;
        for (unsigned int i = 0; i < NUM_MISC_RNA_WORDS && is_last; i++) {
            if (NStr::Find (product, misc_words[i]) != NCBI_NS_STD::string::npos) {
                is_last = false;
            }
        }

        // create a clause of the appropriate type
        CAutoDefParsedClause *new_clause = new CAutoDefParsedClause(bh, cf, mapped_loc, is_first, is_last);        
        string description = "";
        if (word_id == MISC_RNA_WORD_INTERNAL_SPACER
            || word_id == MISC_RNA_WORD_EXTERNAL_SPACER
            || word_id == MISC_RNA_WORD_RNA_INTERGENIC_SPACER
            || word_id == MISC_RNA_WORD_INTERGENIC_SPACER) {
            if (first_word == 0) {
                new_clause->SetTypewordFirst(true);
                description = this_label.substr(misc_words[word_id].length());
            } else {
                new_clause->SetTypewordFirst(false);
                description = this_label.substr(0, first_word);
            }
            new_clause->SetTypeword(misc_words[word_id]);
        } else if (word_id == MISC_RNA_WORD_RNA) {
            description = this_label;
            new_clause->SetTypeword("gene");
            new_clause->SetTypewordFirst(false);
        }
        NStr::TruncateSpacesInPlace(description);
        new_clause->SetDescription(description);
        
        new_clause->SetSuppressLocusTag(suppress_locus_tags);
        
        main_clause.AddSubclause(new_clause);
        is_first = false;   
    }       
    return !is_first;
}

void CAutoDef::x_RemoveOptionalFeatures(CAutoDefFeatureClause_Base *main_clause, CBioseq_Handle bh)
{
    // remove optional features that have not been requested
    if (main_clause == NULL) {
        return;
    }
    
    // keep 5' UTRs only if lonely or requested
    if (!m_Keep5UTRs && !main_clause->IsFeatureTypeLonely(CSeqFeatData::eSubtype_5UTR)) {
        main_clause->RemoveFeaturesByType(CSeqFeatData::eSubtype_5UTR);
    }
    
    // keep 3' UTRs only if lonely or requested
    if (!m_Keep3UTRs && !main_clause->IsFeatureTypeLonely(CSeqFeatData::eSubtype_3UTR)) {
        main_clause->RemoveFeaturesByType(CSeqFeatData::eSubtype_3UTR);
    }
    
    // keep LTRs only if requested or lonely and not in parent
    if (!m_KeepLTRs) {
        if (main_clause->GetNumSubclauses() > 1 
            || main_clause->GetMainFeatureSubtype() != CSeqFeatData::eSubtype_LTR) {
            main_clause->RemoveFeaturesByType(CSeqFeatData::eSubtype_LTR);
        }
    }
           
    // keep promoters only if requested or lonely and not in mRNA
    if (!m_KeepPromoters) {
        if (!main_clause->IsFeatureTypeLonely(CSeqFeatData::eSubtype_promoter)) {
            main_clause->RemoveFeaturesByType(CSeqFeatData::eSubtype_promoter);
        } else {
            main_clause->RemoveFeaturesInmRNAsByType(CSeqFeatData::eSubtype_promoter);
        }
    }
    
    // keep introns only if requested or lonely and not in mRNA
    if (!m_KeepIntrons) {
        if (!main_clause->IsFeatureTypeLonely(CSeqFeatData::eSubtype_intron)) {
            main_clause->RemoveFeaturesByType(CSeqFeatData::eSubtype_intron);
        } else {
            main_clause->RemoveFeaturesInmRNAsByType(CSeqFeatData::eSubtype_intron);
        }
    }
    
    // keep exons only if requested or lonely or in mRNA or in partial CDS or on segment
    if (!m_KeepExons && !IsSegment(bh)) {
        if (main_clause->GetMainFeatureSubtype() != CSeqFeatData::eSubtype_exon) {
            main_clause->RemoveUnwantedExons();
        }
    }
    
    // only keep bioseq precursor RNAs if lonely
    if (!main_clause->IsBioseqPrecursorRNA()) {
        main_clause->RemoveBioseqPrecursorRNAs();
    }
    
    // delete subclauses at end, so that loneliness calculations will be correct
    main_clause->RemoveDeletedSubclauses();
}


bool CAutoDef::x_IsFeatureSuppressed(CSeqFeatData::ESubtype subtype)
{
    unsigned int feat_type = CSeqFeatData::GetTypeFromSubtype(subtype); 
    ITERATE(TFeatTypeItemSet, it, m_SuppressedFeatures)  {
        unsigned i_feat_type = it->GetType();
        if (i_feat_type == CSeqFeatData::e_not_set) {
            return true;
        } else if (feat_type != i_feat_type) {
            continue;
        }
        unsigned int i_subtype = it->GetSubtype();
        if (i_subtype == CSeqFeatData::eSubtype_any
            || i_subtype == subtype ) {
            return true;
        }
    }
    return false;
}


void CAutoDef::SuppressFeature(objects::CFeatListItem feat)
{
    m_SuppressedFeatures.insert(feat);
}


bool CAutoDef::IsSegment(CBioseq_Handle bh)
{
    CSeq_entry_Handle seh = bh.GetParentEntry();
    
    seh = seh.GetParentEntry();
    
    if (seh && seh.IsSet()) {
        CBioseq_set_Handle bsh = seh.GetSet();
        if (bsh.CanGetClass() && bsh.GetClass() == CBioseq_set::eClass_parts) {
            return true;
        }
    }
    return false;
}


void CAutoDef::GetMasterLocation(CBioseq_Handle &bh, CRange<TSeqPos>& range)
{
    CSeq_entry_Handle seh = bh.GetParentEntry();
    CBioseq_Handle    master = bh;
    unsigned int      start = 0, stop = bh.GetBioseqLength() - 1;
    unsigned int      offset = 0;
    
    seh = seh.GetParentEntry();
    
    if (seh && seh.IsSet()) {
        CBioseq_set_Handle bsh = seh.GetSet();
        if (bsh.CanGetClass() && bsh.GetClass() == CBioseq_set::eClass_parts) {
            seh = seh.GetParentEntry();
            if (seh.IsSet()) {
                bsh = seh.GetSet();
                if (bsh.CanGetClass() && bsh.GetClass() == CBioseq_set::eClass_segset) {
                    CBioseq_CI seq_iter(seh);
                    for ( ; seq_iter; ++seq_iter ) {
                        if (seq_iter->CanGetInst_Repr()) {
                            if (seq_iter->GetInst_Repr() == CSeq_inst::eRepr_seg) {
                                master = *seq_iter;
                            } else if (seq_iter->GetInst_Repr() == CSeq_inst::eRepr_raw) {
                                if (*seq_iter == bh) {
                                    start = offset;
                                    stop = offset + bh.GetBioseqLength() - 1;
                                } else {
                                    offset += seq_iter->GetBioseqLength();
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    bh = master;
    range.SetFrom(start);
    range.SetTo(stop);
}


string CAutoDef::x_GetFeatureClauses(CBioseq_Handle bh)
{
    CAutoDefFeatureClause_Base main_clause(m_SuppressLocusTags);
    CAutoDefFeatureClause *new_clause;
    CRange<TSeqPos> range;
    CBioseq_Handle master_bh = bh;
    
    GetMasterLocation(master_bh, range);

    CFeat_CI feat_ci(master_bh);
    while (feat_ci)
    { 
        const CSeq_feat& cf = feat_ci->GetOriginalFeature();
        const CSeq_loc& mapped_loc = feat_ci->GetMappedFeature().GetLocation();
        unsigned int subtype = cf.GetData().GetSubtype();
        unsigned int stop = mapped_loc.GetStop(eExtreme_Positional);
        // unless it's a gene, don't use it unless it ends in the sequence we're looking at
        if ((subtype == CSeqFeatData::eSubtype_gene 
             || subtype == CSeqFeatData::eSubtype_mRNA
             || subtype == CSeqFeatData::eSubtype_cdregion
             || (stop >= range.GetFrom() && stop <= range.GetTo()))
            && !x_IsFeatureSuppressed(cf.GetData().GetSubtype())) {
            new_clause = new CAutoDefFeatureClause(bh, cf, mapped_loc);
            subtype = new_clause->GetMainFeatureSubtype();
            if (new_clause->IsTransposon()) {
                delete new_clause;
                new_clause = new CAutoDefTransposonClause(bh, cf, mapped_loc);
            } else if (new_clause->IsSatelliteClause()) {
                delete new_clause;
                new_clause = new CAutoDefSatelliteClause(bh, cf, mapped_loc);
            } else if (subtype == CSeqFeatData::eSubtype_promoter) {
                delete new_clause;
                new_clause = new CAutoDefPromoterClause(bh, cf, mapped_loc);
            } else if (new_clause->IsIntergenicSpacer()) {
                delete new_clause;
                x_AddIntergenicSpacerFeatures(bh, cf, mapped_loc, main_clause, m_SuppressLocusTags);
                new_clause = NULL;
            } else if (subtype == CSeqFeatData::eSubtype_otherRNA
                       || subtype == CSeqFeatData::eSubtype_misc_RNA
                       || subtype == CSeqFeatData::eSubtype_rRNA) {
                if (x_AddMiscRNAFeatures(bh, cf, mapped_loc, main_clause, m_SuppressLocusTags)) {
                    delete new_clause;
                    new_clause = NULL;
                }
            } else if (new_clause->IsGeneCluster()) {
                delete new_clause;
                new_clause = new CAutoDefGeneClusterClause(bh, cf, mapped_loc);
            } else if (new_clause->GetMainFeatureSubtype() == CSeqFeatData::eSubtype_misc_feature
                       && !NStr::Equal(new_clause->GetTypeword(), "control region")) {
                if (m_MiscFeatRule == eDelete
                    || (m_MiscFeatRule == eNoncodingProductFeat && !new_clause->IsNoncodingProductFeat())) {
                    delete new_clause;
                    new_clause = NULL;
                } else if (m_MiscFeatRule == eCommentFeat) {
                    delete new_clause;
                    new_clause = NULL;
                    if (cf.CanGetComment() && ! NStr::IsBlank(cf.GetComment())) {
                        new_clause = new CAutoDefMiscCommentClause(bh, cf, mapped_loc);
                    }
                }            
            }
        
            if (new_clause != NULL && new_clause->IsRecognizedFeature()) {
                new_clause->SetSuppressLocusTag(m_SuppressLocusTags);
                if (new_clause->GetMainFeatureSubtype() == CSeqFeatData::eSubtype_exon) {
                    new_clause->Label();
                }
                main_clause.AddSubclause(new_clause);
            } else if (new_clause != NULL) {            
                delete new_clause;
            }
        }
        ++feat_ci;
    }
    
    // Group alt-spliced exons first, so that they will be associated with the correct genes and mRNAs
    main_clause.GroupAltSplicedExons(bh);
    main_clause.RemoveDeletedSubclauses();
    
    // Add mRNAs to other clauses
    main_clause.GroupmRNAs();
    main_clause.RemoveDeletedSubclauses();
   
    // Add genes to clauses that need them for descriptions/products
    main_clause.GroupGenes();
        
    main_clause.GroupSegmentedCDSs();
    main_clause.RemoveDeletedSubclauses();
        
    // Group all features
    main_clause.GroupClauses(m_GeneOppStrand);
    main_clause.RemoveDeletedSubclauses();
    
    // now that features have been grouped, can expand lists of spliced exons
    main_clause.ExpandExonLists();
    
    // assign product names for features associated with genes that have products
    main_clause.AssignGeneProductNames(&main_clause);
    
    // reverse the order of clauses for minus-strand CDSfeatures
    main_clause.ReverseCDSClauseLists();
    
    main_clause.Label();
    main_clause.CountUnknownGenes();
    main_clause.RemoveDeletedSubclauses();
    
    // remove miscRNA and otherRNA features that are trans-spliced leaders
    main_clause.RemoveTransSplicedLeaders();
    main_clause.RemoveDeletedSubclauses();
    
    x_RemoveOptionalFeatures(&main_clause, bh);
    
    // if a gene is listed as part of another clause, they do not need
    // to be listed as there own clause
    main_clause.RemoveGenesMentionedElsewhere();
    main_clause.RemoveDeletedSubclauses();
    
    if (m_RemoveTransposonAndInsertionSequenceSubfeatures) {
        main_clause.SuppressTransposonAndInsertionSequenceSubfeatures();
    }

    // if this is a segment, remove genes, mRNAs, and CDSs
    // unless they end on this segment or have subclauses that need their protein/gene
    // information
    main_clause.RemoveNonSegmentClauses(range);
    
    main_clause.Label();

    if (!m_SuppressAltSplicePhrase) {
        main_clause.FindAltSplices();
        main_clause.RemoveDeletedSubclauses();
    } 
    
    main_clause.ConsolidateRepeatedClauses();
    main_clause.RemoveDeletedSubclauses();
    
    main_clause.GroupConsecutiveExons(bh);
    main_clause.RemoveDeletedSubclauses();
    
    main_clause.Label();

    return main_clause.ListClauses(true, false);
}


string OrganelleByGenome(unsigned int genome_val)
{
    string organelle = "";
    switch (genome_val) {
        case CBioSource::eGenome_macronuclear:
            organelle = "macronuclear";
            break;
        case CBioSource::eGenome_nucleomorph:
            organelle = "nucleomorph";
            break;
        case CBioSource::eGenome_mitochondrion:
            organelle = "mitochondrion";
            break;
        case CBioSource::eGenome_apicoplast:
            organelle = "apicoplast";
            break;
        case CBioSource::eGenome_chloroplast:
            organelle = "chloroplast";
            break;
        case CBioSource::eGenome_chromoplast:
            organelle = "chromoplast";
            break;
        case CBioSource::eGenome_kinetoplast:
            organelle = "kinetoplast";
            break;
        case CBioSource::eGenome_plastid:
            organelle = "plastid";
            break;
        case CBioSource::eGenome_cyanelle:
            organelle = "cyanelle";
            break;
        case CBioSource::eGenome_leucoplast:
            organelle = "leucoplast";
            break;
        case CBioSource::eGenome_proplastid:
            organelle = "proplastid";
            break;
        case CBioSource::eGenome_hydrogenosome:
            organelle = "hydrogenosome";
            break;
    }
    return organelle;
}

string CAutoDef::x_GetFeatureClauseProductEnding(const string& feature_clauses,
                                                 CBioseq_Handle bh)
{
    bool pluralize = false;
    
    if (NStr::Find(feature_clauses, "genes") != NCBI_NS_STD::string::npos) {
        pluralize = true;
    } else {
        string::size_type pos = NStr::Find(feature_clauses, "gene");
        if (pos != NCBI_NS_STD::string::npos
            && NStr::Find (feature_clauses, "gene", pos + 4) != NCBI_NS_STD::string::npos) {
            pluralize = true;
        }
    }

    unsigned int genome_val = CBioSource::eGenome_unknown;
    string genome_from_mods = "";
 
    for (CSeqdesc_CI dit(bh, CSeqdesc::e_Source); dit;  ++dit) {
        const CBioSource& bsrc = dit->GetSource();
        if (bsrc.CanGetGenome()) {
            genome_val = bsrc.GetGenome();
        }
        if (bsrc.CanGetSubtype()) {
            ITERATE (CBioSource::TSubtype, subSrcI, bsrc.GetSubtype()) {
                if ((*subSrcI)->GetSubtype() == CSubSource::eSubtype_other) {
                    string note = (*subSrcI)->GetName();
                    if (NStr::Equal(note, "macronuclear") || NStr::Equal(note, "micronuclear")) {
                        genome_from_mods = note;
                    }
                }
            }
        }
        break;
    }
    
    string ending = OrganelleByGenome(genome_val);
    if (NStr::Equal(ending, "mitochondrion")) {
        ending = "mitochondrial";
    }
    if (!NStr::IsBlank(ending)) {
        ending = "; " + ending;
    } else if (m_SpecifyNuclearProduct) {
        ending = OrganelleByGenome(m_ProductFlag);
        if (NStr::IsBlank(ending)) {
            if (!NStr::IsBlank(genome_from_mods)) {
                ending = "; " + genome_from_mods;
            }
        } else {
            if (NStr::Equal(ending, "mitochondrion")) {
                ending = "mitochondrial";
            }
            if (pluralize) {
                ending = "; nuclear genes for " + ending + " products";
            } else {
                ending = "; nuclear gene for " +  ending + " product";
            }
        }
    } 
    return ending;
}


string CAutoDef::GetOneDefLine(CAutoDefModifierCombo *mod_combo, CBioseq_Handle bh)
{
    // for protein sequences, use sequence::GetTitle
    if (bh.CanGetInst() && bh.GetInst().CanGetMol() && bh.GetInst().GetMol() == CSeq_inst::eMol_aa) {
        sequence::TGetTitleFlags flags = sequence::fGetTitle_Reconstruct | sequence::fGetTitle_Organism | sequence::fGetTitle_AllProteins;
        return sequence::GetTitle(bh, flags);
    }
    string org_desc = "Unknown organism";
    unsigned int genome_val = CBioSource::eGenome_unknown;
    
    for (CSeqdesc_CI dit(bh, CSeqdesc::e_Source); dit;  ++dit) {
        const CBioSource& bsrc = dit->GetSource();
        org_desc = mod_combo->GetSourceDescriptionString(bsrc);
        if (bsrc.CanGetGenome()) {
            genome_val = bsrc.GetGenome();
        }
        break;
    }
    string feature_clauses = "";
    if (m_FeatureListType == eListAllFeatures) {
        feature_clauses = " " + x_GetFeatureClauses(bh);
        string ending = x_GetFeatureClauseProductEnding(feature_clauses, bh);
        if (m_AltSpliceFlag) {
            if (NStr::IsBlank(ending)) {
                ending = "; alternatively spliced";
            } else {
                ending += ", alternatively spliced";
            }
        }
        feature_clauses += ending;
        feature_clauses += ".";
    } else if (m_FeatureListType == eCompleteSequence) {
        feature_clauses = ", complete sequence";
    } else if (m_FeatureListType == eCompleteGenome) {
        string organelle = OrganelleByGenome(genome_val);
        if (!NStr::IsBlank(organelle)) {
            feature_clauses = " " + organelle;
        }
        feature_clauses += ", complete genome";
    }
    
    return org_desc + feature_clauses;
}


void CAutoDef::GetAvailableModifiers(CAutoDef::TAvailableModifierSet &mod_set)
{    
    mod_set.clear();
    CAutoDefSourceDescription::TAvailableModifierVector modifier_list;
    modifier_list.clear();
    m_OrigModCombo.GetAvailableModifiers (modifier_list);
    for (unsigned int k = 0; k < modifier_list.size(); k++) {
        mod_set.insert(CAutoDefAvailableModifier(modifier_list[k]));
    }
}


END_SCOPE(objects)
END_NCBI_SCOPE
