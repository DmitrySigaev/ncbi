#ifndef NETCACHE_NC_DB_STAT__HPP
#define NETCACHE_NC_DB_STAT__HPP
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
 * Authors:  Pavel Ivanov
 */

#include <corelib/ncbitime.hpp>
#include <corelib/ncbimtx.hpp>

#include "nc_utils.hpp"

#include <vector>


BEGIN_NCBI_SCOPE


template <class T>
class CNCDBStatFigure
{
public:
    CNCDBStatFigure(void);

    void  AddValue      (T value);

    Uint8 GetCount  (void) const;
    T     GetSum    (void) const;
    T     GetMaximum(void) const;
    T     GetAverage(void) const;

private:
    T      m_ValuesSum;
    Uint8  m_ValuesCount;
    T      m_ValuesMax;
};


/// Object accumulating statistics for NetCache storage
class CNCDBStat
{
public:
    /// Constructor. It's assumed that different CNCDBStat objects are not
    /// constructed concurrently.
    CNCDBStat(void);

    /// Add measurement for number of database parts and difference between
    /// highest and lowest ids of database parts.
    void AddNumberOfDBParts  (size_t num_parts, Int8 ids_span);
    /// Add measurement for the single database part files sizes.
    void AddDBPartSizes      (Int8 meta_size, Int8 data_size);
    /// Add measurement for the total database size (separately for meta files
    /// part and for data files part).
    void AddTotalDBSize      (Int8 meta_size, Int8 data_size);
    /// Add incoming request for blob locking
    void AddLockRequest      (void);
    /// Add granted blob lock
    void AddLockAcquired     (double lock_waited_time);
    /// Add blob lock over not existing blob
    void AddNotExistBlobLock (void);
    /// Add incoming request for blob locking from GC
    void AddGCLockRequest    (void);
    /// Add granted blob lock for GC
    void AddGCLockAcquired   (void);
    /// Add time between blob lock request and release
    void AddTotalLockTime    (double add_time);
    /// Add read blob of given size
    void AddBlobRead         (size_t size);
    /// Add read blob chunk of given size
    void AddChunkRead        (size_t size, double add_time);
    /// Add blob reading that was aborted without reading till end
    void AddStoppedRead      (void);
    /// Add written blob of given size
    void AddBlobWritten      (size_t size);
    /// Add written blob chunk of given size
    void AddChunkWritten     (size_t size, double add_time);
    /// Add blob writing that was not finalized
    void AddStoppedWrite     (void);
    /// Add deleted blob
    void AddBlobDelete       (void);
    /// Add truncated blob
    void AddBlobTruncate     (void);
    /// Add collision of trying to create over already existing blob
    void AddCreateHitExisting(void);
    /// Add check for blob existence
    void AddExistenceCheck   (void);

    /// Print statistics to the given proxy object
    void Print(CPrintTextProxy& proxy);

public:
    // For internal use only

    /// Type of time metrics supported by statistics
    enum ETimeMetric {
        eDbChunkRead,   ///< Time spent reading blob data from database
        eDbChunkWrite,  ///< Time spent writing blob data to database
        eDbInfoRead,    ///< Time spent reading meta-info from database
        eDbInfoWrite,   ///< Time spent writing meta-info to database
        eDbOther        ///< Time spent in all other database operations
                        ///< (mostly select statements)
    };
    /// Add value for given time metric (value in seconds)
    void AddTimeMetric(ETimeMetric metric, double add_time);

private:
    enum {
        kMinSizeInChart = 32  ///< Minimum blob size that will be counted for
                              ///< the statistics chart showing distribution
                              ///< of blobs sizes. Should be a power of 2.
    };

    /// Get index of chart element for given size
    int x_GetSizeIndex(size_t size);
    /// Calculate percentage for given time out of total time spent in locks
    int x_CalcTimePercent(double time);


    /// Main locking mutex for object
    CFastMutex     m_ObjMutex;

