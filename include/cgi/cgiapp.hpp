#ifndef NCBI_CGI_APP__HPP
#define NCBI_CGI_APP__HPP

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
* Authors:
*	Vsevolod Sandomirskiy, Aaron Ucko, Denis Vakatov, Anatoliy Kuznetsov
*
* File Description:
*   Base class for (Fast-)CGI applications
*/

#include <corelib/ncbiapp.hpp>
#include <corelib/ncbireg.hpp>
#include <corelib/ncbitime.hpp>
#include <cgi/ncbicgi.hpp>
#include <cgi/ncbicgir.hpp>
#include <cgi/ncbires.hpp>


BEGIN_NCBI_SCOPE

class CCgiServerContext;
class CCgiStatistics;


/////////////////////////////////////////////////////////////////////////////
//  CCgiApplication::
//

class CCgiApplication : public CNcbiApplication
{
    friend class CCgiStatistics;
    typedef CNcbiApplication CParent;

public:
    CCgiApplication(void);
    ~CCgiApplication(void);

    static CCgiApplication* Instance(void); // Singleton method

    // These methods will throw exception if no server context set
    const CCgiContext& GetContext(void) const  { return x_GetContext(); }
    CCgiContext&       GetContext(void)        { return x_GetContext(); }

    // These methods will throw exception if no resource is set
    const CNcbiResource& GetResource(void) const { return x_GetResource(); }
    CNcbiResource&       GetResource(void)       { return x_GetResource(); }

    virtual void Init(void);  // initialization
    virtual void Exit(void);  // cleanup

    virtual int Run(void);
    virtual int ProcessRequest(CCgiContext& context) = 0;

    virtual CNcbiResource*     LoadResource(void);
    virtual CCgiServerContext* LoadServerContext(CCgiContext& context);

protected:
    // Factory method for the Context object construction
    virtual CCgiContext*   CreateContext(CNcbiArguments*   args = 0,
                                         CNcbiEnvironment* env  = 0,
                                         CNcbiIstream*     inp  = 0,
                                         CNcbiOstream*     out  = 0,
                                         int               ifd  = -1,
                                         int               ofd  = -1);

    void                   RegisterDiagFactory(const string& key,
                                               CDiagFactory* fact);
    CDiagFactory*          FindDiagFactory(const string& key);

    virtual void           ConfigureDiagnostics    (CCgiContext& context);
    virtual void           ConfigureDiagDestination(CCgiContext& context);
    virtual void           ConfigureDiagThreshold  (CCgiContext& context);
    virtual void           ConfigureDiagFormat     (CCgiContext& context);

    // Analyze registry settings ([CGI] Log) and return current logging option
    enum ELogOpt {
        eNoLog,
        eLog,
        eLogOnError
    };
    ELogOpt GetLogOpt(void) const;

    // Class factory for statistics class
    virtual CCgiStatistics* CreateStat();

private:
    // If FastCGI-capable, and run as a Fast-CGI then iterate through
    // the FastCGI loop -- doing initialization and running ProcessRequest()
    // time after time; then return TRUE.
    // Return FALSE overwise.
    // In the "result", return exit code of the last CGI iteration run.
    bool x_RunFastCGI(int* result, unsigned int def_iter = 3);

    // Logging
    enum ELogPostFlags {
        fBegin = 0x1,
        fEnd   = 0x2
    };
    typedef int TLogPostFlags;  // binary OR of "ELogPostFlags"
    void x_LogPost(const char*             msg_header,
                   unsigned int            iteration,
                   const CTime&            start_time,
                   const CNcbiEnvironment* env,
                   TLogPostFlags           flags)
        const;

    CCgiContext&   x_GetContext (void) const;
    CNcbiResource& x_GetResource(void) const;

    auto_ptr<CNcbiResource>   m_Resource;
    auto_ptr<CCgiContext>     m_Context;

    typedef map<string, CDiagFactory*> TDiagFactoryMap;
    TDiagFactoryMap           m_DiagFactories;
};



