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
*  Author: Michael Kimelman, Eugene Vasilchenko
*
*  File Description: GenBank Data loader
*
*/

#include <objmgr/gbloader.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbiobj.hpp>
#include <objmgr/impl/tse_info.hpp>
#include <objmgr/impl/handle_range_map.hpp>
#include <objmgr/impl/data_source.hpp>
#include <objmgr/impl/annot_object.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objmgr/reader_id1.hpp>
#include <objmgr/reader_pubseq.hpp>
#include <dbapi/driver/exception.hpp>
#include <dbapi/driver/interfaces.hpp>
#include "gbload_util.hpp"
#include <bitset>
#include <set>
#include <vector>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


//=======================================================================
//   GBLoader sub classes 
//

//=======================================================================
// GBLoader Public interface 
// 

const char* GB_ENV_VAR = "GENBANK_LOADER_METHOD";
const char* DEFAULT_DRIVERS_ORDER = "PUBSEQOS:ID1";
const string DRV_PUBSEQOS = "PUBSEQOS";
const string DRV_ID1 = "ID1";


CTSEUpload::CTSEUpload()
    : m_mode(eNone)
{
}


CTSEUpload::~CTSEUpload()
{
}


CGBDataLoader::STSEinfo::STSEinfo()
    : locked(0), tseinfop(0)
{
}


CGBDataLoader::STSEinfo::~STSEinfo()
{
}


CGBDataLoader::CCmpTSE::CCmpTSE(const CRef<CSeqref>& sr)
    : m_sr(sr)
{
}


CGBDataLoader::CCmpTSE::CCmpTSE(CSeqref* sr)
    : m_sr(sr)
{
}


CGBDataLoader::CCmpTSE::~CCmpTSE(void)
{
}


CGBDataLoader::SSeqrefs::SSeqrefs()
{
}


CGBDataLoader::SSeqrefs::~SSeqrefs()
{
}


// Create driver specified in "env"
CReader* s_CreateReader(string env)
{
#if defined(HAVE_LIBDL) && defined(HAVE_PUBSEQ_OS)
    if (env == DRV_PUBSEQOS) {
        try {
            return new CPubseqReader;
        }
        catch(exception& e) {
            GBLOG_POST("CPubseqReader is not available ::" << e.what());
            return 0;
        }
        catch(...) {
            LOG_POST("CPubseqReader:: unable to init ");
            return 0;
        }
    }
#endif
    if (env == DRV_ID1) {
        return new CId1Reader;
    }
    return 0;
}


CGBDataLoader::CGBDataLoader(const string& loader_name, CReader *driver,
                             int gc_threshold)
  : CDataLoader(loader_name),
    m_Driver(driver)
{
    GBLOG_POST( "CGBDataLoader");
    const char* env = ::getenv(GB_ENV_VAR);
    if (!env) {
        env = DEFAULT_DRIVERS_ORDER; // default drivers' order
    }
    list<string> drivers;
    NStr::Split(env, ":", drivers);
    for (list<string>::iterator drv = drivers.begin();
        drv != drivers.end()  &&  !m_Driver; ++drv) {
        m_Driver = s_CreateReader(*drv);
    }
    if(!m_Driver) {
        throw runtime_error("Could not create driver: " + string(env));
    }
  
    size_t i = m_Driver->GetParallelLevel();
    m_Locks.m_Pool.SetSize(i<=0?10:i);
    m_Locks.m_SlowTraverseMode=0;
  
    m_TseCount=0;
    m_TseGC_Threshhold=gc_threshold;
    m_InvokeGC=false;
    //GBLOG_POST( "CGBDataLoader(" << loader_name <<"::" << gc_threshold << ")" );
}

CGBDataLoader::~CGBDataLoader(void)
{
    GBLOG_POST( "~CGBDataLoader");
    CGBLGuard g(m_Locks,"~CGBDataLoader");
    if ( m_TseCount ) {
        ERR_POST("CGBDataLoader::~CGBDataLoader: not all TSE were dropped: "<<
                 m_TseCount);
    }
    while ( m_UseListHead ) {
        x_DropTSEinfo(m_UseListHead);
    }
    m_Bs2Sr.clear();
}

bool
CGBDataLoader::GetRecords(const CHandleRangeMap& hrmap,
                          const EChoice choice)
{
    GC();

    bool unreleased_mutex_run;
    int count=0;
  
    char s[100];
#ifdef DEBUG_SYNC
    memset(s,0,sizeof(s));
    {
        strstream ss(s,sizeof(s));
        const char *x;
        switch(choice) {
        case eBlob:        x="eBlob";       break;
        case eBioseq:      x="eBioseq";     break;
        case eCore:        x="eCore";       break;
        case eBioseqCore:  x="eBioseqCore"; break;
        case eSequence:    x="eSequence";   break;
        case eFeatures:    x="eFeatures";   break;
        case eGraph:       x="eGraph";      break;
        case eAll:         x="eAll";        break;
        default:           x="???"; 
        }
        ss << "GetRecords(" << x <<")";
    }
#else
    s[0] = 0;
#endif
    CGBLGuard g(m_Locks,s);
    do {
        unreleased_mutex_run=true;
        ITERATE (CHandleRangeMap, hrange, hrmap.GetMap() ) {
            //GBLOG_POST( "GetRecords-0" );
            if(x_GetRecords(hrange->first,hrange->second,choice))
                unreleased_mutex_run=false;
        }
        _VERIFY(count++ < 10); // actually I would expect it to be 2 at most
    } while (unreleased_mutex_run!=true);
    //GBLOG_POST( "GetRecords-end" );
    return true;
}

