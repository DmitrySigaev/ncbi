#ifndef CONN___NETCACHE_CLIENT__HPP
#define CONN___NETCACHE_CLIENT__HPP

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
 * Authors:  Anatoliy Kuznetsov
 *
 * File Description: Net cache client API.
 *
 */

/// @file netcache_client.hpp
/// NetCache client specs. 
///

#include <corelib/ncbistd.hpp>
#include <connect/ncbi_core.h>


BEGIN_NCBI_SCOPE

class CSocket;
class IReader;

/// Client API for NetCache server
///
class NCBI_XCONNECT_EXPORT CNetCacheClient
{
public:
    /// Construction.
    /// @param sock
    ///    Connected socket to the server
    /// @param client_name
    ///    Identification name of connecting client
    CNetCacheClient(CSocket* sock, const char* client_name=0);

    /// Put BLOB to server
    string PutData(void* buf, size_t size);

    /// Retrieve BLOB from server by key
    /// If BLOB not found method returns NULL
    /// Caller is responsible for deletion of IReader*
    IReader* GetData(const string& key);

protected:

    bool ReadStr(CSocket& sock, string* str);
    bool IsError(const char* str);

private:
    CSocket*    m_Sock;
    const char* m_ClientName;
};


END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.1  2004/10/04 18:44:33  kuznets
 * Initial revision
 *
 *
 * ===========================================================================
 */


#endif  /* UTIL___NETCACHE_CLIENT__HPP */
