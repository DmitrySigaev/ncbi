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
* Revision 1.5  2001/03/19 15:47:37  thiessen
* add row sorting by identifier
*
* Revision 1.4  2001/03/13 01:24:15  thiessen
* working undo system for >1 alignment (e.g., update window)
*
* Revision 1.3  2001/03/09 15:48:43  thiessen
* major changes to add initial update viewer
*
* Revision 1.2  2001/03/06 20:20:43  thiessen
* progress towards >1 alignment in a SequenceDisplay ; misc minor fixes
*
* Revision 1.1  2001/03/01 20:15:29  thiessen
* major rearrangement of sequence viewer code into base and derived classes
*
* ===========================================================================
*/

#ifndef CN3D_SEQUENCE_DISPLAY__HPP
#define CN3D_SEQUENCE_DISPLAY__HPP

#include <corelib/ncbistd.hpp>
#include <corelib/ncbistl.hpp>

#include <map>

#include "cn3d/sequence_viewer_widget.hpp"
#include "cn3d/block_multiple_alignment.hpp"
#include "cn3d/sequence_set.hpp"


BEGIN_SCOPE(Cn3D)

typedef std::map < BlockMultipleAlignment *, BlockMultipleAlignment * > Old2NewAlignmentMap;

////////////////////////////////////////////////////////////////////////////////
// The sequence view is composed of rows which can be from sequence, alignment,
// or any string - anything derived from DisplayRow.
////////////////////////////////////////////////////////////////////////////////

class DisplayRow
{
public:
    virtual int Width(void) const = 0;
    virtual bool GetCharacterTraitsAt(int column, BlockMultipleAlignment::eUnalignedJustification justification,
        char *character, Vector *color, bool *drawBackground,
        wxColour *cellBackgroundColor) const = 0;
    virtual bool GetSequenceAndIndexAt(int column, BlockMultipleAlignment::eUnalignedJustification justification,
        const Sequence **sequence, int *index) const = 0;
    virtual const Sequence * GetSequence(void) const = 0;
    virtual void SelectedRange(int from, int to, BlockMultipleAlignment::eUnalignedJustification justification) const = 0;

    virtual DisplayRow * Clone(const Old2NewAlignmentMap& newAlignments) const = 0;

protected:
    // can't instantiate base class
    DisplayRow(void) { }
};

class DisplayRowFromAlignment : public DisplayRow
{
public:
    int row;
    BlockMultipleAlignment * const alignment;

    DisplayRowFromAlignment(int r, BlockMultipleAlignment *a) :
        row(r), alignment(a) { }

    int Width(void) const { return alignment->AlignmentWidth(); }

    DisplayRow * Clone(const Old2NewAlignmentMap& newAlignments) const
        { return new DisplayRowFromAlignment(row, newAlignments.find(alignment)->second); }

    bool GetCharacterTraitsAt(int column, BlockMultipleAlignment::eUnalignedJustification justification,
        char *character, Vector *color, bool *drawBackground, wxColour *cellBackgroundColor) const;

    bool GetSequenceAndIndexAt(int column, BlockMultipleAlignment::eUnalignedJustification justification,
        const Sequence **sequence, int *index) const
    {
		bool ignore;
        return alignment->GetSequenceAndIndexAt(column, row, justification, sequence, index, &ignore);
    }

    const Sequence * GetSequence(void) const
    {
        return alignment->GetSequenceOfRow(row);
    }

    void SelectedRange(int from, int to, BlockMultipleAlignment::eUnalignedJustification justification) const
    {
        alignment->SelectedRange(row, from, to, justification);
    }
};

class DisplayRowFromSequence : public DisplayRow
{
public:
    const Sequence * const sequence;

    DisplayRowFromSequence(const Sequence *s) : sequence(s) { }

    int Width(void) const { return sequence->sequenceString.size(); }

    DisplayRow * Clone(const Old2NewAlignmentMap& newAlignments) const
        { return new DisplayRowFromSequence(sequence); }

    bool GetCharacterTraitsAt(int column, BlockMultipleAlignment::eUnalignedJustification justification,
        char *character, Vector *color, bool *drawBackground, wxColour *cellBackgroundColor) const;

    bool GetSequenceAndIndexAt(int column, BlockMultipleAlignment::eUnalignedJustification justification,
        const Sequence **sequenceHandle, int *index) const;

    const Sequence * GetSequence(void) const
        { return sequence; }

    void SelectedRange(int from, int to, BlockMultipleAlignment::eUnalignedJustification justification) const;
};

class DisplayRowFromString : public DisplayRow
{
public:
    const std::string title;
    std::string theString;
    const Vector stringColor, backgroundColor;
    const bool hasBackgroundColor;
    BlockMultipleAlignment * const alignment;

    DisplayRowFromString(const std::string& s, const Vector color = Vector(0,0,0.5),
        const std::string& t = "", bool hasBG = false, Vector bgColor = (1,1,1),
        BlockMultipleAlignment *a = NULL) :
        theString(s), stringColor(color), title(t),
        hasBackgroundColor(hasBG), backgroundColor(bgColor), alignment(a) { }

