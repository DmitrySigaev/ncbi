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
 *   TEST for:  NCBI C++ core string-related API
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbi_limits.h>
#include <corelib/version.hpp>
#include <corelib/ncbi_xstr.hpp>
#include <corelib/ncbifloat.h>
#include <corelib/ncbitime.hpp>
#include <corelib/ncbiexec.hpp>
#include <corelib/ncbifile.hpp>
#include <algorithm>
#include <locale.h>
#include <math.h>

#define BOOST_AUTO_TEST_MAIN
#include <corelib/test_boost.hpp>

#include <common/test_assert.h>  /* This header must go last */


// This is to use the ANSI C++ standard templates without the "std::" prefix
// and to use NCBI C++ entities without the "ncbi::" prefix
USING_NCBI_SCOPE;

static const int kBad = 555;


//----------------------------------------------------------------------------
// NStr::StringTo*()
//----------------------------------------------------------------------------

struct SStringNumericValues
{
    const char* str;
    int         flags;
    Int8        num;
    Int8        i;
    Uint8       u;
    Int8        i8;
    Uint8       u8;
    double      d;
    double      delta;

    bool IsGoodInt(void) const {
        return i != kBad;
    }
    bool IsGoodUInt(void) const {
        return u != (Uint8)kBad;
    }
    bool IsGoodInt8(void) const {
        return i8 != kBad;
    }
    bool IsGoodUInt8(void) const {
        return u8 != (Uint8)kBad;
    }
    bool IsGoodDouble(void) const {
        return d != (double)kBad;
    }
    bool Same(const string& s) const {
        if ( str[0] == '+' && isdigit((unsigned char) str[1]) ) {
            if ( s[0] == '+' ) {
                string tmp(s, 1, NPOS);
                return tmp == str + 1;
            } 
            return s == str + 1;
        }
        return s == NStr::TruncateSpaces(str, NStr::eTrunc_Both);
    }
};

#define DF 0

//                str  flags  num   int   uint  Int8  Uint8  double
#define BAD(v)   {  v, DF, -1, kBad, kBad, kBad, kBad, kBad, 0. }
#define STR(v)   { #v, DF, NCBI_CONST_INT8(v), NCBI_CONST_INT8(v), NCBI_CONST_UINT8(v), NCBI_CONST_INT8(v), NCBI_CONST_UINT8(v), v##., 0. }
#define STRI(v)  { #v, DF, -1, NCBI_CONST_INT8(v), kBad, NCBI_CONST_INT8(v), kBad, v##. , 0.}
#define STRU(v)  { #v, DF, -1, kBad, NCBI_CONST_UINT8(v), NCBI_CONST_INT8(v), NCBI_CONST_UINT8(v), v##. , 0.}
#define STR8(v)  { #v, DF, -1, kBad, kBad, NCBI_CONST_INT8(v), NCBI_CONST_UINT8(v), v##. , 0.}
#define STRI8(v) { #v, DF, -1, kBad, kBad, NCBI_CONST_INT8(v), kBad, v##., 0.}
#define STRU8(v) { #v, DF, -1, kBad, kBad, kBad, NCBI_CONST_UINT8(v), v##., 0. }
#define STRD(v)  { #v, DF, -1, kBad, kBad, kBad, kBad, v##. }

static const SStringNumericValues s_Str2NumTests[] = {
    STR(0),
    STR(1),
    STRI(-1),
#if (SIZEOF_INT > 4)
    STRI(-2147483649),
#else
    STRI8(-2147483649),
#endif
    STRI(-2147483648),
    STRI(-2147483647),
    STR(2147483646),
    STR(2147483647),
#if (SIZEOF_INT > 4)
    STR(2147483648),
#else
    STRU(2147483648),
#endif
    STRU(4294967294),
    STRU(4294967295),
#if (SIZEOF_INT > 4)
    STR(4294967296),
#else
    STR8(4294967296),
#endif
    STR(10),
    STR(100),
    STR(1000),
    STR(10000),
    STR(100000),
    STR(1000000),
    STR(10000000),
    STR(100000000),
    STR(1000000000),
#if (SIZEOF_INT > 4)
    STR(10000000000),
    STR(100000000000),
    STR(1000000000000),
    STR(10000000000000),
    STR(100000000000000),
    STR(1000000000000000),
    STR(10000000000000000),
    STR(100000000000000000),
    STR(1000000000000000000),
#else
    STR8(10000000000),
    STR8(100000000000),
    STR8(1000000000000),
    STR8(10000000000000),
    STR8(100000000000000),
    STR8(1000000000000000),
    STR8(10000000000000000),
    STR8(100000000000000000),
    STR8(1000000000000000000),
#endif
    STRD(-9223372036854775809),
    { "-9223372036854775808", DF, -1, kBad, kBad, NCBI_CONST_INT8(-9223372036854775807)-1, kBad, -9223372036854775808., 0. },
    STRI8(-9223372036854775807),
    STR8(9223372036854775806),
    STR8(9223372036854775807),
    STRU8(9223372036854775808),
    STRU8(18446744073709551614),
    STRU8(18446744073709551615),
    STRD(18446744073709551616),

    BAD(""),
    BAD("+"),
    BAD("-"),
    BAD("."),
    BAD(".."),
    BAD("abc"),
    { ".0",  DF, -1, kBad, kBad, kBad, kBad,  .0, 0.  },
    BAD(".0."),
    BAD("..0"),
    { ".01", DF, -1, kBad, kBad, kBad, kBad,  .01, 0. },
    { "1.",  DF, -1, kBad, kBad, kBad, kBad,  1. , 0. },
    { "1.1", DF, -1, kBad, kBad, kBad, kBad,  1.1, 0. },
    BAD("1.1."),
    BAD("1.."),
    STRI(-123),
    BAD("--123"),
    { "+123", DF, 123, 123, 123, 123, 123, 123, 0. },
    BAD("++123"),
    BAD("- 123"),
    BAD(" 123"),

    { " 123",     NStr::fAllowLeadingSpaces,  -1, 123,  123,  123,  123,  123., 0.},
    { " 123",     NStr::fAllowLeadingSymbols, -1, 123,  123,  123,  123,  123., 0. },
    BAD("123 "),
    { "123 ",     NStr::fAllowTrailingSpaces,  -1, 123,  123,  123,  123,  123., 0. },
    { "123 ",     NStr::fAllowTrailingSymbols, -1, 123,  123,  123,  123,  123., 0. },
    { "123(45) ", NStr::fAllowTrailingSymbols, -1, 123,  123,  123,  123,  123., 0. },
    { " 123 ",    NStr::fAllowLeadingSpaces | NStr::fAllowTrailingSpaces, -1, 123,  123,  123,  123,  123., 0. },
    
    { "1,234",    NStr::fAllowCommas, -1,    1234,    1234,    1234,    1234, kBad, 0. },
    { "1,234,567",NStr::fAllowCommas, -1, 1234567, 1234567, 1234567, 1234567, kBad, 0. },
    { "12,34",    NStr::fAllowCommas, -1,    kBad,    kBad,    kBad,    kBad, kBad, 0. },
    { ",123",     NStr::fAllowCommas, -1,    kBad,    kBad,    kBad,    kBad, kBad, 0. },
    { ",123",     NStr::fAllowCommas | NStr::fAllowLeadingSymbols, -1, 123, 123, 123, 123, 123, 0. },
#if (SIZEOF_INT > 4)
    {  "4,294,967,294", NStr::fAllowCommas, -1, NCBI_CONST_UINT8(4294967294), NCBI_CONST_UINT8(4294967294), NCBI_CONST_UINT8(4294967294), NCBI_CONST_UINT8(4294967294), kBad, 0. },
    {  "4,294,967,295", NStr::fAllowCommas, -1, NCBI_CONST_UINT8(4294967295), NCBI_CONST_UINT8(4294967295), NCBI_CONST_UINT8(4294967295), NCBI_CONST_UINT8(4294967295), kBad, 0. },
    {  "4,294,967,296", NStr::fAllowCommas, -1, NCBI_CONST_UINT8(4294967296), NCBI_CONST_UINT8(4294967296), NCBI_CONST_UINT8(4294967296), NCBI_CONST_UINT8(4294967296), kBad, 0. },
    { "-4,294,967,294", NStr::fAllowCommas, -1, NCBI_CONST_INT8(-4294967294), kBad,                         NCBI_CONST_INT8(-4294967294), kBad,                         kBad, 0. },
    { "-4,294,967,295", NStr::fAllowCommas, -1, NCBI_CONST_INT8(-4294967295), kBad,                         NCBI_CONST_INT8(-4294967295), kBad,                         kBad, 0. },
    { "-4,294,967,296", NStr::fAllowCommas, -1, NCBI_CONST_INT8(-4294967296), kBad,                         NCBI_CONST_INT8(-4294967296), kBad,                         kBad, 0. },
#else
    {  "4,294,967,294", NStr::fAllowCommas, -1, kBad,                         NCBI_CONST_UINT8(4294967294), NCBI_CONST_UINT8(4294967294), NCBI_CONST_UINT8(4294967294), kBad, 0. },
    {  "4,294,967,295", NStr::fAllowCommas, -1, kBad,                         NCBI_CONST_UINT8(4294967295), NCBI_CONST_UINT8(4294967295), NCBI_CONST_UINT8(4294967295), kBad, 0. },
    {  "4,294,967,296", NStr::fAllowCommas, -1, kBad,                         kBad,                         NCBI_CONST_UINT8(4294967296), NCBI_CONST_UINT8(4294967296), kBad, 0. },
    { "-4,294,967,294", NStr::fAllowCommas, -1, kBad,                         kBad,                         NCBI_CONST_INT8(-4294967294), kBad,                         kBad, 0. },
    { "-4,294,967,295", NStr::fAllowCommas, -1, kBad,                         kBad,                         NCBI_CONST_INT8(-4294967295), kBad,                         kBad, 0. },
    { "-4,294,967,296", NStr::fAllowCommas, -1, kBad,                         kBad,                         NCBI_CONST_INT8(-4294967296), kBad,                         kBad, 0. },
#endif

    { "+123",     0, 123, 123, 123, 123, 123, 123, 0. },
    { "123",      NStr::fMandatorySign, 123, kBad, kBad, kBad, kBad, kBad, 0. },
    { "+123",     NStr::fMandatorySign, 123,  123,  123,  123,  123, 123, 0. },
    { "-123",     NStr::fMandatorySign,  -1, -123, kBad, -123, kBad, -123, 0. },
    { "+123",     NStr::fAllowLeadingSymbols, 123,  123,  123,  123,  123,  123, 0. },
#if 0
    { "7E-380",   DF, -1, kBad, kBad, kBad, kBad, kBad, 0. },
    { "7E-325",   DF, -1, kBad, kBad, kBad, kBad, kBad, 0. },
    { "7E-324",   DF, -1, kBad, kBad, kBad, kBad, 7E-324, 0. },
    { "7E-323",   DF, -1, kBad, kBad, kBad, kBad, 7E-323, 0. },
#endif
#if 0 && defined(NCBI_OS_LINUX) && (NCBI_PLATFORM_BITS == 32) && !defined(_DEBUG)
    { "7E-38",   DF, -1, kBad, kBad, kBad, kBad, 7E-38, 0.000000000000002e-38 },
#else
    { "7E-38",   DF, -1, kBad, kBad, kBad, kBad, 7E-38, 0. },
#endif
    { "7E38",   DF, -1, kBad, kBad, kBad, kBad, 7E38, 0. },
    { "7E-500",   DF, -1, kBad, kBad, kBad, kBad, kBad, 0. },
    { "7E-512",   DF, -1, kBad, kBad, kBad, kBad, kBad, 0. },
    { "7E500",   DF, -1, kBad, kBad, kBad, kBad, kBad, 0. },
    { "7E512", DF, -1, kBad, kBad, kBad, kBad, kBad, 0. },
    { "7E768", DF, -1, kBad, kBad, kBad, kBad, kBad, 0. },
    { "7E4294967306", DF, -1, kBad, kBad, kBad, kBad, kBad, 0. },
    { ".000000000000000000000000000001", DF, -1, kBad, kBad, kBad, kBad,
      .000000000000000000000000000001, 1e-46 },
    { "-123",     NStr::fAllowLeadingSymbols,  -1, -123, kBad, -123, kBad, -123, 0. }
};


