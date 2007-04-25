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
 * File Description: Network scheduler affinity.
 *
 */
#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>

#include "ns_affinity.hpp"
#include "ns_db.hpp"


BEGIN_NCBI_SCOPE

CAffinityDict::CAffinityDict()
: m_AffDictDB(0),
  m_AffDict_TokenIdx(0)
{
    m_CurAffDB = m_CurTokenIdx = 0;
    m_IdCounter.Set(0);

}

CAffinityDict::~CAffinityDict()
{
    try {
        Close();
    } 
    catch (exception& ex)
    {
        ERR_POST("Error while closing affinity dictionary " << ex.what());
    }
}

void CAffinityDict::Close()
{
    CFastMutexGuard guard(m_DbLock);

    delete m_CurAffDB; m_CurAffDB = 0;
    delete m_CurTokenIdx; m_CurTokenIdx = 0;
    delete m_AffDictDB; m_AffDictDB = 0;
    delete m_AffDict_TokenIdx; m_AffDict_TokenIdx = 0;
}


void CAffinityDict::Open(CBDB_Env& env, const string& queue_name)
{
    Close();

    CFastMutexGuard guard(m_DbLock);

    m_AffDictDB = new SAffinityDictDB();
    m_AffDict_TokenIdx = new SAffinityDictTokenIdx();

    {{
    string fname = string("jsq_") + queue_name + string("_affdict.db");
    m_AffDictDB->SetEnv(env);
    m_AffDictDB->Open(fname.c_str(), CBDB_RawFile::eReadWriteCreate);

    }}

    {{
    string fname = string("jsq_") + queue_name + string("_affdict_token.idx");
    m_AffDict_TokenIdx->SetEnv(env);
    m_AffDict_TokenIdx->Open(fname.c_str(), CBDB_RawFile::eReadWriteCreate);
    }}

    m_CurAffDB    = new CBDB_FileCursor(*m_AffDictDB);
    m_CurTokenIdx = new CBDB_FileCursor(*m_AffDict_TokenIdx);

    {{
    m_CurAffDB->SetCondition(CBDB_FileCursor::eLast);
    if (m_CurAffDB->Fetch() == eBDB_Ok) {
        unsigned aff_id = m_AffDictDB->aff_id;
        m_IdCounter.Set(aff_id);
    }
    }}    
}

unsigned CAffinityDict::CheckToken(const char*       aff_token,
                                   CBDB_Transaction& trans)
{
    unsigned aff_id;

    CFastMutexGuard guard(m_DbLock);

    // check if affinity token string already registered
    {{
        CBDB_CursorGuard cg1(*m_CurTokenIdx);
        m_CurTokenIdx->ReOpen(&trans);
        m_CurTokenIdx->SetCondition(CBDB_FileCursor::eEQ);
        m_CurTokenIdx->From << aff_token;
        if (m_CurTokenIdx->Fetch() == eBDB_Ok) {
            aff_id = m_AffDict_TokenIdx->aff_id;
            return aff_id;
        }
    }}

    // add new affinity token

    while (1) {
        aff_id = m_IdCounter.Add(1);

        // make sure it is not there yet

        {{
        CBDB_CursorGuard cg2(*m_CurAffDB);
        m_CurAffDB->ReOpen(&trans);
        m_CurAffDB->SetCondition(CBDB_FileCursor::eEQ);
        m_CurAffDB->From << aff_id;
        if (m_CurAffDB->Fetch() == eBDB_Ok) {
            aff_id = m_AffDictDB->aff_id;
            if (aff_id > (unsigned)m_IdCounter.Get()) {
                m_IdCounter.Set(aff_id);
            }
            continue;
        }
        }}

        m_AffDictDB->SetTransaction(&trans);

        m_AffDictDB->aff_id = aff_id;
        m_AffDictDB->token = aff_token;
        
        EBDB_ErrCode err = m_AffDictDB->Insert();
        if (err == eBDB_KeyDup) {
            ERR_POST("Duplicate record in affinity dictionary.");
            continue;
        }

        m_AffDict_TokenIdx->SetTransaction(&trans);
        m_AffDict_TokenIdx->aff_id = aff_id;
        m_AffDict_TokenIdx->token = aff_token;

        m_AffDict_TokenIdx->UpdateInsert();

        break;            
    } // while

    return aff_id;

}

unsigned CAffinityDict::GetTokenId(const char* aff_token)
{
    unsigned aff_id;

    CFastMutexGuard guard(m_DbLock);

    m_AffDictDB->SetTransaction(0);
    m_AffDict_TokenIdx->SetTransaction(0);

    CBDB_CursorGuard cg1(*m_CurTokenIdx);
    m_CurTokenIdx->ReOpen(0);
    m_CurTokenIdx->SetCondition(CBDB_FileCursor::eEQ);
    m_CurTokenIdx->From << aff_token;
    if (m_CurTokenIdx->Fetch() == eBDB_Ok) {
        aff_id = m_AffDict_TokenIdx->aff_id;
        return aff_id;
    }
    return 0;
}

