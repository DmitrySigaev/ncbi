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
 * Author:  Jonathan Kans, Clifford Clausen, Aaron Ucko, Mati Shomrat, ....
 *
 * File Description:
 *   Implementation of private parts of the validator
 *   .......
 *
 */
#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbistr.hpp>
#include <corelib/ncbiapp.hpp>
#include <objmgr/object_manager.hpp>

#include "validatorp.hpp"
#include "utilities.hpp"

#include <serial/iterator.hpp>
#include <serial/enumvalues.hpp>

#include <objects/general/Dbtag.hpp>
#include <objects/general/Person_id.hpp>
#include <objects/general/Name_std.hpp>

#include <objects/seqalign/Seq_align.hpp>

#include <objects/seqset/Bioseq_set.hpp>
#include <objects/seqset/Seq_entry.hpp>

#include <objects/seq/Bioseq.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <objects/seq/Seqdesc.hpp>
#include <objects/seq/Seq_descr.hpp>
#include <objects/seq/Pubdesc.hpp>
#include <objects/seq/MolInfo.hpp>
#include <objects/seqfeat/BioSource.hpp>
#include <objects/seqfeat/OrgMod.hpp>
#include <objects/seqfeat/OrgName.hpp>
#include <objects/seqfeat/Org_ref.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqfeat/SubSource.hpp>

#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seqloc/Seq_point.hpp>
#include <objects/seqloc/Textseq_id.hpp>

#include <objects/seqres/Seq_graph.hpp>

#include <objects/submit/Seq_submit.hpp>
#include <objects/submit/Submit_block.hpp>

#include <objmgr/bioseq_ci.hpp>
#include <objmgr/seqdesc_ci.hpp>
#include <objmgr/graph_ci.hpp>
#include <objmgr/seq_annot_ci.hpp>
#include <objmgr/util/feature.hpp>
#include <objmgr/util/sequence.hpp>

#include <objmgr/feat_ci.hpp>
#include <objmgr/align_ci.hpp>
#include <objmgr/seq_vector.hpp>
#include <objmgr/scope.hpp>

#include <objects/pub/Pub.hpp>
#include <objects/pub/Pub_equiv.hpp>

#include <objects/biblio/Author.hpp>
#include <objects/biblio/Auth_list.hpp>
#include <objects/biblio/Cit_art.hpp>
#include <objects/biblio/Cit_book.hpp>
#include <objects/biblio/Cit_gen.hpp>
#include <objects/biblio/Cit_jour.hpp>
#include <objects/biblio/Cit_let.hpp>
#include <objects/biblio/Cit_proc.hpp>
#include <objects/biblio/Cit_sub.hpp>
#include <objects/biblio/PubMedId.hpp>
#include <objects/biblio/PubStatus.hpp>
#include <objects/biblio/Title.hpp>
#include <objects/biblio/Imprint.hpp>
#include <objects/biblio/Affil.hpp>
#include <objects/misc/sequence_macros.hpp>
#include <objects/taxon3/taxon3.hpp>
#include <objects/taxon3/Taxon3_reply.hpp>

#include <objects/valid/Comment_set.hpp>
#include <objects/valid/Comment_rule.hpp>
#include <objects/valid/Field_set.hpp>
#include <objects/valid/Field_rule.hpp>
#include <objects/valid/Dependent_field_set.hpp>
#include <objects/valid/Dependent_field_rule.hpp>

#include <objtools/error_codes.hpp>
#include <util/sgml_entity.hpp>
#include <util/line_reader.hpp>

#include <algorithm>


#include <serial/iterator.hpp>

#define NCBI_USE_ERRCODE_X   Objtools_Validator

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
BEGIN_SCOPE(validator)
using namespace sequence;


// =============================================================================
//                            CValidError_imp Public
// =============================================================================

// Constructor
CValidError_imp::CValidError_imp
(CObjectManager& objmgr, 
 CValidError*       errs,
 Uint4              options) :
      m_ObjMgr(&objmgr),
      m_ErrRepository(errs)
{
      m_Scope = 0;
      m_TSE = 0;
      m_NonASCII = (options & CValidator::eVal_non_ascii) != 0;
      m_SuppressContext = (options & CValidator::eVal_no_context) != 0;
      m_ValidateAlignments = (options & CValidator::eVal_val_align) != 0;
      m_ValidateExons = (options & CValidator::eVal_val_exons) != 0;
      m_SpliceErr = (options & CValidator::eVal_splice_err) != 0;
      m_OvlPepErr = (options & CValidator::eVal_ovl_pep_err) != 0;
      m_RequireTaxonID = (options & CValidator::eVal_need_taxid) != 0;
      m_RequireISOJTA = (options & CValidator::eVal_need_isojta) != 0;
      m_ValidateIdSet = (options & CValidator::eVal_validate_id_set) != 0;
      m_RemoteFetch = (options & CValidator::eVal_remote_fetch) != 0;
      m_FarFetchMRNAproducts = (options & CValidator::eVal_far_fetch_mrna_products) != 0;
      m_FarFetchCDSproducts = (options & CValidator::eVal_far_fetch_cds_products) != 0;
      m_LocusTagGeneralMatch = (options & CValidator::eVal_locus_tag_general_match) != 0;
      m_DoRubiscoText = (options & CValidator::eVal_do_rubisco_test) != 0;
      m_IndexerVersion = (options & CValidator::eVal_indexer_version) != 0;
	  m_UseEntrez = (options & CValidator::eVal_use_entrez) != 0;
      m_IsStandaloneAnnot = false;
      m_NoPubs = false;
      m_NoBioSource = false;
      m_IsGPS = false;
      m_IsGED = false;
      m_IsPDB = false;
      m_IsPatent = false;
      m_IsRefSeq = false;
      m_IsEmbl = false;
      m_IsDdbj = false;
      m_IsNC = false;
      m_IsNG = false;
      m_IsNM = false;
      m_IsNP = false;
      m_IsNR = false;
      m_IsNS  = false;
      m_IsNT = false;
      m_IsNW = false;
      m_IsXR = false;
      m_IsGI = false;
      m_IsGB = false;
      m_FeatLocHasGI = false;
      m_ProductLocHasGI = false;
      m_GeneHasLocusTag = false;
      m_ProteinHasGeneralID = false;
      m_IsINSDInSep = false;
      m_PrgCallback = 0;
      m_NumAlign = 0;
      m_NumAnnot = 0;
      m_NumBioseq = 0;
      m_NumBioseq_set = 0;
      m_NumDesc = 0;
      m_NumDescr = 0;
      m_NumFeat = 0;
      m_NumGraph = 0;
      m_NumMisplacedFeatures = 0;
      m_NumMisplacedGraphs = 0;
      m_NumGenes = 0;
      m_NumGeneXrefs = 0;
      m_NumTpaWithHistory = 0;
      m_NumTpaWithoutHistory = 0;
      m_IsTbl2Asn = false;

    if ( m_SourceQualTags.get() == 0 ) {
        InitializeSourceQualTags();
    }

}


// Destructor
CValidError_imp::~CValidError_imp()
{
}


// Error post methods
void CValidError_imp::PostErr
(EDiagSev sv,
 EErrType et,
 const string&  msg,
 const CSerialObject& obj)
{
    const CSeqdesc* desc = dynamic_cast < const CSeqdesc* > (&obj);
    if (desc != 0) {
        LOG_POST_X(1, Warning << "Seqdesc validation error using default context.");
        PostErr (sv, et, msg, GetTSE(), *desc);
        return;
    }
    const CSeq_feat* feat = dynamic_cast < const CSeq_feat* > (&obj);
    if (feat != 0) {
        PostErr (sv, et, msg, *feat);
        return;
    }
    const CBioseq* seq = dynamic_cast < const CBioseq* > (&obj);
    if (seq != 0) {
        PostErr (sv, et, msg, *seq);
        return;
    }
    const CBioseq_set* set = dynamic_cast < const CBioseq_set* > (&obj);
    if (set != 0) {
        PostErr (sv, et, msg, *set);
        return;
    }
    const CSeq_annot* annot = dynamic_cast < const CSeq_annot* > (&obj);
    if (annot != 0) {
        PostErr (sv, et, msg, *annot);
        return;
    }
    const CSeq_graph* graph = dynamic_cast < const CSeq_graph* > (&obj);
    if (graph != 0) {
        PostErr (sv, et, msg, *graph);
        return;
    }
    const CSeq_align* align = dynamic_cast < const CSeq_align* > (&obj);
    if (align != 0) {
        PostErr (sv, et, msg, *align);
        return;
    }
    const CSeq_entry* entry = dynamic_cast < const CSeq_entry* > (&obj);
    if (entry != 0) {
        PostErr (sv, et, msg, *entry);
        return;
    }
    const CBioSource* src = dynamic_cast < const CBioSource* > (&obj);
    if (src != 0) {
        PostErr (sv, et, msg, *src);
        return;
    }
    const COrg_ref* org = dynamic_cast < const COrg_ref* > (&obj);
    if (org != 0) {
        PostErr (sv, et, msg, *org);
        return;
    }
    const CPubdesc* pd = dynamic_cast < const CPubdesc* > (&obj);
    if (pd != 0) {
        PostErr (sv, et, msg, *pd);
        return;
    }
    const CSeq_submit* ss = dynamic_cast < const CSeq_submit* > (&obj);
    if (ss != 0) {
        PostErr (sv, et, msg, *ss);
        return;
    }
}


/*
void CValidError_imp::PostErr
(EDiagSev       sv,
 EErrType       et,
 const string&  msg,
 TDesc          ds)
{
    // Append Descriptor label
    string desc = "DESCRIPTOR: ";
    ds.GetLabel (&desc, CSeqdesc::eBoth);
    desc += ", NO Descriptor Context";
    m_ErrRepository->AddValidErrItem(sv, et, msg, desc, ds, *m_Scope);
}
*/


