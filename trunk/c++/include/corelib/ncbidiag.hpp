#ifndef CORELIB___NCBIDIAG__HPP
#define CORELIB___NCBIDIAG__HPP

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
 *   More elaborate documentation could be found in:
 *     http://www.ncbi.nlm.nih.gov/IEB/ToolBox/CPP_DOC/
 *            programming_manual/diag.html
 */

#include <corelib/ncbistre.hpp>
#include <list>
#include <map>
#include <stdexcept>


/** @addtogroup Diagnostics
 *
 * @{
 */


BEGIN_NCBI_SCOPE


// Auxiliary macros for a "standard" error posting
#define ERR_POST(message) \
    ( NCBI_NS_NCBI::CNcbiDiag(__FILE__, __LINE__) << message << NCBI_NS_NCBI::Endm )

#define LOG_POST(message) \
    ( NCBI_NS_NCBI::CNcbiDiag(eDiag_Error, eDPF_Log) << message << NCBI_NS_NCBI::Endm )

// ...with error codes
#define ERR_POST_EX(err_code, err_subcode, message) \
    ( NCBI_NS_NCBI::CNcbiDiag(__FILE__, __LINE__) << NCBI_NS_NCBI::ErrCode(err_code, err_subcode) << message << NCBI_NS_NCBI::Endm )

#define LOG_POST_EX(err_code, err_subcode, message) \
    ( NCBI_NS_NCBI::CNcbiDiag(eDiag_Error, eDPF_Log) << NCBI_NS_NCBI::ErrCode(err_code, err_subcode) << message << NCBI_NS_NCBI::Endm )


// Severity level for the posted diagnostics
enum EDiagSev {
    eDiag_Info = 0,
    eDiag_Warning,
    eDiag_Error,
    eDiag_Critical,
    eDiag_Fatal,   // guarantees to exit(or abort)
    eDiag_Trace,
    // limits
    eDiagSevMin = eDiag_Info,
    eDiagSevMax = eDiag_Trace
};


// Severity level change state
enum EDiagSevChange {
    eDiagSC_Unknown,  // Status of changing severity is unknown (first call)
    eDiagSC_Disable,  // Disable change severity level 
    eDiagSC_Enable    // Enable change severity level 
};


// Which parts of the diagnostic context should be posted, and which are not...
// The generic appearance of the posted message is as follows:
//   "<file>", line <line>: <severity>: (<err_code>.<err_subcode>)
//    [<prefix1>::<prefix2>::<prefixN>] <message>\n
//    <err_code_message>\n
//    <err_code_explanation>
//
// e.g. if all flags are set, and prefix string is set to "My prefix", and
// ERR_POST(eDiag_Warning, "Take care!"):
//   "/home/iam/myfile.cpp", line 33: Warning: (2.11) [aa::bb::cc] Take care!
//
// See also SDiagMessage::Compose().
enum EDiagPostFlag {
    eDPF_File               = 0x1,   // set by default #if _DEBUG; else not set
    eDPF_LongFilename       = 0x2,   // set by default #if _DEBUG; else not set
    eDPF_Line               = 0x4,   // set by default #if _DEBUG; else not set
    eDPF_Prefix             = 0x8,   // set by default (always)
    eDPF_Severity           = 0x10,  // set by default (always)
    eDPF_ErrCode            = 0x20,  // set by default (always)
    eDPF_ErrSubCode         = 0x40,  // set by default (always)
    eDPF_ErrCodeMessage     = 0x100, // set by default (always)
    eDPF_ErrCodeExplanation = 0x200, // set by default (always)
    eDPF_ErrCodeUseSeverity = 0x400, // set by default (always)
    eDPF_DateTime           = 0x80,  //
    eDPF_OmitInfoSev        = 0x4000,// no severity indication if eDiag_Info 

    // set all flags
    eDPF_All                = 0x3FFF,
    // set all flags for using with __FILE__ and __LINE__
    eDPF_Trace              = 0x1F,
    // print the posted message only;  without severity, location, prefix, etc.
    eDPF_Log                = 0x0,
    // ignore all other flags, use global flags
    eDPF_Default            = 0x8000
};

typedef int TDiagPostFlags;  // binary OR of "EDiagPostFlag"


// Forward declaration of some classes
class CDiagBuffer;
class CDiagErrCodeInfo;



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



