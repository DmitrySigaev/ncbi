#ifndef NCBI_SOCKET__HPP
#define NCBI_SOCKET__HPP

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
 * Author:  Denis Vakatov, Anton Lavrentiev
 *
 * File Description:
 *   C++ wrapper for the C "SOCK" API (UNIX, MS-Win, MacOS, Darwin)
 *   NOTE:  for more details and documentation see "ncbi_socket.h"
 *     CSocketAPI
 *     CSocket
 *     CListeningSocket
 *
 * ---------------------------------------------------------------------------
 */

#include <connect/ncbi_socket.h>
#include <corelib/ncbistd.hpp>
#include <vector>

#ifdef NCBI_OS_BSD
/* These fast macros mess up class defs below; make them real calls instead */
#  ifdef ntohl
#    undef ntohl
#  endif
#  ifdef ntohs
#    undef ntohs
#  endif
#  ifdef htonl
#    undef htonl
#  endif
#  ifdef htons
#    undef htons
#  endif
#endif


BEGIN_NCBI_SCOPE


/////////////////////////////////////////////////////////////////////////////
//
//  CSocket::
//
//    Listening socket (to accept connections on the server side)
//
// NOTE:  for documentation see SOCK_***() functions in "ncbi_socket.h"
//

class CSocket
{
public:
    CSocket(void);

    // Create a client-side socket connected to "host:port"
    CSocket(const string&   host,
            unsigned short  port,  // always in host byte order
            const STimeout* timeout = 0);

    // Call Close() and destruct
    ~CSocket(void);

    // Direction is one of
    //     eIO_Open  - return eIO_Success if CSocket is okay and open,
    //                 eIO_Closed if closed by Close() or not yet open;
    //     eIO_Read  - status of last read operation;
    //     eIO_Write - status of last write operation.
    // Direction eIO_Close and eIO_ReadWrite generate eIO_InvalidArg error.
    EIO_Status GetStatus(EIO_Event direction) const;

    // Connect to "host:port".
    // NOTE:  should not be called if already connected.
    EIO_Status Connect(const string&   host,
                       unsigned short  port,  // always in host byte order
                       const STimeout* timeout = 0);

    // Reconnect to the same address.
    // NOTE 1:  the socket must not be closed by the time this call is made.
    // NOTE 2:  not for the sockets created by CListeningSocket::Accept().
    EIO_Status Reconnect(const STimeout* timeout = 0);

    EIO_Status Shutdown(EIO_Event how);

    // NOTE:  do not actually close the undelying SOCK if it is not owned
    EIO_Status Close(void);

    // NOTE:  use CSocketAPI::Poll() to wait on several sockets at once
    EIO_Status Wait(EIO_Event event, const STimeout* timeout);

    EIO_Status      SetTimeout(EIO_Event event, const STimeout* timeout);
    const STimeout* GetTimeout(EIO_Event event) const;

    EIO_Status Read(void* buf, size_t size, size_t* n_read = 0,
                    EIO_ReadMethod how = eIO_ReadPlain);

    EIO_Status PushBack(const void* buf, size_t size);

    EIO_Status Write(const void* buf, size_t size, size_t* n_written = 0,
                     EIO_WriteMethod how = eIO_WritePersist);

    // NOTE 1:  either of "host", "port" can be NULL to opt out from storing
    //          obtaining the corresponding value;
    // NOTE 2:  both "*host" and "*port" come out in the same
    //          byte order requested by the third argument.
    void GetPeerAddress(unsigned int* host, unsigned short* port,
                        ENH_ByteOrder byte_order) const;

    SOCK       GetSOCK     (void) const;
    EIO_Status GetOSHandle (void* handle_buf, size_t handle_size) const;

    // use CSocketAPI::SetReadOnWrite() to set the default value
    void SetReadOnWrite(ESwitch read_on_write = eOn);
    // use CSocketAPI::SetDataLogging() to set the default value
    void SetDataLogging(ESwitch log_data = eOn);
    // use CSocketAPI::SetInterruptOnSignal() to set the default value
    void SetInterruptOnSignal(ESwitch interrupt = eOn);

    bool IsServerSide(void) const;

