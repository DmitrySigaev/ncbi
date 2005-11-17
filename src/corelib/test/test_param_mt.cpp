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
 * Author:  Aleksey Grichenko
 *
 * File Description:
 *   Test for parameters in multithreaded environment
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/test_mt.hpp>
#include <corelib/ncbi_param.hpp>

#include <test/test_assert.h>  /* This header must go last */

USING_NCBI_SCOPE;


DEFINE_STATIC_FAST_MUTEX(s_GlobalLock);

const string   kStrParam_Default  = "StrParam Default";
const bool     kBoolParam_Default = false;
const unsigned kUIntParam_Default = 123;
const string   kStaticStr_Default  = "StaticStringParam";

NCBI_PARAM_DECL(int, ParamTest, ThreadIdx);
NCBI_PARAM_DECL(string, ParamTest, StrParam);
NCBI_PARAM_DECL(bool, ParamTest, BoolParam);
NCBI_PARAM_DECL(unsigned int, ParamTest, UIntParam);
NCBI_PARAM_DECL(string, ParamTest, StaticStr);

NCBI_PARAM_DEF(int, ParamTest, ThreadIdx, 0);
NCBI_PARAM_DEF(string, ParamTest, StrParam, kStrParam_Default);
NCBI_PARAM_DEF(bool, ParamTest, BoolParam, kBoolParam_Default);
NCBI_PARAM_DEF(unsigned int, ParamTest, UIntParam, kUIntParam_Default);
NCBI_PARAM_DEF(string, ParamTest, StaticStr, kStaticStr_Default);

struct STestStruct
{
    STestStruct(void) : first(0), second(0) {}
    STestStruct(int f, int s) : first(f), second(s) {}
    bool operator==(const STestStruct& val)
    { return first == val.first  &&  second == val.second; }
    int first, second;
};

istream& operator>>(istream& in, STestStruct val)
{
    in >> val.first >> val.second;
    return in;
}

ostream& operator<<(ostream& out, const STestStruct val)
{
    out << val.first << ' ' << val.second;
    return out;
}

const STestStruct kDefaultPair(123, 456);

NCBI_PARAM_DECL(STestStruct, ParamTest, Struct);
NCBI_PARAM_DEF(STestStruct, ParamTest, Struct, kDefaultPair);

/////////////////////////////////////////////////////////////////////////////
//  Test application

class CTestParamApp : public CThreadedApp
{
public:
    virtual bool Thread_Run(int idx);
protected:
    virtual bool TestApp_Init(void);
    virtual bool TestApp_Exit(void);
private:
    typedef NCBI_PARAM_TYPE(ParamTest, ThreadIdx) TParam_ThreadIdx;
    typedef NCBI_PARAM_TYPE(ParamTest, StrParam) TParam_StrParam;
    typedef NCBI_PARAM_TYPE(ParamTest, BoolParam) TParam_BoolParam;
    typedef NCBI_PARAM_TYPE(ParamTest, UIntParam) TParam_UIntParam;
    typedef NCBI_PARAM_TYPE(ParamTest, Struct) TParam_Struct;

    //TParam_ThreadIdx m_ThreadIdx;
    //TParam_StrParam  m_StrParam;
    //TParam_BoolParam m_BoolParam;
    //TParam_UIntParam m_UIntParam;
};


typedef NCBI_PARAM_TYPE(ParamTest, StaticStr) TParam_StaticStr;

static TParam_StaticStr s_StrParam;

bool CTestParamApp::Thread_Run(int idx)
{
    // Set thread default value
    TParam_ThreadIdx::SetThreadDefault(idx);
    string str_idx = NStr::IntToString(idx);

    _ASSERT(TParam_StrParam::GetDefault() == kStrParam_Default);
    _ASSERT(s_StrParam.Get() == kStaticStr_Default);

    // Initialize parameter with global default
    TParam_StrParam str_param1;
    // Set new thread default
    TParam_StrParam::SetThreadDefault(kStrParam_Default + str_idx);
    // Initialize parameter with thread default
    TParam_StrParam str_param2;

    // Parameters should keep the value set during initialization
    _ASSERT(str_param1.Get() == kStrParam_Default);
    _ASSERT(str_param2.Get() == kStrParam_Default + str_idx);
    // Restore thread default to global default for testing in ST mode
    TParam_StrParam::SetThreadDefault(kStrParam_Default);

    // Set thread default value
    bool odd = idx % 2 != 0;
    TParam_BoolParam::SetThreadDefault(odd);
    TParam_BoolParam bool_param1;
    // Check if the value is correct
    _ASSERT(bool_param1.Get() == odd);
    // Change global default not to match the thread default
    TParam_BoolParam::SetDefault(!bool_param1.Get());
    // The parameter should use thread default rather than global default
    TParam_BoolParam bool_param2;
    _ASSERT(bool_param2.Get() == odd);

    TParam_Struct struct_param;
    _ASSERT(struct_param.Get() == kDefaultPair);

    // Thread default value should not change
    _ASSERT(TParam_ThreadIdx::GetThreadDefault() == idx);
    return true;
}

bool CTestParamApp::TestApp_Init(void)
{
    NcbiCout << NcbiEndl
             << "Testing parameters with "
             << NStr::IntToString(s_NumThreads)
             << " threads..."
             << NcbiEndl;

    return true;
}

bool CTestParamApp::TestApp_Exit(void)
{
    NcbiCout << "Test completed successfully!"
             << NcbiEndl << NcbiEndl;
    return true;
}



/////////////////////////////////////////////////////////////////////////////
//  MAIN

int main(int argc, const char* argv[]) 
{
    CTestParamApp app;
    return app.AppMain(argc, argv, 0, eDS_Default, 0);
}



/*
 * ===========================================================================
 * $Log$
 * Revision 1.2  2005/11/17 23:14:54  grichenk
 * Fixed test in ST mode
 *
 * Revision 1.1  2005/11/17 19:15:23  grichenk
 * Initial revision
 *
 * Revision 6.9  2005/11/07 23:30:13  vakatov
 * Recent fix in the registry code caught a bug in the test (ironic, isn't it?)
 *
 * Revision 6.8  2004/05/14 13:59:51  gorelenk
 * Added include of ncbi_pch.hpp
 *
 * Revision 6.7  2003/05/14 20:57:30  ivanov
 * Made WIN to UNIX eol conversion
 *
 * Revision 6.6  2003/05/14 20:49:04  ivanov
 * Added MT lock for m_Registry protection
 *
 * Revision 6.5  2003/01/31 16:48:32  lavr
 * Remove unused variable "e" from catch() clause
 *
 * Revision 6.4  2002/12/30 23:23:09  vakatov
 * + GetString(), GetInt(), GetBool(), GetDouble() -- with defaults,
 * conversions and error handling control (to extend Get()'s functionality).
 *
 * Revision 6.3  2002/04/23 13:11:50  gouriano
 * test_mt.cpp/hpp moved into another location
 *
 * Revision 6.2  2002/04/16 18:49:08  ivanov
 * Centralize threatment of assert() in tests.
 * Added #include <test/test_assert.h>. CVS log moved to end of file.
 *
 * Revision 6.1  2001/04/06 15:57:09  grichenk
 * Initial revision
 *
 * ===========================================================================
 */
