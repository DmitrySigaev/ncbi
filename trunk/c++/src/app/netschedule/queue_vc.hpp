#ifndef NETSCHEDULE_QUEUE_VC__HPP
#define NETSCHEDULE_QUEUE_VC__HPP


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
 *   Net schedule queue version control
 *
 */
/// @file bdb_queue.hpp
/// NetSchedule job status database. 
///
/// @internal

#include <corelib/version.hpp>
#include <corelib/ncbistr.hpp>

BEGIN_NCBI_SCOPE

/// Netschedule queue client info
///
/// @internal
///
struct CQueueClientInfo
{
    string         client_name;
    CVersionInfo   version_info;

    CQueueClientInfo() : version_info(-1,-1,-1) {}
    CQueueClientInfo(const string& cname, const CVersionInfo& vinfo)
        : client_name(cname), version_info(vinfo)
    {}
};

/// All clients registered to connect
///
/// @internal
///
class CQueueClientInfoList
{
public:
    CQueueClientInfoList() {}
    void AddClientInfo(const CQueueClientInfo& cinfo)
    {
        m_RegisteredClients.push_back(cinfo);
    }

    bool IsMatchingClient(const CQueueClientInfo& cinfo) const
    {
        _ASSERT(IsConfigured());

        ITERATE(vector<CQueueClientInfo>, it, m_RegisteredClients) {
            if (NStr::CompareNocase(cinfo.client_name, it->client_name)==0) {
                if (it->version_info.IsAny())
                    return true;

                if (cinfo.version_info.IsUpCompatible(it->version_info))
                    return true;
            }
        }
        return false;
    }

    bool IsConfigured() const
    {
        return !m_RegisteredClients.empty();
    }


private:
    vector<CQueueClientInfo>   m_RegisteredClients;
};


END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.1  2005/04/05 20:51:34  kuznets
 * Initial revision
 *
 *
 * ===========================================================================
 */

#endif /* NETSCHEDULE_BDB_QUEUE__HPP */
