#ifndef NETSCHEDULE_CLIENT_ADMIN__HPP
#define NETSCHEDULE_CLIENT_ADMIN__HPP

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
 * Authors:  Anatoliy Kuznetsov
 *
 * File Description:
 *   NetSchedule client with all admin interfaces open
 *
 */

#include <connect/services/netschedule_client.hpp>

BEGIN_NCBI_SCOPE


/// Proxy class to open ShutdownServer
///
/// @internal
class CNetScheduleClient_Control : public CNetScheduleClient
{
public:
    CNetScheduleClient_Control(const string&  host,
                               unsigned short port,
                               const string&  queue = "noname")
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


END_NCBI_SCOPE

#endif