BOOST_AUTO_TEST_CASE(s_StringToNum)
{
    NcbiCout << NcbiEndl << "NStr::StringTo*() tests...";

    const size_t count = sizeof(s_Str2NumTests) / sizeof(s_Str2NumTests[0]);

    for (size_t i = 0;  i < count;  ++i) {
      for (int extra = 0;  extra < 4;  ++extra) {
        const SStringNumericValues* test = &s_Str2NumTests[i];
        CTempString str;
        string extra_str;
        if ( !extra ) {
            str = test->str;
        }
        else {
            extra_str = test->str;
            extra_str += "  9x"[extra];
            str = CTempString(extra_str).substr(0, extra_str.size()-1);
        }
        NStr::TStringToNumFlags flags = test->flags;
        NStr::TNumToStringFlags str_flags = 0;
        if ( flags & NStr::fMandatorySign ) 
            str_flags |= NStr::fWithSign;
        if ( flags & NStr::fAllowCommas )
            str_flags |= NStr::fWithCommas;
        bool allow_same_test = (flags < NStr::fAllowLeadingSpaces);

        NcbiCout << "\n*** Checking string '" << str << "'***" << NcbiEndl;

        // num
        {{
            int value = NStr::StringToNonNegativeInt(str);
            NcbiCout << "numeric value: " << value << ", toString: '"
                     << NStr::NumericToString(value) << "'" << NcbiEndl;
            BOOST_CHECK_EQUAL(value, test->num);
        }}

        // int
        try {
            int value = NStr::StringToInt(str, flags);
            NcbiCout << "int value: " << value << ", toString: '"
                     << NStr::NumericToString(value, str_flags) << "'"
                     << NcbiEndl;
            BOOST_CHECK_EQUAL(value, NStr::StringToNumeric<int>(str, flags));
            BOOST_CHECK(test->IsGoodInt());
            BOOST_CHECK_EQUAL(value, test->i);
            if (allow_same_test) {
                BOOST_CHECK(test->Same(NStr::IntToString(value, str_flags)));
                BOOST_CHECK(test->Same(NStr::NumericToString(value, str_flags)));
            }
        }
        catch (CException&) {
            if ( test->IsGoodInt() ) {
                ERR_POST("Cannot convert '" << str << "' to int");
            }
            BOOST_CHECK(!test->IsGoodInt());
        }

        // int
        {
            int value = NStr::StringToInt(str, flags | NStr::fConvErr_NoThrow);
            int err = errno;
            NcbiCout << "int value: " << value << ", toString: '"
                     << NStr::NumericToString(value, str_flags) << "'"
                     << " errno: " << err << NcbiEndl;
            BOOST_CHECK_EQUAL(value, NStr::StringToNumeric<int>(str,
                              flags | NStr::fConvErr_NoThrow));
            if ( value || !err ) {
                BOOST_CHECK(!err);
                BOOST_CHECK(test->IsGoodInt());
                BOOST_CHECK_EQUAL(value, test->i);
                if (allow_same_test) {
                    BOOST_CHECK(test->Same(NStr::IntToString(value, str_flags)));
                    BOOST_CHECK(test->Same(NStr::NumericToString(value, str_flags)));
                }
            }
            else {
                BOOST_CHECK(err);
                BOOST_CHECK(!test->IsGoodInt());
            }
        }

        // unsigned int
        try {
            unsigned int value = NStr::StringToUInt(str, flags);
            NcbiCout << "unsigned int value: " << value << ", toString: '"
                     << NStr::NumericToString(value, str_flags) << "'"
                     << NcbiEndl;
            BOOST_CHECK_EQUAL(value, NStr::StringToNumeric<unsigned int>(str, flags));
            BOOST_CHECK(test->IsGoodUInt());
            BOOST_CHECK_EQUAL(value, test->u);
            if (allow_same_test) {
                BOOST_CHECK(test->Same(NStr::UIntToString(value, str_flags)));
                BOOST_CHECK(test->Same(NStr::NumericToString(value, str_flags)));
            }
        }
        catch (CException&) {
            if ( test->IsGoodUInt() ) {
                ERR_POST("Cannot convert '" << str << "' to unsigned int");
            }
            BOOST_CHECK(!test->IsGoodUInt());
        }

        // unsigned int
        {
            unsigned int value = NStr::StringToUInt(str, flags | NStr::fConvErr_NoThrow);
            int err = errno;
            NcbiCout << "unsigned int value: " << value << ", toString: '"
                     << NStr::NumericToString(value, str_flags) << "'"
                     << " errno: " << err << NcbiEndl;
            BOOST_CHECK_EQUAL(value, NStr::StringToNumeric<unsigned int>(str, 
                              flags | NStr::fConvErr_NoThrow));
            if ( value || !err ) {
                BOOST_CHECK(!err);
                BOOST_CHECK(test->IsGoodUInt());
                BOOST_CHECK_EQUAL(value, test->u);
                if (allow_same_test) {
                    BOOST_CHECK(test->Same(NStr::UIntToString(value, str_flags)));
                    BOOST_CHECK(test->Same(NStr::NumericToString(value, str_flags)));
                }
            }
            else {
                BOOST_CHECK(err);
                BOOST_CHECK(!test->IsGoodUInt());
            }
        }

        // long
        try {
            long value = NStr::StringToLong(str, flags);
            NcbiCout << "long value: " << value << ", toString: '"
                     << NStr::NumericToString(value, str_flags) << "', "
                     << "Int8ToString: '"
                     << NStr::NumericToString(value, str_flags) << "'"
                     << NcbiEndl;
            BOOST_CHECK_EQUAL(value, NStr::StringToNumeric<long>(str, flags));

            #if (SIZEOF_LONG == SIZEOF_INT)
                BOOST_CHECK(test->IsGoodInt());
                BOOST_CHECK_EQUAL(value, test->i);
                if (allow_same_test) {
                    BOOST_CHECK(test->Same(NStr::LongToString(value, str_flags)));
                    BOOST_CHECK(test->Same(NStr::NumericToString(value, str_flags)));
                }
            #else
                BOOST_CHECK(test->IsGoodInt8());
                BOOST_CHECK_EQUAL(value, test->i8);
                if (allow_same_test) {
                    BOOST_CHECK(test->Same(NStr::Int8ToString(value, str_flags)));
                    BOOST_CHECK(test->Same(NStr::NumericToString(value, str_flags)));
                }
            #endif
        }
        catch (CException&) {
            #if (SIZEOF_LONG == SIZEOF_INT)
                if ( test->IsGoodInt() ) {
                    ERR_POST("Cannot convert '" << str << "' to unsigned long");
                }
                BOOST_CHECK(!test->IsGoodInt());
            #else
                if ( test->IsGoodInt8() ) {
                    ERR_POST("Cannot convert '" << str << "' to unsigned long");
                }
                BOOST_CHECK(!test->IsGoodInt8());
            #endif
        }

        // long
        {
            long value = NStr::StringToLong(str, flags | NStr::fConvErr_NoThrow);
            int err = errno;
            NcbiCout << "long value: " << value << ", toString: '"
                     << NStr::NumericToString(value, str_flags) << "', "
                     << "Int8ToString: '"
                     << NStr::NumericToString(value, str_flags) << "'"
                     << " errno: " << err << NcbiEndl;
            BOOST_CHECK_EQUAL(value, NStr::StringToNumeric<long>(str, 
                              flags | NStr::fConvErr_NoThrow));
            if ( value || !err ) {
                BOOST_CHECK(!err);
            #if (SIZEOF_LONG == SIZEOF_INT)
                BOOST_CHECK(test->IsGoodInt());
                BOOST_CHECK_EQUAL(value, test->i);
                if (allow_same_test) {
                    BOOST_CHECK(test->Same(NStr::LongToString(value, str_flags)));
                    BOOST_CHECK(test->Same(NStr::NumericToString(value, str_flags)));
                }
            #else
                BOOST_CHECK(test->IsGoodInt8());
                BOOST_CHECK_EQUAL(value, test->i8);
                if (allow_same_test) {
                    BOOST_CHECK(test->Same(NStr::Int8ToString(value, str_flags)));
                    BOOST_CHECK(test->Same(NStr::NumericToString(value, str_flags)));
                }
            #endif
            }
            else {
                BOOST_CHECK(err);
            #if (SIZEOF_LONG == SIZEOF_INT)
                BOOST_CHECK(!test->IsGoodInt());
            #else
                BOOST_CHECK(!test->IsGoodInt8());
            #endif
            }
        }

        // unsigned long
        try {
            unsigned long value = NStr::StringToULong(str, flags);
            NcbiCout << "unsigned long value: " << value << ", toString: '"
                     << NStr::NumericToString(value, str_flags) << "', "
                     << "UInt8ToString: '"
                     << NStr::NumericToString(value, str_flags) << "'"
                     << NcbiEndl;
            BOOST_CHECK_EQUAL(value, NStr::StringToNumeric<unsigned long>(str, flags));
            #if (SIZEOF_LONG == SIZEOF_INT)
                BOOST_CHECK(test->IsGoodUInt());
                BOOST_CHECK_EQUAL(value, test->u);
                if (allow_same_test) {
                    BOOST_CHECK(test->Same(NStr::ULongToString(value, str_flags)));
                    BOOST_CHECK(test->Same(NStr::NumericToString(value, str_flags)));
                }
            #else
                BOOST_CHECK(test->IsGoodUInt8());
                BOOST_CHECK_EQUAL(value, test->u8);
                if (allow_same_test) {
                    BOOST_CHECK(test->Same(NStr::UInt8ToString(value, str_flags)));
                    BOOST_CHECK(test->Same(NStr::NumericToString(value, str_flags)));
                }
            #endif
        }
        catch (CException&) {
            #if (SIZEOF_LONG == SIZEOF_INT)
                if ( test->IsGoodUInt() ) {
                    ERR_POST("Cannot convert '" << str <<
                             "' to unsigned long");
                }
                BOOST_CHECK(!test->IsGoodUInt());
            #else
                if ( test->IsGoodUInt8() ) {
                    ERR_POST("Cannot convert '" << str <<
                             "' to unsigned long");
                }
                BOOST_CHECK(!test->IsGoodUInt8());
            #endif
        }

        // unsigned long
        {
            unsigned long value = NStr::StringToULong(str, flags | NStr::fConvErr_NoThrow);
            int err = errno;
            NcbiCout << "unsigned long value: " << value << ", toString: '"
                     << NStr::NumericToString(value, str_flags) << "', "
                     << "UInt8ToString: '"
                     << NStr::NumericToString(value, str_flags) << "'"
                     << " errno: " << err << NcbiEndl;
            BOOST_CHECK_EQUAL(value, NStr::StringToNumeric<unsigned long>(str,
                              flags | NStr::fConvErr_NoThrow));
            if ( value || !err ) {
                BOOST_CHECK(!err);
            #if (SIZEOF_LONG == SIZEOF_INT)
                BOOST_CHECK(test->IsGoodUInt());
                BOOST_CHECK_EQUAL(value, test->u);
                if (allow_same_test) {
                    BOOST_CHECK(test->Same(NStr::ULongToString(value, str_flags)));
                    BOOST_CHECK(test->Same(NStr::NumericToString(value, str_flags)));
                }
            #else
                BOOST_CHECK(test->IsGoodUInt8());
                BOOST_CHECK_EQUAL(value, test->u8);
                if (allow_same_test) {
                    BOOST_CHECK(test->Same(NStr::UInt8ToString(value, str_flags)));
                    BOOST_CHECK(test->Same(NStr::NumericToString(value, str_flags)));
                }
            #endif
            }
            else {
                BOOST_CHECK(err);
            #if (SIZEOF_LONG == SIZEOF_INT)
                BOOST_CHECK(!test->IsGoodUInt());
            #else
                BOOST_CHECK(!test->IsGoodUInt8());
            #endif
            }
        }

        // Int8
        try {
            Int8 value = NStr::StringToInt8(str, flags);
            NcbiCout << "Int8 value: " << value << ", toString: '"
                     << NStr::NumericToString(value, str_flags) << "'"
                     << NcbiEndl;
            BOOST_CHECK_EQUAL(value, NStr::StringToNumeric<Int8>(str, flags));
            BOOST_CHECK(test->IsGoodInt8());
            BOOST_CHECK_EQUAL(value, test->i8);
            if (allow_same_test) {
                BOOST_CHECK(test->Same(NStr::Int8ToString(value, str_flags)));
                BOOST_CHECK(test->Same(NStr::NumericToString(value, str_flags)));
            }
        }
        catch (CException&) {
            if ( test->IsGoodInt8() ) {
                ERR_POST("Cannot convert '" << str << "' to Int8");
            }
            BOOST_CHECK(!test->IsGoodInt8());
        }

        // Int8
        {
            Int8 value = NStr::StringToInt8(str, flags | NStr::fConvErr_NoThrow);
            int err = errno;
            NcbiCout << "Int8 value: " << value << ", toString: '"
                     << NStr::NumericToString(value, str_flags) << "'"
                     << " errno: " << err << NcbiEndl;
            BOOST_CHECK_EQUAL(value, NStr::StringToNumeric<Int8>(str, 
                flags | NStr::fConvErr_NoThrow));
            if ( value || !err ) {
                BOOST_CHECK(!err);
                BOOST_CHECK(test->IsGoodInt8());
                BOOST_CHECK_EQUAL(value, test->i8);
                if (allow_same_test) {
                    BOOST_CHECK(test->Same(NStr::Int8ToString(value, str_flags)));
                    BOOST_CHECK(test->Same(NStr::NumericToString(value, str_flags)));
                }
            }
            else {
                BOOST_CHECK(err);
                BOOST_CHECK(!test->IsGoodInt8());
            }
        }

        // Uint8
        try {
            Uint8 value = NStr::StringToUInt8(str, flags);
            NcbiCout << "Uint8 value: " << value << ", toString: '"
                     << NStr::NumericToString(value, str_flags) << "'"
                     << NcbiEndl;
            BOOST_CHECK_EQUAL(value, NStr::StringToNumeric<Uint8>(str, flags));
            BOOST_CHECK(test->IsGoodUInt8());
            BOOST_CHECK_EQUAL(value, test->u8);
            if (allow_same_test) {
                BOOST_CHECK(test->Same(NStr::UInt8ToString(value, str_flags)));
                BOOST_CHECK(test->Same(NStr::NumericToString(value, str_flags)));
            }
        }
        catch (CException&) {
            if ( test->IsGoodUInt8() ) {
                ERR_POST("Cannot convert '" << str << "' to Uint8");
            }
            BOOST_CHECK(!test->IsGoodUInt8());
        }

        // Uint8
        {
            Uint8 value = NStr::StringToUInt8(str, flags | NStr::fConvErr_NoThrow);
            int err = errno;
            NcbiCout << "Uint8 value: " << value << ", toString: '"
                     << NStr::NumericToString(value, str_flags) << "'"
                     << " errno: " << err << NcbiEndl;
            BOOST_CHECK_EQUAL(value, NStr::StringToNumeric<Uint8>(str, 
                              flags | NStr::fConvErr_NoThrow));
            if ( value || !err ) {
                BOOST_CHECK(!err);
                BOOST_CHECK(test->IsGoodUInt8());
                BOOST_CHECK_EQUAL(value, test->u8);
                if (allow_same_test) {
                    BOOST_CHECK(test->Same(NStr::UInt8ToString(value, str_flags)));
                    BOOST_CHECK(test->Same(NStr::NumericToString(value, str_flags)));
                }
            }
            else {
                BOOST_CHECK(err);
                BOOST_CHECK(!test->IsGoodUInt8());
            }
        }

        // double
        try {
            double value = NStr::StringToDouble(str, flags);
            double valueP = NStr::StringToDouble(str, flags | NStr::fDecimalPosix);
//            BOOST_CHECK_EQUAL(value, valueP);
            NcbiCout << "double value: " << value << ", toString: '"
                     << NStr::NumericToString(value, str_flags) << "'"
                     << NcbiEndl;
            NcbiCout << "double valueP: " << valueP << ", toString: '"
                     << NStr::DoubleToString(valueP, -1, str_flags) << "'"
                     << NcbiEndl;
            if ( valueP != value ) {
                NcbiCout << setprecision(24);
                NcbiCout << "value : " << value << NcbiEndl;
                NcbiCout << "valueP: " << valueP << NcbiEndl;
                NcbiCout << setprecision(6);
                NcbiCout << "diff=" << (valueP-value) << " delta=" << test->delta
                    << NcbiEndl;
            }
            BOOST_CHECK_EQUAL(value,  NStr::StringToNumeric<double>(str, flags));
            BOOST_CHECK_EQUAL(valueP, NStr::StringToNumeric<double>(str, flags | NStr::fDecimalPosix));
            BOOST_CHECK(test->IsGoodDouble());
            BOOST_CHECK(valueP >= test->d-test->delta && valueP <= test->d+test->delta);
            BOOST_CHECK(valueP >= test->d-test->delta && valueP <= test->d+test->delta);
            BOOST_CHECK(value  >= test->d-test->delta && value  <= test->d+test->delta);
        }
        catch (CException&) {
            if ( test->IsGoodDouble() ) {
                ERR_POST("Cannot convert '" << str << "' to double");
            }
            BOOST_CHECK(!test->IsGoodDouble());
        }

        // double
        {
            double value = NStr::StringToDouble(str, flags | NStr::fConvErr_NoThrow);
            int err = errno;
            NcbiCout << "double value: " << value << ", toString: '"
                     << NStr::NumericToString(value, str_flags) << "'"
                     << " errno: " << err << NcbiEndl;
            BOOST_CHECK_EQUAL(value, NStr::StringToNumeric<double>(str, 
                              flags | NStr::fConvErr_NoThrow));
            if ( value || !err ) {
                BOOST_CHECK(!err);
                BOOST_CHECK(test->IsGoodDouble());
                BOOST_CHECK(value  >= test->d-test->delta && value  <= test->d+test->delta);
            }
            else {
                BOOST_CHECK(err);
                BOOST_CHECK(!test->IsGoodDouble());
            }
        }

        // double POSIX
        {
            double value = NStr::StringToDouble(str, flags | NStr::fDecimalPosix | NStr::fConvErr_NoThrow);
            int err = errno;
            NcbiCout << "double value: " << value << ", toString: '"
                     << NStr::NumericToString(value, str_flags) << "'"
                     << " errno: " << err << NcbiEndl;
            BOOST_CHECK_EQUAL(value, NStr::StringToNumeric<double>(str, 
                              flags | NStr::fDecimalPosix | NStr::fConvErr_NoThrow));
            if ( value || !err ) {
                if ( value != test->d ) {
                    NcbiCout << setprecision(24);
                    NcbiCout << "value: " << value << NcbiEndl;
                    NcbiCout << "  ref: " << test->d << NcbiEndl;
                    NcbiCout << setprecision(6);
                    NcbiCout << "diff=" << (value-test->d) << " delta=" << test->delta
                        << NcbiEndl;
                }
                BOOST_CHECK(!err);
                BOOST_CHECK(test->IsGoodDouble());
                BOOST_CHECK(value  >= test->d-test->delta && value  <= test->d+test->delta);
            }
            else {
                BOOST_CHECK(err);
                BOOST_CHECK(!test->IsGoodDouble());
            }
        }
      }
    }
}

struct SStringDoublePosixTest
{
    const char* str;
    double result;
    double delta;
};

static const SStringDoublePosixTest s_StrToDoublePosix[] = {
    {"123",                 123.,                1e-13},
    {"123.",                123.,                1e-13},
    {"123.456",             123.456,             1e-13},
    {"-123.456",           -123.456,             1e-14},
    {"-12.45",             -12.45,               1e-15},
    {"0.01",                0.01,                1e-18},
    {"0.01456000",          0.01456,             1e-18},
    {"2147483649",          2147483649.,         0.},
    {"-2147483649",        -2147483649.,         0.},
    {"214748364913",        214748364913.,       0.},
    {"123456789123456789",  123456789123456789., 0.},
    {"123456789123.45",     123456789123.45,     0.},
    {"1234.5678912345",     1234.5678912345,     0.},
    {"1.23456789123456789", 1.23456789123456789, 0.},
    {".123456789",          .123456789,          0.},
    {".123456789123",       .123456789123,       0.},
    {"12e12",               12.e12,              0.},
    {"123.e2",              123.e2,              0.},
    {"123.456e+2",          123.456e+2,          0.},
    {"+123.456e-2",         123.456e-2,          0.},
    {"-123.456e+2",        -123.456e+2,          0.},
    {"-123.456e+12",       -123.456e+12,         0.},
    {"-123.456e+25",       -123.456e+25,         0.},
    {"-123.456e+78",       -123.456e+78,         0.},
    {"-123.456e-2",        -123.456e-2,          0.},
    {"-123.456e-12",       -123.456e-12,         0.},
    {"-123.456e-25",       -123.456e-25,         0.},
    {"-123.456e-78",       -123.456e-78,         0.00000000000002e-078},
    {"-9223372036854775809",      -9223372036854775809.,       0.},
    {"-922337.2036854775809",     -922337.2036854775809,       0.},
    {"-92233720368547.75809",     -92233720368547.75809,       0.},
    {"-9223372036854775808",      -9223372036854775808.,       0.},
    {"-9223372036854775807",      -9223372036854775807.,       0.},
    {"9223372036854775806",        9223372036854775806.,       0.},
    {"9223372036854775807",        9223372036854775807.,       0.},
    {"9223372036854775808",        9223372036854775808.,       0.},
    {"18446744073709551614",       18446744073709551614.,      0.},
    {"18446744073709551615",       18446744073709551615.,      0.},
    {"18446744073709551616",       18446744073709551616.,      0.},
    {"1844674407370955.1616",      1844674407370955.1616,      0.},
    {"1844674407370955.1616",      1844674407370955.1616,      0.},
    {"184467.44073709551616",      184467.44073709551616,      0.},
    {"1.8446744073709551616",      1.8446744073709551616,      0.},
    {"1.8446744073709551616e5",    1.8446744073709551616e5,    0.},
    {"1.8446744073709551616e25",   1.8446744073709551616e25,   0.},
    {"1.8446744073709551616e125",  1.8446744073709551616e125,  0.},
    {"184467.44073709551616e5",    184467.44073709551616e5,    0.},
    {"184467.44073709551616e-5",   184467.44073709551616e-5,   0.},
    {"184467.44073709551616e25",   184467.44073709551616e25,   0.},
    {"184467.44073709551616e-25",  184467.44073709551616e-25,  0.},

    {"1.7976931348623159e+308",  HUGE_VAL, 0.},
    {"1.7976931348623157e+308",  1.7976931348623157e+308, 0.},
    {"1.7976931348623155e+308",  1.7976931348623155e+308, 0.0000000000000003e+308},
    { "1.797693134862315e+307",  1.797693134862315e+307,  0.000000000000002e+307},
    { "1.797693134862315e+306",  1.797693134862315e+306,  0.},
    {"2.2250738585072014e-308",  2.2250738585072014e-308, 0.},
    {"2.2250738585072019e-308",  2.2250738585072019e-308, 0.},
    {"2.2250738585072024e-308",  2.2250738585072024e-308, 0.},
    { "2.225073858507202e-308",  2.225073858507202e-308,  0.},
    {"2.2250738585072016e-307",  2.2250738585072016e-307, 0.0000000000000004e-307},/* NCBI_FAKE_WARNING */
    {"2.2250738585072016e-306",  2.2250738585072016e-306, 0.0000000000000004e-306},/* NCBI_FAKE_WARNING */
    {"2.2250738585072016e-305",  2.2250738585072016e-305, 0.0000000000000004e-305},/* NCBI_FAKE_WARNING */
    {"-123.456e+4578",  -HUGE_VAL, 0.},
    {"-123.456e-4578",  0., 0.},

    { "7E0000000001", 70., 0.},
    { "7E512", HUGE_VAL, 0.},
    { "7E-500", 0., 0.},
    { "7E4294967306", HUGE_VAL, 0.},
    { "1.000000000000000000000000000001", 1., 0. },
    { "000.000000000000000000000000000001", 1e-30, 0.0000000000000001e-030 },
    {NULL,0,0}
};

BOOST_AUTO_TEST_CASE(s_StringToDoublePosix)
{
    NcbiCout << NcbiEndl << "NStr::StringToDoublePosix() tests..." << NcbiEndl;
    char *endptr;
    for (int i = 0; s_StrToDoublePosix[i].str; ++i) {
        const double& result  = s_StrToDoublePosix[i].result;
        double delta   = finite(result)? fabs(result)*2.22e-16: 0;
        const char* str = s_StrToDoublePosix[i].str;
        NcbiCout << "*** Checking string '" << str << "'*** " << NcbiEndl;
        double valuep = NStr::StringToDouble(str, NStr::fDecimalPosix | NStr::fIgnoreErrno);
        NcbiCout << setprecision(24) << result << " vs " << valuep
            << setprecision(6) << NcbiEndl;
        if ( delta == 0 )
            BOOST_CHECK(valuep == result);
        double min = result-delta, max = result+delta;
        if ( finite(min) )
            BOOST_CHECK(valuep >= min);
        if ( finite(max) )
            BOOST_CHECK(valuep <= max);
    }
    
    string out;
    double value;

    value = NStr::StringToDoublePosix("nan", &endptr);
    BOOST_CHECK( isnan(value) );
    BOOST_CHECK( endptr && !*endptr );
    NStr::DoubleToString(out, value, -1, NStr::fDoublePosix);
    BOOST_CHECK( NStr::Compare(out, "NAN") == 0 );
    NStr::NumericToString(out, value, NStr::fDoublePosix);
    BOOST_CHECK( NStr::Compare(out, "NAN") == 0 );

    value = NStr::StringToDoublePosix(out.c_str(), &endptr);
    BOOST_CHECK( isnan(value) );
    BOOST_CHECK( endptr && !*endptr );
    NStr::DoubleToString(out, value, -1, NStr::fDoublePosix);
    BOOST_CHECK( NStr::Compare(out, "NAN") == 0 );
    NStr::NumericToString(out, value, NStr::fDoublePosix);
    BOOST_CHECK( NStr::Compare(out, "NAN") == 0 );

    value = NStr::StringToDoublePosix("inf", &endptr);
    BOOST_CHECK( !finite(value) && value>0.);
    BOOST_CHECK( endptr && !*endptr );
    NStr::DoubleToString(out, value, -1, NStr::fDoublePosix);
    BOOST_CHECK( NStr::Compare(out, "INF") == 0 );
    NStr::NumericToString(out, value, NStr::fDoublePosix);
    BOOST_CHECK( NStr::Compare(out, "INF") == 0 );

    value = NStr::StringToDoublePosix(out.c_str(), &endptr);
    BOOST_CHECK( endptr && !*endptr );
    BOOST_CHECK( !finite(value) && value>0.);
    NStr::DoubleToString(out, value, -1, NStr::fDoublePosix);
    BOOST_CHECK( NStr::Compare(out, "INF") == 0 );
    NStr::NumericToString(out, value, NStr::fDoublePosix);
    BOOST_CHECK( NStr::Compare(out, "INF") == 0 );

    value = NStr::StringToDoublePosix("infinity", &endptr);
    BOOST_CHECK( !finite(value) && value>0. );
    BOOST_CHECK( endptr && !*endptr );
    NStr::DoubleToString(out, value, -1, NStr::fDoublePosix);
    BOOST_CHECK( NStr::Compare(out, "INF") == 0 );
    NStr::NumericToString(out, value, NStr::fDoublePosix);
    BOOST_CHECK( NStr::Compare(out, "INF") == 0 );

    value = NStr::StringToDoublePosix(out.c_str(), &endptr);
    BOOST_CHECK( !finite(value) && value>0. );
    BOOST_CHECK( endptr && !*endptr );
    NStr::DoubleToString(out, value, -1, NStr::fDoublePosix);
    BOOST_CHECK( NStr::Compare(out, "INF") == 0 );
    NStr::NumericToString(out, value, NStr::fDoublePosix);
    BOOST_CHECK( NStr::Compare(out, "INF") == 0 );

    value = NStr::StringToDoublePosix("+inf", &endptr);
    BOOST_CHECK( !finite(value) && value>0. );
    BOOST_CHECK( endptr && !*endptr );
    NStr::DoubleToString(out, value, -1, NStr::fDoublePosix);
    BOOST_CHECK( NStr::Compare(out, "INF") == 0 );
    NStr::NumericToString(out, value, NStr::fDoublePosix);
    BOOST_CHECK( NStr::Compare(out, "INF") == 0 );

    value = NStr::StringToDoublePosix("+infinity", &endptr);
    BOOST_CHECK( !finite(value) && value>0. );
    BOOST_CHECK( endptr && !*endptr );
    NStr::DoubleToString(out, value, -1, NStr::fDoublePosix);
    BOOST_CHECK( NStr::Compare(out, "INF") == 0 );
    NStr::NumericToString(out, value, NStr::fDoublePosix);
    BOOST_CHECK( NStr::Compare(out, "INF") == 0 );

    value = NStr::StringToDoublePosix("-inf", &endptr);
    BOOST_CHECK( !finite(value) && value<0. );
    BOOST_CHECK( endptr && !*endptr );
    NStr::DoubleToString(out, value, -1, NStr::fDoublePosix);
    BOOST_CHECK( NStr::Compare(out, "-INF") == 0 );
    NStr::NumericToString(out, value, NStr::fDoublePosix);
    BOOST_CHECK( NStr::Compare(out, "-INF") == 0 );

    value = NStr::StringToDoublePosix("-infinity", &endptr);
    BOOST_CHECK( !finite(value) && value<0. );
    BOOST_CHECK( endptr && !*endptr );
    NStr::DoubleToString(out, value, -1, NStr::fDoublePosix);
    BOOST_CHECK( NStr::Compare(out, "-INF") == 0 );
    NStr::NumericToString(out, value, NStr::fDoublePosix);
    BOOST_CHECK( NStr::Compare(out, "-INF") == 0 );

    value = NStr::StringToDoublePosix("+Infinity", &endptr);
    BOOST_CHECK( !finite(value) && value>0. );
    BOOST_CHECK( endptr && !*endptr );
    NStr::DoubleToString(out, value, -1, NStr::fDoublePosix);
    BOOST_CHECK( NStr::Compare(out, "INF") == 0 );
    NStr::NumericToString(out, value, NStr::fDoublePosix);
    BOOST_CHECK( NStr::Compare(out, "INF") == 0 );

    value = NStr::StringToDoublePosix("Infinity", &endptr);
    BOOST_CHECK( !finite(value) && value>0. );
    BOOST_CHECK( endptr && !*endptr );
    NStr::DoubleToString(out, value, -1, NStr::fDoublePosix);
    BOOST_CHECK( NStr::Compare(out, "INF") == 0 );
    NStr::NumericToString(out, value, NStr::fDoublePosix);
    BOOST_CHECK( NStr::Compare(out, "INF") == 0 );

    value = NStr::StringToDoublePosix("-infinity", &endptr);
    BOOST_CHECK( !finite(value) && value<0. );
    BOOST_CHECK( endptr && !*endptr );
    NStr::DoubleToString(out, value, -1, NStr::fDoublePosix);
    BOOST_CHECK( NStr::Compare(out, "-INF") == 0 );
    NStr::NumericToString(out, value, NStr::fDoublePosix);
    BOOST_CHECK( NStr::Compare(out, "-INF") == 0 );
}

