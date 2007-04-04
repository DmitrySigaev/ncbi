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
 * Authors:  Denis Vakatov et al.
 *
 * File Description:
 *   NCBI C++ diagnostic API
 *
 */


#include <ncbi_pch.hpp>

#include <ncbiconf.h>

#include <corelib/ncbidiag.hpp>
#include <corelib/ncbithr.hpp>
#include <corelib/ncbimtx.hpp>
#include <corelib/ncbi_safe_static.hpp>
#include <corelib/ncbiexpt.hpp>
#include <corelib/ncbi_process.hpp>
#include <corelib/ncbi_param.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbifile.hpp>
#include <corelib/syslog.hpp>
#include "ncbidiag_p.hpp"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <stack>
#include <fcntl.h>

#if defined(NCBI_OS_MSWIN)
#  include <io.h>
#endif

#if defined(NCBI_OS_MAC)
#  include <corelib/ncbi_os_mac.hpp>
#endif

#if defined(NCBI_OS_UNIX)
#  include <unistd.h>
#  include <sys/utsname.h>
#endif


BEGIN_NCBI_SCOPE

DEFINE_STATIC_MUTEX(s_DiagMutex);

#if defined(NCBI_POSIX_THREADS) && defined(HAVE_PTHREAD_ATFORK)

#include <unistd.h> // for pthread_atfork()

extern "C" {
    static void s_NcbiDiagPreFork(void)
    {
        s_DiagMutex.Lock();
    }
    static void s_NcbiDiagPostFork(void)
    {
        s_DiagMutex.Unlock();
    }
}

#endif


///////////////////////////////////////////////////////
//  Output format parameters

// Use old output format if the flag is set
NCBI_PARAM_DECL(bool, Diag, Old_Post_Format);
NCBI_PARAM_DEF_EX(bool, Diag, Old_Post_Format, true, eParam_NoThread,
                  DIAG_OLD_POST_FORMAT);
typedef NCBI_PARAM_TYPE(Diag, Old_Post_Format) TOldPostFormatParam;

// Auto-print context properties on set/change.
NCBI_PARAM_DECL(bool, Diag, AutoWrite_Context);
NCBI_PARAM_DEF_EX(bool, Diag, AutoWrite_Context, false, eParam_NoThread,
                  DIAG_AUTOWRITE_CONTEXT);
typedef NCBI_PARAM_TYPE(Diag, AutoWrite_Context) TAutoWrite_Context;

// Print system TID rather than CThread::GetSelf()
NCBI_PARAM_DECL(bool, Diag, Print_System_TID);
NCBI_PARAM_DEF_EX(bool, Diag, Print_System_TID, false, eParam_NoThread,
                  DIAG_PRINT_SYSTEM_TID);
typedef NCBI_PARAM_TYPE(Diag, Print_System_TID) TPrintSystemTID;


///////////////////////////////////////////////////////
//  Static variables for Trace and Post filters

static CDiagFilter s_TraceFilter;
static CDiagFilter s_PostFilter;


// Analogue to strstr.
// Returns a pointer to the last occurrence of a search string in a string
const char* 
str_rev_str(const char* begin_str, const char* end_str, const char* str_search)
{
    if (begin_str == NULL)
        return NULL;
    if (end_str == NULL)
        return NULL;
    if (str_search == NULL)
        return NULL;

    const char* search_char = str_search + strlen(str_search);
    const char* cur_char = end_str;

    do {
        --search_char;
        do {
            --cur_char;
        } while(*cur_char != *search_char && cur_char != begin_str);
        if (*cur_char != *search_char)
            return NULL; 
    }
    while (search_char != str_search);
    
    return cur_char;
}



/////////////////////////////////////////////////////////////////////////////
/// CDiagCompileInfo::

CDiagCompileInfo::CDiagCompileInfo(void)
    : m_File(""),
      m_Module(""),
      m_Line(0),
      m_CurrFunctName(0),
      m_Parsed(false)
{
}

CDiagCompileInfo::CDiagCompileInfo(const char* file, 
                                   int         line, 
                                   const char* curr_funct, 
                                   const char* module)
    : m_File(file),
      m_Module(""),
      m_Line(line),
      m_CurrFunctName(curr_funct),
      m_Parsed(false)
{
    if (!file) {
        m_File = "";
        return;
    }
    if (!module)
        return;

    // Check for a file extension without creating of temporary string objects
    const char* cur_extension = strrchr(file, '.');
    if (cur_extension == NULL)
        return; 

    if (*(cur_extension + 1) != '\0') {
        ++cur_extension;
    } else {
        return;
    }

    if (strcmp(cur_extension, "cpp") == 0 || 
        strcmp(cur_extension, "C") == 0 || 
        strcmp(cur_extension, "c") == 0 || 
        strcmp(cur_extension, "cxx") == 0 ) {
        m_Module = module;
    }
}


CDiagCompileInfo::~CDiagCompileInfo(void)
{
    return;
}


void 
CDiagCompileInfo::ParseCurrFunctName(void) const
{
    if (m_CurrFunctName && *m_CurrFunctName) {
        // Parse curr_funct

        const char* end_str = strchr(m_CurrFunctName, '(');
        if ( end_str ) {
            // Get a function/method name
            const char* start_str = NULL;

            // Get a finction start position.
            const char* start_str_tmp =
                str_rev_str(m_CurrFunctName, end_str, "::");
            bool has_class = start_str_tmp != NULL;
            if (start_str_tmp != NULL) {
                start_str = start_str_tmp + 2;
            } else {
                start_str_tmp = str_rev_str(m_CurrFunctName, end_str, " ");
                if (start_str_tmp != NULL) {
                    start_str = start_str_tmp + 1;
                } 
            }

            const char* cur_funct_name =
                (start_str == NULL ? m_CurrFunctName : start_str);
            size_t cur_funct_name_len = end_str - cur_funct_name;
            m_FunctName = string(cur_funct_name, cur_funct_name_len);

           // Get a class name
           if (has_class) {
               end_str = start_str - 2;
               start_str = str_rev_str(m_CurrFunctName, end_str, " ");
               const char* cur_class_name =
                   (start_str == NULL ? m_CurrFunctName : start_str + 1);
               size_t cur_class_name_len = end_str - cur_class_name;
               m_ClassName = string(cur_class_name, cur_class_name_len);
           }
        }
    }
    m_Parsed = true;
}



///////////////////////////////////////////////////////
//  CDiagRecycler::

class CDiagRecycler {
public:
    CDiagRecycler(void)
    {
#if defined(NCBI_POSIX_THREADS) && defined(HAVE_PTHREAD_ATFORK)
        pthread_atfork(s_NcbiDiagPreFork,   // before
                       s_NcbiDiagPostFork,  // after in parent
                       s_NcbiDiagPostFork); // after in child
#endif
    }
    ~CDiagRecycler(void)
    {
        SetDiagHandler(0, false);
        SetDiagErrCodeInfo(0, false);
    }
};

static CSafeStaticPtr<CDiagRecycler> s_DiagRecycler;


///////////////////////////////////////////////////////
//  CDiagContextThreadData::


CDiagContextThreadData::CDiagContextThreadData(void)
    : m_Properties(0),
      m_RequestId(0),
      m_StopWatch(0),
      m_DiagBuffer(new CDiagBuffer)
{
}


CDiagContextThreadData::~CDiagContextThreadData(void)
{
    delete m_Properties;
    delete m_StopWatch;
    delete m_DiagBuffer;
}


void ThreadDataTlsCleanup(CDiagContextThreadData* value, void* cleanup_data)
{
    if ( cleanup_data ) {
        // Copy properties from the main thread's TLS to the global properties.
        CMutexGuard LOCK(s_DiagMutex);
        CDiagContextThreadData::TProperties* props =
            value->GetProperties(CDiagContextThreadData::eProp_Get);
        if ( props ) {
            GetDiagContext().m_Properties.insert(props->begin(),
                                                 props->end());
        }
        // Print stop message.
        if (!CDiagContext::IsSetOldPostFormat()) {
            GetDiagContext().PrintStop();
        }
    }
    delete value;
}


CDiagContextThreadData& CDiagContextThreadData::GetThreadData(void)
{
    static CSafeStaticRef< CTls<CDiagContextThreadData> >
        s_ThreadData(0,
        CSafeStaticLifeSpan(CSafeStaticLifeSpan::eLifeSpan_Long, 1));
    CDiagContextThreadData* data = s_ThreadData->GetValue();
    if ( !data ) {
        // Cleanup data set to null for any thread except the main one.
        // This value is used as a flag to copy threads' properties to global
        // upon TLS cleanup.
        data = new CDiagContextThreadData;
        s_ThreadData->SetValue(data, ThreadDataTlsCleanup,
            CThread::GetSelf() ? 0 : (void*)(1));
    }
    return *data;
}


CDiagContextThreadData::TProperties*
CDiagContextThreadData::GetProperties(EGetProperties flag)
{
    if ( !m_Properties  &&  flag == eProp_Create ) {
        m_Properties = new TProperties;
    }
    return m_Properties;
}


CStopWatch* CDiagContextThreadData::GetOrCreateStopWatch(void)
{
    if ( !m_StopWatch ) {
        m_StopWatch = new CStopWatch(CStopWatch::eStart);
    }
    return m_StopWatch;
}


void CDiagContextThreadData::ResetStopWatch(void)
{
    delete m_StopWatch;
    m_StopWatch = 0;
}


extern int GetDiagRequestId(void)
{
    return CDiagContextThreadData::GetThreadData().GetRequestId();
}


extern void SetDiagRequestId(int id)
{
    return CDiagContextThreadData::GetThreadData().SetRequestId(id);
}


///////////////////////////////////////////////////////
//  CDiagContext::


struct SDiagMessageData
{
    SDiagMessageData(void) : m_UID(0) {}
    ~SDiagMessageData(void) {}

    string m_Message;
    string m_File;
    string m_Module;
    string m_Class;
    string m_Function;
    string m_Prefix;
    string m_ErrText;

    CDiagContext::TUID m_UID;
    CTime              m_Time;

    // If the following properties are not set, take them from DiagContext.
    string m_Host;
    string m_Client;
    string m_Session;
    string m_AppName;
};


CDiagContext::CDiagContext(void)
    : m_UID(0),
      m_StopWatch(new CStopWatch(CStopWatch::eStart)),
      m_MaxMessages(100) // limit number of collected messages to 100
{
}


CDiagContext::~CDiagContext(void)
{
    delete m_StopWatch;
}


CDiagContext::TPID CDiagContext::sm_PID = 0;

CDiagContext::TPID CDiagContext::GetPID(void)
{
    if ( !sm_PID ) {
        sm_PID = CProcess::GetCurrentPid();
    }
    return sm_PID;
}


void CDiagContext::UpdatePID(void)
{
    TPID new_pid = CProcess::GetCurrentPid();
    if (sm_PID == new_pid) {
        // Parent process does not need to update pid/guid
        return;
    }
    sm_PID = new_pid;
    CDiagContext& ctx = GetDiagContext();
    TUID old_uid = ctx.GetUID();
    // Update GUID to match the new PID
    ctx.x_CreateUID();
    ctx.PrintExtra("New process created by fork(), "
        "parent GUID=" + ctx.GetStringUID(old_uid));
}


CDiagContext::TUID CDiagContext::GetUID(void) const
{
    if ( !m_UID ) {
        CMutexGuard LOCK(s_DiagMutex);
        if ( !m_UID ) {
            x_CreateUID();
        }
    }
    return m_UID;
}


string CDiagContext::GetStringUID(TUID uid) const
{
    char buf[18];
    if (uid == 0) {
        uid = GetUID();
    }
    int hi = int((uid >> 32) & 0xFFFFFFFF);
    int lo = int(uid & 0xFFFFFFFF);
    sprintf(buf, "%08X%08X", hi, lo);
    return string(buf);
}


static string s_GetHost(void)
{
    // Check context properties
    string ret = GetDiagContext().GetProperty(
        CDiagContext::kProperty_HostName);
    if ( !ret.empty() )
        return ret;

    ret = GetDiagContext().GetProperty(CDiagContext::kProperty_HostIP);
    if ( !ret.empty() )
        return ret;

#if defined(NCBI_OS_UNIX)
    // UNIX - use uname()
    {{
        struct utsname buf;
        if (uname(&buf) == 0)
            return string(buf.nodename);
    }}
#endif

#if defined(NCBI_OS_MSWIN)
    // MSWIN - use COMPUTERNAME
    const char* compname = ::getenv("COMPUTERNAME");
    if ( compname  &&  *compname )
        return compname;
#endif

    // Server env. - use SERVER_ADDR
    const char* servaddr = ::getenv("SERVER_ADDR");
    if ( servaddr  &&  *servaddr )
        return servaddr;

    // Can not get hostname
    return "UNK_HOST";
}


void CDiagContext::x_CreateUID(void) const
{
    Int8 pid = GetPID();
    time_t t = time(0);
    string host = s_GetHost();
    TUID h = 201;
    ITERATE(string, s, host) {
        h = (h*15 + *s) & 0xFFFF;
    }
    m_UID = (h << 48) + ((pid & 0xFFFF) << 32) + ((t & 0xFFFFFFF) << 4);
}


void CDiagContext::SetAutoWrite(bool value)
{
    TAutoWrite_Context::SetDefault(value);
}


inline bool IsThreadProperty(const string& name)
{
    return
        name == CDiagContext::kProperty_ClientIP   ||
        name == CDiagContext::kProperty_SessionID  ||
        name == CDiagContext::kProperty_ReqStatus  ||
        name == CDiagContext::kProperty_ReqTime    ||
        name == CDiagContext::kProperty_BytesRd    ||
        name == CDiagContext::kProperty_BytesWr;
}


inline bool IsGlobalProperty(const string& name)
{
    return
        name == CDiagContext::kProperty_UserName  ||
        name == CDiagContext::kProperty_HostName  ||
        name == CDiagContext::kProperty_HostIP    ||
        name == CDiagContext::kProperty_AppName   ||
        name == CDiagContext::kProperty_ExitSig   ||
        name == CDiagContext::kProperty_ExitCode;
}


