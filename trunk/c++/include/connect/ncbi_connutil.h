#ifndef NCBI_CONNUTIL__H
#define NCBI_CONNUTIL__H

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
 *   Auxiliary API to:
 *    1.Retrieve connection related info from the registry:
 *       SConnNetInfo
 *       ConnNetInfo_Create()
 *       ConnNetInfo_AdjustForHttpProxy()
 *       ConnNetInfo_Clone()
 *       ConnNetInfo_Print()
 *       ConnNetInfo_Destroy()
 *       #define REG_CONN_***
 *       #define DEF_CONN_***
 *
 *    2.Make a connection to an URL:
 *       URL_Connect()
 *       
 *    3.Perform URL encoding/decoding of data:
 *       URL_Decode()
 *       URL_DecodeEx()
 *       URL_Encode()
 *
 *    4.Compose or parse NCBI-specific Content-Type's:
 *       EMIME_SubType
 *       EMIME_Encoding
 *       MIME_ComposeContentType()
 *       MIME_ParseContentType()
 *
 *    5.Search for a token in the input stream (either CONN or SOCK):
 *       CONN_StripToPattern()
 *       SOCK_StripToPattern()
 *
 *    6.Convert "[host][:port]" from verbal into binary form and vice versa:
 *       StringToHostPort()
 *       HostPortToString()
 *
 * --------------------------------------------------------------------------
 * $Log$
 * Revision 6.16  2001/09/10 21:14:47  lavr
 * Added functions: StringToHostPort()
 *                  HostPortToString()
 *
 * Revision 6.15  2001/06/01 16:01:58  vakatov
 * MIME_ParseContentTypeEx() -- extended description
 *
 * Revision 6.14  2001/05/29 21:15:42  vakatov
 * + eMIME_Plain
 *
 * Revision 6.13  2001/04/24 21:21:38  lavr
 * Special text value "infinite" accepted as infinite timeout from environment
 *
 * Revision 6.12  2001/03/07 23:00:15  lavr
 * Default value for SConnNetInfo::stateless set to empty (FALSE)
 *
 * Revision 6.11  2001/03/02 20:07:07  lavr
 * Typos fixed
 *
 * Revision 6.10  2001/02/26 16:56:41  vakatov
 * Comment SConnNetInfo.
 *
 * Revision 6.9  2001/01/23 23:06:15  lavr
 * SConnNetInfo.debug_printout converted from boolean to enum
 * BUF_StripToPattern() introduced
 *
 * Revision 6.8  2001/01/11 23:05:13  lavr
 * ConnNetInfo_Create() fully documented
 *
 * Revision 6.7  2001/01/08 23:46:10  lavr
 * REQUEST_METHOD -> REQ_METHOD to be consistent with SConnNetInfo
 *
 * Revision 6.6  2001/01/08 22:47:13  lavr
 * ReqMethod constants changed (to conform to coding standard)
 * ClientMode removed; replaced by 2 booleans: stateless and firewall
 * in SConnInfo structure
 *
 * Revision 6.5  2000/12/29 17:47:46  lavr
 * NCBID stuff removed; ClientMode enum added;
 * ConnNetInfo_SetUserHeader added; http_user_header is now
 * included in ConnInfo structure. ConnNetInfo_Destroy parameter
 * changed to be a pointer (was a double pointer).
 *
 * Revision 6.4  2000/11/07 23:23:15  vakatov
 * In-sync with the C Toolkit "connutil.c:R6.15", "connutil.h:R6.13"
 * (with "eMIME_Dispatch" added).
 *
 * Revision 6.3  2000/10/05 22:39:21  lavr
 * SConnNetInfo modified to contain 'client_mode' instead of just 'firewall'
 *
 * Revision 6.2  2000/09/26 22:01:30  lavr
 * Registry entries changed, HTTP request method added
 *
 * Revision 6.1  2000/03/24 22:52:48  vakatov
 * Initial revision
 *
 * ==========================================================================
 */

#include <connect/ncbi_core.h>
#include <connect/ncbi_buffer.h>
#include <connect/ncbi_socket.h>
#include <connect/ncbi_connection.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif


typedef enum {
    eReqMethod_Any = 0,
    eReqMethod_Post,
    eReqMethod_Get
} EReqMethod;


