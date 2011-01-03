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
* Authors:  Paul Thiessen
*
* File Description:
*      dialogs for annotating CDD's
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbireg.hpp>

#include <objects/cdd/Align_annot.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seqloc/Packed_seqint.hpp>
#include <objects/pub/Pub.hpp>
#include <objects/biblio/PubMedId.hpp>
#include <objects/mmdb1/Biostruc_annot_set.hpp>
#include <objects/mmdb1/Biostruc_descr.hpp>
#include <objects/mmdb1/Biostruc_id.hpp>
#include <objects/mmdb1/Mmdb_id.hpp>
#include <objects/mmdb3/Biostruc_feature_set.hpp>
#include <objects/mmdb3/Biostruc_feature_set_descr.hpp>
#include <objects/mmdb3/Biostruc_feature.hpp>
#include <objects/mmdb3/Chem_graph_pntrs.hpp>
#include <objects/mmdb3/Residue_pntrs.hpp>
#include <objects/mmdb3/Residue_interval_pntr.hpp>
#include <objects/mmdb1/Molecule_id.hpp>
#include <objects/mmdb1/Residue_id.hpp>
#include <objects/cn3d/Cn3d_backbone_type.hpp>
#include <objects/cn3d/Cn3d_backbone_style.hpp>
#include <objects/cn3d/Cn3d_general_style.hpp>
#include <objects/cn3d/Cn3d_style_settings.hpp>

#include "remove_header_conflicts.hpp"

#include "cdd_annot_dialog.hpp"
#include "structure_set.hpp"
#include "messenger.hpp"
#include "alignment_manager.hpp"
#include "block_multiple_alignment.hpp"
#include "sequence_set.hpp"
#include "cn3d_tools.hpp"
#include "chemical_graph.hpp"
#include "molecule_identifier.hpp"
#include "opengl_renderer.hpp"
#include "show_hide_manager.hpp"


////////////////////////////////////////////////////////////////////////////////////////////////
// The following is taken unmodified from wxDesigner's C++ header from cdd_annot_dialog.wdr
////////////////////////////////////////////////////////////////////////////////////////////////

#include <wx/image.h>
#include <wx/statline.h>
#include <wx/spinbutt.h>
#include <wx/spinctrl.h>
#include <wx/splitter.h>
#include <wx/listctrl.h>
#include <wx/treectrl.h>
#include <wx/notebook.h>

// Declare window functions

#define ID_L_ANNOT 10000
#define ID_B_NEW_ANNOT 10001
#define ID_B_HIGHLIGHT 10002
#define ID_B_ANNOT_UP 10003
#define ID_B_EDIT_ANNOT 10004
#define ID_B_DEL_ANNOT 10005
#define ID_B_ANNOT_DOWN 10006
#define ID_L_EVID 10007
#define ID_B_NEW_EVID 10008
#define ID_B_SHOW 10009
#define ID_B_EVID_UP 10010
#define ID_B_EDIT_EVID 10011
#define ID_B_DEL_EVID 10012
#define ID_B_EVID_DOWN 10013
wxSizer *SetupCDDAnnotDialog( wxWindow *parent, bool call_fit = TRUE, bool set_sizer = TRUE );

#define ID_R_COMMENT 10014
#define ID_ST_COMMENT 10015
#define ID_T_COMMENT 10016
#define ID_LINE 10017
#define ID_R_PMID 10018
#define ID_ST_PMID 10019
#define ID_T_PMID 10020
#define ID_R_STRUCTURE 10021
#define ID_ST_STRUCTURE 10022
#define ID_T_STRUCTURE 10023
#define ID_B_RERANGE 10024
#define ID_B_EDIT_OK 10025
#define ID_B_EDIT_CANCEL 10026
wxSizer *SetupEvidenceDialog( wxWindow *parent, bool call_fit = TRUE, bool set_sizer = TRUE );

#define ID_ST_TYPE 10027
#define ID_C_TYPE 10028
#define ID_CB_PUTATIVE 10029
#define ID_ST_DESCR 10030
#define ID_T_DESCR 10031
#define ID_ST_DESCR2 10032
#define ID_CMB_DESCR 10033
#define ID_B_DESCR_OK 10034
#define ID_B_DESCR_CANCEL 10035
wxSizer *SetupTypedDescriptionDialog( wxWindow *parent, bool call_fit = TRUE, bool set_sizer = TRUE );

////////////////////////////////////////////////////////////////////////////////////////////////

USING_NCBI_SCOPE;
USING_SCOPE(objects);


BEGIN_SCOPE(Cn3D)

// a "structure evidence" biostruc-annot-set has these qualities - basically, a specific
// comment tag as the first entry in the set's description, and a 'name' descr in the feature set
static const string STRUCTURE_EVIDENCE_COMMENT = "Used as Structure Evidence for CDD Annotation";
#define IS_STRUCTURE_EVIDENCE_BSANNOT(evidence) \
    ((evidence).IsBsannot() && \
     (evidence).GetBsannot().IsSetDescr() && \
     (evidence).GetBsannot().GetDescr().size() > 0 && \
     (evidence).GetBsannot().GetDescr().front()->IsOther_comment() && \
     (evidence).GetBsannot().GetDescr().front()->GetOther_comment() == STRUCTURE_EVIDENCE_COMMENT && \
     (evidence).GetBsannot().GetFeatures().size() > 0 && \
     (evidence).GetBsannot().GetFeatures().front()->IsSetDescr() && \
     (evidence).GetBsannot().GetFeatures().front()->GetDescr().size() > 0 && \
     (evidence).GetBsannot().GetFeatures().front()->GetDescr().front()->IsName())


#define DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(var, id, type) \
    type *var; \
    var = wxDynamicCast(FindWindow(id), type); \
    if (!var) { \
        ERRORMSG("Can't find window with id " << id); \
        return; \
    }

#define DECLARE_AND_FIND_WINDOW_RETURN_FALSE_ON_ERR(var, id, type) \
    type *var; \
    var = wxDynamicCast(FindWindow(id), type); \
    if (!var) { \
        ERRORMSG("Can't find window with id " << id); \
        return false; \
    }

BEGIN_EVENT_TABLE(CDDAnnotateDialog, wxDialog)
    EVT_BUTTON      (-1,    CDDAnnotateDialog::OnButton)
    EVT_LISTBOX     (-1,    CDDAnnotateDialog::OnSelection)
    EVT_CLOSE       (       CDDAnnotateDialog::OnCloseWindow)
END_EVENT_TABLE()

CDDAnnotateDialog::CDDAnnotateDialog(wxWindow *parent, CDDAnnotateDialog **handle, StructureSet *set) :
    wxDialog(parent, -1, "CDD Annotations", wxPoint(400, 100), wxDefaultSize, wxDEFAULT_DIALOG_STYLE),
    dialogHandle(handle), structureSet(set), annotSet(set->GetCDDAnnotSet())
{
    if (annotSet.Empty()) {
        Destroy();
        return;
    }

    // construct the panel
    wxSizer *topSizer = SetupCDDAnnotDialog(this, false);

    // call sizer stuff
    topSizer->Fit(this);
    topSizer->SetSizeHints(this);
    SetClientSize(topSizer->GetMinSize());

    // set initial GUI state
    SetupGUIControls(0, 0);
}

CDDAnnotateDialog::~CDDAnnotateDialog(void)
{
    // so owner knows that this dialog has been destroyed
    if (dialogHandle && *dialogHandle) *dialogHandle = NULL;
    TRACEMSG("destroyed CDDAnnotateDialog");
}

void CDDAnnotateDialog::OnCloseWindow(wxCloseEvent& event)
{
    Destroy();
}

static bool IsFirstResidueOfABlock(const BlockMultipleAlignment::ConstBlockList& blocks, unsigned int masterIndex)
{
    BlockMultipleAlignment::ConstBlockList::const_iterator b, be = blocks.end();
    for (b=blocks.begin(); b!=be; ++b) {
        int from = (*b)->GetRangeOfRow(0)->from;
        if (from == (int) masterIndex)
            return true;
        else if (from > (int) masterIndex)
            return false;
    }
    return false;
}

void CDDAnnotateDialog::GetCurrentHighlightedIntervals(IntervalList *intervals)
{
    const BlockMultipleAlignment *alignment = structureSet->alignmentManager->GetCurrentMultipleAlignment();
    const Sequence *master = alignment->GetMaster();
    BlockMultipleAlignment::ConstBlockList blocks;
    alignment->GetBlocks(&blocks);

    // find intervals of aligned residues of the master sequence that are currently highlighted
    intervals->clear();
    unsigned int first = 0, last = 0;
    while (first < master->Length()) {

        // find first highlighted residue
        while (first < master->Length() &&
               !(GlobalMessenger()->IsHighlighted(master, first) &&
                 alignment->IsAligned(0U, first))) ++first;
        if (first >= master->Length()) break;

        // find last in contiguous stretch of highlighted residues, but not crossing block boundaries
        last = first;
        while (last + 1 < master->Length() &&
               GlobalMessenger()->IsHighlighted(master, last + 1) &&
               alignment->IsAligned(0U, last + 1) &&
               !IsFirstResidueOfABlock(blocks, last + 1)) ++last;

        // create Seq-interval
        CRef < CSeq_interval > interval(new CSeq_interval());
        interval->SetFrom(first);
        interval->SetTo(last);
        master->FillOutSeqId(&(interval->SetId()));
        intervals->push_back(interval);
        first = last + 1;
    }
}

