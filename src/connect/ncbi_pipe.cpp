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
 * Author:  Anton Lavrentiev, Mike DiCuccio, Vladimir Ivanov
 *
 *
 */

#include <ncbi_pch.hpp>
#include <connect/ncbi_pipe.hpp>
#include <corelib/ncbi_system.hpp>
#include "ncbi_core_cxxp.hpp"
#include <assert.h>
#include <memory>
#include <stdio.h>

#ifdef NCBI_OS_MSWIN
#  include <windows.h>
#elif defined NCBI_OS_UNIX
#  include <unistd.h>
#  include <errno.h>
#  include <sys/time.h>
#  include <sys/types.h>
#  include <sys/wait.h>
#  include <signal.h>
#  include <fcntl.h>
#  ifdef NCBI_COMPILER_MW_MSL
#    include <ncbi_mslextras.h>
#  endif
#else
#  error "Class CPipe is supported only on Windows and Unix"
#endif

#define IS_SET(flags, mask) (((flags) & (mask)) == (mask))


BEGIN_NCBI_SCOPE


// Predefined timeout (in milliseconds)
const unsigned long kWaitPrecision = 100;


//////////////////////////////////////////////////////////////////////////////
//
// Auxiliary functions
//

static STimeout* s_SetTimeout(const STimeout* from, STimeout* to)
{
    if ( !from ) {
        return const_cast<STimeout*>(kInfiniteTimeout);
    }
    to->sec  = from->usec / 1000000 + from->sec;
    to->usec = from->usec % 1000000;
    return to;
}


//////////////////////////////////////////////////////////////////////////////
//
// Class CNamedPipeHandle handles forwarded requests from CPipe.
// This class is reimplemented in a platform-specific fashion where needed.
//


#if defined(NCBI_OS_MSWIN)


//////////////////////////////////////////////////////////////////////////////
//
// CPipeHandle -- MS Windows version
//

class CPipeHandle
{
public:
    CPipeHandle();
    ~CPipeHandle();
    EIO_Status Open(const string& cmd, const vector<string>& args,
                    CPipe::TCreateFlags create_flags,
                    const string&       current_dir,
                    const char* const   env[]);
    EIO_Status Close(int* exitcode, const STimeout* timeout);
    EIO_Status CloseHandle (CPipe::EChildIOHandle handle);
    EIO_Status Read(void* buf, size_t count, size_t* n_read,
                    const CPipe::EChildIOHandle from_handle,
                    const STimeout* timeout) const;
    EIO_Status Write(const void* buf, size_t count, size_t* written,
                     const STimeout* timeout) const;
    CPipe::TChildPollMask Poll(CPipe::TChildPollMask mask,
                               const STimeout* timeout) const;
    TProcessHandle GetProcessHandle(void) const { return m_ProcHandle; };

private:
    // Clear object state.
    void   x_Clear(void);
    // Get child's I/O handle.
    HANDLE x_GetHandle(CPipe::EChildIOHandle from_handle) const;
    // Convert STimeout value to number of milliseconds.
    long   x_TimeoutToMSec(const STimeout* timeout) const;
    // Trigger blocking mode on specified I/O handle.
    bool   x_SetNonBlockingMode(HANDLE fd, bool nonblock = true) const;
    // Wait on the file descriptors I/O.
    CPipe::TChildPollMask x_Poll(CPipe::TChildPollMask mask,
                                 const STimeout* timeout) const;
private:
    // I/O handles for child process.
    HANDLE m_ChildStdIn;
    HANDLE m_ChildStdOut;
    HANDLE m_ChildStdErr;

    // Child process descriptor.
    HANDLE m_ProcHandle;
    // Child process pid.
    TPid   m_Pid;

    // Pipe flags
    CPipe::TCreateFlags m_Flags;
};


CPipeHandle::CPipeHandle()
    : m_ProcHandle(INVALID_HANDLE_VALUE),
      m_ChildStdIn(INVALID_HANDLE_VALUE),
      m_ChildStdOut(INVALID_HANDLE_VALUE),
      m_ChildStdErr(INVALID_HANDLE_VALUE),
      m_Flags(0)
{
    return;
}


CPipeHandle::~CPipeHandle()
{
    static const STimeout kZeroTimeout = {0, 0};
    Close(0, &kZeroTimeout);
    x_Clear();
}


