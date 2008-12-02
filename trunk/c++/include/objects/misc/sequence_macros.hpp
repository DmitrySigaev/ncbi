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


/// @NAME Convenience macros for NCBI object iterators
/// @{


/////////////////////////////////////////////////////////////////////////////
/// Macros and typedefs for object subtypes
/////////////////////////////////////////////////////////////////////////////


/// CSeq_id definitions

#define NCBI_SEQID(Type) CSeq_id::e_##Type
typedef CSeq_id::E_Choice TSEQID_CHOICE;

#define NCBI_ACCN(Type) CSeq_id::eAcc_##Type
typedef CSeq_id::EAccessionInfo TACCN_CHOICE;


/// CSeq_inst definitions

#define NCBI_SEQREPR(Type) CSeq_inst::eRepr_##Type
typedef CSeq_inst::TRepr TSEQ_REPR;

#define NCBI_SEQMOL(Type) CSeq_inst::eMol_##Type
typedef CSeq_inst::TMol TSEQ_MOL;

#define NCBI_SEQTOPOLOGY(Type) CSeq_inst::eTopology_##Type
typedef CSeq_inst::TTopology TSEQ_TOPOLOGY;


/// CSeqdesc definitions

#define NCBI_SEQDESC(Type) CSeqdesc::e_##Type
typedef CSeqdesc::E_Choice TSEQDESC_CHOICE;


/// CMolInfo definitions

#define NCBI_BIOMOL(Type) CMolInfo::eBiomol_##Type
typedef CMolInfo::TBiomol TMOLINFO_BIOMOL;

#define NCBI_TECH(Type) CMolInfo::eTech_##Type
typedef CMolInfo::TTech TMOLINFO_TECH;

#define NCBI_COMPLETENESS(Type) CMolInfo::eCompleteness_##Type
typedef CMolInfo::TCompleteness TMOLINFO_COMPLETENESS;


/// CBioSource definitions

#define NCBI_GENOME(Type) CBioSource::eGenome_##Type
typedef CBioSource::TGenome TBIOSOURCE_GENOME;

#define NCBI_ORIGIN(Type) CBioSource::eOrigin_##Type
typedef CBioSource::TOrigin TBIOSOURCE_ORIGIN;


/// CSubSource definitions

#define NCBI_SUBSRC(Type) CSubSource::eSubtype_##Type
typedef CSubSource::TSubtype TSUBSRC_SUBTYPE;


/// COrgMod definitions

#define NCBI_ORGMOD(Type) COrgMod::eSubtype_##Type
typedef COrgMod::TSubtype TORGMOD_SUBTYPE;


/// CPub definitions

#define NCBI_PUB(Type) CPub::e_##Type
typedef CPub::E_Choice TPUB_CHOICE;


/////////////////////////////////////////////////////////////////////////////
/// Macros for obtaining closest specific CSeqdesc applying to a CBioseq
/////////////////////////////////////////////////////////////////////////////


/// IF_EXISTS_CLOSEST_MOLINFO
// Takes const CBioseq& as input and makes reference to CConstRef<CSeqdesc>
// Dereference with const CMolInfo& molinf = (*cref).GetMolinfo();
// If Lvl is not NULL, it must be a pointer to an int

#define IF_EXISTS_CLOSEST_MOLINFO(Cref, Bsq, Lvl) \
if (CConstRef<CSeqdesc> Cref = (Bsq).GetClosestDescriptor (CSeqdesc::e_Molinfo, Lvl))

/// IF_EXISTS_CLOSEST_BIOSOURCE
// Takes const CBioseq& as input and makes reference to CConstRef<CSeqdesc>
// Dereference with const CBioSource& source = (*cref).GetSource();
// If Lvl is not NULL, it must be a pointer to an int

#define IF_EXISTS_CLOSEST_BIOSOURCE(Cref, Bsq, Lvl) \
if (CConstRef<CSeqdesc> Cref = (Bsq).GetClosestDescriptor (CSeqdesc::e_Source, Lvl))

/// IF_EXISTS_CLOSEST_TITLE
// Takes const CBioseq& as input and makes reference to CConstRef<CSeqdesc>
// Dereference with const string& title = (*cref).GetTitle();
// If Lvl is not NULL, it must be a pointer to an int

#define IF_EXISTS_CLOSEST_TITLE(Cref, Bsq, Lvl) \
if (CConstRef<CSeqdesc> Cref = (Bsq).GetClosestDescriptor (CSeqdesc::e_Title, Lvl))

/// IF_EXISTS_CLOSEST_GENBANKBLOCK
// Takes const CBioseq& as input and makes reference to CConstRef<CSeqdesc>
// Dereference with const CGB_block& gbk = (*cref).GetGenbank();
// If Lvl is not NULL, it must be a pointer to an int

#define IF_EXISTS_CLOSEST_GENBANKBLOCK(Cref, Bsq, Lvl) \
if (CConstRef<CSeqdesc> Cref = (Bsq).GetClosestDescriptor (CSeqdesc::e_Genbank, Lvl))

/// IF_EXISTS_CLOSEST_EMBLBLOCK
// Takes const CBioseq& as input and makes reference to CConstRef<CSeqdesc>
// Dereference with const CEMBL_block& ebk = (*cref).GetEmbl();
// If Lvl is not NULL, it must be a pointer to an int

#define IF_EXISTS_CLOSEST_EMBLBLOCK(Cref, Bsq, Lvl) \
if (CConstRef<CSeqdesc> Cref = (Bsq).GetClosestDescriptor (CSeqdesc::e_Embl, Lvl))

/// IF_EXISTS_CLOSEST_PDBBLOCK
// Takes const CBioseq& as input and makes reference to CConstRef<CSeqdesc>
// Dereference with const CPDB_block& pbk = (*cref).GetPdb();
// If Lvl is not NULL, it must be a pointer to an int

#define IF_EXISTS_CLOSEST_PDBBLOCK(Cref, Bsq, Lvl) \
if (CConstRef<CSeqdesc> Cref = (Bsq).GetClosestDescriptor (CSeqdesc::e_Pdb, Lvl))


