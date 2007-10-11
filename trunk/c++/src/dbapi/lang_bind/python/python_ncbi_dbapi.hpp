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
* File Description:
* Status: *Initial*
*
* ===========================================================================
*/

#include <dbapi/dbapi.hpp>
#include <set>

#include "pythonpp/pythonpp_ext.hpp"
#include "pythonpp/pythonpp_dict.hpp"

BEGIN_NCBI_SCOPE

namespace python
{

//////////////////////////////////////////////////////////////////////////////
// eStandardMode stands for Python DB API specification mode
// as defined in http://www.python.org/peps/pep-0249.html
// eSimpleMode is a simplified mode as it is supported by the
// NCBI DBAPI. The eSimpleMode was introduced by a request.
enum EConnectionMode { eSimpleMode, eStandardMode };

//////////////////////////////////////////////////////////////////////////////
class CBinary : public pythonpp::CExtObject<CBinary>
{
public:
    CBinary(void);
    ~CBinary(void);
};

//////////////////////////////////////////////////////////////////////////////
class CNumber : public pythonpp::CExtObject<CNumber>
{
public:
    CNumber(void);
    ~CNumber(void);
};

//////////////////////////////////////////////////////////////////////////////
class CRowID : public pythonpp::CExtObject<CRowID>
{
public:
    CRowID(void);
    ~CRowID(void);
};

////////////////////////////////////////////////////////////////////////////////
enum EStatementType {
    estNone,
    estSelect,
    estInsert,
    estDelete,
    estUpdate,
    estCreate,
    estDrop,
    estAlter,
    estFunction,
    estTransaction
    };

EStatementType
RetrieveStatementType(const string& stmt, EStatementType default_type = estNone);

pythonpp::CTuple MakeTupleFromResult(IResultSet& rs);

////////////////////////////////////////////////////////////////////////////////
class CParamFmt
{
public:
    enum TFormat {eTSQL, eQmark, eNumeric, eNamed, eFormat, ePyFormat};

    CParamFmt(TFormat user_fmt = eTSQL, TFormat drv_fmt = eTSQL);

public:
    TFormat GetUserFmt(void) const
    {
        return m_UserFmt;
    }
    TFormat GetDriverFmt(void) const
    {
        return m_DrvFmt;
    }
    static const char* GetName(TFormat fmt);

private:
    TFormat m_UserFmt;
    TFormat m_DrvFmt;
};

////////////////////////////////////////////////////////////////////////////////
class CStmtStr
{
public:
    CStmtStr(void)
    : m_StmType(estNone)
    {
    }
    CStmtStr(const string& str,
             EStatementType default_type = estSelect,
             const CParamFmt& fmt = CParamFmt()
             )
    : m_StmType(estNone)
    {
        SetStr(str, default_type, fmt);
    }

    // We will accume that SQL has type estFunction if it is
    // hard to get an actual type.
    void SetStr(const string& str,
                EStatementType default_type = estSelect,
                const CParamFmt& fmt = CParamFmt()
                );

public:
    string GetStr(void) const
    {
        return m_StmtStr;
    }
    EStatementType GetType(void) const
    {
        return m_StmType;
    }

private:
    string::size_type find_numeric(const string& str,
                                   string::size_type offset,
                                   int& param_len
                                   );
    string::size_type find_named(const string& str,
                                 string::size_type offset,
                                 int& param_len
                                 );
    string::size_type find_TSQL(const string& str,
                                string::size_type offset,
                                int& param_len
                                );

private:
    string          m_StmtStr;
    EStatementType  m_StmType;
};

//////////////////////////////////////////////////////////////////////////////
// Forward declaration ...
class CConnection;
class CTransaction;

//////////////////////////////////////////////////////////////////////////////
// IStatement plus additinal informaion ...
class CStmtHelper
{
public:
    CStmtHelper(CTransaction* trans);
    CStmtHelper(CTransaction* trans, const CStmtStr& stmt);
    ~CStmtHelper(void);

public:
    // void SetStr(const string& stmt, EStatementType default_type = estFunction);
    void SetStr(const CStmtStr& stmt);
    void SetParam(const string& name, const CVariant& value);

