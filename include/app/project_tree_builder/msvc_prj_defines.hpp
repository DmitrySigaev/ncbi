#ifndef MSVC_PRJ_DEFINES_HEADER
#define MSVC_PRJ_DEFINES_HEADER

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
 * Author:  Viatcheslav Gorelenkov
 *
 */



/// MSVC 7.10 project defines
#define MSVC_PROJECT_FILE_EXT   ".vcproj"

#define MSVC_PROJECT_PLATFORM       "Win32"
#define MSVC_PROJECT_PROJECT_TYPE   "Visual C++"
#define MSVC_PROJECT_VERSION        "7.10"
#define MSVC_PROJECT_KEYWORD_WIN32  "Win32Proj"

/// _MasterProject defines
#define MSVC_MASTERPROJECT_ROOT_NAMESPACE "MasterProject"
#define MSVC_MASTERPROJECT_KEYWORD        "ManagedCProj"


/// MSVC 7.10 solution defines
#define MSVC_SOLUTION_HEADER_LINE   "Microsoft Visual Studio Solution File, Format Version 8.00"
#define MSVC_SOLUTION_ROOT_GUID     "{8BC9CEB8-8B4A-11D0-8D11-00A0C91BC942}"


/*
 * ===========================================================================
 * $Log$
 * Revision 1.2  2004/01/22 17:57:08  gorelenk
 * first version
 *
 * ===========================================================================
 */

#endif // MSVC_PRJ_DEFINES_HEADER