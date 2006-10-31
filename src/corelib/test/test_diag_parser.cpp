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
 *   Test for NCBI diag parser
 *
 */

#define NCBI_MODULE TEST_DIAG_PARSER

#include <ncbi_pch.hpp>
#include <corelib/ncbidiag.hpp>
#include <corelib/ncbithr.hpp>
#include <corelib/ncbiapp.hpp>

#include <test/test_assert.h>  /* This header must go last */


USING_NCBI_SCOPE;


////////////////////////////////
// Test application
//

class CDiagParserApp : public CNcbiApplication
{
public:
    CDiagParserApp(void);
    ~CDiagParserApp(void);

    void Init(void);
    int  Run(void);
private:
    void x_CheckMessage(void);
    void x_CheckMessage(const string& message, bool valid);
};


CDiagParserApp::CDiagParserApp(void)
{
}


CDiagParserApp::~CDiagParserApp(void)
{
}


void CDiagParserApp::Init(void)
{
    GetDiagContext().SetOldPostFormat(false);
}


const int kNumFlags = 12;
static TDiagPostFlags s_TestFlags[kNumFlags] = {
    eDPF_File + eDPF_LongFilename,
    eDPF_Line,
    eDPF_Severity,
    eDPF_ErrorID,
    eDPF_DateTime,
    eDPF_Location,
    eDPF_PID,
    eDPF_TID,
    eDPF_SerialNo,
    eDPF_SerialNo_Thread,
    eDPF_RequestId,
    eDPF_UID
};

