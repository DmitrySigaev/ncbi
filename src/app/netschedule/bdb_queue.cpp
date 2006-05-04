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
 * Authors:  Anatoliy Kuznetsov
 *
 * File Description: Network scheduler job status database.
 *
 */
#include <ncbi_pch.hpp>

#include <corelib/ncbi_system.hpp>
#include <corelib/ncbifile.hpp>
#include <corelib/ncbi_limits.h>
#include <corelib/version.hpp>
#include <corelib/ncbireg.hpp>
#include <connect/services/netschedule_client.hpp>
#include <connect/ncbi_socket.hpp>

#include <db.h>
#include <bdb/bdb_trans.hpp>
#include <bdb/bdb_cursor.hpp>
#include <bdb/bdb_util.hpp>

#include "bdb_queue.hpp"


BEGIN_NCBI_SCOPE

const unsigned k_max_dead_locks = 100;  // max. dead lock repeats


/// Mutex to guard vector of busy IDs 
DEFINE_STATIC_FAST_MUTEX(x_NetSchedulerMutex_BusyID);


/// Class guards the id, guarantees exclusive access to the object
///
/// @internal
///
class CIdBusyGuard
{
public:
    CIdBusyGuard(bm::bvector<>* id_set, 
                 unsigned int   id,
                 unsigned       timeout)
        : m_IdSet(id_set), m_Id(id)
    {
        _ASSERT(id);
        unsigned cnt = 0; unsigned sleep_ms = 10;
        while (true) {
            {{
            CFastMutexGuard guard(x_NetSchedulerMutex_BusyID);
            if (!(*id_set)[id]) {
                id_set->set(id);
                break;
            }
            }}
            cnt += sleep_ms;
            if (cnt > timeout * 1000) {
                NCBI_THROW(CNetServiceException, 
                           eTimeout, "Failed to lock object");
            }
            SleepMilliSec(sleep_ms);
        } // while
    }

    ~CIdBusyGuard()
    {
        Release();
    }

    void Release()
    {
        if (m_Id) {
            CFastMutexGuard guard(x_NetSchedulerMutex_BusyID);
            m_IdSet->set(m_Id, false);
            m_Id = 0;
        }
    }
private:
    CIdBusyGuard(const CIdBusyGuard&);
    CIdBusyGuard& operator=(const CIdBusyGuard&);
private:
    bm::bvector<>*   m_IdSet;
    unsigned int     m_Id;
};




CQueueCollection::CQueueCollection()
{}

CQueueCollection::~CQueueCollection()
{
    NON_CONST_ITERATE(TQueueMap, it, m_QMap) {
        SLockedQueue* q = it->second;
        delete q;
    }
}

void CQueueCollection::Close()
{
    CWriteLockGuard guard(m_Lock);
    NON_CONST_ITERATE(TQueueMap, it, m_QMap) {
        SLockedQueue* q = it->second;
        if (q->lb_coordinator) {
            q->lb_coordinator->StopCollectorThread();
        }
        delete q;
    }
    m_QMap.clear();

}

SLockedQueue& CQueueCollection::GetLockedQueue(const string& name)
{
    CReadLockGuard guard(m_Lock);
    TQueueMap::iterator it = m_QMap.find(name);
    if (it == m_QMap.end()) {
        string msg = "Job queue not found: ";
        msg += name;
        NCBI_THROW(CNetScheduleException, eUnknownQueue, msg);
    }
    return *(it->second);
}


bool CQueueCollection::QueueExists(const string& name) const
{
    CReadLockGuard guard(m_Lock);
    TQueueMap::const_iterator it = m_QMap.find(name);
    return (it != m_QMap.end());
}

void CQueueCollection::AddQueue(const string& name, SLockedQueue* queue)
{
    CWriteLockGuard guard(m_Lock);
    m_QMap[name] = queue;
}






CQueueDataBase::CQueueDataBase()
: m_Env(0),
  m_StopPurge(false),
  m_PurgeLastId(0),
  m_PurgeSkipCnt(0),
  m_DeleteChkPointCnt(0),
  m_FreeStatusMemCnt(0),
  m_LastFreeMem(time(0)),
  m_LastR2P(time(0)),
  m_UdpPort(0)
{
    m_IdCounter.Set(0);
}

CQueueDataBase::~CQueueDataBase()
{
    try {
        Close();
        delete m_Env; // paranoiya check
    } catch (exception& )
    {}
}

void CQueueDataBase::Open(const string& path, 
                          unsigned      cache_ram_size,
                          unsigned      max_locks,
                          unsigned      log_mem_size,
                          unsigned      max_trans)
{
    m_Path = CDirEntry::AddTrailingPathSeparator(path);

    {{
        CDir dir(m_Path);
        if ( !dir.Exists() ) {
            dir.Create();
        }
    }}

    delete m_Env;
    m_Env = new CBDB_Env();


    // memory log option. we probably need to reset LSN
    // numbers
/*
    if (log_mem_size) {
    }

    // Private environment for LSN recovery
    {{
        m_Name = "jsqueue";
        string err_file = m_Path + "err" + string(m_Name) + ".log";
        m_Env->OpenErrFile(err_file.c_str());

        m_Env->SetCacheSize(1024*1024);
        m_Env->OpenPrivate(path.c_str());
        
        m_Env->LsnReset("jsq_test.db");

        delete m_Env;
        m_Env = new CBDB_Env();
    }}
*/



    m_Name = "jsqueue";
    string err_file = m_Path + "err" + string(m_Name) + ".log";
    m_Env->OpenErrFile(err_file.c_str());

    if (log_mem_size) {
        m_Env->SetLogInMemory(true);
        m_Env->SetLogBSize(log_mem_size);
    } else {
        m_Env->SetLogFileMax(200 * 1024 * 1024);
        m_Env->SetLogAutoRemove(true);
    }

    

    // Check if bdb env. files are in place and try to join
    CDir dir(m_Path);
    CDir::TEntries fl = dir.GetEntries("__db.*", CDir::eIgnoreRecursive);

    if (fl.empty()) {
        if (cache_ram_size) {
            m_Env->SetCacheSize(cache_ram_size);
        }
        //unsigned max_locks = m_Env->GetMaxLocks();
        if (max_locks) {
            m_Env->SetMaxLocks(max_locks);
            m_Env->SetMaxLockObjects(max_locks);
        }
        if (max_trans) {
            m_Env->SetTransactionMax(max_trans);
        }

        m_Env->OpenWithTrans(path.c_str(), CBDB_Env::eThreaded);
    } else {
        if (cache_ram_size) {
            m_Env->SetCacheSize(cache_ram_size);
        }
        try {
            m_Env->JoinEnv(path.c_str(), CBDB_Env::eThreaded);
            if (!m_Env->IsTransactional()) {
                LOG_POST(Info << 
                         "JS: '" << 
                         "' Warning: Joined non-transactional environment ");
            }
        } 
        catch (CBDB_ErrnoException& err_ex) 
        {
            if (err_ex.BDB_GetErrno() == DB_RUNRECOVERY) {
                LOG_POST(Warning << 
                         "JS: '" << 
                         "'Warning: DB_ENV returned DB_RUNRECOVERY code."
                         " Running the recovery procedure.");
            }
            m_Env->OpenWithTrans(path.c_str(), 
                                CBDB_Env::eThreaded | CBDB_Env::eRunRecovery);

        }
        catch (CBDB_Exception&)
        {
            m_Env->OpenWithTrans(path.c_str(), 
                                 CBDB_Env::eThreaded | CBDB_Env::eRunRecovery);
        }

    } // if else
    m_Env->SetDirectDB(true);
    m_Env->SetDirectLog(true);

    m_Env->SetLockTimeout(10 * 1000000); // 10 sec

    m_Env->SetTasSpins(5);

    if (m_Env->IsTransactional()) {
        m_Env->SetTransactionTimeout(10 * 1000000); // 10 sec
        m_Env->TransactionCheckpoint();
    }
}

static
CNSLB_ThreasholdCurve* s_ConfigureCurve(const IRegistry& reg, 
                                        const string&    sname,
                                        unsigned         exec_delay)
{
    auto_ptr<CNSLB_ThreasholdCurve> curve;
    string lb_curve = 
        reg.GetString(sname, "lb_curve", kEmptyStr);

    do {

    if (lb_curve.empty() || 
        NStr::CompareNocase(lb_curve, "linear") == 0) {
        double y0 = reg.GetDouble(sname, 
                                  "lb_curve_high", 
                                  0.6, 
                                  0, 
                                  IRegistry::eReturn);
        double yN = reg.GetDouble(sname, 
                                  "lb_curve_linear_low", 
                                  0.15, 
                                  0, 
                                  IRegistry::eReturn);
        curve.reset(new CNSLB_ThreasholdCurve_Linear(y0, yN));
        LOG_POST(Info << sname 
                      << " initializing linear LB curve"
                      << " y0=" << y0 
                      << " yN=" << yN);
        break;
    }

    if (NStr::CompareNocase(lb_curve, "regression") == 0) {
        double y0 = reg.GetDouble(sname, 
                                  "lb_curve_high",
                                  0.85, 
                                  0, 
                                  IRegistry::eReturn);
        double a = reg.GetDouble(sname, 
                                  "lb_curve_regression_a",
                                  -0.2, 
                                  0, 
                                  IRegistry::eReturn);
        curve.reset(new CNSLB_ThreasholdCurve_Regression(y0, a));
        LOG_POST(Info << sname 
                      << " initializing regression LB curve."
                      << " y0=" << y0 
                      << " a="  << a);
        break;
    }

    } while(0);

    if (curve.get()) {
        curve->ReGenerateCurve(exec_delay);
    }
    return curve.release();
}

static
CNSLB_DecisionModule* s_ConfigureDecision(const IRegistry& reg, 
                                          const string&    sname)
{
    auto_ptr<CNSLB_DecisionModule> decision;
    string lb_policy = 
        reg.GetString(sname, "lb_policy", kEmptyStr);

    do {
    if (lb_policy.empty() ||
        NStr::CompareNocase(lb_policy, "rate")==0) {
        decision.reset(new CNSLB_DecisionModule_DistributeRate());
        break;
    }
    if (NStr::CompareNocase(lb_policy, "cpu_avail")==0) {
        decision.reset(new CNSLB_DecisionModule_CPU_Avail());
        break;
    }

    } while(0);

    return decision.release();
}

void CQueueDataBase::ReadConfig(const IRegistry& reg, unsigned* min_run_timeout)
{
    string qname;
    list<string> sections;
    reg.EnumerateSections(&sections);


    string tmp;
    ITERATE(list<string>, it, sections) {
        const string& sname = *it;
        NStr::SplitInTwo(sname, "_", tmp, qname);
        if (NStr::CompareNocase(tmp, "queue") != 0) {
            continue;
        }

        int timeout = 
            reg.GetInt(sname, "timeout", 3600, 0, IRegistry::eReturn);
        int notif_timeout =
            reg.GetInt(sname, "notif_timeout", 7, 0, IRegistry::eReturn);
        int run_timeout =
            reg.GetInt(sname, "run_timeout", 
                                        timeout, 0, IRegistry::eReturn);

        int run_timeout_precision =
            reg.GetInt(sname, "run_timeout_precision", 
                                        run_timeout, 0, IRegistry::eReturn);
        *min_run_timeout = 
            std::min(*min_run_timeout, (unsigned)run_timeout_precision);

        string program_name = reg.GetString(sname, "program", kEmptyStr);

        bool delete_when_done = reg.GetBool(sname, "delete_done", 
                                            false, 0, IRegistry::eReturn);

        bool qexists = m_QueueCollection.QueueExists(qname);

        if (!qexists) {
            LOG_POST(Info 
                << "Mounting queue:           " << qname                 << "\n"
                << "   Timeout:               " << timeout               << "\n"
                << "   Notification timeout:  " << notif_timeout         << "\n"
                << "   Run timeout:           " << run_timeout           << "\n"
                << "   Run timeout precision: " << run_timeout_precision << "\n"
                << "   Programs:              " << program_name          << "\n"
                << "   Delete done:           " << delete_when_done      << "\n"
            );
            MountQueue(qname, timeout, 
                        notif_timeout, 
                        run_timeout, 
                        run_timeout_precision,
                        program_name,
                        delete_when_done);
        } else { // update non-critical queue parameters
            SLockedQueue& queue = m_QueueCollection.GetLockedQueue(qname);
            queue.program_version_list.Clear();
            if (!program_name.empty()) {
                queue.program_version_list.AddClientInfo(program_name);
            }
        }

        SLockedQueue& queue = m_QueueCollection.GetLockedQueue(qname);
        string subm_host = reg.GetString(sname,  "subm_host",  kEmptyStr);
        queue.subm_hosts.SetHosts(subm_host);

        string wnode_host = reg.GetString(sname, "wnode_host", kEmptyStr);
        queue.wnode_hosts.SetHosts(wnode_host);

        {{
        bool dump_db = reg.GetBool(sname, "dump_db", false, 0, IRegistry::eReturn);
        CFastMutexGuard guard(queue.rec_dump_lock);
        queue.rec_dump_flag = dump_db;
        }}


        // re-load load balancing settings
        {{
        bool lb_flag = reg.GetBool(sname, "lb", false, 0, IRegistry::eReturn);
        SLockedQueue& queue = m_QueueCollection.GetLockedQueue(qname);

        if (lb_flag != queue.lb_flag) {

        string lb_service = reg.GetString(sname, "lb_service", kEmptyStr);
        int lb_collect_time =
            reg.GetInt(sname, "lb_collect_time", 5, 0, IRegistry::eReturn);

        if (lb_service.empty()) {
            if (lb_flag) {
                LOG_POST(Error << "Queue:" << sname 
                    << "cannot be load balanced. Missing lb_service ini setting");
            }
            queue.lb_flag = false;
        } else {
            string lb_unknown_host = 
                reg.GetString(sname, "lb_unknown_host", kEmptyStr);

            ENSLB_RunDelayType lb_delay_type = eNSLB_Constant;
            unsigned lb_stall_time = 6;
            string lb_exec_delay_str = 
                        reg.GetString(sname, "lb_exec_delay", kEmptyStr);
            if (NStr::CompareNocase(lb_exec_delay_str, "run_time")) {
                lb_delay_type = eNSLB_RunTimeAvg;
            } else {
                try {
                    int stall_time = NStr::StringToInt(lb_exec_delay_str);
                    if (stall_time > 0) {
                        lb_stall_time = stall_time;
                    }
                } 
                catch(exception& ex)
                {
                    ERR_POST("Invalid value of lb_exec_delay " 
                             << ex.what()
                             << " Offending value:"
                             << lb_exec_delay_str
                             );
                }
            }


            CNSLB_Coordinator::ENonLbHostsPolicy non_lb_hosts = 
                CNSLB_Coordinator::eNonLB_Allow;
            if (NStr::CompareNocase(lb_unknown_host, "deny") == 0) {
                non_lb_hosts = CNSLB_Coordinator::eNonLB_Deny;
            } else
            if (NStr::CompareNocase(lb_unknown_host, "allow") == 0) {
                non_lb_hosts = CNSLB_Coordinator::eNonLB_Allow;
            } else
            if (NStr::CompareNocase(lb_unknown_host, "reserve") == 0) {
                non_lb_hosts = CNSLB_Coordinator::eNonLB_Reserve;
            }



            double lb_stall_time_mult = 
                  reg.GetDouble(sname, 
                                "lb_exec_delay_mult",
                                0.5, 
                                0, 
                                IRegistry::eReturn);


            if (queue.lb_flag == false) { // LB is OFF
                LOG_POST(Error << "Queue:" << sname 
                               << " is load balanced. " << lb_service);

                if (queue.lb_coordinator == 0) {
                    auto_ptr<CNSLB_ThreasholdCurve>  
                      deny_curve(s_ConfigureCurve(reg, sname, lb_stall_time));

                    //CNSLB_DecisionModule*   decision_maker = 0;

                    auto_ptr<CNSLB_DecisionModule_DistributeRate> 
                        decision_distr_rate(
                            new CNSLB_DecisionModule_DistributeRate);

                    auto_ptr<INSLB_Collector>   
                        collect(new CNSLB_LBSMD_Collector());

                    auto_ptr<CNSLB_Coordinator> 
                       coord(new CNSLB_Coordinator(
                                    lb_service,
                                    collect.release(),
                                    deny_curve.release(),
                                    decision_distr_rate.release(),
                                    lb_collect_time,
                                    non_lb_hosts));

                    queue.lb_coordinator = coord.release();
                    queue.lb_stall_delay_type = lb_delay_type;
                    queue.lb_stall_time = lb_stall_time;
                    queue.lb_stall_time_mult = lb_stall_time_mult;
                    queue.lb_flag = true;

                } else {  // LB is ON
                    // reconfigure the LB delay
                    CFastMutexGuard guard(queue.lb_stall_time_lock);
                    queue.lb_stall_delay_type = lb_delay_type;
                    if (lb_delay_type == eNSLB_Constant) {
                        queue.lb_stall_time = lb_stall_time;
                    }
                    queue.lb_stall_time_mult = lb_stall_time_mult;
                }
            }
        }

        } // if lb_flag != queue.lb_flag

        }}

    } // ITERATE

}


