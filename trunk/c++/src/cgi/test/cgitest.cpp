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
 * Author:  Denis Vakatov, Eugene Vasilchenko, Vsevolod Sandomirsky
 *
 * File Description:
 *   TEST for:  NCBI C++ core CGI API
 *   Note:  this is a CGI part of former "coretest.cpp" test
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbireg.hpp>
#include <cgi/ncbires.hpp>
#include <cgi/ncbicgir.hpp>
#include <cgi/cgi_util.hpp>
#include <cgi/cgi_exception.hpp>

#include <algorithm>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>

#include <common/test_assert.h>  /* This header must go last */


// This is to use the ANSI C++ standard templates without the "std::" prefix
// and to use NCBI C++ entities without the "ncbi::" prefix
USING_NCBI_SCOPE;


/////////////////////////////////
// CGI
//

static void TestCgi_Cookies(void)
{
    CCgiCookies cookies("coo1=kie1BAD1;coo2=kie2_ValidPath; ");
    cookies.Add("  coo1=kie1BAD2;CooT=KieT_ExpTime  ");

    string str =
        "eee; BAD COOKIE;  Coo11=Kie11_OK; Coo3=Kie2 SEMI-BAD;"
        "B COO= 7; X C =3; uuu; coo1=Kie1_OK; Coo6=kie6; iii";
    cookies.Add(str);
    cookies.Add("RemoveThisCookie", "BAD");
    cookies.Add(str);

    assert(  cookies.Find("eee") );
    assert( !cookies.Find("BAD COOKIE") );
    assert(  cookies.Find("Coo11") );
    assert(  cookies.Find("Coo2") );
    assert( cookies.Find("Coo3") );
    assert( !cookies.Find("B COO") );
    assert( !cookies.Find("X C") );
    assert( !cookies.Find("X C ") );
    assert(  cookies.Find("uuu") );
    assert(  cookies.Find("coo1") );
    assert(  cookies.Find("CooT") );
    assert(  cookies.Find("Coo6") );
    assert( !cookies.Find("Coo2", "qq.rr.oo", NcbiEmptyString) );
    assert( !cookies.Find("Coo2", "qq.rr.oo", NcbiEmptyString) );
    assert(cookies.Find("Coo2") == cookies.Find("Coo2", "", ""));
    cookies.Find("Coo2")->SetValue("Kie2_OK");

    CCgiCookie c0("Coo5", "Kie5BAD");
    CCgiCookie c1("Coo5", "Kie", "aaa.bbb.ccc", "/");
    CCgiCookie c2(c1);
    c2.SetValue("Kie5_Dom_Sec");
    c2.SetPath("");
    c2.SetSecure(true);

    cookies.Add(c1);
    cookies.Add(c2);

    CCgiCookie* c3 = cookies.Find("coo2", NcbiEmptyString, "");
    c3->SetPath("coo2_ValidPath");

    assert( !cookies.Remove(cookies.Find("NoSuchCookie")) );
    assert( cookies.Remove(cookies.Find("RemoveThisCookie")) );
    assert( !cookies.Remove(cookies.Find("RemoveThisCookie")) );
    assert( !cookies.Find("RemoveThisCookie") );

    assert( cookies.Find("CoO5") );
    assert( cookies.Find("cOo5") == cookies.Find("Coo5", "aaa.bBb.ccC", "") );
    assert( cookies.Find("Coo5")->GetDomain() == "aaa.bbb.ccc" );
    assert( cookies.Find("coo2")->GetDomain().empty() );
    assert( cookies.Find("cOO5")->GetSecure() );
    assert( !cookies.Find("cOo2")->GetSecure() );

    time_t timer = time(0);
    tm *date = gmtime(&timer);
    CCgiCookie *ct = cookies.Find("CooT");
    ct->SetExpDate(*date);

    cookies.Add("AAA", "11", "qq.yy.dd");
    cookies.Add("AAA", "12", "QQ.yy.Dd");
    assert( cookies.Find("AAA", "qq.yy.dd", NcbiEmptyString)->GetValue()
            == "12" );

    cookies.Add("aaa", "1", "QQ.yy.Dd");
    assert( cookies.Find("AAA", "qq.yy.dd", NcbiEmptyString)->GetValue()
            == "1" );

    cookies.Add("aaa", "19");

    cookies.Add("aaa", "21", "QQ.yy.Dd", "path");
    assert( cookies.Find("AAA", "qq.yy.dd", "path")->GetValue() == "21");

    cookies.Add("aaa", "22", "QQ.yy.Dd", "Path");
    assert( cookies.Find("AAA", "qq.yy.dd", "path")->GetValue() == "21" );
    assert( cookies.Find("AAA")->GetValue() == "19" );

    cookies.Add("AAA", "2", "QQ.yy.Dd", "path");
    assert( cookies.Find("AAA", "qq.yy.dd", "path")->GetValue() == "2" );

    cookies.Add("AAA", "3");

    cookies.Add("BBA", "BBA1");
    cookies.Add("BBA", "BBA2", "", "path");

    cookies.Add("BBB", "B1", "oo.pp.yy");
    cookies.Add("BBB", "B2");
    cookies.Add("BBB", "B3", "bb.pp.yy", "path3");
    cookies.Add("BBB", "B3", "cc.pp.yy", "path3");

    CCgiCookies::TRange range;
    assert( cookies.Find("BBA", &range)->GetValue() == "BBA1" );
    assert( cookies.Remove(range) );

    cookies.Add("BBB", "B3", "dd.pp.yy", "path3");
    cookies.Add("BBB", "B3", "aa.pp.yy", "path3");
    cookies.Add("BBB", "B3", "",         "path3");

    cookies.Add("BBC", "BBC1", "", "path");
    cookies.Add("BBC", "BBC2", "44.88.99", "path");

    cookies.Add("BBD", "BBD1",  "", "path");
    cookies.Add("BBD", "BBD20", "44.88.99", "path");
    cookies.Add("BBD", "BBD2",  "44.88.99", "path");
    cookies.Add("BBD", "BBD3",  "77.99.00", "path");

    assert( cookies.Remove( cookies.Find("BBB", "dd.pp.yy", "path3") ) );

    cookies.Add("DDD", "DDD1", "aa.bb.cc", "p1/p2");
    cookies.Add("DDD", "DDD2", "aa.bb.cc");
    cookies.Add("DDD", "DDD3", "aa.bb.cc", "p3/p4");
    cookies.Add("DDD", "DDD4", "aa.bb.cc", "p1");
    cookies.Add("DDD", "DDD5", "aa.bb.cc", "p1/p2/p3");
    cookies.Add("DDD", "DDD6", "aa.bb.cc", "p1/p4");
    assert( cookies.Find("DDD", &range)->GetValue() == "DDD2" );

    assert( cookies.Find("BBD", "44.88.99", "path")->GetValue() == "BBD2" );
    assert( cookies.Remove(cookies.Find("BBD", "77.99.00", "path")) );
    assert( cookies.Remove("BBD") == 2 ); 

    string enc_name ="name%20%21%22%23";
    string enc_val = "val%25%26%27";
    cookies.Add(enc_name + "=" + enc_val);
    assert( cookies.Find(NStr::URLDecode(enc_name))->GetValue() ==
        NStr::URLDecode(enc_val));
    enc_name ="name with bad chars;";
    enc_val = "value with bad chars;";
    cookies.Add(enc_name, enc_val);
    assert( cookies.Find(NStr::URLDecode(enc_name))->GetValue() ==
        NStr::URLDecode(enc_val));

    NcbiCerr << "\n\nCookies:\n\n" << cookies << NcbiEndl;

    cookies.Add("bad name=good_value; good_name=bad value; bad both=bad both",
        CCgiCookies::eOnBadCookie_StoreAndError);
    CCgiCookies::TCRange rg = cookies.GetAll();
    for (CCgiCookies::TCIter it = rg.first; it != rg.second; it++) {
        if ( !(*it)->IsInvalid() ) {
            _ASSERT(cookies.Find((*it)->GetName()));
        }
    }
}


