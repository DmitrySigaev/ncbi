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
 * File Description:
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbistre.hpp>
#include <corelib/ncbimisc.hpp>
#include <corelib/ncbi_system.hpp>
#include <corelib/blob_storage.hpp>

#include <connect/services/grid_client.hpp>
#include <connect/services/grid_client_app.hpp>
#include <connect/services/remote_job.hpp>
#include <connect/services/blob_storage_netcache.hpp>


USING_NCBI_SCOPE;

class CNSSubmitRemoveJobApp : public CGridClientApp
{
public:
    CNSSubmitRemoveJobApp() : m_UsePermanentConnection(false) {}

    virtual void Init(void);
    virtual int Run(void);
    virtual string GetProgramVersion(void) const
    {
        return "CNSSubmitRemoveJobApp version 1.0.0";
    }

protected:

    void PrintJobInfo(const string& job_key);
    void ShowBlob(const string& blob_key);

    virtual bool UseProgressMessage() const { return false; }
    virtual bool UsePermanentConnection() const { return m_UsePermanentConnection; }
    virtual bool UseAutomaticCleanup() const { return false; }
private:
    bool m_UsePermanentConnection;
};

void CNSSubmitRemoveJobApp::Init(void)
{
    // hack!!! It needs to be removed when we know how to deal with unresolved
    // symbols in plugins.
    BlobStorage_RegisterDriver_NetCache(); 

    // Create command-line argument descriptions class
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // Specify USAGE context
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "Remote application jobs submitter");

    arg_desc->AddOptionalKey("q", "queue_name", "NetSchedule queue name", 
                     CArgDescriptions::eString);

    arg_desc->AddOptionalKey("ns", "service", 
                     "NetSchedule service addrress (service_name or host:port)", 
                     CArgDescriptions::eString);

    arg_desc->AddOptionalKey("nc", "service", 
                     "NetCache service addrress (service_name or host:port)", 
                     CArgDescriptions::eString);

    arg_desc->AddOptionalKey("jf",
                             "jobs_file",
                             "File with job descriptions",
                             CArgDescriptions::eString);

    arg_desc->AddOptionalKey("args", 
                             "cmd_args",
                             "Cmd args for the remote application",
                             CArgDescriptions::eString);

    arg_desc->AddOptionalKey("aff", 
                             "affinity",
                             "Affinity token",
                             CArgDescriptions::eString);

    arg_desc->AddOptionalKey("tfiles", 
                             "file_names",
                             "Files for transfer to the remote applicaion side",
                             CArgDescriptions::eString);

    arg_desc->AddOptionalKey("runtime", 
                             "time",
                             "job run timeout",
                             CArgDescriptions::eString);

    arg_desc->AddFlag("exclusive",
                      "Run job in the exclusive mode",
                      false);

    arg_desc->AddOptionalKey("jout", 
                             "file_names",
                             "A file the remote applicaion stdout",
                             CArgDescriptions::eString);

    arg_desc->AddOptionalKey("jerr", 
                             "file_names",
                             "A file the remote applicaion stderr",
                             CArgDescriptions::eString);

    arg_desc->AddOptionalKey("of", 
                             "file_names",
                             "Ouput files with job ids",
                             CArgDescriptions::eString);

    // Setup arg.descriptions for this application
    SetupArgDescriptions(arg_desc.release());
}

static string s_FindParam(const string& str, const string& par_name)
{
    SIZE_TYPE pos = 0;
    pos = NStr::FindNoCase(str, par_name);
    if (pos != NPOS) {
        SIZE_TYPE pos1 = pos + par_name.size();
        while (pos1 != NPOS) {
            pos1 = NStr::Find(str, "\"", pos1);
            if(str[pos1-1] != '\\') 
                break;
            ++pos1;
        }
        if (pos1 == NPOS)
            return "";
        return NStr::ParseEscapes(str.substr(pos+par_name.size(), 
                                             pos1 - pos - par_name.size()));
    }
    return "";
}

