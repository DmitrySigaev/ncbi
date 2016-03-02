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
 * Author:  Dmitry Kazimirov
 *
 * File Description:
 *   Implementation of the unified network blob storage API.
 *
 */

#include <ncbi_pch.hpp>

#include "netservice_api_impl.hpp"
#include "netstorage_direct_nc.hpp"

#include <connect/services/error_codes.hpp>

#include <util/ncbi_url.hpp>

#include <corelib/request_ctx.hpp>

#include <sstream>

#define NCBI_USE_ERRCODE_X  NetStorage_RPC

#define NST_PROTOCOL_VERSION "1.0.0"

#define READ_CHUNK_SIZE (64 * 1024)

#define WRITE_BUFFER_SIZE (64 * 1024)
#define READ_BUFFER_SIZE (64 * 1024)

#define END_OF_DATA_MARKER '\n'

BEGIN_NCBI_SCOPE

const char* CNetStorageException::GetErrCodeString() const
{
    switch (GetErrCode()) {
    case eInvalidArg:           return "eInvalidArg";
    case eNotExists:            return "eNotExist";
    case eAuthError:            return "eAuthError";
    case eIOError:              return "eIOError";
    case eServerError:          return "eServerError";
    case eTimeout:              return "eTimeout";
    case eExpired:              return "eExpired";
    default:                    return CException::GetErrCodeString();
    }
}

static void s_WriteToSocket(CSocket& sock,
        const char* output_buffer, size_t output_buffer_size)
{
    size_t bytes_written;

    while (output_buffer_size > 0) {
        EIO_Status  status = sock.Write(output_buffer,
                output_buffer_size, &bytes_written);
        if (status != eIO_Success) {
            // Error writing to the socket.
            // Log what we can.
            string message_start;

            if (output_buffer_size > 32) {
                CTempString buffer_head(output_buffer, 32);
                message_start = NStr::PrintableString(buffer_head);
                message_start += " (TRUNCATED)";
            } else {
                CTempString buffer_head(output_buffer, output_buffer_size);
                message_start = NStr::PrintableString(buffer_head);
            }

            NCBI_THROW_FMT(CNetStorageException, eIOError,
                    "Error writing message to the NetStorage server " <<
                            sock.GetPeerAddress() << ". "
                    "Socket write error status: " <<
                            IO_StatusStr(status) << ". "
                    "Bytes written: " <<
                            NStr::NumericToString(bytes_written) << ". "
                    "Message begins with: " << message_start);
        }

        output_buffer += bytes_written;
        output_buffer_size -= bytes_written;
    }
}

class CSendJsonOverSocket
{
public:
    CSendJsonOverSocket(CSocket& sock) : m_JSONWriter(m_UTTPWriter), m_Socket(sock) {}

    void SendMessage(const CJsonNode& message);

private:
    void x_SendOutputBuffer();

    CUTTPWriter m_UTTPWriter;
    CJsonOverUTTPWriter m_JSONWriter;
    CSocket& m_Socket;
};

void CSendJsonOverSocket::SendMessage(const CJsonNode& message)
{
    char write_buffer[WRITE_BUFFER_SIZE];

    m_UTTPWriter.Reset(write_buffer, WRITE_BUFFER_SIZE);

    if (!m_JSONWriter.WriteMessage(message))
        do
            x_SendOutputBuffer();
        while (!m_JSONWriter.CompleteMessage());

    x_SendOutputBuffer();
}

void CSendJsonOverSocket::x_SendOutputBuffer()
{
    const char* output_buffer;
    size_t output_buffer_size;

    do {
        m_JSONWriter.GetOutputBuffer(&output_buffer, &output_buffer_size);

        s_WriteToSocket(m_Socket, output_buffer, output_buffer_size);
    } while (m_JSONWriter.NextOutputBuffer());
}

static void s_SendUTTPChunk(const char* chunk, size_t chunk_size, CSocket& sock)
{
    CUTTPWriter uttp_writer;

    char write_buffer[WRITE_BUFFER_SIZE];

    uttp_writer.Reset(write_buffer, WRITE_BUFFER_SIZE);

    uttp_writer.SendChunk(chunk, chunk_size, false);

    const char* output_buffer;
    size_t output_buffer_size;

    do {
        uttp_writer.GetOutputBuffer(&output_buffer, &output_buffer_size);
        s_WriteToSocket(sock, output_buffer, output_buffer_size);
    } while (uttp_writer.NextOutputBuffer());

    uttp_writer.GetOutputBuffer(&output_buffer, &output_buffer_size);
    s_WriteToSocket(sock, output_buffer, output_buffer_size);
}

static void s_SendEndOfData(CSocket& sock)
{
    CUTTPWriter uttp_writer;

    char write_buffer[WRITE_BUFFER_SIZE];

    uttp_writer.Reset(write_buffer, WRITE_BUFFER_SIZE);

    uttp_writer.SendControlSymbol(END_OF_DATA_MARKER);

    const char* output_buffer;
    size_t output_buffer_size;

    do {
        uttp_writer.GetOutputBuffer(&output_buffer, &output_buffer_size);
        s_WriteToSocket(sock, output_buffer, output_buffer_size);
    } while (uttp_writer.NextOutputBuffer());

    uttp_writer.GetOutputBuffer(&output_buffer, &output_buffer_size);
    s_WriteToSocket(sock, output_buffer, output_buffer_size);
}

static void s_ReadSocket(CSocket& sock, void* buffer,
        size_t buffer_size, size_t* bytes_read)
{
    EIO_Status status;

    while ((status = sock.Read(buffer,
            buffer_size, bytes_read)) == eIO_Interrupt)
        /* no-op */;

    if (status != eIO_Success) {
        // eIO_Timeout, eIO_Closed, eIO_InvalidArg,
        // eIO_NotSupported, eIO_Unknown
        NCBI_THROW_FMT(CNetStorageException, eIOError,
                "I/O error while reading from NetStorage server " <<
                        sock.GetPeerAddress() << ". "
                "Socket status: " << IO_StatusStr(status) << '.');
    }
}

