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
*      Classes to display sequences and alignments with wxWindows front end
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.8  2000/09/14 14:55:35  thiessen
* add row reordering; misc fixes
*
* Revision 1.7  2000/09/12 01:47:39  thiessen
* fix minor but obscure bug
*
* Revision 1.6  2000/09/11 22:57:33  thiessen
* working highlighting
*
* Revision 1.5  2000/09/11 01:46:16  thiessen
* working messenger for sequence<->structure window communication
*
* Revision 1.4  2000/09/09 14:35:15  thiessen
* fix wxWin problem with large alignments
*
* Revision 1.3  2000/09/07 21:41:40  thiessen
* fix return type of min_max
*
* Revision 1.2  2000/09/07 17:37:35  thiessen
* minor fixes
*
* Revision 1.1  2000/09/03 18:46:49  thiessen
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

#include <wx/string.h> // kludge for name conflict
#include <corelib/ncbidiag.hpp>

#include "cn3d/sequence_viewer_widget.hpp"

USING_NCBI_SCOPE;


BEGIN_EVENT_TABLE(SequenceViewerWidget, wxScrolledWindow)
    EVT_PAINT               (SequenceViewerWidget::OnPaint)
    EVT_MOUSE_EVENTS        (SequenceViewerWidget::OnMouseEvent)
END_EVENT_TABLE()

SequenceViewerWidget::SequenceViewerWidget(
        wxWindow* parent,
        wxWindowID id,
        const wxPoint& pos,
        const wxSize& size,
        long style,
        const wxString& name
    ) :
    wxScrolledWindow(parent, -1, pos, size, style, name),
    alignment(NULL), currentFont(NULL)
{
    // set default background color
    currentBackgroundColor = *wxWHITE;

    // set default font
    SetCharacterFont(new wxFont(10, wxROMAN, wxNORMAL, wxNORMAL));

    // set default highlight color
    currentHighlightColor.Set(255,255,0);   // yellow

    // set default mouse mode
    mouseMode = eSelect;

    // set default rubber band color
    currentRubberbandColor = *wxRED;
}

SequenceViewerWidget::~SequenceViewerWidget(void)
{
    if (currentFont) delete currentFont;
}

bool SequenceViewerWidget::AttachAlignment(ViewableAlignment *newAlignment)
{
    alignment = newAlignment;

    if (alignment) {
        // set size of virtual area
        alignment->GetSize(&areaWidth, &areaHeight);
        if (areaWidth <= 0 || areaHeight <= 0) return false;

        // "+1" to make sure last real column and row are always visible, even 
        // if visible area isn't exact multiple of cell size
        SetScrollbars(
            cellWidth, cellHeight, 
            areaWidth + 1, areaHeight + 1);

        alignment->MouseOver(-1, -1);

    } else {
        // remove scrollbars
//        areaWidth = areaHeight = 2;
//        SetScrollbars(10, 10, 2, 2, 0, 0);   //can't do this without crash on win98... ?
    }

    return true;
}

void SequenceViewerWidget::SetHighlightColor(const wxColor& highlightColor)
{
    currentHighlightColor = highlightColor;
}

void SequenceViewerWidget::SetBackgroundColor(const wxColor& backgroundColor)
{
    currentBackgroundColor = backgroundColor;
}

void SequenceViewerWidget::SetCharacterFont(wxFont *font)
{
    if (!font) return;

    wxClientDC dc(this);
    dc.SetFont(wxNullFont);

    if (currentFont) delete currentFont;
    currentFont = font;

    dc.SetFont(*currentFont);
    wxCoord chW, chH;
    dc.SetMapMode(wxMM_TEXT);
    dc.GetTextExtent("A", &chW, &chH);
    cellWidth = chW + 1;
	cellHeight = chH;

    // need to reset scrollbars and virtual area size
    AttachAlignment(alignment);
}

void SequenceViewerWidget::SetMouseMode(eMouseMode mode)
{
    mouseMode = mode;
}