string CAffinityDict::GetAffToken(unsigned aff_id)
{
    string token;
    CFastMutexGuard guard(m_DbLock);
    m_AffDictDB->SetTransaction(0);
    m_AffDictDB->aff_id = aff_id;
    if (m_AffDictDB->Fetch() != eBDB_Ok) {
        return kEmptyStr;
    }

    m_AffDictDB->token.ToString(token);
    return token;    
}


void CAffinityDict::RemoveToken(unsigned          aff_id, 
                                CBDB_Transaction& trans)
{
    CFastMutexGuard guard(m_DbLock);

    {{
    CBDB_CursorGuard cg1(*m_CurAffDB);
    m_CurAffDB->ReOpen(&trans);
    m_CurAffDB->SetCondition(CBDB_FileCursor::eEQ);
    m_CurAffDB->From << aff_id;
    if (m_CurAffDB->Fetch() != eBDB_Ok) {
        return;
    }
    }}
    string token;
    m_AffDictDB->token.ToString(token);
    
    m_AffDict_TokenIdx->SetTransaction(&trans);
    m_AffDictDB->SetTransaction(&trans);

    m_AffDict_TokenIdx->token = token;
    m_AffDict_TokenIdx->Delete(CBDB_RawFile::eIgnoreError);

    m_AffDictDB->aff_id = aff_id;
    m_AffDictDB->Delete(CBDB_RawFile::eIgnoreError);
}


// WorkerNodeAffinity

CWorkerNodeAffinity::CWorkerNodeAffinity()
{
}

CWorkerNodeAffinity::~CWorkerNodeAffinity()
{
    try {
        ClearAffinity();
    } 
    catch (exception& ex)
    {
        ERR_POST("Error in ~CWorkerNodeAffinity(): " << ex.what());
    }
}

void CWorkerNodeAffinity::ClearAffinity()
{
    NON_CONST_ITERATE(TAffMap, it, m_AffinityMap) {
        delete it->second; it->second = 0;
    }
    m_AffinityMap.erase(m_AffinityMap.begin(), m_AffinityMap.end());
}

void CWorkerNodeAffinity::ClearAffinity(TNetAddress   addr, 
                                        const string& client_name)
{
    TAffMap::iterator it = m_AffinityMap.find(pair<unsigned, string>(addr, client_name));
    if (it == m_AffinityMap.end()) {
        return;
    }
    delete it->second; it->second = 0;
    m_AffinityMap.erase(it);
}

void CWorkerNodeAffinity::AddAffinity(TNetAddress   addr, 
                                      const string& client_name, 
                                      unsigned      aff_id)
{
    SAffinityInfo* ai = GetAffinity(addr, client_name);
    if (ai == 0) {
        ai = new SAffinityInfo();
        m_AffinityMap[pair<unsigned, string>(addr, client_name)] = ai;
    }
    ai->aff_ids.set(aff_id);
}

void CWorkerNodeAffinity::BlacklistJob(TNetAddress   addr, 
                                       const string& client_name, 
                                       unsigned      job_id)
{
    SAffinityInfo* ai = GetAffinity(addr, client_name);
    if (ai == 0) {
        ai = new SAffinityInfo();
        m_AffinityMap[pair<unsigned, string>(addr, client_name)] = ai;
    }
    ai->blacklisted_jobs.set(job_id);
}

void CWorkerNodeAffinity::OptimizeMemory()
{
    NON_CONST_ITERATE(TAffMap, it, m_AffinityMap) {
        SAffinityInfo* ai = it->second;
        ai->aff_ids.optimize();
        ai->candidate_jobs.optimize();
    }
}


CWorkerNodeAffinity::SAffinityInfo* 
CWorkerNodeAffinity::GetAffinity(TNetAddress   addr, 
                                 const string& client_name)
{
    TAffMap::iterator it = m_AffinityMap.find(
        pair<unsigned, string>(addr, client_name));
    if( it != m_AffinityMap.end()) return it->second;
    return 0;
}

void CWorkerNodeAffinity::RemoveAffinity(unsigned aff_id)
{
    NON_CONST_ITERATE(TAffMap, it, m_AffinityMap) {
        SAffinityInfo* ai = it->second;
        ai->aff_ids.set(aff_id, false);
    }
}

void CWorkerNodeAffinity::RemoveAffinity(const TNSBitVector& bv)
{
    NON_CONST_ITERATE(TAffMap, it, m_AffinityMap) {
        SAffinityInfo* ai = it->second;
        ai->aff_ids -= bv;
    }
}

void CWorkerNodeAffinity::GetAllAssignedAffinity(TNSBitVector* aff_ids)
{
    _ASSERT(aff_ids);

    ITERATE(TAffMap, it, m_AffinityMap) {
        SAffinityInfo* ai = it->second;
        *aff_ids |= ai->aff_ids; 
    }
}

END_NCBI_SCOPE
