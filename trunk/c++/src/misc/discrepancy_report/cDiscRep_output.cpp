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
 * Author:  Jie Chen
 *
 * File Description:
 *   Output for Cpp Discrepany Report
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>

#include <misc/discrepancy_report/hDiscRep_config.hpp>
#include <misc/discrepancy_report/hDiscRep_output.hpp>
#include <misc/discrepancy_report/hUtilib.hpp>

// xmlwrapp include
#include <misc/xmlwrapp/xmlwrapp.hpp>

#include <iostream>
#include <fstream>
#include <sstream>

BEGIN_NCBI_SCOPE

USING_NCBI_SCOPE;
USING_SCOPE(objects);
USING_SCOPE(DiscRepNmSpc);

static CDiscRepInfo thisInfo;

static const s_fataltag extra_fatal [] = {
        {"MISSING_GENOMEASSEMBLY_COMMENTS", NULL, NULL}
};

static const s_fataltag disc_fatal[] = {
        {"BAD_LOCUS_TAG_FORMAT", NULL, NULL},
        {"CONTAINED_CDS", NULL, "completely contained in another coding region but have note"},
        {"DISC_BACTERIAL_PARTIAL_NONEXTENDABLE_PROBLEMS", NULL, NULL},
        {"DISC_BACTERIA_SHOULD_NOT_HAVE_MRNA", NULL, NULL},
        {"DISC_BAD_BGPIPE_QUALS", NULL, NULL},
        {"DISC_CITSUBAFFIL_CONFLICT", NULL, "No citsubs were found!"},
        {"DISC_INCONSISTENT_MOLTYPES", NULL, "Moltypes are consistent"},
        {"DISC_MAP_CHROMOSOME_CONFLICT", NULL, NULL},
        {"DISC_MICROSATELLITE_REPEAT_TYPE", NULL, NULL},
        {"DISC_MISSING_AFFIL", NULL, NULL},
        {"DISC_NONWGS_SETS_PRESENT", NULL, NULL},
        {"DISC_QUALITY_SCORES", "Quality scores are missing on some sequences.", NULL},
        {"DISC_RBS_WITHOUT_GENE", NULL, NULL},
        {"DISC_SHORT_RRNA", NULL, NULL},
        {"DISC_SEGSETS_PRESENT", NULL, NULL},
        {"DISC_SOURCE_QUALS_ASNDISC", "collection-date", NULL},
        {"DISC_SOURCE_QUALS_ASNDISC", "country", NULL},
        {"DISC_SOURCE_QUALS_ASNDISC", "isolation-source", NULL},
        {"DISC_SOURCE_QUALS_ASNDISC", "host", NULL},
        {"DISC_SOURCE_QUALS_ASNDISC", "strain", NULL},
        {"DISC_SOURCE_QUALS_ASNDISC", "taxname", NULL},
        {"DISC_SUBMITBLOCK_CONFLICT", NULL, NULL},
        {"DISC_SUSPECT_RRNA_PRODUCTS", NULL, NULL},
        {"DISC_TITLE_AUTHOR_CONFLICT", NULL, NULL},
        {"DISC_UNPUB_PUB_WITHOUT_TITLE", NULL, NULL},
        {"DISC_USA_STATE", NULL, NULL},
        {"EC_NUMBER_ON_UNKNOWN_PROTEIN", NULL, NULL},
        {"EUKARYOTE_SHOULD_HAVE_MRNA", "no mRNA present", NULL},
        {"INCONSISTENT_LOCUS_TAG_PREFIX", NULL, NULL},
        {"INCONSISTENT_PROTEIN_ID", NULL, NULL},
        {"MISSING_GENES", NULL, NULL},
        {"MISSING_LOCUS_TAGS", NULL, NULL},
        {"MISSING_PROTEIN_ID", NULL, NULL},
        {"ONCALLER_ORDERED_LOCATION", NULL, NULL},
        {"PARTIAL_CDS_COMPLETE_SEQUENCE", NULL, NULL},
        {"PSEUDO_MISMATCH", NULL, NULL},
        {"RNA_CDS_OVERLAP", "coding regions are completely contained in RNAs", NULL},
        {"RNA_CDS_OVERLAP", "coding region is completely contained in an RNA", NULL},
        {"RNA_CDS_OVERLAP", "coding regions completely contain RNAs", NULL},
        {"RNA_CDS_OVERLAP", "coding region completely contains an RNA", NULL},
        {"RNA_NO_PRODUCT", NULL, NULL},
        {"SHOW_HYPOTHETICAL_CDS_HAVING_GENE_NAME", NULL, NULL},
        {"SUSPECT_PRODUCT_NAMES", "Remove organism from product name", NULL},
        {"SUSPECT_PRODUCT_NAMES", "Possible parsing error or incorrect formatting; remove inappropriate symbols", NULL},
        {"TEST_OVERLAPPING_RRNAS", NULL, NULL}
};

static const unsigned disc_cnt = ArraySize(disc_fatal);
static const unsigned extra_cnt = ArraySize(extra_fatal);

bool CDiscRepOutput :: x_NeedsTag(const string& setting_name, const string& desc, const s_fataltag* tags, const unsigned& cnt)
{
   unsigned i;
   for (i =0; i< cnt; i++) {
     if (setting_name == tags[i].setting_name
          && (tags[i].notag_description == NULL
                 || NStr::FindNoCase(desc, tags[i].notag_description) 
                         == string::npos)
          && (tags[i].description == NULL
                 || NStr::FindNoCase(desc, tags[i].description) 
                                             != string::npos) ) {
        return true;
     }
   }
   return false;
};


bool CDiscRepOutput :: x_IsItemTrnaInCDS(const CClickableItem& item)
{
    if (NStr::Equal(item.setting_name, "RNA_CDS_OVERLAP") &&
        (NStr::Find(item.description, "contain tRNA") != string::npos ||
         NStr::Find(item.description, "contains tRNA") != string::npos)) {
        return true;
    } else {
        return false;
    }
}


bool CDiscRepOutput ::IsFatal(const CClickableItem& item, bool extra, bool multiple_nt)
{
    string setting_name = item.setting_name;
    string desc = item.description; 
    bool needs_fatal = false;

    if (x_IsItemTrnaInCDS(item)) {
        needs_fatal = true;
    } else if (x_NeedsTag(setting_name, desc, disc_fatal, disc_cnt)
            || (extra 
                   && x_NeedsTag(setting_name, desc, extra_fatal, extra_cnt))) {        
        if (setting_name == "DISC_SOURCE_QUALS_ASNDISC") {
            if (NStr::FindNoCase(desc, "taxname (all present, all unique)") 
                  != string::npos) {
                if (multiple_nt) {
                    needs_fatal = true;
                }
            } else if (NStr::FindNoCase(desc, "some missing") != string::npos ||
                       NStr::FindNoCase(desc, "some duplicate") != string::npos) {
                needs_fatal = true;
            }
        } else {
            needs_fatal = true;
        }
    } else if (NStr::Equal(setting_name, "RNA_CDS_OVERLAP")) {
        ITERATE (vector <CRef <CClickableItem> >, sit, item.subcategories) {
            if (x_IsItemTrnaInCDS(**sit)) {
                needs_fatal = true;
                break;
            }
        }
    }
    return needs_fatal;
}


void CDiscRepOutput :: x_AddFatalToItem(CClickableItem& item, COutputConfig& oc, bool more_than_one_nucleotide)
{
    if (IsFatal(item, oc.add_extra_output_tag, more_than_one_nucleotide)) {
        item.fatal = true;
        item.description = "FATAL: " + item.description;
    }

    NON_CONST_ITERATE (vector <CRef <CClickableItem> >, sit, item.subcategories) {
        x_AddFatalToItem(**sit, oc, more_than_one_nucleotide);
    }

};

void CDiscRepOutput :: x_AddListOutputTags()
{
  string setting_name, desc, sub_desc;
  COutputConfig& oc = thisInfo.output_config;

  bool na_cnt_grt1 = false;
  vector <string> arr;
  ITERATE (vector <CRef <CClickableItem> >, it, thisInfo.disc_report_data) {
    if ((*it)->setting_name == "DISC_COUNT_NUCLEOTIDES") { // IsDiscCntGrt1
      arr.clear();
      arr = NStr::Tokenize((*it)->description, " ", arr);
      na_cnt_grt1 = (isInt(arr[0]) && NStr::StringToInt(arr[0]) > 1);
      break;
    }
  }
 
  // AddOutputTag
  NON_CONST_ITERATE (vector <CRef <CClickableItem> > , it, 
                                    thisInfo.disc_report_data) {
     x_AddFatalToItem(**it, oc, na_cnt_grt1);
  } 
};

typedef struct sDiscInfo {
   string conf_name;
   string setting_name;
} DiscInfoData;

