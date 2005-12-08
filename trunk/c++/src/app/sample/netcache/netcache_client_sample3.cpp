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
 * Authors:  Maxim Didenko, Vladimir Ivanov, Anatoliy Kuznetsov
 *
 * File Description:  NetCache Sample (uses CNetCacheNSStorage)
 *
 */

/// @file netcache_client_sample3.cpp
/// NetCache sample: 
///    illustrates client creattion and simple store/load scenario
///    using C++ compatible streams and ZIP compression.
///    In some cases compression can dramatically reduce 
///    network and storage overhead of NetCache.
///


#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbireg.hpp>
#include <corelib/ncbi_system.hpp>
#include <corelib/ncbimisc.hpp>
#include <util/compress/zlib.hpp>

#include <connect/services/netcache_client.hpp>
#include <connect/services/netcache_nsstorage_imp.hpp>

USING_NCBI_SCOPE;

    
///////////////////////////////////////////////////////////////////////


/// Sample application
///
/// @internal
///
class CSampleNetCacheClient : public CNcbiApplication
{
public:
    void Init(void);
    int Run(void);
};



void CSampleNetCacheClient::Init(void)
{
    SetDiagPostFlag(eDPF_Trace);
    SetDiagPostLevel(eDiag_Info);
    
    // Setup command line arguments and parameters

    // Create command-line argument descriptions class
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // Specify USAGE context
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "NetCache Client Sample");

    arg_desc->AddOptionalKey("service",
                             "service",
                             "LBSM service name",
                             CArgDescriptions::eString);

    arg_desc->AddOptionalKey("hostport",
                             "host_post",
                             "NetCache server network address (host:port)",
                             CArgDescriptions::eString);

    
    // Setup arg.descriptions for this application
    SetupArgDescriptions(arg_desc.release());
}


int CSampleNetCacheClient::Run(void)
{
    auto_ptr<CNetCacheClient> nc_client;

    const CArgs& args = GetArgs();

    if (args["service"]) {
        // create load balanced client
        string service_name = args["service"].AsString();
        nc_client.reset(
            new CNetCacheClient_LB("nc_client_sample1", service_name));
    } else {
        if (args["hostport"]) {
            // use direct network address
            string host_port = args["hostport"].AsString();
            string host, port_str;
            NStr::SplitInTwo(host_port, ":", host, port_str);
            if (host.empty()) {
                ERR_POST("Invalid net address: empty host name");
                return 1;
            }
            if (port_str.empty()) {
                ERR_POST("Invalid net address: empty port");
                return 1;
            }
            
            unsigned port = NStr::StringToInt(port_str);
            if (port == 0) {
                ERR_POST("Invalid net address: port is zero");
                return 1;
            }

            nc_client.reset(
                new CNetCacheClient(host, port, "nc_client_sample1"));


        } else {
            ERR_POST("Invalid network address. Use -service OR -hostport options.");
            return 1;
        }
    }




    const char test_data[] = "A quick brown fox, jumps over lazy dog.";

    // storage takes respnsibility of deleting NetCache client
    CNetCacheNSStorage storage( nc_client.release() );
    
    // Store the BLOB
    string key;
    CNcbiOstream& os = storage.CreateOStream(key);

    // initialize compression stream
    {{
        CCompressionOStream os_zip(os, new CZipStreamCompressor(),
                                CCompressionStream::fOwnWriter);
        os_zip << test_data;
    }}

    // Reset the storage so we can reuse it to get the data back from 
    // the NetCache
    storage.Reset();

    NcbiCout << key << NcbiEndl;

    SleepMilliSec(500);

    // Get the data back
    try {
        CNcbiIstream& is = storage.GetIStream(key);
        string res;
        {{
            // construct decompresion stream
            // decompression MUST match compression 
            // CZipStreamCompressor - CZipStreamDecompressor
            CCompressionIStream is_zip(is, new CZipStreamDecompressor(),
                                       CCompressionStream::fOwnReader);
            getline(is_zip, res);
        }}
        NcbiCout << res << NcbiEndl;
        
    } catch(CNetScheduleStorageException& ex) {
        ERR_POST(ex.what());
        return 1;
    }
    return 0;

}


int main(int argc, const char* argv[])
{
    return CSampleNetCacheClient().AppMain(argc, argv, 0, eDS_Default, 0);
}


/*
 * ===========================================================================
 * $Log$
 * Revision 1.2  2005/12/08 17:22:58  kuznets
 * Added sample3 to the build, fixed bugs, added comments
 *
 * Revision 1.1  2005/12/08 17:08:21  ivanov
 * Initial revision
 *
 * ===========================================================================
 */
