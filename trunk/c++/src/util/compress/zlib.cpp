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
 * Authors:  Vladimir Ivanov
 *           Jean-loup Gailly, Mark Adler
 *           (used a part of zlib library code from files gzio.c, uncompr.c)
 *
 * File Description:  ZLib Compression API
 *
 * NOTE: The zlib documentation can be found here: 
 *           http://zlib.org,  
 *           http://www.gzip.org/zlib/manual.html
 */


#include <ncbi_pch.hpp>
#include <corelib/ncbi_limits.h>
#include <corelib/ncbifile.hpp>
#include <util/compress/zlib.hpp>
#include <zlib.h>

#include <stdio.h>

BEGIN_NCBI_SCOPE


// Define some macro if not defined
#ifndef DEF_MEM_LEVEL
#  if MAX_MEM_LEVEL >= 8
#    define DEF_MEM_LEVEL 8
#  else
#    define DEF_MEM_LEVEL  MAX_MEM_LEVEL
#  endif
#endif

// Identify as Unix by default.
#ifndef OS_CODE
#  define OS_CODE 0x03
#endif

// Macro to check bits
#define F_ISSET(mask) ((GetFlags() & (mask)) == (mask))

// Get compression stream pointer
#define STREAM ((z_stream*)m_Stream)

// Convert 'size_t' to '[unsigned] int' which used internally in zlib
#define LIMIT_SIZE_PARAM(value) if (value > (size_t)kMax_Int) value = kMax_Int
#define LIMIT_SIZE_PARAM_U(value) if (value > kMax_UInt) value = kMax_UInt

// Maximum size of gzip file header
const size_t kMaxHeaderSize = 1024*4;


//////////////////////////////////////////////////////////////////////////////
//
// CZipCompression
//

CZipCompression::CZipCompression(ELevel level,  int window_bits,
                                 int mem_level, int strategy)
    : CCompression(level)
{
    m_WindowBits = ( window_bits == kZlibDefaultWbits ) ? MAX_WBITS :
                                                          window_bits;
    m_MemLevel   = ( mem_level == kZlibDefaultMemLevel )? DEF_MEM_LEVEL :
                                                          mem_level;
    m_Strategy   = ( strategy == kZlibDefaultStrategy ) ? Z_DEFAULT_STRATEGY :
                                                          strategy;
    // Initialize the compressor stream structure
    m_Stream = new z_stream;
    if ( m_Stream ) {
        memset(m_Stream, 0, sizeof(z_stream));
    }
    return;
}


CZipCompression::~CZipCompression()
{
    delete STREAM;
    return;
}


// gzip magic header
const int gz_magic[2] = {0x1f, 0x8b};

// gzip flag byte
#define ASCII_FLAG   0x01 // bit 0 set: file probably ascii text
#define HEAD_CRC     0x02 // bit 1 set: header CRC present
#define EXTRA_FIELD  0x04 // bit 2 set: extra field present
#define ORIG_NAME    0x08 // bit 3 set: original file name present
#define COMMENT      0x10 // bit 4 set: file comment present
#define RESERVED     0xE0 // bits 5..7: reserved/


// Store only 4 bytes for value
static void s_StoreUI4(unsigned char* buf, unsigned long value)
{
    if ( value > kMax_UI4 ) {
        ERR_POST("CZipCompression:  Stored value " \
                 "exceeded maximum size for Uint4 type");
    }
    for (int i = 0; i < 4; i++) {
        buf[i] = (unsigned char)(value & 0xff);
        value >>= 8;
    }
}


static void s_GetUI4(unsigned char* buf, unsigned long& value)
{
    value = 0;
    for (int i = 3; i >= 0; i--) {
        value <<= 8;
        value += buf[i];
    }
}


