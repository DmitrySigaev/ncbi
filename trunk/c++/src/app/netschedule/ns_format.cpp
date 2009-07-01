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
* Authors:  Victor Joukov
*
* File Description: Formatting functions for NetSchedule
*
*/

#include <ncbi_pch.hpp>
#include "ns_format.hpp"
#include <corelib/ncbistr.hpp>
#include <connect/ncbi_socket.hpp>

BEGIN_NCBI_SCOPE

string FormatNSId(const string& val, SQueueDescription* qdesc)
{
    string res("JSID_01_");
    res += val;
    res += '_';
    res += qdesc->host;
    res += '_';
    res += NStr::IntToString(qdesc->port);
    return res;
}


string FormatHostName(unsigned host, SQueueDescription* qdesc)
{
    if (!host) return "0.0.0.0";
    string host_name;
    map<unsigned, string>::iterator it =
        qdesc->host_name_cache.find(host);
    if (it == qdesc->host_name_cache.end()) {
        host_name = CSocketAPI::gethostbyaddr(host);
        qdesc->host_name_cache[host] = host_name;
    } else {
        host_name = it->second;
    }
    return host_name;
}


string FormatHostName(const string& val, SQueueDescription* qdesc)
{
    return FormatHostName(NStr::StringToUInt(val), qdesc);
}


END_NCBI_SCOPE

