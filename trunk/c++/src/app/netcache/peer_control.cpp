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

#include <ncbi_pch.hpp>

#include <connect/impl/server_connection.hpp>

#include "peer_control.hpp"
#include "active_handler.hpp"
#include "periodic_sync.hpp"


BEGIN_NCBI_SCOPE


typedef map<Uint8, CNCPeerControl*> TControlMap;
static CFastRWLock s_MapLock;
static TControlMap s_Controls;
static CAtomicCounter s_SyncOnInit;
static CAtomicCounter s_WaitToOpenToClients;

static CAtomicCounter s_MirrorQueueSize;
static FILE* s_MirrorLogFile = NULL;
CAtomicCounter CNCPeerControl::sm_TotalCopyRequests;
CAtomicCounter CNCPeerControl::sm_CopyReqsRejected;



SNCMirrorProlong::SNCMirrorProlong(ENCSyncEvent typ,
                                   Uint2 slot_,
                                   const string& key_,
                                   Uint8 rec_no,
                                   Uint8 tm,
                                   const CNCBlobAccessor* accessor)
    : SNCMirrorEvent(typ, slot_, key_, rec_no),
      orig_time(tm)
{
    blob_sum.create_id = accessor->GetCurCreateId();
    blob_sum.create_server = accessor->GetCurCreateServer();
    blob_sum.create_time = accessor->GetCurBlobCreateTime();
    blob_sum.dead_time = accessor->GetCurBlobDeadTime();
    blob_sum.expire = accessor->GetCurBlobExpire();
    blob_sum.ver_expire = accessor->GetCurVerExpire();
    blob_sum.size = accessor->GetCurBlobSize();
}


bool
CNCPeerControl::Initialize(void)
{
    s_MirrorQueueSize.Set(0);
    s_MirrorLogFile = fopen(CNCDistributionConf::GetMirroringSizeFile().c_str(), "a");
    sm_TotalCopyRequests.Set(0);
    sm_CopyReqsRejected.Set(0);
    return true;
}

void
CNCPeerControl::Finalize(void)
{
    if (s_MirrorLogFile)
        fclose(s_MirrorLogFile);
}

CNCPeerControl*
CNCPeerControl::Peer(Uint8 srv_id)
{
    CNCPeerControl* ctrl;
    s_MapLock.ReadLock();
    TControlMap::const_iterator it = s_Controls.find(srv_id);
    if (it != s_Controls.end()) {
        ctrl = it->second;
        s_MapLock.ReadUnlock();
        return ctrl;
    }
    s_MapLock.ReadUnlock();
    s_MapLock.WriteLock();
    it = s_Controls.find(srv_id);
    if (it == s_Controls.end()) {
        ctrl = new CNCPeerControl(srv_id);
        s_Controls[srv_id] = ctrl;
    }
    else {
        ctrl = it->second;
    }
    s_MapLock.WriteUnlock();
    return ctrl;
}

CNCPeerControl::CNCPeerControl(Uint8 srv_id)
    : m_SrvId(srv_id),
      m_FirstNWErrTime(0),
      m_NextSyncTime(0),
      m_ActiveConns(0),
      m_BGConns(0),
      m_SlotsToInitSync(0),
      m_CntActiveSyncs(0),
      m_CntNWErrors(0),
      m_InThrottle(false),
      m_HasBGTasks(false),
      m_InitiallySynced(false)
{}

void
CNCPeerControl::RegisterConnError(void)
{
    CFastMutexGuard guard(m_ObjLock);
    if (m_FirstNWErrTime == 0)
        m_FirstNWErrTime = CNetCacheServer::GetPreciseTime();
    if (++m_CntNWErrors >= CNCDistributionConf::GetCntErrorsToThrottle()) {
        m_InThrottle = true;
        m_ThrottleStart = CNetCacheServer::GetPreciseTime();
    }
}

void
CNCPeerControl::RegisterConnSuccess(void)
{
    CFastMutexGuard guard(m_ObjLock);
    m_InThrottle = false;
    m_FirstNWErrTime = 0;
    m_CntNWErrors = 0;
    m_ThrottleStart = 0;
}

bool
CNCPeerControl::CreateNewSocket(CNCActiveHandler* conn)
{
    if (m_InThrottle) {
        CFastMutexGuard guard(m_ObjLock);
        if (m_InThrottle) {
            Uint8 cur_time = CNetCacheServer::GetPreciseTime();
            Uint8 period = CNCDistributionConf::GetPeerThrottlePeriod();
            if (cur_time - m_ThrottleStart > period) {
                m_InThrottle = false;
                m_FirstNWErrTime = 0;
                m_CntNWErrors = 0;
                m_ThrottleStart = 0;
            }
            else {
                return false;
            }
        }
    }

    CNCActiveHandler_Proxy* proxy = new CNCActiveHandler_Proxy(conn);
    CServer_Connection* srv_conn = new CServer_Connection(proxy);
    conn->SetServerConn(srv_conn, proxy);
    STimeout timeout = {0, 0};
    EIO_Status conn_res = srv_conn->Connect(CSocketAPI::ntoa(Uint4(m_SrvId >> 32)),
                                            Uint2(m_SrvId), &timeout,
                                            fSOCK_LogOff | fSOCK_KeepAlive);
    if (conn_res != eIO_Success) {
        delete srv_conn;
        // proxy is deleted inside srv_conn
        RegisterConnError();
        return false;
    }
    return true;
}

