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
*  Author: Michael Kimelman
*
*  File Description: GenBank Data loader
*
*/

#include <corelib/ncbistd.hpp>
#include <corelib/ncbiobj.hpp>
#include <objects/objmgr/impl/tse_info.hpp>
#include <objects/objmgr/impl/handle_range_map.hpp>
#include <objects/objmgr/impl/data_source.hpp>
#include <objects/objmgr/impl/annot_object.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/objmgr/reader_id1.hpp>
#include <objects/objmgr/reader_pubseq.hpp>
#include <objects/objmgr/gbloader.hpp>
#include "gbload_util.hpp"
#include <bitset>
#include <set>
#include <vector>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


//=======================================================================
//   GBLoader sub classes 
//

struct CGBDataLoader::STSEinfo
{
  typedef set<TSeq_id_Key>  TSeqids;
  enum { eDead, eConfidential, eLast };
  
  STSEinfo         *next;
  STSEinfo         *prev;
  bitset<eLast>     mode;
  auto_ptr<CSeqref> key;
  int               locked;
  CTSE_Info        *tseinfop;
  
  TSeqids           m_SeqIds;
  CTSEUpload        m_upload;
  
  STSEinfo() : next(0), prev(0), key(0),locked(0),tseinfop(0),m_SeqIds(),m_upload()
    {
      //GBLOG_POST("new tse(" << (void*)this << ")");
    }
  ~STSEinfo()
    {
      //GBLOG_POST("delete tse(" << (void*)this << ")");
    }
};

struct CGBDataLoader::SSeqrefs
{
  typedef vector<CSeqref*> TV;
  
  class TSeqrefs : public TV {
  public:
    TSeqrefs() : TV(0) {}
    ~TSeqrefs() { ITERATE(TV,it,*this) delete *it; }
  };
  
  TSeqrefs       *m_Sr;
  CRefresher      m_Timer;
  SSeqrefs() : m_Sr(0) {}
  ~SSeqrefs()
  {
    if (m_Sr)
      delete m_Sr;
  }
};

//=======================================================================
// GBLoader Public interface 
// 

CGBDataLoader::CGBDataLoader(const string& loader_name, CReader *driver,
                             int gc_threshold)
  : CDataLoader(loader_name),
    m_Driver(driver)
{
  GBLOG_POST( "CGBDataLoader");
  
#if defined(HAVE_LIBDL) && defined(HAVE_PUBSEQ_OS)
  if(!m_Driver)
    {
      try
        {
          m_Driver= new CPubseqReader;
        }
      catch(exception& e)
        {
          GBLOG_POST("CPubseqReader is not available ::" << e.what());
          m_Driver=0;
        }
      catch(...)
        {
          LOG_POST("CPubseqReader:: unable to init ");
          m_Driver=0;
        }
    }
#endif
  if(!m_Driver) m_Driver=new CId1Reader;
  
  m_UseListHead = m_UseListTail = 0;
  
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
  ITERATE(TSeqId2Seqrefs,sih_it,m_Bs2Sr)
    {
      if(sih_it->second)
        {
          delete sih_it->second;
        }
    }
  ITERATE(TSr2TSEinfo,tse_it,m_Sr2TseInfo)
    delete tse_it->second;
  delete m_Driver;
}

bool
CGBDataLoader::GetRecords(const CHandleRangeMap& hrmap, const EChoice choice,
                          TTSESet* tse_set)
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
    switch(choice)
      {
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
#endif
  CGBLGuard g(m_Locks,s);
  do
    {
      unreleased_mutex_run=true;
      ITERATE (TLocMap, hrange, hrmap.GetMap() )
        {
          //GBLOG_POST( "GetRecords-0" );
          TSeq_id_Key  sih       = x_GetSeq_id_Key(hrange->first);
          if(x_GetRecords(sih,hrange->second,choice, tse_set))
            unreleased_mutex_run=false;
        }
      _VERIFY(count++ < 10); /// actually I would expect it to be 2 at most
    }
  while (unreleased_mutex_run!=true);
  //GBLOG_POST( "GetRecords-end" );
  return true;
}