typedef enum {
    eDebugPrintout_None = 0,
    eDebugPrintout_Some,
    eDebugPrintout_Data
} EDebugPrintout;


/* Network connection related configurable info struct.
 * ATTENTION:  Do NOT fill out this structure (SConnNetInfo) "from scratch"!
 *             Instead, use ConnNetInfo_Create() described below to create
 *             it, and then fix (hard-code) some fields, if really necessary.
 */
typedef struct {
    char           client_host[64];  /* effective client hostname            */
    char           host[64];         /* host to connect to                   */
    unsigned short port;             /* port to connect to, host byte order  */
    char           path[1024];       /* service: path(e.g. to  a CGI script) */
    char           args[1024];       /* service: args(e.g. for a CGI script) */
    EReqMethod     req_method;       /* method to use in the request         */
    STimeout*      timeout;          /* ptr to i/o tmo (infinite if NULL)    */
    unsigned int   max_try;          /* max. # of attempts to establish conn */
    char           http_proxy_host[64];  /* hostname of HTTP proxy server    */
    unsigned short http_proxy_port;      /* port #   of HTTP proxy server    */
    char           proxy_host[64];   /* host of CERN-like firewall proxy srv */
    EDebugPrintout debug_printout;   /* printout some debug info             */
    int/*bool*/    stateless;        /* to connect in HTTP-like fashion only */
    int/*bool*/    firewall;         /* to use firewall/relay in connects    */
    int/*bool*/    lb_disable;       /* to disable local load-balancing      */
    const char*    http_user_header; /* user header to add to HTTP request   */

    /* the following field(s) are for the internal use only! */
    int/*bool*/    http_proxy_adjusted;
    STimeout       tmo;              /* Default storage for finite timeout   */
} SConnNetInfo;


/* Defaults and the registry entry names for "SConnNetInfo" fields
 */
#define DEF_CONN_REG_SECTION      "CONN"
                                  
#define REG_CONN_HOST             "HOST"
#define DEF_CONN_HOST             "www.ncbi.nlm.nih.gov"
                                  
#define REG_CONN_PORT             "PORT"
#define DEF_CONN_PORT             80
                                  
#define REG_CONN_PATH             "PATH"
#define DEF_CONN_PATH             "/Service/dispd.cgi"
                                  
#define REG_CONN_ARGS             "ARGS"
#define DEF_CONN_ARGS             ""

#define REG_CONN_REQ_METHOD       "REQ_METHOD"
#define DEF_CONN_REQ_METHOD       "POST"
                                  
#define REG_CONN_TIMEOUT          "TIMEOUT"
#define DEF_CONN_TIMEOUT          30.0
                                  
#define REG_CONN_MAX_TRY          "MAX_TRY"
#define DEF_CONN_MAX_TRY          3
                                  
#define REG_CONN_HTTP_PROXY_HOST  "HTTP_PROXY_HOST"
#define DEF_CONN_HTTP_PROXY_HOST  ""
                                  
#define REG_CONN_HTTP_PROXY_PORT  "HTTP_PROXY_PORT"
#define DEF_CONN_HTTP_PROXY_PORT  80
                                  
#define REG_CONN_PROXY_HOST       "PROXY_HOST"
#define DEF_CONN_PROXY_HOST       ""
                                  
#define REG_CONN_DEBUG_PRINTOUT   "DEBUG_PRINTOUT"
#define DEF_CONN_DEBUG_PRINTOUT   ""
                                  
#define REG_CONN_STATELESS        "STATELESS"
#define DEF_CONN_STATELESS        ""

#define REG_CONN_FIREWALL         "FIREWALL"
#define DEF_CONN_FIREWALL         ""

#define REG_CONN_LB_DISABLE       "LB_DISABLE"
#define DEF_CONN_LB_DISABLE       ""