static const SStringNumericValues s_Str2NumNonPosixTests[] = {
    {  ",",   DF,                         -1, kBad, kBad, kBad, kBad, kBad, 0. },
    {  ",,",  DF,                         -1, kBad, kBad, kBad, kBad, kBad, 0. },
    {  ".,",  NStr::fDecimalPosixOrLocal, -1, kBad, kBad, kBad, kBad, kBad, 0. },
    {  ",.",  NStr::fDecimalPosixOrLocal, -1, kBad, kBad, kBad, kBad, kBad, 0. },
    { ",0",   DF,                         -1, kBad, kBad, kBad, kBad,  .0 , 0. },
    { ",0.",  NStr::fDecimalPosixOrLocal, -1, kBad, kBad, kBad, kBad, kBad, 0. },
    { ".0,",  NStr::fDecimalPosixOrLocal, -1, kBad, kBad, kBad, kBad, kBad, 0. },
    { ",01",  DF,                         -1, kBad, kBad, kBad, kBad,  .01, 0. },
    { "1,",   DF,                         -1, kBad, kBad, kBad, kBad,  1. , 0. },
    { "1,1",  DF,                         -1, kBad, kBad, kBad, kBad,  1.1, 0. },
    { "1,1",  NStr::fDecimalPosixOrLocal, -1, kBad, kBad, kBad, kBad,  1.1, 0. },
    { "1,1",  NStr::fDecimalPosix,        -1, kBad, kBad, kBad, kBad,  kBad, 0. },
    { "1.1",  NStr::fDecimalPosixOrLocal, -1, kBad, kBad, kBad, kBad,  1.1, 0. },
    { "1.1",  NStr::fDecimalPosix,        -1, kBad, kBad, kBad, kBad,  1.1, 0. },
    { "1,1.", NStr::fDecimalPosixOrLocal, -1, kBad, kBad, kBad, kBad,  kBad, 0. },
    { "1,1,", DF,                         -1, kBad, kBad, kBad, kBad,  kBad, 0. },
    { "1.,",  NStr::fDecimalPosixOrLocal, -1, kBad, kBad, kBad, kBad,  kBad, 0. },
    { "1.1,", NStr::fDecimalPosixOrLocal, -1, kBad, kBad, kBad, kBad,  kBad, 0. },
    { "1.1,", NStr::fDecimalPosix,        -1, kBad, kBad, kBad, kBad,  kBad, 0. },
    { "1.,",  NStr::fDecimalPosixOrLocal, -1, kBad, kBad, kBad, kBad,  kBad, 0. },
    { "1.,",  NStr::fDecimalPosix,        -1, kBad, kBad, kBad, kBad,  kBad, 0. },

    { "12,34",  DF,                            -1, kBad, kBad, kBad, kBad,  12.34, 0. },
    { "12,34",  NStr::fDecimalPosixOrLocal,    -1, kBad, kBad, kBad, kBad,  12.34, 0. },
    { "12.34",  NStr::fDecimalPosixOrLocal,    -1, kBad, kBad, kBad, kBad,  12.34, 0. },
    { "12.34",  NStr::fDecimalPosix,           -1, kBad, kBad, kBad, kBad,  12.34, 0. },
    { "12,34e-2",  DF,                         -1, kBad, kBad, kBad, kBad,  .1234, 1e-17 },
    { "12,34e-2",  NStr::fDecimalPosixOrLocal, -1, kBad, kBad, kBad, kBad,  .1234, 1e-17 },
    { "12.34e-2",  NStr::fDecimalPosixOrLocal, -1, kBad, kBad, kBad, kBad,  .1234, 1e-17 },
    { "12.34e-2",  NStr::fDecimalPosix,        -1, kBad, kBad, kBad, kBad,  .1234, 1e-17 },
    { "1234,",  NStr::fDecimalPosixOrLocal,    -1, kBad, kBad, kBad, kBad,  1234., 0. },
    { "1234.",  NStr::fDecimalPosix,           -1, kBad, kBad, kBad, kBad,  1234., 0. },
    { "1234",   NStr::fDecimalPosixOrLocal,    -1, kBad, kBad, kBad, kBad,  1234., 0. },
    { "0,0",    NStr::fDecimalPosixOrLocal,    -1, kBad, kBad, kBad, kBad,  0., 0. },
    { "0,000",  NStr::fDecimalPosixOrLocal,    -1, kBad, kBad, kBad, kBad,  0., 0. },
    { "0.000",  NStr::fDecimalPosix,           -1, kBad, kBad, kBad, kBad,  0., 0. },
    { ",,1234", NStr::fDecimalPosixOrLocal,    -1, kBad, kBad, kBad, kBad,  kBad, 0. },
    { "1234,,", NStr::fDecimalPosixOrLocal,    -1, kBad, kBad, kBad, kBad,  kBad, 0. },
    { "12,,34", NStr::fDecimalPosixOrLocal,    -1, kBad, kBad, kBad, kBad,  kBad, 0. }

};

#if 1
BOOST_AUTO_TEST_CASE(s_StringToDouble)
{
    char* prevlocal = strdup( setlocale(LC_NUMERIC,NULL));
    if (!setlocale(LC_NUMERIC,"deu")) {
        if (!setlocale(LC_NUMERIC,"de")) {
            if (!setlocale(LC_NUMERIC,"de_DE")) {
                if (!setlocale(LC_NUMERIC,"fr")) {
		    // cannot find suitable locale, skip the test
                    free(prevlocal);
		            return;
		        }
		    }
		}
    }
    NcbiCout << NcbiEndl << "NStr::StringToDouble() tests...";

    const size_t count = sizeof(s_Str2NumNonPosixTests) / sizeof(s_Str2NumNonPosixTests[0]);

    for (size_t i = 0;  i < count;  ++i) {
        const SStringNumericValues* test = &s_Str2NumNonPosixTests[i];
        const char*                 str  = test->str;
        NStr::TStringToNumFlags    flags = test->flags;

        NcbiCout << "\n*** Checking string '" << str << "'***" << NcbiEndl;
        // double
        try {
            double value = NStr::StringToDouble(str, flags);
            NcbiCout << "double value: " << value << ", toString: '"
                     << NStr::NumericToString(value) << "'"
                     << NcbiEndl;
            BOOST_CHECK(test->IsGoodDouble());
            BOOST_CHECK(value >= test->d-test->delta && value <= test->d+test->delta);
        }
        catch (CException&) {
            if ( test->IsGoodDouble() ) {
                ERR_POST("Cannot convert '" << str << "' to double");
            }
            BOOST_CHECK(!test->IsGoodDouble());
        }
    }
    setlocale(LC_NUMERIC,prevlocal);
    free(prevlocal);
}
#endif


//----------------------------------------------------------------------------
// NStr::StringTo*() radix test
//----------------------------------------------------------------------------

// Writing separate tests for StringToUInt8 because we
// need to test for different radix values such as 2, 8, and 16.

struct SRadixTest {
    const char* str;      // String input
    int         base;     // Radix base 
    Uint8       value;    // Expected value

    bool Same(const string& s) const
    {
        if ( s.empty() ) {
            return true;
        }
        const char* s1 = str;
        const char* s2 = s.c_str();
        
        // Workaround for ICC 8.0. It have an optimizer bug.
        // c_str() can return non-null-terminated string.
        // So we will use strncmp() here instead of strcmp().
        
        size_t n = s.length();
        
        while (*s1 == '0') s1++;
        if ( *s1 == 'x' )  s1++;
        while (*s2 == '0') { s2++; n--; };

        return (NStr::strncmp(s1, s2, n) == 0);
    }
};

static const SRadixTest s_RadixTests[] = {
    { "A",         16, 10 },
    { "0xA",       16, 10 },
    { "B9",        16, 185 },
    { "C5D",       16, 3165 },
    { "FFFF",      16, 65535 },
    { "17ABCDEF",  16, 397135343 },
    { "BADBADBA",  16, 3134959034U },
    { "7",          8, 7 },
    { "17",         8, 15 },
    { "177",        8, 127 },
    { "0123",       8, 83 },
    { "01234567",   8, 342391 },
    { "0",          2, 0 },
    { "1",          2, 1 },
    { "10",         2, 2 },
    { "11",         2, 3 },
    { "100",        2, 4 },
    { "101",        2, 5 }, 
    { "110",        2, 6 },
    { "111",        2, 7 },

    // Autodetect radix base
    { "0xC5D",      0, 3165 },    // base 16
    { "0123",       0, 83   },    // base 8
    { "123",        0, 123  },    // base 10
    { "111",        0, 111  },    // base 10

    // Invalid values come next
    { "10ABCDEFGH",16, kBad },
    { "12345A",    10, kBad },
    { "012345678",  8, kBad },
    { "012",        2, kBad }
};


BOOST_AUTO_TEST_CASE(s_StringToNumRadix)
{
    NcbiCout << NcbiEndl << "NStr::StringTo*() radix tests...";

    const size_t count = sizeof(s_RadixTests)/sizeof(s_RadixTests[0]);
    for (size_t i = 0;  i < count;  ++i) 
    {
        const SRadixTest* test = &s_RadixTests[i];
        NcbiCout << NcbiEndl 
                 << "Checking numeric string: '" << test->str 
                 << "': with base " << test->base << NcbiEndl;

        // Int
        try {
            if ( test->value <= (Uint8)kMax_Int ) {
                int val = NStr::StringToInt(test->str, 0, test->base);
                string str;
                if ( test->base ) {
                    NStr::IntToString(str, val, 0, test->base);
                }
                NcbiCout << "Int value: " << val 
                         << ", toString: '" << str << "'" 
                         << NcbiEndl;
                BOOST_CHECK_EQUAL((Uint8)val, test->value);
                BOOST_CHECK(test->Same(str));
                if ( test->base ) {
                    NStr::NumericToString(str, val, 0, test->base);
                }
                BOOST_CHECK(test->Same(str));
            }
        }
        catch (CException&) {
            BOOST_CHECK_EQUAL(test->value, (Uint8)kBad);
        }


        // UInt
        try {
            if ( test->value <= kMax_UInt ) {
                unsigned int val = 
                    NStr::StringToUInt(test->str, 0, test->base);
                string str;
                if ( test->base ) {
                    NStr::UIntToString(str, val, 0, test->base);
                }
                NcbiCout << "UInt value: " << val 
                         << ", toString: '" << str << "'" 
                         << NcbiEndl;
                BOOST_CHECK_EQUAL(val, test->value);
                BOOST_CHECK(test->Same(str));
                if ( test->base ) {
                    NStr::NumericToString(str, val, 0, test->base);
                }
                BOOST_CHECK(test->Same(str));
            }
        }
        catch (CException&) {
            BOOST_CHECK_EQUAL(test->value, (Uint8)kBad);
        }

        // Int8
        try {
            if ( test->value <= (Uint8)kMax_I8 ) {
                Int8 val = NStr::StringToInt8(test->str, 0, test->base);
                string str;
                if ( test->base ) {
                    NStr::Int8ToString(str, val, 0, test->base);
                }
                NcbiCout << "Int8 value: " << val 
                         << ", toString: '" << str << "'" 
                         << NcbiEndl;
                BOOST_CHECK_EQUAL((Uint8)val, test->value);
                BOOST_CHECK(test->Same(str));
                if ( test->base ) {
                    NStr::NumericToString(str, val, 0, test->base);
                }
                BOOST_CHECK(test->Same(str));
            }
        }
        catch (CException&) {
            BOOST_CHECK_EQUAL(test->value, (Uint8)kBad);
        }

        // Uint8
        try {
            Uint8 val = NStr::StringToUInt8(test->str, 0, test->base);
            string str;
            if ( test->base ) {
                NStr::UInt8ToString(str, val, 0, test->base);
            }
            NcbiCout << "Uint8 value: " << (unsigned)val 
                     << ", toString: '" << str << "'" 
                     << NcbiEndl;
            BOOST_CHECK_EQUAL(val, test->value);
            BOOST_CHECK(test->Same(str));
            if ( test->base ) {
                NStr::NumericToString(str, val, 0, test->base);
            }
            BOOST_CHECK(test->Same(str));
        }
        catch (CException&) {
            BOOST_CHECK_EQUAL(test->value, (Uint8)kBad);
        }
    } 

    // Some additional tests

    string str;

    NStr::IntToString(str, kMax_Int, 0, 2);
#if (SIZEOF_INT == 4)
    BOOST_CHECK(str == "1111111111111111111111111111111");
#elif (SIZEOF_INT == 8)
    BOOST_CHECK(str == "111111111111111111111111111111111111111111111111111111111111111");
#endif
    NStr::NumericToString(str, kMax_Int, 0, 2);
#if (SIZEOF_INT == 4)
    BOOST_CHECK(str == "1111111111111111111111111111111");
#elif (SIZEOF_INT == 8)
    BOOST_CHECK(str == "111111111111111111111111111111111111111111111111111111111111111");
#endif

    NStr::LongToString(str, kMax_Long, 0, 2);
#if (SIZEOF_LONG == 4)
    BOOST_CHECK(str == "1111111111111111111111111111111");
#elif (SIZEOF_LONG == 8)
    BOOST_CHECK(str == "111111111111111111111111111111111111111111111111111111111111111");
#endif
    NStr::NumericToString(str, kMax_Long, 0, 2);
#if (SIZEOF_LONG == 4)
    BOOST_CHECK(str == "1111111111111111111111111111111");
#elif (SIZEOF_LONG == 8)
    BOOST_CHECK(str == "111111111111111111111111111111111111111111111111111111111111111");
#endif

    NStr::UIntToString(str, kMax_UInt, 0, 2);
#if (SIZEOF_INT == 4)
    BOOST_CHECK(str == "11111111111111111111111111111111");
#elif (SIZEOF_INT == 8)
    BOOST_CHECK(str == "1111111111111111111111111111111111111111111111111111111111111111");
#endif
    NStr::NumericToString(str, kMax_UInt, 0, 2);
#if (SIZEOF_INT == 4)
    BOOST_CHECK(str == "11111111111111111111111111111111");
#elif (SIZEOF_INT == 8)
    BOOST_CHECK(str == "1111111111111111111111111111111111111111111111111111111111111111");
#endif

    NStr::ULongToString(str, kMax_ULong, 0, 2);
#if (SIZEOF_LONG == 4)
    BOOST_CHECK(str == "11111111111111111111111111111111");
#elif (SIZEOF_LONG == 8)
    BOOST_CHECK(str == "1111111111111111111111111111111111111111111111111111111111111111");
#endif
    NStr::NumericToString(str, kMax_ULong, 0, 2);
#if (SIZEOF_LONG == 4)
    BOOST_CHECK(str == "11111111111111111111111111111111");
#elif (SIZEOF_LONG == 8)
    BOOST_CHECK(str == "1111111111111111111111111111111111111111111111111111111111111111");
#endif

    NStr::IntToString(str, -1, 0, 8);
#if (SIZEOF_INT == 4) 
    BOOST_CHECK(str == "37777777777");
#elif (SIZEOF_INT == 8)
    BOOST_CHECK(str == "1777777777777777777777");
#endif
    NStr::NumericToString(str, -1, 0, 8);
#if (SIZEOF_INT == 4) 
    BOOST_CHECK(str == "37777777777");
#elif (SIZEOF_INT == 8)
    BOOST_CHECK(str == "1777777777777777777777");
#endif

    NStr::LongToString(str, -1, 0, 8);
#if (SIZEOF_LONG == 4) 
    BOOST_CHECK(str == "37777777777");
#elif (SIZEOF_LONG == 8)
    BOOST_CHECK(str == "1777777777777777777777");
#endif
    NStr::NumericToString(str, (long)-1, 0, 8);
#if (SIZEOF_LONG == 4) 
    BOOST_CHECK(str == "37777777777");
#elif (SIZEOF_LONG == 8)
    BOOST_CHECK(str == "1777777777777777777777");
#endif

    NStr::IntToString(str, -1, 0, 16);
#if (SIZEOF_INT == 4)
    BOOST_CHECK(str == "FFFFFFFF");
#elif (SIZEOF_INT == 8)
    BOOST_CHECK(str == "FFFFFFFFFFFFFFFF");
#endif
    NStr::NumericToString(str, -1, 0, 16);
#if (SIZEOF_INT == 4)
    BOOST_CHECK(str == "FFFFFFFF");
#elif (SIZEOF_INT == 8)
    BOOST_CHECK(str == "FFFFFFFFFFFFFFFF");
#endif

    NStr::LongToString(str, -1, 0, 16);
#if (SIZEOF_LONG == 4)
    BOOST_CHECK(str == "FFFFFFFF");
#elif (SIZEOF_LONG == 8)
    BOOST_CHECK(str == "FFFFFFFFFFFFFFFF");
#endif
    NStr::NumericToString(str, (long)-1, 0, 16);
#if (SIZEOF_LONG == 4)
    BOOST_CHECK(str == "FFFFFFFF");
#elif (SIZEOF_LONG == 8)
    BOOST_CHECK(str == "FFFFFFFFFFFFFFFF");
#endif

    NStr::UInt8ToString(str, NCBI_CONST_UINT8(12345678901234567), 0, 16);
    BOOST_CHECK(str == "2BDC545D6B4B87");
    NStr::NumericToString(str, NCBI_CONST_UINT8(12345678901234567), 0, 16);
    BOOST_CHECK(str == "2BDC545D6B4B87");
}


//----------------------------------------------------------------------------
// NStr::StringTo*_DataSize()
//----------------------------------------------------------------------------

struct SStringDataSizeValues
{
    const char*             str;
    NStr::TStringToNumFlags flags;
    Uint8                   expected;

    bool IsGood(void) const {
        return expected != (Uint8)kBad;
    }
};