CNCActiveHandler*
CNCPeerControl::x_GetPooledConnImpl(void)
{
    if (m_PooledConns.empty())
        return NULL;
    CNCActiveHandler* conn = m_PooledConns.back();
    m_PooledConns.pop_back();
    return conn;
}

CNCActiveHandler*
CNCPeerControl::GetPooledConn(void)
{
    CFastMutexGuard guard(m_ObjLock);
    return x_GetPooledConnImpl();
}

void
CNCPeerControl::AssignClientConn(CNCActiveClientHub* hub)
{
    m_ObjLock.Lock();
    if (m_ActiveConns >= CNCDistributionConf::GetMaxPeerTotalConns()) {
        hub->SetStatus(eNCHubWaitForConn);
        m_Clients.push_back(hub);
        m_ObjLock.Unlock();
        return;
    }
    ++m_ActiveConns;
    CNCActiveHandler* conn = x_GetPooledConnImpl();
    m_ObjLock.Unlock();

    if (!conn) {
        conn = new CNCActiveHandler(m_SrvId, this);
        if (!CreateNewSocket(conn)) {
            delete conn;
            m_ObjLock.Lock();
            if (m_ActiveConns == 0)
                abort();
            --m_ActiveConns;
            m_ObjLock.Unlock();
            hub->SetErrMsg("Cannot connect to peer");
            hub->SetStatus(eNCHubError);
            return;
        }
    }
    hub->SetStatus(eNCHubConnReady);
    hub->SetHandler(conn);
    conn->SetClientHub(hub);
    conn->SetReservedForBG(false);
}

inline void
CNCPeerControl::x_UpdateHasTasks(void)
{
    m_HasBGTasks = !m_SmallMirror.empty()  ||  !m_BigMirror.empty()
                   ||  !m_SyncList.empty();
}

bool
CNCPeerControl::x_ReserveBGConn(void)
{
    if (m_ActiveConns >= CNCDistributionConf::GetMaxPeerTotalConns()
        ||  m_BGConns >= CNCDistributionConf::GetMaxPeerBGConns())
    {
        return false;
    }
    ++m_ActiveConns;
    ++m_BGConns;
    return true;
}

CNCActiveHandler*
CNCPeerControl::x_GetBGConnImpl(void)
{
    CNCActiveHandler* conn = x_GetPooledConnImpl();
    m_ObjLock.Unlock();
    if (!conn) {
        conn = new CNCActiveHandler(m_SrvId, this);
        if (!CreateNewSocket(conn)) {
            delete conn;
            m_ObjLock.Lock();
            if (m_BGConns == 0  ||  m_ActiveConns == 0)
                abort();
            --m_BGConns;
            --m_ActiveConns;
            m_ObjLock.Unlock();
            return NULL;
        }
    }
    conn->SetReservedForBG(true);
    return conn;
}

CNCActiveHandler*
CNCPeerControl::GetBGConn(void)
{
    m_ObjLock.Lock();
    if (!x_ReserveBGConn()) {
        m_ObjLock.Unlock();
        return NULL;
    }
    // x_GetBGConn() releases m_ObjLock
    return x_GetBGConnImpl();
}

