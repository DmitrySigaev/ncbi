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
 * Authors:  Anatoliy Kuznetsov, Victor Joukov
 *
 * File Description: NetSchedule queue collection and database managenement.
 *
 */
#include <ncbi_pch.hpp>

#include <corelib/ncbi_system.hpp> // SleepMilliSec
#include <corelib/ncbireg.hpp>

#include <connect/services/netschedule_api.hpp>
#include <connect/ncbi_socket.hpp>

#include <db.h>
#include <db/bdb/bdb_trans.hpp>
#include <db/bdb/bdb_cursor.hpp>
#include <db/bdb/bdb_util.hpp>

#include <util/time_line.hpp>

#include "queue_coll.hpp"
#include "ns_util.hpp"
#include "netschedule_version.hpp"
#include "ns_server.hpp"


BEGIN_NCBI_SCOPE

/////////////////////////////////////////////////////////////////////////////
// CQueueCollection implementation
CQueueCollection::CQueueCollection(const CQueueDataBase& db) :
    m_QueueDataBase(db)
{}


CQueueCollection::~CQueueCollection()
{}


void CQueueCollection::Close()
{
    CWriteLockGuard guard(m_Lock);
    m_QMap.clear();
}


CRef<CQueue> CQueueCollection::GetQueue(const string& name) const
{
    CReadLockGuard              guard(m_Lock);
    TQueueMap::const_iterator   it = m_QMap.find(name);
    if (it == m_QMap.end())
        NCBI_THROW(CNetScheduleException, eUnknownQueue,
                   "Job queue not found: " + name);
    return it->second;
}


bool CQueueCollection::QueueExists(const string& name) const
{
    CReadLockGuard guard(m_Lock);
    return (m_QMap.find(name) != m_QMap.end());
}


CQueue& CQueueCollection::AddQueue(const string& name,
                                   CQueue*       queue)
{
    CWriteLockGuard guard(m_Lock);
    m_QMap[name] = queue;
    return *queue;
}


bool CQueueCollection::RemoveQueue(const string& name)
{
    CWriteLockGuard guard(m_Lock);
    TQueueMap::iterator it = m_QMap.find(name);
    if (it == m_QMap.end()) return false;
    m_QMap.erase(it);
    return true;
}


// FIXME: remove this arcane construct, CQueue now has all
// access methods required by ITERATE(CQueueCollection, it, m_QueueCollection)
CQueueIterator CQueueCollection::begin()
{
    return CQueueIterator(m_QMap.begin(), &m_Lock);
}


CQueueIterator CQueueCollection::end()
{
    return CQueueIterator(m_QMap.end(), NULL);
}


CQueueConstIterator CQueueCollection::begin() const
{
    return CQueueConstIterator(m_QMap.begin(), &m_Lock);
}


CQueueConstIterator CQueueCollection::end() const
{
    return CQueueConstIterator(m_QMap.end(), NULL);
}


/////////////////////////////////////////////////////////////////////////////
// SNSDBEnvironmentParams implementation

bool SNSDBEnvironmentParams::Read(const IRegistry& reg, const string& sname)
{
    CConfig conf(reg);
    const CConfig::TParamTree* param_tree = conf.GetTree();
    const TPluginManagerParamTree* bdb_tree =
        param_tree->FindSubNode(sname);

    if (!bdb_tree)
        return false;

    CConfig bdb_conf((CConfig::TParamTree*)bdb_tree, eNoOwnership);
    db_storage_ver = bdb_conf.GetString("netschedule", "force_storage_version",
        CConfig::eErr_NoThrow, NETSCHEDULED_STORAGE_VERSION);

    db_path = bdb_conf.GetString("netschedule", "path", CConfig::eErr_Throw);
    db_log_path = ""; // doesn't work yet
        // bdb_conf.GetString("netschedule", "transaction_log_path",
        // CConfig::eErr_NoThrow, "");
#define GetUIntNoErr(name, dflt) \
    (unsigned) bdb_conf.GetInt("netschedule", name, CConfig::eErr_NoThrow, dflt)
#define GetSizeNoErr(name, dflt) \
    (unsigned) bdb_conf.GetDataSize("netschedule", name, CConfig::eErr_NoThrow, dflt)
#define GetBoolNoErr(name, dflt) \
    bdb_conf.GetBool("netschedule", name, CConfig::eErr_NoThrow, dflt)

    max_queues        = GetUIntNoErr("max_queues", 50);

    cache_ram_size    = GetSizeNoErr("mem_size", 0);
    mutex_max         = GetUIntNoErr("mutex_max", 0);
    max_locks         = GetUIntNoErr("max_locks", 0);
    max_lockers       = GetUIntNoErr("max_lockers", 0);
    max_lockobjects   = GetUIntNoErr("max_lockobjects", 0);
    log_mem_size      = GetSizeNoErr("log_mem_size", 0);
    // max_trans is derivative, so we do not read it here

    checkpoint_kb     = GetUIntNoErr("checkpoint_kb", 5000);
    checkpoint_min    = GetUIntNoErr("checkpoint_min", 5);

    sync_transactions = GetBoolNoErr("sync_transactions", false);
    direct_db         = GetBoolNoErr("direct_db", false);
    direct_log        = GetBoolNoErr("direct_log", false);
    private_env       = GetBoolNoErr("private_env", false);
    return true;
}


