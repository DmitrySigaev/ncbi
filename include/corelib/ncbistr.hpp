#ifndef CORELIB___NCBISTR__HPP
#define CORELIB___NCBISTR__HPP

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
 * Authors:  Eugene Vasilchenko, Denis Vakatov
 *
 * File Description:
 *   The NCBI C++ standard methods for dealing with std::string
 *
 */

#include <corelib/ncbitype.h>
#include <corelib/ncbiexpt.hpp>
#include <corelib/ncbistl.hpp>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <string>
#include <list>
#include <vector>


/** @addtogroup String
 *
 * @{
 */


BEGIN_NCBI_SCOPE


/// Empty "C" string (points to a '\0')
NCBI_XNCBI_EXPORT extern const char *const kEmptyCStr;
#define NcbiEmptyCStr NCBI_NS_NCBI::kEmptyCStr


/// Empty "C++" string
class NCBI_XNCBI_EXPORT CNcbiEmptyString
{
public:
    static const string& Get(void);
private:
    static const string& FirstGet(void);
    static const string* m_Str;
};
#define NcbiEmptyString NCBI_NS_NCBI::CNcbiEmptyString::Get()
#define kEmptyStr       NcbiEmptyString


// SIZE_TYPE and NPOS

typedef NCBI_NS_STD::string::size_type SIZE_TYPE;
static const SIZE_TYPE NPOS = NCBI_NS_STD::string::npos;


/////////////////////////////////////////////////////////////////////////////
//  NStr::
//    String-processing utilities
//

class NCBI_XNCBI_EXPORT NStr
{
public:
    /// Convert "str" to a (non-negative) "int" value.
    /// Return "-1" if "str" contains any symbols other than [0-9], or
    /// if it represents a number that does not fit into "int".
    static int StringToNumeric(const string& str);

    /// Whether to prohibit trailing symbols (any symbol but '\0')
    /// in the StringToXxx() conversion functions below
    enum ECheckEndPtr {
        eCheck_Need,   /// Check is necessary
        eCheck_Skip    /// Skip this check
    };

    /// String-to-X conversion functions (throw exception on conversion error).
    /// If "check" is eCheck_Skip then "str" can have trailing symbols
    /// after the number;  otherwise exception will be thrown if there
    /// are trailing symbols present after the converted number.
    static int           StringToInt    (const string& str, int base = 10,
                                         ECheckEndPtr check = eCheck_Need);
    static unsigned int  StringToUInt   (const string& str, int base = 10,
                                         ECheckEndPtr check = eCheck_Need);
    static long          StringToLong   (const string& str, int base = 10,
                                         ECheckEndPtr check = eCheck_Need);
    static unsigned long StringToULong  (const string& str, int base = 10,
                                         ECheckEndPtr check = eCheck_Need);
    static double        StringToDouble (const string& str,
                                         ECheckEndPtr check = eCheck_Need);
    static Int8          StringToInt8   (const string& str);
    static Uint8         StringToUInt8  (const string& str, int base = 10);
    static const void*   StringToPtr    (const string& str);

    /// X-to-String conversion functions
    static string    IntToString   (long value, bool sign = false);
    static string    UIntToString  (unsigned long value);
    static string    Int8ToString  (Int8 value, bool sign = false);
    static string    UInt8ToString (Uint8 value);
    static string    DoubleToString(double value);
    /// Note: If precission is more that maximum for current platform,
    //        then it will be truncated to this maximum.
    static string    DoubleToString(double value, unsigned int precision);
    /// Put result of the conversion into buffer "buf" size of "buf_size".
    /// Return the number of bytes stored in "buf", not counting the
    /// terminating '\0'.
    static SIZE_TYPE DoubleToString(double value, unsigned int precision,
                                    char* buf, SIZE_TYPE buf_size);
    static string PtrToString      (const void* ptr);

    /// Return one of: 'true, 'false'
    static const string& BoolToString(bool value);
    /// Can recognize (case-insensitive) one of:  'true, 't', 'false', 'f'
    static bool          StringToBool(const string& str);


    /// String comparison
    enum ECase {
        eCase,
        eNocase  /// ignore character case
    };

