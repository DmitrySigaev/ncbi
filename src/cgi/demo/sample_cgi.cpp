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
 * Author:  Denis Vakatov
 *
 * File Description:
 *   Plain example of a CGI application.
 *
 *   USAGE:  sample.cgi?message=Some+Message
 *
 *   NOTE:  needs HTML template file "sample_cgi.html" in curr. dir to run!
 *
 */

#include <cgi/cgiapp.hpp>
#include <cgi/cgictx.hpp>
#include <connect/email_diag_handler.hpp>
#include <html/commentdiag.hpp>
#include <html/html.hpp>
#include <html/page.hpp>

using namespace ncbi;


/////////////////////////////////////////////////////////////////////////////
//  CSampleCgi::
//

class CSampleCgi : public CCgiApplication
{
public:
    virtual void Init(void);
    virtual int  ProcessRequest(CCgiContext& ctx);
};


void CSampleCgi::Init()
{
    // Allows CGI client to put the diagnostics to:
    //   HTML body (as comments) -- using CGI arg "&diag-destination=comments"
    RegisterDiagFactory("comments", new CCommentDiagFactory);
    //   E-mail -- using CGI arg "&diag-destination=email:user@host"
    RegisterDiagFactory("email",    new CEmailDiagFactory);
}


int CSampleCgi::ProcessRequest(CCgiContext& ctx)
{
    // Given "CGI context", get access to its "HTTP request" and
    // "HTTP response" sub-objects
    const CCgiRequest& request  = ctx.GetRequest();
    CCgiResponse&      response = ctx.GetResponse();

    // Try to retrieve the message ('message=...') from the URL args
    bool   is_message = false;
    string message    = request.GetEntry("message", &is_message);
    if ( is_message ) {
        message = "'" + message + "'";
    } else {
        message = "<NONE>";
    }

    // Create a HTML page (using template HTML file "sample_cgi.html")
    auto_ptr<CHTMLPage> page;
    try {
        page.reset(new CHTMLPage("Sample CGI", "sample_cgi.html"));
    } catch (exception& e) {
        ERR_POST("Failed to create Sample CGI HTML page: " << e.what());
        return 2;
    }
    SetDiagNode(page.get());
    

    // Register substitution for the template parameter <@MESSAGE@>
    try {
        CHTMLText* text = new CHTMLText(message);
        _TRACE("foo");
        SetDiagNode(text);
        _TRACE("bar");
        page->AddTagMap("MESSAGE", text);
    } catch (exception& e) {
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
//  MAIN
//

int main(int argc, const char* argv[])
{
    int result = CSampleCgi().AppMain(argc, argv, 0, eDS_Default, 0);
    _TRACE("back to normal diags");
    return result;
}


/*
 * ===========================================================================
 * $Log$
 * Revision 1.5  2002/04/16 19:24:42  vakatov
 * Do not use <test/test_assert.h>;  other minor modifications.
 *
 * Revision 1.4  2002/04/16 18:47:08  ivanov
 * Centralize threatment of assert() in tests.
 * Added #include <test/test_assert.h>. CVS log moved to end of file.
 *
 * Revision 1.3  2001/11/19 15:20:18  ucko
 * Switch CGI stuff to new diagnostics interface.
 *
 * Revision 1.2  2001/10/29 15:16:13  ucko
 * Preserve default CGI diagnostic settings, even if customized by app.
 *
 * Revision 1.1  2001/10/04 18:17:54  ucko
 * Accept additional query parameters for more flexible diagnostics.
 * Support checking the readiness of CGI input and output streams.
 *
 * ===========================================================================
 */
