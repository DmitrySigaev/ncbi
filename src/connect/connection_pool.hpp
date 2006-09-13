#ifndef CONNECT___CONNECTION_POOL__HPP
#define CONNECT___CONNECTION_POOL__HPP

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
 * Authors:  Aaron Ucko, Victor Joukov
 *
 */

/// @file connection_pool.hpp
/// Internal header for threaded server connection pools.


#include <corelib/ncbitime.hpp>

#include "server_connection.hpp"


/** @addtogroup ThreadedServer
 *
 * @{
 */


BEGIN_NCBI_SCOPE

class CServer_ConnectionPool
{
public:
    CServer_ConnectionPool(unsigned int& max_connections) :
        m_MaxConnections(max_connections) { }

    typedef IServer_ConnectionBase TConnBase;
    typedef CServer_Connection     TConnection;
    typedef CServer_Listener       TListener;

    enum EConnType {
        eInactiveSocket,
        eActiveSocket,
        eListener
    };

    bool Add(TConnBase* conn, EConnType type);

    /// Resets the expiration time as a bonus.
    void SetConnType(TConnBase* conn, EConnType type);

    /// Clean up inactive connections which are no longer open or
    /// which have been idle for too long.
    void Clean(void);

    void GetPollVec(vector<CSocketAPI::SPoll>& polls) const;

    void StopListening(void);
    void ResumeListening(void);

    struct SPerConnInfo {
        void UpdateExpiration(const TConnBase* conn);

        CTime     expiration;
        EConnType type;
    };

private:
    typedef map<TConnBase*, SPerConnInfo> TData;
    volatile TData                        m_Data;
    mutable CMutex                        m_Mutex;
    unsigned int&                         m_MaxConnections;
};


END_NCBI_SCOPE


/* @} */


/*
 * ===========================================================================
 * $Log$
 * Revision 6.1  2006/09/13 18:32:21  joukovv
 * Added (non-functional yet) framework for thread-per-request thread pooling model,
 * netscheduled.cpp refactored for this model; bug in bdb_cursor.cpp fixed.
 *
 * ===========================================================================
 */

#endif  /* CONNECT___CONNECTION_POOL__HPP */
