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
 *   FTP CONNECTOR
 *
 *   See <connect/ncbi_connector.h> for the detailed specification of
 *   the connector's methods and structures.
 *
 */

#include "ncbi_ansi_ext.h"
#include <connect/ncbi_buffer.h>
#include <connect/ncbi_ftp_connector.h>
#include <connect/ncbi_socket.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>


/***********************************************************************
 *  INTERNAL -- Auxiliary types and static functions
 ***********************************************************************/

/* All internal data necessary to perform the (re)connect and i/o
 */
typedef struct {
    const char*    host;
    unsigned short port;
    const char*    user;
    const char*    pass;
    const char*    path;
    const char*    name;
    ESwitch        log;
    SOCK           cntl;  /* control connection */
    SOCK           data;  /* data    connection */
    BUF            wbuf;  /* write buffer       */
    EIO_Status     r_status;
    EIO_Status     w_status;
} SFTPConnector;


static EIO_Status s_ReadReply(SOCK sock, int* code,
                              char* line, size_t maxlinelen)
{
    int/*bool*/ first = 1/*true*/;
    for (;;) {
        int c, m;
        size_t n;
        char buf[1024];
        EIO_Status status = SOCK_ReadLine(sock, buf, sizeof(buf), &n);
        if (status != eIO_Success)
            return status;
        if (n == sizeof(buf))
            return eIO_Unknown/*line too long*/;
        if (first  ||  isdigit(*buf)) {
            if (sscanf(buf, "%d%n", &c, &m) < 1)
                return eIO_Unknown;
        } else
            c = 0;
        if (first) {
            if (m != 3  ||  code == 0)
                return eIO_Unknown;
            if (line)
                strncpy0(line, &buf[m + 1], maxlinelen);
            *code = c;
            if (buf[m] != '-') {
                if (buf[m] == ' ')
                    break;
                return eIO_Unknown;
            }
            first = 0/*false*/;
        } else if (c == *code  &&  m == 3  &&  buf[m] == ' ')
            break;
    }
    return eIO_Success;
}


static EIO_Status s_FTPReply(SFTPConnector* xxx, int* code,
                             char* line, size_t maxlinelen)
{
    int c = 0;
    EIO_Status status = eIO_Closed;
    if (xxx->cntl) {
        status = s_ReadReply(xxx->cntl, &c, line, maxlinelen);
        if (status == eIO_Success  &&  c == 421)
            status =  eIO_Closed;
        if (status == eIO_Closed  ||  (status == eIO_Success  &&  c == 221)) {
            SOCK_Close(xxx->cntl);
            xxx->cntl = 0;
            if (xxx->data) {
                SOCK_Close(xxx->data);
                xxx->data = 0;
            }
        }
    }
    if (code)
        *code = c;
    return status;
}


static EIO_Status s_WriteCommand(SOCK sock,
                                 const char* cmd, const char* arg)
{
    size_t cmdlen = strlen(cmd);
    size_t arglen = arg ? strlen(arg) : 0;
    size_t linelen = cmdlen + (arglen ? 1/* */ + arglen : 0) + 2/*\r\n*/;
    char* line = malloc(linelen + 1/*\0*/);
    EIO_Status status = eIO_Unknown;
    if (line) {
        memcpy(line, cmd, cmdlen);
        if (arglen) {
            line[cmdlen++] = ' ';
            memcpy(line + cmdlen, arg, arglen);
            cmdlen += arglen;
        }
        line[cmdlen++] = '\r';
        line[cmdlen++] = '\n';
        line[cmdlen]   = '\0';
        status = SOCK_Write(sock, line, linelen, 0, eIO_WritePersist);
        free(line);
    }
    return status;
}


static EIO_Status s_FTPCommand(SFTPConnector* xxx,
                               const char* cmd, const char* arg)
{
    return xxx->cntl ? s_WriteCommand(xxx->cntl, cmd, arg) : eIO_Closed;
}