void CValidError_imp::PostErr
(EDiagSev       sv,
 EErrType       et,
 const string&  msg,
 TFeat          ft)
{
    // Add feature part of label
    string desc = "FEATURE: ";
    feature::GetLabel(ft, &desc, feature::eBoth, m_Scope);

    // Add feature location part of label
    string loc_label;
    if (m_SuppressContext) {
        CSeq_loc loc;
        loc.Assign(ft.GetLocation());
        ChangeSeqLocId(&loc, false, m_Scope.GetPointer());
        loc.GetLabel(&loc_label);
    } else {
        ft.GetLocation().GetLabel(&loc_label);
    }
    if (loc_label.size() > 800) {
        loc_label.replace(797, NPOS, "...");
    }
    if (!loc_label.empty()) {
        desc += "[";
        desc += loc_label;
        desc += "]";
    }

    // Append label for bioseq of feature location
    if (!m_SuppressContext) {
        try {
            CBioseq_Handle hnd = m_Scope->GetBioseqHandle(ft.GetLocation());
            if( hnd ) {
                CBioseq_Handle::TBioseqCore bc = hnd.GetBioseqCore();
                desc += "[";
                bc->GetLabel(&desc, CBioseq::eBoth);
                desc += "]";
            }
		} catch (CException ) {
		} catch (std::exception ) {
        };
    }

    // Append label for product of feature
    if (ft.IsSetProduct()) {
        loc_label.erase();
        if (m_SuppressContext) {
            CSeq_loc loc;
            loc.Assign(ft.GetProduct());
            ChangeSeqLocId(&loc, false, m_Scope.GetPointer());
            loc.GetLabel(&loc_label);
        } else {
            ft.GetProduct().GetLabel(&loc_label);
        }
        if (loc_label.size() > 800) {
            loc_label.replace(797, NPOS, "...");
        }
        if (!loc_label.empty()) {
            desc += "[";
            desc += loc_label;
            desc += "]";
        }
    }
    m_ErrRepository->AddValidErrItem(sv, et, msg, desc, ft, GetAccessionFromObjects(&ft, NULL, *m_Scope));
}


static void s_AppendBioseqLabel(string& str, CValidError_imp::TBioseq sq, bool supress_context)
{
    str += "BIOSEQ: ";
    sq.GetLabel(&str, CBioseq::eContent, supress_context);
}

void CValidError_imp::PostErr
(EDiagSev       sv,
 EErrType       et,
 const string&  msg,
 TBioseq        sq)
{
    // Append bioseq label
    string desc;
    s_AppendBioseqLabel(desc, sq, m_SuppressContext);
    m_ErrRepository->AddValidErrItem(sv, et, msg, desc, sq, GetAccessionFromObjects(&sq, NULL, *m_Scope));
}


static void s_AppendSetLabel(string& str, CValidError_imp::TSet st, bool supress_context)
{
    str += "BIOSEQ-SET: ";

    // GetLabel for CBioseq_set does not follow C Toolkit conventions
    // AND is a horrible performance hit for sets with lots of sequences

    if (!supress_context && st.IsSetClass()) {
        const CEnumeratedTypeValues* tv =
            CBioseq_set::GetTypeInfo_enum_EClass();
        const string& cn = tv->FindName(st.GetClass(), true);
        str += cn;
        str += ": ";
    }

    const CBioseq* best = 0;
    CTypeConstIterator<CBioseq> si(ConstBegin(st));
    if (si) {
        best = &(*si);
    }
    // Add content to label.
    if (!best) {
        str += "(No Bioseqs)";
    } else {
        CNcbiOstrstream os;
        if (best->GetFirstId()) {
            os << best->GetFirstId()->DumpAsFasta();
            str += CNcbiOstrstreamToString(os);
        }
    }
}

void CValidError_imp::PostErr
(EDiagSev      sv,
 EErrType      et,
 const string& msg,
 TSet          st)
{
    // Append Bioseq_set label
    string desc = "BIOSEQ-SET: ";
    s_AppendSetLabel(desc, st, m_SuppressContext);
    m_ErrRepository->AddValidErrItem(sv, et, msg, desc, st, GetAccessionFromObjects(&st, NULL, *m_Scope));
}


void CValidError_imp::PostErr
(EDiagSev       sv,
 EErrType       et,
 const string&  msg,
 TEntry         ctx,
 TDesc          ds)
{
    // Append Descriptor label
    string desc("DESCRIPTOR: ");
    ds.GetLabel(&desc, CSeqdesc::eBoth);

    desc += " ";
    if (ctx.IsSeq()) {
        s_AppendBioseqLabel(desc, ctx.GetSeq(), m_SuppressContext);
    } else {
        s_AppendSetLabel(desc, ctx.GetSet(), m_SuppressContext);
    }
    m_ErrRepository->AddValidErrItem(sv, et, msg, desc, ds, ctx, GetAccessionFromObjects(&ds, &ctx, *m_Scope));
}


//void CValidError_imp::PostErr
//(EDiagSev       sv,
// EErrType       et,
// const string&  msg,
// TBioseq        sq,
// TDesc          ds)
//{
//    // Append Descriptor label
//    string desc("DESCRIPTOR: ");
//    ds.GetLabel(&desc, CSeqdesc::eBoth);
//    
//    s_AppendBioseqLabel(desc, sq, m_SuppressContext);
//    m_ErrRepository->AddValidErrItem(sv, et, msg, desc, ds, *m_Scope);
//    //PostErr(sv, et, msg, sq);
//}


//void CValidError_imp::PostErr
//(EDiagSev        sv,
// EErrType        et,
// const string&   msg,
// TSet            st,
// TDesc           ds)
//{
//    // Append Descriptor label
//    string desc =  " DESCRIPTOR: ";
//    ds.GetLabel(&desc, CSeqdesc::eBoth);
//    s_AppendSetLabel(desc, st, m_SuppressContext);
//    m_ErrRepository->AddValidErrItem(sv, et, msg, desc, st, *m_Scope);
//
//}


void CValidError_imp::PostErr
(EDiagSev       sv,
 EErrType       et,
 const string&  msg,
 TAnnot         an)
{
    // Append Annotation label
    string desc = "ANNOTATION: ";

    // !!! need to decide on the message

    m_ErrRepository->AddValidErrItem(sv, et, msg, desc, an, GetAccessionFromObjects(&an, NULL, *m_Scope));
}


void CValidError_imp::PostErr
(EDiagSev       sv,
 EErrType       et,
 const string&  msg,
 TGraph         graph)
{
    // Append Graph label
    string desc = "GRAPH: ";
    if (graph.IsSetTitle()) {
        desc += graph.GetTitle();
    } else {
        desc += "<Unnamed>";
    }
    desc += " ";
    graph.GetLoc().GetLabel(&desc);

    m_ErrRepository->AddValidErrItem(sv, et, msg, desc, graph, GetAccessionFromObjects(&graph, NULL, *m_Scope));
}


void CValidError_imp::PostErr
(EDiagSev       sv,
 EErrType       et,
 const string&  msg,
 TBioseq        sq,
 TGraph         graph)
{
    // Append Graph label
    string desc("GRAPH: ");
    if ( graph.IsSetTitle() ) {
        desc += graph.GetTitle();
    } else {
        desc += "<Unnamed>";
    }
    desc += " ";
    graph.GetLoc().GetLabel(&desc);
    s_AppendBioseqLabel(desc, sq, m_SuppressContext);
    m_ErrRepository->AddValidErrItem(sv, et, msg, desc, graph, GetAccessionFromObjects(&graph, NULL, *m_Scope));
}


void CValidError_imp::PostErr
(EDiagSev      sv,
 EErrType      et,
 const string& msg,
 TAlign        align)
{
    // Append Alignment label
    string desc = "ALIGNMENT: ";
    desc += align.ENUM_METHOD_NAME(EType)()->FindName(align.GetType(), true);
    try {
        CSeq_align::TDim dim = align.GetDim();
        desc += ", dim=" + NStr::IntToString(dim);
    } catch ( const CUnassignedMember ) {
        desc += ", dim=UNASSIGNED";
    }

    desc += " SEGS: ";
    desc += align.GetSegs().SelectionName(align.GetSegs().Which());

    m_ErrRepository->AddValidErrItem(sv, et, msg, desc, align, GetAccessionFromObjects(&align, NULL, *m_Scope));
}


void CValidError_imp::PostErr
(EDiagSev      sv,
 EErrType      et,
 const string& msg,
 TEntry        entry)
{
    string desc = "SEQ-ENTRY: ";
    entry.GetLabel(&desc, CSeq_entry::eContent);

    m_ErrRepository->AddValidErrItem(sv, et, msg, desc, entry, GetAccessionFromObjects(&entry, NULL, *m_Scope));
}


void CValidError_imp::PostErr
(EDiagSev      sv,
 EErrType      et,
 const string& msg,
 const CBioSource& src)
{
    string desc = "BioSource: ";
    
    m_ErrRepository->AddValidErrItem(sv, et, msg, desc, src, "");
}


void CValidError_imp::PostErr
(EDiagSev      sv,
 EErrType      et,
 const string& msg,
 const COrg_ref& org)
{
    string desc = "Org-ref: ";
    
    m_ErrRepository->AddValidErrItem(sv, et, msg, desc, org, "");
}


void CValidError_imp::PostErr
(EDiagSev      sv,
 EErrType      et,
 const string& msg,
 const CPubdesc& pd)
{
    string desc = "Pubdesc: ";
    
    m_ErrRepository->AddValidErrItem(sv, et, msg, desc, pd, "");
}


void CValidError_imp::PostErr
(EDiagSev      sv,
 EErrType      et,
 const string& msg,
 const CSeq_submit& ss)
{
    string desc = "Seq-submit: ";
    
    m_ErrRepository->AddValidErrItem(sv, et, msg, desc, ss, "");
}