    void Execute(void);
    void Close(void);
    long GetRowCount(void) const;

    bool NextRS(void);
    IResultSet& GetRS(void);
    const IResultSet& GetRS(void) const;
    bool HasRS(void) const;

    int GetReturnStatus(void);

private:
    void DumpResult(void);
    void ReleaseStmt(void);
    void CreateStmt(void);

private:
    CTransaction* const     m_ParentTransaction; //< A transaction to which belongs this cursor object
    auto_ptr<IStatement>    m_Stmt;     //< DBAPI SQL statement interface
    IResultSet*             m_RS;
    CStmtStr                m_StmtStr;
    bool                    m_Executed;
    int                     m_ResultStatus;
    bool                    m_ResultStatusAvailable;
};

//////////////////////////////////////////////////////////////////////////////
// ICallableStatement plus additinal informaion ...
class CCallableStmtHelper
{
public:
    CCallableStmtHelper(CTransaction* trans);
    CCallableStmtHelper(CTransaction* trans, const CStmtStr& stmt, int num_arg);
    ~CCallableStmtHelper(void);

public:
    // void SetStr(const string& stmt, int num_arg, EStatementType default_type = estFunction);
    void SetStr(const CStmtStr& stmt, int num_arg);
    void SetParam(const string& name, const CVariant& value);

    void Execute(void);
    void Close(void);
    long GetRowCount(void) const;

    bool NextRS(void);
    IResultSet& GetRS(void);
    const IResultSet& GetRS(void) const;
    bool HasRS(void) const;

    int GetReturnStatus(void);

private:
    void DumpResult(void);
    void ReleaseStmt(void);
    void CreateStmt(void);

private:
    CTransaction* const             m_ParentTransaction; //< A transaction to which belongs this cursor object
    int                             m_NumOfArgs;         //< Number of arguments in a callable statement
    auto_ptr<ICallableStatement>    m_Stmt;     //< DBAPI SQL statement interface
    IResultSet*                     m_RS;
    CStmtStr                        m_StmtStr;
    bool                            m_Executed;
    int                             m_ResultStatus;
    bool                            m_ResultStatusAvailable;
};

//////////////////////////////////////////////////////////////////////////////
// Cursor borrows connections from a parent transaction.
class CCursor : public pythonpp::CExtObject<CCursor>
{
    friend class CTransaction;

protected:
    CCursor(CTransaction* trans);

public:
    ~CCursor(void);

public:
    // Python methods ...