void CQueueDataBase::MountQueue(const string& queue_name, 
                                int           timeout,
                                int           notif_timeout,
                                int           run_timeout,
                                int           run_timeout_precision,
                                const string& program_name,
                                bool          delete_done)
{
    _ASSERT(m_Env);

    if (m_QueueCollection.QueueExists(queue_name)) {
        LOG_POST(Warning << 
                 "JS: Queue " << queue_name << " already exists.");
        return;
    }

    auto_ptr<SLockedQueue> q(new SLockedQueue(queue_name));
    string fname = string("jsq_") + queue_name + string(".db");
    q->db.SetEnv(*m_Env);

    q->db.RevSplitOff();
    q->db.Open(fname.c_str(), CBDB_RawFile::eReadWriteCreate);

    fname = string("jsq_") + queue_name + string("_affid.idx");
    q->aff_idx.SetEnv(*m_Env);
    q->aff_idx.Open(fname.c_str(), CBDB_RawFile::eReadWriteCreate);

    q->timeout = timeout;
    q->notif_timeout = notif_timeout;
    q->delete_done = delete_done;
    q->last_notif = time(0);

    q->affinity_dict.Open(*m_Env, queue_name);

    m_QueueCollection.AddQueue(queue_name, q.release());

    SLockedQueue& queue = m_QueueCollection.GetLockedQueue(queue_name);

    queue.run_timeout = run_timeout;
    if (run_timeout) {
        queue.run_time_line = new CJobTimeLine(run_timeout_precision, 0);
    }


    // scan the queue, restore the state machine

    CBDB_FileCursor cur(queue.db);
    cur.SetCondition(CBDB_FileCursor::eGE);
    cur.From << 0;

    unsigned recs = 0;

    for (;cur.Fetch() == eBDB_Ok; ++recs) {
        unsigned job_id = queue.db.id;
        int status = queue.db.status;


        if (job_id > (unsigned)m_IdCounter.Get()) {
            m_IdCounter.Set(job_id);
        }
        queue.status_tracker.SetExactStatusNoLock(job_id, 
                      (CNetScheduleClient::EJobStatus) status, 
                      true);

        if (status == (int) CNetScheduleClient::eRunning && 
            queue.run_time_line) {
            // Add object to the first available slot
            // it is going to be rescheduled or dropped
            // in the background control thread
            queue.run_time_line->AddObjectToSlot(0, job_id);
        }
    } // while


    queue.udp_socket.SetReuseAddress(eOn);
    unsigned short udp_port = GetUdpPort();
    if (udp_port) {
        queue.udp_socket.Bind(udp_port);
    }

    // program version control
    if (!program_name.empty()) {
        queue.program_version_list.AddClientInfo(program_name);
    }


    LOG_POST(Info << "Queue records = " << recs);
    
}

void CQueueDataBase::Close()
{
    StopNotifThread();
    StopPurgeThread();
    StopExecutionWatcherThread();

    if (m_Env) {
        m_Env->TransactionCheckpoint();
    }

    m_QueueCollection.Close();
    try {
        if (m_Env) {
            if (m_Env->CheckRemove()) {
                LOG_POST(Info    <<
                         "JS: '" <<
                         m_Name  << "' Unmounted. BDB ENV deleted.");
            } else {
                LOG_POST(Warning << "JS: '" << m_Name 
                                 << "' environment still in use.");
            }
        }
    }
    catch (exception& ex) {
        LOG_POST(Warning << "JS: '" << m_Name 
                         << "' Exception in Close() " << ex.what()
                         << " (ignored.)");
    }

    delete m_Env; m_Env = 0;
}

unsigned int CQueueDataBase::GetNextId()
{
    unsigned int id;

    if (m_IdCounter.Get() >= kMax_I4) {
        m_IdCounter.Set(0);
    }
    id = (unsigned) m_IdCounter.Add(1); 

    if ((id % 1000) == 0) {
        m_Env->TransactionCheckpoint();
    }
    if ((id % 1000000) == 0) {
        m_Env->CleanLog();
    }

    return id;
}

unsigned int CQueueDataBase::GetNextIdBatch(unsigned count)
{
    if (m_IdCounter.Get() >= kMax_I4) {
        m_IdCounter.Set(0);
    }
    unsigned int id = (unsigned) m_IdCounter.Add(count);
    id = id - count + 1;
    return id;
}

void CQueueDataBase::TransactionCheckPoint()
{
    m_Env->TransactionCheckpoint();
    m_Env->CleanLog();
}

void CQueueDataBase::NotifyListeners()
{
    const CQueueCollection::TQueueMap& qm = m_QueueCollection.GetMap();
    ITERATE(CQueueCollection::TQueueMap, it, qm) {
        const string qname = it->first;
        CQueue jq(*this, qname, 0);

        jq.NotifyListeners();
    }
}

void CQueueDataBase::CheckExecutionTimeout()
{
    const CQueueCollection::TQueueMap& qm = m_QueueCollection.GetMap();
    ITERATE(CQueueCollection::TQueueMap, it, qm) {
        const string qname = it->first;
        CQueue jq(*this, qname, 0);

        jq.CheckExecutionTimeout();
    }    
}

void CQueueDataBase::Purge()
{
    unsigned curr_id = m_IdCounter.Get();

    // Re-submit returned jobs 
    {{

    const int kR2P_delay = 7; // re-submission delay
    time_t curr = time(0);

    if (m_LastR2P + kR2P_delay <= curr) {
        m_LastR2P = curr;
        const CQueueCollection::TQueueMap& qm = m_QueueCollection.GetMap();
        ITERATE(CQueueCollection::TQueueMap, it, qm) {
            const string qname = it->first;
            CQueue jq(*this, qname, 0);

            jq.Return2Pending();
        }
    }
    }}


     // check not to rescan the database too often 
     // when we have low client activity

    if (m_PurgeLastId + 3000 > curr_id) {
        ++m_PurgeSkipCnt;
        
        // probably nothing to do yet, skip purge execution

        if (m_PurgeSkipCnt < 10) { // only 10 skips in a row
            return;
        }

        m_PurgeSkipCnt = 0;        
    }


    // Delete obsolete job records, based on time stamps 
    // and expiration timeouts

    unsigned global_del_rec = 0;

    const CQueueCollection::TQueueMap& qm = m_QueueCollection.GetMap();
    ITERATE(CQueueCollection::TQueueMap, it, qm) {
        const string qname = it->first;
        CQueue jq(*this, qname, 0);

        unsigned del_rec, total_del_rec = 0;
        const unsigned batch_size = 100;
        do {
            // ----------------------------------------------
            // Clean failed jobs

            del_rec = jq.CheckDeleteBatch(batch_size, 
                                          CNetScheduleClient::eFailed);
            total_del_rec += del_rec;
            global_del_rec += del_rec;
            m_PurgeLastId += del_rec;

            // ----------------------------------------------
            // Clean canceled jobs

            del_rec = jq.CheckDeleteBatch(batch_size, 
                                          CNetScheduleClient::eCanceled);
            total_del_rec += del_rec;
            global_del_rec += del_rec;
            m_PurgeLastId += del_rec;

            // ----------------------------------------------
            // Clean finished jobs

            del_rec = jq.CheckDeleteBatch(batch_size, 
                                          CNetScheduleClient::eDone);
            total_del_rec += del_rec;
            global_del_rec += del_rec;
            m_PurgeLastId += del_rec;

            // ----------------------------------------------
            // Clean pending jobs

            del_rec = jq.CheckDeleteBatch(batch_size, 
                                          CNetScheduleClient::ePending);
            total_del_rec += del_rec;
            global_del_rec += del_rec;
            m_PurgeLastId += del_rec;

            // do not delete more than certain number of 
            // records from the queue in one Purge
            if (total_del_rec >= 1000) {
                SleepMilliSec(200);
                break;
            } else {
                SleepMilliSec(2000);
            }
            

            {{
                CFastMutexGuard guard(m_PurgeLock);
                if (m_StopPurge) {
                    m_StopPurge = false;
                    return;
                }
            }}

        } while (del_rec == batch_size);

        m_DeleteChkPointCnt += total_del_rec;
        if (m_DeleteChkPointCnt > 1000) {
            m_DeleteChkPointCnt = 0;

            // remove unused affinity elements
            ITERATE(CQueueCollection::TQueueMap, it, qm) {
                const string qname = it->first;
                CQueue jq(*this, qname, 0);
                jq.ClearAffinityIdx();
            }

            m_Env->TransactionCheckpoint();
        }

    } // ITERATE


    // optimize status matrix to free some memory
    //
    time_t curr = time(0);
    m_FreeStatusMemCnt += global_del_rec;
    const int kMemFree_Delay = 15 * 60; 
    // optimize memory every 15 min. or after 1mil of deleted records
    if ((m_FreeStatusMemCnt > 1000000) ||
        (m_LastFreeMem + kMemFree_Delay <= curr)) {
        m_FreeStatusMemCnt = 0;
        m_LastFreeMem = curr;

        const CQueueCollection::TQueueMap& qm = m_QueueCollection.GetMap();
        ITERATE(CQueueCollection::TQueueMap, it, qm) {
            const string qname = it->first;
            CQueue jq(*this, qname, 0);
            jq.FreeUnusedMem();
            {{
                CFastMutexGuard guard(m_PurgeLock);
                if (m_StopPurge) {
                    m_StopPurge = false;
                    return;
                }
            }}
        } // ITERATE
    }

}

void CQueueDataBase::StopPurge()
{
    CFastMutexGuard guard(m_PurgeLock);
    m_StopPurge = true;
}

void CQueueDataBase::RunPurgeThread()
{
# ifdef NCBI_THREADS
       LOG_POST(Info << "Starting guard and cleaning thread.");
       m_PurgeThread.Reset(
           new CJobQueueCleanerThread(*this, 2, 5));
       m_PurgeThread->Run();
# else
        LOG_POST(Warning << 
                 "Cannot run background thread in non-MT configuration.");
# endif

}


void CQueueDataBase::StopPurgeThread()
{
# ifdef NCBI_THREADS
    if (!m_PurgeThread.Empty()) {
        LOG_POST(Info << "Stopping guard and cleaning thread...");
        StopPurge();
        m_PurgeThread->RequestStop();
        m_PurgeThread->Join();
        LOG_POST(Info << "Stopped.");
    }
# endif
}

void CQueueDataBase::RunNotifThread()
{
    if (GetUdpPort() == 0) {
        return;
    }

# ifdef NCBI_THREADS
       LOG_POST(Info << "Starting client notification thread.");
       m_NotifThread.Reset(
           new CJobNotificationThread(*this, 5, 2));
       m_NotifThread->Run();
# else
        LOG_POST(Warning << 
                 "Cannot run background thread in non-MT configuration.");
# endif

}

void CQueueDataBase::StopNotifThread()
{
# ifdef NCBI_THREADS
    if (!m_NotifThread.Empty()) {
        LOG_POST(Info << "Stopping notification thread...");
        m_NotifThread->RequestStop();
        m_NotifThread->Join();
        LOG_POST(Info << "Stopped.");
    }
# endif
}

void CQueueDataBase::RunExecutionWatcherThread(unsigned run_delay)
{
# ifdef NCBI_THREADS
       LOG_POST(Info << "Starting execution watcher thread.");
       m_ExeWatchThread.Reset(
           new CJobQueueExecutionWatcherThread(*this, run_delay, 2));
       m_ExeWatchThread->Run();
# else
       LOG_POST(Warning << 
                "Cannot run background thread in non-MT configuration.");
# endif

}

void CQueueDataBase::StopExecutionWatcherThread()
{
# ifdef NCBI_THREADS
    if (!m_ExeWatchThread.Empty()) {
        LOG_POST(Info << "Stopping execution watch thread...");
        m_ExeWatchThread->RequestStop();
        m_ExeWatchThread->Join();
        LOG_POST(Info << "Stopped.");
    }
# endif
}