EIO_Status CPipeHandle::Open(const string&         cmd,
                             const vector<string>& args,
                             CPipe::TCreateFlags   create_flags,
                             const string&         current_dir,
                             const char* const     env[])
{
    DEFINE_STATIC_FAST_MUTEX(s_Mutex);
    CFastMutexGuard guard_mutex(s_Mutex);

    x_Clear();

    bool need_restore_handles = false; 
    m_Flags = create_flags;

    HANDLE stdin_handle       = INVALID_HANDLE_VALUE;
    HANDLE stdout_handle      = INVALID_HANDLE_VALUE;
    HANDLE stderr_handle      = INVALID_HANDLE_VALUE;
    HANDLE child_stdin_read   = INVALID_HANDLE_VALUE;
    HANDLE child_stdin_write  = INVALID_HANDLE_VALUE;
    HANDLE child_stdout_read  = INVALID_HANDLE_VALUE;
    HANDLE child_stdout_write = INVALID_HANDLE_VALUE;
    HANDLE child_stderr_read  = INVALID_HANDLE_VALUE;
    HANDLE child_stderr_write = INVALID_HANDLE_VALUE;

    try {
        if (m_ProcHandle != INVALID_HANDLE_VALUE) {
            throw string("Pipe is already open");
        }

        // Save current I/O handles
        stdin_handle  = GetStdHandle(STD_INPUT_HANDLE);
        stdout_handle = GetStdHandle(STD_OUTPUT_HANDLE);
        stderr_handle = GetStdHandle(STD_ERROR_HANDLE);
        
        // Flush std.output buffers before remap
        FlushFileBuffers(stdout_handle);
        FlushFileBuffers(stderr_handle);

        // Set base security attributes
        SECURITY_ATTRIBUTES attr;
        attr.nLength = sizeof(attr);
        attr.bInheritHandle = TRUE;
        attr.lpSecurityDescriptor = NULL;

        HANDLE current_process = GetCurrentProcess();
        need_restore_handles = true; 

        // Create pipe for child's stdin
        assert(CPipe::fStdIn_Close);
        if ( !IS_SET(create_flags, CPipe::fStdIn_Close) ) {
            if ( !CreatePipe(&child_stdin_read,
                             &child_stdin_write, &attr, 0) ) {
                throw string("CreatePipe() failed");
            }
            if ( !SetStdHandle(STD_INPUT_HANDLE, child_stdin_read) ) {
                throw string("Failed to remap stdin for child process");
            }
            // Duplicate the handle
            if ( !DuplicateHandle(current_process, child_stdin_write,
                                  current_process, &m_ChildStdIn,
                                  0, FALSE, DUPLICATE_SAME_ACCESS) ) {
                throw string("DuplicateHandle() failed on "
                             "child's stdin handle");
            }
            ::CloseHandle(child_stdin_write);
            x_SetNonBlockingMode(m_ChildStdIn);
        }

        // Create pipe for child's stdout
        assert(CPipe::fStdOut_Close);
        if ( !IS_SET(create_flags, CPipe::fStdOut_Close) ) {
            if ( !CreatePipe(&child_stdout_read,
                             &child_stdout_write, &attr, 0)) {
                throw string("CreatePipe() failed");
            }
            if ( !SetStdHandle(STD_OUTPUT_HANDLE, child_stdout_write) ) {
                throw string("Failed to remap stdout for child process");
            }
            // Duplicate the handle
            if ( !DuplicateHandle(current_process, child_stdout_read,
                                  current_process, &m_ChildStdOut,
                                  0, FALSE, DUPLICATE_SAME_ACCESS) ) {
                throw string("DuplicateHandle() failed on "
                             "child's stdout handle");
            }
            ::CloseHandle(child_stdout_read);
            x_SetNonBlockingMode(m_ChildStdOut);
        }

        // Create pipe for child's stderr
        assert(CPipe::fStdErr_Open);
        if ( IS_SET(create_flags, CPipe::fStdErr_Open) ) {
            if ( !CreatePipe(&child_stderr_read,
                             &child_stderr_write, &attr, 0)) {
                throw string("CreatePipe() failed");
            }
            if ( !SetStdHandle(STD_ERROR_HANDLE, child_stderr_write) ) {
                throw string("Failed to remap stderr for child process");
            }
            // Duplicate the handle
            if ( !DuplicateHandle(current_process, child_stderr_read,
                                  current_process, &m_ChildStdErr,
                                  0, FALSE, DUPLICATE_SAME_ACCESS) ) {
                throw string("DuplicateHandle() failed on "
                             "child's stderr handle");
            }
            ::CloseHandle(child_stderr_read);
            x_SetNonBlockingMode(m_ChildStdErr);
        }

        // Prepare command line to run
        string cmd_line(cmd);
        ITERATE (vector<string>, iter, args) {
            string arg = *iter;
            // Escape the argument with quotes if it is empty
            // or contains spaces.
            if ( arg.empty()  ||
                (arg.find(' ') != NPOS  &&  arg[0] != '"') ) {
                arg = '"' + arg + '"';
            }
            // Add argument to command line
            if ( !cmd_line.empty() ) {
                cmd_line += " ";
            }
            cmd_line += arg;
        }

        // Convert environment array to block form
        AutoPtr<char, ArrayDeleter<char> > env_block;
        if ( env ) {
            // Count block size.
            // It should have one zero byte at least.
            size_t size = 1; 
            int count = 0;
            while ( env[count] ) {
                size += strlen(env[count++]) + 1 /*zero byte*/;
            }
            // Allocate memory
            char* block = new char[size];
            if ( !block )
                NCBI_THROW(CCoreException, eNullPtr, kEmptyStr);
            env_block.reset(block);

            // Copy environment strings
            for (int i=0; i<count; i++) {
                size_t n = strlen(env[i]) + 1;
                memcpy(block, env[i], n);
                block += n;
            }
            *block = '\0';
        }

        // Create child process
        PROCESS_INFORMATION pinfo;
        STARTUPINFO sinfo;
        ZeroMemory(&pinfo, sizeof(PROCESS_INFORMATION));
        ZeroMemory(&sinfo, sizeof(STARTUPINFO));
        sinfo.cb = sizeof(sinfo);

        if ( !CreateProcess(NULL,
                            const_cast<char*> (cmd_line.c_str()),
                            NULL, NULL, TRUE, 0,
                            env_block.get(),
                            current_dir.empty() ? 0 : current_dir.c_str(),
                            &sinfo, &pinfo) ) {
            throw "CreateProcess() for \"" + cmd_line + "\" failed";
        }
        ::CloseHandle(pinfo.hThread);
        m_ProcHandle = pinfo.hProcess;
        m_Pid        = pinfo.dwProcessId; 

        assert(m_ProcHandle != INVALID_HANDLE_VALUE);

        // Restore remapped handles back to their original states
        if ( !SetStdHandle(STD_INPUT_HANDLE,  stdin_handle) ) {
            throw string("Failed to remap stdin for parent process");
        }
        if ( !SetStdHandle(STD_OUTPUT_HANDLE, stdout_handle) ) {
            throw string("Failed to remap stdout for parent process");
        }
        if ( !SetStdHandle(STD_ERROR_HANDLE,  stderr_handle) ) {
            throw string("Failed to remap stderr for parent process");
        }
        // Close unused pipe handles
        ::CloseHandle(child_stdin_read);
        ::CloseHandle(child_stdout_write);
        ::CloseHandle(child_stderr_write);

        return eIO_Success;
    }
    catch (string& what) {
        // Restore all standard handles on error
        if ( need_restore_handles ) {
            SetStdHandle(STD_INPUT_HANDLE,  stdin_handle);
            SetStdHandle(STD_OUTPUT_HANDLE, stdout_handle);
            SetStdHandle(STD_ERROR_HANDLE,  stderr_handle);

            ::CloseHandle(child_stdin_read);
            ::CloseHandle(child_stdin_write);
            ::CloseHandle(child_stdout_read);
            ::CloseHandle(child_stdout_write);
            ::CloseHandle(child_stderr_read);
            ::CloseHandle(child_stderr_write);
        }
        const STimeout kZeroZimeout = {0,0};
        Close(0, &kZeroZimeout);
        ERR_POST(what);
        return eIO_Unknown;
    }
}


EIO_Status CPipeHandle::Close(int* exitcode, const STimeout* timeout)
{
    if (m_ProcHandle == INVALID_HANDLE_VALUE) {
        return eIO_Closed;
    }

    DWORD      x_exitcode = -1;
    EIO_Status status     = eIO_Unknown;

    // Get exit code of child process
    if ( GetExitCodeProcess(m_ProcHandle, &x_exitcode) ) {
        if (x_exitcode == STILL_ACTIVE) {
            // Wait for the child process to exit
            DWORD ws = WaitForSingleObject(m_ProcHandle,
                                           x_TimeoutToMSec(timeout));
            if (ws == WAIT_OBJECT_0) {
                // Get exit code of child process over again
                if ( GetExitCodeProcess(m_ProcHandle, &x_exitcode) ) {
                    status = (x_exitcode == STILL_ACTIVE) ? 
                        eIO_Timeout : eIO_Success;
                }
            } else if (ws == WAIT_TIMEOUT) {
                status = eIO_Timeout;
            }
        } else {
            status = eIO_Success;
        }
    }
    if (status != eIO_Success) {
        x_exitcode = -1;
    }

    // Is the process still running?
    if (status == eIO_Timeout  &&  !IS_SET(m_Flags, CPipe::fKeepOnClose)) {
        status = (!IS_SET(m_Flags, CPipe::fKillOnClose)  ||
                  CProcess(m_Pid, CProcess::ePid).Kill()
                  ? eIO_Success : eIO_Unknown);
    }
    if (status != eIO_Timeout) {
        x_Clear();
    }

    if ( exitcode ) {
        *exitcode = (int) x_exitcode;
    }
    return status;
}


EIO_Status CPipeHandle::CloseHandle(CPipe::EChildIOHandle handle)
{
    switch (handle) {
    case CPipe::eStdIn:
        if (m_ChildStdIn == INVALID_HANDLE_VALUE) {
            return eIO_Closed;
        }
        ::CloseHandle(m_ChildStdIn);
        m_ChildStdIn = INVALID_HANDLE_VALUE;
        break;
    case CPipe::eStdOut:
        if (m_ChildStdOut == INVALID_HANDLE_VALUE) {
            return eIO_Closed;
        }
        ::CloseHandle(m_ChildStdOut);
        m_ChildStdOut = INVALID_HANDLE_VALUE;
        break;
    case CPipe::eStdErr:
        if (m_ChildStdErr == INVALID_HANDLE_VALUE) {
            return eIO_Closed;
        }
        ::CloseHandle(m_ChildStdErr);
        m_ChildStdErr = INVALID_HANDLE_VALUE;
        break;
    default:
        return eIO_InvalidArg;
    }
    return eIO_Success;
}


