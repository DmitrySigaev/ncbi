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
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.55  2001/06/29 18:13:57  thiessen
* initial (incomplete) user annotation system
*
* Revision 1.54  2001/06/21 02:02:33  thiessen
* major update to molecule identification and highlighting ; add toggle highlight (via alt)
*
* Revision 1.53  2001/06/15 14:06:40  thiessen
* save/load asn styles now complete
*
* Revision 1.52  2001/06/14 17:45:10  thiessen
* progress in styles<->asn ; add structure limits
*
* Revision 1.51  2001/06/07 19:05:37  thiessen
* functional (although incomplete) render settings panel ; highlight title - not sequence - upon mouse click
*
* Revision 1.50  2001/06/02 19:14:08  thiessen
* Maximize() not implemented in wxGTK
*
* Revision 1.49  2001/05/31 18:47:07  thiessen
* add preliminary style dialog; remove LIST_TYPE; add thread single and delete all; misc tweaks
*
* Revision 1.48  2001/05/31 14:32:32  thiessen
* tweak font handling
*
* Revision 1.47  2001/05/25 18:15:13  thiessen
* another programDir fix
*
* Revision 1.46  2001/05/25 18:10:40  thiessen
* programDir path fix
*
* Revision 1.45  2001/05/25 15:18:23  thiessen
* fixes for visual id settings in wxGTK
*
* Revision 1.44  2001/05/24 19:13:23  thiessen
* fix to compile on SGI
*
* Revision 1.43  2001/05/22 19:09:30  thiessen
* many minor fixes to compile/run on Solaris/GTK
*
* Revision 1.42  2001/05/21 22:06:51  thiessen
* fix initial glcanvas size bug
*
* Revision 1.41  2001/05/18 19:17:22  thiessen
* get gl context working in GTK
*
* Revision 1.40  2001/05/17 18:34:25  thiessen
* spelling fixes; change dialogs to inherit from wxDialog
*
* Revision 1.39  2001/05/15 23:48:36  thiessen
* minor adjustments to compile under Solaris/wxGTK
*
* Revision 1.38  2001/05/15 17:33:53  thiessen
* log window stays hidden when closed
*
* Revision 1.37  2001/05/15 14:57:54  thiessen
* add cn3d_tools; bring up log window when threading starts
*
* Revision 1.36  2001/05/11 13:45:06  thiessen
* set up data directory
*
* Revision 1.35  2001/05/11 02:10:41  thiessen
* add better merge fail indicators; tweaks to windowing/taskbar
*
* Revision 1.34  2001/05/02 13:46:28  thiessen
* major revision of stuff relating to saving of updates; allow stored null-alignments
*
* Revision 1.33  2001/03/30 14:43:40  thiessen
* show threader scores in status line; misc UI tweaks
*
* Revision 1.32  2001/03/23 15:14:07  thiessen
* load sidechains in CDD's
*
* Revision 1.31  2001/03/22 00:33:16  thiessen
* initial threading working (PSSM only); free color storage in undo stack
*
* Revision 1.30  2001/03/13 01:25:05  thiessen
* working undo system for >1 alignment (e.g., update window)
*
* Revision 1.29  2001/03/09 15:49:04  thiessen
* major changes to add initial update viewer
*
* Revision 1.28  2001/03/06 20:20:50  thiessen
* progress towards >1 alignment in a SequenceDisplay ; misc minor fixes
*
* Revision 1.27  2001/03/02 15:32:52  thiessen
* minor fixes to save & show/hide dialogs, wx string headers
*
* Revision 1.26  2001/03/01 20:15:51  thiessen
* major rearrangement of sequence viewer code into base and derived classes
*
* Revision 1.25  2001/02/22 00:30:06  thiessen
* make directories global ; allow only one Sequence per StructureObject
*
* Revision 1.24  2001/02/13 20:33:49  thiessen
* add information content coloring
*
* Revision 1.23  2001/02/08 23:01:49  thiessen
* hook up C-toolkit stuff for threading; working PSSM calculation
*
* Revision 1.22  2001/01/18 19:37:28  thiessen
* save structure (re)alignments to asn output
*
* Revision 1.21  2000/12/29 19:23:38  thiessen
* save row order
*
* Revision 1.20  2000/12/22 19:26:40  thiessen
* write cdd output files
*
* Revision 1.19  2000/12/21 23:42:15  thiessen
* load structures from cdd's
*
* Revision 1.18  2000/12/20 23:47:47  thiessen
* load CDD's
*
* Revision 1.17  2000/12/15 15:51:47  thiessen
* show/hide system installed
*
* Revision 1.16  2000/11/30 15:49:38  thiessen
* add show/hide rows; unpack sec. struc. and domain features
*
* Revision 1.15  2000/11/13 18:06:52  thiessen
* working structure re-superpositioning
*
* Revision 1.14  2000/11/12 04:02:59  thiessen
* working file save including alignment edits
*
* Revision 1.13  2000/11/02 16:56:01  thiessen
* working editor undo; dynamic slave transforms
*
* Revision 1.12  2000/10/16 14:25:48  thiessen
* working alignment fit coloring
*
* Revision 1.11  2000/10/02 23:25:20  thiessen
* working sequence identifier window in sequence viewer
*
* Revision 1.10  2000/09/20 22:22:27  thiessen
* working conservation coloring; split and center unaligned justification
*
* Revision 1.9  2000/09/14 14:55:34  thiessen
* add row reordering; misc fixes
*
* Revision 1.8  2000/09/12 01:47:38  thiessen
* fix minor but obscure bug
*
* Revision 1.7  2000/09/11 14:06:28  thiessen
* working alignment coloring
*
* Revision 1.6  2000/09/11 01:46:14  thiessen
* working messenger for sequence<->structure window communication
*
* Revision 1.5  2000/09/03 18:46:48  thiessen
* working generalized sequence viewer
*
* Revision 1.4  2000/08/30 19:48:41  thiessen
* working sequence window
*
* Revision 1.3  2000/08/28 18:52:41  thiessen
* start unpacking alignments
*
* Revision 1.2  2000/08/27 18:52:20  thiessen
* extract sequence information
*
* Revision 1.1  2000/08/25 18:42:13  thiessen
* rename main object
*
* Revision 1.21  2000/08/25 14:22:00  thiessen
* minor tweaks
*
* Revision 1.20  2000/08/24 23:40:19  thiessen
* add 'atomic ion' labels
*
* Revision 1.19  2000/08/21 17:22:37  thiessen
* add primitive highlighting for testing
*
* Revision 1.18  2000/08/17 18:33:12  thiessen
* minor fixes to StyleManager
*
* Revision 1.17  2000/08/16 14:18:44  thiessen
* map 3-d objects to molecules
*
* Revision 1.16  2000/08/13 02:43:00  thiessen
* added helix and strand objects
*
* Revision 1.15  2000/08/07 14:13:15  thiessen
* added animation frames
*
* Revision 1.14  2000/08/04 22:49:03  thiessen
* add backbone atom classification and selection feedback mechanism
*
* Revision 1.13  2000/07/27 17:39:21  thiessen
* catch exceptions upon bad file read
*
* Revision 1.12  2000/07/27 13:30:51  thiessen
* remove 'using namespace ...' from all headers
*
* Revision 1.11  2000/07/18 16:50:11  thiessen
* more friendly rotation center setting
*
* Revision 1.10  2000/07/17 22:37:17  thiessen
* fix vector_math typo; correctly set initial view
*
* Revision 1.9  2000/07/17 04:20:49  thiessen
* now does correct structure alignment transformation
*
* Revision 1.8  2000/07/12 23:27:49  thiessen
* now draws basic CPK model
*
* Revision 1.7  2000/07/12 14:11:29  thiessen
* added initial OpenGL rendering engine
*
* Revision 1.6  2000/07/12 02:00:14  thiessen
* add basic wxWindows GUI
*
* Revision 1.5  2000/07/11 13:45:30  thiessen
* add modules to parse chemical graph; many improvements
*
* Revision 1.4  2000/07/01 15:43:50  thiessen
* major improvements to StructureBase functionality
*
* Revision 1.3  2000/06/29 19:17:47  thiessen
* improved atom map
*
* Revision 1.2  2000/06/29 16:46:07  thiessen
* use NCBI streams correctly
*
* Revision 1.1  2000/06/27 20:09:39  thiessen
* initial checkin
*
* ===========================================================================
*/

