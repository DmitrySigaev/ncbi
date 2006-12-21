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
 *
 */

/// @file ncbistr.hpp
/// The NCBI C++ standard methods for dealing with std::string


#include <corelib/ncbitype.h>
#include <corelib/ncbiexpt.hpp>
#include <corelib/ncbimisc.hpp>
#include <corelib/ncbidbg.hpp>
#include <corelib/ncbistl.hpp>
#include <string.h>
#ifdef NCBI_OS_OSF1
#  include <strings.h>
#endif
#include <time.h>
#include <stdarg.h>
#include <string>
#include <list>
#include <vector>
#include <algorithm>


BEGIN_NCBI_SCOPE

/** @addtogroup String
 *
 * @{
 */

/// Empty "C" string (points to a '\0').
NCBI_XNCBI_EXPORT extern const char *const kEmptyCStr;
#define NcbiEmptyCStr NCBI_NS_NCBI::kEmptyCStr


/// Empty "C++" string.
#if defined(NCBI_OS_MSWIN) || ( defined(NCBI_OS_LINUX)  &&  defined(NCBI_COMPILER_GCC) )
class CNcbiEmptyString
{
public:
    /// Get string.
    static const string& Get(void)
    {
        static string empty_str;
        return empty_str;
    }
};
#else
class NCBI_XNCBI_EXPORT CNcbiEmptyString
{
public:
    /// Get string.
    static const string& Get(void);
private:
    /// Helper method to initialize private data member and return
    /// null string.
    static const string& FirstGet(void);
    static const string* m_Str;     ///< Null string pointer.
};
#endif // NCBI_OS_MSWIN....


#define NcbiEmptyString NCBI_NS_NCBI::CNcbiEmptyString::Get()
#define kEmptyStr NcbiEmptyString


// SIZE_TYPE and NPOS

/// Define size type.
typedef NCBI_NS_STD::string::size_type SIZE_TYPE;

/// Define NPOS constant as the special value "std::string::npos" which is
/// returned when a substring search fails, or to indicate an unspecified
/// string position.
static const SIZE_TYPE NPOS = NCBI_NS_STD::string::npos;



/////////////////////////////////////////////////////////////////////////////
///
/// CTempString implements a light-weight string on top of a storage buffer
/// whose lifetime management is known and controlled.  CTempString is designed
/// to perform no memory allocation but provide a string interaction interface
/// conrguent with std::basic_string<char>.  As such, CTempString provides a
/// const-only access interface to its underlying storage.  Care has been taken
/// to avoid allocations and other expensive operations wherever possible.
///

class CTempString
{
public:
    /// @name std::basic_string<> compatibility typedefs and enums
    /// @{
    typedef size_t      size_type;
    typedef const char* const_iterator;
    static const size_type	npos = static_cast<size_type>(-1);
    /// @}

    CTempString(void);
    CTempString(const char* str);
    CTempString(const char* str, size_type length);
    CTempString(const char* str, size_type pos, size_type length);

    // initialization from string literal
    template<size_t Size> CTempString(const char (&str)[Size]);

    CTempString(const string& str);
    CTempString(const string& str, size_type length);
    CTempString(const string& str, size_type pos, size_type length);

    CTempString(const CTempString& str);
    CTempString(const CTempString& str, size_type pos);
    CTempString(const CTempString& str, size_type pos, size_type length);

    /// copy a substring into a string
    /// These are analogs of basic_string::assign()
    void Copy(string& dst, size_type pos, size_type length) const;

    /// @name std::basic_string<> compatibility interface
    /// @{

    const_iterator begin() const;
    const_iterator end() const;

    const char* data(void)   const;
    size_type   length(void) const;
    size_type   size(void) const;
    bool        empty(void)  const;

    /// truncate the string at some specified position
    /// NB: basic_string<> supports additional erase() options that we
    /// do not provide here
    void erase(size_type pos = 0);

    /// Find the first instance of the entire matching string within the
    /// current string, beginning at an optional offset.
    size_type find(const CTempString& match,
                   size_type pos = 0) const;

    /// Find the first occurrence of any character in the matching string
    /// within the current string, beginning at an optional offset.
    size_type find_first_of(const CTempString& match,
                            size_type pos = 0) const;

    /// Find the first occurrence of any character not in the matching string
    /// within the current string, beginning at an optional offset.
    size_type find_first_not_of(const CTempString& match,
                                size_type pos = 0) const;

    /// Obtain a substring from this string, beginning at a given offset
    CTempString substr(size_type pos) const;

    /// Obtain a substring from this string, beginning at a given offset
    /// and extending a specified length
    CTempString substr(size_type pos, size_type len) const;

    /// Index into the current string and provide its character in a read-
    /// only fashion.  If the index is beyond the length of the string,
    /// a NULL character is returned.
    char operator[] (size_type pos) const;

    /// operator== for C-style strings
    bool operator==(const char* str) const;
    /// operator== for std::string strings
    bool operator==(const string& str) const;
    /// operator== for CTempString strings
    bool operator==(const CTempString& str) const;

    /// operator< for C-style strings
    bool operator<(const char* str) const;
    /// operator< for std::string strings
    bool operator<(const string& str) const;
    /// operator< for CTempString strings
    bool operator<(const CTempString& str) const;

    /// @}

    operator string(void) const;

private:

    const char* m_String;  ///< Stored pointer to string
    size_type   m_Length;  ///< Length of string

    // Initialize CTempString with bounds checks
    void x_Reset(void);
    void x_Init(const char* str, size_type str_length,
                size_type pos, size_type length);
    bool x_Equals(const_iterator it1, const_iterator it2) const;
    bool x_Less(const_iterator it1, const_iterator it2) const;
};


NCBI_XNCBI_EXPORT
CNcbiOstream& operator<<(CNcbiOstream& out, const CTempString& str);

/////////////////////////////////////////////////////////////////////////////
///
/// NStr --
///
/// Encapuslates class-wide string processing functions.

class NCBI_XNCBI_EXPORT NStr
{
public:
    /// Convert string to numeric value.
    ///
    /// @param str
    ///   String containing digits.
    /// @return
    ///   - Convert "str" to a (non-negative) "int" value and return
    ///     this value.
    ///   - -1 if "str" contains any symbols other than [0-9], or
    ///     if it represents a number that does not fit into "int".
    static int StringToNumeric(const string& str);


    /// Number to string conversion flags.
    ///
    /// NOTE: 
    ///   If specified base in the *ToString() methods is not default 10,
    ///   that some flags like fWithSign and fWithCommas will be ignored.
    enum ENumToStringFlags {
        fWithSign        = (1 <<  9), ///< Prefix the output value with a sign
        fWithCommas      = (1 << 10), ///< Use commas as thousands separator
        fDoubleFixed     = (1 << 11), ///< Use n.nnnn format for double
        fDoubleScientific= (1 << 12), ///< Use scientific format for double
        fDoubleGeneral   = fDoubleFixed | fDoubleScientific
    };
    typedef int TNumToStringFlags;    ///< Bitwise OR of "ENumToStringFlags"

    /// String to number conversion flags.
    enum EStringToNumFlags {
        fConvErr_NoThrow      = (1 <<  9),   ///< Return "natural null"
        // value on error, instead of throwing (by default) an exception
        
        fMandatorySign        = (1 << 10),   ///< See 'fWithSign'
        fAllowCommas          = (1 << 11),   ///< See 'fWithCommas'
        fAllowLeadingSpaces   = (1 << 12),   ///< Can have leading spaces
        fAllowLeadingSymbols  = (1 << 13) | fAllowLeadingSpaces,
                                             ///< Can have leading non-nums
        fAllowTrailingSpaces  = (1 << 14),   ///< Can have trailing spaces
        fAllowTrailingSymbols = (1 << 15) | fAllowTrailingSpaces,
                                             ///< Can have leading non-nums
        fAllStringToNumFlags  = 0x7F00
    };
    typedef int TStringToNumFlags;   ///< Binary OR of "EStringToNumFlags"

    /// Convert string to int.
    ///
    /// @param str
    ///   String to be converted.
    /// @param flags
    ///   How to convert string to value.
    /// @param base
    ///   Radix base. Default is 10. Allowed values are 0, 2..32.
    /// @return
    ///   - Convert "str" to "int" value and return it.
    ///   - 0 if "str" contains illegal symbols, or if it represents a number
    ///     that does not fit into range, and flag fConvErr_NoThrow is set.
    ///   - Throw an exception otherwise.
    static int StringToInt(const CTempString& str,
                           TStringToNumFlags  flags = 0,
                           int                base  = 10);

    /// Convert string to unsigned int.
    ///
    /// @param str
    ///   String to be converted.
    /// @param flags
    ///   How to convert string to value.
    /// @param base
    ///   Radix base. Default is 10. Allowed values are 0, 2..32.
    /// @return
    ///   - Convert "str" to "unsigned int" value and return it.
    ///   - 0 if "str" contains illegal symbols, or if it represents a number
    ///     that does not fit into range, and flag fConvErr_NoThrow is set.
    ///   - Throw an exception otherwise.
    static unsigned int StringToUInt(const CTempString& str,
                                     TStringToNumFlags  flags = 0,
                                     int                base  = 10);

    /// Convert string to long.
    ///
    /// @param str
    ///   String to be converted.
    /// @param flags
    ///   How to convert string to value.
    /// @param base
    ///   Radix base. Default is 10. Allowed values are 0, 2..32.
    /// @return
    ///   - Convert "str" to "long" value and return it.
    ///   - 0 if "str" contains illegal symbols, or if it represents a number
    ///     that does not fit into range, and flag fConvErr_NoThrow is set.
    ///   - Throw an exception otherwise.
    static long StringToLong(const CTempString& str,
                             TStringToNumFlags  flags = 0,
                             int                base  = 10);

    /// Convert string to unsigned long.
    ///
    /// @param str
    ///   String to be converted.
    /// @param flags
    ///   How to convert string to value.
    /// @param base
    ///   Numeric base of the number symbols (default = 10).
    /// @return
    ///   - Convert "str" to "unsigned long" value and return it.
    ///   - 0 if "str" contains illegal symbols, or if it represents a number
    ///     that does not fit into range, and flag fConvErr_NoThrow is set.
    ///   - Throw an exception otherwise.
    static unsigned long StringToULong(const CTempString& str,
                                       TStringToNumFlags  flags = 0,
                                       int                base  = 10);

    /// Convert string to double.
    ///
    /// @param str
    ///   String to be converted.
    /// @param flags
    ///   How to convert string to value.
    ///   Do not support fAllowCommas flag.
    /// @return
    ///   - Convert "str" to "double" value and return it.
    ///   - 0 if "str" contains illegal symbols, or if it represents a number
    ///     that does not fit into range, and flag fConvErr_NoThrow is set.
    ///   - Throw an exception otherwise.
    static double StringToDouble(const CTempString& str,
                                 TStringToNumFlags  flags = 0);

    /// Convert string to Int8.
    ///
    /// @param str
    ///   String to be converted.
    /// @param flags
    ///   How to convert string to value.
    /// @param base
    ///   Radix base. Default is 10. Allowed values are 0, 2..32.
    /// @return
    ///   - Convert "str" to "Int8" value and return it.
    ///   - 0 if "str" contains illegal symbols, or if it represents a number
    ///     that does not fit into range, and flag fConvErr_NoThrow is set.
    ///   - Throw an exception otherwise.
    static Int8 StringToInt8(const CTempString& str,
                             TStringToNumFlags  flags = 0,
                             int                base  = 10);

    /// Convert string to Uint8.
    ///
    /// @param str
    ///   String to be converted.
    /// @param flags
    ///   How to convert string to value.
    /// @param base
    ///   Radix base. Default is 10. Allowed values are 0, 2..32.
    /// @return
    ///   - Convert "str" to "UInt8" value and return it.
    ///   - 0 if "str" contains illegal symbols, or if it represents a number
    ///     that does not fit into range, and flag fConvErr_NoThrow is set.
    ///   - Throw an exception otherwise.
    static Uint8 StringToUInt8(const CTempString& str,
                               TStringToNumFlags  flags = 0,
                               int                base  = 10);

    /// Convert string to number of bytes. 
    ///
    /// String can contain "software" qualifiers: MB(megabyte), KB (kilobyte)..
    /// Example: 100MB, 1024KB
    /// Note the qualifiers are power-of-2 based, aka kibi-, mebi- etc, so that
    /// 1KB = 1024B (not 1000B), 1MB = 1024KB = 1048576B, etc.
    ///
    /// @param str
    ///   String to be converted.
    /// @param flags
    ///   How to convert string to value.
    /// @param base
    ///   Numeric base of the number (before the qualifier).
    ///   Default is 10. Allowed values are 0, 2..20.
    /// @return
    ///   - Convert "str" to "Uint8" value and return it.
    ///   - 0 if "str" contains illegal symbols, or if it represents a number
    ///     that does not fit into range, and flag fConvErr_NoThrow is set.
    ///   - Throw an exception otherwise.
    static Uint8 StringToUInt8_DataSize(const CTempString& str,
                                        TStringToNumFlags  flags = 0,
                                        int                base  = 10);

    /// Convert string to pointer.
    ///
    /// @param str
    ///   String to be converted.
    /// @return
    ///   Pointer value corresponding to its string representation.
    static const void* StringToPtr(const string& str);

    /// Convert character to integer.
    ///
    /// @param ch
    ///   Character to be converted.
    /// @return
    ///   Integer (0..15) corresponding to the "ch" as a hex digit.
    ///   Return -1 on error.
    static int HexChar(char ch);

    /// Convert Int to String.
    ///
    /// @param value
    ///   Integer value (long) to be converted.
    /// @param flags
    ///   How to convert value to string.
    /// @param base
    ///   Radix base. Default is 10. Allowed values are 2..32.
    ///   Bases 8 and 16 do not add leading '0' and '0x' accordingly.
    ///   If necessary you should add it yourself.
    /// @return
    ///   Converted string value.
    static string IntToString(long value, TNumToStringFlags flags = 0,
                              int  base = 10);

