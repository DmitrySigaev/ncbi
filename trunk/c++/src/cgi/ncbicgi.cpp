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
* Revision 1.24  1999/04/14 21:01:22  vakatov
* s_HexChar():  get rid of "::tolower()"
*
* Revision 1.23  1999/04/14 20:11:56  vakatov
* + <stdio.h>
* Changed all "str.compare(...)" to "NStr::Compare(str, ...)"
*
* Revision 1.22  1999/04/14 17:28:58  vasilche
* Added parsing of CGI parameters from IMAGE input tag like "cmd.x=1&cmd.y=2"
* As a result special parameter is added with empty name: "=cmd"
*
* Revision 1.21  1999/03/01 21:02:22  vasilche
* Added parsing of 'form-data' requests.
*
* Revision 1.20  1999/01/07 22:03:42  vakatov
* s_URL_Decode():  typo fixed
*
* Revision 1.19  1999/01/07 21:15:22  vakatov
* Changed prototypes for URL_DecodeString() and URL_EncodeString()
*
* Revision 1.18  1999/01/07 20:06:04  vakatov
* + URL_DecodeString()
* + URL_EncodeString()
*
* Revision 1.17  1998/12/28 17:56:36  vakatov
* New CVS and development tree structure for the NCBI C++ projects
*
* Revision 1.16  1998/12/04 23:38:35  vakatov
* Workaround SunPro's "buggy const"(see "BW_01")
* Renamed "CCgiCookies::Erase()" method to "...Clear()"
*
* Revision 1.15  1998/12/01 00:27:19  vakatov
* Made CCgiRequest::ParseEntries() to read ISINDEX data, too.
* Got rid of now redundant CCgiRequest::ParseIndexesAsEntries()
*
* Revision 1.14  1998/11/30 21:23:19  vakatov
* CCgiRequest:: - by default, interprete ISINDEX data as regular FORM entries
* + CCgiRequest::ParseIndexesAsEntries()
* Allow FORM entry in format "name1&name2....." (no '=' necessary after name)
*
* Revision 1.13  1998/11/27 20:55:21  vakatov
* CCgiRequest::  made the input stream arg. be optional(std.input by default)
*
* Revision 1.12  1998/11/27 19:44:34  vakatov
* CCgiRequest::  Engage cmd.-line args if "$REQUEST_METHOD" is undefined
*
* Revision 1.11  1998/11/27 15:54:05  vakatov
* Cosmetics in the ParseEntries() diagnostics
*
* Revision 1.10  1998/11/26 00:29:53  vakatov
* Finished NCBI CGI API;  successfully tested on MSVC++ and SunPro C++ 5.0
*
* Revision 1.9  1998/11/24 23:07:30  vakatov
* Draft(almost untested) version of CCgiRequest API
*
* Revision 1.8  1998/11/24 21:31:32  vakatov
* Updated with the ISINDEX-related code for CCgiRequest::
* TCgiEntries, ParseIndexes(), GetIndexes(), etc.
*
* Revision 1.7  1998/11/24 17:52:17  vakatov
* Starting to implement CCgiRequest::
* Fully implemented CCgiRequest::ParseEntries() static function
*
* Revision 1.6  1998/11/20 22:36:40  vakatov
* Added destructor to CCgiCookies:: class
* + Save the works on CCgiRequest:: class in a "compilable" state
*
* Revision 1.5  1998/11/19 23:53:30  vakatov
* Bug/typo fixed
*
* Revision 1.4  1998/11/19 23:41:12  vakatov
* Tested version of "CCgiCookie::" and "CCgiCookies::"
*
* Revision 1.3  1998/11/19 20:02:51  vakatov
* Logic typo:  actually, the cookie string does not contain "Cookie: "
*
* Revision 1.2  1998/11/19 19:50:03  vakatov
* Implemented "CCgiCookies::"
* Slightly changed "CCgiCookie::" API
*
* Revision 1.1  1998/11/18 21:47:53  vakatov
* Draft version of CCgiCookie::
*
* ==========================================================================
*/

