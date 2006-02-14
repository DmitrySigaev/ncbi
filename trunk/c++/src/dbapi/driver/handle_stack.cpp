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
 * File Description:  Handlers Stack
 *
 */

#include <ncbi_pch.hpp>
#include <dbapi/driver/util/handle_stack.hpp>
#include <string.h>
#include <algorithm>

BEGIN_NCBI_SCOPE


CDBHandlerStack::CDBHandlerStack(size_t n)
{
    n = n;
}

CDBHandlerStack::~CDBHandlerStack() 
{
}

void CDBHandlerStack::Push(CDB_UserHandler* h)
{
    m_Stack.push_back( h );
}


void CDBHandlerStack::Pop(CDB_UserHandler* h, bool last)
{
    if ( last ) {
        while ( !m_Stack.empty() ) {
            if (m_Stack.back() == h) {
                m_Stack.pop_back();
                break;
            } else {
                m_Stack.pop_back();
            }
        }
    } else {
        deque<CDB_UserHandler*>::iterator cit;
        
        cit = find(m_Stack.begin(), m_Stack.end(), h);
        if ( cit != m_Stack.end() ) {
            m_Stack.erase( cit, m_Stack.end() );
        }
    }
}


CDBHandlerStack::CDBHandlerStack(const CDBHandlerStack& s)
: m_Stack( s.m_Stack )
{
    return;
}


CDBHandlerStack& CDBHandlerStack::operator= (const CDBHandlerStack& s)
{
    if ( this != &s ) {
        m_Stack = s.m_Stack;
    }
    return *this;
}


void CDBHandlerStack::PostMsg(CDB_Exception* ex)
{
    // Attempting to use m_Stack directly on WorkShop fails because it
    // tries to call the non-const version of rbegin(), and the
    // resulting reverse_iterator can't automatically be converted to
    // a const_reverse_iterator.  (Sigh.)
    const deque<CDB_UserHandler*>& s = m_Stack;
    REVERSE_ITERATE(deque<CDB_UserHandler*>, cit, s) {
        if ( *cit && (*cit)->HandleIt(ex) ) {
            break;
        }
    }
}


END_NCBI_SCOPE



/*
 * ===========================================================================
 * $Log$
 * Revision 1.9  2006/02/14 17:48:23  ssikorsk
 * Fixed algorithm of deleting a handler from a stack in CDBHandlerStack::Pop.
 *
 * Revision 1.8  2005/10/31 21:28:04  ucko
 * Tweak PostMsg to compile under WorkShop, and to take advantage of the
 * REVERSE_ITERATE macro.
 *
 * Revision 1.7  2005/10/31 19:01:40  ssikorsk
 * + #include <algorithm>
 *
 * Revision 1.6  2005/10/31 17:18:39  ssikorsk
 * Revamp CDBHandlerStack to use std::deque
 *
 * Revision 1.5  2004/05/17 21:11:38  gorelenk
 * Added include of PCH ncbi_pch.hpp
 *
 * Revision 1.4  2001/11/06 17:59:53  lavr
 * Formatted uniformly as the rest of the library
 *
 * Revision 1.3  2001/09/27 20:08:32  vakatov
 * Added "DB_" (or "I_") prefix where it was missing
 *
 * Revision 1.2  2001/09/27 16:46:32  vakatov
 * Non-const (was const) exception object to pass to the user handler
 *
 * Revision 1.1  2001/09/21 23:39:59  vakatov
 * -----  Initial (draft) revision.  -----
 * This is a major revamp (by Denis Vakatov, with help from Vladimir Soussov)
 * of the DBAPI "driver" libs originally written by Vladimir Soussov.
 * The revamp involved massive code shuffling and grooming, numerous local
 * API redesigns, adding comments and incorporating DBAPI to the C++ Toolkit.
 *
 * ===========================================================================
 */