int CDiagParserApp::Run(void)
{
    // Valid messages
    x_CheckMessage("03960/000/0000 2C2D0F7851AB7E40 0005/0005 "
                "2006/09/27:13:41:56 NCBIPC1204 UNK_CLIENT "
                "UNK_SESSION cgi_sample.cgi start", true);
    x_CheckMessage("03960/000/0000 2C2D0F7851AB7E40 0010/0010 "
                "2006/09/27:13:41:56 NCBIPC1204 UNK_CLIENT "
                "2C2D0F7851AB7E40_0000SID cgi_sample.cgi "
                "request-stop 200 0.105005566", true);
    x_CheckMessage("15176/003/0006 2A763B485350C030 0098/0008 "
                "2006/10/17:12:59:47 widget3 UNK_CLIENT "
                "UNK_SESSION my_app Message[E]: TEST "
                "\"/home/user/c++/src/corelib/test/my_app.cpp\", "
                "line 81: CMyApp::Thread_Run() "
                "--- Message from thread 6", true);
    x_CheckMessage("15176/003/0006 2A763B485350C030 0098/0008 "
                "2006/10/17:12:59:47 widget3 UNK_CLIENT "
                "UNK_SESSION my_app Error: TEST(111.222) "
                "\"/home/user/c++/src/corelib/test/my_app.cpp\", "
                "line 81: CMyApp::Thread_Run() "
                "--- Message from thread 6", true);
    x_CheckMessage("15176/003/0006 2A763B485350C030 0098/0008 "
                "2006/10/17:12:59:47 widget3 UNK_CLIENT "
                "UNK_SESSION my_app Error: TEST(error.text) "
                "\"/home/user/c++/src/corelib/test/my_app.cpp\", "
                "line 81: CMyApp::Thread_Run() "
                "--- Message from thread 6", true);
    x_CheckMessage("15176/003/0006 2A763B485350C030 0098/0008 "
                "2006/10/17:12:59:47 widget3 UNK_CLIENT "
                "UNK_SESSION my_app Error: TEST(error text) "
                "\"/home/user/c++/src/corelib/test/my_app.cpp\", "
                "line 81: CMyApp::Thread_Run() "
                "--- Message from thread 6", true);
    x_CheckMessage("15176/003/0006 2A763B485350C030 0098/0008 "
                "2006/10/17:12:59:47 widget3 UNK_CLIENT "
                "UNK_SESSION my_app Error: "
                "TEST(error text (with extra '(' and ')')) "
                "\"/home/user/c++/src/corelib/test/my_app.cpp\", "
                "line 81: CMyApp::Thread_Run() "
                "--- Message from thread 6", true);
    x_CheckMessage("15176/003/0006 2A763B485350C030 0098/0008 "
                "2006/10/17:12:59:47 widget3 UNK_CLIENT "
                "UNK_SESSION my_app Message[E]: TEST "
                "\"/home/user/c++/src/corelib/test/my_app.cpp\", "
                "line 81: ::Thread_Run() "
                "--- Message from thread 6", true);
    x_CheckMessage("15176/003/0006 2A763B485350C030 0098/0008 "
                "2006/10/17:12:59:47 widget3 UNK_CLIENT "
                "UNK_SESSION my_app Message[E]: TEST "
                "\"/home/user/c++/src/corelib/test/my_app.cpp\", "
                "line 81: CMyApp:: "
                "--- Message from thread 6", true);
    x_CheckMessage("15176/003/0006 2A763B485350C030 0098/0008 "
                "2006/10/17:12:59:47 widget3 UNK_CLIENT "
                "UNK_SESSION my_app Message[E]: TEST "
                "\"/home/user/c++/src/corelib/test/my_app.cpp\", "
                "line 81: CMyApp::f() "
                "--- Message from thread 6", true);
    // Valid non-MT message
    x_CheckMessage("15176/003 2A763B485350C030 0098 "
                "2006/10/17:12:59:47 widget3 UNK_CLIENT "
                "UNK_SESSION my_app Error: TEST "
                "\"/home/user/c++/src/corelib/test/my_app.cpp\", "
                "line 81: CMyApp::Thread_Run() "
                "--- Message from thread 6", true);
    // Bad messages
    // Missing () after fuunction
    x_CheckMessage("15176/003/0006 2A763B485350C030 0098/0008 "
                "2006/10/17:12:59:47 widget3 UNK_CLIENT "
                "UNK_SESSION my_app Message[E]: TEST "
                "\"/home/user/c++/src/corelib/test/my_app.cpp\", "
                "line 81: CMyApp::Thread_Run "
                "--- Message from thread 6", false);
    // Missing function name
    x_CheckMessage("15176/003/0006 2A763B485350C030 0098/0008 "
                "2006/10/17:12:59:47 widget3 UNK_CLIENT "
                "UNK_SESSION my_app Message[E]: TEST "
                "\"/home/user/c++/src/corelib/test/my_app.cpp\", "
                "line 81: CMyApp::() "
                "--- Message from thread 6", false);
    // Missing class::function
    x_CheckMessage("15176/003/0006 2A763B485350C030 0098/0008 "
                "2006/10/17:12:59:47 widget3 UNK_CLIENT "
                "UNK_SESSION my_app Error: TEST "
                "\"/home/user/c++/src/corelib/test/my_app.cpp\", "
                "line 81: "
                "--- Message from thread 6", false);
    // Missing ) after error text
    x_CheckMessage("15176/003/0006 2A763B485350C030 0098/0008 "
                "2006/10/17:12:59:47 widget3 UNK_CLIENT "
                "UNK_SESSION my_app Error: TEST(error text "
                "\"/home/user/c++/src/corelib/test/my_app.cpp\", "
                "line 81: CMyApp::Thread_Run() "
                "--- Message from thread 6", false);
    // Missing TID/RqID
    x_CheckMessage("15176 2A763B485350C030 0098/0008 "
                "2006/10/17:12:59:47 widget3 UNK_CLIENT "
                "UNK_SESSION my_app Error: TEST "
                "\"/home/user/c++/src/corelib/test/my_app.cpp\", "
                "line 81: CMyApp::Thread_Run() "
                "--- Message from thread 6", false);
    // Missing UID
    x_CheckMessage("15176/003/0006 0098/0008 "
                "2006/10/17:12:59:47 widget3 UNK_CLIENT "
                "UNK_SESSION my_app Error: TEST "
                "\"/home/user/c++/src/corelib/test/my_app.cpp\", "
                "line 81: CMyApp::Thread_Run() "
                "--- Message from thread 6", false);
    // ??? (missing Thread_SerialNumber
    x_CheckMessage("15176/003/0006 2A763B485350C030 0098 "
                "2006/10/17:12:59:47 widget3 UNK_CLIENT "
                "UNK_SESSION my_app Error: TEST "
                "\"/home/user/c++/src/corelib/test/my_app.cpp\", "
                "line 81: CMyApp::Thread_Run() "
                "--- Message from thread 6", false);
    // Missing time
    x_CheckMessage("15176/003/0006 2A763B485350C030 0098/0008 "
                "widget3 UNK_CLIENT "
                "UNK_SESSION my_app Error: TEST "
                "\"/home/user/c++/src/corelib/test/my_app.cpp\", "
                "line 81: CMyApp::Thread_Run() "
                "--- Message from thread 6", false);
    // Missing host
    x_CheckMessage("15176/003/0006 2A763B485350C030 0098/0008 "
                "2006/10/17:12:59:47 UNK_CLIENT "
                "UNK_SESSION my_app Error: TEST "
                "\"/home/user/c++/src/corelib/test/my_app.cpp\", "
                "line 81: CMyApp::Thread_Run() "
                "--- Message from thread 6", false);
    // Missing client
    x_CheckMessage("15176/003/0006 2A763B485350C030 0098/0008 "
                "2006/10/17:12:59:47 widget3 "
                "UNK_SESSION my_app Error: TEST "
                "\"/home/user/c++/src/corelib/test/my_app.cpp\", "
                "line 81: CMyApp::Thread_Run() "
                "--- Message from thread 6", false);
    // Missing severity
    x_CheckMessage("15176/003/0006 2A763B485350C030 0098/0008 "
                "2006/10/17:12:59:47 widget3 UNK_CLIENT "
                "UNK_SESSION my_app TEST "
                "\"/home/user/c++/src/corelib/test/my_app.cpp\", "
                "line 81: CMyApp::Thread_Run() "
                "--- Message from thread 6", false);
    // Missing file, line
    x_CheckMessage("15176/003/0006 2A763B485350C030 0098/0008 "
                "2006/10/17:12:59:47 widget3 UNK_CLIENT "
                "UNK_SESSION my_app Error: TEST "
                "CMyApp::Thread_Run() "
                "--- Message from thread 6", false);
    x_CheckMessage("not a valid message", false);
    x_CheckMessage("", false);

    x_CheckMessage();

    return 0;
}


