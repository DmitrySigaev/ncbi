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
*      Classes to hold rows in a sequence/alignment display
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.49  2002/05/07 20:22:47  thiessen
* fix for BLAST/PSSM
*
* Revision 1.48  2002/05/02 18:40:25  thiessen
* do BLAST/PSSM for debug builds only, for testing
*
* Revision 1.47  2002/04/26 19:01:00  thiessen
* fix display delete bug
*
* Revision 1.46  2002/04/26 13:46:36  thiessen
* comment out all blast/pssm methods
*
* Revision 1.45  2002/03/28 14:06:02  thiessen
* preliminary BLAST/PSSM ; new CD startup style
*
* Revision 1.44  2002/02/28 19:11:52  thiessen
* wrap sequences in single-structure mode
*
* Revision 1.43  2002/02/22 14:24:01  thiessen
* sort sequences in reject dialog ; general identifier comparison
*
* Revision 1.42  2002/02/21 12:26:30  thiessen
* fix row delete bug ; remember threader options
*
* Revision 1.41  2002/02/19 14:59:39  thiessen
* add CDD reject and purge sequence
*
* Revision 1.40  2002/02/15 15:27:12  thiessen
* on Mac, use meta for MouseDown events
*
* Revision 1.39  2002/02/15 01:02:17  thiessen
* ctrl+click keeps edit modes
*
* Revision 1.38  2002/02/05 18:53:25  thiessen
* scroll to residue in sequence windows when selected in structure window
*
* Revision 1.37  2001/12/05 17:16:56  thiessen
* fix row insert bug
*
* Revision 1.36  2001/11/30 14:02:05  thiessen
* progress on sequence imports to single structures
*
* Revision 1.35  2001/11/27 16:26:08  thiessen
* major update to data management system
*
* Revision 1.34  2001/10/08 00:00:09  thiessen
* estimate threader N random starts; edit CDD name
*
* Revision 1.33  2001/09/27 15:37:59  thiessen
* decouple sequence import and BLAST
*
* Revision 1.32  2001/09/19 22:55:39  thiessen
* add preliminary net import and BLAST
*
* Revision 1.31  2001/08/24 14:54:50  thiessen
* make status row # order-independent :)
*
* Revision 1.30  2001/08/24 14:32:51  thiessen
* add row # to status line
*
* Revision 1.29  2001/08/08 02:25:27  thiessen
* add <memory>
*
* Revision 1.28  2001/07/10 16:39:55  thiessen
* change selection control keys; add CDD name/notes dialogs
*
* Revision 1.27  2001/06/21 02:02:34  thiessen
* major update to molecule identification and highlighting ; add toggle highlight (via alt)
*
* Revision 1.26  2001/06/07 19:05:38  thiessen
* functional (although incomplete) render settings panel ; highlight title - not sequence - upon mouse click
*
* Revision 1.25  2001/06/05 13:21:08  thiessen
* fix structure alignment list problems
*
* Revision 1.24  2001/06/04 14:58:00  thiessen
* add proximity sort; highlight sequence on browser launch
*
* Revision 1.23  2001/06/01 18:07:27  thiessen
* fix display clone bug
*
* Revision 1.22  2001/06/01 14:05:12  thiessen
* add float PDB sort
*
* Revision 1.21  2001/06/01 13:35:58  thiessen
* add aligned block number to status line
*
* Revision 1.20  2001/05/31 18:47:08  thiessen
* add preliminary style dialog; remove LIST_TYPE; add thread single and delete all; misc tweaks
*
* Revision 1.19  2001/05/22 19:09:31  thiessen
* many minor fixes to compile/run on Solaris/GTK
*
* Revision 1.18  2001/05/14 16:04:31  thiessen
* fix minor row reordering bug
*
* Revision 1.17  2001/05/11 02:10:42  thiessen
* add better merge fail indicators; tweaks to windowing/taskbar
*
* Revision 1.16  2001/05/09 17:15:06  thiessen
* add automatic block removal upon demotion
*
* Revision 1.15  2001/05/02 16:35:15  thiessen
* launch entrez web page on sequence identifier
*
* Revision 1.14  2001/04/19 12:58:32  thiessen
* allow merge and delete of individual updates
*
* Revision 1.13  2001/04/18 15:46:53  thiessen
* show description, length, and PDB numbering in status line
*
* Revision 1.12  2001/04/05 22:55:35  thiessen
* change bg color handling ; show geometry violations
*
* Revision 1.11  2001/04/04 00:27:14  thiessen
* major update - add merging, threader GUI controls
*
* Revision 1.10  2001/03/30 14:43:41  thiessen
* show threader scores in status line; misc UI tweaks
*
* Revision 1.9  2001/03/30 03:07:34  thiessen
* add threader score calculation & sorting
*
* Revision 1.8  2001/03/22 00:33:17  thiessen
* initial threading working (PSSM only); free color storage in undo stack
*
* Revision 1.7  2001/03/19 15:50:39  thiessen
* add sort rows by identifier
*
* Revision 1.6  2001/03/13 01:25:05  thiessen
* working undo system for >1 alignment (e.g., update window)
*
* Revision 1.5  2001/03/09 15:49:04  thiessen
* major changes to add initial update viewer
*
* Revision 1.4  2001/03/06 20:49:14  thiessen
* fix row->alignment bug
*
* Revision 1.3  2001/03/06 20:20:50  thiessen
* progress towards >1 alignment in a SequenceDisplay ; misc minor fixes
*
* Revision 1.2  2001/03/02 15:32:52  thiessen
* minor fixes to save & show/hide dialogs, wx string headers
*
* Revision 1.1  2001/03/01 20:15:51  thiessen
* major rearrangement of sequence viewer code into base and derived classes
*
* ===========================================================================
*/

#include <wx/string.h> // kludge for now to fix weird namespace conflict
#include <corelib/ncbistd.hpp>
#include <corelib/ncbistl.hpp>
#include <corelib/ncbistre.hpp>
#include <corelib/ncbi_limits.h>

#include <algorithm>
#include <memory>

