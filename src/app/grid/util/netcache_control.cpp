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
 * Authors:  Anatoliy Kuznetsov, Dmitry Kazimirov
 *
 * File Description:  NetCache administration utility
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbistl.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbireg.hpp>
#include <corelib/ncbi_system.hpp>
#include <corelib/ncbimisc.hpp>
#include <corelib/rwstream.hpp>

#include <connect/services/netcache_api.hpp>
#include <connect/services/neticache_client.hpp>
#include <connect/ncbi_socket.hpp>
#include <common/ncbi_package_ver.h>

#ifdef WIN32
#include <io.h>
#include <fcntl.h>
#endif

#define NETCACHE_CONTROL_VERSION_MAJOR 1
#define NETCACHE_CONTROL_VERSION_MINOR 1
#define NETCACHE_CONTROL_VERSION_PATCH 0

USING_NCBI_SCOPE;

///////////////////////////////////////////////////////////////////////

/// Netcache stop application
///
/// @internal
///
class CNetCacheControl : public CNcbiApplication
{
public:
    CNetCacheControl() {
        SetVersion(CVersionInfo(
            NETCACHE_CONTROL_VERSION_MAJOR,
            NETCACHE_CONTROL_VERSION_MINOR,
            NETCACHE_CONTROL_VERSION_PATCH));
    }
    void Init();
    int Run();

private:
    struct SICacheBlobAddress {
        string key;
        int version;
        string subkey;
    };

    void ParseICacheBlobAddress(const string& key,
        SICacheBlobAddress* blob_address);
};



void CNetCacheControl::Init()
{
    // Setup command line arguments and parameters

    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "NCBI NetCache control.");

    arg_desc->AddDefaultPositional("service",
        "NetCache service name.", CArgDescriptions::eString, kEmptyStr);

    arg_desc->AddOptionalKey("auth", "name",
        "Authentication string (client name)", CArgDescriptions::eString);

    arg_desc->AddOptionalKey("icache", "cache_name",
        "Connect to an ICache database", CArgDescriptions::eString);

    arg_desc->AddFlag("shutdown",      "Shutdown server");
    arg_desc->AddFlag("ver",           "Server version");
    arg_desc->AddFlag("getconf",       "Server config");
    arg_desc->AddFlag("health",        "Server health");
    arg_desc->AddFlag("stat",          "Server statistics");
    arg_desc->AddFlag("monitor",       "Monitor server");

    arg_desc->AddOptionalKey("fetch", "key",
        "Retrieve data by key", CArgDescriptions::eString);

    arg_desc->AddOptionalKey("outputfile", "file_name",
        "Send output to a file instead of stdout", CArgDescriptions::eString);

    arg_desc->AddDefaultKey("store", "key",
        "Store data in NetCache", CArgDescriptions::eString, kEmptyStr);

    arg_desc->AddOptionalKey("inputfile", "file_name",
        "Read input from a file instead of stdin", CArgDescriptions::eString);

    arg_desc->AddOptionalKey("owner", "owner",
        "Get BLOB's owner", CArgDescriptions::eString);

    arg_desc->AddOptionalKey("size", "key",
        "Get BLOB size", CArgDescriptions::eString);

    arg_desc->PrintUsageIfNoArgs();

    SetupArgDescriptions(arg_desc.release());
}

void CNetCacheControl::ParseICacheBlobAddress(
    const string& key, SICacheBlobAddress* blob_address)
{
    vector<string> key_parts;

    NStr::Tokenize(key, ",", key_parts);
    if (key_parts.size() != 3) {
        NCBI_THROW_FMT(CArgException, eInvalidArg,
            "Invalid ICache key specification \"" << key << "\" ("
                "expected a comma-separated key,version,subkey triplet).");
    }
    blob_address->version = NStr::StringToInt(key_parts[1]);
    blob_address->key = key_parts.front();
    blob_address->subkey = key_parts.back();
}