static void PrintEntries(TCgiEntries& entries)
{
    for (TCgiEntries::iterator iter = entries.begin();
         iter != entries.end();  ++iter) {
        // assert( !NStr::StartsWith(iter->first, "amp;", NStr::eNocase) );
        NcbiCout << "  (\"" << iter->first << "\", \""
                 << iter->second << "\")" << NcbiEndl;
    }
}

static bool TestEntries(TCgiEntries& entries, const string& str)
{
    NcbiCout << "\n Entries: `" << str << "'\n";
    SIZE_TYPE err_pos = CCgiRequest::ParseEntries(str, entries);
    PrintEntries(entries);

    if ( err_pos ) {
        NcbiCout << "-- Error at position #" << err_pos << NcbiEndl;
        return false;
    }
    return true;
}

static void PrintIndexes(TCgiIndexes& indexes)
{
    for (TCgiIndexes::iterator iter = indexes.begin();
         iter != indexes.end();  ++iter) {
        NcbiCout << "  \"" << *iter << "\"    ";
    }
    NcbiCout << NcbiEndl;
}

static bool TestIndexes(TCgiIndexes& indexes, const string& str)
{
    NcbiCout << "\n Indexes: `" << str << "'\n";
    SIZE_TYPE err_pos = CCgiRequest::ParseIndexes(str, indexes);
    PrintIndexes(indexes);

    if ( err_pos ) {
        NcbiCout << "-- Error at position #" << err_pos << NcbiEndl;
        return false;
    }
    return true;
}