#include "cn3d/sequence_display.hpp"
#include "cn3d/viewer_window_base.hpp"
#include "cn3d/viewer_base.hpp"
#include "cn3d/molecule.hpp"
#include "cn3d/messenger.hpp"
#include "cn3d/structure_set.hpp"
#include "cn3d/style_manager.hpp"
#include "cn3d/sequence_viewer_window.hpp"
#include "cn3d/sequence_viewer.hpp"
#include "cn3d/alignment_manager.hpp"
#include "cn3d/cn3d_colors.hpp"
#include "cn3d/update_viewer.hpp"
#include "cn3d/update_viewer_window.hpp"
#include "cn3d/cn3d_tools.hpp"
#include "cn3d/cn3d_threader.hpp"
#include "cn3d/conservation_colorer.hpp"
#include "cn3d/molecule_identifier.hpp"

USING_NCBI_SCOPE;


BEGIN_SCOPE(Cn3D)

// block marker string stuff
static const char
    blockLeftEdgeChar = '<',
    blockRightEdgeChar = '>',
    blockOneColumnChar = '^',
    blockInsideChar = '-';
static const std::string blockBoundaryStringTitle("(blocks)");

// color converter
static inline void Vector2wxColor(const Vector& colorVec, wxColor *colorWX)
{
    colorWX->Set(
        static_cast<unsigned char>((colorVec[0] + 0.000001) * 255),
        static_cast<unsigned char>((colorVec[1] + 0.000001) * 255),
        static_cast<unsigned char>((colorVec[2] + 0.000001) * 255)
    );
}


////////////////////////////////////////////////////////////////////////////////
// The sequence view is composed of rows which can be from sequence, alignment,
// or any string - anything derived from DisplayRow, implemented below.
////////////////////////////////////////////////////////////////////////////////

bool DisplayRowFromAlignment::GetCharacterTraitsAt(
    int column, BlockMultipleAlignment::eUnalignedJustification justification,
    char *character, Vector *color,
    bool *drawBackground, Vector *cellBackgroundColor) const
{
    bool isHighlighted,
        result = alignment->GetCharacterTraitsAt(
            column, row, justification, character, color,
            &isHighlighted, drawBackground, cellBackgroundColor);

    // always apply highlight color, even if alignment has set its own background color
    if (isHighlighted) {
        *drawBackground = true;
        *cellBackgroundColor = GlobalColors()->Get(Colors::eHighlight);
    }

    return result;
}

DisplayRowFromSequence::DisplayRowFromSequence(const Sequence *s, int from, int to) :
    sequence(s), fromIndex(from), toIndex(to)
{
    if (from < 0 || from >= sequence->Length() || from > to || to < 0 || to >= sequence->Length())
        ERR_POST(Error << "DisplayRowFromSequence::DisplayRowFromSequence() - from/to indexes out of range");
}

bool DisplayRowFromSequence::GetCharacterTraitsAt(
	int column, BlockMultipleAlignment::eUnalignedJustification justification,
    char *character, Vector *color, bool *drawBackground, Vector *cellBackgroundColor) const
{
    int index = column + fromIndex;
    if (index > toIndex)
        return false;

    *character = tolower(sequence->sequenceString[index]);
    if (sequence->molecule)
        *color = sequence->molecule->GetResidueColor(index);
    else
        color->Set(0,0,0);
    if (GlobalMessenger()->IsHighlighted(sequence, index)) {
        *drawBackground = true;
        *cellBackgroundColor = GlobalColors()->Get(Colors::eHighlight);
    } else {
        *drawBackground = false;
    }

    return true;
}

bool DisplayRowFromSequence::GetSequenceAndIndexAt(
    int column, BlockMultipleAlignment::eUnalignedJustification justification,
    const Sequence **sequenceHandle, int *seqIndex) const
{
    int index = column + fromIndex;
    if (index > toIndex)
        return false;

    *sequenceHandle = sequence;
    *seqIndex = index;
    return true;
}

void DisplayRowFromSequence::SelectedRange(int from, int to,
    BlockMultipleAlignment::eUnalignedJustification justification, bool toggle) const
{
    from += fromIndex;
    to += fromIndex;

    // skip if selected outside range
    if (from < fromIndex && to < fromIndex) return;
    if (from > toIndex && to > toIndex) return;

    // trim to within range
    if (from < fromIndex) from = fromIndex;
    else if (from > toIndex) from = toIndex;
    if (to < fromIndex) to = fromIndex;
    else if (to > toIndex) to = toIndex;

    if (toggle)
        GlobalMessenger()->ToggleHighlights(sequence, from, to);
    else
        GlobalMessenger()->AddHighlights(sequence, from, to);
}

bool DisplayRowFromString::GetCharacterTraitsAt(int column,
	BlockMultipleAlignment::eUnalignedJustification justification,
    char *character, Vector *color, bool *drawBackground, Vector *cellBackgroundColor) const
{
    if (column >= theString.size()) return false;

    *character = theString[column];
    *color = stringColor;

    if (hasBackgroundColor) {
        *drawBackground = true;
        *cellBackgroundColor = backgroundColor;
    } else {
        *drawBackground = false;
    }
    return true;
}


////////////////////////////////////////////////////////////////////////////////
// The SequenceDisplay is the structure that holds the DisplayRows of the view.
// It's also derived from ViewableAlignment, in order to be plugged into a
// SequenceDisplayWidget.
////////////////////////////////////////////////////////////////////////////////

SequenceDisplay::SequenceDisplay(bool editable, ViewerWindowBase* const *parentViewerWindow) :
    isEditable(editable), maxRowWidth(0), viewerWindow(parentViewerWindow), startingColumn(0)
{
}

SequenceDisplay::~SequenceDisplay(void)
{
    for (int i=0; i<rows.size(); i++) delete rows[i];
}

void SequenceDisplay::Empty(void)
{
    for (int i=0; i<rows.size(); i++) delete rows[i];
    rows.clear();
    startingColumn = maxRowWidth = 0;
}


SequenceDisplay * SequenceDisplay::Clone(const Old2NewAlignmentMap& newAlignments) const
{
    SequenceDisplay *copy = new SequenceDisplay(isEditable, viewerWindow);
    for (int row=0; row<rows.size(); row++)
        copy->rows.push_back(rows[row]->Clone(newAlignments));
    copy->startingColumn = startingColumn;
    copy->maxRowWidth = maxRowWidth;
    return copy;
}

void SequenceDisplay::AddRow(DisplayRow *row)
{
    rows.push_back(row);
    if (row->Width() > maxRowWidth) maxRowWidth = row->Width();
}

