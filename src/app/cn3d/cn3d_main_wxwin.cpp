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
#include <serial/serial.hpp>            
#include <serial/objistrasn.hpp>       
#include <serial/objistrasnb.hpp>       
#include <objects/ncbimime/Ncbi_mime_asn1.hpp>

// For now, this module will contain a simple wxWindows + wxGLCanvas interface
#include "cn3d/cn3d_main_wxwin.hpp"
#include "cn3d/style_manager.hpp"
#include "cn3d/sequence_viewer.hpp"
#include "cn3d/messenger.hpp"
#include "cn3d/chemical_graph.hpp"

USING_NCBI_SCOPE;
USING_SCOPE(objects);


BEGIN_SCOPE(Cn3D)

// Set the NCBI diagnostic streams to go to this method, which then pastes them
// into a wxWindow. This log window can be closed anytime, but will be hidden,
// not destroyed. More serious errors will bring up a dialog, so that the user
// will get a chance to see the error before the program halts upon a fatal error.

class MsgFrame;

static MsgFrame *logFrame = NULL;
static wxTextCtrl *logText = NULL;

class MsgFrame : public wxFrame
{
public:
    MsgFrame(wxWindow* parent, wxWindowID id, const wxString& title, 
        const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize, 
        long style = wxDEFAULT_FRAME_STYLE, const wxString& name = "frame") :
        wxFrame(parent, id, title, pos, size, style, name) { }
    ~MsgFrame(void) { logFrame = NULL; logText = NULL; }
};

void DisplayDiagnostic(const SDiagMessage& diagMsg)
{
    std::string errMsg;
    diagMsg.Write(errMsg);

    if (diagMsg.m_Severity >= eDiag_Error) {
        wxMessageDialog *dlg =
			new wxMessageDialog(NULL, errMsg.c_str(), "Severe Error!",
				wxOK | wxCENTRE | wxICON_EXCLAMATION);
        dlg->ShowModal();
		delete dlg;
    } else {
        if (!logFrame) {
            logFrame = new MsgFrame(NULL, -1, "Cn3D++ Message Log", 
                wxPoint(500, 0), wxSize(500, 500), wxDEFAULT_FRAME_STYLE);
            logFrame->SetSizeHints(150, 100);
            logText = new wxTextCtrl(logFrame, -1, "", 
                wxPoint(0,0), wxDefaultSize, wxTE_MULTILINE | wxTE_READONLY | wxHSCROLL);
        }
        // seems to be some upper limit on size, at least under MSW - so delete top of log if too big
        if (logText->GetLastPosition() > 30000) logText->Clear();
        *logText << wxString(errMsg.data(), errMsg.size());
        logFrame->Show(true);
    }
}

// `Main program' equivalent, creating GUI framework
IMPLEMENT_APP(Cn3DApp)

BEGIN_EVENT_TABLE(Cn3DApp, wxApp)
    EVT_IDLE(Cn3DApp::OnIdle)
END_EVENT_TABLE()

bool Cn3DApp::OnInit(void)
{
    // setup the diagnostic stream
    SetDiagHandler(DisplayDiagnostic, NULL, NULL);
    SetDiagPostLevel(eDiag_Info); // report all messages

    // read dictionary
    LoadStandardDictionary();

    // create the messenger; tell it about sequence & structure windows
    messenger = new Messenger();

    // create the sequence viewer
    sequenceViewer = new SequenceViewer(messenger);

    // create the main frame window
    structureWindow = new Cn3DMainFrame(messenger, NULL, "Cn3D++",
        wxPoint(0,0), wxSize(500,500), wxDEFAULT_FRAME_STYLE);

    // register viewers with messenger
    messenger->AddSequenceViewer(sequenceViewer);
    messenger->AddStructureWindow(structureWindow);

    // get file name from command line, if present
    if (argc > 2)
        ERR_POST(Fatal << "\nUsage: cn3d [filename]");
    else if (argc == 2) {
        structureWindow->LoadFile(argv[1]);
    }

    return true;
}