void CDDAnnotateDialog::OnButton(wxCommandEvent& event)
{
    switch (event.GetId()) {
        // annotation buttons
        case ID_B_NEW_ANNOT:
            NewAnnotation();
            break;
        case ID_B_DEL_ANNOT:
            DeleteAnnotation();
            break;
        case ID_B_EDIT_ANNOT:
            EditAnnotation();
            break;
        case ID_B_HIGHLIGHT:
            HighlightAnnotation();
            break;
        case ID_B_ANNOT_UP: case ID_B_ANNOT_DOWN:
            MoveAnnotation(event.GetId() == ID_B_ANNOT_UP);
            break;

        // evidence buttons
        case ID_B_NEW_EVID:
            NewEvidence();
            break;
        case ID_B_DEL_EVID:
            DeleteEvidence();
            break;
        case ID_B_EDIT_EVID:
            EditEvidence();
            break;
        case ID_B_SHOW:
            ShowEvidence();
            break;
        case ID_B_EVID_UP: case ID_B_EVID_DOWN:
            MoveEvidence(event.GetId() == ID_B_EVID_UP);
            break;

        default:
            event.Skip();
    }
}

void CDDAnnotateDialog::OnSelection(wxCommandEvent& event)
{
    DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(annots, ID_L_ANNOT, wxListBox)
    DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(evids, ID_L_EVID, wxListBox)

    if (event.GetEventObject() == annots)
        SetupGUIControls(annots->GetSelection(), 0);
    else
        SetupGUIControls(annots->GetSelection(), evids->GetSelection());
}

void CDDAnnotateDialog::SetupGUIControls(int selectAnnot, int selectEvidence)
{
    // get GUI control pointers
    DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(annots, ID_L_ANNOT, wxListBox)
    DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(evids, ID_L_EVID, wxListBox)
    DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(bNewAnnot, ID_B_NEW_ANNOT, wxButton)
    DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(bDelAnnot, ID_B_DEL_ANNOT, wxButton)
    DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(bEditAnnot, ID_B_EDIT_ANNOT, wxButton)
    DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(bHighlight, ID_B_HIGHLIGHT, wxButton)
    DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(bAnnotUp, ID_B_ANNOT_UP, wxButton)
    DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(bAnnotDown, ID_B_ANNOT_DOWN, wxButton)
    DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(bNewEvid, ID_B_NEW_EVID, wxButton)
    DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(bDelEvid, ID_B_DEL_EVID, wxButton)
    DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(bEditEvid, ID_B_EDIT_EVID, wxButton)
    DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(bShow, ID_B_SHOW, wxButton)
    DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(bEvidUp, ID_B_EVID_UP, wxButton)
    DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(bEvidDown, ID_B_EVID_DOWN, wxButton)

    bool readOnly;
    RegistryGetBoolean(REG_ADVANCED_SECTION, REG_CDD_ANNOT_READONLY, &readOnly);

    // fill out annots listbox
    int pos = annots->GetScrollPos(wxVERTICAL);
    annots->Clear();
    CAlign_annot *selectedAnnot = NULL;
    CAlign_annot_set::Tdata::iterator a, ae = annotSet->Set().end();
    for (a=annotSet->Set().begin(); a!=ae; ++a) {
        if ((*a)->IsSetDescription())
            annots->Append((*a)->GetDescription().c_str(), a->GetPointer());
        else
            annots->Append("(no description)", a->GetPointer());
    }
    if (selectAnnot < (int) annots->GetCount())
        annots->SetSelection(selectAnnot);
    else if (annots->GetCount() > 0)
        annots->SetSelection(0);
    if (annots->GetCount() > 0)
        selectedAnnot = reinterpret_cast<CAlign_annot*>(annots->GetClientData(annots->GetSelection()));
    annots->SetFirstItem(pos);

    // fill out evidence listbox
    pos = evids->GetScrollPos(wxVERTICAL);
    evids->Clear();
    CFeature_evidence *selectedEvid = NULL;
    if (selectedAnnot && selectedAnnot->IsSetEvidence()) {
        CAlign_annot::TEvidence::iterator e, ee = selectedAnnot->SetEvidence().end();
        for (e=selectedAnnot->SetEvidence().begin(); e!=ee; ++e) {
            // get evidence "title" depending on type
            wxString evidTitle;
            if ((*e)->IsComment())
                evidTitle.Printf("Comment: %s", (*e)->GetComment().c_str());
            else if ((*e)->IsReference() && (*e)->GetReference().IsPmid())
                evidTitle.Printf("PMID: %i", (*e)->GetReference().GetPmid().Get());
            else if (IS_STRUCTURE_EVIDENCE_BSANNOT(**e))
                evidTitle.Printf("Structure: %s",
                    (*e)->GetBsannot().GetFeatures().front()->GetDescr().front()->GetName().c_str());
            else
                evidTitle = "(unknown type)";
            evids->Append(evidTitle, e->GetPointer());
        }
        if (selectEvidence < (int) evids->GetCount())
            evids->SetSelection(selectEvidence);
        else if (evids->GetCount() > 0)
            evids->SetSelection(0);
        if (evids->GetCount() > 0)
            selectedEvid = reinterpret_cast<CFeature_evidence*>(evids->GetClientData(evids->GetSelection()));
        evids->SetFirstItem(pos);
    }

    // set button states
    bNewAnnot->Enable(!readOnly);
    bDelAnnot->Enable(selectedAnnot != NULL && !readOnly);
    bEditAnnot->Enable(selectedAnnot != NULL && !readOnly);
    bHighlight->Enable(selectedAnnot != NULL);
    bAnnotUp->Enable(annots->GetSelection() > 0 && !readOnly);
    bAnnotDown->Enable(annots->GetSelection() < ((int) annots->GetCount()) - 1 && !readOnly);
    bNewEvid->Enable(selectedAnnot != NULL && !readOnly);
    bDelEvid->Enable(selectedEvid != NULL && !readOnly);
    bEditEvid->Enable(selectedEvid != NULL && !readOnly);
    bShow->Enable(selectedEvid != NULL &&
        ((selectedEvid->IsReference() && selectedEvid->GetReference().IsPmid()) ||
         IS_STRUCTURE_EVIDENCE_BSANNOT(*selectedEvid) || selectedEvid->IsComment()));
    bEvidUp->Enable(evids->GetSelection() > 0 && !readOnly);
    bEvidDown->Enable(evids->GetSelection() < ((int) evids->GetCount()) - 1 && !readOnly);
}

void CDDAnnotateDialog::NewAnnotation(void)
{
    IntervalList intervals;
    GetCurrentHighlightedIntervals(&intervals);
    if (intervals.size() == 0) {
        ERRORMSG("No aligned+highlighted master residues!");
        return;
    }

    CRef < CAlign_annot > annot(new CAlign_annot());
    CDDTypedAnnotDialog dialog(this, *annot, "New Annotation Description");
    int result = dialog.ShowModal();
    if (result == wxCANCEL) return;

    bool getDataResult = dialog.GetData(annot.GetPointer());

    //  If there's no description, don't persist the annotation.
    string descr = (annot->IsSetDescription()) ? annot->GetDescription() : kEmptyStr;
    if (!getDataResult || descr.size() == 0) return;

    // fill out location
    if (intervals.size() == 1) {
        annot->SetLocation().SetInt(*(intervals.front()));
    } else {
        CPacked_seqint *packed = new CPacked_seqint();
        packed->Set() = intervals;  // copy list
        annot->SetLocation().SetPacked_int(*packed);
    }

    // add to annotation list
    annotSet->Set().push_back(annot);
    structureSet->SetDataChanged(StructureSet::eUserAnnotationData);

    // update GUI
    SetupGUIControls(annotSet->Get().size() - 1, 0);
}

void CDDAnnotateDialog::DeleteAnnotation(void)
{
    // get selection
    DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(annots, ID_L_ANNOT, wxListBox)
    if (annots->GetCount() == 0 || annots->GetSelection() < 0) return;
    CAlign_annot *selectedAnnot =
        reinterpret_cast<CAlign_annot*>(annots->GetClientData(annots->GetSelection()));
    if (!selectedAnnot) {
        ERRORMSG("CDDAnnotateDialog::DeleteAnnotation() - error getting annotation pointer");
        return;
    }

    // confirm with user
    int confirm = wxMessageBox("This will remove the selected annotation and all the\n"
        "evidence associated with it. Is this correct?", "Confirm", wxOK | wxCANCEL | wxCENTRE, this);
    if (confirm != wxOK) return;

    // actually delete the annotation
    CAlign_annot_set::Tdata::iterator a, ae = annotSet->Set().end();
    for (a=annotSet->Set().begin(); a!=ae; ++a) {
        if (*a == selectedAnnot) {
            annotSet->Set().erase(a);
            structureSet->SetDataChanged(StructureSet::eUserAnnotationData);
            break;
        }
    }

    // update GUI
    SetupGUIControls(0, 0);
}