static EIO_Status s_FTPLogin(SFTPConnector* xxx, const STimeout* timeout)
{
    EIO_Status status;
    int code;

    assert(xxx->cntl != 0);
    status = SOCK_SetTimeout(xxx->cntl, eIO_ReadWrite, timeout);
    if (status != eIO_Success)
        return status;
    status = s_FTPReply(xxx, &code, 0, 0);
    if (status != eIO_Success)
        return status;
    if (code != 220)
        return eIO_Unknown;
    status = s_FTPCommand(xxx, "USER", xxx->user);
    if (status != eIO_Success)
        return status;
    status = s_FTPReply(xxx, &code, 0, 0);
    if (status != eIO_Success)
        return status;
    if (code == 230)
        return eIO_Success;
    if (code != 331)
        return eIO_Unknown;
    status = s_FTPCommand(xxx, "PASS", xxx->pass);
    if (status != eIO_Success)
        return status;
    status = s_FTPReply(xxx, &code, 0, 0);
    if (status != eIO_Success)
        return status;
    if (code == 230)
        return eIO_Success;
    else
        return eIO_Unknown;
}


static EIO_Status s_FTPChdir(SFTPConnector* xxx, const char* cmd)
{
    if (cmd  ||  (xxx->path  &&  *xxx->path)) {
        int code;
        EIO_Status status = s_FTPCommand(xxx,
                                         cmd ? cmd : "CWD",
                                         cmd ? 0   : xxx->path);
        if (status != eIO_Success)
            return status;
        status = s_FTPReply(xxx, &code, 0, 0);
        if (status != eIO_Success)
            return status;
        if (code != 250)
            return eIO_Unknown;
    }
    return eIO_Success;
}


static EIO_Status s_FTPBinary(SFTPConnector* xxx)
{
    int code;
    EIO_Status status = s_FTPCommand(xxx, "TYPE", "I");
    if (status != eIO_Success)
        return status;
    status = s_FTPReply(xxx, &code, 0, 0);
    if (status != eIO_Success)
        return status;
    if (code == 200)
        return eIO_Success;
    else
        return eIO_Unknown;
}


static EIO_Status s_FTPAbort(SFTPConnector* xxx)
{
    EIO_Status status = eIO_Success;
    if (xxx->data) {
        int code;
        status = s_FTPCommand(xxx, "ABOR", 0);
        SOCK_Read(xxx->data, 0, 1024*1024/*drain up*/, 0, eIO_ReadPlain);
        if (status == eIO_Success)
            status = s_FTPReply(xxx, &code, 0, 0);
        if (status == eIO_Success  &&  code != 226)
            status = eIO_Unknown;
        if (status == eIO_Success)
            status = SOCK_Close(xxx->data);
        else if (xxx->data) {
            SOCK_Close(xxx->data);
            xxx->data = 0;
        }
    }
    return status;
}


static EIO_Status s_FTPPasv(SFTPConnector*  xxx)
{
    static STimeout zero;
    int code, o[6];
    unsigned int i;
    char buf[128], *c;
    unsigned int host;
    unsigned short port;

    EIO_Status status = s_FTPCommand(xxx, "PASV", 0);
    if (status != eIO_Success)
        return status;
    status = s_FTPReply(xxx, &code, buf, sizeof(buf) - 1);
    if (status != eIO_Success  ||  code != 227)
        return eIO_Unknown;
    buf[sizeof(buf) - 1] = '\0';
    if (!(c = strchr(buf, '(')))
        return eIO_Unknown;
    if (sscanf(++c, "%d,%d,%d,%d,%d,%d)",
               &o[0], &o[1], &o[2], &o[3], &o[4], &o[5]) < 6) {
        return eIO_Unknown;
    }
    for (i = 0; i < sizeof(o)/sizeof(o[0]); i++) {
        if (o[i] < 0 || o[i] > 255)
            return eIO_Unknown;
    }
    i = (((((o[0] << 8) + o[1]) << 8) + o[2]) << 8) + o[3];
    host = SOCK_htonl(i);
    i = (o[4] << 8) + o[5];
    port = (unsigned short) i;
    if (SOCK_ntoa(host, buf, sizeof(buf)) == 0  &&
        SOCK_CreateEx(buf, port, &zero, &xxx->data,
                      0, 0, xxx->log) == eIO_Success) {
        return eIO_Success;
    }
    s_FTPAbort(xxx);
    return eIO_Unknown;
}


