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
*      progress meter window
*
* ===========================================================================
*/

#ifndef CN3D_PROGRESS_METER__HPP
#define CN3D_PROGRESS_METER__HPP

#include <corelib/ncbistd.hpp>
#include <corelib/ncbistl.hpp>
#include <corelib/ncbiobj.hpp>

#ifdef __WXMSW__
#include <windows.h>
#include <wx/msw/winundef.h>
#endif
#include <wx/wx.h>


BEGIN_SCOPE(Cn3D)

class ProgressMeter : public wxDialog
{
public:
    // this is intended to be used as a non-modal dialog; it will be shown right away upon construction
    ProgressMeter(wxWindow *parent, const wxString& message, const wxString& title, int maximumValue);

    // set gauge value
    void SetValue(int value, bool doYield = true);

private:
    void OnCloseWindow(wxCloseEvent& event);
    DECLARE_EVENT_TABLE()
};

END_SCOPE(Cn3D)

#endif // CN3D_PROGRESS_METER__HPP


/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.4  2003/02/03 19:20:05  thiessen
* format changes: move CVS Log to bottom of file, remove std:: from .cpp files, and use new diagnostic macros
*
* Revision 1.3  2002/08/15 22:13:16  thiessen
* update for wx2.3.2+ only; add structure pick dialog; fix MultitextDialog bug
*
* Revision 1.2  2001/10/24 17:07:30  thiessen
* add PNG output for wxGTK
*
* Revision 1.1  2001/10/23 13:53:40  thiessen
* add PNG export
*
*/