// Returns length of the .gz header if it exist or 0 otherwise.
// If 'info' not NULL, fill it with information from header.
static
size_t s_CheckGZipHeader(const void* src_buf, size_t src_len,
                         CZipCompression::SFileInfo* info = 0)
{
    unsigned char* buf = (unsigned char*)src_buf;
    // .gz header cannot be less than 10 bytes
    if (src_len < 10) {
        return 0;
    }
    // Check the gzip magic header
    if (buf[0] != gz_magic[0]  ||
        buf[1] != gz_magic[1]) {
        return 0;
    }
    int method = buf[2];
    int flags  = buf[3];
    if (method != Z_DEFLATED  ||  (flags & RESERVED) != 0) {
        return 0;
    }
    // Header length: 
    // gz_magic (2) + methos (1) + flags (1) + time, xflags and OS code (6)
    size_t header_len = 10; 

    if ( info ) {
        s_GetUI4((unsigned char*)buf+4, (unsigned long&)info->mtime);
    }

    // Skip the extra fields
    if ((flags & EXTRA_FIELD) != 0) {
        if (header_len + 2 > src_len) {
            return 0;
        }
        size_t len = buf[header_len++];
        len += ((size_t)buf[header_len++])<<8;
        header_len += len;
    }
    // Skip the original file name
    if ((flags & ORIG_NAME) != 0) {
        size_t pos = header_len;
        while (header_len < src_len  &&  buf[header_len++] != 0);
        if ( info ) {
            info->name.assign((char*)buf+pos, header_len-pos);
        }
    }
    // Skip the file comment
    if ((flags & COMMENT) != 0) {
        size_t pos = header_len;
        while (header_len < src_len  &&  buf[header_len++] != 0);
        if ( info ) {
            info->comment.assign((char*)buf+pos, header_len-pos);
        }
    }
    // Skip the header CRC
    if ((flags & HEAD_CRC) != 0) {
        header_len += 2;
    }
    if (header_len > src_len) {
        return 0;
    }
    return header_len;
}


static size_t s_WriteGZipHeader(void* src_buf, size_t buf_size,
                                const CZipCompression::SFileInfo* info = 0)
{
    char* buf = (char*)src_buf;

    // .gz header cannot be less than 10 bytes
    if (buf_size < 10) {
        return 0;
    }
    unsigned char flags = 0;
    size_t header_len = 10;  // write beginnning of header later

    // Store the original file name.
    // Store it only if buffer have enough size.
    if ( info  &&  !info->name.empty()  &&
         buf_size > (info->name.length() + header_len) ) {
        flags |= ORIG_NAME;
        strncpy((char*)buf+header_len, info->name.data(),info->name.length());
        header_len += info->name.length();
        buf[header_len++] = '\0';
    }
    // Store file comment.
    // Store it only if buffer have enough size.
    if ( info  &&  !info->comment.empty()  &&
         buf_size > (info->comment.length() + header_len) ) {
        flags |= COMMENT;
        strncpy((char*)buf+header_len, info->comment.data(),
                info->comment.length());
        header_len += info->comment.length();
        buf[header_len++] = '\0';
    }

    // Set beginning of header
    memset(buf, 0, 10);
    buf[0] = gz_magic[0];
    buf[1] = gz_magic[1];
    buf[2] = Z_DEFLATED;
    buf[3] = flags;
    /* 4-7 mtime */
    if ( info  &&  info->mtime ) {
        s_StoreUI4((unsigned char*)buf+4, info->mtime);
    }
    /* 8 - xflags == 0*/
    buf[9] = OS_CODE;

    return header_len;
}


static size_t s_WriteGZipFooter(void*         buf,
                                size_t        buf_size,
                                unsigned long total,
                                unsigned long crc)
{
    // .gz footer is 8 bytes length
    if (buf_size < 8) {
        return 0;
    }
    s_StoreUI4((unsigned char*)buf, crc);
    s_StoreUI4((unsigned char*)buf+4, total);

    return 8;
}


void s_CollectFileInfo(const string& filename, 
                       CZipCompression::SFileInfo& info)
{
    CFile file(filename);
    info.name = file.GetName();
    time_t mtime;
    file.GetTimeT(&mtime);
    info.mtime = mtime;
}


