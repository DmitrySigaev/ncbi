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
 * Authors:  Anatoliy Kuznetsov, Rafael Sadyrov
 *
 * File Description:  Network ICache client test
 *
 */

#include <ncbi_pch.hpp>

#include <connect/services/neticache_client.hpp>

#include <connect/ncbi_socket.hpp>
#include <connect/ncbi_types.h>
#include <connect/ncbi_core_cxx.hpp>

#include <corelib/ncbiapp.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbireg.hpp>
#include <corelib/ncbi_system.hpp>
#include <corelib/ncbimisc.hpp>

#include <corelib/test_boost.hpp>

#include <util/random_gen.hpp>

#include <common/test_assert.h>  /* This header must go last */


USING_NCBI_SCOPE;


///////////////////////////////////////////////////////////////////////


// Configuration parameters
NCBI_PARAM_DECL(string, netcache, service_name);
typedef NCBI_PARAM_TYPE(netcache, service_name) TNetCache_ServiceName;
NCBI_PARAM_DEF(string, netcache, service_name, "NC_UnitTest");

NCBI_PARAM_DECL(string, netcache, cache_name);
typedef NCBI_PARAM_TYPE(netcache, cache_name) TNetCache_CacheName;
NCBI_PARAM_DEF(string, netcache, cache_name, "test");

static const string s_ClientName("test_icache");

static int s_Run()
{
    const string service  = TNetCache_ServiceName::GetDefault();
    const string cache_name  = TNetCache_CacheName::GetDefault();

    CNetICacheClient cl(service, cache_name, s_ClientName);

    const static char test_data[] =
            "The quick brown fox jumps over the lazy dog.";
    const static size_t data_size = sizeof(test_data) - 1;

    string uid = GetDiagContext().GetStringUID();
    string key1 = "TEST_IC_CLIENT_KEY1_" + uid;
    string key2 = "TEST_IC_CLIENT_KEY2_" + uid;
    string subkey = "TEST_IC_CLIENT_SUBKEY_" + uid;
    int version = 0;

    {{
    cl.Store(key1, version, subkey, test_data, data_size);
    SleepMilliSec(700);

    bool hb = cl.HasBlobs(key1, subkey);
    assert(hb);

    size_t sz = cl.GetSize(key1, version, subkey);
    assert(sz == data_size);

    char buf[1024] = {0,};
    cl.Read(key1, version, subkey, buf, sizeof(buf) - 1);

    assert(strcmp(buf, test_data) == 0);

    memset(buf, 0, sizeof(buf));
    cl.Read(key1, version, subkey, buf, sizeof(buf) - 1);

    assert(strcmp(buf, test_data) == 0);

    memset(buf, 0, sizeof(buf));
    cl.ReadPart(key1, version, subkey, sizeof("The ") - 1,
        sizeof("The quick") - sizeof("The "), buf, sizeof(buf) - 1);

    assert(strcmp(buf, "quick") == 0);


    sz = cl.GetSize(key1, version, subkey);
    assert(sz == data_size);
    hb = cl.HasBlobs(key1, subkey);
    assert(hb);

    cl.Remove(key1, version, subkey);
    hb = cl.HasBlobs(key1, subkey);
    assert(!hb);

    }}

    {{
    size_t test_size = 1024 * 1024 * 2;
    unsigned char *test_buf = new unsigned char[test_size+10];
    for (size_t i = 0; i < test_size; ++i) {
        test_buf[i] = 127;
    }

    cl.Store(key2, version, subkey, test_buf, test_size);

    for (size_t i = 0; i < test_size; ++i) {
        test_buf[i] = 0;
    }
    SleepMilliSec(700);

    size_t sz = cl.GetSize(key2, version, subkey);
    assert(sz == test_size);


    cl.Read(key2, version, subkey, test_buf, test_size);

    for (size_t i = 0; i < test_size; ++i) {
        if (test_buf[i] != 127) {
            assert(0);
        }
    }

    sz = cl.GetSize(key2, version, subkey);
    assert(sz == test_size);

    cl.Remove(key2, version, subkey);
    bool hb = cl.HasBlobs(key2, subkey);
    assert(!hb);
    }}


    NcbiCout << "stress write" << NcbiEndl;

    // stress write
    {{
    char test_buf[240];
    size_t test_size = sizeof(test_buf);
    for (int i = 0; i < 100; ++i) {
        string key = "TEST_IC_CLIENT_KEY_" + NStr::IntToString(i) + "_" + uid;
        cl.Store(key, 0, subkey, test_buf, test_size);
        cl.Remove(key, 0, subkey);
    }
    }}

    NcbiCout << "check reader/writer" << NcbiEndl;
    {{
        {
        auto_ptr<IWriter> writer1(cl.GetWriteStream(key1, version, subkey));
        string str = "qwerty";
        writer1->Write(str.c_str(), str.size());
        }
        size_t size = cl.GetSize(key1, version, subkey);
        vector<unsigned char> test_buf(1000);
        cl.Read(key1, version, subkey, &test_buf[0], test_buf.size());
        cout << size << endl << string((char*)&test_buf[0],
            test_buf.size()) << endl;
        cl.Remove(key1, version, subkey);
    }}

#ifdef DISABLED_UNTIL_NC_WITH_AGE_SUPPORT_IS_DEPLOYED
    NcbiCout << "blob age test" << NcbiEndl;
    {{
        ++version;
        cl.Store(key1, version, subkey, test_data, data_size);

        char buffer[1024];

        ICache::SBlobAccessDescr blob_access(buffer, sizeof(buffer));

        blob_access.return_current_version = true;
        blob_access.maximum_age = 5;

        SleepSec(2);

        cl.GetBlobAccess(key1, 0, subkey, &blob_access);

        assert(blob_access.blob_found);
        assert(blob_access.reader.get() == NULL);
        assert(blob_access.blob_size < blob_access.buf_size);
        assert(memcmp(blob_access.buf, test_data, data_size) == 0);
        assert(blob_access.current_version == version);
        assert(blob_access.current_version_validity == ICache::eCurrent);
        assert(blob_access.actual_age >= 2);

        blob_access.return_current_version = false;
        blob_access.maximum_age = 2;

        SleepSec(1);

        cl.GetBlobAccess(key1, version, subkey, &blob_access);

        assert(!blob_access.blob_found);

        cl.Remove(key1, version, subkey);
    }}
#endif /* DISABLED_UNTIL_NC_WITH_AGE_SUPPORT_IS_DEPLOYED */

    return 0;
}


