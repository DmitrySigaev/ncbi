/*****************************************************************************
* $Id$
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
*  Authors: Ilya Dondoshansky, Michael Kimelman
*
* ===========================================================================
*
*  gicache_lib.c
*
*****************************************************************************/

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>

#include <sys/mman.h>
#include <sys/poll.h>
#include <fcntl.h>
#include "gicache.hpp"

/****************************************************************************
 *
 * gi_data_index.h 
 *
 ****************************************************************************/

#ifdef TEST_RUN
#  ifdef NDEBUG
#    undef NDEBUG
#  endif
#endif

#define kPageSize 8
#define kPageMask 0xff

#define kLocalOffsetMask 0xffff
#define kFullOffsetMask 0x7fffffff
#define kTopBit 0x80000000

typedef bool Boolean;
#define TRUE true
#define FALSE false
#define INLINE inline
#define NULLB '\0'

typedef struct {
    Uint4*  m_GiIndex;
    char*   m_Data;
    int     m_GiIndexFile;
    int     m_DataFile;
    Uint4   m_GiIndexLen;
    Uint4   m_DataLen;
    Uint4   m_MappedDataLen;
    Uint4   m_MappedIndexLen;
    Boolean m_ReadOnlyMode;
    Uint4   m_IndexCacheLen;
    Uint4   m_DataCacheLen;
    char    m_FileNamePrefix[256];
    Uint4   m_DataUnitSize;
    Uint4   m_IndexCacheSize;
    Uint4*  m_IndexCache;
    Uint4   m_DataCacheSize;
    char*   m_DataCache;
    Boolean m_SequentialData;
    Boolean m_FreeOnDrop;
    int     m_Remapping; /* Count of threads currently trying to remap */
    Boolean m_NeedRemap; /* Is remap needed? */
    Boolean m_RemapOnRead; /* Is remap allowed when reading data? */
} SGiDataIndex;

/****************************************************************************
 *
 * gi_data_index.c
 *
 ****************************************************************************/

#define INDEX_CACHE_SIZE 32768
#define DATA_CACHE_SIZE 8192
static void x_DumpIndexCache(SGiDataIndex* data_index)
{
    if (data_index->m_GiIndexFile >= 0 && data_index->m_IndexCacheLen > 0) {
        _ASSERT(data_index->m_GiIndexLen*sizeof(int) ==
               lseek(data_index->m_GiIndexFile, 0, SEEK_CUR));
        _ASSERT(data_index->m_GiIndexLen*sizeof(int) ==
               lseek(data_index->m_GiIndexFile, 0, SEEK_END));
        /* Write to the index file whatever is still left in cache. */
        write(data_index->m_GiIndexFile, data_index->m_IndexCache,
              data_index->m_IndexCacheLen*sizeof(int));
        data_index->m_GiIndexLen += data_index->m_IndexCacheLen;
        _ASSERT(data_index->m_GiIndexLen*sizeof(int) ==
               lseek(data_index->m_GiIndexFile, 0, SEEK_CUR));
        _ASSERT(data_index->m_GiIndexLen*sizeof(int) ==
               lseek(data_index->m_GiIndexFile, 0, SEEK_END));
        data_index->m_IndexCacheLen = 0;
    }
}

static void x_DumpDataCache(SGiDataIndex* data_index)
{
    if (data_index->m_DataFile >= 0 && data_index->m_DataCacheLen > 0) {
        _ASSERT(data_index->m_DataLen ==
               lseek(data_index->m_DataFile, 0, SEEK_CUR));
        _ASSERT(data_index->m_DataLen ==
               lseek(data_index->m_DataFile, 0, SEEK_END));
        /* Write to the data file whatever is still left in cache. */
        write(data_index->m_DataFile, data_index->m_DataCache,
              data_index->m_DataCacheLen);
        data_index->m_DataLen += data_index->m_DataCacheLen;
        _ASSERT(data_index->m_DataLen == 
               lseek(data_index->m_DataFile, 0, SEEK_CUR));
        _ASSERT(data_index->m_DataLen == 
               lseek(data_index->m_DataFile, 0, SEEK_END));
        data_index->m_DataCacheLen = 0;
    }
}

