#ifndef CONNECT___NCBI_CONN_TEST__HPP
#define CONNECT___NCBI_CONN_TEST__HPP

/* $Id$
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
 * Author:  Anton Lavrentiev
 *
 * @file
 * File Description:
 *   NCBI connectivity test suite.
 *
 */

#include <corelib/ncbistd.hpp>
#include <connect/ncbi_conn_stream.hpp>
#include <connect/ncbi_core_cxx.hpp>
#include <ostream>
#include <string>
#include <vector>

/** @addtogroup Utility
 *
 * @{
 */


BEGIN_NCBI_SCOPE


class NCBI_XCONNECT_EXPORT CConnTest : virtual public CConnIniter
{
public:
    /// Note that each stage has a previous one as a prerequisite with the
    /// only exception for the stateful service (last check) that may work
    /// if forced into the stateless mode when firewall connections
    /// (in preceding check) have been found non-operational...
    enum EStage {
        eHttp,                  ///< Check whether HTTP works
        eDispatcher,            ///< Check whether NCBI dispatcher works
        eStatelessService,      ///< Check whether simplest NCBI service works
        eFirewallConnPoints,    ///< Obtain all FW ports for stateful services
        eFirewallConnections,   ///< Check all FW ports one by one
        eStatefulService        ///< Check whether NCBI stateful service works
    };

    /// Default timeout is 30 seconds;  the tests produce no output.
    ///
    CConnTest(const STimeout* timeout = kDefaultTimeout, ostream* out = 0);

    virtual ~CConnTest() { }

    /// Execute test suite from the very first (eHttp) up to and
    /// including the requested stage "stage".
    /// Return eIO_Success if all tests completed successfully;
    /// other code if not, and then also return an explanatory
    /// message at "*reason" (if "reason" passed non-NULL).
    /// It is expected that the call advances to the next check only
    /// if the previous one was successful (or conditionally successful,
    /// meaning even though it may have formally failed, it still returns
    /// eIO_Success and creates favorable preconditions for the following
    /// check to likely succeed).
    ///
    virtual EIO_Status Execute(EStage& stage, string* reason = 0);

protected:

    /// Auxiliary class to hold FWDaemon CP(connection point)
    /// information and its current boolean status.
    /// Depending on the check progress !okay may mean the CP has
    /// been marked FAIL at NCBI end at an earlier stage of testing,
    /// as well as turn "false" at a later stage is the actual connection
    /// attempt to use (otherwise okay CP) fails.
    ///
    /// m_Fwd holds the current list of CPs sorted by the port number.
    ///
    struct CFWConnPoint {
        unsigned int   host;  ///< Network byte order
        unsigned short port;  ///< Host byte order
        bool           okay;  ///< True if okay

        bool operator < (const CFWConnPoint& p) const
        { return port < p.port; }
    };


    /// User-redefinable checks for each stage.
    /// Every check must include at least one call of PreCheck()
    /// followed by PostCheck() with parameter "step" set to 0
    /// (meaning the "main" check); and as many as optional substeps
    /// (nested or going back to back) enumerated with "step".
    /// Each check returning eIO_Success means that its main check
    /// with all substeps have successfully passed.  A failing check
    /// can return an explanation via the "reason" pointer (if non-NULL)
    /// or at least clear the string.

    virtual EIO_Status HttpOkay          (string* reason);
    virtual EIO_Status DispatcherOkay    (string* reason);
    virtual EIO_Status ServiceOkay       (string* reason);
    virtual EIO_Status GetFWConnections  (string* reason);
    virtual EIO_Status CheckFWConnections(string* reason);
    virtual EIO_Status StatefulOkay      (string* reason);

    /// User-defined rendering callbacks:  PreCheck() and PostCheck().
    /// Each callback receives a stage enumerator and a step within.
    /// At least one step (0) is required and denotes the main check.

    /// PreCheck gets called before the step starts, and contains
    /// either:
    ///   a single-lined step title; or
    ///   a multi-lined step description, with first line being the actual
    ///   title, and remaining lines -- an explanation.
    /// Lines are separated with "\n", and normally do not have any
    /// ending punctuation (but may be capitalized).
    /// The default callback does the following:
    ///   For the single-lined titles, if outputs the title into the output
    ///     stream (if provided in ctor), and then puts the ellipsis (...)
    ///     without ending newline;
    ///   For the multi-lined description, the title is printed on
    ///     the first line, and the each line of the description follows
    ///     as a justified paragraph.  Last paragraph ends with a new line.
    /// Each PreCheck() is expected to reset the m_End member to "false".
    ///
    virtual void       PreCheck (EStage stage, unsigned int step,
                                 const string& title);

    /// PostCheck gets called upon successful ("status" contains eIO_Success)
    /// or unsuccessful (otherwise) completion of any step, with "reason"
    /// having an explanation in either case.  Successful completion
    /// expected to supply only a single line via the "reason" parameter;
    /// while a failing check can supply multiple lines (as an extended
    /// detailed explanation) separated with "\n".
    /// The default callback does the following:
    ///   For succeeding check it prints the contents of "reason" and returns;
    ///   For failing check, it prints the word "FAILED" followed by
    ///    textual representation of "status" and return value of
    ///    GetCheckPoint() if non-empty.  It then prints explanation (found
    ///    in reason) as a numbered list of justified paragraphs.  If there
    ///    is only a single line found, it won't be enumerated.
    /// Each PostCheck() is expected to set the m_End member to "true",
    /// so the nested or back-to-back substeps can be distiguished by
    /// the order of encounter of m_End's value.
    ///
    virtual void       PostCheck(EStage stage, unsigned int step,
                                 EIO_Status status, const string& reason);

    /// Helper function that assures to return eIO_Success if predicate
    /// "failure" is false;  and other code otherwise (taken from
    /// the underlying CONN object of the passed "io", or eIO_Unknown
    /// as a fallback).  Also, it sets m_CheckPoint member to contain
    /// the connection description if available (retrievable with
    /// GetCheckPoint()).
    ///
    ///
    virtual EIO_Status ConnStatus(bool failure, CConn_IOStream& io);

    /// Extended info of the last step IO
    const string&      GetCheckPoint(void) const { return m_CheckPoint; }

    /// As supplied in constructor
    static const STimeout kTimeout;
    const STimeout*       m_Timeout;
    ostream*              m_Out;

    /// Certain properties of communication as determined from configuration
    bool                  m_HttpProxy;
    bool                  m_Stateless;
    bool                  m_Firewall;
    bool                  m_FWProxy;
    bool                  m_Forced;

    vector<CFWConnPoint>  m_Fwd;

    bool                  m_End;

private:
    string                m_CheckPoint;
    STimeout              m_TimeoutValue;
};


END_NCBI_SCOPE


/* @} */

#endif  /* CONNECT___NCBI_CONN_TEST__HPP */