unsigned SNSDBEnvironmentParams::GetNumParams() const
{
    return 15; // do not count max_trans
}


string SNSDBEnvironmentParams::GetParamName(unsigned n) const
{
    switch (n) {
    case 0:  return "path";
    case 1:  return "transaction_log_path";
    case 2:  return "max_queues";
    case 3:  return "mem_size";
    case 4:  return "mutex_max";
    case 5:  return "max_locks";
    case 6:  return "max_lockers";
    case 7:  return "max_lockobjects";
    case 8:  return "log_mem_size";
    case 9:  return "checkpoint_kb";
    case 10: return "checkpoint_min";
    case 11: return "sync_transactions";
    case 12: return "direct_db";
    case 13: return "direct_log";
    case 14: return "private_env";
    default: return "";
    }
}


string SNSDBEnvironmentParams::GetParamValue(unsigned n) const
{
    switch (n) {
    case 0:  return db_path;
    case 1:  return db_log_path;
    case 2:  return NStr::UIntToString(max_queues);
    case 3:  return NStr::UIntToString(cache_ram_size);
    case 4:  return NStr::UIntToString(mutex_max);
    case 5:  return NStr::UIntToString(max_locks);
    case 6:  return NStr::UIntToString(max_lockers);
    case 7:  return NStr::UIntToString(max_lockobjects);
    case 8:  return NStr::UIntToString(log_mem_size);
    case 9:  return NStr::UIntToString(checkpoint_kb);
    case 10: return NStr::UIntToString(checkpoint_min);
    case 11: return NStr::BoolToString(sync_transactions);
    case 12: return NStr::BoolToString(direct_db);
    case 13: return NStr::BoolToString(direct_log);
    case 14: return NStr::BoolToString(private_env);
    default: return "";
    }
}

/////////////////////////////////////////////////////////////////////////////
// CQueueDataBase implementation

CQueueDataBase::CQueueDataBase(CNetScheduleServer *  server)
: m_Host(server->GetBackgroundHost()),
  m_Executor(server->GetRequestExecutor()),
  m_Env(0),
  m_QueueCollection(*this),
  m_StopPurge(false),
  m_FreeStatusMemCnt(0),
  m_LastFreeMem(time(0)),
  m_UdpPort(0),
  m_Server(server)
{}


CQueueDataBase::~CQueueDataBase()
{
    try {
        Close();
    } catch (exception& )
    {}
}


