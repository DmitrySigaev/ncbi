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
 * Authors:  Vsevolod Sandomirskiy, Denis Vakatov
 *
 * File Description:
 *   CNcbiApplication -- a generic NCBI application class
 *   CCgiApplication  -- a NCBI CGI-application class
 *
 */

#include <corelib/ncbienv.hpp>
#include <corelib/ncbireg.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbifile.hpp>
#include <corelib/ncbiapp.hpp>


BEGIN_NCBI_SCOPE


/////////////////////////////////////////////////////////////////////////////
//  Constants
//

static const char* s_ArgLogFile = "-logfile";
static const char* s_ArgCfgFile = "-conffile";
static const char* s_ArgVersion = "-version";


static void s_DiagToStdlog_Cleanup(void* data)
{  // SetupDiag(eDS_ToStdlog)
    CNcbiOfstream* os_log = static_cast<CNcbiOfstream*>(data);
    delete os_log;
}

///////////////////////////////////////////////////////
// CNcbiApplication
//

CNcbiApplication* CNcbiApplication::m_Instance;


CNcbiApplication* CNcbiApplication::Instance(void)
{
    return m_Instance;
}


CNcbiApplication::CNcbiApplication(void)
{
    m_DisableArgDesc = false;
    m_HideArgs = 0;

    // Register the app. instance
    if ( m_Instance ) {
        NCBI_THROW(CAppException,eSecond,
            "Second instance of CNcbiApplication is prohibited");
    }
    m_Instance = this;

    // Create empty version info
    m_Version.reset(new CVersionInfo(0,0));

    // Create empty application arguments & name
    m_Arguments.reset(new CNcbiArguments(0,0));

    // Create empty application environment
    m_Environ.reset(new CNcbiEnvironment);

    // Create an empty registry
    m_Config.reset(new CNcbiRegistry);

}


CNcbiApplication::~CNcbiApplication(void)
{
    m_Instance = 0;
    FlushDiag(0, true);
}


void CNcbiApplication::Init(void)
{
    return;
}


void CNcbiApplication::Exit(void)
{
    return;
}


SIZE_TYPE CNcbiApplication::FlushDiag(CNcbiOstream* os, bool close_diag)
{
    // dyn.cast to CNcbiOstrstream
    CNcbiOstrstream* ostr = dynamic_cast<CNcbiOstrstream*>(m_DiagStream.get());
    if ( !ostr ) {
        _ASSERT( !m_DiagStream.get() );
        return 0;
    }

    // dump all content to "os"
    SIZE_TYPE n_write = 0;
    if ( os )
        n_write = ostr->pcount();

    if ( n_write ) {
        os->write(ostr->str(), n_write);
        ostr->rdbuf()->freeze(0);
    }

    // reset output buffer or destroy
    if ( close_diag ) {
        if ( IsDiagStream(m_DiagStream.get()) ) {
            SetDiagStream(0);
        }
        m_DiagStream.reset(0);
    } else {
        ostr->rdbuf()->SEEKOFF(0, IOS_BASE::beg, IOS_BASE::out);
    }

    // return # of bytes dumped to "os"
    return (os  &&  os->good()) ? n_write : 0;
}


