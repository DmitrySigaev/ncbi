#if defined(NCBIMTX__HPP)  &&  !defined(NCBIMTX__INL)
#define NCBIMTX__INL

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
 * Author: Eugene Vasilchenko
 *
 * File Description:
 *   Mutex classes' inline functions
 *
 */

/////////////////////////////////////////////////////////////////////////////
//  SSystemFastMutexStruct
//

inline
bool SSystemFastMutex::IsInitialized(void) const
{
    return m_Magic == eMutexInitialized;
}

inline
bool SSystemFastMutex::IsUninitialized(void) const
{
    return m_Magic == eMutexUninitialized;
}

inline
void SSystemFastMutex::CheckInitialized(void) const
{
#if defined(INTERNAL_MUTEX_DEBUG)
    if ( !IsInitialized() ) {
        ThrowUninitialized();
    }
#endif
}

inline
void SSystemFastMutex::Lock(void)
{
#if defined(NCBI_NO_THREADS)
    return;
#else
    // check
    CheckInitialized();

    // Acquire system mutex
#  if defined(NCBI_WIN32_THREADS)
    if (WaitForSingleObject(m_Handle, INFINITE) != WAIT_OBJECT_0) {
        ThrowLockFailed();
    }
#  elif defined(NCBI_POSIX_THREADS)
    if ( pthread_mutex_lock(&m_Handle) != 0 ) { // error
        ThrowLockFailed();
    }
#  endif
#endif
}

inline
bool SSystemFastMutex::TryLock(void)
{
#if defined(NCBI_NO_THREADS)
    return true;
#else
    // check
    CheckInitialized();

    // Check if the system mutex is acquired.
    // If not, acquire for the current thread.
#  if defined(NCBI_WIN32_THREADS)
    DWORD status = WaitForSingleObject(m_Handle, 0);
    if (status == WAIT_OBJECT_0) { // ok
        return true;
    }
    else {
        if (status != WAIT_TIMEOUT) { // error
            ThrowTryLockFailed();
        }
        return false;
    }
#  elif defined(NCBI_POSIX_THREADS)
    int status = pthread_mutex_trylock(&m_Handle);
    if (status == 0) { // ok
        return true;
    }
    else {
        if (status != EBUSY) { // error
            ThrowTryLockFailed();
        }
        return false;
    }
#  endif
#endif
}

inline
void SSystemFastMutex::Unlock(void)
{
#if defined(NCBI_NO_THREADS)
    return;
#else
    // check
    CheckInitialized();
        
    // Release system mutex
# if defined(NCBI_WIN32_THREADS)
    if ( !ReleaseMutex(m_Handle) ) { // error
        ThrowUnlockFailed();
    }
# elif defined(NCBI_POSIX_THREADS)
    if ( pthread_mutex_unlock(&m_Handle) != 0 ) { // error
        ThrowUnlockFailed();
    }
# endif
#endif
}

/////////////////////////////////////////////////////////////////////////////
//  SSystemMutex
//

inline
bool SSystemMutex::IsInitialized(void) const
{
    return m_Mutex.IsInitialized();
}

inline
bool SSystemMutex::IsUninitialized(void) const
{
    return m_Mutex.IsUninitialized();
}

inline
void SSystemMutex::InitializeStatic(void)
{
    m_Mutex.InitializeStatic();
}

inline
void SSystemMutex::InitializeDynamic(void)
{
    m_Mutex.InitializeDynamic();
    m_Count = 0;
}

inline
void SSystemMutex::Destroy(void)
{
    m_Mutex.Destroy();
}

inline
void SSystemMutex::Lock(void)
{
    m_Mutex.CheckInitialized();

    CThreadSystemID owner = CThreadSystemID::GetCurrent();
    if ( m_Count > 0 && owner == m_Owner ) {
        // Don't lock twice, just increase the counter
        m_Count++;
        return;
    }

    // Lock the mutex and remember the owner
    m_Mutex.Lock();
    _ASSERT(m_Count == 0);
    m_Owner = owner;
    m_Count = 1;
}


inline
bool SSystemMutex::TryLock(void)
{
    m_Mutex.CheckInitialized();

    CThreadSystemID owner = CThreadSystemID::GetCurrent();
    if ( m_Count > 0 && owner == m_Owner ) {
        // Don't lock twice, just increase the counter
        m_Count++;
        return true;
    }

    // If TryLock is successful, remember the owner
    if ( m_Mutex.TryLock() ) {
        _ASSERT(m_Count == 0);
        m_Owner = owner;
        m_Count = 1;
        return true;
    }

    // Cannot lock right now
    return false;
}


inline
void SSystemMutex::Unlock(void)
{
    m_Mutex.CheckInitialized();

    // No unlocks by threads other than owner.
    // This includes no unlocks of unlocked mutex.
    CThreadSystemID owner = CThreadSystemID::GetCurrent();
    if ( m_Count == 0 || owner != m_Owner ) {
        ThrowNotOwned();
    }

    // No real unlocks if counter > 1, just decrease it
    if (--m_Count > 0) {
        return;
    }

    _ASSERT(m_Count == 0);
    // This was the last lock - clear the owner and unlock the mutex
    m_Mutex.Unlock();
}