    /// ATTENTION.  Be aware that:
    ///
    /// 1) "Compare***(..., SIZE_TYPE pos, SIZE_TYPE n, ...)" functions
    ///    follow the ANSI C++ comparison rules a la "basic_string::compare()":
    ///       str[pos:pos+n) == pattern   --> return 0
    ///       str[pos:pos+n) <  pattern   --> return negative value
    ///       str[pos:pos+n) >  pattern   --> return positive value
    ///
    /// 2) "strn[case]cmp()" functions follow the ANSI C comparison rules:
    ///       str[0:n) == pattern[0:n)   --> return 0
    ///       str[0:n) <  pattern[0:n)   --> return negative value
    ///       str[0:n) >  pattern[0:n)   --> return positive value

    static int CompareCase(const string& str, SIZE_TYPE pos, SIZE_TYPE n,
                           const char* pattern);
    static int CompareCase(const string& str, SIZE_TYPE pos, SIZE_TYPE n,
                           const string& pattern);
    static int CompareCase(const char* s1, const char* s2);

    static int CompareCase(const string& s1, const string& s2);

    static int CompareNocase(const string& str, SIZE_TYPE pos, SIZE_TYPE n,
                             const char* pattern);
    static int CompareNocase(const string& str, SIZE_TYPE pos, SIZE_TYPE n,
                             const string& pattern);
    static int CompareNocase(const char* s1, const char* s2);

    static int CompareNocase(const string& s1, const string& s2);

    static int Compare(const string& str, SIZE_TYPE pos, SIZE_TYPE n,
                       const char* pattern, ECase use_case = eCase);
    static int Compare(const string& str, SIZE_TYPE pos, SIZE_TYPE n,
                       const string& pattern, ECase use_case = eCase);
    static int Compare(const char* s1, const char* s2,
                       ECase use_case = eCase);
    static int Compare(const string& s1, const char* s2,
                       ECase use_case = eCase);
    static int Compare(const char* s1, const string& s2,
                       ECase use_case = eCase);
    static int Compare(const string& s1, const string& s2,
                       ECase use_case = eCase);

    // NOTE.  On some platforms, "strn[case]cmp()" can work faster than their
    //        "Compare***()" counterparts.
    static int strcmp      (const char* s1, const char* s2);
    static int strncmp     (const char* s1, const char* s2, size_t n);
    static int strcasecmp  (const char* s1, const char* s2);
    static int strncasecmp (const char* s1, const char* s2, size_t n);

    // Wrapper for the function strftime() that correct handling %D and %T
    // time formats on MS Windows
    static size_t strftime (char* s, size_t maxsize, const char* format,
                            const struct tm* timeptr);

    /// The following 4 methods change the passed string, then return it
    static string& ToLower(string& str);
    static char*   ToLower(char*   str);
    static string& ToUpper(string& str);
    static char*   ToUpper(char*   str);
    // ...and these two are just dummies to prohibit passing constant C strings
private:
    static void/*dummy*/ ToLower(const char* /*dummy*/);
    static void/*dummy*/ ToUpper(const char* /*dummy*/);
public:

    static bool StartsWith(const string& str, const string& start,
                           ECase use_case = eCase);
    static bool EndsWith(const string& str, const string& end,
                         ECase use_case = eCase);

    enum EOccurrence {
        eFirst,
        eLast
    };

    // Return the start of the first or last (depending on WHICH)
    // occurrence of PATTERN in STR that *starts* within [START, END],
    // or NPOS if it does not occur.
    static SIZE_TYPE Find      (const string& str, const string& pattern,
                                SIZE_TYPE start = 0, SIZE_TYPE end = NPOS,
                                EOccurrence which = eFirst,
                                ECase use_case = eCase);
    static SIZE_TYPE FindCase  (const string& str, const string& pattern,
                                SIZE_TYPE start = 0, SIZE_TYPE end = NPOS,
                                EOccurrence which = eFirst);
    static SIZE_TYPE FindNoCase(const string& str, const string& pattern,
                                SIZE_TYPE start = 0, SIZE_TYPE end = NPOS,
                                EOccurrence which = eFirst);