//////////////////////////////////////////////////////////////////
// The diagnostics class

class CException;

class CNcbiDiag
{
public:
    NCBI_XNCBI_EXPORT
    CNcbiDiag(EDiagSev       sev        = eDiag_Error,
              TDiagPostFlags post_flags = eDPF_Default);
    NCBI_XNCBI_EXPORT
    CNcbiDiag(const char*  file, size_t line,
              EDiagSev       sev        = eDiag_Error,
              TDiagPostFlags post_flags = eDPF_Default);
    NCBI_XNCBI_EXPORT
    ~CNcbiDiag(void);

    // formatted output
    template<class X> const CNcbiDiag& operator<< (const X& x) const {
        m_Buffer.Put(*this, x);
        return *this;
    }

    // manipulator to set error code(s), like:  CNcbiDiag() << ErrCode(5,3);
    const CNcbiDiag& operator<< (const ErrCode& err_code) const;
    const CNcbiDiag& operator<< (const CException& ex) const;

    // other (function-based) manipulators
    const CNcbiDiag& operator<< (const CNcbiDiag& (*f)(const CNcbiDiag&))
        const
    {
        return f(*this);
    }

    // Output manipulators for CNcbiDiag

    // reset the content of current message
    friend const CNcbiDiag& Reset  (const CNcbiDiag& diag);
    // flush currend message, start new one
    friend const CNcbiDiag& Endm   (const CNcbiDiag& diag);
    // flush currend message, then
    // set a severity for the next diagnostic message
    friend const CNcbiDiag& Info    (const CNcbiDiag& diag);
    friend const CNcbiDiag& Warning (const CNcbiDiag& diag);
    friend const CNcbiDiag& Error   (const CNcbiDiag& diag);
    friend const CNcbiDiag& Critical(const CNcbiDiag& diag);
    friend const CNcbiDiag& Fatal   (const CNcbiDiag& diag);
    friend const CNcbiDiag& Trace   (const CNcbiDiag& diag);

    // get a common symbolic name for the severity levels
    static const char* SeverityName(EDiagSev sev);

    // get severity from string. "str_sev" can be the numeric value
    // or a symbolic name (see CDiagBuffer::sm_SeverityName[])
    static bool StrToSeverityLevel(const char* str_sev, EDiagSev& sev);

    // specify file name and line number to post
    // they are active for this message only, and they will be reset to
    // zero after this message is posted
    const CNcbiDiag& SetFile(const char* file) const;
    const CNcbiDiag& SetLine(size_t line) const;

    const CNcbiDiag& SetErrorCode(int code = 0, int subcode = 0) const;

    // get severity, file , line and error code of the current message
    EDiagSev       GetSeverity    (void) const;
    const char*    GetFile        (void) const;
    size_t         GetLine        (void) const;
    int            GetErrorCode   (void) const;
    int            GetErrorSubCode(void) const;
    TDiagPostFlags GetPostFlags   (void) const;

    // display error message
    NCBI_XNCBI_EXPORT
    static void DiagFatal   (const char* file, size_t line,
                             const char* message);
    NCBI_XNCBI_EXPORT
    static void DiagTrouble (const char* file, size_t line);
    NCBI_XNCBI_EXPORT
    static void DiagAssert  (const char* file, size_t line,
                             const char* expression);
    NCBI_XNCBI_EXPORT
    static void DiagValidate(const char* file, size_t line,
                             const char* expression, const char* message);

private:
    mutable EDiagSev       m_Severity;   // severity level of current message
    mutable char           m_File[256];  // file name
    mutable size_t         m_Line;       // line #
    mutable int            m_ErrCode;    // error code
    mutable int            m_ErrSubCode; // error subcode
    mutable CDiagBuffer&   m_Buffer;     // this thread's error message buffer
    mutable TDiagPostFlags m_PostFlags;  // bitwise OR of "EDiagPostFlag"

    // prohibit copy-constructor and assignment
    CNcbiDiag(const CNcbiDiag&);
    CNcbiDiag& operator= (const CNcbiDiag&);
};


/* @} */


/////////////////////////////////////////////////////////////////////////////
// ATTENTION:  the following functions are application-wide, i.e they
//             are not local for a particular thread
/////////////////////////////////////////////////////////////////////////////


