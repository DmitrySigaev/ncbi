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
 * Author:  Kevin Bealer
 *
 */

#include <ncbi_pch.hpp>

#include "seqdbatlas.hpp"
#include <algorithm>
#include <objtools/readers/seqdb/seqdbcommon.hpp>

#if defined(NCBI_OS_UNIX)
#include <unistd.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#endif

BEGIN_NCBI_SCOPE

// Further optimizations:

// 1. Regions could be stored in a map<>, sorted by file, then offset.
// This would allow a binary search instead of sequential and would
// vastly improve the "bad case" of 100_000s of buffers of file data.

// 2. "Scrounging" could be done in the file case.  It is bad to read
// 0-4096 then 4096 to 8192, then 4000-4220.  The third could use the
// first two to avoid reading.  It should either combine the first two
// regions into a new region, or else just copy to a new region and
// leave the old ones alone (possibly marking the old regions as high
// penalty).  Depending on refcnt, penalty, and region sizes.

/// Create an empty atlas.

template<class TIn, class TOut>
TOut SeqDB_CheckLength(TIn value)
{
    TOut result = TOut(value);
    
    if (sizeof(TOut) < sizeof(TIn)) {
        if (TIn(result) != value) {
            NCBI_THROW(CSeqDBException,
                       eFileErr,
                       "Offset type does not span file length.");
        }
    }
    
    return result;
}

CSeqDBAtlas::CSeqDBAtlas(bool use_mmap)
    : m_UseMmap   (use_mmap),
      m_CurAlloc  (0),
      m_LastFID   (0)
{
    for(Uint4 i = 0; i < eNumRecent; i++) {
        m_Recent[i] = 0;
    }
}

CSeqDBAtlas::~CSeqDBAtlas()
{
    x_GarbageCollect(0);
    
    // Clear mapped file regions
    
    if ((! m_Regions.empty()) || (m_CurAlloc != 0)) {
        if (! m_Regions.empty()) {
            ShowLayout(true, 0);
        }
        
        _ASSERT(m_Regions.empty());
        _ASSERT(m_CurAlloc == 0);
    }
    
    // For now, and maybe permanently, enforce balance.
    
    _ASSERT(m_Pool.size() == 0);
    
    // Erase 'manually allocated' elements - In debug mode, this will
    // not execute, because of the above test.
    
    for(TPoolIter i = m_Pool.begin(); i != m_Pool.end(); i++) {
        delete[] (char*)((*i).first);
    }
    
    m_Pool.clear();
}

const char * CSeqDBAtlas::GetFile(const string & fname, TIndx & length, CSeqDBLockHold & locked)
{
    if (! GetFileSize(fname, length)) {
        NCBI_THROW(CSeqDBException, eFileErr, "File did not exist.");
    }
    
    // If allocating more than 256MB in a file, do a full sweep first.
    // This technique may help prevent unnecessary fragmentation.
    // How?  Well, it's kind of a fudge, really: before allocating
    // anything really big, we want to clean memory as much as possible.
    //
    // Essentially, big objects can fail due to fragmentation, even if
    // we are well below the memory bound.  So if there is a big spot
    // where this new allocation may fit, we want to remove any small
    // objects from it first.  When I mention fragmentation here, I
    // mean "you have 1.3 GB of address space left, but no piece
    // bigger than .75 GB".  Memory mapping is sensitive to this
    // because it needs huge contiguous chunks of sizes that are not
    // aligned.
    // 
    // It should be mentioned that this will not (greatly) affect
    // users who are using the round-to-chunk-size allocator.
    
    if (length > eTriggerGC) {
        Lock(locked);
        x_GarbageCollect(0);
    }
    
    return GetRegion(fname, 0, length, locked);
}