    /// Convert Int to String.
    ///
    /// @param out_str
    ///   Output string variable.
    /// @param value
    ///   Integer value (long) to be converted.
    /// @param flags
    ///   How to convert value to string.
    /// @param base
    ///   Radix base. Default is 10. Allowed values are 2..32.
    ///   Bases 8 and 16 do not add leading '0' and '0x' accordingly.
    ///   If necessary you should add it yourself.
    static void IntToString(string& out_str, long value, 
                            TNumToStringFlags flags = 0,
                            int               base  = 10);

    /// Convert UInt to string.
    ///
    /// @param value
    ///   Integer value (unsigned long) to be converted.
    /// @param flags
    ///   How to convert value to string.
    /// @param base
    ///   Radix base. Default is 10. Allowed values are 2..32.
    ///   Bases 8 and 16 do not add leading '0' and '0x' accordingly.
    ///   If necessary you should add it yourself.
    /// @return
    ///   Converted string value.
    static string UIntToString(unsigned long value,
                               TNumToStringFlags flags = 0,
                               int               base  = 10);

    /// Convert UInt to string.
    ///
    /// @param out_str
    ///   Output string variable
    /// @param value
    ///   Integer value (unsigned long) to be converted.
    /// @param flags
    ///   How to convert value to string.
    /// @param base
    ///   Radix base. Default is 10. Allowed values are 2..32.
    ///   Bases 8 and 16 do not add leading '0' and '0x' accordingly.
    ///   If necessary you should add it yourself.
    static void UIntToString(string& out_str, unsigned long value,
                             TNumToStringFlags flags = 0,
                             int               base  = 10);

    /// Convert Int8 to string.
    ///
    /// @param value
    ///   Integer value (Int8) to be converted.
    /// @param flags
    ///   How to convert value to string.
    /// @param base
    ///   Radix base. Default is 10. Allowed values are 2..32.
    ///   Bases 8 and 16 do not add leading '0' and '0x' accordingly.
    ///   If necessary you should add it yourself.
    /// @return
    ///   Converted string value.
    static string Int8ToString(Int8 value,
                               TNumToStringFlags flags = 0,
                               int               base  = 10);

    /// Convert Int8 to string.
    ///
    /// @param out_str
    ///   Output string variable
    /// @param value
    ///   Integer value (Int8) to be converted.
    /// @param flags
    ///   How to convert value to string.
    /// @param base
    ///   Radix base. Default is 10. Allowed values are 2..32.
    ///   Bases 8 and 16 do not add leading '0' and '0x' accordingly.
    ///   If necessary you should add it yourself.
    static void Int8ToString(string& out_str, Int8 value,
                             TNumToStringFlags flags = 0,
                             int               base  = 10);

    /// Convert UInt8 to string.
    ///
    /// @param value
    ///   Integer value (UInt8) to be converted.
    /// @param flags
    ///   How to convert value to string.
    /// @param base
    ///   Radix base. Default is 10. Allowed values are 2..32.
    ///   Bases 8 and 16 do not add leading '0' and '0x' accordingly.
    ///   If necessary you should add it yourself.
    /// @return
    ///   Converted string value.
    static string UInt8ToString(Uint8 value,
                                TNumToStringFlags flags = 0,
                                int               base  = 10);

    /// Convert UInt8 to string.
    ///
    /// @param out_str
    ///   Output string variable
    /// @param value
    ///   Integer value (UInt8) to be converted.
    /// @param flags
    ///   How to convert value to string.
    /// @param base
    ///   Radix base. Default is 10. Allowed values are 2..32.
    ///   Bases 8 and 16 do not add leading '0' and '0x' accordingly.
    ///   If necessary you should add it yourself.
    static void UInt8ToString(string& out_str, Uint8 value,
                              TNumToStringFlags flags = 0,
                              int               base  = 10);

    /// Convert double to string.
    ///
    /// @param value
    ///   Double value to be converted.
    /// @param precision
    ///   Precision value for conversion. If precision is more that maximum
    ///   for current platform, then it will be truncated to this maximum.
    //    If it is negative, that double will be converted to number in
    ///   scientific notation.
    /// @param flags
    ///   How to convert value to string.
    ///   If double format flags are not specified, that next output format
    ///   will be used by default:
    ///     - fDoubleFixed,   if 'precision' >= 0.
    ///     - fDoubleGeneral, if 'precision' < 0.
    /// @return
    ///   Converted string value.
    static string DoubleToString(double value, int precision = -1,
                                 TNumToStringFlags flags = 0);

    /// Convert double to string.
    ///
    /// @param out_str
    ///   Output string variable
    /// @param value
    ///   Double value to be converted.
    /// @param precision
    ///   Precision value for conversion. If precision is more that maximum
    ///   for current platform, then it will be truncated to this maximum.
    //    If it is negative, that double will be converted to number in
    ///   scientific notation.
    /// @param flags
    ///   How to convert value to string.
    ///   If double format flags are not specified, that next output format
    ///   will be used by default:
    ///     - fDoubleFixed,   if 'precision' >= 0.
    ///     - fDoubleGeneral, if 'precision' < 0.
    static void DoubleToString(string& out_str, double value,
                               int precision = -1,
                               TNumToStringFlags flags = 0);

    /// Convert double to string with specified precision and place the result
    /// in the specified buffer.
    ///
    /// @param value
    ///   Double value to be converted.
    /// @param precision
    ///   Precision value for conversion. If precision is more that maximum
    ///   for current platform, then it will be truncated to this maximum.
    /// @param buf
    ///   Put result of the conversion into this buffer.
    /// @param buf_size
    ///   Size of buffer, "buf".
    /// @param flags
    ///   How to convert value to string.
    ///   Default output format is fDoubleFixed.
    /// @return
    ///   The number of bytes stored in "buf", not counting the
    ///   terminating '\0'.
    static SIZE_TYPE DoubleToString(double value, unsigned int precision,
                                    char* buf, SIZE_TYPE buf_size,
                                    TNumToStringFlags flags = 0);

    /// Convert pointer to string.
    ///
    /// @param out_str
    ///   Output string variable
    /// @param str
    ///   Pointer to be converted.
    static void PtrToString(string& out_str, const void* ptr);

    /// Convert pointer to string.
    ///
    /// @param str
    ///   Pointer to be converted.
    /// @return
    ///   String value representing the pointer.
    static string PtrToString(const void* ptr);

    /// Convert bool to string.
    ///
    /// @param value
    ///   Boolean value to be converted.
    /// @return
    ///   One of: 'true, 'false'
    static const string BoolToString(bool value);

    /// Convert string to bool.
    ///
    /// @param str
    ///   Boolean string value to be converted.  Can recognize
    ///   case-insensitive version as one of:  'true, 't', 'yes', 'y'
    ///   for TRUE; and  'false', 'f', 'no', 'n' for FALSE.
    /// @return
    ///   TRUE or FALSE.
    static bool StringToBool(const string& str);


    /// Handle an arbitrary printf-style format string.
    ///
    /// This method exists only to support third-party code that insists on
    /// representing messages in this format; please stick to type-checked
    /// means of formatting such as the above ToString methods and I/O
    /// streams whenever possible.
    static string FormatVarargs(const char* format, va_list args);


    /// Which type of string comparison.
    enum ECase {
        eCase,      ///< Case sensitive compare
        eNocase     ///< Case insensitive compare
    };

    // ATTENTION.  Be aware that:
    //
    // 1) "Compare***(..., SIZE_TYPE pos, SIZE_TYPE n, ...)" functions
    //    follow the ANSI C++ comparison rules a la "basic_string::compare()":
    //       str[pos:pos+n) == pattern   --> return 0
    //       str[pos:pos+n) <  pattern   --> return negative value
    //       str[pos:pos+n) >  pattern   --> return positive value
    //
    // 2) "strn[case]cmp()" functions follow the ANSI C comparison rules:
    //       str[0:n) == pattern[0:n)   --> return 0
    //       str[0:n) <  pattern[0:n)   --> return negative value
    //       str[0:n) >  pattern[0:n)   --> return positive value


    /// Case-sensitive compare of a substring with a pattern.
    ///
    /// @param str
    ///   String containing the substring to be compared.
    /// @param pos
    ///   Start position of substring to be compared.
    /// @param n
    ///   Number of characters in substring to be compared.
    /// @param pattern
    ///   String pattern (char*) to be compared with substring.
    /// @return
    ///   - 0, if str[pos:pos+n) == pattern.   
    ///   - Negative integer, if str[pos:pos+n) <  pattern.   
    ///   - Positive integer, if str[pos:pos+n) >  pattern.   
    /// @sa
    ///   Other forms of overloaded CompareCase() with differences in argument
    ///   types: char* vs. string&
    static int CompareCase(const string& str, SIZE_TYPE pos, SIZE_TYPE n,
                           const char* pattern);

    /// Case-sensitive compare of a substring with a pattern.
    ///
    /// @param str
    ///   String containing the substring to be compared.
    /// @param pos
    ///   Start position of substring to be compared.
    /// @param n
    ///   Number of characters in substring to be compared.
    /// @param pattern
    ///   String pattern (string&) to be compared with substring.
    /// @return
    ///   - 0, if str[pos:pos+n) == pattern.   
    ///   - Negative integer, if str[pos:pos+n) <  pattern.   
    ///   - Positive integer, if str[pos:pos+n) >  pattern.   
    /// @sa
    ///   Other forms of overloaded CompareCase() with differences in argument
    ///   types: char* vs. string&
    static int CompareCase(const string& str, SIZE_TYPE pos, SIZE_TYPE n,
                           const string& pattern);

    /// Case-sensitive compare of two strings -- char* version.
    ///
    /// @param s1
    ///   String to be compared -- operand 1.
    /// @param s2
    ///   String to be compared -- operand 2.
    /// @return
    ///   - 0, if s1 == s2.   
    ///   - Negative integer, if s1 < s2.   
    ///   - Positive integer, if s1 > s2.   
    /// @sa
    ///   CompareNocase(), Compare() versions with same argument types.
    static int CompareCase(const char* s1, const char* s2);

    /// Case-sensitive compare of two strings -- string& version.
    ///
    /// @param s1
    ///   String to be compared -- operand 1.
    /// @param s2
    ///   String to be compared -- operand 2.
    /// @return
    ///   - 0, if s1 == s2.   
    ///   - Negative integer, if s1 < s2.   
    ///   - Positive integer, if s1 > s2.   
    /// @sa
    ///   CompareNocase(), Compare() versions with same argument types.
    static int CompareCase(const string& s1, const string& s2);

    /// Case-insensitive compare of a substring with a pattern.
    ///
    /// @param str
    ///   String containing the substring to be compared.
    /// @param pos
    ///   Start position of substring to be compared.
    /// @param n
    ///   Number of characters in substring to be compared.
    /// @param pattern
    ///   String pattern (char*) to be compared with substring.
    /// @return
    ///   - 0, if str[pos:pos+n) == pattern (case-insensitive compare).   
    ///   - Negative integer, if str[pos:pos+n) <  pattern (case-insensitive
    ///     compare).
    ///   - Positive integer, if str[pos:pos+n) >  pattern (case-insensitive
    ///     compare).
    /// @sa
    ///   Other forms of overloaded CompareNocase() with differences in
    ///   argument types: char* vs. string&
    static int CompareNocase(const string& str, SIZE_TYPE pos, SIZE_TYPE n,
                             const char* pattern);

    /// Case-insensitive compare of a substring with a pattern.
    ///
    /// @param str
    ///   String containing the substring to be compared.
    /// @param pos
    ///   Start position of substring to be compared.
    /// @param n
    ///   Number of characters in substring to be compared.
    /// @param pattern
    ///   String pattern (string&) to be compared with substring.
    /// @return
    ///   - 0, if str[pos:pos+n) == pattern (case-insensitive compare).   
    ///   - Negative integer, if str[pos:pos+n) <  pattern (case-insensitive
    ///     compare).
    ///   - Positive integer, if str[pos:pos+n) >  pattern (case-insensitive
    ///     compare).
    /// @sa
    ///   Other forms of overloaded CompareNocase() with differences in
    ///   argument types: char* vs. string&
    static int CompareNocase(const string& str, SIZE_TYPE pos, SIZE_TYPE n,
                             const string& pattern);

    /// Case-insensitive compare of two strings -- char* version.
    ///
    /// @param s1
    ///   String to be compared -- operand 1.
    /// @param s2
    ///   String to be compared -- operand 2.
    /// @return
    ///   - 0, if s1 == s2 (case-insensitive compare).      
    ///   - Negative integer, if s1 < s2 (case-insensitive compare).      
    ///   - Positive integer, if s1 > s2 (case-insensitive compare).    
    /// @sa
    ///   CompareCase(), Compare() versions with same argument types.
    static int CompareNocase(const char* s1, const char* s2);

    /// Case-insensitive compare of two strings -- string& version.
    ///
    /// @param s1
    ///   String to be compared -- operand 1.
    /// @param s2
    ///   String to be compared -- operand 2.
    /// @return
    ///   - 0, if s1 == s2 (case-insensitive compare).      
    ///   - Negative integer, if s1 < s2 (case-insensitive compare).      
    ///   - Positive integer, if s1 > s2 (case-insensitive compare).    
    /// @sa
    ///   CompareCase(), Compare() versions with same argument types.
    static int CompareNocase(const string& s1, const string& s2);

    /// Compare of a substring with a pattern.
    ///
    /// @param str
    ///   String containing the substring to be compared.
    /// @param pos
    ///   Start position of substring to be compared.
    /// @param n
    ///   Number of characters in substring to be compared.
    /// @param pattern
    ///   String pattern (char*) to be compared with substring.
    /// @param use_case
    ///   Whether to do a case sensitive compare(eCase -- default), or a
    ///   case-insensitive compare (eNocase).
    /// @return
    ///   - 0, if str[pos:pos+n) == pattern.   
    ///   - Negative integer, if str[pos:pos+n) <  pattern.   
    ///   - Positive integer, if str[pos:pos+n) >  pattern.   
    /// @sa
    ///   Other forms of overloaded Compare() with differences in argument
    ///   types: char* vs. string&
    static int Compare(const string& str, SIZE_TYPE pos, SIZE_TYPE n,
                       const char* pattern, ECase use_case = eCase);