// Return "true" if the specified "flag" is set in global "flags"(to be posted)
// If eDPF_Default is set in "flags" then use the current global flags as
// specified by SetDiagPostFlag()/UnsetDiagPostFlag()
inline bool IsSetDiagPostFlag(EDiagPostFlag  flag,
                              TDiagPostFlags flags = eDPF_Default);

// Set all global post flags to "flags";  return previously set flags
NCBI_XNCBI_EXPORT
extern TDiagPostFlags SetDiagPostAllFlags(TDiagPostFlags flags);

// Set/unset the specified flag (globally)
NCBI_XNCBI_EXPORT
extern void SetDiagPostFlag  (EDiagPostFlag flag);

NCBI_XNCBI_EXPORT
extern void UnsetDiagPostFlag(EDiagPostFlag flag);

// Specify a string to prefix all subsequent error postings with
NCBI_XNCBI_EXPORT
extern void SetDiagPostPrefix(const char* prefix);

// Push/pop a string to/from the list of message prefixes
NCBI_XNCBI_EXPORT
extern void PushDiagPostPrefix(const char* prefix);

NCBI_XNCBI_EXPORT
extern void PopDiagPostPrefix (void);

// Auxiliary class to temporarily add a prefix.
class NCBI_XNCBI_EXPORT CDiagAutoPrefix
{
public:
    // Add prefix "prefix"
    CDiagAutoPrefix(const string& prefix);
    CDiagAutoPrefix(const char*   prefix);
    // Remove the prefix (automagically, when the object gets out of scope)
    ~CDiagAutoPrefix(void);
};


// Do not post messages which severity is less than "min_sev"
// Return previous post-level.
// This function have effect only if:
//   1)  environment variable $DIAG_POST_LEVEL is not set, and
//   2)  registry value of DIAG_POST_LEVEL, section DEBUG is not set 
// The value of DIAG_POST_LEVEL can be a digital value (0-9) or 
// string value from CDiagBuffer::sm_SeverityName[] in any case.
#define DIAG_POST_LEVEL "DIAG_POST_LEVEL"
NCBI_XNCBI_EXPORT
extern EDiagSev SetDiagPostLevel(EDiagSev post_sev = eDiag_Error);

// Disable change the diagnostic post level.
// Consecutive using SetDiagPostLevel() will not have effect.
NCBI_XNCBI_EXPORT
extern bool DisableDiagPostLevelChange(bool disable_change = true);

// Abrupt the application if severity is >= "max_sev"
// Return previous die-level
NCBI_XNCBI_EXPORT
extern EDiagSev SetDiagDieLevel(EDiagSev die_sev = eDiag_Fatal);

// Set/unset abort handler.
// If "func"==0 that will be used default handler
typedef void (*FAbortHandler)(void);
NCBI_XNCBI_EXPORT
extern void SetAbortHandler(FAbortHandler func = 0);

// Smart abort function.
// It can process user abort handler and don't popup assert windows
// if specified (environment variable DIAG_SILENT_ABORT is "Y" or "y")
NCBI_XNCBI_EXPORT
extern void Abort(void);

// Disable/enable posting of "eDiag_Trace" messages.
// By default, these messages are disabled unless:
//   1)  environment variable $DIAG_TRACE is set (to any value), or
//   2)  registry value of DIAG_TRACE, section DEBUG is set (to any value)
#define DIAG_TRACE "DIAG_TRACE"
enum EDiagTrace {
    eDT_Default = 0,  // restores the default tracing context
    eDT_Disable,      // ignore messages of severity "eDiag_Trace"
    eDT_Enable        // enable messages of severity "eDiag_Trace"
};
NCBI_XNCBI_EXPORT
extern void SetDiagTrace(EDiagTrace how, EDiagTrace dflt = eDT_Default);


// Set new message handler("func"), data("data") and destructor("cleanup").
// The "func(..., data)" to be called when any instance of "CNcbiDiagBuffer"
// has a new diagnostic message completed and ready to post.
// "cleanup(data)" will be called whenever this hook gets replaced and
// on the program termination.
// NOTE 1:  "func()", "cleanup()" and "g_SetDiagHandler()" calls are
//          MT-protected, so that they would never be called simultaneously
//          from different threads.
// NOTE 2:  By default, the errors will be written to standard error stream.
struct SDiagMessage {
    SDiagMessage(EDiagSev severity, const char* buf, size_t len,
                 const char* file = 0, size_t line = 0,
                 TDiagPostFlags flags = eDPF_Default, const char* prefix = 0,
                 int err_code = 0, int err_subcode = 0,
                 const char* err_text = 0);

