#ifndef CONNECT___NCBI_SERVICE_MISC__H
#define CONNECT___NCBI_SERVICE_MISC__H

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
 *   Top-level API to resolve NCBI service name to the server meta-address:
 *   miscellanea.
 *
 */

#include <connect/ncbi_types.h>


/** @addtogroup ServiceSupport
 *
 * @{
 */


#ifdef __cplusplus
extern "C" {
#endif


#ifdef __cplusplus
}  /* extern "C" */
#endif


/* @} */


/*
 * --------------------------------------------------------------------------
 * $Log$
 * Revision 1.7  2006/11/08 17:01:02  lavr
 * LBSMD_KeepHeapAttached():  Retired
 *
 * Revision 1.6  2006/04/19 14:44:28  lavr
 * Retire deprecated deprecated LBSM_KeepHeapAttached()
 *
 * Revision 1.5  2006/03/16 19:02:08  lavr
 * Scream about LBSMD_KeepHeapAttached
 *
 * Revision 1.4  2006/03/05 17:34:25  lavr
 * LBSM_KeepHeapAttached -> LBSMD_KeepHeapAttached
 *
 * Revision 1.3  2006/01/17 20:17:15  lavr
 * DISP_SetMessageHook() moved from ncbi_service_misc.h to
 * ncbi_http_connector.h and renamed to HTTP_SetNcbiMessageHook()
 *
 * Revision 1.2  2005/12/23 18:08:02  lavr
 * FDISP_MessageHook documented better
 *
 * Revision 1.1  2005/05/04 16:13:14  lavr
 * Initial revision
 *
 * ==========================================================================
 */

#endif /* CONNECT___NCBI_SERVICE_MISC__H */