EIO_Status CPipeHandle::Read(void* buf, size_t count, size_t* read, 
                             const CPipe::EChildIOHandle from_handle,
                             const STimeout* timeout) const
{
    EIO_Status status = eIO_Unknown;

    try {
        if (m_ProcHandle == INVALID_HANDLE_VALUE) {
            status = eIO_Closed;
            throw string("Pipe is closed");
        }
        HANDLE fd = x_GetHandle(from_handle);
        if (fd == INVALID_HANDLE_VALUE) {
            throw string("Pipe I/O handle is closed");
        }
        if ( !count ) {
            return eIO_Success;
        }

        DWORD x_timeout   = x_TimeoutToMSec(timeout);
        DWORD bytes_avail = 0;
        DWORD bytes_read  = 0;

        // Wait for data from the pipe with timeout.
        // Using a loop and periodicaly try PeekNamedPipe is inefficient,
        // but Windows doesn't have asynchronous mechanism to read
        // from a pipe.
        // NOTE:  WaitForSingleObject() doesn't work with anonymous pipes.
        // See CPipe::Poll() for more details.
        for (;;) {
            if ( !PeekNamedPipe(fd, NULL, 0, NULL, &bytes_avail, NULL) ) {
                // Has peer closed connection?
                if (GetLastError() == ERROR_BROKEN_PIPE) {
                    return eIO_Closed;
                }
                throw string("PeekNamedPipe() failed");
            }
            if ( bytes_avail ) {
                break;
            }
            unsigned long x_sleep = kWaitPrecision;
            if (x_timeout != INFINITE) {
                if (x_sleep > x_timeout) {
                    x_sleep = x_timeout;
                }
                if ( !x_sleep ) {
                    break;
                }
                x_timeout -= x_sleep;
            }
            SleepMilliSec(x_sleep);
        }

        // Data is available to read or read request has timed out
        if ( !bytes_avail ) {
            return eIO_Timeout;
        }
        // We must read only "count" bytes of data regardless of
        // the amount available to read
        if (bytes_avail > count) {
            bytes_avail = count;
        }
        if ( !ReadFile(fd, buf, count, &bytes_avail, NULL) ) {
            throw string("Failed to read data from pipe");
        }
        if ( read ) {
            *read = bytes_avail;
        }
        status = eIO_Success;
    }
    catch (string& what) {
        ERR_POST(what);
    }
    return status;
}


EIO_Status CPipeHandle::Write(const void* buf, size_t count,
                              size_t* n_written, const STimeout* timeout) const

{
    EIO_Status status = eIO_Unknown;

    try {
        if (m_ProcHandle == INVALID_HANDLE_VALUE) {
            status = eIO_Closed;
            throw string("Pipe is closed");
        }
        if (m_ChildStdIn == INVALID_HANDLE_VALUE) {
            throw string("Pipe I/O handle is closed");
        }
        if ( !count ) {
            return eIO_Success;
        }

        DWORD x_timeout     = x_TimeoutToMSec(timeout);
        DWORD bytes_written = 0;

        // Try to write data into the pipe within specified time.
        for (;;) {
            if ( !WriteFile(m_ChildStdIn, (char*)buf, count,
                            &bytes_written, NULL) ) {
                if ( n_written ) {
                    *n_written = bytes_written;
                }
                throw string("Failed to write data into pipe");
            }
            if ( bytes_written ) {
                status = eIO_Success;
                break;
            }
            DWORD x_sleep = kWaitPrecision;
            if (x_timeout != INFINITE) {
                if ( x_timeout ) {
                    if (x_sleep > x_timeout) {
                        x_sleep = x_timeout;
                    }
                    x_timeout -= x_sleep;
                } else {
                    status = eIO_Timeout;
                    break;
                }
            }
            SleepMilliSec(x_sleep);
        }
        if ( n_written ) {
            *n_written = bytes_written;
        }
    }
    catch (string& what) {
        ERR_POST(what);
    }
    return status;
}


CPipe::TChildPollMask CPipeHandle::Poll(CPipe::TChildPollMask mask,
                                        const STimeout* timeout) const
{
    CPipe::TChildPollMask poll = 0;

    try {
        if (m_ProcHandle == INVALID_HANDLE_VALUE) {
            throw string("Pipe is closed");
        }
        if (m_ChildStdIn  == INVALID_HANDLE_VALUE  &&
            m_ChildStdOut == INVALID_HANDLE_VALUE  &&
            m_ChildStdErr == INVALID_HANDLE_VALUE) {
            throw string("All pipe I/O handles are closed");
        }
        poll = x_Poll(mask, timeout);
    }
    catch (string& what) {
        ERR_POST(what);
    }
    return poll;
}


void CPipeHandle::x_Clear(void)
{
    if (m_ChildStdIn != INVALID_HANDLE_VALUE) {
        ::CloseHandle(m_ChildStdIn);
        m_ChildStdIn = INVALID_HANDLE_VALUE;
    }
    if (m_ChildStdOut != INVALID_HANDLE_VALUE) {
        ::CloseHandle(m_ChildStdOut);
        m_ChildStdOut = INVALID_HANDLE_VALUE;
    }
    if (m_ChildStdErr != INVALID_HANDLE_VALUE) {
        ::CloseHandle(m_ChildStdErr);
        m_ChildStdErr = INVALID_HANDLE_VALUE;
    }
    m_ProcHandle = INVALID_HANDLE_VALUE;
    m_Pid = 0;
}


HANDLE CPipeHandle::x_GetHandle(CPipe::EChildIOHandle from_handle) const
{
    switch (from_handle) {
    case CPipe::eStdIn:
        return m_ChildStdIn;
    case CPipe::eStdOut:
        return m_ChildStdOut;
    case CPipe::eStdErr:
        return m_ChildStdErr;
    }
    return INVALID_HANDLE_VALUE;
}


long CPipeHandle::x_TimeoutToMSec(const STimeout* timeout) const
{
    return timeout ? timeout->sec * 1000 + (timeout->usec + 500) / 1000 
                   : INFINITE;
}


bool CPipeHandle::x_SetNonBlockingMode(HANDLE fd, bool nonblock) const
{
    // Pipe is in the byte-mode.
    // NOTE: We cannot get a state of a pipe handle opened for writing.
    //       We cannot set a state of a pipe handle opened for reading.
    DWORD state = nonblock ? PIPE_READMODE_BYTE | PIPE_NOWAIT :
                             PIPE_READMODE_BYTE;
    return SetNamedPipeHandleState(fd, &state, NULL, NULL) != 0; 
}


CPipe::TChildPollMask CPipeHandle::x_Poll(CPipe::TChildPollMask mask,
                                          const STimeout* timeout) const
{
    CPipe::TChildPollMask poll = 0;
    DWORD x_timeout = x_TimeoutToMSec(timeout);

    // Wait for data from the pipe with timeout.
    // Using a loop and periodicaly try PeekNamedPipe is inefficient,
    // but Windows doesn't have asynchronous mechanism to read
    // from a pipe.
    // NOTE: WaitForSingleObject() doesn't work with anonymous pipes.

    for (;;) {
        if ( (mask & CPipe::fStdOut)  &&
              m_ChildStdOut != INVALID_HANDLE_VALUE ) {
            DWORD bytes_avail = 0;
            if ( !PeekNamedPipe(m_ChildStdOut, NULL, 0, NULL,
                                &bytes_avail, NULL) ) {
                // Has peer closed connection?
                if (GetLastError() != ERROR_BROKEN_PIPE) {
                    throw string("PeekNamedPipe() failed");
                }
                poll |= CPipe::fStdOut;
            } else if ( bytes_avail ) {
                poll |= CPipe::fStdOut;
            }
        }
        if ( (mask & CPipe::fStdErr)  &&
              m_ChildStdErr != INVALID_HANDLE_VALUE ) {
            DWORD bytes_avail = 0;
            if ( !PeekNamedPipe(m_ChildStdErr, NULL, 0, NULL,
                                &bytes_avail, NULL) ) {
                // Has peer closed connection?
                if (GetLastError() != ERROR_BROKEN_PIPE) {
                    throw string("PeekNamedPipe() failed");
                }
                poll |= CPipe::fStdErr;
            } else if ( bytes_avail ) {
                poll |= CPipe::fStdErr;
            }
        }
        if ( poll ) {
            break;
        }
        unsigned long x_sleep = kWaitPrecision;
        if (x_timeout != INFINITE) {
            if (x_sleep > x_timeout) {
                x_sleep = x_timeout;
            }
            if ( !x_sleep ) {
                break;
            }
            x_timeout -= x_sleep;
        }
        SleepMilliSec(x_sleep);
    }

    // We cannot poll child's stdin, so just copy corresponding flag
    // from source to result mask before return
    poll |= (mask & CPipe::fStdIn);
    return poll;
}


