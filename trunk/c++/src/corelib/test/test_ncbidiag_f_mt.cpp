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
 * Author:  Vyacheslav Kononenko
 *
 * File Description:
 *   Test for "NCBIDIAG" filter mechanism in multithreaded environment
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/test_mt.hpp>
#include <corelib/ncbidiag.hpp>
#include <algorithm>

#include <test/test_assert.h>  /* This header must go last */


USING_NCBI_SCOPE;


#define NUM_TESTS 9



/////////////////////////////////////////////////////////////////////////////
//  Test application

class CTestDiagApp : public CThreadedApp
{
public:
    void SetCase(int case_num);

    virtual bool Thread_Init(int idx);
    virtual bool Thread_Run (int idx);

protected:
    virtual bool TestApp_Init(void);
    virtual bool TestApp_Exit(void);
    virtual bool TestApp_Args(CArgDescriptions& args);

private:
    typedef pair<int,int>           TCase;
    typedef map<TCase, string>      TMessages;
    typedef set< pair<TCase,int> >  TThreads;

    void   x_SetExpects(const string& expects);
    string x_MakeMessage(const string& msg, 
                         int           idx, 
                         const TCase&  ncase);
    void   x_PrintMessages(int         case_number, 
                           int         idx, 
                           const char* module, 
                           const char* nclass, 
                           const char* function);
    
    string    m_Case;
    TMessages m_Messages;
    TThreads  m_CaseExpected;
    bool      m_Expected[NUM_TESTS];
};


static CNcbiOstrstream s_Sout;

void CTestDiagApp::SetCase(int case_num)
{
    switch(case_num){
    case 1 :
        m_Case = "no filter";
        x_SetExpects("111111111");
        break;
    case 2 :
        SetDiagFilter(eDiagFilter_All, "module");
        m_Case = "filter \"module\"";
        x_SetExpects("111100000");
        break;
    case 3 :
        SetDiagFilter(eDiagFilter_All, "::class");
        m_Case = "filter \"::class\"";
        x_SetExpects("110011000");
        break;
    case 4:
        SetDiagFilter(eDiagFilter_All, "function()");
        m_Case = "filter \"function()\"";
        x_SetExpects("101010100");
        break;
    case 5:
        SetDiagFilter(eDiagFilter_All, "?");
        m_Case = "filter \"?\"";
        x_SetExpects("000011110");
        break;
    case 6:
        SetDiagFilter(eDiagFilter_All, "!foo");
        m_Case = "filter \"!foo\"";
        x_SetExpects("111111110");
        break;
    case 7:
        SetDiagFilter(eDiagFilter_All, "/corelib");
        m_Case = "filter \"/corelib\"";
        x_SetExpects("111111111");
        break;
    case 8:
        SetDiagFilter(eDiagFilter_All, "/corelib/");
        m_Case = "filter \"/corelib/\"";
        x_SetExpects("000000000");
        break;
    }
}


void CTestDiagApp::x_SetExpects(const string& expects )
{
    for (int i = 0;  i < NUM_TESTS;  ++i) {
        m_Expected[i] = (expects[i] == '1');
        if (m_Expected[i]) {
            for (int num = 0;  num < 3;  ++num) 
                for (unsigned int thr = 0;  thr < s_NumThreads;  ++thr)
                    m_CaseExpected.insert(
                        TThreads::value_type(TCase(i, num), thr) );
        }
    }
} 


bool CTestDiagApp::Thread_Init(int /* idx */)
{
    return true;
}


string CTestDiagApp::x_MakeMessage(const string& msg, 
                                   int           idx, 
                                   const TCase&  ncase)
{
    return string("!=") + NStr::IntToString(ncase.first) + " " 
        + NStr::IntToString(ncase.second) + " " 
        + NStr::IntToString(idx) + " "
        + msg + "=!";
}