int CNSSubmitRemoveJobApp::Run(void)
{

    IRWRegistry& reg = GetConfig();

    const CArgs& args = GetArgs();
    reg.Set(kNetScheduleDriverName, "client_name", "ns_submit_remote_job");

    if (args["q"]) {
        string queue = args["q"].AsString();   
        reg.Set(kNetScheduleDriverName, "queue_name", queue);
    }
    string service, host, sport;
    if ( args["ns"]) {
        service = args["ns"].AsString();
        if (NStr::SplitInTwo(service, ":", host, sport)) {
            unsigned int port = NStr::StringToUInt(sport);
            reg.Set(kNetScheduleDriverName, "host", host);
            reg.Set(kNetScheduleDriverName, "port", sport);
            m_UsePermanentConnection = true;
        } else {
            reg.Set(kNetScheduleDriverName, "service", service);
            m_UsePermanentConnection = false;
        }
        reg.Set(kNetScheduleDriverName, "use_embedded_storage", "true");
    }

    reg.Set(kNetCacheDriverName, "client_name", "ns_submit_remote_job");

    if ( args["nc"]) {
        service = args["nc"].AsString();
        if (NStr::SplitInTwo(service, ":", host, sport)) {
            unsigned int port = NStr::StringToUInt(sport);
            reg.Set(kNetCacheDriverName, "host", host);
            reg.Set(kNetCacheDriverName, "port", sport);
        } else {
            reg.Set(kNetCacheDriverName, "service", service);
        }
    }

    // Don't forget to call it
    CGridClientApp::Init();

    CNcbiOstream* out = NULL;
    auto_ptr<CNcbiOstream> out_guard;
    if (args["of"]) {
        string fname = args["of"].AsString();
        if (fname == "-")
            out = &NcbiCout;
        else {
            out_guard.reset(new CNcbiOfstream(fname.c_str()));
            if (!out_guard->good())
                NCBI_THROW(CArgException, eInvalidArg,
                       "Could not create file \"" + fname + "\"");
            else
                out = out_guard.get();
        }
    }

    CBlobStorageFactory factory(reg);
    CRemoteAppRequest request(factory);

    if (args["args"]) {
        string cmd = args["args"].AsString();
        string affinity;
        if (args["aff"]) {
            affinity = args["aff"].AsString();
        }
        list<string> tfiles;
        if (args["tfiles"]) {
            NStr::Split(args["tfiles"].AsString(), ";", tfiles);
        }
        request.SetCmdLine(cmd);
        if (tfiles.size() != 0) {
            ITERATE(list<string>, s, tfiles) {
                request.AddFileForTransfer(*s);
            }
        }
        string jout, jerr;
        if (args["jout"]) 
            jout = args["jout"].AsString();

        if (args["jerr"]) 
            jerr = args["jerr"].AsString();
        
        if (!jout.empty() && !jerr.empty())
            request.SetStdOutErrFileNames(jout, jerr);

        if (args["runtime"]) {
            unsigned int rt = NStr::StringToUInt(args["runtime"].AsString());
            request.SetAppRunTimeout(rt);
        }
        if (args["exclusive"]) {
            request.RequestExclusiveMode();
        }


        CGridJobSubmiter& job_submiter = GetGridClient().GetJobSubmiter();
        request.Send(job_submiter.GetOStream());
        string job_key = job_submiter.Submit(affinity);
        if (out)
            *out << job_key << NcbiEndl;
        return 0;
    } if (args["jf"]) {
        string fname = args["jf"].AsString();
        CNcbiIfstream is(fname.c_str());
        if ( !is.good() )
            NCBI_THROW(CArgException, eInvalidArg,
                       "Could not read file \"" + fname + "\"");

        while ( !is.eof() && is.good() ) {
            string line;
            NcbiGetlineEOL(is, line);
            if( line.size() == 0 || line[0] == '#')
                continue;
            string cmd = s_FindParam(line, "args=\"");
            request.SetCmdLine(cmd);
            string affinity = s_FindParam(line, "aff=\"");
            string files = s_FindParam(line, "tfiles=\"");
            if (files.size() != 0) {
                list<string> tfiles;
                NStr::Split(files, ";", tfiles);
                if (tfiles.size() != 0) {
                    ITERATE(list<string>, s, tfiles) {
                        request.AddFileForTransfer(*s);
                    }
                }
            }
            string jout = s_FindParam(line, "jout=\"");
            string jerr = s_FindParam(line, "jerr=\"");

            if (!jout.empty() && !jerr.empty())
                request.SetStdOutErrFileNames(jout, jerr);

            string srt = s_FindParam(line, "runtime=\"");
            if (!srt.empty()) {
                unsigned int rt = NStr::StringToUInt(srt);
                request.SetAppRunTimeout(rt);
            }
            srt = s_FindParam(line, "exclusive=");
            if (!srt.empty()) {
                request.RequestExclusiveMode();
            }
            
            CGridJobSubmiter& job_submiter = GetGridClient().GetJobSubmiter();
            request.Send(job_submiter.GetOStream());
            string job_key = job_submiter.Submit(affinity);
            if (out)
                *out << job_key << NcbiEndl;
        }
            
        return 0;
    }
    NCBI_THROW(CArgException, eInvalidArg,
               "Neither agrs nor jf arguments were specified");

}
int main(int argc, const char* argv[])
{
    return CNSSubmitRemoveJobApp().AppMain(argc, argv);
} 


/*
 * ===========================================================================
 * $Log$
 * Revision 1.4  2006/05/15 15:26:53  didenko
 * Added support for running exclusive jobs
 *
 * Revision 1.3  2006/05/10 19:54:21  didenko
 * Added JobDelayExpiration method to CWorkerNodeContext class
 * Added keep_alive_period and max_job_run_time parmerter to the config
 * file of remote_app
 *
 * Revision 1.2  2006/05/08 15:16:42  didenko
 * Added support for an optional saving of a remote application's stdout
 * and stderr into files on a local file system
 *
 * Revision 1.1  2006/05/03 14:55:21  didenko
 * Added ns_submit_remote_job utility.
 *
 * ===========================================================================
 */
 
