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
 * Author: Pavel Ivanov
 *
 */

#include "nc_pch.hpp"

#include "nc_utils.hpp"


BEGIN_NCBI_SCOPE;


map<int, string> s_MsgForStatus;
map<string, int> s_StatusForMsg;


struct SStatusMsg
{
    int status;
    const char* msg;
};


static SStatusMsg s_StatusMessages[] =
    {
        {eStatus_JustStarted, "ERR:Caching is not completed"},
        {eStatus_Disabled, "ERR:Cache is disabled"},
        {eStatus_NotAllowed, "ERR:Password in the command doesn't match server settings"},
        {eStatus_NoDiskSpace, "ERR:Not enough disk space"},
        {eStatus_BadPassword, "ERR:Access denied."},
        {eStatus_NotFound, "ERR:BLOB not found."},
        {eStatus_CondFailed, "ERR:Too few data for blob"},
        {eStatus_NoImpl, "ERR:Not implemented"},
        {eStatus_ShuttingDown, "ERR:Shutting down"}
    };



void
InitClientMessages(void)
{
    Uint1 num_elems = Uint1(sizeof(s_StatusMessages) / sizeof(s_StatusMessages[0]));
    for (Uint1 i = 0; i < num_elems; ++i) {
        SStatusMsg& stat_msg = s_StatusMessages[i];
        s_MsgForStatus[stat_msg.status] = stat_msg.msg;
        s_StatusForMsg[stat_msg.msg] = stat_msg.status;
    }
}

/////////////////////////////////////////////////////////////////////////////
// alerts


class NCAlertData
{
public:
    NCAlertData(const string& message) :
        m_LastDetectedTimestamp(CSrvTime::Current()),
        m_Message(message), m_TotalCount(1), m_ThisCount(1), m_On(true) {
    }
    void Update(const string& message) {
        m_LastDetectedTimestamp = CSrvTime::Current();
        m_Message = message;
        ++m_TotalCount;
        m_On ? ++m_ThisCount : (m_ThisCount = 1);
        m_On = true;
    }
    void Acknowledge(const string& user) {
        m_AcknowledgedTimestamp = CSrvTime::Current();
        m_User = user;
        m_On = false;
    }
    bool IsOn(void) const {
        return m_On;
    }
    void Report(CSrvSocketTask& task);
private:
    CSrvTime      m_LastDetectedTimestamp;
    CSrvTime      m_AcknowledgedTimestamp;
    string        m_Message;
    string        m_User;
    size_t        m_TotalCount;
    size_t        m_ThisCount;
    bool          m_On;
};
static CMiniMutex                       s_Lock;
static map< CNCAlerts::EAlertType, NCAlertData >   s_Alerts;


void NCAlertData::Report(CSrvSocketTask& task)
{
    char time_buf[50];
    string is("\": "),iss("\": \""), eol(",\n\""), eos("\"");

    task.WriteText("{\n");
    task.WriteText(eos).WriteText("message" ).WriteText(iss).WriteText(m_Message).WriteText(eos);
    m_LastDetectedTimestamp.Print(time_buf, CSrvTime::eFmtHumanSeconds);
    task.WriteText(eol).WriteText("on" ).WriteText(is).WriteText(m_On? "true" : "false");
    task.WriteText(eol).WriteText("count" ).WriteText(is).WriteNumber(m_ThisCount);
    task.WriteText(eol).WriteText("lifetime_count" ).WriteText(is).WriteNumber(m_TotalCount);
    task.WriteText(eol).WriteText("last_detected_time" ).WriteText(iss).WriteText(time_buf).WriteText(eos);
    task.WriteText(eol).WriteText("acknowledged_time" );
    if (m_AcknowledgedTimestamp.Compare(CSrvTime()) == 0) {
        task.WriteText("\": null");
    } else {
        m_AcknowledgedTimestamp.Print(time_buf, CSrvTime::eFmtHumanSeconds);
        task.WriteText(iss).WriteText(time_buf).WriteText(eos);
    }
    task.WriteText(eol).WriteText("user" ).WriteText(iss).WriteText(m_User).WriteText(eos);
    task.WriteText("\n}");
}

void CNCAlerts::Report(CSrvSocketTask& task, bool report_all)
{
    s_Lock.Lock();
    task.WriteText(",\n\"alerts\": [");
    bool first = true;
    for(map< EAlertType, NCAlertData >::iterator it = s_Alerts.begin(); it != s_Alerts.end(); ++it) {
        if (report_all || it->second.IsOn()) {
            if (!first) {
                task.WriteText(",");
            }
            it->second.Report(task);
            first = false;
        }
    }
    task.WriteText("]");
    s_Lock.Unlock();
}

void CNCAlerts::Register(EAlertType alert_type, const string&  message)
{
    string msg(x_TypeToId(alert_type) + ": " + message);
    s_Lock.Lock();
    map< EAlertType, NCAlertData >::iterator found = s_Alerts.find(alert_type);
    if (found != s_Alerts.end()) {
        found->second.Update(msg);
    } else {
        s_Alerts.insert( make_pair(alert_type, NCAlertData(msg)));
    }
    s_Lock.Unlock();
}

CNCAlerts::EAlertAckResult CNCAlerts::Acknowledge(const string&  alert_id,
                                                  const string&  user)
{
    EAlertType  alert_type = x_IdToType(alert_id);
    s_Lock.Lock();
    CNCAlerts::EAlertAckResult res = CNCAlerts::eNotFound;
    map< EAlertType, NCAlertData >::iterator found = s_Alerts.find(alert_type);
    if (found != s_Alerts.end()) {
        found->second.Acknowledge(user);
        res = CNCAlerts::eAcknowledged;
    }
    s_Lock.Unlock();
    return res;
}


struct AlertToId
{
    CNCAlerts::EAlertType     type;
    string              id;
};

static const AlertToId  s_alertToIdMap[] = {
    {CNCAlerts::eUnknown,              "Unknown"},
    {CNCAlerts::eStartupConfigChanged, "StartupConfigChanged"},
    {CNCAlerts::ePidFileFailed,        "PidFileFailed"},
    {CNCAlerts::eStartAfterCrash,      "StartAfterCrash"},
    {CNCAlerts::eStorageReinit,        "StorageReinit"},
    {CNCAlerts::eAccessDenied,         "AccessDenied"},
    {CNCAlerts::eSyncFailed,           "SyncFailed"},
    {CNCAlerts::ePeerIpChanged,        "PeerIpChanged"},
    {CNCAlerts::eDatabaseTooLarge,     "DatabaseTooLarge"},
    {CNCAlerts::eDatabaseOverLimit,    "DatabaseOverLimit"},
    {CNCAlerts::eDiskSpaceLow,         "DiskSpaceLow"},
    {CNCAlerts::eDiskSpaceCritical,    "DiskSpaceCritical"}
};
static const size_t  s_alertToIdMapSize = sizeof(s_alertToIdMap) / sizeof(AlertToId);

CNCAlerts::EAlertType CNCAlerts::x_IdToType(const string&  alert_id)
{
    for (size_t  k = 0; k < s_alertToIdMapSize; ++k) {
        if (NStr::CompareNocase(alert_id, s_alertToIdMap[k].id) == 0)
            return s_alertToIdMap[k].type;
    }
    return CNCAlerts::eUnknown;
}

string  CNCAlerts::x_TypeToId(CNCAlerts::EAlertType  type)
{
    for (size_t  k = 0; k < s_alertToIdMapSize; ++k) {
        if (s_alertToIdMap[k].type == type)
            return s_alertToIdMap[k].id;
    }
    return "Unknown";
}

END_NCBI_SCOPE;