static const DiscInfoData disc_info_list [] = {
  { "Missing Genes", "MISSING_GENES"},
  { "Extra Genes", "EXTRA_GENES"},
  { "Missing Locus Tags", "MISSING_LOCUS_TAGS"},
  { "Duplicate Locus Tags", "DUPLICATE_LOCUS_TAGS"},
  { "Bad Locus Tag Format", "BAD_LOCUS_TAG_FORMAT"},
  { "Inconsistent Locus Tag Prefix", "INCONSISTENT_LOCUS_TAG_PREFIX"},
  { "Nongene Locus Tag", "NON_GENE_LOCUS_TAG"},
  { "Count nucleotide sequences", "DISC_COUNT_NUCLEOTIDES"},
  { "Missing Protein ID", "MISSING_PROTEIN_ID"},
  { "Inconsistent Protein ID", "INCONSISTENT_PROTEIN_ID"},
  { "Feature Location Conflict", "FEATURE_LOCATION_CONFLICT"},
  { "Gene Product Conflict", "GENE_PRODUCT_CONFLICT"},
  { "Duplicate Gene Locus", "DUPLICATE_GENE_LOCUS"},
  { "EC Number Note", "EC_NUMBER_NOTE"},
  { "Pseudo Mismatch", "PSEUDO_MISMATCH"},
  { "Joined Features: on when non-eukaryote", "JOINED_FEATURES"},
  { "Overlapping Genes", "OVERLAPPING_GENES"},
  { "Overlapping CDS", "OVERLAPPING_CDS"},
  { "Contained CDS", "CONTAINED_CDS"},
  { "CDS RNA Overlap", "RNA_CDS_OVERLAP"},
  { "Short Contig", "SHORT_CONTIG"},
  { "Inconsistent BioSource", "INCONSISTENT_BIOSOURCE"},
  { "Suspect Product Name", "SUSPECT_PRODUCT_NAMES"},
  { "Suspect Product Name Typo", "DISC_PRODUCT_NAME_TYPO"},
  { "Suspect Product Name QuickFix", "DISC_PRODUCT_NAME_QUICKFIX"},
  { "Inconsistent Source And Definition Line", "INCONSISTENT_SOURCE_DEFLINE"},
  { "Partial CDSs in Complete Sequences", "PARTIAL_CDS_COMPLETE_SEQUENCE"},
  { "Hypothetical or Unknown Protein with EC Number", "EC_NUMBER_ON_UNKNOWN_PROTEIN"},
  { "Find Missing Tax Lookups", "TAX_LOOKUP_MISSING"} ,
  { "Find Tax Lookup Mismatches", "TAX_LOOKUP_MISMATCH"},
  { "Find Short Sequences", "SHORT_SEQUENCES"},
  { "Suspect Phrases", "SUSPECT_PHRASES"},
  { "Find Suspicious Phrases in Note Text", "DISC_SUSPICIOUS_NOTE_TEXT"},
  { "Count tRNAs", "COUNT_TRNAS"},
  { "Find Duplicate tRNAs", "FIND_DUP_TRNAS"},
  { "Find short and long tRNAs", "FIND_BADLEN_TRNAS"},
  { "Find tRNAs on the same strand", "FIND_STRAND_TRNAS"},
  { "Count rRNAs", "COUNT_RRNAS"},
  { "Find Duplicate rRNAs", "FIND_DUP_RRNAS"},
  { "Find RNAs without Products", "RNA_NO_PRODUCT"},
  { "Transl_except without Note", "TRANSL_NO_NOTE"},
  { "Note without Transl_except", "NOTE_NO_TRANSL"},
  { "Transl_except longer than 3", "TRANSL_TOO_LONG"},
  { "CDS tRNA overlaps", "CDS_TRNA_OVERLAP"},
  { "Count Proteins", "COUNT_PROTEINS"},
  { "Features Intersecting Source Features", "DISC_FEAT_OVERLAP_SRCFEAT"},
  { "CDS on GenProdSet without protein", "MISSING_GENPRODSET_PROTEIN"},
  { "Multiple CDS on GenProdSet, same protein", "DUP_GENPRODSET_PROTEIN"},
  { "mRNA on GenProdSet without transcript ID", "MISSING_GENPRODSET_TRANSCRIPT_ID"},
  { "mRNA on GenProdSet with duplicate ID", "DISC_DUP_GENPRODSET_TRANSCRIPT_ID"},
  { "Greater than 5 percent Ns", "DISC_PERCENT_N"},
  { "Runs of 10 or more Ns", "N_RUNS"},
  { "Zero Base Counts", "ZERO_BASECOUNT"},
  { "Adjacent PseudoGenes with Identical Text", "ADJACENT_PSEUDOGENES"},
  { "Bioseqs longer than 5000nt without Annotations", "DISC_LONG_NO_ANNOTATION"},
  { "Bioseqs without Annotations", "NO_ANNOTATION"},
  { "Influenza Strain/Collection Date Mismatch", "DISC_INFLUENZA_DATE_MISMATCH"},
  { "Introns shorter than 10 nt", "DISC_SHORT_INTRON"},
  { "Viruses should specify collection-date, country, and specific-host", "DISC_MISSING_VIRAL_QUALS"},
  { "Source Qualifier Report", "DISC_SRC_QUAL_PROBLEM"},
  { "All sources in a record should have the same qualifier set", "DISC_MISSING_SRC_QUAL"},
  { "Each source in a record should have unique values for qualifiers", "DISC_DUP_SRC_QUAL"},
  { "Each qualifier on a source should have different values", "DISC_DUP_SRC_QUAL_DATA"},
  { "Sequences with the same haplotype should match", "DISC_HAPLOTYPE_MISMATCH"},
  { "Sequences with rRNA or misc_RNA features should be genomic DNA", "DISC_FEATURE_MOLTYPE_MISMATCH"},
  { "Coding regions on eukaryotic genomic DNA should have mRNAs with matching products", "DISC_CDS_WITHOUT_MRNA"},
  { "Exon and intron locations should abut (unless gene is trans-spliced)", "DISC_EXON_INTRON_CONFLICT"},
  { "Count features present or missing from sequences", "DISC_FEATURE_COUNT"},
  { "BioSources with the same specimen voucher should have the same taxname", "DISC_SPECVOUCHER_TAXNAME_MISMATCH"},
  { "Feature partialness should agree with gene partialness if endpoints match", "DISC_GENE_PARTIAL_CONFLICT"},
  { "Flatfile representation of object contains suspect text", "DISC_FLATFILE_FIND_ONCALLER"},
  { "Flatfile representation of object contains fixable suspect text", "DISC_FLATFILE_FIND_ONCALLER"},
  { "Flatfile representation of object contains unfixable suspect text", "DISC_FLATFILE_FIND_ONCALLER"},
  { "Coding region product contains suspect text", "DISC_CDS_PRODUCT_FIND"},
  { "Definition lines should be unique", "DISC_DUP_DEFLINE"},
  { "ATCC strain should also appear in culture collection", "DUP_DISC_ATCC_CULTURE_CONFLICT"},
  { "For country USA, state should be present and abbreviated", "DISC_USA_STATE"},
  { "All non-protein sequences in a set should have the same moltype", "DISC_INCONSISTENT_MOLTYPES"},
  { "Records should have identical submit-blocks", "DISC_SUBMITBLOCK_CONFLICT"},
  { "Possible linker sequence after poly-A tail", "DISC_POSSIBLE_LINKER"},
  { "Publications with the same titles should have the same authors", "DISC_TITLE_AUTHOR_CONFLICT"},
  { "Genes and features that share endpoints should be on the same strand", "DISC_BAD_GENE_STRAND"},
  { "Eukaryotic sequences with a map source qualifier should also have a chromosome source qualifier", "DISC_MAP_CHROMOSOME_CONFLICT"},
  { "RBS features should have an overlapping gene", "DISC_RBS_WITHOUT_GENE"},
  { "All Cit-subs should have identical affiliations", "DISC_CITSUBAFFIL_CONFLICT"},
  { "Uncultured or environmental sources should have clone", "DISC_REQUIRED_CLONE"},
  { "Source Qualifier test for Asndisc", "DISC_SOURCE_QUALS_ASNDISC"},
  { "Eukaryotic sequences that are not genomic or macronuclear should not have mRNA features", "DISC_mRNA_ON_WRONG_SEQUENCE_TYPE"},
  { "When the organism lineage contains 'Retroviridae' and the molecule type is 'DNA', the location should be set as 'proviral'", "DISC_RETROVIRIDAE_DNA"},
  { "Check for correct capitalization in author names", "DISC_CHECK_AUTH_CAPS"},
  { "Check for gene or genes in rRNA and tRNA products and comments", "DISC_CHECK_RNA_PRODUCTS_AND_COMMENTS"},
  { "Microsatellites must have repeat type of tandem", "DISC_MICROSATELLITE_REPEAT_TYPE"},
  { "If D-loop or control region misc_feat is present, source must be mitochondrial", "DISC_MITOCHONDRION_REQUIRED"},
  { "Unpublished pubs should have titles", "DISC_UNPUB_PUB_WITHOUT_TITLE"},
  { "Check for Quality Scores", "DISC_QUALITY_SCORES"},
  { "rRNA product names should not contain 'internal', 'transcribed', or 'spacer'", "DISC_INTERNAL_TRANSCRIBED_SPACER_RRNA"},
  { "Find partial feature ends on sequences that could be extended", "DISC_PARTIAL_PROBLEMS"},
  { "Find partial feature ends on bacterial sequences that cannot be extended: on when non-eukaryote", "DISC_BACTERIAL_PARTIAL_NONEXTENDABLE_PROBLEMS"},
  { "Find partial feature ends on bacterial sequences that cannot be extended but have exceptions: on when non-eukaryote", "DISC_BACTERIAL_PARTIAL_NONEXTENDABLE_EXCEPTION"},
  { "rRNA product names should not contain 'partial' or 'domain'", "DISC_SUSPECT_RRNA_PRODUCTS"},
  { "suspect misc_feature comments", "DISC_SUSPECT_MISC_FEATURES"},
  { "Missing strain on bacterial 'Genus sp. strain'", "DISC_BACTERIA_MISSING_STRAIN"},
  { "Missing definition lines", "DISC_MISSING_DEFLINES"},
  { "Missing affiliation", "DISC_MISSING_AFFIL"},
  { "Bacterial sources should not have isolate", "DISC_BACTERIA_SHOULD_NOT_HAVE_ISOLATE"},
  { "Bacterial sequences should not have mRNA features", "DISC_BACTERIA_SHOULD_NOT_HAVE_MRNA"},
  { "Coding region has new exception", "DISC_CDS_HAS_NEW_EXCEPTION"},
  { "Trinomial sources should have corresponding qualifier", "DISC_TRINOMIAL_SHOULD_HAVE_QUALIFIER"},
  { "Source has metagenomic qualifier", "DISC_METAGENOMIC"},
  { "Source has metagenome_source qualifier", "DISC_METAGENOME_SOURCE"},
  { "Missing genes", "ONCALLER_GENE_MISSING"},
  { "Superfluous genes", "ONCALLER_SUPERFLUOUS_GENE"},
  { "Short rRNA Features", "DISC_SHORT_RRNA"},
  { "Authority and Taxname should match first two words", "ONCALLER_CHECK_AUTHORITY"},
  { "Submitter blocks and publications have consortiums", "ONCALLER_CONSORTIUM"},
  { "Strain and culture-collection values conflict", "ONCALLER_STRAIN_CULTURE_COLLECTION_MISMATCH"},
  { "Comma or semicolon appears in strain or isolate", "ONCALLER_MULTISRC"} ,
  { "Multiple culture-collection quals", "ONCALLER_MULTIPLE_CULTURE_COLLECTION"},
  { "Segsets present", "DISC_SEGSETS_PRESENT"},
  { "Eco, mut, phy or pop sets present", "DISC_NONWGS_SETS_PRESENT"},
  { "Feature List", "DISC_FEATURE_LIST"},
  { "Category Header", "DISC_CATEGORY_HEADER"},
  { "Mismatched Comments", "DISC_MISMATCHED_COMMENTS"},
  { "BioSources with the same strain should have the same taxname", "DISC_STRAIN_TAXNAME_MISMATCH"},
  { "'Human' in host should be 'Homo sapiens'", "DISC_HUMAN_HOST"},
  { "Genes on bacterial sequences should start with lowercase letters: on when non-eukaryote", "DISC_BAD_BACTERIAL_GENE_NAME"},
  { "Bad gene names", "TEST_BAD_GENE_NAME"},
  { "Location is ordered (intervals interspersed with gaps)", "ONCALLER_ORDERED_LOCATION"},
  { "Comment descriptor present", "ONCALLER_COMMENT_PRESENT"},
  { "Titles on sets", "ONCALLER_DEFLINE_ON_SET"},
  { "HIV RNA location or molecule type inconsistent", "ONCALLER_HIV_RNA_INCONSISTENT"},
  { "Protein sequences should be at least 50 aa, unless they are partial", "SHORT_PROT_SEQUENCES"},
  { "mRNA sequences should not have exons", "TEST_EXON_ON_MRNA"},
  { "Sequences with project IDs", "TEST_HAS_PROJECT_ID"},
  { "Feature has standard_name qualifier", "ONCALLER_HAS_STANDARD_NAME"},
  { "Missing structured comments", "ONCALLER_MISSING_STRUCTURED_COMMENTS"},
  { "Bacteria should have strain", "DISC_REQUIRED_STRAIN"},
  { "Bioseqs should have GenomeAssembly structured comments", "MISSING_GENOMEASSEMBLY_COMMENTS"},
  { "Bacterial taxnames should end with strain", "DISC_BACTERIAL_TAX_STRAIN_MISMATCH"},
  { "CDS has CDD Xref", "TEST_CDS_HAS_CDD_XREF"},
  { "Sequence contains unusual nucleotides", "TEST_UNUSUAL_NT"},
  { "Sequence contains regions of low quality", "TEST_LOW_QUALITY_REGION"},
  { "Organelle location should have genomic moltype", "TEST_ORGANELLE_NOT_GENOMIC"},
  { "Intergenic spacer without plastid location", "TEST_UNWANTED_SPACER"},
  { "Organelle products on non-organelle sequence: on when neither bacteria nor virus", "TEST_ORGANELLE_PRODUCTS"},
  { "Organism ending in sp. needs tax consult", "TEST_SP_NOT_UNCULTURED"},
  { "mRNA sequence contains rearranged or germline", "TEST_BAD_MRNA_QUAL"},
  { "Unnecessary environmental qualifier present", "TEST_UNNECESSARY_ENVIRONMENTAL"},
  { "Unnecessary gene features on virus: on when lineage is not Picornaviridae,Potyviridae,Flaviviridae and Togaviridae", "TEST_UNNECESSARY_VIRUS_GENE"},
  { "Set wrapper on microsatellites or rearranged genes", "TEST_UNWANTED_SET_WRAPPER"},
  { "Missing values in primer set", "TEST_MISSING_PRIMER"},
  { "Unexpected misc_RNA features", "TEST_UNUSUAL_MISC_RNA"},
  { "Species-specific primers, no environmental sample", "TEST_AMPLIFIED_PRIMERS_NO_ENVIRONMENTAL_SAMPLE"},
  { "Duplicate genes on opposite strands", "TEST_DUP_GENES_OPPOSITE_STRANDS"},
  { "Problems with small genome sets", "TEST_SMALL_GENOME_SET_PROBLEM"},
  { "Overlapping rRNA features", "TEST_OVERLAPPING_RRNAS"},
  { "mRNA sequences have CDS/gene on the complement strand", "TEST_MRNA_SEQUENCE_MINUS_STRAND_FEATURES"},
  { "Complete taxname should be present in definition line", "TEST_TAXNAME_NOT_IN_DEFLINE"},
  { "Count number of unverified sequences", "TEST_COUNT_UNVERIFIED"},
  { "Show translation exception", "SHOW_TRANSL_EXCEPT"},
  { "Show hypothetic protein having a gene name", "SHOW_HYPOTHETICAL_CDS_HAVING_GENE_NAME"},
  { "Test defline existence", "TEST_DEFLINE_PRESENT"},
  { "Remove mRNA overlapping a pseudogene", "TEST_MRNA_OVERLAPPING_PSEUDO_GENE"},
  { "Find completely overlapped genes", "FIND_OVERLAPPED_GENES"},
  { "Test BioSources with the same biomaterial but different taxname", "DISC_BIOMATERIAL_TAXNAME_MISMATCH"},
  { "Test BioSources with the same culture collection but different taxname", "DISC_CULTURE_TAXNAME_MISMATCH"},
  { "Test author names missing first and/or last names", "DISC_CHECK_AUTH_NAME"},
  {"Non-Retroviridae biosources are proviral", "NON_RETROVIRIDAE_PROVIRAL"},
  {"RNA bioseqs are proviral", "RNA_PROVIRAL"},
  {"Find sequences Less Than 200 bp", "SHORT_SEQUENCES_200"},
  {"Greater than 10 percent Ns", "DISC_10_PERCENTN"},
  {"Runs of more than 14 Ns", "N_RUNS_14"},
  {"Moltype not mRNA", "MOLTYPE_NOT_MRNA"},
  {"Technique not set as TSA", "TECHNIQUE_NOT_TSA"},
  {"Structured comment not included",  "MISSING_STRUCTURED_COMMENT"},
  {"Project not included", "MISSING_PROJECT"},
  {"Multiple CDS on mRNA", "MULTIPLE_CDS_ON_MRNA"},
  {"CBS strain should also appear in culture collection", "DUP_DISC_CBS_CULTURE_CONFLICT"},
  {"Division code conflicts found", "DIVISION_CODE_CONFLICTS"},
  {"rRNA Standard name conflicts found", "RRNA_NAME_CONFLICTS"},
  {"Eukaryote should have mRNA", "EUKARYOTE_SHOULD_HAVE_MRNA"},
  {"mRNA should have both protein_id and transcript_id", "MRNA_SHOULD_HAVE_PROTEIN_TRANSCRIPT_IDS"},
  {"Country discription should only have 1 colon.", "ONCALLER_COUNTRY_COLON"},
  {"Sequences with BioProject IDs","ONCALLER_BIOPROJECT_ID"},
  {"Type strain comment in OrgMod does not agree with organism name", "ONCALLER_STRAIN_TAXNAME_CONFLICT"},
  {"SubSource collected-by contains more than 3 names", "ONCALLER_MORE_NAMES_COLLECTED_BY"},
  {"SubSource identified-by contains more than 3 names", "ONCALLER_MORE_OR_SPEC_NAMES_IDENTIFIED_BY"},
  {"Suspected organism in identified-by SubSource", "ONCALLER_SUSPECTED_ORG_IDENTIFIED"},
  {"Suspected organism in collected-by SubSource", "ONCALLER_SUSPECTED_ORG_COLLECTED"},
  {"Suspicious structured comment prefix", "ONCALLER_SWITCH_STRUCTURED_COMMENT_PREFIX"},
  {"Cit-sub affiliation street contains text from other affiliation fields", "DISC_CITSUB_AFFIL_DUP_TEXT"},
  {"Duplicate PCR primer pair", "ONCALLER_DUPLICATE_PRIMER_SET"},
  {"Country name end with colon", "END_COLON_IN_COUNTRY"},
  {"Frequently appearing proteins", "DISC_PROTEIN_NAMES"},
  {"Sequence characters at end of defline", "DISC_TITLE_ENDS_WITH_SEQUENCE"},
  {"Inconsistent structured comments", "DISC_INCONSISTENT_STRUCTURED_COMMENTS"},
  {"Inconsistent DBLink fields", "DISC_INCONSISTENT_DBLINK"},
  {"Inconsistent Molinfo Techniqueq", "DISC_INCONSISTENT_MOLINFO_TECH"},
  {"Sequences with gaps", "DISC_GAPS"},
  {"Bad BGPIPE qualifiers", "DISC_BAD_BGPIPE_QUALS"},
  {"Short lncRNA sequences", "TEST_SHORT_LNCRNA"},
  {"Ns at end of sequences", "TEST_TERMINAL_NS"},
  {"Alignment has score attribute", "TEST_ALIGNMENT_HAS_SCORE"},
  {"Uncultured Notes", "UNCULTURED_NOTES_ONCALLER"}
};

