#ifndef _STMT_IMPL_HPP_
#define _STMT_IMPL_HPP_

/* $Id$
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
* File Name:  $Id$
*
* Author:  Michael Kholodov
*   
* File Description:  Statement implementation
*
*
* $Log$
* Revision 1.10  2004/04/08 15:56:58  kholodov
* Multiple bug fixes and optimizations
*
* Revision 1.9  2004/02/10 18:50:44  kholodov
* Modified: made Move() method const
*
* Revision 1.8  2002/12/16 18:56:50  kholodov
* Fixed: memory leak in CStatement object
*
* Revision 1.7  2002/12/05 17:37:23  kholodov
* Fixed: potential memory leak in CStatement::HasMoreResults() method
* Modified: getter and setter name for the internal CDB_Result pointer.
*
* Revision 1.6  2002/10/21 20:38:08  kholodov
* Added: GetParentConn() method to get the parent connection from IStatement,
* ICallableStatement and ICursor objects.
* Fixed: Minor fixes
*
* Revision 1.5  2002/10/03 18:50:00  kholodov
* Added: additional TRACE diagnostics about object deletion
* Fixed: setting parameters in IStatement object is fully supported
* Added: IStatement::ExecuteLast() to execute the last statement with
* different parameters if any
*
* Revision 1.4  2002/09/18 18:49:27  kholodov
* Modified: class declaration and Action method to reflect
* direct inheritance of CActiveObject from IEventListener
*
* Revision 1.3  2002/09/09 20:48:57  kholodov
* Added: Additional trace output about object life cycle
* Added: CStatement::Failed() method to check command status
*
* Revision 1.2  2002/05/16 22:11:12  kholodov
* Improved: using minimum connections possible
*
* Revision 1.1  2002/01/30 14:51:23  kholodov
* User DBAPI implementation, first commit
*
* Revision 1.1  2001/11/30 16:30:03  kholodov
* Snapshot of the first draft of dbapi lib
*
*
*/

#include <dbapi/dbapi.hpp>
#include "active_obj.hpp"
#include <corelib/ncbistre.hpp>
#include "dbexception.hpp"


BEGIN_NCBI_SCOPE

class CStatement : public CActiveObject, 
                   public virtual IStatement
{
public:
    CStatement(class CConnection* conn);

    virtual ~CStatement();
  
    virtual IResultSet* GetResultSet();

    virtual bool HasMoreResults();

    virtual bool HasRows();
    virtual bool Failed();
    virtual int GetRowCount();
  
    
    virtual void Execute(const string& sql);
    virtual void ExecuteUpdate(const string& sql);

    virtual void ExecuteLast();

    virtual void Cancel();
    virtual void Close();

    virtual void SetParam(const CVariant& v, 
                          const string& name);

    virtual void ClearParamList();

    virtual IConnection* GetParentConn();

    CConnection* GetConnection() {
        return m_conn;
    }

    CDB_Result* GetCDB_Result() {
        return m_rs;
    }

    CDB_LangCmd* GetLangCmd();
    

    // Interface IEventListener implementation
    virtual void Action(const CDbapiEvent& e);

protected:    
    void SetBaseCmd(I_BaseCmd *cmd) { m_cmd = cmd; }
    I_BaseCmd* GetBaseCmd() { return m_cmd; }

    void SetCDB_Result(CDB_Result *rs);

    void SetFailed(bool f) {
        m_failed = f;
    }

    void FreeResources();

private:
    class CConnection* m_conn;
    I_BaseCmd *m_cmd;
    CDB_Result *m_rs;
    int m_rowCount;
    bool m_failed;
    typedef map<string, CVariant*> ParamList;
    ParamList m_params;
    //typedef set<CDB_Result*> RequestedRsList;
    //RequestedRsList m_requestedRsList;
    class CResultSet *m_irs;

};

END_NCBI_SCOPE
//====================================================================

#endif // _STMT_IMPL_HPP_