int CNcbiApplication::AppMain
(int                argc,
 const char* const* argv,
 const char* const* envp,
 EAppDiagStream     diag,
 const char*        conf,
 const string&      name)
{
    // SUN WorkShop STL stream library has significant performance loss when
    // sync_with_stdio is true (default)
    // So we turn off sync_with_stdio here:
    IOS_BASE::sync_with_stdio(false);

    // Get program name
    string appname = name;
    if (appname.empty()) {
        if (argc > 0  &&  argv[0] != NULL  &&  *argv[0] != '\0') {
            appname = argv[0];
        } else {
            appname = "ncbi";
        }
    }

    // We do not know standard way of passing arguments to C++ program on Mac,
    // so we will read arguments from special file having extension ".args"
    // and name equal to name of program (name argument of AppMain).
#if defined(NCBI_OS_MAC)
#  define MAX_ARGC 256
#  define MAX_ARG_LEN 1024
    if (argc <= 1) {
        string argsName = appname + ".args";

        CNcbiIfstream in(argsName.c_str());
        if ( in ) {
            int c = 1;
            const char** v = new const char*[MAX_ARGC];
            v[0] = strdup(appname.c_str()); // program name
            char arg[MAX_ARG_LEN];
            while (in.getline(arg, sizeof(arg))  ||  in.gcount()) {
                if ( in.eof() ) {
                    ERR_POST(Warning << argsName << ", line " << c << ": " <<
                             "unfinished last line");
                } else if ( in.fail() ) {
                    ERR_POST(Fatal << argsName << ", line " << c << ": " <<
                             "too long argument: " << arg);
                }
                if (c >= MAX_ARGC) {
                    ERR_POST(Fatal << argsName << ", line " << c << ": " <<
                             "too many arguments");
                }
                v[c++] = strdup(arg);
            }
            argc = c;
            argv = v;
        }
        else {
            ERR_POST(argsName << ": file not found");
        }
    }
#endif

    // Check command line for presence special arguments
    // "-logfile", "-conffile", "-version"
    bool is_diag_setup = false;
    if (!m_DisableArgDesc && argc > 1  &&  argv) {
        const char** v = new const char*[argc];
        v[0] = argv[0];
        int real_arg_index = 1;
        for (int i = 1;  i < argc;  i++) {
            if ( !argv[i] ) {
                continue;
            }
            // Log file
            if ( NStr::strcmp(argv[i], s_ArgLogFile) == 0 ) {
                if ( !argv[i++] ) {
                    continue;
                }
                const char* log = argv[i];
                auto_ptr<CNcbiOfstream> os(new CNcbiOfstream(log));
                if ( !os->good() ) {
                    _TRACE("CNcbiApplication() -- cannot open log file: "
                           << log);
                    continue;
                }
                _TRACE("CNcbiApplication() -- opened log file: " << log);
                // (re)direct the global diagnostics to the log.file
                CNcbiOfstream* os_log = os.release();
                SetDiagStream(os_log, true, s_DiagToStdlog_Cleanup,
                              (void*) os_log);
                diag = eDS_ToStdlog;
                is_diag_setup = true;

            // Configuration file
            } else if ( NStr::strcmp(argv[i], s_ArgCfgFile) == 0 ) {
                if ( !argv[i++] ) {
                    continue;
                }
                conf = argv[i];

            // Version
            } else if ( NStr::strcmp(argv[i], s_ArgVersion) == 0 ) {
                if ( !argv[i++] ) {
                    continue;
                }
                // Print USAGE
                LOG_POST(appname + ": " + GetVersion().Print());
                return 0;

            // Save real argument
            } else {
                v[real_arg_index++] = argv[i];
            }
        }
        if (real_arg_index == argc ) {
            delete[] v;
        } else {
            argc = real_arg_index;
            argv = v;
        }
    }

    // Reset command-line args and application name
    m_Arguments->Reset(argc, argv, appname);

    // Reset application environment
    m_Environ->Reset(envp);

    // Setup some debugging features
    if ( !m_Environ->Get(DIAG_TRACE).empty() ) {
        SetDiagTrace(eDT_Enable, eDT_Enable);
    }
    string post_level = m_Environ->Get(DIAG_POST_LEVEL);
    if ( !post_level.empty() ) {
        EDiagSev sev;
        if (CNcbiDiag::StrToSeverityLevel(post_level.c_str(), sev)) {
            SetDiagFixedPostLevel(sev);
        }
    }
    if ( !m_Environ->Get(ABORT_ON_THROW).empty() ) {
        SetThrowTraceAbort(true);
    }

    // Clear registry content
    m_Config->Clear();

    // Setup for diagnostics
    try {
        if ( !is_diag_setup  &&  !SetupDiag(diag) ) {
            ERR_POST(
                "Application diagnostic stream's setup failed");
        }
    } catch (CException& e) {
        NCBI_RETHROW(e,CAppException, eSetupDiag,
            "Application diagnostic stream's setup failed");
    } catch (exception& e) {
        NCBI_THROW(CAppException, eSetupDiag,
            string("Application diagnostic stream's setup failed") +
            string(": ") + e.what());
    }

    // Load registry from the config file
    try {
        if ( conf ) {
            string x_conf(conf);
            LoadConfig(*m_Config, &x_conf);
        }
    } catch (CException& e) {
        NCBI_RETHROW(e,CAppException, eLoadConfig,
            "Registry data cannot be loaded");
    } catch (exception& e) {
        NCBI_THROW(CAppException, eLoadConfig,
            string("Registry data cannot be loaded") +
            string(": ") + e.what());
    }

    // Setup some debugging features
    if ( !m_Config->Get("DEBUG", DIAG_TRACE).empty() ) {
        SetDiagTrace(eDT_Enable, eDT_Enable);
    }
    if ( !m_Config->Get("DEBUG", ABORT_ON_THROW).empty() ) {
        SetThrowTraceAbort(true);
    }
    post_level = m_Config->Get("DEBUG", DIAG_POST_LEVEL);
    if ( !post_level.empty() ) {
        EDiagSev sev;
        if (CNcbiDiag::StrToSeverityLevel(post_level.c_str(), sev)) {
            SetDiagFixedPostLevel(sev);
        }
    }
    string msg_file = m_Config->Get("DEBUG", DIAG_MESSAGE_FILE);
    if ( !msg_file.empty() ) {
        CDiagErrCodeInfo* info = new CDiagErrCodeInfo();
        if ( !info  ||  !info->Read(msg_file) ) {
            if ( info ) {
                delete info;
            }
            ERR_POST(Warning << "Applications message file \""
                     << msg_file
                     << "\" is not found");
        } else {
            SetDiagErrCodeInfo(info);
        }
    }

    // Call:  Init() + Run() + Exit()
    int exit_code = 1;

    try {
        // Init application
        try {
            Init();
            // If the app still has no arg description - provide default one
            if (!m_DisableArgDesc && !m_ArgDesc.get()) {
                auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);
                arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                    "This program has no mandatory arguments");
                SetupArgDescriptions(arg_desc.release());
            }
        }
        catch (CArgHelpException&) {
            if (!m_DisableArgDesc) {
                if ((m_HideArgs & fHideHelp) != 0)
                {
                    if (m_ArgDesc->Exist("h")) {
                        m_ArgDesc->Delete("h");
                    }
                }
                if ((m_HideArgs & fHideLogfile) == 0 &&
                    !m_ArgDesc->Exist(s_ArgLogFile+1)) {
                    m_ArgDesc->AddOptionalKey( s_ArgLogFile+1, "File_Name",
                        "File to which the program log should be redirected",
                        CArgDescriptions::eOutputFile);
                }
                if ((m_HideArgs & fHideConffile) == 0 &&
                    !m_ArgDesc->Exist(s_ArgCfgFile+1)) {
                    m_ArgDesc->AddOptionalKey( s_ArgCfgFile+1, "File_Name",
                        "Program's configuration (registry) data file",
                        CArgDescriptions::eInputFile);
                }
                if ((m_HideArgs & fHideVersion) == 0 &&
                    !m_ArgDesc->Exist(s_ArgVersion+1)) {
                    m_ArgDesc->AddFlag( s_ArgVersion+1,
                        "Print version number;  ignore other arguments");
                }
            }
            // Print USAGE
            string str;
            LOG_POST(string(72, '='));
            LOG_POST(m_ArgDesc->PrintUsage(str));
            exit_code = 0;
        }
        catch (CArgException& e) {
            NCBI_RETHROW_SAME(e,"Application's initialization failed");
        }
        catch (CException& e) {
            NCBI_REPORT_EXCEPTION(
                "Application's initialization failed",e);
        }
        catch (exception& e) {
            ERR_POST("Application's initialization failed"
                << ": " << e.what());
            exit_code = -2;
        }

        // Run application
        if (exit_code == 1) {
            try {
                exit_code = Run();
            }
            catch (CArgException& e) {
                NCBI_RETHROW_SAME(e,"Application's execution failed");
            }
            catch (CException& e) {
                NCBI_REPORT_EXCEPTION(
                    "Application's execution failed",e);
            }
            catch (exception& e) {
                ERR_POST("Application's execution failed"
                    << ": " << e.what());
                exit_code = -3;
            }
        }

        // Close application
        try {
            Exit();
        }
        catch (CArgException& e) {
            NCBI_RETHROW_SAME(e,"Application's cleanup failed");
        }
        catch (CException& e) {
            NCBI_REPORT_EXCEPTION(
                "Application's cleanup failed",e);
        }
        catch (exception& e) {
            ERR_POST("Application's cleanup failed"
                << ": " << e.what());
        }
    }
    catch (CArgException& e) {
        // Print USAGE and the exception error message
        string str;
        LOG_POST(string(72, '='));
        if ( m_ArgDesc.get() ) {
            LOG_POST(m_ArgDesc->PrintUsage(str) << string(72, '='));
        }
        NCBI_REPORT_EXCEPTION("",e);
        exit_code = -1;
    }

    // Exit
    return exit_code;
}


