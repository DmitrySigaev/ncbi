#ifndef DBAPI_DRIVER___EXCEPTION__HPP
#define DBAPI_DRIVER___EXCEPTION__HPP

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
* Author:  Vladimir Soussov
* 
* File Description:  Exceptions
*
*
*/

#include <stdexcept>
#include <corelib/ncbistl.hpp>
#include <corelib/ncbistr.hpp>


BEGIN_NCBI_SCOPE


enum EDBSeverity {
    eDB_Info,
    eDB_Warning,
    eDB_Error,
    eDB_Fatal,
    eDB_Unknown
};



class CDB_Exception : public exception
{
public:
    // exception type
    enum EType {
        eDS,
        eRPC,
        eSQL,
        eDeadlock,
        eTimeout,
        eClient,
        eMulti
    };

    // exception::what()
    virtual const char* what() const;

    // access
    EType          Type()            const  { return m_Type;           }
    EDBSeverity    Severity()        const  { return m_Severity;       }
    int            ErrCode()         const  { return m_ErrCode;        }
    const string&  OriginatedFrom()  const  { return m_OriginatedFrom; }
    const string&  Message()         const  { return m_Message;        }

    // clone
    virtual CDB_Exception* Clone() const;

    // destructor
    virtual ~CDB_Exception();

protected:
    // constructor
    CDB_Exception(EType type, EDBSeverity severity, int err_code,
                  const string& originated_from, const string& msg);

    // data
    EType       m_Type;
    EDBSeverity m_Severity;
    int         m_ErrCode;
    string      m_OriginatedFrom;
    string      m_Message;
};



class CDB_DSEx : public CDB_Exception
{
public:
    CDB_DSEx(EDBSeverity severity, int err_code,
             const string& originated_from, const string& msg)
        : CDB_Exception(eDS, severity, err_code, originated_from, msg) {
        return;
    }

    virtual CDB_Exception* Clone() const;
};



class CDB_RPCEx : public CDB_Exception
{
public:
    CDB_RPCEx(EDBSeverity severity, int err_code,
              const string& originated_from, const string& msg,
              const string& proc_name, int proc_line);

    const string& ProcName()  const { return m_ProcName; }
    int           ProcLine()  const { return m_ProcLine;     }

    virtual CDB_Exception* Clone() const;

private:
    string m_ProcName;
    int    m_ProcLine;
};



class CDB_SQLEx : public CDB_Exception
{
public:
    CDB_SQLEx(EDBSeverity severity, int err_code,
              const string& originated_from, const string& msg,
              const string& sql_state, int batch_line);

    const string& SqlState()   const { return m_SqlState;  }
    int           BatchLine()  const { return m_BatchLine; }

    virtual CDB_Exception* Clone() const;

private:
    string m_SqlState;
    int    m_BatchLine;
};



class CDB_DeadlockEx : public CDB_Exception
{
public:
    CDB_DeadlockEx(const string& originated_from, const string& msg)
        : CDB_Exception(eDeadlock, eDB_Warning, 123456, originated_from, msg) {
        return;
    }

    virtual CDB_Exception* Clone() const;
};



class CDB_TimeoutEx : public CDB_Exception
{
public:
    CDB_TimeoutEx(int err_code,
                  const string& originated_from, const string& msg)
        : CDB_Exception(eTimeout, eDB_Warning, err_code, originated_from, msg) {
        return;
    }

    virtual CDB_Exception* Clone() const;
};



class CDB_ClientEx : public CDB_Exception
{
public:
    CDB_ClientEx(EDBSeverity severity, int err_code,
                 const string& originated_from, const string& msg)
        : CDB_Exception(eClient, severity, err_code, originated_from, msg) {
        return;
    }

    virtual CDB_Exception* Clone() const;

};

class CDB_MultiEx : public CDB_Exception
{
public:
    CDB_MultiEx(const string& originated_from = kEmptyStr,
                unsigned int  capacity = 64);
    CDB_MultiEx(const CDB_MultiEx& mex);

    bool           Push(const CDB_Exception& e)  { return m_Bag->Push(&e); }
    CDB_Exception* Pop(void)                     { return m_Bag->Pop();    }

    unsigned int NofExceptions() const { return m_Bag->NofExceptions(); }
    unsigned int Capacity()      const { return m_Bag->Capacity();      }

    virtual ~CDB_MultiEx();

private:
    class CDB_MultiExStorage
    {
    public:
        void AddRef() {
            ++m_RefCnt;
        }
        void DelRef() {
            if (--m_RefCnt <= 0)
                delete this;
        }
        bool Push(const CDB_Exception* ex);
	
        CDB_Exception* Pop(void);

        unsigned int NofExceptions() const { return m_NofExs;   }
        unsigned int Capacity()      const { return m_NofRooms; }
	
        CDB_MultiExStorage(unsigned int capacity);
	
        ~CDB_MultiExStorage();

    private:
        unsigned int    m_NofRooms;
        unsigned int    m_NofExs;
        int             m_RefCnt;
        CDB_Exception** m_Ex;
    };

    CDB_MultiExStorage* m_Bag;
};



/////////////////////////////////////////////////////////////////////////////
// CDBUserHandler::  base class for user-defined handlers
//

class CDBUserHandler
{
public:
    // Return TRUE if "ex" is processed, FALSE if not (or if "ex" is NULL)
    virtual bool HandleIt(const CDB_Exception* ex) = 0;
    virtual ~CDBUserHandler();
};



END_NCBI_SCOPE



/*
 * ===========================================================================
 * $Log$
 * Revision 1.2  2001/09/24 20:52:18  vakatov
 * Fixed args like "string& s = 0" to "string& s = kEmptyStr"
 *
 * Revision 1.1  2001/09/21 23:39:52  vakatov
 * -----  Initial (draft) revision.  -----
 * This is a major revamp (by Denis Vakatov, with help from Vladimir Soussov)
 * of the DBAPI "driver" libs originally written by Vladimir Soussov.
 * The revamp involved massive code shuffling and grooming, numerous local
 * API redesigns, adding comments and incorporating DBAPI to the C++ Toolkit.
 *
 * ===========================================================================
 */

#endif  /* DBAPI_DRIVER___EXCEPTION__HPP */