#if !defined(_DEBUG)
inline
#endif
void
CGBDataLoader::x_Check(CConstRef<STSEinfo> _DEBUG_ARG(me))
{
#if defined(_DEBUG)
    unsigned c = 0;
    bool tse_found=false;
    CRef<STSEinfo> tse2 = m_UseListHead, t1;
    while ( tse2 ) {
        c++;
        if( tse2 == me )
            tse_found = true;
        t1 = tse2;
        tse2 = tse2->next;
    }
    _ASSERT(t1 == m_UseListTail);
    _ASSERT(m_Sr2TseInfo.size() == m_TseCount ||
            m_Sr2TseInfo.size() == m_TseCount + 1);
    _ASSERT(c  <= m_TseCount);
    //_ASSERT(m_Tse2TseInfo.size() <= m_TseCount);
    if(me) {
        //GBLOG_POST("check tse " << me << " by " << CThread::GetSelf() );
        _ASSERT(tse_found);
    }
#endif
}


CRef<CGBDataLoader::STSEinfo>
CGBDataLoader::GetTSEinfo(const CTSE_Info& tse_info)
{
    return CRef<STSEinfo>(const_cast<STSEinfo*>
                          (dynamic_cast<const STSEinfo*>
                           (tse_info.GetBlobId().GetPointerOrNull())));
}


bool CGBDataLoader::DropTSE(const CTSE_Info& tse_info)
{
    CGBLGuard g(m_Locks,"drop_tse");
    //TTse2TSEinfo::iterator it = m_Tse2TseInfo.find(sep);
    //if (it == m_Tse2TseInfo.end()) // oops - apprently already done;
    //    return true;
    CRef<STSEinfo> tse = GetTSEinfo(tse_info);
    if ( !tse )
        return true;
    g.Lock(&*tse);
    _ASSERT(tse);
    x_Check(tse);
  
    //m_Tse2TseInfo.erase(it);
    x_DropTSEinfo(tse);
    return true;
}


#define DUMP(stse) (stse).key->printTSE() << \
    " stse=" << &(stse) << \
    " tsei=" << (stse).tseinfop << \
    " tse=" << ((stse).tseinfop? &((stse).tseinfop->GetTSE()): (void*)0)


CConstRef<CTSE_Info>
CGBDataLoader::ResolveConflict(const CSeq_id_Handle& handle,
                               const TTSE_LockSet& tse_set)
{
    bool         conflict=false;

#ifdef _DEBUG  
    {{
        CNcbiOstrstream s;
        s << "CGBDataLoader::ResolveConflict("<<handle.AsString();
        ITERATE ( TTSE_LockSet, it, tse_set ) {
            CRef<STSEinfo> ti = GetTSEinfo(**it);
            s << ", " << DUMP(*ti);
        }
        _TRACE(string(CNcbiOstrstreamToString(s)));
    }}
#endif

    GBLOG_POST( "ResolveConflict" );
    CGBLGuard g(m_Locks,"ResolveConflict");
    CRef<SSeqrefs> sr;
    CConstRef<CTSE_Info> best;
    {
        int cnt = 20;
        while ( !(sr = x_ResolveHandle(handle))  && cnt>0 )
            cnt --;
        if ( !sr )
            return best;
    }

    ITERATE(TTSE_LockSet, sit, tse_set) {
        CConstRef<CTSE_Info> ti = *sit;
        CRef<STSEinfo> tse = GetTSEinfo(*ti);
        if ( !tse )
            continue;
      
        x_Check(tse);

        g.Lock(&*tse);

        if(tse->mode.test(STSEinfo::eDead) && !ti->IsDead()) {
            GetDataSource()->
                x_UpdateTSEStatus(const_cast<CSeq_entry&>(ti->GetSeq_entry()),
                                  true);
        }
        if(tse->m_SeqIds.find(handle)!=tse->m_SeqIds.end()) {
            // listed for given TSE
            if(!best) {
                best=ti; conflict=false;
            }
            else if(!ti->IsDead() && best->IsDead()) {
                best=ti; conflict=false;
            }
            else if(ti->IsDead() && best->IsDead()) {
                conflict=true;
            }
            else if(ti->IsDead() && !best->IsDead()) {
            }
            else {
                conflict=true;
                //_ASSERT(ti->IsDead() || best->IsDead());
            }
        }
        g.Unlock(&*tse);
    }

    if ( !best || conflict ) {
        // try harder
        
        best.Reset();conflict=false;
        ITERATE (SSeqrefs::TSeqrefs, srp, sr->m_Sr) {
            TSr2TSEinfo::iterator tsep = m_Sr2TseInfo.find(CCmpTSE(*srp));
            if (tsep == m_Sr2TseInfo.end()) continue;
            ITERATE(TTSE_LockSet, sit, tse_set) {
                CConstRef<CTSE_Info> ti = *sit;
                //TTse2TSEinfo::iterator it =
                //    m_Tse2TseInfo.find(&ti->GetSeq_entry());
                //if(it==m_Tse2TseInfo.end()) continue;
                CRef<STSEinfo> tinfo = GetTSEinfo(*ti);
                if ( !tinfo )
                    continue;

                if(tinfo==tsep->second) {
                    if ( !best )
                        best=ti;
                    else if (ti != best)
                        conflict=true;
                }
            }
        }
        if(conflict)
            best.Reset();
    }
    if ( !best ) {
        _TRACE("CGBDataLoader::ResolveConflict("<<handle.AsString()<<") - conflict");
    }
    return best;
}