#include <wx/string.h> // kludge for now to fix weird namespace conflict

#include <corelib/ncbistd.hpp>
#include <corelib/ncbienv.hpp>
#include <objects/ncbimime/Ncbi_mime_asn1.hpp>
#include <objects/cdd/Cdd.hpp>

#include "cn3d/asn_reader.hpp"
#include "cn3d/cn3d_main_wxwin.hpp"
#include "cn3d/structure_set.hpp"
#include "cn3d/opengl_renderer.hpp"
#include "cn3d/style_manager.hpp"
#include "cn3d/messenger.hpp"
#include "cn3d/chemical_graph.hpp"
#include "cn3d/alignment_manager.hpp"
#include "cn3d/show_hide_manager.hpp"
#include "cn3d/show_hide_dialog.hpp"
#include "cn3d/cn3d_tools.hpp"

USING_NCBI_SCOPE;
USING_SCOPE(objects);


// from C-toolkit ncbienv.h - for setting registry parameter; used here to avoid having to
// bring in lots of C-toolkit headers
extern "C" {
    extern int Nlm_TransientSetAppParam(const char* file, const char* section, const char* type, const char* value);
}


// `Main program' equivalent, creating GUI framework
IMPLEMENT_APP(Cn3D::Cn3DApp)


BEGIN_SCOPE(Cn3D)

