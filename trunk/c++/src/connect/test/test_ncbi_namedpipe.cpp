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
 * Author:  Vladimir Ivanov, Anton Lavrentiev
 *
 * File Description:  Test program for CNamedPipe[Client|Server] classes
 *
 */

#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbi_system.hpp>

#include <connect/ncbi_namedpipe.hpp>
#include <test/test_assert.h>  // This header must go last


USING_NCBI_SCOPE;

// Test pipe name
#if defined(NCBI_OS_MSWIN)
    const string  kPipeName = "\\\\.\\pipe\\ncbi\\test_pipename";
#elif defined(NCBI_OS_UNIX)
    const string  kPipeName = "/var/tmp/.ncbi_test_pipename";
#endif

const size_t kNumSubBlobs = 10;
const size_t kSubBlobSize = 10*1024;
const size_t kBlobSize    = kNumSubBlobs * kSubBlobSize;


////////////////////////////////
// Auxiliary functions
//

// Reading from pipe
static EIO_Status s_ReadPipe(CNamedPipe& pipe, void* buf, size_t size,
                             size_t* n_read) 
{
    EIO_Status status = pipe.Read(buf, size, n_read);
    LOG_POST("Read from pipe "+ NStr::UIntToString(*n_read) + " bytes");
    return status;
}


// Writing to pipe
static EIO_Status s_WritePipe(CNamedPipe& pipe, const void* buf, size_t size,
                              size_t* n_written) 
{
    EIO_Status status = pipe.Write(buf, size, n_written);
    LOG_POST("Write to pipe "+ NStr::UIntToString(*n_written) + " bytes");
    return status;
}


/////////////////////////////////
// Named pipe client
//

static void Client(int num)
{
    LOG_POST("\nStart client " + NStr::IntToString(num) + "...\n");
    STimeout timeout = {1,0};

    CNamedPipeClient pipe;
    assert(pipe.IsClientSide());
    assert(pipe.SetTimeout(eIO_Open,  &timeout) == eIO_Success);
    assert(pipe.SetTimeout(eIO_Read,  &timeout) == eIO_Success);
    assert(pipe.SetTimeout(eIO_Write, &timeout) == eIO_Success);

    assert(pipe.Open(kPipeName, CNamedPipe::kDefaultTimeout, kSubBlobSize)
           == eIO_Success);

    char buf[kSubBlobSize];
    size_t n_read    = 0;
    size_t n_written = 0;

    // "Hello" test
    {{
        assert(s_WritePipe(pipe, "Hello", 5, &n_written) == eIO_Success);
        assert(n_written == 5);

        assert(s_ReadPipe(pipe, buf, 2, &n_read) == eIO_Success);
        assert(n_read == 2);
        assert(memcmp(buf, "OK", 2) == 0);

    }}
    // Big binary blob test
    {{
        // Send a very big binary blob
        size_t i;
        unsigned char* blob = (unsigned char*) malloc(kBlobSize);
        for (i = 0; i < kBlobSize; blob[i] = (unsigned char) i, i++); 
        for (i = 0; i < kNumSubBlobs; i++) {
            assert(s_WritePipe(pipe, blob + i*kSubBlobSize, kSubBlobSize,
                               &n_written) == eIO_Success);
            assert(n_written == kSubBlobSize);
        }
        // Receive back a very big binary blob
        memset(blob, 0, kBlobSize);
        for (i = 0;  i < kNumSubBlobs; i++) {
            assert(s_ReadPipe(pipe, blob + i*kSubBlobSize, kSubBlobSize,
                              &n_read) == eIO_Success);
            assert(n_read == kSubBlobSize);
        }
        // Check its content
        for (i = 0; i < kBlobSize; i++) {
            assert(blob[i] == (unsigned char)i);
        }
        free(blob);
        LOG_POST("Blob test is OK...");
    }}
}


/////////////////////////////////
// Named pipe server
//

static void Server(void)
{
    LOG_POST("\nStart server...\n");

    char buf[kSubBlobSize];
    size_t   n_read    = 0;
    size_t   n_written = 0;

    STimeout timeout   = {30,0};
    CNamedPipeServer pipe(kPipeName, &timeout, kSubBlobSize + 512);

    assert(pipe.IsServerSide());
    assert(pipe.SetTimeout(eIO_Read,  &timeout) == eIO_Success);
    assert(pipe.SetTimeout(eIO_Write, &timeout) == eIO_Success);

    for (;;) {
        LOG_POST("Listening pipe...");

        EIO_Status status = pipe.Listen();
        switch (status) {
        case eIO_Success:
            LOG_POST("Client is connected...");

            // "Hello" test
            {{
                assert(s_ReadPipe(pipe, buf, 5 , &n_read) == eIO_Success);
                assert(n_read == 5);
                assert(memcmp(buf, "Hello", 5) == 0);

                assert(s_WritePipe(pipe, "OK", 2, &n_written) == eIO_Success);
                assert(n_written == 2);
            }}

            // Big binary blob test
            {{
                // Receive a very big binary blob
                size_t i;
                unsigned char* blob = (unsigned char*) malloc(kBlobSize);

                for (i = 0;  i < kNumSubBlobs; i++) {
                    assert(s_ReadPipe(pipe, blob + i*kSubBlobSize,
                                      kSubBlobSize, &n_read) == eIO_Success);
                    assert(n_read == kSubBlobSize);
                }
 
                // Check its content
                for (i = 0; i < kBlobSize; i++) {
                    assert(blob[i] == (unsigned char)i);
                }
                // Write back a received big blob
                for (i = 0; i < kNumSubBlobs; i++) {
                    assert(s_WritePipe(pipe, blob + i*kSubBlobSize,
                                       kSubBlobSize, &n_written)
                           == eIO_Success);
                    assert(n_written == kSubBlobSize);
                }
                memset(blob, 0, kBlobSize);
                free(blob);
                LOG_POST("Blob test is OK...");
            }}
            LOG_POST("Disconnect client...");
            assert(pipe.Disconnect() == eIO_Success);
            break;

        case eIO_Timeout:
            LOG_POST("Timeout detected...");
            break;

        default:
            _TROUBLE;
        }
    }
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
                              "Test named pipes API");

    // Describe the expected command-line arguments
    arg_desc->AddPositional 
        ("mode", "Test mode",
         CArgDescriptions::eString);
    arg_desc->SetConstraint
        ("mode", &(*new CArgAllow_Strings, "client", "server"));

    // Setup arg.descriptions for this application
    SetupArgDescriptions(arg_desc.release());
}


int CTest::Run(void)
{
    CArgs args = GetArgs();

    if (args["mode"].AsString() == "client") {
        SetDiagPostPrefix("Client");
        for (int i=1; i<=3; i++) {
            Client(i);
        }
    }
    else if (args["mode"].AsString() == "server") {
        SetDiagPostPrefix("Server");
        Server();
    }
    else {
        _TROUBLE;
    }
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
 * Revision 1.2  2003/08/20 14:24:06  ivanov
 * Replaced _TRACE with LOG_POST
 *
 * Revision 1.1  2003/08/18 19:23:07  ivanov
 * Initial revision
 *
 * ===========================================================================
 */