/* This function to fill out the "*info" structure using
 * registry entries named (see above) in macros REG_CONN_<NAME>:
 *
 *  -- INFO FIELD  ----- NAME --------------- REMARKS/EXAMPLES ---------
 *   client_host        <will be assigned to the local host name>
 *   host               HOST
 *   port               PORT
 *   path               PATH
 *   args               ARGS
 *   req_method         REQUEST_METHOD
 *   timeout            TIMEOUT     "<sec>.<usec>": "30.0", "0.005", "infinite"
 *   max_try            MAX_TRY  
 *   http_proxy_host    HTTP_PROXY_HOST no HTTP proxy if empty/NULL
 *   http_proxy_port    HTTP_PROXY_PORT
 *   proxy_host         PROXY_HOST
 *   debug_printout     DEBUG_PRINTOUT
 *   client_mode        CLIENT_MODE
 *   lb_disable         LB_DISABLE
 *
 * A value of the field NAME is first looked for in the environment variable
 * of the form service_CONN_NAME; then in the current corelib registry,
 * in the section 'service' by using key CONN_NAME; then in the environment
 * variable again, but using the name CONN_NAME; and finally in the default
 * registry section (DEF_CONN_REG_SECTION), using just NAME. If service
 * is NULL or empty then the first 2 steps in the above lookup are skipped.
 *
 * For default values see right above, in macros DEF_CONN_<NAME>.
 */
extern SConnNetInfo* ConnNetInfo_Create
(const char* service
 );


/* Adjust the "host:port" to "proxy_host:proxy_port", and
 * "path" to "http://host:port/path" to connect through a HTTP proxy.
 * Return FALSE if already adjusted(see the NOTE), or if cannot adjust
 * (e.g. if "host" + "path" are too long).
 * NOTE:  it does nothing if applied more then once to the same "info"(or its
 *        clone), or when "http_proxy_host" is NULL.
 */
extern int/*bool*/ ConnNetInfo_AdjustForHttpProxy
(SConnNetInfo* info
 );


/* Make an exact and independent copy of "*info".
 */
extern SConnNetInfo* ConnNetInfo_Clone
(const SConnNetInfo* info
 );


/* Set user header (discard previously set header, if any).
 */
extern void ConnNetInfo_SetUserHeader
(SConnNetInfo* info,
 const char*   user_header
 );


/* Printout the "*info" to file "fp".
 */
extern void ConnNetInfo_Print
(const SConnNetInfo* info,
 FILE*               fp
 );


/* Destroy and deallocate info if info is not NULL.
 */
extern void ConnNetInfo_Destroy
(SConnNetInfo* info
 );


/* Hit URL "http://host:port/path?args" with:
 *    {POST|GET} <path>?<args> HTTP/1.0\r\n
 *    <user_header\r\n>
 *    Content-Length: <content_length>\r\n\r\n
 * If "encode_args" is TRUE then URL-encode the "args".
 * "args" can be NULL/empty -- then the '?' symbol does not get added.
 * The "content_length" is mandatory, and it specifies an exact(!) amount
 * of data that you are planning to sent to the resultant socket.
 * If string "user_header" is not NULL/empty, then it must be terminated by a
 * single '\r\n'.
 * 
 * On success, return non-NULL handle to a readable & writable NCBI socket.
 * ATTENTION:  due to the very essence of the HTTP connection, you can
 *             perform only one { WRITE, ..., WRITE, READ, ..., READ } cycle.
 * On error, return NULL.
 *
 * NOTE: the socket must be closed by "ncbi_sock.h:SOCK_Close()" when not
 *       needed anymore.
 */

extern SOCK URL_Connect
(const char*     host,
 unsigned short  port,
 const char*     path,
 const char*     args,
 EReqMethod      req_method,
 size_t          content_length,
 const STimeout* c_timeout,       /* timeout for the CONNECT stage          */
 const STimeout* rw_timeout,      /* timeout for READ and WRITE             */
 const char*     user_header,
 int/*bool*/     encode_args,     /* URL-encode the "args", if any          */
 ESwitch         data_logging     /* sock.data log.; eDefault in most cases */
 );


/* Discard all input data before(and including) the first occurrence of
 * "pattern". If "buf" is not NULL then add the discarded data(including
 * the "pattern") to it. If "n_discarded" is not NULL then "*n_discarded"
 * will return # of discarded bytes.
 */
extern EIO_Status CONN_StripToPattern
(CONN        conn,
 const void* pattern,
 size_t      pattern_size,
 BUF*        buf,
 size_t*     n_discarded
 );

extern EIO_Status SOCK_StripToPattern
(SOCK        sock,
 const void* pattern,
 size_t      pattern_size,
 BUF*        buf,
 size_t*     n_discarded
 );

extern EIO_Status BUF_StripToPattern
(BUF         buffer,
 const void* pattern,
 size_t      pattern_size,
 BUF*        buf,
 size_t*     n_discarded
 );


