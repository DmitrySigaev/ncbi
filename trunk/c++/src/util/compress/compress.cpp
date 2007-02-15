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
 *
 * File Description:  The Compression API
 *
 */

#include <ncbi_pch.hpp>
#include <util/compress/compress.hpp>


BEGIN_NCBI_SCOPE


//////////////////////////////////////////////////////////////////////////////
//
// CCompression
//

CCompression::CCompression(ELevel level)
    : m_DecompressMode(eMode_Unknown),
      m_Level(level), m_ErrorCode(0), m_ErrorMsg(kEmptyStr), m_Flags(0)
{
    return;
}


CCompression::~CCompression(void)
{
    return;
}


CVersionInfo CCompression::GetVersion(void) const
{
    return CVersionInfo(kEmptyStr);
}

CCompression::ELevel CCompression::GetLevel(void) const
{
    if ( m_Level == eLevel_Default) {
        return GetDefaultLevel();
    }
    return m_Level;
}


bool CCompression::x_CompressFile(const string&     src_file,
                                  CCompressionFile& dst_file,
                                  size_t            buf_size)
{
    if ( !buf_size ) {
        SetError(-1, "Buffer size cannot be zero");
        return false;
    }
    CNcbiIfstream is(src_file.c_str(), IOS_BASE::in | IOS_BASE::binary);
    if ( !is.good() ) {
        SetError(-1, "Cannot open source file");
        return false;
    }
    AutoPtr<char, ArrayDeleter<char> > buf(new char[buf_size]);
    while ( is ) {
        is.read(buf.get(), buf_size);
        size_t nread = is.gcount();
        size_t nwritten = dst_file.Write(buf.get(), nread); 
        if ( nwritten != nread ) {
            if ( !GetErrorCode() ) {
                SetError(-1, "Error writing to output file");
            }
            return false;
        }
    }
    return true;
}


bool CCompression::x_DecompressFile(CCompressionFile& src_file,
                                    const string&     dst_file,
                                    size_t            buf_size)
{
    if ( !buf_size ) {
        SetError(-1, "Buffer size cannot be zero");
        return false;
    }
    CNcbiOfstream os(dst_file.c_str(), IOS_BASE::out | IOS_BASE::binary);
    if ( !os.good() ) {
        SetError(-1, "Cannot open source file");
        return false;
    }
    AutoPtr<char, ArrayDeleter<char> > buf(new char[buf_size]);
    size_t nread;
    while ( (nread = src_file.Read(buf.get(), buf_size)) > 0 ) {
        os.write(buf.get(), nread);
        if ( !os.good() ) {
            if ( !GetErrorCode() ) {
                SetError(-1, "Error writing to ouput file");
            }
            return false;
        }
    }
    return true;
}



//////////////////////////////////////////////////////////////////////////////
//
// CCompressionProcessor
//

CCompressionProcessor::CCompressionProcessor(void)
{
    Reset();
    return;
}


CCompressionProcessor::~CCompressionProcessor(void)
{
    return;
}


//////////////////////////////////////////////////////////////////////////////
//
// CCompressionFile
//

CCompressionFile::CCompressionFile(void)
    : m_File(0), m_Mode(eMode_Read)
{
    return;
}


CCompressionFile::~CCompressionFile(void)
{
    return;
}



//////////////////////////////////////////////////////////////////////////////
//
// CCompressionUtil
//

void CCompressionUtil::StoreUI4(void* buffer, unsigned long value)
{
    if ( !buffer ) {
        NCBI_THROW(CCoreException, eInvalidArg, "Incorrect buffer pointer");
    }
    if ( value > kMax_UI4 ) {
        NCBI_THROW(CCoreException, eInvalidArg,
                   "Stored value exceeded maximum size for Uint4 type");
    }
    unsigned char* buf = (unsigned char*)buffer;
    for (int i = 0; i < 4; i++) {
        buf[i] = (unsigned char)(value & 0xff);
        value >>= 8;
    }
}

/// Read 4 bytes from buffer as unsigned long value
void CCompressionUtil::GetUI4(void* buffer, unsigned long value)
{
    if ( !buffer ) {
        NCBI_THROW(CCoreException, eInvalidArg, "Incorrect buffer pointer");
    }
    unsigned char* buf = (unsigned char*)buffer;
    value = 0;
    for (int i = 3; i >= 0; i--) {
        value <<= 8;
        value += buf[i];
    }
}


END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.14  2007/01/10 14:46:01  ivanov
 * + GetVersion()
 *
 * Revision 1.13  2006/12/26 16:16:51  ivanov
 * Get rid of compilation warning
 *
 * Revision 1.12  2006/12/26 15:57:37  ivanov
 * Add a possibility to detect a fact that data in the buffer/file/stream
 * is uncompressed, and allow to use transparent reading (instead of
 * decompression) from it. Added flag CCompression::fAllowTransparentRead.
 *
 * Revision 1.11  2005/07/07 15:39:30  ivanov
 * Improved diagnostic. Call SetError() for the file operations also.
 *
 * Revision 1.10  2005/05/31 17:51:15  ivanov
 * Use AutoPtr's for internal buffers
 *
 * Revision 1.9  2005/02/01 21:47:15  grichenk
 * Fixed warnings
 *
 * Revision 1.8  2004/08/04 18:42:45  ivanov
 * Use binary mode to open file streams in the x_[De]compressFile.
 *
 * Revision 1.7  2004/05/17 21:07:25  gorelenk
 * Added include of PCH ncbi_pch.hpp
 *
 * Revision 1.6  2004/05/10 11:56:08  ivanov
 * Added gzip file format support
 *
 * Revision 1.5  2003/07/15 15:50:50  ivanov
 * Improved error diagnostics
 *
 * Revision 1.4  2003/07/10 19:56:29  ivanov
 * Using classes CNcbi[I/O]fstream instead FILE* streams
 *
 * Revision 1.3  2003/07/10 16:24:26  ivanov
 * Added auxiliary file compression/decompression functions.
 *
 * Revision 1.2  2003/06/03 20:09:16  ivanov
 * The Compression API redesign. Added some new classes, rewritten old.
 *
 * Revision 1.1  2003/04/07 20:21:35  ivanov
 * Initial revision
 *
 * ===========================================================================
 */
