#ifndef NCBITHR__HPP
#define NCBITHR__HPP

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
 * Author:  Denis Vakatov, Aleksey Grichenko
 *
 * File Description:
 *   Multi-threading -- classes, functions, and features.
 *
 *   TLS:
 *      CTlsBase         -- TLS implementation (base class for CTls<>)
 *      CTls<>           -- thread local storage template
 *
 *   THREAD:
 *      CThread          -- thread wrapper class
 *
 *   MUTEX:
 *      CMutex           -- mutex that allows nesting (with runtime checks)
 *      CAutoMutex       -- guarantee mutex release
 *      CMutexGuard      -- acquire mutex, then guarantee for its release
 *
 *   RW-LOCK:
 *      CInternalRWLock  -- platform-dependent RW-lock structure (fwd-decl)
 *      CRWLock          -- Read/Write lock related  data and methods
 *      CAutoRW          -- guarantee RW-lock release
 *      CReadLockGuard   -- acquire R-lock, then guarantee for its release
 *      CWriteLockGuard  -- acquire W-lock, then guarantee for its release
 *
 *   SEMAPHORE:
 *      CSemaphore       -- application-wide semaphore
 *
 *   MISC:
 *      SwapPointers     -- atomic pointer swap operation
 *
 */

#include <corelib/ncbiobj.hpp>
#include <corelib/ncbithr_conf.hpp>
#include <corelib/ncbimtx.hpp>
#include <memory>
#include <set>
#include <list>

#ifdef NCBI_OS_DARWIN
// Needed for SwapPointers, even if not for CAtomicCounter
#  include <CarbonCore/DriverSynchronization.h>
#endif

/** @addtogroup Threads
 *
 * @{
 */


BEGIN_NCBI_SCOPE

/////////////////////////////////////////////////////////////////////////////
//
//  CTlsBase::
//
//  CTls<>::
//
//    Thread local storage
//
//  Store thread-specific data.
//

// only to serve as a base class for CTls<>
class NCBI_XNCBI_EXPORT CTlsBase : public CObject
{
    friend class CRef<CTlsBase>;
    friend class CThread;

public:
    typedef void (*FCleanupBase)(void* value, void* cleanup_data);

protected:
    // All other methods are described just below, in CTls<> class declaration
    CTlsBase(void);

    // Cleanup data, delete TLS key
    ~CTlsBase(void);

    void* x_GetValue(void) const;
    void x_SetValue(void* value, FCleanupBase cleanup=0, void* cleanup_data=0);
    void x_Reset(void);
    void x_Discard(void);

private:
    TTlsKey m_Key;
    bool    m_Initialized;

    // Internal structure to store all three pointers in the same TLS
    struct STlsData {
        void*        m_Value;
        FCleanupBase m_CleanupFunc;
        void*        m_CleanupData;
    };
    STlsData* x_GetTlsData(void) const;
};


template <class TValue>
class CTls : public CTlsBase
{
public:
    // Get the pointer previously stored by SetValue().
    // Return 0 if no value has been stored, or if Reset() was last called.
    TValue* GetValue(void) const
    {
        return reinterpret_cast<TValue*> (x_GetValue());
    }

    // Cleanup previously stored value, store the new value.
    // The "cleanup" function and "cleanup_data" will be used to
    // destroy the new "value" in the next call to SetValue() or Reset().
    // Do not cleanup if the new value is equal to the old one.
    typedef void (*FCleanup)(TValue* value, void* cleanup_data);
    void SetValue(TValue* value, FCleanup cleanup = 0, void* cleanup_data = 0)
    {
        x_SetValue(value,
                   reinterpret_cast<FCleanupBase> (cleanup), cleanup_data);
    }

    // Reset TLS to its initial value (as it was before the first call
    // to SetValue()). Do cleanup if the cleanup function was specified
    // in the previous call to SetValue().
    // NOTE:  Reset() will always be called automatically on the thread
    //        termination, or when the TLS is destroyed.
    void Reset(void) { x_Reset(); }

    // Schedule the TLS to be destroyed
    // (as soon as there are no CRef to it left).
    void Discard(void) { x_Discard(); }
};



/////////////////////////////////////////////////////////////////////////////
//
//  CThread::
//
//    Thread wrapper class
//
//  Base class for user-defined threads. Creates the new thread, then
//  calls user-provided Main() function. The thread then can be detached
//  or joined. In any case, explicit destruction of the thread is prohibited.
//


