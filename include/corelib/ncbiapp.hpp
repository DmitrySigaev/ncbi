#ifndef CORELIB___NCBIAPP__HPP
#define CORELIB___NCBIAPP__HPP

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
 * Authors:  Denis Vakatov, Vsevolod Sandomirskiy
 *
 *
 */

/// @file ncbiapp.hpp
/// Defines the CNcbiApplication and CAppException classes for creating
/// NCBI applications.
///
/// The CNcbiApplication class defines the application framework and the high
/// high level behavior of an application, and the CAppException class is used
/// for the exceptions generated by CNcbiApplication.


#include <corelib/ncbistd.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/version.hpp>
#include <memory>


/// Avoid preprocessor name clash with the NCBI C Toolkit.
#if !defined(NCBI_OS_UNIX)  ||  defined(HAVE_NCBI_C)
#  if defined(GetArgs)
#    undef GetArgs
#  endif
#  define GetArgs GetArgs
#endif


/** @addtogroup AppFramework
 *
 * @{
 */


BEGIN_NCBI_SCOPE


/////////////////////////////////////////////////////////////////////////////
///
/// CAppException --
///
/// Define exceptions generated by CNcbiApplication.
///
/// CAppException inherits its basic functionality from CCoreException
/// and defines additional error codes for applications.

class NCBI_XNCBI_EXPORT CAppException : public CCoreException
{
public:
    /// Error types that an application can generate.
    ///
    /// These error conditions are checked for and caught inside AppMain().
    enum EErrCode {
        eUnsetArgs,     ///< Command-line argument description not found
        eSetupDiag,     ///< Application diagnostic stream setup failed
        eLoadConfig,    ///< Registry data failed to load from config file
        eSecond,        ///< Second instance of CNcbiApplication is prohibited
        eNoRegistry     ///< Registry file cannot be opened
    };

    /// Translate from the error code value to its string representation.
    virtual const char* GetErrCodeString(void) const
    {
        switch (GetErrCode()) {
        case eUnsetArgs:  return "eUnsetArgs";
        case eSetupDiag:  return "eSetupDiag";
        case eLoadConfig: return "eLoadConfig";
        case eSecond:     return "eSecond";
        case eNoRegistry: return "eNoRegistry";
        default:    return CException::GetErrCodeString();
        }
    }

    // Standard exception boilerplate code.
    NCBI_EXCEPTION_DEFAULT(CAppException, CCoreException);
};



///////////////////////////////////////////////////////
// CNcbiApplication
//


// Some forward declarations
class CNcbiRegistry;


/// Where to write the application's diagnostics to.
enum EAppDiagStream {
    eDS_ToStdout,    ///< To standard output stream
    eDS_ToStderr,    ///< To standard error stream
    eDS_ToStdlog,    ///< Add to standard log file (app.name + ".log")
    eDS_ToMemory,    ///< Keep in a temp.memory buffer, see FlushDiag()
    eDS_Disable,     ///< Dont write it anywhere
    eDS_User,        ///< Leave as was previously set (or not set) by user
    eDS_AppSpecific, ///< Depends on the application type
    eDS_Default      ///< "eDS_User" if set, else "eDS_AppSpecific"
};



/////////////////////////////////////////////////////////////////////////////
///
/// CNcbiApplication --
///
/// Basic (abstract) NCBI application class.
///
/// Defines the high level behavior of an NCBI application.
/// A new application is written by deriving a class from the CNcbiApplication
/// and writing an implementation of the Run() and maybe some other (like
/// Init(), Exit(), etc.) methods.

class NCBI_XNCBI_EXPORT CNcbiApplication
{
public:
    /// Singleton method.
    ///
    /// Track the instance of CNcbiApplication, and throw an exception
    /// if an attempt is made to create another instance of the application.
    /// @return
    ///   Current application instance.
    static CNcbiApplication* Instance(void);

    /// Constructor.
    ///
    /// Register the application instance, and reset important
    /// application-specific settings to empty values that will
    /// be set later.
    CNcbiApplication(void);

    /// Destructor.
    ///
    /// Clean up the application settings, flush the diagnostic stream.
    virtual ~CNcbiApplication(void);

