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
 * Authors:  Denis Vakatov, Andrei Gourianov,
 *           Eugene Vasilchenko, Anton Lavrentiev
 *
 * File Description:
 *   CNcbiException
 *   CExceptionReporter
 *   CExceptionReporterStream
 *   CErrnoException
 *   CParseException
 *   + initialization for the "unexpected"
 *
 */

#include <corelib/ncbiexpt.hpp>
#include <errno.h>
#include <string.h>
#include <stdio.h>

#include <stack>
#ifdef NCBI_OS_MSWIN
#  include <windows.h>
#endif


BEGIN_NCBI_SCOPE


/////////////////////////////////
// SetThrowTraceAbort
// DoThrowTraceAbort

static bool s_DoThrowTraceAbort = false; //if to abort() in DoThrowTraceAbort()
static bool s_DTTA_Initialized  = false; //if s_DoThrowTraceAbort is init'd

extern void SetThrowTraceAbort(bool abort_on_throw_trace)
{
    s_DTTA_Initialized = true;
    s_DoThrowTraceAbort = abort_on_throw_trace;
}

extern void DoThrowTraceAbort(void)
{
    if ( !s_DTTA_Initialized ) {
        const char* str = getenv(ABORT_ON_THROW);
        if (str  &&  *str)
            s_DoThrowTraceAbort = true;
        s_DTTA_Initialized  = true;
    }

    if ( s_DoThrowTraceAbort )
        abort();
}

extern void DoDbgPrint(const char* file, int line, const char* message)
{
    CNcbiDiag(file, line, eDiag_Trace) << message;
    DoThrowTraceAbort();
}

extern void DoDbgPrint(const char* file, int line, const string& message)
{
    CNcbiDiag(file, line, eDiag_Trace) << message;
    DoThrowTraceAbort();
}

extern void DoDbgPrint(const char* file, int line,
                       const char* msg1, const char* msg2)
{
    CNcbiDiag(file, line, eDiag_Trace) << msg1 << ": " << msg2;
    DoThrowTraceAbort();
}


/////////////////////////////////////////////////////////////////////////////
// CNcbiException implementation

bool CNcbiException::sm_BkgrEnabled=true;


CNcbiException::CNcbiException(const char* file, int line, EErrCode err_code,
                               const char* message,
                               const CNcbiException* prev_exception) throw()
    :   m_File(file),
        m_Line(line),
        m_ErrCode(err_code),
        m_Msg(message),
        m_InReporter(false)
{
    m_Predecessor = prev_exception ? prev_exception->x_Clone() : 0;
}


CNcbiException::CNcbiException(const CNcbiException& other) throw()
{
    x_Assign(other);
}


CNcbiException::~CNcbiException(void) throw()
{
    if (m_Predecessor) {
        delete m_Predecessor;
        m_Predecessor = 0;
    }
}

void CNcbiException::AddBacklog(const char* file,int line,const char* message)
{
    const CNcbiException* prev = m_Predecessor;
    m_Predecessor = x_Clone();
    if (prev) {
        delete prev;
    }
    m_File = file;
    m_Line = line;
    m_Msg = message;
}


// ---- report --------------

const char* CNcbiException::what(void) const throw()
{
    m_What = ReportAll();
    return m_What.c_str();    
}


void CNcbiException::Report(const char* file, int line,
                            CExceptionReporter* reporter) const
{
    if (reporter ) {
        reporter->Report(file, line, *this);
    }
    // unconditionally ...
    CExceptionReporter::ReportDefault(file, line, *this);
}


string CNcbiException::ReportAll(void) const
{
    // invert the order
    stack<const CNcbiException*> pile;
    const CNcbiException* pex;
    for (pex = this; pex; pex = pex->GetPredecessor()) {
        pile.push(pex);
    }
    ostrstream os;
    os << "NCBI C++ Exception: " << endl;
    for (; !pile.empty(); pile.pop()) {
        //indentation
        for (size_t cnt = (pile.size()-1)*2; cnt!=0; --cnt) {
            os.put(' ');
        }
        pile.top()->ReportStd(os);
        pile.top()->ReportExtra(os);
        os << endl;
    }
    os << '\0';
    if (sm_BkgrEnabled && !m_InReporter) {
        m_InReporter = true;
        CExceptionReporter::ReportDefault(0,0,*this);
        m_InReporter = false;
    }
    return os.str();
}


