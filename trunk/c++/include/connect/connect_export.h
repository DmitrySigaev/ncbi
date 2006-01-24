#ifndef CONNECT___CONNECT_EXPORT__H
#define CONNECT___CONNECT_EXPORT__H

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
 * Author:  Mike DiCuccio
 *
 * File Description:
 *    Defines to provide correct exporting from CONNECT DLL in Windows.
 *    These are necessary to compile DLLs with Visual C++ - exports must be
 *    explicitly labeled as such.
 */


/** @addtogroup WinDLL
 *
 * @{
 */


#if defined(WIN32)  &&  defined(NCBI_DLL_BUILD)

#ifndef _MSC_VER
#  error "This toolkit is not buildable with a compiler other than MSVC."
#endif


/*
 * Dumping ground for Windows-specific stuff
 */
#pragma warning (disable : 4786 4251 4275)


#ifdef NCBI_CORE_EXPORTS
#  define NCBI_XCONNECT_EXPORTS
#endif


#ifdef NCBI_XCONNECT_EXPORTS
#  define NCBI_XCONNECT_EXPORT      __declspec(dllexport)
#else
#  define NCBI_XCONNECT_EXPORT      __declspec(dllimport)
#endif


#else  /*  !defined(NCBI_OS_MSWIN)  ||  !defined(NCBI_DLL_BUILD)  */

/*
 * NULL operations for other cases
 */

#  define NCBI_XCONNECT_EXPORT


#endif


/* @} */


/*
 * ==========================================================================
 * $Log$
 * Revision 6.7  2006/01/24 21:56:50  lavr
 * Fix comment
 *
 * Revision 6.6  2004/03/15 17:16:10  gorelenk
 * Removed #undef NCBI_DLL_BUILD when defined _LIB .
 *
 * Revision 6.5  2003/08/28 19:36:58  lavr
 * Rollback to R6.3
 *
 * Revision 6.4  2003/08/28 18:46:15  ucko
 * #include <ncbiconf.h> so that other connect headers (on the C++ side,
 * anyway) can pick up its settings.
 *
 * Revision 6.3  2003/07/18 21:07:56  ivanov
 * Use WIN32 definition instead of NCBI_MS_WIN
 *
 * Revision 6.2  2003/04/09 17:58:37  siyan
 * Added doxygen support
 *
 * Revision 6.1  2003/01/07 21:58:23  lavr
 * Initial revision
 *
 * ==========================================================================
 */

#endif  /*  CONNECT___CONNECT_EXPORT__H  */
