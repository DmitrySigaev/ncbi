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
 * File Description:  Portable class for work with process pipes
 *
 */

#include <corelib/ncbistd.hpp>
#include <corelib/ncbiobj.hpp>
#include <stdio.h>
#include <vector>


/** @addtogroup Pipes
 *
 * @{
 */


BEGIN_NCBI_SCOPE


/////////////////////////////////////////////////////////////////////////////
// CPipeException - exceptions generated by CPipe


class NCBI_XNCBI_EXPORT CPipeException : public CCoreException
{
public:
    enum EErrCode {
        eBind,
        eUnbind,
        eNoInit
    };
    virtual const char* GetErrCodeString(void) const
    {
        switch (GetErrCode()) {
        case eBind:   return "eBind";
        case eUnbind: return "eUnbind";
        case eNoInit: return "eNoInit";
        default:    return CException::GetErrCodeString();
        }
    }
    NCBI_EXCEPTION_DEFAULT(CPipeException,CCoreException);
};

//////////////////////////////////////////////////////////////////////////////
//
// CPipe class
//

// Create a pipe between program and some child process.
// Program can read from stdin/stderr and write to stdin of the
// executed child process using pipe object functions Read/Write.

class CPipeHandle;

class NCBI_XNCBI_EXPORT CPipe
{
public:

    // Modes for standart I/O handles of a child process
    enum EMode {
        eText,
        eBinary,
        eDoNotUse,
        // Default modes
        eDefaultStdIn  = eText,
        eDefaultStdOut = eText,
        eDefaultStdErr = eDoNotUse
    };

    // Child I/O handles for reading from pipe
    enum EChildIOHandle {
        eStdOut,
        eStdErr
    };

    // Constructors.
    // Second constructor just calls Open(). Trows exceptions on errors.
    CPipe();
    CPipe(const char *cmdname, const vector<string>& args,
          const EMode mode_stdin  = eDefaultStdIn,
          const EMode mode_stdout = eDefaultStdOut,
          const EMode mode_stderr = eDefaultStdErr);


    // If pipe was created that destructor waits for new child process
    // and closes stream on associated pipe. Just calls Close() method.
    ~CPipe(void);

    // Function create a pipe and executes a command with the vector of 
    // arguments "args". The other end of the pipe is associated with the
    // spawned commands standard input or standard output (according open 
    // mode). If some pipe was opened that it will be closed.
    // The function throws on an error.
    void Open(const char *cmdname, const vector<string>& args,
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
                const EChildIOHandle from_handle = eStdOut) const;

    // Writes data to the pipe. 
    // Returns the number of bytes actually written, which may be less than 
    // count if an error occurs.
    size_t Write(const void *buffer, const size_t count) const;

private:
    CRef<CPipeHandle> m_PipeHandle;
};



//////////////////////////////////////////////////////////////////////////////
//
// CPipeIOStream class
//

// This is a class, derived from "std::iostream", does both input and 
// output, using the specified pipe. 


// Forward declaration
class CPipeStreambuf;

// Default buffer size for pipe iostream
const streamsize kPipeDefaultBufSize = 4096;


class NCBI_XNCBI_EXPORT CPipeIOStream : public iostream
{
public:
    // 'ctors
    CPipeIOStream(const CPipe& pipe, 
                  streamsize buf_size = kPipeDefaultBufSize);
    virtual ~CPipeIOStream(void);

    // Set handle of the child process for reading by default (eStdOut/eStdErr)
    void SetReadHandle(const CPipe::EChildIOHandle handle) const;

private:
    // Stream buffer for pipe I/O 
    CPipeStreambuf* m_StreamBuf;
};
 

END_NCBI_SCOPE


/* @} */


/*
 * ===========================================================================
 * $Log$
 * Revision 1.9  2003/04/01 14:20:13  siyan
 * Added doxygen support
 *
 * Revision 1.8  2003/03/03 14:46:02  dicuccio
 * Reimplemented CPipe using separate private platform-specific implementations
 *
 * Revision 1.7  2002/12/18 22:53:21  dicuccio
 * Added export specifier for building DLLs in windows.  Added global list of
 * all such specifiers in mswin_exports.hpp, included through ncbistl.hpp
 *
 * Revision 1.6  2002/07/15 18:17:52  gouriano
 * renamed CNcbiException and its descendents
 *
 * Revision 1.5  2002/07/11 14:17:55  gouriano
 * exceptions replaced by CNcbiException-type ones
 *
 * Revision 1.4  2002/06/12 19:38:45  ucko
 * Remove a superfluous comma from the definition of EMode.
 *
 * Revision 1.3  2002/06/11 19:25:06  ivanov
 * Added class CPipeIOStream
 *
 * Revision 1.2  2002/06/10 18:35:13  ivanov
 * Changed argument's type of a running child program from char*[]
 * to vector<string>
 *
 * Revision 1.1  2002/06/10 16:57:04  ivanov
 * Initial revision
 *
 * ===========================================================================
 */

#endif  /* CORELIB__NCBIPIPE__HPP */
