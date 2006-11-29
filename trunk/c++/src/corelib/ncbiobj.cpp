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

#include <ncbi_pch.hpp>
#include <corelib/ncbiobj.hpp>
#include <corelib/ncbimtx.hpp>
#include <corelib/ncbi_param.hpp>
#include <corelib/ncbimempool.hpp>

//#define USE_SINGLE_ALLOC
//#define USE_DEBUG_NEW

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
// 2. operator new puts a special mask (eMagicCounterNew two times) in the
//    newly allocated memory. Object constructor looks for this mask,
//    if it finds it there - yes, it has been created on the heap.
//
// 3. operator new puts a special mask (single eMagicCounterNew) in the
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
//  CObject::
//


DEFINE_STATIC_FAST_MUTEX(sm_ObjectMutex);

#if defined(NCBI_COUNTER_NEED_MUTEX)
// CAtomicCounter doesn't normally have a .cpp file of its own, so this
// goes here instead.
DEFINE_STATIC_FAST_MUTEX(sm_AtomicCounterMutex);

CAtomicCounter::TValue CAtomicCounter::Add(int delta) THROWS_NONE
{
    CFastMutexGuard LOCK(sm_AtomicCounterMutex);
    return m_Value += delta;
}

#endif

#if USE_HEAPOBJ_LIST
static CSafeStaticPtr< list<const void*> > s_heap_obj;
#endif

#if USE_COMPLEX_MASK
static inline CAtomicCounter* GetSecondCounter(CObject* ptr)
{
  return reinterpret_cast<CAtomicCounter*> (ptr + 1);
}
#endif


#ifdef USE_SINGLE_ALLOC
#define SINGLE_ALLOC_THRESHOLD (   2*1024)
#define SINGLE_ALLOC_POOL_SIZE (1024*1024)

DEFINE_STATIC_FAST_MUTEX(sx_SingleAllocMutex);

static char*  single_alloc_pool = 0;
static size_t single_alloc_pool_size = 0;

void* single_alloc(size_t size)
{
    if ( size > SINGLE_ALLOC_THRESHOLD ) {
        return ::operator new(size);
    }
    sx_SingleAllocMutex.Lock();
    size_t pool_size = single_alloc_pool_size;
    char* pool;
    if ( size > pool_size ) {
        pool_size = SINGLE_ALLOC_POOL_SIZE;
        pool = (char*) malloc(pool_size);
        if ( !pool ) {
            sx_SingleAllocMutex.Unlock();
            throw bad_alloc();
        }
        single_alloc_pool = pool;
        single_alloc_pool_size = pool_size;
    }
    else {
        pool = single_alloc_pool;
    }
    single_alloc_pool      = pool      + size;
    single_alloc_pool_size = pool_size - size;
    sx_SingleAllocMutex.Unlock();
    return pool;
}
#endif

// CObject local new operator to mark allocation in heap
void* CObject::operator new(size_t size)
{
    _ASSERT(size >= sizeof(CObject));
    size = max(size, sizeof(CObject) + sizeof(TCounter));

#ifdef USE_SINGLE_ALLOC
    void* ptr = single_alloc(size);
    memset(ptr, 0, size);
    //static_cast<CObject*>(ptr)->m_Counter.Set(0);
    return ptr;
#else
    void* ptr = ::operator new(size);

#if USE_HEAPOBJ_LIST
    {{
        CFastMutexGuard LOCK(sm_ObjectMutex);
        s_heap_obj->push_front(ptr);
    }}
#else// USE_HEAPOBJ_LIST
    memset(ptr, 0, size);
#  if USE_COMPLEX_MASK
    GetSecondCounter(static_cast<CObject*>(ptr))->Set(eMagicCounterNew);
#  endif// USE_COMPLEX_MASK
#endif// USE_HEAPOBJ_LIST
    static_cast<CObject*>(ptr)->m_Counter.Set(eMagicCounterNew);
    return ptr;
#endif
}


void CObject::operator delete(void* ptr)
{
#ifdef _DEBUG
    CObject* objectPtr = static_cast<CObject*>(ptr);
    TCount magic = objectPtr->m_Counter.Get();
    // magic can be equal to:
    // 1. eMagicCounterDeleted when memory is freed after CObject destructor.
    // 2. eMagicCounterNew when memory is freed before CObject constructor.
    _ASSERT(magic == eMagicCounterDeleted  || magic == eMagicCounterNew);
#endif
    ::operator delete(ptr);
}