    enum ETrunc {
        eTrunc_Begin,  /// truncate leading  spaces only
        eTrunc_End,    /// truncate trailing spaces only
        eTrunc_Both    /// truncate spaces at both begin and end of string
    };
    static string TruncateSpaces(const string& str, ETrunc where=eTrunc_Both);

    /// Starting from position "start_pos", replace no more than "max_replace"
    /// occurrences of substring "search" by string "replace".
    /// If "max_replace" is zero -- then replace all occurrences.
    /// Result will be put to string "dst", and it will be returned as well.
    static string& Replace(const string& src,
                           const string& search,
                           const string& replace,
                           string& dst,
                           SIZE_TYPE start_pos = 0, size_t max_replace = 0);

    /// The same as the above Replace(), but return new string
    static string Replace(const string& src,
                          const string& search,
                          const string& replace,
                          SIZE_TYPE start_pos = 0, size_t max_replace = 0);

    // Whether to merge adjacent delimiters in Split and Tokenize
    enum EMergeDelims {
        eNoMergeDelims,
        eMergeDelims
    };


    /// Split string "str" using symbols from "delim" as delimiters.
    /// Delimiters which immediately follow each other are treated as one
    /// delimiter by default (unlike that in Tokenize()).
    /// Add the resultant tokens to the list "arr". Return "arr".
    static list<string>& Split(const string& str,
                               const string& delim,
                               list<string>& arr,
                               EMergeDelims  merge = eMergeDelims);

    /// Tokenize string "str" using symbols from "delim" as delimeters.
    /// Add the resultant tokens to the vector "arr". Return ref. to "arr".
    /// If delimiter is empty, then input string is appended to "arr" as is.
    static vector<string>& Tokenize(const string&   str,
                                    const string&   delim,
                                    vector<string>& arr,
                                    EMergeDelims    merge = eNoMergeDelims);

    /// Join the strings in "arr" into a single string, separating
    /// each pair with "delim".
    static string Join(const list<string>& arr, const string& delim);

    /// Make a printable version of "str". The non-printable characters will
    /// be represented as "\r", "\n", "\v", "\t", "\0", "\\", or
    /// "\xDD" where DD is the character's code in hexadecimal.
    enum ENewLineMode {
        eNewLine_Quote,         // display "\n" instead of actual linebreak
        eNewLine_Passthru       // break the line on every "\n" occurrance
    };

    static string PrintableString(const string& str,
                                  ENewLineMode  nl_mode = eNewLine_Quote);

    enum EWrapFlags {
        fWrap_Hyphenate  = 0x1, // add a hyphen when breaking words?
        fWrap_HTMLPre    = 0x2  // wrap as preformatted HTML?
    };
    typedef int TWrapFlags; // binary OR of "EWrapFlags"

    /// Split string "str" into lines of width "width" and add the
    /// resulting lines to the list "arr" (returned).  Normally, all
    /// lines will begin with "prefix" (counted against "width"),
    /// but the first line will instead begin with "prefix1" if
    /// you supply it.
    static list<string>& Wrap(const string& str, SIZE_TYPE width,
                              list<string>& arr, TWrapFlags flags = 0,
                              const string* prefix = 0,
                              const string* prefix1 = 0);

    static list<string>& Wrap(const string& str, SIZE_TYPE width,
                              list<string>& arr, TWrapFlags flags,
                              const string& prefix, const string* prefix1 = 0);

    static list<string>& Wrap(const string& str, SIZE_TYPE width,
                              list<string>& arr, TWrapFlags flags,
                              const string& prefix, const string& prefix1);

    /// Similar to the above, but tries to avoid splitting any elements of l.
    /// Delim only applies between elements on the same line; if you want
    /// everything to end with commas or such, you should add them first.
    static list<string>& WrapList(const list<string>& l, SIZE_TYPE width,
                                  const string& delim, list<string>& arr,
                                  TWrapFlags flags = 0,
                                  const string* prefix = 0,
                                  const string* prefix1 = 0);

    static list<string>& WrapList(const list<string>& l, SIZE_TYPE width,
                                  const string& delim, list<string>& arr,
                                  TWrapFlags flags, const string& prefix,
                                  const string* prefix1 = 0);

