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
 * Author:  Anatoliy Kuznetsov, Maxim Didenko
 *
 * File Description:
 *   Implementation of NetSchedule API.
 *
 */

#include <ncbi_pch.hpp>
#include <connect/services/netschedule_api.hpp>

#include "netschedule_api_wait.hpp"

#include <stdio.h>

BEGIN_NCBI_SCOPE



//////////////////////////////////////////////////////////////////////////////
static void s_SerializeJob(string& cmd, const CNetScheduleJob& job, string& aff_prev)
{
    cmd.append("\"");
    cmd.append(NStr::PrintableString(job.input));
    cmd.append("\"");

    if (!job.progress_msg.empty()) {
        cmd.append(" \"");
        cmd.append(job.progress_msg);
        cmd.append("\"");
    }

    if (!job.affinity.empty()) {
        if (job.affinity == aff_prev) { // exactly same affinity(sorted jobs)
            cmd.append(" affp");
        } else{
            cmd.append(" aff=\"");
            cmd.append(job.affinity);
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

string CNetScheduleSubmitter::SubmitJob(CNetScheduleJob& job) const
{
    if (job.input.length() > kNetScheduleMaxDataSize) {
        NCBI_THROW(CNetScheduleException, eDataTooLong, 
            "Input data too long.");
    }


    //cerr << "Input: " << input << endl;
    string cmd = "SUBMIT ";

    string aff_prev;
    s_SerializeJob(cmd, job, aff_prev);

    job.job_id = m_API->SendCmdWaitResponse(m_API->x_GetConnector(), cmd);

    if (job.job_id.empty()) {
        NCBI_THROW(CNetServiceException, eCommunicationError, 
                   "Invalid server response. Empty key.");
    }

    return job.job_id;
}

void CNetScheduleSubmitter::SubmitJobBatch(vector<CNetScheduleJob>& jobs) const
{
    // veryfy the input data
    ITERATE(vector<CNetScheduleJob>, it, jobs) {
        const string& input = it->input;

        if (input.length() > kNetScheduleMaxDataSize) {
            NCBI_THROW(CNetScheduleException, eDataTooLong, 
                "Input data too long.");
        }
    }

    CNetSrvConnector& conn = m_API->x_GetConnector();

    //    m_Sock->DisableOSSendDelay(true);

    // batch command
    
    m_API->SendCmdWaitResponse(conn, "BSUB");

    //m_Sock->DisableOSSendDelay(false);

    string cmd;
    cmd.reserve(kNetScheduleMaxDataSize * 6);
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

        conn.WriteStr(cmd +"\r\n");


        unsigned batch_start = i;
        string aff_prev;
        for (unsigned j = 0; j < batch_size; ++j,++i) {
            cmd.erase();
            s_SerializeJob(cmd, jobs[i], aff_prev);
            /*
            string input = NStr::PrintableString(jobs[i].input);
            const string& aff = jobs[i].affinity;
            if (aff[0]) {
                if (aff == aff_prev) { // exactly same affinity(sorted jobs)
                    sprintf(buf, "\"%s\" affp", 
                            input.c_str()
                            );
                } else {
                    sprintf(buf, "\"%s\" aff=\"%s\"", 
                            input.c_str(),
                            aff.c_str()
                            );
                    aff_prev = aff;
                }
            } else {
                aff_prev.erase();
                sprintf(buf, "\"%s\"", input.c_str());
            }
            conn.WriteBuf(buf, strlen(buf)+1);
            */
            conn.WriteStr(cmd + "\r\n");
        }

        string resp = m_API->SendCmdWaitResponse(conn, "ENDB");

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

    //m_Sock->DisableOSSendDelay(true);

    conn.WriteBuf("ENDS", 5); //???? do we need to wait for the last reponse?
}

struct SWaitJobPred {
    SWaitJobPred(unsigned int job_id) : m_JobId(job_id) {}
    bool operator()(const string& buf) 
    {
        static const char* sign = "JNTF";
        static size_t min_len = 5;
        if ((buf.size() < min_len) || ((buf[0] ^ sign[0]) | (buf[1] ^ sign[1])) ) {
            return false;
        }
        
        const char* job = buf.data() + 5;
        unsigned notif_job_id = (unsigned)::atoi(job);
        if (notif_job_id == m_JobId) {
            return true;
        }
        return false;
    }
    unsigned int m_JobId;
};


CNetScheduleAPI::EJobStatus 
CNetScheduleSubmitter::SubmitJobAndWait(CNetScheduleJob& job,
                                        unsigned       wait_time,
                                        unsigned short udp_port) const
{
    _ASSERT(wait_time);
    _ASSERT(udp_port);

    SubmitJob(job);

    CNetScheduleKey key(job.job_id);

    s_WaitNotification(wait_time, udp_port, SWaitJobPred(key.id));

    CNetScheduleAPI::EJobStatus status = GetJobStatus(job.job_id);
    if ( status == CNetScheduleAPI::eDone || status == CNetScheduleAPI::eFailed)
        m_API->GetJobDetails(job);
    return status;
}

void CNetScheduleSubmitter::CancelJob(const string& job_key) const
{
    m_API->x_SendJobCmdWaitResponse("CANCEL", job_key);
}


END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log: netschedule_api_submitter.cpp,v $
 * Revision 6.2  2007/01/10 16:01:56  ucko
 * +<stdio.h> for sprintf(); tweak for GCC 2.95's string implementation,
 * whose operator [] const returns a value rather than a reference.
 *
 * Revision 6.1  2007/01/09 15:29:55  didenko
 * Added new API for NetSchedule service
 *
 * ===========================================================================
 */
