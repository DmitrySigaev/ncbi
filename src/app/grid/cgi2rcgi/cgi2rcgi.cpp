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
 * Author:  Maxim Didenko, Dmitry Kazimirov
 *
 * File Description:
 *
 */

#include <ncbi_pch.hpp>

#include <cgi/cgiapp_cached.hpp>
#include <cgi/cgictx.hpp>
#include <cgi/cgi_serial.hpp>

#include <html/commentdiag.hpp>
#include <html/html.hpp>
#include <html/page.hpp>

#include <util/xregexp/regexp.hpp>

#include <connect/services/grid_client.hpp>

#include <connect/email_diag_handler.hpp>

#include <corelib/ncbistr.hpp>
#include <corelib/ncbimisc.hpp>
#include <corelib/ncbi_system.hpp>

#include <vector>
#include <map>
#include <sstream>


#define CGI2RCGI_VERSION_MAJOR 2
#define CGI2RCGI_VERSION_MINOR 0
#define CGI2RCGI_VERSION_PATCH 0

// Program version is passed to NetSchedule so that it
// does not allow obsolete clients to submit or execute jobs.
#define PROGRAM_VERSION "Cgi2RCgi ver " \
    NCBI_AS_STRING(CGI2RCGI_VERSION_MAJOR) "." \
    NCBI_AS_STRING(CGI2RCGI_VERSION_MINOR) "." \
    NCBI_AS_STRING(CGI2RCGI_VERSION_PATCH)

USING_NCBI_SCOPE;

static const string kSinceTime = "ctg_time";


/** @addtogroup NetScheduleClient
 *
 * @{
 */

/////////////////////////////////////////////////////////////////////////////
//  Grid Cgi Context
//  Context in which a request is processed
//
class CGridCgiContext
{
public:
    CGridCgiContext(CHTMLPage& page,
        CHTMLPage& custom_http_header, CCgiContext& ctx);

    // Get the HTML page
    CHTMLPage&    GetHTMLPage()       { return m_Page; }

    // Get the self URL
    string        GetSelfURL() const;

    // Get current job progress message
    const string& GetJobProgressMessage() const
        { return m_ProgressMsg; }

    // Get a value from a CGI request. if there is no an entry with a
    // given name it returns an empty string.
    const string& GetPersistentEntryValue(const string& entry_name) const;

    void GetQueryStringEntryValue(const string& entry_name,
        string& value) const;
    void GetRequestEntryValue(const string& entry_name, string& value) const;

    // Save this entry as a cookie add it to serf url
    void PullUpPersistentEntry(const string& entry_name);
    void PullUpPersistentEntry(const string& entry_name, string& value);
    void DefinePersistentEntry(const string& entry_name, const string& value);

    void LoadQueryStringTags();

    // Get CGI Context
    CCgiContext& GetCGIContext() { return m_CgiContext; }

    void SelectView(const string& view_name);
    void SetCompleteResponse(CNcbiIstream& is);
    bool NeedRenderPage() const { return m_NeedRenderPage; }

    string GetHiddenFields() const;

    const string& GetJobInput() const { return m_JobInput; }
    const string& GetJobOutput() const { return m_JobOutput; }

public:
    // Remove all persistent entries from cookie and self url.
    void Clear();

    void SetJobProgressMessage(const string& msg)
        { m_ProgressMsg = msg; }
    void SetJobInput(const string& input) { m_JobInput = input; }
    void SetJobOutput(const string& output) { m_JobOutput = output; }

private:
    typedef map<string,string>    TPersistentEntries;

    CHTMLPage&                    m_Page;
    CHTMLPage&                    m_CustomHTTPHeader;
    CCgiContext&                  m_CgiContext;
    TCgiEntries                   m_ParsedQueryString;
    TPersistentEntries            m_PersistentEntries;
    string                        m_ProgressMsg;
    string                        m_JobInput;
    string                        m_JobOutput;
    bool                          m_NeedRenderPage;
};


