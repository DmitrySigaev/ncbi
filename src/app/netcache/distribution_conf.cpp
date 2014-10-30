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
 * Authors: Denis Vakatov, Pavel Ivanov, Sergey Satskiy
 *
 * File Description: Data structures and API to support blobs mirroring.
 *
 */


#include "nc_pch.hpp"

#include <corelib/ncbireg.hpp>
#include <util/checksum.hpp>
#include <util/random_gen.hpp>

#include "task_server.hpp"

#include "netcached.hpp"
#include "distribution_conf.hpp"
#include "peer_control.hpp"


BEGIN_NCBI_SCOPE


struct SSrvGroupInfo
{
    Uint8   srv_id;
    string  grp;

    SSrvGroupInfo(Uint8 srv, const string& group)
        : srv_id(srv), grp(group)
    {}
};

typedef vector<SSrvGroupInfo>       TSrvGroupsList;
typedef map<Uint2, TSrvGroupsList>  TSrvGroupsMap;

typedef map<Uint2, TServersList>    TSlot2SrvMap;
typedef map<Uint8, vector<Uint2> >  TSrv2SlotMap;

struct SSrvMirrorInfo
{
    TNCPeerList   s_Peers;
    TSlot2SrvMap  s_RawSlot2Servers;
    TSrvGroupsMap s_Slot2Servers;
    TSrv2SlotMap  s_CommonSlots;
};
static SSrvMirrorInfo* s_MirrorConf = NULL;
#if 0
static CMiniMutex s_MirrorConf;
static TNCPeerList s_Peers;
static TSlot2SrvMap s_RawSlot2Servers;
static TSrvGroupsMap s_Slot2Servers;
static TSrv2SlotMap s_CommonSlots;
#endif
static Uint2    s_MaxSlotNumber = 0;

static vector<Uint2> s_SelfSlots;
static Uint2    s_CntSlotBuckets = 0;
static Uint2    s_CntTimeBuckets = 0;
static Uint4    s_SlotRndShare  = numeric_limits<Uint4>::max();
static Uint4    s_TimeRndShare  = numeric_limits<Uint4>::max();
static Uint8    s_SelfID        = 0;
static Uint4    s_SelfIP        = 0;
static string   s_SelfGroup;
static string   s_SelfName;
static CMiniMutex s_KeyRndLock;
static CRandom  s_KeyRnd(CRandom::TValue(time(NULL)));
static string   s_SelfHostIP;
static CAtomicCounter s_BlobId;
static Uint4    s_SelfAlias = 0;
static string   s_MirroringSizeFile;
static string   s_PeriodicLogFile;
static string   s_CopyDelayLogFile;
static FILE*    s_CopyDelayLog = NULL;
static Uint1    s_CntActiveSyncs = 4;
static Uint1    s_MaxSyncsOneServer = 2;
static Uint1    s_SyncPriority = 10;
static Uint2    s_MaxPeerTotalConns = 100;
static Uint2    s_MaxPeerBGConns = 50;
static Uint2    s_CntErrorsToThrottle = 10;
static Uint2    s_CntThrottlesToIpchange = 10;
static Uint8    s_PeerThrottlePeriod = 10 * kUSecsPerSecond;
static Uint1    s_PeerTimeout = 2;
static Uint1    s_BlobListTimeout = 10;
static Uint8    s_SmallBlobBoundary = 65535;
static Uint2    s_MaxMirrorQueueSize = 10000;
static string   s_SyncLogFileName;
static Uint4    s_MaxSlotLogEvents = 0;
static Uint4    s_CleanLogReserve = 0;
static Uint4    s_MaxCleanLogBatch = 0;
static Uint8    s_MinForcedCleanPeriod = 0;
static Uint4    s_CleanAttemptInterval = 0;
static Uint8    s_PeriodicSyncInterval = 0;
//static Uint8    s_PeriodicSyncHeadTime;
//static Uint8    s_PeriodicSyncTailTime;
static Uint8    s_PeriodicSyncTimeout = 0;
static Uint8    s_FailedSyncRetryDelay = 0;
static Uint8    s_NetworkErrorTimeout = 0;
static Uint8    s_MaxBlobSizeSync = 0;
static bool     s_WarnBlobSizeSync = true;
static bool     s_BlobUpdateHotline = true;

static const char*  kNCReg_NCPoolSection       = "mirror";
static string       kNCReg_NCServerPrefix      = "server_";
static string       kNCReg_NCServerSlotsPrefix = "srv_slots_";


