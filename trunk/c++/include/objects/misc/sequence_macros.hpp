#ifndef __SEQUENCE_MACROS__HPP__
#define __SEQUENCE_MACROS__HPP__

/*
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
* Author: Jonathan Kans
*
* File Description: Utility macros for exploring NCBI objects
*
* ===========================================================================
*/


#include <objects/general/general__.hpp>
#include <objects/seq/seq__.hpp>
#include <objects/seqset/seqset__.hpp>
#include <objects/seqloc/seqloc__.hpp>
#include <objects/submit/submit__.hpp>
#include <objects/seqblock/seqblock__.hpp>
#include <objects/seqfeat/seqfeat__.hpp>
#include <objects/seqalign/seqalign__.hpp>
#include <objects/seqres/seqres__.hpp>
#include <objects/biblio/biblio__.hpp>
#include <objects/pub/pub__.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


/// @NAME Convenience macros for NCBI objects
/// @{



/////////////////////////////////////////////////////////////////////////////
/// Macros and typedefs for object subtypes
/////////////////////////////////////////////////////////////////////////////


/// CSeq_entry definitions

#define NCBI_SEQENTRY(Type) CSeq_entry::e_##Type
typedef CSeq_entry::E_Choice TSEQENTRY_CHOICE;

//   Seq     Set


/// CSeq_id definitions

#define NCBI_SEQID(Type) CSeq_id::e_##Type
typedef CSeq_id::E_Choice TSEQID_CHOICE;

//   Local       Gibbsq     Gibbmt      Giim
//   Genbank     Embl       Pir         Swissprot
//   Patent      Other      General     Gi
//   Ddbj        Prf        Pdb         Tpg
//   Tpe         Tpd        Gpipe       Named_annot_track

#define NCBI_ACCN(Type) CSeq_id::eAcc_##Type
typedef CSeq_id::EAccessionInfo TACCN_CHOICE;


/// CSeq_inst definitions

#define NCBI_SEQREPR(Type) CSeq_inst::eRepr_##Type
typedef CSeq_inst::TRepr TSEQ_REPR;

//   virtual     raw     seg       const     ref
//   consen      map     delta     other

#define NCBI_SEQMOL(Type) CSeq_inst::eMol_##Type
typedef CSeq_inst::TMol TSEQ_MOL;

//   dna     rna     aa     na     other

#define NCBI_SEQTOPOLOGY(Type) CSeq_inst::eTopology_##Type
typedef CSeq_inst::TTopology TSEQ_TOPOLOGY;

//   linear     circular     tandem     other

#define NCBI_SEQSTRAND(Type) CSeq_inst::eStrand_##Type
typedef CSeq_inst::TStrand TSEQ_STRAND;

//   ss     ds     mixed     other


/// CSeq_annot definitions

#define NCBI_SEQANNOT(Type) CSeq_annot::TData::e_##Type
typedef CSeq_annot::TData::E_Choice TSEQANNOT_CHOICE;

//   Ftable     Align     Graph     Ids     Locs     Seq_table


/// CAnnotdesc definitions

#define NCBI_ANNOTDESC(Type) CAnnotdesc::e_##Type
typedef CAnnotdesc::E_Choice TANNOTDESC_CHOICE;

//   Name     Title           Comment         Pub
//   User     Create_date     Update_date
//   Src      Align           Region


/// CSeqdesc definitions

#define NCBI_SEQDESC(Type) CSeqdesc::e_##Type
typedef CSeqdesc::E_Choice TSEQDESC_CHOICE;

//   Mol_type     Modif           Method          Name
//   Title        Org             Comment         Num
//   Maploc       Pir             Genbank         Pub
//   Region       User            Sp              Dbxref
//   Embl         Create_date     Update_date     Prf
//   Pdb          Het             Source          Molinfo


/// CMolInfo definitions

#define NCBI_BIOMOL(Type) CMolInfo::eBiomol_##Type
typedef CMolInfo::TBiomol TMOLINFO_BIOMOL;

//   genomic             pre_RNA          mRNA      rRNA
//   tRNA                snRNA            scRNA     peptide
//   other_genetic       genomic_mRNA     cRNA      snoRNA
//   transcribed_RNA     ncRNA            tmRNA     other

#define NCBI_TECH(Type) CMolInfo::eTech_##Type
typedef CMolInfo::TTech TMOLINFO_TECH;

//   standard               est                  sts
//   survey                 genemap              physmap
//   derived                concept_trans        seq_pept
//   both                   seq_pept_overlap     seq_pept_homol
//   concept_trans_a        htgs_1               htgs_2
//   htgs_3                 fli_cdna             htgs_0
//   htc                    wgs                  barcode
//   composite_wgs_htgs     tsa                  other

#define NCBI_COMPLETENESS(Type) CMolInfo::eCompleteness_##Type
typedef CMolInfo::TCompleteness TMOLINFO_COMPLETENESS;

//   complete     partial      no_left       no_right
//   no_ends      has_left     has_right     other


/// CBioSource definitions

#define NCBI_GENOME(Type) CBioSource::eGenome_##Type
typedef CBioSource::TGenome TBIOSOURCE_GENOME;

//   genomic              chloroplast       chromoplast
//   kinetoplast          mitochondrion     plastid
//   macronuclear         extrachrom        plasmid
//   transposon           insertion_seq     cyanelle
//   proviral             virion            nucleomorph
//   apicoplast           leucoplast        proplastid
//   endogenous_virus     hydrogenosome     chromosome
//   chromatophore

#define NCBI_ORIGIN(Type) CBioSource::eOrigin_##Type
typedef CBioSource::TOrigin TBIOSOURCE_ORIGIN;

//   natural       natmut     mut     artificial
//   synthetic     other


/// COrgName definitions

#define NCBI_ORGNAME(Type) COrgName::e_##Type
typedef COrgName::C_Name::E_Choice TORGNAME_CHOICE;

//   Binomial     Virus     Hybrid     Namedhybrid     Partial


/// CSubSource definitions

#define NCBI_SUBSOURCE(Type) CSubSource::eSubtype_##Type
typedef CSubSource::TSubtype TSUBSOURCE_SUBTYPE;

//   chromosome                map                 clone
//   subclone                  haplotype           genotype
//   sex                       cell_line           cell_type
//   tissue_type               clone_lib           dev_stage
//   frequency                 germline            rearranged
//   lab_host                  pop_variant         tissue_lib
//   plasmid_name              transposon_name     insertion_seq_name
//   plastid_name              country             segment
//   endogenous_virus_name     transgenic          environmental_sample
//   isolation_source          lat_lon             collection_date
//   collected_by              identified_by       fwd_primer_seq
//   rev_primer_seq            fwd_primer_name     rev_primer_name
//   metagenomic               mating_type         linkage_group
//   haplogroup                other


/// COrgMod definitions

#define NCBI_ORGMOD(Type) COrgMod::eSubtype_##Type
typedef COrgMod::TSubtype TORGMOD_SUBTYPE;

//   strain                 substrain        type
//   subtype                variety          serotype
//   serogroup              serovar          cultivar
//   pathovar               chemovar         biovar
//   biotype                group            subgroup
//   isolate                common           acronym
//   dosage                 nat_host         sub_species
//   specimen_voucher       authority        forma
//   forma_specialis        ecotype          synonym
//   anamorph               teleomorph       breed
//   gb_acronym             gb_anamorph      gb_synonym
//   culture_collection     bio_material     metagenome_source
//   old_lineage            old_name         other


/// CPub definitions

#define NCBI_PUB(Type) CPub::e_##Type
typedef CPub::E_Choice TPUB_CHOICE;

//   Gen         Sub       Medline     Muid       Article
//   Journal     Book      Proc        Patent     Pat_id
//   Man         Equiv     Pmid


/// CSeq_feat definitions

#define NCBI_SEQFEAT(Type) CSeqFeatData::e_##Type
typedef CSeqFeatData::E_Choice TSEQFEAT_CHOICE;

//   Gene         Org                 Cdregion     Prot
//   Rna          Pub                 Seq          Imp
//   Region       Comment             Bond         Site
//   Rsite        User                Txinit       Num
//   Psec_str     Non_std_residue     Het          Biosrc



/////////////////////////////////////////////////////////////////////////////
/// Macros for obtaining closest specific CSeqdesc applying to a CBioseq
/////////////////////////////////////////////////////////////////////////////


/// IF_EXISTS_CLOSEST base macro calls GetClosestDescriptor with generated components
// If Lvl is not NULL, it must be a pointer to an int

#define IF_EXISTS_CLOSEST(Cref, Var, Lvl, Chs) \
if (CConstRef<CSeqdesc> Cref = (Var).GetClosestDescriptor (Chs, Lvl))


/// IF_EXISTS_CLOSEST_MOLINFO
// CBioseq& as input, dereference with const CMolInfo& molinf = (*cref).GetMolinfo();

#define IF_EXISTS_CLOSEST_MOLINFO(Cref, Var, Lvl) \
IF_EXISTS_CLOSEST (Cref, Var, Lvl, NCBI_SEQDESC(Molinfo))

/// IF_EXISTS_CLOSEST_BIOSOURCE
// CBioseq& as input, dereference with const CBioSource& source = (*cref).GetSource();

#define IF_EXISTS_CLOSEST_BIOSOURCE(Cref, Var, Lvl) \
IF_EXISTS_CLOSEST (Cref, Var, Lvl, NCBI_SEQDESC(Source))

/// IF_EXISTS_CLOSEST_TITLE
// CBioseq& as input, dereference with const string& title = (*cref).GetTitle();

#define IF_EXISTS_CLOSEST_TITLE(Cref, Var, Lvl) \
IF_EXISTS_CLOSEST (Cref, Var, Lvl, NCBI_SEQDESC(Title))

/// IF_EXISTS_CLOSEST_GENBANKBLOCK
// CBioseq& as input, dereference with const CGB_block& gbk = (*cref).GetGenbank();

#define IF_EXISTS_CLOSEST_GENBANKBLOCK(Cref, Var, Lvl) \
IF_EXISTS_CLOSEST (Cref, Var, Lvl, NCBI_SEQDESC(Genbank))

/// IF_EXISTS_CLOSEST_EMBLBLOCK
// CBioseq& as input, dereference with const CEMBL_block& ebk = (*cref).GetEmbl();

#define IF_EXISTS_CLOSEST_EMBLBLOCK(Cref, Var, Lvl) \
IF_EXISTS_CLOSEST (Cref, Var, Lvl, NCBI_SEQDESC(Embl))

/// IF_EXISTS_CLOSEST_PDBBLOCK
// CBioseq& as input, dereference with const CPDB_block& pbk = (*cref).GetPdb();

#define IF_EXISTS_CLOSEST_PDBBLOCK(Cref, Var, Lvl) \
IF_EXISTS_CLOSEST (Cref, Var, Lvl, NCBI_SEQDESC(Pdb))



/////////////////////////////////////////////////////////////////////////////
/// Macros to recursively explore within NCBI data model objects
/////////////////////////////////////////////////////////////////////////////


/// NCBI_SERIAL_TEST_EXPLORE base macro tests to see if loop should be entered
// If okay, calls CTypeConstIterator for recursive exploration

#define NCBI_SERIAL_TEST_EXPLORE(Test, Type, Var, Cont) \
if (! (Test)) {} else for (CTypeConstIterator<Type> Var (Cont); Var; ++Var)


/// VISIT_WITHIN_SEQENTRY base macro makes recursive iterator with generated components
/// VISIT_WITHIN_SEQSET base macro makes recursive iterator with generated components

#define VISIT_WITHIN_SEQENTRY(Typ, Itr, Var) \
NCBI_SERIAL_TEST_EXPLORE ((Var).Which() != CSeq_entry::e_not_set, Typ, Itr, (Var))

#define VISIT_WITHIN_SEQSET(Typ, Itr, Var) \
NCBI_SERIAL_TEST_EXPLORE ((Var).IsSetSeq_set(), Typ, Itr, (Var))


// "VISIT_ALL_XXX_WITHIN_YYY" does a recursive exploration of NCBI objects


/// CSeq_entry explorers

/// VISIT_ALL_SEQENTRYS_WITHIN_SEQENTRY
// CSeq_entry& as input, dereference with const CSeq_entry& seqentry = *itr;

#define VISIT_ALL_SEQENTRYS_WITHIN_SEQENTRY(Itr, Var) \
VISIT_WITHIN_SEQENTRY (CSeq_entry, Itr, Var)

/// VISIT_ALL_BIOSEQS_WITHIN_SEQENTRY
// CSeq_entry& as input, dereference with const CBioseq& bioseq = *itr;

#define VISIT_ALL_BIOSEQS_WITHIN_SEQENTRY(Itr, Var) \
VISIT_WITHIN_SEQENTRY (CBioseq, Itr, Var)

/// VISIT_ALL_SEQSETS_WITHIN_SEQENTRY
// CSeq_entry& as input, dereference with const CBioseq_set& bss = *itr;

#define VISIT_ALL_SEQSETS_WITHIN_SEQENTRY(Itr, Var) \
VISIT_WITHIN_SEQENTRY (CBioseq_set, Itr, Var)

/// VISIT_ALL_SEQDESCS_WITHIN_SEQENTRY
// CSeq_entry& as input, dereference with const CSeqdesc& desc = *itr;

#define VISIT_ALL_SEQDESCS_WITHIN_SEQENTRY(Itr, Var) \
VISIT_WITHIN_SEQENTRY (CSeqdesc, Itr, Var)

#define VISIT_ALL_DESCRIPTORS_WITHIN_SEQENTRY VISIT_ALL_SEQDESCS_WITHIN_SEQENTRY

/// VISIT_ALL_SEQANNOTS_WITHIN_SEQENTRY
// CSeq_entry& as input, dereference with const CSeq_annot& annot = *itr;

#define VISIT_ALL_SEQANNOTS_WITHIN_SEQENTRY(Itr, Var) \
VISIT_WITHIN_SEQENTRY (CSeq_annot, Itr, Var)

#define VISIT_ALL_ANNOTS_WITHIN_SEQENTRY VISIT_ALL_SEQANNOTS_WITHIN_SEQENTRY

/// VISIT_ALL_SEQFEATS_WITHIN_SEQENTRY
// CSeq_entry& as input, dereference with const CSeq_feat& feat = *itr;

#define VISIT_ALL_SEQFEATS_WITHIN_SEQENTRY(Itr, Var) \
VISIT_WITHIN_SEQENTRY (CSeq_feat, Itr, Var)

#define VISIT_ALL_FEATURES_WITHIN_SEQENTRY VISIT_ALL_SEQFEATS_WITHIN_SEQENTRY

/// VISIT_ALL_SEQALIGNS_WITHIN_SEQENTRY
// CSeq_entry& as input, dereference with const CSeq_align& align = *itr;

#define VISIT_ALL_SEQALIGNS_WITHIN_SEQENTRY(Itr, Var) \
VISIT_WITHIN_SEQENTRY (CSeq_align, Itr, Var)

#define VISIT_ALL_ALIGNS_WITHIN_SEQENTRY VISIT_ALL_SEQALIGNS_WITHIN_SEQENTRY

