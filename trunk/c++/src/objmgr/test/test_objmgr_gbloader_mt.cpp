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
* Author:
*           Andrei Gourianov, Michael Kimelman
*
* File Description:
*           Basic test of GenBank data loader
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.14  2002/06/04 17:18:33  kimelman
* memory cleanup :  new/delete/Cref rearrangements
*
* Revision 1.13  2002/05/08 22:23:49  kimelman
* MT fixes
*
* Revision 1.12  2002/05/06 03:28:53  vakatov
* OM/OM1 renaming
*
* Revision 1.11  2002/04/18 23:24:24  kimelman
* bugfix: out of bounds...
*
* Revision 1.10  2002/04/12 22:57:34  kimelman
* warnings cleanup(linux-gcc)
*
* Revision 1.9  2002/04/12 21:10:35  kimelman
* traps for coredumps
*
* Revision 1.8  2002/04/11 20:03:29  kimelman
* switch to pubseq
*
* Revision 1.7  2002/04/09 18:48:17  kimelman
* portability bugfixes: to compile on IRIX, sparc gcc
*
* Revision 1.6  2002/04/05 23:47:20  kimelman
* playing around tests
*
* Revision 1.5  2002/04/05 19:53:13  gouriano
* reset scope history more accurately (was incorrect)
*
* Revision 1.4  2002/04/04 01:35:38  kimelman
* more MT tests
*
* Revision 1.3  2002/04/02 17:24:54  gouriano
* skip useless test passes
*
* Revision 1.2  2002/04/02 16:02:33  kimelman
* MT testing
*
* Revision 1.1  2002/03/30 19:37:08  kimelman
* gbloader MT test
*
* Revision 1.9  2002/03/26 17:24:58  grichenk
* Removed extra ++i
*
* Revision 1.8  2002/03/26 15:40:31  kimelman
* get rid of catch clause
*
* Revision 1.7  2002/03/26 00:15:52  vakatov
* minor beautification
*
* Revision 1.6  2002/03/25 15:44:47  kimelman
* proper logging and exception handling
*
* Revision 1.5  2002/03/22 21:53:07  kimelman
* bugfix: skip missed gi's
*
* Revision 1.4  2002/03/21 19:15:53  kimelman
* GB related bugfixes
*
* Revision 1.3  2002/03/21 19:14:55  kimelman
* GB related bugfixes
*
* Revision 1.2  2002/03/21 16:18:21  gouriano
* *** empty log message ***
*
* Revision 1.1  2002/03/20 21:25:00  gouriano
* *** empty log message ***
*
* ===========================================================================
*/

#include <corelib/ncbiapp.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/seqloc/Seq_id.hpp>

#include <objects/objmgr/object_manager.hpp>
#include <objects/objmgr/scope.hpp>
#include <objects/objmgr/gbloader.hpp>
#include <objects/objmgr/reader_id1.hpp>
#include <objects/objmgr/reader_pubseq.hpp>

#include <serial/serial.hpp>
#include <serial/objostrasn.hpp>


BEGIN_NCBI_SCOPE
using namespace objects;

/////////////////////////////////////////////////////////////////////////////
//
//  Test thread
//

class CTestThread : public CThread
{
public:
  CTestThread(unsigned id, CObjectManager& objmgr, CScope& scope,int start,int stop);
  virtual ~CTestThread();
  
protected:
    virtual void* Main(void);

private:
  unsigned        m_mode;
  CScope         *m_Scope;
  CObjectManager *m_ObjMgr;
  int             m_Start;
  int             m_Stop;
};

CTestThread::CTestThread(unsigned id, CObjectManager& objmgr, CScope& scope,int start,int stop)
    : m_mode(id), m_Scope(&scope), m_ObjMgr(&objmgr), m_Start(start), m_Stop(stop)
{
  LOG_POST("Thread " << start << " - " << stop << " - started");
}

CTestThread::~CTestThread()
{
  LOG_POST("Thread " << m_Start << " - " << m_Stop << " - completed");
}

void* CTestThread::Main(void)
{
  CObjectManager om;
  CObjectManager *pom=0;
  switch((m_mode>>2)&1) {
  case 1: pom =  &*m_ObjMgr; break;
  case 0:
    pom =  &om;
    om.RegisterDataLoader(*new CGBDataLoader("ID",0,2),CObjectManager::eDefault);
    break;
  }
  
  CScope scope1(*pom);
  scope1.AddDefaults();

  LOG_POST(" Processing gis from "<< m_Start << " to " << m_Stop);
  
  for (int i = m_Start;  i < m_Stop;  i++) {
    CScope scope2(*pom);
    scope2.AddDefaults();

    CScope *s;
    switch(m_mode&3) {
    case 2: s =  &*m_Scope; break;
    case 1: s =  &scope1;   break;
    case 0: s =  &scope2;   break;
    default:
      throw runtime_error("unexpected mode");
    }
    
    int gi = i ; // (i + m_Idx)/2+3;
    CSeq_id x;
    x.SetGi(gi);
    CBioseq_Handle h = s->GetBioseqHandle(x);
    if ( !h ) {
      LOG_POST(CThread::GetSelf() << ":: gi=" << gi << " :: not found in ID");
    } else {
      //CObjectOStreamAsn oos(NcbiCout);
      iterate (list<CRef<CSeq_id> >, it, h.GetBioseq().GetId()) {
        //oos << **it;
        //NcbiCout << NcbiEndl;
               ;
      }
      LOG_POST(CThread::GetSelf() << ":: gi=" << gi << " OK");
    }
    s->ResetHistory();
  }
  return 0;
}