class CReadJsonFromSocket
{
public:
    CJsonNode ReadMessage(CSocket& sock);

private:
    CUTTPReader m_UTTPReader;
    CJsonOverUTTPReader m_JSONReader;
};

CJsonNode CReadJsonFromSocket::ReadMessage(CSocket& sock)
{
    try {
        char read_buffer[READ_BUFFER_SIZE];

        size_t bytes_read;

        do {
            s_ReadSocket(sock, read_buffer, READ_BUFFER_SIZE, &bytes_read);

            m_UTTPReader.SetNewBuffer(read_buffer, bytes_read);

        } while (!m_JSONReader.ReadMessage(m_UTTPReader));
    }
    catch (...) {
        sock.Close();
        throw;
    }

    if (m_UTTPReader.GetNextEvent() != CUTTPReader::eEndOfBuffer) {
        string server_address(sock.GetPeerAddress());
        sock.Close();
        NCBI_THROW_FMT(CNetStorageException, eIOError,
                "Extra bytes past message end while reading from " <<
                        server_address << " after receiving " <<
                        m_JSONReader.GetMessage().Repr() << '.');
    }

    return m_JSONReader.GetMessage();
}

struct SIssue
{
    Int8 code;
    string message;
    string scope;
    Int8 sub_code;

    template <class TOstream>
    TOstream& Print(TOstream& os) const
    {
        if (!scope.empty()) os << scope << "::";
        os << code;
        if (sub_code) os << '.' << sub_code;
        return os << " (" << message << ')';
    }

    struct SBuilder : private CJsonNode
    {
        explicit SBuilder(const CJsonNode& node) : CJsonNode(node) {}

        SIssue Build() const
        {
            return SIssue{
                    GetInteger("Code"),
                    GetString("Message"),
                    GetScope(),
                    GetSubCode()
                };
        }

    private:
        string GetScope() const
        {
            if (CJsonNode scope = GetByKeyOrNull("Scope")) {
                return scope.AsString();
            } else {
                return string();
            }
        }

        Int8 GetSubCode() const
        {
            if (CJsonNode sub_code = GetByKeyOrNull("SubCode")) {
                return sub_code.AsInteger();
            } else {
                return 0;
            }
        }
    };

};

// Clang does not like templated operator (sees ambiguity with CNcbiDiag's one).
const CNcbiDiag& operator<< (const CNcbiDiag& diag, const SIssue& issue)
{
    return issue.Print(diag);
}

ostream& operator<< (ostream& os, const SIssue& issue)
{
    return issue.Print(os);
}

void s_ThrowError(Int8 code, Int8 sub_code, const string& err_msg)
{
    // Issues were reported by a NetStorage server v2.2.0 or later
    if (sub_code) {
        switch (code) {
            case 3000:
                code = sub_code; // CNetStorageServerError
                break;
            case 3010:
                throw CNetServiceException(DIAG_COMPILE_INFO, 0,
                        static_cast<CNetServiceException::EErrCode>(sub_code),
                        err_msg);
            case 3020:
                throw CNetStorageException(DIAG_COMPILE_INFO, 0,
                        static_cast<CNetStorageException::EErrCode>(sub_code),
                        err_msg);
        }
    }

    switch (code) {
        case CNetStorageServerError::eNetStorageObjectNotFound:
        case CNetStorageServerError::eRemoteObjectNotFound:
            NCBI_THROW_FMT(CNetStorageException, eNotExists, err_msg);
            break;
        case CNetStorageServerError::eNetStorageObjectExpired:
            NCBI_THROW_FMT(CNetStorageException, eExpired, err_msg);
            break;
        default:
            NCBI_THROW_FMT(CNetStorageException, eServerError, err_msg);
    }
}

static void s_TrapErrors(const CJsonNode& request,
        const CJsonNode& reply, CSocket& sock,
        SNetStorage::SConfig::EErrMode err_mode)
{
    const string server_address(sock.GetPeerAddress());
    CJsonNode issues(reply.GetByKeyOrNull("Warnings"));

    if (issues) {
        for (CJsonIterator it = issues.Iterate(); it; ++it) {
            const SIssue issue(SIssue::SBuilder(*it).Build());
            LOG_POST(Warning << "NetStorage server " << server_address <<
                    " issued warning " << issue);
        }
    }

    const string status = reply.GetString("Status");
    const bool status_ok = status == "OK";
    issues = reply.GetByKeyOrNull("Errors");

    // Got errors
    if (!status_ok || issues) {
        if (status_ok && err_mode != SNetStorage::SConfig::eThrow) {
            if (err_mode == SNetStorage::SConfig::eLog) {
                for (CJsonIterator it = issues.Iterate(); it; ++it) {
                    const SIssue issue(SIssue::SBuilder(*it).Build());
                    LOG_POST(Error << "NetStorage server " << server_address <<
                            " issued error " << issue);
                }
            }
        } else {
            Int8 code = CNetStorageServerError::eUnknownError;
            Int8 sub_code = 0;
            ostringstream errors;

            if (!issues)
                errors << status;
            else {
                const char* prefix = "error ";

                for (CJsonIterator it = issues.Iterate(); it; ++it) {
                    const SIssue issue(SIssue::SBuilder(*it).Build());
                    code = issue.code;
                    sub_code = issue.sub_code;
                    errors << prefix << issue;
                    prefix = ", error ";
                }
            }

            string err_msg = FORMAT("Error while executing " <<
                            request.GetString("Type") << " "
                    "on NetStorage server " <<
                            sock.GetPeerAddress() << ". "
                    "Server returned " << errors.str());

            s_ThrowError(code, sub_code, err_msg);
        }
    }

    if (reply.GetInteger("RE") != request.GetInteger("SN")) {
        NCBI_THROW_FMT(CNetStorageException, eServerError,
                "Message serial number mismatch "
                "(NetStorage server: " << sock.GetPeerAddress() << "; "
                "request: " << request.Repr() << "; "
                "reply: " << reply.Repr() << ").");
    }
}

