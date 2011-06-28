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

#include <ncbi_pch.hpp>

#include "ns_server.hpp"
#include "queue_coll.hpp"


USING_NCBI_SCOPE;


CNetScheduleServer* CNetScheduleServer::sm_netschedule_server = 0;

//////////////////////////////////////////////////////////////////////////
/// NetScheduler threaded server implementation
CNetScheduleServer::CNetScheduleServer()
    : m_BackgroundHost(this),
      m_RequestExecutor(this),
      m_Port(0),
      m_HostNetAddr(0),
      m_Shutdown(false),
      m_SigNum(0),
      m_InactivityTimeout(0),
      m_QueueDB(0),
      m_StartTime(GetFastLocalTime()),
      m_LogFlag(false),
      m_LogBatchEachJobFlag(false),
      m_LogNotificationThreadFlag(false),
      m_LogCleaningThreadFlag(false),
      m_LogStatisticsThreadFlag(false)
{
    m_AtomicCommandNumber.Set(1);
    sm_netschedule_server = this;
}


CNetScheduleServer::~CNetScheduleServer()
{
    delete m_QueueDB;
}


void CNetScheduleServer::AddDefaultListener(IServer_ConnectionFactory* factory)
{
    // port must be set before listening
    _ASSERT(m_Port);
    AddListener(factory, m_Port);
}


void CNetScheduleServer::SetNSParameters(const SNS_Parameters& params)
{
    CServer::SetParameters(params);
    m_Port = params.port;
    m_HostNetAddr = CSocketAPI::gethostbyname(kEmptyStr);
    if (params.use_hostname) {
        m_Host = CSocketAPI::gethostname();
    } else {
        NS_FormatIPAddress(m_HostNetAddr, m_Host);
    }

    m_InactivityTimeout = params.network_timeout;
    m_LogFlag           = params.is_log;

    if (m_LogFlag) {
        m_LogBatchEachJobFlag           = params.log_batch_each_job;
        m_LogNotificationThreadFlag     = params.log_notification_thread;
        m_LogCleaningThreadFlag         = params.log_cleaning_thread;
        m_LogExecutionWatcherThreadFlag = params.log_execution_watcher_thread;
        m_LogStatisticsThreadFlag       = params.log_statistics_thread;
    }
    else {
        // [server]/log key is a top level logging key
        m_LogBatchEachJobFlag           = false;
        m_LogNotificationThreadFlag     = false;
        m_LogCleaningThreadFlag         = false;
        m_LogExecutionWatcherThreadFlag = false;
        m_LogStatisticsThreadFlag       = false;
    }

    m_AdminHosts.SetHosts(params.admin_hosts);
}


bool CNetScheduleServer::ShutdownRequested(void)
{
    return m_Shutdown;
}


void CNetScheduleServer::SetQueueDB(CQueueDataBase* qdb)
{
    delete m_QueueDB;
    m_QueueDB = qdb;
}


void CNetScheduleServer::SetShutdownFlag(int signum)
{
    if (!m_Shutdown) {
        m_Shutdown = true;
        m_SigNum = signum;
    }
}


unsigned CNetScheduleServer::GetCommandNumber()
{
    return m_AtomicCommandNumber.Add(1);
}


// Queue handling
unsigned CNetScheduleServer::Configure(const IRegistry& reg)
{
    return m_QueueDB->Configure(reg);
}


unsigned CNetScheduleServer::CountActiveJobs() const
{
    return m_QueueDB->CountActiveJobs();
}


CRef<CQueue> CNetScheduleServer::OpenQueue(const std::string& name)
{
    return m_QueueDB->OpenQueue(name);
}


void CNetScheduleServer::CreateQueue(const std::string& qname,
                                     const std::string& qclass,
                                     const std::string& comment)
{
    m_QueueDB->CreateQueue(qname, qclass, comment);
}


void CNetScheduleServer::DeleteQueue(const std::string& qname)
{
    m_QueueDB->DeleteQueue(qname);
}


void CNetScheduleServer::QueueInfo(const std::string& qname,
                                   int&               kind,
                                   std::string*       qclass,
                                   std::string*       comment)
{
    m_QueueDB->QueueInfo(qname, kind, qclass, comment);
}


std::string CNetScheduleServer::GetQueueNames(const std::string& sep) const
{
    return m_QueueDB->GetQueueNames(sep);
}


void CNetScheduleServer::PrintMutexStat(CNcbiOstream& out)
{
    m_QueueDB->PrintMutexStat(out);
}


void CNetScheduleServer::PrintLockStat(CNcbiOstream& out)
{
    m_QueueDB->PrintLockStat(out);
}


void CNetScheduleServer::PrintMemStat(CNcbiOstream& out)
{
    m_QueueDB->PrintMemStat(out);
}


unsigned CNetScheduleServer::GetInactivityTimeout(void)
{
    return m_InactivityTimeout;
}


std::string& CNetScheduleServer::GetHost()
{
    return m_Host;
}


unsigned CNetScheduleServer::GetPort() const
{
    return m_Port;
}


unsigned CNetScheduleServer::GetHostNetAddr() const
{
    return m_HostNetAddr;
}


const CTime& CNetScheduleServer::GetStartTime(void) const
{
    return m_StartTime;
}


bool CNetScheduleServer::AdminHostValid(unsigned host) const
{
    return !m_AdminHosts.IsRestrictionSet() ||
            m_AdminHosts.IsAllowed(host);
}


CBackgroundHost& CNetScheduleServer::GetBackgroundHost()
{
    return m_BackgroundHost;
}


CRequestExecutor& CNetScheduleServer::GetRequestExecutor()
{
    return m_RequestExecutor;
}


CNetScheduleServer*  CNetScheduleServer::GetInstance(void)
{
    return sm_netschedule_server;
}


void CNetScheduleServer::Exit()
{
    m_QueueDB->Close();
}

