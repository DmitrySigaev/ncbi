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
 *   CException
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
#  include <corelib/ncbi_os_mswin.hpp>
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
// CException implementation

bool CException::sm_BkgrEnabled=true;


CException::CException(const char* file, int line,
    const CException* prev_exception,
    EErrCode err_code, const string& message) throw()
    :   m_ErrCode(err_code),
        m_InReporter(false)
{
    m_Predecessor = 0;
    x_Init(file,line,message,prev_exception);
}


CException::CException(const CException& other) throw()
{
    m_Predecessor = 0;
    x_Assign(other);
}

CException::CException(void) throw()
{
// this only is called in case of multiple inheritance
    m_ErrCode = CException::eInvalid;
    m_Predecessor = 0;
    m_InReporter = false;
    m_Line = -1;
}


CException::~CException(void) throw()
{
    if (m_Predecessor) {
        delete m_Predecessor;
        m_Predecessor = 0;
    }
}

void CException::AddBacklog(const char* file,int line,
                            const string& message)
{
    const CException* prev = m_Predecessor;
    m_Predecessor = x_Clone();
    if (prev) {
        delete prev;
    }
    x_Init(file,line,message,0);
}


// ---- report --------------

const char* CException::what(void) const throw()
{
    m_What = ReportAll();
    return m_What.c_str();    
}


void CException::Report(const char* file, int line,
                        const string& title,CExceptionReporter* reporter,
                        TDiagPostFlags flags) const
{
    if (reporter ) {
        reporter->Report(file, line, title, *this, flags);
    }
    // unconditionally ... 
    // that is, there will be two reports
    CExceptionReporter::ReportDefault(file, line, title, *this, flags);
}


string CException::ReportAll(TDiagPostFlags flags) const
{
    // invert the order
    stack<const CException*> pile;
    const CException* pex;
    for (pex = this; pex; pex = pex->GetPredecessor()) {
        pile.push(pex);
    }
    ostrstream os;
    os << "NCBI C++ Exception: " << endl;
    for (; !pile.empty(); pile.pop()) {
        //indentation
        os << "    ";
        os << pile.top()->ReportThis(flags) << endl;
    }
    os << '\0';
    if (sm_BkgrEnabled && !m_InReporter) {
        m_InReporter = true;
        CExceptionReporter::ReportDefault(0,0,"(background reporting)",
                                          *this, eDPF_Trace);
        m_InReporter = false;
    }
    return CNcbiOstrstreamToString(os);
}


string CException::ReportThis(TDiagPostFlags flags) const
{
    ostrstream os, osex;
    ReportStd(os, flags);
    ReportExtra(osex);
    if (osex.pcount() != 0) {
        os << " (" << (string)CNcbiOstrstreamToString(osex) << ')';
    }
    return CNcbiOstrstreamToString(os);
}


void CException::ReportStd(ostream& out, TDiagPostFlags flags) const
{
    string text(GetMsg());
    string err_type(GetType());
    err_type += "::";
    err_type += GetErrCodeString();
    SDiagMessage diagmsg(
        eDiag_Error, text.c_str(), text.size(),
        GetFile().c_str(), GetLine(),
        flags, 0,0,0,err_type.c_str());
    diagmsg.Write(out, SDiagMessage::fNoEndl);
}

void CException::ReportExtra(ostream& /*out*/) const
{
    return;
}


const char* CException::GetErrCodeString(void) const
{
    switch (GetErrCode()) {
    case eUnknown: return "eUnknown";
    default:       return "eInvalid";
    }
}


CException::EErrCode CException::GetErrCode (void) const
{
    return typeid(*this) == typeid(CException) ?
        (CException::EErrCode) x_GetErrCode() :
        CException::eInvalid;
}


