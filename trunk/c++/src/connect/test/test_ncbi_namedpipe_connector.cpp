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
 * Author:  Denis Vakatov, Anton Lavrentiev, Vladimir Ivanov
 *
 * File Description:
 *   Standard test for the the NAMEDPIPE-based CONNECTOR
 *
 */

#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbi_system.hpp>

#include <connect/ncbi_namedpipe.hpp>
#include <connect/ncbi_namedpipe_connector.h>
#include <stdio.h>
#include "../ncbi_priv.h"
#include "ncbi_conntest.h"
#include "test_assert.h"  // This header must go last


USING_NCBI_SCOPE;


// test pipe name
#if defined(NCBI_OS_MSWIN)
    const char* kPipeName = "\\\\.\\pipe\\ncbi\\test_con_pipename";
#elif defined(NCBI_OS_UNIX)
    const char* kPipeName = "/var/tmp/.ncbi_test_con_pipename";
#endif

const size_t kBufferSize = 10*1024;


static void Client(STimeout timeout)
{
    CONNECTOR  connector;
    FILE*      log_file;
    char       buf[512];

    CORE_SetLOGFILE(stderr, 0/*false*/);

    log_file = fopen("test_namedpipe_con_client.log", "ab");
    assert(log_file);

    // Tests for NAMEDPIPE CONNECTOR
    sprintf(buf,
            "Starting the NAMEDPIPE CONNECTOR test...\n\n"
            "%s,  timeout = %u.%06u\n\n",
            kPipeName, timeout.sec, timeout.usec);
    NcbiCout << buf;

    connector = NAMEDPIPE_CreateConnector(kPipeName,
                                          CNamedPipe::kDefaultBufferSize);
    CONN_TestConnector(connector, &timeout, log_file, fTC_SingleBouncePrint);

    connector = NAMEDPIPE_CreateConnector(kPipeName, 0);
    CONN_TestConnector(connector, &timeout, log_file, fTC_SingleBounceCheck);

    connector = NAMEDPIPE_CreateConnector(kPipeName, kBufferSize);
    CONN_TestConnector(connector, &timeout, log_file, fTC_Everything);

    // Cleanup
    fclose(log_file);
}


// The server just bounce all incoming data back to the client
static void Server(STimeout timeout, int n_cycle)
{
    EIO_Status status;
    char       message[512];

    sprintf(message,
            "Starting the NAMEDPIPE CONNECTOR io bouncer ...\n\n"
            "%s,  timeout = %u.%06u, n_cycle=%u\n\n",
            kPipeName, timeout.sec, timeout.usec, n_cycle);
    _TRACE(message);

    // Create listening named pipe
    CNamedPipeServer pipe;
    assert(pipe.Create(kPipeName, &timeout, kBufferSize) == eIO_Success);
    assert(pipe.SetTimeout(eIO_Read,  &timeout) == eIO_Success);
    assert(pipe.SetTimeout(eIO_Write, &timeout) == eIO_Success);

    // Accept connections from clients and run test sessions
    while ( n_cycle-- ) {
        size_t  n_read, n_written;
        char    buf[kBufferSize];

        sprintf(message, "Server(n_cycle = %d)", n_cycle);
        _TRACE(message);

        // Listening pipe
        status = pipe.Listen();
        switch (status) {
        case eIO_Success:
            _TRACE("Client is connected...");

            // Bounce all incoming data back to the client
            while ((status = pipe.Read(buf, kBufferSize, &n_read))
                   == eIO_Success) {
                // Dump received data
                sprintf(message, "Read %u bytes:", n_read);
                _TRACE(message);
                NcbiCout.write(buf, n_read);
                assert(NcbiCout.good());

                // Write data back to the pipe
                size_t n_total = 0;
                while (n_total < n_read) {
                    status = pipe.Write(buf + n_total, n_read - n_total,
                                        &n_written);
                    if (status != eIO_Success) {
                        sprintf(message,
                                "Failed to write %u bytes, status = %s",
                                n_read - n_total, IO_StatusStr(status));
                        _TRACE(message);
                        break;
                    }
                    n_total += n_written;
                    sprintf(message,"Written %u, remains %u bytes",
                            n_written, n_read - n_total);
                    _TRACE(message);
                }
            }
            assert(status == eIO_Timeout  ||  status == eIO_Closed);

            // Disconnect client
            _TRACE("Disconnect client...");
            assert(pipe.Disconnect() == eIO_Success);
            break;

        case eIO_Timeout:
            _TRACE("Timeout detected...");
            break;

        default:
            _TROUBLE;
        }
    }
    // Close named pipe
    status = pipe.Close();
    assert(status == eIO_Success  ||  status == eIO_Closed);
}


////////////////////////////////
// Test application
//

class CTest : public CNcbiApplication
{
public:
    virtual void Init(void);
    virtual int  Run(void);
};


void CTest::Init(void)
{
    // Set error posting and tracing on maximum
    SetDiagTrace(eDT_Enable);
    SetDiagPostFlag(eDPF_All);
    UnsetDiagPostFlag(eDPF_Line);
    UnsetDiagPostFlag(eDPF_File);
    UnsetDiagPostFlag(eDPF_LongFilename);
    SetDiagPostLevel(eDiag_Info);

    // Create command-line argument descriptions class
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // Specify USAGE context
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "Test for named pipe connector");

    // Describe the expected command-line arguments
    arg_desc->AddPositional 
        ("mode", "Test mode",
         CArgDescriptions::eString);
    arg_desc->SetConstraint
        ("mode", &(*new CArgAllow_Strings, "client", "server"));

    arg_desc->AddOptionalPositional 
        ("timeout", "Input/output timeout",
         CArgDescriptions::eDouble);
    arg_desc->SetConstraint
        ("timeout", new CArgAllow_Doubles(0.0, 200.0));

    // Setup arg.descriptions for this application
    SetupArgDescriptions(arg_desc.release());
}


int CTest::Run(void)
{
    // Log and data log streams
    CORE_SetLOGFormatFlags(fLOG_None          | fLOG_Level   |
                           fLOG_OmitNoteLevel | fLOG_DateTime);

    CArgs args = GetArgs();
    STimeout timeout = {1, 123456};

    if (args["timeout"].HasValue()) {
        double tv = args["timeout"].AsDouble();
        timeout.sec  = (unsigned int) tv;
        timeout.usec = (unsigned int) ((tv - timeout.sec) * 1000000);
    }
    if (args["mode"].AsString() == "client") {
        SetDiagPostPrefix("Client");
        Client(timeout);
    }
    else if (args["mode"].AsString() == "server") {
        if (!args["timeout"].HasValue()) {
            timeout.sec = 5;
        }
        SetDiagPostPrefix("Server");
        Server(timeout, 10);
    }
    else {
        _TROUBLE;
    }
    CORE_SetLOG(0);
    return 0;
}


///////////////////////////////////
// APPLICATION OBJECT  and  MAIN
//

int main(int argc, const char* argv[])
{
    // Execute main application function
    return CTest().AppMain(argc, argv, 0, eDS_Default, 0);
}


/*
 * ===========================================================================
 * $Log$
 * Revision 1.1  2003/08/18 19:23:07  ivanov
 * Initial revision
 *
 * ===========================================================================
 */
