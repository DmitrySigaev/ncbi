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
 * Author:  Aaron Ucko, NCBI
 *
 * File Description:
 *   Test for log rotation
 *
 */

#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbi_system.hpp>

#include <util/logrotate.hpp>

#include <stdlib.h>
#ifdef NCBI_OS_UNIX
#  include <signal.h>
#  include <unistd.h>
#endif

#include <test/test_assert.h> // must be last

USING_NCBI_SCOPE;


/////////////////////////////////////////////////////////////////////////////
//  CTestLogrotateApplication::


// defined here so SIGHUP can kick it
static auto_ptr<CRotatingLogStream> s_LogStream;

#ifdef NCBI_OS_UNIX
static volatile int s_Signal = 0;

extern "C" {
    static void s_SignalHandler(int signum)
    {
        s_Signal = signum;
    }
}
#endif

class CTestLogrotateApplication : public CNcbiApplication
{
    virtual void Init(void);
    virtual int  Run(void);
    virtual void Exit(void);

};


void CTestLogrotateApplication::Init(void)
{
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "Test of log rotation");

    arg_desc->AddDefaultKey
        ("limit", "N",
         "Approximate maximum length between rotations, in bytes",
         CArgDescriptions::eInteger, "16384");

    SetupArgDescriptions(arg_desc.release());
}


static unsigned int s_Rand(unsigned int limit)
{
    return (rand() >> 5) % limit;
}


int CTestLogrotateApplication::Run(void)
{
    CArgs  args = GetArgs();
    string me   = GetArguments().GetProgramBasename();
    s_LogStream.reset(new CRotatingLogStream(me + ".log",
                                             args["limit"].AsInteger()));
    SetDiagStream(s_LogStream.get());
    // enable verbose diagnostics for best effect
    SetDiagPostLevel(eDiag_Info);
    SetDiagTrace(eDT_Enable);
    SetDiagPostFlag(eDPF_All);
#ifdef NCBI_OS_UNIX
    {{
        struct sigaction sa;
        memset(&sa, 0, sizeof(sa));
        sa.sa_handler = s_SignalHandler;
        sa.sa_flags   = SA_RESTART;
        sigaction(SIGHUP, &sa, 0);
    }}
    me += '/' + NStr::IntToString(getpid());
#endif
    SetDiagPostPrefix(me.c_str());
    srand(time(0));

    for (;;) {
        static const char* messages[] = { "foo", "bar", "baz", "quux" };
        EDiagSev    severity = (EDiagSev)(s_Rand(eDiag_Fatal));
        ErrCode     errcode(rand(), rand());
        const char* message  = messages[s_Rand(4)];
        CNcbiDiag(__FILE__, __LINE__, severity) << errcode << message << Endm;
        SleepMilliSec(s_Rand(256));
#ifdef NCBI_OS_UNIX
        if (s_Signal) {
            LOG_POST("Received signal " << s_Signal << "; rotating logs");
            s_LogStream->Rotate();
            LOG_POST("Done rotating logs");
        }
        s_Signal = 0;
#endif
    }
    
    return 0;
}


/////////////////////////////////////////////////////////////////////////////
//  Cleanup


void CTestLogrotateApplication::Exit(void)
{
    SetDiagStream(0);
}


/////////////////////////////////////////////////////////////////////////////
//  MAIN


int main(int argc, const char* argv[])
{
    // Execute main application function
    return CTestLogrotateApplication().AppMain(argc, argv, 0, eDS_Default, 0);
}


/*
 * ===========================================================================
 * $Log$
 * Revision 1.1  2002/11/25 17:21:01  ucko
 * Add support for automatic log rotation (CRotatingLogStream)
 *
 *
 * ===========================================================================
 */
