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
 *   Eugene Vasilchenko
 *
 * File Description:
 *   Standard CObject and CRef classes for reference counter based GC
 *
 */

#include <corelib/ncbiobj.hpp>


// There was a long and bootless discussion:
// is it possible to determine whether the object has been created
// on the stack or on the heap.
// Correct answer is "it is impossible"
// Still, we try to... (we know it is not 100% bulletproof)
//
// Attempts include:
//
// 1. operator new puts a pointer to newly allocated memory in the list.
//    Object constructor scans the list, if it finds itself there -
//    yes, it has been created on the heap. (If it does not find itself
//    there, it still may have been created on the heap).
//    This method requires thread synchronization.
//
// 2. operator new puts a special mask (eCounterNew two times) in the
//    newly allocated memory. Object constructor looks for this mask,
//    if it finds it there - yes, it has been created on the heap.
//
// 3. operator new puts a special mask (single eCounterNew) in the
//    newly allocated memory. Object constructor looks for this mask,
//    if it finds it there, it also compares addresses of a variable
//    on the stack and itself (also using STACK_THRESHOLD). If these two
//    are "far enough from each other" - yes, the object is on the heap.
//
// From these three methods, the first one seems to be most reliable,
// but also most slow.
// Method #2 is hopefully reliable enough
// Method #3 is unreliable at all (we saw this)
//


#define USE_HEAPOBJ_LIST  0
#if USE_HEAPOBJ_LIST
#  include <corelib/ncbi_safe_static.hpp>
#  include <list>
#  include <algorithm>
#else
#  define USE_COMPLEX_MASK  1
#  if USE_COMPLEX_MASK
#    define STACK_THRESHOLD (64)
#  else
#    define STACK_THRESHOLD (16*1024)
#  endif
#endif


BEGIN_NCBI_SCOPE


/////////////////////////////////////////////////////////////////////////////
//  CNullPointerError::
//


CNullPointerError::CNullPointerError(void)
    THROWS_NONE
{
}

CNullPointerError::~CNullPointerError(void)
    THROWS_NONE
{
}

const char* CNullPointerError::what() const
    THROWS_NONE
{
    return "null pointer error";
}



/////////////////////////////////////////////////////////////////////////////
//  CObject::
//


CFastMutex CObject::sm_ObjectMutex;

#ifdef NCBI_COUNTER_NEED_MUTEX
// CAtomicCounter doesn't normally have a .cpp file of its own, so this
// goes here instead.
CFastMutex CAtomicCounter::sm_Mutex;
#endif

#if USE_HEAPOBJ_LIST
static CSafeStaticPtr< list<const void*> > s_heap_obj;
#endif

// CObject local new operator to mark allocation in heap
void* CObject::operator new(size_t size)
{
    _ASSERT(size >= sizeof(CObject));
    size = max(size, 2*sizeof(TCounter));
    void* ptr = ::operator new(size);

#if USE_HEAPOBJ_LIST
    {{
        CFastMutexGuard LOCK(sm_ObjectMutex);
        s_heap_obj->push_front(ptr);
    }}
#else// USE_HEAPOBJ_LIST
    memset(ptr, 0, size);
#  if USE_COMPLEX_MASK
    TCounter* ttt = &(static_cast<CObject*>(ptr)->m_Counter);
    if (size >= 2*sizeof(TCounter)) {
        (++ttt)->Set(eCounterNew);
    }
#  endif// USE_COMPLEX_MASK
#endif// USE_HEAPOBJ_LIST
    static_cast<CObject*>(ptr)->m_Counter.Set(eCounterNew);
    return ptr;
}


void* CObject::operator new[](size_t size)
{
#ifdef NCBI_OS_MSWIN
    void* ptr = ::operator new(size);
#else
    void* ptr = ::operator new[](size);
#endif
    memset(ptr, 0, size);
    return ptr;
}


#ifdef _DEBUG
void CObject::operator delete(void* ptr)
{
    CObject* objectPtr = static_cast<CObject*>(ptr);
    _ASSERT(objectPtr->m_Counter.Get() == eCounterDeleted  ||
            objectPtr->m_Counter.Get() == eCounterNew);
    ::operator delete(ptr);
}

void CObject::operator delete[](void* ptr)
{
#  ifdef NCBI_OS_MSWIN
    ::operator delete(ptr);
#  else
    ::operator delete[](ptr);
#  endif
}
#endif  /* _DEBUG */