static void x_CloseIndexFiles(SGiDataIndex* data_index)
{
    if (data_index->m_GiIndexFile >= 0) {
        x_DumpIndexCache(data_index);
        close(data_index->m_GiIndexFile);
        data_index->m_GiIndexFile = -1;
        data_index->m_GiIndexLen = 0;
        data_index->m_MappedIndexLen = 0;
    }
}

static void x_CloseDataFiles(SGiDataIndex* data_index)
{
    if (data_index->m_DataFile >= 0) {
        x_DumpDataCache(data_index);
        close(data_index->m_DataFile);
        data_index->m_DataFile = -1;
        data_index->m_DataLen = 0;
        data_index->m_MappedDataLen = 0;
    }
}

/* Closes all files */
static void x_CloseFiles(SGiDataIndex* data_index)
{
    x_CloseIndexFiles(data_index);
    x_CloseDataFiles(data_index);
}

static void x_UnMapIndex(SGiDataIndex* data_index)
{
    if (data_index->m_GiIndex != MAP_FAILED) {
        munmap((char*)data_index->m_GiIndex,
               data_index->m_MappedIndexLen*sizeof(Uint4));
        data_index->m_GiIndex = (Uint4*)MAP_FAILED;
        data_index->m_MappedIndexLen = 0;
    }
}

static void x_UnMapData(SGiDataIndex* data_index)
{
    if (data_index->m_Data != MAP_FAILED) {
        munmap((char*)data_index->m_Data, data_index->m_MappedDataLen);
        data_index->m_Data = (char*)MAP_FAILED;
        data_index->m_MappedDataLen = 0;
    }
}

/* Unmaps data and index files */
static void x_UnMap(SGiDataIndex* data_index)
{
    x_UnMapIndex(data_index);
    x_UnMapData(data_index);
}

static Boolean x_OpenIndexFiles(SGiDataIndex* data_index)
{
    char buf[256];
    int flags;

    if (data_index->m_GiIndexFile >= 0)
        return TRUE;
    
    flags = (data_index->m_ReadOnlyMode?O_RDONLY:O_RDWR|O_APPEND|O_CREAT);
    
    x_UnMapIndex(data_index);
    x_CloseIndexFiles(data_index);
    
    strcpy(buf, data_index->m_FileNamePrefix);

    strcat(buf, "idx");
    data_index->m_GiIndexFile = open(buf,flags,0644);
    data_index->m_GiIndexLen = 
        (data_index->m_GiIndexFile >= 0 ? 
         lseek(data_index->m_GiIndexFile, 0, SEEK_END)/sizeof(int) : 0);

    if (data_index->m_GiIndexLen == 0 && !data_index->m_ReadOnlyMode &&
        data_index->m_GiIndexFile) {
        Uint4* b;
        /* First page of the index is reserved for the pointers to other pages. */
        data_index->m_GiIndexLen = 1<<kPageSize;
        b = (Uint4*) calloc(data_index->m_GiIndexLen, sizeof(Uint4));
        _ASSERT(0 == lseek(data_index->m_GiIndexFile, 0, SEEK_END));
        write(data_index->m_GiIndexFile, b,
              data_index->m_GiIndexLen*sizeof(Uint4));
        free(b);
        _ASSERT(data_index->m_GiIndexLen*sizeof(Uint4) ==
               lseek(data_index->m_GiIndexFile, 0, SEEK_CUR));
        _ASSERT(data_index->m_GiIndexLen*sizeof(Uint4) ==
               lseek(data_index->m_GiIndexFile, 0, SEEK_END));
    }
    
    return (data_index->m_GiIndexFile >= 0);
}

