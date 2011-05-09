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
 *   Government have not placed any restriction on its use or reproduction.
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
 * Authors:  Dmitry Kazimirov
 *
 * File Description: NetSchedule-specific commands of the grid_cli application.
 *
 */

#include <ncbi_pch.hpp>

#include "grid_cli.hpp"

#include <connect/services/grid_rw_impl.hpp>

USING_NCBI_SCOPE;

#define MAX_VISIBLE_DATA_LENGTH 50

void CGridCommandLineInterfaceApp::SetUp_NetScheduleCmd(
    CGridCommandLineInterfaceApp::EAPIClass api_class)
{
    if (m_Opts.auth.empty())
        m_Opts.auth = PROGRAM_NAME;

    string queue(!m_Opts.queue.empty() ? m_Opts.queue : "noname");

    m_NetScheduleAPI = CNetScheduleAPI(m_Opts.ns_service, m_Opts.auth, queue);

    if (!m_Opts.id.empty() && !m_Opts.ns_service.empty()) {
        string host, port;

        if (!NStr::SplitInTwo(m_Opts.ns_service, ":", host, port)) {
            NCBI_THROW(CArgException, eInvalidArg,
                "When job ID is given, '--netschedule' "
                "must be a host:port server address.");
        }

        m_NetScheduleAPI.GetService().StickToServer(host,
            NStr::StringToInt(port));
    }

    if (IsOptionSet(eCompatMode))
        m_NetScheduleAPI.UseOldStyleAuth();

    // If api_class == eWorkerNode: m_NetScheduleAPI.EnableWorkerNodeCompatMode();

    switch (api_class) {
    case eNetScheduleAdmin:
        m_NetScheduleAdmin = m_NetScheduleAPI.GetAdmin();
        break;
    case eNetScheduleSubmitter:
        m_NetScheduleSubmitter = m_NetScheduleAPI.GetSubmitter();
    }
}

void CGridCommandLineInterfaceApp::SetUp_GridClient()
{
    SetUp_NetScheduleCmd(eNetScheduleAPI);
    SetUp_NetCacheCmd(eNetCacheAPI);
    m_GridClient.reset(new CGridClient(m_NetScheduleAPI.GetSubmitter(),
        m_NetCacheAPI, CGridClient::eManualCleanup,
            CGridClient::eProgressMsgOn));
}

void CGridCommandLineInterfaceApp::PrintJobMeta(const CNetScheduleKey& key)
{
    printf("Job number: %u\n"
        "Created by: %s:%u\n",
        key.id,
        g_NetService_TryResolveHost(key.host).c_str(),
        key.port);

    if (!key.queue.empty())
        printf("Queue name: %s\n", key.queue.c_str());
}

void CGridCommandLineInterfaceApp::PrintStorageType(
    const string& data, const char* prefix)
{
    unsigned long data_length = (unsigned long) data.length();

    if (data_length >= 2 && data[1] == ' ') {
        if (data[0] == 'D') {
            printf("%sstorage: embedded, size=%lu\n", prefix, data_length - 2);
            return;
        } else if (data[0] == 'K') {
            printf("%sstorage: netcache, key=%s\n", prefix, data.c_str() + 2);
            return;
        }
    }

    printf("%sstorage: raw, size=%lu\n", prefix, data_length);
}

bool CGridCommandLineInterfaceApp::MatchPrefixAndPrintStorageTypeAndData(
    const string& line, const char* prefix, size_t prefix_length,
    const char* new_prefix)
{
    if (line.length() <= prefix_length || line[line.length() - 1] != '\'' ||
            !NStr::StartsWith(line, CTempString(prefix, prefix_length)))
        return false;

    string data(NStr::ParseEscapes(CTempString(line.data() +
        prefix_length, line.length() - 1 - prefix_length)));

    PrintStorageType(data, new_prefix);

    if (data.length() <= MAX_VISIBLE_DATA_LENGTH)
        printf("%sdata: '%s'\n", new_prefix, NStr::PrintableString(data).c_str());
    else {
        data.erase(data.begin() + MAX_VISIBLE_DATA_LENGTH, data.end());
        printf("%sdata: '%s'...\n", new_prefix, NStr::PrintableString(
            data.data(), MAX_VISIBLE_DATA_LENGTH).c_str());
    }

    return true;
}