void CValidError_imp::PostObjErr 
(EDiagSev sv,
 EErrType et,
 const string&  msg,
 const CSerialObject& obj,
 const CSeq_entry *ctx)
{
    if (ctx == 0) {
        PostErr (sv, et, msg, obj);
    } else {
        const CSeqdesc* desc = dynamic_cast < const CSeqdesc* > (&obj);
        if (desc == 0) {
            PostErr (sv, et, msg, obj);
        } else {
            PostErr (sv, et, msg, *ctx, *desc);
        }
    }
}


void CValidError_imp::PostBadDateError 
(EDiagSev             sv,
 const string&        msg,
 int                  flags,
 const CSerialObject& obj,
 const CSeq_entry *ctx)
{
    string reasons = GetDateErrorDescription(flags);

    NStr::TruncateSpacesInPlace (reasons);
    reasons = msg + " - " + reasons;

    PostObjErr (sv, eErr_GENERIC_BadDate, reasons, obj, ctx);
}


bool CValidError_imp::Validate
(const CSeq_entry& se,
 const CCit_sub* cs,
 CScope* scope)
{
    CSeq_entry_Handle seh;
    try {
        seh = scope->GetSeq_entryHandle(se);
    } catch (const CException ) { ; }
    if (! seh) {
        seh = scope->AddTopLevelSeqEntry(se);
        if (!seh) {
            return false;
        }
    }

    return Validate(seh, cs);
}

bool CValidError_imp::ValidateDescriptorInSeqEntry (const CSeq_entry& se, CValidError_desc *descval)
{
    FOR_EACH_DESCRIPTOR_ON_SEQENTRY (it, se) {
        try {
            descval->ValidateSeqDesc(**it, se);
            if ( m_PrgCallback ) {
                m_PrgInfo.m_CurrentDone++;
                m_PrgInfo.m_TotalDone++;
                if ( m_PrgCallback(&m_PrgInfo) ) {
                    return false;
                }
            }
        } catch ( const exception& e ) {
            PostErr(eDiag_Fatal, eErr_INTERNAL_Exception,
                string("Exeption while validating descriptor. EXCEPTION: ") +
                e.what(), se, **it);
            return true;
        }
    }
    if (se.Which() == CSeq_entry::e_Set) {
        FOR_EACH_SEQENTRY_ON_SEQSET (it, se.GetSet()) {
            if (!ValidateDescriptorInSeqEntry (**it, descval)) {
                return false;
            }
        }
    }
    return true;
}
       

bool CValidError_imp::ValidateDescriptorInSeqEntry (const CSeq_entry& se)
{
    if ( m_PrgCallback ) {
        m_PrgInfo.m_State = CValidator::CProgressInfo::eState_Desc;
        m_PrgInfo.m_Current = m_NumDesc;
        m_PrgInfo.m_CurrentDone = 0;
        m_PrgCallback(&m_PrgInfo);
    }

    //CRef<CValidError_desc> desc_validator(new CValidError_desc(*this)); 
    CValidError_desc desc_validator(*this);
    return ValidateDescriptorInSeqEntry (se, &desc_validator);        
}


bool CValidError_imp::ValidateSeqDescrInSeqEntry (const CSeq_entry& se, CValidError_descr *descr_val)
{
    if (se.IsSetDescr()) {
        descr_val->ValidateSeqDescr (se.GetDescr(), se);
    }
    if (se.Which() == CSeq_entry::e_Set) {
        FOR_EACH_SEQENTRY_ON_SEQSET (it, se.GetSet()) {
            if (!ValidateSeqDescrInSeqEntry (**it, descr_val)) {
                return false;
            }
        }
    }
    return true;
}
       

bool CValidError_imp::ValidateSeqDescrInSeqEntry (const CSeq_entry& se)
{
    if ( m_PrgCallback ) {
        m_PrgInfo.m_State = CValidator::CProgressInfo::eState_Desc;
        m_PrgInfo.m_Current = m_NumDesc;
        m_PrgInfo.m_CurrentDone = 0;
        m_PrgCallback(&m_PrgInfo);
    }

    //CRef<CValidError_desc> desc_validator(new CValidError_desc(*this)); 
    CValidError_descr desc_validator(*this);
    return ValidateSeqDescrInSeqEntry (se, &desc_validator);        
}


bool CValidError_imp::Validate
(const CSeq_entry_Handle& seh,
 const CCit_sub* cs)
{
    _ASSERT(seh);

    if ( m_PrgCallback ) {
        m_PrgInfo.m_State = CValidator::CProgressInfo::eState_Initializing;
        if ( m_PrgCallback(&m_PrgInfo) ) {
            return false;
        }
    }

    // Check that CSeq_entry has data
    if (seh.Which() == CSeq_entry::e_not_set) {
        ERR_POST_X(2, Warning << "Seq_entry not set");
        return false;
    }

    Setup(seh);
 
    // Get first CBioseq object pointer for PostErr below.
    CTypeConstIterator<CBioseq> seq(ConstBegin(*m_TSE));
    if (!seq) {
        ERR_POST_X(3, "No Bioseq anywhere on this Seq-entry");
        return false;
    }

    // If m_NonASCII is true, then this flag was set by the caller
    // of validate to indicate that a non ascii character had been
    // read from a file being used to create a CSeq_entry, that the
    // error had been corrected, but that the error needs to be reported
    // by Validate. Note, Validate is not doing anything other than
    // reporting an error if m_NonASCII is true;
    if (m_NonASCII) {
        PostErr(eDiag_Error, eErr_GENERIC_NonAsciiAsn,
                  "Non-ascii chars in input ASN.1 strings", *seq);
        // Only report the error once
        m_NonASCII = false;
    }

    // Iterate thru components of record and validate each

    // before validating, need to know if we have insd seq in seq-entry
    m_IsINSDInSep = false;
    // also want to know if we have gi
    bool has_gi = false;
    // also want to know if there are any nucleotide sequences
    bool has_nucleotide_sequence = false;

    for (CBioseq_CI bi(GetTSEH(), CSeq_inst::eMol_not_set, CBioseq_CI::eLevel_All); 
         bi && (!m_IsINSDInSep || !has_gi || !has_nucleotide_sequence);
         ++bi) {
        FOR_EACH_SEQID_ON_BIOSEQ (it, *(bi->GetCompleteBioseq())) {
            if ((*it)->IsGenbank()
                || (*it)->IsEmbl()
                || (*it)->IsDdbj()
                || (*it)->IsTpg()
                || (*it)->IsTpe()
                || (*it)->IsTpd()) {
                m_IsINSDInSep = true;
            } else if ((*it)->IsGi()) {
                has_gi = true;
            }
        }
        if (bi->IsSetInst_Mol() && bi->IsNa()) {
            has_nucleotide_sequence = true;
        }
    }

	if (m_IsINSDInSep && IsRefSeq()) {
		PostErr (eDiag_Error, eErr_SEQ_PKG_INSDRefSeqPackaging,
		         "INSD and RefSeq records should not be present in the same set", *seq);
	}

#if 0
    // disabled for now
    // look for long IDs that would collide if truncated at 30 characters
    vector<string> id_strings;
    for (CBioseq_CI bi(GetTSEH(), CSeq_inst::eMol_not_set, CBioseq_CI::eLevel_All); 
         bi;
         ++bi) {
        FOR_EACH_SEQID_ON_BIOSEQ (it, *(bi->GetCompleteBioseq())) {
            if (!IsNCBIFILESeqId(**it)) {
                string label;
                (*it)->GetLabel(&label);
                id_strings.push_back(label);
            }
        }
    }
    stable_sort (id_strings.begin(), id_strings.end());
    for (vector<string>::iterator id_str_it = id_strings.begin();
         id_str_it != id_strings.end();
         ++id_str_it) {        
        string pattern = (*id_str_it).substr(0, 30);
        string first_id = *id_str_it;
        vector<string>::iterator cmp_it = id_str_it;
        ++cmp_it;
        while (cmp_it != id_strings.end() && NStr::StartsWith(*cmp_it, pattern)) {
            CRef<CSeq_id> id(new CSeq_id(*cmp_it));
            CBioseq_Handle bsh = m_Scope->GetBioseqHandle(*id);
            PostErr (eDiag_Warning, eErr_SEQ_INST_BadSeqIdFormat,
                     "First 30 characters of " + first_id + " and " +
                     *cmp_it + " are identical", *(bsh.GetCompleteBioseq()));
            ++id_str_it;
            ++cmp_it;
        }
    }
#endif

    // look for colliding feature IDs
    vector < int > feature_ids;
    for (CFeat_CI fi(GetTSEH()); fi; ++fi) {
        const CSeq_feat& sf = fi->GetOriginalFeature();
        if (sf.IsSetId() && sf.GetId().IsLocal() && sf.GetId().GetLocal().IsId()) {
            feature_ids.push_back(sf.GetId().GetLocal().GetId());
        }
    }

    if (feature_ids.size() > 0) {
        const CTSE_Handle& tse = seh.GetTSE_Handle ();
        stable_sort (feature_ids.begin(), feature_ids.end());
        vector <int>::iterator it = feature_ids.begin();
        int id = *it;
        ++it;
        while (it != feature_ids.end()) {
            if (*it == id) {
                vector<CSeq_feat_Handle> handles = tse.GetFeaturesWithId(CSeqFeatData::e_not_set, id);
                ITERATE( vector<CSeq_feat_Handle>, feat_it, handles ) {
                    PostErr (eDiag_Critical, eErr_SEQ_FEAT_CollidingFeatureIDs,
                             "Colliding feature ID " + NStr::IntToString (id), *(feat_it->GetSeq_feat()));
                }
                while (it != feature_ids.end() && *it == id) {
                    ++it;
                }
                if (it != feature_ids.end()) {
                    id = *it;
                    ++it;
                }
            } else {
                id = *it;
                ++it;
            }
        }
    }

    // look for mixed gps and non-gps sets
	bool has_nongps = false;
	bool has_gps = false;
    
    for (CTypeConstIterator<CBioseq_set> si(*m_TSE); si && (!has_nongps || !has_gps); ++si) {
		if (si->IsSetClass()) {
			if (si->GetClass() == CBioseq_set::eClass_mut_set
			    || si->GetClass() == CBioseq_set::eClass_pop_set
			    || si->GetClass() == CBioseq_set::eClass_phy_set
			    || si->GetClass() == CBioseq_set::eClass_eco_set
			    || si->GetClass() == CBioseq_set::eClass_wgs_set) {
			    has_nongps = true;
			} else if (si->GetClass() == CBioseq_set::eClass_gen_prod_set) {
				has_gps = true;
			}
		}
    }

	if (has_nongps && has_gps) {
		PostErr(eDiag_Error, eErr_SEQ_PKG_GPSnonGPSPackaging,
			"Genomic product set and mut/pop/phy/eco set records should not be present in the same set",
			*seq);
	}


    // validate the main data
    if (seh.IsSeq()) {
        const CBioseq& seq = seh.GetCompleteSeq_entry()->GetSeq();
        CValidError_bioseq bioseq_validator(*this);
        try {
            bioseq_validator.ValidateBioseq(seq);
        } catch ( const exception& e ) {
            PostErr(eDiag_Fatal, eErr_INTERNAL_Exception,
                string("Exeption while validating bioseq. EXCEPTION: ") +
                e.what(), seq);
            return true;
        }
    } else if (seh.IsSet()) {
        const CBioseq_set& set = seh.GetCompleteSeq_entry()->GetSet();
        CValidError_bioseqset bioseqset_validator(*this);
        try {
            bioseqset_validator.ValidateBioseqSet(set);
        } catch ( const exception& e ) {
            PostErr(eDiag_Fatal, eErr_INTERNAL_Exception,
                string("Exeption while validating bioseq set. EXCEPTION: ") +
                e.what(), set);
            return true;
        }
    }


    // validation from data collected during previous step
    
    if ( m_NumTpaWithHistory > 0  &&
         m_NumTpaWithoutHistory > 0 ) {
        PostErr(eDiag_Error, eErr_SEQ_INST_TpaAssmeblyProblem,
            "There are " +
            NStr::IntToString(m_NumTpaWithHistory) +
            " TPAs with history and " + 
            NStr::IntToString(m_NumTpaWithoutHistory) +
            " without history in this record.", *m_TSE);
    }
    if ( m_NumTpaWithoutHistory > 0 && has_gi) {
        PostErr (eDiag_Warning, eErr_SEQ_INST_TpaAssmeblyProblem,
            "There are " +
            NStr::IntToString(m_NumTpaWithoutHistory) +
            " TPAs without history in this record, but the record has a gi number assignment.", *m_TSE);
    }
    if (IsIndexerVersion() && DoesAnyProteinHaveGeneralID() && !IsRefSeq() && has_nucleotide_sequence) {
        PostErr (eDiag_Info, eErr_SEQ_INST_ProteinsHaveGeneralID, 
                 "INDEXER_ONLY - Protein bioseqs have general seq-id.",
                 *(seh.GetCompleteSeq_entry()));
    }
        

    ReportMissingPubs(*m_TSE, cs);
    ReportMissingBiosource(*m_TSE);

    if (m_NumMisplacedFeatures > 1) {
        PostErr (eDiag_Critical, eErr_SEQ_PKG_FeaturePackagingProblem, 
                 "There are " + NStr::IntToString (m_NumMisplacedFeatures) + " mispackaged features in this record.",
                 *(seh.GetCompleteSeq_entry()));
    } else if (m_NumMisplacedFeatures == 1) {
        PostErr (eDiag_Critical, eErr_SEQ_PKG_FeaturePackagingProblem,
                 "There is 1 mispackaged feature in this record.",
                 *(seh.GetCompleteSeq_entry()));
    }
    if ( m_NumGenes == 0  &&  
         m_NumGeneXrefs > 0 ) {
        PostErr(eDiag_Warning, eErr_SEQ_FEAT_OnlyGeneXrefs,
            "There are " + NStr::IntToString(m_NumGeneXrefs) +
            " gene xrefs and no gene features in this record.", *m_TSE);
    }
    ValidateCitations (seh);


    if ( m_NumMisplacedGraphs > 0 ) {
        string num = NStr::IntToString(m_NumMisplacedGraphs);
        PostErr(eDiag_Critical, eErr_SEQ_PKG_GraphPackagingProblem,
            string("There ") + ((m_NumMisplacedGraphs > 1) ? "are " : "is ") + num + 
            " mispackaged graph" + ((m_NumMisplacedGraphs > 1) ? "s" : "") + " in this record.",
            *m_TSE);
    }


    FindEmbeddedScript(*(seh.GetCompleteSeq_entry()));
    FindCollidingSerialNumbers(*(seh.GetCompleteSeq_entry()));

	if (m_UseEntrez) {
		ValidateTaxonomy(*(seh.GetCompleteSeq_entry()));
	}

    // validate cit-sub
    if (cs) {
        ValidateCitSub (*cs, *(seh.GetCompleteSeq_entry()), seh.GetCompleteSeq_entry());
    }

    return true;
}


