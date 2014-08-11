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
 * Authors:  Sergey Satskiy
 *
 * File Description:
 *   NetSchedule clients registry
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/request_ctx.hpp>

#include "ns_clients_registry.hpp"
#include "ns_affinity.hpp"
#include "ns_handler.hpp"


BEGIN_NCBI_SCOPE


CNSClientsRegistry::CNSClientsRegistry() :
    m_LastID(0), m_BlacklistTimeout(), m_ReadBlacklistTimeout()
{}


size_t  CNSClientsRegistry::size(void) const
{
    return m_Clients.size();
}


// Called before any command is issued by the client
// Returns waiting port != 0 if the client has reset waiting on WGET
unsigned short
CNSClientsRegistry::Touch(CNSClientId &          client,
                          CNSAffinityRegistry &  aff_registry,
                          TNSBitVector &         running_jobs,
                          TNSBitVector &         reading_jobs,
                          bool &                 client_was_found,
                          bool &                 session_was_reset,
                          string &               old_session,
                          bool &                 pref_affs_were_reset)
{
    client_was_found = false;
    session_was_reset = false;
    old_session.clear();
    pref_affs_were_reset = false;

    // Check if it is an old-style client
    if (!client.IsComplete())
        return 0;

    CMutexGuard                         guard(m_Lock);
    map< string, CNSClient >::iterator  known_client =
                                             m_Clients.find(client.GetNode());

    if (known_client == m_Clients.end()) {
        // The client is not known yet
        CNSClient       new_ns_client(client, &m_BlacklistTimeout,
                                              &m_ReadBlacklistTimeout);
        unsigned int    client_id = x_GetNextID();

        new_ns_client.SetID(client_id);
        client.SetID(client_id);
        m_Clients[ client.GetNode() ] = new_ns_client;
        m_RegisteredClients.set_bit(client_id);
        return 0;
    }

    client_was_found = true;
    unsigned short  wait_port = 0;

    // The client has connected before
    client.SetID(known_client->second.GetID());
    if (client.GetSession() != known_client->second.GetSession()) {
        // Remove the client from its preferred affinities
        aff_registry.RemoveClientFromAffinities(
                                known_client->second.GetID(),
                                known_client->second.GetPreferredAffinities());

        // The client has changed the session => need to reset waiting if so
        wait_port = x_ResetWaiting(known_client->second, aff_registry);
    }

    if (known_client->second.Touch(client, running_jobs, reading_jobs,
                                   session_was_reset,old_session)) {
        pref_affs_were_reset = true;
        x_BuildWNAffinities();  // session is changed, i.e. the preferred
                                // affinities were reset and they had at
                                // least one bit set
    }

    return wait_port;
}


void CNSClientsRegistry::RegisterSocketWriteError(const CNSClientId &  client)
{
    // Check if it is an old-style client
    if (!client.IsComplete())
        return;

    CMutexGuard                         guard(m_Lock);
    map< string, CNSClient >::iterator  cl = m_Clients.find(client.GetNode());
    if (cl != m_Clients.end())
        cl->second.RegisterSocketWriteError();
}


void
CNSClientsRegistry::AppendType(const CNSClientId &  client,
                               unsigned int         type_to_append)
{
    // Check if it is an old-style client
    if (!client.IsComplete())
        return;

    CMutexGuard                         guard(m_Lock);
    map< string, CNSClient >::iterator  cl = m_Clients.find(client.GetNode());
    if (cl != m_Clients.end())
        cl->second.AppendType(type_to_append);
}


void
CNSClientsRegistry::CheckRunBlacklistedJobsExisted(
                                        const CJobStatusTracker &  tracker)
{
    CMutexGuard                         guard(m_Lock);
    map< string, CNSClient >::iterator  k = m_Clients.begin();

    for ( ; k != m_Clients.end(); ++k)
        k->second.CheckRunBlacklistedJobsExisted(tracker);
}


void
CNSClientsRegistry::CheckReadBlacklistedJobsExisted(
                                        const CJobStatusTracker &  tracker)
{
    CMutexGuard                         guard(m_Lock);
    map< string, CNSClient >::iterator  k = m_Clients.begin();

    for ( ; k != m_Clients.end(); ++k)
        k->second.CheckReadBlacklistedJobsExisted(tracker);
}


