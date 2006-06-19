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
 *   Government have not placed any restriction on its use or reproduction.
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
 * Authors:  Maxim Didenko
 *
 * File Description:  NetSchedule grid client sample
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbimisc.hpp>
#include <corelib/ncbi_system.hpp>

#include <connect/services/grid_client.hpp>
#include <connect/services/grid_client_app.hpp>

#include <connect/services/blob_storage_netcache.hpp>

#include <algorithm>

USING_NCBI_SCOPE;

class CGridClientSampleApp : public CGridClientApp
{
public:

    virtual void Init(void);
    virtual int Run(void);
    virtual string GetProgramVersion(void) const
    {
        // Next formats are valid and supported:
        //   ProgramName 1.2.3
        //   ProgramName version 1.2.3
        //   ProgramName v. 1.2.3
        //   ProgramName ver. 1.2.3

        return "SampleNodeClient version 1.0.1";
    }

};

void CGridClientSampleApp::Init(void)
{
    // hack!!! It needs to be removed when we know how to deal with unresolved
    // symbols in plugins.
    BlobStorage_RegisterDriver_NetCache(); 
    // Don't forget to call it
    CGridClientApp::Init();

    // Create command-line argument descriptions class
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // Specify USAGE context
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "Grid client sample");

    arg_desc->AddOptionalKey("vsize", 
                             "vsize",
                             "Size of the test vector",
                             CArgDescriptions::eInteger);

    arg_desc->AddOptionalKey("jobs", 
                             "jobs",
                             "Number of jobs to submit",
                             CArgDescriptions::eInteger);

    // Setup arg.descriptions for this application
    SetupArgDescriptions(arg_desc.release());
}

int CGridClientSampleApp::Run(void)
{
    CArgs args = GetArgs();

    int vsize = 100000;
    if (args["vsize"]) {
        vsize = args["vsize"].AsInteger();
    }

    int jobs_number = 10;
    if (args["jobs"]) {
        jobs_number = args["jobs"].AsInteger();
    }

    vector<string> job_keys;

    NcbiCout << "Submit a job..." << jobs_number << NcbiEndl;

    for (int i = 0; i < jobs_number; ++i) {
        // Get a job submitter
        CGridJobSubmitter& job_submitter = GetGridClient().GetJobSubmitter();

        // Get an ouptut stream
        CNcbiOstream& os = job_submitter.GetOStream();

        // Send jobs input data
        os << "doubles ";  // output_type - just a list of doubels
        os << vsize << ' ';
        srand( (unsigned)time( NULL ) );
        for (int j = 0; j < vsize; ++j) {
            double d = rand()*0.2;
            os << d << ' ';
        }
        
        // Submit a job
        job_keys.push_back(job_submitter.Submit());
    }
    NcbiCout << NcbiEndl << "Done." << NcbiEndl;
     
    NcbiCout << "Waiting for job " << job_keys.size() << "..." << NcbiEndl;

    unsigned int cnt = 0;
    while (1) {
        SleepMilliSec(100);

        vector<string> done_jobs;
        for(vector<string>::const_iterator it = job_keys.begin();
            it != job_keys.end(); ++it) {
            // Get a job status
            CGridJobStatus& job_status = GetGridClient().GetJobStatus(*it);
            CNetScheduleClient::EJobStatus status;
            status = job_status.GetStatus();

            // A job is done here
            if (status == CNetScheduleClient::eDone) {
                // Get an input stream
                CNcbiIstream& is = job_status.GetIStream();
                int count;
                
                // Get the result
                is >> count;
                vector<double> resvec;
                for (int i = 0; i < count; ++i) {
                    if (!is.good()) {
                        LOG_POST( "Input stream error. Index : " << i );
                        break;
                    }
                    double d;
                    is >> d;
                    resvec.push_back(d);
                }
                LOG_POST( "Job " << *it << " is done." );
                done_jobs.push_back(*it);
                break;
            }
            
            // A job has failed
            if (status == CNetScheduleClient::eFailed) {
                ERR_POST( "Job " << *it << " failed : " 
                          << job_status.GetErrorMessage() );
                done_jobs.push_back(*it);
                break;
            }

            // A job has been canceled
            if (status == CNetScheduleClient::eCanceled) {
                LOG_POST( "Job " << *it << " is canceled.");
                done_jobs.push_back(*it);
                break;
            }
        }            
        for(vector<string>::const_iterator i = done_jobs.begin();
            i != done_jobs.end(); ++i) {
            job_keys.erase(remove(job_keys.begin(),job_keys.end(), *i),job_keys.end());
        }
        if (job_keys.empty())
            break;
         
        // A job is still running
        if (++cnt % 1000 == 0) {
            NcbiCout << "Still waiting..." << NcbiEndl;
        }
    }
    return 0;
}


int main(int argc, const char* argv[])
{
    return CGridClientSampleApp().AppMain(argc, argv, 0, eDS_Default, "grid_client_sample.ini");
} 


/*
 * ===========================================================================
 * $Log$
 * Revision 1.10  2006/06/19 19:41:06  didenko
 * Spelling fix
 *
 * Revision 1.9  2006/03/15 21:59:54  ucko
 * +<algorithm> for remove().
 *
 * Revision 1.8  2006/03/15 17:39:33  didenko
 * Added jobs parameter
 *
 * Revision 1.7  2005/04/18 13:36:15  didenko
 * Changed program version
 *
 * Revision 1.6  2005/04/07 16:49:29  didenko
 * + Program Version checking
 *
 * Revision 1.5  2005/03/28 15:01:37  didenko
 * Added some comments
 *
 * Revision 1.4  2005/03/28 14:54:01  didenko
 * Added job cancelation check
 *
 * Revision 1.3  2005/03/25 16:29:38  didenko
 * Rewritten to use new Grid Client framework
 *
 * Revision 1.2  2005/03/24 15:35:35  didenko
 * Made it compile on Unixes
 *
 * Revision 1.1  2005/03/24 15:10:41  didenko
 * Added samples for Worker node framework
 *
 * ===========================================================================
 */
 