bool
CNCDistributionConf::Initialize(Uint2 control_port)
{
    string log_pfx("Bad configuration: ");
    const CNcbiRegistry& reg = CTaskServer::GetConfRegistry();

    Uint4 self_host = CTaskServer::GetIPByHost(CTaskServer::GetHostName());
    s_SelfHostIP = CTaskServer::IPToString(self_host);
    s_SelfIP = self_host;
    s_SelfID = (Uint8(self_host) << 32) + control_port;
    s_SelfAlias = CNCDistributionConf::CreateHostAlias(self_host, control_port);
    s_BlobId.Set(0);

    string err_message;
    if (!InitMirrorConfig(reg, err_message)) {
        SRV_LOG(Critical, log_pfx << err_message);
        return false;
    }

    if (s_MaxSlotNumber <= 1) {
        s_MaxSlotNumber = 1;
        s_SlotRndShare = numeric_limits<Uint4>::max();
    }
    else {
        s_SlotRndShare = numeric_limits<Uint4>::max() / s_MaxSlotNumber + 1;
    }

    s_MirroringSizeFile = reg.Get(kNCReg_NCPoolSection, "mirroring_log_file");
    s_PeriodicLogFile   = reg.Get(kNCReg_NCPoolSection, "periodic_log_file");

    s_CopyDelayLogFile = reg.Get(kNCReg_NCPoolSection, "copy_delay_log_file");
    if (!s_CopyDelayLogFile.empty()) {
        s_CopyDelayLog = fopen(s_CopyDelayLogFile.c_str(), "a");
    }

    try {
        s_CntSlotBuckets = reg.GetInt(kNCReg_NCPoolSection, "cnt_slot_buckets", 10);
        if (numeric_limits<Uint2>::max() / s_CntSlotBuckets < s_MaxSlotNumber) {
            SRV_LOG(Critical, log_pfx << "too many buckets per slot ("
                              << s_CntSlotBuckets << ") with given number of slots ("
                              << s_MaxSlotNumber << ").");
            return false;
        }
        s_CntTimeBuckets = s_CntSlotBuckets * s_MaxSlotNumber;
        s_TimeRndShare = s_SlotRndShare / s_CntSlotBuckets + 1;
        s_CntActiveSyncs = reg.GetInt(kNCReg_NCPoolSection, "max_active_syncs", 4);
        s_MaxSyncsOneServer = reg.GetInt(kNCReg_NCPoolSection, "max_syncs_one_server", 2);
        s_SyncPriority = reg.GetInt(kNCReg_NCPoolSection, "deferred_priority", 10);
        s_MaxPeerTotalConns = reg.GetInt(kNCReg_NCPoolSection, "max_peer_connections", 100);
        s_MaxPeerBGConns = reg.GetInt(kNCReg_NCPoolSection, "max_peer_bg_connections", 50);
        s_CntErrorsToThrottle = reg.GetInt(kNCReg_NCPoolSection, "peer_errors_for_throttle", 10);
        s_CntThrottlesToIpchange = reg.GetInt(kNCReg_NCPoolSection, "peer_throttles_for_ip_change", 10);
        s_PeerThrottlePeriod = reg.GetInt(kNCReg_NCPoolSection, "peer_throttle_period", 10);
        s_PeerThrottlePeriod *= kUSecsPerSecond;
        s_PeerTimeout = reg.GetInt(kNCReg_NCPoolSection, "peer_communication_timeout", 2);
        s_BlobListTimeout = reg.GetInt(kNCReg_NCPoolSection, "peer_blob_list_timeout", 10);
        s_SmallBlobBoundary = reg.GetInt(kNCReg_NCPoolSection, "small_blob_max_size", 100);
        s_SmallBlobBoundary *= 1000;
        s_MaxMirrorQueueSize = reg.GetInt(kNCReg_NCPoolSection, "max_instant_queue_size", 10000);

        s_SyncLogFileName = reg.GetString(kNCReg_NCPoolSection, "sync_log_file", "sync_events.log");
        s_MaxSlotLogEvents = reg.GetInt(kNCReg_NCPoolSection, "max_slot_log_records", 100000);
        if (s_MaxSlotLogEvents < 10)
            s_MaxSlotLogEvents = 10;
        s_CleanLogReserve = reg.GetInt(kNCReg_NCPoolSection, "clean_slot_log_reserve", 1000);
        if (s_CleanLogReserve >= s_MaxSlotLogEvents)
            s_CleanLogReserve = s_MaxSlotLogEvents - 1;
        s_MaxCleanLogBatch = reg.GetInt(kNCReg_NCPoolSection, "max_clean_log_batch", 10000);
        s_MinForcedCleanPeriod = reg.GetInt(kNCReg_NCPoolSection, "min_forced_clean_log_period", 10);
        s_MinForcedCleanPeriod *= kUSecsPerSecond;
        s_CleanAttemptInterval = reg.GetInt(kNCReg_NCPoolSection, "clean_log_attempt_interval", 1);
        s_PeriodicSyncInterval = reg.GetInt(kNCReg_NCPoolSection, "deferred_sync_interval", 10);
        s_PeriodicSyncInterval *= kUSecsPerSecond;
        s_PeriodicSyncTimeout = reg.GetInt(kNCReg_NCPoolSection, "deferred_sync_timeout", 10);
        s_PeriodicSyncTimeout *= kUSecsPerSecond;
        s_FailedSyncRetryDelay = reg.GetInt(kNCReg_NCPoolSection, "failed_sync_retry_delay", 1);
        s_FailedSyncRetryDelay *= kUSecsPerSecond;
        s_NetworkErrorTimeout = reg.GetInt(kNCReg_NCPoolSection, "network_error_timeout", 300);
        s_NetworkErrorTimeout *= kUSecsPerSecond;
        s_MaxBlobSizeSync = NStr::StringToUInt8_DataSize(reg.GetString(
                           kNCReg_NCPoolSection, "max_blob_size_sync", "1 GB"));
        s_WarnBlobSizeSync  =  reg.GetBool( kNCReg_NCPoolSection, "warn_blob_size_sync", true);
        if (s_MaxBlobSizeSync == 0) {
            s_WarnBlobSizeSync = false;
        }
        s_BlobUpdateHotline =  reg.GetBool( kNCReg_NCPoolSection, "blob_update_hotline", true);

        if (s_WarnBlobSizeSync && s_SmallBlobBoundary > s_MaxBlobSizeSync) {
            SRV_LOG(Critical, log_pfx << "small_blob_max_size ("
                              << s_SmallBlobBoundary << ") is greater than max_blob_size_sync ("
                              << s_MaxBlobSizeSync << ").");
            return false;
        }
    }
    catch (CStringException& ex) {
        SRV_LOG(Critical, log_pfx  << ex);
        return false;
    }
    return true;
}

