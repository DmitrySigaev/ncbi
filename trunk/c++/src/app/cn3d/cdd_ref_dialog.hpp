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
*       dialog for editing CDD references
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.2  2002/04/09 14:38:23  thiessen
* add cdd splash screen
*
* Revision 1.1  2001/10/09 18:57:26  thiessen
* add CDD references editing dialog
*
* ===========================================================================
*/

#ifndef CN3D_CDD_REF_DIALOG__HPP
#define CN3D_CDD_REF_DIALOG__HPP

#include <wx/string.h> // kludge for now to fix weird namespace conflict
#include <corelib/ncbistd.hpp>
#include <corelib/ncbistl.hpp>

#if defined(__WXMSW__)
#include <wx/msw/winundef.h>
#endif

#include <wx/wx.h>

#include <objects/cdd/Cdd_descr_set.hpp>


BEGIN_SCOPE(Cn3D)

class StructureSet;

class CDDRefDialog : public wxDialog
{
public:
    CDDRefDialog(StructureSet *structureSet, CDDRefDialog **handle,
        wxWindow* parent, wxWindowID id, const wxString& title,
        const wxPoint& pos = wxDefaultPosition);
    ~CDDRefDialog(void);

private:
    StructureSet *sSet;
    CDDRefDialog **dialogHandle;
    ncbi::objects::CCdd_descr_set *descrSet;

    void ResetListBox(void);

    // event callbacks
    void OnCloseWindow(wxCloseEvent& event);
    void OnButton(wxCommandEvent& event);

    DECLARE_EVENT_TABLE()
};

END_SCOPE(Cn3D)

#endif // CN3D_CDD_REF_DIALOG__HPP