    // old SOCK (if any) will be closed; "if_to_own" passed as eTakeOwnership
    // causes "sock" to close upon Close() call or destruction of the object.
    void Reset(SOCK sock, EOwnership if_to_own);

private:
    SOCK       m_Socket;
    EOwnership m_IsOwned;

    // disable copy constructor and assignment
    CSocket(const CSocket&);
    CSocket& operator= (const CSocket&);
};



/////////////////////////////////////////////////////////////////////////////
//
//  CListeningSocket::
//
//    Listening socket (to accept connections on the server side)
//
// NOTE:  for documentation see LSOCK_***() functions in "ncbi_socket.h"
//

class CListeningSocket
{
public:
    CListeningSocket(void);
    // NOTE:  "port" ought to be in host byte order
    CListeningSocket(unsigned short port, unsigned short backlog = 5);
    ~CListeningSocket(void);

    // Return eIO_Success if CListeningSocket is opened and bound;
    // Return eIO_Closed if not yet bound or Close()'d.
    EIO_Status GetStatus(void) const;

    // NOTE:  "port" ought to be in host byte order
    EIO_Status Listen(unsigned short port, unsigned short backlog = 5);
    EIO_Status Accept(CSocket*& sock, const STimeout* timeout = 0) const;
    EIO_Status Accept(CSocket&  sock, const STimeout* timeout = 0) const;
    EIO_Status Close(void);

    LSOCK      GetLSOCK    (void) const;
    EIO_Status GetOSHandle (void* handle_buf, size_t handle_size) const;

private:
    LSOCK m_Socket;

    // disable copy constructor and assignment
    CListeningSocket(const CListeningSocket&);
    CListeningSocket& operator= (const CListeningSocket&);
};



/////////////////////////////////////////////////////////////////////////////
//
//  CSocketAPI::
//
//    Global settings related to the socket API
//
// NOTE:  for documentation see SOCK_***() functions in "ncbi_socket.h"
//

class CSocketAPI
{
public:
    // Generic
    static void       AllowSigPipe   (void);
    static EIO_Status Initialize     (void);
    static EIO_Status Shutdown       (void);

    // Defaults  (see also per-socket CSocket::SetReadOnWrite, etc.)
    static void SetReadOnWrite       (ESwitch read_on_write);
    static void SetDataLogging       (ESwitch log_data);
    static void SetInterruptOnSignal (ESwitch interrupt);

    // NOTE:  use CSocket::Wait() to wait for I/O event(s) on a single socket
    struct SPoll {
        SPoll(CSocket& sock, EIO_Event event, EIO_Event revent)
            : m_Socket(sock), m_Event(event), m_REvent(revent) {}
        CSocket&  m_Socket;
        EIO_Event m_Event;
        EIO_Event m_REvent;        
    };
    static EIO_Status Poll(vector<SPoll>&  polls,
                           const STimeout* timeout,
                           size_t*         n_ready = 0);

    // Misc  (mostly BSD-like); "host" ought to be in network byte order
    static string gethostname  (void);               // empty string on error
    static string ntoa         (unsigned int host);
    static string gethostbyaddr(unsigned int host);  // empty string on error
    static unsigned int gethostbyname(const string& hostname);  // 0 on error

    static unsigned int htonl(unsigned int   value);  // host-to-net, long
    static unsigned int ntohl(unsigned int   value);  // net-to-host, long
    static unsigned int htons(unsigned short value);  // host-to-net, short
    static unsigned int ntohs(unsigned short value);  // net-to-host, short
};


/////////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////////
//  IMPLEMENTATION of INLINE functions
/////////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////////
//  CSocket::
//

inline EIO_Status CSocket::GetStatus(EIO_Event direction) const
{
    return direction == eIO_Open ? (m_Socket ? eIO_Success : eIO_Closed)
        : (m_Socket ? SOCK_Status(m_Socket, direction) : eIO_Closed);
}


inline EIO_Status CSocket::SetTimeout(EIO_Event event, const STimeout* timeout)
{
    return m_Socket ? SOCK_SetTimeout(m_Socket, event, timeout) : eIO_Closed;
}


