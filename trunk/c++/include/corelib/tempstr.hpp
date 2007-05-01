#ifndef CORELIB___TEMPSTR__HPP
#define CORELIB___TEMPSTR__HPP

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
 * Authors:  Mike DiCuccio, Eugene Vasilchenko, Aaron Ucko
 *
 * File Description:
 *
 *  CTempString -- Light-weight const string class that can wrap an std::string
 *                 or C-style char array.
 *
 */

#include <corelib/ncbitype.h>
#include <corelib/ncbiexpt.hpp>
#include <corelib/ncbimisc.hpp>
#include <corelib/ncbidbg.hpp>
#include <algorithm>


BEGIN_NCBI_SCOPE


/** @addtogroup String
 *
 * @{
 */

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
    typedef size_t          size_type;
    typedef const char*     const_iterator;
    static const size_type	npos = static_cast<size_type>(-1);
    /// @}

    CTempString(void);
    CTempString(const char* str);
    CTempString(const char* str, size_type len);
    /// Use CTempString(const char* str, size_type len) instead
    NCBI_DEPRECATED_CTOR(CTempString(const char* str, size_type pos, size_type len));

    CTempString(const string& str);
    /// Use CTempString(const char* str, size_type pos, size_type len) instead
    NCBI_DEPRECATED_CTOR(CTempString(const string& str, size_type length));
    CTempString(const string& str, size_type pos, size_type len);

    CTempString(const CTempString& str);
    CTempString(const CTempString& str, size_type pos);
    CTempString(const CTempString& str, size_type pos, size_type len);

    /// copy a substring into a string
    /// Somewhat similar to basic_string::assign()
    void Copy(string& dst, size_type pos, size_type length) const;

    /// @name std::basic_string<> compatibility interface
    /// @{
    
    /// Assign new values to the content of the a string
    CTempString& assign(const char* src_str, size_type len);
    CTempString& assign(const CTempString& src_str);
    CTempString& assign(const CTempString& src_str,
                        size_type          off, 
                        size_type          count);
    

    /// Return an iterator to the string's starting position
    const_iterator begin() const;

    /// Return an iterator to the string's ending position (one past the end of
    /// the represented sequence)
    const_iterator end() const;

    /// Return a pointer to the array represented.  As with
    /// std::basic_string<>, this is not guaranteed to be NULL terminated.
    const char* data(void)   const;

    /// Return the length of the represented array.
    size_type   length(void) const;

    /// Return the length of the represented array.
    size_type   size(void) const;


    /// Return true if the represented string is empty (i.e., the length is
    /// zero)
    bool        empty(void)  const;

    /// Clears the string
    void clear(void);

    /// Truncate the string at some specified position
    /// Note: basic_string<> supports additional erase() options that we
    /// do not provide here
    void erase(size_type pos = 0);

    /// Find the first instance of the entire matching string within the
    /// current string, beginning at an optional offset.
    size_type find(const CTempString& match,
                   size_type pos = 0) const;

    /// Find the first instance of a given character string within the
    /// current string, beginning at an optional offset.
    size_type find(char match, size_type pos = 0) const;

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
    void x_Init(const char* str, size_type str_len,
                size_type pos, size_type len);
    void x_Init(const char* str, size_type str_len,
                size_type pos);
    bool x_Equals(const_iterator it1, const_iterator it2) const;
    bool x_Less(const_iterator it1, const_iterator it2) const;
};


NCBI_XNCBI_EXPORT
CNcbiOstream& operator<<(CNcbiOstream& out, const CTempString& str);


/// Global operator== for string and CTempString
inline
bool operator==(const string& str1, const CTempString& str2)
{
    return str2 == str1;
}

/*
 * @}
 */


/////////////////////////////////////////////////////////////////////////////

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
void CTempString::clear(void)
{
    // clear() assures that m_String points to a NULL-terminated c-style
    // string as a fall-back.
    m_String = "";
    m_Length = 0;
}


inline
void CTempString::x_Init(const char* str, size_type str_len,
                         size_type pos)
{
    if ( pos >= str_len ) {
        clear();
    }
    else {
        m_String = str + pos;
        m_Length = str_len - pos;
    }
}


inline
void CTempString::x_Init(const char* str, size_type str_len,
                         size_type pos, size_type len)
{
    if ( pos >= str_len ) {
        clear();
    }
    else {
        m_String = str + pos;
        m_Length = min(len, str_len - pos);
    }
}


inline
CTempString::CTempString(void)
{
    clear();
}


inline
CTempString::CTempString(const char* str)
{
    if ( !str ) {
        clear();
        return;
    }
    m_String = str;
    m_Length = strlen(str);
}


/// Templatized initialization from a string literal.  This version is
/// optimized to deal specifically with constant-sized built-in arrays.
template<size_t Size>
inline
CTempString literal(const char (&str)[Size])
{
    return CTempString(str, Size-1);
}


inline
CTempString::CTempString(const char* str, size_type len)
    : m_String(str), m_Length(len)
{
}


inline
CTempString::CTempString(const char* str, size_type pos, size_type len)
    : m_String(str+pos), m_Length(len)
{
}


inline
CTempString::CTempString(const string& str)
    : m_String(str.data()), m_Length(str.size())
{
}


inline
CTempString::CTempString(const string& str, size_type len)
    : m_String(str.data()), m_Length(min(len, str.size()))
{
}


inline
CTempString::CTempString(const string& str, size_type pos, size_type len)
{
    x_Init(str.data(), str.size(), pos, len);
}


inline
CTempString::CTempString(const CTempString& str)
    : m_String(str.data()), m_Length(str.size())
{
}


inline
CTempString::CTempString(const CTempString& str, size_type pos)
{
    x_Init(str.data(), str.size(), pos);
}


inline
CTempString::CTempString(const CTempString& str, size_type pos, size_type len)
{
    x_Init(str.data(), str.size(), pos, len);
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
    }
    else {
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

        const_iterator match_begin = match.begin();
        const_iterator match_end   = match.end();
        for ( ;  it != end_it;  ++it) {
            bool found = false;
            for (const_iterator match_it = match_begin;
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
CTempString::size_type CTempString::find(char match, size_type pos) const
{
    if (pos + 1 > length()) {
        return npos;
    }

    for (size_type i = pos;  i < length();  ++i) {
        if (m_String[i] == match) {
            return i;
        }
    }

    return npos;
}

inline
CTempString& CTempString::assign(const char* src, size_type len)
{
    m_String = src;
    m_Length = len;
    return *this;
}


inline
CTempString& CTempString::assign(const CTempString& src_str)
{
    return *this = src_str;
}


inline
CTempString& CTempString::assign(const CTempString& src_str,
                                 size_type          off, 
                                 size_type          count)
{
    x_Init(src_str.data(), src_str.size(), off, count);
    return *this;
}


inline
CTempString CTempString::substr(size_type pos) const
{
    return CTempString(*this, pos);
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


END_NCBI_SCOPE

#endif  // CORELIB___TEMPSTR__HPP
