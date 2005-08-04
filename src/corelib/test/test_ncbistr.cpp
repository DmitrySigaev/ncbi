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
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbireg.hpp>
#include <corelib/version.hpp>
#include <algorithm>

#include <test/test_assert.h>  /* This header must go last */


// This is to use the ANSI C++ standard templates without the "std::" prefix
// and to use NCBI C++ entities without the "ncbi::" prefix
USING_NCBI_SCOPE;


#define OK  NcbiCout << " completed successfully!" << NcbiEndl
static const int kBad = 555;


//----------------------------------------------------------------------------
// NStr::StringTo*()
//----------------------------------------------------------------------------

struct SStringNumericValues
{
    const char* str;
//    NStr::TStringToNumFlags flags;
    int         flags;
    Int8        num;
    Int8        i;
    Uint8       u;
    Int8        i8;
    Uint8       u8;
    double      d;

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
        } else {
            return s == NStr::TruncateSpaces(str, NStr::eTrunc_Both);
        }
    }
};

#define DF NStr::fStringToNumDefault

//                str  flags  num   int   uint  Int8  Uint8  double
#define BAD(v)   {  v, DF, -1, kBad, kBad, kBad, kBad, kBad }
#define STR(v)   { #v, DF, NCBI_CONST_INT8(v), NCBI_CONST_INT8(v), NCBI_CONST_UINT8(v), NCBI_CONST_INT8(v), NCBI_CONST_UINT8(v), v##. }
#define STRI(v)  { #v, DF, -1, NCBI_CONST_INT8(v), kBad, NCBI_CONST_INT8(v), kBad, v##. }
#define STRU(v)  { #v, DF, -1, kBad, NCBI_CONST_UINT8(v), NCBI_CONST_INT8(v), NCBI_CONST_UINT8(v), v##. }
#define STR8(v)  { #v, DF, -1, kBad, kBad, NCBI_CONST_INT8(v), NCBI_CONST_UINT8(v), v##. }
#define STRI8(v) { #v, DF, -1, kBad, kBad, NCBI_CONST_INT8(v), kBad, v##. }
#define STRU8(v) { #v, DF, -1, kBad, kBad, kBad, NCBI_CONST_UINT8(v), v##. }
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
    { "-9223372036854775808", DF, -1, kBad, kBad, NCBI_CONST_INT8(-9223372036854775807)-1, kBad, -9223372036854775808. },
    STRI8(-9223372036854775807),
    STR8(9223372036854775806),
    STR8(9223372036854775807),
    STRU8(9223372036854775808),
    STRU8(18446744073709551614),
    STRU8(18446744073709551615),
    STRD(18446744073709551616),

    BAD(""),
    BAD("."),
    BAD(".."),
    BAD("abc"),
    { ".0",  DF, -1, kBad, kBad, kBad, kBad,  .0  },
    BAD(".0."),
    BAD("..0"),
    { ".01", DF, -1, kBad, kBad, kBad, kBad,  .01 },
    { "1.",  DF, -1, kBad, kBad, kBad, kBad,  1.  },
    { "1.1", DF, -1, kBad, kBad, kBad, kBad,  1.1 },
    BAD("1.1."),
    BAD("1.."),
    STRI(-123),
    BAD("--123"),

    { "+123", DF, 123, 123, 123, 123, 123, 123 },
    BAD("++123"),
    BAD("- 123"),

    // fix StringToDouble(), should be: 
    // BAD(" 123")
    { " 123",  DF, -1, kBad, kBad, kBad, kBad, 123. },