void CDDAnnotateDialog::EditAnnotation(void)
{
    DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(annots, ID_L_ANNOT, wxListBox)
    DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(evids, ID_L_EVID, wxListBox)

    // get selection
    if (annots->GetCount() == 0 || annots->GetSelection() < 0) return;
    CAlign_annot *selectedAnnot =
        reinterpret_cast<CAlign_annot*>(annots->GetClientData(annots->GetSelection()));
    if (!selectedAnnot) {
        ERRORMSG("CDDAnnotateDialog::EditAnnotation() - error getting annotation pointer");
        return;
    }

    IntervalList intervals;
    GetCurrentHighlightedIntervals(&intervals);
    if (intervals.size() > 0) {
        // move the annotation?
        int move = wxMessageBox("Do you want to move the annotation to the currently\n"
            "highlighted and aligned residues?", "Move?", wxYES_NO | wxCANCEL | wxCENTRE, this);
        if (move == wxCANCEL)
            return;
        else if (move == wxYES) {
            // change location
            if (intervals.size() == 1) {
                selectedAnnot->SetLocation().SetInt(*(intervals.front()));
            } else {
                CPacked_seqint *packed = new CPacked_seqint();
                packed->Set() = intervals;  // copy list
                selectedAnnot->SetLocation().SetPacked_int(*packed);
            }
            structureSet->SetDataChanged(StructureSet::eUserAnnotationData);
        }
    }

    // edit type or description
    int initialType = wxNOT_FOUND, finalType = wxNOT_FOUND;
    string initialDescr = kEmptyStr, finalDescr = kEmptyStr;
    if (selectedAnnot->IsSetType()) initialType = selectedAnnot->GetType();
    if (selectedAnnot->IsSetDescription()) initialDescr = selectedAnnot->GetDescription();

    CDDTypedAnnotDialog dialog(this, *selectedAnnot, "Edit Annotation Description");
    int result = dialog.ShowModal();
    if (result == wxOK) {
        bool getDataResult = dialog.GetData(selectedAnnot);

        if (selectedAnnot->IsSetType()) finalType = selectedAnnot->GetType();
        if (selectedAnnot->IsSetDescription()) finalDescr = selectedAnnot->GetDescription();
        
        if (finalDescr == kEmptyStr) {
            if (initialType == wxNOT_FOUND) {
                selectedAnnot->ResetType();
            } else {
                selectedAnnot->SetType(initialType);
            }
            selectedAnnot->SetDescription(initialDescr);
            ERRORMSG("CDDAnnotateDialog::EditAnnotation() - edited annotation must have a non-empty description\nAnnotation is unchanged.");
            return;
        }

        if (getDataResult && ((initialType != finalType) || (finalDescr != initialDescr))) {
            structureSet->SetDataChanged(StructureSet::eUserAnnotationData);
        }
    }

    // update GUI
    SetupGUIControls(annots->GetSelection(), evids->GetSelection());
}

bool CDDAnnotateDialog::HighlightInterval(const ncbi::objects::CSeq_interval& interval)
{
    const BlockMultipleAlignment *alignment = structureSet->alignmentManager->GetCurrentMultipleAlignment();
    const Sequence *master = alignment->GetMaster();

    // make sure annotation sequence matches master sequence
    if (!master->identifier->MatchesSeqId(interval.GetId())) {
        ERRORMSG("CDDAnnotateDialog::HighlightInterval() - interval Seq-id/master sequence mismatch");
        return false;
    }

    // do the highlighting
    return alignment->HighlightAlignedColumnsOfMasterRange(interval.GetFrom(), interval.GetTo());
}

void CDDAnnotateDialog::HighlightAnnotation(void)
{
    DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(annots, ID_L_ANNOT, wxListBox)

    // get selection
    if (annots->GetCount() == 0 || annots->GetSelection() < 0) return;
    CAlign_annot *selectedAnnot =
        reinterpret_cast<CAlign_annot*>(annots->GetClientData(annots->GetSelection()));
    if (!selectedAnnot) {
        ERRORMSG("CDDAnnotateDialog::HighlightAnnotation() - error getting annotation pointer");
        return;
    }

    // highlight annotation's intervals
    GlobalMessenger()->RemoveAllHighlights(true);
    bool okay = true;
    if (selectedAnnot->GetLocation().IsInt()) {
        okay = HighlightInterval(selectedAnnot->GetLocation().GetInt());
    } else if (selectedAnnot->GetLocation().IsPacked_int()) {
        CPacked_seqint::Tdata::iterator s,
            se = selectedAnnot->SetLocation().SetPacked_int().Set().end();
        for (s=selectedAnnot->SetLocation().SetPacked_int().Set().begin(); s!=se; ++s) {
            if (!HighlightInterval(**s))
                okay = false;
        }
    }
    if (!okay)
        wxMessageBox("WARNING: this annotation specifies master residues outside the aligned blocks;"
            " see the message log for details.", "Annotation Error",
            wxOK | wxCENTRE | wxICON_ERROR, this);
}

void CDDAnnotateDialog::MoveAnnotation(bool moveUp)
{
    // get selection
    DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(annots, ID_L_ANNOT, wxListBox)
    DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(evids, ID_L_EVID, wxListBox)
    if (annots->GetCount() == 0 || annots->GetSelection() < 0) return;
    CAlign_annot *selectedAnnot =
        reinterpret_cast<CAlign_annot*>(annots->GetClientData(annots->GetSelection()));
    if (!selectedAnnot) {
        ERRORMSG("CDDAnnotateDialog::MoveAnnotation() - error getting annotation pointer");
        return;
    }

    CAlign_annot_set::Tdata::iterator a, ae = annotSet->Set().end(), aPrev = ae, aSwap = ae;
    CRef < CAlign_annot > tmp;
    for (a=annotSet->Set().begin(); a!=ae; ++a) {

        if (*a == selectedAnnot) {
            // figure out which (prev or next) annot field to swap with
            if (moveUp && aPrev != ae)
                aSwap = aPrev;
            else if (!moveUp && (++(aSwap = a)) != ae)
                ;
            else
                return;

            // do the swap and update GUI
            tmp = *aSwap;
            *aSwap = *a;
            *a = tmp;
            structureSet->SetDataChanged(StructureSet::eUserAnnotationData);
            SetupGUIControls(annots->GetSelection() + (moveUp ? -1 : 1), evids->GetSelection());
            return;
        }

        aPrev = a;
    }

    ERRORMSG("CDDAnnotateDialog::MoveAnnotation() - error finding selected annotation");
}

void CDDAnnotateDialog::NewEvidence(void)
{
    DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(annots, ID_L_ANNOT, wxListBox)

    // get selected annotation
    if (annots->GetCount() == 0 || annots->GetSelection() < 0) return;
    CAlign_annot *selectedAnnot =
        reinterpret_cast<CAlign_annot*>(annots->GetClientData(annots->GetSelection()));
    if (!selectedAnnot) {
        ERRORMSG("CDDAnnotateDialog::NewEvidence() - error getting annotation pointer");
        return;
    }

    // create default comment evidence
    CRef < CFeature_evidence > newEvidence(new CFeature_evidence());
    newEvidence->SetComment("");

    // bring up evidence editor
    CDDEvidenceDialog dialog(this, *newEvidence);
    int result = dialog.ShowModal();

    // add new evidence
    if (result == wxOK) {
        if (dialog.GetData(newEvidence.GetPointer())) {
            selectedAnnot->SetEvidence().push_back(newEvidence);
            SetupGUIControls(annots->GetSelection(), selectedAnnot->GetEvidence().size() - 1);
            structureSet->SetDataChanged(StructureSet::eUserAnnotationData);
        } else
            ERRORMSG("CDDAnnotateDialog::NewEvidence() - error getting dialog data");
    }
}

void CDDAnnotateDialog::DeleteEvidence(void)
{
    // get selected annotation
    DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(annots, ID_L_ANNOT, wxListBox)
    if (annots->GetCount() == 0 || annots->GetSelection() < 0) return;
    CAlign_annot *selectedAnnot =
        reinterpret_cast<CAlign_annot*>(annots->GetClientData(annots->GetSelection()));
    if (!selectedAnnot) {
        ERRORMSG("CDDAnnotateDialog::DeleteEvidence() - error getting annotation pointer");
        return;
    }

    // get selected evidence
    DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(evids, ID_L_EVID, wxListBox)
    if (evids->GetCount() == 0 || evids->GetSelection() < 0) return;
    CFeature_evidence *selectedEvidence =
        reinterpret_cast<CFeature_evidence*>(evids->GetClientData(evids->GetSelection()));
    if (!selectedEvidence) {
        ERRORMSG("CDDAnnotateDialog::DeleteEvidence() - error getting evidence pointer");
        return;
    }

    // confirm with user
    int confirm = wxMessageBox("This will remove the selected evidence from\n"
        "the selected annotation. Is this correct?", "Confirm", wxOK | wxCANCEL | wxCENTRE, this);
    if (confirm != wxOK) return;

    // delete evidence from annotation's list
    CAlign_annot::TEvidence::iterator e, ee = selectedAnnot->SetEvidence().end();
    for (e=selectedAnnot->SetEvidence().begin(); e!=ee; ++e) {
        if (*e == selectedEvidence) {
            selectedAnnot->SetEvidence().erase(e);
            structureSet->SetDataChanged(StructureSet::eUserAnnotationData);
            break;
        }
    }
    if (e == ee) {
        ERRORMSG("CDDAnnotateDialog::DeleteEvidence() - evidence pointer not found in annotation");
        return;
    }

    // update GUI
    SetupGUIControls(annots->GetSelection(), 0);
}

void CDDAnnotateDialog::EditEvidence(void)
{
    // get selected evidence
    DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(evids, ID_L_EVID, wxListBox)
    if (evids->GetCount() == 0 || evids->GetSelection() < 0) return;
    CFeature_evidence *selectedEvidence =
        reinterpret_cast<CFeature_evidence*>(evids->GetClientData(evids->GetSelection()));
    if (!selectedEvidence) {
        ERRORMSG("CDDAnnotateDialog::DeleteEvidence() - error getting evidence pointer");
        return;
    }

    // bring up evidence editor
    CDDEvidenceDialog dialog(this, *selectedEvidence);
    int result = dialog.ShowModal();

    // update evidence
    if (result == wxOK && dialog.HasDataChanged()) {
        if (dialog.GetData(selectedEvidence)) {
            DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(annots, ID_L_ANNOT, wxListBox)
            structureSet->SetDataChanged(StructureSet::eUserAnnotationData);
            SetupGUIControls(annots->GetSelection(), evids->GetSelection());
        } else
            ERRORMSG("CDDAnnotateDialog::EditEvidence() - error getting dialog data");
    }
}

