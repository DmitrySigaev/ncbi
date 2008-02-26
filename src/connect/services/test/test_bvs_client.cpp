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
 * Authors:  Anatoliy Kuznetsov
 *
 * File Description:  NetBVStore client test
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbireg.hpp>
#include <corelib/ncbi_system.hpp>
#include <corelib/ncbimisc.hpp>

#include <connect/services/netbvstore_client.hpp>
#include <connect/ncbi_socket.hpp>
#include <connect/ncbi_types.h>
#include <connect/ncbi_core_cxx.hpp>
#include <common/test_assert.h>  /* This header must go last */


USING_NCBI_SCOPE;


///////////////////////////////////////////////////////////////////////


/// Test application for network ICache client
///
/// @internal
///
class CTestBVSClient : public CNcbiApplication
{
public:
    void Init(void);
    int Run(void);
};


void CTestBVSClient::Init(void)
{
    // Setup command line arguments and parameters

    // Create command-line argument descriptions class
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // Specify USAGE context
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "Network ICache client test");

    arg_desc->AddPositional("hostname",
                            "NetCache host name.", CArgDescriptions::eString);

    arg_desc->AddPositional("port",
                            "Port number.",
                            CArgDescriptions::eInteger);

    arg_desc->AddPositional("bvsname",
                            "BVStore name.",
                            CArgDescriptions::eString);


    // Setup arg.descriptions for this application
    SetupArgDescriptions(arg_desc.release());

//    CONNECT_Init(&GetConfig());

    SetDiagPostLevel(eDiag_Info);
    SetDiagTrace(eDT_Enable);
}



int CTestBVSClient::Run(void)
{
    CArgs args = GetArgs();
    const string& host  = args["hostname"].AsString();
    unsigned short port = args["port"].AsInteger();
    const string& bvsname  = args["bvsname"].AsString();

    CNetBVStoreClient cl(host, port, bvsname, "test_bvs");
    vector<char> buf;
    size_t buf_size;
    bool found = cl.ReadRealloc(10, buf, &buf_size, 0, 10000);

    assert(found);

    return 0;
}


int main(int argc, const char* argv[])
{
    return CTestBVSClient().AppMain(argc, argv, 0, eDS_Default, 0);
}
