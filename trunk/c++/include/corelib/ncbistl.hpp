#ifndef NCBISTL__HPP
#define NCBISTL__HPP

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
*   The NCBI C++/STL use hints
*
* --------------------------------------------------------------------------
* $Log$
* Revision 1.25  2001/12/14 17:41:58  vakatov
* [MSVC++] Added pragma to get rid of warning:
*   "identifier was truncated to '255' characters in the browser information"
*
* Revision 1.24  2001/10/12 19:32:54  ucko
* move BREAK to a central location; move CBioseq::GetTitle to object manager
*
* Revision 1.23  2001/09/18 17:42:53  grichenk
* Disabled warning 4355
*
* Revision 1.22  2001/08/31 20:05:42  ucko
* Fix ICC build.
*
* Revision 1.21  2001/05/21 21:46:17  vakatov
* SIZE_TYPE, NPOS -- moved from <ncbistl.hpp> to <ncbistr.hpp> and
* made non-macros (to avoid possible name clashes)
*
* Revision 1.20  2000/12/23 05:46:05  vakatov
* [MSVC++]  disable warning C4250
*
* Revision 1.19  2000/11/09 18:14:37  vasilche
* Fixed nonstandard behaviour of 'for' statement on MS VC.
*
* Revision 1.18  2000/07/03 18:41:16  vasilche
* Added prefix to NCBI_EAT_SEMICOLON generated typedef to avoid possible name conflict.
*
* Revision 1.17  2000/06/01 15:08:33  vakatov
* + USING_SCOPE(ns)
*
* Revision 1.16  2000/04/18 19:24:33  vasilche
* Added BEGIN_SCOPE and END_SCOPE macros to allow source brawser gather names from namespaces.
*
* Revision 1.15  2000/04/06 16:07:54  vasilche
* Added NCBI_EAT_SEMICOLON macro to allow tailing semicolon without warning.
*
* Revision 1.14  1999/12/28 18:55:25  vasilche
* Reduced size of compiled object files:
* 1. avoid inline or implicit virtual methods (especially destructors).
* 2. avoid std::string's methods usage in inline methods.
* 3. avoid string literals ("xxx") in inline methods.
*
* Revision 1.13  1999/10/12 21:46:31  vakatov
* Assume all supported compilers are namespace-capable and have "std::"
*
* Revision 1.12  1999/05/13 16:43:29  vakatov
* Added Mac(#NCBI_OS_MAC) to the list of supported platforms
* Also, use #NCBI_OS to specify a string representation of the
* current platform("UNIX", "WINDOWS", or "MAC")
*
* Revision 1.11  1999/05/10 14:26:07  vakatov
* Fixes to compile and link with the "egcs" C++ compiler under Linux
*
* Revision 1.10  1999/04/14 19:41:19  vakatov
* [MSVC++] Added pragma's to get rid of some warnings
*
* Revision 1.9  1998/12/03 15:47:50  vakatov
* Scope fixes for #NPOS and #SIZE_TYPE;  redesigned the std:: and ncbi::
* scope usage policy definitions(also, added #NCBI_NS_STD and #NCBI_NS_NCBI)
*
* Revision 1.8  1998/12/01 01:18:42  vakatov
* [HAVE_NAMESPACE]  Added an empty fake "ncbi" namespace -- MSVC++ 6.0
* compiler starts to recognize "std::" scope only after passing through at
* least one "namespace ncbi {}" statement(?!)
*
* Revision 1.7  1998/11/13 00:13:51  vakatov
* Decide whether it NCBI_OS_UNIX or NCBI_OS_MSWIN
*
* Revision 1.6  1998/11/10 01:13:40  vakatov
* Moved "NCBI_USING_NAMESPACE_STD:" to the first(fake) definition of
* namespace "ncbi" -- no need to "using ..." it in every new "ncbi"
* scope definition...
*
* Revision 1.5  1998/11/06 22:42:39  vakatov
* Introduced BEGIN_, END_ and USING_ NCBI_SCOPE macros to put NCBI C++
* API to namespace "ncbi::" and to use it by default, respectively
* Introduced THROWS_NONE and THROWS(x) macros for the exception
* specifications
* Other fixes and rearrangements throughout the most of "corelib" code
* ==========================================================================
*/

#include <ncbiconf.h>

// Get rid of some warnings in MSVC++ 6.00
#if (_MSC_VER >= 1200)
// too long identificator name in the debug info;  truncated
#  pragma warning(disable: 4786)
// too long decorated name;  truncated
#  pragma warning(disable: 4503)
// default copy constructor cannot be generated
#  pragma warning(disable: 4511)
// default assignment operator cannot be generated
#  pragma warning(disable: 4512)
// synonymous name used
#  pragma warning(disable: 4097)
// inherits ... via dominance
#  pragma warning(disable: 4250)
// 'this' : used in base member initializer list
#  pragma warning(disable: 4355)
// identifier was truncated to '255' characters in the browser information
#  pragma warning(disable: 4786)
#endif /* _MSC_VER >= 1200 */


#define BEGIN_SCOPE(ns) namespace ns {
#define END_SCOPE(ns) }
#define USING_SCOPE(ns) using namespace ns

// Using STD and NCBI namespaces
#define NCBI_NS_STD  std
#define NCBI_USING_NAMESPACE_STD using namespace NCBI_NS_STD

#define NCBI_NS_NCBI ncbi
#define BEGIN_NCBI_SCOPE BEGIN_SCOPE(NCBI_NS_NCBI)
#define END_NCBI_SCOPE   END_SCOPE(NCBI_NS_NCBI)
#define USING_NCBI_SCOPE USING_SCOPE(NCBI_NS_NCBI)


// Magic spells ;-) needed for some weird compilers... very empiric
namespace NCBI_NS_STD  { /* the fake one */ }
namespace NCBI_NS_NCBI { /* the fake one, +"std" */ NCBI_USING_NAMESPACE_STD; }
namespace NCBI_NS_NCBI { /* the fake one */ }


// name concatenation
#if !defined(NCBI_NAME2)
#  define NCBI_NAME2(Name1, Name2) Name1##Name2
#endif
#if !defined(NCBI_NAME3)
#  define NCBI_NAME3(Name1, Name2, Name3) Name1##Name2##Name3
#endif
#if !defined(NCBI_EAT_SEMICOLON)
#  define NCBI_EAT_SEMICOLON(UniqueName) \
typedef int NCBI_NAME2(T_EAT_SEMICOLON_,UniqueName)
#endif


// fix nonstandard 'for' statement behaviour on MSVC
#if defined(NCBI_OS_MSWIN) && !defined(for)
# define for if(0);else for
#endif

// ICC may fail to generate code preceded by "template<>"
#ifdef NCBI_COMPILER_ICC
#define EMPTY_TEMPLATE
#else
#define EMPTY_TEMPLATE template<>
#endif

// Sun WorkShop fails to call destructors for objects created in
// for-loop initializers; this macro prevents trouble with iterators
// that contain CRefs by advancing them to the end, avoiding
// "deletion of referenced CObject" errors.
#ifdef NCBI_COMPILER_WORKSHOP
#define BREAK(it) while (it) { ++(it); }  break
#else
#define BREAK(it) break
#endif

#endif /* NCBISTL__HPP */