// global strings for various directories - will include trailing path separator character
std::string
    workingDir,     // current working directory
    userDir,        // directory of latest user-selected file
    programDir,     // directory where Cn3D executable lives
    dataDir;        // 'data' directory with external data files
const std::string& GetWorkingDir(void) { return workingDir; }
const std::string& GetUserDir(void) { return userDir; }
const std::string& GetProgramDir(void) { return programDir; }
const std::string& GetDataDir(void) { return dataDir; }

// top-level window (the main structure window)
static wxFrame *topWindow = NULL;
wxFrame * GlobalTopWindow(void) { return topWindow; }


// Set the NCBI diagnostic streams to go to this method, which then pastes them
// into a wxWindow. This log window can be closed anytime, but will be hidden,
// not destroyed. More serious errors will bring up a dialog, so that the user
// will get a chance to see the error before the program halts upon a fatal error.

class MsgFrame;

static MsgFrame *logFrame = NULL;

class MsgFrame : public wxFrame
{
public:
    MsgFrame(const wxString& title,
        const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize) :
        wxFrame(topWindow, wxID_HIGHEST + 5, title, pos, size, wxDEFAULT_FRAME_STYLE | wxFRAME_FLOAT_ON_PARENT) { }
    ~MsgFrame(void) { logFrame = NULL; logText = NULL; }
    wxTextCtrl *logText;
private:
    void OnCloseWindow(wxCloseEvent& event);
    DECLARE_EVENT_TABLE()
};

BEGIN_EVENT_TABLE(MsgFrame, wxFrame)
    EVT_CLOSE(MsgFrame::OnCloseWindow)
END_EVENT_TABLE()

void MsgFrame::OnCloseWindow(wxCloseEvent& event)
{
    if (event.CanVeto()) {
        Show(false);
        event.Veto();
    } else {
        Destroy();
    }
}

void DisplayDiagnostic(const SDiagMessage& diagMsg)
{
    std::string errMsg;
    diagMsg.Write(errMsg);

    if (diagMsg.m_Severity >= eDiag_Error && diagMsg.m_Severity != eDiag_Trace) {
        wxMessageDialog *dlg =
			new wxMessageDialog(NULL, errMsg.c_str(), "Severe Error!",
				wxOK | wxCENTRE | wxICON_EXCLAMATION);
        dlg->ShowModal();
		delete dlg;
    } else {
        if (!logFrame) {
            logFrame = new MsgFrame("Cn3D++ Message Log", wxPoint(500, 0), wxSize(500, 500));
            logFrame->SetSizeHints(150, 100);
            logFrame->logText = new wxTextCtrl(logFrame, -1, "",
                wxPoint(0,0), wxDefaultSize, wxTE_MULTILINE | wxTE_READONLY | wxHSCROLL);
        }
        // seems to be some upper limit on size, at least under MSW - so delete top of log if too big
        if (logFrame->logText->GetLastPosition() > 30000) logFrame->logText->Clear();
        *(logFrame->logText) << wxString(errMsg.data(), errMsg.size());
    }
}

void RaiseLogWindow(void)
{
    logFrame->logText->ShowPosition(logFrame->logText->GetLastPosition());
    logFrame->Show(true);
#if defined(__WXMSW__)
    if (logFrame->IsIconized()) logFrame->Maximize(false);
#endif
    logFrame->Raise();
}


BEGIN_EVENT_TABLE(Cn3DApp, wxApp)
    EVT_IDLE(Cn3DApp::OnIdle)
END_EVENT_TABLE()

Cn3DApp::Cn3DApp() : wxApp()
{
    // try to force all windows to use best (TrueColor) visuals
    SetUseBestVisual(true);
}