bool CQueueDataBase::Open(const SNSDBEnvironmentParams& params,
                          bool reinit)
{
    const string& db_path     = params.db_path;
    const string& db_log_path = params.db_log_path;
    if (reinit) {
        CDir dir(db_path);
        dir.Remove();
        LOG_POST(Error << "Reinitialization. " << db_path
                        << " removed.");
    }

    m_Path = CDirEntry::AddTrailingPathSeparator(db_path);
    string trailed_log_path = CDirEntry::AddTrailingPathSeparator(db_log_path);

    const string* effective_log_path = trailed_log_path.empty() ?
                                       &m_Path :
                                       &trailed_log_path;

    bool fresh_db(false);
    {{
        CDir dir(m_Path);
        if ( !dir.Exists() ) {
            fresh_db = true;
            dir.Create();
        }
    }}

    string db_storage_ver;
    CFile ver_file(CFile::MakePath(m_Path, "DB_STORAGE_VER"));
    if (fresh_db) {
        db_storage_ver = NETSCHEDULED_STORAGE_VERSION;
        CFileIO f;
        f.Open(ver_file.GetPath(), CFileIO_Base::eCreate, CFileIO_Base::eReadWrite);
        f.Write(db_storage_ver.data(), db_storage_ver.size());
        f.Close();
    } else {
        if (!ver_file.Exists()) {
            // Last storage version which does not have DB_STORAGE_VER file
            db_storage_ver = "4.0.0";
        } else {
            CFileIO f;
            char buf[32];
            f.Open(ver_file.GetPath(), CFileIO_Base::eOpen, CFileIO_Base::eRead);
            size_t n = f.Read(buf, sizeof(buf));
            db_storage_ver.append(buf, n);
            NStr::TruncateSpacesInPlace(db_storage_ver, NStr::eTrunc_End);
            f.Close();
        }
        if (db_storage_ver != params.db_storage_ver) {
            ERR_POST("Storage version mismatch, required " <<
                     params.db_storage_ver <<
                     " present " <<
                     db_storage_ver);
            return false;
        }
    }

    delete m_Env;
    m_Env = new CBDB_Env();

    if (!trailed_log_path.empty()) {
        m_Env->SetLogDir(trailed_log_path);
    }

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

    m_Env->SetLogRegionMax(512 * 1024);
    if (params.log_mem_size) {
        m_Env->SetLogInMemory(true);
        m_Env->SetLogBSize(params.log_mem_size);
    } else {
        m_Env->SetLogFileMax(200 * 1024 * 1024);
        m_Env->SetLogAutoRemove(true);
    }

    // Check if bdb env. files are in place and try to join
    CDir dir(*effective_log_path);
    CDir::TEntries fl = dir.GetEntries("__db.*", CDir::eIgnoreRecursive);

    if (!params.private_env && !fl.empty()) {
        // Opening db with recover flags is unreliable.
        BDB_RecoverEnv(m_Path, false);
        NcbiCout << "Running recovery..." << NcbiEndl;
    }

    CBDB_Env::TEnvOpenFlags opt = CBDB_Env::eThreaded;
    if (params.private_env)
        opt |= CBDB_Env::ePrivate;

    if (params.cache_ram_size)
        m_Env->SetCacheSize(params.cache_ram_size);
    if (params.mutex_max)
        m_Env->MutexSetMax(params.mutex_max);
    if (params.max_locks)
        m_Env->SetMaxLocks(params.max_locks);
    if (params.max_lockers)
        m_Env->SetMaxLockers(params.max_lockers);
    if (params.max_lockobjects)
        m_Env->SetMaxLockObjects(params.max_lockobjects);
    if (params.max_trans)
        m_Env->SetTransactionMax(params.max_trans);
    m_Env->SetTransactionSync(params.sync_transactions ?
                                  CBDB_Transaction::eTransSync :
                                  CBDB_Transaction::eTransASync);

    m_Env->OpenWithTrans(m_Path.c_str(), opt);
    LOG_POST(Info << "Opened " << (params.private_env ? "private " : "")
        << "BDB environment with "
        << m_Env->GetMaxLocks() << " max locks, "
//            << m_Env->GetMaxLockers() << " max lockers, "
//            << m_Env->GetMaxLockObjects() << " max lock objects, "
        << (m_Env->GetTransactionSync() == CBDB_Transaction::eTransSync ?
                                        "" : "a") << "syncronous transactions, "
        << m_Env->MutexGetMax() <<  " max mutexes");


    m_Env->SetDirectDB(params.direct_db);
    m_Env->SetDirectLog(params.direct_log);

    m_Env->SetCheckPointKB(params.checkpoint_kb);
    m_Env->SetCheckPointMin(params.checkpoint_min);

    m_Env->SetLockTimeout(10 * 1000000); // 10 sec

    m_Env->SetTasSpins(5);

    if (m_Env->IsTransactional()) {
        m_Env->SetTransactionTimeout(10 * 1000000); // 10 sec
        m_Env->ForceTransactionCheckpoint();
        m_Env->CleanLog();
    }

    m_QueueDescriptionDB.SetEnv(*m_Env);
    m_QueueDescriptionDB.Open("sys_qdescr.db", CBDB_RawFile::eReadWriteCreate);

    // Allocate SQueueDbBlock's here, open/create corresponding databases
    m_QueueDbBlockArray.Init(*m_Env, m_Path, params.max_queues);
    return true;
}


int CQueueDataBase::x_AllocateQueue(const string& qname, const string& qclass,
                                    int kind, const string& comment)
{
    int pos = m_QueueDbBlockArray.Allocate();
    if (pos < 0) return pos;
    m_QueueDescriptionDB.queue   = qname;
    m_QueueDescriptionDB.kind    = kind;
    m_QueueDescriptionDB.pos     = pos;
    m_QueueDescriptionDB.qclass  = qclass;
    m_QueueDescriptionDB.comment = comment;
    m_QueueDescriptionDB.UpdateInsert();
    return pos;
}


