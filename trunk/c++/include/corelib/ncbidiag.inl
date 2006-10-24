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

    // Flags
    friend bool IsSetDiagPostFlag(EDiagPostFlag flag, TDiagPostFlags flags);
    NCBI_XNCBI_EXPORT
    friend TDiagPostFlags         SetDiagPostAllFlags(TDiagPostFlags flags);
    NCBI_XNCBI_EXPORT friend void SetDiagPostFlag(EDiagPostFlag flag);
    NCBI_XNCBI_EXPORT friend void UnsetDiagPostFlag(EDiagPostFlag flag);
    NCBI_XNCBI_EXPORT
    friend TDiagPostFlags         SetDiagTraceAllFlags(TDiagPostFlags flags);
    NCBI_XNCBI_EXPORT friend void SetDiagTraceFlag(EDiagPostFlag flag);
    NCBI_XNCBI_EXPORT friend void UnsetDiagTraceFlag(EDiagPostFlag flag);
    NCBI_XNCBI_EXPORT friend void SetDiagPostPrefix(const char* prefix);
    NCBI_XNCBI_EXPORT friend void PushDiagPostPrefix(const char* prefix);
    NCBI_XNCBI_EXPORT friend void PopDiagPostPrefix(void);

    //
    friend class CNcbiDiag;
    friend const CNcbiDiag& Reset(const CNcbiDiag& diag);
    friend const CNcbiDiag& Endm(const CNcbiDiag& diag);

    // Severity
    NCBI_XNCBI_EXPORT
    friend EDiagSev SetDiagPostLevel(EDiagSev post_sev);
    NCBI_XNCBI_EXPORT
    friend void SetDiagFixedPostLevel(EDiagSev post_sev);
    NCBI_XNCBI_EXPORT
    friend bool DisableDiagPostLevelChange(bool disable_change);
    NCBI_XNCBI_EXPORT
    friend EDiagSev SetDiagDieLevel(EDiagSev die_sev);
    NCBI_XNCBI_EXPORT
    friend bool IgnoreDiagDieLevel(bool ignore);

    // Others
    NCBI_XNCBI_EXPORT
    friend void SetDiagTrace(EDiagTrace how, EDiagTrace dflt);
    NCBI_XNCBI_EXPORT friend bool IsDiagStream(const CNcbiOstream* os);

    // Handler
    NCBI_XNCBI_EXPORT friend void
    SetDiagHandler(CDiagHandler* handler, bool can_delete);
    NCBI_XNCBI_EXPORT friend CDiagHandler* GetDiagHandler(bool take_ownership);
    NCBI_XNCBI_EXPORT friend bool IsSetDiagHandler(void);

    // Error code information
    NCBI_XNCBI_EXPORT
    friend void SetDiagErrCodeInfo(CDiagErrCodeInfo* info, bool can_delete);
    NCBI_XNCBI_EXPORT
    friend CDiagErrCodeInfo* GetDiagErrCodeInfo(bool take_ownership);
    NCBI_XNCBI_EXPORT
    friend bool IsSetDiagErrCodeInfo(void);