    /// Call a stored database procedure with the given name. The
    /// sequence of parameters must contain one entry for each
    /// argument that the procedure expects. The result of the
    /// call is returned as modified copy of the input
    /// sequence. Input parameters are left untouched, output and
    /// input/output parameters replaced with possibly new values.
    /// callproc(procname[,parameters]);
    pythonpp::CObject callproc(const pythonpp::CTuple& args);
    /// Close the cursor now (rather than whenever __del__ is
    /// called).  The cursor will be unusable from this point
    /// forward; an Error (or subclass) exception will be raised
    /// if any operation is attempted with the cursor.
    /// close();
    pythonpp::CObject close(const pythonpp::CTuple& args);
    /// Prepare and execute a database operation (query or
    /// command).  Parameters may be provided as sequence or
    /// mapping and will be bound to variables in the operation.
    /// Variables are specified in a database-specific notation
    /// (see the module's paramstyle attribute for details). [5]
    /// execute(operation[,parameters]);
    pythonpp::CObject execute(const pythonpp::CTuple& args);
    /// Prepare a database operation (query or command) and then
    /// execute it against all parameter sequences or mappings
    /// found in the sequence seq_of_parameters.
    /// executemany(operation,seq_of_parameters);
    pythonpp::CObject executemany(const pythonpp::CTuple& args);
    /// Fetch the next row of a query result set, returning a
    /// single sequence, or None when no more data is
    /// available. [6]
    /// An Error (or subclass) exception is raised if the previous
    /// call to executeXXX() did not produce any result set or no
    /// call was issued yet.
    /// fetchone();
    pythonpp::CObject fetchone(const pythonpp::CTuple& args);
    /// Fetch the next set of rows of a query result, returning a
    /// sequence of sequences (e.g. a list of tuples). An empty
    /// sequence is returned when no more rows are available.
    /// The number of rows to fetch per call is specified by the
    /// parameter.  If it is not given, the cursor's arraysize
    /// determines the number of rows to be fetched. The method
    /// should try to fetch as many rows as indicated by the size
    /// parameter. If this is not possible due to the specified
    /// number of rows not being available, fewer rows may be
    /// returned.
    /// An Error (or subclass) exception is raised if the previous
    /// call to executeXXX() did not produce any result set or no
    /// call was issued yet.
    /// fetchmany([size=cursor.arraysize]);
    pythonpp::CObject fetchmany(const pythonpp::CTuple& args);
    /// Fetch all (remaining) rows of a query result, returning
    /// them as a sequence of sequences (e.g. a list of tuples).
    /// Note that the cursor's arraysize attribute can affect the
    /// performance of this operation.
    /// An Error (or subclass) exception is raised if the previous
    /// call to executeXXX() did not produce any result set or no
    /// call was issued yet.
    /// fetchall();
    pythonpp::CObject fetchall(const pythonpp::CTuple& args);
    /// This method will make the cursor skip to the next
    /// available set, discarding any remaining rows from the
    /// current set.
    /// If there are no more sets, the method returns
    /// None. Otherwise, it returns a true value and subsequent
    /// calls to the fetch methods will return rows from the next
    /// result set.
    /// An Error (or subclass) exception is raised if the previous
    /// call to executeXXX() did not produce any result set or no
    /// call was issued yet.
    /// nextset();
    pythonpp::CObject nextset(const pythonpp::CTuple& args);
    /// This can be used before a call to executeXXX() to
    /// predefine memory areas for the operation's parameters.
    /// sizes is specified as a sequence -- one item for each
    /// input parameter.  The item should be a Type Object that
    /// corresponds to the input that will be used, or it should
    /// be an integer specifying the maximum length of a string
    /// parameter.  If the item is None, then no predefined memory
    /// area will be reserved for that column (this is useful to
    /// avoid predefined areas for large inputs).
    /// This method would be used before the executeXXX() method
    /// is invoked.
    /// setinputsizes(sizes);
    pythonpp::CObject setinputsizes(const pythonpp::CTuple& args);
    /// Set a column buffer size for fetches of large columns
    /// (e.g. LONGs, BLOBs, etc.).  The column is specified as an
    /// index into the result sequence.  Not specifying the column
    /// will set the default size for all large columns in the
    /// cursor.
    /// This method would be used before the executeXXX() method
    /// is invoked.
    /// setoutputsize(size[,column]);
    pythonpp::CObject setoutputsize(const pythonpp::CTuple& args);
    //
    pythonpp::CObject get_proc_return_status(const pythonpp::CTuple& args);

private:
    CTransaction& GetTransaction(void)
    {
        return *m_ParentTransaction;
    }

private:
    void CloseInternal(void);
    bool NextSetInternal(void);

private:
    CVariant GetCVariant(const pythonpp::CObject& obj) const;

    void SetupParameters(const pythonpp::CDict& dict, CStmtHelper& stmt);
    void SetupParameters(const pythonpp::CDict& dict, CCallableStmtHelper& stmt);

    void ExecuteCurrStatement(void);