    /// Number of database parts
    CNCDBStatFigure<Uint8>            m_NumOfDBParts;
    /// Difference between the highest and lowest database part ids
    CNCDBStatFigure<Int8>             m_DBPartsIdsSpan;
    /// Size of individual meta file in database part
    CNCDBStatFigure<Int8>             m_MetaFileSize;
    /// Size of individual data file in database part
    CNCDBStatFigure<Int8>             m_DataFileSize;
    /// Total size of all meta files in database
    CNCDBStatFigure<Int8>             m_TotalMetaSize;
    /// Total size of all data files in database
    CNCDBStatFigure<Int8>             m_TotalDataSize;
    /// Total size of database
    CNCDBStatFigure<Int8>             m_TotalDBSize;
    /// Number of requests to lock blob
    Uint8                             m_LockRequests;
    /// Number of blob locks acquired
    Uint8                             m_LocksAcquired;
    /// Number of locks acquired for not existing blob
    Uint8                             m_NotExistLocks;
    /// Number of requests to lock blob made by GC
    Uint8                             m_GCLockRequests;
    /// Number of blob locks acquired by GC
    Uint8                             m_GCLocksAcquired;
    /// Total time between blob lock request and release
    double                            m_LocksTotalTime;
    /// Time while blob lock was waited to acquire
    double                            m_LocksWaitedTime;
    /// Maximum size of blob operated at any moment by storage
    size_t                            m_MaxBlobSize;
    /// Maximum size of blob chunk operated at any moment by storage
    size_t                            m_MaxChunkSize;
    /// Number of blobs read from database
    Uint8                             m_ReadBlobs;
    /// Total size of data read from database (sum of sizes of all chunks)
    Uint8                             m_ReadSize;
    /// Number of blobs with reading aborted before blob is finished
    Uint8                             m_StoppedReads;
    /// Distribution of number of blobs read by their size interval
    vector<Uint8>                     m_ReadBySize;
    /// Time spent reading one piece of meta-information about blob
    CNCDBStatFigure<double>           m_InfoReadTime;
    /// Time spent reading one blob chunk
    CNCDBStatFigure<double>           m_ChunkReadTime;
    /// Distribution of time spent reading chunks by their size interval
    vector< CNCDBStatFigure<double> > m_ChunkRTimeBySize;
    /// Number of blobs written to database
    Uint8                             m_WrittenBlobs;
    /// Total size of data written to database (sum of sizes of all chunks)
    Uint8                             m_WrittenSize;
    /// Number of blobs with writing aborted before blob is finalized
    Uint8                             m_StoppedWrites;
    /// Distribution of number of blobs written by there size interval
    vector<Uint8>                     m_WrittenBySize;
    /// Time spent writing one piece of meta-information about blob
    CNCDBStatFigure<double>           m_InfoWriteTime;
    /// Time spent writing one blob chunk
    CNCDBStatFigure<double>           m_ChunkWriteTime;
    /// Distribution of time spent writing chunks by their size interval
    vector< CNCDBStatFigure<double> > m_ChunkWTimeBySize;
    /// Number of blobs deleted from database
    Uint8                             m_DeletedBlobs;
    /// Total time spent on all database operations
    double                            m_TotalDbTime;
    /// Number of blobs truncated down to lower size
    Uint8                             m_TruncatedBlobs;
    /// Number of create requests that have met already existing blob
    Uint8                             m_CreateExists;
    /// Number of checks for blob existence
    Uint8                             m_ExistChecks;
};


/// Object to measure time consumed for some operations worth remembering in
/// storage statistics. Timer automatically starts at object creation and
/// stops at object destruction. Time spent is automatically remembered
/// during object destruction.
class CNCStatTimer
{
public:
    /// Create timer for given object gathering statistics and for given time
    /// metric.
    CNCStatTimer(CNCDBStat* stat, CNCDBStat::ETimeMetric metric);
    ~CNCStatTimer(void);