#include <corelib/ncbicgi.hpp>
#include <stdio.h>
#include <time.h>


// (BEGIN_NCBI_SCOPE must be followed by END_NCBI_SCOPE later in this file)
BEGIN_NCBI_SCOPE



///////////////////////////////////////////////////////
//  CCgiCookie
//

// auxiliary zero "tm" struct
const tm kZeroTime = { 0,0,0,0,0,0,0,0,0 };
inline bool s_ZeroTime(const tm& date) {
    return (::memcmp(&date, &kZeroTime, sizeof(tm)) == 0);
}


CCgiCookie::CCgiCookie(const CCgiCookie& cookie)
    : m_Name(cookie.m_Name),
      m_Value(cookie.m_Value),
      m_Domain(cookie.m_Domain),
      m_ValidPath(cookie.m_ValidPath)
{
    m_Expires = cookie.m_Expires;
    m_Secure  = cookie.m_Secure;
}

CCgiCookie::CCgiCookie(const string& name, const string& value)
{
    if ( name.empty() )
        throw invalid_argument("Empty cookie name");
    x_CheckField(name, " ;,=");
    m_Name = name;

    SetValue(value);
    m_Expires = kZeroTime;
    m_Secure = false;
}

void CCgiCookie::Reset(void)
{
    m_Value.erase();
    m_Domain.erase();
    m_ValidPath.erase();
    m_Expires = kZeroTime;
    m_Secure = false;
}

void CCgiCookie::CopyAttributes(const CCgiCookie& cookie)
{
    if (&cookie == this)
        return;

    m_Value     = cookie.m_Value;
    m_Domain    = cookie.m_Domain;
    m_ValidPath = cookie.m_ValidPath;
    m_Expires   = cookie.m_Expires;
    m_Secure    = cookie.m_Secure;
}

bool CCgiCookie::GetExpDate(string* str) const
{
    if ( !str )
        throw invalid_argument("Null arg. in CCgiCookie::GetExpDate()");
    if ( s_ZeroTime(m_Expires) )
        return false;

    char cs[25];
    if ( !::strftime(cs, sizeof(cs), "%a %b %d %H:%M:%S %Y", &m_Expires) )
        throw runtime_error("CCgiCookie::GetExpDate() -- strftime() failed");
    *str = cs;
    return true;
}

bool CCgiCookie::GetExpDate(tm* exp_date) const
{
    if ( !exp_date )
        throw invalid_argument("Null cookie exp.date");
    if ( s_ZeroTime(m_Expires) )
        return false;
    *exp_date = m_Expires;
    return true;
}

CNcbiOstream& CCgiCookie::Write(CNcbiOstream& os) const
{
    string str;
    str.reserve(1024);

    os << "Set-Cookie: ";

    os << GetName().c_str() << '=';
    if ( GetValue(&str) )
        os << str.c_str();

    if ( GetDomain(&str) )
        os << "; domain=" << str.c_str();
    if ( GetValidPath(&str) )
        os << "; path=" << str.c_str();
    if ( GetExpDate(&str) )
        os << "; expires=" << str.c_str();
    if ( GetSecure() )
        os << "; secure";

    os << NcbiEndl;
    return os;
}

// Check if the cookie field is valid
void CCgiCookie::x_CheckField(const string& str, const char* banned_symbols)
{
    if (banned_symbols  &&  str.find_first_of(banned_symbols) != NPOS)
        throw invalid_argument("CCgiCookie::CheckValidCookieField() [1]");

    for (const char* s = str.c_str();  *s;  s++) {
        if ( !isprint(*s) )
            throw invalid_argument("CCgiCookie::CheckValidCookieField() [2]");
    }
}



///////////////////////////////////////////////////////
// Set of CGI send-cookies
//

