#ifndef NCBI_CGI_CTX__HPP
#define NCBI_CGI_CTX__HPP

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
* Author: 
*	Vsevolod Sandomirskiy
*
* File Description:
*   Basic CGI Application class
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.11  1999/10/28 16:53:53  vasilche
* Fixed bug with error message node.
*
* Revision 1.10  1999/10/28 13:37:49  vasilche
* Fixed small memory leak.
*
* Revision 1.9  1999/10/01 14:22:04  golikov
* Now messages in context are html nodes
*
* Revision 1.8  1999/07/15 19:04:37  sandomir
* GetSelfURL(() added in Context
*
* Revision 1.7  1999/06/29 20:04:37  pubmed
* many changes due to query interface changes
*
* Revision 1.6  1999/05/14 19:21:49  pubmed
* myncbi - initial version; minor changes in CgiContext, history, query
*
* Revision 1.4  1999/05/06 20:32:48  pubmed
* CNcbiResource -> CNcbiDbResource; utils from query; few more context methods
*
* Revision 1.3  1999/05/04 16:14:03  vasilche
* Fixed problems with program environment.
* Added class CNcbiEnvironment for cached access to C environment.
*
* Revision 1.2  1999/04/28 16:54:18  vasilche
* Implemented stream input processing for FastCGI applications.
*
* Revision 1.1  1999/04/27 14:49:48  vasilche
* Added FastCGI interface.
* CNcbiContext renamed to CCgiContext.
*
* Revision 1.11  1999/02/22 21:12:37  sandomir
* MsgRequest -> NcbiContext
*
* Revision 1.10  1998/12/28 23:28:59  vakatov
* New CVS and development tree structure for the NCBI C++ projects
*
* Revision 1.9  1998/12/28 15:43:09  sandomir
* minor fixed in CgiApp and Resource
*
* Revision 1.8  1998/12/10 17:36:54  sandomir
* ncbires.cpp added
*
* Revision 1.7  1998/12/09 22:59:05  lewisg
* use new cgiapp class
*
* Revision 1.6  1998/12/09 17:27:44  sandomir
* tool should be changed to work with the new CCgiApplication
*
* Revision 1.5  1998/12/09 16:49:55  sandomir
* CCgiApplication added
*
* Revision 1.1  1998/12/03 21:24:21  sandomir
* NcbiApplication and CgiApplication updated
*
* Revision 1.3  1998/12/01 19:12:36  lewisg
* added CCgiApplication
*
* Revision 1.2  1998/11/05 21:45:13  sandomir
* std:: deleted
*
* Revision 1.1  1998/11/02 22:10:12  sandomir
* CNcbiApplication added; netest sample updated
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <cgi/ncbicgi.hpp>
#include <cgi/ncbicgir.hpp>
#include <html/html.hpp>

BEGIN_NCBI_SCOPE

class CNcbiEnvironment;
class CNcbiRegistry;
class CNcbiResource;
class CCgiContext;
class CCgiApplication;

class CCgiServerContext
{
public:
    virtual ~CCgiServerContext( void )
        { }
};

//
// class CCgiContext
//
// CCgiContext is a wrapper for request, response, server context.
// In addtion, it contains list of messages as html nodes.
// Having non-const reference, CCgiContext's user has access to its 
// all internal data
// Context will try to create request from given data or default request
// on any request creation error

class CCgiContext
{
public:
    
    CCgiContext( CCgiApplication& app, CNcbiEnvironment* env = 0,
                 CNcbiIstream* in = 0, CNcbiOstream* out = 0,
                 int argc = 0, char** argv = 0 );
    virtual ~CCgiContext( void );

    const CCgiApplication& GetApp(void) const;

    const CNcbiRegistry& GetConfig(void) const;
    CNcbiRegistry& GetConfig(void);
    
    // these methods will throw exception if no server context set
    const CNcbiResource& GetResource(void) const;
    CNcbiResource& GetResource(void);

    const CCgiRequest& GetRequest(void) const;
    CCgiRequest& GetRequest(void);
    
    const CCgiResponse& GetResponse(void) const;
    CCgiResponse& GetResponse( void );
    
    // these methods will throw exception if no server context set
    const CCgiServerContext& GetServCtx(void) const;
    CCgiServerContext& GetServCtx(void);

    //message tree functions
    const CNCBINode& GetMsg(void) const;
    CNCBINode& GetMsg(void);

    void PutMsg(const string& msg);
    void PutMsg(CNCBINode* msg);

    bool EmptyMsg(void);
    void ClearMsg(void);

    // request access wrappers

    // returns entry from request
    // returns empty string if no such entry
    // throws runtime_error if there are several entries with the same name
    string GetRequestValue(const string& name) const;

    void AddRequestValue(const string& name, const string& value);
    void RemoveRequestValues(const string& name);
    void ReplaceRequestValue(const string& name, const string& value);

    // program name access
    const string& GetSelfURL( void ) const;

private:
    CNcbiRegistry& x_GetConfig(void) const;
    CNcbiResource& x_GetResource(void) const;
    CCgiServerContext& x_GetServCtx(void) const;

    CCgiApplication& m_app;
    
    auto_ptr<CCgiRequest> m_request; // CGI request information
    CCgiResponse m_response; // CGI response information

    //head of message tree
    CNodeRef m_msg;

    // server context will be obtained from CCgiApp::LoadServerContext()
    auto_ptr<CCgiServerContext> m_srvCtx; // application defined context

    mutable string m_selfURL;

    friend class CCgiApplication;
}; 

#include <cgi/cgictx.inl>

END_NCBI_SCOPE

#endif // NCBI_CGI_CTX__HPP
