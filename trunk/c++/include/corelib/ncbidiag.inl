#if defined(CORELIB___NCBIDIAG__HPP)  &&  !defined(CORELIB___NCBIDIAG__INL)
#define CORELIB___NCBIDIAG__INL

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
 *   NCBI C++ diagnostic API
 *
 */


/////////////////////////////////////////////////////////////////////////////
// WARNING -- all the beneath is for INTERNAL "ncbidiag" use only,
//            and any classes, typedefs and even "extern" functions and
//            variables declared in this file should not be used anywhere
//            but inside "ncbidiag.inl" and/or "ncbidiag.cpp"!!!
/////////////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////
// CDiagBuffer
// (can be accessed only by "CNcbiDiag" and "CDiagRestorer"
// and created only by GetDiagBuffer())
//

class CDiagBuffer
{
    CDiagBuffer(const CDiagBuffer&);
    CDiagBuffer& operator= (const CDiagBuffer&);

    friend CDiagBuffer& GetDiagBuffer(void);
    friend bool IsSetDiagPostFlag(EDiagPostFlag flag, TDiagPostFlags flags);
    friend TDiagPostFlags SetDiagPostAllFlags(TDiagPostFlags flags);
    friend void SetDiagPostFlag(EDiagPostFlag flag);
    friend void UnsetDiagPostFlag(EDiagPostFlag flag);
    friend void SetDiagPostPrefix(const char* prefix);
    friend void PushDiagPostPrefix(const char* prefix);
    friend void PopDiagPostPrefix();

    friend class CNcbiDiag;
    friend const CNcbiDiag& Reset(const CNcbiDiag& diag);
    friend const CNcbiDiag& Endm(const CNcbiDiag& diag);

    friend EDiagSev SetDiagPostLevel(EDiagSev post_sev);
    friend void SetDiagFixedPostLevel(const EDiagSev post_sev);
    friend bool DisableDiagPostLevelChange(bool disable_change);

    friend EDiagSev SetDiagDieLevel(EDiagSev die_sev);
    friend void SetDiagTrace(EDiagTrace how, EDiagTrace dflt);
    friend void SetDiagHandler(CDiagHandler* handler, bool can_delete);
    friend CDiagHandler* GetDiagHandler(bool take_ownership);
    friend bool IsDiagStream(const CNcbiOstream* os);
    friend bool IsSetDiagHandler(void);
private:
    friend class CDiagRestorer;

    const CNcbiDiag* m_Diag;    // present user
    CNcbiOstream*    m_Stream;  // storage for the diagnostic message

    // user-specified string to add to each posted message
    // (can be constructed from "m_PrefixList" after push/pop operations)
    string m_PostPrefix;

    // list of prefix strings to compose the "m_PostPrefix" from
    typedef list<string> TPrefixList;
    TPrefixList m_PrefixList;


    CDiagBuffer(void);

    //### This is a temporary workaround to allow call the destructor of
    //### static instance of "CDiagBuffer" defined in GetDiagBuffer()
public:
    ~CDiagBuffer(void);
private:
    //###

    // formatted output
    template<class X> void Put(const CNcbiDiag& diag, const X& x) {
        if ( SetDiag(diag) )
            (*m_Stream) << x;
    }

    void Flush  (void);
    void Reset  (const CNcbiDiag& diag);   // reset content of the diag. message
    void EndMess(const CNcbiDiag& diag);   // output current diag. message
    bool SetDiag(const CNcbiDiag& diag);

    // flush & detach the current user
    void Detach(const CNcbiDiag* diag);

    // compose the post prefix using "m_PrefixList"
    void UpdatePrefix(void);

    // the bitwise OR combination of "EDiagPostFlag"
    static TDiagPostFlags sm_PostFlags;

    // static members
    static EDiagSev       sm_PostSeverity;
    static EDiagSevChange sm_PostSeverityChange;
                                           // severity level changing status
    static EDiagSev       sm_DieSeverity;
    static EDiagTrace     sm_TraceDefault; // default state of tracing
    static bool           sm_TraceEnabled; // current state of tracing
                                           // (enable/disable)

    static bool GetTraceEnabled(void);     // dont access sm_TraceEnabled 
                                           // directly
    static bool GetTraceEnabledFirstTime(void);
    static bool GetSeverityChangeEnabledFirstTime(void);

    // call the current diagnostics handler directly
    static void DiagHandler(SDiagMessage& mess);

    // Symbolic name for the severity levels(used by CNcbiDiag::SeverityName)
    static const char* sm_SeverityName[eDiag_Trace+1];

    // Application-wide diagnostic handler
    static CDiagHandler* sm_Handler;
    static bool          sm_CanDeleteHandler;
};

extern CDiagBuffer& GetDiagBuffer(void);


///////////////////////////////////////////////////////
//  CNcbiDiag::

inline CNcbiDiag::~CNcbiDiag(void) {
    m_Buffer.Detach(this);
}

inline const CNcbiDiag& CNcbiDiag::SetLine(size_t line) const {
    m_Line = line;
    return *this;
}