class CNetStorageServerListener : public INetServerConnectionListener
{
public:
    CNetStorageServerListener(const CJsonNode& hello,
            SNetStorage::SConfig::EErrMode err_mode) :
        m_Hello(hello), m_ErrMode(err_mode)
    {
    }

    virtual CRef<INetServerProperties> AllocServerProperties();

    virtual void OnInit(CObject* api_impl,
        CConfig* config, const string& config_section);
    virtual void OnConnected(CNetServerConnection& connection);
    virtual void OnError(const string& err_msg, CNetServer& server);
    virtual void OnWarning(const string& warn_msg, CNetServer& server);
 
private:
    const CJsonNode m_Hello;
    const SNetStorage::SConfig::EErrMode m_ErrMode;
};

CRef<INetServerProperties> CNetStorageServerListener::AllocServerProperties()
{
    return CRef<INetServerProperties>(new INetServerProperties);
}

void CNetStorageServerListener::OnInit(CObject* /*api_impl*/,
        CConfig* /*config*/, const string& /*config_section*/)
{
    //SNetStorageRPC* netstorage_rpc = static_cast<SNetStorageRPC*>(api_impl);
}

void CNetStorageServerListener::OnConnected(
        CNetServerConnection& connection)
{
    CSocket& sock(connection->m_Socket);

    CSendJsonOverSocket message_sender(sock);

    message_sender.SendMessage(m_Hello);

    CReadJsonFromSocket message_reader;

    CJsonNode reply(message_reader.ReadMessage(sock));

    s_TrapErrors(m_Hello, reply, sock, m_ErrMode);
}

void CNetStorageServerListener::OnError(const string& /*err_msg*/,
        CNetServer& /*server*/)
{
}

void CNetStorageServerListener::OnWarning(const string& warn_msg,
        CNetServer& /*server*/)
{
}

struct SNetStorageObjectRPC : public SNetStorageObjectImpl
{
    enum EObjectIdentification {
        eByGeneratedID,
        eByUniqueKey
    };

    enum EState {
        eReady,
        eReading,
        eWriting
    };

    SNetStorageObjectRPC(SNetStorageRPC* netstorage_rpc,
            CJsonNode::TInstance original_request,
            CNetServerConnection::TInstance conn,
            EObjectIdentification object_identification,
            const string& object_loc_or_key,
            TNetStorageFlags flags,
            EState initial_state);

    virtual ~SNetStorageObjectRPC();

    CJsonNode ExchangeUsingOwnService(const CJsonNode& request,
            CNetServerConnection* conn = NULL,
            CNetServer::TInstance server_to_use = NULL) const
    {
        return m_NetStorageRPC->Exchange(m_OwnService,
                request, conn, server_to_use);
    }

    void ReadConfirmation();
    virtual ERW_Result Read(void* buf, size_t count, size_t* bytes_read);

    virtual ERW_Result Write(const void* buf, size_t count,
            size_t* bytes_written);
    virtual void Close();

    virtual string GetLoc();
    virtual bool Eof();
    virtual Uint8 GetSize();
    virtual list<string> GetAttributeList() const;
    virtual string GetAttribute(const string& attr_name) const;
    virtual void SetAttribute(const string& attr_name,
            const string& attr_value);
    virtual CNetStorageObjectInfo GetInfo();
    virtual void SetExpiration(const CTimeout&);

    string FileTrack_Path();

    CJsonNode x_MkRequest(const string& request_type) const;

    CRef<SNetStorageRPC,
            CNetComponentCounterLocker<SNetStorageRPC> > m_NetStorageRPC;

    CNetService m_OwnService;

    CJsonNode m_OriginalRequest;
    CNetServerConnection m_Connection;
    char* m_ReadBuffer;
    CUTTPReader m_UTTPReader;
    const char* m_CurrentChunk;
    size_t m_CurrentChunkSize;
    EObjectIdentification m_ObjectIdentification;
    string m_Locator;
    string m_UniqueKey;
    TNetStorageFlags m_Flags;
    EState m_State;
    bool m_EOF;
};

SNetStorageObjectRPC::SNetStorageObjectRPC(SNetStorageRPC* netstorage_rpc,
        CJsonNode::TInstance original_request,
        CNetServerConnection::TInstance conn,
        EObjectIdentification object_identification,
        const string& object_loc_or_key,
        TNetStorageFlags flags,
        EState initial_state) :
    m_NetStorageRPC(netstorage_rpc),
    m_OriginalRequest(original_request),
    m_Connection(conn),
    m_ReadBuffer(NULL),
    m_ObjectIdentification(object_identification),
    m_Flags(flags),
    m_State(initial_state)
{
    if (object_identification == eByGeneratedID) {
        m_OwnService = netstorage_rpc->GetServiceFromLocator(object_loc_or_key);
        m_Locator = object_loc_or_key;
    } else {
        m_OwnService = netstorage_rpc->m_Service;
        m_UniqueKey = object_loc_or_key;
    }
}

SNetStorageObjectRPC::~SNetStorageObjectRPC()
{
    try {
        Close();
    }
    NCBI_CATCH_ALL("Error while implicitly closing a NetStorage object.");

    delete[] m_ReadBuffer;
}

static const char* const s_NetStorageConfigSections[] = {
    "netstorage_api",
    NULL
};

SNetStorage::SConfig::EDefaultStorage
SNetStorage::SConfig::GetDefaultStorage(const string& value)
{
    if (NStr::CompareNocase(value, "nst") == 0)
        return eNetStorage;
    else if (NStr::CompareNocase(value, "nc") == 0)
        return eNetCache;
    else if (NStr::CompareNocase(value, "nocreate") == 0 ||
            NStr::CompareNocase(value, "no_create") == 0)
        return eNoCreate;
    else {
        NCBI_THROW_FMT(CNetStorageException, eInvalidArg,
                "Invalid default_storage value '" << value << '\'');
        return eUndefined;
    }
}