    static list<string>& WrapList(const list<string>& l, SIZE_TYPE width,
                                  const string& delim, list<string>& arr,
                                  TWrapFlags flags, const string& prefix,
                                  const string& prefix1);
}; // class NStr


/////////////////////////////////////////////////////////////////////////////
// CStringException - exceptions generated by strings


class NCBI_XNCBI_EXPORT CStringException : public CParseTemplException<CCoreException>
{
public:
    enum EErrCode {
        eConvert,
        eBadArgs,
        eFormat
    };
    virtual const char* GetErrCodeString(void) const
    {
        switch (GetErrCode()) {
        case eConvert:  return "eConvert";
        case eBadArgs:  return "eBadArgs";
        case eFormat:   return "eFormat";
        default:    return CException::GetErrCodeString();
        }
    }
    NCBI_EXCEPTION_DEFAULT2(CStringException,
        CParseTemplException<CCoreException>,std::string::size_type);
};

/////////////////////////////////////////////////////////////////////////////
//  Predicates
//


/// Case-sensitive string comparison
struct PCase
{
    /// Return difference between "s1" and "s2"
    int Compare(const string& s1, const string& s2) const;

    /// Return TRUE if s1 < s2
    bool Less(const string& s1, const string& s2) const;

    /// Return TRUE if s1 == s2
    bool Equals(const string& s1, const string& s2) const;

    /// Return TRUE if s1 < s2
    bool operator()(const string& s1, const string& s2) const;
};


/// Case-INsensitive string comparison
struct PNocase
{
    /// Return difference between "s1" and "s2"
    int Compare(const string& s1, const string& s2) const;

    /// Return TRUE if s1 < s2
    bool Less(const string& s1, const string& s2) const;

    /// Return TRUE if s1 == s2
    bool Equals(const string& s1, const string& s2) const;

    /// Return TRUE if s1 < s2 ignoring case
    bool operator()(const string& s1, const string& s2) const;
};



/////////////////////////////////////////////////////////////////////////////
//  Algorithms
//


/// Check equivalence of arguments using predicate
template<class Arg1, class Arg2, class Pred>
inline
bool AStrEquiv(const Arg1& x, const Arg2& y, Pred pr)
{
    return pr.Equals(x, y);
}


/* @} */


/////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////
//  IMPLEMENTATION of INLINE functions
/////////////////////////////////////////////////////////////////////////////



/////////////////////////////////////////////////////////////////////////////
//  CNcbiEmptyString::
//

inline
const string& CNcbiEmptyString::Get(void)
{
    const string* str = m_Str;
    return str ? *str: FirstGet();
}



/////////////////////////////////////////////////////////////////////////////
//  NStr::
//


inline
int NStr::strcmp(const char* s1, const char* s2)
{
    return ::strcmp(s1, s2);
}

inline
int NStr::strncmp(const char* s1, const char* s2, size_t n)
{
    return ::strncmp(s1, s2, n);
}

inline
int NStr::strcasecmp(const char* s1, const char* s2)
{
#if defined(HAVE_STRICMP)
    return ::stricmp(s1, s2);

#elif defined(HAVE_STRCASECMP)
    return ::strcasecmp(s1, s2);

#else
    int diff = 0;
    for ( ;; ++s1, ++s2) {
        char c1 = *s1;
        // calculate difference
        diff = toupper(c1) - toupper(*s2);
        // if end of string or different
        if (!c1  ||  diff)
            break; // return difference
    }
    return diff;
#endif
}

inline
int NStr::strncasecmp(const char* s1, const char* s2, size_t n)
{
#if defined(HAVE_STRICMP)
    return ::strnicmp(s1, s2, n);

#elif defined(HAVE_STRCASECMP)
    return ::strncasecmp(s1, s2, n);

#else
    int diff = 0;
    for ( ; ; ++s1, ++s2, --n) {
        char c1 = *s1;
        // calculate difference
        diff = toupper(c1) - toupper(*s2);
        // if end of string or different
        if (!c1  ||  diff)
            break; // return difference
        if (n == 0)
            return 0;
    }
    return diff;
#endif
}