CQueueDataBase::CQueue::CQueue(CQueueDataBase& db, 
                               const string&   queue_name,
                               unsigned        client_host_addr)
: m_Db(db),
  m_LQueue(db.m_QueueCollection.GetLockedQueue(queue_name)),
  m_ClientHostAddr(client_host_addr),
  m_QueueDbAccessCounter(0)
{
}

unsigned CQueueDataBase::CQueue::CountRecs()
{
    SQueueDB& db = m_LQueue.db;
    CFastMutexGuard guard(m_LQueue.lock);
	db.SetTransaction(0);
    return db.CountRecs();
}


#define NS_PRINT_TIME(msg, t) \
    do \
    { unsigned tt = t; \
      CTime _t(tt); _t.ToLocalTime(); \
      out << msg << (tt ? _t.AsString() : kEmptyStr) << fsp; \
    } while(0)

#define NS_PFNAME(x_fname) \
    (fflag ? (const char*)x_fname : (const char*)"")

void CQueueDataBase::CQueue::x_PrintJobDbStat(SQueueDB&      db, 
                                              CNcbiOstream&  out,
                                              const char*    fsp,
                                              bool           fflag)
{
    out << fsp << NS_PFNAME("id: ") << (unsigned) db.id << fsp;
    CNetScheduleClient::EJobStatus status = 
        (CNetScheduleClient::EJobStatus)(int)db.status;
    out << NS_PFNAME("status: ") << CNetScheduleClient::StatusToString(status) 
        << fsp;

    NS_PRINT_TIME(NS_PFNAME("time_submit: "), db.time_submit);
    NS_PRINT_TIME(NS_PFNAME("time_run: "), db.time_run);
    NS_PRINT_TIME(NS_PFNAME("time_done: "), db.time_done);

    out << NS_PFNAME("timeout: ") << (unsigned)db.timeout << fsp;
    out << NS_PFNAME("run_timeout: ") << (unsigned)db.run_timeout << fsp;

    unsigned subm_addr = db.subm_addr;
    out << NS_PFNAME("subm_addr: ") 
        << (subm_addr ? CSocketAPI::gethostbyaddr(subm_addr) : kEmptyStr) << fsp;
    out << NS_PFNAME("subm_port: ") << (unsigned) db.subm_port << fsp;
    out << NS_PFNAME("subm_timeout: ") << (unsigned) db.subm_timeout << fsp;

    unsigned addr = db.worker_node1;
    out << NS_PFNAME("worker_node1: ")
        << (addr ? CSocketAPI::gethostbyaddr(addr) : kEmptyStr) << fsp;

    addr = db.worker_node2;
    out << NS_PFNAME("worker_node2: ") 
        << (addr ? CSocketAPI::gethostbyaddr(addr) : kEmptyStr) << fsp;

    addr = db.worker_node3;
    out << NS_PFNAME("worker_node3: ") 
        << (addr ? CSocketAPI::gethostbyaddr(addr) : kEmptyStr) << fsp;

    addr = db.worker_node4;
    out << NS_PFNAME("worker_node4: ") 
        << (addr ? CSocketAPI::gethostbyaddr(addr) : kEmptyStr) << fsp;

    addr = db.worker_node5;
    out << NS_PFNAME("worker_node5: ") 
        << (addr ? CSocketAPI::gethostbyaddr(addr) : kEmptyStr) << fsp;

    out << NS_PFNAME("run_counter: ") << (unsigned) db.run_counter << fsp;
    out << NS_PFNAME("ret_code: ") << (unsigned) db.ret_code << fsp;

    out << NS_PFNAME("input: ")        << "'" << (string) db.input       << "'" << fsp;
    out << NS_PFNAME("output: ")       << "'" <<(string) db.output       << "'" << fsp;
    out << NS_PFNAME("err_msg: ")      << "'" <<(string) db.err_msg      << "'" << fsp;
    out << NS_PFNAME("progress_msg: ") << "'" <<(string) db.progress_msg << "'" << fsp;
    out << "\n";
}

void 
CQueueDataBase::CQueue::x_PrintShortJobDbStat(SQueueDB&     db, 
                                              const string& host,
                                              unsigned      port,
                                              CNcbiOstream& out,
                                              const char*   fsp)
{
    char buf[1024];
    sprintf(buf, NETSCHEDULE_JOBMASK, 
                 (unsigned)db.id, host.c_str(), port);
    out << buf << fsp;
    CNetScheduleClient::EJobStatus status = 
        (CNetScheduleClient::EJobStatus)(int)db.status;
    out << CNetScheduleClient::StatusToString(status) << fsp;

    out << "'" << (string) db.input    << "'" << fsp;
    out << "'" << (string) db.output   << "'" << fsp;
    out << "'" << (string) db.err_msg  << "'" << fsp;

    out << "\n";
}


void 
CQueueDataBase::CQueue::PrintJobDbStat(unsigned job_id, CNcbiOstream & out)
{
    SQueueDB& db = m_LQueue.db;
    CFastMutexGuard guard(m_LQueue.lock);
    db.SetTransaction(0);
    db.id = job_id;
    if (db.Fetch() == eBDB_Ok) {
        x_PrintJobDbStat(db, out);
    } else {
        out << "Job not found id=" << job_id;
    }
    out << "\n";
}

void CQueueDataBase::CQueue::PrintJobStatusMatrix(CNcbiOstream & out)
{
    m_LQueue.status_tracker.PrintStatusMatrix(out);
}


void CQueueDataBase::CQueue::PrintAllJobDbStat(CNcbiOstream & out)
{
    SQueueDB& db = m_LQueue.db;
    CFastMutexGuard guard(m_LQueue.lock);

    CBDB_Transaction trans(*db.GetEnv(), 
                           CBDB_Transaction::eTransASync,
                           CBDB_Transaction::eNoAssociation);

    db.SetTransaction(&trans);

    CBDB_FileCursor& cur = *GetCursor(trans);
    CBDB_CursorGuard cg(cur);    

    cur.SetCondition(CBDB_FileCursor::eGE);
    cur.From << 0;

    while (cur.Fetch() == eBDB_Ok) {
        x_PrintJobDbStat(db, out);
        if (!out.good()) break;
    }
}


void CQueueDataBase::CQueue::PrintStat(CNcbiOstream & out)
{
    SQueueDB& db = m_LQueue.db;
    CFastMutexGuard guard(m_LQueue.lock);
	db.SetTransaction(0);
    db.PrintStat(out);
}


void CQueueDataBase::CQueue::PrintQueue(CNcbiOstream & out, 
                            CNetScheduleClient::EJobStatus job_status,
                            const string& host,
                            unsigned      port)
{
    CNetScheduler_JobStatusTracker::TBVector bv;
    m_LQueue.status_tracker.StatusSnapshot(job_status, &bv);
    SQueueDB& db = m_LQueue.db;

    CNetScheduler_JobStatusTracker::TBVector::enumerator en(bv.first());
    for (;en.valid(); ++en) {
        unsigned id = *en;
        {{
        CFastMutexGuard guard(m_LQueue.lock);
	    db.SetTransaction(0);
        
        db.id = id;

        if (db.Fetch() == eBDB_Ok) {
            x_PrintShortJobDbStat(db, host, port, out, "\t");
        }

        }}
        
    } // for
}



void CQueueDataBase::CQueue::PrintNodeStat(CNcbiOstream & out) const
{
    unsigned       host;
    unsigned short port;
    time_t         last_connect;
    string         auth;


    SLockedQueue::TListenerList&  wnodes = m_LQueue.wnodes;
    SLockedQueue::TListenerList::size_type lst_size;
    {{
        CReadLockGuard guard(m_LQueue.wn_lock);
        lst_size = wnodes.size();
    }}

    time_t curr = time(0);

    for (SLockedQueue::TListenerList::size_type i = 0; i < lst_size; ++i) {
        {{
        CReadLockGuard guard(m_LQueue.wn_lock);  
        const SQueueListener* ql = wnodes[i];
        host = ql->host;
        port = ql->udp_port;
        last_connect = ql->last_connect;

        // cut off one day old obsolete connections
        if ( (last_connect + (24 * 3600)) < curr) {
            continue;
        }

        auth = ql->auth;
        }}

        CTime lc_time(last_connect);
        lc_time.ToLocalTime();
        out << auth << " @ " << CSocketAPI::gethostbyaddr(host) 
            << "  UDP:" << port << "  " << lc_time.AsString() << "\n";
    }
}

void CQueueDataBase::CQueue::PrintSubmHosts(CNcbiOstream & out) const
{
    m_LQueue.subm_hosts.PrintHosts(out);
}

void CQueueDataBase::CQueue::PrintWNodeHosts(CNcbiOstream & out) const
{
    m_LQueue.wnode_hosts.PrintHosts(out);
}


unsigned int 
CQueueDataBase::CQueue::Submit(const char*   input,
                               unsigned      host_addr,
                               unsigned      port,
                               unsigned      wait_timeout,
                               const char*   progress_msg,
                               const char*   affinity_token)
{
    unsigned int job_id = m_Db.GetNextId();

    CNetSchedule_JS_Guard js_guard(m_LQueue.status_tracker, 
                                   job_id,
                                   CNetScheduleClient::ePending);
    SQueueDB& db = m_LQueue.db;
    CBDB_Transaction trans(*db.GetEnv(), 
                           CBDB_Transaction::eTransASync,
                           CBDB_Transaction::eNoAssociation);

    unsigned aff_id = 0;
    if (affinity_token && *affinity_token) {
        aff_id = m_LQueue.affinity_dict.CheckToken(affinity_token, trans);
    }

    {{
    CFastMutexGuard guard(m_LQueue.lock);
	db.SetTransaction(&trans);

    x_AssignSubmitRec(
        job_id, input, host_addr, port, wait_timeout, progress_msg, aff_id);

    db.Insert();

    // update affinity index
    if (aff_id) {
        m_LQueue.aff_idx.SetTransaction(&trans);
        x_AddToAffIdx_NoLock(aff_id, job_id);
    }

    }}

    trans.Commit();

    js_guard.Release();

    return job_id;
}

unsigned int 
CQueueDataBase::CQueue::SubmitBatch(vector<SNS_BatchSubmitRec> & batch,
                                    unsigned      host_addr,
                                    unsigned      port,
                                    unsigned      wait_timeout,
                                    const char*   progress_msg)
{
    unsigned job_id = m_Db.GetNextIdBatch(batch.size());

    SQueueDB& db = m_LQueue.db;

    unsigned batch_aff_id = 0; // if batch comes with the same affinity
    bool     batch_has_aff = false;

    // process affinity ids
    {{
    CBDB_Transaction trans(*db.GetEnv(), 
                        CBDB_Transaction::eTransASync,
                        CBDB_Transaction::eNoAssociation);
    for (unsigned i = 0; i < batch.size(); ++i) {
        SNS_BatchSubmitRec& subm = batch[i];
        if (subm.affinity_id == (unsigned)kMax_I4) { // take prev. token
            _ASSERT(i > 0);
            subm.affinity_id = batch[i-1].affinity_id;
        } else {
            if (subm.affinity_token[0]) {
                subm.affinity_id = 
                    m_LQueue.affinity_dict.CheckToken(
                                        subm.affinity_token, trans);
                batch_has_aff = true;
                batch_aff_id = (i == 0 )? subm.affinity_id : 0;
            } else {
                subm.affinity_id = 0;
                batch_aff_id = 0;
            }

        }
    }
  
    trans.Commit();
    }}

    CBDB_Transaction trans(*db.GetEnv(), 
                           CBDB_Transaction::eTransASync,
                           CBDB_Transaction::eNoAssociation);


    {{
    CFastMutexGuard guard(m_LQueue.lock);
	db.SetTransaction(&trans);

    unsigned job_id_cnt = job_id;
    NON_CONST_ITERATE(vector<SNS_BatchSubmitRec>, it, batch) {
        it->job_id = job_id_cnt;
        x_AssignSubmitRec(
            job_id_cnt, 
            it->input, host_addr, port, wait_timeout, 0/*progress_msg*/, 
            it->affinity_id);
        ++job_id_cnt;
        db.Insert();
    } // ITERATE

    // Store the affinity index
    m_LQueue.aff_idx.SetTransaction(&trans);
    if (batch_has_aff) {
        if (batch_aff_id) {  // whole batch comes with the same affinity
            x_AddToAffIdx_NoLock(batch_aff_id,
                                 job_id, 
                                 job_id + batch.size() - 1);
        } else {
            x_AddToAffIdx_NoLock(batch);
        }
    }

    }}
    trans.Commit();

    m_LQueue.status_tracker.AddPendingBatch(job_id, 
                                            job_id + batch.size() - 1);

    return job_id;
}


void 
CQueueDataBase::CQueue::x_AssignSubmitRec(unsigned      job_id,
                                          const char*   input,
                                          unsigned      host_addr,
                                          unsigned      port,
                                          unsigned      wait_timeout,
                                          const char*   progress_msg,
                                          unsigned      aff_id)
{
    SQueueDB& db = m_LQueue.db;

    db.id = job_id;
    db.status = (int) CNetScheduleClient::ePending;

    db.time_submit = time(0);
    db.time_run = 0;
    db.time_done = 0;
    db.timeout = 0;
    db.run_timeout = 0;

    db.subm_addr = host_addr;
    db.subm_port = port;
    db.subm_timeout = wait_timeout;

    db.worker_node1 = 0;
    db.worker_node2 = 0;
    db.worker_node3 = 0;
    db.worker_node4 = 0;
    db.worker_node5 = 0;

    db.run_counter = 0;
    db.ret_code = 0;
    db.time_lb_first_eval = 0;
    db.aff_id = aff_id;

    if (input) {
        db.input = input;
    }
    db.output = "";

    db.err_msg = "";

    db.cout = "";
    db.cerr = "";

    if (progress_msg) {
        db.progress_msg = progress_msg;
    }

}


unsigned 
CQueueDataBase::CQueue::CountStatus(CNetScheduleClient::EJobStatus st) const
{
    return m_LQueue.status_tracker.CountStatus(st);
}

void 
CQueueDataBase::CQueue::StatusStatistics(
    CNetScheduleClient::EJobStatus status,
    CNetScheduler_JobStatusTracker::TBVector::statistics* st) const
{
    m_LQueue.status_tracker.StatusStatistics(status, st);
}