CCgiCookie* CCgiCookies::Add(const string& name, const string& value)
{
    CCgiCookie* ck = Find(name);
    if ( ck ) {  // override existing CCgiCookie
        ck->Reset();
        ck->SetValue(value);
    } else {  // create new CCgiCookie and add it
        ck = new CCgiCookie(name, value);
        x_Add(ck);
    }
    return ck;
}


CCgiCookie* CCgiCookies::Add(const CCgiCookie& cookie)
{
    CCgiCookie* ck = Find(cookie.GetName());
    if ( ck ) {  // override existing CCgiCookie
        ck->CopyAttributes(cookie);
    } else {  // create new CCgiCookie and add it
        ck = new CCgiCookie(cookie);
        x_Add(ck);
    }
    return ck;
}


void CCgiCookies::Add(const CCgiCookies& cookies)
{
    for (TCookies::const_iterator iter = cookies.m_Cookies.begin();
         iter != cookies.m_Cookies.end();  iter++) {
        Add(*(*iter));
    }
}


void CCgiCookies::Add(const string& str)
{
    SIZE_TYPE pos;
    for (pos = str.find_first_not_of(" \t\n"); ; ){
        SIZE_TYPE pos_beg = str.find_first_not_of(' ', pos);
        if (pos_beg == NPOS)
            return; // done
        SIZE_TYPE pos_mid = str.find_first_of('=', pos_beg);
        if (pos_mid == NPOS)
            break; // error
        SIZE_TYPE pos_end = str.find_first_of(';', pos_mid);
        if (pos_end != NPOS) {
            pos = pos_end + 1;
            pos_end--;
        } else {
            pos_end = str.find_last_not_of(" \t\n", str.length());
            if (pos_end == NPOS)
                break; // error
            pos = NPOS; // about to finish
        }

        Add(str.substr(pos_beg, pos_mid-pos_beg),
            str.substr(pos_mid+1, pos_end-pos_mid));
    }

    throw CParseException("Invalid cookie string: `" + str + "'", pos);
}

CNcbiOstream& CCgiCookies::Write(CNcbiOstream& os) const
{
    for (TCookies::const_iterator iter = m_Cookies.begin();
         iter != m_Cookies.end();  iter++) {
        os << *(*iter);
    }
    return os;
}

CCgiCookies::TCookies::iterator CCgiCookies::x_Find(const string& name)
const
{
    TCookies& xx_Cookies = const_cast<TCookies&>(m_Cookies); //BW_01
    for (TCookies::iterator iter = xx_Cookies.begin();
         iter != xx_Cookies.end();  iter++) {
        if (name.compare((*iter)->GetName()) == 0)
            return iter;
    }
    return xx_Cookies.end();
}

void CCgiCookies::Clear(void) {
    for (TCookies::const_iterator iter = m_Cookies.begin();
         iter != m_Cookies.end();  iter++) {
        delete *iter;
    }
    m_Cookies.clear();
}



///////////////////////////////////////////////////////
//  CCgiRequest
//

// Standard property names
static const string s_PropName[eCgi_NProperties + 1] = {
    "SERVER_SOFTWARE",
    "SERVER_NAME",
    "GATEWAY_INTERFACE",
    "SERVER_PROTOCOL",
    "SERVER_PORT",

    "REMOTE_HOST",
    "REMOTE_ADDR",

    "CONTENT_TYPE",
    "CONTENT_LENGTH",

    "REQUEST_METHOD",
    "PATH_INFO",
    "PATH_TRANSLATED",
    "SCRIPT_NAME",
    "QUERY_STRING",

    "AUTH_TYPE",
    "REMOTE_USER",
    "REMOTE_IDENT",

    "HTTP_ACCEPT",
    "HTTP_COOKIE",
    "HTTP_IF_MODIFIED_SINCE",
    "HTTP_REFERER",
    "HTTP_USER_AGENT",

    ""  // eCgi_NProperties
};


const string& CCgiRequest::GetPropertyName(ECgiProp prop)
{
    if ((long)prop < 0  ||  (long)eCgi_NProperties <= (long)prop) {
        _TROUBLE;
        throw logic_error("CCgiRequest::GetPropertyName(BadPropIdx)");
    }
    return s_PropName[prop];
}