SNetStorage::SConfig::EErrMode
SNetStorage::SConfig::GetErrMode(const string& value)
{
    if (NStr::CompareNocase(value, "strict") == 0)
        return eThrow;
    else if (NStr::CompareNocase(value, "ignore") == 0)
        return eIgnore;
    else
        return eLog;
}

void SNetStorage::SConfig::ParseArg(const string& name, const string& value)
{
    if (name == "domain")
        app_domain = value;
    else if (name == "default_storage")
        default_storage = GetDefaultStorage(value);
    else if (name == "metadata")
        metadata = value;
    else if (name == "namespace")
        app_domain = value;
    else if (name == "nst")
        service = value;
    else if (name == "nc")
        nc_service = value;
    else if (name == "cache")
        app_domain = value;
    else if (name == "client")
        client_name = value;
    else if (name == "err_mode")
        err_mode = GetErrMode(value);
}

void SNetStorage::SConfig::Validate(const string& init_string)
{
    if (client_name.empty()) {
        CNcbiApplication* app = CNcbiApplication::Instance();
        if (app != NULL) {
            string path;
            CDirEntry::SplitPath(app->GetProgramExecutablePath(),
                    &path, &client_name);
            if (NStr::EndsWith(path, CDirEntry::GetPathSeparator()))
                path.erase(path.length() - 1);
            string parent_dir;
            CDirEntry::SplitPath(path, NULL, &parent_dir);
            if (!parent_dir.empty()) {
                client_name += '-';
                client_name += parent_dir;
            }
        }
    }

    if (client_name.empty()) {
        NCBI_THROW_FMT(CNetStorageException, eAuthError,
                "Client name is required.");
    }

    switch (default_storage) {
    case eUndefined:
        default_storage =
                !service.empty() ? eNetStorage :
                !nc_service.empty() ? eNetCache :
                eNoCreate;
        break;

    case eNetStorage:
        if (service.empty()) {
            NCBI_THROW_FMT(CNetStorageException, eInvalidArg,
                    init_string << ": 'nst=' parameter is required "
                            "when 'default_storage=nst'");
        }
        break;

    case eNetCache:
        if (nc_service.empty()) {
            NCBI_THROW_FMT(CNetStorageException, eInvalidArg,
                    init_string << ": 'nc=' parameter is required "
                            "when 'default_storage=nc'");
        }
        break;

    default: /* eNoCreate */
        break;
    }
}

SNetStorageRPC::SNetStorageRPC(const TConfig& config,
        TNetStorageFlags default_flags) :
    m_DefaultFlags(default_flags),
    m_Config(config)
#ifdef NCBI_GRID_XSITE_CONN_SUPPORT
    , m_AllowXSiteConnections(false)
#endif
{
    m_RequestNumber.Set(0);

    CJsonNode hello(MkStdRequest("HELLO"));

    hello.SetString("Client", m_Config.client_name);
    hello.SetString("Service", m_Config.service);
    if (!m_Config.metadata.empty())
        hello.SetString("Metadata", m_Config.metadata);
    {{
        CMutexGuard guard(CNcbiApplication::GetInstanceMutex());
        CNcbiApplication* app = CNcbiApplication::Instance();
        if (app != NULL)
            hello.SetString("Application", app->GetProgramExecutablePath());
    }}
    hello.SetString("ProtocolVersion", NST_PROTOCOL_VERSION);

    m_Service = new SNetServiceImpl("NetStorageAPI", m_Config.client_name,
            new CNetStorageServerListener(hello, m_Config.err_mode));

    m_Service->Init(this, m_Config.service,
            NULL, kEmptyStr, s_NetStorageConfigSections);
}

CNetStorageObject SNetStorageRPC::Create(TNetStorageFlags flags)
{
    switch (m_Config.default_storage) {
    /* TConfig::eUndefined is overridden in the constructor */

    case TConfig::eNetStorage:
        break; // This case is handled below the switch.

    case TConfig::eNetCache:
        x_InitNetCacheAPI();
        return CDNCNetStorage::Create(m_NetCacheAPI);

    default: /* TConfig::eNoCreate */
        NCBI_THROW_FMT(CNetStorageException, eAuthError,
                "Object creation is disabled.");
    }

    m_UseNextSubHitID.ProperCommand();
    CJsonNode request(MkStdRequest("CREATE"));

    x_SetStorageFlags(request, flags);
    x_SetICacheNames(request);

    CNetServerConnection conn;

    string object_loc = Exchange(m_Service,
            request, &conn).GetString("ObjectLoc");

    return new SNetStorageObjectRPC(this, request, conn,
            SNetStorageObjectRPC::eByGeneratedID,
            object_loc, flags, SNetStorageObjectRPC::eWriting);
}

CNetStorageObject SNetStorageRPC::Open(const string& object_loc)
{
    if (x_NetCacheMode(object_loc))
        return CDNCNetStorage::Open(m_NetCacheAPI, object_loc);

    return new SNetStorageObjectRPC(this, NULL, NULL,
            SNetStorageObjectRPC::eByGeneratedID,
            object_loc, 0, SNetStorageObjectRPC::eReady);
}

string SNetStorageRPC::Relocate(const string& object_loc,
        TNetStorageFlags flags)
{
    if (x_NetCacheMode(object_loc))
        NCBI_THROW_FMT(CNetStorageException, eNotSupported, object_loc <<
                ": Relocate for NetCache blobs is not implemented");

    m_UseNextSubHitID.ProperCommand();
    CJsonNode request(MkObjectRequest("RELOCATE", object_loc));

    CJsonNode new_location(CJsonNode::NewObjectNode());

    x_SetStorageFlags(new_location, flags);

    request.SetByKey("NewLocation", new_location);

    return Exchange(GetServiceFromLocator(object_loc),
            request).GetString("ObjectLoc");
}

bool SNetStorageRPC::Exists(const string& object_loc)
{
    if (x_NetCacheMode(object_loc))
        try {
            return m_NetCacheAPI.HasBlob(object_loc);
        }
        NETSTORAGE_CONVERT_NETCACHEEXCEPTION("on accessing " + object_loc)

    CJsonNode request(MkObjectRequest("EXISTS", object_loc));

    return Exchange(GetServiceFromLocator(object_loc),
            request).GetBoolean("Exists");
}

