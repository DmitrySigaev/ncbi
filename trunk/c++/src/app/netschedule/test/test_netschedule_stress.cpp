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

#include <connect/services/netschedule_client.hpp>
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
                              "NetSchedule stress test prog='test 1.0.1'");
    
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

    arg_desc->AddDefaultKey("batch", "batch",
                            "Test batch submit",
                            CArgDescriptions::eBoolean,
                            "false",
                            0);

    
    // Setup arg.descriptions for this application
    SetupArgDescriptions(arg_desc.release());
}


void TestRunTimeout(const string&  host, 
                    unsigned short port, 
                    const string&  queue)
{
    NcbiCout << "Run timeout test..." << NcbiEndl;

    CNetScheduleClient cl(host, port, "client_test", queue);
    cl.ActivateRequestRateControl(false);

    const string input = "Hello " + queue;
    vector<string> jobs;
    vector<string> jobs_run;
    string job_key;

    unsigned jcount = 100;

    NcbiCout << "Submit " << jcount << " jobs..." << NcbiEndl;

    for (unsigned i = 0; i < jcount; ++i) {
        job_key = cl.SubmitJob(input);
        jobs.push_back(job_key);
    }

    NcbiCout << "Take jobs..." << NcbiEndl;
    string input_str;
    for (unsigned i = 0; i < jcount; ++i) {
        bool job_exists = cl.GetJob(&job_key, &input_str);
        if (!job_exists)
            break;
        cl.SetRunTimeout(job_key, 10);
        jobs_run.push_back(job_key);
    }

    NcbiCout << NcbiEndl << "Waiting..." << NcbiEndl;
    SleepMilliSec(40 * 1000);
    NcbiCout << NcbiEndl << "Ok." << NcbiEndl;

    
    NcbiCout << "Check status " << jobs_run.size() << " jobs." << NcbiEndl;
    string output;
    NON_CONST_ITERATE(vector<string>, it, jobs_run) {
        const string& jk = *it;
        int ret_code;
        CNetScheduleClient::EJobStatus
            status = cl.GetStatus(jk, &ret_code, &output);
        switch (status) {
        case CNetScheduleClient::ePending:
            break; // job is back to pending, good
        default:
            NcbiCout << jk << " unexpected status:" << status << NcbiEndl;
        }
    } 


    NcbiCout <<  NcbiEndl << "Test end." << NcbiEndl;
}

void TestNetscheduleLB(const string&  queue)
{
    NcbiCout << "Load balancing client test" << NcbiEndl;

    CNetScheduleClient_LB cl("stress_test prog='test 1.0.0'", "NC_test", queue, 1, 50);
    cl.AddServiceAddress("didimo", 9100);
    cl.AddServiceAddress("icoremake2", 9100);

    string job_key;
    const string input = "Hello " + queue;

    unsigned jcount = 70;
    NcbiCout << "Submit " << jcount << " jobs..." << NcbiEndl;

    for (unsigned i = 0; i < jcount; ++i) {
        job_key = cl.SubmitJob(input);
        NcbiCout << job_key << " ";
    }
    NcbiCout << NcbiEndl << "Done." << NcbiEndl;


    NcbiCout << NcbiEndl << "Take jobs..." << NcbiEndl;
    string input_str;
    unsigned cnt = 0;
    for (; true; ++cnt) {
        bool job_exists = cl.WaitJob(&job_key, &input_str, 600, 9111);
        if (!job_exists)
            break;
        cl.SetRunTimeout(job_key, 5);
        NcbiCout << job_key << " ";
    }
    NcbiCout << "Jobs count " << cnt << NcbiEndl;
    NcbiCout << NcbiEndl << "Done." << NcbiEndl;


    NcbiCout <<  NcbiEndl << "Test end." << NcbiEndl;
}


void TestNetwork(const string host, unsigned port, const string& queue_name)
{
    CNetScheduleClient cl(host, port, "stress_test", queue_name);
    cl.SetProgramVersion("test 1.0.0");
    const int kIterCount = 400000;

    cl.SetConnMode(CNetScheduleClient::eKeepConnection);
    cl.ActivateRequestRateControl(false);
    cl.SetCommunicationTimeout().sec = 20;

    NcbiCout << "Testing network using ServerVersion command " <<  kIterCount<< " iterations..." << NcbiEndl;

    CStopWatch sw(CStopWatch::eStart);

    for (unsigned i = 0; i < kIterCount; ++i) {
        cl.ServerVersion();
    }
    NcbiCout << NcbiEndl << "Done." << NcbiEndl;
    double elapsed = sw.Elapsed();
    double rate = kIterCount / elapsed;

    NcbiCout.setf(IOS_BASE::fixed, IOS_BASE::floatfield);
    NcbiCout << "Commands processed: " << kIterCount         << NcbiEndl;
    NcbiCout << "Elapsed: "        << elapsed     << " sec."      << NcbiEndl;
    NcbiCout << "Rate: "           << rate        << " cmd/sec." << NcbiEndl;

}

