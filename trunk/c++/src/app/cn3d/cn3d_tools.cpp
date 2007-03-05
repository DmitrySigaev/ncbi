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
*      Miscellaneous utility functions
*
* ===========================================================================
*/
#include <ncbi_pch.hpp>

#if defined(__WXMSW__)
#include <windows.h>
#include <shellapi.h>   // for ShellExecute, needed to launch browser

#elif defined(__WXGTK__)
#include <unistd.h>

#elif defined(__WXMAC__)
// full paths needed to void having to add -I/Developer/Headers/FlatCarbon to all modules...
#include "/Developer/Headers/FlatCarbon/Types.h"
#include "/Developer/Headers/FlatCarbon/InternetConfig.h"
#endif

#include <corelib/ncbistd.hpp>
#include <corelib/ncbireg.hpp>

#include <objects/seq/Bioseq.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objects/seqset/Bioseq_set.hpp>

#ifdef __WXMSW__
#include <windows.h>
#include <wx/msw/winundef.h>
#endif
#include <wx/wx.h>
#include <wx/file.h>
#include <wx/fileconf.h>

#include "cn3d_tools.hpp"
#include "asn_reader.hpp"

#include <memory>

USING_NCBI_SCOPE;
USING_SCOPE(objects);


BEGIN_SCOPE(Cn3D)

///// Registry stuff /////

static CNcbiRegistry registry;
static string registryFile;
static bool registryChanged = false;

static void SetRegistryDefaults(void)
{
    // default log window startup
    RegistrySetBoolean(REG_CONFIG_SECTION, REG_SHOW_LOG_ON_START, false);
    RegistrySetString(REG_CONFIG_SECTION, REG_FAVORITES_NAME, NO_FAVORITES_FILE);
    RegistrySetInteger(REG_CONFIG_SECTION, REG_MT_DIALOG_POS_X, 50);
    RegistrySetInteger(REG_CONFIG_SECTION, REG_MT_DIALOG_POS_Y, 50);
    RegistrySetInteger(REG_CONFIG_SECTION, REG_MT_DIALOG_SIZE_W, 400);
    RegistrySetInteger(REG_CONFIG_SECTION, REG_MT_DIALOG_SIZE_H, 400);

    // default animation controls
    RegistrySetInteger(REG_ANIMATION_SECTION, REG_SPIN_DELAY, 50);
    RegistrySetDouble(REG_ANIMATION_SECTION, REG_SPIN_INCREMENT, 2.0),
    RegistrySetInteger(REG_ANIMATION_SECTION, REG_FRAME_DELAY, 500);

    // default quality settings
    RegistrySetInteger(REG_QUALITY_SECTION, REG_QUALITY_ATOM_SLICES, 10);
    RegistrySetInteger(REG_QUALITY_SECTION, REG_QUALITY_ATOM_STACKS, 8);
    RegistrySetInteger(REG_QUALITY_SECTION, REG_QUALITY_BOND_SIDES, 6);
    RegistrySetInteger(REG_QUALITY_SECTION, REG_QUALITY_WORM_SIDES, 6);
    RegistrySetInteger(REG_QUALITY_SECTION, REG_QUALITY_WORM_SEGMENTS, 6);
    RegistrySetInteger(REG_QUALITY_SECTION, REG_QUALITY_HELIX_SIDES, 12);
    RegistrySetBoolean(REG_QUALITY_SECTION, REG_HIGHLIGHTS_ON, true);
    RegistrySetString(REG_QUALITY_SECTION, REG_PROJECTION_TYPE, "Perspective");

    if (IsWindowedMode()) {
        // default font for OpenGL (structure window)
        wxFont *font = wxFont::New(
#if defined(__WXMSW__)
            12,
#elif defined(__WXGTK__)
            14,
#elif defined(__WXMAC__)
            14,
#endif
            wxSWISS, wxNORMAL, wxBOLD, false);
        if (font && font->Ok())
            RegistrySetString(REG_OPENGL_FONT_SECTION, REG_FONT_NATIVE_FONT_INFO,
			    font->GetNativeFontInfoDesc().c_str());
        else
            ERRORMSG("Can't create default structure window font");

        if (font) delete font;

        // default font for sequence viewers
        font = wxFont::New(
#if defined(__WXMSW__)
            10,
#elif defined(__WXGTK__)
            14,
#elif defined(__WXMAC__)
            12,
#endif
            wxROMAN, wxNORMAL, wxNORMAL, false);
        if (font && font->Ok())
            RegistrySetString(REG_SEQUENCE_FONT_SECTION, REG_FONT_NATIVE_FONT_INFO,
			    font->GetNativeFontInfoDesc().c_str());
        else
            ERRORMSG("Can't create default sequence window font");
        if (font) delete font;
    }

    // default cache settings
    RegistrySetBoolean(REG_CACHE_SECTION, REG_CACHE_ENABLED, true);
    if (GetPrefsDir().size() > 0)
        RegistrySetString(REG_CACHE_SECTION, REG_CACHE_FOLDER, GetPrefsDir() + "cache");
    else
        RegistrySetString(REG_CACHE_SECTION, REG_CACHE_FOLDER, GetProgramDir() + "cache");
    RegistrySetInteger(REG_CACHE_SECTION, REG_CACHE_MAX_SIZE, 25);

    // default advanced options
    RegistrySetBoolean(REG_ADVANCED_SECTION, REG_CDD_ANNOT_READONLY, true);
#ifdef __WXGTK__
    RegistrySetString(REG_ADVANCED_SECTION, REG_BROWSER_LAUNCH,
        // for launching netscape in a separate window
        "( netscape -noraise -remote 'openURL(<URL>,new-window)' || netscape '<URL>' ) >/dev/null 2>&1 &"
        // for launching netscape in an existing window
//        "( netscape -raise -remote 'openURL(<URL>)' || netscape '<URL>' ) >/dev/null 2>&1 &"
    );
#endif
    RegistrySetInteger(REG_ADVANCED_SECTION, REG_MAX_N_STRUCTS, 10);
    RegistrySetInteger(REG_ADVANCED_SECTION, REG_FOOTPRINT_RES, 0);

    // default stereo options
    RegistrySetDouble(REG_ADVANCED_SECTION, REG_STEREO_SEPARATION, 5.0);
    RegistrySetBoolean(REG_ADVANCED_SECTION, REG_PROXIMAL_STEREO, true);
}

