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
 * Authors:  Sergey Satskiy
 *
 * File Description:
 *   NetStorage clients registry supporting facilities
 *
 */

#include <ncbi_pch.hpp>
#include <connect/ncbi_socket.hpp>

#include "nst_clients.hpp"


USING_NCBI_SCOPE;



CNSTClient::CNSTClient() :
    m_MetadataOption(eMetadataUnknown),
    m_ProtocolVersionProvided(false),
    m_Type(0),
    m_Addr(0),
    m_RegistrationTime(CNSTPreciseTime::Current()),
    m_LastAccess(m_RegistrationTime),
    m_NumberOfBytesWritten(0),
    m_NumberOfBytesRead(0),
    m_NumberOfBytesRelocated(0),
    m_NumberOfObjectsWritten(0),
    m_NumberOfObjectsRead(0),
    m_NumberOfObjectsRelocated(0),
    m_NumberOfObjectsDeleted(0),
    m_NumberOfSockErrors(0),
    m_DBClientID(k_UndefinedClientID)
{}



void CNSTClient::Touch(void)
{
    // Basically updates the last access time
    m_LastAccess = CNSTPreciseTime::Current();
}


CJsonNode  CNSTClient::Serialize(void) const
{
    CJsonNode       client(CJsonNode::NewObjectNode());

    client.SetString("Application", m_Application);
    client.SetString("Service", m_Service);
    client.SetString("ProtocolVersion", m_ProtocolVersion);
    client.SetBoolean("ProtocolVersionProvided", m_ProtocolVersionProvided);
    client.SetBoolean("TicketProvided", !m_Ticket.empty());
    client.SetString("Type", x_TypeAsString());
    client.SetString("PeerAddress", CSocketAPI::gethostbyaddr(m_Addr));
    client.SetString("RegistrationTime",
                     NST_FormatPreciseTime(m_RegistrationTime));
    client.SetString("LastAccess", NST_FormatPreciseTime(m_LastAccess));
    client.SetInteger("BytesWritten", m_NumberOfBytesWritten);
    client.SetInteger("BytesRead", m_NumberOfBytesRead);
    client.SetInteger("BytesRelocated", m_NumberOfBytesRelocated);
    client.SetInteger("ObjectsWritten", m_NumberOfObjectsWritten);
    client.SetInteger("ObjectsRead", m_NumberOfObjectsRead);
    client.SetInteger("ObjectsRelocated", m_NumberOfObjectsRelocated);
    client.SetInteger("ObjectsDeleted", m_NumberOfObjectsDeleted);
    client.SetInteger("SocketErrors", m_NumberOfSockErrors);
    client.SetString("MetadataOption", g_MetadataOptionToId(m_MetadataOption));
    client.SetInteger("DBClientID", m_DBClientID);

    return client;
}


string  CNSTClient::x_TypeAsString(void) const
{
    string      result;

    if (m_Type & eReader)
        result = "reader";

    if (m_Type & eWriter) {
        if (!result.empty())
            result += " | ";
        result += "writer";
    }

    if (m_Type & eAdministrator) {
        if (!result.empty())
            result += " | ";
        result += "admin";
    }

    if (result.empty())
        return "unknown";
    return result;
}




CNSTClientRegistry::CNSTClientRegistry() :
    m_UnknownClientDBID(k_UndefinedClientID)
{}


size_t CNSTClientRegistry::Size(void) const
{
    CMutexGuard     guard(m_Lock);
    return m_Clients.size();
}


void  CNSTClientRegistry::Touch(const CClientKey &  client,
                                const string &      applications,
                                const string &      ticket,
                                const string &      service,
                                const string &      protocol_version,
                                EMetadataOption     metadata_option,
                                unsigned int        peer_address)
{
    if (client.GetName().empty())
        return;

    CMutexGuard                             guard(m_Lock);
    map<CClientKey, CNSTClient>::iterator   found = m_Clients.find(client);

    if (found != m_Clients.end()) {
        // Client with this name existed before. Update the information.
        found->second.Touch();
        found->second.SetApplication(applications);
        found->second.SetTicket(ticket);
        found->second.SetService(service);
        found->second.SetPeerAddress(peer_address);
        found->second.SetMetadataOption(metadata_option);

        if (protocol_version.empty()) {
            found->second.SetProtocolVersion("1.0.0");
            found->second.SetProtocolVersionProvided(false);
        } else {
            found->second.SetProtocolVersion(protocol_version);
            found->second.SetProtocolVersionProvided(true);
        }
        return;
    }

    CNSTClient      new_client;
    new_client.SetApplication(applications);
    new_client.SetTicket(ticket);
    new_client.SetService(service);
    new_client.SetPeerAddress(peer_address);
    new_client.SetMetadataOption(metadata_option);

    if (protocol_version.empty()) {
        new_client.SetProtocolVersion("1.0.0");
        new_client.SetProtocolVersionProvided(false);
    } else {
        new_client.SetProtocolVersion(protocol_version);
        new_client.SetProtocolVersionProvided(true);
    }

    m_Clients[client] = new_client;
}


void  CNSTClientRegistry::Touch(const CClientKey &  client)
{
    if (client.GetName().empty())
        return;

    CMutexGuard                             guard(m_Lock);
    map<CClientKey, CNSTClient>::iterator   found = m_Clients.find(client);
    if (found != m_Clients.end())
        found->second.Touch();
}