BlockMultipleAlignment * SequenceDisplay::GetAlignmentForRow(int row) const
{
    if (row < 0 || row >= rows.size()) return NULL;
    const DisplayRowFromAlignment *displayRow = dynamic_cast<const DisplayRowFromAlignment*>(rows[row]);
    if (displayRow) return displayRow->alignment;
    const DisplayRowFromString *stringRow = dynamic_cast<const DisplayRowFromString*>(rows[row]);
    if (stringRow) return stringRow->alignment;
    return NULL;
}

void SequenceDisplay::UpdateMaxRowWidth(void)
{
    RowVector::iterator r, re = rows.end();
    maxRowWidth = 0;
    for (r=rows.begin(); r!=re; r++)
        if ((*r)->Width() > maxRowWidth) maxRowWidth = (*r)->Width();
}

void SequenceDisplay::AddRowFromAlignment(int row, BlockMultipleAlignment *fromAlignment)
{
    if (!fromAlignment || row < 0 || row >= fromAlignment->NRows()) {
        ERR_POST(Error << "SequenceDisplay::AddRowFromAlignment() failed");
        return;
    }

    AddRow(new DisplayRowFromAlignment(row, fromAlignment));
}

void SequenceDisplay::AddRowFromSequence(const Sequence *sequence, int from, int to)
{
    if (!sequence) {
        ERR_POST(Error << "SequenceDisplay::AddRowFromSequence() failed");
        return;
    }

    AddRow(new DisplayRowFromSequence(sequence, from, to));
}

void SequenceDisplay::AddRowFromString(const std::string& anyString)
{
    AddRow(new DisplayRowFromString(anyString));
}

bool SequenceDisplay::GetRowTitle(int row, wxString *title, wxColour *color) const
{
    const DisplayRow *displayRow = rows[row];

    const DisplayRowFromString *strRow = dynamic_cast<const DisplayRowFromString*>(displayRow);
    if (strRow && !strRow->title.empty()) {
        *title = strRow->title.c_str();
        color->Set(0,0,0);      // black
        return true;
    }

    const Sequence *sequence = displayRow->GetSequence();
    if (!sequence) return false;

    // set title
    *title = sequence->identifier->ToString().c_str();

    // set color - by object if there's an associated molecule
    const DisplayRowFromAlignment *alnRow = dynamic_cast<const DisplayRowFromAlignment*>(displayRow);
    const Molecule *molecule;
    if (alnRow && (molecule=alnRow->alignment->GetSequenceOfRow(alnRow->row)->molecule) != NULL)
        Vector2wxColor(molecule->parentSet->styleManager->GetObjectColor(molecule), color);
    else
        color->Set(0,0,0);      // ... black otherwise

    return true;
}

bool SequenceDisplay::GetCharacterTraitsAt(int column, int row,
        char *character, wxColour *color, bool *drawBackground,
        wxColour *cellBackgroundColor) const
{
    if (row < 0 || row > rows.size()) {
        ERR_POST(Warning << "SequenceDisplay::GetCharacterTraitsAt() - row out of range");
        return false;
    }

    const DisplayRow *displayRow = rows[row];

    if (column >= displayRow->Width())
        return false;

    Vector colorVec, bgColorVec;
    if (!displayRow->GetCharacterTraitsAt(column,
            (*viewerWindow) ? (*viewerWindow)->GetCurrentJustification() : BlockMultipleAlignment::eLeft,
            character, &colorVec, drawBackground, &bgColorVec))
        return false;

    Vector2wxColor(colorVec, color);
    if (*drawBackground) Vector2wxColor(bgColorVec, cellBackgroundColor);
    return true;
}

void SequenceDisplay::MouseOver(int column, int row) const
{
    if (*viewerWindow) {
        wxString idLoc, status;

        if (row >= 0 && row < rows.size()) {
            const DisplayRow *displayRow = rows[row];
            const Sequence *sequence = NULL;

            // show id if we're in sequence area
            if (column >= 0 && column < displayRow->Width()) {

                // show title and seqloc
                int index = -1;
                if (displayRow->GetSequenceAndIndexAt(column,
                        (*viewerWindow)->GetCurrentJustification(), &sequence, &index)) {
                    if (index >= 0) {
                        wxString title;
                        wxColour color;
                        if (GetRowTitle(row, &title, &color))
                            idLoc.Printf("%s, loc %i", title.c_str(), index + 1); // show one-based numbering
                    }
                }

                // show PDB residue number if available (assume resID = index+1)
                if (sequence && index >= 0 && sequence->molecule) {
                    const Residue *residue = sequence->molecule->residues.find(index + 1)->second;
                    if (residue && residue->namePDB.size() > 0) {
                        wxString n = residue->namePDB.c_str();
                        n = n.Strip(wxString::both);
                        if (n.size() > 0)
                            idLoc = idLoc + " (PDB " + n + ")";
                    }
                }

                // show alignment block number and row-wise string
                const DisplayRowFromAlignment *alnRow =
                    dynamic_cast<const DisplayRowFromAlignment *>(displayRow);
                if (alnRow) {
                    int blockNum = alnRow->alignment->GetAlignedBlockNumber(column);
                    int rowNum = 0;
                    // get apparent row number (crude!)
                    for (int r=0; r<=row; r++) {
                        const DisplayRowFromAlignment *aRow =
                            dynamic_cast<const DisplayRowFromAlignment *>(rows[r]);
                        if (aRow && aRow->alignment == alnRow->alignment) rowNum++;
                    }
                    if (blockNum > 0)
                        status.Printf("Block %i, Row %i", blockNum, rowNum);
                    else
                        status.Printf("Row %i", rowNum);
                    if (alnRow->alignment->GetRowStatusLine(alnRow->row).size() > 0) {
                        status += " ; ";
                        status += alnRow->alignment->GetRowStatusLine(alnRow->row).c_str();
                    }
                }
            }

            // else show length and description if we're in the title area
            else if (column < 0) {
                sequence = displayRow->GetSequence();
                if (sequence) {
                    idLoc.Printf("length %i", sequence->Length());
                    status = sequence->description.c_str();
                }
            }
        }

        (*viewerWindow)->SetStatusText(idLoc, 0);
        (*viewerWindow)->SetStatusText(status, 1);
    }
}