/////////////////////////////////////////////////////////////////////////////


CGridCgiContext::CGridCgiContext(CHTMLPage& page,
        CHTMLPage& custom_http_header, CCgiContext& ctx) :
    m_Page(page),
    m_CustomHTTPHeader(custom_http_header),
    m_CgiContext(ctx),
    m_NeedRenderPage(true)
{
    const CCgiRequest& req = ctx.GetRequest();
    string query_string = req.GetProperty(eCgi_QueryString);
    CCgiRequest::ParseEntries(query_string, m_ParsedQueryString);
}

string CGridCgiContext::GetSelfURL() const
{
    string url = m_CgiContext.GetSelfURL();
    bool first = true;
    TPersistentEntries::const_iterator it;
    for (it = m_PersistentEntries.begin();
         it != m_PersistentEntries.end(); ++it) {
        const string& name = it->first;
        const string& value = it->second;
        if (!name.empty() && !value.empty()) {
            if (first) {
                url += '?';
                first = false;
            }
            else
                url += '&';
            url += name + '=' + NStr::URLEncode(value);
        }
    }
    return url;
}

string CGridCgiContext::GetHiddenFields() const
{
    string ret;
    TPersistentEntries::const_iterator it;
    for (it = m_PersistentEntries.begin();
         it != m_PersistentEntries.end(); ++it) {
        const string& name = it->first;
        const string& value = it->second;
        ret += "<INPUT TYPE=\"HIDDEN\" NAME=\"" + name
             + "\" VALUE=\"" + value + "\">\n";
    }
    return ret;
}

const string& CGridCgiContext::GetPersistentEntryValue(
    const string& entry_name) const
{
    TPersistentEntries::const_iterator it = m_PersistentEntries.find(entry_name);
    if (it != m_PersistentEntries.end())
        return it->second;
    return kEmptyStr;
}

void CGridCgiContext::GetQueryStringEntryValue(const string& entry_name,
    string& value) const
{
    ITERATE(TCgiEntries, eit, m_ParsedQueryString) {
        if (NStr::CompareNocase(entry_name, eit->first) == 0) {
            string v = eit->second;
            if (!v.empty())
                value = v;
        }
    }
}

void CGridCgiContext::GetRequestEntryValue(const string& entry_name,
    string& value) const
{
    const TCgiEntries entries = m_CgiContext.GetRequest().GetEntries();
    ITERATE(TCgiEntries, eit, entries) {
        if (NStr::CompareNocase(entry_name, eit->first) == 0) {
            string v = eit->second;
            if (!v.empty())
                value = v;
        }
    }
}

void CGridCgiContext::PullUpPersistentEntry(const string& entry_name)
{
    string value = kEmptyStr;
    PullUpPersistentEntry(entry_name, value);
}

void CGridCgiContext::PullUpPersistentEntry(
    const string& entry_name, string& value)
{
    GetQueryStringEntryValue(entry_name, value);
    if (value.empty())
        GetRequestEntryValue(entry_name, value);
    NStr::TruncateSpacesInPlace(value);
    DefinePersistentEntry(entry_name, value);
}

void CGridCgiContext::DefinePersistentEntry(const string& entry_name,
    const string& value)
{
    if (value.empty()) {
        TPersistentEntries::iterator it =
              m_PersistentEntries.find(entry_name);
        if (it != m_PersistentEntries.end())
            m_PersistentEntries.erase(it);
    } else {
        m_PersistentEntries[entry_name] = value;
    }
}

void CGridCgiContext::LoadQueryStringTags()
{
    ITERATE(TCgiEntries, eit, m_ParsedQueryString) {
        string tag("QUERY_STRING:" + eit->first);
        m_Page.AddTagMap(tag, new CHTMLPlainText(eit->second.c_str()));
        m_CustomHTTPHeader.AddTagMap(tag,
            new CHTMLPlainText(eit->second.c_str()));
    }
}

void CGridCgiContext::Clear()
{
    m_PersistentEntries.clear();
}

