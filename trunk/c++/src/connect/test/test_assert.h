#ifndef TEST_ASSERT__H
#define TEST_ASSERT__H

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
 * Revision 6.18  2002/06/21 13:52:15  lebedev
 * NCBI_OS_DARWIN: assert fix
 *
 * Revision 6.17  2002/05/01 19:14:08  lavr
 * Changed: NCBI_COMPILER_MIPSPRO -> NCBI_OS_IRIX
 *
 * Revision 6.16  2002/04/23 16:23:46  lavr
 * Yet another tweak for Windows :-(
 *
 * Revision 6.15  2002/04/23 14:35:16  lavr
 * Another round of moving things to get tests to link on Windows (Release)
 *
 * Revision 6.14  2002/04/22 20:41:00  ivanov
 * Added #define _ASSERT assert
 *
 * Revision 6.13  2002/04/22 20:36:31  ivanov
 * #undef _ASSERT added for Windows to disable use this macro in tests
 * -- application don't terminate in Windows _ASSERT (CRT library bug).
 *
 * Revision 6.12  2002/04/22 19:28:14  lavr
 * Shuffle things around again to get asserts defined in both Debug and Release
 *
 * Revision 6.11  2002/04/22 14:16:30  lavr
 * Add #undef assert, seems not to work otherwise on SGIs
 *
 * Revision 6.10  2002/04/18 15:50:19  lavr
 * Work around MSVC compiler bug treating formal parameter as a class name
 *
 * Revision 6.9  2002/04/16 22:04:20  lavr
 * #undef _ASSERT added for Windows to resolve macro redefinition
 *
 * Revision 6.8  2002/04/15 23:51:10  lavr
 * +#include <stdio.h> to define stderr
 *
 * Revision 6.7  2002/04/15 20:06:09  lavr
 * Fixed function pointer type: now explicit instead of Microsoft's PFV
 *
 * Revision 6.6  2002/04/15 19:22:24  lavr
 * Register MSVC-specific handler which suppresses popup messages at run-time
 *
 * Revision 6.5  2002/03/22 19:49:55  lavr
 * Undef NDEBUG; include <assert.h>; must go last in the list of include files
 *
 * Revision 6.4  2002/03/21 22:02:35  lavr
 * Provide assert() for MSVC
 *
 * Revision 6.3  2002/01/20 04:56:27  vakatov
 * Use #_MSC_VER rather than #include <ncbiconf.h> as the latter does not
 * exist when the code is compiled for the C Toolkit
 *
 * Revision 6.2  2002/01/19 00:04:00  vakatov
 * Do not force #_DEBUG on MSVC -- or it fails to link some functions which
 * defined in the debug C run-time lib only (such as _CrtDbgReport)
 *
 * Revision 6.1  2002/01/16 21:19:26  vakatov
 * Initial revision
 *
 * ===========================================================================
 */

#include "../ncbi_config.h"


#ifdef NCBI_OS_MSWIN
#  ifdef   _ASSERT
#    undef _ASSERT
#  endif
#  define  Type aType
#  include <crtdbg.h>
#  include <stdio.h>
#  include <windows.h>
#  undef   Type

/* Suppress popup messages on execution errors.
 * NOTE: Windows-specific, suppresses all error message boxes in both runtime
 * and in debug libraries, as well as all General Protection Fault messages.
 * Environment variable DIAG_SILENT_ABORT must be set to "Y" or "y".
 */
static void _SuppressDiagPopupMessages(void)
{
    /* Check environment variable for silent abort app at error */
    const char* value = getenv("DIAG_SILENT_ABORT");
    if (value  &&  (*value == 'Y'  ||  *value == 'y')) {
        /* Windows GPF errors */
        SetErrorMode(SEM_NOGPFAULTERRORBOX);

        /* Runtime library */
        _set_error_mode(_OUT_TO_STDERR);

        /* Debug library */
        _CrtSetReportFile(_CRT_WARN,   stderr);
        _CrtSetReportMode(_CRT_WARN,   _CRTDBG_MODE_FILE);
        _CrtSetReportFile(_CRT_ERROR,  stderr);
        _CrtSetReportMode(_CRT_ERROR,  _CRTDBG_MODE_FILE);
        _CrtSetReportFile(_CRT_ASSERT, stderr);
        _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    }
}

/* Put this function at startup init level 'V', far enough not to mess up with
 * base RTL init, which happens at preceding levels in alphabetical order.
 */
#  pragma data_seg(".CRT$XIV")

static void (*_SDPM)(void) = _SuppressDiagPopupMessages;

#  pragma data_seg()
#endif


#ifdef   NDEBUG
#  undef NDEBUG
#endif
#ifdef   assert
#  undef assert
#endif

/* IRIX stdlib fix (MIPSpro compiler tested): assert.h already included above*/
#ifdef NCBI_OS_IRIX
#  ifdef   __ASSERT_H__
#    undef __ASSERT_H__
#  endif
#endif

/* Likewise on OSF/1 (at least with GCC 3, but this never hurts) */
#ifdef NCBI_OS_OSF1
#  ifdef   _ASSERT_H_
#    undef _ASSERT_H_
#  endif
#endif

/* ...and on Darwin (at least with GCC 3, but this never hurts) */
#ifdef NCBI_OS_DARWIN
#  ifdef   FIXINC_BROKEN_ASSERT_STDLIB_CHECK
#    undef FIXINC_BROKEN_ASSERT_STDLIB_CHECK
#  endif
#endif

#include <assert.h>

#ifdef   _ASSERT
#  undef _ASSERT
#endif
#define  _ASSERT assert


#endif  /* TEST_ASSERT__H */
