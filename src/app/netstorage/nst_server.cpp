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
 * Authors:  Denis Vakatov
 *
 * File Description: NetStorage threaded server
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/resource_info.hpp>

#include "nst_server.hpp"
#include "nst_service_thread.hpp"
#include "nst_config.hpp"
#include "nst_constants.hpp"


USING_NCBI_SCOPE;


CNetStorageServer *     CNetStorageServer::sm_netstorage_server = NULL;
const string            kCrashFlagFileName("CRASH_FLAG");


CNetStorageServer::CNetStorageServer()
   : m_Port(0),
     m_HostNetAddr(CSocketAPI::gethostbyname(kEmptyStr)),
     m_Shutdown(false),
     m_SigNum(0),
     m_Log(default_log),
     m_LogTimingNSTAPI(default_log_timing_nst_api),
     m_LogTimingClientSocket(default_log_timing_client_socket),
     m_SessionID("s" + x_GenerateGUID()),
     m_NetworkTimeout(0),
     m_DataPath("./data." + NStr::NumericToString(m_Port)),
     m_StartTime(CNSTPreciseTime::Current()),
     m_AnybodyCanReconfigure(false),
     m_NeedDecryptCacheReset(false),
     m_LastDecryptCacheReset(0.0),
     m_LastUsedObjectID(0),
     m_LastReservedObjectID(-1)
{
    sm_netstorage_server = this;
}


CNetStorageServer::~CNetStorageServer()
{}


void
CNetStorageServer::AddDefaultListener(IServer_ConnectionFactory *  factory)
{
    AddListener(factory, m_Port);
}


CJsonNode
CNetStorageServer::SetParameters(
                        const SNetStorageServerParameters &  params,
                        bool                                 reconfigure)
{
    if (!reconfigure) {
        CServer::SetParameters(params);

        m_Port = params.port;
        m_Log = params.log;
        m_LogTimingNSTAPI = params.log_timing_nst_api;
        m_LogTimingClientSocket = params.log_timing_client_socket;
        m_NetworkTimeout = params.network_timeout;
        m_AdminClientNames = x_GetAdminClientNames(params.admin_client_names);
        m_DataPath = params.data_path;
        return CJsonNode::NewNullNode();
    }

    // Here: it is reconfiguration.
    // Need to consider:
    // - log
    // - admin names
    // - max connections
    // - network timeout

    set<string>     new_admins = x_GetAdminClientNames(
                                        params.admin_client_names);
    vector<string>  added;
    vector<string>  deleted;

    {
        CFastMutexGuard     guard(m_AdminClientNamesLock);

        // Compare the old and the new sets of administrators
        set_difference(new_admins.begin(), new_admins.end(),
                       m_AdminClientNames.begin(), m_AdminClientNames.end(),
                       inserter(added, added.begin()));
        set_difference(m_AdminClientNames.begin(), m_AdminClientNames.end(),
                       new_admins.begin(), new_admins.end(),
                       inserter(deleted, deleted.begin()));

        m_AdminClientNames = new_admins;
    }

    SServer_Parameters      current_params;
    CServer::GetParameters(&current_params);

    if (m_Log == params.log &&
        m_LogTimingNSTAPI == params.log_timing_nst_api &&
        m_LogTimingClientSocket == params.log_timing_client_socket &&
        m_NetworkTimeout == params.network_timeout &&
        m_DataPath == params.data_path &&
        current_params.max_connections == params.max_connections &&
        added.empty() &&
        deleted.empty())
        return CJsonNode::NewNullNode();

    // Here: there is a difference
    CJsonNode       diff = CJsonNode::NewObjectNode();

    if (m_Log != params.log) {
        CJsonNode   values = CJsonNode::NewObjectNode();

        values.SetByKey("Old", CJsonNode::NewBooleanNode(m_Log));
        values.SetByKey("New", CJsonNode::NewBooleanNode(params.log));
        diff.SetByKey("log", values);

        m_Log = params.log;
    }
    if (m_LogTimingNSTAPI != params.log_timing_nst_api) {
        CJsonNode   values = CJsonNode::NewObjectNode();

        values.SetByKey("Old", CJsonNode::NewBooleanNode(m_LogTimingNSTAPI));
        values.SetByKey("New", CJsonNode::NewBooleanNode(
                                            params.log_timing_nst_api));
        diff.SetByKey("log_timing_nst_api", values);

        m_LogTimingNSTAPI = params.log_timing_nst_api;
    }
    if (m_LogTimingClientSocket != params.log_timing_client_socket) {
        CJsonNode   values = CJsonNode::NewObjectNode();

        values.SetByKey("Old", CJsonNode::NewBooleanNode(
                                            m_LogTimingClientSocket));
        values.SetByKey("New", CJsonNode::NewBooleanNode(
                                            params.log_timing_client_socket));
        diff.SetByKey("log_timing_client_socket", values);

        m_LogTimingClientSocket = params.log_timing_client_socket;
    }

    if (m_NetworkTimeout != params.network_timeout) {
        CJsonNode   values = CJsonNode::NewObjectNode();

        values.SetByKey("Old", CJsonNode::NewIntegerNode(m_NetworkTimeout));
        values.SetByKey("New", CJsonNode::NewIntegerNode(
                                                    params.network_timeout));
        diff.SetByKey("network_timeout", values);

        m_NetworkTimeout = params.network_timeout;
    }

    if (m_DataPath != params.data_path) {
        CJsonNode   values = CJsonNode::NewObjectNode();

        values.SetByKey("Old", CJsonNode::NewStringNode(m_DataPath));
        values.SetByKey("New", CJsonNode::NewStringNode(params.data_path));
        diff.SetByKey("data_path", values);

        m_DataPath = params.data_path;
    }

    if (current_params.max_connections != params.max_connections) {
        CJsonNode   values = CJsonNode::NewObjectNode();

        values.SetByKey("Old", CJsonNode::NewIntegerNode(
                                            current_params.max_connections));
        values.SetByKey("New", CJsonNode::NewIntegerNode(
                                            params.max_connections));
        diff.SetByKey("max_connections", values);

        current_params.max_connections = params.max_connections;
        CServer::SetParameters(current_params);
    }

    if (!added.empty() || !deleted.empty())
        diff.SetByKey("admin_client_name", x_diffInJson(added, deleted));

    return diff;
}