void CGridCgiContext::SelectView(const string& view_name)
{
    m_CustomHTTPHeader.AddTagMap("CUSTOM_HTTP_HEADER",
        new CHTMLText("<@HEADER_" + view_name + "@>"));
    m_Page.AddTagMap("STAT_VIEW", new CHTMLText("<@VIEW_" + view_name + "@>"));
}

void CGridCgiContext::SetCompleteResponse(CNcbiIstream& is)
{
    m_CgiContext.GetResponse().out() << is.rdbuf();
    m_NeedRenderPage = false;
}

/////////////////////////////////////////////////////////////////////////////
//
//  Grid CGI Front-end Application
//
//  Class for CGI applications starting background jobs using
//  NetSchedule. Implements job submission, status check,
//  error processing, etc.  All request processing is done on the back end.
//  CGI application is responsible for UI rendering.
//
class CCgi2RCgiApp : public CCgiApplicationCached
{
public:
    // This method is called on the CGI application initialization -- before
    // starting to process a HTTP request or even receiving one.
    virtual void Init();

    // Factory method for the Context object construction.
    virtual CCgiContext* CreateContextWithFlags(CNcbiArguments* args,
        CNcbiEnvironment* env, CNcbiIstream* inp, CNcbiOstream* out,
            int ifd, int ofd, int flags);

    // The main method of this CGI application.
    // HTTP requests are processed in this method.
    virtual int ProcessRequest(CCgiContext& ctx);

private:
    void DefineRefreshTags(const string& url, int delay);

private:
    enum EJobPhase {
        ePending,
        eRunning,
        eTerminated
    };

    EJobPhase x_CheckJobStatus(CGridCgiContext& grid_ctx,
        CGridJobStatus*& job_status);

    int m_RefreshDelay;
    int m_FirstDelay;

    CNetScheduleAPI m_NSClient;
    CNetCacheAPI m_NSStorage;
    auto_ptr<CGridClient> m_GridClient;
    CCgiResponse* m_Response;

public:
    CCgi2RCgiApp() {
        SetVersion(CVersionInfo(
            CGI2RCGI_VERSION_MAJOR,
            CGI2RCGI_VERSION_MINOR,
            CGI2RCGI_VERSION_PATCH));
    }

private:
    enum {
        eUseQueryString = 1,
        eUseRequestContent = 2
    };

    // This method is called when the worker node reported a failure.
    void OnJobFailed(const string& msg, CGridCgiContext& ctx);

    string m_Title;
    string m_FallBackUrl;
    int    m_FallBackDelay;
    int    m_CancelGoBackDelay;
    string m_DateFormat;
    string m_ElapsedTimeFormat;

    auto_ptr<CHTMLPage> m_Page;
    auto_ptr<CHTMLPage> m_CustomHTTPHeader;

    string m_HtmlTemplate;
    vector<string> m_HtmlIncs;

    string  m_StrPage;

    string m_AffinityName;
    int m_AffinitySource;
};

