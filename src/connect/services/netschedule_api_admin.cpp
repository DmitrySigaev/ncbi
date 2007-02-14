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
 * Author:  Anatoliy Kuznetsov, Maxim Didenko
 *
 * File Description:
 *   Implementation of NetSchedule API.
 *
 */

#include <ncbi_pch.hpp>
#include <connect/services/netschedule_api.hpp>


BEGIN_NCBI_SCOPE

void CNetScheduleAdmin::DropJob(const string& job_key) const
{
    m_API->x_SendJobCmdWaitResponse("DROJ", job_key);
}

void CNetScheduleAdmin::ShutdownServer(CNetScheduleAdmin::EShutdownLevel level) const
{
    if (m_API->GetPoll().IsLoadBalanced()) {
        NCBI_THROW(CNetScheduleException, eCommandIsNotAllowed, 
                   "This command is allowed only for non-loadbalance client.");
    }

    string cmd = "SHUTDOWN ";
    switch( level ) {
    case eDie :
        cmd = "SHUTDOWN SUICIDE ";
        break;
    case eShutdownImmidiate :
        cmd = "SHUTDOWN IMMEDIATE ";
        break;
    default:
        break;
    }
     
    m_API->SendCmdWaitResponse(m_API->x_GetConnector(), cmd); 
}


void CNetScheduleAdmin::ReloadServerConfig() const
{
    if (m_API->GetPoll().IsLoadBalanced()) {
        NCBI_THROW(CNetScheduleException, eCommandIsNotAllowed, 
                   "This command is allowed only for non-loadbalance client.");
    }
    m_API->SendCmdWaitResponse(m_API->x_GetConnector(), "RECO"); 

}

void CNetScheduleAdmin::CreateQueue(const string& qname, const string& qclass,
                                    const string& comment) const
{
    string param = "QCRE " + qname + " " + qclass;
    if (!comment.empty()) {
        param.append(" \"");
        param.append(comment);
        param.append("\"");
    }
    for (CNetSrvConnectorPoll::iterator it = m_API->GetPoll().begin(); 
         it != m_API->GetPoll().end(); ++it) {
        m_API->SendCmdWaitResponse(*it, param); 
    }
}


void CNetScheduleAdmin::DeleteQueue(const string& qname) const
{
    string param = "QDEL " + qname;

    for (CNetSrvConnectorPoll::iterator it = m_API->GetPoll().begin(); 
         it != m_API->GetPoll().end(); ++it) {
        m_API->SendCmdWaitResponse(*it, param); 
    }
}

void CNetScheduleAdmin::DumpJob(CNcbiOstream& out, const string& job_key) const
{
    string cmd = "DUMP " + job_key + "\r\n";
    CNetSrvConnectorHolder conn = m_API->x_GetConnector(job_key);
    conn->WriteStr(cmd);
    m_API->PrintServerOut(conn, out);
}

void CNetScheduleAdmin::DropQueue() const
{
    string cmd = "DROPQ";

    for (CNetSrvConnectorPoll::iterator it = m_API->GetPoll().begin(); 
         it != m_API->GetPoll().end(); ++it) {
        m_API->SendCmdWaitResponse(*it, cmd); 
    }
}


///////////////////????????????????????????/////////////////////////
void CNetScheduleAdmin::GetServerVersion(ISink& sink) const
{ 
    string cmd = "VERSION";
    for (CNetSrvConnectorPoll::iterator it = m_API->GetPoll().begin(); 
         it != m_API->GetPoll().end(); ++it) {
        CNcbiOstream& os = sink.GetOstream(m_API->GetConnectionInfo(*it));
        os << m_API->SendCmdWaitResponse(*it, cmd);
    }        
}



void CNetScheduleAdmin::DumpQueue(ISink& sink) const
{
    string cmd = "DUMP\r\n";        
    for (CNetSrvConnectorPoll::iterator it = m_API->GetPoll().begin(); 
         it != m_API->GetPoll().end(); ++it) {
        CNcbiOstream& os = sink.GetOstream(m_API->GetConnectionInfo(*it));
        it->WriteStr(cmd);
        m_API->PrintServerOut(*it, os);
    }
}


