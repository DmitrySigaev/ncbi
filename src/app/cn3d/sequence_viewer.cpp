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
*      Classes to display sequences and alignments
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.2  2000/08/30 23:46:28  thiessen
* working alignment display
*
* Revision 1.1  2000/08/30 19:48:42  thiessen
* working sequence window
*
* ===========================================================================
*/

#include <wx/string.h> // kludge for now to fix weird namespace conflict
#include <corelib/ncbistd.hpp>

#include <wx/wx.h>

#include "cn3d/sequence_viewer.hpp"
#include "cn3d/sequence_viewer_gui.hpp"
#include "cn3d/alignment_manager.hpp"
#include "cn3d/sequence_set.hpp"

USING_NCBI_SCOPE;


BEGIN_SCOPE(Cn3D)

SequenceViewer::SequenceViewer(void) : viewerGUI(NULL), display(NULL)
{
}

SequenceViewer::~SequenceViewer(void)
{
    DestroyGUI();
    if (display) delete display;
}

void SequenceViewer::DestroyGUI(void)
{
    if (viewerGUI) {
        viewerGUI->Destroy();
        viewerGUI = NULL;
    }
}

void SequenceViewer::NewDisplay(const SequenceDisplay *display)
{
    if (!viewerGUI) viewerGUI = new SequenceViewerGUI(this);
    viewerGUI->NewDisplay(display);
}

static inline int RangeLength(const MultipleAlignment::Range& range)
{
    return (range.to - range.from + 1);
}

static const char UNALIGNED_GAP_CHAR = '~';

static void AddRightJustifiedUAR(SequenceDisplay::RowOfCells *row, int startAddingAt, 
    int length, const MultipleAlignment::Range& range, const Sequence *seq)
{
    int i, rangeLength = RangeLength(range);
    int padLength = length - rangeLength;
    for (i=0; i<padLength; i++) {
        row->at(startAddingAt + i).character = UNALIGNED_GAP_CHAR;
        row->at(startAddingAt + i).index = -1;
    }
    startAddingAt += padLength;
    for (i=0; i<rangeLength; i++) {
        row->at(startAddingAt + i).character = tolower(seq->sequenceString[range.from + i]);
        row->at(startAddingAt + i).index = range.from + i;
    }
}

static void AddLeftJustifiedUAR(SequenceDisplay::RowOfCells *row, int startAddingAt, 
    int length, const MultipleAlignment::Range& range, const Sequence *seq)
{
    int i, rangeLength = RangeLength(range);
    int padLength = length - rangeLength;
    for (i=0; i<rangeLength; i++) {
        row->at(startAddingAt + i).character = tolower(seq->sequenceString[range.from + i]);
        row->at(startAddingAt + i).index = range.from + i;
    }
    startAddingAt += rangeLength;
    for (i=0; i<padLength; i++) {
        row->at(startAddingAt + i).character = UNALIGNED_GAP_CHAR;
        row->at(startAddingAt + i).index = -1;
    }
}

