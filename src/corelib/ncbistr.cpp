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
 *   Government have not placed any restriction on its use or reproduction.
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
 *   Some helper functions
 *
 */

#include <corelib/ncbistd.hpp>
#include <corelib/ncbi_limits.h>
#include <memory>
#include <algorithm>
#include <ctype.h>
#include <errno.h>
#include <stdio.h>


BEGIN_NCBI_SCOPE


inline
std::string::size_type s_DiffPtr(const char* end, const char* start)
{
    if (end) {
        return end - start;
    }
    return 0;
}

const char *const kEmptyCStr = "";

const string* CNcbiEmptyString::m_Str = 0;
const string& CNcbiEmptyString::FirstGet(void) {
    static const string s_Str = "";
    m_Str = &s_Str;
    return s_Str;
}


int NStr::CompareCase(const string& str, SIZE_TYPE pos, SIZE_TYPE n,
                      const char* pattern)
{
    if (pos == NPOS  ||  !n  ||  str.length() <= pos) {
        return *pattern ? -1 : 0;
    }
    if ( !*pattern ) {
        return 1;
    }

    if (n == NPOS  ||  n > str.length() - pos) {
        n = str.length() - pos;
    }

    const char* s = str.data() + pos;
    while (n  &&  *pattern  &&  *s == *pattern) {
        s++;  pattern++;  n--;
    }

    if (n == 0) {
        return *pattern ? -1 : 0;
    }

    return *s - *pattern;
}


int NStr::CompareNocase(const string& str, SIZE_TYPE pos, SIZE_TYPE n,
                        const char* pattern)
{
    if (pos == NPOS  ||  !n  ||  str.length() <= pos) {
        return *pattern ? -1 : 0;
    }
    if ( !*pattern ) {
        return 1;
    }

    if (n == NPOS  ||  n > str.length() - pos) {
        n = str.length() - pos;
    }

    const char* s = str.data() + pos;
    while (n  &&  *pattern  &&  toupper(*s) == toupper(*pattern)) {
        s++;  pattern++;  n--;
    }

    if (n == 0) {
        return *pattern ? -1 : 0;
    }

    return toupper(*s) - toupper(*pattern);
}


int NStr::CompareCase(const string& str, SIZE_TYPE pos, SIZE_TYPE n,
                      const string& pattern)
{
    if (pos == NPOS  ||  !n  ||  str.length() <= pos) {
        return pattern.empty() ? 0 : -1;
    }
    if ( pattern.empty() ) {
        return 1;
    }

    if (n == NPOS  ||  n > str.length() - pos) {
        n = str.length() - pos;
    }

    SIZE_TYPE n_cmp = n;
    if (n_cmp > pattern.length()) {
        n_cmp = pattern.length();
    }
    const char* s = str.data() + pos;
    const char* p = pattern.data();
    while (n_cmp  &&  *s == *p) {
        s++;  p++;  n_cmp--;
    }

    if (n_cmp == 0) {
        if (n == pattern.length())
            return 0;
        return n > pattern.length() ? 1 : -1;
    }

    return *s - *p;
}


int NStr::CompareNocase(const string& str, SIZE_TYPE pos, SIZE_TYPE n,
                        const string& pattern)
{
    if (pos == NPOS  ||  !n  ||  str.length() <= pos) {
        return pattern.empty() ? 0 : -1;
    }
    if ( pattern.empty() ) {
        return 1;
    }

    if (n == NPOS  ||  n > str.length() - pos) {
        n = str.length() - pos;
    }

    SIZE_TYPE n_cmp = n;
    if (n_cmp > pattern.length()) {
        n_cmp = pattern.length();
    }
    const char* s = str.data() + pos;
    const char* p = pattern.data();
    while (n_cmp  &&  toupper(*s) == toupper(*p)) {
        s++;  p++;  n_cmp--;
    }

    if (n_cmp == 0) {
        if (n == pattern.length())
            return 0;
        return n > pattern.length() ? 1 : -1;
    }

    return toupper(*s) - toupper(*p);
}


char* NStr::ToLower(char* str)
{
    char* s;
    for (s = str;  *str;  str++) {
        *str = tolower(*str);
    }
    return s;
}


string& NStr::ToLower(string& str)
{
    NON_CONST_ITERATE (string, it, str) {
        *it = tolower(*it);
    }
    return str;
}


char* NStr::ToUpper(char* str)
{
    char* s;
    for (s = str;  *str;  str++) {
        *str = toupper(*str);
    }
    return s;
}


string& NStr::ToUpper(string& str)
{
    NON_CONST_ITERATE (string, it, str) {
        *it = toupper(*it);
    }
    return str;
}


int NStr::StringToNumeric(const string& str)
{
    if (str.empty()  ||  !isdigit(*str.begin())) {
        return -1;
    }
    errno = 0;
    char* endptr = 0;
    unsigned long value = strtoul(str.c_str(), &endptr, 10);
    if (errno  ||  !endptr  ||  value > (unsigned long) kMax_Int  ||
        *endptr != '\0'  ||  endptr == str.c_str()) {
        return -1;
    }
    return (int) value;
}

# define CHECK_ENDPTR(conv)                                           \
    if (check_endptr == eCheck_Need  &&  *endptr != '\0') {           \
        NCBI_THROW2(CStringException, eBadArgs,                       \
            "String cannot be converted to " conv " - trailing junk", \
            s_DiffPtr(endptr, str.c_str()));                          \
    }

int NStr::StringToInt(const string& str, int base /* = 10 */,
                      ECheckEndPtr check_endptr   /* = eCheck_Need */ )
{
    errno = 0;
    char* endptr = 0;
    long value = strtol(str.c_str(), &endptr, base);
    if (errno  ||  !endptr  ||  endptr == str.c_str()  ||
        value < kMin_Int || value > kMax_Int) {
        NCBI_THROW2(CStringException, eConvert,
                    "String cannot be converted to int",
                    s_DiffPtr(endptr, str.c_str()));
    }
    CHECK_ENDPTR("int");
    return (int) value;
}


