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
 * Author:  Anton Butanayev
 *
 * File Description:
 *   Test for the command-line arguments' processing ("ncbiargs.[ch]pp"):
 *
 * ---------------------------------------------------------------------------
 * $Log$
 * Revision 6.7  2000/10/11 21:03:50  vakatov
 * Cleanup to avoid 64-bit to 32-bit values truncation, etc.
 * (reported by Forte6 Patch 109490-01)
 *
 * Revision 6.6  2000/10/06 21:57:07  butanaev
 * Added Allow() function. Added classes CArgAllowValue, CArgAllowIntInterval.
 *
 * Revision 6.5  2000/09/29 17:11:01  butanaev
 * Got rid of IsDefaultValue(), added IsProvided().
 *
 * Revision 6.4  2000/09/28 21:00:21  butanaev
 * fPreOpen with opposite meaning took over fDelayOpen.
 * IsDefaultValue() added which returns true if no
 * value for an optional argument was provided in cmd. line.
 *
 * Revision 6.3  2000/09/22 21:26:28  butanaev
 * Added example with default arg values.
 *
 * Revision 6.2  2000/09/12 15:01:30  butanaev
 * Examples now switching by environment variable EXAMPLE_NUM.
 *
 * Revision 6.1  2000/08/31 23:55:35  vakatov
 * Initial revision
 *
 * ===========================================================================
 */

#include <stdio.h>
#include <corelib/ncbiargs.hpp>


USING_NCBI_SCOPE;


// Allowing
void Example9(CArgDescriptions& m, int argc, const char* argv[])
{
    m.AddKey("a", "alphaNumericKey",
             "A test alpha-num key argument, can have values (one, two) ",
             CArgDescriptions::eAlnum);

    m.AddKey("i", "integerKey",
             "This is a test integer key argument with values [5 .. 10]",
             CArgDescriptions::eInteger);

    m.Allow("a", new CArgAllowValue("one"));
    m.Allow("a", new CArgAllowValue("two"));
    m.Allow("i", new CArgAllowIntInterval(5, 10));

    auto_ptr<CArgs> a(m.CreateArgs(argc, argv));

    cout << "a=" << (*a)["a"].AsString() << endl;
    cout << "i=" << (*a)["i"].AsInteger() << endl;
}


// Argument with default walue
void Example8(CArgDescriptions& m, int argc, const char* argv[])
{
    m.AddOptionalKey("k",
                     "alphaNumericKey",
                     "This is an optional argument",
                     CArgDescriptions::eAlnum,
                     0,
                     "CORELIB");

    auto_ptr<CArgs> a(m.CreateArgs(argc, argv));

    if(a->IsProvided("k"))
        cout << "argument value was provided" << endl;
    else
        cout << "default argument value used" << endl;

    cout << "k=" << (*a)["k"].AsString()  << endl;
}


// Position arguments - advanced
void Example7(CArgDescriptions& m, int argc, const char* argv[])
{
    m.AddPlain("p1", "This is a plain argument", CArgDescriptions::eAlnum);
    m.AddPlain("p2", "This is a plain argument", CArgDescriptions::eAlnum);
    m.AddExtra("This is an extra argument",      CArgDescriptions::eInteger);

    m.SetConstraint(CArgDescriptions::eMoreOrEqual, 4);

    auto_ptr<CArgs> a(m.CreateArgs(argc, argv));

    cout << "p1=" << (*a)["p1"].AsString()  << endl;
    cout << "p2=" << (*a)["p2"].AsString()  << endl;
    cout << "#1=" << (*a)["#1"].AsInteger() << endl;
    cout << "#2=" << (*a)["#2"].AsInteger() << endl;
}


// Position arguments
void Example6(CArgDescriptions& m, int argc, const char* argv[])
{
    m.AddPlain("p1", "This is a plain argument", CArgDescriptions::eAlnum);
    m.AddPlain("p2", "This is a plain argument", CArgDescriptions::eAlnum);

    auto_ptr<CArgs> a(m.CreateArgs(argc, argv));

    cout << "p1=" << (*a)["p1"].AsString() << endl;
    cout << "p2=" << (*a)["p2"].AsString() << endl;
}


// Files - advanced
void Example5(CArgDescriptions& m, int argc, const char* argv[])
{
    m.AddKey("if", "inputFile", "This is an input file argument",
             CArgDescriptions::eInputFile, CArgDescriptions::fPreOpen);

    m.AddKey("of", "outputFile", "This is an output file argument",
             CArgDescriptions::eOutputFile,
             CArgDescriptions::fAppend);

    auto_ptr<CArgs> a(m.CreateArgs(argc, argv));

    while(! (*a)["if"].AsInputFile().eof()) {
        string tmp;
        (*a)["if"].AsInputFile () >> tmp;
        (*a)["of"].AsOutputFile() << tmp << endl;
    }
}