bool
CNCDistributionConf::InitMirrorConfig(const CNcbiRegistry& reg, string& err_message)
{
    SSrvMirrorInfo* mirrorCfg = new SSrvMirrorInfo;
    SSrvMirrorInfo* prevMirrorCfg = ACCESS_ONCE(s_MirrorConf);
    bool isReconf = prevMirrorCfg != NULL;
    vector<Uint2> self_slots;

    string reg_value;
    bool found_self = false;
    for (int srv_idx = 0; ; ++srv_idx) {
        string value_name = kNCReg_NCServerPrefix + NStr::NumericToString(srv_idx);
        reg_value = reg.Get(kNCReg_NCPoolSection, value_name.c_str());
        if (reg_value.empty())
            break;

        string srv_name = reg_value;
        list<CTempString> srv_fields;
        NStr::Split(reg_value, ":", srv_fields);
        if (srv_fields.size() != 3) {
            err_message = srv_name + ": Invalid peer server specification";
            goto do_error;
        }
        list<CTempString>::const_iterator it_fields = srv_fields.begin();
        string grp_name = *it_fields;
        ++it_fields;
        string host_str = *it_fields;
        Uint4 host = CTaskServer::GetIPByHost(host_str);
        ++it_fields;
        string port_str = *it_fields;
        Uint2 port = NStr::StringToUInt(port_str, NStr::fConvErr_NoThrow);
        if (host == 0  ||  port == 0) {
            err_message = srv_name + ": Host not found";
            goto do_error;
        }
        Uint8 srv_id = (Uint8(host) << 32) + port;
        string peer_str = host_str + ":" + port_str;
        if (srv_id == s_SelfID) {
            if (found_self) {
                err_message = srv_name + ": Host described twice";
                goto do_error;
            }
            found_self = true;
            if (isReconf) {
                if (s_SelfGroup != grp_name || s_SelfName != peer_str) {
                    err_message = srv_name + ": Changes in self description prohibited (group or name)";
                    goto do_error;
                }
            } else {
                s_SelfGroup = grp_name;
                s_SelfName = peer_str;
            }
        }
        else {
            if (mirrorCfg->s_Peers.find(srv_id) != mirrorCfg->s_Peers.end()) {
                err_message = srv_name +  + ": Described twice";
                goto do_error;
            }
            mirrorCfg->s_Peers[srv_id] = peer_str;
        }

        // There must be corresponding description of slots
        value_name = kNCReg_NCServerSlotsPrefix + NStr::NumericToString(srv_idx);
        reg_value = reg.Get(kNCReg_NCPoolSection, value_name.c_str());
        if (reg_value.empty()) {
            err_message = srv_name + ": No slots for server";
            goto do_error;
        }

        list<string> values;
        NStr::Split(reg_value, ",", values);
        ITERATE(list<string>, it, values) {
            Uint2 slot = NStr::StringToUInt(*it,
                NStr::fConvErr_NoThrow | NStr::fAllowLeadingSpaces | NStr::fAllowTrailingSpaces);
            if (slot == 0) {
                err_message = srv_name + ": Bad slot number: " + string(*it);
                goto do_error;
            }
            TServersList& srvs = mirrorCfg->s_RawSlot2Servers[slot];
            if (find(srvs.begin(), srvs.end(), srv_id) != srvs.end()) {
                err_message = srv_name + ": Slot listed twice: " + string(*it);
                goto do_error;
            }
            if (srv_id == s_SelfID) {
                vector<Uint2>& slots = isReconf ? self_slots : s_SelfSlots;
                if (find(slots.begin(), slots.end(), slot) != slots.end()) {
                    err_message = srv_name + ": Slot listed twice: " + string(*it);
                    goto do_error;
                }
                slots.push_back(slot);
            } else {
                srvs.push_back(srv_id);
                mirrorCfg->s_Slot2Servers[slot].push_back(SSrvGroupInfo(srv_id, grp_name));
            }
            if (isReconf) {
                if (slot > s_MaxSlotNumber) {
                    err_message = srv_name + ": Slot numbers cannot exceed " + NStr::NumericToString(s_MaxSlotNumber);
                    goto do_error;
                }
            } else {
                s_MaxSlotNumber = max(slot, s_MaxSlotNumber);
            }
        }
    }
    if (!found_self) {
        if (mirrorCfg->s_Peers.size() != 0) {
            err_message = CTaskServer::GetHostName() + ": Self description not found";
            goto do_error;
        }
        if (!isReconf) {
            s_SelfSlots.push_back(1);
            s_SelfGroup = "grp1";
        }
    }
    if (isReconf && !self_slots.empty()) {
        if (!std::equal(s_SelfSlots.begin(), s_SelfSlots.end(), self_slots.begin())) {
            err_message = s_SelfName + ": Changes in self description prohibited (slots)";
            goto do_error;
        }
    }

    ITERATE(TNCPeerList, it_peer, mirrorCfg->s_Peers)  {
        Uint8 srv_id = it_peer->first;
        vector<Uint2>& common_slots = mirrorCfg->s_CommonSlots[srv_id];
        ITERATE(TSlot2SrvMap, it_slot, mirrorCfg->s_RawSlot2Servers) {
            Uint2 slot = it_slot->first;
            if (find(s_SelfSlots.begin(), s_SelfSlots.end(), slot) == s_SelfSlots.end())
                continue;
            const TServersList& srvs = it_slot->second;
            if (find(srvs.begin(), srvs.end(), srv_id) != srvs.end())
                common_slots.push_back(it_slot->first);
        }
    }

    if (AtomicCAS(s_MirrorConf, prevMirrorCfg, mirrorCfg)) {
// pre-create all peers
        const TNCPeerList& peers = CNCDistributionConf::GetPeers();
        ITERATE(TNCPeerList, p, peers) {
            CNCPeerControl::Peer(p->first);
        }
        return true;
    }
do_error:
    delete mirrorCfg;
    return false;
}