void CDiscRepOutput :: x_InitTestDesc()
{
  m_TestDesc["MISSING_GENES"] = "Missing Genes";
  m_TestDesc["EXTRA_GENES"]   = "Extra Genes";
  m_TestDesc["MISSING_LOCUS_TAGS"]   = "Missing Locus Tags";
  m_TestDesc["DUPLICATE_LOCUS_TAGS"] = "Duplicate Locus Tags";
  m_TestDesc["BAD_LOCUS_TAG_FORMAT"] = "Bad Locus Tag Format";
  m_TestDesc["INCONSISTENT_LOCUS_TAG_PREFIX"] = "Inconsistent Locus Tag Prefix";
  m_TestDesc["NON_GENE_LOCUS_TAG"]     = "Nongene Locus Tag";
  m_TestDesc["DISC_COUNT_NUCLEOTIDES"] = "Count nucleotide sequences";
  m_TestDesc["MISSING_PROTEIN_ID"]     = "Missing Protein ID";
  m_TestDesc["INCONSISTENT_PROTEIN_ID"]   = "Inconsistent Protein ID";
  m_TestDesc["FEATURE_LOCATION_CONFLICT"] = "Feature Location Conflict";
  m_TestDesc["GENE_PRODUCT_CONFLICT"] = "Gene Product Conflict";
  m_TestDesc["DUPLICATE_GENE_LOCUS"]  = "Duplicate Gene Locus";
  m_TestDesc["EC_NUMBER_NOTE"]  = "EC Number Note";
  m_TestDesc["PSEUDO_MISMATCH"] = "Pseudo Mismatch";
  m_TestDesc["JOINED_FEATURES"] = "Joined Features: on when non-eukaryote";
  m_TestDesc["OVERLAPPING_GENES"] = "Overlapping Genes";
  m_TestDesc["OVERLAPPING_CDS"] = "Overlapping CDS";
  m_TestDesc["CONTAINED_CDS"]   = "Contained CDS";
  m_TestDesc["RNA_CDS_OVERLAP"] = "CDS RNA Overlap";
  m_TestDesc["SHORT_CONTIG"]    = "Short Contig";
  m_TestDesc["INCONSISTENT_BIOSOURCE"] = "Inconsistent BioSource";
  m_TestDesc["SUSPECT_PRODUCT_NAMES"]  = "Suspect Product Name";
  m_TestDesc["DISC_PRODUCT_NAME_TYPO"] = "Suspect Product Name Typo";
  m_TestDesc["DISC_PRODUCT_NAME_QUICKFIX"]  = "Suspect Product Name QuickFix";
  m_TestDesc["INCONSISTENT_SOURCE_DEFLINE"] = "Inconsistent Source And Definition Line";
  m_TestDesc["PARTIAL_CDS_COMPLETE_SEQUENCE"] = "Partial CDSs in Complete Sequences";
  m_TestDesc["EC_NUMBER_ON_UNKNOWN_PROTEIN"]  = "Hypothetical or Unknown Protein with EC Number";
  m_TestDesc["TAX_LOOKUP_MISSING"]  = "Find Missing Tax Lookups";
  m_TestDesc["TAX_LOOKUP_MISMATCH"] = "Find Tax Lookup Mismatches";
  m_TestDesc["SHORT_SEQUENCES"] = "Find Short Sequences";
  m_TestDesc["SUSPECT_PHRASES"] = "Suspect Phrases";
  m_TestDesc["DISC_SUSPICIOUS_NOTE_TEXT"] = "Find Suspicious Phrases in Note Text";
  m_TestDesc["COUNT_TRNAS"]    = "Count tRNAs";
  m_TestDesc["FIND_DUP_TRNAS"] = "Find Duplicate tRNAs";
  m_TestDesc["FIND_BADLEN_TRNAS"] = "Find short and long tRNAs";
  m_TestDesc["FIND_STRAND_TRNAS"] = "Find tRNAs on the same strand";
  m_TestDesc["COUNT_RRNAS"]    = "Count rRNAs";
  m_TestDesc["FIND_DUP_RRNAS"] = "Find Duplicate rRNAs";
  m_TestDesc["RNA_NO_PRODUCT"] = "Find RNAs without Products";
  m_TestDesc["TRANSL_NO_NOTE"] = "Transl_except without Note";
  m_TestDesc["NOTE_NO_TRANSL"] = "Note without Transl_except";
  m_TestDesc["TRANSL_TOO_LONG"]  = "Transl_except longer than 3";
  m_TestDesc["CDS_TRNA_OVERLAP"] = "CDS tRNA overlaps";
  m_TestDesc["COUNT_PROTEINS"]   = "Count Proteins";
  m_TestDesc["DISC_FEAT_OVERLAP_SRCFEAT"]  = "Features Intersecting Source Features";
  m_TestDesc["MISSING_GENPRODSET_PROTEIN"] = "CDS on GenProdSet without protein";
  m_TestDesc["DUP_GENPRODSET_PROTEIN"] = "Multiple CDS on GenProdSet, same protein";
  m_TestDesc["MISSING_GENPRODSET_TRANSCRIPT_ID"] = "mRNA on GenProdSet without transcript ID";
  m_TestDesc["DISC_DUP_GENPRODSET_TRANSCRIPT_ID"]= "mRNA on GenProdSet with duplicate ID";
  m_TestDesc["DISC_PERCENT_N"] = "Greater than 5 percent Ns";
  m_TestDesc["N_RUNS"]         = "Runs of 10 or more Ns";
  m_TestDesc["ZERO_BASECOUNT"] = "Zero Base Counts";
  m_TestDesc["ADJACENT_PSEUDOGENES"] = "Adjacent PseudoGenes with Identical Text";
  m_TestDesc["DISC_LONG_NO_ANNOTATION"]= "Bioseqs longer than 5000nt without Annotations";
  m_TestDesc["NO_ANNOTATION"] = "Bioseqs without Annotations";
  m_TestDesc["DISC_INFLUENZA_DATE_MISMATCH"]= "Influenza Strain/Collection Date Mismatch";
  m_TestDesc["DISC_SHORT_INTRON"] = "Introns shorter than 10 nt";
  m_TestDesc["DISC_MISSING_VIRAL_QUALS"] = "Viruses should specify collection-date, country, and specific-host";
  m_TestDesc["DISC_SRC_QUAL_PROBLEM"] = "Source Qualifier Report";
  m_TestDesc["DISC_MISSING_SRC_QUAL"] = "All sources in a record should have the same qualifier set";
  m_TestDesc["DISC_DUP_SRC_QUAL"] = "Each source in a record should have unique values for qualifiers";
  m_TestDesc["DISC_DUP_SRC_QUAL_DATA"] = "Each qualifier on a source should have different values";
  m_TestDesc["DISC_HAPLOTYPE_MISMATCH"]= "Sequences with the same haplotype should match";
  m_TestDesc["DISC_FEATURE_MOLTYPE_MISMATCH"] = "Sequences with rRNA or misc_RNA features should be genomic DNA";
  m_TestDesc["DISC_CDS_WITHOUT_MRNA"] = "Coding regions on eukaryotic genomic DNA should have mRNAs with matching products";
  m_TestDesc["DISC_EXON_INTRON_CONFLICT"] = "Exon and intron locations should abut (unless gene is trans-spliced)";
  m_TestDesc["DISC_FEATURE_COUNT"] = "Count features present or missing from sequences";
  m_TestDesc["DISC_SPECVOUCHER_TAXNAME_MISMATCH"] = "BioSources with the same specimen voucher should have the same taxname";
  m_TestDesc["DISC_GENE_PARTIAL_CONFLICT"] = "Feature partialness should agree with gene partialness if endpoints match";
  m_TestDesc["DISC_FLATFILE_FIND_ONCALLER"] = "Flatfile representation of object contains suspect text";
  m_TestDesc["DISC_FLATFILE_FIND_ONCALLER"] = "Flatfile representation of object contains fixable suspect text";
  m_TestDesc["DISC_FLATFILE_FIND_ONCALLER"] = "Flatfile representation of object contains unfixable suspect text";
  m_TestDesc["DISC_CDS_PRODUCT_FIND"] = "Coding region product contains suspect text";
  m_TestDesc["DISC_DUP_DEFLINE"]      = "Definition lines should be unique";
  m_TestDesc["DUP_DISC_ATCC_CULTURE_CONFLICT"] = "ATCC strain should also appear in culture collection";
  m_TestDesc["DISC_USA_STATE"] = "For country USA, state should be present and abbreviated";
  m_TestDesc["DISC_INCONSISTENT_MOLTYPES"] = "All non-protein sequences in a set should have the same moltype";
  m_TestDesc["DISC_SUBMITBLOCK_CONFLICT"] = "Records should have identical submit-blocks";
  m_TestDesc["DISC_POSSIBLE_LINKER"]      = "Possible linker sequence after poly-A tail";
  m_TestDesc["DISC_TITLE_AUTHOR_CONFLICT"] = "Publications with the same titles should have the same authors";
  m_TestDesc["DISC_BAD_GENE_STRAND"] = "Genes and features that share endpoints should be on the same strand";
  m_TestDesc["DISC_MAP_CHROMOSOME_CONFLICT"] = "Eukaryotic sequences with a map source qualifier should also have a chromosome source qualifier";
  m_TestDesc["DISC_RBS_WITHOUT_GENE"] = "RBS features should have an overlapping gene";
  m_TestDesc["DISC_CITSUBAFFIL_CONFLICT"] = "All Cit-subs should have identical affiliations";
  m_TestDesc["DISC_REQUIRED_CLONE"] = "Uncultured or environmental sources should have clone";
  m_TestDesc["DISC_SOURCE_QUALS_ASNDISC"] = "Source Qualifier test for Asndisc";
  m_TestDesc["DISC_mRNA_ON_WRONG_SEQUENCE_TYPE"] = "Eukaryotic sequences that are not genomic or macronuclear should not have mRNA features";
  m_TestDesc["DISC_RETROVIRIDAE_DNA"] = "When the organism lineage contains 'Retroviridae' and the molecule type is 'DNA', the location should be set as 'proviral'";
  m_TestDesc["DISC_CHECK_AUTH_CAPS"] = "Check for correct capitalization in author names";
  m_TestDesc["DISC_CHECK_RNA_PRODUCTS_AND_COMMENTS"] = "Check for gene or genes in rRNA and tRNA products and comments";
  m_TestDesc["DISC_MICROSATELLITE_REPEAT_TYPE"] = "Microsatellites must have repeat type of tandem";
  m_TestDesc["DISC_MITOCHONDRION_REQUIRED"] = "If D-loop or control region misc_feat is present, source must be mitochondrial";
  m_TestDesc["DISC_UNPUB_PUB_WITHOUT_TITLE"] = "Unpublished pubs should have titles";
  m_TestDesc["DISC_QUALITY_SCORES"]          = "Check for Quality Scores";
  m_TestDesc["DISC_INTERNAL_TRANSCRIBED_SPACER_RRNA"] = "rRNA product names should not contain 'internal', 'transcribed', or 'spacer'";
  m_TestDesc["DISC_PARTIAL_PROBLEMS"] = "Find partial feature ends on sequences that could be extended";
  m_TestDesc["DISC_BACTERIAL_PARTIAL_NONEXTENDABLE_PROBLEMS"] = "Find partial feature ends on bacterial sequences that cannot be extended: on when non-eukaryote";
  m_TestDesc["DISC_BACTERIAL_PARTIAL_NONEXTENDABLE_EXCEPTION"] = "Find partial feature ends on bacterial sequences that cannot be extended but have exceptions: on when non-eukaryote";
  m_TestDesc["DISC_SUSPECT_RRNA_PRODUCTS"] = "rRNA product names should not contain 'partial' or 'domain'";
  m_TestDesc["DISC_SUSPECT_MISC_FEATURES"] = "suspect misc_feature comments";
  m_TestDesc["DISC_BACTERIA_MISSING_STRAIN"] = "Missing strain on bacterial 'Genus sp. strain'";
  m_TestDesc["DISC_MISSING_DEFLINES"] = "Missing definition lines";
  m_TestDesc["DISC_MISSING_AFFIL"]    = "Missing affiliation";
  m_TestDesc["DISC_BACTERIA_SHOULD_NOT_HAVE_ISOLATE"] = "Bacterial sources should not have isolate";
  m_TestDesc["DISC_BACTERIA_SHOULD_NOT_HAVE_MRNA"] = "Bacterial sequences should not have mRNA features";
  m_TestDesc["DISC_CDS_HAS_NEW_EXCEPTION"] = "Coding region has new exception";
  m_TestDesc["DISC_TRINOMIAL_SHOULD_HAVE_QUALIFIER"] = "Trinomial sources should have corresponding qualifier";
  m_TestDesc["DISC_METAGENOMIC"]       = "Source has metagenomic qualifier";
  m_TestDesc["DISC_METAGENOME_SOURCE"] = "Source has metagenome_source qualifier";
  m_TestDesc["ONCALLER_GENE_MISSING"]  = "Missing genes";
  m_TestDesc["ONCALLER_SUPERFLUOUS_GENE"] = "Superfluous genes";
  m_TestDesc["DISC_SHORT_RRNA"]          = "Short rRNA Features";
  m_TestDesc["ONCALLER_CHECK_AUTHORITY"] = "Authority and Taxname should match first two words";
  m_TestDesc["ONCALLER_CONSORTIUM"] = "Submitter blocks and publications have consortiums";
  m_TestDesc["ONCALLER_STRAIN_CULTURE_COLLECTION_MISMATCH"] = "Strain and culture-collection values conflict";
  m_TestDesc["ONCALLER_MULTISRC"] = "Comma or semicolon appears in strain or isolate";
  m_TestDesc["ONCALLER_MULTIPLE_CULTURE_COLLECTION"]= "Multiple culture-collection quals";
  m_TestDesc["DISC_SEGSETS_PRESENT"]     = "Segsets present";
  m_TestDesc["DISC_NONWGS_SETS_PRESENT"] = "Eco, mut, phy or pop sets present";
  m_TestDesc["DISC_FEATURE_LIST"]    = "Feature List";
  m_TestDesc["DISC_CATEGORY_HEADER"] = "Category Header";
  m_TestDesc["DISC_MISMATCHED_COMMENTS"]     = "Mismatched Comments";
  m_TestDesc["DISC_STRAIN_TAXNAME_MISMATCH"] = "BioSources with the same strain should have the same taxname";
  m_TestDesc["DISC_HUMAN_HOST"] = "'Human' in host should be 'Homo sapiens'";
  m_TestDesc["DISC_BAD_BACTERIAL_GENE_NAME"] = "Genes on bacterial sequences should start with lowercase letters: on when non-eukaryote";
  m_TestDesc["TEST_BAD_GENE_NAME"]        = "Bad gene names";
  m_TestDesc["ONCALLER_ORDERED_LOCATION"] = "Location is ordered (intervals interspersed with gaps)";
  m_TestDesc["ONCALLER_COMMENT_PRESENT"] = "Comment descriptor present";
  m_TestDesc["ONCALLER_DEFLINE_ON_SET"]  = "Titles on sets";
  m_TestDesc["ONCALLER_HIV_RNA_INCONSISTENT"] = "HIV RNA location or molecule type inconsistent";
  m_TestDesc["SHORT_PROT_SEQUENCES"] = "Protein sequences should be at least 50 aa, unless they are partial";
  m_TestDesc["TEST_EXON_ON_MRNA"]   = "mRNA sequences should not have exons";
  m_TestDesc["TEST_HAS_PROJECT_ID"] = "Sequences with project IDs";
  m_TestDesc["ONCALLER_HAS_STANDARD_NAME"] = "Feature has standard_name qualifier";
  m_TestDesc["ONCALLER_MISSING_STRUCTURED_COMMENTS"] = "Missing structured comments";
  m_TestDesc["DISC_REQUIRED_STRAIN"] = "Bacteria should have strain";
  m_TestDesc["MISSING_GENOMEASSEMBLY_COMMENTS"] = "Bioseqs should have GenomeAssembly structured comments";
  m_TestDesc["DISC_BACTERIAL_TAX_STRAIN_MISMATCH"] = "Bacterial taxnames should end with strain";
  m_TestDesc["TEST_CDS_HAS_CDD_XREF"] = "CDS has CDD Xref";
  m_TestDesc["TEST_UNUSUAL_NT"]       = "Sequence contains unusual nucleotides";
  m_TestDesc["TEST_LOW_QUALITY_REGION"]    = "Sequence contains regions of low quality";
  m_TestDesc["TEST_ORGANELLE_NOT_GENOMIC"] = "Organelle location should have genomic moltype";
  m_TestDesc["TEST_UNWANTED_SPACER"] = "Intergenic spacer without plastid location";
  m_TestDesc["TEST_ORGANELLE_PRODUCTS"] = "Organelle products on non-organelle sequence: on when neither bacteria nor virus";
  m_TestDesc["TEST_SP_NOT_UNCULTURED"] = "Organism ending in sp. needs tax consult";
  m_TestDesc["TEST_BAD_MRNA_QUAL"] = "mRNA sequence contains rearranged or germline";
  m_TestDesc["TEST_UNNECESSARY_ENVIRONMENTAL"] = "Unnecessary environmental qualifier present";
  m_TestDesc["TEST_UNNECESSARY_VIRUS_GENE"] = "Unnecessary gene features on virus: on when lineage is not Picornaviridae,Potyviridae,Flaviviridae and Togaviridae";
  m_TestDesc["TEST_UNWANTED_SET_WRAPPER"] = "Set wrapper on microsatellites or rearranged genes";
  m_TestDesc["TEST_MISSING_PRIMER"] = "Missing values in primer set";
  m_TestDesc["TEST_UNUSUAL_MISC_RNA"] = "Unexpected misc_RNA features";
  m_TestDesc["TEST_AMPLIFIED_PRIMERS_NO_ENVIRONMENTAL_SAMPLE"] = "Species-specific primers, no environmental sample";
  m_TestDesc["TEST_DUP_GENES_OPPOSITE_STRANDS"] = "Duplicate genes on opposite strands";
  m_TestDesc["TEST_SMALL_GENOME_SET_PROBLEM"] = "Problems with small genome sets";
  m_TestDesc["TEST_OVERLAPPING_RRNAS"] = "Overlapping rRNA features";
  m_TestDesc["TEST_MRNA_SEQUENCE_MINUS_STRAND_FEATURES"] = "mRNA sequences have CDS/gene on the complement strand";
  m_TestDesc["TEST_TAXNAME_NOT_IN_DEFLINE"] = "Complete taxname should be present in definition line";
  m_TestDesc["TEST_COUNT_UNVERIFIED"] = "Count number of unverified sequences";
  m_TestDesc["SHOW_TRANSL_EXCEPT"] = "Show translation exception";
  m_TestDesc["SHOW_HYPOTHETICAL_CDS_HAVING_GENE_NAME"] = "Show hypothetic protein having a gene name";
  m_TestDesc["TEST_DEFLINE_PRESENT"] = "Test defline existence";
  m_TestDesc["TEST_MRNA_OVERLAPPING_PSEUDO_GENE"] = "Remove mRNA overlapping a pseudogene";
  m_TestDesc["FIND_OVERLAPPED_GENES"] = "Find completely overlapped genes";
  m_TestDesc["DISC_BIOMATERIAL_TAXNAME_MISMATCH"] = "Test BioSources with the same biomaterial but different taxname";
  m_TestDesc["DISC_CULTURE_TAXNAME_MISMATCH"] = "Test BioSources with the same culture collection but different taxname";
  m_TestDesc["DISC_CHECK_AUTH_NAME"] = "Test author names missing first and/or last names";
  m_TestDesc["NON_RETROVIRIDAE_PROVIRAL"] = "Non-Retroviridae biosources are proviral";
  m_TestDesc["RNA_PROVIRAL"] = "RNA bioseqs are proviral";
  m_TestDesc["SHORT_SEQUENCES_200"] = "Find sequences Less Than 200 bp";
  m_TestDesc["DISC_10_PERCENTN"] = "Greater than 10 percent Ns";
  m_TestDesc["N_RUNS_14"] = "Runs of more than 14 Ns";
  m_TestDesc["MOLTYPE_NOT_MRNA"] = "Moltype not mRNA";
  m_TestDesc["TECHNIQUE_NOT_TSA"] = "Technique not set as TSA";
  m_TestDesc["MISSING_STRUCTURED_COMMENT"] = "Structured comment not included";
  m_TestDesc["MISSING_PROJECT"] = "Project not included";
  m_TestDesc["MULTIPLE_CDS_ON_MRNA"] = "Multiple CDS on mRNA";
  m_TestDesc["DUP_DISC_CBS_CULTURE_CONFLICT"] = "CBS strain should also appear in culture collection";
  m_TestDesc["DIVISION_CODE_CONFLICTS"] = "Division code conflicts found";
  m_TestDesc["RRNA_NAME_CONFLICTS"] = "rRNA Standard name conflicts found";
  m_TestDesc["EUKARYOTE_SHOULD_HAVE_MRNA"] = "Eukaryote should have mRNA";
  m_TestDesc["MRNA_SHOULD_HAVE_PROTEIN_TRANSCRIPT_IDS"] = "mRNA should have both protein_id and transcript_id";
  m_TestDesc["ONCALLER_COUNTRY_COLON"] = "Country discription should only have 1 colon.";
  m_TestDesc["ONCALLER_BIOPROJECT_ID"] = "Sequences with BioProject IDs";
  m_TestDesc["ONCALLER_STRAIN_TAXNAME_CONFLICT"] = "Type strain comment in OrgMod does not agree with organism name";
  m_TestDesc["ONCALLER_MORE_NAMES_COLLECTED_BY"] = "SubSource collected-by contains more than 3 names";
  m_TestDesc["ONCALLER_MORE_OR_SPEC_NAMES_IDENTIFIED_BY"] = "SubSource identified-by contains more than 3 names";
  m_TestDesc["ONCALLER_SUSPECTED_ORG_IDENTIFIED"] = "Suspected organism in identified-by SubSource";
  m_TestDesc["ONCALLER_SUSPECTED_ORG_COLLECTED"] = "Suspected organism in collected-by SubSource";
  m_TestDesc["ONCALLER_SWITCH_STRUCTURED_COMMENT_PREFIX"] = "Suspicious structured comment prefix";
  m_TestDesc["DISC_CITSUB_AFFIL_DUP_TEXT"] = "Cit-sub affiliation street contains text from other affiliation fields";
  m_TestDesc["ONCALLER_DUPLICATE_PRIMER_SET"] = "Duplicate PCR primer pair";
  m_TestDesc["END_COLON_IN_COUNTRY"] = "Country name end with colon";
  m_TestDesc["DISC_PROTEIN_NAMES"] = "Frequently appearing proteins";
  m_TestDesc["DISC_TITLE_ENDS_WITH_SEQUENCE"] = "Sequence characters at end of defline";
  m_TestDesc["DISC_INCONSISTENT_STRUCTURED_COMMENTS"]= "Inconsistent structured comments";
  m_TestDesc["DISC_INCONSISTENT_DBLINK"] = "Inconsistent DBLink fields";
  m_TestDesc["DISC_INCONSISTENT_MOLINFO_TECH"] = "Inconsistent Molinfo Techniqueq";
  m_TestDesc["DISC_GAPS"] = "Sequences with gaps";
  m_TestDesc["DISC_BAD_BGPIPE_QUALS"] = "Bad BGPIPE qualifiers";
  m_TestDesc["TEST_SHORT_LNCRNA"] = "Short lncRNA sequences";
  m_TestDesc["TEST_TERMINAL_NS"] = "Ns at end of sequences";
  m_TestDesc["TEST_ALIGNMENT_HAS_SCORE"] = "Alignment has score attribute";
  m_TestDesc["UNCULTURED_NOTES_ONCALLER"] = "Uncultured Notes";
};