void CSeqDBAtlas::GetFile(CSeqDBMemLease & lease, const string & fname, TIndx & length, CSeqDBLockHold & locked)
{
    if (! GetFileSize(fname, length)) {
        NCBI_THROW(CSeqDBException, eFileErr, "File did not exist.");
    }
    
    // If allocating more than 256MB in a file, do a full sweep first.
    // This technique may help prevent unnecessary fragmentation.
    // How?  Well, it's kind of a fudge, really: before allocating
    // anythin humongous, we want to clean memory as much as possible.
    //
    // Essentially, big objects can fail due to fragmentation, even if
    // we are well below the memory bound.  So if there is a big spot
    // where this new allocation may fit, we want to remove any small
    // objects from it first.  When I mention fragmentation here, I
    // mean "you have 1.3 GB of address space left, but no piece
    // bigger than .75 GB".  Memory mapping is sensitive to this
    // because it needs huge contiguous chunks of sizes that are not
    // aligned.
    // 
    // It should be mentioned that this will not (greatly) affect
    // users who are using the round-to-chunk-size allocator.
    //
    // Also, for systems with (e.g.) 64 bit address spaces, this
    // could/should be relaxed, to require less mapping.
    
    if (length > eTriggerGC) {
        GarbageCollect(locked);
    }
    
    Lock(locked);
    GetRegion(lease, fname, 0, length);
}

bool CSeqDBAtlas::GetFileSize(const string & fname, TIndx & length)
{
    CFile whole(fname);
    
    if (whole.Exists()) {
        length = SeqDB_CheckLength<Uint8,TIndx>(whole.GetLength());
        return true;
    }
    
    length = 0;
    return false;
}

void CSeqDBAtlas::GarbageCollect(CSeqDBLockHold & locked)
{
    Lock(locked);
    x_GarbageCollect(0);
}

void CSeqDBAtlas::x_GarbageCollect(TIndx reduce_to)
{
    x_ClearRecent();
    
    int max_distinct_clock = 10;
    
    if (m_CurAlloc <= reduce_to) {
        return;
    }
    
    int  num_gcs  = 1;
    
    if (reduce_to > 0) {
        TIndx in_use = m_CurAlloc;
        
        for(unsigned i = 0; i < m_Regions.size(); i++) {
            CRegionMap * mr = m_Regions[i];
            
            if (! mr->InUse()) {
                mr->BumpClock();
                in_use -= mr->Length();
            }
            
            num_gcs = ((num_gcs > mr->GetClock())
                       ? num_gcs
                       : mr->GetClock()); // max
        }
        
        num_gcs = 1 + ((num_gcs < max_distinct_clock)
                       ? num_gcs
                       : max_distinct_clock); //min
    }
    
    while(num_gcs >= 0) {
        num_gcs --;
        
        unsigned i = 0;
        
        while(i < m_Regions.size()) {
            CRegionMap * mr = m_Regions[i];
            
            if (mr->InUse() || mr->GetClock() < num_gcs) {
                i++;
                continue;
            }
            
            unsigned last = m_Regions.size() - 1;
            
            if (i != last) {
                m_Regions[i] = m_Regions[last];
            }
            
            m_Regions.pop_back();
            
            m_CurAlloc -= mr->Length();
            
            m_NameOffsetLookup.erase(mr);
            m_AddressLookup.erase(mr->Data());
            
            delete mr;
            
            if (m_CurAlloc < reduce_to) {
                return;
            }
        }
    }
}


// Algorithm:
// 
// In file mode, get exactly what we need.  Otherwise, if the request
// fits entirely into one slice, use large slice rounding (get large
// pieces).  If it is on a large slice boundary, use small slice
// rounding (get small pieces).


// Rationale:
//
// We would like to map all data using the same slice size, and in
// practice, 99% of the objects *will* fall entirely within the large
// slice boundaries.  But for cases where they do not, we have several
// strategies.
// 
// The simplest is to round both boundaries out to large slices,
// making a double-wide slice.  The problem with this is that in a
// straight-through traversal, every large slice will be mapped twice,
// because each overlap case will cause a mapping of one already
// mapped large slice (the most recent one) and one not-yet mapped
// slice (the one after it).
// 
// Instead, I am using two strategies to avoid this phenomena.  The
// first is the use of one large slice size and one small slice size.
// Boundary cases (including any object not fitting in one slice) will
// be rounded out to the smaller slice size.  This solves the problem
// of the boundary case: the small allocations will only cause a
// minimal "duplication" of the mapped area.
//
// The memory layout pattern due to the previous technique will
// alternate between short and long slices, with each short slice
// echoing the end and beginning of the slices before and after it,
// respectively.  The short slices represent redundant mapping.  To
// prevent the proliferation of short slices, there is a second
// technique to preferentially remove "irregular" sequences.
//
// Each mapping also gets a "penalty" value: the value is added to the
// "clock" value when considering mappings for garbage collection.
// The fragmentation is least when all the sequences are the same
// size.  The penalty value is 0 for mappings of size "slice", 1 for
// mappings of size "small slice" or "small slice * 2", and 2 for
// mappings that do not correspond to any slice size, i.e. the "tail"
// portion of a file, or whole mappings of short files. (KMB)


