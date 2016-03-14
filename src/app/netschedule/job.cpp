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
* Authors:  Victor Joukov
*
* File Description:
*   NetSchedule job
*
*/

#include <ncbi_pch.hpp>

#include <string.h>

#include <connect/services/netschedule_api_expt.hpp>

#include "job.hpp"
#include "ns_queue.hpp"
#include "ns_handler.hpp"
#include "ns_command_arguments.hpp"
#include "ns_affinity.hpp"
#include "ns_group.hpp"
#include "ns_db_dump.hpp"


BEGIN_NCBI_SCOPE


//////////////////////////////////////////////////////////////////////////
// CJobEvent implementation

static string   s_EventAsString[] = {
    "Submit",           // eSubmit
    "BatchSubmit",      // eBatchSubmit
    "Request",          // eRequest
    "Done",             // eDone
    "Return",           // eReturn
    "Fail",             // eFail
    "FinalFail",        // eFinalFail
    "Read",             // eRead
    "ReadFail",         // eReadFail
    "ReadFinalFail",    // eReadFinalFail
    "ReadDone",         // eReadDone
    "ReadRollback",     // eReadRollback
    "Clear",            // eClear
    "Cancel",           // eCancel
    "Timeout",          // eTimeout
    "ReadTimeout",      // eReadTimeout
    "SessionChanged",   // eSessionChanged
    "NSSubmitRollback", // eNSSubmitRollback
    "NSGetRollback",    // eNSGetRollback
    "NSReadRollback",   // eNSReadRollback
    "ReturnNoBlacklist",// eReturnNoBlacklist
    "Reschedule"        // eReschedule
};


string CJobEvent::EventToString(EJobEvent  event)
{
    if (event < eSubmit || event > eReschedule)
        return "UNKNOWN";

    return s_EventAsString[ event ];
}


CJobEvent::CJobEvent() :
    m_Dirty(false),
    m_Status(CNetScheduleAPI::eJobNotFound),
    m_Event(eUnknown),
    m_Timestamp(0, 0),
    m_NodeAddr(0),
    m_RetCode(0)
{}


CNSPreciseTime
GetJobExpirationTime(const CNSPreciseTime &     last_touch,
                     TJobStatus                 status,
                     const CNSPreciseTime &     job_submit_time,
                     const CNSPreciseTime &     job_timeout,
                     const CNSPreciseTime &     job_run_timeout,
                     const CNSPreciseTime &     job_read_timeout,
                     const CNSPreciseTime &     queue_timeout,
                     const CNSPreciseTime &     queue_run_timeout,
                     const CNSPreciseTime &     queue_read_timeout,
                     const CNSPreciseTime &     queue_pending_timeout,
                     const CNSPreciseTime &     event_time)
{
    CNSPreciseTime  last_update = event_time;
    if (last_update == kTimeZero)
        last_update = last_touch;

    if (status == CNetScheduleAPI::eRunning) {
        if (job_run_timeout != kTimeZero)
            return last_update + job_run_timeout;
        return last_update + queue_run_timeout;
    }

    if (status == CNetScheduleAPI::eReading) {
        if (job_read_timeout != kTimeZero)
            return last_update + job_read_timeout;
        return last_update + queue_read_timeout;
    }

    if (status == CNetScheduleAPI::ePending) {
        CNSPreciseTime  regular_expiration = last_update +
                                             queue_timeout;
        if (job_timeout != kTimeZero)
            regular_expiration = last_update + job_timeout;
        CNSPreciseTime  pending_expiration = job_submit_time +
                                             queue_pending_timeout;

        if (regular_expiration < pending_expiration)
            return regular_expiration;
        return pending_expiration;
    }

    if (job_timeout != kTimeZero)
        return last_update + job_timeout;
    return last_update + queue_timeout;
}




//////////////////////////////////////////////////////////////////////////
// CJob implementation