bool
CNCDistributionConf::ReConfig(CNcbiRegistry& new_reg, string& err_message)
// we only add or remove peer servers, nothing else
{
    if (!InitMirrorConfig(new_reg, err_message)) {
        return false;
    }
// modify old registry to remember the changes
    CNcbiRegistry& old_reg = CTaskServer::SetConfRegistry();
    // remove old peers
    string value_name, value;
    size_t srv_idx;
    for (srv_idx = 0; ; ++srv_idx) {
        value_name = kNCReg_NCServerPrefix + NStr::NumericToString(srv_idx);
        value = old_reg.Get(kNCReg_NCPoolSection, value_name);
        if (value.empty()) {
            break;
        }
        old_reg.Set(kNCReg_NCPoolSection, value_name, kEmptyStr, CNcbiRegistry::fPersistent);
        value_name = kNCReg_NCServerSlotsPrefix + NStr::NumericToString(srv_idx);
        old_reg.Set(kNCReg_NCPoolSection, value_name, kEmptyStr, CNcbiRegistry::fPersistent);
    }
    // add new ones
    for (srv_idx = 0; ; ++srv_idx) {
        value_name = kNCReg_NCServerPrefix + NStr::NumericToString(srv_idx);
        value = new_reg.Get(kNCReg_NCPoolSection, value_name);
        if (value.empty())
            break;
        old_reg.Set(kNCReg_NCPoolSection, value_name, value, CNcbiRegistry::fPersistent);
        value_name = kNCReg_NCServerSlotsPrefix + NStr::NumericToString(srv_idx);
        value = new_reg.Get(kNCReg_NCPoolSection, value_name);
        old_reg.Set(kNCReg_NCPoolSection, value_name, value, CNcbiRegistry::fPersistent);
    }

#if 0
// compare (I am not sure though that slot lists should be identical)
    map<string, set<Uint2> >::const_iterator pn, po;
    for(pn = new_peers.begin(); pn != new_peers.end(); ++pn) {
        po = old_peers.find(pn->first); 
        if (po != old_peers.end()) {
            if (pn->second.size() != po->second.size() ||
                !std::equal(pn->second.begin(), pn->second.end(), po->second.begin())) {
                err_message = pn->first + ": Slot lists differ in old and new configurations";
                return false;
            }
        }
    }
    for(po = old_peers.begin(); po != old_peers.end(); ++po) {
        pn = new_peers.find(po->first); 
        if (pn != new_peers.end()) {
            if (po->second.size() != pn->second.size() ||
                !std::equal(po->second.begin(), po->second.end(), pn->second.begin())) {
                err_message = po->first + ": Slot lists differ in old and new configurations";
                return false;
            }
        }
    }

// remove common peers
    // here I assume common peers have identical slot lists
    for(pn = new_peers.begin(); pn != new_peers.end(); ++pn) {
        po = old_peers.find(pn->first); 
        if (po != old_peers.end()) {
            old_peers.erase(po);
        }
    }
    for(po = old_peers.begin(); po != old_peers.end(); ++po) {
        pn = new_peers.find(po->first); 
        if (pn != new_peers.end()) {
            new_peers.erase(pn);
        }
    }
#endif
    return true;
}

