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
*       dialog for CDD splash screen
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.3  2002/08/15 22:13:13  thiessen
* update for wx2.3.2+ only; add structure pick dialog; fix MultitextDialog bug
*
* Revision 1.2  2002/04/09 23:59:09  thiessen
* add cdd annotations read-only option
*
* Revision 1.1  2002/04/09 14:38:24  thiessen
* add cdd splash screen
*
* ===========================================================================
*/

#ifndef CN3D_CDD_SPLASH_DIALOG__HPP
#define CN3D_CDD_SPLASH_DIALOG__HPP

#include <corelib/ncbistd.hpp>
#include <corelib/ncbistl.hpp>

#ifdef __WXMSW__
#include <windows.h>
#include <wx/msw/winundef.h>
#endif
#include <wx/wx.h>


BEGIN_SCOPE(Cn3D)

class Cn3DMainFrame;
class StructureSet;

class CDDSplashDialog : public wxDialog
{
public:
    CDDSplashDialog(Cn3DMainFrame *cn3dFrame, StructureSet *structureSet,
        CDDSplashDialog **parentHandle,
        wxWindow* parent, wxWindowID id, const wxString& title,
        const wxPoint& pos = wxDefaultPosition);
    ~CDDSplashDialog(void);

private:
    CDDSplashDialog **handle;
    Cn3DMainFrame *structureWindow;
    StructureSet *sSet;

    // event callbacks
    void OnCloseWindow(wxCloseEvent& event);
    void OnButton(wxCommandEvent& event);

    DECLARE_EVENT_TABLE()
};

END_SCOPE(Cn3D)

#endif // CN3D_CDD_SPLASH_DIALOG__HPP