CJob::CJob() :
    m_New(true), m_Deleted(false), m_Dirty(0),
    m_Id(0),
    m_Passport(0),
    m_Status(CNetScheduleAPI::ePending),
    m_Timeout(),
    m_RunTimeout(),
    m_ReadTimeout(),
    m_SubmNotifPort(0),
    m_SubmNotifTimeout(),
    m_ListenerNotifAddress(0),
    m_ListenerNotifPort(0),
    m_ListenerNotifAbsTime(),
    m_RunCount(0),
    m_ReadCount(0),
    m_AffinityId(0),
    m_Mask(0),
    m_GroupId(0),
    m_LastTouch()
{}


CJob::CJob(const SNSCommandArguments &  request) :
    m_New(true), m_Deleted(false), m_Dirty(fJobPart),
    m_Id(0),
    m_Passport(0),
    m_Status(CNetScheduleAPI::ePending),
    m_Timeout(),
    m_RunTimeout(),
    m_ReadTimeout(),
    m_SubmNotifPort(request.port),
    m_SubmNotifTimeout(request.timeout, 0),
    m_ListenerNotifAddress(0),
    m_ListenerNotifPort(0),
    m_ListenerNotifAbsTime(),
    m_RunCount(0),
    m_ReadCount(0),
    m_ProgressMsg(""),
    m_AffinityId(0),
    m_Mask(request.job_mask),
    m_GroupId(0),
    m_LastTouch(),
    m_Output("")
{
    SetClientIP(request.ip);
    SetClientSID(request.sid);
    SetNCBIPHID(request.ncbi_phid);
    SetInput(request.input);
}


string CJob::GetErrorMsg() const
{
    if (m_Events.empty())
        return "";
    return GetLastEvent()->GetErrorMsg();
}


int    CJob::GetRetCode() const
{
    if (m_Events.empty())
        return ~0;
    return GetLastEvent()->GetRetCode();
}


void CJob::SetInput(const string &  input)
{
    bool    was_overflow = m_Input.size() > kNetScheduleSplitSize;
    bool    is_overflow = input.size() > kNetScheduleSplitSize;

    if (is_overflow) {
        m_Dirty |= fJobInfoPart;
        if (!was_overflow)
            m_Dirty |= fJobPart;
    } else {
        m_Dirty |= fJobPart;
        if (was_overflow)
            m_Dirty |= fJobInfoPart;
    }
    m_Input = input;
}


void CJob::SetOutput(const string &  output)
{
    if (m_Output.empty() && output.empty())
        return;     // It might be the most common case for failed jobs

    bool    was_overflow = m_Output.size() > kNetScheduleSplitSize;
    bool    is_overflow = output.size() > kNetScheduleSplitSize;

    if (is_overflow) {
        m_Dirty |= fJobInfoPart;
        if (!was_overflow)
            m_Dirty |= fJobPart;
    } else {
        m_Dirty |= fJobPart;
        if (was_overflow)
            m_Dirty |= fJobInfoPart;
    }
    m_Output = output;
}


CJobEvent &  CJob::AppendEvent()
{
    m_Events.push_back(CJobEvent());
    m_Dirty |= fEventsPart;
    return m_Events[m_Events.size()-1];
}


const CJobEvent *  CJob::GetLastEvent() const
{
    // The first event is attached when a job is submitted, so
    // there is no way to have the events empty
    return &(m_Events[m_Events.size()-1]);
}


CJobEvent *  CJob::GetLastEvent()
{
    // The first event is attached when a job is submitted, so
    // there is no way to have the events empty
    m_Dirty |= fEventsPart;
    return &(m_Events[m_Events.size()-1]);
}


CJob::EAuthTokenCompareResult
CJob::CompareAuthToken(const string &  auth_token) const
{
    vector<string>      parts;

    NStr::Split(auth_token, "_", parts, NStr::fSplit_NoMergeDelims);
    if (parts.size() < 2)
        return eInvalidTokenFormat;

    try {
        if (NStr::StringToUInt(parts[0]) != m_Passport)
            return eNoMatch;
        if (NStr::StringToSizet(parts[1]) != m_Events.size())
            return ePassportOnlyMatch;
    } catch (...) {
        // Cannot convert the value
        return eInvalidTokenFormat;
    }
    return eCompleteMatch;
}


void CJob::Delete()
{
    m_Deleted = true;
    m_Dirty = 0;
}