int CNetCacheControl::Run()
{
    const CArgs& args = GetArgs();
    const string& service  = args["service"].AsString();

    bool icache_mode = args["icache"].HasValue();

    if (icache_mode && service.empty()) {
        NCBI_THROW(CArgException, eNoValue, "The \"service\" "
            "argument is required in icache mode.");
    }

    string client_name = args["auth"].HasValue() ?
        args["auth"].AsString() : "netcache_control";

    CNetCacheAPI nc_client(service, client_name);
    CNetCacheAdmin admin = nc_client.GetAdmin();

    CNetICacheClient icache_client(eVoid);
    if (icache_mode)
        icache_client = CNetICacheClient(service,
            args["icache"].AsString(), client_name);

    if (args["fetch"].HasValue()) {
        string key(args["fetch"].AsString());
        size_t blob_size = 0;
        auto_ptr<IReader> reader;
        if (!icache_mode)
            reader.reset(nc_client.GetData(key, &blob_size));
        else {
            SICacheBlobAddress blob_address;
            ParseICacheBlobAddress(key, &blob_address);
            reader.reset(icache_client.GetReadStream(blob_address.key,
                blob_address.version, blob_address.subkey));
        }
        if (!reader.get()) {
            NCBI_THROW(CNetCacheException, eBlobNotFound,
                "Cannot find the requested blob");
        }

        FILE* output;
        if (args["outputfile"].HasValue()) {
            output = fopen(args["outputfile"].AsString().c_str(), "wb");
            if (output == NULL) {
                NCBI_USER_THROW_FMT("Could not open \"" <<
                    args["outputfile"].AsString() << "\" for output.");
            }
        } else {
            output = stdout;
#ifdef WIN32
            setmode(fileno(stdout), O_BINARY);
#endif
        }

        char buffer[16 * 1024];
        ERW_Result read_result;
        size_t bytes_read;

        while ((read_result = reader->Read(buffer,
            sizeof(buffer), &bytes_read)) == eRW_Success) {

            fwrite(buffer, 1, bytes_read, output);
        }

        if (read_result != eRW_Eof) {
            ERR_POST("Error while sending data to the output stream");
            return 1;
        }
    } else if (args["store"]) {
        string key;
        auto_ptr<IWriter> writer;
        if (!icache_mode)
            writer.reset(nc_client.PutData(&key));
        else {
            SICacheBlobAddress blob_address;
            ParseICacheBlobAddress(args["store"].AsString(), &blob_address);
            writer.reset(icache_client.GetWriteStream(blob_address.key,
                blob_address.version, blob_address.subkey));
        }
        if (!writer.get()) {
            NCBI_USER_THROW_FMT("Cannot create blob stream");
        }

        FILE* input;
        if (args["inputfile"].HasValue()) {
            input = fopen(args["inputfile"].AsString().c_str(), "rb");
            if (input == NULL) {
                NCBI_USER_THROW_FMT("Could not open \"" <<
                    args["inputfile"].AsString() << "\" for input.");
            }
        } else {
            input = stdin;
#ifdef WIN32
            setmode(fileno(stdin), O_BINARY);
#endif
        }

        char buffer[16 * 1024];
        size_t bytes_read;
        size_t bytes_written;

        while ((bytes_read = fread(buffer, 1, sizeof(buffer), input)) > 0) {
            if (writer->Write(buffer, bytes_read, &bytes_written) !=
                    eRW_Success || bytes_written != bytes_read) {
                NCBI_USER_THROW_FMT("Error while writing to NetCache");
            }
        }

        if (!icache_mode)
            NcbiCout << key << NcbiEndl;
    } else if (args["owner"]) {
        string key(args["owner"].AsString());
        string owner;
        if (!icache_mode)
            owner = nc_client.GetOwner(key);
        else {
            SICacheBlobAddress blob_address;
            ParseICacheBlobAddress(key, &blob_address);
            icache_client.GetBlobOwner(blob_address.key,
                blob_address.version, blob_address.subkey, &owner);
        }
        NcbiCout << "BLOB belongs to: [" << owner << "]" << NcbiEndl;
    } else if (args["size"]) {
        string key(args["size"].AsString());
        size_t size;
        if (!icache_mode)
            size = nc_client.GetBlobSize(key);
        else {
            SICacheBlobAddress blob_address;
            ParseICacheBlobAddress(key, &blob_address);
            size = icache_client.GetSize(blob_address.key,
                blob_address.version, blob_address.subkey);
        }
        NcbiCout << "BLOB size: " << size << NcbiEndl;
    } else if (args["getconf"])
        admin.PrintConfig(NcbiCout);
    else if (args["health"])
        admin.PrintHealth(NcbiCout);
    else if (args["monitor"])
        admin.Monitor(NcbiCout);
    else if (args["stat"])
        admin.PrintStat(NcbiCout);
    else if (args["ver"])
        admin.GetServerVersion(NcbiCout);
    else if (args["shutdown"]) {
        admin.ShutdownServer();
        NcbiCout << "Shutdown request has been sent to server" << NcbiEndl;
    }

    return 0;
}


int main(int argc, const char* argv[])
{
    return CNetCacheControl().AppMain(argc, argv, 0, eDS_Default, 0);
}