void CRegionMap::x_Roundup(TIndx & begin,
                           TIndx & end,
                           int   & penalty,
                           TIndx   file_size,
                           bool    use_mmap)
{
    // These should be made available to some kind of interface to
    // allow memory-usage tuning.
    
    const Uint4 large_slice = 1024 * 1024 * 256;
    const Uint4 small_slice = 1024 * 8;
    const Uint4 block_size  = 1024 * 512;
    
    _ASSERT(begin <  end);
    _ASSERT(end   <= file_size);
    
    penalty = 0;
    
    int align = 1;
    
    if (use_mmap) {
        TIndx page_b = begin / large_slice;
        TIndx page_e = end   / large_slice;
        
        if (page_b == page_e) {
            align = large_slice;
            penalty = 0;
        } else {
            if ((end-begin) < (small_slice * 2)) {
                penalty = 1;
            } else {
                penalty = 2;
            }
            
            align = small_slice;
        }
    } else {
        // File mode, align to block.  This only helps if there is
        // another sequence in the same block that interest us.
        
        penalty = 2;
        align = block_size;
    }
    
    if (align > 1) {
        // Integer math can do the rounding.
        
        TIndx new_begin = (begin / align) * align;
        TIndx new_end = ((end + align - 1) / align) * align;
        
        if ((new_end + align) > file_size) {
            new_end = file_size;
            penalty = 2;
        }
        
        _ASSERT(new_begin <= begin);
        _ASSERT(new_end   >= end  );
        
        begin = new_begin;
        end   = new_end;
    }
    
    // Final consideration, which naturally clobbers all previous
    // processing.  Since we don't actually have support for the
    // aforementioned in CMemoryFile (yet), the code cannot portably
    // do any of those things.  In UNIX like systems, we will use mmap
    // directly, but on other platforms, the roundup will revert to
    // the whole-file-method for now.  It is possible that other
    // systems also have this ability, in which case ors and elses
    // could be added to the following #if.
    
    bool have_range_mmap = false;
    
#if defined(NCBI_OS_UNIX)
    have_range_mmap = true;
#endif
    
    if (! have_range_mmap) {
        begin = 0;
        end   = file_size;
        
        // Prefer larger items to last longer..
        
        penalty = ((file_size > large_slice)
                   ? 0
                   : 1);
    }
}

const char * CSeqDBAtlas::x_FindRegion(Uint4         fid,
                                       TIndx       & begin,
                                       TIndx       & end,
                                       const char ** start,
                                       CRegionMap ** region)
{
    // Try recent matches first.
    
    for(Uint4 i = 0; i<eNumRecent; i++) {
        if (! m_Recent[i])
            break;
        
        const char * retval = m_Recent[i]->MatchAndUse(fid, begin, end, start);
        
        if (retval) {
            // Moves region to top
            if (region) {
                *region = m_Recent[i];
            }
            
            if (i) {
                x_AddRecent(m_Recent[i]);
            }
            
            return retval;
        }
    }
    
    if (m_NameOffsetLookup.empty()) {
        return 0;
    }
    
    // Start key - will be used to find the least element that is NOT
    // good enough.  We want the elements before this to have
    
    CRegionMap key(0, fid, begin, end);
    
    TNameOffsetTable::iterator iter
        = m_NameOffsetLookup.upper_bound(& key);
    
    while(iter != m_NameOffsetLookup.begin()) {
        --iter;
        
        if ((*iter)->Fid() != fid)
            return 0;
        
        CRegionMap * rmap = *iter;
        
        // Should be guaranteed by the ordering we are using.
        _ASSERT(rmap->Begin() <= begin);
        
        if (rmap->End() >= end) {
            const char * retval = rmap->MatchAndUse(fid, begin, end, start);
            _ASSERT(retval);
            
            if (region) {
                *region = rmap;
            }
            
            x_AddRecent(rmap);
            return retval;
        }
    }
    
    return 0;
}