// Return integer (0..15) corresponding to the "ch" as a hex digit
// Return -1 on error
static int s_HexChar(char ch) THROWS_NONE
{
    if ('0' <= ch  &&  ch <= '9')
        return ch - '0';
    if ('a' <= ch  &&  ch <= 'f')
        return 10 + (ch - 'a');
    if ('A' <= ch  &&  ch <= 'F')
        return 10 + (ch - 'A');
    return -1;
}


// URL-decode string "str" into itself
// Return 0 on success;  otherwise, return 1-based error position
static SIZE_TYPE s_URL_Decode(string& str)
{
    SIZE_TYPE len = str.length();
    if ( !len )
        return 0;

    SIZE_TYPE p = 0;
    for (SIZE_TYPE pos = 0;  pos < len;  p++) {
        switch ( str[pos] ) {
        case '%': {
            if (pos + 2 > len)
                return (pos + 1);
            int i1 = s_HexChar(str[pos+1]);
            if (i1 < 0  ||  15 < i1)
                return (pos + 2);
            int i2 = s_HexChar(str[pos+2]);
            if (i2 < 0  ||  15 < i2)
                return (pos + 3);
            str[p] = s_HexChar(str[pos+1]) * 16 + s_HexChar(str[pos+2]);
            pos += 3;
            break;
        }
        case '+': {
            str[p] = ' ';
            pos++;
            break;
        }
        default:
            str[p] = str[pos++];
        }
    }

    if (p < len) {
        str[p] = '\0';
        str.resize(p);
    }

    return 0;
}


// Parse ISINDEX-like query string into "indexes" XOR "entries" --
// whichever is not null
// Return 0 on success;  return 1-based error position otherwise
static SIZE_TYPE s_ParseIsIndex(const string& str,
                                TCgiIndexes* indexes, TCgiEntries* entries)
{
    _ASSERT( !indexes != !entries );

    SIZE_TYPE len = str.length();
    if ( !len )
        return 0;

    // No '=' and spaces must present in the parsed string
    SIZE_TYPE err_pos = str.find_first_of("=& \t\r\n");
    if (err_pos != NPOS)
        return err_pos + 1;

    // Parse into indexes
    for (SIZE_TYPE beg = 0;  beg < len;  ) {
        // parse and URL-decode ISINDEX name
        SIZE_TYPE end = str.find_first_of('+', beg);
        if (end == beg  ||  end == len-1)
            return end + 1;  // error
        if (end == NPOS)
            end = len;

        string name = str.substr(beg, end - beg);
        if ((err_pos = s_URL_Decode(name)) != 0)
            return beg + err_pos;  // error

        // store
        if ( indexes ) {
            indexes->push_back(name);
        } else {
            pair<const string, string> entry(name, NcbiEmptyString);
            entries->insert(entry);
        }

        // continue
        beg = end + 1;
    }
    return 0;
}


// Guess if "str" is ISINDEX- or FORM-like, then fill out
// "entries" XOR "indexes";  if "indexes_as_entries" is "true" then
// interprete indexes as FORM-like entries(with empty value) and so
// put them to "entries"
static void s_ParseQuery(const string& str,
                         TCgiEntries&  entries,
                         TCgiIndexes&  indexes,
                         bool          indexes_as_entries)
{
    if (str.find_first_of("=&") == NPOS) { // ISINDEX
        SIZE_TYPE err_pos = indexes_as_entries ?
            s_ParseIsIndex(str, 0, &entries) :
            s_ParseIsIndex(str, &indexes, 0);
        if (err_pos != 0)
            throw CParseException("Init CCgiRequest::ParseISINDEX(\"" +
                                  str + "\"", err_pos);
    } else {  // regular(FORM) entries
        SIZE_TYPE err_pos = CCgiRequest::ParseEntries(str, entries);
        if (err_pos != 0)
            throw CParseException("Init CCgiRequest::ParseFORM(\"" +
                                  str + "\")", err_pos);
    }
}