bool CNetStorageServer::ShutdownRequested(void)
{
    return m_Shutdown;
}


void CNetStorageServer::SetShutdownFlag(int signum)
{
    if (!m_Shutdown) {
        m_Shutdown = true;
        m_SigNum = signum;
    }
}


CNetStorageServer *  CNetStorageServer::GetInstance(void)
{
    return sm_netstorage_server;
}


void CNetStorageServer::Exit()
{}


// Resets the decrypt keys cache if necessary
void CNetStorageServer::ResetDecryptCacheIfNeed(void)
{
    const CNSTPreciseTime   k_ResetInterval(100.0);

    if (m_NeedDecryptCacheReset) {
        CNSTPreciseTime     current = CNSTPreciseTime::Current();
        if ((current - m_LastDecryptCacheReset) >= k_ResetInterval) {
            CNcbiEncrypt::Reload();
            m_LastDecryptCacheReset = current;
        }
    }
}


void CNetStorageServer::RegisterNetStorageAPIDecryptError(
                                                const string &  message)
{
    m_NeedDecryptCacheReset = true;
    RegisterAlert(eDecryptInNetStorageAPI, message);
}


void CNetStorageServer::ReportNetStorageAPIDecryptSuccess(void)
{
    m_NeedDecryptCacheReset = false;
}


string  CNetStorageServer::x_GenerateGUID(void) const
{
    // Naive implementation of the unique identifier.
    Int8        pid = CProcess::GetCurrentPid();
    Int8        current_time = time(0);

    return NStr::NumericToString((pid << 32) | current_time);
}


void
CNetStorageServer::UpdateBackendConfiguration(const IRegistry &  reg,
                                              vector<string> &  config_warnings)
{
    m_BackendConfiguration = NSTGetBackendConfiguration(reg, this,
                                                        config_warnings);
}


CJsonNode
CNetStorageServer::GetBackendConfDiff(const CJsonNode &  conf) const
{
    vector<string>  added;
    vector<string>  deleted;
    vector<string>  common;
    vector<string>  changed;

    for (CJsonIterator it = m_BackendConfiguration.Iterate(); it; ++it) {
        string      key = it.GetKey();

        if (conf.HasKey(key))
            common.push_back(key);
        else
            deleted.push_back(key);
    }

    for (CJsonIterator it = conf.Iterate(); it; ++it) {
        string      key = it.GetKey();

        if (!m_BackendConfiguration.HasKey(key))
            added.push_back(key);
    }

    // We do not detect what specifically changed in the certain service
    // configuration. We detect only the fact a certain service has changed.
    for (vector<string>::const_iterator  k = common.begin();
            k != common.end(); ++k) {
        string      old_params = m_BackendConfiguration.GetByKey(*k).Repr();
        string      new_params = conf.GetByKey(*k).Repr();

        if (old_params != new_params)
            changed.push_back(*k);
    }

    // Test if any changes detected
    if (added.empty() && deleted.empty() && changed.empty())
        return CJsonNode::NewNullNode();

    // Here: need to build a dictionary with changes
    CJsonNode       diff = CJsonNode::NewObjectNode();

    if (!added.empty()) {
        CJsonNode       added_services = CJsonNode::NewArrayNode();
        for (vector<string>::const_iterator  k = added.begin();
                k != added.end(); ++k)
            added_services.AppendString(*k);
        diff.SetByKey("AddedServices", added_services);
    }

    if (!deleted.empty()) {
        CJsonNode       deleted_services = CJsonNode::NewArrayNode();
        for (vector<string>::const_iterator  k = deleted.begin();
                k != deleted.end(); ++k)
            deleted_services.AppendString(*k);
        diff.SetByKey("DeletedServices", deleted_services);
    }

    if (!changed.empty()) {
        CJsonNode       changed_services = CJsonNode::NewArrayNode();
        for (vector<string>::const_iterator  k = changed.begin();
                k != changed.end(); ++k)
            changed_services.AppendString(*k);
        diff.SetByKey("ChangedServices", changed_services);
    }

    return diff;
}