unsigned int NStr::StringToUInt(const string& str, int base /* =10 */,
                                ECheckEndPtr check_endptr   /* =eCheck_Need */)
{
    errno = 0;
    char* endptr = 0;
    unsigned long value = strtoul(str.c_str(), &endptr, base);
    if (errno  ||  !endptr  ||  endptr == str.c_str()  ||  value > kMax_UInt) {
        NCBI_THROW2(CStringException, eConvert,
                    "String cannot be converted unsigned int",
                    s_DiffPtr(endptr, str.c_str()));
    }
    CHECK_ENDPTR("unsigned int");
    return (unsigned int) value;
}


long NStr::StringToLong(const string& str, int base /* = 10 */,
                        ECheckEndPtr check_endptr   /* = eCheck_Need */ )
{
    errno = 0;
    char* endptr = 0;
    long value = strtol(str.c_str(), &endptr, base);
    if (errno  ||  !endptr  ||  endptr == str.c_str()) {
        NCBI_THROW2(CStringException, eConvert,
                    "String cannot be converted to long",
                    s_DiffPtr(endptr, str.c_str()));
    }
    CHECK_ENDPTR("long");
    return value;
}


unsigned long NStr::StringToULong(const string& str, int base /*=10 */,
                                  ECheckEndPtr check_endptr   /*=eCheck_Need*/)
{
    errno = 0;
    char* endptr = 0;
    unsigned long value = strtoul(str.c_str(), &endptr, base);
    if (errno  ||  !endptr  ||  endptr == str.c_str()) {
        NCBI_THROW2(CStringException, eConvert,
                    "String cannot be converted to unsigned long",
                    s_DiffPtr(endptr, str.c_str()));
    }
    CHECK_ENDPTR("unsigned long");
    return value;
}


double NStr::StringToDouble(const string& str,
                            ECheckEndPtr check_endptr /* = eCheck_Need */ )
{
    errno = 0;
    char* endptr = 0;
    double value = strtod(str.c_str(), &endptr);
    if (errno  ||  !endptr  ||  endptr == str.c_str()) {
        NCBI_THROW2(CStringException, eConvert,
                    "String cannot be converted to double",
                    s_DiffPtr(endptr, str.c_str()));
    }
    if (*(endptr - 1) != '.'  &&  *endptr == '.')
        endptr++;
    CHECK_ENDPTR("double");
    return value;
}


string NStr::IntToString(long value, bool sign /* = false */ )
{
    char buffer[64];
    ::sprintf(buffer, sign ? "%+ld" : "%ld", value);
    return buffer;
}


string NStr::UIntToString(unsigned long value)
{
    char buffer[64];
    ::sprintf(buffer, "%lu", value);
    return buffer;
}


string NStr::Int8ToString(Int8 value, bool sign /* = false */ )
{
    const size_t kBufSize = (sizeof(value) * CHAR_BIT) / 3 + 2;
    char buffer[kBufSize];
    char* pos = buffer + kBufSize;

    if (value == 0) {
        *--pos = '0';
    }
    else {
        bool is_negative = value < 0;
        if ( is_negative )
            value = -value;

        do {
            *--pos = char('0' + (value % 10));
            value /= 10;
        } while ( value );

        if ( is_negative )
            *--pos = '-';
        else if ( sign )
            *--pos = '+';
    }

    return string(pos, buffer + kBufSize - pos);
}


Int8 NStr::StringToInt8(const string& str)
{
    bool sign = false;
    const char* pc = str.c_str();

    switch (*pc) {
    case '-':
        sign = true;
        /*FALLTHRU*/
    case '+':
        ++pc;
        /*FALLTHRU*/
    default:
        break;
    }

    Int8 n = 0;
    Int8 limdiv = kMax_I8 / 10;
    Int8 limoff = kMax_I8 % 10;

    do {
        if (!isdigit(*pc)) {
            NCBI_THROW2(CStringException, eConvert,
                        "String cannot be converted to Int8 - bad digit",
                        s_DiffPtr(pc, str.c_str()));
        }
        int delta = *pc - '0';
        n *= 10;

        // Overflow checking
        if (n > limdiv || (n == limdiv && delta > limoff)) {
            NCBI_THROW2(CStringException, eConvert,
                        "String cannot be converted Int8 - overflow",
                        s_DiffPtr(pc, str.c_str()));
        }

        n += delta;
    } while (*++pc);

    return sign ? -n : n;
}


Uint8 NStr::StringToUInt8(const string& str, int base /* = 10  */)
{
    const char* pc = str.c_str();

    if (*pc == '+')
        ++pc;

 
    Uint8 n = 0;
    Uint8 limdiv = kMax_UI8 / base;
    int   limoff = int(kMax_UI8 % base);

    do {
        // Do a sanity check for common radixes
        int ch = *pc; 
        if (base == 10  &&  !isdigit(ch)              ||
            base == 16  &&  !isxdigit(ch)             ||
            base == 8   &&  (ch < '0'  ||  ch > '7')  ||
            base == 2   &&  (ch < '0'  ||  ch > '1')) {

            NCBI_THROW2(CStringException, eConvert,
                        "String cannot be converted to UInt8 - bad digit",
                        s_DiffPtr(pc, str.c_str()));
        }

        int delta;  // corresponding numeric value of *pc
        if (isdigit(ch)) {
            delta = ch - '0';
        } else {
            ch = tolower(ch);
            // Got to be 'a' to 'f' because of previous sanity checks
            delta = ch - 'a' + 10;
        }
		
        n *= base;

        // Overflow checking
        if (n > limdiv  ||  (n == limdiv  &&  delta > limoff)) {
            NCBI_THROW2(CStringException, eConvert,
                        "String cannot be converted to UInt8 - overflow",
                        s_DiffPtr(pc, str.c_str()));
        }

        n += delta;
    } while (*++pc);

    return n;
}


string NStr::UInt8ToString(Uint8 value)
{
    const size_t kBufSize = (sizeof(value) * CHAR_BIT) / 3 + 2;
    char buffer[kBufSize];
    char* pos = buffer + kBufSize;
    if ( value == 0 ) {
        *--pos = '0';
    }
    else {
        do {
            *--pos = char('0' + (value % 10));
            value /= 10;
        } while ( value );
    }

    return string(pos, buffer + kBufSize - pos);
}


string NStr::DoubleToString(double value)
{
    char buffer[64];
    ::sprintf(buffer, "%g", value);
    return buffer;
}