private:
    friend class CDiagRestorer;
    friend class CDiagContext;

    const CNcbiDiag*   m_Diag;    // present user
    CNcbiOstream*      m_Stream;  // storage for the diagnostic message
    IOS_BASE::fmtflags m_InitialStreamFlags;

    // user-specified string to add to each posted message
    // (can be constructed from "m_PrefixList" after push/pop operations)
    string m_PostPrefix;

    // list of prefix strings to compose the "m_PostPrefix" from
    typedef list<string> TPrefixList;
    TPrefixList m_PrefixList;

    typedef int TTID;
    typedef int TPID;
    // Cached process ID
    TPID m_PID;
    // Cached thread ID
    TTID m_TID;
    // Count of posted messages for the thread
    int  m_ThreadPostCount;

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

    NCBI_XNCBI_EXPORT
    void Flush  (void);
    void Reset  (const CNcbiDiag& diag);   // reset content of the diag.message
    void EndMess(const CNcbiDiag& diag);   // output current diag. message
    NCBI_XNCBI_EXPORT
    bool SetDiag(const CNcbiDiag& diag);

    // flush & detach the current user
    void Detach(const CNcbiDiag* diag);

    // compose the post prefix using "m_PrefixList"
    void UpdatePrefix(void);

    // the bitwise OR combination of "EDiagPostFlag"
    // Hidden inside the function to adjust default flags depending on
    // registry/environment.
    // inline version
    static TDiagPostFlags& sx_GetPostFlags(void);
    // non-inline version
    NCBI_XNCBI_EXPORT
    static TDiagPostFlags& s_GetPostFlags(void);
    // extra flags ORed in for traces
    static TDiagPostFlags sm_TraceFlags;

    // static members
    static EDiagSev       sm_PostSeverity;
    static EDiagSevChange sm_PostSeverityChange;
                                           // severity level changing status
    static bool           sm_IgnoreToDie;
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
    NCBI_XNCBI_EXPORT
    static const char* sm_SeverityName[eDiag_Trace+1];

    // Application-wide diagnostic handler
    static CDiagHandler* sm_Handler;
    static bool          sm_CanDeleteHandler;

    // Error codes info
    static CDiagErrCodeInfo* sm_ErrCodeInfo;
    static bool              sm_CanDeleteErrCodeInfo;

    // Increment process post number, return the new value
    static int GetProcessPostNumber(bool inc);
};

extern CDiagBuffer& GetDiagBuffer(void);


///////////////////////////////////////////////////////
//  CDiagCompileInfo

inline const char* CDiagCompileInfo::GetFile (void) const 
{ 
    return m_File; 
}

inline const char* CDiagCompileInfo::GetModule(void) const 
{ 
    return m_Module; 
}

inline int CDiagCompileInfo::GetLine(void) const 
{ 
    return m_Line;                   
}

inline const string& CDiagCompileInfo::GetClass (void) const 
{ 
    if (!m_Parsed) {
        ParseCurrFunctName();
    }
    return m_ClassName;
}

inline const string& CDiagCompileInfo::GetFunction(void) const 
{ 
    if (!m_Parsed) {
        ParseCurrFunctName();
    }
    return m_FunctName;
}


///////////////////////////////////////////////////////
//  CNcbiDiag::

#ifdef NCBIDIAG_DEFER_GENERIC_PUT
template<class X>
inline
const CNcbiDiag& CNcbiDiag::Put(const void*, const X& x) const
{
    m_Buffer.Put(*this, x);
    return *this;
}
#endif


inline const CNcbiDiag& CNcbiDiag::operator<< (FIosManip manip) const
{
    m_Buffer.Put(*this, manip);
    return *this;
}