inline
size_t NStr::strftime(char* s, size_t maxsize, const char* format,
                      const struct tm* timeptr)
{
#if defined(NCBI_COMPILER_MSVC)
    string x_format;
    x_format = Replace(format,   "%T", "%H:%M:%S");
    x_format = Replace(x_format, "%D", "%m/%d/%y");
    return ::strftime(s, maxsize, x_format.c_str(), timeptr);
#else
    return ::strftime(s, maxsize, format, timeptr);
#endif
}

inline
int NStr::Compare(const string& str, SIZE_TYPE pos, SIZE_TYPE n,
                  const char* pattern, ECase use_case)
{
    return use_case == eCase ?
        CompareCase(str, pos, n, pattern): CompareNocase(str, pos, n, pattern);
}

inline
int NStr::Compare(const string& str, SIZE_TYPE pos, SIZE_TYPE n,
                  const string& pattern, ECase use_case)
{
    return use_case == eCase ?
        CompareCase(str, pos, n, pattern): CompareNocase(str, pos, n, pattern);
}

inline
int NStr::CompareCase(const char* s1, const char* s2)
{
    return NStr::strcmp(s1, s2);
}

inline
int NStr::CompareNocase(const char* s1, const char* s2)
{
    return NStr::strcasecmp(s1, s2);
}

inline
int NStr::Compare(const char* s1, const char* s2, ECase use_case)
{
    return use_case == eCase ? CompareCase(s1, s2): CompareNocase(s1, s2);
}

inline
int NStr::Compare(const string& s1, const char* s2, ECase use_case)
{
    return Compare(s1.c_str(), s2, use_case);
}

inline
int NStr::Compare(const char* s1, const string& s2, ECase use_case)
{
    return Compare(s1, s2.c_str(), use_case);
}

inline
int NStr::Compare(const string& s1, const string& s2, ECase use_case)
{
    return Compare(s1.c_str(), s2.c_str(), use_case);
}

inline
int NStr::CompareCase(const string& s1, const string& s2)
{
    return CompareCase(s1.c_str(), s2.c_str());
}

inline
int NStr::CompareNocase(const string& s1, const string& s2)
{
    return CompareNocase(s1.c_str(), s2.c_str());
}

inline
bool NStr::StartsWith(const string& str, const string& start, ECase use_case)
{
    return str.size() >= start.size()  &&
        Compare(str, 0, start.size(), start, use_case) == 0;
}

inline
bool NStr::EndsWith(const string& str, const string& end, ECase use_case)
{
    return str.size() >= end.size()  &&
        Compare(str, str.size() - end.size(), end.size(), end, use_case) == 0;
}


inline
SIZE_TYPE NStr::Find(const string& str, const string& pattern,
                     SIZE_TYPE start, SIZE_TYPE end, EOccurrence where,
                     ECase use_case)
{
    return use_case == eCase ? FindCase(str, pattern, start, end, where)
        : FindNoCase(str, pattern, start, end, where);
}

inline
SIZE_TYPE NStr::FindCase(const string& str, const string& pattern,
                         SIZE_TYPE start, SIZE_TYPE end, EOccurrence where)
{
    if (where == eFirst) {
        SIZE_TYPE result = str.find(pattern, start);
        return (result == NPOS  ||  result > end) ? NPOS : result;
    } else {
        SIZE_TYPE result = str.rfind(pattern, end);
        return (result == NPOS  ||  result < start) ? NPOS : result;
    }
}

inline
SIZE_TYPE NStr::FindNoCase(const string& str, const string& pattern,
                           SIZE_TYPE start, SIZE_TYPE end, EOccurrence where)
{
    string str2 = str, pat2 = pattern;
    return FindCase(ToLower(str2), ToLower(pat2), start, end, where);
}


inline
list<string>& NStr::Wrap(const string& str, SIZE_TYPE width, list<string>& arr,
                         NStr::TWrapFlags flags, const string& prefix,
                         const string* prefix1)
{
    return Wrap(str, width, arr, flags, &prefix, prefix1);
}


inline
list<string>& NStr::Wrap(const string& str, SIZE_TYPE width, list<string>& arr,
                         NStr::TWrapFlags flags, const string& prefix,
                         const string& prefix1)
{
    return Wrap(str, width, arr, flags, &prefix, &prefix1);
}