    /// Compare of a substring with a pattern.
    ///
    /// @param str
    ///   String containing the substring to be compared.
    /// @param pos
    ///   Start position of substring to be compared.
    /// @param n
    ///   Number of characters in substring to be compared.
    /// @param pattern
    ///   String pattern (string&) to be compared with substring.
    /// @param use_case
    ///   Whether to do a case sensitive compare(default is eCase), or a
    ///   case-insensitive compare (eNocase).
    /// @return
    ///   - 0, if str[pos:pos+n) == pattern.   
    ///   - Negative integer, if str[pos:pos+n) <  pattern.   
    ///   - Positive integer, if str[pos:pos+n) >  pattern.   
    /// @sa
    ///   Other forms of overloaded Compare() with differences in argument
    ///   types: char* vs. string&
    static int Compare(const string& str, SIZE_TYPE pos, SIZE_TYPE n,
                       const string& pattern, ECase use_case = eCase);

    /// Compare two strings -- char* version.
    ///
    /// @param s1
    ///   String to be compared -- operand 1.
    /// @param s2
    ///   String to be compared -- operand 2.
    /// @param use_case
    ///   Whether to do a case sensitive compare(default is eCase), or a
    ///   case-insensitive compare (eNocase).
    /// @return
    ///   - 0, if s1 == s2.   
    ///   - Negative integer, if s1 < s2.   
    ///   - Positive integer, if s1 > s2.   
    /// @sa
    ///   CompareNocase(), Compare() versions with similar argument types.
    static int Compare(const char* s1, const char* s2,
                       ECase use_case = eCase);

    /// Compare two strings -- string&, char* version.
    ///
    /// @param s1
    ///   String to be compared -- operand 1.
    /// @param s2
    ///   String to be compared -- operand 2.
    /// @param use_case
    ///   Whether to do a case sensitive compare(default is eCase), or a
    ///   case-insensitive compare (eNocase).
    /// @return
    ///   - 0, if s1 == s2.   
    ///   - Negative integer, if s1 < s2.   
    ///   - Positive integer, if s1 > s2.   
    /// @sa
    ///   CompareNocase(), Compare() versions with similar argument types.
    static int Compare(const string& s1, const char* s2,
                       ECase use_case = eCase);

    /// Compare two strings -- char*, string& version.
    ///
    /// @param s1
    ///   String to be compared -- operand 1.
    /// @param s2
    ///   String to be compared -- operand 2.
    /// @param use_case
    ///   Whether to do a case sensitive compare(default is eCase), or a
    ///   case-insensitive compare (eNocase).
    /// @return
    ///   - 0, if s1 == s2.   
    ///   - Negative integer, if s1 < s2.   
    ///   - Positive integer, if s1 > s2.   
    /// @sa
    ///   CompareNocase(), Compare() versions with similar argument types.
    static int Compare(const char* s1, const string& s2,
                       ECase use_case = eCase);

    /// Compare two strings -- string& version.
    ///
    /// @param s1
    ///   String to be compared -- operand 1.
    /// @param s2
    ///   String to be compared -- operand 2.
    /// @param use_case
    ///   Whether to do a case sensitive compare(default is eCase), or a
    ///   case-insensitive compare (eNocase).
    /// @return
    ///   - 0, if s1 == s2.   
    ///   - Negative integer, if s1 < s2.   
    ///   - Positive integer, if s1 > s2.   
    /// @sa
    ///   CompareNocase(), Compare() versions with similar argument types.
    static int Compare(const string& s1, const string& s2,
                       ECase use_case = eCase);

    /// Case-sensitive equality of a substring with a pattern.
    ///
    /// @param str
    ///   String containing the substring to be compared.
    /// @param pos
    ///   Start position of substring to be compared.
    /// @param n
    ///   Number of characters in substring to be compared.
    /// @param pattern
    ///   String pattern (char*) to be compared with substring.
    /// @return
    ///   - true, if str[pos:pos+n) equals pattern.   
    ///   - false, otherwise
    /// @sa
    ///   Other forms of overloaded EqualCase() with differences in argument
    ///   types: char* vs. string&
    static bool EqualCase(const string& str, SIZE_TYPE pos, SIZE_TYPE n,
                           const char* pattern);

    /// Case-sensitive equality of a substring with a pattern.
    ///
    /// @param str
    ///   String containing the substring to be compared.
    /// @param pos
    ///   Start position of substring to be compared.
    /// @param n
    ///   Number of characters in substring to be compared.
    /// @param pattern
    ///   String pattern (string&) to be compared with substring.
    /// @return
    ///   - true, if str[pos:pos+n) equals pattern.   
    ///   - false, otherwise
    /// @sa
    ///   Other forms of overloaded EqualCase() with differences in argument
    ///   types: char* vs. string&
    static bool EqualCase(const string& str, SIZE_TYPE pos, SIZE_TYPE n,
                           const string& pattern);

    /// Case-sensitive equality of two strings -- char* version.
    ///
    /// @param s1
    ///   String to be compared -- operand 1.
    /// @param s2
    ///   String to be compared -- operand 2.
    /// @return
    ///   - true, if s1 equals s2
    ///   - false, otherwise
    /// @sa
    ///   EqualCase(), Equal() versions with same argument types.
    static bool EqualCase(const char* s1, const char* s2);

    /// Case-sensitive equality of two strings -- string& version.
    ///
    /// @param s1
    ///   String to be compared -- operand 1.
    /// @param s2
    ///   String to be compared -- operand 2.
    /// @return
    ///   - true, if s1 equals s2
    ///   - false, otherwise
    /// @sa
    ///   EqualCase(), Equal() versions with same argument types.
    static bool EqualCase(const string& s1, const string& s2);

    /// Case-insensitive equality of a substring with a pattern.
    ///
    /// @param str
    ///   String containing the substring to be compared.
    /// @param pos
    ///   Start position of substring to be compared.
    /// @param n
    ///   Number of characters in substring to be compared.
    /// @param pattern
    ///   String pattern (char*) to be compared with substring.
    /// @return
    ///   - true, if str[pos:pos+n) equals pattern (case-insensitive compare).
    ///   - false, otherwise.
    /// @sa
    ///   Other forms of overloaded EqualNocase() with differences in
    ///   argument types: char* vs. string&
    static bool EqualNocase(const string& str, SIZE_TYPE pos, SIZE_TYPE n,
                             const char* pattern);

    /// Case-insensitive equality of a substring with a pattern.
    ///
    /// @param str
    ///   String containing the substring to be compared.
    /// @param pos
    ///   Start position of substring to be compared.
    /// @param n
    ///   Number of characters in substring to be compared.
    /// @param pattern
    ///   String pattern (string&) to be compared with substring.
    /// @return
    ///   - true, if str[pos:pos+n) equals pattern (case-insensitive compare).
    ///   - false, otherwise.
    /// @sa
    ///   Other forms of overloaded EqualNocase() with differences in
    ///   argument types: char* vs. string&
    static bool EqualNocase(const string& str, SIZE_TYPE pos, SIZE_TYPE n,
                             const string& pattern);

    /// Case-insensitive equality of two strings -- char* version.
    ///
    /// @param s1
    ///   String to be compared -- operand 1.
    /// @param s2
    ///   String to be compared -- operand 2.
    /// @return
    ///   - true, if s1 equals s2 (case-insensitive compare).      
    ///   - false, otherwise.
    /// @sa
    ///   EqualCase(), Equal() versions with same argument types.
    static bool EqualNocase(const char* s1, const char* s2);

    /// Case-insensitive equality of two strings -- string& version.
    ///
    /// @param s1
    ///   String to be compared -- operand 1.
    /// @param s2
    ///   String to be compared -- operand 2.
    /// @return
    ///   - true, if s1 equals s2 (case-insensitive compare).      
    ///   - false, otherwise.
    /// @sa
    ///   EqualCase(), Equal() versions with same argument types.
    static bool EqualNocase(const string& s1, const string& s2);

    /// Test for equality of a substring with a pattern.
    ///
    /// @param str
    ///   String containing the substring to be compared.
    /// @param pos
    ///   Start position of substring to be compared.
    /// @param n
    ///   Number of characters in substring to be compared.
    /// @param pattern
    ///   String pattern (char*) to be compared with substring.
    /// @param use_case
    ///   Whether to do a case sensitive compare(eCase -- default), or a
    ///   case-insensitive compare (eNocase).
    /// @return
    ///   - true, if str[pos:pos+n) equals pattern.   
    ///   - false, otherwise.
    /// @sa
    ///   Other forms of overloaded Equal() with differences in argument
    ///   types: char* vs. string&
    static bool Equal(const string& str, SIZE_TYPE pos, SIZE_TYPE n,
                       const char* pattern, ECase use_case = eCase);

    /// Test for equality of a substring with a pattern.
    ///
    /// @param str
    ///   String containing the substring to be compared.
    /// @param pos
    ///   Start position of substring to be compared.
    /// @param n
    ///   Number of characters in substring to be compared.
    /// @param pattern
    ///   String pattern (string&) to be compared with substring.
    /// @param use_case
    ///   Whether to do a case sensitive compare(default is eCase), or a
    ///   case-insensitive compare (eNocase).
    /// @return
    ///   - 0, if str[pos:pos+n) == pattern.   
    ///   - Negative integer, if str[pos:pos+n) <  pattern.   
    ///   - Positive integer, if str[pos:pos+n) >  pattern.   
    /// @sa
    ///   Other forms of overloaded Equal() with differences in argument
    ///   types: char* vs. string&
    static bool Equal(const string& str, SIZE_TYPE pos, SIZE_TYPE n,
                       const string& pattern, ECase use_case = eCase);

    /// Test for equality of two strings -- char* version.
    ///
    /// @param s1
    ///   String to be compared -- operand 1.
    /// @param s2
    ///   String to be compared -- operand 2.
    /// @param use_case
    ///   Whether to do a case sensitive compare(default is eCase), or a
    ///   case-insensitive compare (eNocase).
    /// @return
    ///   - 0, if s1 == s2.   
    ///   - Negative integer, if s1 < s2.   
    ///   - Positive integer, if s1 > s2.   
    /// @sa
    ///   EqualNocase(), Equal() versions with similar argument types.
    static bool Equal(const char* s1, const char* s2,
                       ECase use_case = eCase);

    /// Test for equality of two strings -- string&, char* version.
    ///
    /// @param s1
    ///   String to be compared -- operand 1.
    /// @param s2
    ///   String to be compared -- operand 2.
    /// @param use_case
    ///   Whether to do a case sensitive compare(default is eCase), or a
    ///   case-insensitive compare (eNocase).
    /// @return
    ///   - true, if s1 equals s2.   
    ///   - false, otherwise.
    /// @sa
    ///   EqualNocase(), Equal() versions with similar argument types.
    static bool Equal(const string& s1, const char* s2,
                       ECase use_case = eCase);

    /// Test for equality of two strings -- char*, string& version.
    ///
    /// @param s1
    ///   String to be compared -- operand 1.
    /// @param s2
    ///   String to be compared -- operand 2.
    /// @param use_case
    ///   Whether to do a case sensitive compare(default is eCase), or a
    ///   case-insensitive compare (eNocase).
    /// @return
    ///   - true, if s1 equals s2.   
    ///   - false, otherwise.
    /// @sa
    ///   EqualNocase(), Equal() versions with similar argument types.
    static bool Equal(const char* s1, const string& s2,
                       ECase use_case = eCase);

    /// Test for equality of two strings -- string& version.
    ///
    /// @param s1
    ///   String to be compared -- operand 1.
    /// @param s2
    ///   String to be compared -- operand 2.
    /// @param use_case
    ///   Whether to do a case sensitive compare(default is eCase), or a
    ///   case-insensitive compare (eNocase).
    /// @return
    ///   - true, if s1 equals s2.   
    ///   - false, otherwise.
    /// @sa
    ///   EqualNocase(), Equal() versions with similar argument types.
    static bool Equal(const string& s1, const string& s2,
                       ECase use_case = eCase);

    // NOTE.  On some platforms, "strn[case]cmp()" can work faster than their
    //        "Compare***()" counterparts.

    /// String compare.
    ///
    /// @param s1
    ///   String to be compared -- operand 1.
    /// @param s2
    ///   String to be compared -- operand 2.
    /// @return
    ///   - 0, if s1 == s2.   
    ///   - Negative integer, if s1 < s2.   
    ///   - Positive integer, if s1 > s2.   
    /// @sa
    ///   strncmp(), strcasecmp(), strncasecmp()
    static int strcmp(const char* s1, const char* s2);

    /// String compare upto specified number of characters.
    ///
    /// @param s1
    ///   String to be compared -- operand 1.
    /// @param s2
    ///   String to be compared -- operand 2.
    /// @param n
    ///   Number of characters in string 
    /// @return
    ///   - 0, if s1 == s2.   
    ///   - Negative integer, if s1 < s2.   
    ///   - Positive integer, if s1 > s2.   
    /// @sa
    ///   strcmp(), strcasecmp(), strncasecmp()
    static int strncmp(const char* s1, const char* s2, size_t n);

    /// Case-insensitive string compare.
    ///
    /// @param s1
    ///   String to be compared -- operand 1.
    /// @param s2
    ///   String to be compared -- operand 2.
    /// @return
    ///   - 0, if s1 == s2.   
    ///   - Negative integer, if s1 < s2.   
    ///   - Positive integer, if s1 > s2.   
    /// @sa
    ///   strcmp(), strncmp(), strncasecmp()
    static int strcasecmp(const char* s1, const char* s2);

    /// Case-insensitive string compare upto specfied number of characters.
    ///
    /// @param s1
    ///   String to be compared -- operand 1.
    /// @param s2
    ///   String to be compared -- operand 2.
    /// @return
    ///   - 0, if s1 == s2.   
    ///   - Negative integer, if s1 < s2.   
    ///   - Positive integer, if s1 > s2.   
    /// @sa
    ///   strcmp(), strcasecmp(), strcasecmp()
    static int strncasecmp(const char* s1, const char* s2, size_t n);

    /// Wrapper for the function strftime() that corrects handling %D and %T
    /// time formats on MS Windows.
    static size_t strftime (char* s, size_t maxsize, const char* format,
                            const struct tm* timeptr);