typedef struct {
    int alignedMoleculeID;
    int from, to;
    int hits;
} ChainInfo;

static const StructureObject * HighlightResidues(const StructureSet *set, const CBiostruc_annot_set& annot)
{
    try {
        if (!annot.IsSetId() || annot.GetId().size() == 0 || !annot.GetId().front()->IsMmdb_id())
            throw "no MMDB ID found in annotation";
        int mmdbID = annot.GetId().front()->GetMmdb_id().Get();

        // map object to aligned molecule ID and interval of alignment on that chain
        typedef map < const StructureObject * , ChainInfo > ObjectMap;
        ObjectMap annotObjects;

        // first find all objects with the annotation's mmdbID; fill out chain id and interval
        const BlockMultipleAlignment *alignment = set->alignmentManager->GetCurrentMultipleAlignment();
        if (!alignment) throw "no alignment";
        BlockMultipleAlignment::UngappedAlignedBlockList alignedBlocks;
        alignment->GetUngappedAlignedBlocks(&alignedBlocks);
        if (alignedBlocks.size() == 0) throw "no aligned blocks";
        for (unsigned int row=0; row<alignment->NRows(); ++row) {
            const StructureObject *object;
            const Sequence *seq = alignment->GetSequenceOfRow(row);
            if (!seq->molecule || seq->molecule->identifier->mmdbID != mmdbID ||
                !seq->molecule->GetParentOfType(&object))
                continue;
            ChainInfo& ci = annotObjects[object];
            ci.alignedMoleculeID = seq->molecule->id;
            ci.from = alignedBlocks.front()->GetRangeOfRow(row)->from;
            ci.to = alignedBlocks.back()->GetRangeOfRow(row)->to;
            ci.hits = 0;
        }
        if (annotObjects.size() == 0)
            throw "no chain of annotation's MMDB ID in the alignment";

        GlobalMessenger()->RemoveAllHighlights(true);
        CCn3d_style_settings globalStyleSettings;
        set->styleManager->GetGlobalStyle().SaveSettingsToASN(&globalStyleSettings);

        // iterate over molecule/residue intervals
        if (annot.GetFeatures().size() > 0 &&
            annot.GetFeatures().front()->GetFeatures().size() > 0 &&
            annot.GetFeatures().front()->GetFeatures().front()->IsSetLocation() &&
            annot.GetFeatures().front()->GetFeatures().front()->GetLocation().IsSubgraph() &&
            annot.GetFeatures().front()->GetFeatures().front()->GetLocation().
                GetSubgraph().IsResidues() &&
            annot.GetFeatures().front()->GetFeatures().front()->GetLocation().
                GetSubgraph().GetResidues().IsInterval() &&
            annot.GetFeatures().front()->GetFeatures().front()->GetLocation().
                GetSubgraph().GetResidues().GetInterval().size() > 0)
        {

            ObjectMap::iterator o, oe = annotObjects.end();
            CResidue_pntrs::TInterval::const_iterator i, ie =
                annot.GetFeatures().front()->GetFeatures().front()->GetLocation().
                    GetSubgraph().GetResidues().GetInterval().end();
            for (i=annot.GetFeatures().front()->GetFeatures().front()->GetLocation().GetSubgraph().GetResidues().GetInterval().begin(); i!=ie; ++i)
            {
                // for each object
                for (o=annotObjects.begin(); o!=oe; ++o) {

                    // find molecule with annotation's moleculeID
                    ChemicalGraph::MoleculeMap::const_iterator
                        m = o->first->graph->molecules.find((*i)->GetMolecule_id().Get());
                    if (m == o->first->graph->molecules.end())
                        throw "molecule with annotation's specified molecule ID not found in object";

                    // make sure this stuff is visible in global style
                    if (m->second->IsProtein()) {
                        if (globalStyleSettings.GetProtein_backbone().GetType() == eCn3d_backbone_type_off &&
                            !globalStyleSettings.GetProtein_sidechains().GetIs_on())
                        {
                            globalStyleSettings.SetProtein_backbone().SetType(eCn3d_backbone_type_trace);
                        }
                    } else if (m->second->IsNucleotide()) {
                        if (globalStyleSettings.GetNucleotide_backbone().GetType() == eCn3d_backbone_type_off &&
                            !globalStyleSettings.GetNucleotide_sidechains().GetIs_on())
                        {
                            globalStyleSettings.SetNucleotide_backbone().SetType(eCn3d_backbone_type_trace);
                        }
                    } else if (m->second->IsSolvent()) {
                        globalStyleSettings.SetSolvents().SetIs_on(true);
                    } else if (m->second->IsHeterogen()) {
                        globalStyleSettings.SetHeterogens().SetIs_on(true);
                    }

                    // highlight residues in interval
                    if (o == annotObjects.begin()) {
                        if ((*i)->GetFrom().Get() >= 1 && (*i)->GetFrom().Get() <= (int)m->second->NResidues() &&
                                (*i)->GetTo().Get() >= 1 && (*i)->GetTo().Get() <= (int)m->second->NResidues() &&
                                (*i)->GetFrom().Get() <= (*i)->GetTo().Get())
                            GlobalMessenger()->AddHighlights(m->second, (*i)->GetFrom().Get(), (*i)->GetTo().Get());
                        else
                            throw "annotation's residue ID out of molecule's residue range";
                    }

                    for (int r=(*i)->GetFrom().Get(); r<=(*i)->GetTo().Get(); ++r) {
                        // count hits of annotated residues in aligned chain+interval
                        if (o->second.alignedMoleculeID == m->second->id && r >= o->second.from && r <= o->second.to)
                            ++(o->second.hits);
                    }
                }
            }

            set->styleManager->SetGlobalStyle(globalStyleSettings);

            // return object with most hits of annotation to aligned chain region
            const StructureObject *bestObject = NULL;
            for (o=annotObjects.begin(); o!=oe; ++o)
                if (!bestObject || o->second.hits > annotObjects[bestObject].hits)
                    bestObject = o->first;
            return bestObject;

        } else
            throw "unrecognized annotation structure";

    } catch (const char *err) {
        ERRORMSG("HighlightResidues() - " << err);
    }
    return NULL;
}

void CDDAnnotateDialog::ShowEvidence(void)
{
    // get selected evidence
    DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(evids, ID_L_EVID, wxListBox)
    if (evids->GetCount() == 0 || evids->GetSelection() < 0) return;
    CFeature_evidence *selectedEvidence =
        reinterpret_cast<CFeature_evidence*>(evids->GetClientData(evids->GetSelection()));
    if (!selectedEvidence) {
        ERRORMSG("CDDAnnotateDialog::ShowEvidence() - error getting evidence pointer");
        return;
    }

    // launch URL given PMID
    if (selectedEvidence->IsReference() && selectedEvidence->GetReference().IsPmid()) {
        wxString url;
        url.Printf("http://www.ncbi.nlm.nih.gov/entrez/query.fcgi?"
            "cmd=Retrieve&db=PubMed&dopt=Abstract&list_uids=%i",
            selectedEvidence->GetReference().GetPmid().Get());
        LaunchWebPage(url);
    }

    // highlight residues if structure evidence
    else if (IS_STRUCTURE_EVIDENCE_BSANNOT(*selectedEvidence)) {
        const StructureObject *bestObject = HighlightResidues(structureSet, selectedEvidence->GetBsannot());
        if (bestObject) {

            // first, set show/hide to make only objects with this mmdb id visible
            structureSet->showHideManager->MakeAllVisible();
            StructureSet::ObjectList::const_iterator o, oe = structureSet->objects.end();
            for (o=structureSet->objects.begin(); o!=oe; ++o) {
                if ((*o)->mmdbID != bestObject->mmdbID) {
                    structureSet->showHideManager->Show(*o, false);
                }

                // on structure of interest, make visible only molecules with highlights
                else {
                    ChemicalGraph::MoleculeMap::const_iterator m, me = (*o)->graph->molecules.end();
                    for (m=(*o)->graph->molecules.begin(); m!=me; ++m)
                        if (!GlobalMessenger()->IsHighlightedAnywhere(m->second))
                            structureSet->showHideManager->Show(m->second, false);
                }
            }

            // now force redrawing of structures, so the frames that contains
            // these objects aren't empty (otherwise, renderer won't show selected frame)
            GlobalMessenger()->ProcessRedraws();

            // now show the frame containing the "best" object (get display list from first molecule)
            unsigned int displayList = bestObject->graph->molecules.find(1)->second->displayLists.front();
            for (unsigned int frame=0; frame<structureSet->frameMap.size(); ++frame) {
                StructureSet::DisplayLists::const_iterator d, de = structureSet->frameMap[frame].end();
                for (d=structureSet->frameMap[frame].begin(); d!=de; ++d) {
                    if (*d == displayList) { // is display list in this frame?
                        structureSet->renderer->ShowFrameNumber(frame);
                        frame = structureSet->frameMap.size(); // to exit out of next-up loop
                        break;
                    }
                }
            }

            wxMessageBox(
                selectedEvidence->GetBsannot().GetFeatures().front()->GetDescr().front()->GetName().c_str(),
                "Structure Evidence", wxOK | wxCENTRE, this);
        }
    }

    else if (selectedEvidence->IsComment()) {
        wxMessageBox(selectedEvidence->GetComment().c_str(), "Comment", wxOK | wxCENTRE, this);
    }

    else {
        ERRORMSG("CDDAnnotateDialog::ShowEvidence() - can't show that evidence type");
    }
}