// WorkShop fails to compare string to char* or to
// initialize string with NULL pointer.
bool CmpStr(const char* val1, const string& val2)
{
    return val1 ? string(val1) == val2 : val2.empty();
}


void CDiagParserApp::x_CheckMessage(void)
{
    SetDiagPostFlag(eDPF_LongFilename);
    SetDiagRequestId(6224);

    CDiagCompileInfo info;
    string result;
    auto_ptr<SDiagMessage> msg;
    CTime post_time(CTime::eCurrent);
    char buffer[4096] = "";
    ostrstream str(buffer, 4096, ios::out);
    try {
        SetDiagStream(&str);
        info = DIAG_COMPILE_INFO;
        ERR_POST_EX(123, 45, "Test error post message");

        result = string(str.str(), str.pcount());
        size_t cr = result.find('\n');
        _ASSERT(cr != NPOS  &&  cr < result.length() - 1);
        result = result.substr(cr + 1, result.length());
        msg.reset(new SDiagMessage(result));
    }
    catch (...) {
        SetDiagStream(&NcbiCout);
        throw;
    }
    SetDiagStream(&NcbiCout);

    _ASSERT(CmpStr(msg->m_File, info.GetFile()));
    _ASSERT(msg->m_Line == size_t(info.GetLine() + 1));
    _ASSERT(msg->m_Severity == eDiag_Error);

    _ASSERT(CmpStr(msg->m_Module, info.GetModule()));
    _ASSERT(msg->m_ErrCode == 123);
    _ASSERT(msg->m_ErrSubCode == 45);

    _ASSERT(msg->GetTime() >= post_time);

    _ASSERT(CmpStr(msg->m_Class, info.GetClass()));
    _ASSERT(CmpStr(msg->m_Function, info.GetFunction()));

    _ASSERT(msg->m_PID);
    // _ASSERT(msg->m_TID);

    // We don't know the exact count after some automatic messages
    _ASSERT(msg->m_ProcPost != 0);
    _ASSERT(msg->m_ThrPost != 0);

    _ASSERT(msg->m_RequestId == 6224);

    _ASSERT(msg->GetUID() == GetDiagContext().GetUID());
}


void CDiagParserApp::x_CheckMessage(const string& message, bool valid)
{
    SDiagMessage dmsg(message);
    _ASSERT(valid  ||  !dmsg.m_Buffer);
}


/////////////////////////////////////////////////////////////////////////////
//  MAIN

int main(int argc, const char* argv[]) 
{
    // Execute main application function
    return CDiagParserApp().AppMain(argc, argv, 0, eDS_Default, 0);
}



/*
 * ===========================================================================
 * $Log$
 * Revision 1.4  2006/10/31 18:41:17  grichenk
 * Redesigned diagnostics setup.
 * Moved the setup function to ncbidiag.cpp.
 *
 * Revision 1.3  2006/07/11 16:39:50  grichenk
 * Reset DiagStream on exceptions
 *
 * Revision 1.2  2006/05/01 18:45:28  grichenk
 * Fixed warnings
 *
 * Revision 1.1  2006/04/17 15:38:26  grichenk
 * Initial revision
 *
 * Revision 1.5  2005/07/05 18:53:09  ivanov
 * Enable some tests, seems that it has been accidentally commented out.
 *
 * Revision 1.4  2005/04/14 20:27:03  ssikorsk
 * Retrieve a class name and a method/function name if NCBI_SHOW_FUNCTION_NAME is defined
 *
 * Revision 1.3  2005/03/15 15:05:34  dicuccio
 * Fixed typo: pathes -> paths
 *
 * Revision 1.2  2004/12/13 14:40:28  kuznets
 * Test for severity filtering
 *
 * Revision 1.1  2004/09/21 18:15:46  kononenk
 * Added tests for "Diagnostic Message Filtering"
 *
 * ===========================================================================
 */
