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
*      structure window object for Cn3D
*
* ===========================================================================
*/


#include <corelib/ncbistd.hpp>

#include <objects/ncbimime/Ncbi_mime_asn1.hpp>
#include <objects/cdd/Cdd.hpp>

#include <algorithm>

#ifdef __WXMSW__
#include <windows.h>
#include <wx/msw/winundef.h>
#endif
#include <wx/wx.h>
#include <wx/html/helpfrm.h>
#include <wx/html/helpctrl.h>
#include <wx/fontdlg.h>
#include <wx/confbase.h>
#include <wx/fileconf.h>
#include <wx/filename.h>

#include "cn3d/asn_reader.hpp"
#include "cn3d/structure_window.hpp"
#include "cn3d/structure_set.hpp"
#include "cn3d/cn3d_glcanvas.hpp"
#include "cn3d/opengl_renderer.hpp"
#include "cn3d/style_manager.hpp"
#include "cn3d/messenger.hpp"
#include "cn3d/chemical_graph.hpp"
#include "cn3d/alignment_manager.hpp"
#include "cn3d/show_hide_manager.hpp"
#include "cn3d/show_hide_dialog.hpp"
#include "cn3d/cn3d_tools.hpp"
#include "cn3d/cdd_annot_dialog.hpp"
#include "cn3d/preferences_dialog.hpp"
#include "cn3d/cdd_ref_dialog.hpp"
#include "cn3d/cn3d_png.hpp"
#include "cn3d/wx_tools.hpp"
#include "cn3d/block_multiple_alignment.hpp"
#include "cn3d/sequence_set.hpp"
#include "cn3d/molecule_identifier.hpp"
#include "cn3d/cdd_splash_dialog.hpp"

// the application icon (under Windows it is in resources)
#if defined(__WXGTK__) || defined(__WXMAC__)
    #include "cn3d/cn3d.xpm"
#endif

#include <ncbienv.h>

USING_NCBI_SCOPE;
USING_SCOPE(objects);


BEGIN_SCOPE(Cn3D)

// global strings
static string
    userDir,        // directory of latest user-selected file
    currentFile;    // name of current working file
const string& GetUserDir(void) { return userDir; }
const string& GetWorkingFilename(void) { return currentFile; }

// global working title
static string workingTitle;
const string& GetWorkingTitle(void) { return workingTitle; }
static void SetWorkingTitle(StructureSet *sSet)
{
    if (sSet->IsCDDInMime() && sSet->GetCDDName().size() > 0)
        workingTitle = sSet->GetCDDName();      // for CDD's sent by server
    else if (sSet->IsCDD())
        workingTitle = GetWorkingFilename();    // for CDD's edited by curators
    else if (sSet->objects.size() > 0) {
        workingTitle = sSet->objects.front()->pdbID;
        if (sSet->objects.size() > 1)
            workingTitle += " neighbors";
    } else
        workingTitle = GetWorkingFilename();
}

// favorites stuff
static bool favoriteStylesChanged = false;
static CCn3d_style_settings_set favoriteStyles;
static bool LoadFavorites(void);
static void SaveFavorites(void);


BEGIN_EVENT_TABLE(StructureWindow, wxFrame)
    EVT_CLOSE     (                                         StructureWindow::OnCloseWindow)
    EVT_MENU      (MID_EXIT,                                StructureWindow::OnExit)
    EVT_MENU      (MID_OPEN,                                StructureWindow::OnOpen)
    EVT_MENU_RANGE(MID_SAVE_SAME, MID_SAVE_AS,              StructureWindow::OnSave)
    EVT_MENU      (MID_PNG,                                 StructureWindow::OnPNG)
    EVT_MENU_RANGE(MID_ZOOM_IN,  MID_ALL_FRAMES,            StructureWindow::OnAdjustView)
    EVT_MENU_RANGE(MID_SHOW_HIDE,  MID_SHOW_SELECTED_DOMAINS,           StructureWindow::OnShowHide)
    EVT_MENU_RANGE(MID_DIST_SELECT_RESIDUES, MID_DIST_SELECT_OTHER_ALL, StructureWindow::OnDistanceSelect)
    EVT_MENU      (MID_REFIT_ALL,                           StructureWindow::OnAlignStructures)
    EVT_MENU_RANGE(MID_EDIT_STYLE, MID_ANNOTATE,            StructureWindow::OnSetStyle)
    EVT_MENU_RANGE(MID_ADD_FAVORITE, MID_FAVORITES_FILE,    StructureWindow::OnEditFavorite)
    EVT_MENU_RANGE(MID_FAVORITES_BEGIN, MID_FAVORITES_END,  StructureWindow::OnSelectFavorite)
    EVT_MENU_RANGE(MID_SHOW_LOG,   MID_SHOW_SEQ_V,          StructureWindow::OnShowWindow)
    EVT_MENU_RANGE(MID_CDD_OVERVIEW, MID_CDD_SHOW_REJECTS,  StructureWindow::OnCDD)
    EVT_MENU      (MID_PREFERENCES,                         StructureWindow::OnPreferences)
    EVT_MENU_RANGE(MID_OPENGL_FONT, MID_SEQUENCE_FONT,      StructureWindow::OnSetFont)
    EVT_MENU_RANGE(MID_PLAY, MID_SET_DELAY,                 StructureWindow::OnAnimate)
    EVT_MENU_RANGE(MID_HELP_COMMANDS, MID_ONLINE_HELP,      StructureWindow::OnHelp)
    EVT_MENU      (MID_ABOUT,                               StructureWindow::OnHelp)
    EVT_TIMER     (MID_ANIMATE,                             StructureWindow::OnAnimationTimer)
    EVT_TIMER     (MID_MESSAGING,                           StructureWindow::OnFileMessagingTimer)
END_EVENT_TABLE()