bool Cn3DApp::OnInit(void)
{
    // setup the diagnostic stream
    SetDiagHandler(DisplayDiagnostic, NULL, NULL);
    SetDiagPostLevel(eDiag_Info); // report all messages

    // create the main frame window
    structureWindow = new Cn3DMainFrame("Cn3D++", wxPoint(0,0), wxSize(500,500));
    SetTopWindow(structureWindow);

    // set up working directories
    workingDir = userDir = wxGetCwd().c_str();
    if (wxIsAbsolutePath(argv[0]))
        programDir = wxPathOnly(argv[0]).c_str();
    else if (wxPathOnly(argv[0]) == "")
        programDir = workingDir;
    else
        programDir = workingDir + wxFILE_SEP_PATH + wxPathOnly(argv[0]).c_str();
    workingDir = workingDir + wxFILE_SEP_PATH;
    programDir = programDir + wxFILE_SEP_PATH;

    // set data dir, and register the path in C toolkit registry (mainly for BLAST code)
    dataDir = programDir + "data" + wxFILE_SEP_PATH;
    Nlm_TransientSetAppParam("ncbi", "ncbi", "data", dataDir.c_str());

    TESTMSG("working dir: " << workingDir.c_str());
    TESTMSG("program dir: " << programDir.c_str());
    TESTMSG("data dir: " << dataDir.c_str());

    // read dictionary
    wxString dictFile = wxString(dataDir.c_str()) + "bstdt.val";
    LoadStandardDictionary(dictFile.c_str());

    // get file name from command line, if present
    if (argc > 2)
        ERR_POST(Fatal << "\nUsage: cn3d [filename]");
    else if (argc == 2)
        structureWindow->LoadFile(argv[1]);
    else
        structureWindow->glCanvas->renderer->AttachStructureSet(NULL);

    return true;
}

int Cn3DApp::OnExit(void)
{
    DeleteStandardDictionary();
	return 0;
}

void Cn3DApp::OnIdle(wxIdleEvent& event)
{
    GlobalMessenger()->ProcessRedraws();

    // call base class OnIdle to continue processing any other system idle-time stuff
    wxApp::OnIdle(event);
}


// data and methods for the main program window (a Cn3DMainFrame)

const int Cn3DMainFrame::UNLIMITED_STRUCTURES = -2;

BEGIN_EVENT_TABLE(Cn3DMainFrame, wxFrame)
    EVT_CLOSE(                                          Cn3DMainFrame::OnCloseWindow)
    EVT_MENU(       MID_EXIT,                           Cn3DMainFrame::OnExit)
    EVT_MENU(       MID_OPEN,                           Cn3DMainFrame::OnOpen)
    EVT_MENU(       MID_SAVE,                           Cn3DMainFrame::OnSave)
    EVT_MENU(       MID_LIMIT_STRUCT,                   Cn3DMainFrame::OnLimit)
    EVT_MENU_RANGE( MID_TRANSLATE,  MID_RESET,          Cn3DMainFrame::OnAdjustView)
    EVT_MENU_RANGE( MID_SHOW_HIDE,  MID_SHOW_SELECTED,  Cn3DMainFrame::OnShowHide)
    EVT_MENU(       MID_REFIT_ALL,                      Cn3DMainFrame::OnAlignStructures)
    EVT_MENU_RANGE( MID_EDIT_STYLE, MID_ANNOTATE,       Cn3DMainFrame::OnSetStyle)
    EVT_MENU_RANGE( MID_QLOW,       MID_QHIGH,          Cn3DMainFrame::OnSetQuality)
    EVT_MENU_RANGE( MID_SHOW_LOG,   MID_SHOW_SEQ_V,     Cn3DMainFrame::OnShowWindow)
END_EVENT_TABLE()