//+++ FIX ME: after transition to new flag versions of StringTo*() replace (>>*) with real flags !!!

    { " 123", (1<<12) /*NStr::fAllowLeadingSpaces*/,  -1, 123,  123,  123,  123,  123. },
    { " 123",  (1<<12)|(1<<13)/*NStr::fAllowLeadingSymbols*/, -1, 123,  123,  123,  123,  123. },

    BAD("123 "),

    // fix StringToDouble(), should be: 
    // { "123 ",  NStr::fAllowTrailingSpaces,  -1, 123,  123,  123,  123,  123. },
    // { "123 ",  NStr::fAllowTrailingSymbols, -1, 123,  123,  123,  123,  123. },
    { "123 ",  (1<<14)/*NStr::fAllowTrailingSpaces*/,  -1, 123,  123,  123,  123,  kBad },
    { "123 ",  (1<<14)|(1<<15)/*NStr::fAllowTrailingSymbols*/, -1, 123,  123,  123,  123,  kBad },

    // fix StringToDouble(), should be: 
    // { " 123 ",  NStr::fAllowLeadingSpaces | NStr::fAllowTrailingSpaces, -1, 123,  123,  123,  123,  123. },
    { " 123 ", (1<<12)|(1<<14)/*NStr::fAllowLeadingSpaces | NStr::fAllowTrailingSpaces*/, -1, 123,  123,  123,  123,  kBad },

    // fix StringToDouble(), should be: 
    // { "1,234",     NStr::fAllowCommas, -1,    1234,    1234,    1234,    1234,    1234 },
    // { "1,234,567", NStr::fAllowCommas, -1, 1234567, 1234567, 1234567, 1234567, 1234567 },
    { "1,234",     (1<<11)/*NStr::fAllowCommas*/, -1,    1234,    1234,    1234,    1234, kBad },
    { "1,234,567", (1<<11)/*NStr::fAllowCommas*/, -1, 1234567, 1234567, 1234567, 1234567, kBad },
    { "12,34",     (1<<11)/*NStr::fAllowCommas*/, -1,    kBad,    kBad,    kBad,    kBad, kBad },
    { ",123",      (1<<11)/*NStr::fAllowCommas*/, -1,    kBad,    kBad,    kBad,    kBad, kBad },
    // fix StringToDouble(), should be: 
    // { ",123",      NStr::fAllowCommas | NStr::fAllowLeadingSymbols, -1, 123, 123, 123, 123, 123 },
    { ",123",      (1<<11)|(1<<12)|(1<<13)/*NStr::fAllowCommas | NStr::fAllowLeadingSymbols*/, -1, 123, 123, 123, 123, kBad },

    { "+123", 0, 123, 123, 123, 123, 123, 123 },
    // fix StringToDouble(), should be: 
    // { "123", NStr::fMandatorySign, 123, kBad, kBad, kBad, kBad, kBad },
    { "123",  (1<<10)/*NStr::fMandatorySign*/, 123, kBad, kBad, kBad, kBad,  123 },
    { "+123", (1<<10)/*NStr::fMandatorySign*/, 123,  123,  123,  123,  123,  123 },
    { "-123", (1<<10)/*NStr::fMandatorySign*/,  -1, -123, kBad, -123, kBad, -123 },
    { "+123", (1<<12)|(1<<13)/*NStr::fAllowLeadingSymbols*/, 123,  123,  123,  123,  123,  123 },
    { "-123", (1<<12)|(1<<13)/*NStr::fAllowLeadingSymbols*/,  -1, -123, kBad, -123, kBad, -123 }
//---
};