#elif defined(NCBI_OS_UNIX)


//////////////////////////////////////////////////////////////////////////////
//
// CPipeHandle -- Unix version
//

class CPipeHandle
{
public:
    CPipeHandle();
    ~CPipeHandle();
    EIO_Status Open(const string&         cmd,
                    const vector<string>& args,
                    CPipe::TCreateFlags   create_flags,
                    const string&         current_dir,
                    const char* const     env[]);
    EIO_Status Close(int* exitcode, const STimeout* timeout);
    EIO_Status CloseHandle(CPipe::EChildIOHandle handle);
    EIO_Status Read(void* buf, size_t count, size_t* read,
                    const CPipe::EChildIOHandle from_handle,
                    const STimeout* timeout) const;
    EIO_Status Write(const void* buf, size_t count, size_t* written,
                     const STimeout* timeout) const;
    CPipe::TChildPollMask Poll(CPipe::TChildPollMask mask,
                               const STimeout* timeout) const;
    TProcessHandle GetProcessHandle(void) const { return m_Pid; };

private:
    // Clear object state.
    void x_Clear(void);
    // Get child's I/O handle.
    int  x_GetHandle(CPipe::EChildIOHandle from_handle) const;
    // Trigger blocking mode on specified I/O handle.
    bool x_SetNonBlockingMode(int fd, bool nonblock = true) const;
    // Wait on the file descriptors I/O.
    CPipe::TChildPollMask x_Poll(CPipe::TChildPollMask mask,
                                 const STimeout* timeout) const;

private:
    // I/O handles for child process.
    int  m_ChildStdIn;
    int  m_ChildStdOut;
    int  m_ChildStdErr;

    // Child process pid.
    TPid m_Pid;

    // Pipe flags
    CPipe::TCreateFlags m_Flags;
};


CPipeHandle::CPipeHandle()
    : m_ChildStdIn(-1), m_ChildStdOut(-1), m_ChildStdErr(-1),
      m_Pid((pid_t)(-1)), m_Flags(0)
{
    return;
}


CPipeHandle::~CPipeHandle()
{
    static const STimeout kZeroTimeout = {0, 0};
    Close(0, &kZeroTimeout);
    x_Clear();
}


// Auxiliary function for exit from forked process with reporting errno
// on errors to specified file descriptor 
void s_Exit(int status, int fd)
{
    int errcode = errno;
    write(fd, &errcode, sizeof(errcode));
    close(fd);
    _exit(status);
}


// Emulate non-existent function execvpe().
// On success, execve() does not return, on error -1 is returned,
// and errno is set appropriately.

static const char* kShell = "/bin/sh";
static const char* kPathDefault = ":/bin:/usr/bin";

static int s_ExecShell(const char *file, char *const argv[], char *const envp[])
{
    // Count number of arguments
    int i;
    for (i = 0; argv[i]; i++);
    i++; // copy last zero element also
    
    // Construct an argument list for the shell.
    // Not all compilers support next construction:
    //   const char* args[i + 1];
    const char **args = new const char*[i+1];
    AutoPtr<const char*,  ArrayDeleter<const char*> > args_ptr(args);

    args[0] = kShell;
    args[1] = file;
    for (; i > 1; i--) {
        args[i] = argv[i - 1];
    }
    // Execute the shell
    return execve(kShell, (char**)args, envp);
}


static int s_ExecVPE(const char *file, char *const argv[], char *const envp[])
{
    // If file name is not specified
    if ( !file  ||  *file == '\0') {
        errno = ENOENT;
        return -1;
    }
    // If the file name contains path
    if ( strchr(file, '/') ) {
        execve(file, argv, envp);
        if (errno == ENOEXEC) {
            return s_ExecShell(file, argv, envp);
        }
        return -1;
    }
    // Get PATH environment variable   
    const char *path = getenv("PATH");
    if ( !path ) {
        path = kPathDefault;
    }
    size_t file_len = strlen(file) + 1 /* '\0' */;
    size_t buf_len = strlen(path) + file_len + 1 /* '/' */;
    char* buf = new char[buf_len];
    if ( !buf ) {
        NCBI_THROW(CCoreException, eNullPtr, kEmptyStr);
    }
    AutoPtr<char, ArrayDeleter<char> > buf_ptr(buf);

    bool eacces_err = false;
    const char* next = path;
    while (*next) {
        next = strchr(path,':');
        if ( !next ) {
            // Last part of the PATH environment variable
            next = path + strlen(path);
        }
        size_t len = next - path;
        if ( len ) {
            // Copy directory name into the buffer
            memmove(buf, path, next - path);
        } else {
            // Two colons side by side -- current directory
            buf[0]='.';
            len = 1;
        }
        // Add slash and file name
        if (buf[len-1] != '/') {
            buf[len++] = '/';
        }
        memcpy(buf + len, file, file_len);
        path = next + 1;

        // Try to execute file with generated name
        execve(buf, argv, envp);
        if (errno == ENOEXEC) {
            return s_ExecShell(buf, argv, envp);
        }
        switch (errno) {
        case EACCES:
            // Permission denied. Memorize this thing and try next path.
            eacces_err = true;
        case ENOENT:
        case ENOTDIR:
            // Try next path directory
            break;
        default:
            // We found an executable file, but cannot execute it
            return -1;
        }
    }
    if ( eacces_err ) {
        errno = EACCES;
    }
    return -1;
}


EIO_Status CPipeHandle::Open(const string&         cmd,
                             const vector<string>& args,
                             CPipe::TCreateFlags   create_flags,
                             const string&         current_dir,
                             const char* const     env[])

