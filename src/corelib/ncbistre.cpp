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
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbistre.hpp>
#include <corelib/stream_utils.hpp>
#if defined(NCBI_OS_UNIX)
#  include <unistd.h>
#endif


BEGIN_NCBI_SCOPE


Int8 NcbiStreamposToInt8(CT_POS_TYPE stream_pos)
{
#ifdef NCBI_OS_MSWIN
    fpos_t fp(stream_pos.seekpos());
    return (Int8) fp;
#else
    return (CT_OFF_TYPE)(stream_pos - CT_POS_TYPE(0));
#endif
}


CT_POS_TYPE NcbiInt8ToStreampos(Int8 pos)
{
#ifdef NCBI_OS_MSWIN
    fpos_t fp(pos);
    mbstate_t mbs;
    memset (&mbs, '\0', sizeof (mbs));
    CT_POS_TYPE p(mbs, fp);
    return p;
#else
    return CT_POS_TYPE(0) + CT_OFF_TYPE(pos);
#endif
}


CNcbiIstream& NcbiGetline(CNcbiIstream& is, string& str, const string& delims)
{
    CT_INT_TYPE ch;
    char        buf[1024];
    SIZE_TYPE   pos = 0;

    str.erase();

    IOS_BASE::fmtflags f = is.flags();
    is.unsetf(IOS_BASE::skipws);
#ifdef NO_PUBSYNC
    if ( !is.ipfx(1) ) {
        is.flags(f);
        return is;
    }
#else
    CNcbiIstream::sentry s(is);
    if ( !s ) {
        is.clear(NcbiFailbit | is.rdstate());
        is.flags(f);
        return is;
    }
#endif

    SIZE_TYPE end = str.max_size();
    SIZE_TYPE i = 0;
    for (ch = is.rdbuf()->sbumpc();  !CT_EQ_INT_TYPE(ch, CT_EOF);
         ch = is.rdbuf()->sbumpc()) {
        i++;
        SIZE_TYPE delim_pos = delims.find(CT_TO_CHAR_TYPE(ch));
        if (delim_pos != NPOS) {
            // Special case -- if two different delimiters are back to
            // back and in the same order as in delims, treat them as
            // a single delimiter (necessary for correct handling of
            // DOS-style CRLF endings).
            CT_INT_TYPE next = is.rdbuf()->sgetc();
            if ( !CT_EQ_INT_TYPE(next, CT_EOF)
                &&  delims.find(CT_TO_CHAR_TYPE(next), delim_pos + 1) != NPOS) {
                is.rdbuf()->sbumpc();
            }
            break;
        }
        if (i == end) {
            is.clear(NcbiFailbit | is.rdstate());      
            break;
        }

        buf[pos++] = CT_TO_CHAR_TYPE(ch);
        if (pos == sizeof(buf)) {
            str.append(buf, pos);
            pos = 0;
        }
    }
    str.append(buf, pos);
    if (ch == EOF) 
        is.clear(NcbiEofbit | is.rdstate());      
    if ( !i )
        is.clear(NcbiFailbit | is.rdstate());      

#ifdef NO_PUBSYNC
    is.isfx();
#endif

    is.flags(f);
    return is;
}


#ifdef NCBI_COMPILER_GCC
#  if NCBI_COMPILER_VERSION < 300
#    define NCBI_COMPILER_GCC29x
#  endif
#endif

extern CNcbiIstream& NcbiGetline(CNcbiIstream& is, string& str, char delim)
{
#if defined(NCBI_USE_OLD_IOSTREAM)
    return NcbiGetline(is, str, string(1, delim));
#elif defined(NCBI_COMPILER_GCC29x)
    // The code below is normally somewhat faster than this call,
    // which typically appends one character at a time to str;
    // however, it blows up when built with some GCC versions.
    return getline(is, str, delim);
#else
    char buf[1024];
    str.erase();
    while (is.good()) {
        CT_INT_TYPE nextc = is.get();
        if (CT_EQ_INT_TYPE(nextc, CT_EOF) 
            ||  CT_EQ_INT_TYPE(nextc, CT_TO_INT_TYPE(delim))) {
            break;
        }
        is.putback(nextc);
        is.get(buf, sizeof(buf), delim);
        str.append(buf, is.gcount());
    }
    if ( str.empty()  &&  is.eof() ) {
        is.setstate(NcbiFailbit);
    }
    return is;
#endif
}