Cn3DMainFrame::Cn3DMainFrame(const wxString& title, const wxPoint& pos, const wxSize& size) :
    wxFrame(NULL, wxID_HIGHEST + 1, title, pos, size, wxDEFAULT_FRAME_STYLE | wxTHICK_FRAME),
    glCanvas(NULL), structureLimit(UNLIMITED_STRUCTURES)
{
    topWindow = this;
    SetSizeHints(150, 150); // min size

    TESTMSG("Welcome to Cn3D++! (built " << __DATE__ << ')');
    RaiseLogWindow();

    // File menu
    wxMenuBar *menuBar = new wxMenuBar;
    wxMenu *menu = new wxMenu;
    menu->Append(MID_OPEN, "&Open");
    menu->Append(MID_SAVE, "&Save");
    menu->AppendSeparator();
    menu->Append(MID_LIMIT_STRUCT, "&Limit Structures");
    menu->AppendSeparator();
    menu->Append(MID_EXIT, "E&xit");
    menuBar->Append(menu, "&File");

    // View menu
    menu = new wxMenu;
    menu->Append(MID_TRANSLATE, "&Translate");
    menu->Append(MID_ZOOM_IN, "Zoom &In");
    menu->Append(MID_ZOOM_OUT, "Zoom &Out");
    menu->Append(MID_RESET, "&Reset");
    menuBar->Append(menu, "&View");

    // Show-Hide menu
    menu = new wxMenu;
    menu->Append(MID_SHOW_HIDE, "&Pick Structures...");
    menu->Append(MID_SHOW_ALL, "Show &Everything");
    menu->Append(MID_SHOW_DOMAINS, "Show Aligned &Domains");
    menu->Append(MID_SHOW_ALIGNED, "Show &Aligned Residues");
    menu->Append(MID_SHOW_UNALIGNED, "Show &Unaligned Residues");
    menu->Append(MID_SHOW_SELECTED, "Show &Selected Residues");
    menuBar->Append(menu, "Show/&Hide");

    // Structure Alignment menu
    menu = new wxMenu;
    menu->Append(MID_REFIT_ALL, "Recompute &All");
    menuBar->Append(menu, "Structure &Alignments");

    // Style menu
    menu = new wxMenu;
    menu->Append(MID_EDIT_STYLE, "Edit &Global Style");
    wxMenu *subMenu = new wxMenu;
    subMenu->Append(MID_WORM, "&Worms");
    subMenu->Append(MID_TUBE, "&Tubes");
    subMenu->Append(MID_WIRE, "Wir&e");
    menu->Append(MID_RENDER, "&Rendering Shortcuts", subMenu);
    subMenu = new wxMenu;
    subMenu->Append(MID_SECSTRUC, "Sec&ondary Structure");
    subMenu->Append(MID_ALIGNED, "&Aligned");
    subMenu->Append(MID_INFO, "&Information Content");
    menu->Append(MID_COLORS, "&Coloring Shortcuts", subMenu);
    menu->AppendSeparator();
    menu->Append(MID_ANNOTATE, "A&nnotate");
    menuBar->Append(menu, "&Style");

    // Quality menu
    menu = new wxMenu;
    menu->Append(MID_QLOW, "&Low");
    menu->Append(MID_QMED, "&Medium");
    menu->Append(MID_QHIGH, "&High");
    menuBar->Append(menu, "&Quality");

    // Window menu
    menu = new wxMenu;
    menu->Append(MID_SHOW_SEQ_V, "Show &Sequence Viewer");
    menu->Append(MID_SHOW_LOG, "Show Message &Log");
    menuBar->Append(menu, "&Window");

    SetMenuBar(menuBar);

    // Make a GLCanvas
#if defined(__WXMSW__)
    int *attribList = NULL;
#elif defined(__WXGTK__)
    int *attribList = NULL;
/*
    int attribList[20] = {
        WX_GL_DOUBLEBUFFER,
        WX_GL_RGBA, WX_GL_MIN_RED, 1, WX_GL_MIN_GREEN, 1, WX_GL_MIN_BLUE, 1,
        WX_GL_ALPHA_SIZE, 0, WX_GL_DEPTH_SIZE, 1,
        None
    };
*/
#else
#error need to define GL attrib list
#endif
    glCanvas = new Cn3DGLCanvas(this, attribList);

    GlobalMessenger()->AddStructureWindow(this);
    Show(true);

    // set up font used by OpenGL
    glCanvas->SetCurrent();
#if defined(__WXMSW__)
    glCanvas->font = new wxFont(12, wxSWISS, wxNORMAL, wxBOLD);
    glCanvas->renderer->SetFont_Windows(glCanvas->font->GetHFONT());
#elif defined(__WXGTK__)
    glCanvas->font = new wxFont(14, wxSWISS, wxNORMAL, wxBOLD);
    glCanvas->renderer->SetFont_GTK(glCanvas->font->GetInternalFont());
#endif
}

Cn3DMainFrame::~Cn3DMainFrame(void)
{
    if (logFrame) {
        logFrame->Destroy();
        logFrame = NULL;
    }
}

void Cn3DMainFrame::OnShowWindow(wxCommandEvent& event)
{
    switch (event.GetId()) {
        case MID_SHOW_LOG:
            RaiseLogWindow();
            break;
        case MID_SHOW_SEQ_V:
            GlobalMessenger()->PostRedrawAllSequenceViewers();
            break;
    }
}

void Cn3DMainFrame::OnCloseWindow(wxCloseEvent& event)
{
    Command(MID_EXIT);
}

void Cn3DMainFrame::OnExit(wxCommandEvent& event)
{
    GlobalMessenger()->RemoveStructureWindow(this); // don't bother with any redraws since we're exiting
    GlobalMessenger()->SequenceWindowsSave();       // save any edited alignment and updates first
    SaveDialog(false);                              // give structure window a chance to save data
    Destroy();
}