inline
list<string>& NStr::WrapList(const list<string>& l, SIZE_TYPE width,
                             const string& delim, list<string>& arr,
                             NStr::TWrapFlags flags, const string& prefix,
                             const string* prefix1)
{
    return WrapList(l, width, delim, arr, flags, &prefix, prefix1);
}


inline
list<string>& NStr::WrapList(const list<string>& l, SIZE_TYPE width,
                             const string& delim, list<string>& arr,
                             NStr::TWrapFlags flags, const string& prefix,
                             const string& prefix1)
{
    return WrapList(l, width, delim, arr, flags, &prefix, &prefix1);
}




/////////////////////////////////////////////////////////////////////////////
//  PCase::
//

inline
int PCase::Compare(const string& s1, const string& s2) const
{
    return NStr::Compare(s1, s2, NStr::eCase);
}

inline
bool PCase::Less(const string& s1, const string& s2) const
{
    return Compare(s1, s2) < 0;
}

inline
bool PCase::Equals(const string& s1, const string& s2) const
{
    return Compare(s1, s2) == 0;
}

inline
bool PCase::operator()(const string& s1, const string& s2) const
{
    return Less(s1, s2);
}



/////////////////////////////////////////////////////////////////////////////
//  PNocase::
//

inline
int PNocase::Compare(const string& s1, const string& s2) const
{
    return NStr::Compare(s1, s2, NStr::eNocase);
}

inline
bool PNocase::Less(const string& s1, const string& s2) const
{
    return Compare(s1, s2) < 0;
}

inline
bool PNocase::Equals(const string& s1, const string& s2) const
{
    return Compare(s1, s2) == 0;
}

inline
bool PNocase::operator()(const string& s1, const string& s2) const
{
    return Less(s1, s2);
}


END_NCBI_SCOPE