static void s_StringToNum()
{
    NcbiCout << "Test NCBISTR:" << NcbiEndl;

    const size_t count = sizeof(s_Str2NumTests) / sizeof(s_Str2NumTests[0]);

    for (size_t i = 0;  i < count;  ++i) {
        const SStringNumericValues* test = &s_Str2NumTests[i];
        const char*                 str  = test->str;
//+++ FIX ME: -- after transition to new flag versions of StringTo*()
        //NStr::TStringToNumFlags flags = test->flags;
        NStr::TStringToNumFlags flags(test->flags, 0);
//---
        NStr::TNumToStringFlags str_flags = 0;
        if ( flags & NStr::fMandatorySign ) 
            str_flags |= NStr::fWithSign;
        if ( flags & NStr::fAllowCommas )
            str_flags |= NStr::fWithCommas;
        bool allow_same_test = (flags < NStr::fAllowLeadingSpaces);

        NcbiCout << "\n*** Checking string '" << str << "'***" << NcbiEndl;

        // num
        {{
            int value = NStr::StringToNumeric(str);
            NcbiCout << "numeric value: " << value << ", toString: '"
                     << NStr::IntToString(value) << "'" << NcbiEndl;
            assert(value == test->num);
        }}

        // int
        try {
            int value = NStr::StringToInt(str, flags);
            NcbiCout << "int value: " << value << ", toString: '"
                     << NStr::IntToString(value, str_flags) << "'"
                     << NcbiEndl;
            assert(test->IsGoodInt());
            assert(value == test->i);
            if (allow_same_test)
                assert(test->Same(NStr::IntToString(value, str_flags)));
        }
        catch (CException& e) {
            NCBI_REPORT_EXCEPTION("TestStrings",e);
            if ( test->IsGoodInt() ) {
                ERR_POST("Cannot convert '" << str << "' to int");
            }
            assert(!test->IsGoodInt());
        }

        // unsigned int
        try {
            unsigned int value = NStr::StringToUInt(str, flags);
            NcbiCout << "unsigned int value: " << value << ", toString: '"
                     << NStr::UIntToString(value, str_flags) << "'"
                     << NcbiEndl;
            assert(test->IsGoodUInt());
            assert(value == test->u);
            if (allow_same_test)
                assert(test->Same(NStr::UIntToString(value, str_flags)));
        }
        catch (CException& e) {
            NCBI_REPORT_EXCEPTION("TestStrings",e);
            if ( test->IsGoodUInt() ) {
                ERR_POST("Cannot convert '" << str << "' to unsigned int");
            }
            assert(!test->IsGoodUInt());
        }

        // long
        try {
            long value = NStr::StringToLong(str, flags);
            NcbiCout << "long value: " << value << ", toString: '"
                     << NStr::IntToString(value, str_flags) << "', "
                     << "Int8ToString: '"
                     << NStr::Int8ToString(value, str_flags) << "'"
                     << NcbiEndl;
            #if (SIZEOF_LONG == SIZEOF_INT)
                assert(test->IsGoodInt());
                assert(value == test->i);
                if (allow_same_test)
                    assert(test->Same(NStr::IntToString(value, str_flags)));
            #else
                assert(test->IsGoodInt8());
                assert(value == test->i8);
                if (allow_same_test)
                    assert(test->Same(NStr::Int8ToString(value, str_flags)));
            #endif
        }
        catch (CException& e) {
            NCBI_REPORT_EXCEPTION("TestStrings",e);
            #if (SIZEOF_LONG == SIZEOF_INT)
                if ( test->IsGoodInt() ) {
                    ERR_POST("Cannot convert '" << str <<
                             "' to unsigned long");
                }
                assert(!test->IsGoodInt());
            #else
                if ( test->IsGoodInt8() ) {
                    ERR_POST("Cannot convert '" << str <<
                             "' to unsigned long");
                }
                assert(!test->IsGoodInt8());
            #endif
        }

        // unsigned long
        try {
            unsigned long value = NStr::StringToULong(str, flags);
            NcbiCout << "unsigned long value: " << value << ", toString: '"
                     << NStr::UIntToString(value, str_flags) << "', "
                     << "UInt8ToString: '"
                     << NStr::UInt8ToString(value, str_flags) << "'"
                     << NcbiEndl;
            #if (SIZEOF_LONG == SIZEOF_INT)
                assert(test->IsGoodUInt());
                assert(value == test->u);
                if (allow_same_test)
                    assert(test->Same(NStr::UIntToString(value, str_flags)));
            #else
                assert(test->IsGoodUInt8());
                assert(value == test->u8);
                if (allow_same_test)
                    assert(test->Same(NStr::UInt8ToString(value, str_flags)));
            #endif
        }
        catch (CException& e) {
            NCBI_REPORT_EXCEPTION("TestStrings",e);
            #if (SIZEOF_LONG == SIZEOF_INT)
                if ( test->IsGoodUInt() ) {
                    ERR_POST("Cannot convert '" << str <<
                             "' to unsigned long");
                }
                assert(!test->IsGoodUInt());
            #else
                if ( test->IsGoodUInt8() ) {
                    ERR_POST("Cannot convert '" << str <<
                             "' to unsigned long");
                }
                assert(!test->IsGoodUInt8());
            #endif
        }

        // Int8
        try {
            Int8 value = NStr::StringToInt8(str, flags);
            NcbiCout << "Int8 value: " << value << ", toString: '"
                     << NStr::Int8ToString(value, str_flags) << "'"
                     << NcbiEndl;
            assert(test->IsGoodInt8());
            assert(value == test->i8);
            if (allow_same_test)
                assert(test->Same(NStr::Int8ToString(value, str_flags)));
        }
        catch (CException& e) {
            NCBI_REPORT_EXCEPTION("TestStrings",e);
            if ( test->IsGoodInt8() ) {
                ERR_POST("Cannot convert '" << str << "' to Int8");
            }
            assert(!test->IsGoodInt8());
        }

        // Uint8
        try {
            Uint8 value = NStr::StringToUInt8(str, flags);
            NcbiCout << "Uint8 value: " << value << ", toString: '"
                     << NStr::UInt8ToString(value, str_flags) << "'"
                     << NcbiEndl;
            assert(test->IsGoodUInt8());
            assert(value == test->u8);
            if (allow_same_test)
                assert(test->Same(NStr::UInt8ToString(value, str_flags)));
        }
        catch (CException& e) {
            NCBI_REPORT_EXCEPTION("TestStrings",e);
            if ( test->IsGoodUInt8() ) {
                ERR_POST("Cannot convert '" << str << "' to Uint8");
            }
            assert(!test->IsGoodUInt8());
        }

        // double
        try {
            double value = NStr::StringToDouble(str, flags);
            NcbiCout << "double value: " << value << ", toString: '"
                     << NStr::DoubleToString(value, str_flags) << "'"
                     << NcbiEndl;
            assert(test->IsGoodDouble());
            assert(value == test->d);
        }
        catch (CException& e) {
            NCBI_REPORT_EXCEPTION("TestStrings",e);
            if ( test->IsGoodDouble() ) {
                ERR_POST("Cannot convert '" << str << "' to double");
            }
            assert(!test->IsGoodDouble());
        }
    }
    OK;
}


//----------------------------------------------------------------------------
// NStr::StringTo*() radix test
//----------------------------------------------------------------------------

// Writing separate tests for StringToUInt8 because we
// need to test for different radix values such as 2, 8, and 16.

struct SRadixTest {
    const char* str;       // String input
    int         base;      // Radix base 
    Uint8       expected;  // Expected value
};

static const SRadixTest s_RadixTests[] = {
    { "A",         16, 10 },
    { "0xA",       16, 10 },
    { "B9",        16, 185 },
    { "C5D",       16, 3165 },
    { "FFFF",      16, 65535 },
    { "17ABCDEF",  16, 397135343 },
    { "BADBADBA",  16, 3134959034U },
    { "7",          8, 7  },
    { "17",         8, 15 },
    { "177",        8, 127},
    { "0123",       8, 83 },
    { "01234567",   8, 342391},
    { "0",          2, 0 },
    { "1",          2, 1 },
    { "10",         2, 2 },
    { "11",         2, 3 },
    { "100",        2, 4 },
    { "101",        2, 5 }, 
    { "110",        2, 6 },
    { "111",        2, 7 },

    // Autodetect radix base
    { "0xC5D",      0, 3165 },  // 16
    { "0123",       0, 83 },    // 8
    { "123",        0, 123 },   // 10
    { "111",        0, 111 },   // 10

    // Invalid values come next
    { "10ABCDEFGH",16, 0 },
    { "12345A",    10, 0 },
    { "012345678",  8, 0 },
    { "012",        2, 0 }
};