const char *
CSeqDBAtlas::x_GetRegion(const string   & fname,
                         TIndx          & begin,
                         TIndx          & end,
                         const char    ** start,
                         CRegionMap    ** rmap)
{
    const char * dummy = 0;
    
    if (start == 0) {
        start = & dummy;
    }
    
    _ASSERT(end > begin);
    
    const string * strp = 0;
    
    Uint4 fid = x_LookupFile(fname, & strp);
    
    const char * retval = 0;
    
    if ((retval = x_FindRegion(fid, begin, end, start, rmap))) {
        return retval;
    }
    
    // Need to add the range, so GC first.
    
    {
        // Use Int8 to avoid "unsigned rollover."
        
        Int8 space_needed = end-begin;
        
        Int8 capacity_left = eMemoryBound;
        capacity_left -= m_CurAlloc;
        
        if (space_needed > capacity_left) {
            x_GarbageCollect(eMemoryBound - (end-begin));
        }
    }
    
    CRegionMap * nregion = new CRegionMap(strp, fid, begin, end);
    
    auto_ptr<CRegionMap> newmap(nregion);
    
    m_NameOffsetLookup.insert(nregion);
    
    if (rmap)
        *rmap = nregion;
    
    if (m_UseMmap) {
        if (newmap->MapMmap()) {
            retval = newmap->Data(begin, end);
            newmap->AddRef();
        }
    }
    
#ifdef _DEBUG
    if (m_UseMmap && (!retval)) {
        cerr << "Warning: mmap failed, trying file mapping..." << endl;
    }
#endif
    
    if (retval == 0 && newmap->MapFile()) {
        retval = newmap->Data(begin, end);
        newmap->AddRef();
    }
    
    newmap->GetBoundaries(start, begin, end);
    
    if (retval == 0) {
        NCBI_THROW(CSeqDBException, eFileErr, "File did not exist.");
    }
    
    m_AddressLookup[nregion->Data()] = nregion;
    
    m_CurAlloc += (end-begin);
    
    m_Regions.push_back(newmap.release());
    
    return retval;
}

const char * CSeqDBAtlas::GetRegion(const string   & fname,
                                    TIndx            begin,
                                    TIndx            end,
                                    CSeqDBLockHold & locked)
{
    Lock(locked);
    
    return x_GetRegion(fname, begin, end, 0, 0);
}

// Assumes lock is held.
void CSeqDBAtlas::GetRegion(CSeqDBMemLease & lease,
                            const string   & fname,
                            TIndx            begin,
                            TIndx            end)
{
    RetRegion(lease);
    
    const char * start(0);
    CRegionMap * rmap(0);
    
    const char * result = x_GetRegion(fname, begin, end, & start, & rmap);
    
    if (result) {
        _ASSERT(start);
        
        lease.SetRegion(begin, end, start, rmap);
    }
}

// Assumes lock is held

/// Releases a hold on a partial mapping of the file.
void CSeqDBAtlas::RetRegion(CSeqDBMemLease & ml)
{
    if (ml.m_Data) {
#ifdef _DEBUG
        const char * datap = ml.m_Data;
#endif
        _ASSERT(ml.m_RMap && ml.m_RMap->InRange(datap));
        
        ml.m_RMap->RetRef();
        
        ml.m_Data  = 0;
        ml.m_Begin = 0;
        ml.m_End   = 0;
    }
}

/// Releases a hold on a partial mapping of the file.
void CSeqDBAtlas::RetRegion(const char * datap, CSeqDBLockHold & locked)
{
    Lock(locked);
    
    for(Uint4 i = 0; i<eNumRecent; i++) {
        CRegionMap * rec_map = m_Recent[i];
        
        if (! rec_map)
            break;
        
        if (rec_map->InRange(datap)) {
            rec_map->RetRef();
            
            if (i) {
                x_AddRecent(rec_map);
            }
            
            return;
        }
    }
    
    CSeqDBAtlas::TAddressTable::iterator iter = m_AddressLookup.upper_bound(datap);
    
    if (iter != m_AddressLookup.begin()) {
        --iter;
        
        CRegionMap * rmap = (*iter).second;
        
        if (rmap->InRange(datap)) {
            x_AddRecent(rmap);
            rmap->RetRef();
            return;
        }
    }
    
    bool worked = x_Free(datap);
    _ASSERT(worked);
    
    if (! worked) {
        cerr << "Address leak in CSeqDBAtlas::RetRegion" << endl;
    }
}