StructureWindow::StructureWindow(const wxString& title, const wxPoint& pos, const wxSize& size) :
    wxFrame(NULL, wxID_HIGHEST + 1, title, pos, size, wxDEFAULT_FRAME_STYLE | wxTHICK_FRAME),
    glCanvas(NULL), cddAnnotateDialog(NULL), cddDescriptionDialog(NULL), cddNotesDialog(NULL),
    cddRefDialog(NULL), helpController(NULL), helpConfig(NULL), cddOverview(NULL),
    fileMessagingManager("Cn3D"), fileMessenger(NULL)
{
    GlobalMessenger()->AddStructureWindow(this);
    animationTimer.SetOwner(this, MID_ANIMATE);
    SetSizeHints(150, 150); // min size
    SetIcon(wxICON(cn3d));

    // File menu
    menuBar = new wxMenuBar;
    wxMenu *menu = new wxMenu;
    menu->Append(MID_OPEN, "&Open\tCtrl+O");
    menu->Append(MID_SAVE_SAME, "&Save\tCtrl+S");
    menu->Append(MID_SAVE_AS, "Save &As...");
    menu->Append(MID_PNG, "&Export PNG");
    menu->AppendSeparator();
    menu->Append(MID_REFIT_ALL, "&Realign Structures");
    menu->AppendSeparator();
    menu->Append(MID_PREFERENCES, "&Preferences...");
    wxMenu *subMenu = new wxMenu;
    subMenu->Append(MID_OPENGL_FONT, "S&tructure Window");
    subMenu->Append(MID_SEQUENCE_FONT, "Se&quence Windows");
    menu->Append(MID_FONTS, "Set &Fonts...", subMenu);
    menu->AppendSeparator();
    menu->Append(MID_EXIT, "E&xit");
    menuBar->Append(menu, "&File");

    // View menu
    menu = new wxMenu;
    menu->Append(MID_ZOOM_IN, "Zoom &In\tz");
    menu->Append(MID_ZOOM_OUT, "Zoom &Out\tx");
    menu->Append(MID_RESTORE, "&Restore");
    menu->Append(MID_RESET, "Rese&t");
    menu->AppendSeparator();
#ifdef __WXMSW__
    menu->Append(MID_NEXT_FRAME, "&Next Frame\tRight");
    menu->Append(MID_PREV_FRAME, "Pre&vious Frame\tLeft");
    menu->Append(MID_FIRST_FRAME, "&First Frame\tDown");
    menu->Append(MID_LAST_FRAME, "&Last Frame\tUp");
#else
    // other platforms don't like to display these long accelerator names
    menu->Append(MID_NEXT_FRAME, "&Next Frame");
    menu->Append(MID_PREV_FRAME, "Pre&vious Frame");
    menu->Append(MID_FIRST_FRAME, "&First Frame");
    menu->Append(MID_LAST_FRAME, "&Last Frame");
#endif
    menu->Append(MID_ALL_FRAMES, "&All Frames\ta");
    menu->AppendSeparator();
    subMenu = new wxMenu;
    subMenu->Append(MID_PLAY, "&Play Frames\tp", "", true);
    subMenu->Check(MID_PLAY, false);
    subMenu->Append(MID_SPIN, "Spi&n\tn", "", true);
    subMenu->Check(MID_SPIN, false);
    subMenu->Append(MID_STOP, "&Stop\ts", "", true);
    subMenu->Check(MID_STOP, true);
    subMenu->Append(MID_SET_DELAY, "Set &Delay");
    menu->Append(MID_ANIMATE, "Ani&mation", subMenu);
    menuBar->Append(menu, "&View");

    // Show-Hide menu
    menu = new wxMenu;
    menu->Append(MID_SHOW_HIDE, "&Pick Structures...");
    menu->AppendSeparator();
    menu->Append(MID_SHOW_ALL, "Show &Everything");
    menu->Append(MID_SHOW_DOMAINS, "Show Aligned &Domains");
    menu->Append(MID_SHOW_ALIGNED, "Show &Aligned Residues");
    menu->Append(MID_SHOW_SELECTED_RESIDUES, "Show &Selected Residues");
    menu->Append(MID_SHOW_SELECTED_DOMAINS, "Show Selected Do&mains");
    subMenu = new wxMenu;
    subMenu->Append(MID_SHOW_UNALIGNED_ALL, "Show &All");
    subMenu->Append(MID_SHOW_UNALIGNED_ALN_DOMAIN, "Show in Aligned &Domains");
    menu->Append(MID_SHOW_UNALIGNED, "&Unaligned Residues", subMenu);
    menu->AppendSeparator();
    subMenu = new wxMenu;
    subMenu->Append(MID_DIST_SELECT_RESIDUES, "&Residues Only");
    subMenu->Append(MID_DIST_SELECT_ALL, "&All Molecules");
    subMenu->Append(MID_DIST_SELECT_OTHER_RESIDUES, "&Other Residues");
    subMenu->Append(MID_DIST_SELECT_OTHER_ALL, "Other &Molecules");
    menu->Append(MID_DIST_SELECT, "Select by Dis&tance...", subMenu);
    menuBar->Append(menu, "Show/&Hide");

    // Style menu
    menu = new wxMenu;
    menu->Append(MID_EDIT_STYLE, "Edit &Global Style");
    // favorites
    favoritesMenu = new wxMenu;
    favoritesMenu->Append(MID_ADD_FAVORITE, "&Add/Replace");
    favoritesMenu->Append(MID_REMOVE_FAVORITE, "&Remove");
    favoritesMenu->Append(MID_FAVORITES_FILE, "&Change File");
    favoritesMenu->AppendSeparator();
    LoadFavorites();
    SetupFavoritesMenu();
    menu->Append(MID_FAVORITES, "&Favorites", favoritesMenu);
    // rendering shortcuts
    subMenu = new wxMenu;
    subMenu->Append(MID_WORM, "&Worms", "", true);
    subMenu->Append(MID_TUBE, "&Tubes", "", true);
    subMenu->Append(MID_WIRE, "Wir&e", "", true);
    subMenu->Append(MID_BNS, "&Ball and Stick", "", true);
    subMenu->Append(MID_SPACE, "&Space Fill", "", true);
    subMenu->AppendSeparator();
    subMenu->Append(MID_SC_TOGGLE, "&Toggle Sidechains");
    menu->Append(MID_RENDER, "&Rendering Shortcuts", subMenu);
    // coloring shortcuts
    subMenu = new wxMenu;
    subMenu->Append(MID_SECSTRUC, "&Secondary Structure", "", true);
    subMenu->Append(MID_ALIGNED, "&Aligned", "", true);
    wxMenu *subMenu2 = new wxMenu;
    subMenu2->Append(MID_IDENTITY, "I&dentity", "", true);
    subMenu2->Append(MID_VARIETY, "&Variety", "", true);
    subMenu2->Append(MID_WGHT_VAR, "&Weighted Variety", "", true);
    subMenu2->Append(MID_INFO, "&Information Content", "", true);
    subMenu2->Append(MID_FIT, "&Fit", "", true);
    subMenu2->Append(MID_BLOCK_FIT, "&Block Fit", "", true);
    subMenu2->Append(MID_BLOCK_Z_FIT, "&Normalized Block Fit", "", true);
    subMenu2->Append(MID_BLOCK_ROW_FIT, "Block &Row Fit", "", true);
    subMenu->Append(MID_CONS, "Sequence &Conservation", subMenu2);
    subMenu->Append(MID_OBJECT, "&Object", "", true);
    subMenu->Append(MID_DOMAIN, "&Domain", "", true);
    subMenu->Append(MID_MOLECULE, "&Molecule", "", true);
    subMenu->Append(MID_RAINBOW, "&Rainbow", "", true);
    subMenu->Append(MID_HYDROPHOB, "&Hydrophobicity", "", true);
    subMenu->Append(MID_CHARGE, "Char&ge", "", true);
    subMenu->Append(MID_TEMP, "&Temperature", "", true);
    subMenu->Append(MID_ELEMENT, "&Element", "", true);
    menu->Append(MID_COLORS, "&Coloring Shortcuts", subMenu);
    //annotate
    menu->AppendSeparator();
    menu->Append(MID_ANNOTATE, "A&nnotate");
    menuBar->Append(menu, "&Style");

    // Window menu
    menu = new wxMenu;
    menu->Append(MID_SHOW_SEQ_V, "Show &Sequence Viewer");
    menu->Append(MID_SHOW_LOG, "Show Message &Log");
    menu->Append(MID_SHOW_LOG_START, "Show Log on Start&up", "", true);
    bool showLog = false;
    RegistryGetBoolean(REG_CONFIG_SECTION, REG_SHOW_LOG_ON_START, &showLog);
    menu->Check(MID_SHOW_LOG_START, showLog);
    menuBar->Append(menu, "&Window");

    // CDD menu
    bool readOnly;
    RegistryGetBoolean(REG_ADVANCED_SECTION, REG_CDD_ANNOT_READONLY, &readOnly);
    menu = new wxMenu;
    menu->Append(MID_CDD_OVERVIEW, "CDD &Overview");
    menu->AppendSeparator();
    menu->Append(MID_EDIT_CDD_NAME, "Edit &Name");
    menu->Enable(MID_EDIT_CDD_NAME, !readOnly);
    menu->Append(MID_EDIT_CDD_DESCR, "Edit &Description");
    menu->Enable(MID_EDIT_CDD_DESCR, !readOnly);
    menu->Append(MID_EDIT_CDD_NOTES, "Edit No&tes");
    menu->Enable(MID_EDIT_CDD_NOTES, !readOnly);
    menu->Append(MID_EDIT_CDD_REFERENCES, "Edit &References");
    menu->Enable(MID_EDIT_CDD_REFERENCES, !readOnly);
    menu->Append(MID_ANNOT_CDD, "Edit &Annotations");
    menu->Enable(MID_ANNOT_CDD, !readOnly);
    menu->AppendSeparator();
    menu->Append(MID_CDD_REJECT_SEQ, "Re&ject Sequence");
    menu->Enable(MID_CDD_REJECT_SEQ, !readOnly);
    menu->Append(MID_CDD_SHOW_REJECTS, "&Show Rejects");
    menuBar->Append(menu, "&CDD");

    // Help menu
    menu = new wxMenu;
    menu->Append(MID_HELP_COMMANDS, "&Commands");
    menu->Append(MID_ONLINE_HELP, "Online &Help...");
    menu->Append(MID_ABOUT, "&About");
    menuBar->Append(menu, "&Help");

    // accelerators for special keys
    wxAcceleratorEntry entries[10];
    entries[0].Set(wxACCEL_NORMAL, WXK_RIGHT, MID_NEXT_FRAME);
    entries[1].Set(wxACCEL_NORMAL, WXK_LEFT, MID_PREV_FRAME);
    entries[2].Set(wxACCEL_NORMAL, WXK_DOWN, MID_FIRST_FRAME);
    entries[3].Set(wxACCEL_NORMAL, WXK_UP, MID_LAST_FRAME);
    entries[4].Set(wxACCEL_NORMAL, 'z', MID_ZOOM_IN);
    entries[5].Set(wxACCEL_NORMAL, 'x', MID_ZOOM_OUT);
    entries[6].Set(wxACCEL_NORMAL, 'a', MID_ALL_FRAMES);
    entries[7].Set(wxACCEL_NORMAL, 'p', MID_PLAY);
    entries[8].Set(wxACCEL_NORMAL, 'n', MID_SPIN);
    entries[9].Set(wxACCEL_NORMAL, 's', MID_STOP);
    wxAcceleratorTable accel(10, entries);
    SetAcceleratorTable(accel);

    SetMenuBar(menuBar);
    menuBar->EnableTop(menuBar->FindMenu("CDD"), false);

    // Make a GLCanvas
#if defined(__WXMSW__)
    int *attribList = NULL;
#elif defined(__WXGTK__)
    int *attribList = NULL;
#elif defined(__WXMAC__)
    int *attribList = NULL;
#endif
    glCanvas = new Cn3DGLCanvas(this, attribList);

    // set initial font
    Show(true); // on X, need to establish gl context first, which requires visible window
    glCanvas->SetCurrent();
    glCanvas->SetGLFontFromRegistry();
}