static void s_StringToNumRadix()
{
    NcbiCout << "Test NCBISTR:" << NcbiEndl;

    const size_t count = sizeof(s_RadixTests)/sizeof(s_RadixTests[0]);
    for (size_t i = 0;  i < count;  ++i) 
    {
        const string& str      = s_RadixTests[i].str;
        int           base     = s_RadixTests[i].base;
        Uint8         expected = s_RadixTests[i].expected;

        NcbiCout << "Checking numeric string: '" << str 
                 << "': with base " << base << NcbiEndl;
        try {
            Uint8 value = NStr::StringToUInt8(str, 
                                              NStr::fStringToNumDefault,
                                              base);
            NcbiCout << "Uint8 value: " << ((unsigned)value)
                     << ", Expected: " << (unsigned)expected << NcbiEndl;
        }
        catch (CException& e) {
            NCBI_REPORT_EXCEPTION("TestStrings",e);
        }
    } 
    OK;
}


//----------------------------------------------------------------------------
// NStr::StringTo*_DataSize()
//----------------------------------------------------------------------------

struct SStringDataSizeValues
{
    const char* str;
//+++ FIX ME
//    NStr::TStringToNumFlags flags;
    int         flags;
//---    
    int         base;
    Uint8       expected;

    bool IsGood(void) const {
        return expected != (Uint8)kBad;
    }
};

static const SStringDataSizeValues s_Str2DataSizeTests[] = {
    // str  flags  base  num
    { "10",    0, 10,      10 },
    { "10k",   0, 10, 10*1024 },
    { "10K",   0, 10, 10*1024 },
    { "10KB",  0, 10, 10*1024 },
    { "10M",   0, 10, 10*1024*1024 },
    { "10MB",  0, 10, 10*1024*1024 },
    { "10K",   0,  2,  2*1024 },
    { "10K",   0,  8,  8*1024 },
    { "AK",    0, 16, 10*1024 },
    { "10K",   0,  0, 10*1024 },
    { "+10K",  0,  0, 10*1024 },
    { "-10K",  0,  0, kBad },
    { "1GBx",  0, 10, kBad },
    { "10000000000000GB", 0, 10, kBad },
    { " 10K",  0, 10, kBad },
    { " 10K",  (1<<12)/*NStr::fAllowLeadingSpaces*/,  10, 10*1024 },
    { "10K ",  0, 10, kBad },
    { "10K ",  (1<<14)/*NStr::fAllowTrailingSpaces*/,  10, 10*1024 },
    { "10K",   (1<<10)/*NStr::fMandatorySign*/, 10, kBad },
    { "+10K",  0, 10, 10*1024 },
    { "+10K",  (1<<10)/*NStr::fMandatorySign*/, 10, 10*1024 },
    { "-10K",  0, 10, kBad },
    { "-10K",  (1<<10)/*NStr::fMandatorySign*/, 10, kBad },
    { "1,000K", 0, 10, kBad },
    { "1,000K",(1<<11)/*NStr::fAllowCommas*/,   10, 1000*1024 },
    { "K10K",  0, 10, kBad },
    { "K10K",  (1<<12)|(1<<13)/*NStr::fAllowLeadingSymbols*/, 10, 10*1024 },
    { "K10K",  (1<<12)|(1<<13)/*NStr::fAllowLeadingSymbols*/, 10, 10*1024 },
    { "10KG",  (1<<14)|(1<<15)/*NStr::fAllowTrailingSymbols*/,10, 10*1024 }
};

static void s_StringToNumDataSize()
{
    NcbiCout << NcbiEndl << "NStr::StringToUInt8_DataSize() tests...";

    const size_t count = sizeof(s_Str2DataSizeTests) /
                         sizeof(s_Str2DataSizeTests[0]);

    for (size_t i = 0;  i < count;  ++i)
    {
        const SStringDataSizeValues* test = &s_Str2DataSizeTests[i];
        const char*                  str  = test->str;
//+++ FIX ME: -- after transition to new flags StringTo*()
        //NStr::TStringToNumFlags flags = test->flags;
        NStr::TStringToNumFlags flags(test->flags, 0);
//---
        NcbiCout << "\n*** Checking string '" << str << "'***" << NcbiEndl;

        try {
            Uint8 value = NStr::StringToUInt8_DataSize(str, flags,test->base);
            NcbiCout << "value: " << value << NcbiEndl;
            assert(test->IsGood());
            assert(value == test->expected);
        }
        catch (CException& e) {
            NCBI_REPORT_EXCEPTION("TestStrings",e);
            if ( test->IsGood() ) {
                ERR_POST("Cannot convert '" << str << "' to data size");
            } else {
                NcbiCout << "value: bad" << NcbiEndl;
            }
            assert(!test->IsGood());
        }
    }
    OK;
}


//----------------------------------------------------------------------------
// NStr::Replace()
//----------------------------------------------------------------------------

