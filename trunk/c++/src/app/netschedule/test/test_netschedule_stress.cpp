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
 * File Description:  NetSchedule stress test (used for performance tuning)
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbireg.hpp>
#include <corelib/ncbi_system.hpp>
#include <corelib/ncbimisc.hpp>

#include <connect/netschedule_client.hpp>
#include <connect/ncbi_socket.hpp>
#include <connect/ncbi_core_cxx.hpp>
#include <connect/ncbi_types.h>


USING_NCBI_SCOPE;

    
///////////////////////////////////////////////////////////////////////


/// Test application
///
/// @internal
///
class CTestNetScheduleStress : public CNcbiApplication
{
public:
    void Init(void);
    int Run(void);
};



void CTestNetScheduleStress::Init(void)
{
    CONNECT_Init();
    SetDiagPostFlag(eDPF_Trace);
    SetDiagPostLevel(eDiag_Info);
    
    // Setup command line arguments and parameters

    // Create command-line argument descriptions class
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // Specify USAGE context
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "NetSchedule stress test");
    
    arg_desc->AddPositional("hostname", 
                            "NetSchedule host name.", 
                            CArgDescriptions::eString);

    arg_desc->AddPositional("port",
                            "Port number.", 
                            CArgDescriptions::eInteger);

    arg_desc->AddPositional("queue", 
                            "NetSchedule queue name (like: noname).",
                            CArgDescriptions::eString);


    arg_desc->AddOptionalKey("jcount", 
                             "jcount",
                             "Number of jobs to submit",
                             CArgDescriptions::eInteger);


    
    // Setup arg.descriptions for this application
    SetupArgDescriptions(arg_desc.release());
}


