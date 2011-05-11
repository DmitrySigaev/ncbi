#ifndef NETSCHEDULE_SERVER__HPP
#define NETSCHEDULE_SERVER__HPP

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
 * Authors:  Anatoliy Kuznetsov, Victor Joukov
 *
 * File Description: NetScheduler threaded server
 *
 */

#include <string>
#include <connect/server.hpp>

#include "ns_server_misc.hpp"
#include "ns_server_params.hpp"
#include "ns_queue.hpp"

BEGIN_NCBI_SCOPE

class CQueueDataBase;


//////////////////////////////////////////////////////////////////////////
/// NetScheduler threaded server
///
/// @internal
class CNetScheduleServer : public CServer
{
public:
    CNetScheduleServer();
    virtual ~CNetScheduleServer();

    void AddDefaultListener(IServer_ConnectionFactory* factory);
    void SetNSParameters(const SNS_Parameters& new_params);

    virtual bool ShutdownRequested(void);

    void SetQueueDB(CQueueDataBase* qdb);

    void SetShutdownFlag(int signum = 0);

    //////////////////////////////////////////////////////////////////
    /// Service for Handlers
    /// TRUE if logging is ON
    bool IsLog() const;
    void SetLogging(bool flag);
    unsigned GetCommandNumber();

    // Queue handling
    unsigned Configure(const IRegistry& reg);
    unsigned CountActiveJobs() const;
    CRef<CQueue> OpenQueue(const std::string& name);
    void CreateQueue(const std::string& qname,
                     const std::string& qclass,
                     const std::string& comment);
    void DeleteQueue(const std::string& qname);
    void QueueInfo(const std::string& qname,
                   int&               kind,
                   std::string*       qclass,
                   std::string*       comment);
    std::string GetQueueNames(const std::string& sep) const;
    void PrintMutexStat(CNcbiOstream& out);
    void PrintLockStat(CNcbiOstream& out);
    void PrintMemStat(CNcbiOstream& out);


    unsigned GetInactivityTimeout(void);
    std::string& GetHost();
    unsigned GetPort() const;
    unsigned GetHostNetAddr() const;
    const CTime& GetStartTime(void) const;
    bool AdminHostValid(unsigned host) const;

    CBackgroundHost&  GetBackgroundHost();
    CRequestExecutor& GetRequestExecutor();

    static CNetScheduleServer*  GetInstance(void);

protected:
    virtual void Exit();

private:
    /// API for background threads
    CNetScheduleBackgroundHost                  m_BackgroundHost;
    CNetScheduleRequestExecutor                 m_RequestExecutor;
    /// Host name where server runs
    std::string                                 m_Host;
    unsigned                                    m_Port;
    unsigned                                    m_HostNetAddr;
    bool                                        m_Shutdown;
    int                                         m_SigNum;  ///< Shutdown signal number
    /// Time to wait for the client (seconds)
    unsigned                                    m_InactivityTimeout;
    CQueueDataBase*                             m_QueueDB;
    CTime                                       m_StartTime;
    CAtomicCounter                              m_LogFlag;
    /// Quick local timer
    CFastLocalTime                              m_LocalTimer;
    /// List of admin stations
    CNetSchedule_AccessList                     m_AdminHosts;

    CAtomicCounter                              m_AtomicCommandNumber;

    static CNetScheduleServer*                  sm_netschedule_server;
};


END_NCBI_SCOPE

#endif