struct SQueueInfo
{
    int    kind;
    int    pos;
    bool   remove; ///< if this queue has a conflict and should be removed?
    string qclass;
};

typedef map<string, SQueueInfo> TDbQueuesMap;

// Add queue to interim map. If the queue with the same name is already
// present, check for consistency.
// @return true if queue was not there, or duplicate is consistent
bool x_AddQueueToMap(TDbQueuesMap& db_queues_map,
                     const string& qname,
                     const string& qclass,
                     int           kind,
                     int           pos)
{
    bool res = true;
    TDbQueuesMap::iterator it;
    if ((it = db_queues_map.find(qname)) != db_queues_map.end()) {
        // check that queue info matches
        SQueueInfo& qi = it->second;
        if (qi.qclass != qclass) {
            LOG_POST(Warning << "Class mismatch for queue '"
            << qname << "', expected '"
            << qi.qclass << "', registered '"
            << qclass << "'.");
            res = false;
        }
        // Definite positions must match, if one of positions is indefinite,
        // i.e. < 0, they are incomparable so it is not an error.
        if (qi.pos != pos  &&  qi.pos >= 0  &&  pos >= 0) {
            LOG_POST(Warning << "Position mismatch for queue '"
                << qname << "', expected "
                << qi.pos << ", registered "
                << pos << ".");
            res = false;
        }
    } else {
        db_queues_map[qname] = SQueueInfo();
        db_queues_map[qname].kind = kind;
        // Mark queue for allocation
        db_queues_map[qname].pos = pos;
        db_queues_map[qname].remove = false;
        db_queues_map[qname].qclass = qclass;
    }
    return res;
}


