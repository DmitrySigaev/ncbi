/* $Id$
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
 * File Description:  Test program for the Pipe API
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbi_system.hpp>
#include <connect/ncbi_conn_stream.hpp>
#include <stdio.h>
#include <string.h>

#if defined(NCBI_OS_MSWIN)
#  include <io.h>
#elif defined(NCBI_OS_UNIX)
#  include <signal.h>
#  include <unistd.h>
#else
#   error "Pipe tests configured for Windows and Unix only."
#endif

#include <common/test_assert.h>  // This header must go last


USING_NCBI_SCOPE;


// Test exit code
const int    kTestResult = 99;

// Use oddly-sized buffers that cross internal block boundaries
const size_t kBufferSize = 1234;


////////////////////////////////
// Auxiliary functions
//

// Read from pipe (up to "size" bytes)
static EIO_Status s_ReadPipe(CPipe& pipe, void* buf, size_t size,
                             size_t* n_read)
{
    size_t     total = 0;
    EIO_Status status;
    
    do {
        size_t cnt;
        status = pipe.Read((char*) buf + total, size - total, &cnt);
        cerr << cnt << " byte(s) read from pipe:" << endl;
        if ( cnt ) {
            cerr.write((char*) buf + total, cnt);
            cerr << endl;
        }
        total += cnt;
    } while (status == eIO_Success  &&  total < size);    
    
    *n_read = total;
    cerr << "Total read from pipe: " << total << " byte(s) ["
         << IO_StatusStr(status) << ']' << endl;
    return status;
}


// Write to pipe
static EIO_Status s_WritePipe(CPipe& pipe, const void* buf, size_t size,
                              size_t* n_written) 
{
    size_t     total = 0;
    EIO_Status status;
    
    do {
        size_t cnt;
        status = pipe.Write((char*) buf + total, size - total, &cnt);
        cerr << cnt << " byte(s) written to pipe:" << endl;
        if ( cnt ) {
            cerr.write((char*) buf + total, cnt);
            cerr << endl;
        }
        total += cnt;
    } while (status == eIO_Success  &&  total < size);
    
    *n_written = total;
    cerr << "Total written to pipe: " << total << " byte(s) ["
         << IO_StatusStr(status) << ']' << endl;
    return status;
}


// Read from file stream
static string s_ReadLine(FILE* fs) 
{
    string str;
    for (;;) {
        char   buf[80];
        char*  res = fgets(buf, sizeof(buf)-1, fs);
        size_t len = res ? strlen(res) : 0;
        cerr << (int) len << " byte(s) read from file:" << endl;
        if (!len) {
            break;
        }
        cerr.write(res, len);
        cerr << endl;
        if (res[len - 1] == '\n') {
            str += string(res, len - 1);
            break;
        }
        str += string(res, len);
    }
    return str;
}


// Write to file stream
static void s_WriteLine(FILE* fs, const string& str)
{
    size_t      written = 0;
    size_t      size = str.size();
    const char* data = str.c_str();
    do {
        size_t cnt = fwrite(data + written, 1, size - written, fs);
        cerr << (int) cnt << " byte(s) written to file:" << endl;
        if (!cnt) {
            break;
        }
        cerr.write(data + written, cnt);
        cerr << endl;
        written += cnt;
    } while (written < size);
    if (written == size) {
        static const char eol[] = { '\n' };
        fwrite(eol, 1, 1, fs);
        fflush(fs);
    }
}


// Soak up from istream until EOF, dump into cerr
static void s_ReadStream(istream& ios)
{
    size_t total = 0;

    for (;;) {
        char   buf[kBufferSize];
        ios.read(buf, sizeof(buf));
        size_t cnt = ios.gcount();
        cerr << "Read from istream: " << cnt << " byte(s):" << endl;
        cerr.write(buf, cnt);
        if ( cnt ) {
            cerr << endl;
        }
        total += cnt;
        if (cnt == 0  &&  ios.eof()) {
            break;
        }
        ios.clear();
    }
    cerr << "Total read from istream " << total << " byte(s)." << endl;
}


////////////////////////////////
// Test application
//

class CTest : public CNcbiApplication
{
public:
    void Init(void);
    int Run(void);
};


void CTest::Init(void)
{
    // Set error posting and tracing on maximum
    SetDiagTrace(eDT_Enable);
    SetDiagPostFlag(eDPF_All);
    SetDiagPostLevel(eDiag_Info);
}


int CTest::Run(void)
{
    const string app = GetArguments().GetProgramName();

    string         cmd, str;
    vector<string> args;
    char           buf[kBufferSize];
    size_t         total;
    size_t         n_read;
    size_t         n_written;
    int            exitcode;
    EIO_Status     status;
    TProcessHandle handle;

    // Create pipe object
    CPipe pipe;

    STimeout io_timeout    = {2,0};
    STimeout close_timeout = {1,0};

    assert(pipe.SetTimeout(eIO_Read,  &io_timeout)    == eIO_Success);
    assert(pipe.SetTimeout(eIO_Write, &io_timeout)    == eIO_Success);
    assert(pipe.SetTimeout(eIO_Close, &close_timeout) == eIO_Success);


    // Pipe for reading (direct from pipe)
#if defined(NCBI_OS_UNIX)
    cmd = "ls";
    args.push_back("-l");
#elif defined (NCBI_OS_MSWIN)
    cmd = GetEnvironment().Get("COMSPEC");
    args.push_back("/c");
    args.push_back("dir *.*");
#endif

    assert(pipe.Open(cmd.c_str(), args, CPipe::fStdIn_Close) == eIO_Success);

    assert(s_WritePipe(pipe, buf, kBufferSize, &n_written) == eIO_Unknown);
    assert(n_written == 0);
    total = 0;
    do {
        status = s_ReadPipe(pipe, buf, kBufferSize, &n_read);
        total += n_read;
    } while (status == eIO_Success);
    assert(status == eIO_Closed);
    assert(total > 0);

    status = pipe.Close(&exitcode);
    cerr << "Command completed with code " << exitcode << " and status "
         << IO_StatusStr(status) << endl;
    assert(status == eIO_Success  &&  exitcode == 0);


    // Pipe for reading (iostream)
    CConn_PipeStream ios(cmd.c_str(), args, CPipe::fStdIn_Close);
    s_ReadStream(ios);

    status = ios.GetPipe().Close(&exitcode);
    cerr << "Command completed with code " << exitcode << " and status "
         << IO_StatusStr(status) << endl;
    assert(status == eIO_Success  &&  exitcode == 0);


    // Unidirectional pipe (direct to pipe)
    // NB: a race condition on child's cerr closure

    args.clear();
    args.push_back("1");

    assert(pipe.Open(app.c_str(), args, CPipe::fStdOut_Close) == eIO_Success);

    assert(s_ReadPipe(pipe, buf, kBufferSize, &n_read) == eIO_Unknown);
    assert(n_read == 0);
    str = "Child, are you ready?";
    assert(s_WritePipe(pipe, str.c_str(), str.length(),
                       &n_written) == eIO_Success);
    assert(n_written == str.length());
    assert(pipe.CloseHandle(CPipe::eStdIn) == eIO_Success);

    status = pipe.Close(&exitcode); 
    cerr << "Command completed with code " << exitcode << " and status "
         << IO_StatusStr(status) << endl;
    assert(status == eIO_Success  &&  exitcode == kTestResult);


    // Bidirectional pipe (direct from pipe)

    args.clear();
    args.push_back("2");

    assert(pipe.Open(app.c_str(), args) == eIO_Success);

    assert(s_ReadPipe(pipe, buf, kBufferSize, &n_read) == eIO_Timeout);
    assert(n_read == 0);
    str = "Child, are you ready again?\n";
    assert(s_WritePipe(pipe, str.c_str(), str.length(),
                       &n_written) == eIO_Success);
    assert(n_written == str.length());
    str = "Ok. Test 2 running.";
    assert(s_ReadPipe(pipe, buf, kBufferSize, &n_read) == eIO_Closed);
    assert(n_read >= str.length());  // do not count EOL in
    assert(memcmp(buf, str.c_str(), str.length()) == 0);
    assert(s_ReadPipe(pipe, buf, kBufferSize, &n_read) == eIO_Closed);
    assert(n_read == 0);

    status = pipe.Close(&exitcode); 
    cerr << "Command completed with code " << exitcode << " and status "
         << IO_StatusStr(status) << endl;
    assert(status == eIO_Success  &&  exitcode == kTestResult);

    assert(s_ReadPipe(pipe, buf, kBufferSize, &n_read) == eIO_Closed);
    assert(n_read == 0);
    assert(s_WritePipe(pipe, buf, kBufferSize, &n_written) == eIO_Closed);
    assert(n_written == 0);


    // Bidirectional pipe (iostream)

    args.clear();
    args.push_back("3");

    CConn_PipeStream ps(app.c_str(), args, CPipe::fStdErr_Open);

    cout << endl;
    for (int i = 5; i<=10; i++) {
        int value; 
        cout << "How much is " << i << "*" << i << "?\t";
        ps << i << endl;
        ps.flush();
        ps >> value;
        cout << value << endl;
        assert(ps.good());
        assert(value == i*i);
    }
    ps >> str;
    cout << str << endl;
    assert(str == "Done");
    ps.GetPipe().SetReadHandle(CPipe::eStdErr);
    ps >> str;
    assert(!str.empty());

    status = ps.GetPipe().Close(&exitcode); 
    cerr << "Command completed with code " << exitcode << " and status "
         << IO_StatusStr(status) << endl;
    assert(status == eIO_Success  &&  exitcode == kTestResult);


    // f*OnClose flags test

    args.clear();
    args.push_back("4");
    
    assert(pipe.Open(app.c_str(), args, CPipe::fKeepOnClose) == eIO_Success);

    handle = pipe.GetProcessHandle();
    assert(handle > 0);

    status = pipe.Close(&exitcode); 
    cerr << "Command completed with code " << exitcode << " and status "
         << IO_StatusStr(status) << endl;
    assert(status == eIO_Timeout  &&  exitcode == -1);
    {{
        CProcess process(handle, CProcess::eHandle);
        assert(process.IsAlive());
        assert(process.Kill());
        assert(!process.IsAlive());
    }}

    assert(pipe.Open(app.c_str(), args,
                     CPipe::fKillOnClose | CPipe::fNewGroup) == eIO_Success);
    handle = pipe.GetProcessHandle();
    assert(handle > 0);

    status = pipe.Close(&exitcode); 
    cerr << "Command completed with code " << exitcode << " and status "
         << IO_StatusStr(status) << endl;
    assert(status == eIO_Success  &&  exitcode == -1);
    {{
        CProcess process(handle, CProcess::eHandle);
        assert(!process.IsAlive());
    }}

    assert(pipe.Open(app.c_str(), args, CPipe::fKeepOnClose) == eIO_Success);

    handle = pipe.GetProcessHandle();
    assert(handle > 0);

    status = pipe.Close(&exitcode); 
    cerr << "Command completed with code " << exitcode << " and status "
         << IO_StatusStr(status) << endl;
    assert(status == eIO_Timeout  &&  exitcode == -1);
    {{
        CProcess process(handle, CProcess::eHandle);
        assert(process.IsAlive());
        CProcess::CExitInfo exitinfo;
        int exitcode = process.Wait(6000/*6 sec*/, &exitinfo);
        cerr << "Command completed with code " << exitcode << ", state "
             << (!exitinfo.IsPresent() ? "Inexistent" :
                 exitinfo.IsExited()   ? "Terminated" :
                 exitinfo.IsAlive()    ? "Alive"      :
                 exitinfo.IsSignaled() ? "Signaled"   : "Unknown") << ": "
             << (exitinfo.IsPresent()
                 ? (exitinfo.IsSignaled() ? exitinfo.GetSignal()   :
                    exitinfo.IsExited()   ? exitinfo.GetExitCode() : -1)
                 : -1)
             << endl;
        assert(exitcode == kTestResult);
        assert(!process.IsAlive());
    }}


    // Done
    cout << "\nTEST completed successfully." << endl;

    return 0;
}