void SequenceDisplay::UpdateAfterEdit(const BlockMultipleAlignment *forAlignment)
{
    UpdateBlockBoundaryRow(forAlignment);
    UpdateMaxRowWidth(); // in case alignment width has changed
    (*viewerWindow)->viewer->PushAlignment();
    if ((*viewerWindow)->AlwaysSyncStructures())
        (*viewerWindow)->SyncStructures();
}

bool SequenceDisplay::MouseDown(int column, int row, unsigned int controls)
{
    TESTMSG("got MouseDown at col:" << column << " row:" << row);

    // process events in title area (launch of browser for entrez page on a sequence)
    if (column < 0 && row >= 0 && row < NRows()) {
        const Sequence *seq = rows[row]->GetSequence();
        if (seq) seq->LaunchWebBrowserWithInfo();
        return false;
    }

    shiftDown = ((controls & ViewableAlignment::eShiftDown) > 0);
    controlDown = ((controls &
#ifdef __WXMAC__
        // on Mac, can't do ctrl-clicks, so use meta instead
        ViewableAlignment::eAltOrMetaDown
#else
        ViewableAlignment::eControlDown
#endif
            ) > 0);
    if (!shiftDown && !controlDown && column == -1)
        GlobalMessenger()->RemoveAllHighlights(true);

    // process events in sequence area
    BlockMultipleAlignment *alignment = GetAlignmentForRow(row);
    if (alignment && column >= 0) {

        // operations for any viewer window
        if ((*viewerWindow)->DoSplitBlock()) {
            if (alignment->SplitBlock(column)) {
                if (!controlDown) (*viewerWindow)->SplitBlockOff();
                UpdateAfterEdit(alignment);
            }
            return false;
        }

        if ((*viewerWindow)->DoDeleteBlock()) {
            if (alignment->DeleteBlock(column)) {
                if (!controlDown) (*viewerWindow)->DeleteBlockOff();
                UpdateAfterEdit(alignment);
            }
            return false;
        }

        // operations specific to the sequence window
        SequenceViewerWindow *sequenceWindow = dynamic_cast<SequenceViewerWindow*>(*viewerWindow);
        if (sequenceWindow && row >= 0) {

            if (sequenceWindow->DoMarkBlock()) {
                if (alignment->MarkBlock(column)) {
                    if (!controlDown) sequenceWindow->MarkBlockOff();
                    GlobalMessenger()->PostRedrawSequenceViewer(sequenceWindow->sequenceViewer);
                }
                return false;
            }

            if (sequenceWindow->DoProximitySort()) {
                if (ProximitySort(row)) {
                    sequenceWindow->ProximitySortOff();
                    GlobalMessenger()->PostRedrawSequenceViewer(sequenceWindow->sequenceViewer);
                }
                return false;
            }

            if (sequenceWindow->DoRealignRow() || sequenceWindow->DoDeleteRow()) {
                DisplayRowFromAlignment *selectedRow = dynamic_cast<DisplayRowFromAlignment*>(rows[row]);
                if (!selectedRow || selectedRow->row == 0 || !selectedRow->alignment) {
                    ERR_POST(Warning << "Can't delete/realign that row...");
                    return false;
                }

                if (sequenceWindow->DoRealignRow()) {
                    std::vector < int > selectedSlaves(1);
                    selectedSlaves[0] = selectedRow->row;
                    sequenceWindow->sequenceViewer->alignmentManager->
                        RealignSlaveSequences(alignment, selectedSlaves);
                    if (!controlDown) sequenceWindow->RealignRowOff();
                    return false;
                }

                if (sequenceWindow->DoDeleteRow()) {
                    if (alignment->NRows() <= 2) {
                        ERR_POST(Warning << "Can't delete that row...");
                        return false;
                    }

                    // in case we need to redraw molecule associated with removed row
                    const Molecule *molecule = alignment->GetSequenceOfRow(selectedRow->row)->molecule;

                    // delete row based on alignment row # (not display row #); redraw molecule
                    if (alignment->DeleteRow(selectedRow->row)) {

                        // delete this row from the display, and update higher row #'s
                        RowVector::iterator r, re = rows.end(), toDelete;
                        for (r=rows.begin(); r!=re; r++) {
                            DisplayRowFromAlignment
                                *currentARow = dynamic_cast<DisplayRowFromAlignment*>(*r);
                            if (!currentARow)
                                continue;
                            else if (currentARow->row > selectedRow->row)
                                (currentARow->row)--;
                            else if (currentARow == selectedRow)
                                toDelete = r;
                        }
                        delete *toDelete;
                        rows.erase(toDelete);

                        if (!controlDown) sequenceWindow->DeleteRowOff();
                        UpdateAfterEdit(alignment);
                        if (molecule && sequenceWindow->AlwaysSyncStructures())
                            GlobalMessenger()->PostRedrawMolecule(molecule);
                    }
                    return false;
                }
            }
        }

        // operations specific to the update window
        UpdateViewerWindow *updateWindow = dynamic_cast<UpdateViewerWindow*>(*viewerWindow);
        if (updateWindow && row >= 0) {

            // thread single
            if (updateWindow->DoThreadSingle()) {
                if (!updateWindow->updateViewer->alignmentManager->GetCurrentMultipleAlignment()) {
                    ERR_POST(Error << "Can't run threader without existing core alignment");
                    return false;
                }
                // get threader options
                globalThreaderOptions.nRandomStarts = Threader::EstimateNRandomStarts(
                    updateWindow->updateViewer->alignmentManager->GetCurrentMultipleAlignment(),
                    alignment);
                ThreaderOptionsDialog optDialog(updateWindow, globalThreaderOptions);
                if (optDialog.ShowModal() == wxCANCEL) return false;  // user cancelled
                if (!optDialog.GetValues(&globalThreaderOptions)) {
                    ERR_POST(Error << "Error retrieving options values from dialog");
                    return false;
                }
                updateWindow->updateViewer->alignmentManager->ThreadUpdate(globalThreaderOptions, alignment);
                updateWindow->ThreadSingleOff();
                return false;
            }

            // BLAST single
            if (updateWindow->DoBlastSingle()) {
                updateWindow->updateViewer->BlastUpdate(alignment, false);
                if (!controlDown) updateWindow->BlastSingleOff();
                return false;
            }

            // BLAST/PSSM single
            if (updateWindow->DoBlastPSSMSingle()) {
                updateWindow->updateViewer->BlastUpdate(alignment, true);
                if (!controlDown) updateWindow->BlastPSSMSingleOff();
                return false;
            }

            // merge single
            if (updateWindow->DoMergeSingle()) {
                AlignmentManager::UpdateMap single;
                single[alignment] = true;
                updateWindow->updateViewer->alignmentManager->MergeUpdates(single);
                if (!controlDown) updateWindow->MergeSingleOff();
                return false;
            }

            // delete single
            if (updateWindow->DoDeleteSingle()) {
                updateWindow->updateViewer->DeleteAlignment(alignment);
                if (!controlDown) updateWindow->DeleteSingleOff();
                return false;
            }
        }
    }

    return true;
}