// Set program version
void CNcbiApplication::SetVersion(const CVersionInfo& version)
{
    m_Version.reset(new CVersionInfo(version));
}


// Get program version
CVersionInfo CNcbiApplication::GetVersion(void)
{
    return *m_Version;
}


void CNcbiApplication::SetupArgDescriptions(CArgDescriptions* arg_desc)
{
    m_ArgDesc.reset(arg_desc);

    if ( arg_desc ) {
        m_Args.reset(arg_desc->CreateArgs(GetArguments()));
    } else {
        m_Args.reset();
    }
}


bool CNcbiApplication::SetupDiag(EAppDiagStream diag)
{
    // Setup diagnostic stream
    switch ( diag ) {
    case eDS_ToStdout: {
        SetDiagStream(&NcbiCout);
        break;
    }
    case eDS_ToStderr: {
        SetDiagStream(&NcbiCerr);
        break;
    }
    case eDS_ToStdlog: {
        // open log.file
        string log = m_Arguments->GetProgramName() + ".log";
        auto_ptr<CNcbiOfstream> os(new CNcbiOfstream(log.c_str()));
        if ( !os->good() ) {
            _TRACE("CNcbiApplication() -- cannot open log file: " << log);
            return false;
        }
        _TRACE("CNcbiApplication() -- opened log file: " << log);

        // (re)direct the global diagnostics to the log.file
        CNcbiOfstream* os_log = os.release();
        SetDiagStream(os_log, true, s_DiagToStdlog_Cleanup, (void*) os_log);
        break;
    }
    case eDS_ToMemory: {
        // direct global diagnostics to the memory-resident output stream
        if ( !m_DiagStream.get() ) {
            m_DiagStream.reset(new CNcbiOstrstream);
        }
        SetDiagStream(m_DiagStream.get());
        break;
    }
    case eDS_Disable: {
        SetDiagStream(0);
        break;
    }
    case eDS_User: {
        // dont change current diag.stream
        break;
    }
    case eDS_AppSpecific: {
        return SetupDiag_AppSpecific();
    }
    case eDS_Default: {
        if ( !IsSetDiagHandler() ) {
            return CNcbiApplication::SetupDiag(eDS_AppSpecific);
        }
        // else eDS_User -- dont change current diag.stream
        break;
    }
    default: {
        _ASSERT(0);
        break;
    }
    } // switch ( diag )

    return true;
}