void CDiagContext::SetProperty(const string& name,
                               const string& value,
                               EPropertyMode mode)
{
    if ( mode == eProp_Default ) {
        mode = IsGlobalProperty(name) ? eProp_Global : eProp_Thread;
    }

    if ( mode == eProp_Global ) {
        CMutexGuard LOCK(s_DiagMutex);
        m_Properties[name] = value;
    }
    else {
        TProperties* props =
            CDiagContextThreadData::GetThreadData().GetProperties(
            CDiagContextThreadData::eProp_Create);
        _ASSERT(props);
        (*props)[name] = value;
    }
    if ( TAutoWrite_Context::GetDefault() ) {
        CMutexGuard LOCK(s_DiagMutex);
        x_PrintMessage(SDiagMessage::eEvent_Extra, name + "=" + value);
    }
}


string CDiagContext::GetProperty(const string& name,
                                 EPropertyMode mode) const
{
    if (mode == eProp_Thread  ||
        (mode == eProp_Default  &&  !IsGlobalProperty(name))) {
        TProperties* props =
            CDiagContextThreadData::GetThreadData().GetProperties(
            CDiagContextThreadData::eProp_Get);
        if ( props ) {
            TProperties::const_iterator tprop = props->find(name);
            if ( tprop != props->end() ) {
                return tprop->second;
            }
        }
        if (mode == eProp_Thread) {
            return kEmptyStr;
        }
    }
    // Check global properties
    CMutexGuard LOCK(s_DiagMutex);
    TProperties::const_iterator gprop = m_Properties.find(name);
    return gprop != m_Properties.end() ? gprop->second : kEmptyStr;
}


void CDiagContext::DeleteProperty(const string& name,
                                    EPropertyMode mode)
{
    if (mode == eProp_Thread  ||
        (mode ==  eProp_Default  &&  !IsGlobalProperty(name))) {
        TProperties* props =
            CDiagContextThreadData::GetThreadData().GetProperties(
            CDiagContextThreadData::eProp_Get);
        if ( props ) {
            TProperties::iterator tprop = props->find(name);
            if ( tprop != props->end() ) {
                props->erase(tprop);
                return;
            }
        }
        if (mode == eProp_Thread) {
            return;
        }
    }
    // Check global properties
    CMutexGuard LOCK(s_DiagMutex);
    TProperties::iterator gprop = m_Properties.find(name);
    if (gprop != m_Properties.end()) {
        m_Properties.erase(gprop);
    }
}


void CDiagContext::PrintProperties(void)
{
    {{
        CMutexGuard LOCK(s_DiagMutex);
        ITERATE(TProperties, gprop, m_Properties) {
            x_PrintMessage(SDiagMessage::eEvent_Extra,
                gprop->first + "=" + gprop->second);
        }
    }}
    TProperties* props =
            CDiagContextThreadData::GetThreadData().GetProperties(
            CDiagContextThreadData::eProp_Get);
    if ( !props ) {
        return;
    }
    ITERATE(TProperties, tprop, *props) {
        x_PrintMessage(SDiagMessage::eEvent_Extra,
            tprop->first + "=" + tprop->second);
    }
}


void CDiagContext::PrintStart(const string& message)
{
    x_PrintMessage(SDiagMessage::eEvent_Start, message);
}


void CDiagContext::PrintStop(void)
{
    x_PrintMessage(SDiagMessage::eEvent_Stop, kEmptyStr);
}


void CDiagContext::PrintExtra(const string& message)
{
    x_PrintMessage(SDiagMessage::eEvent_Extra, message);
}


void CDiagContext::PrintRequestStart(const string& message)
{
    x_PrintMessage(SDiagMessage::eEvent_RequestStart, message);
}


void CDiagContext::PrintRequestStop(void)
{
    x_PrintMessage(SDiagMessage::eEvent_RequestStop, kEmptyStr);
}


const char* CDiagContext::kProperty_UserName    = "user";
const char* CDiagContext::kProperty_HostName    = "host";
const char* CDiagContext::kProperty_HostIP      = "host_ip_addr";
const char* CDiagContext::kProperty_ClientIP    = "client_ip";
const char* CDiagContext::kProperty_SessionID   = "session_id";
const char* CDiagContext::kProperty_AppName     = "app_name";
const char* CDiagContext::kProperty_ExitSig     = "exit_signal";
const char* CDiagContext::kProperty_ExitCode    = "exit_code";
const char* CDiagContext::kProperty_ReqStatus   = "request_status";
const char* CDiagContext::kProperty_ReqTime     = "request_time";
const char* CDiagContext::kProperty_BytesRd     = "bytes_rd";
const char* CDiagContext::kProperty_BytesWr     = "bytes_wr";

static const char* kDiagTimeFormat = "Y/M/D:h:m:s";
// Fixed fields' widths
static const int   kDiagW_PID      = 5;
static const int   kDiagW_TID      = 3;
static const int   kDiagW_RID      = 4;
static const int   kDiagW_SN       = 4;
static const int   kDiagW_UID      = 16;
static const int   kDiagW_Client   = 15;

void CDiagContext::WriteStdPrefix(CNcbiOstream& ostr,
                                  const SDiagMessage* msg) const
{
    CDiagBuffer& buf = GetDiagBuffer();

    SDiagMessage::TPID pid = msg ? msg->m_PID : GetPID();
    SDiagMessage::TTID tid = msg ? msg->m_TID : buf.m_TID;
    string uid = GetStringUID(msg ? msg->GetUID() : 0);
    int rid = msg ? msg->m_RequestId : GetDiagRequestId();
    int psn = msg ? msg->m_ProcPost :
        CDiagBuffer::GetProcessPostNumber(CDiagBuffer::ePostNumber_Increment);
    int tsn = msg ? msg->m_ThrPost : ++buf.m_ThreadPostCount;
    CTime timestamp = msg ? msg->GetTime() : CTime(CTime::eCurrent);
    string host = s_GetHost();
    string client = GetProperty(kProperty_ClientIP);
    string session = GetProperty(kProperty_SessionID);
    string app = GetProperty(kProperty_AppName);

    if ( msg ) {
        // When flushing collected messages, m_Messages should be NULL.
        if ( m_Messages.get() ) {
            msg->x_SaveProperties(host, client, session, app);
        }
        else if ( msg->m_Data ) {
            // Restore saved properties from the message
            host = msg->m_Data->m_Host.empty() ?
                "UNK_HOST" : msg->m_Data->m_Host;
            client = msg->m_Data->m_Client;
            session = msg->m_Data->m_Session;
            app = msg->m_Data->m_AppName;
        }
    }
    if ( client.empty() ) {
        client = "UNK_CLIENT";
    }
    if ( session.empty() ) {
        session = "UNK_SESSION";
    }
    if ( app.empty() ) {
        app = "UNK_APP";
    }

    // Print common fields
    ostr << setfill('0') << setw(kDiagW_PID) << pid << '/'
         << setw(kDiagW_TID) << tid << '/'
         << setw(kDiagW_RID) << rid << ' '
         << setw(0) << setfill(' ') << uid << ' '
         << setfill('0') << setw(kDiagW_SN) << psn << '/'
         << setw(kDiagW_SN) << tsn << ' '
         << setw(0) << timestamp.AsString(kDiagTimeFormat) << ' '
         << host << ' '
         << setfill(' ') << setw(kDiagW_Client) << setiosflags(IOS_BASE::left)
         << setw(0) << client << resetiosflags(IOS_BASE::left) << ' '
         << session << ' '
         << app << ' ';
}


void RequestStopWatchTlsCleanup(CStopWatch* value, void* /*cleanup_data*/)
{
    delete value;
}


void CDiagContext::x_PrintMessage(SDiagMessage::EEventType event,
                                  const string&            message)
{
    if ( IsSetOldPostFormat() ) {
        return;
    }
    CDiagBuffer& buf = GetDiagBuffer();
    CNcbiOstrstream ostr;
    string prop;
    bool need_space = false;

    switch ( event ) {
    case SDiagMessage::eEvent_Start:
    case SDiagMessage::eEvent_Extra:
        break;
    case SDiagMessage::eEvent_RequestStart:
        {
            // Reset properties
            DeleteProperty(kProperty_ReqStatus);
            DeleteProperty(kProperty_ReqTime);
            DeleteProperty(kProperty_BytesRd);
            DeleteProperty(kProperty_BytesWr);
            // Start stopwatch
            CStopWatch* sw =
                CDiagContextThreadData::GetThreadData().GetOrCreateStopWatch();
            _ASSERT(sw);
            sw->Restart();
            break;
        }
    case SDiagMessage::eEvent_Stop:
        prop = GetProperty(kProperty_ExitSig);
        if ( !prop.empty() ) {
            ostr << prop;
            need_space = true;
        }
        prop = GetProperty(kProperty_ExitCode);
        if ( !prop.empty() ) {
            if (need_space) {
                ostr << " ";
            }
            ostr << prop;
            need_space = true;
        }
        if (need_space) {
            ostr << " ";
        }
        ostr << m_StopWatch->AsString();
        break;
    case SDiagMessage::eEvent_RequestStop:
        {
            prop = GetProperty(kProperty_ReqStatus);
            if ( !prop.empty() ) {
                ostr << prop;
                need_space = true;
            }
            prop = GetProperty(kProperty_ReqTime);
            if ( prop.empty() ) {
                // Try to get time from the stopwatch
                CStopWatch* sw =
                    CDiagContextThreadData::GetThreadData().
                    GetOrCreateStopWatch();
                if ( sw ) {
                    prop = sw->AsString();
                    CDiagContextThreadData::GetThreadData().ResetStopWatch();
                }
            }
            if ( !prop.empty() ) {
                if (need_space) {
                    ostr << " ";
                }
                ostr << prop;
                need_space = true;
            }
            prop = GetProperty(kProperty_BytesRd);
            if ( prop.empty() ) {
                prop = "0";
            }
            if (need_space) {
                ostr << " ";
            }
            ostr << prop << " ";
            prop = GetProperty(kProperty_BytesWr);
            if ( prop.empty() ) {
                prop = "0";
            }
            ostr << prop;
            need_space = true;
            // Reset properties
            DeleteProperty(kProperty_ReqStatus);
            DeleteProperty(kProperty_ReqTime);
            DeleteProperty(kProperty_BytesRd);
            DeleteProperty(kProperty_BytesWr);
            break;
        }
    }
    if ( !message.empty() ) {
        if (need_space) {
            ostr << " ";
        }
        ostr << message;
    }
    SDiagMessage mess(eDiag_Info,
                      ostr.str(), ostr.pcount(),
                      0, 0, // file, line
                      eDPF_OmitInfoSev | eDPF_OmitSeparator | eDPF_AppLog,
                      NULL,
                      0, 0, // err code/subcode
                      NULL,
                      0, 0, 0, // module/class/function
                      GetPID(), buf.m_TID,
                      CDiagBuffer::GetProcessPostNumber(
                      CDiagBuffer::ePostNumber_Increment),
                      ++buf.m_ThreadPostCount,
                      GetDiagRequestId());
    mess.m_Event = event;
    buf.DiagHandler(mess);
    ostr.rdbuf()->freeze(false);
}


bool CDiagContext::IsSetOldPostFormat(void)
{
     return TOldPostFormatParam::GetDefault();
}


void CDiagContext::SetOldPostFormat(bool value)
{
    TOldPostFormatParam::SetDefault(value);
}


bool CDiagContext::IsUsingSystemThreadId(void)
{
     return TPrintSystemTID::GetDefault();
}


void CDiagContext::UseSystemThreadId(bool value)
{
    TPrintSystemTID::SetDefault(value);
}


void CDiagContext::SetUsername(const string& username)
{
    SetProperty(kProperty_UserName, username);
}


void CDiagContext::SetHostname(const string& hostname)
{
    SetProperty(kProperty_HostName, hostname);
}


void CDiagContext::InitMessages(size_t max_size)
{
    if ( !m_Messages.get() ) {
        m_Messages.reset(new TMessages);
    }
    m_MaxMessages = max_size;
}


void CDiagContext::PushMessage(const SDiagMessage& message)
{
    if ( m_Messages.get()  &&  m_Messages->size() < m_MaxMessages) {
        m_Messages->push_back(message);
    }
}


void CDiagContext::FlushMessages(CDiagHandler& handler)
{
    if ( !m_Messages.get()  ||  m_Messages->empty() ) {
        return;
    }
    auto_ptr<TMessages> tmp(m_Messages.release());
    //ERR_POST(Message << "***** BEGIN COLLECTED MESSAGES *****");
    ITERATE(TMessages, it, *tmp.get()) {
        handler.Post(*it);
    }
    //ERR_POST(Message << "***** END COLLECTED MESSAGES *****");
    m_Messages.reset(tmp.release());
}


void CDiagContext::DiscardMessages(void)
{
    m_Messages.reset();
}


// Diagnostics setup

static const char* kLogName_None     = "NONE";
static const char* kLogName_Unknown  = "UNKNOWN";
static const char* kLogName_Stdout   = "STDOUT";
static const char* kLogName_Stderr   = "STDERR";
static const char* kLogName_Stream   = "STREAM";
static const char* kLogName_Memory   = "MEMORY";

string GetDefaultLogLocation(CNcbiApplication& app)
{
    static const char* kToolkitRcPath = "/etc/toolkitrc";
    static const char* kWebDirToPort = "Web_dir_to_port";

    string log_path = "/log/";

    string exe_path = CFile(app.GetProgramExecutablePath()).GetDir();
    CNcbiIfstream is(kToolkitRcPath, ios::binary);
    CNcbiRegistry reg(is);
    list<string> entries;
    reg.EnumerateEntries(kWebDirToPort, &entries);
    size_t min_pos = exe_path.length();
    string web_dir;
    // Find the first dir name corresponding to one of the entries
    ITERATE(list<string>, it, entries) {
        if (!it->empty()  &&  (*it)[0] != '/') {
            // not an absolute path
            string mask = "/" + *it;
            if (mask[mask.length() - 1] != '/') {
                mask += "/";
            }
            size_t pos = exe_path.find(mask);
            if (pos < min_pos) {
                min_pos = pos;
                web_dir = *it;
            }
        }
        else {
            // absolute path
            if (exe_path.substr(0, it->length()) == *it) {
                web_dir = *it;
                break;
            }
        }
    }
    if ( !web_dir.empty() ) {
        return log_path + reg.GetString(kWebDirToPort, web_dir, kEmptyStr);
    }
    // Could not find a valid web-dir entry, use port or 'srv'
    const char* port = ::getenv("SERVER_PORT");
    return port ? log_path + string(port) : log_path + "srv";
}