unsigned CQueueDataBase::Configure(const IRegistry& reg)
{
    unsigned min_run_timeout = 3600;
    bool no_default_queues =
        reg.GetBool("server", "no_default_queues", false, 0, IRegistry::eReturn);

    CFastMutexGuard guard(m_ConfigureLock);

    // Temporary storage for merging info from database and config file
    TDbQueuesMap db_queues_map;

    // Read in registered queues from database into db_queues_map
    {{
        m_QueueDescriptionDB.SetTransaction(NULL);
        CBDB_FileCursor cur(m_QueueDescriptionDB);
        cur.SetCondition(CBDB_FileCursor::eFirst);

        while (cur.Fetch() == eBDB_Ok) {
            x_AddQueueToMap(db_queues_map,
                m_QueueDescriptionDB.queue,
                m_QueueDescriptionDB.qclass,
                m_QueueDescriptionDB.kind,
                m_QueueDescriptionDB.pos);
        }
    }}

    x_CleanParamMap();

    // Merge queue data from config file into db_queues_map, filling class
    // info (m_QueueParamMap) too.
    list<string> sections;
    reg.EnumerateSections(&sections);

    ITERATE(list<string>, it, sections) {
        string qclass, tmp;
        const string& sname = *it;
        NStr::SplitInTwo(sname, "_", tmp, qclass);
        if (NStr::CompareNocase(tmp, "queue") != 0 &&
            NStr::CompareNocase(tmp, "qclass") != 0) {
            continue;
        }
        if (m_QueueParamMap.find(qclass) != m_QueueParamMap.end()) {
            LOG_POST(Warning << tmp << " section " << sname
                             << " conflicts with previous " <<
                             (NStr::CompareNocase(tmp, "queue") == 0 ?
                                 "qclass_" : "queue_") << qclass
                             << " section. Ignored.");
            continue;
        }

        // Register queue class
        SQueueParameters* params = new SQueueParameters;
        params->Read(reg, sname);
        m_QueueParamMap[qclass] = params;
        min_run_timeout =
            std::min(min_run_timeout, (unsigned)params->run_timeout_precision);

        // Compatibility with previous convention - create a queue for every
        // class, declared as queue_*
        if (!no_default_queues && NStr::CompareNocase(tmp, "queue") == 0) {
            // The queue name for this case is the same as class name
            string& qname = qclass;
            x_AddQueueToMap(db_queues_map, qname, qclass,
                            CQueue::eKindStatic, -1);
        }
    }

    list<string> queues;
    reg.EnumerateEntries("queues", &queues);
    ITERATE(list<string>, it, queues) {
        const string& qname = *it;
        string qclass = reg.GetString("queues", qname, "");
        if (!qclass.empty()  &&  
            m_QueueParamMap.find(qclass) != m_QueueParamMap.end())
        {
            x_AddQueueToMap(db_queues_map, qname, qclass,
                            CQueue::eKindStatic, -1);
        }
    }

    {{
        CNS_Transaction trans(*m_Env);
        m_QueueDescriptionDB.SetTransaction(&trans);
        // Allocate/deallocate queue db blocks according to merged info,
        // merge this info back into database.
        // NB! We don't actually remove conflicting queues from the database because
        // corrective actions in config file can be taken. We just mask queues and do
        // not attempt to load them.
        NON_CONST_ITERATE(TDbQueuesMap, it, db_queues_map) {
            const string& qname = it->first;
            SQueueInfo& qi = it->second;
            int pos = qi.pos;
            bool qexists = m_QueueCollection.QueueExists(qname);
            if (pos < 0) {
                pos = x_AllocateQueue(qname, qi.qclass, qi.kind, "");
                if (pos < 0) {
                    qi.remove = true;
                    LOG_POST(Warning << "Queue '" << qname
                        << "' can not be allocated: max_queues limit");
                }
            } else {
                if (!qexists  &&  !m_QueueDbBlockArray.Allocate(pos)) {
                    qi.remove = true;
                    LOG_POST(Warning << "Queue '" << qname
                                     << "' position conflict at block #"
                                     << pos);
                }
            }
            qi.pos = pos;
        }
        trans.Commit();
        m_QueueDescriptionDB.Sync();
        m_QueueDescriptionDB.SetTransaction(NULL);
    }}

//    CBDB_FileCursor cur(m_QueueDescriptionDB);
//    cur.SetCondition(CBDB_FileCursor::eFirst);
//    while (cur.Fetch() == eBDB_Ok) {
//        string qname(m_QueueDescriptionDB.queue);
//        int kind = m_QueueDescriptionDB.kind;
//        unsigned pos = m_QueueDescriptionDB.pos;
//        string qclass(m_QueueDescriptionDB.qclass);
    ITERATE(TDbQueuesMap, it, db_queues_map) {
        const string& qname = it->first;
        const SQueueInfo& qi = it->second;
        if (qi.remove) continue;
        int kind = qi.kind;
        int pos = qi.pos;
        const string &qclass = qi.qclass;

        TQueueParamMap::iterator it1 = m_QueueParamMap.find(qclass);
        if (it1 == m_QueueParamMap.end()) {
            LOG_POST(Warning << "Can not find class " << qclass << " for queue " << qname);
            // NB: Class (defined in config file) does not exist anymore for the already
            // loaded queue. I do not know how intelligently handle it, postpone it.
            // ??? Mark queue as dynamic, so we can delete it
//            m_QueueDescriptionDB.kind = CQueue::eKindDynamic;
//            cur.Update();
            continue;
        }
        const SQueueParameters& params = *(it1->second);
        bool qexists = m_QueueCollection.QueueExists(qname);
        if (!qexists) {
            _ASSERT(m_QueueDbBlockArray.Get(pos) != NULL);
            MountQueue(qname, qclass, kind, params, m_QueueDbBlockArray.Get(pos));
        } else {
            UpdateQueueParameters(qname, params);
        }

        // Logging queue parameters
        const char* action = qexists ? "Reconfiguring" : "Mounting";
        string sparams;
        CRef<CQueue> queue(m_QueueCollection.GetQueue(qname));
        CQueue::TParameterList parameters;
        parameters = queue->GetParameters();
        bool first = true;
        ITERATE(CQueue::TParameterList, it1, parameters) {
            if (!first) sparams += ';';
            sparams += it1->first;
            sparams += '=';
            sparams += it1->second;
            first = false;
        }
        LOG_POST(Info << action << " queue '" << qname
                                << "' of class '" << qclass
                                << "' " << sparams);
    }
    return min_run_timeout;
}


unsigned CQueueDataBase::CountActiveJobs() const
{
    unsigned cnt = 0;
    CFastMutexGuard guard(m_ConfigureLock);
    ITERATE(CQueueCollection, it, m_QueueCollection) {
        TNSBitVector bv;
        (*it).JobsWithStatus(CNetScheduleAPI::ePending, &bv);
        (*it).JobsWithStatus(CNetScheduleAPI::eRunning, &bv);
        cnt += bv.count();
    }
    return cnt;
}

    
CRef<CQueue> CQueueDataBase::OpenQueue(const string& name)
{
    return m_QueueCollection.GetQueue(name);
}