bool CNcbiApplication::SetupDiag_AppSpecific(void)
{
    return SetupDiag(eDS_ToStderr);
}


// for the exclusive use by CNcbiApplication::LoadConfig()
static bool s_LoadConfig(CNcbiRegistry& reg, const string& conf)
{
    CNcbiIfstream is(conf.c_str());
    if ( !is.good() ) {
        _TRACE("CNcbiApplication::LoadConfig() -- cannot open registry file: "
               << conf);
        return false;
    }

    _TRACE("CNcbiApplication::LoadConfig() -- reading registry file: " <<conf);
    reg.Read(is);
    return true;
}


// for the exclusive use by CNcbiApplication::LoadConfig()
static bool s_LoadConfigTryPath(CNcbiRegistry& reg, const string& path,
                                const string& conf, const string& basename)
{
    if ( !conf.empty() ) {
        return s_LoadConfig(reg, path + conf);
    }

    // try to derive conf.file name from the application name (for empty"conf")
    string fileName = basename;

    for (;;) {
        // try the next variant -- with added ".ini" file extension
        fileName += ".ini";

        // current directory
        if ( s_LoadConfig(reg, path + fileName) )
            return true;  // success!

        // strip ".ini" file extension (the one added above)
        _ASSERT( fileName.length() > 4 );
        fileName.resize(fileName.length() - 4);

        // strip next file extension, if any left
        SIZE_TYPE dot_pos = fileName.find_last_of(".");
        if ( dot_pos == NPOS ) {
            break;
        }
        fileName.resize(dot_pos);
    }
    return false;
}