void CDiscRepOutput :: x_SortReport(UInt2UInts& prt_ord)
{
   // ini.
   Str2UInt  list_ord;
   unsigned i;
   for (i=0; i< ArraySize(disc_info_list); i++) {
       list_ord[disc_info_list[i].setting_name] = i;
   }

   i=0;
   ITERATE (vector <CRef <CClickableItem> >, it, thisInfo.disc_report_data) {
      prt_ord[list_ord[(*it)->setting_name]].push_back(i++);
   }
};

void CDiscRepOutput :: x_Clear(UInt2UInts* prt_ord)
{
   thisInfo.test_item_list.clear();
   thisInfo.test_item_objs.clear();
   thisInfo.disc_report_data.clear();
   if (prt_ord) prt_ord->clear();
  
   thisInfo.num_entry = 0;
};

// for asndisc
void CDiscRepOutput :: Export()
{
   COutputConfig& oc = thisInfo.output_config;

   UInt2UInts prt_ord;
   x_SortReport(prt_ord);

  if (oc.add_output_tag || oc.add_extra_output_tag) {
       x_AddListOutputTags();
  }

  xml::document xmldoc("asndisc");
  xml::node& xml_root = xmldoc.get_root_node();
  
  if (!oc.xml) {
    *(oc.output_f) << "Discrepancy Report Results\n\n" << "Summary\n";
  }
  x_WriteDiscRepSummary(xml_root, prt_ord);

  if (!oc.xml) {
    *(oc.output_f) << "\n\nDetailed Report\n";
  }
  x_InitTestDesc();   // change code 
  x_WriteDiscRepDetails(thisInfo.disc_report_data, 
                              xml_root, prt_ord, oc.use_flag);

  if (oc.xml) {
    // set some document settings
    xmldoc.set_is_standalone(true);
    xmldoc.set_encoding("UTF-8");

    *(oc.output_f) << xmldoc;
  }
  oc.output_f->flush();

  x_Clear(&prt_ord);
};  // Asndisc:: Export

