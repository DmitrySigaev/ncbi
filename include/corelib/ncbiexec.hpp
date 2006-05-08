#ifndef CORELIB__NCBIEXEC__HPP
#define CORELIB__NCBIEXEC__HPP

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
 * Author:  Vladimir Ivanov
 *
 *
 */

/// @file ncbiexec.hpp 
/// Defines a portable execute class.


#include <corelib/ncbistd.hpp>
#include <corelib/ncbi_limits.hpp>
#include <corelib/ncbi_process.hpp>


/** @addtogroup Exec
 *
 * @{
 */


BEGIN_NCBI_SCOPE


/// Exit code type
typedef int TExitCode;


/////////////////////////////////////////////////////////////////////////////
///
/// CExec --
///
/// Define portable exec class.
///
/// Defines the different ways a process can be spawned.

class NCBI_XNCBI_EXPORT CExec
{
public:

    /// Which exec mode the spawned process is called with.
    enum EMode {
        eOverlay = 0, ///< Overlays calling process with new process,
                      ///< destroying calling process. 
        eWait    = 1, ///< Suspends calling thread until execution of new 
                      ///< process is complete (synchronous operation).
        eNoWait  = 2, ///< Continues to execute calling process concurrently 
                      ///< with new process (asynchronous process).
        eDetach  = 3  ///< Continues to execute calling process; new process 
                      ///< is run in background with no access to console or 
                      ///< keyboard; calls to Wait() against new process will
                      ///< fail; this is an asynchronous spawn.
    };

    /// The result type for Spawn methods.
    /// 
    /// In the eNoWait and eDetach modes Spawn functions returns a process
    /// handle. On MS Windows it is a real process handle of type HANDLE.
    /// On UNIX it is a process identifier (pid).
    /// In the eWait spawn functions returns an exit code of a process.
    /// Throws an exceptions if you try to get exit code instead of 
    /// stored process handle, and otherwise.
    class CResult
    {
    public:
        /// Get exit code
        TExitCode      GetExitCode     (void);
        /// Get process handle/pid
        TProcessHandle GetProcessHandle(void); 
        NCBI_DEPRECATED operator intptr_t(void) const
            { return m_IsHandle ? (intptr_t) m_Result.handle :
                                  (intptr_t) m_Result.exitcode; }

    private:
        union {
            TExitCode      exitcode;
            TProcessHandle handle;
        } m_Result;        ///< Result of Spawn*() methods
        bool m_IsHandle;   ///< m_Result stores handle if TRUE

        friend class CExec;
    };

    /// Execute the specified command.
    ///
    /// Execute the command and return the executed command's exit code.
    /// Throw an exception if command failed to execute. If cmdline is a null
    /// pointer, System() checks if the shell (command interpreter) exists and
    /// is executable. If the shell is available, System() returns a non-zero
    /// value; otherwise, it returns 0.
    static TExitCode System(const char* cmdline);

    /// Spawn a new process with specified command-line arguments.
    ///
    /// In the SpawnL() version, the command-line arguments are passed
    /// individually. SpawnL() is typically used when number of parameters to
    /// the new process is known in advance.
    ///
    /// Meaning of the suffix "L" in method name:
    /// - The letter "L" as suffix refers to the fact that command-line
    ///   arguments are passed separately as arguments.
   ///
    /// @param mode
    ///   Mode for running the process.
    /// @param cmdname
    ///   Path to the process to spawn.
    /// @param argv
    ///   First argument vector parameter.
    /// @param ...
    ///   Argument vector. Must ends with NULL.
    /// @return 
    ///   On success, return:
    ///     - exit code      - in eWait mode.
    ///     - process handle - in eNoWait and eDetach modes.
    ///     - nothing        - in eOverlay mode.   
    ///   Throw an exception if command failed to execute.
    /// @sa
    ///   SpawnLE(), SpawnLP(), SpawnLPE(), SpawnV(), SpawnVE(), SpawnVP(), 
    ///   SpawnVPE().
    static CResult
    SpawnL(EMode mode, const char *cmdname, const char *argv, .../*, NULL */);