int
CNSClientsRegistry::SetClientData(const CNSClientId &  client,
                                  const string &  data, int  data_version)
{
    // Check if it is an old-style client
    if (!client.IsComplete())
        NCBI_THROW(CNetScheduleException, eInvalidParameter,
                   "only non-anonymous clients may set their data");

    CMutexGuard                     guard(m_Lock);
    map< string,
         CNSClient >::iterator      found = m_Clients.find(client.GetNode());

    if (found == m_Clients.end())
        NCBI_THROW(CNetScheduleException, eInternalError,
                   "Cannot find client '" + client.GetNode() +
                   "' to set client data");

    return found->second.SetClientData(data, data_version);
}


void  CNSClientsRegistry::MarkAsAdmin(const CNSClientId &  client)
{
    // Check if it is an old-style client
    if (!client.IsComplete())
        return;

    CMutexGuard                 guard(m_Lock);
    map< string,
         CNSClient >::iterator  admin = m_Clients.find(client.GetNode());

    if (admin != m_Clients.end())
        admin->second.MarkAsAdmin();
}


// Updates the submitter job.
// No need to check session id, it's done in Touch()
void  CNSClientsRegistry::AddToSubmitted(const CNSClientId &  client,
                                         size_t               count)
{
    // Check if it is an old-style client
    if (!client.IsComplete())
        return;

    CMutexGuard                 guard(m_Lock);
    map< string,
         CNSClient >::iterator  submitter = m_Clients.find(client.GetNode());

    if (submitter == m_Clients.end())
        NCBI_THROW(CNetScheduleException, eInternalError,
                   "Cannot find client '" + client.GetNode() +
                   "' to set submitted job");

    submitter->second.RegisterSubmittedJobs(count);
}


// Updates the reader jobs list.
// No need to check session id, it's done in Touch()
void  CNSClientsRegistry::AddToReading(const CNSClientId &  client,
                                       unsigned int         job_id)
{
    // Check if it is an old-style client
    if (!client.IsComplete())
        return;

    CMutexGuard                 guard(m_Lock);
    map< string,
         CNSClient >::iterator  reader = m_Clients.find(client.GetNode());

    if (reader == m_Clients.end())
        NCBI_THROW(CNetScheduleException, eInternalError,
                   "Cannot find client '" + client.GetNode() +
                   "' to set reading job");

    reader->second.RegisterReadingJob(job_id);
}


// Updates the worker node jobs list.
// No need to check session id, it's done in Touch()
void  CNSClientsRegistry::AddToRunning(const CNSClientId &  client,
                                       unsigned int         job_id)
{
    // Check if it is an old-style client
    if (!client.IsComplete())
        return;

    CMutexGuard                 guard(m_Lock);
    map< string,
         CNSClient >::iterator  worker_node = m_Clients.find(client.GetNode());

    if (worker_node == m_Clients.end())
        NCBI_THROW(CNetScheduleException, eInternalError,
                   "Cannot find client '" + client.GetNode() +
                   "' to set running job");

    worker_node->second.RegisterRunningJob(job_id);
}


// Updates the worker node blacklisted jobs list.
// No need to check session id, it's done in Touch()
void  CNSClientsRegistry::AddToRunBlacklist(const CNSClientId &  client,
                                            unsigned int         job_id)
{
    // Check if it is an old-style client
    if (!client.IsComplete())
        return;

    CMutexGuard                 guard(m_Lock);
    map< string,
         CNSClient >::iterator  worker_node = m_Clients.find(client.GetNode());

    if (worker_node == m_Clients.end())
        NCBI_THROW(CNetScheduleException, eInternalError,
                   "Cannot find client '" + client.GetNode() +
                   "' to set blacklisted job");

    worker_node->second.RegisterRunBlacklistedJob(job_id);
}


// Updates the reader blacklisted jobs list.
// No need to check session id, it's done in Touch()
void  CNSClientsRegistry::AddToReadBlacklist(const CNSClientId &  client,
                                             unsigned int         job_id)
{
    // Check if it is an old-style client
    if (!client.IsComplete())
        return;

    CMutexGuard                 guard(m_Lock);
    map< string,
         CNSClient >::iterator  reader = m_Clients.find(client.GetNode());

    if (reader == m_Clients.end())
        NCBI_THROW(CNetScheduleException, eInternalError,
                   "Cannot find client '" + client.GetNode() +
                   "' to set blacklisted job");

    reader->second.RegisterReadBlacklistedJob(job_id);
}