void
CNCPeerControl::PutConnToPool(CNCActiveHandler* conn)
{
    CFastMutexGuard guard(m_ObjLock);
    if (!m_Clients.empty()) {
        CNCActiveClientHub* hub = m_Clients.front();
        m_Clients.pop_front();
        hub->SetHandler(conn);
        conn->SetClientHub(hub);
        if (conn->IsReservedForBG()) {
            if (m_BGConns == 0)
                abort();
            --m_BGConns;
            conn->SetReservedForBG(false);
        }
        hub->SetStatus(eNCHubConnReady);
    }
    else if (m_HasBGTasks
             &&  (conn->IsReservedForBG()
                  ||  m_BGConns < CNCDistributionConf::GetMaxPeerBGConns()))
    {
        if (!conn->IsReservedForBG()) {
            ++m_BGConns;
            conn->SetReservedForBG(true);
        }

        if (!m_SmallMirror.empty()  ||  !m_BigMirror.empty()) {
            SNCMirrorEvent* event;
            if (!m_SmallMirror.empty()) {
                event = m_SmallMirror.back();
                m_SmallMirror.pop_back();
            }
            else {
                event = m_BigMirror.back();
                m_BigMirror.pop_back();
            }
            x_UpdateHasTasks();
            guard.Release();

            x_ProcessMirrorEvent(conn, event);
        }
        else if (!m_SyncList.empty()) {
            CNCActiveSyncControl* sync_ctrl = *m_NextTaskSync;
            SSyncTaskInfo task_info;
            if (!sync_ctrl->GetNextTask(task_info)) {
                TNCActiveSyncListIt cur_it = m_NextTaskSync;
                ++m_NextTaskSync;
                m_SyncList.erase(cur_it);
                if (m_NextTaskSync == m_SyncList.end())
                    m_NextTaskSync = m_SyncList.begin();
            }
            else if (++m_NextTaskSync == m_SyncList.end())
                m_NextTaskSync = m_SyncList.begin();
            x_UpdateHasTasks();
            guard.Release();

            sync_ctrl->ExecuteSyncTask(task_info, conn);
        }
        else
            abort();
    }
    else {
        if (m_ActiveConns == 0)
            abort();
        --m_ActiveConns;
        m_PooledConns.push_back(conn);
    }
}

void
CNCPeerControl::ReleaseConn(CNCActiveHandler* conn)
{
    CFastMutexGuard guard(m_ObjLock);
    if (conn->IsReservedForBG()) {
        if (m_BGConns == 0)
            abort();
        --m_BGConns;
    }
    if (m_ActiveConns == 0)
        abort();
    --m_ActiveConns;
}

void
CNCPeerControl::x_DeleteMirrorEvent(SNCMirrorEvent* event)
{
    if (event->evt_type == eSyncWrite)
        delete event;
    else if (event->evt_type == eSyncProlong)
        delete (SNCMirrorProlong*)event;
    else
        abort();
}

void
CNCPeerControl::x_ProcessMirrorEvent(CNCActiveHandler* conn, SNCMirrorEvent* event)
{
    if (event->evt_type == eSyncWrite) {
        conn->CopyPut(NULL, event->key, event->slot, event->orig_rec_no);
    }
    else if (event->evt_type == eSyncProlong) {
        SNCMirrorProlong* prolong = (SNCMirrorProlong*)event;
        conn->CopyProlong(prolong->key, prolong->slot, prolong->orig_rec_no,
                          prolong->orig_time, prolong->blob_sum);
    }
    else
        abort();
    x_DeleteMirrorEvent(event);
}

void
CNCPeerControl::x_AddMirrorEvent(SNCMirrorEvent* event, Uint8 size)
{
    sm_TotalCopyRequests.Add(1);

    m_ObjLock.Lock();
    if (x_ReserveBGConn()) {
        // x_GetBGConnImpl() releases m_ObjLock
        CNCActiveHandler* conn = x_GetBGConnImpl();
        if (conn)
            x_ProcessMirrorEvent(conn, event);
        else
            x_DeleteMirrorEvent(event);
    }
    else {
        TNCMirrorQueue* q;
        if (size <= CNCDistributionConf::GetSmallBlobBoundary())
            q = &m_SmallMirror;
        else
            q = &m_BigMirror;
        if (q->size() < CNCDistributionConf::GetMaxMirrorQueueSize()) {
            q->push_back(event);
            m_HasBGTasks = true;
            m_ObjLock.Unlock();

            int queue_size = s_MirrorQueueSize.Add(1);
            if (s_MirrorLogFile) {
                Uint8 cur_time = CNetCacheServer::GetPreciseTime();
                fprintf(s_MirrorLogFile, "%lu,%d\n", cur_time, queue_size);
            }
        }
        else {
            m_ObjLock.Unlock();
            sm_CopyReqsRejected.Add(1);
            x_DeleteMirrorEvent(event);
        }
    }
}

void
CNCPeerControl::MirrorWrite(const string& key,
                          Uint2 slot,
                          Uint8 orig_rec_no,
                          Uint8 size)
{
    const TServersList& servers = CNCDistributionConf::GetRawServersForSlot(slot);
    ITERATE(TServersList, it_srv, servers) {
        Uint8 srv_id = *it_srv;
        CNCPeerControl* peer = Peer(srv_id);
        SNCMirrorEvent* event = new SNCMirrorEvent(eSyncWrite, slot, key, orig_rec_no);
        peer->x_AddMirrorEvent(event, size);
    }
}

