#ifndef GRID_CLI__HPP
#define GRID_CLI__HPP

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
 * Authors:  Dmitry Kazimirov
 *
 */

/// @file grid_cli.hpp
/// Declarations of command line interface arguments and handlers.
///

#include <corelib/ncbiapp.hpp>

#include <connect/services/grid_client.hpp>
#include <connect/services/neticache_client.hpp>


#define PROGRAM_NAME "grid_cli"
#define PROGRAM_VERSION "1.0.0"

#define INPUT_OPTION "input"
#define INPUT_FILE_OPTION "input-file"
#define OUTPUT_FILE_OPTION "output-file"
#define QUEUE_OPTION "queue"
#define BATCH_OPTION "batch"
#define AFFINITY_OPTION "affinity"
#define JOB_TAG_OPTION "job-tag"
#define JOB_OUTPUT_OPTION "job-output"
#define GET_NEXT_JOB_OPTION "get-next-job"
#define LIMIT_OPTION "limit"
#define TIMEOUT_OPTION "timeout"
#define CONFIRM_READ_OPTION "confirm-read"
#define ROLLBACK_READ_OPTION "rollback-read"
#define FAIL_READ_OPTION "fail-read"
#define JOB_ID_OPTION "job-id"
#define QUERY_FIELD_OPTION "query-field"
#define WNODE_PORT_OPTION "wnode-port"
#define WAIT_TIMEOUT_OPTION "wait-timeout"
#define FAIL_JOB_OPTION "fail-job"
#define NOW_OPTION "now"
#define DIE_OPTION "die"

#define READJOBS_COMMAND "readjobs"
#define REQUESTJOB_COMMAND "requestjob"

BEGIN_NCBI_SCOPE

enum EOption {
    eUntypedArg,
    eOptionalID,
    eID,
    eAuth,
    eInput,
    eInputFile,
    eOutputFile,
    eOutputFormat,
    eNetCache,
    eCache,
    ePassword,
    eOffset,
    eSize,
    eTTL,
    eEnableMirroring,
    eNetSchedule,
    eQueue,
    eBatch,
    eAffinity,
    eJobTag,
    eExclusiveJob,
    eJobOutput,
    eReturnCode,
    eGetNextJob,
    eLimit,
    eTimeout,
    eConfirmRead,
    eRollbackRead,
    eFailRead,
    eErrorMessage,
    eJobId,
    eWorkerNodes,
    eActiveJobCount,
    eJobsByAffinity,
    eJobsByStatus,
    eQuery,
    eCount,
    eQueryField,
    eSelectByStatus,
    eBrief,
    eStatusOnly,
    eProgressMessageOnly,
    eDeferExpiration,
    eForceReschedule,
    eExtendLifetime,
    eProgressMessage,
    eAllJobs,
    eRegisterWNode,
    eUnregisterWNode,
    eWNodePort,
    eWNodeGUID,
    eWaitTimeout,
    eFailJob,
    eQueueArg,
    eModelQueue,
    eQueueDescription,
    eNow,
    eDie,
    eCompatMode,
    eTotalNumberOfOptions
};

#define OPTION_ACCEPTED 1
#define OPTION_SET 2
#define OPTION_N(number) (1 << number)

class CGridCommandLineInterfaceApp : public CNcbiApplication
{
public:
    CGridCommandLineInterfaceApp(int argc, const char* argv[]);

    virtual string GetProgramVersion() const;

    virtual int Run();

    virtual ~CGridCommandLineInterfaceApp();

private:
    int m_ArgC;
    const char* const* m_ArgV;

    struct SOptions {
        string id;
        string auth;
        string nc_service;
        string cache_name;
        string password;
        size_t offset;
        size_t size;
        unsigned ttl;
        string ns_service;
        string queue;
        string affinity;
        CNetScheduleAPI::TJobTags job_tags;
        string job_output;
        int return_code;
        unsigned batch_size;
        unsigned limit;
        unsigned timeout;
        string reservation_token;
        std::vector<std::string> job_ids;
        string query;
        vector<string> query_fields;
        CNetScheduleAPI::EJobStatus job_status;
        time_t extend_lifetime_by;
        unsigned short wnode_port;
        string wnode_guid;
        string progress_message;
        string queue_description;
        string error_message;
        string input;
        FILE* input_stream;
        FILE* output_stream;