static void s_ParseMultipartEntries(const string& boundary,
                                    const string& str,
                                    TCgiEntries& entries)
{
    // some constants in string
    const string NameStart("Content-Disposition: form-data; name=\"");
    const string Eol("\r\n");

    SIZE_TYPE pos = 0;
    SIZE_TYPE partStart = 0;
    string name;
    for ( ;; ) {
        SIZE_TYPE eol = str.find(Eol, pos);
        if ( eol == NPOS )
            throw CParseException("CCgiRequest::ParseMultipartQuery(\"" +
                                  boundary + "\"): unexpected eof: " + 
                                  str.substr(pos), 0);
        
        if ( eol == pos + boundary.size() &&
             NStr::Compare(str, pos, boundary.size(), boundary) == 0 ) {
            // boundary
            if ( partStart == NPOS ) // in header
                throw CParseException("CCgiRequest::ParseMultipartQuery(\"" +
                                      boundary + "\"): boundary in header: " + 
                                      str.substr(pos), 0);

            if ( partStart != 0 ) {
                SIZE_TYPE partEnd = pos - Eol.size();
                entries.insert(TCgiEntries::value_type(name,
                    str.substr(partStart, partEnd - partStart)));
            }

            partStart = NPOS;
            name = NcbiEmptyString;
        }
        else if ( eol == pos + boundary.size() + 2 &&
                  NStr::Compare(str, pos, boundary.size(), boundary) == 0  &&
                  str[eol-1] == '-' && str[eol-2] == '-') {
            // last boundary
            if ( partStart == NPOS ) // in header
                throw CParseException("CCgiRequest::ParseMultipartQuery(\"" +
                                      boundary + "\"): boundary in header: " + 
                                      str.substr(pos), 0);

            if ( partStart != 0 ) {
                SIZE_TYPE partEnd = pos - Eol.size();
                entries.insert(TCgiEntries::value_type(name,
                    str.substr(partStart, partEnd - partStart)));
            }

            return;
        }
        else {
            if ( partStart == NPOS ) {
               // in header
                if (pos + NameStart.size() <= eol  &&
                    NStr::Compare(str, pos, NameStart.size(), NameStart) ==0) {
                    SIZE_TYPE nameStart = pos + NameStart.size();
                    SIZE_TYPE nameEnd = str.find('\"', nameStart);
                    if ( nameEnd == NPOS )
                        throw CParseException("\
CCgiRequest::ParseMultipartQuery(\"" + boundary + "\"): bad name header " + 
                                              str.substr(pos), 0);

                    // new name
                    name = str.substr(nameStart, nameEnd - nameStart);
                }
                else if ( eol == pos ) {
                    // end of header
                    partStart = eol + Eol.size();
                }
                else {
                    _TRACE("unknown header: \"" <<
                           str.substr(pos, eol - pos) << '"' );
                }
            }
        }
        pos = eol + Eol.size();
    }
}

static void s_ParsePostQuery(const string& contentType, const string& str,
                         TCgiEntries&  entries)
{
    if ( contentType == "application/x-www-form-urlencoded" ) {
        SIZE_TYPE err_pos = CCgiRequest::ParseEntries(str, entries);
        if ( err_pos != 0 )
            throw CParseException("Init CCgiRequest::ParseFORM(\"" +
                                  str + "\")", err_pos);
    }
    else if ( NStr::StartsWith(contentType, "multipart/form-data") ) {
        string start = "boundary=";
        SIZE_TYPE pos = contentType.find(start);
        if ( pos == NPOS )
            throw CParseException("CCgiRequest::ParsePostQuery(\"" +
                                  contentType + "\"): no boundary field", 0);
        s_ParseMultipartEntries("--" + contentType.substr(pos + start.size()),
                              str, entries);
    }
    else {
        throw CParseException("CCgiRequest::ParsePostQuery(\"" +
                              contentType + "\"): invalid content type", 0);
    }
}

