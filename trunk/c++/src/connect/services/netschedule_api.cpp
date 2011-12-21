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

#include <connect/ncbi_conn_exception.hpp>

#include <corelib/ncbi_system.hpp>
#include <corelib/plugin_manager_impl.hpp>
#include <corelib/request_control.hpp>

#include <memory>
#include <stdio.h>


BEGIN_NCBI_SCOPE


/**********************************************************************/

void SNetScheduleAPIImpl::CNetScheduleServerListener::SetAuthString(
    SNetScheduleAPIImpl* impl)
{
    string auth(impl->m_Service->MakeAuthString());

    if (!impl->m_ProgramVersion.empty()) {
        auth += " prog=\"";
        auth += impl->m_ProgramVersion;
        auth += '\"';
    }

    if (!impl->m_ClientNode.empty()) {
        auth += " client_node=\"";
        auth += impl->m_ClientNode;
        auth += '\"';
    }

    if (!impl->m_ClientSession.empty()) {
        auth += " client_session=\"";
        auth += impl->m_ClientSession;
        auth += '\"';
    }

    auth += "\r\n";

    auth += impl->m_Queue;

    // Make the auth token look like a command to be able to
    // check for potential authentication/initialization errors
    // like the "queue not found" error.
    if (!m_WorkerNodeCompatMode)
        auth += "\r\nVERSION";

    m_Auth = auth;
}

void SNetScheduleAPIImpl::CNetScheduleServerListener::OnInit(
    CObject* api_impl, CConfig* config, const string& config_section)
{
    SNetScheduleAPIImpl* ns_impl = static_cast<SNetScheduleAPIImpl*>(api_impl);

    if (ns_impl->m_Queue.empty()) {
        if (config == NULL) {
            NCBI_THROW(CConfigException, eParameterMissing,
                "Could not get queue name");
        }
        ns_impl->m_Queue = config->GetString(config_section,
            "queue_name", CConfig::eErr_Throw, "noname");
    }
    if (config == NULL)
        ns_impl->m_UseEmbeddedStorage = true;
    else {
        try {
            ns_impl->m_UseEmbeddedStorage = config->GetBool(config_section,
                "use_embedded_storage", CConfig::eErr_Throw, true);
        }
        catch (CConfigException&) {
            ns_impl->m_UseEmbeddedStorage = config->GetBool(config_section,
                "use_embedded_input", CConfig::eErr_NoThrow, true);
        }
    }

    SetAuthString(ns_impl);
}

void SNetScheduleAPIImpl::CNetScheduleServerListener::OnConnected(
    CNetServerConnection::TInstance conn)
{
    CNetServerConnection conn_object(conn);

    if (!m_WorkerNodeCompatMode)
        conn_object.Exec(m_Auth);
    else
        conn->WriteLine(m_Auth);
}

void SNetScheduleAPIImpl::CNetScheduleServerListener::OnError(
    const string& err_msg, SNetServerImpl* /*server*/)
{
    string code;
    string msg;
    if (NStr::SplitInTwo(err_msg, ":", code, msg)) {
        // Map code into numeric value
        CException::TErrCode n_code = sm_ExceptionMap.GetCode(code);
        if (n_code != CException::eInvalid) {
            NCBI_THROW(CNetScheduleException, EErrCode(n_code), msg);
        }
    }
    NCBI_THROW(CNetServiceException, eCommunicationError, err_msg);
}

const char* const kNetScheduleAPIDriverName = "netschedule_api";

static const char* const s_NetScheduleConfigSections[] = {
    kNetScheduleAPIDriverName,
    NULL
};

static const string s_NetScheduleAPIName("NetScheduleAPI");

SNetScheduleAPIImpl::SNetScheduleAPIImpl(
        CConfig* config, const string& section,
        const string& service_name, const string& client_name,
        const string& queue_name) :
    m_Service(new SNetServiceImpl(s_NetScheduleAPIName,
        client_name, new CNetScheduleServerListener)),
    m_Queue(queue_name),
    m_ServerParamsAskCount(SERVER_PARAMS_ASK_MAX_COUNT)
{
    m_Service->Init(this, service_name,
        config, section, s_NetScheduleConfigSections);
}

CNetScheduleExceptionMap SNetScheduleAPIImpl::sm_ExceptionMap;

CNetScheduleAPI::CNetScheduleAPI(CNetScheduleAPI::EAppRegistry /*use_app_reg*/,
        const string& conf_section /* = kEmptyStr */) :
    m_Impl(new SNetScheduleAPIImpl(NULL, conf_section,
        kEmptyStr, kEmptyStr, kEmptyStr))
{
}

