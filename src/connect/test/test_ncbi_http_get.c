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
 *   Retrieve a Web document via HTTP
 *
 */

#include "../ncbi_priv.h"
#include <connect/ncbi_ansi_ext.h>
#include <connect/ncbi_http_connector.h>
#include <connect/ncbi_util.h>
#include <stdlib.h>
#include <string.h>
/* This header must go last */
#include "test_assert.h"


int main(int argc, char* argv[])
{
    CONNECTOR     connector;
    SConnNetInfo* net_info;
    THCC_Flags    flags;
    CONN          conn;
    char*         s;

    CORE_SetLOGFILE(stderr, 0/*false*/);

    if (argc != 2 || !*argv[1]) {
        CORE_LOG(eLOG_Error, "URL has to be supplied on the command line");
        exit(1);
    }

    CORE_LOG(eLOG_Note, "Creating network info structure");
    if (!(net_info = ConnNetInfo_Create(0)))
        CORE_LOG(eLOG_Fatal, "Cannot create network info structure");
    net_info->req_method = eReqMethod_Get;

    CORE_LOGF(eLOG_Note,
              ("Parsing URL \"%s\" into network info structure", argv[1]));
    if (!ConnNetInfo_ParseURL(net_info, argv[1]))
        CORE_LOG(eLOG_Fatal, "URL parsing failed");

    if ((s = getenv("CONN_RECONNECT")) != 0 &&
        (strcasecmp(s, "TRUE") == 0 || strcasecmp(s, "1") == 0)) {
        flags = fHCC_AutoReconnect;
        CORE_LOG(eLOG_Note, "Reconnect mode will be used");
    } else
        flags = 0;

    CORE_LOG(eLOG_Note, "Creating HTTP connector");
    if (!(connector = HTTP_CreateConnector(net_info, 0, flags)))
        CORE_LOG(eLOG_Fatal, "Cannot create HTTP connector");

    CORE_LOG(eLOG_Note, "Creating connection");
    if (CONN_Create(connector, &conn) != eIO_Success)
        CORE_LOG(eLOG_Fatal, "Cannot create connection");

    for (;;) {
        size_t n;
        char blob[512];
        EIO_Status status = CONN_Read(conn,blob,sizeof(blob),&n,eIO_ReadPlain);

        if (status != eIO_Success && status != eIO_Closed)
            CORE_LOGF(eLOG_Fatal, ("Read error: %s", IO_StatusStr(status)));
        if (n) {
            fwrite(blob, 1, n, stdout);
            fflush(stdout);
        } else if (status == eIO_Closed) {
            break;
        } else
            CORE_LOG(eLOG_Fatal, "Empty read");
    }

    CORE_LOG(eLOG_Note, "Closing connection");
    CONN_Close(conn);

    CORE_LOG(eLOG_Note, "Completed");
    return 0;
}


/*
 * --------------------------------------------------------------------------
 * $Log$
 * Revision 6.6  2002/08/14 14:42:46  lavr
 * Use fwrite() instead of printf() when printing out the data fetched
 *
 * Revision 6.5  2002/08/07 16:38:08  lavr
 * EIO_ReadMethod enums changed accordingly; log moved to end
 *
 * Revision 6.4  2002/03/22 19:47:23  lavr
 * Test_assert.h made last among the include files
 *
 * Revision 6.3  2002/02/05 21:45:55  lavr
 * Included header files rearranged
 *
 * Revision 6.2  2002/01/16 21:23:15  vakatov
 * Utilize header "test_assert.h" to switch on ASSERTs in the Release mode too
 *
 * Revision 6.1  2001/12/30 19:42:06  lavr
 * Initial revision
 *
 * ==========================================================================
 */
