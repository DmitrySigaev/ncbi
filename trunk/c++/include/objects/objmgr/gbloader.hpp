#ifndef GBLOADER__HPP_INCLUDED
#define GBLOADER__HPP_INCLUDED

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
*  ===========================================================================
*
*  Author: Aleksey Grichenko, Michael Kimelman, Anton Butanayev
*
*  File Description:
*   Data loader base class for object manager
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <corelib/ncbithr.hpp>
#include <objects/seqset/Seq_entry.hpp>

#include <map>
#include <time.h>

#if defined(NCBI_OS_MAC)
#   include <types.h>
#else
#   include <sys/types.h>
#endif

#include <objects/objmgr/data_loader.hpp>
#include <objects/objmgr/reader.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

#if !defined(NDEBUG) && defined(DEBUG_SYNC)
#  if defined(_REENTRANT)
#    define GBLOG_POST(x) LOG_POST(setw(3) << CThread::GetSelf() << ":: " << x)
#  else
#    define GBLOG_POST(x) LOG_POST("0:: " << x)
#  endif 
#else
#  ifdef DEBUG_SYNC
#    undef DEBUG_SYNC
#  endif
#  define GBLOG_POST(x)
#endif
  
/////////////////////////////////////////////////////////////////////////////////
//
// GBDataLoader
//

class CHandleRange;

class NCBI_XOBJMGR_EXPORT CTimer
{
public:
  CTimer();
  time_t Time();
  void   Start();
  void   Stop();
  time_t RetryTime();
  bool   NeedCalibration();
private:
  time_t m_ReasonableRefreshDelay;
  int    m_RequestsDevider;
  int    m_Requests;
  CMutex m_RequestsLock;
  time_t m_Time;
  time_t m_LastCalibrated;
  
  time_t m_StartTime;
  CMutex m_TimerLock;
};

//========================================================================
class NCBI_XOBJMGR_EXPORT CRefresher
{
public:
  CRefresher() : m_RefreshTime(0) { };
  bool NeedRefresh(CTimer &timer) { return timer.Time() > m_RefreshTime; };
  void Reset      (CTimer &timer) { m_RefreshTime = timer.RetryTime();   };
private:
  time_t m_RefreshTime;
};

class NCBI_XOBJMGR_EXPORT CMutexPool
{
  int         m_size;
  CMutex     *m_Locks;
  int        *spread;
public:
  CMutexPool();
  ~CMutexPool(void);
  void SetSize(int size);
  CMutex& GetMutex(int x) { int y=x%m_size; spread[y]++; return m_Locks[y]; }
  template<class A> int  Select(A *a) { return (((unsigned long) a)/sizeof(A)) % m_size ; }
};


class NCBI_XOBJMGR_EXPORT CTSEUpload {
public:
  enum EChoice { eNone,eDone,ePartial };
  CTSEUpload() : m_mode(eNone), m_tse(0) {};
  EChoice                       m_mode;
  CSeq_entry                   *m_tse;
};

class NCBI_XOBJMGR_EXPORT CGBDataLoader : public CDataLoader
{
public:
  struct SLeveledMutex
  {
    unsigned        m_SlowTraverseMode;
    CMutex          m_Lookup;
    CMutexPool      m_Pool;
  };

  CGBDataLoader(const string& loader_name="GENBANK",CReader *driver=0,int gc_threshold=100);
  virtual ~CGBDataLoader(void);
  
  virtual bool DropTSE(const CSeq_entry* sep);
  virtual bool GetRecords(const CHandleRangeMap& hrmap, const EChoice choice,
      TTSESet* tse_set = 0);
  
  virtual CTSE_Info*
  ResolveConflict(const CSeq_id_Handle& handle,
                  const TTSESet&        tse_set);
  
  virtual void GC(void);
  virtual void DebugDump(CDebugDumpContext ddc, unsigned int depth) const;
  
private:
  class CCmpTSE
  {
    private:
      CSeqref *m_sr;
    public:
      CCmpTSE(CSeqref *sr)           : m_sr(sr)        {};
      CCmpTSE(auto_ptr<CSeqref> &asr): m_sr(asr.get()) {};
      ~CCmpTSE(void) {};
      operator bool  (void)       const { return m_sr!=0;};
      bool operator< (const CCmpTSE &b) const { return m_sr->Compare(*b.m_sr,CSeqref::eTSE)< 0;};
      bool operator==(const CCmpTSE &b) const { return m_sr->Compare(*b.m_sr,CSeqref::eTSE)==0;};
      bool operator<=(const CCmpTSE &b) const { return *this == b && *this < b ;};
      CSeqref& get() const { return *m_sr; };
  };
  struct STSEinfo;
  struct SSeqrefs;
  
  typedef CIntStreamable::TInt             TInt;
  
  typedef map<CCmpTSE          ,STSEinfo*> TSr2TSEinfo   ;
  typedef map<const CSeq_entry*,STSEinfo*> TTse2TSEinfo  ;
  typedef map<TSeq_id_Key     , SSeqrefs*> TSeqId2Seqrefs;
  