// Updates the reader jobs list.
// No need to check session id, it's done in Touch()
void  CNSClientsRegistry::ClearReading(const CNSClientId &  client,
                                       unsigned int         job_id)
{
    // Check if it is an old-style client
    if (!client.IsComplete())
        return;

    CMutexGuard                 guard(m_Lock);
    map< string,
         CNSClient >::iterator  reader = m_Clients.find(client.GetNode());

    if (reader != m_Clients.end())
        reader->second.UnregisterReadingJob(job_id);
}


// Updates the reader jobs list.
// Used when a job is timed out. At this moment there is no information about
// the node identifier so all the clients are checked.
void  CNSClientsRegistry::ClearReading(unsigned int  job_id)
{
    // The container itself is not changed.
    // The changes are in what the element holds.
    CMutexGuard                         guard(m_Lock);
    map< string, CNSClient >::iterator  k = m_Clients.begin();

    for ( ; k != m_Clients.end(); ++k)
        k->second.UnregisterReadingJob(job_id);
}


// Updates the client reading job list and its blacklist.
// Used when  the job is timed out and rescheduled for reading as well as when
// RDRB or FRED are received
void  CNSClientsRegistry::ClearReadingSetBlacklist(unsigned int  job_id)
{
    // The container itself is not changed.
    // The changes are in what the element holds.
    CMutexGuard                         guard(m_Lock);
    map< string, CNSClient >::iterator  k = m_Clients.begin();

    for ( ; k != m_Clients.end(); ++k)
        if (k->second.MoveReadingJobToBlacklist(job_id))
            return;
}


// Updates the worker node jobs list.
// No need to check session id, it's done in Touch()
void  CNSClientsRegistry::ClearExecuting(const CNSClientId &  client,
                                         unsigned int         job_id)
{
    // Check if it is an old-style client
    if (!client.IsComplete())
        return;

    CMutexGuard                 guard(m_Lock);
    map< string,
         CNSClient >::iterator  worker_node = m_Clients.find(client.GetNode());

    if (worker_node != m_Clients.end())
        worker_node->second.UnregisterRunningJob(job_id);
}


// Updates the worker node jobs list.
// Used when a job is timed out. At this moment there is no information about
// the node identifier so all the clients are checked.
void  CNSClientsRegistry::ClearExecuting(unsigned int  job_id)
{
    // The container itself is not changed.
    // The changes are in what the element holds.
    CMutexGuard                         guard(m_Lock);
    map< string, CNSClient >::iterator  k = m_Clients.begin();

    for ( ; k != m_Clients.end(); ++k)
        k->second.UnregisterRunningJob(job_id);
}


// Updates the worker node executing job list and its blacklist.
// Used when  the job is timed out and rescheduled. At this moment there is no
// information about the node identifier so all the clients are checked.
void  CNSClientsRegistry::ClearExecutingSetBlacklist(unsigned int  job_id)
{
    // The container itself is not changed.
    // The changes are in what the element holds.
    CMutexGuard                         guard(m_Lock);
    map< string, CNSClient >::iterator  k = m_Clients.begin();

    for ( ; k != m_Clients.end(); ++k)
        if (k->second.MoveRunningJobToBlacklist(job_id))
            return;
}


// Used when CLRN command is received
// provides WGET port != 0 if there was waiting reset
unsigned short
CNSClientsRegistry::ClearWorkerNode(const CNSClientId &    client,
                                    CNSAffinityRegistry &  aff_registry,
                                    TNSBitVector &         running_jobs,
                                    TNSBitVector &         reading_jobs,
                                    bool &                 client_was_found,
                                    string &               old_session,
                                    bool &                 pref_affs_were_reset)
{
    client_was_found = false;
    pref_affs_were_reset = false;

    // The container itself is not changed.
    // The changes are in what the element holds.
    unsigned short                      wait_port = 0;
    string                              client_name = client.GetNode();
    CMutexGuard                         guard(m_Lock);
    map< string, CNSClient >::iterator  worker_node =
                                             m_Clients.find(client_name);

    if (worker_node != m_Clients.end()) {
        client_was_found = true;
        old_session = worker_node->second.GetSession();
        running_jobs = worker_node->second.GetRunningJobs();
        reading_jobs = worker_node->second.GetReadingJobs();

        aff_registry.RemoveClientFromAffinities(
                                worker_node->second.GetID(),
                                worker_node->second.GetPreferredAffinities());
        wait_port = x_ResetWaiting(worker_node->second, aff_registry);

        if (worker_node->second.Clear()) {
            pref_affs_were_reset = true;
            x_BuildWNAffinities(); // Rebuild if there was at
                                   // least one preferred aff
        }
    }

    // Client explicitly said good bye. No need to remember it if it was
    // garbage collected previously.
    m_GCClients.erase(client_name);

    return wait_port;
}