static void TestCgi_Request_Static(void)
{
    // Test CCgiRequest::ParseEntries()
    TCgiEntries entries;
    assert(  TestEntries(entries, "aa=bb&cc=dd") );
    assert(  TestEntries(entries, "e%20e=f%26f&g%2Ag=h+h%2e") );
    entries.clear();
    assert( !TestEntries(entries, " xx=yy") );
    assert(  TestEntries(entries, "xx=&yy=zz") );
    assert(  TestEntries(entries, "rr=") );
    entries.clear();

    assert(  TestEntries(entries, "&&") );
    assert(  TestEntries(entries, "aa") );
    assert(  TestEntries(entries, "bb&") );
    assert(  TestEntries(entries, "&cc") );
    assert(  TestEntries(entries, "&dd&") );
    assert(  TestEntries(entries, "ee&&") );
    assert(  TestEntries(entries, "&&ff") );
    assert(  TestEntries(entries, "&&gg&&") );
    entries.clear();

    assert(  TestEntries(entries, "aa=") );
    assert(  TestEntries(entries, "bb=&") );
    assert(  TestEntries(entries, "&cc=") );
    assert(  TestEntries(entries, "dd=&&") );
    assert(  TestEntries(entries, "&&ee=") );
    entries.clear();

    assert(  TestEntries(entries, "&aa=uu") );
    assert(  TestEntries(entries, "bb=vv&") );
    assert(  TestEntries(entries, "&cc=ww&") );
    assert(  TestEntries(entries, "&&dd=xx") );
    assert(  TestEntries(entries, "ee=yy&&") );
    assert(  TestEntries(entries, "&&ff=zz&&") );
    entries.clear();

    assert(  TestEntries(entries, "aa=vv&&bb=ww") );
    assert(  TestEntries(entries, "cc=&&dd=xx") );
    assert(  TestEntries(entries, "ee=yy&&ff") );
    assert(  TestEntries(entries, "gg&&hh=zz") );
    entries.clear();

    // assert( !TestEntries(entries, "tt=qq=pp") );
    assert( !TestEntries(entries, "=ggg&ppp=PPP") );
    assert(  TestEntries(entries, "a=d&eee") );
    assert(  TestEntries(entries, "xxx&eee") );
    assert(  TestEntries(entries, "xxx+eee") );
    assert(  TestEntries(entries, "UUU") );
    assert(  TestEntries(entries, "a%21%2f%25aa=%2Fd%2c&eee=%3f") );
    entries.clear();

    // some older browsers fail to parse &amp; in HREFs; ensure that
    // we handle it properly.
    assert(  TestEntries(entries, "a=b&amp;c=d&amp;e=f") );
    assert(  TestEntries(entries, "&amp;c=d&amp;e=f") );
    assert(  TestEntries(entries, "&amp;&amp;c=d&amp;&amp;e=f&amp;&amp;") );
    assert(  TestEntries(entries, "amp;c=d&amp;amp;e=f") );
    entries.clear();

    // Test CCgiRequest::ParseIndexes()
    TCgiIndexes indexes;
    assert(  TestIndexes(indexes, "a+bb+ccc+d") );
    assert(  TestIndexes(indexes, "e%20e+f%26f+g%2Ag+hh%2e") );
    indexes.clear();
    assert( !TestIndexes(indexes, " jhg") );
    // assert( !TestIndexes(indexes, "e%h%2e+3") );
    assert(  TestIndexes(indexes, "aa+%20+bb") );
    assert( !TestIndexes(indexes, "aa++bb") );
    indexes.clear();
    assert( !TestIndexes(indexes, "+1") );
    assert( !TestIndexes(indexes, "aa+") );
    assert( !TestIndexes(indexes, "aa+bb  ") );
    assert( !TestIndexes(indexes, "c++b") );
    assert( !TestIndexes(indexes, "ff++ ") );
    assert( !TestIndexes(indexes, "++") );
}

static void TestCgi_Request_Full(CNcbiIstream*         istr,
                                 const CNcbiArguments* args = 0,
                                 CCgiRequest::TFlags   flags = 0)
{
    CCgiRequest CCR(args, 0, istr, flags);

    NcbiCout << "\n\nCCgiRequest::\n";

    try {
        NcbiCout << "GetContentLength(): "
                 << CCR.GetContentLength() << NcbiEndl;
    }
    STD_CATCH ("TestCgi_Request_Full");

    NcbiCout << "GetRandomProperty(\"USER_AGENT\"): "
             << CCR.GetRandomProperty("USER_AGENT") << NcbiEndl;
    NcbiCout << "GetRandomProperty(\"MY_RANDOM_PROP\"): "
             << CCR.GetRandomProperty("MY_RANDOM_PROP") << NcbiEndl;

    NcbiCout << "GetRandomProperty(\"HTTP_MY_RANDOM_PROP\"): "
             << CCR.GetRandomProperty("HTTP_MY_RANDOM_PROP")
             << NcbiEndl;
    NcbiCout << "GetRandomProperty(\"HTTP_MY_RANDOM_PROP\", false): "
             << CCR.GetRandomProperty("HTTP_MY_RANDOM_PROP", false)
             << NcbiEndl;

    NcbiCout << "\nCCgiRequest::  All properties:\n";
    for (size_t prop = 0;  prop < (size_t)eCgi_NProperties;  prop++) {
        NcbiCout << NcbiSetw(24)
                 << CCgiRequest::GetPropertyName((ECgiProp)prop) << " = \""
                 << CCR.GetProperty((ECgiProp)prop) << "\"\n";
    }

    CCgiCookies cookies;
    {{  // Just an example of copying the cookies from a request data
        // Of course, we could use the original request's cookie set
        // ("x_cookies") if we performed only "const" operations on it
        const CCgiCookies& x_cookies = CCR.GetCookies();
        cookies.Add(x_cookies);
    }}
    NcbiCout << "\nCCgiRequest::  All cookies:\n";
    if ( cookies.Empty() )
        NcbiCout << "No cookies specified" << NcbiEndl;
    else
        NcbiCout << cookies << NcbiEndl;

    TCgiEntries entries = CCR.GetEntries();
    NcbiCout << "\nCCgiRequest::  All entries:\n";
    if ( entries.empty() ) {
        NcbiCout << "No entries specified" << NcbiEndl;
    } else {
        PrintEntries(entries);

        if ( !CCR.GetEntry("get_query1").empty() ) {
            NcbiCout << "GetEntry() check." << NcbiEndl;

            assert(CCR.GetEntry("get_query1") == "gq1");
            bool is_found = false;
            assert(CCR.GetEntry("get_query1", &is_found) == "gq1");
            assert(is_found);

            assert(CCR.GetEntry("get_query2", 0).empty());
            is_found = false;
            assert(CCR.GetEntry("get_query2", &is_found).empty());
            assert(is_found);

            assert(CCR.GetEntry("qwe1rtyuioop", &is_found).empty());
            assert(!is_found);
        }
    }

    TCgiIndexes indexes = CCR.GetIndexes();
    NcbiCout << "\nCCgiRequest::  ISINDEX values:\n";
    if ( indexes.empty() ) {
        NcbiCout << "No ISINDEX values specified" << NcbiEndl;
    } else {
        PrintIndexes(indexes);
    }

    CNcbiIstream* is = CCR.GetInputStream();
    if ( is ) {
        NcbiCout << "\nUn-parsed content body:\n";
        NcbiCout << is->rdbuf() << NcbiEndl << NcbiEndl;
    }
}

