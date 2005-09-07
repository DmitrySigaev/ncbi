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

#ifndef DBAPI_UNIT_TEST_H
#define DBAPI_UNIT_TEST_H

#include <corelib/ncbiapp.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbienv.hpp>

#include <boost/test/unit_test.hpp>

using boost::unit_test_framework::test_suite;


BEGIN_NCBI_SCOPE

// Forward declaration
class CDriverManager;
class IDataSource;
class IConnection;
class IStatement;

enum ETransBehavior { eNoTrans, eTransCommit, eTransRollback };

class CTestTransaction
{
public:
    CTestTransaction(IConnection& conn, ETransBehavior tb = eTransCommit);
    ~CTestTransaction(void);

private:
    IConnection* m_Conn;
    ETransBehavior  m_TransBehavior;
};

///////////////////////////////////////////////////////////////////////////
class CTestArguments
{
public:
    CTestArguments(int argc, char * argv[]);

public:
    typedef map<string, string> TDatabaseParameters;

    enum EServerType {
        eUnknown,   //< Server type is not known
        eSybase,    //< Sybase server
        eMsSql,     //< Microsoft SQL server
        eOracle     //< ORACLE server
    };

    string GetDriverName(void) const
    {
        return m_DriverName;
    }
    
    string GetServerName(void) const
    {
        return m_ServerName;
    }
    
    string GetUserName(void) const
    {
        return m_UserName;
    }
    
    string GetUserPassword(void) const
    {
        return m_UserPassword;
    }

    const TDatabaseParameters& GetDBParameters(void) const
    {
        return m_DatabaseParameters;
    }

    string GetDatabaseName(void) const
    {
        return m_DatabaseName;
    }
    EServerType GetServerType(void) const;
    
    
private:
    void SetDatabaseParameters(void);

private:

    string m_DriverName;
    string m_ServerName;
    string m_UserName;
    string m_UserPassword;
    string m_DatabaseName;
    TDatabaseParameters m_DatabaseParameters;
};


///////////////////////////////////////////////////////////////////////////
class CDBAPIUnitTest
{
public:
    CDBAPIUnitTest(const CTestArguments& args);

public:
    void TestInit(void);

public:
    // Test IStatement interface.

    // Testing Approach for value wrappers
    void Test_Variant(void);
    void Test_Variant2(void);

    // Testing Approach for Members
    // Test particular methods.
    void TestGetRowCount();
    void CheckGetRowCount(
        int row_count, 
        ETransBehavior tb = eNoTrans,
        IStatement* stmt = NULL);
    void CheckGetRowCount2(
        int row_count, 
        ETransBehavior tb = eNoTrans,
        IStatement* stmt = NULL);

    void Test_StatementParameters(void);
    void Test_UserErrorHandler(void);

    void Test_SelectStmt(void);
    void Test_SelectStmtXML(void);
    void Test_Cursor(void);
    void Test_Procedure(void);
    void Test_Bulk_Writing(void);
    void Test_GetColumnNo(void);

public:
    void Test_Exception_Safety(void);
    void Test_ES_01(IConnection& conn);

public:
    // Not implemented yet ...
    void Test_Bind(void);
    void Test_Execute(void);

    void Test_Exception(void);

    // Test scenarios.
    void Create_Destroy(void);
    void Repeated_Usage(void);
    void Single_Value_Writing(void);
    void Single_Value_Reading(void);
    void Bulk_Reading(void);
    void Multiple_Resultset(void);
    void Query_Cancelation(void);
    void Error_Conditions(void);
    void Transactional_Behavior(void);

protected:
    const string& GetTableName(void) const
    {
        return m_TableName;
    }
    static void DumpResults(const auto_ptr<IStatement>& auto_stmt);
    
private:
    const CTestArguments m_args;
    CDriverManager& m_DM;
    IDataSource* m_DS;
    auto_ptr<IConnection> m_Conn;

    const string m_TableName;
    unsigned int m_max_varchar_size;
};

