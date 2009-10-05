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
 * Author: Pavel Ivanov
 */

#include <ncbi_pch.hpp>

#include <db/sqlite/sqlitewrapp.hpp>

#include "nc_memory.hpp"

#ifdef NCBI_OS_MSWIN
#  include <windows.h>
#else
#  include <sys/types.h>
#  include <sys/stat.h>
#  include <sys/mman.h>
#  include <fcntl.h>
#endif


BEGIN_NCBI_SCOPE


/// Default global limit on memory consumption
static const size_t kNCMMDefMemoryLimit        = 1024 * 1024 * 1024;
/// Maximum size of small block (which is less than size of memory chunk)
/// manager can allocate
static const unsigned int kNCMMMaxSmallSize    =
      (kNCMMSetDataSize / 2  - kNCMMBlockExtraSize) & ~7;
/// List of all distinct sizes of small blocks memory manager can allocate
static const unsigned int kNCMMSmallSize[kNCMMCntSmallSizes] =
    { 8, 16, 24, 32, 40, 48, 56, 64, 72, 80, 96, 112, 128, 152, 176, 208, 248,
      296, 352, 416, 496, 592, 704, 840, 1008, 1208, 1448, 1736, 2080, 2496,
      (kNCMMSetDataSize / 11 - kNCMMBlockExtraSize) & ~7,
      (kNCMMSetDataSize / 9  - kNCMMBlockExtraSize) & ~7,
      (kNCMMSetDataSize / 8  - kNCMMBlockExtraSize) & ~7,
      (kNCMMSetDataSize / 7  - kNCMMBlockExtraSize) & ~7,
      (kNCMMSetDataSize / 6  - kNCMMBlockExtraSize) & ~7,
      (kNCMMSetDataSize / 5  - kNCMMBlockExtraSize) & ~7,
      (kNCMMSetDataSize / 4  - kNCMMBlockExtraSize) & ~7,
      (kNCMMSetDataSize / 3  - kNCMMBlockExtraSize) & ~7,
      kNCMMMaxSmallSize };
/// List of indexes inside kNCMMSmallSize for each block size that can be
/// requested. Indexes are stored for each size divided by 8 because
/// non-divisible sizes will not be allocated anyway.
/// Array is initialized during manager initialization.
static unsigned int kNCMMSmallSizeIndex[kNCMMMaxSmallSize / 8 + 1] = {0};
/// Number of blocks that fits into one blocks set for each block size.
/// Array is initialized during manager initialization.
static unsigned int kNCMMBlocksPerSet  [kNCMMCntSmallSizes]        = {0};
/// Number of blocks that fits into one emptiness grade of blocks set.
/// Array is initialized during manager initialization.
static unsigned int kNCMMBlocksPerGrade[kNCMMCntSmallSizes]        = {0};
/// Number of bytes lost at the end of blocks set for each block size.
/// Array is initialized during manager initialization.
static unsigned int kNCMMLostPerSet    [kNCMMCntSmallSizes]        = {0};
/// Number of bytes used by manager in each blocks set (different for each
/// block size). Memory mentioned is used by manager as opposed to the one
/// used by real data requested by outside user.
/// Array is initialized during manager initialization.
static unsigned int kNCMMOverheadPerSet[kNCMMCntSmallSizes]        = {0};
/// Number of chunks in each slab's emptiness grade.
/// The formula is taken from s_GetCntPerGrade() and should be exact copy of
/// it. It's repeated here just to make this a compile-time constant.
static const unsigned int kNCMMChunksPerSlabGrade
                      = kNCMMCntChunksInSlab / (kNCMMSlabEmptyGrades - 1) + 1;

/// Size of CNCMMSizePool[] array containing pools for each block size that
/// memory manager can allocate. This array is allocated for each thread
/// (up to kNCMMMaxThreadsCnt instances).
static const size_t kNCMMSizePoolArraySize
                                 = sizeof(CNCMMSizePool) * kNCMMCntSmallSizes;


/// "Magic" minimum size of block requested by SQLite that should be converted
/// to allocation of maximum size - kNCMMSQLiteBigBlockReal. This constant
/// basically means that on any request from SQLite for allocation of chunks
/// chain with more than 2 chunks the actual allocation will contain at least
/// kNCMMSQLiteBigBlockReal bytes.
///
/// @sa kNCMMSQLiteBigBlockReal, s_SQLITE_Mem_Malloc
static const int kNCMMSQLiteBigBlockMin = 2 * kNCMMChunkSize - kNCMMSetBaseSize;
/// The real size of big block that will be allocated for SQLite if it
/// requests for something greater than kNCMMSQLiteBigBlockMin.
///
/// @sa kNCMMSQLiteBigBlockMin, s_SQLITE_Mem_Malloc
static const int kNCMMSQLiteBigBlockReal = 2010000;

/// "The goal" for hash-table in database cache on the number of pages to have
/// for each hash value.
static const int    kNCMMDBHashSizeFactor     = 8;
/// Frequency of background thread iterations in seconds between them.
static const int    kNCMMBGThreadWaitSecs     = 5;
/// Number of chunks that manager allows to be used on top of global limit for
/// memory consumption.
static const size_t kNCMMLimitToleranceChunks = 10;


/// Size of memory page that is a granularity of all allocations from OS.
static const size_t kNCMMMemPageSize  = 4 * 1024;
/// Mask that can move pointer address or memory size to the memory page
/// boundary.
static const size_t kNCMMMemPageAlignMask = ~(kNCMMMemPageSize - 1);

#ifdef NCBI_OS_MSWIN

/// Combination of flags used as "type" parameter to VirtAlloc().
static const DWORD kNCVirtAllocType = MEM_RESERVE | MEM_COMMIT | MEM_TOP_DOWN;
/// Combination of flags used as "permission" parameter to VirtAlloc().
static const DWORD kNCVirtAllocPerm = PAGE_READWRITE;
/// Combination of flags used as "type" parameter to VirtFree().
static const DWORD kNCVirtFreeType  = MEM_RELEASE;

#else

/// Combination of flags used as "protection" parameter to mmap().
static const int kNCMmapProtection  = PROT_READ | PROT_WRITE;

#endif


DEFINE_CLASS_STATIC_FAST_MUTEX(CNCMMDBPage::sm_LRULock);
CNCMMDBPage* CNCMMDBPage::sm_LRUHead = NULL;
CNCMMDBPage* CNCMMDBPage::sm_LRUTail = NULL;

DEFINE_CLASS_STATIC_FAST_MUTEX(CNCMMChunksPool_Getter::sm_CreateLock);
CNCMMChunksPool  CNCMMChunksPool_Getter::sm_Pools   [kNCMMMaxThreadsCnt];
CNCMMChunksPool* CNCMMChunksPool_Getter::sm_PoolPtrs[kNCMMMaxThreadsCnt]
                                                                     = {NULL};
unsigned int     CNCMMChunksPool_Getter::sm_CntUsed     = 0;

CNCMMChunksPool_Getter  CNCMMChunksPool::sm_Getter;

DEFINE_CLASS_STATIC_FAST_MUTEX(CNCMMChainsPool_Getter::sm_CreateLock);
CNCMMChainsPool  CNCMMChainsPool_Getter::sm_Pools   [kNCMMMaxThreadsCnt];
CNCMMChainsPool* CNCMMChainsPool_Getter::sm_PoolPtrs[kNCMMMaxThreadsCnt]
                                                                     = {NULL};
unsigned int     CNCMMChainsPool_Getter::sm_CntUsed     = 0;

CNCMMChainsPool_Getter  CNCMMChainsPool::sm_Getter;

CNCMMReserve CNCMMReserve::sm_Instances[kNCMMSlabEmptyGrades];

DEFINE_CLASS_STATIC_FAST_MUTEX(CNCMMSizePool_Getter::sm_CreateLock);
CNCMMSizePool* CNCMMSizePool_Getter::sm_PoolPtrs[kNCMMMaxThreadsCnt] = {NULL};
unsigned int   CNCMMSizePool_Getter::sm_RefCnts [kNCMMMaxThreadsCnt] = {0};

CNCMMSizePool_Getter  CNCMMSizePool::sm_Getter;
CFastRWLock*          CNCMMSizePool::sm_DestructLock = NULL;

CNCMMStats               CNCMMStats::sm_Stats   [kNCMMMaxThreadsCnt];

CNCMMStats_Getter        CNCMMStats::sm_Getter;

DEFINE_CLASS_STATIC_FAST_MUTEX(CNCMMCentral::sm_CentralLock);
CNCMMStats      CNCMMCentral::sm_Stats;
bool            CNCMMCentral::sm_Initialized = false;
ENCMMMode       CNCMMCentral::sm_Mode        = eNCMemStable;
size_t          CNCMMCentral::sm_MemLimit    = kNCMMDefMemoryLimit;
CAtomicCounter  CNCMMCentral::sm_CntCanAlloc;
CNCMMBGThread*  CNCMMCentral::sm_BGThread    = NULL;



/// Get number of blocks (or something else) per each emptiness grade when
/// given total number of blocks and number of grades.
static inline unsigned int
s_GetCntPerGrade(unsigned int total, unsigned int grades)
{
    return total / (grades - 1) + 1;
}

/// Get emptiness grade value when given current number of blocks (or
/// something else) and number of blocks per each grade.
static inline unsigned int
s_GetGradeValue(unsigned int cur_cnt, unsigned int cnt_per_grade)
{
    return (cur_cnt + cnt_per_grade - 1) / cnt_per_grade;
}


inline void
CNCMMCentral::SetMemLimit(size_t mem_size)
{
    sm_MemLimit = mem_size;
}

inline size_t
CNCMMCentral::GetMemLimit(void)
{
    return sm_MemLimit;
}

inline CNCMMStats&
CNCMMCentral::GetStats(void)
{
    return sm_Stats;
}


inline
CNCMMStats::CNCMMStats(void)
{}

inline void
CNCMMStats::InitInstance(void)
{
    memset(this, 0, sizeof(*this));
    m_ObjLock.InitializeDynamic();
}

void
CNCMMStats::x_CollectAllTo(CNCMMStats* other)
{
    CFastMutexGuard guard(m_ObjLock);

    other->m_SystemMem         += m_SystemMem;
    other->m_FreeMem           += m_FreeMem;
    other->m_ReservedMem       += m_ReservedMem;
    other->m_OverheadMem       += m_OverheadMem;
    other->m_LostMem           += m_LostMem;
    other->m_DBCacheMem        += m_DBCacheMem;
    other->m_DataMem           += m_DataMem;
    other->m_SysAllocs         += m_SysAllocs;
    other->m_SysFrees          += m_SysFrees;
    other->m_SysReallocs       += m_SysReallocs;
    other->m_ChunksPoolRefills += m_ChunksPoolRefills;
    other->m_ChunksPoolCleans  += m_ChunksPoolCleans;
    other->m_SlabsAlloced      += m_SlabsAlloced;
    other->m_SlabsFreed        += m_SlabsFreed;
    other->m_BigAlloced        += m_BigAlloced;
    other->m_BigFreed          += m_BigFreed;
    other->m_DBPagesRequested  += m_DBPagesRequested;
    other->m_DBPagesHit        += m_DBPagesHit;
    for (unsigned int i = 0; i < kNCMMCntSmallSizes; ++i) {
        other->m_BlocksAlloced[i] += m_BlocksAlloced[i];
        other->m_BlocksFreed[i]   += m_BlocksFreed[i];
        other->m_SetsCreated[i]   += m_SetsCreated[i];
        other->m_SetsDeleted[i]   += m_SetsDeleted[i];
    }
    for (unsigned int i = 0; i < kNCMMCntChunksInSlab - 1; ++i) {
        other->m_ChainsAlloced[i] += m_ChainsAlloced[i];
        other->m_ChainsFreed[i]   += m_ChainsFreed[i];
    }
}

inline void
CNCMMStats::SysMemAlloced(size_t alloced_size, size_t asked_size)
{
    CFastMutexGuard guard(m_ObjLock);
    ++m_SysAllocs;
    m_SystemMem += alloced_size;
    m_LostMem   += alloced_size - asked_size;
}

inline void
CNCMMStats::SysMemFreed(size_t alloced_size, size_t asked_size)
{
    CFastMutexGuard guard(m_ObjLock);
    ++m_SysFrees;
    m_SystemMem -= alloced_size;
    m_LostMem   -= alloced_size -= asked_size;
}

inline void
CNCMMStats::SysMemRealloced(void)
{
    CFastMutexGuard guard(m_ObjLock);
    ++m_SysReallocs;
}

inline void
CNCMMStats::OverheadMemAlloced(size_t mem_size)
{
    CNCMMStats* stat = sm_Getter.GetObjPtr();
    CFastMutexGuard guard(stat->m_ObjLock);
    stat->m_OverheadMem += mem_size;
}

