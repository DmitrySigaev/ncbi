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
 * Author:  Maxim Didenko
 *
 * File Description:
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbistr.hpp>
#include <connect/email_diag_handler.hpp>
#include <html/commentdiag.hpp>
#include <connect/services/grid_default_factories.hpp>

#include <misc/grid_cgi/grid_cgiapp.hpp>


#include <vector>

BEGIN_NCBI_SCOPE


/////////////////////////////////////////////////////////////////////////////
//  CGridCgiSampleApplication::
//

const char* s_jscript = 
           "<script language=\"JavaScript\">\n"
           "<!--\n"
           "function RefreshStatus()\n"
           "{ document.location.href=\"<@SELF_URL@>\" }\n"
           "var timer = null\n"
           "var delay = 3000\n"
           "timer = setTimeout(\"RefreshStatus()\", delay)\n"
           "//-->\n"
           "</script>\n";

string CGridCgiApplication::GetRefreshJScript() const
{
    return s_jscript;
}


void CGridCgiApplication::Init()
{
    // Standard CGI framework initialization
    CCgiApplication::Init();

    // Allows CGI client to put the diagnostics to:
    //   HTML body (as comments) -- using CGI arg "&diag-destination=comments"
    RegisterDiagFactory("comments", new CCommentDiagFactory);
    //   E-mail -- using CGI arg "&diag-destination=email:user@host"
    RegisterDiagFactory("email",    new CEmailDiagFactory);


    if (!m_NSClient.get()) {
        CNetScheduleClientFactory cf(GetConfig());
        m_NSClient.reset(cf.CreateInstance());
    }
    if( !m_NSStorage.get()) {
        CNetScheduleStorageFactory_NetCache cf(GetConfig());
        m_NSStorage.reset(cf.CreateInstance());
    }
    m_GridClient.reset(new CGridClient(*m_NSClient, *m_NSStorage));

}


int CGridCgiApplication::ProcessRequest(CCgiContext& ctx)
{
    m_StopJob = false;
    try{
    const CArgs& args = GetArgs();
    if ( args["Stop"] ) 
        m_StopJob = true;
    }
    catch(...) {}

    // Given "CGI context", get access to its "HTTP request" and
    // "HTTP response" sub-objects
    const CCgiRequest& request  = ctx.GetRequest();
    const CCgiCookies& req_cookies = request.GetCookies();
    CCgiResponse&      response = ctx.GetResponse();
    CCgiCookies&       res_cookies = response.Cookies();

    // Create a HTML page (using template HTML file "grid_cgi_sample.html")
    auto_ptr<CHTMLPage> page;
    try {
        page.reset(new CHTMLPage(GetPageTitle(), GetPageTemplate()));
    } catch (exception& e) {
        ERR_POST("Failed to create Sample CGI HTML page: " << e.what());
        return 2;
    }
    SetDiagNode(page.get());

    string message;
    const CCgiCookie* c = req_cookies.Find("job_key", "", "" );
    try {

        if (c) {
            string job_key = c->GetValue();
            CGridJobStatus& job_status = GetGridClient().GetJobStatus(job_key);
            CNetScheduleClient::EJobStatus status;
            status = job_status.GetStatus();
        
            bool remove_cookie = false;
            // A job is done here
            if (status == CNetScheduleClient::eDone) {
                // Get an input stream
                CNcbiIstream& is = job_status.GetIStream();               
                OnJobDone(is, *page);

                remove_cookie = true;
            }

            // A job has failed
            else if (status == CNetScheduleClient::eFailed) {
                OnJobFailed(job_status.GetErrorMessage(), *page);
                remove_cookie = true;
            }

            // A job has been canceled
            else if (status == CNetScheduleClient::eCanceled) {
                OnJobCanceled(*page);
                remove_cookie = true;
            }
            else {
                OnStatusCheck(*page);

                CHTMLText* jscript = new CHTMLText(GetRefreshJScript());
                page->AddTagMap("JSCRIPT", jscript);
            }
             
            if (m_StopJob)
                GetGridClient().CancelJob(job_key);

            if (remove_cookie) {
                CCgiCookie c1(*c);
                CTime exp(CTime::eCurrent, CTime::eGmt);
                c1.SetExpTime(--exp);
                c1.SetValue("");
                res_cookies.Add(c1);
            }
        }        
        else {
            if (CollectParams()) {
                // Get a job submiter
                CGridJobSubmiter& job_submiter = GetGridClient().GetJobSubmiter();

                // Get an ouptut stream
                CNcbiOstream& os = job_submiter.GetOStream();

                OnJobSubmit(os, *page);

                // Submit a job
                string job_key = job_submiter.Submit();
                res_cookies.Add("job_key", job_key);
              
                CHTMLText* jscript = new CHTMLText(GetRefreshJScript());
                page->AddTagMap("JSCRIPT", jscript);

            }
            else {
                ShowParamsPage(*page);
            }
        }

        CHTMLPlainText* self_url = new CHTMLPlainText(ctx.GetSelfURL());
        page->AddTagMap("SELF_URL", self_url);
    }
    catch (exception& e) {
        ERR_POST("Failed to populate Sample CGI HTML page: " << e.what());
        SetDiagNode(NULL);
        return 3;
    }

    // Compose and flush the resultant HTML page
    try {
        _TRACE("stream status: " << ctx.GetStreamStatus());
        response.WriteHeader();
        page->Print(response.out(), CNCBINode::eHTML);
    } catch (exception& e) {
        ERR_POST("Failed to compose/send Sample CGI HTML page: " << e.what());
        SetDiagNode(NULL);
        return 4;
    }

    SetDiagNode(NULL);
    return 0;
}




/////////////////////////////////////////////////////////////////////////////

END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.1  2005/03/30 21:17:05  didenko
 * Initial version
 *
 * ===========================================================================
 */