/////////////////////////////////////////////////////////////////////////////
/// Macros to recursively explore within NCBI data model objects
/////////////////////////////////////////////////////////////////////////////


/// NCBI_SERIAL_TEST_EXPLORE base macro tests to see if loop should be entered
// If okay, calls CTypeConstIterator for recursive exploration

#define NCBI_SERIAL_TEST_EXPLORE(Test, Type, Var, Cont) \
if (! (Test)) {} else for (CTypeConstIterator<Type> Var (Cont); Var; ++Var)


// "VISIT_ALL_XXX_WITHIN_YYY" does a recursive exploration of NCBI objects


/// CSeq_entry explorers

/// VISIT_ALL_SEQENTRYS_WITHIN_SEQENTRY
// Takes const CSeq_entry& as input and makes iterator to const CSeq_entry&
// Dereference with const CSeq_entry& seqentry = *iter;

#define VISIT_ALL_SEQENTRYS_WITHIN_SEQENTRY(Iter, Seq) \
NCBI_SERIAL_TEST_EXPLORE((Seq).Which() != CSeq_entry::e_not_set, \
    CSeq_entry, \
    Iter, \
    (Seq))

/// VISIT_ALL_BIOSEQS_WITHIN_SEQENTRY
// Takes const CSeq_entry& as input and makes iterator to const CBioseq&
// Dereference with const CBioseq& bioseq = *iter;

#define VISIT_ALL_BIOSEQS_WITHIN_SEQENTRY(Iter, Seq) \
NCBI_SERIAL_TEST_EXPLORE((Seq).Which() != CSeq_entry::e_not_set, \
    CBioseq, \
    Iter, \
    (Seq))

/// VISIT_ALL_SEQSETS_WITHIN_SEQENTRY
// Takes const CSeq_entry& as input and makes iterator to const CBioseq_set&
// Dereference with const CBioseq_set& bss = *iter;

#define VISIT_ALL_SEQSETS_WITHIN_SEQENTRY(Iter, Seq) \
NCBI_SERIAL_TEST_EXPLORE((Seq).Which() != CSeq_entry::e_not_set, \
    CBioseq_set, \
    Iter, \
    (Seq))

/// VISIT_ALL_DESCRIPTORS_WITHIN_SEQENTRY
// Takes const CSeq_entry& as input and makes iterator to const CSeqdesc&
// Dereference with const CSeqdesc& desc = *iter;

#define VISIT_ALL_DESCRIPTORS_WITHIN_SEQENTRY(Iter, Seq) \
NCBI_SERIAL_TEST_EXPLORE((Seq).Which() != CSeq_entry::e_not_set, \
    CSeqdesc, \
    Iter, \
    (Seq))

/// VISIT_ALL_ANNOTS_WITHIN_SEQENTRY
// Takes const CSeq_entry& as input and makes iterator to const CSeq_annot&
// Dereference with const CSeq_annot& annot = *iter;

#define VISIT_ALL_ANNOTS_WITHIN_SEQENTRY(Iter, Seq) \
NCBI_SERIAL_TEST_EXPLORE((Seq).Which() != CSeq_entry::e_not_set, \
    CSeq_annot, \
    Iter, \
    (Seq))

/// VISIT_ALL_FEATURES_WITHIN_SEQENTRY
// Takes const CSeq_entry& as input and makes iterator to const CSeq_feat&
// Dereference with const CSeq_feat& feat = *iter;

#define VISIT_ALL_FEATURES_WITHIN_SEQENTRY(Iter, Seq) \
NCBI_SERIAL_TEST_EXPLORE((Seq).Which() != CSeq_entry::e_not_set, \
    CSeq_feat, \
    Iter, \
    (Seq))

/// VISIT_ALL_ALIGNS_WITHIN_SEQENTRY
// Takes const CSeq_entry& as input and makes iterator to const CSeq_feat&
// Dereference with const CSeq_align& align = *iter;

#define VISIT_ALL_ALIGNS_WITHIN_SEQENTRY(Iter, Seq) \
NCBI_SERIAL_TEST_EXPLORE((Seq).Which() != CSeq_entry::e_not_set, \
    CSeq_align, \
    Iter, \
    (Seq))

/// VISIT_ALL_GRAPHS_WITHIN_SEQENTRY
// Takes const CSeq_entry& as input and makes iterator to const CSeq_feat&
// Dereference with const CSeq_graph& graph = *iter;

#define VISIT_ALL_GRAPHS_WITHIN_SEQENTRY(Iter, Seq) \
NCBI_SERIAL_TEST_EXPLORE((Seq).Which() != CSeq_entry::e_not_set, \
    CSeq_graph, \
    Iter, \
    (Seq))


/// CBioseq_set explorers

/// VISIT_ALL_SEQENTRYS_WITHIN_SEQSET
// Takes const CBioseq_set& as input and makes iterator to const CSeq_entry&
// Dereference with const CSeq_entry& seqentry = *iter;

#define VISIT_ALL_SEQENTRYS_WITHIN_SEQSET(Iter, Bss) \
NCBI_SERIAL_TEST_EXPLORE((Bss).IsSetSeq_set(), \
    CSeq_entry, \
    Iter, \
    (Bss))

/// VISIT_ALL_BIOSEQS_WITHIN_SEQSET
// Takes const CBioseq_set& as input and makes iterator to const CBioseq&
// Dereference with const CBioseq& bioseq = *iter;

#define VISIT_ALL_BIOSEQS_WITHIN_SEQSET(Iter, Bss) \
NCBI_SERIAL_TEST_EXPLORE((Bss).IsSetSeq_set(), \
    CBioseq, \
    Iter, \
    (Bss))

/// VISIT_ALL_SEQSETS_WITHIN_SEQSET
// Takes const CBioseq_set& as input and makes iterator to const CBioseq_set&
// Dereference with const CBioseq_set& bss = *iter;

