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
 *   Test UTIL_PushbackStream() interface.
 *
 */

#include "pbacktest.hpp"
#include <corelib/ncbidiag.hpp>
#include <stdio.h>                 // remove()

#include <test/test_assert.h>  /* This header must go last */


int main(int argc, char* argv[])
{
    USING_NCBI_SCOPE;
    static const char filename[] = "test_fstream_pushback.data";

    SetDiagTrace(eDT_Enable);
    SetDiagPostLevel(eDiag_Info);
    SetDiagPostFlag(eDPF_All);

    fstream fs(filename,
               IOS_BASE::in    | IOS_BASE::out   |
               IOS_BASE::trunc | IOS_BASE::binary);

    int ret = TEST_StreamPushback(fs,
                                  argc > 1 ? (unsigned int) atoi(argv[1]) : 0,
                                  true/*rewind*/);
    if (ret == 0)
        remove(filename);
    return ret;
}


/*
 * ==========================================================================
 * $Log$
 * Revision 1.6  2002/04/16 18:52:15  ivanov
 * Centralize threatment of assert() in tests.
 * Added #include <test/test_assert.h>. CVS log moved to end of file.
 *
 * Revision 1.5  2002/03/19 21:57:19  lavr
 * +IOS_BASE::binary in file open
 *
 * Revision 1.4  2002/02/05 21:45:19  lavr
 * Included header files rearranged
 *
 * Revision 1.3  2002/02/05 16:06:41  lavr
 * List of included header files revised; Use macro IOS_BASE instead of raw ios
 *
 * Revision 1.2  2002/02/04 20:24:04  lavr
 * Remove data file if successful
 *
 * Revision 1.1  2002/01/29 16:02:19  lavr
 * Initial revision
 *
 * ==========================================================================
 */
