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

SNetScheduleAPIImpl::CNetScheduleServerListener::CNetScheduleServerListener(
    const std::string& client_name,
    const std::string& queue_name)
{
    m_Auth = client_name;
    m_Auth += "\r\n";

    m_Auth += queue_name;
}

void SNetScheduleAPIImpl::CNetScheduleServerListener::SetAuthString(
    const std::string& client_name,
    const std::string& program_version,
    const std::string& queue_name)
{
    std::string auth = client_name;
    if (!program_version.empty()) {
        auth += " prog='";
        auth += program_version;
        auth += '\'';
    }
    auth += "\r\n";

    auth += queue_name;

    m_Auth = auth;
}

void SNetScheduleAPIImpl::CNetScheduleServerListener::OnConnected(
    CNetServerConnection::TInstance conn)
{
    conn->WriteLine(m_Auth);
}

void SNetScheduleAPIImpl::CNetScheduleServerListener::OnError(
    const string& err_msg, SNetServerConnectionPoolImpl* pool)
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

CNetScheduleExceptionMap SNetScheduleAPIImpl::sm_ExceptionMap;

CNetScheduleAPI::CNetScheduleAPI(
    const std::string& service_name,
    const std::string& client_name,
    const std::string& queue_name,
    const std::string& lbsm_affinity_name) :
    m_Impl(new SNetScheduleAPIImpl(service_name,
        client_name,
        queue_name,
        lbsm_affinity_name))
{
}

void CNetScheduleAPI::SetProgramVersion(const string& pv)
{
    m_Impl->m_ProgramVersion = pv;
    m_Impl->m_Listener->SetAuthString(
        m_Impl->m_Service.GetClientName(), pv, m_Impl->m_Queue);
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
    case eReturned:    return "Returned";
    case eCanceled:    return "Canceled";
    case eFailed:      return "Failed";
    case eDone:        return "Done";
    case eReading:     return "Reading";
    case eConfirmed:   return "Confirmed";
    case eReadFailed:  return "ReadFailed";
    case eTimeout:     return "Timeout";
    case eReadTimeout: return "ReadTimeout";

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
    if (NStr::CompareNocase(status_str, "Returned") == 0) {
        return eReturned;
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
    if (NStr::CompareNocase(status_str, "Timeout") == 0) {
        return eTimeout;
    }
    if (NStr::CompareNocase(status_str, "ReadTimeout") == 0) {
        return eReadTimeout;
    }


    // check acceptable synonyms

    if (NStr::CompareNocase(status_str, "Pend") == 0) {
        return ePending;
    }
    if (NStr::CompareNocase(status_str, "Run") == 0) {
        return eRunning;
    }
    if (NStr::CompareNocase(status_str, "Return") == 0) {
        return eReturned;
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
    m_Impl->m_Service.DiscoverLowPriorityServers(eOff);
    return new SNetScheduleSubmitterImpl(m_Impl);
}

CNetScheduleExecuter CNetScheduleAPI::GetExecuter()
{
    m_Impl->m_Service.DiscoverLowPriorityServers(eOn);
    return new SNetScheduleExecuterImpl(m_Impl);
}

CNetScheduleAdmin CNetScheduleAPI::GetAdmin()
{
    m_Impl->m_Service.DiscoverLowPriorityServers(eOff);
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
        || status == eCanceled || status == eReturned
        || status == eReading || status == eConfirmed
        || status == eReadFailed) {
        //cerr << str <<endl;
        for ( ;*str && isdigit((unsigned char)(*str)); ++str) {}

        for ( ; *str && isspace((unsigned char)(*str)); ++str) {}

        job.ret_code = atoi(str);

        for ( ;*str && isdigit((unsigned char)(*str)); ++str) {}

        job.output.erase();

        for ( ; *str && isspace((unsigned char)(*str)); ++str) {}

        if (*str && *str == '"') {
            ++str;
            for( ;*str && *str; ++str) {
                if (*str == '"' && *(str-1) != '\\') break;
                job.output.push_back(*str);
            }
            /*
            for( ;*str && *str != '"'; ++str) {
                output->push_back(*str);
            }
            */
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
    string cmd = "STATUS";
    if (GetServerParams().fast_status) {
        if (submitter)  cmd = "SST";
        else cmd = "WST";
    }
    string resp = x_SendJobCmdWaitResponse(cmd, job_key);
    const char* str = resp.c_str();
    int st = atoi(str);
    return (CNetScheduleAPI::EJobStatus) st;
}

const CNetScheduleAPI::SServerParams& SNetScheduleAPIImpl::GetServerParams()
{
    CFastMutexGuard g(m_ServerParamsMutex);

    if (!m_ServerParams.get())
        m_ServerParams.reset(new CNetScheduleAPI::SServerParams);
    else
        if (m_ServerParamsAskCount-- > 0)
            return *m_ServerParams;

    m_ServerParamsAskCount = SERVER_PARAMS_ASK_MAX_COUNT;

    m_ServerParams->max_input_size = kMax_UInt;
    m_ServerParams->max_output_size = kMax_UInt;
    m_ServerParams->fast_status = true;

    TDiscoveredServers servers;

    m_Service->DiscoverServers(servers);

    bool was_called = false;

    ITERATE(TDiscoveredServers, it, servers) {
        CNetServerConnection conn = m_Service->GetConnection(*it);

        was_called = true;

        string resp;
        try {
            resp = conn.Exec("GETP");
        } catch (CNetScheduleException& ex) {
            if (ex.GetErrCode() != CNetScheduleException::eProtocolSyntaxError)
                throw;
        } catch (...) {
        }
        list<string> spars;
        NStr::Split(resp, ";", spars);
        bool fast_status = false;
        ITERATE(list<string>, it, spars) {
            string n,v;
            NStr::SplitInTwo(*it,"=",n,v);
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

///////////////////////////////////////////////////////////////////////////////

const char* kNetScheduleAPIDriverName = "netschedule_api";

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
        auto_ptr<TDriver> drv;
        if (params && (driver.empty() || driver == m_DriverName)) {
            if (version.Match(NCBI_INTERFACE_VERSION(IFace))
                                != CVersionInfo::eNonCompatible) {
                CConfig conf(params);

                string client_name = conf.GetString(m_DriverName,
                    "client_name", CConfig::eErr_Throw, "noname");

                string queue_name = conf.GetString(m_DriverName,
                    "queue_name", CConfig::eErr_Throw, "noname");

                string service = conf.GetString(m_DriverName,
                    "service", CConfig::eErr_NoThrow, "");

                NStr::TruncateSpacesInPlace(service);

                if (!service.empty()) {
                    unsigned int communication_timeout =
                        conf.GetInt(m_DriverName, "communication_timeout",
                            CConfig::eErr_NoThrow, 12);

                    drv.reset(new SNetScheduleAPIImpl(service,
                        client_name, queue_name,
                            conf.GetString(m_DriverName, "use_lbsm_affinity",
                                CConfig::eErr_NoThrow, kEmptyStr)));

                    STimeout tm = {communication_timeout, 0};

                    drv->m_Service.SetCommunicationTimeout(tm);

                    drv->m_Service.SetRebalanceStrategy(
                        CreateSimpleRebalanceStrategy(conf, m_DriverName));
                }
            }
        }
        return drv.release();
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
