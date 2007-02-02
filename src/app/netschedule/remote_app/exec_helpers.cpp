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
 * Authors:  Maxim Didenko
 *
 * File Description:  NetSchedule worker node sample
 *
 */

#include <ncbi_pch.hpp>

#include <corelib/ncbistre.hpp>
#include <corelib/stream_utils.hpp>
#include <corelib/ncbifile.hpp>
#include <corelib/ncbireg.hpp>

#include <connect/ncbi_pipe.hpp>
#include <connect/services/grid_worker.hpp>

#include "exec_helpers.hpp"

BEGIN_NCBI_SCOPE

//////////////////////////////////////////////////////////////////////////////
///
CRemoteAppParams::CRemoteAppParams()
    : m_MaxAppRunningTime(0), m_KeepAlivePeriod(0), 
      m_NonZeroExitAction(eDoneOnNonZeroExit),
      m_RunInSeparateDir(false)
{
}

void CRemoteAppParams::Load(const string& sec_name, const IRegistry& reg)
{
    m_MaxAppRunningTime = 
        reg.GetInt(sec_name,"max_app_run_time",0,0,IRegistry::eReturn);
    
    m_KeepAlivePeriod = 
        reg.GetInt(sec_name,"keep_alive_period",0,0,IRegistry::eReturn);

    if (reg.HasEntry(sec_name, "non_zero_exit_action") ) {
        string val = reg.GetString(sec_name, "non_zero_exit_action", "");
        if (NStr::CompareNocase(val, "fail") == 0 )
            m_NonZeroExitAction = eFailOnNonZeroExit;
        else if (NStr::CompareNocase(val, "return") == 0 )
            m_NonZeroExitAction = eReturnOnNonZeroExit;
        else if (NStr::CompareNocase(val, "done") == 0 )
            m_NonZeroExitAction = eDoneOnNonZeroExit;
        else {
           ERR_POST(Warning << "Unknown parameter value : Section [" 
                     << sec_name << "], param : \"non_zero_exit_action\", value : \""
                     << val << "\". Allowed values: fail, return, done"); 
        }

    } else {
        if (reg.HasEntry(sec_name, "fail_on_non_zero_exit") ) {
            if (reg.GetBool(sec_name, "fail_on_non_zero_exit", false, 0, 
                            CNcbiRegistry::eReturn) )
                m_NonZeroExitAction = eFailOnNonZeroExit;
        }
    }

    m_RunInSeparateDir =
        reg.GetBool(sec_name, "run_in_separate_dir", false, 0, 
                    CNcbiRegistry::eReturn);

    if (m_RunInSeparateDir) {
        m_TempDir = reg.GetString(sec_name, "tmp_path", "." );
        if (!CDirEntry::IsAbsolutePath(m_TempDir)) {
            string tmp = CDir::GetCwd() 
                + CDirEntry::GetPathSeparator() 
                + m_TempDir;
            m_TempDir = CDirEntry::NormalizePath(tmp);
        }
    }

    m_AppPath = reg.GetString(sec_name, "app_path", "" );
    if (!CDirEntry::IsAbsolutePath(m_AppPath)) {
        string tmp = CDir::GetCwd() 
            + CDirEntry::GetPathSeparator() 
            + m_AppPath;
        m_AppPath = CDirEntry::NormalizePath(tmp);
    }

    m_MonitorAppPath = reg.GetString(sec_name, "monitor_app_path", "" );
    if (!m_MonitorAppPath.empty()) {
        if (!CDirEntry::IsAbsolutePath(m_MonitorAppPath)) {
            string tmp = CDir::GetCwd() 
                + CDirEntry::GetPathSeparator() 
                + m_MonitorAppPath;
            m_MonitorAppPath = CDirEntry::NormalizePath(tmp);
            CFile f(m_MonitorAppPath);
            if (!f.Exists() || !CanExecRemoteApp(f) ) {
                ERR_POST("Can not execute \"" << m_MonitorAppPath 
                         << "\". The Monitor application will not run!");
                m_MonitorAppPath = "";
            }
        }
    }

    m_MaxMonitorRunningTime = 
        reg.GetInt(sec_name,"max_monitor_running_time",5,0,IRegistry::eReturn);

    m_MonitorPeriod = 
        reg.GetInt(sec_name,"monitor_period",5,0,IRegistry::eReturn);

}

