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
*      dialog for setting styles
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.1  2001/05/31 18:46:28  thiessen
* add preliminary style dialog; remove LIST_TYPE; add thread single and delete all; misc tweaks
*
* ===========================================================================
*/

#ifndef CN3D_STYLE_DIALOG__HPP
#define CN3D_STYLE_DIALOG__HPP

#include <wx/string.h> // kludge for now to fix weird namespace conflict
#include <corelib/ncbistd.hpp>

#if defined(__WXMSW__)
#include <wx/msw/winundef.h>
#endif

#include <wx/wx.h>


BEGIN_SCOPE(Cn3D)

class StyleSettings;

class StyleDialog : public wxDialog
{
public:
    StyleDialog(wxWindow* parent, const StyleSettings& initialSettings);

    // set the StyleSettings from values in the panel; returns true if all values are valid
    bool GetValues(StyleSettings *settings);

private:

    void OnCloseWindow(wxCommandEvent& event);
    void OnButton(wxCommandEvent& event);
    void OnChange(wxCommandEvent& event);

    DECLARE_EVENT_TABLE()
};

END_SCOPE(Cn3D)

#endif // CN3D_STYLE_DIALOG__HPP