ENetStorageRemoveResult SNetStorageRPC::Remove(const string& object_loc)
{
    if (x_NetCacheMode(object_loc)) {
        try {
            if (m_NetCacheAPI.HasBlob(object_loc)) {
                m_NetCacheAPI.Remove(object_loc);
                return eNSTRR_Removed;
            }
        }
        NETSTORAGE_CONVERT_NETCACHEEXCEPTION("on removing " + object_loc)

        return eNSTRR_NotFound;
    }

    m_UseNextSubHitID.ProperCommand();
    CJsonNode request(MkObjectRequest("DELETE", object_loc));
    CJsonNode response(Exchange(GetServiceFromLocator(object_loc), request));
    CJsonNode not_found(response.GetByKeyOrNull("NotFound"));

    return not_found && not_found.AsBoolean() ? eNSTRR_NotFound : eNSTRR_Removed;
}

class CJsonOverUTTPExecHandler : public INetServerExecHandler
{
public:
    CJsonOverUTTPExecHandler(const CJsonNode& request) : m_Request(request) {}

    virtual void Exec(CNetServerConnection::TInstance conn_impl,
            STimeout* timeout);

    CNetServerConnection GetConnection() const {return m_Connection;}

private:
    CJsonNode m_Request;

    CNetServerConnection m_Connection;
};

void CJsonOverUTTPExecHandler::Exec(CNetServerConnection::TInstance conn_impl,
        STimeout* timeout)
{
    CSendJsonOverSocket sender(conn_impl->m_Socket);

    sender.SendMessage(m_Request);

    m_Connection = conn_impl;
}

CJsonNode SNetStorageRPC::Exchange(CNetService service,
        const CJsonNode& request,
        CNetServerConnection* conn,
        CNetServer::TInstance server_to_use) const
{
    CNetServer server(server_to_use != NULL ? server_to_use :
            (CNetServer::TInstance)
                    *service.Iterate(CNetService::eRandomize));

    CJsonOverUTTPExecHandler json_over_uttp_sender(request);

    server->TryExec(json_over_uttp_sender);

    CReadJsonFromSocket message_reader;

    if (conn != NULL)
        *conn = json_over_uttp_sender.GetConnection();

    CSocket& sock = json_over_uttp_sender.GetConnection()->m_Socket;

    CJsonNode reply(message_reader.ReadMessage(sock));

    s_TrapErrors(request, reply, sock, m_Config.err_mode);

    return reply;
}

void SNetStorageRPC::x_SetStorageFlags(CJsonNode& node, TNetStorageFlags flags)
{
    CJsonNode storage_flags(CJsonNode::NewObjectNode());

    if (flags & fNST_Fast)
        storage_flags.SetBoolean("Fast", true);
    if (flags & fNST_Persistent)
        storage_flags.SetBoolean("Persistent", true);
    if (flags & fNST_NetCache)
        storage_flags.SetBoolean("NetCache", true);
    if (flags & fNST_FileTrack)
        storage_flags.SetBoolean("FileTrack", true);
    if (flags & fNST_Movable)
        storage_flags.SetBoolean("Movable", true);
    if (flags & fNST_Cacheable)
        storage_flags.SetBoolean("Cacheable", true);
    if (flags & fNST_NoMetaData)
        storage_flags.SetBoolean("NoMetaData", true);

    node.SetByKey("StorageFlags", storage_flags);
}

void SNetStorageRPC::x_SetICacheNames(CJsonNode& node) const
{
    if (!m_Config.nc_service.empty() && !m_Config.app_domain.empty()) {
        CJsonNode icache(CJsonNode::NewObjectNode());
        icache.SetString("ServiceName", m_Config.nc_service);
        icache.SetString("CacheName", m_Config.app_domain);
        node.SetByKey("ICache", icache);
    }
}

CJsonNode SNetStorageRPC::MkStdRequest(const string& request_type) const
{
    CJsonNode new_request(CJsonNode::NewObjectNode());

    new_request.SetString("Type", request_type);
    new_request.SetInteger("SN", (Int8) m_RequestNumber.Add(1));

    CRequestContext& req = CDiagContext::GetRequestContext();

    if (req.IsSetClientIP()) {
        new_request.SetString("ClientIP", req.GetClientIP());
    }

    if (req.IsSetSessionID()) {
        new_request.SetString("SessionID", req.GetSessionID());
    }

    // TODO:
    // Remove sending this after all NetStorage servers are updated to v2.2.0
    // (ncbi_phid is sent inside ncbi_context).
    // GetNextSubHitID()/GetCurrentSubHitID()) must be still called though
    // (without using returned value) to set appropriate value for ncbi_phid.
    if (req.IsSetHitID()) {
        new_request.SetString("ncbi_phid", m_UseNextSubHitID ?
                req.GetNextSubHitID() : req.GetCurrentSubHitID());
    }

    const auto format = CRequestContext_PassThrough::eFormat_UrlEncoded;
    const CRequestContext_PassThrough context;
    const string ncbi_context(context.Serialize(format));

    if (!ncbi_context.empty()) {
        new_request.SetString("ncbi_context", ncbi_context);
    }

    return new_request;
}

CJsonNode SNetStorageRPC::MkObjectRequest(const string& request_type,
        const string& object_loc) const
{
    CJsonNode new_request(MkStdRequest(request_type));

    new_request.SetString("ObjectLoc", object_loc);

    return new_request;
}

CJsonNode SNetStorageRPC::MkObjectRequest(const string& request_type,
        const string& unique_key, TNetStorageFlags flags) const
{
    CJsonNode new_request(MkStdRequest(request_type));

    CJsonNode user_key(CJsonNode::NewObjectNode());
    user_key.SetString("AppDomain", m_Config.app_domain);
    user_key.SetString("UniqueID", unique_key);
    new_request.SetByKey("UserKey", user_key);

    x_SetStorageFlags(new_request, flags);
    x_SetICacheNames(new_request);

    return new_request;
}