static EIO_Status s_FTPRetrieve(SFTPConnector*  xxx,
                                const char*     cmd)
{
    int code;
    EIO_Status status = s_FTPPasv(xxx);
    if (status != eIO_Success)
        return status;
    status = s_FTPCommand(xxx, cmd, 0);
    if (status != eIO_Success)
        return status;
    status = s_FTPReply(xxx, &code, 0, 0);
    if (status != eIO_Success)
        return status;
    if (code == 150)
        return eIO_Success;
    s_FTPAbort(xxx);
    return eIO_Unknown;
}


static EIO_Status s_FTPQuit(SFTPConnector* xxx)
{
    EIO_Status status;
    int code;
    s_FTPAbort(xxx);
    status = s_FTPCommand(xxx, "QUIT", 0);
    if (status == eIO_Success)
        status = s_FTPReply(xxx, &code, 0, 0);
    if (status == eIO_Success  &&  code != 221)
        status = eIO_Unknown;
    if (xxx->cntl) {
        if (status == eIO_Success)
            status = SOCK_Close(xxx->cntl);
        else
            SOCK_Close(xxx->cntl);
        xxx->cntl = 0;
    }
    return status;
}


static EIO_Status s_FTPExecute(SFTPConnector* xxx, const STimeout* timeout)
{
    EIO_Status status = eIO_Success;
    size_t size = BUF_Size(xxx->wbuf);
    char* s = malloc(size + 1);
    s_FTPAbort(xxx);
    assert(size);
    if (s) {
        if (BUF_Read(xxx->wbuf, s, size) == size  &&
            SOCK_SetTimeout(xxx->cntl, eIO_ReadWrite, timeout) == eIO_Success){
            char* c;
            s[size] = '\0';
            if (!(c = strchr(s, ' ')))
                c = s + strlen(s);
            size = (size_t)(c - s);
            if      (strncasecmp(s, "CWD",  size) == 0) {
                status = s_FTPChdir(xxx, s);
            } else if (strncasecmp(s, "LIST", size) == 0  ||
                       strncasecmp(s, "NLST", size) == 0  ||
                       strncasecmp(s, "RETR", size) == 0) {
                status = s_FTPRetrieve(xxx, s);
            } else if (strncasecmp(s, "REST", size) == 0) {
                status = s_FTPCommand(xxx, s, 0);
                if (status == eIO_Success) {
                    int code;
                    status = s_FTPReply(xxx, &code, 0, 0);
                    if (status == eIO_Success  &&  code != 350)
                        status = eIO_Unknown;
                }
            } else
                status = eIO_Unknown;
        } else
            status = eIO_Unknown;
        free(s);
    } else
        status = eIO_Unknown;
    return status;
}


/***********************************************************************
 *  INTERNAL -- "s_VT_*" functions for the "virt. table" of connector methods
 ***********************************************************************/

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */
    static const char* s_VT_GetType (CONNECTOR       connector);
    static EIO_Status  s_VT_Open    (CONNECTOR       connector,
                                     const STimeout* timeout);
    static EIO_Status  s_VT_Wait    (CONNECTOR       connector,
                                     EIO_Event       event,
                                     const STimeout* timeout);
    static EIO_Status  s_VT_Write   (CONNECTOR       connector,
                                     const void*     buf,
                                     size_t          size,
                                     size_t*         n_written,
                                     const STimeout* timeout);
    static EIO_Status  s_VT_Read    (CONNECTOR       connector,
                                     void*           buf,
                                     size_t          size,
                                     size_t*         n_read,
                                     const STimeout* timeout);
    static EIO_Status  s_VT_Flush   (CONNECTOR       connector,
                                     const STimeout* timeout);
    static EIO_Status  s_VT_Status  (CONNECTOR       connector,
                                     EIO_Event       dir);
    static EIO_Status  s_VT_Close   (CONNECTOR       connector,
                                     const STimeout* timeout);
    static void        s_Setup      (SMetaConnector* meta,
                                     CONNECTOR       connector);
    static void        s_Destroy    (CONNECTOR       connector);
