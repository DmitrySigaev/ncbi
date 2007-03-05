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
 *   Government have not placed any restriction on its use or reproduction.
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
 * Authors:  Maxim Didenko
 *
 * File Description:
 *    NetSchedule Worker Node implementation
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbistre.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbi_process.hpp>

#include <connect/services/grid_globals.hpp>
#include <connect/services/grid_control_thread.hpp>
#include <connect/services/grid_worker_app_impl.hpp>

#include <math.h>

BEGIN_NCBI_SCOPE

/////////////////////////////////////////////////////////////////////////////
// 
///@internal

class CGetVersionProcessor : public CWorkerNodeControlThread::IRequestProcessor 
{
public:
    virtual ~CGetVersionProcessor() {}

    virtual void Process(const string& request,
                         CNcbiOstream& os,
                         CGridWorkerNode& node)
    {
        os << "OK:" << node.GetJobVersion() << WN_BUILD_DATE;
    }
};

class CShutdownProcessor : public CWorkerNodeControlThread::IRequestProcessor 
{
public:
    virtual ~CShutdownProcessor() {}

    virtual bool Authenticate(const string& host,
                              const string& auth, 
                              const string& queue,
                              CNcbiOstream& os,
                              const CGridWorkerNode& node) 
    {
        m_Host = host;
        size_t pos = m_Host.find_first_of(':');
        if (pos != string::npos) {
            m_Host = m_Host.substr(0, pos);
        }
        if (node.IsHostInAdminHostsList(m_Host)) {
            return true;
        }
        os << "ERR:Shutdown access denied.";
        LOG_POST(Warning << CTime(CTime::eCurrent).AsString() 
                 << " Shutdown access denied: " << m_Host);
        return false;
    }

    virtual void Process(const string& request,
                         CNcbiOstream& os,
                         CGridWorkerNode& )
    {
        if (request.find("SUICIDE") != NPOS) {
            LOG_POST(Warning << CTime(CTime::eCurrent).AsString() 
                     << " DIE request has been received from host: " << m_Host);
            LOG_POST(Warning << CTime(CTime::eCurrent).AsString() 
                     << " SERVER IS COMMITTING SUICIDE!!");
            CGridGlobals::GetInstance().KillNode();
        } else {
            CNetScheduleAdmin::EShutdownLevel level =
                CNetScheduleAdmin::eNormalShutdown;
            if (request.find("IMMEDIATE") != NPOS) 
                level = CNetScheduleAdmin::eShutdownImmidiate;
            os << "OK:";
            CGridGlobals::GetInstance().
                RequestShutdown(level);
            LOG_POST(CTime(CTime::eCurrent).AsString() 
                     << " Shutdown request has been received from host: " << m_Host);
        }
    }
private:
    string m_Host;
};

class CGetStatisticsProcessor : public CWorkerNodeControlThread::IRequestProcessor 
{
public:
    CGetStatisticsProcessor() {}
    virtual ~CGetStatisticsProcessor() {}

    virtual void Process(const string& request,
                         CNcbiOstream& os,
                         CGridWorkerNode& node)
    {       
        os << "OK:" << node.GetJobVersion() << WN_BUILD_DATE << endl;
        CNcbiApplication* app = CNcbiApplication::Instance();
        if (app)
            os << "Executable path: " << app->GetProgramExecutablePath() << endl;

        os << "Queue name: " << node.GetQueueName() << endl;
        if (node.GetMaxThreads() > 1)
            os << "Maximum job threads: " << node.GetMaxThreads() << endl;

        if (CGridGlobals::GetInstance().
            GetShutdownLevel() != CNetScheduleAdmin::eNoShutdown) {
                os << "THE NODE IS IN A SHUTTING DOWN MODE!!!" << endl;
        }
        if (CGridGlobals::GetInstance().IsExclusiveMode())
            os << "THE NODE IS IN AN EXCLUSIVE MODE!!!" << endl;

        CGridGlobals::GetInstance().GetJobsWatcher().Print(os);
        if (node.IsOnHold()) {
                os << "THE NODE IDLE TASK IS RUNNING..." << endl;
        }

    }
};

class CGetLoadProcessor : public CWorkerNodeControlThread::IRequestProcessor 
{
public:
    CGetLoadProcessor()  {}
    virtual ~CGetLoadProcessor() {}
   
    virtual bool Authenticate(const string& host,
                              const string& auth, 
                              const string& queue,
                              CNcbiOstream& os,
                              const CGridWorkerNode& node) 
    {
        if (auth != node.GetJobVersion()) {
            os <<"ERR:Wrong Program. Required: " << node.GetJobVersion();
            return false;
        } 
        string qname, connection_info;
        NStr::SplitInTwo(queue, ";", qname, connection_info);
        if (qname != node.GetQueueName()) {
            os << "ERR:Wrong Queue. Required: " << node.GetQueueName();
            return false;
        } 
        if (connection_info != node.GetServiceName()) {
            os << "ERR:Wrong Connection Info. Required: "                     
               << node.GetServiceName();
            return false;
        }
        return true;
    }