    /// Match "str" against the "mask".
    ///
    /// This function do not use regular expressions.
    /// @param str
    ///   String to match.
    /// @param mask
    ///   Mask used to match string "str". And can contains next
    ///   wildcard characters:
    ///     ? - matches to any one symbol in the string.
    ///     * - matches to any number of symbols in the string. 
    /// @param use_case
    ///   Whether to do a case sensitive compare(eCase -- default), or a
    ///   case-insensitive compare (eNocase).
    /// @return
    ///   Return TRUE if "str" matches "mask", and FALSE otherwise.
    /// @sa
    ///    CRegexp, CRegexpUtil
    static bool MatchesMask(const char *str, const char *mask,
                            ECase use_case = eCase);

    /// Match "str" against the "mask".
    ///
    /// This function do not use regular expressions.
    /// @param str
    ///   String to match.
    /// @param mask
    ///   Mask used to match string "str". And can contains next
    ///   wildcard characters:
    ///     ? - matches to any one symbol in the string.
    ///     * - matches to any number of symbols in the string. 
    /// @param use_case
    ///   Whether to do a case sensitive compare(eCase -- default), or a
    ///   case-insensitive compare (eNocase).
    /// @return
    ///   Return TRUE if "str" matches "mask", and FALSE otherwise.
    /// @sa
    ///    CRegexp, CRegexpUtil
    static bool MatchesMask(const string& str, const string& mask,
                            ECase use_case = eCase);

    // The following 4 methods change the passed string, then return it

    /// Convert string to lower case -- string& version.
    /// 
    /// @param str
    ///   String to be converted.
    /// @return
    ///   Lower cased string.
    static string& ToLower(string& str);

    /// Convert string to lower case -- char* version.
    /// 
    /// @param str
    ///   String to be converted.
    /// @return
    ///   Lower cased string.
    static char* ToLower(char*   str);

    /// Convert string to upper case -- string& version.
    /// 
    /// @param str
    ///   String to be converted.
    /// @return
    ///   Upper cased string.
    static string& ToUpper(string& str);

    /// Convert string to upper case -- char* version.
    /// 
    /// @param str
    ///   String to be converted.
    /// @return
    ///   Upper cased string.
    static char* ToUpper(char*   str);

private:
    /// Privatized ToLower() with const char* parameter to prevent passing of 
    /// constant strings.
    static void/*dummy*/ ToLower(const char* /*dummy*/);

    /// Privatized ToUpper() with const char* parameter to prevent passing of 
    /// constant strings.
    static void/*dummy*/ ToUpper(const char* /*dummy*/);

public:
    /// Check if a string starts with a specified prefix value.
    ///
    /// @param str
    ///   String to check.
    /// @param start
    ///   Prefix value to check for.
    /// @param use_case
    ///   Whether to do a case sensitive compare(default is eCase), or a
    ///   case-insensitive compare (eNocase) while checking.
    static bool StartsWith(const string& str, const string& start,
                           ECase use_case = eCase);

    /// Check if a string starts with a specified character value.
    ///
    /// @param str
    ///   String to check.
    /// @param start
    ///   Character value to check for.
    /// @param use_case
    ///   Whether to do a case sensitive compare(default is eCase), or a
    ///   case-insensitive compare (eNocase) while checking.
    static bool StartsWith(const string& str, char start,
                           ECase use_case = eCase);

    /// Check if a string ends with a specified suffix value.
    ///
    /// @param str
    ///   String to check.
    /// @param end
    ///   Suffix value to check for.
    /// @param use_case
    ///   Whether to do a case sensitive compare(default is eCase), or a
    ///   case-insensitive compare (eNocase) while checking.
    static bool EndsWith(const string& str, const string& end,
                         ECase use_case = eCase);

    /// Check if a string ends with a specified character value.
    ///
    /// @param str
    ///   String to check.
    /// @param end
    ///   Character value to check for.
    /// @param use_case
    ///   Whether to do a case sensitive compare(default is eCase), or a
    ///   case-insensitive compare (eNocase) while checking.
    static bool EndsWith(const string& str, char end,
                         ECase use_case = eCase);

    /// Check if a string is blank (has no text).
    ///
    /// @param str
    ///   String to check.
    /// @param pos
    ///   starting position (default 0)
    static bool IsBlank(const string& str, SIZE_TYPE pos = 0);

    /// Whether it is the first or last occurrence.
    enum EOccurrence {
        eFirst,             ///< First occurrence
        eLast               ///< Last occurrence
    };

    /// Find the pattern in the specfied range of a string.
    ///
    /// @param str
    ///   String to search.
    /// @param pattern
    ///   Pattern to search for in "str". 
    /// @param start
    ///   Position in "str" to start search from -- default of 0 means start
    ///   the search from the beginning of the string.
    /// @param end
    ///   Position in "str" to start search up to -- default of NPOS means
    ///   to search to the end of the string.
    /// @param which
    ///   When set to eFirst, this means to find the first occurrence of 
    ///   "pattern" in "str". When set to eLast, this means to find the last
    ///    occurrence of "pattern" in "str".
    /// @param use_case
    ///   Whether to do a case sensitive compare(default is eCase), or a
    ///   case-insensitive compare (eNocase) while searching for the pattern.
    /// @return
    ///   - The start of the first or last (depending on "which" parameter)
    ///   occurrence of "pattern" in "str", within the string interval
    ///   ["start", "end"], or
    ///   - NPOS if there is no occurrence of the pattern.
    static SIZE_TYPE Find(const string& str, const string& pattern,
                          SIZE_TYPE start = 0, SIZE_TYPE end = NPOS,
                          EOccurrence which = eFirst,
                          ECase use_case = eCase);

    /// Find the pattern in the specfied range of a string using a case
    /// sensitive search.
    ///
    /// @param str
    ///   String to search.
    /// @param pattern
    ///   Pattern to search for in "str". 
    /// @param start
    ///   Position in "str" to start search from -- default of 0 means start
    ///   the search from the beginning of the string.
    /// @param end
    ///   Position in "str" to start search up to -- default of NPOS means
    ///   to search to the end of the string.
    /// @param which
    ///   When set to eFirst, this means to find the first occurrence of 
    ///   "pattern" in "str". When set to eLast, this means to find the last
    ///    occurrence of "pattern" in "str".
    /// @return
    ///   - The start of the first or last (depending on "which" parameter)
    ///   occurrence of "pattern" in "str", within the string interval
    ///   ["start", "end"], or
    ///   - NPOS if there is no occurrence of the pattern.
    static SIZE_TYPE FindCase  (const string& str, const string& pattern,
                                SIZE_TYPE start = 0, SIZE_TYPE end = NPOS,
                                EOccurrence which = eFirst);

    /// Find the pattern in the specfied range of a string using a case
    /// insensitive search.
    ///
    /// @param str
    ///   String to search.
    /// @param pattern
    ///   Pattern to search for in "str". 
    /// @param start
    ///   Position in "str" to start search from -- default of 0 means start
    ///   the search from the beginning of the string.
    /// @param end
    ///   Position in "str" to start search up to -- default of NPOS means
    ///   to search to the end of the string.
    /// @param which
    ///   When set to eFirst, this means to find the first occurrence of 
    ///   "pattern" in "str". When set to eLast, this means to find the last
    ///    occurrence of "pattern" in "str".
    /// @return
    ///   - The start of the first or last (depending on "which" parameter)
    ///   occurrence of "pattern" in "str", within the string interval
    ///   ["start", "end"], or
    ///   - NPOS if there is no occurrence of the pattern.
    static SIZE_TYPE FindNoCase(const string& str, const string& pattern,
                                SIZE_TYPE start = 0, SIZE_TYPE end = NPOS,
                                EOccurrence which = eFirst);

    /// Which end to truncate a string.
    enum ETrunc {
        eTrunc_Begin,  ///< Truncate leading spaces only
        eTrunc_End,    ///< Truncate trailing spaces only
        eTrunc_Both    ///< Truncate spaces at both begin and end of string
    };

    /// Truncate spaces in a string.
    ///
    /// @param str
    ///   String to truncate spaces from.
    /// @param where
    ///   Which end of the string to truncate space from. Default is to
    ///   truncate space from both ends (eTrunc_Both).
    static string TruncateSpaces(const string& str, ETrunc where=eTrunc_Both);

    /// Truncate spaces in a string (in-place)
    ///
    /// @param str
    ///   String to truncate spaces from.
    /// @param where
    ///   Which end of the string to truncate space from. Default is to
    ///   truncate space from both ends (eTrunc_Both).
    static void TruncateSpacesInPlace(string& str, ETrunc where=eTrunc_Both);
    
    /// Replace occurrences of a substring within a string.
    ///
    /// @param src
    ///   Source string from which specified substring occurrences are
    ///   replaced.
    /// @param search
    ///   Substring value in "src" that is replaced.
    /// @param replace
    ///   Replace "search" substring with this value.
    /// @param dst
    ///   Result of replacing the "search" string with "replace" in "src".
    ///   This value is also returned by the function.
    /// @param start_pos
    ///   Position to start search from.
    /// @param max_replace
    ///   Replace no more than "max_replace" occurrences of substring "search"
    ///   If "max_replace" is zero(default), then replace all occurrences with
    ///   "replace".
    /// @return
    ///   Result of replacing the "search" string with "replace" in "src". This
    ///   value is placed in "dst" as well.
    /// @sa
    ///   Version of Replace() that returns a new string.
    static string& Replace(const string& src,
                           const string& search,
                           const string& replace,
                           string& dst,
                           SIZE_TYPE start_pos = 0, size_t max_replace = 0);

    /// Replace occurrences of a substring within a string and returns the
    /// result as a new string.
    ///
    /// @param src
    ///   Source string from which specified substring occurrences are
    ///   replaced.
    /// @param search
    ///   Substring value in "src" that is replaced.
    /// @param replace
    ///   Replace "search" substring with this value.
    /// @param start_pos
    ///   Position to start search from.
    /// @param max_replace
    ///   Replace no more than "max_replace" occurrences of substring "search"
    ///   If "max_replace" is zero(default), then replace all occurrences with
    ///   "replace".
    /// @return
    ///   A new string containing the result of replacing the "search" string
    ///   with "replace" in "src"
    /// @sa
    ///   Version of Replace() that has a destination parameter to accept
    ///   result.
    static string Replace(const string& src,
                          const string& search,
                          const string& replace,
                          SIZE_TYPE start_pos = 0, size_t max_replace = 0);

    /// Replace occurrences of a substring within a string.
    ///
    /// On some platforms this function is much faster than Replace()
    /// if sizes of "search" and "replace" strings are equal.
    /// Otherwise, the performance is mainly the same.
    /// @param src
    ///   String whre specified substring occurrences are replaced.
    ///   This value is also returned by the function.
    /// @param search
    ///   Substring value in "src" that is replaced.
    /// @param replace
    ///   Replace "search" substring with this value.
    /// @param start_pos
    ///   Position to start search from.
    /// @param max_replace
    ///   Replace no more than "max_replace" occurrences of substring "search"
    ///   If "max_replace" is zero(default), then replace all occurrences with
    ///   "replace".
    /// @return
    ///   Result of replacing the "search" string with "replace" in "src".
    /// @sa
    ///   Replace
    static string& ReplaceInPlace(string& src,
                                  const string& search,
                                  const string& replace,
                                  SIZE_TYPE     start_pos = 0,
                                  size_t        max_replace = 0);

    /// Whether to merge adjacent delimiters in Split and Tokenize.
    enum EMergeDelims {
        eNoMergeDelims,     ///< No merging of delimiters -- default for
                            ///< Tokenize()
        eMergeDelims        ///< Merge the delimiters -- default for Split()
    };


    /// Split a string using specified delimiters.
    ///
    /// @param str
    ///   String to be split.
    /// @param delim
    ///   Delimiters used to split string "str".
    /// @param arr
    ///   The split tokens are added to the list "arr" and also returned
    ///   by the function. 
    /// @param merge
    ///   Whether to merge the delimiters or not. The default setting of
    ///   eMergeDelims means that delimiters that immediately follow each other
    ///   are treated as one delimiter.
    /// @param token_pos
    ///   Optional array for the tokens' positions in "str".
    /// @return 
    ///   The list "arr" is also returned.
    /// @sa
    ///   Tokenize()
    static list<string>& Split(const string& str,
                               const string& delim,
                               list<string>& arr,
                               EMergeDelims  merge = eMergeDelims,
                               vector<SIZE_TYPE>* token_pos = NULL);

    /// Tokenize a string using the specified set of char delimiters.
    ///
    /// @param str
    ///   String to be tokenized.
    /// @param delim
    ///   Set of char delimiters used to tokenize string "str".
    ///   If delimiter is empty, then input string is appended to "arr" as is.
    /// @param arr
    ///   The tokens defined in "str" by using symbols from "delim" are added
    ///   to the list "arr" and also returned by the function. 
    /// @param merge
    ///   Whether to merge the delimiters or not. The default setting of
    ///   eNoMergeDelims means that delimiters that immediately follow each
    ///    other are treated as separate delimiters.
    /// @param token_pos
    ///   Optional array for the tokens' positions in "str".
    /// @return 
    ///   The list "arr" is also returned.
    /// @sa
    ///   Split, TokenizePattern, TokenizeInTwo
    static vector<string>& Tokenize(const string&   str,
                                    const string&   delim,
                                    vector<string>& arr,
                                    EMergeDelims    merge = eNoMergeDelims,
                                    vector<SIZE_TYPE>* token_pos = NULL);

    /// Tokenize a string using the specified delimiter (string).
    ///
    /// @param str
    ///   String to be tokenized.
    /// @param delim
    ///   Delimiter used to tokenize string "str".
    ///   If delimiter is empty, then input string is appended to "arr" as is.
    /// @param arr
    ///   The tokens defined in "str" by using delimeter "delim" are added
    ///   to the list "arr" and also returned by the function. 
    /// @param merge
    ///   Whether to merge the delimiters or not. The default setting of
    ///   eNoMergeDelims means that delimiters that immediately follow each
    ///   other are treated as separate delimiters.
    /// @param token_pos
    ///   Optional array for the tokens' positions in "str".
    /// @return 
    ///   The list "arr" is also returned.
    /// @sa
    ///   Split, Tokenize
    static
    vector<string>& TokenizePattern(const string&   str,
                                    const string&   delim,
                                    vector<string>& arr,
                                    EMergeDelims    merge = eNoMergeDelims,
                                    vector<SIZE_TYPE>* token_pos = NULL);