void LoadRegistry(void)
{
    // first set up defaults, then override any/all with stuff from registry file
    SetRegistryDefaults();

    if (GetPrefsDir().size() > 0)
        registryFile = GetPrefsDir() + "Preferences";
    else
        registryFile = GetProgramDir() + "Preferences";
    auto_ptr<CNcbiIfstream> iniIn(new CNcbiIfstream(registryFile.c_str(), IOS_BASE::in));
    if (*iniIn) {
        TRACEMSG("loading program registry " << registryFile);
        registry.Read(*iniIn);
    }

    registryChanged = false;
}

void SaveRegistry(void)
{
    if (registryChanged) {
        auto_ptr<CNcbiOfstream> iniOut(new CNcbiOfstream(registryFile.c_str(), IOS_BASE::out));
        if (*iniOut) {
//            TESTMSG("saving program registry " << registryFile);
            registry.Write(*iniOut);
        }
    }
}

bool RegistryIsValidInteger(const string& section, const string& name)
{
    long value;
    wxString regStr = registry.Get(section, name).c_str();
    return (regStr.size() > 0 && regStr.ToLong(&value));
}

bool RegistryIsValidDouble(const string& section, const string& name)
{
    double value;
    wxString regStr = registry.Get(section, name).c_str();
    return (regStr.size() > 0 && regStr.ToDouble(&value));
}

bool RegistryIsValidBoolean(const string& section, const string& name)
{
    string regStr = registry.Get(section, name);
    return (regStr.size() > 0 && (
        toupper((unsigned char) regStr[0]) == 'T' || toupper((unsigned char) regStr[0]) == 'F' ||
        toupper((unsigned char) regStr[0]) == 'Y' || toupper((unsigned char) regStr[0]) == 'N'));
}

bool RegistryIsValidString(const string& section, const string& name)
{
    string regStr = registry.Get(section, name);
    return (regStr.size() > 0);
}

bool RegistryGetInteger(const string& section, const string& name, int *value)
{
    wxString regStr = registry.Get(section, name).c_str();
    long l;
    if (regStr.size() == 0 || !regStr.ToLong(&l)) {
        WARNINGMSG("Can't get long from registry: " << section << ", " << name);
        return false;
    }
    *value = (int) l;
    return true;
}

bool RegistryGetDouble(const string& section, const string& name, double *value)
{
    wxString regStr = registry.Get(section, name).c_str();
    if (regStr.size() == 0 || !regStr.ToDouble(value)) {
        WARNINGMSG("Can't get double from registry: " << section << ", " << name);
        return false;
    }
    return true;
}