static void s_Replace()
{
    NcbiCout << NcbiEndl << "NStr::Replace() tests...";

    string src("aaabbbaaccczzcccXX");
    string dst;

    string search("ccc");
    string replace("RrR");
    NStr::Replace(src, search, replace, dst);
    assert(dst == "aaabbbaaRrRzzRrRXX");

    search = "a";
    replace = "W";
    NStr::Replace(src, search, replace, dst, 6, 1);
    assert(dst == "aaabbbWaccczzcccXX");
    
    search = "bbb";
    replace = "BBB";
    NStr::Replace(src, search, replace, dst, 50);
    assert(dst == "aaabbbaaccczzcccXX");

    search = "ggg";
    replace = "no";
    dst = NStr::Replace(src, search, replace);
    assert(dst == "aaabbbaaccczzcccXX");

    search = "a";
    replace = "A";
    dst = NStr::Replace(src, search, replace);
    assert(dst == "AAAbbbAAccczzcccXX");

    search = "X";
    replace = "x";
    dst = NStr::Replace(src, search, replace, src.size() - 1);
    assert(dst == "aaabbbaaccczzcccXx");

    OK;
}


//----------------------------------------------------------------------------
// NStr::PrintableString/ParseEscapes()
//----------------------------------------------------------------------------

static void s_PrintableString()
{
    NcbiCout << NcbiEndl << "NStr::PrinrableString() tests...";

    // NStr::PrintableString()
    assert(NStr::PrintableString(kEmptyStr).empty());
    assert(NStr::PrintableString("AB\\CD\nAB\rCD\vAB\tCD\'AB\"").
           compare("AB\\\\CD\\nAB\\rCD\\vAB\\tCD'AB\\\"") == 0);
    assert(NStr::PrintableString("A\020B" + string(1, '\0') + "CD").
           compare("A\\x10B\\x00CD") == 0);

    // NStr::ParseEscapes
    assert(NStr::ParseEscapes(kEmptyStr).empty());
    assert(NStr::ParseEscapes("AB\\\\CD\\nAB\\rCD\\vAB\\tCD'AB\\\"").
           compare("AB\\CD\nAB\rCD\vAB\tCD\'AB\"") == 0);
    assert(NStr::ParseEscapes("A\\x10B\\x00CD").
           compare("A\020B" + string(1, '\0') + "CD") == 0);

    OK;
}


//----------------------------------------------------------------------------
// NStr::Compare()
//----------------------------------------------------------------------------

static void s_CompareStr(int expr_res, int valid_res)
{
    int res = expr_res > 0 ? 1 :
        expr_res == 0 ? 0 : -1;
    assert(res == valid_res);
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

static void s_Compare()
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

    assert(NStr::Compare("0123", 0, 2, "12") <  0);
    assert(NStr::Compare("0123", 1, 2, "12") == 0);
    assert(NStr::Compare("0123", 2, 2, "12") >  0);
    assert(NStr::Compare("0123", 3, 2,  "3") == 0);

    OK;
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

static void s_Split()
{
    NcbiCout << NcbiEndl << "NStr::Split() tests...";

    list<string> split;

    for (size_t i = 0; i < sizeof(s_SplitStr) / sizeof(s_SplitStr[0]); i++) {
        NStr::Split(s_SplitStr[i], s_SplitDelim[i], split);
    }
    size_t i = 0;
    ITERATE(list<string>, it, split) {
        assert(i < sizeof(split_result) / sizeof(split_result[0]));
        assert(NStr::Compare(*it, split_result[i++]) == 0);
    }
    OK;
}


//----------------------------------------------------------------------------
// NStr::Tokenize()
//----------------------------------------------------------------------------

static void s_Tokenize()
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
            assert(NStr::Compare(*it, tok_result[i++]) == 0);
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
            assert(NStr::Compare(*it, split_result[i++]) == 0);
        }
    }}

    OK;
}


//----------------------------------------------------------------------------
// NStr::SplitInTo()
//----------------------------------------------------------------------------

struct SSplitInTwo {
    const char* str;
    const char* delim;
    const char* expected_str1;
    const char* expected_str2;
    bool        expected_ret;
};
    
static const SSplitInTwo s_SplitInTwoTest[] = {
    { "ab+cd+ef", "+", "ab", "cd+ef", true },
    { "aaAAabBbbb", "AB", "aa", "AabBbbb", true },
    { "aaCAabBCbbb", "ABC", "aa", "AabBCbbb", true },
    { "-beg-delim-", "-", "", "beg-delim-", true },
    { "end-delim:", ":", "end-delim", "", true },
    { "nodelim", ".,:;-+", "nodelim", "", false },
    { "emptydelim", "", "emptydelim", "", false },
    { "", "emtpystring", "", "", false },
    { "", "", "", "", false }
};

static void s_SplitInTwo()
{
    NcbiCout << NcbiEndl << "NStr::SplitInTwo() tests...";

    string string1, string2;
    bool   result;
    const size_t count = sizeof(s_SplitInTwoTest) /
                         sizeof(s_SplitInTwoTest[0]);
    for (size_t i = 0; i < count; i++) {
        result = NStr::SplitInTwo(s_SplitInTwoTest[i].str,
                                  s_SplitInTwoTest[i].delim,
                                  string1, string2);
        assert(s_SplitInTwoTest[i].expected_ret == result);
        assert(s_SplitInTwoTest[i].expected_str1 == string1);
        assert(s_SplitInTwoTest[i].expected_str2 == string2);
    }
    OK;
}


