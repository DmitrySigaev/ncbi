#ifndef NCBIEXPT__HPP
#define NCBIEXPT__HPP

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
*   NCBI C++ exception handling
*   Includes auxiliary ad hoc macros to "catch" and macros
*   for the C++ exception specification
*
* --------------------------------------------------------------------------
* $Log$
* Revision 1.8  1999/01/04 22:41:41  vakatov
* Do not use so-called "hardware-exceptions" as these are not supported
* (on the signal level) by UNIX
* Do not "set_unexpected()" as it works differently on UNIX and MSVC++
*
* Revision 1.7  1998/12/28 17:56:27  vakatov
* New CVS and development tree structure for the NCBI C++ projects
*
* Revision 1.6  1998/12/01 01:19:47  vakatov
* Moved <string> and <stdexcept> after the <ncbidiag.hpp> to avoid weird
* conflicts under MSVC++ 6.0
*
* Revision 1.5  1998/11/24 17:51:26  vakatov
* + CParseException
* Removed "Ncbi" sub-prefix from the NCBI exception class names
*
* Revision 1.4  1998/11/10 01:17:36  vakatov
* Cleaned, adopted to the standard NCBI C++ framework and incorporated
* the "hardware exceptions" code and tests(originally written by
* V.Sandomirskiy).
* Only tested for MSVC++ compiler yet -- to be continued for SunPro...
*
* Revision 1.3  1998/11/06 22:42:38  vakatov
* Introduced BEGIN_, END_ and USING_ NCBI_SCOPE macros to put NCBI C++
* API to namespace "ncbi::" and to use it by default, respectively
* Introduced THROWS_NONE and THROWS(x) macros for the exception
* specifications
* Other fixes and rearrangements throughout the most of "corelib" code
*
* ==========================================================================
*/

#include <corelib/ncbidiag.hpp>
#include <string>
#include <stdexcept>


// (BEGIN_NCBI_SCOPE must be followed by END_NCBI_SCOPE later in this file)
BEGIN_NCBI_SCOPE


// The use of C++ exception specification mechanism, like:
//   "f(void) throw();"       <==  "f(void) THROWS_NONE;"
//   "g(void) throw(e1,e2);"  <==  "f(void) THROWS((e1,e2));"
#if defined(NCBI_USE_THROW_SPEC)
#  define THROWS_NONE throw()
#  define THROWS(x) throw x
#else
#  define THROWS_NONE
#  define THROWS(x)
#endif


// Standard handling of "exception"-derived exceptions
#define STD_CATCH(message) \
catch (exception& e) \
{ \
      CNcbiDiag diag; \
      diag << Error << "[" << message << "]" << "Exception: " << e.what(); \
}

// Standard handling of "exception"-derived and all other exceptions
#define STD_CATCH_ALL(message) \
STD_CATCH(message) \
    catch (...) \
{ \
      CNcbiDiag diag; \
      diag << Error << "[" << message << "]" << "Unknown exception"; \
}


/////////////////////////////////
// Auxiliary exception classes
//   CErrnoException
//   CParseException
//

class CErrnoException : public runtime_error {
    int m_Errno;
public:
    // Report description of "errno" along with "what"
    CErrnoException(const string& what) throw();
    int GetErrno(void) const THROWS_NONE { return m_Errno; }
};


class CParseException : public runtime_error {
    SIZE_TYPE m_Pos;
public:
    // Report "pos" along with "what"
    CParseException(const string& what, SIZE_TYPE pos) throw();
    SIZE_TYPE GetPos(void) const THROWS_NONE { return m_Pos; }
};


// (END_NCBI_SCOPE must be preceeded by BEGIN_NCBI_SCOPE)
END_NCBI_SCOPE

#endif  /* NCBIEXPT__HPP */
