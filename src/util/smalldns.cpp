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
* Author: Anton Golikov
*
* File Description: Resolve host name to ip address and back using preset ini-file
*
* ===========================================================================
*/

#include <util/smalldns.hpp>
#include <corelib/ncbistr.hpp>
#include <corelib/ncbireg.hpp>

#ifdef NCBI_OS_MAC
#	include <utsname.h>
#else
#	include <sys/utsname.h>
#endif

#include <errno.h>

BEGIN_NCBI_SCOPE

string CSmallDNS::sm_localHostName;

CSmallDNS::CSmallDNS(const string& local_hosts_file /* = "./hosts.ini" */)
{
    const string section("LOCAL_DNS");
    
    CNcbiIfstream is(local_hosts_file.c_str());
    if( !is.good() ) {
        ERR_POST(Error << "CSmallDNS: cannot open file: " << local_hosts_file);
        return;
    }
    CNcbiRegistry reg(is);
    list<string> items;
    
    reg.EnumerateEntries( section, &items );
    for(list<string>::const_iterator it = items.begin(); it != items.end(); ++it ) {
        string val = reg.Get( section, *it );
        if( !IsValidIP(val) ) {
            ERR_POST(Warning << "CSmallDNS: Bad IP address '" << val
                     << "' for " << *it);
        } else {
            m_map[*it] = val;
            m_map[val] = *it;
        }
    }
    is.close();
}
    
CSmallDNS::~CSmallDNS()
{
}

// static
bool CSmallDNS::IsValidIP(const string& ip)
{
    list<string> dig;
    
    NStr::Split(ip, ".", dig);
    if( dig.size() != 4 )
        return false;
    
    for(list<string>::const_iterator it = dig.begin(); it != dig.end(); ++it) {
        try {
            unsigned long i = NStr::StringToULong(*it);
            if( i > 255 )
                return false;
        } catch(...) {
            return false;
        }
    }
    return true;
}

string CSmallDNS::GetLocalIP(void) const
{
    return LocalResolveDNS(GetLocalHost());
}

// static
string CSmallDNS::GetLocalHost(void)
{
    if( sm_localHostName.empty() ) {
        struct utsname unm;
        errno = 0;
        if( uname(&unm) != -1 ) {
            sm_localHostName = unm.nodename;
        } else {
            ERR_POST(Warning << "CSmallDNS: Cannot detect host name, errno:" << errno);
        }
    }
    return sm_localHostName;
}

string CSmallDNS::LocalResolveDNS(const string& host) const
{
    if( IsValidIP(host) ) {
        return host;
    }
    
    map<string, string>::const_iterator it = m_map.find(host);
    if( it != m_map.end() ) {
        return it->second;
    }
    return kEmptyStr;
}

string CSmallDNS::LocalBackResolveDNS(const string& ip) const
{
    if( !IsValidIP(ip) ) {
        return kEmptyStr;
    }
    
    map<string, string>::const_iterator it = m_map.find(ip);
    if( it != m_map.end() ) {
        return it->second;
    }
    return kEmptyStr;
}
END_NCBI_SCOPE