    static bool isDML(EStatementType stmtType)
    {
        return stmtType == estInsert || stmtType == estDelete || stmtType == estUpdate;
    }
    static bool isDDL(EStatementType stmtType)
    {
        return stmtType == estCreate || stmtType == estDrop || stmtType == estAlter;
    }

private:
    pythonpp::CObject       m_PythonConnection;  //< For reference counting purposes only
    pythonpp::CObject       m_PythonTransaction; //< For reference counting purposes only
    CTransaction*           m_ParentTransaction; //< A transaction to which belongs this cursor object
    int                     m_NumOfArgs;         //< Number of arguments in a callable statement
    long                    m_RowsNum;
    IResultSet*             m_RS;
    size_t                  m_ArraySize;
    CStmtStr                m_StmtStr;
    CStmtHelper             m_StmtHelper;
    CCallableStmtHelper     m_CallableStmtHelper;
    bool                    m_AllDataFetched;
    bool                    m_AllSetsFetched;
};

//////////////////////////////////////////////////////////////////////////////
typedef set<CCursor*>           TCursorList;
typedef set<IConnection*>       TConnectionList;

//////////////////////////////////////////////////////////////////////////////
class CSelectConnPool
{
public:
    CSelectConnPool(CTransaction* trans, size_t size = 3);

public:
    IConnection* Create(void);
    void Destroy(IConnection* db_conn);
    void Clear(void);
    // Means that nobody uses connections from the pool.
    bool Empty(void) const
    {
        return ((m_ConnList.size() - m_ConnPool.size()) == 0);
    }

private:
    const CConnection& GetConnection(void) const;
    CConnection& GetConnection(void);

private:
    CTransaction*   m_Transaction;
    const size_t    m_PoolSize;
    TConnectionList m_ConnPool; //< A pool of connection for SELECT statements
    TConnectionList m_ConnList; //< A list of all allocated connection for SELECT statements
};

//////////////////////////////////////////////////////////////////////////////

// eImplicitTrans meant that a transaction will be started automaticaly,
// without your help.
// eExplicitTrans means that you have to start a transaction manualy by calling
// the "BEGIN TRANSACTION" statement.
enum ETransType { eImplicitTrans, eExplicitTrans };

// A pool with one connection only ...
// Strange but useful ...
class CDMLConnPool
{
public:
    CDMLConnPool( CTransaction* trans, ETransType trans_type = eImplicitTrans );

public:
    IConnection* Create(void);
    void Destroy(IConnection* db_conn);
    void Clear(void);
    bool Empty(void) const
    {
        return (m_NumOfActive == 0);
    }

public:
    void commit(void) const;
    void rollback(void) const;

private:
    const CConnection& GetConnection(void) const;
    CConnection& GetConnection(void);

    IStatement& GetLocalStmt(void) const;

private:
    CTransaction*           m_Transaction;
    auto_ptr<IConnection>   m_DMLConnection;  //< Transaction has only one DML connection
    size_t                  m_NumOfActive;
    auto_ptr<IStatement>    m_LocalStmt;      //< DBAPI SQL statement interface
    bool                    m_Started;
    const ETransType        m_TransType;
};

//////////////////////////////////////////////////////////////////////////////
class CTransaction : public pythonpp::CExtObject<CTransaction>
{
    friend class CConnection;
    friend class CSelectConnPool;
    friend class CDMLConnPool;

protected:
    CTransaction(
        CConnection* conn,
        pythonpp::EOwnershipFuture ownnership = pythonpp::eOwned,
        EConnectionMode conn_mode = eSimpleMode
        );

public:
    ~CTransaction(void);

public:
    pythonpp::CObject close(const pythonpp::CTuple& args);
    pythonpp::CObject cursor(const pythonpp::CTuple& args);
    pythonpp::CObject commit(const pythonpp::CTuple& args);
    pythonpp::CObject rollback(const pythonpp::CTuple& args);

public:
    // CCursor factory interface ...
    CCursor* CreateCursor(void);
    void DestroyCursor(CCursor* cursor);

public:
    // Factory for DML connections (actualy, only one connection)
    IConnection* CreateDMLConnection(void)
    {
        return m_DMLConnPool.Create();
    }
    void DestroyDMLConnection(IConnection* db_conn)
    {
        m_DMLConnPool.Destroy(db_conn);
    }

public:
    // Factory for "data-retrieval" connections
    IConnection* CreateSelectConnection(void);
    void DestroySelectConnection(IConnection* db_conn);

public:
    const CConnection& GetParentConnection(void) const
    {
        return *m_ParentConnection;
    }
    CConnection& GetParentConnection(void)
    {
        return *m_ParentConnection;
    }

protected:
    void CloseInternal(void);
    void CommitInternal(void) const
    {
        m_DMLConnPool.commit();
    }
    void RollbackInternal(void) const
    {
        m_DMLConnPool.rollback();
    }
    void CloseOpenCursors(void);

private:
    pythonpp::CObject       m_PythonConnection; //< For reference counting purposes only
    CConnection*            m_ParentConnection; //< A connection to which belongs this transaction object
    TCursorList             m_CursorList;