///////////////////////////////////
// APPLICATION OBJECT  and  MAIN
//

int main(int argc, const char* argv[])
{
    string command;

    // Check arguments
    if (argc > 2) {
        // Invalid arguments
        exit(1);
    }
    if (argc == 1) {
        // Execute main application function
        return CTest().AppMain(argc, argv, 0, eDS_Default, 0);
    }

    // Internal tests
    int test_num = NStr::StringToInt(argv[1]);

    switch ( test_num ) {
    // Spawned process for unidirectional test
    case 1:
    {
        // NB: cout closed in this test; cerr may close early (race)
#ifdef NCBI_OS_UNIX
        // Ignore pipe signals, convert them to errors
        ::signal(SIGPIPE, SIG_IGN);
#endif /*NCBI_OS_UNIX*/
        cerr << endl << "--- CPipe unidirectional test ---" << endl;
        command = s_ReadLine(stdin);
        _TRACE("read back >>" << command << "<<");
        assert(command == "Child, are you ready?");
        cerr << "Ok. Test 1 running." << endl;
        exit(kTestResult);
    }
    // Spawned process for bidirectional test (direct from pipe)
    case 2:
    {
        cerr << endl << "--- CPipe bidirectional test (pipe) ---" << endl;
        command = s_ReadLine(stdin);
        assert(command == "Child, are you ready again?");
        s_WriteLine(stdout, "Ok. Test 2 running.");
        exit(kTestResult);
    }
    // Spawned process for bidirectional test (iostream)
    case 3:
    {
        cerr << endl << "--- CPipe bidirectional test (iostream) ---" << endl;
        for (int i = 5; i <= 10; ++i) {
            int value;
            cin >> value;
            assert(value == i);
            cout << value * value << endl;
            cout.flush();
        }
        cout << "Done" << endl;
        exit(kTestResult);
    }
    // Test for fKeepOnClose && fKillOnClose flags
    case 4:
    {
        SleepSec(3);
        exit(kTestResult);
    }}

    return -1;
}