bool CZipCompression::CompressBuffer(
                      const void* src_buf, size_t  src_len,
                      void*       dst_buf, size_t  dst_size,
                      /* out */            size_t* dst_len)
{
    // Check parameters
    if ( !src_buf || !src_len ) {
        *dst_len = 0;
        SetError(Z_OK);
        return true;
    }
    if ( !dst_buf || !dst_len ) {
        SetError(Z_STREAM_ERROR, "bad argument");
        ERR_POST(FormatErrorMessage("CZipCompression::CompressBuffer"));
        return false;
    }
    LIMIT_SIZE_PARAM_U(src_len);
    LIMIT_SIZE_PARAM_U(dst_size);

    size_t    header_len = 0;
    int       errcode    = Z_OK;
    z_stream  stream;
    
    // Write gzip file header
    if ( F_ISSET(fWriteGZipFormat) ) {
        header_len = s_WriteGZipHeader(dst_buf, dst_size);
        if (!header_len) {
            ERR_POST(
                "CZipCompression::CompressBuffer:  Cannot write gzip header");
            return false;
        }
    }

    stream.next_in  = (unsigned char*)src_buf;
    stream.avail_in = (unsigned int)src_len;
#ifdef MAXSEG_64K
    // Check for source > 64K on 16-bit machine:
    if ( stream.avail_in != src_len ) {
        SetError(Z_BUF_ERROR, zError(Z_BUF_ERROR));
        ERR_POST(FormatErrorMessage("CZipCompression::CompressBuffer"));
        return false;
    }
#endif
    stream.next_out = (unsigned char*)dst_buf + header_len;
    stream.avail_out = (unsigned int)(dst_size - header_len);
    if ( stream.avail_out != dst_size - header_len ) {
        SetError(Z_BUF_ERROR, zError(Z_BUF_ERROR));
        ERR_POST(FormatErrorMessage("CZipCompression::CompressBuffer"));
        return false;
    }

    stream.zalloc = (alloc_func)0;
    stream.zfree  = (free_func)0;
    stream.opaque = (voidpf)0;

    errcode = deflateInit2_(&stream, GetLevel(), Z_DEFLATED,
                            header_len ? -m_WindowBits : m_WindowBits,
                            m_MemLevel, m_Strategy,
                            ZLIB_VERSION, (int)sizeof(z_stream));
    if (errcode == Z_OK) {
        errcode = deflate(&stream, Z_FINISH);
        *dst_len = stream.total_out + header_len;
        if (errcode == Z_STREAM_END) {
            errcode = deflateEnd(&stream);
        } else {
            if ( errcode == Z_OK ) {
                errcode = Z_BUF_ERROR;
            }
            deflateEnd(&stream);
        }
    }
    SetError(errcode, zError(errcode));
    if ( errcode != Z_OK ) {
        ERR_POST(FormatErrorMessage("CZipCompression::CompressBuffer"));
        return false;
    }

    // Write gzip file footer
    if ( F_ISSET(fWriteGZipFormat) ) {
        unsigned long crc = crc32(0L, (unsigned char*)src_buf,
                                      (unsigned int)src_len);
        size_t footer_len = s_WriteGZipFooter(
            (char*)dst_buf + *dst_len, dst_size, src_len, crc);
        if ( !footer_len ) {
            ERR_POST(
                "CZipCompression::CompressBuffer:  Cannot write gzip footer");
            return false;
        }
        *dst_len += footer_len;
    }
    return true;
}


bool CZipCompression::DecompressBuffer(
                      const void* src_buf, size_t  src_len,
                      void*       dst_buf, size_t  dst_size,
                      /* out */            size_t* dst_len)
{
    // Check parameters
    if ( !src_buf || !src_len ) {
        *dst_len = 0;
        SetError(Z_OK);
        return true;
    }
    if ( !dst_buf || !dst_len ) {
        SetError(Z_STREAM_ERROR, "bad argument");
        ERR_POST(FormatErrorMessage("CZipCompression::DecompressBuffer"));
        return false;
    }
    LIMIT_SIZE_PARAM_U(src_len);
    LIMIT_SIZE_PARAM_U(dst_size);

    size_t    header_len = 0;
    int       errcode    = Z_OK;
    z_stream  stream;
    
    // Check file header
    if ( F_ISSET(fCheckFileHeader) ) {
        // Check gzip header in the buffer
        header_len = s_CheckGZipHeader(src_buf, src_len);
    }
    stream.next_in  = (unsigned char*)src_buf + header_len;
    stream.avail_in = (unsigned int)(src_len - header_len);
    // Check for source > 64K on 16-bit machine:
    if ( stream.avail_in != src_len - header_len ) {
        SetError(Z_BUF_ERROR, zError(Z_BUF_ERROR));
        ERR_POST(FormatErrorMessage("CZipCompression::DecompressBuffer"));
        return false;
    }
    stream.next_out = (unsigned char*)dst_buf;
    stream.avail_out = (unsigned int)dst_size;
    if ( stream.avail_out != dst_size ) {
        SetError(Z_BUF_ERROR, zError(Z_BUF_ERROR));
        ERR_POST(FormatErrorMessage("CZipCompression::DecompressBuffer"));
        return false;
    }

    stream.zalloc = (alloc_func)0;
    stream.zfree  = (free_func)0;

    // "window bits" is passed < 0 to tell that there is no zlib header.
    // Note that in this case inflate *requires* an extra "dummy" byte
    // after the compressed stream in order to complete decompression and
    // return Z_STREAM_END. Here the gzip CRC32 ensures that 4 bytes are
    // present after the compressed stream.
        
    errcode = inflateInit2_(&stream, header_len ? -m_WindowBits :m_WindowBits,
                            ZLIB_VERSION, (int)sizeof(z_stream));

    if (errcode == Z_OK) {
        errcode = inflate(&stream, Z_FINISH);
        *dst_len = stream.total_out;
        if (errcode == Z_STREAM_END) {
            errcode = inflateEnd(&stream);
        } else {
            if ( errcode == Z_OK ) {
                errcode = Z_BUF_ERROR;
            }
            inflateEnd(&stream);
        }
    }
    SetError(errcode, zError(errcode));
    if ( errcode != Z_OK ) {
        ERR_POST(FormatErrorMessage("CZipCompression::DecompressBuffer"));
        return false;
    }
    return true;
}