#define VISIT_ALL_SEQSETS_WITHIN_SEQSET(Iter, Bss) \
NCBI_SERIAL_TEST_EXPLORE((Bss).IsSetSeq_set(), \
    CBioseq_set, \
    Iter, \
    (Bss))

/// VISIT_ALL_DESCRIPTORS_WITHIN_SEQSET
// Takes const CBioseq_set& as input and makes iterator to const CSeqdesc&
// Dereference with const CSeqdesc& desc = *iter;

#define VISIT_ALL_DESCRIPTORS_WITHIN_SEQSET(Iter, Bss) \
NCBI_SERIAL_TEST_EXPLORE((Bss).IsSetSeq_set(), \
    CSeqdesc, \
    Iter, \
    (Bss))

/// VISIT_ALL_ANNOTS_WITHIN_SEQSET
// Takes const CBioseq_set& as input and makes iterator to const CSeq_annot&
// Dereference with const CSeq_annot& annot = *iter;

#define VISIT_ALL_ANNOTS_WITHIN_SEQSET(Iter, Bss) \
NCBI_SERIAL_TEST_EXPLORE((Bss).IsSetSeq_set(), \
    CSeq_annot, \
    Iter, \
    (Bss))

/// VISIT_ALL_FEATURES_WITHIN_SEQSET
// Takes const CBioseq_set& as input and makes iterator to const CSeq_feat&
// Dereference with const CSeq_feat& feat = *iter;

#define VISIT_ALL_FEATURES_WITHIN_SEQSET(Iter, Bss) \
NCBI_SERIAL_TEST_EXPLORE((Bss).IsSetSeq_set(), \
    CSeq_feat, \
    Iter, \
    (Bss))

/// VISIT_ALL_ALIGNS_WITHIN_SEQSET
// Takes const CBioseq_set& as input and makes iterator to const CSeq_feat&
// Dereference with const CSeq_align& align = *iter;

#define VISIT_ALL_ALIGNS_WITHIN_SEQSET(Iter, Bss) \
NCBI_SERIAL_TEST_EXPLORE((Bss).IsSetSeq_set(), \
    CSeq_align, \
    Iter, \
    (Bss))

/// VISIT_ALL_GRAPHS_WITHIN_SEQSET
// Takes const CBioseq_set& as input and makes iterator to const CSeq_feat&
// Dereference with const CSeq_graph& graph = *iter;

#define VISIT_ALL_GRAPHS_WITHIN_SEQSET(Iter, Bss) \
NCBI_SERIAL_TEST_EXPLORE((Bss).IsSetSeq_set(), \
    CSeq_graph, \
    Iter, \
    (Bss))



/////////////////////////////////////////////////////////////////////////////
/// Macros to iterate over standard template containers (non-recursive)
/////////////////////////////////////////////////////////////////////////////


/// NCBI_CS_ITERATE base macro tests to see if loop should be entered
// If okay, calls ITERATE for linear STL iteration

#define NCBI_CS_ITERATE(Test, Type, Var, Cont) \
if (! (Test)) {} else ITERATE(Type, Var, Cont)

/// NCBI_NC_ITERATE base macro tests to see if loop should be entered
// If okay, calls NON_CONST_ITERATE for linear STL iteration

#define NCBI_NC_ITERATE(Test, Type, Var, Cont) \
if (! (Test)) {} else NON_CONST_ITERATE(Type, Var, Cont)


// "FOR_EACH_XXX_ON_YYY" does a linear const traversal of STL containers
// "EDIT_EACH_XXX_ON_YYY" does a linear non-const traversal of STL containers


/// CSeq_submit iterators

/// FOR_EACH_SEQENTRY_ON_SEQSUBMIT
// Takes const CSeq_submit& as input and makes iterator to const CSeq_entry&
// Dereference with const CSeq_entry& se = **iter;

#define FOR_EACH_SEQENTRY_ON_SEQSUBMIT(Iter, Ss) \
NCBI_CS_ITERATE( \
    (Ss).IsEntrys(), \
    CSeq_submit::TData::TEntrys, \
    Iter, \
    (Ss).GetData().GetEntrys())

/// EDIT_EACH_SEQENTRY_ON_SEQSUBMIT
// Takes const CSeq_submit& as input and makes iterator to const CSeq_entry&
// Dereference with CSeq_entry& se = **iter;

#define EDIT_EACH_SEQENTRY_ON_SEQSUBMIT(Iter, Ss) \
NCBI_NC_ITERATE( \
    (Ss).IsEntrys(), \
    CSeq_submit::TData::TEntrys, \
    Iter, \
    (Ss).SetData().SetEntrys())


/// CSeq_entry iterators

/// FOR_EACH_DESCRIPTOR_ON_SEQENTRY
// Takes const CSeq_entry& as input and makes iterator to const CSeqdesc&
// Dereference with const CSeqdesc& desc = **iter

#define FOR_EACH_DESCRIPTOR_ON_SEQENTRY(Iter, Seq) \
NCBI_CS_ITERATE( \
    (Seq).IsSetDescr(), \
    CSeq_descr::Tdata, \
    Iter, \
    (Seq).GetDescr().Get())

/// EDIT_EACH_DESCRIPTOR_ON_SEQENTRY
// Takes const CSeq_entry& as input and makes iterator to const CSeqdesc&
// Dereference with CSeqdesc& desc = **iter

#define EDIT_EACH_DESCRIPTOR_ON_SEQENTRY(Iter, Seq) \
NCBI_NC_ITERATE( \
    (Seq).IsSetDescr(), \
    CSeq_descr::Tdata, \
    Iter, \
    (Seq).SetDescr().Set())


/// FOR_EACH_ANNOT_ON_SEQENTRY
// Takes const CSeq_entry& as input and makes iterator to const CSeq_annot&
// Dereference with const CSeq_annot& annot = **iter;

#define FOR_EACH_ANNOT_ON_SEQENTRY(Iter, Seq) \
NCBI_CS_ITERATE( \
    (Seq).IsSetAnnot(), \
    CSeq_entry::TAnnot, \
    Iter, \
    (Seq).GetAnnot())