#if defined(NEED_AUTO_INITIALIZE_MUTEX)

inline
CAutoInitializeStaticFastMutex::TObject&
CAutoInitializeStaticFastMutex::Get(void)
{
    if ( !m_Mutex.IsInitialized() ) {
        Initialize();
    }
    return m_Mutex;
}

inline
CAutoInitializeStaticFastMutex::
operator CAutoInitializeStaticFastMutex::TObject&(void)
{
    return Get();
}

inline
void CAutoInitializeStaticFastMutex::Lock(void)
{
    Get().Lock();
}

inline
void CAutoInitializeStaticFastMutex::Unlock(void)
{
    Get().Unlock();
}

inline
bool CAutoInitializeStaticFastMutex::TryLock(void)
{
    return Get().TryLock();
}

inline
CAutoInitializeStaticMutex::TObject&
CAutoInitializeStaticMutex::Get(void)
{
    if ( !m_Mutex.IsInitialized() ) {
        Initialize();
    }
    return m_Mutex;
}

inline
CAutoInitializeStaticMutex::
operator CAutoInitializeStaticMutex::TObject&(void)
{
    return Get();
}

inline
void CAutoInitializeStaticMutex::Lock(void)
{
    Get().Lock();
}

inline
void CAutoInitializeStaticMutex::Unlock(void)
{
    Get().Unlock();
}

inline
bool CAutoInitializeStaticMutex::TryLock(void)
{
    return Get().TryLock();
}

#endif

/////////////////////////////////////////////////////////////////////////////
//  CFastMutex::
//

inline
CFastMutex::CFastMutex(void)
{
    m_Mutex.InitializeDynamic();
}

inline
CFastMutex::~CFastMutex(void)
{
    m_Mutex.Destroy();
}

inline
CFastMutex::operator SSystemFastMutex&(void)
{
    return m_Mutex;
}

inline
void CFastMutex::Lock(void)
{
    m_Mutex.Lock();
}

inline
void CFastMutex::Unlock(void)
{
    m_Mutex.Unlock();
}

inline
bool CFastMutex::TryLock(void)
{
    return m_Mutex.TryLock();
}

// CFastMutexGuard

inline
CFastMutexGuard::CFastMutexGuard(void)
    : m_Mutex(0)
{
}

inline
CFastMutexGuard::CFastMutexGuard(SSystemFastMutex& mtx)
    : m_Mutex(&mtx)
{
    mtx.Lock();
}

inline
CFastMutexGuard::~CFastMutexGuard(void)
{
    if (m_Mutex)
        m_Mutex->Unlock();
}

inline
void CFastMutexGuard::Release(void)
{
    if ( m_Mutex ) {
        m_Mutex->Unlock();
        m_Mutex = 0;
    }
}

inline
void CFastMutexGuard::Guard(SSystemFastMutex& mtx)
{
    if ( &mtx != m_Mutex ) {
        Release();
        mtx.Lock();
        m_Mutex = &mtx;
    }
}

inline
CMutex::CMutex(void)
{
    m_Mutex.InitializeDynamic();
}

inline
CMutex::~CMutex(void)
{
    m_Mutex.Destroy();
}

inline
CMutex::operator SSystemMutex&(void)
{
    return m_Mutex;
}

inline
void CMutex::Lock(void)
{
    m_Mutex.Lock();
}

inline
void CMutex::Unlock(void)
{
    m_Mutex.Unlock();
}

inline
bool CMutex::TryLock(void)
{
    return m_Mutex.TryLock();
}

inline
CMutexGuard::CMutexGuard(void)
    : m_Mutex(0)
{
}

inline
CMutexGuard::CMutexGuard(SSystemMutex& mtx)
    : m_Mutex(&mtx)
{
    mtx.Lock();
}

inline
CMutexGuard::~CMutexGuard(void)
{
    if ( m_Mutex ) {
        m_Mutex->Unlock();
    }
}

inline
void CMutexGuard::Release(void)
{
    if ( m_Mutex ) {
        m_Mutex->Unlock();
        m_Mutex = 0;
    }
}

inline
void CMutexGuard::Guard(SSystemMutex& mtx)
{
    if ( &mtx != m_Mutex ) {
        Release();
        mtx.Lock();
        m_Mutex = &mtx;
    }
}

/*
 * ===========================================================================
 * $Log$
 * Revision 1.3  2003/06/19 19:16:37  vasilche
 * Added CMutexGuard and CFastMutexGuard constructors.
 *
 * Revision 1.2  2002/09/20 20:02:07  vasilche
 * Added public Lock/Unlock/TryLock
 *
 * Revision 1.1  2002/09/19 20:05:41  vasilche
 * Safe initialization of static mutexes
 *
 * ===========================================================================
 */

#endif