#  ifdef IMPLEMENTED__CONN_WaitAsync
    static EIO_Status s_VT_WaitAsync(void*                   connector,
                                     FConnectorAsyncHandler  func,
                                     SConnectorAsyncHandler* data);
#  endif
#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */


/*ARGSUSED*/
static const char* s_VT_GetType
(CONNECTOR connector)
{
    return "FTP";
}


static EIO_Status s_VT_Open
(CONNECTOR       connector,
 const STimeout* timeout)
{
    SFTPConnector* xxx = (SFTPConnector*) connector->handle;
    EIO_Status status;

    assert(!xxx->data  &&  !xxx->cntl);
    status = SOCK_CreateEx(xxx->host, xxx->port, timeout, &xxx->cntl,
                           0, 0, xxx->log);
    if (status == eIO_Success)
        status = s_FTPLogin(xxx, timeout);
    if (status == eIO_Success)
        status = s_FTPChdir(xxx, 0);
    if (status == eIO_Success)
        status = s_FTPBinary(xxx);
    if (status != eIO_Success) {
        if (xxx->cntl) {
            SOCK_Close(xxx->cntl);
            xxx->cntl = 0;
        }
    }
    xxx->r_status = status;
    xxx->w_status = status;
    return status;
}


static EIO_Status s_VT_Write
(CONNECTOR       connector,
 const void*     buf,
 size_t          size,
 size_t*         n_written,
 const STimeout* timeout)
{
    SFTPConnector* xxx = (SFTPConnector*) connector->handle;
    EIO_Status status;
    const char* c;
    size_t s;

    xxx->w_status = eIO_Success;
    if ( !size )
        return eIO_Success;
    if (!xxx->cntl)
        return eIO_Closed;

    if ((c = strchr((const char*) buf, '\n')) != 0) {
        if (c[1])
            return eIO_Unknown;
        if (c != buf  &&  c[-1] == '\r')
            c--;
        s = (size_t)(c - (const char*) buf);
    } else
        s = size;
    status = BUF_Write(&xxx->wbuf, buf, s) ? eIO_Success : eIO_Unknown;
    if (status == eIO_Success  &&  c) {
        xxx->w_status = s_FTPExecute(xxx, timeout);
        status = xxx->w_status;
    }
    *n_written = size;
    return status;
}


static EIO_Status s_VT_Read
(CONNECTOR       connector,
 void*           buf,
 size_t          size,
 size_t*         n_read,
 const STimeout* timeout)
{
    SFTPConnector* xxx = (SFTPConnector*) connector->handle;
    EIO_Status status = eIO_Closed;
    xxx->r_status = eIO_Success;
    if (xxx->cntl  &&  xxx->data) {
        status = SOCK_SetTimeout(xxx->data, eIO_Read, timeout);
        if (status == eIO_Success) {
            xxx->r_status = SOCK_Read(xxx->data,
                                      buf, size, n_read, eIO_ReadPlain);
            if (xxx->r_status == eIO_Closed) {
                int code;
                SOCK_Close(xxx->data);
                xxx->data = 0;
                SOCK_SetTimeout(xxx->cntl, eIO_Read, timeout);
                s_FTPReply(xxx, &code, 0, 0);
            }
            status = xxx->r_status;
        }
    }
    return status;
}


static EIO_Status s_VT_Wait
(CONNECTOR       connector,
 EIO_Event       event,
 const STimeout* timeout)
{
    SFTPConnector* xxx = (SFTPConnector*) connector->handle;
    if (!xxx->cntl)
        return eIO_Closed;
    if (event & eIO_Write)
        return eIO_Success;
    if (!xxx->data) {
        EIO_Status status;
        if (!BUF_Size(xxx->wbuf))
            return eIO_Closed;
        if ((status = s_FTPExecute(xxx, timeout)) != eIO_Success)
            return status;
        if (!xxx->data)
            return eIO_Closed;
    }
    return SOCK_Wait(xxx->data, eIO_Read, timeout);
}