/* Opens data and index files, including check if they are already open. */
static Boolean x_OpenDataFiles(SGiDataIndex* data_index)
{
    char buf[256];
    int flags;

    if (data_index->m_DataFile >= 0)
        return TRUE;
    
    flags = (data_index->m_ReadOnlyMode?O_RDONLY:O_RDWR|O_APPEND|O_CREAT);
    
    x_UnMapData(data_index);
    x_CloseDataFiles(data_index);
    
    strcpy(buf, data_index->m_FileNamePrefix);
    strcat(buf, "dat");
    data_index->m_DataFile = open(buf,flags,0644);
    data_index->m_DataLen = (data_index->m_DataFile >= 0 ? 
                             lseek(data_index->m_DataFile, 0, SEEK_END) : 0);
    if (data_index->m_DataLen == 0 && !data_index->m_ReadOnlyMode &&
        data_index->m_DataFile) {
        /* Fill first 2*sizeof(int) bytes with 0: this guarantees that all 
         * offsets into data file will be > 0, allowing 0 to mean absense of
         * data. There reason to write 2 integers can be helpful in case of
         * binary data, making sure that data is aligned.
         */
        int  b[2];
        memset(b, 0, sizeof(b));
        _ASSERT(0 == lseek(data_index->m_DataFile, 0, SEEK_END));
        write(data_index->m_DataFile, b, sizeof(b));
        data_index->m_DataLen = sizeof(b);
        _ASSERT(data_index->m_DataLen ==
               lseek(data_index->m_DataFile, 0, SEEK_CUR));
        _ASSERT(data_index->m_DataLen ==
               lseek(data_index->m_DataFile, 0, SEEK_END));
    }
    
    return (data_index->m_DataFile >= 0);
}

/* hack needed for SGI IRIX where MAP_NORESERVE isn't defined. Dima */
#ifndef MAP_NORESERVE
#define MAP_NORESERVE (0)
#endif

static Boolean x_MapIndex(SGiDataIndex* data_index)
{
    int prot;
    Uint4 map_size;

    if (!x_OpenIndexFiles(data_index)) return FALSE;

    if (data_index->m_GiIndex != MAP_FAILED) return TRUE;
    
    x_DumpIndexCache(data_index);
    prot = PROT_READ | (data_index->m_ReadOnlyMode? 0: PROT_WRITE);
    map_size = data_index->m_GiIndexLen*sizeof(Uint4);
    _ASSERT(map_size == lseek(data_index->m_GiIndexFile, 0, SEEK_CUR));
    _ASSERT(map_size == lseek(data_index->m_GiIndexFile, 0, SEEK_END));
    data_index->m_GiIndex =
        (Uint4*)mmap(0, map_size, prot, MAP_SHARED|MAP_NORESERVE, 
                     data_index->m_GiIndexFile, 0);
    data_index->m_MappedIndexLen = data_index->m_GiIndexLen;

    return (data_index->m_GiIndex != MAP_FAILED);
}

static Boolean x_MapData(SGiDataIndex* data_index)
{
    if (!x_OpenDataFiles(data_index)) return FALSE;

    if (data_index->m_Data == MAP_FAILED) {
        int prot = PROT_READ | (data_index->m_ReadOnlyMode ? 0 : PROT_WRITE);
        x_DumpDataCache(data_index);
        
        data_index->m_Data =
            (char*) mmap(0, data_index->m_DataLen, prot,
                         MAP_SHARED|MAP_NORESERVE, data_index->m_DataFile, 0);
        data_index->m_MappedDataLen = data_index->m_DataLen;
    }

    return (data_index->m_Data != MAP_FAILED);
}

static Boolean x_ReMapIndex(SGiDataIndex* data_index)
{
    x_UnMapIndex(data_index);
    x_CloseIndexFiles(data_index);
    if (!x_MapIndex(data_index)) return FALSE;
    
    return TRUE;
}

static Boolean x_ReMapData(SGiDataIndex* data_index)
{
    x_UnMapData(data_index);
    if (!x_MapData(data_index)) return FALSE;
    
    return TRUE;
}

static void x_Flush(SGiDataIndex* data_index)
{
    if (data_index->m_DataFile >= 0) {
        if (data_index->m_DataCacheLen > 0) {
            int prot;

            /* Remap the data file */
            munmap((char*)data_index->m_Data, data_index->m_MappedDataLen);
            data_index->m_Data = (char*)MAP_FAILED;
            /* Dump cache */
            x_DumpDataCache(data_index);

            /* Redo the memory mapping */
            prot = PROT_READ | (data_index->m_ReadOnlyMode? 0: PROT_WRITE);
            data_index->m_Data =
                (char*) mmap(0, data_index->m_DataLen, prot,
                             MAP_SHARED|MAP_NORESERVE, data_index->m_DataFile, 0);
            data_index->m_MappedDataLen = data_index->m_DataLen;
        } else {
            /* Synchronize memory mapped data with the file on disk. */
            msync(data_index->m_Data, data_index->m_DataLen, MS_SYNC);
        }
    }

    /* Flush index file */
    if (data_index->m_GiIndexFile >= 0) {
        if (data_index->m_IndexCacheLen > 0) {
            int prot;

            /* Remap the index file */
            munmap((char*)data_index->m_GiIndex,
                   data_index->m_MappedIndexLen*sizeof(Uint4));
            data_index->m_GiIndex = (Uint4*)MAP_FAILED;
            /* Dump index cache */
            x_DumpIndexCache(data_index);

            /* Redo the memory mapping */
            prot = PROT_READ | (data_index->m_ReadOnlyMode? 0: PROT_WRITE);
            data_index->m_GiIndex =
                (Uint4*) mmap(0, data_index->m_GiIndexLen*sizeof(Uint4), prot, 
                              MAP_SHARED|MAP_NORESERVE,
                              data_index->m_GiIndexFile, 0);
            data_index->m_MappedIndexLen = data_index->m_GiIndexLen;
        } else {
            /* Synchronize memory mapped data with the file on disk. */
            msync((char*)data_index->m_GiIndex, data_index->m_GiIndexLen,
                  MS_SYNC);
        }
    }
}

