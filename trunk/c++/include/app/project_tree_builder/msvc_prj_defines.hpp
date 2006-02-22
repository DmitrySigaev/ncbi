#ifndef PROJECT_TREE_BUILDER__MSVC_PRJ_DEFINES__HPP
#define PROJECT_TREE_BUILDER__MSVC_PRJ_DEFINES__HPP

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

#define MSVC_PROJECT_PROJECT_TYPE   "Visual C++"
#define MSVC_PROJECT_KEYWORD_WIN32  "Win32Proj"

/// _MasterProject defines
#define MSVC_MASTERPROJECT_ROOT_NAMESPACE "MasterProject"
#define MSVC_MASTERPROJECT_KEYWORD        "ManagedCProj"


/// MSVC solution defines
#define MSVC_SOLUTION_ROOT_GUID     "{8BC9CEB8-8B4A-11D0-8D11-00A0C91BC942}"

#define MSVC_DEFAULT_LIBS_TAG       "DefaultLibs"

/// Separator for list values in registry
#define LIST_SEPARATOR      " \t\n\r,;"


#define MSVC_REG_SECTION            "msvc"
#define MSVC_SOLUTION_HEADER_LINE   \
                "Microsoft Visual Studio Solution File, Format Version "

/*
 * ===========================================================================
 * $Log$
 * Revision 1.10  2006/02/22 17:32:44  gouriano
 * Get ready for 64 bits platform
 *
 * Revision 1.9  2006/01/10 17:39:42  gouriano
 * Corrected solution generation for MSVC 2005 Express
 *
 * Revision 1.8  2005/12/27 14:58:14  gouriano
 * Adjustments for MSVC 2005 Express
 *
 * Revision 1.7  2005/12/20 19:34:44  gouriano
 * Added MSVC 800 defines
 *
 * Revision 1.6  2004/06/10 15:12:55  gorelenk
 * Added newline at the file end to avoid GCC warning.
 *
 * Revision 1.5  2004/02/13 20:42:14  gorelenk
 * Minor cosmetic changes.
 *
 * Revision 1.4  2004/02/06 23:15:40  gorelenk
 * Implemented support of ASN projects, semi-auto configure,
 * CPPFLAGS support. Second working version.
 *
 * Revision 1.3  2004/01/29 15:41:23  gorelenk
 * Added "DefaultLibs" define
 *
 * Revision 1.2  2004/01/22 17:57:08  gorelenk
 * first version
 *
 * ===========================================================================
 */

#endif //PROJECT_TREE_BUILDER__MSVC_PRJ_DEFINES__HPP