inline const CNcbiDiag& CNcbiDiag::SetLine(size_t line) const {
    m_Line = line;
    m_ValChngFlags |= fLineIsChanged;
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

inline const char* CNcbiDiag::GetModule(void) const 
{ 
    if (m_ValChngFlags & fModuleIsChanged) {
        return m_Module.c_str();
    }
    return m_CompileInfo.GetModule();
}

inline const char* CNcbiDiag::GetFile(void) const 
{ 
    if (m_ValChngFlags & fFileIsChanged) {
        return m_File.c_str();
    }
    return m_CompileInfo.GetFile();
}

inline const char* CNcbiDiag::GetClass(void) const 
{ 
    if (m_ValChngFlags & fClassIsChanged) {
        return m_Class.c_str();
    }
    return m_CompileInfo.GetClass().c_str();
}

inline const char* CNcbiDiag::GetFunction(void) const 
{ 
    if (m_ValChngFlags & fFunctionIsChanged) {
        return m_Function.c_str();
    }
    return m_CompileInfo.GetFunction().c_str();
}

inline size_t CNcbiDiag::GetLine(void) const {
    if (m_ValChngFlags & fLineIsChanged) {
        return m_Line;
    }
    return m_CompileInfo.GetLine();
}

inline int CNcbiDiag::GetErrorCode(void) const {
    return m_ErrCode;
}

inline int CNcbiDiag::GetErrorSubCode(void) const {
    return m_ErrSubCode;
}

inline TDiagPostFlags CNcbiDiag::GetPostFlags(void) const {
    return (m_PostFlags & eDPF_Default) ?
        (m_PostFlags | CDiagBuffer::s_GetPostFlags()) & ~eDPF_Default :
        m_PostFlags;
}


inline
const char* CNcbiDiag::SeverityName(EDiagSev sev) {
    return CDiagBuffer::sm_SeverityName[sev];
}


///////////////////////////////////////////////////////
//  ErrCode - class for manipulator ErrCode

inline
const CNcbiDiag& CNcbiDiag::Put(const ErrCode*, const ErrCode& err_code) const
{
    return SetErrorCode(err_code.m_Code, err_code.m_SubCode);
}

inline
bool operator< (const ErrCode& ec1, const ErrCode& ec2)
{
    return (ec1.m_Code == ec2.m_Code)
        ? (ec1.m_SubCode < ec2.m_SubCode)
        : (ec1.m_Code < ec2.m_Code);
}


inline
void CNcbiDiag::x_EndMess(void) const
{
    m_Buffer.EndMess(*this);
    m_Buffer.SetDiag(*this);
}


///////////////////////////////////////////////////////
//  Other CNcbiDiag:: manipulators

inline
const CNcbiDiag& CNcbiDiag::Put(const Severity*,
                                const Severity& severity) const
{
    m_Severity = severity.m_Level;
    return *this;
}

inline
const CNcbiDiag& Reset(const CNcbiDiag& diag)  {
    diag.m_Buffer.Reset(diag);
    diag.SetErrorCode(0, 0);
    return diag;
}

inline
const CNcbiDiag& Endm(const CNcbiDiag& diag)  {
    if ( !diag.m_Buffer.m_Diag
        && (diag.GetErrorCode() || diag.GetErrorSubCode()) ) {
        diag.m_Buffer.SetDiag(diag);
    }
    diag.m_Buffer.EndMess(diag);
    return diag;
}

inline
const CNcbiDiag& Info(const CNcbiDiag& diag)  {
    diag.x_EndMess();
    diag.m_Severity = eDiag_Info;
    return diag;
}
inline
const CNcbiDiag& Warning(const CNcbiDiag& diag)  {
    diag.x_EndMess();
    diag.m_Severity = eDiag_Warning;
    return diag;
}
inline
const CNcbiDiag& Error(const CNcbiDiag& diag)  {
    diag.x_EndMess();
    diag.m_Severity = eDiag_Error;
    return diag;
}
inline
const CNcbiDiag& Critical(const CNcbiDiag& diag)  {
    diag.x_EndMess();
    diag.m_Severity = eDiag_Critical;
    return diag;
}
inline
const CNcbiDiag& Fatal(const CNcbiDiag& diag)  {
    diag.x_EndMess();
    diag.m_Severity = eDiag_Fatal;
    return diag;
}
inline
const CNcbiDiag& Trace(const CNcbiDiag& diag)  {
    diag.x_EndMess();
    diag.m_Severity = eDiag_Trace;
    return diag;
}
inline
const CNcbiDiag& Message(const CNcbiDiag& diag)  {
    diag.m_PostFlags |= eDPF_IsMessage;
    return diag;
}


///////////////////////////////////////////////////////
//  CDiagBuffer::

inline
void CDiagBuffer::Reset(const CNcbiDiag& diag) {
    if (&diag == m_Diag) {
        m_Stream->rdbuf()->PUBSEEKOFF(0, IOS_BASE::beg, IOS_BASE::out);
        diag.ResetIsMessageFlag();
    }
}

inline
void CDiagBuffer::EndMess(const CNcbiDiag& diag) {
    if (&diag == m_Diag) {
        // Flush();
        Detach(&diag);
        diag.SetErrorCode(0, 0);
    }
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
        flags |= CDiagBuffer::s_GetPostFlags();
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
                           const char* err_text,
                           const char* module, 
                           const char* nclass, 
                           const char* function,
                           TPID        pid,
                           TTID        tid,
                           int         proc_post,
                           int         thr_post,
                           int         iter)
    : m_Data(0),
      m_Format(eFormat_Auto)
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
    m_Module     = module;
    m_Class      = nclass;
    m_Function   = function;
    m_PID        = pid;
    m_TID        = tid;
    m_ProcPost   = proc_post;
    m_ThrPost    = thr_post;
    m_Iteration  = iter;
}


