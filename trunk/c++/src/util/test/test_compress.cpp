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
 * Author:  Vladimir Ivanov
 *
 * File Description:  Test program for the Compression API
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbi_limits.hpp>
#include <corelib/ncbifile.hpp>

#include <util/compress/bzip2.hpp>
#include <util/compress/zlib.hpp>
#include <util/compress/lzo.hpp>

#include <common/test_assert.h>  // This header must go last


USING_NCBI_SCOPE;

/// Number of tests.
const size_t  kTestCount = 3;
/// Length of data buffers for tests.
const size_t  kDataLength[kTestCount] = { 20, 16*1024, 100*1024 };
/// Output buffer length. ~20% more than maximum value from kDataLength[].
const size_t  kBufLen = 120*1024;  
/// Maximum number of bytes to read.
const size_t  kReadMax = kBufLen;

const int           kUnknownErr   = kMax_Int;
const unsigned int  kUnknown      = kMax_UInt;


//////////////////////////////////////////////////////////////////////////////
//
// The template class for compressor test
//

template<class TCompression,
         class TCompressionFile,
         class TStreamCompressor,
         class TStreamDecompressor>

class CTestCompressor
{
public:
    // Run test for the template compressor usind data from "src_buf"
    static void Run(const char* src_buf, size_t data_len);

    // Print out compress/decompress status
    enum EPrintType { 
        eCompress,
        eDecompress 
    };
    static void PrintResult(EPrintType type, int last_errcode,
                            size_t src_len, size_t dst_len, size_t out_len);
};


/// Print OK message.
#define OK LOG_POST("OK\n")

/// Initialize destination buffers.
#define INIT_BUFFERS  memset(dst_buf, 0, kBufLen); memset(cmp_buf, 0, kBufLen)


template<class TCompression,
         class TCompressionFile,
         class TStreamCompressor,
         class TStreamDecompressor>
void CTestCompressor<TCompression, TCompressionFile,
                     TStreamCompressor, TStreamDecompressor>

    ::Run(const char* src_buf, size_t kDataLen)
{
    const char* kFileName = "compressed.file";

    // Initialize LZO compression
#if defined(HAVE_LIBLZO)
    assert(CLZOCompression::Initialize());
#endif
#   include "test_compress_run.inl"
}

template<class TCompression,
         class TCompressionFile,
         class TStreamCompressor,
         class TStreamDecompressor>
void CTestCompressor<TCompression, TCompressionFile,
                     TStreamCompressor, TStreamDecompressor>
    ::PrintResult(EPrintType type, int last_errcode, 
                  size_t src_len, size_t dst_len, size_t out_len)
{
    LOG_POST(((type == eCompress) ? "Compress   ": "Decompress ")
             << "errcode = "
             << ((last_errcode == kUnknownErr) ? 
                    "?" : NStr::IntToString(last_errcode)) << ", "
             << ((src_len == kUnknown) ? 
                    "?" : NStr::UIntToString(src_len)) << " -> "
             << ((out_len == kUnknown) ? 
                    "?" : NStr::UIntToString(out_len)) << ", limit "
             << ((dst_len == kUnknown) ? 
                    "?" : NStr::UIntToString(dst_len))
    );
}


//////////////////////////////////////////////////////////////////////////////
//
// Test application
//

class CTest : public CNcbiApplication
{
public:
    void Init(void);
    int  Run(void);
};


void CTest::Init(void)
{
    SetDiagPostLevel(eDiag_Warning);
}


int CTest::Run(void)
{
    AutoArray<char> src_buf_arr(kBufLen);
    char* src_buf = src_buf_arr.get();
    assert(src_buf);

    // Set a random starting point
    unsigned int seed = (unsigned int)time(0);
    LOG_POST("Random seed = " << seed);
    srand(seed);

    // Preparing a data for compression
    for (size_t i=0; i<kBufLen; i++) {
        // Use a set of 25 chars [A-Z]
        src_buf[i] = (char)(65+(double)rand()/RAND_MAX*(90-65));

    }
    // Test compressors with different size of data
    for (size_t i = 0; i < kTestCount; i++) {

        LOG_POST("====================================\n" << 
                 "Data size = " << kDataLength[i] << "\n\n");

        LOG_POST("-------------- BZip2 ---------------\n");
        CTestCompressor<CBZip2Compression, CBZip2CompressionFile,
                        CBZip2StreamCompressor, CBZip2StreamDecompressor>
            ::Run(src_buf, kDataLength[i]);

        LOG_POST("-------------- Zlib ----------------\n");
        CTestCompressor<CZipCompression, CZipCompressionFile,
                        CZipStreamCompressor, CZipStreamDecompressor>
            ::Run(src_buf, kDataLength[i]);

#if defined(HAVE_LIBLZO)
        LOG_POST("-------------- LZO -----------------\n");

        CTestCompressor<CLZOCompression, CLZOCompressionFile,
                        CLZOStreamCompressor, CLZOStreamDecompressor>
            ::Run(src_buf, kDataLength[i]);
#endif

        LOG_POST("\nTEST execution completed successfully!\n");
    }
    return 0;
}


//////////////////////////////////////////////////////////////////////////////
//
// MAIN
//

int main(int argc, const char* argv[])
{
    // Execute main application function
    return CTest().AppMain(argc, argv, 0, eDS_Default, 0);
}