static const SStringDataSizeValues s_Str2DataSizeTests[] = {
    // str  flags     num
    { "10",    0,      10 },
    { "10b",   0,      10 },
    { "10k",   0, 10*1000 },
    { "10K",   0, 10*1000 },
    { "10KB",  0, 10*1000 },
    { "10KiB", 0, 10*1024 },
    { "10KIB", 0, 10*1024 },
    { "10K",   NStr::fDS_ForceBinary, 10*1024 },
    { "10KB",  NStr::fDS_ForceBinary, 10*1024 },
    { "10M",   0, 10*1000*1000 },
    { "10MB",  0, 10*1000*1000 },
    { "10MiB", 0, 10*1024*1024 },
    { "10M",   NStr::fDS_ForceBinary, 10*1024*1024 },
    { "10MB",  NStr::fDS_ForceBinary, 10*1024*1024 },
    { "10G",   0, Uint8(10)*1000*1000*1000 },
    { "10GB",  0, Uint8(10)*1000*1000*1000 },
    { "10GiB", 0, Uint8(10)*1024*1024*1024 },
    { "10G",   NStr::fDS_ForceBinary, Uint8(10)*1024*1024*1024 },
    { "10GB",  NStr::fDS_ForceBinary, Uint8(10)*1024*1024*1024 },
    { "10T",   0, Uint8(10)*1000*1000*1000*1000 },
    { "10TB",  0, Uint8(10)*1000*1000*1000*1000 },
    { "10TiB", 0, Uint8(10)*1024*1024*1024*1024 },
    { "10T",   NStr::fDS_ForceBinary, Uint8(10)*1024*1024*1024*1024 },
    { "10TB",  NStr::fDS_ForceBinary, Uint8(10)*1024*1024*1024*1024 },
    { "10P",   0, Uint8(10)*1000*1000*1000*1000*1000 },
    { "10PB",  0, Uint8(10)*1000*1000*1000*1000*1000 },
    { "10PiB", 0, Uint8(10)*1024*1024*1024*1024*1024 },
    { "10P",   NStr::fDS_ForceBinary, Uint8(10)*1024*1024*1024*1024*1024 },
    { "10PB",  NStr::fDS_ForceBinary, Uint8(10)*1024*1024*1024*1024*1024 },
    { "10E",   0, Uint8(10)*1000*1000*1000*1000*1000*1000 },
    { "10EB",  0, Uint8(10)*1000*1000*1000*1000*1000*1000 },
    { "10EiB", 0, Uint8(10)*1024*1024*1024*1024*1024*1024 },
    { "10E",   NStr::fDS_ForceBinary, Uint8(10)*1024*1024*1024*1024*1024*1024 },
    { "10EB",  NStr::fDS_ForceBinary, Uint8(10)*1024*1024*1024*1024*1024*1024 },
    { "+10K",  0, 10*1000 },
    { "-10K",  0, kBad },
    { "1GBx",  0, kBad },
    { "1Gi",   0, kBad },
    { "10000000000000GB", 0, kBad },
    { " 10K",  0, kBad },
    { " 10K",  NStr::fAllowLeadingSpaces, 10*1000 },
    { "10K ",  0, kBad },
    { "10K ",  NStr::fAllowTrailingSpaces, 10*1000 },
    { "10 K",  0, 10*1000 },
    { "10 K",  NStr::fDS_ProhibitSpaceBeforeSuffix, kBad },
    { "10K",   NStr::fMandatorySign, kBad },
    { "+10K",  NStr::fMandatorySign, 10*1000 },
    { "-10K",  NStr::fMandatorySign, kBad },
    { "1,000K", 0, kBad },
    { "1,000K", NStr::fAllowCommas, 1000*1000 },
    { "K10K",  0, kBad },
    { "K10K",  NStr::fAllowLeadingSymbols, 10*1000 },
    { "K10K",  NStr::fAllowLeadingSymbols, 10*1000 },
    { "10KG",  NStr::fAllowTrailingSymbols, 10*1000 },
    { "0.123",  0, 0 },
    { "0.567",  0, 1 },
    { "0.123456 K",  0, 123 },
    { "0.123567 K",  0, 124 },
    { "0.123456 KiB",  0, 126 },
    { "0.123567 KiB",  0, 127 },
    { "0.123 MB",  0, 123000 },
    { "0.123 MiB", 0, 128975 },
    { "0.123456 GiB",  0, 132559871 },
    { "123abc",  NStr::fAllowTrailingSymbols, 123 },
    { "123klm",  NStr::fAllowTrailingSymbols, 123000 },
    { "123klm",  NStr::fAllowTrailingSymbols + NStr::fDS_ForceBinary, 125952 },
    { "123kbc",  NStr::fAllowTrailingSymbols, 123000 },
    { "123kic",  NStr::fAllowTrailingSymbols, 123000 },
    { "123kibc", NStr::fAllowTrailingSymbols, 125952 },
    { "123.abc", NStr::fAllowTrailingSymbols, 123 },
    { "123.kic", NStr::fAllowTrailingSymbols, 123 },
    { "123.001 kib", 0, 125953 },
    { "12.3456 pib", 0, NCBI_CONST_UINT8(13899909889916299) },
    { "abc.123.abc",  NStr::fAllowLeadingSymbols + NStr::fAllowTrailingSymbols, 123 },
    { "abc.+123.abc", NStr::fAllowLeadingSymbols + NStr::fAllowTrailingSymbols, 123 },
    { "abc+.123.abc", NStr::fAllowLeadingSymbols + NStr::fAllowTrailingSymbols, 123 },
    { "abc-.123.abc", NStr::fAllowLeadingSymbols + NStr::fAllowTrailingSymbols, 123 },
    { "abc.-123.abc", NStr::fAllowLeadingSymbols + NStr::fAllowTrailingSymbols, 123 },
    { "abc.123.abc",  NStr::fAllowLeadingSymbols + NStr::fAllowTrailingSymbols
                      + NStr::fMandatorySign, kBad },
    { "abc.+123.abc", NStr::fAllowLeadingSymbols + NStr::fAllowTrailingSymbols
                      + NStr::fMandatorySign, 123 },
    { "abc+.123.abc", NStr::fAllowLeadingSymbols + NStr::fAllowTrailingSymbols
                      + NStr::fMandatorySign, kBad },
    { "abc-.123.abc", NStr::fAllowLeadingSymbols + NStr::fAllowTrailingSymbols
                      + NStr::fMandatorySign, kBad },
    { "abc.-123.abc", NStr::fAllowLeadingSymbols + NStr::fAllowTrailingSymbols
                      + NStr::fMandatorySign, kBad },
    { "abc.123.abc",  NStr::fAllowLeadingSymbols + NStr::fAllowTrailingSymbols
                      + NStr::fDS_ProhibitFractions, 123 },
    { "abc.+123.abc", NStr::fAllowLeadingSymbols + NStr::fAllowTrailingSymbols
                      + NStr::fDS_ProhibitFractions, 123 },
    { "abc+.123.abc", NStr::fAllowLeadingSymbols + NStr::fAllowTrailingSymbols
                      + NStr::fDS_ProhibitFractions, 123 },
    { "abc-.123.abc", NStr::fAllowLeadingSymbols + NStr::fAllowTrailingSymbols
                      + NStr::fDS_ProhibitFractions, 123 },
    { "abc.-123.abc", NStr::fAllowLeadingSymbols + NStr::fAllowTrailingSymbols
                      + NStr::fDS_ProhibitFractions, 123 },
    { "abc.123.abc",  NStr::fAllowLeadingSymbols + NStr::fAllowTrailingSymbols
                      + NStr::fMandatorySign + NStr::fDS_ProhibitFractions, kBad },
    { "abc.+123.abc", NStr::fAllowLeadingSymbols + NStr::fAllowTrailingSymbols
                      + NStr::fMandatorySign + NStr::fDS_ProhibitFractions, 123 },
    { "abc+.123.abc", NStr::fAllowLeadingSymbols + NStr::fAllowTrailingSymbols
                      + NStr::fMandatorySign + NStr::fDS_ProhibitFractions, kBad },
    { "abc-.123.abc", NStr::fAllowLeadingSymbols + NStr::fAllowTrailingSymbols
                      + NStr::fMandatorySign + NStr::fDS_ProhibitFractions, kBad },
    { "abc.-123.abc", NStr::fAllowLeadingSymbols + NStr::fAllowTrailingSymbols
                      + NStr::fMandatorySign + NStr::fDS_ProhibitFractions, kBad },
    { "123.456", 0, 123 },
    { "123.567", 0, 124 },
    { "123.567",   NStr::fAllowTrailingSymbols + NStr::fDS_ProhibitFractions, 123 },
    { "123.567MB", NStr::fAllowTrailingSymbols + NStr::fDS_ProhibitFractions, 123 },
    { "123.567MB", NStr::fDS_ProhibitFractions, kBad },
    { ".34KB",   0, kBad },
    { ".34KB",   NStr::fAllowLeadingSymbols, 34000 },
    { "k.5.6m",  NStr::fAllowLeadingSymbols, 5600000 },
    { "k.5.6m",  NStr::fAllowLeadingSymbols + NStr::fDS_ProhibitFractions, kBad },
    { "k.5.6m",  NStr::fAllowLeadingSymbols + NStr::fDS_ProhibitFractions
                 + NStr::fAllowTrailingSymbols, 5 },
    { "123,4,56789,0", NStr::fAllowCommas, 1234567890 },
    { "123,4,", NStr::fAllowCommas, kBad },
    { "123,4,", NStr::fAllowCommas + NStr::fAllowTrailingSymbols, 1234 },
    { "123,4,,56", NStr::fAllowCommas, kBad },
    { "123,4,,56", NStr::fAllowCommas + NStr::fAllowTrailingSymbols, 1234 },
    { ",123,4", NStr::fAllowCommas, kBad },
    { ",123,4", NStr::fAllowCommas + NStr::fAllowLeadingSymbols, 1234 },
    { "10", NStr::fDecimalPosix + NStr::fConvErr_NoThrow, kBad },
    { "10", NStr::fDecimalPosixOrLocal + NStr::fConvErr_NoThrow, kBad },
    { "10", NStr::fIgnoreErrno + NStr::fConvErr_NoThrow, kBad },
    { "10", NStr::fWithSign + NStr::fConvErr_NoThrow, kBad },
    { "10", NStr::fWithCommas + NStr::fConvErr_NoThrow, kBad },
    { "10", NStr::fDoubleFixed + NStr::fConvErr_NoThrow, kBad },
    { "10", NStr::fDoubleScientific + NStr::fConvErr_NoThrow, kBad },
    { "10", NStr::fDoublePosix + NStr::fConvErr_NoThrow, kBad },
    { "10", NStr::fDS_Binary + NStr::fConvErr_NoThrow, kBad },
    { "10", NStr::fDS_NoDecimalPoint + NStr::fConvErr_NoThrow, kBad },
    { "10", NStr::fDS_PutSpaceBeforeSuffix + NStr::fConvErr_NoThrow, kBad },
    { "10", NStr::fDS_ShortSuffix + NStr::fConvErr_NoThrow, kBad },
    { "10", NStr::fDS_PutBSuffixToo + NStr::fConvErr_NoThrow, kBad }
};

BOOST_AUTO_TEST_CASE(s_StringToNumDataSize)
{
    NcbiCout << NcbiEndl << "NStr::StringToUInt8_DataSize() tests...";

    const size_t count = sizeof(s_Str2DataSizeTests) /
                         sizeof(s_Str2DataSizeTests[0]);

    for (size_t i = 0;  i < count;  ++i)
    {
        const SStringDataSizeValues* test  = &s_Str2DataSizeTests[i];
        const char*                  str   = test->str;
        NStr::TStringToNumFlags      flags = test->flags;
        NcbiCout << "\n*** Checking string '" << str << "'***" << NcbiEndl;

        try {
            Uint8 value = NStr::StringToUInt8_DataSize(str, flags);
            NcbiCout << "value: " << value << NcbiEndl;
            BOOST_CHECK(test->IsGood());
            BOOST_CHECK_EQUAL(value, test->expected);
        }
        catch (CException&) {
            if ( test->IsGood() ) {
                ERR_POST("Cannot convert '" << str << "' to data size");
            } else {
                NcbiCout << "value: bad" << NcbiEndl;
            }
            BOOST_CHECK(!test->IsGood());
        }
    }
}


//----------------------------------------------------------------------------
// NStr::Uint8ToString_DataSize()
//----------------------------------------------------------------------------

struct SUint8DataSizeValues
{
    Uint8                   num;
    NStr::TNumToStringFlags flags;
    unsigned int            max_digits;
    const char*             expected;

    bool IsGood(void) const {
        return expected != NULL;
    }
};

static const SUint8DataSizeValues s_Num2StrDataSizeTests[] = {
    // num  flags    str
    { 10, 0, 3, "10" },
    { 10, NStr::fDS_PutBSuffixToo, 3, "10B" },
    { 10, NStr::fDS_PutBSuffixToo + NStr::fDS_PutSpaceBeforeSuffix, 3, "10 B" },
    { 10*1000, 0, 3, "10.0KB" },
    { 10*1000, NStr::fDS_ShortSuffix, 3, "10.0K" },
    { 10*1024, NStr::fDS_Binary, 3, "10.0KiB" },
    { 10*1024, NStr::fDS_Binary + NStr::fDS_ShortSuffix, 3, "10.0K" },
    { 10*1000*1000, 0, 3, "10.0MB" },
    { 10*1000*1000, NStr::fDS_ShortSuffix, 3, "10.0M" },
    { 10*1024*1024, NStr::fDS_Binary, 3, "10.0MiB" },
    { 10*1024*1024, NStr::fDS_Binary + NStr::fDS_ShortSuffix, 3, "10.0M" },
    { Uint8(10)*1000*1000*1000, 0, 3, "10.0GB" },
    { Uint8(10)*1000*1000*1000, NStr::fDS_ShortSuffix, 3, "10.0G" },
    { Uint8(10)*1024*1024*1024, NStr::fDS_Binary, 3, "10.0GiB" },
    { Uint8(10)*1024*1024*1024, NStr::fDS_Binary + NStr::fDS_ShortSuffix, 3, "10.0G" },
    { Uint8(10)*1000*1000*1000*1000, 0, 3, "10.0TB" },
    { Uint8(10)*1000*1000*1000*1000, NStr::fDS_ShortSuffix, 3, "10.0T" },
    { Uint8(10)*1024*1024*1024*1024, NStr::fDS_Binary, 3, "10.0TiB" },
    { Uint8(10)*1024*1024*1024*1024, NStr::fDS_Binary + NStr::fDS_ShortSuffix, 3, "10.0T" },
    { Uint8(10)*1000*1000*1000*1000*1000, 0, 3, "10.0PB" },
    { Uint8(10)*1000*1000*1000*1000*1000, NStr::fDS_ShortSuffix, 3, "10.0P" },
    { Uint8(10)*1024*1024*1024*1024*1024, NStr::fDS_Binary, 3, "10.0PiB" },
    { Uint8(10)*1024*1024*1024*1024*1024, NStr::fDS_Binary + NStr::fDS_ShortSuffix, 3, "10.0P" },
    { Uint8(10)*1000*1000*1000*1000*1000*1000, 0, 3, "10.0EB" },
    { Uint8(10)*1000*1000*1000*1000*1000*1000, NStr::fDS_ShortSuffix, 3, "10.0E" },
    { Uint8(10)*1024*1024*1024*1024*1024*1024, NStr::fDS_Binary, 3, "10.0EiB" },
    { Uint8(10)*1024*1024*1024*1024*1024*1024, NStr::fDS_Binary + NStr::fDS_ShortSuffix, 3, "10.0E" },
    { 10*1000, NStr::fWithSign, 3, "+10.0KB" },
    { 10*1000, NStr::fDS_NoDecimalPoint, 3, "10KB" },
    { 10*1000, NStr::fDS_PutSpaceBeforeSuffix, 3, "10.0 KB" },
    { 1000, NStr::fDS_NoDecimalPoint, 4, "1000" },
    { 1000, NStr::fDS_NoDecimalPoint + NStr::fWithCommas, 4, "1,000" },
    { 123456789, 0, 6, "123.457MB" },
    { 3456789, 0, 6, "3.45679MB" },
    { 456789, 0, 6, "456.789KB" },
    { 123456789, 0, 4, "123.5MB" },
    { 123456789, NStr::fDS_NoDecimalPoint, 6, "123457KB" },
    { 3456789, NStr::fDS_NoDecimalPoint, 6, "3457KB" },
    { 456789, NStr::fDS_NoDecimalPoint, 6, "456789" },
    { 123456789, NStr::fDS_NoDecimalPoint, 4, "123MB" },
    { 23456789, NStr::fDS_NoDecimalPoint, 4, "23MB" },
    { 3456789, NStr::fDS_NoDecimalPoint, 4, "3457KB" },
    { 123456789, 0, 1, "123MB" },
    { 123456789, 0, 2, "123MB" },
    { 12345, 0, 1, "12.3KB" },
    { NCBI_CONST_UINT8(13899853594920957), NStr::fDS_Binary, 6, "12.3456PiB" },
    { 1000, NStr::fDS_Binary, 3, "0.98KiB" },
    { 1000, NStr::fDS_Binary, 4, "1000" },
    { 1000, NStr::fDS_Binary, 5, "1000" },
    { 1000, NStr::fDS_Binary, 6, "1000" },
    { 1023, NStr::fDS_Binary, 3, "1.00KiB" },
    { 1023, NStr::fDS_Binary, 4, "1023" },
    { 1023, NStr::fDS_Binary, 5, "1023" },
    { 1023, NStr::fDS_Binary, 6, "1023" },
    { 1024, NStr::fDS_Binary, 3, "1.00KiB" },
    { 1024, NStr::fDS_Binary, 4, "1.000KiB" },
    { 1024, NStr::fDS_Binary, 5, "1.000KiB" },
    { 1024, NStr::fDS_Binary, 6, "1.000KiB" },
    { 99999, NStr::fDS_Binary, 3, "97.7KiB" },
    { 99999, NStr::fDS_Binary, 4, "97.66KiB" },
    { 99999, NStr::fDS_Binary, 5, "97.655KiB" },
    { 99999, NStr::fDS_Binary, 6, "97.655KiB" },
    { 999930, NStr::fDS_Binary, 3, "976KiB" },
    { 999930, NStr::fDS_Binary, 4, "976.5KiB" },
    { 999930, NStr::fDS_Binary, 5, "976.49KiB" },
    { 999930, NStr::fDS_Binary, 6, "976.494KiB" },
    { 1000000, NStr::fDS_Binary, 3, "977KiB" },
    { 1000000, NStr::fDS_Binary, 4, "976.6KiB" },
    { 1000000, NStr::fDS_Binary, 5, "976.56KiB" },
    { 1000000, NStr::fDS_Binary, 6, "976.563KiB" },
    { 1023993, NStr::fDS_Binary, 3, "0.98MiB" },
    { 1023993, NStr::fDS_Binary, 4, "1000KiB" },
    { 1023993, NStr::fDS_Binary, 5, "999.99KiB" },
    { 1023993, NStr::fDS_Binary, 6, "999.993KiB" },
    { 1047552, NStr::fDS_Binary, 3, "1.00MiB" },
    { 1047552, NStr::fDS_Binary, 4, "1023KiB" },
    { 1047552, NStr::fDS_Binary, 5, "1023.0KiB" },
    { 1047552, NStr::fDS_Binary, 6, "1023.00KiB" },
    { 1048064, NStr::fDS_Binary, 3, "1.00MiB" },
    { 1048064, NStr::fDS_Binary, 4, "1.000MiB" },
    { 1048064, NStr::fDS_Binary, 5, "1023.5KiB" },
    { 1048064, NStr::fDS_Binary, 6, "1023.50KiB" },
    { 1048576, NStr::fDS_Binary, 3, "1.00MiB" },
    { 1048576, NStr::fDS_Binary, 4, "1.000MiB" },
    { 1048576, NStr::fDS_Binary, 5, "1.0000MiB" },
    { 1048576, NStr::fDS_Binary, 6, "1.00000MiB" },
    { 1572864, NStr::fDS_Binary, 3, "1.50MiB" },
    { 1572864, NStr::fDS_Binary, 4, "1.500MiB" },
    { 1572864, NStr::fDS_Binary, 5, "1.5000MiB" },
    { 1572864, NStr::fDS_Binary, 6, "1.50000MiB" },
    { 1000, NStr::fDS_Binary + NStr::fDS_NoDecimalPoint, 3, "1KiB" },
    { 1000, NStr::fDS_Binary + NStr::fDS_NoDecimalPoint, 4, "1000" },
    { 1000, NStr::fDS_Binary + NStr::fDS_NoDecimalPoint, 5, "1000" },
    { 1000, NStr::fDS_Binary + NStr::fDS_NoDecimalPoint, 6, "1000" },
    { 1023, NStr::fDS_Binary + NStr::fDS_NoDecimalPoint, 3, "1KiB" },
    { 1023, NStr::fDS_Binary + NStr::fDS_NoDecimalPoint, 4, "1023" },
    { 1023, NStr::fDS_Binary + NStr::fDS_NoDecimalPoint, 5, "1023" },
    { 1023, NStr::fDS_Binary + NStr::fDS_NoDecimalPoint, 6, "1023" },
    { 1024, NStr::fDS_Binary + NStr::fDS_NoDecimalPoint, 3, "1KiB" },
    { 1024, NStr::fDS_Binary + NStr::fDS_NoDecimalPoint, 4, "1024" },
    { 1024, NStr::fDS_Binary + NStr::fDS_NoDecimalPoint, 5, "1024" },
    { 1024, NStr::fDS_Binary + NStr::fDS_NoDecimalPoint, 6, "1024" },
    { 99999, NStr::fDS_Binary + NStr::fDS_NoDecimalPoint, 3, "98KiB" },
    { 99999, NStr::fDS_Binary + NStr::fDS_NoDecimalPoint, 4, "98KiB" },
    { 99999, NStr::fDS_Binary + NStr::fDS_NoDecimalPoint, 5, "99999" },
    { 99999, NStr::fDS_Binary + NStr::fDS_NoDecimalPoint, 6, "99999" },
    { 999930, NStr::fDS_Binary + NStr::fDS_NoDecimalPoint, 3, "976KiB" },
    { 999930, NStr::fDS_Binary + NStr::fDS_NoDecimalPoint, 4, "976KiB" },
    { 999930, NStr::fDS_Binary + NStr::fDS_NoDecimalPoint, 5, "976KiB" },
    { 999930, NStr::fDS_Binary + NStr::fDS_NoDecimalPoint, 6, "999930" },
    { 1000000, NStr::fDS_Binary + NStr::fDS_NoDecimalPoint, 3, "977KiB" },
    { 1000000, NStr::fDS_Binary + NStr::fDS_NoDecimalPoint, 4, "977KiB" },
    { 1000000, NStr::fDS_Binary + NStr::fDS_NoDecimalPoint, 5, "977KiB" },
    { 1000000, NStr::fDS_Binary + NStr::fDS_NoDecimalPoint, 6, "977KiB" },
    { 1023993, NStr::fDS_Binary + NStr::fDS_NoDecimalPoint, 3, "1MiB" },
    { 1023993, NStr::fDS_Binary + NStr::fDS_NoDecimalPoint, 4, "1000KiB" },
    { 1023993, NStr::fDS_Binary + NStr::fDS_NoDecimalPoint, 5, "1000KiB" },
    { 1023993, NStr::fDS_Binary + NStr::fDS_NoDecimalPoint, 6, "1000KiB" },
    { 1047552, NStr::fDS_Binary + NStr::fDS_NoDecimalPoint, 3, "1MiB" },
    { 1047552, NStr::fDS_Binary + NStr::fDS_NoDecimalPoint, 4, "1023KiB" },
    { 1047552, NStr::fDS_Binary + NStr::fDS_NoDecimalPoint, 5, "1023KiB" },
    { 1047552, NStr::fDS_Binary + NStr::fDS_NoDecimalPoint, 6, "1023KiB" },
    { 1048064, NStr::fDS_Binary + NStr::fDS_NoDecimalPoint, 3, "1MiB" },
    { 1048064, NStr::fDS_Binary + NStr::fDS_NoDecimalPoint, 4, "1024KiB" },
    { 1048064, NStr::fDS_Binary + NStr::fDS_NoDecimalPoint, 5, "1024KiB" },
    { 1048064, NStr::fDS_Binary + NStr::fDS_NoDecimalPoint, 6, "1024KiB" },
    { 1048576, NStr::fDS_Binary + NStr::fDS_NoDecimalPoint, 3, "1MiB" },
    { 1048576, NStr::fDS_Binary + NStr::fDS_NoDecimalPoint, 4, "1024KiB" },
    { 1048576, NStr::fDS_Binary + NStr::fDS_NoDecimalPoint, 5, "1024KiB" },
    { 1048576, NStr::fDS_Binary + NStr::fDS_NoDecimalPoint, 6, "1024KiB" },
    { 1572864, NStr::fDS_Binary + NStr::fDS_NoDecimalPoint, 3, "2MiB" },
    { 1572864, NStr::fDS_Binary + NStr::fDS_NoDecimalPoint, 4, "1536KiB" },
    { 1572864, NStr::fDS_Binary + NStr::fDS_NoDecimalPoint, 5, "1536KiB" },
    { 1572864, NStr::fDS_Binary + NStr::fDS_NoDecimalPoint, 6, "1536KiB" },
    { 1000, 0, 3, "1.00KB" },
    { 1000, 0, 4, "1.000KB" },
    { 1000, 0, 5, "1.000KB" },
    { 1000, 0, 6, "1.000KB" },
    { 1023, 0, 3, "1.02KB" },
    { 1023, 0, 4, "1.023KB" },
    { 1023, 0, 5, "1.023KB" },
    { 1023, 0, 6, "1.023KB" },
    { 1024, 0, 3, "1.02KB" },
    { 1024, 0, 4, "1.024KB" },
    { 1024, 0, 5, "1.024KB" },
    { 1024, 0, 6, "1.024KB" },
    { 99999, 0, 3, "100KB" },
    { 99999, 0, 4, "100.0KB" },
    { 99999, 0, 5, "99.999KB" },
    { 99999, 0, 6, "99.999KB" },
    { 999930, 0, 3, "1.00MB" },
    { 999930, 0, 4, "999.9KB" },
    { 999930, 0, 5, "999.93KB" },
    { 999930, 0, 6, "999.930KB" },
    { 1000000, 0, 3, "1.00MB" },
    { 1000000, 0, 4, "1.000MB" },
    { 1000000, 0, 5, "1.0000MB" },
    { 1000000, 0, 6, "1.00000MB" },
    { 1023993, 0, 3, "1.02MB" },
    { 1023993, 0, 4, "1.024MB" },
    { 1023993, 0, 5, "1.0240MB" },
    { 1023993, 0, 6, "1.02399MB" },
    { 1047552, 0, 3, "1.05MB" },
    { 1047552, 0, 4, "1.048MB" },
    { 1047552, 0, 5, "1.0476MB" },
    { 1047552, 0, 6, "1.04755MB" },
    { 1048064, 0, 3, "1.05MB" },
    { 1048064, 0, 4, "1.048MB" },
    { 1048064, 0, 5, "1.0481MB" },
    { 1048064, 0, 6, "1.04806MB" },
    { 1048576, 0, 3, "1.05MB" },
    { 1048576, 0, 4, "1.049MB" },
    { 1048576, 0, 5, "1.0486MB" },
    { 1048576, 0, 6, "1.04858MB" },
    { 1572864, 0, 3, "1.57MB" },
    { 1572864, 0, 4, "1.573MB" },
    { 1572864, 0, 5, "1.5729MB" },
    { 1572864, 0, 6, "1.57286MB" },
    { 1000, NStr::fDS_NoDecimalPoint, 3, "1KB" },
    { 1000, NStr::fDS_NoDecimalPoint, 4, "1000" },
    { 1000, NStr::fDS_NoDecimalPoint, 5, "1000" },
    { 1000, NStr::fDS_NoDecimalPoint, 6, "1000" },
    { 1023, NStr::fDS_NoDecimalPoint, 3, "1KB" },
    { 1023, NStr::fDS_NoDecimalPoint, 4, "1023" },
    { 1023, NStr::fDS_NoDecimalPoint, 5, "1023" },
    { 1023, NStr::fDS_NoDecimalPoint, 6, "1023" },
    { 1024, NStr::fDS_NoDecimalPoint, 3, "1KB" },
    { 1024, NStr::fDS_NoDecimalPoint, 4, "1024" },
    { 1024, NStr::fDS_NoDecimalPoint, 5, "1024" },
    { 1024, NStr::fDS_NoDecimalPoint, 6, "1024" },
    { 99999, NStr::fDS_NoDecimalPoint, 3, "100KB" },
    { 99999, NStr::fDS_NoDecimalPoint, 4, "100KB" },
    { 99999, NStr::fDS_NoDecimalPoint, 5, "99999" },
    { 99999, NStr::fDS_NoDecimalPoint, 6, "99999" },
    { 999930, NStr::fDS_NoDecimalPoint, 3, "1MB" },
    { 999930, NStr::fDS_NoDecimalPoint, 4, "1000KB" },
    { 999930, NStr::fDS_NoDecimalPoint, 5, "1000KB" },
    { 999930, NStr::fDS_NoDecimalPoint, 6, "999930" },
    { 1000000, NStr::fDS_NoDecimalPoint, 3, "1MB" },
    { 1000000, NStr::fDS_NoDecimalPoint, 4, "1000KB" },
    { 1000000, NStr::fDS_NoDecimalPoint, 5, "1000KB" },
    { 1000000, NStr::fDS_NoDecimalPoint, 6, "1000KB" },
    { 1023993, NStr::fDS_NoDecimalPoint, 3, "1MB" },
    { 1023993, NStr::fDS_NoDecimalPoint, 4, "1024KB" },
    { 1023993, NStr::fDS_NoDecimalPoint, 5, "1024KB" },
    { 1023993, NStr::fDS_NoDecimalPoint, 6, "1024KB" },
    { 1047552, NStr::fDS_NoDecimalPoint, 3, "1MB" },
    { 1047552, NStr::fDS_NoDecimalPoint, 4, "1048KB" },
    { 1047552, NStr::fDS_NoDecimalPoint, 5, "1048KB" },
    { 1047552, NStr::fDS_NoDecimalPoint, 6, "1048KB" },
    { 1048064, NStr::fDS_NoDecimalPoint, 3, "1MB" },
    { 1048064, NStr::fDS_NoDecimalPoint, 4, "1048KB" },
    { 1048064, NStr::fDS_NoDecimalPoint, 5, "1048KB" },
    { 1048064, NStr::fDS_NoDecimalPoint, 6, "1048KB" },
    { 1048576, NStr::fDS_NoDecimalPoint, 3, "1MB" },
    { 1048576, NStr::fDS_NoDecimalPoint, 4, "1049KB" },
    { 1048576, NStr::fDS_NoDecimalPoint, 5, "1049KB" },
    { 1048576, NStr::fDS_NoDecimalPoint, 6, "1049KB" },
    { 1572864, NStr::fDS_NoDecimalPoint, 3, "2MB" },
    { 1572864, NStr::fDS_NoDecimalPoint, 4, "1573KB" },
    { 1572864, NStr::fDS_NoDecimalPoint, 5, "1573KB" },
    { 1572864, NStr::fDS_NoDecimalPoint, 6, "1573KB" },
    { 2000, NStr::fDS_Binary, 3, "1.95KiB" },
    { 2047, NStr::fDS_Binary, 3, "2.00KiB" },
    { 2000, NStr::fDS_Binary, 6, "1.953KiB" },
    { 2047, NStr::fDS_Binary, 6, "1.999KiB" },
    { 1048575, NStr::fDS_Binary, 6, "1.00000MiB" },
    { 2000000, NStr::fDS_Binary, 6, "1.90735MiB" },
    { 2097146, NStr::fDS_Binary, 6, "1.99999MiB" },
    { 10, NStr::fConvErr_NoThrow, 3, NULL },
    { 10, NStr::fMandatorySign, 3, NULL },
    { 10, NStr::fAllowCommas, 3, NULL },
    { 10, NStr::fAllowLeadingSpaces, 3, NULL },
    { 10, NStr::fAllowLeadingSymbols, 3, NULL },
    { 10, NStr::fAllowTrailingSpaces, 3, NULL },
    { 10, NStr::fAllowTrailingSymbols, 3, NULL },
    { 10, NStr::fDecimalPosix, 3, NULL },
    { 10, NStr::fDecimalPosixOrLocal, 3, NULL },
    { 10, NStr::fIgnoreErrno, 3, NULL },
    { 10, NStr::fDS_ForceBinary, 3, NULL },
    { 10, NStr::fDS_ProhibitFractions, 3, NULL },
    { 10, NStr::fDS_ProhibitSpaceBeforeSuffix, 3, NULL }
};