static void TestCgiMisc(void)
{
    const string str("_ _%_;_\n_:_'_*_\\_\"_");
    {{
        string url = "qwerty";
        url = NStr::URLEncode(str);
        NcbiCout << str << NcbiEndl << url << NcbiEndl;
        assert( url.compare("_+_%25_%3B_%0A_%3A_'_*_%5C_%22_") == 0 );

        string str1 = NStr::URLDecode(url);
        assert( str1 == str );

        string url1 = NStr::URLEncode(str1);
        assert( url1 == url );
    }}
    {{
        string url = "qwerty";
        url = NStr::URLEncode(str, NStr::eUrlEnc_ProcessMarkChars);
        NcbiCout << str << NcbiEndl << url << NcbiEndl;
        assert( url.compare("%5F+%5F%25%5F%3B%5F%0A%5F%3A%5F%27%5F%2A%5F%5C%5F%22%5F") == 0 );

        string str1 = NStr::URLDecode(url);
        assert( str1 == str );

        string url1 = NStr::URLEncode(str1, NStr::eUrlEnc_ProcessMarkChars);
        assert( url1 == url );
    }}

    const string bad_url("%ax");
    try {
        NStr::URLDecode(bad_url);
    } STD_CATCH("%ax");
}


static void TestCgi(const CNcbiArguments& args)
{
    // this is to get rid of warnings on some strict compilers (like SUN Forte)
#define X_PUTENV(s)  ::putenv((char*) s)

    TestCgi_Cookies();
    TestCgi_Request_Static();

    assert( !X_PUTENV("CONTENT_TYPE=application/x-www-form-urlencoded") );

    try { // POST only
        char inp_str[] = "post11=val11&post12void=&post13=val13";
        CNcbiIstrstream istr(inp_str);
        char len[32];
        assert(::sprintf(len, "CONTENT_LENGTH=%ld", (long) ::strlen(inp_str)));
        assert( !::putenv(len) );

        assert( !X_PUTENV("SERVER_PORT=") );
        assert( !X_PUTENV("REMOTE_ADDRESS=") );
        assert( !X_PUTENV("REQUEST_METHOD=POST") );
        assert( !X_PUTENV("QUERY_STRING=") );
        assert( !X_PUTENV("HTTP_COOKIE=") );
        TestCgi_Request_Full(&istr);
    } STD_CATCH("TestCgi(POST only)");

    try { // POST, fDoNotParseContent
        char inp_str[] = "post11=val11&post12void=&post13=val13";
        CNcbiIstrstream istr(inp_str);
        char len[32];
        assert(::sprintf(len, "CONTENT_LENGTH=%ld", (long) ::strlen(inp_str)));
        assert( !::putenv(len) );

        assert( !X_PUTENV("SERVER_PORT=") );
        assert( !X_PUTENV("REMOTE_ADDRESS=") );
        assert( !X_PUTENV("REQUEST_METHOD=POST") );
        assert( !X_PUTENV("QUERY_STRING=") );
        assert( !X_PUTENV("HTTP_COOKIE=") );
        TestCgi_Request_Full(&istr, 0, CCgiRequest::fDoNotParseContent);
    } STD_CATCH("TestCgi(POST only)");

    try { // POST + aux. functions
        char inp_str[] = "post22void=&post23void=";
        CNcbiIstrstream istr(inp_str);
        char len[32];
        assert(::sprintf(len, "CONTENT_LENGTH=%ld", (long) ::strlen(inp_str)));
        assert( !::putenv(len) );

        assert( !X_PUTENV("SERVER_PORT=9999") );
        assert( !X_PUTENV("HTTP_USER_AGENT=MyUserAgent") );
        assert( !X_PUTENV("HTTP_MY_RANDOM_PROP=MyRandomPropValue") );
        assert( !X_PUTENV("REMOTE_ADDRESS=130.14.25.129") );
        TestCgi_Request_Full(&istr);
    } STD_CATCH("TestCgi(POST + aux. functions)");

    // this is for all following tests...
    char inp_str[] = "postXXX=valXXX";
    char len[32];
    assert( ::sprintf(len, "CONTENT_LENGTH=%ld", (long) ::strlen(inp_str)) );
    assert( !::putenv(len) );

    try { // POST + ISINDEX(action)
        CNcbiIstrstream istr(inp_str);
        assert( !X_PUTENV("QUERY_STRING=isidx1+isidx2+isidx3") );
        TestCgi_Request_Full(&istr);
    } STD_CATCH("TestCgi(POST + ISINDEX(action))");

    try { // POST + QUERY(action)
        CNcbiIstrstream istr(inp_str);
        assert( !X_PUTENV("QUERY_STRING=query1=vv1&query2=") );
        TestCgi_Request_Full(&istr);
    } STD_CATCH("TestCgi(POST + QUERY(action))");

    try { // GET ISINDEX + COOKIES
        CNcbiIstrstream istr(inp_str);
        assert( !X_PUTENV("QUERY_STRING=get_isidx1+get_isidx2+get_isidx3") );
        assert( !X_PUTENV("HTTP_COOKIE=cook1=val1; cook2=val2;") );
        TestCgi_Request_Full(&istr, 0, CCgiRequest::fIndexesNotEntries);
    } STD_CATCH("TestCgi(GET ISINDEX + COOKIES)");

    try { // GET REGULAR, NO '='
        CNcbiIstrstream istr(inp_str);
        assert( !X_PUTENV("QUERY_STRING=get_query1_empty&get_query2_empty") );
        TestCgi_Request_Full(&istr);
    } STD_CATCH("TestCgi(GET REGULAR, NO '=' )");

    try { // GET REGULAR + COOKIES
        CNcbiIstrstream istr(inp_str);
        assert( !X_PUTENV("QUERY_STRING=get_query1=gq1&get_query2=") );
        assert( !X_PUTENV("HTTP_COOKIE=_cook1=_val1;_cook2=_val2") );
        TestCgi_Request_Full(&istr);
    } STD_CATCH("TestCgi(GET REGULAR + COOKIES)");

    try { // ERRONEOUS STDIN
        CNcbiIstrstream istr("123");
        assert( !X_PUTENV("QUERY_STRING=get_query1=gq1&get_query2=") );
        assert( !X_PUTENV("HTTP_COOKIE=_cook1=_val1;_cook2=_val2") );
        TestCgi_Request_Full(&istr);
    } STD_CATCH("TestCgi(ERRONEOUS STDIN)");

    try { // USER INPUT(real STDIN)
        assert( !X_PUTENV("QUERY_STRING=u_query1=uq1") );
        assert( !X_PUTENV("HTTP_COOKIE=u_cook1=u_val1; u_cook2=u_val2") );
        assert( !X_PUTENV("REQUEST_METHOD=POST") );
        NcbiCout << "Enter the length of CGI posted data now: " << NcbiFlush;
        long l = 0;
        if (!(NcbiCin >> l)  ||  len < 0) {
            NcbiCin.clear();
            runtime_error("Invalid length of CGI posted data");
        }
        char cs[32];
        assert( ::sprintf(cs, "CONTENT_LENGTH=%ld", (long) l) );
        assert( !X_PUTENV(cs) );
        NcbiCout << "Enter the CGI posted data now(no spaces): " << NcbiFlush;
        NcbiCin >> NcbiWs;
        TestCgi_Request_Full(0);
        NcbiCin.clear();
    } STD_CATCH("TestCgi(USER STDIN)");

    try { // CMD.-LINE ARGS
        assert( !X_PUTENV("REQUEST_METHOD=") );
        assert( !X_PUTENV("QUERY_STRING=MUST NOT BE USED HERE!!!") );
        TestCgi_Request_Full(&NcbiCin/* dummy */, &args);
    } STD_CATCH("TestCgi(CMD.-LINE ARGS)");

    TestCgiMisc();
#undef X_PUTENV
}


