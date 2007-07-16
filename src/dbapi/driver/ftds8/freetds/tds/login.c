/* FreeTDS - Library of routines accessing Sybase and Microsoft databases
 * Copyright (C) 1998-1999  Brian Bruns
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <tds_config.h>
#include "tds.h"
#include "tdsutil.h"
#ifdef DMALLOC
#include <dmalloc.h>
#endif

#ifdef NCBI_FTDS
#ifdef _WIN32
#include <windows.h>
#endif
#endif

#ifdef _WIN32
#define IOCTL(a,b,c) ioctlsocket(a, b, c)
#else
#define IOCTL(a,b,c) ioctl(a, b, c)
#endif

#ifdef HAVE_SSL
#define DOMAIN 1
#else
#define DOMAIN 0
#endif

static char  software_version[]   = "$Id$";
static void *no_unused_var_warn[] = {software_version,
                                     no_unused_var_warn};


void tds_set_version(TDSLOGIN *tds_login, short major_ver, short minor_ver)
{
    tds_login->major_version=major_ver;
    tds_login->minor_version=minor_ver;
}
void tds_set_packet(TDSLOGIN *tds_login, short packet_size)
{
    tds_login->block_size=packet_size;
}
void tds_set_port(TDSLOGIN *tds_login, int port)
{
    tds_login->port=port;
}
void tds_set_passwd(TDSLOGIN *tds_login, char *password)
{
    if (password) {
        strncpy(tds_login->password, password, sizeof(tds_login->password)-1);
    }
}
void tds_set_bulk(TDSLOGIN *tds_login, TDS_TINYINT enabled)
{
    tds_login->bulk_copy = enabled ? 0 : 1;
}
void tds_set_user(TDSLOGIN *tds_login, char *username)
{
    strncpy(tds_login->user_name, username, TDS_MAX_LOGIN_STR_SZ);
}
void tds_set_host(TDSLOGIN *tds_login, char *hostname)
{
    strncpy(tds_login->host_name, hostname, TDS_MAX_LOGIN_STR_SZ);
}
void tds_set_app(TDSLOGIN *tds_login, char *application)
{
    strncpy(tds_login->app_name, application, TDS_MAX_LOGIN_STR_SZ);
}
void tds_set_server(TDSLOGIN *tds_login, char *server)
{
    if(!server || strlen(server) == 0) {
        server = getenv("DSQUERY");
        if(!server || strlen(server) == 0) {
            server = "SYBASE";
        }
    }
    strncpy(tds_login->server_name, server, TDS_MAX_LOGIN_STR_SZ);
}
void tds_set_library(TDSLOGIN *tds_login, char *library)
{
    strncpy(tds_login->library, library, TDS_MAX_LIBRARY_STR_SZ);
}
void tds_set_charset(TDSLOGIN *tds_login, char *charset)
{
    strncpy(tds_login->char_set, charset, TDS_MAX_LOGIN_STR_SZ);
}
void tds_set_language(TDSLOGIN *tds_login, char *language)
{
    strncpy(tds_login->language, language, TDS_MAX_LOGIN_STR_SZ);
}
void tds_set_timeouts(TDSLOGIN *tds_login, int connect, int query, int longquery)                   /* Jeffs' hack to support timeouts */
{
    tds_login->connect_timeout = connect;
    tds_login->query_timeout = query;
    tds_login->longquery_timeout = longquery;
}
void tds_set_longquery_handler(TDSLOGIN * tds_login, void (*longquery_func)(long), long longquery_param) /* Jeff's hack */
{
    tds_login->longquery_func = longquery_func;
    tds_login->longquery_param = longquery_param;
}
void tds_set_capabilities(TDSLOGIN *tds_login, unsigned char *capabilities, int size)
{
    memcpy(tds_login->capabilities, capabilities,
           size > TDS_MAX_CAPABILITY ? TDS_MAX_CAPABILITY : size);
}

