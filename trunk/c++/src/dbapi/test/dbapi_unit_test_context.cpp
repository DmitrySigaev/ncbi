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
 * Author: Sergey Sikorskiy
 *
 * File Description: DBAPI unit-test
 *
 * ===========================================================================
 */

#include <ncbi_pch.hpp>

#include <dbapi/driver/impl/dbapi_driver_utils.hpp>

#include "dbapi_unit_test.hpp"

#include <common/test_assert.h>  /* This header must go last */


BEGIN_NCBI_SCOPE

////////////////////////////////////////////////////////////////////////////////
class CContextThread : public CThread
{
public:
    CContextThread(CDriverManager& dm, CRef<const CTestArguments> args);

protected:
    virtual ~CContextThread(void);
    virtual void* Main(void);
    virtual void  OnExit(void);

    const CTestArguments& GetArgs(void) const
    {
        return *m_Args;
    }

private:
    CRef<const CTestArguments> m_Args;

    CDriverManager&         m_DM;
    IDataSource*            m_DS;
};

///////////////////////////////////////////////////////////////////////////////
CContextThread::CContextThread(CDriverManager& dm,
                               CRef<const CTestArguments> args) :
    m_Args(args),
    m_DM(dm),
    m_DS(NULL)
{
}


CContextThread::~CContextThread(void)
{
}


void*
CContextThread::Main(void)
{
    try {
        m_DS = m_DM.MakeDs(GetArgs().GetConnParams());
        return m_DS;
    } catch (const CDB_ClientEx&) {
        // Ignore it ...
    } catch (...) {
        _ASSERT(false);
    }

    return NULL;
}


void  CContextThread::OnExit(void)
{
    if (m_DS) {
        m_DM.DestroyDs(m_DS);
    }
}


///////////////////////////////////////////////////////////////////////////////
void
CDBAPIUnitTest::Test_DriverContext_One(void)
{
    enum {eNumThreadsMax = 5};
    void* ok;
    int succeeded_num = 0;
    CRef<CContextThread> thr[eNumThreadsMax];

    // Spawn more threads
    for (int i = 0; i < eNumThreadsMax; ++i) {
        thr[i] = new CContextThread(m_DM, m_Args);
        // Allow threads to run even in single thread environment
        thr[i]->Run(CThread::fRunAllowST);
    }

    // Wait for all threads
    for (unsigned int i = 0; i < eNumThreadsMax; ++i) {
        thr[i]->Join(&ok);
        if (ok != NULL) {
            ++succeeded_num;
        }
    }

    BOOST_CHECK(succeeded_num >= 1);
}

///////////////////////////////////////////////////////////////////////////////
void
CDBAPIUnitTest::Test_DriverContext_Many(void)
{
    enum {eNumThreadsMax = 5};
    void* ok;
    int succeeded_num = 0;
    CRef<CContextThread> thr[eNumThreadsMax];

    // Spawn more threads
    for (int i = 0; i < eNumThreadsMax; ++i) {
        thr[i] = new CContextThread(m_DM, m_Args);
        // Allow threads to run even in single thread environment
        thr[i]->Run(CThread::fRunAllowST);
    }

    // Wait for all threads
    for (unsigned int i = 0; i < eNumThreadsMax; ++i) {
        thr[i]->Join(&ok);
        if (ok != NULL) {
            ++succeeded_num;
        }
    }

    BOOST_CHECK_EQUAL(succeeded_num, int(eNumThreadsMax));
}

