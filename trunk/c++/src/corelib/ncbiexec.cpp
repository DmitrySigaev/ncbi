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
 * Authors:  Vladimir Ivanov
 *
 */

#include <corelib/ncbiexec.hpp>
#include <stdio.h>
#include <stdarg.h>

#if defined(NCBI_OS_MSWIN)
#  include <process.h>
#elif defined(NCBI_OS_UNIX)
#  include <unistd.h>
#  include <sys/types.h>
#  include <sys/wait.h>
#  include <errno.h>
#elif defined(NCBI_OS_MAC)
#  error "Class CExec defined only for MS Windows and UNIX platforms"
#endif


BEGIN_NCBI_SCOPE


#if defined(NCBI_OS_MSWIN)
// Convert CExec class mode to the real mode
static int s_GetRealMode(const CExec::EMode mode)
{
    static const int s_Mode[] =  { 
        P_OVERLAY, P_WAIT, P_NOWAIT, P_DETACH 
    };

    int x_mode = (int) mode;
    _ASSERT(0 <= x_mode  &&  x_mode < sizeof(s_Mode)/sizeof(s_Mode[0]));
    return s_Mode[x_mode];
}
#endif


#if defined(NCBI_OS_UNIX)

// Type function to call
enum ESpawnFunc {eV, eVE, eVP, eVPE};

static int s_SpawnUnix(const ESpawnFunc func, const CExec::EMode mode, 
                       const char *cmdname, const char *const *argv, 
                       const char *const *envp = 0)
{
    // Empty environment for Spawn*E
    const char* empty_env[] = { 0 };
    if ( !envp ) {
        envp = empty_env;
    }

    // Replace the current process image with a new process image.
    if (mode == CExec::eOverlay) {
        switch (func) {
        case eV:
        case eVP:
            return execv(cmdname, const_cast<char**>(argv));
        case eVE:
        case eVPE:
            return execve(cmdname, const_cast<char**>(argv), 
                          const_cast<char**>(envp));
        }
        return -1;
    }
    // Fork child process
    pid_t pid;
    switch (pid = fork()) {
    case -1:
        // fork failed
        return -1;
    case 0:
        // Here we're the child
        if (mode == CExec::eDetach) {
            fclose(stdin);
            fclose(stdout);
            fclose(stderr);
            setsid();
        }
        int status =-1;
        switch (func) {
        case eV:
            status = execv(cmdname, const_cast<char**>(argv));
            break;
        case eVP:
            status = execvp(cmdname, const_cast<char**>(argv));
            break;
        case eVE:
        case eVPE:
            status = execve(cmdname, const_cast<char**>(argv),
                            const_cast<char**>(envp));
            break;
        }
        _exit(status);
    }
    // "pid" contains the childs pid
    if (mode == CExec::eWait) {
        return CExec::Wait(pid);
    }
    return pid;
}

#endif


// Get exec arguments
#define GET_EXEC_ARGS \
    int xcnt = 2; \
    va_list vargs; \
    va_start(vargs, argv); \
    while ( va_arg(vargs, const char*) ) xcnt++; \
    va_end(vargs); \
    const char **args = new const char*[xcnt+1]; \
    typedef ArrayDeleter<const char*> TArgsDeleter; \
    AutoPtr<const char*, TArgsDeleter> p_args(args); \
    if ( !args ) return -1; \
    args[0] = cmdname; \
    args[1] = argv; \
    va_start(vargs, argv); \
    int xi = 1; \
    while ( xi < xcnt ) { \
        xi++; \
        args[xi] = va_arg(vargs, const char*); \
    } \
    args[xi] = 0


int CExec::System(const char *cmdline)
{ 
    int status;
#if defined(NCBI_OS_MSWIN)
    status = system(cmdline); 
#elif defined(NCBI_OS_UNIX)
    status = system(cmdline);
#endif
    if (status == -1 ) {
        NCBI_THROW(CExecException,eSystem,
            "CExec::System: call to system failed");
    }
#if defined(NCBI_OS_UNIX)
    return cmdline ? WEXITSTATUS(status) : status;
#else
    return status;
#endif
}


int CExec::SpawnL(const EMode mode, const char *cmdname, const char *argv, ...)
{
    int status;
#if defined(NCBI_OS_MSWIN)
    status = spawnv(s_GetRealMode(mode), cmdname, &cmdname);
#elif defined(NCBI_OS_UNIX)
    GET_EXEC_ARGS;
    status = s_SpawnUnix(eV, mode, cmdname, args);
#endif
    if (status == -1 ) {
        NCBI_THROW(CExecException,eSpawn, "CExec::SpawnL");
    }
    return status;
}


int CExec::SpawnLE(const EMode mode, const char *cmdname,  const char *argv, ...)
{
    int status;
    GET_EXEC_ARGS;
    char** envp = va_arg(vargs, char**);
#if defined(NCBI_OS_MSWIN)
    status = spawnve(s_GetRealMode(mode), cmdname, args, envp);
#elif defined(NCBI_OS_UNIX)
    status = s_SpawnUnix(eVE, mode, cmdname, args, envp);
#endif
    if (status == -1 ) {
        NCBI_THROW(CExecException,eSpawn, "CExec::SpawnLE");
    }
    return status;
}


