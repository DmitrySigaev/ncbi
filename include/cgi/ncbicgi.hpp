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
*   NCBI C++ CGI API:
*      CCgiCookie    -- one CGI cookie
*      CCgiCookies   -- set of CGI cookies
*      CCgiRequest   -- full CGI request
*
* --------------------------------------------------------------------------
* $Log$
* Revision 1.46  2001/01/30 23:17:30  vakatov
* + CCgiRequest::GetEntry()
*
* Revision 1.45  2000/11/01 20:34:04  vasilche
* Added HTTP_EOL string macro.
*
* Revision 1.44  2000/05/01 17:04:31  vasilche
* MSVC doesn't allow contant initialization in class body.
*
* Revision 1.43  2000/05/01 16:58:12  vasilche
* Allow missing Content-Length and Content-Type headers.
*
* Revision 1.42  2000/02/01 22:19:53  vakatov
* CCgiRequest::GetRandomProperty() -- allow to retrieve value of
* properties whose names are not prefixed by "HTTP_" (optional).
* Get rid of the aux.methods GetServerPort() and GetRemoteAddr() which
* are obviously not widely used (but add to the volume of API).
*
* Revision 1.41  2000/01/20 17:52:05  vakatov
* Two CCgiRequest:: constructors:  one using raw "argc", "argv", "envp",
* and another using auxiliary classes "CNcbiArguments" and "CNcbiEnvironment".
* + constructor flag CCgiRequest::fOwnEnvironment to take ownership over
* the passed "CNcbiEnvironment" object.
*
* Revision 1.40  1999/12/30 22:11:59  vakatov
* Fixed and added comments.
* CCgiCookie::GetExpDate() -- use a more standard time string format.
*
* Revision 1.39  1999/11/02 20:35:38  vakatov
* Redesigned of CCgiCookie and CCgiCookies to make them closer to the
* cookie standard, smarter, and easier in use
*
* Revision 1.38  1999/09/03 21:26:44  vakatov
* + #include <memory>
*
* Revision 1.37  1999/06/21 16:04:15  vakatov
* CCgiRequest::CCgiRequest() -- the last(optional) arg is of type
* "TFlags" rather than the former "bool"
*
* Revision 1.36  1999/05/11 03:11:46  vakatov
* Moved the CGI API(along with the relevant tests) from "corelib/" to "cgi/"
*
* Revision 1.35  1999/05/04 16:14:04  vasilche
* Fixed problems with program environment.
* Added class CNcbiEnvironment for cached access to C environment.
*
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
* ==========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <list>
#include <map>
#include <set>
#include <memory>
#include <time.h>

#define HTTP_EOL "\r\n"


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
    //  - "name", "value", "domain" -- must consist of printable ASCII
    //    characters, and not: semicolons(;), commas(,), or space characters.
    //  - "path" -- can have space characters.
    CCgiCookie(const string& name, const string& value,
               const string& domain = NcbiEmptyString,
               const string& path   = NcbiEmptyString);

    // The cookie name cannot be changed during its whole timelife
    const string& GetName(void) const;

    // Compose and write to output stream "os":
    //   "Set-Cookie: name=value; expires=date; path=val_path; domain=dom_name;
    //    secure\n"
    // Here, only "name=value" is mandatory, and other parts are optional
    CNcbiOstream& Write(CNcbiOstream& os) const;

    // Reset everything(but name!) to default state like CCgiCookie(m_Name, "")
    void Reset(void);

    // Set all attribute values(but name!) to those from "cookie"
    void CopyAttributes(const CCgiCookie& cookie);

    // All SetXXX(const string&) methods beneath:
    //  - set the property to "str" if "str" has valid format
    //  - throw the "invalid_argument" if "str" has invalid format
    void SetValue  (const string& str);
    void SetDomain (const string& str);  // not spec'd by default
    void SetPath   (const string& str);  // not spec'd by default
    void SetExpDate(const tm& exp_date); // GMT time (infinite if all zeros)
    void SetSecure (bool secure);        // "false" by default

    // All "const string& GetXXX(...)" methods beneath return reference
    // to "NcbiEmptyString" if the requested attributre is not set
    const string& GetValue  (void) const;
    const string& GetDomain (void) const;
    const string& GetPath   (void) const;
    // Day, dd-Mon-yyyy hh:mm:ss GMT  (return empty string if not set)
    string        GetExpDate(void) const;
    // If exp.date is not set then return "false" and dont assign "*exp_date"
    bool GetExpDate(tm* exp_date) const;
    bool GetSecure(void)          const;

    // compare two cookies
    bool operator<(const CCgiCookie& cookie) const;

    // predicate
    typedef const CCgiCookie* TCPtr;
    struct PLessCPtr {
        bool operator() (const TCPtr& c1, const TCPtr& c2) const {
            return (*c1 < *c2);
        }
    };