// CObject local new operator to mark allocation in other memory chunk
void* CObject::operator new(size_t size, void* place)
{
    _ASSERT(size >= sizeof(CObject));
    memset(place, 0, size);
    //static_cast<CObject*>(ptr)->m_Counter.Set(0);
    return place;
}

// complement placement delete operator -> do nothing
void CObject::operator delete(void* _DEBUG_ARG(ptr), void* /*place*/)
{
#ifdef _DEBUG
    CObject* objectPtr = static_cast<CObject*>(ptr);
    TCount magic = objectPtr->m_Counter.Get();
    // magic can be equal to:
    // 1. eMagicCounterDeleted when memory is freed after CObject destructor.
    // 2. 0 when memory is freed before CObject constructor.
    _ASSERT(magic == eMagicCounterDeleted  || magic == 0);
#endif
}


// CObject new operator from memory pool
void* CObject::operator new(size_t size, CObjectMemoryPool* memory_pool)
{
    _ASSERT(size >= sizeof(CObject));
    if ( !memory_pool ) {
        return operator new(size);
    }
    void* ptr = memory_pool->Allocate(size);
    if ( !ptr ) {
        return operator new(size);
    }
#  if USE_COMPLEX_MASK
    GetSecondCounter(static_cast<CObject*>(ptr))->Set(eMagicCounterPoolNew);
#  endif// USE_COMPLEX_MASK
    static_cast<CObject*>(ptr)->m_Counter.Set(eMagicCounterPoolNew);
    return ptr;
}

// complement pool delete operator
void CObject::operator delete(void* ptr, CObjectMemoryPool* memory_pool)
{
#ifdef _DEBUG
    CObject* objectPtr = static_cast<CObject*>(ptr);
    TCount magic = objectPtr->m_Counter.Get();
    // magic can be equal to:
    // 1. eMagicCounterPoolDeleted when freed after CObject destructor.
    // 2. eMagicCounterPoolNew when freed before CObject constructor.
    _ASSERT(magic == eMagicCounterPoolDeleted ||
            magic == eMagicCounterPoolNew);
#endif
    memory_pool->Deallocate(ptr);
}


// CObject local new operator to mark allocation in heap
void* CObject::operator new[](size_t size)
{
# ifdef NCBI_OS_MSWIN
    void* ptr = ::operator new(size);
# else
    void* ptr = ::operator new[](size);
# endif
    memset(ptr, 0, size);
    return ptr;
}


void CObject::operator delete[](void* ptr)
{
#ifdef NCBI_OS_MSWIN
    ::operator delete(ptr);
#else
    ::operator delete[](ptr);
#endif
}