int CExec::SpawnLP(const EMode mode, const char *cmdname, 
                   const char *argv, ...)
{
    int status;
#if defined(NCBI_OS_MSWIN)
    status = spawnvp(s_GetRealMode(mode), cmdname, &cmdname);
#elif defined(NCBI_OS_UNIX)
    GET_EXEC_ARGS;
    status = s_SpawnUnix(eVP, mode, cmdname, args);
#endif
    if (status == -1 ) {
        NCBI_THROW(CExecException,eSpawn, "CExec::SpawnLP");
    }
    return status;
}


int CExec::SpawnLPE(const EMode mode, const char *cmdname,
                    const char *argv, ...)
{
    int status;
    GET_EXEC_ARGS;
    char** envp = va_arg(vargs, char**);
#if defined(NCBI_OS_MSWIN)
    status = spawnve(s_GetRealMode(mode), cmdname, args, envp);
#elif defined(NCBI_OS_UNIX)
    status = s_SpawnUnix(eVPE, mode, cmdname, args, envp);
#endif
    if (status == -1 ) {
        NCBI_THROW(CExecException,eSpawn, "CExec::SpawnLPE");
    }
    return status;
}


int CExec::SpawnV(const EMode mode, const char *cmdname,
                  const char *const *argv)
{
    int status;
    char** argp = const_cast<char**>(argv);
    argp[0] = const_cast<char*>(cmdname);
#if defined(NCBI_OS_MSWIN)
    status = spawnv(s_GetRealMode(mode), cmdname, argv);
#elif defined(NCBI_OS_UNIX)
    status = s_SpawnUnix(eV, mode, cmdname, argv);
#endif
    if (status == -1 ) {
        NCBI_THROW(CExecException,eSpawn, "CExec::SpawnV");
    }
    return status;
}


int CExec::SpawnVE(const EMode mode, const char *cmdname, 
                   const char *const *argv, const char * const *envp)
{
    int status;
    char** argp = const_cast<char**>(argv);
    argp[0] = const_cast<char*>(cmdname);
#if defined(NCBI_OS_MSWIN)
    status = spawnve(s_GetRealMode(mode), cmdname, argv, envp);
#elif defined(NCBI_OS_UNIX)
    status = s_SpawnUnix(eVE, mode, cmdname, argv, envp);
#endif
    if (status == -1 ) {
        NCBI_THROW(CExecException,eSpawn, "CExec::SpawnVE");
    }
    return status;
}


int CExec::SpawnVP(const EMode mode, const char *cmdname,
                   const char *const *argv)
{
    int status;
    char** argp = const_cast<char**>(argv);
    argp[0] = const_cast<char*>(cmdname);
#if defined(NCBI_OS_MSWIN)
    status = spawnvp(s_GetRealMode(mode), cmdname, argv);
#elif defined(NCBI_OS_UNIX)
    status = s_SpawnUnix(eVP, mode, cmdname, argv);
#endif
    if (status == -1 ) {
        NCBI_THROW(CExecException,eSpawn, "CExec::SpawnVP");
    }
    return status;
}


int CExec::SpawnVPE(const EMode mode, const char *cmdname,
                    const char *const *argv, const char * const *envp)
{
    int status;
    char** argp = const_cast<char**>(argv);
    argp[0] = const_cast<char*>(cmdname);
#if defined(NCBI_OS_MSWIN)
    status = spawnvpe(s_GetRealMode(mode), cmdname, argv, envp);
#elif defined(NCBI_OS_UNIX)
    status = s_SpawnUnix(eVPE, mode, cmdname, argv, envp);
#endif
    if (status == -1 ) {
        NCBI_THROW(CExecException,eSpawn, "CExec::SpawnVPE");
    }
    return status;
}


int CExec::Wait(const int pid)
{
    int status;
#if defined(NCBI_OS_MSWIN)
    if ( cwait(&status, pid, 0) == -1 ) 
        return -1;
#elif defined(NCBI_OS_UNIX)
    if ( waitpid(pid, &status, 0) == -1 ) 
        return -1;
    status = WEXITSTATUS(status);
#endif
    return status;
}


END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.12  2003/04/23 21:02:48  ivanov
 * Removed redundant NCBI_OS_MAC preprocessor directives
 *
 * Revision 1.11  2003/02/24 19:56:05  gouriano
 * use template-based exceptions instead of errno and parse exceptions
 *
 * Revision 1.10  2002/08/15 18:26:29  ivanov
 * Changed s_SpawnUnix() -- empty environment, setsid() for eDetach mode
 *
 * Revision 1.9  2002/07/17 15:12:34  ivanov
 * Changed method of obtaining parameters in the SpawnLE/LPE functions
 * under MS Windows
 *
 * Revision 1.8  2002/07/16 15:04:22  ivanov
 * Fixed warning about uninitialized variable in s_SpawnUnix()
 *
 * Revision 1.7  2002/07/15 16:38:50  ivanov
 * Fixed bug in macro GET_EXEC_ARGS -- write over array bound
 *
 * Revision 1.6  2002/07/11 14:18:26  gouriano
 * exceptions replaced by CNcbiException-type ones
 *
 * Revision 1.5  2002/06/30 03:22:14  vakatov
 * s_GetRealMode() -- formal code rearrangement to avoid a warning
 *
 * Revision 1.4  2002/06/11 19:28:31  ivanov
 * Use AutoPtr<char*> for exec arguments in GET_EXEC_ARGS
 *
 * Revision 1.3  2002/06/04 19:43:20  ivanov
 * Done s_ThrowException static
 *
 * Revision 1.2  2002/05/31 20:49:33  ivanov
 * Removed excrescent headers
 *
 * Revision 1.1  2002/05/30 16:29:13  ivanov
 * Initial revision
 *
 * ===========================================================================
 */
