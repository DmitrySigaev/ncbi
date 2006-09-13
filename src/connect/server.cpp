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

/// @file server.cpp
/// Framework for a multithreaded network server

#include <ncbi_pch.hpp>
#include <connect/ncbi_buffer.h>
#include "connection_pool.hpp"
#include "ncbi_core_cxxp.hpp"
#include <util/thread_pool.hpp>


BEGIN_NCBI_SCOPE


typedef IServer_ConnectionBase TConnBase;
typedef CServer_Connection     TConnection;
typedef CServer_Listener       TListener;


/////////////////////////////////////////////////////////////////////////////
// IServer_MessageHandler implementation
void IServer_MessageHandler::OnSocketEvent(CSocket& socket, EIO_Event event)
{
    // All other events should be implemented by subclass, only read event
    // redirected back to IServer_MessageHandler. May be it is easier to
    // split OnSocketEvent into 4 separate event classes.
    if (!(event & eIO_Read)) return;
    char read_buf[4096];
    size_t n_read;
    EIO_Status status = socket.Read(read_buf, sizeof(read_buf), &n_read);
    switch (status) {
    case eIO_Success:
        break;
    case eIO_Timeout:
        this->OnTimeout(socket);
        return;
    case eIO_Closed:
        this->OnSocketEvent(socket, eIO_Close);
        return;
    default:
        // TODO: ??? OnError
        return;
    }
    int message_tail;
    char *buf_ptr = read_buf;
    for ( ;n_read > 0; ) {
        message_tail = this->CheckMessage(&m_Buffer, buf_ptr, n_read);
        // TODO: what should we do if message_tail > n_read?
        int consumed = n_read - (message_tail < 0 ? 0 : message_tail);
        if (message_tail < 0) {
            return;
        } else {
            this->OnMessage(m_Buffer);
        }
        buf_ptr += consumed;
        n_read -= consumed;
    }
}


/////////////////////////////////////////////////////////////////////////////
// Abstract class for CAcceptRequest and CIORequest
class CServerRequest : public CStdRequest
{
public:
    CServerRequest(EIO_Event event,
                   CServer_ConnectionPool& conn_pool,
                   const STimeout* timeout)
        : m_Event(event), m_ConnPool(conn_pool), m_IdleTimeout(timeout) {}

protected:
    EIO_Event                m_Event;
    CServer_ConnectionPool&  m_ConnPool;
    const STimeout*          m_IdleTimeout;
} ;


/////////////////////////////////////////////////////////////////////////////
// CAcceptRequest
class CAcceptRequest : public CServerRequest
{
public:
    CAcceptRequest(EIO_Event event,
                   CServer_ConnectionPool& conn_pool,
                   const STimeout* timeout,
                   TListener* listener) :
        CServerRequest(event, conn_pool, timeout),
        m_Listener(listener)
        { }
    virtual void Process(void);
private:
    TListener* m_Listener;
} ;

void CAcceptRequest::Process(void)
{
    try {
        TConnection* conn = m_Listener->OnConnectEvent(m_IdleTimeout);
        if (conn) {
            if (m_ConnPool.Add(conn, CServer_ConnectionPool::eActiveSocket)) {
                conn->OnSocketEvent(eIO_Open);
                // TODO: check status, and if ready to read 
                // conn->OnSocketEvent(eIO_Read);
                m_ConnPool.SetConnType(conn,
                                       CServer_ConnectionPool::eInactiveSocket);
            } else {
                // the connection pool is full
                conn->OnOverflow();
                delete conn;
                return;
            }
        }
    } STD_CATCH_ALL("CAcceptRequest::Process");
}

/////////////////////////////////////////////////////////////////////////////
// CIORequest
class CIORequest : public CServerRequest
{
public:
    CIORequest(EIO_Event event,
               CServer_ConnectionPool& conn_pool,
               const STimeout* timeout,
               TConnection* connection) :
        CServerRequest(event, conn_pool, timeout),
        m_Connection(connection)
        { }
    virtual void Process(void);
private:
    TConnection* m_Connection;
} ;

void CIORequest::Process(void)
{
    try {
        m_Connection->OnSocketEvent(m_Event);
    } STD_CATCH_ALL("CIORequest::Process");
    m_ConnPool.SetConnType(m_Connection,
                           CServer_ConnectionPool::eInactiveSocket);
}

CStdRequest* TListener::CreateRequest(EIO_Event event,
                                      CServer_ConnectionPool& conn_pool,
                                      const STimeout* timeout)
{
    return new CAcceptRequest(event, conn_pool, timeout, this);
}

CStdRequest* TConnection::CreateRequest(EIO_Event event,
                                        CServer_ConnectionPool& conn_pool,
                                        const STimeout* timeout)
{
    conn_pool.SetConnType(this, CServer_ConnectionPool::eActiveSocket);
    return new CIORequest(event, conn_pool, timeout, this);
}


