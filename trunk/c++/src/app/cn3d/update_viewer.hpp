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
*      implementation of non-GUI part of update viewer
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.4  2001/04/04 00:27:22  thiessen
* major update - add merging, threader GUI controls
*
* Revision 1.3  2001/03/22 00:32:36  thiessen
* initial threading working (PSSM only); free color storage in undo stack
*
* Revision 1.2  2001/03/13 01:24:16  thiessen
* working undo system for >1 alignment (e.g., update window)
*
* Revision 1.1  2001/03/09 15:48:44  thiessen
* major changes to add initial update viewer
*
* ===========================================================================
*/

#ifndef CN3D_UPDATE_VIEWER__HPP
#define CN3D_UPDATE_VIEWER__HPP

#include <corelib/ncbistl.hpp>

#include <list>

#include "cn3d/viewer_base.hpp"


BEGIN_SCOPE(Cn3D)

class UpdateViewerWindow;
class BlockMultipleAlignment;
class AlignmentManager;

class UpdateViewer : public ViewerBase
{
    friend class UpdateViewerWindow;

public:

    UpdateViewer(AlignmentManager *alnMgr);
    ~UpdateViewer(void);

    void Refresh(void);             // refreshes the window only if present
    void CreateUpdateWindow(void);  // (re)creates the window

    // add new pairwise alignments to the update window
    typedef std::list < BlockMultipleAlignment * > AlignmentList;
    void AddAlignments(const AlignmentList& alignmentList);

    // replace contents of update window with given alignments
    void ReplaceAlignments(const AlignmentList& alignmentList);

    void OverrideBackgroundColor(int column, int row, bool *drawBackground, Vector *bgColorVec);

private:

    AlignmentManager *alignmentManager;
    UpdateViewerWindow *updateWindow;
};

END_SCOPE(Cn3D)

#endif // CN3D_UPDATE_VIEWER__HPP