bool CZipCompression::CompressFile(const string& src_file,
                                   const string& dst_file,
                                   size_t        buf_size)
{
    CZipCompressionFile cf(GetLevel(), m_WindowBits, m_MemLevel, m_Strategy);
    cf.SetFlags(GetFlags() | fWriteGZipFormat);

    // Collect info about compressed file
    CZipCompression::SFileInfo info;
/* +++
    if ( F_ISSET(fWriteGZipFormat) ) {
        s_CollectFileInfo(src_file, info);
    }
    // Open output file
    if ( !cf.Open(dst_file, CCompressionFile::eMode_Write,
        F_ISSET(fWriteGZipFormat) ? &info : 0) ) {
        return false;
    } 
*/
    // To support backward compatibility -- write .gz file format
    s_CollectFileInfo(src_file, info);
    // Open output file
    if ( !cf.Open(dst_file, CCompressionFile::eMode_Write, &info) ) {
        return false;
    } 
/* --- */
    // Make compression
    if ( CCompression::x_CompressFile(src_file, cf, buf_size) ) {
        return cf.Close();
    }
    // Save error info
    int    errcode = cf.GetErrorCode();
    string errmsg  = cf.GetErrorDescription();
    // Close file
    cf.Close();
    // Restore previous error info
    SetError(errcode, errmsg.c_str());
    return false;
}


bool CZipCompression::DecompressFile(const string& src_file,
                                     const string& dst_file,
                                     size_t        buf_size)
{
    CZipCompressionFile cf(GetLevel(), m_WindowBits, m_MemLevel, m_Strategy);
    cf.SetFlags(GetFlags() | fCheckFileHeader);

    if ( !cf.Open(src_file, CCompressionFile::eMode_Read) ) {
        return false;
    } 
    if ( CCompression::x_DecompressFile(cf, dst_file, buf_size) ) {
        return cf.Close();
    }
    // Restore previous error info
    int    errcode = cf.GetErrorCode();
    string errmsg  = cf.GetErrorDescription();
    // Restore previous error info
    cf.Close();
    // Restore previous error info
    SetError(errcode, errmsg.c_str());
    return false;
}


string CZipCompression::FormatErrorMessage(string where,
                                           bool use_stream_data) const
{
    string str = "[" + where + "]  " + GetErrorDescription();
    if ( use_stream_data ) {
        str += ";  error code = " +
               NStr::IntToString(GetErrorCode()) +
               ", number of processed bytes = " +
               NStr::UIntToString(STREAM->total_in);
    }
    return str + ".";
}



//////////////////////////////////////////////////////////////////////////////
//
// CZipCompressionFile
//


CZipCompressionFile::CZipCompressionFile(
    const string& file_name, EMode mode,
    ELevel level, int window_bits, int mem_level, int strategy)
    : CZipCompression(level, window_bits, mem_level, strategy),
      m_Mode(eMode_Read), m_File(0), m_Zip(0)
{
    if ( !Open(file_name, mode) ) {
        const string smode = (mode == eMode_Read) ? "reading" : "writing";
        NCBI_THROW(CCompressionException, eCompressionFile, 
                   "[CZipCompressionFile]  Cannot open file '" + file_name +
                   "' for " + smode + ".");
    }
    return;
}


CZipCompressionFile::CZipCompressionFile(
    ELevel level, int window_bits, int mem_level, int strategy)
    : CZipCompression(level, window_bits, mem_level, strategy),
      m_Mode(eMode_Read), m_File(0), m_Zip(0)
{
    return;
}


CZipCompressionFile::~CZipCompressionFile(void)
{
    Close();
    return;
}


bool CZipCompressionFile::Open(const string& file_name, EMode mode)
{
    return Open(file_name, mode, 0 /*info*/);
}


