#ifndef NCBI_COMM__H
#define NCBI_COMM__H

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
 *   Common part of internal communication protocol used by both sides
 *   (client and server) of firewall daemon and service dispatcher.
 *
 * --------------------------------------------------------------------------
 * $Log$
 * Revision 6.2  2001/01/08 22:34:45  lavr
 * Request-Failed added to protocol
 *
 * Revision 6.1  2000/12/29 18:20:26  lavr
 * Initial revision
 *
 * ==========================================================================
 */

#define NCBID_NAME              "/Service/ncbid.cgi"
#define HTTP_CONNECTION_INFO    "Connection-Info:"
#define HTTP_REQUEST_FAILED     "Request-Failed:"

typedef unsigned int ticket_t;

#endif /* NCBI_COMM__H */