bool RegistryGetBoolean(const string& section, const string& name, bool *value)
{
    string regStr = registry.Get(section, name);
    if (regStr.size() == 0 || !(
            toupper((unsigned char) regStr[0]) == 'T' || toupper((unsigned char) regStr[0]) == 'F' ||
            toupper((unsigned char) regStr[0]) == 'Y' || toupper((unsigned char) regStr[0]) == 'N')) {
        WARNINGMSG("Can't get boolean from registry: " << section << ", " << name);
        return false;
    }
    *value = (toupper((unsigned char) regStr[0]) == 'T' || toupper((unsigned char) regStr[0]) == 'Y');
    return true;
}

bool RegistryGetString(const string& section, const string& name, string *value)
{
    string regStr = registry.Get(section, name);
    if (regStr.size() == 0) {
        WARNINGMSG("Can't get string from registry: " << section << ", " << name);
        return false;
    }
    *value = regStr;
    return true;
}

bool RegistrySetInteger(const string& section, const string& name, int value)
{
    wxString regStr;
    regStr.Printf("%i", value);
    bool okay = registry.Set(section, name, regStr.c_str(), CNcbiRegistry::ePersistent);
    if (!okay)
        ERRORMSG("registry Set(" << section << ", " << name << ") failed");
    else
        registryChanged = true;
    return okay;
}

bool RegistrySetDouble(const string& section, const string& name, double value)
{
    wxString regStr;
    regStr.Printf("%g", value);
    bool okay = registry.Set(section, name, regStr.c_str(), CNcbiRegistry::ePersistent);
    if (!okay)
        ERRORMSG("registry Set(" << section << ", " << name << ") failed");
    else
        registryChanged = true;
    return okay;
}

bool RegistrySetBoolean(const string& section, const string& name, bool value, bool useYesOrNo)
{
    string regStr;
    if (useYesOrNo)
        regStr = value ? "yes" : "no";
    else
        regStr = value ? "true" : "false";
    bool okay = registry.Set(section, name, regStr, CNcbiRegistry::ePersistent);
    if (!okay)
        ERRORMSG("registry Set(" << section << ", " << name << ") failed");
    else
        registryChanged = true;
    return okay;
}

bool RegistrySetString(const string& section, const string& name, const string& value)
{
    bool okay = registry.Set(section, name, value, CNcbiRegistry::ePersistent);
    if (!okay)
        ERRORMSG("registry Set(" << section << ", " << name << ") failed");
    else
        registryChanged = true;
    return okay;
}


///// Misc stuff /////

// global strings for various directories - will include trailing path separator character
static string
    workingDir,     // current working directory
    programDir,     // directory where Cn3D executable lives
    dataDir,        // 'data' directory with external data files
    prefsDir;       // application preferences directory
const string& GetWorkingDir(void) { return workingDir; }
const string& GetProgramDir(void) { return programDir; }
const string& GetDataDir(void) { return dataDir; }
const string& GetPrefsDir(void) { return prefsDir; }

void SetUpWorkingDirectories(const char* argv0)
{
    // set up working directories
    workingDir = wxGetCwd().c_str();
#ifdef __WXGTK__
    if (getenv("CN3D_HOME") != NULL)
        programDir = getenv("CN3D_HOME");
    else
#endif
    if (wxIsAbsolutePath(argv0))
        programDir = wxPathOnly(argv0).c_str();
    else if (wxPathOnly(argv0) == "")
        programDir = workingDir;
    else
        programDir = workingDir + wxFILE_SEP_PATH + wxPathOnly(argv0).c_str();
    workingDir = workingDir + wxFILE_SEP_PATH;
    programDir = programDir + wxFILE_SEP_PATH;

    // find or create preferences folder
    wxString localDir;
    wxSplitPath((wxFileConfig::GetLocalFileName("unused")).c_str(), &localDir, NULL, NULL);
    wxString prefsDirLocal = localDir + wxFILE_SEP_PATH + "Cn3D_User";
    wxString prefsDirProg = wxString(programDir.c_str()) + wxFILE_SEP_PATH + "Cn3D_User";
    if (wxDirExists(prefsDirLocal))
        prefsDir = prefsDirLocal.c_str();
    else if (wxDirExists(prefsDirProg))
        prefsDir = prefsDirProg.c_str();
    else {
        // try to create the folder
        if (wxMkdir(prefsDirLocal) && wxDirExists(prefsDirLocal))
            prefsDir = prefsDirLocal.c_str();
        else if (wxMkdir(prefsDirProg) && wxDirExists(prefsDirProg))
            prefsDir = prefsDirProg.c_str();
    }
    if (prefsDir.size() == 0)
        WARNINGMSG("Can't create Cn3D_User folder at either:"
            << "\n    " << prefsDirLocal
            << "\nor  " << prefsDirProg);
    else
        prefsDir += wxFILE_SEP_PATH;

    // set data dir, and register the path in C toolkit registry (mainly for BLAST code)
#ifdef __WXMAC__
    dataDir = programDir + "../Resources/data/";
#else
    dataDir = programDir + "data" + wxFILE_SEP_PATH;
#endif

    TRACEMSG("working dir: " << workingDir.c_str());
    TRACEMSG("program dir: " << programDir.c_str());
    TRACEMSG("data dir: " << dataDir.c_str());
    TRACEMSG("prefs dir: " << prefsDir.c_str());
}