#if !defined(_DEBUG)
inline
#endif
void
CGBDataLoader::x_Check(STSEinfo *_DEBUG_ARG(me))
{
#if defined(_DEBUG)
    unsigned c = 0;
    bool tse_found=false;
    STSEinfo *tse2 = m_UseListHead, *t1=0;
    while ( tse2 ) {
        c++;
        if( tse2 == me )
            tse_found = true;
        t1 = tse2;
        tse2 = tse2->next;
    }
    _VERIFY(t1 == m_UseListTail);
    _VERIFY(m_Sr2TseInfo.size() == m_TseCount);
    _VERIFY(c  <= m_TseCount);
    _VERIFY(m_Tse2TseInfo.size() <= m_TseCount);
    if(me) {
        //GBLOG_POST("check tse " << me << " by " << CThread::GetSelf() );
        _VERIFY(tse_found);
    }
#endif
}

bool
CGBDataLoader::DropTSE(const CSeq_entry *sep)
{
  CGBLGuard g(m_Locks,"drop_tse");
  TTse2TSEinfo::iterator it = m_Tse2TseInfo.find(sep);
  if (it == m_Tse2TseInfo.end()) // oops - apprently already done;
    return true;
  STSEinfo *tse = it->second;
  g.Lock(tse);
  _VERIFY(tse);
  x_Check(tse);
  
  m_Tse2TseInfo.erase(it);
  x_DropTSEinfo(tse);
  return true;
}

CTSE_Info*
CGBDataLoader::ResolveConflict(const CSeq_id_Handle& handle, const TTSESet& tse_set)
{
  TSeq_id_Key  sih       = x_GetSeq_id_Key(handle);
  SSeqrefs*    sr=0;
  CTSE_Info*   best=0;
  bool         conflict=false;
  
  GBLOG_POST( "ResolveConflict" );
  CGBLGuard g(m_Locks,"ResolveConflict");
  {
    int cnt = 20;
    while (!x_ResolveHandle(sih,sr) && cnt>0)
      cnt --;
    if(cnt==0)
      return 0;
  }
  ITERATE(TTSESet, sit, tse_set)
    {
      CTSE_Info *ti = const_cast<CTSE_Info*>(*sit);
      const CSeq_entry *sep = ti->m_TSE;
      TTse2TSEinfo::iterator it = m_Tse2TseInfo.find(sep);
      if(it==m_Tse2TseInfo.end()) continue;
      STSEinfo *tse = it->second;
      
      x_Check(tse);

      g.Lock(tse);

      if(tse->mode.test(STSEinfo::eDead) && !ti->m_Dead)
        ti->m_Dead = true;
      if(tse->m_SeqIds.find(sih)!=tse->m_SeqIds.end()) // listed for given TSE
        {
          if(!best)
            { best=ti; conflict=false; }
          else if(!ti->m_Dead && best->m_Dead)
            { best=ti; conflict=false; }
          else if(ti->m_Dead && best->m_Dead)
            { conflict=true; }
          else if(ti->m_Dead && !best->m_Dead)
            ;
          else
            {
              conflict=true;
              //_VERIFY(ti->m_Dead || best->m_Dead);
            }
        }
      g.Unlock(tse);
    }

  if(best && !conflict)
    return best;
  
  // try harder
  
  best=0;conflict=false;
  if (sr->m_Sr)
    {
      ITERATE (SSeqrefs::TSeqrefs, srp, *sr->m_Sr)
        {
          TSr2TSEinfo::iterator tsep = m_Sr2TseInfo.find(CCmpTSE(*srp));
          if (tsep == m_Sr2TseInfo.end()) continue;
          ITERATE(TTSESet, sit, tse_set)
            {
              CTSE_Info *ti = const_cast<CTSE_Info*>(*sit);;
              TTse2TSEinfo::iterator it = m_Tse2TseInfo.find(ti->m_TSE.GetPointer());
              if(it==m_Tse2TseInfo.end()) continue;
              if(it->second==tsep->second)
                {
                  if(!best)           best=ti;
                  else if(ti != best) conflict=true;
                }
            }
        }
    }
  if(conflict) best=0;
  return best;
}