BOOST_AUTO_TEST_CASE(s_NumToStringDataSize)
{
    NcbiCout << NcbiEndl << "NStr::UInt8ToString_DataSize() tests...";

    const size_t count = sizeof(s_Num2StrDataSizeTests) /
                         sizeof(s_Num2StrDataSizeTests[0]);

    for (size_t i = 0;  i < count;  ++i)
    {
        const SUint8DataSizeValues* test = &s_Num2StrDataSizeTests[i];
        Uint8 num = test->num;
        NStr::TNumToStringFlags flags = test->flags;
        unsigned int max_digits = test->max_digits;

        try {
            string value = NStr::UInt8ToString_DataSize(num, flags, max_digits);
            BOOST_CHECK(test->IsGood());
            BOOST_CHECK_EQUAL(value, test->expected);
        }
        catch (CException&) {
            if ( test->IsGood() ) {
                ERR_POST("Cannot convert " << num << " to data size string");
            }
            BOOST_CHECK(!test->IsGood());
        }
    }
}


//----------------------------------------------------------------------------
// NStr::Replace()
//----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(s_Replace)
{
    NcbiCout << NcbiEndl << "NStr::Replace() tests...";

    string src("aaabbbaaccczzcccXX");
    string dst;

    string search("ccc");
    string replace("RrR");
    NStr::Replace(src, search, replace, dst);
    BOOST_CHECK_EQUAL(dst, string("aaabbbaaRrRzzRrRXX"));

    search = "a";
    replace = "W";
    NStr::Replace(src, search, replace, dst, 6, 1);
    BOOST_CHECK_EQUAL(dst, string("aaabbbWaccczzcccXX"));
    
    search = "bbb";
    replace = "BBB";
    NStr::Replace(src, search, replace, dst, 50);
    BOOST_CHECK_EQUAL(dst, string("aaabbbaaccczzcccXX"));

    search = "ggg";
    replace = "no";
    dst = NStr::Replace(src, search, replace);
    BOOST_CHECK_EQUAL(dst, string("aaabbbaaccczzcccXX"));

    search = "a";
    replace = "A";
    dst = NStr::Replace(src, search, replace);
    BOOST_CHECK_EQUAL(dst, string("AAAbbbAAccczzcccXX"));

    search = "X";
    replace = "x";
    dst = NStr::Replace(src, search, replace, src.size() - 1);
    BOOST_CHECK_EQUAL(dst, string("aaabbbaaccczzcccXx"));

    // ReplaceInPlace

    search = "a";
    replace = "W";
    NStr::ReplaceInPlace(src, search, replace);
    BOOST_CHECK_EQUAL(src, string("WWWbbbWWccczzcccXX"));

    search = "W";
    replace = "a";
    NStr::ReplaceInPlace(src, search, replace, 2, 2);
    BOOST_CHECK_EQUAL(src, string("WWabbbaWccczzcccXX"));

    search = "a";
    replace = "bb";
    NStr::ReplaceInPlace(src, search, replace);
    BOOST_CHECK_EQUAL(src, string("WWbbbbbbbWccczzcccXX"));

    search = "bb";
    replace = "c";
    NStr::ReplaceInPlace(src, search, replace);
    BOOST_CHECK_EQUAL(src, string("WWcccbWccczzcccXX"));
}


//----------------------------------------------------------------------------
// NStr::PrintableString/ParseEscapes()
//----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(s_PrintableString)
{
    NcbiCout << NcbiEndl << "NStr::{PrintableString|ParseEscapes}() tests...";

    // NStr::PrintableString()
    BOOST_CHECK(NStr::PrintableString(kEmptyStr).empty());
    BOOST_CHECK(NStr::PrintableString
                ("AB\\CD\nAB\rCD\vAB?\tCD\'AB\"").compare
                ("AB\\\\CD\\nAB\\rCD\\vAB\?\\tCD\\\'AB\\\"") == 0);
    BOOST_CHECK(NStr::PrintableString
                ("A\x01\r\202\x000F\0205B" + string(1,'\0') + "CD").compare
                ("A\\1\\r\\202\\17\\0205B\\0CD") == 0);
    BOOST_CHECK(NStr::PrintableString
                ("A\x01\r\202\x000F\0205B" + string(1,'\0') + "CD",
                 NStr::fPrintable_Full).compare
                ("A\\001\\r\\202\\017\\0205B\\000CD") == 0);
    BOOST_CHECK(NStr::PrintableString
                ("A\nB\\\nC").compare
                ("A\\nB\\\\\\nC") == 0);
    BOOST_CHECK(NStr::PrintableString
                ("A\nB\\\nC", NStr::fNewLine_Passthru).compare
                ("A\\n\\\nB\\\\\\n\\\nC") == 0);

    // NStr::ParseEscapes
    BOOST_CHECK(NStr::ParseEscapes(kEmptyStr).empty());
    BOOST_CHECK(NStr::ParseEscapes
                ("AB\\\\CD\\nAB\\rCD\\vAB\?\\tCD\\\'AB\\\"").compare
                ("AB\\CD\nAB\rCD\vAB?\tCD\'AB\"") == 0);
    BOOST_CHECK(NStr::ParseEscapes
                ("A\\1\\r2\\17\\0205B\\x00CD\\x000F").compare
                ("A\x01\r2\x0F\x10""5B\xCD\x0F") == 0);
    BOOST_CHECK(NStr::ParseEscapes
                ("A\\\nB\\nC\\\\\\n\\\nD").compare
                ("AB\nC\\\nD") == 0);

    // NStr::PrintableString() vs. Printable()
    size_t size = (size_t)(rand() & 0xFFFF);
    string data;
    for (size_t i = 0;  i < size;  i++) {
        data += char(rand() & 0xFF);
    }
    BOOST_CHECK(data.size() == size);

    // NB: embedded '\0's are also being checked
    CNcbiOstrstream os1;
    os1 << Printable(data);
    const string& s1 = NStr::PrintableString(data);
    //NcbiCout << NStr::UInt8ToString(Uint8(s1.size())) << NcbiEndl
    //         << s1 << NcbiEndl;
    BOOST_CHECK(s1.compare(CNcbiOstrstreamToString(os1)) == 0);

    // Just to make sure it still parses okay
    BOOST_CHECK(NStr::ParseEscapes(s1).compare(data) == 0);

    // NB: checks C string (no '\0's)
    const char* str = data.c_str();
    CNcbiOstrstream os2;
    os2 << Printable(str);
    const string& s2 = NStr::PrintableString(str);
    //NcbiCout << NStr::UInt8ToString(Uint8(s2.size())) << NcbiEndl
    //         << s2 << NcbiEndl;
    BOOST_CHECK(s2.compare(CNcbiOstrstreamToString(os2)) == 0);

}


//----------------------------------------------------------------------------
// NStr::Compare()
//----------------------------------------------------------------------------

static void s_CompareStr(int expr_res, int valid_res)
{
    int res = expr_res > 0 ? 1 :
        expr_res == 0 ? 0 : -1;
    BOOST_CHECK_EQUAL(res, valid_res);
}

struct SStrCompare
{
    const char* s1;
    const char* s2;

    int case_res;      /* -1, 0, 1 */
    int nocase_res;    /* -1, 0, 1 */

    SIZE_TYPE n; 
    int n_case_res;    /* -1, 0, 1 */
    int n_nocase_res;  /* -1, 0, 1 */
};

static const SStrCompare s_StrCompare[] = {
    { "", "",  0, 0,  0,     0, 0 },
    { "", "",  0, 0,  NPOS,  0, 0 },
    { "", "",  0, 0,  10,    0, 0 },
    { "", "",  0, 0,  1,     0, 0 },

    { "a", "",  1, 1,  0,     0, 0 },
    { "a", "",  1, 1,  1,     1, 1 },
    { "a", "",  1, 1,  2,     1, 1 },
    { "a", "",  1, 1,  NPOS,  1, 1 },

    { "", "bb",  -1, -1,  0,     -1, -1 },
    { "", "bb",  -1, -1,  1,     -1, -1 },
    { "", "bb",  -1, -1,  2,     -1, -1 },
    { "", "bb",  -1, -1,  3,     -1, -1 },
    { "", "bb",  -1, -1,  NPOS,  -1, -1 },

    { "ba", "bb",  -1, -1,  0,     -1, -1 },
    { "ba", "bb",  -1, -1,  1,     -1, -1 },
    { "ba", "b",    1,  1,  1,      0,  0 },
    { "ba", "bb",  -1, -1,  2,     -1, -1 },
    { "ba", "bb",  -1, -1,  3,     -1, -1 },
    { "ba", "bb",  -1, -1,  NPOS,  -1, -1 },

    { "a", "A",  1, 0,  0,    -1, -1 },
    { "a", "A",  1, 0,  1,     1,  0 },
    { "a", "A",  1, 0,  2,     1,  0 },
    { "a", "A",  1, 0,  NPOS,  1,  0 },

    { "A", "a",  -1, 0,  0,     -1, -1 },
    { "A", "a",  -1, 0,  1,     -1,  0 },
    { "A", "a",  -1, 0,  2,     -1,  0 },
    { "A", "a",  -1, 0,  NPOS,  -1,  0 },

    { "ba", "ba1",  -1, -1,  0,     -1, -1 },
    { "ba", "ba1",  -1, -1,  1,     -1, -1 },
    { "ba", "ba1",  -1, -1,  2,     -1, -1 },
    { "bA", "ba",   -1,  0,  2,     -1,  0 },
    { "ba", "ba1",  -1, -1,  3,     -1, -1 },
    { "ba", "ba1",  -1, -1,  NPOS,  -1, -1 },

    { "ba1", "ba",  1, 1,  0,    -1, -1 },
    { "ba1", "ba",  1, 1,  1,    -1, -1 },
    { "ba1", "ba",  1, 1,  2,     0,  0 },
    { "ba",  "bA",  1, 0,  2,     1,  0 },
    { "ba1", "ba",  1, 1,  3,     1,  1 },
    { "ba1", "ba",  1, 1,  NPOS,  1,  1 },
    { "ba1", "ba",  1, 1,  NPOS,  1,  1 }
};

BOOST_AUTO_TEST_CASE(s_Compare)
{
    NcbiCout << NcbiEndl << "NStr::Compare() tests...";

    const SStrCompare* rec;
    const size_t count = sizeof(s_StrCompare) / sizeof(s_StrCompare[0]);

    for (size_t i = 0;  i < count;  i++) {
        rec = &s_StrCompare[i];

        string s1 = rec->s1;
        s_CompareStr(NStr::Compare(s1, rec->s2, NStr::eCase), rec->case_res);
        s_CompareStr(NStr::Compare(s1, rec->s2, NStr::eNocase),
                     rec->nocase_res);
        s_CompareStr(NStr::Compare(s1, 0, rec->n, rec->s2, NStr::eCase),
                     rec->n_case_res);
        s_CompareStr(NStr::Compare(s1, 0, rec->n, rec->s2, NStr::eNocase),
                     rec->n_nocase_res);

        string s2 = rec->s2;
        s_CompareStr(NStr::Compare(s1, s2, NStr::eCase), rec->case_res);
        s_CompareStr(NStr::Compare(s1, s2, NStr::eNocase), rec->nocase_res);
        s_CompareStr(NStr::Compare(s1, 0, rec->n, s2, NStr::eCase),
                     rec->n_case_res);
        s_CompareStr(NStr::Compare(s1, 0, rec->n, s2, NStr::eNocase),
                     rec->n_nocase_res);
    }

    BOOST_CHECK(NStr::Compare("0123", 0, 2, "12") <  0);
    BOOST_CHECK(NStr::Compare("0123", 1, 2, "12") == 0);
    BOOST_CHECK(NStr::Compare("0123", 2, 2, "12") >  0);
    BOOST_CHECK(NStr::Compare("0123", 3, 2,  "3") == 0);
}