bool Cn3DMainFrame::SaveDialog(bool canCancel)
{
    // check for whether save is necessary
    if (!glCanvas->structureSet || !glCanvas->structureSet->HasDataChanged())
        return true;

    int option = wxYES_NO | wxYES_DEFAULT | wxICON_EXCLAMATION | wxCENTRE;
    if (canCancel) option |= wxCANCEL;

    wxMessageDialog dialog(NULL, "Do you want to save your work to a file?", "", option);
    option = dialog.ShowModal();

    if (option == wxID_CANCEL) return false; // user cancelled this operation

    if (option == wxID_YES) {
        wxCommandEvent event;
        OnSave(event);    // save data
    }

    return true;
}

void Cn3DMainFrame::OnAdjustView(wxCommandEvent& event)
{
    glCanvas->SetCurrent();
    switch (event.GetId()) {
        case MID_TRANSLATE:
            glCanvas->renderer->ChangeView(OpenGLRenderer::eXYTranslateHV, 25, 25);
            break;
        case MID_ZOOM_IN:
            glCanvas->renderer->ChangeView(OpenGLRenderer::eZoomIn);
            break;
        case MID_ZOOM_OUT:
            glCanvas->renderer->ChangeView(OpenGLRenderer::eZoomOut);
            break;
        case MID_RESET:
            glCanvas->renderer->ResetCamera();
            break;
        default: ;
    }
    glCanvas->Refresh(false);
}

void Cn3DMainFrame::OnShowHide(wxCommandEvent& event)
{
    if (glCanvas->structureSet) {

        switch (event.GetId()) {

        case MID_SHOW_HIDE:
        {
            std::vector < std::string > structureNames;
            std::vector < bool > structureVisibilities;
            glCanvas->structureSet->showHideManager->GetShowHideInfo(&structureNames, &structureVisibilities);
            wxString *titles = new wxString[structureNames.size()];
            for (int i=0; i<structureNames.size(); i++) titles[i] = structureNames[i].c_str();

            ShowHideDialog dialog(
                titles,
                &structureVisibilities,
                glCanvas->structureSet->showHideManager,
                NULL, -1, "Show/Hide Structures", wxPoint(400, 50), wxSize(200, 300));
            dialog.ShowModal();
    //        delete titles;    // apparently deleted by wxWindows
            break;
        }

        case MID_SHOW_ALL:
            glCanvas->structureSet->showHideManager->MakeAllVisible();
            break;
        case MID_SHOW_DOMAINS:
            glCanvas->structureSet->showHideManager->ShowAlignedDomains(glCanvas->structureSet);
            break;
        case MID_SHOW_ALIGNED:
            glCanvas->structureSet->showHideManager->ShowResidues(glCanvas->structureSet, true);
            break;
        case MID_SHOW_UNALIGNED:
            glCanvas->structureSet->showHideManager->ShowResidues(glCanvas->structureSet, false);
            break;
        case MID_SHOW_SELECTED:
            glCanvas->structureSet->showHideManager->ShowSelectedResidues(glCanvas->structureSet);
            break;
        }
    }
}

void Cn3DMainFrame::OnAlignStructures(wxCommandEvent& event)
{
    if (glCanvas->structureSet) {
        glCanvas->structureSet->alignmentManager->RealignAllSlaveStructures();
        glCanvas->SetCurrent();
        glCanvas->Refresh(false);
    }
}

void Cn3DMainFrame::OnSetStyle(wxCommandEvent& event)
{
    if (glCanvas->structureSet) {
        glCanvas->SetCurrent();
        switch (event.GetId()) {
            case MID_EDIT_STYLE:
                if (!glCanvas->structureSet->styleManager->EditGlobalStyle(this, glCanvas->structureSet))
                    return;
                break;
            case MID_ANNOTATE:
                if (!glCanvas->structureSet->styleManager->EditUserAnnotations(this, glCanvas->structureSet))
                    return;
                break;
            case MID_WORM:
                glCanvas->structureSet->styleManager->SetGlobalRenderingStyle(StyleSettings::eWormDisplay);
                break;
            case MID_TUBE:
                glCanvas->structureSet->styleManager->SetGlobalRenderingStyle(StyleSettings::eTubeDisplay);
                break;
            case MID_WIRE:
                glCanvas->structureSet->styleManager->SetGlobalRenderingStyle(StyleSettings::eWireframeDisplay);
                break;
            case MID_SECSTRUC:
                glCanvas->structureSet->styleManager->SetGlobalColorScheme(StyleSettings::eBySecondaryStructure);
                break;
            case MID_ALIGNED:
                glCanvas->structureSet->styleManager->SetGlobalColorScheme(StyleSettings::eByAligned);
                break;
            case MID_INFO:
                glCanvas->structureSet->styleManager->SetGlobalColorScheme(StyleSettings::eByInformationContent);
                break;
            default:
                return;
        }
        glCanvas->structureSet->styleManager->CheckGlobalStyleSettings(glCanvas->structureSet);
        GlobalMessenger()->PostRedrawAllStructures();
        GlobalMessenger()->PostRedrawAllSequenceViewers();
    }
}