private:
    string m_Name;
    string m_Value;
    string m_Domain;
    string m_Path;
    tm     m_Expires;  // GMT time zone
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
//  The cookie is uniquely identified by {name, domain, path}.
//  "name" is mandatory and non-empty;  "domain" and "path" are optional.
//  "name" and "domain" are not case-sensitive;  "path" is case-sensitive.
//

class CCgiCookies {
public:
    typedef set<CCgiCookie*, CCgiCookie::PLessCPtr>  TSet;
    typedef TSet::iterator         TIter;
    typedef TSet::const_iterator   TCIter;
    typedef pair<TIter,  TIter>    TRange;
    typedef pair<TCIter, TCIter>   TCRange;

    // Empty set of cookies
    CCgiCookies(void);
    // Format of the string:  "name1=value1; name2=value2; ..."
    CCgiCookies(const string& str);
    // Destructor
    ~CCgiCookies(void);

    // Return "true" if this set contains no cookies
    bool Empty(void) const;

    // All Add() functions:
    // if the added cookie has the same {name, domain, path} as an already
    // existing one then the new cookie will override the old one
    CCgiCookie* Add(const string& name, const string& value,
                    const string& domain = NcbiEmptyString,
                    const string& path   = NcbiEmptyString);
    CCgiCookie* Add(const CCgiCookie& cookie);  // add a copy of "cookie"
    void Add(const CCgiCookies& cookies);  // update by a set of cookies
    void Add(const string& str); // "name1=value1; name2=value2; ..."

    // Return NULL if cannot find this exact cookie
    CCgiCookie*       Find(const string& name,
                           const string& domain, const string& path);
    const CCgiCookie* Find(const string& name,
                           const string& domain, const string& path) const;

    // Return the first matched cookie with name "name", or NULL if
    // there is no such cookie(s).
    // Also, if "range" is non-NULL then assign its "first" and
    // "second" fields to the beginning and the end of the range
    // of cookies matching the name "name".
    // NOTE:  if there is a cookie with empty domain and path then
    //        this cookie is guaranteed to be returned.
    CCgiCookie*       Find(const string& name, TRange*  range=0);
    const CCgiCookie* Find(const string& name, TCRange* range=0) const;

    // Remove "cookie" from this set;  deallocate it if "destroy" is true
    // Return "false" if can not find "cookie" in this set
    bool Remove(CCgiCookie* cookie, bool destroy=true);

    // Remove (and destroy if "destroy" is true) all cookies belonging
    // to range "range".  Return # of found and removed cookies.
    size_t Remove(TRange& range, bool destroy=true);

    // Remove (and destroy if "destroy" is true) all cookies with the
    // given "name".  Return # of found and removed cookies.
    size_t Remove(const string& name, bool destroy=true);

    // Remove all stored cookies
    void Clear(void);

