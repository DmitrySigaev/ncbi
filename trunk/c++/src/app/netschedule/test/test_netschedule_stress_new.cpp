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

#include <connect/services/netschedule_api.hpp>
#include <connect/ncbi_socket.hpp>
#include <connect/ncbi_core_cxx.hpp>
#include <connect/ncbi_types.h>

#include <connect/services/blob_storage_netcache.hpp>


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
                              "NetSchedule stress test prog='test 1.2'");
    
    arg_desc->AddKey("service", 
                     "service_name",
                     "NetSchedule service name (format: host:port or servcie_name).", 
                     CArgDescriptions::eString);

    arg_desc->AddKey("queue", 
                     "queue_name",
                     "NetSchedule queue name (like: noname).",
                     CArgDescriptions::eString);


    arg_desc->AddOptionalKey("jobs", 
                             "jobs",
                             "Number of jobs to submit",
                             CArgDescriptions::eInteger);

    arg_desc->AddDefaultKey("batch", "batch",
                            "Test batch submit",
                            CArgDescriptions::eBoolean,
                            "true",
                            0);

    arg_desc->AddDefaultKey("keepconn", "keepconn",
                            "Keep connection",
                            CArgDescriptions::eBoolean,
                            "false",
                            0);

    
    // Setup arg.descriptions for this application
    SetupArgDescriptions(arg_desc.release());
}

