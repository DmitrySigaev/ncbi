#ifndef CONNECT___NCBI_SOCKET__H
#define CONNECT___NCBI_SOCKET__H

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
 * Author:  Denis Vakatov
 *
 * File Description:
 *   Plain portable TCP/IP socket API for:  UNIX, MS-Win, MacOS
 *   Platform-specific library requirements:
 *     [UNIX ]   -DNCBI_OS_UNIX     -lresolv -lsocket -lnsl
 *     [MSWIN]   -DNCBI_OS_MSWIN    wsock32.lib
 *     [MacOS]   -DNCBI_OS_MAC      NCSASOCK -- BSD-style socket emulation lib
 *
 *********************************
 * Generic:
 *
 *  SOCK, LSOCK
 *
 *  SOCK_AllowSigPipeAPI
 *
 *  SOCK_InitializeAPI
 *  SOCK_ShutdownAPI
 *
 * Listening socket (handle LSOCK):
 *
 *  LSOCK_Create
 *  LSOCK_Accept
 *  LSOCK_Close
 *  
 * I/O Socket (handle SOCK):
 *
 *  SOCK_Create (see also LSOCK_Accept)
 *  SOCK_Reconnect
 *  SOCK_Shutdown
 *  SOCK_Close
 *  SOCK_Wait
 *  SOCK_Poll
 *  SOCK_SetTimeout
 *  SOCK_GetReadTimeout
 *  SOCK_GetWriteTimeout
 *  SOCK_Read (including "peek" and "persistent read")
 *  SOCK_PushBack
 *  SOCK_Status
 *  SOCK_Write
 *  SOCK_GetPeerAddress
 *
 *  SOCK_SetReadOnWriteAPI
 *  SOCK_SetReadOnWrite
 *  SOCK_SetInterruptOnSignalAPI
 *  SOCK_SetInterruptOnSignal
 *
 *  SOCK_IsServerSide
 *
 * Data logging:
 *
 *  SOCK_SetDataLoggingAPI
 *  SOCK_SetDataLogging
 *
 * Auxiliary:
 *
 *  SOCK_gethostname
 *  SOCK_ntoa
 *  SOCK_HostToNetShort
 *  SOCK_HostToNetLong
 *  SOCK_NetToHostShort
 *  SOCK_NetToHostLong
 *
 */

#if defined(NCBISOCK__H)
#  error "<ncbisock.h> and <ncbi_socket.h> must never be #include'd together"
#endif

#include <connect/ncbi_core.h>