NCBI_PARAM_DECL(bool, Log, Truncate);
NCBI_PARAM_DEF_EX(bool, Log, Truncate, false, eParam_NoThread, LOG_TRUNCATE);
typedef NCBI_PARAM_TYPE(Log, Truncate) TLogTruncateParam;


bool CDiagContext::GetLogTruncate(void)
{
    return TLogTruncateParam::GetDefault();
}


void CDiagContext::SetLogTruncate(bool value)
{
    TLogTruncateParam::SetDefault(value);
}


ios::openmode s_GetLogOpenMode(void)
{
    return ios::out |
        (CDiagContext::GetLogTruncate() ? ios::trunc : ios::app);
}


NCBI_PARAM_DECL(bool, Log, NoCreate);
NCBI_PARAM_DEF_EX(bool, Log, NoCreate, false, eParam_NoThread, LOG_NOCREATE);
typedef NCBI_PARAM_TYPE(Log, NoCreate) TLogNoCreate;

bool OpenLogFileFromConfig(CNcbiRegistry& config)
{
    string logname = config.GetString("LOG", "File", kEmptyStr);
    // In eDS_User mode do not use config unless IgnoreEnvArg
    // is set to true.
    if ( !logname.empty() ) {
        if ( TLogNoCreate::GetDefault()  &&  !CDirEntry(logname).Exists() ) {
            return false;
        }
        return SetLogFile(logname, eDiagFile_All, true);
    }
    return false;
}


void CDiagContext::SetupDiag(EAppDiagStream       ds,
                             CNcbiRegistry*       config,
                             EDiagCollectMessages collect)
{
    // Initialize message collecting
    if (collect == eDCM_Init) {
        GetDiagContext().InitMessages();
    }
    else if (collect == eDCM_InitNoLimit) {
        GetDiagContext().InitMessages(size_t(-1));
    }

    bool log_switched = false;
    bool try_root_log_first = false;
    if ( config ) {
        try_root_log_first = config->GetBool("LOG", "TryRootLogFirst", false)
            &&  (ds == eDS_ToStdlog  ||  ds == eDS_Default);
        bool force_config = config->GetBool("LOG", "IgnoreEnvArg", false);
        if ( force_config ) {
            try_root_log_first = false;
        }
        if (force_config  ||  (ds != eDS_User  &&  !try_root_log_first)) {
            log_switched = OpenLogFileFromConfig(*config);
        }
    }

    if ( !log_switched ) {
        string old_log_name;
        CDiagHandler* handler = GetDiagHandler();
        if ( handler ) {
            old_log_name = handler->GetLogName();
        }
        CNcbiApplication* app = CNcbiApplication::Instance();

        switch ( ds ) {
        case eDS_ToStdout:
            if (old_log_name != kLogName_Stdout) {
                SetDiagHandler(new CStreamDiagHandler(&cout,
                    true, kLogName_Stdout), true);
                log_switched = true;
            }
            break;
        case eDS_ToStderr:
            if (old_log_name != kLogName_Stderr) {
                SetDiagHandler(new CStreamDiagHandler(&cerr,
                    true, kLogName_Stderr), true);
                log_switched = true;
            }
            break;
        case eDS_ToMemory:
            if (old_log_name != kLogName_Memory) {
                GetDiagContext().InitMessages(size_t(-1));
                SetDiagStream(0, false, 0, 0, kLogName_Memory);
                log_switched = true;
            }
            collect = eDCM_NoChange; // prevent flushing to memory
            break;
        case eDS_Disable:
            if (old_log_name != kLogName_None) {
                SetDiagStream(0, false, 0, 0, kLogName_None);
                log_switched = true;
            }
            break;
        case eDS_User:
            // log_switched = true;
            collect = eDCM_Discard;
            break;
        case eDS_AppSpecific:
            if ( app ) {
                app->SetupDiag_AppSpecific(); /* NCBI_FAKE_WARNING */
            }
            collect = eDCM_Discard;
            break;
        case eDS_ToSyslog:
            if (old_log_name != CSysLog::kLogName_Syslog) {
                try {
                    SetDiagHandler(new CSysLog);
                    log_switched = true;
                    break;
                } catch (...) {
                    // fall through
                }
            } else {
                break;
            }
        case eDS_ToStdlog:
        case eDS_Default:
            if ( app ) {
                string log_base =
                    CFile(app->GetProgramExecutablePath()).GetBase() + ".log";
                // Try /log/<port>
                string log_name = CFile::ConcatPath(
                    GetDefaultLogLocation(*app), log_base);
                if ( SetLogFile(log_name, eDiagFile_All) ) {
                    log_switched = true;
                    break;
                }
                if ( try_root_log_first  &&  OpenLogFileFromConfig(*config) ) {
                    log_switched = true;
                    break;
                }
                // Try to switch to /log/fallback/
                log_name = CFile::ConcatPath("/log/fallback/", log_base);
                if ( SetLogFile(log_name, eDiagFile_All) ) {
                    log_switched = true;
                    break;
                }
                // Try cwd/ for eDS_ToStdlog only
                if (ds == eDS_ToStdlog) {
                    log_name = CFile::ConcatPath(".", log_base);
                    log_switched = SetLogFile(log_name, eDiagFile_All);
                }
                if ( !log_switched ) {
                    ERR_POST(Info << "Failed to set log file to " +
                        CFile::NormalizePath(log_name));
                }
            }
            else {
                static const char* kDefaultFallback = "/log/fallback/UNKNOWN";
                // Try to switch to /log/fallback/UNKNOWN
                if ( SetLogFile(kDefaultFallback, eDiagFile_All) ) {
                    log_switched = true;
                }
                else {
                    ERR_POST(Info <<
                        "Failed to set log file to " << kDefaultFallback);
                }
            }
            if (!log_switched  &&  old_log_name != kLogName_Stderr) {
                SetDiagHandler(new CStreamDiagHandler(&cerr,
                    true, kLogName_Stderr), true);
                log_switched = true;
            }
            break;
        default:
            ERR_POST(Warning << "Unknown EAppDiagStream value");
            _ASSERT(0);
            break;
        }
    }

    if (collect == eDCM_Flush) {
        CDiagHandler* handler = GetDiagHandler();
        if ( log_switched  &&  handler ) {
            GetDiagContext().FlushMessages(*handler);
        }
        collect = eDCM_Discard;
    }
    if (collect == eDCM_Discard) {
        GetDiagContext().DiscardMessages();
    }
}


CDiagContext& GetDiagContext(void)
{
    // Make the context live longer than other diag safe-statics
    static CSafeStaticPtr<CDiagContext> s_DiagContext(0,
        CSafeStaticLifeSpan(CSafeStaticLifeSpan::eLifeSpan_Long));

    return s_DiagContext.Get();
}


///////////////////////////////////////////////////////
//  CDiagBuffer::

#if defined(NDEBUG)
EDiagSev       CDiagBuffer::sm_PostSeverity       = eDiag_Error;
#else
EDiagSev       CDiagBuffer::sm_PostSeverity       = eDiag_Warning;
#endif /* else!NDEBUG */

EDiagSevChange CDiagBuffer::sm_PostSeverityChange = eDiagSC_Unknown;
                                                  // to be set on first request

static const TDiagPostFlags s_OldDefaultPostFlags =
    eDPF_Prefix | eDPF_Severity | eDPF_ErrorID | 
    eDPF_ErrCodeMessage | eDPF_ErrCodeExplanation |
    eDPF_ErrCodeUseSeverity | eDPF_AtomicWrite;
static const TDiagPostFlags s_NewDefaultPostFlags =
    s_OldDefaultPostFlags |
#if defined(NCBI_THREADS)
    eDPF_TID | eDPF_SerialNo_Thread |
#endif
    eDPF_PID | eDPF_SerialNo | eDPF_AtomicWrite;
static TDiagPostFlags s_PostFlags = 0;
static bool s_DiagPostFlagsInitialized = false;

inline
TDiagPostFlags& CDiagBuffer::sx_GetPostFlags(void)
{
    if (!s_DiagPostFlagsInitialized) {
        s_PostFlags = TOldPostFormatParam::GetDefault() ?
            s_OldDefaultPostFlags : s_NewDefaultPostFlags;
        s_DiagPostFlagsInitialized = true;
    }
    return s_PostFlags;
}


TDiagPostFlags& CDiagBuffer::s_GetPostFlags(void)
{
    return sx_GetPostFlags();
}


TDiagPostFlags CDiagBuffer::sm_TraceFlags         = eDPF_Trace;

bool           CDiagBuffer::sm_IgnoreToDie        = false;
EDiagSev       CDiagBuffer::sm_DieSeverity        = eDiag_Fatal;

EDiagTrace     CDiagBuffer::sm_TraceDefault       = eDT_Default;
bool           CDiagBuffer::sm_TraceEnabled;     // to be set on first request


const char*    CDiagBuffer::sm_SeverityName[eDiag_Trace+1] = {
    "Info", "Warning", "Error", "Critical", "Fatal", "Trace" };


void* InitDiagHandler(void)
{
    CDiagContext::SetupDiag(eDS_Default, 0, eDCM_Init);
    return 0;
}


// MT-safe initialization of the default handler
CDiagHandler* CreateDefaultDiagHandler(void)
{
    CMutexGuard guard(s_DiagMutex);
    return new CStreamDiagHandler(&NcbiCerr,
                                  true,
                                  kLogName_Stderr);
}


// Use s_DefaultHandler only for purposes of comparison, as installing
// another handler will normally delete it.
CDiagHandler*      s_DefaultHandler = CreateDefaultDiagHandler();
CDiagHandler*      CDiagBuffer::sm_Handler = s_DefaultHandler;
bool               CDiagBuffer::sm_CanDeleteHandler = true;
CDiagErrCodeInfo*  CDiagBuffer::sm_ErrCodeInfo = 0;
bool               CDiagBuffer::sm_CanDeleteErrCodeInfo = false;

// For initialization only
void* s_DiagHandlerInitializer = InitDiagHandler();


inline Uint8 s_GetThreadId(void)
{
    if (TPrintSystemTID::GetDefault()) {
        return (Uint8)(CThreadSystemID::GetCurrent().m_ID);
    } else {
        return CThread::GetSelf();
    }
}


CDiagBuffer::CDiagBuffer(void)
    : m_Stream(new CNcbiOstrstream),
      m_InitialStreamFlags(m_Stream->flags()),
      m_InUse(false),
      m_TID(s_GetThreadId()),
      m_ThreadPostCount(0)
{
    m_Diag = 0;
}

CDiagBuffer::~CDiagBuffer(void)
{
#if (_DEBUG > 1)
    if (m_Diag  ||  dynamic_cast<CNcbiOstrstream*>(m_Stream)->pcount())
        Abort();
#endif
    delete m_Stream;
    m_Stream = 0;
}

void CDiagBuffer::DiagHandler(SDiagMessage& mess)
{
    if ( CDiagBuffer::sm_Handler ) {
        CMutexGuard LOCK(s_DiagMutex);
        if ( CDiagBuffer::sm_Handler ) {
            CDiagBuffer& diag_buf = GetDiagBuffer();
            mess.m_Prefix = diag_buf.m_PostPrefix.empty() ?
                0 : diag_buf.m_PostPrefix.c_str();
            CDiagBuffer::sm_Handler->Post(mess);
        }
    }
    GetDiagContext().PushMessage(mess);
}

bool CDiagBuffer::SetDiag(const CNcbiDiag& diag)
{
    if ( m_InUse  ||  !m_Stream ) {
        return false;
    }

    // Check severity level change status
    if ( sm_PostSeverityChange == eDiagSC_Unknown ) {
        GetSeverityChangeEnabledFirstTime();
    }

    EDiagSev sev = diag.GetSeverity();
    if ((sev < sm_PostSeverity  &&  (sev < sm_DieSeverity  ||  sm_IgnoreToDie))
        ||  (sev == eDiag_Trace  &&  !GetTraceEnabled())) {
        return false;
    }

    if (m_Diag != &diag) {
        if ( dynamic_cast<CNcbiOstrstream*>(m_Stream)->pcount() ) {
            Flush();
        }
        m_Diag = &diag;
    }

    return true;
}


class CRecursionGuard
{
public:
    CRecursionGuard(bool& flag) : m_Flag(flag) { m_Flag = true; }
    ~CRecursionGuard(void) { m_Flag = false; }
private:
    bool& m_Flag;
};