CJob::EJobFetchResult CJob::x_Fetch(CQueue* queue)
{
    SJobDB &        job_db      = queue->m_QueueDbBlock->job_db;
    SJobInfoDB &    job_info_db = queue->m_QueueDbBlock->job_info_db;
    SEventsDB &     events_db   = queue->m_QueueDbBlock->events_db;

    m_Id            = job_db.id;

    m_Passport      = job_db.passport;
    m_Status        = TJobStatus(int(job_db.status));
    m_Timeout       = CNSPreciseTime(job_db.timeout);
    m_RunTimeout    = CNSPreciseTime(job_db.run_timeout);
    m_ReadTimeout   = CNSPreciseTime(job_db.read_timeout);

    m_SubmNotifPort    = job_db.subm_notif_port;
    m_SubmNotifTimeout = CNSPreciseTime(job_db.subm_notif_timeout);

    m_ListenerNotifAddress = job_db.listener_notif_addr;
    m_ListenerNotifPort    = job_db.listener_notif_port;
    m_ListenerNotifAbsTime = CNSPreciseTime(job_db.listener_notif_abstime);

    m_RunCount      = job_db.run_counter;
    m_ReadCount     = job_db.read_counter;
    m_AffinityId    = job_db.aff_id;
    m_Mask          = job_db.mask;
    m_GroupId       = job_db.group_id;
    m_LastTouch     = CNSPreciseTime(job_db.last_touch);

    m_ClientIP      = job_db.client_ip;
    m_ClientSID     = job_db.client_sid;
    m_NCBIPHID      = job_db.ncbi_phid;

    if (!(char) job_db.input_overflow)
        job_db.input.ToString(m_Input);
    if (!(char) job_db.output_overflow)
        job_db.output.ToString(m_Output);
    job_db.progress_msg.ToString(m_ProgressMsg);

    // JobInfoDB, can be optimized by adding lazy load
    EBDB_ErrCode        res;

    if ((char) job_db.input_overflow ||
        (char) job_db.output_overflow) {
        job_info_db.id = m_Id;
        res = job_info_db.Fetch();

        if (res != eBDB_Ok) {
            if (res != eBDB_NotFound) {
                ERR_POST("Error reading the job input/output DB, job_key " <<
                         queue->MakeJobKey(m_Id));
                return eJF_DBErr;
            }
            ERR_POST("DB inconsistency detected. Long input/output "
                     "expected but no record found in the DB. job_key " <<
                     queue->MakeJobKey(m_Id));
            return eJF_DBErr;
        }

        if ((char) job_db.input_overflow)
            job_info_db.input.ToString(m_Input);
        if ((char) job_db.output_overflow)
            job_info_db.output.ToString(m_Output);
    }

    // EventsDB
    m_Events.clear();
    CBDB_FileCursor &       cur = queue->GetEventsCursor();
    CBDB_CursorGuard        cg(cur);

    cur.SetCondition(CBDB_FileCursor::eEQ);
    cur.From << m_Id;

    for (unsigned n = 0; (res = cur.Fetch()) == eBDB_Ok; ++n) {
        CJobEvent &       event = AppendEvent();

        event.m_Status     = TJobStatus(int(events_db.status));
        event.m_Event      = CJobEvent::EJobEvent(int(events_db.event));
        event.m_Timestamp  = CNSPreciseTime(events_db.timestamp);
        event.m_NodeAddr   = events_db.node_addr;
        event.m_RetCode    = events_db.ret_code;
        events_db.client_node.ToString(event.m_ClientNode);
        events_db.client_session.ToString(event.m_ClientSession);
        events_db.err_msg.ToString(event.m_ErrorMsg);
        event.m_Dirty = false;
    }
    if (res != eBDB_NotFound) {
        ERR_POST("Error reading queue events db, job_key " <<
                 queue->MakeJobKey(m_Id));
        return eJF_DBErr;
    }

    m_New   = false;
    m_Dirty = 0;
    return eJF_Ok;
}