void CNetScheduleAdmin::PrintQueue(ISink& sink, CNetScheduleAPI::EJobStatus status) const
{
    string cmd = "QPRT " + CNetScheduleAPI::StatusToString(status) + "\r\n";        
    for (CNetSrvConnectorPoll::iterator it = m_API->GetPoll().begin(); 
         it != m_API->GetPoll().end(); ++it) {
        CNcbiOstream& os = sink.GetOstream(m_API->GetConnectionInfo(*it));
        it->WriteStr(cmd);
        m_API->PrintServerOut(*it, os);
    }
}



void CNetScheduleAdmin::GetServerStatistics(ISink& sink, 
                                            EStatisticsOptions opt) const
{
    string cmd = "STAT";
    if (opt == eStatisticsAll) {
        cmd += " ALL";
    }
    cmd += "\r\n";
    for(CNetSrvConnectorPoll::iterator it = m_API->GetPoll().begin(); 
        it != m_API->GetPoll().end(); ++it) {
        CNcbiOstream& os = sink.GetOstream(m_API->GetConnectionInfo(*it));
        it->WriteStr(cmd);
        m_API->PrintServerOut(*it, os);
    }
}


void CNetScheduleAdmin::Monitor(CNcbiOstream & out) const
{
    if (m_API->GetPoll().IsLoadBalanced()) {
        NCBI_THROW(CNetScheduleException, eCommandIsNotAllowed, 
                   "This command is allowed only for non-loadbalance client.");
    }
    
    CNetSrvConnectorHolder conn = m_API->x_GetConnector();
    conn->WriteStr("MONI QUIT\r\n");

    conn->Telnet(out, "END");
}

void CNetScheduleAdmin::Logging(bool on_off) const
{

    if (m_API->GetPoll().IsLoadBalanced()) {
        NCBI_THROW(CNetScheduleException, eCommandIsNotAllowed, 
                   "This command is allowed only for non-loadbalance client.");
    }
    string cmd = "LOG ";
    cmd += on_off ? "ON" : "OFF";

    m_API->SendCmdWaitResponse(m_API->x_GetConnector(), cmd); 
}


void CNetScheduleAdmin::GetQueueList(ISink& sink) const
{
    string cmd = "QLST";
    for(CNetSrvConnectorPoll::iterator it = m_API->GetPoll().begin(); 
        it != m_API->GetPoll().end(); ++it) {
        CNcbiOstream& os = sink.GetOstream(m_API->GetConnectionInfo(*it));
        os << m_API->SendCmdWaitResponse(*it, cmd);
    }
}


void CNetScheduleAdmin::StatusSnapshot(CNetScheduleAdmin::TStatusMap&  status_map,
                                       const string& affinity_token) const
{
    string cmd = "STSN";
    cmd.append(" aff=\"");
    cmd.append(affinity_token);
    cmd.append("\"");

    for(CNetSrvConnectorPoll::iterator it = m_API->GetPoll().begin(); 
        it != m_API->GetPoll().end(); ++it) {
        it->WriteStr(cmd);
        it->WaitForServer();

        string line;
        while (true) {
            if (it->ReadStr(line)) {
                break;
            }
            m_API->CheckServerOK(line);
            if (line == "END")
                break;

            // parse the status message
            string st_str, cnt_str;
            bool delim = NStr::SplitInTwo(line, " ", st_str, cnt_str);
            if (delim) {
                CNetScheduleAPI::EJobStatus status = CNetScheduleAPI::StringToStatus(st_str);
                unsigned cnt = NStr::StringToUInt(cnt_str);
                status_map[status] += cnt;
            }
        }
    }
}

unsigned long CNetScheduleAdmin::Count(const string& query) const
{
    unsigned long count = 0;
    string cmd = "QERY ";
    cmd.append("\"");
    cmd.append(NStr::PrintableString(query));
    cmd.append("\"");

    for(CNetSrvConnectorPoll::iterator it = m_API->GetPoll().begin(); 
        it != m_API->GetPoll().end(); ++it) {
        string resp = m_API->SendCmdWaitResponse(*it, cmd);
        count += NStr::StringToULong(resp);
    }
    return count;
}
void CNetScheduleAdmin::ForceReschedule(const string& query) const
{
}
void CNetScheduleAdmin::Cancel(const string& query) const
{
}
void CNetScheduleAdmin::Select(const string& query, const vector<string>& fields, CNcbiOstream& os) const
{
}

END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log: netschedule_api_admin.cpp,v $
 * Revision 6.1  2007/01/09 15:29:55  didenko
 * Added new API for NetSchedule service
 *
 * ===========================================================================
 */
