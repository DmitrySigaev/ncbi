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
*      implementation of GUI part of main sequence/alignment viewer
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.22  2002/09/09 22:51:19  thiessen
* add basic taxonomy tree viewer
*
* Revision 1.21  2002/09/09 13:38:23  thiessen
* separate save and save-as
*
* Revision 1.20  2002/09/05 18:38:58  thiessen
* add sort by highlights
*
* Revision 1.19  2002/09/03 13:15:58  thiessen
* add A2M export
*
* Revision 1.18  2002/06/13 14:54:07  thiessen
* add sort by self-hit
*
* Revision 1.17  2002/06/13 13:32:39  thiessen
* add self-hit calculation
*
* Revision 1.16  2002/06/05 14:28:40  thiessen
* reorganize handling of window titles
*
* Revision 1.15  2002/05/17 19:10:27  thiessen
* preliminary range restriction for BLAST/PSSM
*
* Revision 1.14  2002/04/22 14:27:29  thiessen
* add alignment export
*
* Revision 1.13  2001/12/06 23:13:46  thiessen
* finish import/align new sequences into single-structure data; many small tweaks
*
* Revision 1.12  2001/06/04 14:33:55  thiessen
* add proximity sort; highlight sequence on browser launch
*
* Revision 1.11  2001/06/01 14:04:54  thiessen
* add float PDB sort
*
* Revision 1.10  2001/05/23 17:43:28  thiessen
* change dialog implementation to wxDesigner; interface changes
*
* Revision 1.9  2001/05/11 02:10:04  thiessen
* add better merge fail indicators; tweaks to windowing/taskbar
*
* Revision 1.8  2001/05/09 17:14:52  thiessen
* add automatic block removal upon demotion
*
* Revision 1.7  2001/04/05 22:54:51  thiessen
* change bg color handling ; show geometry violations
*
* Revision 1.6  2001/03/30 14:43:11  thiessen
* show threader scores in status line; misc UI tweaks
*
* Revision 1.5  2001/03/30 03:07:09  thiessen
* add threader score calculation & sorting
*
* Revision 1.4  2001/03/19 15:47:38  thiessen
* add row sorting by identifier
*
* Revision 1.3  2001/03/13 01:24:16  thiessen
* working undo system for >1 alignment (e.g., update window)
*
* Revision 1.2  2001/03/09 15:48:43  thiessen
* major changes to add initial update viewer
*
* Revision 1.1  2001/03/01 20:15:29  thiessen
* major rearrangement of sequence viewer code into base and derived classes
*
* ===========================================================================
*/

#ifndef CN3D_SEQUENCE_VIEWER_WINDOW__HPP
#define CN3D_SEQUENCE_VIEWER_WINDOW__HPP

#include "cn3d/viewer_window_base.hpp"


BEGIN_SCOPE(Cn3D)

class SequenceViewer;
class SequenceDisplay;
class TaxonomyTree;

class SequenceViewerWindow : public ViewerWindowBase
{
    friend class SequenceDisplay;

public:
    SequenceViewerWindow(SequenceViewer *parentSequenceViewer);
    ~SequenceViewerWindow(void);

    bool RequestEditorEnable(bool enable);
    void EnableDerivedEditorMenuItems(bool enabled);
    void TurnOnEditor(void);
    void SetWindowTitle(void);

private:
    SequenceViewer *sequenceViewer;
    TaxonomyTree *taxonomyTree;

    // menu identifiers - additional items beyond base class items
    enum {
        // view menu
        MID_SHOW_HIDE_ROWS = START_VIEWER_WINDOW_DERIVED_MID,
        MID_SCORE_THREADER,
        MID_SELF_HIT,
        MID_TAXONOMY,
        MID_EXPORT,
            MID_EXPORT_FASTA,
            MID_EXPORT_A2M,
            MID_EXPORT_TEXT,
            MID_EXPORT_HTML,
        // edit menu
        MID_DELETE_ROW,
        MID_SORT_ROWS,   // sort rows submenu
            MID_SORT_IDENT,
            MID_SORT_THREADER,
            MID_FLOAT_PDBS,
            MID_FLOAT_HIGHLIGHTS,
            MID_SORT_SELF_HIT,
            MID_PROXIMITY_SORT,
        // mouse mode
        MID_MOVE_ROW,
        // update menu
        MID_SHOW_UPDATES,
        MID_REALIGN_ROW,
        MID_REALIGN_ROWS,
        MID_MARK_BLOCK,
        MID_CLEAR_MARKS
    };

    void OnShowHideRows(wxCommandEvent& event);
    void OnDeleteRow(wxCommandEvent& event);
    void OnMoveRow(wxCommandEvent& event);
    void OnShowUpdates(wxCommandEvent& event);
    void OnRealign(wxCommandEvent& event);
    void OnSort(wxCommandEvent& event);
    void OnScoreThreader(wxCommandEvent& event);
    void OnMarkBlock(wxCommandEvent& event);
    void OnExport(wxCommandEvent& event);
    void OnSelfHit(wxCommandEvent& event);
    void OnTaxonomy(wxCommandEvent& event);

    // called before an operation (e.g., alignment editor enable) that requires
    // all rows of an alignment to be visible; 'false' return should abort that operation
    bool QueryShowAllRows(void);

    void OnCloseWindow(wxCloseEvent& event);

    wxMenu *updateMenu;

    void DeleteRowOff(void)
    {
        menuBar->Check(MID_DELETE_ROW, false);
        SetCursor(wxNullCursor);
    }
    void RealignRowOff(void)
    {
        menuBar->Check(MID_REALIGN_ROW, false);
        SetCursor(wxNullCursor);
    }
    void MarkBlockOff(void)
    {
        menuBar->Check(MID_MARK_BLOCK, false);
        SetCursor(wxNullCursor);
    }
    void ProximitySortOff(void)
    {
        menuBar->Check(MID_PROXIMITY_SORT, false);
        SetCursor(wxNullCursor);
    }

    SequenceViewerWidget::eMouseMode GetMouseModeForCreateAndMerge(void)
    {
        return SequenceViewerWidget::eSelectColumns;
    }

    DECLARE_EVENT_TABLE()

public:

    bool SaveDialog(bool prompt, bool canCancel);

    bool DoDeleteRow(void) const { return menuBar->IsChecked(MID_DELETE_ROW); }
    bool DoRealignRow(void) const { return menuBar->IsChecked(MID_REALIGN_ROW); }
    bool DoMarkBlock(void) const { return menuBar->IsChecked(MID_MARK_BLOCK); }
    bool DoProximitySort(void) const { return menuBar->IsChecked(MID_PROXIMITY_SORT); }

    void CancelDerivedSpecialModesExcept(int id)
    {
        if (id != MID_DELETE_ROW && DoDeleteRow()) DeleteRowOff();
        if (id != MID_REALIGN_ROW && DoRealignRow()) RealignRowOff();
        if (id != MID_MARK_BLOCK && DoMarkBlock()) MarkBlockOff();
        if (id != MID_PROXIMITY_SORT && DoProximitySort()) ProximitySortOff();
    }

    void EnableExport(bool enabled) { menuBar->Enable(MID_EXPORT, enabled); }
};

END_SCOPE(Cn3D)

#endif // CN3D_SEQUENCE_VIEWER_WINDOW__HPP