{
    DEFINE_STATIC_FAST_MUTEX(s_Mutex);
    CFastMutexGuard guard_mutex(s_Mutex);

    x_Clear();
    m_Flags = create_flags;
    int pipe_in[2], pipe_out[2], pipe_err[2], status_pipe[2];

    // Child process I/O handles
    pipe_in[0]     = -1;
    pipe_out[1]    = -1;
    pipe_err[1]    = -1;
    status_pipe[0] = -1;
    status_pipe[1] = -1;

    try {
        if (m_Pid != (pid_t)(-1)) {
            throw string("Pipe is already open");
        }

        // Create pipe for child's stdin
        assert(CPipe::fStdIn_Close);
        if ( !IS_SET(create_flags, CPipe::fStdIn_Close) ) {
            if (pipe(pipe_in) < 0) {
                throw string("Failed to create pipe for stdin");
            }
            m_ChildStdIn = pipe_in[1];
            x_SetNonBlockingMode(m_ChildStdIn);
        }
        // Create pipe for child's stdout
        assert(CPipe::fStdOut_Close);
        if ( !IS_SET(create_flags, CPipe::fStdOut_Close) ) {
            if (pipe(pipe_out) < 0) {
                throw string("Failed to create pipe for stdout");
            }
            fflush(stdout);
            m_ChildStdOut = pipe_out[0];
            x_SetNonBlockingMode(m_ChildStdOut);
        }
        // Create pipe for child's stderr
        assert(CPipe::fStdErr_Open);
        if ( IS_SET(create_flags, CPipe::fStdErr_Open) ) {
            if (pipe(pipe_err) < 0 ) {
                throw string("Failed to create pipe for stderr");
            }
            fflush(stderr);
            m_ChildStdErr = pipe_err[0];
            x_SetNonBlockingMode(m_ChildStdErr);
        }

        // Create temporary pipe to get status of execution
        // of the child process
        if (pipe(status_pipe) < 0) {
            throw string("Failed to create status pipe");
        }
        fcntl(status_pipe[0], F_SETFL, 
              fcntl(status_pipe[0], F_GETFL, 0) & ~O_NONBLOCK);
        fcntl(status_pipe[1], F_SETFD, 
              fcntl(status_pipe[1], F_GETFD, 0) | FD_CLOEXEC);

        // Fork child process
        switch (m_Pid = fork()) {
        case (pid_t)(-1):
            // Fork failed
            throw string("Failed to fork process");

        case 0:
            // Now we are in the child process
            int status = -1;

            // Close unused pipe handle
            close(status_pipe[0]);

            // Bind child's standard I/O file handles to pipe
            assert(CPipe::fStdIn_Close);
            if ( !IS_SET(create_flags, CPipe::fStdIn_Close) ) {
                if (pipe_in[0] != STDIN_FILENO) {
                    if (dup2(pipe_in[0], STDIN_FILENO) < 0) {
                        s_Exit(status, status_pipe[1]);
                    }
                    close(pipe_in[0]);
                }
                close(pipe_in[1]);
            } else {
                freopen("/dev/null", "r", stdin);
            }
            assert(CPipe::fStdOut_Close);
            if ( !IS_SET(create_flags, CPipe::fStdOut_Close) ) {
                if (pipe_out[1] != STDOUT_FILENO) {
                    if (dup2(pipe_out[1], STDOUT_FILENO) < 0) {
                        s_Exit(status, status_pipe[1]);
                    }
                    close(pipe_out[1]);
                }
                close(pipe_out[0]);
            } else {
                freopen("/dev/null", "w", stdout);
            }
            assert(!CPipe::fStdErr_Close);
            if ( IS_SET(create_flags, CPipe::fStdErr_Open) ) {
                if (pipe_err[1] != STDERR_FILENO) {
                    if (dup2(pipe_err[1], STDERR_FILENO) < 0) {
                        s_Exit(status, status_pipe[1]);
                    }
                    close(pipe_err[1]);
                }
                close(pipe_err[0]);
            } else {
                freopen("/dev/null", "a", stderr);
            }

            // Prepare program arguments
            size_t cnt = args.size();
            size_t i   = 0;
            const char** x_args = new const char*[cnt + 2];
            typedef ArrayDeleter<const char*> TArgsDeleter;
            AutoPtr<const char*, TArgsDeleter> p_args = x_args;
            ITERATE (vector<string>, arg, args) {
                x_args[++i] = arg->c_str();
            }
            x_args[0] = cmd.c_str();
            x_args[cnt + 1] = 0;

            // Change current working wirectory if specified
            if ( !current_dir.empty() ) {
                chdir(current_dir.c_str());
            }
            // Execute the program
            if ( env ) {
                // Emulate non-existent execvpe()
                status = s_ExecVPE(cmd.c_str(),
                                   const_cast<char**> (x_args),
                                   const_cast<char**>(env));
            } else {
                status = execvp(cmd.c_str(), const_cast<char**> (x_args));
            }
            s_Exit(status, status_pipe[1]);
        }

        // Close unused pipe handles
        assert(CPipe::fStdIn_Close);
        if ( !IS_SET(create_flags, CPipe::fStdIn_Close)  ) {
            close(pipe_in[0]);
            pipe_in[0] = -1;
        }
        assert(CPipe::fStdOut_Close);
        if ( !IS_SET(create_flags, CPipe::fStdOut_Close) ) {
            close(pipe_out[1]);
            pipe_out[1] = -1;
        }
        assert(!CPipe::fStdErr_Close);
        if (  IS_SET(create_flags, CPipe::fStdErr_Open)  ) {
            close(pipe_err[1]);
            pipe_err[1] = -1;
        }
        close(status_pipe[1]);

        // Check status pipe.
        // If it have some data, this is an errno from the child process.
        // If EOF in status pipe, that child executed successful.
        // Retry if either blocked or interrupted

        // Try to read errno from forked process
        int errcode;
        size_t n;
        while ((n = read(status_pipe[0], &errcode, sizeof(errcode))) < 0) {
            if (errno != EINTR)
                break;
        }
        if (n > 0) {
            // Child could not run -- rip it and exit with error
            errno = n >= sizeof(errcode) ? errcode : 0;
            waitpid(m_Pid, 0, 0);
            string errmsg = "Failed to execute programm";
            if (errno) {
                errmsg = errmsg + ": " + strerror(errcode);
            }
            throw errmsg;
        }
        close(status_pipe[0]);
        return eIO_Success;
    } 
    catch (string& what) {
        if ( pipe_in[0]  != -1 ) {
            close(pipe_in[0]);
        }
        if ( pipe_out[1] != -1 ) {
            close(pipe_out[1]);
        }
        if ( pipe_err[1] != -1 ) {
            close(pipe_err[1]);
        }
        if ( status_pipe[0] != -1 ) {
            close(status_pipe[0]);
        }
        if ( status_pipe[1] != -1 ) {
            close(status_pipe[1]);
        }
        // Close all opened file descriptors (close timeout doesn't apply here)
        Close(0,0);
        ERR_POST(what);
        return eIO_Unknown;
    }
}


EIO_Status CPipeHandle::Close(int* exitcode, const STimeout* timeout)
{    
    EIO_Status status     = eIO_Unknown;
    int        x_exitcode = -1;

    if (m_Pid == (pid_t)(-1)) {
        status = eIO_Closed;
    } else {
        int           x_options;
        unsigned long x_timeout;

        if ( timeout ) {
            // If timeout is not infinite
            x_timeout = timeout->sec * 1000 + (timeout->usec + 500) / 1000;
            x_options = WNOHANG;
        } else {
            x_timeout = 0/*irrelevant*/;
            x_options = 0;
        }

        // Retry if interrupted by signal
        for (;;) {
            pid_t ws = waitpid(m_Pid, &x_exitcode, x_options);
            if (ws > 0) {
                // Process has terminated
                assert(ws == m_Pid);
                status = eIO_Success;
                break;
            } else if (ws == 0) {
                // Process is still running
                assert(timeout);
                if ( !x_timeout  &&  status == eIO_Timeout ) {
                    break;
                }
                status = eIO_Timeout;
                if ( !timeout->sec  &&  !timeout->usec ) {
                    break;
                }
                unsigned long x_sleep = kWaitPrecision;
                if (x_sleep > x_timeout ) {
                    x_sleep = x_timeout;
                }
                SleepMilliSec(x_sleep);
                x_timeout -= x_sleep;
            } else if (errno != EINTR) {
                break;
            }
        }
    }
    if (status != eIO_Success) {
        x_exitcode = -1;
    }

    // Is the process still running?
    if (status == eIO_Timeout  &&  !IS_SET(m_Flags, CPipe::fKeepOnClose)) {
        status = (!IS_SET(m_Flags, CPipe::fKillOnClose)  ||
                  CProcess(m_Pid, CProcess::ePid).Kill()
                  ? eIO_Success : eIO_Unknown);
    }
    if (status != eIO_Timeout) {
        x_Clear();
    }

    if ( exitcode ) {
        // Get real exit code or -1 on error
        *exitcode = (status == eIO_Success  &&  x_exitcode != -1) ?
            WEXITSTATUS(x_exitcode) : -1;
    }
    return status;
}


EIO_Status CPipeHandle::CloseHandle(CPipe::EChildIOHandle handle)
{
    switch ( handle ) {
    case CPipe::eStdIn:
        if (m_ChildStdIn == -1) {
            return eIO_Closed;
        }
        close(m_ChildStdIn);
        m_ChildStdIn = -1;
        break;
    case CPipe::eStdOut:
        if (m_ChildStdOut == -1) {
            return eIO_Closed;
        }
        close(m_ChildStdOut);
        m_ChildStdOut = -1;
        break;
    case CPipe::eStdErr:
        if (m_ChildStdErr == -1) {
            return eIO_Closed;
        }
        close(m_ChildStdErr);
        m_ChildStdErr = -1;
        break;
    default:
        return eIO_InvalidArg;
    }
    return eIO_Success;
}