StructureWindow::~StructureWindow(void)
{
}

void StructureWindow::OnCloseWindow(wxCloseEvent& event)
{
    animationTimer.Stop();
    fileMessagingTimer.Stop();
    Command(MID_EXIT);
}

void StructureWindow::OnExit(wxCommandEvent& event)
{
    animationTimer.Stop();
    fileMessagingTimer.Stop();
    GlobalMessenger()->RemoveStructureWindow(this); // don't bother with any redraws since we're exiting
    GlobalMessenger()->SequenceWindowsSave(true);   // save any edited alignment and updates first
    SaveDialog(true, false);                        // give structure window a chance to save data
    SaveFavorites();

    // remove help window if present
    if (helpController) {
        if (helpController->GetFrame())
            helpController->GetFrame()->Close(true);
        wxConfig::Set(NULL);
        delete helpConfig; // saves data
        delete helpController;
    }

    DestroyNonModalDialogs();
    Destroy();
}

void StructureWindow::SetupFileMessenger(const std::string& messageFilename, bool readOnly)
{
    if (fileMessenger) return;
    fileMessenger = fileMessagingManager.CreateNewFileMessenger(messageFilename, this, readOnly);
    fileMessagingTimer.SetOwner(this, MID_MESSAGING);
    fileMessagingTimer.Start(200, false);
}

void StructureWindow::OnFileMessagingTimer(wxTimerEvent& event)
{
    // poll message files
    fileMessagingManager.PollMessageFiles();
}

void StructureWindow::ReceivedCommand(const std::string& fromApp, unsigned long id,
    const std::string& command, const std::string& data)
{
    INFOMSG("received command " << id << " from " << fromApp << ": " << command << ", data:\n" << data);
    fileMessenger->SendReply(fromApp, id,
        MessageResponder::REPLY_ERROR, "Unrecognized command");
}

void StructureWindow::ReceivedReply(const std::string& fromApp, unsigned long id,
    MessageResponder::ReplyStatus status, const std::string& data)
{
    if (status == MessageResponder::REPLY_OKAY)
        INFOMSG(fromApp << ": got OKAY from " << fromApp << " (command " << id << "), data: " << data);
    else
        ERRORMSG(fromApp << ": got ERROR from " << fromApp << " (command " << id << "), data: " << data);
}

void StructureWindow::SendCommand(const std::string& toApp,
    const std::string& command, const std::string& data)
{
    if (!IsFileMessengerActive()) {
        ERRORMSG("SendCommand: no message file active!");
        return;
    }

    // for now, just assign command id's in numerical order
    static unsigned long nextCommandID = 1;
    INFOMSG("sending command " << nextCommandID << " to " << toApp << ": " << command);
    fileMessenger->SendCommand(toApp, nextCommandID++, command, data);
}

void StructureWindow::SetWindowTitle(void)
{
    SetTitle(wxString(GetWorkingTitle().c_str()) + " - Cn3D " + CN3D_VERSION_STRING);
}

void StructureWindow::OnHelp(wxCommandEvent& event)
{
    if (event.GetId() == MID_HELP_COMMANDS) {
        if (!helpController) {
            wxString path = wxString(GetPrefsDir().c_str()) + "help_cache";
            if (!wxDirExists(path)) {
                INFOMSG("trying to create help cache folder " << path.c_str());
                wxMkdir(path);
            }
            helpController = new wxHtmlHelpController(wxHF_DEFAULTSTYLE);
            helpController->SetTempDir(path);
            path = path + wxFILE_SEP_PATH + "Config";
            INFOMSG("saving help config in " << path.c_str());
            helpConfig = new wxFileConfig("Cn3D", "NCBI", path);
            wxConfig::Set(helpConfig);
            helpController->UseConfig(wxConfig::Get());
#ifdef __WXMAC__
            path = wxString(GetProgramDir().c_str()) + "../Resources/cn3d_commands.htb";
#else
            path = wxString(GetProgramDir().c_str()) + "cn3d_commands.htb";
#endif
            if (!helpController->AddBook(path))
                ERRORMSG("Can't load help book at " << path.c_str());
        }
        if (event.GetId() == MID_HELP_COMMANDS)
            helpController->Display("Cn3D Commands");
    }

    else if (event.GetId() == MID_ONLINE_HELP) {
        LaunchWebPage("http://www.ncbi.nlm.nih.gov/Structure/CN3D/cn3d.shtml");
    }

    else if (event.GetId() == MID_ABOUT) {
        wxString message(
            "Cn3D version "
            CN3D_VERSION_STRING
            "\n\n"
            "Produced by the National Center for Biotechnology Information\n"
            "     http://www.ncbi.nlm.nih.gov\n\n"
            "Please direct all questions and comments to:\n"
            "     info@ncbi.nlm.nih.gov"
        );
        wxMessageBox(message, "About Cn3D", wxOK | wxICON_INFORMATION, this);
    }
}