#ifdef __WXMSW__
// code borrowed (and modified) from Nlm_MSWin_OpenDocument() in vibutils.c
static bool MSWin_OpenDocument(const char* doc_name)
{
    int status = (int) ShellExecute(0, "open", doc_name, NULL, NULL, SW_SHOWNORMAL);
    if (status <= 32) {
        ERRORMSG("Unable to open document \"" << doc_name << "\", error = " << status);
        return false;
    }
    return true;
}
#endif

#ifdef __WXMAC__
static OSStatus MacLaunchURL(ConstStr255Param urlStr)
{
    OSStatus err;
    ICInstance inst;
    SInt32 startSel;
    SInt32 endSel;

    err = ICStart(&inst, 'Cn3D');
    if (err == noErr) {
#if !TARGET_CARBON
        err = ICFindConfigFile(inst, 0, nil);
#endif
        if (err == noErr) {
            startSel = 0;
            endSel = strlen(urlStr);
            err = ICLaunchURL(inst, "\p", urlStr, endSel, &startSel, &endSel);
        }
        ICStop(inst);
    }
    return err;
}
#endif

void LaunchWebPage(const char *url)
{
    if(!url) return;
    INFOMSG("launching url " << url);

#if defined(__WXMSW__)
    if (!MSWin_OpenDocument(url)) {
        ERRORMSG("Unable to launch browser");
    }

#elif defined(__WXGTK__)
    string command;
    RegistryGetString(REG_ADVANCED_SECTION, REG_BROWSER_LAUNCH, &command);
    size_t pos = 0;
    while ((pos=command.find("<URL>", pos)) != string::npos)
        command.replace(pos, 5, url);
    TRACEMSG("launching browser with: " << command);
    system(command.c_str());

#elif defined(__WXMAC__)
    MacLaunchURL(url);
#endif
}

CRef < CBioseq > FetchSequenceViaHTTP(const string& id)
{
    CSeq_entry seqEntry;
    string err;
    static const string host("www.ncbi.nlm.nih.gov"), path("/entrez/viewer.fcgi");
    string args = string("view=0&maxplex=1&save=idf&val=") + id;
    INFOMSG("Trying to load sequence from URL " << host << path << '?' << args);

    CRef < CBioseq > bioseq;
    if (GetAsnDataViaHTTP(host, path, args, &seqEntry, &err)) {
        if (seqEntry.IsSeq())
            bioseq.Reset(&(seqEntry.SetSeq()));
        else if (seqEntry.IsSet() && seqEntry.GetSet().GetSeq_set().front()->IsSeq())
            bioseq.Reset(&(seqEntry.SetSet().SetSeq_set().front()->SetSeq()));
        else
            ERRORMSG("FetchSequenceViaHTTP() - confused by SeqEntry format");
    }
    if (bioseq.Empty())
        ERRORMSG("FetchSequenceViaHTTP() - HTTP Bioseq retrieval failed, err: " << err);

    return bioseq;
}

static const string NCBIStdaaResidues("-ABCDEFGHIKLMNPQRSTVWXYZU*OJ");

// gives NCBIStdaa residue number for a character (or value for 'X' if char not found)
unsigned char LookupNCBIStdaaNumberFromCharacter(char r)
{
    typedef map < char, unsigned char > Char2UChar;
    static Char2UChar charMap;

    if (charMap.size() == 0) {
        for (unsigned int i=0; i<NCBIStdaaResidues.size(); ++i)
            charMap[NCBIStdaaResidues[i]] = (unsigned char) i;
    }

    Char2UChar::const_iterator n = charMap.find(toupper((unsigned char) r));
    if (n != charMap.end())
        return n->second;
    else
        return charMap.find('X')->second;
}

char LookupCharacterFromNCBIStdaaNumber(unsigned char n)
{
    if (n <= 27)
        return NCBIStdaaResidues[n];
    ERRORMSG("LookupCharacterFromNCBIStdaaNumber() - valid values are 0 - 27");
    return '?';
}

END_SCOPE(Cn3D)