EIO_Status CPipeHandle::Read(void* buf, size_t count, size_t* n_read, 
                             const CPipe::EChildIOHandle from_handle,
                             const STimeout* timeout) const
{
    EIO_Status status = eIO_Unknown;

    try {
        if (m_Pid == (pid_t)(-1)) {
            status = eIO_Closed;
            throw string("Pipe is closed");
        }
        int fd = x_GetHandle(from_handle);
        if (fd == -1) {
            throw string("Pipe I/O handle is closed");
        }
        if ( !count ) {
            return eIO_Success;
        }

        // Retry if either blocked or interrupted
        for (;;) {
            // Try to read
            ssize_t bytes_read = read(fd, buf, count);
            if (bytes_read >= 0) {
                if ( n_read ) {
                    *n_read = (size_t)bytes_read;
                }
                status = bytes_read ? eIO_Success : eIO_Closed;
                break;
            }

            // Blocked -- wait for data to come;  exit if timeout/error
            if (errno == EAGAIN  ||  errno == EWOULDBLOCK) {
                if ( ( timeout  &&  !timeout->sec  &&  !timeout->usec )  ||
                    !x_Poll(from_handle, timeout) ) {
                    status = eIO_Timeout;
                    break;
                }
                continue;
            }

            // Interrupted read -- restart
            if (errno != EINTR) {
                throw string("Failed to read data from pipe");
            }
        }
    }
    catch (string& what) {
        ERR_POST(what);
    }
    return status;
}


EIO_Status CPipeHandle::Write(const void* buf, size_t count,
                              size_t* n_written, const STimeout* timeout) const

{
    EIO_Status status = eIO_Unknown;

    try {
        if (m_Pid == (pid_t)(-1)) {
            status = eIO_Closed;
            throw string("Pipe is closed");
        }
        if (m_ChildStdIn == -1) {
            throw string("Pipe I/O handle is closed");
        }
        if ( !count ) {
            return eIO_Success;
        }

        // Retry if either blocked or interrupted
        for (;;) {
            // Try to write
            ssize_t bytes_written = write(m_ChildStdIn, buf, count);
            if (bytes_written >= 0) {
                if ( n_written ) {
                    *n_written = (size_t) bytes_written;
                }
                status = eIO_Success;
                break;
            }

            // Peer has closed its end
            if (errno == EPIPE) {
                return eIO_Closed;
            }

            // Blocked -- wait for write readiness;  exit if timeout/error
            if (errno == EAGAIN  ||  errno == EWOULDBLOCK) {
                if ( ( timeout  &&  !timeout->sec  &&  !timeout->usec)  ||
                    !x_Poll(CPipe::fStdIn, timeout) ) {
                    status = eIO_Timeout;
                    break;
                }
                continue;
            }

            // Interrupted write -- restart
            if (errno != EINTR) {
                throw string("Failed to write data into pipe");
            }
        }
    }
    catch (string& what) {
        ERR_POST(what);
    }
    return status;
}


CPipe::TChildPollMask CPipeHandle::Poll(CPipe::TChildPollMask mask,
                                        const STimeout* timeout) const
{
    CPipe::TChildPollMask poll = 0;

    try {
        if (m_Pid == (pid_t)(-1)) {
            throw string("Pipe is closed");
        }
        if (m_ChildStdIn  == -1  &&
            m_ChildStdOut == -1  &&
            m_ChildStdErr == -1) {
            throw string("All pipe I/O handles are closed");
        }
        poll = x_Poll(mask, timeout);
    }
    catch (string& what) {
        ERR_POST(what);
    }
    return poll;
}


void CPipeHandle::x_Clear(void)
{
    if (m_ChildStdIn  != -1) {
        close(m_ChildStdIn);
        m_ChildStdIn   = -1;
    }
    if (m_ChildStdOut != -1) {
        close(m_ChildStdOut);
        m_ChildStdOut  = -1;
    }
    if (m_ChildStdErr != -1) {
        close(m_ChildStdErr);
        m_ChildStdErr  = -1;
    }
    m_Pid = -1;
}


int CPipeHandle::x_GetHandle(CPipe::EChildIOHandle from_handle) const
{
    switch (from_handle) {
    case CPipe::eStdIn:
        return m_ChildStdIn;
    case CPipe::eStdOut:
        return m_ChildStdOut;
    case CPipe::eStdErr:
        return m_ChildStdErr;
    default:
        break;
    }
    return -1;
}


bool CPipeHandle::x_SetNonBlockingMode(int fd, bool nonblock) const
{
    return fcntl(fd, F_SETFL,
                 nonblock ?
                 fcntl(fd, F_GETFL, 0) |  O_NONBLOCK :
                 fcntl(fd, F_GETFL, 0) & ~O_NONBLOCK) != -1;
}


CPipe::TChildPollMask CPipeHandle::x_Poll(CPipe::TChildPollMask mask,
                                          const STimeout* timeout) const
{
    CPipe::TChildPollMask poll = 0;

    for (;;) { // Auto-resume if interrupted by a signal
        struct timeval* tmp;
        struct timeval  tm;

        if ( timeout ) {
            // NB: Timeout has already been normalized
            tm.tv_sec  = timeout->sec;
            tm.tv_usec = timeout->usec;
            tmp = &tm;
        } else {
            tmp = 0;
        }

        fd_set rfds;
        fd_set wfds;
        fd_set efds;

        FD_ZERO(&rfds);
        FD_ZERO(&wfds);
        FD_ZERO(&efds);

        int max = -1;

        if ( (mask & CPipe::fStdIn)   &&  m_ChildStdIn  != -1 ) {
            FD_SET(m_ChildStdIn,  &wfds);
            FD_SET(m_ChildStdIn,  &efds);
            if (max < m_ChildStdIn) {
                max = m_ChildStdIn;
            }
        }
        if ( (mask & CPipe::fStdOut)  &&  m_ChildStdOut != -1 ) {
            FD_SET(m_ChildStdOut, &rfds);
            FD_SET(m_ChildStdOut, &efds);
            if (max < m_ChildStdOut) {
                max = m_ChildStdOut;
            }
        }
        if ( (mask & CPipe::fStdErr)  &&  m_ChildStdErr != -1 ) {
            FD_SET(m_ChildStdErr, &rfds);
            FD_SET(m_ChildStdErr, &efds);
            if (max < m_ChildStdErr) {
                max = m_ChildStdErr;
            }
        }

        int n = select(max + 1, &rfds, &wfds, &efds, tmp);

        if (n == 0) {
            // timeout
            break;
        } else if (n > 0) {
            if ( m_ChildStdIn  != -1  &&
                ( FD_ISSET(m_ChildStdIn,  &wfds)  ||
                  FD_ISSET(m_ChildStdIn,  &efds) ) ) {
                poll |= CPipe::fStdIn;
            }
            if ( m_ChildStdOut != -1  &&
                ( FD_ISSET(m_ChildStdOut, &rfds)  ||
                  FD_ISSET(m_ChildStdOut, &efds) ) ) {
                poll |= CPipe::fStdOut;
            }
            if ( m_ChildStdErr != -1  &&
                ( FD_ISSET(m_ChildStdErr, &rfds)  ||
                  FD_ISSET(m_ChildStdErr, &efds) ) ) {
                poll |= CPipe::fStdErr;
            }
            break;
        } else if (errno != EINTR) {
            throw string("Failed select() on pipe");
        }
        // continue
    }
    return poll;
}


#endif  /* NCBI_OS_UNIX | NCBI_OS_MSWIN */


//////////////////////////////////////////////////////////////////////////////
//
// CPipe
//