    /// Split a string into two pieces using the specified delimiters
    ///
    /// @param str 
    ///   String to be split.
    /// @param delim
    ///   Delimiters used to split string "str".
    /// @param str1
    ///   The sub-string of "str" before the first character of "delim".
    ///   It will not contain any characters in "delim".
    ///   Will be empty if "str" begin with a "delim" character.
    /// @param str2
    ///   The sub-string of "str" after the first character of "delim" found.
    ///   May contain "delim" characters.
    ///   Will be empty if "str" had no "delim" characters or ended
    ///   with the first "delim" charcter.
    /// @return
    ///   true if a symbol from "delim" was found in "str", false if not.
    ///   This lets you distinguish when there were no delimiters and when
    ///   the very last character was the first delimiter.
    /// @sa
    ///   Split, Tokenoze, TokenizePattern
    static bool SplitInTwo(const string& str, 
                           const string& delim,
                           string& str1,
                           string& str2);
                         

    /// Join strings using the specified delimiter.
    ///
    /// @param arr
    ///   Array of strings to be joined.
    /// @param delim
    ///   Delimiter used to join the string.
    /// @return 
    ///   The strings in "arr" are joined into a single string, separated
    ///   with "delim".
    static string Join(const list<string>& arr,   const string& delim);
    static string Join(const vector<string>& arr, const string& delim);

    /// How to display new line characters.
    ///
    /// Assists in making a printable version of "str".
    enum ENewLineMode {
        eNewLine_Quote,         ///< Display "\n" instead of actual linebreak
        eNewLine_Passthru       ///< Break the line on every "\n" occurrance
    };

    /// Get a printable version of the specified string. 
    ///
    /// The non-printable characters will be represented as "\r", "\n", "\v",
    /// "\t", "\"", "\\", or "\xDD" where DD is the character's code in
    /// hexadecimal.
    ///
    /// @param str
    ///   The string whose printable version is wanted.
    /// @param nl_mode
    ///   How to represent the new line character. The default setting of 
    ///   eNewLine_Quote displays the new line as "\n". If set to
    ///   eNewLine_Passthru, a line break is used instead.
    /// @return
    ///   Return a printable version of "str".
    /// @sa
    ///   ParseEscapes
    static string PrintableString(const string& str,
                                  ENewLineMode  nl_mode = eNewLine_Quote);

    /// Parse C-style escape sequences in the specified string, including
    /// all those produced by PrintableString.
    static string ParseEscapes(const string& str);

    /// How to wrap the words in a string to a new line.
    enum EWrapFlags {
        fWrap_Hyphenate  = 0x1, ///< Add a hyphen when breaking words?
        fWrap_HTMLPre    = 0x2, ///< Wrap as preformatted HTML?
        fWrap_FlatFile   = 0x4  ///< Wrap for flat file use.
    };
    typedef int TWrapFlags;     ///< Binary OR of "EWrapFlags"

    /// Encode a string for C/C++.
    ///
    /// Synonym for PrintableString().
    /// @sa PrintableString
    static string CEncode(const string& str);

    /// Encode a string for JavaScript.
    ///
    /// Like to CEncode(), but process some symbols in different way.
    /// @sa PrintableString, CEncode
    static string JavaScriptEncode(const string& str);

    /// Wrap the specified string into lines of a specified width -- prefix,
    /// prefix1 default version.
    ///
    /// Split string "str" into lines of width "width" and add the
    /// resulting lines to the list "arr". Normally, all
    /// lines will begin with "prefix" (counted against "width"),
    /// but the first line will instead begin with "prefix1" if
    /// you supply it.
    ///
    /// @param str
    ///   String to be split into wrapped lines.
    /// @param width
    ///   Width of each wrapped line.
    /// @param arr
    ///   List of strings containing wrapped lines.
    /// @param flags
    ///   How to wrap the words to a new line. See EWrapFlags documentation.
    /// @param prefix
    ///   The prefix string added to each wrapped line, except the first line,
    ///   unless "prefix1" is set.
    ///   If "prefix" is set to 0(default), do not add a prefix string to the
    ///   wrapped lines.
    /// @param prefix1
    ///   The prefix string for the first line. Use this for the first line
    ///   instead of "prefix".
    ///   If "prefix1" is set to 0(default), do not add a prefix string to the
    ///   first line.
    /// @return
    ///   Return "arr", the list of wrapped lines.
    static list<string>& Wrap(const string& str, SIZE_TYPE width,
                              list<string>& arr, TWrapFlags flags = 0,
                              const string* prefix = 0,
                              const string* prefix1 = 0);

    /// Wrap the specified string into lines of a specified width -- prefix1
    /// default version.
    ///
    /// Split string "str" into lines of width "width" and add the
    /// resulting lines to the list "arr". Normally, all
    /// lines will begin with "prefix" (counted against "width"),
    /// but the first line will instead begin with "prefix1" if
    /// you supply it.
    ///
    /// @param str
    ///   String to be split into wrapped lines.
    /// @param width
    ///   Width of each wrapped line.
    /// @param arr
    ///   List of strings containing wrapped lines.
    /// @param flags
    ///   How to wrap the words to a new line. See EWrapFlags documentation.
    /// @param prefix
    ///   The prefix string added to each wrapped line, except the first line,
    ///   unless "prefix1" is set.
    ///   If "prefix" is set to 0, do not add a prefix string to the wrapped
    ///   lines.
    /// @param prefix1
    ///   The prefix string for the first line. Use this for the first line
    ///   instead of "prefix".
    ///   If "prefix1" is set to 0(default), do not add a prefix string to the
    ///   first line.
    /// @return
    ///   Return "arr", the list of wrapped lines.
    static list<string>& Wrap(const string& str, SIZE_TYPE width,
                              list<string>& arr, TWrapFlags flags,
                              const string& prefix, const string* prefix1 = 0);

    /// Wrap the specified string into lines of a specified width.
    ///
    /// Split string "str" into lines of width "width" and add the
    /// resulting lines to the list "arr". Normally, all
    /// lines will begin with "prefix" (counted against "width"),
    /// but the first line will instead begin with "prefix1" if
    /// you supply it.
    ///
    /// @param str
    ///   String to be split into wrapped lines.
    /// @param width
    ///   Width of each wrapped line.
    /// @param arr
    ///   List of strings containing wrapped lines.
    /// @param flags
    ///   How to wrap the words to a new line. See EWrapFlags documentation.
    /// @param prefix
    ///   The prefix string added to each wrapped line, except the first line,
    ///   unless "prefix1" is set.
    ///   If "prefix" is set to 0, do not add a prefix string to the wrapped
    ///   lines.
    /// @param prefix1
    ///   The prefix string for the first line. Use this for the first line
    ///   instead of "prefix".
    ///   If "prefix1" is set to 0, do not add a prefix string to the first
    ///   line.
    /// @return
    ///   Return "arr", the list of wrapped lines.
    static list<string>& Wrap(const string& str, SIZE_TYPE width,
                              list<string>& arr, TWrapFlags flags,
                              const string& prefix, const string& prefix1);


    /// Wrap the list using the specified criteria -- default prefix, 
    /// prefix1 version.
    ///
    /// WrapList() is similar to Wrap(), but tries to avoid splitting any
    /// elements of the list to be wrapped. Also, the "delim" only applies
    /// between elements on the same line; if you want everything to end with
    /// commas or such, you should add them first.
    ///
    /// @param l
    ///   The list to be wrapped.
    /// @param width
    ///   Width of each wrapped line.
    /// @param delim
    ///   Delimiters used to split elements on the same line.
    /// @param arr
    ///   List containing the wrapped list result.
    /// @param flags
    ///   How to wrap the words to a new line. See EWrapFlags documentation.
    /// @param prefix
    ///   The prefix string added to each wrapped line, except the first line,
    ///   unless "prefix1" is set.
    ///   If "prefix" is set to 0(default), do not add a prefix string to the
    ///   wrapped lines.
    /// @param prefix1
    ///   The prefix string for the first line. Use this for the first line
    ///   instead of "prefix".
    ///   If "prefix1" is set to 0(default), do not add a prefix string to the
    ///   first line.
    /// @return
    ///   Return "arr", the wrapped list.
    static list<string>& WrapList(const list<string>& l, SIZE_TYPE width,
                                  const string& delim, list<string>& arr,
                                  TWrapFlags flags = 0,
                                  const string* prefix = 0,
                                  const string* prefix1 = 0);

    /// Wrap the list using the specified criteria -- default prefix1 version.
    ///
    /// WrapList() is similar to Wrap(), but tries to avoid splitting any
    /// elements of the list to be wrapped. Also, the "delim" only applies
    /// between elements on the same line; if you want everything to end with
    /// commas or such, you should add them first.
    ///
    /// @param l
    ///   The list to be wrapped.
    /// @param width
    ///   Width of each wrapped line.
    /// @param delim
    ///   Delimiters used to split elements on the same line.
    /// @param arr
    ///   List containing the wrapped list result.
    /// @param flags
    ///   How to wrap the words to a new line. See EWrapFlags documentation.
    /// @param prefix
    ///   The prefix string added to each wrapped line, except the first line,
    ///   unless "prefix1" is set.
    ///   If "prefix" is set to 0, do not add a prefix string to the
    ///   wrapped lines.
    /// @param prefix1
    ///   The prefix string for the first line. Use this for the first line
    ///   instead of "prefix".
    ///   If "prefix1" is set to 0(default), do not add a prefix string to the
    ///   first line.
    /// @return
    ///   Return "arr", the wrappe list.
    static list<string>& WrapList(const list<string>& l, SIZE_TYPE width,
                                  const string& delim, list<string>& arr,
                                  TWrapFlags flags, const string& prefix,
                                  const string* prefix1 = 0);
        
    /// Wrap the list using the specified criteria.
    ///
    /// WrapList() is similar to Wrap(), but tries to avoid splitting any
    /// elements of the list to be wrapped. Also, the "delim" only applies
    /// between elements on the same line; if you want everything to end with
    /// commas or such, you should add them first.
    ///
    /// @param l
    ///   The list to be wrapped.
    /// @param width
    ///   Width of each wrapped line.
    /// @param delim
    ///   Delimiters used to split elements on the same line.
    /// @param arr
    ///   List containing the wrapped list result.
    /// @param flags
    ///   How to wrap the words to a new line. See EWrapFlags documentation.
    /// @param prefix
    ///   The prefix string added to each wrapped line, except the first line,
    ///   unless "prefix1" is set.
    ///   If "prefix" is set to 0, do not add a prefix string to the
    ///   wrapped lines.
    /// @param prefix1
    ///   The prefix string for the first line. Use this for the first line
    ///   instead of "prefix".
    ///   If "prefix1" is set to 0, do not add a prefix string to the
    ///   first line.
    /// @return
    ///   Return "arr", the wrapped list.
    static list<string>& WrapList(const list<string>& l, SIZE_TYPE width,
                                  const string& delim, list<string>& arr,
                                  TWrapFlags flags, const string& prefix,
                                  const string& prefix1);
}; // class NStr



/////////////////////////////////////////////////////////////////////////////
///
/// CStringUTF8 --
///
///   An UTF-8 string.
///   Stores character data in UTF-8 encoding form.
///   Being initialized, converts source characters into UTF-8.
///   Can convert data back into a particular encoding form (non-UTF8)
///   Supported encodings:
///      ISO 8859-1 (Latin1)
///      Microsoft Windows code page 1252
///      UCS-2/UTF-16 (no surrogates)

enum EEncoding {
    eEncoding_Unknown,
    eEncoding_UTF8,
    eEncoding_Ascii,
    eEncoding_ISO8859_1,
    eEncoding_Windows_1252
};
typedef Uint4 TUnicodeSymbol;

class NCBI_XNCBI_EXPORT CStringUTF8 : public string
{
public:
    enum EValidate {
        eNoValidate,
        eValidate
    };

    /// Default constructor.
    CStringUTF8(void)
    {
    }

    /// Destructor.
    ~CStringUTF8(void)
    {
    }

    /// Copy constructor.
    ///
    /// @param src
    ///   Source UTF-8 string
    /// @param validate
    ///   Verify that the source character encoding is really UTF-8
    CStringUTF8(const CStringUTF8& src, EValidate validate = eNoValidate)
        : string(src)
    {
        if (validate == eValidate) {
            x_Validate();
        }
    }

    /// Constructor from a C++ string
    ///
    /// @param src
    ///   Source string
    /// @param encoding
    ///   Character encoding of the source string
    /// @param validate
    ///   Verify the character encoding of the source
    CStringUTF8(const string& src,
                EEncoding encoding = eEncoding_ISO8859_1,
                EValidate validate = eNoValidate)
        : string()
    {
        x_Append(src.c_str(), encoding, validate);
    }

    /// Constructor from a C string
    ///
    /// @param src
    ///   Source zero-terminated character buffer
    /// @param encoding
    ///   Character encoding of the source string
    /// @param validate
    ///   Verify the character encoding of the source
    CStringUTF8(const char* src,
                EEncoding encoding = eEncoding_ISO8859_1,
                EValidate validate = eNoValidate)
        : string()
    {
        x_Append(src, encoding, validate);
    }

#if defined(HAVE_WSTRING)
    /// Constructor from a wstring (UTF-16).
    ///
    /// Defined only if wstring is supported by the compiler.
    CStringUTF8(const wstring& src)
        : string()
    {
        x_Append( src.c_str());
    }

    /// Constructor from a whcar_t* (UTF-16).
    ///
    /// Defined only if wstring is supported by the compiler.
    CStringUTF8(const wchar_t* src)
        : string()
    {
        x_Append(src);
    }
#endif // HAVE_WSTRING

    /// Assign to UTF8 string
    CStringUTF8& operator= (const CStringUTF8& src)
    {
        string::operator= (src);
        return *this;
    }

    /// Assign to C++ string in Latin1 encoding.
    CStringUTF8& operator= (const string& src)
    {
        erase();
        x_Append(src.c_str());
        return *this;
    }

    /// Assign to C string in Latin1 encoding.
    CStringUTF8& operator= (const char* src)
    {
        erase();
        x_Append(src);
        return *this;
    }

#if defined(HAVE_WSTRING)
    /// Assign to C++ wstring in UTF16 encoding
    ///
    /// Defined only if wstring is supported by the compiler.
    CStringUTF8& operator= (const wstring& src)
    {
        erase();
        x_Append(src.c_str());
        return *this;
    }