/*
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
*/
void TestBatchSubmit(const string& service,
                     const string& queue_name, unsigned jcount)
{
    CNetScheduleAPI cl(service, "stress_test", queue_name);
    cl.SetProgramVersion("test 1.0.0");
    cl.SetConnMode(INetServiceAPI::eKeepConnection);
   
    typedef vector<CNetScheduleJob> TJobs;
    TJobs jobs;

    for (unsigned i = 0; i < jcount; ++i) {
        CNetScheduleJob job("HELLO BSUBMIT", "affinity", CNetScheduleAPI::eExclusiveJob);
        job.tags.push_back(CNetScheduleAPI::TJobTag("job_grp", NStr::UIntToString(i/10)));
        jobs.push_back(job);
    }
    
    {{
    NcbiCout << "Submit " << jobs.size() << " jobs..." << NcbiEndl;

    CNetScheduleSubmitter submitter = cl.GetSubmitter();
    CStopWatch sw(CStopWatch::eStart);
    submitter.SubmitJobBatch(jobs);

    NcbiCout << NcbiEndl << "Done." << NcbiEndl;
    double elapsed = sw.Elapsed();
    double rate = jobs.size() / elapsed;

    NcbiCout.setf(IOS_BASE::fixed, IOS_BASE::floatfield);
    NcbiCout << "Elapsed: "  << elapsed << " sec." << NcbiEndl;
    NcbiCout << "Rate: "     << rate    << " jobs/sec." << NcbiEndl;
    }}

    ITERATE(TJobs, it, jobs) {
        _ASSERT(!it->job_id.empty());
    }

    NcbiCout << "Get Jobs with permanent connection... " << NcbiEndl;

    // Fetch it back

    //    cl.SetCommunicationTimeout().sec = 20;

    {{
    unsigned cnt = 0;
    CNetScheduleJob job("INPUT");
    job.output = "DONE";

    CStopWatch sw(CStopWatch::eStart);
    CNetScheduleExecuter executer = cl.GetExecuter();

    for (;1;++cnt) {
        bool job_exists = executer.PutResultGetJob(job, job);
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


class CSimpleSink : public CNetScheduleAdmin::ISink
{
public:
    CSimpleSink(CNcbiOstream& os) : m_Os(os) {}
    ~CSimpleSink() {}
    
    virtual CNcbiOstream& GetOstream(CNetSrvConnector& conn)
    {
        m_Os << conn.GetHost() << ":" << conn.GetPort() << endl;
        return m_Os;
    }
    virtual void EndOfData(CNetSrvConnector& conn)
    {
        m_Os << endl;
    }
private:
    CNcbiOstream& m_Os;
};

int CTestNetScheduleStress::Run(void)
{
    const CArgs& args = GetArgs();
    const string&  service  = args["service"].AsString();
    const string&  queue = args["queue"].AsString();
    bool batch = args["batch"].AsBoolean();
    bool keepconn = args["keepconn"].AsBoolean();

    unsigned jcount = 10000;
    if (args["jobs"]) {
        jcount = args["jobs"].AsInteger();
    }

    //    TestRunTimeout(host, port, queue_name);
//    TestNetwork(host, port, queue_name);

    if (batch) {
        TestBatchSubmit(service, queue, jcount);
        return 0;
    }

    CNetScheduleAPI::EJobStatus status;
    CNetScheduleAPI cl(service, "stress_test", queue);
    cl.SetProgramVersion("test wn 1.0.1");
    cl.SetConnMode(keepconn? INetServiceAPI::eKeepConnection 
                           : INetServiceAPI::eCloseConnection);

    {
        CNetScheduleAdmin admin = cl.GetAdmin();
        CSimpleSink sink(cout);
        admin.GetServerVersion(sink);
    }
    //        SleepSec(40);
    //        return 0;
    string input = "Hello " + queue;

    CNetScheduleSubmitter submitter = cl.GetSubmitter();
    CNetScheduleExecuter executer = cl.GetExecuter();

    CNetScheduleJob job(input);
    job.progress_msg = "pmsg";
    submitter.SubmitJob(job);
    NcbiCout << job.job_id << NcbiEndl;
    submitter.GetProgressMsg(job);
    _ASSERT(job.progress_msg == "pmsg");

    // test progress message
    job.progress_msg = "progress report message";
    executer.PutProgressMsg(job);

    string pmsg = job.progress_msg;
    job.progress_msg = "";
    submitter.GetProgressMsg(job);
    NcbiCout << pmsg << NcbiEndl;
    _ASSERT(pmsg == job.progress_msg);


    job.error_msg = "test error\r\nmessage";
    executer.PutFailure(job);
    status = cl.GetJobDetails(job);
    
    //    _ASSERT(status == CNetScheduleAPI::eFailed);
    //    NcbiCout << err_msg << NcbiEndl;
    //    _ASSERT(err_msg == err);

    //> ?????????? How should it really work??????????????
    if (status != CNetScheduleAPI::eFailed) {
        NcbiCerr << "Job " << job.job_id << " succeeded!" << NcbiEndl;
    } else {
        NcbiCout << job.error_msg << NcbiEndl;
        /*
        if (job.error_msg != err) {
            NcbiCerr << "Incorrect error message: " << job.error_msg << NcbiEndl;
        }
        */
    }
    //< ?????????? How should it really work??????????????

    CNetScheduleAdmin admin = cl.GetAdmin();
    
    admin.DropJob(job.job_id);
    status = executer.GetJobStatus(job.job_id);

    _ASSERT(status == CNetScheduleAPI::eJobNotFound);

    vector<string> jobs;
    jobs.reserve(jcount);

    {{
    CStopWatch sw(CStopWatch::eStart);

    NcbiCout << "Submit " << jcount << " jobs..." << NcbiEndl;

    for (unsigned i = 0; i < jcount; ++i) {
        CNetScheduleJob job(input);
        submitter.SubmitJob(job);
        jobs.push_back( job.job_id );
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
        status = executer.GetJobStatus(jk);
        //status = cl.GetStatus(jk, &ret_code, &output);
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
        CNetScheduleJob job;
        bool job_exists = executer.WaitJob(job, 60, 9111);
//        bool job_exists = cl.GetJob(&job_key, &input);
        if (!job_exists)
            break;
        executer.ReturnJob(job.job_id);
        jobs_returned.push_back(job.job_id);
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

    for (; 1; ++cnt) {
        CNetScheduleJob job;
        bool job_exists = executer.GetJob(job);
        if (!job_exists)
            break;

        jobs_processed.push_back(job.job_id);

        job.output = "DONE " + queue;
        executer.PutResult(job);
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

    NON_CONST_ITERATE(vector<string>, it, jobs_returned) {
        const string& jk = *it;
        status = submitter.GetJobStatus(jk);
        switch (status) {
        case CNetScheduleAPI::ePending:
            NcbiCout << "Job pending: " << jk << NcbiEndl;
            break;
        case CNetScheduleAPI::eReturned:
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