void CCgiRequest::x_Init(CNcbiIstream* istr, int argc, char** argv,
                         bool indexes_as_entries)
{
    // cache "standard" properties
    for (size_t prop = 0;  prop < (size_t)eCgi_NProperties;  prop++) {
        x_GetPropertyByName(GetPropertyName((ECgiProp)prop));
    }

    // compose cookies
    m_Cookies.Add(GetProperty(eCgi_HttpCookie));

    // parse entries or indexes, if any
    const string* query_string = 0;
    string arg_string;
    if ( GetProperty(eCgi_RequestMethod).empty() ) {
        // special case("$REQUEST METHOD" undefined, so use cmd.-line args)
        if (argc > 1  &&  argv  &&  argv[1]  &&  *argv[1])
            arg_string = argv[1];
        query_string = &arg_string;
    }
    else // regular case -- read from "$QUERY_STRING"
        query_string = &GetProperty(eCgi_QueryString);

    // POST method?
    if ( AStrEquiv(GetProperty(eCgi_RequestMethod), "POST", PNocase())) {
        if ( !istr )
            istr = &NcbiCin;  // default input stream
        size_t len = GetContentLength();
        string str;
        str.resize(len);
        if (!istr->read(&str[0], len)  ||  istr->gcount() != len)
            throw CParseException("Init CCgiRequest::CCgiRequest -- error "
                                  "in reading POST content", istr->gcount());

        // parse query from the POST content
        s_ParsePostQuery(GetProperty(eCgi_ContentType), str, m_Entries);
    }
    else {
        // parse "$QUERY_STRING"(or cmd.-line arg)
        s_ParseQuery(*query_string, m_Entries, m_Indexes, indexes_as_entries);
    }

    if ( m_Entries.find(NcbiEmptyString) != m_Entries.end() ) {
        // there is already empty name key
        ERR_POST("empty key name: we'll not check for IMAGE names");
        return;
    }

    // check for IMAGE input entries like: "Command.x=5&Command.y=3" and
    // put them with empty string key for better access
    string imageName;
    for ( TCgiEntriesI i = m_Entries.begin(); i != m_Entries.end(); ++i ) {
        const string& entry = i->first;
        SIZE_TYPE size = entry.size();
        // check for our case
        if ( size > 2 && NStr::Compare(entry, size - 2, 2, ".x") == 0 ) {
            // get base name of IMAGE
            string name = entry.substr(0, size - 2);
            // check for .y part
            if ( m_Entries.find(name + ".y") != m_Entries.end() ) {
                // name is correct IMAGE name
                if ( imageName.empty() ) {
                    // is't first name - Ok
                    imageName = name;
                }
                else {
                    // is't second name - error: we will not change anything
                    ERR_POST("duplicated IMAGE name: \"" << imageName <<
                             "\" and \"" << name << "\"");
                    return;
                }
            }
        }
    }
    m_Entries.insert(TCgiEntries::value_type(NcbiEmptyString, imageName));
}

const string& CCgiRequest::x_GetPropertyByName(const string& name)
{
    TCgiProperties::const_iterator find_iter = m_Properties.find(name);

    if (find_iter != m_Properties.end())
        return (*find_iter).second;  // already retrieved(cached)

    // retrieve and cache for the later use
    const char* env_str = ::getenv(name.c_str());
    pair<TCgiProperties::iterator, bool> ins_pair =
        m_Properties.insert(TCgiProperties::value_type
                            (name, env_str ? env_str : NcbiEmptyCStr));
    _ASSERT( ins_pair.second );
    return (*ins_pair.first).second;
}


const string& CCgiRequest::GetProperty(ECgiProp property)
const
{
    // This does not work on SunPro 5.0 by some reason:
    //    return m_Properties.find(GetPropName(property)))->second;
    // and this is the workaround [BW_01]:
    TCgiProperties& xx_Properties = const_cast<TCgiProperties&>(m_Properties);
    return xx_Properties.find(GetPropertyName(property))->second;
}