// A maximal double precision used in the double to string conversion
#if defined(NCBI_OS_MSWIN)
const unsigned int kMaxDoublePrecision = 200;
#else
const unsigned int kMaxDoublePrecision = 308;
#endif
// A maximal size of a double value in a string form.
// Exponent size + sign + dot + ending '\0' + max.precision
const unsigned int kMaxDoubleStringSize = 308 + 3 + kMaxDoublePrecision;


string NStr::DoubleToString(double value, unsigned int precision)
{
    char buffer[kMaxDoubleStringSize];
    SIZE_TYPE n = DoubleToString(value, precision, buffer,
                                 kMaxDoubleStringSize);
    buffer[n] = '\0';
    return buffer;
}


SIZE_TYPE NStr::DoubleToString(double value, unsigned int precision,
                               char* buf, SIZE_TYPE buf_size)
{
    char buffer[kMaxDoubleStringSize];
    if (precision > kMaxDoublePrecision) {
        precision = kMaxDoublePrecision;
    }
    int n = ::sprintf(buffer, "%.*f", (int) precision, value);
    SIZE_TYPE n_copy = min((SIZE_TYPE) n, buf_size);
    memcpy(buf, buffer, n_copy);
    return n_copy;
}


string NStr::PtrToString(const void* value)
{
    char buffer[64];
    ::sprintf(buffer, "%p", value);
    return buffer;
}


const void* NStr::StringToPtr(const string& str)
{
    void *ptr = NULL;
    ::sscanf(str.c_str(), "%p", &ptr);
    return ptr;
}


static const string s_kTrueString  = "true";
static const string s_kFalseString = "false";
static const string s_kTString     = "t";
static const string s_kFString     = "f";
static const string s_kYesString   = "yes";
static const string s_kNoString    = "no";
static const string s_kYString     = "y";
static const string s_kNString     = "n";


const string& NStr::BoolToString(bool value)
{
    return value ? s_kTrueString : s_kFalseString;
}


bool NStr::StringToBool(const string& str)
{
    if ( AStrEquiv(str, s_kTrueString,  PNocase())  ||
         AStrEquiv(str, s_kTString,     PNocase())  ||
         AStrEquiv(str, s_kYesString,   PNocase())  ||
         AStrEquiv(str, s_kYString,     PNocase()) )
        return true;

    if ( AStrEquiv(str, s_kFalseString, PNocase())  ||
         AStrEquiv(str, s_kFString,     PNocase())  ||
         AStrEquiv(str, s_kNoString,    PNocase())  ||
         AStrEquiv(str, s_kNString,     PNocase()) )
        return false;

    NCBI_THROW2(CStringException, eConvert,
                "String cannot be converted to bool", 0);
}


SIZE_TYPE NStr::FindNoCase(const string& str, const string& pattern,
                           SIZE_TYPE start, SIZE_TYPE end, EOccurrence where)
{
    string    pat(pattern, 0, 1);
    SIZE_TYPE l = pattern.size();
    if (isupper(pat[0])) {
        pat += (char)tolower(pat[0]);
    } else if (islower(pat[0])) {
        pat += (char)toupper(pat[0]);
    }
    if (where == eFirst) {
        SIZE_TYPE pos = str.find_first_of(pat, start);
        while (pos != NPOS  &&  pos <= end
               &&  CompareNocase(str, pos, l, pattern) != 0) {
            pos = str.find_first_of(pat, pos + 1);
        }
        return pos > end ? NPOS : pos;
    } else { // eLast
        SIZE_TYPE pos = str.find_last_of(pat, end);
        while (pos != NPOS  &&  pos >= start
               &&  CompareNocase(str, pos, l, pattern) != 0) {
            pos = str.find_last_of(pat, pos - 1);
        }
        return pos < start ? NPOS : pos;
    }
}


string NStr::TruncateSpaces(const string& str, ETrunc where)
{
    SIZE_TYPE beg = 0;
    if (where == eTrunc_Begin  ||  where == eTrunc_Both) {
        while (beg < str.length()  &&  isspace(str[beg]))
            beg++;
        if (beg == str.length())
            return kEmptyStr;
    }
    SIZE_TYPE end = str.length() - 1;
    if (where == eTrunc_End  ||  where == eTrunc_Both) {
        while ( isspace(str[end]) )
            end--;
    }
    _ASSERT( beg <= end );
    return str.substr(beg, end - beg + 1);
}


string& NStr::Replace(const string& src,
                      const string& search, const string& replace,
                      string& dst, SIZE_TYPE start_pos, size_t max_replace)
{
    // source and destination should not be the same
    if (&src == &dst) {
        NCBI_THROW2(CStringException, eBadArgs,
                    "String replace called with source == destination", 0);
    }

    dst = src;

    if( start_pos + search.size() > src.size() ||
        search == replace)
        return dst;

    for(size_t count = 0; !(max_replace && count >= max_replace); count++) {
        start_pos = dst.find(search, start_pos);
        if(start_pos == NPOS)
            break;
        dst.replace(start_pos, search.size(), replace);
        start_pos += replace.size();
    }
    return dst;
}


string NStr::Replace(const string& src,
                     const string& search, const string& replace,
                     SIZE_TYPE start_pos, size_t max_replace)
{
    string dst;
    return Replace(src, search, replace, dst, start_pos, max_replace);
}


list<string>& NStr::Split(const string& str, const string& delim,
                          list<string>& arr, EMergeDelims merge)
{
    for (size_t pos = 0; ; ) {
        size_t prev_pos = (merge == eMergeDelims
                           ? str.find_first_not_of(delim, pos)
                           : pos);
        if (prev_pos == NPOS) {
            break;
        }
        pos = str.find_first_of(delim, prev_pos);
        if (pos == NPOS) {
            arr.push_back(str.substr(prev_pos));
            break;
        } else {
            arr.push_back(str.substr(prev_pos, pos - prev_pos));
            ++pos;
        }
    }
    return arr;
}


vector<string>& NStr::Tokenize(const string& str, const string& delim,
                               vector<string>& arr, EMergeDelims merge)
{
    if (delim.empty()) {
        arr.push_back(str);
        return arr;
    }

    size_t pos, prev_pos;

    // Count number of tokens to determine the array size
    size_t tokens = 0;

    for (pos = prev_pos = 0; pos < str.length(); ++pos) {
        char c = str[pos];
        size_t dpos = delim.find(c);
        if (dpos != string::npos) ++tokens;
    }

    arr.reserve(arr.size() + tokens + 1);

    // Tokenization
    for (pos = 0; ; ) {
        prev_pos = (merge == eMergeDelims ? str.find_first_not_of(delim, pos)
                    : pos);
        if (prev_pos == NPOS) {
            break;
        }
        pos = str.find_first_of(delim, prev_pos);
        if (pos == NPOS) {
            arr.push_back(str.substr(prev_pos));
            break;
        } else {
            arr.push_back(str.substr(prev_pos, pos - prev_pos));
            ++pos;
        }
    }

    return arr;
}