#ifdef __cplusplus
extern "C" {
#endif


/******************************************************************************
 *  TYPEDEFS & MACROS
 */


/* Network and host byte order enumeration type
 */
typedef enum {
    eNH_HostByteOrder,
    eNH_NetworkByteOrder
} ENH_ByteOrder;


/* Forward declarations of the hidden socket internal structure, and
 * their upper-level handles to use by the LSOCK_*() and SOCK_*() API
 */
struct LSOCK_tag;                /* listening socket:  internal storage  */
typedef struct LSOCK_tag* LSOCK; /* listening socket:  handle */

struct SOCK_tag;                 /* socket:  internal storage  */
typedef struct SOCK_tag*  SOCK;  /* socket:  handle */



/******************************************************************************
 *  Multi-Thread safety
 *
 * If you are using this API in a multi-thread application, and there is
 * more than one thread using this API, it is safe to call SOCK_InitializeAPI()
 * explicitly in the beginning of your main thread, before you run any other
 * threads, and to call SOCK_ShutdownAPI() after all threads are exited.
 *
 * As soon as the API is initialized it becomes relatively MT-safe, however
 * you still must not operate with the same LSOCK or SOCK objects from
 * different threads simultaneously.
 *
 * A MUCH BETTER WAY of dealing with this issue is to provide your own MT
 * locking callback (see CORE_SetLOCK in "ncbi_core.h"). This will also
 * guarantee the proper MT protection should some other SOCK functions
 * start to access any static data in the future.
 */



/******************************************************************************
 *   Error Logging
 *
 * NOTE:  Use CORE_SetLOG() from "ncbi_core.h" to setup the log handler.
 */


/* By default ("log_data" == eDefault,eOff), the data is not logged.
 * To start logging the data, call this func with "log_data" == eOn.
 * To stop  logging the data, call this func with "log_data" == eOff.
 */
extern void SOCK_SetDataLoggingAPI(ESwitch log_data);


/* Control the data logging for socket "sock" individually.
 * To reset to the global default behavior (as set by SOCK_SetDataLoggingAPI),
 * call this function with "log_data" == eDefault.
 */
extern void SOCK_SetDataLogging(SOCK sock, ESwitch log_data);



/******************************************************************************
 *   I/O restart on signals
 */


/* By default ("on_off" == eDefault,eOff), I/O is restartable if interrupted.
 */
extern void SOCK_SetInterruptOnSignalAPI(ESwitch on_off);


/* Control sockets individually. eDefault causes the use of global API flag.
 */
extern void SOCK_SetInterruptOnSignal(SOCK sock, ESwitch on_off);



/******************************************************************************
 *  API Initialization and Shutdown/Cleanup
 */


/* By default (on UNIX platforms) the SOCK API functions automagically call
 * "signal(SIGPIPE, SIG_IGN)" on initialization.  To prohibit this feature,
 * you must call SOCK_AllowSigPipeAPI() before you call any other
 * function from the SOCK API.
 */
extern void SOCK_AllowSigPipeAPI(void);


/* Initialize all internal/system data & resources to be used by the SOCK API.
 * NOTE:
 *  You can safely call it more than once; just, all calls after the first
 *  one will have no result. 
 * NOTE:
 *  Usually, SOCK API does not require an explicit initialization -- as it is
 *  guaranteed to initialize itself automagically, in one of API functions,
 *  when necessary. Yet, see the "Multi Thread safety" remark above.
 */
extern EIO_Status SOCK_InitializeAPI(void);


/* Cleanup; destroy all internal/system data & resources used by the SOCK API.
 * ATTENTION:  no function from the SOCK API should be called after this call!
 * NOTE: you can safely call it more than once; just, all calls after the first
 *       one will have no result. 
 */
extern EIO_Status SOCK_ShutdownAPI(void);



/******************************************************************************
 *  LISTENING SOCKET
 */


/* [SERVER-side]  Create and initialize the server-side(listening) socket
 * (socket() + bind() + listen())
 * NOTE: on some systems, "backlog" can be silently limited down to 128 (or 5).
 */
extern EIO_Status LSOCK_Create
(unsigned short port,    /* [in]  the port to listen at                  */
 unsigned short backlog, /* [in]  maximal # of pending connections       */
 LSOCK*         lsock    /* [out] handle of the created listening socket */
 );


/* [SERVER-side]  Accept connection from a client.
 * NOTE: the "*timeout" is for this accept() only.  To set I/O timeout,
 *       use SOCK_SetTimeout();  all I/O timeouts are infinite by default.
 */
extern EIO_Status LSOCK_Accept
(LSOCK           lsock,    /* [in]  handle of a listening socket */
 const STimeout* timeout,  /* [in]  timeout (infinite if NULL)   */
 SOCK*           sock      /* [out] handle of the created socket */
 );


/* [SERVER-side]  Close the listening socket, destroy relevant internal data.
 */
extern EIO_Status LSOCK_Close(LSOCK lsock);


/* Get an OS-dependent native socket handle to use by platform-specific API.
 * FYI:  on MS-Windows it will be "SOCKET", on other platforms -- "int".
 */
extern EIO_Status LSOCK_GetOSHandle
(LSOCK  lsock,
 void*  handle_buf,  /* pointer to a memory area to put the OS handle at */
 size_t handle_size  /* the exact(!) size of the expected OS handle */
 );



/******************************************************************************
 *  SOCKET
 */


/* [CLIENT-side]  Connect client to another(server-side, listening) socket
 * (socket() + connect() [+ select()])
 */
extern EIO_Status SOCK_Create
(const char*     host,    /* [in]  host to connect to                     */
 unsigned short  port,    /* [in]  port to connect to                     */
 const STimeout* timeout, /* [in]  the connect timeout (infinite if NULL) */
 SOCK*           sock     /* [out] handle of the created socket           */
 );


/* [CLIENT-side]  Close the socket referred by "sock" and then connect
 * it to another "host:port";  fail if it takes more than "timeout".
 * (close() + connect() [+ select()])
 *
 * HINT: if "host" is NULL then connect to the same host address as before;
 *       if "port" is zero then connect to the same port # as before.
 * NOTE: "new" socket inherits the old i/o timeouts,
 */
extern EIO_Status SOCK_Reconnect
(SOCK            sock,    /* [in] handle of the socket to reconnect      */
 const char*     host,    /* [in] host to connect to  (can be NULL)      */
 unsigned short  port,    /* [in] port to connect to  (can be 0)         */
 const STimeout* timeout  /* [in] the connect timeout (infinite if NULL) */
 );


/* Shutdown the connection in only one direction (specified by "direction").
 * Later attempts to I/O (or to wait) in the shutdown direction will
 * do nothing, and immediately return with "eIO_Closed" status.
 */
extern EIO_Status SOCK_Shutdown
(SOCK      sock,
 EIO_Event how   /* [in] one of:  eIO_Read, eIO_Write, eIO_ReadWrite */
 );


/* Close the connection, destroy relevant internal data.
 * The "sock" handle goes invalid after this function call, regardless
 * of whether the call was successful or not.
 * NOTE: if eIO_Close timeout was specified then it blocks until
 *       either all unsent data are sent or the timeout expires.
 */
extern EIO_Status SOCK_Close(SOCK sock);


/* Block on the socket until either read/write (dep. on the "event" arg) is
 * available or timeout expires (if "timeout" is NULL then assume it infinite).
 */
extern EIO_Status SOCK_Wait
(SOCK            sock,
 EIO_Event       event,  /* [in] one of:  eIO_Read, eIO_Write, eIO_ReadWrite */
 const STimeout* timeout
 );


/* Block until at least one of the sockets enlisted in "polls" array
 * (of size "n") becomes available for requested operation (event),
 * or until timeout expires (wait indefinitely if timeout is passed NULL).
 * Return eIO_Success if at least one socket was found ready; eIO_Timeout
 * if timeout expired; eIO_Unknown if underlying system call(s) failed.
 * NOTE1: for a socket found not ready for an operation, eIO_Open is returned
 *        in its "revent"; for a failing socket, eIO_Close is returned;
 * NOTE2: this call can return eIO_InvalidArg if a non-NULL socket polled with
 *        a bad "event" (like eIO_Open or eIO_Close);
 * NOTE3: if either both "n" and "polls" are NULL, or all sockets in "polls"
 *        are NULL, then the returned result is either
 *        eIO_Timeout (after the specified amount of time was spent idle), or
 *        eIO_Interrupted (if signal came while the waiting was in progress).
 */

typedef struct {
    SOCK      sock;   /* [in]           SOCK to poll (NULL if not to poll)   */
    EIO_Event event;  /* [in]  one of:  eIO_Read, eIO_Write, eIO_ReadWrite   */
    EIO_Event revent; /* [out] one of:  eIO_Open/Read/Write/ReadWrite/Close  */
} SSOCK_Poll;

extern EIO_Status SOCK_Poll
(size_t          n,         /* [in]      # of SSOCK_Poll elems in "polls"    */
 SSOCK_Poll      polls[],   /* [in|out]  array of query/result structures    */
 const STimeout* timeout,   /* [in]      max time to wait (infinite if NULL) */
 size_t*         n_ready    /* [out]     # of ready sockets  (can be NULL)   */
 );


/* Specify timeout for the connection i/o (see SOCK_[Read|Write|Close] funcs).
 * If "timeout" is NULL then set the timeout to be infinite;
 * NOTE: the default timeout is infinite (wait "ad infinitum" on i/o).
 */
extern EIO_Status SOCK_SetTimeout
(SOCK            sock,
 EIO_Event       event,   /* [in]  one of:  eIO_[Read/Write/ReadWrite/Close] */
 const STimeout* timeout  /* [in]  new timeout value to set                  */
 );


/* Get the connection's i/o timeout (or NULL, if the timeout is infinite).
 * NOTE:  the returned timeout is guaranteed to be pointing to a valid
 *        (and correct) structure in memory at least until the SOCK is closed
 *        or SOCK_SetTimeout is called for this "sock".
 */
extern const STimeout* SOCK_GetTimeout
(SOCK      sock,
 EIO_Event event  /* [in]  one of:  eIO_[Read/Write/Close] */
 );


/* Read up to "size" bytes from "sock" to the mem.buffer pointed by "buf".
 * In "*n_read", return the number of successfully read bytes.
 * If there is no data available to read (also, if eIO_ReadPersist and cannot
 * read exactly "size" bytes) and the timeout(see SOCK_SetTimeout) is expired
 * then return eIO_Timeout.
 * If "buf" is passed NULL, then:
 *   1) if PEEK -- read up to "size" bytes and store them in internal buffer;
 *   2) else -- discard up to "size" bytes from internal buffer and socket.
 * NOTE1: Theoretically, eIO_Closed may indicate an empty message
 *        rather than a real closure of the connection...
 * NOTE2: If on input "size" == 0, then "*n_read" is set to 0, and
 *        return value can be either of eIO_Success, eIO_Closed or
 *        eIO_Unknown depending on connection status of the socket.
 */
extern EIO_Status SOCK_Read
(SOCK           sock,
 void*          buf,    /* [out] data buffer to read to          */
 size_t         size,   /* [in]  max # of bytes to read to "buf" */
 size_t*        n_read, /* [out] # of bytes read  (can be NULL)  */
 EIO_ReadMethod how     /* [in]  how to read the data            */
 );


/* Push the specified data back to the socket input queue (in the socket's
 * internal read buffer). These can be any data, not necessarily the data
 * previously read from the socket.
 */
extern EIO_Status SOCK_PushBack
(SOCK        sock,
 const void* buf,  /* [in] data to push back to the socket's local buffer */
 size_t      size  /* [in] # of bytes (starting at "buf") to pushback     */
 );


/* Return (for the specified "direction"):
 *   eIO_Closed     -- if the connection was shutdown by SOCK_Shutdown(), or
 *                     (for "eIO_Read" only) if EOF was detected
 *   eIO_Unknown    -- if an error was detected during the last I/O
 *   eIO_InvalidArg -- if "direction" is not one of:  eIO_Read, eIO_Write
 *   eIO_Success    -- otherwise (incl. eIO_Timeout on last I/O)
 *
 * NOTE:  The SOCK_Read() and SOCK_Wait(eIO_Read) will not return any error
 *        as long as there is any unread (buffered) data left.
 *        Thus, when you are "peeking" data instead of actually reading it,
 *        then this is the only "non-destructive" way to check whether EOF
 *        or an error has occurred on read.
 */
extern EIO_Status SOCK_Status
(SOCK      sock,
 EIO_Event direction  /* [in] one of:  eIO_Read, eIO_Write */
 );


/* Write "size" bytes from buffer "buf" to "sock".
 * In "*n_written", return the number of bytes actually written.
 * eIO_WritePlain   --  write as many bytes as possible at once and return
 *                      immediately; if no bytes can be written then wait
 *                      at most WRITE timeout, try again and return.
 * eIO_WritePersist --  write all data (doing an internal retry loop
 *                      if necessary); if any single write attempt times out
 *                      or fails then stop writing and return (error code).
 * Return status: eIO_Success -- some bytes were written successfully  [Plain]
 *                            -- all bytes were written successfully [Persist]
 *                other code denotes an error, but some bytes might have
 *                been sent (check *n_written to know).
 */
extern EIO_Status SOCK_Write
(SOCK            sock,
 const void*     buf,       /* [in]  data to write to the socket             */
 size_t          size,      /* [in]  # of bytes (starting at "buf") to write */
 size_t*         n_written, /* [out] # of written bytes (can be NULL)        */
 EIO_WriteMethod how        /* [in]  eIO_WritePlain | eIO_WritePersist       */
 );


/* Get host and port of the socket's peer.
 * If "network_byte_order" is true(non-zero) then return the host/port in the
 * network byte order; otherwise return them in the local host byte order.
 */
extern void SOCK_GetPeerAddress
(SOCK            sock,
 unsigned int*   host,               /* [out] the peer's host (can be NULL) */
 unsigned short* port,               /* [out] the peer's port (can be NULL) */
 ENH_ByteOrder   byte_order          /* [in]  host/port byte order          */
 );


/* Get an OS-dependent native socket handle to use by platform-specific API.
 * FYI:  on MS-Windows it will be "SOCKET", on other platforms -- "int".
 */
extern EIO_Status SOCK_GetOSHandle
(SOCK   sock,
 void*  handle_buf,  /* pointer to a memory area to put the OS handle at */
 size_t handle_size  /* the exact(!) size of the expected OS handle      */
 );


/* By default ("on_off" == eDefault,eOff), sockets will not try to read data
 * from inside SOCK_Write(). If you want to automagically upread the data
 * (and cache it in the internal socket buffer) when the write operation
 * is not immediately available, call this func with "on_off" == eOn.
 */
extern void SOCK_SetReadOnWriteAPI(ESwitch on_off);


/* Control the reading-while-writing feature for socket "sock" individually.
 * To reset to the global default behavior (as set by
 * SOCK_SetReadOnWriteAPI), call this function with "on_off" == eDefault.
 */
extern void SOCK_SetReadOnWrite(SOCK sock, ESwitch on_off);


/* Return non-zero value if socket "sock" was created by LSOCK_Accept().
 * Return zero otherwise.
 */
extern int/*bool*/ SOCK_IsServerSide(SOCK sock);



/******************************************************************************
 *  AUXILIARY network-specific functions (added for the portability reasons)
 */


/* Return zero on success, non-zero on error.  See BSD gethostname().
 */
extern int SOCK_gethostname
(char*  name,    /* [out] (guaranteed to be '\0'-terminated)      */
 size_t namelen  /* [in]  max # of bytes allowed to put to "name" */
 );


/* Return zero on success.  Vaguely related to the BSD inet_ntoa().
 */
extern int SOCK_ntoa
(unsigned int host,     /* [in]  must be in the network byte-order           */
 char*        buf,      /* [out] to be filled by smth. like "123.45.67.89\0" */
 size_t       buf_size  /* [in]  max # of bytes to put to "buf", >= 16       */
 );


/* See man for the BSDisms, htonl() and htons().
 */
extern unsigned int SOCK_HostToNetLong(unsigned int value);

#define SOCK_NetToHostLong SOCK_HostToNetLong

extern unsigned short SOCK_HostToNetShort(unsigned short value);

#define SOCK_NetToHostShort SOCK_HostToNetShort

/* Deprecated */
#define SOCK_htonl SOCK_HostToNetLong
#define SOCK_ntohl SOCK_NetToHostLong
#define SOCK_htons SOCK_HostToNetShort
#define SOCK_ntohs SOCK_NetToHostShort


/* Return INET host address (in network byte order) of the
 * specified host (or local host, if hostname is passed as NULL),
 * which can be either domain name or an IP address in
 * dotted notation (e.g. "123.45.67.89\0"). Return 0 on error.
 * NOTE: "0.0.0.0" and "255.255.255.255" are considered not valid.
 */
extern unsigned int SOCK_gethostbyname
(const char* hostname  /* [in]  return current host address if hostname is 0 */
 );


/* Take INET host address (in network byte order) and fill out the
 * the provided buffer with the name, which the address corresponds to
 * (in case of multiple names the primary name is used). Return value 0
 * means error, while success is denoted by the 'name' argument returned.
 */
extern char* SOCK_gethostbyaddr
(unsigned int host,    /* [in]  host address in network byte order           */
 char*        name,    /* [out] buffer to put the name to                    */
 size_t       namelen  /* [in]  size (bytes) of the buffer above             */
 );


#ifdef __cplusplus
} /* extern "C" */
#endif