// initialization in debug mode
void CObject::InitCounter(void)
{
    if ( m_Counter.Get() != eCounterNew ) {
        // takes care of statically allocated case
        m_Counter.Set(eCounterNotInHeap);
    }
    else {
        bool inStack = false;
#if USE_HEAPOBJ_LIST
        const void* ptr = dynamic_cast<const void*>(this);
        {{
            CFastMutexGuard LOCK(sm_ObjectMutex);
            list<const void*>::iterator i =
                find( s_heap_obj->begin(), s_heap_obj->end(), ptr);
            inStack = (i == s_heap_obj->end());
            if (!inStack) {
                s_heap_obj->erase(i);
            }
        }}
#else // USE_HEAPOBJ_LIST
#  if USE_COMPLEX_MASK
        TCounter* ttt = &m_Counter;
        ++ttt;
        inStack = (ttt->Get() != eCounterNew);
#  endif // USE_COMPLEX_MASK
        // m_Counter == eCounterNew -> possibly in heap
        if (!inStack) {
        char stackObject;
        const char* stackObjectPtr = &stackObject;
        const char* objectPtr = reinterpret_cast<const char*>(this);
        inStack =
#  ifdef STACK_GROWS_UP
            (objectPtr < stackObjectPtr) &&
#  else
            (objectPtr < stackObjectPtr + STACK_THRESHOLD) &&
#  endif
#  ifdef STACK_GROWS_DOWN
            (objectPtr > stackObjectPtr);
#  else
            (objectPtr > stackObjectPtr - STACK_THRESHOLD);
#  endif
        }
#endif // USE_HEAPOBJ_LIST

        // surely not in heap
        if ( inStack )
            m_Counter.Set(eCounterNotInHeap);
        else
            m_Counter.Set(eCounterInHeap);
    }
}


CObject::CObject(void)
{
    InitCounter();
}


CObject::CObject(const CObject& /*src*/)
{
    InitCounter();
}


CObject::~CObject(void)
{
    TCount count = m_Counter.Get();
    if ( count == TCount(eCounterInHeap)
        ||  count == TCount(eCounterNotInHeap) ) {
        // reference counter is zero -> ok
    }
    else if ( ObjectStateValid(count) ) {
        _ASSERT(ObjectStateReferenced(count));
        // referenced object
        THROW1_TRACE(runtime_error,
                     "deletion of referenced CObject");
    }
    else if ( count == eCounterDeleted ) {
        // deleted object
        THROW1_TRACE(runtime_error,
                     "double deletion of CObject");
    }
    else {
        // bad object
        THROW1_TRACE(runtime_error,
                     "deletion of corrupted CObject");
    }
    // mark object as deleted
    m_Counter.Set(eCounterDeleted);
}


void CObject::AddReferenceOverflow(TCount count) const
{
    if ( ObjectStateValid(count) ) {
        // counter overflow
        THROW1_TRACE(runtime_error,
                     "AddReference: CObject reference counter overflow");
    }
    else {
        // bad object
        THROW1_TRACE(runtime_error,
                     "AddReference of invalid CObject");
    }
}


void CObject::RemoveLastReference(void) const
{
    TCount count = m_Counter.Get();
    if ( count == TCount(eCounterInHeap) ) {
        // last reference to heap object -> delete
        delete this;
    }
    else if ( count == TCount(eCounterNotInHeap) ) {
        // last reference to non heap object -> do nothing
    }
    else {
        _ASSERT(!ObjectStateValid(count + eCounterStep));
        // bad object
        THROW1_TRACE(runtime_error,
                     "RemoveReference of unreferenced CObject");
    }
}


void CObject::ReleaseReference(void) const
{
    TCount count = m_Counter.Add(-eCounterStep) + eCounterStep;
    if ( ObjectStateReferenced(count) ) {
        return;
    }
    m_Counter.Add(eCounterStep); // undo

    // error
    if ( !ObjectStateValid(count) ) {
        THROW1_TRACE(runtime_error,
                     "ReleaseReference of corrupted CObject");
    } else {
        THROW1_TRACE(runtime_error,
                     "ReleaseReference of unreferenced CObject");
    }
}


void CObject::DoNotDeleteThisObject(void)
{
    bool is_valid;
    {{
        CFastMutexGuard LOCK(sm_ObjectMutex);
        TCount count = m_Counter.Get();
        is_valid = ObjectStateValid(count);
        if (is_valid  &&  !ObjectStateReferenced(count)) {
            m_Counter.Set(eCounterNotInHeap);
            return;
        }
    }}
    

    if ( is_valid ) {
        THROW1_TRACE(runtime_error,
                     "DoNotDeleteThisObject of referenced CObject");
    } else {
        THROW1_TRACE(runtime_error,
                     "DoNotDeleteThisObject of corrupted CObject");
    }
}