void SequenceDisplay::SelectedRectangle(int columnLeft, int rowTop,
    int columnRight, int rowBottom)
{
    TESTMSG("got SelectedRectangle " << columnLeft << ',' << rowTop << " to "
        << columnRight << ',' << rowBottom);

	BlockMultipleAlignment::eUnalignedJustification justification =
		(*viewerWindow)->GetCurrentJustification();

    BlockMultipleAlignment *alignment = GetAlignmentForRow(rowTop);
    if (alignment && alignment == GetAlignmentForRow(rowBottom)) {
        if ((*viewerWindow)->DoMergeBlocks()) {
            if (alignment->MergeBlocks(columnLeft, columnRight)) {
                if (!controlDown) (*viewerWindow)->MergeBlocksOff();
                UpdateAfterEdit(alignment);
            }
            return;
        }
        if ((*viewerWindow)->DoCreateBlock()) {
            if (alignment->CreateBlock(columnLeft, columnRight, justification)) {
                if (!controlDown) (*viewerWindow)->CreateBlockOff();
                UpdateAfterEdit(alignment);
            }
            return;
        }
    }

    if (!shiftDown && !controlDown)
        GlobalMessenger()->RemoveAllHighlights(true);

    for (int i=rowTop; i<=rowBottom; i++)
        rows[i]->SelectedRange(columnLeft, columnRight, justification, controlDown); // toggle if control down
}

void SequenceDisplay::DraggedCell(int columnFrom, int rowFrom,
    int columnTo, int rowTo)
{
    TESTMSG("got DraggedCell " << columnFrom << ',' << rowFrom << " to "
        << columnTo << ',' << rowTo);
    if (rowFrom == rowTo && columnFrom == columnTo) return;
    if (rowFrom != rowTo && columnFrom != columnTo) return;     // ignore diagonal drag

    if (columnFrom != columnTo) {
        BlockMultipleAlignment *alignment = GetAlignmentForRow(rowFrom);
        if (alignment) {
            // process horizontal drag on special block boundary row
            DisplayRowFromString *strRow = dynamic_cast<DisplayRowFromString*>(rows[rowFrom]);
            if (strRow) {
                wxString title;
                wxColour ignored;
                if (GetRowTitle(rowFrom, &title, &ignored) && title == blockBoundaryStringTitle.c_str()) {
                    char ch = strRow->theString[columnFrom];
                    if (ch == blockRightEdgeChar || ch == blockLeftEdgeChar || ch == blockOneColumnChar) {
                        if (alignment->MoveBlockBoundary(columnFrom, columnTo))
                            UpdateAfterEdit(alignment);
                    }
                }
                return;
            }

            // process drag on regular row - block row shift
            DisplayRowFromAlignment *alnRow = dynamic_cast<DisplayRowFromAlignment*>(rows[rowFrom]);
            if (alnRow) {
                if (alignment->ShiftRow(alnRow->row, columnFrom, columnTo,
                        (*viewerWindow)->GetCurrentJustification()))
                    UpdateAfterEdit(alignment);
                return;
            }
        }
        return;
    }

    // use vertical drag to reorder row; move rowFrom so that it ends up just before the
    // initial rowTo row
    if (rowFrom == rowTo - 1) return;
    if (rowTo > rowFrom) rowTo--;
    RowVector newRows(rows);
    DisplayRow *row = newRows[rowFrom];
    RowVector::iterator r = newRows.begin();
    int i;
    for (i=0; i<rowFrom; i++) r++; // get iterator for position rowFrom
    newRows.erase(r);
    for (r=newRows.begin(), i=0; i<rowTo; i++) r++; // get iterator for position rowTo
    newRows.insert(r, row);

    // make sure that the master row of an alignment is still first
    bool masterOK = true;
    for (i=0; i<newRows.size(); i++) {
        DisplayRowFromAlignment *alnRow = dynamic_cast<DisplayRowFromAlignment*>(newRows[i]);
        if (alnRow) {
            if (alnRow->row != 0) {
                ERR_POST(Warning << "The first alignment row must always be the master sequence");
                masterOK = false;
            }
            break;
        }
    }
    if (masterOK) {
        rows = newRows;
        (*viewerWindow)->viewer->PushAlignment();   // make this an undoable operation
        GlobalMessenger()->PostRedrawAllSequenceViewers();
    }
}

void SequenceDisplay::RedrawAlignedMolecules(void) const
{
    for (int i=0; i<rows.size(); i++) {
        const Sequence *sequence = rows[i]->GetSequence();
        if (sequence && sequence->molecule)
            GlobalMessenger()->PostRedrawMolecule(sequence->molecule);
    }
}

DisplayRowFromString * SequenceDisplay::FindBlockBoundaryRow(const BlockMultipleAlignment *forAlignment) const
{
    DisplayRowFromString *blockBoundaryRow = NULL;
    for (int row=0; row<rows.size(); row++) {
        if ((blockBoundaryRow = dynamic_cast<DisplayRowFromString*>(rows[row])) != NULL) {
            if (blockBoundaryRow->alignment == forAlignment &&
                blockBoundaryRow->title == blockBoundaryStringTitle)
                break;
            else
                blockBoundaryRow = NULL;
        }
    }
    return blockBoundaryRow;
}

static inline DisplayRowFromString * CreateBlockBoundaryRow(BlockMultipleAlignment *forAlignment)
{
    return new DisplayRowFromString("", Vector(0,0,0),
        blockBoundaryStringTitle, true, Vector(0.8,0.8,1), forAlignment);
}