Uint2 CCgiRequest::GetServerPort(void) const
{
    long l = -1;
    const string& str = GetProperty(eCgi_ServerPort);
    if (sscanf(str.c_str(), "%ld", &l) != 1  ||  l < 0  ||  kMax_UI2 < l) {
        throw runtime_error("CCgiRequest:: Bad server port: \"" + str + "\"");
    }
    return (Uint2)l;
}


// Uint4 CCgiRequest::GetRemoteAddr(void) const
// {
//     /////  These definitions are to be removed as soon as we have
//     /////  a portable network header!
//     const Uint4 INADDR_NONE = (Uint4)(-1);
//     extern unsigned long inet_addr(const char *);
//     /////
// 
//     const string& str = GetProperty(eCgi_RemoteAddr);
//     Uint4 addr = inet_addr(str.c_str());
//     if (addr == INADDR_NONE)
//         throw runtime_error("CCgiRequest:: Invalid client address: " + str);
//     return addr;
// }


size_t CCgiRequest::GetContentLength(void) const
{
    long l = -1;
    const string& str = GetProperty(eCgi_ContentLength);
    if (sscanf(str.c_str(), "%ld", &l) != 1  ||  l < 0)
        throw runtime_error("CCgiRequest:: Invalid content length: " + str);
    return (size_t)l;
}


SIZE_TYPE CCgiRequest::ParseEntries(const string& str, TCgiEntries& entries)
{
    SIZE_TYPE len = str.length();
    if ( !len )
        return 0;

    // If no '=' present in the parsed string then try to parse it as ISINDEX
    if (str.find_first_of("&=") == NPOS)
        return s_ParseIsIndex(str, 0, &entries);

    // No spaces must present in the parsed string
    SIZE_TYPE err_pos = str.find_first_of(" \t\r\n");
    if (err_pos != NPOS)
        return err_pos + 1;

    // Parse into entries
    for (SIZE_TYPE beg = 0;  beg < len;  ) {
        // parse and URL-decode name
        SIZE_TYPE mid = str.find_first_of(" \t\r\n=&", beg);
        if (mid == beg  ||  (str[mid] == '&'  &&  mid == len-1)  ||
            (mid != NPOS  &&  str[mid] != '&'  &&  str[mid] != '='))
            return mid + 1;  // error
        if (mid == NPOS)
            mid = len;

        string name = str.substr(beg, mid - beg);
        if ((err_pos = s_URL_Decode(name)) != 0)
            return beg + err_pos;  // error

        // parse and URL-decode value(if any)
        string value;
        if (str[mid] == '=') { // has a value 
            mid++;
            SIZE_TYPE end = str.find_first_of(" =&", mid);
            if (end != NPOS  &&  (str[end] != '&'  ||  end == len-1))
                return end + 1;  // error
            if (end == NPOS)
                end = len;

            value = str.substr(mid, end - mid);
            if ((err_pos = s_URL_Decode(value)) != 0)
                return mid + err_pos;  // error

            beg = end + 1;
        } else {  // has no value
            beg = mid + 1;
        }

        // compose & store the name-value pair
        pair<const string, string> entry(name, value);
        entries.insert(entry);
    }
    return 0;
}


SIZE_TYPE CCgiRequest::ParseIndexes(const string& str, TCgiIndexes& indexes)
{
    return s_ParseIsIndex(str, &indexes, 0);
}


extern string URL_DecodeString(const string& str)
{
    string    x_str = str;
    SIZE_TYPE err_pos = s_URL_Decode(x_str);
    if (err_pos != 0)
        throw CParseException("URL_DecodeString(<badly_formatted_str>)",
                              err_pos);
    return x_str;
}


