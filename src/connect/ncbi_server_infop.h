#ifndef NCBI_SERVER_INFOP__H
#define NCBI_SERVER_INFOP__H

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
 * Author:  Anton Lavrentiev, Denis Vakatov
 *
 * File Description:
 *   NCBI server meta-address info (private part)
 *
 * --------------------------------------------------------------------------
 * $Log$
 * Revision 6.1  2001/11/16 20:25:53  lavr
 * +g_SERV_LocalServerDefault as a private global parameter
 *
 * ==========================================================================
 */

#ifdef __cplusplus
extern "C" {
#endif


extern int/*bool*/ g_SERV_LocalServerDefault;


#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif /* NCBI_SERVER_INFOP__H */