bool NStr::SplitInTwo(const string& str, const string& delim,
                      string& str1, string& str2)
{
    SIZE_TYPE delim_pos = str.find_first_of(delim);
    if (NPOS == delim_pos) {   // only one piece.
        str1 = str;
        str2 = kEmptyStr;
        return false;
    }
    str1 = str.substr(0, delim_pos);
    str2 = str.substr(delim_pos + 1); // skip only one delimiter character.

    return true;
}


string NStr::Join(const list<string>& arr, const string& delim)
{
    if (arr.empty()) {
        return kEmptyStr;
    }

    string                       result = arr.front();
    list<string>::const_iterator it     = arr.begin();
    SIZE_TYPE needed = result.size();
    while (++it != arr.end()) {
        needed += delim.size() + it->size();
    }
    result.reserve(needed);
    it = arr.begin();
    while (++it != arr.end()) {
        result += delim;
        result += *it;
    }
    return result;
}


string NStr::PrintableString(const string&      str,
                             NStr::ENewLineMode nl_mode)
{
    static const char s_Hex[] = "0123456789ABCDEF";
    ITERATE ( string, it, str ) {
        if ( !isprint(*it) || *it == '"' || *it == '\\' ) {
            // bad character - convert via CNcbiOstrstream
            CNcbiOstrstream out;
            // write first good characters in one chunk
            out.write(str.data(), it-str.begin());
            // convert all other characters one by one
            do {
                if ( isprint(*it) ) {
                    // escape '"' and '\\' anyway
                    if ( *it == '"' || *it == '\\' )
                        out.put('\\');
                    out.put(*it);
                }
                else if (*it == '\n') {
                    // newline needs special processing
                    if (nl_mode == eNewLine_Quote) {
                        out.write("\\n", 2);
                    }
                    else {
                        out.put('\n');
                    }
                } else {
                    // all other non-printable characters need to be escaped
                    out.put('\\');
                    if (*it == '\t') {
                        out.put('t');
                    } else if (*it == '\r') {
                        out.put('r');
                    } else if (*it == '\v') {
                        out.put('v');
                    } else {
                        // hex string for non-standard codes
                        out.put('x');
                        out.put(s_Hex[(unsigned char) *it >> 4]);
                        out.put(s_Hex[(unsigned char) *it & 15]);
                    }
                }
            } while (++it < it_end); // it_end is from ITERATE macro
            return CNcbiOstrstreamToString(out);
        }
    }
    // all characters are good - return orignal string
    return str;
}


// Determines the end of an HTML <...> tag, accounting for attributes
// and comments (the latter allowed only within <!...>).
static SIZE_TYPE s_EndOfTag(const string& str, SIZE_TYPE start)
{
    _ASSERT(start < str.size()  &&  str[start] == '<');
    bool comments_ok = (start + 1 < str.size()  &&  str[start + 1] == '!');
    for (SIZE_TYPE pos = start + 1;  pos < str.size();  ++pos) {
        switch (str[pos]) {
        case '>': // found the end
            return pos;

        case '\"': // start of "string"; advance to end
            pos = str.find('\"', pos + 1);
            if (pos == NPOS) {
                NCBI_THROW2(CStringException, eFormat,
                            "Unclosed string in HTML tag", start);
                // return pos;
            }
            break;

        case '-': // possible start of -- comment --; advance to end
            if (comments_ok  &&  pos + 1 < str.size()
                &&  str[pos + 1] == '-') {
                pos = str.find("--", pos + 2);
                if (pos == NPOS) {
                    NCBI_THROW2(CStringException, eFormat,
                                "Unclosed comment in HTML tag", start);
                    // return pos;
                } else {
                    ++pos;
                }
            }
        }
    }
    NCBI_THROW2(CStringException, eFormat, "Unclosed HTML tag", start);
    // return NPOS;
}


// Determines the end of an HTML &foo; character/entity reference
// (which might not actually end with a semicolon :-/)
static SIZE_TYPE s_EndOfReference(const string& str, SIZE_TYPE start)
{
    _ASSERT(start < str.size()  &&  str[start] == '&');
#ifdef NCBI_STRICT_HTML_REFS
    return str.find(';', start + 1);
#else
    SIZE_TYPE pos = str.find_first_not_of
        ("#0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz",
         start + 1);
    if (pos == NPOS  ||  str[pos] == ';') {
        return pos;
    } else {
        return pos - 1;
    }
#endif
}


static SIZE_TYPE s_VisibleWidth(const string& str, bool is_html)
{
    if (is_html) {
        SIZE_TYPE width = 0, pos = 0;
        for (;;) {
            SIZE_TYPE pos2 = str.find_first_of("<&", pos);
            if (pos2 == NPOS) {
                width += str.size() - pos;
                break;
            } else {
                width += pos2 - pos;
                if (str[pos2] == '&') {
                    ++width;
                    pos = s_EndOfReference(str, pos);
                } else {
                    pos = s_EndOfTag(str, pos);
                }
                if (pos == NPOS) {
                    break;
                } else {
                    ++pos;
                }
            }
        }
        return width;
    } else {
        return str.size();
    }
}