    /// Assign to wide-char C string in UTF16 encoding
    ///
    /// Defined only if wstring is supported by the compiler.
    CStringUTF8& operator= (const wchar_t* src)
    {
        erase();
        x_Append(src);
        return *this;
    }
#endif // HAVE_WSTRING

    /// Append a string in UTF8 encoding
    CStringUTF8& operator+= (const CStringUTF8& src)
    {
        string::operator+= (src);
        return *this;
    }

    /// Append a C++ string in Latin1 encoding
    CStringUTF8& operator+= (const string& src)
    {
        x_Append(src.c_str());
        return *this;
    }

    /// Append a C string in Latin1 encoding
    CStringUTF8& operator+= (const char* src)
    {
        x_Append(src);
        return *this;
    }

#if defined(HAVE_WSTRING)
    /// Append a C++ wstring in UTF-16 encoding
    ///
    /// Defined only if wstring is supported by the compiler.
    CStringUTF8& operator+= (const wstring& src)
    {
        x_Append(src.c_str());
        return *this;
    }

    /// Append a wide-char C string in UTF-16 encoding
    ///
    /// Defined only if wstring is supported by the compiler.
    CStringUTF8& operator+= (const wchar_t* src)
    {
        x_Append(src);
        return *this;
    }
#endif // HAVE_WSTRING
    
    /// Assign to C++ string
    ///
    /// @param src
    ///   Source string
    /// @param encoding
    ///   Character encoding of the source string
    /// @param validate
    ///   Verify the character encoding of the source
    CStringUTF8& Assign(const string& src,
                        EEncoding encoding,
                        EValidate validate = eNoValidate)
    {
        erase();
        x_Append(src.c_str(), encoding, validate);
        return *this;
    }

    /// Assign to C string
    ///
    /// @param src
    ///   Source zero-terminated character buffer
    /// @param encoding
    ///   Character encoding of the source string
    /// @param validate
    ///   Verify the character encoding of the source
    CStringUTF8& Assign(const char* src,
                        EEncoding encoding,
                        EValidate validate = eNoValidate)
    {
        erase();
        x_Append(src, encoding, validate);
        return *this;
    }

    /// Assign to a single character
    ///
    /// @param ch
    ///   Character
    /// @param encoding
    ///   Character encoding
    CStringUTF8& Assign(char ch,
                        EEncoding encoding)
    {
        erase();
        x_AppendChar( CharToSymbol( ch, encoding ) );
        return *this;
    }

    /// Append a C++ string
    ///
    /// @param src
    ///   Source string
    /// @param encoding
    ///   Character encoding of the source string
    /// @param validate
    ///   Verify the character encoding of the source
    CStringUTF8& Append(const string& src,
                        EEncoding encoding,
                        EValidate validate = eNoValidate)
    {
        x_Append(src.c_str(), encoding, validate);
        return *this;
    }

    /// Append a C string
    ///
    /// @param src
    ///   Source zero-terminated character buffer
    /// @param encoding
    ///   Character encoding of the source string
    /// @param validate
    ///   Verify the character encoding of the source
    CStringUTF8& Append(const char* src,
                        EEncoding encoding,
                        EValidate validate = eNoValidate)
    {
        x_Append(src, encoding, validate);
        return *this;
    }

    /// Append single character
    ///
    /// @param ch
    ///   Character
    /// @param encoding
    ///   Character encoding
    CStringUTF8& Append(char ch,
                        EEncoding encoding)
    {
        x_AppendChar( CharToSymbol( ch, encoding ) );
        return *this;
    }

    /// Append single Unicode code point
    ///
    /// @param ch
    ///   Unicode code point
    CStringUTF8& Append(TUnicodeSymbol ch)
    {
        x_AppendChar(ch);
        return *this;
    }

    /// Get the number of symbols (code points) in the string
    ///
    /// @return
    ///   Number of symbols (code points)
    SIZE_TYPE GetSymbolCount(void) const;
    
    /// Get the number of valid UTF-8 symbols (code points) in the buffer
    ///
    /// @param src
    ///   Character buffer
    /// @param buf_size
    ///   The number of bytes in the buffer
    /// @return
    ///   Number of valid symbols (no exception thrown)
    static SIZE_TYPE GetValidSymbolCount(const char* src, SIZE_TYPE buf_size);
    
    /// Get the number of valid UTF-8 bytes (code units) in the buffer
    ///
    /// @param src
    ///   Character buffer
    /// @param buf_size
    ///   The number of bytes in the buffer
    /// @return
    ///   Number of valid bytes (no exception thrown)
    static SIZE_TYPE GetValidBytesCount(const char* src, SIZE_TYPE buf_size);

    /// Check that the character encoding of the string is valid UTF-8
    ///
    /// @return
    ///   Result of the check
    bool IsValid(void) const
    {
        return MatchEncoding(c_str(), eEncoding_UTF8);
    }
    /// Convert to ISO 8859-1 (Latin1) character representation
    ///
    /// Can throw a CStringException if the conversion is impossible
    /// or the string has invalid UTF-8 format.
    string AsLatin1(void) const
    {
        return AsSingleByteString(eEncoding_ISO8859_1);
    }
    
    /// Convert the string to a single-byte character representation
    ///
    /// Can throw a CStringException if the conversion is impossible
    /// or the string has invalid UTF-8 format.
    /// @param encoding
    ///   Desired encoding
    /// @return
    ///   C++ string
    string AsSingleByteString(EEncoding encoding) const;

#if defined(HAVE_WSTRING)
    /// Convert to Unicode (UTF-16 with no surrogates).
    ///
    /// Can throw a CStringException if the conversion is impossible
    /// or the string has invalid UTF-8 format.
    /// Defined only if wstring is supported by the compiler.
    wstring AsUnicode(void) const;
#endif // HAVE_WSTRING

    /// Guess the encoding of the C string
    ///
    /// It can distinguish between UTF-8, Latin1, and Win1252 only
    /// @param src
    ///   Source zero-terminated character buffer
    /// @return
    ///   Encoding
    static EEncoding GuessEncoding( const char* src);

    /// Guess the encoding of the C++ string
    ///
    /// It can distinguish between UTF-8, Latin1, and Win1252 only
    /// @param src
    ///   Source string
    /// @return
    ///   Encoding
    static EEncoding GuessEncoding( const string& src)
    {
        return GuessEncoding( src.c_str());
    }

    /// Check the encoding of the C string
    ///
    /// Check that the encoding of the source is the same, or
    /// is compatible with the specified one
    /// @param src
    ///   Source string
    /// @param encoding
    ///   Character encoding form to check against
    /// @return
    ///   Boolean result: encoding is same or compatible
    static bool MatchEncoding( const char* src, EEncoding encoding);

    /// Check the encoding of the C++ string
    ///
    /// Check that the encoding of the source is the same, or
    /// is compatible with the specified one
    /// @param src
    ///   Source string
    /// @param encoding
    ///   Character encoding form to check against
    /// @return
    ///   Boolean result: encoding is same or compatible
    static bool MatchEncoding( const string& src, EEncoding encoding)
    {
        return MatchEncoding( src.c_str(), encoding);
    }
    
    /// Convert encoded character into UTF16
    ///
    /// @param ch
    ///   Encoded character
    /// @param encoding
    ///   Character encoding
    /// @return
    ///   Code point
    static TUnicodeSymbol CharToSymbol(char ch, EEncoding encoding);
    
    /// Convert Unicode code point into encoded character
    ///
    /// @param ch
    ///   Code point
    /// @param encoding
    ///   Character encoding
    /// @return
    ///   Encoded character
    static char SymbolToChar(TUnicodeSymbol sym, EEncoding encoding);

    /// Convert sequence of UTF8 code units into Unicode code point
    ///
    /// @param src
    ///   UTF8 zero-terminated buffer
    /// @return
    ///   Unicode code point
    static TUnicodeSymbol Decode(const char*& src);

    /// Convert first character of UTF8 sequence into Unicode
    ///
    /// @param ch
    ///   character
    /// @param more
    ///   if the character is valid, - how many more characters to expect
    /// @return
    ///   non-zero, if the character is valid
    static TUnicodeSymbol  DecodeFirst(char ch, size_t& more);

    /// Convert next character of UTF8 sequence into Unicode
    ///
    /// @param ch
    ///   character
    /// @param ch16
    ///   Unicode character
    /// @return
    ///   non-zero, if the character is valid
    static TUnicodeSymbol  DecodeNext(TUnicodeSymbol chU, char ch);

private:
    /// Function AsAscii is deprecated - use AsLatin1() instead
    string AsAscii(void) const
    {
        return AsLatin1();
    }

    void   x_Validate(void) const;
    /// Convert Unicode code point into UTF8 and append
    void   x_AppendChar(TUnicodeSymbol ch);
    /// Convert coded character sequence into UTF8 and append
    void   x_Append(const char* src,
                    EEncoding encoding = eEncoding_ISO8859_1,
                    EValidate validate = eNoValidate);
#if defined(HAVE_WSTRING)
    /// Convert Unicode character sequence into UTF8 and append
    void x_Append(const wchar_t* src);
#endif // HAVE_WSTRING
    /// Check how many bytes is needed to represent the code point in UTF8
    static size_t x_BytesNeeded(TUnicodeSymbol ch);
    /// Check if the character is valid first code unit of UTF8
    static bool   x_EvalFirst(char ch, size_t& more);
    /// Check if the character is valid non-first code unit of UTF8
    static bool   x_EvalNext(char ch);
};



/////////////////////////////////////////////////////////////////////////////
///
/// CParseTemplException --
///
/// Define template class for parsing exception. This class is used to define
/// exceptions for complex parsing tasks and includes an additional m_Pos
/// data member. The constructor requires that an additional postional
/// parameter be supplied along with the description message.

template <class TBase>
class CParseTemplException : EXCEPTION_VIRTUAL_BASE public TBase
{
public:
    /// Error types that for exception class.
    enum EErrCode {
        eErr        ///< Generic error 
    };

    /// Translate from the error code value to its string representation.
    virtual const char* GetErrCodeString(void) const
    {
        switch (GetErrCode()) {
        case eErr: return "eErr";
        default:   return CException::GetErrCodeString();
        }
    }

    /// Constructor.
    ///
    /// Report "pos" along with "what".
    CParseTemplException(const CDiagCompileInfo &info,
        const CException* prev_exception,
        EErrCode err_code,const string& message,
        string::size_type pos, EDiagSev severity = eDiag_Error)
          : TBase(info, prev_exception,
            (typename TBase::EErrCode)(CException::eInvalid),
            message), m_Pos(pos)
    {
        this->x_Init(info,
                     string("{") + NStr::UIntToString((unsigned long)m_Pos) +
                     "} " + message,
                     prev_exception,
                     severity);
        this->x_InitErrCode((CException::EErrCode) err_code);
    }

    /// Constructor.
    CParseTemplException(const CParseTemplException<TBase>& other)
        : TBase(other)
    {
        m_Pos = other.m_Pos;
        x_Assign(other);
    }

    /// Destructor.
    virtual ~CParseTemplException(void) throw() {}

    /// Report error position.
    virtual void ReportExtra(ostream& out) const
    {
        out << "m_Pos = " << (unsigned long)m_Pos;
    }

    // Attributes.

    /// Get exception class type.
    virtual const char* GetType(void) const { return "CParseTemplException"; }

    /// Get error code.
    EErrCode GetErrCode(void) const
    {
        return typeid(*this) == typeid(CParseTemplException<TBase>) ?
            (typename CParseTemplException<TBase>::EErrCode)
                this->x_GetErrCode() :
            (typename CParseTemplException<TBase>::EErrCode)
                CException::eInvalid;
    }

    /// Get error position.
    string::size_type GetPos(void) const throw() { return m_Pos; }

protected:
    /// Constructor.
    CParseTemplException(void)
    {
        m_Pos = 0;
    }

    /// Helper clone method.
    virtual const CException* x_Clone(void) const
    {
        return new CParseTemplException<TBase>(*this);
    }

private:
    string::size_type m_Pos;    ///< Error position
};


/////////////////////////////////////////////////////////////////////////////
///
/// CStringException --
///
/// Define exceptions generated by string classes.
///
/// CStringException inherits its basic functionality from
/// CParseTemplException<CCoreException> and defines additional error codes
/// for string parsing.

class NCBI_XNCBI_EXPORT CStringException : public CParseTemplException<CCoreException>
{
public:
    /// Error types that string classes can generate.
    enum EErrCode {
        eConvert,       ///< Failure to convert string
        eBadArgs,       ///< Bad arguments to string methods 
        eFormat         ///< Wrong format for any input to string methods
    };

    /// Translate from the error code value to its string representation.
    virtual const char* GetErrCodeString(void) const;

    // Standard exception boilerplate code.
    NCBI_EXCEPTION_DEFAULT2(CStringException,
        CParseTemplException<CCoreException>, std::string::size_type);
};



/////////////////////////////////////////////////////////////////////////////
//  Predicates
//



/////////////////////////////////////////////////////////////////////////////
///
/// Define Case-sensitive string comparison methods.
///
/// Used as arguments to template functions for specifying the type of 
/// comparison.

template <typename T>
struct PCase_Generic
{
    /// Return difference between "s1" and "s2".
    int Compare(const T& s1, const T& s2) const;

    /// Return TRUE if s1 < s2.
    bool Less(const T& s1, const T& s2) const;

    /// Return TRUE if s1 == s2.
    bool Equals(const T& s1, const T& s2) const;

    /// Return TRUE if s1 < s2.
    bool operator()(const T& s1, const T& s2) const;
};

typedef PCase_Generic<string>       PCase;
typedef PCase_Generic<const char *> PCase_CStr;



/////////////////////////////////////////////////////////////////////////////
///
/// Define Case-insensitive string comparison methods.
///
/// Used as arguments to template functions for specifying the type of 
/// comparison.
///
/// @sa PNocase_Conditional_Generic

template <typename T>
struct PNocase_Generic
{
    /// Return difference between "s1" and "s2".
    int Compare(const T& s1, const T& s2) const;

    /// Return TRUE if s1 < s2.
    bool Less(const T& s1, const T& s2) const;