//=======================================================================
// GBLoader private interface
// 

void CGBDataLoader::x_ExcludeFromDropList(CRef<STSEinfo> tse)
{
    _ASSERT(tse);
    _TRACE("x_ExcludeFromDropList("<<DUMP(*tse)<<")");
    if ( bool(tse->next) || bool(tse->prev) ) {
        x_Check(tse);
        if ( tse->next ) {
            _ASSERT(tse != m_UseListTail);
            tse->next->prev = tse->prev;
        }
        else {
            _ASSERT(tse == m_UseListTail);
            m_UseListTail = tse->prev;
        }
        if ( tse->prev ) {
            _ASSERT(tse != m_UseListHead);
            tse->prev->next = tse->next;
        }
        else {
            _ASSERT(tse == m_UseListHead);
            m_UseListHead = tse->next;
        }
        tse->prev.Reset();
        tse->next.Reset();
        --m_TseCount;
    }
    else if ( tse == m_UseListHead ) {
        _ASSERT(tse == m_UseListTail);
        x_Check(tse);
        m_UseListHead.Reset();
        m_UseListTail.Reset();
        --m_TseCount;
    }
    else {
        _ASSERT(tse != m_UseListTail);
    }
    x_Check();
}


void CGBDataLoader::x_AppendToDropList(CRef<STSEinfo> tse)
{
    _ASSERT(tse);
    _TRACE("x_AppendToDropList("<<DUMP(*tse)<<")");
    _ASSERT(m_Sr2TseInfo[CCmpTSE(tse->key)] == tse);
    x_Check();
    _ASSERT(!tse->next && !tse->prev);
    if ( m_UseListTail ) {
        tse->prev = m_UseListTail;
        m_UseListTail->next = tse;
        m_UseListTail = tse;
    }
    else {
        _ASSERT(!m_UseListHead);
        m_UseListHead = m_UseListTail = tse;
    }
    ++m_TseCount;
    x_Check(tse);
}


void CGBDataLoader::x_UpdateDropList(CRef<STSEinfo> tse)
{
    _ASSERT(tse);
    // reset LRU links
    if(tse == m_UseListTail) // already the last one
        return;
  
    // Unlink from current place
    x_ExcludeFromDropList(tse);
    x_AppendToDropList(tse);
}

void CGBDataLoader::x_DropTSEinfo(CRef<STSEinfo> tse)
{
    if(!tse) return;
    _TRACE( "DropBlob(" << DUMP(*tse) << ")" );
  
    ITERATE(STSEinfo::TSeqids,sih_it,tse->m_SeqIds) {
        m_Bs2Sr.erase(*sih_it);
    }

    x_ExcludeFromDropList(tse);
    m_Sr2TseInfo.erase(CCmpTSE(tse->key));
}

void CGBDataLoader::GC(void)
{
    // GBLOG_POST( "X_GC " << m_TseCount << "," << m_TseGC_Threshhold << "," << m_InvokeGC);
    // dirty read - but that ok for garbage collector
    if(!m_InvokeGC || m_TseCount==0) return ;
    if(m_TseCount < m_TseGC_Threshhold) {
        if(m_TseCount < 0.5*m_TseGC_Threshhold)
            m_TseGC_Threshhold = (m_TseCount + 3*m_TseGC_Threshhold)/4;
        return;
    }
    GBLOG_POST( "X_GC " << m_TseCount);
    //GetDataSource()->x_CleanupUnusedEntries();

    CGBLGuard g(m_Locks,"GC");
    x_Check();

    unsigned skip=0;
    unsigned skip_max = (int)(0.1*m_TseCount + 1); /* scan 10% of least recently used pile before giving up */
    CRef<STSEinfo> cur_tse = m_UseListHead;
    while ( bool(cur_tse) && skip<skip_max) {
        CRef<STSEinfo> tse_to_drop(cur_tse);
        cur_tse = cur_tse->next;
        ++skip;
        // fast checks
        if (tse_to_drop->locked) continue;
        if (tse_to_drop->tseinfop && tse_to_drop->tseinfop->Locked()) continue;
        //CRef<CTSE_Info> tse_info(tse_to_drop->tseinfop);

        CSeq_entry *sep = tse_to_drop->m_upload.m_tse;
        if ( !sep ) {
            if (tse_to_drop->m_upload.m_mode != CTSEUpload::eNone) {
                g.Lock(&*tse_to_drop);
                GBLOG_POST("X_GC:: drop nonexistent tse " << tse_to_drop);
                x_DropTSEinfo(tse_to_drop);
                g.Unlock(&*tse_to_drop);
                --skip;
            }
            continue;
        }
        //if(m_Tse2TseInfo.find(sep) == m_Tse2TseInfo.end()) continue;
        
        GBLOG_POST("X_GC::DropTSE(" << tse_to_drop << "::" << tse_to_drop->key->printTSE() << ")");
        CRef<CSeq_entry> se(sep);
        //g.Unlock();
        g.Lock(&*tse_to_drop);
        if( GetDataSource()->DropTSE(*se) ) {
            --skip;
            m_InvokeGC=false;
        }
        g.Unlock(&*tse_to_drop);
        //g.Lock();
#if defined(NCBI_THREADS)
        unsigned i=0;
        for(cur_tse = m_UseListHead; bool(cur_tse) && i<skip; ++i) {
            cur_tse = cur_tse->next;
        }
#endif
    }
    if(m_InvokeGC) { // nothing has been cleaned up
        //assert(m_TseGC_Threshhold<=m_TseCount); // GC entrance condition
        m_TseGC_Threshhold = m_TseCount+2; // do not even try until next load
    } else if(m_TseCount < 0.5*m_TseGC_Threshhold) {
        m_TseGC_Threshhold = (m_TseCount + m_TseGC_Threshhold)/2;
    }
}