CJob::EJobFetchResult  CJob::Fetch(CQueue *  queue, unsigned  id)
{
    SJobDB &        job_db = queue->m_QueueDbBlock->job_db;

    job_db.id = id;

    EBDB_ErrCode    res = job_db.Fetch();
    if (res != eBDB_Ok) {
        if (res != eBDB_NotFound) {
            ERR_POST("Error reading queue job db, job_key " <<
                     queue->MakeJobKey(id));
            return eJF_DBErr;
        }
        return eJF_NotFound;
    }
    return x_Fetch(queue);
}


bool CJob::Flush(CQueue* queue)
{
    if (m_Deleted) {
        queue->EraseJob(m_Id, m_Status);
        return true;
    }

    if (m_Dirty == 0 && m_New == false)
        return true;

    SJobDB &        job_db      = queue->m_QueueDbBlock->job_db;
    SJobInfoDB &    job_info_db = queue->m_QueueDbBlock->job_info_db;
    SEventsDB &     events_db   = queue->m_QueueDbBlock->events_db;

    bool            flush_job = (m_Dirty & fJobPart) || m_New;
    bool            input_overflow = m_Input.size() > kNetScheduleSplitSize;
    bool            output_overflow = m_Output.size() > kNetScheduleSplitSize;

    // JobDB (QueueDB)
    if (flush_job) {
        job_db.id       = m_Id;
        job_db.passport = m_Passport;
        job_db.status   = int(m_Status);

        job_db.timeout      = (double)m_Timeout;
        job_db.run_timeout  = (double)m_RunTimeout;
        job_db.read_timeout = (double)m_ReadTimeout;

        job_db.subm_notif_port    = m_SubmNotifPort;
        job_db.subm_notif_timeout = (double)m_SubmNotifTimeout;

        job_db.listener_notif_addr    = m_ListenerNotifAddress;
        job_db.listener_notif_port    = m_ListenerNotifPort;
        job_db.listener_notif_abstime = (double)m_ListenerNotifAbsTime;

        job_db.run_counter  = m_RunCount;
        job_db.read_counter = m_ReadCount;
        job_db.aff_id       = m_AffinityId;
        job_db.mask         = m_Mask;
        job_db.group_id     = m_GroupId;
        job_db.last_touch   = (double)m_LastTouch;

        job_db.client_ip    = m_ClientIP;
        job_db.client_sid   = m_ClientSID;
        job_db.ncbi_phid    = m_NCBIPHID;

        if (!input_overflow) {
            job_db.input_overflow = 0;
            job_db.input = m_Input;
        } else {
            job_db.input_overflow = 1;
            job_db.input = "";
        }
        if (!output_overflow) {
            job_db.output_overflow = 0;
            job_db.output = m_Output;
        } else {
            job_db.output_overflow = 1;
            job_db.output = "";
        }
        job_db.progress_msg = m_ProgressMsg;
    }

    // JobInfoDB
    bool    nonempty_job_info = input_overflow || output_overflow;
    if (m_Dirty & fJobInfoPart) {
        job_info_db.id = m_Id;
        if (input_overflow)
            job_info_db.input = m_Input;
        else
            job_info_db.input = "";
        if (output_overflow)
            job_info_db.output = m_Output;
        else
            job_info_db.output = "";
    }
    if (flush_job)
        job_db.UpdateInsert();
    if (m_Dirty & fJobInfoPart) {
        if (nonempty_job_info)
            job_info_db.UpdateInsert();
        else
            job_info_db.Delete(CBDB_File::eIgnoreError);
    }

    // EventsDB
    unsigned n = 0;
    NON_CONST_ITERATE(vector<CJobEvent>, it, m_Events) {
        CJobEvent &         event = *it;

        if (event.m_Dirty) {
            events_db.id             = m_Id;
            events_db.event_id       = n;
            events_db.status         = int(event.m_Status);
            events_db.event          = int(event.m_Event);
            events_db.timestamp      = (double)event.m_Timestamp;
            events_db.node_addr      = event.m_NodeAddr;
            events_db.ret_code       = event.m_RetCode;
            events_db.client_node    = event.m_ClientNode;
            events_db.client_session = event.m_ClientSession;
            events_db.err_msg        = event.m_ErrorMsg;
            events_db.UpdateInsert();
            event.m_Dirty = false;
        }
        ++n;
    }

    m_New = false;
    m_Dirty = 0;

    return true;
}