BOOST_AUTO_TEST_CASE(s_XCompare)
{
    NcbiCout << NcbiEndl << "XStr::Compare() tests...";

    const SStrCompare* rec;
    const size_t count = sizeof(s_StrCompare) / sizeof(s_StrCompare[0]);

    for (size_t i = 0;  i < count;  i++) {
        rec = &s_StrCompare[i];

        string s1 = rec->s1;
        s_CompareStr(XStr::Compare(s1, rec->s2, XStr::eCase),   rec->case_res);
        s_CompareStr(XStr::Compare(s1, rec->s2, XStr::eNocase),
                     rec->nocase_res);
        s_CompareStr(XStr::Compare(s1, 0, rec->n, rec->s2, XStr::eCase),
                     rec->n_case_res);
        s_CompareStr(XStr::Compare(s1, 0, rec->n, rec->s2, XStr::eNocase),
                     rec->n_nocase_res);

        string s2 = rec->s2;
        s_CompareStr(XStr::Compare(s1, s2, XStr::eCase), rec->case_res);
        s_CompareStr(XStr::Compare(s1, s2, XStr::eNocase), rec->nocase_res);
        s_CompareStr(XStr::Compare(s1, 0, rec->n, s2, XStr::eCase),
                     rec->n_case_res);
        s_CompareStr(XStr::Compare(s1, 0, rec->n, s2, XStr::eNocase),
                     rec->n_nocase_res);
    }

    BOOST_CHECK(XStr::Compare("0123", 0, 2, "12") <  0);
    BOOST_CHECK(XStr::Compare("0123", 1, 2, "12") == 0);
    BOOST_CHECK(XStr::Compare("0123", 2, 2, "12") >  0);
    BOOST_CHECK(XStr::Compare("0123", 3, 2,  "3") == 0);
}


//----------------------------------------------------------------------------
// NStr::Split()
//----------------------------------------------------------------------------

static const string s_SplitStr[] = {
    "ab+cd+ef",
    "aaAAabBbbb",
    "-abc-def--ghijk---",
    "a12c3ba45acb678bc",
    "nodelim",
    "emptydelim",
    ";",
    ""
};

static const string s_SplitDelim[] = {
    "+", "AB", "-", "abc", "*", "", ";", "*"
};

static const string split_result[] = {
    "ab", "cd", "ef",
    "aa", "ab", "bbb",
    "abc", "def", "ghijk",
    "12", "3", "45", "678",
    "nodelim",
    "emptydelim"
};

BOOST_AUTO_TEST_CASE(s_Split)
{
    NcbiCout << NcbiEndl << "NStr::Split() tests...";

    list<string> split;

    for (size_t i = 0; i < sizeof(s_SplitStr) / sizeof(s_SplitStr[0]); i++) {
        NStr::Split(s_SplitStr[i], s_SplitDelim[i], split);
    }
    size_t j = 0;
    ITERATE(list<string>, it, split) {
        BOOST_CHECK(j < sizeof(split_result) / sizeof(split_result[0]));
        BOOST_CHECK(NStr::Compare(*it, split_result[j++]) == 0);
    }
}


//----------------------------------------------------------------------------
// NStr::Tokenize()
//----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(s_Tokenize)
{
    NcbiCout << NcbiEndl << "NStr::Tokenize() tests...";

    static const string s_TokStr[] = {
        "ab+cd+ef",
        "123;45,78",
        "1;",
        ";1",
        "emptydelim"
    };
    static const string s_TokDelim[] = {
        "+", ";,", ";", ";", ""
    };
    static const string tok_result[] = {
        "ab", "cd", "ef",
        "123", "45", "78",
        "1", "", 
        "", "1",
        "emptydelim"
    };

    vector<string> tok;

    for (size_t i = 0; i < sizeof(s_TokStr) / sizeof(s_TokStr[0]); ++i) {
        NStr::Tokenize(s_TokStr[i], s_TokDelim[i], tok);               
    }
    {{
        size_t i = 0;
        ITERATE(vector<string>, it, tok) {
            BOOST_CHECK_EQUAL(NStr::Compare(*it, tok_result[i++]), 0);
        }
    }}
    
    tok.clear();

    for (size_t i = 0; i < sizeof(s_SplitStr) / sizeof(s_SplitStr[0]); i++) {
        NStr::Tokenize(s_SplitStr[i], s_SplitDelim[i], tok,
                       NStr::eMergeDelims);
    }
    {{
        size_t i = 0;
        ITERATE(vector<string>, it, tok) {
            BOOST_CHECK_EQUAL(NStr::Compare(*it, split_result[i++]), 0);
        }
    }}
}


//----------------------------------------------------------------------------
// NStr::SplitInTo()
//----------------------------------------------------------------------------

struct SSplitInTwo {
    const char*  str;
    const char*  delim;
    NStr::EMergeDelims merge;
    const char*  expected_str1;
    const char*  expected_str2;
    bool         expected_ret;
};
    
static const SSplitInTwo s_SplitInTwoTest[] = {
    { "ab+cd+ef",    "+",      NStr::eNoMergeDelims, "ab", "cd+ef",     true },
    { "ab+cd+ef",    "+",      NStr::eMergeDelims,   "ab", "cd+ef",     true },
    { "ab+++cd+ef",  "+",      NStr::eMergeDelims,   "ab", "cd+ef",     true },
    { "ab+++",       "+",      NStr::eMergeDelims,   "ab", "",          true },
    { "aaAAabBbbb",  "AB",     NStr::eNoMergeDelims, "aa", "AabBbbb",   true },
    { "aaABAabBbbb", "AB",     NStr::eMergeDelims,   "aa", "abBbbb",    true },
    { "aaCAabBCbbb", "ABC",    NStr::eNoMergeDelims, "aa", "AabBCbbb",  true },
    { "-beg-delim-", "-",      NStr::eNoMergeDelims, "",   "beg-delim-",true },
    { "end-delim:",  ":",      NStr::eNoMergeDelims, "end-delim",  "",  true },
    { "nodelim",     ".,:;-+", NStr::eNoMergeDelims, "nodelim",    "",  false },
    { "emptydelim",  "",       NStr::eNoMergeDelims, "emptydelim", "",  false },
    { "", "emtpystring", NStr::eNoMergeDelims, "", "", false },
    { "", "", NStr::eNoMergeDelims, "", "", false }
};

BOOST_AUTO_TEST_CASE(s_SplitInTwo)
{
    NcbiCout << NcbiEndl << "NStr::SplitInTwo() tests...";

    string string1, string2;
    bool   result;
    const size_t count = sizeof(s_SplitInTwoTest) /
                         sizeof(s_SplitInTwoTest[0]);
    for (size_t i = 0; i < count; i++) {
        result = NStr::SplitInTwo(s_SplitInTwoTest[i].str,
                                  s_SplitInTwoTest[i].delim,
                                  string1, string2, s_SplitInTwoTest[i].merge);
        BOOST_CHECK_EQUAL(s_SplitInTwoTest[i].expected_ret, result);
        BOOST_CHECK_EQUAL(s_SplitInTwoTest[i].expected_str1, string1);
        BOOST_CHECK_EQUAL(s_SplitInTwoTest[i].expected_str2, string2);
    }
}


//----------------------------------------------------------------------------
// NStr::TokenizePattern()
//----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(s_TokenizePattern)
{
    NcbiCout << NcbiEndl << "NStr::TokenizePattern() tests...";

    static const string tok_pattern_str =
        "<tag>begin<tag>text<tag><tag><tag>end<tag>";
    static const string tok_pattern_delim = "<tag>";
    static const string tok_pattern_result_m[] = {
        "begin", "text", "end"
    };
    static const string tok_pattern_result_nm[] = {
        "", "begin", "text",  "", "", "end", ""
    };

    vector<string> tok;

    NStr::TokenizePattern(tok_pattern_str, tok_pattern_delim,
                          tok, NStr::eMergeDelims);
    {{
        BOOST_CHECK_EQUAL(sizeof(tok_pattern_result_m)/sizeof(tok_pattern_result_m[0]),
               tok.size());
        size_t i = 0;
        ITERATE(vector<string>, it, tok) {
            BOOST_CHECK_EQUAL(NStr::Compare(*it, tok_pattern_result_m[i++]), 0);
        }
    }}
    tok.clear();

    NStr::TokenizePattern(tok_pattern_str, tok_pattern_delim,
                          tok, NStr::eNoMergeDelims);
    {{
        BOOST_CHECK_EQUAL(sizeof(tok_pattern_result_nm)/sizeof(tok_pattern_result_nm[0]),
               tok.size());
        size_t i = 0;
        ITERATE(vector<string>, it, tok) {
            BOOST_CHECK_EQUAL(NStr::Compare(*it, tok_pattern_result_nm[i++]), 0);
        }
    }}
}


//----------------------------------------------------------------------------
// NStr::ToLower/ToUpper()
//----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(s_Case)
{
    NcbiCout << NcbiEndl << "NStr::ToLower/ToUpper() tests...";

    static const struct {
        const char* orig;
        const char* x_lower;
        const char* x_upper;
    } s_Tri[] = {
        { "", "", "" },
        { "a", "a", "A" },
        { "4", "4", "4" },
        { "B5a", "b5a", "B5A" },
        { "baObaB", "baobab", "BAOBAB" },
        { "B", "b", "B" },
        { "B", "b", "B" }
    };
    static const char s_Indiff[] =
        "#@+_)(*&^%/?\"':;~`'\\!\v|=-0123456789.,><{}[]\t\n\r";

    {{
        char indiff[sizeof(s_Indiff) + 1];
        ::strcpy(indiff, s_Indiff);
        BOOST_CHECK_EQUAL(NStr::Compare(s_Indiff, indiff), 0);
        BOOST_CHECK_EQUAL(NStr::Compare(s_Indiff, NStr::ToLower(indiff)), 0);
        ::strcpy(indiff, s_Indiff);
        BOOST_CHECK_EQUAL(NStr::Compare(s_Indiff, NStr::ToUpper(indiff)), 0);
        BOOST_CHECK_EQUAL(NStr::Compare(s_Indiff, NStr::ToLower(indiff)), 0);
    }}
    {{
        string indiff;
        indiff = s_Indiff;
        BOOST_CHECK_EQUAL(NStr::Compare(s_Indiff, indiff), 0);
        BOOST_CHECK_EQUAL(NStr::Compare(s_Indiff, NStr::ToLower(indiff)), 0);
        indiff = s_Indiff;
        BOOST_CHECK_EQUAL(NStr::Compare(s_Indiff, NStr::ToUpper(indiff)), 0);
        BOOST_CHECK_EQUAL(NStr::Compare(s_Indiff, NStr::ToLower(indiff)), 0);
    }}

    for (size_t i = 0;  i < sizeof(s_Tri) / sizeof(s_Tri[0]);  i++) {
        BOOST_CHECK_EQUAL(NStr::Compare(s_Tri[i].orig, s_Tri[i].x_lower, NStr::eNocase),0);
        BOOST_CHECK_EQUAL(NStr::Compare(s_Tri[i].orig, s_Tri[i].x_upper, NStr::eNocase),0);
        string orig = s_Tri[i].orig;
        BOOST_CHECK_EQUAL(NStr::Compare(orig, s_Tri[i].x_lower, NStr::eNocase), 0);
        BOOST_CHECK_EQUAL(NStr::Compare(orig, s_Tri[i].x_upper, NStr::eNocase), 0);
        string x_lower = s_Tri[i].x_lower;
        {{
            char x_str[16];
            ::strcpy(x_str, s_Tri[i].orig);
            BOOST_CHECK(::strlen(x_str) < sizeof(x_str));
            BOOST_CHECK_EQUAL(NStr::Compare(NStr::ToLower(x_str), x_lower), 0);
            ::strcpy(x_str, s_Tri[i].orig);
            BOOST_CHECK_EQUAL(NStr::Compare(NStr::ToUpper(x_str), s_Tri[i].x_upper),0);
            BOOST_CHECK_EQUAL(NStr::Compare(x_lower, NStr::ToLower(x_str)), 0);
        }}
        {{
            string x_str;
            x_lower = s_Tri[i].x_lower;
            x_str = s_Tri[i].orig;
            BOOST_CHECK_EQUAL(NStr::Compare(NStr::ToLower(x_str), x_lower), 0);
            x_str = s_Tri[i].orig;
            BOOST_CHECK_EQUAL(NStr::Compare(NStr::ToUpper(x_str), s_Tri[i].x_upper),0);
            BOOST_CHECK_EQUAL(NStr::Compare(x_lower, NStr::ToLower(x_str)), 0);
        }}
    }
}


//----------------------------------------------------------------------------
// NStr::str[n]casecmp()
//----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(s_strcasecmp)
{
    NcbiCout << NcbiEndl << "NStr::str[n]casecmp() tests...";

    BOOST_CHECK_EQUAL(NStr::strncasecmp("ab", "a", 1), 0);
    BOOST_CHECK_EQUAL(NStr::strncasecmp("Ab", "a", 1), 0);
    BOOST_CHECK_EQUAL(NStr::strncasecmp("a", "Ab", 1), 0);
    BOOST_CHECK_EQUAL(NStr::strncasecmp("a", "ab", 1), 0);

    BOOST_CHECK_EQUAL(NStr::strcasecmp("a",  "A"), 0);
    BOOST_CHECK_EQUAL(NStr::strcasecmp("a",  "a"), 0);
    BOOST_CHECK(NStr::strcasecmp("ab", "a") != 0);
    BOOST_CHECK(NStr::strcasecmp("a", "ab") != 0);
    BOOST_CHECK(NStr::strcasecmp("a",   "") != 0);
    BOOST_CHECK(NStr::strcasecmp("",   "a") != 0);
    BOOST_CHECK_EQUAL(NStr::strcasecmp("",    ""), 0);
}


//----------------------------------------------------------------------------
// NStr::AStrEquiv()  &   NStr::Equal*()
//----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(s_Equal)
{
    string as1("abcdefg ");
    string as2("abcdefg ");
    string as3("aBcdEfg ");
    string as4("lsekfu");

    NcbiCout << NcbiEndl << "AStrEquiv tests...";

    BOOST_CHECK_EQUAL( AStrEquiv(as1, as2, PNocase()), true );
    BOOST_CHECK_EQUAL( AStrEquiv(as1, as3, PNocase()), true );
    BOOST_CHECK_EQUAL( AStrEquiv(as3, as4, PNocase()), false );
    BOOST_CHECK_EQUAL( AStrEquiv(as1, as2, PCase()),   true );
    BOOST_CHECK_EQUAL( AStrEquiv(as1, as3, PCase()),   false );
    BOOST_CHECK_EQUAL( AStrEquiv(as2, as4, PCase()),   false );

    NcbiCout << NcbiEndl << "Equal{Case,Nocase} tests...";

    BOOST_CHECK_EQUAL( NStr::EqualNocase(as1, as2), true );
    BOOST_CHECK_EQUAL( NStr::EqualNocase(as1, as3), true );
    BOOST_CHECK_EQUAL( NStr::EqualNocase(as3, as4), false );
    BOOST_CHECK_EQUAL( NStr::EqualCase(as1, as2),   true );
    BOOST_CHECK_EQUAL( NStr::EqualCase(as1, as3),   false );
    BOOST_CHECK_EQUAL( NStr::EqualCase(as2, as4),   false );
}


//----------------------------------------------------------------------------
// Reference counting
//----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(s_ReferenceCounting)
{
    NcbiCout << NcbiEndl 
             << "Testing string reference counting properties:"
             << NcbiEndl;

    string s1(10, '1');
    string s2(s1);
    if ( s1.data() != s2.data() ) {
        NcbiCout << "BAD: string reference counting is OFF" << NcbiEndl;
    }
    else {
        NcbiCout << "GOOD: string reference counting is ON" << NcbiEndl;
        for (size_t i = 0; i < 4; i++) {
            NcbiCout << "Restoring reference counting"<<NcbiEndl;
            s2 = s1;
            if ( s1.data() != s2.data() ) {
                NcbiCout << "BAD: cannot restore string reference " \
                    "counting" << NcbiEndl;
                continue;
            }
            NcbiCout << "GOOD: reference counting is ON" << NcbiEndl;

            const char* type = i&1? "str.begin()": "str.c_str()";
            NcbiCout << "Calling "<<type<<NcbiEndl;
            if ( i&1 ) {
                s2.begin();
            }
            else {
                s1.c_str();
            }
            if ( s1.data() == s2.data() ) {
                NcbiCout << "GOOD: "<< type 
                            <<" doesn't affect reference counting"<< NcbiEndl;
                continue;
            }
            NcbiCout << "OK: "<< type 
                        << " turns reference counting OFF" << NcbiEndl;

            NcbiCout << "Restoring reference counting"<<NcbiEndl;
            s2 = s1;
            if ( s1.data() != s2.data() ) {
                NcbiCout << "BAD: " << type 
                            <<" turns reference counting OFF completely"
                            << NcbiEndl;
                continue;
            }
            NcbiCout << "GOOD: reference counting is ON" << NcbiEndl;

            if ( i&1 ) continue;

            NcbiCout << "Calling " << type << " on source" << NcbiEndl;
            s1.c_str();
            if ( s1.data() != s2.data() ) {
                NcbiCout << "BAD: "<< type
                            << " on source turns reference counting OFF"
                            << NcbiEndl;
            }

            NcbiCout << "Calling "<< type <<" on destination" << NcbiEndl;
            s2.c_str();
            if ( s1.data() != s2.data() ) {
                NcbiCout << "BAD: " << type
                         <<" on destination turns reference counting OFF"
                         << NcbiEndl;
            }
        }
    }
}


//----------------------------------------------------------------------------
// NStr::FindNoCase()
//----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(s_FindNoCase)
{
    NcbiCout << NcbiEndl << "NStr::FindNoCase() tests...";
    BOOST_CHECK_EQUAL(NStr::FindNoCase(" abcd", " xyz"), NPOS);
    BOOST_CHECK_EQUAL(NStr::FindNoCase(" abcd", " xyz", 0, NPOS, NStr::eLast), NPOS);
    BOOST_CHECK(NStr::FindNoCase(" abcd", " aBc", 0, NPOS, NStr::eLast) == 0);
}


//----------------------------------------------------------------------------
// CVersionInfo:: parse from str
//----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(s_VersionInfo)
{
    NcbiCout << NcbiEndl << "CVersionInfo::FromStr tests...";
    {{
        CVersionInfo ver("1.2.3");
        BOOST_CHECK_EQUAL(ver.GetMajor(), 1);
        BOOST_CHECK_EQUAL(ver.GetMinor(), 2);
        BOOST_CHECK_EQUAL(ver.GetPatchLevel(), 3);

        ver.FromStr("12.35");
        BOOST_CHECK_EQUAL(ver.GetMajor(), 12);
        BOOST_CHECK_EQUAL(ver.GetMinor(), 35);
        BOOST_CHECK_EQUAL(ver.GetPatchLevel(), 0);

        BOOST_CHECK_THROW( ver.FromStr("12.35a"), exception);
    }}

    NcbiCout << NcbiEndl << "ParseVersionString tests...";

    static const struct {
        const char* str;
        const char* name;
        int         ver_major;
        int         ver_minor;
        int         patch_level;
    } s_VerInfo[] = {
        { "1.3.2",                      "",                  1, 3, 2 },
        { "My_Program21p32c 1.3.3",     "My_Program21p32c",  1, 3, 3 },
        { "2.3.4 ( program)",           "program",           2, 3, 4 },
        { "version 50.1.0",             "",                 50, 1, 0 },
        { "MyProgram version 50.2.1",   "MyProgram",        50, 2, 1 },
        { "MyProgram ver. 50.3.1",      "MyProgram",        50, 3, 1 },
        { "MyOtherProgram2 ver 51.3.1", "MyOtherProgram2",  51, 3, 1 },
        { "Program_ v. 1.3.1",          "Program_",          1, 3, 1 }
    };

    CVersionInfo ver("1.2.3");
    string       name;

    for (size_t i = 0;  i < sizeof(s_VerInfo) / sizeof(s_VerInfo[0]);  i++) {
        ParseVersionString(s_VerInfo[i].str, &name, &ver);
        BOOST_CHECK_EQUAL(name,                s_VerInfo[i].name);
        BOOST_CHECK_EQUAL(ver.GetMajor(),      s_VerInfo[i].ver_major);
        BOOST_CHECK_EQUAL(ver.GetMinor(),      s_VerInfo[i].ver_minor);
        BOOST_CHECK_EQUAL(ver.GetPatchLevel(), s_VerInfo[i].patch_level);
    }
    ParseVersionString("MyProgram ", &name, &ver);
    BOOST_CHECK_EQUAL(name, string("MyProgram"));
    BOOST_CHECK(ver.IsAny());
}