string
CNSClientsRegistry::PrintClientsList(const CQueue *               queue,
                                     const CNSAffinityRegistry &  aff_registry,
                                     size_t                       batch_size,
                                     bool                         verbose) const
{
    TNSBitVector        batch;
    string              result;

    TNSBitVector                registered_clients = GetRegisteredClients();
    TNSBitVector::enumerator    en(registered_clients.first());

    while (en.valid()) {
        batch.set_bit(*en);
        ++en;

        if (batch.count() >= batch_size) {
            result += x_PrintSelected(batch, queue,
                                      aff_registry, verbose) + "\n";
            batch.clear();
        }
    }

    if (batch.count() > 0)
        result += x_PrintSelected(batch, queue, aff_registry, verbose) + "\n";
    return result;
}


string
CNSClientsRegistry::x_PrintSelected(const TNSBitVector &         batch,
                                    const CQueue *               queue,
                                    const CNSAffinityRegistry &  aff_registry,
                                    bool                         verbose) const
{
    string      buffer;
    size_t      printed = 0;

    CMutexGuard                                 guard(m_Lock);
    map< string, CNSClient >::const_iterator    k = m_Clients.begin();

    for ( ; k != m_Clients.end(); ++k) {
        if (batch[k->second.GetID()]) {
            buffer += k->second.Print(k->first, queue, aff_registry,
                                      m_GCClients, verbose);
            ++printed;
            if (printed >= batch.count())
                break;
        }
    }

    return buffer;
}


void  CNSClientsRegistry::SetWaiting(const CNSClientId &          client,
                                     unsigned short               port,
                                     const TNSBitVector &         aff_ids,
                                     CNSAffinityRegistry &        aff_registry)
{
    // Check if it is an old-style client
    if (!client.IsComplete())
        return;

    CMutexGuard                 guard(m_Lock);
    map< string,
         CNSClient >::iterator  worker_node = m_Clients.find(client.GetNode());

    if (worker_node == m_Clients.end())
        NCBI_THROW(CNetScheduleException, eInternalError,
                   "Cannot find client '" + client.GetNode() +
                   "' to set waiting attributes");

    worker_node->second.SetWaitPort(port);
    worker_node->second.RegisterWaitAffinities(aff_ids);
    aff_registry.SetWaitClientForAffinities(
                                    worker_node->second.GetID(),
                                    worker_node->second.GetWaitAffinities());
}


unsigned short
CNSClientsRegistry::ResetWaiting(const CNSClientId &    client,
                                 CNSAffinityRegistry &  aff_registry)
{
    // Check if it is an old-style client
    if (!client.IsComplete())
        return 0;

    return ResetWaiting(client.GetNode(), aff_registry);
}


unsigned short
CNSClientsRegistry::ResetWaiting(const string &         node_name,
                                 CNSAffinityRegistry &  aff_registry)
{
    if (node_name.empty())
        return 0;

    CMutexGuard                         guard(m_Lock);
    map< string, CNSClient >::iterator  worker_node = m_Clients.find(node_name);

    if (worker_node == m_Clients.end())
        return 0;

    return x_ResetWaiting(worker_node->second, aff_registry);
}


unsigned short
CNSClientsRegistry::x_ResetWaiting(CNSClient &            client,
                                   CNSAffinityRegistry &  aff_registry)
{
    // We need to reset affinities the client might had been waiting for
    // and reset the waiting port.
    if (client.AnyWaitAffinity()) {
        aff_registry.RemoveWaitClientFromAffinities(client.GetID(),
                                                    client.GetWaitAffinities());
        client.ClearWaitAffinities();
    }
    return client.GetAndResetWaitPort();
}