void SequenceViewer::DisplayAlignment(const MultipleAlignment *multiple)
{
    if (display) delete display;
    display = new SequenceDisplay();

    SequenceDisplay::RowOfCells *row;

    typedef std::list < MultipleAlignment::Range > UnalignedRegionList;
    typedef std::list < UnalignedRegionList > UnalignedRegionLists;
    UnalignedRegionLists URs;
    typedef std::vector < int > MaxUnalignedLengths;
    MaxUnalignedLengths ULs(multiple->NBlocks() + 1, 0);

    // for each sequence, get the unaligned residues between each block (and at ends).
    // Also keep track of the maximum length of each unaligned region, across
    // all sequences, for padding with unaligned gaps.
    SequenceList::const_iterator s, se = multiple->sequences->end();
    MultipleAlignment::BlockList::const_iterator b, be = multiple->blocks.end(), bp1;
    int i = 0, length, j;
    for (s=multiple->sequences->begin(); s!=se; s++, i++) {

        // check for null-alignment
        if (multiple->NBlocks() == 0) {
            row = new SequenceDisplay::RowOfCells((*s)->Length());
            for (j=0; j < (*s)->Length(); j++) {
                row->at(j).character = tolower((*s)->sequenceString[j]);
                row->at(j).index = j;
            }
            display->AddRow(*s, row);
        }

        // real alignment
        else {
            URs.resize(URs.size() + 1);
            UnalignedRegionList& UR = URs.back();
            j = 0;

            for (b=multiple->blocks.begin(); b!=be; b++) {
                bp1 = b; bp1++;

                // left tail
                if (b == multiple->blocks.begin()) {
                    length = b->at(i).from;
                    UR.resize(UR.size() + 1);
                    UR.back().from = 0;
                    UR.back().to = length - 1;
                    if (length > ULs[j]) ULs[j] = length;
                    j++;
                }

                // right tail
                if (bp1 == multiple->blocks.end()) {
                    length = (*s)->sequenceString.size() - 1 - b->at(i).to;
                    UR.resize(UR.size() + 1);
                    UR.back().from = b->at(i).to + 1;
                    UR.back().to = UR.back().from + length - 1;
                    if (length > ULs[j]) ULs[j] = length;
                    break;
                }

                // between blocks
                else {
                    length = bp1->at(i).from - b->at(i).to - 1;
                    UR.resize(UR.size() + 1);
                    UR.back().from = b->at(i).to + 1;
                    UR.back().to = UR.back().from + length - 1;
                    if (length > ULs[j]) ULs[j] = length;
                    j++;
                }
            }
        }
    }

    // now we have all the info needed to create padded alignment strings
    if (multiple->NBlocks() > 0) {

        // calculate total alignment length
        int alignmentLength = 0;
        for (b=multiple->blocks.begin(), i=0; i<multiple->NBlocks(); b++, i++) {
            alignmentLength += ULs[i] + RangeLength(b->front());
        }
        alignmentLength += ULs[i]; // right tail

        // create display row for each sequence
        UnalignedRegionLists::const_iterator URs_i = URs.begin();
        for (i=0, s=multiple->sequences->begin(); s!=se; s++, URs_i++, i++) {
            UnalignedRegionList::const_iterator UR_i = URs_i->begin();
            MaxUnalignedLengths::const_iterator UL_i = ULs.begin();

            // allocate new display row
            row = new SequenceDisplay::RowOfCells(alignmentLength);
            TESTMSG("new row size " << row->size());

            int startAddingAt = 0, blockSize;
            for (b=multiple->blocks.begin(); b!=be; b++, UR_i++, UL_i++) {

                // add left tail
                if (b == multiple->blocks.begin()) { // always right-justified for left tail
                    AddRightJustifiedUAR(row, 0, *UL_i, *UR_i, *s);
                }
                // add unaligned region to the left of the current block
                else {
                    // should eventually have dynamic justification choice here...
                    AddRightJustifiedUAR(row, startAddingAt, *UL_i, *UR_i, *s);
                }
                startAddingAt += *UL_i;

                // add block
                blockSize = RangeLength(b->front());
                for (j=0; j<blockSize; j++) {
                    row->at(startAddingAt + j).character =
                        toupper((*s)->sequenceString[b->at(i).from + j]);
                    row->at(startAddingAt + j).index = b->at(i).from + j;
                }
                startAddingAt += blockSize;
            }

            // add right tail
            AddLeftJustifiedUAR(row, startAddingAt, *UL_i, *UR_i, *s);

            display->AddRow(*s, row);
        }
    }

    NewDisplay(display);
}

void SequenceViewer::DisplaySequences(const SequenceList *sequenceList)
{
    if (display) delete display;
    display = new SequenceDisplay();

    // populate each line of the display with one sequence - in lower case to
    // emphasize that they're unaligned
    SequenceList::const_iterator s, se = sequenceList->end();
    SequenceDisplay::RowOfCells *row;
    int i;
    for (s=sequenceList->begin(); s!=se; s++) {
        if (s != sequenceList->begin()) display->AddRow(NULL, NULL); // put blank lines inbetween
        row = new SequenceDisplay::RowOfCells((*s)->Length());
        for (i=0; i < (*s)->Length(); i++) {
            row->at(i).character = tolower((*s)->sequenceString[i]);
            row->at(i).index = i;
        }
        display->AddRow(*s, row);
    }

    NewDisplay(display);
}

END_SCOPE(Cn3D)


/////////////////////////////////////////////////////////////////////////////
// The stuff below here is the sequence viewer GUI (outside Cn3D namespace //
/////////////////////////////////////////////////////////////////////////////

SequenceDisplay::SequenceDisplay(void) : maxRowWidth(0)
{
}

SequenceDisplay::~SequenceDisplay(void)
{
    for (int i=0; i<rows.size(); i++) delete rows[i];
}

void SequenceDisplay::AddRow(const Cn3D::Sequence *sequence, RowOfCells *row)
{
    sequences.push_back(sequence);

    rows.resize(rows.size() + 1);
    rows.back() = row;

    if (row && row->size() > maxRowWidth) maxRowWidth = row->size();
}


BEGIN_EVENT_TABLE(SequenceViewerGUI, wxFrame)
END_EVENT_TABLE()