    int Width(void) const { return theString.size(); }

    DisplayRow * Clone(const Old2NewAlignmentMap& newAlignments) const
        { return new DisplayRowFromString(
            theString, stringColor, title, hasBackgroundColor, backgroundColor,
            newAlignments.find(alignment)->second); }

    bool GetCharacterTraitsAt(int column, BlockMultipleAlignment::eUnalignedJustification justification,
        char *character, Vector *color, bool *drawBackground, wxColour *cellBackgroundColor) const;

    bool GetSequenceAndIndexAt(int column, BlockMultipleAlignment::eUnalignedJustification justification,
        const Sequence **sequenceHandle, int *index) const { return false; }

    const Sequence * GetSequence(void) const { return NULL; }

    void SelectedRange(int from, int to, BlockMultipleAlignment::eUnalignedJustification justification) const { } // do nothing
};


////////////////////////////////////////////////////////////////////////////////
// The SequenceDisplay is the structure that holds the DisplayRows of the view.
// It's also derived from ViewableAlignment, in order to be plugged into a
// SequenceViewerWidget.
////////////////////////////////////////////////////////////////////////////////

class SequenceViewer;
class ViewerWindowBase;

class SequenceDisplay : public ViewableAlignment
{
    friend class SequenceViewer;

public:
    SequenceDisplay(bool editable, ViewerWindowBase* const *parentViewerWindow);
    ~SequenceDisplay(void);

    // these functions add a row to the end of the display, from various sources
    void AddRowFromAlignment(int row, BlockMultipleAlignment *fromAlignment);
    void AddRowFromSequence(const Sequence *sequence);
    void AddRowFromString(const std::string& anyString);

    // adds a string row to the alignment, that contains block boundary indicators
    DisplayRowFromString * FindBlockBoundaryRow(const BlockMultipleAlignment *forAlignment) const;
    void UpdateBlockBoundaryRow(const BlockMultipleAlignment *forAlignment) const;
    void AddBlockBoundaryRow(BlockMultipleAlignment *forAlignment);  // adds to end of display!
    void AddBlockBoundaryRows(void);
    void RemoveBlockBoundaryRows(void);

    // returns a list of all sequences in the display (in order) - useful for single alignments only!
    typedef std::vector < const Sequence * > SequenceList;
    void GetSlaveSequences(SequenceList *seqs) const;

    // sorting functions - only for single-alignment displays!
    void SortRowsByIdentifier(void);

    // recreate the display from the given alignment.
    // NOTE: this assumes the display is from only a single alignment, and will put the block
    // boundary row at the top, regardless of where it was initially
    void RecreateFromEditedMultiple(BlockMultipleAlignment *multiple);

    // create a new copy of this object
    SequenceDisplay * Clone(const Old2NewAlignmentMap& newAlignments) const;

private:
    const bool isEditable;

    // generic row manipulation functions
    void AddRow(DisplayRow *row);
    BlockMultipleAlignment * SequenceDisplay::GetAlignmentForRow(int row) const;
    void UpdateBlockBoundaryRow(DisplayRowFromString *blockBoundaryRow) const;

    ViewerWindowBase* const *viewerWindow;

    int startingColumn;
    typedef std::vector < DisplayRow * > RowVector;
    RowVector rows;

    int maxRowWidth;
    void UpdateMaxRowWidth(void);
    void UpdateAfterEdit(const BlockMultipleAlignment *forAlignment);

    bool controlDown;

    void SortRows(void);

public:

    int NRows(void) const { return rows.size(); }
    bool IsEditable(void) const { return isEditable; }

    // redraw all molecules associated with the SequenceDisplay
    void RedrawAlignedMolecules(void) const;

    // column to scroll to when this display is first shown
    void SetStartingColumn(int column) { startingColumn = column; }
    int GetStartingColumn(void) const { return startingColumn; }

    // methods required by ViewableAlignment base class
    void GetSize(int *setColumns, int *setRows) const
        { *setColumns = maxRowWidth; *setRows = rows.size(); }
    bool GetRowTitle(int row, wxString *title, wxColour *color) const;
    bool GetCharacterTraitsAt(int column, int row,              // location
        char *character,										// character to draw
        wxColour *color,                                        // color of this character
        bool *drawBackground,                                   // special background color?
        wxColour *cellBackgroundColor
    ) const;

    // callbacks for ViewableAlignment
    void MouseOver(int column, int row) const;
    bool MouseDown(int column, int row, unsigned int controls);
    void SelectedRectangle(int columnLeft, int rowTop, int columnRight, int rowBottom);
    void DraggedCell(int columnFrom, int rowFrom, int columnTo, int rowTo);
};

END_SCOPE(Cn3D)

#endif // CN3D_SEQUENCE_DISPLAY__HPP