static TNSBitVector     s_empty_vector = TNSBitVector();

TNSBitVector
CNSClientsRegistry::GetRunBlacklistedJobs(const CNSClientId &  client) const
{
    if (!client.IsComplete())
        return s_empty_vector;

    return GetRunBlacklistedJobs(client.GetNode());
}


TNSBitVector
CNSClientsRegistry::GetRunBlacklistedJobs(const string &  client_node) const
{
    CMutexGuard                         guard(m_Lock);
    map< string,
         CNSClient >::const_iterator    found = m_Clients.find(client_node);

    if (found == m_Clients.end())
        return s_empty_vector;

    return found->second.GetRunBlacklistedJobs();
}


TNSBitVector
CNSClientsRegistry::GetReadBlacklistedJobs(const CNSClientId &  client) const
{
    if (!client.IsComplete())
        return s_empty_vector;

    return GetReadBlacklistedJobs(client.GetNode());
}


TNSBitVector
CNSClientsRegistry::GetReadBlacklistedJobs(const string &  client_node) const
{
    CMutexGuard                         guard(m_Lock);
    map< string,
         CNSClient >::const_iterator    found = m_Clients.find(client_node);

    if (found == m_Clients.end())
        return s_empty_vector;

    return found->second.GetReadBlacklistedJobs();
}


TNSBitVector  CNSClientsRegistry::GetPreferredAffinities(
                                const CNSClientId &  client) const
{
    if (!client.IsComplete())
        return s_empty_vector;

    return GetPreferredAffinities(client.GetNode());
}


TNSBitVector  CNSClientsRegistry::GetPreferredAffinities(
                                const string &  node) const
{
    if (node.empty())
        return s_empty_vector;

    CMutexGuard                                 guard(m_Lock);
    map< string, CNSClient >::const_iterator    found = m_Clients.find(node);

    if (found == m_Clients.end())
        return s_empty_vector;

    return found->second.GetPreferredAffinities();
}


TNSBitVector  CNSClientsRegistry::GetAllPreferredAffinities(void) const
{
    CMutexGuard     guard(m_Lock);
    return m_WNAffinities;
}


TNSBitVector  CNSClientsRegistry::GetWaitAffinities(
                                const CNSClientId &  client) const
{
    if (!client.IsComplete())
        return s_empty_vector;

    return GetWaitAffinities(client.GetNode());
}


TNSBitVector  CNSClientsRegistry::GetWaitAffinities(
                                const string &  node) const
{
    if (node.empty())
        return s_empty_vector;

    CMutexGuard                                 guard(m_Lock);
    map< string, CNSClient >::const_iterator    found = m_Clients.find(node);

    if (found == m_Clients.end())
        return s_empty_vector;

    return found->second.GetWaitAffinities();
}


TNSBitVector  CNSClientsRegistry::GetRegisteredClients(void) const
{
    CMutexGuard         guard(m_Lock);
    return m_RegisteredClients;
}


void  CNSClientsRegistry::UpdatePreferredAffinities(
                                const CNSClientId &   client,
                                const TNSBitVector &  aff_to_add,
                                const TNSBitVector &  aff_to_del)
{
    if (!client.IsComplete())
        return;

    CMutexGuard                 guard(m_Lock);
    map< string,
         CNSClient >::iterator  found = m_Clients.find(client.GetNode());

    if (found == m_Clients.end())
        NCBI_THROW(CNetScheduleException, eInternalError,
                   "Cannot find client '" + client.GetNode() +
                   "' to update preferred affinities");

    found->second.AddPreferredAffinities(aff_to_add);
    found->second.RemovePreferredAffinities(aff_to_del);

    // Update the union bit vector with WN affinities
    if (aff_to_del.any())
        x_BuildWNAffinities();
    else
        m_WNAffinities |= aff_to_add;
}