void CDDAnnotateDialog::MoveEvidence(bool moveUp)
{
    DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(annots, ID_L_ANNOT, wxListBox)
    DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(evids, ID_L_EVID, wxListBox)

    // get selected annotation
    if (annots->GetCount() == 0 || annots->GetSelection() < 0) return;
    CAlign_annot *selectedAnnot =
        reinterpret_cast<CAlign_annot*>(annots->GetClientData(annots->GetSelection()));
    if (!selectedAnnot) {
        ERRORMSG("CDDAnnotateDialog::MoveEvidence() - error getting annotation pointer");
        return;
    }

    // get selected evidence
    if (evids->GetCount() == 0 || evids->GetSelection() < 0) return;
    CFeature_evidence *selectedEvidence =
        reinterpret_cast<CFeature_evidence*>(evids->GetClientData(evids->GetSelection()));
    if (!selectedEvidence) {
        ERRORMSG("CDDAnnotateDialog::MoveEvidence() - error getting evidence pointer");
        return;
    }

    CAlign_annot::TEvidence::iterator e, ee = selectedAnnot->SetEvidence().end(), ePrev = ee, eSwap = ee;
    CRef < CFeature_evidence > tmp;
    for (e=selectedAnnot->SetEvidence().begin(); e!=ee; ++e) {

        if (*e == selectedEvidence) {
            // figure out which (prev or next) evidence field to swap with
            if (moveUp && ePrev != ee)
                eSwap = ePrev;
            else if (!moveUp && (++(eSwap = e)) != ee)
                ;
            else
                return;

            // do the swap and update GUI
            tmp = *eSwap;
            *eSwap = *e;
            *e = tmp;
            structureSet->SetDataChanged(StructureSet::eUserAnnotationData);
            SetupGUIControls(annots->GetSelection(), evids->GetSelection() + (moveUp ? -1 : 1));
            return;
        }

        ePrev = e;
    }
}


///// CDDEvidenceDialog stuff /////

BEGIN_EVENT_TABLE(CDDEvidenceDialog, wxDialog)
    EVT_CLOSE       (       CDDEvidenceDialog::OnCloseWindow)
    EVT_BUTTON      (-1,    CDDEvidenceDialog::OnButton)
    EVT_RADIOBUTTON (-1,    CDDEvidenceDialog::OnChange)
    EVT_TEXT        (-1,    CDDEvidenceDialog::OnChange)
END_EVENT_TABLE()

CDDEvidenceDialog::CDDEvidenceDialog(wxWindow *parent, const ncbi::objects::CFeature_evidence& initial) :
    wxDialog(parent, -1, "CDD Annotations", wxPoint(400, 100), wxDefaultSize, wxDEFAULT_DIALOG_STYLE),
    changed(false), rerange(false)
{
    // construct the panel
    wxSizer *topSizer = SetupEvidenceDialog(this, false);

    // call sizer stuff
    topSizer->Fit(this);
    topSizer->SetSizeHints(this);

    // set initial states
    if (initial.IsComment()) {
        DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(rComment, ID_R_COMMENT, wxRadioButton)
        DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(tComment, ID_T_COMMENT, wxTextCtrl)
        rComment->SetValue(true);
        tComment->SetValue(initial.GetComment().c_str());
        tComment->SetFocus();
    } else if (initial.IsReference() && initial.GetReference().IsPmid()) {
        DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(rPMID, ID_R_PMID, wxRadioButton)
        DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(tPMID, ID_T_PMID, wxTextCtrl)
        rPMID->SetValue(true);
        wxString pmid;
        pmid.Printf("%i", initial.GetReference().GetPmid().Get());
        tPMID->SetValue(pmid);
        tPMID->SetFocus();
    } else if (IS_STRUCTURE_EVIDENCE_BSANNOT(initial)) {
        DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(rSTRUCTURE, ID_R_STRUCTURE, wxRadioButton)
        DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(tSTRUCTURE, ID_T_STRUCTURE, wxTextCtrl)
        rSTRUCTURE->SetValue(true);
        tSTRUCTURE->SetValue(
            initial.GetBsannot().GetFeatures().front()->GetDescr().front()->GetName().c_str());
    } else {
        ERRORMSG("CDDEvidenceDialog::CDDEvidenceDialog() - "
            "don't (yet) know how to edit this evidence type");
    }
    SetupGUIControls();
}

// same as hitting cancel button
void CDDEvidenceDialog::OnCloseWindow(wxCloseEvent& event)
{
    EndModal(wxCANCEL);
}

void CDDEvidenceDialog::OnButton(wxCommandEvent& event)
{
    switch (event.GetId()) {
        case ID_B_EDIT_OK: {
            EndModal(wxOK);
            break;
        }
        case ID_B_EDIT_CANCEL:
            EndModal(wxCANCEL);
            break;
        case ID_B_RERANGE:
            rerange = changed = true;
            break;
        default:
            event.Skip();
    }
}

void CDDEvidenceDialog::OnChange(wxCommandEvent& event)
{
    changed = true;
    SetupGUIControls();
}

void CDDEvidenceDialog::SetupGUIControls(void)
{
    DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(rComment, ID_R_COMMENT, wxRadioButton)
    DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(stComment, ID_ST_COMMENT, wxStaticText)
    DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(tComment, ID_T_COMMENT, wxTextCtrl)
    DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(rPMID, ID_R_PMID, wxRadioButton)
    DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(stPMID, ID_ST_PMID, wxStaticText)
    DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(tPMID, ID_T_PMID, wxTextCtrl)
    DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(rStructure, ID_R_STRUCTURE, wxRadioButton)
    DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(stStructure, ID_ST_STRUCTURE, wxStaticText)
    DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(tStructure, ID_T_STRUCTURE, wxTextCtrl)
    DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(bStructure, ID_B_RERANGE, wxButton)

//    stComment->Enable(rComment->GetValue());
    tComment->Enable(rComment->GetValue());
//    stPMID->Enable(rPMID->GetValue());
    tPMID->Enable(rPMID->GetValue());
//    stStructure->Enable(rStructure->GetValue());
    tStructure->Enable(rStructure->GetValue());
    bStructure->Enable(rStructure->GetValue());
}

bool CDDEvidenceDialog::GetData(ncbi::objects::CFeature_evidence *evidence)
{
    DECLARE_AND_FIND_WINDOW_RETURN_FALSE_ON_ERR(rComment, ID_R_COMMENT, wxRadioButton)
    DECLARE_AND_FIND_WINDOW_RETURN_FALSE_ON_ERR(rPMID, ID_R_PMID, wxRadioButton)
    DECLARE_AND_FIND_WINDOW_RETURN_FALSE_ON_ERR(rSTRUCTURE, ID_R_STRUCTURE, wxRadioButton)

    if (rComment->GetValue()) {
        DECLARE_AND_FIND_WINDOW_RETURN_FALSE_ON_ERR(tComment, ID_T_COMMENT, wxTextCtrl)
        if (tComment->GetValue().size() > 0) {
            evidence->SetComment(tComment->GetValue().c_str());
            return true;
        } else {
            ERRORMSG("CDDEvidenceDialog::GetData() - comment must not be zero-length");
            return false;
        }
    }

    else if (rPMID->GetValue()) {
        DECLARE_AND_FIND_WINDOW_RETURN_FALSE_ON_ERR(tPMID, ID_T_PMID, wxTextCtrl)
        unsigned long pmid;
        if (tPMID->GetValue().ToULong(&pmid)) {
            evidence->SetReference().SetPmid().Set(pmid);
            return true;
        } else {
            ERRORMSG("CDDEvidenceDialog::GetData() - PMID must be a positive integer");
            return false;
        }
    }

    else if (rSTRUCTURE->GetValue()) {
        DECLARE_AND_FIND_WINDOW_RETURN_FALSE_ON_ERR(tSTRUCTURE, ID_T_STRUCTURE, wxTextCtrl)
        // either leave existing range alone, or use new one if none present or rerange button pressed
        if (!evidence->IsBsannot() || rerange) {
            CBiostruc_annot_set *ref =
                GlobalMessenger()->CreateBiostrucAnnotSetForHighlightsOnSingleObject();
            if (ref)
                evidence->SetBsannot(*ref);
            else
                return false;
        }
        // reset structure evidence tag
        CRef < CBiostruc_descr > descr(new CBiostruc_descr());
        descr->SetOther_comment(STRUCTURE_EVIDENCE_COMMENT);
        evidence->SetBsannot().SetDescr().clear();
        evidence->SetBsannot().SetDescr().push_front(descr);
        // reset name
        CRef < CBiostruc_feature_set_descr > name(new CBiostruc_feature_set_descr());
        name->SetName(tSTRUCTURE->GetValue().c_str());
        evidence->SetBsannot().SetFeatures().front()->SetDescr().clear();
        evidence->SetBsannot().SetFeatures().front()->SetDescr().push_front(name);
        return true;
    }

    ERRORMSG("CDDEvidenceDialog::GetData() - unknown evidence type");
    return false;
}


///// CDDTypedAnnotDialog stuff /////

BEGIN_EVENT_TABLE(CDDTypedAnnotDialog, wxDialog)
    EVT_CLOSE       (       CDDTypedAnnotDialog::OnCloseWindow)
    EVT_BUTTON      (-1,    CDDTypedAnnotDialog::OnButton)
    EVT_CHOICE      (-1,    CDDTypedAnnotDialog::OnTypeChoice)
    EVT_COMBOBOX    (-1,    CDDTypedAnnotDialog::OnChange)
    EVT_TEXT        (-1,    CDDTypedAnnotDialog::OnChange)
    EVT_TEXT_ENTER  (-1,    CDDTypedAnnotDialog::OnChange)
END_EVENT_TABLE()

const string CDDTypedAnnotDialog::typesNamesIniFile = "cdd_annot_types.ini";
CDDTypedAnnotDialog::TPredefinedSites CDDTypedAnnotDialog::predefinedSites;

