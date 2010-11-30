#ifndef CONNECT_SERVICES_GRID__GRID_CLIENT__HPP
#define CONNECT_SERVICES_GRID__GRID_CLIENT__HPP

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
 * Authors:  Maxim Didenko, Dmitry Kazimirov
 *
 */

/// @file grid_client.hpp
/// NetSchedule Framework specs.
///


#include <connect/services/netschedule_api.hpp>
#include <connect/services/netcache_api.hpp>

#include <connect/connect_export.h>

#include <corelib/ncbistre.hpp>


BEGIN_NCBI_SCOPE

/** @addtogroup NetScheduleClient
 *
 * @{
 */


class CGridClient;

/// Grid Job Submitter
///
class NCBI_XCONNECT_EXPORT CGridJobSubmitter
{
public:

    ~CGridJobSubmitter();

    /// Set a job's input This string will be sent to
    /// then the job is submitted.
    ///
    /// This method can be used to send a short data to the worker node.
    /// To send a large data use GetOStream method. Don't call this
    /// method after GetOStream method is called.
    ///
    void SetJobInput(const string& input);

    /// Get a stream where a client can write an input
    /// data for the remote job
    ///
    CNcbiOstream& GetOStream();

    /// Set a job mask
    ///
    void SetJobMask(CNetScheduleAPI::TJobMask mask);

    /// Set job tags
    ///
    void SetJobTags(const CNetScheduleAPI::TJobTags& tags);

    /// Set a job affinity
    ///
    void SetJobAffinity(const string& affinity);

    /// Submit a job to the queue
    ///
    /// @return a job key
    string Submit(const string& affinity = "");

private:
    /// Only CGridClient can create an instance of this class
    friend class CGridClient;
    CGridJobSubmitter(CGridClient&, bool use_progress);

    CGridClient& m_GridClient;
    CNetScheduleJob m_Job;
    bool         m_UseProgress;
    auto_ptr<IEmbeddedStreamWriter> m_Writer;
    auto_ptr<CNcbiOstream> m_WStream;

    /// The copy constructor and the assignment operator
    /// are prohibited
    CGridJobSubmitter(const CGridJobSubmitter&);
    CGridJobSubmitter& operator=(CGridJobSubmitter&);
};

/// Grid Job Batch  Submitter
///
class NCBI_XCONNECT_EXPORT CGridJobBatchSubmitter
{
public:

    ~CGridJobBatchSubmitter();

    /// Set a job's input This string will be sent to
    /// then the job is submitted.
    ///
    /// This method can be used to send a short data to the worker node.
    /// To send a large data use GetOStream method. Don't call this
    /// method after GetOStream method is called.
    ///
    void SetJobInput(const string& input);

    /// Get a stream where a client can write an input
    /// data for the remote job
    ///
    CNcbiOstream& GetOStream();

    /// Set a job mask
    ///
    void SetJobMask(CNetScheduleAPI::TJobMask mask);

    /// Set job tags
    ///
    void SetJobTags(const CNetScheduleAPI::TJobTags& tags);

    /// Set a job affinity
    ///
    void SetJobAffinity(const string& affinity);

    void PrepareNextJob();

    /// Submit a batch to the queue
    ///
    void Submit();

    ///
    void Reset();

    const vector<CNetScheduleJob>& GetBatch() const { return m_Jobs; }
private:
    /// Only CGridClient can create an instance of this class
    friend class CGridClient;

    void CheckIfAlreadySubmitted();

    explicit CGridJobBatchSubmitter(CGridClient&);

    CGridClient& m_GridClient;
    vector<CNetScheduleJob> m_Jobs;
    size_t       m_JobIndex;
    bool         m_HasBeenSubmitted;
    auto_ptr<IEmbeddedStreamWriter> m_Writer;
    auto_ptr<CNcbiOstream> m_WStream;

    /// The copy constructor and the assignment operator
    /// are prohibited
    CGridJobBatchSubmitter(const CGridJobBatchSubmitter&);
    CGridJobBatchSubmitter& operator=(CGridJobBatchSubmitter&);
};

/// Grid Job Status checker
///
class NCBI_XCONNECT_EXPORT CGridJobStatus
{
public:

    ~CGridJobStatus();

    /// Get a job's output string.
    ///
    /// This string can be used in two ways.
    /// 1. It can be an output data from a remote job (if that data is short)
    ///    If it is so don't use GetIStream method.
    /// 2. It holds a key to a data stored in an external data storage.
    ///    (NetCache)  In this case use GetIStream method to get a stream with
    ///    a job's result.
    ///
    const string& GetJobOutput();

    /// Get a job's input sting
    const string& GetJobInput();

    /// Get a job's return code
    //
    int           GetReturnCode();

    /// If something bad has happened this method will return an
    /// explanation
    ///
    const string& GetErrorMessage();

    /// Get a job status
    ///
    CNetScheduleAPI::EJobStatus GetStatus();

    /// Get a stream with a job's result. Stream is based on network
    /// data storage (NetCache). Size of the input data can be determined
    /// using GetInputBlobSize
    ///
    CNcbiIstream& GetIStream();

