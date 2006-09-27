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
* File Description:
*   threaded server connection pool
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include "connection_pool.hpp"


BEGIN_NCBI_SCOPE


// SPerConnInfo
void CServer_ConnectionPool::SPerConnInfo::UpdateExpiration(
    const TConnBase* conn)
{
    const STimeout* timeout = kDefaultTimeout;
    const CSocket*  socket  = dynamic_cast<const CSocket*>(conn);

    if (socket) {
        timeout = socket->GetTimeout(eIO_ReadWrite);
    }
    if (type == eInactiveSocket  &&  timeout != kDefaultTimeout
        &&  timeout != kInfiniteTimeout) {
        expiration.SetCurrent();
        expiration.AddSecond(timeout->sec);
        expiration.AddNanoSecond(timeout->usec * 1000);
    } else {
        expiration.Clear();
    }
}


// CServer_ControlConnection
CStdRequest* CServer_ControlConnection::CreateRequest(
    EIO_Event event,
    CServer_ConnectionPool& connPool,
    const STimeout* timeout)
{
    char buf[64];
    Read(buf, sizeof(buf));
    return NULL;
}


//
CServer_ConnectionPool::CServer_ConnectionPool(unsigned max_connections) :
        m_MaxConnections(max_connections)
{
    // Create internal signalling connection from m_ControlSocket to
    // m_ControlSocketForPoll
    unsigned short port = 2049;
    CListeningSocket listener;
    for (port = 2049; port < 65535; ++port) {
        if (eIO_Success == listener.Listen(port, 5, fLSCE_BindLocal))
            break;
    }
    m_ControlSocket.Connect("127.0.0.1", port);
    listener.Accept(dynamic_cast<CSocket&>(m_ControlSocketForPoll));
}

CServer_ConnectionPool::~CServer_ConnectionPool()
{
    CMutexGuard guard(m_Mutex);
    TData& data = const_cast<TData&>(m_Data);
    for (TData::iterator next = data.begin(), it = next++;
         next != data.end();
         it = next, ++next) {
         it->first->OnTimeout();
         delete it->first;
         data.erase(it);
    }
}

bool CServer_ConnectionPool::Add(TConnBase* conn, EConnType type)
{
    CMutexGuard guard(m_Mutex);
    TData& data = const_cast<TData&>(m_Data);

    if (data.size() >= m_MaxConnections) {
        // XXX - try to prune old idle connections before giving up?
        return false;
    }

    SPerConnInfo& info = data[conn];
    info.type = type;
    info.UpdateExpiration(conn);

    return true;
}


void CServer_ConnectionPool::SetConnType(TConnBase* conn, EConnType type)
{
    CMutexGuard guard(m_Mutex);
    TData& data = const_cast<TData&>(m_Data);
    TData::iterator it = data.find(conn);
    if (it == data.end()) {
        _TRACE("SetConnType called on unknown connection.");
    } else {
        it->second.type = type;
        it->second.UpdateExpiration(conn);
    }
    // Signal poll cycle to re-read poll vector by sending
    // byte to control socket
    if (type == eInactiveSocket &&
        eIO_Success == m_ControlSocket.GetStatus(eIO_Open)) {
        m_ControlSocket.Write("", 1, NULL);
    }
}


void CServer_ConnectionPool::Clean(void)
{
    CTime now(CTime::eCurrent, CTime::eGmt);
    CMutexGuard guard(m_Mutex);
    TData& data = const_cast<TData&>(m_Data);
    if (data.empty()) {
        return;
    }
    for (TData::iterator next = data.begin(), it = next++;
         next != data.end();
         it = next, ++next) {
        if (it->second.type != eInactiveSocket) {
            continue;
        }
        if (!it->first->IsOpen() || it->second.expiration <= now) {
            it->first->OnTimeout();
            delete it->first;
            data.erase(it);
        }
    }
}


void CServer_ConnectionPool::GetPollVec(vector<CSocketAPI::SPoll>& polls) const
{
    polls.clear();
    CMutexGuard guard(m_Mutex);
    const TData& data = const_cast<const TData&>(m_Data);
    // Control socket goes here as well
    polls.reserve(data.size()+1);
    polls.push_back(CSocketAPI::SPoll(
        dynamic_cast<CPollable*>(&m_ControlSocketForPoll), eIO_Read));
    ITERATE (TData, it, data) {
        CPollable* pollable = dynamic_cast<CPollable*>(it->first);
        _ASSERT(pollable);
        // Check that socket is not processing packet - safeguards against
        // out-of-order packet processing by effectively pulling socket from
        // poll vector until it is done with previous packet. See comment in
        // server.cpp CServer_Connection::CreateRequest and CIORequest::Process
        if (it->second.type != eActiveSocket) {
            if (!it->first->IsOpen() ) {
                continue;
            }
            polls.push_back
                (CSocketAPI::SPoll(pollable, it->first->GetEventsToPollFor()));
        }
    }
}


void CServer_ConnectionPool::StopListening(void)
{
    CMutexGuard guard(m_Mutex);
    const TData& data = const_cast<const TData&>(m_Data);
    ITERATE (TData, it, data) {
        it->first->Passivate();
    }
}


void CServer_ConnectionPool::ResumeListening(void)
{
    CMutexGuard guard(m_Mutex);
    const TData& data = const_cast<const TData&>(m_Data);
    ITERATE (TData, it, data) {
        it->first->Activate();
    }
}

END_NCBI_SCOPE

/*
* ===========================================================================
*
* $Log$
* Revision 6.2  2006/09/27 21:26:06  joukovv
* Thread-per-request is finally implemented. Interface changed to enable
* streams, line-based message handler added, netscedule adapted.
*
* Revision 6.1  2006/09/13 18:32:21  joukovv
* Added (non-functional yet) framework for thread-per-request thread pooling model,
* netscheduled.cpp refactored for this model; bug in bdb_cursor.cpp fixed.
*
*
* ===========================================================================
*/