/// EDIT_EACH_ANNOT_ON_SEQENTRY
// Takes const CSeq_entry& as input and makes iterator to const CSeq_annot&
// Dereference with CSeq_annot& annot = **iter;

#define EDIT_EACH_ANNOT_ON_SEQENTRY(Iter, Seq) \
NCBI_NC_ITERATE( \
    (Seq).IsSetAnnot(), \
    CSeq_entry::TAnnot, \
    Iter, \
    (Seq).SetAnnot())


/// CBioseq iterators

/// FOR_EACH_DESCRIPTOR_ON_BIOSEQ
// Takes const CBioseq& as input and makes iterator to const CSeqdesc&
// Dereference with const CSeqdesc& desc = **iter

#define FOR_EACH_DESCRIPTOR_ON_BIOSEQ(Iter, Bsq) \
NCBI_CS_ITERATE( \
    (Bsq).IsSetDescr(), \
    CBioseq::TDescr::Tdata, \
    Iter, \
    (Bsq).GetDescr().Get())

/// EDIT_EACH_DESCRIPTOR_ON_BIOSEQ
// Takes const CBioseq& as input and makes iterator to const CSeqdesc&
// Dereference with CSeqdesc& desc = **iter

#define EDIT_EACH_DESCRIPTOR_ON_BIOSEQ(Iter, Bsq) \
NCBI_NC_ITERATE( \
    (Bsq).IsSetDescr(), \
    CBioseq::TDescr::Tdata, \
    Iter, \
    (Bsq).SetDescr().Set())


/// FOR_EACH_ANNOT_ON_BIOSEQ
// Takes const CBioseq& as input and makes iterator to const CSeq_annot&
// Dereference with const CSeq_annot& annot = **iter;

#define FOR_EACH_ANNOT_ON_BIOSEQ(Iter, Bsq) \
NCBI_CS_ITERATE( \
    (Bsq).IsSetAnnot(), \
    CBioseq::TAnnot, \
    Iter, \
    (Bsq).GetAnnot())

/// EDIT_EACH_ANNOT_ON_BIOSEQ
// Takes const CBioseq& as input and makes iterator to const CSeq_annot&
// Dereference with CSeq_annot& annot = **iter;

#define EDIT_EACH_ANNOT_ON_BIOSEQ(Iter, Bsq) \
NCBI_NC_ITERATE( \
    (Bsq).IsSetAnnot(), \
    CBioseq::TAnnot, \
    Iter, \
    (Bsq).SetAnnot())


/// FOR_EACH_SEQID_ON_BIOSEQ
// Takes const CBioseq& as input and makes iterator to const CSeq_id&
// Dereference with const CSeq_id& sid = **iter;

#define FOR_EACH_SEQID_ON_BIOSEQ(Iter, Bsq) \
NCBI_CS_ITERATE( \
    (Bsq).IsSetId(), \
    CBioseq::TId, \
    Iter, \
    (Bsq).GetId())

/// EDIT_EACH_SEQID_ON_BIOSEQ
// Takes const CBioseq& as input and makes iterator to const CSeq_id&
// Dereference with CSeq_id& sid = **iter;

#define EDIT_EACH_SEQID_ON_BIOSEQ(Iter, Bsq) \
NCBI_NC_ITERATE( \
    (Bsq).IsSetId(), \
    CBioseq::TId, \
    Iter, \
    (Bsq).SetId())


/// CBioseq_set iterators

/// FOR_EACH_DESCRIPTOR_ON_SEQSET
// Takes const CBioseq_set& as input and makes iterator to const CSeqdesc&
// Dereference with const CSeqdesc& desc = **iter;

#define FOR_EACH_DESCRIPTOR_ON_SEQSET(Iter, Bss) \
NCBI_CS_ITERATE( \
    (Bss).IsSetDescr(), \
    CBioseq_set::TDescr::Tdata, \
    Iter, \
    (Bss).GetDescr().Get())

/// EDIT_EACH_DESCRIPTOR_ON_SEQSET
// Takes const CBioseq_set& as input and makes iterator to const CSeqdesc&
// Dereference with CSeqdesc& desc = **iter;

#define EDIT_EACH_DESCRIPTOR_ON_SEQSET(Iter, Bss) \
NCBI_NC_ITERATE( \
    (Bss).IsSetDescr(), \
    CBioseq_set::TDescr::Tdata, \
    Iter, \
    (Bss).SetDescr().Set())


/// FOR_EACH_ANNOT_ON_SEQSET
// Takes const CBioseq_set& as input and makes iterator to const CSeq_annot&
// Dereference with const CSeq_annot& annot = **iter;

#define FOR_EACH_ANNOT_ON_SEQSET(Iter, Bss) \
NCBI_CS_ITERATE( \
    (Bss).IsSetAnnot(), \
    CBioseq_set::TAnnot, \
    Iter, \
    (Bss).GetAnnot())

/// EDIT_EACH_ANNOT_ON_SEQSET
// Takes const CBioseq_set& as input and makes iterator to const CSeq_annot&
// Dereference with CSeq_annot& annot = **iter;

#define EDIT_EACH_ANNOT_ON_SEQSET(Iter, Bss) \
NCBI_NC_ITERATE( \
    (Bss).IsSetAnnot(), \
    CBioseq_set::TAnnot, \
    Iter, \
    (Bss).SetAnnot())


/// FOR_EACH_SEQENTRY_ON_SEQSET
// Takes const CBioseq_set& as input and makes iterator to const CSeq_entry&
// Dereference with const CSeq_entry& se = **iter;

#define FOR_EACH_SEQENTRY_ON_SEQSET(Iter, Bss) \
NCBI_CS_ITERATE( \
    (Bss).IsSetSeq_set(), \
    CBioseq_set::TSeq_set, \
    Iter, \
    (Bss).GetSeq_set())