void CDiagBuffer::Flush(void)
{
    if ( m_InUse ) {
        return;
    }
    CRecursionGuard guard(m_InUse);

    EDiagSev sev = m_Diag->GetSeverity();

    // Do nothing if diag severity is lower than allowed
    if (!m_Diag  ||
        (sev < sm_PostSeverity  &&  (sev < sm_DieSeverity  ||  sm_IgnoreToDie))
        ||  (sev == eDiag_Trace  &&  !GetTraceEnabled())) {
        return;
    }

    CNcbiOstrstream* ostr = dynamic_cast<CNcbiOstrstream*>(m_Stream);
    const char* message = 0;
    size_t size = 0;
    if ( ostr->pcount() ) {
        message = ostr->str();
        size = ostr->pcount();
        ostr->rdbuf()->freeze(false);
    }
    TDiagPostFlags flags = m_Diag->GetPostFlags();
    if (sev == eDiag_Trace) {
        flags |= sm_TraceFlags;
    } else if (sev == eDiag_Fatal) {
        // normally only happens once, so might as well pull everything
        // in for the record...
        flags |= sm_TraceFlags | eDPF_Trace;
    }

    if (  m_Diag->CheckFilters()  ) {
        string dest;
        if (IsSetDiagPostFlag(eDPF_PreMergeLines, flags)) {
            string src(message, size);
            NStr::Replace(NStr::Replace(src,"\r",""),"\n",";", dest);
            message = dest.c_str();
            size = dest.length();
        }
        SDiagMessage mess(sev, message, size,
                          m_Diag->GetFile(),
                          m_Diag->GetLine(),
                          flags,
                          NULL,
                          m_Diag->GetErrorCode(),
                          m_Diag->GetErrorSubCode(),
                          NULL,
                          m_Diag->GetModule(),
                          m_Diag->GetClass(),
                          m_Diag->GetFunction(),
                          CDiagContext::GetPID(), m_TID,
                          GetProcessPostNumber(
                          CDiagBuffer::ePostNumber_Increment),
                          ++m_ThreadPostCount,
                          GetDiagRequestId());
        DiagHandler(mess);
    }

#if defined(NCBI_COMPILER_KCC)
    // KCC's implementation of "freeze(false)" makes the ostrstream buffer
    // stuck.  We need to replace the frozen stream with the new one.
    delete ostr;
    m_Stream = new CNcbiOstrstream;
#else
    // reset flags to initial value
    m_Stream->flags(m_InitialStreamFlags);
#endif

    Reset(*m_Diag);

    if (sev >= sm_DieSeverity  &&  sev != eDiag_Trace  &&  !sm_IgnoreToDie) {
        m_Diag = 0;
#if defined(NCBI_OS_MAC)
        if ( g_Mac_SpecialEnvironment ) {
            throw runtime_error("Application aborted.");
        }
#endif
        Abort();
    }
}


bool CDiagBuffer::GetTraceEnabledFirstTime(void)
{
    CMutexGuard LOCK(s_DiagMutex);
    const char* str = ::getenv(DIAG_TRACE);
    if (str  &&  *str) {
        sm_TraceDefault = eDT_Enable;
    } else {
        sm_TraceDefault = eDT_Disable;
    }
    sm_TraceEnabled = (sm_TraceDefault == eDT_Enable);
    return sm_TraceEnabled;
}


bool CDiagBuffer::GetSeverityChangeEnabledFirstTime(void)
{
    CMutexGuard LOCK(s_DiagMutex);
    if ( sm_PostSeverityChange != eDiagSC_Unknown ) {
        return sm_PostSeverityChange == eDiagSC_Enable;
    }
    const char* str = ::getenv(DIAG_POST_LEVEL);
    EDiagSev sev;
    if (str  &&  *str  &&  CNcbiDiag::StrToSeverityLevel(str, sev)) {
        SetDiagFixedPostLevel(sev);
    } else {
        sm_PostSeverityChange = eDiagSC_Enable;
    }
    return sm_PostSeverityChange == eDiagSC_Enable;
}


void CDiagBuffer::UpdatePrefix(void)
{
    m_PostPrefix.erase();
    ITERATE(TPrefixList, prefix, m_PrefixList) {
        if (prefix != m_PrefixList.begin()) {
            m_PostPrefix += "::";
        }
        m_PostPrefix += *prefix;
    }
}


int CDiagBuffer::GetProcessPostNumber(EPostNumberIncrement inc)
{
    static CAtomicCounter s_ProcessPostCount;
    return inc ? s_ProcessPostCount.Add(1) : s_ProcessPostCount.Get();
}


///////////////////////////////////////////////////////
//  CDiagMessage::


int s_ParseInt(const string& message,
               size_t&       pos,    // start position
               size_t        width,  // fixed width or 0
               char          sep)    // trailing separator (throw if not found)
{
    if (pos >= message.length()) {
        NCBI_THROW(CException, eUnknown,
            "Failed to parse diagnostic message");
    }
    int ret = 0;
    if (width > 0) {
        if (message[pos + width] != sep) {
            NCBI_THROW(CException, eUnknown,
                "Missing separator after integer");
        }
    }
    else {
        width = message.find(sep, pos);
        if (width == NPOS) {
            NCBI_THROW(CException, eUnknown,
                "Missing separator after integer");
        }
        width -= pos;
    }

    ret = NStr::StringToInt(message.substr(pos, width));
    pos += width + 1;
    return ret;
}


string s_ParseStr(const string& message,
                  size_t&       pos,             // start position
                  char          sep,             // separator
                  bool          optional = false) // do not throw if not found
{
    if (pos >= message.length()) {
        NCBI_THROW(CException, eUnknown,
            "Failed to parse diagnostic message");
    }
    size_t pos1 = pos;
    pos = message.find(sep, pos1);
    if (pos == NPOS) {
        if ( !optional ) {
            NCBI_THROW(CException, eUnknown,
                "Failed to parse diagnostic message");
        }
        pos = pos1;
        return kEmptyStr;
    }
    if ( pos == pos1 + 1  &&  !optional ) {
        // The separator is in the next position, no empty string allowed
        NCBI_THROW(CException, eUnknown,
            "Failed to parse diagnostic message");
    }
    while (pos < message.length()  &&  message[pos] == sep) {
        pos++;
    }
    return message.substr(pos1, pos - pos1 - 1);
}


SDiagMessage::SDiagMessage(const string& message)
    : m_Severity(eDiagSevMin),
      m_Buffer(0),
      m_BufferLen(0),
      m_File(0),
      m_Module(0),
      m_Class(0),
      m_Function(0),
      m_Line(0),
      m_ErrCode(0),
      m_ErrSubCode(0),
      m_Flags(0),
      m_Prefix(0),
      m_ErrText(0),
      m_PID(0),
      m_TID(0),
      m_ProcPost(0),
      m_ThrPost(0),
      m_RequestId(0),
      m_Data(0),
      m_Format(eFormat_Auto)
{
    size_t pos = 0;
    m_Data = new SDiagMessageData;

    try {
        // Fixed prefix
        m_PID = s_ParseInt(message, pos, kDiagW_PID, '/');
        m_TID = s_ParseInt(message, pos, kDiagW_TID, '/');
        m_RequestId = s_ParseInt(message, pos, kDiagW_RID, ' ');

        if (message[pos + kDiagW_UID] != ' ') {
            return;
        }
        m_Data->m_UID =
            NStr::StringToUInt8(message.substr(pos, kDiagW_UID), 0, 16);
        pos += kDiagW_UID + 1;
        
        m_ProcPost = s_ParseInt(message, pos, kDiagW_SN, '/');
        m_ThrPost = s_ParseInt(message, pos, kDiagW_SN, ' ');

        // Date and time
        string tmp = s_ParseStr(message, pos, ' ');
        m_Data->m_Time = CTime(tmp, kDiagTimeFormat);
        // Host
        m_Data->m_Host = s_ParseStr(message, pos, ' ');
        // Client
        m_Data->m_Client = s_ParseStr(message, pos, ' ');
        // Session ID
        m_Data->m_Session = s_ParseStr(message, pos, ' ');
        // Application name
        m_Data->m_AppName = s_ParseStr(message, pos, ' ');

        // Severity or event type
        bool have_severity = false;
        size_t severity_pos = pos;
        tmp = s_ParseStr(message, pos, ':', true);
        if ( !tmp.empty() ) {
            if ( tmp.find("Message[") == 0  &&  tmp.length() == 10 ) {
                // Get the real severity
                switch ( tmp[8] ) {
                case 'T':
                    m_Severity = eDiag_Trace;
                    break;
                case 'I':
                    m_Severity = eDiag_Info;
                    break;
                case 'W':
                    m_Severity = eDiag_Warning;
                    break;
                case 'E':
                    m_Severity = eDiag_Error;
                    break;
                case 'C':
                    m_Severity = eDiag_Critical;
                    break;
                case 'F':
                    m_Severity = eDiag_Fatal;
                    break;
                default:
                    NCBI_THROW(CException, eUnknown,
                        "Unknown message severity");
                }
                m_Flags |= eDPF_IsMessage;
                have_severity = true;
            }
            else {
                have_severity =
                    CNcbiDiag::StrToSeverityLevel(tmp.c_str(), m_Severity);
            }
        }
        if ( have_severity ) {
            do {
                pos++;
            } while (pos < message.length()  &&  message[pos] == ' ');
        }
        else {
            // Check event type rather than severity level
            pos = severity_pos;
            tmp = s_ParseStr(message, pos, ' ', true);
            if (tmp.empty()  &&  severity_pos < message.length()) {
                tmp = message.substr(severity_pos, message.length());
                pos = message.length();
            }
            if (tmp == GetEventName(eEvent_Start)) {
                m_Event = eEvent_Start;
            }
            else if (tmp == GetEventName(eEvent_Stop)) {
                m_Event = eEvent_Stop;
            }
            else if (tmp == GetEventName(eEvent_RequestStart)) {
                m_Event = eEvent_RequestStart;
            }
            else if (tmp == GetEventName(eEvent_RequestStop)) {
                m_Event = eEvent_RequestStop;
            }
            else if (tmp == GetEventName(eEvent_Extra)) {
                m_Event = eEvent_Extra;
            }
            else {
                return;
            }
            // The rest is the message (do not parse status, bytes etc.)
            if (pos < message.length()) {
                m_Data->m_Message = message.substr(pos, message.length());
                m_Buffer = &m_Data->m_Message[0];
                m_BufferLen = m_Data->m_Message.length();
            }
            m_Format = eFormat_New;
            return;
        }

        // <module>, <module>(<err_code>.<err_subcode>) or <module>(<err_text>)
        size_t mod_pos = pos;
        tmp = s_ParseStr(message, pos, ' ');
        size_t lbr = tmp.find("(");
        if (lbr != NPOS) {
            if (tmp[tmp.length() - 1] != ')') {
                // Space(s) inside the error text, try to find closing ')'
                int open_br = 1;
                while (pos < message.length()  &&  open_br > 0) {
                    if (message[pos] == '(') {
                        open_br++;
                    }
                    else if (message[pos] == ')') {
                        open_br--;
                    }
                    pos++;
                }
                if (pos >= message.length()  ||  message[pos] != ' ') {
                    return;
                }
                tmp = message.substr(mod_pos, pos - mod_pos);
                // skip space(s)
                while (pos < message.length()  &&  message[pos] == ' ') {
                    pos++;
                }
            }
            m_Data->m_Module = tmp.substr(0, lbr);
            tmp = tmp.substr(lbr + 1, tmp.length() - lbr - 2);
            size_t dot_pos = tmp.find('.');
            if (dot_pos != NPOS) {
                // Try to parse error code/subcode
                try {
                    m_ErrCode = NStr::StringToInt(tmp.substr(0, dot_pos));
                    m_ErrSubCode = NStr::StringToInt(
                        tmp.substr(dot_pos + 1, tmp.length()));
                }
                catch (CStringException) {
                    m_ErrCode = 0;
                    m_ErrSubCode = 0;
                }
            }
            if (!m_ErrCode  &&  !m_ErrSubCode) {
                m_Data->m_ErrText = tmp;
                m_ErrText = m_Data->m_ErrText.c_str();
            }
        }
        else {
            m_Data->m_Module = tmp;
        }
        if ( !m_Data->m_Module.empty() ) {
            m_Module = m_Data->m_Module.c_str();
        }
        
        // ["<file>", ][line <line>][:]
        if (message[pos] != '"') {
            return;
        }
        pos++; // skip "
        tmp = s_ParseStr(message, pos, '"');
        m_Data->m_File = tmp;
        m_File = m_Data->m_File.c_str();
        if (message.substr(pos, 7) != ", line ") {
            return;
        }
        pos += 7;
        m_Line = s_ParseInt(message, pos, 0, ':');
        while (pos < message.length()  &&  message[pos] == ' ') {
            pos++;
        }

        // Class:: Class::Function() ::Function()
        tmp = s_ParseStr(message, pos, ' ');
        size_t dcol = tmp.find("::");
        if (dcol == NPOS) {
            return;
        }
        if (dcol > 0) {
            m_Data->m_Class = tmp.substr(0, dcol);
            m_Class = m_Data->m_Class.c_str();
        }
        dcol += 2;
        if (dcol < tmp.length() - 2) {
            // Remove "()"
            if (tmp[tmp.length() - 2] != '(' || tmp[tmp.length() - 1] != ')') {
                return;
            }
            tmp.resize(tmp.length() - 2);
            m_Data->m_Function = tmp.substr(dcol, tmp.length());
            m_Function = m_Data->m_Function.c_str();
        }
        else {
            return;
        }

        if (message.substr(pos, 4) != "--- ") {
            return;
        }
        pos += 4;

        // All the rest goes to message - no way to parse prefix/error code.
        // [<prefix1>::<prefix2>::.....]
        // <message>
        // <err_code_message> and <err_code_explanation>
        m_Data->m_Message = message.substr(pos, message.length());
        m_Buffer = &m_Data->m_Message[0];
        m_BufferLen = m_Data->m_Message.length();
    }
    catch (CException) {
        return;
    }

    m_Format = eFormat_New;
}


SDiagMessage::~SDiagMessage(void)
{
    if ( m_Data ) {
        delete m_Data;
    }
}


SDiagMessage::SDiagMessage(const SDiagMessage& message)
{
    *this = message;
}


