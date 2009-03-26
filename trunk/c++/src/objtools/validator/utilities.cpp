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
 * Author:  Mati Shomrat
 *
 * File Description:
 *      Implementation of utility classes and functions.
 *
 */
#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbistr.hpp>

#include <serial/enumvalues.hpp>
#include <serial/serialimpl.hpp>

#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqfeat/SeqFeatData.hpp>
#include <objects/seqfeat/Gb_qual.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objects/seqset/Bioseq_set.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/misc/sequence_macros.hpp>
#include <objmgr/bioseq_handle.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/seq_vector.hpp>
#include <objmgr/util/sequence.hpp>

#include <vector>
#include <algorithm>
#include <list>

#include "utilities.hpp"

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
BEGIN_SCOPE(validator)

// =============================================================================
//                           Enumeration of GBQual types
// =============================================================================


CGbqualType::EType CGbqualType::GetType(const string& qual)
{
    EType type;
    try {
        type = static_cast<EType>(GetTypeInfo_enum_EType()->FindValue(qual));
    } catch (runtime_error rt) {
        type = CGbqualType::e_Bad;
    } catch (CSerialException e) {
        type = CGbqualType::e_Bad;
    }

    return type;
}


CGbqualType::EType CGbqualType::GetType(const CGb_qual& qual)
{
    return GetType(qual.GetQual());
}


const string& CGbqualType::GetString(CGbqualType::EType gbqual)
{
    return GetTypeInfo_enum_EType()->FindName(gbqual, true);
}


BEGIN_NAMED_ENUM_IN_INFO("", CGbqualType::, EType, false)
{
    ADD_ENUM_VALUE("bad",               e_Bad);
    ADD_ENUM_VALUE("allele",            e_Allele);
    ADD_ENUM_VALUE("anticodon",         e_Anticodon);
    ADD_ENUM_VALUE("bio_material",      e_Bio_material);
    ADD_ENUM_VALUE("bound_moiety",      e_Bound_moiety);
    ADD_ENUM_VALUE("cell_line",         e_Cell_line);
    ADD_ENUM_VALUE("cell_type",         e_Cell_type);
    ADD_ENUM_VALUE("chromosome",        e_Chromosome);
    ADD_ENUM_VALUE("chloroplast",       e_Chloroplast);
    ADD_ENUM_VALUE("chromoplast",       e_Chromoplast);
    ADD_ENUM_VALUE("citation",          e_Citation);
    ADD_ENUM_VALUE("clone",             e_Clone);
    ADD_ENUM_VALUE("clone_lib",         e_Clone_lib);
    ADD_ENUM_VALUE("codon",             e_Codon);
    ADD_ENUM_VALUE("codon_start",       e_Codon_start);
    ADD_ENUM_VALUE("collected_by",      e_Collected_by);
    ADD_ENUM_VALUE("collection_date",   e_Collection_date);
    ADD_ENUM_VALUE("compare",           e_Compare);
    ADD_ENUM_VALUE("cons_splice",       e_Cons_splice);
    ADD_ENUM_VALUE("country",           e_Country);
    ADD_ENUM_VALUE("cultivar",          e_Cultivar);
    ADD_ENUM_VALUE("culture_collection",          e_Culture_collection);
    ADD_ENUM_VALUE("cyanelle",          e_Cyanelle);
    ADD_ENUM_VALUE("cyt_map",           e_Cyt_map);
    ADD_ENUM_VALUE("db_xref",           e_Db_xref);
    ADD_ENUM_VALUE("dev_stage",         e_Dev_stage);
    ADD_ENUM_VALUE("direction",         e_Direction);
    ADD_ENUM_VALUE("EC_number",         e_EC_number);
    ADD_ENUM_VALUE("ecotype",           e_Ecotype);
    ADD_ENUM_VALUE("environmental_sample",        e_Environmental_sample);
    ADD_ENUM_VALUE("estimated_length",  e_Estimated_length);
    ADD_ENUM_VALUE("evidence",          e_Evidence);
    ADD_ENUM_VALUE("exception",         e_Exception);
    ADD_ENUM_VALUE("experiment",        e_Experiment);
    ADD_ENUM_VALUE("focus",             e_Focus);
    ADD_ENUM_VALUE("frequency",         e_Frequency);
    ADD_ENUM_VALUE("function",          e_Function);
    ADD_ENUM_VALUE("gen_map",           e_Gen_map);
    ADD_ENUM_VALUE("gene",              e_Gene);
    ADD_ENUM_VALUE("gene_synonym",      e_Gene_synonym);
    ADD_ENUM_VALUE("gdb_xref",          e_Gdb_xref);
    ADD_ENUM_VALUE("germline",          e_Germline);
    ADD_ENUM_VALUE("haplotype",         e_Haplotype);
    ADD_ENUM_VALUE("host",              e_Host);
    ADD_ENUM_VALUE("identified_by",     e_Identified_by);
    ADD_ENUM_VALUE("inference",         e_Inference);
    ADD_ENUM_VALUE("insertion_seq",     e_Insertion_seq);
    ADD_ENUM_VALUE("isolate",           e_Isolate);
    ADD_ENUM_VALUE("isolation_source",  e_Isolation_source);
    ADD_ENUM_VALUE("kinetoplast",       e_Kinetoplast);
    ADD_ENUM_VALUE("lat_lon",           e_Lat_lon);
    ADD_ENUM_VALUE("label",             e_Label);
    ADD_ENUM_VALUE("lab_host",          e_Lab_host);
    ADD_ENUM_VALUE("locus_tag",         e_Locus_tag);
    ADD_ENUM_VALUE("macronuclear",      e_Macronuclear);
    ADD_ENUM_VALUE("map",               e_Map);
    ADD_ENUM_VALUE("mating_type",       e_Mating_type);
    ADD_ENUM_VALUE("metagenomic",       e_Metagenomic);
    ADD_ENUM_VALUE("mitochondrion",     e_Mitochondrion);
    ADD_ENUM_VALUE("mobile_element",    e_Mobile_element);
    ADD_ENUM_VALUE("mod_base",          e_Mod_base);
    ADD_ENUM_VALUE("mol_type",          e_Mol_type);
    ADD_ENUM_VALUE("ncRNA_class",       e_NcRNA_class);
    ADD_ENUM_VALUE("note",              e_Note);
    ADD_ENUM_VALUE("number",            e_Number);
    ADD_ENUM_VALUE("old_locus_tag",     e_Old_locus_tag);
    ADD_ENUM_VALUE("operon",            e_Operon);
    ADD_ENUM_VALUE("organelle",         e_Organelle);
    ADD_ENUM_VALUE("organism",          e_Organism);
    ADD_ENUM_VALUE("partial",           e_Partial);
    ADD_ENUM_VALUE("PCR_conditions",    e_PCR_conditions);
    ADD_ENUM_VALUE("PCR_primers",       e_PCR_primers);
    ADD_ENUM_VALUE("phenotype",         e_Phenotype);
    ADD_ENUM_VALUE("pop_variant",       e_Pop_variant);
    ADD_ENUM_VALUE("plasmid",           e_Plasmid);
    ADD_ENUM_VALUE("product",           e_Product);
    ADD_ENUM_VALUE("proviral",          e_Proviral);
    ADD_ENUM_VALUE("pseudo",            e_Pseudo);
    ADD_ENUM_VALUE("rad_map",           e_Rad_map);
    ADD_ENUM_VALUE("rearranged",        e_Rearranged);
    ADD_ENUM_VALUE("replace",           e_Replace);
    ADD_ENUM_VALUE("ribosomal_slippage",        e_Ribosomal_slippage);
    ADD_ENUM_VALUE("rpt_family",        e_Rpt_family);
    ADD_ENUM_VALUE("rpt_type",          e_Rpt_type);
    ADD_ENUM_VALUE("rpt_unit",          e_Rpt_unit);
    ADD_ENUM_VALUE("rpt_unit_range",    e_Rpt_unit_range);
    ADD_ENUM_VALUE("rpt_unit_seq",      e_Rpt_unit_seq);
    ADD_ENUM_VALUE("satellite",         e_Satellite);
    ADD_ENUM_VALUE("segment",           e_Segment);
    ADD_ENUM_VALUE("sequenced_mol",     e_Sequenced_mol);
    ADD_ENUM_VALUE("serotype",          e_Serotype);
    ADD_ENUM_VALUE("serovar",           e_Serovar);
    ADD_ENUM_VALUE("sex",               e_Sex);
    ADD_ENUM_VALUE("site",              e_Site);
    ADD_ENUM_VALUE("site_type",         e_Site_type);
    ADD_ENUM_VALUE("standard_name",     e_Standard_name);
    ADD_ENUM_VALUE("strain",            e_Strain);
    ADD_ENUM_VALUE("sub_clone",         e_Sub_clone);
    ADD_ENUM_VALUE("sub_species",       e_Sub_species);
    ADD_ENUM_VALUE("sub_strain",        e_Sub_strain);
    ADD_ENUM_VALUE("tag_peptide",       e_Tag_peptide);
    ADD_ENUM_VALUE("tissue_lib",        e_Tissue_lib);
    ADD_ENUM_VALUE("tissue_type",       e_Tissue_type);
    ADD_ENUM_VALUE("transcript_id",     e_Transcript_id);
    ADD_ENUM_VALUE("transgenic",        e_Transgenic);
    ADD_ENUM_VALUE("transl_except",     e_Transl_except);
    ADD_ENUM_VALUE("transl_table",      e_Transl_table);
    ADD_ENUM_VALUE("translation",       e_Translation);
    ADD_ENUM_VALUE("transposon",        e_Transposon);
    ADD_ENUM_VALUE("trans_splicing",    e_Trans_splicing);
    ADD_ENUM_VALUE("usedin",            e_Usedin);
    ADD_ENUM_VALUE("variety",           e_Variety);
    ADD_ENUM_VALUE("virion",            e_Virion);
    ADD_ENUM_VALUE("specimen_voucher",  e_Specimen_voucher);
    ADD_ENUM_VALUE("protein_id",        e_Protein_id);
}
END_ENUM_INFO


// =============================================================================
//                        Associating Features and GBQuals
// =============================================================================


bool CFeatQualAssoc::IsLegalGbqual
(CSeqFeatData::ESubtype ftype,
 CGbqualType::EType gbqual) 
{
    return Instance()->IsLegal(ftype, gbqual);
}


const CFeatQualAssoc::TGBQualTypeVec& CFeatQualAssoc::GetMandatoryGbquals
(CSeqFeatData::ESubtype ftype)
{
    return Instance()->GetMandatoryQuals(ftype);
}


auto_ptr<CFeatQualAssoc> CFeatQualAssoc::sm_Instance;

bool CFeatQualAssoc::IsLegal
(CSeqFeatData::ESubtype ftype, 
 CGbqualType::EType gbqual)
{
    if ( m_LegalGbquals.find(ftype) != m_LegalGbquals.end() ) {
        if ( find( m_LegalGbquals[ftype].begin(), 
                   m_LegalGbquals[ftype].end(), 
                   gbqual) != m_LegalGbquals[ftype].end() ) {
            return true;
        }
    }
    return false;
}


const CFeatQualAssoc::TGBQualTypeVec& CFeatQualAssoc::GetMandatoryQuals
(CSeqFeatData::ESubtype ftype)
{
    static TGBQualTypeVec empty;

    if ( m_MandatoryGbquals.find(ftype) != m_MandatoryGbquals.end() ) {
        return m_MandatoryGbquals[ftype];
    }

    return empty;
}


CFeatQualAssoc* CFeatQualAssoc::Instance(void)
{
    if ( !sm_Instance.get() ) {
        sm_Instance.reset(new CFeatQualAssoc);
    }
    return sm_Instance.get();
}


CFeatQualAssoc::CFeatQualAssoc(void)
{
    PoplulateLegalGbquals();
    PopulateMandatoryGbquals();
}


void CFeatQualAssoc::Associate
(CSeqFeatData::ESubtype feat_subtype,
 CGbqualType::EType gbqual_type)
{
    // IMPORTANT: Bug in GCC compile prevent the code to be written as:
    //    m_LegalGbqual[feat_subtype].push_back(gbqual_type);
    TGBQualTypeVec& vec = m_LegalGbquals[feat_subtype];
    vec.push_back(gbqual_type);
}