    mutable EDiagSev m_Severity;
    const char*      m_Buffer;  // not guaranteed to be '\0'-terminated!
    size_t           m_BufferLen;
    const char*      m_File;
    size_t           m_Line;
    int              m_ErrCode;
    int              m_ErrSubCode;
    TDiagPostFlags   m_Flags;   // bitwise OR of "EDiagPostFlag"
    const char*      m_Prefix;
    const char*      m_ErrText; // sometimes 'error' has no numeric code,
                                // still it can be represented as text

    // Compose a message string in the standard format(see also "flags"):
    //    "<file>", line <line>: <severity>: [<prefix>] <message> [EOL]
    // and put it to string "str", or write to an output stream "os".
    enum EDiagWriteFlags {
        fNone   = 0x0,
        fNoEndl = 0x01
    };
    typedef int TDiagWriteFlags;  // binary OR of "EDiagWriteFlags"

    void          Write(string& str,      TDiagWriteFlags flags = fNone) const;
    CNcbiOstream& Write(CNcbiOstream& os, TDiagWriteFlags flags = fNone) const;
};

inline CNcbiOstream& operator<< (CNcbiOstream& os, const SDiagMessage& mess) {
    return mess.Write(os);
}


// Base diag.handler class
class NCBI_XNCBI_EXPORT CDiagHandler
{
public:
    virtual ~CDiagHandler(void) {}
    virtual void Post(const SDiagMessage& mess) = 0;
};

typedef void (*FDiagHandler)(const SDiagMessage& mess);
typedef void (*FDiagCleanup)(void* data);

NCBI_XNCBI_EXPORT
extern void          SetDiagHandler(CDiagHandler* handler,
                                    bool can_delete = true);
NCBI_XNCBI_EXPORT
extern CDiagHandler* GetDiagHandler(bool take_ownership = false);

NCBI_XNCBI_EXPORT
extern void SetDiagHandler(FDiagHandler func,
                           void*        data,
                           FDiagCleanup cleanup);

// Return TRUE if user has ever set (or unset) diag. handler
NCBI_XNCBI_EXPORT
extern bool IsSetDiagHandler(void);


// Specialization of "CDiagHandler" for the stream-based diagnostics
class NCBI_XNCBI_EXPORT CStreamDiagHandler : public CDiagHandler
{
public:
    // This does *not* own the stream; users will need to clean it up
    // themselves if appropriate.
    CStreamDiagHandler(CNcbiOstream* os, bool quick_flush = true)
        : m_Stream(os), m_QuickFlush(quick_flush) {}
    virtual void Post(const SDiagMessage& mess);

    NCBI_XNCBI_EXPORT friend bool IsDiagStream(const CNcbiOstream* os);

protected:
    CNcbiOstream* m_Stream;

private:
    bool          m_QuickFlush;
};


// Write the error diagnostics to output stream "os"
// (this uses the SetDiagHandler() functionality)
NCBI_XNCBI_EXPORT
extern void SetDiagStream
(CNcbiOstream* os,
 bool          quick_flush  = true,// do stream flush after every message
 FDiagCleanup  cleanup      = 0,   // call "cleanup(cleanup_data)" if diag.
 void*         cleanup_data = 0    // stream is changed (see SetDiagHandler)
 );

// Return TRUE if "os" is the current diag. stream
NCBI_XNCBI_EXPORT 
extern bool IsDiagStream(const CNcbiOstream* os);


class NCBI_XNCBI_EXPORT CDiagFactory
{
public:
    virtual CDiagHandler* New(const string& s) = 0;
};


// Auxiliary class to limit the duration of changes to diagnostic settings.
class NCBI_XNCBI_EXPORT CDiagRestorer
{
public:
    CDiagRestorer (void); // captures current settings
    ~CDiagRestorer(void); // restores captured settings
private:
    // Prohibit dynamic allocation; there's no good reason to allow it,
    // and out-of-order destruction is problematic
    void* operator new      (size_t)  { throw runtime_error("forbidden"); }
    void* operator new[]    (size_t)  { throw runtime_error("forbidden"); }
    void  operator delete   (void*)   { throw runtime_error("forbidden"); }
    void  operator delete[] (void*)   { throw runtime_error("forbidden"); }