SDiagMessage& SDiagMessage::operator=(const SDiagMessage& message)
{
    if (&message != this) {
        m_Format = message.m_Format;
        if ( message.m_Data ) {
            m_Data = new SDiagMessageData(*message.m_Data);
        }
        else {
            m_Data = new SDiagMessageData;
            if (message.m_Buffer) {
                m_Data->m_Message =
                    string(message.m_Buffer, message.m_BufferLen);
            }
            if ( message.m_File ) {
                m_Data->m_File = message.m_File;
            }
            if ( message.m_Module ) {
                m_Data->m_Module = message.m_Module;
            }
            if ( message.m_Class ) {
                m_Data->m_Class = message.m_Class;
            }
            if ( message.m_Function ) {
                m_Data->m_Function = message.m_Function;
            }
            if ( message.m_Prefix ) {
                m_Data->m_Prefix = message.m_Prefix;
            }
            if ( message.m_ErrText ) {
                m_Data->m_ErrText = message.m_ErrText;
            }
        }
        m_Severity = message.m_Severity;
        m_Line = message.m_Line;
        m_ErrCode = message.m_ErrCode;
        m_ErrSubCode = message.m_ErrSubCode;
        m_Flags = message.m_Flags;
        m_PID = message.m_PID;
        m_TID = message.m_TID;
        m_ProcPost = message.m_ProcPost;
        m_ThrPost = message.m_ThrPost;
        m_RequestId = message.m_RequestId;
        m_Event = message.m_Event;

        m_Buffer = m_Data->m_Message.empty() ? 0 : m_Data->m_Message.c_str();
        m_BufferLen = m_Data->m_Message.empty() ?
            0 : m_BufferLen = m_Data->m_Message.length();
        m_File = m_Data->m_File.empty() ? 0 : m_Data->m_File.c_str();
        m_Module = m_Data->m_Module.empty() ? 0 : m_Data->m_Module.c_str();
        m_Class = m_Data->m_Class.empty() ? 0 : m_Data->m_Class.c_str();
        m_Function = m_Data->m_Function.empty()
            ? 0 : m_Data->m_Function.c_str();
        m_Prefix = m_Data->m_Prefix.empty() ? 0 : m_Data->m_Prefix.c_str();
        m_ErrText = m_Data->m_ErrText.empty() ? 0 : m_Data->m_ErrText.c_str();
    }
    return *this;
}


string SDiagMessage::GetEventName(EEventType event)
{
    switch ( event ) {
    case eEvent_Start:
        return "start";
    case eEvent_Stop:
        return "stop";
    case eEvent_Extra:
        return "extra";
    case eEvent_RequestStart:
        return "request-start";
    case eEvent_RequestStop:
        return "request-stop";
    }
    return kEmptyStr;
}


CDiagContext::TUID SDiagMessage::GetUID(void) const
{
    return m_Data ? m_Data->m_UID : GetDiagContext().GetUID();
}


CTime SDiagMessage::GetTime(void) const
{
    return m_Data ? m_Data->m_Time : CTime(CTime::eCurrent);
}


inline
bool SDiagMessage::x_IsSetOldFormat(void) const
{
    return m_Format == eFormat_Auto ? GetDiagContext().IsSetOldPostFormat()
        : m_Format == eFormat_Old;
}


void SDiagMessage::Write(string& str, TDiagWriteFlags flags) const
{
    CNcbiOstrstream ostr;
    Write(ostr, flags);
    ostr.put('\0');
    str = ostr.str();
    ostr.rdbuf()->freeze(false);
}


CNcbiOstream& SDiagMessage::Write(CNcbiOstream&   os,
                                  TDiagWriteFlags flags) const
{
    // GetDiagContext().PushMessage(*this);

    if (IsSetDiagPostFlag(eDPF_MergeLines, m_Flags)) {
        CNcbiOstrstream ostr;
        string src, dest;
        x_Write(ostr, fNoEndl);
        ostr.put('\0');
        src = ostr.str();
        ostr.rdbuf()->freeze(false);
        NStr::Replace(NStr::Replace(src,"\r",""),"\n","", dest);
        os << dest;
        if ((flags & fNoEndl) == 0) {
            os << NcbiEndl;
        }
        return os;
    } else {
        return x_Write(os, flags);
    }
}


CNcbiOstream& SDiagMessage::x_Write(CNcbiOstream& os,
                                    TDiagWriteFlags flags) const
{
    return x_IsSetOldFormat() ? x_OldWrite(os, flags) : x_NewWrite(os, flags);
}


string SDiagMessage::x_GetModule(void) const
{
    if ( m_Module && *m_Module ) {
        return string(m_Module);
    }
    if ( x_IsSetOldFormat() ) {
        return kEmptyStr;
    }
    if ( !m_File || !(*m_File) ) {
        return kEmptyStr;
    }
    char sep_chr = CDirEntry::GetPathSeparator();
    const char* last_sep = strrchr(m_File, sep_chr);
    if ( !last_sep || !*last_sep ) {
        return kEmptyStr;
    }
    const char* sep = strchr(m_File, sep_chr);
    while (sep < last_sep) {
        _ASSERT(sep && *sep);
        const char* next_sep = strchr(sep+1, sep_chr);
        if (next_sep == last_sep) {
            string ret(sep+1, next_sep - sep - 1);
            NStr::ToUpper(ret);
            return ret;
        }
        sep = next_sep;
    }
    return kEmptyStr;
}


CNcbiOstream& SDiagMessage::x_OldWrite(CNcbiOstream& os,
                                       TDiagWriteFlags flags) const
{
    // Date & time
    if (IsSetDiagPostFlag(eDPF_DateTime, m_Flags)) {
        static const char timefmt[] = "%D %T ";
        time_t t = time(0);
        char datetime[32];
        struct tm* tm;
#ifdef HAVE_LOCALTIME_R
        struct tm temp;
        localtime_r(&t, &temp);
        tm = &temp;
#else
        tm = localtime(&t);
#endif /*HAVE_LOCALTIME_R*/
        NStr::strftime(datetime, sizeof(datetime), timefmt, tm);
        os << datetime;
    }
    // "<file>"
    bool print_file = (m_File  &&  *m_File  &&
                       IsSetDiagPostFlag(eDPF_File, m_Flags));
    if ( print_file ) {
        const char* x_file = m_File;
        if ( !IsSetDiagPostFlag(eDPF_LongFilename, m_Flags) ) {
            for (const char* s = m_File;  *s;  s++) {
                if (*s == '/'  ||  *s == '\\'  ||  *s == ':')
                    x_file = s + 1;
            }
        }
        os << '"' << x_file << '"';
    }

    // , line <line>
    bool print_line = (m_Line  &&  IsSetDiagPostFlag(eDPF_Line, m_Flags));
    if ( print_line )
        os << (print_file ? ", line " : "line ") << m_Line;

    // :
    if (print_file  ||  print_line)
        os << ": ";

    // Get error code description
    bool have_description = false;
    SDiagErrCodeDescription description;
    if ((m_ErrCode  ||  m_ErrSubCode)  &&
        (IsSetDiagPostFlag(eDPF_ErrCodeMessage, m_Flags)  || 
         IsSetDiagPostFlag(eDPF_ErrCodeExplanation, m_Flags)  ||
         IsSetDiagPostFlag(eDPF_ErrCodeUseSeverity, m_Flags))  &&
         IsSetDiagErrCodeInfo()) {

        CDiagErrCodeInfo* info = GetDiagErrCodeInfo();
        if ( info  && 
             info->GetDescription(ErrCode(m_ErrCode, m_ErrSubCode), 
                                  &description) ) {
            have_description = true;
            if (IsSetDiagPostFlag(eDPF_ErrCodeUseSeverity, m_Flags) && 
                description.m_Severity != -1 )
                m_Severity = (EDiagSev)description.m_Severity;
        }
    }

    // <severity>:
    if (IsSetDiagPostFlag(eDPF_Severity, m_Flags)  &&
        (m_Severity != eDiag_Info || !IsSetDiagPostFlag(eDPF_OmitInfoSev))) {
        if ( IsSetDiagPostFlag(eDPF_IsMessage, m_Flags) ) {
            os << "Message: ";
        }
        else {
            os << CNcbiDiag::SeverityName(m_Severity) << ": ";
        }
    }

    // (<err_code>.<err_subcode>) or (err_text)
    if ((m_ErrCode  ||  m_ErrSubCode || m_ErrText)  &&
        IsSetDiagPostFlag(eDPF_ErrCode, m_Flags)) {
        os << '(';
        if (m_ErrText) {
            os << m_ErrText;
        } else {
            os << m_ErrCode;
            if ( IsSetDiagPostFlag(eDPF_ErrSubCode, m_Flags)) {
                os << '.' << m_ErrSubCode;
            }
        }
        os << ") ";
    }

    // Module::Class::Function -
    bool have_module = (m_Module && *m_Module);
    bool print_location =
        ( have_module ||
         (m_Class     &&  *m_Class ) ||
         (m_Function  &&  *m_Function))
        && IsSetDiagPostFlag(eDPF_Location, m_Flags);

    if (print_location) {
        // Module:: Module::Class Module::Class::Function()
        // ::Class ::Class::Function()
        // Module::Function() Function()
        bool need_double_colon = false;

        if ( have_module ) {
            os << x_GetModule();
            need_double_colon = true;
        }

        if (m_Class  &&  *m_Class) {
            if (need_double_colon)
                os << "::";
            os << m_Class;
            need_double_colon = true;
        }

        if (m_Function  &&  *m_Function) {
            if (need_double_colon)
                os << "::";
            need_double_colon = false;
            os << m_Function << "()";
        }

        if( need_double_colon )
            os << "::";

        os << " - ";
    }

    // [<prefix1>::<prefix2>::.....]
    if (m_Prefix  &&  *m_Prefix  &&  IsSetDiagPostFlag(eDPF_Prefix, m_Flags))
        os << '[' << m_Prefix << "] ";

    // <message>
    if (m_BufferLen)
        os.write(m_Buffer, m_BufferLen);

    // <err_code_message> and <err_code_explanation>
    if (have_description) {
        if (IsSetDiagPostFlag(eDPF_ErrCodeMessage, m_Flags) &&
            !description.m_Message.empty())
            os << NcbiEndl << description.m_Message << ' ';
        if (IsSetDiagPostFlag(eDPF_ErrCodeExplanation, m_Flags) &&
            !description.m_Explanation.empty())
            os << NcbiEndl << description.m_Explanation;
    }

    // Endl
    if ((flags & fNoEndl) == 0) {
        os << NcbiEndl;
    }

    return os;
}


CNcbiOstream& SDiagMessage::x_NewWrite(CNcbiOstream& os,
                                       TDiagWriteFlags flags) const
{
    GetDiagContext().WriteStdPrefix(os, this);

    // Get error code description
    bool have_description = false;
    SDiagErrCodeDescription description;
    if ((m_ErrCode  ||  m_ErrSubCode)  &&
        IsSetDiagPostFlag(eDPF_ErrCodeUseSeverity, m_Flags)  &&
        IsSetDiagErrCodeInfo()) {

        CDiagErrCodeInfo* info = GetDiagErrCodeInfo();
        if ( info  && 
             info->GetDescription(ErrCode(m_ErrCode, m_ErrSubCode), 
                                  &description) ) {
            have_description = true;
            if (description.m_Severity != -1)
                m_Severity = (EDiagSev)description.m_Severity;
        }
    }

    // <severity>:
    if ( IsSetDiagPostFlag(eDPF_AppLog, m_Flags) ) {
        os << GetEventName(m_Event) << " ";
    }
    else {
        string sev = CNcbiDiag::SeverityName(m_Severity);
        if ( IsSetDiagPostFlag(eDPF_IsMessage, m_Flags) ) {
            os << "Message[" << sev[0] << "]: ";
        }
        else {
            os << sev << ": ";
        }
    }

    // <module>-<err_code>.<err_subcode> or <module>-<err_text>
    bool have_module = (m_Module && *m_Module) || (m_File && *m_File);
    bool print_err_id = have_module || m_ErrCode || m_ErrSubCode || m_ErrText;

    if (print_err_id) {
        if ( have_module ) {
            os << x_GetModule();
        }
        if (m_ErrCode  ||  m_ErrSubCode || m_ErrText) {
            if (m_ErrText) {
                os << "(" << m_ErrText << ")";
            } else {
                os << "(" << m_ErrCode << '.' << m_ErrSubCode << ")";
            }
        }
        os << " ";
    }

    // "<file>"
    bool print_file = m_File  &&  *m_File;
    if ( print_file ) {
        const char* x_file = m_File;
        if ( !IsSetDiagPostFlag(eDPF_LongFilename, m_Flags) ) {
            for (const char* s = m_File;  *s;  s++) {
                if (*s == '/'  ||  *s == '\\'  ||  *s == ':')
                    x_file = s + 1;
            }
        }
        os << '"' << x_file << '"';
    }
    // , line <line>
    if ( m_Line )
        os << (print_file ? ", line " : "line ") << m_Line;
    // :
    if (print_file  ||  m_Line)
        os << ": ";

    // Class::Function
    bool print_loc = (m_Class && *m_Class ) || (m_Function && *m_Function);
    if (print_loc) {
        // Class:: Class::Function() ::Function()
        if (m_Class  &&  *m_Class) {
            os << m_Class;
        }
        os << "::";
        if (m_Function  &&  *m_Function) {
            os << m_Function << "() ";
        }
    }

    if ( !IsSetDiagPostFlag(eDPF_OmitSeparator, m_Flags)) {
        os << "--- ";
    }

    // [<prefix1>::<prefix2>::.....]
    if (m_Prefix  &&  *m_Prefix  &&  IsSetDiagPostFlag(eDPF_Prefix, m_Flags))
        os << '[' << m_Prefix << "] ";

    // <message>
    if (m_BufferLen)
        os.write(m_Buffer, m_BufferLen);

    // <err_code_message> and <err_code_explanation>
    if (have_description) {
        if (IsSetDiagPostFlag(eDPF_ErrCodeMessage, m_Flags) &&
            !description.m_Message.empty())
            os << NcbiEndl << description.m_Message << ' ';
        if (IsSetDiagPostFlag(eDPF_ErrCodeExplanation, m_Flags) &&
            !description.m_Explanation.empty())
            os << NcbiEndl << description.m_Explanation;
    }

    // Endl
    if ((flags & fNoEndl) == 0) {
        os << NcbiEndl;
    }

    return os;
}


void SDiagMessage::x_InitData(void) const
{
    if ( !m_Data ) {
        m_Data = new SDiagMessageData;
    }
    if (m_Data->m_Message.empty()  &&  m_Buffer) {
        m_Data->m_Message = string(m_Buffer, m_BufferLen);
    }
    if (m_Data->m_File.empty()  &&  m_File) {
        m_Data->m_File = m_File;
    }
    if (m_Data->m_Module.empty()  &&  m_Module) {
        m_Data->m_Module = m_Module;
    }
    if (m_Data->m_Class.empty()  &&  m_Class) {
        m_Data->m_Class = m_Class;
    }
    if (m_Data->m_Function.empty()  &&  m_Function) {
        m_Data->m_Function = m_Function;
    }
    if (m_Data->m_Prefix.empty()  &&  m_Prefix) {
        m_Data->m_Prefix = m_Prefix;
    }
    if (m_Data->m_ErrText.empty()  &&  m_ErrText) {
        m_Data->m_ErrText = m_ErrText;
    }

    if ( !m_Data->m_UID ) {
        m_Data->m_UID = GetDiagContext().GetUID();
    }
    if ( m_Data->m_Time.IsEmpty() ) {
        m_Data->m_Time = CTime(CTime::eCurrent);
    }
}