void Cn3DMainFrame::LoadFile(const char *filename)
{
    glCanvas->SetCurrent();

    // clear old data
    if (glCanvas->structureSet) {
        GlobalMessenger()->RemoveAllHighlights(false);
        delete glCanvas->structureSet;
        glCanvas->structureSet = NULL;
        glCanvas->renderer->AttachStructureSet(NULL);
        glCanvas->Refresh(false);
    }

    if (wxIsAbsolutePath(filename))
        userDir = std::string(wxPathOnly(filename).c_str()) + wxFILE_SEP_PATH;
    else if (wxPathOnly(filename) == "")
        userDir = workingDir;
    else
        userDir = workingDir + wxPathOnly(filename).c_str() + wxFILE_SEP_PATH;
    TESTMSG("user dir: " << userDir.c_str());

    // try to decide if what ASN type this is, and if it's binary or ascii
    CNcbiIstream *inStream = new CNcbiIfstream(filename, IOS_BASE::in | IOS_BASE::binary);
    if (!inStream) {
        ERR_POST(Error << "Cannot open file '" << filename << "' for reading");
        return;
    }
    std::string firstWord;
    *inStream >> firstWord;
    delete inStream;

    static const std::string
        asciiMimeFirstWord = "Ncbi-mime-asn1",
        asciiCDDFirstWord = "Cdd";
    bool isMime = false, isCDD = false, isBinary = true;
    if (firstWord == asciiMimeFirstWord) {
        isMime = true;
        isBinary = false;
    } else if (firstWord == asciiCDDFirstWord) {
        isCDD = true;
        isBinary = false;
    }

    // try to read the file as various ASN types (if it's not clear from the first ascii word).
    // If read is successful, the StructureSet will own the asn data object, to keep it
    // around for output later on
    bool readOK = false;
    std::string err;
    if (!isCDD) {
        TESTMSG("trying to read file '" << filename << "' as " <<
            ((isBinary) ? "binary" : "ascii") << " mime");
        CNcbi_mime_asn1 *mime = new CNcbi_mime_asn1();
        SetDiagPostLevel(eDiag_Fatal); // ignore all but Fatal errors while reading data
        readOK = ReadASNFromFile(filename, *mime, isBinary, err);
        SetDiagPostLevel(eDiag_Info);
        if (readOK) {
            glCanvas->structureSet = new StructureSet(mime);
        } else {
            ERR_POST(Warning << "error: " << err);
            delete mime;
        }
    }
    if (!readOK) {
        TESTMSG("trying to read file '" << filename << "' as " <<
            ((isBinary) ? "binary" : "ascii") << " cdd");
        CCdd *cdd = new CCdd();
        SetDiagPostLevel(eDiag_Fatal); // ignore all but Fatal errors while reading data
        readOK = ReadASNFromFile(filename, *cdd, isBinary, err);
        SetDiagPostLevel(eDiag_Info);
        if (readOK) {
            glCanvas->structureSet = new StructureSet(cdd, userDir.c_str(), structureLimit);
        } else {
            ERR_POST(Warning << "error: " << err);
            delete cdd;
        }
    }
    if (!readOK) {
        ERR_POST(Error << "File is not a recognized data type (Ncbi-mime-asn1 or Cdd)");
        return;
    }

    SetTitle(wxFileNameFromPath(filename) + " - Cn3D++");
    glCanvas->renderer->AttachStructureSet(glCanvas->structureSet);
    glCanvas->Refresh(false);
}

void Cn3DMainFrame::OnOpen(wxCommandEvent& event)
{
    if (glCanvas->structureSet) {
        GlobalMessenger()->SequenceWindowsSave();   // give sequence window a chance to save an edited alignment
        SaveDialog(false);                          // give structure window a chance to save data
    }

    const wxString& filestr = wxFileSelector("Choose a text or binary ASN1 file to open", userDir.c_str());
    if (!filestr.IsEmpty())
        LoadFile(filestr.c_str());
}

void Cn3DMainFrame::OnSave(wxCommandEvent& event)
{
    if (!glCanvas->structureSet) return;

    // force a save of any edits to alignment and updates first
    GlobalMessenger()->SequenceWindowsSave();

    wxString outputFilename = wxFileSelector(
        "Choose a filename for output", userDir.c_str(), "",
        ".prt", "All Files|*.*|Binary ASN (*.val)|*.val|ASCII CDD (*.acd)|*.acd|ASCII ASN (*.prt)|*.prt",
        wxSAVE | wxOVERWRITE_PROMPT);
    TESTMSG("save file: '" << outputFilename.c_str() << "'");
    if (!outputFilename.IsEmpty())
        glCanvas->structureSet->SaveASNData(outputFilename.c_str(), (outputFilename.Right(4) == ".val"));
}

