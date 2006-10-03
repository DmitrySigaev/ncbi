#ifndef NS_QUEUE_CLEAN_THREAD__HPP
#define NS_QUEUE_CLEAN_THREAD__HPP

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
 * Authors:  Anatoliy Kuznetsov
 *
 * File Description: Queue cleaning thread.
 *                   
 *
 */

#include <util/thread_nonstop.hpp>

BEGIN_NCBI_SCOPE

class CQueueDataBase;

/// Thread class, removes obsolete job records
///
/// @internal
class CJobQueueCleanerThread : public CThreadNonStop
{
public:
    CJobQueueCleanerThread(CQueueDataBase& qdb,
                           unsigned run_delay)
    : CThreadNonStop(run_delay),
      m_QueueDB(qdb)
    {}

    virtual void DoJob(void);
private:
    CJobQueueCleanerThread(const CJobQueueCleanerThread&);
    CJobQueueCleanerThread& operator=(const CJobQueueCleanerThread&);
private:
    CQueueDataBase&  m_QueueDB;
};


/// Thread class, watches job execution, reschedules forgotten jobs
///
/// @internal
class CJobQueueExecutionWatcherThread : public CThreadNonStop
{
public:
    CJobQueueExecutionWatcherThread(CQueueDataBase& qdb,
                                    unsigned run_delay)
    : CThreadNonStop(run_delay),
      m_QueueDB(qdb)
    {}

    virtual void DoJob(void);
private:
    CJobQueueExecutionWatcherThread(const CJobQueueExecutionWatcherThread&);
    CJobQueueExecutionWatcherThread& 
        operator=(const CJobQueueExecutionWatcherThread&);
private:
    CQueueDataBase&  m_QueueDB;
};


END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.4  2006/10/03 14:56:57  joukovv
 * Delayed job deletion implemented, code restructured preparing to move to
 * thread-per-request model.
 *
 * Revision 1.3  2005/03/09 17:37:17  kuznets
 * Added node notification thread and execution control timeline
 *
 * Revision 1.2  2005/02/23 19:18:18  kuznets
 * New line at the file end (GCC)
 *
 * Revision 1.1  2005/02/23 19:15:30  kuznets
 * Initial revision
 *
 *
 * ===========================================================================
 */

#endif