    /// Return TRUE if s1 == s2.
    bool Equals(const T& s1, const T& s2) const;

    /// Return TRUE if s1 < s2 ignoring case.
    bool operator()(const T& s1, const T& s2) const;
};

typedef PNocase_Generic<string>       PNocase;
typedef PNocase_Generic<const char *> PNocase_CStr;


/////////////////////////////////////////////////////////////////////////////
///
/// Define Case-insensitive string comparison methods.
/// Case sensitivity can be turned on and off at runtime.
///
/// Used as arguments to template functions for specifying the type of 
/// comparison.
///
/// @sa PNocase_Generic

template <typename T>
class PNocase_Conditional_Generic
{
public:
    /// Construction
    PNocase_Conditional_Generic(NStr::ECase case_sens = NStr::eCase);

    /// Get comparison type
    NStr::ECase GetCase() const { return m_CaseSensitive; }

    /// Set comparison type
    void SetCase(NStr::ECase case_sens) { m_CaseSensitive = case_sens; }

    /// Return difference between "s1" and "s2".
    int Compare(const T& s1, const T& s2) const;

    /// Return TRUE if s1 < s2.
    bool Less(const T& s1, const T& s2) const;

    /// Return TRUE if s1 == s2.
    bool Equals(const T& s1, const T& s2) const;

    /// Return TRUE if s1 < s2 ignoring case.
    bool operator()(const T& s1, const T& s2) const;
private:
    NStr::ECase m_CaseSensitive; ///< case sensitive when TRUE
};

typedef PNocase_Conditional_Generic<string>       PNocase_Conditional;
typedef PNocase_Conditional_Generic<const char *> PNocase_Conditional_CStr;


/////////////////////////////////////////////////////////////////////////////
//  Algorithms
//


/// Check equivalence of arguments using predicate.
template<class Arg1, class Arg2, class Pred>
inline
bool AStrEquiv(const Arg1& x, const Arg2& y, Pred pr)
{
    return pr.Equals(x, y);
}


/* @} */



/////////////////////////////////////////////////////////////////////////////
//
//  IMPLEMENTATION of INLINE functions
//
/////////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////////
//  CNcbiEmptyString::
//
#if !defined(NCBI_OS_MSWIN) && !( defined(NCBI_OS_LINUX)  &&  defined(NCBI_COMPILER_GCC) )
inline
const string& CNcbiEmptyString::Get(void)
{
    const string* str = m_Str;
    return str ? *str: FirstGet();
}
#endif


/////////////////////////////////////////////////////////////////////////////
//  CTempString::
//

inline
CTempString::const_iterator CTempString::begin() const
{
    return m_String;
}

inline
CTempString::const_iterator CTempString::end() const
{
    return m_String + m_Length;
}

inline
const char* CTempString::data(void) const
{
    _ASSERT(m_String);
    return m_String;
}

inline
CTempString::size_type CTempString::length(void) const
{
    return m_Length;
}

inline
CTempString::size_type CTempString::size(void) const
{
    return m_Length;
}

inline
bool CTempString::empty(void) const
{
    return m_Length == 0;
}

inline
char CTempString::operator[] (size_type pos) const
{
    if ( pos < m_Length ) {
        return m_String[pos];
    }
    return '\0';
}

inline
void CTempString::x_Reset(void)
{
    m_String = "";
    m_Length = 0;
}

inline
void CTempString::x_Init(const char* str, size_type str_length,
                         size_type pos, size_type length)
{
    if ( pos >= str_length ) {
        x_Reset();
        return;
    }
    m_String = str + pos;
    m_Length = min(length, str_length - pos);
    return;
}

inline
CTempString::CTempString(void)
{
    x_Reset();
    return;
}

inline
CTempString::CTempString(const char* str)
{
    if ( !str ) {
        x_Reset();
        return;
    }
    m_String = str;
    m_Length = strlen(str);
    return;
}

template<size_t Size>
inline
CTempString::CTempString(const char (&str)[Size])
    : m_String(str), m_Length(Size-1)
{
    return;
}

inline
CTempString::CTempString(const char* str, size_type length)
    : m_String(str), m_Length(length)
{
    return;
}

inline
CTempString::CTempString(const char* str, size_type pos, size_type length)
    : m_String(str+pos), m_Length(length)
{
    return;
}

inline
CTempString::CTempString(const string& str)
    : m_String(str.data()), m_Length(str.size())
{
    return;
}

inline
CTempString::CTempString(const string& str, size_type length)
    : m_String(str.data()), m_Length(min(str.size(), length))
{
    return;
}

inline
CTempString::CTempString(const string& str, size_type pos, size_type length)
    : m_String(NULL)
    , m_Length(0)
{
    x_Init(str.data(), str.size(), pos, length);
    _ASSERT(m_String);
    return;
}


inline
CTempString::CTempString(const CTempString& str)
    : m_String(NULL)
    , m_Length(0)
{
    x_Init(str.data(), str.size(), 0, str.size());
}

inline
CTempString::CTempString(const CTempString& str, size_type pos)
    : m_String(NULL)
    , m_Length(0)
{
    x_Init(str.data(), str.size(), pos, str.size() - pos);
}

inline
CTempString::CTempString(const CTempString& str, size_type pos, size_type length)
{
    x_Init(str.data(), str.size(), pos, length);
}


inline
void CTempString::erase(size_type pos)
{
    if (pos < m_Length) {
        m_Length = pos;
    }
}


/// copy a substring into a string
/// These are analogs of basic_string::assign()
inline
void CTempString::Copy(string& dst, size_type pos, size_type len) const
{
    if (pos < length()) {
        len = min(len, length()-pos);
        dst.assign(begin() + pos, begin() + pos + len);
    } else {
        dst.erase();
    }
}

inline
CTempString::size_type CTempString::find_first_of(const CTempString& match,
                                                  size_type pos) const
{
    if (match.length()  &&  pos < length()) {
        const_iterator it = std::find_first_of(begin() + pos, end(),
                                               match.begin(), match.end());
        if (it != end()) {
            return it - begin();
        }
    }
    return npos;
}


inline
CTempString::size_type CTempString::find_first_not_of(const CTempString& match,
                                                      size_type pos) const
{
    if (match.length()  &&  pos < length()) {
        const_iterator it = begin() + pos;
        const_iterator end_it = end();

        const_iterator match_end = match.end();
        for ( ;  it != end_it;  ++it) {
            bool found = false;
            for (const_iterator match_it = match.begin();
                 match_it != match_end;  ++match_it) {
                if (*it == *match_it) {
                    found = true;
                    break;
                }
            }
            if ( !found ) {
                return it - begin();
            }
        }
    }
    return npos;
}


inline
CTempString::size_type CTempString::find(const CTempString& match,
                                         size_type pos) const
{
    if (pos + match.length() > length()) {
        return npos;
    }
    if (match.length() == 0) {
        return pos;
    }

    size_type length_limit = length() - match.length();
    while ( (pos = find_first_of(CTempString(match, 0, 1), pos)) !=
            string::npos) {
        if (pos > length_limit) {
            return npos;
        }

        int res = memcmp(begin() + pos + 1,
                         match.begin() + 1,
                         match.length() - 1);
        if (res == 0) {
            return pos;
        }
        ++pos;
    }
    return npos;
}


inline
CTempString CTempString::substr(size_type pos) const
{
    return CTempString(*this, pos, npos);
}


inline
CTempString CTempString::substr(size_type pos, size_type len) const
{
    return CTempString(*this, pos, len);
}


inline
CTempString::operator string(void) const
{
    return string(data(), length());
}

inline
bool CTempString::x_Equals(const_iterator it2, const_iterator end2) const
{
    size_type comp_len = end2 - it2;
    if (comp_len != length()) {
        return false;
    }
    return (memcmp(begin(), it2, comp_len) == 0);
}


inline
bool CTempString::operator==(const char* str) const
{
    if ( !str || !m_String ) {
        return !str && !m_String;
    }
    return x_Equals(str, str + strlen(str));
}

inline
bool CTempString::operator==(const string& str) const
{
    return x_Equals(str.data(), str.data() + str.size());
}

inline
bool CTempString::operator==(const CTempString& str) const
{
    return x_Equals(str.data(), str.data() + str.size());
}

inline
bool CTempString::x_Less(const_iterator it2, const_iterator end2) const
{
    size_type other_len = end2 - it2;
    size_type comp_len = min(other_len, length());
    int res = memcmp(begin(), it2, comp_len);
    if ( res != 0 ) {
        return res < 0;
    }
    return length() < other_len;
}


inline
bool CTempString::operator<(const char* str) const
{
    if ( !str || !m_String ) {
        return str  &&  !m_String;
    }
    return x_Less(str, str + strlen(str));
}

inline
bool CTempString::operator<(const string& str) const
{
    return x_Less(str.data(), str.data() + str.size());
}

inline
bool CTempString::operator<(const CTempString& str) const
{
    return x_Less(str.data(), str.data() + str.size());
}


/////////////////////////////////////////////////////////////////////////////
//  NStr::
//

inline
string NStr::IntToString(long value,
                         TNumToStringFlags flags, int base)
{
    string ret;
    IntToString(ret, value, flags, base);
    return ret;
}

inline
string NStr::UIntToString(unsigned long value,
                          TNumToStringFlags flags, int base)
{
    string ret;
    UIntToString(ret, value, flags, base);
    return ret;
}

inline
int NStr::HexChar(char ch)
{
    unsigned int rc = ch - '0';
    if (rc <= 9) {
        return rc;
    } else {
        rc = (ch | ' ') - 'a';
        return rc <= 5 ? int(rc + 10) : -1;
    }
}

inline
bool NStr::MatchesMask(const string& str, const string& mask, ECase use_case)
{
    return MatchesMask(str.c_str(), mask.c_str(), use_case);
}

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
#if NCBI_COMPILER_MSVC && (_MSC_VER >= 1400)
    return ::_stricmp(s1, s2);
#else
    return ::stricmp(s1, s2);
#endif

#elif defined(HAVE_STRCASECMP_LC)
    return ::strcasecmp(s1, s2);

#else
    int diff = 0;
    for ( ;; ++s1, ++s2) {
        char c1 = *s1;
        // calculate difference
        diff = tolower((unsigned char) c1) - tolower((unsigned char)(*s2));
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
#if NCBI_COMPILER_MSVC && (_MSC_VER >= 1400)
    return ::_strnicmp(s1, s2, n);
#else
    return ::strnicmp(s1, s2, n);
#endif

#elif defined(HAVE_STRCASECMP_LC)
    return ::strncasecmp(s1, s2, n);

#else
    int diff = 0;
    for ( ; ; ++s1, ++s2, --n) {
        if (n == 0)
            return 0;
        char c1 = *s1;
        // calculate difference
        diff = tolower((unsigned char) c1) - tolower((unsigned char)(*s2));
        // if end of string or different
        if (!c1  ||  diff)
            break; // return difference
    }
    return diff;
#endif
}

