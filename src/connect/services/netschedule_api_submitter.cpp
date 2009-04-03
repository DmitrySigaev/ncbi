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
 * Author:  Anatoliy Kuznetsov, Maxim Didenko, Dmitry Kazimirov
 *
 * File Description:
 *   Implementation of NetSchedule API.
 *
 */

#include <ncbi_pch.hpp>

#include "netschedule_api_impl.hpp"

#include <connect/services/netschedule_api.hpp>
#include <connect/services/util.hpp>

#include <corelib/request_ctx.hpp>

#include <stdio.h>

BEGIN_NCBI_SCOPE



//////////////////////////////////////////////////////////////////////////////
static void s_SerializeJob(string& cmd, const CNetScheduleJob& job,
    unsigned short udp_port, unsigned wait_time, string& aff_prev)
{
    cmd.append("\"");
    string ps_input = NStr::PrintableString(job.input);

    cmd.append(ps_input);
    cmd.append("\"");

    if (!job.progress_msg.empty()) {
        cmd.append(" \"");
        cmd.append(job.progress_msg);
        cmd.append("\"");
    }

    if (udp_port != 0) {
        cmd.append(" ");
        cmd.append(NStr::UIntToString(udp_port));
        cmd.append(" ");
        cmd.append(NStr::UIntToString(wait_time));
    }

    if (!job.affinity.empty()) {
        if (job.affinity == aff_prev) { // exactly same affinity(sorted jobs)
            cmd.append(" affp");
        } else{
            cmd.append(" aff=\"");
            cmd.append(NStr::PrintableString(job.affinity));
            cmd.append("\"");
            aff_prev = job.affinity;
        }
    }

    if (job.mask != CNetScheduleAPI::eEmptyMask) {
        cmd.append(" msk=");
        cmd.append(NStr::UIntToString(job.mask));
    }

    if( !job.tags.empty() ) {
        string tags;
        ITERATE(CNetScheduleAPI::TJobTags, tag, job.tags) {
            if( tag != job.tags.begin() )
                tags.append("\t");
            tags.append(tag->first);
            tags.append("\t");
            tags.append(tag->second);
        }
        cmd.append(" tags=\"");
        cmd.append(NStr::PrintableString(tags));
        cmd.append("\"");
    }

}

static void s_AppendClientIPAndSessionID(std::string& cmd)
{
    CRequestContext& req = CDiagContext::GetRequestContext();

    if (req.IsSetClientIP()) {
        cmd += " ip=\"";
        cmd += req.GetClientIP();
        cmd += '"';
    }

    if (req.IsSetSessionID()) {
        cmd += " sid=\"";
        cmd += NStr::PrintableString(req.GetSessionID());
        cmd += '"';
    }
}

inline
void static s_CheckInputSize(const string& input, size_t max_input_size)
{
    if (input.length() >  max_input_size) {
        NCBI_THROW(CNetScheduleException, eDataTooLong,
                   "Input data too long.");
    }
}

string CNetScheduleSubmitter::SubmitJob(CNetScheduleJob& job) const
{
    return m_Impl->SubmitJobImpl(job, 0, 0);
}

string SNetScheduleSubmitterImpl::SubmitJobImpl(CNetScheduleJob& job,
    unsigned short udp_port, unsigned wait_time) const
{
    size_t max_input_size = m_API->GetServerParams().max_input_size;
    s_CheckInputSize(job.input, max_input_size);

    string cmd = "SUBMIT ";

    string aff_prev;
    s_SerializeJob(cmd, job, udp_port, wait_time, aff_prev);

    s_AppendClientIPAndSessionID(cmd);

    job.job_id = m_API->m_Service->GetBestConnection().Exec(cmd);

    if (job.job_id.empty()) {
        NCBI_THROW(CNetServiceException, eCommunicationError,
                   "Invalid server response. Empty key.");
    }

    return job.job_id;
}