int Cn3DApp::OnExit(void)
{
    // delete dictionary
    DeleteStandardDictionary();

    delete messenger;
	return 0;
}

void Cn3DApp::OnIdle(wxIdleEvent& event)
{
    messenger->ProcessRedraws();

    // call base class OnIdle to continue processing any other system idle-time stuff
    wxApp::OnIdle(event);
}


// data and methods for the main program window (a Cn3DMainFrame)

BEGIN_EVENT_TABLE(Cn3DMainFrame, wxFrame)
    EVT_CLOSE(Cn3DMainFrame::OnCloseWindow)
    EVT_MENU(MID_EXIT,      Cn3DMainFrame::OnExit)
    EVT_MENU(MID_OPEN,      Cn3DMainFrame::OnOpen)
    EVT_MENU_RANGE(MID_TRANSLATE,   MID_RESET,      Cn3DMainFrame::OnAdjustView)
    EVT_MENU_RANGE(MID_SECSTRUC,    MID_WIREFRAME,  Cn3DMainFrame::OnSetStyle)
    EVT_MENU_RANGE(MID_QLOW,        MID_QHIGH,      Cn3DMainFrame::OnSetQuality)
END_EVENT_TABLE()

Cn3DMainFrame::Cn3DMainFrame(Messenger *mesg,
    wxFrame *parent, const wxString& title, const wxPoint& pos, const wxSize& size, long style) :
    wxFrame(parent, -1, title, pos, size, style | wxTHICK_FRAME),
    glCanvas(NULL), messenger(mesg)
{
    SetSizeHints(150, 150); // min size

    // File menu
    wxMenuBar *menuBar = new wxMenuBar;
    wxMenu *menu = new wxMenu;
    menu->Append(MID_OPEN, "&Open");
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

    // Style menu
    menu = new wxMenu;
    menu->Append(MID_SECSTRUC, "&Secondary Structure");
    menu->Append(MID_ALIGN, "&Alignment");
    wxMenu *subMenu = new wxMenu;
    subMenu->Append(MID_IDENT, "&Identity");
    subMenu->Append(MID_VARIETY, "&Variety");
    subMenu->Append(MID_WGHT_VAR, "&Weighted Variety");
    menu->Append(MID_CONS, "&Conservation", subMenu);
    menu->Append(MID_WIREFRAME, "&Wireframe");
    menuBar->Append(menu, "&Style");

    // Quality menu
    menu = new wxMenu;
    menu->Append(MID_QLOW, "&Low");
    menu->Append(MID_QMED, "&Medium");
    menu->Append(MID_QHIGH, "&High");
    menuBar->Append(menu, "&Quality");

    SetMenuBar(menuBar);

    // Make a GLCanvas
#ifdef __WXMSW__
    int *gl_attrib = NULL;
#else
    int gl_attrib[20] = { 
        GLX_RGBA, GLX_RED_SIZE, 1, GLX_GREEN_SIZE, 1,
        GLX_BLUE_SIZE, 1, GLX_DEPTH_SIZE, 1,
        GLX_DOUBLEBUFFER, None };
#endif
    glCanvas = new Cn3DGLCanvas(messenger,
        this, -1, wxPoint(0, 0), wxSize(400, 400), 0, "Cn3DGLCanvas", gl_attrib);
    glCanvas->SetCurrent();

    Show(true);
}

Cn3DMainFrame::~Cn3DMainFrame(void)
{
    glCanvas->Destroy();
    if (logFrame) {
        logText->Destroy();
        logFrame->Destroy();
        logText = NULL;
        logFrame = NULL;
    }
    messenger->RemoveStructureWindow(this);
}

void Cn3DMainFrame::OnCloseWindow(wxCloseEvent& event)
{
    Destroy();
}

void Cn3DMainFrame::OnExit(wxCommandEvent& event)
{
    Destroy();
}