void CCgi2RCgiApp::Init()
{
    // Standard CGI framework initialization
    CCgiApplicationCached::Init();

    // Grid client initialization
    CNcbiRegistry& config(GetConfig());
    string grid_cgi_section("grid_cgi");

    m_RefreshDelay = config.GetInt(grid_cgi_section,
        "refresh_delay", 5, IRegistry::eReturn);

    m_FirstDelay = config.GetInt(grid_cgi_section,
        "expect_complete", 5, IRegistry::eReturn);

    if (m_FirstDelay > 20)
        m_FirstDelay = 20;

    if (m_FirstDelay < 0)
        m_FirstDelay = 0;

    m_NSClient = CNetScheduleAPI(config);
    m_NSClient.SetProgramVersion(PROGRAM_VERSION);

    m_NSStorage = CNetCacheAPI(config);

    m_GridClient.reset(new CGridClient(
        m_NSClient.GetSubmitter(),
        m_NSStorage,
        config.GetBool(grid_cgi_section, "automatic_cleanup",
            true, IRegistry::eReturn) ?
                CGridClient::eAutomaticCleanup : CGridClient::eManualCleanup,
        config.GetBool(grid_cgi_section, "use_progress",
            true, IRegistry::eReturn) ?
                CGridClient::eProgressMsgOn : CGridClient::eProgressMsgOff,
        config.GetBool(kNetScheduleAPIDriverName,
            !config.Get(kNetScheduleAPIDriverName,
                "use_embedded_storage").empty() ?
                    "use_embedded_storage" : "use_embedded_input",
                        false, 0, CNcbiRegistry::eReturn)));

    // Allows CGI client to put the diagnostics to:
    //   HTML body (as comments) -- using CGI arg "&diag-destination=comments"
    RegisterDiagFactory("comments", new CCommentDiagFactory);
    //   E-mail -- using CGI arg "&diag-destination=email:user@host"
    RegisterDiagFactory("email",    new CEmailDiagFactory);


    // Initialize processing of both cmd-line arguments and HTTP entries

    // Create CGI argument descriptions class
    //  (For CGI applications only keys can be used)
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // Specify USAGE context
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "Cgi2RCgi application");

    arg_desc->AddOptionalKey("Cancel",
                             "Cancel",
                             "Cancel Job",
                             CArgDescriptions::eString);

    // Setup arg.descriptions for this application
    SetupArgDescriptions(arg_desc.release());

    // Read configuration parameters
    string cgi2rcgi_section("cgi2rcgi");

    m_Title = config.GetString(cgi2rcgi_section, "cgi_title",
                                    "Remote CGI Status Checker");

    m_HtmlTemplate = config.GetString(cgi2rcgi_section, "html_template",
                                           "cgi2rcgi.html");

    string incs = config.GetString(cgi2rcgi_section, "html_template_includes",
                                        "cgi2rcgi.inc.html");

    NStr::Tokenize(incs, ",; ", m_HtmlIncs, NStr::eMergeDelims);


    m_FallBackUrl = config.GetString(cgi2rcgi_section,
        "fall_back_url", kEmptyStr);
    m_FallBackDelay = config.GetInt(cgi2rcgi_section,
        "error_fall_back_delay", -1, IRegistry::eReturn);

    m_CancelGoBackDelay = config.GetInt(cgi2rcgi_section,
        "cancel_fall_back_delay", 0, IRegistry::eReturn);

    if (m_FallBackUrl.empty()) {
        m_FallBackDelay = -1;
        m_CancelGoBackDelay = -1;
    }

    m_AffinitySource = 0;
    m_AffinityName = config.GetString(cgi2rcgi_section,
        "affinity_name", kEmptyStr);

    if (!m_AffinityName.empty()) {
        vector<string> affinity_methods;
        NStr::Tokenize(config.GetString(cgi2rcgi_section,
            "affinity_source", "GET"), ", ;&|",
                affinity_methods, NStr::eMergeDelims);
        for (vector<string>::const_iterator it = affinity_methods.begin();
                it != affinity_methods.end(); ++it) {
            if (*it == "GET")
                m_AffinitySource |= eUseQueryString;
            else if (*it == "POST")
                m_AffinitySource |= eUseRequestContent;
            else {
                NCBI_THROW_FMT(CArgException, eConstraint,
                    "Invalid affinity_source value '" << *it << '\'');
            }
        }
    }

    // Disregard the case of CGI arguments
    CCgiRequest::TFlags flags = CCgiRequest::fCaseInsensitiveArgs;

    if (config.GetBool(cgi2rcgi_section, "ignore_query_string",
            true, IRegistry::eReturn))
        flags |= CCgiRequest::fIgnoreQueryString;

    if (config.GetBool(cgi2rcgi_section, "donot_parse_content",
            true, IRegistry::eReturn) &&
                !(m_AffinitySource & eUseRequestContent))
        flags |= CCgiRequest::fDoNotParseContent;

    SetRequestFlags(flags);

    m_DateFormat = config.GetString(cgi2rcgi_section,
        "date_format", "D B Y, h:m:s");

    m_ElapsedTimeFormat = config.GetString(cgi2rcgi_section,
        "elapsed_time_format", "S");
}