static void TestCgiResponse(const CNcbiArguments& args)
{
    NcbiCout << "Starting CCgiResponse test" << NcbiEndl;

    CCgiResponse response1;
    // Optional (since it's the default)
    response1.SetOutput(&NcbiCout);

    if (args.Size() > 2) {
        CCgiCookies cookies(args[2]);
        response1.Cookies().Add(cookies);
    }
    response1.Cookies().Remove(response1.Cookies().Find("to-Remove"));
    NcbiCout << "Cookies: " << response1.Cookies() << NcbiEndl;

    CCgiResponse response2;

    NcbiCout << "Generated simple HTTP response:" << NcbiEndl;
    response2.WriteHeader() << "Data1" << NcbiEndl << NcbiFlush;
    response2.out() << "Data2" << NcbiEndl << NcbiFlush;
    NcbiCout << "End of simple HTTP response" << NcbiEndl << NcbiEndl;

    CCgiResponse response3;

    response3.SetHeaderValue("Some-Header-Name", "Some Header Value");
    response3.SetHeaderValue("status", "399 Something is BAAAAD!!!!!");
    response3.SetStatus(301, "Moved");

    NcbiCout << "Generated HTTP response:" << NcbiEndl;
    response3.WriteHeader() << "Data1" << NcbiEndl << NcbiFlush;
    response3.out() << "Data2" << NcbiEndl << NcbiFlush;
    NcbiCout << "End of HTTP response" << NcbiEndl << NcbiEndl;

    CCgiResponse response4;

    response4.SetRawCgi(true);
    NcbiCout << "Generated HTTP \"raw CGI\" response:" << NcbiEndl;
    response4.WriteHeader() << "Data1" << NcbiEndl << NcbiFlush;
    response4.out() << "Data2" << NcbiEndl << NcbiFlush;
    NcbiCout << "End of HTTP \"raw CGI\" response" << NcbiEndl << NcbiEndl;

    try {
        NcbiCout << "Checking double headed HTTP response:" << NcbiEndl;
        response4.WriteHeader() << "Data1" << NcbiEndl << NcbiFlush;
    } catch (CCgiResponseException& e) {
        _ASSERT(e.GetErrCode() == CCgiResponseException::eDoubleHeader);
        NCBI_REPORT_EXCEPTION("Caught", e);
    }
    NcbiCout << "Checking double headed HTTP response into a different stream:"
             << NcbiEndl;
    response4.WriteHeader(NcbiCerr);
}


