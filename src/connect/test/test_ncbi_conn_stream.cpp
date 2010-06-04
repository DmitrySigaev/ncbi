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
 * Author:  Anton Lavrentiev
 *
 * File Description:
 *   Standard test for the CONN-based streams
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbi_process.hpp>
#include <connect/ncbi_conn_stream.hpp>
#include <connect/ncbi_socket.hpp>
#include <stdlib.h>
#include <time.h>
#include "../ncbi_ansi_ext.h"
#include "../ncbi_priv.h"               /* CORE logging facilities */
#include <common/test_assert.h>  // This header must go last

#ifdef NCBI_OS_UNIX
#  define DEV_NULL "/dev/null"
#else
#  define DEV_NULL "NUL"
#endif // NCBI_OS_UNIX


static const size_t kBufferSize = 512*1024;


BEGIN_NCBI_SCOPE


static CNcbiRegistry* s_CreateRegistry(void)
{
    CNcbiRegistry* reg = new CNcbiRegistry;

    // Compose a test registry
    reg->Set("ID1", DEF_CONN_REG_SECTION "_" REG_CONN_HOST, DEF_CONN_HOST);
    reg->Set("ID1", DEF_CONN_REG_SECTION "_" REG_CONN_PATH, DEF_CONN_PATH);
    reg->Set("ID1", DEF_CONN_REG_SECTION "_" REG_CONN_ARGS, DEF_CONN_ARGS);
    reg->Set(DEF_CONN_REG_SECTION, REG_CONN_HOST,     "www.ncbi.nlm.nih.gov");
    reg->Set(DEF_CONN_REG_SECTION, REG_CONN_PATH,      "/Service/bounce.cgi");
    reg->Set(DEF_CONN_REG_SECTION, REG_CONN_ARGS,           "arg1+arg2+arg3");
    reg->Set(DEF_CONN_REG_SECTION, REG_CONN_REQ_METHOD,     "POST");
    reg->Set(DEF_CONN_REG_SECTION, REG_CONN_TIMEOUT,        "5.0");
    reg->Set(DEF_CONN_REG_SECTION, REG_CONN_DEBUG_PRINTOUT, "TRUE");
    return reg;
}


END_NCBI_SCOPE