void CFeatQualAssoc::PoplulateLegalGbquals(void)
{

    //allele
    Associate( CSeqFeatData::eSubtype_allele, CGbqualType::e_Citation );
    Associate( CSeqFeatData::eSubtype_allele, CGbqualType::e_Db_xref );
    Associate( CSeqFeatData::eSubtype_allele, CGbqualType::e_Evidence );
    Associate( CSeqFeatData::eSubtype_allele, CGbqualType::e_Experiment );
    Associate( CSeqFeatData::eSubtype_allele, CGbqualType::e_Frequency );
    Associate( CSeqFeatData::eSubtype_allele, CGbqualType::e_Gene );
    Associate( CSeqFeatData::eSubtype_allele, CGbqualType::e_Gene_synonym );
    Associate( CSeqFeatData::eSubtype_allele, CGbqualType::e_Inference );
    Associate( CSeqFeatData::eSubtype_allele, CGbqualType::e_Label );
    Associate( CSeqFeatData::eSubtype_allele, CGbqualType::e_Locus_tag );
    Associate( CSeqFeatData::eSubtype_allele, CGbqualType::e_Map );
    Associate( CSeqFeatData::eSubtype_allele, CGbqualType::e_Note );
    Associate( CSeqFeatData::eSubtype_allele, CGbqualType::e_Old_locus_tag );
    Associate( CSeqFeatData::eSubtype_allele, CGbqualType::e_Partial );
    Associate( CSeqFeatData::eSubtype_allele, CGbqualType::e_Phenotype );
    Associate( CSeqFeatData::eSubtype_allele, CGbqualType::e_Product );
    Associate( CSeqFeatData::eSubtype_allele, CGbqualType::e_Replace );
    Associate( CSeqFeatData::eSubtype_allele, CGbqualType::e_Standard_name );
    Associate( CSeqFeatData::eSubtype_allele, CGbqualType::e_Usedin );

    //attenuator
    Associate( CSeqFeatData::eSubtype_attenuator, CGbqualType::e_Allele );
    Associate( CSeqFeatData::eSubtype_attenuator, CGbqualType::e_Citation );
    Associate( CSeqFeatData::eSubtype_attenuator, CGbqualType::e_Db_xref );
    Associate( CSeqFeatData::eSubtype_attenuator, CGbqualType::e_Evidence );
    Associate( CSeqFeatData::eSubtype_attenuator, CGbqualType::e_Experiment );
    Associate( CSeqFeatData::eSubtype_attenuator, CGbqualType::e_Gene );
    Associate( CSeqFeatData::eSubtype_attenuator, CGbqualType::e_Gene_synonym );
    Associate( CSeqFeatData::eSubtype_attenuator, CGbqualType::e_Inference );
    Associate( CSeqFeatData::eSubtype_attenuator, CGbqualType::e_Label );
    Associate( CSeqFeatData::eSubtype_attenuator, CGbqualType::e_Locus_tag );
    Associate( CSeqFeatData::eSubtype_attenuator, CGbqualType::e_Map );
    Associate( CSeqFeatData::eSubtype_attenuator, CGbqualType::e_Note );
    Associate( CSeqFeatData::eSubtype_attenuator, CGbqualType::e_Old_locus_tag );
    Associate( CSeqFeatData::eSubtype_attenuator, CGbqualType::e_Operon );
    Associate( CSeqFeatData::eSubtype_attenuator, CGbqualType::e_Partial );
    Associate( CSeqFeatData::eSubtype_attenuator, CGbqualType::e_Phenotype );
    Associate( CSeqFeatData::eSubtype_attenuator, CGbqualType::e_Usedin );

    //C_region
    Associate( CSeqFeatData::eSubtype_C_region, CGbqualType::e_Allele );
    Associate( CSeqFeatData::eSubtype_C_region, CGbqualType::e_Citation );
    Associate( CSeqFeatData::eSubtype_C_region, CGbqualType::e_Db_xref );
    Associate( CSeqFeatData::eSubtype_C_region, CGbqualType::e_Evidence );
    Associate( CSeqFeatData::eSubtype_C_region, CGbqualType::e_Experiment );
    Associate( CSeqFeatData::eSubtype_C_region, CGbqualType::e_Gene );
    Associate( CSeqFeatData::eSubtype_C_region, CGbqualType::e_Gene_synonym );
    Associate( CSeqFeatData::eSubtype_C_region, CGbqualType::e_Inference );
    Associate( CSeqFeatData::eSubtype_C_region, CGbqualType::e_Label );
    Associate( CSeqFeatData::eSubtype_C_region, CGbqualType::e_Locus_tag );
    Associate( CSeqFeatData::eSubtype_C_region, CGbqualType::e_Map );
    Associate( CSeqFeatData::eSubtype_C_region, CGbqualType::e_Note );
    Associate( CSeqFeatData::eSubtype_C_region, CGbqualType::e_Old_locus_tag );
    Associate( CSeqFeatData::eSubtype_C_region, CGbqualType::e_Partial );
    Associate( CSeqFeatData::eSubtype_C_region, CGbqualType::e_Product );
    Associate( CSeqFeatData::eSubtype_C_region, CGbqualType::e_Pseudo );
    Associate( CSeqFeatData::eSubtype_C_region, CGbqualType::e_Standard_name );
    Associate( CSeqFeatData::eSubtype_C_region, CGbqualType::e_Usedin );

    //CAAT_signal
    Associate( CSeqFeatData::eSubtype_CAAT_signal, CGbqualType::e_Allele );
    Associate( CSeqFeatData::eSubtype_CAAT_signal, CGbqualType::e_Citation );
    Associate( CSeqFeatData::eSubtype_CAAT_signal, CGbqualType::e_Db_xref );
    Associate( CSeqFeatData::eSubtype_CAAT_signal, CGbqualType::e_Evidence );
    Associate( CSeqFeatData::eSubtype_CAAT_signal, CGbqualType::e_Experiment );
    Associate( CSeqFeatData::eSubtype_CAAT_signal, CGbqualType::e_Gene );
    Associate( CSeqFeatData::eSubtype_CAAT_signal, CGbqualType::e_Gene_synonym );
    Associate( CSeqFeatData::eSubtype_CAAT_signal, CGbqualType::e_Inference );
    Associate( CSeqFeatData::eSubtype_CAAT_signal, CGbqualType::e_Label );
    Associate( CSeqFeatData::eSubtype_CAAT_signal, CGbqualType::e_Locus_tag );
    Associate( CSeqFeatData::eSubtype_CAAT_signal, CGbqualType::e_Map );
    Associate( CSeqFeatData::eSubtype_CAAT_signal, CGbqualType::e_Note );
    Associate( CSeqFeatData::eSubtype_CAAT_signal, CGbqualType::e_Old_locus_tag );
    Associate( CSeqFeatData::eSubtype_CAAT_signal, CGbqualType::e_Partial );
    Associate( CSeqFeatData::eSubtype_CAAT_signal, CGbqualType::e_Usedin );

    //cdregion
    Associate( CSeqFeatData::eSubtype_cdregion, CGbqualType::e_Allele );
    Associate( CSeqFeatData::eSubtype_cdregion, CGbqualType::e_Citation );
    Associate( CSeqFeatData::eSubtype_cdregion, CGbqualType::e_Codon_start );
    Associate( CSeqFeatData::eSubtype_cdregion, CGbqualType::e_Codon );
    Associate( CSeqFeatData::eSubtype_cdregion, CGbqualType::e_Db_xref );
    Associate( CSeqFeatData::eSubtype_cdregion, CGbqualType::e_EC_number );
    Associate( CSeqFeatData::eSubtype_cdregion, CGbqualType::e_Evidence );
    Associate( CSeqFeatData::eSubtype_cdregion, CGbqualType::e_Exception );
    Associate( CSeqFeatData::eSubtype_cdregion, CGbqualType::e_Experiment );
    Associate( CSeqFeatData::eSubtype_cdregion, CGbqualType::e_Function );
    Associate( CSeqFeatData::eSubtype_cdregion, CGbqualType::e_Gdb_xref );
    Associate( CSeqFeatData::eSubtype_cdregion, CGbqualType::e_Gene );
    Associate( CSeqFeatData::eSubtype_cdregion, CGbqualType::e_Gene_synonym );
    Associate( CSeqFeatData::eSubtype_cdregion, CGbqualType::e_Inference );
    Associate( CSeqFeatData::eSubtype_cdregion, CGbqualType::e_Label );
    Associate( CSeqFeatData::eSubtype_cdregion, CGbqualType::e_Locus_tag );
    Associate( CSeqFeatData::eSubtype_cdregion, CGbqualType::e_Map );
    Associate( CSeqFeatData::eSubtype_cdregion, CGbqualType::e_Note );
    Associate( CSeqFeatData::eSubtype_cdregion, CGbqualType::e_Number );
    Associate( CSeqFeatData::eSubtype_cdregion, CGbqualType::e_Old_locus_tag );
    Associate( CSeqFeatData::eSubtype_cdregion, CGbqualType::e_Operon );
    Associate( CSeqFeatData::eSubtype_cdregion, CGbqualType::e_Partial );
    Associate( CSeqFeatData::eSubtype_cdregion, CGbqualType::e_Product );
    Associate( CSeqFeatData::eSubtype_cdregion, CGbqualType::e_Protein_id );
    Associate( CSeqFeatData::eSubtype_cdregion, CGbqualType::e_Pseudo );
    Associate( CSeqFeatData::eSubtype_cdregion, CGbqualType::e_Ribosomal_slippage );
    Associate( CSeqFeatData::eSubtype_cdregion, CGbqualType::e_Standard_name );
    Associate( CSeqFeatData::eSubtype_cdregion, CGbqualType::e_Trans_splicing );
    Associate( CSeqFeatData::eSubtype_cdregion, CGbqualType::e_Transl_except );
    Associate( CSeqFeatData::eSubtype_cdregion, CGbqualType::e_Transl_table );
    Associate( CSeqFeatData::eSubtype_cdregion, CGbqualType::e_Translation );
    Associate( CSeqFeatData::eSubtype_cdregion, CGbqualType::e_Usedin );

    //conflict
    Associate( CSeqFeatData::eSubtype_conflict, CGbqualType::e_Citation );
    Associate( CSeqFeatData::eSubtype_conflict, CGbqualType::e_Allele );
    Associate( CSeqFeatData::eSubtype_conflict, CGbqualType::e_Compare );
    Associate( CSeqFeatData::eSubtype_conflict, CGbqualType::e_Db_xref );
    Associate( CSeqFeatData::eSubtype_conflict, CGbqualType::e_Evidence );
    Associate( CSeqFeatData::eSubtype_conflict, CGbqualType::e_Experiment );
    Associate( CSeqFeatData::eSubtype_conflict, CGbqualType::e_Gene );
    Associate( CSeqFeatData::eSubtype_conflict, CGbqualType::e_Gene_synonym );
    Associate( CSeqFeatData::eSubtype_conflict, CGbqualType::e_Inference );
    Associate( CSeqFeatData::eSubtype_conflict, CGbqualType::e_Locus_tag );
    Associate( CSeqFeatData::eSubtype_conflict, CGbqualType::e_Map );
    Associate( CSeqFeatData::eSubtype_conflict, CGbqualType::e_Note );
    Associate( CSeqFeatData::eSubtype_conflict, CGbqualType::e_Old_locus_tag );
    Associate( CSeqFeatData::eSubtype_conflict, CGbqualType::e_Replace );
    Associate( CSeqFeatData::eSubtype_conflict, CGbqualType::e_Usedin );

    //D_loop
    Associate( CSeqFeatData::eSubtype_D_loop, CGbqualType::e_Allele );
    Associate( CSeqFeatData::eSubtype_D_loop, CGbqualType::e_Citation );
    Associate( CSeqFeatData::eSubtype_D_loop, CGbqualType::e_Db_xref );
    Associate( CSeqFeatData::eSubtype_D_loop, CGbqualType::e_Evidence );
    Associate( CSeqFeatData::eSubtype_D_loop, CGbqualType::e_Experiment );
    Associate( CSeqFeatData::eSubtype_D_loop, CGbqualType::e_Gene );
    Associate( CSeqFeatData::eSubtype_D_loop, CGbqualType::e_Gene_synonym );
    Associate( CSeqFeatData::eSubtype_D_loop, CGbqualType::e_Inference );
    Associate( CSeqFeatData::eSubtype_D_loop, CGbqualType::e_Label );
    Associate( CSeqFeatData::eSubtype_D_loop, CGbqualType::e_Locus_tag );
    Associate( CSeqFeatData::eSubtype_D_loop, CGbqualType::e_Map );
    Associate( CSeqFeatData::eSubtype_D_loop, CGbqualType::e_Note );
    Associate( CSeqFeatData::eSubtype_D_loop, CGbqualType::e_Old_locus_tag );
    Associate( CSeqFeatData::eSubtype_D_loop, CGbqualType::e_Partial );
    Associate( CSeqFeatData::eSubtype_D_loop, CGbqualType::e_Usedin );

    //D_segment
    Associate( CSeqFeatData::eSubtype_D_segment, CGbqualType::e_Allele );
    Associate( CSeqFeatData::eSubtype_D_segment, CGbqualType::e_Citation );
    Associate( CSeqFeatData::eSubtype_D_segment, CGbqualType::e_Db_xref );
    Associate( CSeqFeatData::eSubtype_D_segment, CGbqualType::e_Evidence );
    Associate( CSeqFeatData::eSubtype_D_segment, CGbqualType::e_Experiment );
    Associate( CSeqFeatData::eSubtype_D_segment, CGbqualType::e_Gene );
    Associate( CSeqFeatData::eSubtype_D_segment, CGbqualType::e_Gene_synonym );
    Associate( CSeqFeatData::eSubtype_D_segment, CGbqualType::e_Inference );
    Associate( CSeqFeatData::eSubtype_D_segment, CGbqualType::e_Label );
    Associate( CSeqFeatData::eSubtype_D_segment, CGbqualType::e_Locus_tag );
    Associate( CSeqFeatData::eSubtype_D_segment, CGbqualType::e_Map );
    Associate( CSeqFeatData::eSubtype_D_segment, CGbqualType::e_Note );
    Associate( CSeqFeatData::eSubtype_D_segment, CGbqualType::e_Old_locus_tag );
    Associate( CSeqFeatData::eSubtype_D_segment, CGbqualType::e_Partial );
    Associate( CSeqFeatData::eSubtype_D_segment, CGbqualType::e_Product );
    Associate( CSeqFeatData::eSubtype_D_segment, CGbqualType::e_Pseudo );
    Associate( CSeqFeatData::eSubtype_D_segment, CGbqualType::e_Standard_name );
    Associate( CSeqFeatData::eSubtype_D_segment, CGbqualType::e_Usedin );

    //enhancer
    Associate( CSeqFeatData::eSubtype_enhancer, CGbqualType::e_Allele );
    Associate( CSeqFeatData::eSubtype_enhancer, CGbqualType::e_Bound_moiety );
    Associate( CSeqFeatData::eSubtype_enhancer, CGbqualType::e_Citation );
    Associate( CSeqFeatData::eSubtype_enhancer, CGbqualType::e_Db_xref );
    Associate( CSeqFeatData::eSubtype_enhancer, CGbqualType::e_Evidence );
    Associate( CSeqFeatData::eSubtype_enhancer, CGbqualType::e_Experiment );
    Associate( CSeqFeatData::eSubtype_enhancer, CGbqualType::e_Gene );
    Associate( CSeqFeatData::eSubtype_enhancer, CGbqualType::e_Gene_synonym );
    Associate( CSeqFeatData::eSubtype_enhancer, CGbqualType::e_Inference );
    Associate( CSeqFeatData::eSubtype_enhancer, CGbqualType::e_Label );
    Associate( CSeqFeatData::eSubtype_enhancer, CGbqualType::e_Locus_tag );
    Associate( CSeqFeatData::eSubtype_enhancer, CGbqualType::e_Map );
    Associate( CSeqFeatData::eSubtype_enhancer, CGbqualType::e_Note );
    Associate( CSeqFeatData::eSubtype_enhancer, CGbqualType::e_Old_locus_tag );
    Associate( CSeqFeatData::eSubtype_enhancer, CGbqualType::e_Partial );
    Associate( CSeqFeatData::eSubtype_enhancer, CGbqualType::e_Standard_name );
    Associate( CSeqFeatData::eSubtype_enhancer, CGbqualType::e_Usedin );

    //exon
    Associate( CSeqFeatData::eSubtype_exon, CGbqualType::e_Allele );
    Associate( CSeqFeatData::eSubtype_exon, CGbqualType::e_Citation );
    Associate( CSeqFeatData::eSubtype_exon, CGbqualType::e_Db_xref );
    Associate( CSeqFeatData::eSubtype_exon, CGbqualType::e_EC_number );
    Associate( CSeqFeatData::eSubtype_exon, CGbqualType::e_Evidence );
    Associate( CSeqFeatData::eSubtype_exon, CGbqualType::e_Experiment );
    Associate( CSeqFeatData::eSubtype_exon, CGbqualType::e_Function );
    Associate( CSeqFeatData::eSubtype_exon, CGbqualType::e_Gene );
    Associate( CSeqFeatData::eSubtype_exon, CGbqualType::e_Gene_synonym );
    Associate( CSeqFeatData::eSubtype_exon, CGbqualType::e_Inference );
    Associate( CSeqFeatData::eSubtype_exon, CGbqualType::e_Label );
    Associate( CSeqFeatData::eSubtype_exon, CGbqualType::e_Locus_tag );
    Associate( CSeqFeatData::eSubtype_exon, CGbqualType::e_Map );
    Associate( CSeqFeatData::eSubtype_exon, CGbqualType::e_Note );
    Associate( CSeqFeatData::eSubtype_exon, CGbqualType::e_Number );
    Associate( CSeqFeatData::eSubtype_exon, CGbqualType::e_Old_locus_tag );
    Associate( CSeqFeatData::eSubtype_exon, CGbqualType::e_Partial );
    Associate( CSeqFeatData::eSubtype_exon, CGbqualType::e_Product );
    Associate( CSeqFeatData::eSubtype_exon, CGbqualType::e_Pseudo );
    Associate( CSeqFeatData::eSubtype_exon, CGbqualType::e_Standard_name );
    Associate( CSeqFeatData::eSubtype_exon, CGbqualType::e_Usedin );

    //gap
    Associate( CSeqFeatData::eSubtype_gap, CGbqualType::e_Estimated_length );
    Associate( CSeqFeatData::eSubtype_gap, CGbqualType::e_Evidence );
    Associate( CSeqFeatData::eSubtype_gap, CGbqualType::e_Experiment );
    Associate( CSeqFeatData::eSubtype_gap, CGbqualType::e_Inference );
    Associate( CSeqFeatData::eSubtype_gap, CGbqualType::e_Map );
    Associate( CSeqFeatData::eSubtype_gap, CGbqualType::e_Note );

    //GC_signal
    Associate( CSeqFeatData::eSubtype_GC_signal, CGbqualType::e_Allele );
    Associate( CSeqFeatData::eSubtype_GC_signal, CGbqualType::e_Citation );
    Associate( CSeqFeatData::eSubtype_GC_signal, CGbqualType::e_Db_xref );
    Associate( CSeqFeatData::eSubtype_GC_signal, CGbqualType::e_Evidence );
    Associate( CSeqFeatData::eSubtype_GC_signal, CGbqualType::e_Experiment );
    Associate( CSeqFeatData::eSubtype_GC_signal, CGbqualType::e_Gene );
    Associate( CSeqFeatData::eSubtype_GC_signal, CGbqualType::e_Gene_synonym );
    Associate( CSeqFeatData::eSubtype_GC_signal, CGbqualType::e_Inference );
    Associate( CSeqFeatData::eSubtype_GC_signal, CGbqualType::e_Label );
    Associate( CSeqFeatData::eSubtype_GC_signal, CGbqualType::e_Locus_tag );
    Associate( CSeqFeatData::eSubtype_GC_signal, CGbqualType::e_Map );
    Associate( CSeqFeatData::eSubtype_GC_signal, CGbqualType::e_Note );
    Associate( CSeqFeatData::eSubtype_GC_signal, CGbqualType::e_Old_locus_tag );
    Associate( CSeqFeatData::eSubtype_GC_signal, CGbqualType::e_Partial );
    Associate( CSeqFeatData::eSubtype_GC_signal, CGbqualType::e_Usedin );

    //gene
    Associate( CSeqFeatData::eSubtype_gene, CGbqualType::e_Allele );
    Associate( CSeqFeatData::eSubtype_gene, CGbqualType::e_Citation );
    Associate( CSeqFeatData::eSubtype_gene, CGbqualType::e_Db_xref );
    Associate( CSeqFeatData::eSubtype_gene, CGbqualType::e_Evidence );
    Associate( CSeqFeatData::eSubtype_gene, CGbqualType::e_Experiment );
    Associate( CSeqFeatData::eSubtype_gene, CGbqualType::e_Function );
    Associate( CSeqFeatData::eSubtype_gene, CGbqualType::e_Gene );
    Associate( CSeqFeatData::eSubtype_gene, CGbqualType::e_Gene_synonym );
    Associate( CSeqFeatData::eSubtype_gene, CGbqualType::e_Inference );
    Associate( CSeqFeatData::eSubtype_gene, CGbqualType::e_Label );
    Associate( CSeqFeatData::eSubtype_gene, CGbqualType::e_Locus_tag );
    Associate( CSeqFeatData::eSubtype_gene, CGbqualType::e_Map );
    Associate( CSeqFeatData::eSubtype_gene, CGbqualType::e_Note );
    Associate( CSeqFeatData::eSubtype_gene, CGbqualType::e_Old_locus_tag );
    Associate( CSeqFeatData::eSubtype_gene, CGbqualType::e_Operon );
    Associate( CSeqFeatData::eSubtype_gene, CGbqualType::e_Partial );
    Associate( CSeqFeatData::eSubtype_gene, CGbqualType::e_Phenotype );
    Associate( CSeqFeatData::eSubtype_gene, CGbqualType::e_Product );
    Associate( CSeqFeatData::eSubtype_gene, CGbqualType::e_Pseudo );
    Associate( CSeqFeatData::eSubtype_gene, CGbqualType::e_Trans_splicing );
    Associate( CSeqFeatData::eSubtype_gene, CGbqualType::e_Usedin );

    //iDNA
    Associate( CSeqFeatData::eSubtype_iDNA, CGbqualType::e_Allele );
    Associate( CSeqFeatData::eSubtype_iDNA, CGbqualType::e_Citation );
    Associate( CSeqFeatData::eSubtype_iDNA, CGbqualType::e_Db_xref );
    Associate( CSeqFeatData::eSubtype_iDNA, CGbqualType::e_Evidence );
    Associate( CSeqFeatData::eSubtype_iDNA, CGbqualType::e_Experiment );
    Associate( CSeqFeatData::eSubtype_iDNA, CGbqualType::e_Function );
    Associate( CSeqFeatData::eSubtype_iDNA, CGbqualType::e_Gene );
    Associate( CSeqFeatData::eSubtype_iDNA, CGbqualType::e_Gene_synonym );
    Associate( CSeqFeatData::eSubtype_iDNA, CGbqualType::e_Inference );
    Associate( CSeqFeatData::eSubtype_iDNA, CGbqualType::e_Label );
    Associate( CSeqFeatData::eSubtype_iDNA, CGbqualType::e_Locus_tag );
    Associate( CSeqFeatData::eSubtype_iDNA, CGbqualType::e_Map );
    Associate( CSeqFeatData::eSubtype_iDNA, CGbqualType::e_Note );
    Associate( CSeqFeatData::eSubtype_iDNA, CGbqualType::e_Number );
    Associate( CSeqFeatData::eSubtype_iDNA, CGbqualType::e_Old_locus_tag );
    Associate( CSeqFeatData::eSubtype_iDNA, CGbqualType::e_Partial );
    Associate( CSeqFeatData::eSubtype_iDNA, CGbqualType::e_Standard_name );
    Associate( CSeqFeatData::eSubtype_iDNA, CGbqualType::e_Usedin );

    //intron
    Associate( CSeqFeatData::eSubtype_intron, CGbqualType::e_Allele );
    Associate( CSeqFeatData::eSubtype_intron, CGbqualType::e_Citation );
    Associate( CSeqFeatData::eSubtype_intron, CGbqualType::e_Cons_splice );
    Associate( CSeqFeatData::eSubtype_intron, CGbqualType::e_Db_xref );
    Associate( CSeqFeatData::eSubtype_intron, CGbqualType::e_Evidence );
    Associate( CSeqFeatData::eSubtype_intron, CGbqualType::e_Experiment );
    Associate( CSeqFeatData::eSubtype_intron, CGbqualType::e_Function );
    Associate( CSeqFeatData::eSubtype_intron, CGbqualType::e_Gene );
    Associate( CSeqFeatData::eSubtype_intron, CGbqualType::e_Gene_synonym );
    Associate( CSeqFeatData::eSubtype_intron, CGbqualType::e_Inference );
    Associate( CSeqFeatData::eSubtype_intron, CGbqualType::e_Label );
    Associate( CSeqFeatData::eSubtype_intron, CGbqualType::e_Locus_tag );
    Associate( CSeqFeatData::eSubtype_intron, CGbqualType::e_Map );
    Associate( CSeqFeatData::eSubtype_intron, CGbqualType::e_Note );
    Associate( CSeqFeatData::eSubtype_intron, CGbqualType::e_Number );
    Associate( CSeqFeatData::eSubtype_intron, CGbqualType::e_Old_locus_tag );
    Associate( CSeqFeatData::eSubtype_intron, CGbqualType::e_Partial );
    Associate( CSeqFeatData::eSubtype_intron, CGbqualType::e_Pseudo );
    Associate( CSeqFeatData::eSubtype_intron, CGbqualType::e_Standard_name );
    Associate( CSeqFeatData::eSubtype_intron, CGbqualType::e_Usedin );

    //J_segment
    Associate( CSeqFeatData::eSubtype_J_segment, CGbqualType::e_Allele );
    Associate( CSeqFeatData::eSubtype_J_segment, CGbqualType::e_Citation );
    Associate( CSeqFeatData::eSubtype_J_segment, CGbqualType::e_Db_xref );
    Associate( CSeqFeatData::eSubtype_J_segment, CGbqualType::e_Evidence );
    Associate( CSeqFeatData::eSubtype_J_segment, CGbqualType::e_Experiment );
    Associate( CSeqFeatData::eSubtype_J_segment, CGbqualType::e_Gene );
    Associate( CSeqFeatData::eSubtype_J_segment, CGbqualType::e_Gene_synonym );
    Associate( CSeqFeatData::eSubtype_J_segment, CGbqualType::e_Inference );
    Associate( CSeqFeatData::eSubtype_J_segment, CGbqualType::e_Label );
    Associate( CSeqFeatData::eSubtype_J_segment, CGbqualType::e_Locus_tag );
    Associate( CSeqFeatData::eSubtype_J_segment, CGbqualType::e_Map );
    Associate( CSeqFeatData::eSubtype_J_segment, CGbqualType::e_Note );
    Associate( CSeqFeatData::eSubtype_J_segment, CGbqualType::e_Old_locus_tag );
    Associate( CSeqFeatData::eSubtype_J_segment, CGbqualType::e_Partial );
    Associate( CSeqFeatData::eSubtype_J_segment, CGbqualType::e_Product );
    Associate( CSeqFeatData::eSubtype_J_segment, CGbqualType::e_Pseudo );
    Associate( CSeqFeatData::eSubtype_J_segment, CGbqualType::e_Standard_name );
    Associate( CSeqFeatData::eSubtype_J_segment, CGbqualType::e_Usedin );

    //LTR
    Associate( CSeqFeatData::eSubtype_LTR, CGbqualType::e_Allele );
    Associate( CSeqFeatData::eSubtype_LTR, CGbqualType::e_Citation );
    Associate( CSeqFeatData::eSubtype_LTR, CGbqualType::e_Db_xref );
    Associate( CSeqFeatData::eSubtype_LTR, CGbqualType::e_Evidence );
    Associate( CSeqFeatData::eSubtype_LTR, CGbqualType::e_Experiment );
    Associate( CSeqFeatData::eSubtype_LTR, CGbqualType::e_Function );
    Associate( CSeqFeatData::eSubtype_LTR, CGbqualType::e_Gene );
    Associate( CSeqFeatData::eSubtype_LTR, CGbqualType::e_Gene_synonym );
    Associate( CSeqFeatData::eSubtype_LTR, CGbqualType::e_Inference );
    Associate( CSeqFeatData::eSubtype_LTR, CGbqualType::e_Label );
    Associate( CSeqFeatData::eSubtype_LTR, CGbqualType::e_Locus_tag );
    Associate( CSeqFeatData::eSubtype_LTR, CGbqualType::e_Note );
    Associate( CSeqFeatData::eSubtype_LTR, CGbqualType::e_Old_locus_tag );
    Associate( CSeqFeatData::eSubtype_LTR, CGbqualType::e_Partial );
    Associate( CSeqFeatData::eSubtype_LTR, CGbqualType::e_Standard_name );
    Associate( CSeqFeatData::eSubtype_LTR, CGbqualType::e_Usedin );

    //mat_peptide
    Associate( CSeqFeatData::eSubtype_mat_peptide, CGbqualType::e_Allele );
    Associate( CSeqFeatData::eSubtype_mat_peptide, CGbqualType::e_Citation );
    Associate( CSeqFeatData::eSubtype_mat_peptide, CGbqualType::e_Db_xref );
    Associate( CSeqFeatData::eSubtype_mat_peptide, CGbqualType::e_EC_number );
    Associate( CSeqFeatData::eSubtype_mat_peptide, CGbqualType::e_Evidence );
    Associate( CSeqFeatData::eSubtype_mat_peptide, CGbqualType::e_Evidence );
    Associate( CSeqFeatData::eSubtype_mat_peptide, CGbqualType::e_Experiment );
    Associate( CSeqFeatData::eSubtype_mat_peptide, CGbqualType::e_Experiment );
    Associate( CSeqFeatData::eSubtype_mat_peptide, CGbqualType::e_Function );
    Associate( CSeqFeatData::eSubtype_mat_peptide, CGbqualType::e_Gene );
    Associate( CSeqFeatData::eSubtype_mat_peptide, CGbqualType::e_Gene_synonym );
    Associate( CSeqFeatData::eSubtype_mat_peptide, CGbqualType::e_Inference );
    Associate( CSeqFeatData::eSubtype_mat_peptide, CGbqualType::e_Inference );
    Associate( CSeqFeatData::eSubtype_mat_peptide, CGbqualType::e_Label );
    Associate( CSeqFeatData::eSubtype_mat_peptide, CGbqualType::e_Locus_tag );
    Associate( CSeqFeatData::eSubtype_mat_peptide, CGbqualType::e_Map );
    Associate( CSeqFeatData::eSubtype_mat_peptide, CGbqualType::e_Note );
    Associate( CSeqFeatData::eSubtype_mat_peptide, CGbqualType::e_Old_locus_tag );
    Associate( CSeqFeatData::eSubtype_mat_peptide, CGbqualType::e_Partial );
    Associate( CSeqFeatData::eSubtype_mat_peptide, CGbqualType::e_Product );
    Associate( CSeqFeatData::eSubtype_mat_peptide, CGbqualType::e_Pseudo );
    Associate( CSeqFeatData::eSubtype_mat_peptide, CGbqualType::e_Standard_name );
    Associate( CSeqFeatData::eSubtype_mat_peptide, CGbqualType::e_Usedin );

    //misc_binding
    Associate( CSeqFeatData::eSubtype_misc_binding, CGbqualType::e_Bound_moiety );
    Associate( CSeqFeatData::eSubtype_misc_binding, CGbqualType::e_Allele );
    Associate( CSeqFeatData::eSubtype_misc_binding, CGbqualType::e_Citation );
    Associate( CSeqFeatData::eSubtype_misc_binding, CGbqualType::e_Db_xref );
    Associate( CSeqFeatData::eSubtype_misc_binding, CGbqualType::e_Evidence );
    Associate( CSeqFeatData::eSubtype_misc_binding, CGbqualType::e_Experiment );
    Associate( CSeqFeatData::eSubtype_misc_binding, CGbqualType::e_Function );
    Associate( CSeqFeatData::eSubtype_misc_binding, CGbqualType::e_Gene );
    Associate( CSeqFeatData::eSubtype_misc_binding, CGbqualType::e_Gene_synonym );
    Associate( CSeqFeatData::eSubtype_misc_binding, CGbqualType::e_Inference );
    Associate( CSeqFeatData::eSubtype_misc_binding, CGbqualType::e_Label );
    Associate( CSeqFeatData::eSubtype_misc_binding, CGbqualType::e_Locus_tag );
    Associate( CSeqFeatData::eSubtype_misc_binding, CGbqualType::e_Map );
    Associate( CSeqFeatData::eSubtype_misc_binding, CGbqualType::e_Note );
    Associate( CSeqFeatData::eSubtype_misc_binding, CGbqualType::e_Old_locus_tag );
    Associate( CSeqFeatData::eSubtype_misc_binding, CGbqualType::e_Partial );
    Associate( CSeqFeatData::eSubtype_misc_binding, CGbqualType::e_Usedin );

    //misc_difference
    Associate( CSeqFeatData::eSubtype_misc_difference, CGbqualType::e_Allele );
    Associate( CSeqFeatData::eSubtype_misc_difference, CGbqualType::e_Citation );
    Associate( CSeqFeatData::eSubtype_misc_difference, CGbqualType::e_Clone );
    Associate( CSeqFeatData::eSubtype_misc_difference, CGbqualType::e_Compare );
    Associate( CSeqFeatData::eSubtype_misc_difference, CGbqualType::e_Db_xref );
    Associate( CSeqFeatData::eSubtype_misc_difference, CGbqualType::e_Evidence );
    Associate( CSeqFeatData::eSubtype_misc_difference, CGbqualType::e_Experiment );
    Associate( CSeqFeatData::eSubtype_misc_difference, CGbqualType::e_Gene );
    Associate( CSeqFeatData::eSubtype_misc_difference, CGbqualType::e_Gene_synonym );
    Associate( CSeqFeatData::eSubtype_misc_difference, CGbqualType::e_Inference );
    Associate( CSeqFeatData::eSubtype_misc_difference, CGbqualType::e_Label );
    Associate( CSeqFeatData::eSubtype_misc_difference, CGbqualType::e_Locus_tag );
    Associate( CSeqFeatData::eSubtype_misc_difference, CGbqualType::e_Map );
    Associate( CSeqFeatData::eSubtype_misc_difference, CGbqualType::e_Note );
    Associate( CSeqFeatData::eSubtype_misc_difference, CGbqualType::e_Old_locus_tag );
    Associate( CSeqFeatData::eSubtype_misc_difference, CGbqualType::e_Partial );
    Associate( CSeqFeatData::eSubtype_misc_difference, CGbqualType::e_Phenotype );
    Associate( CSeqFeatData::eSubtype_misc_difference, CGbqualType::e_Replace );
    Associate( CSeqFeatData::eSubtype_misc_difference, CGbqualType::e_Standard_name );
    Associate( CSeqFeatData::eSubtype_misc_difference, CGbqualType::e_Usedin );

    //misc_feature
    Associate( CSeqFeatData::eSubtype_misc_feature, CGbqualType::e_Allele );
    Associate( CSeqFeatData::eSubtype_misc_feature, CGbqualType::e_Citation );
    Associate( CSeqFeatData::eSubtype_misc_feature, CGbqualType::e_Db_xref );
    Associate( CSeqFeatData::eSubtype_misc_feature, CGbqualType::e_Evidence );
    Associate( CSeqFeatData::eSubtype_misc_feature, CGbqualType::e_Experiment );
    Associate( CSeqFeatData::eSubtype_misc_feature, CGbqualType::e_Function );
    Associate( CSeqFeatData::eSubtype_misc_feature, CGbqualType::e_Gene );
    Associate( CSeqFeatData::eSubtype_misc_feature, CGbqualType::e_Gene_synonym );
    Associate( CSeqFeatData::eSubtype_misc_feature, CGbqualType::e_Inference );
    Associate( CSeqFeatData::eSubtype_misc_feature, CGbqualType::e_Label );
    Associate( CSeqFeatData::eSubtype_misc_feature, CGbqualType::e_Locus_tag );
    Associate( CSeqFeatData::eSubtype_misc_feature, CGbqualType::e_Map );
    Associate( CSeqFeatData::eSubtype_misc_feature, CGbqualType::e_Note );
    Associate( CSeqFeatData::eSubtype_misc_feature, CGbqualType::e_Number );
    Associate( CSeqFeatData::eSubtype_misc_feature, CGbqualType::e_Old_locus_tag );
    Associate( CSeqFeatData::eSubtype_misc_feature, CGbqualType::e_Partial );
    Associate( CSeqFeatData::eSubtype_misc_feature, CGbqualType::e_Phenotype );
    Associate( CSeqFeatData::eSubtype_misc_feature, CGbqualType::e_Product );
    Associate( CSeqFeatData::eSubtype_misc_feature, CGbqualType::e_Pseudo );
    Associate( CSeqFeatData::eSubtype_misc_feature, CGbqualType::e_Standard_name );
    Associate( CSeqFeatData::eSubtype_misc_feature, CGbqualType::e_Usedin );

    //misc_recomb
    Associate( CSeqFeatData::eSubtype_misc_recomb, CGbqualType::e_Allele );
    Associate( CSeqFeatData::eSubtype_misc_recomb, CGbqualType::e_Citation );
    Associate( CSeqFeatData::eSubtype_misc_recomb, CGbqualType::e_Db_xref );
    Associate( CSeqFeatData::eSubtype_misc_recomb, CGbqualType::e_Evidence );
    Associate( CSeqFeatData::eSubtype_misc_recomb, CGbqualType::e_Experiment );
    Associate( CSeqFeatData::eSubtype_misc_recomb, CGbqualType::e_Gene );
    Associate( CSeqFeatData::eSubtype_misc_recomb, CGbqualType::e_Gene_synonym );
    Associate( CSeqFeatData::eSubtype_misc_recomb, CGbqualType::e_Inference );
    Associate( CSeqFeatData::eSubtype_misc_recomb, CGbqualType::e_Label );
    Associate( CSeqFeatData::eSubtype_misc_recomb, CGbqualType::e_Locus_tag );
    Associate( CSeqFeatData::eSubtype_misc_recomb, CGbqualType::e_Map );
    Associate( CSeqFeatData::eSubtype_misc_recomb, CGbqualType::e_Note );
    Associate( CSeqFeatData::eSubtype_misc_recomb, CGbqualType::e_Old_locus_tag );
    Associate( CSeqFeatData::eSubtype_misc_recomb, CGbqualType::e_Partial );
    Associate( CSeqFeatData::eSubtype_misc_recomb, CGbqualType::e_Standard_name );
    Associate( CSeqFeatData::eSubtype_misc_recomb, CGbqualType::e_Usedin );

    //misc_RNA
    Associate( CSeqFeatData::eSubtype_misc_RNA, CGbqualType::e_Allele );
    Associate( CSeqFeatData::eSubtype_misc_RNA, CGbqualType::e_Citation );
    Associate( CSeqFeatData::eSubtype_misc_RNA, CGbqualType::e_Db_xref );
    Associate( CSeqFeatData::eSubtype_misc_RNA, CGbqualType::e_Evidence );
    Associate( CSeqFeatData::eSubtype_misc_RNA, CGbqualType::e_Experiment );
    Associate( CSeqFeatData::eSubtype_misc_RNA, CGbqualType::e_Function );
    Associate( CSeqFeatData::eSubtype_misc_RNA, CGbqualType::e_Gene );
    Associate( CSeqFeatData::eSubtype_misc_RNA, CGbqualType::e_Gene_synonym );
    Associate( CSeqFeatData::eSubtype_misc_RNA, CGbqualType::e_Inference );
    Associate( CSeqFeatData::eSubtype_misc_RNA, CGbqualType::e_Label );
    Associate( CSeqFeatData::eSubtype_misc_RNA, CGbqualType::e_Locus_tag );
    Associate( CSeqFeatData::eSubtype_misc_RNA, CGbqualType::e_Map );
    Associate( CSeqFeatData::eSubtype_misc_RNA, CGbqualType::e_Note );
    Associate( CSeqFeatData::eSubtype_misc_RNA, CGbqualType::e_Old_locus_tag );
    Associate( CSeqFeatData::eSubtype_misc_RNA, CGbqualType::e_Operon );
    Associate( CSeqFeatData::eSubtype_misc_RNA, CGbqualType::e_Partial );
    Associate( CSeqFeatData::eSubtype_misc_RNA, CGbqualType::e_Product );
    Associate( CSeqFeatData::eSubtype_misc_RNA, CGbqualType::e_Pseudo );
    Associate( CSeqFeatData::eSubtype_misc_RNA, CGbqualType::e_Standard_name );
    Associate( CSeqFeatData::eSubtype_misc_RNA, CGbqualType::e_Trans_splicing );
    Associate( CSeqFeatData::eSubtype_misc_RNA, CGbqualType::e_Usedin );

    //misc_signal
    Associate( CSeqFeatData::eSubtype_misc_signal, CGbqualType::e_Allele );
    Associate( CSeqFeatData::eSubtype_misc_signal, CGbqualType::e_Citation );
    Associate( CSeqFeatData::eSubtype_misc_signal, CGbqualType::e_Db_xref );
    Associate( CSeqFeatData::eSubtype_misc_signal, CGbqualType::e_Evidence );
    Associate( CSeqFeatData::eSubtype_misc_signal, CGbqualType::e_Experiment );
    Associate( CSeqFeatData::eSubtype_misc_signal, CGbqualType::e_Function );
    Associate( CSeqFeatData::eSubtype_misc_signal, CGbqualType::e_Gene );
    Associate( CSeqFeatData::eSubtype_misc_signal, CGbqualType::e_Gene_synonym );
    Associate( CSeqFeatData::eSubtype_misc_signal, CGbqualType::e_Inference );
    Associate( CSeqFeatData::eSubtype_misc_signal, CGbqualType::e_Label );
    Associate( CSeqFeatData::eSubtype_misc_signal, CGbqualType::e_Locus_tag );
    Associate( CSeqFeatData::eSubtype_misc_signal, CGbqualType::e_Map );
    Associate( CSeqFeatData::eSubtype_misc_signal, CGbqualType::e_Note );
    Associate( CSeqFeatData::eSubtype_misc_signal, CGbqualType::e_Old_locus_tag );
    Associate( CSeqFeatData::eSubtype_misc_signal, CGbqualType::e_Operon );
    Associate( CSeqFeatData::eSubtype_misc_signal, CGbqualType::e_Partial );
    Associate( CSeqFeatData::eSubtype_misc_signal, CGbqualType::e_Phenotype );
    Associate( CSeqFeatData::eSubtype_misc_signal, CGbqualType::e_Standard_name );
    Associate( CSeqFeatData::eSubtype_misc_signal, CGbqualType::e_Usedin );

    //misc_structure
    Associate( CSeqFeatData::eSubtype_misc_structure, CGbqualType::e_Allele );
    Associate( CSeqFeatData::eSubtype_misc_structure, CGbqualType::e_Citation );
    Associate( CSeqFeatData::eSubtype_misc_structure, CGbqualType::e_Db_xref );
    Associate( CSeqFeatData::eSubtype_misc_structure, CGbqualType::e_Evidence );
    Associate( CSeqFeatData::eSubtype_misc_structure, CGbqualType::e_Experiment );
    Associate( CSeqFeatData::eSubtype_misc_structure, CGbqualType::e_Function );
    Associate( CSeqFeatData::eSubtype_misc_structure, CGbqualType::e_Gene );
    Associate( CSeqFeatData::eSubtype_misc_structure, CGbqualType::e_Gene_synonym );
    Associate( CSeqFeatData::eSubtype_misc_structure, CGbqualType::e_Inference );
    Associate( CSeqFeatData::eSubtype_misc_structure, CGbqualType::e_Label );
    Associate( CSeqFeatData::eSubtype_misc_structure, CGbqualType::e_Locus_tag );
    Associate( CSeqFeatData::eSubtype_misc_structure, CGbqualType::e_Map );
    Associate( CSeqFeatData::eSubtype_misc_structure, CGbqualType::e_Note );
    Associate( CSeqFeatData::eSubtype_misc_structure, CGbqualType::e_Old_locus_tag );
    Associate( CSeqFeatData::eSubtype_misc_structure, CGbqualType::e_Partial );
    Associate( CSeqFeatData::eSubtype_misc_structure, CGbqualType::e_Standard_name );
    Associate( CSeqFeatData::eSubtype_misc_structure, CGbqualType::e_Usedin );

    //modified_base
    Associate( CSeqFeatData::eSubtype_modified_base, CGbqualType::e_Mod_base );
    Associate( CSeqFeatData::eSubtype_modified_base, CGbqualType::e_Allele );
    Associate( CSeqFeatData::eSubtype_modified_base, CGbqualType::e_Citation );
    Associate( CSeqFeatData::eSubtype_modified_base, CGbqualType::e_Db_xref );
    Associate( CSeqFeatData::eSubtype_modified_base, CGbqualType::e_Evidence );
    Associate( CSeqFeatData::eSubtype_modified_base, CGbqualType::e_Experiment );
    Associate( CSeqFeatData::eSubtype_modified_base, CGbqualType::e_Frequency );
    Associate( CSeqFeatData::eSubtype_modified_base, CGbqualType::e_Gene );
    Associate( CSeqFeatData::eSubtype_modified_base, CGbqualType::e_Gene_synonym );
    Associate( CSeqFeatData::eSubtype_modified_base, CGbqualType::e_Inference );
    Associate( CSeqFeatData::eSubtype_modified_base, CGbqualType::e_Label );
    Associate( CSeqFeatData::eSubtype_modified_base, CGbqualType::e_Locus_tag );
    Associate( CSeqFeatData::eSubtype_modified_base, CGbqualType::e_Map );
    Associate( CSeqFeatData::eSubtype_modified_base, CGbqualType::e_Note );
    Associate( CSeqFeatData::eSubtype_modified_base, CGbqualType::e_Old_locus_tag );
    Associate( CSeqFeatData::eSubtype_modified_base, CGbqualType::e_Usedin );

    //mRNA
    Associate( CSeqFeatData::eSubtype_mRNA, CGbqualType::e_Allele );
    Associate( CSeqFeatData::eSubtype_mRNA, CGbqualType::e_Citation );
    Associate( CSeqFeatData::eSubtype_mRNA, CGbqualType::e_Db_xref );
    Associate( CSeqFeatData::eSubtype_mRNA, CGbqualType::e_Evidence );
    Associate( CSeqFeatData::eSubtype_mRNA, CGbqualType::e_Experiment );
    Associate( CSeqFeatData::eSubtype_mRNA, CGbqualType::e_Function );
    Associate( CSeqFeatData::eSubtype_mRNA, CGbqualType::e_Gene );
    Associate( CSeqFeatData::eSubtype_mRNA, CGbqualType::e_Gene_synonym );
    Associate( CSeqFeatData::eSubtype_mRNA, CGbqualType::e_Inference );
    Associate( CSeqFeatData::eSubtype_mRNA, CGbqualType::e_Label );
    Associate( CSeqFeatData::eSubtype_mRNA, CGbqualType::e_Locus_tag );
    Associate( CSeqFeatData::eSubtype_mRNA, CGbqualType::e_Map );
    Associate( CSeqFeatData::eSubtype_mRNA, CGbqualType::e_Note );
    Associate( CSeqFeatData::eSubtype_mRNA, CGbqualType::e_Old_locus_tag );
    Associate( CSeqFeatData::eSubtype_mRNA, CGbqualType::e_Operon );
    Associate( CSeqFeatData::eSubtype_mRNA, CGbqualType::e_Partial );
    Associate( CSeqFeatData::eSubtype_mRNA, CGbqualType::e_Product );
    Associate( CSeqFeatData::eSubtype_mRNA, CGbqualType::e_Pseudo );
    Associate( CSeqFeatData::eSubtype_mRNA, CGbqualType::e_Standard_name );
    Associate( CSeqFeatData::eSubtype_mRNA, CGbqualType::e_Trans_splicing );
    Associate( CSeqFeatData::eSubtype_mRNA, CGbqualType::e_Transcript_id );
    Associate( CSeqFeatData::eSubtype_mRNA, CGbqualType::e_Usedin );

    //mutation
    Associate( CSeqFeatData::eSubtype_mutation, CGbqualType::e_Citation );
    Associate( CSeqFeatData::eSubtype_mutation, CGbqualType::e_Db_xref );
    Associate( CSeqFeatData::eSubtype_mutation, CGbqualType::e_Evidence );
    Associate( CSeqFeatData::eSubtype_mutation, CGbqualType::e_Experiment );
    Associate( CSeqFeatData::eSubtype_mutation, CGbqualType::e_Frequency );
    Associate( CSeqFeatData::eSubtype_mutation, CGbqualType::e_Gene );
    Associate( CSeqFeatData::eSubtype_mutation, CGbqualType::e_Gene_synonym );
    Associate( CSeqFeatData::eSubtype_mutation, CGbqualType::e_Inference );
    Associate( CSeqFeatData::eSubtype_mutation, CGbqualType::e_Label );
    Associate( CSeqFeatData::eSubtype_mutation, CGbqualType::e_Locus_tag );
    Associate( CSeqFeatData::eSubtype_mutation, CGbqualType::e_Map );
    Associate( CSeqFeatData::eSubtype_mutation, CGbqualType::e_Note );
    Associate( CSeqFeatData::eSubtype_mutation, CGbqualType::e_Old_locus_tag );
    Associate( CSeqFeatData::eSubtype_mutation, CGbqualType::e_Phenotype );
    Associate( CSeqFeatData::eSubtype_mutation, CGbqualType::e_Product );
    Associate( CSeqFeatData::eSubtype_mutation, CGbqualType::e_Replace );
    Associate( CSeqFeatData::eSubtype_mutation, CGbqualType::e_Standard_name );
    Associate( CSeqFeatData::eSubtype_mutation, CGbqualType::e_Usedin );

    //N_region
    Associate( CSeqFeatData::eSubtype_N_region, CGbqualType::e_Citation );
    Associate( CSeqFeatData::eSubtype_N_region, CGbqualType::e_Db_xref );
    Associate( CSeqFeatData::eSubtype_N_region, CGbqualType::e_Evidence );
    Associate( CSeqFeatData::eSubtype_N_region, CGbqualType::e_Experiment );
    Associate( CSeqFeatData::eSubtype_N_region, CGbqualType::e_Gene );
    Associate( CSeqFeatData::eSubtype_N_region, CGbqualType::e_Gene_synonym );
    Associate( CSeqFeatData::eSubtype_N_region, CGbqualType::e_Inference );
    Associate( CSeqFeatData::eSubtype_N_region, CGbqualType::e_Label );
    Associate( CSeqFeatData::eSubtype_N_region, CGbqualType::e_Locus_tag );
    Associate( CSeqFeatData::eSubtype_N_region, CGbqualType::e_Map );
    Associate( CSeqFeatData::eSubtype_N_region, CGbqualType::e_Note );
    Associate( CSeqFeatData::eSubtype_N_region, CGbqualType::e_Old_locus_tag );
    Associate( CSeqFeatData::eSubtype_N_region, CGbqualType::e_Product );
    Associate( CSeqFeatData::eSubtype_N_region, CGbqualType::e_Pseudo );
    Associate( CSeqFeatData::eSubtype_N_region, CGbqualType::e_Standard_name );
    Associate( CSeqFeatData::eSubtype_N_region, CGbqualType::e_Usedin );

    //ncRNA
    Associate( CSeqFeatData::eSubtype_ncRNA, CGbqualType::e_NcRNA_class );
    Associate( CSeqFeatData::eSubtype_ncRNA, CGbqualType::e_Allele );
    Associate( CSeqFeatData::eSubtype_ncRNA, CGbqualType::e_Citation );
    Associate( CSeqFeatData::eSubtype_ncRNA, CGbqualType::e_Db_xref );
    Associate( CSeqFeatData::eSubtype_ncRNA, CGbqualType::e_Evidence );
    Associate( CSeqFeatData::eSubtype_ncRNA, CGbqualType::e_Experiment );
    Associate( CSeqFeatData::eSubtype_ncRNA, CGbqualType::e_Function );
    Associate( CSeqFeatData::eSubtype_ncRNA, CGbqualType::e_Gene );
    Associate( CSeqFeatData::eSubtype_ncRNA, CGbqualType::e_Gene_synonym );
    Associate( CSeqFeatData::eSubtype_ncRNA, CGbqualType::e_Inference );
    Associate( CSeqFeatData::eSubtype_ncRNA, CGbqualType::e_Label );
    Associate( CSeqFeatData::eSubtype_ncRNA, CGbqualType::e_Locus_tag );
    Associate( CSeqFeatData::eSubtype_ncRNA, CGbqualType::e_Map );
    Associate( CSeqFeatData::eSubtype_ncRNA, CGbqualType::e_Note );
    Associate( CSeqFeatData::eSubtype_ncRNA, CGbqualType::e_Old_locus_tag );
    Associate( CSeqFeatData::eSubtype_ncRNA, CGbqualType::e_Operon );
    Associate( CSeqFeatData::eSubtype_ncRNA, CGbqualType::e_Partial );
    Associate( CSeqFeatData::eSubtype_ncRNA, CGbqualType::e_Product );
    Associate( CSeqFeatData::eSubtype_ncRNA, CGbqualType::e_Pseudo );
    Associate( CSeqFeatData::eSubtype_ncRNA, CGbqualType::e_Standard_name );
    Associate( CSeqFeatData::eSubtype_ncRNA, CGbqualType::e_Trans_splicing );
    Associate( CSeqFeatData::eSubtype_ncRNA, CGbqualType::e_Usedin );

    //old_sequence
    Associate( CSeqFeatData::eSubtype_old_sequence, CGbqualType::e_Citation );
    Associate( CSeqFeatData::eSubtype_old_sequence, CGbqualType::e_Allele );
    Associate( CSeqFeatData::eSubtype_old_sequence, CGbqualType::e_Compare );
    Associate( CSeqFeatData::eSubtype_old_sequence, CGbqualType::e_Db_xref );
    Associate( CSeqFeatData::eSubtype_old_sequence, CGbqualType::e_Evidence );
    Associate( CSeqFeatData::eSubtype_old_sequence, CGbqualType::e_Experiment );
    Associate( CSeqFeatData::eSubtype_old_sequence, CGbqualType::e_Gene );
    Associate( CSeqFeatData::eSubtype_old_sequence, CGbqualType::e_Gene_synonym );
    Associate( CSeqFeatData::eSubtype_old_sequence, CGbqualType::e_Inference );
    Associate( CSeqFeatData::eSubtype_old_sequence, CGbqualType::e_Locus_tag );
    Associate( CSeqFeatData::eSubtype_old_sequence, CGbqualType::e_Map );
    Associate( CSeqFeatData::eSubtype_old_sequence, CGbqualType::e_Note );
    Associate( CSeqFeatData::eSubtype_old_sequence, CGbqualType::e_Old_locus_tag );
    Associate( CSeqFeatData::eSubtype_old_sequence, CGbqualType::e_Partial );
    Associate( CSeqFeatData::eSubtype_old_sequence, CGbqualType::e_Replace );
    Associate( CSeqFeatData::eSubtype_old_sequence, CGbqualType::e_Usedin );

    //operon
    Associate( CSeqFeatData::eSubtype_operon, CGbqualType::e_Operon );
    Associate( CSeqFeatData::eSubtype_operon, CGbqualType::e_Allele );
    Associate( CSeqFeatData::eSubtype_operon, CGbqualType::e_Citation );
    Associate( CSeqFeatData::eSubtype_operon, CGbqualType::e_Db_xref );
    Associate( CSeqFeatData::eSubtype_operon, CGbqualType::e_Evidence );
    Associate( CSeqFeatData::eSubtype_operon, CGbqualType::e_Experiment );
    Associate( CSeqFeatData::eSubtype_operon, CGbqualType::e_Function );
    Associate( CSeqFeatData::eSubtype_operon, CGbqualType::e_Inference );
    Associate( CSeqFeatData::eSubtype_operon, CGbqualType::e_Label );
    Associate( CSeqFeatData::eSubtype_operon, CGbqualType::e_Map );
    Associate( CSeqFeatData::eSubtype_operon, CGbqualType::e_Note );
    Associate( CSeqFeatData::eSubtype_operon, CGbqualType::e_Partial );
    Associate( CSeqFeatData::eSubtype_operon, CGbqualType::e_Phenotype );
    Associate( CSeqFeatData::eSubtype_operon, CGbqualType::e_Pseudo );
    Associate( CSeqFeatData::eSubtype_operon, CGbqualType::e_Usedin );

    //oriT
    Associate( CSeqFeatData::eSubtype_oriT, CGbqualType::e_Allele );
    Associate( CSeqFeatData::eSubtype_oriT, CGbqualType::e_Bound_moiety );
    Associate( CSeqFeatData::eSubtype_oriT, CGbqualType::e_Citation );
    Associate( CSeqFeatData::eSubtype_oriT, CGbqualType::e_Db_xref );
    Associate( CSeqFeatData::eSubtype_oriT, CGbqualType::e_Direction );
    Associate( CSeqFeatData::eSubtype_oriT, CGbqualType::e_Evidence );
    Associate( CSeqFeatData::eSubtype_oriT, CGbqualType::e_Experiment );
    Associate( CSeqFeatData::eSubtype_oriT, CGbqualType::e_Gene );
    Associate( CSeqFeatData::eSubtype_oriT, CGbqualType::e_Gene_synonym );
    Associate( CSeqFeatData::eSubtype_oriT, CGbqualType::e_Inference );
    Associate( CSeqFeatData::eSubtype_oriT, CGbqualType::e_Label );
    Associate( CSeqFeatData::eSubtype_oriT, CGbqualType::e_Locus_tag );
    Associate( CSeqFeatData::eSubtype_oriT, CGbqualType::e_Map );
    Associate( CSeqFeatData::eSubtype_oriT, CGbqualType::e_Note );
    Associate( CSeqFeatData::eSubtype_oriT, CGbqualType::e_Old_locus_tag );
    Associate( CSeqFeatData::eSubtype_oriT, CGbqualType::e_Partial );
    Associate( CSeqFeatData::eSubtype_oriT, CGbqualType::e_Rpt_family );
    Associate( CSeqFeatData::eSubtype_oriT, CGbqualType::e_Rpt_type );
    Associate( CSeqFeatData::eSubtype_oriT, CGbqualType::e_Rpt_unit );
    Associate( CSeqFeatData::eSubtype_oriT, CGbqualType::e_Rpt_unit_range );
    Associate( CSeqFeatData::eSubtype_oriT, CGbqualType::e_Rpt_unit_seq );
    Associate( CSeqFeatData::eSubtype_oriT, CGbqualType::e_Standard_name );
    Associate( CSeqFeatData::eSubtype_oriT, CGbqualType::e_Usedin );

    //polyA_signal
    Associate( CSeqFeatData::eSubtype_polyA_signal, CGbqualType::e_Allele );
    Associate( CSeqFeatData::eSubtype_polyA_signal, CGbqualType::e_Citation );
    Associate( CSeqFeatData::eSubtype_polyA_signal, CGbqualType::e_Db_xref );
    Associate( CSeqFeatData::eSubtype_polyA_signal, CGbqualType::e_Evidence );
    Associate( CSeqFeatData::eSubtype_polyA_signal, CGbqualType::e_Experiment );
    Associate( CSeqFeatData::eSubtype_polyA_signal, CGbqualType::e_Gene );
    Associate( CSeqFeatData::eSubtype_polyA_signal, CGbqualType::e_Gene_synonym );
    Associate( CSeqFeatData::eSubtype_polyA_signal, CGbqualType::e_Inference );
    Associate( CSeqFeatData::eSubtype_polyA_signal, CGbqualType::e_Label );
    Associate( CSeqFeatData::eSubtype_polyA_signal, CGbqualType::e_Locus_tag );
    Associate( CSeqFeatData::eSubtype_polyA_signal, CGbqualType::e_Map );
    Associate( CSeqFeatData::eSubtype_polyA_signal, CGbqualType::e_Note );
    Associate( CSeqFeatData::eSubtype_polyA_signal, CGbqualType::e_Old_locus_tag );
    Associate( CSeqFeatData::eSubtype_polyA_signal, CGbqualType::e_Partial );
    Associate( CSeqFeatData::eSubtype_polyA_signal, CGbqualType::e_Usedin );

    //polyA_site
    Associate( CSeqFeatData::eSubtype_polyA_site, CGbqualType::e_Allele );
    Associate( CSeqFeatData::eSubtype_polyA_site, CGbqualType::e_Citation );
    Associate( CSeqFeatData::eSubtype_polyA_site, CGbqualType::e_Db_xref );
    Associate( CSeqFeatData::eSubtype_polyA_site, CGbqualType::e_Evidence );
    Associate( CSeqFeatData::eSubtype_polyA_site, CGbqualType::e_Experiment );
    Associate( CSeqFeatData::eSubtype_polyA_site, CGbqualType::e_Gene );
    Associate( CSeqFeatData::eSubtype_polyA_site, CGbqualType::e_Gene_synonym );
    Associate( CSeqFeatData::eSubtype_polyA_site, CGbqualType::e_Inference );
    Associate( CSeqFeatData::eSubtype_polyA_site, CGbqualType::e_Label );
    Associate( CSeqFeatData::eSubtype_polyA_site, CGbqualType::e_Locus_tag );
    Associate( CSeqFeatData::eSubtype_polyA_site, CGbqualType::e_Map );
    Associate( CSeqFeatData::eSubtype_polyA_site, CGbqualType::e_Note );
    Associate( CSeqFeatData::eSubtype_polyA_site, CGbqualType::e_Old_locus_tag );
    Associate( CSeqFeatData::eSubtype_polyA_site, CGbqualType::e_Usedin );

    //precursor_RNA
    Associate( CSeqFeatData::eSubtype_preRNA, CGbqualType::e_Allele );
    Associate( CSeqFeatData::eSubtype_preRNA, CGbqualType::e_Citation );
    Associate( CSeqFeatData::eSubtype_preRNA, CGbqualType::e_Db_xref );
    Associate( CSeqFeatData::eSubtype_preRNA, CGbqualType::e_Evidence );
    Associate( CSeqFeatData::eSubtype_preRNA, CGbqualType::e_Experiment );
    Associate( CSeqFeatData::eSubtype_preRNA, CGbqualType::e_Function );
    Associate( CSeqFeatData::eSubtype_preRNA, CGbqualType::e_Gene );
    Associate( CSeqFeatData::eSubtype_preRNA, CGbqualType::e_Gene_synonym );
    Associate( CSeqFeatData::eSubtype_preRNA, CGbqualType::e_Inference );
    Associate( CSeqFeatData::eSubtype_preRNA, CGbqualType::e_Label );
    Associate( CSeqFeatData::eSubtype_preRNA, CGbqualType::e_Locus_tag );
    Associate( CSeqFeatData::eSubtype_preRNA, CGbqualType::e_Map );
    Associate( CSeqFeatData::eSubtype_preRNA, CGbqualType::e_Note );
    Associate( CSeqFeatData::eSubtype_preRNA, CGbqualType::e_Old_locus_tag );
    Associate( CSeqFeatData::eSubtype_preRNA, CGbqualType::e_Operon );
    Associate( CSeqFeatData::eSubtype_preRNA, CGbqualType::e_Partial );
    Associate( CSeqFeatData::eSubtype_preRNA, CGbqualType::e_Product );
    Associate( CSeqFeatData::eSubtype_preRNA, CGbqualType::e_Standard_name );
    Associate( CSeqFeatData::eSubtype_preRNA, CGbqualType::e_Trans_splicing );
    Associate( CSeqFeatData::eSubtype_preRNA, CGbqualType::e_Usedin );

    //prim_transcript
    Associate( CSeqFeatData::eSubtype_prim_transcript, CGbqualType::e_Allele );
    Associate( CSeqFeatData::eSubtype_prim_transcript, CGbqualType::e_Citation );
    Associate( CSeqFeatData::eSubtype_prim_transcript, CGbqualType::e_Db_xref );
    Associate( CSeqFeatData::eSubtype_prim_transcript, CGbqualType::e_Evidence );
    Associate( CSeqFeatData::eSubtype_prim_transcript, CGbqualType::e_Experiment );
    Associate( CSeqFeatData::eSubtype_prim_transcript, CGbqualType::e_Function );
    Associate( CSeqFeatData::eSubtype_prim_transcript, CGbqualType::e_Gene );
    Associate( CSeqFeatData::eSubtype_prim_transcript, CGbqualType::e_Gene_synonym );
    Associate( CSeqFeatData::eSubtype_prim_transcript, CGbqualType::e_Inference );
    Associate( CSeqFeatData::eSubtype_prim_transcript, CGbqualType::e_Label );
    Associate( CSeqFeatData::eSubtype_prim_transcript, CGbqualType::e_Locus_tag );
    Associate( CSeqFeatData::eSubtype_prim_transcript, CGbqualType::e_Map );
    Associate( CSeqFeatData::eSubtype_prim_transcript, CGbqualType::e_Note );
    Associate( CSeqFeatData::eSubtype_prim_transcript, CGbqualType::e_Old_locus_tag );
    Associate( CSeqFeatData::eSubtype_prim_transcript, CGbqualType::e_Operon );
    Associate( CSeqFeatData::eSubtype_prim_transcript, CGbqualType::e_Partial );
    Associate( CSeqFeatData::eSubtype_prim_transcript, CGbqualType::e_Standard_name );
    Associate( CSeqFeatData::eSubtype_prim_transcript, CGbqualType::e_Usedin );

    //primer_bind
    Associate( CSeqFeatData::eSubtype_primer_bind, CGbqualType::e_Allele );
    Associate( CSeqFeatData::eSubtype_primer_bind, CGbqualType::e_Citation );
    Associate( CSeqFeatData::eSubtype_primer_bind, CGbqualType::e_Db_xref );
    Associate( CSeqFeatData::eSubtype_primer_bind, CGbqualType::e_Evidence );
    Associate( CSeqFeatData::eSubtype_primer_bind, CGbqualType::e_Experiment );
    Associate( CSeqFeatData::eSubtype_primer_bind, CGbqualType::e_Gene );
    Associate( CSeqFeatData::eSubtype_primer_bind, CGbqualType::e_Gene_synonym );
    Associate( CSeqFeatData::eSubtype_primer_bind, CGbqualType::e_Inference );
    Associate( CSeqFeatData::eSubtype_primer_bind, CGbqualType::e_Label );
    Associate( CSeqFeatData::eSubtype_primer_bind, CGbqualType::e_Locus_tag );
    Associate( CSeqFeatData::eSubtype_primer_bind, CGbqualType::e_Map );
    Associate( CSeqFeatData::eSubtype_primer_bind, CGbqualType::e_Note );
    Associate( CSeqFeatData::eSubtype_primer_bind, CGbqualType::e_Old_locus_tag );
    Associate( CSeqFeatData::eSubtype_primer_bind, CGbqualType::e_Partial );
    Associate( CSeqFeatData::eSubtype_primer_bind, CGbqualType::e_PCR_conditions );
    Associate( CSeqFeatData::eSubtype_primer_bind, CGbqualType::e_Standard_name );
    Associate( CSeqFeatData::eSubtype_primer_bind, CGbqualType::e_Usedin );

    //promoter
    Associate( CSeqFeatData::eSubtype_promoter, CGbqualType::e_Allele );
    Associate( CSeqFeatData::eSubtype_promoter, CGbqualType::e_Bound_moiety );
    Associate( CSeqFeatData::eSubtype_promoter, CGbqualType::e_Citation );
    Associate( CSeqFeatData::eSubtype_promoter, CGbqualType::e_Db_xref );
    Associate( CSeqFeatData::eSubtype_promoter, CGbqualType::e_Evidence );
    Associate( CSeqFeatData::eSubtype_promoter, CGbqualType::e_Experiment );
    Associate( CSeqFeatData::eSubtype_promoter, CGbqualType::e_Function );
    Associate( CSeqFeatData::eSubtype_promoter, CGbqualType::e_Gene );
    Associate( CSeqFeatData::eSubtype_promoter, CGbqualType::e_Gene_synonym );
    Associate( CSeqFeatData::eSubtype_promoter, CGbqualType::e_Inference );
    Associate( CSeqFeatData::eSubtype_promoter, CGbqualType::e_Label );
    Associate( CSeqFeatData::eSubtype_promoter, CGbqualType::e_Locus_tag );
    Associate( CSeqFeatData::eSubtype_promoter, CGbqualType::e_Map );
    Associate( CSeqFeatData::eSubtype_promoter, CGbqualType::e_Note );
    Associate( CSeqFeatData::eSubtype_promoter, CGbqualType::e_Old_locus_tag );
    Associate( CSeqFeatData::eSubtype_promoter, CGbqualType::e_Operon );
    Associate( CSeqFeatData::eSubtype_promoter, CGbqualType::e_Partial );
    Associate( CSeqFeatData::eSubtype_promoter, CGbqualType::e_Phenotype );
    Associate( CSeqFeatData::eSubtype_promoter, CGbqualType::e_Pseudo );
    Associate( CSeqFeatData::eSubtype_promoter, CGbqualType::e_Standard_name );
    Associate( CSeqFeatData::eSubtype_promoter, CGbqualType::e_Usedin );

    //protein_bind
    Associate( CSeqFeatData::eSubtype_protein_bind, CGbqualType::e_Bound_moiety );
    Associate( CSeqFeatData::eSubtype_protein_bind, CGbqualType::e_Allele );
    Associate( CSeqFeatData::eSubtype_protein_bind, CGbqualType::e_Citation );
    Associate( CSeqFeatData::eSubtype_protein_bind, CGbqualType::e_Db_xref );
    Associate( CSeqFeatData::eSubtype_protein_bind, CGbqualType::e_Evidence );
    Associate( CSeqFeatData::eSubtype_protein_bind, CGbqualType::e_Experiment );
    Associate( CSeqFeatData::eSubtype_protein_bind, CGbqualType::e_Function );
    Associate( CSeqFeatData::eSubtype_protein_bind, CGbqualType::e_Gene );
    Associate( CSeqFeatData::eSubtype_protein_bind, CGbqualType::e_Gene_synonym );
    Associate( CSeqFeatData::eSubtype_protein_bind, CGbqualType::e_Inference );
    Associate( CSeqFeatData::eSubtype_protein_bind, CGbqualType::e_Label );
    Associate( CSeqFeatData::eSubtype_protein_bind, CGbqualType::e_Locus_tag );
    Associate( CSeqFeatData::eSubtype_protein_bind, CGbqualType::e_Map );
    Associate( CSeqFeatData::eSubtype_protein_bind, CGbqualType::e_Note );
    Associate( CSeqFeatData::eSubtype_protein_bind, CGbqualType::e_Old_locus_tag );
    Associate( CSeqFeatData::eSubtype_protein_bind, CGbqualType::e_Operon );
    Associate( CSeqFeatData::eSubtype_protein_bind, CGbqualType::e_Partial );
    Associate( CSeqFeatData::eSubtype_protein_bind, CGbqualType::e_Standard_name );
    Associate( CSeqFeatData::eSubtype_protein_bind, CGbqualType::e_Usedin );

    //RBS
    Associate( CSeqFeatData::eSubtype_RBS, CGbqualType::e_Allele );
    Associate( CSeqFeatData::eSubtype_RBS, CGbqualType::e_Citation );
    Associate( CSeqFeatData::eSubtype_RBS, CGbqualType::e_Db_xref );
    Associate( CSeqFeatData::eSubtype_RBS, CGbqualType::e_Evidence );
    Associate( CSeqFeatData::eSubtype_RBS, CGbqualType::e_Experiment );
    Associate( CSeqFeatData::eSubtype_RBS, CGbqualType::e_Gene );
    Associate( CSeqFeatData::eSubtype_RBS, CGbqualType::e_Gene_synonym );
    Associate( CSeqFeatData::eSubtype_RBS, CGbqualType::e_Inference );
    Associate( CSeqFeatData::eSubtype_RBS, CGbqualType::e_Label );
    Associate( CSeqFeatData::eSubtype_RBS, CGbqualType::e_Locus_tag );
    Associate( CSeqFeatData::eSubtype_RBS, CGbqualType::e_Map );
    Associate( CSeqFeatData::eSubtype_RBS, CGbqualType::e_Note );
    Associate( CSeqFeatData::eSubtype_RBS, CGbqualType::e_Old_locus_tag );
    Associate( CSeqFeatData::eSubtype_RBS, CGbqualType::e_Partial );
    Associate( CSeqFeatData::eSubtype_RBS, CGbqualType::e_Standard_name );
    Associate( CSeqFeatData::eSubtype_RBS, CGbqualType::e_Usedin );

    //repeat_region
    Associate( CSeqFeatData::eSubtype_repeat_region, CGbqualType::e_Allele );
    Associate( CSeqFeatData::eSubtype_repeat_region, CGbqualType::e_Citation );
    Associate( CSeqFeatData::eSubtype_repeat_region, CGbqualType::e_Db_xref );
    Associate( CSeqFeatData::eSubtype_repeat_region, CGbqualType::e_Evidence );
    Associate( CSeqFeatData::eSubtype_repeat_region, CGbqualType::e_Experiment );
    Associate( CSeqFeatData::eSubtype_repeat_region, CGbqualType::e_Function );
    Associate( CSeqFeatData::eSubtype_repeat_region, CGbqualType::e_Gene );
    Associate( CSeqFeatData::eSubtype_repeat_region, CGbqualType::e_Gene_synonym );
    Associate( CSeqFeatData::eSubtype_repeat_region, CGbqualType::e_Inference );
    Associate( CSeqFeatData::eSubtype_repeat_region, CGbqualType::e_Insertion_seq );
    Associate( CSeqFeatData::eSubtype_repeat_region, CGbqualType::e_Label );
    Associate( CSeqFeatData::eSubtype_repeat_region, CGbqualType::e_Locus_tag );
    Associate( CSeqFeatData::eSubtype_repeat_region, CGbqualType::e_Map );
    Associate( CSeqFeatData::eSubtype_repeat_region, CGbqualType::e_Mobile_element );
    Associate( CSeqFeatData::eSubtype_repeat_region, CGbqualType::e_Note );
    Associate( CSeqFeatData::eSubtype_repeat_region, CGbqualType::e_Old_locus_tag );
    Associate( CSeqFeatData::eSubtype_repeat_region, CGbqualType::e_Partial );
    Associate( CSeqFeatData::eSubtype_repeat_region, CGbqualType::e_Rpt_family );
    Associate( CSeqFeatData::eSubtype_repeat_region, CGbqualType::e_Rpt_type );
    Associate( CSeqFeatData::eSubtype_repeat_region, CGbqualType::e_Rpt_unit );
    Associate( CSeqFeatData::eSubtype_repeat_region, CGbqualType::e_Rpt_unit_range );
    Associate( CSeqFeatData::eSubtype_repeat_region, CGbqualType::e_Rpt_unit_seq );
    Associate( CSeqFeatData::eSubtype_repeat_region, CGbqualType::e_Satellite );
    Associate( CSeqFeatData::eSubtype_repeat_region, CGbqualType::e_Standard_name );
    Associate( CSeqFeatData::eSubtype_repeat_region, CGbqualType::e_Transposon );
    Associate( CSeqFeatData::eSubtype_repeat_region, CGbqualType::e_Usedin );

    //repeat_unit
    Associate( CSeqFeatData::eSubtype_repeat_unit, CGbqualType::e_Allele );
    Associate( CSeqFeatData::eSubtype_repeat_unit, CGbqualType::e_Citation );
    Associate( CSeqFeatData::eSubtype_repeat_unit, CGbqualType::e_Db_xref );
    Associate( CSeqFeatData::eSubtype_repeat_unit, CGbqualType::e_Evidence );
    Associate( CSeqFeatData::eSubtype_repeat_unit, CGbqualType::e_Experiment );
    Associate( CSeqFeatData::eSubtype_repeat_unit, CGbqualType::e_Function );
    Associate( CSeqFeatData::eSubtype_repeat_unit, CGbqualType::e_Gene );
    Associate( CSeqFeatData::eSubtype_repeat_unit, CGbqualType::e_Gene_synonym );
    Associate( CSeqFeatData::eSubtype_repeat_unit, CGbqualType::e_Inference );
    Associate( CSeqFeatData::eSubtype_repeat_unit, CGbqualType::e_Label );
    Associate( CSeqFeatData::eSubtype_repeat_unit, CGbqualType::e_Locus_tag );
    Associate( CSeqFeatData::eSubtype_repeat_unit, CGbqualType::e_Map );
    Associate( CSeqFeatData::eSubtype_repeat_unit, CGbqualType::e_Note );
    Associate( CSeqFeatData::eSubtype_repeat_unit, CGbqualType::e_Old_locus_tag );
    Associate( CSeqFeatData::eSubtype_repeat_unit, CGbqualType::e_Partial );
    Associate( CSeqFeatData::eSubtype_repeat_unit, CGbqualType::e_Rpt_family );
    Associate( CSeqFeatData::eSubtype_repeat_unit, CGbqualType::e_Rpt_type );
    Associate( CSeqFeatData::eSubtype_repeat_unit, CGbqualType::e_Rpt_unit );
    Associate( CSeqFeatData::eSubtype_repeat_unit, CGbqualType::e_Rpt_unit_range );
    Associate( CSeqFeatData::eSubtype_repeat_unit, CGbqualType::e_Rpt_unit_seq );
    Associate( CSeqFeatData::eSubtype_repeat_unit, CGbqualType::e_Usedin );

    //rep_origin
    Associate( CSeqFeatData::eSubtype_rep_origin, CGbqualType::e_Allele );
    Associate( CSeqFeatData::eSubtype_rep_origin, CGbqualType::e_Citation );
    Associate( CSeqFeatData::eSubtype_rep_origin, CGbqualType::e_Db_xref );
    Associate( CSeqFeatData::eSubtype_rep_origin, CGbqualType::e_Direction );
    Associate( CSeqFeatData::eSubtype_rep_origin, CGbqualType::e_Evidence );
    Associate( CSeqFeatData::eSubtype_rep_origin, CGbqualType::e_Experiment );
    Associate( CSeqFeatData::eSubtype_rep_origin, CGbqualType::e_Gene );
    Associate( CSeqFeatData::eSubtype_rep_origin, CGbqualType::e_Gene_synonym );
    Associate( CSeqFeatData::eSubtype_rep_origin, CGbqualType::e_Inference );
    Associate( CSeqFeatData::eSubtype_rep_origin, CGbqualType::e_Label );
    Associate( CSeqFeatData::eSubtype_rep_origin, CGbqualType::e_Locus_tag );
    Associate( CSeqFeatData::eSubtype_rep_origin, CGbqualType::e_Map );
    Associate( CSeqFeatData::eSubtype_rep_origin, CGbqualType::e_Note );
    Associate( CSeqFeatData::eSubtype_rep_origin, CGbqualType::e_Old_locus_tag );
    Associate( CSeqFeatData::eSubtype_rep_origin, CGbqualType::e_Partial );
    Associate( CSeqFeatData::eSubtype_rep_origin, CGbqualType::e_Standard_name );
    Associate( CSeqFeatData::eSubtype_rep_origin, CGbqualType::e_Usedin );

    //rRNA
    Associate( CSeqFeatData::eSubtype_rRNA, CGbqualType::e_Allele );
    Associate( CSeqFeatData::eSubtype_rRNA, CGbqualType::e_Citation );
    Associate( CSeqFeatData::eSubtype_rRNA, CGbqualType::e_Db_xref );
    Associate( CSeqFeatData::eSubtype_rRNA, CGbqualType::e_Evidence );
    Associate( CSeqFeatData::eSubtype_rRNA, CGbqualType::e_Experiment );
    Associate( CSeqFeatData::eSubtype_rRNA, CGbqualType::e_Function );
    Associate( CSeqFeatData::eSubtype_rRNA, CGbqualType::e_Gene );
    Associate( CSeqFeatData::eSubtype_rRNA, CGbqualType::e_Gene_synonym );
    Associate( CSeqFeatData::eSubtype_rRNA, CGbqualType::e_Inference );
    Associate( CSeqFeatData::eSubtype_rRNA, CGbqualType::e_Label );
    Associate( CSeqFeatData::eSubtype_rRNA, CGbqualType::e_Locus_tag );
    Associate( CSeqFeatData::eSubtype_rRNA, CGbqualType::e_Map );
    Associate( CSeqFeatData::eSubtype_rRNA, CGbqualType::e_Note );
    Associate( CSeqFeatData::eSubtype_rRNA, CGbqualType::e_Old_locus_tag );
    Associate( CSeqFeatData::eSubtype_rRNA, CGbqualType::e_Operon );
    Associate( CSeqFeatData::eSubtype_rRNA, CGbqualType::e_Partial );
    Associate( CSeqFeatData::eSubtype_rRNA, CGbqualType::e_Product );
    Associate( CSeqFeatData::eSubtype_rRNA, CGbqualType::e_Pseudo );
    Associate( CSeqFeatData::eSubtype_rRNA, CGbqualType::e_Standard_name );
    Associate( CSeqFeatData::eSubtype_rRNA, CGbqualType::e_Usedin );

    //S_region
    Associate( CSeqFeatData::eSubtype_S_region, CGbqualType::e_Allele );
    Associate( CSeqFeatData::eSubtype_S_region, CGbqualType::e_Citation );
    Associate( CSeqFeatData::eSubtype_S_region, CGbqualType::e_Db_xref );
    Associate( CSeqFeatData::eSubtype_S_region, CGbqualType::e_Evidence );
    Associate( CSeqFeatData::eSubtype_S_region, CGbqualType::e_Experiment );
    Associate( CSeqFeatData::eSubtype_S_region, CGbqualType::e_Gene );
    Associate( CSeqFeatData::eSubtype_S_region, CGbqualType::e_Gene_synonym );
    Associate( CSeqFeatData::eSubtype_S_region, CGbqualType::e_Inference );
    Associate( CSeqFeatData::eSubtype_S_region, CGbqualType::e_Label );
    Associate( CSeqFeatData::eSubtype_S_region, CGbqualType::e_Locus_tag );
    Associate( CSeqFeatData::eSubtype_S_region, CGbqualType::e_Map );
    Associate( CSeqFeatData::eSubtype_S_region, CGbqualType::e_Note );
    Associate( CSeqFeatData::eSubtype_S_region, CGbqualType::e_Old_locus_tag );
    Associate( CSeqFeatData::eSubtype_S_region, CGbqualType::e_Partial );
    Associate( CSeqFeatData::eSubtype_S_region, CGbqualType::e_Product );
    Associate( CSeqFeatData::eSubtype_S_region, CGbqualType::e_Pseudo );
    Associate( CSeqFeatData::eSubtype_S_region, CGbqualType::e_Standard_name );
    Associate( CSeqFeatData::eSubtype_S_region, CGbqualType::e_Usedin );

    //satellite
    Associate( CSeqFeatData::eSubtype_satellite, CGbqualType::e_Allele );
    Associate( CSeqFeatData::eSubtype_satellite, CGbqualType::e_Citation );
    Associate( CSeqFeatData::eSubtype_satellite, CGbqualType::e_Db_xref );
    Associate( CSeqFeatData::eSubtype_satellite, CGbqualType::e_Evidence );
    Associate( CSeqFeatData::eSubtype_satellite, CGbqualType::e_Experiment );
    Associate( CSeqFeatData::eSubtype_satellite, CGbqualType::e_Gene );
    Associate( CSeqFeatData::eSubtype_satellite, CGbqualType::e_Gene_synonym );
    Associate( CSeqFeatData::eSubtype_satellite, CGbqualType::e_Inference );
    Associate( CSeqFeatData::eSubtype_satellite, CGbqualType::e_Label );
    Associate( CSeqFeatData::eSubtype_satellite, CGbqualType::e_Locus_tag );
    Associate( CSeqFeatData::eSubtype_satellite, CGbqualType::e_Map );
    Associate( CSeqFeatData::eSubtype_satellite, CGbqualType::e_Note );
    Associate( CSeqFeatData::eSubtype_satellite, CGbqualType::e_Old_locus_tag );
    Associate( CSeqFeatData::eSubtype_satellite, CGbqualType::e_Partial );
    Associate( CSeqFeatData::eSubtype_satellite, CGbqualType::e_Rpt_family );
    Associate( CSeqFeatData::eSubtype_satellite, CGbqualType::e_Rpt_type );
    Associate( CSeqFeatData::eSubtype_satellite, CGbqualType::e_Rpt_unit );
    Associate( CSeqFeatData::eSubtype_satellite, CGbqualType::e_Rpt_unit_range );
    Associate( CSeqFeatData::eSubtype_satellite, CGbqualType::e_Rpt_unit_seq );
    Associate( CSeqFeatData::eSubtype_satellite, CGbqualType::e_Standard_name );
    Associate( CSeqFeatData::eSubtype_satellite, CGbqualType::e_Usedin );

    //scRNA
    Associate( CSeqFeatData::eSubtype_scRNA, CGbqualType::e_Allele );
    Associate( CSeqFeatData::eSubtype_scRNA, CGbqualType::e_Citation );
    Associate( CSeqFeatData::eSubtype_scRNA, CGbqualType::e_Db_xref );
    Associate( CSeqFeatData::eSubtype_scRNA, CGbqualType::e_Evidence );
    Associate( CSeqFeatData::eSubtype_scRNA, CGbqualType::e_Experiment );
    Associate( CSeqFeatData::eSubtype_scRNA, CGbqualType::e_Function );
    Associate( CSeqFeatData::eSubtype_scRNA, CGbqualType::e_Gene );
    Associate( CSeqFeatData::eSubtype_scRNA, CGbqualType::e_Gene_synonym );
    Associate( CSeqFeatData::eSubtype_scRNA, CGbqualType::e_Inference );
    Associate( CSeqFeatData::eSubtype_scRNA, CGbqualType::e_Label );
    Associate( CSeqFeatData::eSubtype_scRNA, CGbqualType::e_Locus_tag );
    Associate( CSeqFeatData::eSubtype_scRNA, CGbqualType::e_Map );
    Associate( CSeqFeatData::eSubtype_scRNA, CGbqualType::e_Note );
    Associate( CSeqFeatData::eSubtype_scRNA, CGbqualType::e_Old_locus_tag );
    Associate( CSeqFeatData::eSubtype_scRNA, CGbqualType::e_Partial );
    Associate( CSeqFeatData::eSubtype_scRNA, CGbqualType::e_Product );
    Associate( CSeqFeatData::eSubtype_scRNA, CGbqualType::e_Pseudo );
    Associate( CSeqFeatData::eSubtype_scRNA, CGbqualType::e_Standard_name );
    Associate( CSeqFeatData::eSubtype_scRNA, CGbqualType::e_Usedin );

    //sig_peptide
    Associate( CSeqFeatData::eSubtype_sig_peptide, CGbqualType::e_Allele );
    Associate( CSeqFeatData::eSubtype_sig_peptide, CGbqualType::e_Citation );
    Associate( CSeqFeatData::eSubtype_sig_peptide, CGbqualType::e_Db_xref );
    Associate( CSeqFeatData::eSubtype_sig_peptide, CGbqualType::e_Evidence );
    Associate( CSeqFeatData::eSubtype_sig_peptide, CGbqualType::e_Experiment );
    Associate( CSeqFeatData::eSubtype_sig_peptide, CGbqualType::e_Function );
    Associate( CSeqFeatData::eSubtype_sig_peptide, CGbqualType::e_Gene );
    Associate( CSeqFeatData::eSubtype_sig_peptide, CGbqualType::e_Gene_synonym );
    Associate( CSeqFeatData::eSubtype_sig_peptide, CGbqualType::e_Inference );
    Associate( CSeqFeatData::eSubtype_sig_peptide, CGbqualType::e_Label );
    Associate( CSeqFeatData::eSubtype_sig_peptide, CGbqualType::e_Locus_tag );
    Associate( CSeqFeatData::eSubtype_sig_peptide, CGbqualType::e_Map );
    Associate( CSeqFeatData::eSubtype_sig_peptide, CGbqualType::e_Note );
    Associate( CSeqFeatData::eSubtype_sig_peptide, CGbqualType::e_Old_locus_tag );
    Associate( CSeqFeatData::eSubtype_sig_peptide, CGbqualType::e_Partial );
    Associate( CSeqFeatData::eSubtype_sig_peptide, CGbqualType::e_Product );
    Associate( CSeqFeatData::eSubtype_sig_peptide, CGbqualType::e_Pseudo );
    Associate( CSeqFeatData::eSubtype_sig_peptide, CGbqualType::e_Standard_name );
    Associate( CSeqFeatData::eSubtype_sig_peptide, CGbqualType::e_Usedin );

    //snoRNA
    Associate( CSeqFeatData::eSubtype_snoRNA, CGbqualType::e_Allele );
    Associate( CSeqFeatData::eSubtype_snoRNA, CGbqualType::e_Citation );
    Associate( CSeqFeatData::eSubtype_snoRNA, CGbqualType::e_Db_xref );
    Associate( CSeqFeatData::eSubtype_snoRNA, CGbqualType::e_Evidence );
    Associate( CSeqFeatData::eSubtype_snoRNA, CGbqualType::e_Experiment );
    Associate( CSeqFeatData::eSubtype_snoRNA, CGbqualType::e_Function );
    Associate( CSeqFeatData::eSubtype_snoRNA, CGbqualType::e_Gene );
    Associate( CSeqFeatData::eSubtype_snoRNA, CGbqualType::e_Gene_synonym );
    Associate( CSeqFeatData::eSubtype_snoRNA, CGbqualType::e_Inference );
    Associate( CSeqFeatData::eSubtype_snoRNA, CGbqualType::e_Label );
    Associate( CSeqFeatData::eSubtype_snoRNA, CGbqualType::e_Locus_tag );
    Associate( CSeqFeatData::eSubtype_snoRNA, CGbqualType::e_Map );
    Associate( CSeqFeatData::eSubtype_snoRNA, CGbqualType::e_Note );
    Associate( CSeqFeatData::eSubtype_snoRNA, CGbqualType::e_Old_locus_tag );
    Associate( CSeqFeatData::eSubtype_snoRNA, CGbqualType::e_Partial );
    Associate( CSeqFeatData::eSubtype_snoRNA, CGbqualType::e_Product );
    Associate( CSeqFeatData::eSubtype_snoRNA, CGbqualType::e_Pseudo );
    Associate( CSeqFeatData::eSubtype_snoRNA, CGbqualType::e_Standard_name );
    Associate( CSeqFeatData::eSubtype_snoRNA, CGbqualType::e_Usedin );

    //snRNA
    Associate( CSeqFeatData::eSubtype_snRNA, CGbqualType::e_Allele );
    Associate( CSeqFeatData::eSubtype_snRNA, CGbqualType::e_Citation );
    Associate( CSeqFeatData::eSubtype_snRNA, CGbqualType::e_Db_xref );
    Associate( CSeqFeatData::eSubtype_snRNA, CGbqualType::e_Evidence );
    Associate( CSeqFeatData::eSubtype_snRNA, CGbqualType::e_Experiment );
    Associate( CSeqFeatData::eSubtype_snRNA, CGbqualType::e_Function );
    Associate( CSeqFeatData::eSubtype_snRNA, CGbqualType::e_Gene );
    Associate( CSeqFeatData::eSubtype_snRNA, CGbqualType::e_Gene_synonym );
    Associate( CSeqFeatData::eSubtype_snRNA, CGbqualType::e_Inference );
    Associate( CSeqFeatData::eSubtype_snRNA, CGbqualType::e_Label );
    Associate( CSeqFeatData::eSubtype_snRNA, CGbqualType::e_Locus_tag );
    Associate( CSeqFeatData::eSubtype_snRNA, CGbqualType::e_Map );
    Associate( CSeqFeatData::eSubtype_snRNA, CGbqualType::e_Note );
    Associate( CSeqFeatData::eSubtype_snRNA, CGbqualType::e_Old_locus_tag );
    Associate( CSeqFeatData::eSubtype_snRNA, CGbqualType::e_Partial );
    Associate( CSeqFeatData::eSubtype_snRNA, CGbqualType::e_Product );
    Associate( CSeqFeatData::eSubtype_snRNA, CGbqualType::e_Pseudo );
    Associate( CSeqFeatData::eSubtype_snRNA, CGbqualType::e_Standard_name );
    Associate( CSeqFeatData::eSubtype_snRNA, CGbqualType::e_Usedin );

    //source
    Associate( CSeqFeatData::eSubtype_source, CGbqualType::e_Organism );
    Associate( CSeqFeatData::eSubtype_source, CGbqualType::e_Bio_material );
    Associate( CSeqFeatData::eSubtype_source, CGbqualType::e_Cell_line );
    Associate( CSeqFeatData::eSubtype_source, CGbqualType::e_Cell_type );
    Associate( CSeqFeatData::eSubtype_source, CGbqualType::e_Chloroplast );
    Associate( CSeqFeatData::eSubtype_source, CGbqualType::e_Chromoplast );
    Associate( CSeqFeatData::eSubtype_source, CGbqualType::e_Chromosome );
    Associate( CSeqFeatData::eSubtype_source, CGbqualType::e_Citation );
    Associate( CSeqFeatData::eSubtype_source, CGbqualType::e_Clone_lib );
    Associate( CSeqFeatData::eSubtype_source, CGbqualType::e_Clone );
    Associate( CSeqFeatData::eSubtype_source, CGbqualType::e_Collected_by );
    Associate( CSeqFeatData::eSubtype_source, CGbqualType::e_Collection_date );
    Associate( CSeqFeatData::eSubtype_source, CGbqualType::e_Country );
    Associate( CSeqFeatData::eSubtype_source, CGbqualType::e_Cultivar );
    Associate( CSeqFeatData::eSubtype_source, CGbqualType::e_Culture_collection );
    Associate( CSeqFeatData::eSubtype_source, CGbqualType::e_Cyanelle );
    Associate( CSeqFeatData::eSubtype_source, CGbqualType::e_Db_xref );
    Associate( CSeqFeatData::eSubtype_source, CGbqualType::e_Dev_stage );
    Associate( CSeqFeatData::eSubtype_source, CGbqualType::e_Ecotype );
    Associate( CSeqFeatData::eSubtype_source, CGbqualType::e_Environmental_sample );
    Associate( CSeqFeatData::eSubtype_source, CGbqualType::e_Focus );
    Associate( CSeqFeatData::eSubtype_source, CGbqualType::e_Frequency );
    Associate( CSeqFeatData::eSubtype_source, CGbqualType::e_Germline );
    Associate( CSeqFeatData::eSubtype_source, CGbqualType::e_Haplotype );
    Associate( CSeqFeatData::eSubtype_source, CGbqualType::e_Identified_by );
    Associate( CSeqFeatData::eSubtype_source, CGbqualType::e_Insertion_seq );
    Associate( CSeqFeatData::eSubtype_source, CGbqualType::e_Isolate );
    Associate( CSeqFeatData::eSubtype_source, CGbqualType::e_Isolation_source );
    Associate( CSeqFeatData::eSubtype_source, CGbqualType::e_Kinetoplast );
    Associate( CSeqFeatData::eSubtype_source, CGbqualType::e_Lab_host );
    Associate( CSeqFeatData::eSubtype_source, CGbqualType::e_Label );
    Associate( CSeqFeatData::eSubtype_source, CGbqualType::e_Lat_lon );
    Associate( CSeqFeatData::eSubtype_source, CGbqualType::e_Macronuclear );
    Associate( CSeqFeatData::eSubtype_source, CGbqualType::e_Map );
    Associate( CSeqFeatData::eSubtype_source, CGbqualType::e_Mating_type );
    Associate( CSeqFeatData::eSubtype_source, CGbqualType::e_Metagenomic );
    Associate( CSeqFeatData::eSubtype_source, CGbqualType::e_Mitochondrion );
    Associate( CSeqFeatData::eSubtype_source, CGbqualType::e_Mol_type );
    Associate( CSeqFeatData::eSubtype_source, CGbqualType::e_Note );
    Associate( CSeqFeatData::eSubtype_source, CGbqualType::e_Organelle );
    Associate( CSeqFeatData::eSubtype_source, CGbqualType::e_PCR_primers );
    Associate( CSeqFeatData::eSubtype_source, CGbqualType::e_Plasmid );
    Associate( CSeqFeatData::eSubtype_source, CGbqualType::e_Pop_variant );
    Associate( CSeqFeatData::eSubtype_source, CGbqualType::e_Proviral );
    Associate( CSeqFeatData::eSubtype_source, CGbqualType::e_Rearranged );
    Associate( CSeqFeatData::eSubtype_source, CGbqualType::e_Segment );
    Associate( CSeqFeatData::eSubtype_source, CGbqualType::e_Sequenced_mol );
    Associate( CSeqFeatData::eSubtype_source, CGbqualType::e_Serotype );
    Associate( CSeqFeatData::eSubtype_source, CGbqualType::e_Serovar );
    Associate( CSeqFeatData::eSubtype_source, CGbqualType::e_Sex );
    Associate( CSeqFeatData::eSubtype_source, CGbqualType::e_Specimen_voucher );
    Associate( CSeqFeatData::eSubtype_source, CGbqualType::e_Strain );
    Associate( CSeqFeatData::eSubtype_source, CGbqualType::e_Sub_clone );
    Associate( CSeqFeatData::eSubtype_source, CGbqualType::e_Sub_species );
    Associate( CSeqFeatData::eSubtype_source, CGbqualType::e_Sub_strain );
    Associate( CSeqFeatData::eSubtype_source, CGbqualType::e_Tissue_lib );
    Associate( CSeqFeatData::eSubtype_source, CGbqualType::e_Tissue_type );
    Associate( CSeqFeatData::eSubtype_source, CGbqualType::e_Transgenic );
    Associate( CSeqFeatData::eSubtype_source, CGbqualType::e_Transposon );
    Associate( CSeqFeatData::eSubtype_source, CGbqualType::e_Usedin );
    Associate( CSeqFeatData::eSubtype_source, CGbqualType::e_Variety );
    Associate( CSeqFeatData::eSubtype_source, CGbqualType::e_Virion );

    //stem_loop
    Associate( CSeqFeatData::eSubtype_stem_loop, CGbqualType::e_Allele );
    Associate( CSeqFeatData::eSubtype_stem_loop, CGbqualType::e_Citation );
    Associate( CSeqFeatData::eSubtype_stem_loop, CGbqualType::e_Db_xref );
    Associate( CSeqFeatData::eSubtype_stem_loop, CGbqualType::e_Evidence );
    Associate( CSeqFeatData::eSubtype_stem_loop, CGbqualType::e_Experiment );
    Associate( CSeqFeatData::eSubtype_stem_loop, CGbqualType::e_Function );
    Associate( CSeqFeatData::eSubtype_stem_loop, CGbqualType::e_Gene );
    Associate( CSeqFeatData::eSubtype_stem_loop, CGbqualType::e_Gene_synonym );
    Associate( CSeqFeatData::eSubtype_stem_loop, CGbqualType::e_Inference );
    Associate( CSeqFeatData::eSubtype_stem_loop, CGbqualType::e_Label );
    Associate( CSeqFeatData::eSubtype_stem_loop, CGbqualType::e_Locus_tag );
    Associate( CSeqFeatData::eSubtype_stem_loop, CGbqualType::e_Map );
    Associate( CSeqFeatData::eSubtype_stem_loop, CGbqualType::e_Note );
    Associate( CSeqFeatData::eSubtype_stem_loop, CGbqualType::e_Old_locus_tag );
    Associate( CSeqFeatData::eSubtype_stem_loop, CGbqualType::e_Operon );
    Associate( CSeqFeatData::eSubtype_stem_loop, CGbqualType::e_Partial );
    Associate( CSeqFeatData::eSubtype_stem_loop, CGbqualType::e_Standard_name );
    Associate( CSeqFeatData::eSubtype_stem_loop, CGbqualType::e_Usedin );

    //STS
    Associate( CSeqFeatData::eSubtype_STS, CGbqualType::e_Allele );
    Associate( CSeqFeatData::eSubtype_STS, CGbqualType::e_Citation );
    Associate( CSeqFeatData::eSubtype_STS, CGbqualType::e_Db_xref );
    Associate( CSeqFeatData::eSubtype_STS, CGbqualType::e_Evidence );
    Associate( CSeqFeatData::eSubtype_STS, CGbqualType::e_Experiment );
    Associate( CSeqFeatData::eSubtype_STS, CGbqualType::e_Gene );
    Associate( CSeqFeatData::eSubtype_STS, CGbqualType::e_Gene_synonym );
    Associate( CSeqFeatData::eSubtype_STS, CGbqualType::e_Inference );
    Associate( CSeqFeatData::eSubtype_STS, CGbqualType::e_Label );
    Associate( CSeqFeatData::eSubtype_STS, CGbqualType::e_Locus_tag );
    Associate( CSeqFeatData::eSubtype_STS, CGbqualType::e_Map );
    Associate( CSeqFeatData::eSubtype_STS, CGbqualType::e_Note );
    Associate( CSeqFeatData::eSubtype_STS, CGbqualType::e_Old_locus_tag );
    Associate( CSeqFeatData::eSubtype_STS, CGbqualType::e_Partial );
    Associate( CSeqFeatData::eSubtype_STS, CGbqualType::e_Standard_name );
    Associate( CSeqFeatData::eSubtype_STS, CGbqualType::e_Usedin );

    //TATA_signal
    Associate( CSeqFeatData::eSubtype_TATA_signal, CGbqualType::e_Allele );
    Associate( CSeqFeatData::eSubtype_TATA_signal, CGbqualType::e_Citation );
    Associate( CSeqFeatData::eSubtype_TATA_signal, CGbqualType::e_Db_xref );
    Associate( CSeqFeatData::eSubtype_TATA_signal, CGbqualType::e_Evidence );
    Associate( CSeqFeatData::eSubtype_TATA_signal, CGbqualType::e_Experiment );
    Associate( CSeqFeatData::eSubtype_TATA_signal, CGbqualType::e_Gene );
    Associate( CSeqFeatData::eSubtype_TATA_signal, CGbqualType::e_Gene_synonym );
    Associate( CSeqFeatData::eSubtype_TATA_signal, CGbqualType::e_Inference );
    Associate( CSeqFeatData::eSubtype_TATA_signal, CGbqualType::e_Label );
    Associate( CSeqFeatData::eSubtype_TATA_signal, CGbqualType::e_Locus_tag );
    Associate( CSeqFeatData::eSubtype_TATA_signal, CGbqualType::e_Map );
    Associate( CSeqFeatData::eSubtype_TATA_signal, CGbqualType::e_Note );
    Associate( CSeqFeatData::eSubtype_TATA_signal, CGbqualType::e_Old_locus_tag );
    Associate( CSeqFeatData::eSubtype_TATA_signal, CGbqualType::e_Partial );
    Associate( CSeqFeatData::eSubtype_TATA_signal, CGbqualType::e_Usedin );

    //terminator
    Associate( CSeqFeatData::eSubtype_terminator, CGbqualType::e_Allele );
    Associate( CSeqFeatData::eSubtype_terminator, CGbqualType::e_Citation );
    Associate( CSeqFeatData::eSubtype_terminator, CGbqualType::e_Db_xref );
    Associate( CSeqFeatData::eSubtype_terminator, CGbqualType::e_Evidence );
    Associate( CSeqFeatData::eSubtype_terminator, CGbqualType::e_Experiment );
    Associate( CSeqFeatData::eSubtype_terminator, CGbqualType::e_Gene );
    Associate( CSeqFeatData::eSubtype_terminator, CGbqualType::e_Gene_synonym );
    Associate( CSeqFeatData::eSubtype_terminator, CGbqualType::e_Inference );
    Associate( CSeqFeatData::eSubtype_terminator, CGbqualType::e_Label );
    Associate( CSeqFeatData::eSubtype_terminator, CGbqualType::e_Locus_tag );
    Associate( CSeqFeatData::eSubtype_terminator, CGbqualType::e_Map );
    Associate( CSeqFeatData::eSubtype_terminator, CGbqualType::e_Note );
    Associate( CSeqFeatData::eSubtype_terminator, CGbqualType::e_Old_locus_tag );
    Associate( CSeqFeatData::eSubtype_terminator, CGbqualType::e_Operon );
    Associate( CSeqFeatData::eSubtype_terminator, CGbqualType::e_Partial );
    Associate( CSeqFeatData::eSubtype_terminator, CGbqualType::e_Standard_name );
    Associate( CSeqFeatData::eSubtype_terminator, CGbqualType::e_Usedin );

    //tmRNA
    Associate( CSeqFeatData::eSubtype_tmRNA, CGbqualType::e_Allele );
    Associate( CSeqFeatData::eSubtype_tmRNA, CGbqualType::e_Citation );
    Associate( CSeqFeatData::eSubtype_tmRNA, CGbqualType::e_Db_xref );
    Associate( CSeqFeatData::eSubtype_tmRNA, CGbqualType::e_Evidence );
    Associate( CSeqFeatData::eSubtype_tmRNA, CGbqualType::e_Experiment );
    Associate( CSeqFeatData::eSubtype_tmRNA, CGbqualType::e_Function );
    Associate( CSeqFeatData::eSubtype_tmRNA, CGbqualType::e_Gene );
    Associate( CSeqFeatData::eSubtype_tmRNA, CGbqualType::e_Gene_synonym );
    Associate( CSeqFeatData::eSubtype_tmRNA, CGbqualType::e_Inference );
    Associate( CSeqFeatData::eSubtype_tmRNA, CGbqualType::e_Label );
    Associate( CSeqFeatData::eSubtype_tmRNA, CGbqualType::e_Locus_tag );
    Associate( CSeqFeatData::eSubtype_tmRNA, CGbqualType::e_Map );
    Associate( CSeqFeatData::eSubtype_tmRNA, CGbqualType::e_Note );
    Associate( CSeqFeatData::eSubtype_tmRNA, CGbqualType::e_Old_locus_tag );
    Associate( CSeqFeatData::eSubtype_tmRNA, CGbqualType::e_Operon );
    Associate( CSeqFeatData::eSubtype_tmRNA, CGbqualType::e_Partial );
    Associate( CSeqFeatData::eSubtype_tmRNA, CGbqualType::e_Product );
    Associate( CSeqFeatData::eSubtype_tmRNA, CGbqualType::e_Pseudo );
    Associate( CSeqFeatData::eSubtype_tmRNA, CGbqualType::e_Standard_name );
    Associate( CSeqFeatData::eSubtype_tmRNA, CGbqualType::e_Tag_peptide );
    Associate( CSeqFeatData::eSubtype_tmRNA, CGbqualType::e_Usedin );

    //transit_peptide
    Associate( CSeqFeatData::eSubtype_transit_peptide, CGbqualType::e_Allele );
    Associate( CSeqFeatData::eSubtype_transit_peptide, CGbqualType::e_Citation );
    Associate( CSeqFeatData::eSubtype_transit_peptide, CGbqualType::e_Db_xref );
    Associate( CSeqFeatData::eSubtype_transit_peptide, CGbqualType::e_Evidence );
    Associate( CSeqFeatData::eSubtype_transit_peptide, CGbqualType::e_Experiment );
    Associate( CSeqFeatData::eSubtype_transit_peptide, CGbqualType::e_Function );
    Associate( CSeqFeatData::eSubtype_transit_peptide, CGbqualType::e_Gene );
    Associate( CSeqFeatData::eSubtype_transit_peptide, CGbqualType::e_Gene_synonym );
    Associate( CSeqFeatData::eSubtype_transit_peptide, CGbqualType::e_Inference );
    Associate( CSeqFeatData::eSubtype_transit_peptide, CGbqualType::e_Label );
    Associate( CSeqFeatData::eSubtype_transit_peptide, CGbqualType::e_Locus_tag );
    Associate( CSeqFeatData::eSubtype_transit_peptide, CGbqualType::e_Map );
    Associate( CSeqFeatData::eSubtype_transit_peptide, CGbqualType::e_Note );
    Associate( CSeqFeatData::eSubtype_transit_peptide, CGbqualType::e_Old_locus_tag );
    Associate( CSeqFeatData::eSubtype_transit_peptide, CGbqualType::e_Partial );
    Associate( CSeqFeatData::eSubtype_transit_peptide, CGbqualType::e_Product );
    Associate( CSeqFeatData::eSubtype_transit_peptide, CGbqualType::e_Pseudo );
    Associate( CSeqFeatData::eSubtype_transit_peptide, CGbqualType::e_Standard_name );
    Associate( CSeqFeatData::eSubtype_transit_peptide, CGbqualType::e_Usedin );

    //tRNA
    Associate( CSeqFeatData::eSubtype_tRNA, CGbqualType::e_Allele );
    Associate( CSeqFeatData::eSubtype_tRNA, CGbqualType::e_Anticodon );
    Associate( CSeqFeatData::eSubtype_tRNA, CGbqualType::e_Citation );
    Associate( CSeqFeatData::eSubtype_tRNA, CGbqualType::e_Db_xref );
    Associate( CSeqFeatData::eSubtype_tRNA, CGbqualType::e_Evidence );
    Associate( CSeqFeatData::eSubtype_tRNA, CGbqualType::e_Experiment );
    Associate( CSeqFeatData::eSubtype_tRNA, CGbqualType::e_Function );
    Associate( CSeqFeatData::eSubtype_tRNA, CGbqualType::e_Gene );
    Associate( CSeqFeatData::eSubtype_tRNA, CGbqualType::e_Gene_synonym );
    Associate( CSeqFeatData::eSubtype_tRNA, CGbqualType::e_Inference );
    Associate( CSeqFeatData::eSubtype_tRNA, CGbqualType::e_Label );
    Associate( CSeqFeatData::eSubtype_tRNA, CGbqualType::e_Locus_tag );
    Associate( CSeqFeatData::eSubtype_tRNA, CGbqualType::e_Map );
    Associate( CSeqFeatData::eSubtype_tRNA, CGbqualType::e_Note );
    Associate( CSeqFeatData::eSubtype_tRNA, CGbqualType::e_Old_locus_tag );
    Associate( CSeqFeatData::eSubtype_tRNA, CGbqualType::e_Partial );
    Associate( CSeqFeatData::eSubtype_tRNA, CGbqualType::e_Product );
    Associate( CSeqFeatData::eSubtype_tRNA, CGbqualType::e_Pseudo );
    Associate( CSeqFeatData::eSubtype_tRNA, CGbqualType::e_Standard_name );
    Associate( CSeqFeatData::eSubtype_tRNA, CGbqualType::e_Trans_splicing );
    Associate( CSeqFeatData::eSubtype_tRNA, CGbqualType::e_Usedin );

    //unsure
    Associate( CSeqFeatData::eSubtype_unsure, CGbqualType::e_Allele );
    Associate( CSeqFeatData::eSubtype_unsure, CGbqualType::e_Citation );
    Associate( CSeqFeatData::eSubtype_unsure, CGbqualType::e_Db_xref );
    Associate( CSeqFeatData::eSubtype_unsure, CGbqualType::e_Evidence );
    Associate( CSeqFeatData::eSubtype_unsure, CGbqualType::e_Experiment );
    Associate( CSeqFeatData::eSubtype_unsure, CGbqualType::e_Gene );
    Associate( CSeqFeatData::eSubtype_unsure, CGbqualType::e_Gene_synonym );
    Associate( CSeqFeatData::eSubtype_unsure, CGbqualType::e_Inference );
    Associate( CSeqFeatData::eSubtype_unsure, CGbqualType::e_Label );
    Associate( CSeqFeatData::eSubtype_unsure, CGbqualType::e_Locus_tag );
    Associate( CSeqFeatData::eSubtype_unsure, CGbqualType::e_Map );
    Associate( CSeqFeatData::eSubtype_unsure, CGbqualType::e_Note );
    Associate( CSeqFeatData::eSubtype_unsure, CGbqualType::e_Old_locus_tag );
    Associate( CSeqFeatData::eSubtype_unsure, CGbqualType::e_Replace );
    Associate( CSeqFeatData::eSubtype_unsure, CGbqualType::e_Usedin );

    //V_region
    Associate( CSeqFeatData::eSubtype_V_region, CGbqualType::e_Allele );
    Associate( CSeqFeatData::eSubtype_V_region, CGbqualType::e_Citation );
    Associate( CSeqFeatData::eSubtype_V_region, CGbqualType::e_Db_xref );
    Associate( CSeqFeatData::eSubtype_V_region, CGbqualType::e_Evidence );
    Associate( CSeqFeatData::eSubtype_V_region, CGbqualType::e_Experiment );
    Associate( CSeqFeatData::eSubtype_V_region, CGbqualType::e_Gene );
    Associate( CSeqFeatData::eSubtype_V_region, CGbqualType::e_Gene_synonym );
    Associate( CSeqFeatData::eSubtype_V_region, CGbqualType::e_Inference );
    Associate( CSeqFeatData::eSubtype_V_region, CGbqualType::e_Label );
    Associate( CSeqFeatData::eSubtype_V_region, CGbqualType::e_Locus_tag );
    Associate( CSeqFeatData::eSubtype_V_region, CGbqualType::e_Map );
    Associate( CSeqFeatData::eSubtype_V_region, CGbqualType::e_Note );
    Associate( CSeqFeatData::eSubtype_V_region, CGbqualType::e_Old_locus_tag );
    Associate( CSeqFeatData::eSubtype_V_region, CGbqualType::e_Partial );
    Associate( CSeqFeatData::eSubtype_V_region, CGbqualType::e_Product );
    Associate( CSeqFeatData::eSubtype_V_region, CGbqualType::e_Pseudo );
    Associate( CSeqFeatData::eSubtype_V_region, CGbqualType::e_Standard_name );
    Associate( CSeqFeatData::eSubtype_V_region, CGbqualType::e_Usedin );

    //V_segment
    Associate( CSeqFeatData::eSubtype_V_segment, CGbqualType::e_Allele );
    Associate( CSeqFeatData::eSubtype_V_segment, CGbqualType::e_Citation );
    Associate( CSeqFeatData::eSubtype_V_segment, CGbqualType::e_Db_xref );
    Associate( CSeqFeatData::eSubtype_V_segment, CGbqualType::e_Evidence );
    Associate( CSeqFeatData::eSubtype_V_segment, CGbqualType::e_Experiment );
    Associate( CSeqFeatData::eSubtype_V_segment, CGbqualType::e_Gene );
    Associate( CSeqFeatData::eSubtype_V_segment, CGbqualType::e_Gene_synonym );
    Associate( CSeqFeatData::eSubtype_V_segment, CGbqualType::e_Inference );
    Associate( CSeqFeatData::eSubtype_V_segment, CGbqualType::e_Label );
    Associate( CSeqFeatData::eSubtype_V_segment, CGbqualType::e_Locus_tag );
    Associate( CSeqFeatData::eSubtype_V_segment, CGbqualType::e_Map );
    Associate( CSeqFeatData::eSubtype_V_segment, CGbqualType::e_Note );
    Associate( CSeqFeatData::eSubtype_V_segment, CGbqualType::e_Old_locus_tag );
    Associate( CSeqFeatData::eSubtype_V_segment, CGbqualType::e_Partial );
    Associate( CSeqFeatData::eSubtype_V_segment, CGbqualType::e_Product );
    Associate( CSeqFeatData::eSubtype_V_segment, CGbqualType::e_Pseudo );
    Associate( CSeqFeatData::eSubtype_V_segment, CGbqualType::e_Standard_name );
    Associate( CSeqFeatData::eSubtype_V_segment, CGbqualType::e_Usedin );

    //variation
    Associate( CSeqFeatData::eSubtype_variation, CGbqualType::e_Allele );
    Associate( CSeqFeatData::eSubtype_variation, CGbqualType::e_Citation );
    Associate( CSeqFeatData::eSubtype_variation, CGbqualType::e_Compare );
    Associate( CSeqFeatData::eSubtype_variation, CGbqualType::e_Db_xref );
    Associate( CSeqFeatData::eSubtype_variation, CGbqualType::e_Evidence );
    Associate( CSeqFeatData::eSubtype_variation, CGbqualType::e_Experiment );
    Associate( CSeqFeatData::eSubtype_variation, CGbqualType::e_Frequency );
    Associate( CSeqFeatData::eSubtype_variation, CGbqualType::e_Gene );
    Associate( CSeqFeatData::eSubtype_variation, CGbqualType::e_Gene_synonym );
    Associate( CSeqFeatData::eSubtype_variation, CGbqualType::e_Inference );
    Associate( CSeqFeatData::eSubtype_variation, CGbqualType::e_Label );
    Associate( CSeqFeatData::eSubtype_variation, CGbqualType::e_Locus_tag );
    Associate( CSeqFeatData::eSubtype_variation, CGbqualType::e_Map );
    Associate( CSeqFeatData::eSubtype_variation, CGbqualType::e_Note );
    Associate( CSeqFeatData::eSubtype_variation, CGbqualType::e_Old_locus_tag );
    Associate( CSeqFeatData::eSubtype_variation, CGbqualType::e_Partial );
    Associate( CSeqFeatData::eSubtype_variation, CGbqualType::e_Phenotype );
    Associate( CSeqFeatData::eSubtype_variation, CGbqualType::e_Product );
    Associate( CSeqFeatData::eSubtype_variation, CGbqualType::e_Replace );
    Associate( CSeqFeatData::eSubtype_variation, CGbqualType::e_Standard_name );
    Associate( CSeqFeatData::eSubtype_variation, CGbqualType::e_Usedin );

    //3clip
    Associate( CSeqFeatData::eSubtype_3clip, CGbqualType::e_Allele );
    Associate( CSeqFeatData::eSubtype_3clip, CGbqualType::e_Citation );
    Associate( CSeqFeatData::eSubtype_3clip, CGbqualType::e_Db_xref );
    Associate( CSeqFeatData::eSubtype_3clip, CGbqualType::e_Evidence );
    Associate( CSeqFeatData::eSubtype_3clip, CGbqualType::e_Experiment );
    Associate( CSeqFeatData::eSubtype_3clip, CGbqualType::e_Function );
    Associate( CSeqFeatData::eSubtype_3clip, CGbqualType::e_Gene );
    Associate( CSeqFeatData::eSubtype_3clip, CGbqualType::e_Gene_synonym );
    Associate( CSeqFeatData::eSubtype_3clip, CGbqualType::e_Inference );
    Associate( CSeqFeatData::eSubtype_3clip, CGbqualType::e_Label );
    Associate( CSeqFeatData::eSubtype_3clip, CGbqualType::e_Locus_tag );
    Associate( CSeqFeatData::eSubtype_3clip, CGbqualType::e_Map );
    Associate( CSeqFeatData::eSubtype_3clip, CGbqualType::e_Note );
    Associate( CSeqFeatData::eSubtype_3clip, CGbqualType::e_Old_locus_tag );
    Associate( CSeqFeatData::eSubtype_3clip, CGbqualType::e_Partial );
    Associate( CSeqFeatData::eSubtype_3clip, CGbqualType::e_Standard_name );
    Associate( CSeqFeatData::eSubtype_3clip, CGbqualType::e_Trans_splicing );
    Associate( CSeqFeatData::eSubtype_3clip, CGbqualType::e_Usedin );

    //3UTR
    Associate( CSeqFeatData::eSubtype_3UTR, CGbqualType::e_Allele );
    Associate( CSeqFeatData::eSubtype_3UTR, CGbqualType::e_Citation );
    Associate( CSeqFeatData::eSubtype_3UTR, CGbqualType::e_Db_xref );
    Associate( CSeqFeatData::eSubtype_3UTR, CGbqualType::e_Evidence );
    Associate( CSeqFeatData::eSubtype_3UTR, CGbqualType::e_Experiment );
    Associate( CSeqFeatData::eSubtype_3UTR, CGbqualType::e_Function );
    Associate( CSeqFeatData::eSubtype_3UTR, CGbqualType::e_Gene );
    Associate( CSeqFeatData::eSubtype_3UTR, CGbqualType::e_Gene_synonym );
    Associate( CSeqFeatData::eSubtype_3UTR, CGbqualType::e_Inference );
    Associate( CSeqFeatData::eSubtype_3UTR, CGbqualType::e_Label );
    Associate( CSeqFeatData::eSubtype_3UTR, CGbqualType::e_Locus_tag );
    Associate( CSeqFeatData::eSubtype_3UTR, CGbqualType::e_Map );
    Associate( CSeqFeatData::eSubtype_3UTR, CGbqualType::e_Note );
    Associate( CSeqFeatData::eSubtype_3UTR, CGbqualType::e_Old_locus_tag );
    Associate( CSeqFeatData::eSubtype_3UTR, CGbqualType::e_Partial );
    Associate( CSeqFeatData::eSubtype_3UTR, CGbqualType::e_Standard_name );
    Associate( CSeqFeatData::eSubtype_3UTR, CGbqualType::e_Trans_splicing );
    Associate( CSeqFeatData::eSubtype_3UTR, CGbqualType::e_Usedin );

    //5clip
    Associate( CSeqFeatData::eSubtype_5clip, CGbqualType::e_Allele );
    Associate( CSeqFeatData::eSubtype_5clip, CGbqualType::e_Citation );
    Associate( CSeqFeatData::eSubtype_5clip, CGbqualType::e_Db_xref );
    Associate( CSeqFeatData::eSubtype_5clip, CGbqualType::e_Evidence );
    Associate( CSeqFeatData::eSubtype_5clip, CGbqualType::e_Experiment );
    Associate( CSeqFeatData::eSubtype_5clip, CGbqualType::e_Function );
    Associate( CSeqFeatData::eSubtype_5clip, CGbqualType::e_Gene );
    Associate( CSeqFeatData::eSubtype_5clip, CGbqualType::e_Gene_synonym );
    Associate( CSeqFeatData::eSubtype_5clip, CGbqualType::e_Inference );
    Associate( CSeqFeatData::eSubtype_5clip, CGbqualType::e_Label );
    Associate( CSeqFeatData::eSubtype_5clip, CGbqualType::e_Locus_tag );
    Associate( CSeqFeatData::eSubtype_5clip, CGbqualType::e_Map );
    Associate( CSeqFeatData::eSubtype_5clip, CGbqualType::e_Note );
    Associate( CSeqFeatData::eSubtype_5clip, CGbqualType::e_Old_locus_tag );
    Associate( CSeqFeatData::eSubtype_5clip, CGbqualType::e_Partial );
    Associate( CSeqFeatData::eSubtype_5clip, CGbqualType::e_Standard_name );
    Associate( CSeqFeatData::eSubtype_5clip, CGbqualType::e_Trans_splicing );
    Associate( CSeqFeatData::eSubtype_5clip, CGbqualType::e_Usedin );

    //5UTR
    Associate( CSeqFeatData::eSubtype_5UTR, CGbqualType::e_Allele );
    Associate( CSeqFeatData::eSubtype_5UTR, CGbqualType::e_Citation );
    Associate( CSeqFeatData::eSubtype_5UTR, CGbqualType::e_Db_xref );
    Associate( CSeqFeatData::eSubtype_5UTR, CGbqualType::e_Evidence );
    Associate( CSeqFeatData::eSubtype_5UTR, CGbqualType::e_Experiment );
    Associate( CSeqFeatData::eSubtype_5UTR, CGbqualType::e_Function );
    Associate( CSeqFeatData::eSubtype_5UTR, CGbqualType::e_Gene );
    Associate( CSeqFeatData::eSubtype_5UTR, CGbqualType::e_Gene_synonym );
    Associate( CSeqFeatData::eSubtype_5UTR, CGbqualType::e_Inference );
    Associate( CSeqFeatData::eSubtype_5UTR, CGbqualType::e_Label );
    Associate( CSeqFeatData::eSubtype_5UTR, CGbqualType::e_Locus_tag );
    Associate( CSeqFeatData::eSubtype_5UTR, CGbqualType::e_Map );
    Associate( CSeqFeatData::eSubtype_5UTR, CGbqualType::e_Note );
    Associate( CSeqFeatData::eSubtype_5UTR, CGbqualType::e_Old_locus_tag );
    Associate( CSeqFeatData::eSubtype_5UTR, CGbqualType::e_Partial );
    Associate( CSeqFeatData::eSubtype_5UTR, CGbqualType::e_Standard_name );
    Associate( CSeqFeatData::eSubtype_5UTR, CGbqualType::e_Trans_splicing );
    Associate( CSeqFeatData::eSubtype_5UTR, CGbqualType::e_Usedin );

    //10_signal
    Associate( CSeqFeatData::eSubtype_10_signal, CGbqualType::e_Allele );
    Associate( CSeqFeatData::eSubtype_10_signal, CGbqualType::e_Citation );
    Associate( CSeqFeatData::eSubtype_10_signal, CGbqualType::e_Db_xref );
    Associate( CSeqFeatData::eSubtype_10_signal, CGbqualType::e_Evidence );
    Associate( CSeqFeatData::eSubtype_10_signal, CGbqualType::e_Experiment );
    Associate( CSeqFeatData::eSubtype_10_signal, CGbqualType::e_Gene );
    Associate( CSeqFeatData::eSubtype_10_signal, CGbqualType::e_Gene_synonym );
    Associate( CSeqFeatData::eSubtype_10_signal, CGbqualType::e_Inference );
    Associate( CSeqFeatData::eSubtype_10_signal, CGbqualType::e_Label );
    Associate( CSeqFeatData::eSubtype_10_signal, CGbqualType::e_Locus_tag );
    Associate( CSeqFeatData::eSubtype_10_signal, CGbqualType::e_Map );
    Associate( CSeqFeatData::eSubtype_10_signal, CGbqualType::e_Note );
    Associate( CSeqFeatData::eSubtype_10_signal, CGbqualType::e_Old_locus_tag );
    Associate( CSeqFeatData::eSubtype_10_signal, CGbqualType::e_Operon );
    Associate( CSeqFeatData::eSubtype_10_signal, CGbqualType::e_Partial );
    Associate( CSeqFeatData::eSubtype_10_signal, CGbqualType::e_Standard_name );
    Associate( CSeqFeatData::eSubtype_10_signal, CGbqualType::e_Usedin );

    //35_signal
    Associate( CSeqFeatData::eSubtype_35_signal, CGbqualType::e_Allele );
    Associate( CSeqFeatData::eSubtype_35_signal, CGbqualType::e_Citation );
    Associate( CSeqFeatData::eSubtype_35_signal, CGbqualType::e_Db_xref );
    Associate( CSeqFeatData::eSubtype_35_signal, CGbqualType::e_Evidence );
    Associate( CSeqFeatData::eSubtype_35_signal, CGbqualType::e_Experiment );
    Associate( CSeqFeatData::eSubtype_35_signal, CGbqualType::e_Gene );
    Associate( CSeqFeatData::eSubtype_35_signal, CGbqualType::e_Gene_synonym );
    Associate( CSeqFeatData::eSubtype_35_signal, CGbqualType::e_Inference );
    Associate( CSeqFeatData::eSubtype_35_signal, CGbqualType::e_Label );
    Associate( CSeqFeatData::eSubtype_35_signal, CGbqualType::e_Locus_tag );
    Associate( CSeqFeatData::eSubtype_35_signal, CGbqualType::e_Map );
    Associate( CSeqFeatData::eSubtype_35_signal, CGbqualType::e_Note );
    Associate( CSeqFeatData::eSubtype_35_signal, CGbqualType::e_Old_locus_tag );
    Associate( CSeqFeatData::eSubtype_35_signal, CGbqualType::e_Operon );
    Associate( CSeqFeatData::eSubtype_35_signal, CGbqualType::e_Partial );
    Associate( CSeqFeatData::eSubtype_35_signal, CGbqualType::e_Standard_name );
    Associate( CSeqFeatData::eSubtype_35_signal, CGbqualType::e_Usedin );
}