CGBDataLoader::TMask
CGBDataLoader::x_Request2SeqrefMask(const EChoice /*choice*/)
{
    /** ignore choice for now
    switch(choice)
    {
      // split code
    default:
        return ~0;
    }
    **/
    return ~0;
}

CGBDataLoader::TMask
CGBDataLoader::x_Request2BlobMask(const EChoice /*choice*/)
{
    /** ignore choice for now
    // split code
    switch(choice)
    {
        // split code
    default:
        return 1;
    }
    **/
    return 1;
}

bool CGBDataLoader::x_GetRecords(const CSeq_id_Handle& sih,
                                 const CHandleRange& hrange,
                                 EChoice choice)
{
    TMask         sr_mask   = x_Request2SeqrefMask(choice);
    TMask         blob_mask = x_Request2BlobMask(choice);
    bool         global_mutex_was_released=false;

    //GBLOG_POST( "x_GetRecords" );
    CRef<SSeqrefs>    sr = x_ResolveHandle(sih);
    if( !sr )
        global_mutex_was_released=true;
  
    if( sr->m_Sr.empty() ) // no data for given seqid
        return global_mutex_was_released;
  
    //GBLOG_POST( "x_GetRecords-Seqref_iterate" );
    ITERATE (SSeqrefs::TSeqrefs, srp, sr->m_Sr) {
        // skip TSE which doesn't contain requested type of info
        //GBLOG_POST( "x_GetRecords-Seqref_iterate_0" );
        if( ((~(*srp)->Flag()) & sr_mask) == 0 ) continue;
        //GBLOG_POST( "list uploaded TSE");
        //for(TSr2TSEinfo::iterator tsep = m_Sr2TseInfo.begin(); tsep != m_Sr2TseInfo.end(); ++tsep)
        //  {
        //     GBLOG_POST(tsep->first.get().printTSE());
        //  }
        // GBLOG_POST("x_GetRecords-Seqref_iterate_1" << (*srp)->print() );
      
        // find TSE info for each seqref
        TSr2TSEinfo::iterator tsep = m_Sr2TseInfo.find(CCmpTSE(*srp));
        CRef<STSEinfo> tse;
        if (tsep != m_Sr2TseInfo.end()) {
            tse = tsep->second;
            //GBLOG_POST( "x_GetRecords-oldTSE(" << tse << ") mode=" << (tse->m_upload.m_mode));
        }
        else {
            tse.Reset(new STSEinfo());
            tse->key = *srp;
            m_Sr2TseInfo[CCmpTSE(*srp)] = tse;
            x_AppendToDropList(tse);
            GBLOG_POST( "x_GetRecords-newTSE(" << tse << ") ");
        }

        CGBLGuard g(m_Locks,CGBLGuard::eMain,"x_GetRecords");
        g.Lock(&*tse);
        tse->locked++;
        {{ // make sure we have reverse reference to handle
            STSEinfo::TSeqids &sid = tse->m_SeqIds;
            if (sid.find(sih) == sid.end())
                sid.insert(sih);
        }}
        bool new_tse=false;
      
        ITERATE (CHandleRange, lrange , hrange) {
            //GBLOG_POST( "x_GetRecords-range_0" );
            // check Data
            //GBLOG_POST( "x_GetRecords-range_0" );
            if(!x_NeedMoreData(&tse->m_upload,
                               **srp,
                               lrange->first.GetFrom(),
                               lrange->first.GetTo(),
                               blob_mask)
                )
                continue;
          
            _ASSERT(tse->m_upload.m_mode != CTSEUpload::eDone);

            //GBLOG_POST( "x_GetRecords-range_1" );
            // need update
            g.Local();
            global_mutex_was_released=true;

            int try_cnt = 2;
            while( try_cnt-- > 0 ) {
                CReader::TConn conn = m_Locks.m_Pool.Select(&*tse);
                try {
                    new_tse = new_tse ||
                        x_GetData(tse,
                                  **srp,
                                  lrange->first.GetFrom(),
                                  lrange->first.GetTo(),
                                  blob_mask,
                                  conn
                            );
                    break;
                }
                catch(const CIOException &e) {
                    LOG_POST(e.what());
                    LOG_POST("GenBank connection failed: Reconnecting....");
                    m_Driver->Reconnect(conn);
                }
                catch(const CDB_Exception &e) {
                    LOG_POST(e.what());
                    LOG_POST("GenBank connection failed: Reconnecting....");
                    m_Driver->Reconnect(conn);
                }
                catch(const exception &e) {
                    LOG_POST(e.what());
                    LOG_POST("GenBank connection failed: Reconnecting....");
                    m_Driver->Reconnect(conn);
                }
                catch (...) {
                    LOG_POST(CThread::GetSelf()<<":: Data request failed....");
                    g.Lock();
                    g.Lock(&*tse);
                    tse->locked--;
                    x_UpdateDropList(tse); // move up as just checked
                    throw;
                }
            }
            if ( try_cnt < 0 ) {
                LOG_POST("CGBLoader:GetData: Data Request failed :: "
                         "exceeded maximum attempts count");
                g.Lock();
                g.Lock(&*tse);
                tse->locked--;
                x_UpdateDropList(tse); // move up as just checked
                throw runtime_error("CGBLoader:GetData: Multiple attempts to retrieve data failed");
            }
        }
        g.Lock();
        g.Lock(&*tse);
        x_UpdateDropList(tse);
        if(new_tse) {
            x_Check();
            _ASSERT(tse->m_upload.m_tse);
            _ASSERT(tse->tseinfop);
            //_ASSERT(m_Tse2TseInfo.find(tse->m_upload.m_tse) == m_Tse2TseInfo.end());
            //m_Tse2TseInfo[tse->m_upload.m_tse]=tse; // insert
            new_tse=false;
        }
        tse->locked--;
        x_Check(tse);
    } // iterate seqrefs
    return global_mutex_was_released;
}