/////////////////////////////////////////////////////////////////////////////
// CServer implementation

CServer::CServer(void)
{
    // TODO: auto_ptr-based initialization
    m_Parameters = new SServer_Parameters();
    m_ConnectionPool = new CServer_ConnectionPool(
        m_Parameters->max_connections);
    CONNECT_InitInternal();
}


CServer::~CServer()
{
    delete m_Parameters;
    delete m_ConnectionPool;
}


void CServer::AddListener(IServer_ConnectionFactory* factory,
                          unsigned short port)
{
    m_ConnectionPool->Add(new TListener(factory, port),
                    CServer_ConnectionPool::eListener);    
}


void CServer::SetParameters(const SServer_Parameters& new_params)
{
    if (new_params.init_threads <= 0  ||
        new_params.max_threads  < new_params.init_threads  ||
        new_params.max_threads > 1000) {
        NCBI_THROW(CServerException, eBadParameters,
                   "CServer::SetParameters: Bad parameters");
    }
    // TODO: notify m_ConnectionPool about changes in max_connections
    *m_Parameters = new_params;
}


void CServer::GetParameters(SServer_Parameters* params)
{
    *params = *m_Parameters;
}


void CServer::Run(void)
{
    CStdPoolOfThreads threadPool(m_Parameters->max_threads,
                                 m_Parameters->queue_size,
                                 m_Parameters->spawn_threshold);
    threadPool.Spawn(m_Parameters->init_threads);

    vector<CSocketAPI::SPoll> polls;
    size_t                    count;
    bool                      wait = false;
    while ( !ShutdownRequested() ) {
        m_ConnectionPool->Clean();

        if (wait) {
            unsigned int timeout_sec = kMax_UInt, timeout_nsec = 0;
            if (m_Parameters->accept_timeout != kDefaultTimeout
                && m_Parameters->accept_timeout != kInfiniteTimeout) {
                timeout_sec  = m_Parameters->accept_timeout->sec;
                timeout_nsec = m_Parameters->accept_timeout->usec * 1000;
            }
            try {
                threadPool.WaitForRoom(timeout_sec, timeout_nsec);
                wait = false;
                if (m_Parameters->temporarily_stop_listening) {
                    m_ConnectionPool->ResumeListening();
                }
            } catch (CBlockingQueueException&) {
                // timed-out
                // ??? Shouldn't we ProcessTimeout here?
            }
            continue;
        }

        m_ConnectionPool->GetPollVec(polls);
        CSocketAPI::Poll(polls, m_Parameters->accept_timeout, &count);
        if (count == 0) {
            ProcessTimeout();
            continue;
        }

        bool listener_activity = false;
        ITERATE (vector<CSocketAPI::SPoll>, it, polls) {
            if (!it->m_REvent) continue;
            TConnBase* conn_base = dynamic_cast<TConnBase*>(it->m_Pollable);
            _ASSERT(conn_base);
            // TODO: replace dynamic_cast with virtual call
            TListener*   listener = dynamic_cast<TListener*>  (it->m_Pollable);
            if (listener) {
                listener_activity = true;
            }
            try {
                threadPool.AcceptRequest(CRef<CStdRequest>
                     (conn_base->CreateRequest(it->m_REvent,
                                               *m_ConnectionPool,
                                               m_Parameters->idle_timeout)));
                if (threadPool.IsFull()  &&
                    m_Parameters->temporarily_stop_listening) {

                    m_ConnectionPool->StopListening();
                    wait = true;
                    break;
                }
            } catch (CBlockingQueueException&) {
                // avoid spinning if only regular sockets are active,
                // as they simply remain "on hold"
                wait = !listener_activity;
                // Queue is full, drop incoming connection.
                // ??? What should we do if conn_base is CServerConnection?
                // Should we close it?
                conn_base->OnOverflow();
            }
        }
    }

    m_ConnectionPool->StopListening();
    threadPool.KillAllThreads(true);
}


/////////////////////////////////////////////////////////////////////////////
// SServer_Parameters implementation

static const STimeout k_DefaultIdleTimeout = { 600, 0 };

SServer_Parameters::SServer_Parameters() :
    max_connections(100),
    temporarily_stop_listening(false),
    accept_timeout(kInfiniteTimeout),
    idle_timeout(&k_DefaultIdleTimeout),
    init_threads(5),
    max_threads(10),
    queue_size(20),
    spawn_threshold(1)
{
}

END_NCBI_SCOPE


/*
 * ===========================================================================
 *
 * $Log$
 * Revision 6.1  2006/09/13 18:32:21  joukovv
 * Added (non-functional yet) framework for thread-per-request thread pooling model,
 * netscheduled.cpp refactored for this model; bug in bdb_cursor.cpp fixed.
 *
 *
 * ===========================================================================
 */