void CQueueDataBase::CQueue::ForceReschedule(unsigned int job_id)
{
    {{
    SQueueDB& db = m_LQueue.db;
    CBDB_Transaction trans(*db.GetEnv(), 
                           CBDB_Transaction::eTransASync,
                           CBDB_Transaction::eNoAssociation);

    CFastMutexGuard guard(m_LQueue.lock);
    db.SetTransaction(0);

    db.id = job_id;
    if (db.Fetch() == eBDB_Ok) {
        //int status = db.status;
        db.status = (int) CNetScheduleClient::ePending;
        db.worker_node5 = 0; 

        unsigned run_counter = db.run_counter;
        if (run_counter) {
            db.run_counter = --run_counter;
        }

	    db.SetTransaction(&trans);
        db.UpdateInsert();
        trans.Commit();

    } else {
        // TODO: Integrity error or job just expired?
        return;
    }    
    }}


    m_LQueue.status_tracker.ForceReschedule(job_id);

}

void CQueueDataBase::CQueue::Cancel(unsigned int job_id)
{
//    CIdBusyGuard id_guard(&m_Db.m_UsedIds, job_id, 3);

    CNetSchedule_JS_Guard js_guard(m_LQueue.status_tracker, 
                                   job_id,
                                   CNetScheduleClient::eCanceled);
    CNetScheduleClient::EJobStatus st = js_guard.GetOldStatus();
    if (m_LQueue.status_tracker.IsCancelCode(st)) {
        js_guard.Release();
        return;
    }

    {{
    SQueueDB& db = m_LQueue.db;
    CBDB_Transaction trans(*db.GetEnv(), 
                           CBDB_Transaction::eTransASync,
                           CBDB_Transaction::eNoAssociation);

    CFastMutexGuard guard(m_LQueue.lock);
    db.SetTransaction(0);

    db.id = job_id;
    if (db.Fetch() == eBDB_Ok) {
        int status = db.status;
        if (status < (int) CNetScheduleClient::eCanceled) {
            db.status = (int) CNetScheduleClient::eCanceled;
            db.time_done = time(0);
            
	        db.SetTransaction(&trans);
            db.UpdateInsert();
            trans.Commit();
        }
    } else {
        // TODO: Integrity error or job just expired?
    }    
    }}
    js_guard.Release();

    RemoveFromTimeLine(job_id);
}

void CQueueDataBase::CQueue::DropJob(unsigned job_id)
{
    x_DropJob(job_id);

    if (m_LQueue.monitor.IsMonitorActive()) {
        CTime tmp_t(CTime::eCurrent);
        string msg = tmp_t.AsString();
        msg += " CQueue::DropJob() job id=";
        msg += NStr::IntToString(job_id);
        m_LQueue.monitor.SendString(msg);
    }
}

void CQueueDataBase::CQueue::x_DropJob(unsigned job_id)
{
    SQueueDB& db = m_LQueue.db;
    CBDB_Transaction trans(*db.GetEnv(), 
                           CBDB_Transaction::eTransASync,
                           CBDB_Transaction::eNoAssociation);

    m_LQueue.status_tracker.SetStatus(job_id, 
                                      CNetScheduleClient::eJobNotFound);
    {{
    CFastMutexGuard guard(m_LQueue.lock);
    db.SetTransaction(&trans);

    CBDB_FileCursor& cur = *GetCursor(trans);
    CBDB_CursorGuard cg(cur);    
    cur.SetCondition(CBDB_FileCursor::eEQ);
    cur.From << job_id;

    if (cur.FetchFirst() != eBDB_Ok) {
        return;
    }

    x_DeleteDBRec(db, cur);
    }}

    trans.Commit();
    
}

void CQueueDataBase::CQueue::PutResult(unsigned int  job_id,
                                       int           ret_code,
                                       const char*   output,
                                       bool          remove_from_tl)
{
    CNetSchedule_JS_Guard js_guard(m_LQueue.status_tracker, 
                                job_id,
                                CNetScheduleClient::eDone);
    CNetScheduleClient::EJobStatus st = js_guard.GetOldStatus();
    if (m_LQueue.status_tracker.IsCancelCode(st)) {
        js_guard.Release();
        return;
    }
    bool rec_updated;
    SSubmitNotifInfo si;
    time_t curr = time(0);

    SQueueDB& db = m_LQueue.db;

    for (unsigned repeat = 0; true; ) {
        try {
            CBDB_Transaction trans(*db.GetEnv(), 
                                CBDB_Transaction::eTransASync,
                                CBDB_Transaction::eNoAssociation);

            {{
            CFastMutexGuard guard(m_LQueue.lock);
            db.SetTransaction(&trans);
            CBDB_FileCursor& cur = *GetCursor(trans);
            CBDB_CursorGuard cg(cur);
            rec_updated = 
                x_UpdateDB_PutResultNoLock(db, curr, cur, 
                                        job_id, ret_code, output, &si);
            }}

            trans.Commit();
            js_guard.Release();
        } 
        catch (CBDB_ErrnoException& ex) {
            if (ex.IsDeadLock()) {
                if (++repeat < k_max_dead_locks) {
                    if (m_LQueue.monitor.IsMonitorActive()) {
                        m_LQueue.monitor.SendString(
                            "DeadLock repeat in CQueue::PutResult");
                    }
                    SleepMilliSec(250);
                    continue;
                }
            } else 
            if (ex.IsNoMem()) {
                if (++repeat < k_max_dead_locks) {
                    if (m_LQueue.monitor.IsMonitorActive()) {
                        m_LQueue.monitor.SendString(
                            "No resource repeat in CQueue::PutResult");
                    }
                    SleepMilliSec(250);
                    continue;
                }
            }
            ERR_POST("Too many transaction repeats in CQueue::PutResult.");
            throw;
        }
        break;
    } // for

    if (remove_from_tl) {
        RemoveFromTimeLine(job_id);
    }


    if (rec_updated) {
        // check if we need to send a UDP notification
        if ( si.subm_timeout && si.subm_addr && si.subm_port &&
            (si.time_submit + si.subm_timeout >= (unsigned)curr)) {

            char msg[1024];
            sprintf(msg, "JNTF %u", job_id);

            CFastMutexGuard guard(m_LQueue.us_lock);

            //EIO_Status status = 
                m_LQueue.udp_socket.Send(msg, strlen(msg)+1, 
                              CSocketAPI::ntoa(si.subm_addr), si.subm_port);
        }

        // Update runtime statistics

        unsigned run_elapsed = curr - si.time_run;
        unsigned turn_around = curr - si.time_submit;
        {{
        CFastMutexGuard guard(m_LQueue.qstat_lock);
        m_LQueue.qstat.run_count++;
        m_LQueue.qstat.total_run_time += run_elapsed;
        m_LQueue.qstat.total_turn_around_time += turn_around;
        }}

    }

    if (m_LQueue.monitor.IsMonitorActive()) {
        CTime tmp_t(CTime::eCurrent);
        string msg = tmp_t.AsString();
        msg += " CQueue::PutResult() job id=";
        msg += NStr::IntToString(job_id);
        msg += " ret_code=";
        msg += NStr::IntToString(ret_code);
        msg += " output=";
        msg += output;

        m_LQueue.monitor.SendString(msg);
    }

    
}

bool 
CQueueDataBase::CQueue::x_UpdateDB_PutResultNoLock(
                                        SQueueDB&            db,
                                        time_t               curr,
                                        CBDB_FileCursor&     cur,
                                        unsigned             job_id,
                                        int                  ret_code,
                                        const char*          output,
                                        SSubmitNotifInfo*    subm_info)
{
    cur.SetCondition(CBDB_FileCursor::eEQ);
    cur.From << job_id;

    if (cur.FetchFirst() != eBDB_Ok) {
        // TODO: Integrity error or job just expired?
        return false;
    }

    if (subm_info) {
        subm_info->time_submit  = db.time_submit;
        subm_info->time_run     = db.time_run;
        subm_info->subm_addr    = db.subm_addr;
        subm_info->subm_port    = db.subm_port;
        subm_info->subm_timeout = db.subm_timeout;
    }

    if (m_LQueue.delete_done) {
        cur.Delete();
    } else {
        db.status = (int) CNetScheduleClient::eDone;
        db.ret_code = ret_code;
        db.output = output;
        db.time_done = curr;

        cur.Update();
    }
    return true;
}

SQueueDB* CQueueDataBase::CQueue::x_GetLocalDb()
{
    SQueueDB* pqdb = m_QueueDB.get();
    if (pqdb == 0) {
        ++m_QueueDbAccessCounter;
        if (m_QueueDbAccessCounter > 2) {
            const string& file_name = m_LQueue.db.FileName();
            CBDB_Env* env = m_LQueue.db.GetEnv();
            m_QueueDB.reset(pqdb = new SQueueDB());
            if (env) {
                pqdb->SetEnv(*env);
            }
            pqdb->Open(file_name.c_str(), CBDB_RawFile::eReadWrite);
        }
    }
    return pqdb;
}

CBDB_FileCursor* 
CQueueDataBase::CQueue::x_GetLocalCursor(CBDB_Transaction& trans)
{
    CBDB_FileCursor* pcur = m_QueueDB_Cursor.get();
    if (pcur) {
        pcur->ReOpen(&trans);
        return pcur;
    }

    SQueueDB* pqdb = m_QueueDB.get();
    if (pqdb == 0) {
        return 0;
    }
    pcur = new CBDB_FileCursor(*pqdb, 
                               trans,
                               CBDB_FileCursor::eReadModifyUpdate);
    m_QueueDB_Cursor.reset(pcur);
    return pcur;
}

void 
CQueueDataBase::CQueue::PutResultGetJob(unsigned int   done_job_id,
                                        int            ret_code,
                                        const char*    output,
                                        char*          key_buf,
                                        unsigned int   worker_node,
                                        unsigned int*  job_id,
                                        char*          input,
                                        const string&  host,
                                        unsigned       port,
                                        bool           update_tl,
                                        const string&  client_name)
{
    _ASSERT(job_id);
    _ASSERT(input);

    unsigned dead_locks = 0;       // dead lock counter

    time_t curr = time(0);
    
    bool need_update;
    unsigned pending_job_id = 
        PutDone_FindPendingJob(client_name,
                               worker_node,
                               done_job_id, &need_update,
                               m_LQueue.aff_map_lock);
    bool done_rec_updated;
    bool use_db_mutex;

    // When working with the same database file concurrently there is
    // chance of internal Berkeley DB deadlock.(They say this is legal!)
    // In this case Berkeley DB returns an error code(DB_LOCK_DEADLOCK)
    // and recovery is up to the application.
    // If it happens I repeat the transaction several times.
    //
repeat_transaction:
    SQueueDB* pqdb = x_GetLocalDb();
    if (pqdb) {
        // we use private (this thread only data file)
        use_db_mutex = false;
    } else {
        use_db_mutex = true;
        pqdb = &m_LQueue.db;
    }

    unsigned job_aff_id = 0;

    try {
        CBDB_Transaction trans(*(pqdb->GetEnv()), 
                            CBDB_Transaction::eTransASync,
                            CBDB_Transaction::eNoAssociation);

        if (use_db_mutex) {
            m_LQueue.lock.Lock();
        }
        pqdb->SetTransaction(&trans);

        CBDB_FileCursor* pcur;
        if (use_db_mutex) {
            pcur = GetCursor(trans);
        } else {
            pcur = x_GetLocalCursor(trans);
        }
        CBDB_FileCursor& cur = *pcur;
        {{
            CBDB_CursorGuard cg(cur);

            // put result
            if (need_update) {
                done_rec_updated =
                    x_UpdateDB_PutResultNoLock(
                      *pqdb, curr, cur, done_job_id, ret_code, output, NULL);
            }

            if (pending_job_id) {
                *job_id = pending_job_id;
                EGetJobUpdateStatus upd_status;
                upd_status =
                    x_UpdateDB_GetJobNoLock(*pqdb, curr, cur, trans,
                                         worker_node, pending_job_id, input,
                                          &job_aff_id);
                switch (upd_status) {
                case eGetJobUpdate_JobFailed:
                    m_LQueue.status_tracker.ChangeStatus(*job_id, 
                                                CNetScheduleClient::eFailed);
                    *job_id = 0;
                    break;
                case eGetJobUpdate_JobStopped:
                    *job_id = 0;
                    break;
                case eGetJobUpdate_NotFound:
                    *job_id = 0;
                    break;
                case eGetJobUpdate_Ok:
                    break;
                default:
                    _ASSERT(0);
                } // switch

            } else {
                *job_id = 0;
            }
        }}

        if (use_db_mutex) {
            m_LQueue.lock.Unlock();
        }

        trans.Commit();

    }
    catch (CBDB_ErrnoException& ex) {
        if (use_db_mutex) {
            m_LQueue.lock.Unlock();
        }
        if (ex.IsDeadLock()) {
            if (++dead_locks < k_max_dead_locks) {
                if (m_LQueue.monitor.IsMonitorActive()) {
                    m_LQueue.monitor.SendString(
                        "DeadLock repeat in CQueue::JobExchange");
                }
                SleepMilliSec(250);
                goto repeat_transaction;
            } 
        }
        else
        if (ex.IsNoMem()) {
            if (++dead_locks < k_max_dead_locks) {
                if (m_LQueue.monitor.IsMonitorActive()) {
                    m_LQueue.monitor.SendString(
                        "No resource repeat in CQueue::PutResult");
                }
                SleepMilliSec(250);
                goto repeat_transaction;
            }
        }
        ERR_POST("Too many transaction repeats in CQueue::JobExchange.");
        throw;
    }
    catch (...) {
        if (use_db_mutex) {
            m_LQueue.lock.Unlock();
        }
        throw;
    }


    if (job_aff_id) {
        CFastMutexGuard aff_guard(m_LQueue.aff_map_lock);
        m_LQueue.worker_aff_map.AddAffinity(worker_node, 
                                            client_name, 
                                            job_aff_id);
    }

    if (*job_id) {
        sprintf(key_buf, NETSCHEDULE_JOBMASK, *job_id, host.c_str(), port);
    }

    if (update_tl) {
        TimeLineExchange(done_job_id, *job_id, curr);
    }
    if (done_rec_updated) {
        // TODO: send a UDP notification and update runtime stat
    }

    if (m_LQueue.monitor.IsMonitorActive()) {
        CTime tmp_t(CTime::eCurrent);
        string msg = tmp_t.AsString();

        msg += " CQueue::PutResultGetJob() (PUT) job id=";
        msg += NStr::IntToString(done_job_id);
        msg += " ret_code=";
        msg += NStr::IntToString(ret_code);
        msg += " output=";
        msg += output;

        m_LQueue.monitor.SendString(msg);
    }

}


