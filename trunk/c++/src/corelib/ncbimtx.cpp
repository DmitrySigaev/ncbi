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
 * Author:  Denis Vakatov, Aleksey Grichenko
 *
 * File Description:
 *   Multi-threading -- fast mutexes
 *
 *   MUTEX:
 *      CInternalMutex   -- platform-dependent mutex functionality
 *
 * ---------------------------------------------------------------------------
 * $Log$
 * Revision 1.1  2001/03/26 20:31:13  vakatov
 * Initial revision (moved code from "ncbithr.cpp")
 *
 * ===========================================================================
 */

#include <corelib/ncbimtx.hpp>
#include <assert.h>


BEGIN_NCBI_SCOPE


/////////////////////////////////////////////////////////////////////////////
//  Auxiliary
//


#if defined(_DEBUG)
#  define verify(expr) assert(expr)
#  define s_Verify(state, message)  assert(((void) message, (state)))
#else  /* _DEBUG */
#  define verify(expr) ((void)(expr))
inline
void s_Verify(bool state, const char* message)
{
    if ( !state ) {
        throw runtime_error(message);
    }
}
#endif  /* _DEBUG */



/////////////////////////////////////////////////////////////////////////////
//  CInternalMutex::
//

CInternalMutex::CInternalMutex(void)
{
    // Create platform-dependent mutex handle
#if defined(NCBI_WIN32_THREADS)
    s_Verify((m_Handle = CreateMutex(NULL, FALSE, NULL)) != NULL,
             "CInternalMutex::CInternalMutex() -- error creating mutex");
#elif defined(NCBI_POSIX_THREADS)
    s_Verify(pthread_mutex_init(&m_Handle, 0) == 0,
             "CInternalMutex::CInternalMutex() -- error creating mutex");
#endif
    m_Initialized = true;
    return;
}


CInternalMutex::~CInternalMutex(void)
{
    // Destroy system mutex handle
#if defined(NCBI_WIN32_THREADS)
    verify(CloseHandle(m_Handle) != 0);
#elif defined(NCBI_POSIX_THREADS)
    verify(pthread_mutex_destroy(&m_Handle) == 0);
#endif
    m_Initialized = false;
    return;
}


END_NCBI_SCOPE