void CFeatQualAssoc::PopulateMandatoryGbquals(void)
{
    // gene feature requires gene gbqual
    m_MandatoryGbquals[CSeqFeatData::eSubtype_gene].push_back(CGbqualType::e_Gene);
    // ncRNA requires ncRNA_class
    m_MandatoryGbquals[CSeqFeatData::eSubtype_ncRNA].push_back(CGbqualType::e_NcRNA_class);

    // misc_binding & protein_bind require bound_moiety
    m_MandatoryGbquals[CSeqFeatData::eSubtype_misc_binding].push_back(CGbqualType::e_Bound_moiety);
    m_MandatoryGbquals[CSeqFeatData::eSubtype_protein_bind].push_back(CGbqualType::e_Bound_moiety);

    // modified_base requires mod_base
    m_MandatoryGbquals[CSeqFeatData::eSubtype_modified_base].push_back(CGbqualType::e_Mod_base);

    // gap requires estimated length
    m_MandatoryGbquals[CSeqFeatData::eSubtype_gap].push_back(CGbqualType::e_Estimated_length);

    // operon requires operon
    m_MandatoryGbquals[CSeqFeatData::eSubtype_operon].push_back(CGbqualType::e_Operon);


}


// =============================================================================
//                                 Country Names
// =============================================================================