int CTestNetScheduleStress::Run(void)
{
    CArgs args = GetArgs();
    const string&  host  = args["hostname"].AsString();
    unsigned short port = args["port"].AsInteger();
    const string&  queue_name = args["queue"].AsString();  

    unsigned jcount = 10000;
    if (args["jcount"]) {
        jcount = args["jcount"].AsInteger();
    }
    CNetScheduleClient::EJobStatus status;
    CNetScheduleClient cl(host, port, "client_test", queue_name);
    cl.SetRequestRateControl(false);

    const string input = "Hello " + queue_name;

    string job_key = cl.SubmitJob(input);
    NcbiCout << job_key << NcbiEndl;

    
    
    vector<string> jobs;
    jobs.reserve(jcount);

    {{
    CStopWatch sw(true);

    NcbiCout << "Submit " << jcount << " jobs..." << NcbiEndl;

    for (unsigned i = 0; i < jcount; ++i) {
        job_key = cl.SubmitJob(input);
        jobs.push_back(job_key);
        if (i % 1000 == 0) {
            NcbiCout << "." << flush;
        }
    }
    NcbiCout << NcbiEndl << "Done." << NcbiEndl;
    double elapsed = sw.Elapsed();
    double avg = elapsed / jcount;

    NcbiCout.setf(IOS_BASE::fixed, IOS_BASE::floatfield);
    NcbiCout << "Elapsed: "  << elapsed << " sec." << NcbiEndl;
    NcbiCout << "Avg time: " << avg << " sec." << NcbiEndl;
    }}

    
    
    
    
    NcbiCout << NcbiEndl << "Waiting..." << NcbiEndl;
    SleepMilliSec(40 * 1000);
    NcbiCout << NcbiEndl << "Ok." << NcbiEndl;

    {{
    NcbiCout << NcbiEndl 
             << "GetStatus " << jobs.size() << " jobs..." << NcbiEndl;

    CStopWatch sw(true);

    string output;
    NON_CONST_ITERATE(vector<string>, it, jobs) {
        const string& jk = *it;
        int ret_code;
        status = cl.GetStatus(jk, &ret_code, &output);
    }

    double elapsed = sw.Elapsed();
    if (jcount) {
        double avg = elapsed / (double)jcount;

        NcbiCout.setf(IOS_BASE::fixed, IOS_BASE::floatfield);
        NcbiCout << "Elapsed :"  << elapsed << " sec." << NcbiEndl;
        NcbiCout << "Avg time :" << avg << " sec." << NcbiEndl;
    }
    }}

    
    
    
    
    NcbiCout << NcbiEndl << "Waiting..." << NcbiEndl;
    SleepMilliSec(40 * 1000);
    NcbiCout << NcbiEndl << "Ok." << NcbiEndl;
    
    vector<string> jobs_returned;
    jobs_returned.reserve(jcount);

    {{
    NcbiCout << NcbiEndl << "Take-Return jobs..." << NcbiEndl;
    CStopWatch sw(true);
    string input;

    unsigned cnt = 0;
    for (; cnt < jcount/2; ++cnt) {
        bool job_exists = cl.WaitJob(&job_key, &input, 60, 9111);
//        bool job_exists = cl.GetJob(&job_key, &input);
        if (!job_exists)
            break;
        cl.ReturnJob(job_key);
        jobs_returned.push_back(job_key);
    }
    NcbiCout << "Returned " << cnt << " jobs." << NcbiEndl;

    double elapsed = sw.Elapsed();
    if (cnt) {
        double avg = elapsed / cnt;

        NcbiCout.setf(IOS_BASE::fixed, IOS_BASE::floatfield);
        NcbiCout << "Jobs processed: " << cnt << NcbiEndl;
        NcbiCout << "Elapsed: "  << elapsed << " sec." << NcbiEndl;
        NcbiCout << "Avg time: " << avg << " sec." << NcbiEndl;
    }

    
    
    
    NcbiCout << NcbiEndl << "Waiting..." << NcbiEndl;
    SleepMilliSec(40 * 1000);
    NcbiCout << NcbiEndl << "Ok." << NcbiEndl;

    }}

    vector<string> jobs_processed;
    jobs_processed.reserve(jcount);

    {{
    NcbiCout << NcbiEndl << "Processing..." << NcbiEndl;
    SleepMilliSec(8000);
    CStopWatch sw(true);
    
    unsigned cnt = 0;
    string input;
    string out = "DONE " + queue_name;

    for (; 1; ++cnt) {
        bool job_exists = cl.GetJob(&job_key, &input);
        if (!job_exists)
            break;

        jobs_processed.push_back(job_key);

        cl.PutResult(job_key, 0, out);
    }
    double elapsed = sw.Elapsed();

    if (cnt) {
        double avg = elapsed / cnt;

        NcbiCout.setf(IOS_BASE::fixed, IOS_BASE::floatfield);
        NcbiCout << "Jobs processed: " << cnt << NcbiEndl;
        NcbiCout << "Elapsed: "  << elapsed << " sec." << NcbiEndl;
        NcbiCout << "Avg time: " << avg << " sec." << NcbiEndl;
    }

    }}


    {{
    NcbiCout << NcbiEndl << "Check returned jobs..." << NcbiEndl;
    SleepMilliSec(5000);
    CStopWatch sw(true);

    string output;
    NON_CONST_ITERATE(vector<string>, it, jobs_returned) {
        const string& jk = *it;
        int ret_code;
        status = cl.GetStatus(jk, &ret_code, &output);
        switch (status) {
        case CNetScheduleClient::ePending:
            NcbiCout << "Job pending: " << jk << NcbiEndl;
            break;
        case CNetScheduleClient::eReturned:
            NcbiCout << "Job returned: " << jk << NcbiEndl;
            break;
        default:
            break;
        }
    }

    }}


    NcbiCout << NcbiEndl << "Done." << NcbiEndl;
    return 0;
}


int main(int argc, const char* argv[])
{
    return CTestNetScheduleStress().AppMain(argc, argv, 0, eDS_Default, 0);
}


/*
 * ===========================================================================
 * $Log$
 * Revision 1.4  2005/03/04 13:26:33  kuznets
 * Disabled request rate control for stress test
 *
 * Revision 1.3  2005/03/04 12:08:50  kuznets
 * Test for WaitJob
 *
 * Revision 1.2  2005/02/28 18:41:14  kuznets
 * test for ReturnJob()
 *
 * Revision 1.1  2005/02/17 17:20:12  kuznets
 * Initial revision
 *
 *
 * ===========================================================================
 */