int main(int argc, const char* argv[])
{
    USING_NCBI_SCOPE;
    CNcbiRegistry* reg;
    TFCDC_Flags flags = 0;
    SConnNetInfo* net_info;
    size_t i, j, k, l, m, n, size;

    CORE_SetLOGFormatFlags(fLOG_None          | fLOG_Level   |
                           fLOG_OmitNoteLevel | fLOG_DateTime);
    CORE_SetLOGFILE(stderr, 0/*false*/);
    if (argc <= 1)
        g_NCBI_ConnectRandomSeed = (int) time(0) ^ NCBI_CONNECT_SRAND_ADDEND;
    else
        g_NCBI_ConnectRandomSeed = atoi(argv[1]);
    CORE_LOGF(eLOG_Note, ("Random SEED = %u", g_NCBI_ConnectRandomSeed));
    srand(g_NCBI_ConnectRandomSeed);

    // Set error posting and tracing on maximum
    SetDiagTrace(eDT_Enable);
    SetDiagPostAllFlags(eDPF_All | eDPF_OmitInfoSev);
    UnsetDiagPostFlag(eDPF_Line);
    UnsetDiagPostFlag(eDPF_File);
    UnsetDiagPostFlag(eDPF_Location);
    UnsetDiagPostFlag(eDPF_LongFilename);
    SetDiagPostLevel(eDiag_Info);
    reg = s_CreateRegistry();
    CONNECT_Init(reg);


    LOG_POST(Info << "Test 0 of 7: Checking error log setup");
    ERR_POST(Info << "Test log message using C++ Toolkit posting");
    CORE_LOG(eLOG_Note, "Another test message using C Toolkit posting");
    LOG_POST(Info << "Test 0 passed\n");


    LOG_POST("Test 1 of 7: Memory stream");
    // Testing memory stream out-of-sequence interleaving operations
    m = (rand() & 0x00FF) + 1;
    size = 0;
    for (n = 0;  n < m;  n++) {
        CConn_MemoryStream* ms = 0;
        string data, back;
        size_t sz = 0;
#if 0
        LOG_POST("  Micro-test " << (int) n << " of " << (int) m << " start");
#endif
        k = (rand() & 0x00FF) + 1;
        for (i = 0;  i < k;  i++) {
            l = (rand() & 0x00FF) + 1;
            string bit;
            bit.resize(l);
            for (j = 0;  j < l;  j++) {
                bit[j] = "0123456789"[rand() % 10];
            }
#if 0
            LOG_POST("    Data bit at " << (unsigned long) sz <<
                     ", " << (unsigned long) l << " byte(s) long: " << bit);
#endif
            sz += l;
            data += bit;
            if (ms)
                assert(*ms << bit);
            else if (i == 0) {
                switch (n % 4) {
                case 0:
#if 0
                    LOG_POST("  CConn_MemoryStream()");
#endif
                    ms = new CConn_MemoryStream;
                    assert(*ms << bit);
                    break;
                case 1:
                {{
                    BUF buf = 0;
                    assert(BUF_Write(&buf, bit.data(), l));
#if 0
                    LOG_POST("  CConn_MemoryStream(BUF)");
#endif
                    ms = new CConn_MemoryStream(buf, eTakeOwnership);
                    break;
                }}
                default:
                    break;
                }
            }
        }
        switch (n % 4) {
        case 2:
#if 0
            LOG_POST("  CConn_MemoryStream(" <<
                     (unsigned long) data.size() << ')');
#endif
            ms = new CConn_MemoryStream(data.data(),data.size(), eNoOwnership);
            break;
        case 3:
        {{
            BUF buf = 0;
            assert(BUF_Append(&buf, data.data(), data.size()));
#if 0
            LOG_POST("  CConn_MemoryStream(BUF, " <<
                     (unsigned long) data.size() << ')');
#endif
            ms = new CConn_MemoryStream(buf, eTakeOwnership);
            break;
        }}
        default:
            break;
        }
        assert(ms);
        if (!(rand() & 1)) {
            assert(*ms << endl);
            *ms >> back;
            assert(ms->good());
            SetDiagTrace(eDT_Disable);
            *ms >> ws;
            SetDiagTrace(eDT_Enable);
            ms->clear();
        } else
            ms->ToString(&back);
#if 0
        LOG_POST("  Data size=" << (unsigned long) data.size());
        LOG_POST("  Back size=" << (unsigned long) back.size());
        for (i = 0;  i < data.size()  &&  i < back.size(); i++) {
            if (data[i] != back[i]) {
                LOG_POST("  Data differ at pos " << (unsigned long) i <<
                         ": '" << data[i] << "' vs '" << back[i] << '\'');
                break;
            }
        }
#endif
        assert(back == data);
        size += sz;
        delete ms;
#if 0
        LOG_POST("  Micro-test " << (int) n << " of " << (int) m << " finish");
#endif
    }
    LOG_POST("Test 1 passed in " <<
             (int) m    << " iteration(s) with " <<
             (int) size << " byte(s) transferred\n");


    LOG_POST("Test 2 of 7:  FTP download");
    if (!(net_info = ConnNetInfo_Create(0)))
        ERR_POST(Fatal << "Cannot create net info");
    if (net_info->debug_printout == eDebugPrintout_Some)
        flags |= fFCDC_LogControl;
    else {
        char val[32];
        ConnNetInfo_GetValue(0, REG_CONN_DEBUG_PRINTOUT, val, sizeof(val),
                             DEF_CONN_DEBUG_PRINTOUT);
        flags |= strcasecmp(val, "all") == 0 ? fFCDC_LogAll : fFCDC_LogData;
    }
    CConn_FTPDownloadStream ftp("ftp.ncbi.nlm.nih.gov",
                                "Misc/test_ncbi_conn_stream.FTP.data",
                                "ftp"/*default*/, "-none"/*default*/,
                                "/toolbox/ncbi_tools++/DATA",
                                0/*port = default*/, flags,
                                0/*offset*/, net_info->timeout);

    for (size = 0;  ftp.good();  size += ftp.gcount()) {
        char buf[512];
        ftp.read(buf, sizeof(buf));
    }
    ftp.Close();
    if (size) {
        LOG_POST("Test 2 passed: "
                 << size << " bytes downloaded via FTP\n");
    } else
        ERR_POST(Fatal << "Test 2 failed: no file downloaded");


    LOG_POST("Test 3 of 7:  FTP upload");
    ifstream ifs("/am/ncbiapdata/test_data/ftp/test_ncbi_ftp_upload");
    if (ifs) {
        string src;
        ifs >> src;
        ifs.close();
        string dst(src.size(), '\0');
        size_t n_read, n_written;
        BASE64_Decode(src.c_str(), src.size(), &n_read,
                      &dst[0], dst.size(), &n_written);
        dst.resize(n_written);
        size_t cpos = dst.find_first_of(':');
        string user, pass;
        if (cpos != NPOS) {
            user = dst.substr(0, cpos);
            pass = dst.substr(cpos + 1);
        }
        CTime  start(CTime::eCurrent);
        string filename("test_ncbi_conn_stream");
        filename += '-' + CSocketAPI::gethostname();
        filename += '-' + NStr::UInt8ToString(CProcess::GetCurrentPid());
        filename += '-' + start.AsString("YMDhms");
        filename += ".tmp";
        // to use advanced xfer modes if available
        flags    |= fFCDC_UseFeatures;
        CConn_FTPUploadStream upload("ftp-private.ncbi.nlm.nih.gov",
                                     user, pass, filename, "test_upload",
                                     0/*port = default*/, flags,
                                     0/*offset*/, net_info->timeout);
        size = 0;
        while (size < (10<<20)  &&  upload.good()) {
            char buf[4096];
            size_t n = (size_t) rand() % sizeof(buf) + 1;
            for (size_t i = 0;  i < n;  i++)
                buf[i] = rand() & 0xFF;
            if (upload.write(buf, n))
                size += n;
        }
        unsigned long val = 0;
        unsigned long filesize = 0;
        if (upload) {
            upload >> val;
        }
        if (size  &&  val == (unsigned long) size) {
            string speedstr;
            string filetime;
            upload.clear();
            upload << "SIZE " << filename << NcbiEndl;
            upload >> filesize;
            upload.clear();
            upload << "MDTM " << filename << NcbiEndl;
            upload >> filetime;
            if (!filetime.empty()) {
                time_t stop = (time_t) NStr::StringToUInt(filetime);
                time_t time = stop - start.GetTimeT();
                double rate = (val / 1024.0) / (time ? time : 1);
                speedstr = (" in "
                            + NStr::UIntToString(time)
                            + " sec @ "
                            + NStr::DoubleToString(rate, 2, NStr::fDoubleFixed)
                            + " KB/s");
            }
            LOG_POST("Test 3 passed: " <<
                     size << " bytes uploaded via FTP" <<
                     (val == filesize ? " and verified" : "") <<
                     speedstr << '\n');
        } else {
            ERR_POST(Fatal << "Test 3 failed: " <<
                     val << " out of " << size << " byte(s) uploaded");
        }
        upload.clear();
        upload << "DELE " << filename << NcbiEndl;
    } else
        LOG_POST("Test 3 skipped\n");

    ConnNetInfo_Destroy(net_info);


    {{
        // Silent test for timeouts and memory leaks in an unused stream
        STimeout tmo = {8, 0};
        CConn_IOStream* s  =
            new CConn_ServiceStream("ID1", fSERV_Any, 0, 0, &tmo);
        delete s;
    }}


    LOG_POST("Test 4 of 7: Big buffer bounce");
    CConn_HttpStream ios(0, "User-Header: My header\r\n", 0, 0, 0, 0,
                         fHCC_UrlEncodeArgs | fHCC_AutoReconnect |
                         fHCC_Flushable);

    char *buf1 = new char[kBufferSize + 1];
    char *buf2 = new char[kBufferSize + 2];

    for (j = 0; j < kBufferSize/1024; j++) {
        for (i = 0; i < 1024 - 1; i++)
            buf1[j*1024 + i] = "0123456789"[rand() % 10];
        buf1[j*1024 + 1024 - 1] = '\n';
    }
    buf1[kBufferSize] = '\0';

    if (!(ios << buf1)) {
        ERR_POST("Error sending data");
        return 1;
    }
    assert(ios.tellp() == (CT_POS_TYPE)((CT_OFF_TYPE)(kBufferSize)));

    if (!ios.flush()) {
        ERR_POST("Error flushing data");
        return 1;
    }

    ios.read(buf2, kBufferSize + 1);
    streamsize buflen = ios.gcount();

    if (!ios.good() && !ios.eof()) {
        ERR_POST("Error receiving data");
        return 2;
    }

    LOG_POST(buflen << " bytes obtained" << (ios.eof() ? " (EOF)" : ""));
    buf2[buflen] = '\0';

    for (i = 0; i < kBufferSize; i++) {
        if (!buf2[i])
            break;
        if (buf2[i] != buf1[i])
            break;
    }
    if (i < kBufferSize)
        ERR_POST("Not entirely bounced, mismatch position: " << i + 1);
    else if ((size_t) buflen > kBufferSize)
        ERR_POST("Sent: " << kBufferSize << ", bounced: " << buflen);
    else
        LOG_POST("Test 4 passed\n");

    // Clear EOF condition
    ios.clear();


    LOG_POST("Test 5 of 7: Random bounce");

    if (!(ios << buf1)) {
        ERR_POST("Error sending data");
        return 1;
    }
    assert(ios.tellp() == (CT_POS_TYPE)((CT_OFF_TYPE)(2*kBufferSize)));

    j = 0;
    buflen = 0;
    for (i = 0, l = 0; i < kBufferSize; i += j, l++) {
        k = rand()%15 + 1;

        if (i + k > kBufferSize + 1)
            k = kBufferSize + 1 - i;
        ios.read(&buf2[i], k);
        j = ios.gcount();
        if (!ios.good() && !ios.eof()) {
            ERR_POST("Error receiving data");
            return 2;
        }
        if (j != k)
            LOG_POST("Bytes requested: " << k << ", received: " << j);
        buflen += j;
        l++;
        if (!j && ios.eof())
            break;
    }

    LOG_POST(buflen << " bytes obtained in " << l << " iteration(s)" <<
             (ios.eof() ? " (EOF)" : ""));
    buf2[buflen] = '\0';

    for (i = 0; i < kBufferSize; i++) {
        if (!buf2[i])
            break;
        if (buf2[i] != buf1[i])
            break;
    }
    if (i < kBufferSize)
        ERR_POST("Not entirely bounced, mismatch position: " << i + 1);
    else if ((size_t) buflen > kBufferSize)
        ERR_POST("Sent: " << kBufferSize << ", bounced: " << buflen);
    else
        LOG_POST("Test 5 passed\n");

    // Clear EOF condition
    ios.clear();


    LOG_POST("Test 6 of 7: Truly binary bounce");

    for (i = 0; i < kBufferSize; i++)
        buf1[i] = (char)(255/*rand()%256*/);

    ios.write(buf1, kBufferSize);

    if (!ios.good()) {
        ERR_POST("Error sending data");
        return 1;
    }
    assert(ios.tellp() == (CT_POS_TYPE)((CT_OFF_TYPE)(3*kBufferSize)));

    ios.read(buf2, kBufferSize + 1);
    buflen = ios.gcount();

    if (!ios.good() && !ios.eof()) {
        ERR_POST("Error receiving data");
        return 2;
    }

    LOG_POST(buflen << " bytes obtained" << (ios.eof() ? " (EOF)" : ""));

    for (i = 0; i < kBufferSize; i++) {
        if (buf2[i] != buf1[i])
            break;
    }
    if (i < kBufferSize)
        ERR_POST("Not entirely bounced, mismatch position: " << i + 1);
    else if ((size_t) buflen > kBufferSize)
        ERR_POST("Sent: " << kBufferSize << ", bounced: " << buflen);
    else
        LOG_POST("Test 6 passed\n");

    delete[] buf1;
    delete[] buf2;


    LOG_POST("Test 7 of 7: NcbiStreamCopy()");

    ofstream null(DEV_NULL);
    assert(null);

    CConn_HttpStream http("http://www.ncbi.nlm.nih.gov"
                          "/cpp/network/dispatcher.html", 0,
                          "My-Header: Header\r\n", fHCC_Flushable);
    http << "Sample input -- should be ignored";

    if (!http.good()  ||  !http.flush()  ||  !NcbiStreamCopy(null, http))
        ERR_POST(Fatal << "Test 6 failed");
    else
        LOG_POST("Test 7 passed\n");

    CORE_SetREG(0);
    delete reg;

    CORE_LOG(eLOG_Note, "TEST completed successfully");
    CORE_SetLOCK(0);
    CORE_SetLOG(0);
    return 0/*okay*/;
}
