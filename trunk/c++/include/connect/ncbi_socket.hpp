#ifndef CONNECT___NCBI_SOCKET__HPP
#define CONNECT___NCBI_SOCKET__HPP

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
 *     CSocket
 *     CListeningSocket
 *     CSocketAPI
 *
 * ---------------------------------------------------------------------------
 */

#include <connect/ncbi_socket.h>
#include <corelib/ncbistd.hpp>
#include <vector>


/** @addtogroup Sockets
 *
 * @{
 */


BEGIN_NCBI_SCOPE


enum ECopyTimeout {
    eCopyTimeoutsFromSOCK,
    eCopyTimeoutsToSOCK
};


/////////////////////////////////////////////////////////////////////////////
//
//  CSocket::
//
// NOTE:  for documentation see SOCK_***() functions in "ncbi_socket.h"
//

class NCBI_XCONNECT_EXPORT CSocket
{
public:
    // Special values for timeouts as accepted by member functions below:
    static const STimeout *const kDefaultTimeout;  // use value last set
    static const STimeout *const kInfiniteTimeout; // ad infinitum
    // Initially, all timeouts are infinite.

    CSocket(void);

    // Create a client-side socket connected to "host:port".
    // NOTE 1:  the created underlying "SOCK" will be owned by the "CSocket";
    // NOTE 2:  timeout from the argument becomes new eIO_Open timeout.
    CSocket(const string&   host,
            unsigned short  port,      // always in host byte order
            const STimeout* timeout = kInfiniteTimeout,
            ESwitch         log     = eDefault);

    // Call Close(), then self-destruct
    ~CSocket(void);

    // Direction is one of
    //     eIO_Open  - return eIO_Success if CSocket is okay and open,
    //                 eIO_Closed if closed by Close() or not yet open;
    //     eIO_Read  - status of last read operation;
    //     eIO_Write - status of last write operation.
    // Direction eIO_Close and eIO_ReadWrite generate eIO_InvalidArg error.
    EIO_Status GetStatus(EIO_Event direction) const;

    // Connect to "host:port".
    // NOTE 1:  should not be called if already connected;
    // NOTE 2:  timeout from the argument becomes new eIO_Open timeout.
    EIO_Status Connect(const string&   host,
                       unsigned short  port,      // always in host byte order
                       const STimeout* timeout = kDefaultTimeout,
                       ESwitch         log     = eDefault);

    // Reconnect to the same address.
    // NOTE 1:  the socket must not be closed by the time this call is made;
    // NOTE 2:  not for the sockets created by CListeningSocket::Accept();
    // NOTE 3:  timeout from the argument becomes new eIO_Open timeout.
    EIO_Status Reconnect(const STimeout* timeout = kDefaultTimeout);

    EIO_Status Shutdown(EIO_Event how);

    // NOTE:  closes the undelying SOCK only if it is owned by this "CSocket"!
    EIO_Status Close(void);

    // NOTE:  use CSocketAPI::Poll() to wait on several sockets at once
    EIO_Status Wait(EIO_Event event, const STimeout* timeout);

    // NOTE 1:  by default, initially all timeouts are infinite;
    // NOTE 2:  SetTimeout(..., kDefaultTimeout) has no effect.
    EIO_Status      SetTimeout(EIO_Event event, const STimeout* timeout);
    const STimeout* GetTimeout(EIO_Event event) const;

    EIO_Status Read(void* buf, size_t size, size_t* n_read = 0,
                    EIO_ReadMethod how = eIO_ReadPlain);

    EIO_Status PushBack(const void* buf, size_t size);

    EIO_Status Write(const void* buf, size_t size, size_t* n_written = 0,
                     EIO_WriteMethod how = eIO_WritePersist);

    // NOTE 1:  either of "host", "port" can be NULL to opt out from
    //          obtaining the corresponding value;
    // NOTE 2:  both "*host" and "*port" come out in the same
    //          byte order requested by the third argument.
    void GetPeerAddress(unsigned int* host, unsigned short* port,
                        ENH_ByteOrder byte_order) const;

    // Specify if this "CSocket" is to own the underlying "SOCK".
    // Return previous ownership mode.
    EOwnership SetOwnership(EOwnership if_to_own);

    // Access to the underlying "SOCK" and the system-specific socket handle.
    SOCK       GetSOCK    (void) const;
    EIO_Status GetOSHandle(void* handle_buf, size_t handle_size) const;

