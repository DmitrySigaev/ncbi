#ifndef DBAPI_DRIVER_CTLIB___DBAPI_DRIVER_CTLIB_UTILS__HPP
#define DBAPI_DRIVER_CTLIB___DBAPI_DRIVER_CTLIB_UTILS__HPP

/* $Id$
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
 * Author:  Sergey Sikorskiy
 *
 * File Description:  Small utility classes common to the ctlib driver.
 *
 */

#include <dbapi/driver/impl/dbapi_driver_utils.hpp>


BEGIN_NCBI_SCOPE


/////////////////////////////////////////////////////////////////////////////
extern NCBI_DBAPIDRIVER_CTLIB_EXPORT const string kDBAPI_CTLIB_DriverName;

extern "C"
{

NCBI_DBAPIDRIVER_CTLIB_EXPORT
void
NCBI_EntryPoint_xdbapi_ctlib(
    CPluginManager<I_DriverContext>::TDriverInfoList&   info_list,
    CPluginManager<I_DriverContext>::EEntryPointRequest method);

NCBI_DBAPIDRIVER_CTLIB_EXPORT
void
NCBI_EntryPoint_xdbapi_ftds64_ctlib(
    CPluginManager<I_DriverContext>::TDriverInfoList&   info_list,
    CPluginManager<I_DriverContext>::EEntryPointRequest method);

} // extern C


/////////////////////////////////////////////////////////////////////////////
// Singleton

CDBExceptionStorage& GetCTLExceptionStorage(void);


END_NCBI_SCOPE


#endif // DBAPI_DRIVER_CTLIB___DBAPI_DRIVER_CTLIB_UTILS__HPP

/*
 * ===========================================================================
 * $Log$
 * Revision 1.3  2006/08/02 15:13:14  ssikorsk
 * + NCBI_EntryPoint_xdbapi_ftds64_ctlib
 *
 * Revision 1.2  2006/07/12 16:29:30  ssikorsk
 * Separated interface and implementation of CDB classes.
 *
 * Revision 1.1  2006/05/11 18:02:57  ssikorsk
 * Added GetCTLExceptionStorage function
 *
 * ===========================================================================
 */