class NCBI_XNCBI_EXPORT CThread : public CObject
{
    friend class CRef<CThread>;
    friend class CTlsBase;

public:
    // Must be allocated in the heap (only!).
    CThread(void);

    // bitwise OR'd flags for thread creation passed to Run()
    enum ERunMode {
        fRunDefault  = 0x00,    // default
        fRunDetached = 0x01,    // run the thread detached (non-joinable)
// these two may not be supported (and will be ignored) on some platforms
        fRunBound    = 0x10,    // run thread in a 1:1 thread:LPW mode
        fRunUnbound  = 0x20,    // run thread in a N:1 thread:LPW mode
        fRunAllowST  = 0x100    // allow threads to run in single thread builds
    };
    typedef int TRunMode;  // binary OR of "ERunMode"

    // Run the thread:
    // create new thread, initialize it, and call user-provided Main() method.
    bool Run(TRunMode flags = fRunDefault);

    // Inform the thread that user does not need to wait for its termination.
    // The thread object will be destroyed by Exit().
    // If the thread has already been terminated by Exit, Detach() will
    // also schedule the thread object for destruction.
    // NOTE:  it is no more safe to use this thread object after Detach(),
    //        unless there are still CRef<> based references to it!
    void Detach(void);

    // Wait for the thread termination.
    // The thread object will be scheduled for destruction right here,
    // inside Join(). Only one call to Join() is allowed.
    void Join(void** exit_data = 0);

    // Cancel current thread. If the thread is detached, then schedule
    // the thread object for destruction.
    // Cancellation is performed by throwing an exception of type
    // CExitThreadException to allow destruction of all objects in
    // thread's stack, so Exit() method shell not be called from any
    // destructor.
    static void Exit(void* exit_data);

    // If the thread has not been Run() yet, then schedule the thread object
    // for destruction, and return TRUE.
    // Otherwise, do nothing, and return FALSE.
    bool Discard(void);

    // Get ID of current thread (for main thread it is always zero).
    typedef unsigned int TID;
    static TID GetSelf(void);

    // Get system ID of the current thread - for internal use only.
    // The ID is unique only while the thread is running and may be
    // re-used by another thread later.
    static void GetSystemID(TThreadSystemID* id);

protected:
    // Derived (user-created) class must provide a real thread function.
    virtual void* Main(void) = 0;

    // Override this to execute finalization code.
    // Unlike destructor, this code will be executed before
    // thread termination and as a part of the thread.
    virtual void OnExit(void);

    // To be called only internally!
    // NOTE:  destructor of the derived (user-provided) class should be
    //        declared "protected", too!
    virtual ~CThread(void);

private:
    TID           m_ID;            // thread ID
    TThreadHandle m_Handle;        // platform-dependent thread handle
    bool          m_IsRun;         // if Run() was called for the thread
    bool          m_IsDetached;    // if the thread is detached
    bool          m_IsJoined;      // if Join() was called for the thread
    bool          m_IsTerminated;  // if Exit() was called for the thread
    CRef<CThread> m_SelfRef;       // "this" -- to avoid premature destruction
    void*         m_ExitData;      // as returned by Main() or passed to Exit()

    // Function to use (internally) as the thread's startup function
    static TWrapperRes Wrapper(TWrapperArg arg);

    // To store "CThread" object related to the current (running) thread
    static CTls<CThread>* sm_ThreadsTls;
    // Safe access to "sm_ThreadsTls"
    static CTls<CThread>& GetThreadsTls(void)
    {
        if ( !sm_ThreadsTls ) {
            CreateThreadsTls();
        }
        return *sm_ThreadsTls;
    }

    // sm_ThreadsTls initialization and cleanup functions
    static void CreateThreadsTls(void);
    friend void s_CleanupThreadsTls(void* /* ptr */);

    // Keep all TLS references to clean them up in Exit()
    typedef set< CRef<CTlsBase> > TTlsSet;
    TTlsSet m_UsedTls;
    static void AddUsedTls(CTlsBase* tls);
};


/// Set *location to new_value, and return its immediately previous contents.
void* SwapPointers(void * volatile * location, void* new_value);

/// Out-of-line implementation; defined and used ifdef NCBI_SLOW_ATOMIC_SWAP.
void* x_SwapPointers(void * volatile * location, void* new_value);


/* @} */


/////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////
//  IMPLEMENTATION of INLINE functions
/////////////////////////////////////////////////////////////////////////////



/////////////////////////////////////////////////////////////////////////////
//  CTlsBase::
//

