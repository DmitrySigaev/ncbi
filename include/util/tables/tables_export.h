#ifndef UTIL_TABLES___TABLES_EXPORT__H
#define UTIL_TABLES___TABLES_EXPORT__H

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
 * Authors:  Anatoliy Kuznetsov, Mike DiCuccio, Aaron Ucko
 *
 * File Description:
 *    Defines to provide correct exporting from TABLES DLL in Windows.
 *    These are necessary to compile DLLs with Visual C++ - exports must be
 *    explicitly labeled as such.
 */


/** @addtogroup WinDLL
 *
 * @{
 */


#ifdef _LIB
#  undef NCBI_DLL_BUILD
#endif


#if defined(WIN32)  &&  defined(NCBI_DLL_BUILD)

#ifndef _MSC_VER
#  error "This toolkit is not buildable with a compiler other than MSVC."
#endif


/*
 * Dumping ground for Windows-specific stuff
 */
#pragma warning (disable : 4786 4251 4275)


#ifdef NCBI_CORE_EXPORTS
#  define NCBI_TABLES_EXPORTS
#endif


/* This is a static lib on MSVC 7.10 */

#if (_MSC_VER >= 1310)
#  define NCBI_TABLES_EXPORT
#else
# ifdef NCBI_TABLES_EXPORTS
#   define NCBI_TABLES_EXPORT      __declspec(dllexport)
# else
#   define NCBI_TABLES_EXPORT      __declspec(dllimport)
# endif /* NCBI_TABLES_EXPORTS */
#endif  /* (_MSC_VER >= 1310) */



#else  /*  !defined(NCBI_OS_MSWIN)  ||  !defined(NCBI_DLL_BUILD)  */

/*
 * NULL operations for other cases
 */

#  define NCBI_TABLES_EXPORT


#endif


/* @} */


/*
 * ==========================================================================
 *
 * $Log$
 * Revision 1.2  2004/03/11 20:32:20  gorelenk
 * Conditionaly changed definition of NCBI_TABLES_EXPORT export prefix.
 *
 * Revision 1.1  2003/08/21 19:48:19  ucko
 * Add tables library (shared with C) for raw score matrices, etc.
 *
 *
 * ==========================================================================
 */

#endif  /*  UTIL_TABLES___TABLES_EXPORT__H  */