CNetScheduleAPI::CNetScheduleAPI(const IRegistry& reg,
        const string& conf_section)
{
    CConfig conf(reg);
    m_Impl = new SNetScheduleAPIImpl(&conf, conf_section,
        kEmptyStr, kEmptyStr, kEmptyStr);
}

CNetScheduleAPI::CNetScheduleAPI(CConfig* conf, const string& conf_section) :
    m_Impl(new SNetScheduleAPIImpl(conf, conf_section,
        kEmptyStr, kEmptyStr, kEmptyStr))
{
}

CNetScheduleAPI::CNetScheduleAPI(const string& service_name,
        const string& client_name, const string& queue_name) :
    m_Impl(new SNetScheduleAPIImpl(NULL, kEmptyStr,
        service_name, client_name, queue_name))
{
}

void CNetScheduleAPI::SetProgramVersion(const string& pv)
{
    m_Impl->m_ProgramVersion = pv;

    m_Impl->UpdateListener();
}

const string& CNetScheduleAPI::GetProgramVersion() const
{
    return m_Impl->m_ProgramVersion;
}

const string& CNetScheduleAPI::GetQueueName() const
{
    return m_Impl->m_Queue;
}

string CNetScheduleAPI::StatusToString(EJobStatus status)
{
    switch(status) {
    case eJobNotFound: return "NotFound";
    case ePending:     return "Pending";
    case eRunning:     return "Running";
    case eCanceled:    return "Canceled";
    case eFailed:      return "Failed";
    case eDone:        return "Done";
    case eReading:     return "Reading";
    case eConfirmed:   return "Confirmed";
    case eReadFailed:  return "ReadFailed";

    default: _ASSERT(0);
    }
    return kEmptyStr;
}

CNetScheduleAPI::EJobStatus
CNetScheduleAPI::StringToStatus(const string& status_str)
{
    if (NStr::CompareNocase(status_str, "Pending") == 0) {
        return ePending;
    }
    if (NStr::CompareNocase(status_str, "Running") == 0) {
        return eRunning;
    }
    if (NStr::CompareNocase(status_str, "Canceled") == 0) {
        return eCanceled;
    }
    if (NStr::CompareNocase(status_str, "Failed") == 0) {
        return eFailed;
    }
    if (NStr::CompareNocase(status_str, "Done") == 0) {
        return eDone;
    }
    if (NStr::CompareNocase(status_str, "Reading") == 0) {
        return eReading;
    }
    if (NStr::CompareNocase(status_str, "Confirmed") == 0) {
        return eConfirmed;
    }
    if (NStr::CompareNocase(status_str, "ReadFailed") == 0) {
        return eReadFailed;
    }


    // check acceptable synonyms

    if (NStr::CompareNocase(status_str, "Pend") == 0) {
        return ePending;
    }
    if (NStr::CompareNocase(status_str, "Run") == 0) {
        return eRunning;
    }
    if (NStr::CompareNocase(status_str, "Cancel") == 0) {
        return eCanceled;
    }
    if (NStr::CompareNocase(status_str, "Fail") == 0) {
        return eFailed;
    }

    return eJobNotFound;
}

CNetScheduleSubmitter CNetScheduleAPI::GetSubmitter()
{
    return new SNetScheduleSubmitterImpl(m_Impl);
}

CNetScheduleExecuter CNetScheduleAPI::GetExecuter()
{
    return new SNetScheduleExecuterImpl(m_Impl);
}

CNetScheduleAdmin CNetScheduleAPI::GetAdmin()
{
    return new SNetScheduleAdminImpl(m_Impl);
}

CNetService CNetScheduleAPI::GetService()
{
    return m_Impl->m_Service;
}

