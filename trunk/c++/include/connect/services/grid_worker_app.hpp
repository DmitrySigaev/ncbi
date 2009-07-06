#ifndef CONNECT_SERVICES__GRID_WORKER_APP_HPP
#define CONNECT_SERVICES__GRID_WORKER_APP_HPP


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
 * Authors:  Maxim Didenko, Anatoliy Kuznetsov, Dmitry Kazimirov
 *
 * File Description:
 *   NetSchedule Worker Node Framework Application.
 *
 */

/// @file grid_worker_app.hpp
/// NetSchedule worker node application.
///


#include <connect/services/grid_worker.hpp>
#include <connect/services/blob_storage_netcache.hpp>

BEGIN_NCBI_SCOPE

/** @addtogroup NetScheduleClient
 *
 * @{
 */

/// Default implementation of a worker node initialization
/// interface
class CDefaultWorkerNodeInitContext : public IWorkerNodeInitContext
{
public:
    CDefaultWorkerNodeInitContext(CNcbiApplication& app)
        : m_App(app)
    {}

    virtual ~CDefaultWorkerNodeInitContext() {}

    virtual const IRegistry&        GetConfig() const
    { return m_App.GetConfig(); }

    virtual const CArgs&            GetArgs() const
    { return m_App.GetArgs(); }

    virtual const CNcbiEnvironment& GetEnvironment() const
    { return m_App.GetEnvironment(); }

    virtual IWorkerNodeCleanupEventSource* GetCleanupEventSource() const;

private:
    CNcbiApplication& m_App;

    CDefaultWorkerNodeInitContext(const CDefaultWorkerNodeInitContext&);
    CDefaultWorkerNodeInitContext& operator=(const CDefaultWorkerNodeInitContext&);
};

/// Listener of events generated by CGridWorkerNodeApp.
/// @see CGridWorkerApp::SetListener().
class IGridWorkerNodeApp_Listener
{
public:
    /// Notify that CGridWorkerNode::Run() is about to be executed.
    /// This method can be overridden to implement worker node-
    /// specific initialization.
    virtual void OnGridWorkerStart() = 0;

    /// Notify that CGridWorkerNode::Run() has just finished.
    virtual void OnGridWorkerStop() = 0;

    virtual ~IGridWorkerNodeApp_Listener() {}
};

/// An adapter class for IGridWorkerNodeApp_Listener.
class CGridWorkerNodeApp_Listener : public IGridWorkerNodeApp_Listener
{
    virtual void OnGridWorkerStart() {}
    virtual void OnGridWorkerStop() {}
};

/// Main Worker Node application
///
/// @note
/// Worker node application is parameterized using INI file settings.
/// Please read the sample ".ini" file for more information.
///
class NCBI_XCONNECT_EXPORT CGridWorkerApp : public CNcbiApplication
{
public:

    enum ESignalHandling {
        eStandardSignalHandling = 0,
        eManualSignalHandling
    };

    CGridWorkerApp(IWorkerNodeJobFactory* job_factory,
                   IBlobStorageFactory* storage_factory = NULL,
                   INetScheduleClientFactory* client_factory = NULL,
                   ESignalHandling signal_handling = eStandardSignalHandling);

    CGridWorkerApp(IWorkerNodeJobFactory* job_factory,
                   const CVersionInfo& version_info);

    /// Register a listener of events of this class.
    ///
    /// @note
    /// This object takes ownership over the listener object
    /// and will automatically 'delete' it.
    ///
    /// @param listener A pointer to the listener object.
    ///   NULL can be passed to stop listening and reset the
    ///   internal pointer to the default handler.
    void SetListener(IGridWorkerNodeApp_Listener* listener);

    /// Sets the default logging format for this application.
    /// Can be overridden by the [log]/merge_lines configuration
    /// parameter.  This method must be called before the Init()
    /// method is invoked, otherwise it will have no effect.
    ///
    /// @param merge_log_lines If true, multiple log lines will
    ///        be concatenated and separated by a semicolon.
    ///        If false, the lines are not concatenated.
    void SetMergeLogLines(bool merge_log_lines = true);