string CNcbiException::ReportThis(void) const
{
    ostrstream os;
    ReportStd(os);
    ReportExtra(os);
    os << '\0';
    return os.str();
}


void CNcbiException::ReportStd(ostream& out) const
{
    out <<
        GetFile() << "(" << GetLine() << ") : " <<
        GetType() << "::" << GetErrCodeString() << " : \"" <<
        GetMsg() << "\" ";
}

void CNcbiException::ReportExtra(ostream& /*out*/) const
{
    return;
}


const char* CNcbiException::GetErrCodeString(void) const
{
    switch (GetErrCode()) {
    case eUnknown: return "eUnknown";
    default:       return "eInvalid";
    }
}


CNcbiException::EErrCode CNcbiException::GetErrCode (void) const
{
    return typeid(*this) == typeid(CNcbiException) ?
        (CNcbiException::EErrCode) x_GetErrCode() :
        CNcbiException::eInvalid;
}


void CNcbiException::x_ReportToDebugger(void) const
{
#ifdef NCBI_OS_MSWIN
    bool prev = EnableBackgroundReporting(false);
    OutputDebugString(ReportAll().c_str());
    EnableBackgroundReporting(prev);
#endif
}


bool CNcbiException::EnableBackgroundReporting(bool enable)
{
    bool prev = sm_BkgrEnabled;
    sm_BkgrEnabled = enable;
    return prev;
}


const CNcbiException* CNcbiException::x_Clone(void) const
{
    return new CNcbiException(*this);
}


void CNcbiException::x_Assign(const CNcbiException& src)
{
    m_File    = src.m_File;
    m_Line    = src.m_Line;
    m_Msg     = src.m_Msg;
    m_Predecessor =
        src.m_Predecessor ? src.m_Predecessor->x_Clone() : 0;
    x_AssignErrCode(src);
}


void CNcbiException::x_AssignErrCode(const CNcbiException& src)
{
    m_ErrCode = typeid(*this) == typeid(src) ?
        src.m_ErrCode : CNcbiException::eInvalid;
}


void CNcbiException::x_InitErrCode(EErrCode err_code)
{
    m_ErrCode = err_code;
    if (m_ErrCode != eInvalid && !m_Predecessor) {
        x_ReportToDebugger();
    }
}


/////////////////////////////////////////////////////////////////////////////
// CExceptionReporter

const CExceptionReporter* CExceptionReporter::sm_DefHandler = 0;

bool CExceptionReporter::sm_DefEnabled = true;


CExceptionReporter::CExceptionReporter(void)
{
    return;
}


CExceptionReporter::~CExceptionReporter(void)
{
    return;
}


void CExceptionReporter::SetDefault(const CExceptionReporter* handler)
{
    sm_DefHandler = handler;
}


const CExceptionReporter* CExceptionReporter::GetDefault(void)
{
    return sm_DefHandler;
}


bool CExceptionReporter::EnableDefault(bool enable)
{
    bool prev = sm_DefEnabled;
    sm_DefEnabled = enable;
    return prev;
}


void CExceptionReporter::ReportDefault(const char* file, int line,
                                       const CNcbiException& ex)
{
    if ( !sm_DefEnabled )
        return;

    if ( sm_DefHandler ) {
        sm_DefHandler->Report(file, line, ex);
    } else {
        CNcbiDiag(file, line) << ex;
    }
}


/////////////////////////////////////////////////////////////////////////////
// CExceptionReporterStream


CExceptionReporterStream::CExceptionReporterStream(ostream& out)
    : m_Out(out)
{
    return;
}


CExceptionReporterStream::~CExceptionReporterStream(void)
{
    return;
}