CCgiContext* CCgi2RCgiApp::CreateContextWithFlags(CNcbiArguments* args,
    CNcbiEnvironment* env, CNcbiIstream* inp, CNcbiOstream* out,
        int ifd, int ofd, int flags)
{
    if (flags & CCgiRequest::fDoNotParseContent)
        return CCgiApplicationCached::CreateContextWithFlags(args, env,
            inp, out, ifd, ofd, flags);

    if (m_AffinitySource & eUseRequestContent)
        return CCgiApplicationCached::CreateContextWithFlags(args, env,
            inp, out, ifd, ofd, flags | CCgiRequest::fSaveRequestContent);

    // The 'env' argument is only valid in FastCGI mode.
    if (env == NULL)
        env = const_cast<CNcbiEnvironment*>(&GetEnvironment());

    size_t content_length = 0;

    try {
        content_length = (size_t) NStr::StringToUInt(
            env->Get(CCgiRequest::GetPropertyName(eCgi_ContentLength)));
    }
    catch (...) {
    }

    // Based on the CONTENT_LENGTH CGI parameter, decide whether to parse
    // the POST request in search of the job_key parameter.
    return CCgiApplicationCached::CreateContextWithFlags(args, env, inp,
        out, ifd, ofd, flags | (content_length > 0 &&
            content_length < 128 ? CCgiRequest::fSaveRequestContent :
                CCgiRequest::fDoNotParseContent));
}

static const string kGridCgiForm =
    "<FORM METHOD=\"GET\" ACTION=\"<@SELF_URL@>\">\n"
    "<@HIDDEN_FIELDS@>\n<@STAT_VIEW@>\n"
    "</FORM>";

class CRegexpTemplateFilter : public CHTMLPage::TTemplateLibFilter
{
public:
    CRegexpTemplateFilter(CHTMLPage* page) : m_Page(page) {}

    virtual bool TestAttribute(const string& attr_name,
        const string& test_pattern);

private:
    CHTMLPage* m_Page;
};

bool CRegexpTemplateFilter::TestAttribute(const string& attr_name,
    const string& test_pattern)
{
    CNCBINode* node = m_Page->MapTag(attr_name);

    if (node == NULL)
        return false;

    CNcbiOstrstream node_stream;

    node->Print(node_stream, CNCBINode::ePlainText);

    CRegexp regexp(test_pattern, CRegexp::fCompile_ignore_case);

    return regexp.IsMatch(node_stream.str());
}