void CException::x_ReportToDebugger(void) const
{
#ifdef NCBI_OS_MSWIN
    ostrstream os;
    os << "NCBI C++ Exception: " << endl;
    os <<
        GetFile() << "(" << GetLine() << ") : " <<
        GetType() << "::" << GetErrCodeString() << " : \"" <<
        GetMsg() << "\" ";
    ReportExtra(os);
    os << endl << '\0';
    OutputDebugString(((string)CNcbiOstrstreamToString(os)).c_str());
#endif
    DoThrowTraceAbort();
}


bool CException::EnableBackgroundReporting(bool enable)
{
    bool prev = sm_BkgrEnabled;
    sm_BkgrEnabled = enable;
    return prev;
}

const CException* CException::x_Clone(void) const
{
    return new CException(*this);
}


void CException::x_Init(const string& file,int line,const string& message,
                        const CException* prev_exception)
{
    m_File = file;
    m_Line = line;
    m_Msg  = message;
    if (!m_Predecessor && prev_exception) {
        m_Predecessor = prev_exception->x_Clone();
    }
}


void CException::x_Assign(const CException& src)
{
    m_InReporter = false;
    x_Init(src.m_File, src.m_Line, src.m_Msg, src.m_Predecessor);
    x_AssignErrCode(src);
}


void CException::x_AssignErrCode(const CException& src)
{
    m_ErrCode = typeid(*this) == typeid(src) ?
        src.m_ErrCode : CException::eInvalid;
}


void CException::x_InitErrCode(EErrCode err_code)
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
    const string& title,const CException& ex, TDiagPostFlags flags)
{
    if ( !sm_DefEnabled )
        return;

    if ( sm_DefHandler ) {
        sm_DefHandler->Report(file, line, title, ex, flags);
    } else {
        CNcbiDiag(file, line, eDiag_Error, flags) << title << ex;
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
    const string& title, const CException& ex, TDiagPostFlags flags) const
{
    SDiagMessage diagmsg(
        eDiag_Error, title.c_str(), title.size(),
        file, line, flags);
    diagmsg.Write(m_Out);
    m_Out << "NCBI C++ Exception: " << endl;
    // invert the order
    stack<const CException*> pile;
    const CException* pex;
    for (pex = &ex; pex; pex = pex->GetPredecessor()) {
        pile.push(pex);
    }
    for (; !pile.empty(); pile.pop()) {
        pex = pile.top();
        m_Out << "    ";
        m_Out << pex->ReportThis(flags) << endl;
    }
}


#ifndef INLINE_CSTRERRADAPT
// We have to make this an external function with GCC 2.95 on OSF/1.
const char* CStrErrAdapt::strerror(int errnum)
{
    return ::strerror(errnum);
}
#endif



END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.32  2003/04/29 19:47:10  ucko
 * KLUDGE: avoid inlining CStrErrAdapt::strerror with GCC 2.95 on OSF/1,
 * due to a weird, apparently platform-specific, bug.
 *
 * Revision 1.31  2003/02/24 19:56:05  gouriano
 * use template-based exceptions instead of errno and parse exceptions
 *
 * Revision 1.30  2003/02/20 17:55:19  gouriano
 * added the possibility to abort a program on an attempt to throw CException
 *
 * Revision 1.29  2003/01/13 20:42:50  gouriano
 * corrected the problem with ostrstream::str(): replaced such calls with
 * CNcbiOstrstreamToString(os)
 *
 * Revision 1.28  2002/09/19 20:05:42  vasilche
 * Safe initialization of static mutexes
 *
 * Revision 1.27  2002/08/20 19:11:30  gouriano
 * added DiagPostFlags into CException reporting functions
 *
 * Revision 1.26  2002/07/29 19:29:42  gouriano
 * changes to allow multiple inheritance in CException classes
 *
 * Revision 1.24  2002/07/15 18:17:24  gouriano
 * renamed CNcbiException and its descendents
 *
 * Revision 1.23  2002/07/11 14:18:26  gouriano
 * exceptions replaced by CNcbiException-type ones
 *
 * Revision 1.22  2002/06/27 18:56:16  gouriano
 * added "title" parameter to report functions
 *
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