void SequenceViewerWidget::OnPaint(wxPaintEvent& event)
{
    ERR_POST(Info << "painting SequenceViewerWidget");

    wxPaintDC dc(this);

    int vsX, vsY, 
        updLeft, updRight, updTop, updBottom,
        firstCellX, firstCellY,
        lastCellX, lastCellY,
        x, y;

    dc.BeginDrawing();

    // characters to be drawn transparently over background
    dc.SetBackgroundMode(wxTRANSPARENT);

    // set font for characters
    if (alignment) dc.SetFont(*currentFont);

    // get the update rect list, so that we can draw *only* the cells 
    // in the part of the window that needs redrawing; update region
    // coordinates are relative to the visible part of the drawing area
    wxRegionIterator upd(GetUpdateRegion());

    for (; upd; upd++) {

        // draw background
        dc.SetPen(*(wxThePenList->
            FindOrCreatePen(currentBackgroundColor, 1, wxSOLID)));
        dc.SetBrush(*(wxTheBrushList->
            FindOrCreateBrush(currentBackgroundColor, wxSOLID)));
        dc.DrawRectangle(upd.GetX(), upd.GetY(), upd.GetW(), upd.GetH());

        if (!alignment) continue;

        // figure out which cells contain the corners of the update region

        // get upper left corner of visible area
        GetViewStart(&vsX, &vsY);  // returns coordinates in scroll units (cells)

        // get coordinates of update region corners relative to virtual area
        updLeft = vsX*cellWidth + upd.GetX();
        updTop = vsY*cellHeight + upd.GetY();
        updRight = updLeft + upd.GetW() - 1;
        updBottom = updTop + upd.GetH() - 1;

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
        for (y=firstCellY; y<=lastCellY; y++) {
            for (x=firstCellX; x<=lastCellX; x++) {
                DrawCell(dc, x, y, vsX, vsY, false);
            }
        }
    }

    dc.EndDrawing();
}

void SequenceViewerWidget::DrawCell(wxDC& dc, int x, int y, int vsX, int vsY, bool redrawBackground)
{
	char character;
    wxColor color;
    bool isHighlighted, drawChar;

    drawChar = alignment->GetCharacterTraitsAt(x, y, &character, &color, &isHighlighted);

    // adjust x,y into visible area coordinates
    x = (x - vsX) * cellWidth;
    y = (y - vsY) * cellHeight;

    // if necessary, redraw background with appropriate color
    if (isHighlighted || redrawBackground) {
        if (drawChar && isHighlighted) {
            dc.SetPen(*(wxThePenList->FindOrCreatePen(currentHighlightColor, 1, wxSOLID)));
            dc.SetBrush(*(wxTheBrushList->FindOrCreateBrush(currentHighlightColor, wxSOLID)));
        } else {
            dc.SetPen(*(wxThePenList->FindOrCreatePen(currentBackgroundColor, 1, wxSOLID)));
            dc.SetBrush(*(wxTheBrushList->FindOrCreateBrush(currentBackgroundColor, wxSOLID)));
        }
        dc.DrawRectangle(x, y, cellWidth, cellHeight);
    }

    if (!drawChar) return;

    // set character color
    dc.SetTextForeground(color);

    // measure character size
    wxString buf(character);
    wxCoord chW, chH;
    dc.GetTextExtent(buf, &chW, &chH);

    // draw character in the middle of the cell
    dc.DrawText(buf,
        x + (cellWidth - chW)/2,
        y + (cellHeight - chH)/2
    );
}

static inline void min_max(int a, int b, int *c, int *d)
{
    if (a <= b) {
        *c = a;
        *d = b;
    } else {
        *c = b;
        *d = a;
    }
}

void SequenceViewerWidget::DrawLine(wxDC& dc, int x1, int y1, int x2, int y2)
{
    if (currentRubberbandType == eSolid) {
        dc.DrawLine(x1, y1, x2, y2);
    } else { // short-dashed line
        int i, ie;
        if (x1 == x2) { // vertical line
            min_max(y1, y2, &i, &ie);
            ie -= 1;
            for (; i<=ie; i++)
                if (i%4 == 0) dc.DrawLine(x1, i, x1, i + 2);
        } else {        // horizontal line
            min_max(x1, x2, &i, &ie);
            ie -= 1;
            for (; i<=ie; i++)
                if (i%4 == 0) dc.DrawLine(i, y1, i + 2, y1);
        }
    }
}