/// VISIT_ALL_SEQGRAPHS_WITHIN_SEQENTRY
// CSeq_entry& as input, dereference with const CSeq_graph& graph = *itr;

#define VISIT_ALL_SEQGRAPHS_WITHIN_SEQENTRY(Itr, Var) \
VISIT_WITHIN_SEQENTRY (CSeq_graph, Itr, Var)

#define VISIT_ALL_GRAPHS_WITHIN_SEQENTRY VISIT_ALL_SEQGRAPHS_WITHIN_SEQENTRY


/// CBioseq_set explorers

/// VISIT_ALL_SEQENTRYS_WITHIN_SEQSET
// CBioseq_set& as input, dereference with const CSeq_entry& seqentry = *itr;

#define VISIT_ALL_SEQENTRYS_WITHIN_SEQSET(Itr, Var) \
VISIT_WITHIN_SEQSET (CSeq_entry, Itr, Var)

/// VISIT_ALL_BIOSEQS_WITHIN_SEQSET
// CBioseq_set& as input, dereference with const CBioseq& bioseq = *itr;

#define VISIT_ALL_BIOSEQS_WITHIN_SEQSET(Itr, Var) \
VISIT_WITHIN_SEQSET (CBioseq, Itr, Var)

/// VISIT_ALL_SEQSETS_WITHIN_SEQSET
// CBioseq_set& as input, dereference with const CBioseq_set& bss = *itr;

#define VISIT_ALL_SEQSETS_WITHIN_SEQSET(Itr, Var) \
VISIT_WITHIN_SEQSET (CBioseq_set, Itr, Var)

/// VISIT_ALL_SEQDESCS_WITHIN_SEQSET
// CBioseq_set& as input, dereference with const CSeqdesc& desc = *itr;

#define VISIT_ALL_SEQDESCS_WITHIN_SEQSET(Itr, Var) \
VISIT_WITHIN_SEQSET (CSeqdesc, Itr, Var)

#define VISIT_ALL_DESCRIPTORS_WITHIN_SEQSET VISIT_ALL_SEQDESCS_WITHIN_SEQSET

/// VISIT_ALL_SEQANNOTS_WITHIN_SEQSET
// CBioseq_set& as input, dereference with const CSeq_annot& annot = *itr;

#define VISIT_ALL_SEQANNOTS_WITHIN_SEQSET(Itr, Var) \
VISIT_WITHIN_SEQSET (CSeq_annot, Itr, Var)

#define VISIT_ALL_ANNOTS_WITHIN_SEQSET VISIT_ALL_SEQANNOTS_WITHIN_SEQSET

/// VISIT_ALL_SEQFEATS_WITHIN_SEQSET
// CBioseq_set& as input, dereference with const CSeq_feat& feat = *itr;

#define VISIT_ALL_SEQFEATS_WITHIN_SEQSET(Itr, Var) \
VISIT_WITHIN_SEQSET (CSeq_feat, Itr, Var)

#define VISIT_ALL_FEATURES_WITHIN_SEQSET VISIT_ALL_SEQFEATS_WITHIN_SEQSET

/// VISIT_ALL_SEQALIGNS_WITHIN_SEQSET
// CBioseq_set& as input, dereference with const CSeq_align& align = *itr;

#define VISIT_ALL_SEQALIGNS_WITHIN_SEQSET(Itr, Var) \
VISIT_WITHIN_SEQSET (CSeq_align, Itr, Var)

#define VISIT_ALL_ALIGNS_WITHIN_SEQSET VISIT_ALL_SEQALIGNS_WITHIN_SEQSET

/// VISIT_ALL_SEQGRAPHS_WITHIN_SEQSET
// CBioseq_set& as input, dereference with const CSeq_graph& graph = *itr;

#define VISIT_ALL_SEQGRAPHS_WITHIN_SEQSET(Itr, Var) \
VISIT_WITHIN_SEQSET (CSeq_graph, Itr, Var)

#define VISIT_ALL_GRAPHS_WITHIN_SEQSET VISIT_ALL_SEQGRAPHS_WITHIN_SEQSET



/////////////////////////////////////////////////////////////////////////////
/// Macros to iterate over standard template containers (non-recursive)
/////////////////////////////////////////////////////////////////////////////


/// NCBI_LIST_ERASE_ITERATE iterates lists allowing deletion of any object

#define NCBI_LIST_ERASE_ITERATE(Type, Var, Cont) \
for ( Type::iterator NCBI_NAME2(Var,_next) = (Cont).begin(), \
      Var = NCBI_NAME2(Var,_next) != (Cont).end() ? \
      NCBI_NAME2(Var,_next)++ : (Cont).end(); \
      Var != (Cont).end(); \
      Var = NCBI_NAME2(Var,_next) != (Cont).end() ? \
      NCBI_NAME2(Var,_next)++ : (Cont).end())

/// NCBI_CS_ITERATE base macro tests to see if loop should be entered
// If okay, calls ITERATE for linear STL iteration

#define NCBI_CS_ITERATE(Test, Type, Var, Cont) \
if (! (Test)) {} else ITERATE (Type, Var, Cont)

/// NCBI_NC_ITERATE base macro tests to see if loop should be entered
// If okay, calls NCBI_LIST_ERASE_ITERATE for linear STL iteration

#define NCBI_NC_ITERATE(Test, Type, Var, Cont) \
if (! (Test)) {} else NCBI_LIST_ERASE_ITERATE (Type, Var, Cont)

/// NCBI_SWITCH base macro tests to see if switch should be performed
// If okay, calls switch statement

#define NCBI_SWITCH(Test, Chs) \
if (! (Test)) {} else switch(Chs)


/// FOR_EACH base macro calls NCBI_CS_ITERATE with generated components