    /// Main function (entry point) for the NCBI application.
    ///
    /// You can specify where to write the diagnostics to (EAppDiagStream),
    /// and where to get the configuration file (LoadConfig()) to load
    /// to the application registry (accessible via GetConfig()).
    ///
    /// Throw exception if:
    ///  - not-only instance
    ///  - cannot load explicitly specified config.file
    ///  - SetupDiag() throws an exception
    ///
    /// If application name is not specified a default of "ncbi" is used.
    /// Certain flags such as -logfile, -conffile and -version are special so
    /// AppMain() processes them separately.
    /// @return
    ///   Exit code from Run(). Can also return non-zero value if application
    ///   threw an exception.
    /// @sa
    ///   Init(), Run(), Exit()
    int AppMain
    (int                argc,     ///< argc in a regular main(argc, argv, envp)
     const char* const* argv,     ///< argv in a regular main(argc, argv, envp)
     const char* const* envp = 0, ///< argv in a regular main(argc, argv, envp)
     EAppDiagStream     diag = eDS_Default,     ///< Specify diagnostic stream
     const char*        conf = NcbiEmptyCStr,   ///< Specify registry to load
     const string&      name = NcbiEmptyString  ///< Specify application name
     );

    /// Initialize the application.
    ///
    /// The default behavior of this is "do nothing". If you have special
    /// initialization logic that needs to be peformed, then you must override
    /// this method with your own logic.
    virtual void Init(void);

    /// Run the application.
    ///
    /// It is defined as a pure virtual method -- so you must(!) supply the
    /// Run() method to implement the application-specific logic.
    /// @return
    ///   Exit code.
    virtual int Run(void) = 0;

    /// Cleanup on application exit.
    ///
    /// Perform cleanup before exiting. The default behavior of this is
    /// "do nothing". If you have special cleanup logic that needs to be
    /// performed, then you must override this method with your own logic.
    virtual void Exit(void);

    /// Get the application's cached unprocessed command-line arguments.
    const CNcbiArguments& GetArguments(void) const;

    /// Get parsed command line arguments.
    ///
    /// Get command line arguments parsed according to the arg descriptions
    /// set by SetArgDescriptions(). Throw exception if no descriptions
    /// have been set.
    /// @return
    ///   The CArgs object containing parsed cmd.-line arguments.
    /// @sa
    ///   SetArgDescriptions().
    const CArgs& GetArgs(void) const;

    /// Get the application's cached environment.
    const CNcbiEnvironment& GetEnvironment(void) const;

    /// Get the application's cached configuration parameters.
    const CNcbiRegistry& GetConfig(void) const;
    CNcbiRegistry& GetConfig(void);

    /// Flush the in-memory diagnostic stream (for "eDS_ToMemory" case only).
    ///
    /// In case of "eDS_ToMemory", the diagnostics is stored in
    /// the internal application memory buffer ("m_DiagStream").
    /// Call this function to dump all the diagnostics to stream "os" and
    /// purge the buffer.
    /// @param  os
    ///   Output stream to dump diagnostics to. If it is NULL, then
    ///   nothing will be written to it (but the buffer will still be purged).
    /// @param  close_diag
    ///   If "close_diag" is TRUE, then also destroy "m_DiagStream".
    /// @return
    ///   Total number of bytes actually written to "os".
    SIZE_TYPE FlushDiag(CNcbiOstream* os, bool close_diag = false);

    /// Get the application's "display" name.
    ///
    /// Get name of this application, suitable for displaying
    /// or for using as the base name for other files.
    /// Will be the 'name' argument of AppMain if given.
    /// Otherwise will be taken from the actual name of the application file
    /// or argv[0].
    string GetProgramDisplayName(void) const;

protected:
    /// Disable argument descriptions.
    ///
    /// Call if you do not want cmd.line args to be parsed at all.
    /// Note that by default ArgDescriptions are enabled (i.e. required).
    void DisableArgDescriptions(void);

    /// Which standard flag's descriptions should not be displayed in
    /// the usage message.
    ///
    /// Do not display descriptions of the standard flags such as the
    ///    -h, -logfile, -conffile, -version
    /// flags in the usage message. Note that you still can pass them in
    /// the command line.
    enum EHideStdArgs {
        fHideHelp     = 0x01,  ///< Hide help description
        fHideLogfile  = 0x02,  ///< Hide log file description
        fHideConffile = 0x04,  ///< Hide configuration file description
        fHideVersion  = 0x08   ///< Hide version description
    };
    typedef int THideStdArgs;  ///< Binary OR of "EHideStdArgs"

    /// Set the hide mask for the Hide Std Flags.
    void HideStdArgs(THideStdArgs hide_mask);

