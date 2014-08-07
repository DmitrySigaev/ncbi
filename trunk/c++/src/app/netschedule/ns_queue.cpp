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
 *   NetSchedule queue structure and parameters
 */
#include <ncbi_pch.hpp>

#include "ns_queue.hpp"
#include "ns_queue_param_accessor.hpp"
#include "background_host.hpp"
#include "ns_util.hpp"
#include "ns_server.hpp"
#include "ns_precise_time.hpp"
#include "ns_rollback.hpp"
#include "queue_database.hpp"
#include "ns_handler.hpp"
#include "ns_ini_params.hpp"

#include <corelib/ncbi_system.hpp> // SleepMilliSec
#include <corelib/request_ctx.hpp>
#include <db/bdb/bdb_trans.hpp>
#include <util/qparse/query_parse.hpp>
#include <util/qparse/query_exec.hpp>
#include <util/qparse/query_exec_bv.hpp>
#include <util/bitset/bmalgo.h>


BEGIN_NCBI_SCOPE



//////////////////////////////////////////////////////////////////////////
CQueueEnumCursor::CQueueEnumCursor(CQueue *  queue, unsigned int  start_after)
  : CBDB_FileCursor(queue->m_QueueDbBlock->job_db)
{
    SetCondition(CBDB_FileCursor::eGT);
    From << start_after;
}


//////////////////////////////////////////////////////////////////////////
// CQueue


// Used together with m_SavedId. m_SavedId is saved in a DB and is used
// as to start from value for the restarted neschedule.
// s_ReserveDelta value is used to avoid to often DB updates
static const unsigned int       s_ReserveDelta = 10000;


CQueue::CQueue(CRequestExecutor&     executor,
               const string&         queue_name,
               TQueueKind            queue_kind,
               CNetScheduleServer *  server,
               CQueueDataBase &      qdb)
  :
    m_Server(server),
    m_QueueDB(qdb),
    m_RunTimeLine(NULL),
    m_Executor(executor),
    m_QueueName(queue_name),
    m_Kind(queue_kind),
    m_QueueDbBlock(0),
    m_TruncateAtDetach(false),

    m_LastId(0),
    m_SavedId(s_ReserveDelta),

    m_JobsToDeleteOps(0),
    m_ReadJobsOps(0),

    m_Timeout(default_timeout),
    m_RunTimeout(default_run_timeout),
    m_ReadTimeout(default_read_timeout),
    m_FailedRetries(default_failed_retries),
    m_MaxInputSize(kNetScheduleMaxDBDataSize),
    m_MaxOutputSize(kNetScheduleMaxDBDataSize),
    m_WNodeTimeout(default_wnode_timeout),
    m_PendingTimeout(default_pending_timeout),
    m_KeyGenerator(server->GetHost(), server->GetPort(), queue_name),
    m_Log(server->IsLog()),
    m_LogBatchEachJob(server->IsLogBatchEachJob()),
    m_RefuseSubmits(false),
    m_MaxAffinities(server->GetMaxAffinities()),
    m_AffinityHighMarkPercentage(server->GetAffinityHighMarkPercentage()),
    m_AffinityLowMarkPercentage(server->GetAffinityLowMarkPercentage()),
    m_AffinityHighRemoval(server->GetAffinityHighRemoval()),
    m_AffinityLowRemoval(server->GetAffinityLowRemoval()),
    m_AffinityDirtPercentage(server->GetAffinityDirtPercentage()),
    m_NotificationsList(qdb, server->GetNodeID(), queue_name),
    m_NotifHifreqInterval(default_notif_hifreq_interval), // 0.1 sec
    m_NotifHifreqPeriod(default_notif_hifreq_period),
    m_NotifLofreqMult(default_notif_lofreq_mult),
    m_DumpBufferSize(default_dump_buffer_size),
    m_DumpClientBufferSize(default_dump_client_buffer_size),
    m_DumpAffBufferSize(default_dump_aff_buffer_size),
    m_DumpGroupBufferSize(default_dump_group_buffer_size),
    m_ScrambleJobKeys(default_scramble_job_keys),
    m_PauseStatus(eNoPause),
    m_ClientRegistryTimeoutWorkerNode(
                                default_client_registry_timeout_worker_node),
    m_ClientRegistryMinWorkerNodes(default_client_registry_min_worker_nodes),
    m_ClientRegistryTimeoutAdmin(default_client_registry_timeout_admin),
    m_ClientRegistryMinAdmins(default_client_registry_min_admins),
    m_ClientRegistryTimeoutSubmitter(default_client_registry_timeout_submitter),
    m_ClientRegistryMinSubmitters(default_client_registry_min_submitters),
    m_ClientRegistryTimeoutReader(default_client_registry_timeout_reader),
    m_ClientRegistryMinReaders(default_client_registry_min_readers),
    m_ClientRegistryTimeoutUnknown(default_client_registry_timeout_unknown),
    m_ClientRegistryMinUnknowns(default_client_registry_min_unknowns)
{
    _ASSERT(!queue_name.empty());
}


CQueue::~CQueue()
{
    delete m_RunTimeLine;
    x_Detach();
}


void CQueue::Attach(SQueueDbBlock* block)
{
    x_Detach();
    m_QueueDbBlock = block;
    m_AffinityRegistry.Attach(&m_QueueDbBlock->aff_dict_db);
    m_GroupRegistry.Attach(&m_QueueDbBlock->group_dict_db);

    // Here we have a db, so we can read the counter value we should start from
    m_LastId = x_ReadStartFromCounter();
    m_SavedId = m_LastId + s_ReserveDelta;
    if (m_SavedId < m_LastId) {
        // Overflow
        m_LastId = 0;
        m_SavedId = s_ReserveDelta;
    }
    x_UpdateStartFromCounter();
}


class CTruncateRequest : public CStdRequest
{
public:
    CTruncateRequest(SQueueDbBlock* qdbblock)
        : m_QueueDbBlock(qdbblock)
    {}

    virtual void Process()
    {
        // See CQueue::Detach why we can do this without locking.
        CStopWatch      sw(CStopWatch::eStart);
        m_QueueDbBlock->Truncate();
        m_QueueDbBlock->allocated = false;
        LOG_POST(Message << Warning << "Clean up of db block "
                         << m_QueueDbBlock->pos << " complete, "
                         << sw.Elapsed() << " elapsed");
    }
private:
    SQueueDbBlock* m_QueueDbBlock;

};


void CQueue::x_Detach(void)
{
    m_AffinityRegistry.Detach();
    if (!m_QueueDbBlock)
        return;

    // We are here have synchronized access to m_QueueDbBlock without mutex
    // because we are here only when the last reference to CQueue is
    // destroyed. So as long m_QueueDbBlock->allocated is true it cannot
    // be allocated again and thus cannot be accessed as well.
    // As soon as we're done with detaching (here) or truncating (in separate
    // request, submitted for execution to the server thread pool), we can
    // set m_QueueDbBlock->allocated to false. Boolean write is atomic by
    // definition and test-and-set is executed from synchronized code in
    // CQueueDbBlockArray::Allocate.

    if (m_TruncateAtDetach && !m_Server->ShutdownRequested()) {
        CRef<CStdRequest> request(new CTruncateRequest(m_QueueDbBlock));
        m_Executor.SubmitRequest(request);
    } else {
        m_QueueDbBlock->allocated = false;
    }

    m_QueueDbBlock = 0;
    return;
}


void CQueue::SetParameters(const SQueueParameters &  params)
{
    // When modifying this, modify all places marked with PARAMETERS
    CFastMutexGuard     guard(m_ParamLock);

    m_Timeout    = params.timeout;
    m_RunTimeout = params.run_timeout;
    if (params.run_timeout && !m_RunTimeLine) {
        // One time only. Precision can not be reset.
        m_RunTimeLine =
            new CJobTimeLine(params.run_timeout_precision.Sec(), 0);
        m_RunTimeoutPrecision = params.run_timeout_precision;
    }

    m_ReadTimeout           = params.read_timeout;
    m_FailedRetries         = params.failed_retries;
    m_BlacklistTime         = params.blacklist_time;
    m_MaxInputSize          = params.max_input_size;
    m_MaxOutputSize         = params.max_output_size;
    m_WNodeTimeout          = params.wnode_timeout;
    m_PendingTimeout        = params.pending_timeout;
    m_MaxPendingWaitTimeout = CNSPreciseTime(params.max_pending_wait_timeout);
    m_NotifHifreqInterval   = params.notif_hifreq_interval;
    m_NotifHifreqPeriod     = params.notif_hifreq_period;
    m_NotifLofreqMult       = params.notif_lofreq_mult;
    m_HandicapTimeout       = CNSPreciseTime(params.notif_handicap);
    m_DumpBufferSize        = params.dump_buffer_size;
    m_DumpClientBufferSize  = params.dump_client_buffer_size;
    m_DumpAffBufferSize     = params.dump_aff_buffer_size;
    m_DumpGroupBufferSize   = params.dump_group_buffer_size;
    m_ScrambleJobKeys       = params.scramble_job_keys;
    m_LinkedSections        = params.linked_sections;

    m_ClientRegistryTimeoutWorkerNode =
                            params.client_registry_timeout_worker_node;
    m_ClientRegistryMinWorkerNodes = params.client_registry_min_worker_nodes;
    m_ClientRegistryTimeoutAdmin = params.client_registry_timeout_admin;
    m_ClientRegistryMinAdmins = params.client_registry_min_admins;
    m_ClientRegistryTimeoutSubmitter = params.client_registry_timeout_submitter;
    m_ClientRegistryMinSubmitters = params.client_registry_min_submitters;
    m_ClientRegistryTimeoutReader = params.client_registry_timeout_reader;
    m_ClientRegistryMinReaders = params.client_registry_min_readers;
    m_ClientRegistryTimeoutUnknown = params.client_registry_timeout_unknown;
    m_ClientRegistryMinUnknowns = params.client_registry_min_unknowns;

    m_ClientsRegistry.SetBlacklistTimeout(m_BlacklistTime);

    // program version control
    m_ProgramVersionList.Clear();
    if (!params.program_name.empty()) {
        m_ProgramVersionList.AddClientInfo(params.program_name);
    }
    m_SubmHosts.SetHosts(params.subm_hosts);
    m_WnodeHosts.SetHosts(params.wnode_hosts);
}


CQueue::TParameterList CQueue::GetParameters() const
{
    TParameterList          parameters;
    CQueueParamAccessor     qp(*this);
    unsigned                nParams = qp.GetNumParams();

    for (unsigned n = 0; n < nParams; ++n) {
        parameters.push_back(
            pair<string, string>(qp.GetParamName(n), qp.GetParamValue(n)));
    }
    return parameters;
}


void CQueue::GetMaxIOSizesAndLinkedSections(
                unsigned int &  max_input_size,
                unsigned int &  max_output_size,
                map< string, map<string, string> > & linked_sections) const
{
    CQueueParamAccessor     qp(*this);

    max_input_size = qp.GetMaxInputSize();
    max_output_size = qp.GetMaxOutputSize();
    GetLinkedSections(linked_sections);
}


void
CQueue::GetLinkedSections(map< string,
                               map<string, string> > &  linked_sections) const
{
    for (map<string, string>::const_iterator  k = m_LinkedSections.begin();
         k != m_LinkedSections.end(); ++k) {
        map< string, string >   values = m_QueueDB.GetLinkedSection(k->second);

        if (!values.empty())
            linked_sections[k->first] = values;
    }
}


// Used while loading the status matrix.
// The access to the DB is not protected here.
CNSPreciseTime  CQueue::x_GetSubmitTime(unsigned int  job_id)
{
    CBDB_FileCursor     events_cur(m_QueueDbBlock->events_db);
    events_cur.SetCondition(CBDB_FileCursor::eEQ);
    events_cur.From << job_id;

    // The submit event is always first
    if (events_cur.FetchFirst() == eBDB_Ok)
        return CNSPreciseTime(m_QueueDbBlock->events_db.timestamp_sec,
                              m_QueueDbBlock->events_db.timestamp_nsec);
    return CNSPreciseTime();
}


bool CQueue::x_NoMoreReadJobs(const string &  group)
{
    // Used only in the GetJobForReadingOrWait().
    // Operation lock has to be already taken.
    // Provides true if there are no more jobs for reading.
    vector<CNetScheduleAPI::EJobStatus>     from_state;

    from_state.push_back(CNetScheduleAPI::ePending);
    from_state.push_back(CNetScheduleAPI::eRunning);
    from_state.push_back(CNetScheduleAPI::eDone);
    from_state.push_back(CNetScheduleAPI::eFailed);
    from_state.push_back(CNetScheduleAPI::eCanceled);

    TNSBitVector    candidates = m_StatusTracker.GetJobs(from_state);
    // Remove those which have been read or in process of reading
    candidates -= m_ReadJobs;
    // Add those which are in a process of reading.
    // This needs to be done after '- m_ReadJobs' because that vector holds
    // both jobs which have been read and jobs which are in a process of
    // reading. When calculating 'no_more_jobs' the only already read jobs must
    // be excluded.
    candidates |= m_StatusTracker.GetJobs(CNetScheduleAPI::eReading);

    // Apply the group limit if so
    if (!group.empty())
        candidates &= m_GroupRegistry.GetJobs(group, false);

    return !candidates.any();
}


unsigned CQueue::LoadStatusMatrix()
{
    unsigned int        recs = 0;
    CFastMutexGuard     guard(m_OperationLock);

    // Load the known affinities and groups
    m_AffinityRegistry.LoadAffinityDictionary();
    m_GroupRegistry.LoadGroupDictionary();

    // scan the queue, load the state machine from DB
    CBDB_FileCursor     cur(m_QueueDbBlock->job_db);

    cur.InitMultiFetch(1024*1024);
    cur.SetCondition(CBDB_FileCursor::eGE);
    cur.From << 0;

    for (; cur.Fetch() == eBDB_Ok; ) {
        unsigned int    job_id = m_QueueDbBlock->job_db.id;
        unsigned int    group_id = m_QueueDbBlock->job_db.group_id;
        unsigned int    aff_id = m_QueueDbBlock->job_db.aff_id;
        CNSPreciseTime  last_touch =
                            CNSPreciseTime(
                                    m_QueueDbBlock->job_db.last_touch_sec,
                                    m_QueueDbBlock->job_db.last_touch_nsec);
        CNSPreciseTime  job_timeout =
                            CNSPreciseTime(
                                    m_QueueDbBlock->job_db.timeout_sec,
                                    m_QueueDbBlock->job_db.timeout_nsec);
        CNSPreciseTime  job_run_timeout =
                            CNSPreciseTime(
                                    m_QueueDbBlock->job_db.run_timeout_sec,
                                    m_QueueDbBlock->job_db.run_timeout_nsec);
        CNSPreciseTime  job_read_timeout =
                            CNSPreciseTime(
                                    m_QueueDbBlock->job_db.read_timeout_sec,
                                    m_QueueDbBlock->job_db.read_timeout_nsec);
        TJobStatus      status = TJobStatus(static_cast<int>(
                                                m_QueueDbBlock->job_db.status));

        m_StatusTracker.SetExactStatusNoLock(job_id, status, true);

        if ((status == CNetScheduleAPI::eRunning ||
             status == CNetScheduleAPI::eReading) &&
            m_RunTimeLine) {
            // Add object to the first available slot;
            // it is going to be rescheduled or dropped
            // in the background control thread
            // We can use time line without lock here because
            // the queue is still in single-use mode while
            // being loaded.
            m_RunTimeLine->AddObject(m_RunTimeLine->GetHead(), job_id);
        }

        // Register the job for the affinity if so
        if (aff_id != 0)
            m_AffinityRegistry.AddJobToAffinity(job_id, aff_id);

        // Register the job in the group registry
        if (group_id != 0)
            m_GroupRegistry.AddJob(group_id, job_id);

        CNSPreciseTime      submit_time(0, 0);
        if (status == CNetScheduleAPI::ePending) {
            submit_time = x_GetSubmitTime(job_id);
            if (submit_time == CNSPreciseTime())
                submit_time = CNSPreciseTime::Current();  // Be on a safe side
        }

        // Register the loaded job with the garbage collector
        // Pending jobs will be processed later
        CNSPreciseTime      precise_submit(submit_time);
        m_GCRegistry.RegisterJob(job_id, precise_submit, aff_id, group_id,
                                 GetJobExpirationTime(last_touch, status,
                                                      submit_time,
                                                      job_timeout,
                                                      job_run_timeout,
                                                      job_read_timeout,
                                                      m_Timeout,
                                                      m_RunTimeout,
                                                      m_ReadTimeout,
                                                      m_PendingTimeout,
                                                      CNSPreciseTime()));
        ++recs;
    }

    // Make sure that there are no affinity IDs in the registry for which there
    // are no jobs and initialize the next affinity ID counter.
    m_AffinityRegistry.FinalizeAffinityDictionaryLoading();

    // Make sure that there are no group IDs in the registry for which there
    // are no jobs and initialize the next group ID counter.
    m_GroupRegistry.FinalizeGroupDictionaryLoading();

    return recs;
}


// Used to log a single job
void CQueue::x_LogSubmit(const CJob &  job)
{
    CDiagContext_Extra  extra = GetDiagContext().Extra()
        .Print("job_key", MakeJobKey(job.GetId()))
        .Print("queue", GetQueueName());

    extra.Flush();
}