void CQueueDataBase::CQueue::PutProgressMessage(unsigned int  job_id,
                                                const char*   msg)
{
    SQueueDB& db = m_LQueue.db;
    CBDB_Transaction trans(*db.GetEnv(), 
                           CBDB_Transaction::eTransASync,
                           CBDB_Transaction::eNoAssociation);

    CFastMutexGuard guard(m_LQueue.lock);
    db.SetTransaction(&trans);

    {{
    CBDB_FileCursor& cur = *GetCursor(trans);
    CBDB_CursorGuard cg(cur);    

    cur.SetCondition(CBDB_FileCursor::eEQ);
    cur.From << job_id;

    if (cur.FetchFirst() != eBDB_Ok) {
        // TODO: Integrity error or job just expired?
        return;
    }
    db.progress_msg = msg;
    cur.Update();
    }}

    trans.Commit();

    if (m_LQueue.monitor.IsMonitorActive()) {
        CTime tmp_t(CTime::eCurrent);
        string mmsg = tmp_t.AsString();
        mmsg += " CQueue::PutProgressMessage() job id=";
        mmsg += NStr::IntToString(job_id);
        mmsg += " msg=";
        mmsg += msg;

        m_LQueue.monitor.SendString(mmsg);
    }

}


void CQueueDataBase::CQueue::JobFailed(unsigned int  job_id,
                                       const string& err_msg)
{
    m_LQueue.status_tracker.SetStatus(job_id, CNetScheduleClient::eFailed);

    SQueueDB& db = m_LQueue.db;
    CBDB_Transaction trans(*db.GetEnv(), 
                           CBDB_Transaction::eTransASync,
                           CBDB_Transaction::eNoAssociation);

    unsigned subm_addr, subm_port, subm_timeout, time_submit;
    time_t curr = time(0);

    {{
    CFastMutexGuard guard(m_LQueue.lock);
    db.SetTransaction(&trans);

    CBDB_FileCursor& cur = *GetCursor(trans);
    CBDB_CursorGuard cg(cur);    

    cur.SetCondition(CBDB_FileCursor::eEQ);
    cur.From << job_id;

    if (cur.FetchFirst() != eBDB_Ok) {
        // TODO: Integrity error or job just expired?
        return;
    }
    time_submit = db.time_submit;
    subm_addr = db.subm_addr;
    subm_port = db.subm_port;
    subm_timeout = db.subm_timeout;
    
    db.status = (int) CNetScheduleClient::eFailed;
    db.time_done = curr;
    db.err_msg = err_msg;

    cur.Update();

    }}

    trans.Commit();

    RemoveFromTimeLine(job_id);

    // check if we need to send a UDP notification

    if ( subm_addr && subm_timeout &&
        (time_submit + subm_timeout >= (unsigned)curr)) {

        char msg[1024];
        sprintf(msg, "JNTF %u", job_id);

        CFastMutexGuard guard(m_LQueue.us_lock);

        //EIO_Status status = 
            m_LQueue.udp_socket.Send(msg, strlen(msg)+1, 
                                     CSocketAPI::ntoa(subm_addr), subm_port);
    }

    if (m_LQueue.monitor.IsMonitorActive()) {
        CTime tmp_t(CTime::eCurrent);
        string msg = tmp_t.AsString();
        msg += " CQueue::JobFailed() job id=";
        msg += NStr::IntToString(job_id);
        msg += " err_msg=";
        msg += err_msg;
        m_LQueue.monitor.SendString(msg);
    }

}


void CQueueDataBase::CQueue::SetJobRunTimeout(unsigned job_id, unsigned tm)
{
    CNetScheduleClient::EJobStatus st = GetStatus(job_id);
    if (st != CNetScheduleClient::eRunning) {
        return;
    }
    SQueueDB& db = m_LQueue.db;
    CBDB_Transaction trans(*db.GetEnv(), 
                           CBDB_Transaction::eTransASync,
                           CBDB_Transaction::eNoAssociation);

    unsigned exp_time = 0;
    time_t curr = time(0);

    {{
    CFastMutexGuard guard(m_LQueue.lock);
    db.SetTransaction(&trans);

    CBDB_FileCursor& cur = *GetCursor(trans);
    CBDB_CursorGuard cg(cur);    

    cur.SetCondition(CBDB_FileCursor::eEQ);
    cur.From << job_id;

    if (cur.FetchFirst() != eBDB_Ok) {
        return;
    }

    int status = db.status;

    if (status != (int)CNetScheduleClient::eRunning) {
        return;
    }

    unsigned time_run = db.time_run;
    unsigned run_timeout = db.run_timeout;

    exp_time = x_ComputeExpirationTime(time_run, run_timeout);
    if (exp_time == 0) {
        return;
    }

    db.run_timeout = tm;
    db.time_run = curr;

    cur.Update();

    }}

    trans.Commit();

    {{
        CJobTimeLine& tl = *m_LQueue.run_time_line;

        CWriteLockGuard guard(m_LQueue.rtl_lock);
        tl.MoveObject(exp_time, curr + tm, job_id);
    }}

    if (m_LQueue.monitor.IsMonitorActive()) {
        CTime tmp_t(CTime::eCurrent);
        string msg = tmp_t.AsString();
        msg += " CQueue::SetJobRunTimeout: Job id=";
        msg += NStr::IntToString(job_id);
        tmp_t.SetTimeT(curr + tm);
        tmp_t.ToLocalTime();
        msg += " new_expiration_time=";
        msg += tmp_t.AsString();
        msg += " job_timeout(sec)=";
        msg += NStr::IntToString(tm);
        msg += " job_timeout(minutes)=";
        msg += NStr::IntToString(tm/60);

        m_LQueue.monitor.SendString(msg);
    }

}

void CQueueDataBase::CQueue::ReturnJob(unsigned int job_id)
{
    _ASSERT(job_id);

//    CIdBusyGuard id_guard(&m_Db.m_UsedIds, job_id, 3);
    CNetSchedule_JS_Guard js_guard(m_LQueue.status_tracker, 
                                   job_id,
                                   CNetScheduleClient::eReturned);
    CNetScheduleClient::EJobStatus st = js_guard.GetOldStatus();
    // if canceled or already returned or done
    if ( m_LQueue.status_tracker.IsCancelCode(st) || 
        (st == CNetScheduleClient::eReturned) || 
        (st == CNetScheduleClient::eDone)) {
        js_guard.Release();
        return;
    }
    {{
    SQueueDB& db = m_LQueue.db;

    CFastMutexGuard guard(m_LQueue.lock);
    db.SetTransaction(0);

    db.id = job_id;
    if (db.Fetch() == eBDB_Ok) {            
        CBDB_Transaction trans(*db.GetEnv(), 
                               CBDB_Transaction::eTransASync,
                               CBDB_Transaction::eNoAssociation);
        db.SetTransaction(&trans);

        // Pending status is not a bug here, returned and pending
        // has the same meaning, but returned jobs are getting delayed
        // for a little while (eReturned status)
        db.status = (int) CNetScheduleClient::ePending;
        unsigned run_counter = db.run_counter;
        if (run_counter) {
            db.run_counter = --run_counter;
        }

        db.UpdateInsert();

        trans.Commit();
    } else {
        // TODO: Integrity error or job just expired?
    }    
    }}
    js_guard.Release();
    RemoveFromTimeLine(job_id);

    if (m_LQueue.monitor.IsMonitorActive()) {
        CTime tmp_t(CTime::eCurrent);
        string msg = tmp_t.AsString();
        msg += " CQueue::ReturnJob: job id=";
        msg += NStr::IntToString(job_id);
        m_LQueue.monitor.SendString(msg);
    }
}

void CQueueDataBase::CQueue::GetJobLB(unsigned int   worker_node,
                                      unsigned int*  job_id, 
                                      char*          input)
{
    _ASSERT(worker_node && input);
    unsigned get_attempts = 0;
    unsigned fetch_attempts = 0;
    const unsigned kMaxGetAttempts = 100;


    ++get_attempts;
    if (get_attempts > kMaxGetAttempts) {
        *job_id = 0;
        return;
    }

    *job_id = m_LQueue.status_tracker.BorrowPendingJob();

    if (!*job_id) {
        return;
    }
    CNetSchedule_JS_BorrowGuard bguard(m_LQueue.status_tracker, 
                                       *job_id,
                                       CNetScheduleClient::ePending);

    time_t curr = time(0);
    unsigned lb_stall_time = 0;
    ENSLB_RunDelayType stall_delay_type;


    //
    // Define current execution delay
    //

    {{
    CFastMutexGuard guard(m_LQueue.lb_stall_time_lock);
    stall_delay_type = m_LQueue.lb_stall_delay_type;
    lb_stall_time    = m_LQueue.lb_stall_time;
    }}

    switch (stall_delay_type) {
    case eNSLB_Constant:
        break;
    case eNSLB_RunTimeAvg:
        {
            double total_run_time, run_count, lb_stall_time_mult;

            {{
            CFastMutexGuard guard(m_LQueue.qstat_lock);
            if (m_LQueue.qstat.run_count) {
                total_run_time = m_LQueue.qstat.total_run_time;
                run_count = m_LQueue.qstat.run_count;
                lb_stall_time_mult = m_LQueue.lb_stall_time_mult;
            } else {
                // no statistics yet, nothing to do, take default queue value
                break;
            }
            }}

            double avg_time = total_run_time / run_count;
            lb_stall_time = (unsigned)(avg_time * lb_stall_time_mult);
        }
        break;
    default:
        _ASSERT(0);
    } // switch

    if (lb_stall_time == 0) {
        lb_stall_time = 6;
    }


fetch_db:
    ++fetch_attempts;
    if (fetch_attempts > kMaxGetAttempts) {
        LOG_POST(Error << "Failed to fetch the job record job_id=" << *job_id);
        *job_id = 0;
        return;
    }
 

    try {
        SQueueDB& db = m_LQueue.db;
        CBDB_Transaction trans(*db.GetEnv(),
                            CBDB_Transaction::eTransASync,
                            CBDB_Transaction::eNoAssociation);
        {{
        CFastMutexGuard guard(m_LQueue.lock);
        db.SetTransaction(&trans);

        CBDB_FileCursor& cur = *GetCursor(trans);
        CBDB_CursorGuard cg(cur);    

        cur.SetCondition(CBDB_FileCursor::eEQ);
        cur.From << *job_id;
        if (cur.Fetch() != eBDB_Ok) {
            if (fetch_attempts < kMaxGetAttempts) {
                goto fetch_db;
            }
            *job_id = 0;
            return;
        }
        CNetScheduleClient::EJobStatus status = 
            (CNetScheduleClient::EJobStatus)(int)db.status;

        // internal integrity check
        if (!(status == CNetScheduleClient::ePending ||
              status == CNetScheduleClient::eReturned)
            ) {
            if (m_LQueue.status_tracker.IsCancelCode(
                (CNetScheduleClient::EJobStatus) status)) {
                // this job has been canceled while i'm fetching
                *job_id = 0;
                return;
            }
                ERR_POST(Error << "GetJobLB()::Status integrity violation " 
                            << " job = " << *job_id 
                            << " status = " << status
                            << " expected status = " 
                            << (int)CNetScheduleClient::ePending);
            *job_id = 0;
            return;
        }

        const char* fld_str = db.input;
        ::strcpy(input, fld_str);
        unsigned run_counter = db.run_counter;

        unsigned time_submit = db.time_submit;
        unsigned timeout = db.timeout;
        if (timeout == 0) {
            timeout = m_LQueue.timeout;
        }

        _ASSERT(timeout);

        // check if job already expired
        if (timeout && (time_submit + timeout < (unsigned)curr)) {
            *job_id = 0; 
            db.time_run = 0;
            db.time_done = curr;
            db.status = (int) CNetScheduleClient::eFailed;
            db.err_msg = "Job expired and cannot be scheduled.";

            bguard.ReturnToStatus(CNetScheduleClient::eFailed);

            if (m_LQueue.monitor.IsMonitorActive()) {
                CTime tmp_t(CTime::eCurrent);
                string msg = tmp_t.AsString();
                msg += " CQueue::GetJobLB() timeout expired job id=";
                msg += NStr::IntToString(*job_id);
                m_LQueue.monitor.SendString(msg);
            }

            cur.Update();
            trans.Commit();

            return;
        } 

        CNSLB_Coordinator::ENonLbHostsPolicy unk_host_policy =
            m_LQueue.lb_coordinator->GetNonLBPolicy();

        // check if job must be scheduled immediatelly, 
        // because it's stalled for too long
        //
        unsigned time_lb_first_eval = db.time_lb_first_eval;
        unsigned stall_time = 0; 
        if (time_lb_first_eval) {
            stall_time = curr - time_lb_first_eval;
            if (lb_stall_time) {
                if (stall_time > lb_stall_time) {
                    // stalled for too long! schedule the job
                    if (unk_host_policy != CNSLB_Coordinator::eNonLB_Deny) {
                        goto grant_job;
                    }
                }
            } else {
                // exec delay not set, schedule the job
                goto grant_job;
            }
        } else { // first LB evaluation
            db.time_lb_first_eval = curr;
        }

        // job can be scheduled now, if load balancing situation is permitting
        {{
        CNSLB_Coordinator* coordinator = m_LQueue.lb_coordinator;
        if (coordinator) {
            CNSLB_DecisionModule::EDecision lb_decision = 
                coordinator->Evaluate(worker_node, stall_time);
            switch (lb_decision) {
            case CNSLB_DecisionModule::eGrantJob:
                break;
            case CNSLB_DecisionModule::eDenyJob:
                *job_id = 0;
                break;
            case CNSLB_DecisionModule::eHostUnknown:
                if (unk_host_policy != CNSLB_Coordinator::eNonLB_Allow) {
                    *job_id = 0;
                }
                break;
            case CNSLB_DecisionModule::eNoLBInfo:
                break;
            case CNSLB_DecisionModule::eInsufficientInfo:
                break;
            default:
                _ASSERT(0);
            } // switch

            if (m_LQueue.monitor.IsMonitorActive()) {
                CTime tmp_t(CTime::eCurrent);
                string msg = " CQueue::GetJobLB() LB decision = ";
                msg += CNSLB_DecisionModule::DecisionToStrint(lb_decision);
                msg += " job id = ";
                msg += NStr::IntToString(*job_id);
                msg += " worker_node=";
                msg += CSocketAPI::gethostbyaddr(worker_node);
                m_LQueue.monitor.SendString(msg);
            }

        }
        }}
grant_job:
        // execution granted, update job record information
        if (*job_id) {
            db.status = (int) CNetScheduleClient::eRunning;
            db.time_run = curr;
            db.run_timeout = 0;
            db.run_counter = ++run_counter;

            switch (run_counter) {
            case 1:
                db.worker_node1 = worker_node;
                break;
            case 2:
                db.worker_node2 = worker_node;
                break;
            case 3:
                db.worker_node3 = worker_node;
                break;
            case 4:
                db.worker_node4 = worker_node;
                break;
            case 5:
                db.worker_node5 = worker_node;
                break;
            default:

                bguard.ReturnToStatus(CNetScheduleClient::eFailed);
                ERR_POST(Error << "Too many run attempts. job=" << *job_id);
                db.status = (int) CNetScheduleClient::eFailed;
                db.err_msg = "Too many run attempts.";
                db.time_done = curr;
                db.run_counter = --run_counter;

                if (m_LQueue.monitor.IsMonitorActive()) {
                    CTime tmp_t(CTime::eCurrent);
                    string msg = tmp_t.AsString();
                    msg += " CQueue::GetJobLB() Too many run attempts job id=";
                    msg += NStr::IntToString(*job_id);
                    m_LQueue.monitor.SendString(msg);
                }

                *job_id = 0; 

            } // switch

        } // if (*job_id)

        cur.Update();

        }}
        trans.Commit();

    } 
    catch (exception&)
    {
        *job_id = 0;
        throw;
    }
    bguard.ReturnToStatus(CNetScheduleClient::eRunning);

    if (m_LQueue.monitor.IsMonitorActive() && *job_id) {
        CTime tmp_t(CTime::eCurrent);
        string msg = tmp_t.AsString();
        msg += " CQueue::GetJobLB() job id=";
        msg += NStr::IntToString(*job_id);
        msg += " worker_node=";
        msg += CSocketAPI::gethostbyaddr(worker_node);
        m_LQueue.monitor.SendString(msg);
    }

    // setup the job in the timeline
    if (*job_id && m_LQueue.run_time_line) {
        CJobTimeLine& tl = *m_LQueue.run_time_line;
        time_t projected_time_done = curr + m_LQueue.run_timeout;

        CWriteLockGuard guard(m_LQueue.rtl_lock);
        tl.AddObject(projected_time_done, *job_id);
    }
}

