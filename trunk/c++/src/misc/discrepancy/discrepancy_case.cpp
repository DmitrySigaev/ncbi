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
 * Authors: Sema
 *
 */

#include <ncbi_pch.hpp>
#include "discrepancy_core.hpp"
#include "utils.hpp"
#include <objects/general/Dbtag.hpp>
#include <objects/macro/Molecule_type.hpp>
#include <objects/macro/Molecule_class_type.hpp>
#include <objects/seqfeat/Imp_feat.hpp>
#include <objects/seqfeat/SeqFeatXref.hpp>
#include <objects/seq/seqport_util.hpp>
#include <objects/macro/String_constraint.hpp>
#include <objects/misc/sequence_util_macros.hpp>
#include <objects/seq/MolInfo.hpp>
#include <objmgr/bioseq_ci.hpp>
#include <objmgr/feat_ci.hpp>
#include <objmgr/seq_vector.hpp>
#include <objmgr/util/feature.hpp>
#include <objmgr/util/sequence.hpp>
#include <objmgr/bioseq_handle.hpp>
#include <objmgr/seqdesc_ci.hpp>
#include <sstream>

BEGIN_NCBI_SCOPE;
BEGIN_SCOPE(NDiscrepancy)
USING_SCOPE(objects);

DISCREPANCY_MODULE(discrepancy_case);


// COUNT_NUCLEOTIDES

DISCREPANCY_CASE(COUNT_NUCLEOTIDES, CSeq_inst, eOncaller, "Count nucleotide sequences")
{
    CSeq_inst::TMol mol = obj.GetMol();
    if (mol != CSeq_inst::eMol_dna && mol != CSeq_inst::eMol_rna && mol != CSeq_inst::eMol_na) {
        return;
    }
    m_Objs["[n] nucleotide Bioseq[s] [is] present"].Add(*new CDiscrepancyObject(context.GetCurrentBioseq(), context.GetScope(), context.GetFile(), context.GetKeepRef()));
}