void
CNCDistributionConf::Finalize(void)
{
    if (s_CopyDelayLog)
        fclose(s_CopyDelayLog);
}

void CNCDistributionConf::WriteSetup(CSrvSocketTask& task)
{
    string is("\": "),iss("\": \""), eol(",\n\""), str("_str"), eos("\"");

    //self
    task.WriteText(eol).WriteText(kNCReg_NCServerPrefix).WriteNumber(0).WriteText(iss);
    task.WriteText(s_SelfName).WriteText("\"");
    task.WriteText(eol).WriteText(kNCReg_NCServerSlotsPrefix).WriteNumber(0).WriteText(is).WriteText("[");
    ITERATE( vector<Uint2>, s, s_SelfSlots) {
        if (s != s_SelfSlots.begin()) {
            task.WriteText(",");
        }
        task.WriteNumber(*s);
    }
    task.WriteText("]");

    // peers
    int idx=1;
    SSrvMirrorInfo* mirrorConf = ACCESS_ONCE(s_MirrorConf);
    ITERATE( TNCPeerList, p, mirrorConf->s_Peers) {
        task.WriteText(eol).WriteText(kNCReg_NCServerPrefix).WriteNumber(idx).WriteText(iss);
        task.WriteText(p->second).WriteText("\"");

        Uint8 srv_id = p->first;
        vector<Uint2> slots;
        // find slots servers by this peer
        ITERATE(TSlot2SrvMap, it_slot, mirrorConf->s_RawSlot2Servers) {
            Uint2 slot = it_slot->first;
            const TServersList& srvs = it_slot->second;
            if (find(srvs.begin(), srvs.end(), srv_id) != srvs.end()) {
                slots.push_back(slot);
            }
        }
        task.WriteText(eol).WriteText(kNCReg_NCServerSlotsPrefix).WriteNumber(idx).WriteText(is).WriteText("[");
        ITERATE( vector<Uint2>, s, slots) {
            if (s != slots.begin()) {
                task.WriteText(",");
            }
            task.WriteNumber(*s);
        }
        task.WriteText("]");
        ++idx;
    }

    task.WriteText(eol).WriteText("mirroring_log_file" ).WriteText(iss).WriteText(s_MirroringSizeFile).WriteText("\"");
    task.WriteText(eol).WriteText("periodic_log_file"  ).WriteText(iss).WriteText(s_PeriodicLogFile  ).WriteText("\"");
    task.WriteText(eol).WriteText("copy_delay_log_file").WriteText(iss).WriteText(s_CopyDelayLogFile ).WriteText("\"");
    task.WriteText(eol).WriteText("sync_log_file"      ).WriteText(iss).WriteText(s_SyncLogFileName  ).WriteText("\"");

    task.WriteText(eol).WriteText("cnt_slot_buckets"           ).WriteText(is).WriteNumber(s_CntSlotBuckets);
    task.WriteText(eol).WriteText("max_active_syncs"           ).WriteText(is).WriteNumber(s_CntActiveSyncs);
    task.WriteText(eol).WriteText("max_syncs_one_server"       ).WriteText(is).WriteNumber(s_MaxSyncsOneServer);
    task.WriteText(eol).WriteText("deferred_priority"          ).WriteText(is).WriteNumber(s_SyncPriority);
    task.WriteText(eol).WriteText("max_peer_connections"       ).WriteText(is).WriteNumber(s_MaxPeerTotalConns);
    task.WriteText(eol).WriteText("max_peer_bg_connections"    ).WriteText(is).WriteNumber(s_MaxPeerBGConns);
    task.WriteText(eol).WriteText("peer_errors_for_throttle"   ).WriteText(is).WriteNumber(s_CntErrorsToThrottle);
    task.WriteText(eol).WriteText("peer_throttles_for_ip_change").WriteText(is).WriteNumber(s_CntThrottlesToIpchange);
    task.WriteText(eol).WriteText("peer_throttle_period"       ).WriteText(is).WriteNumber(s_PeerThrottlePeriod/kUSecsPerSecond);
    task.WriteText(eol).WriteText("peer_communication_timeout" ).WriteText(is).WriteNumber(s_PeerTimeout);
    task.WriteText(eol).WriteText("peer_blob_list_timeout"     ).WriteText(is).WriteNumber(s_BlobListTimeout);
    task.WriteText(eol).WriteText("small_blob_max_size"        ).WriteText(is).WriteNumber(s_SmallBlobBoundary/1000);
    task.WriteText(eol).WriteText("max_instant_queue_size"     ).WriteText(is).WriteNumber(s_MaxMirrorQueueSize);
    task.WriteText(eol).WriteText("max_slot_log_records"       ).WriteText(is).WriteNumber(s_MaxSlotLogEvents);
    task.WriteText(eol).WriteText("clean_slot_log_reserve"     ).WriteText(is).WriteNumber(s_CleanLogReserve);
    task.WriteText(eol).WriteText("max_clean_log_batch"        ).WriteText(is).WriteNumber(s_MaxCleanLogBatch);
    task.WriteText(eol).WriteText("min_forced_clean_log_period").WriteText(is).WriteNumber(s_MinForcedCleanPeriod/kUSecsPerSecond);
    task.WriteText(eol).WriteText("clean_log_attempt_interval" ).WriteText(is).WriteNumber(s_CleanAttemptInterval);
    task.WriteText(eol).WriteText("deferred_sync_interval"     ).WriteText(is).WriteNumber(s_PeriodicSyncInterval/kUSecsPerSecond);
    task.WriteText(eol).WriteText("deferred_sync_timeout"      ).WriteText(is).WriteNumber(s_PeriodicSyncTimeout/ kUSecsPerSecond);
    task.WriteText(eol).WriteText("failed_sync_retry_delay"    ).WriteText(is).WriteNumber(s_FailedSyncRetryDelay/kUSecsPerSecond);
    task.WriteText(eol).WriteText("network_error_timeout"      ).WriteText(is).WriteNumber(s_NetworkErrorTimeout/ kUSecsPerSecond);
    task.WriteText(eol).WriteText("max_blob_size_sync").WriteText(str).WriteText(iss)
                                                   .WriteText(NStr::UInt8ToString_DataSize( s_MaxBlobSizeSync)).WriteText(eos);
    task.WriteText(eol).WriteText("max_blob_size_sync").WriteText(is ).WriteNumber( s_MaxBlobSizeSync);
    task.WriteText(eol).WriteText("warn_blob_size_sync").WriteText(is ).WriteText( NStr::BoolToString(s_WarnBlobSizeSync));
    task.WriteText(eol).WriteText("blob_update_hotline").WriteText(is ).WriteText( NStr::BoolToString(s_BlobUpdateHotline));
}

