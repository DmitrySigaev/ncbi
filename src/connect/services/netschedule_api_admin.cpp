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
 * Author:  Anatoliy Kuznetsov, Maxim Didenko, Dmitry Kazimirov
 *
 * File Description:
 *   Implementation of NetSchedule API.
 *
 */

#include <ncbi_pch.hpp>

#include "netschedule_api_impl.hpp"

#include <connect/services/netschedule_api.hpp>


BEGIN_NCBI_SCOPE

void CNetScheduleAdmin::DropJob(const string& job_key)
{
    m_Impl->m_API->x_SendJobCmdWaitResponse("DROJ", job_key);
}

void CNetScheduleAdmin::ShutdownServer(CNetScheduleAdmin::EShutdownLevel level) const
{
    string cmd;

    switch (level) {
    case eDie:
        cmd = "SHUTDOWN SUICIDE ";
        break;
    case eShutdownImmediate:
        cmd = "SHUTDOWN IMMEDIATE ";
        break;
    default:
        cmd = "SHUTDOWN ";
    }

    m_Impl->m_API->m_Service->RequireStandAloneServerSpec().Exec(cmd);
}


void CNetScheduleAdmin::ForceReschedule(const string& job_key) const
{
    m_Impl->m_API->x_SendJobCmdWaitResponse("FRES", job_key);
}

void CNetScheduleAdmin::ReloadServerConfig() const
{
    m_Impl->m_API->m_Service->RequireStandAloneServerSpec().Exec("RECO");
}

void CNetScheduleAdmin::CreateQueue(const string& qname, const string& qclass,
                                    const string& comment, ECreateQueueFlags flags) const
{
    string param = "QCRE " + qname;
    param += ' ';
    param += qclass;

    if (!comment.empty()) {
        param += " \"";
        param += comment;
        param += '"';
    }

    for (CNetServerGroupIterator it =
            m_Impl->m_API->m_Service.DiscoverServers().Iterate(); it; ++it) {
        try {
            (*it).Connect().Exec(param);
        }
        catch (CNetScheduleException& ex) {
            if (flags != eIgnoreDuplicateName &&
                    ex.GetErrCode() == CNetScheduleException::eDuplicateName)
                throw;
        }
    }
}


void CNetScheduleAdmin::DeleteQueue(const string& qname) const
{
    string param = "QDEL " + qname;

    for (CNetServerGroupIterator it =
            m_Impl->m_API->m_Service.DiscoverServers().Iterate(); it; ++it)
        (*it).Connect().Exec(param);
}

void CNetScheduleAdmin::DumpJob(CNcbiOstream& out, const string& job_key) const
{
    CNetServerCmdOutput output = m_Impl->m_API->x_GetConnection(
        job_key).ExecMultiline("DUMP " + job_key);

    std::string line;

    while (output.ReadLine(line))
        out << line << "\n";
}

void CNetScheduleAdmin::DropQueue() const
{
    string cmd = "DROPQ";

    for (CNetServerGroupIterator it =
            m_Impl->m_API->m_Service.DiscoverServers().Iterate(); it; ++it)
        (*it).Connect().Exec(cmd);
}


void CNetScheduleAdmin::PrintServerVersion(CNcbiOstream& output_stream) const
{
    m_Impl->m_API->m_Service.PrintCmdOutput("VERSION",
        output_stream, CNetService::eSingleLineOutput);
}


void CNetScheduleAdmin::DumpQueue(CNcbiOstream& output_stream) const
{
    m_Impl->m_API->m_Service.PrintCmdOutput("DUMP",
        output_stream, CNetService::eMultilineOutput);
}


void CNetScheduleAdmin::PrintQueue(CNcbiOstream& output_stream,
    CNetScheduleAPI::EJobStatus status) const
{
    string cmd = "QPRT " + CNetScheduleAPI::StatusToString(status);
    m_Impl->m_API->m_Service.PrintCmdOutput(cmd,
        output_stream, CNetService::eMultilineOutput);
}

unsigned CNetScheduleAdmin::CountActiveJobs()
{
    return NStr::StringToUInt(m_Impl->m_API->m_Service->
        RequireStandAloneServerSpec().Exec("ACNT"));
}

void CNetScheduleAdmin::GetWorkerNodes(
    std::list<SWorkerNodeInfo>& worker_nodes) const
{
    worker_nodes.clear();

    std::set<std::pair<std::string, unsigned short> > m_Unique;

    for (CNetServerGroupIterator it =
            m_Impl->m_API->m_Service.DiscoverServers().Iterate(); it; ++it) {
        CNetServerConnection conn = (*it).Connect();

        CNetServerCmdOutput output = conn.ExecMultiline("STAT");

        bool nodes_info = false;
        string response;

        while (output.ReadLine(response)) {
            if (NStr::StartsWith(response, "[Configured"))
                break;
            if (!nodes_info) {
                if (NStr::Compare(response, "[Worker node statistics]:") == 0)
                    nodes_info = true;
                else
                    continue;
            } else {
                if (response.empty())
                    continue;

                string name;
                NStr::SplitInTwo(response, " ", name, response);
                NStr::TruncateSpacesInPlace(response);
                string prog;
                NStr::SplitInTwo(response, "@", prog, response);
                NStr::TruncateSpacesInPlace(response);
                prog = prog.substr(6,prog.size()-8);
                string host;
                NStr::SplitInTwo(response, " ", host, response);

                if (NStr::Compare(host, "localhost") == 0)
                    host = CSocketAPI::gethostbyaddr(
                        CSocketAPI::gethostbyname(conn.GetHost()));

                NStr::TruncateSpacesInPlace(response);

                string sport, stime;

                NStr::SplitInTwo(response, " ", sport, stime);
                NStr::SplitInTwo(sport, ":", response, sport);

                unsigned short port = (unsigned short) NStr::StringToInt(sport);

                if (m_Unique.insert(make_pair(host,port)).second) {
                    worker_nodes.push_back(SWorkerNodeInfo());

                    SWorkerNodeInfo& wn_info = worker_nodes.back();
                    wn_info.name = name;
                    wn_info.prog = prog;
                    wn_info.host = host;
                    wn_info.port = port;

                    NStr::TruncateSpacesInPlace(stime);
                    wn_info.last_access = CTime(stime, "M/D/Y h:m:s");
                }
            }
        }
    }
}

