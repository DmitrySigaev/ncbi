#ifndef NCBISTD__HPP
#define NCBISTD__HPP

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
*   The NCBI C++ standard #include's and #defin'itions
*
* --------------------------------------------------------------------------
* $Log$
* Revision 1.21  1999/05/28 20:12:29  vakatov
* [HAVE_NO_AUTO_PTR]  Prohibit "operator=" in the home-made "auto_ptr::"
*
* Revision 1.20  1999/04/16 17:45:31  vakatov
* [MSVC++] Replace the <windef.h>'s min/max macros by the hand-made templates.
*
* Revision 1.19  1999/04/15 21:56:47  vakatov
* Introduced NcbiMin/NcbiMax to workaround some portability issues with
* the standard "min/max"
*
* Revision 1.18  1999/04/14 21:20:31  vakatov
* Dont use "snprintf()" as it is not quite portable yet
*
* Revision 1.17  1999/04/14 19:46:01  vakatov
* Fixed for the features:
*    { NCBI_OBSOLETE_STR_COMPARE, HAVE_NO_AUTO_PTR, HAVE_NO_SNPRINTF }
*
* Revision 1.16  1999/04/09 19:51:36  sandomir
* minor changes in NStr::StringToXXX - base added
*
* Revision 1.15  1999/01/21 16:18:04  sandomir
* minor changes due to NStr namespace to contain string utility functions
*
* Revision 1.14  1999/01/11 22:05:45  vasilche
* Fixed CHTML_font size.
* Added CHTML_image input element.
*
* Revision 1.13  1998/12/28 17:56:29  vakatov
* New CVS and development tree structure for the NCBI C++ projects
*
* Revision 1.12  1998/12/21 17:19:36  sandomir
* VC++ fixes in ncbistd; minor fixes in Resource
*
* Revision 1.11  1998/12/17 21:50:43  sandomir
* CNCBINode fixed in Resource; case insensitive string comparison predicate added
*
* Revision 1.10  1998/12/15 17:38:16  vasilche
* Added conversion functions string <> int.
*
* Revision 1.9  1998/12/04 23:36:30  vakatov
* + NcbiEmptyCStr and NcbiEmptyString (const)
*
* Revision 1.8  1998/11/06 22:42:38  vakatov
* Introduced BEGIN_, END_ and USING_ NCBI_SCOPE macros to put NCBI C++
* API to namespace "ncbi::" and to use it by default, respectively
* Introduced THROWS_NONE and THROWS(x) macros for the exception
* specifications
* Other fixes and rearrangements throughout the most of "corelib" code
*
* ==========================================================================
*/

#include <corelib/ncbitype.h>
#include <corelib/ncbistl.hpp>
#include <corelib/ncbidbg.hpp>
#include <corelib/ncbiexpt.hpp>

// (BEGIN_NCBI_SCOPE must be followed by END_NCBI_SCOPE later in this file)
BEGIN_NCBI_SCOPE

const char   NcbiEmptyCStr[] = "";
const string NcbiEmptyString;
// tools

struct NStr {

    // conversion functions
    static int StringToInt(const string& str, int base = 10);
    static unsigned int StringToUInt(const string& str, int base = 10);
    static double StringToDouble(const string& str);
    static string IntToString(int value);
    static string IntToString(int value, bool sign);
    static string UIntToString(unsigned int value);
    static string DoubleToString(double value);

    /*  str[pos:pos+n) == pattern  --> return 0
     *  str[pos:pos+n) <  pattern  --> return negative mismatch position
     *  str[pos:pos+n) >  pattern  --> return positive mismatch position
     */
    static int Compare(const string& str, SIZE_TYPE pos, SIZE_TYPE n,
                       const char* pattern) {
#if defined(NCBI_OBSOLETE_STR_COMPARE)
        return str.compare(pattern, pos, n);
#else
        return str.compare(pos, n, pattern);
#endif
    }
    static int Compare(const string& str, SIZE_TYPE pos, SIZE_TYPE n,
                       const string& pattern) {
#if defined(NCBI_OBSOLETE_STR_COMPARE)
        return str.compare(pattern, pos, n);
#else
        return str.compare(pos, n, pattern);
#endif
    }
    
    static bool StartsWith(const string& str, const string& start) {
        return str.size() >= start.size()  &&
            Compare(str, (SIZE_TYPE)0, start.size(), start) == 0;
    }
    
    static bool EndsWith(const string& str, const string& end) {
        SIZE_TYPE pos = str.size() - end.size();
        return pos >= 0  &&
            Compare(str, pos, end.size(), end) == 0;
    }

}; // struct NStr
       
// predicates

// case-insensitive string comparison
struct PNocase
{
    bool operator() ( const string&, const string& ) const;
};

// algorithms

template<class Pred>
bool AStrEquiv( const string& x, const string& y, Pred pr )
{  
  return !( pr( x, y ) || pr( y, x ) );
}


// auto_ptr

#if defined(HAVE_NO_AUTO_PTR)
template <class X> class auto_ptr {
public:
    auto_ptr(X* p = 0) : m_Ptr(p) {}
    ~auto_ptr(void) { delete m_Ptr; }

    X&  operator*(void)         const { return *m_Ptr; }
    X*  operator->(void)        const { return m_Ptr; }
    int operator==(const X* p)  const { return (m_Ptr == p); }
    X*  get(void)               const { return m_Ptr; }

    X* release(void) {
        X* p = m_Ptr;
        m_Ptr = 0;
        return p;
    }

    void reset(X* p = 0) {
        if (m_Ptr != p) {
            delete m_Ptr;
            m_Ptr = p;
        }
    }

protected:
    X* m_Ptr;

private:
    // prohibited!
    auto_ptr(auto_ptr<X>&)               { throw 0; }
    auto_ptr<X>& operator=(auto_ptr<X>&) { throw 0; }
    auto_ptr<X>& operator=(X* p)         { throw 0; }
};
#endif /* HAVE_NO_AUTO_PTR */


// dont use min/max from MSVC++ <windef.h>
#if defined(HAVE_WINDOWS_H)
#define NOMINMAX
#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif
template <class T>
inline const T& min(const T& a, const T& b) {
  return b < a ? b : a;
}
template <class T>
inline const T& max(const T& a, const T& b) {
  return  a < b ? b : a;
}
#endif /* min && max */


// (END_NCBI_SCOPE must be preceeded by BEGIN_NCBI_SCOPE)
END_NCBI_SCOPE

#endif /* NCBISTD__HPP */
