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
 * Authors:  Victor Joukov
 *
 * File Description:
 *   NetSchedule queue structure and parameters
 */
#include <ncbi_pch.hpp>

#include "squeue.hpp"
#include "job_time_line.hpp"
#include "nslb.hpp"


BEGIN_NCBI_SCOPE


void SQueueParameters::Read(const IRegistry& reg, const string& sname)
{
    timeout = reg.GetInt(sname, "timeout", 3600, 0, IRegistry::eReturn);
    notif_timeout =
        reg.GetInt(sname, "notif_timeout", 7, 0, IRegistry::eReturn);
    run_timeout =
        reg.GetInt(sname, "run_timeout", 
                                    timeout, 0, IRegistry::eReturn);
    run_timeout_precision =
        reg.GetInt(sname, "run_timeout_precision", 
                                    run_timeout, 0, IRegistry::eReturn);

    program_name = reg.GetString(sname, "program", kEmptyStr);

    delete_when_done = reg.GetBool(sname, "delete_done", 
                                        false, 0, IRegistry::eReturn);
    failed_retries = 
        reg.GetInt(sname, "failed_retries", 
                                    0, 0, IRegistry::eReturn);
    subm_hosts = reg.GetString(sname,  "subm_host",  kEmptyStr);
    wnode_hosts = reg.GetString(sname, "wnode_host", kEmptyStr);
    dump_db = reg.GetBool(sname, "dump_db", false, 0, IRegistry::eReturn);
}


void SQueueLBParameters::Read(const IRegistry& reg, const string& sname)
{
    lb_flag = reg.GetBool(sname, "lb", false, 0, IRegistry::eReturn);
    lb_service = reg.GetString(sname, "lb_service", kEmptyStr);
    lb_collect_time =
        reg.GetInt(sname, "lb_collect_time", 5, 0, IRegistry::eReturn);
    lb_unknown_host = reg.GetString(sname, "lb_unknown_host", kEmptyStr);
    lb_exec_delay_str = reg.GetString(sname, "lb_exec_delay", kEmptyStr);
    lb_stall_time_mult = 
        reg.GetDouble(sname, "lb_exec_delay_mult", 0.5, 0, IRegistry::eReturn);

    string curve_type = reg.GetString(sname, "lb_curve", kEmptyStr);
    if (curve_type.empty() || 
        NStr::CompareNocase(curve_type, "linear") == 0) {
        lb_curve = eLBLinear;
        lb_curve_high = reg.GetDouble(sname,
                                      "lb_curve_high",
                                      0.6,
                                      0,
                                      IRegistry::eReturn);
        lb_curve_linear_low = reg.GetDouble(sname,
                                            "lb_curve_linear_low",
                                            0.15,
                                            0,
                                            IRegistry::eReturn);
        LOG_POST(Info << sname 
                      << " initializing linear LB curve"
                      << " y0=" << lb_curve_high
                      << " yN=" << lb_curve_linear_low);
    } else if (NStr::CompareNocase(curve_type, "regression") == 0) {
        lb_curve = eLBRegression;
        lb_curve_high = reg.GetDouble(sname,
                                      "lb_curve_high",
                                      0.85,
                                      0,
                                      IRegistry::eReturn);
        lb_curve_regression_a = reg.GetDouble(sname,
                                              "lb_curve_regression_a",
                                              -0.2,
                                              0,
                                              IRegistry::eReturn);
        LOG_POST(Info << sname 
                      << " initializing regression LB curve."
                      << " y0=" << lb_curve_high 
                      << " a="  << lb_curve_regression_a);
    }
}


SLockedQueue::SLockedQueue(const string& queue_name) 
    : timeout(3600), 
        notif_timeout(7), 
        delete_done(false),
        failed_retries(0),
        last_notif(0), 
        q_notif("NCBI_JSQ_"),
        run_time_line(0),

        rec_dump("jsqd_"+queue_name+".dump", 10 * (1024 * 1024)),
        rec_dump_flag(false),
        lb_flag(false),
        lb_coordinator(0),
        lb_stall_delay_type(eNSLB_Constant),
        lb_stall_time(6),
        lb_stall_time_mult(1.0)
{
    _ASSERT(!queue_name.empty());
    q_notif.append(queue_name);
    m_StatThread.Reset(new CStatisticsThread(*this));
    m_StatThread->Run();
}

SLockedQueue::~SLockedQueue()
{
    NON_CONST_ITERATE(TListenerList, it, wnodes) {
        SQueueListener* node = *it;
        delete node;
    }
    delete run_time_line;
    delete lb_coordinator;
}

enum {
    MEASURE_INTERVAL = 1,
    DECAY_INTERVAL = 10, // for informational purposes only, see EXP below
    FSHIFT = 7,
    FIXED_1 = 1 << FSHIFT,
    EXP = 116 // 2 ^ FSHIFT / 2 ^ ( MEASURE_INTERVAL * log2(e) / DECAY_INTERVAL)
};

SLockedQueue::CStatisticsThread::CStatisticsThread(TContainer& container)
    : CThreadNonStop(MEASURE_INTERVAL),
        m_Container(container)
{
}

void SLockedQueue::CStatisticsThread::DoJob(void) {
    unsigned counter;
    counter = m_Container.m_GetCounter.Get();
    m_Container.m_GetCounter.Add(-counter);
    m_Container.m_GetAverage = (EXP * m_Container.m_GetAverage +
                                (FIXED_1-EXP) * (counter << FSHIFT)) >> FSHIFT;
    counter = m_Container.m_PutCounter.Get();
    m_Container.m_PutCounter.Add(-counter);
    m_Container.m_PutAverage = (EXP * m_Container.m_PutAverage +
                                (FIXED_1-EXP) * (counter << FSHIFT)) >> FSHIFT;
}

double SLockedQueue::GetGetAverage(void)
{
    return m_GetAverage / double(FIXED_1 * MEASURE_INTERVAL);
}

double SLockedQueue::GetPutAverage(void)
{
    return m_PutAverage / double(FIXED_1 * MEASURE_INTERVAL);
}


END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.2  2006/11/27 16:46:21  joukovv
 * Iterator to CQueueCollection introduced to decouple it with CQueueDataBase;
 * un-nested CQueue from CQueueDataBase; instrumented code to count job
 * throughput average.
 *
 * Revision 1.1  2006/10/31 19:35:26  joukovv
 * Queue creation and reading of its parameters decoupled. Code reorganized to
 * reduce coupling in general. Preparing for queue-on-demand.
 *
 * ===========================================================================
 */