#ifdef NCBI_FTDS
TDSSOCKET *tds_connect(TDSLOGIN *login, TDSCONTEXT *context, void *parent)
{
    TDSSOCKET   *tds;
    struct sockaddr_in      sin;
    /* Jeff's hack - begin */
    unsigned long ioctl_blocking = 1;
    struct timeval selecttimeout;
    fd_set fds;
    fd_set fds1;
    int retval, n;
    // time_t start, now;
    TDSCONFIGINFO *config;
    /* 13 + max string of 32bit int, 30 should cover it */
    char query[30];
    char *tmpstr;
    int connect_timeout = 0;


    FD_ZERO (&fds);
    FD_ZERO (&fds1);
    /* end */

    config = tds_get_config(NULL, login, context->locale);

    /*
    ** If a dump file has been specified, start logging
    */
    if (config->dump_file) {
        tdsdump_open(config->dump_file);
    }

    /*
    ** The initial login packet must have a block size of 512.
    ** Once the connection is established the block size can be changed
    ** by the server with TDS_ENV_CHG_TOKEN
    */
    tds = tds_alloc_socket(context, 512);
    tds_set_parent(tds, parent);
    tds->config = config;

    tds->major_version=config->major_version;
    tds->minor_version=config->minor_version;
    tds->emul_little_endian=config->emul_little_endian;
#ifdef WORDS_BIGENDIAN
    if (IS_TDS70(tds) || IS_TDS80(tds)) {
        /* TDS 7/8 only supports little endian */
        tds->emul_little_endian=1;
    }
#endif

    /* set up iconv */
    if (config->client_charset) {
        tds_iconv_open(tds, config->client_charset);
    }

    /* specified a date format? */
    /*
      if (config->date_fmt) {
        tds->date_fmt=strdup(config->date_fmt);
      }
    */
    if (login->connect_timeout) {
        connect_timeout = login->connect_timeout;
    } else if (config->connect_timeout) {
        connect_timeout = config->connect_timeout;
    }

    /* Jeff's hack - begin */
    tds->timeout = login->query_timeout;
    tds->longquery_timeout = login->longquery_timeout;
    tds->longquery_func = login->longquery_func;
    tds->longquery_param = login->longquery_param;
    /* end */

    /* verify that ip_addr is not NULL */
    if (!config->ip_addr[0]) {
        tdsdump_log(TDS_DBG_ERROR, "%L IP address pointer is NULL\n");
        if (config->server_name) {
            tmpstr = malloc(strlen(config->server_name)+100);
            if (tmpstr) {
                sprintf(tmpstr,"Server %s not found!",config->server_name);
                tds_client_msg(tds->tds_ctx, tds, 10019, 9, 0, 0, tmpstr);
                free(tmpstr);
            }
        } else {
            tds_client_msg(tds->tds_ctx, tds, 10020, 9, 0, 0, "No server specified!");
        }
        tds_free_config(config);
        tds_free_socket(tds);
        return NULL;
    }
    memcpy(tds->capabilities,login->capabilities,TDS_MAX_CAPABILITY);

    for(n= 0; n < NCBI_NUM_SERVERS; n++) {
        if(config->ip_addr[n] == NULL) {
            /* no more servers */
            tds_client_msg(tds->tds_ctx, tds, 20009, 9, 0, 0,
                           "Server is unavailable or does not exist.");
            tds_free_config(config);
            tds_free_socket(tds);
            return NULL;
        }
        sin.sin_addr.s_addr = inet_addr(config->ip_addr[n]);
        if (sin.sin_addr.s_addr == -1) {
            tdsdump_log(TDS_DBG_ERROR, "%L inet_addr() failed, IP = %s\n", config->ip_addr[n]);
            continue;
        }

        sin.sin_family = AF_INET;
        sin.sin_port = htons(config->port[n]);


        tdsdump_log(TDS_DBG_INFO1, "%L Connecting addr %s port %d\n",
                    inet_ntoa(sin.sin_addr), ntohs(sin.sin_port));

        if ((tds->s = socket (AF_INET, SOCK_STREAM, 0)) < 0) {
            perror ("socket");
            tds_free_config(config);
            tds_free_socket(tds);
            return NULL;
        }

#ifdef FIONBIO
        ioctl_blocking = 1; /* ~0; //TRUE; */
        if (IOCTL(tds->s, FIONBIO, &ioctl_blocking) < 0) {
            tds_free_config(config);
            tds_free_socket(tds);
            return NULL;
        }
#endif
        retval = connect(tds->s, (struct sockaddr *) &sin, sizeof(sin));
        if (retval < 0 && (sock_errno == TDSSOCK_EINPROGRESS || sock_errno == TDSSOCK_EWOULDBLOCK)) retval = 0;
        if (retval < 0) {
            CLOSESOCKET(tds->s);
            continue;
        }
        /* Select on writeability for connect_timeout */
        FD_ZERO (&fds);
        FD_ZERO (&fds1);
        for(retval= -1; retval < 0;) {
            selecttimeout.tv_sec = connect_timeout;
            selecttimeout.tv_usec = 0;
            FD_SET (tds->s, &fds);
            FD_SET (tds->s, &fds1);
            retval = select(tds->s + 1, NULL, &fds, &fds1,
                            (connect_timeout > 0)? &selecttimeout : NULL);
            if((retval < 0) && (errno != EINTR)) {
                break;
            }
        }
        if(retval > 0) {
          int r, r_len= sizeof(r);
          if(FD_ISSET(tds->s, &fds1)) retval= -1;
          if(getsockopt(tds->s, SOL_SOCKET, SO_ERROR, &r, &r_len) < 0 ||
             r != 0) retval= -1;
        }
        if(retval > 0)
          break;
        close(tds->s);
        tds->s= 0;
    }

    if(n >= NCBI_NUM_SERVERS) {
        char message[128];

        if (config && config->server_name) {
            sprintf(message, "Server '%s' is unavailable or does not exist.",
                    config->server_name);
        } else {
            sprintf(message, "Server is unavailable or does not exist.");
        }
        tds_client_msg(tds->tds_ctx, tds, 20009, 9, 0, 0, message);
        tds_free_config(config);
        tds_free_socket(tds);
        return NULL;
    }

    if (IS_TDS7_PLUS(tds)) {
        tds->out_flag=0x10;
        tds7_send_login(tds,config);
    } else {
        tds->out_flag=0x02;
        tds_send_login(tds,config);
    }
    if (!tds_process_login_tokens(tds)) {
        tds_client_msg(tds->tds_ctx, tds, 20014, 9, 0, 0,
                       "Login incorrect.");
        tds_free_config(config);
        tds_free_socket(tds);
        tds = NULL;
        return NULL;
    }
    if (tds && config->text_size) {
        sprintf(query,"set textsize %d", config->text_size);
        retval = tds_submit_query(tds,query);
        if (retval == TDS_SUCCEED) {
            while (tds_process_result_tokens(tds)==TDS_SUCCEED);
        }
    }

    tds->config = NULL;
    tds_free_config(config);
    return tds;
}
#else
TDSSOCKET *tds_connect(TDSLOGIN *login, TDSCONTEXT *context, void *parent)
{
    TDSSOCKET   *tds;
    struct sockaddr_in      sin;
    /* Jeff's hack - begin */
    unsigned long ioctl_blocking = 1;
    struct timeval selecttimeout;
    fd_set fds;
    int retval;
    time_t start, now;
    TDSCONFIGINFO *config;
    /* 13 + max string of 32bit int, 30 should cover it */
    char query[30];
    char *tmpstr;
    int connect_timeout = 0;


    FD_ZERO (&fds);
    /* end */

    config = tds_get_config(NULL, login, context->locale);

    /*
    ** If a dump file has been specified, start logging
    */
    if (config->dump_file) {
        tdsdump_open(config->dump_file);
    }

    /*
    ** The initial login packet must have a block size of 512.
    ** Once the connection is established the block size can be changed
    ** by the server with TDS_ENV_CHG_TOKEN
    */
    tds = tds_alloc_socket(context, 512);
    tds_set_parent(tds, parent);
    tds->config = config;

    tds->major_version=config->major_version;
    tds->minor_version=config->minor_version;
    tds->emul_little_endian=config->emul_little_endian;
#ifdef WORDS_BIGENDIAN
    if (IS_TDS70(tds) || IS_TDS80(tds)) {
        /* TDS 7/8 only supports little endian */
        tds->emul_little_endian=1;
    }
#endif

    /* set up iconv */
    if (config->client_charset) {
        tds_iconv_open(tds, config->client_charset);
    }

    /* specified a date format? */
    /*
      if (config->date_fmt) {
        tds->date_fmt=strdup(config->date_fmt);
      }
    */
    if (login->connect_timeout) {
        connect_timeout = login->connect_timeout;
    } else if (config->connect_timeout) {
        connect_timeout = config->connect_timeout;
    }

    /* Jeff's hack - begin */
    tds->timeout = (connect_timeout) ? login->query_timeout : 0;
    tds->longquery_timeout = (connect_timeout) ? login->longquery_timeout : 0;
    tds->longquery_func = login->longquery_func;
    tds->longquery_param = login->longquery_param;
    /* end */

    /* verify that ip_addr is not NULL */
    if (!config->ip_addr) {
        tdsdump_log(TDS_DBG_ERROR, "%L IP address pointer is NULL\n");
        if (config->server_name) {
            tmpstr = malloc(strlen(config->server_name)+100);
            if (tmpstr) {
                sprintf(tmpstr,"Server %s not found!",config->server_name);
                tds_client_msg(tds->tds_ctx, tds, 10019, 9, 0, 0, tmpstr);
                free(tmpstr);
            }
        } else {
            tds_client_msg(tds->tds_ctx, tds, 10020, 9, 0, 0, "No server specified!");
        }
        tds_free_config(config);
        tds_free_socket(tds);
        return NULL;
    }
    sin.sin_addr.s_addr = inet_addr(config->ip_addr);
    if (sin.sin_addr.s_addr == -1) {
        tdsdump_log(TDS_DBG_ERROR, "%L inet_addr() failed, IP = %s\n", config->ip_addr);
        tds_free_config(config);
        tds_free_socket(tds);
        return NULL;
    }

    sin.sin_family = AF_INET;
    sin.sin_port = htons(config->port);

    memcpy(tds->capabilities,login->capabilities,TDS_MAX_CAPABILITY);

    tdsdump_log(TDS_DBG_INFO1, "%L Connecting addr %s port %d\n", inet_ntoa(sin.sin_addr), ntohs(sin.sin_port));
    if ((tds->s = socket (AF_INET, SOCK_STREAM, 0)) < 0) {
        perror ("socket");
        tds_free_config(config);
        tds_free_socket(tds);
        return NULL;
    }

    /* Jeff's hack *** START OF NEW CODE *** */
    if (connect_timeout) {
        start = time (NULL);
        ioctl_blocking = 1; /* ~0; //TRUE; */
#ifdef FIONBIO
        if (IOCTL(tds->s, FIONBIO, &ioctl_blocking) < 0) {
            tds_free_config(config);
            tds_free_socket(tds);
            return NULL;
        }
#endif
        retval = connect(tds->s, (struct sockaddr *) &sin, sizeof(sin));
        if (retval < 0 && errno == EINPROGRESS) retval = 0;
        if (retval < 0) {
            if (config && config->server_name) {
                sprintf(message, "Server '%s' is unavailable or does not exist.",
                        config->server_name);
            } else {
                sprintf(message, "Server is unavailable or does not exist.");
            }
            tds_client_msg(tds->tds_ctx, tds, 20009, 9, 0, 0, message);
            tds_free_config(config);
            tds_free_socket(tds);
            return NULL;
        }
        /* Select on writeability for connect_timeout */
        now = start;
        while ((retval == 0) && ((now-start) < connect_timeout)) {
            FD_SET (tds->s, &fds);
            selecttimeout.tv_sec = connect_timeout - (now-start);
            selecttimeout.tv_usec = 0;
            retval = select(tds->s + 1, NULL, &fds, NULL, &selecttimeout);
            if (retval < 0 && errno == EINTR)
                retval = 0;
            now = time (NULL);
        }

        if ((now-start) > connect_timeout) {
            if (config && config->server_name) {
                sprintf(message, "Server '%s' is unavailable or does not exist.",
                        config->server_name);
            } else {
                sprintf(message, "Server is unavailable or does not exist.");
            }
            tds_client_msg(tds->tds_ctx, tds, 20009, 9, 0, 0, message);
            tds_free_config(config);
            tds_free_socket(tds);
            return NULL;
        }
    } else {
        if (connect(tds->s, (struct sockaddr *) &sin, sizeof(sin)) <0) {
            char message[128];
            sprintf(message, "src/tds/login.c: tds_connect: %s:%d",
                    inet_ntoa(sin.sin_addr), ntohs(sin.sin_port));
            perror(message);
            if (config && config->server_name) {
                sprintf(message, "Server '%s' is unavailable or does not exist.",
                        config->server_name);
            } else {
                sprintf(message, "Server is unavailable or does not exist.");
            }
            tds_client_msg(tds->tds_ctx, tds, 20009, 9, 0, 0, message);
            tds_free_config(config);
            tds_free_socket(tds);
            return NULL;
        }
    }
    /* END OF NEW CODE */

    if (IS_TDS7_PLUS(tds)) {
        tds->out_flag=0x10;
        tds7_send_login(tds,config);
    } else {
        tds->out_flag=0x02;
        tds_send_login(tds,config);
    }
    if (!tds_process_login_tokens(tds)) {
        tds_client_msg(tds->tds_ctx, tds, 20014, 9, 0, 0,
                       "Login incorrect.");
        tds_free_config(config);
        tds_free_socket(tds);
        tds = NULL;
        return NULL;
    }
    if (tds && config->text_size) {
        sprintf(query,"set textsize %d", config->text_size);
        retval = tds_submit_query(tds,query);
        if (retval == TDS_SUCCEED) {
            while (tds_process_result_tokens(tds)==TDS_SUCCEED);
        }
    }

    tds->config = NULL;
    tds_free_config(config);
    return tds;
}
#endif
int tds_send_login(TDSSOCKET *tds, TDSCONFIGINFO *config)
{
    /*   char *tmpbuf;
         int tmplen;*/
#ifdef WORDS_BIGENDIAN
    unsigned char be1[]= {0x02,0x00,0x06,0x04,0x08,0x01};
#endif
    unsigned char le1[]= {0x03,0x01,0x06,0x0a,0x09,0x01};
    unsigned char magic2[]={0x00,0x00};

    unsigned char magic3[]= {0x00,0x00,0x00};

/* these seem to endian flags as well 13,17 on intel/alpha 12,16 on power */

#ifdef WORDS_BIGENDIAN
    unsigned char be2[]= {0x00,12,16};
#endif
    unsigned char le2[]= {0x00,13,17};

   /*
   ** the former byte 0 of magic5 causes the language token and message to be
   ** absent from the login acknowledgement if set to 1. There must be a way
   ** of setting this in the client layer, but I am not aware of any thing of
   ** the sort -- bsb 01/17/99
   */
    unsigned char magic5[]= {0x00,0x00};
    unsigned char magic6[]= {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};
    unsigned char magic7=   0x01;

    unsigned char magic42[]= {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};
    unsigned char magic50[]= {0x00,0x00,0x00,0x00};
    /*
** capabilities are now part of the tds structure.
   unsigned char capabilities[]= {0x01,0x07,0x03,109,127,0xFF,0xFF,0xFF,0xFE,0x02,0x07,0x00,0x00,0x0A,104,0x00,0x00,0x00};
*/
/*
** This is the original capabilities packet we were working with (sqsh)
   unsigned char capabilities[]= {0x01,0x07,0x03,109,127,0xFF,0xFF,0xFF,0xFE,0x02,0x07,0x00,0x00,0x0A,104,0x00,0x00,0x00};
** original with 4.x messages
   unsigned char capabilities[]= {0x01,0x07,0x03,109,127,0xFF,0xFF,0xFF,0xFE,0x02,0x07,0x00,0x00,0x00,120,192,0x00,0x0D};
** This is isql 11.0.3
   unsigned char capabilities[]= {0x01,0x07,0x00,96, 129,207, 0xFF,0xFE,62,  0x02,0x07,0x00,0x00,0x00,120,192,0x00,0x0D};
** like isql but with 5.0 messages
   unsigned char capabilities[]= {0x01,0x07,0x00,96, 129,207, 0xFF,0xFE,62,  0x02,0x07,0x00,0x00,0x00,120,192,0x00,0x00};
**
*/

    unsigned char protocol_version[4];
    unsigned char program_version[4];

    int rc;
    char blockstr[10], passwdstr[255];

    if (IS_TDS42(tds)) {
        memcpy(protocol_version,"\004\002\000\000",4);
        memcpy(program_version,"\004\002\000\000",4);
    } else if (IS_TDS46(tds)) {
        memcpy(protocol_version,"\004\006\000\000",4);
        memcpy(program_version,"\004\002\000\000",4);
    } else if (IS_TDS50(tds)) {
        memcpy(protocol_version,"\005\000\000\000",4);
        memcpy(program_version,"\005\000\000\000",4);
    } else {
        tdsdump_log(TDS_DBG_SEVERE,"Unknown protocol version!\n");
        exit(1);
    }
    /*
   ** the following code is adapted from  Arno Pedusaar's
   ** (psaar@fenar.ee) MS-SQL Client. His was a much better way to
   ** do this, (well...mine was a kludge actually) so here's mostly his
   */

    rc=tds_put_string(tds,config->host_name,TDS_MAX_LOGIN_STR_SZ);   /* client host name */
    rc|=tds_put_string(tds,config->user_name,TDS_MAX_LOGIN_STR_SZ);  /* account name */
    rc|=tds_put_string(tds,config->password,TDS_MAX_LOGIN_STR_SZ);  /* account password */
    //rc|=tds_put_string(tds,"37876",TDS_MAX_LOGIN_STR_SZ);        /* host process */
    rc|=tds_put_string(tds,"11111",TDS_MAX_LOGIN_STR_SZ);        /* host process */
#ifdef WORDS_BIGENDIAN
    if (tds->emul_little_endian) {
        rc|=tds_put_n(tds,le1,6);
    } else {
        rc|=tds_put_n(tds,be1,6);
    }
#else
    rc|=tds_put_n(tds,le1,6);
#endif
    rc|=tds_put_byte(tds,config->bulk_copy);
    rc|=tds_put_n(tds,magic2,2);
    if (IS_TDS42(tds)) {
        rc|=tds_put_int(tds,512);
    } else {
        rc|=tds_put_int(tds,0);
    }
    rc|=tds_put_n(tds,magic3,3);
    rc|=tds_put_string(tds,config->app_name,TDS_MAX_LOGIN_STR_SZ);
    rc|=tds_put_string(tds,config->server_name,TDS_MAX_LOGIN_STR_SZ);
    if (IS_TDS42(tds)) {
        rc|=tds_put_string(tds,config->password,255);
    } else {
        if(config->password == NULL) {
            sprintf(passwdstr, "%c%c%s", 0, 0, "");
            rc|=tds_put_buf(tds,passwdstr,255,(unsigned char)2);
        } else {
            sprintf(passwdstr,"%c%c%s",0,
                    (unsigned char)strlen(config->password),
                    config->password);
            rc|=tds_put_buf(tds,passwdstr,255,(unsigned char)strlen(config->password)+2);
        }
    }

    rc|=tds_put_n(tds,protocol_version,4); /* TDS version; { 0x04,0x02,0x00,0x00 } */
    rc|=tds_put_string(tds,config->library,10);  /* client program name */
    if (IS_TDS42(tds)) {
        rc|=tds_put_int(tds,0);
    } else {
        rc|=tds_put_n(tds,program_version,4); /* program version ? */
    }
#ifdef WORDS_BIGENDIAN
    if (tds->emul_little_endian) {
        rc|=tds_put_n(tds,le2,3);
    } else {
        rc|=tds_put_n(tds,be2,3);
    }
#else
    rc|=tds_put_n(tds,le2,3);
#endif
    rc|=tds_put_string(tds,config->language,TDS_MAX_LOGIN_STR_SZ);  /* language */
    rc|=tds_put_byte(tds,config->suppress_language);
    rc|=tds_put_n(tds,magic5,2);
    rc|=tds_put_byte(tds,config->encrypted);
    rc|=tds_put_n(tds,magic6,10);
    rc|=tds_put_string(tds,config->char_set,TDS_MAX_LOGIN_STR_SZ);  /* charset */
    rc|=tds_put_byte(tds,magic7);
    sprintf(blockstr,"%d",config->block_size);
    rc|=tds_put_string(tds,blockstr,6); /* network packet size */
    if (IS_TDS42(tds)) {
        rc|=tds_put_n(tds,magic42,8);
    } else if (IS_TDS46(tds)) {
        rc|=tds_put_n(tds,magic42,4);
    } else if (IS_TDS50(tds)) {
        rc|=tds_put_n(tds,magic50,4);
        rc|=tds_put_byte(tds, TDS_CAP_TOKEN);
        rc|=tds_put_smallint(tds, 18);
        rc|=tds_put_n(tds,tds->capabilities,TDS_MAX_CAPABILITY);
    }

    /*
   tmpbuf = malloc(tds->out_pos);
   tmplen = tds->out_pos;
   memcpy(tmpbuf, tds->out_buf, tmplen);
   tdsdump_off();
*/
    rc|=tds_flush_packet(tds);
    /*
   tdsdump_on();
   tdsdump_log(TDS_DBG_NETWORK, "Sending packet (passwords supressed)@ %L\n%D\n", tmpbuf, tmplen);
   free(tmpbuf);
*/
   /* get_incoming(tds->s); */
    return 0;
}