bool CDiscRepOutput :: x_RmTagInDescp(string& str, const string& tag)
{
  string::size_type pos;

  pos = str.find(tag);
  if (pos != string::npos && !pos) {
      str =  CTempString(str).substr(tag.size());
      return true;
  }
  else {
     return false;
  }
}


void CDiscRepOutput :: x_WriteDiscRepSummary(xml::node& xml_root, UInt2UInts& prt_ord)
{
  string desc, strtmp, setting_name;
  bool is_SusProdNm = false;
  COutputConfig& oc = thisInfo.output_config;

  CRef <CClickableItem> c_item;

  ITERATE (UInt2UInts, it, prt_ord) {
    ITERATE (vector <unsigned>, jt, it->second) {

      c_item = thisInfo.disc_report_data[*jt];
      desc = c_item->description;
      if ( desc.empty()) continue;

      // FATAL tag
      if (x_RmTagInDescp(desc, "FATAL: ") && !oc.xml) {
           *(oc.output_f)  << "FATAL: ";
      }
      
      // msg level
      if ( oc.msg_level == COutputConfig :: e_Fatal && !(c_item->fatal)) {
         continue;
      }

      strtmp = c_item->fatal ? "FATAL" : "INFO";
      setting_name = c_item->setting_name;
      is_SusProdNm = ("SUSPECT_PRODUCT_NAMES" == setting_name);
      if (!oc.xml) {
         *(oc.output_f) << setting_name << ": " << desc << endl;
      }
      else { 
         xml::node::iterator 
             xit = xml_root.insert(xml_root.end(), 
                                   xml::node("message", desc.c_str()));
         // set attributes:
         xit->get_attributes().insert("code", setting_name.c_str());
         xit->get_attributes().insert("prefix", "Summary");
         xit->get_attributes().insert("severity", strtmp.c_str());
      }
      if (is_SusProdNm) {
           x_WriteDiscRepSubcategories(c_item->subcategories, xml_root);
      }
    }
  }
  
} // WriteDiscrepancyReportSummary


void CDiscRepOutput:: x_WriteDiscRepSubcategories(const vector <CRef <CClickableItem> >& subcategories, xml::node& xml_node, unsigned ident)
{
   COutputConfig& oc = thisInfo.output_config;
   unsigned i;
   string desc, strtmp;
   ITERATE (vector <CRef <CClickableItem> >, it, subcategories) {
      desc = (*it)->description;
      if (oc.msg_level == COutputConfig :: e_Fatal && !(*it)->fatal) {
          continue;
      }
      if (!oc.xml) {
         for (i=0; i< ident; i++) *(oc.output_f) << "\t"; 
         *(oc.output_f) << desc << endl; 
      }
      else {
        xml::node sub_node("message", desc.c_str());

         // set attributes:
        strtmp = (*it)->fatal ? "FATAL" : "INFO";
        sub_node.get_attributes().insert("code",((*it)->setting_name.c_str()));
        sub_node.get_attributes().insert("prefix", "Summary");
        sub_node.get_attributes().insert("severity", strtmp.c_str());
        xml_node.push_back(sub_node);
      }
      x_WriteDiscRepSubcategories( (*it)->subcategories, xml_node, ident+1);
   }
} // WriteDiscRepSubcategories