// Convenience class for random generator
struct CRandomSingleton
{
    CRandom r;
    CRandomSingleton() { r.Randomize(); }

    static CRandom& instance()
    {
        static CRandomSingleton rs;
        return rs.r;
    }
};


// Convenience function to fill container with random char data
template <class TContainer>
void RandomFill(TContainer& container, size_t length, bool printable = true)
{
    CRandom& random(CRandomSingleton::instance());
    const char kMin = printable ? '!' : numeric_limits<char>::min();
    const char kMax = printable ? '~' : numeric_limits<char>::max();
    container.clear();
    container.reserve(length);
    while (length-- > 0) {
        container.push_back(kMin + random.GetRandIndex(kMax - kMin + 1));
    }
}


// Convenience function to generate a unique key
string GetUniqueKey()
{
    return NStr::NumericToString(time(NULL)) + "t" +
        NStr::NumericToString(CRandomSingleton::instance().GetRandUint8());
}


static void s_SimpleTest()
{
    const string service  = TNetCache_ServiceName::GetDefault();
    const string cache_name  = TNetCache_CacheName::GetDefault();

    CNetICacheClient api(service, cache_name, s_ClientName);

    const size_t kIterations = 50;
    const size_t kSrcSize = 20 * 1024 * 1024; // 20MB
    const size_t kBufSize = 100 * 1024; // 100KB
    vector<char> src;
    vector<char> buf;

    src.reserve(kSrcSize);
    buf.reserve(kBufSize);

    const int version = 0;

    for (size_t i = 0; i < kIterations; ++i) {
        const string key = GetUniqueKey();
        const string subkey = GetUniqueKey();

        // Creating blob
        RandomFill(src, kSrcSize, false);
        api.Store(key, version, subkey, src.data(), src.size());
        BOOST_REQUIRE_MESSAGE(api.HasBlob(key, subkey),
                "Blob does not exist (" << i << ")");
        BOOST_REQUIRE_MESSAGE(api.GetBlobSize(key, version, subkey) == kSrcSize,
                "Blob size (GetBlobSize) differs from the source (" << i << ")");

        // Checking blob
        size_t size = 0;
        auto_ptr<IReader> reader(api.GetReadStream(key, version, subkey, &size));

        BOOST_REQUIRE_MESSAGE(size == kSrcSize,
                "Blob size (GetData) differs from the source (" << i << ")");
        BOOST_REQUIRE_MESSAGE(reader.get(),
                "Failed to get reader (" << i << ")");

        const char* ptr = src.data();

        for (;;) {
            size_t read = 0;

            switch (reader->Read(buf.data(), kBufSize, &read)) {
            case eRW_Success:
                BOOST_REQUIRE_MESSAGE(!memcmp(buf.data(), ptr, read),
                        "Blob content does not match the source (" << i << ")");
                BOOST_REQUIRE_MESSAGE(size >= read,
                        "Blob size is greater than the source (" << i << ")");
                ptr += read;
                size -= read;
                continue;

            case eRW_Eof:
                BOOST_REQUIRE_MESSAGE(!size,
                        "Blob size is less than the source (" << i << ")");
                break;

            default:
                BOOST_ERROR("Reading blob failed (" << i << ")");
            }

            break;
        }

        // Removing blob
        api.RemoveBlob(key, version, subkey);

        // Checking removed blob
        BOOST_REQUIRE_MESSAGE(!api.HasBlob(key, subkey),
                "Removed blob still exists (" << i << ")");
        auto_ptr<IReader> fail_reader(api.GetReadStream(key, version, subkey, &size));
        BOOST_REQUIRE_MESSAGE(!fail_reader.get(),
                "Got reader for removed blob (" << i << ")");
    }
}

BOOST_AUTO_TEST_SUITE(NetICacheClient)

BOOST_AUTO_TEST_CASE(OldTest)
{
    EDiagSev prev_level = SetDiagPostLevel(eDiag_Info);
    SetDiagTrace(eDT_Enable);
    s_Run();
    SetDiagTrace(eDT_Disable);
    SetDiagPostLevel(prev_level);
}

BOOST_AUTO_TEST_CASE(SimpleTest)
{
    s_SimpleTest();
}

BOOST_AUTO_TEST_SUITE_END()