unsigned int  CQueue::Submit(const CNSClientId &        client,
                             CJob &                     job,
                             const string &             aff_token,
                             const string &             group,
                             bool                       logging,
                             CNSRollbackInterface * &   rollback_action)
{
    // the only config parameter used here is the max input size so there is no
    // need to have a safe parameters accessor.

    if (job.GetInput().size() > m_MaxInputSize)
        NCBI_THROW(CNetScheduleException, eDataTooLong, "Input is too long");

    unsigned int    aff_id = 0;
    unsigned int    group_id = 0;
    CNSPreciseTime  current_time = CNSPreciseTime::Current();
    unsigned int    job_id = GetNextId();
    CJobEvent &     event = job.AppendEvent();

    job.SetId(job_id);
    job.SetPassport(rand());
    job.SetLastTouch(current_time);

    event.SetNodeAddr(client.GetAddress());
    event.SetStatus(CNetScheduleAPI::ePending);
    event.SetEvent(CJobEvent::eSubmit);
    event.SetTimestamp(current_time);
    event.SetClientNode(client.GetNode());
    event.SetClientSession(client.GetSession());

    // Special treatment for system job masks
    if (job.GetMask() & CNetScheduleAPI::eOutOfOrder)
    {
        // NOT IMPLEMENTED YET: put job id into OutOfOrder list.
        // The idea is that there can be an urgent job, which
        // should be executed before jobs which were submitted
        // earlier, e.g. for some administrative purposes. See
        // CNetScheduleAPI::EJobMask in file netschedule_api.hpp
    }

    // Take the queue lock and start the operation
    {{
        CFastMutexGuard     guard(m_OperationLock);

        {{
            CNSTransaction      transaction(this);

            if (!group.empty()) {
                group_id = m_GroupRegistry.AddJob(group, job_id);
                job.SetGroupId(group_id);
            }
            if (!aff_token.empty()) {
                aff_id = m_AffinityRegistry.ResolveAffinityToken(aff_token,
                                                                 job_id, 0);
                job.SetAffinityId(aff_id);
            }
            job.Flush(this);

            transaction.Commit();
        }}

        m_StatusTracker.AddPendingJob(job_id);

        // Register the job with the client
        m_ClientsRegistry.AddToSubmitted(client, 1);

        // Make the decision whether to send or not a notification
        if (m_PauseStatus == eNoPause)
            m_NotificationsList.Notify(job_id, aff_id,
                                       m_ClientsRegistry,
                                       m_AffinityRegistry,
                                       m_GroupRegistry,
                                       m_NotifHifreqPeriod,
                                       m_HandicapTimeout,
                                       eGet);

        m_GCRegistry.RegisterJob(job_id, current_time,
                                 aff_id, group_id,
                                 job.GetExpirationTime(m_Timeout,
                                                       m_RunTimeout,
                                                       m_ReadTimeout,
                                                       m_PendingTimeout,
                                                       current_time));
    }}

    m_StatisticsCounters.CountSubmit(1);
    if (logging)
        x_LogSubmit(job);

    rollback_action = new CNSSubmitRollback(client, job_id);
    return job_id;
}


unsigned int
CQueue::SubmitBatch(const CNSClientId &             client,
                    vector< pair<CJob, string> > &  batch,
                    const string &                  group,
                    bool                            logging,
                    CNSRollbackInterface * &        rollback_action)
{
    unsigned int    batch_size = batch.size();
    unsigned int    job_id = GetNextJobIdForBatch(batch_size);
    TNSBitVector    affinities;
    CNSPreciseTime  curr_time = CNSPreciseTime::Current();

    {{
        unsigned int        job_id_cnt = job_id;
        unsigned int        group_id = 0;
        bool                no_aff_jobs = false;

        CFastMutexGuard     guard(m_OperationLock);

        {{
            CNSTransaction      transaction(this);

            // This might create a new record in the DB
            group_id = m_GroupRegistry.ResolveGroup(group);

            for (size_t  k = 0; k < batch_size; ++k) {

                CJob &              job = batch[k].first;
                const string &      aff_token = batch[k].second;
                CJobEvent &         event = job.AppendEvent();

                job.SetId(job_id_cnt);
                job.SetPassport(rand());
                job.SetGroupId(group_id);
                job.SetLastTouch(curr_time);

                event.SetNodeAddr(client.GetAddress());
                event.SetStatus(CNetScheduleAPI::ePending);
                event.SetEvent(CJobEvent::eBatchSubmit);
                event.SetTimestamp(curr_time);
                event.SetClientNode(client.GetNode());
                event.SetClientSession(client.GetSession());

                if (!aff_token.empty()) {
                    unsigned int    aff_id = m_AffinityRegistry.
                                                ResolveAffinityToken(aff_token,
                                                                     job_id_cnt,
                                                                     0);

                    job.SetAffinityId(aff_id);
                    affinities.set_bit(aff_id, true);
                }
                else
                    no_aff_jobs = true;

                job.Flush(this);
                ++job_id_cnt;
            }

            transaction.Commit();
        }}

        m_GroupRegistry.AddJobs(group_id, job_id, batch_size);
        m_StatusTracker.AddPendingBatch(job_id, job_id + batch_size - 1);
        m_ClientsRegistry.AddToSubmitted(client, batch_size);

        // Make a decision whether to notify clients or not
        TNSBitVector        jobs;
        jobs.set_range(job_id, job_id + batch_size - 1);

        if (m_PauseStatus == eNoPause)
            m_NotificationsList.Notify(jobs, affinities, no_aff_jobs,
                                       m_ClientsRegistry,
                                       m_AffinityRegistry,
                                       m_GroupRegistry,
                                       m_NotifHifreqPeriod,
                                       m_HandicapTimeout,
                                       eGet);

        for (size_t  k = 0; k < batch_size; ++k) {
            m_GCRegistry.RegisterJob(
                        batch[k].first.GetId(), curr_time,
                        batch[k].first.GetAffinityId(), group_id,
                        batch[k].first.GetExpirationTime(m_Timeout,
                                                         m_RunTimeout,
                                                         m_ReadTimeout,
                                                         m_PendingTimeout,
                                                         curr_time));
        }
    }}

    m_StatisticsCounters.CountSubmit(batch_size);
    if (m_LogBatchEachJob && logging)
        for (size_t  k = 0; k < batch_size; ++k)
            x_LogSubmit(batch[k].first);

    rollback_action = new CNSBatchSubmitRollback(client, job_id, batch_size);
    return job_id;
}



static const size_t     k_max_dead_locks = 10;  // max. dead lock repeats

TJobStatus  CQueue::PutResult(const CNSClientId &     client,
                              const CNSPreciseTime &  curr,
                              unsigned int            job_id,
                              const string &          job_key,
                              CJob &                  job,
                              const string &          auth_token,
                              int                     ret_code,
                              const string &          output)
{
    // The only one parameter (max output size) is required for the put
    // operation so there is no need to use CQueueParamAccessor

    if (output.size() > m_MaxOutputSize)
        NCBI_THROW(CNetScheduleException, eDataTooLong,
                   "Output is too long");

    size_t              dead_locks = 0; // dead lock counter

    for (;;) {
        try {
            CFastMutexGuard     guard(m_OperationLock);
            TJobStatus          old_status = GetJobStatus(job_id);

            if (old_status == CNetScheduleAPI::eDone) {
                m_StatisticsCounters.CountTransition(CNetScheduleAPI::eDone,
                                                     CNetScheduleAPI::eDone);
                return old_status;
            }

            if (old_status != CNetScheduleAPI::ePending &&
                old_status != CNetScheduleAPI::eRunning &&
                old_status != CNetScheduleAPI::eFailed)
                return old_status;

            {{
                CNSTransaction      transaction(this);
                x_UpdateDB_PutResultNoLock(job_id, auth_token, curr,
                                           ret_code, output, job,
                                           client);
                transaction.Commit();
            }}

            m_StatusTracker.SetStatus(job_id, CNetScheduleAPI::eDone);
            m_StatisticsCounters.CountTransition(old_status,
                                                 CNetScheduleAPI::eDone);
            m_ClientsRegistry.ClearExecuting(job_id);

            m_GCRegistry.UpdateLifetime(job_id,
                                        job.GetExpirationTime(m_Timeout,
                                                              m_RunTimeout,
                                                              m_ReadTimeout,
                                                              m_PendingTimeout,
                                                              curr));

            TimeLineRemove(job_id);

            if (job.ShouldNotifySubmitter(curr))
                m_NotificationsList.NotifyJobStatus(job.GetSubmAddr(),
                                                    job.GetSubmNotifPort(),
                                                    job_key,
                                                    job.GetStatus(),
                                                    job.GetLastEventIndex());
            if (job.ShouldNotifyListener(curr, m_JobsToNotify))
                m_NotificationsList.NotifyJobStatus(job.GetListenerNotifAddr(),
                                                    job.GetListenerNotifPort(),
                                                    job_key,
                                                    job.GetStatus(),
                                                    job.GetLastEventIndex());

            // Notify the readers if the job has not been given for reading yet
            if (m_ReadJobs[job_id] == false)
                m_NotificationsList.Notify(job_id, job.GetAffinityId(),
                                           m_ClientsRegistry,
                                           m_AffinityRegistry,
                                           m_GroupRegistry,
                                           m_NotifHifreqPeriod,
                                           m_HandicapTimeout,
                                           eRead);
            return old_status;
        }
        catch (CBDB_ErrnoException &  ex) {
            if (ex.IsDeadLock()) {
                if (++dead_locks < k_max_dead_locks) {
                    ERR_POST("DeadLock repeat in CQueue::PutResult");
                    SleepMilliSec(100);
                    continue;
                }
            } else if (ex.IsNoMem()) {
                if (++dead_locks < k_max_dead_locks) {
                    ERR_POST("No resource repeat in CQueue::PutResult");
                    SleepMilliSec(100);
                    continue;
                }
            }

            if (ex.IsDeadLock() || ex.IsNoMem()) {
                string  message = "Too many transaction repeats in "
                                  "CQueue::PutResult";
                ERR_POST(message);
                NCBI_THROW(CNetScheduleException, eTryAgain, message);
            }

            throw;
        }
    }
}


bool
CQueue::GetJobOrWait(const CNSClientId &       client,
                     unsigned short            port, // Port the client
                                                     // will wait on
                     unsigned int              timeout, // If timeout != 0 =>
                                                        // WGET
                     const CNSPreciseTime &    curr,
                     const list<string> *      aff_list,
                     bool                      wnode_affinity,
                     bool                      any_affinity,
                     bool                      exclusive_new_affinity,
                     bool                      new_format,
                     const string &            group,
                     CJob *                    new_job,
                     CNSRollbackInterface * &  rollback_action,
                     string &                  added_pref_aff)
{
    // We need exactly 1 parameter - m_RunTimeout, so we can access it without
    // CQueueParamAccessor

    size_t      dead_locks = 0;                 // dead lock counter

    // This is a worker node command, so mark the node type as a worker
    // node
    m_ClientsRegistry.AppendType(client, CNSClient::eWorkerNode);

    for (;;) {
        try {
            TNSBitVector        aff_ids;

            {{
                CFastMutexGuard     guard(m_OperationLock);

                if (wnode_affinity) {
                    // Check first that the were no preferred affinities reset
                    // for the client
                    if (m_ClientsRegistry.GetAffinityReset(client))
                        return false;   // Affinity was reset, so
                                        // no job for the client

                    // It is possible that the client had been garbage
                    // collected
                    if (m_ClientsRegistry.WasGarbageCollected(client))
                        return false;
                }

                // Resolve affinities regardless whether it is GET or WGET. It
                // is supposed that the client knows better what affinities to
                // expect i.e. even if they do not exist yet, they may appear
                // soon.
                {{
                    CNSTransaction      transaction(this);

                    m_GroupRegistry.ResolveGroup(group);
                    aff_ids = m_AffinityRegistry.ResolveAffinities(*aff_list);
                    transaction.Commit();
                }}

                x_UnregisterGetListener(client, port);
            }}

            for (;;) {
                // No lock here to make it possible to pick a job
                // simultaneously from many threads
                x_SJobPick  job_pick = x_FindPendingJob(client, aff_ids,
                                                        wnode_affinity,
                                                        any_affinity,
                                                        exclusive_new_affinity,
                                                        group);
                {{
                    bool                outdated_job = false;
                    CFastMutexGuard     guard(m_OperationLock);

                    if (job_pick.job_id == 0) {
                        if (exclusive_new_affinity)
                            // Second try only if exclusive new aff is set on
                            job_pick = x_FindOutdatedPendingJob(client, 0);

                        if (job_pick.job_id == 0) {
                            if (timeout != 0) {
                                // WGET:
                                // There is no job, so the client might need to
                                // be registered
                                // in the waiting list
                                if (port > 0)
                                    x_RegisterGetListener(
                                            client, port, timeout, aff_ids,
                                            wnode_affinity, any_affinity,
                                            exclusive_new_affinity,
                                            new_format, group);
                            }
                            return true;
                        }
                        outdated_job = true;
                    } else {
                        // Check that the job is still Pending; it could be
                        // grabbed by another WN or GC
                        if (GetJobStatus(job_pick.job_id) !=
                                CNetScheduleAPI::ePending)
                            continue;   // Try to pick a job again

                        if (exclusive_new_affinity) {
                            if (m_GCRegistry.IsOutdatedJob(
                                            job_pick.job_id,
                                            m_MaxPendingWaitTimeout) == false) {
                                x_SJobPick  outdated_pick =
                                                x_FindOutdatedPendingJob(
                                                            client,
                                                            job_pick.job_id);
                                if (outdated_pick.job_id != 0) {
                                    job_pick = outdated_pick;
                                    outdated_job = true;
                                }
                            }
                        }
                    }

                    // The job is still pending, check if it was received as
                    // with exclusive affinity
                    if (job_pick.exclusive && job_pick.aff_id != 0 &&
                        outdated_job == false) {
                        if (m_ClientsRegistry.IsPreferredByAny(
                                                        job_pick.aff_id))
                            continue;  // Other WN grabbed this affinity already
                        bool added = m_ClientsRegistry.
                                            UpdatePreferredAffinities(
                                                client, job_pick.aff_id, 0);
                        m_AffinityRegistry.AddClientToAffinity(client.GetID(),
                                                               job_pick.aff_id);
                        if (added)
                            added_pref_aff = m_AffinityRegistry.GetTokenByID(
                                                            job_pick.aff_id);
                    }
                    if (outdated_job && job_pick.aff_id != 0) {
                        bool added = m_ClientsRegistry.
                                            UpdatePreferredAffinities(
                                                client, job_pick.aff_id, 0);
                        m_AffinityRegistry.AddClientToAffinity(client.GetID(),
                                                               job_pick.aff_id);
                        if (added)
                            added_pref_aff = m_AffinityRegistry.GetTokenByID(
                                                            job_pick.aff_id);
                    }

                    if (x_UpdateDB_GetJobNoLock(client, curr,
                                                job_pick.job_id, *new_job)) {
                        // The job is not expired and successfully read
                        m_StatusTracker.SetStatus(job_pick.job_id,
                                                  CNetScheduleAPI::eRunning);

                        m_StatisticsCounters.CountTransition(
                                                    CNetScheduleAPI::ePending,
                                                    CNetScheduleAPI::eRunning);
                        if (outdated_job)
                            m_StatisticsCounters.CountOutdatedPick();

                        m_GCRegistry.UpdateLifetime(
                                    job_pick.job_id,
                                    new_job->GetExpirationTime(m_Timeout,
                                                               m_RunTimeout,
                                                               m_ReadTimeout,
                                                               m_PendingTimeout,
                                                               curr));
                        TimeLineAdd(job_pick.job_id, curr + m_RunTimeout);
                        m_ClientsRegistry.AddToRunning(client, job_pick.job_id);

                        if (new_job->ShouldNotifyListener(curr, m_JobsToNotify))
                            m_NotificationsList.NotifyJobStatus(
                                            new_job->GetListenerNotifAddr(),
                                            new_job->GetListenerNotifPort(),
                                            MakeJobKey(new_job->GetId()),
                                            new_job->GetStatus(),
                                            new_job->GetLastEventIndex());

                        // If there are no more pending jobs, let's clear the
                        // list of delayed exact notifications.
                        if (!m_StatusTracker.AnyPending())
                            m_NotificationsList.ClearExactGetNotifications();

                        rollback_action = new CNSGetJobRollback(
                                                    client, job_pick.job_id);
                        return true;
                    }

                    // Reset job_id and try again
                    new_job->SetId(0);
                }}
            }
        }
        catch (CBDB_ErrnoException &  ex) {
            if (ex.IsDeadLock()) {
                if (++dead_locks < k_max_dead_locks) {
                    ERR_POST("DeadLock repeat in CQueue::GetJobOrWait");
                    SleepMilliSec(100);
                    continue;
                }
            } else if (ex.IsNoMem()) {
                if (++dead_locks < k_max_dead_locks) {
                    ERR_POST("No resource repeat in CQueue::GetJobOrWait");
                    SleepMilliSec(100);
                    continue;
                }
            }

            if (ex.IsDeadLock() || ex.IsNoMem()) {
                string  message = "Too many transaction repeats in "
                                  "CQueue::GetJobOrWait";
                ERR_POST(message);
                NCBI_THROW(CNetScheduleException, eTryAgain, message);
            }

            throw;
        }
    }
    return true;
}


void  CQueue::CancelWaitGet(const CNSClientId &  client)
{
    bool    result;

    {{
        CFastMutexGuard     guard(m_OperationLock);

        result = x_UnregisterGetListener(client, 0);
    }}

    if (result == false)
        LOG_POST(Message << Warning << "Attempt to cancel WGET for the client "
                                       "which does not wait anything (node: "
                         << client.GetNode() << " session: "
                         << client.GetSession() << ")");
}


void  CQueue::CancelWaitRead(const CNSClientId &  client)
{
    bool    result;

    {{
        CFastMutexGuard     guard(m_OperationLock);

        result = x_UnregisterReadListener(client);
    }}

    if (result == false)
        LOG_POST(Message << Warning << "Attempt to cancel waiting READ for the "
                                       "client which does not wait anything "
                                       "(node: "
                         << client.GetNode() << " session: "
                         << client.GetSession() << ")");
}


