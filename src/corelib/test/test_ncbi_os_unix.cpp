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
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbidiag.hpp>
#include <corelib/ncbi_os_unix.hpp>
#include <stdio.h>
#include <test/test_assert.h>  /* This header must go last */


int main()
{
    USING_NCBI_SCOPE;

    // Set err.-posting and tracing to maximum
    SetDiagTrace(eDT_Enable);
    SetDiagPostFlag(eDPF_All);
    SetDiagPostLevel(eDiag_Info);

    // Run tests
    _ASSERT(Daemonize("/test_ncbi_os_unix.log") == false);
    _ASSERT(Daemonize("./test_ncbi_os_unix.log",
                      fDaemon_DontChroot | fDaemon_KeepStdout) == true);
    _ASSERT(access("./test_ncbi_os_unix.log", F_OK) == 0);

    LOG_POST("TEST COMPLETED SUCCESSFULLY");
    sleep(60);
    remove("./test_ncbi_os_unix.log");

    return 0;
}


/*
 * ===========================================================================
 * $Log$
 * Revision 1.3  2004/05/14 13:59:51  gorelenk
 * Added include of ncbi_pch.hpp
 *
 * Revision 1.2  2003/10/01 12:04:29  ucko
 * +<stdio.h> for remove()
 *
 * Revision 1.1  2003/09/30 20:54:45  lavr
 * Initial revision
 *
 * ===========================================================================
 */
