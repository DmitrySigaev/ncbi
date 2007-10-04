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
#include "connection_pool.hpp"
#include "server_connection.hpp"
#include <connect/ncbi_buffer.h>
#include <connect/server.hpp>
#include <util/thread_pool.hpp>


BEGIN_NCBI_SCOPE


typedef IServer_ConnectionBase TConnBase;
typedef CServer_Connection     TConnection;
typedef CServer_Listener       TListener;


/////////////////////////////////////////////////////////////////////////////
// IServer_MessageHandler implementation
void IServer_MessageHandler::OnRead(void)
{
    CSocket &socket = GetSocket();
    char read_buf[4096];
    size_t n_read;
    EIO_Status status = socket.Read(read_buf, sizeof(read_buf), &n_read);
    switch (status) {
    case eIO_Success:
        break;
    case eIO_Timeout:
        this->OnTimeout();
        return;
    case eIO_Closed:
        this->OnClose();
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
// IServer_LineMessageHandler implementation
int IServer_LineMessageHandler::CheckMessage(
    BUF* buffer, const void * data, size_t size)
{
    size_t n, skip;
    const char * msg = (const char *) data;
    skip = 0;
    if (size && m_SeenCR && msg[0] == '\n') {
        ++skip;
    }
    m_SeenCR = false;
    for (n = skip; n < size; ++n) {
        if (msg[n] == '\r' || msg[n] == '\n' || msg[n] == '\0') {
            m_SeenCR = msg[n] == '\r';
            break;
        }
    }
    BUF_Write(buffer, msg+skip, n-skip);
    return size - n - 1;
}


/////////////////////////////////////////////////////////////////////////////
// Abstract class for CAcceptRequest and CServerConnectionRequest
class CServer_Request : public CStdRequest
{
public:
    CServer_Request(EIO_Event event,
                    CServer_ConnectionPool& conn_pool,
                    const STimeout* timeout)
        : m_Event(event), m_ConnPool(conn_pool), m_IdleTimeout(timeout) {}

    virtual void Cancel(void) = 0;

protected:
    EIO_Event                m_Event;
    CServer_ConnectionPool&  m_ConnPool;
    const STimeout*          m_IdleTimeout;
} ;


/////////////////////////////////////////////////////////////////////////////
// CAcceptRequest
class CAcceptRequest : public CServer_Request
{
public:
    CAcceptRequest(EIO_Event event,
                   CServer_ConnectionPool& conn_pool,
                   const STimeout* timeout,
                   TListener* listener);
    virtual void Process(void);
    virtual void Cancel(void);
private:
    TConnection* m_Connection;
} ;

CAcceptRequest::CAcceptRequest(EIO_Event event,
                               CServer_ConnectionPool& conn_pool,
                               const STimeout* timeout,
                               TListener* listener) :
        CServer_Request(event, conn_pool, timeout),
        m_Connection(NULL)
{
    // Accept connection in main thread to avoid race for listening
    // socket's accept method, but postpone connection's OnOpen for
    // pool thread because it can be arbitrarily long.
    static const STimeout kZeroTimeout = { 0, 0 };
    auto_ptr<CServer_Connection> conn(
        new CServer_Connection(listener->m_Factory->Create()));
    if (listener->Accept(*conn, &kZeroTimeout) != eIO_Success)
        return;
    conn->SetTimeout(eIO_ReadWrite, m_IdleTimeout);
    m_Connection = conn.release();
}

void CAcceptRequest::Process(void)
{
    if (!m_Connection) return;
    try {
        if (m_ConnPool.Add(m_Connection,
                            CServer_ConnectionPool::eActiveSocket)) {
            m_Connection->OnSocketEvent(eIO_Open);
            m_ConnPool.SetConnType(m_Connection,
                                    CServer_ConnectionPool::eInactiveSocket);
        } else {
            // the connection pool is full
            m_Connection->OnOverflow();
            delete m_Connection;
        }
    } STD_CATCH_ALL("CAcceptRequest::Process");
}

void CAcceptRequest::Cancel(void)
{
    m_Connection->OnOverflow();
    delete m_Connection;
}

/////////////////////////////////////////////////////////////////////////////
// CServerConnectionRequest
class CServerConnectionRequest : public CServer_Request
{
public:
    CServerConnectionRequest(EIO_Event event,
               CServer_ConnectionPool& conn_pool,
               const STimeout* timeout,
               TConnection* connection) :
        CServer_Request(event, conn_pool, timeout),
        m_Connection(connection)
        { }
    virtual void Process(void);
    virtual void Cancel(void);
private:
    TConnection* m_Connection;
} ;


void CServerConnectionRequest::Process(void)
{
    try {
        m_Connection->OnSocketEvent(m_Event);
    } STD_CATCH_ALL("CServerConnectionRequest::Process");
    // Return socket to poll vector
    m_ConnPool.SetConnType(m_Connection,
                           CServer_ConnectionPool::eInactiveSocket);
}


void CServerConnectionRequest::Cancel(void)
{
    m_Connection->OnOverflow();
    // Return socket to poll vector
    m_ConnPool.SetConnType(m_Connection,
                           CServer_ConnectionPool::eInactiveSocket);
}


/////////////////////////////////////////////////////////////////////////////
// CServer_Listener
CStdRequest* TListener::CreateRequest(EIO_Event event,
                                      CServer_ConnectionPool& conn_pool,
                                      const STimeout* timeout)
{
    return new CAcceptRequest(event, conn_pool, timeout, this);
}


/////////////////////////////////////////////////////////////////////////////
// CServer_Connection
CStdRequest* TConnection::CreateRequest(EIO_Event event,
                                        CServer_ConnectionPool& conn_pool,
                                        const STimeout* timeout)
{
    // Pull out socket from poll vector
    // See CServerConnectionRequest::Process and
    // CServer_ConnectionPool::GetPollAndTimerVec
    conn_pool.SetConnType(this, CServer_ConnectionPool::eActiveSocket);
    //
    return new CServerConnectionRequest(event, conn_pool, timeout, this);
}

bool TConnection::IsOpen(void)
{
    m_Open = m_Open  &&  GetStatus(eIO_Open) == eIO_Success;
    return m_Open;
}

void TConnection::OnSocketEvent(EIO_Event event)
{
    if (event == (EIO_Event) -1) {
        m_Handler->OnTimer();
    } else if (eIO_Open == event) {
        m_Handler->OnOpen();
    } else if (eIO_Close == event) {
        m_Handler->OnClose();
        m_Open = false;
    } else {
        if (eIO_Read & event)
            m_Handler->OnRead();
        if (eIO_Write & event)
            m_Handler->OnWrite();
        // We don't need here to check GetStatus(eIO_Open), it is in IsOpen
        // method, but we need to check success of last read/write operations
        // after them.
        m_Open = m_Open
                 &&  GetStatus(eIO_Read)  == eIO_Success
                 &&  GetStatus(eIO_Write) == eIO_Success;
    }
}


/////////////////////////////////////////////////////////////////////////////
// CServer implementation

CServer::CServer(void)
{
    // TODO: auto_ptr-based initialization
    m_Parameters = new SServer_Parameters();
    m_ConnectionPool = new CServer_ConnectionPool(
        m_Parameters->max_connections);
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
        NCBI_THROW(CServer_Exception, eBadParameters,
                   "CServer::SetParameters: Bad parameters");
    }
    *m_Parameters = new_params;
    m_ConnectionPool->SetMaxConnections(m_Parameters->max_connections);
}


void CServer::GetParameters(SServer_Parameters* params)
{
    *params = *m_Parameters;
}


void CServer::StartListening(void)
{
    m_ConnectionPool->StartListening();
}


static inline bool operator <(const STimeout& to1, const STimeout& to2)
{
    return to1.sec != to2.sec ? to1.sec < to2.sec : to1.usec < to2.usec;
}


void CServer::Run(void)
{
    CStdPoolOfThreads threadPool(m_Parameters->max_threads,
                                 kMax_UInt,
                                 m_Parameters->spawn_threshold);
    threadPool.Spawn(m_Parameters->init_threads);

    StartListening();
    Init();

    vector<CSocketAPI::SPoll> polls;
    size_t                    count;
    vector<IServer_ConnectionBase*> timer_requests;
    STimeout timer_timeout;
    const STimeout* timeout;

    while (!ShutdownRequested()) {
        m_ConnectionPool->Clean();

        timeout = m_Parameters->accept_timeout;

        if (m_ConnectionPool->GetPollAndTimerVec(polls,
            timer_requests, &timer_timeout) &&
            (timeout == kDefaultTimeout ||
            timeout == kInfiniteTimeout ||
            timer_timeout < *timeout))
            timeout = &timer_timeout;

        CSocketAPI::Poll(polls, timeout, &count);

        if (count == 0) {
            if (timeout != &timer_timeout)
                ProcessTimeout();
            else {
                ITERATE (vector<IServer_ConnectionBase*>, it, timer_requests) {
                    CreateRequest(threadPool, *it, (EIO_Event) -1, timeout);
                }
            }
            continue;
        }

        ITERATE (vector<CSocketAPI::SPoll>, it, polls) {
            if (!it->m_REvent) continue;
            TConnBase* conn_base = dynamic_cast<TConnBase*>(it->m_Pollable);
            _ASSERT(conn_base);
            CreateRequest(threadPool, conn_base, it->m_REvent,
                m_Parameters->idle_timeout);
        }
    }

    // We need to kill all processing threads first, so that there
    // is no request with already destroyed connection left.
    threadPool.KillAllThreads(true);
    Exit();
    // We stop listening only here to provide port lock until application
    // cleaned up after execution.
    m_ConnectionPool->StopListening();
    // Here we finally free to erase connection pool.
    m_ConnectionPool->Erase();
}


void CServer::Init()
{
}


void CServer::Exit()
{
}


void CServer::CreateRequest(CStdPoolOfThreads& threadPool,
    IServer_ConnectionBase* conn_base,
    EIO_Event event, const STimeout* timeout)
{
    CRef<CStdRequest> request(conn_base->CreateRequest(event,
        *m_ConnectionPool, timeout));

    if (request) {
        try {
            threadPool.AcceptRequest(request);
        } catch (CBlockingQueueException&) {
            // This is impossible event, but we handle it gently
            LOG_POST(Critical << "Thread pool queue full");
            CServer_Request* req =
                dynamic_cast<CServer_Request*>(request.GetPointer());
            _ASSERT(req);
            // Queue is full, drop request, indirectly droppin incoming
            // connection (see also CAcceptRequest::Cancel)
            // ??? What should we do if conn_base is CServerConnection?
            // Should we close it? (see also CServerConnectionRequest::Cancel)
            req->Cancel();
        }
    }
}

/////////////////////////////////////////////////////////////////////////////
// SServer_Parameters implementation

static const STimeout k_DefaultIdleTimeout = { 600, 0 };

SServer_Parameters::SServer_Parameters() :
    max_connections(10000),
    temporarily_stop_listening(false),
    accept_timeout(kInfiniteTimeout),
    idle_timeout(&k_DefaultIdleTimeout),
    init_threads(5),
    max_threads(10),
    spawn_threshold(1)
{
}

const char* CServer_Exception::GetErrCodeString(void) const
{
    switch (GetErrCode()) {
    case eBadParameters: return "eBadParameters";
    case eCouldntListen: return "eCouldntListen";
    default:             return CException::GetErrCodeString();
    }
}

END_NCBI_SCOPE