// Platform-specific EndOfLine
const char* Endl(void)
{
#if   defined(NCBI_OS_MAC)
    static const char s_Endl[] = "\r";
#elif defined(NCBI_OS_MSWIN)
    static const char s_Endl[] = "\r\n";
#else /* assume UNIX-like EOLs */
    static const char s_Endl[] = "\n";
#endif
    return s_Endl;
}


// Get the next line taking into account platform specifics of End-of-Line
CNcbiIstream& NcbiGetlineEOL(CNcbiIstream& is, string& str)
{
#if   defined(NCBI_OS_MAC)
    NcbiGetline(is, str, '\r');
#elif defined(NCBI_OS_MSWIN)
    NcbiGetline(is, str, '\n');
    if (!str.empty()  &&  str[str.length()-1] == '\r')
        str.resize(str.length() - 1);
#elif defined(NCBI_OS_DARWIN)
    NcbiGetline(is, str, "\r\n");
#else /* assume UNIX-like EOLs */
    NcbiGetline(is, str, '\n');
#endif
    // special case -- an empty line
    if (is.fail()  &&  !is.eof()  &&  !is.gcount()  &&  str.empty())
        is.clear(is.rdstate() & ~NcbiFailbit);
    return is;
}


bool NcbiStreamCopy(CNcbiOstream& os, CNcbiIstream& is)
{
    if (!is  ||  !(os << is.rdbuf()))
        return false;
    os.flush();
    if (!os.good())
        return false;
    if (!CT_EQ_INT_TYPE(is.peek(), CT_EOF)) {
        os.clear(IOS_BASE::failbit);
        return false;
    }
    return true;
}


CNcbiOstrstreamToString::operator string(void) const
{
    SIZE_TYPE length = m_Out.pcount();
    if ( length == 0 )
        return string();
    const char* str = m_Out.str();
    m_Out.freeze(false);
    return string(str, length);
}


CNcbiOstream& operator<<(CNcbiOstream& out, const CTempString& str)
{
    return out.write(str.data(), str.length());
}


CNcbiOstream& operator<<(CNcbiOstream& out, CUpcaseStringConverter s)
{
    ITERATE ( string, c, s.m_String ) {
        out.put(char(toupper((unsigned char)(*c))));
    }
    return out;
}


CNcbiOstream& operator<<(CNcbiOstream& out, CLocaseStringConverter s)
{
    ITERATE ( string, c, s.m_String ) {
        out.put(char(tolower((unsigned char)(*c))));
    }
    return out;
}


CNcbiOstream& operator<<(CNcbiOstream& out, CUpcaseCharPtrConverter s)
{
    for ( const char* c = s.m_String; *c; ++c ) {
        out.put(char(toupper((unsigned char)(*c))));
    }
    return out;
}


CNcbiOstream& operator<<(CNcbiOstream& out, CLocaseCharPtrConverter s)
{
    for ( const char* c = s.m_String; *c; ++c ) {
        out.put(char(tolower((unsigned char)(*c))));
    }
    return out;
}


#ifdef NCBI_COMPILER_MSVC
#  if _MSC_VER >= 1200  &&  _MSC_VER < 1300
CNcbiOstream& operator<<(CNcbiOstream& out, __int64 val)
{
    return (out << NStr::Int8ToString(val));
}
#  endif
#endif


static const char s_Hex[] = "0123456789ABCDEF";

string Printable(char c)
{
    string s;
    switch ( c ) {
    case '\0':  s = "\\0";   break;
    case '\\':  s = "\\\\";  break;
    case '\n':  s = "\\n";   break;
    case '\t':  s = "\\t";   break;
    case '\r':  s = "\\r";   break;
    case '\v':  s = "\\v";   break;
    default:
        {
            if ( isprint((unsigned char) c) ) {
                s = c;
            } else {
                s = "\\x";
                s += s_Hex[(unsigned char) c / 16];
                s += s_Hex[(unsigned char) c % 16];
            }
        }
    }
    return s;
}


inline
void WritePrintable(CNcbiOstream& out, char c)
{
    switch ( c ) {
    case '\0':  out.write("\\0",  2);  break;
    case '\\':  out.write("\\\\", 2);  break;
    case '\n':  out.write("\\n",  2);  break;
    case '\t':  out.write("\\t",  2);  break;
    case '\r':  out.write("\\r",  2);  break;
    case '\v':  out.write("\\v",  2);  break;
    default:
        if ( isprint((unsigned char) c) ) {
            out.put(c);
        } else {
            out.write("\\x", 2);
            out.put(s_Hex[(unsigned char) c / 16]);
            out.put(s_Hex[(unsigned char) c % 16]);
        }
        break;
    }
}