/*
 * ===========================================================================
 *
 * $Log$
 * Revision 1.40  2003/03/31 13:31:06  siyan
 * Minor changes to doxygen support
 *
 * Revision 1.39  2003/03/31 13:22:02  siyan
 * Added doxygen support
 *
 * Revision 1.38  2003/02/26 16:42:27  siyan
 * Added base parameter to NStr::StringToUInt8 to support different radixes
 * such as base 10 (default), 16, 8, 2.
 *
 * Revision 1.37  2003/02/24 19:54:50  gouriano
 * use template-based exceptions instead of errno and parse exceptions
 *
 * Revision 1.36  2003/02/20 18:41:49  dicuccio
 * Added NStr::StringToPtr()
 *
 * Revision 1.35  2003/01/24 16:58:54  ucko
 * Add an optional parameter to Split and Tokenize indicating whether to
 * merge adjacent delimiters; drop suggestion about joining WrapList's
 * output with ",\n" because long items may be split across lines.
 *
 * Revision 1.34  2003/01/21 23:22:06  vakatov
 * NStr::Tokenize() to return reference, and not a new "vector<>".
 *
 * Revision 1.33  2003/01/21 20:08:28  ivanov
 * Added function NStr::DoubleToString(value, precision, buf, buf_size)
 *
 * Revision 1.32  2003/01/14 21:16:12  kuznets
 * +NStr::Tokenize
 *
 * Revision 1.31  2003/01/10 22:15:56  kuznets
 * Working on String2Int8
 *
 * Revision 1.30  2003/01/10 21:59:06  kuznets
 * +NStr::String2Int8
 *
 * Revision 1.29  2003/01/10 00:08:05  vakatov
 * + Int8ToString(),  UInt8ToString()
 *
 * Revision 1.28  2003/01/06 16:43:05  ivanov
 * + DoubleToString() with 'precision'
 *
 * Revision 1.27  2002/12/20 19:40:45  ucko
 * Add NStr::Find and variants.
 *
 * Revision 1.26  2002/12/18 22:53:21  dicuccio
 * Added export specifier for building DLLs in windows.  Added global list of
 * all such specifiers in mswin_exports.hpp, included through ncbistl.hpp
 *
 * Revision 1.25  2002/10/18 21:02:30  ucko
 * Drop obsolete (and misspelled!) reference to fWrap_UsePrefix1 from
 * usage description.
 *
 * Revision 1.24  2002/10/18 20:48:41  lavr
 * +ENewLine_Mode and '\n' translation in NStr::PrintableString()
 *
 * Revision 1.23  2002/10/16 19:29:17  ucko
 * +fWrap_HTMLPre
 *
 * Revision 1.22  2002/10/03 14:44:33  ucko
 * Tweak the interfaces to NStr::Wrap* to avoid publicly depending on
 * kEmptyStr, removing the need for fWrap_UsePrefix1 in the process; also
 * drop fWrap_FavorPunct, as WrapList should be a better choice for such
 * situations.
 *
 * Revision 1.21  2002/10/02 20:14:52  ucko
 * Add Join, Wrap, and WrapList functions to NStr::.
 *
 * Revision 1.20  2002/07/17 16:49:03  gouriano
 * added ncbiexpt.hpp include
 *
 * Revision 1.19  2002/07/15 18:17:52  gouriano
 * renamed CNcbiException and its descendents
 *
 * Revision 1.18  2002/07/11 14:17:55  gouriano
 * exceptions replaced by CNcbiException-type ones
 *
 * Revision 1.17  2002/06/18 16:03:49  ivanov
 * Fixed #ifdef clause in NStr::strftime()
 *
 * Revision 1.16  2002/06/18 15:19:36  ivanov
 * Added NStr::strftime() -- correct handling %D and %T time formats on MS Windows
 *
 * Revision 1.15  2002/05/02 15:38:02  ivanov
 * Fixed comments for String-to-X functions
 *
 * Revision 1.14  2002/05/02 15:25:51  ivanov
 * Added new parameter to String-to-X functions for skipping the check
 * the end of string on zero
 *
 * Revision 1.13  2002/04/11 20:39:19  ivanov
 * CVS log moved to end of the file
 *
 * Revision 1.12  2002/03/01 17:54:38  kans
 * include string.h and ctype.h
 *
 * Revision 1.11  2002/02/22 22:23:20  vakatov
 * Comments changed/added for string comparison functions
 * #elseif --> #elif
 *
 * Revision 1.10  2002/02/22 19:51:57  ivanov
 * Changed NCBI_OS_WIN to HAVE_STRICMP in the NStr::strncasecmp and
 * NStr::strcasecmp functions
 *
 * Revision 1.9  2002/02/22 17:50:22  ivanov
 * Added compatible compare functions strcmp, strncmp, strcasecmp, strncasecmp.
 * Was speed-up some Compare..() functions.
 *
 * Revision 1.8  2001/08/30 17:03:59  thiessen
 * remove class name from NStr member function declaration
 *
 * Revision 1.7  2001/08/30 00:39:29  vakatov
 * + NStr::StringToNumeric()
 * Moved all of "ncbistr.inl" code to here.
 * NStr::BoolToString() to return "const string&" rather than "string".
 * Also, well-groomed the code.
 *
 * Revision 1.6  2001/05/21 21:46:17  vakatov
 * SIZE_TYPE, NPOS -- moved from <ncbistl.hpp> to <ncbistr.hpp> and
 * made non-macros (to avoid possible name clashes)
 *
 * Revision 1.5  2001/05/17 14:54:18  lavr
 * Typos corrected
 *
 * Revision 1.4  2001/04/12 21:44:34  vakatov
 * Added dummy and private NStr::ToUpper/Lower(const char*) to prohibit
 * passing of constant C strings to the "regular" NStr::ToUpper/Lower()
 * variants.
 *
 * Revision 1.3  2001/03/16 19:38:55  grichenk
 * Added NStr::Split()
 *
 * Revision 1.2  2000/12/28 16:57:41  golikov
 * add string version of case an nocase cmp
 *
 * Revision 1.1  2000/12/15 15:36:30  vasilche
 * Added header corelib/ncbistr.hpp for all string utility functions.
 * Optimized string utility functions.
 * Added assignment operator to CRef<> and CConstRef<>.
 * Add Upcase() and Locase() methods for automatic conversion.
 *
 * ===========================================================================
 */

#endif  /* CORELIB___NCBISTR__HPP */