/// EDIT_EACH_SEQENTRY_ON_SEQSET
// Takes const CBioseq_set& as input and makes iterator to const CSeq_entry&
// Dereference with CSeq_entry& se = **iter;

#define EDIT_EACH_SEQENTRY_ON_SEQSET(Iter, Bss) \
NCBI_NC_ITERATE( \
    (Bss).IsSetSeq_set(), \
    CBioseq_set::TSeq_set, \
    Iter, \
    (Bss).SetSeq_set())


/// CSeq_annot iterators

/// FOR_EACH_FEATURE_ON_ANNOT
// Takes const CSeq_annot& as input and makes iterator to const CSeq_feat&
// Dereference with const CSeq_feat& feat = **iter;

#define FOR_EACH_FEATURE_ON_ANNOT(Iter, San) \
NCBI_CS_ITERATE( \
    (San).IsFtable(), \
    CSeq_annot::TData::TFtable, \
    Iter, \
    (San).GetData().GetFtable())

/// EDIT_EACH_FEATURE_ON_ANNOT
// Takes const CSeq_annot& as input and makes iterator to const CSeq_feat&
// Dereference with CSeq_feat& feat = **iter;

#define EDIT_EACH_FEATURE_ON_ANNOT(Iter, San) \
NCBI_NC_ITERATE( \
    (San).IsFtable(), \
    CSeq_annot::TData::TFtable, \
    Iter, \
    (San).SetData().SetFtable())


/// FOR_EACH_ALIGN_ON_ANNOT
// Takes const CSeq_annot& as input and makes iterator to const CSeq_align&
// Dereference with const CSeq_align& align = **iter;

#define FOR_EACH_ALIGN_ON_ANNOT(Iter, San) \
NCBI_CS_ITERATE( \
    (San).IsAlign(), \
    CSeq_annot::TData::TAlign, \
    Iter, \
    (San).GetData().GetAlign())

/// EDIT_EACH_ALIGN_ON_ANNOT
// Takes const CSeq_annot& as input and makes iterator to const CSeq_align&
// Dereference with CSeq_align& align = **iter;

#define EDIT_EACH_ALIGN_ON_ANNOT(Iter, San) \
NCBI_NC_ITERATE( \
    (San).IsAlign(), \
    CSeq_annot::TData::TAlign, \
    Iter, \
    (San).SetData().SetAlign())


/// FOR_EACH_GRAPH_ON_ANNOT
// Takes const CSeq_annot& as input and makes iterator to const CSeq_graph&
// Dereference with const CSeq_graph& graph = **iter;

#define FOR_EACH_GRAPH_ON_ANNOT(Iter, San) \
NCBI_CS_ITERATE( \
    (San).IsGraph(), \
    CSeq_annot::TData::TGraph, \
    Iter, \
    (San).GetData().GetGraph())

/// EDIT_EACH_GRAPH_ON_ANNOT
// Takes const CSeq_annot& as input and makes iterator to const CSeq_graph&
// Dereference with CSeq_graph& graph = **iter;

#define EDIT_EACH_GRAPH_ON_ANNOT(Iter, San) \
NCBI_NC_ITERATE( \
    (San).IsGraph(), \
    CSeq_annot::TData::TGraph, \
    Iter, \
    (San).SetData().SetGraph())


/// CSeq_descr iterators

/// FOR_EACH_DESCRIPTOR_ON_DESCR
// Takes const CSeq_descr& as input and makes iterator to const CSeqdesc&
// Dereference with const CSeqdesc& desc = **iter;

#define FOR_EACH_DESCRIPTOR_ON_DESCR(Iter, Descr) \
NCBI_CS_ITERATE( \
    (Descr).IsSet(), \
    CSeq_descr::Tdata, \
    Iter, \
    (Descr).Get())

/// EDIT_EACH_DESCRIPTOR_ON_DESCR
// Takes const CSeq_descr& as input and makes iterator to const CSeqdesc&
// Dereference with CSeqdesc& desc = **iter;

#define EDIT_EACH_DESCRIPTOR_ON_DESCR(Iter, Descr) \
NCBI_NC_ITERATE( \
    (Descr).IsSet(), \
    CSeq_descr::Tdata, \
    Iter, \
    (Descr).Set())


/// CBioSource iterators

/// FOR_EACH_SUBSOURCE_ON_BIOSOURCE
// Takes const CBioSource& as input and makes iterator to const CSubSource&
// Dereference with const CSubSource& sbs = **iter

#define FOR_EACH_SUBSOURCE_ON_BIOSOURCE(Iter, Bsrc) \
NCBI_CS_ITERATE( \
    (Bsrc).IsSetSubtype(), \
    CBioSource::TSubtype, \
    Iter, \
    (Bsrc).GetSubtype())

/// EDIT_EACH_SUBSOURCE_ON_BIOSOURCE
// Takes const CBioSource& as input and makes iterator to const CSubSource&
// Dereference with CSubSource& sbs = **iter

#define EDIT_EACH_SUBSOURCE_ON_BIOSOURCE(Iter, Bsrc) \
NCBI_NC_ITERATE( \
    (Bsrc).IsSetSubtype(), \
    CBioSource::TSubtype, \
    Iter, \
    (Bsrc).SetSubtype())


/// FOR_EACH_ORGMOD_ON_BIOSOURCE
// Takes const CBioSource& as input and makes iterator to const COrgMod&
// Dereference with const COrgMod& omd = **iter

#define FOR_EACH_ORGMOD_ON_BIOSOURCE(Iter, Bsrc) \
NCBI_CS_ITERATE( \
    (Bsrc).IsSetOrgMod(), \
    COrgName::TMod, \
    Iter, \
    (Bsrc).GetOrgname().GetMod())

/// EDIT_EACH_ORGMOD_ON_BIOSOURCE
// Takes const CBioSource& as input and makes iterator to const COrgMod&
// Dereference with COrgMod& omd = **iter

#define EDIT_EACH_ORGMOD_ON_BIOSOURCE(Iter, Bsrc) \
NCBI_NC_ITERATE( \
    (Bsrc).IsSetOrgMod(), \
    COrgName::TMod, \
    Iter, \
    (Bsrc).SetOrgname().SetMod())