void CValidError_imp::Validate(const CSeq_submit& ss, CScope* scope)
{
    // Check that ss is type e_Entrys
    if ( ss.GetData().Which() != CSeq_submit::C_Data::e_Entrys ) {
        return;
    }

    // Get CCit_sub pointer
    const CCit_sub* cs = &ss.GetSub().GetCit();

    // Just loop thru CSeq_entrys
    FOR_EACH_SEQENTRY_ON_SEQSUBMIT (se_itr, ss) {
        const CSeq_entry& se = **se_itr;
        Validate (se, cs, scope);
    }
}


void CValidError_imp::Validate(const CSeq_annot_Handle& sah)
{
    Setup(sah);
    
    // Iterate thru components of record and validate each

    CValidError_annot annot_validator(*this);
    annot_validator.ValidateSeqAnnot(sah);

    switch (sah.Which()) {
    case CSeq_annot::TData::e_Ftable :
        {
            CValidError_feat feat_validator(*this);
            // for (CTypeConstIterator <CSeq_feat> fi (sa); fi; ++fi) {
            for (CFeat_CI fi (sah); fi; ++fi) {
                const CSeq_feat& sf = fi->GetOriginalFeature();
                feat_validator.ValidateSeqFeat(sf);
            }
        }
        break;

    case CSeq_annot::TData::e_Align :
        {
            CValidError_align align_validator(*this);
            // for (CTypeConstIterator <CSeq_align> ai (sa); ai; ++ai) {
            for (CAlign_CI ai(sah); ai; ++ai) {
                const CSeq_align& sa = ai.GetOriginalSeq_align();
                align_validator.ValidateSeqAlign(sa);
            }
        }
        break;

    case CSeq_annot::TData::e_Graph :
        {
            CValidError_graph graph_validator(*this);
            // for (CTypeConstIterator <CSeq_graph> gi (sa); gi; ++gi) {
            for (CGraph_CI gi(sah); gi; ++gi) {
                const CSeq_graph& sg = gi->GetOriginalGraph();
                graph_validator.ValidateSeqGraph(sg);
            }
        }
        break;
    default:
        break;
    }
    FindEmbeddedScript(*(sah.GetCompleteSeq_annot()));
    FindCollidingSerialNumbers(*(sah.GetCompleteSeq_annot()));
}


void CValidError_imp::Validate(const CSeq_feat& feat)
{
    CValidError_feat feat_validator(*this);
    feat_validator.ValidateSeqFeat(feat);
    if (feat.IsSetData() && feat.GetData().IsBiosrc()) {
        const CBioSource& src = feat.GetData().GetBiosrc();
        if (src.IsSetOrg()) {
            ValidateTaxonomy (src.GetOrg(), src.IsSetGenome() ? src.GetGenome() : CBioSource::eGenome_unknown);
        }
    }
    FindEmbeddedScript(feat);
    FindCollidingSerialNumbers(feat);
}


void CValidError_imp::Validate(const CBioSource& src)
{
    ValidateBioSource(src, src);
    if (src.IsSetOrg()) {
        ValidateTaxonomy (src.GetOrg(), src.IsSetGenome() ? src.GetGenome() : CBioSource::eGenome_unknown);
    }
    FindEmbeddedScript(src);
    FindCollidingSerialNumbers(src);
}


void CValidError_imp::Validate(const CPubdesc& pubdesc)
{
    ValidatePubdesc(pubdesc, pubdesc);
    FindEmbeddedScript(pubdesc);
    FindCollidingSerialNumbers(pubdesc);
}


void CValidError_imp::SetProgressCallback
(CValidator::TProgressCallback callback,
 void* user_data)
{
    m_PrgCallback = callback;
    m_PrgInfo.m_UserData = user_data;
}