//////////////////////////////////////////////////////////////////////////////
///

bool CanExecRemoteApp(const CFile& file)
{
    CDirEntry::TMode user_mode  = 0;
    if (!file.GetMode(&user_mode))
        return false;
    if (user_mode & CDirEntry::fExecute)
        return true;
    return false;
}

//////////////////////////////////////////////////////////////////////////////
///
struct STmpDirGuard
{
    STmpDirGuard(const string& path) : m_Path(path) 
    { 
        if (!m_Path.empty()) {
            CDir dir(m_Path); 
            if (!dir.Exists()) 
                dir.CreatePath(); 
        }
    }
    ~STmpDirGuard() { if (!m_Path.empty()) CDir(m_Path).Remove(); }
    const string& m_Path;
};
//////////////////////////////////////////////////////////////////////////////
///
class CPipeProcessWatcher_Base : public CPipe::IProcessWatcher
{
public:
    CPipeProcessWatcher_Base(int max_app_running_time, int app_running_time)
        : m_MaxAppRunningTime(max_app_running_time),
          m_AppRunningTime(app_running_time)
    {
        if (m_MaxAppRunningTime > 0 || m_AppRunningTime > 0)
            m_RunningTime.reset(new CStopWatch(CStopWatch::eStart));        
    }
    
    virtual ~CPipeProcessWatcher_Base() {}

    virtual CPipe::IProcessWatcher::EAction Watch(TProcessHandle /*pid*/) 
    {
        if (m_RunningTime.get()) {
            double elapsed = m_RunningTime->Elapsed();
            if (m_AppRunningTime > 0 &&  elapsed > (double)m_AppRunningTime) {
                ERR_POST( "The application's runtime has exceeded a time("
                        << m_AppRunningTime
                        << " sec) set by the client");
                return CPipe::IProcessWatcher::eStop;
            }
            if ( m_MaxAppRunningTime > 0 && elapsed > (double)m_MaxAppRunningTime) {
                ERR_POST("The application's runtime has exceeded a time("
                         << m_MaxAppRunningTime
                         <<" sec) set in the config file");
                return CPipe::IProcessWatcher::eStop;
            }
        }
        return CPipe::IProcessWatcher::eContinue;
    }
private:
    int m_MaxAppRunningTime;
    int m_AppRunningTime;
    auto_ptr<CStopWatch> m_RunningTime;

};
//////////////////////////////////////////////////////////////////////////////
///
class CRAMonitor
{
public:
    CRAMonitor(const string& app, int max_app_running_time) 
        : m_App(app), m_MaxAppRunningTime(max_app_running_time)  {}

    int Run(vector<string>& args, CNcbiOstream& out, CNcbiOstream& err)
    {
        CPipeProcessWatcher_Base callback(m_MaxAppRunningTime, 0);
        CNcbiStrstream in;
        int exit_value;
        CPipe::EFinish ret = CPipe::eCanceled;
        try {
            ret = CPipe::ExecWait(m_App, args, in, 
                                  out, err, exit_value, 
                                  kEmptyStr, NULL,
                                  &callback);
        } catch( exception& ex ) {
            err << ex.what();
        } catch( ... ) {
            err << "Unknown error";
        }
        if(ret != CPipe::eDone || exit_value > 2)
            return 3;
        return exit_value;
    }

private:
    string m_App;
    int m_MaxAppRunningTime;
};

//////////////////////////////////////////////////////////////////////////////
///
class CPipeProcessWatcher : public CPipeProcessWatcher_Base
{
public:
    CPipeProcessWatcher( CWorkerNodeJobContext& context,
                   int max_app_running_time,
                   int app_running_time,
                   int keep_alive_period)
        : CPipeProcessWatcher_Base(max_app_running_time, app_running_time),
          m_Context(context), m_KeepAlivePeriod(keep_alive_period),
          m_Monitor(NULL)
    {
        if (m_KeepAlivePeriod > 0)
            m_KeepAlive.reset(new CStopWatch(CStopWatch::eStart));

    }
    
    virtual ~CPipeProcessWatcher() {}