unsigned 
CQueueDataBase::CQueue::PutDone_FindPendingJob(const string&  client_name,
                                               unsigned       client_addr,
                                               unsigned int   done_job_id,
                                               bool*          need_db_update,
                                               CFastMutex&    aff_map_lock)
{
    unsigned job_id = 0;

    // affinity: get list of job candidates
    // previous GetPendingJob() call may have precomputed candidate jobids

    {{
        CFastMutexGuard aff_guard(aff_map_lock);
        CWorkerNodeAffinity::SAffinityInfo* ai = 
            m_LQueue.worker_aff_map.GetAffinity(client_addr, client_name);

        if (ai != 0) {  // established affinity association
            while (1) {
                if (ai->cand_jobs.any()) {  // candidates?

                    bool pending_jobs_avail = 
                        m_LQueue.status_tracker.PutDone_GetPending(
                                                        done_job_id, 
                                                        need_db_update, 
                                                        &ai->cand_jobs,
                                                        &job_id);
                    if (m_LQueue.delete_done) {
                        m_LQueue.status_tracker.SetStatus(done_job_id, 
                                         CNetScheduleClient::eJobNotFound);
                    }
                    done_job_id = 0;
                    if (job_id)
                        return job_id;
                    if (!pending_jobs_avail) {
                        return 0;
                    }
                }
                // no candidates immediately available - find candidates                
                if (ai->aff_ids.any()) { // there is an affinity association
                    {{
                        CFastMutexGuard guard(m_LQueue.lock);
                        x_ReadAffIdx_NoLock(ai->aff_ids, &ai->cand_jobs);
                    }}
                    if (!ai->cand_jobs.any()) { // no candidates
                        break;
                    }
                    m_LQueue.status_tracker.PendingIntersect(&ai->cand_jobs);
                    ai->cand_jobs.count(); // re-calc count to speed up any()
                    if (!ai->cand_jobs.any()) {
                        break;
                    }
                    ai->cand_jobs.optimize(0, bm::bvector<>::opt_free_0);
                }
            } // while
        }
    }}

    // no affinity association or there are no more jobs with 
    // established affinity


    // try to find a vacant(not taken by any other worker node) affinity id
    {{
        bm::bvector<> assigned_aff;
        {{
        CFastMutexGuard aff_guard(aff_map_lock);
        m_LQueue.worker_aff_map.GetAllAssignedAffinity(&assigned_aff);
        }}

        if (assigned_aff.any()) {
            // get all jobs belonging to other (already assigned) affinities
            bm::bvector<> assigned_candidate_jobs;
            {{
            CFastMutexGuard guard(m_LQueue.lock);
            x_ReadAffIdx_NoLock(assigned_aff, &assigned_candidate_jobs);
            }}
            // we got list of jobs we do NOT want to schedule
            bool pending_jobs_avail = 
                m_LQueue.status_tracker.PutDone_GetPending(
                                                done_job_id, 
                                                need_db_update, 
                                                assigned_candidate_jobs,
                                                &job_id);
            if (m_LQueue.delete_done) {
                m_LQueue.status_tracker.SetStatus(done_job_id, 
                                    CNetScheduleClient::eJobNotFound);
            }
            done_job_id = 0;
            if (job_id)
                return job_id;
            if (!pending_jobs_avail) {
                return 0;
            }
        }

    }}
    

    // we just take the first available job in the queue

    _ASSERT(job_id == 0);
    job_id = 
        m_LQueue.status_tracker.PutDone_GetPending(done_job_id, 
                                                   need_db_update);
    if (m_LQueue.delete_done) {
        m_LQueue.status_tracker.SetStatus(done_job_id, 
                                          CNetScheduleClient::eJobNotFound);
    }

    done_job_id = 0; // paranoiya assignment

    return job_id;
}


void CQueueDataBase::CQueue::GetJob(unsigned int   worker_node,
                                    unsigned int*  job_id, 
                                    char*          input,
                                    const string&  client_name)
{
    if (m_LQueue.lb_flag) {
        GetJobLB(worker_node, job_id, input);
        return;
    }

    _ASSERT(worker_node && input);
    unsigned get_attempts = 0;
    const unsigned kMaxGetAttempts = 100;
    EGetJobUpdateStatus upd_status;

get_job_id:

    ++get_attempts;
    if (get_attempts > kMaxGetAttempts) {
        *job_id = 0;
        return;
    }
    time_t curr = time(0);

    // affinity: get list of job candidates
    // previous GetJob() call may have precomputed sutable job ids
    //

    *job_id = PutDone_FindPendingJob(client_name,
                                     worker_node,
                                     0, 0, // no done job here
                                     m_LQueue.aff_map_lock);
    if (!*job_id) {
        return;
    }

    unsigned job_aff_id = 0;

    try {
        SQueueDB& db = m_LQueue.db;
        CBDB_Transaction trans(*db.GetEnv(), 
                               CBDB_Transaction::eTransASync,
                               CBDB_Transaction::eNoAssociation);


        {{
        CFastMutexGuard guard(m_LQueue.lock);
        db.SetTransaction(&trans);

        CBDB_FileCursor& cur = *GetCursor(trans);
        CBDB_CursorGuard cg(cur); 

        upd_status = 
            x_UpdateDB_GetJobNoLock(db, curr, cur, trans, 
                                    worker_node, *job_id, input,
                                    &job_aff_id);
        }}
        trans.Commit();

        switch (upd_status) {
        case eGetJobUpdate_JobFailed:
            m_LQueue.status_tracker.ChangeStatus(*job_id, 
                                         CNetScheduleClient::eFailed);
            *job_id = 0;
            break;
        case eGetJobUpdate_JobStopped:
            *job_id = 0;
            break;
        case eGetJobUpdate_NotFound:
            *job_id = 0;
            break;
        case eGetJobUpdate_Ok:
            break;
        default:
            _ASSERT(0);
        } // switch


        if (m_LQueue.monitor.IsMonitorActive() && *job_id) {
            CTime tmp_t(CTime::eCurrent);
            string msg = tmp_t.AsString();
            msg += " CQueue::GetJob() job id=";
            msg += NStr::IntToString(*job_id);
            msg += " worker_node=";
            msg += CSocketAPI::gethostbyaddr(worker_node);
            m_LQueue.monitor.SendString(msg);
        }

    } 
    catch (exception&)
    {
        m_LQueue.status_tracker.ChangeStatus(*job_id, 
                                             CNetScheduleClient::ePending);
        *job_id = 0;
        throw;
    }

    // if we picked up expired job and need to re-get another job id    
    if (*job_id == 0) {
        goto get_job_id;
    }

    if (job_aff_id) {
        CFastMutexGuard aff_guard(m_LQueue.aff_map_lock);
        m_LQueue.worker_aff_map.AddAffinity(worker_node, 
                                            client_name, 
                                            job_aff_id);
    }


    // setup the job in the timeline
    if (*job_id && m_LQueue.run_time_line) {
        CJobTimeLine& tl = *m_LQueue.run_time_line;
        time_t projected_time_done = curr + m_LQueue.run_timeout;

        CWriteLockGuard guard(m_LQueue.rtl_lock);
        tl.AddObject(projected_time_done, *job_id);
    }
}


CQueueDataBase::CQueue::EGetJobUpdateStatus 
CQueueDataBase::CQueue::x_UpdateDB_GetJobNoLock(
                                            SQueueDB&            db,
                                            time_t               curr,
                                            CBDB_FileCursor&     cur,
                                            CBDB_Transaction&    trans,
                                            unsigned int         worker_node,
                                            unsigned             job_id,
                                            char*                input,
                                            unsigned*            aff_id)
{
    const unsigned kMaxGetAttempts = 100;

    for (unsigned fetch_attempts = 0; fetch_attempts < kMaxGetAttempts;
         ++fetch_attempts) {

        cur.SetCondition(CBDB_FileCursor::eEQ);
        cur.From << job_id;

        if (cur.Fetch() != eBDB_Ok) {
            cur.Close();
            cur.ReOpen(&trans);
            continue;
        }
        int status = db.status;
        *aff_id = db.aff_id;

        // internal integrity check
        if (!(status == (int)CNetScheduleClient::ePending ||
              status == (int)CNetScheduleClient::eReturned)
            ) {
            if (m_LQueue.status_tracker.IsCancelCode(
                (CNetScheduleClient::EJobStatus) status)) {
                // this job has been canceled while i'm fetching
                return eGetJobUpdate_JobStopped;
            }
            ERR_POST(Error 
                << "x_UpdateDB_GetJobNoLock::Status integrity violation "
                << " job = "     << job_id
                << " status = "  << status
                << " expected status = "
                << (int)CNetScheduleClient::ePending);
            return eGetJobUpdate_JobStopped;
        }

        const char* fld_str = db.input;
        ::strcpy(input, fld_str);
        unsigned run_counter = db.run_counter;

        db.status = (int) CNetScheduleClient::eRunning;
        db.time_run = curr;
        db.run_timeout = 0;
        db.run_counter = ++run_counter;

        unsigned time_submit = db.time_submit;
        unsigned timeout = db.timeout;
        if (timeout == 0) {
            timeout = m_LQueue.timeout;
        }

        _ASSERT(timeout);
        // check if job already expired
        if (timeout && (time_submit + timeout < (unsigned)curr)) {
            db.time_run = 0;
            db.time_done = curr;
            db.run_counter = --run_counter;
            db.status = (int) CNetScheduleClient::eFailed;
            db.err_msg = "Job expired and cannot be scheduled.";
            m_LQueue.status_tracker.ChangeStatus(job_id, 
                                        CNetScheduleClient::eFailed);

            cur.Update();

            if (m_LQueue.monitor.IsMonitorActive()) {
                CTime tmp_t(CTime::eCurrent);
                string msg = tmp_t.AsString();
                msg += 
                 " CQueue::x_UpdateDB_GetJobNoLock() timeout expired job id=";
                msg += NStr::IntToString(job_id);
                m_LQueue.monitor.SendString(msg);
            }
            return eGetJobUpdate_JobFailed;

        } else {  // job not expired
            switch (run_counter) {
            case 1:
                db.worker_node1 = worker_node;
                break;
            case 2:
                db.worker_node2 = worker_node;
                break;
            case 3:
                db.worker_node3 = worker_node;
                break;
            case 4:
                db.worker_node4 = worker_node;
                break;
            case 5:
                db.worker_node5 = worker_node;
                break;
            default:
                m_LQueue.status_tracker.ChangeStatus(job_id, 
                                            CNetScheduleClient::eFailed);
                ERR_POST(Error << "Too many run attempts. job=" << job_id);
                db.status = (int) CNetScheduleClient::eFailed;
                db.err_msg = "Too many run attempts.";
                db.time_done = curr;
                db.run_counter = --run_counter;

                cur.Update();

                if (m_LQueue.monitor.IsMonitorActive()) {
                    CTime tmp_t(CTime::eCurrent);
                    string msg = tmp_t.AsString();
                    msg += " CQueue::GetJob() Too many run attempts job id=";
                    msg += NStr::IntToString(job_id);
                    m_LQueue.monitor.SendString(msg);
                }

                return eGetJobUpdate_JobFailed;

            } // switch

            // all checks passed successfully...
            cur.Update();
            return eGetJobUpdate_Ok;

        } // else
    } // for

    return eGetJobUpdate_NotFound;
 
}


void CQueueDataBase::CQueue::GetJob(char*          key_buf, 
                                    unsigned int   worker_node,
                                    unsigned int*  job_id,
                                    char*          input,
                                    const string&  host,
                                    unsigned       port,
                                    const string&  client_name)
{
    GetJob(worker_node, job_id, input, client_name);
    if (*job_id) {
        sprintf(key_buf, NETSCHEDULE_JOBMASK, *job_id, host.c_str(), port);
    }
}