/// COrgRef iterators

/// FOR_EACH_ORGMOD_ON_ORGREF
// Takes const COrgRef& as input and makes iterator to const COrgMod&
// Dereference with const COrgMod& omd = **iter

#define FOR_EACH_ORGMOD_ON_ORGREF(Iter, Org) \
NCBI_CS_ITERATE( \
    (Org).IsSetOrgMod(), \
    COrgName::TMod, \
    Iter, \
    (Org).GetOrgname().GetMod())

/// EDIT_EACH_ORGMOD_ON_ORGREF
// Takes const COrgRef& as input and makes iterator to const COrgMod&
// Dereference with COrgMod& omd = **iter

#define EDIT_EACH_ORGMOD_ON_ORGREF(Iter, Org) \
NCBI_NC_ITERATE( \
    (Org).IsSetOrgMod(), \
    COrgName::TMod, \
    Iter, \
    (Org).SetOrgname().SetMod())


/// COrgName iterators

/// FOR_EACH_ORGMOD_ON_ORGNAME
// Takes const COrgName& as input and makes iterator to const COrgMod&
// Dereference with const COrgMod& omd = **iter

#define FOR_EACH_ORGMOD_ON_ORGNAME(Iter, Onm) \
NCBI_CS_ITERATE( \
    (Onm).IsSetMod(), \
    COrgName::TMod, \
    Iter, \
    (Onm).GetMod())

/// EDIT_EACH_ORGMOD_ON_ORGNAME
// Takes const COrgName& as input and makes iterator to const COrgMod&
// Dereference with COrgMod& omd = **iter

#define EDIT_EACH_ORGMOD_ON_ORGNAME(Iter, Onm) \
NCBI_NC_ITERATE( \
    (Onm).IsSetMod(), \
    COrgName::TMod, \
    Iter, \
    (Onm).SetMod())


/// CPubdesc iterators

/// FOR_EACH_PUB_ON_PUBDESC
// Takes const CPubdesc& as input and makes iterator to const CPub&
// Dereference with const CPub& pub = **iter;

#define FOR_EACH_PUB_ON_PUBDESC(Iter, Pbd) \
NCBI_CS_ITERATE( \
    (Pbd).IsSetPub() && \
        (Pbd).GetPub().IsSet(), \
    CPub_equiv::Tdata, \
    Iter, \
    (Pbd).GetPub().Get())

/// EDIT_EACH_PUB_ON_PUBDESC
// Takes const CPubdesc& as input and makes iterator to const CPub&
// Dereference with CPub& pub = **iter;

#define EDIT_EACH_PUB_ON_PUBDESC(Iter, Pbd) \
NCBI_NC_ITERATE( \
    (Pbd).IsSetPub() && \
        (Pbd).GetPub().IsSet(), \
    CPub_equiv::Tdata, \
    Iter, \
    (Pbd).SetPub().Set())


/// CPub iterators

//// FOR_EACH_AUTHOR_ON_PUB
// Takes const CPub& as input and makes iterator to const CAuthor&
// Dereference with const CAuthor& auth = **iter;

#define FOR_EACH_AUTHOR_ON_PUB(Iter, Pub) \
NCBI_CS_ITERATE( \
    (Pub).IsSetAuthors() && \
        (Pub).GetAuthors().IsSetNames() && \
        (Pub).GetAuthors().GetNames().IsStd() , \
    CAuth_list::C_Names::TStd, \
    Iter, \
    (Pub).GetAuthors().GetNames().GetStd())

//// EDIT_EACH_AUTHOR_ON_PUB
// Takes const CPub& as input and makes iterator to const CAuthor&
// Dereference with CAuthor& auth = **iter;

#define EDIT_EACH_AUTHOR_ON_PUB(Iter, Pub) \
NCBI_NC_ITERATE( \
    (Pub).IsSetAuthors() && \
        (Pub).GetAuthors().IsSetNames() && \
        (Pub).GetAuthors().GetNames().IsStd() , \
    CAuth_list::C_Names::TStd, \
    Iter, \
    (Pub).SetAuthors().SetNames().SetStd())


/// CGB_block iterators

/// FOR_EACH_EXTRAACCN_ON_GENBANKBLOCK
// Takes const CGB_block& as input and makes iterator to const string&
// Dereference with const string& str = *iter;

#define FOR_EACH_EXTRAACCN_ON_GENBANKBLOCK(Iter, Gbk) \
NCBI_CS_ITERATE( \
    (Gbk).IsSetExtra_accessions(), \
    CGB_block::TExtra_accessions, \
    Iter, \
    (Gbk).GetExtra_accessions())

/// EDIT_EACH_EXTRAACCN_ON_GENBANKBLOCK
// Takes const CGB_block& as input and makes iterator to const string&
// Dereference with string& str = *iter;

#define EDIT_EACH_EXTRAACCN_ON_GENBANKBLOCK(Iter, Gbk) \
NCBI_NC_ITERATE( \
    (Gbk).IsSetExtra_accessions(), \
    CGB_block::TExtra_accessions, \
    Iter, \
    (Gbk).SetExtra_accessions())


/// FOR_EACH_KEYWORD_ON_GENBANKBLOCK
// Takes const CGB_block& as input and makes iterator to const string&
// Dereference with const string& str = *iter;

#define FOR_EACH_KEYWORD_ON_GENBANKBLOCK(Iter, Gbk) \
NCBI_CS_ITERATE( \
    (Gbk).IsSetKeywords(), \
    CGB_block::TKeywords, \
    Iter, \
    (Gbk).GetKeywords())

/// EDIT_EACH_KEYWORD_ON_GENBANKBLOCK
// Takes const CGB_block& as input and makes iterator to const string&
// Dereference with string& str = *iter;