struct TVersion {
    int major;
    int minor;
    int patch;
};

struct SUserAgent {
    const char*                   str;        // in
    CCgiUserAgent::EBrowser       browser;    // out
    TVersion                      browser_v;  // out
    CCgiUserAgent::EBrowserEngine engine;     // out
    TVersion                      engine_v;   // out
    TVersion                      mozilla_v;  // out
};

const SUserAgent s_UserAgentTests[] = {

    // VendorProduct tests

    { "SomeUnknownBrowser",
        CCgiUserAgent::eUnknown,        {-1, -1, -1},
        CCgiUserAgent::eEngine_Unknown, {-1, -1, -1},
        {-1, -1, -1}
    },
    { "SomeUnknownBrowser/1.0",
        CCgiUserAgent::eUnknown,        {-1, -1, -1},
        CCgiUserAgent::eEngine_Unknown, {-1, -1, -1},
        {-1, -1, -1}
    },
    { "Mozilla/5.0 (Windows) Firefox",
        CCgiUserAgent::eFirefox,        {-1, -1, -1},
        CCgiUserAgent::eEngine_Gecko,   {-1, -1, -1},
        { 5, 0, -1}
    },
    { "Mozilla/5.0 (Windows) Firefox;",
        CCgiUserAgent::eFirefox,        {-1, -1, -1},
        CCgiUserAgent::eEngine_Gecko,   {-1, -1, -1},
        { 5, 0, -1}
    },
    { "Mozilla/5.0 (Windows) Firefox/1",
        CCgiUserAgent::eFirefox,        { 1, -1, -1},
        CCgiUserAgent::eEngine_Gecko,   {-1, -1, -1},
        { 5, 0, -1}
    },
    { "Mozilla/4.0 (BeOS R4.5;US) Opera 3.62  [en]",
        CCgiUserAgent::eOpera,          { 3, 62, -1},
        CCgiUserAgent::eEngine_Unknown, {-1, -1, -1},
        { 4, 0, -1}
    },
    { "Mozilla/5.0 (Windows; U; Windows NT 5.1; nl-NL; rv:1.7.5) Gecko/20041202 Firefox/1.0",
        CCgiUserAgent::eFirefox,        { 1,  0, -1},
        CCgiUserAgent::eEngine_Gecko,   { 1,  7,  5},
        { 5, 0, -1}
    },
    { "Mozilla/5.0 (Windows; U; Windows NT 5.1; en-US; rv:1.9.0.2) Gecko/2008091620 Firefox/3.0.2 (.NET CLR 3.5.30729)",
        CCgiUserAgent::eFirefox,        {  3,  0,  2},
        CCgiUserAgent::eEngine_Gecko,   {  1,  9,  0},
        { 5, 0, -1}
    },
    { "Mozilla/5.0 (Windows; U; Windows NT 5.1; en-US; rv:1.7.5) Gecko/20041107 Googlebot/2.1",
        CCgiUserAgent::eCrawler,        { 2,  1, -1},
        CCgiUserAgent::eEngine_Bot,     {-1, -1, -1},
        { 5,  0, -1}
    },
    { "Mozilla/5.0 (Windows; U; Windows NT 5.1; en-US; rv:1.8.1b2) Gecko/20060821 SeaMonkey/1.1a",
        CCgiUserAgent::eSeaMonkey,      {  1,  1, -1},
        CCgiUserAgent::eEngine_Gecko,   {  1,  8,  1},
        { 5, 0, -1}
    },
    { "Mozilla/5.0 (Macintosh; U; PPC Mac OS X; it-it) AppleWebKit/412 (KHTML, like Gecko) Safari/412",
        CCgiUserAgent::eSafari,         {412, -1, -1},
        CCgiUserAgent::eEngine_KHTML,   {412, -1, -1},
        { 5, 0, -1}
    },
    { "Mozilla/5.0 (Macintosh; U; PPC Mac OS X; en-US) AppleWebKit/125.4 (KHTML, like Gecko, Safari) OmniWeb/v563.51",
        CCgiUserAgent::eOmniWeb,        {563, 51, -1},
        CCgiUserAgent::eEngine_KHTML,   {125,  4, -1},
        { 5, 0, -1}
    },
    { "Mozilla/5.0 (Windows; U; Windows NT 5.1; en-US) AppleWebKit/525.13 (KHTML, like Gecko) Chrome/0.2.149.27 Safari/525.13",
        CCgiUserAgent::eChrome,         {  0,  2,149},
        CCgiUserAgent::eEngine_KHTML,   {525, 13, -1},
        { 5, 0, -1}
    },

    // AppComment tests

    { "Mozilla/5.0 (compatible; iCab 3.0.1; Macintosh; U; PPC Mac OS X)",
        CCgiUserAgent::eiCab,           { 3,  0,  1},
        CCgiUserAgent::eEngine_Unknown, {-1, -1, -1},
        { 5, 0, -1}
    },
    { "Mozilla/5.0 (compatible; iCab 3.0.1)",
        CCgiUserAgent::eiCab,           { 3,  0,  1},
        CCgiUserAgent::eEngine_Unknown, {-1, -1, -1},
        { 5, 0, -1}
    },
    { "Mozilla/5.0 (compatible; Konqueror/3.1-rc3; i686 Linux; 20020515)",
        CCgiUserAgent::eKonqueror,      { 3,  1, -1},
        CCgiUserAgent::eEngine_Unknown, {-1, -1, -1},
        { 5, 0, -1}
    },
    { "Mozilla/4.0 (compatible; MSIE 6.0; MSN 2.5; Windows 98)",
        CCgiUserAgent::eIE,             { 6,  0, -1},
        CCgiUserAgent::eEngine_IE,      { 6,  0, -1},
        { 4, 0, -1}
    },
    { "Mozilla/4.0 (compatible; MSIE 7.0b; Windows NT 6.0)",
        CCgiUserAgent::eIE,             { 7,  0, -1},
        CCgiUserAgent::eEngine_IE,      { 7,  0, -1},
        { 4, 0, -1}
    },
    { "Mozilla/4.0 (compatible; MSIE 6.0(en); Windows NT 5.1; Avant Browser [avantbrowser.com]; iOpus-I-M; QXW03416; .NET CLR 1.1.4322)",
        CCgiUserAgent::eAvantBrowser,   {-1, -1, -1},
        CCgiUserAgent::eEngine_IE,      { 6,  0, -1},
        { 4, 0, -1}
    },

    // Mozilla compatible

    { "Mozilla/5.0 (compatible; unknown; i686 Linux; 20020515)",
        CCgiUserAgent::eMozillaCompatible, {-1, -1, -1},
        CCgiUserAgent::eEngine_Unknown,    {-1, -1, -1},
        { 5, 0, -1}
    },
    { "Mozilla/6.2 [en] (Windows NT 5.1; U)",
        CCgiUserAgent::eMozilla,        { 6,  2, -1},
        CCgiUserAgent::eEngine_Gecko,   {-1, -1, -1},
        { 6, 2, -1}
    },
    { "Mozilla/4.75 [en] (Win98; U)libwww-perl/5.41",
        CCgiUserAgent::eScript,         { 5, 41, -1},
        CCgiUserAgent::eEngine_Bot,     {-1, -1, -1},
        { 4, 75, -1}
    },

     // Genuine Netscape/Mozilla

    { "Mozilla/4.7 [en] (WinNT; U)",
        CCgiUserAgent::eNetscape,       { 4,  7, -1},
        CCgiUserAgent::eEngine_Unknown, {-1, -1, -1},
        { 4, 7, -1}
    },
    { "Mozilla/4.7C-CCK-MCD {C-UDP; EBM-APPLE} (Macintosh; I; PPC)",
        CCgiUserAgent::eNetscape,       { 4,  7, -1},
        CCgiUserAgent::eEngine_Unknown, {-1, -1, -1},
        { 4, 7, -1}
    },
    { "Mozilla/5.0 (Windows; U; Windows NT 5.0; en-US; rv:1.0.1) Gecko/20020823 Netscape6/6.2.3",
        CCgiUserAgent::eNetscape,       { 6,  2,  3},
        CCgiUserAgent::eEngine_Gecko,   { 1,  0,  1},
        { 5, 0, -1}
    },
    { "Mozilla/5.0 (X11; U; Linux i686; en-US; rv:1.4.1) Gecko/20031008",
        CCgiUserAgent::eMozilla,        { 5,  0, -1},
        CCgiUserAgent::eEngine_Gecko,   { 1,  4,  1},
        { 5, 0, -1}
    },

     // AppProduct token tests

    { "Microsoft Internet Explorer/4.0b1 (Windows 95)",
        CCgiUserAgent::eIE,             { 4,  0, -1},
        CCgiUserAgent::eEngine_IE,      { 4,  0, -1},
        {-1, -1, -1}
    },
    { "Lynx/2.8.4rel.1 libwww-FM/2.14",
        CCgiUserAgent::eLynx,           { 2,  8,  4},
        CCgiUserAgent::eEngine_Unknown, {-1, -1, -1},
        {-1, -1, -1}
    },
    { "Avant Browser (http://www.avantbrowser.com)",
        CCgiUserAgent::eAvantBrowser,   {-1, -1, -1},
        CCgiUserAgent::eEngine_IE,      {-1, -1, -1},
        {-1, -1, -1}
    },
    { "Opera/3.62 (Windows NT 5.0; U)  [en] (www.proxomitron.de)",
        CCgiUserAgent::eOpera,          { 3, 62, -1},
        CCgiUserAgent::eEngine_Unknown, {-1, -1, -1},
        {-1, -1, -1}
    },
    { "check_http/1.89 (nagios-plugins 1.4.3)",
        CCgiUserAgent::eNagios,         { 1,  89, -1},
        CCgiUserAgent::eEngine_Bot,     {-1,  -1, -1},
        { -1, -1, -1}
    }
};