size_t
CNCDistributionConf::CountServersForSlot(Uint2 slot)
{
    return s_MirrorConf->s_Slot2Servers[slot].size();
}

void
CNCDistributionConf::GetServersForSlot(Uint2 slot, TServersList& lst)
{
    TSrvGroupsList srvs = s_MirrorConf->s_Slot2Servers[slot];
    random_shuffle(srvs.begin(), srvs.end());
    lst.clear();
    for (size_t i = 0; i < srvs.size(); ++i) {
        if (srvs[i].grp == s_SelfGroup)
            lst.push_back(srvs[i].srv_id);
    }
    for (size_t i = 0; i < srvs.size(); ++i) {
        if (srvs[i].grp != s_SelfGroup)
            lst.push_back(srvs[i].srv_id);
    }
}

const TServersList&
CNCDistributionConf::GetRawServersForSlot(Uint2 slot)
{
    return s_MirrorConf->s_RawSlot2Servers[slot];
}

const vector<Uint2>&
CNCDistributionConf::GetCommonSlots(Uint8 server)
{
    return s_MirrorConf->s_CommonSlots[server];
}

bool
CNCDistributionConf::HasCommonSlots(Uint8 server)
{
    return !s_MirrorConf->s_CommonSlots[server].empty();
}

Uint8
CNCDistributionConf::GetSelfID(void)
{
    return s_SelfID;
}

const TNCPeerList&
CNCDistributionConf::GetPeers(void)
{
    return s_MirrorConf->s_Peers;
}

bool
CNCDistributionConf::HasPeers(void)
{
    return !s_MirrorConf->s_Peers.empty();
}

string
CNCDistributionConf::GetPeerNameOrEmpty(Uint8 srv_id)
{
    string name;
    if (srv_id == s_SelfID) {
        name = s_SelfName;
    }
    else {
        const TNCPeerList& peers = CNCDistributionConf::GetPeers();
        if (peers.find(srv_id) != peers.end()) {
            name = peers.find(srv_id)->second;
        } else {
            name = CNCPeerControl::GetPeerNameOrEmpty(srv_id);
        }
    }
    return name;
}

string
CNCDistributionConf::GetPeerName(Uint8 srv_id)
{
    string name(GetPeerNameOrEmpty(srv_id));
    if (name.empty()) {
        name = "unknown_server";
    }
    return name;
}

string
CNCDistributionConf::GetFullPeerName(Uint8 srv_id)
{
    string name( GetPeerName(srv_id));
    name += " (";
    name += NStr::UInt8ToString(srv_id);
    name += ") ";
    return name;
}


void
CNCDistributionConf::GetPeerServers(TServersList& lst)
{
    lst.clear();
    const TNCPeerList& peers = CNCDistributionConf::GetPeers();
    ITERATE(TNCPeerList, it_peer, peers)  {
        if (GetSelfID() != it_peer->first) {
            lst.push_back(it_peer->first);
        }
    }
}