// legal country names
const string CCountries::sm_Countries[] = {
    "Afghanistan",
    "Albania",
    "Algeria",
    "American Samoa",
    "Andorra",
    "Angola",
    "Anguilla",
    "Antarctica",
    "Antigua and Barbuda",
    "Arctic Ocean",
    "Argentina",
    "Armenia",
    "Aruba",
    "Ashmore and Cartier Islands",
    "Atlantic Ocean",
    "Australia",
    "Austria",
    "Azerbaijan",
    "Bahamas",
    "Bahrain",
    "Baker Island",
    "Bangladesh",
    "Barbados",
    "Bassas da India",
    "Belarus",
    "Belgium",
    "Belize",
    "Benin",
    "Bermuda",
    "Bhutan",
    "Bolivia",
    "Bosnia and Herzegovina",
    "Botswana",
    "Bouvet Island",
    "Brazil",
    "British Virgin Islands",
    "Brunei",
    "Bulgaria",
    "Burkina Faso",
    "Burundi",
    "Cambodia",
    "Cameroon",
    "Canada",
    "Cape Verde",
    "Cayman Islands",
    "Central African Republic",
    "Chad",
    "Chile",
    "China",
    "Christmas Island",
    "Clipperton Island",
    "Cocos Islands",
    "Colombia",
    "Comoros",
    "Cook Islands",
    "Coral Sea Islands",
    "Costa Rica",
    "Cote d'Ivoire",
    "Croatia",
    "Cuba",
    "Cyprus",
    "Czech Republic",
    "Democratic Republic of the Congo",
    "Denmark",
    "Djibouti",
    "Dominica",
    "Dominican Republic",
    "East Timor",
    "Ecuador",
    "Egypt",
    "El Salvador",
    "Equatorial Guinea",
    "Eritrea",
    "Estonia",
    "Ethiopia",
    "Europa Island",
    "Falkland Islands (Islas Malvinas)",
    "Faroe Islands",
    "Fiji",
    "Finland",
    "France",
    "French Guiana",
    "French Polynesia",
    "French Southern and Antarctic Lands",
    "Gabon",
    "Gambia",
    "Gaza Strip",
    "Georgia",
    "Germany",
    "Ghana",
    "Gibraltar",
    "Glorioso Islands",
    "Greece",
    "Greenland",
    "Grenada",
    "Guadeloupe",
    "Guam",
    "Guatemala",
    "Guernsey",
    "Guinea-Bissau",
    "Guinea",
    "Guyana",
    "Haiti",
    "Heard Island and McDonald Islands",
    "Honduras",
    "Hong Kong",
    "Howland Island",
    "Hungary",
    "Iceland",
    "India",
    "Indian Ocean",
    "Indonesia",
    "Iran",
    "Iraq",
    "Ireland",
    "Isle of Man",
    "Israel",
    "Italy",
    "Jamaica",
    "Jan Mayen",
    "Japan",
    "Jarvis Island",
    "Jersey",
    "Johnston Atoll",
    "Jordan",
    "Juan de Nova Island",
    "Kazakhstan",
    "Kenya",
    "Kerguelen Archipelago",
    "Kingman Reef",
    "Kiribati",
    "Kosovo",
    "Kuwait",
    "Kyrgyzstan",
    "Laos",
    "Latvia",
    "Lebanon",
    "Lesotho",
    "Liberia",
    "Libya",
    "Liechtenstein",
    "Lithuania",
    "Luxembourg",
    "Macau",
    "Macedonia",
    "Madagascar",
    "Malawi",
    "Malaysia",
    "Maldives",
    "Mali",
    "Malta",
    "Marshall Islands",
    "Martinique",
    "Mauritania",
    "Mauritius",
    "Mayotte",
    "Mexico",
    "Micronesia",
    "Midway Islands",
    "Moldova",
    "Monaco",
    "Mongolia",
    "Montenegro",
    "Montserrat",
    "Morocco",
    "Mozambique",
    "Myanmar",
    "Namibia",
    "Nauru",
    "Navassa Island",
    "Nepal",
    "Netherlands Antilles",
    "Netherlands",
    "New Caledonia",
    "New Zealand",
    "Nicaragua",
    "Niger",
    "Nigeria",
    "Niue",
    "Norfolk Island",
    "North Korea",
    "North Sea",
    "Northern Mariana Islands",
    "Norway",
    "Oman",
    "Pacific Ocean",
    "Pakistan",
    "Palau",
    "Palmyra Atoll",
    "Panama",
    "Papua New Guinea",
    "Paracel Islands",
    "Paraguay",
    "Peru",
    "Philippines",
    "Pitcairn Islands",
    "Poland",
    "Portugal",
    "Puerto Rico",
    "Qatar",
    "Republic of the Congo",
    "Reunion",
    "Romania",
    "Russia",
    "Rwanda",
    "Saint Helena",
    "Saint Kitts and Nevis",
    "Saint Lucia",
    "Saint Pierre and Miquelon",
    "Saint Vincent and the Grenadines",
    "Samoa",
    "San Marino",
    "Sao Tome and Principe",
    "Saudi Arabia",
    "Senegal",
    "Serbia and Montenegro",
    "Serbia",
    "Seychelles",
    "Sierra Leone",
    "Singapore",
    "Slovakia",
    "Slovenia",
    "Solomon Islands",
    "Somalia",
    "South Africa",
    "South Georgia and the South Sandwich Islands",
    "South Korea",
    "Spain",
    "Spratly Islands",
    "Sri Lanka",
    "Sudan",
    "Suriname",
    "Svalbard",
    "Swaziland",
    "Sweden",
    "Switzerland",
    "Syria",
    "Taiwan",
    "Tajikistan",
    "Tanzania",
    "Thailand",
    "Togo",
    "Tokelau",
    "Tonga",
    "Trinidad and Tobago",
    "Tromelin Island",
    "Tunisia",
    "Turkey",
    "Turkmenistan",
    "Turks and Caicos Islands",
    "Tuvalu",
    "Uganda",
    "Ukraine",
    "United Arab Emirates",
    "United Kingdom",
    "Uruguay",
    "USA",
    "Uzbekistan",
    "Vanuatu",
    "Venezuela",
    "Viet Nam",
    "Virgin Islands",
    "Wake Island",
    "Wallis and Futuna",
    "West Bank",
    "Western Sahara",
    "Yemen",
    "Yugoslavia",
    "Zambia",
    "Zimbabwe",
};