///////////////////////////////////////////////////////
//  CDiagErrCodeInfo::


inline
CDiagErrCodeInfo::CDiagErrCodeInfo(void)
{
    return;
}


inline
CDiagErrCodeInfo::CDiagErrCodeInfo(const string& file_name)
{
    if ( !Read(file_name) ) {
        throw runtime_error
            ("CDiagErrCodeInfo::  failed to read error descriptions from file "
             + file_name);
    }
}


inline
CDiagErrCodeInfo::CDiagErrCodeInfo(CNcbiIstream& is)
{
    if ( !Read(is) ) {
        throw runtime_error
            ("CDiagErrCodeInfo::  failed to read error descriptions");
    }
}


inline
CDiagErrCodeInfo::~CDiagErrCodeInfo(void)
{
    Clear();
}


inline
void CDiagErrCodeInfo::Clear(void)
{
    m_Info.clear();
}


inline
void CDiagErrCodeInfo::SetDescription
(const ErrCode&                 err_code, 
 const SDiagErrCodeDescription& description)
{
    m_Info[err_code] = description;
}


inline
bool CDiagErrCodeInfo::HaveDescription(const ErrCode& err_code) const
{
    return m_Info.find(err_code) != m_Info.end();
}


/////////////////////////////////////////////////////////////////////////////
/// MDiagModuleCpp::

/*inline
const CNcbiDiag& operator<< (const CNcbiDiag&      diag,
                             const MDiagModuleCpp& module)
{
    if(module.m_Module)
        diag.SetModule(module.m_Module);
    return diag;
    }*/


/////////////////////////////////////////////////////////////////////////////
/// MDiagModule::

inline
MDiagModule::MDiagModule(const char* module)
    : m_Module(module)
{
}


inline
const CNcbiDiag& operator<< (const CNcbiDiag& diag, const MDiagModule& module)
{
    return diag.SetModule(module.m_Module);
}



/////////////////////////////////////////////////////////////////////////////
/// MDiagClass::

inline
MDiagClass::MDiagClass(const char* nclass)
    : m_Class(nclass)
{
}


inline
const CNcbiDiag& operator<< (const CNcbiDiag& diag, const MDiagClass& nclass)
{
    return diag.SetClass(nclass.m_Class);
}



/////////////////////////////////////////////////////////////////////////////
/// MDiagFunction::

inline
MDiagFunction::MDiagFunction(const char* function)
    : m_Function(function)
{
}


inline
const CNcbiDiag& operator<< (const CNcbiDiag& diag, const MDiagFunction& function)
{
    return diag.SetFunction(function.m_Function);
}