list<string>& NStr::Wrap(const string& str, SIZE_TYPE width,
                         list<string>& arr, NStr::TWrapFlags flags,
                         const string* prefix, const string* prefix1)
{
    if (prefix == 0) {
        prefix = &kEmptyStr;
    }

    const string* pfx = prefix1 ? prefix1 : prefix;
    SIZE_TYPE     pos = 0, len = str.size();
    string        hyphen; // "-" or empty
    bool          is_html  = flags & fWrap_HTMLPre ? true : false;

    enum EScore { // worst to best
        eForced,
        ePunct,
        eSpace,
        eNewline
    };

    while (pos < len) {
        SIZE_TYPE column     = s_VisibleWidth(*pfx, is_html);
        SIZE_TYPE column0    = column;
        // the next line will start at best_pos
        SIZE_TYPE best_pos   = NPOS;
        EScore    best_score = eForced;
        for (SIZE_TYPE pos2 = pos;  pos2 < len && column <= width;
             ++pos2, ++column) {
            EScore    score     = eForced;
            SIZE_TYPE score_pos = pos2;
            char      c         = str[pos2];

            if (c == '\n') {
                best_pos   = pos2;
                best_score = eNewline;
                break;
            } else if (isspace(c)) {
                if (pos2 > 0  &&  isspace(str[pos2 - 1])) {
                    continue; // take the first space of a group
                }
                score = eSpace;
            } else if (is_html  &&  c == '<') {
                // treat tags as zero-width...
                pos2 = s_EndOfTag(str, pos2);
                --column;
            } else if (is_html  &&  c == '&') {
                // ...and references as single characters
                pos2 = s_EndOfReference(str, pos2);
            } else if (ispunct(c)) {
                if (c == '('  ||  c == '['  ||  c == '{'  ||  c == '<'
                    ||  c == '`') { // opening element
                    score = ePunct;
                } else if (score_pos < len - 1) {
                    // Prefer breaking *after* most types of punctuation.
                    score = ePunct;
                    ++score_pos;
                }
            }

            if (score >= best_score) {
                best_pos   = score_pos;
                best_score = score;
            }

            while (pos < len - 1  &&  str[pos + 1] == '\b') {
                // Account for backspaces
                ++pos;
                if (column > column0) {
                    --column;
                }
            }
        }

        if (best_score != eNewline  &&  column <= width) {
            // If the whole remaining text can fit, don't split it...
            best_pos = len;
        } else if (best_score == eForced  &&  (flags & fWrap_Hyphenate)) {
            hyphen = "-";
            --best_pos;
        }
        arr.push_back(*pfx);
        {{ // eat backspaces and the characters (if any) that precede them
            string    line(str, pos, best_pos - pos);
            SIZE_TYPE bs = 0;
            while ((bs = line.find('\b', bs)) != NPOS) {
                if (bs > 0) {
                    line.erase(bs - 1, 2);
                } else {
                    line.erase(0, 1);
                }
            }
            arr.back() += line;
        }}
        arr.back() += hyphen;
        pos    = best_pos;
        pfx    = prefix;
        hyphen = kEmptyStr;

        if (best_score == eSpace) {
            // If breaking at a group of spaces, skip over the whole group
            while (pos < len  &&  isspace(str[pos])  &&  str[pos] != '\n') {
                ++pos;
            }
        } else if (best_score == eNewline) {
            ++pos;
        }
        while (pos < len  &&  str[pos] == '\b') {
            ++pos;
        }
    }

    return arr;
}


list<string>& NStr::WrapList(const list<string>& l, SIZE_TYPE width,
                             const string& delim, list<string>& arr,
                             NStr::TWrapFlags flags, const string* prefix,
                             const string* prefix1)
{
    if (l.empty()) {
        return arr;
    }

    const string* pfx      = prefix1 ? prefix1 : prefix;
    string        s        = *pfx;
    bool          is_html  = flags & fWrap_HTMLPre ? true : false;
    SIZE_TYPE     column   = s_VisibleWidth(s,     is_html);
    SIZE_TYPE     delwidth = s_VisibleWidth(delim, is_html);
    bool          at_start = true;
    ITERATE (list<string>, it, l) {
        SIZE_TYPE term_width = s_VisibleWidth(*it, is_html);
        if (at_start) {
            if (column + term_width <= width) {
                s += *it;
                column += term_width;
                at_start = false;
            } else {
                // Can't fit, even on its own line; break separately.
                Wrap(*it, width, arr, flags, prefix, pfx);
                pfx      = prefix;
                s        = *prefix;
                column   = s_VisibleWidth(s, is_html);
                at_start = true;
            }
        } else if (column + delwidth + term_width <= width) {
            s += delim;
            s += *it;
            column += delwidth + term_width;
            at_start = false;
        } else {
            // Can't fit on this line; break here and try again.
            arr.push_back(s);
            pfx      = prefix;
            s        = *prefix;
            column   = s_VisibleWidth(s, is_html);
            at_start = true;
            --it;
        }
    }

    arr.push_back(s);
    return arr;
}


#if !defined(HAVE_STRDUP)
extern char* strdup(const char* str)
{
    if ( !str )
        return 0;

    size_t size   = strlen(str) + 1;
    void*  result = malloc(size);
    return (char*) (result ? memcpy(result, str, size) : 0);
}
#endif


/////////////////////////////////////////////////////////////////////////////
//  CStringUTF8

void CStringUTF8::x_Append(const char* src)
{
    const char* srcBuf;
    size_t needed = 0;
    for (srcBuf = src; *srcBuf; ++srcBuf) {
        Uint1 ch = *srcBuf;
        if (ch < 0x80) {
            ++needed;
        } else {
            needed += 2;
        }
    }

    if ( !needed )
        return;

    reserve(length()+needed+1);
    for (srcBuf = src; *srcBuf; ++srcBuf) {
        Uint1 ch = *srcBuf;
        if (ch < 0x80) {
            append(1, ch);
        } else {
            append(1, Uint1((ch >> 6) | 0xC0));
            append(1, Uint1((ch & 0x3F) | 0x80));
        }
    }
}


#if defined(HAVE_WSTRING)
void CStringUTF8::x_Append(const wchar_t* src)
{
    const wchar_t* srcBuf;
    size_t needed = 0;
    for (srcBuf = src; *srcBuf; ++srcBuf) {
        Uint2 ch = *srcBuf;
        if (ch < 0x80) {
            ++needed;
        } else if (ch < 0x800) {
            needed += 2;
        } else {
            needed += 3;
        }
    }

    if ( !needed )
        return;

    reserve(length()+needed+1);
    for (srcBuf = src; *srcBuf; ++srcBuf) {
        Uint2 ch = *srcBuf;
        if (ch < 0x80) {
            append(1, ch);
        }
        else if (ch < 0x800) {
            append(1, Uint2((ch >> 6) | 0xC0));
            append(1, Uint2((ch & 0x3F) | 0x80));
        } else {
            append(1, Uint2((ch >> 12) | 0xE0));
            append(1, Uint2(((ch >> 6) & 0x3F) | 0x80));
            append(1, Uint2((ch & 0x3F) | 0x80));
        }
    }
}
#endif // HAVE_WSTRING


