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
 *   Standard test for the CONN-based streams
 *
 * --------------------------------------------------------------------------
 * $Log$
 * Revision 6.3  2001/01/13 00:01:26  lavr
 * Changes in REG_cxx2c() prototype -> appropriate changes in the test
 * Explicit registry at the end
 *
 * Revision 6.2  2001/01/12 05:49:31  vakatov
 * Get rid of unused "argc", "argv" in main()
 *
 * Revision 6.1  2001/01/11 23:09:36  lavr
 * Initial revision
 *
 * ==========================================================================
 */

#if defined(NDEBUG)
#  undef NDEBUG
#endif 

#include <connect/ncbi_util.h>
#include <connect/ncbi_conn_stream.hpp>
#include <connect/ncbi_core_cxx.hpp>
#include <stdlib.h>
#include <string.h>


BEGIN_NCBI_SCOPE

static CNcbiRegistry* s_CreateRegistry(void)
{
    CNcbiRegistry* reg = new CNcbiRegistry;

    // Compose a test registry
    reg->Set(DEF_CONN_REG_SECTION, REG_CONN_DEBUG_PRINTOUT, "TRUE");
    reg->Set(DEF_CONN_REG_SECTION, REG_CONN_HOST,           "ray");
    reg->Set(DEF_CONN_REG_SECTION, REG_CONN_PATH,   "/Service/io_bounce.cgi");
    reg->Set(DEF_CONN_REG_SECTION, REG_CONN_ARGS,           "arg1+arg2+arg3");
    reg->Set(DEF_CONN_REG_SECTION, REG_CONN_REQ_METHOD,     "POST");
    reg->Set(DEF_CONN_REG_SECTION, REG_CONN_TIMEOUT,        "5.0");
    reg->Set(DEF_CONN_REG_SECTION, REG_CONN_DEBUG_PRINTOUT, "TRUE");
    return reg;
}

const size_t kBufferSize = 300*1024;

END_NCBI_SCOPE


int main(void)
{
    USING_NCBI_SCOPE;

    SetDiagTrace(eDT_Enable);
    CORE_SetLOGFILE(stderr, 0/*false*/);
    CORE_SetREG(REG_cxx2c(s_CreateRegistry(), true));

    CConn_HttpStream ios(0, "User-Header: My header\r\n", fHCC_UrlEncodeArgs);

    char *buf1 = new char[kBufferSize + 1];
    char *buf2 = new char[kBufferSize + 1];
    
    for (size_t j = 0; j < kBufferSize/1024; j++) {
        for (size_t i = 0; i < 1024 - 1; i++)
            buf1[j*1024 + i] = "0123456789"[rand() % 10];
        buf1[j*1024 + 1023] = '\n';
    }
    buf1[kBufferSize] = '\0';

    if (!(ios << buf1)) {
        cerr << "Error sending data" << endl;
        return 1;
    }

    ios.read(buf2, kBufferSize);

    if (!ios.good() && !ios.eof()) {
        cerr << "Error receiving data" << endl;
        return 2;
    }

    size_t buflen = ios.gcount();
    cerr << buflen << " bytes obtained" << endl;

    buf2[buflen] = '\0';
   
    size_t len = strlen(buf1);
    size_t i;

    for (i = 0; i < len; i++) {
        if (!buf2[i])
            break;
        if (buf2[i] != buf1[i])
            break;
    }
    if (i < len)
        cerr << "Not entirely bounced " << i+1 << endl;
    else
        cerr << "Reply received okay!" << endl;

    CORE_SetREG(0);

    return 0/*okay*/;
}