int CCgi2RCgiApp::ProcessRequest(CCgiContext& ctx)
{
    // Given "CGI context", get access to its "HTTP request" and
    // "HTTP response" sub-objects
    CCgiResponse& response = ctx.GetResponse();
    m_Response = &response;

    // Create an HTML page (using the template HTML file)
    try {
        m_Page.reset(new CHTMLPage(m_Title, m_HtmlTemplate));
        CHTMLText* stat_view = new CHTMLText(kGridCgiForm);
        m_Page->AddTagMap("VIEW", stat_view);
    }
    catch (exception& e) {
        ERR_POST("Failed to create " << m_Title << " HTML page: " << e.what());
        return 2;
    }
    m_CustomHTTPHeader.reset(new CHTMLPage);
    m_CustomHTTPHeader->SetTemplateString("<@CUSTOM_HTTP_HEADER@>");
    CGridCgiContext grid_ctx(*m_Page, *m_CustomHTTPHeader, ctx);

    string job_key(kEmptyStr);
    grid_ctx.PullUpPersistentEntry("job_key", job_key);
    grid_ctx.PullUpPersistentEntry("Cancel");

    grid_ctx.LoadQueryStringTags();
    try {
        EJobPhase phase = eTerminated;
        CGridJobStatus* job_status = NULL;

        grid_ctx.PullUpPersistentEntry(kSinceTime);

        try {
            if (!job_key.empty()) {
                phase = x_CheckJobStatus(grid_ctx, job_status);

                if (phase == eTerminated)
                    grid_ctx.Clear();
                else {
                    // Check if job cancellation has been requested
                    // via the user interface(HTML).
                    if (GetArgs()["Cancel"] ||
                            !grid_ctx.GetPersistentEntryValue("Cancel").empty())
                        m_GridClient->CancelJob(job_key);

                    DefineRefreshTags(grid_ctx.GetSelfURL(), m_RefreshDelay);
                }
            }
            else {
                // Get a job submitter
                CGridJobSubmitter& submitter = m_GridClient->GetJobSubmitter();
                // Submit a job
                if (!m_AffinityName.empty()) {
                    string affinity;
                    if (m_AffinitySource & eUseQueryString)
                        grid_ctx.GetQueryStringEntryValue(m_AffinityName,
                            affinity);
                    if (affinity.empty() &&
                            m_AffinitySource & eUseRequestContent)
                        grid_ctx.GetRequestEntryValue(m_AffinityName, affinity);
                    if (!affinity.empty())
                        submitter.SetJobAffinity(affinity);
                }
                try {
                    // The job is ready to be sent to the queue.
                    // Prepare the input data.
                    CNcbiOstream& os = submitter.GetOStream();
                    // Send the input data.
                    ctx.GetRequest().Serialize(os);
                    string saved_content(kEmptyStr);
                    try {
                        saved_content = ctx.GetRequest().GetContent();
                    }
                    catch (...) {
                        // An exception is normal when the content
                        // is not saved, disregard the exception.
                    }
                    if (!saved_content.empty())
                        os.write(saved_content.data(), saved_content.length());
                    job_key = submitter.Submit();
                    grid_ctx.DefinePersistentEntry("job_key", job_key);

                    unsigned long wait_time = m_FirstDelay*1000;
                    unsigned long sleep_time = 6;
                    unsigned long total_sleep_time = 0;
                    for (;;) {
                        if (total_sleep_time >= wait_time) {
                            // The job has just been submitted.
                            // Render a report page
                            CTime time(GetFastLocalTime());
                            grid_ctx.DefinePersistentEntry(kSinceTime,
                                NStr::IntToString((long) time.GetTimeT()));
                            grid_ctx.SelectView("JOB_SUBMITTED");
                            DefineRefreshTags(grid_ctx.GetSelfURL(),
                                m_RefreshDelay);
                            break;
                        }
                        SleepMilliSec(sleep_time);

                        phase = x_CheckJobStatus(grid_ctx, job_status);
                        if (phase == eTerminated)
                            break;

                        total_sleep_time += sleep_time;
                        sleep_time += sleep_time/3;
                    }
                }
                catch (CNetScheduleException& ex) {
                    ERR_POST("Failed to submit a job: " << ex.what());
                    OnJobFailed(ex.GetErrCode() ==
                            CNetScheduleException::eTooManyPendingJobs ?
                        "NetSchedule Queue is busy" : ex.what(), grid_ctx);
                    phase = eTerminated;
                }
                catch (exception& ex) {
                    ERR_POST("Failed to submit a job: " << ex.what());
                    OnJobFailed(ex.what(), grid_ctx);
                    phase = eTerminated;
                }
                if (phase == eTerminated)
                    grid_ctx.Clear();
            }
        } // try
        catch (exception& ex) {
            ERR_POST("Job's reported as failed: " << ex.what());
            OnJobFailed(ex.what(), grid_ctx);
        }
        CHTMLPlainText* self_url =
            new CHTMLPlainText(grid_ctx.GetSelfURL(), true);
        m_Page->AddTagMap("SELF_URL", self_url);
        m_CustomHTTPHeader->AddTagMap("SELF_URL", self_url);
        CHTMLPlainText* hidden_fields =
            new CHTMLPlainText(grid_ctx.GetHiddenFields(), true);
        m_Page->AddTagMap("HIDDEN_FIELDS", hidden_fields);

        CTime now(GetFastLocalTime());
        m_Page->AddTagMap("DATE",
            new CHTMLText(now.AsString(m_DateFormat)));
        string since_time = grid_ctx.GetPersistentEntryValue(kSinceTime);
        if (!since_time.empty()) {
            m_Page->AddTagMap("SINCE_TIME", new CHTMLText(since_time));
            m_CustomHTTPHeader->AddTagMap("SINCE_TIME",
                new CHTMLText(since_time));
            time_t tt = NStr::StringToInt(since_time);
            CTime start;
            start.SetTimeT(tt);
            m_Page->AddTagMap("SINCE",
                         new CHTMLText(start.AsString(m_DateFormat)));
            CTimeSpan ts = now - start;
            m_Page->AddTagMap("ELAPSED_TIME_MSG_HERE",
                         new CHTMLText("<@ELAPSED_TIME_MSG@>"));
            m_Page->AddTagMap("ELAPSED_TIME",
                         new CHTMLText(ts.AsString(m_ElapsedTimeFormat)));
        }
        m_Page->AddTagMap("JOB_ID", new CHTMLText(job_key));
        m_CustomHTTPHeader->AddTagMap("JOB_ID", new CHTMLText(job_key));
        if (phase == eRunning) {
            grid_ctx.SetJobProgressMessage(job_status->GetProgressMessage());
            CHTMLText* err = new CHTMLText(grid_ctx.GetJobProgressMessage());
            grid_ctx.GetHTMLPage().AddTagMap("PROGERSS_MSG", err);
            err = new CHTMLText(grid_ctx.GetJobProgressMessage());
            grid_ctx.GetHTMLPage().AddTagMap("PROGRESS_MSG", err);
        }
    } //try
    catch (exception& e) {
        ERR_POST("Failed to populate " << m_Title <<
            " HTML page: " << e.what());
        return 3;
    }

    if (!grid_ctx.NeedRenderPage())
        return 0;

    // Compose and flush the resultant HTML page
    try {
        CRegexpTemplateFilter filter(m_Page.get());

        vector<string>::const_iterator it;
        for (it = m_HtmlIncs.begin(); it != m_HtmlIncs.end(); ++it) {
            string lib = NStr::TruncateSpaces(*it);
            m_Page->LoadTemplateLibFile(lib, &filter);
            m_CustomHTTPHeader->LoadTemplateLibFile(lib, &filter);
        }

        stringstream header_stream;
        m_CustomHTTPHeader->Print(header_stream, CNCBINode::ePlainText);
        string header_line;
        while (header_stream.good()) {
            getline(header_stream, header_line);
            if (!header_line.empty())
                response.out() << header_line << "\r\n";
        }
        response.WriteHeader();
        m_Page->Print(response.out(), CNCBINode::eHTML);
    }
    catch (exception& e) {
        ERR_POST("Failed to compose/send " << m_Title <<
            " HTML page: " << e.what());
        return 4;
    }

    return 0;
}

