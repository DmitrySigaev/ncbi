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
 * Author: Anatoliy Kuznetsov
 *
 * File Description:  BDB libarary types implementations.
 *
 */

#include <ncbi_pch.hpp>
#include <util/thread_nonstop.hpp>
#include <corelib/ncbi_system.hpp>


BEGIN_NCBI_SCOPE


CThreadNonStop::CThreadNonStop(unsigned run_delay,
                               unsigned stop_request_poll)
: m_RunInterval(run_delay),
  m_StopRequest(false)
{
    m_StopPollInterval = 
        stop_request_poll <= run_delay ? stop_request_poll : 1;
}

bool CThreadNonStop::IsStopRequested() const
{
    m_Lock.ReadLock();
    bool b = m_StopRequest;
    m_Lock.Unlock();
    return b;
}

void CThreadNonStop::RequestStop()
{
    m_Lock.WriteLock();
    m_StopRequest = true;
    m_Lock.Unlock();
}

void* CThreadNonStop::Main(void)
{
    for (bool flag = true; flag; ) {

        DoJob();

        for (unsigned i = 0; i < m_RunInterval; i += m_StopPollInterval) {
            SleepSec(m_StopPollInterval);
            if (IsStopRequested()) {
                return 0;
            }
        } // for i
        if (IsStopRequested())
            break;
        
    } // for flag

    return 0;
}


END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.1  2004/10/07 18:01:00  kuznets
 * Initial revision
 *
 *
 * ===========================================================================
 */