CNetScheduleAPI::EJobStatus
    CNetScheduleAPI::GetJobDetails(CNetScheduleJob& job)
{

    /// These attributes are not supported yet
    job.progress_msg.erase();
    job.affinity.erase();
    job.mask = 0;
    job.tags.clear();
    ///

    string resp = m_Impl->x_SendJobCmdWaitResponse("STATUS" , job.job_id);

    const char* str = resp.c_str();

    int st = atoi(str);
    EJobStatus status = (EJobStatus) st;

    if (status == eDone || status == eFailed
        || status == eRunning || status == ePending
        || status == eCanceled
        || status == eReading || status == eConfirmed
        || status == eReadFailed) {
        for ( ;*str && !isspace((unsigned char)(*str)); ++str) {}

        for ( ; *str && isspace((unsigned char)(*str)); ++str) {}

        job.ret_code = atoi(str);

        for ( ;*str && !isspace((unsigned char)(*str)); ++str) {}

        job.output.erase();

        for ( ; *str && isspace((unsigned char)(*str)); ++str) {}

        if (*str && *str == '"') {
            ++str;
            for( ;*str && *str; ++str) {
                if (*str == '"' && *(str-1) != '\\') break;
                job.output.push_back(*str);
            }
        }
        job.output = NStr::ParseEscapes(job.output);

        job.input.erase();
        job.error_msg.erase();
        if (!*str)
            return status;

        for (++str; *str && isspace((unsigned char)(*str)); ++str) {}

        if (!*str)
            return status;

        if (*str && *str == '"') {
            ++str;
            for( ;*str && *str; ++str) {
                if (*str == '"' && *(str-1) != '\\') break;
                job.error_msg.push_back(*str);
            }
        }
        job.error_msg = NStr::ParseEscapes(job.error_msg);

        if (!*str)
            return status;

        for (++str; *str && isspace((unsigned char)(*str)); ++str) {}

        if (!*str)
            return status;

        if (*str && *str == '"') {
            ++str;
            for( ;*str && *str; ++str) {
                if (*str == '"' && *(str-1) != '\\') break;
                job.input.push_back(*str);
            }
        }
        job.input = NStr::ParseEscapes(job.input);
    }

    return status;
}

CNetScheduleAPI::EJobStatus SNetScheduleAPIImpl::x_GetJobStatus(
    const string& job_key, bool submitter)
{
    return (CNetScheduleAPI::EJobStatus) atoi(
        x_SendJobCmdWaitResponse(submitter ? "SST" : "WST", job_key).c_str());
}

const CNetScheduleAPI::SServerParams& SNetScheduleAPIImpl::GetServerParams()
{
    CFastMutexGuard g(m_FastMutex);

    if (!m_ServerParams.get())
        m_ServerParams.reset(new CNetScheduleAPI::SServerParams);
    else
        if (m_ServerParamsAskCount-- > 0)
            return *m_ServerParams;

    m_ServerParamsAskCount = SERVER_PARAMS_ASK_MAX_COUNT;

    m_ServerParams->max_input_size = kMax_UInt;
    m_ServerParams->max_output_size = kMax_UInt;
    m_ServerParams->fast_status = true;

    bool was_called = false;

    for (CNetServiceIterator it = m_Service.Iterate(); it; ++it) {
        was_called = true;

        string resp;
        try {
            resp = (*it).ExecWithRetry("GETP").response;
        } catch (CNetScheduleException& ex) {
            if (ex.GetErrCode() != CNetScheduleException::eProtocolSyntaxError)
                throw;
        } catch (...) {
        }
        list<string> spars;
        NStr::Split(resp, ";", spars);
        bool fast_status = false;
        ITERATE(list<string>, param, spars) {
            string n,v;
            NStr::SplitInTwo(*param,"=",n,v);
            if (n == "max_input_size") {
                size_t val = NStr::StringToInt(v) / 4;
                if (m_ServerParams->max_input_size > val)
                    m_ServerParams->max_input_size = val ;
            } else if (n == "max_output_size") {
                size_t val = NStr::StringToInt(v) / 4;
                if (m_ServerParams->max_output_size > val)
                    m_ServerParams->max_output_size = val;
            } else if (n == "fast_status" && v == "1") {
                fast_status = true;
            }
        }
        if (m_ServerParams->fast_status)
            m_ServerParams->fast_status = fast_status;
    }

    if (m_ServerParams->max_input_size == kMax_UInt)
        m_ServerParams->max_input_size = kNetScheduleMaxDBDataSize / 4;
    if (m_ServerParams->max_output_size == kMax_UInt)
        m_ServerParams->max_output_size = kNetScheduleMaxDBDataSize / 4;
    if (!was_called)
        m_ServerParams->fast_status = false;

    return *m_ServerParams;
}

const CNetScheduleAPI::SServerParams& CNetScheduleAPI::GetServerParams()
{
    return m_Impl->GetServerParams();
}

void CNetScheduleAPI::GetProgressMsg(CNetScheduleJob& job)
{
    string resp = m_Impl->x_SendJobCmdWaitResponse("MGET", job.job_id);
    job.progress_msg = NStr::ParseEscapes(resp);
}