bool  CNSClientsRegistry::UpdatePreferredAffinities(
                                const CNSClientId &   client,
                                unsigned int          aff_to_add,
                                unsigned int          aff_to_del)
{
    if (aff_to_add + aff_to_del == 0)
        return false;

    if (!client.IsComplete())
        return false;

    CMutexGuard                 guard(m_Lock);
    map< string,
         CNSClient >::iterator  found = m_Clients.find(client.GetNode());

    if (found == m_Clients.end())
        NCBI_THROW(CNetScheduleException, eInternalError,
                   "Cannot find client '" + client.GetNode() +
                   "' to update preferred affinities");

    bool    aff_added = found->second.AddPreferredAffinity(aff_to_add);
    found->second.RemovePreferredAffinity(aff_to_del);

    // Update the union bit vector with WN affinities
    if (aff_to_del != 0)
        x_BuildWNAffinities();
    else
        if (aff_added)
            m_WNAffinities.set_bit(aff_to_add);
    return aff_added;
}


void
CNSClientsRegistry::SetPreferredAffinities(const CNSClientId &   client,
                                           const TNSBitVector &  aff_to_set)
{
    if (!client.IsComplete())
        return;

    string                      client_name = client.GetNode();
    CMutexGuard                 guard(m_Lock);
    map< string,
         CNSClient >::iterator  found = m_Clients.find(client_name);

    if (found == m_Clients.end())
        NCBI_THROW(CNetScheduleException, eInternalError,
                   "Cannot find client '" + client.GetNode() +
                   "' to update preferred affinities");

    found->second.SetPreferredAffinities(aff_to_set);

    // Remove the client from the GC collected, because it affects affinities
    // only which are re-set here anyway
    m_GCClients.erase(client_name);
    x_BuildWNAffinities();
}


bool
CNSClientsRegistry::IsRequestedAffinity(
                            const string &         name,
                            const TNSBitVector &   aff,
                            bool                   use_preferred) const
{
    if (name.empty())
        return false;

    CMutexGuard                         guard(m_Lock);
    map< string,
         CNSClient >::const_iterator    worker_node = m_Clients.find(name);

    if (worker_node == m_Clients.end())
        return false;

    return worker_node->second.IsRequestedAffinity(aff, use_preferred);
}


bool  CNSClientsRegistry::IsPreferredByAny(unsigned int  aff_id) const
{
    CMutexGuard     guard(m_Lock);
    return m_WNAffinities[aff_id];
}


bool  CNSClientsRegistry::GetAffinityReset(const CNSClientId &   client) const
{
    if (!client.IsComplete())
        return false;

    CMutexGuard                                 guard(m_Lock);
    map< string, CNSClient >::const_iterator    found =
                                             m_Clients.find(client.GetNode());

    if (found == m_Clients.end())
        return false;

    return found->second.GetAffinityReset();
}


string  CNSClientsRegistry::GetNodeName(unsigned int  id) const
{
    CMutexGuard                                 guard(m_Lock);
    map< string, CNSClient >::const_iterator    k = m_Clients.begin();

    for ( ; k != m_Clients.end(); ++k )
        if (k->second.GetID() == id)
            return k->first;
    return "";
}


void
CNSClientsRegistry::StaleWNs(const CNSPreciseTime &  current_time,
                             const CNSPreciseTime &  timeout,
                             CNSAffinityRegistry &   aff_registry,
                             CNSNotificationList &   notif_registry,
                             bool                    is_log)
{
    // Checks if any of the worker nodes are inactive for too long
    bool                                    need_aff_rebuild = false;
    CMutexGuard                             guard(m_Lock);
    map< string, CNSClient >::iterator      k = m_Clients.begin();

    for ( ; k != m_Clients.end(); ++k ) {

        if ((k->second.GetType() & CNSClient::eWorkerNode) == 0)
            continue;
        if (k->second.GetState() != CNSClient::eActive)
            continue;

        // WN may wait on GET2 longer than the configured timeout.
        // Take the longest timeout of two if so.
        if (current_time - k->second.GetLastAccess() <= timeout)
            continue;


        unsigned short      wait_port = k->second.GetWaitPort();
        unsigned int        wait_address = k->second.GetPeerAddress();
        if (wait_port != 0) {
            CNSPreciseTime      get_lifetime = notif_registry.
                                GetPassiveNotificationLifetime(wait_address,
                                                               wait_port);
            if (current_time <= get_lifetime)
                continue;
        }

        // Here: the record has expired - clean the preferred affinities

        if (k->second.Purge(aff_registry)) {
            // Preferred affinities were reset
            need_aff_rebuild = true;

            if (is_log) {
                CRef<CRequestContext>   ctx;
                ctx.Reset(new CRequestContext());
                ctx->SetRequestID();
                GetDiagContext().SetRequestContext(ctx);
                GetDiagContext().PrintRequestStart()
                                .Print("_type", "worker_node_watch")
                                .Print("client_node", k->first)
                                .Print("client_session", k->second.GetSession())
                                .Print("preferred_affinities_reset", "yes");
                ctx->SetRequestStatus(CNetScheduleHandler::eStatus_OK);
                GetDiagContext().PrintRequestStop();
            }
        }

        wait_port = x_ResetWaiting(k->second, aff_registry);
        if (wait_port != 0)
            notif_registry.UnregisterListener(wait_address, wait_port);
    }

    if (need_aff_rebuild)
        x_BuildWNAffinities();
}


