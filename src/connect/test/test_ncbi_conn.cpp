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
 * Author:  Anton Lavrentiev
 *
 * File Description:
 *   Run NCBI connectivity test
 *
 */

#include <ncbi_pch.hpp>
#include "../ncbi_priv.h"
#include <corelib/ncbiapp.hpp>
#include <connect/ncbi_connutil.h>
#include <connect/ncbi_conn_test.hpp>
#include <connect/ncbi_socket.hpp>
#include <iomanip>
#include <math.h>


BEGIN_NCBI_SCOPE


////////////////////////////////
// Test application
//

class CTest : public CNcbiApplication
{
public:
    CTest(void);

    virtual void Init(void);
    virtual int  Run (void);
};


CTest::CTest(void)
{
    // Set error posting and tracing on maximum
    SetDiagTrace(eDT_Enable);
    SetDiagPostAllFlags(eDPF_All | eDPF_OmitInfoSev);
    UnsetDiagPostFlag(eDPF_Line);
    UnsetDiagPostFlag(eDPF_File);
    UnsetDiagPostFlag(eDPF_Location);
    UnsetDiagPostFlag(eDPF_LongFilename);
    SetDiagPostLevel(eDiag_Trace);

    HideStdArgs(-1/*everything*/);
}


void CTest::Init(void)
{
    auto_ptr<CArgDescriptions> args(new CArgDescriptions);
    if (args->Exist ("h")) {
        args->Delete("h");
    }
    if (args->Exist ("xmlhelp")) {
        args->Delete("xmlhelp");
    }
    args->AddExtra(0/*no mandatory*/, 1/*single timeout argument allowed*/,
                   "Timeout", CArgDescriptions::eDouble);
    args->SetUsageContext(GetArguments().GetProgramBasename(),
                          "NCBI Connectivity Test Suite");
    SetupArgDescriptions(args.release());
}


int CTest::Run(void)
{
    IRWRegistry& reg = GetConfig();
    reg.Set(DEF_CONN_REG_SECTION, REG_CONN_DEBUG_PRINTOUT, "DATA");
    CONNECT_Init(&reg);

    const CArgs& args = GetArgs();
    size_t n = args.GetNExtra();
    double timeout;

    if (!n) {
        char val[40];
        ConnNetInfo_GetValue(0, REG_CONN_TIMEOUT, val, sizeof(val), "");
        if (!*val)
            timeout = DEF_CONN_TIMEOUT;
        else
            timeout = fabs(atof(val));
    } else
        timeout = fabs(args[1].AsDouble());

    NcbiCout << NcbiEndl << "NCBI Connectivity Test (Timeout = "
             << setprecision(6) << timeout << "s)" << NcbiEndl;

    STimeout tmo;
    tmo.sec  = (unsigned int)  timeout;
    tmo.usec = (unsigned int)((timeout - tmo.sec) * 1000000.0);

    CConnTest test(&tmo, &cout);

    CConnTest::EStage everything = CConnTest::eStatefulService;
    EIO_Status status = test.Execute(everything);

    NcbiCout << NcbiEndl;
    if (status != eIO_Success) {
        NcbiCout << "Check " << GetLogFile() << " for more information."
                 << NcbiEndl << "Please remember to make its contents"
            " available if contacting NCBI.";
    } else {
        _ASSERT(everything == CConnTest::eStatefulService);
        NcbiCout << "NCBI Connectivity Test PASSED!";
    }
    NcbiCout << NcbiEndl << NcbiEndl << NcbiFlush;

    CORE_SetLOG(0);
    return status == eIO_Success ? 0 : 1;
}


END_NCBI_SCOPE


int main(int argc, const char* argv[])
{
    USING_NCBI_SCOPE;

    // Empty log each app start
    CDiagContext::SetLogTruncate(true);
    SetLogFile("test_ncbi_conn.log");

    // Execute main application function
    return CTest().AppMain(argc, argv, 0, eDS_User, 0);
}