CPipe::CPipe(void)
    : m_PipeHandle(0), m_ReadHandle(eStdOut),
      m_ReadStatus(eIO_Closed), m_WriteStatus(eIO_Closed),
      m_ReadTimeout(0), m_WriteTimeout(0), m_CloseTimeout(0)
 
{
    CONNECT_InitInternal();
    // Create new OS-specific pipe handle
    m_PipeHandle = new CPipeHandle();
    if ( !m_PipeHandle ) {
        NCBI_THROW(CPipeException, eInit,
                   "Cannot create OS-specific pipe handle");
    }
}


CPipe::CPipe(const string& cmd, const vector<string>& args,
             TCreateFlags  create_flags,
             const string& current_dir,
             const char*   const env[])
    : m_PipeHandle(0), m_ReadHandle(eStdOut),
      m_ReadStatus(eIO_Closed), m_WriteStatus(eIO_Closed),
      m_ReadTimeout(0), m_WriteTimeout(0), m_CloseTimeout(0)
{
    CONNECT_InitInternal();
    // Create new OS-specific pipe handle
    m_PipeHandle = new CPipeHandle();
    if ( !m_PipeHandle ) {
        NCBI_THROW(CPipeException, eInit,
                   "Cannot create OS-specific pipe handle");
    }
    EIO_Status status = Open(cmd, args, create_flags, current_dir, env);
    if (status != eIO_Success) {
        NCBI_THROW(CPipeException, eOpen, "CPipe::Open() failed");
    }
}


CPipe::~CPipe(void)
{
    Close();
    if ( m_PipeHandle ) {
        delete m_PipeHandle;
    }
}


EIO_Status CPipe::Open(const string& cmd, const vector<string>& args,
                       TCreateFlags  create_flags,
                       const string& current_dir,
                       const char*   const env[])
{
    if ( !m_PipeHandle ) {
        return eIO_Unknown;
    }
    assert(CPipe::fStdIn_Close);

    EIO_Status status = m_PipeHandle->Open(cmd, args, create_flags,
                                           current_dir, env);
    if (status == eIO_Success) {
        m_ReadStatus  = eIO_Success;
        m_WriteStatus = eIO_Success;
    }
    return status;
}


EIO_Status CPipe::Close(int* exitcode)
{
    if ( !m_PipeHandle ) {
        return eIO_Unknown;
    }
    m_ReadStatus  = eIO_Closed;
    m_WriteStatus = eIO_Closed;

    m_PipeHandle->CloseHandle(eStdIn);
    m_PipeHandle->CloseHandle(eStdOut);
    m_PipeHandle->CloseHandle(eStdErr);
    return m_PipeHandle->Close(exitcode, m_CloseTimeout);
}


EIO_Status CPipe::CloseHandle(EChildIOHandle handle)
{
    if ( !m_PipeHandle ) {
        return eIO_Unknown;
    }
    return m_PipeHandle->CloseHandle(handle);
}


EIO_Status CPipe::Read(void* buf, size_t count, size_t* read,
                       EChildIOHandle from_handle)
{
    if ( read ) {
        *read = 0;
    }
    if (from_handle == eDefault)
        from_handle = m_ReadHandle;
    if (from_handle == eStdIn) {
        return eIO_InvalidArg;
    }
    if (count  &&  !buf) {
        return eIO_InvalidArg;
    }
    if ( !m_PipeHandle ) {
        return eIO_Unknown;
    }
    m_ReadStatus = m_PipeHandle->Read(buf, count, read, from_handle,
                                      m_ReadTimeout);
    return m_ReadStatus;
}


EIO_Status CPipe::SetReadHandle(EChildIOHandle from_handle)
{
    if (from_handle == eStdIn) {
        return eIO_Unknown;
    }
    if (from_handle != eDefault) {
        m_ReadHandle = from_handle;
    }
    return eIO_Success;
}


EIO_Status CPipe::Write(const void* buf, size_t count, size_t* written)
{
    if ( written ) {
        *written = 0;
    }
    if (count  &&  !buf) {
        return eIO_InvalidArg;
    }
    if ( !m_PipeHandle ) {
        return eIO_Unknown;
    }
    m_WriteStatus = m_PipeHandle->Write(buf, count, written,
                                        m_WriteTimeout);
    return m_WriteStatus;
}


CPipe::TChildPollMask CPipe::Poll(TChildPollMask mask, 
                                  const STimeout* timeout)
{
    if ( !m_PipeHandle ) {
        NCBI_THROW(CPipeException, eInit,
                   "OS-specific pipe handle is not created");
    }
    if ( !mask ) {
        return 0;
    }
    TChildPollMask x_mask = mask;
    if ( mask & fDefault ) {
        x_mask |= m_ReadHandle;
    }
    TChildPollMask poll = m_PipeHandle->Poll(x_mask, timeout);
    if ( mask & fDefault ) {
        TChildPollMask p = poll;
        poll &= mask;
        if ( p & m_ReadHandle ) {
            poll |= fDefault;
        }
    }
    return poll;
}


EIO_Status CPipe::Status(EIO_Event direction) const
{
    switch ( direction ) {
    case eIO_Read:
        return m_ReadStatus;
    case eIO_Write:
        return m_WriteStatus;
    default:
        break;
    }
    return eIO_InvalidArg;
}


EIO_Status CPipe::SetTimeout(EIO_Event event, const STimeout* timeout)
{
    if (timeout == kDefaultTimeout) {
        return eIO_Success;
    }
    switch ( event ) {
    case eIO_Close:
        m_CloseTimeout = s_SetTimeout(timeout, &m_CloseTimeoutValue);
        break;
    case eIO_Read:
        m_ReadTimeout  = s_SetTimeout(timeout, &m_ReadTimeoutValue);
        break;
    case eIO_Write:
        m_WriteTimeout = s_SetTimeout(timeout, &m_WriteTimeoutValue);
        break;
    case eIO_ReadWrite:
        m_ReadTimeout  = s_SetTimeout(timeout, &m_ReadTimeoutValue);
        m_WriteTimeout = s_SetTimeout(timeout, &m_WriteTimeoutValue);
        break;
    default:
        return eIO_InvalidArg;
    }
    return eIO_Success;
}


const STimeout* CPipe::GetTimeout(EIO_Event event) const
{
    switch ( event ) {
    case eIO_Close:
        return m_CloseTimeout;
    case eIO_Read:
        return m_ReadTimeout;
    case eIO_Write:
        return m_WriteTimeout;
    default:
        ;
    }
    return kDefaultTimeout;
}


TProcessHandle CPipe::GetProcessHandle(void) const
{
    return m_PipeHandle ? m_PipeHandle->GetProcessHandle() : 0;
}