void SequenceDisplay::AddBlockBoundaryRows(void)
{
    if (!IsEditable()) return;

    // find alignment master rows
    int i = 0;
    std::map < const BlockMultipleAlignment * , bool > doneForAlignment;
    do {
        RowVector::iterator r;
        for (r=rows.begin(), i=0; i<rows.size(); r++, i++) {
            DisplayRowFromAlignment *alnRow = dynamic_cast<DisplayRowFromAlignment*>(*r);
            if (!alnRow || alnRow->row != 0 || !alnRow->alignment ||
                doneForAlignment.find(alnRow->alignment) != doneForAlignment.end()) continue;

            // insert block row before each master row
            DisplayRowFromString *blockBoundaryRow = CreateBlockBoundaryRow(alnRow->alignment);
            UpdateBlockBoundaryRow(blockBoundaryRow);
            rows.insert(r, blockBoundaryRow);
            doneForAlignment[alnRow->alignment] = true;
            break;  // insert on vector can cause invalidation of iterators, so start over
        }
    } while (i < rows.size());

    if (*viewerWindow) (*viewerWindow)->UpdateDisplay(this);
}

void SequenceDisplay::AddBlockBoundaryRow(BlockMultipleAlignment *forAlignment)
{
    DisplayRowFromString *blockBoundaryRow = CreateBlockBoundaryRow(forAlignment);
    AddRow(blockBoundaryRow);
    UpdateBlockBoundaryRow(blockBoundaryRow);
}

void SequenceDisplay::UpdateBlockBoundaryRow(const BlockMultipleAlignment *forAlignment) const
{
    DisplayRowFromString *blockBoundaryRow;
    if (!IsEditable() || (blockBoundaryRow = FindBlockBoundaryRow(forAlignment)) == NULL ||
        !blockBoundaryRow->alignment) return;
    UpdateBlockBoundaryRow(blockBoundaryRow);
}

void SequenceDisplay::UpdateBlockBoundaryRow(DisplayRowFromString *blockBoundaryRow) const
{
    if (!IsEditable() || !blockBoundaryRow || !blockBoundaryRow->alignment) return;

    int alignmentWidth = blockBoundaryRow->alignment->AlignmentWidth();
    blockBoundaryRow->theString.resize(alignmentWidth);

    // fill out block boundary marker string
    int blockColumn, blockWidth;
    for (int i=0; i<alignmentWidth; i++) {
        blockBoundaryRow->alignment->GetAlignedBlockPosition(i, &blockColumn, &blockWidth);
        if (blockColumn >= 0 && blockWidth > 0) {
            if (blockWidth == 1)
                blockBoundaryRow->theString[i] = blockOneColumnChar;
            else if (blockColumn == 0)
                blockBoundaryRow->theString[i] = blockLeftEdgeChar;
            else if (blockColumn == blockWidth - 1)
                blockBoundaryRow->theString[i] = blockRightEdgeChar;
            else
                blockBoundaryRow->theString[i] = blockInsideChar;
        } else
            blockBoundaryRow->theString[i] = ' ';
    }
    if (*viewerWindow) GlobalMessenger()->PostRedrawSequenceViewer((*viewerWindow)->viewer);
}

template < class T >
static void VectorRemoveElements(std::vector < T >& v, const std::vector < bool >& remove, int nToRemove)
{
    if (v.size() != remove.size()) {
#ifndef _DEBUG
		// MSVC gets internal compiler error here on debug builds... ugh!
        ERR_POST(Error << "VectorRemoveElements() - size mismatch");
#endif
        return;
    }

    std::vector < T > copy(v.size() - nToRemove);
    int i, nRemoved = 0;
    for (i=0; i<v.size(); i++) {
        if (remove[i])
            nRemoved++;
        else
            copy[i - nRemoved] = v[i];
    }
    if (nRemoved != nToRemove) {
#ifndef _DEBUG
        ERR_POST(Error << "VectorRemoveElements() - bad nToRemove");
#endif
        return;
    }

    v = copy;
}

void SequenceDisplay::RemoveBlockBoundaryRows(void)
{
    std::vector < bool > toRemove(rows.size(), false);
    int nToRemove = 0;
    for (int row=0; row<rows.size(); row++) {
        DisplayRowFromString *blockBoundaryRow = dynamic_cast<DisplayRowFromString*>(rows[row]);
        if (blockBoundaryRow && blockBoundaryRow->title == blockBoundaryStringTitle) {
            delete blockBoundaryRow;
            toRemove[row] = true;
            nToRemove++;
        }
    }
    VectorRemoveElements(rows, toRemove, nToRemove);
    UpdateMaxRowWidth();
    if (*viewerWindow) (*viewerWindow)->UpdateDisplay(this);
}

void SequenceDisplay::GetProteinSequences(SequenceList *seqs) const
{
    seqs->clear();
    for (int row=0; row<rows.size(); row++) {
        const Sequence *seq = rows[row]->GetSequence();
        if (seq && seq->isProtein)
            seqs->push_back(seq);
    }
}

void SequenceDisplay::GetSequences(const BlockMultipleAlignment *forAlignment, SequenceList *seqs) const
{
    seqs->clear();
    for (int row=0; row<rows.size(); row++) {
        DisplayRowFromAlignment *alnRow = dynamic_cast<DisplayRowFromAlignment*>(rows[row]);
        if (alnRow && alnRow->alignment == forAlignment)
            seqs->push_back(alnRow->alignment->GetSequenceOfRow(alnRow->row));
    }
}

void SequenceDisplay::GetRowOrder(
    const BlockMultipleAlignment *forAlignment, std::vector < int > *slaveRowOrder) const
{
    slaveRowOrder->clear();
    for (int row=0; row<rows.size(); row++) {
        DisplayRowFromAlignment *alnRow = dynamic_cast<DisplayRowFromAlignment*>(rows[row]);
        if (alnRow && alnRow->alignment == forAlignment)
            slaveRowOrder->push_back(alnRow->row);
    }
}

// comparison function: if CompareRows(a, b) == true, then row a moves up
typedef bool (*CompareRows)(const DisplayRowFromAlignment *a, const DisplayRowFromAlignment *b);