const string CCountries::sm_Former_Countries[] = {
    "Belgian Congo",
    "British Guiana",
    "Burma",
    "Czechoslovakia",
    "Ivory Coast",
    "Serbia and Montenegro",
    "Siam",
    "USSR",
    "Yugoslavia",
    "Zaire",
};

bool CCountries::IsValid(const string& country)
{
    string name = country;
    size_t pos = country.find(':');

    if ( pos != string::npos ) {
        name = country.substr(0, pos);
    }

    // try current countries
    const string *begin = sm_Countries;
    const string *end = &(sm_Countries[sizeof(sm_Countries) / sizeof(string)]);

    return find(begin, end, name) != end;
}


bool CCountries::WasValid(const string& country)
{
    string name = country;
    size_t pos = country.find(':');

    if ( pos != string::npos ) {
        name = country.substr(0, pos);
    }

    // try formerly-valid countries
    const string *begin = sm_Former_Countries;
    const string *end = &(sm_Former_Countries[sizeof(sm_Former_Countries) / sizeof(string)]);

    return find(begin, end, name) != end;
}


// =============================================================================
//                                    Functions
// =============================================================================


bool IsClassInEntry(const CSeq_entry& se, CBioseq_set::EClass clss)
{
    for ( CTypeConstIterator <CBioseq_set> si(se); si; ++si ) {
        if ( si->GetClass() == clss ) {
            return true;
        }
    }
    return false;
}