void
CNCDistributionConf::GenerateBlobKey(Uint2 local_port,
                                     string& key, Uint2& slot, Uint2& time_bucket,
                                     unsigned int ver)
{
    s_KeyRndLock.Lock();
    Uint4 rnd_num = s_KeyRnd.GetRand();
    s_KeyRndLock.Unlock();

    Uint2 cnt_pieces = Uint2(s_SelfSlots.size());
    Uint4 piece_share = (CRandom::GetMax() + 1) / cnt_pieces + 1;
    Uint2 index = rnd_num / piece_share;
    rnd_num -= index * piece_share;
    slot = s_SelfSlots[index];
    Uint4 remain = rnd_num % s_SlotRndShare;
    Uint4 key_rnd = (slot - 1) * s_SlotRndShare + remain;
    time_bucket = Uint2((slot - 1) * s_CntSlotBuckets + remain / s_TimeRndShare) + 1;
    CNetCacheKey::GenerateBlobKey(&key,
                                  static_cast<Uint4>(s_BlobId.Add(1)),
                                  s_SelfHostIP, local_port, ver, key_rnd);
}

bool
CNCDistributionConf::GetSlotByKey(const string& key, Uint2& slot, Uint2& time_bucket)
{
    if (key[0] == '\1')
        // NetCache-generated key
        return GetSlotByNetCacheKey(key, slot, time_bucket);
    else {
        // ICache key provided by client
        GetSlotByICacheKey(key, slot, time_bucket);
        return true;
    }
}

bool
CNCDistributionConf::GetSlotByNetCacheKey(const string& key,
        Uint2& slot, Uint2& time_bucket)
{
    size_t ind = 0;
    unsigned key_rnd = 0;
#if 0
#define SKIP_UNDERSCORE(key, ind) \
    ind = key.find('_', ind + 1); \
    if (ind == string::npos) \
        return false;

    SKIP_UNDERSCORE(key, ind);      // version
    SKIP_UNDERSCORE(key, ind);      // id
    SKIP_UNDERSCORE(key, ind);      // host
    SKIP_UNDERSCORE(key, ind);      // port
    SKIP_UNDERSCORE(key, ind);      // time
    SKIP_UNDERSCORE(key, ind);      // random
#else
    ind = key.rfind('_');
    if (ind == string::npos) {
        goto on_error;
    }
#endif
    ++ind;
    key_rnd = NStr::StringToUInt(
            CTempString(&key[ind], key.size() - ind),
            NStr::fConvErr_NoThrow | NStr::fAllowTrailingSymbols);
    if (key_rnd == 0 && errno != 0) {
        goto on_error;
    }
    GetSlotByRnd(key_rnd, slot, time_bucket);
    return true;

on_error:
    CNetCacheKey tmp;
    if (!CNetCacheKey::ParseBlobKey(key.data(), key.size(), &tmp)) {
        SRV_LOG(Critical, "CNetCacheKey failed to parse blob key: " << key);
        return false;
    }
    SRV_LOG(Error, "NetCache failed to parse blob key: " << key);
    GetSlotByRnd(tmp.GetRandomPart(), slot, time_bucket);
    return true;
}

void
CNCDistributionConf::GetSlotByICacheKey(const string& key,
        Uint2& slot, Uint2& time_bucket)
{
    CChecksum crc32(CChecksum::eCRC32);

    crc32.AddChars(key.data(), key.size());

    GetSlotByRnd(crc32.GetChecksum(), slot, time_bucket);
}

void
CNCDistributionConf::GetSlotByRnd(Uint4 key_rnd,
        Uint2& slot, Uint2& time_bucket)
{
    // Slot numbers are 1-based
    slot = Uint2(key_rnd / s_SlotRndShare) + 1;
    time_bucket = Uint2((slot - 1) * s_CntSlotBuckets
                        + key_rnd % s_SlotRndShare / s_TimeRndShare) + 1;
}

Uint4
CNCDistributionConf::GetMainSrvIP(const CNCBlobKey& key)
{
    try {
        if (key.GetVersion() == 3) {
            Uint4 alias = key.GetHostPortCRC32();
            return (alias == s_SelfAlias) ? s_SelfIP :
                CNCPeerControl::FindIPbyAlias(alias);
        }
        const string& host_str(key.GetHost());
        if (s_SelfHostIP == host_str) {
            return s_SelfIP;
        }
        Uint4 ip = CNCPeerControl::FindIPbyName(host_str);
        if (ip != 0) {
            return ip;
        }
        return CTaskServer::GetIPByHost(host_str);
    }
    catch (...) {
        return 0;
    }
}

Uint4
CNCDistributionConf::CreateHostAlias(Uint4 ip, Uint4 port)
{
    string ip_str(CTaskServer::IPToString(ip));
    return CNetCacheKey::CalculateChecksum(ip_str, port);
}