void Cn3DMainFrame::OnAdjustView(wxCommandEvent& event)
{
    glCanvas->SetCurrent();
    switch (event.GetId()) {
        case MID_TRANSLATE:
            glCanvas->renderer.ChangeView(OpenGLRenderer::eXYTranslateHV, 25, 25);
            break;
        case MID_ZOOM_IN:
            glCanvas->renderer.ChangeView(OpenGLRenderer::eZoomIn);
            break;
        case MID_ZOOM_OUT:
            glCanvas->renderer.ChangeView(OpenGLRenderer::eZoomOut);
            break;
        case MID_RESET:
            glCanvas->renderer.ResetCamera();
            break;
        default: ;
    }
    glCanvas->Refresh(false);
}

void Cn3DMainFrame::OnSetStyle(wxCommandEvent& event)
{
    if (glCanvas->structureSet) {
        glCanvas->SetCurrent();
        switch (event.GetId()) {
            case MID_SECSTRUC:
                glCanvas->structureSet->styleManager->SetToSecondaryStructure();
                break;
            case MID_ALIGN:
                glCanvas->structureSet->styleManager->SetToAlignment(StyleSettings::eAligned);
                break;
            case MID_IDENT:
                glCanvas->structureSet->styleManager->SetToAlignment(StyleSettings::eIdentity);
                break;
            case MID_VARIETY:
                glCanvas->structureSet->styleManager->SetToAlignment(StyleSettings::eVariety);
                break;
            case MID_WGHT_VAR:
                glCanvas->structureSet->styleManager->SetToAlignment(StyleSettings::eWeightedVariety);
                break;
            case MID_WIREFRAME:
                glCanvas->structureSet->styleManager->SetToWireframe();
                break;
            default: ;
        }
        glCanvas->structureSet->styleManager->CheckStyleSettings(glCanvas->structureSet);
        messenger->PostRedrawAllStructures();
        messenger->PostRedrawSequenceViewers();
    }
}

void Cn3DMainFrame::LoadFile(const char *filename)
{
    glCanvas->SetCurrent();

    // clear old data
    if (glCanvas->structureSet) {
        messenger->RemoveAllHighlights(false);
        delete glCanvas->structureSet;
        glCanvas->structureSet = NULL;
        glCanvas->renderer.AttachStructureSet(NULL);
        glCanvas->Refresh(false);
    }

    // initialize the binary input stream 
    auto_ptr<CNcbiIstream> inStream;
    inStream.reset(new CNcbiIfstream(filename, IOS_BASE::in | IOS_BASE::binary));
    if (!(*inStream)) {
        ERR_POST(Error << "Cannot open file '" << filename << "'");
        return;
    }

    // try to decide if it's binary or ascii
    static const std::string asciiMimeFirstWord = "Ncbi-mime-asn1";
    std::string firstWord;
    *inStream >> firstWord;
    inStream->seekg(0);

    auto_ptr<CObjectIStream> inObject;
    if (firstWord == asciiMimeFirstWord) {
        // Associate ASN.1 text serialization methods with the input 
        inObject.reset(new CObjectIStreamAsn(*inStream));
    } else {
        // Associate ASN.1 binary serialization methods with the input 
        inObject.reset(new CObjectIStreamAsnBinary(*inStream));
    }

    // Read the CNcbi_mime_asn1 data 
    CNcbi_mime_asn1 mime;
    try {
        *inObject >> mime;
    } catch (exception& e) {
        ERR_POST(Error << "Cannot read file '" << filename << "'\nreason: " << e.what());
        return;
    }

    // Create StructureSet from mime data
    glCanvas->structureSet = new StructureSet(mime, messenger);
    glCanvas->renderer.AttachStructureSet(glCanvas->structureSet);
    glCanvas->Refresh(false);
}

void Cn3DMainFrame::OnOpen(wxCommandEvent& event)
{
    const wxString& filestr = wxFileSelector("Choose a binary ASN1 file to open");
    if (filestr.IsEmpty()) return;
    LoadFile(filestr.c_str());
}