static bool CompareRowsByIdentifier(const DisplayRowFromAlignment *a, const DisplayRowFromAlignment *b)
{
    return MoleculeIdentifier::CompareIdentifiers(
        a->alignment->GetSequenceOfRow(a->row)->identifier,
        b->alignment->GetSequenceOfRow(b->row)->identifier);
}

static bool CompareRowsByScore(const DisplayRowFromAlignment *a, const DisplayRowFromAlignment *b)
{
    return (a->alignment->GetRowDouble(a->row) > b->alignment->GetRowDouble(b->row));
}

static bool CompareRowsFloatPDB(const DisplayRowFromAlignment *a, const DisplayRowFromAlignment *b)
{
    return (a->alignment->GetSequenceOfRow(a->row)->identifier->pdbID.size() > 0 &&
            b->alignment->GetSequenceOfRow(b->row)->identifier->pdbID.size() == 0);
}

static CompareRows rowComparisonFunction = NULL;

void SequenceDisplay::SortRowsByIdentifier(void)
{
    rowComparisonFunction = CompareRowsByIdentifier;
    SortRows();
    (*viewerWindow)->viewer->PushAlignment();   // make this an undoable operation
}

void SequenceDisplay::SortRowsByThreadingScore(double weightPSSM)
{
    if (!CalculateRowScoresWithThreader(weightPSSM)) return;
    rowComparisonFunction = CompareRowsByScore;
    SortRows();
    TESTMSG("sorted rows");
    (*viewerWindow)->viewer->PushAlignment();   // make this an undoable operation
}

void SequenceDisplay::FloatPDBRowsToTop(void)
{
    rowComparisonFunction = CompareRowsFloatPDB;
    SortRows();
    (*viewerWindow)->viewer->PushAlignment();   // make this an undoable operation
}

void SequenceDisplay::SortRows(void)
{
    if (!rowComparisonFunction) {
        ERR_POST(Error << "SequenceDisplay::SortRows() - must first set comparison function");
        return;
    }

    // to simplify sorting, construct list of slave rows only
    std::vector < DisplayRowFromAlignment * > slaves;
    int row;
    for (row=0; row<rows.size(); row++) {
        DisplayRowFromAlignment *alnRow = dynamic_cast<DisplayRowFromAlignment*>(rows[row]);
        if (alnRow && alnRow->row > 0)
            slaves.push_back(alnRow);
    }

    // do the sort
    stable_sort(slaves.begin(), slaves.end(), rowComparisonFunction);
    rowComparisonFunction = NULL;

    // recreate the row list with new order
    RowVector newRows(rows.size());
    int nSlaves = 0;
    for (row=0; row<rows.size(); row++) {
        DisplayRowFromAlignment *alnRow = dynamic_cast<DisplayRowFromAlignment*>(rows[row]);
        if (alnRow && alnRow->row > 0)
            newRows[row] = slaves[nSlaves++];   // put sorted slaves in place
        else
            newRows[row] = rows[row];           // leave other rows in original order
    }
    if (nSlaves == slaves.size())   // sanity check
        rows = newRows;
    else
        ERR_POST(Error << "SequenceDisplay::SortRows() - internal inconsistency");
}

bool SequenceDisplay::ProximitySort(int displayRow)
{
    DisplayRowFromAlignment *keyRow = dynamic_cast<DisplayRowFromAlignment*>(rows[displayRow]);
    if (!keyRow || keyRow->row == 0) return false;
    if (keyRow->alignment->NRows() < 3) return true;

    TESTMSG("doing Proximity Sort on alignment row " << keyRow->row);
    int row;
    auto_ptr<BlockMultipleAlignment::UngappedAlignedBlockList>
        blocks(keyRow->alignment->GetUngappedAlignedBlocks());
    BlockMultipleAlignment::UngappedAlignedBlockList::const_iterator b, be = blocks->end();
    const Sequence *seq1 = keyRow->alignment->GetSequenceOfRow(keyRow->row);
    std::vector < DisplayRowFromAlignment * > sortedByScore;

    // calculate scores for each row based on simple Blosum62 sum of all aligned pairs
    for (row=0; row<rows.size(); row++) {
        DisplayRowFromAlignment *alnRow = dynamic_cast<DisplayRowFromAlignment*>(rows[row]);
        if (!alnRow) continue;
        sortedByScore.push_back(alnRow);

        if (alnRow == keyRow) {
            keyRow->alignment->SetRowDouble(keyRow->row, kMax_Double);
            keyRow->alignment->SetRowStatusLine(keyRow->row, "(key row)");
        } else {
            const Sequence *seq2 = alnRow->alignment->GetSequenceOfRow(alnRow->row);
            double score = 0.0;
            for (b=blocks->begin(); b!=be; b++) {
                const Block::Range
                    *r1 = (*b)->GetRangeOfRow(keyRow->row),
                    *r2 = (*b)->GetRangeOfRow(alnRow->row);
                for (int i=0; i<(*b)->width; i++)
                    score += Blosum62Map
                        [seq1->sequenceString[r1->from + i]] [seq2->sequenceString[r2->from + i]];
            }
            alnRow->alignment->SetRowDouble(alnRow->row, score);
            wxString str;
            str.Printf("Score vs. key row: %i", (int) score);
            alnRow->alignment->SetRowStatusLine(alnRow->row, str.c_str());
        }
    }
    if (sortedByScore.size() != keyRow->alignment->NRows()) {
        ERR_POST(Error << "SequenceDisplay::ProximitySort() - wrong # rows in sort list");
        return false;
    }

    // sort by these scores
    stable_sort(sortedByScore.begin(), sortedByScore.end(), CompareRowsByScore);

    // find where the master row is in sorted list
    int M;
    for (M=0; M<sortedByScore.size(); M++) if (sortedByScore[M]->row == 0) break;

    // arrange by proximity to key row
    std::vector < DisplayRowFromAlignment * > arrangedByProximity(sortedByScore.size(), NULL);
    arrangedByProximity[0] = sortedByScore[M];  // move master back to top
    arrangedByProximity[M] = sortedByScore[0];  // move key row to M
    int i = 1, j = 1, N, R = 1;
    while (R < sortedByScore.size()) {
        N = M + i*j;    // iterate N = M+1, M-1, M+2, M-2, ...
        j = -j;
        if (j > 0) i++;
        if (N > 0 && N < sortedByScore.size() && !arrangedByProximity[N]) {
            if (R == M) R++;
            arrangedByProximity[N] = sortedByScore[R++];
        }
    }

    // recreate the row list with new order
    RowVector newRows(rows.size());
    int nNewRows = 0;
    for (row=0; row<rows.size(); row++) {
        DisplayRowFromAlignment *alnRow = dynamic_cast<DisplayRowFromAlignment*>(rows[row]);
        if (alnRow)
            newRows[row] = arrangedByProximity[nNewRows++];   // put arranged rows in place
        else
            newRows[row] = rows[row];           // leave other rows in original order
    }
    if (nNewRows == arrangedByProximity.size())   // sanity check
        rows = newRows;
    else
        ERR_POST(Error << "SequenceDisplay::ProximitySort() - internal inconsistency");

    // finally, highlight the key row and scroll approximately there
    GlobalMessenger()->RemoveAllHighlights(false);
    GlobalMessenger()->AddHighlights(seq1, 0, seq1->Length() - 1);
    (*viewerWindow)->ScrollToRow(M - 3);
    (*viewerWindow)->viewer->PushAlignment();   // make this an undoable operation
    return true;
}