    /// Spawn a new process with specified command-line arguments and
    /// environment settings.
    ///
    /// In the SpawnLE() version, the command-line arguments and environment
    /// pointer are passed individually. SpawnLE() is typically used when
    /// number of parameters to the new process and individual environment 
    /// parameter settings are known in advance.
    ///
    /// Meaning of the suffix "LE" in method name:
    /// - The letter "L" as suffix refers to the fact that command-line
    ///   arguments are passed separately as arguments.
    /// - The letter "E" as suffix refers to the fact that environment pointer,
    ///   envp, is passed as an array of pointers to environment settings to 
    ///   the new process. The NULL environment pointer indicates that the new 
    ///   process will inherit the parents process's environment.
    ///
    /// @param mode
    ///   Mode for running the process.
    /// @param cmdname
    ///   Path of file to be executed.
    /// @param argv
    ///   First argument vector parameter.
    /// @param ...
    ///   Argument vector. Must ends with NULL.
    /// @param envp
    ///   Pointer to vector with environment variables which will be used
    ///   instead of current environment. Last value in vector must be NULL.
    /// @return 
    ///   On success, return:
    ///     - exit code      - in eWait mode.
    ///     - process handle - in eNoWait and eDetach modes.
    ///     - nothing        - in eOverlay mode.   
    ///   Throw an exception if command failed to execute.
    /// @sa
    ///   SpawnL(), SpawnLP(), SpawnLPE(), SpawnV(), SpawnVE(), SpawnVP(), 
    ///   SpawnVPE().
    static CResult
    SpawnLE (EMode mode, const char *cmdname, 
             const char *argv, ... /*, NULL, const char *envp[] */);

    /// Spawn a new process with variable number of command-line arguments and
    /// find file to execute from the PATH environment variable.
    ///
    /// In the SpawnLP() version, the command-line arguments are passed
    /// individually and the PATH environment variable is used to find the
    /// file to execute. SpawnLP() is typically used when number
    /// of parameters to the new process is known in advance but the exact
    /// path to the executable is not known.
    ///
    /// Meaning of the suffix "LP" in method name:
    /// - The letter "L" as suffix refers to the fact that command-line
    ///   arguments are passed separately as arguments.
    /// - The letter "P" as suffix refers to the fact that the PATH
    ///   environment variable is used to find file to execute - on a Unix
    ///   platform this feature works in functions without letter "P" in
    ///   function name. 
    ///
    /// @param mode
    ///   Mode for running the process.
    /// @param cmdname
    ///   Path of file to be executed.
    /// @param argv
    ///   First argument vector parameter.
    /// @param ...
    ///   Argument vector. Must ends with NULL.
    /// @return 
    ///   On success, return:
    ///     - exit code      - in eWait mode.
    ///     - process handle - in eNoWait and eDetach modes.
    ///     - nothing        - in eOverlay mode.   
    ///   Throw an exception if command failed to execute.
    /// @sa
    ///   SpawnL(), SpawnLE(), SpawnLPE(), SpawnV(), SpawnVE(), SpawnVP(), 
    ///   SpawnVPE().
    static CResult 
    SpawnLP(EMode mode, const char *cmdname, const char *argv, .../*, NULL*/);