// initialization in debug mode
void CObject::InitCounter(void)
{
    // This code can't use Get(), which may block waiting for an
    // update that will never happen.
    // ATTENTION:  this code can cause UMR (Uninit Mem Read) -- it's okay here!
    TCount main_counter = m_Counter.m_Value;
    if ( main_counter != eMagicCounterNew &&
         main_counter != eMagicCounterPoolNew ) {
        // takes care of statically allocated case
        m_Counter.Set(eInitCounterNotInHeap);
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
        inStack = GetSecondCounter(this)->m_Value != main_counter;
#  endif // USE_COMPLEX_MASK
        // m_Counter == main_counter -> possibly in heap
        if ( !inStack ) {
            char stackObject;
            const char* stackObjectPtr = &stackObject;
            const char* objectPtr = reinterpret_cast<const char*>(this);
#  if defined STACK_GROWS_UP
            inStack =
                (objectPtr < stackObjectPtr) && 
                (objectPtr > stackObjectPtr - STACK_THRESHOLD);
#  elif defined STACK_GROWS_DOWN
            inStack =
                (objectPtr > stackObjectPtr) &&
                (objectPtr < stackObjectPtr + STACK_THRESHOLD);
#  else
            inStack =
                (objectPtr < stackObjectPtr + STACK_THRESHOLD) && 
                (objectPtr > stackObjectPtr - STACK_THRESHOLD);
#  endif
        }
#endif // USE_HEAPOBJ_LIST
        
        if ( inStack ) {
            // surely not in heap
            m_Counter.Set(eInitCounterInStack);
        }
        else if ( main_counter == eMagicCounterNew ) {
            // allocated in heap
            m_Counter.Set(eInitCounterInHeap);
        }
        else {
            // allocated in memory pool
            m_Counter.Set(eInitCounterInPool);
        }
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


#ifdef _DEBUG
# define ObjFatal Fatal
#else
# define ObjFatal Critical
#endif

CObject::~CObject(void)
{
    TCount count = m_Counter.Get();
    if ( ObjectStateUnreferenced(count) ) {
        // reference counter is zero -> ok
    }
    else if ( ObjectStateValid(count) ) {
        _ASSERT(ObjectStateReferenced(count));
        // referenced object
        ERR_POST(ObjFatal << "CObject::~CObject: "
                 "Referenced CObject may not be deleted");
    }
    else if ( count == eMagicCounterDeleted ||
              count == eMagicCounterPoolDeleted ) {
        // deleted object
        ERR_POST(ObjFatal << "CObject::~CObject: "
                 "CObject is already deleted");
    }
    else {
        // bad object
        ERR_POST(ObjFatal << "CObject::~CObject: "
                 "CObject is corrupted");
    }
    // mark object as deleted
    TCount final_magic;
    if ( ObjectStateIsAllocatedInPool(count) ) {
        final_magic = eMagicCounterPoolDeleted;
    }
    else {
        final_magic = eMagicCounterDeleted;
    }
    m_Counter.Set(final_magic);
}


void CObject::CheckReferenceOverflow(TCount count) const
{
    if ( ObjectStateValid(count) ) {
        // counter overflow
        NCBI_THROW(CObjectException, eRefOverflow,
                   "CObject::CheckReferenceOverflow: "
                   "CObject's reference counter overflow");
    }
    else if ( count == eMagicCounterDeleted ||
              count == eMagicCounterPoolDeleted ) {
        // deleted object
        NCBI_THROW(CObjectException, eDeleted,
                   "CObject::CheckReferenceOverflow: "
                   "CObject is already deleted");
    }
    else {
        // bad object
        NCBI_THROW(CObjectException, eCorrupted,
                   "CObject::CheckReferenceOverflow: "
                   "CObject is corrupted");
    }
}


void CObject::RemoveLastReference(void) const
{
    TCount count = m_Counter.Get();
    if ( ObjectStateCanBeDeleted(count) ) {
        // last reference to heap object -> delete it
        if ( ObjectStateUnreferenced(count) ) {
            if ( count == TCount(eInitCounterInHeap) ) {
                delete this;
            }
            else {
                _ASSERT(count == TCount(eInitCounterInPool));
                CObjectMemoryPool::Delete(this);
            }
            return;
        }
    }
    else {
        if ( ObjectStateValid(count) ) {
            // last reference to non heap object -> do nothing
            return;
        }
    }

    // Error here
    // restore original value
    count = m_Counter.Add(eCounterStep);
    // bad object
    if ( ObjectStateValid(count) ) {
        ERR_POST(ObjFatal << "CObject::RemoveLastReference: "
                 "CObject was referenced again");
    }
    else if ( count == eMagicCounterDeleted ||
              count == eMagicCounterPoolDeleted ) {
        ERR_POST(ObjFatal << "CObject::RemoveLastReference: "
                 "CObject is already deleted");
    }
    else {
        ERR_POST(ObjFatal << "CObject::RemoveLastReference: "
                 "CObject is corrupted");
    }
}


void CObject::ReleaseReference(void) const
{
    TCount count = m_Counter.Add(-eCounterStep);
    if ( ObjectStateValid(count) ) {
        return;
    }
    m_Counter.Add(eCounterStep); // undo

    // error
    if ( count == eMagicCounterDeleted ||
         count == eMagicCounterPoolDeleted ) {
        // deleted object
        NCBI_THROW(CObjectException, eCorrupted,
                   "CObject::ReleaseReference: "
                   "CObject is already deleted");
    }
    else {
        NCBI_THROW(CObjectException, eCorrupted,
                   "CObject::ReleaseReference: "
                   "CObject is corrupted");
    }
}


void CObject::DoNotDeleteThisObject(void)
{
    TCount count;
    {{
        CFastMutexGuard LOCK(sm_ObjectMutex);
        count = m_Counter.Get();
        if ( ObjectStateValid(count) ) {
            // valid and unreferenced
            // reset all 'in heap' flags -> make it non-heap without signature
            m_Counter.Add(-int(count & eStateBitsInHeapMask));
            return;
        }
    }}
    
    if ( count == eMagicCounterDeleted ||
         count == eMagicCounterPoolDeleted ) {
        // deleted object
        NCBI_THROW(CObjectException, eCorrupted,
                   "CObject::DoNotDeleteThisObject: "
                   "CObject is already deleted");
    }
    else {
        NCBI_THROW(CObjectException, eCorrupted,
                   "CObject::DoNotDeleteThisObject: "
                   "CObject is corrupted");
    }
}


void CObject::DoDeleteThisObject(void)
{
#ifndef USE_SINGLE_ALLOC
    TCount count;
    {{
        CFastMutexGuard LOCK(sm_ObjectMutex);
        count = m_Counter.Get();
        // DoDeleteThisObject is not allowed for stack objects
        enum {
            eCheckBits = eStateBitsValid | eStateBitsHeapSignature
        };

        if ( (count & eCheckBits) == TCount(eCheckBits) ) {
            if ( !(count & eStateBitsInHeap) ) {
                // set 'in heap' flag
                m_Counter.Add(eStateBitsInHeap);
            }
            return;
        }
    }}
    if ( ObjectStateValid(count) ) {
        ERR_POST(ObjFatal << "CObject::DoDeleteThisObject: "
                 "object was created without heap signature");
    }
    else if ( count == eMagicCounterDeleted ||
         count == eMagicCounterPoolDeleted ) {
        // deleted object
        NCBI_THROW(CObjectException, eCorrupted,
                   "CObject::DoDeleteThisObject: "
                   "CObject is already deleted");
    }
    else {
        NCBI_THROW(CObjectException, eCorrupted,
                   "CObject::DoDeleteThisObject: "
                   "CObject is corrupted");
    }
#endif
}


NCBI_PARAM_DECL(bool, NCBI, ABORT_ON_COBJECT_THROW);
NCBI_PARAM_DEF_EX(bool, NCBI, ABORT_ON_COBJECT_THROW, false,
                  eParam_NoThread, NCBI_ABORT_ON_COBJECT_THROW);

void CObjectException::x_InitErrCode(CException::EErrCode err_code)
{
    CCoreException::x_InitErrCode(err_code);
    static NCBI_PARAM_TYPE(NCBI, ABORT_ON_COBJECT_THROW) sx_abort_on_throw;
    if ( sx_abort_on_throw.Get() ) {
        Abort();
    }
}

void CObject::DebugDump(CDebugDumpContext ddc, unsigned int /*depth*/) const
{
    ddc.SetFrame("CObject");
    ddc.Log("address",  dynamic_cast<const CDebugDumpable*>(this), 0);
//    ddc.Log("memory starts at", dynamic_cast<const void*>(this));
//    ddc.Log("onHeap", CanBeDeleted());
}


NCBI_PARAM_DECL(bool, NCBI, ABORT_ON_NULL);
NCBI_PARAM_DEF_EX(bool, NCBI, ABORT_ON_NULL, false,
                  eParam_NoThread, NCBI_ABORT_ON_NULL);

void CObject::ThrowNullPointerException(void)
{
    static NCBI_PARAM_TYPE(NCBI, ABORT_ON_NULL) sx_abort_on_null;
    if ( sx_abort_on_null.Get() ) {
        Abort();
    }
    NCBI_EXCEPTION_VAR(ex,
        CCoreException, eNullPtr, "Attempt to access NULL pointer.");
    ex.SetSeverity(eDiag_Critical);
    NCBI_EXCEPTION_THROW(ex);
    // NCBI_THROW(CCoreException, eNullPtr, "Attempt to access NULL pointer.");
}


void CObjectCounterLocker::ReportIncompatibleType(const type_info& type)
{
#ifdef _DEBUG
    ERR_POST(Fatal <<
             "Type " << type.name() << " must be derived from CObject");
#else
    NCBI_THROW(CCoreException, eInvalidArg,
               string("Type ")+type.name()+" must be derived from CObject");
#endif
}

const char* CObjectException::GetErrCodeString(void) const
{
    switch (GetErrCode()) {
    case eRefDelete:    return "eRefDelete";
    case eDeleted:      return "eDeleted";
    case eCorrupted:    return "eCorrupted";
    case eRefOverflow:  return "eRefOverflow";
    case eNoRef:        return "eNoRef";
    case eRefUnref:     return "eRefUnref";
    default:    return CException::GetErrCodeString();
    }
}

END_NCBI_SCOPE

#ifdef USE_DEBUG_NEW

static bool s_EnvFlag(const char* env_var_name)
{
    const char* value = getenv(env_var_name);
    return value  &&  (*value == 'Y'  ||  *value == 'y' || *value == '1');
}


struct SAllocHeader
{
    unsigned magic;
    unsigned seq_number;
    size_t   size;
    void*    ptr;
};


struct SAllocFooter
{
    void*    ptr;
    size_t   size;
    unsigned seq_number;
    unsigned magic;
};

static const size_t kAllocSizeBefore = 32;
static const size_t kAllocSizeAfter = 32;

static const char kAllocFillBeforeArray = 0xba;
static const char kAllocFillBeforeOne   = 0xbb;
static const char kAllocFillInside      = 0xdd;
static const char kAllocFillAfter       = 0xaa;
static const char kAllocFillFree        = 0xee;

static const unsigned kAllocMagicHeader = 0x8b9b0b0b;
static const unsigned kAllocMagicFooter = 0x9e8e0e0e;
static const unsigned kFreedMagicHeader = 0x8b0bdead;
static const unsigned kFreedMagicFooter = 0x9e0edead;

static std::bad_alloc bad_alloc_instance;

DEFINE_STATIC_FAST_MUTEX(s_alloc_mutex);
static NCBI_NS_NCBI::CAtomicCounter seq_number;
static const size_t kLogSize = 64 * 1024;
struct SAllocLog {
    unsigned seq_number;
    enum EType {
        eEmpty = 0,
        eInit,
        eNew,
        eNewArr,
        eDelete,
        eDeleteArr
    };
    char   type;
    char   completed;
    size_t size;
    void*  ptr;
};
static SAllocLog alloc_log[kLogSize];


static inline SAllocHeader* get_header(void* ptr)
{
    return (SAllocHeader*) ((char*) ptr-kAllocSizeBefore);
}


static inline void* get_guard_before(SAllocHeader* header)
{
    return (char*) header + sizeof(SAllocHeader);
}


static inline size_t get_guard_before_size()
{
    return kAllocSizeBefore - sizeof(SAllocHeader);
}


static inline size_t get_extra_size(size_t size)
{
    return (-size) & 7;
}


static inline size_t get_guard_after_size(size_t size)
{
    return kAllocSizeAfter - sizeof(SAllocFooter) + get_extra_size(size);
}


static inline void* get_ptr(SAllocHeader* header)
{
    return (char*) header + kAllocSizeBefore;
}


static inline void* get_guard_after(SAllocFooter* footer, size_t size)
{
    return (char*)footer-get_guard_after_size(size);
}


static inline SAllocFooter* get_footer(SAllocHeader* header, size_t size)
{
    return (SAllocFooter*)((char*)get_ptr(header)+size+get_guard_after_size(size));
}


static inline size_t get_total_size(size_t size)
{
    return size+get_extra_size(size) + (kAllocSizeBefore+kAllocSizeAfter);
}


static inline size_t get_all_guards_size(size_t size)
{
    return get_total_size(size) - (sizeof(SAllocHeader)+sizeof(SAllocFooter));
}


SAllocLog& start_log(unsigned number, SAllocLog::EType type)
{
    SAllocLog& slot = alloc_log[number % kLogSize];
    slot.type = SAllocLog::eInit;
    slot.completed = false;
    slot.seq_number = number;
    slot.ptr = 0;
    slot.size = 0;
    slot.type = type;
    return slot;
}


void memchk(const void* ptr, char byte, size_t size)
{
    for ( const char* p = (const char*)ptr; size; ++p, --size ) {
        if ( *p != byte ) {
            std::abort();
        }
    }
}


void* s_alloc_mem(size_t size, bool array) throw()
{
    //fprintf(stderr, "s_alloc_mem(%u, %d)\n", (unsigned)size, array);
    unsigned number = seq_number.Add(1);
    SAllocLog& log =
        start_log(number, array? SAllocLog::eNewArr: SAllocLog::eNew);
    log.size = size;

    SAllocHeader* header;
    {{
        NCBI_NS_NCBI::CFastMutexGuard guard(s_alloc_mutex);
        header = (SAllocHeader*)std::malloc(get_total_size(size));
    }}
    if ( !header ) {
        log.completed = true;
        return 0;
    }
    SAllocFooter* footer = get_footer(header, size);

    header->magic = kAllocMagicHeader;
    footer->magic = kAllocMagicFooter;
    header->seq_number = footer->seq_number = number;
    header->size = footer->size = size;
    header->ptr = footer->ptr = log.ptr = get_ptr(header);

    std::memset(get_guard_before(header),
                array? kAllocFillBeforeArray: kAllocFillBeforeOne,
                get_guard_before_size());
    std::memset(get_guard_after(footer, size),
                kAllocFillAfter,
                get_guard_after_size(size));
    std::memset(get_ptr(header), kAllocFillInside, size);

    log.completed = true;
    return get_ptr(header);
}


void s_free_mem(void* ptr, bool array)
{
    //fprintf(stderr, "s_free_mem(%p, %d)\n", ptr, array);
    unsigned number = seq_number.Add(1);
    SAllocLog& log =
        start_log(number, array ? SAllocLog::eDeleteArr: SAllocLog::eDelete);
    if ( ptr ) {
        log.ptr = ptr;
        
        SAllocHeader* header = get_header(ptr);
        if ( header->magic != kAllocMagicHeader ||
             header->seq_number >= number ||
             header->ptr != get_ptr(header) ) {
            abort();
        }
        size_t size = log.size = header->size;
        SAllocFooter* footer = get_footer(header, size);
        if ( footer->magic      != kAllocMagicFooter ||
             footer->seq_number != header->seq_number ||
             footer->ptr        != get_ptr(header) ||
             footer->size       != size ) {
            abort();
        }
        
        memchk(get_guard_before(header),
               array? kAllocFillBeforeArray: kAllocFillBeforeOne,
               get_guard_before_size());
        memchk(get_guard_after(footer, size),
               kAllocFillAfter,
               get_guard_after_size(size));

        header->magic = kFreedMagicHeader;
        footer->magic = kFreedMagicFooter;
        footer->seq_number = number;
        static bool no_clear = s_EnvFlag("DEBUG_NEW_NO_FILL_ON_DELETE");
        if ( !no_clear ) {
            std::memset(get_guard_before(header),
                        kAllocFillFree,
                        get_all_guards_size(size));
        }
        static bool no_free = s_EnvFlag("DEBUG_NEW_NO_FREE_ON_DELETE");
        if ( !no_free ) {
            NCBI_NS_NCBI::CFastMutexGuard guard(s_alloc_mutex);
            std::free(header);
        }
    }
    log.completed = true;
}


void* operator new(size_t size) throw(std::bad_alloc)
{
    void* ret = s_alloc_mem(size, false);
    if ( !ret )
        throw bad_alloc_instance;
    return ret;
}


void* operator new(size_t size, const std::nothrow_t&) throw()
{
    return s_alloc_mem(size, false);
}


void* operator new[](size_t size) throw(std::bad_alloc)
{
    void* ret = s_alloc_mem(size, true);
    if ( !ret )
        throw bad_alloc_instance;
    return ret;
}


void* operator new[](size_t size, const std::nothrow_t&) throw()
{
    return s_alloc_mem(size, true);
}


void operator delete(void* ptr) throw()
{
    s_free_mem(ptr, false);
}


void  operator delete(void* ptr, const std::nothrow_t&) throw()
{
    s_free_mem(ptr, false);
}


void  operator delete[](void* ptr) throw()
{
    s_free_mem(ptr, true);
}


void  operator delete[](void* ptr, const std::nothrow_t&) throw()
{
    s_free_mem(ptr, true);
}

#endif



/*
 * ===========================================================================
 * $Log$
 * Revision 1.58  2006/11/29 15:19:07  grichenk
 * Throw null pointer exception with critical severity.
 *
 * Revision 1.57  2006/11/29 13:56:29  gouriano
 * Moved GetErrorCodeString method into cpp
 *
 * Revision 1.56  2006/07/17 14:17:27  ucko
 * RemoveLastReference, DoDeleteThisObject: Ensure that both sides of ==
 * have the same signedness to ensure correct behavior under VisualAge.
 *
 * Revision 1.55  2006/02/21 14:38:59  vasilche
 * Implemented templates CIRef and CIConstRef.
 *
 * Revision 1.54  2006/01/09 19:31:49  grichenk
 * Fixed names of env. variables.
 *
 * Revision 1.53  2006/01/05 20:40:17  grichenk
 * Added explicit environment variable name for params.
 * Added default value caching flag to CParam constructor.
 *
 * Revision 1.52  2005/11/17 18:47:18  grichenk
 * Replaced GetConfigXXX with CParam<>.
 *
 * Revision 1.51  2005/04/26 14:52:11  vasilche
 * Fixed warning.
 *
 * Revision 1.50  2005/04/26 14:08:33  vasilche
 * Allow allocation of CObjects from CObjectMemoryPool.
 * Documented CObject counter bits.
 *
 * Revision 1.49  2005/03/17 19:54:30  grichenk
 * DoDeleteThisObject() fails for objects not in heap.
 *
 * Revision 1.48  2005/03/14 17:03:37  vasilche
 * Allow to set NCBI_ABORT_ON_NULL and NCBI_ABORT_ON_COBJECT_THROW via registry.
 *
 * Revision 1.47  2004/09/30 18:35:17  vasilche
 * Relax check for ref counter to allow temporary references.
 *
 * Revision 1.46  2004/08/04 14:35:33  vasilche
 * Renamed debug alloc functions to avoid name clash with system libraries. Changed memory filling constants.
 *
 * Revision 1.45  2004/05/14 13:59:27  gorelenk
 * Added include of ncbi_pch.hpp
 *
 * Revision 1.44  2004/04/05 15:58:43  vakatov
 * CObject::InitCounter() -- ATTENTION:  this code can cause UMR -- it's okay
 *
 * Revision 1.43  2004/02/19 16:44:19  vasilche
 * Modified debug new/delete code for 64 bit builds.
 *
 * Revision 1.42  2004/02/17 21:08:19  vasilche
 * Added code to debug memory allocation problems (currently commented out).
 *
 * Revision 1.41  2003/12/17 20:10:07  vasilche
 * Avoid throwing exceptions from destructors.
 * Display Fatal message in Debug mode and Critical message in Release mode.
 *
 * Revision 1.40  2003/09/17 15:58:29  vasilche
 * Allow debug abort when:
 * CObjectException is thrown - env var NCBI_ABORT_ON_COBJECT_THROW=[Yy1],
 * CNullPointerException is thrown - env var NCBI_ABORT_ON_NULL=[Yy1].
 * Allow quit abort in debug mode and coredump in release mode:
 * env var DIAG_SILENT_ABORT=[Yy1Nn0].
 *
 * Revision 1.39  2003/09/17 15:20:46  vasilche
 * Moved atomic counter swap functions to separate file.
 * Added CRef<>::AtomicResetFrom(), CRef<>::AtomicReleaseTo() methods.
 *
 * Revision 1.38  2003/08/12 12:06:58  siyan
 * Changed name of implementation for AddReferenceOverflow(). It is now
 * CheckReferenceOverflow().
 *
 * Revision 1.37  2003/07/17 23:01:32  vasilche
 * Added matching operator delete.
 *
 * Revision 1.36  2003/07/17 20:01:07  vasilche
 * Added inplace operator new().
 *
 * Revision 1.35  2003/03/06 19:40:52  ucko
 * InitCounter: read m_Value directly rather than going through Get(), which
 * would end up spinning forever if it came across NCBI_COUNTER_RESERVED_VALUE.
 *
 * Revision 1.34  2002/11/27 12:53:45  dicuccio
 * Added CObject::ThrowNullPointerException() to get around some inlining issues.
 *
 * Revision 1.33  2002/09/20 13:52:46  vasilche
 * Renamed sm_Mutex -> sm_AtomicCounterMutex
 *
 * Revision 1.32  2002/09/19 20:05:43  vasilche
 * Safe initialization of static mutexes
 *
 * Revision 1.31  2002/08/28 17:05:50  vasilche
 * Remove virtual inheritance, fixed heap detection
 *
 * Revision 1.30  2002/07/15 18:17:24  gouriano
 * renamed CNcbiException and its descendents
 *
 * Revision 1.29  2002/07/11 14:18:27  gouriano
 * exceptions replaced by CNcbiException-type ones
 *
 * Revision 1.28  2002/05/29 21:17:57  gouriano
 * minor change in debug dump
 *
 * Revision 1.27  2002/05/28 17:59:55  gouriano
 * minor modification od DebugDump
 *
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