void Cn3DMainFrame::OnSetQuality(wxCommandEvent& event)
{
    switch (event.GetId()) {
        case MID_QLOW: glCanvas->renderer.SetLowQuality(); break;
        case MID_QMED: glCanvas->renderer.SetMediumQuality(); break;
        case MID_QHIGH: glCanvas->renderer.SetHighQuality(); break;
        default: ;
    }
    messenger->PostRedrawAllStructures();
}


// data and methods for the GLCanvas used for rendering structures (Cn3DGLCanvas)

BEGIN_EVENT_TABLE(Cn3DGLCanvas, wxGLCanvas)
    EVT_SIZE                (Cn3DGLCanvas::OnSize)
    EVT_PAINT               (Cn3DGLCanvas::OnPaint)
    EVT_CHAR                (Cn3DGLCanvas::OnChar)
    EVT_MOUSE_EVENTS        (Cn3DGLCanvas::OnMouseEvent)
    EVT_ERASE_BACKGROUND    (Cn3DGLCanvas::OnEraseBackground)
END_EVENT_TABLE()

Cn3DGLCanvas::Cn3DGLCanvas(Messenger *mesg,
    wxWindow *parent, wxWindowID id,
    const wxPoint& pos, const wxSize& size, long style, const wxString& name, int *gl_attrib) :
    wxGLCanvas(parent, id, pos, size, style, name, gl_attrib), 
    structureSet(NULL), messenger(mesg)
{
    SetCurrent();
    font = new wxFont(12, wxSWISS, wxNORMAL, wxBOLD);

#ifdef __WXMSW__
    renderer.SetFont_Windows(font->GetHFONT());
#endif
}

Cn3DGLCanvas::~Cn3DGLCanvas(void)
{
    if (structureSet) delete structureSet;
    if (font) delete font;
}

void Cn3DGLCanvas::OnPaint(wxPaintEvent& event)
{
    // This is a dummy, to avoid an endless succession of paint messages.
    // OnPaint handlers must always create a wxPaintDC.
    wxPaintDC dc(this);

#ifndef __WXMOTIF__
    if (!GetContext()) return;
#endif

    SetCurrent();
    renderer.Display();
    SwapBuffers();
}

void Cn3DGLCanvas::OnSize(wxSizeEvent& event)
{
#ifndef __WXMOTIF__
    if (!GetContext()) return;
#endif

    SetCurrent();
    int width, height;
    GetClientSize(&width, &height);
    renderer.SetSize(width, height);
}

void Cn3DGLCanvas::OnMouseEvent(wxMouseEvent& event)
{
    static bool dragging = false;
    static long last_x, last_y;

#ifndef __WXMOTIF__
    if (!GetContext()) return;
#endif
    SetCurrent();

    // keep mouse focus while holding down button
    if (event.LeftDown()) CaptureMouse();
    if (event.LeftUp()) ReleaseMouse();

    if (event.LeftIsDown()) {
        if (!dragging) {
            dragging = true;
        } else {
            renderer.ChangeView(OpenGLRenderer::eXYRotateHV,
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
        if (structureSet && renderer.GetSelected(event.GetX(), event.GetY(), &name))
            structureSet->SelectedAtom(name);
    }
}

void Cn3DGLCanvas::OnChar(wxKeyEvent& event)
{
#ifndef __WXMOTIF__
    if (!GetContext()) return;
#endif
    SetCurrent();

    switch (event.KeyCode()) {
        case 'a': case 'A': renderer.ShowAllFrames(); Refresh(false); break;
        case WXK_DOWN: renderer.ShowFirstFrame(); Refresh(false); break;
        case WXK_UP: renderer.ShowLastFrame(); Refresh(false); break;
        case WXK_RIGHT: renderer.ShowNextFrame(); Refresh(false); break;
        case WXK_LEFT: renderer.ShowPreviousFrame(); Refresh(false); break;
        default: event.Skip();
    }
}

void Cn3DGLCanvas::OnEraseBackground(wxEraseEvent& event)
{
    // Do nothing, to avoid flashing.
}

END_SCOPE(Cn3D)