BOOST_AUTO_TEST_CASE(s_StringUTF8)
{
    const unsigned char s_ExtAscii[] = {
        0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f,
0x10,0x11,0x12,0x13,0x14,0x15,0x16,0x17,0x18,0x19,0x1a,0x1b,0x1c,0x1d,0x1e,0x1f,
0x20,0x21,0x22,0x23,0x24,0x25,0x26,0x27,0x28,0x29,0x2a,0x2b,0x2c,0x2d,0x2e,0x2f,
0x30,0x31,0x32,0x33,0x34,0x35,0x36,0x37,0x38,0x39,0x3a,0x3b,0x3c,0x3d,0x3e,0x3f,
0x40,0x41,0x42,0x43,0x44,0x45,0x46,0x47,0x48,0x49,0x4a,0x4b,0x4c,0x4d,0x4e,0x4f,
0x50,0x51,0x52,0x53,0x54,0x55,0x56,0x57,0x58,0x59,0x5a,0x5b,0x5c,0x5d,0x5e,0x5f,
0x60,0x61,0x62,0x63,0x64,0x65,0x66,0x67,0x68,0x69,0x6a,0x6b,0x6c,0x6d,0x6e,0x6f,
0x70,0x71,0x72,0x73,0x74,0x75,0x76,0x77,0x78,0x79,0x7a,0x7b,0x7c,0x7d,0x7e,0x7f,
0x80,0x81,0x82,0x83,0x84,0x85,0x86,0x87,0x88,0x89,0x8a,0x8b,0x8c,0x8d,0x8e,0x8f,
0x90,0x91,0x92,0x93,0x94,0x95,0x96,0x97,0x98,0x99,0x9a,0x9b,0x9c,0x9d,0x9e,0x9f,
0xa0,0xa1,0xa2,0xa3,0xa4,0xa5,0xa6,0xa7,0xa8,0xa9,0xaa,0xab,0xac,0xad,0xae,0xaf,
0xb0,0xb1,0xb2,0xb3,0xb4,0xb5,0xb6,0xb7,0xb8,0xb9,0xba,0xbb,0xbc,0xbd,0xbe,0xbf,
0xc0,0xc1,0xc2,0xc3,0xc4,0xc5,0xc6,0xc7,0xc8,0xc9,0xca,0xcb,0xcc,0xcd,0xce,0xcf,
0xd0,0xd1,0xd2,0xd3,0xd4,0xd5,0xd6,0xd7,0xd8,0xd9,0xda,0xdb,0xdc,0xdd,0xde,0xdf,
0xe0,0xe1,0xe2,0xe3,0xe4,0xe5,0xe6,0xe7,0xe8,0xe9,0xea,0xeb,0xec,0xed,0xee,0xef,
0xf0,0xf1,0xf2,0xf3,0xf4,0xf5,0xf6,0xf7,0xf8,0xf9,0xfa,0xfb,0xfc,0xfd,0xfe,0xff,0x00};
    const unsigned char s_Converted[] = {
0xc2,0x80,0xc2,0x81,0xc2,0x82,0xc2,0x83,0xc2,0x84,0xc2,0x85,0xc2,0x86,0xc2,0x87,
0xc2,0x88,0xc2,0x89,0xc2,0x8a,0xc2,0x8b,0xc2,0x8c,0xc2,0x8d,0xc2,0x8e,0xc2,0x8f,
0xc2,0x90,0xc2,0x91,0xc2,0x92,0xc2,0x93,0xc2,0x94,0xc2,0x95,0xc2,0x96,0xc2,0x97,
0xc2,0x98,0xc2,0x99,0xc2,0x9a,0xc2,0x9b,0xc2,0x9c,0xc2,0x9d,0xc2,0x9e,0xc2,0x9f,
0xc2,0xa0,0xc2,0xa1,0xc2,0xa2,0xc2,0xa3,0xc2,0xa4,0xc2,0xa5,0xc2,0xa6,0xc2,0xa7,
0xc2,0xa8,0xc2,0xa9,0xc2,0xaa,0xc2,0xab,0xc2,0xac,0xc2,0xad,0xc2,0xae,0xc2,0xaf,
0xc2,0xb0,0xc2,0xb1,0xc2,0xb2,0xc2,0xb3,0xc2,0xb4,0xc2,0xb5,0xc2,0xb6,0xc2,0xb7,
0xc2,0xb8,0xc2,0xb9,0xc2,0xba,0xc2,0xbb,0xc2,0xbc,0xc2,0xbd,0xc2,0xbe,0xc2,0xbf,
0xc3,0x80,0xc3,0x81,0xc3,0x82,0xc3,0x83,0xc3,0x84,0xc3,0x85,0xc3,0x86,0xc3,0x87,
0xc3,0x88,0xc3,0x89,0xc3,0x8a,0xc3,0x8b,0xc3,0x8c,0xc3,0x8d,0xc3,0x8e,0xc3,0x8f,
0xc3,0x90,0xc3,0x91,0xc3,0x92,0xc3,0x93,0xc3,0x94,0xc3,0x95,0xc3,0x96,0xc3,0x97,
0xc3,0x98,0xc3,0x99,0xc3,0x9a,0xc3,0x9b,0xc3,0x9c,0xc3,0x9d,0xc3,0x9e,0xc3,0x9f,
0xc3,0xa0,0xc3,0xa1,0xc3,0xa2,0xc3,0xa3,0xc3,0xa4,0xc3,0xa5,0xc3,0xa6,0xc3,0xa7,
0xc3,0xa8,0xc3,0xa9,0xc3,0xaa,0xc3,0xab,0xc3,0xac,0xc3,0xad,0xc3,0xae,0xc3,0xaf,
0xc3,0xb0,0xc3,0xb1,0xc3,0xb2,0xc3,0xb3,0xc3,0xb4,0xc3,0xb5,0xc3,0xb6,0xc3,0xb7,
0xc3,0xb8,0xc3,0xb9,0xc3,0xba,0xc3,0xbb,0xc3,0xbc,0xc3,0xbd,0xc3,0xbe,0xc3,0xbf};

    NcbiCout << NcbiEndl << "CStringUTF8 tests...";

    const char *src, *res, *conv;
    src = (char*)s_ExtAscii;
    conv= (char*)s_Converted;
    CStringUTF8 str(src);
    res = str.c_str();

    BOOST_CHECK_EQUAL( strncmp(src,res,126), 0);
    BOOST_CHECK( strlen(res+127) == 256 );
    res += 127;
    BOOST_CHECK_EQUAL( strncmp(conv,res,256), 0);

    string sample("micro=� Agrave=� atilde=� ccedil=�");
    str = sample;
    BOOST_CHECK_EQUAL(strcmp(str.c_str(),"micro=µ Agrave=À atilde=ã ccedil=ç"), 0);
    BOOST_CHECK_EQUAL( str.AsLatin1(), sample);

    BOOST_CHECK_EQUAL(128u, CStringUTF8::GetValidSymbolCount((const char*)s_Converted, sizeof(s_Converted)));
    BOOST_CHECK_EQUAL(127u, CStringUTF8::GetValidSymbolCount((const char*)s_ExtAscii));
    BOOST_CHECK_EQUAL(34u,  CStringUTF8::GetValidSymbolCount(str.data(), str.length()));
    BOOST_CHECK_EQUAL(34u,  CStringUTF8::GetValidSymbolCount(str));

    BOOST_CHECK_EQUAL(256u, CStringUTF8::GetValidBytesCount((const char*)s_Converted, sizeof(s_Converted)));
    BOOST_CHECK_EQUAL(127u, CStringUTF8::GetValidBytesCount((const char*)s_ExtAscii));
    BOOST_CHECK_EQUAL(38u,  CStringUTF8::GetValidBytesCount(str.data(), str.length()));
    BOOST_CHECK_EQUAL(38u,  CStringUTF8::GetValidBytesCount(str));

    BOOST_CHECK_EQUAL( CStringUTF8::StringToEncoding("UtF-8"),        eEncoding_UTF8);
    BOOST_CHECK_EQUAL( CStringUTF8::StringToEncoding("Windows-1252"), eEncoding_Windows_1252);
    BOOST_CHECK_EQUAL( CStringUTF8::StringToEncoding("cp367"),        eEncoding_Ascii);
    BOOST_CHECK_EQUAL( CStringUTF8::StringToEncoding("csISOLatin1"),  eEncoding_ISO8859_1);
    BOOST_CHECK_EQUAL( CStringUTF8::StringToEncoding("ISO-2022-CN-EXT"), eEncoding_Unknown);
}

BOOST_AUTO_TEST_CASE(s_TruncateSpaces)
{
    const char* szEmpty     = "";
    const char* szSpaces    = "  \t\n  \t\n  \t\n";
    const char* szTrunc     = "some long\tmultiline\nstring";
    const char* szBegSpace  = "  \t\nsome long\tmultiline\nstring";
    const char* szEndSpace  = "some long\tmultiline\nstring  \t\n";
    const char* szBothSpace = "  \t\nsome long\tmultiline\nstring  \t\n";

    const string sEmpty     = szEmpty;
    const string sSpaces    = szSpaces;
    const string sTrunc     = szTrunc;
    const string sBegSpace  = szBegSpace;
    const string sEndSpace  = szEndSpace;
    const string sBothSpace = szBothSpace;

    const CTempString tsEmpty    (szEmpty    );
    const CTempString tsSpaces   (szSpaces   );
    const CTempString tsTrunc    (szTrunc    );
    const CTempString tsBegSpace (szBegSpace );
    const CTempString tsEndSpace (szEndSpace );
    const CTempString tsBothSpace(szBothSpace);

    BOOST_CHECK_EQUAL( NStr::TruncateSpaces(szEmpty,     NStr::eTrunc_Begin), tsEmpty    );
    BOOST_CHECK_EQUAL( NStr::TruncateSpaces(szEmpty,     NStr::eTrunc_End  ), tsEmpty    );
    BOOST_CHECK_EQUAL( NStr::TruncateSpaces(szEmpty,     NStr::eTrunc_Both ), tsEmpty    );
    BOOST_CHECK_EQUAL( NStr::TruncateSpaces(tsEmpty,     NStr::eTrunc_Begin), tsEmpty    );
    BOOST_CHECK_EQUAL( NStr::TruncateSpaces(tsEmpty,     NStr::eTrunc_End  ), tsEmpty    );
    BOOST_CHECK_EQUAL( NStr::TruncateSpaces(tsEmpty,     NStr::eTrunc_Both ), tsEmpty    );
    BOOST_CHECK_EQUAL( NStr::TruncateSpaces(sEmpty,      NStr::eTrunc_Begin), sEmpty     );
    BOOST_CHECK_EQUAL( NStr::TruncateSpaces(sEmpty,      NStr::eTrunc_End  ), sEmpty     );
    BOOST_CHECK_EQUAL( NStr::TruncateSpaces(sEmpty,      NStr::eTrunc_Both ), sEmpty     );

    BOOST_CHECK_EQUAL( NStr::TruncateSpaces(szSpaces,    NStr::eTrunc_Begin), tsEmpty    );
    BOOST_CHECK_EQUAL( NStr::TruncateSpaces(szSpaces,    NStr::eTrunc_End  ), tsEmpty    );
    BOOST_CHECK_EQUAL( NStr::TruncateSpaces(szSpaces,    NStr::eTrunc_Both ), tsEmpty    );
    BOOST_CHECK_EQUAL( NStr::TruncateSpaces(tsSpaces,    NStr::eTrunc_Begin), tsEmpty    );
    BOOST_CHECK_EQUAL( NStr::TruncateSpaces(tsSpaces,    NStr::eTrunc_End  ), tsEmpty    );
    BOOST_CHECK_EQUAL( NStr::TruncateSpaces(tsSpaces,    NStr::eTrunc_Both ), tsEmpty    );
    BOOST_CHECK_EQUAL( NStr::TruncateSpaces(sSpaces,     NStr::eTrunc_Begin), sEmpty     );
    BOOST_CHECK_EQUAL( NStr::TruncateSpaces(sSpaces,     NStr::eTrunc_End  ), sEmpty     );
    BOOST_CHECK_EQUAL( NStr::TruncateSpaces(sSpaces,     NStr::eTrunc_Both ), sEmpty     );

    BOOST_CHECK_EQUAL( NStr::TruncateSpaces(szTrunc,     NStr::eTrunc_Begin), tsTrunc    );
    BOOST_CHECK_EQUAL( NStr::TruncateSpaces(szTrunc,     NStr::eTrunc_End  ), tsTrunc    );
    BOOST_CHECK_EQUAL( NStr::TruncateSpaces(szTrunc,     NStr::eTrunc_Both ), tsTrunc    );
    BOOST_CHECK_EQUAL( NStr::TruncateSpaces(tsTrunc,     NStr::eTrunc_Begin), tsTrunc    );
    BOOST_CHECK_EQUAL( NStr::TruncateSpaces(tsTrunc,     NStr::eTrunc_End  ), tsTrunc    );
    BOOST_CHECK_EQUAL( NStr::TruncateSpaces(tsTrunc,     NStr::eTrunc_Both ), tsTrunc    );
    BOOST_CHECK_EQUAL( NStr::TruncateSpaces(sTrunc,      NStr::eTrunc_Begin), sTrunc     );
    BOOST_CHECK_EQUAL( NStr::TruncateSpaces(sTrunc,      NStr::eTrunc_End  ), sTrunc     );
    BOOST_CHECK_EQUAL( NStr::TruncateSpaces(sTrunc,      NStr::eTrunc_Both ), sTrunc     );

    BOOST_CHECK_EQUAL( NStr::TruncateSpaces(szBegSpace,  NStr::eTrunc_Begin), tsTrunc    );
    BOOST_CHECK_EQUAL( NStr::TruncateSpaces(szBegSpace,  NStr::eTrunc_End  ), tsBegSpace );
    BOOST_CHECK_EQUAL( NStr::TruncateSpaces(szBegSpace,  NStr::eTrunc_Both ), tsTrunc    );
    BOOST_CHECK_EQUAL( NStr::TruncateSpaces(tsBegSpace,  NStr::eTrunc_Begin), tsTrunc    );
    BOOST_CHECK_EQUAL( NStr::TruncateSpaces(tsBegSpace,  NStr::eTrunc_End  ), tsBegSpace );
    BOOST_CHECK_EQUAL( NStr::TruncateSpaces(tsBegSpace,  NStr::eTrunc_Both ), tsTrunc    );
    BOOST_CHECK_EQUAL( NStr::TruncateSpaces(sBegSpace,   NStr::eTrunc_Begin), sTrunc     );
    BOOST_CHECK_EQUAL( NStr::TruncateSpaces(sBegSpace,   NStr::eTrunc_End  ), sBegSpace  );
    BOOST_CHECK_EQUAL( NStr::TruncateSpaces(sBegSpace,   NStr::eTrunc_Both ), sTrunc     );

    BOOST_CHECK_EQUAL( NStr::TruncateSpaces(szEndSpace,  NStr::eTrunc_Begin), tsEndSpace );
    BOOST_CHECK_EQUAL( NStr::TruncateSpaces(szEndSpace,  NStr::eTrunc_End  ), tsTrunc    );
    BOOST_CHECK_EQUAL( NStr::TruncateSpaces(szEndSpace,  NStr::eTrunc_Both ), tsTrunc    );
    BOOST_CHECK_EQUAL( NStr::TruncateSpaces(tsEndSpace,  NStr::eTrunc_Begin), tsEndSpace );
    BOOST_CHECK_EQUAL( NStr::TruncateSpaces(tsEndSpace,  NStr::eTrunc_End  ), tsTrunc    );
    BOOST_CHECK_EQUAL( NStr::TruncateSpaces(tsEndSpace,  NStr::eTrunc_Both ), tsTrunc    );
    BOOST_CHECK_EQUAL( NStr::TruncateSpaces(sEndSpace,   NStr::eTrunc_Begin), sEndSpace  );
    BOOST_CHECK_EQUAL( NStr::TruncateSpaces(sEndSpace,   NStr::eTrunc_End  ), sTrunc     );
    BOOST_CHECK_EQUAL( NStr::TruncateSpaces(sEndSpace,   NStr::eTrunc_Both ), sTrunc     );

    BOOST_CHECK_EQUAL( NStr::TruncateSpaces(szBothSpace, NStr::eTrunc_Begin), tsEndSpace );
    BOOST_CHECK_EQUAL( NStr::TruncateSpaces(szBothSpace, NStr::eTrunc_End  ), tsBegSpace );
    BOOST_CHECK_EQUAL( NStr::TruncateSpaces(szBothSpace, NStr::eTrunc_Both ), tsTrunc    );
    BOOST_CHECK_EQUAL( NStr::TruncateSpaces(tsBothSpace, NStr::eTrunc_Begin), tsEndSpace );
    BOOST_CHECK_EQUAL( NStr::TruncateSpaces(tsBothSpace, NStr::eTrunc_End  ), tsBegSpace );
    BOOST_CHECK_EQUAL( NStr::TruncateSpaces(tsBothSpace, NStr::eTrunc_Both ), tsTrunc    );
    BOOST_CHECK_EQUAL( NStr::TruncateSpaces(sBothSpace,  NStr::eTrunc_Begin), sEndSpace  );
    BOOST_CHECK_EQUAL( NStr::TruncateSpaces(sBothSpace,  NStr::eTrunc_End  ), sBegSpace  );
    BOOST_CHECK_EQUAL( NStr::TruncateSpaces(sBothSpace,  NStr::eTrunc_Both ), sTrunc     );
}



BOOST_AUTO_TEST_CASE(s_GetField)
{
    BOOST_CHECK_EQUAL( NStr::GetField(NULL, 17, "not important"), string() );
    BOOST_CHECK_EQUAL( NStr::GetField("", 0, ":"), string() );
    BOOST_CHECK_EQUAL( NStr::GetField("", 10, ":"), string() );
    BOOST_CHECK_EQUAL( NStr::GetField("one", 0, ":"), string( "one" ) );
    BOOST_CHECK_EQUAL( NStr::GetField("one:", 0, ":"), string( "one" ) );
    BOOST_CHECK_EQUAL( NStr::GetField("one: two", 0, ":"), string( "one" ) );
    BOOST_CHECK_EQUAL( NStr::GetField("one: two", 1, ":"), string( " two" ) );
    BOOST_CHECK_EQUAL( NStr::GetField("one: two", 1, "-.:;"), string( " two" ) );
    BOOST_CHECK_EQUAL( NStr::GetField("one: two", 1, "-.:"), string( " two" ) );
    BOOST_CHECK_EQUAL( NStr::GetField("one::two", 1, "-.:;"), string() );
    BOOST_CHECK_EQUAL( NStr::GetField("one::two", 176, "-.:;"), string() );

    BOOST_CHECK_EQUAL( NStr::GetField(NULL, 17, "not important", NStr::eMergeDelims), string() );
    BOOST_CHECK_EQUAL( NStr::GetField("", 0, ":", NStr::eMergeDelims), string() );
    BOOST_CHECK_EQUAL( NStr::GetField("", 10, ":", NStr::eMergeDelims), string() );
    BOOST_CHECK_EQUAL( NStr::GetField("one", 0, ":", NStr::eMergeDelims), string( "one" ) );
    BOOST_CHECK_EQUAL( NStr::GetField("one:", 0, ":", NStr::eMergeDelims), string( "one" ) );
    BOOST_CHECK_EQUAL( NStr::GetField("one::: two", 0, ":", NStr::eMergeDelims), string( "one" ) );
    BOOST_CHECK_EQUAL( NStr::GetField("one:::two:", 1, ":", NStr::eMergeDelims), string( "two" ) );
    BOOST_CHECK_EQUAL( NStr::GetField("one::-:two::", 1, "-.:;", NStr::eMergeDelims), string( "two" ) );
    BOOST_CHECK_EQUAL( NStr::GetField("one:two", 1, "-.:", NStr::eMergeDelims), string( "two" ) );
    BOOST_CHECK_EQUAL( NStr::GetField("one.two.", 1, "-.:;", NStr::eMergeDelims), string( "two" ) );
    BOOST_CHECK_EQUAL( NStr::GetField("one::two", 176, "-.:;", NStr::eMergeDelims), string() );
}

BOOST_AUTO_TEST_CASE(s_GetFieldSingleDilimiter)
{
    BOOST_CHECK_EQUAL( NStr::GetField(NULL, 17, 'n'), string() );
    BOOST_CHECK_EQUAL( NStr::GetField("", 0, ':'), string() );
    BOOST_CHECK_EQUAL( NStr::GetField("", 10, ':'), string() );
    BOOST_CHECK_EQUAL( NStr::GetField("one", 0, ':'), string( "one" ) );
    BOOST_CHECK_EQUAL( NStr::GetField("one:", 0, ':'), string( "one" ) );
    BOOST_CHECK_EQUAL( NStr::GetField("one: two", 0, ':'), string( "one" ) );
    BOOST_CHECK_EQUAL( NStr::GetField("one: two", 1, ':'), string( " two" ) );
    BOOST_CHECK_EQUAL( NStr::GetField("one::two", 1, ':'), string() );
    BOOST_CHECK_EQUAL( NStr::GetField("one::two", 176, ':'), string() );

    BOOST_CHECK_EQUAL( NStr::GetField(NULL, 17, 'n', NStr::eMergeDelims), string() );
    BOOST_CHECK_EQUAL( NStr::GetField("", 0, ':', NStr::eMergeDelims), string() );
    BOOST_CHECK_EQUAL( NStr::GetField("", 10, ':', NStr::eMergeDelims), string() );
    BOOST_CHECK_EQUAL( NStr::GetField("one", 0, ':', NStr::eMergeDelims), string( "one" ) );
    BOOST_CHECK_EQUAL( NStr::GetField("one:", 0, ':', NStr::eMergeDelims), string( "one" ) );
    BOOST_CHECK_EQUAL( NStr::GetField("one::: two", 0, ':', NStr::eMergeDelims), string( "one" ) );
    BOOST_CHECK_EQUAL( NStr::GetField("one:::two:", 1, ':', NStr::eMergeDelims), string( "two" ) );
    BOOST_CHECK_EQUAL( NStr::GetField("one::-:two::", 2, ':', NStr::eMergeDelims), string( "two" ) );
    BOOST_CHECK_EQUAL( NStr::GetField("one:two", 1, ':', NStr::eMergeDelims), string( "two" ) );
    BOOST_CHECK_EQUAL( NStr::GetField("one.two.", 1, '.', NStr::eMergeDelims), string( "two" ) );
    BOOST_CHECK_EQUAL( NStr::GetField("one::two", 176, ':', NStr::eMergeDelims), string() );
}