void CExceptionReporterStream::Report(const char* file, int line,
                                      const CNcbiException& ex) const
{
    const CNcbiException* pex;
    m_Out << "NCBI C++ Exception";
    if (file) {
        m_Out << " at \"" << file << "\", line " << line; 
    }
    m_Out << ":" << endl;
    // invert the order
    stack<const CNcbiException*> pile;
    for (pex = &ex; pex; pex = pex->GetPredecessor()) {
        pile.push(pex);
    }
    for (; !pile.empty(); pile.pop()) {
        pex = pile.top();
        m_Out <<
            "    " << // indentation
            pex->GetType() << "::" << pex->GetErrCodeString() << " at \"" <<
            pex->GetFile() <<  "\", line " << pex->GetLine() << ": \"" <<
            pex->GetMsg()  << "\" ";
        pex->ReportExtra(m_Out);
        m_Out << endl;
    }
}


/////////////////////////////////////////////////////////////////////////////
//  CErrnoException

CErrnoException::CErrnoException(const string& what) THROWS_NONE
: runtime_error(what + ": " + ::strerror(errno)), m_Errno(errno)
{
}

CErrnoException::~CErrnoException(void) THROWS_NONE
{
}


/////////////////////////////////
//  CParseException

static string s_ComposeParse(const string& what, string::size_type pos)
{
    char s[32];
    ::sprintf(s, "%ld", (long)pos);
    string str;
    str.reserve(256);
    return str.append("{").append(s).append("} ").append(what);
}

CParseException::CParseException(const string& what, string::size_type pos)
    THROWS_NONE
: runtime_error(s_ComposeParse(what,pos)), m_Pos(pos)
{
}

CParseException::~CParseException(void) THROWS_NONE
{
}


END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.21  2002/06/26 18:38:04  gouriano
 * added CNcbiException class
 *
 * Revision 1.20  2002/04/11 21:08:02  ivanov
 * CVS log moved to end of the file
 *
 * Revision 1.19  2001/07/30 14:42:10  lavr
 * eDiag_Trace and eDiag_Fatal always print as much as possible
 *
 * Revision 1.18  2001/05/21 21:44:00  vakatov
 * SIZE_TYPE --> string::size_type
 *
 * Revision 1.17  2001/05/17 15:04:59  lavr
 * Typos corrected
 *
 * Revision 1.16  2000/11/16 23:52:41  vakatov
 * Porting to Mac...
 *
 * Revision 1.15  2000/04/04 22:30:26  vakatov
 * SetThrowTraceAbort() -- auto-set basing on the application
 * environment and/or registry
 *
 * Revision 1.14  1999/12/29 13:58:39  vasilche
 * Added THROWS_NONE.
 *
 * Revision 1.13  1999/12/28 21:04:18  vasilche
 * Removed three more implicit virtual destructors.
 *
 * Revision 1.12  1999/11/18 20:12:43  vakatov
 * DoDbgPrint() -- prototyped in both _DEBUG and NDEBUG
 *
 * Revision 1.11  1999/10/04 16:21:04  vasilche
 * Added full set of macros THROW*_TRACE
 *
 * Revision 1.10  1999/09/27 16:23:24  vasilche
 * Changed implementation of debugging macros (_TRACE, _THROW*, _ASSERT etc),
 * so that they will be much easier for compilers to eat.
 *
 * Revision 1.9  1999/05/04 00:03:13  vakatov
 * Removed the redundant severity arg from macro ERR_POST()
 *
 * Revision 1.8  1999/04/14 19:53:29  vakatov
 * + <stdio.h>
 *
 * Revision 1.7  1999/01/04 22:41:43  vakatov
 * Do not use so-called "hardware-exceptions" as these are not supported
 * (on the signal level) by UNIX
 * Do not "set_unexpected()" as it works differently on UNIX and MSVC++
 *
 * Revision 1.6  1998/12/28 17:56:37  vakatov
 * New CVS and development tree structure for the NCBI C++ projects
 *
 * Revision 1.3  1998/11/13 00:17:13  vakatov
 * [UNIX] Added handler for the unexpected exceptions
 *
 * Revision 1.2  1998/11/10 17:58:42  vakatov
 * [UNIX] Removed extra #define's (POSIX... and EXTENTIONS...)
 * Allow adding strings in CNcbiErrnoException(must have used "CC -xar"
 * instead of just "ar" when building a library;  otherwise -- link error)
 *
 * Revision 1.1  1998/11/10 01:20:01  vakatov
 * Initial revision(derived from former "ncbiexcp.cpp")
 *
 * ===========================================================================
 */