bool IsDeltaOrFarSeg(const CSeq_loc& loc, CScope* scope)
{
    CBioseq_Handle bsh = scope->GetBioseqHandle(loc);
    const CSeq_entry& se = *bsh.GetTopLevelEntry().GetCompleteSeq_entry();

    if ( bsh.IsSetInst_Repr() ) {
        CBioseq_Handle::TInst::TRepr repr = bsh.GetInst_Repr();
        if ( repr == CSeq_inst::eRepr_delta ) {
            if ( !IsClassInEntry(se, CBioseq_set::eClass_nuc_prot) ) {
                return true;
            }
        }
        if ( repr == CSeq_inst::eRepr_seg ) {
            if ( !IsClassInEntry(se, CBioseq_set::eClass_parts) ) {
                return true;
            }
        }
    }

    return false;
}


// Check if string is either empty or contains just white spaces
bool IsBlankString(const string& str)
{
    if (NStr::IsBlank (str)) {
        return true;
    } else {
        return false;
    }
}


bool IsBlankStringList(const list< string >& str_list)
{
    ITERATE( list< string >, str, str_list ) {
        if ( !IsBlankString(*str) ) {
            return false;
        }
    }
    return true;
}


int GetGIForSeqId(const CSeq_id& id)
{
    return 0;
    //return CID1Client().AskGetgi(id);
}