CDDTypedAnnotDialog::CDDTypedAnnotDialog(wxWindow *parent, const ncbi::objects::CAlign_annot& initial, const string& title) :
    wxDialog(parent, -1, title.c_str(), wxPoint(400, 100), wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER),
    changed(false), isTypeDataRead(false)
{
    bool isPutative = false;
    bool isPredefDescr = false;
    int type = 0;
    int predefType, predefTypeIndex;
    wxString descr;

    // construct the panel
    wxSizer *topSizer = SetupTypedDescriptionDialog(this, false);

    // call sizer stuff
    topSizer->Fit(this);
    topSizer->SetSizeHints(this);

    DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(cType, ID_C_TYPE, wxChoice)
    DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(cmbDescr, ID_CMB_DESCR, wxComboBox)
    DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(cbPutative, ID_CB_PUTATIVE, wxCheckBox)

    //  Load the pre-defined type categories and descriptions
    if (predefinedSites.empty()) {
        string typeFile = GetDataDir() + typesNamesIniFile;
        LoadTypes(typeFile);
    }

    //  Populate the wxChoice element
    if (!isTypeDataRead && predefinedSites.empty()) {
        string message = "Predefined types & names are unavailable;\nonly annotations of type 'Other' may be created.\n\nCheck that the file '" + typesNamesIniFile + "' is present in the Cn3D 'data' directory.";
        wxMessageBox(message.c_str(), "Predefined Types/Names Unavailable", wxICON_EXCLAMATION | wxCENTRE, this);
    } else {
        cType->Clear();
        ITERATE (TPredefinedSites, cit, predefinedSites) {
            cType->Append(cit->second.first);
        }
    }

    // Determine values with which to initialize the widgets.
    if (initial.IsSetType()) {
        if (IsValidType(initial.GetType())) {
            type = initial.GetType();
        }
    }
    if (initial.IsSetDescription() && !initial.GetDescription().empty()) {

        descr = wxString(initial.GetDescription().c_str());
        isPutative = NStr::StartsWith(initial.GetDescription(), "putative", NStr::eNocase);
        if (isPutative) {
            //  remove the putative prefix for display in the widget
            descr = descr.Mid(8);
            descr.Trim(false);
        }
        isPredefDescr = IsPredefinedDescr(descr, predefType, predefTypeIndex);
        if (isPredefDescr) {
            if (initial.IsSetType() && (type != predefType)) {
                WARNINGMSG("CDDTypedAnnotDialog::CDDTypedAnnotDialog() - stored type inconsistent with descriptions's designated type");
            }
            type = predefType;
        }
    }

    //  Initialize the widgets.
    cType->SetSelection(type);
    AdjustComboboxForType(type);
    cbPutative->SetValue(isPutative);
    if (isPredefDescr) {
        cmbDescr->SetSelection(predefTypeIndex);
    } else if (descr.length() > 0) {
        cmbDescr->SetValue(descr);
    } else {
        cmbDescr->SetSelection(wxNOT_FOUND);
    }
    cmbDescr->SetFocus();

    SetupGUIControls();
}

void CDDTypedAnnotDialog::LoadTypes(const string& filename)
{
    static const string ID("id");
    static const int DEFAULT_ID(-1);

    int id;
    int accessFlags = (IRegistry::fCaseFlags | IRegistry::fInternalSpaces);
    int readFlags = (IRegistry::fCaseFlags | IRegistry::fInternalSpaces);
    string value;
    list<string> categories, names;
    set<string> valuesInAlphaOrder;

    //  Use CMemoryRegistry simply to leverage registry file format parser.
    CMemoryRegistry dummyRegistry;
    auto_ptr<CNcbiIfstream> iniIn(new CNcbiIfstream(filename.c_str(), IOS_BASE::in | IOS_BASE::binary));
    if (*iniIn) {

        isTypeDataRead = true;

        TRACEMSG("loading predefined annotation types " << filename);
        dummyRegistry.Read(*iniIn, readFlags);
        dummyRegistry.EnumerateSections(&categories, accessFlags);

        //  Loop over all type categories
        ITERATE (list<string>, cit, categories) {
            if (*cit == kEmptyStr) continue;

            //  'id' is assumed to be positive, and the id will be used to
            //  define the position in the wxChoice element so it is important
            //  the input file has consecutive id values starting from zero.
            id = dummyRegistry.GetInt(*cit, ID, DEFAULT_ID, accessFlags, CMemoryRegistry::eReturn);
            if (id > DEFAULT_ID) {
                TTypeNamesPair& typeNamesPair = predefinedSites[id];
                wxArrayString& values = typeNamesPair.second;
                typeNamesPair.first = wxString(cit->c_str());

                //  Get all named fields for this section.
                dummyRegistry.EnumerateEntries(*cit, &names, accessFlags);

                //  Get the value for each non-id named field; sort alphabetically
                valuesInAlphaOrder.clear();
                ITERATE (list<string>, eit, names) {
                    if (*eit == ID || *eit == kEmptyStr) continue;
                    value = dummyRegistry.GetString(*cit, *eit, kEmptyStr, accessFlags);
                    if (value != kEmptyStr) {
                        valuesInAlphaOrder.insert(value);
                    }
                }

                ITERATE (set<string>, vit, valuesInAlphaOrder) {
                    values.push_back(wxString(vit->c_str()));
                }
            }
        }
    }
}

void CDDTypedAnnotDialog::AdjustComboboxForType(int type)
{
    DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(cmbDescr, ID_CMB_DESCR, wxComboBox)
    int sel = cmbDescr->GetSelection();
    wxString existingDescr = cmbDescr->GetValue();

    cmbDescr->Clear();
    TPredefinedSites::const_iterator cit = predefinedSites.find(type);
    if (cit != predefinedSites.end()) {
        cmbDescr->Append(cit->second.second);
    }

    //  If there was free-form text displayed, keep displaying it.
    //  Otherwise, make sure a pre-defined type name is *not* shown.
    if (sel == wxNOT_FOUND) {
        cmbDescr->SetValue(existingDescr);
    } else {
        cmbDescr->SetSelection(wxNOT_FOUND);
    }
}

// same as hitting cancel button
void CDDTypedAnnotDialog::OnCloseWindow(wxCloseEvent& event)
{
    EndModal(wxCANCEL);
}

void CDDTypedAnnotDialog::OnButton(wxCommandEvent& event)
{
    switch (event.GetId()) {
        case ID_B_DESCR_OK: {
            EndModal(wxOK);
            break;
        }
        case ID_B_DESCR_CANCEL:
            EndModal(wxCANCEL);
            break;
        default:
            event.Skip();
    }
}

void CDDTypedAnnotDialog::OnChange(wxCommandEvent& event)
{
    changed = true;
    SetupGUIControls();
}

void CDDTypedAnnotDialog::OnTypeChoice(wxCommandEvent& event)
{
    DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(cType, ID_C_TYPE, wxChoice)

    changed = true;
    AdjustComboboxForType(cType->GetSelection());
    SetupGUIControls();
}

bool CDDTypedAnnotDialog::GetData(ncbi::objects::CAlign_annot* alignAnnot)
{
    static const string ws(" \t\n\r");
    static const string leading(" \t\n\r.,;:!@#$%^&\\|/?}])>");   //  no closing delimiter at start
    static const string trailing(" \t\n\r.,;:!@#$%^&\\|/?{[(<");  //  no opening delimiter at end

    bool result = false;
    if (!alignAnnot) return result;

    DECLARE_AND_FIND_WINDOW_RETURN_FALSE_ON_ERR(cType, ID_C_TYPE, wxChoice)
    DECLARE_AND_FIND_WINDOW_RETURN_FALSE_ON_ERR(cmbDescr, ID_CMB_DESCR, wxComboBox)
    DECLARE_AND_FIND_WINDOW_RETURN_FALSE_ON_ERR(cbPutative, ID_CB_PUTATIVE, wxCheckBox)

    bool isPredefDescr = false;
    bool isPutative = cbPutative->GetValue();
    int predefType, predefTypeIndex;
    int type = cType->GetSelection();
    int sel = cmbDescr->GetSelection();
    string descr = string(cmbDescr->GetValue().c_str());


    //  Remove disallowed leading/trailing characters.
    SIZE_TYPE firstGoodChar = descr.find_first_not_of(leading);
    if (firstGoodChar != NPOS && firstGoodChar > 0) {
        descr = descr.substr(firstGoodChar);
    }
    SIZE_TYPE lastGoodChar = descr.find_last_not_of(trailing);
    if (lastGoodChar != NPOS && lastGoodChar < descr.size() - 1) {
        descr = descr.substr(0, lastGoodChar + 1);
    }

    //  Remove internal runs of whitespace.
    vector<string> tokens;
    NStr::Tokenize(descr, ws, tokens, NStr::eMergeDelims);
    if (tokens.size() > 0) {
        descr = tokens[0];
        for (unsigned int i = 1; i < tokens.size(); ++i) {
            descr += " " + tokens[i];
        }
    }    

    //  If user typed 'putative' as a prefix in the description, that 
    //  supercedes the checkbox state.  Also, strip that prefix from 
    //  the string so it does not impact the predefined description check.  
    if (NStr::StartsWith(descr, "putative", NStr::eNocase)) {
        isPutative = true;  // override the checkbox
        descr = descr.substr(8);
    }
    NStr::TruncateSpacesInPlace(descr);

    //  Simple attempt to regularize capitalization for predefined descriptions.
    //  This is tricky, though, since descriptions may contain capitalized
    //  substrings by convention (e.g., 'DNA binding site' and 'Cu binding site'
    //  are acceptible, but 'Active Site' should become 'active site').
    wxString descrWx = wxString(descr.c_str());
    predefType = wxNOT_FOUND;
    if (IsPredefinedDescr(descrWx, predefType, predefTypeIndex)) {
        isPredefDescr = true;
    } else if (IsPredefinedDescr(descrWx.Lower(), predefType, predefTypeIndex)) {
        NStr::ToLower(descr);
        isPredefDescr = true;
    }

    //  If 'descr' corresponds to a predefined descriptions, ensure the 
    //  predefined type was used (prevents a user from manually overriding 
    //  the standard type/name mappings defined in 'cdd_annot_types.ini').
    //  Force the type back to the standard type for the given description,
    //  and provide a warning to the user.
    if (isPredefDescr) {
        if (type != predefType) {
            wxString typeChosen = predefinedSites[type].first;
            wxString predefTypeStr = predefinedSites[predefType].first;
            wxString message;
            message.Printf("The predefined description '%s' corresponds to type '%s' and will be changed from your chosen type '%s'.\n"
                "Press 'OK' to reassign to the standard type '%s'.\nPress 'Cancel' to return and not create/edit the annotation.\n", 
                cmbDescr->GetValue().c_str(), predefTypeStr.c_str(), typeChosen.c_str(), predefTypeStr.c_str());
            int reply = wxMessageBox(message, "Inconsistent type", wxOK | wxCANCEL | wxCENTRE, this);
            if (reply == wxOK) {
                type = predefType;
            } else {
                descr.erase();
            }
        }
    }

    if (descr.length() > 0) {
        //  add 'putative' prefix if needed and not already present
        if (isPutative && !NStr::StartsWith(descr, "putative", NStr::eNocase)) {
            descr = "putative " + descr;
        }
        alignAnnot->SetDescription(descr);
        alignAnnot->SetType(type);
        result = true;
    }
    return result;
}

