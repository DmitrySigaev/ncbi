#ifndef CORELIB__NCBIPIPE__HPP
#define CORELIB__NCBIPIPE__HPP

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
 * File Description:  Portable class for work with process/file pipes
 *
 */

#include <corelib/ncbistd.hpp>
#include <corelib/ncbistre.hpp>
#include <stdio.h>

#if defined(NCBI_OS_MSWIN)
#  include <io.h>
#elif defined(NCBI_OS_UNIX)
#  include <unistd.h>
#elif defined(NCBI_OS_MAC)
#endif


BEGIN_NCBI_SCOPE


//////////////////////////////////////////////////////////////////////////////
//
// CPipe class
//

// Create a pipe between program and some child process.
// Program can read from stdin/stderr and write to stdin of the
// executed child process using pipe object functions Read/Write.

class CPipe
{
public:
    // The exception specific for this class:
    class CException : public runtime_error {
    public:
        CException(const string& message) : runtime_error(message) {}
    };

    // Modes for standart I/O handles of a child process
    enum EMode {
        eText,
        eBinary,
        eDoNotUse,
        // Default modes
        eDefaultStdIn  = eText,
        eDefaultStdOut = eText,
        eDefaultStdErr = eDoNotUse,
    };

    // Child I/O handles for reading from pipe
    enum EChildIOHandle {
        eStdOut,
        eStdErr
    };

    // Constructors.
    // Second constructor just calls Open(). Trows exceptions on errors.
    CPipe();
    CPipe(const char *cmdname, const char *const *args = 0,
           const EMode mode_stdin  = eDefaultStdIn,
           const EMode mode_stdout = eDefaultStdOut,
           const EMode mode_stderr = eDefaultStdErr);


    // If pipe was created that destructor waits for new child process
    // and closes stream on associated pipe. Just calls Close() method.
    ~CPipe(void);

    // Function create a pipe and executes a command with the vector of 
    // arguments "args" (using CExec::SpawnVP(), starting index of 
    // arguments in array is 1 -- see CExec class).
    // The other end of the pipe is associated with the spawned commands
    // standard input or standard output (according open mode). 
    // If some pipe was opened that it will be closed.
    // The function throws on an error.
    void Open(const char *cmdname, const char *const *args = 0,
           const EMode mode_stdin  = eDefaultStdIn,
           const EMode mode_stdout = eDefaultStdOut,
           const EMode mode_stderr = eDefaultStdErr);

    // Waits for new spawned child process and closes associated pipe.
    // Return exit code of child process if it has terminate succesfully, 
    // or -1 if an error has occurs.
    int Close();

    // Reads data from the pipe. 
    // Returns the number of bytes actually read, which may be less than count 
    // if an error occurs or if the end of the pipe file stream is encountered 
    // before reaching count.
    size_t Read(void *buffer, const size_t count, 
                const EChildIOHandle from_handle = eStdOut);

    // Writes data to the pipe. 
    // Returns the number of bytes actually written, which may be less than 
    // count if an error occurs.
    size_t Write(const void *buffer, const size_t count);

private:
    // Initialization of the class members
    void Init();

private:
    // I/O handles of the child process for:
    int m_StdIn;    // write to the shild stdin
    int m_StdOut;   // read from the child stdout
    int m_StdErr;   // read from the child stderr

    // Pid of running child process
    int m_Pid;
};


END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.1  2002/06/10 16:57:04  ivanov
 * Initial revision
 *
 * ===========================================================================
 */

#endif  /* CORELIB__NCBIPIPE__HPP */