inline const CNcbiDiag& CNcbiDiag::SetErrorCode(int code, int subcode) const {
    m_ErrCode = code;
    m_ErrSubCode = subcode;
    return *this;
}

inline EDiagSev CNcbiDiag::GetSeverity(void) const {
    return m_Severity;
}

inline const char* CNcbiDiag::GetFile(void) const {
    return m_File;
}

inline size_t CNcbiDiag::GetLine(void) const {
    return m_Line;
}

inline int CNcbiDiag::GetErrorCode(void) const {
    return m_ErrCode;
}

inline int CNcbiDiag::GetErrorSubCode(void) const {
    return m_ErrSubCode;
}

inline TDiagPostFlags CNcbiDiag::GetPostFlags(void) const {
    return m_PostFlags;
}


inline
const char* CNcbiDiag::SeverityName(EDiagSev sev) {
    return CDiagBuffer::sm_SeverityName[sev];
}


///////////////////////////////////////////////////////
//  ErrCode - class for manipulator ErrCode

class ErrCode
{
public:
    ErrCode(int code, int subcode = 0)
        : m_Code(code), m_SubCode(subcode)
    { }
    int m_Code;
    int m_SubCode;
};

inline
const CNcbiDiag& CNcbiDiag::operator<< (const ErrCode& err_code) const
{
    return SetErrorCode(err_code.m_Code, err_code.m_SubCode);
}


///////////////////////////////////////////////////////
//  Other CNcbiDiag:: manipulators

inline
const CNcbiDiag& Reset(const CNcbiDiag& diag)  {
    diag.m_Buffer.Reset(diag);
    return diag;
}

inline
const CNcbiDiag& Endm(const CNcbiDiag& diag)  {
    diag.m_Buffer.EndMess(diag);
    return diag;
}

inline
const CNcbiDiag& Info(const CNcbiDiag& diag)  {
    diag << Endm;
    diag.m_Severity = eDiag_Info;
    return diag;
}
inline
const CNcbiDiag& Warning(const CNcbiDiag& diag)  {
    diag << Endm;
    diag.m_Severity = eDiag_Warning;
    return diag;
}
inline
const CNcbiDiag& Error(const CNcbiDiag& diag)  {
    diag << Endm;
    diag.m_Severity = eDiag_Error;
    return diag;
}
inline
const CNcbiDiag& Critical(const CNcbiDiag& diag)  {
    diag << Endm;
    diag.m_Severity = eDiag_Critical;
    return diag;
}
inline
const CNcbiDiag& Fatal(const CNcbiDiag& diag)  {
    diag << Endm;
    diag.m_Severity = eDiag_Fatal;
    return diag;
}
inline
const CNcbiDiag& Trace(const CNcbiDiag& diag)  {
    diag << Endm;
    diag.m_Severity = eDiag_Trace;
    return diag;
}


///////////////////////////////////////////////////////
//  CDiagBuffer::

inline
void CDiagBuffer::Reset(const CNcbiDiag& diag) {
    if (&diag == m_Diag)
        m_Stream->rdbuf()->SEEKOFF(0, IOS_BASE::beg, IOS_BASE::out);
}

inline
void CDiagBuffer::EndMess(const CNcbiDiag& diag) {
    if (&diag == m_Diag)
        Flush();
}

inline
void CDiagBuffer::Detach(const CNcbiDiag* diag) {
    if (diag == m_Diag) {
        Flush();
        m_Diag = 0;
    }
}

inline
bool CDiagBuffer::GetTraceEnabled(void) {
    return (sm_TraceDefault == eDT_Default) ?
        GetTraceEnabledFirstTime() : sm_TraceEnabled;
}


///////////////////////////////////////////////////////
//  EDiagPostFlag::

inline
bool IsSetDiagPostFlag(EDiagPostFlag flag, TDiagPostFlags flags) {
    if (flags & eDPF_Default)
        flags = CDiagBuffer::sm_PostFlags;
    return (flags & flag) != 0;
}



///////////////////////////////////////////////////////
//  CDiagMessage::

inline
SDiagMessage::SDiagMessage(EDiagSev severity,
                           const char* buf, size_t len,
                           const char* file, size_t line,
                           TDiagPostFlags flags, const char* prefix,
                           int err_code, int err_subcode,
                           const char* err_text)
{
    m_Severity   = severity;
    m_Buffer     = buf;
    m_BufferLen  = len;
    m_File       = file;
    m_Line       = line;
    m_Flags      = flags;
    m_Prefix     = prefix;
    m_ErrCode    = err_code;
    m_ErrSubCode = err_subcode;
    m_ErrText    = err_text;
}


