#ifndef CONNECT___NCBI_LBSMD__H
#define CONNECT___NCBI_LBSMD__H

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
 *   Low-level API to resolve NCBI service name to the server meta-address
 *   with the use of NCBI Load-Balancing Service Mapper (LBSMD).
 *
 */

#include "ncbi_servicep.h"


#ifdef __cplusplus
extern "C" {
#endif


const SSERV_VTable* SERV_LBSMD_Open(SERV_ITER iter,
                                    SSERV_Info** info, HOST_INFO* host_info);


char* SERV_LBSMD_GetConfig(void);


int LBSM_HINFO_CpuCount(const void* load);


int LBSM_HINFO_TaskCount(const void* load);


int/*bool*/ LBSM_HINFO_LoadAverage(const void* load, double lavg[2]);


int/*bool*/ LBSM_HINFO_Status(const void* load, double status[2]);


int/*bool*/ LBSM_HINFO_BLASTParams(const void* load, unsigned int blast[8]);


#ifdef __cplusplus
}  /* extern "C" */
#endif


/*
 * --------------------------------------------------------------------------
 * $Log$
 * Revision 6.8  2002/10/28 20:12:57  lavr
 * Module renamed and host info API included
 *
 * Revision 6.7  2002/10/11 19:52:45  lavr
 * +SERV_LBSMD_GetConfig()
 *
 * Revision 6.6  2002/09/19 18:09:02  lavr
 * Header file guard macro changed; log moved to end
 *
 * Revision 6.5  2002/04/13 06:40:28  lavr
 * Few tweaks to reduce the number of syscalls made
 *
 * Revision 6.4  2001/04/24 21:31:22  lavr
 * SERV_LBSMD_LOCAL_SVC_BONUS moved to .c file
 *
 * Revision 6.3  2000/12/29 18:19:12  lavr
 * BONUS added for services running locally.
 *
 * Revision 6.2  2000/05/22 16:53:13  lavr
 * Rename service_info -> server_info everywhere (including
 * file names) as the latter name is more relevant
 *
 * Revision 6.1  2000/05/12 18:39:18  lavr
 * First working revision
 *
 * ==========================================================================
 */

#endif /* CONNECT___NCBI_LBSMD__H */