inline const STimeout* CSocket::GetTimeout(EIO_Event event) const
{
    return m_Socket ? SOCK_GetTimeout(m_Socket, event) : 0;
}


inline SOCK CSocket::GetSOCK(void) const
{
    return m_Socket;
}


inline EIO_Status CSocket::GetOSHandle(void* handle_buf, size_t handle_size) const
{
    return m_Socket
        ? SOCK_GetOSHandle(m_Socket, handle_buf, handle_size) : eIO_Closed;
}

 
inline void CSocket::SetReadOnWrite(ESwitch read_on_write)
{
    if ( m_Socket )
        SOCK_SetReadOnWrite(m_Socket, read_on_write);
}


inline void CSocket::SetDataLogging(ESwitch log_data)
{
    if ( m_Socket )
        SOCK_SetDataLogging(m_Socket, log_data);
}


inline void CSocket::SetInterruptOnSignal(ESwitch interrupt)
{
    if ( m_Socket )
        SOCK_SetInterruptOnSignal(m_Socket, interrupt);
}


inline bool CSocket::IsServerSide(void) const
{
    return m_Socket && SOCK_IsServerSide(m_Socket) ? true : false;
}



/////////////////////////////////////////////////////////////////////////////
//  CListeningSocket::
//

inline EIO_Status CListeningSocket::GetStatus(void) const
{
    return m_Socket ? eIO_Success : eIO_Closed;
}


inline LSOCK CListeningSocket::GetLSOCK(void) const
{
    return m_Socket;
}


inline EIO_Status CListeningSocket::GetOSHandle(void*  handle_buf,
                                         size_t handle_size) const
{
    return m_Socket
        ? LSOCK_GetOSHandle(m_Socket, handle_buf, handle_size) : eIO_Closed;
}



/////////////////////////////////////////////////////////////////////////////
//  CSocketAPI::
//

inline void CSocketAPI::AllowSigPipe(void)
{
    SOCK_AllowSigPipeAPI();
}


inline EIO_Status CSocketAPI::Initialize(void)
{
    return SOCK_InitializeAPI();
}


inline EIO_Status CSocketAPI::Shutdown(void)
{
    return SOCK_ShutdownAPI();
}


inline void CSocketAPI::SetReadOnWrite(ESwitch read_on_write)
{
    SOCK_SetReadOnWriteAPI(read_on_write);
}


inline void CSocketAPI::SetDataLogging(ESwitch log_data)
{
    SOCK_SetDataLoggingAPI(log_data);
}


inline void CSocketAPI::SetInterruptOnSignal(ESwitch interrupt)
{
    SOCK_SetInterruptOnSignalAPI(interrupt);
}


inline unsigned int CSocketAPI::htonl(unsigned int value)
{
    return SOCK_htonl(value);
}


inline unsigned int CSocketAPI::ntohl(unsigned int value)
{
    return SOCK_ntohl(value);
}


inline unsigned int CSocketAPI::htons(unsigned short value)
{
    return SOCK_htons(value);
}


inline unsigned int CSocketAPI::ntohs(unsigned short value)
{
    return SOCK_ntohs(value);
}


/////////////////////////////////////////////////////////////////////////////


END_NCBI_SCOPE


/*
 * ---------------------------------------------------------------------------
 * $Log$
 * Revision 6.6  2002/08/15 18:45:03  lavr
 * CSocketAPI::Poll() documented in more details in ncbi_socket.h(SOCK_Poll)
 *
 * Revision 6.5  2002/08/14 15:04:37  sadykov
 * Prepend "inline" for GetOSHandle() method impl
 *
 * Revision 6.4  2002/08/13 19:28:29  lavr
 * Move most methods out-of-line; fix inline methods to have "inline"
 *
 * Revision 6.3  2002/08/12 20:58:09  lavr
 * More comments on parameters usage of certain methods
 *
 * Revision 6.2  2002/08/12 20:24:04  lavr
 * Resolve expansion mess with byte order marcos (use calls instead) -- FreeBSD
 *
 * Revision 6.1  2002/08/12 15:11:34  lavr
 * Initial revision
 *
 * ===========================================================================
 */

#endif /* NCBI_SOCKET__HPP */