void CNetScheduleSubmitter::SubmitJobBatch(vector<CNetScheduleJob>& jobs) const
{
    // verify the input data
    size_t max_input_size = m_Impl->m_API->GetServerParams().max_input_size;

    ITERATE(vector<CNetScheduleJob>, it, jobs) {
        const string& input = it->input;
        s_CheckInputSize(input, max_input_size);
    }

    CNetServerConnection conn = m_Impl->m_API->m_Service->GetBestConnection();

    // Batch submit command.
    std::string cmd = "BSUB";

    s_AppendClientIPAndSessionID(cmd);

    conn.Exec(cmd);

    cmd.reserve(max_input_size * 6);
    string host;
    unsigned short port = 0;
    for (unsigned i = 0; i < jobs.size(); ) {

        // Batch size should be reasonable not to trigger network timeout
        const unsigned kMax_Batch = 10000;

        unsigned batch_size = jobs.size() - i;
        if (batch_size > kMax_Batch) {
            batch_size = kMax_Batch;
        }

        cmd.erase();
        cmd = "BTCH ";
        cmd.append(NStr::UIntToString(batch_size));

        conn->WriteLine(cmd);


        unsigned batch_start = i;
        string aff_prev;
        for (unsigned j = 0; j < batch_size; ++j,++i) {
            cmd.erase();
            s_SerializeJob(cmd, jobs[i], 0, 0, aff_prev);

            conn->WriteLine(cmd);
        }

        string resp = conn.Exec("ENDB");

        if (resp.empty()) {
            NCBI_THROW(CNetServiceException, eProtocolError,
                    "Invalid server response. Empty key.");
        }

        // parse the batch answer
        // FORMAT:
        //  first_job_id host port

        {{
        const char* s = resp.c_str();
        unsigned first_job_id = ::atoi(s);

        if (host.empty()) {
            for (; *s != ' '; ++s) {
                if (*s == 0) {
                    NCBI_THROW(CNetServiceException, eProtocolError,
                            "Invalid server response. Batch answer format.");
                }
            }
            ++s;
            if (*s == 0) {
                NCBI_THROW(CNetServiceException, eProtocolError,
                        "Invalid server response. Batch answer format.");
            }
            for (; *s != ' '; ++s) {
                if (*s == 0) {
                    NCBI_THROW(CNetServiceException, eProtocolError,
                            "Invalid server response. Batch answer format.");
                }
                host.push_back(*s);
            }
            ++s;
            if (*s == 0) {
                NCBI_THROW(CNetServiceException, eProtocolError,
                        "Invalid server response. Batch answer format.");
            }

            port = atoi(s);
            if (port == 0) {
                NCBI_THROW(CNetServiceException, eProtocolError,
                        "Invalid server response. Port=0.");
            }
        }

        // assign job ids, protocol guarantees all jobs in batch will
        // receive sequential numbers, so server sends only first job id
        //
        for (unsigned j = 0; j < batch_size; ++j) {
            CNetScheduleKey key(first_job_id, host, port);
            jobs[batch_start].job_id = string(key);
            ++first_job_id;
            ++batch_start;
        }

        }}


    } // for

    conn.Exec("ENDS");
}

class CReadCmdExecutor : public INetServerFinder
{
public:
    CReadCmdExecutor(const string& cmd, std::string& batch_id,
            std::vector<std::string>& job_ids) :
        m_Cmd(cmd), m_BatchId(batch_id), m_JobIds(job_ids)
    {
    }

    virtual bool Consider(CNetServer server);

private:
    std::string m_Cmd;
    std::string& m_BatchId;
    std::vector<std::string>& m_JobIds;
};

bool CReadCmdExecutor::Consider(CNetServer server)
{
    std::string response = server.Connect().Exec(m_Cmd);

    if (response.empty() || response == "0 " || response == "0")
        return false;

    std::string encoded_bitvector;

    NStr::SplitInTwo(response, " ", m_BatchId, encoded_bitvector);

    CBitVectorDecoder bvdec(encoded_bitvector);

    std::string host = server.GetHost();
    unsigned port = server.GetPort();

    unsigned from, to;

    while (bvdec.GetNextRange(from, to))
        while (from <= to)
            m_JobIds.push_back(CNetScheduleKey(from++, host, port));

    return true;
}

bool CNetScheduleSubmitter::Read(std::string& batch_id,
    std::vector<std::string>& job_ids,
    unsigned max_jobs, unsigned timeout)
{
    std::string cmd("READ ");

    cmd.append(NStr::UIntToString(max_jobs));

    if (timeout > 0) {
        cmd += ' ';
        cmd += NStr::UIntToString(timeout);
    }

    return m_Impl->m_API->m_Service->FindServer(
        new CReadCmdExecutor(cmd, batch_id, job_ids));
}