CNcbiOstream& operator<<(CNcbiOstream& out, CPrintableStringConverter s)
{
    ITERATE ( string, c, s.m_String ) {
        WritePrintable(out, *c);
    }
    return out;
}


CNcbiOstream& operator<<(CNcbiOstream& out, CPrintableCharPtrConverter s)
{
    for ( const char* c = s.m_String; *c; ++c ) {
        WritePrintable(out, *c);
    }
    return out;
}


#if defined(NCBI_COMPILER_WORKSHOP)
// We have to use two #if's here because KAI C++ cannot handle #if foo == bar
#  if (NCBI_COMPILER_VERSION == 530)
// The version that ships with the compiler is buggy.
// Here's a working (and simpler!) one.
template<>
istream& istream::read(char *s, streamsize n)
{
    sentry ipfx(*this, 1);

    try {
        if (rdbuf()->sgetc() == traits_type::eof()) {
            // Workaround for bug in sgetn.  *SIGH*.
            __chcount = 0;
            setstate(eofbit);
            return *this;
        }
        __chcount = rdbuf()->sgetn(s, n);
        if (__chcount == 0) {
            setstate(eofbit);
        } else if (__chcount < n) {
            setstate(eofbit | failbit);
        } else if (!ipfx) {
            setstate(failbit);
        } 
    } catch (...) {
        setstate(failbit);
        throw;
    }

    return *this;
}
#  endif  /* NCBI_COMPILER_VERSION == 530 */
#endif  /* NCBI_COMPILER_WORKSHOP */


EEncodingForm ReadIntoUtf8(
    CNcbiIstream&     input,
    CStringUTF8*      result,
    EEncodingForm     ef             /*  = eEncodingForm_Unknown*/,
    EReadUnknownNoBOM what_if_no_bom /* = eNoBOM_GuessEncoding*/
)
{
    EEncodingForm ef_bom = eEncodingForm_Unknown;
    result->erase();
    if (!input.good()) {
        return ef_bom;
    }

    const int buf_size = 4096;//2048;//256;
    char tmp[buf_size+2];
    Uint2* us = reinterpret_cast<Uint2*>(tmp);

// check for Byte Order Mark
    const int bom_max = 4;
    memset(tmp,0,bom_max);
    input.read(tmp,bom_max);
    int n = input.gcount();
    {
        int bom_len=0;
        Uchar* uc = reinterpret_cast<Uchar*>(tmp);
        if (n >= 3 && uc[0] == 0xEF && uc[1] == 0xBB && uc[2] == 0xBF) {
            ef_bom = eEncodingForm_Utf8;
            uc[0] = uc[3];
            bom_len=3;
        }
        else if (n >= 2 && (us[0] == 0xFEFF || us[0] == 0xFFFE)) {
            if (us[0] == 0xFEFF) {
                ef_bom = eEncodingForm_Utf16Native;
            } else {
                ef_bom = eEncodingForm_Utf16Foreign;
            }
            us[0] = us[1];
            bom_len=2;
        }
        if (ef == eEncodingForm_Unknown || ef == ef_bom) {
            ef = ef_bom;
            n -= bom_len;
        }
        // else proceed at user's risk
    }

// keep reading
    while (n != 0  ||  (input.good()  &&  !input.eof())) {

        if (n == 0) {
            input.read(tmp, buf_size);
            n = input.gcount();
            result->reserve(max(result->capacity(), result->size() + n));
        }
        tmp[n] = '\0';

        switch (ef) {
        case eEncodingForm_Utf16Foreign:
            {
                char buf[buf_size];
                swab(tmp,buf,n);
                memcpy(tmp, buf, n);
            }
            // no break here
        case eEncodingForm_Utf16Native:
            {
                Uint2* u = us;
                for (n = n/2; n--; ++u) {
                    result->Append(*u);
                }
            }
            break;
        case eEncodingForm_ISO8859_1:
            result->Append(tmp,eEncoding_ISO8859_1);
            break;
        case eEncodingForm_Windows_1252:
            result->Append(tmp,eEncoding_Windows_1252);
            break;
        case eEncodingForm_Utf8:
//            result->Append(tmp,eEncoding_UTF8);   
            result->append(tmp,n);
            break;
        default:
            if (what_if_no_bom == eNoBOM_GuessEncoding) {
                if (n == bom_max) {
                    input.read(tmp + n, buf_size - n);
                    n += input.gcount();
                    result->reserve(max(result->capacity(), result->size() + n));
                }
                tmp[n] = '\0';
                EEncoding enc = CStringUTF8::GuessEncoding(tmp);
                switch (enc) {
                default:
                case eEncoding_Unknown:
                    if (CStringUTF8::GetValidBytesCount(tmp, n) != 0) {
                        ef = eEncodingForm_Utf8;
                        result->Append(tmp,enc);
                    }
                    else {
                        NCBI_THROW(CCoreException, eCore,
                                "ReadIntoUtf8: cannot guess text encoding");
                    }
                    break;
                case eEncoding_UTF8:
                    ef = eEncodingForm_Utf8;
                    // no break here
                case eEncoding_Ascii:
                case eEncoding_ISO8859_1:
                case eEncoding_Windows_1252:
                    result->Append(tmp,enc);
                    break;
                }
            } else {
//                result->Append(tmp,eEncoding_UTF8);
                result->append(tmp,n);
            }
            break;
        }
        n = 0;
    }
    return ef_bom;
}