void  CNSClientsRegistry::Purge(const CNSPreciseTime &  current_time,
                                const CNSPreciseTime &  timeout_worker_node,
                                unsigned int            min_worker_nodes,
                                const CNSPreciseTime &  timeout_admin,
                                unsigned int            min_admins,
                                const CNSPreciseTime &  timeout_submitter,
                                unsigned int            min_submitters,
                                const CNSPreciseTime &  timeout_reader,
                                unsigned int            min_readers,
                                const CNSPreciseTime &  timeout_unknown,
                                unsigned int            min_unknowns,
                                bool                    is_log)
{
    CMutexGuard     guard(m_Lock);

    x_PurgeWNs(current_time, timeout_worker_node, min_worker_nodes, is_log);
    x_PurgeAdmins(current_time, timeout_admin, min_admins, is_log);
    x_PurgeSubmitters(current_time, timeout_submitter, min_submitters, is_log);
    x_PurgeReaders(current_time, timeout_reader, min_readers, is_log);
    x_PurgeUnknowns(current_time, timeout_unknown, min_unknowns, is_log);
}


unsigned int  CNSClientsRegistry::x_GetNextID(void)
{
    CFastMutexGuard     guard(m_LastIDLock);

    // 0 is an invalid value, so make sure the ID != 0
    ++m_LastID;
    if (m_LastID == 0)
        m_LastID = 1;
    return m_LastID;
}


// Must be called under the lock
void  CNSClientsRegistry::x_BuildWNAffinities(void)
{
    m_WNAffinities.clear();
    for (map< string, CNSClient >::const_iterator
            k = m_Clients.begin(); k != m_Clients.end(); ++k )
        if (k->second.GetType() & CNSClient::eWorkerNode)
            m_WNAffinities |= k->second.GetPreferredAffinities();
}


bool
CNSClientsRegistry::WasGarbageCollected(const CNSClientId &  client) const
{
    if (!client.IsComplete())
        return false;

    CMutexGuard                                 guard(m_Lock);
    return m_GCClients.find(client.GetNode()) != m_GCClients.end();
}


// Compares last access time of two clients basing on their identifiers
class AgeFunctor
{
    public:
        AgeFunctor(map< string, CNSClient > &  clients) :
            m_Clients(clients)
        {}

        bool operator () (const string &  lhs, const string &  rhs)
        {
            return m_Clients[ lhs ].GetLastAccess() <
                   m_Clients[ rhs ].GetLastAccess();
        }

    private:
        map< string, CNSClient > &    m_Clients;
};