list<string>
CQueue::ChangeAffinity(const CNSClientId &     client,
                       const list<string> &    aff_to_add,
                       const list<string> &    aff_to_del)
{
    // It is guaranteed here that the client is a new style one.
    // I.e. it has both client_node and client_session.

    list<string>    msgs;   // Warning messages for the socket
    unsigned int    client_id = client.GetID();
    TNSBitVector    current_affinities =
                         m_ClientsRegistry.GetPreferredAffinities(client);
    TNSBitVector    aff_id_to_add;
    TNSBitVector    aff_id_to_del;
    bool            any_to_add = false;
    bool            any_to_del = false;

    // Identify the affinities which should be deleted
    for (list<string>::const_iterator  k(aff_to_del.begin());
         k != aff_to_del.end(); ++k) {
        unsigned int    aff_id = m_AffinityRegistry.GetIDByToken(*k);

        if (aff_id == 0) {
            // The affinity is not known for NS at all
            LOG_POST(Message << Warning << "Client '" << client.GetNode()
                             << "' deletes unknown affinity '"
                             << *k << "'. Ignored.");
            msgs.push_back("eAffinityNotFound:"
                           "unknown affinity to delete: " + *k);
            continue;
        }

        if (current_affinities[aff_id] == false) {
            // This a try to delete something which has not been added or
            // deleted before.
            LOG_POST(Message << Warning << "Client '" << client.GetNode()
                             << "' deletes affinity '" << *k
                             << "' which is not in the list of the "
                                "preferred client affinities. Ignored.");
            msgs.push_back("eAffinityNotPreferred:not registered affinity "
                           "to delete: " + *k);
            continue;
        }

        // The affinity will really be deleted
        aff_id_to_del.set(aff_id, true);
        any_to_del = true;
    }


    // Check that the update of the affinities list will not exceed the limit
    // for the max number of affinities per client.
    // Note: this is not precise check. There could be non-unique affinities in
    // the add list or some of affinities to add could already be in the list.
    // The precise checking however requires more CPU and blocking so only an
    // approximate (but fast) checking is done.
    if (current_affinities.count() + aff_to_add.size()
                                   - aff_id_to_del.count() >
                                         m_MaxAffinities) {
        NCBI_THROW(CNetScheduleException, eTooManyPreferredAffinities,
                   "The client '" + client.GetNode() +
                   "' exceeds the limit (" +
                   NStr::NumericToString(m_MaxAffinities) +
                   ") of the preferred affinities. Changed request ignored.");
    }

    // To avoid logging under the lock
    vector<string>  already_added_affinities;

    {{
        CFastMutexGuard     guard(m_OperationLock);
        CNSTransaction      transaction(this);

        // Convert the aff_to_add to the affinity IDs
        for (list<string>::const_iterator  k(aff_to_add.begin());
             k != aff_to_add.end(); ++k ) {
            unsigned int    aff_id =
                                 m_AffinityRegistry.ResolveAffinityToken(*k,
                                                                 0, client_id);

            if (current_affinities[aff_id] == true) {
                already_added_affinities.push_back(*k);
                continue;
            }

            aff_id_to_add.set(aff_id, true);
            any_to_add = true;
        }

        transaction.Commit();
    }}

    // Log the warnings and add it to the warning message
    for (vector<string>::const_iterator  j(already_added_affinities.begin());
         j != already_added_affinities.end(); ++j) {
        // That was a try to add something which has already been added
        LOG_POST(Message << Warning << "Client '" << client.GetNode()
                         << "' adds affinity '" << *j
                         << "' which is already in the list of the "
                            "preferred client affinities. Ignored.");
        msgs.push_back("eAffinityAlreadyPreferred:already registered "
                       "affinity to add: " + *j);
    }

    if (any_to_del)
        m_AffinityRegistry.RemoveClientFromAffinities(client_id,
                                                      aff_id_to_del);

    if (any_to_add || any_to_del)
        m_ClientsRegistry.UpdatePreferredAffinities(client,
                                                    aff_id_to_add,
                                                    aff_id_to_del);

    if (m_ClientsRegistry.WasGarbageCollected(client)) {
        LOG_POST(Message << Warning << "Client '" << client.GetNode()
                         << "' has been garbage collected and tries to "
                            "update its preferred affinities.");
        msgs.push_back("eClientGarbageCollected:the client had been "
                       "garbage collected");
    }
    return msgs;
}


void  CQueue::SetAffinity(const CNSClientId &     client,
                          const list<string> &    aff)
{
    if (aff.size() > m_MaxAffinities) {
        NCBI_THROW(CNetScheduleException, eTooManyPreferredAffinities,
                   "The client '" + client.GetNode() +
                   "' exceeds the limit (" +
                   NStr::NumericToString(m_MaxAffinities) +
                   ") of the preferred affinities. Set request ignored.");
    }

    unsigned int    client_id = client.GetID();
    TNSBitVector    aff_id_to_set;
    TNSBitVector    already_added_aff_id;


    TNSBitVector    current_affinities =
                         m_ClientsRegistry.GetPreferredAffinities(client);
    {{
        CFastMutexGuard     guard(m_OperationLock);
        CNSTransaction      transaction(this);

        // Convert the aff to the affinity IDs
        for (list<string>::const_iterator  k(aff.begin());
             k != aff.end(); ++k ) {
            unsigned int    aff_id =
                                 m_AffinityRegistry.ResolveAffinityToken(*k,
                                                                 0, client_id);

            if (current_affinities[aff_id] == true)
                already_added_aff_id.set(aff_id, true);

            aff_id_to_set.set(aff_id, true);
        }

        transaction.Commit();
    }}

    TNSBitVector    aff_id_to_del = current_affinities - already_added_aff_id;

    if (aff_id_to_del.any())
        m_AffinityRegistry.RemoveClientFromAffinities(client_id,
                aff_id_to_del);

    m_ClientsRegistry.SetPreferredAffinities(client, aff_id_to_set);
}


int CQueue::SetClientData(const CNSClientId &  client,
                          const string &  data, int  data_version)
{
    return m_ClientsRegistry.SetClientData(client, data, data_version);
}


TJobStatus  CQueue::JobDelayExpiration(unsigned int            job_id,
                                       CJob &                  job,
                                       const CNSPreciseTime &  tm)
{
    CNSPreciseTime      queue_run_timeout = GetRunTimeout();
    CNSPreciseTime      curr = CNSPreciseTime::Current();

    {{
        CFastMutexGuard     guard(m_OperationLock);
        TJobStatus          status = GetJobStatus(job_id);

        if (status != CNetScheduleAPI::eRunning)
            return status;

        CNSPreciseTime  time_start = CNSPreciseTime();
        CNSPreciseTime  run_timeout = CNSPreciseTime();
        {{
            CNSTransaction      transaction(this);

            if (job.Fetch(this, job_id) != CJob::eJF_Ok)
                return CNetScheduleAPI::eJobNotFound;

            time_start = job.GetLastEvent()->GetTimestamp();
            run_timeout = job.GetRunTimeout();
            if (run_timeout == CNSPreciseTime())
                run_timeout = queue_run_timeout;

            if (time_start + run_timeout > curr + tm)
                return CNetScheduleAPI::eRunning;   // Old timeout is enough to
                                                    // cover this request, so
                                                    // keep it.

            job.SetRunTimeout(curr + tm - time_start);
            job.SetLastTouch(curr);
            job.Flush(this);
            transaction.Commit();
        }}

        // No need to update the GC registry because the running (and reading)
        // jobs are skipped by GC
        CNSPreciseTime      exp_time = CNSPreciseTime();
        if (run_timeout != CNSPreciseTime())
            exp_time = time_start + run_timeout;

        TimeLineMove(job_id, exp_time, curr + tm);
    }}

    return CNetScheduleAPI::eRunning;
}


TJobStatus  CQueue::JobDelayReadExpiration(unsigned int            job_id,
                                           CJob &                  job,
                                           const CNSPreciseTime &  tm)
{
    CNSPreciseTime      queue_read_timeout = GetReadTimeout();
    CNSPreciseTime      curr = CNSPreciseTime::Current();

    {{
        CFastMutexGuard     guard(m_OperationLock);
        TJobStatus          status = GetJobStatus(job_id);

        if (status != CNetScheduleAPI::eReading)
            return status;

        CNSPreciseTime  time_start = CNSPreciseTime();
        CNSPreciseTime  read_timeout = CNSPreciseTime();
        {{
            CNSTransaction      transaction(this);

            if (job.Fetch(this, job_id) != CJob::eJF_Ok)
                return CNetScheduleAPI::eJobNotFound;

            time_start = job.GetLastEvent()->GetTimestamp();
            read_timeout = job.GetReadTimeout();
            if (read_timeout == CNSPreciseTime())
                read_timeout = queue_read_timeout;

            if (time_start + read_timeout > curr + tm)
                return CNetScheduleAPI::eReading;   // Old timeout is enough to
                                                    // cover this request, so
                                                    // keep it.

            job.SetReadTimeout(curr + tm - time_start);
            job.SetLastTouch(curr);
            job.Flush(this);
            transaction.Commit();
        }}

        // No need to update the GC registry because the running (and reading)
        // jobs are skipped by GC
        CNSPreciseTime      exp_time = CNSPreciseTime();
        if (read_timeout != CNSPreciseTime())
            exp_time = time_start + read_timeout;

        TimeLineMove(job_id, exp_time, curr + tm);
    }}

    return CNetScheduleAPI::eReading;
}



TJobStatus  CQueue::GetStatusAndLifetime(unsigned int      job_id,
                                         CJob &            job,
                                         bool              need_touch,
                                         CNSPreciseTime *  lifetime)
{
    CFastMutexGuard     guard(m_OperationLock);
    TJobStatus          status = GetJobStatus(job_id);

    if (status == CNetScheduleAPI::eJobNotFound)
        return status;

    CNSPreciseTime      curr = CNSPreciseTime::Current();

    {{
        CNSTransaction      transaction(this);

        if (job.Fetch(this, job_id) != CJob::eJF_Ok)
            NCBI_THROW(CNetScheduleException, eInternalError,
                       "Error fetching job");

        if (need_touch) {
            job.SetLastTouch(curr);
            job.Flush(this);
            transaction.Commit();
        }
    }}

    if (need_touch) {
        m_GCRegistry.UpdateLifetime(job_id,
                                    job.GetExpirationTime(m_Timeout,
                                                          m_RunTimeout,
                                                          m_ReadTimeout,
                                                          m_PendingTimeout,
                                                          curr));
    }

    *lifetime = x_GetEstimatedJobLifetime(job_id, status);
    return status;
}


TJobStatus  CQueue::SetJobListener(unsigned int            job_id,
                                   CJob &                  job,
                                   unsigned int            address,
                                   unsigned short          port,
                                   const CNSPreciseTime &  timeout,
                                   size_t *                last_event_index)
{
    CNSPreciseTime      curr = CNSPreciseTime::Current();
    TJobStatus          status = CNetScheduleAPI::eJobNotFound;
    CFastMutexGuard     guard(m_OperationLock);
    bool                switched_on = true;

    {{
        CNSTransaction      transaction(this);

        if (job.Fetch(this, job_id) != CJob::eJF_Ok)
            return status;

        *last_event_index = job.GetLastEventIndex();
        status = job.GetStatus();
        if (status == CNetScheduleAPI::eCanceled)
            return status;  // Listener notifications are valid for all states
                            // except of Cancel - there are no transitions from
                            // Cancel.

        if (address == 0 || port == 0 || timeout == CNSPreciseTime()) {
            // If at least one of the values is 0 => no notifications
            // So to make the job properly dumped put zeros everywhere.
            job.SetListenerNotifAddr(0);
            job.SetListenerNotifPort(0);
            job.SetListenerNotifAbsTime(CNSPreciseTime());
            switched_on = false;
        } else {
            job.SetListenerNotifAddr(address);
            job.SetListenerNotifPort(port);
            job.SetListenerNotifAbsTime(curr + timeout);
        }

        job.SetLastTouch(curr);
        job.Flush(this);
        transaction.Commit();
    }}

    m_JobsToNotify.set_bit(job_id, switched_on);

    return status;
}


bool CQueue::PutProgressMessage(unsigned int    job_id,
                                CJob &          job,
                                const string &  msg)
{
    CNSPreciseTime      curr = CNSPreciseTime::Current();

    {{
        CFastMutexGuard     guard(m_OperationLock);

        {{
            CNSTransaction      transaction(this);

            if (job.Fetch(this, job_id) != CJob::eJF_Ok)
                return false;

            job.SetProgressMsg(msg);
            job.SetLastTouch(curr);
            job.Flush(this);
            transaction.Commit();
        }}

        m_GCRegistry.UpdateLifetime(job_id,
                                    job.GetExpirationTime(m_Timeout,
                                                          m_RunTimeout,
                                                          m_ReadTimeout,
                                                          m_PendingTimeout,
                                                          curr));
    }}

    return true;
}


TJobStatus  CQueue::ReturnJob(const CNSClientId &     client,
                              unsigned int            job_id,
                              const string &          job_key,
                              CJob &                  job,
                              const string &          auth_token,
                              string &                warning,
                              TJobReturnOption        how)
{
    CNSPreciseTime      current_time = CNSPreciseTime::Current();
    CFastMutexGuard     guard(m_OperationLock);
    TJobStatus          old_status = GetJobStatus(job_id);

    if (old_status != CNetScheduleAPI::eRunning)
        return old_status;

    {{
         CNSTransaction      transaction(this);

        if (job.Fetch(this, job_id) != CJob::eJF_Ok)
            NCBI_THROW(CNetScheduleException, eInternalError,
                       "Error fetching job");

        if (!auth_token.empty()) {
            // Need to check authorization token first
            CJob::EAuthTokenCompareResult   token_compare_result =
                                            job.CompareAuthToken(auth_token);
            if (token_compare_result == CJob::eInvalidTokenFormat)
                NCBI_THROW(CNetScheduleException, eInvalidAuthToken,
                           "Invalid authorization token format");
            if (token_compare_result == CJob::eNoMatch)
                NCBI_THROW(CNetScheduleException, eInvalidAuthToken,
                           "Authorization token does not match");
            if (token_compare_result == CJob::ePassportOnlyMatch) {
                // That means the job has been given to another worker node
                // by whatever reason (expired/failed/returned before)
                LOG_POST(Message << Warning << "Received RETURN2 with only "
                                               "passport matched.");
                warning = "eJobPassportOnlyMatch:Only job passport matched. "
                          "Command is ignored.";
                return old_status;
            }
            // Here: the authorization token is OK, we can continue
        }


        unsigned int    run_count = job.GetRunCount();
        CJobEvent *     event = job.GetLastEvent();

        if (!event)
            ERR_POST("No JobEvent for running job");

        event = &job.AppendEvent();
        event->SetNodeAddr(client.GetAddress());
        event->SetStatus(CNetScheduleAPI::ePending);
        switch (how) {
            case eWithBlacklist:
                event->SetEvent(CJobEvent::eReturn);
                break;
            case eWithoutBlacklist:
                event->SetEvent(CJobEvent::eReturnNoBlacklist);
                break;
            case eRollback:
                event->SetEvent(CJobEvent::eNSGetRollback);
                break;
        }
        event->SetTimestamp(current_time);
        event->SetClientNode(client.GetNode());
        event->SetClientSession(client.GetSession());

        if (run_count)
            job.SetRunCount(run_count-1);

        job.SetStatus(CNetScheduleAPI::ePending);
        job.SetLastTouch(current_time);
        job.Flush(this);

        transaction.Commit();
    }}

    m_StatusTracker.SetStatus(job_id, CNetScheduleAPI::ePending);
    switch (how) {
        case eWithBlacklist:
            m_StatisticsCounters.CountTransition(old_status,
                                                 CNetScheduleAPI::ePending);
            break;
        case eWithoutBlacklist:
            m_StatisticsCounters.CountToPendingWithoutBlacklist(1);
            break;
        case eRollback:
            m_StatisticsCounters.CountNSGetRollback(1);
            break;
    }
    TimeLineRemove(job_id);
    m_ClientsRegistry.ClearExecuting(job_id);
    if (how == eWithBlacklist)
        m_ClientsRegistry.AddToBlacklist(client, job_id);
    m_GCRegistry.UpdateLifetime(job_id,
                                job.GetExpirationTime(m_Timeout,
                                                      m_RunTimeout,
                                                      m_ReadTimeout,
                                                      m_PendingTimeout,
                                                      current_time));

    if (job.ShouldNotifyListener(current_time, m_JobsToNotify))
        m_NotificationsList.NotifyJobStatus(job.GetListenerNotifAddr(),
                                            job.GetListenerNotifPort(),
                                            job_key,
                                            job.GetStatus(),
                                            job.GetLastEventIndex());

    if (m_PauseStatus == eNoPause)
        m_NotificationsList.Notify(job_id,
                                   job.GetAffinityId(), m_ClientsRegistry,
                                   m_AffinityRegistry, m_GroupRegistry,
                                   m_NotifHifreqPeriod, m_HandicapTimeout,
                                   eGet);

    return old_status;
}


