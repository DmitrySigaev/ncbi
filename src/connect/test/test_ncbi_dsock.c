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
 * Author:  Anton Lavrentiev
 *
 * File Description:
 *   Test suite for datagram socket API
 *
 */

#include "../ncbi_ansi_ext.h"
#include "../ncbi_priv.h"               /* CORE logging facilities */
#include <connect/ncbi_socket.h>
#include <errno.h>
#include <stdlib.h>
#include <time.h>
#ifdef NCBI_OS_UNIX
#  include <unistd.h>
#endif /*NCBI_OS_UNIX*/
/* This header must go last */
#include "test_assert.h"

#ifdef NCBI_OS_BSD
   /* FreeBSD has this limit :-/ Source: `sysctl net.inet.udp.maxdgram` */
#  define MAX_DGRAM_SIZE 9*1024
#else
   /* This is the maximal datagram size defined by the UDP standard */
#  define MAX_DGRAM_SIZE 65535
#endif

#define DEFAULT_PORT 55555


static int s_Usage(const char* prog)
{
    CORE_LOGF(eLOG_Error, ("Usage:\n%s {client|server} [port [seed]]", prog));
    return 1;
}


static int s_Server(int x_port)
{
    char           addr[32];
    char*          buf;
    SOCK           server;
    EIO_Status     status;
    STimeout       timeout;
    unsigned int   peeraddr;
    unsigned short peerport, port;
    size_t         msgsize, n, len;
    char           minibuf[255];

    if (x_port <= 0) {
        CORE_LOG(eLOG_Error, "[Server]  Port wrongly specified");
        return 1;
    }

    port = (unsigned short) x_port;
    CORE_LOGF(eLOG_Note, ("[Server]  Opening DSOCK on port %hu", port));

    if ((status = DSOCK_Create(port, &server)) != eIO_Success) {
        CORE_LOGF(eLOG_Error, ("[Server]  Error creating DSOCK: %s",
                               IO_StatusStr(status)));
        return 1;
    }

    if ((status = DSOCK_WaitMsg(server, 0/*infinite*/)) != eIO_Success) {
        CORE_LOGF(eLOG_Error, ("[Server]  Error waiting on DSOCK: %s",
                               IO_StatusStr(status)));
        return 1;
    }

    timeout.sec  = 0;
    timeout.usec = 0;
    if ((status= SOCK_SetTimeout(server, eIO_Read, &timeout)) != eIO_Success) {
        CORE_LOGF(eLOG_Error, ("[Server]  Error setting zero read tmo: %s",
                               IO_StatusStr(status)));
        return 1;
    }

    len = (size_t)(((double) rand()/(double) RAND_MAX)*sizeof(minibuf));
    if ((status = DSOCK_RecvMsg(server, 0, minibuf, len, &msgsize,
                                &peeraddr, &peerport)) != eIO_Success) {
        CORE_LOGF(eLOG_Error, ("[Server]  Error reading from DSOCK: %s",
                               IO_StatusStr(status)));
        return 1;
    }
    if (len > msgsize)
        len = msgsize;

    if (SOCK_ntoa(peeraddr, addr, sizeof(addr)) != 0)
        strcpy(addr, "<unknown>");

    CORE_LOGF(eLOG_Note, ("[Server]  Message received from %s:%hu, %lu bytes",
                          addr, peerport, (unsigned long) msgsize));

    if (!(buf = malloc(msgsize ? msgsize : 1))) {
        CORE_LOG_ERRNO(eLOG_Error, errno, "[Server]  Cannot alloc msg buf");
        return 1;
    }
    if (len)
        memcpy(buf, minibuf, len);

    while (len < msgsize) {
        n = (size_t)(((double) rand()/(double) RAND_MAX)*(msgsize - len) +0.5);
        if ((status = SOCK_Read(server, buf + len, n, &n, eIO_ReadPlain))
            != eIO_Success) {
            CORE_LOGF(eLOG_Error,("[Server]  Error reading msg @ byte %lu: %s",
                                  (unsigned long) len, IO_StatusStr(status)));
            return 1;
        }
        len += n;
    }
    assert(SOCK_Read(server, 0, 1, &n, eIO_ReadPlain) == eIO_Closed);

    CORE_LOG(eLOG_Note, "[Server]  Bouncing the message to sender");

    timeout.sec  = 1;
    timeout.usec = 0;
    if ((status= SOCK_SetTimeout(server, eIO_Write, &timeout)) != eIO_Success){
        CORE_LOGF(eLOG_Error, ("[Server]  Error setting write tmo: %s",
                               IO_StatusStr(status)));
        return 1;
    }

    for (len = 0; len < msgsize; len += n) {
        n = (size_t)(((double) rand()/(double) RAND_MAX)*(msgsize - len) +0.5);
        if ((status = SOCK_Write(server, buf + len, n, &n, eIO_WritePlain))
            != eIO_Success) {
            CORE_LOGF(eLOG_Error,("[Server]  Error writing msg @ byte %lu: %s",
                                  (unsigned long) len, IO_StatusStr(status)));
            return 1;
        }
    }

    free(buf);

    if ((status = DSOCK_SendMsg(server, addr, peerport, "--Reply--", 9))
        != eIO_Success) {
        CORE_LOGF(eLOG_Error, ("[Server]  Error sending to DSOCK: %s",
                               IO_StatusStr(status)));
        return 1;
    }

    if ((status = SOCK_Close(server)) != eIO_Success) {
        CORE_LOGF(eLOG_Error, ("[Server]  Error closing DSOCK: %s",
                               IO_StatusStr(status)));
        return 1;
    }

    CORE_LOG(eLOG_Note, "[Server]  Completed successfully");
    return 0;
}