// draw a rubberband around the cells
void SequenceViewerWidget::DrawRubberband(wxDC& dc, int fromX, int fromY, int toX, int toY, int vsX, int vsY)
{
    // find upper-left and lower-right corners
    int minX, minY, maxX, maxY;
    min_max(fromX, toX, &minX, &maxX);
    min_max(fromY, toY, &minY, &maxY);

    // convert to pixel coordinates of corners
    minX = (minX - vsX) * cellWidth;
    minY = (minY - vsY) * cellHeight;
    maxX = (maxX - vsX) * cellWidth + cellWidth - 1;
    maxY = (maxY - vsY) * cellHeight + cellHeight - 1;

    // set color
    dc.SetPen(*(wxThePenList->FindOrCreatePen(currentRubberbandColor, 1, wxSOLID)));

    // draw sides
    DrawLine(dc, minX, minY, maxX, minY);   // top
    DrawLine(dc, maxX, minY, maxX, maxY);   // right
    DrawLine(dc, maxX, maxY, minX, maxY);   // bottom
    DrawLine(dc, minX, maxY, minX, minY);   // left
}

// move the rubber band to a new rectangle, erasing only the side(s) of the
// rectangle that is changing
void SequenceViewerWidget::MoveRubberband(wxDC &dc, int fromX, int fromY, 
    int prevToX, int prevToY, int toX, int toY, int vsX, int vsY)
{
    int i;

    if ((prevToX >= fromX && toX < fromX) ||
        (prevToX < fromX && toX >= fromX) ||
        (prevToY >= fromY && toY < fromY) ||
        (prevToY < fromY && toY >= fromY)) {
        // need to completely redraw if rectangle is "flipped"
        RemoveRubberband(dc, fromX, fromY, prevToX, prevToY, vsX, vsY);

    } else {
        int a, b;

        // erase moving bottom/top side if dragging up/down
        if (toY != prevToY) {
            min_max(fromX, prevToX, &a, &b);
            for (i=a; i<=b; i++) DrawCell(dc, i, prevToY, vsX, vsY, true);
        }

        // erase partial top and bottom if dragging left by more than one
        a = -1; b = -2;
        if (fromX <= toX && toX < prevToX) {
            a = toX + 1;
            b = prevToX - 1;
        } else if (prevToX < toX && toX < fromX) {
            a = prevToX + 1;
            b = toX - 1;
        }
        for (i=a; i<=b; i++) {
            DrawCell(dc, i, fromY, vsX, vsY, true);
            DrawCell(dc, i, prevToY, vsX, vsY, true);
        }

        // erase moving left/right side
        if (toX != prevToX) {
            min_max(fromY, prevToY, &a, &b);
            for (i=a; i<=b; i++) DrawCell(dc, prevToX, i, vsX, vsY, true);
        }

        // erase partial left and right sides if dragging up/down by more than one
        a = -1; b = -2;
        if (fromY <= toY && toY < prevToY) {
            a = toY + 1;
            b = prevToY - 1;
        } else if (prevToY < toY && toY < fromY) {
            a = prevToY + 1;
            b = toY - 1;
        }
        for (i=a; i<=b; i++) {
            DrawCell(dc, fromX, i, vsX, vsY, true);
            DrawCell(dc, prevToX, i, vsX, vsY, true);
        }
    }

    // redraw whole new one
    DrawRubberband(dc, fromX, fromY, toX, toY, vsX, vsY);
}

// redraw only those cells necessary to remove rubber band
void SequenceViewerWidget::RemoveRubberband(wxDC& dc, int fromX, int fromY, int toX, int toY, int vsX, int vsY)
{
    int i, min, max;

    // remove top and bottom
    min_max(fromX, toX, &min, &max);
    for (i=min; i<=max; i++) {
        DrawCell(dc, i, fromY, vsX, vsY, true);
        DrawCell(dc, i, toY, vsX, vsY, true);
    }
    // remove left and right
    min_max(fromY, toY, &min, &max);
    for (i=min+1; i<=max-1; i++) {
        DrawCell(dc, fromX, i, vsX, vsY, true);
        DrawCell(dc, toX, i, vsX, vsY, true);
    }
}