static Boolean GiDataIndex_ReMap(SGiDataIndex* data_index, int delay)
{
    /* If some other thread has already done the remapping or is in the process
       of doing it, there is nothing to do here. */
    if (!data_index->m_NeedRemap || data_index->m_Remapping)
        return TRUE;

    ++data_index->m_Remapping;

    /* Wait a little bit and check if some other thread has started doing the
       remapping. In that case let the other thread do it. */
    poll(NULL, 0, delay);

    if (data_index->m_Remapping > 1) {
        data_index->m_Remapping--;
        return FALSE;
    }
    assert(data_index->m_Remapping == 1);


    x_Flush(data_index);

    if (!x_ReMapIndex(data_index))
        return FALSE;
    if (!x_ReMapData(data_index))
        return FALSE;

    /* Inform any other threads that may want remapped data that remapping has
       already finished. */
    data_index->m_Remapping = 0;
    data_index->m_NeedRemap = FALSE;

    return TRUE;
}

/* For each page of 256 2-byte index array slots, the first 2 slots (0 and 1)
 * contain the full 4-byte offset for page element 0.
 * To catch up with the array index, the relative offsets for page elements
 * 1 and 2 are encoded in the 1-byte parts of slot 2. 
 * All other relative 2-byte offsets are encoded in slots corresponding to 
 * the page element with the same index.
 * To distinguish presence of element 0 in the index, the 4-byte offsets for 
 * leaf pages are encoded in the first 31 bits, and top bit is only set when
 * 0th element exists.
 * On error condition this function returns -1
 */
static int
x_GetIndexOffset(SGiDataIndex* data_index, int gi, Uint4 page, int level)
{
    int base = 0;
    Uint4 base_page;
    Uint4 remainder = page & kPageMask;
    Uint4* gi_index = NULL;

    if (page >= data_index->m_GiIndexLen + data_index->m_IndexCacheLen) {
        /* This is an error condition, however it is not fatal, hence return 0,
           which means this page could not be found in the index! */
        return 0;
    } else {
        if (page < data_index->m_GiIndexLen) {
            if (page >= data_index->m_MappedIndexLen) {
                data_index->m_NeedRemap = TRUE;
                if (!data_index->m_RemapOnRead ||
                    !GiDataIndex_ReMap(data_index,0))
                    return -1;
            }
            gi_index = data_index->m_GiIndex;
        } else {
            page -= data_index->m_GiIndexLen;
            gi_index = data_index->m_IndexCache;
        }

        /* Non-leaf pages contain direct offsets to other index pages.
         * When data is added to index sequentially, the leaf offsets can be
         * encoded in 2 bytes as increment to the offset on the beginning of the
         * page, i.e. 2 gis can be encoded in the same 4-byte index array
         * element.
         */
        if (level > 0 || !data_index->m_SequentialData) {
            base = (int) gi_index[page];
        } else if (remainder == 0 && (gi&1) == 0) {
            if ((gi_index[page] & kTopBit) == kTopBit)
                base = gi_index[page] & kFullOffsetMask;
            else
                base = 0;
        } else {
            Uint4 mask; 
            base_page = page - remainder;
            if (remainder == 0 && (gi&1) == 1) {
                base = (gi_index[base_page+1]>>24);
                mask = kPageMask;
            } else if (remainder == 1 && (gi&1) == 0) {
                base = ((gi_index[page]>>16) & kPageMask);
                mask = kPageMask;
            } else if ((gi&1) == 0) {
                base = (gi_index[page]>>16);
                mask = kLocalOffsetMask;
            } else { /* (gi&1) == 1 */
                base = (gi_index[page]&kLocalOffsetMask);
                mask = kLocalOffsetMask;
            }
            
            if ((Uint4)base == mask) {
                base = (int) gi_index[base_page] & kFullOffsetMask;
            } else if (base != 0) {
                base += (int) gi_index[base_page] & kFullOffsetMask;
            }
        }
    }

    return base;
}

