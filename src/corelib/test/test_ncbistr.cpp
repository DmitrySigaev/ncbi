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

#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbireg.hpp>
#include <algorithm>

#include <test/test_assert.h>  /* This header must go last */


// This is to use the ANSI C++ standard templates without the "std::" prefix
// and to use NCBI C++ entities without the "ncbi::" prefix
USING_NCBI_SCOPE;


/////////////////////////////////
// Utilities
//


static void TestStrings_StrCompare(int expr_res, int valid_res)
{
    int res = expr_res > 0 ? 1 :
        expr_res == 0 ? 0 : -1;
    assert(res == valid_res);
}


typedef struct {
    const char* s1;
    const char* s2;

    int case_res;    /* -1, 0, 1 */
    int nocase_res;  /* -1, 0, 1 */

    SIZE_TYPE n; 
    int n_case_res;    /* -1, 0, 1 */
    int n_nocase_res;  /* -1, 0, 1 */
} SStrCompare;


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
    static const string s_Strings[] = {
        "",
        "1.",
        "-2147483649",
        "-2147483648",
        "-1",
        "0",
        "2147483647",
        "2147483648",
        "4294967295",
        "4294967296",
        " 123 ",
        "-324",
        " 567",
        "+890",
        "zzz"
    };

    NcbiCout << "Test NCBISTR:" << NcbiEndl;

    const size_t count = sizeof(s_Strings) / sizeof(s_Strings[0]);

    //        CExceptionReporterStream reporter(cerr);
        //        CExceptionReporter::SetDefault(&reporter);
        //        CExceptionReporter::EnableDefault(false);
        //        CExceptionReporter::EnableDefault(true);
    //        CExceptionReporter::SetDefault(0);

    SetupDiag(eDS_ToStdout);
        /*

        CExceptionReporter::EnableDefault(true);
        cerr << endl;
        NCBI_REPORT_EXCEPTION(
            "****** default reporter (stream) ******",e);

        CExceptionReporter::SetDefault(0);
        cerr << endl;
        NCBI_REPORT_EXCEPTION(
            "****** default reporter (diag) ******",e);
        */

    for (size_t i = 0;  i < count;  ++i) {
        const string& str = s_Strings[i];
        NcbiCout << "Checking string: '" << str << "':" << NcbiEndl;

        {{
            int value = NStr::StringToNumeric(str);
            NcbiCout << "numeric value: " << value << ", toString: '"
                     << NStr::IntToString(value) << "'" << NcbiEndl;
        }}

        try {
            int value = NStr::StringToInt(str);
            NcbiCout << "int value: " << value << ", toString: '"
                     << NStr::IntToString(value) << "'" << NcbiEndl;
        }
        catch (CException& e) {
            NCBI_REPORT_EXCEPTION("TestStrings",e);
        }
//        STD_CATCH("TestStrings");

        try {
            unsigned int value = NStr::StringToUInt(str);
            NcbiCout << "unsigned int value: " << value << ", toString: '"
                     << NStr::UIntToString(value) << "'" << NcbiEndl;
        }
        catch (CException& e) {
            NCBI_REPORT_EXCEPTION("TestStrings",e);
        }

        try {
            long value = NStr::StringToLong(str);
            NcbiCout << "long value: " << value << ", toString: '"
                     << NStr::IntToString(value) << "'" << NcbiEndl;
            NcbiCout << "      Int8ToString: '"
                     << NStr::Int8ToString(value) << "'" << NcbiEndl;
        }
        catch (CException& e) {
            NCBI_REPORT_EXCEPTION("TestStrings",e);
        }

        try {
            unsigned long value = NStr::StringToULong(str);
            NcbiCout << "unsigned long value: " << value << ", toString: '"
                     << NStr::UIntToString(value) << "'" << NcbiEndl;
            NcbiCout << "      UInt8ToString: '"
                     << NStr::UInt8ToString(value) << "'" << NcbiEndl;
        }
        catch (CException& e) {
            NCBI_REPORT_EXCEPTION("TestStrings",e);
        }

        try {
            double value = NStr::StringToDouble(str);
            NcbiCout << "double value: " << value << ", toString: '"
                     << NStr::DoubleToString(value) << "'" << NcbiEndl;
        }
        catch (CException& e) {
            NCBI_REPORT_EXCEPTION("TestStrings",e);
        }
        
        try {
            Int8 value = NStr::StringToInt8(str);
            NcbiCout << "Int8 value: " << value << ", toString: '"
                     << NStr::Int8ToString(value) << "'" << NcbiEndl;
        }
        catch (CException& e) {
            NCBI_REPORT_EXCEPTION("TestStrings",e);
        }

        try {
            Uint8 value = NStr::StringToUInt8(str);
            NcbiCout << "Uint8 value: " << value << ", toString: '"
                     << NStr::UInt8ToString(value) << "'" << NcbiEndl;
        }
        catch (CException& e) {
            NCBI_REPORT_EXCEPTION("TestStrings",e);
        }
        
    }

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

    NcbiCout << " completed successfully!" << NcbiEndl;


    // NStr::PrintableString()
    assert(NStr::PrintableString(kEmptyStr).empty());
    assert(NStr::PrintableString("AB\\CD\nAB\rCD\vAB\tCD\'AB\"").
            compare("AB\\\\CD\\nAB\\rCD\\vAB\\tCD'AB\\\"") == 0);
    assert(NStr::PrintableString("A\020B" + string(1, '\0') + "CD").
            compare("A\\x10B\\0CD") == 0);


    // NStr::Compare()
    NcbiCout << NcbiEndl << "NStr::Compare() tests...";

    size_t j;
    const SStrCompare* rec;
    string s1, s2;

    for (j = 0;  j < sizeof(s_StrCompare) / sizeof(s_StrCompare[0]);  j++) {
        rec = &s_StrCompare[j];
        s1 = rec->s1;

        TestStrings_StrCompare
            (NStr::Compare(s1, rec->s2, NStr::eCase), rec->case_res);
        TestStrings_StrCompare
            (NStr::Compare(s1, rec->s2, NStr::eNocase), rec->nocase_res);

        TestStrings_StrCompare
            (NStr::Compare(s1, 0, rec->n, rec->s2, NStr::eCase),
             rec->n_case_res);
        TestStrings_StrCompare
            (NStr::Compare(s1, 0, rec->n, rec->s2, NStr::eNocase),
             rec->n_nocase_res);

        s2 = rec->s2;

        TestStrings_StrCompare
            (NStr::Compare(s1, s2, NStr::eCase), rec->case_res);
        TestStrings_StrCompare
            (NStr::Compare(s1, s2, NStr::eNocase), rec->nocase_res);

        TestStrings_StrCompare
            (NStr::Compare(s1, 0, rec->n, s2, NStr::eCase),
             rec->n_case_res);
        TestStrings_StrCompare
            (NStr::Compare(s1, 0, rec->n, s2, NStr::eNocase),
             rec->n_nocase_res);
    }

    assert(NStr::Compare("0123", 0, 2, "12") <  0);
    assert(NStr::Compare("0123", 1, 2, "12") == 0);
    assert(NStr::Compare("0123", 2, 2, "12") >  0);
    assert(NStr::Compare("0123", 3, 2,  "3") == 0);

    NcbiCout << " completed successfully!" << NcbiEndl;

    // NStr::Split()
    NcbiCout << NcbiEndl << "NStr::Split() tests...";

    static const string s_SplitStr[] = {
        "ab+cd+ef",
        "aaAAabBbbb",
        "-abc-def--ghijk---",
        "a12c3ba45acb678bc"
        "nodelim",
        "emptydelim",
        ""
    };
    static const string s_SplitDelim[] = {
        "+", "AB", "-", "abc", "*", "", "*"
    };
    static const string split_result[] = {
        "ab", "cd", "ef",
        "aa", "ab", "bbb",
        "abc", "def", "ghijk",
        "12", "3", "45", "678",
        "nodelim",
        "emptydelim"
    };

    list<string> split;

    for (size_t i = 0; i < sizeof(s_SplitStr) / sizeof(s_SplitStr[0]); i++) {
        NStr::Split(s_SplitStr[i], s_SplitDelim[i], split);
    }

    int i = 0;
    iterate(list<string>, it, split) {
        assert(NStr::Compare(*it, split_result[i++]) == 0);
    }
    
    NcbiCout << " completed successfully!" << NcbiEndl;


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


    for (j = 0;  j < sizeof(s_Tri) / sizeof(s_Tri[0]);  j++) {
        assert(NStr::Compare(s_Tri[j].orig, s_Tri[j].x_lower, NStr::eNocase)
                == 0);
        assert(NStr::Compare(s_Tri[j].orig, s_Tri[j].x_upper, NStr::eNocase)
                == 0);

        string orig = s_Tri[j].orig;
        assert(NStr::Compare(orig, s_Tri[j].x_lower, NStr::eNocase)
                == 0);
        assert(NStr::Compare(orig, s_Tri[j].x_upper, NStr::eNocase)
                == 0);

        string x_lower = s_Tri[j].x_lower;

        {{
            char x_str[16];
            ::strcpy(x_str, s_Tri[j].orig);
            assert(::strlen(x_str) < sizeof(x_str));
            assert(NStr::Compare(NStr::ToLower(x_str), x_lower) == 0);
            ::strcpy(x_str, s_Tri[j].orig);
            assert(NStr::Compare(NStr::ToUpper(x_str), s_Tri[j].x_upper) ==0);
            assert(NStr::Compare(x_lower, NStr::ToLower(x_str)) == 0);
        }}

        {{
            string x_str;
            x_lower = s_Tri[j].x_lower;
            x_str = s_Tri[j].orig;
            assert(NStr::Compare(NStr::ToLower(x_str), x_lower) == 0);
            x_str = s_Tri[j].orig;
            assert(NStr::Compare(NStr::ToUpper(x_str), s_Tri[j].x_upper) ==0);
            assert(NStr::Compare(x_lower, NStr::ToLower(x_str)) == 0);
        }}
    }
    NcbiCout << " completed successfully!" << NcbiEndl;

    NcbiCout << NcbiEndl << "AStrEquiv tests...";

    string as1("abcdefg ");
    string as2("abcdefg ");
    string as3("aBcdEfg ");
    string as4("lsekfu");

    assert( AStrEquiv(as1, as2, PNocase()) == true );
    assert( AStrEquiv(as1, as3, PNocase()) == true );
    assert( AStrEquiv(as3, as4, PNocase()) == false );
    assert( AStrEquiv(as1, as2, PCase())   == true );
    assert( AStrEquiv(as1, as3, PCase())   == false );
    assert( AStrEquiv(as2, as4, PCase())   == false );

    NcbiCout << " completed successfully!" << NcbiEndl;

    NcbiCout << NcbiEndl << "TEST_NCBISTR execution completed successfully!"
             << NcbiEndl << NcbiEndl << NcbiEndl;

    return 0;
}


  
/////////////////////////////////
// APPLICATION OBJECT
//   and
// MAIN
//


int main(int argc, const char* argv[] /*, const char* envp[]*/)
{
    CTestApplication theTestApplication;
    return theTestApplication.AppMain(argc, argv, 0 /*envp*/, eDS_ToMemory);
}


/*
 * --------------------------------------------------------------------------
 * $Log$
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