void CDDTypedAnnotDialog::SetupGUIControls(void)
{
}

unsigned int CDDTypedAnnotDialog::NumPredefinedDescrs(int type) const
{
    unsigned int n = 0;
    TPredefinedSites::const_iterator cit = predefinedSites.find(type);
    if (cit != predefinedSites.end()) {
        n = cit->second.second.size();
    }
    return n;
}

bool CDDTypedAnnotDialog::IsValidType(int type) const
{
    TPredefinedSites::const_iterator cit = predefinedSites.find(type);
    return (cit != predefinedSites.end());
}

bool CDDTypedAnnotDialog::IsPredefinedDescrForType(int type, const wxString& descr) const
{
    bool result = false;
    TPredefinedSites::const_iterator cit = predefinedSites.find(type);
    if (cit != predefinedSites.end()) {
        const wxArrayString& as = cit->second.second;
        for (wxArrayString::const_iterator asCit = as.begin(); asCit != as.end(); ++asCit) {
            if (*asCit == descr) {
                result = true;
                break;
            }
        }
    }
    return result;
}

//  Return true if 'descr' is a predefined name for some type; return false otherwise.
//  When found, 'type' is set to be the type code, and 'typeIndex' is its position
//  in the corresponding wxArrayString.  When not found, 'type' and 'typeIndex' are
//  set to wxNOT_FOUND.
bool CDDTypedAnnotDialog::IsPredefinedDescr(const wxString& descr, int& type, int& typeIndex) const
{
    bool result = false;
    int index;
    TPredefinedSites::const_iterator pdsCit = predefinedSites.begin();
    TPredefinedSites::const_iterator pdsEnd = predefinedSites.end();

    type = wxNOT_FOUND;
    typeIndex = wxNOT_FOUND;
    for (; (pdsCit != pdsEnd) && !result; ++pdsCit) {
        index = 0;
        const wxArrayString& as = pdsCit->second.second;
        for (wxArrayString::const_iterator asCit = as.begin(); asCit != as.end(); ++asCit, ++index) {
            if (*asCit == descr) {
                result = true;
                type = pdsCit->first;
                typeIndex = index;
                break;
            }
        }
    }
    return result;
}

END_SCOPE(Cn3D)


////////////////////////////////////////////////////////////////////////////////////////////////
// The following is taken unmodified from wxDesigner's C++ code from cdd_annot_dialog.wdr
////////////////////////////////////////////////////////////////////////////////////////////////

wxSizer *SetupCDDAnnotDialog( wxWindow *parent, bool call_fit, bool set_sizer )
{
    wxBoxSizer *item0 = new wxBoxSizer( wxVERTICAL );

    wxFlexGridSizer *item1 = new wxFlexGridSizer( 1, 0, 0, 0 );

    wxStaticBox *item3 = new wxStaticBox( parent, -1, "Annotations" );
    wxStaticBoxSizer *item2 = new wxStaticBoxSizer( item3, wxVERTICAL );

    wxString *strs4 = (wxString*) NULL;
    wxListBox *item4 = new wxListBox( parent, ID_L_ANNOT, wxDefaultPosition, wxSize(200,100), 0, strs4, wxLB_SINGLE );
    item2->Add( item4, 0, wxGROW|wxALIGN_CENTER_VERTICAL|wxALL, 5 );

    wxGridSizer *item5 = new wxGridSizer( 2, 0, 0, 0 );

    wxButton *item6 = new wxButton( parent, ID_B_NEW_ANNOT, "New", wxDefaultPosition, wxDefaultSize, 0 );
    item5->Add( item6, 0, wxALIGN_CENTRE|wxALL, 5 );

    wxButton *item7 = new wxButton( parent, ID_B_HIGHLIGHT, "Highlight", wxDefaultPosition, wxDefaultSize, 0 );
    item5->Add( item7, 0, wxALIGN_CENTRE|wxALL, 5 );

    wxButton *item8 = new wxButton( parent, ID_B_ANNOT_UP, "Move Up", wxDefaultPosition, wxDefaultSize, 0 );
    item5->Add( item8, 0, wxALIGN_CENTRE|wxALL, 5 );

    wxButton *item9 = new wxButton( parent, ID_B_EDIT_ANNOT, "Edit", wxDefaultPosition, wxDefaultSize, 0 );
    item5->Add( item9, 0, wxALIGN_CENTRE|wxALL, 5 );

    wxButton *item10 = new wxButton( parent, ID_B_DEL_ANNOT, "Delete", wxDefaultPosition, wxDefaultSize, 0 );
    item5->Add( item10, 0, wxALIGN_CENTRE|wxALL, 5 );

    wxButton *item11 = new wxButton( parent, ID_B_ANNOT_DOWN, "Move Down", wxDefaultPosition, wxDefaultSize, 0 );
    item5->Add( item11, 0, wxALIGN_CENTRE|wxALL, 5 );

    item2->Add( item5, 0, wxALIGN_CENTRE|wxALL, 5 );

    item1->Add( item2, 0, wxALIGN_CENTRE|wxALL, 5 );

    wxStaticBox *item13 = new wxStaticBox( parent, -1, "Evidence" );
    wxStaticBoxSizer *item12 = new wxStaticBoxSizer( item13, wxVERTICAL );

    wxString *strs14 = (wxString*) NULL;
    wxListBox *item14 = new wxListBox( parent, ID_L_EVID, wxDefaultPosition, wxSize(300,100), 0, strs14, wxLB_SINGLE );
    item12->Add( item14, 0, wxGROW|wxALIGN_CENTER_VERTICAL|wxALL, 5 );

    wxGridSizer *item15 = new wxGridSizer( 2, 0, 0, 0 );

    wxButton *item16 = new wxButton( parent, ID_B_NEW_EVID, "New", wxDefaultPosition, wxDefaultSize, 0 );
    item15->Add( item16, 0, wxALIGN_CENTRE|wxALL, 5 );

    wxButton *item17 = new wxButton( parent, ID_B_SHOW, "Show", wxDefaultPosition, wxDefaultSize, 0 );
    item15->Add( item17, 0, wxALIGN_CENTRE|wxALL, 5 );

    wxButton *item18 = new wxButton( parent, ID_B_EVID_UP, "Move Up", wxDefaultPosition, wxDefaultSize, 0 );
    item15->Add( item18, 0, wxALIGN_CENTRE|wxALL, 5 );

    wxButton *item19 = new wxButton( parent, ID_B_EDIT_EVID, "Edit", wxDefaultPosition, wxDefaultSize, 0 );
    item15->Add( item19, 0, wxALIGN_CENTRE|wxALL, 5 );

    wxButton *item20 = new wxButton( parent, ID_B_DEL_EVID, "Delete", wxDefaultPosition, wxDefaultSize, 0 );
    item15->Add( item20, 0, wxALIGN_CENTRE|wxALL, 5 );

    wxButton *item21 = new wxButton( parent, ID_B_EVID_DOWN, "Move Down", wxDefaultPosition, wxDefaultSize, 0 );
    item15->Add( item21, 0, wxALIGN_CENTRE|wxALL, 5 );

    item12->Add( item15, 0, wxALIGN_CENTRE|wxALL, 5 );

    item1->Add( item12, 0, wxALIGN_CENTRE|wxALL, 5 );

    item0->Add( item1, 0, wxALIGN_CENTRE, 5 );

    if (set_sizer)
    {
        parent->SetAutoLayout( TRUE );
        parent->SetSizer( item0 );
        if (call_fit)
        {
            item0->Fit( parent );
            item0->SetSizeHints( parent );
        }
    }

    return item0;
}