void CQueueDataBase::MountQueue(const string& qname,
                                const string& qclass,
                                TQueueKind    kind,
                                const SQueueParameters& params,
                                SQueueDbBlock* queue_db_block)
{
    _ASSERT(m_Env);

    auto_ptr<CQueue> q(new CQueue(m_Executor, qname, qclass, kind, m_Server));
    q->Attach(queue_db_block);
    q->SetParameters(params);

    CQueue& queue = m_QueueCollection.AddQueue(qname, q.release());

    unsigned recs = queue.LoadStatusMatrix();

    queue.SetPort(GetUdpPort());

    LOG_POST(Info << "Queue records = " << recs);
}


void CQueueDataBase::CreateQueue(const string& qname, const string& qclass,
                                 const string& comment)
{
    CFastMutexGuard guard(m_ConfigureLock);
    if (m_QueueCollection.QueueExists(qname)) {
        string err = string("Queue \"") + qname + "\" already exists";
        NCBI_THROW(CNetScheduleException, eDuplicateName, err);
    }
    TQueueParamMap::iterator it = m_QueueParamMap.find(qclass);
    if (it == m_QueueParamMap.end()) {
        string err = string("Can not find class \"") + qclass +
                            "\" for queue \"" + qname + "\"";
        NCBI_THROW(CNetScheduleException, eUnknownQueueClass, err);
    }
    // Find vacant position in queue block for new queue
    CNS_Transaction trans(*m_Env);
    m_QueueDescriptionDB.SetTransaction(&trans);
    int pos = x_AllocateQueue(qname, qclass,
                              CQueue::eKindDynamic, comment);

    if (pos < 0) NCBI_THROW(CNetScheduleException, eUnknownQueue,
        "Cannot allocate queue: max_queues limit");
    trans.Commit();
    m_QueueDescriptionDB.Sync();
    const SQueueParameters& params = *(it->second);
    MountQueue(qname, qclass,
               CQueue::eKindDynamic, params, m_QueueDbBlockArray.Get(pos));
}


void CQueueDataBase::DeleteQueue(const string& qname)
{
    CFastMutexGuard guard(m_ConfigureLock);
    CNS_Transaction trans(*m_Env);
    m_QueueDescriptionDB.SetTransaction(&trans);
    m_QueueDescriptionDB.queue = qname;
    if (m_QueueDescriptionDB.Fetch() != eBDB_Ok) {
        string msg = "Job queue not found: ";
        msg += qname;
        NCBI_THROW(CNetScheduleException, eUnknownQueue, msg);
    }
    int kind = m_QueueDescriptionDB.kind;
    if (!kind) {
        string msg = "Static queue \"";
        msg += qname;
        msg += "\" can not be deleted";
        NCBI_THROW(CNetScheduleException, eAccessDenied, msg);
    }
    // Signal queue to wipe out database files.
    CRef<CQueue> queue(m_QueueCollection.GetQueue(qname));
    queue->MarkForDeletion();
    // Remove it from collection
    if (!m_QueueCollection.RemoveQueue(qname)) {
        string msg = "Job queue not found: ";
        msg += qname;
        NCBI_THROW(CNetScheduleException, eUnknownQueue, msg);
    }

    // To call CQueueDbBlockArray::Free here was a deadlock error - we can't
    // reset transaction until it is executing. So we moved setting 'allocated'
    // flag to CQueue::Detach where everything is in single threaded mode,
    // and access to boolean seems to be atomic anyway.

    // Remove it from DB
    m_QueueDescriptionDB.Delete(CBDB_File::eIgnoreError);
    trans.Commit();
}


void CQueueDataBase::QueueInfo(const string& qname, int& kind,
                               string* qclass, string* comment)
{
    CFastMutexGuard guard(m_ConfigureLock);
    m_QueueDescriptionDB.SetTransaction(NULL);
    m_QueueDescriptionDB.queue = qname;
    if (m_QueueDescriptionDB.Fetch() != eBDB_Ok) {
        string msg = "Job queue not found: ";
        msg += qname;
        NCBI_THROW(CNetScheduleException, eUnknownQueue, msg);
    }
    kind = m_QueueDescriptionDB.kind;
    string qclass_str(m_QueueDescriptionDB.qclass);
    *qclass = qclass_str;
    string comment_str(m_QueueDescriptionDB.comment);
    *comment = comment_str;
}


string CQueueDataBase::GetQueueNames(const string& sep) const
{
    string names;
    ITERATE(CQueueCollection, it, m_QueueCollection) {
        names += it.GetName(); names += sep;
    }
    return names;
}