bool CNcbiApplication::LoadConfig(CNcbiRegistry& reg, const string* conf)
{
    // don't load at all
    if ( !conf ) {
        return false;
    }

    string x_conf;
    string cnf = CDirEntry::ConvertToOSPath(*conf);

    // load from the specified file name only (with path)
    if (cnf.find_first_of("/\\:") != NPOS) {
        // detect if it is a absolute path
        if ( CDirEntry::IsAbsolutePath(cnf) ) {
            // absolute path
            x_conf = cnf;
        } else {
            // path is relative to the program location
            x_conf = CDirEntry::ConcatPathEx(m_Arguments->GetProgramDirname(),
                                             cnf);
        }
        // do load
        x_conf = NStr::TruncateSpaces(x_conf);
        if ( !s_LoadConfig(reg, x_conf) ) {
            NCBI_THROW(CAppException,eNoRegistry,
                "Registry file cannot be opened");
        }
        return true;
    }

    string basename = m_Arguments->GetProgramBasename();

    // current (working) directory
    if ( s_LoadConfigTryPath(reg, kEmptyStr, cnf, basename) ) {
        return true;
    }
    // path from environment variable "NCBI"
    char* ptr = getenv("NCBI");
    if (ptr  &&
        s_LoadConfigTryPath(reg, CDirEntry::AddTrailingPathSeparator(ptr),
                            cnf, basename) ) {
            return true;
    }
    // home directory
    if ( s_LoadConfigTryPath(reg, CDir::GetHome(), cnf, basename) ) {
        return true;
    }
    // program directory
    string dirname = m_Arguments->GetProgramDirname();
    if (!dirname.empty()  &&
        s_LoadConfigTryPath(reg, dirname, cnf, basename)) {
        return true;
    }

    ERR_POST(Warning <<
             "Registry file of application \""
             << m_Arguments->GetProgramBasename()
             << "\" is not found");

    return false;
}


void CNcbiApplication::DisableArgDescriptions(void)
{
    m_DisableArgDesc = true;
}

void CNcbiApplication::HideStdArgs(THideStdArgs hide_mask)
{
    m_HideArgs = hide_mask;
}