void SNetScheduleSubmitterImpl::ExecReadCommand(const char* cmd_start,
    const char* cmd_name,
    const std::string& batch_id,
    const std::vector<std::string>& job_ids,
    const std::string& error_message)
{
    if (job_ids.empty()) {
        NCBI_THROW_FMT(CNetScheduleException, eInvalidParameter,
            cmd_name << ": no job keys specified");
    }

    CBitVectorEncoder bvenc;

    std::vector<std::string>::const_iterator job_id = job_ids.begin();

    CNetScheduleKey first_key(*job_id);

    bvenc.AppendInteger(first_key.id);

    while (++job_id != job_ids.end()) {
        CNetScheduleKey key(*job_id);

        if (key.host != first_key.host || key.port != first_key.port) {
            NCBI_THROW_FMT(CNetScheduleException, eInvalidParameter,
                cmd_name << ": all jobs must belong to a single NS");
        }

        bvenc.AppendInteger(key.id);
    }

    std::string cmd = cmd_start + batch_id;

    cmd += " \"";
    cmd += bvenc.Encode();
    cmd += '"';

    if (!error_message.empty()) {
        cmd += " \"";
        cmd += NStr::PrintableString(error_message);
        cmd += '"';
    }

    m_API->m_Service->GetConnection(first_key.host, first_key.port).Exec(cmd);
}

void CNetScheduleSubmitter::ReadConfirm(const std::string& batch_id,
    const std::vector<std::string>& job_ids)
{
    m_Impl->ExecReadCommand("CFRM ", "ReadConfirm",
        batch_id, job_ids, kEmptyStr);
}

void CNetScheduleSubmitter::ReadRollback(const std::string& batch_id,
    const std::vector<std::string>& job_ids)
{
    m_Impl->ExecReadCommand("RDRB ", "ReadRollback",
        batch_id, job_ids, kEmptyStr);
}

void CNetScheduleSubmitter::ReadFail(const std::string& batch_id,
    const std::vector<std::string>& job_ids,
    const std::string& error_message)
{
    m_Impl->ExecReadCommand("FRED ", "ReadFail",
        batch_id, job_ids, error_message);
}

struct SWaitJobPred {
    SWaitJobPred(unsigned int job_id) : m_JobId(job_id) {}

    bool operator()(const string& buf)
    {
        static const char* sign = "JNTF";

        if ((buf.size() < 5) ||
            ((buf[0] ^ sign[0]) | (buf[1] ^ sign[1])))
            return false;

        return m_JobId == (unsigned) ::atoi(buf.data() + 5);
    }

    unsigned int m_JobId;
};


CNetScheduleAPI::EJobStatus
CNetScheduleSubmitter::SubmitJobAndWait(CNetScheduleJob& job,
                                        unsigned       wait_time,
                                        unsigned short udp_port)
{
    _ASSERT(wait_time);
    _ASSERT(udp_port);

    m_Impl->SubmitJobImpl(job, udp_port, wait_time);

    CNetScheduleKey key(job.job_id);

    s_WaitNotification(wait_time, udp_port, SWaitJobPred(key.id));

    CNetScheduleAPI::EJobStatus status = GetJobStatus(job.job_id);

    if (status == CNetScheduleAPI::eDone ||
        status == CNetScheduleAPI::eFailed)
        m_Impl->m_API.GetJobDetails(job);

    return status;
}

void CNetScheduleSubmitter::CancelJob(const string& job_key) const
{
    m_Impl->m_API->x_SendJobCmdWaitResponse("CANCEL", job_key);
}

CNetScheduleAPI::EJobStatus
    CNetScheduleSubmitter::GetJobStatus(const string& job_key)
{
    return m_Impl->m_API->x_GetJobStatus(job_key, true);
}

CNetScheduleAPI::EJobStatus
    CNetScheduleSubmitter::GetJobDetails(CNetScheduleJob& job)
{
    return m_Impl->m_API.GetJobDetails(job);
}

void CNetScheduleSubmitter::GetProgressMsg(CNetScheduleJob& job)
{
    m_Impl->m_API.GetProgressMsg(job);
}

const CNetScheduleAPI::SServerParams& CNetScheduleSubmitter::GetServerParams()
{
    return m_Impl->m_API->GetServerParams();
}

END_NCBI_SCOPE