inline
size_t NStr::strftime(char* s, size_t maxsize, const char* format,
                      const struct tm* timeptr)
{
#if defined(NCBI_COMPILER_MSVC)
    string x_format = Replace(format, "%T", "%H:%M:%S");
    ReplaceInPlace(x_format,          "%D", "%m/%d/%y");
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
bool NStr::Equal(const string& str, SIZE_TYPE pos, SIZE_TYPE n,
                  const char* pattern, ECase use_case)
{
    return use_case == eCase ?
        EqualCase(str, pos, n, pattern) : EqualNocase(str, pos, n, pattern);
}

inline
bool NStr::Equal(const string& str, SIZE_TYPE pos, SIZE_TYPE n,
                  const string& pattern, ECase use_case)
{
    return use_case == eCase ?
        EqualCase(str, pos, n, pattern) : EqualNocase(str, pos, n, pattern);
}

inline
bool NStr::EqualCase(const char* s1, const char* s2)
{
    return NStr::strcmp(s1, s2) == 0;
}

inline
bool NStr::EqualNocase(const char* s1, const char* s2)
{
    return NStr::strcasecmp(s1, s2) == 0;
}

inline
bool NStr::EqualCase(const string& str, SIZE_TYPE pos, SIZE_TYPE n,
                     const char* pattern)
{
    return NStr::CompareCase(str, pos, n, pattern) == 0;
}

inline
bool NStr::EqualCase(const string& str, SIZE_TYPE pos, SIZE_TYPE n,
                     const string& pattern)
{
    return NStr::CompareCase(str, pos, n, pattern) == 0;
}

inline
bool NStr::Equal(const char* s1, const char* s2, ECase use_case)
{
    return (use_case == eCase ? EqualCase(s1, s2) : EqualNocase(s1, s2));
}

inline
bool NStr::Equal(const string& s1, const char* s2, ECase use_case)
{
    return Equal(s1.c_str(), s2, use_case);
}

inline
bool NStr::Equal(const char* s1, const string& s2, ECase use_case)
{
    return Equal(s1, s2.c_str(), use_case);
}

inline
bool NStr::Equal(const string& s1, const string& s2, ECase use_case)
{
    return Equal(s1.c_str(), s2.c_str(), use_case);
}

inline
bool NStr::EqualCase(const string& s1, const string& s2)
{
    // return EqualCase(s1.c_str(), s2.c_str());
    return s1 == s2;
}

inline
bool NStr::EqualNocase(const string& s1, const string& s2)
{
    return EqualNocase(s1.c_str(), s2.c_str());
}

inline
bool NStr::EqualNocase(const string& str, SIZE_TYPE pos, SIZE_TYPE n,
                             const char* pattern)
{
    return CompareNocase(str, pos, n, pattern) == 0;
}

inline
bool NStr::EqualNocase(const string& str, SIZE_TYPE pos, SIZE_TYPE n,
                             const string& pattern)
{
    return CompareNocase(str, pos, n, pattern) == 0;
}

inline
bool NStr::StartsWith(const string& str, const string& start, ECase use_case)
{
    return str.size() >= start.size()  &&
        Compare(str, 0, start.size(), start, use_case) == 0;
}

inline
bool NStr::StartsWith(const string& str, char start, ECase use_case)
{
    return !str.empty()  &&
        ((use_case == eCase) ? (str[0] == start) :
         (toupper((unsigned char) str[0]) == start  ||
          tolower((unsigned char) str[0])));
}

inline
bool NStr::EndsWith(const string& str, const string& end, ECase use_case)
{
    return str.size() >= end.size()  &&
        Compare(str, str.size() - end.size(), end.size(), end, use_case) == 0;
}

inline
bool NStr::EndsWith(const string& str, char end, ECase use_case)
{
    if (!str.empty()) {
        char last = str[str.length() - 1];
        return (use_case == eCase) ? (last == end) :
               (toupper((unsigned char) last) == end  ||
                tolower((unsigned char) last) == end);
    }
    return false;
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
string NStr::CEncode(const string& str)
{
    return PrintableString(str);
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
//  PCase_Generic::
//

template <typename T>
inline
int PCase_Generic<T>::Compare(const T& s1, const T& s2) const
{
    return NStr::Compare(s1, s2, NStr::eCase);
}

template <typename T>
inline
bool PCase_Generic<T>::Less(const T& s1, const T& s2) const
{
    return Compare(s1, s2) < 0;
}

template <typename T>
inline
bool PCase_Generic<T>::Equals(const T& s1, const T& s2) const
{
    return Compare(s1, s2) == 0;
}

template <typename T>
inline
bool PCase_Generic<T>::operator()(const T& s1, const T& s2) const
{
    return Less(s1, s2);
}



////////////////////////////////////////////////////////////////////////////
//  PNocase_Generic<T>::
//


template <typename T>
inline
int PNocase_Generic<T>::Compare(const T& s1, const T& s2) const
{
    return NStr::Compare(s1, s2, NStr::eNocase);
}

template <typename T>
inline
bool PNocase_Generic<T>::Less(const T& s1, const T& s2) const
{
    return Compare(s1, s2) < 0;
}

template <typename T>
inline
bool PNocase_Generic<T>::Equals(const T& s1, const T& s2) const
{
    return Compare(s1, s2) == 0;
}

template <typename T>
inline
bool PNocase_Generic<T>::operator()(const T& s1, const T& s2) const
{
    return Less(s1, s2);
}

////////////////////////////////////////////////////////////////////////////
//  PNocase_Conditional_Generic<T>::
//

template <typename T>
inline
PNocase_Conditional_Generic<T>::PNocase_Conditional_Generic(NStr::ECase cs)
    : m_CaseSensitive(cs)
{}

template <typename T>
inline
int PNocase_Conditional_Generic<T>::Compare(const T& s1, const T& s2) const
{
    return NStr::Compare(s1, s2, m_CaseSensitive);
}

template <typename T>
inline
bool PNocase_Conditional_Generic<T>::Less(const T& s1, const T& s2) const
{
    return Compare(s1, s2) < 0;
}

template <typename T>
inline
bool PNocase_Conditional_Generic<T>::Equals(const T& s1, const T& s2) const
{
    return Compare(s1, s2) == 0;
}

template <typename T>
inline
bool PNocase_Conditional_Generic<T>::operator()(const T& s1, const T& s2) const
{
    return Less(s1, s2);
}



END_NCBI_SCOPE


/*
 * ===========================================================================
 *
 * $Log$
 * Revision 1.116  2006/12/21 12:46:55  dicuccio
 * Added erase() for simple truncation
 *
 * Revision 1.115  2006/12/20 16:39:02  gouriano
 * Fixed MSVC2005 compiler error
 *
 * Revision 1.114  2006/12/20 13:22:59  dicuccio
 * Further updates to CTempString:
 * - Code rearrangements and patches from Eugene Vasilchenko: Unified
 *   initialization; reordered functions for better inlining.
 * - Implemented find()
 *
 * Revision 1.113  2006/12/18 13:01:26  dicuccio
 * Make CTempString more congruent with std::string
 *
 * Revision 1.112  2006/11/29 13:55:39  gouriano
 * Moved GetErrorCodeString method into cpp
 *
 * Revision 1.111  2006/10/17 14:02:32  ucko
 * Drop extraneous NStr:: qualifier from HexChar's declaration to fix
 * compilation under GCC 4.1.x.
 *
 * Revision 1.110  2006/10/16 15:41:20  ucko
 * HexChar: clean up formatting, and tweak logic to avoid GCC warnings.
 *
 * Revision 1.109  2006/10/16 15:10:48  ivanov
 * + NStr::HexChar()
 *
 * Revision 1.108  2006/09/21 18:15:22  lavr
 * NStr::strftime() to use ReplaceInPlace()
 *
 * Revision 1.107  2006/09/18 15:10:08  gouriano
 * Changed TUnicodeSymbol to 4 bytes
 *
 * Revision 1.106  2006/09/11 13:08:04  gouriano
 * Added buffer validation into CStringUTF8
 *
 * Revision 1.105  2006/08/31 17:23:49  gouriano
 * Possibility to append unicode char to UTF8 string
 *
 * Revision 1.104  2006/08/23 13:32:50  ivanov
 * + NStr::ReplaceInPlace()
 *
 * Revision 1.103  2006/04/19 18:38:23  ivanov
 * Added additional optional parameter to Split(), Tokenize() and
 * TokenizePattern() to get tokens' positions in source string
 *
 * Revision 1.102  2006/02/06 15:46:58  ivanov
 * Replaced class-based NStr::TStringToNumFlags to int-based counterparts
 *
 * Revision 1.101  2006/01/18 19:45:22  ssikorsk
 * Added an extra argument to CException::x_Init
 *
 * Revision 1.100  2006/01/03 17:41:23  ivanov
 * Added comment for NStr:StringToDouble()
 *
 * Revision 1.99  2005/12/27 14:53:54  gouriano
 * Adjustments for MSVC 2005 Express
 *
 * Revision 1.98  2005/10/27 15:53:06  gouriano
 * Further enhancements of CStringUTF8
 *
 * Revision 1.97  2005/10/21 17:35:52  gouriano
 * Enhanced CStringUTF8
 *
 * Revision 1.96  2005/10/19 12:03:53  ivanov
 * Removed obsolete NStr::StringTo*() methods.
 * NStr::*ToString() -- added new radix base parameter instead of flags.
 *
 * Revision 1.95  2005/10/17 18:24:22  ivanov
 * Added fBinary flag to NStr::ENumToStringFlags enum
 *
 * Revision 1.94  2005/10/17 13:27:03  ivanov
 * Added fOctal and fHex flags to NStr::ENumToStringFlags enum.
 *
 * Revision 1.93  2005/10/03 14:10:10  gouriano
 * Corrected CStringUTF8 class
 *
 * Revision 1.92  2005/08/25 18:57:00  ivanov
 * Moved JavaScriptEncode() from CHTMLHelper:: to NStr::.
 * Changed \" processing.
 *
 * Revision 1.91  2005/08/04 11:08:43  ivanov
 * Revamp of NStr::StringTo*() functions
 *
 * Revision 1.90  2005/06/10 20:46:07  lavr
 * #include <corelib/ncbimisc.h> instead of <ctype.h>
 *
 * Revision 1.89  2005/06/06 15:26:30  lavr
 * Explicit (unsigned char) casts in ctype routines
 *
 * Revision 1.88  2005/06/03 16:20:47  lavr
 * Explicit (unsigned char) casts in ctype routines
 *
 * Revision 1.87  2005/05/27 13:54:30  lavr
 * Unprintable character from eSign's comment removed
 *
 * Revision 1.86  2005/05/18 15:23:02  shomrat
 * Added starting position to IsBlank
 *
 * Revision 1.85  2005/05/17 15:48:14  lavr
 * Correct the power-of-2 suffix conversion in StringToUInt8_DataSize()
 *
 * Revision 1.84  2005/05/17 15:42:14  lavr
 * Mention power-2 suffix conversion in StringToUInt8_DataSize()
 *
 * Revision 1.83  2005/05/12 19:59:06  vasilche
 * Fixed void return.
 *
 * Revision 1.82  2005/05/12 14:33:49  ivanov
 * Made obsolete "sign" methods Int[8]ToString() private -- use versions
 * with the flag parameter.
 *
 * Revision 1.81  2005/05/12 11:13:01  ivanov
 * Added NStr::*Int*ToString() version with flags parameter
 *
 * Revision 1.80  2005/05/04 15:56:01  ucko
 * Genericize PCase et al. to allow for C-string-based variants, mainly
 * useful in conjunction with CStaticArray{Map,Set}.
 *
 * Revision 1.79  2005/04/29 14:41:15  ivanov
 * + NStr::TokenizePattern()
 *
 * Revision 1.78  2005/03/16 15:28:30  ivanov
 * MatchesMask(): Added parameter for case sensitive/insensitive matching
 *
 * Revision 1.77  2005/02/23 15:34:18  gouriano
 * Type cast to get rid of compiler warnings
 *
 * Revision 1.76  2005/02/16 15:04:35  ssikorsk
 * Tweaked kEmptyStr with Linux GCC
 *
 * Revision 1.75  2005/01/05 16:54:50  ivanov
 * Added string version of NStr::MatchesMask()
 *
 * Revision 1.74  2004/12/28 21:19:20  grichenk
 * Static strings changed to char*
 *
 * Revision 1.73  2004/12/08 12:47:16  kuznets
 * +PNocase_Conditional (case sensitive/insensitive comparison for maps)
 *
 * Revision 1.72  2004/11/24 15:16:13  shomrat
 * + fWrap_FlatFile - perform flat-file specific line wrap
 *
 * Revision 1.71  2004/11/05 16:30:02  shomrat
 * Fixed implementation (inline) of Equal methods
 *
 * Revision 1.70  2004/10/15 12:00:52  ivanov
 * Renamed NStr::StringToUInt_DataSize -> NStr::StringToUInt8_DataSize.
 * Added doxygen @return statement to NStr::StringTo* comments.
 * Added additional arguments to NStr::StringTo[U]Int8 to select radix
 * (now it is not fixed with predefined values, and can be arbitrary)
 * and action on not permitted trailing symbols in the converting string.
 *
 * Revision 1.69  2004/10/13 13:05:38  ivanov
 * Some cosmetics
 *
 * Revision 1.68  2004/10/13 01:05:06  vakatov
 * NStr::strncasecmp() -- fixed bug in "hand-made" code
 *
 * Revision 1.67  2004/10/05 16:34:10  shomrat
 * in place TruncateSpaces changed to TruncateSpacesInPlace
 *
 * Revision 1.66  2004/10/05 16:12:58  shomrat
 * + in place TruncateSpaces
 *
 * Revision 1.65  2004/10/04 14:27:31  ucko
 * Treat all letters as lowercase for case-insensitive comparisons.
 *
 * Revision 1.64  2004/10/01 15:17:38  shomrat
 * Added test if string starts/ends with a specified char and test if string is blank
 *
 * Revision 1.63  2004/09/22 13:32:16  kononenk
 * "Diagnostic Message Filtering" functionality added.
 * Added function SetDiagFilter()
 * Added class CDiagCompileInfo and macro DIAG_COMPILE_INFO
 * Module, class and function attribute added to CNcbiDiag and CException
 * Parameters __FILE__ and __LINE in CNcbiDiag and CException changed to
 *  CDiagCompileInfo + fixes on derived classes and their usage
 * Macro NCBI_MODULE can be used to set default module name in cpp files
 *
 * Revision 1.62  2004/09/21 18:44:55  kuznets
 * SoftStringToUInt renamed StringToUInt_DataSize
 *
 * Revision 1.61  2004/09/21 18:23:32  kuznets
 * +NStr::SoftStringToUInt KB, MB converter
 *
 * Revision 1.60  2004/09/07 21:25:03  ucko
 * +<strings.h> on OSF/1, as it may be needed for str(n)casecmp's declaration.
 *
 * Revision 1.59  2004/08/19 13:02:17  dicuccio
 * Dropped unnecessary export specifier on exceptions
 *
 * Revision 1.58  2004/07/04 19:11:23  vakatov
 * Do not use "throw()" specification after constructors and assignment
 * operators of exception classes inherited from "std::exception" -- as it
 * causes ICC 8.0 generated code to abort in Release mode.
 *
 * Revision 1.57  2004/06/21 12:14:50  ivanov
 * Added additional parameter for all StringToXxx() function that specify
 * an action which will be performed on conversion error: to throw an
 * exception, or just to return zero.
 *
 * Revision 1.56  2004/05/26 20:46:35  ucko
 * Fix backwards logic in Equal{Case,Nocase}.
 *
 * Revision 1.55  2004/04/26 14:44:30  ucko
 * Move CParseTemplException from ncbiexpt.hpp, as it needs NStr.
 *
 * Revision 1.54  2004/03/11 22:56:51  gorelenk
 * Removed tabs.
 *
 * Revision 1.53  2004/03/11 18:48:34  gorelenk
 * Added (condionaly) Windows-specific declaration of class CNcbiEmptyString.
 *
 * Revision 1.52  2004/03/05 14:21:54  shomrat
 * added missing definitions
 *
 * Revision 1.51  2004/03/05 12:28:00  ivanov
 * Moved CDirEntry::MatchesMask() to NStr class.
 *
 * Revision 1.50  2004/03/04 20:45:21  shomrat
 * Added equality checks
 *
 * Revision 1.49  2004/03/04 13:38:39  kuznets
 * + set of ToString conversion functions taking outout string as a parameter,
 * not a return value (should give a performance advantage in some cases)
 *
 * Revision 1.48  2003/12/12 17:26:45  ucko
 * +FormatVarargs
 *
 * Revision 1.47  2003/12/01 20:45:25  ucko
 * Extend Join to handle vectors as well as lists.
 * Add ParseEscapes (inverse of PrintableString).
 *
 * Revision 1.46  2003/11/18 11:57:37  siyan
 * Changed so @addtogroup does not cross namespace boundary
 *
 * Revision 1.45  2003/11/03 14:51:55  siyan
 * Fixed a typo in StringToInt header
 *
 * Revision 1.44  2003/08/19 15:17:20  rsmith
 * Add NStr::SplitInTwo() function.
 *
 * Revision 1.43  2003/08/15 18:14:54  siyan
 * Added documentation.
 *
 * Revision 1.42  2003/05/22 20:08:00  gouriano
 * added UTF8 strings
 *
 * Revision 1.41  2003/05/14 21:51:54  ucko
 * Move FindNoCase out of line and reimplement it to avoid making
 * lowercase copies of both strings.
 *
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