    /// Set size of chunk read/written
    void SetChunkSize(size_t size);

private:
    /// Object gathering statistics
    CNCDBStat*             m_Stat;
    /// Metric to measure by this object
    CNCDBStat::ETimeMetric m_Metric;
    /// Chunk size in case if metrics is about reading/writing chunk
    size_t                 m_ChunkSize;
    /// Timer ticking
    CStopWatch             m_Timer;
};



template <class T>
inline
CNCDBStatFigure<T>::CNCDBStatFigure(void)
    : m_ValuesSum(0),
      m_ValuesCount(0),
      m_ValuesMax(0)
{}

template <class T>
inline void
CNCDBStatFigure<T>::AddValue(T value)
{
    m_ValuesSum += value;
    ++m_ValuesCount;
    m_ValuesMax = max(m_ValuesMax, value);
}

template <class T>
inline Uint8
CNCDBStatFigure<T>::GetCount(void) const
{
    return m_ValuesCount;
}

template <class T>
inline T
CNCDBStatFigure<T>::GetSum(void) const
{
    return m_ValuesSum;
}

template <class T>
inline T
CNCDBStatFigure<T>::GetMaximum(void) const
{
    return m_ValuesMax;
}

template <class T>
inline T
CNCDBStatFigure<T>::GetAverage(void) const
{
    return m_ValuesCount == 0? 0: T(m_ValuesSum / m_ValuesCount);
}



inline void
CNCDBStat::AddNumberOfDBParts(size_t num_parts, Int8 ids_span)
{
    CFastMutexGuard guard(m_ObjMutex);
    m_NumOfDBParts.AddValue(Uint8(num_parts));
    m_DBPartsIdsSpan.AddValue(ids_span);
}

inline void
CNCDBStat::AddDBPartSizes(Int8 meta_size, Int8 data_size)
{
    CFastMutexGuard guard(m_ObjMutex);
    m_MetaFileSize.AddValue(meta_size);
    m_DataFileSize.AddValue(data_size);
}

inline void
CNCDBStat::AddTotalDBSize(Int8 meta_size, Int8 data_size)
{
    CFastMutexGuard guard(m_ObjMutex);
    m_TotalMetaSize.AddValue(meta_size);
    m_TotalDataSize.AddValue(data_size);
    m_TotalDBSize.AddValue(meta_size + data_size);
}

inline void
CNCDBStat::AddLockRequest(void)
{
    CFastMutexGuard guard(m_ObjMutex);
    ++m_LockRequests;
}

inline void
CNCDBStat::AddLockAcquired(double lock_waited_time)
{
    CFastMutexGuard guard(m_ObjMutex);
    ++m_LocksAcquired;
    m_LocksWaitedTime += lock_waited_time;
}

inline void
CNCDBStat::AddNotExistBlobLock(void)
{
    CFastMutexGuard guard(m_ObjMutex);
    ++m_NotExistLocks;
}

inline void
CNCDBStat::AddGCLockRequest(void)
{
    CFastMutexGuard guard(m_ObjMutex);
    ++m_GCLockRequests;
}

inline void
CNCDBStat::AddGCLockAcquired(void)
{
    CFastMutexGuard guard(m_ObjMutex);
    ++m_GCLocksAcquired;
}

inline void
CNCDBStat::AddTotalLockTime(double add_time)
{
    CFastMutexGuard guard(m_ObjMutex);
    m_LocksTotalTime += add_time;
}

inline void
CNCDBStat::AddBlobRead(size_t size)
{
    CFastMutexGuard guard(m_ObjMutex);
    ++m_ReadBlobs;
    ++m_ReadBySize[x_GetSizeIndex(size)];
    m_MaxBlobSize = max(m_MaxBlobSize, size);
}