//=======================================================================
// GBLoader private interface
// 

const CSeq_id*
CGBDataLoader::x_GetSeqId(const TSeq_id_Key h)
{
  return x_GetSeq_id(x_GetSeq_id_Handle(h));
};

void
CGBDataLoader::x_UpdateDropList(STSEinfo *tse)
{
  _VERIFY(tse);
  // reset LRU links
  if(tse == m_UseListTail) // already the last one
    return;
  
  // Unlink from current place
  if(tse->prev) tse->prev->next=tse->next;
  if(tse->next) tse->next->prev=tse->prev;
  if(tse == m_UseListHead )
    {
      _VERIFY(tse->next);
      m_UseListHead = tse->next;
    }
  tse->prev = tse->next=0;
  if(m_UseListTail)
    {
      tse->prev = m_UseListTail;
      m_UseListTail->next = tse;
      m_UseListTail = tse;
    }
  else
    {
      _VERIFY(m_UseListHead==0);
      m_UseListHead = m_UseListTail = tse; 
    }
  x_Check(0);
}

void
CGBDataLoader::x_DropTSEinfo(STSEinfo *tse)
{
  GBLOG_POST( "DropTse(" << tse <<")" );
  if(!tse) return;
  
  m_Sr2TseInfo.erase(CCmpTSE(tse->key.get()));
  ITERATE(STSEinfo::TSeqids,sih_it,tse->m_SeqIds)
    {
      TSeqId2Seqrefs::iterator bsit = m_Bs2Sr.find(*sih_it);
      if(bsit == m_Bs2Sr.end()) continue;
      // delete sih
      delete bsit->second;
      m_Bs2Sr.erase(bsit);
    }
  if(m_UseListHead==tse) m_UseListHead=tse->next;
  if(m_UseListTail==tse) m_UseListTail=tse->prev;
  if(tse->next) tse->next->prev=tse->prev;
  if(tse->prev) tse->prev->next=tse->next;
  delete tse;
  m_TseCount --;
  x_Check(0);
}

void
CGBDataLoader::GC(void)
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
    x_Check(0);

    unsigned skip=0;
    unsigned skip_max = (int)(0.1*m_TseCount + 1); /* scan 10% of least recently used pile before giving up */
    STSEinfo *cur_tse = m_UseListHead;
    while (cur_tse && skip<skip_max) {
        STSEinfo *tse_to_drop = cur_tse;
        cur_tse = cur_tse->next;
        ++skip;
        // fast checks
        if (tse_to_drop->locked) continue;
        if (tse_to_drop->tseinfop && tse_to_drop->tseinfop->Locked()) continue;

        const CSeq_entry *sep = tse_to_drop->m_upload.m_tse;
        if ( !sep ) {
            if (tse_to_drop->m_upload.m_mode != CTSEUpload::eNone) {
                g.Lock(tse_to_drop);
                GBLOG_POST("X_GC:: drop nonexistent tse " << tse_to_drop);
                x_DropTSEinfo(tse_to_drop);
                g.Unlock(tse_to_drop);
                --skip;
            }
            continue;
        }
        if(m_Tse2TseInfo.find(sep) == m_Tse2TseInfo.end()) continue;
        
#ifdef DEBUG_SYNC
        char b[100];
        GBLOG_POST("X_GC::DropTSE(" << tse_to_drop << "::" << tse_to_drop->key->printTSE(b,sizeof(b)) << ")");
#endif
        CConstRef<CSeq_entry> se(sep);
        g.Unlock();
        if(GetDataSource()->DropTSE(*se) ) {
            skip--;
            m_InvokeGC=false;
        }
        g.Lock();
#if defined(NCBI_THREADS)
        int i=0;
        for(cur_tse = m_UseListHead;cur_tse && i<skip; ++i) cur_tse = cur_tse->next ;
#endif
    }
    if(m_InvokeGC) { // nothing has been cleaned up
      //assert(m_TseGC_Threshhold<=m_TseCount); // GC entrance condition
      m_TseGC_Threshhold = m_TseCount+2; // do not even try until next load
    } else if(m_TseCount < 0.5*m_TseGC_Threshhold) {
      m_TseGC_Threshhold = (m_TseCount + m_TseGC_Threshhold)/2;
    }
}