bool CZipCompressionFile::Open(const string& file_name, EMode mode,
                               SFileInfo* info)
{
    m_Mode = mode;

    // Open a file
    if ( mode == eMode_Read ) {
        m_File = new CNcbiFstream(file_name.c_str(),
                                  IOS_BASE::in | IOS_BASE::binary);
    } else {
        m_File = new CNcbiFstream(file_name.c_str(),
                                  IOS_BASE::out | IOS_BASE::binary |
                                  IOS_BASE::trunc);
    }
    if ( !m_File->good() ) {
        Close();
        return false;
    }
    // Get file information
    if ( mode == eMode_Read  &&  F_ISSET(fCheckFileHeader)  &&  info) {
        char buf[kMaxHeaderSize];
        m_File->read(buf, kMaxHeaderSize);
        m_File->seekg(0);
        s_CheckGZipHeader(buf, m_File->gcount(), info);
    }

    // Create compression stream for I/O
    if ( mode == eMode_Read ) {
        CZipDecompressor* decompressor = 
            new CZipDecompressor(m_WindowBits, GetFlags());
        CCompressionStreamProcessor* processor = 
            new CCompressionStreamProcessor(
                decompressor, CCompressionStreamProcessor::eDelete,
                kCompressionDefaultBufSize, kCompressionDefaultBufSize);
        m_Zip = 
            new CCompressionIOStream(
                *m_File, processor, 0, CCompressionStream::fOwnReader);
    } else {
        CZipCompressor* compressor = 
            new CZipCompressor(
                GetLevel(), m_WindowBits, m_MemLevel, m_Strategy, GetFlags());
        if ( F_ISSET(fWriteGZipFormat)  &&  info) {
            // Enable compressor to write info information about
            // compressed file into gzip file header
            compressor->SetFileInfo(*info);
        }
        CCompressionStreamProcessor* processor = 
            new CCompressionStreamProcessor(
                compressor, CCompressionStreamProcessor::eDelete,
                kCompressionDefaultBufSize, kCompressionDefaultBufSize);
        m_Zip = 
            new CCompressionIOStream(
                *m_File, 0, processor, CCompressionStream::fOwnWriter);
    }
    if ( !m_Zip->good() ) {
        Close();
        return false;
    }
    return true;
} 


long CZipCompressionFile::Read(void* buf, size_t len)
{
    LIMIT_SIZE_PARAM_U(len);

    if ( !m_Zip  ||  m_Mode != eMode_Read ) {
        NCBI_THROW(CCompressionException, eCompressionFile, 
            "[CZipCompressionFile::Read]  File must be opened for reading");
    }
    m_Zip->read((char*)buf, len);
    streamsize nread = m_Zip->gcount();
    if ( nread ) {
        return nread;
    }
    if ( m_Zip->eof() ) {
        return 0;
    }
    return -1;
}


long CZipCompressionFile::Write(const void* buf, size_t len)
{
    if ( !m_Zip  ||  m_Mode != eMode_Write ) {
        NCBI_THROW(CCompressionException, eCompressionFile, 
            "[CZipCompressionFile::Write]  File must be opened for writing");
    }
    // Redefine standard behaviour for case of writing zero bytes
    if (len == 0) {
        return 0;
    }
    LIMIT_SIZE_PARAM_U(len);

    m_Zip->write((char*)buf, len);
    if ( m_Zip->good() ) {
        return len;
    }
    return -1;
}


bool CZipCompressionFile::Close(void)
{
    // Close compression/decompression stream
    if ( m_Zip ) {
        m_Zip->Finalize();
        delete m_Zip;
        m_Zip = 0;
    }
    // Close file stream
    if ( m_File ) {
        m_File->close();
        delete m_File;
        m_File = 0;
    }
    return true;
}



//////////////////////////////////////////////////////////////////////////////
//
// CZipCompressor
//


CZipCompressor::CZipCompressor(ELevel level,  int window_bits,
                               int mem_level, int strategy, TFlags flags)
    : CZipCompression(level, window_bits, mem_level, strategy),
      m_CRC32(0), m_NeedWriteHeader(true)
{
    SetFlags(flags);
}


CZipCompressor::~CZipCompressor()
{
}


void CZipCompressor::SetFileInfo(const SFileInfo& info)
{
    m_FileInfo = info;
}


CCompressionProcessor::EStatus CZipCompressor::Init(void)
{
    // Initialize members
    Reset();
    SetBusy();
    m_CRC32 = 0;
    m_NeedWriteHeader = true;
    m_Cache.erase();
    //!!!m_Cache.reserve(kMaxHeaderSize);

    // Initialize the compressor stream structure
    memset(STREAM, 0, sizeof(z_stream));
    // Create a compressor stream
    int errcode = deflateInit2_(STREAM, GetLevel(), Z_DEFLATED,
                                F_ISSET(fWriteGZipFormat) ? -m_WindowBits :
                                                             m_WindowBits,
                                m_MemLevel, m_Strategy,
                                ZLIB_VERSION, (int)sizeof(z_stream));
    SetError(errcode, zError(errcode));

    if ( errcode == Z_OK ) {
        return eStatus_Success;
    }
    ERR_POST(FormatErrorMessage("CZipCompressor::Init"));
    return eStatus_Error;
}