    string            m_PostPrefix;
    list<string>      m_PrefixList;
    TDiagPostFlags    m_PostFlags;
    EDiagSev          m_PostSeverity;
    EDiagSevChange    m_PostSeverityChange;
    EDiagSev          m_DieSeverity;
    EDiagTrace        m_TraceDefault;
    bool              m_TraceEnabled;
    CDiagHandler*     m_Handler;
    bool              m_CanDeleteHandler;
    CDiagErrCodeInfo* m_ErrCodeInfo;
    bool              m_CanDeleteErrCodeInfo;
};



//////////////////////////////////////////////////////////////////////////////
//
// CDiagErrCodeInfo class
//
// Auxilary class for CNcbiDiag. Using for read error messages from message 
// file and explain error on base the error code and subcode.


// Structure using for store the errors code and subcode description.
struct SDiagErrCodeDescription {
    SDiagErrCodeDescription(void);
    SDiagErrCodeDescription(const string& message,
                            const string& explanation,
                            int           severity = -1/*do not override*/)
        : m_Message(message),
          m_Explanation(explanation),
          m_Severity(severity)
    {
        return;
    }

public:
    string m_Message;     // Error message (short)
    string m_Explanation; // Error message (with detailed explanation)
    int    m_Severity;    // Message severity (if less that 0, then use 
                          // current diagnostic severity level)
};


class NCBI_XNCBI_EXPORT CDiagErrCodeInfo
{
public:
    // Constructors
    CDiagErrCodeInfo(void);
    CDiagErrCodeInfo(const string& file_name);  // can throw runtime_error
    CDiagErrCodeInfo(CNcbiIstream& is);         // can throw runtime_error

    // Destructor
    ~CDiagErrCodeInfo(void);

    // Read error descriptions from the specified file/stream,
    // store it in memory.
    bool Read(const string& file_name);
    bool Read(CNcbiIstream& is);
    
    // Delete all stored descriptions from memory
    void Clear(void);

    // Get description message for the error by its code.
    // Return TRUE if error description exists for this code;
    // return FALSE otherwise.
    bool GetDescription(const ErrCode&           err_code, 
                        SDiagErrCodeDescription* description) const;

    // Set error description for specified error code.
    // If description for this code already exist, then it will be overwritten.
    void SetDescription(const ErrCode&                 err_code, 
                        const SDiagErrCodeDescription& description);

    // Return TRUE if description for specified error code exists, 
    // otherwise return FALSE.
    bool HaveDescription(const ErrCode& err_code) const;

private:
    // Map for error messages
    typedef map<ErrCode, SDiagErrCodeDescription> TInfo;
    TInfo m_Info;
};


// Set/get handler for processing error codes. 
// By default this handler is unset. 
// NcbiApplication can init itself only if registry key DIAG_MESSAGE_FILE
// section DEBUG) is specified. The value of this key should be a name 
// of the file with the error codes explanations.
// http://www.ncbi.nlm.nih.gov/IEB/ToolBox/CPP_DOC/programming_manual/diag.html#errcodes

#define DIAG_MESSAGE_FILE "MessageFile"

NCBI_XNCBI_EXPORT
extern void SetDiagErrCodeInfo(CDiagErrCodeInfo* info,
                               bool              can_delete = true);

NCBI_XNCBI_EXPORT
extern CDiagErrCodeInfo* GetDiagErrCodeInfo(bool take_ownership = false);



///////////////////////////////////////////////////////
// All inline function implementations and internal data
// types, etc. are in this file

#include <corelib/ncbidiag.inl>


END_NCBI_SCOPE




