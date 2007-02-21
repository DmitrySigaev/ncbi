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

#include <connect/services/netschedule_api.hpp>
#include <connect/services/remote_job.hpp>

BEGIN_NCBI_SCOPE

/////////////////////////////////////////////////////////////////////
///
class CNSInfoCollector;

class CNSJobInfo
{
public:
    CNSJobInfo(const string& id, CNSInfoCollector& collector);

    ~CNSJobInfo();

    CNetScheduleAPI::EJobStatus GetStatus() const;
    const string& GetId() const { return m_Job.job_id; }
    const string& GetRawInput() const;
    const string& GetRawOutput() const;
    const string& GetErrMsg() const;
    int GetRetCode() const;

    CNcbiIstream& GetStdIn() const;
    CNcbiIstream& GetStdOut() const;
    CNcbiIstream& GetStdErr() const;
    const string& GetCmdLine() const;
   
private:

    void x_Load();
    void x_Load() const;

    CNcbiIstream*               x_CreateStream(const string& blob_or_data) const;
    CRemoteAppRequest_Executer& x_GetRequest() const;
    CRemoteAppResult&           x_GetResult() const;

    CNetScheduleJob m_Job;
    CNetScheduleAPI::EJobStatus m_Status;

    CNSInfoCollector& m_Collector;
    mutable CRemoteAppRequest_Executer* m_Request;
    mutable CRemoteAppResult* m_Result;

    CNSJobInfo(const CNSJobInfo&);
    CNSJobInfo& operator=(const CNSJobInfo&);
};

//////////////////////////////////////////////////////////////////////
///
class CWNodeInfo
{
public:
    CWNodeInfo(const string& name, const string& prog,
               const string& host, unsigned short port,
               const CTime& last_access);
    ~CWNodeInfo();
    
    const string& GetName() const { return m_Host; }
    const string& GetProgName() const { return m_Prog; }
    const string& GetHost() const { return m_Host; }
    unsigned short GetPort() const { return m_Port; }
    const CTime& GetLastAccess() const { return m_LastAccess; }

    void Shutdown(CNetScheduleAdmin::EShutdownLevel level) const;

private:
    
    string m_Name;
    string m_Prog;
    string m_Host;
    unsigned short m_Port;
    CTime m_LastAccess;
        
};

//////////////////////////////////////////////////////////////////////
///
class CNSInfoCollector
{
public:
    CNSInfoCollector(const string& queue, const string& service_name,
                     CBlobStorageFactory& factory);

    template<typename TInfo>
    class IAction {
    public:
        virtual ~IAction() {}
        virtual void operator()(const TInfo& info) = 0;
    };

    typedef map<pair<string,unsigned int>, list<string> > TQueueCont;

    void TraverseJobs(CNetScheduleAPI::EJobStatus, IAction<CNSJobInfo>&);
    void TraverseNodes(IAction<CWNodeInfo>&);
    void DropQueue();

    void GetQueues(TQueueCont& queues);

    CNSJobInfo* CreateJobInfo(const string& job_id);

    CNcbiIstream& GetBlobContent(const string& blob_id, size_t* blob_size = NULL);
    
private:

    friend class CNSJobInfo;

    auto_ptr<CNetScheduleAPI> m_Services;
    CNetScheduleAPI& x_GetAPI() { return *m_Services; }

    CRemoteAppRequest_Executer& x_GetRequest();
    CRemoteAppResult& x_GetResult();

    IBlobStorage* x_CreateStorage() { return m_Factory.CreateInstance(); }

    CBlobStorageFactory& m_Factory;

    auto_ptr<IBlobStorage> m_Storage;
    auto_ptr<CRemoteAppRequest_Executer> m_Request;
    auto_ptr<CRemoteAppResult> m_Result;
private:
    
    CNSInfoCollector(const CNSInfoCollector&);
    CNSInfoCollector& operator=(const CNSInfoCollector&);
};

END_NCBI_SCOPE

#endif 