#define EDIT_EACH_KEYWORD_ON_GENBANKBLOCK(Iter, Gbk) \
NCBI_NC_ITERATE( \
    (Gbk).IsSetKeywords(), \
    CGB_block::TKeywords, \
    Iter, \
    (Gbk).SetKeywords())


/// CEMBL_block iterators

/// FOR_EACH_EXTRAACCN_ON_EMBLBLOCK
// Takes const CEMBL_block& as input and makes iterator to const string&
// Dereference with const string& str = *iter;

#define FOR_EACH_EXTRAACCN_ON_EMBLBLOCK(Iter, Ebk) \
NCBI_CS_ITERATE( \
    (Ebk).IsSetExtra_acc(), \
    CEMBL_block::TExtra_acc, \
    Iter, \
    (Ebk).GetExtra_acc())

/// EDIT_EACH_EXTRAACCN_ON_EMBLBLOCK
// Takes const CEMBL_block& as input and makes iterator to const string&
// Dereference with string& str = *iter;

#define EDIT_EACH_EXTRAACCN_ON_EMBLBLOCK(Iter, Ebk) \
NCBI_NC_ITERATE( \
    (Ebk).IsSetExtra_acc(), \
    CEMBL_block::TExtra_acc, \
    Iter, \
    (Ebk).SetExtra_acc())


/// FOR_EACH_KEYWORD_ON_EMBLBLOCK
// Takes const CEMBL_block& as input and makes iterator to const string&
// Dereference with const string& str = *iter;

#define FOR_EACH_KEYWORD_ON_EMBLBLOCK(Iter, Ebk) \
NCBI_CS_ITERATE( \
    (Ebk).IsSetKeywords(), \
    CEMBL_block::TKeywords, \
    Iter, \
    (Ebk).GetKeywords())

/// EDIT_EACH_KEYWORD_ON_EMBLBLOCK
// Takes const CEMBL_block& as input and makes iterator to const string&
// Dereference with string& str = *iter;

#define EDIT_EACH_KEYWORD_ON_EMBLBLOCK(Iter, Ebk) \
NCBI_NC_ITERATE( \
    (Ebk).IsSetKeywords(), \
    CEMBL_block::TKeywords, \
    Iter, \
    (Ebk).SetKeywords())


/// CPDB_block iterators

/// FOR_EACH_COMPOUND_ON_PDBBLOCK
// Takes const CPDB_block& as input and makes iterator to const string&
// Dereference with const string& str = *iter;

#define FOR_EACH_COMPOUND_ON_PDBBLOCK(Iter, Pbk) \
NCBI_CS_ITERATE( \
    (Pbk).IsSetCompound(), \
    CPDB_block::TCompound, \
    Iter, \
    (Pbk).GetCompound())

/// EDIT_EACH_COMPOUND_ON_PDBBLOCK
// Takes const CPDB_block& as input and makes iterator to const string&
// Dereference with string& str = *iter;

#define EDIT_EACH_COMPOUND_ON_PDBBLOCK(Iter, Pbk) \
NCBI_NC_ITERATE( \
    (Pbk).IsSetCompound(), \
    CPDB_block::TCompound, \
    Iter, \
    (Pbk).SetCompound())


/// FOR_EACH_SOURCE_ON_PDBBLOCK
// Takes const CPDB_block& as input and makes iterator to const string&
// Dereference with const string& str = *iter;

#define FOR_EACH_SOURCE_ON_PDBBLOCK(Iter, Pbk) \
NCBI_CS_ITERATE( \
    (Pbk).IsSetSource(), \
    CPDB_block::TSource, \
    Iter, \
    (Pbk).GetSource())

/// EDIT_EACH_SOURCE_ON_PDBBLOCK
// Takes const CPDB_block& as input and makes iterator to const string&
// Dereference with string& str = *iter;

#define EDIT_EACH_SOURCE_ON_PDBBLOCK(Iter, Pbk) \
NCBI_NC_ITERATE( \
    (Pbk).IsSetSource(), \
    CPDB_block::TSource, \
    Iter, \
    (Pbk).SetSource())


/// CSeq_feat iterators

/// FOR_EACH_GBQUAL_ON_FEATURE
// Takes const CSeq_feat& as input and makes iterator to const CGb_qual&
// Dereference with const CGb_qual& gbq = **iter;

#define FOR_EACH_GBQUAL_ON_FEATURE(Iter, Sft) \
NCBI_CS_ITERATE( \
    (Sft).IsSetQual(), \
    CSeq_feat::TQual, \
    Iter, \
    (Sft).GetQual())

/// EDIT_EACH_GBQUAL_ON_FEATURE
// Takes const CSeq_feat& as input and makes iterator to const CGb_qual&
// Dereference with CGb_qual& gbq = **iter;

#define EDIT_EACH_GBQUAL_ON_FEATURE(Iter, Sft) \
NCBI_NC_ITERATE( \
    (Sft).IsSetQual(), \
    CSeq_feat::TQual, \
    Iter, \
    (Sft).SetQual())


/// FOR_EACH_SEQFEATXREF_ON_FEATURE
// Takes const CSeq_feat& as input and makes iterator to const CSeqFeatXref&
// Dereference with const CSeqFeatXref& sfx = **iter;

#define FOR_EACH_SEQFEATXREF_ON_FEATURE(Iter, Sft) \
NCBI_CS_ITERATE( \
    (Sft).IsSetXref(), \
    CSeq_feat::TXref, \
    Iter, \
    (Sft).GetXref())

/// EDIT_EACH_SEQFEATXREF_ON_FEATURE
// Takes const CSeq_feat& as input and makes iterator to const CSeqFeatXref&
// Dereference with CSeqFeatXref& sfx = **iter;

#define EDIT_EACH_SEQFEATXREF_ON_FEATURE(Iter, Sft) \
NCBI_NC_ITERATE( \
    (Sft).IsSetXref(), \
    CSeq_feat::TXref, \
    Iter, \
    (Sft).SetXref())


/// FOR_EACH_DBXREF_ON_FEATURE
// Takes const CSeq_feat& as input and makes iterator to const CDbtag&
// Dereference with const CDbtag& dbt = **iter;