void Cn3DMainFrame::OnLimit(wxCommandEvent& event)
{
    long newLimit = wxGetNumberFromUser(
        "Enter the maximum number of structures to display (-1 for unlimited)",
        "Max:", "Structure Limit", 10, -1, 1000, this);
    if (newLimit >= 0)
        structureLimit = (int) newLimit;
    else
        structureLimit = UNLIMITED_STRUCTURES;
}

void Cn3DMainFrame::OnSetQuality(wxCommandEvent& event)
{
    switch (event.GetId()) {
        case MID_QLOW: glCanvas->renderer->SetLowQuality(); break;
        case MID_QMED: glCanvas->renderer->SetMediumQuality(); break;
        case MID_QHIGH: glCanvas->renderer->SetHighQuality(); break;
        default: ;
    }
    GlobalMessenger()->PostRedrawAllStructures();
}


// data and methods for the GLCanvas used for rendering structures (Cn3DGLCanvas)

BEGIN_EVENT_TABLE(Cn3DGLCanvas, wxGLCanvas)
    EVT_SIZE                (Cn3DGLCanvas::OnSize)
    EVT_PAINT               (Cn3DGLCanvas::OnPaint)
    EVT_CHAR                (Cn3DGLCanvas::OnChar)
    EVT_MOUSE_EVENTS        (Cn3DGLCanvas::OnMouseEvent)
    EVT_ERASE_BACKGROUND    (Cn3DGLCanvas::OnEraseBackground)
END_EVENT_TABLE()

Cn3DGLCanvas::Cn3DGLCanvas(wxWindow *parent, int *attribList) :
    wxGLCanvas(parent, -1, wxPoint(0, 0), wxDefaultSize, wxSUNKEN_BORDER, "Cn3DGLCanvas", attribList),
    structureSet(NULL)
{
    renderer = new OpenGLRenderer();
}

Cn3DGLCanvas::~Cn3DGLCanvas(void)
{
    if (structureSet) delete structureSet;
    if (font) delete font;
    delete renderer;
}

void Cn3DGLCanvas::OnPaint(wxPaintEvent& event)
{
    // This is a dummy, to avoid an endless succession of paint messages.
    // OnPaint handlers must always create a wxPaintDC.
    wxPaintDC dc(this);

    if (!GetContext() || !renderer) return;
    SetCurrent();
    renderer->Display();
    SwapBuffers();
}

void Cn3DGLCanvas::OnSize(wxSizeEvent& event)
{
    if (!GetContext() || !renderer) return;
    SetCurrent();
    int width, height;
    GetClientSize(&width, &height);
    renderer->SetSize(width, height);
}

void Cn3DGLCanvas::OnMouseEvent(wxMouseEvent& event)
{
    static bool dragging = false;
    static long last_x, last_y;

    if (!GetContext() || !renderer) return;
    SetCurrent();

    // keep mouse focus while holding down button
    if (event.LeftDown()) CaptureMouse();
    if (event.LeftUp()) ReleaseMouse();

    if (event.LeftIsDown()) {
        if (!dragging) {
            dragging = true;
        } else {
            renderer->ChangeView(OpenGLRenderer::eXYRotateHV,
                event.GetX()-last_x, event.GetY()-last_y);
            Refresh(false);
        }
        last_x = event.GetX();
        last_y = event.GetY();
    } else {
        dragging = false;
    }

    if (event.RightDown()) {
        unsigned int name;
        if (structureSet && renderer->GetSelected(event.GetX(), event.GetY(), &name))
            structureSet->SelectedAtom(name);
    }
}

void Cn3DGLCanvas::OnChar(wxKeyEvent& event)
{
    if (!GetContext() || !renderer) return;
    SetCurrent();

    switch (event.KeyCode()) {
        case 'a': case 'A': renderer->ShowAllFrames(); Refresh(false); break;
        case WXK_DOWN: renderer->ShowFirstFrame(); Refresh(false); break;
        case WXK_UP: renderer->ShowLastFrame(); Refresh(false); break;
        case WXK_RIGHT: renderer->ShowNextFrame(); Refresh(false); break;
        case WXK_LEFT: renderer->ShowPreviousFrame(); Refresh(false); break;
        default: event.Skip();
    }
}

void Cn3DGLCanvas::OnEraseBackground(wxEraseEvent& event)
{
    // Do nothing, to avoid flashing.
}

END_SCOPE(Cn3D)