bool CDiscRepOutput :: x_OkToExpand(CRef < CClickableItem > c_item) 
{
  COutputConfig& oc = thisInfo.output_config;

  if (c_item->setting_name == "DISC_FEATURE_COUNT") {
       return false;
  }
  else if ( (c_item->item_list.empty() || c_item->expanded)
#if 0
                     ||oc.expand_report_categories[c_item->setting_name])
#endif
             && !(c_item->subcategories.empty()) ) {
      return true;
  }
  else {
     return false;
  }
};


bool CDiscRepOutput :: x_SubsHaveTags(CRef <CClickableItem> c_item)
{
  if (c_item->subcategories.empty()) {
      return false;
  }
  size_t pos;
  ITERATE (vector <CRef <CClickableItem> >, sit, c_item->subcategories) {
    pos = (*sit)->description.find("FATAL: ");
     if ( (pos != string::npos) && !pos) {
         return true;
     }
     else if (x_SubsHaveTags(*sit)) {
          return true;
     }
  }
  return false;
};

void CDiscRepOutput :: x_WriteDiscRepDetails(vector <CRef < CClickableItem > > disc_rep_dt, xml::node& xml_root, UInt2UInts& prt_ord, bool use_flag, bool IsSubcategory)
{
  COutputConfig& oc = thisInfo.output_config;
  string prefix, desc, strtmp;
  unsigned i, j;
  vector <unsigned> prt_idx;
  if (IsSubcategory) {
    for (i=0; i< disc_rep_dt.size(); i++) {
       prt_idx.push_back(i);
    }
  }
  else {
     ITERATE (UInt2UInts, it, prt_ord) {
       ITERATE (vector <unsigned>, jt, it->second) {
          prt_idx.push_back(*jt);
       }
     }
  }

  string xml_prefix, xml_severity;
  CRef <CClickableItem> c_item;
  bool has_fatal;
  for (j=0; j< prt_idx.size(); j++) {
      has_fatal = false;
      c_item = disc_rep_dt[prt_idx[j]];
      desc = c_item->description;
      if ( desc.empty() ) continue;

      // prefix
      if (use_flag) {
          prefix = "DiscRep_";
          prefix += IsSubcategory ? "SUB:" : "ALL:";
          
          strtmp = c_item->setting_name;
          prefix += strtmp.empty()? " " : strtmp + ": ";
      }
 
      // FATAL tag
      if (x_RmTagInDescp(desc, "FATAL: ")) {
          prefix = "FATAL: " + prefix;
          has_fatal = true;
      }

      if (oc.msg_level == COutputConfig :: e_Fatal && !(c_item->fatal)) continue;

      if ( (oc.add_output_tag || oc.add_extra_output_tag) 
                && (has_fatal || x_SubsHaveTags(c_item)) ) {
            c_item->expanded = true;
      }
      // summary report
      if (oc.summary_report) {
         if (!oc.xml) {
             *(oc.output_f) << prefix << desc << endl;
         }
      }
      else {
         x_WriteDiscRepItems(c_item, prefix, xml_root);
      }
      if ( x_OkToExpand (c_item) ) {
        for (i = 0; i< c_item->subcategories.size() -1; i++) {
              c_item->subcategories[i]->next_sibling = true;
        }
        if (use_flag 
               && c_item->setting_name == "DISC_INCONSISTENT_BIOSRC_DEFLINE") {
             x_WriteDiscRepDetails(c_item->subcategories, xml_root, 
                                    prt_ord, false, false);
        } 
        else {
             x_WriteDiscRepDetails(c_item->subcategories, xml_root,
                                    prt_ord, oc.use_flag, true);
        }
      } 
  }
};   // WriteDiscRepDetails



bool CDiscRepOutput :: x_SuppressItemListForFeatureTypeForOutputFiles(const string& setting_name)
{
  if (setting_name == "DISC_FEATURE_COUNT"
       || setting_name == "DISC_MISSING_SRC_QUAL"
       || setting_name == "DISC_DUP_SRC_QUAL"
       || setting_name == "DISC_DUP_SRC_QUAL_DATA"
       || setting_name == "DISC_SOURCE_QUALS_ASNDISC") {
    return true;
  } 
  else {
     return false;
  }

}; // SuppressItemListForFeatureTypeForOutputFiles



void CDiscRepOutput :: x_WriteDiscRepItems(CRef <CClickableItem> c_item, const string& prefix, xml::node& xml_node)
{
   COutputConfig& oc = thisInfo.output_config;
   string xml_prefix, xml_severity;
   if (oc.use_flag && 
         x_SuppressItemListForFeatureTypeForOutputFiles(c_item->setting_name)){

       if (!prefix.empty()) {
         if (!oc.xml) {
               *(oc.output_f) << prefix;
         }
         xml_prefix = (prefix.find("DiscRep_ALL") != string::npos) ?
                        "DiscRep_ALL" : "DiscRep_SUB";
       }
       xml_severity = c_item->fatal ? "FATAL" : "INFO";
       if (!oc.xml) {
          *(oc.output_f) << c_item->description << endl;
       }
       else {
         xml::node sub_node("message", (c_item->description).c_str());
         sub_node.get_attributes().insert("code", c_item->setting_name.c_str());
         sub_node.get_attributes().insert("prefix", xml_prefix.c_str());
         sub_node.get_attributes().insert("severity", xml_severity.c_str());
         xml_node.push_back(sub_node);
       }
  } 
  else {
     x_StandardWriteDiscRepItems (oc, c_item, prefix, !c_item->expanded, xml_node);
  }
  if ((!c_item->next_sibling && c_item->subcategories.empty()) 
        || (!c_item->subcategories.empty() && !c_item->item_list.empty()
                &&  !c_item->expanded) ) {

      if (!oc.xml) {
         *(oc.output_f) << endl;
      }
 }
  
} // WriteDiscRepItems()


void CDiscRepOutput :: x_StandardWriteDiscRepItems(COutputConfig& oc, CRef <CClickableItem> c_item, const string& prefix, bool list_features_if_subcat, xml::node& xml_node)
{
  string xml_prefix, xml_severity, strtmp;
  if (!prefix.empty()) {
     if (!oc.xml) {
        *(oc.output_f) << prefix;
     }
     if (prefix.find("DiscRep_ALL") != string::npos) {
        xml_prefix = "DiscRep_ALL";
     }
     else xml_prefix = "DiscRep_SUB";
  }
  string desc = c_item->description;
  xml_severity = c_item->fatal ? "FATAL" : "INFO";
  if (!oc.xml) {
      *(oc.output_f) << desc << endl;
  }
  else {
        xml::node sub_node("message", desc.c_str());
        xml::attributes& att = sub_node.get_attributes();
        att.insert("code",(c_item->setting_name.c_str()));
        att.insert("prefix", xml_prefix.c_str());
        att.insert("severity", xml_severity.c_str());
        xml_node.push_back(sub_node);
  }

  if (c_item->subcategories.empty() || list_features_if_subcat) {
    string xmlids(kEmptyStr), feat_id, seq_id, seq_loc, setting_nm_desc;
    size_t pos;
    vector <string> arr;
    setting_nm_desc = m_TestDesc[c_item->setting_name];
    ITERATE (vector <string>, it, c_item->item_list) {
      // remove XmlIds from description;
      if ( (pos = (*it).find(thisInfo.xml_label)) != string::npos ) {
          desc = (*it).substr(0, pos);
          xmlids = (*it).substr(pos + thisInfo.xml_label.size());
          arr.clear();
          arr = NStr::Tokenize(xmlids, ",", arr);
          seq_id = (arr.size() >= 1) ? arr[0] : kEmptyStr;
          feat_id = (arr.size() >= 2) ? arr[1] : kEmptyStr;
          seq_loc = (arr.size() >= 3) ? arr[2] : kEmptyStr;
      } 
      else {
        desc = (*it);
        feat_id = seq_id = seq_loc = kEmptyStr;
      }
      if (!oc.xml) {
          *(oc.output_f) << desc << endl;
      }
      else {
        xml::node sub_node("message", setting_nm_desc.c_str());
        xml::attributes& att = sub_node.get_attributes();
        att.insert("code",(c_item->setting_name.c_str()));
        att.insert("prefix", xml_prefix.c_str());
        att.insert("severity", xml_severity.c_str());
        if (!seq_id.empty())  att.insert("seq-id", seq_id.c_str());
        if (!feat_id.empty()) att.insert("feat-id", feat_id.c_str());
        if (!seq_loc.empty()) att.insert("seq-loc", seq_loc.c_str());
        xml_node.push_back(sub_node);
      }
    }
  }

}; // StandardWriteDiscRepItems()

string CDiscRepOutput :: x_GetDesc4GItem(string desc)
{
   // FATAL tag
   if (x_RmTagInDescp(desc, "FATAL: ")) {
        return ("FATAL: " + desc);
   }
   else {
        return desc;
   }
};

void CDiscRepOutput :: x_OutputRepToGbenchItem(const CClickableItem& c_item,  CClickableText& item) 
{
   if (!c_item.item_list.empty()) {
      vector <string> v_desc = c_item.item_list;
      vector <string> v_acc;
      v_acc.reserve(c_item.item_list.size());
      size_t pos;
      string strtmp;
      vector <string> arr;
      NON_CONST_ITERATE (vector <string>, it, v_desc) {
         if ( (pos = (*it).find(thisInfo.xml_label)) != string::npos) {
            strtmp = (*it).substr(pos + thisInfo.xml_label.size());
            (*it) = (*it).substr(0, pos);
            arr.clear();
            if (strtmp.empty()) {
               v_acc.push_back(kEmptyStr);
            }
            else {
               arr = NStr::Tokenize(strtmp, ",", arr);
               v_acc.push_back(arr[0]);
            }
         }
      }
      item.SetObjdescs().insert(item.SetObjdescs().end(), 
                                  v_desc.begin(), 
                                  v_desc.end());
      if (!v_acc.empty()) {
         item.SetAccessions().insert(item.SetAccessions().end(),
                                     v_acc.begin(), v_acc.end());
      }

      if (!c_item.obj_list.empty()) {
            item.SetObjects().insert(item.SetObjects().end(), 
                                     c_item.obj_list.begin(),
                                     c_item.obj_list.end());
      }

      item.SetSettingName(c_item.setting_name);
      item.SetAutofixFunc(c_item.fix_function);
   }
   if (!c_item.subcategories.empty()) {
      string desc;
      ITERATE (vector < CRef <CClickableItem > >, sit, c_item.subcategories) {
            desc = (*sit)->description;
            if (desc.empty()) continue;
            desc = x_GetDesc4GItem(desc);
            CRef <CClickableText> sub (new CClickableText(desc));             
            x_OutputRepToGbenchItem(**sit, *sub);
            item.SetSubitems().push_back(sub);
      }
   }
   if (c_item.expanded) {
         item.SetSelfExpanded();
   }
};


