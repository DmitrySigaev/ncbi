#ifndef NETSCHEDULE_SERVICE_THREAD__HPP
#define NETSCHEDULE_SERVICE_THREAD__HPP

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
 * Authors:  Denis Vakatov (design), Sergey Satskiy (implementation)
 *
 * File Description: NetSchedule service thread
 *
 *
 */

#include <util/thread_nonstop.hpp>
#include "background_host.hpp"
#include <sys/prctl.h>

BEGIN_NCBI_SCOPE

class CQueueDataBase;
class CNetScheduleServer;


// Thread class, prints the statistics counters
// and makes decision about shutdown in case if draining was requested
class CServiceThread : public CThreadNonStop
{
public:
    CServiceThread(CNetScheduleServer &  server,
                   CBackgroundHost &     host,
                   CQueueDataBase &      qdb,
                   const bool &          logging,
                   const unsigned int &  statistics_interval)
    : CThreadNonStop(1),    // Once in 1 seconds
      m_Server(server),
      m_Host(host),
      m_QueueDB(qdb),
      m_StatisticsLogging(logging),
      m_StatisticsInterval(statistics_interval),
      m_LastStatisticsOutput(0),
      m_LastDrainCheck(0),
      m_LastConfigFileCheck(0)
    {}

    virtual void DoJob(void);
    virtual void *  Main(void)
    {
        prctl(PR_SET_NAME, "netscheduled_st", 0, 0, 0);
        return CThreadNonStop::Main();
    }

private:
    void  x_CheckDrainShutdown(void);
    void  x_CheckConfigFile(void);

private:
    CServiceThread(const CServiceThread &);
    CServiceThread &  operator=(const CServiceThread &);

private:
    CNetScheduleServer &    m_Server;
    CBackgroundHost &       m_Host;
    CQueueDataBase &        m_QueueDB;
    const bool &            m_StatisticsLogging;
    const unsigned int &    m_StatisticsInterval;
    time_t                  m_LastStatisticsOutput;
    time_t                  m_LastDrainCheck;
    time_t                  m_LastConfigFileCheck;
};


END_NCBI_SCOPE

#endif /* NETSCHEDULE_SERVICE_THREAD__HPP */