bool CNetStorageServer::IsAdminClientName(const string &  name) const
{
    set<string>::const_iterator     found;
    CFastMutexGuard                 guard(m_AdminClientNamesLock);

    found = m_AdminClientNames.find(name);
    return found != m_AdminClientNames.end();
}


set<string>
CNetStorageServer::x_GetAdminClientNames(const string &  client_names)
{
    vector<string>      admins;
    NStr::Split(client_names, ";, ", admins,
                NStr::fSplit_MergeDelimiters | NStr::fSplit_Truncate);
    return set<string>(admins.begin(), admins.end());
}


enum EAlertAckResult CNetStorageServer::AcknowledgeAlert(const string &  id,
                                                         const string &  user)
{
    return m_Alerts.Acknowledge(id, user);
}


enum EAlertAckResult CNetStorageServer::AcknowledgeAlert(EAlertType  alert_type,
                                                         const string &  user)
{
    return m_Alerts.Acknowledge(alert_type, user);
}


void CNetStorageServer::RegisterAlert(EAlertType  alert_type,
                                      const string &  message)
{
    m_Alerts.Register(alert_type, message);
}


CJsonNode CNetStorageServer::SerializeAlerts(void) const
{
    return m_Alerts.Serialize();
}


CNSTDatabase &  CNetStorageServer::GetDb(void)
{
    if (m_Db.get())
        return *m_Db;

    m_Db.reset(new CNSTDatabase(this));
    return *m_Db;
}


bool  CNetStorageServer::InMetadataServices(const string &  service) const
{
    return m_MetadataServiceRegistry.IsKnown(service);
}


bool  CNetStorageServer::GetServiceTTL(const string &            service,
                                       TNSTDBValue<CTimeSpan> &  ttl) const
{
    return m_MetadataServiceRegistry.GetTTL(service, ttl);
}


bool
CNetStorageServer::GetServiceProlongOnRead(const string &  service,
                                           CTimeSpan &  prolong_on_read) const
{
    return m_MetadataServiceRegistry.GetProlongOnRead(service, prolong_on_read);
}


bool
CNetStorageServer::GetServiceProlongOnWrite(const string &  service,
                                            CTimeSpan &  prolong_on_write) const
{
    return m_MetadataServiceRegistry.GetProlongOnWrite(service,
                                                       prolong_on_write);
}


bool
CNetStorageServer::GetServiceProperties(const string &  service,
                              CNSTServiceProperties &  service_props) const
{
    return m_MetadataServiceRegistry.GetServiceProperties(service,
                                                          service_props);
}


CJsonNode
CNetStorageServer::ReadMetadataConfiguration(const IRegistry &  reg)
{
    return m_MetadataServiceRegistry.ReadConfiguration(reg);
}


CJsonNode
CNetStorageServer::SerializeMetadataInfo(void) const
{
    return m_MetadataServiceRegistry.Serialize();
}


void
CNetStorageServer::RunServiceThread(void)
{
    m_ServiceThread.Reset(new CNetStorageServiceThread(*this));
    m_ServiceThread->Run();
}


void
CNetStorageServer::StopServiceThread(void)
{
    if (!m_ServiceThread.Empty()) {
        m_ServiceThread->RequestStop();
        m_ServiceThread->Join();
        m_ServiceThread.Reset(0);
    }
}


CJsonNode
CNetStorageServer::x_diffInJson(const vector<string> &  added,
                                const vector<string> &  deleted) const
{
    CJsonNode       diff = CJsonNode::NewObjectNode();

    if (!added.empty()) {
        CJsonNode       add_node = CJsonNode::NewArrayNode();
        for (vector<string>::const_iterator  k = added.begin();
                k != added.end(); ++k)
            add_node.AppendString(*k);
        diff.SetByKey("Added", add_node);
    }

    if (!deleted.empty()) {
        CJsonNode       del_node = CJsonNode::NewArrayNode();
        for (vector<string>::const_iterator  k = deleted.begin();
                k != deleted.end(); ++k)
            del_node.AppendString(*k);
        diff.SetByKey("Deleted", del_node);
    }

    return diff;
}