inline
CTlsBase::STlsData* CTlsBase::x_GetTlsData(void)
const
{
    if ( !m_Initialized ) {
        return 0;
    }

    void* tls_data;

#if defined(NCBI_WIN32_THREADS)
    tls_data = TlsGetValue(m_Key);
#elif defined(NCBI_POSIX_THREADS)
    tls_data = pthread_getspecific(m_Key);
#else
    tls_data = m_Key;
#endif

    return static_cast<STlsData*> (tls_data);
}


inline
void* CTlsBase::x_GetValue(void)
const
{
    // Get TLS-stored structure
    STlsData* tls_data = x_GetTlsData();

    // If assigned, extract and return user data
    return tls_data ? tls_data->m_Value : 0;
}



/////////////////////////////////////////////////////////////////////////////
//  CThread::
//

inline
CThread::TID CThread::GetSelf(void)
{
    // Get pointer to the current thread object
    CThread* thread_ptr = GetThreadsTls().GetValue();

    // If zero, it is main thread which has no CThread object
    return thread_ptr ? thread_ptr->m_ID : 0/*main thread*/;
}


// Special value, stands for "no thread" thread ID
const CThread::TID kThreadID_None = 0xFFFFFFFF;


// Sigh... this is a big mess because WorkShop's handling of inline
// asm is awkward and a lot of platforms supply compare-and-swap but
// no suitable unconditional swap.  It also doesn't help that standard
// interfaces are typically for integral types.
#if !defined(NCBI_COMPILER_WORKSHOP)  ||  defined(NCBI_COUNTER_IMPLEMENTATION)
#  if defined(NCBI_COMPILER_WORKSHOP)  &&  !defined(NCBI_NO_THREADS)
#    ifdef __sparcv9
extern "C"
void* NCBICORE_asm_casx(void* new_value, void** location, void* old_value);
#    elif defined(__i386)
extern "C"
void* NCBICORE_asm_xchg(void* new_value, void** location);
#    endif
#  else
inline
#  endif
void* SwapPointers(void * volatile * location, void* new_value)
{
    void** nv_loc = const_cast<void**>(location);
#  ifdef NCBI_NO_THREADS
    void* old_value = *nv_loc;
    *nv_loc = new_value;
    return old_value;
#  elif defined(NCBI_COMPILER_COMPAQ)
    return reinterpret_cast<void*>
        (__ATOMIC_EXCH_QUAD(nv_loc, reinterpret_cast<long>(new_value)));
#  elif defined(NCBI_OS_IRIX)
    return reinterpret_cast<void*>
        (test_and_set(reinterpret_cast<unsigned long*>(nv_loc),
                      reinterpret_cast<unsigned long>(new_value)));
#  elif defined(NCBI_OS_AIX)
    boolean_t swapped   = FALSE;
    void*     old_value = *nv_loc;
    while (swapped == FALSE) {
        swapped = compare_and_swap(reinterpret_cast<atomic_p>(nv_loc),
                                   reinterpret_cast<int*>(&old_value),
                                   reinterpret_cast<int>(new_value));
    }
    return old_value;
#  elif defined(NCBI_OS_DARWIN)
    Boolean swapped = FALSE;
    void*   old_value;
    while (swapped == FALSE) {
        old_value = *nv_loc;
        swapped = CompareAndSwap(reinterpret_cast<UInt32>(old_value),
                                 reinterpret_cast<UInt32>(new_value),
                                 reinterpret_cast<UInt32*>(nv_loc));
    }
    return old_value;
#  elif defined(NCBI_OS_MAC)
    Boolean swapped = FALSE;
    void*   old_value;
    while (swapped == FALSE) {
        old_value = *nv_loc;
        swapped = OTCompareAndSwapPtr(*nv_loc, new_value, nv_loc);
    }
    return old_value;
#  elif defined(NCBI_OS_MSWIN)
    // InterlockedExchangePointer would be better, but older SDK versions
    // don't declare it. :-/
    return reinterpret_cast<void*>
        (InterlockedExchange(reinterpret_cast<LPLONG>(nv_loc),
                             reinterpret_cast<LONG>(new_value)));
#  elif defined(NCBI_COUNTER_ASM_OK)
#    ifdef __i386
    void* old_value;
#      ifdef NCBI_COMPILER_WORKSHOP
    old_value = NCBICORE_asm_xchg(new_value, nv_loc);
#      else
    asm volatile("xchg %0, %1" : "+m" (*nv_loc), "=r" (old_value)
        : "1" (new_value), "m" (*nv_loc));
#      endif
    return old_value;
#    elif defined(__sparcv9)
    void* old_value = *nv_loc;
    void* tmp       = new_value;
    while (tmp != old_value) {
#      ifdef NCBI_COMPILER_WORKSHOP
        tmp = NCBICORE_asm_casx(tmp, nv_loc, old_value);
#      else
        asm volatile("casx [%3], %2, %1" : "+m" (*nv_loc), "+r" (tmp)
                     : "r" (old_value), "r" (nv_loc));
#      endif
    }
    return old_value;
#    elif defined(__sparc)
    void* old_value;
#      ifdef NCBI_COMPILER_WORKSHOP
    old_value = reinterpret_cast<void*>
        (NCBICORE_asm_swap(reinterpret_cast<TNCBIAtomicValue>(new_value),
                           reinterpret_cast<TNCBIAtomicValue*>(nv_loc)));
#      else
    asm volatile("swap [%2], %1" : "+m" (*nv_loc), "=r" (old_value)
        : "r" (nv_loc), "1" (new_value));
#      endif
    return old_value;
#    else
#      define NCBI_SLOW_ATOMIC_SWAP
    return x_SwapPointers(location, new_value);
#    endif
#  else
#    define NCBI_SLOW_ATOMIC_SWAP
    return x_SwapPointers(location, new_value);
#  endif
}
#endif