/*
 * ===========================================================================
 * $Log$
 * Revision 1.33  2002/07/10 16:18:43  ivanov
 * Added CNcbiDiag::StrToSeverityLevel().
 * Rewrite and rename SetDiagFixedStrPostLevel() -> SetDiagFixedPostLevel()
 *
 * Revision 1.32  2002/07/09 16:38:00  ivanov
 * Added GetSeverityChangeEnabledFirstTime().
 * Fix usage forced set severity post level from environment variable
 * to work without NcbiApplication::AppMain()
 *
 * Revision 1.31  2002/07/02 18:26:08  ivanov
 * Added CDiagBuffer::DisableDiagPostLevelChange()
 *
 * Revision 1.30  2002/06/26 18:36:37  gouriano
 * added CNcbiException class
 *
 * Revision 1.29  2002/04/23 19:57:26  vakatov
 * Made the whole CNcbiDiag class "mutable" -- it helps eliminate
 * numerous warnings issued by SUN Forte6U2 compiler.
 * Do not use #NO_INCLASS_TMPL anymore -- apparently all modern
 * compilers seem to be supporting in-class template methods.
 *
 * Revision 1.28  2002/04/11 20:39:17  ivanov
 * CVS log moved to end of the file
 *
 * Revision 1.27  2002/02/13 22:39:13  ucko
 * Support AIX.
 *
 * Revision 1.26  2002/02/07 19:45:53  ucko
 * Optionally transfer ownership in GetDiagHandler.
 *
 * Revision 1.25  2001/11/14 15:14:59  ucko
 * Revise diagnostic handling to be more object-oriented.
 *
 * Revision 1.24  2001/10/29 15:16:11  ucko
 * Preserve default CGI diagnostic settings, even if customized by app.
 *
 * Revision 1.23  2001/10/16 23:44:05  vakatov
 * + SetDiagPostAllFlags()
 *
 * Revision 1.22  2001/06/14 03:37:28  vakatov
 * For the ErrCode manipulator, use CNcbiDiag:: method rather than a friend
 *
 * Revision 1.21  2001/06/13 23:19:36  vakatov
 * Revamped previous revision (prefix and error codes)
 *
 * Revision 1.20  2001/06/13 20:51:52  ivanov
 * + PushDiagPostPrefix(), PopPushDiagPostPrefix() - stack post prefix messages.
 * + ERR_POST_EX, LOG_POST_EX - macros for posting with error codes.
 * + ErrCode(code[,subcode]) - CNcbiDiag error code manipulator.
 * + eDPF_ErrCode, eDPF_ErrSubCode - new post flags.
 *
 * Revision 1.19  2001/05/17 14:51:04  lavr
 * Typos corrected
 *
 * Revision 1.18  2000/04/04 22:31:57  vakatov
 * SetDiagTrace() -- auto-set basing on the application
 * environment and/or registry
 *
 * Revision 1.17  2000/02/18 16:54:04  vakatov
 * + eDiag_Critical
 *
 * Revision 1.16  2000/01/20 16:52:30  vakatov
 * SDiagMessage::Write() to replace SDiagMessage::Compose()
 * + operator<<(CNcbiOstream& os, const SDiagMessage& mess)
 * + IsSetDiagHandler(), IsDiagStream()
 *
 * Revision 1.15  1999/12/29 22:30:22  vakatov
 * Use "exit()" rather than "abort()" in non-#_DEBUG mode
 *
 * Revision 1.14  1999/12/28 18:55:24  vasilche
 * Reduced size of compiled object files:
 * 1. avoid inline or implicit virtual methods (especially destructors).
 * 2. avoid std::string's methods usage in inline methods.
 * 3. avoid string literals ("xxx") in inline methods.
 *
 * Revision 1.13  1999/09/27 16:23:20  vasilche
 * Changed implementation of debugging macros (_TRACE, _THROW*, _ASSERT etc),
 * so that they will be much easier for compilers to eat.
 *
 * Revision 1.12  1999/05/14 16:23:18  vakatov
 * CDiagBuffer::Reset: easy fix
 *
 * Revision 1.11  1999/05/12 21:11:42  vakatov
 * Minor fixes to compile by the Mac CodeWarrior C++ compiler
 *
 * Revision 1.10  1999/05/04 00:03:07  vakatov
 * Removed the redundant severity arg from macro ERR_POST()
 *
 * Revision 1.9  1999/04/30 19:20:58  vakatov
 * Added more details and more control on the diagnostics
 * See #ERR_POST, EDiagPostFlag, and ***DiagPostFlag()
 *
 * Revision 1.8  1998/12/30 21:52:17  vakatov
 * Fixed for the new SunPro 5.0 beta compiler that does not allow friend
 * templates and member(in-class) templates
 *
 * Revision 1.7  1998/11/05 00:00:42  vakatov
 * Fix in CDiagBuffer::Reset() to avoid "!=" ambiguity when using
 * new(templated) streams
 *
 * Revision 1.6  1998/11/04 23:46:36  vakatov
 * Fixed the "ncbidbg/diag" header circular dependencies
 *
 * Revision 1.5  1998/11/03 22:28:33  vakatov
 * Renamed Die/Post...Severity() to ...Level()
 *
 * Revision 1.4  1998/11/03 20:51:24  vakatov
 * Adaptation for the SunPro compiler glitchs(see conf. #NO_INCLASS_TMPL)
 *
 * Revision 1.3  1998/10/30 20:08:25  vakatov
 * Fixes to (first-time) compile and test-run on MSVS++
 * ==========================================================================
 */

#endif /* def CORELIB___NCBIDIAG__HPP  &&  ndef CORELIB___NCBIDIAG__INL */
