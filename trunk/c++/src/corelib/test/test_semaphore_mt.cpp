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
* Author:  Andrei Gourianov, gouriano@ncbi.nlm.nih.gov
*
* File Description:
*   Test CSemaphore class in multithreaded environment
*   in order to run correctly the number of threads MUST be even!
*
*   the test is a very simple producer/consumer model
*   one thread produces "items" (increments integer counter)
*	next thread consumes the same amount of items (decrements integer counter)
*	"Content" semaphore is used to notify consumers of how many items are
*   available.
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 6.1  2001/12/10 18:37:29  gouriano
* *** empty log message ***
*
* ===========================================================================
*/

#include <corelib/ncbidiag.hpp>
#include <corelib/ncbithr.hpp>
#include "test_mt.hpp"

USING_NCBI_SCOPE;

/////////////////////////////////////////////////////////////////////////////
//  Test application

class CTestSemaphoreApp : public CThreadedApp
{
public:
    virtual bool Thread_Init(int idx);
    virtual bool Thread_Run(int idx);
protected:
    virtual bool TestApp_Init(void);
    virtual bool TestApp_Exit(void);

private:
    void Produce( int Num);
    void Consume( int Num);
    void Sleep(void);

    static CSemaphore s_semContent, s_semStorage;
    static int s_Counter, s_Id;
};

/////////////////////////////////////////////////////////////////////////////

CSemaphore CTestSemaphoreApp::s_semContent(0,20);
CSemaphore CTestSemaphoreApp::s_semStorage(1,1);
int CTestSemaphoreApp::s_Counter=0;
int CTestSemaphoreApp::s_Id=0;

/////////////////////////////////////////////////////////////////////////////
//  implementation

void CTestSemaphoreApp::Produce( int Num)
// produce Num items
{
// Storage semaphore acts as a kind of mutex - its only purpose
// is to protect Counter
    s_semStorage.Wait();
    s_Counter += Num;
    NcbiCout << "+" << Num << "=" << s_Counter << NcbiEndl;
    s_semStorage.Post();

// Content semaphore notifies consumer threads of how many items can be
// consumed. Slow consumption with fast production causes Content semaphore
// to overflow from time to time. We catch exception and wait for consumers
// to consume something
    for (bool Posted=false; !Posted;)
    {
        try
        {
            s_semContent.Post( Num);
            Posted = true;
        }
        catch (exception& e)
        {
            NcbiCout << e.what() << NcbiEndl;
#if defined(NCBI_OS_MSWIN)
    		::Sleep(500);
#else
//            ::sleep(1);
#endif
        }
    }
}

void CTestSemaphoreApp::Consume( int Num)
// consume Num items
{
    for (int i = Num; i > 0; --i )
    {
// we can only consume one by one
        s_semContent.Wait();
        s_semStorage.Wait();
        --s_Counter;
        NcbiCout << "-1=" << s_Counter << NcbiEndl;
        s_semStorage.Post();

#if defined(NCBI_OS_MSWIN)
		::Sleep(500);
#else
//        Sleep();
//        ::sleep(1);
#endif
    }
}

void CTestSemaphoreApp::Sleep(void)
{
    // do some silly stuff
    char cBuf[32];
    for (int j = 20000; j > 0; --j)
    {
        strcpy(cBuf, "blahblah");
        strcpy(cBuf, "");
    }
}

bool CTestSemaphoreApp::Thread_Init(int idx)
{
    return true;
}

bool CTestSemaphoreApp::Thread_Run(int idx)
{
//  one thread produces, next - consumes
//  production is fast, consumption is slow (because of Sleep)

//  in order to run correctly the number of threads MUST be even!

    if ( idx % 2 != 1)
    {
        Consume( (idx/2)%3 + 1);
    }
    else
    {
        Produce( (idx/2)%3 + 1);
    }
    return true;
}

bool CTestSemaphoreApp::TestApp_Init(void)
{
    NcbiCout
        << NcbiEndl
        << "Testing semaphores with "
        << NStr::IntToString(s_NumThreads)
        << " threads"
        << NcbiEndl;
    return true;
}

bool CTestSemaphoreApp::TestApp_Exit(void)
{
    NcbiCout
        << "Test completed"
        << NcbiEndl
        << " counter = " << s_Counter
        << NcbiEndl;
// storage must be available
    _ASSERT( s_semStorage.TryWait());
// content must be empty
    _ASSERT( !s_semContent.TryWait());
	_ASSERT( s_Counter == 0);
    return true;
}



/////////////////////////////////////////////////////////////////////////////
//  MAIN

int main(int argc, const char* argv[]) 
{
    return CTestSemaphoreApp().AppMain(argc, argv, 0, eDS_Default, 0);
}