void CValidError_imp::ValidateDbxref
(const CDbtag& xref,
 const CSerialObject& obj,
 bool biosource,
 const CSeq_entry *ctx)
{
	if (xref.IsSetTag() && xref.GetTag().IsStr()) {
		if (ContainsSgml(xref.GetTag().GetStr())) {
            PostObjErr(eDiag_Warning, eErr_GENERIC_SgmlPresentInText,
                "dbxref value " + xref.GetTag().GetStr() + " has SGML",
				obj, ctx);
		}
	}

    if ( !xref.CanGetDb() ) {
        return;
    }
    const string& db = xref.GetDb();

	if (ContainsSgml(db)) {
        PostObjErr(eDiag_Warning, eErr_GENERIC_SgmlPresentInText,
            "dbxref database " + db + " has SGML",
			obj, ctx);
	}

    CDbtag::EDbtagType db_type = xref.GetType();

    bool refseq = IsRefSeq();

	bool src_db = false;
	bool refseq_db = false;
	string correct_caps = "";

	if (xref.GetDBFlags (refseq_db, src_db, correct_caps)) {
		if ( biosource) {
			if (NStr::EqualCase(correct_caps, db)) {
				if (src_db) {
					// it's all good
				} else if (refseq_db) {
					if (refseq || IsGPS()) {
						PostObjErr (eDiag_Warning, eErr_SEQ_FEAT_IllegalDbXref, 
									"RefSeq-specific db_xref type " + db + " should not used on an OrgRef",
									obj, ctx);
					} else {
						PostObjErr (eDiag_Warning, eErr_SEQ_FEAT_IllegalDbXref,
									"RefSeq-specific db_xref type " + db + " should not used on a non-RefSeq OrgRef",
									obj, ctx);
					}
				} else {
					PostObjErr (eDiag_Warning, eErr_SEQ_FEAT_IllegalDbXref,
								"db_xref type " + db + " should not used on an OrgRef",
								obj, ctx);
				}
			} else {
				// capitalization is bad
				if (src_db) {
					PostObjErr (eDiag_Warning, eErr_SEQ_FEAT_IllegalDbXref, 
								"Illegal db_xref type " + db + ", legal capitalization is " + correct_caps,
								obj, ctx);
				} else {
					PostObjErr (eDiag_Warning, eErr_SEQ_FEAT_IllegalDbXref, 
				                "Illegal db_xref type " + db + ", legal capitalization is " + correct_caps + ", but should not be used on an OrgRef",
								obj, ctx);
				}
			}
		} else {
			if (NStr::EqualCase(correct_caps, db)) {
				if (refseq_db) {
					if (refseq || IsGPS()) {
                        // it's all good
					} else {
						PostObjErr (eDiag_Warning, eErr_SEQ_FEAT_IllegalDbXref, 
									"db_xref type " + db + " is only legal for RefSeq",
									obj, ctx);
					}
				} else if (src_db && NStr::EqualNocase(db, "taxon")) {
					PostObjErr (eDiag_Warning, eErr_SEQ_FEAT_IllegalDbXref,
								"db_xref type taxon should only be used on an OrgRef",
								obj, ctx);
				}
			} else {
				// capitalization is bad
				if (src_db && NStr::EqualNocase(db, "taxon")) {
					PostObjErr (eDiag_Warning, eErr_SEQ_FEAT_IllegalDbXref,
								"Illegal db_xref type " + db + ", legal capitalization is " + correct_caps + ", but should only be used on an OrgRef",
								obj, ctx);
				} else {
					PostObjErr (eDiag_Warning, eErr_SEQ_FEAT_IllegalDbXref,
								"Illegal db_xref type " + db + ", legal capitalization is " + correct_caps,
								obj, ctx);
				}
			}
		}
	} else {
        PostObjErr(eDiag_Warning, eErr_SEQ_FEAT_IllegalDbXref,
            "Illegal db_xref type " + db, obj, ctx);
	}

}


void CValidError_imp::ValidateDbxref
(TDbtags& xref_list,
 const CSerialObject& obj,
 bool biosource,
 const CSeq_entry *ctx)
{
	string last_db = "";

    ITERATE( TDbtags, xref, xref_list) {
		if (biosource
			&& (*xref)->IsSetDb()) {
			if (!NStr::IsBlank(last_db) 
				&& NStr::EqualNocase((*xref)->GetDb(), last_db)) {
				PostObjErr (eDiag_Warning, eErr_SEQ_DESCR_BioSourceDbTagConflict, 
							"BioSource uses db " + last_db + " multiple times",
							obj, ctx);
			}
			last_db = (*xref)->GetDb();
		}
        ValidateDbxref(**xref, obj, biosource, ctx);
    }
}


void CValidError_imp::ValidateSeqLoc
(const CSeq_loc& loc,
 const CBioseq_Handle&  seq,
 const string&   prefix,
 const CSerialObject& obj)
{
    bool circular = false;
    circular = seq  &&  seq.GetInst_Topology() == CSeq_inst::eTopology_circular;
    
    bool ordered = true, adjacent = false, chk = true,
        unmarked_strand = false, mixed_strand = false;
    const CSeq_id* id_cur = 0, *id_prv = 0;
    const CSeq_interval *int_cur = 0, *int_prv = 0;
    ENa_strand strand_cur = eNa_strand_unknown,
        strand_prv = eNa_strand_unknown;

    unsigned int num_mix = 0;
    unsigned int zero_gi = 0;

    CTypeConstIterator<CSeq_loc> lit = ConstBegin(loc);
    for (; lit; ++lit) {
        try {
            switch (lit->Which()) {
            case CSeq_loc::e_Int:
                int_cur = &lit->GetInt();
                strand_cur = int_cur->IsSetStrand() ?
                    int_cur->GetStrand() : eNa_strand_unknown;
                id_cur = &int_cur->GetId();
                chk = IsValid(*int_cur, m_Scope);
                if (chk  &&  int_prv  && ordered  &&
                    !circular  && id_prv) {
                    if (IsSameBioseq(*id_prv, *id_cur, m_Scope)) {
                        if (strand_cur == eNa_strand_minus) {
                            if (int_prv->GetTo() < int_cur->GetTo()) {
                                ordered = false;
                            }
                            if (int_cur->GetTo() + 1 == int_prv->GetFrom()) {
                                adjacent = true;
                            }
                        } else {
                            if (int_prv->GetTo() > int_cur->GetTo()) {
                                ordered = false;
                            }
                            if (int_prv->GetTo() + 1 == int_cur->GetFrom()) {
                                adjacent = true;
                            }
                        }
                    }
                }
                if (int_prv) {
                    if (IsSameBioseq(int_prv->GetId(), int_cur->GetId(), m_Scope)){
                        if (strand_prv == strand_cur  &&
                            int_prv->GetFrom() == int_cur->GetFrom()  &&
                            int_prv->GetTo() == int_cur->GetTo()) {
                            PostErr(eDiag_Error,
                                eErr_SEQ_FEAT_DuplicateInterval,
                                "Duplicate exons in location", obj);
                        }
                    }
                }
                int_prv = int_cur;
                break;
            case CSeq_loc::e_Pnt:
                strand_cur = lit->GetPnt().IsSetStrand() ?
                    lit->GetPnt().GetStrand() : eNa_strand_unknown;
                id_cur = &lit->GetPnt().GetId();
                chk = IsValid(lit->GetPnt(), m_Scope);
                int_prv = 0;
                break;
            case CSeq_loc::e_Packed_pnt:
                strand_cur = lit->GetPacked_pnt().IsSetStrand() ?
                    lit->GetPacked_pnt().GetStrand() : eNa_strand_unknown;
                id_cur = &lit->GetPacked_pnt().GetId();
                chk = IsValid(lit->GetPacked_pnt(), m_Scope);
                int_prv = 0;
                break;
            case CSeq_loc::e_Null:
                break;
            default:
                strand_cur = eNa_strand_other;
                id_cur = 0;
                int_prv = 0;
                break;
            }
            if (!chk) {
                string lbl;
                lit->GetLabel(&lbl);
                PostErr(eDiag_Critical, eErr_SEQ_FEAT_Range,
                    prefix + ": SeqLoc [" + lbl + "] out of range", obj);
            }

            if (lit->IsMix()) {
                num_mix ++;
            }

            if (lit->GetId() != 0 && lit->GetId()->IsGi() && lit->GetId()->GetGi() == 0) {
                zero_gi ++;
            }
            
            if (lit->Which() != CSeq_loc::e_Null) {
                if (strand_prv != eNa_strand_other  &&
                    strand_cur != eNa_strand_other) {
                    if (id_cur  &&  id_prv  &&
                        IsSameBioseq(*id_cur, *id_prv, m_Scope)) {
                        if (strand_prv != strand_cur) {
                            if ((strand_prv == eNa_strand_plus  &&
                                strand_cur == eNa_strand_unknown)  ||
                                (strand_prv == eNa_strand_unknown  &&
                                strand_cur == eNa_strand_plus)) {
                                unmarked_strand = true;
                            } else {
                                mixed_strand = true;
                            }
                        }
                    }
                }                
                strand_prv = strand_cur;
                id_prv = id_cur;
            }
        } catch( const exception& e ) {
            string label;
            lit->GetLabel(&label);
            PostErr(eDiag_Error, eErr_INTERNAL_Exception,  
                "Exception caught while validating location " +
                label + ". Exception: " + e.what(), obj);
                
            strand_cur = eNa_strand_other;
            id_cur = 0;
            int_prv = 0;
        }
        
    }

    if (num_mix > 1) {
        string label;
        loc.GetLabel(&label);
        PostErr (eDiag_Error, eErr_SEQ_FEAT_NestedSeqLocMix, 
            prefix + ": SeqLoc [" + label + "] has nested SEQLOC_MIX elements",
                 obj);
    }
    if (zero_gi > 0) {
        string label = "?";
        if (seq && seq.IsSetId()) {
            label = seq.GetId().front().GetSeqId()->AsFastaString();
        }

        PostErr (eDiag_Critical, eErr_SEQ_FEAT_FeatureLocationIsGi0,
                 "Feature has " + NStr::IntToString(zero_gi) 
                 + " gi|0 location" + (zero_gi > 1 ? "s" : "")
                 + " on Bioseq " + label,
                 obj);
    }


    // Warn if different parts of a seq-loc refer to the same bioseq using 
    // differnt id types (i.e. gi and accession)
    ValidateSeqLocIds(loc, obj);
    
    bool trans_splice = false;
    bool exception = false;
    const CSeq_feat* sfp = dynamic_cast<const CSeq_feat*>(&obj);
    if (sfp != 0) {
        
        // Publication intervals ordering does not matter
        
        if ( sfp->GetData().GetSubtype() == CSeqFeatData::eSubtype_pub ) {
            ordered = true;
            adjacent = false;
        }
        
        // ignore ordering of heterogen bonds
        
        if ( sfp->GetData().GetSubtype() == CSeqFeatData::eSubtype_het ) {
            ordered = true;
            adjacent = false;
        }
        
        // misc_recomb intervals SHOULD be in reverse order
        if ( sfp->GetData().GetSubtype() == CSeqFeatData::eSubtype_misc_recomb ) {
            ordered = true;
        }
        
        // primer_bind intervals MAY be in on opposite strands
        
        if ( sfp->GetData().GetSubtype() == CSeqFeatData::eSubtype_primer_bind ) {
            mixed_strand = false;
            unmarked_strand = false;
            ordered = true;
        }
        
        exception = sfp->IsSetExcept() ?  sfp->GetExcept() : false;
        if (exception  &&  sfp->CanGetExcept_text()) {
            // trans splicing exception turns off both mixed_strand and
            // out_of_order messages
            if (NStr::FindNoCase(sfp->GetExcept_text(), "trans-splicing") != NPOS) {
                trans_splice = true;
            }
        }
    }

    string loc_lbl;
    if (adjacent) {
        loc.GetLabel(&loc_lbl);
        EDiagSev sev = exception ? eDiag_Warning : eDiag_Error;
        PostErr(sev, eErr_SEQ_FEAT_AbuttingIntervals,
            prefix + ": Adjacent intervals in SeqLoc [" +
            loc_lbl + "]", obj);
    }

    if (trans_splice) {
        return;
    }

    if (mixed_strand  ||  unmarked_strand  ||  !ordered) {
        if (loc_lbl.empty()) {
            loc.GetLabel(&loc_lbl);
        }
        if (mixed_strand) {
            PostErr(eDiag_Error, eErr_SEQ_FEAT_MixedStrand,
                prefix + ": Mixed strands in SeqLoc ["
                + loc_lbl + "]", obj);
        } else if (unmarked_strand) {
            PostErr(eDiag_Warning, eErr_SEQ_FEAT_MixedStrand,
                prefix + ": Mixed plus and unknown strands in SeqLoc ["
                + loc_lbl + "]", obj);
        }
        if (!ordered) {
            PostErr(eDiag_Error, eErr_SEQ_FEAT_SeqLocOrder,
                prefix + ": Intervals out of order in SeqLoc [" +
                loc_lbl + "]", obj);
        }
        return;
    }

    if ( seq  &&
         seq.IsSetInst_Repr()  &&
         seq.GetInst_Repr() != CSeq_inst::eRepr_seg ) {
        return;
    }

    // Check for intervals out of order on segmented Bioseq
    if ( seq  &&  BadSeqLocSortOrder(*seq.GetCompleteBioseq(), loc, m_Scope) ) {
        if (loc_lbl.empty()) {
            loc.GetLabel(&loc_lbl);
        }
        PostErr(eDiag_Error, eErr_SEQ_FEAT_SeqLocOrder,
            prefix + "Intervals out of order in SeqLoc [" +
            loc_lbl + "]", obj);
    }

    // Check for mixed strand on segmented Bioseq
    if ( IsMixedStrands(loc) ) {
        if (loc_lbl.empty()) {
            loc.GetLabel(&loc_lbl);
        }
        PostErr(eDiag_Error, eErr_SEQ_FEAT_MixedStrand,
            prefix + ": Mixed strands in SeqLoc [" +
            loc_lbl + "]", obj);
    }
}