BOOST_AUTO_TEST_CASE(s_GetField_Unsafe)
{
    BOOST_CHECK_EQUAL( NStr::GetField_Unsafe(NULL, 17, "not important"), string() );
    BOOST_CHECK_EQUAL( NStr::GetField_Unsafe("", 0, ":"), string() );
    BOOST_CHECK_EQUAL( NStr::GetField_Unsafe("", 10, ":"), string() );
    BOOST_CHECK_EQUAL( NStr::GetField_Unsafe("one", 0, ":"), string( "one" ) );
    BOOST_CHECK_EQUAL( NStr::GetField_Unsafe("one:", 0, ":"), string( "one" ) );
    BOOST_CHECK_EQUAL( NStr::GetField_Unsafe("one: two", 0, ":"), string( "one" ) );
    BOOST_CHECK_EQUAL( NStr::GetField_Unsafe("one: two", 1, ":"), string( " two" ) );
    BOOST_CHECK_EQUAL( NStr::GetField_Unsafe("one: two", 1, "-.:;"), string( " two" ) );
    BOOST_CHECK_EQUAL( NStr::GetField_Unsafe("one: two", 1, "-.:"), string( " two" ) );
    BOOST_CHECK_EQUAL( NStr::GetField_Unsafe("one::two", 1, "-.:;"), string() );
    BOOST_CHECK_EQUAL( NStr::GetField_Unsafe("one::two", 176, "-.:;"), string() );

    BOOST_CHECK_EQUAL( NStr::GetField_Unsafe(NULL, 17, "not important", NStr::eMergeDelims), string() );
    BOOST_CHECK_EQUAL( NStr::GetField_Unsafe("", 0, ":", NStr::eMergeDelims), string() );
    BOOST_CHECK_EQUAL( NStr::GetField_Unsafe("", 10, ":", NStr::eMergeDelims), string() );
    BOOST_CHECK_EQUAL( NStr::GetField_Unsafe("one", 0, ":", NStr::eMergeDelims), string( "one" ) );
    BOOST_CHECK_EQUAL( NStr::GetField_Unsafe("one:", 0, ":", NStr::eMergeDelims), string( "one" ) );
    BOOST_CHECK_EQUAL( NStr::GetField_Unsafe("one::: two", 0, ":", NStr::eMergeDelims), string( "one" ) );
    BOOST_CHECK_EQUAL( NStr::GetField_Unsafe("one:::two:", 1, ":", NStr::eMergeDelims), string( "two" ) );
    BOOST_CHECK_EQUAL( NStr::GetField_Unsafe("one::-:two::", 1, "-.:;", NStr::eMergeDelims), string( "two" ) );
    BOOST_CHECK_EQUAL( NStr::GetField_Unsafe("one:two", 1, "-.:", NStr::eMergeDelims), string( "two" ) );
    BOOST_CHECK_EQUAL( NStr::GetField_Unsafe("one.two.", 1, "-.:;", NStr::eMergeDelims), string( "two" ) );
    BOOST_CHECK_EQUAL( NStr::GetField_Unsafe("one::two", 176, "-.:;", NStr::eMergeDelims), string() );
}

BOOST_AUTO_TEST_CASE(s_GetFieldSingleDilimiter_Unsafe)
{
    BOOST_CHECK_EQUAL( NStr::GetField_Unsafe(NULL, 17, 'n'), string() );
    BOOST_CHECK_EQUAL( NStr::GetField_Unsafe("", 0, ':'), string() );
    BOOST_CHECK_EQUAL( NStr::GetField_Unsafe("", 10, ':'), string() );
    BOOST_CHECK_EQUAL( NStr::GetField_Unsafe("one", 0, ':'), string( "one" ) );
    BOOST_CHECK_EQUAL( NStr::GetField_Unsafe("one:", 0, ':'), string( "one" ) );
    BOOST_CHECK_EQUAL( NStr::GetField_Unsafe("one: two", 0, ':'), string( "one" ) );
    BOOST_CHECK_EQUAL( NStr::GetField_Unsafe("one:two", 1, ':'), string( "two" ) );
    BOOST_CHECK_EQUAL( NStr::GetField_Unsafe("one:two:", 1, ':'), string( "two" ) );
    BOOST_CHECK_EQUAL( NStr::GetField_Unsafe("one::two", 1, ':'), string() );
    BOOST_CHECK_EQUAL( NStr::GetField_Unsafe("one::two", 176, ':'), string() );

    BOOST_CHECK_EQUAL( NStr::GetField_Unsafe(NULL, 17, 'n', NStr::eMergeDelims), string() );
    BOOST_CHECK_EQUAL( NStr::GetField_Unsafe("", 0, ':', NStr::eMergeDelims), string() );
    BOOST_CHECK_EQUAL( NStr::GetField_Unsafe("", 10, ':', NStr::eMergeDelims), string() );
    BOOST_CHECK_EQUAL( NStr::GetField_Unsafe("one", 0, ':', NStr::eMergeDelims), string( "one" ) );
    BOOST_CHECK_EQUAL( NStr::GetField_Unsafe("one:", 0, ':', NStr::eMergeDelims), string( "one" ) );
    BOOST_CHECK_EQUAL( NStr::GetField_Unsafe("one::: two", 0, ':', NStr::eMergeDelims), string( "one" ) );
    BOOST_CHECK_EQUAL( NStr::GetField_Unsafe("one:::two:", 1, ':', NStr::eMergeDelims), string( "two" ) );
    BOOST_CHECK_EQUAL( NStr::GetField_Unsafe("one::-:two::", 2, ':', NStr::eMergeDelims), string( "two" ) );
    BOOST_CHECK_EQUAL( NStr::GetField_Unsafe("one:two", 1, ':', NStr::eMergeDelims), string( "two" ) );
    BOOST_CHECK_EQUAL( NStr::GetField_Unsafe("one.two.", 1, '.', NStr::eMergeDelims), string( "two" ) );
    BOOST_CHECK_EQUAL( NStr::GetField_Unsafe("one::two", 176, ':', NStr::eMergeDelims), string() );
}

BOOST_AUTO_TEST_CASE(s_SQLEncode)
{
    BOOST_CHECK_EQUAL( NStr::SQLEncode("should not be touched"), CStringUTF8("'should not be touched'") );
    BOOST_CHECK_EQUAL( NStr::SQLEncode(""), CStringUTF8("''") );
    BOOST_CHECK_EQUAL( NStr::SQLEncode("'"), CStringUTF8("''''") );
    BOOST_CHECK_EQUAL( NStr::SQLEncode("\\'"), CStringUTF8("'\\'''") );
    BOOST_CHECK_EQUAL( NStr::SQLEncode("'a"), CStringUTF8("'''a'") );
    BOOST_CHECK_EQUAL( NStr::SQLEncode("a'"), CStringUTF8("'a'''") );
    BOOST_CHECK_EQUAL( NStr::SQLEncode("`1234567890-=~!@#$%^&*()_+qwertyuiop[]\\asdfghjkl;zxcvbnm,./QWERTYUIOP{}|ASDFGHJKL:\"ZXCVBNM<>?"),
                               CStringUTF8("'`1234567890-=~!@#$%^&*()_+qwertyuiop[]\\asdfghjkl;zxcvbnm,./QWERTYUIOP{}|ASDFGHJKL:\"ZXCVBNM<>?'") );

    const unsigned char s_UpperHalf[] = {
        0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8A, 0x8B, 0x8C, 0x8D, 0x8E, 0x8F,
        0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9A, 0x9B, 0x9C, 0x9D, 0x9E, 0x9F,
        0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7, 0xA8, 0xA9, 0xAA, 0xAB, 0xAC, 0xAD, 0xAE, 0xAF,
        0xB0, 0xB1, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7, 0xB8, 0xB9, 0xBA, 0xBB, 0xBC, 0xBD, 0xBE, 0xBF,
        0xC0, 0xC1, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7, 0xC8, 0xC9, 0xCA, 0xCB, 0xCC, 0xCD, 0xCE, 0xCF,
        0xD0, 0xD1, 0xD2, 0xD3, 0xD4, 0xD5, 0xD6, 0xD7, 0xD8, 0xD9, 0xDA, 0xDB, 0xDC, 0xDD, 0xDE, 0xDF,
        0xE0, 0xE1, 0xE2, 0xE3, 0xE4, 0xE5, 0xE6, 0xE7, 0xE8, 0xE9, 0xEA, 0xEB, 0xEC, 0xED, 0xEE, 0xEF,
        0xF0, 0xF1, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7, 0xF8, 0xF9, 0xFA, 0xFB, 0xFC, 0xFD, 0xFE, 0xFF
    };

    const unsigned char s_Expected[] = {
  '\'', 0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8A, 0x8B, 0x8C, 0x8D, 0x8E, 0x8F,
        0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9A, 0x9B, 0x9C, 0x9D, 0x9E, 0x9F,
        0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7, 0xA8, 0xA9, 0xAA, 0xAB, 0xAC, 0xAD, 0xAE, 0xAF,
        0xB0, 0xB1, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7, 0xB8, 0xB9, 0xBA, 0xBB, 0xBC, 0xBD, 0xBE, 0xBF,
        0xC0, 0xC1, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7, 0xC8, 0xC9, 0xCA, 0xCB, 0xCC, 0xCD, 0xCE, 0xCF,
        0xD0, 0xD1, 0xD2, 0xD3, 0xD4, 0xD5, 0xD6, 0xD7, 0xD8, 0xD9, 0xDA, 0xDB, 0xDC, 0xDD, 0xDE, 0xDF,
        0xE0, 0xE1, 0xE2, 0xE3, 0xE4, 0xE5, 0xE6, 0xE7, 0xE8, 0xE9, 0xEA, 0xEB, 0xEC, 0xED, 0xEE, 0xEF,
        0xF0, 0xF1, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7, 0xF8, 0xF9, 0xFA, 0xFB, 0xFC, 0xFD, 0xFE, 0xFF, '\''
    };

    CStringUTF8      upperHalf( string((char*)s_UpperHalf, 128) );
    CStringUTF8      expected( string((char*)s_Expected, 130) );

    BOOST_CHECK_EQUAL( NStr::SQLEncode(upperHalf), CStringUTF8(expected) );
}

BOOST_AUTO_TEST_CASE(s_StringToIntSpeed)
{
    NcbiCout << NcbiEndl << "NStr:: String to Int speed tests..." << NcbiEndl;

    const int COUNT = 10000000;
    const int TESTS = 6;
    const string ss[TESTS] = { "", "0", "1", "12345", "1234567890", "TRACE" };
    const int ssr[TESTS] = { -1, 0, 1, 12345, 1234567890, -1 };

    for ( int t = 0; t < TESTS; ++t ) 
    {
        int v = NStr::StringToNumeric<int>(ss[t], NStr::fConvErr_NoThrow);
        if ( !v && errno ) v = -1;
        if ( v != ssr[t] ) Abort();

        errno = 0;
        Uint8 v8 = NStr::StringToUInt8(ss[t], NStr::fConvErr_NoThrow);
        v = (int)v8;
        if ( !v8 && errno ) v = -1;
        if ( v != ssr[t] ) Abort();
        
        try {
            v = (int)NStr::StringToNumeric<Uint8>(ss[t]);
        }
        catch ( exception& ) {
            v = -1;
        }
        if ( v != ssr[t] ) Abort();
        try {
            v = (int)NStr::StringToUInt8(ss[t]);
        }
        catch ( exception& ) {
            v = -1;
        }
        if ( v != ssr[t] ) Abort();
    }
    for ( int t = 0; t < TESTS; ++t ) {
        CTempString s = ss[t];
        CStopWatch sw;
        double time;

        if ( 1 ) {
            sw.Restart();
            for ( int i = 0; i < COUNT; ++i ) {
                NStr::StringToNumeric<int>(ss[t], NStr::fConvErr_NoThrow);
            }
            time = sw.Elapsed();
            NcbiCout << "StringToNumeric<int>("<<ss[t]<<") time: " << time << endl;
        }
        if ( 1 ) {
            sw.Restart();
            for ( int i = 0; i < COUNT; ++i ) {
                Uint8 v = NStr::StringToUInt8(s, NStr::fConvErr_NoThrow);
                if ( !v && errno ) v = Uint8(-1);
            }
            time = sw.Elapsed();
            NcbiCout << "StringToInt8("<<ss[t]<<") time: " << time << endl;
        }
        if ( 0 ) {
            sw.Restart();
            for ( int i = 0; i < COUNT; ++i ) {
                Uint8 v;
                try {
                    v = NStr::StringToUInt8(s);
                }
                catch ( exception& ) {
                    v = Uint8(-1);
                }
            }
            time = sw.Elapsed();
            NcbiCout << "StringToInt8("<<ss[t]<<") time: " << time << endl;
        }
    }
}


BOOST_AUTO_TEST_CASE(s_StringToDoubleSpeed)
{
    NcbiCout << NcbiEndl << "NStr:: String to Double speed tests..." << NcbiEndl;

    const int COUNT = 10000000;
    const string ss[] = {
        "", "0", "1", "12", "123", "123456789", "1234567890", "TRACE",
        "0e9", "1e9",
        "1.234567890123456789e300", "-1.234567890123456789e-300",
        "1.234567890123456789e200", "-1.234567890123456789e-200" 
    };
    const double ssr[] = {
        -1, 0, 1, 12, 123, 123456789, 1234567890, -1,
        0, 1e9,
        1.234567890123456789e300, -1.234567890123456789e-300,
        1.234567890123456789e200, -1.234567890123456789e-200
    };
    double ssr_min[sizeof(ssr)/sizeof(ssr[0])];
    double ssr_max[sizeof(ssr)/sizeof(ssr[0])];
    const size_t TESTS = ArraySize(ss);

    int flags = NStr::fConvErr_NoThrow|NStr::fAllowLeadingSpaces;
    double v;
    for ( int t = 0; t < TESTS; ++t ) {
        if ( 1 ) {
            double r_min = ssr[t], r_max = r_min;
            if ( r_min < 0 ) {
                r_min *= 1+1e-15;
                r_max *= 1-1e-15;
            }
            else {
                r_min *= 1-1e-15;
                r_max *= 1+1e-15;
            }
            ssr_min[t] = r_min;
            ssr_max[t] = r_max;
        }
        if ( 1 ) {
            errno = 0;
            v = NStr::StringToDouble(ss[t], flags|NStr::fDecimalPosix);
            if ( errno ) v = -1;
            if ( v < ssr_min[t] || v > ssr_max[t] )
                ERR_POST(Fatal<<v<<" != "<<ssr[t]<<" for \"" << ss[t] << "\"");
        }

        if ( 1 ) {
            errno = 0;
            v = NStr::StringToDouble(ss[t], flags);
            if ( errno ) v = -1;
            if ( v < ssr_min[t] || v > ssr_max[t] )
                ERR_POST(Fatal<<v<<" != "<<ssr[t]<<" for \"" << ss[t] << "\"");
        }

        if ( 1 ) {
            errno = 0;
            char* errptr;
            v = NStr::StringToDoublePosix(ss[t].c_str(), &errptr);
            if ( errno || (errptr&&(*errptr||errptr==ss[t].c_str())) ) v = -1;
            if ( v < ssr_min[t] || v > ssr_max[t] )
                ERR_POST(Fatal<<v<<" != "<<ssr[t]<<" for \"" << ss[t] << "\"");
        }

        if ( 1 ) {
            errno = 0;
            char* errptr;
            v = strtod(ss[t].c_str(), &errptr);
            if ( errno || (errptr&&(*errptr||errptr==ss[t].c_str())) ) v = -1;
            if ( v < ssr_min[t] || v > ssr_max[t] )
                ERR_POST(Fatal<<v<<" != "<<ssr[t]<<" for \"" << ss[t] << "\"");
        }
    }
    for ( int t = 0; t < TESTS; ++t ) {
        string s1 = ss[t];
        CTempStringEx s = ss[t];
        const char* s2 = ss[t].c_str();
        CStopWatch sw;
        double time;

        if ( 1 ) {
            sw.Restart();
            for ( int i = 0; i < COUNT; ++i ) {
                errno = 0;
                v = NStr::StringToDouble(s, flags|NStr::fDecimalPosix);
                if ( errno ) v = -1;
            }
            time = sw.Elapsed();
            NcbiCout << "StringToDouble("<<ss[t]<<", Posix) time: " << time << endl;
        }
        if ( 1 ) {
            sw.Restart();
            for ( int i = 0; i < COUNT; ++i ) {
                errno = 0;
                v = NStr::StringToDouble(s, flags);
                if ( errno ) v = -1;
            }
            time = sw.Elapsed();
            NcbiCout << "StringToDouble("<<ss[t]<<") time: " << time << endl;
        }
        if ( 1 ) {
            sw.Restart();
            for ( int i = 0; i < COUNT; ++i ) {
                errno = 0;
                char* errptr;
                v = NStr::StringToDoublePosix(s2, &errptr);
                if ( errno || (errptr&&(*errptr||errptr==s2)) ) v = -1;
            }
            time = sw.Elapsed();
            NcbiCout << "StringToDoublePosix("<<ss[t]<<") time: " << time << endl;
        }
        if ( 1 ) {
            sw.Restart();
            for ( int i = 0; i < COUNT; ++i ) {
                errno = 0;
                char* errptr;
                v = strtod(s2, &errptr);
                if ( errno || (errptr&&(*errptr||errptr==s2)) ) v = -1;
            }
            time = sw.Elapsed();
            NcbiCout << "strtod("<<ss[t]<<") time: " << time << endl;
        }
    }
}


static const string s_ShellStr[] = {
    "abc",            // normal string, no encoding
    "ab\acd",         // non-printable chars, need BASH encoding
    "ab\ncd",         // EOL in the string
    "ab cd\tef",      // spaces - need quotes
    "ab!{}cd?*",      // more special chars which need quotes
    "ab'cd'ef",       // single quote - use double quotes around the string
    "ab ' cd ' ef",   // the same with extra-spaces
    "ab'$cd",         // additional chars ($, \) - can not use double quotes
    "\"ab cd ef\"",   // double quotes - use single quotes around the string
    "ab\\cd",         // backslash
    "ab\\cd'ef",      // backslash with single quote
    "ab\\cd\"ef",     // backslash with double quote
    "ab'\\cd\"$ef",   // ', ", \, $
    "",               // empty string
    "''",             // empty single quotes
    "\"\""            // empty double quotes
};


#ifdef NCBI_OS_UNIX
BOOST_AUTO_TEST_CASE(s_ShellEncode)
{
    cout << endl << "ShellEncode tests..." << endl;

    string echo_file = CFile::GetTmpName(CFile::eTmpFileCreate);
    string cmd_file = "./echo.sh";

    {{
        CNcbiOfstream out(cmd_file.c_str());
        out << "#! /bin/bash" << endl;

        for (size_t i = 0;  i < sizeof(s_ShellStr) / sizeof(s_ShellStr[0]);  i++) {
            string cmd = "echo -E ";
            cmd += NStr::ShellEncode(s_ShellStr[i]);
            cmd += " >> ";
            cmd += echo_file;
           out << cmd << endl;
        }
    }}
    CFile(cmd_file).SetMode(CFile::fDefaultUser | CFile::fExecute);
    BOOST_CHECK_EQUAL(CExec::System(cmd_file.c_str()), 0);

    CNcbiIfstream in(echo_file.c_str());
    string s, line;
    for (size_t i = 0;  i < sizeof(s_ShellStr) / sizeof(s_ShellStr[0]);  i++) {
        s.clear();
        size_t eol_pos = 0;
        do {
            getline(in, line);
            s.append(line);
            eol_pos = s_ShellStr[i].find('\n', eol_pos);
            if (eol_pos != NPOS) {
                eol_pos++;
                s.append("\n");
            }
        } while (eol_pos != NPOS);
        BOOST_CHECK_EQUAL(s_ShellStr[i], s);
    }
    CFile(echo_file).Remove();
    CFile(cmd_file).Remove();
}
#endif


NCBITEST_INIT_TREE()
{
    NCBITEST_DISABLE(s_StringToIntSpeed);
    NCBITEST_DISABLE(s_StringToDoubleSpeed);
}