bool SequenceDisplay::CalculateRowScoresWithThreader(double weightPSSM)
{
    if (isEditable) {
        SequenceViewer *seqViewer = dynamic_cast<SequenceViewer*>((*viewerWindow)->viewer);
        if (seqViewer) {
            seqViewer->alignmentManager->CalculateRowScoresWithThreader(weightPSSM);
            TESTMSG("calculated row scores");
            return true;
        }
    }
    return false;
}

void SequenceDisplay::RowsAdded(int nRowsAddedToMultiple, BlockMultipleAlignment *multiple)
{
    if (nRowsAddedToMultiple <= 0) return;

    // find the last row that's from this multiple
    int r, nRows = 0, lastAlnRowIndex;
    DisplayRowFromAlignment *lastAlnRow = NULL;
    for (r=0; r<rows.size(); r++) {
        DisplayRowFromAlignment *alnRow = dynamic_cast<DisplayRowFromAlignment*>(rows[r]);
        if (alnRow && alnRow->alignment == multiple) {
            lastAlnRow = alnRow;
            lastAlnRowIndex = r;
            nRows++;
        }
    }
    if (!lastAlnRow || multiple->NRows() != nRows + nRowsAddedToMultiple) {
        ERR_POST(Error << "SequenceDisplay::RowsAdded() - inconsistent parameters");
        return;
    }

    // move higher rows up to leave space for new rows
    nRows = rows.size() - 1 - lastAlnRowIndex;
    rows.resize(rows.size() + nRowsAddedToMultiple);
    for (r=0; r<nRows; r++)
        rows[rows.size() - 1 - r] = rows[rows.size() - 1 - r - nRows];

    // add new rows, assuming new rows in multiple are at the end of the multiple
    for (r=0; r<nRowsAddedToMultiple; r++)
        rows[lastAlnRowIndex + 1 + r] = new DisplayRowFromAlignment(
            multiple->NRows() + r - nRowsAddedToMultiple, multiple);

    UpdateAfterEdit(multiple);
}

void SequenceDisplay::RowsRemoved(const std::vector < int >& rowsRemoved,
    const BlockMultipleAlignment *multiple)
{
    if (rowsRemoved.size() == 0) return;

    // first, construct a map of old alignment row numbers -> new row numbers; also do sanity checks
    int i;
    vector < int > alnRowNumbers(multiple->NRows() + rowsRemoved.size());
    vector < bool > removedAlnRows(alnRowNumbers.size(), false);
    for (i=0; i<alnRowNumbers.size(); i++) alnRowNumbers[i] = i;
    for (i=0; i<rowsRemoved.size(); i++) {
        if (rowsRemoved[i] < 1 || rowsRemoved[i] >= alnRowNumbers.size()) {
            ERR_POST(Error << "SequenceDisplay::RowsRemoved() - can't remove row " << removedAlnRows[i]);
            return;
        } else
            removedAlnRows[rowsRemoved[i]] = true;
    }
    VectorRemoveElements(alnRowNumbers, removedAlnRows, rowsRemoved.size());
    std::map < int, int > oldRowToNewRow;
    for (i=0; i<alnRowNumbers.size(); i++) oldRowToNewRow[alnRowNumbers[i]] = i;

    // then tag rows to remove from display, and update row numbers for rows not removed
    vector < bool > removeDisplayRows(rows.size(), false);
    for (i=0; i<rows.size(); i++) {
        DisplayRowFromAlignment *alnRow = dynamic_cast<DisplayRowFromAlignment*>(rows[i]);
        if (alnRow && alnRow->alignment == multiple) {
            if (removedAlnRows[alnRow->row]) {
                delete rows[i];
                removeDisplayRows[i] = true;
            } else
                alnRow->row = oldRowToNewRow[alnRow->row];
        }
    }
    VectorRemoveElements(rows, removeDisplayRows, rowsRemoved.size());

    UpdateAfterEdit(multiple);
}

bool SequenceDisplay::GetDisplayCoordinates(const Molecule *molecule, int seqIndex,
    BlockMultipleAlignment::eUnalignedJustification justification, int *column, int *row)
{
    if (!molecule || seqIndex < 0) return false;

    int displayRow;
    const Sequence *seq;

    // search by Molecule*
    for (displayRow=0; displayRow<NRows(); displayRow++) {
        seq = rows[displayRow]->GetSequence();
        if (seq && seq->molecule == molecule) {
            *row = displayRow;

            const DisplayRowFromAlignment *alnRow = dynamic_cast<DisplayRowFromAlignment*>(rows[displayRow]);
            if (alnRow) {
                *column = alnRow->alignment->GetAlignmentIndex(alnRow->row, seqIndex, justification);
                return (*column >= 0);
            }

            const DisplayRowFromSequence *seqRow = dynamic_cast<DisplayRowFromSequence*>(rows[displayRow]);
            if (seqRow && seqIndex >= seqRow->fromIndex && seqIndex <= seqRow->toIndex) {
                *column = seqIndex - seqRow->fromIndex;
                return true;
            }
        }
    }
    return false;
}

END_SCOPE(Cn3D)