    CDMLConnPool            m_DMLConnPool;    //< A pool of connections for DML statements
    CSelectConnPool         m_SelectConnPool; //< A pool of connections for SELECT statements
    const EConnectionMode   m_ConnectionMode;
};

//////////////////////////////////////////////////////////////////////////////
class CConnParam
{
public:
    CConnParam(
        const string& driver_name,
        const string& db_type,
        const string& server_name,
        const string& db_name,
        const string& user_name,
        const string& user_pswd
        );
    ~CConnParam(void);

public:
    enum EServerType {
        eSybase,    //< Sybase server
        eMsSql,     //< Microsoft SQL server
        eOracle,    //< ORACLE server
        eSqlite,    //< SQLITE database
        eMySql,     //< MySQL server
        eUnknown    //< Server type is not known
    };
    typedef map<string, string> TDatabaseParameters;

public:
    /// Return current driver name
    const string& GetDriverName(void) const
    {
        return m_driver_name;
    }
    /// Return current server type
    EServerType GetServerType(void) const
    {
        return m_ServerType;
    }
    const string& GetDBType(void) const
    {
        return m_db_type;
    }
    const string& GetServerName(void) const
    {
        return m_server_name;
    }
    const string& GetDBName(void) const
    {
        return m_db_name;
    }
    const string& GetUserName(void) const
    {
        return m_user_name;
    }
    const string& GetUserPswd(void) const
    {
        return m_user_pswd;
    }
    const TDatabaseParameters& GetDatabaseParameters(void) const
    {
        return m_DatabaseParameters;
    }

private:
    const string m_driver_name;
    const string m_db_type;
    const string m_server_name;
    const string m_db_name;
    const string m_user_name;
    const string m_user_pswd;

    EServerType         m_ServerType;
    TDatabaseParameters m_DatabaseParameters;
};

//////////////////////////////////////////////////////////////////////////////
// CConnection does not represent an "physical" connection to a database.
// In current implementation CConnection is a factory of CTransaction.
// CTransaction owns and manages "physical" connections to a database.

class CConnection : public pythonpp::CExtObject<CConnection>
{
public:
    CConnection(
        const CConnParam& conn_param,
        EConnectionMode conn_mode = eSimpleMode
        );
    ~CConnection(void);

public:
    // Python Interface ...
    pythonpp::CObject close(const pythonpp::CTuple& args);
    pythonpp::CObject cursor(const pythonpp::CTuple& args);
    pythonpp::CObject commit(const pythonpp::CTuple& args);
    pythonpp::CObject rollback(const pythonpp::CTuple& args);
    pythonpp::CObject transaction(const pythonpp::CTuple& args);

public:
    // Connection factory interface ...
    IConnection* MakeDBConnection(void) const;

public:
    // CTransaction factory interface ...
    CTransaction* CreateTransaction(void);
    void DestroyTransaction(CTransaction* trans);

protected:
    CTransaction& GetDefaultTransaction(void)
    {
        return *m_DefTransaction;
    }

private:
    typedef set<CTransaction*> TTransList;