    /// Get the size of an input stream
    ///
    size_t        GetBlobSize() const     { return m_BlobSize; }

    /// Get a job interim message
    ///
    /// @param data_key
    ///     Blob key
    ///
    string GetProgressMessage();

private:
    /// Only CGridClient can create an instance of this class
    friend class CGridClient;
    CGridJobStatus(CGridClient&, bool auto_cleanup, bool use_progress);
    void x_SetJobKey(const string& job_key);
    void x_GetJobDetails();

    CGridClient& m_GridClient;
    CNetScheduleJob m_Job;
    size_t       m_BlobSize;
    bool         m_AutoCleanUp;
    bool         m_UseProgress;
    auto_ptr<CNcbiIstream> m_RStream;
    bool         m_JobDetailsRead;

    /// The copy constructor and the assignment operator
    /// are prohibited
    CGridJobStatus(const CGridJobStatus&);
    CGridJobStatus& operator=(const CGridJobStatus&);
};

/// Grid Client
///
class NCBI_XCONNECT_EXPORT CGridClient
{
public:

    enum ECleanUp {
        eAutomaticCleanup = 0,
        eManualCleanup
    };

    enum EProgressMsg {
        eProgressMsgOn = 0,
        eProgressMsgOff
    };

    /// Constructor
    ///
    /// @param ns_client
    ///     NetSchedule client - an instance of CNetScheduleSubmitter.
    /// @param storage
    ///     NetSchedule storage
    /// @param cleanup
    ///     if true the grid client will automatically remove
    ///     a job's input data from a storage when the job is
    ///     done or canceled
    ///
    CGridClient(CNetScheduleSubmitter::TInstance ns_client,
                IBlobStorage& storage,
                ECleanUp cleanup,
                EProgressMsg progress_msg);

    NCBI_DEPRECATED_CTOR(
    CGridClient(CNetScheduleSubmitter::TInstance ns_client,
                IBlobStorage& storage,
                ECleanUp cleanup,
                EProgressMsg progress_msg,
                bool unused));

    /// Constructor
    ///
    /// @param ns_client
    ///     NetSchedule client - an instance of CNetScheduleSubmitter.
    /// @param nc_client
    ///     NetCache client - an instance of CNetCacheAPI.
    /// @param cleanup
    ///     if true the grid client will automatically remove
    ///     a job's input data from a storage when the job is
    ///     done or canceled
    ///
    CGridClient(CNetScheduleSubmitter::TInstance ns_client,
                CNetCacheAPI::TInstance nc_client,
                ECleanUp cleanup,
                EProgressMsg progress_msg);

    NCBI_DEPRECATED_CTOR(
    CGridClient(CNetScheduleSubmitter::TInstance ns_client,
                CNetCacheAPI::TInstance nc_client,
                ECleanUp cleanup,
                EProgressMsg progress_msg,
                bool unused));

    /// Get a job submitter
    ///
    CGridJobSubmitter& GetJobSubmitter();

    /// Get a job submitter
    ///
    CGridJobBatchSubmitter& GetJobBatchSubmitter();

    /// Get a job status checker
    ///
    /// @param job_key
    ///     Job key
    ///
    CGridJobStatus&   GetJobStatus(const string& job_key);

    /// Cancel Job
    ///
    /// @param job_key
    ///     Job key
    ///
    void CancelJob(const string& job_key);

    /// Remove a data blob from the storage
    ///
    /// @param data_key
    ///     Blob key
    ///
    void RemoveDataBlob(const string& data_key);

    CNetScheduleSubmitter&  GetNSClient() { return m_NSClient; }
    CNetCacheAPI& GetNetCacheAPI()  { return m_NetCacheAPI; }

    size_t GetMaxServerInputSize();

private:
    void Init(ECleanUp cleanup, EProgressMsg progress_msg);

    CNetScheduleSubmitter m_NSClient;
    CNetCacheAPI m_NetCacheAPI;

    auto_ptr<CGridJobSubmitter> m_JobSubmitter;
    auto_ptr<CGridJobBatchSubmitter> m_JobBatchSubmitter;
    auto_ptr<CGridJobStatus> m_JobStatus;

    /// The copy constructor and the assignment operator
    /// are prohibited
    CGridClient(const CGridClient&);
    CGridClient& operator=(const CGridClient&);
};

/// Grid Client exception
///
class CGridClientException : public CException
{
public:
    enum EErrCode {
        eBatchAlreadySubmitted
    };

    virtual const char* GetErrCodeString(void) const
    {
        switch (GetErrCode())
        {
        case eBatchAlreadySubmitted: return "eBatchAlreadySubmitted";
        default:                     return CException::GetErrCodeString();
        }
    }

    NCBI_EXCEPTION_DEFAULT(CGridClientException, CException);
};

/////////////////////////////////////////////////////////////////////////////

END_NCBI_SCOPE

#endif // CONNECT_SERVICES_GRID__GRID_CLIENT__HPP