void StructureWindow::OnDistanceSelect(wxCommandEvent& event)
{
    if (!glCanvas->structureSet) return;
    static double latestCutoff = 5.0;
    GetFloatingPointDialog dialog(this, "Enter a distance cutoff (in Angstroms):", "Distance?",
        0.0, 1000.0, 0.5, latestCutoff);
    if (dialog.ShowModal() == wxOK) {
        latestCutoff = dialog.GetValue();
        glCanvas->structureSet->SelectByDistance(latestCutoff,
            (event.GetId() == MID_DIST_SELECT_RESIDUES || event.GetId() == MID_DIST_SELECT_OTHER_RESIDUES),
            (event.GetId() == MID_DIST_SELECT_OTHER_RESIDUES || event.GetId() == MID_DIST_SELECT_OTHER_ALL));
    }
}

void StructureWindow::OnPNG(wxCommandEvent& event)
{
    ExportPNG(glCanvas);
}

void StructureWindow::OnAnimate(wxCommandEvent& event)
{
    if (event.GetId() != MID_SET_DELAY) {
        menuBar->Check(MID_PLAY, false);
        menuBar->Check(MID_SPIN, false);
        menuBar->Check(MID_STOP, true);
    }
    if (!glCanvas->structureSet) return;

    int currentDelay;
    if (!RegistryGetInteger(REG_CONFIG_SECTION, REG_ANIMATION_DELAY, &currentDelay))
        return;

    // play
    if (event.GetId() == MID_PLAY) {
        if (glCanvas->structureSet->frameMap.size() > 1) {
            animationTimer.Start(currentDelay, false);
            animationMode = ANIM_FRAMES;
            menuBar->Check(MID_PLAY, true);
            menuBar->Check(MID_STOP, false);
        }
    }

    // spin
    if (event.GetId() == MID_SPIN) {
        animationTimer.Start(40, false);
        animationMode = ANIM_SPIN;
        menuBar->Check(MID_SPIN, true);
        menuBar->Check(MID_STOP, false);
    }

    // stop
    else if (event.GetId() == MID_STOP) {
        animationTimer.Stop();
    }

    // set delay
    else if (event.GetId() == MID_SET_DELAY) {
        long newDelay = wxGetNumberFromUser("Enter a delay value in milliseconds (1..5000)",
            "Delay:", "Set Delay", (long) currentDelay, 1, 5000, this);
        if (newDelay > 0) {
            if (!RegistrySetInteger(REG_CONFIG_SECTION, REG_ANIMATION_DELAY, (int) newDelay))
                return;
            // change delay if timer is running
            if (animationTimer.IsRunning()) {
                animationTimer.Stop();
                animationTimer.Start((int) newDelay, false);
            }
        }
    }
}

void StructureWindow::OnAnimationTimer(wxTimerEvent& event)
{
    if (animationMode == ANIM_FRAMES) {
        // simply pretend the user selected "next frame"
        Command(MID_NEXT_FRAME);
    }

    else if (animationMode == ANIM_SPIN) {
        // pretend the user dragged the mouse to the right
        glCanvas->renderer->ChangeView(OpenGLRenderer::eXYRotateHV, 3, 0);
        glCanvas->Refresh(false);
    }
}

void StructureWindow::OnSetFont(wxCommandEvent& event)
{
    string section, faceName;
    if (event.GetId() == MID_OPENGL_FONT)
        section = REG_OPENGL_FONT_SECTION;
    else if (event.GetId() == MID_SEQUENCE_FONT)
        section = REG_SEQUENCE_FONT_SECTION;
    else
        return;

    // get initial font info from registry, and create wxFont
    string nativeFont;
    RegistryGetString(section, REG_FONT_NATIVE_FONT_INFO, &nativeFont);
    auto_ptr<wxFont> initialFont(wxFont::New(wxString(nativeFont.c_str())));
    if (!initialFont.get() || !initialFont->Ok())
    {
        ERRORMSG("StructureWindow::OnSetFont() - error setting up initial font");
        return;
    }
    wxFontData initialFontData;
    initialFontData.SetInitialFont(*initialFont);

    // bring up font chooser dialog
#if wxCHECK_VERSION(2,3,3)
    wxFontDialog dialog(this, initialFontData);
#else
    wxFontDialog dialog(this, &initialFontData);
#endif
    int result = dialog.ShowModal();

    // if user selected a font
    if (result == wxID_OK) {

        // set registry values appropriately
        wxFontData& fontData = dialog.GetFontData();
        wxFont font = fontData.GetChosenFont();
        if (!RegistrySetString(section, REG_FONT_NATIVE_FONT_INFO, font.GetNativeFontInfoDesc().c_str()))
        {
            ERRORMSG("StructureWindow::OnSetFont() - error setting registry data");
            return;
        }

        // call font setup
        INFOMSG("setting new font");
        if (event.GetId() == MID_OPENGL_FONT) {
            glCanvas->SetCurrent();
            glCanvas->SetGLFontFromRegistry();
            GlobalMessenger()->PostRedrawAllStructures();
        } else if (event.GetId() == MID_SEQUENCE_FONT) {
            GlobalMessenger()->NewSequenceViewerFont();
        }
    }
}

static string GetFavoritesFile(bool forRead)
{
    // try to get value from registry
    string file;
    RegistryGetString(REG_CONFIG_SECTION, REG_FAVORITES_NAME, &file);

    // if not set, ask user for a folder, then set in registry
    if (file == NO_FAVORITES_FILE) {
        file = wxFileSelector("Select a file for favorites:",
            GetPrefsDir().c_str(), "Favorites", "", "*.*",
            forRead ? wxOPEN : wxSAVE | wxOVERWRITE_PROMPT).c_str();
        if (file.size() > 0)
            if (!RegistrySetString(REG_CONFIG_SECTION, REG_FAVORITES_NAME, file))
                ERRORMSG("Error setting favorites file in registry");
    }

    return file;
}

static bool LoadFavorites(void)
{
    string favoritesFile;
    RegistryGetString(REG_CONFIG_SECTION, REG_FAVORITES_NAME, &favoritesFile);
    if (favoritesFile != NO_FAVORITES_FILE) {
        favoriteStyles.Reset();
        if (wxFile::Exists(favoritesFile.c_str())) {
            INFOMSG("loading favorites from " << favoritesFile);
            string err;
            if (ReadASNFromFile(favoritesFile.c_str(), &favoriteStyles, false, &err)) {
                favoriteStylesChanged = false;
                return true;
            }
        } else {
            WARNINGMSG("Favorites file does not exist: " << favoritesFile);
            RegistrySetString(REG_CONFIG_SECTION, REG_FAVORITES_NAME, NO_FAVORITES_FILE);
        }
    }
    return false;
}

static void SaveFavorites(void)
{
    if (favoriteStylesChanged) {
        int choice = wxMessageBox("Do you want to save changes to your current Favorites file?",
            "Save favorites?", wxYES_NO);
        if (choice == wxYES) {
            string favoritesFile = GetFavoritesFile(false);
            if (favoritesFile != NO_FAVORITES_FILE) {
                string err;
                if (!WriteASNToFile(favoritesFile.c_str(), favoriteStyles, false, &err))
                    ERRORMSG("Error saving Favorites to " << favoritesFile << '\n' << err);
                favoriteStylesChanged = false;
            }
        }
    }
}