    CConnParam              m_ConnParam;
    CDriverManager&         m_DM;
    IDataSource*            m_DS;
    CTransaction*           m_DefTransaction;   //< The lifetime of the default transaction will be managed by Python
    TTransList              m_TransList;        //< List of user-defined transactions
    const EConnectionMode   m_ConnectionMode;
    string                  m_ModuleName;
};

//////////////////////////////////////////////////////////////////////////////
// Python Database API exception classes ... 2/4/2005 8:18PM

//   This is the exception inheritance layout:
//
//    StandardError
//    |__Warning
//    |__Error
//        |__InterfaceError
//        |__DatabaseError
//            |__DataError
//            |__OperationalError
//            |__IntegrityError
//            |__InternalError
//            |__ProgrammingError
//            |__NotSupportedError

class CWarning : public pythonpp::CUserError<CWarning>
{
public:
    CWarning(const string& msg)
    : pythonpp::CUserError<CWarning>( msg )
    {
    }
};

class CError : public pythonpp::CUserError<CError>
{
public:
    CError(const string& msg)
    : pythonpp::CUserError<CError>( msg )
    {
    }

protected:
    CError(const string& msg, PyObject* err_type)
    : pythonpp::CUserError<CError>(msg, err_type)
    {
    }
};

class CInterfaceError : public pythonpp::CUserError<CInterfaceError, CError>
{
public:
    CInterfaceError(const string& msg)
    : pythonpp::CUserError<CInterfaceError, CError>( msg )
    {
    }
};

class CDatabaseError : public pythonpp::CUserError<CDatabaseError, CError>
{
public:
    CDatabaseError(const string& msg)
    : pythonpp::CUserError<CDatabaseError, CError>( msg )
    {
    }

protected:
    CDatabaseError(const string& msg, PyObject* err_type)
    : pythonpp::CUserError<CDatabaseError, CError>(msg, err_type)
    {
    }
};

class CInternalError : public pythonpp::CUserError<CInternalError, CDatabaseError>
{
public:
    CInternalError(const string& msg)
    : pythonpp::CUserError<CInternalError, CDatabaseError>( msg )
    {
    }
};

class COperationalError : public pythonpp::CUserError<COperationalError, CDatabaseError>
{
public:
    COperationalError(const string& msg)
    : pythonpp::CUserError<COperationalError, CDatabaseError>( msg )
    {
    }
};

class CProgrammingError : public pythonpp::CUserError<CProgrammingError, CDatabaseError>
{
public:
    CProgrammingError(const string& msg)
    : pythonpp::CUserError<CProgrammingError, CDatabaseError>( msg )
    {
    }
};

class CIntegrityError : public pythonpp::CUserError<CIntegrityError, CDatabaseError>
{
public:
    CIntegrityError(const string& msg)
    : pythonpp::CUserError<CIntegrityError, CDatabaseError>( msg )
    {
    }
};

class CDataError : public pythonpp::CUserError<CDataError, CDatabaseError>
{
public:
    CDataError(const string& msg)
    : pythonpp::CUserError<CDataError, CDatabaseError>( msg )
    {
    }
};

class CNotSupportedError : public pythonpp::CUserError<CNotSupportedError, CDatabaseError>
{
public:
    CNotSupportedError(const string& msg)
    : pythonpp::CUserError<CNotSupportedError, CDatabaseError>( msg )
    {
    }
};

//////////////////////////////////////////////////////////////////////////////
inline
const CConnection&
CSelectConnPool::GetConnection(void) const
{
    return m_Transaction->GetParentConnection();
}

inline
CConnection&
CSelectConnPool::GetConnection(void)
{
    return m_Transaction->GetParentConnection();
}

//////////////////////////////////////////////////////////////////////////////
inline
const CConnection&
CDMLConnPool::GetConnection(void) const
{
    return m_Transaction->GetParentConnection();
}

inline
CConnection&
CDMLConnPool::GetConnection(void)
{
    return m_Transaction->GetParentConnection();
}

}

END_NCBI_SCOPE


