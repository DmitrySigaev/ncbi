#ifndef NCBICGI__HPP
#define NCBICGI__HPP

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
*   NCBI C++ CGI API
*
* --------------------------------------------------------------------------
* $Log$
* Revision 1.34  1999/04/14 19:52:20  vakatov
* + <time.h>
*
* Revision 1.33  1999/03/08 22:42:49  vakatov
* Added "string CCgiCookie::GetValue(void)"
*
* Revision 1.32  1999/01/14 21:25:14  vasilche
* Changed CPageList to work via form image input elements.
*
* Revision 1.31  1999/01/07 21:15:19  vakatov
* Changed prototypes for URL_DecodeString() and URL_EncodeString()
*
* Revision 1.30  1999/01/07 20:06:03  vakatov
* + URL_DecodeString()
* + URL_EncodeString()
*
* Revision 1.29  1998/12/28 17:56:25  vakatov
* New CVS and development tree structure for the NCBI C++ projects
*
* Revision 1.28  1998/12/14 20:25:35  sandomir
* changed with Command handling
*
* Revision 1.27  1998/12/09 19:25:32  vakatov
* Made CCgiRequest::GetRandomProperty() look "const"
*
* Revision 1.26  1998/12/04 23:38:34  vakatov
* Workaround SunPro's "buggy const"(see "BW_01")
* Renamed "CCgiCookies::Erase()" method to "...Clear()"
*
* Revision 1.25  1998/12/01 15:39:35  sandomir
* xmlstore.hpp|cpp moved to another dir
*
* Revision 1.24  1998/12/01 00:27:16  vakatov
* Made CCgiRequest::ParseEntries() to read ISINDEX data, too.
* Got rid of now redundant CCgiRequest::ParseIndexesAsEntries()
*
* Revision 1.23  1998/11/30 21:23:17  vakatov
* CCgiRequest:: - by default, interprete ISINDEX data as regular FORM entries
* + CCgiRequest::ParseIndexesAsEntries()
* Allow FORM entry in format "name1&name2....." (no '=' necessary after name)
*
* Revision 1.22  1998/11/27 20:55:18  vakatov
* CCgiRequest::  made the input stream arg. be optional(std.input by default)
*
* Revision 1.21  1998/11/27 19:44:31  vakatov
* CCgiRequest::  Engage cmd.-line args if "$REQUEST_METHOD" is undefined
*
* Revision 1.20  1998/11/26 00:29:50  vakatov
* Finished NCBI CGI API;  successfully tested on MSVC++ and SunPro C++ 5.0
*
* Revision 1.19  1998/11/24 23:07:28  vakatov
* Draft(almost untested) version of CCgiRequest API
*
* Revision 1.18  1998/11/24 21:31:30  vakatov
* Updated with the ISINDEX-related code for CCgiRequest::
* TCgiEntries, ParseIndexes(), GetIndexes(), etc.
*
* Revision 1.17  1998/11/24 17:52:14  vakatov
* Starting to implement CCgiRequest::
* Fully implemented CCgiRequest::ParseEntries() static function
*
* Revision 1.16  1998/11/20 22:36:38  vakatov
* Added destructor to CCgiCookies:: class
* + Save the works on CCgiRequest:: class in a "compilable" state
*
* Revision 1.15  1998/11/19 23:41:09  vakatov
* Tested version of "CCgiCookie::" and "CCgiCookies::"
*
* Revision 1.14  1998/11/19 20:02:49  vakatov
* Logic typo:  actually, the cookie string does not contain "Cookie: "
*
* Revision 1.13  1998/11/19 19:50:00  vakatov
* Implemented "CCgiCookies::"
* Slightly changed "CCgiCookie::" API
*
* Revision 1.12  1998/11/18 21:47:50  vakatov
* Draft version of CCgiCookie::
*
* Revision 1.11  1998/11/17 23:47:13  vakatov
* + CCgiRequest::EMedia
*
* Revision 1.10  1998/11/17 02:02:08  vakatov
* Compiles through with SunPro C++ 5.0
*
* ==========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <list>
#include <map>
#include <time.h>

// (BEGIN_NCBI_SCOPE must be followed by END_NCBI_SCOPE later in this file)
BEGIN_NCBI_SCOPE


///////////////////////////////////////////////////////
// The CGI send-cookie class
//

class CCgiCookie {
public:
    // Copy constructor
    CCgiCookie(const CCgiCookie& cookie);

    // Throw the "invalid_argument" if "name" or "value" have invalid format
    //  - the "name" must not be empty; it must not contain '='
    //  - both "name" and "value" must not contain: ";, "
    CCgiCookie(const string& name, const string& value);

    // The cookie name cannot be changed during its whole timelife
    const string& GetName(void) const;