    virtual void Process(const string& request,
                         CNcbiOstream& os,
                         CGridWorkerNode& node)
    {
        int load = 0;
        if (!node.IsOnHold())
            load = node.GetMaxThreads() 
                - CGridGlobals::GetInstance().GetJobsWatcher().GetJobsRunningNumber();
        os << "OK:" << load;
    }
};

class CUnknownProcessor : public CWorkerNodeControlThread::IRequestProcessor 
{
public:
    virtual ~CUnknownProcessor() {}

    virtual void Process(const string& request,
                         CNcbiOstream& os,
                         CGridWorkerNode& node)
    {
        os << "ERR:Unknown command -- " << request;
    }
};

const string SHUTDOWN_CMD = "SHUTDOWN";
const string VERSION_CMD = "VERSION";
const string STAT_CMD = "STAT";
const string GETLOAD_CMD = "GETLOAD";

/////////////////////////////////////////////////////////////////////////////
// 
///@internal
CWorkerNodeControlThread::CWorkerNodeControlThread(unsigned int port, 
                                                   CGridWorkerNode& worker_node)
    : CThreadedServer(port), m_WorkerNode(worker_node), m_ShutdownRequested(false)
      //      m_ShutdownRequested(false)
{
    m_InitThreads = 1;
    m_MaxThreads = 3;
    m_ThrdSrvAcceptTimeout.sec = 1;
    m_ThrdSrvAcceptTimeout.usec = 0;
    m_AcceptTimeout = &m_ThrdSrvAcceptTimeout;

    m_Processors[VERSION_CMD] = new CGetVersionProcessor;
    m_Processors[SHUTDOWN_CMD] = new CShutdownProcessor;
    m_Processors[STAT_CMD] = new CGetStatisticsProcessor;
    m_Processors[GETLOAD_CMD] = new CGetLoadProcessor;
}

CWorkerNodeControlThread::~CWorkerNodeControlThread()
{
    LOG_POST(CTime(CTime::eCurrent).AsString() << " Control server stopped.");
}

#define JS_CHECK_IO_STATUS(x) \
        switch (x)  { \
        case eIO_Success: \
            break; \
        default: \
            return; \
        } 

void CWorkerNodeControlThread::Process(SOCK sock)
{
    EIO_Status io_st;
    CSocket socket;
    try {
        socket.Reset(sock, eTakeOwnership, eCopyTimeoutsFromSOCK);
        socket.DisableOSSendDelay();
        
        string auth;
        io_st = socket.ReadLine(auth);
        JS_CHECK_IO_STATUS(io_st);
        
        string queue;
        io_st = socket.ReadLine(queue);
        JS_CHECK_IO_STATUS(io_st);

        string request;
        io_st = socket.ReadLine(request);
        JS_CHECK_IO_STATUS(io_st);

        string host = socket.GetPeerAddress();
        
        CNcbiOstrstream os;        
        TProcessorCont::iterator it;
        for (it = m_Processors.begin(); it != m_Processors.end(); ++it) {
            if (NStr::StartsWith(request, it->first)) {
                IRequestProcessor& processor = *(it->second);
                if (processor.Authenticate(host, auth, queue, os, m_WorkerNode))
                    processor.Process(request, os, m_WorkerNode);
                break;
            }
        }
        if (it == m_Processors.end()) {
            CUnknownProcessor processor;
            processor.Process(request, os, m_WorkerNode);
        }
            
        os << ends;
        try {
            socket.Write(os.str(), os.pcount());
        }  catch (...) {
            os.freeze(false);
            throw;
        }
        os.freeze(false);

    }
    catch (exception& ex)
    {
        ERR_POST(CTime(CTime::eCurrent).AsString() 
                 << " Exception in the control server : " << ex.what());
        string err = "ERR:" + NStr::PrintableString(ex.what());
        socket.Write(err.c_str(), err.length() + 1 );     
    }
}

bool CWorkerNodeControlThread::ShutdownRequested(void) 
{
    //    return CGridGlobals::GetInstance().
    //        GetShutdownLevel() != CNetScheduleClient::eNoShutdown;
    return m_ShutdownRequested;
}

void CWorkerNodeControlThread::ProcessTimeout(void)
{
    CGridGlobals::GetInstance().GetJobsWatcher().CheckInfinitLoop();
}

END_NCBI_SCOPE