END_NCBI_SCOPE



/*
 * ===========================================================================
 * $Log$
 * Revision 1.20  2003/07/31 19:29:03  ucko
 * SwapPointers: fix for Mac OS (classic and X) and AIX.
 *
 * Revision 1.19  2003/06/27 17:27:44  ucko
 * +SwapPointers
 *
 * Revision 1.18  2003/05/08 20:50:08  grichenk
 * Allow MT tests to run in ST mode using CThread::fRunAllowST flag.
 *
 * Revision 1.17  2003/03/31 13:30:13  siyan
 * Minor changes to doxygen support
 *
 * Revision 1.16  2003/03/31 13:02:47  siyan
 * Added doxygen support
 *
 * Revision 1.15  2002/12/18 22:53:21  dicuccio
 * Added export specifier for building DLLs in windows.  Added global list of
 * all such specifiers in mswin_exports.hpp, included through ncbistl.hpp
 *
 * Revision 1.14  2002/09/30 16:57:34  vasilche
 * Removed extra comma in enum.
 *
 * Revision 1.13  2002/09/30 16:32:28  vasilche
 * Fixed bug with self referenced CThread.
 * Added bound running flag to CThread.
 * Fixed concurrency level on POSIX threads.
 *
 * Revision 1.12  2002/09/19 20:05:41  vasilche
 * Safe initialization of static mutexes
 *
 * Revision 1.11  2002/09/13 15:14:43  ucko
 * Give CSemaphore::TryWait an optional timeout (defaulting to 0)
 *
 * Revision 1.10  2002/07/15 18:17:52  gouriano
 * renamed CNcbiException and its descendents
 *
 * Revision 1.9  2002/07/11 14:17:55  gouriano
 * exceptions replaced by CNcbiException-type ones
 *
 * Revision 1.8  2002/04/11 20:39:19  ivanov
 * CVS log moved to end of the file
 *
 * Revision 1.7  2001/12/10 18:07:53  vakatov
 * Added class "CSemaphore" -- application-wide semaphore
 *
 * Revision 1.6  2001/05/17 14:54:33  lavr
 * Typos corrected
 *
 * Revision 1.5  2001/03/30 22:57:32  grichenk
 * + CThread::GetSystemID()
 *
 * Revision 1.4  2001/03/26 21:45:28  vakatov
 * Workaround static initialization/destruction traps:  provide better
 * timing control, and allow safe use of the objects which are
 * either not yet initialized or already destructed. (with A.Grichenko)
 *
 * Revision 1.3  2001/03/13 22:43:19  vakatov
 * Full redesign.
 * Implemented all core functionality.
 * Thoroughly tested on different platforms.
 *
 * Revision 1.2  2000/12/11 06:48:51  vakatov
 * Revamped Mutex and RW-lock APIs
 *
 * Revision 1.1  2000/12/09 00:03:26  vakatov
 * First draft:  Mutex and RW-lock API
 *
 * ===========================================================================
 */

#endif  /* NCBITHR__HPP */