    void SetMonitor(CRAMonitor& monitor, int monitor_perod)
    {
        m_Monitor = &monitor;
        m_MonitorPeriod = monitor_perod;
        if (m_MonitorPeriod)
            m_MonitorWatch.reset(new CStopWatch(CStopWatch::eStart));        
        
    }

    

    virtual CPipe::IProcessWatcher::EAction Watch(TProcessHandle pid) 
    {
        if (m_Context.GetShutdownLevel() == 
            CNetScheduleAdmin::eShutdownImmidiate) {
            return CPipe::IProcessWatcher::eStop;
        }

        CPipe::IProcessWatcher::EAction action = CPipeProcessWatcher_Base::Watch(pid);
        if( action != CPipe::IProcessWatcher::eContinue )
            return action;

        if (m_KeepAlive.get() 
            && m_KeepAlive->Elapsed() > (double) m_KeepAlivePeriod ) {
            m_Context.JobDelayExpiration(m_KeepAlivePeriod + 10);
            m_KeepAlive->Restart();
        }
        if (m_Monitor && m_MonitorWatch.get() 
            && m_MonitorWatch->Elapsed() > (double) m_MonitorPeriod) {
            CNcbiStrstream out;
            CNcbiStrstream err;
            vector<string> args;
            args.push_back( "-pid");
            args.push_back( NStr::UInt8ToString((Uint8)pid) );
            args.push_back( "-jid");
            args.push_back( m_Context.GetJobKey() );

            int ret = m_Monitor->Run(args, out, err);
            switch(ret) {
            case 0:
                if( out.pcount() > 0 )
                    out << '\0';
                    m_Context.PutProgressMessage(out.str(), true);
                if( err.pcount() > 0 && m_Context.IsLogRequested()) {
                    x_Log("Info", err);
                }
                break;
            case 1:
                x_Log("job is returned", err);
                return CPipe::IProcessWatcher::eStop;
            case 2: 
                {
                    x_Log("job failed", err);
                    string errmsg;
                    if( out.pcount() > 0 ) {
                        out << '\0';
                        errmsg = out.str();
                    } else 
                        errmsg = "Monitor requested the job termination.";
                    throw runtime_error(errmsg);
                }
                break;
            default:
                x_Log("internal error", err);
                break;
            }
            m_MonitorWatch->Restart();
        }

        return CPipe::IProcessWatcher::eContinue;
    }

private:
    inline void x_Log(const string& what, CNcbiStrstream& sstream) 
    {
        if( sstream.pcount() > 0 ) {
            LOG_POST( CTime(CTime::eCurrent).AsString() 
                      << ": Job " << m_Context.GetJobKey() 
                      << " (Monitor): " << what << ": " << sstream.rdbuf());
        } else {
            LOG_POST( CTime(CTime::eCurrent).AsString() 
                      << ": Job " << m_Context.GetJobKey() 
                      << " (Monitor): " << what << ".");
        }

    }

    CWorkerNodeJobContext& m_Context;
    int m_KeepAlivePeriod;
    auto_ptr<CStopWatch> m_KeepAlive;
    CRAMonitor* m_Monitor;
    auto_ptr<CStopWatch> m_MonitorWatch;
    int m_MonitorPeriod;

};

//////////////////////////////////////////////////////////////////////////////
///
bool ExecRemoteApp(const string& cmd, 
                   const vector<string>& args,
                   CNcbiIstream& in, CNcbiOstream& out, CNcbiOstream& err,
                   int& exit_value,
                   CWorkerNodeJobContext& context,
                   int max_app_running_time,
                   int app_running_time,
                   int keep_alive_period,
                   const string& tmp_path,
                   const char* const env[],
                   const string& monitor_app,
                   int max_monitor_running_time,
                   int monitor_period)
{
    STmpDirGuard guard(tmp_path);

    CPipeProcessWatcher callback(context,
                                 max_app_running_time,
                                 app_running_time,
                                 keep_alive_period);

    auto_ptr<CRAMonitor> ra_monitor;
    if (!monitor_app.empty() && monitor_period > 0) {
        ra_monitor.reset(new CRAMonitor(monitor_app, max_monitor_running_time));
        callback.SetMonitor(*ra_monitor, monitor_period);
    }

    return CPipe::ExecWait(cmd, args, in, out, err, exit_value, 
                           tmp_path, env, &callback) == CPipe::eDone;
}


END_NCBI_SCOPE