bool 
CQueueDataBase::CQueue::GetJobDescr(unsigned int job_id,
                                    int*         ret_code,
                                    char*        input,
                                    char*        output,
                                    char*        err_msg,
                                    char*        progress_msg,
                            CNetScheduleClient::EJobStatus expected_status)
{
    SQueueDB& db = m_LQueue.db;

    for (unsigned i = 0; i < 3; ++i) {
        {{
        CFastMutexGuard guard(m_LQueue.lock);
        db.SetTransaction(0);

        db.id = job_id;
        if (db.Fetch() == eBDB_Ok) {
            if (expected_status != CNetScheduleClient::eJobNotFound) {
                CNetScheduleClient::EJobStatus status =
                    (CNetScheduleClient::EJobStatus)(int)db.status;
                if (status != expected_status) {
                    goto wait_sleep;
                }
            }
            if (ret_code)
                *ret_code = db.ret_code;

            if (input) {
                ::strcpy(input, (const char* )db.input);
            }
            if (output) {
                ::strcpy(output, (const char* )db.output);
            }
            if (err_msg) {
                ::strcpy(err_msg, (const char* )db.err_msg);
            }
            if (progress_msg) {
                ::strcpy(progress_msg, (const char* )db.progress_msg);
            }

            return true;
        }
        }}
    wait_sleep:
        // failed to read the record (maybe looks like writer is late, so we 
        // need to retry a bit later)
        SleepMilliSec(300);
    }

    return false; // job not found
}

CNetScheduleClient::EJobStatus 
CQueueDataBase::CQueue::GetStatus(unsigned int job_id) const
{
    return m_LQueue.status_tracker.GetStatus(job_id);
}

CBDB_FileCursor* CQueueDataBase::CQueue::GetCursor(
    CBDB_Transaction& trans
    )
{
    CBDB_FileCursor* cur = m_LQueue.cur.get();
    if (cur) { 
        cur->ReOpen(&trans);
        return cur;
    }
    cur = new CBDB_FileCursor(m_LQueue.db, 
                              trans,
                              CBDB_FileCursor::eReadModifyUpdate);
    m_LQueue.cur.reset(cur);
    return cur;
    
}

bool CQueueDataBase::CQueue::CheckDelete(unsigned int job_id)
{
    SQueueDB& db = m_LQueue.db;
    CBDB_Transaction trans(*db.GetEnv(), 
                           CBDB_Transaction::eTransASync,
                           CBDB_Transaction::eNoAssociation);

    time_t curr = time(0);
    unsigned time_done, time_submit;
    int job_ttl;

    {{
    CFastMutexGuard guard(m_LQueue.lock);
    db.SetTransaction(&trans);

    CBDB_FileCursor& cur = *GetCursor(trans);
    CBDB_CursorGuard cg(cur);    
    cur.SetCondition(CBDB_FileCursor::eEQ);
    cur.From << job_id;

    if (cur.FetchFirst() != eBDB_Ok) {
        return true; // already deleted
    }

    int status = db.status;
/*
    if (! (
           (status == (int) CNetScheduleClient::eDone) ||
           (status == (int) CNetScheduleClient::eCanceled) ||
           (status == (int) CNetScheduleClient::eFailed) 
           )
        ) {
        return true;
    }
*/
    unsigned queue_ttl = m_LQueue.timeout;
    job_ttl = db.timeout;
    if (job_ttl <= 0) {
        job_ttl = queue_ttl;
    }


    time_submit = db.time_submit;
    time_done = db.time_done;

    // pending jobs expire just as done jobs
    if (time_done == 0 && status == (int) CNetScheduleClient::ePending) {
        time_done = time_submit;
    }

    if (time_done == 0) { 
        if (time_submit + (job_ttl * 10) > (unsigned)curr) {
            return false;
        }
    } else {
        if (time_done + job_ttl > (unsigned)curr) {
            return false;
        }
    }

    x_DeleteDBRec(db, cur);

    }}
    trans.Commit();

    m_LQueue.status_tracker.SetStatus(job_id, 
                                      CNetScheduleClient::eJobNotFound);

    if (m_LQueue.monitor.IsMonitorActive()) {
        CTime tm(CTime::eCurrent);
        string msg = tm.AsString();
        msg += " CQueue::CheckDelete: Job deleted id=";
        msg += NStr::IntToString(job_id);
        tm.SetTimeT(time_done);
        tm.ToLocalTime();
        msg += " time_done=";
        msg += tm.AsString();
        msg += " job_ttl(sec)=";
        msg += NStr::IntToString(job_ttl);
        msg += " job_ttl(minutes)=";
        msg += NStr::IntToString(job_ttl/60);

        m_LQueue.monitor.SendString(msg);
    }

    return true;
}

void CQueueDataBase::CQueue::x_DeleteDBRec(SQueueDB&  db, 
                                           CBDB_FileCursor& cur)
{
    // dump the record
    {{
        CFastMutexGuard dguard(m_LQueue.rec_dump_lock);
        if (m_LQueue.rec_dump_flag) {
            x_PrintJobDbStat(db,
                             m_LQueue.rec_dump,
                             ";",               // field separator
                             false              // no field names
                             );
        }
    }}

    cur.Delete(CBDB_File::eIgnoreError);
}


unsigned 
CQueueDataBase::CQueue::CheckDeleteBatch(unsigned batch_size,
                                        CNetScheduleClient::EJobStatus status)
{
    unsigned dcnt;
    for (dcnt = 0; dcnt < batch_size; ++dcnt) {
        unsigned job_id = m_LQueue.status_tracker.GetFirst(status);
        if (job_id == 0) {
            break;
        }
        bool deleted = CheckDelete(job_id);
        if (!deleted) {
            break;
        }
    } // for
    return dcnt;
}

void CQueueDataBase::CQueue::ClearAffinityIdx()
{
    // read the queue database, find the first phisically available job_id,
    // then scan all index records, delete all the old (obsolete) record 
    // (job_id) references

    SQueueDB& db = m_LQueue.db;

    unsigned first_job_id = 0;
    {{
    CFastMutexGuard guard(m_LQueue.lock);
    CBDB_Transaction trans(*db.GetEnv(), 
                           CBDB_Transaction::eTransASync,
                           CBDB_Transaction::eNoAssociation);

    db.SetTransaction(&trans);

    // get the first phisically available record in the queue database

    CBDB_FileCursor& cur = *GetCursor(trans);
    CBDB_CursorGuard cg(cur);
    cur.SetCondition(CBDB_FileCursor::eGE);
    cur.From << 0;

    if (cur.Fetch() == eBDB_Ok) {
        first_job_id = db.id;
    }
    }}

    if (first_job_id <= 1) {
        return;
    }


    bm::bvector<> bv(bm::BM_GAP);

    // get list of all affinity tokens in the index
    {{
    CFastMutexGuard guard(m_LQueue.lock);
    CBDB_Transaction trans(*db.GetEnv(), 
                           CBDB_Transaction::eTransASync,
                           CBDB_Transaction::eNoAssociation);
    m_LQueue.aff_idx.SetTransaction(&trans);
    CBDB_FileCursor cur(m_LQueue.aff_idx);
    cur.SetCondition(CBDB_FileCursor::eGE);
    while (cur.Fetch() == eBDB_Ok) {
        unsigned aff_id = m_LQueue.aff_idx.aff_id;
        bv.set(aff_id);
    }
    }}

    // clear all "hanging references
    bm::bvector<>::enumerator en(bv.first());
    for(; en.valid(); ++en) {
        unsigned aff_id = *en;
        {{
        CFastMutexGuard guard(m_LQueue.lock);
        CBDB_Transaction trans(*db.GetEnv(), 
                                CBDB_Transaction::eTransASync,
                                CBDB_Transaction::eNoAssociation);
        m_LQueue.aff_idx.SetTransaction(&trans);
        SQueueAffinityIdx::TParent::TBitVector bvect(bm::BM_GAP);
        m_LQueue.aff_idx.aff_id = aff_id;
        /*EBDB_ErrCode ret = */
            m_LQueue.aff_idx.ReadVectorOr(&bvect);
        bvect.set_range(0, first_job_id-1, false);
        bvect.optimize();
        m_LQueue.aff_idx.aff_id = aff_id;
        if (bvect.any()) {
            m_LQueue.aff_idx.WriteVector(bvect, SQueueAffinityIdx::eNoCompact);
        } else {
            m_LQueue.aff_idx.Delete();
        }
        trans.Commit();
        }}
    } // for

}


void CQueueDataBase::CQueue::Truncate()
{
    SQueueDB& db = m_LQueue.db;
    
    CNetScheduler_JobStatusTracker::TBVector del_bv(bm::BM_GAP);
    
    m_LQueue.status_tracker.ClearAll(&del_bv);
    if (del_bv.none()) {
        return;
    }

    CFastMutexGuard guard(m_LQueue.lock);

    BDB_batch_delete_recs(db, del_bv.first(), del_bv.end(), 
                         100, (CFastMutex*)(0));

    // control shot
    CBDB_Transaction trans(*db.GetEnv(), 
                           CBDB_Transaction::eTransASync);
    db.SetTransaction(&trans);
    db.Truncate();
    trans.Commit();
}

void CQueueDataBase::CQueue::RemoveFromTimeLine(unsigned job_id)
{
    if (m_LQueue.run_time_line) {
        CWriteLockGuard guard(m_LQueue.rtl_lock);
        m_LQueue.run_time_line->RemoveObject(job_id);
    }

    if (m_LQueue.monitor.IsMonitorActive()) {
        CTime tmp_t(CTime::eCurrent);
        string msg = tmp_t.AsString();
        msg += " CQueue::RemoveFromTimeLine: job id=";
        msg += NStr::IntToString(job_id);
        m_LQueue.monitor.SendString(msg);
    }
}

void 
CQueueDataBase::CQueue::TimeLineExchange(unsigned remove_job_id, 
                                         unsigned add_job_id,
                                         time_t   curr)
{
    if (m_LQueue.run_time_line) {
        CWriteLockGuard guard(m_LQueue.rtl_lock);
        m_LQueue.run_time_line->RemoveObject(remove_job_id);

        if (add_job_id) {
            CJobTimeLine& tl = *m_LQueue.run_time_line;
            time_t projected_time_done = curr + m_LQueue.run_timeout;
            tl.AddObject(projected_time_done, add_job_id);
        }
    }
    if (m_LQueue.monitor.IsMonitorActive()) {
        CTime tmp_t(CTime::eCurrent);
        string msg = tmp_t.AsString();
        msg += " CQueue::TimeLineExchange: job removed=";
        msg += NStr::IntToString(remove_job_id);
        msg += " job added=";
        msg += NStr::IntToString(add_job_id);
        m_LQueue.monitor.SendString(msg);
    }
}

SLockedQueue::TListenerList::iterator
CQueueDataBase::CQueue::FindListener(
                                unsigned int    host_addr, 
                                unsigned short  udp_port)
{
    SLockedQueue::TListenerList&  wnodes = m_LQueue.wnodes;
    CReadLockGuard guard(m_LQueue.wn_lock);
    NON_CONST_ITERATE(SLockedQueue::TListenerList, it, wnodes) {
        const SQueueListener& ql = *(*it);
        if ((ql.udp_port == udp_port) &&
            (ql.host == host_addr)
            ) {
                return it;
        }
    }
    return wnodes.end();
}


void CQueueDataBase::CQueue::RegisterNotificationListener(
                                            unsigned int    host_addr,
                                            unsigned short  udp_port,
                                            int             timeout,
                                            const string&   auth)
{
    SLockedQueue::TListenerList::iterator it =
                                FindListener(host_addr, udp_port);
    
    time_t curr = time(0);
    CWriteLockGuard guard(m_LQueue.wn_lock);
    if (it != m_LQueue.wnodes.end()) {  // update registration timestamp
        SQueueListener& ql = *(*it);
        if ((ql.timeout = timeout)!= 0) {
            ql.last_connect = curr;
            ql.auth = auth;
        } else {
        }
        return;
    }

    // new element

    if (timeout) {
        m_LQueue.wnodes.push_back(
            new SQueueListener(host_addr, udp_port, curr, timeout, auth));
    }
}

void 
CQueueDataBase::CQueue::UnRegisterNotificationListener(
                                            unsigned int    host_addr,
                                            unsigned short  udp_port)
{
    SLockedQueue::TListenerList::iterator it =
                                FindListener(host_addr, udp_port);
   CWriteLockGuard guard(m_LQueue.wn_lock);
   if (it != m_LQueue.wnodes.end()) {
        SQueueListener* node = *it;
        delete node;
        m_LQueue.wnodes.erase(it);
   }
}

void CQueueDataBase::CQueue::ClearAffinity(unsigned int  host_addr,
                                           const string& auth)
{
    CFastMutexGuard guard(m_LQueue.aff_map_lock);
    m_LQueue.worker_aff_map.ClearAffinity(host_addr, auth);
}

void CQueueDataBase::CQueue::SetMonitorSocket(SOCK sock)
{
    auto_ptr<CSocket> s(new CSocket());
    s->Reset(sock, eTakeOwnership, eCopyTimeoutsFromSOCK);
    m_LQueue.monitor.SetSocket(s.get());
    s.release();
}


void CQueueDataBase::CQueue::NotifyListeners()
{
    unsigned short udp_port = m_Db.GetUdpPort();
    if (udp_port == 0) {
        return;
    }

    SLockedQueue::TListenerList&  wnodes = m_LQueue.wnodes;

    time_t curr = time(0);

    if (m_LQueue.notif_timeout == 0 ||
        !m_LQueue.status_tracker.AnyPending()) {
        return;
    }


    SLockedQueue::TListenerList::size_type lst_size;

    {{
        CWriteLockGuard guard(m_LQueue.wn_lock);
        lst_size = wnodes.size();
        if ((lst_size == 0) ||
            (m_LQueue.last_notif + m_LQueue.notif_timeout > curr)) {
            return;
        } 
        m_LQueue.last_notif = curr;
    }}

    const char* msg = m_LQueue.q_notif.c_str();
    size_t msg_len = m_LQueue.q_notif.length()+1;
    
    for (SLockedQueue::TListenerList::size_type i = 0; 
         i < lst_size; 
         ++i) 
    {
        unsigned host;
        unsigned short port;
        SQueueListener* ql;
        {{
            CReadLockGuard guard(m_LQueue.wn_lock);
            ql = wnodes[i];
            if (ql->last_connect + ql->timeout < curr) {
                continue;
            }
        }}
        host = ql->host;
        port = ql->udp_port;

        if (port) {
            CFastMutexGuard guard(m_LQueue.us_lock);
            //EIO_Status status = 
                m_LQueue.udp_socket.Send(msg, msg_len, 
                                     CSocketAPI::ntoa(host), port);
        }
        // periodically check if we have no more jobs left
        if ((i % 10 == 0) &&
            !m_LQueue.status_tracker.AnyPending()) {
            break;
        }

    }
}