CGBDataLoader::TInt
CGBDataLoader::x_Request2SeqrefMask(const EChoice choice)
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

CGBDataLoader::TInt
CGBDataLoader::x_Request2BlobMask(const EChoice choice)
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

bool
CGBDataLoader::x_GetRecords(const TSeq_id_Key sih,const CHandleRange &hrange,EChoice choice,
                            TTSESet* tse_set)
{
    TInt         sr_mask   = x_Request2SeqrefMask(choice);
    TInt         blob_mask = x_Request2BlobMask(choice);
    SSeqrefs*    sr=0;
    bool         global_mutex_was_released=false;

    //GBLOG_POST( "x_GetRecords" );
    if(!x_ResolveHandle(sih,sr))
        global_mutex_was_released=true;
  
    if(!sr->m_Sr) // no data for given seqid
        return global_mutex_was_released;
  
    //GBLOG_POST( "x_GetRecords-Seqref_iterate" );
    ITERATE (SSeqrefs::TSeqrefs, srp, *sr->m_Sr) {
        // skip TSE which doesn't contain requested type of info
        //GBLOG_POST( "x_GetRecords-Seqref_iterate_0" );
        if( ((~(*srp)->Flag()) & sr_mask) == 0 ) continue;
        //GBLOG_POST( "list uploaded TSE");
        // char b[100];
        //for(TSr2TSEinfo::iterator tsep = m_Sr2TseInfo.begin(); tsep != m_Sr2TseInfo.end(); ++tsep)
        //  {
        //     GBLOG_POST(tsep->first.get().printTSE(b,sizeof(b)));
        //  }
        // GBLOG_POST("x_GetRecords-Seqref_iterate_1" << (*srp)->print(b,sizeof(b)) );
      
        // find TSE info for each seqref
        TSr2TSEinfo::iterator tsep = m_Sr2TseInfo.find(CCmpTSE(*srp));
        STSEinfo *tse;
        if (tsep != m_Sr2TseInfo.end()) {
            tse = tsep->second;
            //GBLOG_POST( "x_GetRecords-oldTSE(" << tse << ") mode=" << (tse->m_upload.m_mode));
        }
        else {
            tse = new STSEinfo();
            tse->key.reset((*srp)->Dup());
            m_Sr2TseInfo[CCmpTSE(tse->key.get())] = tse;
            m_TseCount++;
            GBLOG_POST( "x_GetRecords-newTSE(" << tse << ") ");
        }

        CGBLGuard g(m_Locks,CGBLGuard::eMain,"x_GetRecords");
        g.Lock(tse);
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
                               *srp,
                               lrange->first.GetFrom(),
                               lrange->first.GetTo(),
                               blob_mask)
                )
                continue;
          
            _VERIFY(tse->m_upload.m_mode != CTSEUpload::eDone);

            //GBLOG_POST( "x_GetRecords-range_1" );
            // need update
            g.Local();
            global_mutex_was_released=true;

            int try_cnt=2;
            while(try_cnt-->0) {
                try {
                    new_tse = new_tse ||
                        x_GetData(tse,
                                  *srp,
                                  lrange->first.GetFrom(),
                                  lrange->first.GetTo(),
                                  blob_mask,
                                  tse_set
                            );
                    break;
                }
                catch(const CIOException &e) {
                    LOG_POST("ID connection failed: Reconnecting...." << e.what());
                }
                catch(const CDB_Exception &e) {
                    LOG_POST("ID connection failed: Reconnecting...." << e.what());
                }
                catch ( const exception &e ) {
                    LOG_POST(CThread::GetSelf() << ":: Data request failed...." << e.what());
                    g.Lock();
                    g.Lock(tse);
                    tse->locked--;
                    x_UpdateDropList(tse); // move up as just checked
                    throw;
                }
                catch (...) {
                    LOG_POST(CThread::GetSelf() << ":: Data request failed....");
                    g.Lock();
                    g.Lock(tse);
                    tse->locked--;
                    x_UpdateDropList(tse); // move up as just checked
                    throw;
                }
            }
            if(try_cnt<0) {
                LOG_POST("CGBLoader:GetData: Data Request failed :: exceeded maximum attempts count");
                g.Lock();
                g.Lock(tse);
                tse->locked--;
                x_UpdateDropList(tse); // move up as just checked
                throw runtime_error("CGBLoader:GetData: Multiple attempts to retrieve data failed");
            }
        }
        g.Lock();
        g.Lock(tse);
        x_UpdateDropList(tse);
        if(new_tse) {
            x_Check(0);
            _VERIFY(tse->m_upload.m_tse);
            _ASSERT(m_Tse2TseInfo.find(tse->m_upload.m_tse) == m_Tse2TseInfo.end());
            m_Tse2TseInfo[tse->m_upload.m_tse]=tse; // insert
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


bool
CGBDataLoader::x_ResolveHandle(const TSeq_id_Key h,SSeqrefs* &sr)
{
  sr=0;
  TSeqId2Seqrefs::iterator bsit = m_Bs2Sr.find(h);
  if (bsit == m_Bs2Sr.end() )
    sr = m_Bs2Sr[h] = new SSeqrefs();
  else
    sr = bsit->second;

  CGBLGuard g(m_Locks,CGBLGuard::eMain,"x_ResolveHandle");
  g.Lock(sr);
  if(!sr->m_Timer.NeedRefresh(m_Timer)) return true;
  g.Local();
  //GBLOG_POST( "ResolveHandle-before(" << h << ") " << (sr->m_Sr?sr->m_Sr->size():0) );
  
  
  SSeqrefs::TSeqrefs *osr=sr->m_Sr;
  sr->m_Sr=0;
  int try_cnt=2;
  while(try_cnt-->0)
    {
      CTimerGuard tg(m_Timer);
      try
        {
          for(CIStream srs(m_Driver->SeqrefStreamBuf(*x_GetSeqId(h),
                                                     m_Locks.m_Pool.Select(sr)));
              ! srs.Eof() && srs; )
            {
              CSeqref *seqRef = m_Driver->RetrieveSeqref(srs);
              if(!sr->m_Sr) sr->m_Sr = new SSeqrefs::TSeqrefs();
              sr->m_Sr->push_back(seqRef);
            }
          break;
        }
      catch(const CIOException &e)
        {
          LOG_POST(e.what());
          LOG_POST("ID connection failed: Reconnecting....");
        }
      catch(const CDB_Exception &e)
        {
          LOG_POST(e.what());
          LOG_POST("ID connection failed: Reconnecting....");
        }
    }
  if(try_cnt<0)
    {
      sr->m_Sr=osr;
      osr=0;
      throw runtime_error("Network trouble - failed to connect to NCBI ID services");
    }
  sr->m_Timer.Reset(m_Timer);
  
  g.Lock(); // will unlock everything and lock lookupMutex again 
  
  GBLOG_POST( "ResolveHandle(" << h << ") " << (sr->m_Sr?sr->m_Sr->size():0) );
  if(sr->m_Sr)
    {
      ITERATE(SSeqrefs::TSeqrefs, srp, *(sr->m_Sr))
        {
          char b[100];
          b[0] = 0;
          b[0] = b[0];
          GBLOG_POST( (*srp)->print(b,sizeof(b)));
        }
    }
  
  if(osr)
    {
      bsit = m_Bs2Sr.find(h); // make sure we are not deleted in the unlocked time 
      if (bsit != m_Bs2Sr.end())
        {
          SSeqrefs::TSeqrefs *nsr=bsit->second->m_Sr;
          
          // catch dissolving TSE and mark them dead
          //GBLOG_POST( "old seqrefs");
          ITERATE(SSeqrefs::TSeqrefs,srp,*osr)
            {
              //(*srp)->print(); cout);
              bool found=false;
              ITERATE(SSeqrefs::TSeqrefs,nsrp,*nsr)
                if(CCmpTSE(*srp)==CCmpTSE(*nsrp)) {found=true; break;}
              if(found) continue;
              TSr2TSEinfo::iterator tsep = m_Sr2TseInfo.find(CCmpTSE(*srp));
              if (tsep == m_Sr2TseInfo.end()) continue;
              
              // update TSE info 
              STSEinfo *tse = tsep->second;
              g.Lock(tse);
              bool mark_dead  = tse->mode.test(STSEinfo::eDead);
              if(mark_dead) tse->mode.set(STSEinfo::eDead);
              tse->m_SeqIds.erase(h); // drop h as refewrenced seqid
              g.Unlock(tse);
              if(mark_dead && tse->m_upload.m_tse)
                { // inform data_source :: make sure to avoid deadlocks
                  GetDataSource()->x_UpdateTSEStatus(*(tse->m_upload.m_tse),true);
                }
            }
        }
      delete osr;
    }
  return false;
}

bool
CGBDataLoader::x_NeedMoreData(CTSEUpload *tse_up,CSeqref* srp,int from,int to,TInt blob_mask)
{
  bool need_data=true;
  
  if (tse_up->m_mode==CTSEUpload::eDone)
    need_data=false;
  if (tse_up->m_mode==CTSEUpload::ePartial)
    {
      // split code : check tree for presence of data and
      // return from routine if all data already loaded
      // present;
    }
  //GBLOG_POST( "x_NeedMoreData(" << srp << "," << tse_up << ") need_data " << need_data <<);
  return need_data;
}

bool
CGBDataLoader::x_GetData(STSEinfo *tse,CSeqref* srp,int from,int to,TInt blob_mask,
                         TTSESet* tse_set)
{
  CTSEUpload *tse_up = &tse->m_upload;
  if(!x_NeedMoreData(tse_up,srp,from,to,blob_mask))
    return false;
  bool new_tse = false;
  char s[100];
  GBLOG_POST( "GetBlob(" << srp->printTSE(s,sizeof(s)) << "," << tse_up << ") " <<
              from << ":"<< to << ":=" << tse_up->m_mode);
  
  _VERIFY(tse_up->m_mode != CTSEUpload::eDone);
  if (tse_up->m_mode == CTSEUpload::eNone)
    {
      CBlobClass cl;
      int count=0;
      cl.Value() = blob_mask;
      for(CIStream bs(srp->BlobStreamBuf(from, to, cl,
                                         m_Locks.m_Pool.Select(tse)));
          !bs.Eof() && bs; ++count)
        {
          auto_ptr<CBlob> blob(srp->RetrieveBlob(bs));
          GBLOG_POST( "GetBlob(" << srp << ") " << from << ":"<< to << "  class("<<blob->Class()<<")");
          m_InvokeGC=true;
          if (blob->Class()==0)
            {
              tse_up->m_tse    = blob->Seq_entry();
              if(tse_up->m_tse)
                {
                  tse_up->m_mode   = CTSEUpload::eDone;
                  GBLOG_POST( "GetBlob(" << s << ") " << "- whole blob retrieved");
                  new_tse=true;
                  tse->tseinfop=GetDataSource()->AddTSE(*(tse_up->m_tse), tse_set);
                }
              else
                {
                  tse_up->m_mode   = CTSEUpload::eDone;
                  LOG_POST( "GetBlob(" <<  ") " << "- retrieval of the whole blob failed - no data available");
                }
            }
          else
            {
              GBLOG_POST("GetBlob(" << s << ") " << "- partial load");
              _ASSERT(tse_up->m_mode != CTSEUpload::eDone);
              if(tse_up->m_mode != CTSEUpload::ePartial)
                tse_up->m_mode = CTSEUpload::ePartial;
              // split code : upload tree
              _VERIFY(0);
            }
        }
      if(count==0)
        {
          tse_up->m_mode = CTSEUpload::eDone;
          // TODO log message
          LOG_POST("ERROR: can not retrive sequence  : " << srp->print(s,sizeof(s)));
        }
      
      //GBLOG_POST( "GetData-after:: " << from << to <<  endl;
    }
  else
    {
      _VERIFY(tse_up->m_mode==CTSEUpload::ePartial);
      _VERIFY(0);
    }
  return new_tse;
}

void
CGBDataLoader::DebugDump(CDebugDumpContext ddc, unsigned int depth) const
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