void SNetStorageRPC::x_InitNetCacheAPI()
{
    if (!m_NetCacheAPI) {
        CNetCacheAPI nc_api(m_Config.nc_service, m_Config.client_name);
        nc_api.SetCompoundIDPool(m_CompoundIDPool);
        nc_api.SetDefaultParameters(nc_use_compound_id = true);

#ifdef NCBI_GRID_XSITE_CONN_SUPPORT
        if (m_AllowXSiteConnections) {
            nc_api.GetService().AllowXSiteConnections();
        }
#endif

        m_NetCacheAPI = nc_api;
    }
}

bool SNetStorageRPC::x_NetCacheMode(const string& object_loc)
{
    if (!CNetCacheKey::ParseBlobKey(object_loc.data(), object_loc.length(),
            NULL, m_CompoundIDPool))
        return false;

    x_InitNetCacheAPI();
    return true;
}

string SNetStorageObjectRPC::GetLoc()
{
    return m_Locator;
}

void SNetStorageObjectRPC::ReadConfirmation()
{
    CSocket& sock = m_Connection->m_Socket;

    CJsonOverUTTPReader json_reader;
    try {
        size_t bytes_read;

        while (!json_reader.ReadMessage(m_UTTPReader)) {
            s_ReadSocket(sock, m_ReadBuffer,
                    READ_BUFFER_SIZE, &bytes_read);

            m_UTTPReader.SetNewBuffer(m_ReadBuffer, bytes_read);
        }

        if (m_UTTPReader.GetNextEvent() != CUTTPReader::eEndOfBuffer) {
            NCBI_THROW_FMT(CNetStorageException, eIOError,
                    "Extra bytes past confirmation message "
                    "while reading " << m_Locator << " from " <<
                    sock.GetPeerAddress() << '.');
        }
    }
    catch (...) {
        m_UTTPReader.Reset();
        m_Connection->Close();
        throw;
    }

    s_TrapErrors(m_OriginalRequest, json_reader.GetMessage(), sock,
            m_NetStorageRPC->m_Config.err_mode);
}

ERW_Result SNetStorageObjectRPC::Read(void* buffer, size_t buf_size,
        size_t* bytes_read)
{
    if (m_State == eWriting) {
        NCBI_THROW_FMT(CNetStorageException, eInvalidArg,
                "Cannot read a NetStorage file while writing to it");
    }

    size_t bytes_read_local;

    if (m_State == eReady) {
        m_OriginalRequest = x_MkRequest("READ");

        if (m_ReadBuffer == NULL)
            m_ReadBuffer = new char[READ_BUFFER_SIZE];

        CNetServer server(*m_OwnService.Iterate(CNetService::eRandomize));

        CJsonOverUTTPExecHandler json_over_uttp_sender(m_OriginalRequest);

        server->TryExec(json_over_uttp_sender);

        CSocket& sock = json_over_uttp_sender.GetConnection()->m_Socket;

        CJsonOverUTTPReader json_reader;

        try {
            do {
                s_ReadSocket(sock, m_ReadBuffer, READ_BUFFER_SIZE,
                        &bytes_read_local);

                m_UTTPReader.SetNewBuffer(m_ReadBuffer, bytes_read_local);

            } while (!json_reader.ReadMessage(m_UTTPReader));
        }
        catch (...) {
            m_UTTPReader.Reset();
            sock.Close();
            throw;
        }

        s_TrapErrors(m_OriginalRequest, json_reader.GetMessage(), sock,
            m_NetStorageRPC->m_Config.err_mode);

        m_Connection = json_over_uttp_sender.GetConnection();

        m_CurrentChunkSize = 0;

        m_State = eReading;
        m_EOF = false;
    }

    char* buf_pos = reinterpret_cast<char*>(buffer);

    if (m_CurrentChunkSize >= buf_size) {
        if (buf_size > 0) {
            memcpy(buf_pos, m_CurrentChunk, buf_size);
            m_CurrentChunk += buf_size;
            m_CurrentChunkSize -= buf_size;
        }
        if (bytes_read != NULL)
            *bytes_read = buf_size;
        return eRW_Success;
    }

    if (m_CurrentChunkSize > 0) {
        memcpy(buf_pos, m_CurrentChunk, m_CurrentChunkSize);
        buf_pos += m_CurrentChunkSize;
        buf_size -= m_CurrentChunkSize;
    }

    size_t bytes_copied = m_CurrentChunkSize;

    m_CurrentChunkSize = 0;

    if (m_EOF) {
        if (bytes_read != NULL)
            *bytes_read = bytes_copied;
        return bytes_copied ? eRW_Success : eRW_Eof;
    }

    try {
        while (buf_size > 0) {
            switch (m_UTTPReader.GetNextEvent()) {
            case CUTTPReader::eChunkPart:
            case CUTTPReader::eChunk:
                m_CurrentChunk = m_UTTPReader.GetChunkPart();
                m_CurrentChunkSize = m_UTTPReader.GetChunkPartSize();

                if (m_CurrentChunkSize >= buf_size) {
                    memcpy(buf_pos, m_CurrentChunk, buf_size);
                    m_CurrentChunk += buf_size;
                    m_CurrentChunkSize -= buf_size;
                    if (bytes_read != NULL)
                        *bytes_read = bytes_copied + buf_size;
                    return eRW_Success;
                }

                memcpy(buf_pos, m_CurrentChunk, m_CurrentChunkSize);
                buf_pos += m_CurrentChunkSize;
                buf_size -= m_CurrentChunkSize;
                bytes_copied += m_CurrentChunkSize;
                m_CurrentChunkSize = 0;
                break;

            case CUTTPReader::eControlSymbol:
                if (m_UTTPReader.GetControlSymbol() != END_OF_DATA_MARKER) {
                    NCBI_THROW_FMT(CNetStorageException, eIOError,
                            "NetStorage API: invalid end-of-data-stream "
                            "terminator: " <<
                                    (int) m_UTTPReader.GetControlSymbol());
                }
                m_EOF = true;
                ReadConfirmation();
                if (bytes_read != NULL)
                    *bytes_read = bytes_copied;
                return bytes_copied ? eRW_Success : eRW_Eof;

            case CUTTPReader::eEndOfBuffer:
                s_ReadSocket(m_Connection->m_Socket, m_ReadBuffer,
                        READ_BUFFER_SIZE, &bytes_read_local);

                m_UTTPReader.SetNewBuffer(m_ReadBuffer, bytes_read_local);
                break;

            default:
                NCBI_THROW_FMT(CNetStorageException, eIOError,
                        "NetStorage API: invalid UTTP status "
                        "while reading " << m_Locator);
            }
        }

        if (bytes_read != NULL)
            *bytes_read = bytes_copied;
        return eRW_Success;
    }
    catch (...) {
        m_State = eReady;
        m_UTTPReader.Reset();
        m_Connection->Close();
        m_Connection = NULL;
        throw;
    }
}

