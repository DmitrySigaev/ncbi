#ifndef CORELIB_TEST___TEST_ASSERT__H
#define CORELIB_TEST___TEST_ASSERT__H

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
 * Author:  Denis Vakatov
 *
 * File Description:
 *   Setup #NDEBUG and #_DEBUG preprocessor macro in a way that ASSERTs
 *   will be active even in the "Release" mode (it's useful for test apps).
 *
 * --------------------------------------------------------------------------
 * $Log$
 * Revision 6.2  2002/01/19 00:04:00  vakatov
 * Do not force #_DEBUG on MSVC -- or it fails to link some functions which
 * defined in the debug C run-time lib only (such as _CrtDbgReport)
 *
 * Revision 6.1  2002/01/16 21:19:26  vakatov
 * Initial revision
 *
 * ===========================================================================
 */

#include <ncbiconf.h>

#if defined(NDEBUG)
#  undef  NDEBUG
#endif 

#if !defined(_DEBUG)  &&  !defined(NCBI_OS_MSWIN)
#  define _DEBUG
#endif 


#endif  /* CORELIB_TEST___TEST_ASSERT__H */