        struct SICacheBlobKey {
            string key;
            int version;
            string subkey;

            SICacheBlobKey() : version(0) {}
        } icache_key;

        char option_flags[eTotalNumberOfOptions];

        SOptions() : offset(0), size(0), ttl(0), return_code(0),
            batch_size(0), limit(0), timeout(0), extend_lifetime_by(0),
            wnode_port(0),
            input_stream(NULL), output_stream(NULL)
        {
            memset(option_flags, 0, eTotalNumberOfOptions);
        }
    } m_Opts;

private:
    bool IsOptionAcceptedButNotSet(EOption option)
    {
        return m_Opts.option_flags[option] == OPTION_ACCEPTED;
    }

    bool IsOptionSet(EOption option)
    {
        return m_Opts.option_flags[option] == OPTION_SET;
    }

    int IsOptionSet(EOption option, int mask)
    {
        return m_Opts.option_flags[option] == OPTION_SET ? mask : 0;
    }

    CNetCacheAPI m_NetCacheAPI;
    CNetCacheAdmin m_NetCacheAdmin;
    CNetICacheClient m_NetICacheClient;
    CNetScheduleAPI m_NetScheduleAPI;
    CNetScheduleAdmin m_NetScheduleAdmin;
    CNetScheduleSubmitter m_NetScheduleSubmitter;
    CNetScheduleExecuter m_NetScheduleExecutor;
    auto_ptr<CGridClient> m_GridClient;

// NetCache commands.
public:
    int Cmd_GetBlob();
    int Cmd_PutBlob();
    int Cmd_BlobInfo();
    int Cmd_RemoveBlob();
    int Cmd_ReinitNetCache();

// NetSchedule commands.
public:
    int Cmd_JobInfo();
    int Cmd_SubmitJob();
    int Cmd_GetJobInput();
    int Cmd_GetJobOutput();
    int Cmd_ReadJobs();
    int Cmd_CancelJob();
    int Cmd_Kill();
    int Cmd_RegWNode();
    int Cmd_RequestJob();
    int Cmd_CommitJob();
    int Cmd_ReturnJob();
    int Cmd_UpdateJob();
    int Cmd_NetScheduleQuery();
    int Cmd_QueueInfo();
    int Cmd_DumpQueue();
    int Cmd_CreateQueue();
    int Cmd_GetQueueList();
    int Cmd_DeleteQueue();

// Miscellaneous commands.
public:
    int Cmd_WhatIs();
    int Cmd_ServerInfo();
    int Cmd_Stats();
    int Cmd_Health();
    int Cmd_GetConf();
    int Cmd_Reconf();
    int Cmd_Shutdown();

// Implementation details.
private:
    static void PrintLine(const string& line);
    enum EAPIClass {
        eUnknownAPI,
        eNetCacheAPI,
        eNetICacheClient,
        eNetCacheAdmin,
        eNetScheduleAPI,
        eNetScheduleAdmin,
        eNetScheduleSubmitter,
        eNetScheduleExecutor,
        eGridClient
    };
    EAPIClass SetUp_AdminCmd();
    void SetUp_NetCacheCmd(EAPIClass api_class);
    void PrintBlobMeta(const CNetCacheKey& key);
    void ParseICacheKey(bool permit_empty_version = false,
        bool* version_is_defined = NULL);
    void PrintICacheServerUsed();
    void SetUp_NetScheduleCmd(EAPIClass api_class);
    void SetUp_GridClient();
    void PrintJobMeta(const CNetScheduleKey& key);
    static void PrintStorageType(const string& data, const char* prefix);
    static bool MatchPrefixAndPrintStorageTypeAndData(const string& line,
        const char* prefix, size_t prefix_length, const char* new_prefix);
    int DumpJobInputOutput(const string& data_or_blob_id);
    int PrintJobAttrsAndDumpInput(const CNetScheduleJob& job);
};

END_NCBI_SCOPE

#endif // GRID_CLI__HPP
