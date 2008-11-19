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
 * Author: Maxim Didenko
 *
 * File Description:
 *   Implementation of NetSchedule API.
 *
 */

#include <ncbi_pch.hpp>

#include <connect/services/netschedule_api_expt.hpp>


BEGIN_NCBI_SCOPE

CNetScheduleExceptionMap::CNetScheduleExceptionMap()
{
    m_Map["eInternalError"]       = CNetScheduleException::eInternalError;
    m_Map["eProtocolSyntaxError"] = CNetScheduleException::eProtocolSyntaxError;
    m_Map["eAuthenticationError"] = CNetScheduleException::eAuthenticationError;
    m_Map["eKeyFormatError"]      = CNetScheduleException::eKeyFormatError;
    m_Map["eInvalidJobStatus"]    = CNetScheduleException::eInvalidJobStatus;
    m_Map["eUnknownQueue"]        = CNetScheduleException::eUnknownQueue;
    m_Map["eUnknownQueueClass"]   = CNetScheduleException::eUnknownQueueClass;
    m_Map["eTooManyPendingJobs"]  = CNetScheduleException::eTooManyPendingJobs;
    m_Map["eDataTooLong"]         = CNetScheduleException::eDataTooLong;
    m_Map["eInvalidClient"]       = CNetScheduleException::eInvalidClient;
    m_Map["eAccessDenied"]        = CNetScheduleException::eAccessDenied;
    m_Map["eDuplicateName"]       = CNetScheduleException::eDuplicateName;
    m_Map["eQuerySyntaxError"]    = CNetScheduleException::eQuerySyntaxError;
    m_Map["eCommandIsNotAllowed"] = CNetScheduleException::eCommandIsNotAllowed;
    m_Map["eObsoleteCommand"]     = CNetScheduleException::eObsoleteCommand;
    m_Map["eInvalidParameter"]    = CNetScheduleException::eInvalidParameter;
    m_Map["eNoJobsWithAffinity"]  = CNetScheduleException::eNoJobsWithAffinity;
}

CException::TErrCode CNetScheduleExceptionMap::GetCode(const string& name)
{
    TMap::iterator it = m_Map.find(name);
    if (it == m_Map.end())
        return CException::eInvalid;
    return it->second;
}

END_NCBI_SCOPE