inline void
CNCMMStats::OverheadMemFreed(size_t mem_size)
{
    CNCMMStats* stat = sm_Getter.GetObjPtr();
    CFastMutexGuard guard(stat->m_ObjLock);
    stat->m_OverheadMem -= mem_size;
}

inline void
CNCMMStats::FreeChunkCreated(void)
{
    CNCMMStats* stat = sm_Getter.GetObjPtr();
    CFastMutexGuard guard(stat->m_ObjLock);
    stat->m_FreeMem += kNCMMChunkSize;
}

inline void
CNCMMStats::FreeChunkDeleted(void)
{
    CNCMMStats* stat = sm_Getter.GetObjPtr();
    CFastMutexGuard guard(stat->m_ObjLock);
    stat->m_FreeMem -= kNCMMChunkSize;
}

inline void
CNCMMStats::ReservedMemCreated(size_t mem_size)
{
    CNCMMStats* stat = sm_Getter.GetObjPtr();
    CFastMutexGuard guard(stat->m_ObjLock);
    stat->m_ReservedMem += mem_size;
}

inline void
CNCMMStats::ReservedMemDeleted(size_t mem_size)
{
    CNCMMStats* stat = sm_Getter.GetObjPtr();
    CFastMutexGuard guard(stat->m_ObjLock);
    stat->m_ReservedMem -= mem_size;
}

inline void
CNCMMStats::ChunksPoolRefilled(void)
{
    CNCMMStats* stat = sm_Getter.GetObjPtr();
    CFastMutexGuard guard(stat->m_ObjLock);
    ++stat->m_ChunksPoolRefills;
}

inline void
CNCMMStats::ChunksPoolCleaned(void)
{
    CNCMMStats* stat = sm_Getter.GetObjPtr();
    CFastMutexGuard guard(stat->m_ObjLock);
    ++stat->m_ChunksPoolCleans;
}

inline void
CNCMMStats::DBPageCreated(void)
{
    CNCMMStats* stat = sm_Getter.GetObjPtr();
    CFastMutexGuard guard(stat->m_ObjLock);
    stat->m_DBCacheMem  += kNCMMDBPageDataSize;
    stat->m_OverheadMem += kNCMMChunkSize - kNCMMDBPageDataSize;
}

inline void
CNCMMStats::DBPageDeleted(void)
{
    CNCMMStats* stat = sm_Getter.GetObjPtr();
    CFastMutexGuard guard(stat->m_ObjLock);
    stat->m_DBCacheMem  -= kNCMMDBPageDataSize;
    stat->m_OverheadMem -= kNCMMChunkSize - kNCMMDBPageDataSize;
}

inline void
CNCMMStats::BlocksSetCreated(unsigned int size_index)
{
    CNCMMStats* stat = sm_Getter.GetObjPtr();
    CFastMutexGuard guard(stat->m_ObjLock);
    stat->m_LostMem     += kNCMMLostPerSet    [size_index];
    stat->m_OverheadMem += kNCMMOverheadPerSet[size_index];
    ++stat->m_SetsCreated                     [size_index];
}

inline void
CNCMMStats::BlocksSetDeleted(unsigned int size_index)
{
    CNCMMStats* stat = sm_Getter.GetObjPtr();
    CFastMutexGuard guard(stat->m_ObjLock);
    stat->m_LostMem     -= kNCMMLostPerSet    [size_index];
    stat->m_OverheadMem -= kNCMMOverheadPerSet[size_index];
    ++stat->m_SetsDeleted                     [size_index];
}

inline void
CNCMMStats::SlabCreated(void)
{
    CNCMMStats* stat = sm_Getter.GetObjPtr();
    CFastMutexGuard guard(stat->m_ObjLock);
    stat->m_LostMem += kNCMMSlabSize - sizeof(CNCMMSlab);
    ++stat->m_SlabsAlloced;
}

inline void
CNCMMStats::SlabDeleted(void)
{
    CNCMMStats* stat = sm_Getter.GetObjPtr();
    CFastMutexGuard guard(stat->m_ObjLock);
    stat->m_LostMem -= kNCMMSlabSize - sizeof(CNCMMSlab);
    ++stat->m_SlabsFreed;
}

inline void
CNCMMStats::MemBlockAlloced(unsigned int size_index)
{
    CNCMMStats* stat = sm_Getter.GetObjPtr();
    CFastMutexGuard guard(stat->m_ObjLock);
    ++stat->m_BlocksAlloced          [size_index];
    stat->m_DataMem += kNCMMSmallSize[size_index];
}

inline void
CNCMMStats::MemBlockFreed(unsigned int size_index)
{
    CNCMMStats* stat = sm_Getter.GetObjPtr();
    CFastMutexGuard guard(stat->m_ObjLock);
    ++stat->m_BlocksFreed            [size_index];
    stat->m_DataMem -= kNCMMSmallSize[size_index];
}

inline void
CNCMMStats::ChunksChainAlloced(size_t       block_size,
                               unsigned int chain_size)
{
    CNCMMStats* stat = sm_Getter.GetObjPtr();
    CFastMutexGuard guard(stat->m_ObjLock);
    stat->m_DataMem     += block_size;
    stat->m_OverheadMem += kNCMMSetBaseSize;
    stat->m_LostMem     += chain_size * kNCMMChunkSize
                                            - (kNCMMSetBaseSize + block_size);
    ++stat->m_ChainsAlloced[chain_size];
}

inline void
CNCMMStats::ChunksChainFreed(size_t       block_size,
                                      unsigned int chain_size)
{
    CNCMMStats* stat = sm_Getter.GetObjPtr();
    CFastMutexGuard guard(stat->m_ObjLock);
    stat->m_DataMem     -= block_size;
    stat->m_OverheadMem -= kNCMMSetBaseSize;
    stat->m_LostMem     -= chain_size * kNCMMChunkSize
                                            - (kNCMMSetBaseSize + block_size);
    ++stat->m_ChainsFreed[chain_size];
}

inline void
CNCMMStats::BigBlockAlloced(size_t block_size)
{
    CNCMMStats* stat = sm_Getter.GetObjPtr();
    CFastMutexGuard guard(stat->m_ObjLock);
    stat->m_DataMem     += block_size;
    stat->m_OverheadMem += sizeof(CNCMMBigBlockSlab);
    ++stat->m_BigAlloced;
}

inline void
CNCMMStats::BigBlockFreed(size_t block_size)
{
    CNCMMStats* stat = sm_Getter.GetObjPtr();
    CFastMutexGuard guard(stat->m_ObjLock);
    stat->m_DataMem     -= block_size;
    stat->m_OverheadMem -= sizeof(CNCMMBigBlockSlab);
    ++stat->m_BigFreed;
}

inline void
CNCMMStats::DBPageHitInCache(void)
{
    CNCMMStats* stat = sm_Getter.GetObjPtr();
    CFastMutexGuard guard(stat->m_ObjLock);
    ++stat->m_DBPagesRequested;
    ++stat->m_DBPagesHit;
}

inline void
CNCMMStats::DBPageNotInCache(void)
{
    CNCMMStats* stat = sm_Getter.GetObjPtr();
    CFastMutexGuard guard(stat->m_ObjLock);
    ++stat->m_DBPagesRequested;
}

inline size_t
CNCMMStats::GetSystemMem(void)
{
    return m_SystemMem;
}

inline void
CNCMMStats::CollectAllStats(CNCMMStats* dest_stats)
{
    dest_stats->InitInstance();
    CNCMMCentral::GetStats().x_CollectAllTo(dest_stats);
    for (unsigned int i = 0; i < kNCMMMaxThreadsCnt; ++i) {
        sm_Stats[i].x_CollectAllTo(dest_stats);
    }
}

inline void
CNCMMStats::GetUsageNumbers(size_t* free_mem, size_t* db_cache_mem)
 {
    *free_mem = *db_cache_mem = 0;
    for (unsigned int i = 0; i < kNCMMMaxThreadsCnt; ++i) {
        *free_mem     += sm_Stats[i].m_FreeMem;
        *db_cache_mem += sm_Stats[i].m_DBCacheMem;
    }
}

inline void
CNCMMStats::Print(CPrintTextProxy& proxy)
{
    proxy << "Memory limit - " << CNCMMCentral::GetMemLimit()
                               << " (real used - "
                               << m_SystemMem - m_FreeMem << ")" << endl;
    proxy << "Memory used - " << m_SystemMem   << " (sys), "
                              << m_DataMem     << " (data), "
                              << m_DBCacheMem  << " (db cache), "
                              << m_FreeMem     << " (free), "
                              << m_ReservedMem << " (reserved), "
                              << m_OverheadMem << " (overhead), "
                              << m_LostMem     << " (lost)" << endl
          << "Sys calls   - " << m_SysAllocs        << " (allocs), "
                              << m_SysFrees         << " (frees), "
                              << m_SysReallocs      << " (reallocs), "
                              << m_ChunksPoolRefills << " (pool refills), "
                              << m_ChunksPoolCleans << " (pool releases)" << endl
          << "Cache hit   - " << int(double(m_DBPagesHit)
                                     / m_DBPagesRequested * 100) << "%" << endl
          << "Allocations - " << m_SlabsAlloced << " (+slabs), "
                              << m_SlabsFreed   << " (-slabs), "
                              << m_BigAlloced   << " (+big), "
                              << m_BigFreed     << " (-big)" << endl
          << "By size:" << endl;
    for (unsigned int i = 0; i < kNCMMCntSmallSizes; ++i) {
        proxy << kNCMMSmallSize[i]  << " - "
              << m_BlocksAlloced[i] << " (+blocks), "
              << m_BlocksFreed[i]   << " (-blocks), "
              << m_SetsCreated[i]   << " (+sets), "
              << m_SetsDeleted[i]   << " (-sets)" << endl;
    }
    Uint8 other_alloced = 0, other_freed = 0;
    for (unsigned int i = 1; i <= kNCMMCntChunksInSlab; ++i) {
        // A bit of hard coding - NetCache doesn't allocate other chain sizes
        // anyway.
        if (i == 1  ||  i == 2  ||  i == kNCMMCntChunksInSlab - 2) {
            proxy << (i * kNCMMChunkSize - kNCMMSetBaseSize) << " - "
                  << m_ChainsAlloced[i] << " (+blocks), "
                  << m_ChainsFreed[i]   << " (-blocks)" << endl;
        }
        else {
            other_alloced += m_ChainsAlloced[i];
            other_freed   += m_ChainsFreed  [i];
        }
    }
    proxy << "other - " << other_alloced << " (+blocks), "
                        << other_freed   << " (-blocks)" << endl;
}


inline
CNCMMStats_Getter::CNCMMStats_Getter(void)
{}

inline void
CNCMMStats_Getter::Initialize(void)
{
    TBase::Initialize();
    for (unsigned int i = 0; i < kNCMMMaxThreadsCnt; ++i) {
        CNCMMStats::sm_Stats[i].InitInstance();
    }
    GetObjPtr();
}

CNCMMStats*
CNCMMStats_Getter::CreateTlsObject(void)
{
    unsigned int index;
#ifdef NCBI_OS_MSWIN
    // Just to be consistent with other pools use single object on Windows.
    index = 0;
#else
    index = g_GetNCThreadIndex() % kNCMMMaxThreadsCnt;
#endif  // NCBI_OS_MSWIN

    return &CNCMMStats::sm_Stats[index];
}

void
CNCMMStats_Getter::DeleteTlsObject(void* mem_ptr)
{}


inline void
CNCMMStats::Initialize(void)
{
    sm_Getter.Initialize();
}


// Special location for the method to be before all methods using it.
inline unsigned int
CNCMMSlab::GetEmptyGrade(void)
{
    return m_EmptyGrade;
}


inline void*
CNCMMCentral::x_AlignToBottom(void* ptr)
{
    return reinterpret_cast<void*>(
                              reinterpret_cast<size_t>(ptr) & kNCMMAlignMask);
}

inline void*
CNCMMCentral::x_AlignToTop(void* ptr)
{
    return x_AlignToBottom(static_cast<char*>(ptr) + kNCMMAlignSize - 1);
}

inline size_t
CNCMMCentral::x_AlignSizeToPage(size_t size)
{
    return (size + kNCMMMemPageSize - 1) & kNCMMMemPageAlignMask;
}

#ifndef NCBI_OS_MSWIN