extern string URL_EncodeString(const string& str)
{
    static const char s_Encode[256][4] = {
        "%00", "%01", "%02", "%03", "%04", "%05", "%06", "%07",
        "%08", "%09", "%0A", "%0B", "%0C", "%0D", "%0E", "%0F",
        "%10", "%11", "%12", "%13", "%14", "%15", "%16", "%17",
        "%18", "%19", "%1A", "%1B", "%1C", "%1D", "%1E", "%1F",
        "+",   "!",   "%22", "%23", "$",   "%25", "%26", "'",
        "(",   ")",   "*",   "%2B", ",",   "-",   ".",   "%2F",
        "0",   "1",   "2",   "3",   "4",   "5",   "6",   "7",
        "8",   "9",   "%3A", "%3B", "%3C", "%3D", "%3E", "%3F",
        "%40", "A",   "B",   "C",   "D",   "E",   "F",   "G",
        "H",   "I",   "J",   "K",   "L",   "M",   "N",   "O",
        "P",   "Q",   "R",   "S",   "T",   "U",   "V",   "W",
        "X",   "Y",   "Z",   "%5B", "%5C", "%5D", "%5E", "_",
        "%60", "a",   "b",   "c",   "d",   "e",   "f",   "g",
        "h",   "i",   "j",   "k",   "l",   "m",   "n",   "o",
        "p",   "q",   "r",   "s",   "t",   "u",   "v",   "w",
        "x",   "y",   "z",   "%7B", "%7C", "%7D", "%7E", "%7F",
        "%80", "%81", "%82", "%83", "%84", "%85", "%86", "%87",
        "%88", "%89", "%8A", "%8B", "%8C", "%8D", "%8E", "%8F",
        "%90", "%91", "%92", "%93", "%94", "%95", "%96", "%97",
        "%98", "%99", "%9A", "%9B", "%9C", "%9D", "%9E", "%9F",
        "%A0", "%A1", "%A2", "%A3", "%A4", "%A5", "%A6", "%A7",
        "%A8", "%A9", "%AA", "%AB", "%AC", "%AD", "%AE", "%AF",
        "%B0", "%B1", "%B2", "%B3", "%B4", "%B5", "%B6", "%B7",
        "%B8", "%B9", "%BA", "%BB", "%BC", "%BD", "%BE", "%BF",
        "%C0", "%C1", "%C2", "%C3", "%C4", "%C5", "%C6", "%C7",
        "%C8", "%C9", "%CA", "%CB", "%CC", "%CD", "%CE", "%CF",
        "%D0", "%D1", "%D2", "%D3", "%D4", "%D5", "%D6", "%D7",
        "%D8", "%D9", "%DA", "%DB", "%DC", "%DD", "%DE", "%DF",
        "%E0", "%E1", "%E2", "%E3", "%E4", "%E5", "%E6", "%E7",
        "%E8", "%E9", "%EA", "%EB", "%EC", "%ED", "%EE", "%EF",
        "%F0", "%F1", "%F2", "%F3", "%F4", "%F5", "%F6", "%F7",
        "%F8", "%F9", "%FA", "%FB", "%FC", "%FD", "%FE", "%FF"
    };

    string url_str;

    SIZE_TYPE len = str.length();
    if ( !len )
        return url_str;

    SIZE_TYPE pos;
    SIZE_TYPE url_len = len;
    const unsigned char* cstr = (const unsigned char*)str.c_str();
    for (pos = 0;  pos < len;  pos++) {
        if (s_Encode[cstr[pos]][0] == '%')
            url_len += 2;
    }
    url_str.reserve(url_len + 1);
    url_str.resize(url_len);

    SIZE_TYPE p = 0;
    for (pos = 0;  pos < len;  pos++, p++) {
        const char* subst = s_Encode[cstr[pos]];
        if (*subst != '%') {
            url_str[p] = *subst;
        } else {
            url_str[  p] = '%';
            url_str[++p] = *(++subst);
            url_str[++p] = *(++subst);
        }
    }

    _ASSERT( p == url_len );
    url_str[url_len] = '\0';
    return url_str;
}


// (END_NCBI_SCOPE must be preceeded by BEGIN_NCBI_SCOPE)
END_NCBI_SCOPE