    // Printout all cookies into the stream "os" (see also CCgiCookie::Write())
    CNcbiOstream& Write(CNcbiOstream& os) const;

private:
    TSet m_Cookies;

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
typedef map<string, string>          TCgiProperties;
typedef multimap<string, string>     TCgiEntries;
typedef TCgiEntries::iterator        TCgiEntriesI;
typedef TCgiEntries::const_iterator  TCgiEntriesCI;
typedef list<string>                 TCgiIndexes;


// Forward class declarations
class CNcbiArguments;
class CNcbiEnvironment;

// constant returned by GetContentLength() when Content-Length header is missing
static const size_t kContentLengthUnknown = size_t(-1);

//
class CCgiRequest {
public:
    // Startup initialization:
    //   retrieve request's properties and cookies from environment
    //   retrieve request's entries from "$QUERY_STRING"
    // If "$REQUEST_METHOD" == "POST" then add entries from stream "istr"
    // If "$REQUEST_METHOD" is undefined then try to retrieve the request's
    // entries from the 1st cmd.-line argument, and do not use "$QUERY_STRING"
    // and "istr" at all
    typedef int TFlags;
    enum Flags {
        // dont handle indexes as regular FORM entries with empty value
        fIndexesNotEntries  = 0x1,
        // do not parse $QUERY_STRING
        fIgnoreQueryString  = 0x2,
        // own the passed "env" (and destroy them it in destructor)
        fOwnEnvironment     = 0x4
    };
    CCgiRequest(const         CNcbiArguments*   args = 0,
                const         CNcbiEnvironment* env  = 0,
                CNcbiIstream* istr  = 0 /* NcbiCin */,
                TFlags        flags = 0);
    // args := CNcbiArguments(argc,argv), env := CNcbiEnvironment(envp)
    CCgiRequest(int                argc,
                const char* const* argv,
                const char* const* envp  = 0,
                CNcbiIstream*      istr  = 0,
                TFlags             flags = 0);

    // Destructor
    ~CCgiRequest(void);

    // Get name(not value!) of a "standard" property
    static const string& GetPropertyName(ECgiProp prop);

    // Get value of a "standard" property (return empty string if not defined)
    const string& GetProperty(ECgiProp prop) const;

    // Get value of a random client property;  if "http" is TRUE then add
    // prefix "HTTP_" to the property name.
    // NOTE:  usually, the value is extracted from the environment variable
    //        named "$[HTTP]_<key>". Be advised, however, that in the case of
    //        FastCGI application, the set (and values) of env.variables change
    //        from request to request, and they differ from those returned
    //        by CNcbiApplication::GetEnvironment()!
    const string& GetRandomProperty(const string& key, bool http=true) const;

    // Get content length (using value of the property 'eCgi_ContentLength')
    // may return kContentLengthUnknown if Content-Length header is missing
    size_t GetContentLength(void) const;

    // Retrieve the request cookies
    const CCgiCookies& GetCookies(void) const;

    // Get a set of entries(decoded) received from the client.
    // Also includes "indexes" if "indexes_as_entries" in the
    // constructor was "true"(default).
    const TCgiEntries& GetEntries(void) const;

    // Get entry value by name
    // NOTE:  There can be more than one entry with the same name;
    //        only one of these entry will be returned.
    // To get all matches, use GetEntries() and "multimap::" member functions.
    const string& GetEntry(const string& name, bool* is_found = 0);

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
    // set of environment variables
    const CNcbiEnvironment*    m_Env;
    auto_ptr<CNcbiEnvironment> m_OwnEnv;
    // set of the request FORM-like entries(already retrieved; cached)
    TCgiEntries m_Entries;
    // set of the request ISINDEX-like indexes(already retrieved; cached)
    TCgiIndexes m_Indexes;
    // set of the request cookies(already retrieved; cached)
    CCgiCookies m_Cookies;

    // the real constructor code
    void x_Init(const CNcbiArguments*   args,
                const CNcbiEnvironment* env,
                CNcbiIstream*           istr,
                TFlags                  flags);

    // retrieve(and cache) a property of given name
    const string& x_GetPropertyByName(const string& name) const;

    // prohibit default initialization and assignment
    CCgiRequest(const CCgiRequest&);  // { _TROUBLE; }
    CCgiRequest& operator=(const CCgiRequest&) { _TROUBLE;  return *this; }
};  // CCgiRequest


// Decode the URL-encoded string "str";  return the result of decoding
// If "str" format is invalid then throw CParseException
extern string URL_DecodeString(const string& str);

// URL-encode regular string "str";  return the result of encoding
extern string URL_EncodeString(const string& str);


///////////////////////////////////////////////////////
// All inline function implementations are in this file
#include <cgi/ncbicgi.inl>


// (END_NCBI_SCOPE must be preceeded by BEGIN_NCBI_SCOPE)
END_NCBI_SCOPE

#endif  /* NCBICGI__HPP */