CCompressionProcessor::EStatus CZipCompressor::Process(
                      const char* in_buf,  size_t  in_len,
                      char*       out_buf, size_t  out_size,
                      /* out */            size_t* in_avail,
                      /* out */            size_t* out_avail)
{
    LIMIT_SIZE_PARAM_U(in_len);
    LIMIT_SIZE_PARAM_U(out_size);

    size_t header_len = 0;

    // Write gzip file header
    if ( F_ISSET(fWriteGZipFormat)  &&  m_NeedWriteHeader ) {
        header_len = s_WriteGZipHeader(out_buf, out_size, &m_FileInfo);
        if (!header_len) {
            ERR_POST("CZipCompressor::Process:  Cannot write gzip header");
            return eStatus_Error;
        }
        m_NeedWriteHeader = false;
    }
    STREAM->next_in   = (unsigned char*)const_cast<char*>(in_buf);
    STREAM->avail_in  = (unsigned int)in_len;
    STREAM->next_out  = (unsigned char*)out_buf + header_len;
    STREAM->avail_out = (unsigned int)(out_size - header_len);

    int errcode = deflate(STREAM, Z_NO_FLUSH);
    SetError(errcode, zError(errcode));
    *in_avail  = STREAM->avail_in;
    *out_avail = out_size - STREAM->avail_out;
    IncreaseProcessedSize(in_len - *in_avail);
    IncreaseOutputSize(*out_avail);

    // If we writing in gzip file format
    if ( F_ISSET(fWriteGZipFormat) ) {
        // Update the CRC32 for processed data
        m_CRC32 = crc32(m_CRC32, (unsigned char*)in_buf,
                        (unsigned int)(in_len - *in_avail));
    }
    if ( errcode == Z_OK ) {
        return eStatus_Success;
    }
    ERR_POST(FormatErrorMessage("CZipCompressor::Process"));
    return eStatus_Error;
}


CCompressionProcessor::EStatus CZipCompressor::Flush(
                      char* out_buf, size_t  out_size,
                      /* out */      size_t* out_avail)
{
    if ( !out_size ) {
        return eStatus_Overflow;
    }
    LIMIT_SIZE_PARAM_U(out_size);

    STREAM->next_in   = 0;
    STREAM->avail_in  = 0;
    STREAM->next_out  = (unsigned char*)out_buf;
    STREAM->avail_out = (unsigned int)out_size;

    int errcode = deflate(STREAM, Z_SYNC_FLUSH);
    SetError(errcode, zError(errcode));
    *out_avail = out_size - STREAM->avail_out;
    IncreaseOutputSize(*out_avail);

    if ( errcode == Z_OK  ||  errcode == Z_BUF_ERROR ) {
        if ( STREAM->avail_out == 0) {
            return eStatus_Overflow;
        }
        return eStatus_Success;
    }
    ERR_POST(FormatErrorMessage("CZipCompressor::Flush"));
    return eStatus_Error;
}


CCompressionProcessor::EStatus CZipCompressor::Finish(
                      char* out_buf, size_t  out_size,
                      /* out */      size_t* out_avail)
{
    if ( !out_size ) {
        return eStatus_Overflow;
    }
    LIMIT_SIZE_PARAM_U(out_size);

    STREAM->next_in   = 0;
    STREAM->avail_in  = 0;
    STREAM->next_out  = (unsigned char*)out_buf;
    STREAM->avail_out = (unsigned int)out_size;

    int errcode = deflate(STREAM, Z_FINISH);
    SetError(errcode, zError(errcode));
    *out_avail = out_size - STREAM->avail_out;
    IncreaseOutputSize(*out_avail);

    switch (errcode) {
    case Z_OK:
        return eStatus_Overflow;
    case Z_STREAM_END:
        // Write .gz file footer
        if ( F_ISSET(fWriteGZipFormat) ) {
            size_t footer_len = 
                s_WriteGZipFooter(out_buf + *out_avail, STREAM->avail_out,
                                  GetProcessedSize(), m_CRC32);
            if ( !footer_len ) {
                ERR_POST("CZipCompressor::Finish: Cannot write gzip footer");
                return eStatus_Overflow;
            }
            *out_avail += footer_len;
        }
        return eStatus_EndOfData;
    }
    ERR_POST(FormatErrorMessage("CZipCompressor::Finish"));
    return eStatus_Error;
}


CCompressionProcessor::EStatus CZipCompressor::End(void)
{
    int errcode = deflateEnd(STREAM);
    SetError(errcode, zError(errcode));
    SetBusy(false);

    if ( errcode == Z_OK ) {
        return eStatus_Success;
    }
    ERR_POST(FormatErrorMessage("CZipCompressor::End"));
    return eStatus_Error;
}



