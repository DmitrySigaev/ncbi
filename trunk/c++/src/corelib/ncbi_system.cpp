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
* Author:  Vladimir Ivanov
*
* File Description:
*
*   System functions:
*      SetHeapLimit()
*      SetCpuTimeLimit()
*      
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.4  2001/07/04 20:03:37  vakatov
* Added missing header <unistd.h>.
* Check for the exit code of the signal() function calls.
*
* Revision 1.3  2001/07/02 21:33:07  vakatov
* Fixed against SIGXCPU during the signal handling.
* Increase the amount of reserved memory for the memory limit handler
* to 10K (to fix for the 64-bit WorkShop compiler).
* Use standard C++ arg.processing (ncbiargs) in the test suite.
* Cleaned up the code. Get rid of the "Ncbi_" prefix.
*
* Revision 1.2  2001/07/02 18:45:14  ivanov
* Added #include <sys/resource.h> and extern "C" to handlers
*
* Revision 1.1  2001/07/02 16:45:35  ivanov
* Initialization
*
* ===========================================================================
*/

#include <corelib/ncbireg.hpp>
#include <corelib/ncbimtx.hpp>
#include <corelib/ncbitime.hpp>


// Define HEAP_LIMIT and CPU_LIMIT (UNIX only)
#ifdef NCBI_OS_UNIX
#  include <sys/resource.h>
#  include <sys/times.h>
#  include <limits.h>
#  include <unistd.h>
#  define USE_SETHEAPLIMIT
#  define USE_SETCPULIMIT
#endif

#ifdef USE_SETCPULIMIT
#  include <signal.h>
#endif


BEGIN_NCBI_SCOPE


//---------------------------------------------------------------------------


#ifdef NCBI_OS_UNIX

enum EExitCode {
    eEC_None,
    eEC_Memory,
    eEC_Cpu
};


static CFastMutex s_ExitHandler_Mutex;
static bool       s_ExitHandlerIsSet = false;
static EExitCode  s_ExitCode         = eEC_None;
static CTime      s_TimeSet;
static size_t     s_HeapLimit        = 0;
static size_t     s_CpuTimeLimit     = 0;
static char*      s_ReserveMemory    = 0;


extern "C" {
    static void s_ExitHandler(void);
    static void s_SignalHandler(int sig);
}



/* Routine to be called at the exit from application
 */

static void s_ExitHandler(void)
{
    CFastMutexGuard LOCK(s_ExitHandler_Mutex);

    // Free reserved memory
    if ( s_ReserveMemory ) {
        delete[] s_ReserveMemory;
        s_ReserveMemory = 0;
    }

    switch(s_ExitCode) {

    case eEC_Memory:
        {
            ERR_POST("Memory heap limit exceeded in allocating memory " \
                     "with operator new (" << s_HeapLimit << " bytes)");
            break;
        }

    case eEC_Cpu: 
        {
            ERR_POST("CPU time limit exceeded (" << s_CpuTimeLimit << " sec)");
            tms buffer;
            if ( times(&buffer) == (clock_t)(-1) ) {
                ERR_POST("Error in getting CPU time consumed with program");
                break;
            }
            LOG_POST("\tuser CPU time   : " << 
                     buffer.tms_utime/CLK_TCK << " sec");
            LOG_POST("\tsystem CPU time : " << 
                     buffer.tms_stime/CLK_TCK << " sec");
            LOG_POST("\ttotal CPU time  : " << 
                     (buffer.tms_stime + buffer.tms_utime)/CLK_TCK << " sec");
            break;
        }

    default:
        break;
    }
    
    // Write program's time
    CTime ct(CTime::eCurrent);
    CTime et(2000, 1, 1);
    et.AddSecond((int) (ct.GetTimeT() - s_TimeSet.GetTimeT()));
    LOG_POST("Program's time: " << Endm <<
             "\tstart limit - " << s_TimeSet.AsString() << Endm <<
             "\ttermination - " << ct.AsString() << Endm);
    et.SetFormat("h:m:s");
    LOG_POST("\texecution   - " << et.AsString());
}


/* Set routine to be called at the exit from application
 */
static bool s_SetExitHandler()
{
    // Set exit routine if it not set yet
    CFastMutexGuard LOCK(s_ExitHandler_Mutex);
    if ( !s_ExitHandlerIsSet ) {
        if (atexit(s_ExitHandler) != 0) {
            return false;
        }
        s_ExitHandlerIsSet = true;
        s_TimeSet.SetCurrent();
        // Reserve some memory (10Kb)
        s_ReserveMemory = new char[10000];
    }
    return true;
}
    
#endif /* NCBI_OS_UNIX */



/////////////////////////////////////////////////////////////////////////////
//
//  SetHeapLimit
//


#ifdef USE_SETHEAPLIMIT

/* Handler for operator new.
 */
static void s_NewHandler()
{
    s_ExitCode = eEC_Memory;
    exit(-1);
}


bool SetHeapLimit(size_t max_heap_size)
{
    if (s_HeapLimit == max_heap_size) 
        return true;
    
    if ( !s_SetExitHandler() )
        return false;

    // Set new heap limit
    CFastMutexGuard LOCK(s_ExitHandler_Mutex);
    rlimit rl;
    if ( max_heap_size ) {
        set_new_handler(s_NewHandler);
        rl.rlim_cur = rl.rlim_max = max_heap_size;
    }
    else {
        // Set off heap limit
        set_new_handler(0);
        rl.rlim_cur = rl.rlim_max = RLIM_INFINITY;
    }
    if (setrlimit(RLIMIT_DATA, &rl) != 0) 
        return false;

    s_HeapLimit = max_heap_size;

    return true;
}

#else

bool SetHeapLimit(size_t max_heap_size)
{
  return false;
}

#endif /* USE_SETHEAPLIMIT */



/////////////////////////////////////////////////////////////////////////////
//
//  SetCpuTimeLimit
//


#ifdef USE_SETCPULIMIT

static void s_SignalHandler(int sig)
{
    _ASSERT(sig == SIGXCPU);

    _VERIFY(signal(SIGXCPU, SIG_IGN) != SIG_ERR);

    s_ExitCode = eEC_Cpu;
    exit(-1);
}


bool SetCpuTimeLimit(size_t max_cpu_time)
{
    if (s_CpuTimeLimit == max_cpu_time) 
        return true;
    
    if ( !s_SetExitHandler() )
        return false;
    
    // Set new CPU time limit
    CFastMutexGuard LOCK(s_ExitHandler_Mutex);

    struct rlimit rl;
    if ( max_cpu_time ) {
        rl.rlim_cur = rl.rlim_max = max_cpu_time;
    }
    else {
        // Set off CPU time limit
        rl.rlim_cur = rl.rlim_max = RLIM_INFINITY;
    }

    if (setrlimit(RLIMIT_CPU, &rl) != 0) {
        return false;
    }
    s_CpuTimeLimit = max_cpu_time;

    // Set signal handler for SIGXCPU
    if (signal(SIGXCPU, s_SignalHandler) == SIG_ERR) {
        return false;
    }

    return true;
}

#else

bool SetCpuTimeLimit(size_t max_cpu_time)
{
    return false;
}

#endif /* USE_SETCPULIMIT */


END_NCBI_SCOPE
