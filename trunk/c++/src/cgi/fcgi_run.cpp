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
 * Author: Eugene Vasilchenko, Denis Vakatov
 *
 * File Description:
 *   Fast-CGI loop function -- used in "cgiapp.cpp"::CCgiApplication::Run().
 *   NOTE:  see also a stub function in "cgi_run.cpp".
 *
 * ---------------------------------------------------------------------------
 * $Log$
 * Revision 1.1  1999/12/17 03:55:03  vakatov
 * Initial revision
 *
 * ===========================================================================
 */

#include <cgi/cgiapp.hpp>

BEGIN_NCBI_SCOPE

#if !defined(HAVE_FASTCGI)

bool CCgiApplication::RunFastCGI(unsigned)
{
    _TRACE("CCgiApplication::RunFastCGI:  return (FastCGI is not supported)");
    return false;
}

#else  /* HAVE_FASTCGI */

# include "fcgibuf.hpp"
# include <corelib/ncbienv.hpp>
# include <corelib/ncbireg.hpp>
# include <cgi/cgictx.hpp>

# include <fcgiapp.h>
# include <sys/stat.h>
# include <errno.h>


static time_t s_GetModTime(const char* filename)
{
    struct stat st;
    if (stat(filename, &st) != 0) {
        ERR_POST("s_GetModTime(): " << strerror(errno));
        THROW1_TRACE(CErrnoException,
                     "Cannot get modification time of the CGI executable");
    }
    return st.st_mtime;
}


bool CCgiApplication::RunFastCGI(unsigned def_iter)
{
    // Is it run as a Fast-CGI or as a plain CGI?
    if ( FCGX_IsCGI() ) {
        _TRACE("CCgiApplication::RunFastCGI:  return (run as a plain CGI)");
        return false;
    }


    // Max. number of the Fast-CGI loop iterations
    int iterations;
    string param = GetConfig().Get("FastCGI", "Iterations");
    if ( param.empty() ) {
        iterations = def_iter;
    } else {
        try {
            iterations = NStr::StringToInt(param);
        } catch (exception& e) {
            ERR_POST("CCgiApplication::RunFastCGI:  invalid FastCGI:Iterations"
                     " config.parameter value: " << param);
            iterations = def_iter;
        }
    }
    _TRACE("CCgiApplication::Run: FastCGI limited to "
           << iterations << " iterations");


    // Main Fast-CGI loop
    time_t mtime = s_GetModTime(m_Argv[0]);
    for (int iteration = 0;  iteration < iterations;  ++iteration) {
        _TRACE("CCgiApplication::FastCGI: " << (iteration + 1)
               << " iteration of " << iterations);

        // accept the next request and obtain its data
        FCGX_Stream *pfin, *pfout, *pferr;
        FCGX_ParamArray penv;
        if ( FCGX_Accept(&pfin, &pfout, &pferr, &penv) != 0 ) {
            _TRACE("CCgiApplication::RunFastCGI: no more requests");
            break;
        }

        // default exit status (error)
        FCGX_SetExitStatus(-1, pfout);

        // process the request
        try {
            // initialize CGI context with the new request data
            CNcbiEnvironment env(penv);
            CCgiObuffer obuf(pfout, 512);              
            CNcbiOstream ostr(&obuf);
            CCgiIbuffer ibuf(pfin);
            CNcbiIstream istr(&ibuf);
            auto_ptr<CCgiContext> ctx(CreateContext(&env, &istr, &ostr));

            // checking for exit request
            if (ctx->GetRequest().GetEntries().find("exitfastcgi") !=
                ctx->GetRequest().GetEntries().end()) {
                ostr << "Content-Type: text/html\r\n\r\nDone";
                _TRACE("CCgiApplication::RunFastCGI: aborting by request");
                FCGX_Finish();
                break;
            }

            if ( !GetConfig().Get("FastCGI", "Debug").empty() ) {
                ctx->PutMsg("FastCGI: " + NStr::IntToString(iteration + 1) +
                            " iteration of " + NStr::IntToString(iterations));
            }

            // call ProcessRequest()
            _TRACE("CCgiApplication::Run: calling ProcessRequest()");
            FCGX_SetExitStatus(ProcessRequest(*ctx), pfout);
        }
        catch (exception& e) {
            ERR_POST("CCgiApplication::ProcessRequest() failed: " << e.what());
            // (ignore the error, proceed with the next iteration anyway)
        }

        // close current request
        _TRACE("CCgiApplication::RunFastCGI: FINISHING");
        FCGX_Finish();

        // check if this CGI executable has been changed
        time_t mtimeNew = s_GetModTime(m_Argv[0]);    
        if (mtimeNew != mtime) {
            _TRACE("CCgiApplication::RunFastCGI: "
                   "the program modification date has changed");
            break;
        }
    } // Main Fast-CGI loop

    // done
    _TRACE("CCgiApplication::RunFastCGI:  return(FastCGI loop finished)");
    return true;
}

#endif /* HAVE_FASTCGI */

END_NCBI_SCOPE
