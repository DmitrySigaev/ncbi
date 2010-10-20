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
 * Author: Maxim Didenko
 *
 * File Description:
 *   Implementation of net cache client.
 *
 */

#include <ncbi_pch.hpp>

#include "netcache_rw.hpp"

#include <connect/services/netcache_api_expt.hpp>
#include <connect/services/error_codes.hpp>


#define NCBI_USE_ERRCODE_X   ConnServ_NetCache


BEGIN_NCBI_SCOPE

#define CACHE_XFER_BUFFER_SIZE 4096
#define MAX_PENDING_COUNT (1024 * 1024 * 1024)

static const string s_InputBlobCachePrefix = ".nc_cache_input.";
static const string s_OutputBlobCachePrefix = ".nc_cache_output.";

CNetCacheReader::CNetCacheReader(SNetCacheAPIImpl* impl,
        const CNetServer::SExecResult& exec_result,
        size_t* blob_size_ptr,
        CNetCacheAPI::ECachingMode caching_mode) :
    m_Connection(exec_result.conn),
    m_CachingEnabled(caching_mode == CNetCacheAPI::eCaching_AppDefault ?
        impl->m_CacheInput : caching_mode == CNetCacheAPI::eCaching_Enable)
{
    string::size_type pos = exec_result.response.find("SIZE=");

    if (pos == string::npos) {
        NCBI_THROW(CNetCacheException, eInvalidServerResponse,
            "No SIZE field in reply to the blob reading command");
    }

    m_BlobBytesToRead = NStr::StringToUInt8(
        exec_result.response.c_str() + pos + sizeof("SIZE=") - 1);

    if (blob_size_ptr != NULL) {
        *blob_size_ptr = CheckBlobSize(m_BlobBytesToRead);
    }

    if (m_CachingEnabled) {
        m_CacheFile.CreateTemporary(impl->m_TempDir, s_InputBlobCachePrefix);

        char buf[CACHE_XFER_BUFFER_SIZE];
        Uint8 bytes_to_read = m_BlobBytesToRead;

        while (bytes_to_read > 0) {
            size_t bytes_read = 0;
            SocketRead(buf, sizeof(buf) <= bytes_to_read ?
                sizeof(buf) : (size_t) bytes_to_read, &bytes_read);
            m_CacheFile.Write(buf, bytes_read);
            bytes_to_read -= bytes_read;
        }

        m_Connection = NULL;

        if (m_CacheFile.GetFilePos() != m_BlobBytesToRead) {
            NCBI_THROW(CNetCacheException, eBlobClipped,
                "Blob size is greater than the amount of data cached for it");
        }

        m_CacheFile.Flush();
        m_CacheFile.SetFilePos(0);
    }
}

CNetCacheReader::~CNetCacheReader()
{
    try {
        Close();
    } NCBI_CATCH_ALL_X(10, "CNetCacheReader::~CNetCacheReader()");
}

void CNetCacheReader::Close()
{
    if (m_CachingEnabled)
        m_CacheFile.Close();
    else if (m_BlobBytesToRead != 0)
        m_Connection->Abort();
}

ERW_Result CNetCacheReader::Read(void*   buf,
                                 size_t  count,
                                 size_t* bytes_read_ptr)
{
    if (m_BlobBytesToRead == 0) {
        if (bytes_read_ptr != NULL)
            *bytes_read_ptr = 0;
        return eRW_Eof;
    }

    if (m_BlobBytesToRead < count)
        count = (size_t) m_BlobBytesToRead;

    size_t bytes_read = 0;

    if (count > 0) {
        if (!m_CachingEnabled)
            SocketRead(buf, count, &bytes_read);
        else if ((bytes_read = m_CacheFile.Read(buf, count)) == 0)
            ReportPrematureEOF();

        m_BlobBytesToRead -= bytes_read;
    }

    if (bytes_read_ptr != NULL)
        *bytes_read_ptr = bytes_read;

    return eRW_Success;
}

ERW_Result CNetCacheReader::PendingCount(size_t* count)
{
    if (m_CachingEnabled || m_BlobBytesToRead == 0) {
        *count = m_BlobBytesToRead < MAX_PENDING_COUNT ?
            (size_t) m_BlobBytesToRead : MAX_PENDING_COUNT;
        return eRW_Success;
    } else {
        return CSocketReaderWriter(&m_Connection->m_Socket).PendingCount(count);
    }
}

void CNetCacheReader::SocketRead(void* buf, size_t count, size_t* bytes_read)
{
    EIO_Status status = m_Connection->m_Socket.Read(buf, count, bytes_read);

    switch (status) {
    case eIO_Success:
        break;

    case eIO_Timeout:
        NCBI_THROW(CNetServiceException, eTimeout,
            "Timeout while reading blob contents");

    case eIO_Closed:
        if (count > *bytes_read)
            ReportPrematureEOF();
        break;

    default:
        NCBI_THROW_FMT(CNetServiceException, eCommunicationError,
            "Error while reading blob: " << IO_StatusStr(status));
    }
}

void CNetCacheReader::ReportPrematureEOF()
{
    m_BlobBytesToRead = 0;
    NCBI_THROW_FMT(CNetCacheException, eBlobClipped,
        "Amount read is less than the expected blob size "
        "(premature EOF while reading from " <<
        m_Connection->m_Server->m_Address.AsString() << ")");
}