void CValidError_imp::AddBioseqWithNoBiosource(const CBioseq& seq)
{
    m_BioseqWithNoSource.push_back(CConstRef<CBioseq>(&seq));
}


void CValidError_imp::AddProtWithoutFullRef(const CBioseq_Handle& seq)
{
    const CSeq_feat* cds = GetCDSForProduct(seq);
    if ( cds == 0 ) {
        PostErr (eDiag_Error, eErr_SEQ_FEAT_NoProtRefFound, 
                 "No full length Prot-ref feature applied to this Bioseq", *(seq.GetCompleteBioseq()));
    } else {
        PostErr (eDiag_Error, eErr_SEQ_FEAT_NoProtRefFound, 
                 "No full length Prot-ref feature applied to this Bioseq", *cds);
    }
}


bool CValidError_imp::IsWGSIntermediate(const CBioseq& seq)
{
	bool wgs = false;

	FOR_EACH_DESCRIPTOR_ON_BIOSEQ (it, seq) {
		if ((*it)->IsMolinfo() && (*it)->GetMolinfo().IsSetTech()
			&& (*it)->GetMolinfo().GetTech() == CMolInfo::eTech_wgs) {
			wgs = true;
		    break;
		}
	}
	if (!wgs) {
		return false;
	}

    bool is_other = false;
    bool has_gi = false;

	FOR_EACH_SEQID_ON_BIOSEQ (it, seq) {
		if ((*it)->IsOther()) {
			is_other = true;
			break;
		} else if ((*it)->IsGi()) {
			has_gi = true;
			break;
		}
	}
    if (!is_other || has_gi) {
        return false;
    }

	return true;
}


bool CValidError_imp::IsWGSIntermediate(const CSeq_entry& se)
{
	if (se.IsSeq()) {
		return IsWGSIntermediate(se.GetSeq());
	} else if (se.IsSet()) {
		const CBioseq_set& set = se.GetSet();
		FOR_EACH_SEQENTRY_ON_SEQSET (it, set) {
			if ((*it)->IsSet()) {
                return IsWGSIntermediate(**it);
			} else if ((*it)->IsSeq() && (*it)->GetSeq().IsNa()) {
				return IsWGSIntermediate((*it)->GetSeq());
			}
		}
	}
	return false;
}


void CValidError_imp::ReportMissingBiosource(const CSeq_entry& se)
{
    if(m_NoBioSource  &&  !m_IsPatent  &&  !m_IsPDB) {
        PostErr(eDiag_Error, eErr_SEQ_DESCR_NoOrgFound,
            "No organism name anywhere on this entire record.", se);
        return;
    }
    
    size_t num_no_source = m_BioseqWithNoSource.size();
    
    for ( size_t i = 0; i < num_no_source; ++i ) {
        PostErr(eDiag_Error, eErr_SEQ_DESCR_NoOrgFound,
                "No organism name has been applied to this Bioseq.  Other qualifiers may exist.",
                *(m_BioseqWithNoSource[i]));
    }
}


bool CValidError_imp::IsNucAcc(const string& acc)
{
    if ( isupper((unsigned char) acc[0])  &&  acc.find('_') != NPOS ) {
        return true;
    }

    return false;
}


bool CValidError_imp::IsFarLocation(const CSeq_loc& loc)
{
    for ( CSeq_loc_CI citer(loc); citer; ++citer ) {
        CConstRef<CSeq_id> id(&citer.GetSeq_id());
        if ( id ) {
            CBioseq_Handle near_seq = 
                m_Scope->GetBioseqHandleFromTSE(*id, GetTSE());
            if ( !near_seq ) {
                return true;
            }
        }
    }

    return false;
}


CConstRef<CSeq_feat> CValidError_imp::GetCDSGivenProduct(const CBioseq& seq)
{
    CConstRef<CSeq_feat> feat;

    CBioseq_Handle bsh = m_Scope->GetBioseqHandle(seq);

    if ( bsh ) {
        if ( IsNT()  &&  m_TSE ) {
            // In case of a NT bioseq limit the search to features packaged on the 
            // NT (we assume features have been pulled from the segments to the NT).
            CFeat_CI fi(bsh, 
                        SAnnotSelector(CSeqFeatData::e_Cdregion)
                        .SetByProduct()
                        .SetLimitTSE(m_Scope->GetSeq_entryHandle(*m_TSE)));
            if ( fi ) {
                // return the first one (should be the one packaged on the
                // nuc-prot set).
                feat.Reset(&(fi->GetOriginalFeature()));
            }
        } else {
            CFeat_CI fi(bsh, 
                        SAnnotSelector(CSeqFeatData::e_Cdregion)
                        .SetByProduct());
            if ( fi ) {
                // return the first one (should be the one packaged on the
                // nuc-prot set).
                feat.Reset(&(fi->GetOriginalFeature()));
            }
        }
    }

    return feat;
}


CConstRef<CSeq_feat> CValidError_imp::GetmRNAGivenProduct(const CBioseq& seq)
{
    CConstRef<CSeq_feat> feat;

    CBioseq_Handle bsh = m_Scope->GetBioseqHandle(seq);


    if ( bsh ) {
        // In case of a NT bioseq limit the search to features packaged on the 
        // NT (we assume features have been pulled from the segments to the NT).
        CSeq_entry_Handle limit;
        if ( IsNT()  &&  m_TSE ) {
            limit = m_Scope->GetSeq_entryHandle(*m_TSE);
        }

        if (limit) {
            CFeat_CI fi(bsh, 
                        SAnnotSelector(CSeqFeatData::eSubtype_mRNA)
                        .SetByProduct()
                        .SetLimitTSE(limit));
            if ( fi ) {
                // return the first one (should be the one packaged on the
                // nuc-prot set).
                feat.Reset(&(fi->GetOriginalFeature()));
            }
        } else {
            CFeat_CI fi(bsh, 
                        SAnnotSelector(CSeqFeatData::eSubtype_mRNA)
                        .SetByProduct());
            if ( fi ) {
                // return the first one (should be the one packaged on the
                // nuc-prot set).
                feat.Reset(&(fi->GetOriginalFeature()));
            }
        }
    }

    return feat;
}


const CSeq_entry* CValidError_imp::GetAncestor
(const CBioseq& seq,
 CBioseq_set::EClass clss)
{
    const CSeq_entry* parent = 0;
    for ( parent = seq.GetParentEntry(); 
          parent != 0;
          parent = parent->GetParentEntry() ) {
        if ( parent->IsSet() ) {
            const CBioseq_set& set = parent->GetSet();
            if ( set.IsSetClass()  &&  set.GetClass() == clss ) {
                break;
            }
        }
    }
    return parent;
}