bool SNetStorageObjectRPC::Eof()
{
    switch (m_State) {
    case eReady:
        return false;

    case eReading:
        return m_CurrentChunkSize == 0 && m_EOF;

    default: /* case eWriting: */
        NCBI_THROW_FMT(CNetStorageException, eInvalidArg,
            "Cannot read a NetStorage file while writing");
    }
}

ERW_Result SNetStorageObjectRPC::Write(const void* buf_pos, size_t buf_size,
        size_t* bytes_written)
{
    if (m_State == eReading) {
        NCBI_THROW_FMT(CNetStorageException, eInvalidArg,
            "Cannot write a NetStorage file while reading");
    }

    if (m_State == eReady) {
        m_NetStorageRPC->m_UseNextSubHitID.ProperCommand();
        m_OriginalRequest = x_MkRequest("WRITE");

        m_Locator = ExchangeUsingOwnService(m_OriginalRequest,
                &m_Connection).GetString("ObjectLoc");

        m_State = eWriting;
    }

    try {
        s_SendUTTPChunk(reinterpret_cast<const char*>(buf_pos),
                buf_size, m_Connection->m_Socket);
        if (bytes_written != NULL)
            *bytes_written = buf_size;
        return eRW_Success;
    }
    catch (exception&) {
        m_State = eReady;
        m_Connection->Close();
        m_Connection = NULL;
        throw;
    }
}

Uint8 SNetStorageObjectRPC::GetSize()
{
    if (m_State != eReady) {
        NCBI_THROW_FMT(CNetStorageException, eInvalidArg,
                "Cannot get object size while reading or writing");
    }

    CJsonNode request(x_MkRequest("GETSIZE"));

    return (Uint8) ExchangeUsingOwnService(request).GetInteger("Size");
}

list<string> SNetStorageObjectRPC::GetAttributeList() const
{
    if (m_State != eReady) {
        NCBI_THROW_FMT(CNetStorageException, eInvalidArg,
                "Cannot get object attribute while reading or writing");
    }

    CJsonNode request(x_MkRequest("GETATTRLIST"));

    CJsonNode reply(ExchangeUsingOwnService(request));
    CJsonNode names(reply.GetByKeyOrNull("AttributeNames"));
    list<string> result;

    if (names) {
        for (CJsonIterator it = names.Iterate(); it; ++it) {
            result.push_back((*it).AsString());
        }
    }

    return result;
}

string SNetStorageObjectRPC::GetAttribute(const string& attr_name) const
{
    if (m_State != eReady) {
        NCBI_THROW_FMT(CNetStorageException, eInvalidArg,
                "Cannot get object attribute while reading or writing");
    }

    CJsonNode request(x_MkRequest("GETATTR"));

    request.SetString("AttrName", attr_name);

    return ExchangeUsingOwnService(request).GetString("AttrValue");
}

void SNetStorageObjectRPC::SetAttribute(const string& attr_name,
        const string& attr_value)
{
    if (m_State != eReady) {
        NCBI_THROW_FMT(CNetStorageException, eInvalidArg,
                "Cannot set object attribute while reading or writing");
    }

    CJsonNode request(x_MkRequest("SETATTR"));

    request.SetString("AttrName", attr_name);
    request.SetString("AttrValue", attr_value);

    ExchangeUsingOwnService(request);
}

CNetStorageObjectInfo SNetStorageObjectRPC::GetInfo()
{
    if (m_State != eReady) {
        NCBI_THROW_FMT(CNetStorageException, eInvalidArg,
                "Cannot get object info while reading or writing");
    }

    CJsonNode request(x_MkRequest("GETOBJECTINFO"));

    return g_CreateNetStorageObjectInfo(m_Locator,
            ExchangeUsingOwnService(request));
}

void SNetStorageObjectRPC::SetExpiration(const CTimeout& ttl)
{
    if (m_State != eReady) {
        NCBI_THROW_FMT(CNetStorageException, eInvalidArg,
                "Cannot set expiration while reading or writing");
    }

    CJsonNode request(x_MkRequest("SETEXPTIME"));

    if (ttl.IsFinite()) {
        request.SetString("TTL", ttl.GetAsTimeSpan().AsString("dTh:m:s"));
    } else {
        request.SetString("TTL", "infinity");
    }

    ExchangeUsingOwnService(request);
}

string SNetStorageObjectRPC::FileTrack_Path()
{
    CJsonNode request(x_MkRequest("LOCKFTPATH"));

    return ExchangeUsingOwnService(request).GetString("Path");
}

void SNetStorageObjectRPC::Close()
{
    if (m_State == eReady)
        return;

    CNetServerConnection conn_copy(m_Connection);
    m_Connection = NULL;

    if (m_State == eReading) {
        m_State = eReady;

        if (!m_EOF) {
            m_UTTPReader.Reset();
            conn_copy->Close();
        }
    } else { /* m_State == eWriting */
        m_State = eReady;

        CSocket& sock = conn_copy->m_Socket;

        s_SendEndOfData(sock);

        CReadJsonFromSocket message_reader;

        s_TrapErrors(m_OriginalRequest,
                message_reader.ReadMessage(sock), sock,
                m_NetStorageRPC->m_Config.err_mode);
    }
}