void CCgi2RCgiApp::DefineRefreshTags(const string& url, int idelay)
{
    if (idelay >= 0) {
        CHTMLText* redirect = new CHTMLText(
                    "<META HTTP-EQUIV=Refresh "
                    "CONTENT=\"<@REDIRECT_DELAY@>; URL=<@REDIRECT_URL@>\">");
        m_Page->AddTagMap("REDIRECT", redirect);

        CHTMLPlainText* delay = new CHTMLPlainText(NStr::IntToString(idelay));
        m_Page->AddTagMap("REDIRECT_DELAY", delay);
    }

    CHTMLPlainText* h_url = new CHTMLPlainText(url, true);
    m_Page->AddTagMap("REDIRECT_URL", h_url);
    m_CustomHTTPHeader->AddTagMap("REDIRECT_URL", h_url);
    m_Response->SetHeaderValue("Expires", "0");
    m_Response->SetHeaderValue("Pragma", "no-cache");
    m_Response->SetHeaderValue("Cache-Control",
        "no-cache, no-store, max-age=0, private, must-revalidate");
    m_Response->SetHeaderValue("NCBI-RCGI-RetryURL", url);
}


CCgi2RCgiApp::EJobPhase CCgi2RCgiApp::x_CheckJobStatus(
        CGridCgiContext& grid_ctx, CGridJobStatus*& job_status)
{
    string job_key = grid_ctx.GetPersistentEntryValue("job_key");
    job_status = &m_GridClient->GetJobStatus(job_key);

    CNetScheduleAPI::EJobStatus status;
    status = job_status->GetStatus();
    grid_ctx.SetJobInput(job_status->GetJobInput());
    grid_ctx.SetJobOutput(job_status->GetJobOutput());

    EJobPhase phase = eTerminated;
    bool save_result = false;
    grid_ctx.GetCGIContext().GetResponse().SetHeaderValue("NCBI-RCGI-JobStatus",
        CNetScheduleAPI::StatusToString(status));
    switch (status) {
    case CNetScheduleAPI::eDone:
        // The worker node has finished the job and the
        // result is ready to be retrieved.
        {
            CNcbiIstream& is = job_status->GetIStream();

            if (job_status->GetBlobSize() > 0) {
                grid_ctx.SetCompleteResponse(is);
            } else {
                m_StrPage = "<html><head><title>Empty Result</title>"
                            "</head><body>Empty Result</body></html>";
                grid_ctx.GetHTMLPage().SetTemplateString(m_StrPage.c_str());
            }
        }
        save_result = true;
        break;
    case CNetScheduleAPI::eFailed:
        // a job has failed
        OnJobFailed(job_status->GetErrorMessage(), grid_ctx);
        break;

    case CNetScheduleAPI::eCanceled:
        // The job has been canceled
        grid_ctx.DefinePersistentEntry(kSinceTime, kEmptyStr);
        // Render a job cancellation page
        grid_ctx.SelectView("JOB_CANCELED");

        DefineRefreshTags(m_FallBackUrl.empty() ?
            grid_ctx.GetCGIContext().GetSelfURL() : m_FallBackUrl,
                m_CancelGoBackDelay);
        break;

    case CNetScheduleAPI::eJobNotFound:
        // The job has expired
        OnJobFailed("Job is not found.", grid_ctx);
        break;

    case CNetScheduleAPI::ePending:
    case CNetScheduleAPI::eReturned:
        // The job is in the NetSchedule queue and
        // is waiting for a worker node.
        // Render a status report page
        grid_ctx.SelectView("JOB_PENDING");
        phase = ePending;
        break;

    case CNetScheduleAPI::eRunning:
        // The job is being processed by a worker node
        // Render a status report page
        grid_ctx.SelectView("JOB_RUNNING");
        phase = eRunning;
        break;

    default:
        _ASSERT(0 && "Unexpected job state");
    }
    SetRequestId(job_key, save_result);
    return phase;
}

void CCgi2RCgiApp::OnJobFailed(const string& msg,
                                  CGridCgiContext& ctx)
{
    ctx.DefinePersistentEntry(kSinceTime, kEmptyStr);
    // Render a error page
    ctx.SelectView("JOB_FAILED");

    string fall_back_url = m_FallBackUrl.empty() ?
        ctx.GetCGIContext().GetSelfURL() : m_FallBackUrl;
    DefineRefreshTags(fall_back_url, m_FallBackDelay);

    CHTMLPlainText* err = new CHTMLPlainText(msg);
    ctx.GetHTMLPage().AddTagMap("MSG", err);
}

/////////////////////////////////////////////////////////////////////////////
int main(int argc, const char* argv[])
{
    return CCgi2RCgiApp().AppMain(argc, argv, 0, eDS_Default);
}