/////////////////////////////////////////////////////////////////////////////
//  CCgiStatistics::
//
//    CGI statistics information
//

class CCgiStatistics
{
    friend class CCgiApplication;
public:
    virtual ~CCgiStatistics();

protected:
    CCgiStatistics(CCgiApplication& cgi_app);

    // Reset statistics class. Method called only ones for CGI
    // applications and every iteration if it is FastCGI.
    virtual void Reset(const CTime& start_time,
                       int          result,
                       const char*  err_msg = 0);

    // Compose message for statistics logging.
    // This default implementation constructs the message from the fragments
    // composed with the help of "Compose_Xxx()" methods (see below).
    // NOTE:  It can return empty string (when time cut-off is engaged).
    virtual string Compose(void);

    // Log the message
    virtual void   Submit(const string& message);

protected:
    virtual string Compose_ProgramName (void);
    virtual string Compose_Timing      (const CTime& end_time);
    virtual string Compose_Entries     (void);
    virtual string Compose_Result      (void);
    virtual string Compose_ErrMessage  (void);

protected:
    CCgiApplication& m_CgiApp;     // Reference on the "mother app"
    string           m_LogDelim;   // Log delimiter
    CTime            m_StartTime;  // CGI start time
    int              m_Result;     // Return code
    string           m_ErrMsg;     // Error message
};


END_NCBI_SCOPE

/*
* ===========================================================================
*
* $Log$
* Revision 1.30  2003/02/05 01:21:43  ucko
* "friend" -> "friend class" for GCC 3; CVS log -> end.
*
* Revision 1.29  2003/02/04 21:27:13  kuznets
* + Implementation of statistics logging
*
* Revision 1.28  2003/01/23 19:58:40  kuznets
* CGI logging improvements
*
* Revision 1.27  2001/11/19 15:20:16  ucko
* Switch CGI stuff to new diagnostics interface.
*
* Revision 1.26  2001/10/31 15:30:19  golikov
* warning removed
*
* Revision 1.25  2001/10/17 14:18:04  ucko
* Add CCgiApplication::SetCgiDiagHandler for the benefit of derived
* classes that overload ConfigureDiagDestination.
*
* Revision 1.24  2001/10/05 14:56:20  ucko
* Minor interface tweaks for CCgiStreamDiagHandler and descendants.
*
* Revision 1.23  2001/10/04 18:17:51  ucko
* Accept additional query parameters for more flexible diagnostics.
* Support checking the readiness of CGI input and output streams.
*
* Revision 1.22  2001/06/13 21:04:35  vakatov
* Formal improvements and general beautifications of the CGI lib sources.
*
* Revision 1.21  2001/01/12 21:58:25  golikov
* cgicontext available from cgiapp
*
* Revision 1.20  2000/01/20 17:54:55  vakatov
* CCgiApplication:: constructor to get CNcbiArguments, and not raw argc/argv.
* SetupDiag_AppSpecific() to override the one from CNcbiApplication:: -- lest
* to write the diagnostics to the standard output.
*
* Revision 1.19  1999/12/17 04:06:20  vakatov
* Added CCgiApplication::RunFastCGI()
*
* Revision 1.18  1999/11/15 15:53:19  sandomir
* Registry support moved from CCgiApplication to CNcbiApplication
*
* Revision 1.17  1999/05/14 19:21:48  pubmed
* myncbi - initial version; minor changes in CgiContext, history, query
*
* Revision 1.15  1999/05/06 20:32:47  pubmed
* CNcbiResource -> CNcbiDbResource; utils from query; few more context methods
*
* Revision 1.14  1999/04/30 16:38:08  vakatov
* #include <ncbireg.hpp> to provide CNcbiRegistry class definition(see R1.13)
*
* Revision 1.13  1999/04/27 17:01:23  vakatov
* #include <ncbires.hpp> to provide CNcbiResource class definition
* for the "auto_ptr<CNcbiResource>" (required by "delete" under MSVC++)
*
* Revision 1.12  1999/04/27 14:49:46  vasilche
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

#endif // NCBI_CGI_APP__HPP