/////////////////////////////////////////////////////////////////////////////
//
//  CTestApplication::
//


class CTestApplication : public CNcbiApplication
{
public:
    virtual int Run(void);
    int Test(const unsigned test_id,const unsigned thread_count);
};

const unsigned c_TestFrom = 1;
const unsigned c_TestTo   = 201;
const unsigned c_GI_count = c_TestTo - c_TestFrom;

int CTestApplication::Test(const unsigned test_mode,const unsigned thread_count)
{
  int step= c_GI_count/thread_count;
  CObjectManager Om;
  {
    CScope         scope(Om);
    CTestThread  **thr = new (CTestThread*)[thread_count];
  
    // CRef< CGBDataLoader> pLoader = new CGBDataLoader;
    // pOm->RegisterDataLoader(*pLoader, CObjectManager::eDefault);
    if(((test_mode>>2)&1)==1)
      {
        Om.RegisterDataLoader(*new CGBDataLoader("ID", 0,1+2*thread_count),
                              CObjectManager::eDefault);
        scope.AddDefaults();
      }
    
    for (unsigned i=0; i<thread_count; ++i)
      {
        thr[i] = new CTestThread(test_mode, Om, scope,c_TestFrom+i*step,c_TestFrom+(i+1)*step);
        thr[i]->Run();
      }
    
    for (unsigned i=0; i<thread_count; i++) {
      LOG_POST("Thread " << i << " @join");
      thr[i]->Join();
    }
    
#if 0 
    // Destroy all threads : has already been destroyed by join
    for (unsigned i=0; i<thread_count; i++) {
      LOG_POST("Thread " << i << " @delete");
      delete thr[i];
    }
#endif
    
    delete [] thr;
  }
  return 0;
}

int CTestApplication::Run()
{
  unsigned timing[4/*threads*/][2/*om*/][3/*scope*/];
  unsigned tc = sizeof(timing)/sizeof(*timing);

  memset(timing,0,sizeof(timing));
  
  CORE_SetLOCK(MT_LOCK_cxx2c());
  CORE_SetLOG(LOG_cxx2c());

  {
    time_t x = time(0);
    LOG_POST("START: " << time(0) );
#if defined(HAVE_PUBSEQ_OS)
    {
      CGBDataLoader preload_ctlib_("ID", 0,1);
    }
    LOG_POST("CTLIB loaded: " << time(0)-x  );
#endif
  }
  
  for(unsigned thr=tc,i=0 ; thr > 0 ; --thr)
    for(unsigned global_om=0;global_om<=(thr>1U?1U:0U); ++global_om)
      for(unsigned global_scope=0;global_scope<=(thr==1U?1U:(global_om==0U?1U:2U)); ++global_scope)
        {
          unsigned mode = (global_om<<2) + global_scope ;
          LOG_POST("TEST: threads:" << thr << ", om=" << (global_om?"global":"local ") <<
                   ", scope=" << (global_scope==0?"auto      ":(global_scope==1?"per thread":"global    ")));
          time_t start=time(0);
          Test(mode,thr);
          timing[thr-1][global_om][global_scope] = time(0)-start ;
          LOG_POST("==================================================");
          LOG_POST("Test(" << i++ << ") completed  ===============");
        }
  
  for(unsigned global_om=0;global_om<=1; ++global_om)
    for(unsigned global_scope=0;global_scope<=2; ++global_scope)
      for(unsigned thr=0; thr < tc ; ++thr)
        {
          LOG_POST("TEST: threads:" << thr+1 << ", om=" << (global_om?"global":"local ") <<
                   ", scope=" << (global_scope==0?"auto      ":(global_scope==1?"per thread":"global    ")) <<
                   " ==>> " << timing[thr][global_om][global_scope] << " sec");
        }
  
  LOG_POST("Tests completed");
  return 0;
}

END_NCBI_SCOPE



/////////////////////////////////////////////////////////////////////////////
//
//  MAIN
//


USING_NCBI_SCOPE;

int main(int argc, const char* argv[])
{
  return CTestApplication().AppMain(argc, argv, 0, eDS_Default, 0);
}