static EIO_Status s_VT_Flush
(CONNECTOR       connector,
 const STimeout* timeout)
{
    SFTPConnector* xxx = (SFTPConnector*) connector->handle;
    size_t size = BUF_Size(xxx->wbuf);
    EIO_Status status = eIO_Success;
    if (size != 0)
        status = s_FTPExecute(xxx, timeout);
    return status;
}


static EIO_Status s_VT_Status
(CONNECTOR connector,
 EIO_Event dir)
{
    SFTPConnector* xxx = (SFTPConnector*) connector->handle;

    switch (dir) {
    case eIO_Read:
        return xxx->r_status;
    case eIO_Write:
        return xxx->w_status;
    default:
        assert(0); /* should never happen as checked by connection */
        return eIO_InvalidArg;
    }
}


/*ARGSUSED*/
static EIO_Status s_VT_Close
(CONNECTOR       connector,
 const STimeout* timeout)
{
    SFTPConnector* xxx = (SFTPConnector*) connector->handle;
    if (xxx->cntl)
        SOCK_SetTimeout(xxx->cntl, eIO_ReadWrite, timeout);
    return  s_FTPQuit(xxx);
}


static void s_Setup
(SMetaConnector* meta,
 CONNECTOR       connector)
{
    /* initialize virtual table */
    CONN_SET_METHOD(meta, get_type,   s_VT_GetType,   connector);
    CONN_SET_METHOD(meta, open,       s_VT_Open,      connector);
    CONN_SET_METHOD(meta, wait,       s_VT_Wait,      connector);
    CONN_SET_METHOD(meta, write,      s_VT_Write,     connector);
    CONN_SET_METHOD(meta, flush,      s_VT_Flush,     connector);
    CONN_SET_METHOD(meta, read,       s_VT_Read,      connector);
    CONN_SET_METHOD(meta, status,     s_VT_Status,    connector);
    CONN_SET_METHOD(meta, close,      s_VT_Close,     connector);
#ifdef IMPLEMENTED__CONN_WaitAsync
    CONN_SET_METHOD(meta, wait_async, s_VT_WaitAsync, connector);
#endif
    meta->default_timeout = 0/*infinite*/;
}


static void s_Destroy
(CONNECTOR connector)
{
    SFTPConnector* xxx = (SFTPConnector*) connector->handle;
    free((void*) xxx->host);
    free((void*) xxx->user);
    free((void*) xxx->pass);
    if (xxx->path)
        free((void*) xxx->path);
    if (xxx->name)
        free((void*) xxx->name);
    BUF_Destroy(xxx->wbuf);
    free(xxx);
    connector->handle = 0;
    free(connector);
}


/***********************************************************************
 *  EXTERNAL -- the connector's "constructors"
 ***********************************************************************/

extern CONNECTOR FTP_CreateDownloadConnector(const char*    host,
                                             unsigned short port,
                                             const char*    user,
                                             const char*    pass,
                                             const char*    path,
                                             ESwitch        log)
{
    CONNECTOR      ccc = (SConnector*) malloc(sizeof(SConnector));
    SFTPConnector* xxx = (SFTPConnector*) malloc(sizeof(*xxx));

    xxx->data    = 0;
    xxx->cntl    = 0;
    xxx->wbuf    = 0;
    xxx->host    = strdup(host);
    xxx->port    = port ? port : 21;
    xxx->user    = strdup(user  &&  *user ? user : "ftp");
    xxx->pass    = strdup(pass  &&  *pass ? pass : "none");
    xxx->path    = path  &&  *path ? strdup(path) : 0;
    xxx->name    = 0;
    xxx->log     = log;
    /* initialize connector data */
    ccc->handle  = xxx;
    ccc->next    = 0;
    ccc->meta    = 0;
    ccc->setup   = s_Setup;
    ccc->destroy = s_Destroy;

    return ccc;
}


/*
 * --------------------------------------------------------------------------
 * $Log$
 * Revision 1.2  2004/12/07 14:21:55  lavr
 * Init wbuf in ctor
 *
 * Revision 1.1  2004/12/06 17:48:38  lavr
 * Initial revision
 *
 * ==========================================================================
 */