    /// Flags to adjust standard I/O streams' behaviour.
    ///
    /// Set these flags if you insist on using compiler-specific defaults
    /// for standard "C++" I/O streams (Cin/Cout/Cerr).
    enum EStdioSetup {
        fDefault_SyncWithStdio  = 0x01,
        ///< Use compiler-specific default as pertains to the synchronizing
        ///< of "C++" Cin/Cout/Cerr streams with their "C" counterparts.

        fDefault_CinBufferSize  = 0x02
        ///< Use compiler-specific default of Cin buffer size.
    };
    typedef int TStdioSetupFlags;  ///< Binary OR of "EStdioSetup"

    /// Adjust the behavior of standard I/O streams.
    ///
    /// Unless this function is called, the behaviour of "C++" Cin/Cout/Cerr
    /// streams will be the same regardless of the compiler used.
    ///
    /// IMPLEMENTATION NOTE: Do not call this function more than once
    /// and from places other than App's constructor.
    void SetStdioFlags(TStdioSetupFlags stdio_flags);

    /// Set the version number for the program.
    ///
    /// If not set, a default of 0.0.0 (unknown) is used.
    void SetVersion(const CVersionInfo& version);

    /// Get the program version information.
    CVersionInfo GetVersion(void);

    /// Setup the command line argument descriptions.
    ///
    /// Call from the Init() method. The passed "arg_desc" will be owned
    /// by this class, and it will be deleted by ~CNcbiApplication(),
    /// or if SetupArgDescriptions() is called again.
    void SetupArgDescriptions(CArgDescriptions* arg_desc);

    /// Setup the application diagnostic stream.
    /// @return
    ///   TRUE if successful,  FALSE otherwise.
    bool SetupDiag(EAppDiagStream diag);

    /// Setup application specific diagnostic stream.
    ///
    /// Called from SetupDiag when it is passed the eDS_AppSpecific parameter.
    /// Currently, this calls SetupDiag(eDS_ToStderr) to setup diagonistic
    /// stream to the std error channel.
    /// @return
    ///   TRUE if successful, FALSE otherwise.
    virtual bool SetupDiag_AppSpecific(void);

    /// Load configuration settings from the configuration file to
    /// the registry.
    ///
    /// Load (add) registry settings from the configuration file specified as
    /// the "conf" arg passed to AppMain(). The "conf" argument has the
    /// following special meanings:
    ///  - NULL      -- dont even try to load registry from any file at all;
    ///  - non-empty -- if "conf" contains a path, then try to load from the
    ///                 conf.file of name "conf" (only!). Else - see NOTE.
    ///                 TIP: if the path is not fully qualified then:
    ///                      if it starts from "../" or "./" -- look starting
    ///                      from the current working dir.
    ///  - empty     -- compose conf.file name from the application name
    ///                 plus ".ini". If it does not match an existing
    ///                 file, then try to strip file extensions, e.g. for
    ///                 "my_app.cgi.exe" -- try subsequently:
    ///                   "my_app.cgi.exe.ini", "my_app.cgi.ini", "my_app.ini".
    ///
    /// NOTE:
    /// If "conf" arg is empty or non-empty, but without path, then config file
    /// will be sought for in the following order:
    ///  - in the current work directory;
    ///  - in the dir defined by environment variable "NCBI";
    ///  - in the user home directory;
    ///  - in the program dir.
    ///
    /// Throw an exception if "conf" is non-empty, and cannot open file.
    /// Throw an exception if file exists, but contains invalid entries.
    /// @param reg
    ///   The loaded registry is returned via the reg parameter.
    /// @param conf
    ///   The configuration file to loaded the registry entries from.
    /// @return
    ///   TRUE only if the file was non-NULL, found and successfully read.
    virtual bool LoadConfig(CNcbiRegistry& reg, const string* conf);

    /// Get the home directory for the current user.
    string GetHomeDir(void);

    /// Set program's display name.
    ///
    /// Set up application name suitable for display or as a basename for
    /// other files. It can also be set by the user when calling AppMain().
    void SetProgramDisplayName(const string& app_name);

    /// Find the application's executable file.
    ///
    /// Find the path and name of the executable file that this application is
    /// running from. Will be accesible by GetArguments.GetProgramName().
    /// @param argc	
    ///   Standard argument count "argc".
    /// @param argv
    ///   Standard argument vector "argv".
    /// @return
    ///   Name of application's executable file.
    string FindProgramExecutablePath(int argc, const char* const* argv);