void CSeqDBAtlas::ShowLayout(bool locked, TIndx index)
{
    CFastMutexGuard guard;
    
    if (! locked) {
        guard.Guard(m_Lock);
    }
    
    // MSVC fails to grok "ostream << [Uint8]". (Okalee-dokalee...)
    
#if _DEBUG
    cout << "\n\nShowing layout (index " << NStr::UInt8ToString((Uint8)index)
         << "), current alloc = " << m_CurAlloc << endl;
#endif
    
    for(unsigned i = 0; i < m_Regions.size(); i++) {
        m_Regions[i]->Show();
    }
    
#if _DEBUG
    cout << "\n\n" << endl;
#endif
}

// This does not attempt to garbage collect, but it will influence
// garbage collection if it is used enough.

char * CSeqDBAtlas::Alloc(Uint4 length, CSeqDBLockHold & locked)
{
    // What should/will happen on allocation failure?
    
    Lock(locked);
    
    if (! length) {
        length = 1;
    }
    
    // Allocate/clear
    
    char * newcp = new char[length];
    memset(newcp, 0, length);
    
    // Add to pool.
    
    _ASSERT(m_Pool.find(newcp) == m_Pool.end());
    
    m_Pool[newcp] = length;
    m_CurAlloc += length;
    
    return newcp;
}


void CSeqDBAtlas::Free(const char * freeme, CSeqDBLockHold & locked)
{
    Lock(locked);
    
#ifdef _DEBUG
    bool found =
        x_Free(freeme);
#endif
    
    _ASSERT(found);
}


bool CSeqDBAtlas::x_Free(const char * freeme)
{
    TPoolIter i = m_Pool.find((const char*) freeme);
    
    if (i == m_Pool.end()) {
        return false;
    }
    
    Uint4 sz = (*i).second;
    
    _ASSERT(m_CurAlloc >= sz);
    m_CurAlloc -= sz;
    
    char * cp = (char*) freeme;
    delete[] cp;
    m_Pool.erase(i);
    
    return true;
}

// Assumes lock is held.

void CRegionMap::Show()
{
#if VERBOSE
    cout << " [" << static_cast<const void*>(m_Data) << "]-["
         << static_cast<const void*>(m_Data + m_End - m_Begin) << "]: "
         << m_Fname << ", ref=" << m_Ref;
#endif
}

CRegionMap::CRegionMap(const string * fname, Uint4 fid, TIndx begin, TIndx end)
    : m_Data     (0),
      m_MemFile  (0),
      m_ManualMap(false),
      m_Fname    (fname),
      m_Begin    (begin),
      m_End      (end),
      m_Fid      (fid),
      m_Ref      (0),
      m_Clock    (0),
      m_Penalty  (0)
{
}

CRegionMap::~CRegionMap()
{
    if (m_MemFile) {
        delete m_MemFile;
        m_MemFile = 0;
        m_Data    = 0;
    }
    if (m_ManualMap) {
#if defined(NCBI_OS_UNIX)
        munmap(const_cast<char*>(m_Data), (size_t)(m_End - m_Begin));
        m_Data = 0;
#endif
        _ASSERT(m_Data == 0);
    }
    if (m_Data) {
        delete[] ((char*) m_Data);
        m_Data = 0;
    }
}