void TestBatchSubmit(const string host, unsigned port,
                     const string& queue_name, unsigned jcount)
{
    CNetScheduleClient cl(host, port, "stress_test", queue_name);
    cl.SetProgramVersion("test 1.0.0");
    
    CNetScheduleClient::SJobBatch jobs;

    for (unsigned i = 0; i < jcount; ++i) {
        jobs.job_list.push_back(CNetScheduleClient::SBatchSubm("HELLO BSUBMIT"));
    }
    
    {{
    NcbiCout << "Submit " << jobs.job_list.size() << " jobs..." << NcbiEndl;

    CStopWatch sw(CStopWatch::eStart);
    cl.SubmitJobBatch(jobs);

    NcbiCout << NcbiEndl << "Done." << NcbiEndl;
    double elapsed = sw.Elapsed();
    double rate = jobs.job_list.size() / elapsed;

    NcbiCout.setf(IOS_BASE::fixed, IOS_BASE::floatfield);
    NcbiCout << "Elapsed: "  << elapsed << " sec." << NcbiEndl;
    NcbiCout << "Rate: "     << rate    << " jobs/sec." << NcbiEndl;
    }}

    ITERATE(CNetScheduleClient::TBatchSubmitJobList, it, jobs.job_list) {
        _ASSERT(it->job_id);
    }

    NcbiCout << "Get Jobs with permanent connection... " << NcbiEndl;

    // Fetch it back

    cl.SetConnMode(CNetScheduleClient::eKeepConnection);
    cl.ActivateRequestRateControl(false);
    cl.SetCommunicationTimeout().sec = 20;

    {{
    unsigned cnt = 0;
    string job_key, input;
    string out = "DONE";

    CStopWatch sw(CStopWatch::eStart);

    for (;1;++cnt) {
        bool job_exists = cl.PutResultGetJob(job_key, 0, out, &job_key, &input);
        if (!job_exists) {
            break;
        }
    }
    NcbiCout << NcbiEndl << "Done." << NcbiEndl;
    double elapsed = sw.Elapsed();
    double rate = cnt / elapsed;

    NcbiCout.setf(IOS_BASE::fixed, IOS_BASE::floatfield);
    NcbiCout << "Jobs processed: " << cnt         << NcbiEndl;
    NcbiCout << "Elapsed: "        << elapsed     << " sec."      << NcbiEndl;
    NcbiCout << "Rate: "           << rate        << " jobs/sec." << NcbiEndl;
    }}
}


int CTestNetScheduleStress::Run(void)
{
    CArgs args = GetArgs();
    const string&  host  = args["hostname"].AsString();
    unsigned short port = args["port"].AsInteger();
    const string&  queue_name = args["queue"].AsString();
    bool batch = args["batch"].AsBoolean();
    string output;
    int ret_code;

    unsigned jcount = 10000;
    if (args["jcount"]) {
        jcount = args["jcount"].AsInteger();
    }

    //    TestRunTimeout(host, port, queue_name);
//    TestNetscheduleLB(queue_name);

//    TestNetwork(host, port, queue_name);
    if (batch) {
        TestBatchSubmit(host, port, queue_name, jcount);
        return 0;
    }

    CNetScheduleClient::EJobStatus status;
    CNetScheduleClient cl(host, port, "client_test", queue_name);
    cl.ActivateRequestRateControl(false);
    cl.SetProgramVersion("test 1.0.0");

    const string input = "Hello " + queue_name;

    string job_key = cl.SubmitJob(input, "pmsg");
    NcbiCout << job_key << NcbiEndl;
    string pmsg = cl.GetProgressMsg(job_key);
    _ASSERT(pmsg == "pmsg");

    // test progress message
    string progress_msg = "progress report message";
    cl.PutProgressMsg(job_key, progress_msg);

    pmsg = cl.GetProgressMsg(job_key);
    NcbiCout << pmsg << NcbiEndl;
    _ASSERT(pmsg == progress_msg);


    string err = "test error\r\nmessage";
    cl.PutFailure(job_key, err);
    string err_msg;
    status = cl.GetStatus(job_key, &ret_code, &output, &err_msg);
    if (status != CNetScheduleClient::eFailed) {
        NcbiCerr << "Job " << job_key << " succeeded!" << NcbiEndl;
    } else {
        NcbiCout << err_msg << NcbiEndl;
        if (err_msg != err) {
            NcbiCerr << "Incorrect error message: " << err_msg << NcbiEndl;
        }
    }

    cl.DropJob(job_key);
    status = cl.GetStatus(job_key, &ret_code, &output);

    if (status != CNetScheduleClient::eJobNotFound) {
        NcbiCerr << "Job " << job_key << " still exists!" << NcbiEndl;
    } else {
        NcbiCout << "Job " << job_key << " has been deleted." << NcbiEndl;
    }


    vector<string> jobs;
    jobs.reserve(jcount);

    {{
    CStopWatch sw(CStopWatch::eStart);

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

    CStopWatch sw(CStopWatch::eStart);

    unsigned i = 0;
    NON_CONST_ITERATE(vector<string>, it, jobs) {
        const string& jk = *it;
        int ret_code;
        status = cl.GetStatus(jk, &ret_code, &output);
        if (i++ % 1000 == 0) {
            NcbiCout << "." << flush;
        }
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
    CStopWatch sw(CStopWatch::eStart);
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
    CStopWatch sw(CStopWatch::eStart);
    
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
    CStopWatch sw(CStopWatch::eStart);

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