string CStringUTF8::AsAscii(void) const
{
    string result;
    const char* srcBuf;
    size_t needed = 0;
    bool bad = false;
    bool enough = true;
    for (srcBuf = c_str(); *srcBuf; ++srcBuf) {
        Uint1 ch = *srcBuf;
        if ((ch & 0x80) == 0) {
            ++needed;
        } else if ((ch & 0xE0) == 0xC0) {
            enough = (ch & 0x1F) <= 0x03;
            if (enough) {
                ++needed;
                ch = *(++srcBuf);
                bad = (ch & 0xC0) != 0x80;
            }
        } else if ((ch & 0xF0) == 0xE0) {
            enough = false;
        } else {
            bad = true;
        }
        if (!enough) {
            NCBI_THROW2(CStringException, eConvert,
                        "Cannot convert UTF8 string to single-byte string",
                        s_DiffPtr(srcBuf,c_str()));
        }
        if (bad) {
            NCBI_THROW2(CStringException, eFormat,
                        "Wrong UTF8 format",
                        s_DiffPtr(srcBuf,c_str()));
        }
    }
    result.reserve( needed+1);
    for (srcBuf = c_str(); *srcBuf; ++srcBuf) {
        Uint1 chRes;
        size_t more;
        Uint1 ch = *srcBuf;
        if ((ch & 0x80) == 0) {
            chRes = ch;
            more = 0;
        } else {
            chRes = (ch & 0x1F);
            more = 1;
        }
        while (more--) {
            ch = *(++srcBuf);
            chRes = (chRes << 6) | (ch & 0x3F);
        }
        result += chRes;
    }
    return result;
}


#if defined(HAVE_WSTRING)
wstring CStringUTF8::AsUnicode(void) const
{
    wstring result;
    const char* srcBuf;
    size_t needed = 0;
    bool bad = false;
    for (srcBuf = c_str(); *srcBuf; ++srcBuf) {
        Uint1 ch = *srcBuf;
        if ((ch & 0x80) == 0) {
            ++needed;
        } else if ((ch & 0xE0) == 0xC0) {
            ++needed;
            ch = *(++srcBuf);
            bad = (ch & 0xC0) != 0x80;
        } else if ((ch & 0xF0) == 0xE0) {
            ++needed;
            ch = *(++srcBuf);
            bad = (ch & 0xC0) != 0x80;
            if (!bad) {
                ch = *(++srcBuf);
                bad = (ch & 0xC0) != 0x80;
            }
        } else {
            bad = true;
        }
        if (bad) {
            NCBI_THROW2(CStringException, eFormat,
                        "Wrong UTF8 format",
                        s_DiffPtr(srcBuf,c_str()));
        }
    }
    result.reserve( needed+1);
    for (srcBuf = c_str(); *srcBuf; ++srcBuf) {
        Uint2 chRes;
        size_t more;
        Uint1 ch = *srcBuf;
        if ((ch & 0x80) == 0) {
            chRes = ch;
            more = 0;
        } else if ((ch & 0xE0) == 0xC0) {
            chRes = (ch & 0x1F);
            more = 1;
        } else {
            chRes = (ch & 0x0F);
            more = 2;
        }
        while (more--) {
            ch = *(++srcBuf);
            chRes = (chRes << 6) | (ch & 0x3F);
        }
        result += chRes;
    }
    return result;
}
#endif // HAVE_WSTRING