bool CValidError_imp::IsSerialNumberInComment(const string& comment)
{
    size_t pos = comment.find('[', 0);
    while ( pos != string::npos ) {
        ++pos;
        if ( isdigit((unsigned char) comment[pos]) ) {
            while ( isdigit((unsigned char) comment[pos]) ) {
                ++pos;
            }
            if ( comment[pos] == ']' ) {
                return true;
            }
        }

        pos = comment.find('[', pos);
    }
    return false;
}


bool CValidError_imp::CheckSeqVector(const CSeqVector& vec)
{
    if ( IsSequenceAvaliable(vec) ) {
        return true;
    }

    if ( IsRemoteFetch() ) {
        // issue some sort of error
    }
    return false;
}


bool CValidError_imp::IsSequenceAvaliable(const CSeqVector& vec)
{
    // IMPORTANT: This is a temporary implementation, due to (yet) restricted
    // implementation of the Scope / object manager classes.
    // if the first and last elements are accesible the sequence is available.
    try {
        vec[0]; 
        vec[vec.size() - 1];
    } catch ( const exception& ) {
        // do something
        return false;
    }

    return true;
}


static void s_CollectPubDescriptorLabels (const CSeq_entry& se,
                                          vector<int>& pmids, vector<int>& muids, vector<int>& serials,
                                          vector<string>& published_labels, vector<string>& unpublished_labels)
{
    FOR_EACH_SEQDESC_ON_SEQENTRY (it, se) {
        if ((*it)->IsPub()) {
            GetPubdescLabels ((*it)->GetPub(), pmids, muids, serials, published_labels, unpublished_labels);
        }
    }

    if (se.IsSet()) {
        FOR_EACH_SEQENTRY_ON_SEQSET (it, se.GetSet()) {
            s_CollectPubDescriptorLabels (**it, pmids, muids, serials, published_labels, unpublished_labels);
        }
    }
}


void CValidError_imp::ValidateCitations (const CSeq_entry_Handle& seh)
{
    vector<int> pmids;
    vector<int> muids;
    vector<int> serials;
    vector<string> published_labels;
    vector<string> unpublished_labels;

    // collect labels for pubs on record
    s_CollectPubDescriptorLabels (*(seh.GetCompleteSeq_entry()), pmids, muids, serials, published_labels, unpublished_labels);
                
    CFeat_CI feat (seh, SAnnotSelector(CSeqFeatData::e_Pub));
    while (feat) {
        GetPubdescLabels (feat->GetData().GetPub(), pmids, muids, serials, published_labels, unpublished_labels);
        ++feat;
    }

    // now examine citations to determine whether they match a pub on the record
    CFeat_CI f (seh);
    while (f) {
        if (f->IsSetCit() && f->GetCit().IsPub()) {            
            ITERATE (CPub_set::TPub, cit_it, f->GetCit().GetPub()) {
                bool found = false;

                if ((*cit_it)->IsPmid()) {
                    vector<int>::iterator it = pmids.begin();    
                    while (it != pmids.end() && !found) {
                        if (*it == (*cit_it)->GetPmid()) {
                            found = true;
                        }
                        ++it;
                    }
                    if (!found) {
                        PostErr (eDiag_Warning, eErr_SEQ_FEAT_FeatureCitationProblem,
                                 "Citation on feature refers to uid ["
                                 + NStr::IntToString((*cit_it)->GetPmid())
                                 + "] not on a publication in the record",
                                 f->GetOriginalFeature());
                    }
                } else if ((*cit_it)->IsMuid()) {
                    vector<int>::iterator it = muids.begin();    
                    while (it != muids.end() && !found) {
                        if (*it == (*cit_it)->GetMuid()) {
                            found = true;
                        }
                        ++it;
                    }
                    if (!found) {
                        PostErr (eDiag_Warning, eErr_SEQ_FEAT_FeatureCitationProblem,
                                 "Citation on feature refers to uid ["
                                 + NStr::IntToString((*cit_it)->GetMuid())
                                 + "] not on a publication in the record",
                                 f->GetOriginalFeature());
                    }
                } else if ((*cit_it)->IsEquiv()) {
                    continue;
                } else {                    
                    string label;
                    (*cit_it)->GetLabel(&label, CPub::eContent, true);
                    if (NStr::EndsWith (label, ">")) {
                        label = label.substr(0, label.length() - 2);
                    }
                    size_t len = label.length();
                    vector<string>::iterator unpub_it = unpublished_labels.begin();
                    while (unpub_it != unpublished_labels.end() && !found) {
                        size_t it_len =(*unpub_it).length();
                        if (NStr::EqualNocase (*unpub_it, 0, it_len > len ? len : it_len, label)) {
                            found = true;
                        }
                        ++unpub_it;
                    }
                    vector<string>::iterator pub_it = published_labels.begin();

                    while (pub_it != published_labels.end() && !found) {
                        size_t it_len =(*pub_it).length();
                        if (NStr::EqualNocase (*pub_it, 0, it_len > len ? len : it_len, label)) {
                            PostErr (eDiag_Warning, eErr_SEQ_FEAT_FeatureCitationProblem,
                                     "Citation on feature needs to be updated to published uid",
                                     f->GetOriginalFeature());
                            found = true;
                        }
                        ++pub_it;
                    }
                    if (!found) {
                        PostErr (eDiag_Warning, eErr_SEQ_FEAT_FeatureCitationProblem,
                                 "Citation on feature refers to a publication not in the record",
                                 f->GetOriginalFeature());
                    }
                }
            }
        }
        ++f;
    }
}


// =============================================================================
//                                  Private
// =============================================================================


const char * script_tags[] = {"<script", "<object", "<applet", "<embed", "<form", "javascript:", "vbscript:", NULL};

void CValidError_imp::FindEmbeddedScript (const CSerialObject& obj)
{
    bool found = false;
    CStdTypeConstIterator<string> it(obj);

    while (it && !found) {
        for (int i = 0; script_tags[i] != NULL && !found; i++) {
            if (NStr::FindNoCase (*it, script_tags[i]) != string::npos) {
                found = true;
            }
        }
        ++it;
    }

    if (found) {
        PostErr (eDiag_Error, eErr_GENERIC_EmbeddedScript, "Script tag found in item", obj);
    }
}


bool CValidError_imp::IsMixedStrands(const CSeq_loc& loc)
{
    if ( SeqLocCheck(loc, m_Scope) == eSeqLocCheck_warning ) {
        return false;
    }

    CSeq_loc_CI curr(loc);
    if ( !curr ) {
        return false;
    }
    CSeq_loc_CI prev = curr;
    ++curr;
    
    while ( curr ) {
        ENa_strand curr_strand = curr.GetStrand();
        ENa_strand prev_strand = prev.GetStrand();

        if ( (prev_strand == eNa_strand_minus  &&  
              curr_strand != eNa_strand_minus)   ||
             (prev_strand != eNa_strand_minus  &&  
              curr_strand == eNa_strand_minus) ) {
            return true;
        }

        prev = curr;
        ++curr;
    }

    return false;
}


static bool s_SeqLocHasGI (const CSeq_loc& loc)
{
    bool rval = false;

    for ( CSeq_loc_CI it(loc); it && !rval; ++it ) {
        if (it.GetSeq_id().IsGi()) {
            rval = true;
        }
    }
    return rval;
}


static bool s_FieldRuleCompare (
    const CField_rule* p1,
    const CField_rule* p2
)

{
	return NStr::Compare(p1->GetField_name(), p2->GetField_name()) < 0;
}


CRef<CComment_set> CValidError_imp::GetStructuredCommentRules(void)
{
    if (m_StructuredCommentRules) {
        return m_StructuredCommentRules;
    }
	// note - may want to do this initialization later, when needed
    string dir;
    string fname;
    if (CNcbiApplication* app = CNcbiApplication::Instance()) {
        dir = app->GetConfig().Get("NCBI", "Data");
        if ( !dir.empty()  
            && CFile(CDirEntry::MakePath(dir, "validrules.prt")).Exists()) {
            dir = CDirEntry::AddTrailingPathSeparator(dir);
            fname = dir + "validrules.prt";
        } else {
            dir.erase();
        }
    }
    if (fname.empty()) {
        ERR_POST_X(2, Info << "Unable to load structured comment rules.");
    } else {
        auto_ptr<CObjectIStream> in;
        in.reset(CObjectIStream::Open(fname, eSerial_AsnText));
        string header = in->ReadFileHeader();

        m_StructuredCommentRules.Reset(new CComment_set());
        in->Read(ObjectInfo(*m_StructuredCommentRules), CObjectIStream::eNoFileHeader);        
        if (m_StructuredCommentRules->IsSet()) {
            NON_CONST_ITERATE(CComment_set::Tdata, it, m_StructuredCommentRules->Set()) {
                if (!(*it)->GetRequire_order() && (*it)->IsSetFields()) {
                    CField_set& fields = (*it)->SetFields();
                    fields.Set().sort(s_FieldRuleCompare);
                }
            }
        }
        
    }

    return m_StructuredCommentRules;
}