int CGridCommandLineInterfaceApp::Cmd_JobInfo()
{
    SetUp_NetScheduleCmd(eNetScheduleAdmin);

    CNetScheduleAPI::EJobStatus status;

    if (IsOptionSet(eDeferExpiration)) {
        status = m_NetScheduleAPI.GetSubmitter().GetJobStatus(m_Opts.id);
        if (IsOptionSet(eStatusOnly)) {
            PrintLine(CNetScheduleAPI::StatusToString(status));
            return 0;
        }
    } else if (IsOptionSet(eStatusOnly)) {
        PrintLine(CNetScheduleAPI::StatusToString(
            m_NetScheduleAPI.GetExecuter().GetJobStatus(m_Opts.id)));
        return 0;
    }

    if (IsOptionSet(eProgressMessageOnly)) {
        CNetScheduleJob job;
        job.job_id = m_Opts.id;
        m_NetScheduleAPI.GetProgressMsg(job);
        if (!job.progress_msg.empty())
            printf("%s\n", job.progress_msg.c_str());
        return 0;
    }

    PrintJobMeta(CNetScheduleKey(m_Opts.id));

    if (!IsOptionSet(eBrief)) {
        CNetServerMultilineCmdOutput output =
            m_NetScheduleAdmin.DumpJob(m_Opts.id);

        string line;

        static const char s_VersionString[] = "NCBI NetSchedule";
        static const char s_IdPrefix[] = "id:";
        static const char s_InputPrefix[] = "input: '";
        static const char s_OutputPrefix[] = "output: '";

        while (output.ReadLine(line)) {
            if (!line.empty() &&
                line[0] != '[' &&
                !NStr::StartsWith(line, CTempString(s_VersionString,
                    sizeof(s_VersionString) - 1)) &&
                !NStr::StartsWith(line, CTempString(s_IdPrefix,
                    sizeof(s_IdPrefix) - 1)) &&
                !MatchPrefixAndPrintStorageTypeAndData(line, s_InputPrefix,
                    sizeof(s_InputPrefix) - 1, "input-") &&
                !MatchPrefixAndPrintStorageTypeAndData(line, s_OutputPrefix,
                    sizeof(s_OutputPrefix) - 1, "output-"))
                PrintLine(line);
        }
    } else {
        CNetScheduleJob job;
        job.job_id = m_Opts.id;
        status = m_NetScheduleAPI.GetJobDetails(job);

        printf("Status: %s\n", CNetScheduleAPI::StatusToString(status).c_str());

        if (status == CNetScheduleAPI::eJobNotFound)
            return 0;

        PrintStorageType(job.input, "Input ");

        switch (status) {
        default:
            if (job.output.empty())
                break;
            /* FALL THROUGH */

        case CNetScheduleAPI::eDone:
        case CNetScheduleAPI::eReading:
        case CNetScheduleAPI::eConfirmed:
        case CNetScheduleAPI::eReadFailed:
            PrintStorageType(job.output, "Output ");
            break;
        }

        if (!job.error_msg.empty())
            printf("Error message: %s\n", job.error_msg.c_str());
    }
    return 0;
}

int CGridCommandLineInterfaceApp::Cmd_SubmitJob()
{
    SetUp_GridClient();

    CGridJobSubmitter& submitter(m_GridClient->GetJobSubmitter());

    CNcbiOstream& job_input_stream = submitter.GetOStream();

    if (IsOptionSet(eInput)) {
        job_input_stream.write(m_Opts.input.data(), m_Opts.input.length());
        if (job_input_stream.bad())
            goto ErrorExit;
    } else {
        char buffer[16 * 1024];
        size_t bytes_read;

        while ((bytes_read = fread(buffer, 1,
                sizeof(buffer), m_Opts.input_stream)) > 0) {
            job_input_stream.write(buffer, bytes_read);
            if (job_input_stream.bad())
                goto ErrorExit;
            if (feof(m_Opts.input_stream))
                break;
        }
    }

    printf("%s\n", submitter.Submit(m_Opts.affinity).c_str());

    return 0;

ErrorExit:
    fprintf(stderr, PROGRAM_NAME ": error while writing job input.\n");
    return 3;
}