#define FOR_EACH_DBXREF_ON_FEATURE(Iter, Sft) \
NCBI_CS_ITERATE( \
    (Sft).IsSetDbxref(), \
    CSeq_feat::TDbxref, \
    Iter, \
    (Sft).GetDbxref())

/// EDIT_EACH_DBXREF_ON_FEATURE
// Takes const CSeq_feat& as input and makes iterator to const CDbtag&
// Dereference with CDbtag& dbt = **iter;

#define EDIT_EACH_DBXREF_ON_FEATURE(Iter, Sft) \
NCBI_NC_ITERATE( \
    (Sft).IsSetDbxref(), \
    CSeq_feat::TDbxref, \
    Iter, \
    (Sft).SetDbxref())


/// CGene_ref iterators

/// FOR_EACH_SYNONYM_ON_GENE
// Takes const CGene_ref& as input and makes iterator to const string&
// Dereference with const string& str = *iter;

#define FOR_EACH_SYNONYM_ON_GENE(Iter, Grf) \
NCBI_CS_ITERATE( \
    (Grf).IsSetSyn(), \
    CGene_ref::TSyn, \
    Iter, \
    (Grf).GetSyn())

/// EDIT_EACH_SYNONYM_ON_GENE
// Takes const CGene_ref& as input and makes iterator to const string&
// Dereference with string& str = *iter;

#define EDIT_EACH_SYNONYM_ON_GENE(Iter, Grf) \
NCBI_NC_ITERATE( \
    (Grf).IsSetSyn(), \
    CGene_ref::TSyn, \
    Iter, \
    (Grf).SetSyn())


/// CCdregion iterators

/// FOR_EACH_CODEBREAK_ON_CDREGION
// Takes const CCdregion& as input and makes iterator to const CCode_break&
// Dereference with const CCode_break& cbk = **iter;

#define FOR_EACH_CODEBREAK_ON_CDREGION(Iter, Cdr) \
NCBI_CS_ITERATE( \
    (Cdr).IsSetCode_break(), \
    CCdregion::TCode_break, \
    Iter, \
    (Cdr).GetCode_break())

/// EDIT_EACH_CODEBREAK_ON_CDREGION
// Takes const CCdregion& as input and makes iterator to const CCode_break&
// Dereference with CCode_break& cbk = **iter;

#define EDIT_EACH_CODEBREAK_ON_CDREGION(Iter, Cdr) \
NCBI_NC_ITERATE( \
    (Cdr).IsSetCode_break(), \
    CCdregion::TCode_break, \
    Iter, \
    (Cdr).SetCode_break())


/// CProt_ref iterators

/// FOR_EACH_NAME_ON_PROT
// Takes const CProt_ref& as input and makes iterator to const string&
// Dereference with const string& str = *iter;

#define FOR_EACH_NAME_ON_PROT(Iter, Prf) \
NCBI_CS_ITERATE( \
    (Prf).IsSetName(), \
    CProt_ref::TName, \
    Iter, \
    (Prf).GetName())

/// EDIT_EACH_NAME_ON_PROT
// Takes const CProt_ref& as input and makes iterator to const string&
// Dereference with string& str = *iter;

#define EDIT_EACH_NAME_ON_PROT(Iter, Prf) \
NCBI_NC_ITERATE( \
    (Prf).IsSetName(), \
    CProt_ref::TName, \
    Iter, \
    (Prf).SetName())


/// FOR_EACH_ECNUMBER_ON_PROT
// Takes const CProt_ref& as input and makes iterator to const string&
// Dereference with const string& str = *iter;

#define FOR_EACH_ECNUMBER_ON_PROT(Iter, Prf) \
NCBI_CS_ITERATE( \
    (Prf).IsSetEc(), \
    CProt_ref::TEc, \
    Iter, \
    (Prf).GetEc())

/// EDIT_EACH_ECNUMBER_ON_PROT
// Takes const CProt_ref& as input and makes iterator to const string&
// Dereference with string& str = *iter;

#define EDIT_EACH_ECNUMBER_ON_PROT(Iter, Prf) \
NCBI_NC_ITERATE( \
    (Prf).IsSetEc(), \
    CProt_ref::TEc, \
    Iter, \
    (Prf).SetEc())


/// FOR_EACH_ACTIVITY_ON_PROT
// Takes const CProt_ref& as input and makes iterator to const string&
// Dereference with const string& str = *iter;

#define FOR_EACH_ACTIVITY_ON_PROT(Iter, Prf) \
NCBI_CS_ITERATE( \
    (Prf).IsSetActivity(), \
    CProt_ref::TActivity, \
    Iter, \
    (Prf).GetActivity())

/// EDIT_EACH_ACTIVITY_ON_PROT
// Takes const CProt_ref& as input and makes iterator to const string&
// Dereference with string& str = *iter;

#define EDIT_EACH_ACTIVITY_ON_PROT(Iter, Prf) \
NCBI_NC_ITERATE( \
    (Prf).IsSetActivity(), \
    CProt_ref::TActivity, \
    Iter, \
    (Prf).SetActivity())


/// list <string> iterators

/// FOR_EACH_STRING_IN_LIST
// Takes const list <string>& as input and makes iterator to const string&
// Dereference with const string& str = *iter;

#define FOR_EACH_STRING_IN_LIST(Iter, Lst) \
NCBI_CS_ITERATE( \
    ! (Lst).empty(), \
    list <string>, \
    Iter, \
    (Lst))

/// EDIT_EACH_STRING_IN_LIST
// Takes const list <string>& as input and makes iterator to const string&
// Dereference with string& str = *iter;

#define EDIT_EACH_STRING_IN_LIST(Iter, Lst) \
NCBI_NC_ITERATE( \
    ! (Lst).empty(), \
    list <string>, \
    Iter, \
    (Lst))

/// @}

END_SCOPE(objects)
END_NCBI_SCOPE

#endif /* __SEQUENCE_MACROS__HPP__ */