    /// Spawn a new process with specified command-line arguments, 
    /// environment settings and find file to execute from the PATH
    /// environment variable.
    ///
    /// In the SpawnLPE() version, the command-line arguments and environment
    /// pointer are passed individually, and the PATH environment variable
    /// is used to find the file to execute. SpawnLPE() is typically used when
    /// number of parameters to the new process and individual environment
    /// parameter settings are known in advance, but the exact path to the
    /// executable is not known.
    ///
    /// Meaning of the suffix "LPE" in method name:
    /// - The letter "L" as suffix refers to the fact that command-line
    ///   arguments are passed separately as arguments.
    /// - The letter "P" as suffix refers to the fact that the PATH
    ///   environment variable is used to find file to execute - on a Unix
    ///   platform this feature works in functions without letter "P" in
    ///   function name. 
    /// - The letter "E" as suffix refers to the fact that environment pointer,
    ///   envp, is passed as an array of pointers to environment settings to 
    ///   the new process. The NULL environment pointer indicates that the new 
    ///   process will inherit the parents process's environment.
    ///
    /// @param mode
    ///   Mode for running the process.
    /// @param cmdname
    ///   Path of file to be executed.
    /// @param argv
    ///   First argument vector parameter.
    /// @param ...
    ///   Argument vector. Must ends with NULL.
    /// @param envp
    ///   Pointer to vector with environment variables which will be used
    ///   instead of current environment. Last value in an array must be NULL.
    /// @return 
    ///   On success, return:
    ///     - exit code      - in eWait mode.
    ///     - process handle - in eNoWait and eDetach modes.
    ///     - nothing        - in eOverlay mode.   
    ///    Throw an exception if command failed to execute.
    /// @sa
    ///   SpawnL(), SpawnLE(), SpawnLP(), SpawnV(), SpawnVE(), SpawnVP(), 
    ///   SpawnVPE().
    static CResult
    SpawnLPE(EMode mode, const char *cmdname,
             const char *argv, ... /*, NULL, const char *envp[] */);

    /// Spawn a new process with variable number of command-line arguments. 
    ///
    /// In the SpawnV() version, the command-line arguments are a variable
    /// number. The array of pointers to arguments must have a length of 1 or
    /// more and you must assign parameters for the new process beginning
    /// from 1.
    ///
    /// Meaning of the suffix "V" in method name:
    /// - The letter "V" as suffix refers to the fact that the number of
    /// command-line arguments are variable.
    ///
    /// @param mode
    ///   Mode for running the process.
    /// @param cmdline
    ///   Path of file to be executed.
    /// @param argv
    ///   Pointer to argument vector. Last value in vector must be NULL.
    /// @return 
    ///   On success, return:
    ///     - exit code      - in eWait mode.
    ///     - process handle - in eNoWait and eDetach modes.
    ///     - nothing        - in eOverlay mode.   
    ///   Throw an exception if command failed to execute.
    /// @sa
    ///   SpawnL(), SpawnLE(), SpawnLP(), SpawnLPE(), SpawnVE(), SpawnVP(), 
    ///   SpawnVPE().
    static CResult
    SpawnV(EMode mode, const char *cmdname, const char *const *argv);

    /// Spawn a new process with variable number of command-line arguments
    /// and specified environment settings.
    ///
    /// In the SpawnVE() version, the command-line arguments are a variable
    /// number. The array of pointers to arguments must have a length of 1 or
    /// more and you must assign parameters for the new process beginning from
    /// 1.  The individual environment parameter settings are known in advance
    /// and passed explicitly.
    ///
    /// Meaning of the suffix "VE" in method name:
    /// - The letter "V" as suffix refers to the fact that the number of
    ///   command-line arguments are variable.
    /// - The letter "E" as suffix refers to the fact that environment pointer,
    ///   envp, is passed as an array of pointers to environment settings to 
    ///   the new process. The NULL environment pointer indicates that the new 
    ///   process will inherit the parents process's environment.
    ///
    /// @param mode
    ///   Mode for running the process.
    /// @param cmdname
    ///   Path of file to be executed.
    /// @param argv
    ///   Argument vector. Last value must be NULL.
    /// @param envp
    ///   Pointer to vector with environment variables which will be used
    ///   instead of current environment. Last value in an array must be NULL.
    /// @return 
    ///   On success, return:
    ///     - exit code      - in eWait mode.
    ///     - process handle - in eNoWait and eDetach modes.
    ///     - nothing        - in eOverlay mode.   
    ///   Throw an exception if command failed to execute.
    /// @sa
    ///   SpawnL(), SpawnLE(), SpawnLP(), SpawnLPE(), SpawnV(), SpawnVP(), 
    ///   SpawnVPE().
    static CResult
    SpawnVE(EMode mode, const char *cmdname,
            const char *const *argv, const char *const *envp);