void SDiagMessage::x_SaveProperties(const string& host,
                                    const string& client,
                                    const string& session,
                                    const string& app_name) const
{
    x_InitData();
    // Do not update properties if already set
    if ( m_Data->m_Host.empty() ) {
        m_Data->m_Host = host;
    }
    if ( m_Data->m_Client.empty() ) {
        m_Data->m_Client = client;
    }
    if ( m_Data->m_Session.empty() ) {
        m_Data->m_Session = session;
    }
    if ( m_Data->m_AppName.empty() ) {
        m_Data->m_AppName = app_name;
    }
}


///////////////////////////////////////////////////////
//  CDiagAutoPrefix::

CDiagAutoPrefix::CDiagAutoPrefix(const string& prefix)
{
    PushDiagPostPrefix(prefix.c_str());
}

CDiagAutoPrefix::CDiagAutoPrefix(const char* prefix)
{
    PushDiagPostPrefix(prefix);
}

CDiagAutoPrefix::~CDiagAutoPrefix(void)
{
    PopDiagPostPrefix();
}


///////////////////////////////////////////////////////
//  EXTERN


static TDiagPostFlags s_SetDiagPostAllFlags(TDiagPostFlags& flags,
                                            TDiagPostFlags  new_flags)
{
    CMutexGuard LOCK(s_DiagMutex);

    TDiagPostFlags prev_flags = flags;
    if (new_flags & eDPF_Default) {
        new_flags |= prev_flags;
        new_flags &= ~eDPF_Default;
    }
    flags = new_flags;
    return prev_flags;
}


static void s_SetDiagPostFlag(TDiagPostFlags& flags, EDiagPostFlag flag)
{
    if (flag == eDPF_Default)
        return;

    CMutexGuard LOCK(s_DiagMutex);
    flags |= flag;
}


static void s_UnsetDiagPostFlag(TDiagPostFlags& flags, EDiagPostFlag flag)
{
    if (flag == eDPF_Default)
        return;

    CMutexGuard LOCK(s_DiagMutex);
    flags &= ~flag;
}


extern TDiagPostFlags SetDiagPostAllFlags(TDiagPostFlags flags)
{
    return s_SetDiagPostAllFlags(CDiagBuffer::sx_GetPostFlags(), flags);
}

extern void SetDiagPostFlag(EDiagPostFlag flag)
{
    s_SetDiagPostFlag(CDiagBuffer::sx_GetPostFlags(), flag);
}

extern void UnsetDiagPostFlag(EDiagPostFlag flag)
{
    s_UnsetDiagPostFlag(CDiagBuffer::sx_GetPostFlags(), flag);
}


extern TDiagPostFlags SetDiagTraceAllFlags(TDiagPostFlags flags)
{
    return s_SetDiagPostAllFlags(CDiagBuffer::sm_TraceFlags, flags);
}

extern void SetDiagTraceFlag(EDiagPostFlag flag)
{
    s_SetDiagPostFlag(CDiagBuffer::sm_TraceFlags, flag);
}

extern void UnsetDiagTraceFlag(EDiagPostFlag flag)
{
    s_UnsetDiagPostFlag(CDiagBuffer::sm_TraceFlags, flag);
}


extern void SetDiagPostPrefix(const char* prefix)
{
    CDiagBuffer& buf = GetDiagBuffer();
    if ( prefix ) {
        buf.m_PostPrefix = prefix;
    } else {
        buf.m_PostPrefix.erase();
    }
    buf.m_PrefixList.clear();
}


extern void PushDiagPostPrefix(const char* prefix)
{
    if (prefix  &&  *prefix) {
        CDiagBuffer& buf = GetDiagBuffer();
        buf.m_PrefixList.push_back(prefix);
        buf.UpdatePrefix();
    }
}


extern void PopDiagPostPrefix(void)
{
    CDiagBuffer& buf = GetDiagBuffer();
    if ( !buf.m_PrefixList.empty() ) {
        buf.m_PrefixList.pop_back();
        buf.UpdatePrefix();
    }
}


extern EDiagSev SetDiagPostLevel(EDiagSev post_sev)
{
    if (post_sev < eDiagSevMin  ||  post_sev > eDiagSevMax) {
        NCBI_THROW(CCoreException, eInvalidArg,
                   "SetDiagPostLevel() -- Severity must be in the range "
                   "[eDiagSevMin..eDiagSevMax]");
    }

    CMutexGuard LOCK(s_DiagMutex);
    EDiagSev sev = CDiagBuffer::sm_PostSeverity;
    if ( CDiagBuffer::sm_PostSeverityChange != eDiagSC_Disable) {
        if (post_sev == eDiag_Trace) {
            // special case
            SetDiagTrace(eDT_Enable);
            post_sev = eDiag_Info;
        }
        CDiagBuffer::sm_PostSeverity = post_sev;
    }
    return sev;
}


extern int CompareDiagPostLevel(EDiagSev sev1, EDiagSev sev2)
{
    if (sev1 == sev2) return 0;
    if (sev1 == eDiag_Trace) return -1;
    if (sev2 == eDiag_Trace) return 1;
    return sev1 - sev2;
}


extern bool IsVisibleDiagPostLevel(EDiagSev sev)
{
    if (sev == eDiag_Trace) {
        return CDiagBuffer::GetTraceEnabled();
    }
    EDiagSev sev2;
    {{
        CMutexGuard LOCK(s_DiagMutex);
        sev2 = CDiagBuffer::sm_PostSeverity;
    }}
    return CompareDiagPostLevel(sev, sev2) >= 0;
}


extern void SetDiagFixedPostLevel(const EDiagSev post_sev)
{
    SetDiagPostLevel(post_sev);
    DisableDiagPostLevelChange();
}


extern bool DisableDiagPostLevelChange(bool disable_change)
{
    CMutexGuard LOCK(s_DiagMutex);
    bool prev_status = (CDiagBuffer::sm_PostSeverityChange == eDiagSC_Enable);
    CDiagBuffer::sm_PostSeverityChange = disable_change ? eDiagSC_Disable : 
                                                          eDiagSC_Enable;
    return prev_status;
}


extern EDiagSev SetDiagDieLevel(EDiagSev die_sev)
{
    if (die_sev < eDiagSevMin  ||  die_sev > eDiag_Fatal) {
        NCBI_THROW(CCoreException, eInvalidArg,
                   "SetDiagDieLevel() -- Severity must be in the range "
                   "[eDiagSevMin..eDiag_Fatal]");
    }

    CMutexGuard LOCK(s_DiagMutex);
    EDiagSev sev = CDiagBuffer::sm_DieSeverity;
    CDiagBuffer::sm_DieSeverity = die_sev;
    return sev;
}


extern bool IgnoreDiagDieLevel(bool ignore)
{
    CMutexGuard LOCK(s_DiagMutex);
    bool retval = CDiagBuffer::sm_IgnoreToDie;
    CDiagBuffer::sm_IgnoreToDie = ignore;
    return retval;
}


extern void SetDiagTrace(EDiagTrace how, EDiagTrace dflt)
{
    CMutexGuard LOCK(s_DiagMutex);
    (void) CDiagBuffer::GetTraceEnabled();

    if (dflt != eDT_Default)
        CDiagBuffer::sm_TraceDefault = dflt;

    if (how == eDT_Default)
        how = CDiagBuffer::sm_TraceDefault;
    CDiagBuffer::sm_TraceEnabled = (how == eDT_Enable);
}


extern void SetDiagHandler(CDiagHandler* handler, bool can_delete)
{
    CMutexGuard LOCK(s_DiagMutex);
    CDiagContext& ctx = GetDiagContext();
    bool report_switch = ctx.IsSetOldPostFormat()  &&
        CDiagBuffer::GetProcessPostNumber(
        CDiagBuffer::ePostNumber_NoIncrement) > 0;
    string old_name, new_name;

    if ( CDiagBuffer::sm_Handler ) {
        old_name = CDiagBuffer::sm_Handler->GetLogName();
    }
    if ( handler ) {
        new_name = handler->GetLogName();
        if (report_switch  &&  new_name != old_name) {
            ctx.PrintExtra("Switching diagnostics to " + new_name);
        }
    }
    if ( CDiagBuffer::sm_CanDeleteHandler )
        delete CDiagBuffer::sm_Handler;
    CDiagBuffer::sm_Handler          = handler;
    CDiagBuffer::sm_CanDeleteHandler = can_delete;
    if (report_switch  &&  !old_name.empty()  &&  new_name != old_name) {
        ctx.PrintExtra("Switched diagnostics from " + old_name);
    }
}


extern bool IsSetDiagHandler(void)
{
    return (CDiagBuffer::sm_Handler != s_DefaultHandler);
}

extern CDiagHandler* GetDiagHandler(bool take_ownership)
{
    CMutexGuard LOCK(s_DiagMutex);
    if (take_ownership) {
        _ASSERT(CDiagBuffer::sm_CanDeleteHandler);
        CDiagBuffer::sm_CanDeleteHandler = false;
    }
    return CDiagBuffer::sm_Handler;
}


extern CDiagBuffer& GetDiagBuffer(void)
{
    return CDiagContextThreadData::GetThreadData().GetDiagBuffer();
}


string CDiagHandler::GetLogName(void)
{
    string name = typeid(*this).name();
    return name.empty() ? kLogName_Unknown
        : string(kLogName_Unknown) + "(" + name + ")";
}


CStreamDiagHandler_Base::CStreamDiagHandler_Base(void)
    : m_LogName(kLogName_Stream)
{
}


string CStreamDiagHandler_Base::GetLogName(void)
{
    return m_LogName;
}


void CStreamDiagHandler_Base::Reopen(bool /* truncate */)
{
    return;
}


CStreamDiagHandler::CStreamDiagHandler(CNcbiOstream* os,
                                       bool          quick_flush,
                                       const string& stream_name)
    : m_Stream(os),
      m_QuickFlush(quick_flush)
{
    if ( !stream_name.empty() ) {
        SetLogName(stream_name);
    }
}


void CStreamDiagHandler::Post(const SDiagMessage& mess)
{
    if ( !m_Stream ) {
        return;
    }
    if ( IsSetDiagPostFlag(eDPF_AtomicWrite, mess.m_Flags) ) {
        CNcbiOstrstream str_os;
        str_os << mess;
        m_Stream->write(str_os.str(), str_os.pcount());
        str_os.rdbuf()->freeze(false);
    }
    else {
        *m_Stream << mess;
    }
    if (m_QuickFlush) {
        *m_Stream << NcbiFlush;
    }
}


// CFileDiagHandler

CFileHandleDiagHandler::CFileHandleDiagHandler(const string& fname)
    : m_Handle(-1),
      m_LastReopen(new CTime(CTime::eCurrent))
{
    SetLogName(fname);
    Reopen(CDiagContext::GetLogTruncate());
}


CFileHandleDiagHandler::~CFileHandleDiagHandler(void)
{
    delete m_LastReopen;
    if (m_Handle >= 0) {
        close(m_Handle);
    }
}


void CFileHandleDiagHandler::Reopen(bool truncate)
{
    if (m_Handle >= 0) {
        close(m_Handle);
    }
    int mode = O_WRONLY | O_APPEND | O_CREAT;
    if ( truncate ) {
        mode |= O_TRUNC;
    }

    mode_t perm = CDirEntry::MakeModeT(
        CDirEntry::fRead | CDirEntry::fWrite,
        CDirEntry::fRead | CDirEntry::fWrite,
        CDirEntry::fRead | CDirEntry::fWrite,
        0);
    m_Handle = open(CFile::ConvertToOSPath(GetLogName()).c_str(),
                    mode, perm);
    m_LastReopen->SetCurrent();
    if (m_Handle == -1) {
        string msg;
        switch ( errno ) {
        case EACCES:
            msg = "access denied";
            break;
        case EEXIST:
            msg = "file already exists";
            break;
        case EINVAL:
            msg = "invalid open mode";
            break;
        case EMFILE:
            msg = "too many open files";
            break;
        case ENOENT:
            msg = "invalid file or directoty name";
            break;
        }
        if ( !m_Messages.get() ) {
            m_Messages.reset(new TMessages);
        }
        // ERR_POST(Info << "Failed to reopen log: " << msg);
        return;
    }
    // Flush the collected messages, if any, once the handle if available
    if ( m_Messages.get() ) {
        ITERATE(TMessages, it, *m_Messages) {
            CNcbiOstrstream str_os;
            str_os << *it;
            write(m_Handle, str_os.str(), str_os.pcount());
            str_os.rdbuf()->freeze(false);
        }
        m_Messages.reset();
    }
}


const int kLogReopenDelay = 60; // Reopen log every 60 seconds

void CFileHandleDiagHandler::Post(const SDiagMessage& mess)
{
    // Period is longer than for CFileDiagHandler to prevent double-reopening
    if (mess.GetTime().DiffSecond(*m_LastReopen) >= kLogReopenDelay + 5) {
        Reopen();
    }

    // If the handle is not available, collect the messages until they
    // can be written.
    if ( m_Messages.get() ) {
        // Limit number of stored messages to 1000
        if ( m_Messages->size() < 1000 ) {
            m_Messages->push_back(mess);
        }
        return;
    }

    CNcbiOstrstream str_os;
    str_os << mess;
    write(m_Handle, str_os.str(), str_os.pcount());
    str_os.rdbuf()->freeze(false);
}


// CFileDiagHandler

static bool s_SplitLogFile = false;

extern void SetSplitLogFile(bool value)
{
    s_SplitLogFile = value;
}


extern bool GetSplitLogFile(void)
{
    return s_SplitLogFile;
}


bool s_IsSpecialLogName(const string& name)
{
    return name.empty()
        ||  name == "-"
        ||  name == "/dev/null";
}