SequenceViewerGUI::SequenceViewerGUI(Cn3D::SequenceViewer *parent) :
    wxFrame(NULL, -1, "Cn3D++ Sequence Viewer", wxPoint(0,500), wxSize(500,200)),
    drawingArea(NULL), viewerParent(parent)
{
    drawingArea = new SequenceDrawingArea(this);
}

SequenceViewerGUI::~SequenceViewerGUI(void)
{
    viewerParent->viewerGUI = NULL; // make sure SequenceViewer knows the GUI is gone
    drawingArea->Destroy();
}

void SequenceViewerGUI::NewDisplay(const SequenceDisplay *newDisplay)
{
    drawingArea->NewDisplay(newDisplay);
    Show(true);
}


BEGIN_EVENT_TABLE(SequenceDrawingArea, wxScrolledWindow)
END_EVENT_TABLE()

SequenceDrawingArea::SequenceDrawingArea(SequenceViewerGUI *parent) :
    wxScrolledWindow(parent, -1, wxPoint(0,0), wxDefaultSize, wxHSCROLL | wxVSCROLL)
{
    // set up cell size based on the font
    font = wxFont(10, wxROMAN, wxNORMAL, wxNORMAL);
    wxClientDC dc(this);
    dc.SetFont(font);
    wxCoord chW, chH;
    dc.SetMapMode(wxMM_TEXT);
    dc.GetTextExtent("A", &chW, &chH);
    cellWidth = chW;
    cellHeight = chH;

    // set background
    dc.SetBackgroundMode(wxTRANSPARENT);    // no font background
}

void SequenceDrawingArea::NewDisplay(const SequenceDisplay *newDisplay)
{
    display = newDisplay;

    areaWidth = display->maxRowWidth;
    areaHeight = display->rows.size();

    // set size of virtual area
    SetScrollbars(cellWidth, cellHeight, areaWidth, areaHeight);
    Refresh(false);
}

void SequenceDrawingArea::OnDraw(wxDC& dc)
{
    int visX, visY, 
        updLeft, updRight, updTop, updBottom,
        firstCellX, firstCellY,
        lastCellX, lastCellY,
        x, y;

    // set font for characters
    dc.SetFont(font);

    // get the update rect list, so that we can draw *only* the cells 
    // in the part of the window that needs redrawing; update region
    // coordinates are relative to the visible part of the drawing area
    wxRegionIterator upd(GetUpdateRegion());

    for (; upd; upd++) {

        // figure out which cells contain the corners of the update region

        // get upper left corner of visible area
        GetViewStart(&visX, &visY);  // returns coordinates in scroll units (cells)
        visX *= cellWidth;
        visY *= cellHeight;

        // get coordinates of update region corners relative to virtual area
        updLeft = visX + upd.GetX();
        updTop = visY + upd.GetY();
        updRight = updLeft + upd.GetW() - 1;
        updBottom = updTop + upd.GetH() - 1;

        // draw background
        dc.SetPen(*wxWHITE_PEN);
        dc.SetBrush(*wxWHITE_BRUSH);
        dc.DrawRectangle(updLeft, updTop, upd.GetW(), upd.GetH());

        if (!display) continue;

        // firstCell[X,Y] is upper leftmost cell to draw, and is the cell
        // that contains the upper left corner of the update region
        firstCellX = updLeft / cellWidth;
        firstCellY = updTop / cellHeight;

        // lastCell[X,Y] is the lower rightmost cell displayed; including partial
        // cells if the visible area isn't an exact multiple of cell size. (It
        // turns out to be very difficult to only display complete cells...)
        lastCellX = updRight / cellWidth;
        lastCellY = updBottom / cellHeight;

        // restrict to size of virtual area, if visible area is larger
        // than the virtual area
        if (lastCellX >= areaWidth) lastCellX = areaWidth - 1;
        if (lastCellY >= areaHeight) lastCellY = areaHeight - 1;

        // draw cells
        for (x=firstCellX; x<=lastCellX; x++) {
            for (y=firstCellY; y<=lastCellY; y++) {
                DrawCell(dc, x, y);
            }
        }
    }
}

void SequenceDrawingArea::DrawCell(wxDC& dc, int x, int y)
{
    if (!display->rows[y] || x >= display->rows[y]->size()) return;

    // measure character size
    wxString buf(display->rows[y]->at(x).character);
    wxCoord chW, chH;
    dc.GetTextExtent(buf, &chW, &chH);

    // draw character in the middle of the cell
    dc.SetTextForeground(*wxBLACK);         // just black letters for now
    dc.DrawText(buf,
        x*cellWidth + (cellWidth - chW)/2,
        y*cellHeight + (cellHeight - chH)/2
    );
}
