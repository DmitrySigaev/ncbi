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
* Author:  Aaron Ucko
*
* File Description:
*   Meta-source file including replacements for system functions
*   lacking on some platforms.
*
* ===========================================================================
*/

#include <ncbiconf.h>

#ifdef software_version
#undef software_version
#endif

#ifdef no_unused_var_warn
#undef no_unused_var_warn
#endif

#ifndef HAVE_ASPRINTF
#define software_version asprintf_software_version
#define no_unused_var_warn asprintf_no_unused_var_warn
#include "../replacements/asprintf.c"
#undef software_version
#undef no_unused_var_warn
#endif

#ifndef HAVE_ATOLL
#define software_version atoll_software_version
#define no_unused_var_warn atoll_no_unused_var_warn
#include "../replacements/atoll.c"
#undef software_version
#undef no_unused_var_warn
#endif

#ifndef HAVE_ICONV
#define software_version iconv_software_version
#define no_unused_var_warn iconv_no_unused_var_warn
#include "../replacements/iconv_win.c"
#undef software_version
#undef no_unused_var_warn
#endif

#ifndef HAVE_STRTOK_R
#define software_version strtok_r_software_version
#define no_unused_var_warn strtok_r_no_unused_var_warn
#include "../replacements/strtok_r.c"
#undef software_version
#undef no_unused_var_warn
#endif

#ifndef HAVE_VASPRINTF
#define software_version vasprintf_software_version
#define no_unused_var_warn vasprintf_no_unused_var_warn
#include "../replacements/vasprintf.c"
#undef software_version
#undef no_unused_var_warn
#endif

/*
* ===========================================================================
*
* $Log$
* Revision 1.2  2005/07/21 00:27:04  ucko
* Avoid multiple definitions of software_version and no_unused_var_warn
* via preprocessor tricks.
*
* Revision 1.1  2005/07/20 22:42:57  ucko
* Add a meta-source file including replacements for system functions
* lacking on some platforms.
*
*
* ===========================================================================
*/