    // NOTE:  use CSocketAPI::SetReadOnWrite() to set the default value
    void SetReadOnWrite(ESwitch read_on_write = eOn);
    // NOTE:  use CSocketAPI::SetDataLogging() to set the default value
    void SetDataLogging(ESwitch log = eOn);
    // NOTE:  use CSocketAPI::SetInterruptOnSignal() to set the default value
    void SetInterruptOnSignal(ESwitch interrupt = eOn);

    bool IsClientSide(void) const;
    bool IsServerSide(void) const;
    bool IsDatagram  (void) const;

    // Close the current underlying "SOCK" (if any, and if owned),
    // and from now on use "sock" as the underlying "SOCK" instead.
    // NOTE:  "if_to_own" applies to the (new) "sock"
    void Reset(SOCK sock, EOwnership if_to_own, ECopyTimeout whence);

protected:
    SOCK       m_Socket;
    EOwnership m_IsOwned;

private:
    // disable copy constructor and assignment
    CSocket(const CSocket&);
    CSocket& operator= (const CSocket&);

    // Timeouts
    STimeout* o_timeout;  // eIO_Open
    STimeout* r_timeout;  // eIO_Read
    STimeout* w_timeout;  // eIO_Write
    STimeout* c_timeout;  // eIO_Close
    STimeout oo_timeout;  // storage for o_timeout
    STimeout rr_timeout;  // storage for r_timeout
    STimeout ww_timeout;  // storage for w_timeout
    STimeout cc_timeout;  // storage for c_timeout
};



/////////////////////////////////////////////////////////////////////////////
//
//  CDatagramSocket::
//
//    Datagram socket
//
// NOTE:  for documentation see DSOCK_***() functions in "ncbi_socket.h"
//

class NCBI_XCONNECT_EXPORT CDatagramSocket : public CSocket
{
public:
    // NOTE:  the created underlying "SOCK" will be owned by the object
    CDatagramSocket(ESwitch do_log = eDefault);

    EIO_Status Bind(unsigned short port);

    // NOTE:  unlike system's connect() this method only specifies the default
    // destination, and does not restrict the source of the incoming messages.
    EIO_Status Connect(const string& host, unsigned short port);

    EIO_Status Wait(const STimeout* timeout = kInfiniteTimeout);

    EIO_Status Send(const void*     data    = 0,
                    size_t          datalen = 0,
                    const string&   host    = string(),
                    unsigned short  port    = 0);

    EIO_Status Recv(void*           buf         = 0,
                    size_t          buflen      = 0,
                    size_t*         msglen      = 0,
                    string*         sender_host = 0,
                    unsigned short* sender_port = 0,
                    size_t          maxmsglen   = 0);

    EIO_Status Clear(EIO_Event direction);

    EIO_Status SetBroadcast(bool do_broadcast = true);

protected:
    // NOTE: these calls are not valid with datagram sockets
    EIO_Status Shutdown(EIO_Event how);
    EIO_Status Reconnect(const STimeout* timeout);

private:
    // disable copy constructor and assignment
    CDatagramSocket(const CDatagramSocket&);
    CDatagramSocket& operator= (const CDatagramSocket&);
};



/////////////////////////////////////////////////////////////////////////////
//
//  CListeningSocket::
//
//    Listening socket (to accept connections on the server side)
//
// NOTE:  for documentation see LSOCK_***() functions in "ncbi_socket.h"
//

class NCBI_XCONNECT_EXPORT CListeningSocket
{
public:
    static const STimeout *const kInfiniteTimeout; // ad infinitum

    CListeningSocket(void);
    // NOTE:  "port" ought to be in host byte order
    CListeningSocket(unsigned short port, unsigned short backlog = 5);

    // Call Close(), then self-destruct
    ~CListeningSocket(void);

    // Return eIO_Success if CListeningSocket is opened and bound;
    // Return eIO_Closed if not yet bound or Close()'d.
    EIO_Status GetStatus(void) const;

    // NOTE:  "port" ought to be in host byte order
    EIO_Status Listen(unsigned short port, unsigned short backlog = 5);

    // NOTE: the created "CSocket" will own its underlying "SOCK"
    EIO_Status Accept(CSocket*& sock,
                      const STimeout* timeout = kInfiniteTimeout) const;
    EIO_Status Accept(CSocket&  sock,
                      const STimeout* timeout = kInfiniteTimeout) const;