bool
CNCDistributionConf::IsServedLocally(Uint2 slot)
{
    return find(s_SelfSlots.begin(), s_SelfSlots.end(), slot) != s_SelfSlots.end();
}

Uint2
CNCDistributionConf::GetCntSlotBuckets(void)
{
    return s_CntSlotBuckets;
}

Uint2
CNCDistributionConf::GetCntTimeBuckets(void)
{
    return s_CntTimeBuckets;
}

const vector<Uint2>&
CNCDistributionConf::GetSelfSlots(void)
{
    return s_SelfSlots;
}

const string&
CNCDistributionConf::GetMirroringSizeFile(void)
{
    return s_MirroringSizeFile;
}

const string&
CNCDistributionConf::GetPeriodicLogFile(void)
{
    return s_PeriodicLogFile;
}

Uint1
CNCDistributionConf::GetCntActiveSyncs(void)
{
    return s_CntActiveSyncs;
}

Uint1
CNCDistributionConf::GetMaxSyncsOneServer(void)
{
    return s_MaxSyncsOneServer;
}

Uint1
CNCDistributionConf::GetSyncPriority(void)
{
    return CNCServer::IsInitiallySynced()? s_SyncPriority: 1;
}

Uint2
CNCDistributionConf::GetMaxPeerTotalConns(void)
{
    return s_MaxPeerTotalConns;
}

Uint2
CNCDistributionConf::GetMaxPeerBGConns(void)
{
    return s_MaxPeerBGConns;
}

Uint2
CNCDistributionConf::GetCntErrorsToThrottle(void)
{
    return s_CntErrorsToThrottle;
}

Uint2
CNCDistributionConf::GetCntThrottlesToIpchange(void)
{
    return s_CntThrottlesToIpchange;
}

Uint8
CNCDistributionConf::GetPeerThrottlePeriod(void)
{
    return s_PeerThrottlePeriod;
}

Uint1
CNCDistributionConf::GetPeerTimeout(void)
{
    return s_PeerTimeout;
}

Uint1
CNCDistributionConf::GetBlobListTimeout(void)
{
    return s_BlobListTimeout;
}

Uint8
CNCDistributionConf::GetSmallBlobBoundary(void)
{
    return s_SmallBlobBoundary;
}

Uint2
CNCDistributionConf::GetMaxMirrorQueueSize(void)
{
    return s_MaxMirrorQueueSize;
}

const string&
CNCDistributionConf::GetSyncLogFileName(void)
{
    return s_SyncLogFileName;
}

Uint4
CNCDistributionConf::GetMaxSlotLogEvents(void)
{
    return s_MaxSlotLogEvents;
}

Uint4
CNCDistributionConf::GetCleanLogReserve(void)
{
    return s_CleanLogReserve;
}

Uint4
CNCDistributionConf::GetMaxCleanLogBatch(void)
{
    return s_MaxCleanLogBatch;
}

Uint8
CNCDistributionConf::GetMinForcedCleanPeriod(void)
{
    return s_MinForcedCleanPeriod;
}

Uint4
CNCDistributionConf::GetCleanAttemptInterval(void)
{
    return s_CleanAttemptInterval;
}

Uint8
CNCDistributionConf::GetPeriodicSyncInterval(void)
{
    return s_PeriodicSyncInterval;
}

Uint8
CNCDistributionConf::GetPeriodicSyncHeadTime(void)
{
    //return s_PeriodicSyncHeadTime;
    return 0;
}

Uint8
CNCDistributionConf::GetPeriodicSyncTailTime(void)
{
    //return s_PeriodicSyncTailTime;
    return 0;
}

Uint8
CNCDistributionConf::GetPeriodicSyncTimeout(void)
{
    return s_PeriodicSyncTimeout;
}

Uint8
CNCDistributionConf::GetFailedSyncRetryDelay(void)
{
    return s_FailedSyncRetryDelay;
}

Uint8
CNCDistributionConf::GetNetworkErrorTimeout(void)
{
    return s_NetworkErrorTimeout;
}
Uint8
CNCDistributionConf::GetMaxBlobSizeSync(void)
{
    return s_MaxBlobSizeSync;
}
bool
 CNCDistributionConf::GetWarnBlobSizeSync(void)
 {
    return s_WarnBlobSizeSync;
 }
bool
 CNCDistributionConf::GetBlobUpdateHotline(void)
 {
    return s_BlobUpdateHotline;
 }

void
CNCDistributionConf::PrintBlobCopyStat(Uint8 create_time, Uint8 create_server, Uint8 write_server)
{
    if (s_CopyDelayLog) {
        Uint8 cur_time = CSrvTime::Current().AsUSec();
        fprintf(s_CopyDelayLog,
                "%" NCBI_UINT8_FORMAT_SPEC ",%" NCBI_UINT8_FORMAT_SPEC
                ",%" NCBI_UINT8_FORMAT_SPEC ",%" NCBI_UINT8_FORMAT_SPEC "\n",
                cur_time, create_server, write_server, cur_time - create_time);
    }
}


END_NCBI_SCOPE