EEncodingForm GetTextEncodingForm(CNcbiIstream& input,
                                  EBOMDiscard   discard_bom)
{
    EEncodingForm ef = eEncodingForm_Unknown;
    if (input.good()) {
        const int bom_max = 4;
        char tmp[bom_max];
        memset(tmp,0,bom_max);
        Uint2* us = reinterpret_cast<Uint2*>(tmp);
        Uchar* uc = reinterpret_cast<Uchar*>(tmp);
        input.get(tmp[0]);
        int n = input.gcount();
        if (n == 1 && (uc[0] == 0xEF || uc[0] == 0xFE || uc[0] == 0xFF)) {
            input.get(tmp[1]);
            if (input.gcount()==1) {
                ++n;
                if (us[0] == 0xFEFF) {
                    ef = eEncodingForm_Utf16Native;
                } else if (us[0] == 0xFFFE) {
                    ef = eEncodingForm_Utf16Foreign;
                } else if (uc[1] == 0xBB) {
                    input.get(tmp[2]);
                    if (input.gcount()==1) {
                        ++n;
                        if (uc[2] == 0xBF) {
                            ef = eEncodingForm_Utf8;
                        }
                    }
                }
            }
        }
        if (ef == eEncodingForm_Unknown) {
            if (n > 1) {
                CStreamUtils::Pushback(input,tmp,n);
            } else if (n == 1) {
                input.unget();
            }
        } else {
            if (discard_bom == eBOM_Keep) {
                CStreamUtils::Pushback(input,tmp,n);
            }
        }
    }
    return ef;
}


/****************************************************************************
 * BASE64- Encoding/Decoding
 */

