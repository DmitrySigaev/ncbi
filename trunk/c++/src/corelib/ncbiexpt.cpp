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
*   CErrnoException
*   CParseException
*   + initialization for the "unexpected"
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.7  1999/01/04 22:41:43  vakatov
* Do not use so-called "hardware-exceptions" as these are not supported
* (on the signal level) by UNIX
* Do not "set_unexpected()" as it works differently on UNIX and MSVC++
*
* Revision 1.6  1998/12/28 17:56:37  vakatov
* New CVS and development tree structure for the NCBI C++ projects
*
* Revision 1.3  1998/11/13 00:17:13  vakatov
* [UNIX] Added handler for the unexpected exceptions
*
* Revision 1.2  1998/11/10 17:58:42  vakatov
* [UNIX] Removed extra #define's (POSIX... and EXTENTIONS...)
* Allow adding strings in CNcbiErrnoException(must have used "CC -xar"
* instead of just "ar" when building a library;  otherwise -- link error)
*
* Revision 1.1  1998/11/10 01:20:01  vakatov
* Initial revision(derived from former "ncbiexcp.cpp")
*
* ===========================================================================
*/

#include <corelib/ncbiexpt.hpp>
#include <errno.h>
#include <string.h>

// (BEGIN_NCBI_SCOPE must be followed by END_NCBI_SCOPE later in this file)
BEGIN_NCBI_SCOPE


/////////////////////////////////
//  CErrnoException

CErrnoException::CErrnoException(const string& what)
    throw() : runtime_error(what + ": " + ::strerror(errno)) {
        m_Errno = errno;
}


/////////////////////////////////
//  CParseException

static string s_ComposeParse(const string& what, SIZE_TYPE pos)
{
    char s[32];
    ::sprintf(s, "%ld", (long)pos);
    string str;
    str.reserve(256);
    return str.append("{").append(s).append("} ").append(what);
}

CParseException::CParseException(const string& what, SIZE_TYPE pos)
    throw() : runtime_error(s_ComposeParse(what,pos)) {
        m_Pos = pos;
}

// (END_NCBI_SCOPE must be preceeded by BEGIN_NCBI_SCOPE)
END_NCBI_SCOPE