END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.61  2006/06/16 14:46:25  ivanov
 * MSWin: CPipeHandle::Open -- escape arguments with quotes if it
 * is empty or contains spaces.
 *
 * Revision 1.60  2006/05/23 14:57:56  ivanov
 * Fixed typo in last commit
 *
 * Revision 1.59  2006/05/23 14:55:43  ivanov
 * CPipe::Open() - use string instead of char* for current directory
 *
 * Revision 1.58  2006/05/23 11:16:05  ivanov
 * Added possibility to specify a current working directory and
 * environment for created child processes.
 *
 * Revision 1.57  2006/03/13 11:44:47  ivanov
 * [Unix] CPipeHandle::Open() - using additional pipe to check if
 * the child has actually started.
 * Simplify s_FormatErrorMessage().
 *
 * Revision 1.56  2006/03/09 17:35:30  ivanov
 * MSWin: CPipeHandle::Read() wait timeout value in full
 *
 * Revision 1.55  2006/03/09 14:23:05  didenko
 * Added mutex lock in CPipe::Open method on Windows to prevent race condition
 *
 * Revision 1.54  2006/03/08 17:22:14  didenko
 * Quick and dirty fix for preventing race condition when a child process
 * is starting
 *
 * Revision 1.53  2006/03/08 14:36:29  ivanov
 * CPipe::Poll() -- added implementation for Windows,
 * fixed fDefault flag handling again.
 *
 * Revision 1.52  2006/03/07 17:18:40  lavr
 * Always check against redirect-into-self for dup'ed file descriptors
 *
 * Revision 1.51  2006/03/07 14:59:46  lavr
 * CPipe::Close()[Unix] adj.tmo.after sleep [in case actual slept time ret'd]
 *
 * Revision 1.50  2006/03/03 12:29:25  ivanov
 * Fixed compilation on MS Windows
 *
 * Revision 1.49  2006/03/03 03:17:45  lavr
 * Fix Close() on UNIX (small change on Windows, too)
 *
 * Revision 1.48  2006/03/02 20:28:12  lavr
 * Move timeout restrictions out of x_Poll() [for Poll() to work correctly]
 *
 * Revision 1.47  2006/03/02 20:15:51  lavr
 * Fix Unix version of CPipeHandle::x_Poll()
 *
 * Revision 1.46  2006/03/02 19:09:33  ivanov
 * CPipe::Poll() -- fixed fDefault handling
 *
 * Revision 1.45  2006/03/02 18:51:01  lavr
 * Use exception mask in select(); some code formatting
 *
 * Revision 1.44  2006/03/02 17:33:39  ivanov
 * UNIX: CPipeHandle:: use x_Poll() instead of x_Wait() in Read/Write
 *
 * Revision 1.43  2006/03/02 14:28:24  ivanov
 * UNIX: CPipeHandle::Close() -- release zombie process from the system
 *
 * Revision 1.42  2006/03/01 17:01:17  ivanov
 * + CPipe::Poll() -- Unix version only
 *
 * Revision 1.41  2005/11/29 19:55:43  lavr
 * When requested to close, reopen standard fds to /dev/null instead
 *
 * Revision 1.40  2005/11/26 03:20:59  lavr
 * Close all open handles on CPipe::Close()
 *
 * Revision 1.39  2005/06/28 16:26:57  lavr
 * Call CONNECT_InitInternal() for auto-magic init in ctors
 *
 * Revision 1.38  2005/04/20 19:13:59  lavr
 * +<assert.h>
 *
 * Revision 1.37  2004/05/17 20:58:13  gorelenk
 * Added include of PCH ncbi_pch.hpp
 *
 * Revision 1.36  2003/12/04 16:28:52  ivanov
 * CPipeHandle::Close(): added handling new create flags f*OnClose.
 * Added GetProcessHandle(). Throw/catch string exceptions instead char*.
 *
 * Revision 1.35  2003/11/14 13:06:01  ucko
 * +<stdio.h> (still needed by source...)
 *
 * Revision 1.34  2003/11/04 03:11:51  lavr
 * x_GetHandle(): default switch case added [for GCC2.95.2 to be happy]
 *
 * Revision 1.33  2003/10/24 16:52:38  lavr
 * Check RW bits before E bits in select()
 *
 * Revision 1.32  2003/09/25 04:40:42  lavr
 * Fixed uninitted member in ctor; few more minor changes
 *
 * Revision 1.31  2003/09/23 21:08:37  lavr
 * PipeStreambuf and special stream removed: now all in ncbi_conn_stream.cpp
 *
 * Revision 1.30  2003/09/16 13:42:36  ivanov
 * Added deleting OS-specific pipe handle in the destructor
 *
 * Revision 1.29  2003/09/09 19:30:33  ivanov
 * Fixed I/O timeout handling in the UNIX CPipeHandle. Cleanup code.
 * Comments and messages changes.
 *
 * Revision 1.28  2003/09/04 13:55:47  kans
 * added include sys/time.h to fix Mac compiler error
 *
 * Revision 1.27  2003/09/03 21:37:05  ivanov
 * Removed Linux ESPIPE workaround
 *
 * Revision 1.26  2003/09/03 20:53:02  ivanov
 * UNIX CPipeHandle::Read(): Added workaround for Linux -- read() returns
 * ESPIPE if a second pipe end is closed.
 * UNIX CPipeHandle::Close(): Implemented close timeout handling.
 *
 * Revision 1.25  2003/09/03 14:35:59  ivanov
 * Fixed previous accidentally commited log message
 *
 * Revision 1.24  2003/09/03 14:29:58  ivanov
 * Set r/w status to eIO_Success in the CPipe::Open()
 *
 * Revision 1.23  2003/09/02 20:32:34  ivanov
 * Moved ncbipipe to CONNECT library from CORELIB.
 * Rewritten CPipe class using I/O timeouts.
 *
 * Revision 1.22  2003/06/18 21:21:48  vakatov
 * Formal code formatting
 *
 * Revision 1.21  2003/06/18 18:59:33  rsmith
 * put in 0,1,2 for fileno(stdin/out/err) for library that does not have
 * fileno().
 *
 * Revision 1.20  2003/05/19 21:21:38  vakatov
 * Get rid of unnecessary unreached statement
 *
 * Revision 1.19  2003/05/08 20:13:35  ivanov
 * Cleanup #include <util/...>
 *
 * Revision 1.18  2003/04/23 20:49:21  ivanov
 * Entirely rewritten the Unix version of the CPipeHandle.
 * Added CPipe/CPipeHandle::CloseHandle().
 * Added flushing a standart output streams before it redirecting.
 *
 * Revision 1.17  2003/04/04 16:02:38  lavr
 * Lines wrapped at 79th column; some minor reformatting
 *
 * Revision 1.16  2003/04/03 14:15:48  rsmith
 * combine pp symbols NCBI_COMPILER_METROWERKS & _MSL_USING_MW_C_HEADERS
 * into NCBI_COMPILER_MW_MSL
 *
 * Revision 1.15  2003/04/02 16:22:34  rsmith
 * clean up metrowerks ifdefs.
 *
 * Revision 1.14  2003/04/02 13:29:53  rsmith
 * include ncbi_mslextras.h when compiling with MSL libs in Codewarrior.
 *
 * Revision 1.13  2003/03/10 18:57:08  kuznets
 * iterate->ITERATE
 *
 * Revision 1.12  2003/03/07 16:19:39  ivanov
 * MSWin CPipeHandle::Close() -- Handling GetExitCodeProcess() returns
 * value 259 (STILL_ACTIVE)
 *
 * Revision 1.11  2003/03/06 21:12:10  ivanov
 * Formal comments rearrangement
 *
 * Revision 1.10  2003/03/03 14:47:20  dicuccio
 * Remplemented CPipe using private platform specific classes.  Reimplemented
 * Win32 pipes using CreatePipe() / CreateProcess() - enabled CPipe in windows
 * subsystem
 *
 * Revision 1.9  2002/09/05 18:38:07  vakatov
 * Formal code rearrangement to get rid of comp.warnings;  some nice-ification
 *
 * Revision 1.8  2002/07/15 18:17:24  gouriano
 * renamed CNcbiException and its descendents
 *
 * Revision 1.7  2002/07/11 14:18:27  gouriano
 * exceptions replaced by CNcbiException-type ones
 *
 * Revision 1.6  2002/07/02 16:22:25  ivanov
 * Added closing file descriptors of the local end of the pipe in Close()
 *
 * Revision 1.5  2002/06/12 14:20:44  ivanov
 * Fixed some bugs in CPipeStreambuf class.
 *
 * Revision 1.4  2002/06/12 13:48:56  ivanov
 * Fixed contradiction in types of constructor CPipeStreambuf and its
 * realization
 *
 * Revision 1.3  2002/06/11 19:25:35  ivanov
 * Added class CPipeIOStream, CPipeStreambuf
 *
 * Revision 1.2  2002/06/10 18:35:27  ivanov
 * Changed argument's type of a running child program from char*[]
 * to vector<string>
 *
 * Revision 1.1  2002/06/10 16:59:39  ivanov
 * Initial revision
 *
 * ===========================================================================
 */