bool CJob::ShouldNotifySubmitter(const CNSPreciseTime &  current_time) const
{
    // The very first event is always a submit
    if (m_SubmNotifTimeout && m_SubmNotifPort)
        return m_Events[0].m_Timestamp +
               CNSPreciseTime(m_SubmNotifTimeout, 0) >= current_time;
    return false;
}


bool CJob::ShouldNotifyListener(const CNSPreciseTime &  current_time,
                                TNSBitVector &          jobs_to_notify) const
{
    if (m_ListenerNotifAbsTime != kTimeZero &&
        m_ListenerNotifAddress != 0 &&
        m_ListenerNotifPort != 0) {
        if (m_ListenerNotifAbsTime >= current_time) {
            // Still need to notify
            return true;
        }

        // Notifications timed out
        jobs_to_notify.set_bit(m_Id, false);
        return false;
    }

    // There was no need to notify at all
    return false;
}


// Used to DUMP a job
string CJob::Print(const CQueue &               queue,
                   const CNSAffinityRegistry &  aff_registry,
                   const CNSGroupsRegistry &    group_registry) const
{
    CNSPreciseTime  timeout = m_Timeout;
    CNSPreciseTime  run_timeout = m_RunTimeout;
    CNSPreciseTime  read_timeout = m_ReadTimeout;
    CNSPreciseTime  pending_timeout = queue.GetPendingTimeout();
    CNSPreciseTime  queue_timeout = queue.GetTimeout();
    CNSPreciseTime  queue_run_timeout = queue.GetRunTimeout();
    CNSPreciseTime  queue_read_timeout = queue.GetReadTimeout();
    string          result;

    result.reserve(2048);   // Voluntary; must be enough for most of the cases

    if (m_Timeout == kTimeZero)
        timeout = queue_timeout;
    if (m_RunTimeout == kTimeZero)
        run_timeout = queue_run_timeout;
    if (m_ReadTimeout == kTimeZero)
        read_timeout = queue_read_timeout;

    CNSPreciseTime  exp_time;
    if (m_Status == CNetScheduleAPI::eRunning ||
        m_Status == CNetScheduleAPI::eReading)
        exp_time = GetExpirationTime(queue_timeout,
                                     queue_run_timeout,
                                     queue_read_timeout,
                                     pending_timeout,
                                     GetLastEventTime());
    else
        exp_time = GetExpirationTime(queue_timeout,
                                     queue_run_timeout,
                                     queue_read_timeout,
                                     pending_timeout,
                                     m_LastTouch);

    result = "OK:id: " + NStr::NumericToString(m_Id) + "\n"
             "OK:key: " + queue.MakeJobKey(m_Id) + "\n"
             "OK:status: " + CNetScheduleAPI::StatusToString(m_Status) + "\n"
             "OK:last_touch: " + NS_FormatPreciseTime(m_LastTouch) +"\n";

    if (m_Status == CNetScheduleAPI::eRunning ||
        m_Status == CNetScheduleAPI::eReading)
        result += "OK:erase_time: n/a (timeout: " +
                  NS_FormatPreciseTimeAsSec(timeout) +
                  " sec, pending timeout: " +
                  NS_FormatPreciseTimeAsSec(pending_timeout) + " sec)\n";
    else
        result += "OK:erase_time: " + NS_FormatPreciseTime(exp_time) +
                  " (timeout: " +
                  NS_FormatPreciseTimeAsSec(timeout) +
                  " sec, pending timeout: " +
                  NS_FormatPreciseTimeAsSec(pending_timeout) + " sec)\n";

    if (m_Status != CNetScheduleAPI::eRunning &&
        m_Status != CNetScheduleAPI::eReading) {
        result += "OK:run_expiration: n/a (timeout: " +
                  NS_FormatPreciseTimeAsSec(run_timeout) + " sec)\n"
                  "OK:read_expiration: n/a (timeout: " +
                  NS_FormatPreciseTimeAsSec(read_timeout) + " sec)\n";
    } else {
        if (m_Status == CNetScheduleAPI::eRunning) {
            result += "OK:run_expiration: " + NS_FormatPreciseTime(exp_time) +
                      " (timeout: " +
                      NS_FormatPreciseTimeAsSec(run_timeout) + " sec)\n"
                      "OK:read_expiration: n/a (timeout: " +
                      NS_FormatPreciseTimeAsSec(read_timeout) + " sec)\n";
        } else {
            // Reading job
            result += "OK:run_expiration: n/a (timeout: " +
                      NS_FormatPreciseTimeAsSec(run_timeout) + " sec)\n"
                      "OK:read_expiration: " + NS_FormatPreciseTime(exp_time) +
                      " (timeout: " +
                      NS_FormatPreciseTimeAsSec(read_timeout) + " sec)\n";
        }
    }

    if (m_SubmNotifPort != 0)
        result += "OK:subm_notif_port: " +
                  NStr::NumericToString(m_SubmNotifPort) + "\n";
    else
        result += "OK:subm_notif_port: n/a\n";

    if (m_SubmNotifTimeout != kTimeZero) {
        CNSPreciseTime  subm_exp_time = m_Events[0].m_Timestamp +
                                        m_SubmNotifTimeout;
        result += "OK:subm_notif_expiration: " +
                  NS_FormatPreciseTime(subm_exp_time) + " (timeout: " +
                  NS_FormatPreciseTimeAsSec(m_SubmNotifTimeout) + " sec)\n";
    }
    else
        result += "OK:subm_notif_expiration: n/a\n";

    if (m_ListenerNotifAddress == 0 || m_ListenerNotifPort == 0)
        result += "OK:listener_notif: n/a\n";
    else
        result += "OK:listener_notif: " +
                  CSocketAPI::gethostbyaddr(m_ListenerNotifAddress) + ":" +
                  NStr::NumericToString(m_ListenerNotifPort) + "\n";

    if (m_ListenerNotifAbsTime != kTimeZero)
        result += "OK:listener_notif_expiration: " +
                  NS_FormatPreciseTime(m_ListenerNotifAbsTime) + "\n";
    else
        result += "OK:listener_notif_expiration: n/a\n";


    // Print detailed information about the job events
    int                         event = 1;

    ITERATE(vector<CJobEvent>, it, m_Events) {
        unsigned int    addr = it->GetNodeAddr();

        result += "OK:event" + NStr::NumericToString(event++) + ": "
                  "client=";
        if (addr == 0)
            result += "ns ";
        else
            result += CSocketAPI::gethostbyaddr(addr) + " ";

        result += "event=" + CJobEvent::EventToString(it->GetEvent()) + " "
                  "status=" +
                  CNetScheduleAPI::StatusToString(it->GetStatus()) + " "
                  "ret_code=" + NStr::NumericToString(it->GetRetCode()) + " ";

        // Time part
        result += "timestamp=";
        CNSPreciseTime  start = it->GetTimestamp();
        if (start == kTimeZero)
            result += "n/a ";
        else
            result += "'" + NS_FormatPreciseTime(start) + "' ";

        // The rest
        result += "node='" + it->GetClientNode() + "' "
                  "session='" + it->GetClientSession() + "' "
                  "err_msg=" + it->GetQuotedErrorMsg() + "\n";
    }

    result += "OK:run_counter: " + NStr::NumericToString(m_RunCount) + "\n"
              "OK:read_counter: " + NStr::NumericToString(m_ReadCount) + "\n";

    if (m_AffinityId != 0)
        result += "OK:affinity: " + NStr::NumericToString(m_AffinityId) + " ('" +
                  NStr::PrintableString(aff_registry.GetTokenByID(m_AffinityId)) +
                  "')\n";
    else
        result += "OK:affinity: n/a\n";

    if (m_GroupId != 0)
        result += "OK:group: " + NStr::NumericToString(m_GroupId) + " ('" +
            NStr::PrintableString(group_registry.ResolveGroup(m_GroupId)) +
            "')\n";
    else
        result += "OK:group: n/a\n";

    result += "OK:mask: " + NStr::NumericToString(m_Mask) + "\n"
              "OK:input: '" + NStr::PrintableString(m_Input) + "'\n"
              "OK:output: '" + NStr::PrintableString(m_Output) + "'\n"
              "OK:progress_msg: '" + m_ProgressMsg + "'\n"
              "OK:remote_client_sid: " + NStr::PrintableString(m_ClientSID) + "\n"
              "OK:remote_client_ip: " + NStr::PrintableString(m_ClientIP) + "\n"
              "OK:ncbi_phid: " + NStr::PrintableString(m_NCBIPHID) + "\n";

    return result;
}