    /// Spawn a new process with variable number of command-line arguments and
    /// find file to execute from the PATH environment variable.
    ///
    /// In the SpawnVP() version, the command-line arguments are a variable
    /// number. The array of pointers to arguments must have a length of 1 or
    /// more and you must assign parameters for the new process beginning from
    /// 1. The PATH environment variable is used to find the file to execute.
    ///
    /// Meaning of the suffix "VP" in method name:
    /// - The letter "V" as suffix refers to the fact that the number of
    ///   command-line arguments are variable.
    /// - The letter "P" as suffix refers to the fact that the PATH
    ///   environment variable is used to find file to execute - on a Unix
    ///   platform this feature works in functions without letter "P" in
    ///   function name. 
    ///
    /// @param mode
    ///   Mode for running the process.
    /// @param cmdname
    ///   Path of file to be executed.
    /// @param argv
    ///   Pointer to argument vector. Last value in vector must be NULL.
    /// @return 
    ///   On success, return:
    ///     - exit code      - in eWait mode.
    ///     - process handle - in eNoWait and eDetach modes.
    ///     - nothing        - in eOverlay mode.   
    ///   Throw an exception if command failed to execute.
    /// @sa
    ///   SpawnL(), SpawnLE(), SpawnLP(), SpawnLPE(), SpawnV(), SpawnVE(), 
    ///   SpawnVPE().
    static CResult
    SpawnVP(EMode mode, const char *cmdname, const char *const *argv);

    /// Spawn a new process with variable number of command-line arguments
    /// and specified environment settings, and find the file to execute
    /// from the PATH environment variable.
    ///
    /// In the SpawnVPE() version, the command-line arguments are a variable
    /// number. The array of pointers to arguments must have a length of 1 or
    /// more and you must assign parameters for the new process beginning from
    /// 1. The PATH environment variable is used to find the file to execute,
    /// and the environment is passed via an environment vector pointer.
    ///
    /// Meaning of the suffix "VPE" in method name:
    /// - The letter "V" as suffix refers to the fact that the number of
    ///   command-line arguments are variable.
    /// - The letter "P" as suffix refers to the fact that the PATH
    ///   environment variable is used to find file to execute - on a Unix
    ///   platform this feature works in functions without letter "P" in
    ///   function name. 
    /// - The letter "E" as suffix refers to the fact that environment pointer,
    ///   envp, is passed as an array of pointers to environment settings to 
    ///   the new process. The NULL environment pointer indicates that the new 
    ///   process will inherit the parents process's environment.
    ///
    /// @param mode
    ///   Mode for running the process.
    /// @param cmdname
    ///   Path of file to be executed.
    /// @param argv
    ///   Argument vector. Last value must be NULL.
    /// @param envp
    ///   Pointer to vector with environment variables which will be used
    ///   instead of current environment. Last value in an array must be NULL.
    /// @return 
    ///   On success, return:
    ///     - exit code      - in eWait mode.
    ///     - process handle - in eNoWait and eDetach modes.
    ///     - nothing        - in eOverlay mode.   
    ///   Throw an exception if command failed to execute.
    /// @sa
    ///   SpawnL(), SpawnLE(), SpawnLP(), SpawnLPE(), SpawnV(), SpawnVE(),
    ///   SpawnVP(), 
    static CResult
    SpawnVPE(EMode mode, const char *cmdname,
             const char *const *argv, const char *const *envp);

    /// Run console application .
    ///
    /// Wait until the child process with "handle" terminates, and return
    /// immeditately if the specifed child process has already terminated.
    /// @param handle
    ///   Wait on child process with identifier "handle", returned by one 
    ///   of the Spawn* function in eNoWait and eDetach modes.
    /// @param timeout
    ///   Time-out interval. By default it is infinite.
    /// @return
    ///   - Exit code of child process, if no errors.
    ///   - (-1), if error has occurred.
    static TExitCode Wait(TProcessHandle handle,
                          unsigned long timeout = kMax_ULong);