CFileDiagHandler::CFileDiagHandler(void)
    : m_Err(0),
      m_Log(0),
      m_Trace(0),
      m_LastReopen(new CTime(CTime::eCurrent))
{
    SetLogFile("-", eDiagFile_All, true);
}


CFileDiagHandler::~CFileDiagHandler(void)
{
    delete m_LastReopen;
}


bool s_CanOpenLogFile(const string& file_name)
{
    CDirEntry entry(CDirEntry::CreateAbsolutePath(file_name));
    if ( !entry.Exists() ) {
        // Use directory instead, must be writable
        string dir = entry.GetDir();
        entry = CDirEntry(dir.empty() ? "." : dir);
        if ( !entry.Exists() ) {
            return false;
        }
    }
    // Need at least 20K of free space to write logs
    try {
        if (CFileUtil::GetFreeDiskSpace(entry.GetDir()) < 1024*20) {
            return false;
        }
    }
    catch (CException) {
    }
    CDirEntry::TMode mode = 0;
    return entry.GetMode(&mode)  &&  (mode & CDirEntry::fWrite) != 0;
}


CStreamDiagHandler_Base* s_CreateHandler(const string& fname, bool& failed)
{
    if ( fname.empty()  ||  fname == "/dev/null") {
        return 0;
    }
    if (fname == "-") {
        return new CStreamDiagHandler(&NcbiCerr, true, kLogName_Stderr);
    }
    CFileHandleDiagHandler* fh = new CFileHandleDiagHandler(fname);
    if ( !fh->Valid() ) {
        failed = true;
        ERR_POST(Info << "Failed to open log file: " << fname);
        return new CStreamDiagHandler(&NcbiCerr, true, kLogName_Stderr);
    }
    return fh;
}


bool CFileDiagHandler::SetLogFile(const string& file_name,
                                  EDiagFileType file_type,
                                  bool          /*quick_flush*/)
{
    bool special = s_IsSpecialLogName(file_name);
    bool failed = false;
    switch ( file_type ) {
    case eDiagFile_All:
        {
            // Remove known extension if any
            string adj_name = file_name;
            if ( !special ) {
                CDirEntry entry(file_name);
                string ext = entry.GetExt();
                adj_name = file_name;
                if (ext == ".log"  ||  ext == ".err"  ||  ext == ".trace") {
                    adj_name = entry.GetDir() + entry.GetBase();
                }
            }
            string err_name = special ? adj_name : adj_name + ".err";
            string log_name = special ? adj_name : adj_name + ".log";
            string trace_name = special ? adj_name : adj_name + ".trace";

            if (!special  &&
                (!s_CanOpenLogFile(err_name)  ||
                !s_CanOpenLogFile(log_name)  ||
                !s_CanOpenLogFile(trace_name))) {
                return false;
            }

            m_Err.reset(s_CreateHandler(err_name, failed));
            m_Log.reset(s_CreateHandler(log_name, failed));
            m_Trace.reset(s_CreateHandler(trace_name, failed));
            m_LastReopen->SetCurrent();
            break;
        }
    case eDiagFile_Err:
        if ( !special  &&  !s_CanOpenLogFile(file_name) ) {
            return false;
        }
        m_Err.reset(s_CreateHandler(file_name, failed));
        break;
    case eDiagFile_Log:
        if ( !special  &&  !s_CanOpenLogFile(file_name) ) {
            return false;
        }
        m_Log.reset(s_CreateHandler(file_name, failed));
        break;
    case eDiagFile_Trace:
        if ( !special  &&  !s_CanOpenLogFile(file_name) ) {
            return false;
        }
        m_Trace.reset(s_CreateHandler(file_name, failed));
        break;
    }
    if (file_name == "") {
        SetLogName(kLogName_None);
    }
    else if (file_name == "-") {
        SetLogName(kLogName_Stderr);
    }
    else {
        SetLogName(file_name);
    }
    return !failed;
}


string CFileDiagHandler::GetLogFile(EDiagFileType file_type) const
{
    switch ( file_type ) {
    case eDiagFile_Err:
        return m_Err->GetLogName();
    case eDiagFile_Log:
        return m_Log->GetLogName();
    case eDiagFile_Trace:
        return m_Trace->GetLogName();
    case eDiagFile_All:
        break;  // kEmptyStr
    }
    return kEmptyStr;
}


CNcbiOstream* CFileDiagHandler::GetLogStream(EDiagFileType file_type)
{
    CStreamDiagHandler_Base* handler = 0;
    switch ( file_type ) {
    case eDiagFile_Err:
        handler = m_Err.get();
    case eDiagFile_Log:
        handler = m_Log.get();
    case eDiagFile_Trace:
        handler = m_Trace.get();
    case eDiagFile_All:
        return 0;
    }
    return handler ? handler->GetStream() : 0;
}


void CFileDiagHandler::Reopen(bool truncate)
{
    if ( m_Err.get() ) {
        m_Err->Reopen(truncate);
    }
    if ( m_Log.get() ) {
        m_Log->Reopen(truncate);
    }
    if ( m_Trace.get() ) {
        m_Trace->Reopen(truncate);
    }
    m_LastReopen->SetCurrent();
}


void CFileDiagHandler::Post(const SDiagMessage& mess)
{
    // Check time and re-open the streams
    if (mess.GetTime().DiffSecond(*m_LastReopen) >= kLogReopenDelay) {
        Reopen();
    }

    // Output the message
    CStreamDiagHandler_Base* handler = 0;
    if ( IsSetDiagPostFlag(eDPF_AppLog, mess.m_Flags) ) {
        handler = m_Log.get();
    }
    else {
        switch ( mess.m_Severity ) {
        case eDiag_Info:
        case eDiag_Trace:
            handler = m_Trace.get();
            break;
        default:
            handler = m_Err.get();
        }
    }
    if ( !handler ) {
        return;
    }
    handler->Post(mess);
}


extern bool SetLogFile(const string& file_name,
                       EDiagFileType file_type,
                       bool quick_flush)
{
    // Check if a non-existing dir is specified
    if ( !s_IsSpecialLogName(file_name) ) {
        string dir = CFile(file_name).GetDir();
        if ( !dir.empty()  &&  !CDir(dir).Exists() ) {
            return false;
        }
    }

    bool no_split = !s_SplitLogFile;
    if ( no_split ) {
        if (file_type != eDiagFile_All) {
            ERR_POST(Info <<
                "Failed to set log file for the selected event type: "
                "split log is disabled");
            return false;
        }
        // Check special filenames
        if ( file_name.empty()  ||  file_name == "/dev/null" ) {
            // no output
            SetDiagStream(0, quick_flush, 0, 0, kLogName_None);
        }
        else if (file_name == "-") {
            // output to stderr
            SetDiagStream(&NcbiCerr, quick_flush, 0, 0, kLogName_Stderr);
        }
        else {
            if ( !s_CanOpenLogFile(file_name) ) {
                return false;
            }
            // output to file
            CFileHandleDiagHandler* fhandler =
                new CFileHandleDiagHandler(file_name);
            if ( !fhandler->Valid() ) {
                ERR_POST(Info << "Failed to initialize log: " << file_name);
                return false;
            }
            SetDiagHandler(fhandler);
        }
    }
    else {
        CFileDiagHandler* handler =
            dynamic_cast<CFileDiagHandler*>(GetDiagHandler());
        if ( !handler ) {
            // Install new handler
            handler = new CFileDiagHandler();
            if ( handler->SetLogFile(file_name, file_type, quick_flush) ) {
                SetDiagHandler(handler);
                return true;
            }
            else {
                return false;
            }
        }
        // Update the existing handler
        return handler->SetLogFile(file_name, file_type, quick_flush);
    }
    return true;
}


extern string GetLogFile(EDiagFileType file_type)
{
    CFileDiagHandler* handler =
        dynamic_cast<CFileDiagHandler*>(GetDiagHandler());
    return handler ? handler->GetLogFile(file_type) : kEmptyStr;
}


extern bool IsDiagStream(const CNcbiOstream* os)
{
    CStreamDiagHandler_Base* sdh
        = dynamic_cast<CStreamDiagHandler_Base*>(CDiagBuffer::sm_Handler);
    return (sdh  &&  sdh->GetStream() == os);
}


extern void SetDiagErrCodeInfo(CDiagErrCodeInfo* info, bool can_delete)
{
    CMutexGuard LOCK(s_DiagMutex);
    if ( CDiagBuffer::sm_CanDeleteErrCodeInfo  &&
         CDiagBuffer::sm_ErrCodeInfo )
        delete CDiagBuffer::sm_ErrCodeInfo;
    CDiagBuffer::sm_ErrCodeInfo = info;
    CDiagBuffer::sm_CanDeleteErrCodeInfo = can_delete;
}

extern bool IsSetDiagErrCodeInfo(void)
{
    return (CDiagBuffer::sm_ErrCodeInfo != 0);
}

extern CDiagErrCodeInfo* GetDiagErrCodeInfo(bool take_ownership)
{
    CMutexGuard LOCK(s_DiagMutex);
    if (take_ownership) {
        _ASSERT(CDiagBuffer::sm_CanDeleteErrCodeInfo);
        CDiagBuffer::sm_CanDeleteErrCodeInfo = false;
    }
    return CDiagBuffer::sm_ErrCodeInfo;
}


extern void SetDiagFilter(EDiagFilter what, const char* filter_str)
{
    CMutexGuard LOCK(s_DiagMutex);
    if (what == eDiagFilter_Trace  ||  what == eDiagFilter_All) 
        s_TraceFilter.Fill(filter_str);

    if (what == eDiagFilter_Post  ||  what == eDiagFilter_All) 
        s_PostFilter.Fill(filter_str);
}


static
bool s_CheckDiagFilter(const CException& ex, EDiagSev sev, const char* file)
{
    if (sev == eDiag_Fatal) 
        return true;

    CMutexGuard LOCK(s_DiagMutex);

    // check for trace filter
    if (sev == eDiag_Trace) {
        EDiagFilterAction action = s_TraceFilter.CheckFile(file);
        if(action == eDiagFilter_None)
            return s_TraceFilter.Check(ex, sev) == eDiagFilter_Accept;
        return action == eDiagFilter_Accept;
    }

    // check for post filter
    EDiagFilterAction action = s_PostFilter.CheckFile(file);
    if (action == eDiagFilter_None) {
        action = s_PostFilter.Check(ex, sev);
    }

    return (action == eDiagFilter_Accept);
}



///////////////////////////////////////////////////////
//  CNcbiDiag::

CNcbiDiag::CNcbiDiag(EDiagSev sev, TDiagPostFlags post_flags)
    : m_Severity(sev), 
      m_ErrCode(0), 
      m_ErrSubCode(0),
      m_Buffer(GetDiagBuffer()), 
      m_PostFlags(post_flags),
      m_CheckFilters(true),
      m_Line(0),
      m_ValChngFlags(0)
{
}


CNcbiDiag::CNcbiDiag(const CDiagCompileInfo &info,
                     EDiagSev sev, TDiagPostFlags post_flags)
    : m_Severity(sev), 
      m_ErrCode(0), 
      m_ErrSubCode(0),
      m_Buffer(GetDiagBuffer()), 
      m_PostFlags(post_flags),
      m_CheckFilters(true),
      m_CompileInfo(info),
      m_Line(info.GetLine()),
      m_ValChngFlags(0)
{
    SetFile(   info.GetFile()   );
    SetModule( info.GetModule() );
}

CNcbiDiag::~CNcbiDiag(void) 
{
    m_Buffer.Detach(this);
}

const CNcbiDiag& CNcbiDiag::SetFile(const char* file) const
{
    m_File = file;
    m_ValChngFlags |= fFileIsChanged;
    return *this;
}


const CNcbiDiag& CNcbiDiag::SetModule(const char* module) const
{
    m_Module = module;
    m_ValChngFlags |= fModuleIsChanged;
    return *this;
}


const CNcbiDiag& CNcbiDiag::SetClass(const char* nclass ) const
{
    m_Class = nclass;
    m_ValChngFlags |= fClassIsChanged;
    return *this;
}


const CNcbiDiag& CNcbiDiag::SetFunction(const char* function) const
{
    m_Function = function;
    m_ValChngFlags |= fFunctionIsChanged;
    return *this;
}


bool CNcbiDiag::CheckFilters(void) const
{
    if ( !m_CheckFilters ) {
        m_CheckFilters = true;
        return true;
    }

    EDiagSev current_sev = GetSeverity();
    if (current_sev == eDiag_Fatal) 
        return true;

    CMutexGuard LOCK(s_DiagMutex);
    if (GetSeverity() == eDiag_Trace) {
        // check for trace filter
        return 
         s_TraceFilter.Check(*this, this->GetSeverity()) != eDiagFilter_Reject;
    }
    
    // check for post filter and severity
    return 
        s_PostFilter.Check(*this, this->GetSeverity()) != eDiagFilter_Reject;
}


// Formatted output of stack trace
void s_FormatStackTrace(CNcbiOstream& os, const CStackTrace& trace)
{
    string old_prefix = trace.GetPrefix();
    trace.SetPrefix("      ");
    os << "\n     Stack trace:\n" << trace;
    trace.SetPrefix(old_prefix);
}


const CNcbiDiag& CNcbiDiag::Put(const CStackTrace*,
                                const CStackTrace& stacktrace) const
{
    if ( !stacktrace.Empty() ) {
        stacktrace.SetPrefix("      ");
        ostrstream os;
        s_FormatStackTrace(os, stacktrace);
        *this << (string) CNcbiOstrstreamToString(os);
    }
    return *this;
}