void CTestDiagApp::x_PrintMessages(int         test_number,
                                   int         idx, 
                                   const char* module, 
                                   const char* nclass, 
                                   const char* function)
{
    string location  = string(module) + "::" + nclass + "::" +
        function + "()";
    string postmsg   = location + " in ERR_POST";
    string exceptmsg = location + " in the one level exception"; 
    string secondmsg = location + " in the two levels exception";

    if (idx == 0) {
        m_Messages[ TCase( test_number, 0 ) ] = postmsg;
        m_Messages[ TCase( test_number, 1 ) ] = exceptmsg;
        m_Messages[ TCase( test_number, 2 ) ] = secondmsg;
    }

    // ERR_POST
    ERR_POST(x_MakeMessage(postmsg, idx, TCase(test_number,0))
             << MDiagModule(module) 
             << MDiagClass(nclass) 
             << MDiagFunction(function));
    
    // one level exception
    try {
        throw NCBI_EXCEPTION(CException, eUnknown, "exception")
            .SetModule(module).SetClass(nclass).SetFunction(function);
    }
    catch (const  CException& ex) {
        NCBI_REPORT_EXCEPTION(x_MakeMessage(exceptmsg, 
                                            idx, 
                                            TCase(test_number, 1)), ex);
    }

    // two level exceptions
    try {
        try {
            throw NCBI_EXCEPTION(CException, eUnknown, "exception1")
                .SetModule(module).SetClass(nclass).SetFunction(function);
        }
        catch (const  CException& ex) {
            throw NCBI_EXCEPTION_EX(ex, CException, eUnknown, "exception2")
                .SetModule("foo").SetClass("foo").SetFunction("foo");

        }
    }
    catch (const  CException& ex) {
        NCBI_REPORT_EXCEPTION(x_MakeMessage(secondmsg, 
                                            idx, 
                                            TCase(test_number, 2)), ex);
    }
}


bool CTestDiagApp::Thread_Run(int idx)
{
    int testnum = 0;

    x_PrintMessages(testnum++, idx, "module", "class", "function");
    x_PrintMessages(testnum++, idx, "module", "class", ""        );
    x_PrintMessages(testnum++, idx, "module", "",      "function");
    x_PrintMessages(testnum++, idx, "module", "",      ""        );
    x_PrintMessages(testnum++, idx, "",       "class", "function");
    x_PrintMessages(testnum++, idx, "",       "class", ""        );
    x_PrintMessages(testnum++, idx, "",       "",      "function");
    x_PrintMessages(testnum++, idx, "",       "",      ""        );
    x_PrintMessages(testnum++, idx, "foo",    "foo",   "foo"     );

    return true;
}


bool CTestDiagApp::TestApp_Args(CArgDescriptions& args)
{
    args.AddPositional("case_number",
                       "case number",
                       CArgDescriptions::eInteger);

    args.SetConstraint("case_number",
                       new CArgAllow_Integers(1, 8));

    return true;
}


bool CTestDiagApp::TestApp_Init(void)
{
    // Get arguments
    const CArgs& args = GetArgs();
    SetCase( args["case_number"].AsInteger() );

    NcbiCout << NcbiEndl
             << "Testing NCBIDIAG FILTERING case ("
             << m_Case << ") with "
             << NStr::IntToString(s_NumThreads)
             << " threads..."
             << NcbiEndl;

    SetDiagPostFlag(eDPF_Location);
    SetDiagPostFlag(eDPF_MergeLines);
    // Output to the string stream -- to verify the result
    SetDiagStream(&s_Sout);

    return true;
}


bool CTestDiagApp::TestApp_Exit(void)
{
    // Cleanup
    SetDiagStream(0);

    // Verify the result
    typedef list<string> TStrings;
    TStrings messages;

    // Get the list of messages
    NStr::Split(CNcbiOstrstreamToString(s_Sout), "\r\n", messages);

    bool result = true;
    ITERATE(TStrings, i, messages) {
        size_t beg = i->find("!=");
        size_t end = i->find("=!");
        if (beg == string::npos  ||  end == string::npos)
            continue;

        string          full_msg = i->substr(beg + 2, end - beg - 2);
        CNcbiIstrstream in( full_msg.c_str() );
        TCase           ncase;
        int             thread;
        string          msg;
        
        in >> ncase.first >> ncase.second >> thread;
        getline(in, msg);

        if ( !m_CaseExpected.erase(make_pair(ncase, thread)) ) {
            NcbiCout << "Error: unexpected message \"" 
                     << msg << "\" for thread " << thread << NcbiEndl;
            result = false;
        }
        
    }

    ITERATE(TThreads, i, m_CaseExpected) {
        NcbiCout << "Error: message \"" << m_Messages[ i->first ]
                 << "\" for thread " << i->second << " not found"
                 << NcbiEndl;
        result = false;
    }

    if (result) {
        NcbiCout << "Test completed successfully!"
                 << NcbiEndl << NcbiEndl;
    }

    return result;
}



/////////////////////////////////////////////////////////////////////////////
//  MAIN

int main(int argc, const char* argv[]) 
{
    CTestDiagApp app;

    app.AppMain(argc, argv, 0, eDS_Default, 0);
    return 0;
}



/*
 * ===========================================================================
 * $Log$
 * Revision 1.3  2005/02/01 21:47:14  grichenk
 * Fixed warnings
 *
 * Revision 1.2  2004/09/22 17:05:03  ucko
 * Don't rely on automatic pair<> conversion.
 *
 * Revision 1.1  2004/09/21 18:15:46  kononenk
 * Added tests for "Diagnostic Message Filtering"
 *
 * ===========================================================================
 */