void StructureWindow::OnEditFavorite(wxCommandEvent& event)
{
    if (!glCanvas->structureSet) return;

    if (event.GetId() == MID_ADD_FAVORITE) {
        // get name from user
        wxString name = wxGetTextFromUser("Enter a name for this style:", "Input name", "", this);
        if (name.size() == 0) return;

        // replace style of same name
        CCn3d_style_settings *settings;
        CCn3d_style_settings_set::Tdata::iterator f, fe = favoriteStyles.Set().end();
        for (f=favoriteStyles.Set().begin(); f!=fe; f++) {
            if (Stricmp((*f)->GetName().c_str(), name.c_str()) == 0) {
                settings = f->GetPointer();
                break;
            }
        }
        // else add style to list
        if (f == favoriteStyles.Set().end()) {
            if (favoriteStyles.Set().size() >= MID_FAVORITES_END - MID_FAVORITES_BEGIN + 1) {
                ERRORMSG("Already have max # Favorites");
                return;
            } else {
                CRef < CCn3d_style_settings > ref(settings = new CCn3d_style_settings());
                favoriteStyles.Set().push_back(ref);
            }
        }

        // put in data from global style
        if (!glCanvas->structureSet->styleManager->GetGlobalStyle().SaveSettingsToASN(settings))
            ERRORMSG("Error converting global style to asn");
        settings->SetName(name.c_str());
        favoriteStylesChanged = true;
    }

    else if (event.GetId() == MID_REMOVE_FAVORITE) {
        wxString *choices = new wxString[favoriteStyles.Get().size()];
        int i = 0;
        CCn3d_style_settings_set::Tdata::iterator f, fe = favoriteStyles.Set().end();
        for (f=favoriteStyles.Set().begin(); f!=fe; f++)
            choices[i++] = (*f)->GetName().c_str();
        int picked = wxGetSingleChoiceIndex("Choose a style to remove from the Favorites list:",
            "Select for removal", favoriteStyles.Set().size(), choices, this);
        if (picked < 0 || picked >= favoriteStyles.Set().size()) return;
        for (f=favoriteStyles.Set().begin(), i=0; f!=fe; f++, i++) {
            if (i == picked) {
                favoriteStyles.Set().erase(f);
                favoriteStylesChanged = true;
                break;
            }
        }
    }

    else if (event.GetId() == MID_FAVORITES_FILE) {
        SaveFavorites();
        favoriteStyles.Reset();
        RegistrySetString(REG_CONFIG_SECTION, REG_FAVORITES_NAME, NO_FAVORITES_FILE);
        string newFavorites = GetFavoritesFile(true);
        if (newFavorites != NO_FAVORITES_FILE && wxFile::Exists(newFavorites.c_str())) {
            if (!LoadFavorites())
                ERRORMSG("Error loading Favorites from " << newFavorites.c_str());
        }
    }

    // update menu
    SetupFavoritesMenu();
}

void StructureWindow::SetupFavoritesMenu(void)
{
    int i;
    for (i=MID_FAVORITES_BEGIN; i<=MID_FAVORITES_END; i++) {
        wxMenuItem *item = favoritesMenu->FindItem(i);
        if (item) favoritesMenu->Delete(item);
    }

    CCn3d_style_settings_set::Tdata::const_iterator f, fe = favoriteStyles.Get().end();
    for (f=favoriteStyles.Get().begin(), i=0; f!=fe; f++, i++)
        favoritesMenu->Append(MID_FAVORITES_BEGIN + i, (*f)->GetName().c_str());
}

void StructureWindow::OnSelectFavorite(wxCommandEvent& event)
{
    if (!glCanvas->structureSet) return;

    if (event.GetId() >= MID_FAVORITES_BEGIN && event.GetId() <= MID_FAVORITES_END) {
        int index = event.GetId() - MID_FAVORITES_BEGIN;
        CCn3d_style_settings_set::Tdata::const_iterator f, fe = favoriteStyles.Get().end();
        int i = 0;
        for (f=favoriteStyles.Get().begin(); f!=fe; f++, i++) {
            if (i == index) {
                INFOMSG("using favorite: " << (*f)->GetName());
                glCanvas->structureSet->styleManager->SetGlobalStyle(**f);
                SetRenderingMenuFlag(0);
                SetColoringMenuFlag(0);
                break;
            }
        }
    }
}

void StructureWindow::OnShowWindow(wxCommandEvent& event)
{
    switch (event.GetId()) {
        case MID_SHOW_LOG:
            RaiseLogWindow();
            break;
        case MID_SHOW_SEQ_V:
            if (glCanvas->structureSet) glCanvas->structureSet->alignmentManager->ShowSequenceViewer();
            break;
        case MID_SHOW_LOG_START:
            RegistrySetBoolean(REG_CONFIG_SECTION, REG_SHOW_LOG_ON_START,
                menuBar->IsChecked(MID_SHOW_LOG_START));
            break;
    }
}

void StructureWindow::DialogTextChanged(const MultiTextDialog *changed)
{
    if (!changed || !glCanvas->structureSet) return;

    if (changed == cddNotesDialog) {
        StructureSet::TextLines lines;
        cddNotesDialog->GetLines(&lines);
        if (!glCanvas->structureSet->SetCDDNotes(lines))
            ERRORMSG("Error saving CDD notes");
    }
    if (changed == cddDescriptionDialog) {
        string line;
        cddDescriptionDialog->GetLine(&line);
        if (!glCanvas->structureSet->SetCDDDescription(line))
            ERRORMSG("Error saving CDD description");
    }
}

void StructureWindow::DialogDestroyed(const MultiTextDialog *destroyed)
{
    TRACEMSG("MultiTextDialog destroyed");
    if (destroyed == cddNotesDialog) cddNotesDialog = NULL;
    if (destroyed == cddDescriptionDialog) cddDescriptionDialog = NULL;
}

void StructureWindow::DestroyNonModalDialogs(void)
{
    if (cddAnnotateDialog) cddAnnotateDialog->Destroy();
    if (cddNotesDialog) cddNotesDialog->DestroyDialog();
    if (cddDescriptionDialog) cddDescriptionDialog->DestroyDialog();
    if (cddRefDialog) cddRefDialog->Destroy();
    if (cddOverview) cddOverview->Destroy();
}

void StructureWindow::OnPreferences(wxCommandEvent& event)
{
    PreferencesDialog dialog(this);
    int result = dialog.ShowModal();
}

bool StructureWindow::SaveDialog(bool prompt, bool canCancel)
{
    // check for whether save is necessary
    if (!glCanvas->structureSet || !glCanvas->structureSet->HasDataChanged())
        return true;

    int option = wxID_YES;

    if (prompt) {
        option = wxYES_NO | wxYES_DEFAULT | wxICON_EXCLAMATION | wxCENTRE;
        if (canCancel) option |= wxCANCEL;

        wxMessageDialog dialog(NULL, "Do you want to save your work to a file?", "", option);
        option = dialog.ShowModal();

        if (option == wxID_CANCEL) return false; // user cancelled this operation
    }

    if (option == wxID_YES) {
        wxCommandEvent event;
        event.SetId(prompt ? MID_SAVE_AS : MID_SAVE_SAME);
        OnSave(event);    // save data
    }

    return true;
}

// for sorting sequence list in reject dialog
typedef pair < const Sequence * , wxString > SeqAndDescr;
typedef vector < SeqAndDescr > SeqAndDescrList;
static bool CompareSequencesByIdentifier(const SeqAndDescr& a, const SeqAndDescr& b)
{
    return MoleculeIdentifier::CompareIdentifiers(
        a.first->identifier, b.first->identifier);
}

void StructureWindow::ShowCDDOverview(void)
{
    if (!cddOverview)
        cddOverview = new CDDSplashDialog(
            this, glCanvas->structureSet, &cddOverview,
            this, -1, "CDD Descriptive Items", wxPoint(200,50));
    cddOverview->Raise();
    cddOverview->Show(true);
}

void StructureWindow::ShowCDDAnnotations(void)
{
    if (!cddAnnotateDialog)
        cddAnnotateDialog = new CDDAnnotateDialog(this, &cddAnnotateDialog, glCanvas->structureSet);
    cddAnnotateDialog->Raise();
    cddAnnotateDialog->Show(true);
}