    // Compose and write to output stream "os":
    //   "Set-Cookie: name=value; expires=date; path=val_path; domain=dom_name;
    //    expires\n"
    // (here, only "name=value" is mandatory)
    CNcbiOstream& Write(CNcbiOstream& os) const;

    // Reset everything(but name!) to default state like CCgiCookie(m_Name, "")
    void Reset(void);

    // Set all attribute values(but name!) to those from "cookie"
    void CopyAttributes(const CCgiCookie& cookie);

    // All SetXXX() methods beneath:
    //  - set the property to "str" if "str" has valid format
    //  - throw the "invalid_argument" if "str" has invalid format
    void SetValue     (const string& str);
    void SetDomain    (const string& str);  // not spec'd by default
    void SetValidPath (const string& str);  // not spec'd by default
    void SetExpDate   (const tm& exp_date); // infinite by default
    void SetSecure    (bool secure);        // "false" by default

    // All "bool GetXXX(...)" methods beneath:
    //  - return "true"  and copy the property to the "str" if the prop. is set
    //  - return "false" and empty the "str" if the property is not set
    //  - throw the "invalid_argument" exception if argument is a zero pointer
    string GetValue   (void)         const;  // if not set, return empty string
    bool GetValue     (string*  str) const;
    bool GetDomain    (string*  str) const;
    bool GetValidPath (string*  str) const;
    bool GetExpDate   (string*  str) const;  // "Wed Aug 9 07:49:37 1994"
    bool GetExpDate   (tm* exp_date) const;
    bool GetSecure    (void)         const;

private:
    string m_Name;
    string m_Value;
    string m_Domain;
    string m_ValidPath;
    tm     m_Expires;
    bool   m_Secure;

    static void x_CheckField(const string& str, const char* banned_symbols);
    static bool x_GetString(string* str, const string& val);
    // prohibit default assignment
    CCgiCookie& operator=(const CCgiCookie&) { _TROUBLE;  return *this; }
};  // CCgiCookie


inline CNcbiOstream& operator<<(CNcbiOstream& os, const CCgiCookie& cookie) {
    return cookie.Write(os);
}



///////////////////////////////////////////////////////
// Set of CGI send-cookies
//

class CCgiCookies {
public:
    // Empty set of cookies
    CCgiCookies(void);
    // Format of the string:  "name1=value1; name2=value2; ..."
    CCgiCookies(const string& str);
    // Destructor
    ~CCgiCookies(void);

    // All Add() functions override the value and attributes of cookie
    // already existing in this set if the added cookie has the same name
    CCgiCookie* Add(const string& name, const string& value);
    CCgiCookie* Add(const CCgiCookie& cookie);  // add a copy of "cookie"
    void Add(const CCgiCookies& cookies);  // update by a set of cookies
    void Add(const string& str); // "name1=value1; name2=value2; ..."

    CCgiCookie* Find(const string& name) const;  // return zero if can not find
    bool Empty(void) const;  // "true" if contains no cookies
    bool Remove(const string& name);  // return "false" if can not find
    void Clear(void);  // remove all stored cookies

    // Printout all cookies into the stream "os"(see also CCgiCookie::Write())
    CNcbiOstream& Write(CNcbiOstream& os) const;

private:
    typedef list<CCgiCookie*> TCookies;
    TCookies m_Cookies;  // (guaranteed to have no same-name cookies)
    void x_Add(CCgiCookie* cookie);
    TCookies::iterator x_Find(const string& name) const;
    // prohibit default initialization and assignment
    CCgiCookies(const CCgiCookies&) { _TROUBLE; }
    CCgiCookies& operator=(const CCgiCookies&) { _TROUBLE;  return *this; }
};  // CCgiCookies


inline CNcbiOstream& operator<<(CNcbiOstream& os, const CCgiCookies& cookies) {
    return cookies.Write(os);
}



///////////////////////////////////////////////////////
// The CGI request class
//

// Set of "standard" HTTP request properties
enum ECgiProp {
    // server properties
    eCgi_ServerSoftware = 0,
    eCgi_ServerName,
    eCgi_GatewayInterface,
    eCgi_ServerProtocol,
    eCgi_ServerPort,        // see also "GetServerPort()"

    // client properties
    eCgi_RemoteHost,
    eCgi_RemoteAddr,        // see also "GetRemoteAddr()"

    // client data properties
    eCgi_ContentType,
    eCgi_ContentLength,     // see also "GetContentLength()"

    // request properties
    eCgi_RequestMethod,
    eCgi_PathInfo,
    eCgi_PathTranslated,
    eCgi_ScriptName,
    eCgi_QueryString,

    // authentication info
    eCgi_AuthType,
    eCgi_RemoteUser,
    eCgi_RemoteIdent,