  CReader        *m_Driver;
  TSr2TSEinfo     m_Sr2TseInfo;
  TTse2TSEinfo    m_Tse2TseInfo;
  
  TSeqId2Seqrefs  m_Bs2Sr;
  
  CTimer          m_Timer;
  
  SLeveledMutex   m_Locks;
  
  STSEinfo       *m_UseListHead;
  STSEinfo       *m_UseListTail;
  unsigned        m_TseCount;
  unsigned        m_TseGC_Threshhold;
  bool            m_InvokeGC;
  void            x_UpdateDropList(STSEinfo *p);
  void            x_DropTSEinfo(STSEinfo *tse);
  
  //
  // private code
  //
  const CSeq_id*  x_GetSeqId(const TSeq_id_Key h);
  
  TInt            x_Request2SeqrefMask(const EChoice choice);
  TInt            x_Request2BlobMask(const EChoice choice);
  
  
  typedef map<CSeq_id_Handle,CHandleRange> TLocMap;
  bool            x_GetRecords(const TSeq_id_Key key,const CHandleRange &hrange, EChoice choice,
      TTSESet* tse_set);
  bool            x_ResolveHandle(const TSeq_id_Key h,SSeqrefs* &sr);
  bool            x_NeedMoreData(CTSEUpload *tse_up,CSeqref* srp,int from,int to,TInt blob_mask);
  bool            x_GetData(STSEinfo *tse,CSeqref* srp,int from,int to,TInt blob_mask,
      TTSESet* tse_set);
  void            x_Check(STSEinfo *me);
};


END_SCOPE(objects)
END_NCBI_SCOPE

#endif
/* ---------------------------------------------------------------------------
 *
 * $Log$
 * Revision 1.27  2002/12/26 20:51:35  dicuccio
 * Added Win32 export specifier
 *
 * Revision 1.26  2002/07/23 15:31:18  kimelman
 * fill statistics for MutexPool
 *
 * Revision 1.25  2002/07/22 22:53:20  kimelman
 * exception handling fixed: 2level mutexing moved to Guard class + added
 * handling of confidential data.
 *
 * Revision 1.24  2002/07/19 18:36:14  lebedev
 * NCBI_OS_MAC: include path changed for types.h
 *
 * Revision 1.23  2002/07/08 20:50:56  grichenk
 * Moved log to the end of file
 * Replaced static mutex (in CScope, CDataSource) with the mutex
 * pool. Redesigned CDataSource data locking.
 *
 * Revision 1.22  2002/06/04 17:18:32  kimelman
 * memory cleanup :  new/delete/Cref rearrangements
 *
 * Revision 1.21  2002/05/14 20:06:23  grichenk
 * Improved CTSE_Info locking by CDataSource and CDataLoader
 *
 * Revision 1.20  2002/05/09 21:41:01  kimelman
 * MT tuning
 *
 * Revision 1.19  2002/05/06 03:30:35  vakatov
 * OM/OM1 renaming
 *
 * Revision 1.18  2002/05/03 21:28:01  ucko
 * Introduce T(Signed)SeqPos.
 *
 * Revision 1.17  2002/04/11 02:09:52  vakatov
 * Get rid of a warning
 *
 * Revision 1.16  2002/04/09 19:04:21  kimelman
 * make gcc happy
 *
 * Revision 1.15  2002/04/09 18:48:14  kimelman
 * portability bugfixes: to compile on IRIX, sparc gcc
 *
 * Revision 1.14  2002/04/09 15:53:42  kimelman
 * turn off debug messages
 *
 * Revision 1.13  2002/04/08 23:09:23  vakatov
 * CMutexPool::Select()  -- fixed for 64-bit compilation
 *
 * Revision 1.12  2002/04/05 23:47:17  kimelman
 * playing around tests
 *
 * Revision 1.11  2002/04/04 01:35:33  kimelman
 * more MT tests
 *
 * Revision 1.10  2002/04/02 16:02:28  kimelman
 * MT testing
 *
 * Revision 1.9  2002/03/30 19:37:05  kimelman
 * gbloader MT test
 *
 * Revision 1.8  2002/03/29 02:47:01  kimelman
 * gbloader: MT scalability fixes
 *
 * Revision 1.7  2002/03/27 20:22:31  butanaev
 * Added connection pool.
 *
 * Revision 1.6  2002/03/26 15:39:24  kimelman
 * GC fixes
 *
 * Revision 1.5  2002/03/21 01:34:49  kimelman
 * gbloader related bugfixes
 *
 * Revision 1.4  2002/03/20 21:26:23  gouriano
 * *** empty log message ***
 *
 * Revision 1.3  2002/03/20 19:06:29  kimelman
 * bugfixes
 *
 * Revision 1.2  2002/03/20 17:04:25  gouriano
 * minor changes to make it compilable on MS Windows
 *
 * Revision 1.1  2002/03/20 04:50:35  kimelman
 * GB loader added
 *
 */