static int s_Client(int x_port)
{
    unsigned short port;
    size_t         msgsize, n;
    STimeout       timeout;
    EIO_Status     status;
    SOCK           client;
    char*          buf;

    if (x_port <= 0) {
        CORE_LOG(eLOG_Error, "[Client]  Port wrongly specified");
        return 1;
    }
 
    port = (unsigned short) x_port;
    CORE_LOGF(eLOG_Note, ("[Client]  Opening DSOCK on port %hu", port));

    if ((status = DSOCK_Create(0, &client)) != eIO_Success) {
        CORE_LOGF(eLOG_Error, ("[Client]  Error creating DSOCK: %s",
                               IO_StatusStr(status)));
        return 1;
    }

    msgsize = (size_t)(((double)rand()/(double)RAND_MAX)*(MAX_DGRAM_SIZE-10));
    CORE_LOGF(eLOG_Note, ("[Client]  Generating a message %lu bytes long",
                          (unsigned long) msgsize));

    if (!(buf = malloc(2*msgsize + 9))) {
        CORE_LOG_ERRNO(eLOG_Error, errno, "[Client]  Cannot alloc msg buf");
        return 1;        
    }

    for (n = 0; n < msgsize; n++)
        buf[n] = rand() % 0xFF;

    if ((status = DSOCK_SendMsg(client, "127.0.0.1", port, buf, msgsize))
        != eIO_Success) {
        CORE_LOGF(eLOG_Error, ("[Client]  Error sending to DSOCK: %s",
                               IO_StatusStr(status)));
        return 1;
    }

    timeout.sec  = 1;
    timeout.usec = 0;
    if ((status = SOCK_SetTimeout(client, eIO_Read, &timeout)) != eIO_Success){
        CORE_LOGF(eLOG_Error, ("[Client]  Error setting read tmo: %s",
                               IO_StatusStr(status)));
        return 1;
    }

    if ((status = DSOCK_RecvMsg(client, 0, &buf[msgsize],
                                msgsize + 9, &n, 0, 0))
        != eIO_Success) {
        CORE_LOGF(eLOG_Error, ("[Client]  Error reading from DSOCK: %s",
                               IO_StatusStr(status)));
        return 1;
    }

    if (n != msgsize + 9) {
        CORE_LOGF(eLOG_Error, ("[Client]  Received message of wrong size: %lu",
                               (unsigned long) n));
        return 1;
    } else {
        CORE_LOGF(eLOG_Note, ("[Client]  Received message back, %lu bytes",
                              (unsigned long) n));
    }
    assert(SOCK_Read(client, 0, 1, &n, eIO_ReadPlain) == eIO_Closed);

    for (n = 0; n < msgsize; n++) {
        if (buf[n] != buf[msgsize + n])
            break;
    }

    if (n < msgsize) {
        CORE_LOGF(eLOG_Error, ("[Client]  Bounced message corrupted, off=%lu",
                               (unsigned long) n));
        return 1;
    }

    if (strncmp(&buf[msgsize*2], "--Reply--", 9) != 0) {
        CORE_LOGF(eLOG_Error, ("[Client]  No signature in the message: %.9s",
                               &buf[msgsize*2]));
        return 1;
    }

    free(buf);

    if ((status = SOCK_Close(client)) != eIO_Success) {
        CORE_LOGF(eLOG_Error, ("[Client]  Error closing DSOCK: %s",
                               IO_StatusStr(status)));
        return 1;
    }

    CORE_LOG(eLOG_Note, "[Client]  Completed successfully");
    return 0;
}


int main(int argc, const char* argv[])
{
    unsigned long seed;
    const char*   env = getenv("CONN_DEBUG_PRINTOUT");

    CORE_SetLOGFILE(stderr, 0/*false*/);
    CORE_SetLOGFormatFlags
        (fLOG_None | fLOG_Level | fLOG_OmitNoteLevel | fLOG_DateTime );

    if (argc < 2 || argc > 4)
        return s_Usage(argv[0]);

    if (argc <= 3) {
#ifdef NCBI_OS_UNIX
        seed = (unsigned long) time(0) + (unsigned long) getpid();
#else
        seed = (unsigned long) time(0);
#endif /*NCBI_OS_UNIX*/
    } else
        sscanf(argv[3], "%lu", &seed);
    CORE_LOGF(eLOG_Note, ("Random SEED = %lu", seed));
    srand(seed);

    if (env && (strcasecmp(env, "1") == 0    ||
                strcasecmp(env, "yes") == 0  ||
                strcasecmp(env, "true") == 0 ||
                strcasecmp(env, "some") == 0 ||
                strcasecmp(env, "data") == 0)){
        SOCK_SetDataLoggingAPI(eOn);
    }

    if (strcasecmp(argv[1], "client") == 0)
        return s_Client(argv[2] ? atoi(argv[2]) : DEFAULT_PORT);

    if (strcasecmp(argv[1], "server") == 0)
        return s_Server(argv[2] ? atoi(argv[2]) : DEFAULT_PORT);

    return s_Usage(argv[0]);
}


/*
 * --------------------------------------------------------------------------
 * $Log$
 * Revision 6.4  2003/02/06 04:34:28  lavr
 * Do not exceed maximal dgram size on BSD; trickier readout in s_Server()
 *
 * Revision 6.3  2003/02/04 22:04:47  lavr
 * Protection from truncation to 0 in double->int conversion
 *
 * Revision 6.2  2003/01/17 01:26:44  lavr
 * Test improved/extended
 *
 * Revision 6.1  2003/01/16 16:33:06  lavr
 * Initial revision
 *
 * ==========================================================================
 */
