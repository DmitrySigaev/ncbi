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
 *   A simple test program to listen on a socket and just bounce all
 *   incoming data (e.g., can be used to test "ncbi_socket_connector.[ch]")
 *
 * --------------------------------------------------------------------------
 * $Log$
 * Revision 6.2  2002/01/16 21:23:14  vakatov
 * Utilize header "test_assert.h" to switch on ASSERTs in the Release mode too
 *
 * Revision 6.1  2000/04/07 20:04:37  vakatov
 * Initial revision
 *
 * ==========================================================================
 */

#include "test_assert.h"

#include <connect/ncbi_socket.h>
#include <stdio.h>


static FILE* s_LogFile;


/* Listen on the specified "port".
 * Accept connections and bounce the incoming data back to the client
 * (only one client at a time).
 * Perform up to "n_cycle" sessions, then exit.
 */
static void s_DoServer(unsigned short port, int n_cycle)
{
    LSOCK      lsock;
    EIO_Status status;

    fprintf(s_LogFile, "DoServer(port = %hu, n_cycle = %u)\n", port, n_cycle);

    /* Create listening socket */
    status = LSOCK_Create(port, 1, &lsock);
    assert(status == eIO_Success);

    /* Accept connections from clients and run test sessions */
    while ( n_cycle-- ) {
        SOCK     sock;
        size_t   n_io, n_io_done;
        char     buf[1024];
        STimeout timeout;

        /* Accept connection */
        status = LSOCK_Accept(lsock, 0, &sock);
        assert(status == eIO_Success);

        /* Set i/o timeouts for the accepted connection */
        timeout.sec  = 100;
        timeout.usec = 888;
        status = SOCK_SetTimeout(sock, eIO_Read,  &timeout);
        assert(status == eIO_Success);
        status = SOCK_SetTimeout(sock, eIO_Write, &timeout);
        assert(status == eIO_Success);

        /* Bounce all incoming data back to the client */
        while ((status = SOCK_Read(sock, buf, sizeof(buf), &n_io, eIO_Plain))
               == eIO_Success) {
            status = SOCK_Write(sock, buf, n_io, &n_io_done);
            if (status != eIO_Success) {
                fprintf(s_LogFile,
                        "Failed to write -- DoServer(n_cycle = %d): %s\n",
                        n_cycle, IO_StatusStr(status));
                break;
            }
        }
        assert(status == eIO_Closed);

        /* Close connection */
        status = SOCK_Close(sock);
        assert(status == eIO_Success  ||  status == eIO_Closed);
    }

    /* Close listening socket */
    status = LSOCK_Close(lsock);
    assert(status == eIO_Success);
}


int main(int argc, const char* argv[])
{
    /* variables */
#define MIN_PORT 5001
    unsigned short port = 0;
    int n_cycle = 100;

    /* logging */
    s_LogFile = fopen("socket_io_bouncer.log", "a+");
    assert(s_LogFile);

    /* cmd.-line args */
    switch ( argc ) {
    case 3: {
        if (sscanf(argv[2], "%d", &n_cycle) != 1  ||  n_cycle <= 0)
            break;
    }
    case 2: {
        int i_port = -1;
        if (sscanf(argv[1], "%d", &i_port) != 1  ||
            i_port < MIN_PORT  ||  65535 < i_port)
            break;
        port = (unsigned short) i_port;
    }
    } /* switch */

    if (port == 0) {
        fprintf(stderr,
                "Usage: %s <port> [n_cycle]\n where <port> not less than %d\n",
                argv[0], (int) MIN_PORT);
        fprintf(s_LogFile,
                "Usage: %s <port> [n_cycle]\n where <port> not less than %d\n",
                argv[0], (int) MIN_PORT);
        return 1;
    }

    /* run */
    s_DoServer(port, n_cycle);

    /* cleanup */
    verify(SOCK_ShutdownAPI() == eIO_Success);
    fclose(s_LogFile);
    return 0;
}
