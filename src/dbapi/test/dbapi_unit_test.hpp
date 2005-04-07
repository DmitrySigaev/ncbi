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

#include <cppunit/extensions/HelperMacros.h>

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

class CDBAPIUnitTest : public CPPUNIT_NS::TestFixture
{
    CPPUNIT_TEST_SUITE( CDBAPIUnitTest );
    CPPUNIT_TEST( Test_Variant );
    CPPUNIT_TEST( TestGetRowCount );
    CPPUNIT_TEST_SUITE_END();

public:
    CDBAPIUnitTest();

public:
    void setUp();
    void tearDown();

public:

public:
    // Test IStatement interface.

    // Testing Approach for value wrappers
    void Test_Variant(void);

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

    void Test_Bind(void);
    void Test_Execute(void);
    void Test_Procedure(void);

    void Test_Exception(void);
    void Test_Exception_Safety(void);

    // Test scenarios.
    void Create_Destroy(void);
    void Repeated_Usage(void);
    void Single_Value_Writing(void);
    void Bulk_Writing(void);
    void Single_Value_Reading(void);
    void Bulk_Reading(void);
    void Multiple_Resultset(void);
    void Query_Cancelation(void);
    void Error_Conditions(void);
    void Transactional_Behavior(void);

private:
    void SetDatabaseParameters(void);

private:
    typedef map<string, string> TDatabaseParameters;

    CDriverManager& m_DM;
    IDataSource* m_DS;
    auto_ptr<IConnection> m_Conn;
    TDatabaseParameters  m_DatabaseParameters;

private:
    const string m_TableName;
};

class CUnitTestApp : public CNcbiApplication
{
public:
    virtual ~CUnitTestApp(void);

private:
    virtual void Init(void);
    virtual int  Run(void);
    virtual void Exit(void);
};

END_NCBI_SCOPE

#endif  // DBAPI_UNIT_TEST_H

/* ===========================================================================
 *
 * $Log$
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