static const char* sOnCallerToolPriorities[] = {
  "DISC_COUNT_NUCLEOTIDES",
  "DISC_DUP_DEFLINE",
  "DISC_MISSING_DEFLINES",
  "TEST_TAXNAME_NOT_IN_DEFLINE",
  "TEST_HAS_PROJECT_ID",
  "ONCALLER_BIOPROJECT_ID",
  "ONCALLER_DEFLINE_ON_SET",
  "TEST_UNWANTED_SET_WRAPPER",
  "TEST_COUNT_UNVERIFIED",
  "DISC_SRC_QUAL_PROBLEM",
  "DISC_FEATURE_COUNT_oncaller",
  "NO_ANNOTATION",
  "DISC_FEATURE_MOLTYPE_MISMATCH",
  "TEST_ORGANELLE_NOT_GENOMIC",
  "DISC_INCONSISTENT_MOLTYPES",
  "DISC_CHECK_AUTH_CAPS",
  "ONCALLER_CONSORTIUM",
  "DISC_UNPUB_PUB_WITHOUT_TITLE",
  "DISC_TITLE_AUTHOR_CONFLICT",
  "DISC_SUBMITBLOCK_CONFLICT",
  "DISC_CITSUBAFFIL_CONFLICT",
  "DISC_CHECK_AUTH_NAME",
  "DISC_MISSING_AFFIL",
  "DISC_USA_STATE",
  "DISC_CITSUB_AFFIL_DUP_TEXT",
  "DISC_DUP_SRC_QUAL",
  "DISC_MISSING_SRC_QUAL",
  "DISC_DUP_SRC_QUAL_DATA",
  "ONCALLER_DUPLICATE_PRIMER_SET",
  "ONCALLER_MORE_NAMES_COLLECTED_BY",
  "ONCALLER_MORE_OR_SPEC_NAMES_IDENTIFIED_BY",
  "ONCALLER_SUSPECTED_ORG_IDENTIFIED",
  "ONCALLER_SUSPECTED_ORG_COLLECTED",
  "DISC_MISSING_VIRAL_QUALS",
  "DISC_INFLUENZA_DATE_MISMATCH",
  "DISC_HUMAN_HOST",
  "DISC_SPECVOUCHER_TAXNAME_MISMATCH",
  "DISC_BIOMATERIAL_TAXNAME_MISMATCH",
  "DISC_CULTURE_TAXNAME_MISMATCH",
  "DISC_STRAIN_TAXNAME_MISMATCH",
  "DISC_BACTERIA_SHOULD_NOT_HAVE_ISOLATE",
  "DISC_BACTERIA_MISSING_STRAIN",
  "ONCALLER_STRAIN_TAXNAME_CONFLICT",
  "TEST_SP_NOT_UNCULTURED",
  "DISC_REQUIRED_CLONE",
  "TEST_UNNECESSARY_ENVIRONMENTAL",
  "TEST_AMPLIFIED_PRIMERS_NO_ENVIRONMENTAL_SAMPLE",
  "ONCALLER_MULTISRC",
  "ONCALLER_COUNTRY_COLON",
  "END_COLON_IN_COUNTRY",
  "DUP_DISC_ATCC_CULTURE_CONFLICT",
  "DUP_DISC_CBS_CULTURE_CONFLICT",
  "ONCALLER_STRAIN_CULTURE_COLLECTION_MISMATCH",
  "ONCALLER_MULTIPLE_CULTURE_COLLECTION",
  "DISC_TRINOMIAL_SHOULD_HAVE_QUALIFIER",
  "ONCALLER_CHECK_AUTHORITY",
  "DISC_MAP_CHROMOSOME_CONFLICT",
  "DISC_METAGENOMIC",
  "DISC_METAGENOME_SOURCE",
  "DISC_RETROVIRIDAE_DNA",
  "NON_RETROVIRIDAE_PROVIRAL",
  "ONCALLER_HIV_RNA_INCONSISTENT",
  "RNA_PROVIRAL",
  "TEST_BAD_MRNA_QUAL",
  "DISC_MITOCHONDRION_REQUIRED",
  "TEST_UNWANTED_SPACER",
  "TEST_SMALL_GENOME_SET_PROBLEM",
  "ONCALLER_SUPERFLUOUS_GENE",
  "ONCALLER_GENE_MISSING",
  "DISC_GENE_PARTIAL_CONFLICT",
  "DISC_BAD_GENE_STRAND",
  "TEST_UNNECESSARY_VIRUS_GENE",
  "NON_GENE_LOCUS_TAG",
  "DISC_RBS_WITHOUT_GENE",
  "ONCALLER_ORDERED_LOCATION",
  "MULTIPLE_CDS_ON_MRNA",
  "DISC_CDS_WITHOUT_MRNA",
  "DISC_mRNA_ON_WRONG_SEQUENCE_TYPE",
  "DISC_BACTERIA_SHOULD_NOT_HAVE_MRNA",
  "TEST_MRNA_OVERLAPPING_PSEUDO_GENE",
  "TEST_EXON_ON_MRNA",
  "DISC_CDS_HAS_NEW_EXCEPTION",
  "DISC_SHORT_INTRON",
  "DISC_EXON_INTRON_CONFLICT",
  "PSEUDO_MISMATCH",
  "RNA_NO_PRODUCT",
  "FIND_BADLEN_TRNAS",
  "DISC_MICROSATELLITE_REPEAT_TYPE",
  "DISC_SHORT_RRNA",
  "DISC_POSSIBLE_LINKER",
  "DISC_HAPLOTYPE_MISMATCH",
  "DISC_FLATFILE_FIND_ONCALLER",
  "DISC_CDS_PRODUCT_FIND",
  "DISC_SUSPICIOUS_NOTE_TEXT",
  "DISC_CHECK_RNA_PRODUCTS_AND_COMMENTS",
  "DISC_INTERNAL_TRANSCRIBED_SPACER_RRNA",
  "ONCALLER_COMMENT_PRESENT",
  "SUSPECT_PRODUCT_NAMES",
  "UNCULTURED_NOTES_ONCALLER"
};

void CDiscRepOutput :: x_InitializeOnCallerToolGroups()
{
   m_OnCallerToolGroups["DISC_FEATURE_MOLTYPE_MISMATCH"] = eMol;
   m_OnCallerToolGroups["TEST_ORGANELLE_NOT_GENOMIC"] = eMol;
   m_OnCallerToolGroups["DISC_INCONSISTENT_MOLTYPES"] = eMol;

   m_OnCallerToolGroups["DISC_CHECK_AUTH_CAPS"] = eCitSub;
   m_OnCallerToolGroups["ONCALLER_CONSORTIUM"] = eCitSub;
   m_OnCallerToolGroups["DISC_UNPUB_PUB_WITHOUT_TITLE"] = eCitSub;
   m_OnCallerToolGroups["DISC_TITLE_AUTHOR_CONFLICT"] = eCitSub;
   m_OnCallerToolGroups["DISC_SUBMITBLOCK_CONFLICT"] = eCitSub;
   m_OnCallerToolGroups["DISC_CITSUBAFFIL_CONFLICT"] = eCitSub;
   m_OnCallerToolGroups["DISC_CHECK_AUTH_NAME"] = eCitSub;
   m_OnCallerToolGroups["DISC_MISSING_AFFIL"] = eCitSub;
   m_OnCallerToolGroups["DISC_USA_STATE"] = eCitSub;
   m_OnCallerToolGroups["DISC_CITSUB_AFFIL_DUP_TEXT"] = eCitSub;
   
   m_OnCallerToolGroups["DISC_DUP_SRC_QUAL"] = eSource;
   m_OnCallerToolGroups["DISC_MISSING_SRC_QUAL"] = eSource;
   m_OnCallerToolGroups["DISC_DUP_SRC_QUAL_DATA"] = eSource;
   m_OnCallerToolGroups["DISC_MISSING_VIRAL_QUALS"] = eSource;
   m_OnCallerToolGroups["DISC_INFLUENZA_DATE_MISMATCH"] = eSource;
   m_OnCallerToolGroups["DISC_HUMAN_HOST"] = eSource;
   m_OnCallerToolGroups["DISC_SPECVOUCHER_TAXNAME_MISMATCH"] = eSource;
   m_OnCallerToolGroups["DISC_BIOMATERIAL_TAXNAME_MISMATCH"] = eSource;
   m_OnCallerToolGroups["DISC_CULTURE_TAXNAME_MISMATCH"] = eSource;
   m_OnCallerToolGroups["DISC_STRAIN_TAXNAME_MISMATCH"] = eSource;
   m_OnCallerToolGroups["DISC_BACTERIA_SHOULD_NOT_HAVE_ISOLATE"] = eSource;
   m_OnCallerToolGroups["DISC_BACTERIA_MISSING_STRAIN"] = eSource;
   m_OnCallerToolGroups["ONCALLER_STRAIN_TAXNAME_CONFLICT"] = eSource;
   m_OnCallerToolGroups["TEST_SP_NOT_UNCULTURED"] = eSource;
   m_OnCallerToolGroups["DISC_REQUIRED_CLONE"] = eSource;
   m_OnCallerToolGroups["TEST_UNNECESSARY_ENVIRONMENTAL"] = eSource;
   m_OnCallerToolGroups["TEST_AMPLIFIED_PRIMERS_NO_ENVIRONMENTAL_SAMPLE"] = eSource;
   m_OnCallerToolGroups["ONCALLER_MULTISRC"] = eSource;
   m_OnCallerToolGroups["ONCALLER_COUNTRY_COLON"] = eSource;
   m_OnCallerToolGroups["END_COLON_IN_COUNTRY"] = eSource;
   m_OnCallerToolGroups["DUP_DISC_ATCC_CULTURE_CONFLICT"] = eSource;
   m_OnCallerToolGroups["DUP_DISC_CBS_CULTURE_CONFLICT"] = eSource;
   m_OnCallerToolGroups["ONCALLER_STRAIN_CULTURE_COLLECTION_MISMATCH"]=eSource;
   m_OnCallerToolGroups["ONCALLER_MULTIPLE_CULTURE_COLLECTION"] = eSource;
   m_OnCallerToolGroups["DISC_TRINOMIAL_SHOULD_HAVE_QUALIFIER"] = eSource;
   m_OnCallerToolGroups["ONCALLER_CHECK_AUTHORITY"] = eSource;
   m_OnCallerToolGroups["DISC_MAP_CHROMOSOME_CONFLICT"] = eSource;
   m_OnCallerToolGroups["DISC_METAGENOMIC"] = eSource;
   m_OnCallerToolGroups["DISC_METAGENOME_SOURCE"] = eSource;
   m_OnCallerToolGroups["DISC_RETROVIRIDAE_DNA"] = eSource;
   m_OnCallerToolGroups["NON_RETROVIRIDAE_PROVIRAL"] = eSource;
   m_OnCallerToolGroups["ONCALLER_HIV_RNA_INCONSISTENT"] = eSource;
   m_OnCallerToolGroups["RNA_PROVIRAL"] = eSource;
   m_OnCallerToolGroups["TEST_BAD_MRNA_QUAL"] = eSource;
   m_OnCallerToolGroups["DISC_MITOCHONDRION_REQUIRED"] = eSource;
   m_OnCallerToolGroups["TEST_UNWANTED_SPACER"] = eSource;
   m_OnCallerToolGroups["TEST_SMALL_GENOME_SET_PROBLEM"] = eSource;
   m_OnCallerToolGroups["ONCALLER_DUPLICATE_PRIMER_SET"] = eSource;
   m_OnCallerToolGroups["ONCALLER_MORE_NAMES_COLLECTED_BY"] = eSource;
   m_OnCallerToolGroups["ONCALLER_MORE_OR_SPEC_NAMES_IDENTIFIED_BY"] = eSource;
   m_OnCallerToolGroups["ONCALLER_SUSPECTED_ORG_IDENTIFIED"] = eSource;
   m_OnCallerToolGroups["ONCALLER_SUSPECTED_ORG_COLLECTED"] = eSource;

   m_OnCallerToolGroups["ONCALLER_SUPERFLUOUS_GENE"] = eFeature;
   m_OnCallerToolGroups["ONCALLER_GENE_MISSING"] = eFeature;
   m_OnCallerToolGroups["DISC_GENE_PARTIAL_CONFLICT"] = eFeature;
   m_OnCallerToolGroups["DISC_BAD_GENE_STRAND"] = eFeature;
   m_OnCallerToolGroups["TEST_UNNECESSARY_VIRUS_GENE"] = eFeature;
   m_OnCallerToolGroups["NON_GENE_LOCUS_TAG"] = eFeature;
   m_OnCallerToolGroups["DISC_RBS_WITHOUT_GENE"] = eFeature;
   m_OnCallerToolGroups["ONCALLER_ORDERED_LOCATION"] = eFeature;
   m_OnCallerToolGroups["MULTIPLE_CDS_ON_MRNA"] = eFeature;
   m_OnCallerToolGroups["DISC_CDS_WITHOUT_MRNA"] = eFeature;
   m_OnCallerToolGroups["DISC_mRNA_ON_WRONG_SEQUENCE_TYPE"] = eFeature;
   m_OnCallerToolGroups["DISC_BACTERIA_SHOULD_NOT_HAVE_MRNA"] = eFeature;
   m_OnCallerToolGroups["TEST_MRNA_OVERLAPPING_PSEUDO_GENE"] = eFeature;
   m_OnCallerToolGroups["TEST_EXON_ON_MRNA"] = eFeature;
   m_OnCallerToolGroups["DISC_CDS_HAS_NEW_EXCEPTION"] = eFeature;
   m_OnCallerToolGroups["DISC_SHORT_INTRON"] = eFeature;
   m_OnCallerToolGroups["DISC_EXON_INTRON_CONFLICT"] = eFeature;
   m_OnCallerToolGroups["PSEUDO_MISMATCH"] = eFeature;
   m_OnCallerToolGroups["RNA_NO_PRODUCT"] = eFeature;
   m_OnCallerToolGroups["FIND_BADLEN_TRNA"] = eFeature;
   m_OnCallerToolGroups["DISC_MICROSATELLITE_REPEAT_TYPE"] = eFeature;
   m_OnCallerToolGroups["DISC_SHORT_RRNA"] = eFeature;

   m_OnCallerToolGroups["DISC_FLATFILE_FIND_ONCALLER"] = eSuspectText;
   m_OnCallerToolGroups["DISC_FLATFILE_FIND_ONCALLER_FIXABLE"] = eSuspectText;
   m_OnCallerToolGroups["DISC_FLATFILE_FIND_ONCALLER_UNFIXABLE"] =eSuspectText;
   m_OnCallerToolGroups["DISC_CDS_PRODUCT_FIND"] = eSuspectText;
   m_OnCallerToolGroups["DISC_SUSPICIOUS_NOTE_TEXT:"] = eSuspectText;
   m_OnCallerToolGroups["DISC_CHECK_RNA_PRODUCTS_AND_COMMENTS"] = eSuspectText;
   m_OnCallerToolGroups["DISC_INTERNAL_TRANSCRIBED_SPACER_RRNA"] =eSuspectText;
   m_OnCallerToolGroups["ONCALLER_COMMENT_PRESENT"] = eSuspectText;
   m_OnCallerToolGroups["SUSPECT_PRODUCT_NAMES"] = eSuspectText;
   m_OnCallerToolGroups["UNCULTURED_NOTES_ONCALLER"] = eSuspectText;
};