void CValidError_imp::Setup(const CSeq_entry_Handle& seh) 
{
    // "Save" the Seq-entry
    m_TSEH = seh;

    m_Scope.Reset(&m_TSEH.GetScope());
    
    m_TSE = m_TSEH.GetCompleteSeq_entry();
    
    // If no Pubs/BioSource in CSeq_entry, post only one error
    CTypeConstIterator<CPub> pub(ConstBegin(*m_TSE));
    m_NoPubs = !pub;
    CTypeConstIterator<CBioSource> src(ConstBegin(*m_TSE));
    m_NoBioSource = !src;
    
    // Look for genomic product set
    for (CTypeConstIterator <CBioseq_set> si (*m_TSE); si; ++si) {
        if (si->IsSetClass ()) {
            if (si->GetClass () == CBioseq_set::eClass_gen_prod_set) {
                m_IsGPS = true;
            }
        }
    }

    // Examine all Seq-ids on Bioseqs
    for (CTypeConstIterator <CBioseq> bi (*m_TSE); bi; ++bi) {
        FOR_EACH_SEQID_ON_BIOSEQ (sid_itr, *bi) {
            const CSeq_id& sid = **sid_itr;
            CSeq_id::E_Choice typ = sid.Which();
            switch (typ) {
                case CSeq_id::e_not_set:
                    break;
                case CSeq_id::e_Local:
                    break;
                case CSeq_id::e_Gibbsq:
                    break;
                case CSeq_id::e_Gibbmt:
                    break;
                case CSeq_id::e_Giim:
                    break;
                case CSeq_id::e_Genbank:
                    m_IsGB = true;
                    m_IsGED = true;
                    break;
                case CSeq_id::e_Embl:
                    m_IsGED = true;
                    m_IsEmbl = true;
                    break;
                case CSeq_id::e_Pir:
                    break;
                case CSeq_id::e_Swissprot:
                    break;
                case CSeq_id::e_Patent:
                    m_IsPatent = true;
                    break;
                case CSeq_id::e_Other:
                    m_IsRefSeq = true;
                    // and do RefSeq subclasses up front as well
                    if (sid.GetOther().IsSetAccession()) {
                        string acc =sid.GetOther().GetAccession().substr(0, 3);
                        if (acc == "NC_") {
                            m_IsNC = true;
                        } else if (acc == "NG_") {
                            m_IsNG = true;
                        } else if (acc == "NM_") {
                            m_IsNM = true;
                        } else if (acc == "NP_") {
                            m_IsNP = true;
                        } else if (acc == "NR_") {
                            m_IsNR = true;
                        } else if (acc == "NS_") {
                            m_IsNS = true;
                        } else if (acc == "NT_") {
                            m_IsNT = true;
                        } else if (acc == "NW_") {
                            m_IsNW = true;
                        } else if (acc == "XR_") {
                            m_IsXR = true;
                        }
                    }
                    break;
                case CSeq_id::e_General:
                    if ((*bi).IsAa() && !sid.GetGeneral().IsSkippable()) {
                        m_ProteinHasGeneralID = true;
                    }
                    break;
                case CSeq_id::e_Gi:
                    m_IsGI = true;
                    break;
                case CSeq_id::e_Ddbj:
                    m_IsGED = true;
                    m_IsDdbj = true;
                    break;
                case CSeq_id::e_Prf:
                    break;
                case CSeq_id::e_Pdb:
                    m_IsPDB = true;
                    break;
                case CSeq_id::e_Tpg:
                    break;
                case CSeq_id::e_Tpe:
                    break;
                case CSeq_id::e_Tpd:
                    break;
                default:
                    break;
            }
        }
    }

    // examine features for location gi, product gi, and locus tag
    for (CFeat_CI feat_ci (seh); 
         feat_ci && (!m_FeatLocHasGI || !m_ProductLocHasGI || !m_GeneHasLocusTag);
         ++feat_ci) {
        if (s_SeqLocHasGI(feat_ci->GetLocation())) {
            m_FeatLocHasGI = true;
        }
        if (feat_ci->IsSetProduct() && s_SeqLocHasGI(feat_ci->GetProduct())) {
            m_ProductLocHasGI = true;
        }
        if (feat_ci->IsSetData() && feat_ci->GetData().IsGene() 
            && feat_ci->GetData().GetGene().IsSetLocus_tag()
            && !NStr::IsBlank (feat_ci->GetData().GetGene().GetLocus_tag())) {
            m_GeneHasLocusTag = true;
        }
    }
    
    if ( m_PrgCallback ) {
        m_NumAlign = 0;
        for (CTypeConstIterator<CSeq_align> i(*m_TSE); i; ++i) {
            m_NumAlign++;
        }
        m_NumAnnot = 0;
        for (CTypeConstIterator<CSeq_annot> i(*m_TSE); i; ++i) {
            m_NumAnnot++;
        }
        m_NumBioseq = 0;
        for (CTypeConstIterator<CBioseq> i(*m_TSE); i; ++i) {
            m_NumBioseq++;
        }
        m_NumBioseq_set = 0;
        for (CTypeConstIterator<CBioseq_set> i(*m_TSE); i; ++i) {
            m_NumBioseq_set++;
        }
        m_NumDesc = 0;
        for (CTypeConstIterator<CSeqdesc> i(*m_TSE); i; ++i) {
            m_NumDesc++;
        }
        m_NumDescr = 0;
        for (CTypeConstIterator<CSeq_descr> i(*m_TSE); i; ++i) {
            m_NumDescr++;
        }
        m_NumFeat = 0;
        for (CTypeConstIterator<CSeq_feat> i(*m_TSE); i; ++i) {
            m_NumFeat++;
        }
        m_NumGraph = 0;
        for (CTypeConstIterator<CSeq_graph> i(*m_TSE); i; ++i) {
            m_NumGraph++;
        }
        m_PrgInfo.m_Total = m_NumAlign + m_NumAnnot + m_NumBioseq + 
            m_NumBioseq_set + m_NumDesc + m_NumDescr + m_NumFeat + 
            m_NumGraph;
    }

    if (CNcbiApplication::Instance()->GetProgramDisplayName() == "tbl2asn") {
        m_IsTbl2Asn = true;
    }
    if (!m_IsTbl2Asn) {
        m_RequireTaxonID = true;
    }
}


void CValidError_imp::SetScope(const CSeq_entry& se)
{
    m_Scope.Reset(new CScope(*m_ObjMgr));
    m_Scope->AddTopLevelSeqEntry(*const_cast<CSeq_entry*>(&se));
    m_Scope->AddDefaults();
}


void CValidError_imp::Setup(const CSeq_annot_Handle& sah)
{
    m_IsStandaloneAnnot = true;
    if (! m_Scope) {
        m_Scope.Reset(& sah.GetScope());
    }
    m_TSE.Reset(new CSeq_entry); // set a dummy Seq-entry
    m_TSEH = m_Scope->AddTopLevelSeqEntry(*m_TSE);
}


void CValidError_imp::ValidateSeqLocIds
(const CSeq_loc& loc,
 const CSerialObject& obj)
{
    for ( CSeq_loc_CI lit(loc); lit; ++lit ) {
        const CSeq_id& id1 = lit.GetSeq_id();
        CSeq_loc_CI  lit2 = lit;
        for ( ++lit2; lit2; ++lit2 ) {
            const CSeq_id& id2 = lit2.GetSeq_id();
            if ( IsSameBioseq(id1, id2, m_Scope)  &&  !id1.Match(id2) ) {
                PostErr(eDiag_Warning,
                    eErr_SEQ_FEAT_DifferntIdTypesInSeqLoc,
                    "Two ids refer to the same bioseq but are of "
                    "different type", obj);
            }
        }
    } 
}


// =============================================================================
//                         CValidError_base Implementation
// =============================================================================


CValidError_base::CValidError_base(CValidError_imp& imp) :
    m_Imp(imp), m_Scope(imp.GetScope())
{
}


CValidError_base::~CValidError_base()
{
}


void CValidError_base::PostErr
(EDiagSev sv,
 EErrType et,
 const string& msg,
 const CSerialObject& obj) 
{
    m_Imp.PostErr(sv, et, msg, obj);
}


//void CValidError_base::PostErr
//(EDiagSev sv,
// EErrType et,
// const string& msg,
// TDesc ds)
//{
//    m_Imp.PostErr(sv, et, msg, ds);
//}


void CValidError_base::PostErr
(EDiagSev sv,
 EErrType et,
 const string& msg,
 TFeat ft)
{
    m_Imp.PostErr(sv, et, msg, ft);
}


void CValidError_base::PostErr
(EDiagSev sv,
 EErrType et,
 const string& msg,
 TBioseq sq)
{
    m_Imp.PostErr(sv, et, msg, sq);
}


void CValidError_base::PostErr
(EDiagSev sv,
 EErrType et,
 const string& msg,
 TEntry ctx,
 TDesc ds)
{
    m_Imp.PostErr(sv, et, msg, ctx, ds);
}


//void CValidError_base::PostErr
//(EDiagSev sv,
// EErrType et,
// const string& msg,
// TBioseq sq,
// TDesc ds)
//{
//    m_Imp.PostErr(sv, et, msg, sq, ds);
//}


void CValidError_base::PostErr
(EDiagSev sv,
 EErrType et,
 const string& msg, 
 TSet set)
{
    m_Imp.PostErr(sv, et, msg, set);
}


//void CValidError_base::PostErr
//(EDiagSev sv, 
// EErrType et, 
// const string& msg, 
// TSet set,
// TDesc ds)
//{
//    m_Imp.PostErr(sv, et, msg, set, ds);
//}


void CValidError_base::PostErr
(EDiagSev sv,
 EErrType et,
 const string& msg,
 TAnnot annot)
{
    m_Imp.PostErr(sv, et, msg, annot);
}

void CValidError_base::PostErr
(EDiagSev sv,
 EErrType et,
 const string& msg,
 TGraph graph)
{
    m_Imp.PostErr(sv, et, msg, graph);
}


void CValidError_base::PostErr
(EDiagSev sv,
 EErrType et,
 const string& msg,
 TBioseq sq,
 TGraph graph)
{
    m_Imp.PostErr(sv, et, msg, sq, graph);
}


void CValidError_base::PostErr
(EDiagSev sv,
 EErrType et,
 const string& msg,
 TAlign align)
{
    m_Imp.PostErr(sv, et, msg, align);
}


void CValidError_base::PostErr
(EDiagSev sv,
 EErrType et,
 const string& msg,
 TEntry entry)
{
    m_Imp.PostErr(sv, et, msg, entry);
}


END_SCOPE(validator)
END_SCOPE(objects)
END_NCBI_SCOPE