void  CNSTClientRegistry::RegisterSocketWriteError(const CClientKey &  client)
{
    if (client.GetName().empty())
        return;

    CMutexGuard                             guard(m_Lock);
    map<CClientKey, CNSTClient>::iterator   found = m_Clients.find(client);
    if (found != m_Clients.end())
        found->second.RegisterSocketWriteError();
}


void  CNSTClientRegistry::AppendType(const CClientKey &  client,
                                     unsigned int        type_to_append)
{
    if (client.GetName().empty())
        return;

    CMutexGuard                             guard(m_Lock);
    map<CClientKey, CNSTClient>::iterator   found = m_Clients.find(client);
    if (found != m_Clients.end())
        found->second.AppendType(type_to_append);
}


void  CNSTClientRegistry::AddBytesWritten(const CClientKey &  client,
                                          size_t              count)
{
    if (client.GetName().empty())
        return;

    CMutexGuard                             guard(m_Lock);
    map<CClientKey, CNSTClient>::iterator   found = m_Clients.find(client);
    if (found != m_Clients.end())
        found->second.AddBytesWritten(count);
}


void  CNSTClientRegistry::AddBytesRead(const CClientKey &  client,
                                       size_t              count)
{
    if (client.GetName().empty())
        return;

    CMutexGuard                             guard(m_Lock);
    map<CClientKey, CNSTClient>::iterator   found = m_Clients.find(client);
    if (found != m_Clients.end())
        found->second.AddBytesRead(count);
}


void  CNSTClientRegistry::AddBytesRelocated(const CClientKey &  client,
                                            size_t              count)
{
    if (client.GetName().empty())
        return;

    CMutexGuard                             guard(m_Lock);
    map<CClientKey, CNSTClient>::iterator   found = m_Clients.find(client);
    if (found != m_Clients.end())
        found->second.AddBytesRelocated(count);
}


void  CNSTClientRegistry::AddObjectsWritten(const CClientKey &  client,
                                            size_t              count)
{
    if (client.GetName().empty())
        return;

    CMutexGuard                             guard(m_Lock);
    map<CClientKey, CNSTClient>::iterator   found = m_Clients.find(client);
    if (found != m_Clients.end())
        found->second.AddObjectsWritten(count);
}


void  CNSTClientRegistry::AddObjectsRead(const CClientKey &  client,
                                         size_t              count)
{
    if (client.GetName().empty())
        return;

    CMutexGuard                             guard(m_Lock);
    map<CClientKey, CNSTClient>::iterator   found = m_Clients.find(client);
    if (found != m_Clients.end())
        found->second.AddObjectsRead(count);
}


void  CNSTClientRegistry::AddObjectsRelocated(const CClientKey &  client,
                                              size_t              count)
{
    if (client.GetName().empty())
        return;

    CMutexGuard                             guard(m_Lock);
    map<CClientKey, CNSTClient>::iterator   found = m_Clients.find(client);
    if (found != m_Clients.end())
        found->second.AddObjectsRelocated(count);
}


void  CNSTClientRegistry::AddObjectsDeleted(const CClientKey &  client,
                                            size_t              count)
{
    if (client.GetName().empty())
        return;

    CMutexGuard                             guard(m_Lock);
    map<CClientKey, CNSTClient>::iterator   found = m_Clients.find(client);
    if (found != m_Clients.end())
        found->second.AddObjectsDeleted(count);
}


// Note: the CNetStorageHandler::x_GetClientID() may be called at the end of the
//       GETOBJECTINFO handling: the ClientID might need to be updated but the
//       client could actually be unknown, i.e. both name and namespace are
//       empty. This makes a special case for the client.
Int8  CNSTClientRegistry::GetDBClientID(const CClientKey &  client) const
{
    if (client.GetName().empty()) {
        if (client.GetNamespace().empty())
            return m_UnknownClientDBID;     // special case
        return k_UndefinedClientID;
    }

    CMutexGuard                                  guard(m_Lock);
    map<CClientKey, CNSTClient>::const_iterator  found = m_Clients.find(client);
    if (found != m_Clients.end())
        return found->second.GetDBClientID();
    return k_UndefinedClientID;
}


// Note: the CNetStorageHandler::x_GetClientID() may be called at the end of the
//       GETOBJECTINFO handling: the ClientID might need to be updated but the
//       client could actually be unknown, i.e. both name and namespace are
//       empty. This makes a special case for the client.
void  CNSTClientRegistry::SetDBClientID(const CClientKey &  client, Int8  id)
{
    if (client.GetName().empty()) {
        if (client.GetNamespace().empty())
            m_UnknownClientDBID = id;       // special case
        return;
    }

    CMutexGuard                             guard(m_Lock);
    map<CClientKey, CNSTClient>::iterator   found = m_Clients.find(client);
    if (found != m_Clients.end())
        found->second.SetDBClientID(id);
}


CJsonNode  CNSTClientRegistry::Serialize(void) const
{
    CJsonNode       clients(CJsonNode::NewArrayNode());
    CMutexGuard     guard(m_Lock);

    for (map<CClientKey, CNSTClient>::const_iterator  k = m_Clients.begin();
         k != m_Clients.end(); ++k) {
        CJsonNode   single_client(k->second.Serialize());

        single_client.SetString("Name", k->first.GetName());
        single_client.SetString("Namespace", k->first.GetNamespace());
        clients.Append(single_client);
    }

    return clients;
}

