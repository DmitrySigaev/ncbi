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
 * Author: Maxim Didenko
 *
 *
 */
#include <ncbi_pch.hpp>

#include <corelib/blob_storage.hpp>

#include <cgi/ncbicgi.hpp>
#include <cgi/cgiapp.hpp>

#include <misc/grid_cgi/cgi2grid.hpp>

#include <connect/services/grid_client.hpp>
#include <connect/services/netschedule_api.hpp>
#include <connect/services/ns_client_factory.hpp>

BEGIN_NCBI_SCOPE

static inline 
string s_GetCgiTunnel2GridUrl(const CCgiRequest& cgi_request)
{
    string ret = "http://";
    string server(cgi_request.GetProperty(eCgi_ServerName));
    if (NStr::StartsWith(server, "www.ncbi",   NStr::eNocase) )
        ret += "www.ncbi.nlm.nih.gov";
    else if (NStr::StartsWith(server, "web.ncbi",   NStr::eNocase) )
        ret += "web.ncbi.nlm.nih.gov";
    else if (NStr::StartsWith(server, "wwwqa.ncbi",   NStr::eNocase) )
        ret += "wwwqa.ncbi.nlm.nih.gov";
    else if (NStr::StartsWith(server, "webqa.ncbi",   NStr::eNocase) )
        ret += "webqa.ncbi.nlm.nih.gov";
    else 
        ret += "web.ncbi.nlm.nih.gov";

    ret += "/Service/cgi_tunnel2grid/cgi_tunnel2grid.cgi";
    
    return ret;
}

static inline string s_GetElapsedTime()
{
    return NStr::IntToString((long) CTime(CTime::eCurrent).GetTimeT());
}


CNcbiOstream& CGI2GRID_ComposeHtmlPage(CCgiApplication&    app,
                                       CNcbiOstream&       os,
                                       const CCgiRequest&  cgi_request,
                                       const string&       project_name,
                                       const string&       return_url)
{
    auto_ptr<IBlobStorage> ns_storage;
    auto_ptr<CGridClient> grid_client;

    CNetScheduleClientFactory cfc(app.GetConfig());
    CNetScheduleAPI ns_client = cfc.CreateInstance();
    ns_client.SetProgramVersion("Cgi_Tunnel2Grid ver 1.0.0");

    CBlobStorageFactory cfs(app.GetConfig());
    ns_storage.reset(cfs.CreateInstance());
    grid_client.reset(new CGridClient(ns_client.GetSubmitter(), *ns_storage,
                                      CGridClient::eManualCleanup,
                                      CGridClient::eProgressMsgOn)
                      );
    
    CGridJobSubmitter& job_submitter = grid_client->GetJobSubmitter();
    CNcbiOstream& job_os = job_submitter.GetOStream();
    cgi_request.Serialize(job_os);
    string job_key = job_submitter.Submit();
    string url = s_GetCgiTunnel2GridUrl(cgi_request);
    url += "?ctg_project=" + NStr::URLEncode(project_name);
    url += "&job_key=" + job_key;
    url += "&ctg_error_url=" + NStr::URLEncode(return_url);                   
    url += "&ctg_time=" + s_GetElapsedTime();
    os << "<html><head><<META HTTP-EQUIV=Refresh CONTENT=\"0;" 
       << url << "\"></head><body></body></html>";
    return os;
}


END_NCBI_SCOPE

/* @} */