    /// If you override this method, do call CGridWorkerApp::Init()
    /// from inside of your overriding method.
    virtual void Init(void);

    /// Do not override this method yourself! It includes all the Worker Node
    /// specific machinery. If you override it, do call CGridWorkerApp::Run()
    /// from inside your overriding method.
    virtual int Run(void);

    virtual void SetupArgDescriptions(CArgDescriptions* arg_desc);

    void RequestShutdown();

    CGridWorkerNode* GetWorkerNode() const {return m_WorkerNode.get();}

protected:

    const IWorkerNodeInitContext& GetInitContext();

private:
    void Construct(IWorkerNodeJobFactory* job_factory,
        IBlobStorageFactory* storage_factory = NULL,
        INetScheduleClientFactory* client_factory = NULL,
        ESignalHandling signal_handling = eStandardSignalHandling);

    auto_ptr<IWorkerNodeInitContext> m_WorkerNodeInitContext;

    auto_ptr<CGridWorkerNode> m_WorkerNode;

    bool m_MergeLogLines;

    CGridWorkerApp(const CGridWorkerApp&);
    CGridWorkerApp& operator=(const CGridWorkerApp&);
};

inline void CGridWorkerApp::SetListener(IGridWorkerNodeApp_Listener* listener)
{
    m_WorkerNode->SetListener(listener);
}

inline void CGridWorkerApp::SetMergeLogLines(bool merge_log_lines /* = true*/)
{
    m_MergeLogLines = merge_log_lines;
}

#define NCBI_WORKERNODE_MAIN(TWorkerNodeJob, Version)                       \
    NCBI_DECLARE_WORKERNODE_FACTORY(TWorkerNodeJob, Version);               \
    int main(int argc, const char* argv[])                                  \
    {                                                                       \
        GetDiagContext().SetOldPostFormat(false);                           \
        BlobStorage_RegisterDriver_NetCache();                              \
        CGridWorkerApp app(new TWorkerNodeJob##Factory,                     \
            CVersionInfo(#Version));                                        \
        return app.AppMain(argc, argv, NULL, eDS_ToStdlog);                 \
    }

#define NCBI_WORKERNODE_MAIN_EX(TWorkerNodeJob,                             \
        TWorkerNodeIdleTask, Version)                                       \
    NCBI_DECLARE_WORKERNODE_FACTORY_EX(TWorkerNodeJob,                      \
            TWorkerNodeIdleTask, Version);                                  \
    int main(int argc, const char* argv[])                                  \
    {                                                                       \
        GetDiagContext().SetOldPostFormat(false);                           \
        BlobStorage_RegisterDriver_NetCache();                              \
        CGridWorkerApp app(new TWorkerNodeJob##FactoryEx,                   \
            CVersionInfo(#Version));                                        \
        return app.AppMain(argc, argv, NULL, eDS_ToStdlog);                 \
    }

#define NCBI_WORKERNODE_MAIN_MERGE_LOG_LINES(TWorkerNodeJob,                \
        TWorkerNodeIdleTask, Version)                                       \
    NCBI_DECLARE_WORKERNODE_FACTORY_EX(TWorkerNodeJob,                      \
            TWorkerNodeIdleTask, Version);                                  \
    int main(int argc, const char* argv[])                                  \
    {                                                                       \
        GetDiagContext().SetOldPostFormat(false);                           \
        BlobStorage_RegisterDriver_NetCache();                              \
        CGridWorkerApp app(new TWorkerNodeJob##FactoryEx,                   \
            CVersionInfo(#Version));                                        \
        app.SetMergeLogLines();                                             \
        return app.AppMain(argc, argv, NULL, eDS_ToStdlog);                 \
    }

/* @} */


END_NCBI_SCOPE

#endif //CONNECT_SERVICES__GRID_WORKER_APP_HPP