////////////////////////////////////////////////////////////////////////////////
void CDBAPIUnitTest::Test_SetLogStream(void)
{
    try {
        CNcbiOfstream logfile("dbapi_unit_test.log");

        GetDS().SetLogStream(&logfile);
        GetDS().SetLogStream(&logfile);

        // Test block ...
        {
            // No errors ...
            {
                auto_ptr<IConnection> auto_conn(GetDS().CreateConnection());
                BOOST_CHECK(auto_conn.get() != NULL);

                auto_conn->Connect(GetArgs().GetConnParams());

                auto_ptr<IStatement> auto_stmt(auto_conn->GetStatement());
                auto_stmt->SendSql("SELECT name FROM sysobjects");
                BOOST_CHECK(auto_stmt->HasMoreResults());
                DumpResults(auto_stmt.get());
            }

            GetDS().SetLogStream(&logfile);

            // Force errors ...
            {
                auto_ptr<IConnection> auto_conn(GetDS().CreateConnection());
                BOOST_CHECK(auto_conn.get() != NULL);

                auto_conn->Connect(GetArgs().GetConnParams());

                auto_ptr<IStatement> auto_stmt(auto_conn->GetStatement());
                try {
                    auto_stmt->SendSql("SELECT oops FROM sysobjects");
                    BOOST_CHECK( auto_stmt->HasMoreResults() );
                } catch(const CDB_Exception&) {
                    auto_stmt->Cancel();
                }
            }

            GetDS().SetLogStream(&logfile);
        }

        // Install user-defined error handler (eTakeOwnership)
        {
            {
                I_DriverContext* drv_context = GetDS().GetDriverContext();

                if (GetArgs().IsODBCBased()) {
                    drv_context->PushCntxMsgHandler(new CDB_UserHandler_Exception_ODBC,
                                                    eTakeOwnership
                                                    );
                    drv_context->PushDefConnMsgHandler(new CDB_UserHandler_Exception_ODBC,
                                                       eTakeOwnership
                                                       );
                } else {
                    CRef<CDB_UserHandler> hx(new CDB_UserHandler_Exception);
                    CDB_UserHandler::SetDefault(hx);
                    drv_context->PushCntxMsgHandler(new CDB_UserHandler_Exception,
                                                    eTakeOwnership);
                    drv_context->PushDefConnMsgHandler(new CDB_UserHandler_Exception,
                                                       eTakeOwnership);
                }
            }

            // Test block ...
            {
                // No errors ...
                {
                    auto_ptr<IConnection> auto_conn(GetDS().CreateConnection());
                    BOOST_CHECK(auto_conn.get() != NULL);

                    auto_conn->Connect(GetArgs().GetConnParams());

                    auto_ptr<IStatement> auto_stmt(auto_conn->GetStatement());
                    auto_stmt->SendSql("SELECT name FROM sysobjects");
                    BOOST_CHECK(auto_stmt->HasMoreResults());
                    DumpResults(auto_stmt.get());
                }

                GetDS().SetLogStream(&logfile);

                // Force errors ...
                {
                    auto_ptr<IConnection> auto_conn(GetDS().CreateConnection());
                    BOOST_CHECK(auto_conn.get() != NULL);

                    auto_conn->Connect(GetArgs().GetConnParams());

                    auto_ptr<IStatement> auto_stmt(auto_conn->GetStatement());
                    try {
                        auto_stmt->SendSql("SELECT oops FROM sysobjects");
                        BOOST_CHECK( auto_stmt->HasMoreResults() );
                    } catch(const CDB_Exception&) {
                        auto_stmt->Cancel();
                    }
                }

                GetDS().SetLogStream(&logfile);
            }
        }

        // Install user-defined error handler (eNoOwnership)
        {
            CMsgHandlerGuard handler_guard(*GetDS().GetDriverContext());

            // Test block ...
            {
                // No errors ...
                {
                    auto_ptr<IConnection> auto_conn(GetDS().CreateConnection());
                    BOOST_CHECK(auto_conn.get() != NULL);

                    auto_conn->Connect(GetArgs().GetConnParams());

                    auto_ptr<IStatement> auto_stmt(auto_conn->GetStatement());
                    auto_stmt->SendSql("SELECT name FROM sysobjects");
                    BOOST_CHECK(auto_stmt->HasMoreResults());
                    DumpResults(auto_stmt.get());
                }

                GetDS().SetLogStream(&logfile);

                // Force errors ...
                {
                    auto_ptr<IConnection> auto_conn(GetDS().CreateConnection());
                    BOOST_CHECK(auto_conn.get() != NULL);

                    auto_conn->Connect(GetArgs().GetConnParams());

                    auto_ptr<IStatement> auto_stmt(auto_conn->GetStatement());
                    try {
                        auto_stmt->SendSql("SELECT oops FROM sysobjects");
                        BOOST_CHECK( auto_stmt->HasMoreResults() );
                    } catch(const CDB_Exception&) {
                        auto_stmt->Cancel();
                    }
                }

                GetDS().SetLogStream(&logfile);
            }
        }
    }
    catch(const CException& ex) {
        DBAPI_BOOST_FAIL(ex);
    }
}



END_NCBI_SCOPE