inline void*
CNCMMCentral::x_DoCallMmap(size_t size)
{
    void* ptr;
#ifdef MAP_ANONYMOUS
    ptr = mmap(NULL, size, kNCMmapProtection,
               MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
#else
    static int fd = ::open("/dev/zero", O_RDWR);
    ptr = mmap(NULL, size, kNCMmapProtection,
               MAP_PRIVATE,                 fd, 0);
#endif
    _ASSERT(ptr != MAP_FAILED);
    return ptr;
}

#endif

void
CNCMMCentral::x_AllocAlignedLongWay(void*& mem_ptr, size_t size)
{
    sm_Stats.SysMemRealloced();

#ifdef NCBI_OS_MSWIN

    // Release what we've already obtained
    VirtualFree(mem_ptr, 0, kNCVirtFreeType);
    // Get address space where there's aligned block for sure
    mem_ptr = VirtualAlloc(NULL, size + kNCMMAlignSize,
                           kNCVirtAllocType, kNCVirtAllocPerm);
    void* aligned_ptr = x_AlignToTop(mem_ptr);

    // Re-alloc into the same address space as aligned address
    VirtualFree(mem_ptr, 0, kNCVirtFreeType);
    mem_ptr = VirtualAlloc(aligned_ptr, size,
                           kNCVirtAllocType, kNCVirtAllocPerm);

    // If it will be allocated in another memory then we're doomed and cannot
    // do anything on Windows.
    _ASSERT(mem_ptr == aligned_ptr);

#else

    // Release what we've already obtained.
    // Cast to char* in all unmap()s is necessary for WorkShop and harmless
    // for other compilers.
    munmap(static_cast<char*>(mem_ptr), size);

    // Get address space where there's aligned block for sure
    mem_ptr = x_DoCallMmap(size + kNCMMAlignSize);
    void* aligned_ptr = x_AlignToTop(mem_ptr);

    // Release parts of allocated block before and after aligned one
    size_t before_size = static_cast<char*>(aligned_ptr)
                         - static_cast<char*>(mem_ptr);
    if (before_size != 0)
        munmap(static_cast<char*>(mem_ptr), before_size);

    size_t after_size = kNCMMAlignSize - before_size;
    if (after_size != 0) {
        void* after_ptr = static_cast<char*>(aligned_ptr) + size;
        munmap(static_cast<char*>(after_ptr), after_size);
    }

    mem_ptr = aligned_ptr;

#endif
}

inline void*
CNCMMCentral::x_DoAlloc(size_t& size)
{
    size_t unaligned_size = size;
    size = x_AlignSizeToPage(size);

    void* mem_ptr =
#ifdef NCBI_OS_MSWIN
            VirtualAlloc(NULL, size, kNCVirtAllocType, kNCVirtAllocPerm);
#else
            x_DoCallMmap(size);
#endif

    sm_Stats.SysMemAlloced(size, unaligned_size);
    return mem_ptr;
}

inline void*
CNCMMCentral::SysAlloc(size_t size)
{
    CFastMutexGuard guard(sm_CentralLock);
    return x_DoAlloc(size);
}

inline void*
CNCMMCentral::SysAllocAligned(size_t size)
{
    CFastMutexGuard guard(sm_CentralLock);

    void* mem_ptr = x_DoAlloc(size);
    void* aligned_ptr = x_AlignToBottom(mem_ptr);
    if (aligned_ptr != mem_ptr) {
        x_AllocAlignedLongWay(mem_ptr, size);
    }
    return mem_ptr;
}

inline void
CNCMMCentral::SysFree(void* ptr, size_t size)
{
    CFastMutexGuard guard(sm_CentralLock);
    size_t aligned_size = x_AlignSizeToPage(size);

#ifdef NCBI_OS_MSWIN
    VirtualFree(ptr, 0, kNCVirtFreeType);
#else
    // Cast to char* is necessary for WorkShop and harmless for other
    // compilers.
    munmap(static_cast<char*>(ptr), aligned_size);
#endif

    sm_Stats.SysMemFreed(aligned_size, size);
}

inline CNCMMSlab*
CNCMMCentral::GetSlab(void* mem_ptr)
{
    return static_cast<CNCMMSlab*>(x_AlignToBottom(mem_ptr));
}

inline ENCMMMode
CNCMMCentral::GetMemMode(void)
{
    if (sm_Stats.GetSystemMem() < sm_MemLimit)
        return eNCMemGrowing;
    else
        return sm_Mode;
}

inline void
CNCMMCentral::DBPageCreated(void)
{
    CNCMMStats::DBPageCreated();
    if (sm_Mode == eNCMemGrowing  &&  sm_CntCanAlloc.Add(-1) == 0) {
        sm_Mode = eNCMemStable;
    }
}

inline void
CNCMMCentral::DBPageDeleted(void)
{
    CNCMMStats::DBPageDeleted();
    if (sm_Mode == eNCMemShrinking  &&  sm_CntCanAlloc.Add(1) == 0) {
        sm_Mode = eNCMemStable;
    }
}


inline
CNCMMFreeChunkBase::CNCMMFreeChunkBase(void)
{}

inline CNCMMFreeChunk*
CNCMMFreeChunkBase::GetChainStart(void)
{
    return m_ChainStart;
}

inline unsigned int
CNCMMFreeChunkBase::GetChainSize(void)
{
    return m_ChainSize;
}

inline unsigned int
CNCMMFreeChunkBase::GetSlabGrade(void)
{
    return m_SlabGrade;
}


inline
CNCMMFreeChunk::CNCMMFreeChunk(void)
{
    CNCMMStats::FreeChunkCreated();
}

inline
CNCMMFreeChunk::~CNCMMFreeChunk(void)
{
    CNCMMStats::FreeChunkDeleted();
}

inline void
CNCMMFreeChunk::MarkChain(unsigned int chain_size, unsigned int slab_grade)
{
    CNCMMFreeChunk* chain_end = this + (chain_size - 1);
    m_ChainStart = chain_end->m_ChainStart = this;
    m_ChainSize  = chain_end->m_ChainSize  = chain_size;
    m_PrevChain  = NULL;
    m_NextChain  = NULL;
    m_SlabGrade  = chain_end->m_SlabGrade  = slab_grade;
}

inline void*
CNCMMFreeChunk::operator new(size_t, void* mem_ptr)
{
    return mem_ptr;
}

inline void
CNCMMFreeChunk::operator delete(void*, void*)
{}

#ifdef NCBI_OS_MSWIN
// For some weird reason MSVC needs this function though it's never used.
inline void
CNCMMFreeChunk::operator delete(void*)
{}
#endif // NCBI_OS_MSWIN

inline void
CNCMMFreeChunk::DestroyChain(unsigned int chain_size)
{
    CNCMMFreeChunk* chain_end = this + chain_size;
    for (CNCMMFreeChunk* chunk = this; chunk != chain_end; ++chunk) {
        chunk->~CNCMMFreeChunk();
    }
}

inline CNCMMFreeChunk*
CNCMMFreeChunk::ConstructChain(void* mem_ptr, unsigned int chain_size)
{
    CNCMMFreeChunk* chain_start = new (mem_ptr) CNCMMFreeChunk();
    CNCMMFreeChunk* chain_end   = chain_start + chain_size;
    for (CNCMMFreeChunk* chunk = chain_start + 1; chunk != chain_end; ++chunk)
    {
        new (chunk) CNCMMFreeChunk();
    }
    return chain_start;
}


inline void
SNCMMChainInfo::Initialize(void)
{
    start      = NULL;
    size       = 0;
    slab_grade = 0;
}

inline void
SNCMMChainInfo::AssignFromChain(CNCMMFreeChunk* chain)
{
    start      = chain->GetChainStart();
    size       = chain->GetChainSize ();
    slab_grade = chain->GetSlabGrade ();
}


inline void
CNCMMChunksPool::InitInstance(void)
{
    m_ObjLock.InitializeDynamic();
    m_GetPtr = m_PutPtr = &m_Chunks[0];
    m_RefCnt.Set(0);
}

inline void
CNCMMChunksPool::AddRef(void)
{
    m_RefCnt.Add(1);
}

inline unsigned int
CNCMMChunksPool::ReleaseRef(void)
{
    unsigned int ref_cnt = static_cast<unsigned int>(m_RefCnt.Add(-1));
    if (ref_cnt == 0) {
        x_ReleaseAllChunks();
    }
    return ref_cnt;
}


inline
CNCMMChunksPool_Getter::CNCMMChunksPool_Getter(void)
{}

inline void
CNCMMChunksPool_Getter::Initialize(void)
{
    TBase::Initialize();
    for (unsigned int i = 0; i < kNCMMMaxThreadsCnt; ++i) {
        sm_Pools[i].InitInstance();
        sm_PoolPtrs[i] = &sm_Pools[i];
    }
    GetObjPtr();
}

CNCMMChunksPool*
CNCMMChunksPool_Getter::CreateTlsObject(void)
{
#ifdef NCBI_OS_MSWIN

    // To avoid loss of memory on Windows we always use one pool although it's
    // slower.
    return &sm_Pools[0];

#else  // NCBI_OS_MSWIN

    CFastMutexGuard guard(sm_CreateLock);

    CNCMMChunksPool* pool;
    if (sm_CntUsed == kNCMMMaxThreadsCnt) {
        pool = &sm_Pools[g_GetNCThreadIndex() % kNCMMMaxThreadsCnt];
    }
    else {
        pool = sm_PoolPtrs[sm_CntUsed++];
    }
    pool->AddRef();
    return pool;

#endif  // NCBI_OS_MSWIN
}

void
CNCMMChunksPool_Getter::DeleteTlsObject(void* mem_ptr)
{
#ifndef NCBI_OS_MSWIN

    CFastMutexGuard guard(sm_CreateLock);

    CNCMMChunksPool* pool = static_cast<CNCMMChunksPool*>(mem_ptr);
    if (pool->ReleaseRef() == 0) {
        sm_PoolPtrs[--sm_CntUsed] = pool;
    }

#endif
}


inline void
CNCMMChunksPool::Initialize(void)
{
    sm_Getter.Initialize();
}

inline
CNCMMChunksPool::CNCMMChunksPool(void)
{}

inline CNCMMFreeChunk**
CNCMMChunksPool::x_AdvanceChunkPtr(CNCMMFreeChunk** chunk_ptr)
{
    ++chunk_ptr;
    if (chunk_ptr == &m_Chunks[kNCMMCntChunksInPool])
        chunk_ptr = &m_Chunks[0];
    return chunk_ptr;
}

inline void*
CNCMMChunksPool::x_GetChunk(void)
{
    CFastMutexGuard guard(m_ObjLock);

    if (m_GetPtr == m_PutPtr)
        x_RefillPool();
    CNCMMFreeChunk* chunk = *m_GetPtr;
    m_GetPtr = x_AdvanceChunkPtr(m_GetPtr);
    chunk->~CNCMMFreeChunk();
    CNCMMStats::ReservedMemDeleted(kNCMMChunkSize);
    return chunk;
}

inline void
CNCMMChunksPool::x_PutChunk(void* mem_ptr)
{
    CFastMutexGuard guard(m_ObjLock);

    *m_PutPtr = new (mem_ptr) CNCMMFreeChunk();
    CNCMMStats::ReservedMemCreated(kNCMMChunkSize);
    m_PutPtr = x_AdvanceChunkPtr(m_PutPtr);
    if (m_GetPtr == m_PutPtr)
        x_CleanPoolSpace();
}

void
CNCMMChunksPool::x_RefillPool(void)
{
    CNCMMStats::ChunksPoolRefilled();

    const unsigned int cnt_to_ask = kNCMMCntChunksInPool / 2;
    unsigned int cnt_got = 0;
    while (cnt_got == 0) {
        cnt_got = CNCMMReserve::GetChunks(&m_Chunks[0], cnt_to_ask);
        if (cnt_got == 0) {
            new CNCMMSlab();
        }
    }
    m_GetPtr = &m_Chunks[0];
    m_PutPtr = m_GetPtr + cnt_got;

    CNCMMStats::ReservedMemCreated(cnt_got * kNCMMChunkSize);
}

void
CNCMMChunksPool::x_CleanPoolSpace(void)
{
    CNCMMStats::ChunksPoolCleaned();

    const unsigned int half_size = kNCMMCntChunksInPool / 2;
    if (m_PutPtr - m_Chunks >= half_size) {
        m_PutPtr -= half_size;
        CNCMMReserve::DumpChunks(m_PutPtr, half_size);
    }
    else {
        CNCMMReserve::DumpChunks(&m_Chunks[0], half_size);
        m_PutPtr = &m_Chunks[0];
        m_GetPtr = &m_Chunks[half_size];
    }

    CNCMMStats::ReservedMemDeleted(half_size * kNCMMChunkSize);
}

void
CNCMMChunksPool::x_ReleaseAllChunks(void)
{
    CNCMMStats::ChunksPoolCleaned();

    if (m_PutPtr > m_GetPtr) {
        size_t cnt_chunks = m_PutPtr - m_GetPtr;
        CNCMMReserve::DumpChunks(m_GetPtr, cnt_chunks);
        CNCMMStats::ReservedMemDeleted(cnt_chunks * kNCMMChunkSize);
    }
    else if (m_PutPtr < m_GetPtr) {
        size_t cnt_chunks = &m_Chunks[kNCMMCntChunksInPool] - m_GetPtr;
        CNCMMReserve::DumpChunks(m_GetPtr, cnt_chunks);
        if (m_PutPtr > &m_Chunks[0]) {
            size_t cnt_on_bottom = m_PutPtr - &m_Chunks[0];
            CNCMMReserve::DumpChunks(&m_Chunks[0], cnt_on_bottom);
            cnt_chunks += cnt_on_bottom;
        }
        CNCMMStats::ReservedMemDeleted(cnt_chunks * kNCMMChunkSize);
    }
    m_GetPtr = m_PutPtr = &m_Chunks[0];
}

inline void*
CNCMMChunksPool::GetChunk(void)
{
    return sm_Getter.GetObjPtr()->x_GetChunk();
}

inline void
CNCMMChunksPool::PutChunk(void* mem_ptr)
{
    sm_Getter.GetObjPtr()->x_PutChunk(mem_ptr);
}


inline CNCMMDBPage*
CNCMMDBPage::FromDataPtr(void* data)
{
    return reinterpret_cast<CNCMMDBPage*>(
        static_cast<char*>(data) - (sizeof(CNCMMDBPage) - kNCMMDBPageDataSize));
}

inline void*
CNCMMDBPage::GetData(void)
{
    return m_Data;
}

inline CNCMMDBCache*
CNCMMDBPage::GetCache(void)
{
    return m_Cache;
}

inline void
CNCMMDBPage::SetCache(CNCMMDBCache* cache)
{
    _ASSERT(m_Cache == NULL  ||  cache == NULL);

    m_Cache = cache;
}

inline bool
CNCMMDBPage::x_IsReallyInLRU(void)
{
    return m_PrevInLRU  ||  sm_LRUHead == this;
}

inline bool
CNCMMDBPage::IsInLRU(void)
{
    return (m_StateFlags & fInLRU) != 0;
}

inline void
CNCMMDBPage::AddToLRU(void)
{
    _ASSERT(!IsInLRU());

    CFastMutexGuard guard(sm_LRULock);

    if ((m_StateFlags & fPeeked) == 0) {
        m_PrevInLRU = sm_LRUTail;
        if (sm_LRUTail) {
            sm_LRUTail->m_NextInLRU = this;
        }
        else {
            _ASSERT(!sm_LRUHead);
            sm_LRUHead = this;
        }
        sm_LRUTail = this;
    }
    m_StateFlags |= fInLRU;
}

inline void
CNCMMDBPage::x_RemoveFromLRUImpl(void)
{
    if (m_PrevInLRU) {
        m_PrevInLRU->m_NextInLRU = m_NextInLRU;
    }
    else {
        _ASSERT(sm_LRUHead == this);
        sm_LRUHead = m_NextInLRU;
    }
    if (m_NextInLRU) {
        m_NextInLRU->m_PrevInLRU = m_PrevInLRU;
    }
    else {
        _ASSERT(sm_LRUTail == this);
        sm_LRUTail = m_PrevInLRU;
    }
    m_PrevInLRU = m_NextInLRU = NULL;
}

inline void
CNCMMDBPage::RemoveFromLRU(void)
{
    CFastMutexGuard guard(sm_LRULock);

    if (x_IsReallyInLRU())
        x_RemoveFromLRUImpl();
    m_StateFlags &= ~fInLRU;
}

inline CNCMMDBPage*
CNCMMDBPage::PeekLRUForDelete(void)
{
    CFastMutexGuard guard(sm_LRULock);

    CNCMMDBPage* page = sm_LRUHead;
    if (page) {
        page->m_StateFlags |= fPeeked;
        page->x_RemoveFromLRUImpl();
    }
    return page;
}

inline bool
CNCMMDBPage::DoPeekedDelete(void)
{
    _ASSERT((m_StateFlags & fPeeked) != 0);

    bool do_delete;
    {{
        CFastMutexGuard guard(sm_LRULock);
        do_delete = IsInLRU();
        _ASSERT(!do_delete  ||  !x_IsReallyInLRU());
        m_StateFlags &= ~fPeeked;
    }}
    if (do_delete) {
        delete this;
    }
    return do_delete;
}

inline
CNCMMDBPage::CNCMMDBPage(void)
    : m_Cache(NULL),
      m_StateFlags(0),
      m_PrevInLRU(NULL),
      m_NextInLRU(NULL)
{
    // Required by SQLite if page is new for the cache instance
    *reinterpret_cast<void**>(m_Data) = NULL;

    CNCMMCentral::DBPageCreated();
}

inline
CNCMMDBPage::~CNCMMDBPage(void)
{
    if (m_Cache) {
        RemoveFromLRU();
        m_Cache->DetachPage(this);
    }
    CNCMMCentral::DBPageDeleted();
}

inline void*
CNCMMDBPage::operator new(size_t _DEBUG_ARG(size))
{
    _ASSERT(size == kNCMMChunkSize);

    ENCMMMode mode = CNCMMCentral::GetMemMode();
    switch (mode) {
    case eNCMemShrinking:
        CNCMMDBCache::DeleteOnePage();
        // fall through
    case eNCMemStable:
        CNCMMDBCache::DeleteOnePage();
        break;
    default:
        break;
    }
    return CNCMMChunksPool::GetChunk();
}

inline void
CNCMMDBPage::operator delete(void* mem_ptr)
{
    CNCMMChunksPool::PutChunk(mem_ptr);
}

inline void
CNCMMDBPage::RequestDelete(void)
{
    RemoveFromLRU();
    // Here m_StateFlags is checked outside sm_LRULock used to access it
    // everywhere else because fPeeked cannot be set during execution of
    // RemoveFromLRU() - it can be set before that or after that. If it was
    // set before that then it will not be reset before next if because
    // resetting is made under cache's mutex and this method works also under
    // cache's mutex. If fPeeked wasn't set before RemoveFromLRU() then it
    // will not be set after it because page should be in LRU list for the
    // flag to be set. So this check is appropriate.
    if ((m_StateFlags & fPeeked) == 0)
        delete this;
}


inline
CNCMMBlocksSetBase::CNCMMBlocksSetBase(void)
{}

inline size_t
CNCMMBlocksSetBase::GetBlockSize(void)
{
    // For CNCMMBlocksSet m_BlockSize stores index of block size, for
    // CNCMMBigBlockSet m_BlockSize stores size itself and it will alwayse be
    // greater than kNCMMCntSmallSizes.
    return (m_BlocksSize < kNCMMCntSmallSizes?
                           kNCMMSmallSize[m_BlocksSize]: m_BlocksSize);
}

inline CNCMMSizePool*
CNCMMBlocksSet::GetPool(void)
{
    return m_Pool;
}

inline void
CNCMMBlocksSet::SetPool(CNCMMSizePool* pool)
{
    m_Pool = pool;
}

inline void**
CNCMMBlocksSet::x_GetFirstFreePtr(void)
{
    return reinterpret_cast<void**>(m_Data);
}

inline unsigned int
CNCMMBlocksSet::CountFreeBlocks(void)
{
    return static_cast<unsigned int>(m_LastFree - x_GetFirstFreePtr());
}

inline
CNCMMBlocksSet::CNCMMBlocksSet(CNCMMSizePool* pool, unsigned int size_index)
{
    m_Pool = pool;
    m_NextInPool = m_PrevInPool = NULL;
    m_BlocksSize = size_index;

    // Initial filling of table of free blocks
    void** free_ptr         = x_GetFirstFreePtr();
    m_LastFree              = free_ptr + kNCMMBlocksPerSet[size_index];
    char* block_ptr         = reinterpret_cast<char*>(m_LastFree);
    unsigned int block_size = kNCMMSmallSize[size_index];
    while (free_ptr < m_LastFree) {
        *free_ptr = block_ptr;
        block_ptr += block_size;
        ++free_ptr;
    }
    _ASSERT(block_ptr <= &m_Data[kNCMMSetDataSize]);

    CNCMMStats::BlocksSetCreated(size_index);
}

inline
CNCMMBlocksSet::~CNCMMBlocksSet(void)
{
    // Check that all blocks in the set are freed
    _ASSERT(CountFreeBlocks() == kNCMMBlocksPerSet[m_BlocksSize]);

    CNCMMStats::BlocksSetDeleted(static_cast<unsigned int>(m_BlocksSize));
}

inline unsigned int
CNCMMBlocksSet::GetEmptyGrade(void)
{
    return m_EmptyGrade;
}

inline void
CNCMMBlocksSet::x_CalcEmptyGrade(void)
{
    m_EmptyGrade = s_GetGradeValue(CountFreeBlocks(),
                                   kNCMMBlocksPerGrade[m_BlocksSize])
                   + CNCMMCentral::GetSlab(this)->GetEmptyGrade()
                                                        * kNCMMSetEmptyGrades;
    _ASSERT(m_EmptyGrade < kNCMMTotalEmptyGrades);
}

inline void*
CNCMMBlocksSet::GetBlock(void)
{
    _ASSERT(m_LastFree != x_GetFirstFreePtr());
    --m_LastFree;
    x_CalcEmptyGrade();
    return *m_LastFree;
}

inline void
CNCMMBlocksSet::ReleaseBlock(void* block)
{
    *m_LastFree = block;
    ++m_LastFree;
    x_CalcEmptyGrade();
}

inline void*
CNCMMBlocksSet::operator new(size_t _DEBUG_ARG(size))
{
    _ASSERT(size == kNCMMChunkSize);

    return CNCMMChunksPool::GetChunk();
}

inline void
CNCMMBlocksSet::operator delete(void* mem_ptr)
{
    CNCMMChunksPool::PutChunk(mem_ptr);
}


inline
CNCMMSlabBase::CNCMMSlabBase(void)
{}


inline void*
CNCMMSlab::operator new(size_t _DEBUG_ARG(size))
{
    _ASSERT(size <= kNCMMSlabSize);
    return CNCMMCentral::SysAllocAligned(kNCMMSlabSize);
}

inline void
CNCMMSlab::operator delete(void* mem_ptr)
{
    CNCMMCentral::SysFree(mem_ptr, kNCMMSlabSize);
}

inline unsigned int
CNCMMSlab::x_GetChunkIndex(void* mem_ptr)
{
    return (static_cast<char*>(mem_ptr) - reinterpret_cast<char*>(m_Chunks))
           / kNCMMChunkSize;
}

inline CNCMMBlocksSetBase*
CNCMMSlab::GetBlocksSet(void* mem_ptr)
{
    return reinterpret_cast<CNCMMBlocksSetBase*>(
                                         &m_Chunks[x_GetChunkIndex(mem_ptr)]);
}

inline unsigned int
CNCMMSlab::x_CalcEmptyGrade(void)
{
    m_EmptyGrade = s_GetGradeValue(m_CntFree, kNCMMChunksPerSlabGrade);
    _ASSERT(m_EmptyGrade < kNCMMSlabEmptyGrades);
    return m_EmptyGrade;
}

inline unsigned int
CNCMMSlab::MarkChainOccupied(const SNCMMChainInfo& chain)
{
    CFastMutexGuard guard(m_ObjLock);

    _ASSERT(m_CntFree >= chain.size);
    m_CntFree -= chain.size;
    x_CalcEmptyGrade();
    m_FreeMask.InvertBits(x_GetChunkIndex(chain.start), chain.size);
    return m_EmptyGrade;
}

inline void
CNCMMSlab::MarkChainFree(SNCMMChainInfo* chain,
                         SNCMMChainInfo* chain_left,
                         SNCMMChainInfo* chain_right)
{
    CFastMutexGuard guard(m_ObjLock);

    _ASSERT(m_CntFree + chain->size <= kNCMMCntChunksInSlab);
    m_CntFree += chain->size;
    x_CalcEmptyGrade();
    chain->slab_grade = m_EmptyGrade;
    chain->start->MarkChain(chain->size, chain->slab_grade);

    unsigned int chain_index = x_GetChunkIndex(chain->start);
    m_FreeMask.InvertBits(chain_index, chain->size);

    if (chain_index != 0  &&  m_FreeMask.IsBitSet(chain_index - 1)) {
        chain_left->AssignFromChain(&m_Chunks[chain_index - 1]);
    }
    unsigned int next_index = chain_index + chain->size;
    if (next_index < kNCMMCntChunksInSlab
        &&  m_FreeMask.IsBitSet(next_index))
    {
        chain_right->AssignFromChain(&m_Chunks[next_index]);
    }
}

inline
CNCMMSlab::CNCMMSlab(void)
{
    m_CntFree    = kNCMMCntChunksInSlab;
    m_EmptyGrade = kNCMMSlabEmptyGrades - 1;
    m_FreeMask.Initialize(1);
    CNCMMReserve::IntroduceChain(&m_Chunks[0], kNCMMCntChunksInSlab);
    CNCMMStats::SlabCreated();
}

inline
CNCMMSlab::~CNCMMSlab(void)
{
    CNCMMStats::SlabDeleted();
}


inline void
CNCMMChainsPool::InitInstance(void)
{
    m_ObjLock.InitializeDynamic();
    memset(m_FreeChains, 0, sizeof(m_FreeChains));
    m_RefCnt.Set(0);
}

inline void
CNCMMChainsPool::AddRef(void)
{
    m_RefCnt.Add(1);
}

inline unsigned int
CNCMMChainsPool::ReleaseRef(void)
{
    unsigned int ref_cnt = static_cast<unsigned int>(m_RefCnt.Add(-1));
    if (ref_cnt == 0) {
        x_ReleaseAllChains();
    }
    return ref_cnt;
}


inline
CNCMMChainsPool_Getter::CNCMMChainsPool_Getter(void)
{}

inline void
CNCMMChainsPool_Getter::Initialize(void)
{
    TBase::Initialize();
    for (unsigned int i = 0; i < kNCMMMaxThreadsCnt; ++i) {
        sm_Pools[i].InitInstance();
        sm_PoolPtrs[i] = &sm_Pools[i];
    }
    GetObjPtr();
}

CNCMMChainsPool*
CNCMMChainsPool_Getter::CreateTlsObject(void)
{
#ifdef NCBI_OS_MSWIN

    // To avoid loss of memory on Windows we always use one pool although it's
    // slower.
    return &sm_Pools[0];

#else // NCBI_OS_MSWIN

    CFastMutexGuard guard(sm_CreateLock);

    if (sm_CntUsed == kNCMMMaxThreadsCnt) {
    }

    CNCMMChainsPool* pool;
    if (sm_CntUsed == kNCMMMaxThreadsCnt) {
        pool = &sm_Pools[g_GetNCThreadIndex() % kNCMMMaxThreadsCnt];
    }
    else {
        pool = sm_PoolPtrs[sm_CntUsed++];
    }
    pool->AddRef();
    return pool;

#endif  // NCBI_OS_MSWIN
}

void
CNCMMChainsPool_Getter::DeleteTlsObject(void* mem_ptr)
{
#ifndef NCBI_OS_MSWIN

    CFastMutexGuard guard(sm_CreateLock);

    CNCMMChainsPool* pool = static_cast<CNCMMChainsPool*>(mem_ptr);
    if (pool->ReleaseRef() == 0) {
        sm_PoolPtrs[--sm_CntUsed] = pool;
    }

#endif  // NCBI_OS_MSWIN
}


inline void
CNCMMChainsPool::Initialize(void)
{
    sm_Getter.Initialize();
}

inline
CNCMMChainsPool::CNCMMChainsPool(void)
{}

inline void*
CNCMMChainsPool::x_GetChain(unsigned int chain_size)
{
    void* chain = NULL;
    {{
        CFastMutexGuard guard(m_ObjLock);
        if (m_FreeChains[chain_size]) {
            chain = m_FreeChains[chain_size];
            m_FreeChains[chain_size] = NULL;
        }
    }}
    if (chain) {
        CNCMMStats::ReservedMemDeleted(chain_size * kNCMMChunkSize);
    }
    else {
        chain = CNCMMReserve::GetChain(chain_size);
    }
    return chain;
}

inline void
CNCMMChainsPool::x_PutChain(void* chain, unsigned int chain_size)
{
    void* to_release;
    {{
        CFastMutexGuard guard(m_ObjLock);

        to_release = m_FreeChains[chain_size];
        m_FreeChains[chain_size] = chain;
    }}
    if (to_release) {
        CNCMMReserve::DumpChain(to_release, chain_size);
    }
    else {
        CNCMMStats::ReservedMemCreated(chain_size * kNCMMChunkSize);
    }
}

void
CNCMMChainsPool::x_ReleaseAllChains(void)
{
    size_t all_size = 0;
    for (unsigned int i = 1; i <= kNCMMCntChunksInSlab; ++i) {
        if (m_FreeChains[i]) {
            all_size += i;
            CNCMMReserve::DumpChain(m_FreeChains[i], i);
        }
    }
    CNCMMStats::ReservedMemDeleted(all_size * kNCMMChunkSize);
}

inline void*
CNCMMChainsPool::GetChain(unsigned int chain_size)
{
    _ASSERT(chain_size <= kNCMMCntChunksInSlab + 1);
    if (chain_size > kNCMMCntChunksInSlab)
        chain_size = kNCMMCntChunksInSlab;
    return sm_Getter.GetObjPtr()->x_GetChain(chain_size);
}

inline void
CNCMMChainsPool::PutChain(void* mem_ptr, unsigned int chain_size)
{
    _ASSERT(chain_size <= kNCMMCntChunksInSlab + 1);
    if (chain_size > kNCMMCntChunksInSlab)
        chain_size = kNCMMCntChunksInSlab;
    sm_Getter.GetObjPtr()->x_PutChain(mem_ptr, chain_size);
}


inline
CNCMMReserve::CNCMMReserve(void)
{}

inline void
CNCMMReserve::x_InitInstance(void)
{
    m_ObjLock.InitializeDynamic();
    m_ExistMask.Initialize(0);
    memset(m_Chains, 0, sizeof(m_Chains));
}

inline void
CNCMMReserve::Initialize(void)
{
    for (unsigned int i = 0; i < kNCMMSlabEmptyGrades; ++i) {
        sm_Instances[i].x_InitInstance();
    }
}

inline void
CNCMMReserve::x_MarkListIfEmpty(unsigned int list_index)
{
    if (!m_Chains[list_index]) {
        m_ExistMask.InvertBits(list_index, 1);
    }
}

inline void
CNCMMReserve::Link(const SNCMMChainInfo& chain)
{
    CFastMutexGuard guard(m_ObjLock);

    x_MarkListIfEmpty(chain.size);
    CNCMMFreeChunk*& list_head = m_Chains[chain.size];

    chain.start->m_NextChain = list_head;
    if (list_head)
        list_head->m_PrevChain = chain.start;
    list_head = chain.start;
}

inline void
CNCMMReserve::x_Unlink(const SNCMMChainInfo& chain)
{
    CNCMMFreeChunk*& list_head = m_Chains[chain.size];
    _ASSERT(chain.start->m_PrevChain  ||  list_head == chain.start);

    if (chain.start->m_PrevChain) {
        chain.start->m_PrevChain->m_NextChain = chain.start->m_NextChain;
    }
    else {
        list_head = chain.start->m_NextChain;
        x_MarkListIfEmpty(chain.size);
    }
    if (chain.start->m_NextChain)
        chain.start->m_NextChain->m_PrevChain = chain.start->m_PrevChain;
    chain.start->m_NextChain = chain.start->m_PrevChain = NULL;
}

inline bool
CNCMMReserve::UnlinkIfExist(const SNCMMChainInfo& chain)
{
    CFastMutexGuard guard(m_ObjLock);

    CNCMMFreeChunk* chain_in_pool = m_Chains[chain.size];
    while (chain_in_pool  &&  chain_in_pool != chain.start) {
        chain_in_pool = chain_in_pool->m_NextChain;
    }
    if (chain_in_pool) {
        x_Unlink(chain);
        return true;
    }
    return false;
}

inline bool
CNCMMReserve::x_FindFreeChain(unsigned int    min_size,
                              SNCMMChainInfo* chain)
{
    unsigned int chain_size = min_size;
    if (!m_Chains[chain_size]) {
        int new_size = m_ExistMask.GetClosestSet(chain_size);
        if (new_size == -1) {
            return false;
        }
        chain_size = static_cast<unsigned int>(new_size);
    }
    _ASSERT(m_Chains[chain_size]);
    chain->start = m_Chains[chain_size];
    chain->size = chain_size;
    return true;
}

inline bool
CNCMMReserve::x_GetChain(unsigned int min_size, SNCMMChainInfo* chain)
{
    CFastMutexGuard guard(m_ObjLock);

    if (!x_FindFreeChain(min_size, chain))
        return false;
    x_Unlink(*chain);
    return true;
}

inline void
CNCMMReserve::x_MarkChainFree(SNCMMChainInfo* chain,
                              SNCMMChainInfo* chain_left,
                              SNCMMChainInfo* chain_right)
{
    CNCMMSlab* chain_slab = CNCMMCentral::GetSlab(chain->start);
    chain_slab->MarkChainFree(chain, chain_left, chain_right);
}

inline void
CNCMMReserve::x_CreateNewChain(const SNCMMChainInfo& chain)
{
    chain.start->MarkChain(chain.size, chain.slab_grade);
    sm_Instances[chain.slab_grade].Link(chain);
}

inline void
CNCMMReserve::x_CreateMergedChain(const SNCMMChainInfo& chain)
{
    if (chain.size == kNCMMCntChunksInSlab) {
        delete CNCMMCentral::GetSlab(chain.start);
    }
    else {
        x_CreateNewChain(chain);
    }
}

inline void
CNCMMReserve::x_OccupyChain(const SNCMMChainInfo& chain,
                            unsigned int          occupy_size)
{
    _ASSERT(chain.size >= occupy_size);
    SNCMMChainInfo occupied(chain);
    occupied.size = occupy_size;
    CNCMMSlab*   chain_slab = CNCMMCentral::GetSlab(chain.start);
    unsigned int slab_grade = chain_slab->MarkChainOccupied(occupied);
    if (chain.size != occupy_size) {
        SNCMMChainInfo next_chain;
        next_chain.start      = chain.start + occupy_size;
        next_chain.size       = chain.size  - occupy_size;
        next_chain.slab_grade = slab_grade;
        x_CreateNewChain(next_chain);
    }
}

inline unsigned int
CNCMMReserve::FillChunksFromChains(CNCMMFreeChunk** chunk_ptrs,
                                   unsigned int     max_cnt,
                                   SNCMMChainInfo*  chain_ptrs)
{
    CFastMutexGuard guard(m_ObjLock);

    SNCMMChainInfo cur_chain;
    if (!x_FindFreeChain(1, &cur_chain))
        return 0;
    unsigned int cnt_chains = 0, cnt_chunks = 0;
    while (max_cnt > 0  &&  cur_chain.start != NULL) {
        CNCMMFreeChunk* next_start = cur_chain.start->m_NextChain;
        SNCMMChainInfo& saved_chain = chain_ptrs[cnt_chains++];
        saved_chain.start = cur_chain.start;
        saved_chain.size  = cur_chain.size;
        CNCMMFreeChunk* chunk = cur_chain.start;
        unsigned int cnt_in_chain = 0;
        while (cnt_in_chain < cur_chain.size  &&  max_cnt > 0) {
            *chunk_ptrs = chunk;
            ++cnt_chunks;
            ++chunk_ptrs;
            --max_cnt;
            ++chunk;
            ++cnt_in_chain;
        }
        cur_chain.start = next_start;
    }
    if (cur_chain.start) {
        cur_chain.start->m_PrevChain = NULL;
    }
    m_Chains[cur_chain.size] = cur_chain.start;
    x_MarkListIfEmpty(cur_chain.size);
    return cnt_chunks;
}

void*
CNCMMReserve::GetChain(unsigned int chain_size)
{
    for (;;) {
        for (unsigned int i = 1; i < kNCMMSlabEmptyGrades; ++i) {
            SNCMMChainInfo chain;
            chain.Initialize();
            if (sm_Instances[i].x_GetChain(chain_size, &chain)) {
                x_OccupyChain(chain, chain_size);
                chain.start->DestroyChain(chain_size);
                return chain.start;
            }
        }
        new CNCMMSlab();
    }
}

inline void
CNCMMReserve::x_MergeChain(const SNCMMChainInfo& chain_from,
                           SNCMMChainInfo&       chain_to)
{
    chain_to.size += chain_from.size;
    if (chain_from.start < chain_to.start)
        chain_to.start = chain_from.start;
}

inline bool
CNCMMReserve::x_MergeChainIfValid(const SNCMMChainInfo& chain_from,
                                  SNCMMChainInfo&       chain_to)
{
    if (sm_Instances[chain_from.slab_grade].UnlinkIfExist(chain_from)) {
        x_MergeChain(chain_from, chain_to);
        return true;
    }
    return false;
}

void
CNCMMReserve::DumpChain(void* chain_ptr, unsigned int chain_size)
{
    _ASSERT(chain_size <= kNCMMCntChunksInSlab);

    SNCMMChainInfo chain, chain_left, chain_right;
    chain_left .Initialize();
    chain_right.Initialize();
    chain.size  = chain_size;
    chain.start = CNCMMFreeChunk::ConstructChain(chain_ptr, chain_size);
    x_MarkChainFree(&chain, &chain_left, &chain_right);
    if (chain_left.start) {
        x_MergeChainIfValid(chain_left, chain);
    }
    if (chain_right.start) {
        x_MergeChainIfValid(chain_right, chain);
    }
    x_CreateMergedChain(chain);
}

void
CNCMMReserve::IntroduceChain(CNCMMFreeChunk* chain, unsigned int chain_size)
{
    SNCMMChainInfo chain_info;
    chain_info.start = chain;
    chain_info.size  = chain_size;
    chain_info.slab_grade = kNCMMSlabEmptyGrades - 1;
    x_CreateNewChain(chain_info);
}

unsigned int
CNCMMReserve::GetChunks(CNCMMFreeChunk** chunk_ptrs, unsigned int max_cnt)
{
    _ASSERT(max_cnt <= kNCMMCntChunksInPool);
    SNCMMChainInfo got_chains[kNCMMCntChunksInPool];
    memset(got_chains, 0, sizeof(got_chains));

    unsigned int cnt_got = 0;
    for (unsigned int i = 1; cnt_got == 0  &&  i < kNCMMSlabEmptyGrades; ++i)
    {
        cnt_got = sm_Instances[i].FillChunksFromChains(
                                             chunk_ptrs, max_cnt, got_chains);
    }
    unsigned int num = 0;
    unsigned int cnt_left = cnt_got;
    while (cnt_left != 0) {
        _ASSERT(got_chains[num].start != NULL);
        unsigned int occupy_size = min(got_chains[num].size, cnt_left);
        x_OccupyChain(got_chains[num], occupy_size);
        cnt_left -= occupy_size;
        ++num;
    }
    return cnt_got;
}

inline void
CNCMMReserve::x_SortChunkPtrs(CNCMMFreeChunk** chunk_ptrs, unsigned int cnt)
{
    for (unsigned int i = 0; i < cnt; ++i) {
        for (unsigned int j = i + 1; j < cnt; ++j) {
            if (chunk_ptrs[i] > chunk_ptrs[j])
                swap(chunk_ptrs[i], chunk_ptrs[j]);
        }
    }
}

void
CNCMMReserve::DumpChunks(CNCMMFreeChunk** chunk_ptrs, unsigned int cnt)
{
    _ASSERT(cnt > 0  &&  cnt <= kNCMMCntChunksInPool);
    SNCMMChainInfo chains[kNCMMCntChunksInPool];
    SNCMMChainInfo lefts [kNCMMCntChunksInPool];
    SNCMMChainInfo rights[kNCMMCntChunksInPool];
    bool           merged[kNCMMCntChunksInPool];

    x_SortChunkPtrs(chunk_ptrs, cnt);
    memset(merged, 0, sizeof(merged));

    for (unsigned int i = 0; i < cnt; ++i) {
        chains[i].start = chunk_ptrs[i];
        chains[i].size  = 1;
        lefts [i].start = rights[i].start = NULL;
        x_MarkChainFree(&chains[i], &lefts[i], &rights[i]);
    }
    for (unsigned int i = cnt - 1; i >= 1; --i) {
        if (rights[i].start)
            x_MergeChainIfValid(rights[i], chains[i]);
        if (lefts[i].start) {
            if (rights[i - 1].start == lefts[i].start) {
                chains[i - 1].slab_grade = chains[i].slab_grade;
                if (x_MergeChainIfValid(rights[i - 1], chains[i])) {
                    merged[i] = true;
                    x_MergeChain(chains[i], chains[i - 1]);
                }
                rights[i - 1].start = NULL;
            }
            else if (chains[i - 1].start == lefts[i].start) {
                _ASSERT(rights[i - 1].start == NULL);
                x_MergeChain(chains[i], chains[i - 1]);
                chains[i - 1].slab_grade = chains[i].slab_grade;
                merged[i] = true;
            }
            else {
                x_MergeChainIfValid(lefts[i], chains[i]);
            }
        }
    }
    if (rights[0].start)
        x_MergeChainIfValid(rights[0], chains[0]);
    if (lefts[0].start)
        x_MergeChainIfValid(lefts[0], chains[0]);

    for (unsigned int i = 0; i < cnt; ++i) {
        if (!merged[i]) {
            x_CreateMergedChain(chains[i]);
        }
        else {
            _ASSERT(i > 0  &&  chains[i - 1].start + chains[i - 1].size
                                         >= chains[i].start + chains[i].size);
        }
    }
}


inline
CNCMMBigBlockSet::CNCMMBigBlockSet(size_t blocks_size)
{
    m_NextInPool = m_PrevInPool = NULL;
    m_BlocksSize = blocks_size;
    m_LastFree   = NULL;
}

inline
CNCMMBigBlockSet::~CNCMMBigBlockSet(void)
{
    // All data should be preserved to be used in operator delete(void*).
}

inline unsigned int
CNCMMBigBlockSet::x_GetChunksCnt(size_t combined_size)
{
    size_t full_size = combined_size + kNCMMSetBaseSize;
    return static_cast<unsigned int>(
                           (full_size + kNCMMChunkSize - 1) / kNCMMChunkSize);
}

inline void*
CNCMMBigBlockSet::GetData(void)
{
    return this + 1;
}

inline void*
CNCMMBigBlockSet::operator new(size_t, size_t combined_size)
{
    _ASSERT(combined_size <= kNCMMMaxCombinedSize);

    unsigned int chain_size = x_GetChunksCnt(combined_size);
    CNCMMStats::ChunksChainAlloced(combined_size, chain_size);
    return CNCMMChainsPool::GetChain(chain_size);
}

inline void
CNCMMBigBlockSet::operator delete(void* mem_ptr, size_t combined_size)
{
    unsigned int chain_size = x_GetChunksCnt(combined_size);
    CNCMMStats::ChunksChainFreed(combined_size, chain_size);
    CNCMMChainsPool::PutChain(mem_ptr, chain_size);
}

inline void
CNCMMBigBlockSet::operator delete(void* mem_ptr)
{
    CNCMMBigBlockSet* bl_set = static_cast<CNCMMBigBlockSet*>(mem_ptr);
    operator delete(mem_ptr, bl_set->m_BlocksSize);
}


inline
CNCMMBigBlockSlab::CNCMMBigBlockSlab(size_t big_block_size)
    : m_BlockSet(big_block_size)
{
    m_CntFree    = 0;
    m_EmptyGrade = 0;
    m_FreeMask.Initialize(0);
    CNCMMStats::BigBlockAlloced(big_block_size);
}

inline
CNCMMBigBlockSlab::~CNCMMBigBlockSlab(void)
{
    CNCMMStats::BigBlockFreed(m_BlockSet.GetBlockSize());
    // All data should be preserved for use in operator delete().
}

inline void*
CNCMMBigBlockSlab::GetData(void)
{
    return m_BlockSet.GetData();
}

inline void*
CNCMMBigBlockSlab::operator new(size_t, size_t big_block_size)
{
    size_t full_size = big_block_size + sizeof(CNCMMBigBlockSlab);
    return CNCMMCentral::SysAllocAligned(full_size);
}

inline void
CNCMMBigBlockSlab::operator delete(void* mem_ptr, size_t big_block_size)
{
    size_t full_size = big_block_size + sizeof(CNCMMBigBlockSlab);
    CNCMMCentral::SysFree(mem_ptr, full_size);
}

inline void
CNCMMBigBlockSlab::operator delete(void* mem_ptr)
{
    CNCMMBigBlockSlab* slab = static_cast<CNCMMBigBlockSlab*>(mem_ptr);
    operator delete(mem_ptr, slab->m_BlockSet.GetBlockSize());
}


inline CNCMMSizePool*
CNCMMSizePool_Getter::GetMainPool(unsigned int size_index)
{
    _ASSERT(sm_PoolPtrs[1]);
    return &sm_PoolPtrs[1][size_index];
}


inline void
CNCMMSizePool::x_RemoveSetFromList(CNCMMBlocksSet* bl_set,
                                   unsigned int    list_grade)
{
    CNCMMBlocksSet*& list_head = m_Sets[list_grade];
    if (!bl_set->m_PrevInPool  &&  list_head != bl_set)
        return;

    if (bl_set->m_PrevInPool) {
        bl_set->m_PrevInPool->m_NextInPool = bl_set->m_NextInPool;
    }
    else {
        list_head = bl_set->m_NextInPool;
        if (!list_head)
            m_ExistMask.InvertBits(list_grade, 1);
    }
    if (bl_set->m_NextInPool) {
        bl_set->m_NextInPool->m_PrevInPool = bl_set->m_PrevInPool;
    }
    bl_set->m_PrevInPool = bl_set->m_NextInPool = NULL;
}

inline void
CNCMMSizePool::x_AddSetToList(CNCMMBlocksSet* bl_set,
                              unsigned int    list_grade)
{
    CNCMMBlocksSet*& list_head = m_Sets[list_grade];
    _ASSERT(!bl_set->m_PrevInPool  &&  list_head != bl_set);

    bl_set->m_NextInPool = list_head;
    if (list_head) {
        list_head->m_PrevInPool = bl_set;
    }
    else {
        m_ExistMask.InvertBits(list_grade, 1);
    }
    list_head = bl_set;
}

inline void
CNCMMSizePool::x_AddBlocksSet(CNCMMBlocksSet* bl_set, unsigned int grade)
{
    bl_set->SetPool(this);
    x_AddSetToList(bl_set, grade);
}

inline
CNCMMSizePool::CNCMMSizePool(unsigned int size_index)
    : m_SizeIndex(size_index),
      m_EmptySet(NULL)
{
    m_ObjLock.InitializeDynamic();
    memset(m_Sets, 0, sizeof(m_Sets));
    m_ExistMask.Initialize(0);
}

inline
CNCMMSizePool::~CNCMMSizePool(void)
{
    CFastWriteGuard guard(*sm_DestructLock);

    CNCMMSizePool* main_pool = CNCMMSizePool_Getter::GetMainPool(m_SizeIndex);
    _ASSERT(main_pool != this);
    for (unsigned int grade = 0; grade < kNCMMTotalEmptyGrades; ++grade) {
        CNCMMBlocksSet* bl_set;
        while ((bl_set = m_Sets[grade]) != NULL) {
            x_RemoveSetFromList(bl_set, grade);
            main_pool->x_AddBlocksSet(bl_set, grade);
        }
    }
    if (m_EmptySet) {
        delete m_EmptySet;
    }
}

inline void*
CNCMMSizePool::operator new(size_t, void* ptr)
{
    return ptr;
}

inline void
CNCMMSizePool::operator delete(void*, void*)
{}


inline
CNCMMSizePool_Getter::CNCMMSizePool_Getter(void)
{}

inline void
CNCMMSizePool_Getter::Initialize(void)
{
    TBase::Initialize();
    memset(sm_PoolPtrs, 0, sizeof(sm_PoolPtrs));
    memset(sm_RefCnts,  0, sizeof(sm_RefCnts));
    GetObjPtr();
}

CNCMMSizePool*
CNCMMSizePool_Getter::CreateTlsObject(void)
{
    CFastMutexGuard guard(sm_CreateLock);

    unsigned int index;
#ifdef NCBI_OS_MSWIN
    index = 1;
#else
    index = g_GetNCThreadIndex() % kNCMMMaxThreadsCnt;
#endif

    CNCMMSizePool*& array_ptr = sm_PoolPtrs[index];
    if (!array_ptr) {
        // We have to save thread index along with pointer to array because
        // in DeleteTlsObject() thread index will not be available.
        // Constant 8 is here to ensure proper alignment.
        void* mem_ptr = CNCMMCentral::SysAlloc(kNCMMSizePoolArraySize + 8);
        unsigned int* index_ptr = static_cast<unsigned int*>(mem_ptr);
        *index_ptr = index;
        void* tls_obj_ptr  = static_cast<char*>(mem_ptr) + 8;
        array_ptr = static_cast<CNCMMSizePool*>(tls_obj_ptr);
        for (unsigned int i = 0; i < kNCMMCntSmallSizes; ++i) {
            new (&array_ptr[i]) CNCMMSizePool(i);
        }
        CNCMMStats::OverheadMemAlloced(kNCMMSizePoolArraySize);
    }
    ++sm_RefCnts[index];
    return array_ptr;
}

void
CNCMMSizePool_Getter::DeleteTlsObject(void* obj_ptr)
{
#ifndef NCBI_OS_MSWIN

    CFastMutexGuard guard(sm_CreateLock);

    CNCMMSizePool* pools_array = static_cast<CNCMMSizePool*>(obj_ptr);
    void* mem_ptr = static_cast<char*>(obj_ptr) - 8;
    unsigned int* index_ptr = static_cast<unsigned int*>(mem_ptr);
    unsigned int index = *index_ptr;
    _ASSERT(sm_PoolPtrs[index] == pools_array);
    if (--sm_RefCnts[index] == 0) {
        for (unsigned int i = 0; i < kNCMMCntSmallSizes; ++i) {
            pools_array[i].~CNCMMSizePool();
        }
        CNCMMCentral::SysFree(mem_ptr, kNCMMSizePoolArraySize + 8);
        CNCMMStats::OverheadMemFreed(kNCMMSizePoolArraySize);
        sm_PoolPtrs[index] = NULL;
    }

#endif
}


inline void
CNCMMSizePool::x_CheckGradeChange(CNCMMBlocksSet* bl_set,
                                  unsigned int    old_grade)
{
    unsigned int new_grade = bl_set->GetEmptyGrade();
    if (old_grade != new_grade) {
        x_RemoveSetFromList(bl_set, old_grade);
        x_AddSetToList(bl_set, new_grade);
    }
}

inline void*
CNCMMSizePool::x_AllocateBlock(void)
{
    CFastMutexGuard guard(m_ObjLock);

    CNCMMStats::MemBlockAlloced(m_SizeIndex);

    CNCMMBlocksSet* bl_set;
    int grade = m_ExistMask.GetClosestSet(1);
    if (grade == -1) {
        if (m_EmptySet) {
            bl_set = m_EmptySet;
            m_EmptySet = NULL;
            CNCMMStats::ReservedMemDeleted(kNCMMChunkSize);
        }
        else {
            // Release mutex to allow deallocators to not wait for global
            // allocation
            guard.Release();
            bl_set = new CNCMMBlocksSet(this, m_SizeIndex);
            guard.Guard(m_ObjLock);
        }
        grade = kNCMMTotalEmptyGrades;
    }
    else {
        bl_set = m_Sets[grade];
    }
    void* block = bl_set->GetBlock();
    if (bl_set->CountFreeBlocks() == 0) {
        x_RemoveSetFromList(bl_set, grade);
        x_AddSetToList(bl_set, 0);
    }
    else {
        x_CheckGradeChange(bl_set, grade);
    }
    return block;
}

inline void
CNCMMSizePool::x_DeallocateBlock(void* block, CNCMMBlocksSet* bl_set)
{
    _ASSERT(bl_set->GetBlockSize() == kNCMMSmallSize[m_SizeIndex]);

    CFastMutexGuard guard(m_ObjLock);

    CNCMMStats::MemBlockFreed(m_SizeIndex);
    int old_grade = bl_set->GetEmptyGrade();
    if (bl_set->CountFreeBlocks() == 0)
        old_grade = 0;
    bl_set->ReleaseBlock(block);
    if (bl_set->CountFreeBlocks() == kNCMMBlocksPerSet[m_SizeIndex]) {
        if (m_EmptySet) {
            delete m_EmptySet;
        }
        else {
            CNCMMStats::ReservedMemCreated(kNCMMChunkSize);
        }
        x_RemoveSetFromList(bl_set, old_grade);
        m_EmptySet = bl_set;
    }
    else {
        x_CheckGradeChange(bl_set, old_grade);
    }
}

inline void
CNCMMSizePool::Initialize(void)
{
    static CFastRWLock destruct_lock;

    sm_Getter.Initialize();
    sm_DestructLock = &destruct_lock;
}

inline void*
CNCMMSizePool::AllocateBlock(size_t size)
{
    _ASSERT(size <= kNCMMMaxSmallSize);
    unsigned int index = kNCMMSmallSizeIndex[(size + 7) / 8];
    return sm_Getter.GetObjPtr()[index].x_AllocateBlock();
}

inline void
CNCMMSizePool::DeallocateBlock(void* mem_ptr, CNCMMBlocksSet* bl_set)
{
    CFastReadGuard destr_guard(*sm_DestructLock);
    bl_set->GetPool()->x_DeallocateBlock(mem_ptr, bl_set);
}


inline
CNCMMBGThread::CNCMMBGThread(void)
    : m_WaitForStop(0, 1)
{}

CNCMMBGThread::~CNCMMBGThread(void)
{}

inline void
CNCMMBGThread::Stop(void)
{
    m_WaitForStop.Post();
}


inline size_t
CNCMMCentral::GetMemorySize(void* mem_ptr)
{
    if (!mem_ptr) return 0;
    return GetSlab(mem_ptr)->GetBlocksSet(mem_ptr)->GetBlockSize();
}

void*
CNCMMCentral::AllocMemory(size_t size)
{
    if (!sm_Initialized)
        x_Initialize();

    void* mem_ptr;
    if (size <= kNCMMMaxSmallSize) {
        mem_ptr = CNCMMSizePool::AllocateBlock(size);
    }
    else if (size <= kNCMMMaxCombinedSize) {
        CNCMMBigBlockSet* bl_set = new (size) CNCMMBigBlockSet(size);
        mem_ptr = bl_set->GetData();
    }
    else {
        CNCMMBigBlockSlab* slab = new (size) CNCMMBigBlockSlab(size);
        mem_ptr = slab->GetData();
    }

    return mem_ptr;
}

void
CNCMMCentral::DeallocMemory(void* mem_ptr)
{
    if (!mem_ptr)
        return;

    CNCMMSlab* slab            = GetSlab(mem_ptr);
    CNCMMBlocksSetBase* bl_set = slab->GetBlocksSet(mem_ptr);
    size_t block_size          = bl_set->GetBlockSize();
    if (block_size <= kNCMMMaxSmallSize) {
        CNCMMSizePool::DeallocateBlock(mem_ptr,
                                       static_cast<CNCMMBlocksSet*>(bl_set));
    }
    else if (block_size <= kNCMMMaxCombinedSize) {
        delete static_cast<CNCMMBigBlockSet*>(bl_set);
    }
    else {
        delete reinterpret_cast<CNCMMBigBlockSlab*>(slab);
    }
}

inline void*
CNCMMCentral::ReallocMemory(void* mem_ptr, size_t new_size)
{
    void* new_ptr = AllocMemory(new_size);
    if (new_ptr  &&  mem_ptr) {
        size_t old_size = GetMemorySize(mem_ptr);
        memcpy(new_ptr, mem_ptr, min(old_size, new_size));
        DeallocMemory(mem_ptr);
    }
    return new_ptr;
}

void
CNCMMCentral::x_Initialize(void)
{
    sm_Stats.InitInstance();

    // Initialize all arrays related to info about blocks set for each block
    // size.
    for (unsigned int i = 0; i < kNCMMCntSmallSizes; ++i) {
        size_t block_mem        = kNCMMSmallSize[i] + kNCMMBlockExtraSize;
        unsigned int cnt_blocks = static_cast<unsigned int>(
                                  kNCMMSetDataSize  / block_mem);
        size_t useful_mem       = cnt_blocks        * block_mem;
        size_t extra_mem        = cnt_blocks        * kNCMMBlockExtraSize;
        kNCMMBlocksPerSet[i]    = cnt_blocks;
        kNCMMBlocksPerGrade[i]  = s_GetCntPerGrade(cnt_blocks,
                                                   kNCMMSetEmptyGrades);
        kNCMMLostPerSet[i]      = kNCMMSetDataSize - useful_mem;
        kNCMMOverheadPerSet[i]  = kNCMMSetBaseSize + extra_mem;
    }

    // Initialize lookup table for indexes of memory manager block size for
    // each possible requested block size.
    unsigned int sz_ind = 0, sz = 0, lookup_ind = 0;
    for (; sz <= kNCMMMaxSmallSize; sz += 8, ++lookup_ind) {
        if (sz > kNCMMSmallSize[sz_ind])
            ++sz_ind;
        kNCMMSmallSizeIndex[lookup_ind] = sz_ind;
    }

    // Initialize all other subsystems
    g_InitNCThreadIndexes();
    CNCMMStats     ::Initialize();
    CNCMMChunksPool::Initialize();
    CNCMMChainsPool::Initialize();
    CNCMMReserve   ::Initialize();
    CNCMMSizePool  ::Initialize();

    sm_Initialized = true;
}

inline void
CNCMMCentral::RunLateInit(void)
{
    sm_BGThread = new CNCMMBGThread();
    sm_BGThread->Run(CThread::fRunDetached);
}

inline void
CNCMMCentral::PrepareToStop(void)
{
    sm_Mode = eNCFinalized;
    sm_BGThread->Stop();
    sm_BGThread = NULL;
}

inline void
CNCMMCentral::DoBackgroundWork(void)
{
    size_t free_mem, db_cache_mem;
    CNCMMStats::GetUsageNumbers(&free_mem, &db_cache_mem);

    size_t sys_mem      = sm_Stats.GetSystemMem();
    size_t used_mem     = sys_mem - free_mem;
    size_t db_mem_limit = sm_MemLimit / 2;
    if (sys_mem < sm_MemLimit) {
        sm_Mode = eNCMemStable;
    }
    else if (used_mem < sm_MemLimit) {
        size_t alloc_cnt = (sm_MemLimit - used_mem) / kNCMMChunkSize + 1;
        sm_CntCanAlloc.Set(CAtomicCounter::TValue(alloc_cnt));
        sm_Mode = eNCMemGrowing;
    }
    else if (db_cache_mem < db_mem_limit) {
        size_t alloc_cnt = (db_mem_limit - db_cache_mem) / kNCMMChunkSize + 1;
        sm_CntCanAlloc.Set(CAtomicCounter::TValue(alloc_cnt));
        sm_Mode = eNCMemGrowing;
    }
    else {
        size_t cnt_for_db = (db_cache_mem - db_mem_limit) / kNCMMChunkSize - 1;
        size_t cnt_total  = (used_mem - sm_MemLimit) / kNCMMChunkSize - 1;
        cnt_total = min(cnt_total, cnt_for_db);
        if (cnt_total <= kNCMMLimitToleranceChunks) {
            sm_Mode = eNCMemStable;
        }
        else {
            sm_CntCanAlloc.Set(CAtomicCounter::TValue(-int(cnt_total)));
            sm_Mode = eNCMemShrinking;
        }
    }
    //printf("limit=%llu, used=%llu, free=%llu\n", sm_MemLimit, used_mem, free_mem);
}


void*
CNCMMBGThread::Main(void)
{
    while (CNCMMCentral::GetMemMode() != eNCFinalized) {
        CNCMMCentral::DoBackgroundWork();
        m_WaitForStop.TryWait(kNCMMBGThreadWaitSecs);
    }
    return NULL;
}



inline CNCMMDBPage**
CNCMMDBPagesHash::x_AllocHash(int size)
{
    size_t mem_size = size * sizeof(CNCMMDBPage*);
    CNCMMDBPage** hash = reinterpret_cast<CNCMMDBPage**>(
                                         CNCMMCentral::AllocMemory(mem_size));
    //? CNCMMStats::OverheadMemAlloced(mem_size);

    memset(hash, 0, mem_size);
    return hash;
}

inline void
CNCMMDBPagesHash::x_FreeHash(CNCMMDBPage** hash, int size)
{
    if (size != 0) {
        CNCMMCentral::DeallocMemory(hash);
        //? size_t mem_size = size * sizeof(CNCMMDBPage*);
        //? CNCMMStats::OverheadMemFreed(mem_size);
    }
}

inline
CNCMMDBPagesHash::CNCMMDBPagesHash(void)
    : m_Hash(NULL),
      m_Size(0)
{}

inline
CNCMMDBPagesHash::~CNCMMDBPagesHash(void)
{
    x_FreeHash(m_Hash, m_Size);
}

inline void
CNCMMDBPagesHash::SetOptimalSize(int new_size)
{
    new_size /= kNCMMDBHashSizeFactor;
    for (;;) {
        int next_num = new_size & (new_size - 1);
        if (next_num == 0)
            break;
        new_size = next_num;
    }
    if (new_size == m_Size)
        return;

    CNCMMDBPage** new_hash = x_AllocHash(new_size);
    for (int i = 0; i < m_Size; ++i) {
        CNCMMDBPage* page = m_Hash[i];
        while (page) {
            CNCMMDBPage* next = page->m_NextInHash;
            CNCMMDBPage*& hash_val = new_hash[page->m_Key & (new_size - 1)];
            page->m_NextInHash = hash_val;
            hash_val = page;
            page = next;
        }
    }
    x_FreeHash(m_Hash, m_Size);
    m_Hash = new_hash;
    m_Size = new_size;
}

inline void
CNCMMDBPagesHash::PutPage(unsigned int key, CNCMMDBPage* page)
{
    // According to SQLite's implementation of page cache adding to hash when
    // there's other page with the same key exist should not ever happen.
    // So we will keep the same assumption and will not check its
    // truthfulness.
    CNCMMDBPage*& hash_val = m_Hash[key & (m_Size - 1)];
    page->m_NextInHash = hash_val;
    page->m_Key        = key;
    hash_val           = page;
}

inline CNCMMDBPage*
CNCMMDBPagesHash::GetPage(unsigned int key)
{
    unsigned int index = key & (m_Size - 1);
    CNCMMDBPage* page = m_Hash[index];
    while (page  &&  page->m_Key != key) {
        _ASSERT((page->m_Key & (m_Size - 1)) == index);
        page = page->m_NextInHash;
    }
    return page;
}

inline bool
CNCMMDBPagesHash::RemovePage(CNCMMDBPage* page)
{
    unsigned int index = page->m_Key & (m_Size - 1);
    CNCMMDBPage** hash_val = &m_Hash[index];
    while (*hash_val  &&  *hash_val != page) {
        _ASSERT(((*hash_val)->m_Key & (m_Size - 1)) == index);
        hash_val = &(*hash_val)->m_NextInHash;
    }
    if (*hash_val) {
        *hash_val = page->m_NextInHash;
        return true;
    }
    return false;
}

inline int
CNCMMDBPagesHash::RemoveAllPages(unsigned int min_key)
{
    int cnt_removed = 0;
    for (int i = 0; i < m_Size; ++i) {
        CNCMMDBPage** page_ptr = &m_Hash[i];
        for (CNCMMDBPage* page = *page_ptr; page; page = *page_ptr) {
            _ASSERT(int(page->m_Key & (m_Size - 1)) == i);
            if (page->m_Key >= min_key) {
                *page_ptr = page->m_NextInHash;
                page->m_NextInHash = NULL;
                page->SetCache(NULL);
                page->RequestDelete();
                ++cnt_removed;
            }
            else {
                page_ptr = &page->m_NextInHash;
            }
        }
    }
    return cnt_removed;
}


inline
CNCMMDBCache::CNCMMDBCache(int page_size, bool purgeable)
    : m_Purgable(purgeable),
      m_CacheSize(0),
      m_MaxKey(0)
{
    if (size_t(page_size) > kNCMMDBPageDataSize) {
        CNcbiDiag::DiagTrouble(DIAG_COMPILE_INFO,
                               "Memory chunk size is inappropriate");
    }
    //? CNCMMStats::OverheadMemAlloced(sizeof(*this));
}

inline void
CNCMMDBCache::SetOptimalSize(int num_pages)
{
    CFastMutexGuard guard(m_ObjLock);

    m_Pages.SetOptimalSize(num_pages);
}

inline int
CNCMMDBCache::GetSize(void)
{
    return m_CacheSize;
}

inline void
CNCMMDBCache::x_AttachPage(unsigned int key, CNCMMDBPage* page)
{
    m_Pages.PutPage(key, page);
    page->SetCache(this);
    ++m_CacheSize;
    m_MaxKey = max(m_MaxKey, key);
}

// Not inline because of use in CNCMMDBPage::~CNCMMDBPage().
void
CNCMMDBCache::DetachPage(CNCMMDBPage* page)
{
    _ASSERT(page->GetCache() == this);

    page->SetCache(NULL);
    if (m_Pages.RemovePage(page)) {
        _ASSERT(m_CacheSize > 0);
        if (--m_CacheSize == 0) {
            m_MaxKey = 0;
        }
    }
}

inline void
CNCMMDBCache::DeleteAllPages(unsigned int min_key)
{
    CFastMutexGuard guard(m_ObjLock);

    if (m_MaxKey < min_key  ||  m_CacheSize == 0)
        return;

    int cnt_removed = m_Pages.RemoveAllPages(min_key);
    //printf("DeleteAll removed %d\n", cnt_removed);
    _ASSERT(m_CacheSize >= cnt_removed);
    m_CacheSize -= cnt_removed;
    m_MaxKey = (min_key == 0 || m_CacheSize == 0? 0: min_key - 1);
}

inline void*
CNCMMDBCache::PinPage(unsigned int key, EPageCreate create_flag)
{
    CFastMutexGuard guard(m_ObjLock);

    CNCMMDBPage* page = m_Pages.GetPage(key);
    if (page) {
        if (page->IsInLRU()) {
            CNCMMStats::DBPageHitInCache();
            page->RemoveFromLRU();
        }
        // If page is not in LRU then it's already pinned, so we will not
        // count it as a new page request.
    }
    else {
        // Page is not found, so it's always a new page request
        CNCMMStats::DBPageNotInCache();
        if (create_flag == eDoNotCreate) {
            return NULL;
        }
        guard.Release();
        page = new CNCMMDBPage();
        guard.Guard(m_ObjLock);
        x_AttachPage(key, page);
    }
    return page->GetData();
}

inline void
CNCMMDBCache::UnpinPage(void* data, bool must_delete)
{
    CNCMMDBPage* page = CNCMMDBPage::FromDataPtr(data);
    if (must_delete) {
        CFastMutexGuard guard(m_ObjLock);
        DetachPage(page);
        page->RequestDelete();
    }
    else if (m_Purgable) {
        page->AddToLRU();
    }
}

inline void
CNCMMDBCache::ChangePageKey(void*        data,
                            unsigned int old_key,
                            unsigned int new_key)
{
    CFastMutexGuard guard(m_ObjLock);

    CNCMMDBPage* page = CNCMMDBPage::FromDataPtr(data);
    DetachPage(page);
    x_AttachPage(new_key, page);
}

inline
CNCMMDBCache::~CNCMMDBCache(void)
{
    DeleteAllPages(0);
    _ASSERT(m_CacheSize == 0);

    //? CNCMMStats::OverheadMemFreed(sizeof(*this));
}

inline void*
CNCMMDBCache::operator new(size_t size)
{
    return ::operator new(size);
}

inline void
CNCMMDBCache::operator delete(void* ptr)
{
    ::operator delete(ptr);
}

bool
CNCMMDBCache::DeleteOnePage(void)
{
    for (;;) {
        CNCMMDBPage* page = CNCMMDBPage::PeekLRUForDelete();
        if (!page)
            return false;
        CNCMMDBCache* cache = page->GetCache();
        if (!cache) {
            delete page;
            return true;
        }
        else {
            CFastMutexGuard guard(cache->m_ObjLock);
            if (page->DoPeekedDelete())
                return true;
        }
        // Page should have been extracted from LRU by somebody.
    }
}



// Functions for exposing database cache to SQLite

/// Initialize database cache
static int
s_SQLITE_PCache_Init(void*)
{
    return SQLITE_OK;
}

/// Deinitialize database cache
static void
s_SQLITE_PCache_Shutdown(void*)
{}

/// Create new cache instance
static sqlite3_pcache *
s_SQLITE_PCache_Create(int szPage, int bPurgeable)
{
    return reinterpret_cast<sqlite3_pcache*>(
                           new CNCMMDBCache(size_t(szPage), bPurgeable != 0));
}

/// Set size of database cache (number of pages)
static void
s_SQLITE_PCache_SetSize(sqlite3_pcache* pcache, int nCachesize)
{
    reinterpret_cast<CNCMMDBCache*>(pcache)->SetOptimalSize(nCachesize);
}

/// Get number of pages stored in cache
static int
s_SQLITE_PCache_GetSize(sqlite3_pcache* pcache)
{
    return reinterpret_cast<CNCMMDBCache*>(pcache)->GetSize();
}

/// Get page from cache
static void*
s_SQLITE_PCache_GetPage(sqlite3_pcache* pcache,
                        unsigned int    key,
                        int             createFlag)
{
    return reinterpret_cast<CNCMMDBCache*>(pcache)
                        ->PinPage(key, CNCMMDBCache::EPageCreate(createFlag));
}

/// Release page (make it reusable by others)
static void
s_SQLITE_PCache_UnpinPage(sqlite3_pcache* pcache, void* page, int discard)
{
    reinterpret_cast<CNCMMDBCache*>(pcache)->UnpinPage(page, discard != 0);
}

/// Change key of the page in cache
static void
s_SQLITE_PCache_RekeyPage(sqlite3_pcache* pcache,
                          void*           page,
                          unsigned int    oldKey,
                          unsigned int    newKey)
{
    reinterpret_cast<CNCMMDBCache*>(pcache)->ChangePageKey(page, oldKey, newKey);
}

/// Truncate cache, delete all pages with keys greater or equal to given limit
static void
s_SQLITE_PCache_Truncate(sqlite3_pcache* pcache, unsigned int iLimit)
{
    reinterpret_cast<CNCMMDBCache*>(pcache)->DeleteAllPages(iLimit);
}

/// Destroy cache instance
static void
s_SQLITE_PCache_Destroy(sqlite3_pcache* pcache)
{
    delete reinterpret_cast<CNCMMDBCache*>(pcache);
}

///
static void*
s_SQLITE_Mem_Malloc(int size)
{
    if (size > kNCMMSQLiteBigBlockMin) {
        _ASSERT(size <= kNCMMSQLiteBigBlockReal);
        size = kNCMMSQLiteBigBlockReal;
    }
    return CNCMMCentral::AllocMemory(size);
}

///
static void
s_SQLITE_Mem_Free(void* ptr)
{
    CNCMMCentral::DeallocMemory(ptr);
}

///
static void*
s_SQLITE_Mem_Realloc(void* ptr, int new_size)
{
    return CNCMMCentral::ReallocMemory(ptr, new_size);
}

///
static int
s_SQLITE_Mem_Size(void* ptr)
{
    return CNCMMCentral::GetMemorySize(ptr);
}

///
static int
s_SQLITE_Mem_Roundup(int size)
{
    return size;
}

static int
s_SQLITE_Mem_Init(void*)
{
    return SQLITE_OK;
}

static void
s_SQLITE_Mem_Shutdown(void*)
{}


/// All methods of database cache exposed to SQLite
static sqlite3_pcache_methods s_NCDBCacheMethods = {
    NULL,                       /* pArg */
    s_SQLITE_PCache_Init,       /* xInit */
    s_SQLITE_PCache_Shutdown,   /* xShutdown */
    s_SQLITE_PCache_Create,     /* xCreate */
    s_SQLITE_PCache_SetSize,    /* xCachesize */
    s_SQLITE_PCache_GetSize,    /* xPagecount */
    s_SQLITE_PCache_GetPage,    /* xFetch */
    s_SQLITE_PCache_UnpinPage,  /* xUnpin */
    s_SQLITE_PCache_RekeyPage,  /* xRekey */
    s_SQLITE_PCache_Truncate,   /* xTruncate */
    s_SQLITE_PCache_Destroy     /* xDestroy */
};

///
static sqlite3_mem_methods s_NCMallocMethods = {
    s_SQLITE_Mem_Malloc,    /* xMalloc */
    s_SQLITE_Mem_Free,      /* xFree */
    s_SQLITE_Mem_Realloc,   /* xRealloc */
    s_SQLITE_Mem_Size,      /* xSize */
    s_SQLITE_Mem_Roundup,   /* xRoundup */
    s_SQLITE_Mem_Init,      /* xInit */
    s_SQLITE_Mem_Shutdown,  /* xShutdown */
    NULL                    /* pAppData */
};


void
CNCMemManager::InitializeApp(void)
{
    CSQLITE_Global::SetCustomPageCache(&s_NCDBCacheMethods);
    CSQLITE_Global::SetCustomMallocFuncs(&s_NCMallocMethods);

    CNCMMCentral::RunLateInit();
}

void
CNCMemManager::FinalizeApp(void)
{
    CNCMMCentral::PrepareToStop();
}

void
CNCMemManager::SetLimit(size_t mem_size)
{
    CNCMMCentral::SetMemLimit(mem_size);
}

void
CNCMemManager::PrintStats(CPrintTextProxy& proxy)
{
    CNCMMStats stats_sum;
    CNCMMStats::CollectAllStats(&stats_sum);
    stats_sum.Print(proxy);
}

END_NCBI_SCOPE


void*
operator new (size_t size)
{
    return NCBI_NS_NCBI::CNCMMCentral::AllocMemory(size);
}

void
operator delete (void* ptr)
{
    NCBI_NS_NCBI::CNCMMCentral::DeallocMemory(ptr);
}

void*
operator new[] (size_t size)
{
    return NCBI_NS_NCBI::CNCMMCentral::AllocMemory(size);
}

void
operator delete[] (void* ptr)
{
    NCBI_NS_NCBI::CNCMMCentral::DeallocMemory(ptr);
}

// Changing of C library allocation functions on Windows is very tricky (if
// possible at all) and NetCache will never run in production on Windows. So
// let's just avoid not necessary headache.
#ifndef NCBI_OS_MSWIN

void*
malloc(size_t size)
{
    return NCBI_NS_NCBI::CNCMMCentral::AllocMemory(size);
}

void
free(void* ptr)
{
    NCBI_NS_NCBI::CNCMMCentral::DeallocMemory(ptr);
}

void*
realloc(void* ptr, size_t new_size)
{
    return NCBI_NS_NCBI::CNCMMCentral::ReallocMemory(ptr, new_size);
}

void*
calloc(size_t num, size_t size)
{
    size_t mem_size = size * num;
    void* mem_ptr = NCBI_NS_NCBI::CNCMMCentral::AllocMemory(mem_size);
    memset(mem_ptr, 0, mem_size);
    return mem_ptr;
}

#endif