TJobStatus  CQueue::RescheduleJob(const CNSClientId &     client,
                                  unsigned int            job_id,
                                  const string &          job_key,
                                  const string &          auth_token,
                                  const string &          aff_token,
                                  const string &          group,
                                  bool &                  auth_token_ok,
                                  CJob &                  job)
{
    CNSPreciseTime      current_time = CNSPreciseTime::Current();
    CFastMutexGuard     guard(m_OperationLock);
    TJobStatus          old_status = GetJobStatus(job_id);
    unsigned int        affinity_id = 0;
    unsigned int        group_id = 0;
    unsigned int        job_affinity_id;
    unsigned int        job_group_id;

    if (old_status != CNetScheduleAPI::eRunning)
        return old_status;

    // Resolve affinity and group in a separate transaction
    if (!aff_token.empty() || !group.empty()) {
        {{
            CNSTransaction      transaction(this);
            if (!aff_token.empty())
                affinity_id = m_AffinityRegistry.ResolveAffinity(aff_token);
            if (!group.empty())
                group_id = m_GroupRegistry.ResolveGroup(group);
            transaction.Commit();
        }}
    }

    {{
         CNSTransaction      transaction(this);

        if (job.Fetch(this, job_id) != CJob::eJF_Ok)
            NCBI_THROW(CNetScheduleException, eInternalError,
                       "Error fetching job");

        // Need to check authorization token first
        CJob::EAuthTokenCompareResult   token_compare_result =
                                            job.CompareAuthToken(auth_token);

        if (token_compare_result == CJob::eInvalidTokenFormat)
            NCBI_THROW(CNetScheduleException, eInvalidAuthToken,
                       "Invalid authorization token format");

        if (token_compare_result != CJob::eCompleteMatch) {
            auth_token_ok = false;
            return old_status;
        }

        // Here: the authorization token is OK, we can continue
        auth_token_ok = true;

        // Memorize the job group and affinity for the proper updates after
        // the transaction is finished
        job_affinity_id = job.GetAffinityId();
        job_group_id = job.GetGroupId();

        // Update the job affinity and group
        job.SetAffinityId(affinity_id);
        job.SetGroupId(group_id);

        unsigned int    run_count = job.GetRunCount();
        CJobEvent *     event = job.GetLastEvent();

        if (!event)
            ERR_POST("No JobEvent for running job");

        event = &job.AppendEvent();
        event->SetNodeAddr(client.GetAddress());
        event->SetStatus(CNetScheduleAPI::ePending);
        event->SetEvent(CJobEvent::eReschedule);
        event->SetTimestamp(current_time);
        event->SetClientNode(client.GetNode());
        event->SetClientSession(client.GetSession());

        if (run_count)
            job.SetRunCount(run_count-1);

        job.SetStatus(CNetScheduleAPI::ePending);
        job.SetLastTouch(current_time);
        job.Flush(this);

        transaction.Commit();
    }}

    // Job has been updated in the DB. Update the affinity and group
    // registries as needed.
    if (job_affinity_id != affinity_id) {
        if (job_affinity_id != 0)
            m_AffinityRegistry.RemoveJobFromAffinity(job_id, job_affinity_id);
        if (affinity_id != 0)
            m_AffinityRegistry.AddJobToAffinity(job_id, affinity_id);
    }
    if (job_group_id != group_id) {
        if (job_group_id != 0)
            m_GroupRegistry.RemoveJob(job_group_id, job_id);
        if (group_id != 0)
            m_GroupRegistry.AddJob(group_id, job_id);
    }
    if (job_affinity_id != affinity_id || job_group_id != group_id)
        m_GCRegistry.ChangeAffinityAndGroup(job_id, affinity_id, group_id);

    m_StatusTracker.SetStatus(job_id, CNetScheduleAPI::ePending);
    m_StatisticsCounters.CountToPendingRescheduled(1);

    TimeLineRemove(job_id);
    m_ClientsRegistry.ClearExecuting(job_id);
    m_GCRegistry.UpdateLifetime(job_id,
                                job.GetExpirationTime(m_Timeout,
                                                      m_RunTimeout,
                                                      m_ReadTimeout,
                                                      m_PendingTimeout,
                                                      current_time));

    if (job.ShouldNotifyListener(current_time, m_JobsToNotify))
        m_NotificationsList.NotifyJobStatus(job.GetListenerNotifAddr(),
                                            job.GetListenerNotifPort(),
                                            job_key,
                                            job.GetStatus(),
                                            job.GetLastEventIndex());

    if (m_PauseStatus == eNoPause)
        m_NotificationsList.Notify(job_id,
                                   job.GetAffinityId(), m_ClientsRegistry,
                                   m_AffinityRegistry, m_GroupRegistry,
                                   m_NotifHifreqPeriod, m_HandicapTimeout,
                                   eGet);
    return old_status;
}


TJobStatus  CQueue::ReadAndTouchJob(unsigned int      job_id,
                                    CJob &            job,
                                    CNSPreciseTime *  lifetime)
{
    CFastMutexGuard         guard(m_OperationLock);
    TJobStatus              status = GetJobStatus(job_id);

    if (status == CNetScheduleAPI::eJobNotFound)
        return status;

    CNSPreciseTime          curr = CNSPreciseTime::Current();
    {{
        CNSTransaction      transaction(this);

        if (job.Fetch(this, job_id) != CJob::eJF_Ok)
            NCBI_THROW(CNetScheduleException, eInternalError,
                       "Error fetching job");

        job.SetLastTouch(curr);
        job.Flush(this);
        transaction.Commit();
    }}

    m_GCRegistry.UpdateLifetime(job_id,
                                job.GetExpirationTime(m_Timeout,
                                                      m_RunTimeout,
                                                      m_ReadTimeout,
                                                      m_PendingTimeout,
                                                      curr));
    *lifetime = x_GetEstimatedJobLifetime(job_id, status);
    return status;
}


// Deletes all the jobs from the queue
void CQueue::Truncate(bool  logging)
{
    CJob                                job;
    TNSBitVector                        bv;
    vector<CNetScheduleAPI::EJobStatus> statuses;
    TNSBitVector                        jobs_to_notify;
    CNSPreciseTime                      current_time =
                                                CNSPreciseTime::Current();

    // Pending and running jobs should be notified if requested
    statuses.push_back(CNetScheduleAPI::ePending);
    statuses.push_back(CNetScheduleAPI::eRunning);

    {{
        CFastMutexGuard     guard(m_OperationLock);

        jobs_to_notify = m_StatusTracker.GetJobs(statuses);

        // Make a decision if the jobs should really be notified
        {{
            CNSTransaction              transaction(this);
            TNSBitVector::enumerator    en(jobs_to_notify.first());
            for (; en.valid(); ++en) {
                unsigned int    job_id = *en;

                if (job.Fetch(this, job_id) != CJob::eJF_Ok) {
                    ERR_POST("Cannot fetch job " << DecorateJob(job_id) <<
                             " while dropping all jobs in the queue");
                    continue;
                }

                if (job.ShouldNotifySubmitter(current_time))
                    m_NotificationsList.NotifyJobStatus(
                                                job.GetSubmAddr(),
                                                job.GetSubmNotifPort(),
                                                MakeJobKey(job_id),
                                                job.GetStatus(),
                                                job.GetLastEventIndex());

                if (logging)
                    GetDiagContext().Extra().Print("job_key",
                                                   MakeJobKey(job_id))
                                            .Print("job_phid",
                                                   job.GetNCBIPHID());
            }

            // There is no need to commit transaction - there were no changes
        }}

        m_StatusTracker.ClearAll(&bv);
        m_AffinityRegistry.ClearMemoryAndDatabase();
        m_GroupRegistry.ClearMemoryAndDatabase();

        CWriteLockGuard     rtl_guard(m_RunTimeLineLock);
        m_RunTimeLine->ReInit(0);
    }}

    x_Erase(bv);
}


TJobStatus  CQueue::Cancel(const CNSClientId &  client,
                           unsigned int         job_id,
                           const string &       job_key,
                           CJob &               job,
                           bool                 is_ns_rollback)
{
    TJobStatus          old_status;
    CNSPreciseTime      current_time = CNSPreciseTime::Current();

    {{
        CFastMutexGuard     guard(m_OperationLock);

        old_status = m_StatusTracker.GetStatus(job_id);
        if (old_status == CNetScheduleAPI::eJobNotFound)
            return CNetScheduleAPI::eJobNotFound;
        if (old_status == CNetScheduleAPI::eCanceled) {
            if (is_ns_rollback)
                m_StatisticsCounters.CountNSSubmitRollback(1);
            else
                m_StatisticsCounters.CountTransition(
                                            CNetScheduleAPI::eCanceled,
                                            CNetScheduleAPI::eCanceled);
            return CNetScheduleAPI::eCanceled;
        }

        {{
            CNSTransaction      transaction(this);
            if (job.Fetch(this, job_id) != CJob::eJF_Ok)
                return CNetScheduleAPI::eJobNotFound;

            CJobEvent *     event = &job.AppendEvent();

            event->SetNodeAddr(client.GetAddress());
            event->SetStatus(CNetScheduleAPI::eCanceled);
            if (is_ns_rollback)
                event->SetEvent(CJobEvent::eNSSubmitRollback);
            else
                event->SetEvent(CJobEvent::eCancel);
            event->SetTimestamp(current_time);
            event->SetClientNode(client.GetNode());
            event->SetClientSession(client.GetSession());

            job.SetStatus(CNetScheduleAPI::eCanceled);
            job.SetLastTouch(current_time);
            job.Flush(this);

            transaction.Commit();
        }}

        m_StatusTracker.SetStatus(job_id, CNetScheduleAPI::eCanceled);
        if (is_ns_rollback)
            m_StatisticsCounters.CountNSSubmitRollback(1);
        else
            m_StatisticsCounters.CountTransition(old_status,
                                                 CNetScheduleAPI::eCanceled);

        TimeLineRemove(job_id);
        if (old_status == CNetScheduleAPI::eRunning)
            m_ClientsRegistry.ClearExecuting(job_id);
        else if (old_status == CNetScheduleAPI::eReading)
            m_ClientsRegistry.ClearReading(job_id);

        m_GCRegistry.UpdateLifetime(job_id,
                                    job.GetExpirationTime(m_Timeout,
                                                          m_RunTimeout,
                                                          m_ReadTimeout,
                                                          m_PendingTimeout,
                                                          current_time));

        if (job.ShouldNotifyListener(current_time, m_JobsToNotify))
            m_NotificationsList.NotifyJobStatus(job.GetListenerNotifAddr(),
                                                job.GetListenerNotifPort(),
                                                job_key,
                                                job.GetStatus(),
                                                job.GetLastEventIndex());

        // Notify the readers if the job has not been given for reading yet
        // and it was not a rollback due to a socket write error
        if (m_ReadJobs[job_id] == false && is_ns_rollback == false)
            m_NotificationsList.Notify(job_id, job.GetAffinityId(),
                                       m_ClientsRegistry,
                                       m_AffinityRegistry,
                                       m_GroupRegistry,
                                       m_NotifHifreqPeriod,
                                       m_HandicapTimeout,
                                       eRead);
    }}

    if ((old_status == CNetScheduleAPI::eRunning ||
         old_status == CNetScheduleAPI::ePending) &&
         job.ShouldNotifySubmitter(current_time))
        m_NotificationsList.NotifyJobStatus(job.GetSubmAddr(),
                                            job.GetSubmNotifPort(),
                                            job_key,
                                            job.GetStatus(),
                                            job.GetLastEventIndex());

    return old_status;
}


void  CQueue::CancelAllJobs(const CNSClientId &  client,
                            bool                 logging)
{
    vector<CNetScheduleAPI::EJobStatus> statuses;

    // All except cancelled
    statuses.push_back(CNetScheduleAPI::ePending);
    statuses.push_back(CNetScheduleAPI::eRunning);
    statuses.push_back(CNetScheduleAPI::eFailed);
    statuses.push_back(CNetScheduleAPI::eDone);
    statuses.push_back(CNetScheduleAPI::eReading);
    statuses.push_back(CNetScheduleAPI::eConfirmed);
    statuses.push_back(CNetScheduleAPI::eReadFailed);

    CFastMutexGuard     guard(m_OperationLock);
    x_CancelJobs(client, m_StatusTracker.GetJobs(statuses),
                 logging);
    return;
}


void CQueue::x_CancelJobs(const CNSClientId &   client,
                          const TNSBitVector &  jobs_to_cancel,
                          bool                  logging)
{
    CJob                        job;
    CNSPreciseTime              current_time = CNSPreciseTime::Current();
    TNSBitVector::enumerator    en(jobs_to_cancel.first());
    for (; en.valid(); ++en) {
        unsigned int    job_id = *en;
        TJobStatus      old_status = m_StatusTracker.GetStatus(job_id);

        {{
            CNSTransaction              transaction(this);
            if (job.Fetch(this, job_id) != CJob::eJF_Ok) {
                ERR_POST("Cannot fetch job " << DecorateJob(job_id) <<
                         " while cancelling jobs");
                continue;
            }

            CJobEvent *     event = &job.AppendEvent();

            event->SetNodeAddr(client.GetAddress());
            event->SetStatus(CNetScheduleAPI::eCanceled);
            event->SetEvent(CJobEvent::eCancel);
            event->SetTimestamp(current_time);
            event->SetClientNode(client.GetNode());
            event->SetClientSession(client.GetSession());

            job.SetStatus(CNetScheduleAPI::eCanceled);
            job.SetLastTouch(current_time);
            job.Flush(this);

            transaction.Commit();
        }}

        m_StatusTracker.SetStatus(job_id, CNetScheduleAPI::eCanceled);
        m_StatisticsCounters.CountTransition(old_status,
                                             CNetScheduleAPI::eCanceled);

        TimeLineRemove(job_id);
        if (old_status == CNetScheduleAPI::eRunning)
            m_ClientsRegistry.ClearExecuting(job_id);
        else if (old_status == CNetScheduleAPI::eReading)
            m_ClientsRegistry.ClearReading(job_id);

        m_GCRegistry.UpdateLifetime(job_id,
                                    job.GetExpirationTime(m_Timeout,
                                                          m_RunTimeout,
                                                          m_ReadTimeout,
                                                          m_PendingTimeout,
                                                          current_time));

        if ((old_status == CNetScheduleAPI::eRunning ||
             old_status == CNetScheduleAPI::ePending) &&
             job.ShouldNotifySubmitter(current_time))
            m_NotificationsList.NotifyJobStatus(job.GetSubmAddr(),
                                                job.GetSubmNotifPort(),
                                                MakeJobKey(job_id),
                                                job.GetStatus(),
                                                job.GetLastEventIndex());
        if (job.ShouldNotifyListener(current_time, m_JobsToNotify))
            m_NotificationsList.NotifyJobStatus(job.GetListenerNotifAddr(),
                                                job.GetListenerNotifPort(),
                                                MakeJobKey(job_id),
                                                job.GetStatus(),
                                                job.GetLastEventIndex());

        // Notify the readers if the job has not been given for reading yet
        if (m_ReadJobs[job_id] == false)
            m_NotificationsList.Notify(job_id, job.GetAffinityId(),
                                       m_ClientsRegistry,
                                       m_AffinityRegistry,
                                       m_GroupRegistry,
                                       m_NotifHifreqPeriod,
                                       m_HandicapTimeout,
                                       eRead);

        if (logging)
            GetDiagContext().Extra().Print("job_key", MakeJobKey(job_id))
                                    .Print("job_phid", job.GetNCBIPHID());
    }
}


// This function must be called under the operations lock.
// If called for not existing job then an exception is generated
CNSPreciseTime
CQueue::x_GetEstimatedJobLifetime(unsigned int   job_id,
                                  TJobStatus     status) const
{
    if (status == CNetScheduleAPI::eRunning ||
        status == CNetScheduleAPI::eReading)
        return CNSPreciseTime::Current() + GetTimeout();
    return m_GCRegistry.GetLifetime(job_id);
}


void
CQueue::CancelSelectedJobs(const CNSClientId &         client,
                           const string &              group,
                           const string &              aff_token,
                           const vector<TJobStatus> &  job_statuses,
                           bool                        logging,
                           vector<string> &            warnings)
{
    if (group.empty() && aff_token.empty() && job_statuses.empty()) {
        // This possible if there was only 'Canceled' status and
        // it was filtered out. A warning for this case is already produced
        return;
    }

    TNSBitVector        jobs_to_cancel;
    vector<TJobStatus>  statuses;

    if (job_statuses.empty()) {
        // All statuses
        statuses.push_back(CNetScheduleAPI::ePending);
        statuses.push_back(CNetScheduleAPI::eRunning);
        statuses.push_back(CNetScheduleAPI::eCanceled);
        statuses.push_back(CNetScheduleAPI::eFailed);
        statuses.push_back(CNetScheduleAPI::eDone);
        statuses.push_back(CNetScheduleAPI::eReading);
        statuses.push_back(CNetScheduleAPI::eConfirmed);
        statuses.push_back(CNetScheduleAPI::eReadFailed);
    }
    else {
        // The user specified statuses explicitly
        // The list validity is checked by the caller.
        statuses = job_statuses;
    }

    CFastMutexGuard     guard(m_OperationLock);
    jobs_to_cancel = m_StatusTracker.GetJobs(statuses);

    if (!group.empty()) {
        try {
            jobs_to_cancel &= m_GroupRegistry.GetJobs(group);
        } catch (...) {
            jobs_to_cancel.clear();
            warnings.push_back("eGroupNotFound:job group " + group +
                               " is not found");
            if (logging)
                LOG_POST(Message << Warning <<
                         "Job group '" + group +
                         "' is not found. No jobs are canceled.");
        }
    }

    if (!aff_token.empty()) {
        unsigned int    aff_id = m_AffinityRegistry.GetIDByToken(aff_token);
        if (aff_id == 0) {
            jobs_to_cancel.clear();
            warnings.push_back("eAffinityNotFound:" + aff_token +
                               " is not found");
            if (logging)
                LOG_POST(Message << Warning <<
                         "Affinity '" + aff_token +
                         "' is not found. No jobs are canceled.");
        }
        else
            jobs_to_cancel &= m_AffinityRegistry.GetJobsWithAffinity(aff_id);
    }

    x_CancelJobs(client, jobs_to_cancel, logging);
}


TJobStatus  CQueue::GetJobStatus(unsigned int  job_id) const
{
    return m_StatusTracker.GetStatus(job_id);
}


bool CQueue::IsEmpty() const
{
    CFastMutexGuard     guard(m_OperationLock);
    return !m_StatusTracker.AnyJobs();
}


unsigned int CQueue::GetNextId()
{
    CFastMutexGuard     guard(m_LastIdLock);

    // Job indexes are expected to start from 1,
    // the m_LastId is 0 at the very beginning
    ++m_LastId;
    if (m_LastId >= m_SavedId) {
        m_SavedId += s_ReserveDelta;
        if (m_SavedId < m_LastId) {
            // Overflow for the saved ID
            m_LastId = 1;
            m_SavedId = s_ReserveDelta;
        }
        x_UpdateStartFromCounter();
    }
    return m_LastId;
}