class CTimerGuard
{
    CTimer *t;
    bool    calibrating;
public:
    CTimerGuard(CTimer& x) : t(&x) { calibrating=t->NeedCalibration(); if(calibrating) t->Start(); }
    ~CTimerGuard() { if(calibrating) t->Stop(); }
};


CRef<CGBDataLoader::SSeqrefs>
CGBDataLoader::x_ResolveHandle(const CSeq_id_Handle& h)
{
    CGBLGuard g(m_Locks,CGBLGuard::eMain,"x_ResolveHandle");

    CRef<SSeqrefs> sr;
    TSeqId2Seqrefs::iterator bsit = m_Bs2Sr.find(h);
    if (bsit == m_Bs2Sr.end() ) {
        sr.Reset(new SSeqrefs());
        m_Bs2Sr[h] = sr;
    }
    else {
        sr = bsit->second;
    }

    g.Lock(&*sr);
    if( !sr->m_Timer.NeedRefresh(m_Timer) )
        return sr;
    g.Local();
    //GBLOG_POST( "ResolveHandle-before(" << h << ") " << sr->m_Sr.size() );
  
    SSeqrefs::TSeqrefs osr;
    bool got = false;
    for ( int try_cnt = 2; !got && try_cnt > 0; --try_cnt ) {
        CTimerGuard tg(m_Timer);
        CReader::TConn conn = m_Locks.m_Pool.Select(&*sr);
        try {
            osr.clear();
            got = m_Driver->RetrieveSeqrefs(osr, h.GetSeqId(), conn);
        }
        catch(const CIOException &e) {
            LOG_POST(e.what());
            LOG_POST("GenBank connection failed: Reconnecting....");
            m_Driver->Reconnect(conn);
        }
        catch(const CDB_Exception &e) {
            LOG_POST(e.what());
            LOG_POST("GenBank connection failed: Reconnecting....");
            m_Driver->Reconnect(conn);
        }
        catch(const exception &e) {
            LOG_POST(e.what());
            LOG_POST("GenBank connection failed: Reconnecting....");
            m_Driver->Reconnect(conn);
        }
    }
    if ( !got ) {
        throw runtime_error("Network trouble - failed to connect to NCBI ID services");
    }

    swap(sr->m_Sr, osr);
    sr->m_Timer.Reset(m_Timer);
  
    g.Lock(); // will unlock everything and lock lookupMutex again 
  
    GBLOG_POST( "ResolveHandle(" << h << ") " << sr->m_Sr.size() );
    ITERATE(SSeqrefs::TSeqrefs, srp, sr->m_Sr) {
        GBLOG_POST( (*srp)->print());
    }
  
    if ( !osr.empty() ) {
        bsit = m_Bs2Sr.find(h);
        // make sure we are not deleted in the unlocked time 
        if (bsit != m_Bs2Sr.end()) {
            SSeqrefs::TSeqrefs& nsr=bsit->second->m_Sr;
          
            // catch dissolving TSE and mark them dead
            //GBLOG_POST( "old seqrefs");
            ITERATE(SSeqrefs::TSeqrefs,srp,osr) {
                //(*srp)->print(); cout);
                bool found=false;
                ITERATE(SSeqrefs::TSeqrefs,nsrp,nsr) {
                    if( CCmpTSE(*srp) == CCmpTSE(*nsrp) ) {
                        found=true;
                        break;
                    }
                }
                if(found) continue;
                TSr2TSEinfo::iterator tsep = m_Sr2TseInfo.find(CCmpTSE(*srp));
                if (tsep == m_Sr2TseInfo.end()) continue;
              
                // update TSE info 
                CRef<STSEinfo> tse = tsep->second;
                g.Lock(&*tse);
                bool mark_dead  = tse->mode.test(STSEinfo::eDead);
                if(mark_dead) tse->mode.set(STSEinfo::eDead);
                tse->m_SeqIds.erase(h); // drop h as refewrenced seqid
                g.Unlock(&*tse);
                if(mark_dead && bool(tse->m_upload.m_tse)) {
                    // inform data_source :: make sure to avoid deadlocks
                    GetDataSource()->x_UpdateTSEStatus(*(tse->m_upload.m_tse),true);
                }
            }
        }
    }
    return sr;
}