int tds7_send_auth(TDSSOCKET *tds, unsigned char *challenge)
{
#if DOMAIN
    int rc;
    int current_pos;
    TDSANSWER answer;

    /* FIXME: stuff duplicate in tds7_send_login */
    const char* domain;
    const char* user_name;
    const char* p;
    int user_name_len;
    int host_name_len;
    int password_len;
    int domain_len;
    unsigned char unicode_string[256];

    TDSCONFIGINFO *config = tds->config;

    /* check config */
    if (!config)
        return TDS_FAIL;

    /* parse a bit of config */
    domain = config->default_domain;
    user_name = config->user_name;
    user_name_len = user_name ? strlen(user_name) : 0;
    host_name_len = config->host_name ? strlen(config->host_name) : 0;
    password_len = config->password ? strlen(config->password) : 0;
    domain_len = domain ? strlen(domain) : 0;

    /* check override of domain */
    if (user_name && (p=strchr(user_name,'\\')) != NULL)
        {
            domain = user_name;
            domain_len = p-user_name;

            user_name = p+1;
            user_name_len = strlen(user_name);
        }

    tds->out_flag=0x11;
    tds_put_n(tds, "NTLMSSP", 8);
    tds_put_int(tds, 3); /* sequence 3 */

    current_pos = 64 + (domain_len+user_name_len+host_name_len)*2;

    tds_put_smallint(tds, 24);  /* lan man resp length */
    tds_put_smallint(tds, 24);  /* lan man resp length */
    tds_put_int(tds, current_pos);  /* resp offset */
    current_pos += 24;

    tds_put_smallint(tds, 24);  /* nt resp length */
    tds_put_smallint(tds, 24);  /* nt resp length */
    tds_put_int(tds, current_pos);  /* nt resp offset */

    current_pos = 64;

    /* domain */
    tds_put_smallint(tds, domain_len*2);
    tds_put_smallint(tds, domain_len*2);
    tds_put_int(tds, current_pos);
    current_pos += domain_len*2;

    /* username */
    tds_put_smallint(tds, user_name_len*2);
    tds_put_smallint(tds, user_name_len*2);
    tds_put_int(tds, current_pos);
    current_pos += user_name_len*2;

    /* hostname */
    tds_put_smallint(tds, host_name_len*2);
    tds_put_smallint(tds, host_name_len*2);
    tds_put_int(tds, current_pos);
    current_pos += host_name_len*2;

    /* unknown */
    tds_put_smallint(tds, 0);
    tds_put_smallint(tds, 0);
    tds_put_int(tds, current_pos + (24*2));

    /* flags */
    tds_put_int(tds,0x8201);

    tds7_ascii2unicode(tds,domain, unicode_string, 256);
    tds_put_n(tds,unicode_string,domain_len * 2);
    tds7_ascii2unicode(tds,user_name, unicode_string, 256);
    tds_put_n(tds,unicode_string,user_name_len * 2);
    tds7_ascii2unicode(tds,config->host_name, unicode_string, 256);
    tds_put_n(tds,unicode_string,host_name_len * 2);

    tds_answer_challenge(config->password, challenge, &answer);
    tds_put_n(tds, answer.lm_resp, 24);
    tds_put_n(tds, answer.nt_resp, 24);

    /* for security reason clear structure */
    memset(&answer,0,sizeof(TDSANSWER));

    rc=tds_flush_packet(tds);

    return rc;
#else
    return TDS_SUCCEED;
#endif
}
/*
** tds7_send_login() -- Send a TDS 7.0 login packet
** TDS 7.0 login packet is vastly different and so gets its own function
*/
int tds7_send_login(TDSSOCKET *tds, TDSCONFIGINFO *config)
{
    int rc;
#if DOMAIN
    static const unsigned char magic1_domain[] =
    {6,0x7d,0x0f,0xfd,
     0xff,0x0,0x0,0x0,  /* Client PID */
     /* the 0x80 in the third byte controls whether this is a domain login
      * or not  0x80 = yes, 0x00 = no */
     0x0,0xe0,0x83,0x0, /* Connection ID of the Primary Server (?) */
     0x0,               /* Option Flags 1 */
     0x68,              /* Option Flags 2 */
     0x01,
     0x00,
     0x00,0x09,0x04,0x00,
     0x00};
#endif
    static const unsigned char magic1_server[] =
    {6,0x83,0xf2,0xf8,  /* Client Program version */
     0xff,0x0,0x0,0x0,  /* Client PID */
     0x0,0xe0,0x03,0x0, /* Connection ID of the Primary Server (?) */
     0x0,               /* Option Flags 1 */
     0x88,              /* Option Flags 2 */
     0xff,              /* Type Flags     */
     0xff,              /* reserved Flags */
     0xff,0x36,0x04,0x00,
     0x00};
    unsigned const char *magic1 = magic1_server;
#if 0
    /* also seen */
    {6,0x7d,0x0f,0xfd,
         0xff,0x0,0x0,0x0,
         0x0,0xe0,0x83,0x0,
         0x0,
         0x68,
         0x01,
         0x00,
         0x00,0x09,0x04,0x00,
         0x00};
#endif
    static const unsigned char magic2[] = {0x00,0x40,0x33,0x9a,0x6b,0x50};
    /* 0xb4,0x00,0x30,0x00,0xe4,0x00,0x00,0x00; */
    static const unsigned char magic3[] = "NTLMSSP";
    unsigned char unicode_string[255];
    int packet_size;
    int current_pos;
#if DOMAIN
    int domain_login = config->try_domain_login ? 1 : 0;
#endif

    const char* domain = config->default_domain;
    const char* user_name = config->user_name;
    const char* p;
    int user_name_len = user_name ? strlen(user_name) : 0;
    int host_name_len = config->host_name ? strlen(config->host_name) : 0;
    int app_name_len = config->app_name ? strlen(config->app_name) : 0;
    int password_len = config->password ? strlen(config->password) : 0;
    int server_name_len = config->server_name ? strlen(config->server_name) : 0;
    int library_len = config->library ? strlen(config->library) : 0;
    int language_len = config->language ? strlen(config->language) : 0;
    int domain_len = domain ? strlen(domain) : 0;
    int auth_len = 0;

    /* check override of domain */
    if (user_name && (p=strchr(user_name,'\\')) != NULL)
        {
            domain = user_name;
            domain_len = p-user_name;

            user_name = p+1;
            user_name_len = strlen(user_name);
        }

    packet_size = 86 + (
                        host_name_len +
                        app_name_len +
                        server_name_len +
                        library_len +
                        language_len)*2;
#if DOMAIN
    if (domain_login) {
        magic1 = magic1_domain;
        auth_len = 32 + host_name_len + domain_len;
        packet_size += auth_len;
    } else
#endif
        packet_size += (user_name_len + password_len)*2;

#ifdef NCBI_FTDS
    tds_put_int(tds, packet_size);
    if (IS_TDS80(tds)) {
      static const unsigned char tds8Version[] = { 0x01, 0x00, 0x00, 0x71 };
        tds_put_n(tds, tds8Version, 4);
    } else {
      static const unsigned char tds7Version[] = { 0x00, 0x00, 0x00, 0x70 };
        tds_put_n(tds, tds7Version, 4);
    }
#else
    tds_put_smallint(tds,packet_size);
    tds_put_n(tds,NULL,5);
    if (IS_TDS80(tds)) {
        tds_put_byte(tds,0x80);
    } else {
        tds_put_byte(tds,0x70);
    }
    tds_put_n(tds,NULL,3);       /* rest of TDSVersion which is a 4 byte field    */
#endif
#ifdef NCBI_FTDS
    if(config->block_size < 512 || config->block_size > 1000000)
      config->block_size= 4096;
    tds_put_int(tds, config->block_size);

    tds_put_n(tds, magic1, 4);  /* client program version ? */

    packet_size= getpid();
    tds_put_int(tds, packet_size);  /* process id of this process */

    /*tds_put_n(tds, magic1+8, 13);*/
#if 1
    {
    static const unsigned char connection_id[] = { 0x00, 0x00, 0x00, 0x00 };
    unsigned char option_flag1 = 0x00;
    unsigned char option_flag2 = 0x00;
    static const unsigned char sql_type_flag = 0x00;
    static const unsigned char reserved_flag = 0x00;
    static const unsigned char time_zone[] = { 0x88, 0xff, 0xff, 0xff };
    static const unsigned char collation[] = { 0x36, 0x04, 0x00, 0x00 };

    tds_put_n(tds, connection_id, 4);
    option_flag1 |= 0x80;   /* enable warning messages if SET LANGUAGE issued   */
    option_flag1 |= 0x40;   /* change to initial database must succeed          */
    option_flag1 |= 0x20;   /* enable warning messages if USE <database> issued */

    tds_put_byte(tds, option_flag1);

    option_flag2 |= 0x02;   /* client is an ODBC driver                         */
    option_flag2 |= 0x01;   /* change to initial language must succeed          */

    tds_put_byte(tds, option_flag2);

    tds_put_byte(tds, sql_type_flag);
    tds_put_byte(tds, reserved_flag);

    tds_put_n(tds, time_zone, 4);
    tds_put_n(tds, collation, 4);
    }
#endif


#else
    tds_put_n(tds,NULL,4);       /* desired packet size being requested by client */
    tds_put_n(tds,magic1,21);
#endif

    current_pos = 86; /* ? */
    /* host name */
    tds_put_smallint(tds,current_pos);
    tds_put_smallint(tds,host_name_len);
    current_pos += host_name_len * 2;
#if DOMAIN
    if (domain_login) {
        tds_put_smallint(tds,0);
        tds_put_smallint(tds,0);
        tds_put_smallint(tds,0);
        tds_put_smallint(tds,0);
    } else {
#endif
        /* username */
        tds_put_smallint(tds,current_pos);
        tds_put_smallint(tds,user_name_len);
        current_pos += user_name_len * 2;
        /* password */
        tds_put_smallint(tds,current_pos);
        tds_put_smallint(tds,password_len);
        current_pos += password_len * 2;
#if DOMAIN
    }
#endif
    /* app name */
    tds_put_smallint(tds,current_pos);
    tds_put_smallint(tds,app_name_len);
    current_pos += app_name_len * 2;
    /* server name */
    tds_put_smallint(tds,current_pos);
    tds_put_smallint(tds,server_name_len);
    current_pos += server_name_len * 2;
    /* unknown */
    tds_put_smallint(tds,0);
    tds_put_smallint(tds,0);
    /* library name */
    tds_put_smallint(tds,current_pos);
    tds_put_smallint(tds,library_len);
    current_pos += library_len * 2;
    /* language  - kostya@warmcat.excom.spb.su */
    tds_put_smallint(tds,current_pos);
    tds_put_smallint(tds,language_len);
    current_pos += language_len * 2;
    /* database name */
    tds_put_smallint(tds,current_pos);
    tds_put_smallint(tds,0);

    /* MAC address */
    tds_put_n(tds,magic2,6);

    /* authentication stuff */
    tds_put_smallint(tds, current_pos);
#if DOMAIN
    if (domain_login) {
        tds_put_smallint(tds, auth_len); /* this matches numbers at end of packet */
        current_pos += auth_len;
    } else
        tds_put_smallint(tds, 0);
#else
    tds_put_smallint(tds, 0);
#endif

    /* unknown */
    tds_put_smallint(tds, current_pos);
    tds_put_smallint(tds, 0);

    tds7_ascii2unicode(tds,config->host_name, unicode_string, 255);
    tds_put_n(tds,unicode_string,host_name_len * 2);
#if DOMAIN
    if (!domain_login) {
#endif
        tds7_ascii2unicode(tds,config->user_name, unicode_string, 255);
        tds_put_n(tds,unicode_string,user_name_len * 2);
        tds7_ascii2unicode(tds,config->password, unicode_string, 255);
        tds7_crypt_pass(unicode_string, password_len * 2, unicode_string);
        tds_put_n(tds,unicode_string,password_len * 2);
#if DOMAIN
    }
#endif
    tds7_ascii2unicode(tds,config->app_name, unicode_string, 255);
    tds_put_n(tds,unicode_string,app_name_len * 2);
    tds7_ascii2unicode(tds,config->server_name, unicode_string, 255);
    tds_put_n(tds,unicode_string,server_name_len * 2);
    tds7_ascii2unicode(tds,config->library, unicode_string, 255);
    tds_put_n(tds,unicode_string,library_len * 2);
    tds7_ascii2unicode(tds,config->language, unicode_string, 255);
    tds_put_n(tds,unicode_string,language_len * 2);

#if DOMAIN
    if (domain_login) {
        /* from here to the end of the packet is the NTLMSSP authentication */
        tds_put_n(tds,magic3,8);
        /* sequence 1 client -> server */
        tds_put_int(tds,1);
        /* flags */
        tds_put_int(tds,0xb201);

        /* domain info */
        tds_put_smallint(tds,domain_len);
        tds_put_smallint(tds,domain_len);
        tds_put_int(tds,32 + host_name_len);

        /* hostname info */
        tds_put_smallint(tds,host_name_len);
        tds_put_smallint(tds,host_name_len);
        tds_put_int(tds,32);

        /* hostname and domain */
        tds_put_n(tds,config->host_name,host_name_len);
        tds_put_n(tds,domain,domain_len);
    }
#endif

    tdsdump_off();
    rc=tds_flush_packet(tds);
    tdsdump_on();

    return rc;
}

/*
** tds7_crypt_pass() -- 'encrypt' TDS 7.0 style passwords.
** the calling function is responsible for ensuring crypt_pass is at least
** 'len' characters
*/
unsigned char *
tds7_crypt_pass(const unsigned char *clear_pass, int len, unsigned char *crypt_pass)
{
    int i;
    unsigned char xormask = 0x5A;
    unsigned char hi_nibble,lo_nibble ;

    for (i=0;i<len;i++) {
        lo_nibble  = (clear_pass[i] ^ xormask) >> 4;
        hi_nibble  = (clear_pass[i] ^ xormask) << 4;
        crypt_pass[i] = hi_nibble | lo_nibble;
    }
    return crypt_pass;
}