///////////////////////////////////////////////////////////////////////////
struct CDBAPITestSuite : public test_suite
{
    CDBAPITestSuite(const CTestArguments& args);
    ~CDBAPITestSuite(void);
};

END_NCBI_SCOPE

#endif  // DBAPI_UNIT_TEST_H

/* ===========================================================================
 *
 * $Log$
 * Revision 1.24  2005/09/07 11:08:19  ssikorsk
 * Added Test_GetColumnNo to the test-suite
 *
 * Revision 1.23  2005/08/29 16:07:23  ssikorsk
 * Adapted Bulk_Writing for Sybase.
 *
 * Revision 1.22  2005/08/17 18:05:47  ssikorsk
 * Added initial tests for BulkInsert with INT and BIGINT datatypes
 *
 * Revision 1.21  2005/08/15 18:56:56  ssikorsk
 * Added Test_SelectStmtXML to the test-suite
 *
 * Revision 1.20  2005/08/12 15:46:43  ssikorsk
 * Added an initial bulk test to the test suite.
 *
 * Revision 1.19  2005/08/10 16:56:50  ssikorsk
 * Added Test_Variant2 to the test-suite
 *
 * Revision 1.18  2005/08/09 16:09:40  ssikorsk
 * Added the 'Test_Cursor' test to the test-suite
 *
 * Revision 1.17  2005/08/09 13:14:42  ssikorsk
 * Added a 'Test_Procedure' test to the test-suite
 *
 * Revision 1.16  2005/07/11 11:13:02  ssikorsk
 * Added a 'TestSelect' test to the test-suite
 *
 * Revision 1.15  2005/05/12 18:42:57  ssikorsk
 * Improved the "Test_UserErrorHandler" test-case
 *
 * Revision 1.14  2005/05/12 15:33:34  ssikorsk
 * initial version of Test_UserErrorHandler
 *
 * Revision 1.13  2005/05/05 20:28:34  ucko
 * Remove redundant CTestArguments:: when declaring SetDatabaseParameters.
 *
 * Revision 1.12  2005/05/05 15:09:21  ssikorsk
 * Moved from CPPUNIT to Boost.Test
 *
 * Revision 1.11  2005/04/12 18:12:10  ssikorsk
 * Added SetAutoClearInParams and IsAutoClearInParams functions to IStatement
 *
 * Revision 1.10  2005/04/11 14:13:15  ssikorsk
 * Explicitly clean a parameter list after Execute (because of the ctlib driver)
 *
 * Revision 1.9  2005/04/07 20:29:12  ssikorsk
 * Added more dedicated statements to each test
 *
 * Revision 1.8  2005/04/07 19:16:03  ssikorsk
 *
 * Added two different test patterns:
 * 1) New/dedicated statement for each test;
 * 2) Reusable statement for all tests;
 *
 * Revision 1.7  2005/04/07 14:07:16  ssikorsk
 * Added CheckGetRowCount2
 *
 * Revision 1.6  2005/02/16 21:46:40  ssikorsk
 * Improved CVariant test
 *
 * Revision 1.5  2005/02/16 20:01:20  ssikorsk
 * Added CVariant test
 *
 * Revision 1.4  2005/02/15 17:32:29  ssikorsk
 * Added  TDS "version" parameter with database connection
 *
 * Revision 1.3  2005/02/15 16:06:24  ssikorsk
 * Added driver and server parameters to the test-suite (handled via CNcbiApplication)
 *
 * Revision 1.2  2005/02/11 16:12:02  ssikorsk
 * Improved GetRowCount test
 *
 * Revision 1.1  2005/02/04 17:25:02  ssikorsk
 * Renamed dbapi-unit-test to dbapi_unit_test.
 * Added dbapi_unit_test to the test suite.
 *
 * Revision 1.1  2005/02/03 16:06:46  ssikorsk
 * Added: initial version of a cppunit test for the DBAPI
 *
 * ===========================================================================
 */