// Reserves the given number of the job IDs
unsigned int CQueue::GetNextJobIdForBatch(unsigned int  count)
{
    CFastMutexGuard     guard(m_LastIdLock);

    // Job indexes are expected to start from 1 and be monotonously growing
    unsigned int    start_index = m_LastId;

    m_LastId += count;
    if (m_LastId < start_index ) {
        // Overflow
        m_LastId = count;
        m_SavedId = count + s_ReserveDelta;
        x_UpdateStartFromCounter();
    }

    // There were no overflow, check the reserved value
    if (m_LastId >= m_SavedId) {
        m_SavedId += s_ReserveDelta;
        if (m_SavedId < m_LastId) {
            // Overflow for the saved ID
            m_LastId = count;
            m_SavedId = count + s_ReserveDelta;
        }
        x_UpdateStartFromCounter();
    }

    return m_LastId - count + 1;
}


void CQueue::x_UpdateStartFromCounter(void)
{
    // Updates the start_from value in the Berkley DB file
    if (m_QueueDbBlock) {
        // The only record is expected and its key is always 1
        m_QueueDbBlock->start_from_db.pseudo_key = 1;
        m_QueueDbBlock->start_from_db.start_from = m_SavedId;
        m_QueueDbBlock->start_from_db.UpdateInsert();
    }
    return;
}


unsigned int  CQueue::x_ReadStartFromCounter(void)
{
    // Reads the start_from value from the Berkley DB file
    if (m_QueueDbBlock) {
        // The only record is expected and its key is always 1
        m_QueueDbBlock->start_from_db.pseudo_key = 1;
        m_QueueDbBlock->start_from_db.Fetch();
        return m_QueueDbBlock->start_from_db.start_from;
    }
    return 1;
}


void CQueue::GetJobForReadingOrWait(const CNSClientId &       client,
                                    unsigned int              port,
                                    unsigned int              timeout,
                                    const string &            group,
                                    CJob *                    job,
                                    string *                  affinity,
                                    bool *                    no_more_jobs,
                                    CNSRollbackInterface * &  rollback_action)
{
    CNSPreciseTime                          curr = CNSPreciseTime::Current();
    vector<CNetScheduleAPI::EJobStatus>     from_state;

    // This is a reader command, so mark the node type as a worker
    // node
    m_ClientsRegistry.AppendType(client, CNSClient::eReader);

    from_state.push_back(CNetScheduleAPI::eDone);
    from_state.push_back(CNetScheduleAPI::eFailed);
    from_state.push_back(CNetScheduleAPI::eCanceled);


    {{
        CFastMutexGuard     guard(m_OperationLock);

        // Resolve affinities and groups. It is
        // supposed that the client knows better what affinities and groups to
        // expect i.e. even if they do not exist yet, they may appear
        // soon.
        {{
            CNSTransaction      transaction(this);

            m_GroupRegistry.ResolveGroup(group);
            // Uncomment when READ starts supporting affinities
            // aff_ids = m_AffinityRegistry.ResolveAffinities(*aff_list);
            transaction.Commit();
        }}

        x_UnregisterReadListener(client);

        TNSBitVector        candidates = m_StatusTracker.GetJobs(from_state);

        // Exclude blacklisted jobs
        candidates -= m_ClientsRegistry.GetBlacklistedJobs(client);

        if (!group.empty())
            // Apply restrictions on the group jobs if so
            candidates &= m_GroupRegistry.GetJobs(group, false);

        // Exclude those jobs which have been read or in a process of reading
        candidates -= m_ReadJobs;

        if (!candidates.any()) {
            // No jobs for reading now however there could be in the future.
            *affinity = "";
            *no_more_jobs = x_NoMoreReadJobs(group);
            if (timeout != 0 and port > 0)
                x_RegisterReadListener(client, port, timeout,
                                       TNSBitVector(), false, true, false,
                                       group);
            return;
        }

        unsigned int        job_id = *candidates.first();
        TJobStatus          old_status = GetJobStatus(job_id);
        {{
            CNSTransaction     transaction(this);

            // Fetch the job and update it and flush
            job->Fetch(this, job_id);

            unsigned int    read_count = job->GetReadCount() + 1;
            CJobEvent *     event = &job->AppendEvent();

            event->SetStatus(CNetScheduleAPI::eReading);
            event->SetEvent(CJobEvent::eRead);
            event->SetTimestamp(curr);
            event->SetNodeAddr(client.GetAddress());
            event->SetClientNode(client.GetNode());
            event->SetClientSession(client.GetSession());

            job->SetReadTimeout(CNSPreciseTime());
            job->SetStatus(CNetScheduleAPI::eReading);
            job->SetReadCount(read_count);
            job->SetLastTouch(curr);
            job->Flush(this);

            transaction.Commit();
        }}

        TimeLineAdd(job_id, curr + m_ReadTimeout);

        // Update the memory cache
        m_StatusTracker.SetStatus(job_id, CNetScheduleAPI::eReading);
        m_StatisticsCounters.CountTransition(old_status,
                                             CNetScheduleAPI::eReading);
        m_ClientsRegistry.AddToReading(client, job_id);

        m_GCRegistry.UpdateLifetime(job_id,
                                    job->GetExpirationTime(m_Timeout,
                                                           m_RunTimeout,
                                                           m_ReadTimeout,
                                                           m_PendingTimeout,
                                                           curr));

        if (job->ShouldNotifyListener(curr, m_JobsToNotify))
            m_NotificationsList.NotifyJobStatus(job->GetListenerNotifAddr(),
                                                job->GetListenerNotifPort(),
                                                MakeJobKey(job_id),
                                                job->GetStatus(),
                                                job->GetLastEventIndex());

        rollback_action = new CNSReadJobRollback(client, job_id, old_status);
    }}

    if (job->GetAffinityId() == 0)
        *affinity = "";
    else
        *affinity = m_AffinityRegistry.GetTokenByID(job->GetAffinityId());

    *no_more_jobs = false;  // There is at least one - the one given
    m_ReadJobs.set_bit(job->GetId());
    ++m_ReadJobsOps;
    return;
}


TJobStatus  CQueue::ConfirmReadingJob(const CNSClientId &  client,
                                      unsigned int         job_id,
                                      const string &       job_key,
                                      CJob &               job,
                                      const string &       auth_token)
{
    TJobStatus      old_status = x_ChangeReadingStatus(
                                                client, job_id, job_key,
                                                job, auth_token, "",
                                                CNetScheduleAPI::eConfirmed,
                                                false, false);
    m_ClientsRegistry.ClearReading(client, job_id);
    return old_status;
}


TJobStatus  CQueue::FailReadingJob(const CNSClientId &  client,
                                   unsigned int         job_id,
                                   const string &       job_key,
                                   CJob &               job,
                                   const string &       auth_token,
                                   const string &       err_msg,
                                   bool                 no_retries)
{
    TJobStatus      old_status = x_ChangeReadingStatus(
                                                client, job_id, job_key,
                                                job, auth_token, err_msg,
                                                CNetScheduleAPI::eReadFailed,
                                                false, no_retries);
    m_ClientsRegistry.ClearReadingSetBlacklist(job_id);
    return old_status;
}


TJobStatus  CQueue::ReturnReadingJob(const CNSClientId &  client,
                                     unsigned int         job_id,
                                     const string &       job_key,
                                     CJob &               job,
                                     const string &       auth_token,
                                     bool                 is_ns_rollback,
                                     TJobStatus           target_status)
{
    TJobStatus      old_status = x_ChangeReadingStatus(
                                                client, job_id, job_key,
                                                job, auth_token, "",
                                                target_status,
                                                is_ns_rollback,
                                                false);
    if (is_ns_rollback)
        m_ClientsRegistry.ClearReading(job_id);
    else
        m_ClientsRegistry.ClearReadingSetBlacklist(job_id);
    return old_status;
}


TJobStatus  CQueue::x_ChangeReadingStatus(const CNSClientId &  client,
                                          unsigned int         job_id,
                                          const string &       job_key,
                                          CJob &               job,
                                          const string &       auth_token,
                                          const string &       err_msg,
                                          TJobStatus           target_status,
                                          bool                 is_ns_rollback,
                                          bool                 no_retries)
{
    CNSPreciseTime                              current_time =
                                                    CNSPreciseTime::Current();
    CStatisticsCounters::ETransitionPathOption  path_option =
                                                    CStatisticsCounters::eNone;
    CFastMutexGuard                             guard(m_OperationLock);
    TJobStatus                                  old_status =
                                                    GetJobStatus(job_id);

    if (old_status != CNetScheduleAPI::eReading)
        return old_status;

    {{
        CNSTransaction      transaction(this);

        if (job.Fetch(this, job_id) != CJob::eJF_Ok)
            NCBI_THROW(CNetScheduleException, eInternalError,
                       "Error fetching job");

        // Check that authorization token matches
        if (is_ns_rollback == false) {
            CJob::EAuthTokenCompareResult   token_compare_result =
                                            job.CompareAuthToken(auth_token);
            if (token_compare_result == CJob::eInvalidTokenFormat)
                NCBI_THROW(CNetScheduleException, eInvalidAuthToken,
                           "Invalid authorization token format");
            if (token_compare_result == CJob::eNoMatch)
                NCBI_THROW(CNetScheduleException, eInvalidAuthToken,
                           "Authorization token does not match");
        }

        // Sanity check of the current job state
        if (job.GetStatus() != CNetScheduleAPI::eReading)
            NCBI_THROW(CNetScheduleException, eInternalError,
                "Internal inconsistency detected. The job state in memory is " +
                CNetScheduleAPI::StatusToString(CNetScheduleAPI::eReading) +
                " while in database it is " +
                CNetScheduleAPI::StatusToString(job.GetStatus()));

        if (target_status == CNetScheduleAPI::eJobNotFound)
            target_status = job.GetStatusBeforeReading();


        // Add an event
        CJobEvent &     event = job.AppendEvent();
        event.SetTimestamp(current_time);
        event.SetNodeAddr(client.GetAddress());
        event.SetClientNode(client.GetNode());
        event.SetClientSession(client.GetSession());
        event.SetErrorMsg(err_msg);

        if (is_ns_rollback) {
            event.SetEvent(CJobEvent::eNSReadRollback);
            job.SetReadCount(job.GetReadCount() - 1);
        } else {
            switch (target_status) {
                case CNetScheduleAPI::eFailed:
                case CNetScheduleAPI::eDone:
                case CNetScheduleAPI::eCanceled:
                    event.SetEvent(CJobEvent::eReadRollback);
                    job.SetReadCount(job.GetReadCount() - 1);
                    break;
                case CNetScheduleAPI::eReadFailed:
                    if (no_retries) {
                        event.SetEvent(CJobEvent::eReadFinalFail);
                    } else {
                        event.SetEvent(CJobEvent::eReadFail);
                        // Check the number of tries first
                        if (job.GetReadCount() <= m_FailedRetries) {
                            // The job needs to be re-scheduled for reading
                            target_status = CNetScheduleAPI::eDone;
                            path_option = CStatisticsCounters::eFail;
                        }
                    }
                    break;
                case CNetScheduleAPI::eConfirmed:
                    event.SetEvent(CJobEvent::eReadDone);
                    break;
                default:
                    _ASSERT(0);
                    break;
            }
        }

        event.SetStatus(target_status);
        job.SetStatus(target_status);
        job.SetLastTouch(current_time);

        job.Flush(this);
        transaction.Commit();
    }}

    if (target_status != CNetScheduleAPI::eConfirmed &&
        target_status != CNetScheduleAPI::eReadFailed) {
        m_ReadJobs.set_bit(job_id, false);
        ++m_ReadJobsOps;

        // Notify the readers
        m_NotificationsList.Notify(job_id, job.GetAffinityId(),
                                   m_ClientsRegistry,
                                   m_AffinityRegistry,
                                   m_GroupRegistry,
                                   m_NotifHifreqPeriod,
                                   m_HandicapTimeout,
                                   eRead);
    }

    TimeLineRemove(job_id);

    m_StatusTracker.SetStatus(job_id, target_status);
    m_GCRegistry.UpdateLifetime(job_id, job.GetExpirationTime(m_Timeout,
                                                              m_RunTimeout,
                                                              m_ReadTimeout,
                                                              m_PendingTimeout,
                                                              current_time));
    if (is_ns_rollback)
        m_StatisticsCounters.CountNSReadRollback(1);
    else
        m_StatisticsCounters.CountTransition(CNetScheduleAPI::eReading,
                                             target_status,
                                             path_option);
    if (job.ShouldNotifyListener(current_time, m_JobsToNotify))
        m_NotificationsList.NotifyJobStatus(job.GetListenerNotifAddr(),
                                            job.GetListenerNotifPort(),
                                            job_key,
                                            job.GetStatus(),
                                            job.GetLastEventIndex());
    return CNetScheduleAPI::eReading;
}


// This function is called from places where the operations lock has been
// already taken. So there is no lock around memory status tracker
void CQueue::EraseJob(unsigned int  job_id)
{
    m_StatusTracker.Erase(job_id);

    {{
        // Request delayed record delete
        CFastMutexGuard     jtd_guard(m_JobsToDeleteLock);

        m_JobsToDelete.set_bit(job_id);
        ++m_JobsToDeleteOps;
    }}
    TimeLineRemove(job_id);
}


void CQueue::x_Erase(const TNSBitVector &  job_ids)
{
    CFastMutexGuard     jtd_guard(m_JobsToDeleteLock);

    m_JobsToDelete |= job_ids;
    m_JobsToDeleteOps += job_ids.count();
}


void CQueue::OptimizeMem()
{
    m_StatusTracker.OptimizeMem();
}


CQueue::x_SJobPick
CQueue::x_FindPendingJob(const CNSClientId  &  client,
                         const TNSBitVector &  aff_ids,
                         bool                  wnode_affinity,
                         bool                  any_affinity,
                         bool                  exclusive_new_affinity,
                         const string &        group)
{
    x_SJobPick      job_pick = { 0, false, 0 };
    bool            explicit_aff = aff_ids.any();
    unsigned int    wnode_aff_candidate = 0;
    unsigned int    exclusive_aff_candidate = 0;
    TNSBitVector    blacklisted_jobs =
                    m_ClientsRegistry.GetBlacklistedJobs(client);
    TNSBitVector    group_jobs = m_GroupRegistry.GetJobs(group, false);

    TNSBitVector    pref_aff;
    if (wnode_affinity) {
        pref_aff = m_ClientsRegistry.GetPreferredAffinities(client);
        wnode_affinity = wnode_affinity && pref_aff.any();
    }

    TNSBitVector    all_pref_aff;
    if (exclusive_new_affinity)
        all_pref_aff = m_ClientsRegistry.GetAllPreferredAffinities();

    if (explicit_aff || wnode_affinity || exclusive_new_affinity) {
        // Check all pending jobs
        TNSBitVector                pending_jobs =
                            m_StatusTracker.GetJobs(CNetScheduleAPI::ePending);
        TNSBitVector::enumerator    en(pending_jobs.first());


        for (; en.valid(); ++en) {
            unsigned int    job_id = *en;

            if (blacklisted_jobs[job_id] == true)
                continue;

            if (!group.empty()) {
                if (group_jobs[job_id] == false)
                    continue;
            }

            unsigned int    aff_id = m_GCRegistry.GetAffinityID(job_id);
            if (aff_id != 0 && explicit_aff) {
                if (aff_ids[aff_id] == true) {
                    job_pick.job_id = job_id;
                    job_pick.exclusive = false;
                    job_pick.aff_id = aff_id;
                    return job_pick;
                }
            }

            if (aff_id != 0 && wnode_affinity) {
                if (pref_aff[aff_id] == true) {
                    if (explicit_aff == false) {
                        job_pick.job_id = job_id;
                        job_pick.exclusive = false;
                        job_pick.aff_id = aff_id;
                        return job_pick;
                    }
                    if (wnode_aff_candidate == 0)
                        wnode_aff_candidate = job_id;
                    continue;
                }
            }

            if (exclusive_new_affinity) {
                if (aff_id == 0 || all_pref_aff[aff_id] == false) {
                    if (explicit_aff == false && wnode_affinity == false) {
                        job_pick.job_id = job_id;
                        job_pick.exclusive = true;
                        job_pick.aff_id = aff_id;
                        return job_pick;
                    }
                    if (exclusive_aff_candidate == 0)
                        exclusive_aff_candidate = job_id;
                }
            }
        }

        if (wnode_aff_candidate != 0) {
            job_pick.job_id = wnode_aff_candidate;
            job_pick.exclusive = false;
            job_pick.aff_id = 0;
            return job_pick;
        }

        if (exclusive_aff_candidate != 0) {
            job_pick.job_id = exclusive_aff_candidate;
            job_pick.exclusive = true;
            job_pick.aff_id = m_GCRegistry.GetAffinityID(
                                                exclusive_aff_candidate);
            return job_pick;
        }
    }


    if (any_affinity ||
        (!explicit_aff && !wnode_affinity && !exclusive_new_affinity)) {
        job_pick.job_id = m_StatusTracker.GetJobByStatus(
                                    CNetScheduleAPI::ePending,
                                    blacklisted_jobs,
                                    group_jobs, !group.empty());
        job_pick.exclusive = false;
        job_pick.aff_id = 0;
        return job_pick;
    }

    return job_pick;
}


CQueue::x_SJobPick
CQueue::x_FindOutdatedPendingJob(const CNSClientId &  client,
                                 unsigned int         picked_earlier)
{
    x_SJobPick      job_pick = { 0, false, 0 };

    if (m_MaxPendingWaitTimeout.tv_sec == 0 &&
        m_MaxPendingWaitTimeout.tv_nsec == 0)
        return job_pick;    // Not configured

    TNSBitVector    outdated_pending =
                        m_StatusTracker.GetOutdatedPendingJobs(
                                m_MaxPendingWaitTimeout,
                                m_GCRegistry);
    if (picked_earlier != 0)
        outdated_pending.set_bit(picked_earlier, false);

    if (!outdated_pending.any())
        return job_pick;

    outdated_pending -= m_ClientsRegistry.GetBlacklistedJobs(client);

    if (!outdated_pending.any())
        return job_pick;

    job_pick.job_id = *outdated_pending.first();
    job_pick.aff_id = m_GCRegistry.GetAffinityID(job_pick.job_id);
    job_pick.exclusive = job_pick.aff_id != 0;
    return job_pick;
}