inline void
CNCDBStat::AddChunkRead(size_t size, double add_time)
{
    CFastMutexGuard guard(m_ObjMutex);
    m_ReadSize += size;
    m_ChunkReadTime.AddValue(add_time);
    m_ChunkRTimeBySize[x_GetSizeIndex(size)].AddValue(add_time);
    m_MaxChunkSize = max(m_MaxChunkSize, size);
}

inline void
CNCDBStat::AddStoppedRead(void)
{
    CFastMutexGuard guard(m_ObjMutex);
    ++m_ReadBlobs;
    ++m_StoppedReads;
}

inline void
CNCDBStat::AddBlobWritten(size_t size)
{
    CFastMutexGuard guard(m_ObjMutex);
    ++m_WrittenBlobs;
    ++m_WrittenBySize[x_GetSizeIndex(size)];
    m_MaxBlobSize = max(m_MaxBlobSize, size);
}

inline void
CNCDBStat::AddChunkWritten(size_t size, double add_time)
{
    CFastMutexGuard guard(m_ObjMutex);
    m_WrittenSize += size;
    m_ChunkWriteTime.AddValue(add_time);
    m_ChunkWTimeBySize[x_GetSizeIndex(size)].AddValue(add_time);
    m_MaxChunkSize = max(m_MaxChunkSize, size);
}

inline void
CNCDBStat::AddStoppedWrite(void)
{
    CFastMutexGuard guard(m_ObjMutex);
    ++m_WrittenBlobs;
    ++m_StoppedWrites;
}

inline void
CNCDBStat::AddBlobDelete(void)
{
    CFastMutexGuard guard(m_ObjMutex);
    ++m_DeletedBlobs;
}

inline void
CNCDBStat::AddBlobTruncate(void)
{
    CFastMutexGuard guard(m_ObjMutex);
    ++m_TruncatedBlobs;
}

inline void
CNCDBStat::AddCreateHitExisting(void)
{
    CFastMutexGuard guard(m_ObjMutex);
    ++m_CreateExists;
}

inline void
CNCDBStat::AddExistenceCheck(void)
{
    CFastMutexGuard guard(m_ObjMutex);
    ++m_ExistChecks;
}

inline void
CNCDBStat::AddTimeMetric(ETimeMetric metric, double add_time)
{
    CFastMutexGuard guard(m_ObjMutex);
    switch (metric) {
    case eDbInfoRead:
        m_InfoReadTime.AddValue(add_time);
        m_TotalDbTime += add_time;
        break;
    case eDbInfoWrite:
        m_InfoWriteTime.AddValue(add_time);
        m_TotalDbTime += add_time;
        break;
    case eDbChunkRead:
    case eDbChunkWrite:
        // Here we will be if reading/writing of chunk has failed
    case eDbOther:
        m_TotalDbTime += add_time;
        break;
    default:
        break;
    }
}



inline
CNCStatTimer::CNCStatTimer(CNCDBStat*             stat,
                           CNCDBStat::ETimeMetric metric)
    : m_Stat(stat),
      m_Metric(metric),
      m_ChunkSize(0),
      m_Timer(CStopWatch::eStart)
{
    _ASSERT(stat);
}

inline
CNCStatTimer::~CNCStatTimer(void)
{
    if (m_ChunkSize != 0) {
        if (m_Metric == CNCDBStat::eDbChunkRead)
            m_Stat->AddChunkRead(m_ChunkSize, m_Timer.Elapsed());
        else
            m_Stat->AddChunkWritten(m_ChunkSize, m_Timer.Elapsed());
    }
    else {
        m_Stat->AddTimeMetric(m_Metric, m_Timer.Elapsed());
    }
}

inline void
CNCStatTimer::SetChunkSize(size_t size)
{
    _ASSERT(m_Metric == CNCDBStat::eDbChunkWrite
            ||  m_Metric == CNCDBStat::eDbChunkRead);

    m_ChunkSize = size;
}

END_NCBI_SCOPE

#endif /* NETCACHE_NC_DB_STAT__HPP */
