#ifndef NETSTORAGE_VERSION__HPP
#define NETSTORAGE_VERSION__HPP

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
 * Authors:  Denis Vakatov
 *
 * File Description: Network Storage middle man server versions
 *
 */
#include <common/ncbi_package_ver.h>


#define NETSTORAGE_STRINGIFY(x)    #x
#define NETSTORAGE_VERSION_COMPOSE_STR(a, b, c)  \
            NETSTORAGE_STRINGIFY(a) "." \
            NETSTORAGE_STRINGIFY(b) "." \
            NETSTORAGE_STRINGIFY(c)

#define NETSTORAGED_VERSION        NCBI_PACKAGE_VERSION
#define NETSTORAGED_BUILD_DATE     __DATE__ " " __TIME__

// Protocol
#define NETSTORAGED_PROTOCOL_VERSION_MAJOR 1
#define NETSTORAGED_PROTOCOL_VERSION_MINOR 0
#define NETSTORAGED_PROTOCOL_VERSION_PATCH 0
#define NETSTORAGED_PROTOCOL_VERSION       \
            NETSTORAGE_VERSION_COMPOSE_STR( \
                NETSTORAGED_PROTOCOL_VERSION_MAJOR, \
                NETSTORAGED_PROTOCOL_VERSION_MINOR, \
                NETSTORAGED_PROTOCOL_VERSION_PATCH)


#define NETSTORAGED_FULL_VERSION \
    "NCBI NetStorage server Version " NETSTORAGED_VERSION \
    " Protocol version " NETSTORAGED_PROTOCOL_VERSION \
    " build " NETSTORAGED_BUILD_DATE


#endif /* NETSTORAGE_VERSION__HPP */