static void s_VerifyClientCredentialString(const string& str,
    const CTempString& param_name)
{
    size_t len = str.length();

    if (len == 0) {
        NCBI_THROW_FMT(CConfigException, eParameterMissing,
            "'" << param_name << "' cannot be empty");
    }

    const char* ch = str.data();

    do {
        switch (*ch) {
        case '|':
        case '-':
        case ':':
        case '_':
            break;

        default:
            if ((*ch < 'a' || *ch > 'z') && (*ch < 'A' || *ch > 'Z') &&
                    (*ch < '0' || *ch > '9')) {
                NCBI_THROW_FMT(CConfigException, eParameterMissing,
                    "parameter '" << param_name <<
                        "' contains invalid character(s)");
            }
        }
        ch++;
    } while (--len != 0);
}

void CNetScheduleAPI::SetClientNode(const string& client_node)
{
    s_VerifyClientCredentialString(client_node, "client_node");

    m_Impl->m_ClientNode = client_node;

    m_Impl->UpdateListener();
}

void CNetScheduleAPI::SetClientSession(const string& client_session)
{
    s_VerifyClientCredentialString(client_session, "client_session");

    m_Impl->m_ClientSession = client_session;

    m_Impl->UpdateListener();
}

void CNetScheduleAPI::EnableWorkerNodeCompatMode()
{
    SNetScheduleAPIImpl::CNetScheduleServerListener* listener =
        static_cast<SNetScheduleAPIImpl::CNetScheduleServerListener*>(
            m_Impl->m_Service->m_Listener.GetPointer());

    listener->m_WorkerNodeCompatMode = true;
    listener->SetAuthString(m_Impl);
}

void CNetScheduleAPI::UseOldStyleAuth()
{
    m_Impl->m_Service->m_UseOldStyleAuth = true;

    m_Impl->UpdateListener();
}

///////////////////////////////////////////////////////////////////////////////

/// @internal
class CNetScheduleAPICF : public IClassFactory<SNetScheduleAPIImpl>
{
public:

    typedef SNetScheduleAPIImpl TDriver;
    typedef SNetScheduleAPIImpl IFace;
    typedef IFace TInterface;
    typedef IClassFactory<SNetScheduleAPIImpl> TParent;
    typedef TParent::SDriverInfo TDriverInfo;
    typedef TParent::TDriverList TDriverList;

    /// Construction
    ///
    /// @param driver_name
    ///   Driver name string
    /// @param patch_level
    ///   Patch level implemented by the driver.
    ///   By default corresponds to interface patch level.
    CNetScheduleAPICF(const string& driver_name = kNetScheduleAPIDriverName,
                      int patch_level = -1)
        : m_DriverVersionInfo
        (ncbi::CInterfaceVersion<IFace>::eMajor,
         ncbi::CInterfaceVersion<IFace>::eMinor,
         patch_level >= 0 ?
            patch_level : ncbi::CInterfaceVersion<IFace>::ePatchLevel),
          m_DriverName(driver_name)
    {
        _ASSERT(!m_DriverName.empty());
    }

    /// Create instance of TDriver
    virtual TInterface*
    CreateInstance(const string& driver  = kEmptyStr,
                   CVersionInfo version = NCBI_INTERFACE_VERSION(IFace),
                   const TPluginManagerParamTree* params = 0) const
    {
        if (params && (driver.empty() || driver == m_DriverName) &&
                version.Match(NCBI_INTERFACE_VERSION(IFace)) !=
                    CVersionInfo::eNonCompatible) {
            CConfig config(params);
            return new SNetScheduleAPIImpl(&config, m_DriverName,
                kEmptyStr, kEmptyStr, kEmptyStr);
        }
        return NULL;
    }

    void GetDriverVersions(TDriverList& info_list) const
    {
        info_list.push_back(TDriverInfo(m_DriverName, m_DriverVersionInfo));
    }

protected:
    CVersionInfo  m_DriverVersionInfo;
    string        m_DriverName;
};


void NCBI_XCONNECT_EXPORT NCBI_EntryPoint_xnetscheduleapi(
     CPluginManager<SNetScheduleAPIImpl>::TDriverInfoList&   info_list,
     CPluginManager<SNetScheduleAPIImpl>::EEntryPointRequest method)
{
       CHostEntryPointImpl<CNetScheduleAPICF>::
           NCBI_EntryPointImpl(info_list, method);

}


END_NCBI_SCOPE