// Must be called under the lock
void
CNSClientsRegistry::x_PurgeWNs(const CNSPreciseTime &  current_time,
                               const CNSPreciseTime &  timeout_worker_node,
                               unsigned int            min_worker_nodes,
                               bool                    is_log)
{
    map< string, CNSClient >::iterator      k = m_Clients.begin();
    list< string >                          inactive;
    unsigned int                            inactive_count = 0;
    unsigned int                            total_count = 0;

    // Count active and inactive worker nodes
    for ( ; k != m_Clients.end(); ++k ) {

        if ((k->second.GetType() & CNSClient::eWorkerNode) == 0)
            continue;

        ++total_count;
        if (k->second.GetState() != CNSClient::eActive) {
            if (current_time - k->second.GetLastAccess() >
                timeout_worker_node) {
                ++inactive_count;
                inactive.push_back(k->first);
            }
        }
    }

    if (total_count <= min_worker_nodes || inactive_count == 0)
        return;

    // Calculate the number of records to be deleted
    unsigned int    active_count = total_count - inactive_count;
    unsigned int    remove_count = 0;
    if (active_count >= min_worker_nodes)
        remove_count = inactive_count;
    else
        remove_count = total_count - min_worker_nodes;

    // Sort the inactive ones to delete the oldest
    inactive.sort(AgeFunctor(m_Clients));

    // Delete the oldest records
    for (list<string>::iterator  j = inactive.begin();
         j != inactive.end() && remove_count > 0; ++j, --remove_count) {

        // The affinity reset flag is NOT set if:
        // - the client issued CLEAR
        // - the client did not have preferred affinities
        // It makes sense to memo the client only if it had preferred
        // affinities.
        if (m_Clients[*j].GetAffinityReset())
            m_GCClients.insert(*j);

        if (m_GCClients.size() > 100000)
            ERR_POST("Garbage collected worker node list exceeds 100000 "
                     "records. There are currently " << m_GCClients.size() <<
                     " records.");
        m_Clients.erase(*j);
    }
}


// Must be called under the lock
void
CNSClientsRegistry::x_PurgeAdmins(const CNSPreciseTime &  current_time,
                                  const CNSPreciseTime &  timeout_admin,
                                  unsigned int            min_admins,
                                  bool                    is_log)
{
    x_PurgeInactiveClients(current_time, timeout_admin, min_admins,
                           CNSClient::eAdmin, is_log);
}


// Must be called under the lock
void
CNSClientsRegistry::x_PurgeSubmitters(const CNSPreciseTime &  current_time,
                                      const CNSPreciseTime &  timeout_submitter,
                                      unsigned int            min_submitters,
                                      bool                    is_log)
{
    x_PurgeInactiveClients(current_time, timeout_submitter, min_submitters,
                           CNSClient::eSubmitter, is_log);
}


// Must be called under the lock
void
CNSClientsRegistry::x_PurgeReaders(const CNSPreciseTime &  current_time,
                                   const CNSPreciseTime &  timeout_reader,
                                   unsigned int            min_readers,
                                   bool                    is_log)
{
    x_PurgeInactiveClients(current_time, timeout_reader, min_readers,
                           CNSClient::eReader, is_log);
}


// Must be called under the lock
void
CNSClientsRegistry::x_PurgeUnknowns(const CNSPreciseTime &  current_time,
                                    const CNSPreciseTime &  timeout_unknown,
                                    unsigned int            min_unknowns,
                                    bool                    is_log)
{
    x_PurgeInactiveClients(current_time, timeout_unknown, min_unknowns,
                           0, is_log);
}


void
CNSClientsRegistry::x_PurgeInactiveClients(const CNSPreciseTime &  current_time,
                                           const CNSPreciseTime &  timeout,
                                           unsigned int            min_clients,
                                           unsigned int            client_type,
                                           bool                    is_log)
{
    map< string, CNSClient >::iterator      k = m_Clients.begin();
    list< string >                          inactive;
    unsigned int                            inactive_count = 0;
    unsigned int                            total_count = 0;    // of given type

    // Count active and inactive admins
    for ( ; k != m_Clients.end(); ++k ) {

        if (client_type == 0) {
            if (k->second.GetType() != 0)
                continue;
        } else {
            if ((k->second.GetType() & client_type) == 0)
                continue;
            // Worker nodes are handled separately
            if (k->second.GetType() & CNSClient::eWorkerNode)
                continue;
        }

        ++total_count;
        if (current_time - k->second.GetLastAccess() > timeout) {
            ++inactive_count;
            inactive.push_back(k->first);
        }
    }

    if (total_count <= min_clients || inactive_count == 0)
        return;

    // Calculate the number of records to be deleted
    unsigned int    active_count = total_count - inactive_count;
    unsigned int    remove_count = 0;
    if (active_count >= min_clients)
        remove_count = inactive_count;
    else
        remove_count = total_count - min_clients;

    // Sort the inactive ones to delete the oldest
    inactive.sort(AgeFunctor(m_Clients));

    // Delete the oldest records
    for (list<string>::iterator  j = inactive.begin();
         j != inactive.end() && remove_count > 0; ++j, --remove_count) {
        m_Clients.erase(*j);
    }
    return;

}


END_NCBI_SCOPE