TJobStatus  CJob::GetStatusBeforeReading(void) const
{
    ssize_t     index = m_Events.size() - 1;
    while (index >= 0) {
        if (m_Events[index].GetStatus() == CNetScheduleAPI::eReading)
            break;
        --index;
    }

    --index;
    if (index < 0)
        NCBI_THROW(CNetScheduleException, eInternalError,
                   "inconsistency in the job history. "
                   "No reading status found or no event before reading.");
    return m_Events[index].GetStatus();
}


void CJob::Dump(FILE *  jobs_file) const
{
    // Fill in the job dump structure
    SJobDump        job_dump;

    job_dump.id = m_Id;
    job_dump.passport = m_Passport;
    job_dump.status = (int) m_Status;
    job_dump.timeout = (double)m_Timeout;
    job_dump.run_timeout = (double)m_RunTimeout;
    job_dump.read_timeout = (double)m_ReadTimeout;
    job_dump.subm_notif_port = m_SubmNotifPort;
    job_dump.subm_notif_timeout = (double)m_SubmNotifTimeout;
    job_dump.listener_notif_addr = m_ListenerNotifAddress;
    job_dump.listener_notif_port = m_ListenerNotifPort;
    job_dump.listener_notif_abstime = (double)m_ListenerNotifAbsTime;
    job_dump.run_counter = m_RunCount;
    job_dump.read_counter = m_ReadCount;
    job_dump.aff_id = m_AffinityId;
    job_dump.mask = m_Mask;
    job_dump.group_id = m_GroupId;
    job_dump.last_touch = (double)m_LastTouch;
    job_dump.progress_msg_size = m_ProgressMsg.size();
    job_dump.number_of_events = m_Events.size();

    job_dump.client_ip_size = m_ClientIP.size();
    memcpy(job_dump.client_ip, m_ClientIP.data(), m_ClientIP.size());
    job_dump.client_sid_size = m_ClientSID.size();
    memcpy(job_dump.client_sid, m_ClientSID.data(), m_ClientSID.size());
    job_dump.ncbi_phid_size = m_NCBIPHID.size();
    memcpy(job_dump.ncbi_phid, m_NCBIPHID.data(), m_NCBIPHID.size());

    try {
        job_dump.Write(jobs_file, m_ProgressMsg.data());
    } catch (const exception &  ex) {
        throw runtime_error("Writing error while dumping a job properties: " +
                            string(ex.what()));
    }

    // Fill in the events structure
    for (vector<CJobEvent>::const_iterator it = m_Events.begin();
         it != m_Events.end(); ++it) {
        const CJobEvent &   event = *it;
        SJobEventsDump      events_dump;

        events_dump.event = int(event.m_Event);
        events_dump.status = int(event.m_Status);
        events_dump.timestamp = (double)event.m_Timestamp;
        events_dump.node_addr = event.m_NodeAddr;
        events_dump.ret_code = event.m_RetCode;
        events_dump.client_node_size = event.m_ClientNode.size();
        events_dump.client_session_size = event.m_ClientSession.size();
        events_dump.err_msg_size = event.m_ErrorMsg.size();

        try {
            events_dump.Write(jobs_file, event.m_ClientNode.data(),
                                         event.m_ClientSession.data(),
                                         event.m_ErrorMsg.data());
        } catch (const exception &  ex) {
            throw runtime_error("Writing error while dumping a job events: " +
                                string(ex.what()));
        }
    }

    // Fill in the job input/output structure
    SJobIODump      job_io_dump;

    job_io_dump.input_size = m_Input.size();
    job_io_dump.output_size = m_Output.size();

    try {
        job_io_dump.Write(jobs_file, m_Input.data(), m_Output.data());
    } catch (const exception &  ex) {
        throw runtime_error("Writing error while dumping a job "
                            "input/output: " + string(ex.what()));
    }
}