CJsonNode SNetStorageObjectRPC::x_MkRequest(const string& request_type) const
{
    if (m_ObjectIdentification == eByGeneratedID)
        return m_NetStorageRPC->MkObjectRequest(request_type, m_Locator);
    else
        return m_NetStorageRPC->MkObjectRequest(request_type,
                m_UniqueKey, m_Flags);
}

ERW_Result SNetStorageObjectImpl::PendingCount(size_t* count)
{
    *count = 0;
    return eRW_Success;
}

ERW_Result SNetStorageObjectImpl::Flush()
{
    return eRW_Success;
}

void SNetStorageObjectImpl::Abort()
{
    Close();
}

IReader& SNetStorageObjectImpl::GetReader()
{
    return *this;
}

IEmbeddedStreamWriter& SNetStorageObjectImpl::GetWriter()
{
    return *this;
}

void SNetStorageObjectImpl::Read(string* data)
{
    char buffer[READ_CHUNK_SIZE];

    data->resize(0);
    size_t bytes_read;

    do {
        Read(buffer, sizeof(buffer), &bytes_read);
        data->append(buffer, bytes_read);
    } while (!Eof());

    Close();
}

struct SNetStorageByKeyRPC : public SNetStorageByKeyImpl
{
    SNetStorageByKeyRPC(const TConfig& config,
            TNetStorageFlags default_flags);

    virtual CNetStorageObject Open(const string& unique_key,
            TNetStorageFlags flags = 0);
    virtual string Relocate(const string& unique_key,
            TNetStorageFlags flags, TNetStorageFlags old_flags = 0);
    virtual bool Exists(const string& key, TNetStorageFlags flags = 0);
    virtual ENetStorageRemoveResult Remove(const string& key,
            TNetStorageFlags flags = 0);

    CRef<SNetStorageRPC,
            CNetComponentCounterLocker<SNetStorageRPC> > m_NetStorageRPC;
};

SNetStorageByKeyRPC::SNetStorageByKeyRPC(const TConfig& config,
        TNetStorageFlags default_flags) :
    m_NetStorageRPC(new SNetStorageRPC(config, default_flags))
{
    if (m_NetStorageRPC->m_Config.app_domain.empty()) {
        NCBI_THROW_FMT(CNetStorageException, eInvalidArg,
                "'domain' parameter is missing from the initialization string");
    }
}

CNetStorageObject SNetStorageByKeyRPC::Open(const string& unique_key,
        TNetStorageFlags flags)
{
    return new SNetStorageObjectRPC(m_NetStorageRPC, NULL, NULL,
            SNetStorageObjectRPC::eByUniqueKey,
            unique_key, flags, SNetStorageObjectRPC::eReady);
}

string SNetStorageByKeyRPC::Relocate(const string& unique_key,
        TNetStorageFlags flags, TNetStorageFlags old_flags)
{
    m_NetStorageRPC->m_UseNextSubHitID.ProperCommand();
    CJsonNode request(m_NetStorageRPC->MkObjectRequest(
            "RELOCATE", unique_key, old_flags));

    CJsonNode new_location(CJsonNode::NewObjectNode());

    SNetStorageRPC::x_SetStorageFlags(new_location, flags);

    request.SetByKey("NewLocation", new_location);

    return m_NetStorageRPC->Exchange(m_NetStorageRPC->m_Service,
            request).GetString("ObjectLoc");
}

bool SNetStorageByKeyRPC::Exists(const string& key, TNetStorageFlags flags)
{
    CJsonNode request(m_NetStorageRPC->MkObjectRequest("EXISTS", key, flags));

    return m_NetStorageRPC->Exchange(m_NetStorageRPC->m_Service,
            request).GetBoolean("Exists");
}

ENetStorageRemoveResult SNetStorageByKeyRPC::Remove(const string& key,
        TNetStorageFlags flags)
{
    m_NetStorageRPC->m_UseNextSubHitID.ProperCommand();
    CJsonNode request(m_NetStorageRPC->MkObjectRequest("DELETE", key, flags));

    CJsonNode response(
            m_NetStorageRPC->Exchange(m_NetStorageRPC->m_Service, request));

    CJsonNode not_found(response.GetByKeyOrNull("NotFound"));

    return not_found && not_found.AsBoolean() ? eNSTRR_NotFound : eNSTRR_Removed;
}

struct SNetStorageAdminImpl : public CObject
{
    SNetStorageAdminImpl(CNetStorage::TInstance netstorage_impl) :
        m_NetStorageRPC(static_cast<SNetStorageRPC*>(netstorage_impl))
    {
    }

    CRef<SNetStorageRPC,
            CNetComponentCounterLocker<SNetStorageRPC> > m_NetStorageRPC;
};

CNetStorageAdmin::CNetStorageAdmin(CNetStorage::TInstance netstorage_impl) :
    m_Impl(new SNetStorageAdminImpl(netstorage_impl))
{
}

CNetService CNetStorageAdmin::GetService()
{
    return m_Impl->m_NetStorageRPC->m_Service;
}

CJsonNode CNetStorageAdmin::MkNetStorageRequest(const string& request_type)
{
    return m_Impl->m_NetStorageRPC->MkStdRequest(request_type);
}

CJsonNode CNetStorageAdmin::ExchangeJson(const CJsonNode& request,
        CNetServer::TInstance server_to_use, CNetServerConnection* conn)
{
    return m_Impl->m_NetStorageRPC->Exchange(m_Impl->m_NetStorageRPC->m_Service,
            request, conn, server_to_use);
}

SNetStorageImpl* SNetStorage::CreateImpl(const SConfig& config,
        TNetStorageFlags default_flags)
{
    return new SNetStorageRPC(config, default_flags);
}

SNetStorageByKeyImpl* SNetStorage::CreateByKeyImpl(const SConfig& config,
        TNetStorageFlags default_flags)
{
    return new SNetStorageByKeyRPC(config, default_flags);
}

END_NCBI_SCOPE