//----------------------------------------------------------------------------
// NStr::TokenizePattern()
//----------------------------------------------------------------------------

static void s_TokenizePattern()
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
        assert(sizeof(tok_pattern_result_m)/sizeof(tok_pattern_result_m[0])
               == tok.size());
        size_t i = 0;
        ITERATE(vector<string>, it, tok) {
            assert(NStr::Compare(*it, tok_pattern_result_m[i++]) == 0);
        }
    }}
    tok.clear();

    NStr::TokenizePattern(tok_pattern_str, tok_pattern_delim,
                          tok, NStr::eNoMergeDelims);
    {{
        assert(sizeof(tok_pattern_result_nm)/sizeof(tok_pattern_result_nm[0])
               == tok.size());
        size_t i = 0;
        ITERATE(vector<string>, it, tok) {
            assert(NStr::Compare(*it, tok_pattern_result_nm[i++]) == 0);
        }
    }}
    OK;
}


//----------------------------------------------------------------------------
// NStr::ToLower/ToUpper()
//----------------------------------------------------------------------------

static void s_Case()
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
        assert(NStr::Compare(s_Indiff, indiff) == 0);
        assert(NStr::Compare(s_Indiff, NStr::ToLower(indiff)) == 0);
        ::strcpy(indiff, s_Indiff);
        assert(NStr::Compare(s_Indiff, NStr::ToUpper(indiff)) == 0);
        assert(NStr::Compare(s_Indiff, NStr::ToLower(indiff)) == 0);
    }}
    {{
        string indiff;
        indiff = s_Indiff;
        assert(NStr::Compare(s_Indiff, indiff) == 0);
        assert(NStr::Compare(s_Indiff, NStr::ToLower(indiff)) == 0);
        indiff = s_Indiff;
        assert(NStr::Compare(s_Indiff, NStr::ToUpper(indiff)) == 0);
        assert(NStr::Compare(s_Indiff, NStr::ToLower(indiff)) == 0);
    }}

    for (size_t i = 0;  i < sizeof(s_Tri) / sizeof(s_Tri[0]);  i++) {
        assert(NStr::Compare(s_Tri[i].orig, s_Tri[i].x_lower, NStr::eNocase)
               == 0);
        assert(NStr::Compare(s_Tri[i].orig, s_Tri[i].x_upper, NStr::eNocase)
               == 0);
        string orig = s_Tri[i].orig;
        assert(NStr::Compare(orig, s_Tri[i].x_lower, NStr::eNocase) == 0);
        assert(NStr::Compare(orig, s_Tri[i].x_upper, NStr::eNocase) == 0);
        string x_lower = s_Tri[i].x_lower;
        {{
            char x_str[16];
            ::strcpy(x_str, s_Tri[i].orig);
            assert(::strlen(x_str) < sizeof(x_str));
            assert(NStr::Compare(NStr::ToLower(x_str), x_lower) == 0);
            ::strcpy(x_str, s_Tri[i].orig);
            assert(NStr::Compare(NStr::ToUpper(x_str), s_Tri[i].x_upper) ==0);
            assert(NStr::Compare(x_lower, NStr::ToLower(x_str)) == 0);
        }}
        {{
            string x_str;
            x_lower = s_Tri[i].x_lower;
            x_str = s_Tri[i].orig;
            assert(NStr::Compare(NStr::ToLower(x_str), x_lower) == 0);
            x_str = s_Tri[i].orig;
            assert(NStr::Compare(NStr::ToUpper(x_str), s_Tri[i].x_upper) ==0);
            assert(NStr::Compare(x_lower, NStr::ToLower(x_str)) == 0);
        }}
    }
    OK;
}


//----------------------------------------------------------------------------
// NStr::str[n]casecmp()
//----------------------------------------------------------------------------

static void s_strcasecmp()
{
    NcbiCout << NcbiEndl << "NStr::str[n]casecmp() tests...";

    assert(NStr::strncasecmp("ab", "a", 1) == 0);
    assert(NStr::strncasecmp("Ab", "a", 1) == 0);
    assert(NStr::strncasecmp("a", "Ab", 1) == 0);
    assert(NStr::strncasecmp("a", "ab", 1) == 0);

    assert(NStr::strcasecmp("a",  "A") == 0);
    assert(NStr::strcasecmp("a",  "a") == 0);
    assert(NStr::strcasecmp("ab", "a") != 0);
    assert(NStr::strcasecmp("a", "ab") != 0);
    assert(NStr::strcasecmp("a",   "") != 0);
    assert(NStr::strcasecmp("",   "a") != 0);
    assert(NStr::strcasecmp("",    "") == 0);

    OK;
}


//----------------------------------------------------------------------------
// NStr::AStrEquiv()  &   NStr::Equal*()
//----------------------------------------------------------------------------

