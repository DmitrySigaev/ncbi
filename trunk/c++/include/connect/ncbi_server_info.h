#ifndef NCBI_SERVICE_INFO__H
#define NCBI_SERVICE_INFO__H

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
 *   NCBI service meta-address info
 *   Note that all service meta-addresses are allocated as
 *   single contiguous pieces of memory, which can be copied in whole
 *   with the use of 'SERV_SizeOfInfo' call. Dynamically allocated
 *   service infos can be freed with a direct call to 'free'.
 *
 * --------------------------------------------------------------------------
 * $Log$
 * Revision 6.5  2000/05/15 19:06:05  lavr
 * Use home-made ANSI extentions (NCBI_***)
 *
 * Revision 6.4  2000/05/12 21:42:11  lavr
 * SSERV_Info::  use ESERV_Type, ESERV_Flags instead of TSERV_Type, TSERV_Flags
 *
 * Revision 6.3  2000/05/12 18:31:48  lavr
 * First working revision
 *
 * Revision 6.2  2000/05/09 15:31:29  lavr
 * Minor changes
 *
 * Revision 6.1  2000/05/05 20:24:00  lavr
 * Initial revision
 *
 * ==========================================================================
 */

#include <stddef.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif


/* Bit-mask of service types
 */
typedef enum {
    fSERV_Ncbid      = 0x1,
    fSERV_Standalone = 0x2,
    fSERV_HttpGet    = 0x4,
    fSERV_HttpPost   = 0x8,
    fSERV_Http       = fSERV_HttpGet | fSERV_HttpPost
} ESERV_Type;
typedef int TSERV_Type;  /*  bit-wise OR of "ESERV_Type" flags */


/* Flags to specify the algorithm for selecting the most preferred
 * service from the set of available services
 */
typedef enum {
    fSERV_Regular = 0x0,
    fSERV_Blast   = 0x1
} ESERV_Flags;
typedef int TSERV_Flags;


#define SERV_DEFAULT_FLAG       fSERV_Regular


/* Verbal representation of a service type (no internal spaces allowed)
 */
const char* SERV_TypeStr(ESERV_Type type);


/* Read service info type.
 * If successful, assign "type" and return pointer to the position
 * in the "str" immediately following the type tag.
 * On error, return NULL.
 */
const char* SERV_ReadType(const char* str, ESERV_Type* type);


/* Meta-addresses for various types of NCBI services
 */
typedef struct {
    size_t         args;
#define SERV_NCBID_ARGS(i)      ((char *)(i) + (i)->args)
} SSERV_NcbidInfo;

typedef struct {
    int            dummy;       /* placeholder, not used */
} SSERV_StandaloneInfo;

typedef struct {
    size_t         path;
    size_t         args;
#define SERV_HTTP_PATH(i)       ((char *)(i) + (i)->path)
#define SERV_HTTP_ARGS(i)       ((char *)(i) + (i)->args)
} SSERV_HttpInfo;


/* Generic NCBI service meta-address
 */
typedef union {
    SSERV_NcbidInfo      ncbid;
    SSERV_StandaloneInfo standalone;
    SSERV_HttpInfo       http;
} USERV_Info;

typedef struct {
    ESERV_Type     type;
    unsigned int   host;
    unsigned short port;
    ESERV_Flags    flag;
    time_t         time;
    USERV_Info     u;
} SSERV_Info;


/* Constructors for the various types of NCBI services' meta-addresses
 */
SSERV_Info* SERV_CreateNcbidInfo
(unsigned int   host,
 unsigned short port,
 const char*    args
 );

SSERV_Info* SERV_CreateStandaloneInfo
(unsigned int   host,
 unsigned short port
 );

SSERV_Info* SERV_CreateHttpInfo
(ESERV_Type     type,           /* Verified, must be one of fSERV_Http* */
 unsigned int   host,
 unsigned short port,
 const char*    path,
 const char*    args
 );


/* Dump service info to a string.
 * The service type goes first, and it is followed by a single space.
 * The returned string is '\0'-terminated, and must be deallocated by 'free()'.
 * If 'skip_host' is true, no host address is put to the string
 * (neither it will be if the service info has the address as 0).
 */
char* SERV_WriteInfo(const SSERV_Info* info, int/*bool*/ skip_host);


/* Read full service info (including type) from string "str"
 * (e.g. composed by SERV_WriteInfo). Result can be later freed by 'free()'.
 * If 'default_host' is not 0, the service info will be assigned this
 * value, and a host address should NOT be specified in the input textual
 * service representation.
 */
SSERV_Info* SERV_ReadInfo(const char* info_str, unsigned int default_host);


/* Return an actual size (in bytes) the service info occupies
 * (to be used for copying info structures in whole).
 */
size_t SERV_SizeOfInfo(const SSERV_Info* info);


/* Return 'true' if two service infos are equal.
 */
int/*bool*/ SERV_EqualInfo(const SSERV_Info* info1, const SSERV_Info* info2);


#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif /* NCBI_SERVICE_INFO__H */