const CNcbiDiag& CNcbiDiag::x_Put(const CException& ex) const
{
    if ( !s_CheckDiagFilter(ex, GetSeverity(), GetFile()) ) {
        m_Buffer.Reset(*this);
        return *this;
    }

    m_CheckFilters = false;

    *this << "\nNCBI C++ Exception:\n";

    const CException* pex;
    stack<const CException*> pile;
    // invert the order
    for (pex = &ex; pex; pex = pex->GetPredecessor()) {
        pile.push(pex);
    }
    for (; !pile.empty(); pile.pop()) {
        pex = pile.top();
        string text(pex->GetMsg());
        {
            ostrstream os;
            pex->ReportExtra(os);
            if (os.pcount() != 0) {
                text += " (";
                text += (string) CNcbiOstrstreamToString(os);
                text += ')';
            }
        }
        const CStackTrace* stacktrace = pex->GetStackTrace();
        if ( stacktrace ) {
            ostrstream os;
            s_FormatStackTrace(os, *stacktrace);
            text += (string) CNcbiOstrstreamToString(os);
        }
        string err_type(pex->GetType());
        err_type += "::";
        err_type += pex->GetErrCodeString();
        SDiagMessage diagmsg(pex->GetSeverity(),
                             text.c_str(), 
                             text.size(),
                             pex->GetFile().c_str(),
                             pex->GetLine(),
                             GetPostFlags(),
                             NULL,
                             pex->GetErrCode(),
                             0,
                             err_type.c_str(),
                             pex->GetModule().c_str(),
                             pex->GetClass().c_str(),
                             pex->GetFunction().c_str(),
                             CDiagContext::GetPID(),
                             m_Buffer.m_TID,
                             CDiagBuffer::GetProcessPostNumber(
                             CDiagBuffer::ePostNumber_Increment),
                             ++m_Buffer.m_ThreadPostCount,
                             GetDiagRequestId());
        string report;
        diagmsg.Write(report);
        *this << "    "; // indentation
        *this << report;
    }
    

    return *this;
}


bool CNcbiDiag::StrToSeverityLevel(const char* str_sev, EDiagSev& sev)
{
    if (!str_sev || !*str_sev) {
        return false;
    } 
    // Digital value
    int nsev = NStr::StringToNumeric(str_sev);

    if (nsev > eDiagSevMax) {
        nsev = eDiagSevMax;
    } else if ( nsev == -1 ) {
        // String value
        for (int s = eDiagSevMin; s <= eDiagSevMax; s++) {
            if (NStr::CompareNocase(CNcbiDiag::SeverityName(EDiagSev(s)),
                                    str_sev) == 0) {
                nsev = s;
                break;
            }
        }
    }
    sev = EDiagSev(nsev);
    // Unknown value
    return sev >= eDiagSevMin  &&  sev <= eDiagSevMax;
}

void CNcbiDiag::DiagFatal(const CDiagCompileInfo& info,
                          const char* message)
{
    CNcbiDiag(info, NCBI_NS_NCBI::eDiag_Fatal) << message << Endm;
}

void CNcbiDiag::DiagTrouble(const CDiagCompileInfo& info,
                            const char* message)
{
    DiagFatal(info, message);
}

void CNcbiDiag::DiagAssert(const CDiagCompileInfo& info,
                           const char* expression,
                           const char* message)
{
    CNcbiDiag(info, NCBI_NS_NCBI::eDiag_Fatal, eDPF_Trace) <<
        "Assertion failed: (" <<
        (expression ? expression : "") << ") " <<
        (message ? message : "") << Endm;
}

void CNcbiDiag::DiagValidate(const CDiagCompileInfo& info,
                             const char* _DEBUG_ARG(expression),
                             const char* message)
{
#ifdef _DEBUG
    if ( xncbi_GetValidateAction() != eValidate_Throw ) {
        DiagAssert(info, expression, message);
    }
#endif
    throw CCoreException(info, 0, CCoreException::eCore, message);
}

///////////////////////////////////////////////////////
//  CDiagRestorer::

CDiagRestorer::CDiagRestorer(void)
{
    CMutexGuard LOCK(s_DiagMutex);
    const CDiagBuffer& buf  = GetDiagBuffer();
    m_PostPrefix            = buf.m_PostPrefix;
    m_PrefixList            = buf.m_PrefixList;
    m_PostFlags             = buf.sx_GetPostFlags();
    m_PostSeverity          = buf.sm_PostSeverity;
    m_PostSeverityChange    = buf.sm_PostSeverityChange;
    m_IgnoreToDie           = buf.sm_IgnoreToDie;
    m_DieSeverity           = buf.sm_DieSeverity;
    m_TraceDefault          = buf.sm_TraceDefault;
    m_TraceEnabled          = buf.sm_TraceEnabled;
    m_Handler               = buf.sm_Handler;
    m_CanDeleteHandler      = buf.sm_CanDeleteHandler;
    m_ErrCodeInfo           = buf.sm_ErrCodeInfo;
    m_CanDeleteErrCodeInfo  = buf.sm_CanDeleteErrCodeInfo;
    // avoid premature cleanup
    buf.sm_CanDeleteHandler     = false;
    buf.sm_CanDeleteErrCodeInfo = false;
}

CDiagRestorer::~CDiagRestorer(void)
{
    {{
        CMutexGuard LOCK(s_DiagMutex);
        CDiagBuffer& buf          = GetDiagBuffer();
        buf.m_PostPrefix          = m_PostPrefix;
        buf.m_PrefixList          = m_PrefixList;
        buf.sx_GetPostFlags()     = m_PostFlags;
        buf.sm_PostSeverity       = m_PostSeverity;
        buf.sm_PostSeverityChange = m_PostSeverityChange;
        buf.sm_IgnoreToDie        = m_IgnoreToDie;
        buf.sm_DieSeverity        = m_DieSeverity;
        buf.sm_TraceDefault       = m_TraceDefault;
        buf.sm_TraceEnabled       = m_TraceEnabled;
    }}
    SetDiagHandler(m_Handler, m_CanDeleteHandler);
    SetDiagErrCodeInfo(m_ErrCodeInfo, m_CanDeleteErrCodeInfo);
}


//////////////////////////////////////////////////////
//  internal diag. handler classes for compatibility:

class CCompatDiagHandler : public CDiagHandler
{
public:
    CCompatDiagHandler(FDiagHandler func, void* data, FDiagCleanup cleanup)
        : m_Func(func), m_Data(data), m_Cleanup(cleanup)
        { }
    ~CCompatDiagHandler(void)
        {
            if (m_Cleanup) {
                m_Cleanup(m_Data);
            }
        }
    virtual void Post(const SDiagMessage& mess) { m_Func(mess); }

private:
    FDiagHandler m_Func;
    void*        m_Data;
    FDiagCleanup m_Cleanup;
};


extern void SetDiagHandler(FDiagHandler func,
                           void*        data,
                           FDiagCleanup cleanup)
{
    SetDiagHandler(new CCompatDiagHandler(func, data, cleanup));
}


class CCompatStreamDiagHandler : public CStreamDiagHandler
{
public:
    CCompatStreamDiagHandler(CNcbiOstream* os,
                             bool          quick_flush  = true,
                             FDiagCleanup  cleanup      = 0,
                             void*         cleanup_data = 0,
                             const string& stream_name = kEmptyStr)
        : CStreamDiagHandler(os, quick_flush, stream_name),
          m_Cleanup(cleanup), m_CleanupData(cleanup_data)
        {
        }

    ~CCompatStreamDiagHandler(void)
        {
            if (m_Cleanup) {
                m_Cleanup(m_CleanupData);
            }
        }

private:
    FDiagCleanup m_Cleanup;
    void*        m_CleanupData;
};


extern void SetDiagStream(CNcbiOstream* os,
                          bool          quick_flush,
                          FDiagCleanup  cleanup,
                          void*         cleanup_data,
                          const string& stream_name)
{
    SetDiagHandler(new CCompatStreamDiagHandler(os, quick_flush,
                                                cleanup, cleanup_data,
                                                stream_name));
}


extern CNcbiOstream* GetDiagStream(void)
{
    CDiagHandler* diagh = GetDiagHandler();
    if ( !diagh ) {
        return 0;
    }
    CStreamDiagHandler_Base* sh =
        dynamic_cast<CStreamDiagHandler_Base*>(diagh);
    // This can also be CFileDiagHandler, check it later
    if ( sh  &&  sh->GetStream() ) {
        return sh->GetStream();
    }
    CFileDiagHandler* fh =
        dynamic_cast<CFileDiagHandler*>(diagh);
    if ( fh ) {
        return fh->GetLogStream(eDiagFile_Err);
    }
    return 0;
}


extern void SetDoubleDiagHandler(void)
{
    ERR_POST(Error << "SetDoubleDiagHandler() is not implemented");
}


//////////////////////////////////////////////////////
//  abort handler


static FAbortHandler s_UserAbortHandler = 0;

extern void SetAbortHandler(FAbortHandler func)
{
    s_UserAbortHandler = func;
}


extern void Abort(void)
{
    // If defined user abort handler then call it 
    if ( s_UserAbortHandler )
        s_UserAbortHandler();
    
    // If don't defined handler or application doesn't still terminated

    // Check environment variable for silent exit
    const char* value = ::getenv("DIAG_SILENT_ABORT");
    if (value  &&  (*value == 'Y'  ||  *value == 'y'  ||  *value == '1')) {
        ::exit(255);
    }
    else if (value  &&  (*value == 'N'  ||  *value == 'n' || *value == '0')) {
        ::abort();
    }
    else
#define NCBI_TOTALVIEW_ABORT_WORKAROUND 1
#if defined(NCBI_TOTALVIEW_ABORT_WORKAROUND)
        // The condition in the following if statement is always 'true'.
        // It's a workaround for TotalView 6.5 (beta) to properly display
        // stacktrace at this point.
        if ( !(value && *value == 'Y') )
#endif
            {
#if defined(_DEBUG)
                ::abort();
#else
                ::exit(255);
#endif
            }
}


///////////////////////////////////////////////////////
//  CDiagErrCodeInfo::
//

SDiagErrCodeDescription::SDiagErrCodeDescription(void)
        : m_Message(kEmptyStr),
          m_Explanation(kEmptyStr),
          m_Severity(-1)
{
    return;
}


bool CDiagErrCodeInfo::Read(const string& file_name)
{
    CNcbiIfstream is(file_name.c_str());
    if ( !is.good() ) {
        return false;
    }
    return Read(is);
}


// Parse string for CDiagErrCodeInfo::Read()

bool s_ParseErrCodeInfoStr(string&          str,
                           const SIZE_TYPE  line,
                           int&             x_code,
                           int&             x_severity,
                           string&          x_message,
                           bool&            x_ready)
{
    list<string> tokens;    // List with line tokens

    try {
        // Get message text
        SIZE_TYPE pos = str.find_first_of(':');
        if (pos == NPOS) {
            x_message = kEmptyStr;
        } else {
            x_message = NStr::TruncateSpaces(str.substr(pos+1));
            str.erase(pos);
        }

        // Split string on parts
        NStr::Split(str, ",", tokens);
        if (tokens.size() < 2) {
            ERR_POST("Error message file parsing: Incorrect file format "
                     ", line " + NStr::UInt8ToString(line));
            return false;
        }
        // Mnemonic name (skip)
        tokens.pop_front();

        // Error code
        string token = NStr::TruncateSpaces(tokens.front());
        tokens.pop_front();
        x_code = NStr::StringToInt(token);

        // Severity
        if ( !tokens.empty() ) { 
            token = NStr::TruncateSpaces(tokens.front());
            EDiagSev sev;
            if (CNcbiDiag::StrToSeverityLevel(token.c_str(), sev)) {
                x_severity = sev;
            } else {
                ERR_POST(Warning << "Error message file parsing: "
                         "Incorrect severity level in the verbose "
                         "message file, line " + NStr::UInt8ToString(line));
            }
        } else {
            x_severity = -1;
        }
    }
    catch (CException& e) {
        ERR_POST(Warning << "Error message file parsing: " << e.GetMsg() <<
                 ", line " + NStr::UInt8ToString(line));
        return false;
    }
    x_ready = true;
    return true;
}

  
bool CDiagErrCodeInfo::Read(CNcbiIstream& is)
{
    string       str;                      // The line being parsed
    SIZE_TYPE    line;                     // # of the line being parsed
    bool         err_ready       = false;  // Error data ready flag 
    int          err_code        = 0;      // First level error code
    int          err_subcode     = 0;      // Second level error code
    string       err_message;              // Short message
    string       err_text;                 // Error explanation
    int          err_severity    = -1;     // Use default severity if  
                                           // has not specified
    int          err_subseverity = -1;     // Use parents severity if  
                                           // has not specified

    for (line = 1;  NcbiGetlineEOL(is, str);  line++) {
        
        // This is a comment or empty line
        if (!str.length()  ||  NStr::StartsWith(str,"#")) {
            continue;
        }
        // Add error description
        if (err_ready  &&  str[0] == '$') {
            if (err_subseverity == -1)
                err_subseverity = err_severity;
            SetDescription(ErrCode(err_code, err_subcode), 
                SDiagErrCodeDescription(err_message, err_text,
                                        err_subseverity));
            // Clean
            err_subseverity = -1;
            err_text     = kEmptyStr;
            err_ready    = false;
        }

        // Get error code
        if (NStr::StartsWith(str,"$$")) {
            if (!s_ParseErrCodeInfoStr(str, line, err_code, err_severity, 
                                       err_message, err_ready))
                continue;
            err_subcode = 0;
        
        } else if (NStr::StartsWith(str,"$^")) {
        // Get error subcode
            s_ParseErrCodeInfoStr(str, line, err_subcode, err_subseverity,
                                  err_message, err_ready);
      
        } else if (err_ready) {
        // Get line of explanation message
            if (!err_text.empty()) {
                err_text += '\n';
            }
            err_text += str;
        }
    }
    if (err_ready) {
        if (err_subseverity == -1)
            err_subseverity = err_severity;
        SetDescription(ErrCode(err_code, err_subcode), 
            SDiagErrCodeDescription(err_message, err_text, err_subseverity));
    }
    return true;
}


bool CDiagErrCodeInfo::GetDescription(const ErrCode& err_code, 
                                      SDiagErrCodeDescription* description)
    const
{
    // Find entry
    TInfo::const_iterator find_entry = m_Info.find(err_code);
    if (find_entry == m_Info.end()) {
        return false;
    }
    // Get entry value
    const SDiagErrCodeDescription& entry = find_entry->second;
    if (description) {
        *description = entry;
    }
    return true;
}

const char* g_DiagUnknownFunction(void)
{
    return "UNK_FUNCTION";
}


END_NCBI_SCOPE