void CQueueDataBase::UpdateQueueParameters(const string& qname,
                                           const SQueueParameters& params)
{
    CRef<CQueue> queue(m_QueueCollection.GetQueue(qname));
    queue->SetParameters(params);
}

void CQueueDataBase::Close()
{
    // Check that we're still open
    if (!m_Env) return;

    StopNotifThread();
    StopPurgeThread();
    StopExecutionWatcherThread();

    m_Env->ForceTransactionCheckpoint();
    m_Env->CleanLog();

    x_CleanParamMap();

    m_QueueCollection.Close();

    // Close pre-allocated databases
    m_QueueDbBlockArray.Close();

    m_QueueDescriptionDB.Close();
    try {
        if (m_Env->CheckRemove()) {
            LOG_POST(Info    <<
                        "JS: '" <<
                        m_Name  << "' Unmounted. BDB ENV deleted.");
        } else {
            LOG_POST(Warning << "JS: '" << m_Name
                                << "' environment still in use.");
        }
    }
    catch (exception& ex) {
        LOG_POST(Warning << "JS: '" << m_Name
                         << "' Exception in Close() " << ex.what()
                         << " (ignored.)");
    }

    delete m_Env; m_Env = 0;
}


void CQueueDataBase::TransactionCheckPoint(bool clean_log)
{
    m_Env->TransactionCheckpoint();
    if (clean_log) m_Env->CleanLog();
}


void CQueueDataBase::NotifyListeners(void)
{
    NON_CONST_ITERATE(CQueueCollection, it, m_QueueCollection) {
        (*it).NotifyListeners(false, 0);
    }
}


void CQueueDataBase::CheckExecutionTimeout(void)
{
    NON_CONST_ITERATE(CQueueCollection, it, m_QueueCollection) {
        (*it).CheckExecutionTimeout();
    }
}


void CQueueDataBase::Purge(void)
{
    unsigned global_del_rec = 0;

    // Delete obsolete job records, based on time stamps
    // and expiration timeouts
    unsigned n_queue_limit = 10000; // TODO: determine this based on load

    CNetScheduleAPI::EJobStatus statuses_to_delete_from[] = {
        CNetScheduleAPI::eFailed, CNetScheduleAPI::eCanceled,
        CNetScheduleAPI::eDone, CNetScheduleAPI::ePending,
        CNetScheduleAPI::eReadFailed, CNetScheduleAPI::eConfirmed
    };
    const int kStatusesSize = sizeof(statuses_to_delete_from) /
                              sizeof(CNetScheduleAPI::EJobStatus);

    vector<string> queues_to_delete;
    NON_CONST_ITERATE(CQueueCollection, it, m_QueueCollection) {
        unsigned queue_del_rec = 0;
        const unsigned batch_size = 100;
        if ((*it).IsExpired()) {
            queues_to_delete.push_back(it.GetName());
            continue;
        }
        unsigned deleted_by_status[kStatusesSize];
        for (int n = 0; n < kStatusesSize; ++n)
            deleted_by_status[n] = 0;
        for (bool stop_flag = false; !stop_flag; ) {
            // stop if all statuses have less than batch_size jobs to delete
            stop_flag = true;
            for (int n = 0; n < kStatusesSize; ++n) {
                CNetScheduleAPI::EJobStatus s = statuses_to_delete_from[n];
                unsigned del_rec = (*it).CheckJobsExpiry(batch_size, s);
                deleted_by_status[n] += del_rec;
                stop_flag = stop_flag && (del_rec < batch_size);
                queue_del_rec += del_rec;
            }

            // do not delete more than certain number of
            // records from the queue in one Purge
            if (queue_del_rec >= n_queue_limit)
                break;

            if (x_CheckStopPurge()) return;
        }
        for (int n = 0; n < kStatusesSize; ++n) {
            CNetScheduleAPI::EJobStatus s = statuses_to_delete_from[n];
            if (deleted_by_status[n] > 0
                &&  s == CNetScheduleAPI::ePending) { // ? other states
                LOG_POST(Warning << deleted_by_status[n] <<
                    " jobs deleted from non-final state " <<
                    CNetScheduleAPI::StatusToString(s));
            }
        }
        global_del_rec += queue_del_rec;
    }

    // Delete jobs unconditionally, from internal vector.
    // This is done to spread massive job deletion in time
    // and thus smooth out peak loads
    // TODO: determine batch size based on load
    unsigned n_jobs_to_delete = 1000;
    unsigned unc_del_rec = x_PurgeUnconditional(n_jobs_to_delete);
    global_del_rec += unc_del_rec;

    // TODO: Handle tags here - see below for affinity

    // Remove unused affinity elements
    NON_CONST_ITERATE(CQueueCollection, it, m_QueueCollection) {
        (*it).ClearAffinityIdx();
    }

    TransactionCheckPoint();

    m_FreeStatusMemCnt += global_del_rec;
    x_OptimizeStatusMatrix();

    ITERATE(vector<string>, it, queues_to_delete) {
        try {
            DeleteQueue((*it));
        } catch (...) { // TODO: use more specific exception
            LOG_POST(Warning << "Queue " << (*it) << " already gone.");
        }
    }
}