int CGridCommandLineInterfaceApp::DumpJobInputOutput(
    const string& data_or_blob_id)
{
    try {
        CStringOrBlobStorageReader reader(data_or_blob_id, m_NetCacheAPI);

        char buffer[16 * 1024];
        size_t bytes_read;

        while (reader.Read(buffer, sizeof(buffer), &bytes_read) != eRW_Eof)
            if (fwrite(buffer, 1, bytes_read,
                    m_Opts.output_stream) < bytes_read)
                goto Error;
    }
    catch (CStringOrBlobStorageRWException& e) {
        if (e.GetErrCode() != CStringOrBlobStorageRWException::eInvalidFlag)
            throw;
        if (fwrite(data_or_blob_id.data(), 1, data_or_blob_id.length(),
                m_Opts.output_stream) < data_or_blob_id.length())
            goto Error;
    }

    return 0;

Error:
    fprintf(stderr, PROGRAM_NAME ": error while writing job data.\n");
    return 3;
}

int CGridCommandLineInterfaceApp::Cmd_GetJobInput()
{
    SetUp_GridClient();

    CNetScheduleJob job;
    job.job_id = m_Opts.id;

    if (m_NetScheduleAPI.GetJobDetails(job) == CNetScheduleAPI::eJobNotFound) {
        fprintf(stderr, PROGRAM_NAME ": job %s has expired.\n", job.job_id.c_str());
        return 3;
    }

    return DumpJobInputOutput(job.input);
}

int CGridCommandLineInterfaceApp::Cmd_GetJobOutput()
{
    SetUp_GridClient();

    CNetScheduleJob job;
    job.job_id = m_Opts.id;
    CNetScheduleAPI::EJobStatus status = m_NetScheduleAPI.GetJobDetails(job);

    switch (status) {
    case CNetScheduleAPI::eDone:
    case CNetScheduleAPI::eReading:
    case CNetScheduleAPI::eConfirmed:
    case CNetScheduleAPI::eReadFailed:
        break;

    default:
        fprintf(stderr, PROGRAM_NAME
            ": cannot retrieve job output for job status %s.\n",
            CNetScheduleAPI::StatusToString(status).c_str());
        return 3;
    }

    return DumpJobInputOutput(job.output);
}

int CGridCommandLineInterfaceApp::Cmd_ReadJobs()
{
    SetUp_NetScheduleCmd(eNetScheduleSubmitter);

    std::vector<std::string> job_ids;

    if (!IsOptionSet(eConfirmRead) && !IsOptionSet(eRollbackRead)) {
        if (!IsOptionSet(eLimit)) {
            fprintf(stderr, PROGRAM_NAME " " READJOBS_COMMAND
                ": option '--" LIMIT_OPTION "' is required.\n");
            return 2;
        }

        std::string batch_id;

        if (m_NetScheduleSubmitter.Read(batch_id,
                job_ids, m_Opts.limit, m_Opts.timeout)) {
            printf("%s\n", batch_id.c_str());

            ITERATE(std::vector<std::string>, job_id, job_ids) {
                fprintf(m_Opts.output_stream, "%s\n", job_id->c_str());
            }
        }
    } else {
        char job_id[1024];

        while (fgets(job_id, sizeof(job_id), m_Opts.input_stream) != NULL)
            job_ids.push_back(job_id);

        if (IsOptionSet(eConfirmRead))
            m_NetScheduleSubmitter.ReadConfirm(
                m_Opts.reservation_token, job_ids);
        else
            m_NetScheduleSubmitter.ReadRollback(
                m_Opts.reservation_token, job_ids);
    }

    return 0;
}

int CGridCommandLineInterfaceApp::Cmd_CancelJob()
{
    SetUp_NetScheduleCmd(eNetScheduleAPI);

    m_NetScheduleAPI.GetSubmitter().CancelJob(m_Opts.id);

    return 0;
}

int CGridCommandLineInterfaceApp::Cmd_Kill()
{
    SetUp_NetScheduleCmd(eNetScheduleAdmin);

    if (IsOptionSet(eAllJobs))
        m_NetScheduleAdmin.DropQueue();
    else {
        if (!IsOptionSet(eOptionalID)) {
            fprintf(stderr, "kill: job ID required\n");
            return 1;
        }
        m_NetScheduleAdmin.DropJob(m_Opts.id);
    }

    return 0;
}

int CGridCommandLineInterfaceApp::Cmd_RequestJob()
{
    return 0;
}

int CGridCommandLineInterfaceApp::Cmd_CommitJob()
{
    return 0;
}