/*
 * ---------------------------------------------------------------------------
 * $Log$
 * Revision 6.22  2002/09/19 18:07:06  lavr
 * Consistency check moved up to be the first thing
 *
 * Revision 6.21  2002/08/27 03:15:01  lavr
 * Deprecate SOCK_{nh}to{hn}{ls}, define more elaborate call names
 * SOCK_{Net|Host}To{Host|Net}{Long|Short} instead
 *
 * Revision 6.20  2002/08/15 18:44:18  lavr
 * SOCK_Poll() documented in more details
 *
 * Revision 6.19  2002/08/12 14:59:12  lavr
 * Additional (last) argument for SOCK_Write: write_mode
 *
 * Revision 6.18  2002/08/07 16:31:00  lavr
 * Added enum ENH_ByteOrder; renamed SOCK_GetAddress() ->
 * SOCK_GetPeerAddress() and now accepts ENH_ByteOrder as last arg;
 * added SOCK_SetInterruptOnSignal[API]; write-status (w_status) made current;
 * log moved to end
 *
 * Revision 6.17  2002/04/26 16:40:43  lavr
 * New method: SOCK_Poll()
 *
 * Revision 6.16  2002/04/22 20:52:34  lavr
 * +SOCK_htons(), macros SOCK_ntohl() and SOCK_ntohs()
 *
 * Revision 6.15  2001/12/03 21:33:48  vakatov
 * + SOCK_IsServerSide()
 *
 * Revision 6.14  2001/09/10 16:10:41  vakatov
 * SOCK_gethostbyname() -- special cases "0.0.0.0" and "255.255.255.255"
 *
 * Revision 6.13  2001/05/21 15:11:46  ivanov
 * Added (with Denis Vakatov) automatic read on write data from the socket
 * (stall protection).
 * Added functions SOCK_SetReadOnWriteAPI(), SOCK_SetReadOnWrite()
 * and internal function s_SelectStallsafe().
 *
 * Revision 6.12  2001/04/23 22:22:06  vakatov
 * SOCK_Read() -- special treatment for "buf" == NULL
 *
 * Revision 6.11  2001/03/22 17:44:14  vakatov
 * + SOCK_AllowSigPipeAPI()
 *
 * Revision 6.10  2001/03/06 23:54:10  lavr
 * Renamed: SOCK_gethostaddr -> SOCK_gethostbyname
 * Added:   SOCK_gethostbyaddr
 *
 * Revision 6.9  2001/03/02 20:05:15  lavr
 * Typos fixed
 *
 * Revision 6.8  2000/12/26 21:40:01  lavr
 * SOCK_Read modified to handle properly the case of 0 byte reading
 *
 * Revision 6.7  2000/12/05 23:27:44  lavr
 * Added SOCK_gethostaddr
 *
 * Revision 6.6  2000/11/15 18:51:05  vakatov
 * Add SOCK_Shutdown() and SOCK_Status().  Remove SOCK_Eof().
 *
 * Revision 6.5  2000/06/23 19:34:41  vakatov
 * Added means to log binary data
 *
 * Revision 6.4  2000/05/30 23:31:37  vakatov
 * SOCK_host2inaddr() renamed to SOCK_ntoa(), the home-made out-of-scratch
 * implementation to work properly with GCC on IRIX64 platforms
 *
 * Revision 6.3  2000/03/24 23:12:04  vakatov
 * Starting the development quasi-branch to implement CONN API.
 * All development is performed in the NCBI C++ tree only, while
 * the NCBI C tree still contains "frozen" (see the last revision) code.
 *
 * Revision 6.2  2000/02/23 22:33:38  vakatov
 * Can work both "standalone" and as a part of NCBI C++ or C toolkits
 *
 * Revision 6.1  1999/10/18 15:36:39  vakatov
 * Initial revision (derived from the former "ncbisock.[ch]")
 *
 * ===========================================================================
 */

#endif /* CONNECT___NCBI_SOCKET__H */