END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.98  2003/10/31 13:15:20  lavr
 * Fix typos in the log of the previous commit :-)
 *
 * Revision 1.97  2003/10/31 12:59:46  lavr
 * Better diagnostics messages from exceptions; some other cosmetic changes
 *
 * Revision 1.96  2003/10/03 15:16:02  ucko
 * NStr::Join: preallocate as much space as we need for result.
 *
 * Revision 1.95  2003/09/17 15:18:29  vasilche
 * Reduce memory allocations in NStr::PrintableString()
 *
 * Revision 1.94  2003/08/19 15:17:20  rsmith
 * Add NStr::SplitInTwo() function.
 *
 * Revision 1.93  2003/06/16 15:19:03  ucko
 * FindNoCase: always honor both start and end (oops).
 *
 * Revision 1.92  2003/05/22 20:09:29  gouriano
 * added UTF8 strings
 *
 * Revision 1.91  2003/05/14 21:52:09  ucko
 * Move FindNoCase out of line and reimplement it to avoid making
 * lowercase copies of both strings.
 *
 * Revision 1.90  2003/03/25 22:15:40  lavr
 * NStr::PrintableString():: Print NUL char as \x00 instead of \0
 *
 * Revision 1.89  2003/03/20 13:27:52  dicuccio
 * Oops.  Removed old code wrapped in #if 0...#endif.
 *
 * Revision 1.88  2003/03/20 13:27:11  dicuccio
 * Changed NStr::StringToPtr() - now symmetric with NSrt::PtrToString (there were
 * too many special cases).
 *
 * Revision 1.87  2003/03/17 12:49:26  dicuccio
 * Fixed indexing error in NStr::PtrToString() - buffer is 0-based index, not
 * 1-based
 *
 * Revision 1.86  2003/03/11 16:57:12  ucko
 * Process backspaces in NStr::Wrap, allowing in particular the use of
 * " \b" ("-\b") to indicate mid-word break (hyphenation) points.
 *
 * Revision 1.85  2003/03/10 18:57:08  kuznets
 * iterate->ITERATE
 *
 * Revision 1.84  2003/03/04 00:02:21  vakatov
 * NStr::PtrToString() -- use runtime check for "0x"
 * NStr::StringToPtr() -- minor polishing
 *
 * Revision 1.83  2003/02/27 15:34:01  lavr
 * Bugfix in converting string to double [spurious dots], some reformatting
 *
 * Revision 1.82  2003/02/26 21:07:52  siyan
 * Remove const for base parameter for StringToUInt8
 *
 * Revision 1.81  2003/02/26 20:34:11  siyan
 * Added/deleted whitespaces to conform to existing coding style
 *
 * Revision 1.80  2003/02/26 16:45:53  siyan
 * Reimplemented NStr::StringToUInt8 to support additional base parameters
 * that can take radix values such as 10(default), 16, 8, 2.
 * Reimplemented StringToPtr to support 64 bit addresses.
 *
 * Revision 1.79  2003/02/25 19:14:53  kuznets
 * NStr::StringToBool changed to understand YES/NO
 *
 * Revision 1.78  2003/02/25 15:43:40  dicuccio
 * Added #ifdef'd hack for MSVC's non-standard sprintf() in PtrToString(),
 * '%p' lacks a leading '0x'
 *
 * Revision 1.77  2003/02/25 14:43:53  dicuccio
 * Added handling of special NULL pointer encoding in StringToPtr()
 *
 * Revision 1.76  2003/02/24 20:25:59  gouriano
 * use template-based errno and parse exceptions
 *
 * Revision 1.75  2003/02/21 21:20:01  vakatov
 * Fixed some types, did some casts to avoid compilation warnings in 64-bit
 *
 * Revision 1.74  2003/02/20 18:41:28  dicuccio
 * Added NStr::StringToPtr()
 *
 * Revision 1.73  2003/02/11 22:11:03  ucko
 * Make NStr::WrapList a no-op if the input list is empty.
 *
 * Revision 1.72  2003/02/06 21:31:35  ucko
 * Fixed an off-by-one error in NStr::Wrap.
 *
 * Revision 1.71  2003/02/04 21:54:12  ucko
 * NStr::Wrap: when breaking on punctuation, try to position the break
 * *after* everything but opening delimiters.
 *
 * Revision 1.70  2003/01/31 03:39:11  lavr
 * Heed int->bool performance warning
 *
 * Revision 1.69  2003/01/27 20:06:59  ivanov
 * Get rid of compilation warnings in StringToUInt8() and DoubleToString()
 *
 * Revision 1.68  2003/01/24 16:59:27  ucko
 * Add an optional parameter to Split and Tokenize indicating whether to
 * merge adjacent delimiters; clean up WrapList slightly.
 *
 * Revision 1.67  2003/01/21 23:22:22  vakatov
 * NStr::Tokenize() to return reference, and not a new "vector<>".
 *
 * Revision 1.66  2003/01/21 20:08:01  ivanov
 * Added function NStr::DoubleToString(value, precision, buf, buf_size)
 *
 * Revision 1.65  2003/01/14 22:13:56  kuznets
 * Overflow check reimplemented for NStr::StringToInt
 *
 * Revision 1.64  2003/01/14 21:16:46  kuznets
 * +Nstr::Tokenize
 *
 * Revision 1.63  2003/01/13 14:47:16  kuznets
 * Implemented overflow checking for StringToInt8 function
 *
 * Revision 1.62  2003/01/10 22:17:06  kuznets
 * Implemented NStr::String2Int8
 *
 * Revision 1.61  2003/01/10 16:49:54  kuznets
 * Cosmetics
 *
 * Revision 1.60  2003/01/10 15:27:12  kuznets
 * Eliminated int -> bool performance warning
 *
 * Revision 1.59  2003/01/10 00:08:17  vakatov
 * + Int8ToString(),  UInt8ToString()
 *
 * Revision 1.58  2003/01/06 16:42:45  ivanov
 * + DoubleToString() with 'precision'
 *
 * Revision 1.57  2002/10/18 20:48:56  lavr
 * +ENewLine_Mode and '\n' translation in NStr::PrintableString()
 *
 * Revision 1.56  2002/10/17 14:41:20  ucko
 * * Make s_EndOf{Tag,Reference} actually static (oops).
 * * Pull width-determination code from WrapList into a separate function
 *   (s_VisibleWidth) and make Wrap{,List} call it for everything rather
 *   than assuming prefixes and delimiters to be plain text.
 * * Add a column variable to WrapList, as it may not equal s.size().
 *
 * Revision 1.55  2002/10/16 19:30:36  ucko
 * Add support for wrapping HTML <PRE> blocks.  (Not yet tested, but
 * behavior without fWrap_HTMLPre should stay the same.)
 *
 * Revision 1.54  2002/10/11 19:41:48  ucko
 * Clean up NStr::Wrap a bit more, doing away with the "limit" variables
 * for ease of potential extension.
 *
 * Revision 1.53  2002/10/03 14:44:35  ucko
 * Tweak the interfaces to NStr::Wrap* to avoid publicly depending on
 * kEmptyStr, removing the need for fWrap_UsePrefix1 in the process; also
 * drop fWrap_FavorPunct, as WrapList should be a better choice for such
 * situations.
 *
 * Revision 1.52  2002/10/02 20:15:09  ucko
 * Add Join, Wrap, and WrapList functions to NStr::.
 *
 * Revision 1.51  2002/09/04 15:16:57  lavr
 * Backslashed double quote (\") in PrintableString()
 *
 * Revision 1.50  2002/07/15 18:17:25  gouriano
 * renamed CNcbiException and its descendents
 *
 * Revision 1.49  2002/07/11 14:18:28  gouriano
 * exceptions replaced by CNcbiException-type ones
 *
 * Revision 1.48  2002/05/02 15:25:37  ivanov
 * Added new parameter to String-to-X functions for skipping the check
 * the end of string on zero
 *
 * Revision 1.47  2002/04/11 21:08:03  ivanov
 * CVS log moved to end of the file
 *
 * Revision 1.46  2002/02/22 17:50:52  ivanov
 * Added compatible compare functions strcmp, strncmp, strcasecmp, strncasecmp.
 * Was speed-up some Compare..() functions.
 *
 * Revision 1.45  2001/08/30 00:36:45  vakatov
 * + NStr::StringToNumeric()
 * Also, well-groomed the code and get rid of some compilation warnings.
 *
 * Revision 1.44  2001/05/30 15:56:25  vakatov
 * NStr::CompareNocase, NStr::CompareCase -- get rid of the possible
 * compilation warning (ICC compiler:  "return statement missing").
 *
 * Revision 1.43  2001/05/17 15:04:59  lavr
 * Typos corrected
 *
 * Revision 1.42  2001/04/12 21:39:44  vakatov
 * NStr::Replace() -- check against source and dest. strings being the same
 *
 * Revision 1.41  2001/04/11 20:15:29  vakatov
 * NStr::PrintableString() -- cast "char" to "unsigned char".
 *
 * Revision 1.40  2001/03/16 19:38:35  grichenk
 * Added NStr::Split()
 *
 * Revision 1.39  2001/01/03 17:45:35  vakatov
 * + <ncbi_limits.h>
 *
 * Revision 1.38  2000/12/15 15:36:41  vasilche
 * Added header corelib/ncbistr.hpp for all string utility functions.
 * Optimized string utility functions.
 * Added assignment operator to CRef<> and CConstRef<>.
 * Add Upcase() and Locase() methods for automatic conversion.
 *
 * Revision 1.37  2000/12/12 14:20:36  vasilche
 * Added operator bool to CArgValue.
 * Various NStr::Compare() methods made faster.
 * Added class Upcase for printing strings to ostream with automatic conversion
 *
 * Revision 1.36  2000/12/11 20:42:50  vakatov
 * + NStr::PrintableString()
 *
 * Revision 1.35  2000/11/16 23:52:41  vakatov
 * Porting to Mac...
 *
 * Revision 1.34  2000/11/07 04:06:08  vakatov
 * kEmptyCStr (equiv. to NcbiEmptyCStr)
 *
 * Revision 1.33  2000/10/11 21:03:49  vakatov
 * Cleanup to avoid 64-bit to 32-bit values truncation, etc.
 * (reported by Forte6 Patch 109490-01)
 *
 * Revision 1.32  2000/08/03 20:21:29  golikov
 * Added predicate PCase for AStrEquiv
 * PNocase, PCase goes through NStr::Compare now
 *
 * Revision 1.31  2000/07/19 19:03:55  vakatov
 * StringToBool() -- short and case-insensitive versions of "true"/"false"
 * ToUpper/ToLower(string&) -- fixed
 *
 * Revision 1.30  2000/06/01 19:05:40  vasilche
 * NStr::StringToInt now reports errors for tailing symbols in release version
 * too
 *
 * Revision 1.29  2000/05/01 19:02:25  vasilche
 * Force argument in NStr::StringToInt() etc to be full number.
 * This check will be in DEBUG version for month.
 *
 * Revision 1.28  2000/04/19 18:36:04  vakatov
 * Fixed for non-zero "pos" in s_Compare()
 *
 * Revision 1.27  2000/04/17 04:15:08  vakatov
 * NStr::  extended Compare(), and allow case-insensitive string comparison
 * NStr::  added ToLower() and ToUpper()
 *
 * Revision 1.26  2000/04/04 22:28:09  vakatov
 * NStr::  added conversions for "long"
 *
 * Revision 1.25  2000/02/01 16:48:09  vakatov
 * CNcbiEmptyString::  more dancing around the Sun "feature" (see also R1.24)
 *
 * Revision 1.24  2000/01/20 16:24:42  vakatov
 * Kludging around the "NcbiEmptyString" to ensure its initialization when
 * it is used by the constructor of a statically allocated object
 * (I believe that it is actually just another Sun WorkShop compiler "feature")
 *
 * Revision 1.23  1999/12/28 18:55:43  vasilche
 * Reduced size of compiled object files:
 * 1. avoid inline or implicit virtual methods (especially destructors).
 * 2. avoid std::string's methods usage in inline methods.
 * 3. avoid string literals ("xxx") in inline methods.
 *
 * Revision 1.22  1999/12/17 19:04:09  vasilche
 * NcbiEmptyString made extern.
 *
 * Revision 1.21  1999/11/26 19:29:09  golikov
 * fix
 *
 * Revision 1.20  1999/11/26 18:45:17  golikov
 * NStr::Replace added
 *
 * Revision 1.19  1999/11/17 22:05:04  vakatov
 * [!HAVE_STRDUP]  Emulate "strdup()" -- it's missing on some platforms
 *
 * Revision 1.18  1999/10/13 16:30:30  vasilche
 * Fixed bug in PNocase which appears under GCC implementation of STL.
 *
 * Revision 1.17  1999/07/08 16:10:14  vakatov
 * Fixed a warning in NStr::StringToUInt()
 *
 * Revision 1.16  1999/07/06 15:21:06  vakatov
 * + NStr::TruncateSpaces(const string& str, ETrunc where=eTrunc_Both)
 *
 * Revision 1.15  1999/06/15 20:50:05  vakatov
 * NStr::  +BoolToString, +StringToBool
 *
 * Revision 1.14  1999/05/27 15:21:40  vakatov
 * Fixed all StringToXXX() functions
 *
 * Revision 1.13  1999/05/17 20:10:36  vasilche
 * Fixed bug in NStr::StringToUInt which cause an exception.
 *
 * Revision 1.12  1999/05/04 00:03:13  vakatov
 * Removed the redundant severity arg from macro ERR_POST()
 *
 * Revision 1.11  1999/04/22 14:19:04  vasilche
 * Added _TRACE_THROW() macro, which can be configured to produce coredump
 * at a point of throwing an exception.
 *
 * Revision 1.10  1999/04/14 21:20:33  vakatov
 * Dont use "snprintf()" as it is not quite portable yet
 *
 * Revision 1.9  1999/04/14 19:57:36  vakatov
 * Use limits from <ncbitype.h> rather than from <limits>.
 * [MSVC++]  fix for "snprintf()" in <ncbistd.hpp>.
 *
 * Revision 1.8  1999/04/09 19:51:37  sandomir
 * minor changes in NStr::StringToXXX - base added
 *
 * Revision 1.7  1999/01/21 16:18:04  sandomir
 * minor changes due to NStr namespace to contain string utility functions
 *
 * Revision 1.6  1999/01/11 22:05:50  vasilche
 * Fixed CHTML_font size.
 * Added CHTML_image input element.
 *
 * Revision 1.5  1998/12/28 17:56:39  vakatov
 * New CVS and development tree structure for the NCBI C++ projects
 *
 * Revision 1.4  1998/12/21 17:19:37  sandomir
 * VC++ fixes in ncbistd; minor fixes in Resource
 *
 * Revision 1.3  1998/12/17 21:50:45  sandomir
 * CNCBINode fixed in Resource; case insensitive string comparison predicate
 * added
 *
 * Revision 1.2  1998/12/15 17:36:33  vasilche
 * Fixed "double colon" bug in multithreaded version of headers.
 *
 * Revision 1.1  1998/12/15 15:43:22  vasilche
 * Added utilities to convert string <> int.
 * ===========================================================================
 */