list< CRef< CSeq_id > > GetSeqIdsForGI(int gi)
{
    return list< CRef< CSeq_id > >();
    //return CID1Client().AskGetseqidsfromgi(gi);
}


CSeqVector GetSequenceFromLoc
(const CSeq_loc& loc,
 CScope& scope,
 CBioseq_Handle::EVectorCoding coding)
{
    CConstRef<CSeqMap> map = 
        CSeqMap::CreateSeqMapForSeq_loc(loc, &scope);
    ENa_strand strand = sequence::GetStrand(loc, &scope);

    return CSeqVector(*map, scope, coding, strand);
}


CSeqVector GetSequenceFromFeature
(const CSeq_feat& feat,
 CScope& scope,
 CBioseq_Handle::EVectorCoding coding,
 bool product)
{

    if ( (product   &&  !feat.CanGetProduct())  ||
         (!product  &&  !feat.CanGetLocation()) ) {
        return CSeqVector();
    }

    const CSeq_loc* loc = product ? &feat.GetProduct() : &feat.GetLocation();
    return GetSequenceFromLoc(*loc, scope, coding);
}


/***** Calculate Accession for a given object *****/

static string s_GetBioseqAcc(const CBioseq_Handle& handle)
{
    if (handle) {
        try {
            return sequence::GetId(handle).GetSeqId()->GetSeqIdString();
        } catch (CException&) {}
    }
    return kEmptyStr;
}