string  CNetStorageServer::x_GetCrashFlagFileName(void) const
{
    return CFile::MakePath(m_DataPath, kCrashFlagFileName);
}


bool  CNetStorageServer::x_DoesCrashFlagFileExist(void) const
{
    return CFile(x_GetCrashFlagFileName()).Exists();
}


void  CNetStorageServer::CheckStartAfterCrash(void)
{
    if (x_DoesCrashFlagFileExist()) {
        // There could be two reasons:
        // - start after crash
        // - previous instance could not remove the file by some reasons, e.g.
        //   due to changed permissions
        RegisterAlert(eStartAfterCrash,
                      "The server did not stop gracefully last time");

        // Try to remove the file to check the permissions and re-create it
        string      error = RemoveCrashFlagFile();
        if (!error.empty()) {
            ERR_POST(error);
            RegisterAlert(eCrashSignalFileCreation, error);
            return;
        }

        error = CreateCrashFlagFile();
        if (!error.empty()) {
            ERR_POST(error);
            RegisterAlert(eCrashSignalFileCreation, error);
            return;
        }

        // Here: the crash flag file has been successfully re-created
        return;
    }

    // Here: the crash flag file does not exist.
    // Check that the directory exists
    bool    dir_created = true;
    if (!CDirEntry(m_DataPath).Exists()) {
        string  msg = "The data path directory " + m_DataPath +
                      " does not exist. It is OK if it is a first time "
                      "server start. Otherwise someone may have removed the "
                      "directory and this may mask the previous instance "
                      "crash";
        RegisterAlert(eDataPathAbsence, msg);
        ERR_POST(msg);

        // Create the directory recursively
        try {
            CDir        dir(m_DataPath);
            if (dir.CreatePath() == false) {
                msg = "Error creating the data path directory " + m_DataPath;
                RegisterAlert(eCrashSignalFileCreation, msg);
                ERR_POST(msg);
                dir_created = false;
            }
        } catch (const exception &  ex) {
            msg = "Error while creating the data path directory (" +
                  m_DataPath + "): " + ex.what();
            RegisterAlert(eCrashSignalFileCreation, msg);
            ERR_POST(msg);
            dir_created = false;
        } catch (...) {
            msg = "Unknown error while creating the data path directory (" +
                  m_DataPath + ")";
            RegisterAlert(eCrashSignalFileCreation, msg);
            ERR_POST(msg);
            dir_created = false;
        }
    }

    // Create the flag file
    if (dir_created) {
        string      error = CreateCrashFlagFile();
        if (!error.empty()) {
            ERR_POST(error);
            RegisterAlert(eCrashSignalFileCreation, error);
        }
    }
}


string  CNetStorageServer::CreateCrashFlagFile(void)
{
    try {
        CFile   crash_file(x_GetCrashFlagFileName());
        if (!crash_file.Exists()) {
            CFileIO     f;
            f.Open(x_GetCrashFlagFileName(),
                   CFileIO_Base::eCreate,
                   CFileIO_Base::eReadWrite);
            f.Close();
        }
    } catch (const exception &  ex) {
        return "Error while creating the start-after-crash detection "
               "file (" + x_GetCrashFlagFileName() + "): " + ex.what();
    } catch (...) {
        return "Unknown error while creating the start-after-crash "
               "detection file (" + x_GetCrashFlagFileName() + ").";
    }
    return kEmptyStr;
}


string  CNetStorageServer::RemoveCrashFlagFile(void)
{
    try {
        CFile       crash_file(x_GetCrashFlagFileName());
        if (crash_file.Exists())
            crash_file.Remove();
    } catch (const exception &  ex) {
        return "Error while removing the start-after-crash detection "
               "file (" + x_GetCrashFlagFileName() + "): " + ex.what() +
               "\nWhen the server restarts with the same data path it will "
               "set the 'StartAfterCrash' alert";
    } catch (...) {
        return "Unknown error while removing the start-after-crash detection "
               "file (" + x_GetCrashFlagFileName() + "). When the server "
               "restarts with the same data path it will set the "
               "'StartAfterCrash' alert";
    }
    return kEmptyStr;
}


Int8 CNetStorageServer::GetNextObjectID(void)
{
    CFastMutexGuard     guard(m_NextObjectIDLock);

    ++m_LastUsedObjectID;
    if (m_LastUsedObjectID <= m_LastReservedObjectID)
        return m_LastUsedObjectID;

    // All reserved IDs were used, retrieve more

    const Int8  batch_size = 10000;
    GetDb().ExecSP_GetNextObjectID(m_LastUsedObjectID, batch_size);
    m_LastReservedObjectID = m_LastUsedObjectID + batch_size - 1;
    return m_LastUsedObjectID;
}

