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
*      implementation of non-GUI part of main sequence/alignment viewer
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.35  2001/03/01 20:15:51  thiessen
* major rearrangement of sequence viewer code into base and derived classes
*
* Revision 1.34  2001/02/13 01:03:57  thiessen
* backward-compatible domain ID's in output; add ability to delete rows
*
* Revision 1.33  2001/02/08 23:01:51  thiessen
* hook up C-toolkit stuff for threading; working PSSM calculation
*
* Revision 1.32  2001/01/26 19:29:59  thiessen
* limit undo stack size ; fix memory leak
*
* Revision 1.31  2000/12/29 19:23:39  thiessen
* save row order
*
* Revision 1.30  2000/12/21 23:42:16  thiessen
* load structures from cdd's
*
* Revision 1.29  2000/12/15 15:51:47  thiessen
* show/hide system installed
*
* Revision 1.28  2000/11/30 15:49:39  thiessen
* add show/hide rows; unpack sec. struc. and domain features
*
* Revision 1.27  2000/11/17 19:48:14  thiessen
* working show/hide alignment row
*
* Revision 1.26  2000/11/12 04:02:59  thiessen
* working file save including alignment edits
*
* Revision 1.25  2000/11/11 21:15:54  thiessen
* create Seq-annot from BlockMultipleAlignment
*
* Revision 1.24  2000/11/03 01:12:44  thiessen
* fix memory problem with alignment cloning
*
* Revision 1.23  2000/11/02 16:56:02  thiessen
* working editor undo; dynamic slave transforms
*
* Revision 1.22  2000/10/19 12:40:54  thiessen
* avoid multiple sequence redraws with scroll set
*
* Revision 1.21  2000/10/17 14:35:06  thiessen
* added row shift - editor basically complete
*
* Revision 1.20  2000/10/16 20:03:07  thiessen
* working block creation
*
* Revision 1.19  2000/10/12 19:20:45  thiessen
* working block deletion
*
* Revision 1.18  2000/10/12 16:22:45  thiessen
* working block split
*
* Revision 1.17  2000/10/12 02:14:56  thiessen
* working block boundary editing
*
* Revision 1.16  2000/10/05 18:34:43  thiessen
* first working editing operation
*
* Revision 1.15  2000/10/04 17:41:30  thiessen
* change highlight color (cell background) handling
*
* Revision 1.14  2000/10/03 18:59:23  thiessen
* added row/column selection
*
* Revision 1.13  2000/10/02 23:25:22  thiessen
* working sequence identifier window in sequence viewer
*
* Revision 1.12  2000/09/20 22:22:27  thiessen
* working conservation coloring; split and center unaligned justification
*
* Revision 1.11  2000/09/15 19:24:22  thiessen
* allow repeated structures w/o different local id
*
* Revision 1.10  2000/09/14 14:55:34  thiessen
* add row reordering; misc fixes
*
* Revision 1.9  2000/09/12 01:47:38  thiessen
* fix minor but obscure bug
*
* Revision 1.8  2000/09/11 22:57:33  thiessen
* working highlighting
*
* Revision 1.7  2000/09/11 14:06:29  thiessen
* working alignment coloring
*
* Revision 1.6  2000/09/11 01:46:16  thiessen
* working messenger for sequence<->structure window communication
*
* Revision 1.5  2000/09/08 20:16:55  thiessen
* working dynamic alignment views
*
* Revision 1.4  2000/09/07 17:37:35  thiessen
* minor fixes
*
* Revision 1.3  2000/09/03 18:46:49  thiessen
* working generalized sequence viewer
*
* Revision 1.2  2000/08/30 23:46:28  thiessen
* working alignment display
*
* Revision 1.1  2000/08/30 19:48:42  thiessen
* working sequence window
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>

#include "cn3d/sequence_viewer.hpp"
#include "cn3d/sequence_viewer_window.hpp"
#include "cn3d/sequence_display.hpp"
#include "cn3d/messenger.hpp"
#include "cn3d/alignment_manager.hpp"

USING_NCBI_SCOPE;


BEGIN_SCOPE(Cn3D)

SequenceViewer::SequenceViewer(AlignmentManager *alnMgr) :
    // not sure why this cast is necessary, but MSVC requires it...
    ViewerBase(reinterpret_cast<ViewerWindowBase**>(&sequenceWindow)),
    sequenceWindow(NULL), alignmentManager(alnMgr)
{
}

SequenceViewer::~SequenceViewer(void)
{
}

void SequenceViewer::CreateSequenceWindow(void)
{
    SequenceDisplay *display = GetCurrentDisplay();
    if (display) {
        if (!sequenceWindow) sequenceWindow = new SequenceViewerWindow(this);
        sequenceWindow->NewDisplay(display);
        sequenceWindow->ScrollToColumn(display->GetStartingColumn());
        sequenceWindow->Show(true);
        // ScrollTo causes immediate redraw, so don't need a second one
        GlobalMessenger()->UnPostRedrawSequenceViewer(this);
    }
}

void SequenceViewer::Refresh(void)
{
    if (sequenceWindow)
        sequenceWindow->Refresh();
    else
        CreateSequenceWindow();
}

void SequenceViewer::SaveDialog(void)
{
    if (sequenceWindow) sequenceWindow->SaveDialog(false);
}

void SequenceViewer::SaveAlignment(void)
{
    KeepOnlyStackTop();

    // go back into the original pairwise alignment data and save according to the
    // current edited BlockMultipleAlignment and display row order
    std::vector < int > rowOrder;
    const SequenceDisplay *display = GetCurrentDisplay();
    for (int i=0; i<display->rows.size(); i++) {
        DisplayRowFromAlignment *alnRow = dynamic_cast<DisplayRowFromAlignment*>(display->rows[i]);
        if (alnRow) rowOrder.push_back(alnRow->row);
    }
    alignmentManager->SavePairwiseFromMultiple(alignmentStack.back(), rowOrder);
}

void SequenceViewer::DisplayAlignment(BlockMultipleAlignment *alignment)
{
    ClearStacks();

    SequenceDisplay *display = new SequenceDisplay(true, viewerWindow);
    for (int row=0; row<alignment->NRows(); row++)
        display->AddRowFromAlignment(row, alignment);

    // set starting scroll to a few residues left of the first aligned block
    display->SetStartingColumn(alignment->GetFirstAlignedBlockPosition() - 5);

    InitStacks(alignment, display);
    if (sequenceWindow)
        sequenceWindow->UpdateDisplay(display);
    else
        CreateSequenceWindow();
}

void SequenceViewer::DisplaySequences(const SequenceList *sequenceList)
{
    ClearStacks();

    SequenceDisplay *display = new SequenceDisplay(false, viewerWindow);
    // populate each line of the display with one sequence, with blank lines inbetween
    SequenceList::const_iterator s, se = sequenceList->end();
    for (s=sequenceList->begin(); s!=se; s++) {
        if (s != sequenceList->begin()) display->AddRowFromString("");
        display->AddRowFromSequence(*s);
    }

    InitStacks(NULL, display);
    if (sequenceWindow)
        sequenceWindow->UpdateDisplay(display);
    else
        CreateSequenceWindow();
}

END_SCOPE(Cn3D)