void CObject::DoDeleteThisObject(void)
{
    {{
        CFastMutexGuard LOCK(sm_ObjectMutex);
        TCount count = m_Counter.Get();
        if ( ObjectStateValid(count) ) {
            if ( !(count & eStateBitsInHeap) ) {
                m_Counter.Add(eStateBitsInHeap);
            }
            return;
        }
    }}

    THROW1_TRACE(runtime_error,
                 "DoDeleteThisObject of corrupted CObject");
}
                        
void CObject::DebugDump(CDebugDumpContext ddc, unsigned int /*depth*/) const
{
    ddc.SetFrame("CObject");
    ddc.Log("this", dynamic_cast<const void*>(this));
    ddc.Log("onHeap", CanBeDeleted());
}


END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.26  2002/05/24 14:12:10  ucko
 * Provide CAtomicCounter::sm_Mutex if necessary.
 *
 * Revision 1.25  2002/05/23 22:24:23  ucko
 * Use low-level atomic operations for reference counts
 *
 * Revision 1.24  2002/05/17 14:27:12  gouriano
 * added DebugDump base class and function to CObject
 *
 * Revision 1.23  2002/05/14 21:12:11  gouriano
 * DebugDump() moved into a separate class
 *
 * Revision 1.22  2002/05/14 14:44:24  gouriano
 * added DebugDump function to CObject
 *
 * Revision 1.21  2002/05/10 20:53:12  gouriano
 * more stack/heap tests
 *
 * Revision 1.20  2002/05/10 19:42:31  gouriano
 * object on stack vs on heap - do it more accurately
 *
 * Revision 1.19  2002/04/11 21:08:03  ivanov
 * CVS log moved to end of the file
 *
 * Revision 1.18  2001/08/20 18:35:10  ucko
 * Clarified InitCounter's treatment of statically allocated objects.
 *
 * Revision 1.17  2001/08/20 15:59:43  ucko
 * Test more accurately whether CObjects were created on the stack.
 *
 * Revision 1.16  2001/05/17 15:04:59  lavr
 * Typos corrected
 *
 * Revision 1.15  2001/04/03 18:08:54  grichenk
 * Converted eCounterNotInHeap to TCounter(eCounterNotInHeap)
 *
 * Revision 1.14  2001/03/26 21:22:52  vakatov
 * Minor cosmetics
 *
 * Revision 1.13  2001/03/13 22:43:50  vakatov
 * Made "CObject" MT-safe
 * + CObject::DoDeleteThisObject()
 *
 * Revision 1.12  2000/12/26 22:00:19  vasilche
 * Fixed error check for case CObject constructor never called.
 *
 * Revision 1.11  2000/11/01 21:20:55  vasilche
 * Fixed missing new[] and delete[] on MSVC.
 *
 * Revision 1.10  2000/11/01 20:37:16  vasilche
 * Fixed detection of heap objects.
 * Removed ECanDelete enum and related constructors.
 * Disabled sync_with_stdio ad the beginning of AppMain.
 *
 * Revision 1.9  2000/10/17 17:59:08  vasilche
 * Detected misuse of CObject constructors will be reported via ERR_POST
 * and will not throw exception.
 *
 * Revision 1.8  2000/10/13 16:26:30  vasilche
 * Added heuristic for detection of CObject allocation in heap.
 *
 * Revision 1.7  2000/08/15 19:41:41  vasilche
 * Changed reference counter to allow detection of more errors.
 *
 * Revision 1.6  2000/06/16 16:29:51  vasilche
 * Added SetCanDelete() method to allow to change CObject 'in heap' status
 * immediately after creation.
 *
 * Revision 1.5  2000/06/07 19:44:22  vasilche
 * Removed unneeded THROWS declaration - they lead to encreased code size.
 *
 * Revision 1.4  2000/03/29 15:50:41  vasilche
 * Added const version of CRef - CConstRef.
 * CRef and CConstRef now accept classes inherited from CObject.
 *
 * Revision 1.3  2000/03/08 17:47:30  vasilche
 * Simplified error check.
 *
 * Revision 1.2  2000/03/08 14:18:22  vasilche
 * Fixed throws instructions.
 *
 * Revision 1.1  2000/03/07 14:03:15  vasilche
 * Added CObject class as base for reference counted objects.
 * Added CRef templace for reference to CObject descendant.
 *
 * ===========================================================================
 */