bool CGBDataLoader::x_NeedMoreData(CTSEUpload *tse_up,
                                   const CSeqref& /*srp*/,
                                   int /*from*/, int /*to*/,
                                   TMask /*blob_mask*/)
{
    bool need_data=true;
  
    if (tse_up->m_mode==CTSEUpload::eDone)
        need_data=false;
    if (tse_up->m_mode==CTSEUpload::ePartial) {
        // split code : check tree for presence of data and
        // return from routine if all data already loaded
        // present;
    }
    //GBLOG_POST( "x_NeedMoreData(" << srp << "," << tse_up << ") need_data " << need_data <<);
    return need_data;
}

bool CGBDataLoader::x_GetData(CRef<STSEinfo> tse,
                              const CSeqref& srp,
                              int from,int to,
                              TMask blob_mask,
                              CReader::TConn conn)
{
    CTSEUpload *tse_up = &tse->m_upload;
    if(!x_NeedMoreData(tse_up,srp,from,to,blob_mask))
        return false;
    bool new_tse = false;
    GBLOG_POST( "GetBlob(" << srp.print() << "," <<
                tse_up << ") " << from << ":"<< to << ":=" << tse_up->m_mode);
  
    _ASSERT(tse_up->m_mode != CTSEUpload::eDone);
    if (tse_up->m_mode == CTSEUpload::eNone) {
        CSeqref::TBlobClass cl = blob_mask;
        int count=0;
        for ( CRef<CBlobSource> bs(srp.GetBlobSource(from, to, cl, conn));
              bs->HaveMoreBlobs(); ++count) {
            CRef<CBlob> blob(bs->RetrieveBlob());
            GBLOG_POST("GetBlob(" << srp.print() << ") " <<
                       from << ":"<< to << "  class("<<blob->Class()<<")");
            m_InvokeGC=true;
            if (blob->Class()==0) {
                tse_up->m_tse    = blob->Seq_entry();
                if(tse_up->m_tse) {
                    tse_up->m_mode   = CTSEUpload::eDone;
                    new_tse=true;
                    CRef<CTSE_Info> tseinfo = 
                        GetDataSource()->AddTSE(*(tse_up->m_tse), false, tse);
                    tse->tseinfop = &*tseinfo;
                    _TRACE("GetBlob(" << srp.print() << " = " << DUMP(*tse) << ") - whole blob retrieved");
                }
                else {
                    tse_up->m_mode   = CTSEUpload::eDone;
                    LOG_POST("GetBlob(" <<  srp.print() << ") " << "- "
                             "retrieval of the whole blob failed - "
                             "no data available");
                }
            }
            else {
                LOG_POST("GetBlob(" <<srp.print()<< ") " << "- partial load");
                _ASSERT(tse_up->m_mode != CTSEUpload::eDone);
                if(tse_up->m_mode != CTSEUpload::ePartial)
                    tse_up->m_mode = CTSEUpload::ePartial;
                // split code : upload tree
                _ASSERT(0);
            }
        }
        if(count==0) {
            tse_up->m_mode = CTSEUpload::eDone;
            // TODO log message
            LOG_POST("ERROR: can not retrive sequence  : " << srp.print());
        }
      
        //GBLOG_POST( "GetData-after:: " << from << to <<  endl;
    }
    else {
        _ASSERT(tse_up->m_mode==CTSEUpload::ePartial);
        _ASSERT(0);
    }
    return new_tse;
}

void
CGBDataLoader::DebugDump(CDebugDumpContext ddc, unsigned int /*depth*/) const
{
  ddc.SetFrame("CGBLoader");
  // CObject::DebugDump( ddc, depth);
  DebugDumpValue(ddc,"m_TseCount", m_TseCount);
  DebugDumpValue(ddc,"m_TseGC_Threshhold", m_TseGC_Threshhold);
}

END_SCOPE(objects)
END_NCBI_SCOPE