    // semi-standard properties(from HTTP header)
    eCgi_HttpAccept,
    eCgi_HttpCookie,
    eCgi_HttpIfModifiedSince,
    eCgi_HttpReferer,
    eCgi_HttpUserAgent,

    // # of CCgiRequest-supported standard properties
    // for internal use only!
    eCgi_NProperties
};  // ECgiProp


// Typedefs
typedef map<string, string>      TCgiProperties;
typedef multimap<string, string> TCgiEntries;
typedef TCgiEntries::iterator    TCgiEntriesI;
typedef TCgiEntries::const_iterator    TCgiEntriesCI;
typedef list<string>             TCgiIndexes;


//
class CCgiRequest {
public:
    // Startup initialization:
    //   retrieve request's properties and cookies from environment
    //   retrieve request's entries from environment and/or stream "istr"
    // By default(when "istr" is null) use standard input stream("NcbiCin")
    // If "indexes_as_entries" is "true" then interpret indexes as regular
    // FORM entries(with empty value)
    CCgiRequest(CNcbiIstream* istr=0, bool indexes_as_entries=true);
    // (Mostly for the debugging) If "$REQUEST_METHOD" is undefined then
    // try to retrieve request's entries from the 1st cmd.-line argument, and
    // do not use "$QUERY_STRING" and "istr" at all
    // See above for description of "istr" and "indexes_as_entries"
    CCgiRequest(int argc, char* argv[], CNcbiIstream* istr=0,
                bool indexes_as_entries=true);
    // Destructor
    ~CCgiRequest(void);

    // Get name(not value!) of a "standard" property
    static const string& GetPropertyName(ECgiProp prop);
    // Get value of a "standard" property(empty string if not specified)
    const string& GetProperty(ECgiProp prop) const;
    // Get value of a random client propertiy("$HTTP_<key>")
    const string& GetRandomProperty(const string& key) const;
    // Auxiliaries(to convert from the "string" representation)
    Uint2  GetServerPort(void) const;
    // Uint4  GetRemoteAddr(void) const;  // (in the network byte order)
    size_t GetContentLength(void) const;

    // Retrieve the request cookies
    const CCgiCookies& GetCookies(void) const;

    // Get a set of entries(decoded) received from the client.
    // Also includes "indexes" if "indexes_as_entries" in the
    // constructor was "true"(default).
    const TCgiEntries& GetEntries(void) const;

    // Get a set of indexes(decoded) received from the client.
    // It will always be empty if "indexes_as_entries" in the constructor
    // was "true"(default).
    const TCgiIndexes& GetIndexes(void) const;

    // Decode the URL-encoded(FORM or ISINDEX) string "str" into a set of
    // entries <"name", "value"> and add them to the "entries" set.
    // The new entries are added without overriding the original ones, even
    // if they have the same names.
    // FORM    format:  "name1=value1&.....", ('=' and 'value' are optional)
    // ISINDEX format:  "val1+val2+val3+....."
    // If the "str" is in ISINDEX format then the entry "value" will be empty.
    // On success, return zero;  otherwise return location(1-based) of error
    static SIZE_TYPE ParseEntries(const string& str, TCgiEntries& entries);

    // Decode the URL-encoded string "str" into a set of ISINDEX-like entries
    // and add them to the "indexes" set
    // On success, return zero, otherwise return location(1-based) of error
    static SIZE_TYPE ParseIndexes(const string& str, TCgiIndexes& indexes);

private:
    // set of the request properties(already retrieved; cached)
    TCgiProperties m_Properties;
    // set of the request FORM-like entries(already retrieved; cached)
    TCgiEntries m_Entries;
    // set of the request ISINDEX-like indexes(already retrieved; cached)
    TCgiIndexes m_Indexes;
    // set of the request cookies(already retrieved; cached)
    CCgiCookies m_Cookies;

    // the real constructor code
    void x_Init(CNcbiIstream* istr, int argc, char** argv,
                bool indexes_as_entries);
    // retrieve(and cache) a property of given name
    const string& x_GetPropertyByName(const string& name);

    // prohibit default initialization and assignment
    CCgiRequest(const CCgiRequest&) { _TROUBLE; }
    CCgiRequest& operator=(const CCgiRequest&) { _TROUBLE;  return *this; }
};  // CCgiRequest


// Decode the URL-encoded string "str";  return the result of decoding
// If "str" format is invalid then throw CParseException
extern string URL_DecodeString(const string& str);

// URL-encode regular string "str";  return the result of encoding
extern string URL_EncodeString(const string& str);


///////////////////////////////////////////////////////
// All inline function implementations are in this file
#include <corelib/ncbicgi.inl>


// (END_NCBI_SCOPE must be preceeded by BEGIN_NCBI_SCOPE)
END_NCBI_SCOPE

#endif  /* NCBICGI__HPP */
