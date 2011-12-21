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

#include <ctype.h>

USING_NCBI_SCOPE;

#define MAX_VISIBLE_DATA_LENGTH 50

void CGridCommandLineInterfaceApp::SetUp_NetScheduleCmd(
    CGridCommandLineInterfaceApp::EAPIClass api_class)
{
    string queue(!m_Opts.queue.empty() ? m_Opts.queue : "noname");

    if (!IsOptionSet(eID))
        m_NetScheduleAPI = CNetScheduleAPI(m_Opts.ns_service,
            m_Opts.auth, queue);
    else if (!m_Opts.ns_service.empty()) {
        string host, port;

        if (!NStr::SplitInTwo(m_Opts.ns_service, ":", host, port)) {
            NCBI_THROW(CArgException, eInvalidArg,
                "When job ID is given, '--netschedule' "
                "must be a host:port server address.");
        }

        m_NetScheduleAPI = CNetScheduleAPI(m_Opts.ns_service,
            m_Opts.auth, queue);
        m_NetScheduleAPI.GetService().StickToServer(host,
            NStr::StringToInt(port));
    } else {
        CNetScheduleKey key(m_Opts.id);
        key.host.push_back(':');
        key.host.append(NStr::UIntToString(key.port));
        m_NetScheduleAPI = CNetScheduleAPI(key.host, m_Opts.auth, queue);
    }

    if (IsOptionSet(eCompatMode)) {
        m_NetScheduleAPI.UseOldStyleAuth();
        if (IsOptionSet(eWorkerNode))
            m_NetScheduleAPI.EnableWorkerNodeCompatMode();
    }

    switch (api_class) {
    case eNetScheduleAPI:
        break;
    case eNetScheduleAdmin:
    case eWorkerNodeAdmin:
        m_NetScheduleAdmin = m_NetScheduleAPI.GetAdmin();
        break;
    case eNetScheduleSubmitter:
        m_NetScheduleSubmitter = m_NetScheduleAPI.GetSubmitter();
        break;
    case eNetScheduleExecutor:
        m_NetScheduleExecutor =
            m_NetScheduleAPI.GetExecuter(m_Opts.wnode_port, m_Opts.wnode_guid);
        break;
    default:
        _ASSERT(0);
        break;
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

class CAttrListParser
{
public:
    enum ENextAttributeType {
        eAttributeWithValue,
        eStandAloneAttribute,
        eNoMoreAttributes
    };

    void Reset(const char* position) {m_Position = position;}

    ENextAttributeType NextAttribute(CTempString& attr_name,
        string& attr_value);

private:
    const char* m_Position;
};

CAttrListParser::ENextAttributeType CAttrListParser::NextAttribute(
    CTempString& attr_name, string& attr_value)
{
    while (isspace(*m_Position))
        ++m_Position;

    if (*m_Position == '\0')
        return eNoMoreAttributes;

    const char* start_pos = m_Position;

    for (;;)
        if (*m_Position == '=') {
            attr_name.assign(start_pos, m_Position - start_pos);
            break;
        } else if (isspace(*m_Position)) {
            attr_name.assign(start_pos, m_Position - start_pos);
            while (isspace(*++m_Position))
                ;
            if (*m_Position == '=')
                break;
            else
                return eStandAloneAttribute;
        } else if (*++m_Position == '\0') {
            attr_name.assign(start_pos, m_Position - start_pos);
            return eStandAloneAttribute;
        }

    // Skip the equals sign and the spaces that may follow it.
    while (isspace(*++m_Position))
        ;

    start_pos = m_Position;
    char quotation_mark;

    switch (*m_Position) {
    case '\0':
        NCBI_THROW_FMT(CArgException, eInvalidArg, PROGRAM_NAME
            ": empty attribute value must be specified as " <<
                attr_name << "=\"\"");
    case '\'':
    case '"':
        quotation_mark = *start_pos++;
        do {
            if (*++m_Position == '\\' && *++m_Position != '\0')
                ++m_Position;
            if (*m_Position == '\0') {
                NCBI_THROW(CArgException, eInvalidArg, PROGRAM_NAME
                    ": unterminated attribute value");
            }
        } while (*m_Position != quotation_mark);
        attr_value = NStr::ParseEscapes(
            CTempString(start_pos, m_Position - start_pos));
        ++m_Position;
        break;
    default:
        while (*++m_Position != '\0' && !isspace(*m_Position))
            ;
        attr_value = NStr::ParseEscapes(
            CTempString(start_pos, m_Position - start_pos));
    }

    return eAttributeWithValue;
}

#define EVENT_WORD "event"
#define EVENT_WORD_LEN (sizeof(EVENT_WORD) - 1)

bool CGridCommandLineInterfaceApp::ParseAndPrintJobEvents(const string& line)
{
    static const CTempString event_word(EVENT_WORD, EVENT_WORD_LEN);
    if (!NStr::StartsWith(line, event_word))
        return false;

    const char* event_info = line.c_str() + EVENT_WORD_LEN;
    if (*event_info < '0' || *event_info > '9')
        return false;

    while (*++event_info >= '0' && *event_info <= '9')
        ;
    printf("[%.*s]\n", int(event_info - line.data()), line.data());
    if (*event_info != ':')
        return false;
    ++event_info;

    try {
        CAttrListParser attr_parser;
        attr_parser.Reset(event_info);
        CTempString attr_name;
        string attr_value;
        CAttrListParser::ENextAttributeType next_attr_type;
        for (;;)
            if ((next_attr_type = attr_parser.NextAttribute(attr_name,
                    attr_value)) == CAttrListParser::eAttributeWithValue)
                printf("    %.*s: %s\n", int(attr_name.length()),
                    attr_name.data(), attr_value.c_str());
            else if (next_attr_type == CAttrListParser::eNoMoreAttributes)
                break;
            else // CAttrListParser::eStandAloneAttribute
                printf("    %.*s\n", int(attr_name.length()), attr_name.data());
    }
    catch (CArgException&) {
        return false;
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
                    sizeof(s_OutputPrefix) - 1, "output-") &&
                !ParseAndPrintJobEvents(line))
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

class CBatchSubmitAttrParser
{
public:
    CBatchSubmitAttrParser(FILE* input_stream) :
        m_InputStream(input_stream),
        m_LineNumber(0)
    {
    }
    bool NextLine();
    bool NextAttribute();
    EOption GetAttributeType() const {return m_JobAttribute;}
    const string& GetAttributeValue() const {return m_JobAttributeValue;}
    size_t GetLineNumber() const {return m_LineNumber;}

private:
    FILE* m_InputStream;
    size_t m_LineNumber;
    string m_Line;
    CAttrListParser m_AttrParser;
    EOption m_JobAttribute;
    string m_JobAttributeValue;
};

bool CBatchSubmitAttrParser::NextLine()
{
    if (m_InputStream == NULL)
        return false;

    ++m_LineNumber;
    m_Line.resize(0);

    char buffer[64 * 1024];
    size_t bytes_read;

    while (fgets(buffer, sizeof(buffer), m_InputStream) != NULL)
        if ((bytes_read = strlen(buffer)) > 0) {
            if (buffer[bytes_read - 1] != '\n')
                m_Line.append(buffer, bytes_read);
            else {
                m_Line.append(buffer, bytes_read - 1);
                m_AttrParser.Reset(m_Line.c_str());
                return true;
            }
        }

    m_InputStream = NULL;

    if (m_Line.empty())
        return false;
    else {
        m_AttrParser.Reset(m_Line.c_str());
        return true;
    }
}

bool CBatchSubmitAttrParser::NextAttribute()
{
    m_JobAttribute = eUntypedArg;

#define ATTR_CHECK_SET(name, type) \
    if (attr_name.length() == sizeof(name) - 1 && \
            memcmp(attr_name.data(), name, sizeof(name) - 1) == 0) { \
        m_JobAttribute = type; \
        break; \
    }

    CTempString attr_name;

    CAttrListParser::ENextAttributeType next_attr_type =
        m_AttrParser.NextAttribute(attr_name, m_JobAttributeValue);

    if (next_attr_type == CAttrListParser::eNoMoreAttributes)
        return false;

    switch (attr_name[0]) {
    case 'i':
        ATTR_CHECK_SET("input", eInput);
        break;
    case 'a':
        ATTR_CHECK_SET("affinity", eAffinity);
        break;
    case 't':
        ATTR_CHECK_SET("tag", eJobTag);
        break;
    case 'e':
        ATTR_CHECK_SET("exclusive", eExclusiveJob);
        break;
    case 'p':
        ATTR_CHECK_SET("progress_message", eProgressMessage);
    }

#define AT_POS(pos) " at line " << m_LineNumber << \
    ", column " << (pos - m_Line.data() + 1)

    switch (m_JobAttribute) {
    case eUntypedArg:
        NCBI_THROW_FMT(CArgException, eInvalidArg, PROGRAM_NAME
            ": unknown attribute " << attr_name <<
                AT_POS(attr_name.data()));

    case eExclusiveJob:
        break;

    default:
        if (next_attr_type != CAttrListParser::eAttributeWithValue) {
            NCBI_THROW_FMT(CArgException, eInvalidArg, PROGRAM_NAME
                ": attribute " << attr_name <<
                    " requires a value" << AT_POS(attr_name.data()));
        }
    }

    return true;
}

int CGridCommandLineInterfaceApp::Cmd_SubmitJob()
{
    SetUp_GridClient();

    if (IsOptionSet(eBatch)) {
        CBatchSubmitAttrParser attr_parser(m_Opts.input_stream);

        CTempString job_tag_name;
        CTempString job_tag_value;

        if (m_Opts.batch_size <= 1) {
            while (attr_parser.NextLine()) {
                CGridJobSubmitter& submitter(m_GridClient->GetJobSubmitter());
                CNetScheduleAPI::TJobTags job_tags(m_Opts.job_tags);
                bool input_set = false;
                while (attr_parser.NextAttribute()) {
                    const string& attr_value(attr_parser.GetAttributeValue());
                    switch (attr_parser.GetAttributeType()) {
                    case eInput:
                        input_set = true;
                        {
                            CNcbiOstream& job_input_stream(
                                submitter.GetOStream());
                            job_input_stream.write(attr_value.data(),
                                attr_value.length());
                            if (job_input_stream.bad())
                                goto ErrorExit;
                        }
                        break;
                    case eAffinity:
                        submitter.SetJobAffinity(attr_value);
                        break;
                    case eJobTag:
                        NStr::SplitInTwo(attr_value, "=",
                            job_tag_name, job_tag_value);
                        job_tags.push_back(CNetScheduleAPI::TJobTag(
                            job_tag_name, job_tag_value));
                        break;
                    case eExclusiveJob:
                        submitter.SetJobMask(CNetScheduleAPI::eExclusiveJob);
                        break;
                    case eProgressMessage:
                        submitter.SetProgressMessage(attr_value);
                        break;
                    default:
                        _ASSERT(0);
                        break;
                    }
                }
                if (!input_set) {
                    NCBI_THROW_FMT(CArgException, eInvalidArg, PROGRAM_NAME
                        ": attribute \"input\" is required at line " <<
                            attr_parser.GetLineNumber());
                }
                if (!job_tags.empty())
                    submitter.SetJobTags(job_tags);
                fprintf(m_Opts.output_stream,
                    "%s\n", submitter.Submit(m_Opts.affinity).c_str());
            }
        } else {
            CGridJobBatchSubmitter& batch_submitter(
                m_GridClient->GetJobBatchSubmitter());
            unsigned remaining_batch_size = m_Opts.batch_size;

            while (attr_parser.NextLine()) {
                if (remaining_batch_size == 0) {
                    batch_submitter.Submit();
                    const vector<CNetScheduleJob>& jobs =
                        batch_submitter.GetBatch();
                    ITERATE(vector<CNetScheduleJob>, it, jobs)
                        fprintf(m_Opts.output_stream,
                            "%s\n", it->job_id.c_str());
                    batch_submitter.Reset();
                    remaining_batch_size = m_Opts.batch_size;
                }
                batch_submitter.PrepareNextJob();
                CNetScheduleAPI::TJobTags job_tags(m_Opts.job_tags);
                bool input_set = false;
                while (attr_parser.NextAttribute()) {
                    const string& attr_value(attr_parser.GetAttributeValue());
                    switch (attr_parser.GetAttributeType()) {
                    case eInput:
                        input_set = true;
                        {
                            CNcbiOstream& job_input_stream(
                                batch_submitter.GetOStream());
                            job_input_stream.write(attr_value.data(),
                                attr_value.length());
                            if (job_input_stream.bad())
                                goto ErrorExit;
                        }
                        break;
                    case eAffinity:
                        batch_submitter.SetJobAffinity(attr_value);
                        break;
                    case eJobTag:
                        NStr::SplitInTwo(attr_value, "=",
                            job_tag_name, job_tag_value);
                        job_tags.push_back(CNetScheduleAPI::TJobTag(
                            job_tag_name, job_tag_value));
                        break;
                    case eExclusiveJob:
                        batch_submitter.SetJobMask(
                            CNetScheduleAPI::eExclusiveJob);
                        break;
                    case eProgressMessage:
                        batch_submitter.SetProgressMessage(attr_value);
                        break;
                    default:
                        _ASSERT(0);
                        break;
                    }
                }
                if (!input_set) {
                    NCBI_THROW_FMT(CArgException, eInvalidArg, PROGRAM_NAME
                        ": attribute \"input\" is required at line " <<
                            attr_parser.GetLineNumber());
                }
                if (!job_tags.empty())
                    batch_submitter.SetJobTags(job_tags);
                --remaining_batch_size;
            }
            if (remaining_batch_size < m_Opts.batch_size) {
                batch_submitter.Submit();
                const vector<CNetScheduleJob>& jobs =
                    batch_submitter.GetBatch();
                ITERATE(vector<CNetScheduleJob>, it, jobs)
                    fprintf(m_Opts.output_stream,
                    "%s\n", it->job_id.c_str());
                batch_submitter.Reset();
            }
        }
    } else {
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

        if (IsOptionSet(eJobTag))
            submitter.SetJobTags(m_Opts.job_tags);

        if (IsOptionSet(eExclusiveJob))
            submitter.SetJobMask(CNetScheduleAPI::eExclusiveJob);

        if (IsOptionSet(eProgressMessage))
            submitter.SetProgressMessage(m_Opts.progress_message);

        fprintf(m_Opts.output_stream,
            "%s\n", submitter.Submit(m_Opts.affinity).c_str());
    }

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

int CGridCommandLineInterfaceApp::PrintJobAttrsAndDumpInput(
    const CNetScheduleJob& job)
{
    printf("%s\n", job.job_id.c_str());
    if (!job.affinity.empty()) {
        string affinity(NStr::PrintableString(job.affinity));
        printf(job.mask & CNetScheduleAPI::eExclusiveJob ?
            "affinity=\"%s\" exclusive\n" : "affinity=\"%s\"\n",
            affinity.c_str());
    } else
        printf(job.mask & CNetScheduleAPI::eExclusiveJob ?
            "exclusive\n" : "\n");
    return DumpJobInputOutput(job.input);
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

    if (!IsOptionSet(eConfirmRead) && !IsOptionSet(eFailRead) &&
            !IsOptionSet(eRollbackRead)) {
        if (!IsOptionSet(eLimit)) {
            fprintf(stderr, PROGRAM_NAME " " READJOBS_COMMAND
                ": option '--" LIMIT_OPTION "' is required.\n");
            return 2;
        }

        std::string batch_id;

        if (m_NetScheduleSubmitter.Read(batch_id,
                m_Opts.job_ids, m_Opts.limit, m_Opts.timeout)) {
            printf("%s\n", batch_id.c_str());

            ITERATE(std::vector<std::string>, job_id, m_Opts.job_ids) {
                fprintf(m_Opts.output_stream, "%s\n", job_id->c_str());
            }
        }
    } else {
        if (!IsOptionSet(eJobId)) {
            char job_id[1024];

            while (fgets(job_id, sizeof(job_id), m_Opts.input_stream) != NULL)
                m_Opts.job_ids.push_back(job_id);
        }

        if (IsOptionSet(eConfirmRead))
            m_NetScheduleSubmitter.ReadConfirm(
                m_Opts.reservation_token, m_Opts.job_ids);
        else if (IsOptionSet(eFailRead))
            m_NetScheduleSubmitter.ReadFail(
                m_Opts.reservation_token, m_Opts.job_ids, m_Opts.error_message);
        else
            m_NetScheduleSubmitter.ReadRollback(
                m_Opts.reservation_token, m_Opts.job_ids);
    }

    return 0;
}

int CGridCommandLineInterfaceApp::Cmd_CancelJob()
{
    SetUp_NetScheduleCmd(eNetScheduleAPI);

    m_NetScheduleAPI.GetSubmitter().CancelJob(m_Opts.id);

    return 0;
}

int CGridCommandLineInterfaceApp::Cmd_RegWNode()
{
    SetUp_NetScheduleCmd(eNetScheduleExecutor);

    switch (IsOptionSet(eRegisterWNode, OPTION_N(0)) |
        IsOptionSet(eUnregisterWNode, OPTION_N(1))) {
    case OPTION_N(0): // eRegisterWNode
        printf("%s\n", m_NetScheduleExecutor.GetGUID().c_str());
        break;
    case OPTION_N(1): // eUnregisterWNode
        m_NetScheduleExecutor.UnRegisterClient();
        break;

    default:
        fprintf(stderr, PROGRAM_NAME
            ": exactly one registration option is required.\n");
        return 2;
    }

    return 0;
}

int CGridCommandLineInterfaceApp::Cmd_RequestJob()
{
    SetUp_NetScheduleCmd(eNetScheduleExecutor);

    CNetScheduleJob job;

    if (!IsOptionSet(eWaitTimeout)) {
        if (m_NetScheduleExecutor.GetJob(job, m_Opts.affinity))
            return PrintJobAttrsAndDumpInput(job);
    } else {
        if (!IsOptionSet(eWNodePort)) {
            fprintf(stderr, PROGRAM_NAME " " REQUESTJOB_COMMAND
                ": option '--" WAIT_TIMEOUT_OPTION "' requires '--"
                WNODE_PORT_OPTION "' to be specified as well\n");
            return 2;
        }
        if (m_NetScheduleExecutor.WaitJob(job,
                m_Opts.timeout, m_Opts.affinity))
            return PrintJobAttrsAndDumpInput(job);
    }

    return 0;
}

int CGridCommandLineInterfaceApp::Cmd_CommitJob()
{
    SetUp_NetScheduleCmd(eNetScheduleExecutor);

    CNetScheduleJob job;

    job.job_id = m_Opts.id;
    job.ret_code = m_Opts.return_code;

    auto_ptr<IEmbeddedStreamWriter> writer(new CStringOrBlobStorageWriter(
        m_NetScheduleAPI.GetServerParams().max_output_size,
        m_NetCacheAPI, job.output));

    char buffer[16 * 1024];
    size_t bytes_read;

    if (!IsOptionSet(eJobOutput))
        while ((bytes_read = fread(buffer, 1,
                sizeof(buffer), m_Opts.input_stream)) > 0) {
            if (writer->Write(buffer, bytes_read) != eRW_Success)
                goto ErrorExit;
            if (feof(m_Opts.input_stream))
                break;
        }
    else
        if (writer->Write(m_Opts.job_output.data(),
                m_Opts.job_output.length()) != eRW_Success)
            goto ErrorExit;

    if (!IsOptionSet(eFailJob)) {
        if (!IsOptionSet(eGetNextJob))
            m_NetScheduleExecutor.PutResult(job);
        else {
            CNetScheduleJob new_job;

            if (m_NetScheduleExecutor.PutResultGetJob(job,
                    new_job, m_Opts.affinity))
                return PrintJobAttrsAndDumpInput(new_job);
        }
    } else {
        job.error_msg = m_Opts.error_message;
        m_NetScheduleExecutor.PutFailure(job);
        if (IsOptionSet(eGetNextJob) &&
                m_NetScheduleExecutor.GetJob(job, m_Opts.affinity))
            return PrintJobAttrsAndDumpInput(job);
    }

    return 0;

ErrorExit:
    fprintf(stderr, PROGRAM_NAME ": error while submitting job output.\n");
    return 3;
}

int CGridCommandLineInterfaceApp::Cmd_ReturnJob()
{
    SetUp_NetScheduleCmd(eNetScheduleExecutor);

    m_NetScheduleExecutor.ReturnJob(m_Opts.id);

    return 0;
}

int CGridCommandLineInterfaceApp::Cmd_UpdateJob()
{
    SetUp_NetScheduleCmd(eNetScheduleAdmin);

    if (IsOptionSet(eExtendLifetime))
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

    if (IsOptionSet(eDropJobs))
        m_NetScheduleAdmin.DropQueue();
    else
        m_NetScheduleAdmin.DeleteQueue(m_Opts.id);

    return 0;
}
