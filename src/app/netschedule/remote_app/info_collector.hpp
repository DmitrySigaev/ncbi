#ifndef __NETSCHEDULE_INFO_COLLECTOR__HPP
#define __NETSCHEDULE_INFO_COLLECTOR__HPP

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
 * Authors: Maxim Didenko
 *
 * File Description:
 *
 */

#include <corelib/ncbimisc.hpp>
#include <corelib/blob_storage.hpp>

#include <connect/services/netschedule_client.hpp>
#include <connect/services/remote_job.hpp>

BEGIN_NCBI_SCOPE


/// Proxy class to open ShutdownServer
///
/// @internal
class CNSClientHelper : public CNetScheduleClient 
{
public:
    CNSClientHelper(const string&  host,
                    unsigned short port,
                    const string&  queue)
      : CNetScheduleClient(host, port, "netschedule_admin", queue)
    {}

    using CNetScheduleClient::ShutdownServer;
    using CNetScheduleClient::DropQueue;
    using CNetScheduleClient::PrintStatistics;
    using CNetScheduleClient::Monitor;
    using CNetScheduleClient::DumpQueue;
    using CNetScheduleClient::PrintQueue;
    using CNetScheduleClient::Logging;
    using CNetScheduleClient::ReloadServerConfig;
    using CNetScheduleClient::GetQueueList;
    // using CNetScheduleClient::CheckConnect;
    virtual bool CheckConnect(const string& key)
    {
        return CNetScheduleClient::CheckConnect(key);
    }
};

class CNSInfoCollector;

class CNSJobInfo
{
public:
    ~CNSJobInfo();

    CNetScheduleClient::EJobStatus GetStatus() const;
    const string& GetId() const { return m_Id; }
    const string& GetRawInput() const;
    const string& GetRawOutput() const;
    int GetRetCode() const;

    CNcbiIstream& GetStdIn() const;
    CNcbiIstream& GetStdOut() const;
    CNcbiIstream& GetStdErr() const;
    const string& GetCmdLine() const;
    
private:
    friend class CNSInfoCollector;
    CNSJobInfo(const string& id, CNSInfoCollector& collector);

    void x_Load();
    void x_Load() const;

    CNcbiIstream*               x_CreateStream(const string& blob_or_data) const;
    CRemoteAppRequest_Executer& x_GetRequest() const;
    CRemoteAppResult&           x_GetResult() const;

    string m_Id;
    CNetScheduleClient::EJobStatus m_Status;
    string m_Input;
    string m_Output;
    int m_RetCode;

    CNSInfoCollector& m_Collector;
    mutable CRemoteAppRequest_Executer* m_Request;
    mutable CRemoteAppResult* m_Result;

    CNSJobInfo(const CNSJobInfo&);
    CNSJobInfo& operator=(const CNSJobInfo&);
};

class CNSInfoCollector
{
public:
    CNSInfoCollector(const string& queue, const string& service_name,
                     CBlobStorageFactory& factory);
    CNSInfoCollector(const string& queue, const string& host, unsigned short port,
                     CBlobStorageFactory& factory);

    class IJobAction {
    public:
        virtual ~IJobAction() {}
        virtual void operator()(const CNSJobInfo& info) = 0;
    };
    void TraverseJobs(CNetScheduleClient::EJobStatus, IJobAction&);

    CNSJobInfo* CreateJobInfo(const string& job_id);

    CNcbiIstream& GetBlobContent(const string& blob_id, size_t* blob_size = NULL);
    
private:

    friend class CNSJobInfo;

    typedef pair<string, unsigned short> TSrvID;
    typedef map<TSrvID, AutoPtr<CNSClientHelper> > TServices;

    TServices m_Services;
    CNSClientHelper& x_GetClient(const string& job_id);
    CNSClientHelper& x_GetClient(const string& host, unsigned short port);
    CRemoteAppRequest_Executer& x_GetRequest();
    CRemoteAppResult& x_GetResult();

    IBlobStorage* x_CreateStorage() { return m_Factory.CreateInstance(); }

    string m_QueueName;
    CBlobStorageFactory& m_Factory;

    auto_ptr<IBlobStorage> m_Storage;
    auto_ptr<CRemoteAppRequest_Executer> m_Request;
    auto_ptr<CRemoteAppResult> m_Result;
private:
    
    CNSInfoCollector(const CNSInfoCollector&);
    CNSInfoCollector& operator=(const CNSInfoCollector&);
};

END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.1  2006/05/19 13:40:40  didenko
 * Added ns_remote_job_control utility
 *
 * ===========================================================================
 */

#endif 