DISCREPANCY_SUMMARIZE(COUNT_NUCLEOTIDES)
{
    m_Objs["[n] nucleotide Bioseq[s] [is] present"]; // If no sequences found still report 0
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


// COUNT_PROTEINS

DISCREPANCY_CASE(COUNT_PROTEINS, CSeq_inst, eDisc, "Count Proteins")
{
    if (obj.GetMol() != CSeq_inst::eMol_aa) {
        return;
    }
    m_Objs["[n] protein sequence[s] [is] present"].Add(*new CDiscrepancyObject(context.GetCurrentBioseq(), context.GetScope(), context.GetFile(), context.GetKeepRef()));
}


DISCREPANCY_SUMMARIZE(COUNT_PROTEINS)
{
    m_Objs["[n] protein sequence[s] [is] present"]; // If no sequences found still report 0
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


// MISSING_PROTEIN_ID
DISCREPANCY_CASE(MISSING_PROTEIN_ID, CSeq_inst, eDisc, "Missing Protein ID")
{
    CConstRef<CBioseq> bioseq = context.GetCurrentBioseq();
    if( ! bioseq || ! bioseq->IsAa() ) {
        return;
    }

    const CSeq_id * protein_id = context.GetProteinId();
    if( ! protein_id ) {
        m_Objs["[n] protein[s] [has] invalid ID[s]."].Add(
            *new CDiscrepancyObject(
                context.GetCurrentBioseq(), context.GetScope(),
                context.GetFile(), context.GetKeepRef()), false);
    }
}

DISCREPANCY_SUMMARIZE(MISSING_PROTEIN_ID)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


// INCONSISTENT_PROTEIN_ID
DISCREPANCY_CASE(INCONSISTENT_PROTEIN_ID, CSeq_inst, eDisc,
                 "Inconsistent Protein ID")
{
    CConstRef<CBioseq> bioseq = context.GetCurrentBioseq();
    if( ! bioseq || ! bioseq->IsAa() ) {
        return;
    }

    const CSeq_id * protein_id = context.GetProteinId();
    if( ! protein_id ) {
        return;
    }
    _ASSERT(protein_id->IsGeneral());
    CTempString protein_id_prefix(
        GET_STRING_FLD_OR_BLANK(protein_id->GetGeneral(), Db));
    if( protein_id_prefix.empty() ) {
        return;
    }
    string protein_id_prefix_lowercase = protein_id_prefix;
    NStr::ToLower(protein_id_prefix_lowercase);

    // find (or create if none before) the canonical form of the
    // protein_id_prefix since case-insensitivity means it could have
    // multiple forms.  Here, the canonical form is the way it appears
    // the first time.
    CReportNode& canonical_forms_node = m_Objs["canonical forms"][protein_id_prefix_lowercase];
    CTempString canonical_protein_id_prefix;
    if( canonical_forms_node.empty() ) {
        // haven't seen this protein_id_prefix_lowercase before so we have
        // to set the canonical form.
        canonical_protein_id_prefix = protein_id_prefix;
        canonical_forms_node[protein_id_prefix];
    } else {
        // use previously set canonical form;
        canonical_protein_id_prefix =
            canonical_forms_node.GetMap().begin()->first;
    }
    _ASSERT(NStr::EqualNocase(protein_id_prefix, canonical_protein_id_prefix));

    m_Objs[kEmptyStr]["[n] sequence[s] [has] protein ID prefix " + canonical_protein_id_prefix].Add(
        *new CDiscrepancyObject(
            context.GetCurrentBioseq(), context.GetScope(),
            context.GetFile(), context.GetKeepRef()), false);
}

DISCREPANCY_SUMMARIZE(INCONSISTENT_PROTEIN_ID)
{
    // if _all_ are identical, don't report
    CReportNode& reports_collected = m_Objs[kEmptyStr];
    if( reports_collected.GetMap().size() <= 1 ) {
        // if there are no sequences or all sequences have the same
        // canonical protein id, then do not show any discrepancy
        return;
    }

    m_ReportItems = reports_collected.Export(*this)->GetSubitems();
}

// SHORT_SEQUENCES
DISCREPANCY_CASE(SHORT_SEQUENCES, CSeq_inst, eDisc, "Find Short Sequences")
{
    if (obj.IsAa()) {
        return;
    }
    if (obj.IsSetLength() && obj.GetLength() < 50 && !context.IsCurrentRnaInGenProdSet()) {
        m_Objs["[n] sequence[s] [is] shorter than 50 nt"].Add(*new CDiscrepancyObject(context.GetCurrentBioseq(), context.GetScope(), context.GetFile(), context.GetKeepRef()));
    }
}


DISCREPANCY_SUMMARIZE(SHORT_SEQUENCES)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


// N_RUNS

void FindNRuns(vector<CRange<TSeqPos> >& runs, const CSeq_data& seq_data, const TSeqPos start_pos, const TSeqPos min_run_length) {
    CSeq_data as_iupacna;
    TSeqPos nconv = CSeqportUtil::Convert(seq_data, &as_iupacna, CSeq_data::e_Iupacna);
    if (nconv == 0) {
        return;
    }

    CRange<TSeqPos> this_run;

    TSeqPos position = start_pos;

    const string& iupacna_str = as_iupacna.GetIupacna().Get();
    ITERATE(string, base, iupacna_str) {
        TSeqPos curr_length = this_run.GetLength();

        switch (*base) {
            case 'N':
                if (curr_length == 0) {
                    this_run.SetFrom(position);
                }
                this_run.SetToOpen(position + 1);
                break;
            default:
                if (curr_length >= min_run_length) {
                    runs.push_back(this_run);
                }

                if (curr_length != 0) {
                    this_run = CRange<TSeqPos>::GetEmpty();
                }
                break;
        }
        ++position;
    }
    if (this_run.GetLength() >= min_run_length) {
        runs.push_back(this_run);
    }
}

DISCREPANCY_CASE(N_RUNS, CSeq_inst, eDisc, "More than 10 Ns in a row")
{
    if (obj.IsAa() || context.SequenceHasFarPointers()) {
        return;
    }

    bool found_any = false;
    vector<CRange<TSeqPos> > runs;

    ostringstream sub_key;
    sub_key << context.GetCurrentBioseq()->GetFirstId()->GetSeqIdString();
    sub_key << " has runs of Ns at the following locations: ";

    // CSeqMap, not CSeqVector, because gaps should not count as N runs.
    const CRef<CSeqMap> seq_map = CSeqMap::CreateSeqMapForBioseq(*context.GetCurrentBioseq());
    SSeqMapSelector sel;
    sel.SetFlags(CSeqMap::fFindData);
    CSeqMap_CI seq_iter(seq_map, &context.GetScope(), sel);
    for (; seq_iter; ++seq_iter) {
        FindNRuns(runs, seq_iter.GetData(), seq_iter.GetPosition(), 10);
        ITERATE(vector<CRange<TSeqPos> >, run, runs) {
            if (!found_any) {
                found_any = true;
            } else {
                sub_key << ", ";
            }

            sub_key << (run->GetFrom() + 1) << "-" << (run->GetTo() + 1);
        }
        runs.clear();
    }

    if (found_any) {
        m_Objs["[n] sequence[s] [has] runs of 10 or more Ns"][sub_key.str()].Ext().Add(*new CDiscrepancyObject(context.GetCurrentBioseq(), context.GetScope(), context.GetFile(), context.GetKeepRef()));
    }
}

DISCREPANCY_SUMMARIZE(N_RUNS)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


// PERCENT_N

DISCREPANCY_CASE(PERCENT_N, CSeq_inst, eDisc, "More than 5 percent Ns")
{
    if (obj.IsAa() || context.SequenceHasFarPointers()) {
        return;
    }
    const CSeqSummary& sum = context.GetNucleotideCount();
    if (sum.N * 100. / sum.Len > 5) {
        m_Objs["[n] sequence[s] [has] more than 5% Ns"].Add(*new CDiscrepancyObject(context.GetCurrentBioseq(), context.GetScope(), context.GetFile(), context.GetKeepRef()));
    }
}


DISCREPANCY_SUMMARIZE(PERCENT_N)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


// INTERNAL_TRANSCRIBED_SPACER_RRNA

static const char* kRRNASpacer[] = { "internal", "transcribed", "spacer", "\0" };


DISCREPANCY_CASE(INTERNAL_TRANSCRIBED_SPACER_RRNA, CRNA_ref, eOncaller, "Look for rRNAs that contain either 'internal', 'transcribed' or 'spacer'")
{
    CConstRef<CSeq_feat> feat = context.GetCurrentSeq_feat();
    if (feat->GetData().GetSubtype() != CSeqFeatData::eSubtype_rRNA) {
        return;
    }

    const string rna_name = obj.GetRnaProductName();
    for (size_t i = 0; kRRNASpacer[i][0] != '\0'; ++i) {
        if (NStr::FindNoCase(rna_name, kRRNASpacer[i]) != NPOS) {
            m_Objs["[n] rRNA feature products contain 'internal', 'transcribed', or 'spacer'"].Add(
                *new CDiscrepancyObject(context.GetCurrentSeq_feat(), context.GetScope(), context.GetFile(), context.GetKeepRef()));
        }
    }
}


DISCREPANCY_SUMMARIZE(INTERNAL_TRANSCRIBED_SPACER_RRNA)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


// OVERLAPPING_CDS

static bool StrandsMatch(const CSeq_loc& loc1, const CSeq_loc& loc2)    //
{
    ENa_strand strand1 = loc1.GetStrand();
    ENa_strand strand2 = loc2.GetStrand();
    return (strand1 == eNa_strand_minus && strand2 == eNa_strand_minus) || (strand1 != eNa_strand_minus && strand2 != eNa_strand_minus);
}


static string GetProductName(const CProt_ref& prot)
{
    string prot_nm(kEmptyStr);
    if (prot.IsSetName() && prot.GetName().size() > 0) {
        prot_nm = prot.GetName().front();
    }
    return prot_nm;
}


static string GetProductName(const CSeq_feat& cds, CScope& scope)
{
    string prot_nm(kEmptyStr);
    if (cds.IsSetProduct()) {
        CBioseq_Handle prot_bsq = sequence::GetBioseqFromSeqLoc(cds.GetProduct(), scope);

        if (prot_bsq) {
            CFeat_CI prot_ci(prot_bsq, CSeqFeatData::e_Prot);
            if (prot_ci) {
                prot_nm = GetProductName(prot_ci->GetOriginalFeature().GetData().GetProt());
            }
        }
    }
    else if (cds.IsSetXref()) {
        ITERATE(CSeq_feat::TXref, it, cds.GetXref()) {
            if ((*it)->IsSetData() && (*it)->GetData().IsProt()) {
                prot_nm = GetProductName((*it)->GetData().GetProt());
            }
        }
    }
    return prot_nm;

}


static const string kSimilarProductWords[] = { "transposase", "integrase" };

static const int kNumSimilarProductWords = sizeof (kSimilarProductWords) / sizeof (string);

static const string kIgnoreSimilarProductWords[] = { "hypothetical protein", "phage", "predicted protein" };

static const int kNumIgnoreSimilarProductWords = sizeof (kIgnoreSimilarProductWords) / sizeof (string);


static bool ProductNamesAreSimilar(const string& product1, const string& product2)
{
    bool str1_has_similarity_word = false, str2_has_similarity_word = false;

    size_t i;
    // if both product names contain one of the special case similarity words, the product names are similar.
  
    for (i = 0; i < kNumSimilarProductWords; i++) {
        if (string::npos != NStr::FindNoCase(product1, kSimilarProductWords[i])) {
            str1_has_similarity_word = true;
        }

        if (string::npos != NStr::FindNoCase(product2, kSimilarProductWords[i])) {
            str2_has_similarity_word = true;
        }
    }
    if (str1_has_similarity_word && str2_has_similarity_word) {
        return true;
    }
  
    // otherwise, if one of the product names contains one of special ignore similarity
    // words, the product names are not similar.

    for (i = 0; i < kNumIgnoreSimilarProductWords; i++) {
        if (string::npos != NStr::FindNoCase(product1, kIgnoreSimilarProductWords[i]) || string::npos != NStr::FindNoCase(product2, kIgnoreSimilarProductWords[i])) {
            return false;
        }
    }

    return !NStr::CompareNocase(product1, product2);
}


static bool ShouldIgnore(const string& product)
{
    if (NStr::Find(product, "transposon") != string::npos || NStr::Find(product, "transposase") != string::npos) {
        return true;
    }
    CString_constraint constraint;
    constraint.SetMatch_text("ABC");
    constraint.SetCase_sensitive(true);
    constraint.SetWhole_word(true);
    return constraint.Match(product);
}


static bool LocationsOverlapOnSameStrand(const CSeq_loc& loc1, const CSeq_loc& loc2, CScope* scope)
{
    return StrandsMatch(loc1, loc2) &&
        sequence::Compare(loc1, loc2, scope, sequence::fCompareOverlapping) != sequence::eNoOverlap;
}


static const string kOverlappingCDSNoteText = "overlaps another CDS with the same product name";


static bool HasOverlapNote(const CSeq_feat& feat)
{
    return feat.IsSetComment() && NStr::Find(feat.GetComment(), kOverlappingCDSNoteText) != string::npos;
}


static bool SetOverlapNote(CSeq_feat& feat)
{
    if (feat.IsSetComment() && NStr::Find(feat.GetComment(), kOverlappingCDSNoteText) != string::npos) {
        return false;
    }
    AddComment(feat, (string)kOverlappingCDSNoteText);
    return true;
}


DISCREPANCY_CASE(OVERLAPPING_CDS, CSeqFeatData, eDisc, "Overlapping CDs")
{
    static const char* kOverlap0 = "[n] coding region[s] overlap[S] another coding region with a similar or identical name.";
    static const char* kOverlap1 = "[n] coding region[s] overlap[S] another coding region with a similar or identical name, but [has] the appropriate note text.";
    static const char* kOverlap2 = "[n] coding region[s] overlap[S] another coding region with a similar or identical name and [does] not have the appropriate note text.";

    if (obj.Which() != CSeqFeatData::e_Cdregion) {
        return;
    }
    if (!context.GetCurrentBioseq()->CanGetInst() || !context.GetCurrentBioseq()->GetInst().IsNa()) {
        return;
    }
    string product = GetProductName(*context.GetCurrentSeq_feat(), context.GetScope());
    if (product.empty() || ShouldIgnore(product)) {
        return;
    }
    const CSeq_loc& location = context.GetCurrentSeq_feat()->GetLocation();

    if (m_Count != context.GetCountBioseq()) {  // cleanup temporary data
        m_Count = context.GetCountBioseq();
        CReportNode tmp = m_Objs[kEmptyStr];    // keep the test results
        m_Objs.clear();
        m_Objs[kEmptyStr] = tmp;
    }

    CReportNode::TNodeMap& map = m_Objs.GetMap();
    NON_CONST_ITERATE(CReportNode::TNodeMap, it, map) {
        if (it->first.empty() || !ProductNamesAreSimilar(it->first, product)) {
            continue;
        }
        TReportObjectList& list = it->second->GetObjects();
        NON_CONST_ITERATE(TReportObjectList, robj, list) {
            CConstRef<CSeq_feat> sf((CSeq_feat*)&*(*robj)->GetObject());
            const CSeq_loc& loc = sf->GetLocation();
            if (!LocationsOverlapOnSameStrand(location, loc, &context.GetScope())) {
                continue;
            }
            bool has_note = HasOverlapNote(*sf);
            m_Objs[kEmptyStr][kOverlap0][has_note ? kOverlap1 : kOverlap2].Add(*new CDiscrepancyObject(sf, context.GetScope(), context.GetFile(), context.GetKeepRef(), !has_note));
            has_note = HasOverlapNote(*context.GetCurrentSeq_feat());
            m_Objs[kEmptyStr][kOverlap0][has_note ? kOverlap1 : kOverlap2].Add(*new CDiscrepancyObject(context.GetCurrentSeq_feat(), context.GetScope(), context.GetFile(), context.GetKeepRef(), !has_note));
        }
    }
    m_Objs[product].Add(*new CDiscrepancyObject(context.GetCurrentSeq_feat(), context.GetScope(), context.GetFile(), true));
}


DISCREPANCY_SUMMARIZE(OVERLAPPING_CDS)
{
    m_ReportItems = m_Objs[kEmptyStr].Export(*this)->GetSubitems();
}


DISCREPANCY_AUTOFIX(OVERLAPPING_CDS)
{
    TReportObjectList list = item->GetDetails();
    NON_CONST_ITERATE(TReportObjectList, it, list) {
        const CSeq_feat* sf = dynamic_cast<const CSeq_feat*>(dynamic_cast<CDiscrepancyObject*>((*it).GetNCPointer())->GetObject().GetPointer());
        if (sf) {
            CRef<CSeq_feat> new_feat(new CSeq_feat());
            new_feat->Assign(*sf);
            if (SetOverlapNote(*new_feat)) {
                CSeq_feat_EditHandle feh(scope.GetSeq_featHandle(*sf));
                feh.Replace(*new_feat);
            }
        }
    }
}

DISCREPANCY_CASE(PARTIAL_CDS_COMPLETE_SEQUENCE, CSeq_feat, eDisc, "Partial CDSs in Complete Sequences")
{
    if (obj.GetData().GetSubtype() != CSeqFeatData::eSubtype_cdregion) {
        return;
    }

    // leave if this CDS is not at least in some way marked as partial
    if( ! GET_FIELD_OR_DEFAULT(obj, Partial, false) &&
        ! (obj.IsSetLocation() && obj.GetLocation().IsPartialStart(eExtreme_Biological)) &&
        ! (obj.IsSetLocation() && obj.GetLocation().IsPartialStop(eExtreme_Biological) ))
    {
        return;
    }

    // leave if we're not on a complete sequence
    const CMolInfo * mol_info = context.GetCurrentMolInfo();
    if( ! mol_info ||
        ! FIELD_EQUALS(*mol_info, Completeness,
                       CMolInfo::eCompleteness_complete) )
    {
        return;
    }

    // record the issue
    m_Objs["[n] partial CDS[s] in complete sequence[s]"].Add(
        *new CDiscrepancyObject(
            context.GetCurrentSeq_feat(), context.GetScope(),
            context.GetFile(), context.GetKeepRef()));
}

DISCREPANCY_SUMMARIZE(PARTIAL_CDS_COMPLETE_SEQUENCE)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}

// OVERLAPPING_RRNAS

DISCREPANCY_CASE(OVERLAPPING_RRNAS, CSeq_feat_BY_BIOSEQ, eDisc, "Overlapping rRNAs")
{
    if (obj.GetData().GetSubtype() != CSeqFeatData::eSubtype_rRNA) {
        return;
    }

    // See if we have moved to the "next" Bioseq
    if (m_Count != context.GetCountBioseq()) {
        m_Count = context.GetCountBioseq();
        m_Objs["rRNAs"].clear();
    }

    // We ask to keep the reference because we do need the actual object to stick around so we can deal with them later.
    CRef<CDiscrepancyObject> this_disc_obj(new CDiscrepancyObject(CConstRef<CSeq_feat>(&obj), context.GetScope(), context.GetFile(), true));
    const CSeq_loc& this_location = obj.GetLocation();

    NON_CONST_ITERATE(TReportObjectList, robj, m_Objs["rRNAs"].GetObjects())
    {
        const CDiscrepancyObject* other_disc_obj = dynamic_cast<CDiscrepancyObject*>(robj->GetNCPointer());
        const CSeq_feat* other_seq_feat = dynamic_cast<const CSeq_feat*>(other_disc_obj->GetObject().GetPointer());
        const CSeq_loc& other_location = other_seq_feat->GetLocation();

        if (sequence::Compare(this_location, other_location, &context.GetScope(), sequence::fCompareOverlapping) != sequence::eNoOverlap) {
            // We add with unique=false because it's O(num overlaps) each time to uniquify.
            // We do it at the end instead.
            m_Objs["[n] rRNA feature[s] overlap[S] another rRNA feature."]
                .Add(**robj, false)
                .Add(*this_disc_obj, false);
        }
    }

    m_Objs["rRNAs"].Add(*this_disc_obj);
}

DISCREPANCY_SUMMARIZE(OVERLAPPING_RRNAS)
{
    m_Objs.GetMap().erase("rRNAs");

    // Make the list unique now.
    TReportObjectList& hits = m_Objs["[n] rRNA feature[s] overlap[S] another rRNA feature."].GetObjects();
    set<CRef<CReportObj> > uniq(hits.begin(), hits.end());
    hits.assign(uniq.begin(), uniq.end());
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


// CONTAINED_CDS

static bool HasContainedNote(const CSeq_feat& feat)
{
    return feat.IsSetComment() && NStr::EqualNocase(feat.GetComment(), "completely contained in another CDS");
}


static void DeleteProteinSequence(CBioseq_Handle prot)
{
    // Should be a protein!
    if ( prot && prot.IsProtein() && !prot.IsRemoved() ) {
        // Get the protein parent set before you remove the protein
        CBioseq_set_Handle bssh = prot.GetParentBioseq_set();

        // Delete the protein
        CBioseq_EditHandle prot_eh(prot);
        prot_eh.Remove();

        // If lone nuc remains, renormalize the nuc-prot set
        if (bssh && bssh.IsSetClass() 
            && bssh.GetClass() == CBioseq_set::eClass_nuc_prot
            && !bssh.IsEmptySeq_set() 
            && bssh.GetBioseq_setCore()->GetSeq_set().size() == 1) 
        {
            // Renormalize the lone nuc that's inside the nuc-prot set into  
            // a nuc bioseq.  This call will remove annots/descrs from the 
            // set and attach them to the seq.
            bssh.GetParentEntry().GetEditHandle().ConvertSetToSeq();
        }
    }
}


static bool ConvertCDSToMiscFeat(const CSeq_feat& feat, CScope& scope)
{
    if (!feat.GetData().IsCdregion() || feat.GetData().GetSubtype() == CSeqFeatData::eSubtype_misc_feature) {
        return false;
    }
    CRef<CSeq_feat> replacement(new CSeq_feat());
    replacement->Assign(feat);
    replacement->SetData().SetImp().SetKey("misc_feature");
    if (feat.IsSetProduct()) {
        string product = GetProductName(feat, scope);
        if (!NStr::IsBlank(product)) {
            AddComment(*replacement, product);
        }
    }
    try {
        CSeq_feat_EditHandle feh(scope.GetSeq_featHandle(feat));
        feh.Replace(*replacement);
    } catch (...) {
        // feature may have already been removed or converted
        return false;
    }
    return true;
}


DISCREPANCY_CASE(CONTAINED_CDS, CSeqFeatData, eDisc, "Contained CDs")
{
    static const char* kContained = "[n] coding region[s] completely contained in another coding region.";
    static const char* kContainedNote = "[n] coding region[s] completely contained in another coding region, but have note.";
    static const char* kContainedSame = "[n] coding region[s] completely contained in another coding region on the same strand.";
    static const char* kContainedOpps = "[n] coding region[s] completely contained in another coding region, but on the opposite strand.";

    if (obj.Which() != CSeqFeatData::e_Cdregion) {
        return;
    }
    if (!context.GetCurrentBioseq()->CanGetInst() || !context.GetCurrentBioseq()->GetInst().IsNa() || !context.IsEukaryotic()) {
        return;
    }

    if (m_Count != context.GetCountBioseq()) {
        m_Count = context.GetCountBioseq();
        m_Objs["tmp"].clear();
    }

    const CSeq_loc& location = context.GetCurrentSeq_feat()->GetLocation();
    TReportObjectList& list = m_Objs["tmp"].GetObjects();
    NON_CONST_ITERATE(TReportObjectList, robj, list) {
        CConstRef<CSeq_feat> sf((CSeq_feat*)&*(*robj)->GetObject());
        const CSeq_loc& loc = sf->GetLocation();
        sequence::ECompare compare = sequence::Compare(loc, location, &context.GetScope(), sequence::fCompareOverlapping);
        if (compare == sequence::eContains || compare == sequence::eSame || compare == sequence::eContained) {
            const char* strand = StrandsMatch(loc, location) ? kContainedSame : kContainedOpps;
            m_Objs[kEmptyStr][kContained][HasContainedNote(*sf) ? kContainedNote : strand].Add(*new CDiscrepancyObject(sf, context.GetScope(), context.GetFile(), context.GetKeepRef(), compare == sequence::eContains));
            m_Objs[kEmptyStr][kContained][HasContainedNote(*context.GetCurrentSeq_feat()) ? kContainedNote : strand].Add(*new CDiscrepancyObject(context.GetCurrentSeq_feat(), context.GetScope(), context.GetFile(), context.GetKeepRef(), compare == sequence::eContained));
        }
    }
    m_Objs["tmp"].Add(*new CDiscrepancyObject(context.GetCurrentSeq_feat(), context.GetScope(), context.GetFile(), true));
}


DISCREPANCY_SUMMARIZE(CONTAINED_CDS)
{
    m_ReportItems = m_Objs[kEmptyStr].Export(*this)->GetSubitems();
}


DISCREPANCY_AUTOFIX(CONTAINED_CDS)
{
    TReportObjectList list = item->GetDetails();
    NON_CONST_ITERATE(TReportObjectList, it, list) {
        if ((*it)->CanAutofix()) {
            const CSeq_feat* sf = dynamic_cast<const CSeq_feat*>(dynamic_cast<CDiscrepancyObject*>((*it).GetNCPointer())->GetObject().GetPointer());
            if (sf) {
                ConvertCDSToMiscFeat(*sf, scope);
            }
        }
    }
}


DISCREPANCY_CASE(ZERO_BASECOUNT, CSeq_inst, eDisc, "Zero Base Counts")
{
    static const char* kMsg = "[n] sequence[s] [has] a zero basecount for a nucleotide";
    if (obj.IsAa() || context.SequenceHasFarPointers()) {
        return;
    }
    const CSeqSummary& sum = context.GetNucleotideCount();
    if (!sum.A) {
        m_Objs[kMsg]["[n] sequence[s] [has] no As"].Ext().Add(*new CDiscrepancyObject(context.GetCurrentBioseq(), context.GetScope(), context.GetFile(), context.GetKeepRef()));
    }
    if (!sum.C) {
        m_Objs[kMsg]["[n] sequence[s] [has] no Cs"].Ext().Add(*new CDiscrepancyObject(context.GetCurrentBioseq(), context.GetScope(), context.GetFile(), context.GetKeepRef()));
    }
    if (!sum.G) {
        m_Objs[kMsg]["[n] sequence[s] [has] no Gs"].Ext().Add(*new CDiscrepancyObject(context.GetCurrentBioseq(), context.GetScope(), context.GetFile(), context.GetKeepRef()));
    }
    if (!sum.T) {
        m_Objs[kMsg]["[n] sequence[s] [has] no Ts"].Ext().Add(*new CDiscrepancyObject(context.GetCurrentBioseq(), context.GetScope(), context.GetFile(), context.GetKeepRef()));
    }
}


DISCREPANCY_SUMMARIZE(ZERO_BASECOUNT)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


// NONWGS_SETS_PRESENT
DISCREPANCY_CASE(NONWGS_SETS_PRESENT, CBioseq_set, eDisc,
                 "Eco, mut, phy or pop sets present")
{
    _ASSERT(&obj == &*context.GetCurrentBioseq_set());

    if( ! obj.IsSetClass() ) {
        return;
    }

    CBioseq_set::EClass bioseq_set_class = obj.GetClass();
    switch(bioseq_set_class) {
    case CBioseq_set::eClass_eco_set:
    case CBioseq_set::eClass_mut_set:
    case CBioseq_set::eClass_phy_set:
    case CBioseq_set::eClass_pop_set:
        // non-WGS set found
        m_Objs["[n] set[s] [is] of type eco, mut, phy or pop"].Add(
            *new CDiscrepancyObject(
                context.GetCurrentBioseq_set(), context.GetScope(),
                context.GetFile(), context.GetKeepRef()),
            false);
        break;
    default:
        // other types are fine
        break;
    }
}

DISCREPANCY_SUMMARIZE(NONWGS_SETS_PRESENT)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}

DISCREPANCY_CASE(NO_ANNOTATION, CSeq_inst, eDisc|eOncaller, "No annotation")
{
    if (obj.IsAa() || context.HasFeatures()) {
        return;
    }
    m_Objs["[n] bioseq[s] [has] no features"].Add(*new CDiscrepancyObject(context.GetCurrentBioseq(), context.GetScope(), context.GetFile(), context.GetKeepRef()));
}


DISCREPANCY_SUMMARIZE(NO_ANNOTATION)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


DISCREPANCY_CASE(LONG_NO_ANNOTATION, CSeq_inst, eDisc, "No annotation for LONG sequence")
{
    const int kSeqLength = 5000;
    if (obj.IsAa() || context.HasFeatures() || !(obj.CanGetLength() && obj.GetLength() > kSeqLength)) {
        return;
    }
    m_Objs["[n] bioseq[s] [is] longer than 5000nt and [has] no features"].Add(*new CDiscrepancyObject(context.GetCurrentBioseq(), context.GetScope(), context.GetFile(), context.GetKeepRef()));
}


DISCREPANCY_SUMMARIZE(LONG_NO_ANNOTATION)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


DISCREPANCY_CASE(POSSIBLE_LINKER, CSeq_inst, eOncaller, "Detect linker sequence after poly-A tail")
{
    if (obj.IsAa() ) {
        return;
    }

    CBioseq_Handle bsh = context.GetScope().GetBioseqHandle(*context.GetCurrentBioseq());
    if ( !bsh.IsSetInst_Mol()  ||
         bsh.GetInst_Mol() != CSeq_inst::eMol_rna ) {
        return;
    }

    CSeqdesc_CI desc_it (bsh, CSeqdesc::e_Molinfo);
    if ( !desc_it ) {
        return;
    }

    if ( ! (desc_it->GetMolinfo().IsSetBiomol())  || 
         (desc_it->GetMolinfo().GetBiomol() != CMolInfo::eBiomol_mRNA) ) {
        return;
    }

    if (bsh.IsSetInst_Length() && bsh.GetInst_Length() < 30) {
        // not long enough to have poly-a tail
        return;
    }
        
    CSeqVector seq_vec(*context.GetCurrentBioseq(), &context.GetScope(),
                       CBioseq_Handle::eCoding_Iupac,
                       eNa_strand_plus
                       );

    string seq_data(kEmptyStr);
    seq_vec.GetSeqData(bsh.GetInst_Length()-30, bsh.GetInst_Length(), seq_data);

    size_t tail_len = 0;
    bool found_linker = false;

    ITERATE(string, base_it , seq_data) {
        char base = toupper(*base_it);

        if (base == 'A') {
            tail_len++;
        } else if (tail_len > 20) {
            found_linker = true;
            break;
        } else {
            tail_len = 0;
        }
    }

    if (found_linker) {
        m_Objs["[n] bioseq[s] may have linker sequence after the poly-A tail"].
          Add(*new CDiscrepancyObject(context.GetCurrentBioseq(),
                                      context.GetScope(),
                                      context.GetFile(),
                                      context.GetKeepRef()));
    }
}


DISCREPANCY_SUMMARIZE(POSSIBLE_LINKER)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


DISCREPANCY_CASE(MISSING_LOCUS_TAGS, CSeqFeatData, eDisc, "Missing locus tags")
{
    if (obj.Which() != CSeqFeatData::e_Gene) {
        return;
    }

    const CGene_ref& gene_ref = obj.GetGene();

    // Skip pseudo-genes
    if (gene_ref.CanGetPseudo() && gene_ref.GetPseudo() == true) {
        return;
    }

    // Report missing or empty locus tags
    if (!gene_ref.CanGetLocus_tag() || NStr::TruncateSpaces(gene_ref.GetLocus_tag()).empty()) {
        m_Objs["[n] gene[s] [has] no locus tag[s]."].Add(*new CDiscrepancyObject(context.GetCurrentSeq_feat(), context.GetScope(), context.GetFile(), context.GetKeepRef()));
    }
}


DISCREPANCY_SUMMARIZE(MISSING_LOCUS_TAGS)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


DISCREPANCY_CASE(INCONSISTENT_LOCUS_TAG_PREFIX, CSeqFeatData, eDisc, "Inconsistent locus tag prefix")
{
    if (obj.Which() != CSeqFeatData::e_Gene) {
        return;
    }

    const CGene_ref& gene_ref = obj.GetGene();

    // Skip pseudo-genes
    if (gene_ref.CanGetPseudo() && gene_ref.GetPseudo() == true) {
        return;
    }

    // Skip missing locus tags
    if (!gene_ref.CanGetLocus_tag()) {
        return;
    }

    // Report on good locus tags - are they consistent?
    string locus_tag = gene_ref.GetLocus_tag();
    if (!locus_tag.empty() && !context.IsBadLocusTagFormat(locus_tag)) {
        // Store each unique prefix in a bin
        // If there is more than 1 bin, the prefixes are inconsistent
        string prefix;
        string tagvalue;
        NStr::SplitInTwo(locus_tag, "_", prefix, tagvalue);

        stringstream ss;
        ss << "[n] feature[s] [has] locus tag prefix " << prefix << ".";
        m_Objs[ss.str()].Add(*new CDiscrepancyObject(context.GetCurrentSeq_feat(), context.GetScope(), context.GetFile(), context.GetKeepRef()));
    }
}


DISCREPANCY_SUMMARIZE(INCONSISTENT_LOCUS_TAG_PREFIX)
{
    // If there is more than 1 bin, the prefixes are inconsistent
    if (m_Objs.GetMap().size() > 1) {
        m_ReportItems = m_Objs.Export(*this)->GetSubitems();
    }
}


static const string kInconsistent_Moltype = "[n] sequences have inconsistent moltypes";

DISCREPANCY_CASE(INCONSISTENT_MOLTYPES, CSeq_inst, eOncaller, "Inconsistent molecule types")
{
    if (obj.IsAa() ) {
        return;
    }
    // Report on nucs only

    // Initialize moltype string with MolInfo.biomol 
    string moltype;
    const CMolInfo * mol_info = context.GetCurrentMolInfo();
    if( mol_info && mol_info->CanGetBiomol() ) {
        CMolInfo::TBiomol biomol = mol_info->GetBiomol();
        moltype = CMolInfo::GetBiomolName(biomol);
    }

    // If MolInfo.biomol is empty or all spaces, use "genomic" by default
    if (NStr::IsBlank(moltype)) {
        moltype = "genomic";
    }

    // Append Seq-inst.mol to moltype
    if (context.GetCurrentBioseq()->CanGetInst() &&
        context.GetCurrentBioseq()->GetInst().CanGetMol())
    {
        CSeq_inst::TMol mol = context.GetCurrentBioseq()->GetInst().GetMol();
        moltype += string(" ") + CSeq_inst::GetMoleculeClass(mol);
    }

    // Add each nuc bioseq, regardless of moltype, to get a total count
    m_Objs[kInconsistent_Moltype].Add(*new CDiscrepancyObject(context.GetCurrentBioseq(), context.GetScope(), context.GetFile(), context.GetKeepRef()));

    // Add each unique moltype as a key to the Extended Output
    stringstream ss;
    ss << "[n] sequence[s] [has] moltype " << moltype;
    m_Objs[kInconsistent_Moltype][ss.str()].Ext().Add(*new CDiscrepancyObject(context.GetCurrentBioseq(), context.GetScope(), context.GetFile(), context.GetKeepRef()));
}


DISCREPANCY_SUMMARIZE(INCONSISTENT_MOLTYPES)
{
    // If there is more than 1 key, the moltypes are inconsistent
    if (m_Objs[kInconsistent_Moltype].GetMap().size() > 1) {
        m_ReportItems = m_Objs.Export(*this)->GetSubitems();
    }
}


DISCREPANCY_CASE(BAD_LOCUS_TAG_FORMAT, CSeqFeatData, eDisc, "Bad locus tag format")
{
    if (obj.Which() != CSeqFeatData::e_Gene) {
        return;
    }

    const CGene_ref& gene_ref = obj.GetGene();

    // Skip pseudo-genes
    if (gene_ref.CanGetPseudo() && gene_ref.GetPseudo() == true) {
        return;
    }

    // Skip missing locus tags
    if (!gene_ref.CanGetLocus_tag()) {
        return;
    }

    // Report non-empty, bad-format locus tags
    string locus_tag = gene_ref.GetLocus_tag();
    if (!locus_tag.empty() && context.IsBadLocusTagFormat(locus_tag)) {
        m_Objs["[n] locus tag[s] [is] incorrectly formatted."].Add(*new CDiscrepancyObject(context.GetCurrentSeq_feat(), context.GetScope(), context.GetFile(), context.GetKeepRef()));
    }
}


DISCREPANCY_SUMMARIZE(BAD_LOCUS_TAG_FORMAT)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}