// Files
void Example4(CArgDescriptions& m, int argc, const char* argv[])
{
    m.AddKey("if", "inputFile", "This is an input file argument",
             CArgDescriptions::eInputFile);

    m.AddKey("of", "outputFile", "This is an output file argument",
             CArgDescriptions::eOutputFile);

    auto_ptr<CArgs> a(m.CreateArgs(argc, argv));

    while( !(*a)["if"].AsInputFile().eof() ) {
        string tmp;
        (*a)["if"].AsInputFile () >> tmp;
        (*a)["of"].AsOutputFile() << tmp << endl;
    }
}


// Optionals
void Example3(CArgDescriptions& m, int argc, const char* argv[])
{
    m.AddOptionalKey("k1", "fistOptionalKey",
                     "This is an optional argument", CArgDescriptions::eAlnum);
    m.AddOptionalKey("k2", "secondOptionalKey",
                     "This is an optional argument", CArgDescriptions::eAlnum);

    m.AddFlag("f1", "Flag 1");
    m.AddFlag("f2", "Flag 2");

    auto_ptr<CArgs> a(m.CreateArgs(argc, argv));

    if (a->Exist("k1"))
        cout << "k1=" << (*a)["k1"].AsString() << endl;
    if (a->Exist("k2"))
        cout << "k2=" << (*a)["k2"].AsString() << endl;

    if (a->Exist("f1"))
        cout << "f1 was provided" << endl;
    if (a->Exist("f2"))
        cout << "f2 was provided" << endl;
}


// Data types
void Example2(CArgDescriptions& m, int argc, const char* argv[])
{
    m.AddKey("ka", "alphaNumericKey", "This is a test alpha-num key argument",
             CArgDescriptions::eAlnum);

    m.AddKey("kb", "booleanKey", "This is a test boolean key argument",
             CArgDescriptions::eBoolean);

    m.AddKey("ki", "integerKey", "This is a test integer key argument",
             CArgDescriptions::eInteger);

    m.AddKey("kd", "doubleKey", "This is a test double key argument",
             CArgDescriptions::eDouble);

    auto_ptr<CArgs> a(m.CreateArgs(argc, argv));

    cout << "ka=" << (*a)["ka"].AsString() << endl;
    cout << "kb=" << ((*a)["kb"].AsBoolean() ? "True" : "Flase") << endl;
    cout << "ki=" << (*a)["ki"].AsInteger() << endl;
    cout << "kd=" << (*a)["kd"].AsDouble() << endl;
}


// The simplest example
void Example1(CArgDescriptions& m, int argc, const char* argv[])
{
    m.AddKey("k", "key", "This is a key argument", CArgDescriptions::eAlnum);

    auto_ptr<CArgs> a(m.CreateArgs(argc, argv));

    cout << "k=" << (*a)["k"].AsString() << endl;
}

// Tests' array
typedef void (*FTest)(CArgDescriptions& m, int argc, const char* argv[]);


static FTest s_Test[] =
{
    Example1,
    Example2,
    Example3,
    Example4,
    Example5,
    Example6,
    Example7,
    Example8,
    Example9
};


// Main
int main(int argc, const char* argv[])
{
    char* errorMsg =
        "To run example number N environment variable EXAMPLE_NUM "
        "must be set to N, where N in 1 .. %lu interval";

    size_t size = sizeof(s_Test) / sizeof(s_Test[0]);
    char tmp[1000];
    sprintf(tmp, errorMsg, (unsigned int) size);

    char* exampleNum = getenv("EXAMPLE_NUM");
    if( !exampleNum) {
        cerr << tmp << endl;
        return 0;
    }

    size_t i;
    try {
        i = NStr::StringToULong(exampleNum);
    } catch (...) {
        i = 0;
    }

    if (i < 1  ||  i > size) {
        cerr << tmp << endl;
        return 0;
    }

    i--;

    CArgDescriptions m;
    try {
        m.SetUsageContext(argv[0], "Program for argument testing, #" +
                          NStr::UIntToString(i + 1));
        s_Test[i] (m, argc, argv);
    } catch (exception& e) {
        string a;
        cerr << e.what() << endl;
        cerr << string(72, '=') << endl;
        cerr << m.PrintUsage(a) << endl;
    }

    return 0;
}