// true => job loaded
// false => EOF
// exception => reading problem
bool CJob::LoadFromDump(FILE *  jobs_file,
                        char *  input_buf,
                        char *  output_buf)
{
    SJobDump        job_dump;
    char            progress_msg_buf[kNetScheduleMaxDBDataSize];

    if (job_dump.Read(jobs_file, progress_msg_buf) == 1)
        return false;

    // Fill in the job fields
    m_New = true;
    m_Deleted = false;
    m_Id = job_dump.id;
    m_Passport = job_dump.passport;
    m_Status = (TJobStatus)job_dump.status;
    m_Timeout = CNSPreciseTime(job_dump.timeout);
    m_RunTimeout = CNSPreciseTime(job_dump.run_timeout);
    m_ReadTimeout = CNSPreciseTime(job_dump.read_timeout);
    m_SubmNotifPort = job_dump.subm_notif_port;
    m_SubmNotifTimeout = CNSPreciseTime(job_dump.subm_notif_timeout);
    m_ListenerNotifAddress = job_dump.listener_notif_addr;
    m_ListenerNotifPort = job_dump.listener_notif_port;
    m_ListenerNotifAbsTime = CNSPreciseTime(job_dump.listener_notif_abstime);
    m_RunCount = job_dump.run_counter;
    m_ReadCount = job_dump.read_counter;
    m_ProgressMsg.clear();
    if (job_dump.progress_msg_size > 0)
        m_ProgressMsg = string(progress_msg_buf, job_dump.progress_msg_size);
    m_AffinityId = job_dump.aff_id;
    m_Mask = job_dump.mask;
    m_GroupId = job_dump.group_id;
    m_LastTouch = CNSPreciseTime(job_dump.last_touch);
    m_ClientIP = string(job_dump.client_ip, job_dump.client_ip_size);
    m_ClientSID = string(job_dump.client_sid, job_dump.client_sid_size);
    m_NCBIPHID = string(job_dump.ncbi_phid, job_dump.ncbi_phid_size);

    // Read the job events
    m_Events.clear();
    SJobEventsDump  event_dump;
    char            client_node_buf[kMaxWorkerNodeIdSize];
    char            client_session_buf[kMaxWorkerNodeIdSize];
    char            err_msg_buf[kNetScheduleMaxDBErrSize];
    for (size_t  k = 0; k < job_dump.number_of_events; ++k) {
        if (event_dump.Read(jobs_file, client_node_buf, client_session_buf,
                                       err_msg_buf) != 0)
            throw runtime_error("Unexpected end of the dump file. "
                                "Cannot read expected job events." );

        // Fill the event and append it to the job events
        CJobEvent       event;
        event.m_Dirty = true;
        event.m_Status = (TJobStatus)event_dump.status;
        event.m_Event = (CJobEvent::EJobEvent)event_dump.event;
        event.m_Timestamp = CNSPreciseTime(event_dump.timestamp);
        event.m_NodeAddr = event_dump.node_addr;
        event.m_RetCode = event_dump.ret_code;
        event.m_ClientNode.clear();
        if (event_dump.client_node_size > 0)
            event.m_ClientNode = string(client_node_buf,
                                        event_dump.client_node_size);
        event.m_ClientSession.clear();
        if (event_dump.client_session_size > 0)
            event.m_ClientSession = string(client_session_buf,
                                           event_dump.client_session_size);
        event.m_ErrorMsg.clear();
        if (event_dump.err_msg_size > 0)
            event.m_ErrorMsg = string(err_msg_buf,
                                      event_dump.err_msg_size);

        m_Events.push_back(event);
    }


    // Read the job input/output
    SJobIODump  io_dump;

    if (io_dump.Read(jobs_file, input_buf, output_buf) != 0)
        throw runtime_error("Unexpected end of the dump file. "
                            "Cannot read expected job input/output." );

    SetInput(string(input_buf, io_dump.input_size));
    SetOutput(string(output_buf, io_dump.output_size));
    return true;
}



END_NCBI_SCOPE