string CQueue::GetAffinityList(const CNSClientId &    client)
{
    m_ClientsRegistry.MarkAsAdmin(client);

    list< SAffinityStatistics >
                statistics = m_AffinityRegistry.GetAffinityStatistics(
                                                            m_StatusTracker);

    string          aff_list;
    for (list< SAffinityStatistics >::const_iterator  k = statistics.begin();
         k != statistics.end(); ++k) {
        if (!aff_list.empty())
            aff_list += "&";

        aff_list += NStr::URLEncode(k->m_Token) + '=' +
                    NStr::NumericToString(k->m_NumberOfPendingJobs) + "," +
                    NStr::NumericToString(k->m_NumberOfRunningJobs) + "," +
                    NStr::NumericToString(k->m_NumberOfPreferred) + "," +
                    NStr::NumericToString(k->m_NumberOfWaitGet);
    }
    return aff_list;
}


TJobStatus CQueue::FailJob(const CNSClientId &    client,
                           unsigned int           job_id,
                           const string &         job_key,
                           CJob &                 job,
                           const string &         auth_token,
                           const string &         err_msg,
                           const string &         output,
                           int                    ret_code,
                           bool                   no_retries,
                           string                 warning)
{
    unsigned        failed_retries;
    unsigned        max_output_size;
    {{
        CQueueParamAccessor     qp(*this);
        failed_retries  = qp.GetFailedRetries();
        max_output_size = qp.GetMaxOutputSize();
    }}

    if (output.size() > max_output_size) {
        NCBI_THROW(CNetScheduleException, eDataTooLong,
                   "Output is too long");
    }

    CNSPreciseTime      curr = CNSPreciseTime::Current();
    bool                rescheduled = false;
    TJobStatus          old_status;

    {{
        CFastMutexGuard     guard(m_OperationLock);
        TJobStatus          new_status = CNetScheduleAPI::eFailed;

        old_status = GetJobStatus(job_id);
        if (old_status == CNetScheduleAPI::eFailed) {
            m_StatisticsCounters.CountTransition(CNetScheduleAPI::eFailed,
                                                 CNetScheduleAPI::eFailed);
            return old_status;
        }

        if (old_status != CNetScheduleAPI::eRunning) {
            // No job state change
            return old_status;
        }

        {{
            CNSTransaction     transaction(this);

            if (job.Fetch(this, job_id) != CJob::eJF_Ok)
                NCBI_THROW(CNetScheduleException, eInternalError,
                           "Error fetching job");

            if (!auth_token.empty()) {
                // Need to check authorization token first
                CJob::EAuthTokenCompareResult   token_compare_result =
                                            job.CompareAuthToken(auth_token);
                if (token_compare_result == CJob::eInvalidTokenFormat)
                    NCBI_THROW(CNetScheduleException, eInvalidAuthToken,
                               "Invalid authorization token format");
                if (token_compare_result == CJob::eNoMatch)
                    NCBI_THROW(CNetScheduleException, eInvalidAuthToken,
                               "Authorization token does not match");
                if (token_compare_result == CJob::ePassportOnlyMatch) {
                    // That means the job has been given to another worker node
                    // by whatever reason (expired/failed/returned before)
                    LOG_POST(Message << Warning << "Received FPUT2 with only "
                                                   "passport matched.");
                    warning = "eJobPassportOnlyMatch:Only job passport "
                              "matched. Command is ignored.";
                    return old_status;
                }
                // Here: the authorization token is OK, we can continue
            }

            CJobEvent *     event = job.GetLastEvent();
            if (!event)
                ERR_POST("No JobEvent for running job");

            event = &job.AppendEvent();
            if (no_retries)
                event->SetEvent(CJobEvent::eFinalFail);
            else
                event->SetEvent(CJobEvent::eFail);
            event->SetStatus(CNetScheduleAPI::eFailed);
            event->SetTimestamp(curr);
            event->SetErrorMsg(err_msg);
            event->SetRetCode(ret_code);
            event->SetNodeAddr(client.GetAddress());
            event->SetClientNode(client.GetNode());
            event->SetClientSession(client.GetSession());

            if (no_retries) {
                job.SetStatus(CNetScheduleAPI::eFailed);
                event->SetStatus(CNetScheduleAPI::eFailed);
                rescheduled = false;
                if (m_Log)
                    LOG_POST(Message << Warning << "Job failed "
                                        "unconditionally, no_retries = 1");
            } else {
                unsigned                run_count = job.GetRunCount();
                if (run_count <= failed_retries) {
                    job.SetStatus(CNetScheduleAPI::ePending);
                    event->SetStatus(CNetScheduleAPI::ePending);

                    new_status = CNetScheduleAPI::ePending;

                    rescheduled = true;
                } else {
                    job.SetStatus(CNetScheduleAPI::eFailed);
                    event->SetStatus(CNetScheduleAPI::eFailed);
                    new_status = CNetScheduleAPI::eFailed;
                    rescheduled = false;
                    if (m_Log)
                        LOG_POST(Message << Warning << "Job failed, exceeded "
                                            "max number of retries ("
                                         << failed_retries << ")");
                }
            }

            job.SetOutput(output);
            job.SetLastTouch(curr);
            job.Flush(this);
            transaction.Commit();
        }}

        m_StatusTracker.SetStatus(job_id, new_status);
        if (new_status == CNetScheduleAPI::ePending)
            m_StatisticsCounters.CountTransition(CNetScheduleAPI::eRunning,
                                                 new_status,
                                                 CStatisticsCounters::eFail);
        else
            m_StatisticsCounters.CountTransition(CNetScheduleAPI::eRunning,
                                                 new_status,
                                                 CStatisticsCounters::eNone);

        TimeLineRemove(job_id);

        // Replace it with ClearExecuting(client, job_id) when all clients
        // provide their credentials and job passport is checked strictly
        m_ClientsRegistry.ClearExecuting(job_id);
        m_ClientsRegistry.AddToBlacklist(client, job_id);

        m_GCRegistry.UpdateLifetime(job_id,
                                    job.GetExpirationTime(m_Timeout,
                                                          m_RunTimeout,
                                                          m_ReadTimeout,
                                                          m_PendingTimeout,
                                                          curr));

        if (!rescheduled  &&  job.ShouldNotifySubmitter(curr)) {
            m_NotificationsList.NotifyJobStatus(job.GetSubmAddr(),
                                                job.GetSubmNotifPort(),
                                                job_key,
                                                job.GetStatus(),
                                                job.GetLastEventIndex());
        }

        if (rescheduled && m_PauseStatus == eNoPause)
            m_NotificationsList.Notify(job_id,
                                       job.GetAffinityId(), m_ClientsRegistry,
                                       m_AffinityRegistry, m_GroupRegistry,
                                       m_NotifHifreqPeriod, m_HandicapTimeout,
                                       eGet);

        if (new_status == CNetScheduleAPI::eFailed)
            if (m_ReadJobs[job_id] == false)
                m_NotificationsList.Notify(job_id, job.GetAffinityId(),
                                           m_ClientsRegistry,
                                           m_AffinityRegistry,
                                           m_GroupRegistry,
                                           m_NotifHifreqPeriod,
                                           m_HandicapTimeout,
                                           eRead);

        if (job.ShouldNotifyListener(curr, m_JobsToNotify))
            m_NotificationsList.NotifyJobStatus(job.GetListenerNotifAddr(),
                                                job.GetListenerNotifPort(),
                                                job_key,
                                                job.GetStatus(),
                                                job.GetLastEventIndex());
    }}

    return old_status;
}


string  CQueue::GetAffinityTokenByID(unsigned int  aff_id) const
{
    return m_AffinityRegistry.GetTokenByID(aff_id);
}


void CQueue::ClearWorkerNode(const CNSClientId &  client,
                             bool &               client_was_found,
                             string &             old_session,
                             bool &               pref_affs_were_reset)
{
    // Get the running and reading jobs and move them to the corresponding
    // states (pending and done)

    TNSBitVector    running_jobs;
    TNSBitVector    reading_jobs;

    {{
        CFastMutexGuard     guard(m_OperationLock);
        unsigned short      wait_port = m_ClientsRegistry.ClearWorkerNode(
                                                client,
                                                m_AffinityRegistry,
                                                running_jobs,
                                                reading_jobs,
                                                client_was_found,
                                                old_session,
                                                pref_affs_were_reset);

        if (wait_port != 0)
            m_NotificationsList.UnregisterListener(client, wait_port);
    }}

    if (running_jobs.any())
        x_ResetRunningDueToClear(client, running_jobs);
    if (reading_jobs.any())
        x_ResetReadingDueToClear(client, reading_jobs);
    return;
}


// Triggered from a notification thread only
void CQueue::NotifyListenersPeriodically(const CNSPreciseTime &  current_time)
{
    if (m_MaxPendingWaitTimeout.tv_sec != 0 ||
        m_MaxPendingWaitTimeout.tv_nsec != 0) {
        // Pending outdated timeout is configured, so check for
        // outdated jobs
        CFastMutexGuard     guard(m_OperationLock);
        TNSBitVector        outdated_pending =
                                    m_StatusTracker.GetOutdatedPendingJobs(
                                        m_MaxPendingWaitTimeout,
                                        m_GCRegistry);
        if (outdated_pending.any())
            m_NotificationsList.CheckOutdatedJobs(outdated_pending,
                                                  m_ClientsRegistry,
                                                  m_NotifHifreqPeriod);
    }


    // Check the configured notification interval
    static CNSPreciseTime   last_notif_timeout = CNSPreciseTime::Never();
    static size_t           skip_limit = 0;
    static size_t           skip_count;

    if (m_NotifHifreqInterval != last_notif_timeout) {
        last_notif_timeout = m_NotifHifreqInterval;
        skip_count = 0;
        skip_limit = size_t(m_NotifHifreqInterval/0.1);
    }

    ++skip_count;
    if (skip_count < skip_limit)
        return;

    skip_count = 0;

    // The NotifyPeriodically() and CheckTimeout() calls may need to modify
    // the clients and affinity registry so it is safer to take the queue lock.
    CFastMutexGuard     guard(m_OperationLock);
    if (m_StatusTracker.AnyPending())
        m_NotificationsList.NotifyPeriodically(current_time,
                                               m_NotifLofreqMult,
                                               m_ClientsRegistry,
                                               m_AffinityRegistry);
    else
        m_NotificationsList.CheckTimeout(current_time,
                                         m_ClientsRegistry,
                                         m_AffinityRegistry);
}


CNSPreciseTime CQueue::NotifyExactListeners(void)
{
    return m_NotificationsList.NotifyExactListeners();
}


string CQueue::PrintClientsList(bool verbose) const
{
    return m_ClientsRegistry.PrintClientsList(this, m_AffinityRegistry,
                                              m_DumpClientBufferSize, verbose);
}


string CQueue::PrintNotificationsList(bool verbose) const
{
    return m_NotificationsList.Print(m_ClientsRegistry, m_AffinityRegistry,
                                     verbose);
}


string CQueue::PrintAffinitiesList(bool verbose) const
{
    return m_AffinityRegistry.Print(this, m_ClientsRegistry,
                                    m_DumpAffBufferSize, verbose);
}


string CQueue::PrintGroupsList(bool verbose) const
{
    return m_GroupRegistry.Print(this, m_DumpGroupBufferSize, verbose);
}


void CQueue::CheckExecutionTimeout(bool  logging)
{
    if (!m_RunTimeLine)
        return;

    CNSPreciseTime  queue_run_timeout = GetRunTimeout();
    CNSPreciseTime  queue_read_timeout = GetReadTimeout();
    CNSPreciseTime  curr = CNSPreciseTime::Current();
    TNSBitVector    bv;
    {{
        CReadLockGuard  guard(m_RunTimeLineLock);
        m_RunTimeLine->ExtractObjects(curr.Sec(), &bv);
    }}

    TNSBitVector::enumerator en(bv.first());
    for ( ;en.valid(); ++en) {
        x_CheckExecutionTimeout(queue_run_timeout, queue_read_timeout,
                                *en, curr, logging);
    }
}


void CQueue::x_CheckExecutionTimeout(const CNSPreciseTime &  queue_run_timeout,
                                     const CNSPreciseTime &  queue_read_timeout,
                                     unsigned int            job_id,
                                     const CNSPreciseTime &  curr_time,
                                     bool                    logging)
{
    CJob                    job;
    CNSPreciseTime          time_start = CNSPreciseTime();
    CNSPreciseTime          run_timeout = CNSPreciseTime();
    CNSPreciseTime          read_timeout = CNSPreciseTime();
    CNSPreciseTime          exp_time = CNSPreciseTime();
    TJobStatus              status;
    TJobStatus              new_status;
    CJobEvent::EJobEvent    event_type;


    {{
        CFastMutexGuard         guard(m_OperationLock);

        status = GetJobStatus(job_id);
        if (status == CNetScheduleAPI::eRunning) {
            new_status = CNetScheduleAPI::ePending;
            event_type = CJobEvent::eTimeout;
        } else if (status == CNetScheduleAPI::eReading) {
            new_status = CNetScheduleAPI::eDone;
            event_type = CJobEvent::eReadTimeout;
        } else
            return; // Execution timeout is for Running and Reading jobs only


        {{
            CNSTransaction      transaction(this);

            if (job.Fetch(this, job_id) != CJob::eJF_Ok)
                return;

            CJobEvent *     event = job.GetLastEvent();
            time_start = event->GetTimestamp();
            run_timeout = job.GetRunTimeout();
            if (run_timeout == CNSPreciseTime())
                run_timeout = queue_run_timeout;

            if (status == CNetScheduleAPI::eRunning &&
                run_timeout == CNSPreciseTime())
                // 0 timeout means the job never fails
                return;

            read_timeout = job.GetReadTimeout();
            if (read_timeout == CNSPreciseTime())
                read_timeout = queue_read_timeout;

            if (status == CNetScheduleAPI::eReading &&
                read_timeout == CNSPreciseTime())
                // 0 timeout means the job never fails
                return;

            // Calculate the expiration time
            if (status == CNetScheduleAPI::eRunning)
                exp_time = time_start + run_timeout;
            else
                exp_time = time_start + read_timeout;

            if (curr_time < exp_time) {
                // we need to register job in time line
                TimeLineAdd(job_id, exp_time);
                return;
            }

            // The job timeout (running or reading) is expired.
            // Check the try counter, we may need to fail the job.
            if (status == CNetScheduleAPI::eRunning) {
                // Running state
                if (job.GetRunCount() > m_FailedRetries)
                    new_status = CNetScheduleAPI::eFailed;
            } else {
                // Reading state
                if (job.GetReadCount() > m_FailedRetries)
                    new_status = CNetScheduleAPI::eReadFailed;
                else
                    new_status = job.GetStatusBeforeReading();
                m_ReadJobs.set_bit(job_id, false);
                ++m_ReadJobsOps;
            }

            job.SetStatus(new_status);
            job.SetLastTouch(curr_time);

            event = &job.AppendEvent();
            event->SetStatus(new_status);
            event->SetEvent(event_type);
            event->SetTimestamp(curr_time);

            job.Flush(this);
            transaction.Commit();
        }}


        m_StatusTracker.SetStatus(job_id, new_status);
        m_GCRegistry.UpdateLifetime(job_id,
                                    job.GetExpirationTime(m_Timeout,
                                                          m_RunTimeout,
                                                          m_ReadTimeout,
                                                          m_PendingTimeout,
                                                          curr_time));

        if (status == CNetScheduleAPI::eRunning) {
            if (new_status == CNetScheduleAPI::ePending) {
                // Timeout and reschedule, put to blacklist as well
                m_ClientsRegistry.ClearExecutingSetBlacklist(job_id);
                m_StatisticsCounters.CountTransition(
                                            CNetScheduleAPI::eRunning,
                                            CNetScheduleAPI::ePending,
                                            CStatisticsCounters::eTimeout);
            } else {
                m_ClientsRegistry.ClearExecuting(job_id);
                m_StatisticsCounters.CountTransition(
                                            CNetScheduleAPI::eRunning,
                                            CNetScheduleAPI::eFailed,
                                            CStatisticsCounters::eTimeout);
            }
        } else {
            if (new_status == CNetScheduleAPI::eReadFailed) {
                m_ClientsRegistry.ClearReading(job_id);
                m_StatisticsCounters.CountTransition(
                                            CNetScheduleAPI::eReading,
                                            CNetScheduleAPI::eReadFailed,
                                            CStatisticsCounters::eTimeout);
            } else {
                // The target status could be Done, Failed, Canceled.
                // The job could be read again by another reader.
                m_ClientsRegistry.ClearReadingSetBlacklist(job_id);
                m_StatisticsCounters.CountTransition(
                                            CNetScheduleAPI::eReading,
                                            new_status,
                                            CStatisticsCounters::eTimeout);
            }
        }

        if (new_status == CNetScheduleAPI::ePending &&
            m_PauseStatus == eNoPause)
            m_NotificationsList.Notify(job_id,
                                       job.GetAffinityId(), m_ClientsRegistry,
                                       m_AffinityRegistry, m_GroupRegistry,
                                       m_NotifHifreqPeriod, m_HandicapTimeout,
                                       eGet);

        if (new_status == CNetScheduleAPI::eDone ||
            new_status == CNetScheduleAPI::eFailed ||
            new_status == CNetScheduleAPI::eCanceled)
            if (m_ReadJobs[job_id] == false)
                m_NotificationsList.Notify(job_id, job.GetAffinityId(),
                                           m_ClientsRegistry,
                                           m_AffinityRegistry,
                                           m_GroupRegistry,
                                           m_NotifHifreqPeriod,
                                           m_HandicapTimeout,
                                           eRead);

        if (job.ShouldNotifyListener(curr_time, m_JobsToNotify))
            m_NotificationsList.NotifyJobStatus(job.GetListenerNotifAddr(),
                                                job.GetListenerNotifPort(),
                                                MakeJobKey(job_id),
                                                job.GetStatus(),
                                                job.GetLastEventIndex());
    }}

    if (status == CNetScheduleAPI::eRunning &&
        new_status == CNetScheduleAPI::eFailed &&
        job.ShouldNotifySubmitter(curr_time))
        m_NotificationsList.NotifyJobStatus(job.GetSubmAddr(),
                                            job.GetSubmNotifPort(),
                                            MakeJobKey(job_id),
                                            job.GetStatus(),
                                            job.GetLastEventIndex());

    if (logging)
    {
        string      purpose;
        if (status == CNetScheduleAPI::eRunning)
            purpose = "execution";
        else
            purpose = "reading";

        GetDiagContext().Extra()
                .Print("msg", "Timeout expired, rescheduled for " + purpose)
                .Print("msg_code", "410")   // The code is for
                                            // searching in applog
                .Print("job_key", MakeJobKey(job_id))
                .Print("queue", m_QueueName)
                .Print("run_counter", job.GetRunCount())
                .Print("read_counter", job.GetReadCount())
                .Print("time_start", NS_FormatPreciseTime(time_start))
                .Print("exp_time", NS_FormatPreciseTime(exp_time))
                .Print("run_timeout", run_timeout)
                .Print("read_timeout", read_timeout);
    }
}


