#ifndef NCBISTRE__HPP
#define NCBISTRE__HPP

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
*   NCBI C++ stream class wrappers
*   Triggering between "new" and "old" C++ stream libraries
*
* --------------------------------------------------------------------------
* $Log$
* Revision 1.8  1998/11/26 00:27:05  vakatov
* Added <iomanip[.h]> and relevant #define's NcbiXXX
*
* Revision 1.7  1998/11/06 22:42:39  vakatov
* Introduced BEGIN_, END_ and USING_ NCBI_SCOPE macros to put NCBI C++
* API to namespace "ncbi::" and to use it by default, respectively
* Introduced THROWS_NONE and THROWS(x) macros for the exception
* specifications
* Other fixes and rearrangements throughout the most of "corelib" code
*
* ==========================================================================
*/

#include <ncbistl.hpp>

// Determine which iostream library to use, include appropriate
// headers, and #define specific preprocessor variables
#if !defined(HAVE_IOSTREAM_H)
#  define NCBI_USE_NEW_IOSTREAM
#endif

#if defined(HAVE_IOSTREAM)  &&  defined(NCBI_USE_NEW_IOSTREAM)
#  include <iostream>
#  include <fstream>
#  include <strstream>
#  include <iomanip>
#  if defined(NCBI_NO_NAMESPACE)
#    define IO_PREFIX
#  else
#    define IO_PREFIX  std
#  endif
#  define IOS_BASE   IO_PREFIX::ios_base
#  define SEEKOFF    pubseekoff

#elif defined(HAVE_IOSTREAM_H)
#  include <iostream.h>
#  include <fstream.h>
#  if defined(HAVE_STRSTREA_H)
#    include <strstrea.h>
#  else
#    include <strstream.h>
#  endif
#  include <iomanip.h>
#  define IO_PREFIX
#  define IOS_BASE   ::ios
#  define SEEKOFF    seekoff

#else
#  error "Cannot find neither <iostream> nor <iostream.h>!"
#endif

// (BEGIN_NCBI_SCOPE must be followed by END_NCBI_SCOPE later in this file)
BEGIN_NCBI_SCOPE


// I/O classes
typedef IO_PREFIX::streampos     CNcbiStreampos;
typedef IO_PREFIX::streamoff     CNcbiStreamoff;

typedef IO_PREFIX::ios           CNcbiIos;

typedef IO_PREFIX::streambuf     CNcbiStreambuf;
typedef IO_PREFIX::istream       CNcbiIstream;
typedef IO_PREFIX::ostream       CNcbiOstream;
typedef IO_PREFIX::iostream      CNcbiIostream;

typedef IO_PREFIX::strstreambuf  CNcbiStrstreambuf;
typedef IO_PREFIX::istrstream    CNcbiIstrstream;
typedef IO_PREFIX::ostrstream    CNcbiOstrstream;
typedef IO_PREFIX::strstream     CNcbiStrstream;

typedef IO_PREFIX::filebuf       CNcbiFilebuf;
typedef IO_PREFIX::ifstream      CNcbiIfstream;
typedef IO_PREFIX::ofstream      CNcbiOfstream;
typedef IO_PREFIX::fstream       CNcbiFstream;

// Standard I/O streams
#define NcbiCin   IO_PREFIX::cin
#define NcbiCout  IO_PREFIX::cout
#define NcbiCerr  IO_PREFIX::cerr
#define NcbiClog  IO_PREFIX::clog

// I/O manipulators
#define NcbiEndl   IO_PREFIX::endl
#define NcbiEnds   IO_PREFIX::ends
#define NcbiFlush  IO_PREFIX::flush
#define NcbiDec    IO_PREFIX::dec
#define NcbiHex    IO_PREFIX::hex
#define NcbiOct    IO_PREFIX::oct
#define NcbiWs     IO_PREFIX::ws

#define NcbiSetbase       IO_PREFIX::setbase
#define NcbiResetiosflags IO_PREFIX::resetiosflags
#define NcbiSetiosflags   IO_PREFIX::setiosflags
#define NcbiSetfill       IO_PREFIX::setfill
#define NcbiSetprecision  IO_PREFIX::setprecision
#define NcbiSetw          IO_PREFIX::setw


// (END_NCBI_SCOPE must be preceeded by BEGIN_NCBI_SCOPE)
END_NCBI_SCOPE

#endif /* NCBISTRE__HPP */