bool CRegionMap::MapMmap()
{
    bool rv = false;
    
    CFile file(*m_Fname);
    
    TIndx flength = SeqDB_CheckLength<Uint8,TIndx>(file.GetLength());
    
    if (file.Exists() &&
        (m_Begin == 0)          &&
        (m_End   == flength)) {
        
        m_MemFile    = new CMemoryFile(*m_Fname);
        m_Data       = (const char*) m_MemFile->GetPtr();
        m_ManualMap  = false;
        
        rv = true;
    }
    
    if (! rv) {
        x_Roundup(m_Begin, m_End, m_Penalty, flength, true);
        
#if defined(NCBI_OS_UNIX)
        // Use default attributes (we were going to anyway)
        int map_protect = PROT_READ;
        int file_access = O_RDONLY;
        int map_share   = MAP_PRIVATE;
        
        // Open file
        int fd = open(m_Fname->c_str(), file_access);
        if (fd < 0) {
            return false;
        }
        
        // Map file to memory
        m_Data = (const char*) mmap(0, (size_t) (m_End - m_Begin), map_protect, map_share, fd, (size_t) m_Begin);
        close(fd);
        
        if (m_Data == (const char*)MAP_FAILED) {
            m_Data = 0;
        } else {
            m_ManualMap = true;
            rv = true;
        }
#endif
    }
    
    // This does more harm than good, for reasons unknown.
    
// #if defined(NCBI_OS_UNIX)
//     if (rv && eSeqMadvise) {
//         cerr << "MADVISE: data " << (Uint4*)(m_Data) << " length " << (m_End - m_Begin) << " sequential." << endl;
//         madvise((void*) m_Data, m_End - m_Begin, MADV_SEQUENTIAL);
//     }
// #endif
    
    return rv;
}

bool CRegionMap::MapFile()
{
    // Okay, rethink:
    
    // 1. Unlike mmap, the file state disappears, the only state here
    // will be the data itself.  The stream is not kept because we
    // only need to read the data into a buffer, then the stream goes
    // away.
    
    // Steps:
    
    // 1. round up slice.
    // 2. open file, seek, and read.
    //    a. if read fails, delete section, return failure, and quit.
    //    a. if read works, return true.
    
    // Find file and get length
    
    CFile file(*m_Fname);
    CNcbiIfstream istr(m_Fname->c_str());
    
    if ((! file.Exists()) || istr.fail()) {
        return false;
    }
    
    // Round up slice size -- this should be a win for sequences if
    // they are read sequentially, and are smaller than one block
    // (16,000 base pairs).  This is a safe bet.  We are trading
    // memory bandwidth for disk bandwidth.  A more intelligent
    // algorithm might heuristically determine whether such an
    // optimization is working, or even switch back and forth and
    // measure performance.

    // For now, this code has to be rock solid, so I am avoiding
    // anything like cleverness.  The assumptions are:

    // (1) That reading whole blocks (and therefore avoiding multiple
    // IOs per block) saves more time than is lost in read(2) when
    // copying the (potentially) unused parts of the block.
    //
    // (2) That it is better for memory performance to store
    // whole-blocks in memory than to store 'trimmed' sequences.
    
    // If you think this is not true, consider the case of the index
    // file, where we would be storing regions consisting of only 4 or
    // 8 bytes.
    
    x_Roundup(m_Begin, m_End, m_Penalty, SeqDB_CheckLength<Uint8,TIndx>(file.GetLength()), false);
    
    istr.seekg(m_Begin);
    
    Uint8 rdsize8 = m_End - m_Begin;
    _ASSERT((TIndx(rdsize8) & TIndx(-1)) == rdsize8);
    
    TIndx rdsize = (TIndx) rdsize8;
    
    char * newbuf = new char[rdsize];
    
    if (! newbuf) {
        return false;
    }
    
    unsigned amt_read = 0;
    
    while((amt_read < rdsize) && istr) {
        istr.read(newbuf + amt_read, rdsize - amt_read);
        
        int count = istr.gcount();
        if (! count) {
            delete[] newbuf;
            return false;
        }
        
        amt_read += count;
    }
    
    m_Data = newbuf;
    
    return (amt_read == rdsize);
}

const char * CRegionMap::Data(TIndx begin, TIndx end)
{
    _ASSERT(m_Data != 0);
    _ASSERT(begin  >= m_Begin);
    _ASSERT(end    <= m_End);
    
    return m_Data + begin - m_Begin;
}

Uint4 CSeqDBAtlas::x_LookupFile(const string  & fname,
                                const string ** map_fname_ptr)
{
    map<string, Uint4>::iterator i = m_FileIDs.find(fname);
    
    if (i == m_FileIDs.end()) {
        m_FileIDs[fname] = ++ m_LastFID;
        
        i = m_FileIDs.find(fname);
    }
    
    // Get address of string in string->fid table.
    
    *map_fname_ptr = & (*i).first;
    
    return (*i).second;
}

END_NCBI_SCOPE