// QUALITY_SCORES

DISCREPANCY_CASE(QUALITY_SCORES, CSeq_annot, eDisc, "Check for quality scores")
{
    if (!context.GetCurrentBioseq()->IsSetInst() || context.GetCurrentBioseq()->IsAa()) {
        return;
    }

    if (m_Count != context.GetCountBioseq()) { // stepping onto a new sequence
        m_Count = context.GetCountBioseq();
        if (obj.IsGraph()) {
            m_Objs["has qual scores"].Add(*new CDiscrepancyObject(context.GetCurrentBioseq(),
                context.GetScope(), context.GetFile(), context.GetKeepRef()));
        }
    }
}


DISCREPANCY_SUMMARIZE(QUALITY_SCORES)
{
    size_t all_na = context.GetNaSeqs().size();
    size_t quals = m_Objs["has qual scores"].GetObjects().size();
    m_Objs.clear();

    string some = NStr::NumericToString(all_na - quals);
    if (all_na == quals) {
        m_Objs["Quality scores are present on all sequences"];
    }
    else if (quals == 0) {
        m_Objs["Quality scores are missing on all sequences"];
    }
    else {
        m_Objs["Quality scores are missing on some(" + some + ") sequences"];
    }
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}

END_SCOPE(NDiscrepancy)
END_NCBI_SCOPE