void s_PrintUserAgentVersion(const string& name, TUserAgentVersion& v)
{
    cout << name;
    if ( v.GetMajor() >= 0 ) {
        cout << v.GetMajor();
        if ( v.GetMinor() >= 0 ) {
            cout << "." << v.GetMinor();
            if ( v.GetPatchLevel() >= 0 ) {
                cout << "." << v.GetPatchLevel() << endl;
            }
        }
    }
    cout << endl;
}


void TestUserAgent(void)
{
    CCgiUserAgent agent;

    for (size_t i=0; i<sizeof(s_UserAgentTests)/sizeof(s_UserAgentTests[0]); i++) {
        const SUserAgent* a = &s_UserAgentTests[i];
        cout << a->str << endl; 
        agent.Reset(a->str);

        TUserAgentVersion v;

        // Browser version

        CCgiUserAgent::EBrowser b = agent.GetBrowser();
        string b_name = agent.GetBrowserName();
        cout << "Browser        : " << b_name << " (" << b << ")" << endl;
        v = agent.GetBrowserVersion();
        s_PrintUserAgentVersion("Version        : ", v);
        assert(a->browser == b);
        assert(a->browser_v.major == v.GetMajor());
        assert(a->browser_v.minor == v.GetMinor());
        assert(a->browser_v.patch == v.GetPatchLevel());

        // Engine version

        CCgiUserAgent::EBrowserEngine e = agent.GetEngine();
        v = agent.GetEngineVersion();
        cout << "Engine         : " << e << endl;
        s_PrintUserAgentVersion("Engine version : ", v);
        assert(a->engine == e);
        assert(a->engine_v.major == v.GetMajor());
        assert(a->engine_v.minor == v.GetMinor());
        assert(a->engine_v.patch == v.GetPatchLevel());

        // Mozilla-compatible version

        v = agent.GetMozillaVersion();
        s_PrintUserAgentVersion("Mozilla version: ", v);
        assert(a->mozilla_v.major == v.GetMajor());
        assert(a->mozilla_v.minor == v.GetMinor());
        assert(a->mozilla_v.patch == v.GetPatchLevel());

        cout << endl;
    }

    // IsBot() -- simple test
    {{
        agent.Reset("Mozilla/4.75 [en] (Win98; U)libwww-perl/5.41");
        assert(agent.GetBrowser() == CCgiUserAgent::eScript);
        assert(agent.GetEngine()  == CCgiUserAgent::eEngine_Bot);
        assert( agent.IsBot());
        // Treat all as bots, except scripts
        assert(!agent.IsBot(CCgiUserAgent::fBotAll & ~CCgiUserAgent::fBotScript));

        agent.Reset("Mozilla/4.75 (SomeNewBot/1.2.3)");
        assert(agent.GetBrowser() == CCgiUserAgent::eNetscape);
        assert(agent.GetEngine()  == CCgiUserAgent::eEngine_Unknown);
        assert(!agent.IsBot());
        assert( agent.IsBot(CCgiUserAgent::fBotAll, "SomeNewCrawler SomeNewBot SomeOtherBot"));
    }}
}


/////////////////////////////////
// Test CGI application
//

class CTestApplication : public CNcbiApplication
{
public:
    virtual ~CTestApplication(void);
    virtual int Run(void);
};


int CTestApplication::Run(void)
{
    TestCgi( GetArguments() );
    TestCgiResponse( GetArguments() );
    TestUserAgent();

    return 0;
}

CTestApplication::~CTestApplication()
{
    SetDiagStream(0);
}



/////////////////////////////////
// APPLICATION OBJECT
//   and
// MAIN
//

static CTestApplication theTestApplication;

int main(int argc, const char* argv[])
{
    // Execute main application function
    return theTestApplication.AppMain(argc, argv);
}