// Checks up to given # of jobs at the given status for expiration and
// marks up to given # of jobs for deletion.
// Returns the # of performed scans, the # of jobs marked for deletion and
// the last scanned job id.
SPurgeAttributes
CQueue::CheckJobsExpiry(const CNSPreciseTime &  current_time,
                        SPurgeAttributes        attributes,
                        unsigned int            last_job,
                        TJobStatus              status)
{
    TNSBitVector        job_ids;
    SPurgeAttributes    result;
    unsigned int        job_id;
    unsigned int        aff;
    unsigned int        group;

    result.job_id = attributes.job_id;
    result.deleted = 0;
    {{
        CFastMutexGuard     guard(m_OperationLock);

        for (result.scans = 0;
             result.scans < attributes.scans; ++result.scans) {
            job_id = m_StatusTracker.GetNext(status, result.job_id);
            if (job_id == 0)
                break;  // No more jobs in the state
            if (last_job != 0 && job_id >= last_job)
                break;  // The job in the state is above the limit

            result.job_id = job_id;

            if (m_GCRegistry.DeleteIfTimedOut(job_id, current_time,
                                              &aff, &group))
            {
                // The job is expired and needs to be marked for deletion
                m_StatusTracker.Erase(job_id);
                job_ids.set_bit(job_id);
                ++result.deleted;

                // check if the affinity should also be updated
                if (aff != 0)
                    m_AffinityRegistry.RemoveJobFromAffinity(job_id, aff);

                // Check if the group registry should also be updated
                if (group != 0)
                    m_GroupRegistry.RemoveJob(group, job_id);

                if (result.deleted >= attributes.deleted)
                    break;
            }
        }
    }}

    if (result.deleted > 0) {
        x_Erase(job_ids);

        CFastMutexGuard     guard(m_OperationLock);
        TNSBitVector        jobs_to_notify = m_JobsToNotify & job_ids;
        if (jobs_to_notify.any()) {
            TNSBitVector::enumerator    en(jobs_to_notify.first());
            CNSTransaction              transaction(this);

            for (; en.valid(); ++en) {
                unsigned int    id = *en;
                CJob            job;

                if (job.Fetch(this, id) == CJob::eJF_Ok) {
                    if (job.ShouldNotifyListener(current_time, m_JobsToNotify))
                        m_NotificationsList.NotifyJobStatus(
                                            job.GetListenerNotifAddr(),
                                            job.GetListenerNotifPort(),
                                            MakeJobKey(id),
                                            CNetScheduleAPI::eDeleted,
                                            job.GetLastEventIndex());

                }
                m_JobsToNotify.set_bit(id, false);
            }
        }

        if (!m_StatusTracker.AnyPending())
            m_NotificationsList.ClearExactGetNotifications();
    }


    return result;
}


void CQueue::TimeLineMove(unsigned int            job_id,
                          const CNSPreciseTime &  old_time,
                          const CNSPreciseTime &  new_time)
{
    if (!job_id || !m_RunTimeLine)
        return;

    CWriteLockGuard guard(m_RunTimeLineLock);
    m_RunTimeLine->MoveObject(old_time.Sec(), new_time.Sec(), job_id);
}


void CQueue::TimeLineAdd(unsigned int            job_id,
                         const CNSPreciseTime &  job_time)
{
    if (!job_id  ||  !m_RunTimeLine  ||  !job_time)
        return;

    CWriteLockGuard guard(m_RunTimeLineLock);
    m_RunTimeLine->AddObject(job_time.Sec(), job_id);
}


void CQueue::TimeLineRemove(unsigned int  job_id)
{
    if (!m_RunTimeLine)
        return;

    CWriteLockGuard guard(m_RunTimeLineLock);
    m_RunTimeLine->RemoveObject(job_id);
}


void CQueue::TimeLineExchange(unsigned int            remove_job_id,
                              unsigned int            add_job_id,
                              const CNSPreciseTime &  new_time)
{
    if (!m_RunTimeLine)
        return;

    CWriteLockGuard guard(m_RunTimeLineLock);
    if (remove_job_id)
        m_RunTimeLine->RemoveObject(remove_job_id);
    if (add_job_id)
        m_RunTimeLine->AddObject(new_time.Sec(), add_job_id);
}


unsigned int  CQueue::DeleteBatch(unsigned int  max_deleted)
{
    // Copy the vector with deleted jobs
    TNSBitVector    jobs_to_delete;

    {{
         CFastMutexGuard     guard(m_JobsToDeleteLock);
         jobs_to_delete = m_JobsToDelete;
    }}

    static const size_t         chunk_size = 100;
    unsigned int                del_rec = 0;
    TNSBitVector::enumerator    en = jobs_to_delete.first();
    TNSBitVector                deleted_jobs;

    while (en.valid() && del_rec < max_deleted) {
        {{
            CFastMutexGuard     guard(m_OperationLock);
            CNSTransaction      transaction(this);

            for (size_t n = 0;
                 en.valid() && n < chunk_size && del_rec < max_deleted;
                 ++en, ++n) {
                unsigned int    job_id = *en;

                try {
                    m_QueueDbBlock->job_db.id = job_id;
                    m_QueueDbBlock->job_db.Delete();
                    ++del_rec;
                    deleted_jobs.set_bit(job_id, true);
                } catch (CBDB_ErrnoException& ex) {
                    ERR_POST("BDB error " << ex.what());
                }

                try {
                    m_QueueDbBlock->job_info_db.id = job_id;
                    m_QueueDbBlock->job_info_db.Delete();
                } catch (CBDB_ErrnoException& ex) {
                    ERR_POST("BDB error " << ex.what());
                }

                x_DeleteJobEvents(job_id);

                // The job might be the one which was given for reading
                // so the garbage should be collected
                m_ReadJobs.set_bit(job_id, false);
                ++m_ReadJobsOps;
            }

            transaction.Commit();
        }}
    }

    if (del_rec > 0) {
        m_StatisticsCounters.CountDBDeletion(del_rec);

        {{
            TNSBitVector::enumerator    en = deleted_jobs.first();
            CFastMutexGuard             guard(m_JobsToDeleteLock);
            for (; en.valid(); ++en) {
                m_JobsToDelete.set_bit(*en, false);
                ++m_JobsToDeleteOps;
            }

            if (m_JobsToDeleteOps >= 1000000) {
                m_JobsToDeleteOps = 0;
                m_JobsToDelete.optimize(0, TNSBitVector::opt_free_0);
            }
        }}

        CFastMutexGuard     guard(m_OperationLock);
        if (m_ReadJobsOps >= 1000000) {
            m_ReadJobsOps = 0;
            m_ReadJobs.optimize(0, TNSBitVector::opt_free_0);
        }
    }
    return del_rec;
}


// See CXX-2838 for the description of how affinities garbage collection is
// going  to work.
unsigned int  CQueue::PurgeAffinities(void)
{
    unsigned int    aff_dict_size = m_AffinityRegistry.size();

    if (aff_dict_size < (m_AffinityLowMarkPercentage / 100.0) * m_MaxAffinities)
        // Did not reach the dictionary low mark
        return 0;

    unsigned int    del_limit = m_AffinityHighRemoval;
    if (aff_dict_size <
            (m_AffinityHighMarkPercentage / 100.0) * m_MaxAffinities) {
        // Here: check the percentage of the affinities that have no references
        // to them
        unsigned int    candidates_size =
                                    m_AffinityRegistry.CheckRemoveCandidates();

        if (candidates_size <
                (m_AffinityDirtPercentage / 100.0) * m_MaxAffinities)
            // The number of candidates to be deleted is low
            return 0;

        del_limit = m_AffinityLowRemoval;
    }


    // Here: need to delete affinities from the memory and DB
    CFastMutexGuard     guard(m_OperationLock);
    CNSTransaction      transaction(this);

    unsigned int        del_count =
                                m_AffinityRegistry.CollectGarbage(del_limit);
    transaction.Commit();

    return del_count;
}


unsigned int  CQueue::PurgeGroups(void)
{
    CFastMutexGuard     guard(m_OperationLock);
    CNSTransaction      transaction(this);

    unsigned int        del_count = m_GroupRegistry.CollectGarbage(100);
    transaction.Commit();

    return del_count;
}


void  CQueue::StaleWNodes(const CNSPreciseTime &  current_time)
{
    // Clears the worker nodes affinities if the workers are inactive for
    // the configured timeout
    CFastMutexGuard     guard(m_OperationLock);
    m_ClientsRegistry.StaleWNs(current_time, m_WNodeTimeout,
                            m_AffinityRegistry, m_NotificationsList, m_Log);
}


void CQueue::PurgeBlacklistedJobs(void)
{
    m_ClientsRegistry.CheckBlacklistedJobsExisted(m_StatusTracker);
}


void  CQueue::PurgeClientRegistry(const CNSPreciseTime &  current_time)
{
    CFastMutexGuard     guard(m_OperationLock);
    m_ClientsRegistry.Purge(current_time,
                            m_ClientRegistryTimeoutWorkerNode,
                            m_ClientRegistryMinWorkerNodes,
                            m_ClientRegistryTimeoutAdmin,
                            m_ClientRegistryMinAdmins,
                            m_ClientRegistryTimeoutSubmitter,
                            m_ClientRegistryMinSubmitters,
                            m_ClientRegistryTimeoutReader,
                            m_ClientRegistryMinReaders,
                            m_ClientRegistryTimeoutUnknown,
                            m_ClientRegistryMinUnknowns, m_Log);
}


void CQueue::x_DeleteJobEvents(unsigned int  job_id)
{
    try {
        for (unsigned int  event_number = 0; ; ++event_number) {
            m_QueueDbBlock->events_db.id = job_id;
            m_QueueDbBlock->events_db.event_id = event_number;
            if (m_QueueDbBlock->events_db.Delete() == eBDB_NotFound)
                break;
        }
    } catch (CBDB_ErrnoException& ex) {
        ERR_POST("BDB error " << ex.what());
    }
}


CBDB_FileCursor& CQueue::GetEventsCursor()
{
    CBDB_Transaction* trans = m_QueueDbBlock->events_db.GetBDBTransaction();
    CBDB_FileCursor* cur = m_EventsCursor.get();
    if (!cur) {
        cur = new CBDB_FileCursor(m_QueueDbBlock->events_db);
        m_EventsCursor.reset(cur);
    } else {
        cur->Close();
    }
    cur->ReOpen(trans);
    return *cur;
}


string CQueue::PrintJobDbStat(const CNSClientId &  client,
                              unsigned int         job_id)
{
    m_ClientsRegistry.MarkAsAdmin(client);

    // Check first that the job has not been deleted yet
    {{
        CFastMutexGuard     guard(m_JobsToDeleteLock);
        if (m_JobsToDelete[job_id])
            return "";
    }}

    CJob                job;
    {{
        CFastMutexGuard     guard(m_OperationLock);

        m_QueueDbBlock->job_db.SetTransaction(NULL);
        m_QueueDbBlock->events_db.SetTransaction(NULL);
        m_QueueDbBlock->job_info_db.SetTransaction(NULL);

        CJob::EJobFetchResult   res = job.Fetch(this, job_id);
        if (res != CJob::eJF_Ok)
            return "";
    }}

    return job.Print(*this, m_AffinityRegistry, m_GroupRegistry);
}


string CQueue::PrintAllJobDbStat(const CNSClientId &         client,
                                 const string &              group,
                                 const string &              aff_token,
                                 const vector<TJobStatus> &  job_statuses,
                                 unsigned int                start_after_job_id,
                                 unsigned int                count,
                                 bool                        logging)
{
    m_ClientsRegistry.MarkAsAdmin(client);

    // Form a bit vector of all jobs to dump
    vector<TJobStatus>      statuses;
    TNSBitVector            jobs_to_dump;

    if (job_statuses.empty()) {
        // All statuses
        statuses.push_back(CNetScheduleAPI::ePending);
        statuses.push_back(CNetScheduleAPI::eRunning);
        statuses.push_back(CNetScheduleAPI::eCanceled);
        statuses.push_back(CNetScheduleAPI::eFailed);
        statuses.push_back(CNetScheduleAPI::eDone);
        statuses.push_back(CNetScheduleAPI::eReading);
        statuses.push_back(CNetScheduleAPI::eConfirmed);
        statuses.push_back(CNetScheduleAPI::eReadFailed);
    }
    else {
        // The user specified statuses explicitly
        // The list validity is checked by the caller.
        statuses = job_statuses;
    }


    {{
        CFastMutexGuard     guard(m_OperationLock);
        jobs_to_dump = m_StatusTracker.GetJobs(statuses);

        // Check if a certain group has been specified
        if (!group.empty()) {
            try {
                jobs_to_dump &= m_GroupRegistry.GetJobs(group);
            } catch (...) {
                jobs_to_dump.clear();
                if (logging)
                    LOG_POST(Message << Warning <<
                             "Job group '" + group +
                             "' is not found. No jobs to dump.");
            }
        }

        if (!aff_token.empty()) {
            unsigned int    aff_id = m_AffinityRegistry.GetIDByToken(aff_token);
            if (aff_id == 0) {
                jobs_to_dump.clear();
                if (logging)
                    LOG_POST(Message << Warning <<
                             "Affinity '" + aff_token +
                             "' is not found. No jobs to dump.");
            } else
                jobs_to_dump &= m_AffinityRegistry.GetJobsWithAffinity(aff_id);
        }
    }}

    return x_DumpJobs(jobs_to_dump, start_after_job_id, count);
}


string CQueue::x_DumpJobs(const TNSBitVector &    jobs_to_dump,
                          unsigned int            start_after_job_id,
                          unsigned int            count)
{
    if (!jobs_to_dump.any())
        return "";

    // Skip the jobs which should not be dumped
    TNSBitVector::enumerator    en(jobs_to_dump.first());
    while (en.valid() && *en <= start_after_job_id)
        ++en;

    // Identify the required buffer size for jobs
    size_t      buffer_size = m_DumpBufferSize;
    if (count != 0 && count < buffer_size)
        buffer_size = count;

    string  result;
    result.reserve(2048*buffer_size);

    {{
        CJob    buffer[buffer_size];

        size_t      read_jobs = 0;
        size_t      printed_count = 0;

        for ( ; en.valid(); ) {
            {{
                CFastMutexGuard     guard(m_OperationLock);
                m_QueueDbBlock->job_db.SetTransaction(NULL);
                m_QueueDbBlock->events_db.SetTransaction(NULL);
                m_QueueDbBlock->job_info_db.SetTransaction(NULL);

                for ( ; en.valid() && read_jobs < buffer_size; ++en )
                    if (buffer[read_jobs].Fetch(this, *en) == CJob::eJF_Ok) {
                        ++read_jobs;
                        ++printed_count;

                        if (count != 0)
                            if (printed_count >= count)
                                break;
                    }
            }}

            // Print what was read
            for (size_t  index = 0; index < read_jobs; ++index) {
                result += "\n" + buffer[index].Print(*this,
                                                     m_AffinityRegistry,
                                                     m_GroupRegistry) +
                          "OK:GC erase time: " +
                          NS_FormatPreciseTime(
                                  m_GCRegistry.GetLifetime(
                                                buffer[index].GetId())) +
                          "\n";
            }

            if (count != 0)
                if (printed_count >= count)
                    break;

            read_jobs = 0;
        }
    }}

    return result;
}


unsigned CQueue::CountStatus(TJobStatus st) const
{
    return m_StatusTracker.CountStatus(st);
}


void CQueue::StatusStatistics(TJobStatus                status,
                              TNSBitVector::statistics* st) const
{
    m_StatusTracker.StatusStatistics(status, st);
}


string CQueue::MakeJobKey(unsigned int  job_id) const
{
    if (m_ScrambleJobKeys)
        return m_KeyGenerator.GenerateCompoundID(job_id,
                                                 m_Server->GetCompoundIDPool());
    return m_KeyGenerator.Generate(job_id);
}


