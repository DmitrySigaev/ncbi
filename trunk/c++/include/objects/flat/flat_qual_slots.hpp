#ifndef OBJECTS_FLAT___FLAT_QUAL_SLOTS__HPP
#define OBJECTS_FLAT___FLAT_QUAL_SLOTS__HPP

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
*
* File Description:
*   new (early 2003) flat-file formatter -- qualifier slots
*   (public only because one can't predeclare enums...)
*
*/

#include <corelib/ncbistd.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


enum EFeatureQualifier {
    eFQ_none,
    eFQ_allele,
    eFQ_anticodon,
    eFQ_bond,
    eFQ_bond_type,
    eFQ_bound_moiety,
    eFQ_cds_product,
    eFQ_citation,
    eFQ_clone,
    eFQ_coded_by,
    eFQ_codon,
    eFQ_codon_start,
    eFQ_cons_splice,
    eFQ_db_xref,
    eFQ_derived_from,
    eFQ_direction,
    eFQ_EC_number,
    eFQ_evidence,
    eFQ_exception,
    eFQ_exception_note,
    eFQ_figure,
    eFQ_frequency,
    eFQ_function,
    eFQ_gene,
    eFQ_gene_desc,
    eFQ_gene_allele,
    eFQ_gene_map,
    eFQ_gene_syn,
    eFQ_gene_note,
    eFQ_gene_xref,
    eFQ_go_component,
    eFQ_go_function,
    eFQ_go_process,
    eFQ_heterogen,
    eFQ_illegal_qual,
    eFQ_insertion_seq,
    eFQ_label,
    eFQ_locus_tag,
    eFQ_map,
    eFQ_maploc,
    eFQ_mod_base,
    eFQ_modelev,
    eFQ_number,
    eFQ_organism,
    eFQ_partial,
    eFQ_PCR_conditions,
    eFQ_peptide,
    eFQ_phenotype,
    eFQ_product,
    eFQ_product_quals,
    eFQ_prot_activity,
    eFQ_prot_comment,
    eFQ_prot_EC_number,
    eFQ_prot_note,
    eFQ_prot_method,
    eFQ_prot_conflict,
    eFQ_prot_desc,
    eFQ_prot_missing,
    eFQ_prot_name,
    eFQ_prot_names,
    eFQ_protein_id,
    eFQ_pseudo,
    eFQ_region,
    eFQ_region_name,
    eFQ_replace,
    eFQ_rpt_family,
    eFQ_rpt_type,
    eFQ_rpt_unit,
    eFQ_rrna_its,
    eFQ_sec_str_type,
    eFQ_selenocysteine,
    eFQ_seqfeat_note,
    eFQ_site,
    eFQ_site_type,
    eFQ_standard_name,
    eFQ_transcription,
    eFQ_transcript_id,
    eFQ_transl_except,
    eFQ_transl_table,
    eFQ_translation,
    eFQ_transposon,
    eFQ_trna_aa,
    eFQ_trna_codons,
    eFQ_usedin,
    eFQ_xtra_prod_quals
};


enum ESourceQualifier {
    eSQ_none,
    eSQ_acronym,
    eSQ_anamorph,
    eSQ_authority,
    eSQ_biotype,
    eSQ_biovar,
    eSQ_breed,
    eSQ_cell_line,
    eSQ_cell_type,
    eSQ_chemovar,
    eSQ_chromosome,
    eSQ_citation,
    eSQ_clone,
    eSQ_clone_lib,
    eSQ_common,
    eSQ_common_name,
    eSQ_country,
    eSQ_cultivar,
    eSQ_db_xref,
    eSQ_org_xref,
    eSQ_dev_stage,
    eSQ_dosage,
    eSQ_ecotype,
    eSQ_endogenous_virus_name,
    eSQ_environmental_sample,
    eSQ_extrachrom,
    eSQ_focus,
    eSQ_forma,
    eSQ_forma_specialis,
    eSQ_frequency,
    eSQ_gb_acronym,
    eSQ_gb_anamorph,
    eSQ_gb_synonym,
    eSQ_genotype,
    eSQ_germline,
    eSQ_group,
    eSQ_haplotype,
    eSQ_insertion_seq_name,
    eSQ_isolate,
    eSQ_isolation_source,
    eSQ_lab_host,
    eSQ_label,
    eSQ_macronuclear,
    eSQ_map,
    eSQ_mol_type,
    eSQ_old_lineage,
    eSQ_old_name,
    eSQ_organism,
    eSQ_organelle,
    eSQ_orgmod_note,
    eSQ_pathovar,
    eSQ_plasmid_name,
    eSQ_plastid_name,
    eSQ_pop_variant,
    eSQ_rearranged,
    eSQ_segment,
    eSQ_seqfeat_note,
    eSQ_sequenced_mol,
    eSQ_serogroup,
    eSQ_serotype,
    eSQ_serovar,
    eSQ_sex,
    eSQ_spec_or_nat_host,
    eSQ_specimen_voucher,
    eSQ_strain,
    eSQ_subclone,
    eSQ_subgroup,
    eSQ_sub_species,
    eSQ_substrain,
    eSQ_subtype,
    eSQ_subsource_note,
    eSQ_synonym,
    eSQ_teleomorph,
    eSQ_tissue_lib,
    eSQ_tissue_type,
    eSQ_transgenic,
    eSQ_transposon_name,
    eSQ_type,
    eSQ_unstructured,
    eSQ_usedin,
    eSQ_variety,
    eSQ_zero_orgmod,
    eSQ_one_orgmod,
    eSQ_zero_subsrc
};


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ===========================================================================
*
* $Log$
* Revision 1.1  2003/03/10 16:39:08  ucko
* Initial check-in of new flat-file generator
*
*
* ===========================================================================
*/

#endif  /* OBJECTS_FLAT___FLAT_QUAL_SLOTS__HPP */