static char* x_GetGiData(SGiDataIndex* data_index, int gi)
{
    Uint4 page = 0;
    int base = 0;
    int shift = (data_index->m_SequentialData ? 1 : 0);
    int level;

    /* If some thread is currently remapping, the data is in an inconsistent
       state, therefore return NULL. */
    if (data_index->m_Remapping)
        return NULL;

    if ((data_index->m_GiIndex == MAP_FAILED ||
         data_index->m_Data == MAP_FAILED)) {
        data_index->m_NeedRemap = TRUE;
        if (!data_index->m_RemapOnRead || !GiDataIndex_ReMap(data_index, 0))
            return NULL;
    }

    _ASSERT((data_index->m_GiIndex != MAP_FAILED) && 
           (data_index->m_Data != MAP_FAILED));

    for (level = 3; level >= 0; --level) {

        /* Get this gi's page number and find the starting offset for that page's 
           information in the index file. */
        page = (Uint4)base + ((gi>>(level*kPageSize+shift)) & kPageMask);

        /* The page can never point beyond the length of the index. If that 
         * happens, bail out.
         * If we got to a page that has been written, but not yet mapped, 
         * remapping must be done here.
         */
        base = x_GetIndexOffset(data_index, gi, page, level);
        
        /* The 0th page in the index file is reserved for the start offsets of
         * other pages. If we got there, it means the requested gi is not 
         * present in the index.
         */
        if (base <= 0)
            return NULL;
    }
    
    /* If offset points beyond the combined length of the mapped data and cache,
       try to remap data. If that still doesn't help, bail out. */
    if ((Uint4)base >= data_index->m_DataLen + data_index->m_DataCacheLen) {
        data_index->m_NeedRemap = TRUE;
        if (!data_index->m_RemapOnRead || !GiDataIndex_ReMap(data_index,0) ||
            (Uint4)base >= data_index->m_DataLen + data_index->m_DataCacheLen) 
            return NULL;
    }

    /* If offset is beyond the mapped data, get the data from cache, otherwise
       from the memory mapped location. */
    if ((Uint4)base >= data_index->m_DataLen) {
        return data_index->m_DataCache + base - data_index->m_DataLen;
    } else {
        /* If offset points to data that has been written to disk but not yet
           mapped, remap now. */
        if ((Uint4)base >= data_index->m_MappedDataLen) {
            data_index->m_NeedRemap = TRUE;
            if (!data_index->m_RemapOnRead || !GiDataIndex_ReMap(data_index,0))
                return NULL;
        }
        return data_index->m_Data + base;
    }
}

/* Constructor */
static SGiDataIndex*
GiDataIndex_New(SGiDataIndex* data_index, int unit_size, const char* name,
                Boolean readonly, Boolean sequential)
{
    if (!data_index) {
        data_index = (SGiDataIndex*) malloc(sizeof(SGiDataIndex));
        data_index->m_FreeOnDrop = TRUE;
    } else {
        data_index->m_FreeOnDrop = FALSE;
    }

    data_index->m_ReadOnlyMode = readonly;
    _ASSERT(strlen(name) < 256);
    strncpy(data_index->m_FileNamePrefix, name, 256);
    data_index->m_DataUnitSize = unit_size;
    data_index->m_SequentialData = sequential;
    data_index->m_GiIndex = ((Uint4*)MAP_FAILED);
    data_index->m_Data = ((char*)MAP_FAILED);
    data_index->m_GiIndexFile = -1;
    data_index->m_DataFile = -1;
    data_index->m_GiIndexLen = 0;
    data_index->m_DataLen = 0;
    data_index->m_MappedDataLen = 0;
    data_index->m_MappedIndexLen = 0;
    data_index->m_IndexCacheLen = 0;
    data_index->m_IndexCacheSize = INDEX_CACHE_SIZE;
    data_index->m_IndexCache =
        (Uint4*) malloc(data_index->m_IndexCacheSize*sizeof(Uint4));
    data_index->m_DataCacheLen = 0;
    data_index->m_DataCacheSize = unit_size*DATA_CACHE_SIZE;
    data_index->m_DataCache = (char*) malloc(data_index->m_DataCacheSize);
    data_index->m_Remapping = 0;
    data_index->m_NeedRemap = TRUE;
    data_index->m_RemapOnRead = TRUE;

    return data_index;
}