#define FOR_EACH(Base, Itr, Var) \
NCBI_CS_ITERATE (Base##_Test(Var), Base##_Type, Itr, Base##_Get(Var))

/// EDIT_EACH base macro calls NCBI_NC_ITERATE with generated components

#define EDIT_EACH(Base, Itr, Var) \
NCBI_NC_ITERATE (Base##_Test(Var), Base##_Type, Itr, Base##_Set(Var))

/// ERASE_ITEM base macro

#define ERASE_ITEM(Base, Itr, Var) \
(Base##_Set(Var).erase(Itr))


/// IF_HAS base macro

#define IF_HAS(Base, Var) \
if (Base##_Test(Var))

/// ITEM_HAS base macro

#define ITEM_HAS(Base, Var) \
(Base##_Test(Var))

/// IF_CHOICE base macro

#define IF_CHOICE(Base, Var, Chs) \
if (Base##_Test(Var) && Base##_Chs(Var) == Chs)

/// CHOICE_IS base macro

#define CHOICE_IS(Base, Var, Chs) \
(Base##_Test(Var) && Base##_Chs(Var) == Chs)


/// SWITCH_ON base macro calls NCBI_SWITCH with generated components

#define SWITCH_ON(Base, Var) \
NCBI_SWITCH (Base##_Test(Var), Base##_Chs(Var))


// "FOR_EACH_XXX_ON_YYY" does a linear const traversal of STL containers
// "EDIT_EACH_XXX_ON_YYY" does a linear non-const traversal of STL containers

// "SWITCH_ON_XXX_CHOICE" switches on the item subtype

// "ERASE_XXX_ON_YYY" deletes a specified object within an iterator

// Miscellaneous macros for testing objects include
// "IF_XXX_IS_YYY" or "IF_XXX_IS_YYY"
// "XXX_IS_YYY" or "XXX_HAS_YYY"
// "IF_XXX_CHOICE_IS"
// "XXX_CHOICE_IS"


/// CSeq_submit macros

#define SEQENTRY_ON_SEQSUBMIT_Type      CSeq_submit::TData::TEntrys
#define SEQENTRY_ON_SEQSUBMIT_Test(Var) (Var).IsEntrys()
#define SEQENTRY_ON_SEQSUBMIT_Get(Var)  (Var).GetData().GetEntrys()
#define SEQENTRY_ON_SEQSUBMIT_Set(Var)  (Var).SetData().SetEntrys()

/// FOR_EACH_SEQENTRY_ON_SEQSUBMIT
/// EDIT_EACH_SEQENTRY_ON_SEQSUBMIT
// CSeq_submit& as input, dereference with [const] CSeq_entry& se = **itr;

#define FOR_EACH_SEQENTRY_ON_SEQSUBMIT(Itr, Var) \
FOR_EACH (SEQENTRY_ON_SEQSUBMIT, Itr, Var)

#define EDIT_EACH_SEQENTRY_ON_SEQSUBMIT(Itr, Var) \
EDIT_EACH (SEQENTRY_ON_SEQSUBMIT, Itr, Var)

/// ERASE_SEQENTRY_ON_SEQSUBMIT

#define ERASE_SEQENTRY_ON_SEQSUBMIT(Itr, Var) \
ERASE_ITEM (SEQENTRY_ON_SEQSUBMIT, Itr, Var)


/// CSeq_entry macros

/// SEQENTRY_CHOICE macros

#define SEQENTRY_CHOICE_Test(Var) (Var).Which() != CSeq_entry::e_not_set
#define SEQENTRY_CHOICE_Chs(Var)  (Var).Which()

/// IF_SEQENTRY_CHOICE_IS

#define IF_SEQENTRY_CHOICE_IS(Var, Chs) \
IF_CHOICE (SEQENTRY_CHOICE, Var, Chs)

/// SEQENTRY_CHOICE_IS

#define SEQENTRY_CHOICE_IS(Var, Chs) \
CHOICE_IS (SEQENTRY_CHOICE, Var, Chs)

/// IF_SEQENTRY_IS_SEQ

#define IF_SEQENTRY_IS_SEQ(Var) \
IF_SEQENTRY_CHOICE_IS (Var, NCBI_SEQENTRY(Seq))

/// SEQENTRY_IS_SEQ

#define SEQENTRY_IS_SEQ(Var) \
EQENTRY_CHOICE_IS (Var, NCBI_SEQENTRY(Seq))

/// IF_SEQENTRY_IS_SET

#define IF_SEQENTRY_IS_SET(Var) \
IF_SEQENTRY_CHOICE_IS (Var, NCBI_SEQENTRY(Set))

/// SEQENTRY_IS_SET

#define SEQENTRY_IS_SET(Var) \
SEQENTRY_CHOICE_IS (Var, NCBI_SEQENTRY(Set))

/// SWITCH_ON_SEQENTRY_CHOICE

#define SWITCH_ON_SEQENTRY_CHOICE(Var) \
SWITCH_ON (SEQENTRY_CHOICE, Var)


/// SEQDESC_ON_SEQENTRY macros

#define SEQDESC_ON_SEQENTRY_Type      CSeq_descr::Tdata
#define SEQDESC_ON_SEQENTRY_Test(Var) (Var).IsSetDescr()
#define SEQDESC_ON_SEQENTRY_Get(Var)  (Var).GetDescr().Get()
#define SEQDESC_ON_SEQENTRY_Set(Var)  (Var).SetDescr().Set()

/// IF_SEQENTRY_HAS_SEQDESC

#define IF_SEQENTRY_HAS_SEQDESC(Var) \
IF_HAS (SEQDESC_ON_SEQENTRY, Var)

/// SEQENTRY_HAS_SEQDESC

#define SEQENTRY_HAS_SEQDESC(Var) \
ITEM_HAS (SEQDESC_ON_SEQENTRY, Var)

/// FOR_EACH_SEQDESC_ON_SEQENTRY
/// EDIT_EACH_SEQDESC_ON_SEQENTRY
// CSeq_entry& as input, dereference with [const] CSeqdesc& desc = **itr

#define FOR_EACH_SEQDESC_ON_SEQENTRY(Itr, Var) \
FOR_EACH (SEQDESC_ON_SEQENTRY, Itr, Var)

#define EDIT_EACH_SEQDESC_ON_SEQENTRY(Itr, Var) \
EDIT_EACH (SEQDESC_ON_SEQENTRY, Itr, Var)

/// ERASE_SEQDESC_ON_SEQENTRY

#define ERASE_SEQDESC_ON_SEQENTRY(Itr, Var) \
ERASE_ITEM (SEQDESC_ON_SEQENTRY, Itr, Var)

/// IF_SEQENTRY_HAS_DESCRIPTOR
/// SEQENTRY_HAS_DESCRIPTOR
/// FOR_EACH_DESCRIPTOR_ON_SEQENTRY
/// EDIT_EACH_DESCRIPTOR_ON_SEQENTRY
/// ERASE_DESCRIPTOR_ON_SEQENTRY

#define IF_SEQENTRY_HAS_DESCRIPTOR IF_SEQENTRY_HAS_SEQDESC
#define SEQENTRY_HAS_DESCRIPTOR SEQENTRY_HAS_SEQDESC
#define FOR_EACH_DESCRIPTOR_ON_SEQENTRY FOR_EACH_SEQDESC_ON_SEQENTRY
#define EDIT_EACH_DESCRIPTOR_ON_SEQENTRY EDIT_EACH_SEQDESC_ON_SEQENTRY
#define ERASE_DESCRIPTOR_ON_SEQENTRY ERASE_SEQDESC_ON_SEQENTRY


/// SEQANNOT_ON_SEQENTRY macros

#define SEQANNOT_ON_SEQENTRY_Type      CSeq_entry::TAnnot
#define SEQANNOT_ON_SEQENTRY_Test(Var) (Var).IsSetAnnot()
#define SEQANNOT_ON_SEQENTRY_Get(Var)  (Var).GetAnnot()
#define SEQANNOT_ON_SEQENTRY_Set(Var)  (Var).(Seq).SetAnnot()

/// IF_SEQENTRY_HAS_SEQANNOT

#define IF_SEQENTRY_HAS_SEQANNOT(Var) \
IF_HAS (SEQANNOT_ON_SEQENTRY, Var)

/// SEQENTRY_HAS_SEQANNOT

#define SEQENTRY_HAS_SEQANNOT(Var) \
ITEM_HAS (SEQANNOT_ON_SEQENTRY, Var)

/// FOR_EACH_SEQANNOT_ON_SEQENTRY
/// EDIT_EACH_SEQANNOT_ON_SEQENTRY
// CSeq_entry& as input, dereference with [const] CSeq_annot& annot = **itr;

#define FOR_EACH_SEQANNOT_ON_SEQENTRY(Itr, Var) \
FOR_EACH (SEQANNOT_ON_SEQENTRY, Itr, Var)

#define EDIT_EACH_SEQANNOT_ON_SEQENTRY(Itr, Var) \
EDIT_EACH (SEQANNOT_ON_SEQENTRY, Itr, Var)

/// ERASE_SEQANNOT_ON_SEQENTRY

#define ERASE_SEQANNOT_ON_SEQENTRY(Itr, Var) \
ERASE_ITEM (SEQANNOT_ON_SEQENTRY, Itr, Var)

/// IF_SEQENTRY_HAS_ANNOT
/// SEQENTRY_HAS_ANNOT
/// FOR_EACH_ANNOT_ON_SEQENTRY
/// EDIT_EACH_ANNOT_ON_SEQENTRY
/// ERASE_ANNOT_ON_SEQENTRY

#define IF_SEQENTRY_HAS_ANNOT IF_SEQENTRY_HAS_SEQANNOT
#define SEQENTRY_HAS_ANNOT SEQENTRY_HAS_SEQANNOT
#define FOR_EACH_ANNOT_ON_SEQENTRY FOR_EACH_SEQANNOT_ON_SEQENTRY
#define EDIT_EACH_ANNOT_ON_SEQENTRY EDIT_EACH_SEQANNOT_ON_SEQENTRY
#define ERASE_ANNOT_ON_SEQENTRY ERASE_SEQANNOT_ON_SEQENTRY


/// CBioseq macros

/// SEQDESC_ON_BIOSEQ macros

#define SEQDESC_ON_BIOSEQ_Type      CBioseq::TDescr::Tdata
#define SEQDESC_ON_BIOSEQ_Test(Var) (Var).IsSetDescr()
#define SEQDESC_ON_BIOSEQ_Get(Var)  (Var).GetDescr().Get()
#define SEQDESC_ON_BIOSEQ_Set(Var)  (Var).SetDescr().Set()

/// IF_BIOSEQ_HAS_SEQDESC

#define IF_BIOSEQ_HAS_SEQDESC(Var) \
IF_HAS (SEQDESC_ON_BIOSEQ, Var)

/// BIOSEQ_HAS_SEQDESC

#define BIOSEQ_HAS_SEQDESC(Var) \
ITEM_HAS (SEQDESC_ON_BIOSEQ, Var)

/// FOR_EACH_SEQDESC_ON_BIOSEQ
/// EDIT_EACH_SEQDESC_ON_BIOSEQ
// CBioseq& as input, dereference with [const] CSeqdesc& desc = **itr

#define FOR_EACH_SEQDESC_ON_BIOSEQ(Itr, Var) \
FOR_EACH (SEQDESC_ON_BIOSEQ, Itr, Var)

#define EDIT_EACH_SEQDESC_ON_BIOSEQ(Itr, Var) \
EDIT_EACH (SEQDESC_ON_BIOSEQ, Itr, Var)

/// ERASE_SEQDESC_ON_BIOSEQ

#define ERASE_SEQDESC_ON_BIOSEQ(Itr, Var) \
ERASE_ITEM (SEQDESC_ON_BIOSEQ, Itr, Var)

/// IF_BIOSEQ_HAS_DESCRIPTOR
/// BIOSEQ_HAS_DESCRIPTOR
/// FOR_EACH_DESCRIPTOR_ON_BIOSEQ
/// EDIT_EACH_DESCRIPTOR_ON_BIOSEQ
/// ERASE_DESCRIPTOR_ON_BIOSEQ

#define IF_BIOSEQ_HAS_DESCRIPTOR IF_BIOSEQ_HAS_SEQDESC
#define BIOSEQ_HAS_DESCRIPTOR BIOSEQ_HAS_SEQDESC
#define FOR_EACH_DESCRIPTOR_ON_BIOSEQ FOR_EACH_SEQDESC_ON_BIOSEQ
#define EDIT_EACH_DESCRIPTOR_ON_BIOSEQ EDIT_EACH_SEQDESC_ON_BIOSEQ
#define ERASE_DESCRIPTOR_ON_BIOSEQ ERASE_SEQDESC_ON_BIOSEQ


/// SEQANNOT_ON_BIOSEQ macros

#define SEQANNOT_ON_BIOSEQ_Type      CBioseq::TAnnot
#define SEQANNOT_ON_BIOSEQ_Test(Var) (Var).IsSetAnnot()
#define SEQANNOT_ON_BIOSEQ_Get(Var)  (Var).GetAnnot()
#define SEQANNOT_ON_BIOSEQ_Set(Var)  (Var).SetAnnot()

/// IF_BIOSEQ_HAS_SEQANNOT

#define IF_BIOSEQ_HAS_SEQANNOT(Var) \
IF_HAS (SEQANNOT_ON_BIOSEQ, Var)

/// BIOSEQ_HAS_SEQANNOT

#define BIOSEQ_HAS_SEQANNOT(Var) \
ITEM_HAS (SEQANNOT_ON_BIOSEQ, Var)

/// FOR_EACH_SEQANNOT_ON_BIOSEQ
/// EDIT_EACH_SEQANNOT_ON_BIOSEQ
// CBioseq& as input, dereference with [const] CSeq_annot& annot = **itr;

#define FOR_EACH_SEQANNOT_ON_BIOSEQ(Itr, Var) \
FOR_EACH (SEQANNOT_ON_BIOSEQ, Itr, Var)

#define EDIT_EACH_SEQANNOT_ON_BIOSEQ(Itr, Var) \
EDIT_EACH (SEQANNOT_ON_BIOSEQ, Itr, Var)

/// ERASE_SEQANNOT_ON_BIOSEQ

#define ERASE_SEQANNOT_ON_BIOSEQ(Itr, Var) \
ERASE_ITEM (SEQANNOT_ON_BIOSEQ, Itr, Var)

/// IF_BIOSEQ_HAS_ANNOT
/// BIOSEQ_HAS_ANNOT
/// FOR_EACH_ANNOT_ON_BIOSEQ
/// EDIT_EACH_ANNOT_ON_BIOSEQ
/// ERASE_ANNOT_ON_BIOSEQ

#define IF_BIOSEQ_HAS_ANNOT IF_BIOSEQ_HAS_SEQANNOT
#define BIOSEQ_HAS_ANNOT BIOSEQ_HAS_SEQANNOT
#define FOR_EACH_ANNOT_ON_BIOSEQ FOR_EACH_SEQANNOT_ON_BIOSEQ
#define EDIT_EACH_ANNOT_ON_BIOSEQ EDIT_EACH_SEQANNOT_ON_BIOSEQ
#define ERASE_ANNOT_ON_BIOSEQ ERASE_SEQANNOT_ON_BIOSEQ


/// SEQID_ON_BIOSEQ macros

#define SEQID_ON_BIOSEQ_Type      CBioseq::TId
#define SEQID_ON_BIOSEQ_Test(Var) (Var).IsSetId()
#define SEQID_ON_BIOSEQ_Get(Var)  (Var).GetId()
#define SEQID_ON_BIOSEQ_Set(Var)  (Var).SetId()

/// IF_BIOSEQ_HAS_SEQID

#define IF_BIOSEQ_HAS_SEQID(Var) \
IF_HAS (SEQID_ON_BIOSEQ, Var)

/// BIOSEQ_HAS_SEQID

#define BIOSEQ_HAS_SEQID(Var) \
ITEM_HAS (SEQID_ON_BIOSEQ, Var)

/// FOR_EACH_SEQID_ON_BIOSEQ
/// EDIT_EACH_SEQID_ON_BIOSEQ
// CBioseq& as input, dereference with [const] CSeq_id& sid = **itr;

#define FOR_EACH_SEQID_ON_BIOSEQ(Itr, Var) \
FOR_EACH (SEQID_ON_BIOSEQ, Itr, Var)

#define EDIT_EACH_SEQID_ON_BIOSEQ(Itr, Var) \
EDIT_EACH (SEQID_ON_BIOSEQ, Itr, Var)

/// ERASE_SEQID_ON_BIOSEQ

#define ERASE_SEQID_ON_BIOSEQ(Itr, Var) \
ERASE_ITEM (SEQID_ON_BIOSEQ, Itr, Var)


/// CSeq_id macros

/// SEQID_CHOICE macros

#define SEQID_CHOICE_Test(Var) (Var).Which() != CSeq_id::e_not_set
#define SEQID_CHOICE_Chs(Var)  (Var).Which()

/// IF_SEQID_CHOICE_IS

#define IF_SEQID_CHOICE_IS(Var, Chs) \
IF_CHOICE (SEQID_CHOICE, Var, Chs)

/// SEQID_CHOICE_IS

#define SEQID_CHOICE_IS(Var, Chs) \
CHOICE_IS (SEQID_CHOICE, Var, Chs)

/// SWITCH_ON_SEQID_CHOICE

#define SWITCH_ON_SEQID_CHOICE(Var) \
SWITCH_ON (SEQID_CHOICE, Var)


/// CBioseq_set macros

/// SEQDESC_ON_SEQSET macros

#define SEQDESC_ON_SEQSET_Type      CBioseq_set::TDescr::Tdata
#define SEQDESC_ON_SEQSET_Test(Var) (Var).IsSetDescr()
#define SEQDESC_ON_SEQSET_Get(Var)  (Var).GetDescr().Get()
#define SEQDESC_ON_SEQSET_Set(Var)  (Var).SetDescr().Set()

/// IF_SEQSET_HAS_SEQDESC

#define IF_SEQSET_HAS_SEQDESC(Var) \
IF_HAS (SEQDESC_ON_SEQSET, Var)

/// SEQSET_HAS_SEQDESC

#define SEQSET_HAS_SEQDESC(Var) \
ITEM_HAS (SEQDESC_ON_SEQSET, Var)

/// FOR_EACH_SEQDESC_ON_SEQSET
/// EDIT_EACH_SEQDESC_ON_SEQSET
// CBioseq_set& as input, dereference with [const] CSeqdesc& desc = **itr;

#define FOR_EACH_SEQDESC_ON_SEQSET(Itr, Var) \
FOR_EACH (SEQDESC_ON_SEQSET, Itr, Var)

#define EDIT_EACH_SEQDESC_ON_SEQSET(Itr, Var) \
EDIT_EACH (SEQDESC_ON_SEQSET, Itr, Var)

/// ERASE_SEQDESC_ON_SEQSET

#define ERASE_SEQDESC_ON_SEQSET(Itr, Var) \
ERASE_ITEM (SEQDESC_ON_SEQSET, Itr, Var)

/// IF_SEQSET_HAS_DESCRIPTOR
/// SEQSET_HAS_DESCRIPTOR
/// FOR_EACH_DESCRIPTOR_ON_SEQSET
/// EDIT_EACH_DESCRIPTOR_ON_SEQSET
/// ERASE_DESCRIPTOR_ON_SEQSET

#define IF_SEQSET_HAS_DESCRIPTOR IF_SEQSET_HAS_SEQDESC
#define SEQSET_HAS_DESCRIPTOR SEQSET_HAS_SEQDESC
#define FOR_EACH_DESCRIPTOR_ON_SEQSET FOR_EACH_SEQDESC_ON_SEQSET
#define EDIT_EACH_DESCRIPTOR_ON_SEQSET EDIT_EACH_SEQDESC_ON_SEQSET
#define ERASE_DESCRIPTOR_ON_SEQSET ERASE_SEQDESC_ON_SEQSET


/// SEQANNOT_ON_SEQSET macros

#define SEQANNOT_ON_SEQSET_Type      CBioseq_set::TAnnot
#define SEQANNOT_ON_SEQSET_Test(Var) (Var).IsSetAnnot()
#define SEQANNOT_ON_SEQSET_Get(Var)  (Var).GetAnnot()
#define SEQANNOT_ON_SEQSET_Set(Var)  (Var).SetAnnot()

/// IF_SEQSET_HAS_SEQANNOT

#define IF_SEQSET_HAS_SEQANNOT(Var) \
IF_HAS (SEQANNOT_ON_SEQSET, Var)

/// SEQSET_HAS_SEQANNOT

#define SEQSET_HAS_SEQANNOT(Var) \
ITEM_HAS (SEQANNOT_ON_SEQSET, Var)

/// FOR_EACH_SEQANNOT_ON_SEQSET
/// EDIT_EACH_SEQANNOT_ON_SEQSET
// CBioseq_set& as input, dereference with [const] CSeq_annot& annot = **itr;

#define FOR_EACH_SEQANNOT_ON_SEQSET(Itr, Var) \
FOR_EACH (SEQANNOT_ON_SEQSET, Itr, Var)

#define EDIT_EACH_SEQANNOT_ON_SEQSET(Itr, Var) \
EDIT_EACH (SEQANNOT_ON_SEQSET, Itr, Var)

/// ERASE_SEQANNOT_ON_SEQSET

#define ERASE_SEQANNOT_ON_SEQSET(Itr, Var) \
ERASE_ITEM (SEQANNOT_ON_SEQSET, Itr, Var)

/// IF_SEQSET_HAS_ANNOT
/// SEQSET_HAS_ANNOT
/// FOR_EACH_ANNOT_ON_SEQSET
/// EDIT_EACH_ANNOT_ON_SEQSET
/// ERASE_ANNOT_ON_SEQSET

#define IF_SEQSET_HAS_ANNOT IF_SEQSET_HAS_SEQANNOT
#define SEQSET_HAS_ANNOT SEQSET_HAS_SEQANNOT
#define FOR_EACH_ANNOT_ON_SEQSET FOR_EACH_SEQANNOT_ON_SEQSET
#define EDIT_EACH_ANNOT_ON_SEQSET EDIT_EACH_SEQANNOT_ON_SEQSET
#define ERASE_ANNOT_ON_SEQSET ERASE_SEQANNOT_ON_SEQSET


/// SEQENTRY_ON_SEQSET macros

#define SEQENTRY_ON_SEQSET_Type      CBioseq_set::TSeq_set
#define SEQENTRY_ON_SEQSET_Test(Var) (Var).IsSetSeq_set()
#define SEQENTRY_ON_SEQSET_Get(Var)  (Var).GetSeq_set()
#define SEQENTRY_ON_SEQSET_Set(Var)  (Var).SetSeq_set()

/// IF_SEQSET_HAS_SEQENTRY

#define IF_SEQSET_HAS_SEQENTRY(Var) \
IF_HAS (SEQENTRY_ON_SEQSET, Var)

/// SEQSET_HAS_SEQENTRY

#define SEQSET_HAS_SEQENTRY(Var) \
ITEM_HAS (SEQENTRY_ON_SEQSET, Var)

/// FOR_EACH_SEQENTRY_ON_SEQSET
/// EDIT_EACH_SEQENTRY_ON_SEQSET
// CBioseq_set& as input, dereference with [const] CSeq_entry& se = **itr;

#define FOR_EACH_SEQENTRY_ON_SEQSET(Itr, Var) \
FOR_EACH (SEQENTRY_ON_SEQSET, Itr, Var)

#define EDIT_EACH_SEQENTRY_ON_SEQSET(Itr, Var) \
EDIT_EACH (SEQENTRY_ON_SEQSET, Itr, Var)

/// ERASE_SEQENTRY_ON_SEQSET

#define ERASE_SEQENTRY_ON_SEQSET(Itr, Var) \
ERASE_ITEM (SEQENTRY_ON_SEQSET, Itr, Var)


/// CSeq_annot macros

/// SEQFEAT_ON_SEQANNOT macros

#define SEQFEAT_ON_SEQANNOT_Type      CSeq_annot::TData::TFtable
#define SEQFEAT_ON_SEQANNOT_Test(Var) (Var).IsFtable()
#define SEQFEAT_ON_SEQANNOT_Get(Var)  (Var).GetData().GetFtable()
#define SEQFEAT_ON_SEQANNOT_Set(Var)  (Var).SetData().SetFtable()

/// IF_SEQANNOT_IS_SEQFEAT

#define IF_SEQANNOT_IS_SEQFEAT(Var) \
IF_HAS (SEQFEAT_ON_SEQANNOT, Var)

/// SEQANNOT_IS_SEQFEAT

#define SEQANNOT_IS_SEQFEAT(Var) \
ITEM_HAS (SEQFEAT_ON_SEQANNOT, Var)

/// FOR_EACH_SEQFEAT_ON_SEQANNOT
/// EDIT_EACH_SEQFEAT_ON_SEQANNOT
// CSeq_annot& as input, dereference with [const] CSeq_feat& feat = **itr;

#define FOR_EACH_SEQFEAT_ON_SEQANNOT(Itr, Var) \
FOR_EACH (SEQFEAT_ON_SEQANNOT, Itr, Var)

#define EDIT_EACH_SEQFEAT_ON_SEQANNOT(Itr, Var) \
EDIT_EACH (SEQFEAT_ON_SEQANNOT, Itr, Var)

/// ERASE_SEQFEAT_ON_SEQANNOT

#define ERASE_SEQFEAT_ON_SEQANNOT(Itr, Var) \
ERASE_ITEM (SEQFEAT_ON_SEQANNOT, Itr, Var)

/// IF_ANNOT_IS_FEATURE
/// ANNOT_IS_FEATURE
/// FOR_EACH_FEATURE_ON_ANNOT
/// EDIT_EACH_FEATURE_ON_ANNOT
/// ERASE_FEATURE_ON_ANNOT

#define IF_ANNOT_IS_FEATURE IF_SEQANNOT_IS_SEQFEAT
#define ANNOT_IS_FEATURE SEQANNOT_IS_SEQFEAT
#define FOR_EACH_FEATURE_ON_ANNOT FOR_EACH_SEQFEAT_ON_SEQANNOT
#define EDIT_EACH_FEATURE_ON_ANNOT EDIT_EACH_SEQFEAT_ON_SEQANNOT
#define ERASE_FEATURE_ON_ANNOT ERASE_SEQFEAT_ON_SEQANNOT


/// SEQALIGN_ON_SEQANNOT macros

#define SEQALIGN_ON_SEQANNOT_Type      CSeq_annot::TData::TAlign
#define SEQALIGN_ON_SEQANNOT_Test(Var) (Var).IsAlign()
#define SEQALIGN_ON_SEQANNOT_Get(Var)  (Var).GetData().GetAlign()
#define SEQALIGN_ON_SEQANNOT_Set(Var)  (Var).SetData().SetAlign()

/// IF_SEQANNOT_IS_SEQALIGN

#define IF_SEQANNOT_IS_SEQALIGN(Var) \
IF_HAS (SEQALIGN_ON_SEQANNOT, Var)

/// SEQANNOT_IS_SEQALIGN

#define SEQANNOT_IS_SEQALIGN(Var) \
ITEM_HAS (SEQALIGN_ON_SEQANNOT, Var)

/// FOR_EACH_SEQALIGN_ON_SEQANNOT
/// EDIT_EACH_SEQALIGN_ON_SEQANNOT
// CSeq_annot& as input, dereference with [const] CSeq_align& align = **itr;

#define FOR_EACH_SEQALIGN_ON_SEQANNOT(Itr, Var) \
FOR_EACH (SEQALIGN_ON_SEQANNOT, Itr, Var)

#define EDIT_EACH_SEQALIGN_ON_SEQANNOT(Itr, Var) \
EDIT_EACH (SEQALIGN_ON_SEQANNOT, Itr, Var)

/// ERASE_SEQALIGN_ON_SEQANNOT

#define ERASE_SEQALIGN_ON_SEQANNOT(Itr, Var) \
ERASE_ITEM (SEQALIGN_ON_SEQANNOT, Itr, Var)

/// IF_ANNOT_IS_ALIGN
/// ANNOT_IS_ALIGN
/// FOR_EACH_ALIGN_ON_ANNOT
/// EDIT_EACH_ALIGN_ON_ANNOT
/// ERASE_ALIGN_ON_ANNOT

#define IF_ANNOT_IS_ALIGN IF_SEQANNOT_IS_SEQALIGN
#define ANNOT_IS_ALIGN SEQANNOT_IS_SEQALIGN
#define FOR_EACH_ALIGN_ON_ANNOT FOR_EACH_SEQALIGN_ON_SEQANNOT
#define EDIT_EACH_ALIGN_ON_ANNOT EDIT_EACH_SEQALIGN_ON_SEQANNOT
#define ERASE_ALIGN_ON_ANNOT ERASE_SEQALIGN_ON_SEQANNOT


/// SEQGRAPH_ON_SEQANNOT macros

#define SEQGRAPH_ON_SEQANNOT_Type      CSeq_annot::TData::TGraph
#define SEQGRAPH_ON_SEQANNOT_Test(Var) (Var).IsGraph()
#define SEQGRAPH_ON_SEQANNOT_Get(Var)  (Var).GetData().GetGraph()
#define SEQGRAPH_ON_SEQANNOT_Set(Var)  SetData().SetGraph()

/// IF_SEQANNOT_IS_SEQGRAPH

#define IF_SEQANNOT_IS_SEQGRAPH(Var) \
IF_HAS (SEQGRAPH_ON_SEQANNOT, Var)

/// SEQANNOT_IS_SEQGRAPH

#define SEQANNOT_IS_SEQGRAPH(Var) \
ITEM_HAS (SEQGRAPH_ON_SEQANNOT, Var)

/// FOR_EACH_SEQGRAPH_ON_SEQANNOT
/// EDIT_EACH_SEQGRAPH_ON_SEQANNOT
// CSeq_annot& as input, dereference with [const] CSeq_graph& graph = **itr;

#define FOR_EACH_SEQGRAPH_ON_SEQANNOT(Itr, Var) \
FOR_EACH (SEQGRAPH_ON_SEQANNOT, Itr, Var)

#define EDIT_EACH_SEQGRAPH_ON_SEQANNOT(Itr, Var) \
EDIT_EACH (SEQGRAPH_ON_SEQANNOT, Itr, Var)

/// ERASE_SEQGRAPH_ON_SEQANNOT

#define ERASE_SEQGRAPH_ON_SEQANNOT(Itr, Var) \
ERASE_ITEM (SEQGRAPH_ON_SEQANNOT, Itr, Var)

/// IF_ANNOT_IS_GRAPH
/// ANNOT_IS_GRAPH
/// FOR_EACH_GRAPH_ON_ANNOT
/// EDIT_EACH_GRAPH_ON_ANNOT
/// ERASE_GRAPH_ON_ANNOT

#define IF_ANNOT_IS_GRAPH IF_SEQANNOT_IS_SEQGRAPH
#define ANNOT_IS_GRAPH SEQANNOT_IS_SEQGRAPH
#define FOR_EACH_GRAPH_ON_ANNOT FOR_EACH_SEQGRAPH_ON_SEQANNOT
#define EDIT_EACH_GRAPH_ON_ANNOT EDIT_EACH_SEQGRAPH_ON_SEQANNOT
#define ERASE_GRAPH_ON_ANNOT ERASE_SEQGRAPH_ON_SEQANNOT


/// SEQTABLE_ON_SEQANNOT macros

#define SEQTABLE_ON_SEQANNOT_Type      CSeq_annot::TData::TSeq_table
#define SEQTABLE_ON_SEQANNOT_Test(Var) (Var).IsSeq_table()
#define SEQTABLE_ON_SEQANNOT_Get(Var)  (Var).GetData().GetSeq_table()
#define SEQTABLE_ON_SEQANNOT_Set(Var)  SetData().SetSeq_table()

/// IF_SEQANNOT_IS_SEQTABLE

#define IF_SEQANNOT_IS_SEQTABLE(Var) \
IF_HAS (SEQTABLE_ON_SEQANNOT, Var)

/// SEQANNOT_IS_SEQTABLE

#define SEQANNOT_IS_SEQTABLE(Var) \
ITEM_HAS (SEQTABLE_ON_SEQANNOT, Var)

/// IF_ANNOT_IS_TABLE
/// ANNOT_IS_TABLE

#define IF_ANNOT_IS_TABLE IF_SEQANNOT_IS_SEQTABLE
#define ANNOT_IS_TABLE SEQANNOT_IS_SEQTABLE


/// ANNOTDESC_ON_SEQANNOT macros

#define ANNOTDESC_ON_SEQANNOT_Type      CSeq_annot::TDesc::Tdata
#define ANNOTDESC_ON_SEQANNOT_Test(Var) (Var).IsSetDesc() && (Var).GetDesc().IsSet()
#define ANNOTDESC_ON_SEQANNOT_Get(Var)  (Var).GetDesc().Get()
#define ANNOTDESC_ON_SEQANNOT_Set(Var)  (Var).SetDesc()

/// IF_SEQANNOT_HAS_ANNOTDESC

#define IF_SEQANNOT_HAS_ANNOTDESC(Var) \
IF_HAS (ANNOTDESC_ON_SEQANNOT, Var)

/// SEQANNOT_HAS_ANNOTDESC

#define SEQANNOT_HAS_ANNOTDESC(Var) \
ITEM_HAS (ANNOTDESC_ON_SEQANNOT, Var)

/// FOR_EACH_ANNOTDESC_ON_SEQANNOT
/// EDIT_EACH_ANNOTDESC_ON_SEQANNOT
// CSeq_annot& as input, dereference with [const] CAnnotdesc& desc = **itr;

#define FOR_EACH_ANNOTDESC_ON_SEQANNOT(Itr, Var) \
FOR_EACH (ANNOTDESC_ON_SEQANNOT, Itr, Var)

#define EDIT_EACH_ANNOTDESC_ON_SEQANNOT(Itr, Var) \
EDIT_EACH (ANNOTDESC_ON_SEQANNOT, Itr, Var)

/// ERASE_ANNOTDESC_ON_SEQANNOT

#define ERASE_ANNOTDESC_ON_SEQANNOT(Itr, Var) \
ERASE_ITEM (ANNOTDESC_ON_SEQANNOT, Itr, Var)

/// IF_ANNOT_HAS_ANNOTDESC
/// ANNOT_HAS_ANNOTDESC
/// FOR_EACH_ANNOTDESC_ON_ANNOT
/// EDIT_EACH_ANNOTDESC_ON_ANNOT
/// ERASE_ANNOTDESC_ON_ANNOT

#define IF_ANNOT_HAS_ANNOTDESC IF_SEQANNOT_HAS_ANNOTDESC
#define ANNOT_HAS_ANNOTDESC SEQANNOT_HAS_ANNOTDESC
#define FOR_EACH_ANNOTDESC_ON_ANNOT FOR_EACH_ANNOTDESC_ON_SEQANNOT
#define EDIT_EACH_ANNOTDESC_ON_ANNOT EDIT_EACH_ANNOTDESC_ON_SEQANNOT
#define ERASE_ANNOTDESC_ON_ANNOT ERASE_ANNOTDESC_ON_SEQANNOT


/// CAnnotdesc macros

/// ANNOTDESC_CHOICE macros

#define ANNOTDESC_CHOICE_Test(Var) (Var).Which() != CAnnotdesc::e_not_set
#define ANNOTDESC_CHOICE_Chs(Var)  (Var).Which()

/// IF_ANNOTDESC_CHOICE_IS

#define IF_ANNOTDESC_CHOICE_IS(Var, Chs) \
IF_CHOICE (ANNOTDESC_CHOICE, Var, Chs)

/// ANNOTDESC_CHOICE_IS

#define ANNOTDESC_CHOICE_IS(Var, Chs) \
CHOICE_IS (ANNOTDESC_CHOICE, Var, Chs)

/// SWITCH_ON_ANNOTDESC_CHOICE

#define SWITCH_ON_ANNOTDESC_CHOICE(Var) \
SWITCH_ON (ANNOTDESC_CHOICE, Var)


/// CSeq_descr macros

/// SEQDESC_ON_SEQDESCR macros

#define SEQDESC_ON_SEQDESCR_Type      CSeq_descr::Tdata
#define SEQDESC_ON_SEQDESCR_Test(Var) (Var).IsSet()
#define SEQDESC_ON_SEQDESCR_Get(Var)  (Var).Get()
#define SEQDESC_ON_SEQDESCR_Set(Var)  (Var).Set()

/// IF_SEQDESCR_HAS_SEQDESC

#define IF_SEQDESCR_HAS_SEQDESC(Var) \
IF_HAS (SEQDESC_ON_SEQDESCR, Var)

/// SEQDESCR_HAS_SEQDESC

#define SEQDESCR_HAS_SEQDESC(Var) \
ITEM_HAS (SEQDESC_ON_SEQDESCR, Var)

/// FOR_EACH_SEQDESC_ON_SEQDESCR
/// EDIT_EACH_SEQDESC_ON_SEQDESCR
// CSeq_descr& as input, dereference with [const] CSeqdesc& desc = **itr;

#define FOR_EACH_SEQDESC_ON_SEQDESCR(Itr, Var) \
FOR_EACH (SEQDESC_ON_SEQDESCR, Itr, Var)

#define EDIT_EACH_SEQDESC_ON_SEQDESCR(Itr, Var) \
EDIT_EACH (SEQDESC_ON_SEQDESCR, Itr, Var)

/// ERASE_SEQDESC_ON_SEQDESCR

#define ERASE_SEQDESC_ON_SEQDESCR(Itr, Var) \
ERASE_ITEM (SEQDESC_ON_SEQDESCR, Itr, Var)

/// IF_DESCR_HAS_DESCRIPTOR
/// DESCR_HAS_DESCRIPTOR
/// FOR_EACH_DESCRIPTOR_ON_DESCR
/// EDIT_EACH_DESCRIPTOR_ON_DESCR
/// ERASE_DESCRIPTOR_ON_DESCR

#define IF_DESCR_HAS_DESCRIPTOR IF_SEQDESCR_HAS_SEQDESC
#define DESCR_HAS_DESCRIPTOR SEQDESCR_HAS_SEQDESC
#define FOR_EACH_DESCRIPTOR_ON_DESCR FOR_EACH_SEQDESC_ON_SEQDESCR
#define EDIT_EACH_DESCRIPTOR_ON_DESCR EDIT_EACH_SEQDESC_ON_SEQDESCR
#define ERASE_DESCRIPTOR_ON_DESCR ERASE_SEQDESC_ON_SEQDESCR


/// CSeqdesc macros

/// SEQDESC_CHOICE macros

#define SEQDESC_CHOICE_Test(Var) (Var).Which() != CSeqdesc::e_not_set
#define SEQDESC_CHOICE_Chs(Var)  (Var).Which()

/// IF_SEQDESC_CHOICE_IS

#define IF_SEQDESC_CHOICE_IS(Var, Chs) \
IF_CHOICE (SEQDESC_CHOICE, Var, Chs)

/// SEQDESC_CHOICE_IS

#define SEQDESC_CHOICE_IS(Var, Chs) \
CHOICE_IS (SEQDESC_CHOICE, Var, Chs)

/// SWITCH_ON_SEQDESC_CHOICE

#define SWITCH_ON_SEQDESC_CHOICE(Var) \
SWITCH_ON (SEQDESC_CHOICE, Var)

/// IF_DESCRIPTOR_CHOICE_IS
/// DESCRIPTOR_CHOICE_IS
/// SWITCH_ON_DESCRIPTOR_CHOICE

#define IF_DESCRIPTOR_CHOICE_IS IF_SEQDESC_CHOICE_IS
#define DESCRIPTOR_CHOICE_IS SEQDESC_CHOICE_IS
#define SWITCH_ON_DESCRIPTOR_CHOICE SWITCH_ON_SEQDESC_CHOICE


/// CMolInfo macros

/// MOLINFO_BIOMOL macros

#define MOLINFO_BIOMOL_Test(Var) (Var).IsSetBiomol()
#define MOLINFO_BIOMOL_Chs(Var)  (Var).GetBiomol()

/// IF_MOLINFO_BIOMOL_IS

#define IF_MOLINFO_BIOMOL_IS(Var, Chs) \
IF_CHOICE (MOLINFO_BIOMOL, Var, Chs)

/// MOLINFO_BIOMOL_IS

#define MOLINFO_BIOMOL_IS(Var, Chs) \
CHOICE_IS (MOLINFO_BIOMOL, Var, Chs)

/// SWITCH_ON_MOLINFO_BIOMOL

#define SWITCH_ON_MOLINFO_BIOMOL(Var) \
SWITCH_ON (MOLINFO_BIOMOL, Var)


/// MOLINFO_TECH macros

#define MOLINFO_TECH_Test(Var) (Var).IsSetTech()
#define MOLINFO_TECH_Chs(Var)  (Var).GetTech()

/// IF_MOLINFO_TECH_IS

#define IF_MOLINFO_TECH_IS(Var, Chs) \
IF_CHOICE (MOLINFO_TECH, Var, Chs)

/// MOLINFO_TECH_IS

#define MOLINFO_TECH_IS(Var, Chs) \
CHOICE_IS (MOLINFO_TECH, Var, Chs)

/// SWITCH_ON_MOLINFO_TECH

#define SWITCH_ON_MOLINFO_TECH(Var) \
SWITCH_ON (MOLINFO_TECH, Var)


/// MOLINFO_COMPLETENESS macros

#define MOLINFO_COMPLETENESS_Test(Var) (Var).IsSetCompleteness()
#define MOLINFO_COMPLETENESS_Chs(Var)  (Var).GetCompleteness()

/// IF_MOLINFO_COMPLETENESS_IS

#define IF_MOLINFO_COMPLETENESS_IS(Var, Chs) \
IF_CHOICE (MOLINFO_COMPLETENESS, Var, Chs)

/// MOLINFO_COMPLETENESS_IS

#define MOLINFO_COMPLETENESS_IS(Var, Chs) \
CHOICE_IS (MOLINFO_COMPLETENESS, Var, Chs)

/// SWITCH_ON_MOLINFO_COMPLETENESS

#define SWITCH_ON_MOLINFO_COMPLETENESS(Var) \
SWITCH_ON (MOLINFO_COMPLETENESS, Var)


/// CBioSource macros

/// BIOSOURCE_GENOME macros

#define BIOSOURCE_GENOME_Test(Var) (Var).IsSetGenome()
#define BIOSOURCE_GENOME_Chs(Var)  (Var).GetGenome()

/// IF_BIOSOURCE_GENOME_IS

#define IF_BIOSOURCE_GENOME_IS(Var, Chs) \
IF_CHOICE (BIOSOURCE_GENOME, Var, Chs)

/// BIOSOURCE_GENOME_IS

#define BIOSOURCE_GENOME_IS(Var, Chs) \
CHOICE_IS (BIOSOURCE_GENOME, Var, Chs)

/// SWITCH_ON_BIOSOURCE_GENOME

#define SWITCH_ON_BIOSOURCE_GENOME(Var) \
SWITCH_ON (BIOSOURCE_GENOME, Var)


/// BIOSOURCE_ORIGIN macros

#define BIOSOURCE_ORIGIN_Test(Var) (Var).IsSetOrigin()
#define BIOSOURCE_ORIGIN_Chs(Var)  (Var).GetOrigin()

/// IF_BIOSOURCE_ORIGIN_IS

#define IF_BIOSOURCE_ORIGIN_IS(Var, Chs) \
IF_CHOICE (BIOSOURCE_ORIGIN, Var, Chs)

/// BIOSOURCE_ORIGIN_IS

#define BIOSOURCE_ORIGIN_IS(Var, Chs) \
CHOICE_IS (BIOSOURCE_ORIGIN, Var, Chs)

/// SWITCH_ON_BIOSOURCE_ORIGIN

#define SWITCH_ON_BIOSOURCE_ORIGIN(Var) \
SWITCH_ON (BIOSOURCE_ORIGIN, Var)


/// ORGREF_ON_BIOSOURCE macros

#define ORGREF_ON_BIOSOURCE_Test(Var) (Var).IsSetOrg()

/// IF_BIOSOURCE_HAS_ORGREF

#define IF_BIOSOURCE_HAS_ORGREF(Var) \
IF_HAS (ORGREF_ON_BIOSOURCE, Var)

/// BIOSOURCE_HAS_ORGREF

#define BIOSOURCE_HAS_ORGREF(Var) \
ITEM_HAS (ORGREF_ON_BIOSOURCE, Var)


/// ORGNAME_ON_BIOSOURCE macros

#define ORGNAME_ON_BIOSOURCE_Test(Var) (Var).IsSetOrgname()

/// IF_BIOSOURCE_HAS_ORGNAME

#define IF_BIOSOURCE_HAS_ORGNAME(Var) \
IF_HAS (ORGNAME_ON_BIOSOURCE_Test, Var)

/// BIOSOURCE_HAS_ORGNAME

#define BIOSOURCE_HAS_ORGNAME(Var) \
ITEM_HAS (ORGNAME_ON_BIOSOURCE_Test, Var)


/// SUBSOURCE_ON_BIOSOURCE macros

#define SUBSOURCE_ON_BIOSOURCE_Type      CBioSource::TSubtype
#define SUBSOURCE_ON_BIOSOURCE_Test(Var) (Var).IsSetSubtype()
#define SUBSOURCE_ON_BIOSOURCE_Get(Var)  (Var).GetSubtype()
#define SUBSOURCE_ON_BIOSOURCE_Set(Var)  (Var).SetSubtype()

/// IF_BIOSOURCE_HAS_SUBSOURCE

#define IF_BIOSOURCE_HAS_SUBSOURCE(Var) \
IF_HAS (SUBSOURCE_ON_BIOSOURCE, Var)

/// BIOSOURCE_HAS_SUBSOURCE

#define BIOSOURCE_HAS_SUBSOURCE(Var) \
ITEM_HAS (SUBSOURCE_ON_BIOSOURCE, Var)

/// FOR_EACH_SUBSOURCE_ON_BIOSOURCE
/// EDIT_EACH_SUBSOURCE_ON_BIOSOURCE
// CBioSource& as input, dereference with [const] CSubSource& sbs = **itr

#define FOR_EACH_SUBSOURCE_ON_BIOSOURCE(Itr, Var) \
FOR_EACH (SUBSOURCE_ON_BIOSOURCE, Itr, Var)

#define EDIT_EACH_SUBSOURCE_ON_BIOSOURCE(Itr, Var) \
EDIT_EACH (SUBSOURCE_ON_BIOSOURCE, Itr, Var)

/// ERASE_SUBSOURCE_ON_BIOSOURCE

#define ERASE_SUBSOURCE_ON_BIOSOURCE(Itr, Var) \
ERASE_ITEM (SUBSOURCE_ON_BIOSOURCE, Itr, Var)


/// ORGMOD_ON_BIOSOURCE macros

#define ORGMOD_ON_BIOSOURCE_Type      COrgName::TMod
#define ORGMOD_ON_BIOSOURCE_Test(Var) (Var).IsSetOrgMod()
#define ORGMOD_ON_BIOSOURCE_Get(Var)  (Var).GetOrgname().GetMod()
#define ORGMOD_ON_BIOSOURCE_Set(Var)  (Var).SetOrgname().SetMod()

/// IF_BIOSOURCE_HAS_ORGMOD

#define IF_BIOSOURCE_HAS_ORGMOD(Var) \
IF_HAS (ORGMOD_ON_BIOSOURCE, Var)

/// BIOSOURCE_HAS_ORGMOD

#define BIOSOURCE_HAS_ORGMOD(Var) \
ITEM_HAS (ORGMOD_ON_BIOSOURCE, Var)

/// FOR_EACH_ORGMOD_ON_BIOSOURCE
/// EDIT_EACH_ORGMOD_ON_BIOSOURCE
// CBioSource& as input, dereference with [const] COrgMod& omd = **itr

#define FOR_EACH_ORGMOD_ON_BIOSOURCE(Itr, Var) \
FOR_EACH (ORGMOD_ON_BIOSOURCE, Itr, Var)

#define EDIT_EACH_ORGMOD_ON_BIOSOURCE(Itr, Var) \
EDIT_EACH (ORGMOD_ON_BIOSOURCE, Itr, Var)

/// ERASE_ORGMOD_ON_BIOSOURCE

#define ERASE_ORGMOD_ON_BIOSOURCE(Itr, Var) \
ERASE_ITEM (ORGMOD_ON_BIOSOURCE, Itr, Var)


/// COrg_ref macros

/// ORGMOD_ON_ORGREF macros

#define ORGMOD_ON_ORGREF_Type      COrgName::TMod
#define ORGMOD_ON_ORGREF_Test(Var) (Var).IsSetOrgMod()
#define ORGMOD_ON_ORGREF_Get(Var)  (Var).GetOrgname().GetMod()
#define ORGMOD_ON_ORGREF_Set(Var)  (Var).SetOrgname().SetMod()

/// IF_ORGREF_HAS_ORGMOD

#define IF_ORGREF_HAS_ORGMOD(Var) \
IF_HAS (ORGMOD_ON_ORGREF, Var)

/// ORGREF_HAS_ORGMOD

#define ORGREF_HAS_ORGMOD(Var) \
ITEM_HAS (ORGMOD_ON_ORGREF, Var)

/// FOR_EACH_ORGMOD_ON_ORGREF
/// EDIT_EACH_ORGMOD_ON_ORGREF
// COrg_ref& as input, dereference with [const] COrgMod& omd = **itr

#define FOR_EACH_ORGMOD_ON_ORGREF(Itr, Var) \
FOR_EACH (ORGMOD_ON_ORGREF, Itr, Var)

#define EDIT_EACH_ORGMOD_ON_ORGREF(Itr, Var) \
EDIT_EACH (ORGMOD_ON_ORGREF, Itr, Var)

/// ERASE_ORGMOD_ON_ORGREF

#define ERASE_ORGMOD_ON_ORGREF(Itr, Var) \
ERASE_ITEM (ORGMOD_ON_ORGREF, Itr, Var)


/// DBXREF_ON_ORGREF macros

#define DBXREF_ON_ORGREF_Type      COrg_ref::TDb
#define DBXREF_ON_ORGREF_Test(Var) (Var).IsSetDb()
#define DBXREF_ON_ORGREF_Get(Var)  (Var).GetDb()
#define DBXREF_ON_ORGREF_Set(Var)  (Var).SetDb()

/// IF_ORGREF_HAS_DBXREF

#define IF_ORGREF_HAS_DBXREF(Var) \
IF_HAS (DBXREF_ON_ORGREF, Var)

/// ORGREF_HAS_DBXREF

#define ORGREF_HAS_DBXREF(Var) \
ITEM_HAS (DBXREF_ON_ORGREF, Var)

/// FOR_EACH_DBXREF_ON_ORGREF
/// EDIT_EACH_DBXREF_ON_ORGREF
// COrg_ref& as input, dereference with [const] CDbtag& dbt = **itr

#define FOR_EACH_DBXREF_ON_ORGREF(Itr, Var) \
FOR_EACH (DBXREF_ON_ORGREF, Itr, Var)

#define EDIT_EACH_DBXREF_ON_ORGREF(Itr, Var) \
EDIT_EACH (DBXREF_ON_ORGREF, Itr, Var)

/// ERASE_DBXREF_ON_ORGREF

#define ERASE_DBXREF_ON_ORGREF(Itr, Var) \
ERASE_ITEM (DBXREF_ON_ORGREF, Itr, Var)


/// MOD_ON_ORGREF macros

#define MOD_ON_ORGREF_Type      COrg_ref::TMod
#define MOD_ON_ORGREF_Test(Var) (Var).IsSetMod()
#define MOD_ON_ORGREF_Get(Var)  (Var).GetMod()
#define MOD_ON_ORGREF_Set(Var)  (Var).SetMod()

/// IF_ORGREF_HAS_MOD

#define IF_ORGREF_HAS_MOD(Var) \
IF_HAS (MOD_ON_ORGREF, Var)

/// ORGREF_HAS_MOD

#define ORGREF_HAS_MOD(Var) \
ITEM_HAS (MOD_ON_ORGREF, Var)

/// FOR_EACH_MOD_ON_ORGREF
/// EDIT_EACH_MOD_ON_ORGREF
// COrg_ref& as input, dereference with [const] string& str = *itr

#define FOR_EACH_MOD_ON_ORGREF(Itr, Var) \
FOR_EACH (MOD_ON_ORGREF, Itr, Var)

#define EDIT_EACH_MOD_ON_ORGREF(Itr, Var) \
EDIT_EACH (MOD_ON_ORGREF, Itr, Var)

/// ERASE_MOD_ON_ORGREF

#define ERASE_MOD_ON_ORGREF(Itr, Var) \
ERASE_ITEM (MOD_ON_ORGREF, Itr, Var)


/// COrgName macros

/// ORGNAME_CHOICE macros

#define ORGNAME_CHOICE_Test(Var) (Var).IsSetName() && \
                                     (Var).GetName().Which() != COrgName::e_not_set
#define ORGNAME_CHOICE_Chs(Var)  (Var).GetName().Which()

/// IF_ORGNAME_CHOICE_IS

#define IF_ORGNAME_CHOICE_IS(Var, Chs) \
IF_CHOICE (ORGNAME_CHOICE, Var, Chs)

/// ORGNAME_CHOICE_IS

#define ORGNAME_CHOICE_IS(Var, Chs) \
CHOICE_IS (ORGNAME_CHOICE, Var, Chs)

/// SWITCH_ON_ORGNAME_CHOICE

#define SWITCH_ON_ORGNAME_CHOICE(Var) \
SWITCH_ON (ORGNAME_CHOICE, Var)


/// ORGMOD_ON_ORGNAME macros

#define ORGMOD_ON_ORGNAME_Type      COrgName::TMod
#define ORGMOD_ON_ORGNAME_Test(Var) (Var).IsSetMod()
#define ORGMOD_ON_ORGNAME_Get(Var)  (Var).GetMod()
#define ORGMOD_ON_ORGNAME_Set(Var)  (Var).SetMod()

/// IF_ORGNAME_HAS_ORGMOD

#define IF_ORGNAME_HAS_ORGMOD(Var) \
IF_HAS (ORGMOD_ON_ORGNAME, Var)

/// ORGNAME_HAS_ORGMOD

#define ORGNAME_HAS_ORGMOD(Var) \
ITEM_HAS (ORGMOD_ON_ORGNAME, Var)

/// FOR_EACH_ORGMOD_ON_ORGNAME
/// EDIT_EACH_ORGMOD_ON_ORGNAME
// COrgName& as input, dereference with [const] COrgMod& omd = **itr

#define FOR_EACH_ORGMOD_ON_ORGNAME(Itr, Var) \
FOR_EACH (ORGMOD_ON_ORGNAME, Itr, Var)

#define EDIT_EACH_ORGMOD_ON_ORGNAME(Itr, Var) \
EDIT_EACH (ORGMOD_ON_ORGNAME, Itr, Var)

/// ERASE_ORGMOD_ON_ORGNAME

#define ERASE_ORGMOD_ON_ORGNAME(Itr, Var) \
ERASE_ITEM (ORGMOD_ON_ORGNAME, Itr, Var)


/// CSubSource macros

/// SUBSOURCE_CHOICE macros

#define SUBSOURCE_CHOICE_Test(Var) (Var).IsSetSubtype()
#define SUBSOURCE_CHOICE_Chs(Var)  (Var).GetSubtype()

/// IF_SUBSOURCE_CHOICE_IS

#define IF_SUBSOURCE_CHOICE_IS(Var, Chs) \
IF_CHOICE (SUBSOURCE_CHOICE, Var, Chs)

/// SUBSOURCE_CHOICE_IS

#define SUBSOURCE_CHOICE_IS(Var, Chs) \
CHOICE_IS (SUBSOURCE_CHOICE, Var, Chs)

/// SWITCH_ON_SUBSOURCE_CHOICE

#define SWITCH_ON_SUBSOURCE_CHOICE(Var) \
SWITCH_ON (SUBSOURCE_CHOICE, Var)


/// COrgMod macros

/// ORGMOD_CHOICE macros

#define ORGMOD_CHOICE_Test(Var) (Var).IsSetSubtype()
#define ORGMOD_CHOICE_Chs(Var)  (Var).GetSubtype()

/// IF_ORGMOD_CHOICE_IS

#define IF_ORGMOD_CHOICE_IS(Var, Chs) \
IF_CHOICE (ORGMOD_CHOICE, Var, Chs)

/// ORGMOD_CHOICE_IS

#define ORGMOD_CHOICE_IS(Var, Chs) \
CHOICE_IS (ORGMOD_CHOICE, Var, Chs)

/// SWITCH_ON_ORGMOD_CHOICE

#define SWITCH_ON_ORGMOD_CHOICE(Var) \
SWITCH_ON (ORGMOD_CHOICE, Var)


/// CPubdesc macros

/// PUB_ON_PUBDESC macros

#define PUB_ON_PUBDESC_Type      CPub_equiv::Tdata
#define PUB_ON_PUBDESC_Test(Var) (Var).IsSetPub() && (Var).GetPub().IsSet()
#define PUB_ON_PUBDESC_Get(Var)  (Var).GetPub().Get()
#define PUB_ON_PUBDESC_Set(Var)  (Var).SetPub().Set()

/// IF_PUBDESC_HAS_PUB

#define IF_PUBDESC_HAS_PUB(Var) \
IF_HAS (PUB_ON_PUBDESC, Var)

/// PUBDESC_HAS_PUB

#define PUBDESC_HAS_PUB(Var) \
ITEM_HAS (PUB_ON_PUBDESC, Var)

/*
#define IF_PUBDESC_HAS_PUB(Pbd) \
if ((Pbd).IsSetPub())

#define PUBDESC_HAS_PUB(Pbd) \
((Pbd).IsSetPub())
*/

/// FOR_EACH_PUB_ON_PUBDESC
/// EDIT_EACH_PUB_ON_PUBDESC
// CPubdesc& as input, dereference with [const] CPub& pub = **itr;

#define FOR_EACH_PUB_ON_PUBDESC(Itr, Var) \
FOR_EACH (PUB_ON_PUBDESC, Itr, Var)

#define EDIT_EACH_PUB_ON_PUBDESC(Itr, Var) \
EDIT_EACH (PUB_ON_PUBDESC, Itr, Var)

/// ERASE_PUB_ON_PUBDESC

#define ERASE_PUB_ON_PUBDESC(Itr, Var) \
ERASE_ITEM (PUB_ON_PUBDESC, Itr, Var)


/// CPub macros

/// AUTHOR_ON_PUB macros

#define AUTHOR_ON_PUB_Type      CAuth_list::C_Names::TStd
#define AUTHOR_ON_PUB_Test(Var) (Var).IsSetAuthors() && \
                                    (Var).GetAuthors().IsSetNames() && \
                                    (Var).GetAuthors().GetNames().IsStd()
#define AUTHOR_ON_PUB_Get(Var)  (Var).GetAuthors().GetNames().GetStd()
#define AUTHOR_ON_PUB_Set(Var)  (Var).SetAuthors().SetNames().SetStd()

/// IF_PUB_HAS_AUTHOR

#define IF_PUB_HAS_AUTHOR(Var) \
IF_HAS (AUTHOR_ON_PUB, Var)

/// PUB_HAS_AUTHOR

#define PUB_HAS_AUTHOR(Var) \
ITEM_HAS (AUTHOR_ON_PUB, Var)

/// FOR_EACH_AUTHOR_ON_PUB
/// EDIT_EACH_AUTHOR_ON_PUB
// CPub& as input, dereference with [const] CAuthor& auth = **itr;

#define FOR_EACH_AUTHOR_ON_PUB(Itr, Var) \
FOR_EACH (AUTHOR_ON_PUB, Itr, Var)

#define EDIT_EACH_AUTHOR_ON_PUB(Itr, Var) \
EDIT_EACH (AUTHOR_ON_PUB, Itr, Var)

/// ERASE_AUTHOR_ON_PUB

#define ERASE_AUTHOR_ON_PUB(Itr, Var) \
ERASE_ITEM (AUTHOR_ON_PUB, Itr, Var)


/// CGB_block macros

/// EXTRAACCN_ON_GENBANKBLOCK macros

#define EXTRAACCN_ON_GENBANKBLOCK_Type      CGB_block::TExtra_accessions
#define EXTRAACCN_ON_GENBANKBLOCK_Test(Var) (Var).IsSetExtra_accessions()
#define EXTRAACCN_ON_GENBANKBLOCK_Get(Var)  (Var).GetExtra_accessions()
#define EXTRAACCN_ON_GENBANKBLOCK_Set(Var)  (Var).SetExtra_accessions()

/// IF_GENBANKBLOCK_HAS_EXTRAACCN

#define IF_GENBANKBLOCK_HAS_EXTRAACCN(Var) \
IF_HAS (EXTRAACCN_ON_GENBANKBLOCK, Var)

/// GENBANKBLOCK_HAS_EXTRAACCN

#define GENBANKBLOCK_HAS_EXTRAACCN(Var) \
ITEM_HAS (EXTRAACCN_ON_GENBANKBLOCK, Var)

/// FOR_EACH_EXTRAACCN_ON_GENBANKBLOCK
/// EDIT_EACH_EXTRAACCN_ON_GENBANKBLOCK
// CGB_block& as input, dereference with [const] string& str = *itr;

#define FOR_EACH_EXTRAACCN_ON_GENBANKBLOCK(Itr, Var) \
FOR_EACH (EXTRAACCN_ON_GENBANKBLOCK, Itr, Var)

#define EDIT_EACH_EXTRAACCN_ON_GENBANKBLOCK(Itr, Var) \
EDIT_EACH (EXTRAACCN_ON_GENBANKBLOCK, Itr, Var)

/// ERASE_EXTRAACCN_ON_GENBANKBLOCK

#define ERASE_EXTRAACCN_ON_GENBANKBLOCK(Itr, Var) \
ERASE_ITEM (EXTRAACCN_ON_GENBANKBLOCK, Itr, Var)


/// KEYWORD_ON_GENBANKBLOCK macros

#define KEYWORD_ON_GENBANKBLOCK_Type      CGB_block::TKeywords
#define KEYWORD_ON_GENBANKBLOCK_Test(Var) (Var).IsSetKeywords()
#define KEYWORD_ON_GENBANKBLOCK_Get(Var)  (Var).GetKeywords()
#define KEYWORD_ON_GENBANKBLOCK_Set(Var)  (Var).SetKeywords()

/// IF_GENBANKBLOCK_HAS_KEYWORD

#define IF_GENBANKBLOCK_HAS_KEYWORD(Var) \
IF_HAS (KEYWORD_ON_GENBANKBLOCK, Var)

/// GENBANKBLOCK_HAS_KEYWORD

#define GENBANKBLOCK_HAS_KEYWORD(Var) \
ITEM_HAS (KEYWORD_ON_GENBANKBLOCK, Var)

/// FOR_EACH_KEYWORD_ON_GENBANKBLOCK
/// EDIT_EACH_KEYWORD_ON_GENBANKBLOCK
// CGB_block& as input, dereference with [const] string& str = *itr;

#define FOR_EACH_KEYWORD_ON_GENBANKBLOCK(Itr, Var) \
FOR_EACH (KEYWORD_ON_GENBANKBLOCK, Itr, Var)

#define EDIT_EACH_KEYWORD_ON_GENBANKBLOCK(Itr, Var) \
EDIT_EACH (KEYWORD_ON_GENBANKBLOCK, Itr, Var)

/// ERASE_KEYWORD_ON_GENBANKBLOCK

#define ERASE_KEYWORD_ON_GENBANKBLOCK(Itr, Var) \
ERASE_ITEM (KEYWORD_ON_GENBANKBLOCK, Itr, Var)


/// CEMBL_block macros

/// EXTRAACCN_ON_EMBLBLOCK macros

#define EXTRAACCN_ON_EMBLBLOCK_Type      CEMBL_block::TExtra_acc
#define EXTRAACCN_ON_EMBLBLOCK_Test(Var) (Var).IsSetExtra_acc()
#define EXTRAACCN_ON_EMBLBLOCK_Get(Var)  (Var).GetExtra_acc()
#define EXTRAACCN_ON_EMBLBLOCK_Set(Var)  (Var).SetExtra_acc()

/// IF_EMBLBLOCK_HAS_EXTRAACCN

#define IF_EMBLBLOCK_HAS_EXTRAACCN(Var) \
IF_HAS (EXTRAACCN_ON_EMBLBLOCK, Var)

/// EMBLBLOCK_HAS_EXTRAACCN

#define EMBLBLOCK_HAS_EXTRAACCN(Var) \
ITEM_HAS (EXTRAACCN_ON_EMBLBLOCK, Var)

/// FOR_EACH_EXTRAACCN_ON_EMBLBLOCK
/// EDIT_EACH_EXTRAACCN_ON_EMBLBLOCK
// CEMBL_block& as input, dereference with [const] string& str = *itr;

#define FOR_EACH_EXTRAACCN_ON_EMBLBLOCK(Itr, Var) \
FOR_EACH (EXTRAACCN_ON_EMBLBLOCK, Itr, Var)

#define EDIT_EACH_EXTRAACCN_ON_EMBLBLOCK(Itr, Var) \
EDIT_EACH (EXTRAACCN_ON_EMBLBLOCK, Itr, Var)

/// ERASE_EXTRAACCN_ON_EMBLBLOCK

#define ERASE_EXTRAACCN_ON_EMBLBLOCK(Itr, Var) \
ERASE_ITEM (EXTRAACCN_ON_EMBLBLOCK, Itr, Var)


/// KEYWORD_ON_EMBLBLOCK macros

#define KEYWORD_ON_EMBLBLOCK_Type      CEMBL_block::TKeywords
#define KEYWORD_ON_EMBLBLOCK_Test(Var) (Var).IsSetKeywords()
#define KEYWORD_ON_EMBLBLOCK_Get(Var)  (Var).GetKeywords()
#define KEYWORD_ON_EMBLBLOCK_Set(Var)  (Var).SetKeywords()

/// IF_EMBLBLOCK_HAS_KEYWORD

#define IF_EMBLBLOCK_HAS_KEYWORD(Var) \
IF_HAS (KEYWORD_ON_EMBLBLOCK, Var)

/// EMBLBLOCK_HAS_KEYWORD

#define EMBLBLOCK_HAS_KEYWORD(Var) \
ITEM_HAS (KEYWORD_ON_EMBLBLOCK, Var)

/// FOR_EACH_KEYWORD_ON_EMBLBLOCK
/// EDIT_EACH_KEYWORD_ON_EMBLBLOCK
// CEMBL_block& as input, dereference with [const] string& str = *itr;

#define FOR_EACH_KEYWORD_ON_EMBLBLOCK(Itr, Var) \
FOR_EACH (KEYWORD_ON_EMBLBLOCK, Itr, Var)

#define EDIT_EACH_KEYWORD_ON_EMBLBLOCK(Itr, Var) \
EDIT_EACH (KEYWORD_ON_EMBLBLOCK, Itr, Var)

/// ERASE_KEYWORD_ON_EMBLBLOCK

#define ERASE_KEYWORD_ON_EMBLBLOCK(Itr, Var) \
ERASE_ITEM (KEYWORD_ON_EMBLBLOCK, Itr, Var)


/// CPDB_block macros

/// COMPOUND_ON_PDBBLOCK macros

#define COMPOUND_ON_PDBBLOCK_Type      CPDB_block::TCompound
#define COMPOUND_ON_PDBBLOCK_Test(Var) (Var).IsSetCompound()
#define COMPOUND_ON_PDBBLOCK_Get(Var)  (Var).GetCompound()
#define COMPOUND_ON_PDBBLOCK_Set(Var)  (Var).SetCompound()

/// IF_PDBBLOCK_HAS_COMPOUND

#define IF_PDBBLOCK_HAS_COMPOUND(Var) \
IF_HAS (COMPOUND_ON_PDBBLOCK, Var)

/// PDBBLOCK_HAS_COMPOUND

#define PDBBLOCK_HAS_COMPOUND(Var) \
ITEM_HAS (COMPOUND_ON_PDBBLOCK, Var)

/// FOR_EACH_COMPOUND_ON_PDBBLOCK
/// EDIT_EACH_COMPOUND_ON_PDBBLOCK
// CPDB_block& as input, dereference with [const] string& str = *itr;

#define FOR_EACH_COMPOUND_ON_PDBBLOCK(Itr, Var) \
FOR_EACH (COMPOUND_ON_PDBBLOCK, Itr, Var)

#define EDIT_EACH_COMPOUND_ON_PDBBLOCK(Itr, Var) \
EDIT_EACH (COMPOUND_ON_PDBBLOCK, Itr, Var)

/// ERASE_COMPOUND_ON_PDBBLOCK

#define ERASE_COMPOUND_ON_PDBBLOCK(Itr, Var) \
ERASE_ITEM (COMPOUND_ON_PDBBLOCK, Itr, Var)


/// SOURCE_ON_PDBBLOCK macros

#define SOURCE_ON_PDBBLOCK_Type      CPDB_block::TSource
#define SOURCE_ON_PDBBLOCK_Test(Var) (Var).IsSetSource()
#define SOURCE_ON_PDBBLOCK_Get(Var)  (Var).GetSource()
#define SOURCE_ON_PDBBLOCK_Set(Var)  (Var).SetSource()

/// IF_PDBBLOCK_HAS_SOURCE

#define IF_PDBBLOCK_HAS_SOURCE(Var) \
IF_HAS (SOURCE_ON_PDBBLOCK, Var)

/// PDBBLOCK_HAS_SOURCE

#define PDBBLOCK_HAS_SOURCE(Var) \
ITEM_HAS (SOURCE_ON_PDBBLOCK, Var)

/// FOR_EACH_SOURCE_ON_PDBBLOCK
/// EDIT_EACH_SOURCE_ON_PDBBLOCK
// CPDB_block& as input, dereference with [const] string& str = *itr;

#define FOR_EACH_SOURCE_ON_PDBBLOCK(Itr, Var) \
FOR_EACH (SOURCE_ON_PDBBLOCK, Itr, Var)

#define EDIT_EACH_SOURCE_ON_PDBBLOCK(Itr, Var) \
EDIT_EACH (SOURCE_ON_PDBBLOCK, Itr, Var)

/// ERASE_SOURCE_ON_PDBBLOCK

#define ERASE_SOURCE_ON_PDBBLOCK(Itr, Var) \
ERASE_ITEM (SOURCE_ON_PDBBLOCK, Itr, Var)


/// CSeq_feat macros

/// SEQFEAT_CHOICE macros

#define SEQFEAT_CHOICE_Test(Var) (Var).IsSetData()
#define SEQFEAT_CHOICE_Chs(Var)  (Var).GetData().Which()

/// IF_SEQFEAT_CHOICE_IS

#define IF_SEQFEAT_CHOICE_IS(Var, Chs) \
IF_CHOICE (SEQFEAT_CHOICE, Var, Chs)

/// SEQFEAT_CHOICE_IS

#define SEQFEAT_CHOICE_IS(Var, Chs) \
CHOICE_IS (SEQFEAT_CHOICE, Var, Chs)

/// SWITCH_ON_SEQFEAT_CHOICE

#define SWITCH_ON_SEQFEAT_CHOICE(Var) \
SWITCH_ON (SEQFEAT_CHOICE, Var)

/// IF_FEATURE_CHOICE_IS
/// FEATURE_CHOICE_IS
/// SWITCH_ON_FEATURE_CHOICE

#define IF_FEATURE_CHOICE_IS IF_SEQFEAT_CHOICE_IS
#define FEATURE_CHOICE_IS SEQFEAT_CHOICE_IS
#define SWITCH_ON_FEATURE_CHOICE SWITCH_ON_SEQFEAT_CHOICE


/// GBQUAL_ON_SEQFEAT macros

#define GBQUAL_ON_SEQFEAT_Type      CSeq_feat::TQual
#define GBQUAL_ON_SEQFEAT_Test(Var) (Var).IsSetQual()
#define GBQUAL_ON_SEQFEAT_Get(Var)  (Var).GetQual()
#define GBQUAL_ON_SEQFEAT_Set(Var)  (Var).SetQual()

/// IF_SEQFEAT_HAS_GBQUAL

#define IF_SEQFEAT_HAS_GBQUAL(Var) \
IF_HAS (GBQUAL_ON_SEQFEAT, Var)

/// SEQFEAT_HAS_GBQUAL

#define SEQFEAT_HAS_GBQUAL(Var) \
ITEM_HAS (GBQUAL_ON_SEQFEAT, Var)

/// FOR_EACH_GBQUAL_ON_SEQFEAT
/// EDIT_EACH_GBQUAL_ON_SEQFEAT
// CSeq_feat& as input, dereference with [const] CGb_qual& gbq = **itr;

#define FOR_EACH_GBQUAL_ON_SEQFEAT(Itr, Var) \
FOR_EACH (GBQUAL_ON_SEQFEAT, Itr, Var)

#define EDIT_EACH_GBQUAL_ON_SEQFEAT(Itr, Var) \
EDIT_EACH (GBQUAL_ON_SEQFEAT, Itr, Var)

/// ERASE_GBQUAL_ON_SEQFEAT

#define ERASE_GBQUAL_ON_SEQFEAT(Itr, Var) \
ERASE_ITEM (GBQUAL_ON_SEQFEAT, Itr, Var)

/// IF_FEATURE_HAS_GBQUAL
/// FEATURE_HAS_GBQUAL
/// FOR_EACH_GBQUAL_ON_FEATURE
/// EDIT_EACH_GBQUAL_ON_FEATURE
/// ERASE_GBQUAL_ON_FEATURE

#define IF_FEATURE_HAS_GBQUAL IF_SEQFEAT_HAS_GBQUAL
#define FEATURE_HAS_GBQUAL SEQFEAT_HAS_GBQUAL
#define FOR_EACH_GBQUAL_ON_FEATURE FOR_EACH_GBQUAL_ON_SEQFEAT
#define EDIT_EACH_GBQUAL_ON_FEATURE EDIT_EACH_GBQUAL_ON_SEQFEAT
#define ERASE_GBQUAL_ON_FEATURE ERASE_GBQUAL_ON_SEQFEAT


/// SEQFEATXREF_ON_SEQFEAT macros

#define SEQFEATXREF_ON_SEQFEAT_Type      CSeq_feat::TXref
#define SEQFEATXREF_ON_SEQFEAT_Test(Var) (Var).IsSetXref()
#define SEQFEATXREF_ON_SEQFEAT_Get(Var)  (Var).GetXref()
#define SEQFEATXREF_ON_SEQFEAT_Set(Var)  (Var).SetXref()

/// IF_SEQFEAT_HAS_SEQFEATXREF

#define IF_SEQFEAT_HAS_SEQFEATXREF(Var) \
IF_HAS (SEQFEATXREF_ON_SEQFEAT, Var)

/// SEQFEAT_HAS_SEQFEATXREF

#define SEQFEAT_HAS_SEQFEATXREF(Var) \
ITEM_HAS (SEQFEATXREF_ON_SEQFEAT, Var)

/// FOR_EACH_SEQFEATXREF_ON_SEQFEAT
/// EDIT_EACH_SEQFEATXREF_ON_SEQFEAT
// CSeq_feat& as input, dereference with [const] CSeqFeatXref& sfx = **itr;

#define FOR_EACH_SEQFEATXREF_ON_SEQFEAT(Itr, Var) \
FOR_EACH (SEQFEATXREF_ON_SEQFEAT, Itr, Var)

#define EDIT_EACH_SEQFEATXREF_ON_SEQFEAT(Itr, Var) \
EDIT_EACH (SEQFEATXREF_ON_SEQFEAT, Itr, Var)

/// ERASE_SEQFEATXREF_ON_SEQFEAT

#define ERASE_SEQFEATXREF_ON_SEQFEAT(Itr, Var) \
ERASE_ITEM (SEQFEATXREF_ON_SEQFEAT, Itr, Var)

/// IF_FEATURE_HAS_SEQFEATXREF
/// FEATURE_HAS_SEQFEATXREF
/// FOR_EACH_SEQFEATXREF_ON_FEATURE
/// EDIT_EACH_SEQFEATXREF_ON_FEATURE
/// ERASE_SEQFEATXREF_ON_FEATURE

#define IF_FEATURE_HAS_SEQFEATXREF IF_SEQFEAT_HAS_SEQFEATXREF
#define FEATURE_HAS_SEQFEATXREF SEQFEAT_HAS_SEQFEATXREF
#define FOR_EACH_SEQFEATXREF_ON_FEATURE FOR_EACH_SEQFEATXREF_ON_SEQFEAT
#define EDIT_EACH_SEQFEATXREF_ON_FEATURE EDIT_EACH_SEQFEATXREF_ON_SEQFEAT
#define ERASE_SEQFEATXREF_ON_FEATURE ERASE_SEQFEATXREF_ON_SEQFEAT


/// DBXREF_ON_SEQFEAT macros

#define DBXREF_ON_SEQFEAT_Type      CSeq_feat::TDbxref
#define DBXREF_ON_SEQFEAT_Test(Var) (Var).IsSetDbxref()
#define DBXREF_ON_SEQFEAT_Get(Var)  (Var).GetDbxref()
#define DBXREF_ON_SEQFEAT_Set(Var)  (Var).SetDbxref()

/// IF_SEQFEAT_HAS_DBXREF

#define IF_SEQFEAT_HAS_DBXREF(Var) \
IF_HAS (DBXREF_ON_SEQFEAT, Var)

/// SEQFEAT_HAS_DBXREF

#define SEQFEAT_HAS_DBXREF(Var) \
ITEM_HAS (DBXREF_ON_SEQFEAT, Var)

/// FOR_EACH_DBXREF_ON_SEQFEAT
/// EDIT_EACH_DBXREF_ON_SEQFEAT
// CSeq_feat& as input, dereference with [const] CDbtag& dbt = **itr;

#define FOR_EACH_DBXREF_ON_SEQFEAT(Itr, Var) \
FOR_EACH (DBXREF_ON_SEQFEAT, Itr, Var)

#define EDIT_EACH_DBXREF_ON_SEQFEAT(Itr, Var) \
EDIT_EACH (DBXREF_ON_SEQFEAT, Itr, Var)

/// ERASE_DBXREF_ON_SEQFEAT

#define ERASE_DBXREF_ON_SEQFEAT(Itr, Var) \
ERASE_ITEM (DBXREF_ON_SEQFEAT, Itr, Var)

/// IF_FEATURE_HAS_DBXREF
/// FEATURE_HAS_DBXREF
/// FOR_EACH_DBXREF_ON_FEATURE
/// EDIT_EACH_DBXREF_ON_FEATURE
/// ERASE_DBXREF_ON_FEATURE

#define IF_FEATURE_HAS_DBXREF IF_SEQFEAT_HAS_DBXREF
#define FEATURE_HAS_DBXREF SEQFEAT_HAS_DBXREF
#define FOR_EACH_DBXREF_ON_FEATURE FOR_EACH_DBXREF_ON_SEQFEAT
#define EDIT_EACH_DBXREF_ON_FEATURE EDIT_EACH_DBXREF_ON_SEQFEAT
#define ERASE_DBXREF_ON_FEATURE ERASE_DBXREF_ON_SEQFEAT


/// CSeqFeatData macros

/// SEQFEATDATA_CHOICE macros

#define SEQFEATDATA_CHOICE_Test(Var) (Var).Which() != CSeqFeatData::e_not_set
#define SEQFEATDATA_CHOICE_Chs(Var)  (Var).Which()

/// IF_SEQFEATDATA_CHOICE_IS

#define IF_SEQFEATDATA_CHOICE_IS(Var, Chs) \
IF_CHOICE (SEQFEATDATA_CHOICE, Var, Chs)

/// SEQFEATDATA_CHOICE_IS

#define SEQFEATDATA_CHOICE_IS(Var, Chs) \
CHOICE_IS (SEQFEATDATA_CHOICE, Var, Chs)

/// SWITCH_ON_SEQFEATDATA_CHOICE

#define SWITCH_ON_SEQFEATDATA_CHOICE(Var) \
SWITCH_ON (SEQFEATDATA_CHOICE, Var)


/// CSeqFeatXref macros

/// SEQFEATXREF_CHOICE macros

#define SEQFEATXREF_CHOICE_Test(Var) (Var).IsSetData()
#define SEQFEATXREF_CHOICE_Chs(Var)  (Var).GetData().Which()

/// IF_SEQFEATXREF_CHOICE_IS

#define IF_SEQFEATXREF_CHOICE_IS(Var, Chs) \
IF_CHOICE (SEQFEATXREF_CHOICE, Var, Chs)

/// SEQFEATXREF_CHOICE_IS

#define SEQFEATXREF_CHOICE_IS(Var, Chs) \
CHOICE_IS (SEQFEATXREF_CHOICE, Var, Chs)

/// SWITCH_ON_SEQFEATXREF_CHOICE

#define SWITCH_ON_SEQFEATXREF_CHOICE(Var) \
SWITCH_ON (SEQFEATXREF_CHOICE, Var)


/// CGene_ref macros

/// SYNONYM_ON_GENEREF macros

#define SYNONYM_ON_GENEREF_Type      CGene_ref::TSyn
#define SYNONYM_ON_GENEREF_Test(Var) (Var).IsSetSyn()
#define SYNONYM_ON_GENEREF_Get(Var)  (Var).GetSyn()
#define SYNONYM_ON_GENEREF_Set(Var)  (Var).SetSyn()

/// IF_GENEREF_HAS_SYNONYM

#define IF_GENEREF_HAS_SYNONYM(Var) \
IF_HAS (SYNONYM_ON_GENEREF, Var)

/// GENEREF_HAS_SYNONYM

#define GENEREF_HAS_SYNONYM(Var) \
ITEM_HAS (SYNONYM_ON_GENEREF, Var)

/// FOR_EACH_SYNONYM_ON_GENEREF
/// EDIT_EACH_SYNONYM_ON_GENEREF
// CGene_ref& as input, dereference with [const] string& str = *itr;

#define FOR_EACH_SYNONYM_ON_GENEREF(Itr, Var) \
FOR_EACH (SYNONYM_ON_GENEREF, Itr, Var)

#define EDIT_EACH_SYNONYM_ON_GENEREF(Itr, Var) \
EDIT_EACH (SYNONYM_ON_GENEREF, Itr, Var)

/// ERASE_SYNONYM_ON_GENEREF

#define ERASE_SYNONYM_ON_GENEREF(Itr, Var) \
ERASE_ITEM (SYNONYM_ON_GENEREF, Itr, Var)

/// IF_GENE_HAS_SYNONYM
/// GENE_HAS_SYNONYM
/// FOR_EACH_SYNONYM_ON_GENE
/// EDIT_EACH_SYNONYM_ON_GENE
/// ERASE_SYNONYM_ON_GENE

#define IF_GENE_HAS_SYNONYM IF_GENEREF_HAS_SYNONYM
#define GENE_HAS_SYNONYM GENEREF_HAS_SYNONYM
#define FOR_EACH_SYNONYM_ON_GENE FOR_EACH_SYNONYM_ON_GENEREF
#define EDIT_EACH_SYNONYM_ON_GENE EDIT_EACH_SYNONYM_ON_GENEREF
#define ERASE_SYNONYM_ON_GENE ERASE_SYNONYM_ON_GENEREF


/// DBXREF_ON_GENEREF macros

#define DBXREF_ON_GENEREF_Type      CGene_ref::TDb
#define DBXREF_ON_GENEREF_Test(Var) (Var).IsSetDb()
#define DBXREF_ON_GENEREF_Get(Var)  (Var).GetDb()
#define DBXREF_ON_GENEREF_Set(Var)  (Var).SetDb()

/// IF_GENEREF_HAS_DBXREF

#define IF_GENEREF_HAS_DBXREF(Var) \
IF_HAS (DBXREF_ON_GENEREF, Var)

/// GENEREF_HAS_DBXREF

#define GENEREF_HAS_DBXREF(Var) \
ITEM_HAS (DBXREF_ON_GENEREF, Var)

/// FOR_EACH_DBXREF_ON_GENEREF
/// EDIT_EACH_DBXREF_ON_GENEREF
// CGene_ref& as input, dereference with [const] CDbtag& dbt = *itr;

#define FOR_EACH_DBXREF_ON_GENEREF(Itr, Var) \
FOR_EACH (DBXREF_ON_GENEREF, Itr, Var)

#define EDIT_EACH_DBXREF_ON_GENEREF(Itr, Var) \
EDIT_EACH (DBXREF_ON_GENEREF, Itr, Var)

/// ERASE_DBXREF_ON_GENEREF

#define ERASE_DBXREF_ON_GENEREF(Itr, Var) \
ERASE_ITEM (DBXREF_ON_GENEREF, Itr, Var)

/// IF_GENE_HAS_DBXREF
/// GENE_HAS_DBXREF
/// FOR_EACH_DBXREF_ON_GENE
/// EDIT_EACH_DBXREF_ON_GENE
/// ERASE_DBXREF_ON_GENE

#define IF_GENE_HAS_DBXREF IF_GENEREF_HAS_DBXREF
#define GENE_HAS_DBXREF GENEREF_HAS_DBXREF
#define FOR_EACH_DBXREF_ON_GENE FOR_EACH_DBXREF_ON_GENEREF
#define EDIT_EACH_DBXREF_ON_GENE EDIT_EACH_DBXREF_ON_GENEREF
#define ERASE_DBXREF_ON_GENE ERASE_DBXREF_ON_GENEREF


/// CCdregion macros

/// CODEBREAK_ON_CDREGION macros

#define CODEBREAK_ON_CDREGION_Type      CCdregion::TCode_break
#define CODEBREAK_ON_CDREGION_Test(Var) (Var).IsSetCode_break()
#define CODEBREAK_ON_CDREGION_Get(Var)  (Var).GetCode_break()
#define CODEBREAK_ON_CDREGION_Set(Var)  (Var).SetCode_break()

/// IF_CDREGION_HAS_CODEBREAK

#define IF_CDREGION_HAS_CODEBREAK(Var) \
IF_HAS (CODEBREAK_ON_CDREGION, Var)

/// CDREGION_HAS_CODEBREAK

#define CDREGION_HAS_CODEBREAK(Var) \
ITEM_HAS (CODEBREAK_ON_CDREGION, Var)

/// FOR_EACH_CODEBREAK_ON_CDREGION
/// EDIT_EACH_CODEBREAK_ON_CDREGION
// CCdregion& as input, dereference with [const] CCode_break& cbk = **itr;

#define FOR_EACH_CODEBREAK_ON_CDREGION(Itr, Var) \
FOR_EACH (CODEBREAK_ON_CDREGION, Itr, Var)

#define EDIT_EACH_CODEBREAK_ON_CDREGION(Itr, Var) \
EDIT_EACH (CODEBREAK_ON_CDREGION, Itr, Var)

/// ERASE_CODEBREAK_ON_CDREGION

#define ERASE_CODEBREAK_ON_CDREGION(Itr, Var) \
ERASE_ITEM (CODEBREAK_ON_CDREGION, Itr, Var)


/// CProt_ref macros

/// NAME_ON_PROTREF macros

#define NAME_ON_PROTREF_Type      CProt_ref::TName
#define NAME_ON_PROTREF_Test(Var) (Var).IsSetName()
#define NAME_ON_PROTREF_Get(Var)  (Var).GetName()
#define NAME_ON_PROTREF_Set(Var)  (Var).SetName()

/// IF_PROTREF_HAS_NAME

#define IF_PROTREF_HAS_NAME(Var) \
IF_HAS (NAME_ON_PROTREF, Var)

/// PROTREF_HAS_NAME

#define PROTREF_HAS_NAME(Var) \
ITEM_HAS (NAME_ON_PROTREF, Var)

/// FOR_EACH_NAME_ON_PROTREF
/// EDIT_EACH_NAME_ON_PROTREF
// CProt_ref& as input, dereference with [const] string& str = *itr;

#define FOR_EACH_NAME_ON_PROTREF(Itr, Var) \
FOR_EACH (NAME_ON_PROTREF, Itr, Var)

#define EDIT_EACH_NAME_ON_PROTREF(Itr, Var) \
EDIT_EACH (NAME_ON_PROTREF, Itr, Var)

/// ERASE_NAME_ON_PROTREF

#define ERASE_NAME_ON_PROTREF(Itr, Var) \
ERASE_ITEM (NAME_ON_PROTREF, Itr, Var)

/// IF_PROT_HAS_NAME
/// PROT_HAS_NAME
/// FOR_EACH_NAME_ON_PROT
/// EDIT_EACH_NAME_ON_PROT
/// ERASE_NAME_ON_PROT

#define IF_PROT_HAS_NAME IF_PROTREF_HAS_NAME
#define PROT_HAS_NAME PROTREF_HAS_NAME
#define FOR_EACH_NAME_ON_PROT FOR_EACH_NAME_ON_PROTREF
#define EDIT_EACH_NAME_ON_PROT EDIT_EACH_NAME_ON_PROTREF
#define ERASE_NAME_ON_PROT ERASE_NAME_ON_PROTREF


/// ECNUMBER_ON_PROTREF macros

#define ECNUMBER_ON_PROTREF_Type      CProt_ref::TEc
#define ECNUMBER_ON_PROTREF_Test(Var) (Var).IsSetEc()
#define ECNUMBER_ON_PROTREF_Get(Var)  (Var).GetEc()
#define ECNUMBER_ON_PROTREF_Set(Var)  (Var).SetEc()

/// IF_PROTREF_HAS_ECNUMBER

#define IF_PROTREF_HAS_ECNUMBER(Var) \
IF_HAS (ECNUMBER_ON_PROTREF, Var)

/// PROTREF_HAS_ECNUMBER

#define PROTREF_HAS_ECNUMBER(Var) \
ITEM_HAS (ECNUMBER_ON_PROTREF, Var)

/// FOR_EACH_ECNUMBER_ON_PROTREF
/// EDIT_EACH_ECNUMBER_ON_PROTREF
// CProt_ref& as input, dereference with [const] string& str = *itr;

#define FOR_EACH_ECNUMBER_ON_PROTREF(Itr, Var) \
FOR_EACH (ECNUMBER_ON_PROTREF, Itr, Var)

#define EDIT_EACH_ECNUMBER_ON_PROTREF(Itr, Var) \
EDIT_EACH (ECNUMBER_ON_PROTREF, Itr, Var)

/// ERASE_ECNUMBER_ON_PROTREF

#define ERASE_ECNUMBER_ON_PROTREF(Itr, Var) \
ERASE_ITEM (ECNUMBER_ON_PROTREF, Itr, Var)

/// IF_PROT_HAS_ECNUMBER
/// PROT_HAS_ECNUMBER
/// FOR_EACH_ECNUMBER_ON_PROT
/// EDIT_EACH_ECNUMBER_ON_PROT
/// ERASE_ECNUMBER_ON_PROT

#define IF_PROT_HAS_ECNUMBER IF_PROTREF_HAS_ECNUMBER
#define PROT_HAS_ECNUMBER PROTREF_HAS_ECNUMBER
#define FOR_EACH_ECNUMBER_ON_PROT FOR_EACH_ECNUMBER_ON_PROTREF
#define EDIT_EACH_ECNUMBER_ON_PROT EDIT_EACH_ECNUMBER_ON_PROTREF
#define ERASE_ECNUMBER_ON_PROT ERASE_ECNUMBER_ON_PROTREF


/// ACTIVITY_ON_PROTREF macros

#define ACTIVITY_ON_PROTREF_Type      CProt_ref::TActivity
#define ACTIVITY_ON_PROTREF_Test(Var) (Var).IsSetActivity()
#define ACTIVITY_ON_PROTREF_Get(Var)  (Var).GetActivity()
#define ACTIVITY_ON_PROTREF_Set(Var)  (Var).SetActivity()

/// IF_PROTREF_HAS_ACTIVITY

#define IF_PROTREF_HAS_ACTIVITY(Var) \
IF_HAS (ACTIVITY_ON_PROTREF, Var)

/// PROTREF_HAS_ACTIVITY

#define PROTREF_HAS_ACTIVITY(Var) \
ITEM_HAS (ACTIVITY_ON_PROTREF, Var)

/// FOR_EACH_ACTIVITY_ON_PROTREF
/// EDIT_EACH_ACTIVITY_ON_PROTREF
// CProt_ref& as input, dereference with [const] string& str = *itr;

#define FOR_EACH_ACTIVITY_ON_PROTREF(Itr, Var) \
FOR_EACH (ACTIVITY_ON_PROTREF, Itr, Var)

#define EDIT_EACH_ACTIVITY_ON_PROTREF(Itr, Var) \
EDIT_EACH (ACTIVITY_ON_PROTREF, Itr, Var)

/// ERASE_ACTIVITY_ON_PROTREF

#define ERASE_ACTIVITY_ON_PROTREF(Itr, Var) \
ERASE_ITEM (ACTIVITY_ON_PROTREF, Itr, Var)

/// IF_PROT_HAS_ACTIVITY
/// PROT_HAS_ACTIVITY
/// FOR_EACH_ACTIVITY_ON_PROT
/// EDIT_EACH_ACTIVITY_ON_PROT
/// ERASE_ACTIVITY_ON_PROT

#define IF_PROT_HAS_ACTIVITY IF_PROTREF_HAS_ACTIVITY
#define PROT_HAS_ACTIVITY PROTREF_HAS_ACTIVITY
#define FOR_EACH_ACTIVITY_ON_PROT FOR_EACH_ACTIVITY_ON_PROTREF
#define EDIT_EACH_ACTIVITY_ON_PROT EDIT_EACH_ACTIVITY_ON_PROTREF
#define ERASE_ACTIVITY_ON_PROT ERASE_ACTIVITY_ON_PROTREF


/// DBXREF_ON_PROTREF macros

#define DBXREF_ON_PROTREF_Type      CProt_ref::TDb
#define DBXREF_ON_PROTREF_Test(Var) (Var).IsSetDb()
#define DBXREF_ON_PROTREF_Get(Var)  (Var).GetDb()
#define DBXREF_ON_PROTREF_Set(Var)  (Var).SetDb()

/// IF_PROTREF_HAS_DBXREF

#define IF_PROTREF_HAS_DBXREF(Var) \
IF_HAS (DBXREF_ON_PROTREF, Var)

/// PROTREF_HAS_DBXREF

#define PROTREF_HAS_DBXREF(Var) \
ITEM_HAS (DBXREF_ON_PROTREF, Var)

/// FOR_EACH_DBXREF_ON_PROTREF
/// EDIT_EACH_DBXREF_ON_PROTREF
// CProt_ref& as input, dereference with [const] CDbtag& dbt = *itr;

#define FOR_EACH_DBXREF_ON_PROTREF(Itr, Var) \
FOR_EACH (DBXREF_ON_PROTREF, Itr, Var)

#define EDIT_EACH_DBXREF_ON_PROTREF(Itr, Var) \
EDIT_EACH (DBXREF_ON_PROTREF, Itr, Var)

/// ERASE_DBXREF_ON_PROTREF

#define ERASE_DBXREF_ON_PROTREF(Itr, Var) \
ERASE_ITEM (DBXREF_ON_PROTREF, Itr, Var)

/// IF_PROT_HAS_DBXREF
/// PROT_HAS_DBXREF
/// FOR_EACH_DBXREF_ON_PROT
/// EDIT_EACH_DBXREF_ON_PROT
/// ERASE_DBXREF_ON_PROT

#define IF_PROT_HAS_DBXREF IF_PROTREF_HAS_DBXREF
#define PROT_HAS_DBXREF PROTREF_HAS_DBXREF
#define FOR_EACH_DBXREF_ON_PROT FOR_EACH_DBXREF_ON_PROTREF
#define EDIT_EACH_DBXREF_ON_PROT EDIT_EACH_DBXREF_ON_PROTREF
#define ERASE_DBXREF_ON_PROT ERASE_DBXREF_ON_PROTREF


/// list <string> macros

/// STRING_IN_LIST macros

#define STRING_IN_LIST_Type      list <string>
#define STRING_IN_LIST_Test(Var) (! (Var).empty())
#define STRING_IN_LIST_Get(Var)  (Var)
#define STRING_IN_LIST_Set(Var)  (Var)

/// IF_LIST_HAS_STRING

#define IF_LIST_HAS_STRING(Var) \
IF_HAS (STRING_IN_LIST, Var)

/// LIST_HAS_STRING

#define LIST_HAS_STRING(Var) \
ITEM_HAS (STRING_IN_LIST, Var)

/// FOR_EACH_STRING_IN_LIST
/// EDIT_EACH_STRING_IN_LIST
// list <string>& as input, dereference with [const] string& str = *itr;

#define FOR_EACH_STRING_IN_LIST(Itr, Var) \
FOR_EACH (STRING_IN_LIST, Itr, Var)

#define EDIT_EACH_STRING_IN_LIST(Itr, Var) \
EDIT_EACH (STRING_IN_LIST, Itr, Var)

/// ERASE_STRING_IN_LIST

#define ERASE_STRING_IN_LIST(Itr, Var) \
ERASE_ITEM (STRING_IN_LIST, Itr, Var)


/// <string> macros

/// CHAR_IN_STRING macros

#define CHAR_IN_STRING_Type      string
#define CHAR_IN_STRING_Test(Var) (! (Var).empty())
#define CHAR_IN_STRING_Get(Var)  (Var)
#define CHAR_IN_STRING_Set(Var)  (Var)

/// IF_STRING_HAS_CHAR

#define IF_STRING_HAS_CHAR(Var) \
IF_HAS (CHAR_IN_STRING, Var)

/// STRING_HAS_CHAR

#define STRING_HAS_CHAR(Var) \
ITEM_HAS (CHAR_IN_STRING, Var)

/// FOR_EACH_CHAR_IN_STRING
/// EDIT_EACH_CHAR_IN_STRING
// string& as input, dereference with [const] char& ch = *itr;

#define FOR_EACH_CHAR_IN_STRING(Itr, Var) \
FOR_EACH (CHAR_IN_STRING, Itr, Var)

#define EDIT_EACH_CHAR_IN_STRING(Itr, Var) \
EDIT_EACH (CHAR_IN_STRING, Itr, Var)

/// ERASE_CHAR_IN_STRING

#define ERASE_CHAR_IN_STRING(Itr, Var) \
ERASE_ITEM (CHAR_IN_STRING, Itr, Var)



/// @}

END_SCOPE(objects)
END_NCBI_SCOPE

#endif /* __SEQUENCE_MACROS__HPP__ */