    /// Honor debug settings.
    ///
    /// Read the [DEBUG] section of the specified registry and
    /// set the diagnostic settings as found in that section.
    /// Specifically, the method reads the settings for parameters:
    /// ABORT_ON_THROW, DIAG_POST_LEVEL, DIAG_MESSAGE_FILE.
    /// @param reg
    ///   Registry to read from. If NULL, use the current registry setting.
    void HonorDebugSettings(CNcbiRegistry* reg = 0);

private:
    /// Setup C++ standard I/O streams' behaviour.
    ///
    /// Called from AppMain() to do compiler-specific optimization
    /// for C++ I/O streams. For example, since SUN WorkShop STL stream
    /// library has significant performance loss when sync_with_stdio is
    /// TRUE (default), so we turn it off. Another, for GCC version greater
    /// than 3.00 we forcibly set cin stream buffer size to 4096 bytes -- which
    /// boosts the performance dramatically.
    void x_SetupStdio(void);

    static CNcbiApplication*   m_Instance;   ///< Current app. instance
    auto_ptr<CVersionInfo>     m_Version;    ///< Program version
    auto_ptr<CNcbiEnvironment> m_Environ;    ///< Cached application env.
    auto_ptr<CNcbiRegistry>    m_Config;     ///< Guaranteed to be non-NULL
    auto_ptr<CNcbiOstream>     m_DiagStream; ///< Opt., aux., see eDS_ToMemory
    auto_ptr<CNcbiArguments>   m_Arguments;  ///< Command-line arguments
    auto_ptr<CArgDescriptions> m_ArgDesc;    ///< Cmd.-line arg descriptions
    auto_ptr<CArgs>            m_Args;       ///< Parsed cmd.-line args
    bool                       m_DisableArgDesc;  ///< Arg desc. disabled
    THideStdArgs               m_HideArgs;   ///< Std cmd.-line flags to hide
    TStdioSetupFlags           m_StdioFlags; ///< Std C++ I/O adjustments
    char*                      m_CinBuffer;  ///< Cin buffer if changed
    string                     m_ProgramDisplayName;  ///< Display name of app
};


/* @} */



/////////////////////////////////////////////////////////////////////////////
//  IMPLEMENTATION of INLINE functions
/////////////////////////////////////////////////////////////////////////////


inline const CNcbiArguments& CNcbiApplication::GetArguments(void) const {
    return *m_Arguments;
}

inline const CArgs& CNcbiApplication::GetArgs(void) const {
    if ( !m_Args.get() ) {
        NCBI_THROW(CAppException, eUnsetArgs,
                   "Command-line argument description is not found");
    }
    return *m_Args;
}

inline const CNcbiEnvironment& CNcbiApplication::GetEnvironment(void) const {
    return *m_Environ;
}

inline const CNcbiRegistry& CNcbiApplication::GetConfig(void) const {
    return *m_Config;
}

inline CNcbiRegistry& CNcbiApplication::GetConfig(void) {
    return *m_Config;
}

inline string  CNcbiApplication::GetProgramDisplayName(void) const {
    return m_ProgramDisplayName;
}


END_NCBI_SCOPE