/* URL-decode up to "src_size" symbols(bytes) from buffer "src_buf".
 * Write the decoded data to buffer "dst_buf", but no more than "dst_size"
 * bytes.
 * Assign "*src_read" to the # of bytes successfully decoded from "src_buf".
 * Assign "*dst_written" to the # of bytes written to buffer "dst_buf".
 * Return FALSE only if cannot decode nothing, and an unrecoverable
 * URL-encoding error (such as an invalid symbol or a bad "%.." sequence)
 * has occurred.
 * NOTE:  the unfinished "%.." sequence is fine -- return TRUE, but dont
 *        "read" it.
 */
extern int/*bool*/ URL_Decode
(const void* src_buf,    /* [in]     non-NULL */
 size_t      src_size,   /* [in]              */
 size_t*     src_read,   /* [out]    non-NULL */
 void*       dst_buf,    /* [in/out] non-NULL */
 size_t      dst_size,   /* [in]              */
 size_t*     dst_written /* [out]    non-NULL */
 );


/* Act just like URL_Decode (see above) but caller can allow the specified
 * non-standard URL symbols in the input buffer to be decoded "as is".
 * The extra allowed symbols are passed in a '\0'-terminated string
 * "allow_symbols" (it can be NULL or empty -- then this will be an exact
 * equivalent of URL_Decode).
 */
extern int/*bool*/ URL_DecodeEx
(const void* src_buf,      /* [in]     non-NULL  */
 size_t      src_size,     /* [in]               */
 size_t*     src_read,     /* [out]    non-NULL  */
 void*       dst_buf,      /* [in/out] non-NULL  */
 size_t      dst_size,     /* [in]               */
 size_t*     dst_written,  /* [out]    non-NULL  */
 const char* allow_symbols /* [in]     '\0'-term */
 );


/* URL-encode up to "src_size" symbols(bytes) from buffer "src_buf".
 * Write the encoded data to buffer "dst_buf", but no more than "dst_size"
 * bytes.
 * Assign "*src_read" to the # of bytes successfully encoded from "src_buf".
 * Assign "*dst_written" to the # of bytes written to buffer "dst_buf".
 */
extern void URL_Encode
(const void* src_buf,    /* [in]     non-NULL */
 size_t      src_size,   /* [in]              */
 size_t*     src_read,   /* [out]    non-NULL */
 void*       dst_buf,    /* [in/out] non-NULL */
 size_t      dst_size,   /* [in]              */
 size_t*     dst_written /* [out]    non-NULL */
 );



/****************************************************************************
 * NCBI-specific MIME content type and sub-types
 * (the API to compose and parse them)
 *    Content-Type: <type>/<MIME_ComposeSubType()>\r\n
 *
 *    Content-Type: <type>/<subtype>-<encoding>\r\n
 *
 * where  MIME_ComposeSubType(EMIME_SubType subtype, EMIME_Encoding encoding):
 *   "x-<subtype>-<encoding>":
 *     "x-<subtype>",   "x-<subtype>-urlencoded",   "x-<subtype>-<encoding>",
 *     "x-dispatch",    "x-dispatch-urlencoded",    "x-dispatch-<encoding>
 *     "x-asn-text",    "x-asn-text-urlencoded",    "x-asn-text-<encoding>
 *     "x-asn-binary",  "x-asn-binary-urlencoded",  "x-asn-binary-<encoding>"
 *     "x-www-form",    "x-www-form-urlencoded",    "x-www-form-<encoding>"
 *     "html",          "html-urlencoded",          "html-<encoding>"
 *     "x-unknown",     "x-unknown-urlencoded",     "x-unknown-<encoding>"
 *
 *  Note:  <subtype> and <encoding> are expected to contain only
 *         alphanumeric symbols, '-' and '_'. They are case-insensitive.
 ****************************************************************************/


/* Type
 */
typedef enum {
    eMIME_T_NcbiData = 0,  /* "x-ncbi-data"  (NCBI specific data) */
    eMIME_T_Text,          /* "text" */
    eMIME_T_Application,   /* "application" */
    /* eMIME_T_???, "<type>"  here go other types */
    eMIME_T_Unknown        /* "unknown" */
} EMIME_Type;


/* SubType
 */