unsigned CQueueDataBase::x_PurgeUnconditional(unsigned batch_size) {
    // Purge unconditional jobs
    unsigned del_rec = 0;
    NON_CONST_ITERATE(CQueueCollection, it, m_QueueCollection) {
        del_rec += (*it).DeleteBatch(batch_size);
    }
    return del_rec;
}


void CQueueDataBase::x_OptimizeStatusMatrix(void)
{
    time_t curr = time(0);
    // optimize memory every 15 min. or after 1 million of deleted records
    const int kMemFree_Delay = 15 * 60;
    const unsigned kRecordThreshold = 1000000;
    if ((m_FreeStatusMemCnt > kRecordThreshold) ||
        (m_LastFreeMem + kMemFree_Delay <= curr)) {
        m_FreeStatusMemCnt = 0;
        m_LastFreeMem = curr;

        NON_CONST_ITERATE(CQueueCollection, it, m_QueueCollection) {
            (*it).OptimizeMem();
            if (x_CheckStopPurge()) return;
        }
    }
}


void CQueueDataBase::x_CleanParamMap(void)
{
    NON_CONST_ITERATE(TQueueParamMap, it, m_QueueParamMap) {
        SQueueParameters* params = it->second;
        delete params;
    }
    m_QueueParamMap.clear();
}


void CQueueDataBase::StopPurge(void)
{
    CFastMutexGuard guard(m_PurgeLock);
    m_StopPurge = true;
}


bool CQueueDataBase::x_CheckStopPurge(void)
{
    CFastMutexGuard guard(m_PurgeLock);
    bool stop_purge = m_StopPurge;
    m_StopPurge = false;
    return stop_purge;
}


void CQueueDataBase::RunPurgeThread(void)
{
    LOG_POST(Info << "Starting guard and cleaning thread...");
    m_PurgeThread.Reset(
        new CJobQueueCleanerThread(m_Host, *this, 1));
    m_PurgeThread->Run();
    LOG_POST(Info << "Started.");
}


void CQueueDataBase::StopPurgeThread(void)
{
    if (!m_PurgeThread.Empty()) {
        LOG_POST(Info << "Stopping guard and cleaning thread...");
        StopPurge();
        m_PurgeThread->RequestStop();
        m_PurgeThread->Join();
        m_PurgeThread.Reset(0);
        LOG_POST(Info << "Stopped.");
    }
}

void CQueueDataBase::RunNotifThread(void)
{
    if (GetUdpPort() == 0) {
        return;
    }

    LOG_POST(Info << "Starting client notification thread...");
    m_NotifThread.Reset(
        new CJobNotificationThread(*this, 1));
    m_NotifThread->Run();
    LOG_POST(Info << "Started.");
}

void CQueueDataBase::StopNotifThread(void)
{
    if (!m_NotifThread.Empty()) {
        LOG_POST(Info << "Stopping notification thread...");
        m_NotifThread->RequestStop();
        m_NotifThread->Join();
        m_NotifThread.Reset(0);
        LOG_POST(Info << "Stopped.");
    }
}

void CQueueDataBase::RunExecutionWatcherThread(unsigned run_delay)
{
    LOG_POST(Info << "Starting execution watcher thread...");
    m_ExeWatchThread.Reset(
        new CJobQueueExecutionWatcherThread(m_Host, *this, run_delay));
    m_ExeWatchThread->Run();
    LOG_POST(Info << "Started.");
}

void CQueueDataBase::StopExecutionWatcherThread(void)
{
    if (!m_ExeWatchThread.Empty()) {
        LOG_POST(Info << "Stopping execution watch thread...");
        m_ExeWatchThread->RequestStop();
        m_ExeWatchThread->Join();
        m_ExeWatchThread.Reset(0);
        LOG_POST(Info << "Stopped.");
    }
}


END_NCBI_SCOPE