void StructureWindow::ShowCDDReferences(void)
{
    if (!cddRefDialog)
        cddRefDialog = new CDDRefDialog(
            glCanvas->structureSet, &cddRefDialog, this, -1, "CDD References");
    cddRefDialog->Raise();
    cddRefDialog->Show(true);
}

void StructureWindow::OnCDD(wxCommandEvent& event)
{
    if (!glCanvas->structureSet || !glCanvas->structureSet->IsCDD()) return;
    switch (event.GetId()) {

        case MID_CDD_OVERVIEW:
            ShowCDDOverview();
            break;

        case MID_EDIT_CDD_NAME: {
            wxString newName = wxGetTextFromUser("Enter or edit the CDD name:",
                "CDD Name", glCanvas->structureSet->GetCDDName().c_str(), this, -1, -1, false);
            if (newName.size() > 0) {
                if (!glCanvas->structureSet->SetCDDName(newName.c_str()))
                    ERRORMSG("Error saving CDD name");
                SetWorkingTitle(glCanvas->structureSet);
                GlobalMessenger()->SetAllWindowTitles();
            }
            break;
        }

        case MID_EDIT_CDD_DESCR:
            if (!cddDescriptionDialog) {
                StructureSet::TextLines line(1);
                line[0] = glCanvas->structureSet->GetCDDDescription().c_str();
                cddDescriptionDialog = new MultiTextDialog(this, line,
                    this, -1, "CDD Description", wxPoint(50, 50), wxSize(400, 200));
            }
            cddDescriptionDialog->ShowDialog(true);
            break;

        case MID_EDIT_CDD_NOTES:
            if (!cddNotesDialog) {
                StructureSet::TextLines lines;
                if (!glCanvas->structureSet->GetCDDNotes(&lines)) break;
                cddNotesDialog = new MultiTextDialog(this, lines,
                    this, -1, "CDD Notes", wxPoint(100, 100), wxSize(500, 400));
            }
            cddNotesDialog->ShowDialog(true);
            break;

        case MID_EDIT_CDD_REFERENCES:
            ShowCDDReferences();
            break;

        case MID_ANNOT_CDD:
            ShowCDDAnnotations();
            break;

        case MID_CDD_SHOW_REJECTS:
            glCanvas->structureSet->ShowRejects();
            break;

        case MID_CDD_REJECT_SEQ: {
            // make a list of slave sequences
            SeqAndDescrList seqsDescrs;
            const MoleculeIdentifier *master =
                glCanvas->structureSet->alignmentManager->
                    GetCurrentMultipleAlignment()->GetSequenceOfRow(0)->identifier;
            const StructureSet::RejectList *rejects = glCanvas->structureSet->GetRejects();
            SequenceSet::SequenceList::const_iterator
                s, se = glCanvas->structureSet->sequenceSet->sequences.end();
            for (s=glCanvas->structureSet->sequenceSet->sequences.begin(); s!=se; s++) {
                if ((*s)->identifier != master) {

                    // make sure this sequence isn't already rejected
                    bool rejected = false;
                    if (rejects) {
                        StructureSet::RejectList::const_iterator r, re = rejects->end();
                        for (r=rejects->begin(); r!=re; r++) {
                            CReject_id::TIds::const_iterator i, ie = (*r)->GetIds().end();
                            for (i=(*r)->GetIds().begin(); i!=ie; i++) {
                                if ((*s)->identifier->MatchesSeqId(**i)) {
                                    rejected = true;
                                    break;
                                }
                            }
                            if (rejected) break;
                        }
                    }
                    if (rejected) continue;

                    wxString description((*s)->identifier->ToString().c_str());
                    if ((*s)->description.size() > 0)
                        description += wxString("     ") + (*s)->description.c_str();
                    seqsDescrs.resize(seqsDescrs.size() + 1);
                    seqsDescrs.back().first = *s;
                    seqsDescrs.back().second = description;
                }
            }
            // sort by identifier
            stable_sort(seqsDescrs.begin(), seqsDescrs.end(), CompareSequencesByIdentifier);

            // user dialogs for selection and reason
            wxString *choices = new wxString[seqsDescrs.size()];
            int choice;
            for (choice=0; choice<seqsDescrs.size(); choice++) choices[choice] = seqsDescrs[choice].second;
            choice = wxGetSingleChoiceIndex("Reject which sequence?", "Reject Sequence",
                seqsDescrs.size(), choices, this);
            if (choice >= 0) {
                wxString message = "Are you sure you want to reject this sequence?\n\n";
                message += choices[choice];
                message += "\n\nIf so, enter a brief reason why:";
                wxString reason = wxGetTextFromUser(message, "Reject Sequence", "", this);
                if (reason.size() == 0) {
                    wxMessageBox("Reject action cancelled!", "", wxOK | wxICON_INFORMATION, this);
                } else {
                    int purge = wxMessageBox("Do you want to purge all instances of this sequence "
                        " from the multiple alignment and update list?",
                        "", wxYES_NO | wxICON_QUESTION, this);
                    // finally, actually perform the rejection+purge
                    glCanvas->structureSet->
                        RejectAndPurgeSequence(seqsDescrs[choice].first, reason.c_str(), purge == wxYES);
                }
            }
            break;
        }
    }
}

void StructureWindow::OnAdjustView(wxCommandEvent& event)
{
    glCanvas->SetCurrent();
    switch (event.GetId()) {
        case MID_ZOOM_IN:       glCanvas->renderer->ChangeView(OpenGLRenderer::eZoomIn); break;
        case MID_ZOOM_OUT:      glCanvas->renderer->ChangeView(OpenGLRenderer::eZoomOut); break;
        case MID_RESET:         glCanvas->renderer->ResetCamera(); break;
        case MID_RESTORE:       glCanvas->renderer->RestoreSavedView(); break;
        case MID_FIRST_FRAME:   glCanvas->renderer->ShowFirstFrame(); break;
        case MID_LAST_FRAME:    glCanvas->renderer->ShowLastFrame(); break;
        case MID_NEXT_FRAME:    glCanvas->renderer->ShowNextFrame(); break;
        case MID_PREV_FRAME:    glCanvas->renderer->ShowPreviousFrame(); break;
        case MID_ALL_FRAMES:    glCanvas->renderer->ShowAllFrames(); break;
        default:
            break;
    }
    glCanvas->Refresh(false);
}