/* Destructor */
static SGiDataIndex* GiDataIndex_Free(SGiDataIndex* data_index)
{
    if (!data_index)
        return NULL;

    x_Flush(data_index);
    x_UnMap(data_index);
    x_CloseFiles(data_index);
    free(data_index->m_IndexCache);
    free(data_index->m_DataCache);
    if (data_index->m_FreeOnDrop)
        free(data_index);
    return NULL;
}

/* Returns data corresponding to a given gi for reading only. */
static const char* GiDataIndex_GetData(SGiDataIndex* data_index, int gi)
{
    return x_GetGiData(data_index, gi);
}

/****************************************************************************
 *
 * gicache_lib.c 
 *
 ****************************************************************************/

#define MAX_ACCESSION_LENGTH 64

static SGiDataIndex gi_cache;

static void x_GICacheInit(const char* prefix, Boolean readonly)
{
    char prefix_str[256];
    sprintf(prefix_str, "%s.", (prefix ? prefix : "gi2acc"));
    /* When reading data, use readonly mode. */
    if ( strcmp(gi_cache.m_FileNamePrefix, prefix_str) != 0 ) {
        GiDataIndex_Free(&gi_cache);
        GiDataIndex_New(&gi_cache, MAX_ACCESSION_LENGTH, prefix_str, readonly, TRUE);
    }
}

void GICache_ReadData(const char *prefix)
{
    x_GICacheInit(prefix, TRUE);
}

static INLINE int s_DecodeInt4(const char* buf, int* val)
{
    if ((buf[0] & 0x80) != 0) {
        *val = ((buf[0]&0x7f)<<24) | ((buf[1]&0xff)<<16) |
               ((buf[2]&0xff)<<8)  | (buf[3]&0xff);
        return 4;
    } else {
        *val = ((buf[0]&0x7f)<<8) | (buf[1]&0xff);
        return 2;
    }
}

static INLINE int s_DecodeInt2(const char* buf, int* val)
{
    if ((buf[0] & 0x80) != 0) {
        *val = ((buf[0]&0x7f)<<8) | (buf[1]&0xff);
        return 2;
    } else {
        *val = buf[0] & 0x7f;
        return 1;
    }
}

static INLINE
void s_Decode3Plus5Accession(const char* buf, char* prefix, 
                             int* prefix_length, int* suffix)
{
    prefix[0] = ((buf[0]&0xff)>>3) + 'A' - 1;
    prefix[1] = (((buf[0]&0x07)<<2) | ((buf[1]&0xff)>>6)) + 'A' - 1;
    prefix[2] = (((buf[1]&0xff)>>1) & 0x1f) + 'A' - 1;
    *prefix_length = 3;
    *suffix = ((int)(buf[1] & 0x01)<<16) | (((int)(buf[2]&0xff)<<8) & 0xff00) |
               ((int)(buf[3] & 0xff));
}

static INLINE
void s_Decode2LetterAccession(const char* buf, Uint1 control_byte, char* prefix,
                             int* prefix_length, int* suffix)
{
    Boolean is_refseq = ((control_byte & (1<<5)) != 0);
    Boolean large_suffix = ((control_byte & (1<<6)) != 0);
    Uint1 byte;

    prefix[0] = ((buf[0]&0xff)>>3) + 'A' - 1;
    byte = ((buf[0]&0x07)<<2) | ((buf[1]&0xff)>>6);
    if (byte == 0) {
        *prefix_length = 1;
    } else {
        prefix[1] = byte + 'A' - 1;
        if (is_refseq) {
            prefix[2] = '_';
            *prefix_length = 3;
        } else {
            *prefix_length = 2;
        } 
    }

    if (large_suffix) {
        *suffix = (((int)(buf[1] & 0x1f)<<24) | (((int)(buf[2]) & 0xff)<<16) |
                  ((int)(buf[3] & 0xff)<<8) | ((int)(buf[4] & 0xff)));
    } else {
        *suffix = (((int)(buf[1] & 0x3f)<<16) | (((int)(buf[2]) & 0xff)<<8) |
                  ((int)(buf[3] & 0xff)));
    }
}