/*
 * ==========================================================================
 *
 * $Log$
 * Revision 1.56  2003/04/11 17:55:44  lavr
 * Proper indentation of some fragments
 *
 * Revision 1.55  2003/03/31 15:36:51  siyan
 * Added doxygen support
 *
 * Revision 1.54  2002/12/18 22:53:21  dicuccio
 * Added export specifier for building DLLs in windows.  Added global list of
 * all such specifiers in mswin_exports.hpp, included through ncbistl.hpp
 *
 * Revision 1.53  2002/09/24 18:28:20  vasilche
 * Fixed behavour of CNcbiDiag::DiagValidate() in release mode
 *
 * Revision 1.52  2002/09/19 20:05:41  vasilche
 * Safe initialization of static mutexes
 *
 * Revision 1.51  2002/09/16 20:49:06  vakatov
 * Cosmetics and comments
 *
 * Revision 1.50  2002/08/20 19:13:09  gouriano
 * added DiagWriteFlags into SDiagMessage::Write
 *
 * Revision 1.49  2002/08/16 15:02:50  ivanov
 * Added class CDiagAutoPrefix
 *
 * Revision 1.48  2002/08/01 18:48:07  ivanov
 * Added stuff to store and output error verbose messages for error codes
 *
 * Revision 1.47  2002/07/15 18:17:50  gouriano
 * renamed CNcbiException and its descendents
 *
 * Revision 1.46  2002/07/10 16:18:42  ivanov
 * Added CNcbiDiag::StrToSeverityLevel().
 * Rewrite and rename SetDiagFixedStrPostLevel() -> SetDiagFixedPostLevel()
 *
 * Revision 1.45  2002/07/09 16:38:00  ivanov
 * Added GetSeverityChangeEnabledFirstTime().
 * Fix usage forced set severity post level from environment variable
 * to work without NcbiApplication::AppMain()
 *
 * Revision 1.44  2002/07/02 18:26:08  ivanov
 * Added CDiagBuffer::DisableDiagPostLevelChange()
 *
 * Revision 1.43  2002/06/26 18:36:37  gouriano
 * added CNcbiException class
 *
 * Revision 1.42  2002/04/23 19:57:25  vakatov
 * Made the whole CNcbiDiag class "mutable" -- it helps eliminate
 * numerous warnings issued by SUN Forte6U2 compiler.
 * Do not use #NO_INCLASS_TMPL anymore -- apparently all modern
 * compilers seem to be supporting in-class template methods.
 *
 * Revision 1.41  2002/04/16 18:38:02  ivanov
 * SuppressDiagPopupMessages() moved to "test/test_assert.h"
 *
 * Revision 1.40  2002/04/11 20:39:16  ivanov
 * CVS log moved to end of the file
 *
 * Revision 1.39  2002/04/11 19:58:03  ivanov
 * Added function SuppressDiagPopupMessages()
 *
 * Revision 1.38  2002/04/10 14:45:04  ivanov
 * Abort() moved from static to extern and added to header file
 *
 * Revision 1.37  2002/04/01 22:34:35  ivanov
 * Added SetAbortHandler() function to set user abort handler
 *
 * Revision 1.36  2002/02/07 19:45:53  ucko
 * Optionally transfer ownership in GetDiagHandler.
 *
 * Revision 1.35  2001/11/14 15:14:58  ucko
 * Revise diagnostic handling to be more object-oriented.
 *
 * Revision 1.34  2001/10/29 15:37:29  ucko
 * Rewrite dummy new/delete bodies to avoid GCC warnings.
 *
 * Revision 1.33  2001/10/29 15:25:55  ucko
 * Give (prohibited) CDiagRestorer::new/delete dummy bodies.
 *
 * Revision 1.32  2001/10/29 15:16:11  ucko
 * Preserve default CGI diagnostic settings, even if customized by app.
 *
 * Revision 1.31  2001/10/16 23:44:04  vakatov
 * + SetDiagPostAllFlags()
 *
 * Revision 1.30  2001/08/09 16:24:05  lavr
 * Added eDPF_OmitInfoSev to log message format flags
 *
 * Revision 1.29  2001/07/26 21:28:49  lavr
 * Remove printing DateTime stamp by default
 *
 * Revision 1.28  2001/07/25 19:12:41  lavr
 * Added date/time stamp for message logging
 *
 * Revision 1.27  2001/06/14 03:37:28  vakatov
 * For the ErrCode manipulator, use CNcbiDiag:: method rather than a friend
 *
 * Revision 1.26  2001/06/13 23:19:36  vakatov
 * Revamped previous revision (prefix and error codes)
 *
 * Revision 1.25  2001/06/13 20:51:52  ivanov
 * + PushDiagPostPrefix(), PopPushDiagPostPrefix() - stack post prefix messages.
 * + ERR_POST_EX, LOG_POST_EX - macros for posting with error codes.
 * + ErrCode(code[,subcode]) - CNcbiDiag error code manipulator.
 * + eDPF_ErrCode, eDPF_ErrSubCode - new post flags.
 *
 * Revision 1.24  2001/05/17 14:50:58  lavr
 * Typos corrected
 *
 * Revision 1.23  2000/11/29 16:58:23  vakatov
 * Added LOG_POST() macro -- to print the posted message only
 *
 * Revision 1.22  2000/10/24 19:54:44  vakatov
 * Diagnostics to go to CERR by default (was -- disabled by default)
 *
 * Revision 1.21  2000/04/18 19:50:10  vakatov
 * Get rid of the old-fashioned C-like "typedef enum {...} E;"
 *
 * Revision 1.20  2000/04/04 22:31:57  vakatov
 * SetDiagTrace() -- auto-set basing on the application
 * environment and/or registry
 *
 * Revision 1.19  2000/03/10 14:17:40  vasilche
 * Added missing namespace specifier to macro.
 *
 * Revision 1.18  2000/02/18 16:54:02  vakatov
 * + eDiag_Critical
 *
 * Revision 1.17  2000/01/20 16:52:29  vakatov
 * SDiagMessage::Write() to replace SDiagMessage::Compose()
 * + operator<<(CNcbiOstream& os, const SDiagMessage& mess)
 * + IsSetDiagHandler(), IsDiagStream()
 *
 * Revision 1.16  1999/12/29 22:30:22  vakatov
 * Use "exit()" rather than "abort()" in non-#_DEBUG mode
 *
 * Revision 1.15  1999/12/28 18:55:24  vasilche
 * Reduced size of compiled object files:
 * 1. avoid inline or implicit virtual methods (especially destructors).
 * 2. avoid std::string's methods usage in inline methods.
 * 3. avoid string literals ("xxx") in inline methods.
 *
 * Revision 1.14  1999/12/27 19:44:15  vakatov
 * Fixes for R1.13:
 * ERR_POST() -- use eDPF_Default rather than eDPF_Trace;  forcibly flush
 * ("<< Endm") the diag. stream. Get rid of the eDPF_CopyFilename, always
 * make a copy of the file name.
 *
 * Revision 1.13  1999/09/27 16:23:20  vasilche
 * Changed implementation of debugging macros (_TRACE, _THROW*, _ASSERT etc),
 * so that they will be much easier for compilers to eat.
 *
 * Revision 1.12  1999/05/04 00:03:06  vakatov
 * Removed the redundant severity arg from macro ERR_POST()
 *
 * Revision 1.11  1999/04/30 19:20:57  vakatov
 * Added more details and more control on the diagnostics
 * See #ERR_POST, EDiagPostFlag, and ***DiagPostFlag()
 *
 * Revision 1.10  1999/03/15 16:08:09  vakatov
 * Fixed "{...}" macros to "do {...} while(0)" lest it to mess up with "if"s
 *
 * Revision 1.9  1999/03/12 18:04:06  vakatov
 * Added ERR_POST macro to perform a plain "standard" error posting
 *
 * Revision 1.8  1998/12/30 21:52:16  vakatov
 * Fixed for the new SunPro 5.0 beta compiler that does not allow friend
 * templates and member(in-class) templates
 *
 * Revision 1.7  1998/12/28 17:56:27  vakatov
 * New CVS and development tree structure for the NCBI C++ projects
 *
 * Revision 1.6  1998/11/06 22:42:37  vakatov
 * Introduced BEGIN_, END_ and USING_ NCBI_SCOPE macros to put NCBI C++
 * API to namespace "ncbi::" and to use it by default, respectively
 * Introduced THROWS_NONE and THROWS(x) macros for the exception
 * specifications
 * Other fixes and rearrangements throughout the most of "corelib" code
 *
 * Revision 1.5  1998/11/04 23:46:35  vakatov
 * Fixed the "ncbidbg/diag" header circular dependencies
 *
 * Revision 1.4  1998/11/03 20:51:24  vakatov
 * Adaptation for the SunPro compiler glitchs(see conf. #NO_INCLASS_TMPL)
 *
 * Revision 1.3  1998/10/30 20:08:20  vakatov
 * Fixes to (first-time) compile and test-run on MSVS++
 *
 * Revision 1.2  1998/10/27 23:06:58  vakatov
 * Use NCBI C++ interface to iostream's
 * ==========================================================================
 */

#endif  /* CORELIB___NCBIDIAG__HPP */