wxSizer *SetupEvidenceDialog( wxWindow *parent, bool call_fit, bool set_sizer )
{
    wxBoxSizer *item0 = new wxBoxSizer( wxVERTICAL );

    wxStaticBox *item2 = new wxStaticBox( parent, -1, "Evidence Options" );
    wxStaticBoxSizer *item1 = new wxStaticBoxSizer( item2, wxVERTICAL );

    wxFlexGridSizer *item3 = new wxFlexGridSizer( 1, 0, 0, 0 );
    item3->AddGrowableCol( 2 );

    wxRadioButton *item4 = new wxRadioButton( parent, ID_R_COMMENT, "", wxDefaultPosition, wxDefaultSize, 0 );
    item3->Add( item4, 0, wxALIGN_BOTTOM|wxALIGN_CENTER_HORIZONTAL|wxALL, 5 );

    wxStaticText *item5 = new wxStaticText( parent, ID_ST_COMMENT, "Comment:", wxDefaultPosition, wxDefaultSize, 0 );
    item3->Add( item5, 0, wxALIGN_CENTRE|wxRIGHT|wxTOP|wxBOTTOM, 5 );

    wxTextCtrl *item6 = new wxTextCtrl( parent, ID_T_COMMENT, "", wxDefaultPosition, wxSize(150,-1), 0 );
    item3->Add( item6, 0, wxGROW|wxALIGN_CENTER_VERTICAL|wxALL, 5 );

    item1->Add( item3, 0, wxGROW|wxALIGN_CENTER_VERTICAL, 5 );

    wxStaticLine *item7 = new wxStaticLine( parent, ID_LINE, wxDefaultPosition, wxSize(20,-1), wxLI_HORIZONTAL );
    item1->Add( item7, 0, wxGROW|wxALIGN_CENTER_VERTICAL|wxALL, 5 );

    wxFlexGridSizer *item8 = new wxFlexGridSizer( 1, 0, 0, 0 );
    item8->AddGrowableCol( 2 );

    wxRadioButton *item9 = new wxRadioButton( parent, ID_R_PMID, "", wxDefaultPosition, wxDefaultSize, 0 );
    item8->Add( item9, 0, wxALIGN_BOTTOM|wxALIGN_CENTER_HORIZONTAL|wxALL, 5 );

    wxStaticText *item10 = new wxStaticText( parent, ID_ST_PMID, "Reference (PubMed ID):", wxDefaultPosition, wxDefaultSize, 0 );
    item8->Add( item10, 0, wxALIGN_CENTRE|wxRIGHT|wxTOP|wxBOTTOM, 5 );

    wxTextCtrl *item11 = new wxTextCtrl( parent, ID_T_PMID, "", wxDefaultPosition, wxDefaultSize, 0 );
    item8->Add( item11, 0, wxGROW|wxALIGN_CENTER_VERTICAL|wxALL, 5 );

    item1->Add( item8, 0, wxGROW|wxALIGN_CENTER_VERTICAL, 5 );

    wxStaticLine *item12 = new wxStaticLine( parent, ID_LINE, wxDefaultPosition, wxSize(20,-1), wxLI_HORIZONTAL );
    item1->Add( item12, 0, wxGROW|wxALIGN_CENTER_VERTICAL|wxALL, 5 );

    wxFlexGridSizer *item13 = new wxFlexGridSizer( 1, 0, 0, 0 );
    item13->AddGrowableCol( 2 );

    wxRadioButton *item14 = new wxRadioButton( parent, ID_R_STRUCTURE, "", wxDefaultPosition, wxDefaultSize, 0 );
    item13->Add( item14, 0, wxALIGN_BOTTOM|wxALIGN_CENTER_HORIZONTAL|wxALL, 5 );

    wxStaticText *item15 = new wxStaticText( parent, ID_ST_STRUCTURE, "Structure:", wxDefaultPosition, wxDefaultSize, 0 );
    item13->Add( item15, 0, wxALIGN_CENTRE|wxRIGHT|wxTOP|wxBOTTOM, 5 );

    wxTextCtrl *item16 = new wxTextCtrl( parent, ID_T_STRUCTURE, "", wxDefaultPosition, wxSize(80,-1), 0 );
    item13->Add( item16, 0, wxGROW|wxALIGN_CENTER_VERTICAL|wxALL, 5 );

    item1->Add( item13, 0, wxGROW|wxALIGN_CENTER_VERTICAL, 5 );

    wxButton *item17 = new wxButton( parent, ID_B_RERANGE, "Reset Residues to Current Highlights", wxDefaultPosition, wxDefaultSize, 0 );
    item1->Add( item17, 0, wxALIGN_CENTRE|wxALL, 5 );

    item0->Add( item1, 0, wxGROW|wxALIGN_CENTER_VERTICAL|wxALL, 5 );

    wxFlexGridSizer *item18 = new wxFlexGridSizer( 1, 0, 0, 0 );

    wxButton *item19 = new wxButton( parent, ID_B_EDIT_OK, "OK", wxDefaultPosition, wxDefaultSize, 0 );
    item19->SetDefault();
    item18->Add( item19, 0, wxALIGN_CENTRE|wxALL, 5 );

    item18->Add( 20, 20, 0, wxALIGN_CENTRE|wxALL, 5 );

    wxButton *item20 = new wxButton( parent, ID_B_EDIT_CANCEL, "Cancel", wxDefaultPosition, wxDefaultSize, 0 );
    item18->Add( item20, 0, wxALIGN_CENTRE|wxALL, 5 );

    item0->Add( item18, 0, wxALIGN_CENTRE|wxALL, 5 );

    if (set_sizer)
    {
        parent->SetAutoLayout( TRUE );
        parent->SetSizer( item0 );
        if (call_fit)
        {
            item0->Fit( parent );
            item0->SetSizeHints( parent );
        }
    }

    return item0;
}

wxSizer *SetupTypedDescriptionDialog( wxWindow *parent, bool call_fit, bool set_sizer )
{
    wxBoxSizer *item0 = new wxBoxSizer( wxVERTICAL );

    wxStaticBox *item2 = new wxStaticBox( parent, -1, wxT("Typed Description") );
    wxStaticBoxSizer *item1 = new wxStaticBoxSizer( item2, wxVERTICAL );

    wxFlexGridSizer *item3 = new wxFlexGridSizer( 1, 0, 0, 0 );
    item3->AddGrowableCol( 2 );

    wxStaticText *item4 = new wxStaticText( parent, ID_ST_TYPE, wxT("Type:"), wxDefaultPosition, wxDefaultSize, 0 );
    item3->Add( item4, 0, wxALIGN_CENTER_VERTICAL|wxRIGHT|wxTOP|wxBOTTOM, 5 );

    item3->Add( 20, 20, 0, wxALIGN_CENTER|wxALL, 5 );

    wxString strs5[] = 
    {
        wxT("Other")
    };
    wxChoice *item5 = new wxChoice( parent, ID_C_TYPE, wxDefaultPosition, wxSize(150,-1), 1, strs5, 0 );
    item3->Add( item5, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5 );

    item3->Add( 10, 20, 0, wxALIGN_CENTER|wxALL, 5 );

    wxCheckBox *item6 = new wxCheckBox( parent, ID_CB_PUTATIVE, wxT("Is Putative?"), wxDefaultPosition, wxDefaultSize, 0 );
    item3->Add( item6, 0, wxALIGN_RIGHT|wxALIGN_CENTER_VERTICAL|wxALL, 5 );

    item1->Add( item3, 0, wxGROW|wxALIGN_CENTER_VERTICAL|wxALL, 5 );

    wxFlexGridSizer *item7 = new wxFlexGridSizer( 1, 0, 0, 0 );
    item7->AddGrowableCol( 1 );

    wxStaticText *item8 = new wxStaticText( parent, ID_ST_DESCR2, wxT("Description:"), wxDefaultPosition, wxDefaultSize, 0 );
    item7->Add( item8, 0, wxALIGN_CENTER_VERTICAL|wxRIGHT|wxTOP|wxBOTTOM, 5 );

    wxString strs9[] = 
    {
        wxT("ComboItem"), 
        wxT("1Item"), 
        wxT("222"), 
        wxT("333"), 
        wxT("444")
    };
    wxComboBox *item9 = new wxComboBox( parent, ID_CMB_DESCR, wxT(""), wxDefaultPosition, wxSize(200,-1), 5, strs9, wxCB_DROPDOWN );
    item7->Add( item9, 0, wxGROW|wxALIGN_CENTER_VERTICAL|wxALL, 5 );

    item1->Add( item7, 0, wxGROW|wxALIGN_CENTER_VERTICAL|wxALL, 5 );

    wxStaticLine *item10 = new wxStaticLine( parent, ID_LINE, wxDefaultPosition, wxSize(20,-1), wxLI_HORIZONTAL );
    item1->Add( item10, 0, wxGROW|wxALIGN_CENTER_VERTICAL|wxALL, 5 );

    wxFlexGridSizer *item11 = new wxFlexGridSizer( 1, 0, 0, 0 );

    wxButton *item12 = new wxButton( parent, ID_B_DESCR_OK, wxT("OK"), wxDefaultPosition, wxDefaultSize, 0 );
    item12->SetDefault();
    item11->Add( item12, 0, wxALIGN_CENTER|wxALL, 5 );

    wxButton *item13 = new wxButton( parent, ID_B_DESCR_CANCEL, wxT("Cancel"), wxDefaultPosition, wxDefaultSize, 0 );
    item11->Add( item13, 0, wxALIGN_RIGHT|wxALIGN_CENTER_VERTICAL|wxALL, 5 );

    item1->Add( item11, 0, wxALIGN_RIGHT|wxALIGN_CENTER_VERTICAL, 5 );

    item0->Add( item1, 0, wxGROW|wxALIGN_CENTER_VERTICAL|wxALL, 5 );

    if (set_sizer)
    {
        parent->SetSizer( item0 );
        if (call_fit)
            item0->SetSizeHints( parent );
    }
    
    return item0;
}