/* ---------------------------------------------------------------------------
* $Log$
* Revision 1.75  2003/06/10 19:01:07  vasilche
* Fixed loader methods string.
*
* Revision 1.74  2003/06/10 15:25:33  vasilche
* Changed wrong _ASSERT to _VERIFY
*
* Revision 1.73  2003/06/02 16:06:37  dicuccio
* Rearranged src/objects/ subtree.  This includes the following shifts:
*     - src/objects/asn2asn --> arc/app/asn2asn
*     - src/objects/testmedline --> src/objects/ncbimime/test
*     - src/objects/objmgr --> src/objmgr
*     - src/objects/util --> src/objmgr/util
*     - src/objects/alnmgr --> src/objtools/alnmgr
*     - src/objects/flat --> src/objtools/flat
*     - src/objects/validator --> src/objtools/validator
*     - src/objects/cddalignview --> src/objtools/cddalignview
* In addition, libseq now includes six of the objects/seq... libs, and libmmdb
* replaces the three libmmdb? libs.
*
* Revision 1.72  2003/05/20 21:13:02  vasilche
* Fixed ambiguity on MSVC.
*
* Revision 1.71  2003/05/20 18:27:29  vasilche
* Fixed ambiguity on MSVC.
*
* Revision 1.70  2003/05/20 16:18:42  vasilche
* Fixed compilation errors on GCC.
*
* Revision 1.69  2003/05/20 15:44:37  vasilche
* Fixed interaction of CDataSource and CDataLoader in multithreaded app.
* Fixed some warnings on WorkShop.
* Added workaround for memory leak on WorkShop.
*
* Revision 1.68  2003/05/13 20:27:05  vasilche
* Added lost SAT SATKEY info in error message.
*
* Revision 1.67  2003/05/13 20:21:10  vasilche
* *** empty log message ***
*
* Revision 1.66  2003/05/13 20:14:40  vasilche
* Catching exceptions and reconnection were moved from readers to genbank loader.
*
* Revision 1.65  2003/05/13 18:32:29  vasilche
* Fixed use of GBLOG_POST() macro.
*
* Revision 1.64  2003/05/12 19:18:29  vasilche
* Fixed locking of object manager classes in multi-threaded application.
*
* Revision 1.63  2003/05/12 18:26:08  vasilche
* Removed buggy _ASSERT() on reconnection.
*
* Revision 1.62  2003/05/06 16:52:28  vasilche
* Try to reconnect to genbank on any exception.
*
* Revision 1.61  2003/04/29 19:51:13  vasilche
* Fixed interaction of Data Loader garbage collector and TSE locking mechanism.
* Made some typedefs more consistent.
*
* Revision 1.60  2003/04/24 16:12:38  vasilche
* Object manager internal structures are splitted more straightforward.
* Removed excessive header dependencies.
*
* Revision 1.59  2003/04/18 17:38:01  grichenk
* Use GENBANK_LOADER_METHOD env. variable to specify GB readers.
* Default is "PUBSEQOS:ID1".
*
* Revision 1.58  2003/04/15 16:32:29  dicuccio
* Added include for I_DriverContext from DBAPI library - avoids concerning
* warning about deletion of unknwon type.
*
* Revision 1.57  2003/04/15 15:30:15  vasilche
* Added include <memory> when needed.
* Removed buggy buffer in printing methods.
* Removed unnecessary include of stream_util.hpp.
*
* Revision 1.56  2003/04/15 14:24:08  vasilche
* Changed CReader interface to not to use fake streams.
*
* Revision 1.55  2003/03/21 19:22:51  grichenk
* Redesigned TSE locking, replaced CTSE_Lock with CRef<CTSE_Info>.
*
* Revision 1.54  2003/03/11 15:51:06  kuznets
* iterate -> ITERATE
*
* Revision 1.53  2003/03/05 20:54:41  vasilche
* Commented out wrong assert().
*
* Revision 1.52  2003/03/03 20:34:51  vasilche
* Added NCBI_THREADS macro - it's opposite to NCBI_NO_THREADS.
* Avoid using _REENTRANT macro - use NCBI_THREADS instead.
*
* Revision 1.51  2003/03/01 23:07:42  kimelman
* bugfix: MTsafe
*
* Revision 1.50  2003/03/01 22:27:57  kimelman
* performance fixes
*
* Revision 1.49  2003/02/27 21:58:26  vasilche
* Fixed performance of Object Manager's garbage collector.
*
* Revision 1.48  2003/02/26 18:03:31  vasilche
* Added some error check.
* Fixed formatting.
*
* Revision 1.47  2003/02/24 18:57:22  vasilche
* Make feature gathering in one linear pass using CSeqMap iterator.
* Do not use feture index by sub locations.
* Sort features at the end of gathering in one vector.
* Extracted some internal structures and classes in separate header.
* Delay creation of mapped features.
*
* Revision 1.46  2003/02/05 17:59:17  dicuccio
* Moved formerly private headers into include/objects/objmgr/impl
*
* Revision 1.45  2003/01/22 20:11:54  vasilche
* Merged functionality of CSeqMapResolved_CI to CSeqMap_CI.
* CSeqMap_CI now supports resolution and iteration over sequence range.
* Added several caches to CScope.
* Optimized CSeqVector().
* Added serveral variants of CBioseqHandle::GetSeqVector().
* Tried to optimize annotations iterator (not much success).
* Rewritten CHandleRange and CHandleRangeMap classes to avoid sorting of list.
*
* Revision 1.44  2002/12/26 20:53:02  dicuccio
* Moved tse_info.hpp -> include/ tree.  Minor tweaks to relieve compiler
* warnings in MSVC.
*
* Revision 1.43  2002/11/08 19:43:35  grichenk
* CConstRef<> constructor made explicit
*
* Revision 1.42  2002/11/04 21:29:12  grichenk
* Fixed usage of const CRef<> and CRef<> constructor
*
* Revision 1.41  2002/09/09 16:12:43  dicuccio
* Fixed minor typo ("conneciton" -> "connection").
*
* Revision 1.40  2002/07/24 20:30:27  ucko
* Move CTimerGuard out to file scope to fix a MIPSpro compiler core dump.
*
* Revision 1.39  2002/07/22 22:53:24  kimelman
* exception handling fixed: 2level mutexing moved to Guard class + added
* handling of confidential data.
*
* Revision 1.38  2002/06/04 17:18:33  kimelman
* memory cleanup :  new/delete/Cref rearrangements
*
* Revision 1.37  2002/05/14 20:06:26  grichenk
* Improved CTSE_Info locking by CDataSource and CDataLoader
*
* Revision 1.36  2002/05/10 16:44:32  kimelman
* tuning to allow pubseq enable build
*
* Revision 1.35  2002/05/08 22:23:48  kimelman
* MT fixes
*
* Revision 1.34  2002/05/06 03:28:47  vakatov
* OM/OM1 renaming
*
* Revision 1.33  2002/05/03 21:28:10  ucko
* Introduce T(Signed)SeqPos.
*
* Revision 1.32  2002/04/30 18:56:54  gouriano
* added multithreading-related initialization
*
* Revision 1.31  2002/04/28 03:36:47  vakatov
* Temporarily turn off R1.30(b) unconditionally -- until it is buildable
*
* Revision 1.30  2002/04/26 16:32:23  kimelman
* a) turn on GC b) turn on PubSeq where sybase is available
*
* Revision 1.29  2002/04/12 22:54:28  kimelman
* pubseq_reader auto call commented per Denis request
*
* Revision 1.28  2002/04/12 21:10:33  kimelman
* traps for coredumps
*
* Revision 1.27  2002/04/11 18:45:35  ucko
* Pull in extra headers to make KCC happy.
*
* Revision 1.26  2002/04/10 22:47:56  kimelman
* added pubseq_reader as default one
*
* Revision 1.25  2002/04/09 19:04:23  kimelman
* make gcc happy
*
* Revision 1.24  2002/04/09 18:48:15  kimelman
* portability bugfixes: to compile on IRIX, sparc gcc
*
* Revision 1.23  2002/04/05 23:47:18  kimelman
* playing around tests
*
* Revision 1.22  2002/04/04 01:35:35  kimelman
* more MT tests
*
* Revision 1.21  2002/04/02 17:27:00  gouriano
* bugfix: skip test for yet unregistered data
*
* Revision 1.20  2002/04/02 16:27:20  gouriano
* memory leak
*
* Revision 1.19  2002/04/02 16:02:31  kimelman
* MT testing
*
* Revision 1.18  2002/03/30 19:37:06  kimelman
* gbloader MT test
*
* Revision 1.17  2002/03/29 02:47:04  kimelman
* gbloader: MT scalability fixes
*
* Revision 1.16  2002/03/27 20:23:50  butanaev
* Added connection pool.
*
* Revision 1.15  2002/03/26 23:31:08  gouriano
* memory leaks and garbage collector fix
*
* Revision 1.14  2002/03/26 15:39:24  kimelman
* GC fixes
*
* Revision 1.13  2002/03/25 17:49:12  kimelman
* ID1 failure handling
*
* Revision 1.12  2002/03/25 15:44:46  kimelman
* proper logging and exception handling
*
* Revision 1.11  2002/03/22 18:56:05  kimelman
* GC list fix
*
* Revision 1.10  2002/03/22 18:51:18  kimelman
* stream WS skipping fix
*
* Revision 1.9  2002/03/22 18:15:47  grichenk
* Unset "skipws" flag in binary stream
*
* Revision 1.8  2002/03/21 23:16:32  kimelman
* GC bugfixes
*
* Revision 1.7  2002/03/21 21:39:48  grichenk
* garbage collector bugfix
*
* Revision 1.6  2002/03/21 19:14:53  kimelman
* GB related bugfixes
*
* Revision 1.5  2002/03/21 01:34:53  kimelman
* gbloader related bugfixes
*
* Revision 1.4  2002/03/20 21:24:59  gouriano
* *** empty log message ***
*
* Revision 1.3  2002/03/20 19:06:30  kimelman
* bugfixes
*
* Revision 1.2  2002/03/20 17:03:24  gouriano
* minor changes to make it compilable on MS Windows
*
* Revision 1.1  2002/03/20 04:50:13  kimelman
* GB loader added
*
* ===========================================================================
*/
