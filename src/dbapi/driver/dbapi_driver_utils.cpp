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
 * Author:  Sergey Sikorskiy
 *
 * File Description:  Small utility classes common to all drivers.
 *
 */

#include <ncbi_pch.hpp>

#include <dbapi/driver/impl/dbapi_driver_utils.hpp>

BEGIN_NCBI_SCOPE

/////////////////////////////////////////////////////////////////////////////
struct SNoLock
{
    void operator() (CDB_UserHandler::TExceptions& resource) const {}
};

struct SUnLock
{
    void operator() (CDB_UserHandler::TExceptions& resource) const 
    {
        NON_CONST_ITERATE(CDB_UserHandler::TExceptions, it, resource) {
            delete *it;
        }

        resource.clear();
    }
};
    

/////////////////////////////////////////////////////////////////////////////
CDBExceptionStorage::CDBExceptionStorage(void)
{
}

CDBExceptionStorage::~CDBExceptionStorage(void) throw()
{
    try {
        NON_CONST_ITERATE(CDB_UserHandler::TExceptions, it, m_Exceptions) {
            delete *it;
        }    
    }
    NCBI_CATCH_ALL( kEmptyStr )
}
    
void CDBExceptionStorage::Accept(CDB_Exception const& e)
{
    CFastMutexGuard mg(m_Mutex);
    
    m_Exceptions.push_back(e.Clone());
}

void CDBExceptionStorage::Handle(CDBHandlerStack& handler)
{
    typedef CGuard<CDB_UserHandler::TExceptions, SNoLock, SUnLock> TGuard;
    
    if (!m_Exceptions.empty()) {
        CFastMutexGuard mg(m_Mutex);
        TGuard guard(m_Exceptions);
        
        if (!handler.HandleExceptions(m_Exceptions)) {
            NON_CONST_ITERATE(CDB_UserHandler::TExceptions, it, m_Exceptions) {
                handler.PostMsg(*it);
            }
        }
    }
}



END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.1  2006/05/11 17:57:20  ssikorsk
 * Implemented CDBExceptionStorage class
 *
 * ===========================================================================
 */