void StructureWindow::OnShowHide(wxCommandEvent& event)
{
    if (glCanvas->structureSet) {

        switch (event.GetId()) {

        case MID_SHOW_HIDE:
        {
            vector < string > structureNames;
            vector < bool > structureVisibilities;
            glCanvas->structureSet->showHideManager->GetShowHideInfo(&structureNames, &structureVisibilities);
            wxString *titles = new wxString[structureNames.size()];
            for (int i=0; i<structureNames.size(); i++) titles[i] = structureNames[i].c_str();

            ShowHideDialog dialog(
                titles, &structureVisibilities, glCanvas->structureSet->showHideManager, false,
                this, -1, "Show/Hide Structures", wxPoint(200, 50));
            dialog.ShowModal();
            //delete titles;    // apparently deleted by wxWindows
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
        case MID_SHOW_UNALIGNED_ALL:
            glCanvas->structureSet->showHideManager->ShowResidues(glCanvas->structureSet, false);
            break;
        case MID_SHOW_UNALIGNED_ALN_DOMAIN:
            glCanvas->structureSet->showHideManager->
                ShowUnalignedResiduesInAlignedDomains(glCanvas->structureSet);
            break;
        case MID_SHOW_SELECTED_RESIDUES:
            glCanvas->structureSet->showHideManager->ShowSelectedResidues(glCanvas->structureSet);
            break;
        case MID_SHOW_SELECTED_DOMAINS:
            glCanvas->structureSet->showHideManager->ShowDomainsWithHighlights(glCanvas->structureSet);
            break;
        }
    }
}

void StructureWindow::OnAlignStructures(wxCommandEvent& event)
{
    if (glCanvas->structureSet) {
        glCanvas->structureSet->alignmentManager->RealignAllSlaveStructures();
        glCanvas->SetCurrent();
        glCanvas->Refresh(false);
    }
}

#define RENDERING_SHORTCUT(type, menu) \
    glCanvas->structureSet->styleManager->SetGlobalRenderingStyle(StyleSettings::type); \
    SetRenderingMenuFlag(menu); \
    break
#define COLORING_SHORTCUT(type, menu) \
    glCanvas->structureSet->styleManager->SetGlobalColorScheme(StyleSettings::type); \
    SetColoringMenuFlag(menu); \
    break

void StructureWindow::OnSetStyle(wxCommandEvent& event)
{
    if (glCanvas->structureSet) {
        glCanvas->SetCurrent();
        switch (event.GetId()) {
            case MID_EDIT_STYLE:
                if (!glCanvas->structureSet->styleManager->EditGlobalStyle(this))
                    return;
                SetRenderingMenuFlag(0);
                SetColoringMenuFlag(0);
                break;
            case MID_ANNOTATE:
                if (!glCanvas->structureSet->styleManager->EditUserAnnotations(this))
                    return;
                break;
            case MID_WORM: RENDERING_SHORTCUT(eWormShortcut, MID_WORM);
            case MID_TUBE: RENDERING_SHORTCUT(eTubeShortcut, MID_TUBE);
            case MID_WIRE: RENDERING_SHORTCUT(eWireframeShortcut, MID_WIRE);
            case MID_BNS: RENDERING_SHORTCUT(eBallAndStickShortcut, MID_BNS);
            case MID_SPACE: RENDERING_SHORTCUT(eSpacefillShortcut, MID_SPACE);
            case MID_SC_TOGGLE: RENDERING_SHORTCUT(eToggleSidechainsShortcut, 0);
            case MID_SECSTRUC: COLORING_SHORTCUT(eSecondaryStructureShortcut, MID_SECSTRUC);
            case MID_ALIGNED: COLORING_SHORTCUT(eAlignedShortcut, MID_ALIGNED);
            case MID_IDENTITY: COLORING_SHORTCUT(eIdentityShortcut, MID_IDENTITY);
            case MID_VARIETY: COLORING_SHORTCUT(eVarietyShortcut, MID_VARIETY);
            case MID_WGHT_VAR: COLORING_SHORTCUT(eWeightedVarietyShortcut, MID_WGHT_VAR);
            case MID_INFO: COLORING_SHORTCUT(eInformationContentShortcut, MID_INFO);
            case MID_FIT: COLORING_SHORTCUT(eFitShortcut, MID_FIT);
            case MID_BLOCK_FIT: COLORING_SHORTCUT(eBlockFitShortcut, MID_BLOCK_FIT);
            case MID_BLOCK_Z_FIT: COLORING_SHORTCUT(eBlockZFitShortcut, MID_BLOCK_Z_FIT);
            case MID_BLOCK_ROW_FIT: COLORING_SHORTCUT(eBlockRowFitShortcut, MID_BLOCK_ROW_FIT);
            case MID_OBJECT: COLORING_SHORTCUT(eObjectShortcut, MID_OBJECT);
            case MID_DOMAIN: COLORING_SHORTCUT(eDomainShortcut, MID_DOMAIN);
            case MID_MOLECULE: COLORING_SHORTCUT(eMoleculeShortcut, MID_MOLECULE);
            case MID_RAINBOW: COLORING_SHORTCUT(eRainbowShortcut, MID_RAINBOW);
            case MID_HYDROPHOB: COLORING_SHORTCUT(eHydrophobicityShortcut, MID_HYDROPHOB);
            case MID_CHARGE: COLORING_SHORTCUT(eChargeShortcut, MID_CHARGE);
            case MID_TEMP: COLORING_SHORTCUT(eTemperatureShortcut, MID_TEMP);
            case MID_ELEMENT: COLORING_SHORTCUT(eElementShortcut, MID_ELEMENT);
            default:
                return;
        }
        glCanvas->structureSet->styleManager->CheckGlobalStyleSettings();
        GlobalMessenger()->PostRedrawAllStructures();
        GlobalMessenger()->PostRedrawAllSequenceViewers();
    }
}

void StructureWindow::SetRenderingMenuFlag(int which)
{
    menuBar->Check(MID_WORM, (which == MID_WORM));
    menuBar->Check(MID_TUBE, (which == MID_TUBE));
    menuBar->Check(MID_WIRE, (which == MID_WIRE));
    menuBar->Check(MID_BNS, (which == MID_BNS));
    menuBar->Check(MID_SPACE, (which == MID_SPACE));
}

void StructureWindow::SetColoringMenuFlag(int which)
{
    menuBar->Check(MID_SECSTRUC, (which == MID_SECSTRUC));
    menuBar->Check(MID_ALIGNED, (which == MID_ALIGNED));
    menuBar->Check(MID_IDENTITY, (which == MID_IDENTITY));
    menuBar->Check(MID_VARIETY, (which == MID_VARIETY));
    menuBar->Check(MID_WGHT_VAR, (which == MID_WGHT_VAR));
    menuBar->Check(MID_INFO, (which == MID_INFO));
    menuBar->Check(MID_FIT, (which == MID_FIT));
    menuBar->Check(MID_BLOCK_FIT, (which == MID_BLOCK_FIT));
    menuBar->Check(MID_BLOCK_Z_FIT, (which == MID_BLOCK_Z_FIT));
    menuBar->Check(MID_BLOCK_ROW_FIT, (which == MID_BLOCK_ROW_FIT));
    menuBar->Check(MID_OBJECT, (which == MID_OBJECT));
    menuBar->Check(MID_DOMAIN, (which == MID_DOMAIN));
    menuBar->Check(MID_MOLECULE, (which == MID_MOLECULE));
    menuBar->Check(MID_RAINBOW, (which == MID_RAINBOW));
    menuBar->Check(MID_HYDROPHOB, (which == MID_HYDROPHOB));
    menuBar->Check(MID_CHARGE, (which == MID_CHARGE));
    menuBar->Check(MID_TEMP, (which == MID_TEMP));
    menuBar->Check(MID_ELEMENT, (which == MID_ELEMENT));
}

void StructureWindow::LoadFile(const char *filename)
{
    SetCursor(*wxHOURGLASS_CURSOR);
    glCanvas->SetCurrent();

    // clear old data
    if (glCanvas->structureSet) {
        DestroyNonModalDialogs();
        GlobalMessenger()->RemoveAllHighlights(false);
        delete glCanvas->structureSet;
        glCanvas->structureSet = NULL;
        glCanvas->renderer->AttachStructureSet(NULL);
        glCanvas->Refresh(false);
    }

    if (wxIsAbsolutePath(filename))
        userDir = string(wxPathOnly(filename).c_str()) + wxFILE_SEP_PATH;
    else if (wxPathOnly(filename) == "")
        userDir = GetWorkingDir();
    else
        userDir = GetWorkingDir() + wxPathOnly(filename).c_str() + wxFILE_SEP_PATH;
    INFOMSG("user dir: " << userDir.c_str());

    // try to decide if what ASN type this is, and if it's binary or ascii
    CNcbiIstream *inStream = new CNcbiIfstream(filename, IOS_BASE::in | IOS_BASE::binary);
    if (!(*inStream)) {
        ERRORMSG("Cannot open file '" << filename << "' for reading");
        SetCursor(wxNullCursor);
        delete inStream;
        return;
    }
    currentFile = wxFileNameFromPath(filename);

    string firstWord;
    *inStream >> firstWord;
    delete inStream;

    static const string
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

    // get current structure limit
    int structureLimit = kMax_Int;
    if (!RegistryGetInteger(REG_ADVANCED_SECTION, REG_MAX_N_STRUCTS, &structureLimit))
        WARNINGMSG("Can't get structure limit from registry");

    // try to read the file as various ASN types (if it's not clear from the first ascii word).
    // If read is successful, the StructureSet will own the asn data object, to keep it
    // around for output later on
    bool readOK = false;
    string err;
    if (!isCDD) {
        TRACEMSG("trying to read file '" << filename << "' as " <<
            ((isBinary) ? "binary" : "ascii") << " mime");
        CNcbi_mime_asn1 *mime = new CNcbi_mime_asn1();
        SetDiagPostLevel(eDiag_Fatal); // ignore all but Fatal errors while reading data
        readOK = ReadASNFromFile(filename, mime, isBinary, &err);
        SetDiagPostLevel(eDiag_Info);
        if (readOK) {
            glCanvas->structureSet = new StructureSet(mime, structureLimit, glCanvas->renderer);
            // if CDD is contained in a mime, then show CDD splash screen
            if (glCanvas->structureSet->IsCDD()) ShowCDDOverview();
        } else {
            TRACEMSG("error: " << err);
            delete mime;
        }
    }
    if (!readOK) {
        TRACEMSG("trying to read file '" << filename << "' as " <<
            ((isBinary) ? "binary" : "ascii") << " cdd");
        CCdd *cdd = new CCdd();
        SetDiagPostLevel(eDiag_Fatal); // ignore all but Fatal errors while reading data
        readOK = ReadASNFromFile(filename, cdd, isBinary, &err);
        SetDiagPostLevel(eDiag_Info);
        if (readOK) {
            glCanvas->structureSet = new StructureSet(cdd, structureLimit, glCanvas->renderer);
        } else {
            TRACEMSG("error: " << err);
            delete cdd;
        }
    }
    if (!readOK) {
        ERRORMSG("File not found, not readable, or is not a recognized data type");
        SetCursor(wxNullCursor);
        return;
    }

    SetWorkingTitle(glCanvas->structureSet);
    GlobalMessenger()->SetAllWindowTitles();

    // use a default style if no global style stored in asn data
    if (glCanvas->structureSet->hasUserStyle) {
        SetRenderingMenuFlag(0);
        SetColoringMenuFlag(0);
    } else {
        // set default rendering style and view, and turn on corresponding style menu flags
        if (glCanvas->structureSet->alignmentSet) {
            glCanvas->structureSet->styleManager->SetGlobalRenderingStyle(StyleSettings::eTubeShortcut);
            SetRenderingMenuFlag(MID_TUBE);
            if (glCanvas->structureSet->IsCDD()) {
                glCanvas->structureSet->styleManager->SetGlobalColorScheme(StyleSettings::eInformationContentShortcut);
                SetColoringMenuFlag(MID_INFO);
            } else {
                glCanvas->structureSet->styleManager->SetGlobalColorScheme(StyleSettings::eIdentityShortcut);
                SetColoringMenuFlag(MID_IDENTITY);
            }
        } else {
            glCanvas->structureSet->styleManager->SetGlobalRenderingStyle(StyleSettings::eWormShortcut);
            SetRenderingMenuFlag(MID_WORM);
            glCanvas->structureSet->styleManager->SetGlobalColorScheme(StyleSettings::eSecondaryStructureShortcut);
            SetColoringMenuFlag(MID_SECSTRUC);
        }
    }

    menuBar->EnableTop(menuBar->FindMenu("CDD"), glCanvas->structureSet->IsCDD());
    glCanvas->renderer->AttachStructureSet(glCanvas->structureSet);
    if (!glCanvas->renderer->HasASNViewSettings())
        glCanvas->structureSet->CenterViewOnAlignedResidues();
    glCanvas->Refresh(false);
    SetCursor(wxNullCursor);
}

void StructureWindow::OnOpen(wxCommandEvent& event)
{
    if (glCanvas->structureSet) {
        GlobalMessenger()->SequenceWindowsSave(true);   // give sequence window a chance to save
        SaveDialog(true, false);                        // give structure window a chance to save data
    }

    const wxString& filestr = wxFileSelector("Choose a text or binary ASN1 file to open", userDir.c_str(),
        "", "", "All Files|*.*|CDD (*.acd)|*.acd|Binary ASN (*.val)|*.val|ASCII ASN (*.prt)|*.prt",
        wxOPEN | wxFILE_MUST_EXIST);

    if (!filestr.IsEmpty())
        LoadFile(filestr.c_str());
}

void StructureWindow::OnSave(wxCommandEvent& event)
{
    if (!glCanvas->structureSet) return;

    // determine whether to prompt user for filename
    bool prompt = (event.GetId() == MID_SAVE_AS);
    if (!prompt) {
        wxString dir = wxString(userDir.c_str()).MakeLower();
        // always prompt if this file is stored in some temp folder (e.g. browser cache)
        if (dir.Contains("cache") || dir.Contains("temp") || dir.Contains("tmp"))
            prompt = true;
    }

    // force a save of any edits to alignment and updates first
    GlobalMessenger()->SequenceWindowsSave(prompt);

    wxString outputFolder = wxString(userDir.c_str(), userDir.size() - 1); // remove trailing /
    wxString outputFilename;

    if (prompt) {
        wxFileName fn(currentFile.c_str());
        wxFileDialog dialog(this, "Choose a filename for output", outputFolder,
#ifdef __WXGTK__
            fn.GetFullName(),
#else
            fn.GetName(),
#endif
            "All Files|*.*|Binary ASN (*.val)|*.val|ASCII CDD (*.acd)|*.acd|ASCII ASN (*.prt)|*.prt",
            wxSAVE | wxOVERWRITE_PROMPT);
        dialog.SetFilterIndex(fn.GetExt() == "val" ? 1 : (fn.GetExt() == "acd" ? 2 :
            (fn.GetExt() == "prt" ? 3 : 0)));
        if (dialog.ShowModal() == wxID_OK)
            outputFilename = dialog.GetPath();
    } else
        outputFilename = (userDir + currentFile).c_str();

    INFOMSG("save file: '" << outputFilename.c_str() << "'");

    if (!outputFilename.IsEmpty()) {
        glCanvas->structureSet->SaveASNData(outputFilename.c_str(), (outputFilename.Right(4) == ".val"));

#ifdef __WXMAC__
        // set mac file type and creator
        wxFileName wxfn(outputFilename);
        if (wxfn.FileExists())
            if (!wxfn.MacSetTypeAndCreator('TEXT', 'Cn3D'))
                WARNINGMSG("Unable to set Mac file type/creator");
#endif

        // set path/name/title
        if (wxIsAbsolutePath(outputFilename))
            userDir = string(wxPathOnly(outputFilename).c_str()) + wxFILE_SEP_PATH;
        else if (wxPathOnly(outputFilename) == "")
            userDir = GetWorkingDir();
        else
            userDir = GetWorkingDir() + wxPathOnly(outputFilename).c_str() + wxFILE_SEP_PATH;
        currentFile = wxFileNameFromPath(outputFilename);
        SetWorkingTitle(glCanvas->structureSet);
        GlobalMessenger()->SetAllWindowTitles();
    }
}

END_SCOPE(Cn3D)

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.3  2003/03/13 18:55:17  thiessen
* tweak file load error reporting
*
* Revision 1.2  2003/03/13 16:57:14  thiessen
* fix favorites load/save problem
*
* Revision 1.1  2003/03/13 14:26:18  thiessen
* add file_messaging module; split cn3d_main_wxwin into cn3d_app, cn3d_glcanvas, structure_window, cn3d_tools
*
*/