void CQueueDataBase::CQueue::CheckExecutionTimeout()
{
    if (m_LQueue.run_time_line == 0) {
        return;
    }
    CJobTimeLine& tl = *m_LQueue.run_time_line;
    time_t curr = time(0);
    unsigned curr_slot;
    {{
        CReadLockGuard guard(m_LQueue.rtl_lock);
        curr_slot = tl.TimeLineSlot(curr);
    }}
    if (curr_slot == 0) {
        return;
    }
    --curr_slot;

    CJobTimeLine::TObjVector bv;
    {{
        CReadLockGuard guard(m_LQueue.rtl_lock);
        tl.EnumerateObjects(&bv, curr_slot);
    }}
    CJobTimeLine::TObjVector::enumerator en(bv.first());
    for (;en.valid(); ++en) {
        unsigned job_id = *en;
        unsigned exp_time = CheckExecutionTimeout(job_id, curr_slot);

        // job may need to moved in the timeline to some future slot
        
        if (exp_time) {
            CWriteLockGuard guard(m_LQueue.rtl_lock);
            unsigned job_slot = tl.TimeLineSlot(exp_time);
            if (job_slot <= curr_slot) {
                ++job_slot;
            }
            tl.AddObjectToSlot(job_slot, job_id);
        }
    } // for

    CWriteLockGuard guard(m_LQueue.rtl_lock);
    tl.HeadTruncate(curr_slot);
}

time_t CQueueDataBase::CQueue::CheckExecutionTimeout(unsigned job_id,
                                                   time_t   curr_time)
{
    SQueueDB& db = m_LQueue.db;

    CNetScheduleClient::EJobStatus status = GetStatus(job_id);

    if (status != CNetScheduleClient::eRunning) {
        return 0;
    }

    CBDB_Transaction trans(*db.GetEnv(), 
                        CBDB_Transaction::eTransASync,
                        CBDB_Transaction::eNoAssociation);

    // TODO: get current job status from the status index
    unsigned time_run, run_timeout;
    time_t   exp_time;
    {{
    CFastMutexGuard guard(m_LQueue.lock);
    db.SetTransaction(&trans);

    CBDB_FileCursor& cur = *GetCursor(trans);
    CBDB_CursorGuard cg(cur);

    cur.SetCondition(CBDB_FileCursor::eEQ);
    cur.From << job_id;
    if (cur.Fetch() != eBDB_Ok) {
        return 0;
    }
    int status = db.status;
    if (status != (int)CNetScheduleClient::eRunning) {
        return 0;
    }

    time_run = db.time_run;
    run_timeout = db.run_timeout;
/*
    if (run_timeout == 0) {
        run_timeout = m_LQueue.run_timeout;
    }
    if (run_timeout == 0) {
        return 0;
    }
*/
    exp_time = x_ComputeExpirationTime(time_run, run_timeout);
    if (!(curr_time < exp_time)) { 
        return exp_time;
    }
    db.status = (int) CNetScheduleClient::ePending;
    db.time_done = 0;

    cur.Update();

    }}

    trans.Commit();

    m_LQueue.status_tracker.SetStatus(job_id, CNetScheduleClient::eReturned);

    if (m_LQueue.monitor.IsMonitorActive()) {
        CTime tm(CTime::eCurrent);
        string msg = tm.AsString();
        msg += " CQueue::CheckExecutionTimeout: Job rescheduled id=";
        msg += NStr::IntToString(job_id);
        tm.SetTimeT(time_run);
        tm.ToLocalTime();
        msg += " time_run=";
        msg += tm.AsString();

        tm.SetTimeT(exp_time);
        tm.ToLocalTime();
        msg += " exp_time=";
        msg += tm.AsString();

        msg += " run_timeout(sec)=";
        msg += NStr::IntToString(run_timeout);
        msg += " run_timeout(minutes)=";
        msg += NStr::IntToString(run_timeout/60);

        m_LQueue.monitor.SendString(msg);
    }

    return 0;
}

time_t 
CQueueDataBase::CQueue::x_ComputeExpirationTime(unsigned time_run, 
                                                unsigned run_timeout) const
{
    if (run_timeout == 0) {
        run_timeout = m_LQueue.run_timeout;
    }
    if (run_timeout == 0) {
        return 0;
    }
    time_t exp_time = time_run + run_timeout;
    return exp_time;
}

void CQueueDataBase::CQueue::x_AddToAffIdx_NoLock(unsigned aff_id, 
                                                  unsigned job_id_from,
                                                  unsigned job_id_to)

{
    SQueueAffinityIdx::TParent::TBitVector bv(bm::BM_GAP);

    // check if vector is in the database

    // read vector from the file
    m_LQueue.aff_idx.aff_id = aff_id;
    /*EBDB_ErrCode ret = */m_LQueue.aff_idx.ReadVectorOr(&bv);
    if (job_id_to == 0) {
        bv.set(job_id_from);
    } else {
        bv.set_range(job_id_from, job_id_to);
    }
    m_LQueue.aff_idx.aff_id = aff_id;
    m_LQueue.aff_idx.WriteVector(bv, SQueueAffinityIdx::eNoCompact);
}

void CQueueDataBase::CQueue::x_AddToAffIdx_NoLock(
                                    const vector<SNS_BatchSubmitRec>& batch)
{
    typedef SQueueAffinityIdx::TParent::TBitVector TBVector;
    typedef map<unsigned, TBVector*>               TBVMap;
    
    TBVMap  bv_map;
    try {

        unsigned bsize = batch.size();
        for (unsigned i = 0; i < bsize; ++i) {
            const SNS_BatchSubmitRec& bsub = batch[i];
            unsigned aff_id = bsub.affinity_id;
            unsigned job_id_start = bsub.job_id;

            TBVector* aff_bv;

            TBVMap::iterator aff_it = bv_map.find(aff_id);
            if (aff_it == bv_map.end()) { // new element
                auto_ptr<TBVector> bv(new TBVector(bm::BM_GAP));
                m_LQueue.aff_idx.aff_id = aff_id;
                /*EBDB_ErrCode ret = */
                    m_LQueue.aff_idx.ReadVectorOr(bv.get());
                aff_bv = bv.get();
                bv_map[aff_id] = bv.release();
            } else {
                aff_bv = aff_it->second;
            }


            // look ahead for the same affinity id
            unsigned j;
            for (j=i+1; j < bsize; ++j) {
                if (batch[j].affinity_id != aff_id) {
                    break;
                }
                _ASSERT(batch[j].job_id == (batch[j-1].job_id+1));
                //job_id_end = batch[j].job_id;
            }
            --j;

            if ((i!=j) && (aff_id == batch[j].affinity_id)) {
                unsigned job_id_end = batch[j].job_id;
                aff_bv->set_range(job_id_start, job_id_end);
                i = j;
            } else { // look ahead failed
                aff_bv->set(job_id_start);
            }

        } // for

        // save all changes to the database
        NON_CONST_ITERATE(TBVMap, it, bv_map) {
            unsigned aff_id = it->first;
            TBVector* bv = it->second;
            bv->optimize();

            m_LQueue.aff_idx.aff_id = aff_id;
            m_LQueue.aff_idx.WriteVector(*bv, SQueueAffinityIdx::eNoCompact);

            delete it->second; it->second = 0;
        }
    } 
    catch (exception& )
    {
        NON_CONST_ITERATE(TBVMap, it, bv_map) {
            delete it->second; it->second = 0;
        }
        throw;
    }

}

void 
CQueueDataBase::CQueue::x_ReadAffIdx_NoLock(
                        const bm::bvector<>& aff_id_set,
                        bm::bvector<>*       job_candidates)
{
    m_LQueue.aff_idx.SetTransaction(0);
    bm::bvector<>::enumerator en(aff_id_set.first());
    for(; en.valid(); ++en) {  // for each affinity id
        unsigned aff_id = *en;
        m_LQueue.aff_idx.aff_id = aff_id;
        m_LQueue.aff_idx.ReadVectorOr(job_candidates);
    }
}



END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.75  2006/05/04 15:36:03  kuznets
 * Fixed bug in deleting done jobs
 *
 * Revision 1.74  2006/05/03 15:18:32  kuznets
 * Fixed deletion of done jobs
 *
 * Revision 1.73  2006/04/17 15:46:54  kuznets
 * Added option to remove job when it is done (similar to LSF)
 *
 * Revision 1.72  2006/04/14 12:43:28  kuznets
 * Fixed crash when deleting affinity records
 *
 * Revision 1.71  2006/03/30 20:54:23  kuznets
 * Improved handling of BDB resource conflicts
 *
 * Revision 1.70  2006/03/30 17:38:55  kuznets
 * Set max. transactions according to number of active threads
 *
 * Revision 1.69  2006/03/30 16:12:40  didenko
 * Increased the max_dead_locks number
 *
 * Revision 1.68  2006/03/29 17:39:42  kuznets
 * Turn off reverse splitting for main queue file to reduce collisions
 *
 * Revision 1.67  2006/03/28 21:41:17  kuznets
 * Use default page size.Hope smaller p-size reduce collisions
 *
 * Revision 1.66  2006/03/28 21:21:22  kuznets
 * cleaned up comments
 *
 * Revision 1.65  2006/03/28 20:37:56  kuznets
 * Trying to work around deadlock by repeating transaction
 *
 * Revision 1.64  2006/03/28 19:35:17  kuznets
 * Commented out use of local database to fix dead lock
 *
 * Revision 1.63  2006/03/17 14:40:25  kuznets
 * fixed warning
 *
 * Revision 1.62  2006/03/17 14:25:29  kuznets
 * Force reschedule (to re-try failed jobs)
 *
 * Revision 1.61  2006/03/16 19:37:28  kuznets
 * Fixed possible race condition between client and worker
 *
 * Revision 1.60  2006/03/13 16:01:36  kuznets
 * Fixed queue truncation (transaction log overflow). Added commands to print queue selectively
 *
 * Revision 1.59  2006/02/23 20:05:10  kuznets
 * Added grid client registration-unregistration
 *
 * Revision 1.58  2006/02/23 15:45:04  kuznets
 * Added more frequent and non-intrusive memory optimization of status matrix
 *
 * Revision 1.57  2006/02/21 14:44:57  kuznets
 * Bug fixes, improvements in statistics
 *
 * Revision 1.56  2006/02/09 17:07:42  kuznets
 * Various improvements in job scheduling with respect to affinity
 *
 * Revision 1.55  2006/02/08 15:17:33  kuznets
 * Tuning and bug fixing of job affinity
 *
 * Revision 1.54  2006/02/06 14:10:29  kuznets
 * Added job affinity
 *
 * Revision 1.53  2005/11/01 15:16:14  kuznets
 * Cleaned unused variables
 *
 * Revision 1.52  2005/09/20 14:05:25  kuznets
 * Minor bug fix
 *
 * Revision 1.51  2005/08/30 14:19:33  kuznets
 * Added thread-local database for better scalability
 *
 * Revision 1.50  2005/08/29 12:10:18  kuznets
 * Removed dead code
 *
 * Revision 1.49  2005/08/26 12:36:10  kuznets
 * Performance optimization
 *
 * Revision 1.48  2005/08/25 16:35:01  kuznets
 * Fixed bug (uncommited transaction)
 *
 * Revision 1.47  2005/08/24 18:17:26  kuznets
 * Reduced priority of notifiction thread, increased Tas spins
 *
 * Revision 1.46  2005/08/22 14:01:58  kuznets
 * Added JobExchange command
 *
 * Revision 1.45  2005/08/15 13:29:46  kuznets
 * Implemented batch job submission
 *
 * Revision 1.44  2005/07/25 16:14:31  kuznets
 * Revisited LB parameters, added options to compute job stall delay as fraction of AVG runtime
 *
 * Revision 1.43  2005/07/21 15:41:02  kuznets
 * Added monitoring for LB info
 *
 * Revision 1.42  2005/07/21 12:39:27  kuznets
 * Improved load balancing module
 *
 * Revision 1.41  2005/07/14 13:40:07  kuznets
 * compilation bug fixes
 *
 * Revision 1.40  2005/07/14 13:12:56  kuznets
 * Added load balancer
 *
 * Revision 1.39  2005/06/22 15:36:09  kuznets
 * Minor tweaks in record print formatting
 *
 * Revision 1.38  2005/06/21 16:00:22  kuznets
 * Added archival dump of all deleted records
 *
 * Revision 1.37  2005/06/20 15:49:43  kuznets
 * Node statistics: do not show obsolete connections
 *
 * Revision 1.36  2005/06/20 13:31:08  kuznets
 * Added access control for job submitters and worker nodes
 *
 * Revision 1.35  2005/05/12 18:37:33  kuznets
 * Implemented config reload
 *
 * Revision 1.34  2005/05/06 13:07:32  kuznets
 * Fixed bug in cleaning database
 *
 * Revision 1.33  2005/05/05 16:52:41  kuznets
 * Added error messages when job expires
 *
 * Revision 1.32  2005/05/05 16:07:15  kuznets
 * Added individual job dumping
 *
 * Revision 1.31  2005/05/04 19:09:43  kuznets
 * Added queue dumping
 *
 * Revision 1.30  2005/05/02 14:44:40  kuznets
 * Implemented remote monitoring
 *
 * Revision 1.29  2005/04/28 17:40:26  kuznets
 * Added functions to rack down forgotten nodes
 *
 * Revision 1.28  2005/04/25 14:42:53  kuznets
 * Fixed bug in GetJob()
 *
 * Revision 1.27  2005/04/21 13:37:35  kuznets
 * Fixed race condition between Submit job and Get job
 *
 * Revision 1.26  2005/04/20 15:59:33  kuznets
 * Progress message to Submit
 *
 * Revision 1.25  2005/04/19 19:34:05  kuznets
 * Adde progress report messages
 *
 * Revision 1.24  2005/04/11 13:53:25  kuznets
 * Code cleanup
 *
 * Revision 1.23  2005/04/06 12:39:55  kuznets
 * + client version control
 *
 * Revision 1.22  2005/03/29 16:48:28  kuznets
 * Use atomic counter for job id assignment
 *
 * Revision 1.21  2005/03/22 19:02:54  kuznets
 * Reflecting chnages in connect layout
 *
 * Revision 1.20  2005/03/22 16:14:49  kuznets
 * +PrintStat()
 *
 * Revision 1.19  2005/03/21 13:07:28  kuznets
 * Added some statistical functions
 *
 * Revision 1.18  2005/03/17 20:37:07  kuznets
 * Implemented FPUT
 *
 * Revision 1.17  2005/03/17 16:04:38  kuznets
 * Get job algorithm improved to re-get if job expired
 *
 * ===========================================================================
 */