//////////////////////////////////////////////////////////////////////////////
//
// CZipDecompressor
//


CZipDecompressor::CZipDecompressor(int window_bits, TFlags flags)
    : CZipCompression(eLevel_Default, window_bits, 0, 0),
      m_NeedCheckHeader(true)
{
    SetFlags(flags);
}


CZipDecompressor::~CZipDecompressor()
{
}


CCompressionProcessor::EStatus CZipDecompressor::Init(void)
{
    // Initialize members
    Reset();
    SetBusy();
    m_NeedCheckHeader = true;
    m_Cache.erase();
    m_Cache.reserve(kMaxHeaderSize);

    // Initialize the compressor stream structure
    memset(STREAM, 0, sizeof(z_stream));
    // Create a compressor stream
    int errcode = inflateInit2_(STREAM, m_WindowBits,
                                ZLIB_VERSION, (int)sizeof(z_stream));

    SetError(errcode, zError(errcode));

    if ( errcode == Z_OK ) {
        return eStatus_Success;
    }
    ERR_POST(FormatErrorMessage("CZipDecompressor::Init"));
    return eStatus_Error;
}


CCompressionProcessor::EStatus CZipDecompressor::Process(
                      const char* in_buf,  size_t  in_len,
                      char*       out_buf, size_t  out_size,
                      /* out */            size_t* in_avail,
                      /* out */            size_t* out_avail)
{
    if ( !out_size ) {
        return eStatus_Overflow;
    }
    LIMIT_SIZE_PARAM_U(in_len);
    LIMIT_SIZE_PARAM_U(out_size);

    STREAM->next_in   = (unsigned char*)const_cast<char*>(in_buf);
    STREAM->avail_in  = (unsigned int)in_len;
    STREAM->next_out  = (unsigned char*)out_buf;
    STREAM->avail_out = (unsigned int)out_size;

    bool    from_cache   = false;
    size_t  avail_adj    = in_len;
    size_t  old_avail_in = 0;

    // Check file header
    if ( F_ISSET(fCheckFileHeader) ) {
        size_t header_len = 0;
        if ( m_NeedCheckHeader ) {
            if (in_buf  &&  m_Cache.size() < kMaxHeaderSize) {
                size_t n = min(kMaxHeaderSize - m_Cache.size(), in_len);
                m_Cache.append(in_buf, n);
                avail_adj -= n;
                if (m_Cache.size() < kMaxHeaderSize) {
                    // Data block is very small and was cached.
                    *in_avail  = 0;
                    *out_avail = 0;
                    return eStatus_Success;
                }
            }
            // Check gzip header in the buffer
            header_len = s_CheckGZipHeader(m_Cache.c_str(), m_Cache.size());
            _ASSERT(header_len < kMaxHeaderSize);
            // If gzip header found, skip it
            if ( header_len ) {
                m_Cache.erase(0, header_len);
                inflateEnd(STREAM);
                int errcode = inflateInit2_(STREAM,
                                            header_len ? -m_WindowBits :
                                                          m_WindowBits,
                                            ZLIB_VERSION,
                                            (int)sizeof(z_stream));

                SetError(errcode, zError(errcode));
                if ( errcode != Z_OK ) {
                    return eStatus_Error;
                }
            }
            // Already skipped or we don't have header here
            if (header_len  ||  in_buf) {
                m_NeedCheckHeader = false;
            }
        }
        // Have some cached unprocessed data
        if ( m_Cache.size() ) {
            STREAM->next_in   = (unsigned char*)(m_Cache.c_str());
            STREAM->avail_in  = (unsigned int)m_Cache.size();
            STREAM->next_out  = (unsigned char*)out_buf;
            STREAM->avail_out = (unsigned int)out_size;
            from_cache        = true;
            old_avail_in      = STREAM->avail_in;
        }
    }

    int errcode = inflate(STREAM, Z_SYNC_FLUSH);
    SetError(errcode, zError(errcode));

    if ( from_cache ) {
        m_Cache.erase(0, old_avail_in - STREAM->avail_in);
        *in_avail = avail_adj;
        IncreaseProcessedSize(old_avail_in - STREAM->avail_in);
    } else {
        *in_avail = STREAM->avail_in;
        IncreaseProcessedSize(in_len - *in_avail);
    }
    *out_avail = out_size - STREAM->avail_out;
    IncreaseOutputSize(*out_avail);

    switch (errcode) {
    case Z_OK:
        if ( from_cache  &&  avail_adj == 0  &&  STREAM->avail_in > 0 ) {
            return eStatus_Overflow;
        }
        return eStatus_Success;
    case Z_STREAM_END:
        return eStatus_EndOfData;
    }
    ERR_POST(FormatErrorMessage("CZipDecompressor::Process"));
    return eStatus_Error;
}


