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
 * Author: Sergey Sikorskiy
 *
 * File Description: 
 *      Unit-test for functions Convert() and ConvertSafe().
 *
 * ===========================================================================
 */

#include <ncbi_pch.hpp>

#include <util/value_convert.hpp>

// Keep Boost's inclusion of <limits> from breaking under old WorkShop versions.
#if defined(numeric_limits)  &&  defined(NCBI_NUMERIC_LIMITS)
#  undef numeric_limits
#endif

#define BOOST_AUTO_TEST_MAIN
#include <boost/test/auto_unit_test.hpp>
#ifndef BOOST_AUTO_TEST_CASE
#  define BOOST_AUTO_TEST_CASE BOOST_AUTO_UNIT_TEST
#endif

#include <common/test_assert.h>  /* This header must go last */


USING_NCBI_SCOPE;

////////////////////////////////////////////////////////////////////////////////
BOOST_AUTO_TEST_CASE(ValueConvertSafe)
{
    const Uint1 value_Uint1 = 255;
    const Int1 value_Int1 = -127;
    const Uint2 value_Uint2 = 64000;
    const Int2 value_Int2 = -32768;
    const Uint4 value_Uint4 = 4000000000;
    const Int4 value_Int4 = -2147483648;
    const Uint8 value_Uint8 = 9223372036854775808ULL;
    const Int8 value_Int8 = -9223372036854775807LL;
    // const float value_float = float(21.4);
    // const double value_double = 42.8;
    const bool value_bool = true;
    const string value_string = "test string 0987654321";
    const char* value_char = "test char* 1234567890";
    // const char* value_binary = "test binary 1234567890 binary";
    const CTime value_CTime( CTime::eCurrent );
    const string str_value_char(value_char);
    const string str_Uint1("255");
    const string str_Int1("-127");
    const string str_Uint2("64000");
    const string str_Int2("-32768");
    const string str_Uint4("4000000000");
    const string str_Int4("-2147483648");
    const string str_Uint8("9223372036854775808");
    const string str_Int8("-9223372036854775807");
    const string str_bool("yes");
    const string str_float("21.4");
    const string str_double("42.8");

    try {
        // Int8
        {
            Int8 value = 0;

            value = ConvertSafe(value_Int8);
            BOOST_CHECK_EQUAL(value, value_Int8);

            value = ConvertSafe(value_Int4);
            BOOST_CHECK_EQUAL(value, value_Int4);

            value = ConvertSafe(value_Uint4);
            BOOST_CHECK_EQUAL(value, value_Uint4);

            value = ConvertSafe(value_Int2);
            BOOST_CHECK_EQUAL(value, value_Int2);

            value = ConvertSafe(value_Uint2);
            BOOST_CHECK_EQUAL(value, value_Uint2);

            value = ConvertSafe(value_Int1);
            BOOST_CHECK_EQUAL(value, value_Int1);

            value = ConvertSafe(value_Uint1);
            BOOST_CHECK_EQUAL(value, value_Uint1);

            value = ConvertSafe(value_bool);
            BOOST_CHECK_EQUAL(value, value_bool);

            // Doesn't compile ...
            // value = ConvertSafe(value_float);
            // BOOST_CHECK_EQUAL(value, Uint8(value_float));

            // Doesn't compile ...
            // value = ConvertSafe(value_double);
            // BOOST_CHECK_EQUAL(value, Uint8(value_double));

            // Won't convert ...
            // value = ConvertSafe(value_CTime);
            // BOOST_CHECK_EQUAL(value, value_CTime);

            value = ConvertSafe(str_Int8);
            BOOST_CHECK_EQUAL(value, value_Int8);

            value = ConvertSafe(str_Int4);
            BOOST_CHECK_EQUAL(value, value_Int4);

            value = ConvertSafe(str_Uint4);
            BOOST_CHECK_EQUAL(value, value_Uint4);

            value = ConvertSafe(str_Int2);
            BOOST_CHECK_EQUAL(value, value_Int2);

            value = ConvertSafe(str_Uint2);
            BOOST_CHECK_EQUAL(value, value_Uint2);

            value = ConvertSafe(str_Int1);
            BOOST_CHECK_EQUAL(value, value_Int1);

            value = ConvertSafe(str_Uint1);
            BOOST_CHECK_EQUAL(value, value_Uint1);

            // Requires more than one conversion ...
            // value = ConvertSafe(str_bool);
            // BOOST_CHECK_EQUAL(value, value_bool);

            // Won't convert at run-time ...
            // value = ConvertSafe(str_float);
            // BOOST_CHECK_EQUAL(value, value_float);

            // Won't convert at run-time ...
            // value = ConvertSafe(str_double);
            // BOOST_CHECK_EQUAL(value, value_double);
        }

        // Uint8
        {
            Uint8 value = 0;

            value = ConvertSafe(value_Uint8);
            BOOST_CHECK_EQUAL(value, value_Uint8);

            value = ConvertSafe(value_Uint4);
            BOOST_CHECK_EQUAL(value, value_Uint4);

            value = ConvertSafe(value_Uint2);
            BOOST_CHECK_EQUAL(value, value_Uint2);

            value = ConvertSafe(value_Uint1);
            BOOST_CHECK_EQUAL(value, value_Uint1);

            value = ConvertSafe(value_bool);
            BOOST_CHECK_EQUAL(value, value_bool);

            // Doesn't compile ...
            // value = ConvertSafe(value_float);
            // BOOST_CHECK_EQUAL(value, Uint8(value_float));

            // Doesn't compile ...
            // value = ConvertSafe(value_double);
            // BOOST_CHECK_EQUAL(value, Uint8(value_double));

            // Won't convert ...
            // value = ConvertSafe(value_CTime);
            // BOOST_CHECK_EQUAL(value, value_CTime);

            value = ConvertSafe(str_Uint8);
            BOOST_CHECK_EQUAL(value, value_Uint8);

            value = ConvertSafe(str_Uint4);
            BOOST_CHECK_EQUAL(value, value_Uint4);

            value = ConvertSafe(str_Uint2);
            BOOST_CHECK_EQUAL(value, value_Uint2);

            value = ConvertSafe(str_Uint1);
            BOOST_CHECK_EQUAL(value, value_Uint1);

            // Requires more than one conversion ...
            // value = ConvertSafe(str_bool);
            // BOOST_CHECK_EQUAL(value, value_bool);

            // Won't convert at run-time ...
            // value = ConvertSafe(str_float);
            // BOOST_CHECK_EQUAL(value, value_float);

            // Won't convert at run-time ...
            // value = ConvertSafe(str_double);
            // BOOST_CHECK_EQUAL(value, value_double);
        }

        // Int4
        {
            Int4 value = 0;

            value = ConvertSafe(value_Int4);
            BOOST_CHECK_EQUAL(value, value_Int4);

            value = ConvertSafe(value_Int2);
            BOOST_CHECK_EQUAL(value, value_Int2);

            value = ConvertSafe(value_Uint2);
            BOOST_CHECK_EQUAL(value, value_Uint2);

            value = ConvertSafe(value_Int1);
            BOOST_CHECK_EQUAL(value, value_Int1);

            value = ConvertSafe(value_Uint1);
            BOOST_CHECK_EQUAL(value, value_Uint1);

            value = ConvertSafe(value_bool);
            BOOST_CHECK_EQUAL(value, value_bool);

            // Doesn't compile ...
            // value = ConvertSafe(value_float);
            // BOOST_CHECK_EQUAL(value, Uint8(value_float));

            // Doesn't compile ...
            // value = ConvertSafe(value_double);
            // BOOST_CHECK_EQUAL(value, Uint8(value_double));

            // Won't convert ...
            // value = ConvertSafe(value_CTime);
            // BOOST_CHECK_EQUAL(value, value_CTime);

            // Cannot convert at run-time ...
            // value = ConvertSafe(str_Int8);
            // BOOST_CHECK_EQUAL(value, value_Int8);

            value = ConvertSafe(str_Int4);
            BOOST_CHECK_EQUAL(value, value_Int4);

            value = ConvertSafe(str_Int2);
            BOOST_CHECK_EQUAL(value, value_Int2);

            value = ConvertSafe(str_Uint2);
            BOOST_CHECK_EQUAL(value, value_Uint2);

            value = ConvertSafe(str_Int1);
            BOOST_CHECK_EQUAL(value, value_Int1);

            value = ConvertSafe(str_Uint1);
            BOOST_CHECK_EQUAL(value, value_Uint1);

            // Requires more than one conversion ...
            // value = ConvertSafe(str_bool);
            // BOOST_CHECK_EQUAL(value, value_bool);

            // Won't convert at run-time ...
            // value = ConvertSafe(str_float);
            // BOOST_CHECK_EQUAL(value, value_float);

            // Won't convert at run-time ...
            // value = ConvertSafe(str_double);
            // BOOST_CHECK_EQUAL(value, value_double);
        }

        // Uint4
        {
            Uint4 value = 0;

            value = ConvertSafe(value_Uint4);
            BOOST_CHECK_EQUAL(value, value_Uint4);

            value = ConvertSafe(value_Uint2);
            BOOST_CHECK_EQUAL(value, value_Uint2);

            value = ConvertSafe(value_Uint1);
            BOOST_CHECK_EQUAL(value, value_Uint1);

            value = ConvertSafe(value_bool);
            BOOST_CHECK_EQUAL(value, value_bool);

            // Doesn't compile ...
            // value = ConvertSafe(value_float);
            // BOOST_CHECK_EQUAL(value, Uint8(value_float));

            // Doesn't compile ...
            // value = ConvertSafe(value_double);
            // BOOST_CHECK_EQUAL(value, Uint8(value_double));

            // Won't convert ...
            // value = ConvertSafe(value_CTime);
            // BOOST_CHECK_EQUAL(value, value_CTime);

            // Cannot convert at run-time ...
            // value = ConvertSafe(str_Int8);
            // BOOST_CHECK_EQUAL(value, value_Int8);

            value = ConvertSafe(str_Uint4);
            BOOST_CHECK_EQUAL(value, value_Uint4);

            value = ConvertSafe(str_Uint2);
            BOOST_CHECK_EQUAL(value, value_Uint2);

            value = ConvertSafe(str_Uint1);
            BOOST_CHECK_EQUAL(value, value_Uint1);

            // Requires more than one conversion ...
            // value = ConvertSafe(str_bool);
            // BOOST_CHECK_EQUAL(value, value_bool);

            // Won't convert at run-time ...
            // value = ConvertSafe(str_float);
            // BOOST_CHECK_EQUAL(value, value_float);

            // Won't convert at run-time ...
            // value = ConvertSafe(str_double);
            // BOOST_CHECK_EQUAL(value, value_double);
        }

        // Int2
        {
            Int2 value = 0;

            value = ConvertSafe(value_Int2);
            BOOST_CHECK_EQUAL(value, value_Int2);

            value = ConvertSafe(value_Int1);
            BOOST_CHECK_EQUAL(value, value_Int1);

            value = ConvertSafe(value_Uint1);
            BOOST_CHECK_EQUAL(value, value_Uint1);

            value = ConvertSafe(value_bool);
            BOOST_CHECK_EQUAL(value, value_bool);

            // Doesn't compile ...
            // value = ConvertSafe(value_float);
            // BOOST_CHECK_EQUAL(value, Uint8(value_float));

            // Doesn't compile ...
            // value = ConvertSafe(value_double);
            // BOOST_CHECK_EQUAL(value, Uint8(value_double));

            // Won't convert ...
            value = ConvertSafe(value_CTime);
            // BOOST_CHECK_EQUAL(value, value_CTime);

            // Doesn't compile ...
            // value = ConvertSafe(str_Int8);
            // BOOST_CHECK_EQUAL(value, value_Int8);

            // Doesn't compile ...
            // value = ConvertSafe(str_Int4);
            // BOOST_CHECK_EQUAL(value, value_Int4);

            // Doesn't compile ...
            // value = ConvertSafe(str_Int2);
            // BOOST_CHECK_EQUAL(value, value_Int2);

            // Doesn't compile ...
            // value = ConvertSafe(str_Int1);
            // BOOST_CHECK_EQUAL(value, value_Int1);

            // Doesn't compile ...
            // value = ConvertSafe(str_Uint1);
            // BOOST_CHECK_EQUAL(value, value_Uint1);

            // Requires more than one conversion ...
            // value = ConvertSafe(str_bool);
            // BOOST_CHECK_EQUAL(value, value_bool);

            // Won't convert at run-time ...
            // value = ConvertSafe(str_float);
            // BOOST_CHECK_EQUAL(value, value_float);

            // Won't convert at run-time ...
            // value = ConvertSafe(str_double);
            // BOOST_CHECK_EQUAL(value, value_double);
        }

        // Uint2
        {
            Uint2 value = 0;

            value = ConvertSafe(value_Uint2);
            BOOST_CHECK_EQUAL(value, value_Uint2);

            value = ConvertSafe(value_Uint1);
            BOOST_CHECK_EQUAL(value, value_Uint1);

            value = ConvertSafe(value_bool);
            BOOST_CHECK_EQUAL(value, value_bool);

            // Doesn't compile ...
            // value = ConvertSafe(value_float);
            // BOOST_CHECK_EQUAL(value, Uint8(value_float));

            // Doesn't compile ...
            // value = ConvertSafe(value_double);
            // BOOST_CHECK_EQUAL(value, Uint8(value_double));

            // Won't convert ...
            // value = ConvertSafe(value_CTime);
            // BOOST_CHECK_EQUAL(value, value_CTime);

            // Cannot convert at run-time ...
            // value = ConvertSafe(str_Int8);
            // BOOST_CHECK_EQUAL(value, value_Int8);

            // Cannot convert at run-time ...
            // value = ConvertSafe(str_Uint4);
            // BOOST_CHECK_EQUAL(value, value_Uint4);

            // Doesn't compile ...
            // value = ConvertSafe(str_Uint2);
            // BOOST_CHECK_EQUAL(value, value_Uint2);

            // Doesn't compile ...
            // value = ConvertSafe(str_Uint1);
            // BOOST_CHECK_EQUAL(value, value_Uint1);

            // Requires more than one conversion ...
            // value = ConvertSafe(str_bool);
            // BOOST_CHECK_EQUAL(value, value_bool);

            // Won't convert at run-time ...
            // value = ConvertSafe(str_float);
            // BOOST_CHECK_EQUAL(value, value_float);

            // Won't convert at run-time ...
            // value = ConvertSafe(str_double);
            // BOOST_CHECK_EQUAL(value, value_double);
        }

        // Int1
        {
            Int1 value = 0;

            value = ConvertSafe(value_Int1);
            BOOST_CHECK_EQUAL(value, value_Int1);

            value = ConvertSafe(value_bool);
            BOOST_CHECK_EQUAL(value, value_bool);

            // Doesn't compile ...
            // value = ConvertSafe(value_float);
            // BOOST_CHECK_EQUAL(value, Uint8(value_float));

            // Doesn't compile ...
            // value = ConvertSafe(value_double);
            // BOOST_CHECK_EQUAL(value, Uint8(value_double));

            // Won't convert ...
            // value = ConvertSafe(value_CTime);
            // BOOST_CHECK_EQUAL(value, value_CTime);

            // Cannot convert at run-time ...
            // value = ConvertSafe(str_Int8);
            // BOOST_CHECK_EQUAL(value, value_Int8);

            // Cannot convert at run-time ...
            // value = ConvertSafe(str_Int4);
            // BOOST_CHECK_EQUAL(value, value_Int4);

            // Cannot convert at run-time ...
            // value = ConvertSafe(str_Int2);
            // BOOST_CHECK_EQUAL(value, value_Int2);

            // Doesn't compile ...
            // value = ConvertSafe(str_Int1);
            // BOOST_CHECK_EQUAL(value, value_Int1);

            // Requires more than one conversion ...
            // value = ConvertSafe(str_bool);
            // BOOST_CHECK_EQUAL(value, value_bool);

            // Won't convert at run-time ...
            // value = ConvertSafe(str_float);
            // BOOST_CHECK_EQUAL(value, value_float);

            // Won't convert at run-time ...
            // value = ConvertSafe(str_double);
            // BOOST_CHECK_EQUAL(value, value_double);
        }

        // Uint1
        {
            Uint1 value = 0;

            value = ConvertSafe(value_Uint1);
            BOOST_CHECK_EQUAL(value, value_Uint1);

            value = ConvertSafe(value_bool);
            BOOST_CHECK_EQUAL(value, value_bool);

            // Doesn't compile ...
            // value = ConvertSafe(value_float);
            // BOOST_CHECK_EQUAL(value, Uint8(value_float));

            // Doesn't compile ...
            // value = ConvertSafe(value_double);
            // BOOST_CHECK_EQUAL(value, Uint8(value_double));

            // Won't convert ...
            // value = ConvertSafe(value_CTime);
            // BOOST_CHECK_EQUAL(value, value_CTime);

            // Cannot convert at run-time ...
            // value = ConvertSafe(str_Int8);
            // BOOST_CHECK_EQUAL(value, value_Int8);

            // CSafeConvPolicy doesn't allow this conversion.
            // because of return type of NStr::StringToUInt.
            // value = ConvertSafe(str_Uint4);
            // BOOST_CHECK_EQUAL(value, value_Uint4);

            // CSafeConvPolicy doesn't allow this conversion.
            // because of return type of NStr::StringToUInt.
            // value = ConvertSafe(str_Uint2);
            // BOOST_CHECK_EQUAL(value, value_Uint2);

            // CSafeConvPolicy doesn't allow this conversion.
            // because of return type of NStr::StringToUInt.
            // value = ConvertSafe(str_Uint1);
            // BOOST_CHECK_EQUAL(value, value_Uint1);

            // Requires more than one conversion ...
            // value = ConvertSafe(str_bool);
            // BOOST_CHECK_EQUAL(value, value_bool);

            // Won't ConvertSafe at run-time ...
            // value = ConvertSafe(str_float);
            // BOOST_CHECK_EQUAL(value, value_float);

            // Doesn't compile ...
            // value = ConvertSafe(str_double);
            // BOOST_CHECK_EQUAL(value, value_double);
        }

        // bool
        {
            bool value = false;

            value = ConvertSafe(value_Int8);
            BOOST_CHECK_EQUAL(value, true);

            value = ConvertSafe(value_Int4);
            BOOST_CHECK_EQUAL(value, true);

            value = ConvertSafe(value_Uint4);
            BOOST_CHECK_EQUAL(value, true);

            value = ConvertSafe(value_Int2);
            BOOST_CHECK_EQUAL(value, true);

            value = ConvertSafe(value_Uint2);
            BOOST_CHECK_EQUAL(value, true);

            value = ConvertSafe(value_Int1);
            BOOST_CHECK_EQUAL(value, true);

            value = ConvertSafe(value_Uint1);
            BOOST_CHECK_EQUAL(value, true);

            value = ConvertSafe(value_bool);
            BOOST_CHECK_EQUAL(value, value_bool);

            // Doesn't compile ...
            // value = ConvertSafe(value_float);
            // BOOST_CHECK_EQUAL(value, Uint8(value_float));

            // Doesn't compile ...
            // value = ConvertSafe(value_double);
            // BOOST_CHECK_EQUAL(value, Uint8(value_double));

            value = ConvertSafe(value_CTime);
            BOOST_CHECK_EQUAL(value, !value_CTime.IsEmpty());

            // Won't convert at run-time ...
            // value = ConvertSafe(str_Int8);
            // BOOST_CHECK_EQUAL(value, true);

            // Won't convert at run-time ...
            // value = ConvertSafe(str_Int4);
            // BOOST_CHECK_EQUAL(value, true);

            // Won't convert at run-time ...
            // value = ConvertSafe(str_Uint4);
            // BOOST_CHECK_EQUAL(value, true);

            // Won't convert at run-time ...
            // value = ConvertSafe(str_Int2);
            // BOOST_CHECK_EQUAL(value, true);

            // Won't convert at run-time ...
            // value = ConvertSafe(str_Uint2);
            // BOOST_CHECK_EQUAL(value, true);

            // Won't convert at run-time ...
            // value = ConvertSafe(str_Int1);
            // BOOST_CHECK_EQUAL(value, true);

            // Won't convert at run-time ...
            // value = ConvertSafe(str_Uint1);
            // BOOST_CHECK_EQUAL(value, true);

            value = ConvertSafe(str_bool);
            BOOST_CHECK_EQUAL(value, value_bool);

            // Won't convert at run-time ...
            // value = ConvertSafe(str_float);
            // BOOST_CHECK_EQUAL(value, value_float);

            // Won't convert at run-time ...
            // value = ConvertSafe(str_double);
            // BOOST_CHECK_EQUAL(value, value_double);
        }

        // Boolean expressions ...
        {
            const string yes("yes");
            const string no("no");

            if (true && ConvertSafe(yes)) {
                BOOST_CHECK(true);
            } else {
                BOOST_CHECK(false);
            }

            if (false || ConvertSafe(no)) {
                BOOST_CHECK(false);
            } else {
                BOOST_CHECK(true);
            }

            if (ConvertSafe(yes) && ConvertSafe(yes)) {
                BOOST_CHECK(true);
            } else {
                BOOST_CHECK(false);
            }

            if (ConvertSafe(no) || ConvertSafe(no)) {
                BOOST_CHECK(false);
            } else {
                BOOST_CHECK(true);
            }

            if (!ConvertSafe(no)) {
                BOOST_CHECK(true);
            } else {
                BOOST_CHECK(false);
            }
        }

    }
    catch(const CException& ex) {
        BOOST_FAIL(ex.what());
    }
}