/*
 * ===========================================================================
 * $Log$
 * Revision 1.60  2006/10/24 19:11:55  ivanov
 * Cosmetics: replaced tabulation with spaces
 *
 * Revision 1.59  2006/06/06 16:44:07  grichenk
 * Added SetDoubleDiagHandler().
 * Added Severity().
 *
 * Revision 1.58  2006/05/24 18:52:30  grichenk
 * Added Message manipulator
 *
 * Revision 1.57  2006/04/17 15:37:43  grichenk
 * Added code to parse a string back into SDiagMessage
 *
 * Revision 1.56  2006/04/05 18:55:54  lavr
 * Reimplement IgnoreDiagDieLevel() [and change prototype to final form]
 *
 * Revision 1.55  2006/02/16 13:17:57  lavr
 * SEEKOFF -> PUBSEEKOFF (SEEKOFF is obsolescent a long ago)
 *
 * Revision 1.54  2006/01/17 16:30:02  grichenk
 * Fixed output for std stream manipulators.
 *
 * Revision 1.53  2005/12/20 16:16:02  grichenk
 * Fixed warnings
 *
 * Revision 1.52  2005/12/14 19:02:32  grichenk
 * Redesigned format of messages, added new values.
 * Added CDiagContext.
 *
 * Revision 1.51  2005/11/22 16:36:37  vakatov
 * CNcbiDiag::operator<< related fixes to allow for the no-hassle
 * posting of exceptions derived from CException. Before, only the
 * CException itself could be posted without additiional casting.
 *
 * Revision 1.50  2005/05/18 16:00:09  ucko
 * CDiagBuffer: note m_Stream's initial flags, and restore them on every
 * call to Flush to prevent settings from inadvertantly leaking between
 * messages.
 *
 * Revision 1.49  2005/05/04 19:54:13  ssikorsk
 * Store internal data in std::string instead of AutoPtr within CDiagCompileInfo and CNcbiDiag.
 *
 * Revision 1.48  2005/05/04 15:26:13  ssikorsk
 * Moved getters into inl-file. Initialised m_File and m_Module with empty
 * string in ctor instead of checking for NULL in getters every time.
 *
 * Revision 1.47  2005/05/04 14:14:30  ssikorsk
 * Made CNcbiDiag dtor not inline.
 *
 * Revision 1.46  2005/05/04 13:18:54  ssikorsk
 * Revamped CDiagCompileInfo and CNcbiDiag to use dynamically allocated buffers instead of predefined
 *
 * Revision 1.45  2004/09/22 13:32:16  kononenk
 * "Diagnostic Message Filtering" functionality added.
 * Added function SetDiagFilter()
 * Added class CDiagCompileInfo and macro DIAG_COMPILE_INFO
 * Module, class and function attribute added to CNcbiDiag and CException
 * Parameters __FILE__ and __LINE in CNcbiDiag and CException changed to
 * CDiagCompileInfo + fixes on derived classes and their usage
 * Macro NCBI_MODULE can be used to set default module name in cpp files
 *
 * Revision 1.44  2004/08/17 14:34:25  dicuccio
 * Added export specifiers for some static variables
 *
 * Revision 1.43  2004/04/26 19:28:24  ucko
 * Make previous change compiler-dependent due to MSVC bugginess.
 *
 * Revision 1.42  2004/04/26 14:35:48  ucko
 * Move CNcbiDiag::operator<< to ncbidiag.inl to make GCC 3.4 happy.
 *
 * Revision 1.41  2004/03/18 23:03:35  vakatov
 * Cosmetics
 *
 * Revision 1.40  2004/03/18 22:49:32  vakatov
 * SetDiagFixedPostLevel() -- get rid of extraneous and breaking 'const' in arg
 *
 * Revision 1.39  2004/03/10 19:54:12  gorelenk
 * Changed NCBI_XNCBI_EXPORT prefixes for class CDiagBuffer members.
 *
 * Revision 1.38  2003/11/12 20:30:25  ucko
 * Make extra flags for severity-trace messages tunable.
 *
 * Revision 1.37  2003/11/06 21:40:34  vakatov
 * A somewhat more natural handling of the 'eDPF_Default' flag -- replace
 * it by the current global flags, then merge these with other flags (if any)
 *
 * Revision 1.36  2003/04/25 20:53:16  lavr
 * Introduce draft version of IgnoreDiagDieLevel()
 * Clear error code/subcode from Endm() and Reset() manipulators
 *
 * Revision 1.35  2002/12/18 22:53:21  dicuccio
 * Added export specifier for building DLLs in windows.  Added global list of
 * all such specifiers in mswin_exports.hpp, included through ncbistl.hpp
 *
 * Revision 1.34  2002/08/01 18:48:08  ivanov
 * Added stuff to store and output error verbose messages for error codes
 *
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
 * + PushDiagPostPrefix(),PopPushDiagPostPrefix() - stack post prefix messages.
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