typedef enum {
    eMIME_Dispatch = 0,  /* "x-dispatch"    (dispatcher info) */
    eMIME_AsnText,       /* "x-asn-text"    (text ASN.1 data) */
    eMIME_AsnBinary,     /* "x-asn-binary"  (binary ASN.1 data) */
    eMIME_Fasta,         /* "x-fasta"       (data in FASTA format) */
    eMIME_WwwForm,       /* "x-www-form" */
    /* standard MIMEs */
    eMIME_Html,          /* "html" */
    eMIME_Plain,         /* "plain" */
    /* eMIME_???,           "<subtype>"   here go other NCBI subtypes */
    eMIME_Unknown        /* "x-unknown"     (an arbitrary binary data) */
} EMIME_SubType;


/* Encoding
 */
typedef enum {
    eENCOD_Url = 0,  /* "-urlencoded"   (the content is URL-encoded) */
    /* eENCOD_???,      "-<encoding>"   here go other NCBI encodings */
    eENCOD_None      /* ""              (the content is passed "as is") */
} EMIME_Encoding;


/* Write up to "buflen" bytes to "buf":
 *   Content-Type: <type>/[x-]<subtype>-<encoding>\r\n
 * Return pointer to the "buf".
 */
#define MAX_CONTENT_TYPE_LEN 64
extern char* MIME_ComposeContentTypeEx
(EMIME_Type     type,
 EMIME_SubType  subtype,
 EMIME_Encoding encoding,
 char*          buf,
 size_t         buflen    /* must be at least MAX_CONTENT_TYPE_LEN */
 );

/* Exactly equivalent to MIME_ComposeContentTypeEx(eMIME_T_NcbiData, ...)
 */
extern char* MIME_ComposeContentType
(EMIME_SubType  subtype,
 EMIME_Encoding encoding,
 char*          buf,
 size_t         buflen
 );


/* Parse the NCBI-specific content-type; the (case-insensitive) "str"
 * can be in the following two formats:
 *   Content-Type: <type>/x-<subtype>-<encoding>
 *   <type>/x-<subtype>-<encoding>
 *
 * NOTE:  all leading spaces and all trailing spaces (and any trailing symbols,
 *        if they separated from the content type by at least one space) will
 *        be ignored, e.g. these are valid content type strings:
 *           "   Content-Type: text/plain  foobar"
 *           "  text/html \r\n  barfoo coocoo ....\n boooo"
 *
 * If it does not match any of NCBI MIME type/subtypes/encodings, then
 * return TRUE, eMIME_T_Unknown, eMIME_Unknown or eENCOD_None, respectively.
 * If the passed "str" has an invalid (non-HTTP ContentType) format
 * (or if it is NULL/empty), then
 * return FALSE, eMIME_T_Unknown, eMIME_Unknown, and eENCOD_None.
 */
extern int/*bool*/ MIME_ParseContentTypeEx
(const char*     str,      /* the HTTP "Content-Type:" header to parse */
 EMIME_Type*     type,     /* can be NULL */
 EMIME_SubType*  subtype,  /* can be NULL */
 EMIME_Encoding* encoding  /* can be NULL */
 );

/* Requires the MIME type be "x-ncbi-data"
 */
extern int/*bool*/ MIME_ParseContentType
(const char*     str,      /* the HTTP "Content-Type:" header to parse */
 EMIME_SubType*  subtype,  /* can be NULL */
 EMIME_Encoding* encoding  /* can be NULL */
 );


/* Read (skipping leading blanks) "[host][:port]" from a string.
 * On success, return the advanced pointer past the host/port read.
 * If no host/port detected, return 'str'.
 * On format error, return 0.
 * If host and/or port fragments are missing,
 * then corresponding 'host'/'port' value returned as 0.
 * Note that 'host' returned is in network byte order,
 * unlike 'port', which always comes out in host (native) byte order.
 */
extern const char* StringToHostPort
(const char*     str,   /* must not be NULL */
 unsigned int*   host,  /* must not be NULL */
 unsigned short* port   /* must not be NULL */
 );


/* Print host:port into provided buffer string, not to exceed 'buflen' size.
 * Suppress printing host if parameter 'host' is zero.
 * Return the number of bytes printed.
 */
extern size_t HostPortToString
(unsigned int   host,
 unsigned short port,
 char*          buf,
 size_t         buflen
 );


#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif /* NCBI_CONNUTIL__H */