    /// Spawn a new process with specified command-line
    ///
    /// MSWin:
    ///    This function try to run a program in invisible mode, without
    ///    visible window. This can be used to run console programm from 
    ///    non-console application. If it runs from console application,
    ///    the parent's console window can be used by child process.
    ///    Executing non-console program can show theirs windows or not,
    ///    this depends. In eDetach mode the main window/console of
    ///    the running program can be visible, use eNoWait instead.
    ///    Note, that if the running program cannot self-terminate, that
    ///    it can be never terminated.
    /// Unix:
    ///    In current implementation equal to SpawnL().
    /// @param mode
    ///   Mode for running the process.
    /// @param cmdname
    ///   Path to the process to spawn.
    /// @param argv
    ///   First argument vector parameter.
    /// @param ...
    ///   Argument vector. Must ends with NULL.
    /// @return 
    ///   On success, return:
    ///     - exit code      - in eWait mode.
    ///     - process handle - in eNoWait and eDetach modes.
    ///     - nothing        - in eOverlay mode.   
    ///   Throw an exception if command failed to execute.
    /// @sa
    ///   SpawnL()
    static CResult
    RunSilent(EMode mode, const char *cmdname,
              const char *argv, ... /*, NULL */);
};


/////////////////////////////////////////////////////////////////////////////
///
/// CExecException --
///
/// Define exceptions generated by CExec.
///
/// CExecException inherits its basic functionality from
/// CErrnoTemplException<CCoreException> and defines additional error codes
/// for errors generated by CExec.

class CExecException : public CErrnoTemplException<CCoreException>
{
public:
    /// Error types that CExec can generate.
    enum EErrCode {
        eSystem,      ///< System error
        eSpawn,       ///< Spawn error
        eResult       ///< Result interpretation error
    };

    /// Translate from the error code value to its string representation.
    virtual const char* GetErrCodeString(void) const
    {
        switch (GetErrCode()) {
        case eSystem: return "eSystem";
        case eSpawn:  return "eSpawn";
        case eResult: return "eResult";
        default:      return CException::GetErrCodeString();
        }
    }

    // Standard exception boilerplate code.
    NCBI_EXCEPTION_DEFAULT(CExecException,
                           CErrnoTemplException<CCoreException>);
};


END_NCBI_SCOPE


/* @} */


/*
 * ===========================================================================
 * $Log$
 * Revision 1.17  2006/05/08 13:57:10  ivanov
 * Changed return type of all Spawn* function from int to CResult
 *
 * Revision 1.16  2006/03/20 15:46:51  ivanov
 * + CExec::RunSilent()
 *
 * Revision 1.15  2004/08/19 13:01:51  dicuccio
 * Dropped unnecessary export specifier on exceptions
 *
 * Revision 1.14  2004/08/18 15:57:56  ivanov
 * Minor comment changes
 *
 * Revision 1.13  2004/04/01 14:14:01  lavr
 * Spell "occurred", "occurrence", and "occurring"
 *
 * Revision 1.12  2003/09/25 17:59:12  ivanov
 * Comment changes
 *
 * Revision 1.11  2003/09/25 17:19:05  ucko
 * CExec::Wait: add an optional timeout argument per the new (forwarding)
 * implementation.
 *
 * Revision 1.10  2003/09/16 17:48:03  ucko
 * Remove redundant "const"s from arguments passed by value.
 *
 * Revision 1.9  2003/09/16 15:22:31  ivanov
 * Minor comments changes
 *
 * Revision 1.8  2003/07/30 11:08:44  siyan
 * Documentation changes.
 *
 * Revision 1.7  2003/03/31 16:40:07  siyan
 * Added doxygen support
 *
 * Revision 1.6  2003/02/24 19:54:52  gouriano
 * use template-based exceptions instead of errno and parse exceptions
 *
 * Revision 1.5  2002/12/18 22:53:21  dicuccio
 * Added export specifier for building DLLs in windows.  Added global list of
 * all such specifiers in mswin_exports.hpp, included through ncbistl.hpp
 *
 * Revision 1.4  2002/07/11 14:17:54  gouriano
 * exceptions replaced by CNcbiException-type ones
 *
 * Revision 1.3  2002/06/10 18:55:27  ivanov
 * Added comment note about arguments with spaces inside
 *
 * Revision 1.2  2002/05/31 20:48:39  ivanov
 * Clean up code
 *
 * Revision 1.1  2002/05/30 16:30:45  ivanov
 * Initial revision
 *
 * ===========================================================================
 */

#endif  /* CORELIB__NCBIEXEC__HPP */