    // NOTE:  closes the undelying SOCK only if it is owned by this "CSocket"!
    EIO_Status Close(void);

    // Specify if this "CListeningSocket" is to own the underlying "LSOCK".
    // Return previous ownership mode.
    EOwnership SetOwnership(EOwnership if_to_own);

    // Access to the underlying "LSOCK" and the system-specific socket handle
    LSOCK      GetLSOCK    (void) const;
    EIO_Status GetOSHandle (void* handle_buf, size_t handle_size) const;

private:
    LSOCK      m_Socket;
    EOwnership m_IsOwned;

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

class NCBI_XCONNECT_EXPORT CSocketAPI
{
public:
    // Generic
    static void       AllowSigPipe   (void);
    static EIO_Status Initialize     (void);
    static EIO_Status Shutdown       (void);

    // Defaults  (see also per-socket CSocket::SetReadOnWrite, etc.)
    static void SetReadOnWrite       (ESwitch read_on_write);
    static void SetDataLogging       (ESwitch log);
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
    static string       gethostname  (void);                // empty str on err
    static string       ntoa         (unsigned int  host);
    static string       gethostbyaddr(unsigned int  host);  // empty str on err
    static unsigned int gethostbyname(const string& hostname);  // 0 on error

    static unsigned int HostToNetLong (unsigned int   value);
    static unsigned int NetToHostLong (unsigned int   value);
    static unsigned int HostToNetShort(unsigned short value);
    static unsigned int NetToHostShort(unsigned short value);
};


/* @} */


/////////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////////
//  IMPLEMENTATION of INLINE functions
/////////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////////
//  CSocket::
//

inline EIO_Status CSocket::Shutdown(EIO_Event how)
{
    return m_Socket ? SOCK_Shutdown(m_Socket, how) : eIO_Closed;
}


inline EIO_Status CSocket::Wait(EIO_Event event, const STimeout* timeout)
{
    return m_Socket ? SOCK_Wait(m_Socket, event, timeout) : eIO_Closed; 
}


inline EIO_Status CSocket::PushBack(const void* buf, size_t size)
{
    return m_Socket ? SOCK_PushBack(m_Socket, buf, size) : eIO_Closed;
}


inline EIO_Status CSocket::GetStatus(EIO_Event direction) const
{
    return direction == eIO_Open ? (m_Socket ? eIO_Success : eIO_Closed)
        : (m_Socket ? SOCK_Status(m_Socket, direction) : eIO_Closed);
}


inline EOwnership CSocket::SetOwnership(EOwnership if_to_own)
{
    EOwnership prev_ownership = m_IsOwned;
    m_IsOwned                 = if_to_own;
    return prev_ownership;
}


inline SOCK CSocket::GetSOCK(void) const
{
    return m_Socket;
}


inline EIO_Status CSocket::GetOSHandle(void* hnd_buf, size_t hnd_siz) const
{
    return m_Socket? SOCK_GetOSHandle(m_Socket, hnd_buf, hnd_siz) : eIO_Closed;
}

 
inline void CSocket::SetReadOnWrite(ESwitch read_on_write)
{
    if ( m_Socket )
        SOCK_SetReadOnWrite(m_Socket, read_on_write);
}


inline void CSocket::SetDataLogging(ESwitch log)
{
    if ( m_Socket )
        SOCK_SetDataLogging(m_Socket, log);
}


inline void CSocket::SetInterruptOnSignal(ESwitch interrupt)
{
    if ( m_Socket )
        SOCK_SetInterruptOnSignal(m_Socket, interrupt);
}


inline bool CSocket::IsClientSide(void) const
{
    return m_Socket && SOCK_IsClientSide(m_Socket) ? true : false;
}


inline bool CSocket::IsServerSide(void) const
{
    return m_Socket && SOCK_IsServerSide(m_Socket) ? true : false;
}


inline bool CSocket::IsDatagram(void) const
{
    return m_Socket && SOCK_IsDatagram(m_Socket) ? true : false;
}



/////////////////////////////////////////////////////////////////////////////
//  CDatagramSocket::
//

inline EIO_Status CDatagramSocket::Wait(const STimeout* timeout)
{
    return m_Socket ? DSOCK_WaitMsg(m_Socket, timeout) : eIO_Closed;
}


inline EIO_Status CDatagramSocket::Clear(EIO_Event direction)
{
    return m_Socket ? DSOCK_WipeMsg(m_Socket, direction) : eIO_Closed;
}


inline EIO_Status CDatagramSocket::SetBroadcast(bool do_broadcast)
{
    return m_Socket ? DSOCK_SetBroadcast(m_Socket, do_broadcast) : eIO_Closed;
}



/////////////////////////////////////////////////////////////////////////////
//  CListeningSocket::
//

inline EIO_Status CListeningSocket::GetStatus(void) const
{
    return m_Socket ? eIO_Success : eIO_Closed;
}


inline EOwnership CListeningSocket::SetOwnership(EOwnership if_to_own)
{
    EOwnership prev_ownership = m_IsOwned;
    m_IsOwned                 = if_to_own;
    return prev_ownership;
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


inline void CSocketAPI::SetDataLogging(ESwitch log)
{
    SOCK_SetDataLoggingAPI(log);
}


inline void CSocketAPI::SetInterruptOnSignal(ESwitch interrupt)
{
    SOCK_SetInterruptOnSignalAPI(interrupt);
}


inline unsigned int CSocketAPI::HostToNetLong(unsigned int value)
{
    return SOCK_HostToNetLong(value);
}


inline unsigned int CSocketAPI::NetToHostLong(unsigned int value)
{
    return SOCK_NetToHostLong(value);
}


inline unsigned int CSocketAPI::HostToNetShort(unsigned short value)
{
    return SOCK_HostToNetShort(value);
}


inline unsigned int CSocketAPI::NetToHostShort(unsigned short value)
{
    return SOCK_NetToHostShort(value);
}


/////////////////////////////////////////////////////////////////////////////


END_NCBI_SCOPE


/*
 * ---------------------------------------------------------------------------
 * $Log$
 * Revision 6.24  2003/05/14 03:46:44  lavr
 * Match revised datagram socket API
 *
 * Revision 6.23  2003/04/30 17:03:22  lavr
 * Modified prototypes for CDatagramSocket::Send() and CDatagramSocket::Recv()
 *
 * Revision 6.22  2003/04/11 20:58:12  lavr
 * CDatagramSocket:: API defined completely
 *
 * Revision 6.21  2003/04/09 19:05:55  siyan
 * Added doxygen support
 *
 * Revision 6.20  2003/02/20 17:55:09  lavr
 * Inlining CSocket::Shutdown() and CSocket::Wait()
 *
 * Revision 6.19  2003/02/14 22:03:32  lavr
 * Add internal CSocket timeouts and document them
 *
 * Revision 6.18  2003/01/28 19:29:07  lavr
 * DSOCK: Remove reference to kEmptyStr and thus to xncbi
 *
 * Revision 6.17  2003/01/26 04:28:37  lavr
 * Quick fix CDatagramSocket ctor ambiguity
 *
 * Revision 6.16  2003/01/24 23:01:32  lavr
 * Added class CDatagramSocket
 *
 * Revision 6.15  2003/01/07 22:01:43  lavr
 * ChangeLog message corrected
 *
 * Revision 6.14  2003/01/07 21:58:24  lavr
 * Comment corrected
 *
 * Revision 6.13  2002/12/19 14:51:48  dicuccio
 * Added export specifier for Win32 DLL builds.
 *
 * Revision 6.12  2002/12/04 16:54:08  lavr
 * Add extra parameter "log" to CSocket() constructor and CSocket::Connect()
 *
 * Revision 6.11  2002/11/14 01:11:33  lavr
 * Minor formatting changes
 *
 * Revision 6.10  2002/09/19 18:05:25  lavr
 * Header file guard macro changed
 *
 * Revision 6.9  2002/09/17 20:45:28  lavr
 * A typo corrected
 *
 * Revision 6.8  2002/09/16 22:32:47  vakatov
 * Allow to change ownership for the underlying sockets "on-the-fly";
 * plus some minor (mostly formal) code and comments rearrangements
 *
 * Revision 6.7  2002/08/27 03:19:29  lavr
 * CSocketAPI:: Removed methods: htonl(), htons(), ntohl(), ntohs(). Added
 * as replacements: {Host|Net}To{Net|Host}{Long|Short}() (see ncbi_socket.h)
 *
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

#endif /* CONNECT___NCBI_SOCKET__HPP */