/////////////////////////////////////////////////
CNetCacheWriter::CNetCacheWriter(SNetCacheAPIImpl* impl,
        string* blob_id,
        unsigned time_to_live,
        ENetCacheResponseType response_type,
        CNetCacheAPI::ECachingMode caching_mode) :
    m_NetCacheAPI(impl),
    m_BlobID(*blob_id),
    m_TimeToLive(time_to_live),
    m_ResponseType(response_type),
    m_CachingEnabled(caching_mode == CNetCacheAPI::eCaching_AppDefault ?
        impl->m_CacheOutput : caching_mode == CNetCacheAPI::eCaching_Enable)
{
    if (m_CachingEnabled)
        m_CacheFile.CreateTemporary(impl->m_TempDir, s_OutputBlobCachePrefix);

    if (!m_CachingEnabled || blob_id->empty()) {
        EstablishConnection();
        *blob_id = m_BlobID;
    }
}

CNetCacheWriter::~CNetCacheWriter()
{
    try {
        Close();
    } NCBI_CATCH_ALL_X(11, "Exception in ~CNetCacheWriter() [IGNORED]");
}

void CNetCacheWriter::Close()
{
    if (m_CachingEnabled) {
        m_CacheFile.Flush();

        bool blob_written = false;

        if (IsConnectionOpen()) {
            try {
                UploadCacheFile();
                blob_written = true;
            }
            catch (CNetServiceException&) {
            }
        }

        if (!blob_written) {
            EstablishConnection();

            UploadCacheFile();
        }
    } else if (!IsConnectionOpen())
        return;

    ERW_Result res = m_TransmissionWriter->Close();

    if (res != eRW_Success) {
        AbortConnection();
        if (res == eRW_Timeout) {
            NCBI_THROW(CNetServiceException, eTimeout,
                "Timeout while sending EOF packet");
        } else {
            NCBI_THROW(CNetServiceException, eCommunicationError,
                "IO error while sending EOF packet");
        }
    }

    if (m_ResponseType == eNetCache_Wait) {
        try {
            string dummy;
            m_Connection->ReadCmdOutputLine(dummy);
        }
        catch (...) {
            AbortConnection();
            throw;
        }
    }

    ResetWriters();

    m_Connection = NULL;
}

ERW_Result CNetCacheWriter::Write(const void* buf,
                                  size_t      count,
                                  size_t*     bytes_written_ptr)
{
    if (m_CachingEnabled) {
        size_t bytes_written = m_CacheFile.Write(buf, count);
        if (bytes_written_ptr != NULL)
            *bytes_written_ptr = bytes_written;
    } else if (IsConnectionOpen())
        Transmit(buf, count, bytes_written_ptr);
    else
        return eRW_Error;

    return eRW_Success;
}

ERW_Result CNetCacheWriter::Flush(void)
{
    if (!m_CachingEnabled && IsConnectionOpen())
        m_TransmissionWriter->Flush();

    return eRW_Success;
}

void CNetCacheWriter::WriteBufferAndClose(const char* buf_ptr, size_t buf_size)
{
    size_t bytes_written;

    while (buf_size > 0) {
        Write(buf_ptr, buf_size, &bytes_written);
        buf_ptr += bytes_written;
        buf_size -= bytes_written;
    }

    Close();
}

void CNetCacheWriter::ResetWriters()
{
    try {
        m_TransmissionWriter.reset();
        m_SocketReaderWriter.reset();
    }
    catch (...) {
    }
}

void CNetCacheWriter::EstablishConnection()
{
    ResetWriters();

    m_Connection = m_NetCacheAPI->InitiateWriteCmd(&m_BlobID, m_TimeToLive);

    m_SocketReaderWriter.reset(
        new CSocketReaderWriter(&m_Connection->m_Socket));

    m_TransmissionWriter.reset(
        new CTransmissionWriter(m_SocketReaderWriter.get(),
            eNoOwnership, CTransmissionWriter::eSendEofPacket));
}

void CNetCacheWriter::AbortConnection()
{
    ResetWriters();

    if (m_Connection->m_Socket.GetStatus(eIO_Open) != eIO_Closed)
        m_Connection->Abort();

    m_Connection = NULL;
}

void CNetCacheWriter::Transmit(const void* buf,
    size_t count, size_t* bytes_written)
{
    ERW_Result res = m_TransmissionWriter->Write(buf, count, bytes_written);

    try {
        if (res != eRW_Success /*&& res != eRW_NotImplemented*/) {
            NCBI_THROW(CNetServiceException, eCommunicationError,
                g_RW_ResultToString(res));
        }

        STimeout to = {0, 0};
        switch (m_Connection->m_Socket.Wait(eIO_Read, &to)) {
            case eIO_Success:
                {
                    string msg;
                    if (m_Connection->m_Socket.ReadLine(msg) != eIO_Closed) {
                        if (msg.empty())
                            break;
                        if (msg.find("ERR:") == 0) {
                            msg.erase(0, 4);
                            msg = NStr::ParseEscapes(msg);
                        }
                        NCBI_THROW(CNetCacheException, eServerError, msg);
                    } // else FALL THROUGH
                }

            case eIO_Closed:
                NCBI_THROW(CNetServiceException, eCommunicationError,
                    "Server closed communication channel (timeout?)");

            default:
                break;
        }
    }
    catch (...) {
        AbortConnection();
        throw;
    }
}

void CNetCacheWriter::UploadCacheFile()
{
    char buf[CACHE_XFER_BUFFER_SIZE];
    size_t bytes_read;
    size_t bytes_written;

    m_CacheFile.SetFilePos(0);
    while ((bytes_read = m_CacheFile.Read(buf, sizeof(buf))) > 0)
        Transmit(buf, bytes_read, &bytes_written);
}

END_NCBI_SCOPE