static void s_Equal()
{
    string as1("abcdefg ");
    string as2("abcdefg ");
    string as3("aBcdEfg ");
    string as4("lsekfu");

    NcbiCout << NcbiEndl << "AStrEquiv tests...";

    assert( AStrEquiv(as1, as2, PNocase()) == true );
    assert( AStrEquiv(as1, as3, PNocase()) == true );
    assert( AStrEquiv(as3, as4, PNocase()) == false );
    assert( AStrEquiv(as1, as2, PCase())   == true );
    assert( AStrEquiv(as1, as3, PCase())   == false );
    assert( AStrEquiv(as2, as4, PCase())   == false );
    OK;

    NcbiCout << NcbiEndl << "Equal{Case,Nocase} tests...";

    assert( NStr::EqualNocase(as1, as2) == true );
    assert( NStr::EqualNocase(as1, as3) == true );
    assert( NStr::EqualNocase(as3, as4) == false );
    assert( NStr::EqualCase(as1, as2)   == true );
    assert( NStr::EqualCase(as1, as3)   == false );
    assert( NStr::EqualCase(as2, as4)   == false );
    OK;
}


//----------------------------------------------------------------------------
// Reference counting
//----------------------------------------------------------------------------

static void s_ReferenceCounting()
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
    OK;
}


//----------------------------------------------------------------------------
// NStr::FindNoCase()
//----------------------------------------------------------------------------

static void s_FindNoCase()
{
    NcbiCout << NcbiEndl << "NStr::FindNoCase() tests...";
    assert(NStr::FindNoCase(" abcd", " xyz") == NPOS);
    assert(NStr::FindNoCase(" abcd", " xyz", 0, NPOS, NStr::eLast) == NPOS);
    assert(NStr::FindNoCase(" abcd", " aBc", 0, NPOS, NStr::eLast) == 0);

    OK;
}


//----------------------------------------------------------------------------
// CVersionInfo:: parse from str
//----------------------------------------------------------------------------

static void s_VersionInfo()
{
    NcbiCout << NcbiEndl << "CVersionInfo::FromStr tests...";
    {{
        CVersionInfo ver("1.2.3");
        assert(ver.GetMajor() == 1  &&  ver.GetMinor() == 2  &&
               ver.GetPatchLevel() == 3);
        ver.FromStr("12.35");
        assert(ver.GetMajor() == 12  &&  ver.GetMinor() == 35  &&
               ver.GetPatchLevel() == 0);
        {{
            bool err_catch = false;
            try {
                ver.FromStr("12.35a");
            }
            catch (exception&) {
                err_catch = true;
            }
            assert(err_catch);
        }}
    }}
    OK;

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
        assert(name                == s_VerInfo[i].name);
        assert(ver.GetMajor()      == s_VerInfo[i].ver_major);
        assert(ver.GetMinor()      == s_VerInfo[i].ver_minor);
        assert(ver.GetPatchLevel() == s_VerInfo[i].patch_level);
    }
    ParseVersionString("MyProgram ", &name, &ver);
    assert(name == "MyProgram");
    assert(ver.IsAny());

    OK;
}


/////////////////////////////////
// Test application
//

class CTestApplication : public CNcbiApplication
{
public:
    void Init(void);
    int Run(void);
};


void CTestApplication::Init(void)
{
    // Set err.-posting and tracing to maximum
    SetDiagTrace(eDT_Enable);
    SetDiagPostFlag(eDPF_All);
    SetDiagPostLevel(eDiag_Info);
}


int CTestApplication::Run(void)
{
    SetupDiag(eDS_ToStdout);
    CExceptionReporter::EnableDefault(false);
    //CExceptionReporter::EnableDefault(true);

    s_Compare();
    s_Case();
    s_strcasecmp();
    s_Equal();
    s_StringToNum();
    s_StringToNumRadix();
    s_StringToNumDataSize();
    s_Replace();
    s_Split();
    s_SplitInTwo();
    s_Tokenize();
    s_TokenizePattern();
    s_PrintableString();
    s_FindNoCase();
    s_ReferenceCounting();
    s_VersionInfo();

    NcbiCout << NcbiEndl 
             << "TEST_NCBISTR execution completed successfully!"
             << NcbiEndl << NcbiEndl;

    return 0;
}

  
/////////////////////////////////
// APPLICATION OBJECT and MAIN
//

int main(int argc, const char* argv[] /*, const char* envp[]*/)
{
    CTestApplication theTestApplication;
    return theTestApplication.AppMain(argc, argv, 0 /*envp*/, eDS_ToMemory);
}