void CNetScheduleAdmin::PrintServerStatistics(CNcbiOstream& output_stream,
    EStatisticsOptions opt) const
{
    m_Impl->m_API->m_Service.PrintCmdOutput(
        opt == eStatisticsBrief ? "STAT" : "STAT ALL",
        output_stream, CNetService::eMultilineOutput);
}


void CNetScheduleAdmin::Monitor(CNcbiOstream & out) const
{
    CNetServerConnection conn =
        m_Impl->m_API->m_Service->RequireStandAloneServerSpec();
    conn->WriteLine("MONI QUIT");
    conn.Telnet(out);
}

void CNetScheduleAdmin::Logging(bool on_off) const
{
    m_Impl->m_API->m_Service->RequireStandAloneServerSpec().Exec(
        on_off ? "LOG ON" : "LOG OFF");
}


void CNetScheduleAdmin::GetQueueList(TQueueList& qlist) const
{
    for (CNetServerGroupIterator it =
            m_Impl->m_API->m_Service.DiscoverServers().Iterate(); it; ++it) {
        CNetServer server = *it;

        qlist.push_back(SServerQueueList(server));

        NStr::Split(server.Connect().Exec("QLST"), ",;", qlist.back().queues);
    }
}

void CNetScheduleAdmin::StatusSnapshot(
    CNetScheduleAdmin::TStatusMap&  status_map,
    const string& affinity_token) const
{
    string cmd = "STSN aff=\"";
    cmd.append(NStr::PrintableString(affinity_token));
    cmd.append("\"");

    TDiscoveredServers servers;

    CNetService service = m_Impl->m_API->m_Service;

    service->DiscoverServers(servers);

    ITERATE(TDiscoveredServers, it, servers) {
        CNetServerCmdOutput cmd_output = service->
            GetConnection(*it).ExecMultiline(cmd);

        string output_line;

        while (cmd_output.ReadLine(output_line)) {
            // parse the status message
            string st_str, cnt_str;
            if (NStr::SplitInTwo(output_line, " ", st_str, cnt_str)) {
                status_map[CNetScheduleAPI::StringToStatus(st_str)] +=
                    NStr::StringToUInt(cnt_str);
            }
        }
    }
}

unsigned long CNetScheduleAdmin::Count(const string& query) const
{
    std::string cmd = "QERY \"";
    cmd.append(NStr::PrintableString(query));
    cmd.append("\" COUNT");

    unsigned long counter = 0;

    for (CNetServerGroupIterator it =
            m_Impl->m_API->m_Service.DiscoverServers().Iterate(); it; ++it) {
        counter += NStr::StringToULong((*it).Connect().Exec(cmd));
    }

    return counter;
}

void CNetScheduleAdmin::Query(const string& query,
    const vector<string>& fields, CNcbiOstream& os) const
{
    if (fields.empty())
        return;

    string cmd = "QERY \"";
    cmd += NStr::PrintableString(query);
    cmd += "\" SLCT \"";

    string sfields;
    ITERATE(vector<string>, it, fields) {
        if (it != fields.begin() )
            sfields += ',';
        sfields += *it;
    }

    cmd += NStr::PrintableString(sfields);
    cmd += '"';

    m_Impl->m_API->m_Service.PrintCmdOutput(cmd, os,
        CNetService::eDumpNoHeaders);
}

void CNetScheduleAdmin::Select(const string& select_stmt, CNcbiOstream& os) const
{
    string cmd = "QSEL \"";
    cmd += NStr::PrintableString(select_stmt);
    cmd += '"';

    m_Impl->m_API->m_Service.PrintCmdOutput(cmd, os,
        CNetService::eDumpNoHeaders);
}

void CNetScheduleAdmin::RetrieveKeys(const string& query,
    CNetScheduleKeys& ids) const
{
    SNetScheduleAdminImpl::TIDsMap inter_ids;

    std::string cmd = "QERY \"";
    cmd.append(NStr::PrintableString(query));
    cmd.append("\" IDS");

    for (CNetServerGroupIterator it =
            m_Impl->m_API->m_Service.DiscoverServers().Iterate(); it; ++it) {
        CNetServer server = *it;

        inter_ids[SNetScheduleAdminImpl::TIDsMap::key_type(CSocketAPI::ntoa(
            CSocketAPI::gethostbyname(server->m_Address.first)),
            server->m_Address.second)] = server.Connect().Exec(cmd);
    }

    ids.x_Clear();
    ITERATE(SNetScheduleAdminImpl::TIDsMap, it, inter_ids) {
        ids.x_Add(it->first, it->second);
    }
}

END_NCBI_SCOPE