END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.52  2003/02/17 06:30:05  vakatov
 * Get rid of an unused variable
 *
 * Revision 1.51  2002/12/26 17:13:03  ivanov
 * Added version info and Set/GetVersion functions into CNcbiApplication class
 *
 * Revision 1.50  2002/12/18 22:54:48  dicuccio
 * Shuffled some headers to avoid a potentially serious compiler warning
 * (deletion of incomplete type in Windows).
 *
 * Revision 1.49  2002/08/08 18:36:50  gouriano
 * added HideStdArgs function
 *
 * Revision 1.48  2002/08/08 13:39:06  gouriano
 * logfile & conffile-related correction
 *
 * Revision 1.47  2002/08/02 20:13:06  gouriano
 * added possibility to disable arg descriptions
 *
 * Revision 1.46  2002/08/01 19:02:17  ivanov
 * Added autoload of the verbose message file specified in the
 * applications configuration file.
 *
 * Revision 1.45  2002/07/31 18:33:44  gouriano
 * added default argument description,
 * added info about logfile and conffile optional arguments
 *
 * Revision 1.44  2002/07/22 19:33:28  ivanov
 * Fixed bug with internal processing parameters -conffile and -logfile
 *
 * Revision 1.43  2002/07/15 18:17:23  gouriano
 * renamed CNcbiException and its descendents
 *
 * Revision 1.42  2002/07/11 14:18:25  gouriano
 * exceptions replaced by CNcbiException-type ones
 *
 * Revision 1.41  2002/07/10 16:20:13  ivanov
 * Changes related with renaming SetDiagFixedStrPostLevel()->SetDiagFixedPostLevel()
 *
 * Revision 1.40  2002/07/09 16:36:52  ivanov
 * s_SetFixedDiagPostLevel() moved to ncbidiag.cpp
 * and renamed to SetDiagFixedStrPostLevel()
 *
 * Revision 1.39  2002/07/02 18:31:38  ivanov
 * Added assignment the value of diagnostic post level from environment variable
 * and config file if set. Disable to change it from application in this case.
 *
 * Revision 1.38  2002/06/19 16:57:56  ivanov
 * Added new default command line parameters "-logfile <file>" and
 * "-conffile <file>" into CNcbiApplication::AppMain()
 *
 * Revision 1.37  2002/04/11 21:08:00  ivanov
 * CVS log moved to end of the file
 *
 * Revision 1.36  2002/01/22 19:28:37  ivanov
 * Changed ConcatPath() -> ConcatPathEx() in LoadConfig()
 *
 * Revision 1.35  2002/01/20 05:53:59  vakatov
 * Get rid of a GCC warning;  formally rearrange some code.
 *
 * Revision 1.34  2002/01/10 16:52:20  ivanov
 * Changed LoadConfig() -- new method to search the config file
 *
 * Revision 1.33  2001/08/20 14:58:10  vakatov
 * Get rid of some compilation warnings
 *
 * Revision 1.32  2001/08/10 18:26:07  vakatov
 * Allow user to throw "CArgException" (and thus force USAGE printout
 * with user-provided explanation) not from any of Init(), Run() or Exit().
 *
 * Revision 1.31  2001/07/07 01:19:28  juran
 * Use "ncbi" for app name on Mac OS (if argv[0] is null).
 *
 * Revision 1.30  2001/03/26 20:07:40  vakatov
 * [NCBI_OS_MAC]  Use argv[0] (if available) as basename for ".args"
 *
 * Revision 1.29  2001/02/02 16:19:27  vasilche
 * Fixed reading program arguments on Mac
 *
 * Revision 1.28  2001/02/01 19:53:26  vasilche
 * Reading program arguments from file moved to CNcbiApplication::AppMain.
 *
 * Revision 1.27  2000/12/23 05:50:53  vakatov
 * AppMain() -- check m_ArgDesc for NULL
 *
 * Revision 1.26  2000/11/29 17:00:25  vakatov
 * Use LOG_POST instead of ERR_POST to print cmd.-line arg usage
 *
 * Revision 1.25  2000/11/24 23:33:12  vakatov
 * CNcbiApplication::  added SetupArgDescriptions() and GetArgs() to
 * setup cmd.-line argument description, and then to retrieve their
 * values, respectively. Also implements internal error handling and
 * printout of USAGE for the described arguments.
 *
 * Revision 1.24  2000/11/01 20:37:15  vasilche
 * Fixed detection of heap objects.
 * Removed ECanDelete enum and related constructors.
 * Disabled sync_with_stdio ad the beginning of AppMain.
 *
 * Revision 1.23  2000/06/09 18:41:04  vakatov
 * FlushDiag() -- check for empty diag.buffer
 *
 * Revision 1.22  2000/04/04 22:33:35  vakatov
 * Auto-set the tracing and the "abort-on-throw" debugging features
 * basing on the application environment and/or registry
 *
 * Revision 1.21  2000/01/20 17:51:18  vakatov
 * Major redesign and expansion of the "CNcbiApplication" class to 
 *  - embed application arguments   "CNcbiArguments"
 *  - embed application environment "CNcbiEnvironment"
 *  - allow auto-setup or "by choice" (see enum EAppDiagStream) of diagnostics
 *  - allow memory-resided "per application" temp. diagnostic buffer
 *  - allow one to specify exact name of the config.-file to load, or to
 *    ignore the config.file (via constructor's "conf" arg)
 *  - added detailed comments
 *
 * Revision 1.20  1999/12/29 21:20:18  vakatov
 * More intelligent lookup for the default config.file. -- try to strip off
 * file extensions if cannot find an exact match;  more comments and tracing
 *
 * Revision 1.19  1999/11/15 15:54:59  sandomir
 * Registry support moved from CCgiApplication to CNcbiApplication
 *
 * Revision 1.18  1999/06/11 20:30:37  vasilche
 * We should catch exception by reference, because catching by value
 * doesn't preserve comment string.
 *
 * Revision 1.17  1999/05/04 00:03:12  vakatov
 * Removed the redundant severity arg from macro ERR_POST()
 *
 * Revision 1.16  1999/04/30 19:21:03  vakatov
 * Added more details and more control on the diagnostics
 * See #ERR_POST, EDiagPostFlag, and ***DiagPostFlag()
 *
 * Revision 1.15  1999/04/27 14:50:06  vasilche
 * Added FastCGI interface.
 * CNcbiContext renamed to CCgiContext.
 *
 * Revision 1.14  1999/02/22 21:12:38  sandomir
 * MsgRequest -> NcbiContext
 *
 * Revision 1.13  1998/12/28 23:29:06  vakatov
 * New CVS and development tree structure for the NCBI C++ projects
 *
 * Revision 1.12  1998/12/28 15:43:12  sandomir
 * minor fixed in CgiApp and Resource
 *
 * Revision 1.11  1998/12/14 15:30:07  sandomir
 * minor fixes in CNcbiApplication; command handling fixed
 *
 * Revision 1.10  1998/12/09 22:59:35  lewisg
 * use new cgiapp class
 *
 * Revision 1.7  1998/12/09 16:49:56  sandomir
 * CCgiApplication added
 *
 * Revision 1.4  1998/12/03 21:24:23  sandomir
 * NcbiApplication and CgiApplication updated
 *
 * Revision 1.3  1998/12/01 19:12:08  lewisg
 * added CCgiApplication
 *
 * Revision 1.2  1998/11/05 21:45:14  sandomir
 * std:: deleted
 *
 * Revision 1.1  1998/11/02 22:10:13  sandomir
 * CNcbiApplication added; netest sample updated
 *
 * ===========================================================================
 */