void CDiscRepOutput :: x_OrderResult(UInt2UInts& ord2i_citem)
{
   int i=-1;
   unsigned pri;

   ITERATE (vector <CRef <CClickableItem> >, it, thisInfo.disc_report_data) {
      i++;
      if ((*it)->description.empty()) continue;
      if (m_OnCallerToolPriorities.find((*it)->setting_name)
            != m_OnCallerToolPriorities.end()) {
         pri = m_OnCallerToolPriorities[(*it)->setting_name];
      }
      else {
         pri = m_OnCallerToolPriorities.size() + i + 1;
      }
      ord2i_citem[pri].push_back((unsigned)i);
   }
};

void CDiscRepOutput :: x_GroupResult(map <EOnCallerGrp, string>& grp_idx_str) 
{
   unsigned i=0;
   ITERATE (vector <CRef <CClickableItem> >, it, thisInfo.disc_report_data) {
      if (m_OnCallerToolGroups.find((*it)->setting_name)
            != m_OnCallerToolGroups.end()) {
          EOnCallerGrp e_grp = m_OnCallerToolGroups[(*it)->setting_name];
          if (grp_idx_str.find(e_grp) == grp_idx_str.end()) {
              grp_idx_str[e_grp] = NStr::UIntToString(i);
          }
          else {
              grp_idx_str[e_grp] += ("," + NStr::UIntToString(i));
          }
      }
      i++;
   }
};

void CDiscRepOutput :: x_ReorderAndGroupOnCallerResults(UInt2UInts& ord2i_citem, map <EOnCallerGrp, string>& grp_idx_str)
{
   // x_InitializeOnCallerToolPriorities();
   m_OnCallerToolPriorities.clear();
   m_OnCallerToolGroups.clear();
   for (unsigned i=0; i< ArraySize(sOnCallerToolPriorities); i++) {
     m_OnCallerToolPriorities[sOnCallerToolPriorities[i]] = i;
   }
   x_InitializeOnCallerToolGroups();
   x_OrderResult(ord2i_citem);
   x_GroupResult(grp_idx_str);
};

string CDiscRepOutput :: x_GetGrpName(EOnCallerGrp e_grp)
{
   switch (e_grp) {
      case eMol: return string("Molecule type tests");
      case eCitSub: return string("Cit-sub type tests");
      case eSource: return string("Source tests");
      case eFeature: return string("Feature tests");
      case eSuspectText: return string("Suspect text tests");
      default:
        NCBI_USER_THROW("Unknown Oncaller Tool group type: " + NStr::IntToString((int)e_grp));
   };
   return kEmptyStr;
};

CRef <CClickableItem> CDiscRepOutput :: x_CollectSameGroupToGbench(UInt2UInts& ord2i_citem, EOnCallerGrp e_grp, const string& grp_idxes)
{
    vector <string> arr;
    arr = NStr::Tokenize(grp_idxes, ",", arr);
    UInt2UInts sub_ord2idxes;
    unsigned idx;
    CRef <CClickableItem> new_item (new CClickableItem);
    new_item->setting_name = "DISC_CATEGORY_HEADER";
    new_item->description = x_GetGrpName(e_grp);
    new_item->expanded = true;
    string strtmp;
    ITERATE (vector <string>, it, arr) {
        idx = NStr::StringToUInt(*it);
        strtmp = thisInfo.disc_report_data[idx]->setting_name;
        sub_ord2idxes[m_OnCallerToolPriorities[strtmp]].push_back(idx);
    }
    switch (e_grp) {
       case eMol: 
       case eCitSub:
       case eSource:
       case eFeature:
       {
          ITERATE (UInt2UInts, it, sub_ord2idxes) {
            ITERATE (vector <unsigned>, iit, it->second) {
                new_item->subcategories.push_back(thisInfo.disc_report_data[*iit]);
            }
            ord2i_citem[it->first].clear();
          }  
          break;
       }
       case eSuspectText:
       {
          unsigned typo_cnt = 0;
          ITERATE (UInt2UInts, it, sub_ord2idxes) {
             if (thisInfo.disc_report_data[it->second[0]]->setting_name
                     == "SUSPECT_PRODUCT_NAMES") {
                ITERATE (vector <unsigned>, iit, it->second) {
                  NON_CONST_ITERATE (vector <CRef <CClickableItem> >, 
                                     sit, 
                               thisInfo.disc_report_data[*iit]->subcategories){
                    if ((*sit)->description == "Typo") {
                        ITERATE (vector <CRef <CClickableItem> >, ssit, 
                                    (*sit)->subcategories) {
                           typo_cnt += (*ssit)->item_list.size();
                        }
                        
                       (*sit)->description 
                          = CTestAndRepData::GetContainsComment(typo_cnt,
                                                                "product name")
                             + "typos.";
                       new_item->subcategories.push_back(*sit);
                       break;
                    }
                  }
                }
             }
             else {
                ITERATE (vector <unsigned>, iit, it->second) {
                    new_item->subcategories.push_back(thisInfo.disc_report_data[*iit]);
                }
             }
             ord2i_citem[it->first].clear();
             if (new_item->subcategories.empty()) {
                new_item.Reset(0);
             }
          }
          break;
       }
       default: break;
    }

    return new_item;
};

void CDiscRepOutput :: x_SendItemToGbench(CRef <CClickableItem> citem, vector <CRef <CClickableText> >& item_list)
{
   string desc = citem->description;
   if ( desc.empty()) return;
   desc = x_GetDesc4GItem(desc);
   CRef <CClickableText> item (new CClickableText(desc));
   x_OutputRepToGbenchItem(*citem, *item);
   item_list.push_back(item);
};

// for gbench
void CDiscRepOutput :: Export(vector <CRef <CClickableText> >& item_list)
{
   const COutputConfig& oc = thisInfo.output_config;

   if (!thisInfo.disc_report_data.empty()) {
      if (oc.add_output_tag || oc.add_extra_output_tag) {
         x_AddListOutputTags(); 
      }
      string desc;
      if (thisInfo.report == fOncaller) {
          UInt2UInts ord2i_citem;
          map <EOnCallerGrp, string> grp_idx_str;
          vector <string> arr;
          x_ReorderAndGroupOnCallerResults(ord2i_citem, grp_idx_str);
          NON_CONST_ITERATE (UInt2UInts, it, ord2i_citem) {
             if (it->second.empty()) {
                 continue;
             }
             CRef <CClickableItem> this_item 
                 = thisInfo.disc_report_data[it->second[0]];
             if (m_OnCallerToolGroups.find(this_item->setting_name) 
                    != m_OnCallerToolGroups.end()){
                EOnCallerGrp 
                   e_grp = m_OnCallerToolGroups[this_item->setting_name];
                CRef <CClickableItem> 
                    new_item(x_CollectSameGroupToGbench(ord2i_citem, 
                                                        e_grp,
                                                        grp_idx_str[e_grp]));
                if (new_item.NotEmpty()) {
                     x_SendItemToGbench(new_item, item_list);
                }
             }
             else {
                ITERATE (vector <unsigned>, iit, it->second) {
                   CRef <CClickableItem> this_item 
                       = thisInfo.disc_report_data[*iit];
                   x_SendItemToGbench(this_item, item_list);
                }
             }
          }
      }
      else {
        UInt2UInts prt_ord;
        x_SortReport(prt_ord);
        ITERATE (UInt2UInts, it, prt_ord) {
          ITERATE (vector <unsigned>, jt, it->second) {
              x_SendItemToGbench(thisInfo.disc_report_data[*jt], item_list);
          }
        } 
      }
   };

   x_Clear();
};

// for unit test
void CDiscRepOutput :: Export(vector <CRef <CClickableItem> >& c_item, const string& setting_name)
{
   if (!thisInfo.disc_report_data.empty()) {
     ITERATE ( vector <CRef <CClickableItem> >, it, thisInfo.disc_report_data){
         if ( (*it)->setting_name == setting_name) {
            c_item.push_back((*it));
         }
     }
   }

   x_Clear();
};

// for unit test
void CDiscRepOutput :: Export(CClickableItem& c_item, const string& setting_name)
{
   if (!thisInfo.disc_report_data.empty()) {
      if (thisInfo.disc_report_data.size() == 1) {
          c_item = thisInfo.disc_report_data[0].GetObject();
      }
      else {
         ITERATE ( vector <CRef <CClickableItem> >, it, thisInfo.disc_report_data) {
            if ( (*it)->setting_name == setting_name) {
               c_item = (*it).GetObject();
            }
         }
      }
   }

   x_Clear();
} 

END_NCBI_SCOPE