void CQueue::TouchClientsRegistry(CNSClientId &  client,
                                  bool &         client_was_found,
                                  bool &         session_was_reset,
                                  string &       old_session,
                                  bool &         pref_affs_were_reset)
{
    TNSBitVector        running_jobs;
    TNSBitVector        reading_jobs;

    {{
        CFastMutexGuard     guard(m_OperationLock);
        unsigned short      wait_port = m_ClientsRegistry.Touch(
                                                client,
                                                m_AffinityRegistry,
                                                running_jobs,
                                                reading_jobs,
                                                client_was_found,
                                                session_was_reset,
                                                old_session,
                                                pref_affs_were_reset);
        if (wait_port != 0)
            m_NotificationsList.UnregisterListener(client, wait_port);
    }}


    if (running_jobs.any())
        x_ResetRunningDueToNewSession(client, running_jobs);
    if (reading_jobs.any())
        x_ResetReadingDueToNewSession(client, reading_jobs);
}


void CQueue::MarkClientAsAdmin(const CNSClientId &  client)
{
    m_ClientsRegistry.MarkAsAdmin(client);
}


void CQueue::RegisterSocketWriteError(const CNSClientId &  client)
{
    CFastMutexGuard     guard(m_OperationLock);
    m_ClientsRegistry.RegisterSocketWriteError(client);
}


// Moves the job to Pending/Failed or to Done/ReadFailed
// when event event_type has come
TJobStatus  CQueue::x_ResetDueTo(const CNSClientId &     client,
                                 unsigned int            job_id,
                                 const CNSPreciseTime &  current_time,
                                 TJobStatus              status_from,
                                 CJobEvent::EJobEvent    event_type)
{
    CJob                job;
    TJobStatus          new_status;

    CFastMutexGuard     guard(m_OperationLock);

    {{
        CNSTransaction      transaction(this);

        if (job.Fetch(this, job_id) != CJob::eJF_Ok) {
            ERR_POST("Cannot fetch job to reset it due to " <<
                     CJobEvent::EventToString(event_type) <<
                     ". Job: " << DecorateJob(job_id));
            return CNetScheduleAPI::eJobNotFound;
        }

        if (status_from == CNetScheduleAPI::eRunning) {
            // The job was running
            if (job.GetRunCount() > m_FailedRetries)
                new_status = CNetScheduleAPI::eFailed;
            else
                new_status = CNetScheduleAPI::ePending;
        } else {
            // The job was reading
            if (job.GetReadCount() > m_FailedRetries)
                new_status = CNetScheduleAPI::eReadFailed;
            else
                new_status = job.GetStatusBeforeReading();
            m_ReadJobs.set_bit(job_id, false);
            ++m_ReadJobsOps;
        }

        job.SetStatus(new_status);
        job.SetLastTouch(current_time);

        CJobEvent *     event = &job.AppendEvent();
        event->SetStatus(new_status);
        event->SetEvent(event_type);
        event->SetTimestamp(current_time);
        event->SetClientNode(client.GetNode());
        event->SetClientSession(client.GetSession());

        job.Flush(this);
        transaction.Commit();
    }}

    // Update the memory map
    m_StatusTracker.SetStatus(job_id, new_status);

    // Count the transition
    CStatisticsCounters::ETransitionPathOption
                                transition_path = CStatisticsCounters::eNone;
    if (event_type == CJobEvent::eClear)
        transition_path = CStatisticsCounters::eClear;
    else if (event_type == CJobEvent::eSessionChanged)
        transition_path = CStatisticsCounters::eNewSession;
    m_StatisticsCounters.CountTransition(status_from, new_status,
                                         transition_path);

    m_GCRegistry.UpdateLifetime(job_id,
                                job.GetExpirationTime(m_Timeout,
                                                      m_RunTimeout,
                                                      m_ReadTimeout,
                                                      m_PendingTimeout,
                                                      current_time));

    // remove the job from the time line
    TimeLineRemove(job_id);

    // Notify those who wait for the jobs if needed
    if (new_status == CNetScheduleAPI::ePending &&
        m_PauseStatus == eNoPause)
        m_NotificationsList.Notify(job_id, job.GetAffinityId(),
                                   m_ClientsRegistry,
                                   m_AffinityRegistry,
                                   m_GroupRegistry,
                                   m_NotifHifreqPeriod,
                                   m_HandicapTimeout,
                                   eGet);
    // Notify readers if they wait for jobs
    if (new_status == CNetScheduleAPI::eDone ||
        new_status == CNetScheduleAPI::eFailed ||
        new_status == CNetScheduleAPI::eCanceled)
        if (m_ReadJobs[job_id] == false)
            m_NotificationsList.Notify(job_id, job.GetAffinityId(),
                                       m_ClientsRegistry,
                                       m_AffinityRegistry,
                                       m_GroupRegistry,
                                       m_NotifHifreqPeriod,
                                       m_HandicapTimeout,
                                       eRead);

    if (job.ShouldNotifyListener(current_time, m_JobsToNotify))
        m_NotificationsList.NotifyJobStatus(job.GetListenerNotifAddr(),
                                            job.GetListenerNotifPort(),
                                            MakeJobKey(job_id),
                                            job.GetStatus(),
                                            job.GetLastEventIndex());

    return new_status;
}


void CQueue::x_ResetRunningDueToClear(const CNSClientId &   client,
                                      const TNSBitVector &  jobs)
{
    CNSPreciseTime  current_time = CNSPreciseTime::Current();
    for (TNSBitVector::enumerator  en(jobs.first()); en.valid(); ++en) {
        try {
            x_ResetDueTo(client, *en, current_time,
                         CNetScheduleAPI::eRunning, CJobEvent::eClear);
        } catch (...) {
            ERR_POST("Error resetting a running job when worker node is "
                     "cleared. Job: " << DecorateJob(*en));
        }
    }
}


void CQueue::x_ResetReadingDueToClear(const CNSClientId &   client,
                                      const TNSBitVector &  jobs)
{
    CNSPreciseTime  current_time = CNSPreciseTime::Current();
    for (TNSBitVector::enumerator  en(jobs.first()); en.valid(); ++en) {
        try {
            x_ResetDueTo(client, *en, current_time,
                         CNetScheduleAPI::eReading, CJobEvent::eClear);
        } catch (...) {
            ERR_POST("Error resetting a reading job when worker node is "
                     "cleared. Job: " << DecorateJob(*en));
        }
    }
}


void CQueue::x_ResetRunningDueToNewSession(const CNSClientId &   client,
                                           const TNSBitVector &  jobs)
{
    CNSPreciseTime  current_time = CNSPreciseTime::Current();
    for (TNSBitVector::enumerator  en(jobs.first()); en.valid(); ++en) {
        try {
            x_ResetDueTo(client, *en, current_time,
                         CNetScheduleAPI::eRunning, CJobEvent::eSessionChanged);
        } catch (...) {
            ERR_POST("Error resetting a running job when worker node "
                     "changed session. Job: " << DecorateJob(*en));
        }
    }
}


void CQueue::x_ResetReadingDueToNewSession(const CNSClientId &   client,
                                           const TNSBitVector &  jobs)
{
    CNSPreciseTime  current_time = CNSPreciseTime::Current();
    for (TNSBitVector::enumerator  en(jobs.first()); en.valid(); ++en) {
        try {
            x_ResetDueTo(client, *en, current_time,
                         CNetScheduleAPI::eReading, CJobEvent::eSessionChanged);
        } catch (...) {
            ERR_POST("Error resetting a reading job when worker node "
                     "changed session. Job: " << DecorateJob(*en));
        }
    }
}


void CQueue::x_RegisterGetListener(const CNSClientId &   client,
                                   unsigned short        port,
                                   unsigned int          timeout,
                                   const TNSBitVector &  aff_ids,
                                   bool                  wnode_aff,
                                   bool                  any_aff,
                                   bool                  exclusive_new_affinity,
                                   bool                  new_format,
                                   const string &        group)
{
    // Add to the notification list and save the wait port
    m_NotificationsList.RegisterListener(client, port, timeout,
                                         wnode_aff, any_aff,
                                         exclusive_new_affinity, new_format,
                                         group, eGet);
    if (client.IsComplete())
        m_ClientsRegistry.SetWaiting(client, port, aff_ids, m_AffinityRegistry);
    return;
}


void
CQueue::x_RegisterReadListener(const CNSClientId &   client,
                               unsigned short        port,
                               unsigned int          timeout,
                               const TNSBitVector &  aff_ids,
                               bool                  reader_aff,
                               bool                  any_aff,
                               bool                  exclusive_new_affinity,
                               const string &        group)
{
    // Add to the notification list and save the wait port
    m_NotificationsList.RegisterListener(client, port, timeout,
                                         reader_aff, any_aff,
                                         exclusive_new_affinity, true,
                                         group, eRead);
    m_ClientsRegistry.SetWaiting(client, port, aff_ids, m_AffinityRegistry);
}


bool CQueue::x_UnregisterGetListener(const CNSClientId &  client,
                                     unsigned short       port)
{
    unsigned short      notif_port = port;

    if (client.IsComplete())
        // New clients have the port in their registry record
        notif_port = m_ClientsRegistry.ResetWaiting(client, m_AffinityRegistry);

    if (notif_port > 0) {
        m_NotificationsList.UnregisterListener(client, notif_port);
        return true;
    }

    return false;
}


bool CQueue::x_UnregisterReadListener(const CNSClientId &  client)
{
    unsigned short  notif_port = m_ClientsRegistry.
                                    ResetWaiting(client, m_AffinityRegistry);

    if (notif_port > 0) {
        m_NotificationsList.UnregisterListener(client, notif_port);
        return true;
    }

    return false;
}


void CQueue::PrintStatistics(size_t &  aff_count) const
{
    CRef<CRequestContext>   ctx;
    ctx.Reset(new CRequestContext());
    ctx->SetRequestID();

    CDiagContext &      diag_context = GetDiagContext();

    diag_context.SetRequestContext(ctx);
    CDiagContext_Extra      extra = diag_context.PrintRequestStart();

    size_t      affinities = m_AffinityRegistry.size();
    aff_count += affinities;

    // The member is called only if there is a request context
    extra.Print("_type", "statistics_thread")
         .Print("queue", GetQueueName())
         .Print("affinities", affinities)
         .Print("pending", CountStatus(CNetScheduleAPI::ePending))
         .Print("running", CountStatus(CNetScheduleAPI::eRunning))
         .Print("canceled", CountStatus(CNetScheduleAPI::eCanceled))
         .Print("failed", CountStatus(CNetScheduleAPI::eFailed))
         .Print("done", CountStatus(CNetScheduleAPI::eDone))
         .Print("reading", CountStatus(CNetScheduleAPI::eReading))
         .Print("confirmed", CountStatus(CNetScheduleAPI::eConfirmed))
         .Print("readfailed", CountStatus(CNetScheduleAPI::eReadFailed));
    m_StatisticsCounters.PrintTransitions(extra);
    extra.Flush();

    ctx->SetRequestStatus(CNetScheduleHandler::eStatus_OK);
    diag_context.PrintRequestStop();
    ctx.Reset();
    diag_context.SetRequestContext(NULL);
}


unsigned int CQueue::GetJobsToDeleteCount(void) const
{
    CFastMutexGuard     jtd_guard(m_JobsToDeleteLock);
    return m_JobsToDelete.count();
}


string CQueue::PrintTransitionCounters(void) const
{
    return m_StatisticsCounters.PrintTransitions() +
           "OK:garbage_jobs: " +
           NStr::NumericToString(GetJobsToDeleteCount()) + "\n"
           "OK:affinity_registry_size: " +
           NStr::NumericToString(m_AffinityRegistry.size()) + "\n"
           "OK:client_registry_size: " +
           NStr::NumericToString(m_ClientsRegistry.size()) + "\n";
}


void CQueue::GetJobsPerState(const string &  group_token,
                             const string &  aff_token,
                             size_t *        jobs) const
{
    TNSBitVector        group_jobs;
    TNSBitVector        aff_jobs;
    CFastMutexGuard     guard(m_OperationLock);

    if (!group_token.empty())
        group_jobs = m_GroupRegistry.GetJobs(group_token);
    if (!aff_token.empty()) {
        unsigned int  aff_id = m_AffinityRegistry.GetIDByToken(aff_token);
        if (aff_id == 0)
            NCBI_THROW(CNetScheduleException, eAffinityNotFound,
                       "Unknown affinity token \"" + aff_token + "\"");

        aff_jobs = m_AffinityRegistry.GetJobsWithAffinity(aff_id);
    }

    for (size_t  index(0); index < g_ValidJobStatusesSize; ++index) {
        TNSBitVector  candidates =
                        m_StatusTracker.GetJobs(g_ValidJobStatuses[index]);

        if (!group_token.empty())
            candidates &= group_jobs;
        if (!aff_token.empty())
            candidates &= aff_jobs;

        jobs[index] = candidates.count();
    }
}


string CQueue::PrintJobsStat(const string &  group_token,
                             const string &  aff_token) const
{
    size_t              total = 0;
    string              result;
    size_t              jobs_per_state[g_ValidJobStatusesSize];

    GetJobsPerState(group_token, aff_token, jobs_per_state);

    for (size_t  index(0); index < g_ValidJobStatusesSize; ++index) {
        result += "OK:" +
                  CNetScheduleAPI::StatusToString(g_ValidJobStatuses[index]) +
                  ": " + NStr::NumericToString(jobs_per_state[index]) + "\n";
        total += jobs_per_state[index];
    }
    result += "OK:Total: " + NStr::NumericToString(total) + "\n";
    return result;
}


unsigned int CQueue::CountActiveJobs(void) const
{
    vector<CNetScheduleAPI::EJobStatus>     statuses;

    statuses.push_back(CNetScheduleAPI::ePending);
    statuses.push_back(CNetScheduleAPI::eRunning);
    return m_StatusTracker.CountStatus(statuses);
}


void CQueue::SetPauseStatus(const CNSClientId &  client, TPauseStatus  status)
{
    m_ClientsRegistry.MarkAsAdmin(client);

    bool        need_notifications = (status == eNoPause &&
                                      m_PauseStatus != eNoPause);

    m_PauseStatus = status;
    if (need_notifications)
        m_NotificationsList.onQueueResumed(m_StatusTracker.AnyPending());
}


void CQueue::RegisterQueueResumeNotification(unsigned int  address,
                                             unsigned short  port,
                                             bool  new_format)
{
    m_NotificationsList.AddToQueueResumedNotifications(address, port,
                                                       new_format);
}


void CQueue::x_UpdateDB_PutResultNoLock(unsigned                job_id,
                                        const string &          auth_token,
                                        const CNSPreciseTime &  curr,
                                        int                     ret_code,
                                        const string &          output,
                                        CJob &                  job,
                                        const CNSClientId &     client)
{
    if (job.Fetch(this, job_id) != CJob::eJF_Ok)
        NCBI_THROW(CNetScheduleException, eInternalError,
                   "Error fetching job");

    if (!auth_token.empty()) {
        // Need to check authorization token first
        CJob::EAuthTokenCompareResult   token_compare_result =
                                        job.CompareAuthToken(auth_token);
        if (token_compare_result == CJob::eInvalidTokenFormat)
            NCBI_THROW(CNetScheduleException, eInvalidAuthToken,
                       "Invalid authorization token format");
        if (token_compare_result == CJob::eNoMatch)
            NCBI_THROW(CNetScheduleException, eInvalidAuthToken,
                       "Authorization token does not match");
        if (token_compare_result == CJob::ePassportOnlyMatch) {
            // That means that the job has been executing by another worker
            // node at the moment, but we can accept the results anyway
            LOG_POST(Message << Warning << "Received PUT2 with only "
                                           "passport matched.");
        }
        // Here: the authorization token is OK, we can continue
    }

    // Append the event
    CJobEvent *     event = &job.AppendEvent();
    event->SetStatus(CNetScheduleAPI::eDone);
    event->SetEvent(CJobEvent::eDone);
    event->SetTimestamp(curr);
    event->SetRetCode(ret_code);

    event->SetClientNode(client.GetNode());
    event->SetClientSession(client.GetSession());
    event->SetNodeAddr(client.GetAddress());

    job.SetStatus(CNetScheduleAPI::eDone);
    job.SetOutput(output);
    job.SetLastTouch(curr);

    job.Flush(this);
    return;
}


// If the job.job_id != 0 => the job has been read successfully
// Exception => DB errors
// Return: true  => job is ok to get
//         false => job is expired and was marked for deletion,
//                  need to get another job
bool  CQueue::x_UpdateDB_GetJobNoLock(const CNSClientId &     client,
                                      const CNSPreciseTime &  curr,
                                      unsigned int            job_id,
                                      CJob &                  job)
{
    CNSTransaction      transaction(this);

    if (job.Fetch(this, job_id) != CJob::eJF_Ok)
        NCBI_THROW(CNetScheduleException, eInternalError,
                   "Error fetching job");

    // The job has been read successfully, check if it is still within the
    // expiration timeout;
    // Note: run_timeout and read_timeout are not applicable because the
    //       job is guaranteed in the pending state
    if (job.GetExpirationTime(m_Timeout, CNSPreciseTime(), CNSPreciseTime(),
                              m_PendingTimeout, CNSPreciseTime()) <= curr) {
        // The job has expired, so mark it for deletion
        EraseJob(job_id);

        if (job.GetAffinityId() != 0)
            m_AffinityRegistry.RemoveJobFromAffinity(
                                    job_id, job.GetAffinityId());
        if (job.GetGroupId() != 0)
            m_GroupRegistry.RemoveJob(job.GetGroupId(), job_id);

        return false;
    }

    unsigned        run_count = job.GetRunCount() + 1;
    CJobEvent &     event = job.AppendEvent();

    event.SetStatus(CNetScheduleAPI::eRunning);
    event.SetEvent(CJobEvent::eRequest);
    event.SetTimestamp(curr);

    event.SetClientNode(client.GetNode());
    event.SetClientSession(client.GetSession());
    event.SetNodeAddr(client.GetAddress());

    job.SetStatus(CNetScheduleAPI::eRunning);
    job.SetRunTimeout(CNSPreciseTime());
    job.SetRunCount(run_count);
    job.SetLastTouch(curr);

    job.Flush(this);
    transaction.Commit();

    return true;
}


END_NCBI_SCOPE