CCompressionProcessor::EStatus CZipDecompressor::Flush(
                      char*   out_buf,
                      size_t  out_size,
                      size_t* out_avail)
{
    // Process cached data
    if (F_ISSET(fCheckFileHeader)  &&  m_Cache.size()) {
        size_t in_avail;
        return Process(0, 0, out_buf, out_size, &in_avail, out_avail);
    }
    return eStatus_Success;
}


CCompressionProcessor::EStatus CZipDecompressor::Finish(
                      char*   out_buf,
                      size_t  out_size,
                      size_t* out_avail)
{
    // Process cached data
    if (F_ISSET(fCheckFileHeader)  &&  m_Cache.size()) {
        size_t in_avail;
        return Process(0, 0, out_buf, out_size, &in_avail, out_avail);
    }
    return eStatus_Success;
}


CCompressionProcessor::EStatus CZipDecompressor::End(void)
{
    int errcode = inflateEnd(STREAM);
    SetBusy(false);
    if ( errcode == Z_OK ) {
        return eStatus_Success;
    }
    ERR_POST(FormatErrorMessage("CZipDecompressor::End"));
    return eStatus_Error;
}


END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.22  2005/06/09 11:10:39  ivanov
 * CZipCompression::CompressFile() -- always write .gz file format
 * to support backward compatibility.
 *
 * Revision 1.21  2005/06/06 10:52:48  ivanov
 * Rewritten CZipCompressionFile using compression streams.
 * CompressFile() now can write file`s name/mtime into gzip file header.
 *
 * Revision 1.20  2005/05/14 20:00:13  vakatov
 * CZipCompressionFile::Write() -- a thinko bug fixed
 *
 * Revision 1.19  2005/04/25 19:01:41  ivanov
 * Changed parameters and buffer sizes from being 'int', 'unsigned int' or
 * 'unsigned long' to unified 'size_t'
 *
 * Revision 1.18  2005/02/15 19:14:53  ivanov
 * CZipDecompressor::Process(): fixed memory leak if gzip file header present
 * in compressed file
 *
 * Revision 1.17  2004/11/23 16:57:21  ivanov
 * Fixed compilation warning
 *
 * Revision 1.16  2004/11/17 18:00:17  ivanov
 * Moved #include <zlib.h> from .hpp to .cpp
 *
 * Revision 1.15  2004/11/12 17:37:16  ivanov
 * Fixed bug in CZipDecompressor::Process() if fCheckFileHeader flag is set.
 *
 * Revision 1.14  2004/06/02 16:14:12  ivanov
 * Fixed processed bytes counting in the CZipDecompressor::Process()
 *
 * Revision 1.13  2004/05/17 21:07:25  gorelenk
 * Added include of PCH ncbi_pch.hpp
 *
 * Revision 1.12  2004/05/10 19:25:09  ucko
 * +<stdio.h> for sprintf()
 *
 * Revision 1.11  2004/05/10 13:06:45  ucko
 * Default OS_CODE to the Unix value (0x03).
 *
 * Revision 1.10  2004/05/10 12:57:03  ivanov
 * Removed #include <zutil.h>
 *
 * Revision 1.9  2004/05/10 11:56:08  ivanov
 * Added gzip file format support
 *
 * Revision 1.8  2004/04/19 14:17:15  ivanov
 * Added gzip file format support into stream decompressor
 *
 * Revision 1.7  2004/04/01 14:14:03  lavr
 * Spell "occurred", "occurrence", and "occurring"
 *
 * Revision 1.6  2003/07/15 15:46:31  ivanov
 * Improved error diagnostics. Save status codes and it's description after
 * each compression operation, and ERR_POST it if error occurred.
 *
 * Revision 1.5  2003/07/10 16:30:30  ivanov
 * Implemented CompressFile/DecompressFile functions.
 * Added ability to skip a gzip file headers into DecompressBuffer().
 *
 * Revision 1.4  2003/06/17 15:43:58  ivanov
 * Minor cosmetics and comments changes
 *
 * Revision 1.3  2003/06/03 20:09:16  ivanov
 * The Compression API redesign. Added some new classes, rewritten old.
 *
 * Revision 1.2  2003/04/15 16:49:33  ivanov
 * Added processing zlib return code Z_BUF_ERROR to DeflateFlush()
 *
 * Revision 1.1  2003/04/07 20:21:35  ivanov
 * Initial revision
 *
 * ===========================================================================
 */