void
CNCPeerControl::MirrorProlong(const string& key,
                            Uint2 slot,
                            Uint8 orig_rec_no,
                            Uint8 orig_time,
                            const CNCBlobAccessor* accessor)
{
    const TServersList& servers = CNCDistributionConf::GetRawServersForSlot(slot);
    ITERATE(TServersList, it_srv, servers) {
        Uint8 srv_id = *it_srv;
        CNCPeerControl* peer = Peer(srv_id);
        SNCMirrorProlong* event = new SNCMirrorProlong(eSyncProlong, slot, key,
                                               orig_rec_no, orig_time, accessor);
        peer->x_AddMirrorEvent(event, 0);
    }
}

Uint8
CNCPeerControl::GetMirrorQueueSize(void)
{
    return s_MirrorQueueSize.Get();
}

Uint8
CNCPeerControl::GetMirrorQueueSize(Uint8 srv_id)
{
    CNCPeerControl* peer = Peer(srv_id);
    CFastMutexGuard guard(peer->m_ObjLock);
    return peer->m_SmallMirror.size() + peer->m_BigMirror.size();
}

void
CNCPeerControl::SetServersForInitSync(Uint4 cnt_servers)
{
    s_SyncOnInit.Set(cnt_servers);
    s_WaitToOpenToClients.Set(cnt_servers);
}

bool
CNCPeerControl::HasServersForInitSync(void)
{
    return s_SyncOnInit.Get() != 0;
}

bool
CNCPeerControl::StartActiveSync(void)
{
    CFastMutexGuard guard(m_ObjLock);
    if (m_CntActiveSyncs >= CNCDistributionConf::GetMaxSyncsOneServer())
        return false;
    ++m_CntActiveSyncs;
    return true;
}

void
CNCPeerControl::x_SrvInitiallySynced(void)
{
    if (!m_InitiallySynced) {
        INFO_POST("Initial sync for " << m_SrvId << " completed");
        m_InitiallySynced = true;
        s_SyncOnInit.Add(-1);
    }
}

void
CNCPeerControl::x_SlotsInitiallySynced(Uint2 cnt_slots)
{
    if (cnt_slots != 0  &&  m_SlotsToInitSync != 0) {
        if (cnt_slots != 1) {
            LOG_POST("Server " << m_SrvId << " is out of reach");
        }
        m_SlotsToInitSync -= cnt_slots;
        if (m_SlotsToInitSync == 0) {
            x_SrvInitiallySynced();
            if (s_WaitToOpenToClients.Add(-1) == 0)
                CNetCacheServer::InitialSyncComplete();
        }
    }
}

void
CNCPeerControl::AddInitiallySyncedSlot(void)
{
    CFastMutexGuard guard(m_ObjLock);
    x_SlotsInitiallySynced(1);
}

void
CNCPeerControl::RegisterSyncStop(bool is_passive, Uint8 next_sync_delay)
{
    CFastMutexGuard guard(m_ObjLock);
    Uint8 now = CNetCacheServer::GetPreciseTime();
    Uint8 next_time = now + next_sync_delay;
    if (m_FirstNWErrTime == 0) {
        g_SetNextTime(m_NextSyncTime, now, false);
    }
    else {
        g_SetNextTime(m_NextSyncTime, next_time, true);
        if (m_InThrottle)
            x_SrvInitiallySynced();
        if (now - m_FirstNWErrTime >= CNCDistributionConf::GetNetworkErrorTimeout())
        {
            x_SlotsInitiallySynced(m_SlotsToInitSync);
        }
    }

    if (!is_passive)
        --m_CntActiveSyncs;
}

bool
CNCPeerControl::AddSyncControl(CNCActiveSyncControl* sync_ctrl)
{
    bool has_more = true;
    SSyncTaskInfo task_info;

    m_ObjLock.Lock();
    while (has_more  &&  x_ReserveBGConn()) {
        // x_GetBGConnImpl() releases m_ObjLock
        CNCActiveHandler* conn = x_GetBGConnImpl();
        if (!conn)
            return false;
        has_more = sync_ctrl->GetNextTask(task_info);
        sync_ctrl->ExecuteSyncTask(task_info, conn);
        m_ObjLock.Lock();
    }
    if (has_more) {
        m_SyncList.push_back(sync_ctrl);
        m_HasBGTasks = true;
    }
    m_ObjLock.Unlock();

    return true;
}

bool
CNCPeerControl::FinishSync(CNCActiveSyncControl* sync_ctrl)
{
    m_ObjLock.Lock();
    if (x_ReserveBGConn()) {
        // x_GetBGConnImpl() releases m_ObjLock
        CNCActiveHandler* conn = x_GetBGConnImpl();
        if (!conn)
            return false;

        SSyncTaskInfo task_info;
        sync_ctrl->GetNextTask(task_info);
        sync_ctrl->ExecuteSyncTask(task_info, conn);
    }
    else {
        m_SyncList.push_back(sync_ctrl);
        m_HasBGTasks = true;
        m_ObjLock.Unlock();
    }
    return true;
}

END_NCBI_SCOPE