extern "C" void BASE64_Encode
(const void* src_buf,
 size_t      src_size,
 size_t*     src_read,
 void*       dst_buf,
 size_t      dst_size,
 size_t*     dst_written,
 size_t*     line_len)
{
    static const char syms[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ" /*26*/
        "abcdefghijklmnopqrstuvwxyz" /*52*/
        "0123456789+/";              /*64*/
    const size_t max_len = line_len ? *line_len : 76;
    const size_t max_src =
        ((dst_size - (max_len ? dst_size/(max_len + 1) : 0)) >> 2) * 3;
    unsigned char* src = (unsigned char*) src_buf;
    unsigned char* dst = (unsigned char*) dst_buf;
    size_t len = 0, i = 0, j = 0;
    unsigned char temp = 0, c;
    unsigned char shift = 2;
    if (!max_src  ||  !src_size) {
        *src_read    = 0;
        *dst_written = 0;
        if (dst_size > 0) {
            *dst = '\0';
        }
        return;
    }
    if (src_size > max_src) {
        src_size = max_src;
    }
    c = src[0];
    for (;;) {
        unsigned char bits = (c >> shift) & 0x3F;
        if (max_len  &&  len >= max_len) {
            dst[j++] = '\n';
            len = 0;
        }
        _ASSERT((size_t)(temp | bits) < sizeof(syms) - 1);
        dst[j++] = syms[temp | bits];
        len++;
        if (i >= src_size) {
            break;
        }
        shift += 2;
        shift &= 7;
        temp = (c << (8 - shift)) & 0x3F;
        if (shift) {
            c = ++i < src_size ? src[i] : 0;
        } else if (i + 1 == src_size) {
            i++;
        }
    }
    _ASSERT(j <= dst_size);
    *src_read = i;
    for (i = 0; i < (3 - src_size % 3) % 3; i++) {
        if (max_len  &&  len >= max_len) {
            dst[j++] = '\n';
            len = 0;
        }
        dst[j++] = '=';
        len++;
    }
    _ASSERT(j <= dst_size);
    *dst_written = j;
    if (j < dst_size) {
        dst[j] = '\0';
    }
}


extern "C" int/*bool*/ BASE64_Decode
(const void* src_buf,
 size_t      src_size,
 size_t*     src_read,
 void*       dst_buf,
 size_t      dst_size,
 size_t*     dst_written)
{
    unsigned char* src = (unsigned char*) src_buf;
    unsigned char* dst = (unsigned char*) dst_buf;
    size_t i = 0, j = 0, k = 0, l;
    unsigned int temp = 0;
    if (src_size < 4  ||  dst_size < 3) {
        *src_read    = 0;
        *dst_written = 0;
        return 0/*false*/;
    }
    for (;;) {
        int/*bool*/  ok = i < src_size ? 1/*true*/ : 0/*false*/;
        unsigned char c = ok ? src[i++] : '=';
        if (c == '=') {
            c  = 64; /*end*/
        } else if (c >= 'A'  &&  c <= 'Z') {
            c -= 'A';
        } else if (c >= 'a'  &&  c <= 'z') {
            c -= 'a' - 26;
        } else if (c >= '0'  &&  c <= '9') {
            c -= '0' - 52;
        } else if (c == '+') {
            c  = 62;
        } else if (c == '/') {
            c  = 63;
        } else {
            continue;
        }
        temp <<= 6;
        temp  |= c & 0x3F;
        if (!(++k & 3)  ||  c == 64) {
            if (c == 64) {
                if (k < 2) {
                    if (ok) {
                        /* pushback leading '=' */
                        --i;
                    }
                    break;
                }
                switch (k) {
                case 2:
                    temp >>= 4;
                    break;
                case 3:
                    temp >>= 10;
                    break;
                case 4:
                    temp >>= 8;
                    break;
                default:
                    _ASSERT(0);
                    break;
                }
                l = 4 - k;
                while (l > 0) {
                    /* eat up '='-padding */
                    if (i >= src_size)
                        break;
                    if (src[i] == '=')
                        l--;
                    else if (src[i] != '\r'  &&  src[i] != '\n')
                        break;
                    i++;
                }
            } else {
                k = 0;
            }
            switch (k) {
            case 0:
                dst[j++] = (temp & 0xFF0000) >> 16;
                /*FALLTHRU*/;
            case 4:
                dst[j++] = (temp & 0xFF00) >> 8;
                /*FALLTHRU*/
            case 3:
                dst[j++] = (temp & 0xFF);
                break;
            default:
                break;
            }
            if (j + 3 >= dst_size  ||  c == 64) {
                break;
            }
            temp = 0;
        }
    }
    *src_read    = i;
    *dst_written = j;
    return i  &&  j ? 1/*true*/ : 0/*false*/;
}


END_NCBI_SCOPE


// See in the header why it is outside of NCBI scope (SunPro bug workaround...)

#if defined(NCBI_USE_OLD_IOSTREAM)
extern NCBI_NS_NCBI::CNcbiOstream& operator<<(NCBI_NS_NCBI::CNcbiOstream& os,
                                              const NCBI_NS_STD::string& str)
{
    return str.empty() ? os : os << str.c_str();
}


extern NCBI_NS_NCBI::CNcbiIstream& operator>>(NCBI_NS_NCBI::CNcbiIstream& is,
                                              NCBI_NS_STD::string& str)
{
    int ch;
    if ( !is.ipfx() )
        return is;

    str.erase();

    SIZE_TYPE end = str.max_size();
    if ( is.width() )
        end = (int)end < is.width() ? end : is.width(); 

    SIZE_TYPE i = 0;
    for (ch = is.rdbuf()->sbumpc();
         ch != EOF  &&  !isspace((unsigned char) ch);
         ch = is.rdbuf()->sbumpc()) {
        str.append(1, (char)ch);
        i++;
        if (i == end)
            break;
    }
    if (ch == EOF) 
        is.clear(NcbiEofbit | is.rdstate());      
    if ( !i )
        is.clear(NcbiFailbit | is.rdstate());      

    is.width(0);
    return is;
}


#endif  /* NCBI_USE_OLD_IOSTREAM */