/*
 * ===========================================================================
 * $Log$
 * Revision 1.37  2003/07/28 10:58:54  siyan
 * Minor comment changes.
 *
 * Revision 1.36  2003/07/17 00:05:33  siyan
 * Changed doc on some enum types so Doxygen picks them up better.
 *
 * Revision 1.35  2003/07/07 13:55:13  siyan
 * Added documentation and made documentation consistent. Made it easier
 * for Doxygen to pick up necessary documentation.
 *
 * Revision 1.34  2003/06/25 15:58:59  rsmith
 * factor out config file DEBUG settings into HonorDebugSettings
 *
 * Revision 1.33  2003/06/23 18:02:21  vakatov
 * CNcbiApplication::MacArgMunging() moved from header to the source file.
 * Fixed, reformatted and added comments.
 *
 * Revision 1.32  2003/06/16 13:52:27  rsmith
 * Add ProgramDisplayName member. Program name becomes real executable full
 * path. Handle Mac special arg handling better.
 *
 * Revision 1.31  2003/06/05 18:14:34  lavr
 * SetStdioFlags(): comment from impl not to call twice or not from ctor
 *
 * Revision 1.30  2003/03/31 13:26:00  siyan
 * Added doxygen support
 *
 * Revision 1.29  2003/03/19 19:36:09  gouriano
 * added optional adjustment of stdio streams
 *
 * Revision 1.28  2002/12/26 17:12:42  ivanov
 * Added version info and Set/GetVersion functions into CNcbiApplication class
 *
 * Revision 1.27  2002/12/18 22:53:21  dicuccio
 * Added export specifier for building DLLs in windows.  Added global list of
 * all such specifiers in mswin_exports.hpp, included through ncbistl.hpp
 *
 * Revision 1.26  2002/10/28 22:36:15  vakatov
 * Fixes in some comments
 *
 * Revision 1.25  2002/08/08 18:38:16  gouriano
 * added HideStdArgs function
 *
 * Revision 1.24  2002/08/02 20:11:51  gouriano
 * added possibility to disable arg descriptions
 *
 * Revision 1.23  2002/07/15 18:17:50  gouriano
 * renamed CNcbiException and its descendents
 *
 * Revision 1.22  2002/07/11 14:17:53  gouriano
 * exceptions replaced by CNcbiException-type ones
 *
 * Revision 1.21  2002/04/11 20:39:16  ivanov
 * CVS log moved to end of the file
 *
 * Revision 1.20  2002/01/10 16:51:52  ivanov
 * Changed LoadConfig() -- new method to search the config file
 *
 * Revision 1.19  2002/01/07 16:58:21  vakatov
 * CNcbiApplication::GetArgs() -- a clearer error message
 *
 * Revision 1.18  2001/05/17 14:50:13  lavr
 * Typos corrected
 *
 * Revision 1.17  2001/04/13 02:58:43  vakatov
 * Do not apply R1.16 for non-UNIX platforms where we cannot configure
 * HAVE_NCBI_C yet
 *
 * Revision 1.16  2001/04/12 22:55:09  vakatov
 * [HAVE_NCBI_C]  Handle #GetArgs to avoid name clash with the NCBI C Toolkit
 *
 * Revision 1.15  2000/11/24 23:33:10  vakatov
 * CNcbiApplication::  added SetupArgDescriptions() and GetArgs() to
 * setup cmd.-line argument description, and then to retrieve their
 * values, respectively. Also implements internal error handling and
 * printout of USAGE for the described arguments.
 *
 * Revision 1.14  2000/01/20 17:51:16  vakatov
 * Major redesign and expansion of the "CNcbiApplication" class to
 *  - embed application arguments   "CNcbiArguments"
 *  - embed application environment "CNcbiEnvironment"
 *  - allow auto-setup or "by choice" (see enum EAppDiagStream) of diagnostics
 *  - allow memory-resided "per application" temp. diagnostic buffer
 *  - allow one to specify exact name of the config.-file to load, or to
 *    ignore the config.file (via constructor's "conf" arg)
 *  - added detailed comments
 *
 * Revision 1.13  1999/12/29 21:20:16  vakatov
 * More intelligent lookup for the default config.file. -- try to strip off
 * file extensions if cannot find an exact match;  more comments and tracing
 *
 * Revision 1.12  1999/11/15 18:57:01  vakatov
 * Added <memory> (for "auto_ptr<>" template)
 *
 * Revision 1.11  1999/11/15 15:53:27  sandomir
 * Registry support moved from CCgiApplication to CNcbiApplication
 *
 * Revision 1.10  1999/04/27 14:49:50  vasilche
 * Added FastCGI interface.
 * CNcbiContext renamed to CCgiContext.
 *
 * Revision 1.9  1998/12/28 17:56:25  vakatov
 * New CVS and development tree structure for the NCBI C++ projects
 *
 * Revision 1.8  1998/12/09 17:30:12  sandomir
 * ncbicgi.hpp deleted from ncbiapp.hpp
 *
 * Revision 1.7  1998/12/09 16:49:56  sandomir
 * CCgiApplication added
 *
 * Revision 1.6  1998/12/07 23:46:52  vakatov
 * Merged with "cgiapp.hpp";  minor fixes
 *
 * Revision 1.4  1998/12/03 21:24:21  sandomir
 * NcbiApplication and CgiApplication updated
 *
 * Revision 1.3  1998/12/01 19:12:36  lewisg
 * added CCgiApplication
 *
 * Revision 1.2  1998/11/05 21:45:13  sandomir
 * std:: deleted
 *
 * Revision 1.1  1998/11/02 22:10:12  sandomir
 * CNcbiApplication added; netest sample updated
 * ===========================================================================
 */

#endif  /* CORELIB___NCBIAPP__HPP */