static string s_GetSeq_featAcc(const CSeq_feat& feat, CScope& scope)
{
    CBioseq_Handle seq = sequence::GetBioseqFromSeqLoc(feat.GetLocation(), scope);
    return s_GetBioseqAcc(seq);
}


static string s_GetBioseqAcc(const CBioseq& seq, CScope& scope)
{
    CBioseq_Handle handle = scope.GetBioseqHandle(seq);
    return s_GetBioseqAcc(handle);
}

static const CBioseq* s_GetSeqFromSet(const CBioseq_set& bsst, CScope& scope)
{
    const CBioseq* retval = NULL;

    switch (bsst.GetClass()) {
        case CBioseq_set::eClass_gen_prod_set:
            // find the genomic bioseq
            FOR_EACH_SEQENTRY_ON_SEQSET (it, bsst) {
                if ((*it)->IsSeq()) {
                    const CSeq_inst& inst = (*it)->GetSeq().GetInst();
                    if (inst.IsSetMol()  &&  inst.GetMol() == CSeq_inst::eMol_dna) {
                        retval = &(*it)->GetSeq();
                        break;
                    }
                }
            }
            break;
        case CBioseq_set::eClass_nuc_prot:
            // find the nucleotide bioseq
            FOR_EACH_SEQENTRY_ON_SEQSET (it, bsst) {
                if ((*it)->IsSeq()  &&  (*it)->GetSeq().IsNa()) {
                    retval = &(*it)->GetSeq();
                    break;
                }
            }
            break;
        case CBioseq_set::eClass_segset:
            // find the master bioseq
            FOR_EACH_SEQENTRY_ON_SEQSET (it, bsst) {
                if ((*it)->IsSeq()) {
                    retval = &(*it)->GetSeq();
                    break;
                }
            }
            break;

        default:
            // find the first bioseq
            CTypeConstIterator<CBioseq> seqit(ConstBegin(bsst));
            if (seqit) {
                retval = &(*seqit);
            }
            break;
    }

    return retval;
}


string GetAccessionFromObjects(const CSerialObject* obj, const CSeq_entry* ctx, CScope& scope)
{
    if (ctx) {
        if (ctx->IsSeq()) {
            return s_GetBioseqAcc(ctx->GetSeq(), scope);
        } else if (ctx->IsSet()) {
            const CBioseq* seq = s_GetSeqFromSet(ctx->GetSet(), scope);
            if (seq != NULL) {
                return s_GetBioseqAcc(*seq, scope);
            }
        }
    } else if (obj) {
        if (obj->GetThisTypeInfo() == CSeq_feat::GetTypeInfo()) {
            const CSeq_feat& feat = dynamic_cast<const CSeq_feat&>(*obj);
            return s_GetSeq_featAcc(feat, scope);
        } else if (obj->GetThisTypeInfo() == CBioseq::GetTypeInfo()) {
            const CBioseq& seq = dynamic_cast<const CBioseq&>(*obj);
            return s_GetBioseqAcc(seq, scope);
        } else if (obj->GetThisTypeInfo() == CBioseq_set::GetTypeInfo()) {
            const CBioseq_set& bsst = dynamic_cast<const CBioseq_set&>(*obj);
            const CBioseq* seq = s_GetSeqFromSet(bsst, scope);
            if (seq != NULL) {
                return s_GetBioseqAcc(*seq, scope);
            }
        } // TO DO: graph
    }
    return kEmptyStr;
}


CBioseq_set_Handle GetGenProdSetParent (CBioseq_set_Handle set)
{
    CBioseq_set_Handle gps;

    CSeq_entry_Handle parent = set.GetParentEntry();
    if (!parent) {
        return gps;
    } else if (!(parent = parent.GetParentEntry())) {
        return gps;
    } else if (!parent.IsSet()) {
        return gps;
    } else if (parent.GetSet().IsSetClass() && parent.GetSet().GetClass() == CBioseq_set::eClass_gen_prod_set) {
        return parent.GetSet();
    } else {
        return GetGenProdSetParent (parent.GetSet());
    }    
}


CBioseq_set_Handle GetGenProdSetParent (CBioseq_Handle bioseq)
{
    CBioseq_set_Handle gps;

    CSeq_entry_Handle parent = bioseq.GetParentEntry();
    if (!parent) {
        return gps;
    } else if (!(parent = parent.GetParentEntry())) {
        return gps;
    } else if (!parent.IsSet()) {
        return gps;
    } else if (parent.GetSet().IsSetClass() && parent.GetSet().GetClass() == CBioseq_set::eClass_gen_prod_set) {
        return parent.GetSet();
    } else {
        return GetGenProdSetParent (parent.GetSet());
    }
}


EAccessionFormatError ValidateAccessionString (string accession, bool require_version)
{
    if (NStr::IsBlank (accession)) {
        return eAccessionFormat_null;
    } else if (accession.length() >= 16) {
        return eAccessionFormat_too_long;
    } else if (accession.length() < 3 
               || ! isalpha (accession.c_str()[0]) 
               || ! isupper (accession.c_str()[0])
               || ! isalpha (accession.c_str()[1])
               || ! isupper (accession.c_str()[1])) {
        return eAccessionFormat_no_start_letters;
    }
    
    string str = accession;
    if (NStr::StartsWith (str, "NZ_")) {
        str = str.substr(3);
    }
    
    const char *cp = str.c_str();
    int numAlpha = 0;

    while (isalpha (*cp)) {
        numAlpha++;
        cp++;
    }

    int numUndersc = 0;

    while (*cp == '_') {
        numUndersc++;
        cp++;
    }

    int numDigits = 0;
    while (isdigit (*cp)) {
        numDigits++;
        cp++;
    }

    if ((*cp != '\0' && *cp != ' ' && *cp != '.') || numUndersc > 1) {
        return eAccessionFormat_wrong_number_of_digits;
    }

    EAccessionFormatError rval = eAccessionFormat_valid;

    if (require_version) {
        if (*cp != '.') {
            rval = eAccessionFormat_missing_version;
        }
        cp++;
        int numVersion = 0;
        while (isdigit (*cp)) {
            numVersion++;
            cp++;
        }
        if (numVersion < 1) {
            rval = eAccessionFormat_missing_version;
        } else if (*cp != '\0' && *cp != ' ') {
            rval = eAccessionFormat_bad_version;
        }
    }


    if (numUndersc == 0) {
        if ((numAlpha == 1 && numDigits == 5) 
            || (numAlpha == 2 && numDigits == 6)
            || (numAlpha == 3 && numDigits == 5)
            || (numAlpha == 4 && numDigits == 8)
            || (numAlpha == 5 && numDigits == 7)) {
            return rval;
        } 
    } else if (numUndersc == 1) {
        if (numAlpha != 2 || (numDigits != 6 && numDigits != 8 && numDigits != 9)) {
            return eAccessionFormat_wrong_number_of_digits;
        }
        char first_letter = accession.c_str()[0];
        char second_letter = accession.c_str()[1];
        if (first_letter == 'N' || first_letter == 'X' || first_letter == 'Z') { 
            if (second_letter == 'M' || second_letter == 'C'
                || second_letter == 'T' || second_letter == 'P'
                || second_letter == 'G' || second_letter == 'R'
                || second_letter == 'S' || second_letter == 'W'
                || second_letter == 'W' || second_letter == 'Z') {
                return rval;
            }
        }
        if ((first_letter == 'A' || first_letter == 'Y')
            && second_letter == 'P') {
            return rval;
        }
    }

    return eAccessionFormat_wrong_number_of_digits;
}


END_SCOPE(validator)
END_SCOPE(objects)
END_NCBI_SCOPE