/*
 * ==========================================================================
 * $Log$
 * Revision 6.47  2005/08/04 11:11:01  ivanov
 * Move each test to separate static function.
 * Added more tests for StringTo*().
 *
 * Revision 6.46  2005/06/06 20:22:21  vasilche
 * Use getline instead of gets.
 *
 * Revision 6.45  2005/06/03 16:42:46  lavr
 * Explicit (unsigned char) casts in ctype routines
 *
 * Revision 6.44  2005/05/16 15:55:42  ivanov
 * Fixed compilation warning
 *
 * Revision 6.43  2005/05/13 16:27:31  ivanov
 * Enabled commented asserts -- StringTo[U]Int8 was fixed.
 * Minor cosmetics.
 *
 * Revision 6.42  2005/05/13 14:00:12  vasilche
 * Added more checks of int to string conversion.
 *
 * Revision 6.41  2005/05/12 17:01:23  vasilche
 * Added detailed data for *Int*ToString test.
 *
 * Revision 6.40  2005/05/06 12:42:35  kuznets
 * New test case
 *
 * Revision 6.39  2005/04/29 14:42:05  ivanov
 * Added test for NStr::TokenizePattern()
 *
 * Revision 6.38  2005/04/18 14:26:34  kuznets
 * More complicated version string cases
 *
 * Revision 6.37  2005/04/04 16:17:41  kuznets
 * + Test for version strings
 *
 * Revision 6.36  2004/10/21 15:25:41  vakatov
 * Warning fixes
 *
 * Revision 6.35  2004/10/15 12:02:46  ivanov
 * Renamed NStr::StringToUInt_DataSize -> NStr::StringToUInt8_DataSize.
 *
 * Revision 6.34  2004/10/13 13:11:54  ivanov
 * NStr::StringToUInt_DataSize - added overflow test.
 * Some cosmetics.
 *
 * Revision 6.33  2004/10/13 01:07:05  vakatov
 * + NStr::strncasecmp
 * + NStr::strcasecmp
 * NStr::StringToUInt_DataSize -do not check for values which can overflow Uint
 *
 * Revision 6.32  2004/09/21 18:45:20  kuznets
 * SoftStringToUInt renamed StringToUInt_DataSize
 *
 * Revision 6.31  2004/09/21 18:24:41  kuznets
 * +test NStr::SoftStringToUInt
 *
 * Revision 6.30  2004/08/04 16:52:01  vakatov
 * Signed/unsigned warning fix
 *
 * Revision 6.29  2004/07/20 18:47:29  ucko
 * Use the Split test-cases for Tokenize(..., eMergeDelims).
 *
 * Revision 6.28  2004/05/26 20:46:54  ucko
 * Add tests for Equal{Case,Nocase}.
 *
 * Revision 6.27  2004/05/26 19:26:29  ucko
 * Add a(n incomplete) set of tests for FindNoCase.
 *
 * Revision 6.26  2004/05/14 13:59:51  gorelenk
 * Added include of ncbi_pch.hpp
 *
 * Revision 6.25  2003/12/02 15:23:25  ucko
 * Add tests for NStr::ParseEscapes (inverted from tests for PrintableString)
 *
 * Revision 6.24  2003/10/01 20:43:30  ivanov
 * Get rid of compilation warnings; some formal code rearrangement
 *
 * Revision 6.23  2003/08/19 15:17:20  rsmith
 * Add NStr::SplitInTwo() function.
 *
 * Revision 6.22  2003/07/23 17:49:55  vasilche
 * Commented out memory usage test.
 *
 * Revision 6.21  2003/07/22 21:45:05  vasilche
 * Commented out memory usage test.
 *
 * Revision 6.20  2003/07/17 20:01:38  vasilche
 * Added test for string reference counting.
 *
 * Revision 6.19  2003/07/09 20:52:20  vasilche
 * Added test for string's reference counting.
 *
 * Revision 6.18  2003/03/25 22:16:11  lavr
 * Conform to new NUL char representation from NStr::PrintableString()
 *
 * Revision 6.17  2003/03/10 18:57:08  kuznets
 * iterate->ITERATE
 *
 * Revision 6.16  2003/02/27 15:34:23  lavr
 * Add tests for stray dots in numbers
 *
 * Revision 6.15  2003/02/26 20:35:13  siyan
 * Added/deleted whitespaces to conform to existing code style.
 *
 * Revision 6.14  2003/02/26 16:47:36  siyan
 * Added test cases to test new version of NStr::StringToUInt8.
 *
 * Revision 6.13  2003/01/21 23:55:44  vakatov
 * Get rid of a warning
 *
 * Revision 6.12  2003/01/14 21:17:58  kuznets
 * + test for NStr::Tokenize
 *
 * Revision 6.11  2003/01/13 14:48:08  kuznets
 * minor fix
 *
 * Revision 6.10  2003/01/10 22:17:39  kuznets
 * Implemented test for NStr::String2Int8
 *
 * Revision 6.9  2003/01/10 00:08:28  vakatov
 * + Int8ToString(),  UInt8ToString()
 *
 * Revision 6.8  2002/10/17 16:56:02  ivanov
 * Added tests for 'b' and 'B' time format symbols
 *
 * Revision 6.7  2002/09/04 19:32:11  vakatov
 * Minor change to reflect the changed handling of '"' by NStr::PrintableString
 *
 * Revision 6.6  2002/07/15 18:17:26  gouriano
 * renamed CNcbiException and its descendents
 *
 * Revision 6.5  2002/07/11 14:18:29  gouriano
 * exceptions replaced by CNcbiException-type ones
 *
 * Revision 6.4  2002/04/16 18:49:08  ivanov
 * Centralize threatment of assert() in tests.
 * Added #include <test/test_assert.h>. CVS log moved to end of file.
 *
 * Revision 6.3  2001/08/30 00:39:58  vakatov
 * + NStr::StringToNumeric()
 *
 * Revision 6.2  2001/05/17 15:05:09  lavr
 * Typos corrected
 *
 * Revision 6.1  2001/03/26 20:34:38  vakatov
 * Initial revision (moved from "coretest.cpp")
 *
 * ==========================================================================
 */