int CGridCommandLineInterfaceApp::Cmd_ReturnJob()
{
    SetUp_NetScheduleCmd(eNetScheduleAPI);

    m_NetScheduleAPI.GetExecuter().ReturnJob(m_Opts.id);

    return 0;
}

int CGridCommandLineInterfaceApp::Cmd_UpdateJob()
{
    SetUp_NetScheduleCmd(eNetScheduleAdmin);

    if (IsOptionSet(eForceReschedule))
        m_NetScheduleAdmin.ForceReschedule(m_Opts.id);
    else if (IsOptionSet(eExtendLifetime))
        m_NetScheduleAPI.GetExecuter().JobDelayExpiration(
            m_Opts.id, (unsigned) m_Opts.extend_lifetime_by);

    if (IsOptionSet(eProgressMessage)) {
        CNetScheduleJob job;
        job.job_id = m_Opts.id;
        job.progress_msg = m_Opts.progress_message;
        m_NetScheduleAPI.GetExecuter().PutProgressMsg(job);
    }

    return 0;
}

int CGridCommandLineInterfaceApp::Cmd_NetScheduleQuery()
{
    SetUp_NetScheduleCmd(eNetScheduleAdmin);

    NStr::TruncateSpacesInPlace(m_Opts.query, NStr::eTrunc_Begin);

    if (IsOptionSet(eCount))
        NcbiCout << m_NetScheduleAdmin.Count(m_Opts.query) << NcbiEndl;
    else if (IsOptionSet(eQueryField))
        m_NetScheduleAdmin.Query(m_Opts.query, m_Opts.query_fields, NcbiCout);
    else if (NStr::StartsWith(m_Opts.query, "SELECT", NStr::eNocase))
        m_NetScheduleAdmin.Select(m_Opts.query, NcbiCout);
    else {
        CNetScheduleKeys keys;
        m_NetScheduleAdmin.RetrieveKeys(m_Opts.query, keys);
        ITERATE(CNetScheduleKeys, it, keys) {
            NcbiCout << string(*it) << NcbiEndl;
        }
    }

    return 0;
}

int CGridCommandLineInterfaceApp::Cmd_QueueInfo()
{
    SetUp_NetScheduleCmd(eNetScheduleAdmin);

    m_NetScheduleAdmin.PrintQueueInfo(NcbiCout, m_Opts.id);

    return 0;
}

int CGridCommandLineInterfaceApp::Cmd_DumpQueue()
{
    SetUp_NetScheduleCmd(eNetScheduleAdmin);

    if (!IsOptionSet(eSelectByStatus))
        m_NetScheduleAdmin.DumpQueue(NcbiCout);
    else
        m_NetScheduleAdmin.PrintQueue(NcbiCout, m_Opts.job_status);

    return 0;
}

int CGridCommandLineInterfaceApp::Cmd_CreateQueue()
{
    SetUp_NetScheduleCmd(eNetScheduleAdmin);

    m_NetScheduleAdmin.CreateQueue(m_Opts.id, m_Opts.queue,
        m_Opts.queue_description);

    return 0;
}

int CGridCommandLineInterfaceApp::Cmd_GetQueueList()
{
    SetUp_NetScheduleCmd(eNetScheduleAdmin);

    CNetScheduleAdmin::TQueueList queues;

    m_NetScheduleAdmin.GetQueueList(queues);

    typedef set<string> TServerSet;
    typedef map<string, TServerSet> TQueueRegister;

    TQueueRegister queue_register;

    ITERATE (CNetScheduleAdmin::TQueueList, it, queues) {
        string server_address(g_NetService_gethostname(it->server.GetHost()));
        server_address += ':';
        server_address += NStr::UIntToString(it->server.GetPort());

        ITERATE(std::list<std::string>, queue, it->queues) {
            queue_register[*queue].insert(server_address);
        }
    }

    ITERATE(TQueueRegister, it, queue_register) {
        NcbiCout << it->first;
        if (it->second.size() != queues.size()) {
            const char* sep = " (limited to ";
            ITERATE(TServerSet, server, it->second) {
                NcbiCout << sep << *server;
                sep = ", ";
            }
            NcbiCout << ")";
        }
        NcbiCout << NcbiEndl;
    }

    return 0;
}

int CGridCommandLineInterfaceApp::Cmd_DeleteQueue()
{
    SetUp_NetScheduleCmd(eNetScheduleAdmin);

    m_NetScheduleAdmin.DeleteQueue(m_Opts.id);

    return 0;
}
