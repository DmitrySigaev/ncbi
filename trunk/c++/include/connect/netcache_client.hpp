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
 * File Description:
 *   Net cache client API.
 *
 */

/// @file netcache_client.hpp
/// NetCache client specs. 
///

#include <connect/connect_export.h>
#include <corelib/ncbistd.hpp>
#include <util/reader_writer.hpp>


BEGIN_NCBI_SCOPE


class CSocket;


/// Client API for NetCache server
///
/// After any Put, Get transactions connection socket
/// is closed (part of NetCache protocol implemenation)
class NCBI_XCONNECT_EXPORT CNetCacheClient
{
public:
    CNetCacheClient(const string&  host,
                    unsigned short port,
                    const string&  client_name = kEmptyStr);

    /// Construction.
    /// @param sock
    ///    Connected socket to the server
    /// @param client_name
    ///    Identification name of connecting client
    CNetCacheClient(CSocket*      sock,
                    const string& client_name = kEmptyStr);

    ~CNetCacheClient();

    /// Put BLOB to server
    ///
    /// @param time_to_live
    ///    Timeout value in seconds. 0 - server side default assumed.
    string PutData(const void*   buf,
                   size_t        size,
                   unsigned int  time_to_live = 0);

    /// Retrieve BLOB from server by key
    /// If BLOB not found method returns NULL
    /// Caller is responsible for deletion of the IReader*
    ///
    /// @param key
    ///    BLOB key to read (returned by PutData)
    /// @return
    ///    IReader* (caller must delete this)
    IReader* GetData(const string& key);

    enum EReadResult {
        eReadComplete, ///< The whole BLOB has been read
        eNotFound,     ///< BLOB not found or error
        eReadPart      ///< Read part of the BLOB (buffer capacity)
    };

    /// Retrieve BLOB from server by key
    ///
    /// @note
    ///    Function waits for enought data to arrive.
    EReadResult GetData(const string&  key,
                        void*          buf, 
                        size_t         buf_size, 
                        size_t*        n_read = 0);

    /// Shutdown the server daemon.
    void ShutdownServer();

protected:
    bool ReadStr(CSocket& sock, string* str);
    void WriteStr(const char* str, size_t len);

    bool IsError(const char* str);

    void SendClientName();

private:
    CSocket*       m_Sock;
    string         m_Host;
    unsigned short m_Port;
    EOwnership     m_OwnSocket;
    string         m_ClientName;
};


END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.4  2004/10/08 12:30:34  lavr
 * Cosmetics
 *
 * Revision 1.3  2004/10/05 19:01:59  kuznets
 * Implemented ShutdownServer()
 *
 * Revision 1.2  2004/10/05 18:17:58  kuznets
 * comments, added GetData
 *
 * Revision 1.1  2004/10/04 18:44:33  kuznets
 * Initial revision
 *
 *
 * ===========================================================================
 */


#endif  /* CONN___NETCACHE_CLIENT__HPP */