////////////////////////////////////////////////////////////////////////////////
BOOST_AUTO_TEST_CASE(ValueConvertRuntime)
{
    const Uint1 value_Uint1 = 255;
    const Int1 value_Int1 = -127;
    const Uint2 value_Uint2 = 64000;
    const Int2 value_Int2 = -32768;
    const Uint4 value_Uint4 = 4000000000;
    const Int4 value_Int4 = -2147483648;
    const Uint8 value_Uint8 = 9223372036854775808ULL;
    const Int8 value_Int8 = -9223372036854775807LL;
    // const float value_float = float(21.4);
    // const double value_double = 42.8;
    const bool value_bool = true;
    const string value_string = "test string 0987654321";
    const char* value_char = "test char* 1234567890";
    // const char* value_binary = "test binary 1234567890 binary";
    const CTime value_CTime( CTime::eCurrent );
    const string str_value_char(value_char);
    const string str_Uint1("255");
    const string str_Int1("-127");
    const string str_Uint2("64000");
    const string str_Int2("-32768");
    const string str_Uint4("4000000000");
    const string str_Int4("-2147483648");
    const string str_Uint8("9223372036854775808");
    const string str_Int8("-9223372036854775807");
    const string str_bool("yes");
    const string str_float("21.4");
    const string str_double("42.8");

    try {
        // Int8
        {
            Int8 value = 0;

            value = Convert(value_Int8);
            BOOST_CHECK_EQUAL(value, value_Int8);

            value = Convert(value_Int4);
            BOOST_CHECK_EQUAL(value, value_Int4);

            value = Convert(value_Uint4);
            BOOST_CHECK_EQUAL(value, value_Uint4);

            value = Convert(value_Int2);
            BOOST_CHECK_EQUAL(value, value_Int2);

            value = Convert(value_Uint2);
            BOOST_CHECK_EQUAL(value, value_Uint2);

            value = Convert(value_Int1);
            BOOST_CHECK_EQUAL(value, value_Int1);

            value = Convert(value_Uint1);
            BOOST_CHECK_EQUAL(value, value_Uint1);

            value = Convert(value_bool);
            BOOST_CHECK_EQUAL(value, value_bool);

            // Doesn't compile ...
            // value = Convert(value_float);
            // BOOST_CHECK_EQUAL(value, Uint8(value_float));

            // Doesn't compile ...
            // value = Convert(value_double);
            // BOOST_CHECK_EQUAL(value, Uint8(value_double));

            // Won't Convert ...
            // value = Convert(value_CTime);
            // BOOST_CHECK_EQUAL(value, value_CTime);

            value = Convert(str_Int8);
            BOOST_CHECK_EQUAL(value, value_Int8);

            value = Convert(str_Int4);
            BOOST_CHECK_EQUAL(value, value_Int4);

            value = Convert(str_Uint4);
            BOOST_CHECK_EQUAL(value, value_Uint4);

            value = Convert(str_Int2);
            BOOST_CHECK_EQUAL(value, value_Int2);

            value = Convert(str_Uint2);
            BOOST_CHECK_EQUAL(value, value_Uint2);

            value = Convert(str_Int1);
            BOOST_CHECK_EQUAL(value, value_Int1);

            value = Convert(str_Uint1);
            BOOST_CHECK_EQUAL(value, value_Uint1);

            // Requires more than one conversion ...
            // value = Convert(str_bool);
            // BOOST_CHECK_EQUAL(value, value_bool);

            // Won't Convert at run-time ...
            // value = Convert(str_float);
            // BOOST_CHECK_EQUAL(value, value_float);

            // Won't Convert at run-time ...
            // value = Convert(str_double);
            // BOOST_CHECK_EQUAL(value, value_double);
        }

        // Uint8
        {
            Uint8 value = 0;

            value = Convert(value_Uint8);
            BOOST_CHECK_EQUAL(value, value_Uint8);

            value = Convert(value_Uint4);
            BOOST_CHECK_EQUAL(value, value_Uint4);

            value = Convert(value_Uint2);
            BOOST_CHECK_EQUAL(value, value_Uint2);

            value = Convert(value_Uint1);
            BOOST_CHECK_EQUAL(value, value_Uint1);

            value = Convert(value_bool);
            BOOST_CHECK_EQUAL(value, value_bool);

            // Doesn't compile ...
            // value = Convert(value_float);
            // BOOST_CHECK_EQUAL(value, Uint8(value_float));

            // Doesn't compile ...
            // value = Convert(value_double);
            // BOOST_CHECK_EQUAL(value, Uint8(value_double));

            // Won't Convert ...
            // value = Convert(value_CTime);
            // BOOST_CHECK_EQUAL(value, value_CTime);

            value = Convert(str_Uint8);
            BOOST_CHECK_EQUAL(value, value_Uint8);

            value = Convert(str_Uint4);
            BOOST_CHECK_EQUAL(value, value_Uint4);

            value = Convert(str_Uint2);
            BOOST_CHECK_EQUAL(value, value_Uint2);

            value = Convert(str_Uint1);
            BOOST_CHECK_EQUAL(value, value_Uint1);

            // Requires more than one conversion ...
            // value = Convert(str_bool);
            // BOOST_CHECK_EQUAL(value, value_bool);

            // Won't Convert at run-time ...
            // value = Convert(str_float);
            // BOOST_CHECK_EQUAL(value, value_float);

            // Won't Convert at run-time ...
            // value = Convert(str_double);
            // BOOST_CHECK_EQUAL(value, value_double);
        }

        // Int4
        {
            Int4 value = 0;

            value = Convert(value_Int4);
            BOOST_CHECK_EQUAL(value, value_Int4);

            value = Convert(value_Int2);
            BOOST_CHECK_EQUAL(value, value_Int2);

            value = Convert(value_Uint2);
            BOOST_CHECK_EQUAL(value, value_Uint2);

            value = Convert(value_Int1);
            BOOST_CHECK_EQUAL(value, value_Int1);

            value = Convert(value_Uint1);
            BOOST_CHECK_EQUAL(value, value_Uint1);

            value = Convert(value_bool);
            BOOST_CHECK_EQUAL(value, value_bool);

            // Doesn't compile ...
            // value = Convert(value_float);
            // BOOST_CHECK_EQUAL(value, Uint8(value_float));

            // Doesn't compile ...
            // value = Convert(value_double);
            // BOOST_CHECK_EQUAL(value, Uint8(value_double));

            // Won't Convert ...
            // value = Convert(value_CTime);
            // BOOST_CHECK_EQUAL(value, value_CTime);

            // Cannot Convert at run-time ...
            // value = Convert(str_Int8);
            // BOOST_CHECK_EQUAL(value, value_Int8);

            value = Convert(str_Int4);
            BOOST_CHECK_EQUAL(value, value_Int4);

            value = Convert(str_Int2);
            BOOST_CHECK_EQUAL(value, value_Int2);

            value = Convert(str_Uint2);
            BOOST_CHECK_EQUAL(value, value_Uint2);

            value = Convert(str_Int1);
            BOOST_CHECK_EQUAL(value, value_Int1);

            value = Convert(str_Uint1);
            BOOST_CHECK_EQUAL(value, value_Uint1);

            // Requires more than one conversion ...
            // value = Convert(str_bool);
            // BOOST_CHECK_EQUAL(value, value_bool);

            // Won't Convert at run-time ...
            // value = Convert(str_float);
            // BOOST_CHECK_EQUAL(value, value_float);

            // Won't Convert at run-time ...
            // value = Convert(str_double);
            // BOOST_CHECK_EQUAL(value, value_double);
        }

        // Uint4
        {
            Uint4 value = 0;

            value = Convert(value_Uint4);
            BOOST_CHECK_EQUAL(value, value_Uint4);

            value = Convert(value_Uint2);
            BOOST_CHECK_EQUAL(value, value_Uint2);

            value = Convert(value_Uint1);
            BOOST_CHECK_EQUAL(value, value_Uint1);

            value = Convert(value_bool);
            BOOST_CHECK_EQUAL(value, value_bool);

            // Doesn't compile ...
            // value = Convert(value_float);
            // BOOST_CHECK_EQUAL(value, Uint8(value_float));

            // Doesn't compile ...
            // value = Convert(value_double);
            // BOOST_CHECK_EQUAL(value, Uint8(value_double));

            // Won't Convert ...
            // value = Convert(value_CTime);
            // BOOST_CHECK_EQUAL(value, value_CTime);

            // Cannot Convert at run-time ...
            // value = Convert(str_Int8);
            // BOOST_CHECK_EQUAL(value, value_Int8);

            value = Convert(str_Uint4);
            BOOST_CHECK_EQUAL(value, value_Uint4);

            value = Convert(str_Uint2);
            BOOST_CHECK_EQUAL(value, value_Uint2);

            value = Convert(str_Uint1);
            BOOST_CHECK_EQUAL(value, value_Uint1);

            // Requires more than one conversion ...
            // value = Convert(str_bool);
            // BOOST_CHECK_EQUAL(value, value_bool);

            // Won't Convert at run-time ...
            // value = Convert(str_float);
            // BOOST_CHECK_EQUAL(value, value_float);

            // Won't Convert at run-time ...
            // value = Convert(str_double);
            // BOOST_CHECK_EQUAL(value, value_double);
        }

        // Int2
        {
            Int2 value = 0;

            value = Convert(value_Int2);
            BOOST_CHECK_EQUAL(value, value_Int2);

            value = Convert(value_Int1);
            BOOST_CHECK_EQUAL(value, value_Int1);

            value = Convert(value_Uint1);
            BOOST_CHECK_EQUAL(value, value_Uint1);

            value = Convert(value_bool);
            BOOST_CHECK_EQUAL(value, value_bool);

            // Doesn't compile ...
            // value = Convert(value_float);
            // BOOST_CHECK_EQUAL(value, Uint8(value_float));

            // Doesn't compile ...
            // value = Convert(value_double);
            // BOOST_CHECK_EQUAL(value, Uint8(value_double));

            // Won't Convert ...
            // value = Convert(value_CTime);
            // BOOST_CHECK_EQUAL(value, value_CTime);

            // Cannot Convert at run-time ...
            // value = Convert(str_Int8);
            // BOOST_CHECK_EQUAL(value, value_Int8);

            // Cannot Convert at run-time ...
            // value = Convert(str_Int4);
            // BOOST_CHECK_EQUAL(value, value_Int4);

            value = Convert(str_Int2);
            BOOST_CHECK_EQUAL(value, value_Int2);

            value = Convert(str_Int1);
            BOOST_CHECK_EQUAL(value, value_Int1);

            value = Convert(str_Uint1);
            BOOST_CHECK_EQUAL(value, value_Uint1);

            // Requires more than one conversion ...
            // value = Convert(str_bool);
            // BOOST_CHECK_EQUAL(value, value_bool);

            // Won't Convert at run-time ...
            // value = Convert(str_float);
            // BOOST_CHECK_EQUAL(value, value_float);

            // Won't Convert at run-time ...
            // value = Convert(str_double);
            // BOOST_CHECK_EQUAL(value, value_double);
        }

        // Uint2
        {
            Uint2 value = 0;

            value = Convert(value_Uint2);
            BOOST_CHECK_EQUAL(value, value_Uint2);

            value = Convert(value_Uint1);
            BOOST_CHECK_EQUAL(value, value_Uint1);

            value = Convert(value_bool);
            BOOST_CHECK_EQUAL(value, value_bool);

            // Doesn't compile ...
            // value = Convert(value_float);
            // BOOST_CHECK_EQUAL(value, Uint8(value_float));

            // Doesn't compile ...
            // value = Convert(value_double);
            // BOOST_CHECK_EQUAL(value, Uint8(value_double));

            // Won't Convert ...
            // value = Convert(value_CTime);
            // BOOST_CHECK_EQUAL(value, value_CTime);

            // Cannot Convert at run-time ...
            // value = Convert(str_Int8);
            // BOOST_CHECK_EQUAL(value, value_Int8);

            // Cannot Convert at run-time ...
            // value = Convert(str_Uint4);
            // BOOST_CHECK_EQUAL(value, value_Uint4);

            value = Convert(str_Uint2);
            BOOST_CHECK_EQUAL(value, value_Uint2);

            value = Convert(str_Uint1);
            BOOST_CHECK_EQUAL(value, value_Uint1);

            // Requires more than one conversion ...
            // value = Convert(str_bool);
            // BOOST_CHECK_EQUAL(value, value_bool);

            // Won't Convert at run-time ...
            // value = Convert(str_float);
            // BOOST_CHECK_EQUAL(value, value_float);

            // Won't Convert at run-time ...
            // value = Convert(str_double);
            // BOOST_CHECK_EQUAL(value, value_double);
        }

        // Int1
        {
            Int1 value = 0;

            value = Convert(value_Int1);
            BOOST_CHECK_EQUAL(value, value_Int1);

            value = Convert(value_bool);
            BOOST_CHECK_EQUAL(value, value_bool);

            // Doesn't compile ...
            // value = Convert(value_float);
            // BOOST_CHECK_EQUAL(value, Uint8(value_float));

            // Doesn't compile ...
            // value = Convert(value_double);
            // BOOST_CHECK_EQUAL(value, Uint8(value_double));

            // Won't Convert ...
            // value = Convert(value_CTime);
            // BOOST_CHECK_EQUAL(value, value_CTime);

            // Cannot Convert at run-time ...
            // value = Convert(str_Int8);
            // BOOST_CHECK_EQUAL(value, value_Int8);

            // Cannot Convert at run-time ...
            // value = Convert(str_Int4);
            // BOOST_CHECK_EQUAL(value, value_Int4);

            // Cannot Convert at run-time ...
            // value = Convert(str_Int2);
            // BOOST_CHECK_EQUAL(value, value_Int2);

            value = Convert(str_Int1);
            BOOST_CHECK_EQUAL(value, value_Int1);

            // Requires more than one conversion ...
            // value = Convert(str_bool);
            // BOOST_CHECK_EQUAL(value, value_bool);

            // Won't Convert at run-time ...
            // value = Convert(str_float);
            // BOOST_CHECK_EQUAL(value, value_float);

            // Won't Convert at run-time ...
            // value = Convert(str_double);
            // BOOST_CHECK_EQUAL(value, value_double);
        }

        // Uint1
        {
            Uint1 value = 0;

            value = Convert(value_Uint1);
            BOOST_CHECK_EQUAL(value, value_Uint1);

            value = Convert(value_bool);
            BOOST_CHECK_EQUAL(value, value_bool);

            // Doesn't compile ...
            // value = Convert(value_float);
            // BOOST_CHECK_EQUAL(value, Uint8(value_float));

            // Doesn't compile ...
            // value = Convert(value_double);
            // BOOST_CHECK_EQUAL(value, Uint8(value_double));

            // Won't Convert ...
            // value = Convert(value_CTime);
            // BOOST_CHECK_EQUAL(value, value_CTime);

            // Cannot Convert at run-time ...
            // value = Convert(str_Int8);
            // BOOST_CHECK_EQUAL(value, value_Int8);

            // CSafeConvPolicy doesn't allow this conversion.
            // because of return type of NStr::StringToUInt.
            // value = Convert(str_Uint4);
            // BOOST_CHECK_EQUAL(value, value_Uint4);

            // CSafeConvPolicy doesn't allow this conversion.
            // because of return type of NStr::StringToUInt.
            // value = Convert(str_Uint2);
            // BOOST_CHECK_EQUAL(value, value_Uint2);

            // CSafeConvPolicy doesn't allow this conversion.
            // because of return type of NStr::StringToUInt.
            // value = Convert(str_Uint1);
            // BOOST_CHECK_EQUAL(value, value_Uint1);

            // Requires more than one conversion ...
            // value = Convert(str_bool);
            // BOOST_CHECK_EQUAL(value, value_bool);

            // Won't Convert at run-time ...
            // value = Convert(str_float);
            // BOOST_CHECK_EQUAL(value, value_float);

            // Won't Convert at run-time ...
            // value = Convert(str_double);
            // BOOST_CHECK_EQUAL(value, value_double);
        }

        // bool
        {
            bool value = false;

            value = Convert(value_Int8);
            BOOST_CHECK_EQUAL(value, true);

            value = Convert(value_Int4);
            BOOST_CHECK_EQUAL(value, true);

            value = Convert(value_Uint4);
            BOOST_CHECK_EQUAL(value, true);

            value = Convert(value_Int2);
            BOOST_CHECK_EQUAL(value, true);

            value = Convert(value_Uint2);
            BOOST_CHECK_EQUAL(value, true);

            value = Convert(value_Int1);
            BOOST_CHECK_EQUAL(value, true);

            value = Convert(value_Uint1);
            BOOST_CHECK_EQUAL(value, true);

            value = Convert(value_bool);
            BOOST_CHECK_EQUAL(value, value_bool);

            // Doesn't compile ...
            // value = Convert(value_float);
            // BOOST_CHECK_EQUAL(value, Uint8(value_float));

            // Doesn't compile ...
            // value = Convert(value_double);
            // BOOST_CHECK_EQUAL(value, Uint8(value_double));

            value = Convert(value_CTime);
            BOOST_CHECK_EQUAL(value, !value_CTime.IsEmpty());

            // Won't Convert at run-time ...
            // value = Convert(str_Int8);
            // BOOST_CHECK_EQUAL(value, true);

            // Won't Convert at run-time ...
            // value = Convert(str_Int4);
            // BOOST_CHECK_EQUAL(value, true);

            // Won't Convert at run-time ...
            // value = Convert(str_Uint4);
            // BOOST_CHECK_EQUAL(value, true);

            // Won't Convert at run-time ...
            // value = Convert(str_Int2);
            // BOOST_CHECK_EQUAL(value, true);

            // Won't Convert at run-time ...
            // value = Convert(str_Uint2);
            // BOOST_CHECK_EQUAL(value, true);

            // Won't Convert at run-time ...
            // value = Convert(str_Int1);
            // BOOST_CHECK_EQUAL(value, true);

            // Won't Convert at run-time ...
            // value = Convert(str_Uint1);
            // BOOST_CHECK_EQUAL(value, true);

            value = Convert(str_bool);
            BOOST_CHECK_EQUAL(value, value_bool);

            // Won't Convert at run-time ...
            // value = Convert(str_float);
            // BOOST_CHECK_EQUAL(value, value_float);

            // Won't Convert at run-time ...
            // value = Convert(str_double);
            // BOOST_CHECK_EQUAL(value, value_double);
        }

    }
    catch(const CException& ex) {
        BOOST_FAIL(ex.what());
    }
}