static INLINE
void s_Decode4Plus9Accession(const char* buf, Uint1 control_byte, char* prefix,
                             int* prefix_length, int* suffix)
{
    int pos = 0;
    if ((control_byte & (1<<5)) != 0) {
        /* NZ_-type Refseq */
        sprintf(prefix, "NZ_");
        pos = 3;
    }

    prefix[pos] = ((buf[0]&0xff)>>3) + 'A' - 1; 
    prefix[pos+1] = (((buf[0]&0x07)<<2) | ((buf[1]&0xff)>>6)) + 'A' - 1;
    prefix[pos+2] = (((buf[1]&0xff)>>1) & 0x1f) + 'A' - 1;
    prefix[pos+3] = (((buf[1]&0x01)<<4) | ((buf[2]&0xff)>>4)) + 'A' - 1;
    *prefix_length = pos + 4;
    *suffix = ((int)(buf[2]&0x0f)<<24) | ((int)(buf[3] & 0xff)<<16) | 
        ((int)(buf[4]&0xff)<<8) | ((int)(buf[5]&0xff));
}


static int
s_DecodeGiAccession(const char* inbuf, char* acc, int acc_len)
{
    Uint1 control_byte = *inbuf;
    Uint1 suffix_length = control_byte & 0x07;
    int version = 1;
    /* Use internal buffer to retrieve accession */
    char acc_buf[MAX_ACCESSION_LENGTH];
    int retval = 1;

    const char* buf = inbuf + 1;

    /* Skip the bytes containing sequence length */
    if ((buf[0]) & 0x80) {
        buf += 4;
    } else {
        buf += 2;
    }

    /* Retrieve version */
    if (control_byte & (1<<3)) {
        buf += s_DecodeInt2(buf, &version);
    }

    /* Retrieve integer accession suffix */
    if (suffix_length > 0) {
        int suffix = 0;
        int prefix_length = 0;
        if ((control_byte & (1<<4)) != 0) {
            s_Decode2LetterAccession(buf, control_byte, acc_buf, &prefix_length,
                                     &suffix); 
        } else if ((control_byte & 0xf0) == (1<<5)) {
            s_Decode3Plus5Accession(buf, acc_buf, &prefix_length, &suffix);
        } else if ((control_byte & (1<<6)) != 0) {
            s_Decode4Plus9Accession(buf, control_byte, acc_buf, &prefix_length,
                                    &suffix);
        } else {
            prefix_length = strlen(buf);
            strncpy(acc_buf, buf, prefix_length);
            buf += prefix_length + 1;
            buf += s_DecodeInt4(buf, &suffix);
        }

        sprintf(acc_buf+prefix_length, "%.*d", suffix_length+2, suffix);
    } else {
        sprintf(acc_buf, "%s", buf);
    }

    if (version > 0)
        sprintf(&acc_buf[strlen(acc_buf)], ".%d", version);

    /* If retrieved accession fits into the client-supplied buffer, then just
     * copy, otherwise copy the part that fits into the supplied buffer.
     */
    if (strlen(acc_buf) < acc_len) {
        strcpy(acc, acc_buf);
    } else {
        strncpy(acc, acc_buf, acc_len - 1);
        acc[acc_len-1] = NULLB;
        retval = 0;
    }
    return retval;
}

int GICache_GetAccession(int gi, char* acc, int acc_len)
{
    int retval = 0;
    const char* gi_data = GiDataIndex_GetData(&gi_cache, gi);
    if (gi_data) {
        retval = s_DecodeGiAccession(gi_data, acc, acc_len);
    } else {
        acc[0] = NULLB;
    }
    return retval;
}

int GICache_GetLength(int gi)
{
    int length = 0;
    const char *x = GiDataIndex_GetData(&gi_cache, gi);

    if(!x) return 0;

    x++; /* Skip control byte */
    x += s_DecodeInt4(x, &length);
    return length;
}