void SequenceViewerWidget::OnMouseEvent(wxMouseEvent& event)
{
    static const ViewableAlignment *prevAlignment = NULL;
    static int prevMOX = -1, prevMOY = -1;
    static bool dragging = false;

    if (!alignment) {
        prevAlignment = NULL;
        prevMOX = prevMOY = -1;
        dragging = false;
        return;
    }
    if (alignment != prevAlignment) {
        prevMOX = prevMOY = -1;
        prevAlignment = alignment;
        dragging = false;
    }

    // get coordinates of mouse when it's over the drawing area
    wxCoord mX, mY;
    event.GetPosition(&mX, &mY);

    // translate visible area coordinates to cell coordinates
    int vsX, vsY, cellX, cellY, MOX, MOY;
    GetViewStart(&vsX, &vsY);
    cellX = MOX = vsX + mX / cellWidth;
    cellY = MOY = vsY + mY / cellHeight;

    // if the mouse is leaving the window, use cell coordinates of most
    // recent known mouse-over cell
    if (event.Leaving()) {
        cellX = prevMOX;
        cellY = prevMOY;
    }

    // do MouseOver if not in the same cell (or outside area) as last time
    if (MOX >= areaWidth || MOY >= areaHeight) 
        MOX = MOY = -1;
    if (MOX != prevMOX || MOY != prevMOY)
        alignment->MouseOver(MOX, MOY);
    prevMOX = MOX;
    prevMOY = MOY;

    // limit coordinates of selection to virtual area
    if (cellX < 0) cellX = 0;
    else if (cellX >= areaWidth) cellX = areaWidth - 1;
    if (cellY < 0) cellY = 0;
    else if (cellY >= areaHeight) cellY = areaHeight - 1;


    // keep track of position of selection start, as well as last
    // cell dragged to during selection
    static int fromX, fromY, prevToX, prevToY;

    // limit dragging movement if necessary
    if (dragging) {
        if (mouseMode == eDragHorizontal) cellY = fromY;
        if (mouseMode == eDragVertical) cellX = fromX;
    }

    // process beginning of selection
    if (event.LeftDown()) {

        // find out which (if any) control keys are down at this time
        unsigned int controls = 0;
        if (event.ShiftDown()) controls |= ViewableAlignment::eShiftDown;
        if (event.ControlDown()) controls |= ViewableAlignment::eControlDown;
        if (event.AltDown() || event.MetaDown()) controls |= ViewableAlignment::eAltOrMetaDown;

        // send MouseDown message
        alignment->MouseDown(MOX, MOY, controls);

        if (MOX != -1) {    // don't start selection if mouse-down is not inside display area
            prevToX = fromX = cellX;
            prevToY = fromY = cellY;
            dragging = true;

            ERR_POST(Info << "drawing initial rubberband");
            wxClientDC dc(this);
            dc.BeginDrawing();
            currentRubberbandType = (mouseMode == eSelect) ? eDot : eSolid;
            DrawRubberband(dc, fromX, fromY, fromX, fromY, vsX, vsY);
            dc.EndDrawing();
        }
    }
    
    // process end of selection, on mouse-up, or if the mouse leaves the window
    else if (dragging && (event.LeftUp() || event.Leaving() || event.Entering())) {
        if (!event.LeftUp()) {
            cellX = prevToX;
            cellY = prevToY;
        }
        dragging = false;
        wxClientDC dc(this);
        dc.BeginDrawing();
        dc.SetFont(*currentFont);

        // remove rubberband
        if (mouseMode == eSelect)
            RemoveRubberband(dc, fromX, fromY, cellX, cellY, vsX, vsY);
        else {
            DrawCell(dc, fromX, fromY, vsX, vsY, true);
            if (cellX != fromX || cellY != fromY)
                DrawCell(dc, cellX, cellY, vsX, vsY, true);
        }
        dc.EndDrawing();

        // do appropriate callback
        if (mouseMode == eSelect)
            alignment->SelectedRectangle(
                (fromX < cellX) ? fromX : cellX,
                (fromY < cellY) ? fromY : cellY,
                (cellX > fromX) ? cellX : fromX,
                (cellY > fromY) ? cellY : fromY);
        else
            alignment->DraggedCell(fromX, fromY, cellX, cellY);
    }

    // process continuation of selection - redraw rectangle
    else if (dragging && (cellX != prevToX || cellY != prevToY)) {
        wxClientDC dc(this);
        dc.BeginDrawing();
        dc.SetFont(*currentFont);
        currentRubberbandType = eDot;
        if (mouseMode == eSelect) {
            MoveRubberband(dc, fromX, fromY, prevToX, prevToY, cellX, cellY, vsX, vsY);
        } else {
            if (prevToX != fromX || prevToY != fromY)
                DrawCell(dc, prevToX, prevToY, vsX, vsY, true);
            if (cellX != fromX || cellY != fromY) {
                DrawRubberband(dc, cellX, cellY, cellX, cellY, vsX, vsY);
            }
        }
        dc.EndDrawing();
        prevToX = cellX;
        prevToY = cellY;
    }
}

