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
 * Author: Eugene Vasilchenko
 *
 * File Description:
 *   Implementation of CByteSource and CByteSourceReader
 *   and their specializations'.
 *
 */

#include <corelib/ncbistd.hpp>
#include <util/bytesrc.hpp>
#include <util/util_exception.hpp>
#include <util/stream_utils.hpp>
#include <algorithm>


BEGIN_NCBI_SCOPE


typedef CFileSourceCollector::TFilePos TFilePos;
typedef CFileSourceCollector::TFileOff TFileOff;


CByteSource::~CByteSource(void)
{
    return;
}


CByteSourceReader::~CByteSourceReader(void)
{
    return;
}


CRef<CSubSourceCollector> 
CByteSourceReader::SubSource(size_t /*prevent*/,CRef<CSubSourceCollector> parent)
{
    return CRef<CSubSourceCollector>(new CMemorySourceCollector(parent));
}


CSubSourceCollector::CSubSourceCollector(CRef<CSubSourceCollector> parent)
 : m_ParentSubSource(parent)
{
}

CSubSourceCollector::~CSubSourceCollector(void)
{
}

void CSubSourceCollector::AddChunk(const char* buffer, size_t bufferLength)
{
    if (!m_ParentSubSource.IsNull()) {
        m_ParentSubSource->AddChunk(buffer,bufferLength);
    }
}

/* Mac compiler doesn't like getting these flags as unsigned int (thiessen)
static inline
unsigned IFStreamFlags(bool binary)
{
    return binary ? (IOS_BASE::in | IOS_BASE::binary) : IOS_BASE::in;
}
*/
#define IFStreamFlags(isBinary) \
  (isBinary? (IOS_BASE::in | IOS_BASE::binary): IOS_BASE::in)


bool CByteSourceReader::EndOfData(void) const
{
    return true;
}


CRef<CByteSourceReader> CStreamByteSource::Open(void)
{
    return CRef<CByteSourceReader>
      (new CStreamByteSourceReader(this, m_Stream));
}


size_t CStreamByteSourceReader::Read(char* buffer, size_t bufferLength)
{
    return CStreamUtils::Readsome(*m_Stream, buffer, bufferLength);
}


bool CStreamByteSourceReader::EndOfData(void) const
{
    return m_Stream->eof();
}


CFStreamByteSource::CFStreamByteSource(const string& fileName, bool binary)
    : CStreamByteSource(*new CNcbiIfstream(fileName.c_str(),
                                           IFStreamFlags(binary)))
{
    if ( !*m_Stream ) {
//        THROW1_TRACE(runtime_error, "file not found: " + fileName);
        NCBI_THROW(CUtilException,eNoInput,"file not found: " + fileName);
    }
}


CFStreamByteSource::~CFStreamByteSource(void)
{
    delete m_Stream;
}


CFileByteSource::CFileByteSource(const string& fileName, bool binary)
    : m_FileName(fileName), m_Binary(binary)
{
    return;
}


CFileByteSource::CFileByteSource(const CFileByteSource& file)
    : m_FileName(file.m_FileName), m_Binary(file.m_Binary)
{
    return;
}


CRef<CByteSourceReader> CFileByteSource::Open(void)
{
    return CRef<CByteSourceReader>(new CFileByteSourceReader(this));
}


CSubFileByteSource::CSubFileByteSource(const CFileByteSource& file,
                                       TFilePos start, TFileOff length)
    : CParent(file), m_Start(start), m_Length(length)
{
    return;
}


CRef<CByteSourceReader> CSubFileByteSource::Open(void)
{
    return CRef<CByteSourceReader>
      (new CSubFileByteSourceReader(this, m_Start, m_Length));
}


CFileByteSourceReader::CFileByteSourceReader(const CFileByteSource* source)
    : CStreamByteSourceReader(source, 0),
      m_FileSource(source),
      m_FStream(source->GetFileName().c_str(),
                IFStreamFlags(source->IsBinary()))
{
    if ( !m_FStream ) {
//        THROW1_TRACE(runtime_error, "file not found: " +source->GetFileName());
        NCBI_THROW(CUtilException,eNoInput,
            "file not found: " +source->GetFileName());
    }
    m_Stream = &m_FStream;
}


CSubFileByteSourceReader::CSubFileByteSourceReader(const CFileByteSource* s,
                                                   TFilePos start,
                                                   TFileOff length)
    : CParent(s), m_Length(length)
{
    m_Stream->seekg(start);
}


size_t CSubFileByteSourceReader::Read(char* buffer, size_t bufferLength)
{
    if ( TFileOff(bufferLength) > m_Length )
        bufferLength = size_t(m_Length);
    size_t count = CParent::Read(buffer, bufferLength);
    m_Length -= TFileOff(count);
    return count;
}


bool CSubFileByteSourceReader::EndOfData(void) const
{
    return m_Length == 0;
}


CRef<CSubSourceCollector> 
CFileByteSourceReader::SubSource(size_t prepend, 
                                 CRef<CSubSourceCollector> parent)
{
    return CRef<CSubSourceCollector>(new CFileSourceCollector(m_FileSource,
                                    m_Stream->tellg() - TFileOff(prepend),
                                    parent));
}


CFileSourceCollector::CFileSourceCollector(CConstRef<CFileByteSource> s,
                                           TFilePos start,
                                           CRef<CSubSourceCollector> parent)
    : CSubSourceCollector(parent),
      m_FileSource(s),
      m_Start(start),
      m_Length(0)
{
    return;
}


void CFileSourceCollector::AddChunk(const char* buffer,
                                    size_t bufferLength)
{
    CSubSourceCollector::AddChunk(buffer, bufferLength);
    m_Length += TFileOff(bufferLength);
}


CRef<CByteSource> CFileSourceCollector::GetSource(void)
{
    return CRef<CByteSource>(new CSubFileByteSource(*m_FileSource,
                                  m_Start, m_Length));
}


CMemoryChunk::CMemoryChunk(const char* data, size_t dataSize,
                           CRef<CMemoryChunk> prevChunk)
    : m_Data(new char[dataSize]),
      m_DataSize(dataSize)
{
    memcpy(m_Data, data, dataSize);
    if ( prevChunk )
        prevChunk->m_NextChunk = this;
    prevChunk = this;
}


CMemoryChunk::~CMemoryChunk(void)
{
    delete m_Data;
}


CMemoryByteSource::~CMemoryByteSource(void)
{
}


CRef<CByteSourceReader> CMemoryByteSource::Open(void)
{
    return CRef<CByteSourceReader>(new CMemoryByteSourceReader(m_Bytes));
}


size_t CMemoryByteSourceReader::GetCurrentChunkAvailable(void) const
{
    return m_CurrentChunk->GetDataSize() - m_CurrentChunkOffset;
}


size_t CMemoryByteSourceReader::Read(char* buffer, size_t bufferLength)
{
    while ( m_CurrentChunk ) {
        size_t avail = GetCurrentChunkAvailable();
        if ( avail == 0 ) {
            // end of current chunk
            CConstRef<CMemoryChunk> rest = m_CurrentChunk->GetNextChunk();
            m_CurrentChunk = rest;
            m_CurrentChunkOffset = 0;
        }
        else {
            size_t c = min(bufferLength, avail);
            memcpy(buffer, m_CurrentChunk->GetData(m_CurrentChunkOffset), c);
            m_CurrentChunkOffset += c;
            return c;
        }
    }
    return 0;
}


bool CMemoryByteSourceReader::EndOfData(void) const
{
    return !m_CurrentChunk;
}

CMemorySourceCollector::CMemorySourceCollector(CRef<CSubSourceCollector> parent)
: CSubSourceCollector(parent)
{
}

void CMemorySourceCollector::AddChunk(const char* buffer,
                                      size_t bufferLength)
{
    CSubSourceCollector::AddChunk(buffer, bufferLength);

    m_LastChunk = new CMemoryChunk(buffer, bufferLength, m_LastChunk);
    if ( !m_FirstChunk )
        m_FirstChunk = m_LastChunk;
}


CRef<CByteSource> CMemorySourceCollector::GetSource(void)
{
    return CRef<CByteSource>(new CMemoryByteSource(m_FirstChunk));
}


CIWriterSourceCollector::CIWriterSourceCollector(IWriter*    writer, 
                                                 EOwnership  own, 
                                       CRef<CSubSourceCollector>  parent)
    : CSubSourceCollector(parent),
      m_IWriter(writer),
      m_Own(own)
{
}


CIWriterSourceCollector::~CIWriterSourceCollector()
{
    if (m_Own)
        delete m_IWriter;
}


void CIWriterSourceCollector::SetWriter(IWriter*   writer, 
                                        EOwnership own)
{
    if (m_Own)
        delete m_IWriter;
    m_IWriter = writer;
    m_Own = own;    
}

void CIWriterSourceCollector::AddChunk(const char* buffer, size_t bufferLength)
{
    size_t written;
    m_IWriter->Write(buffer, bufferLength, &written);
}

CRef<CByteSource> CIWriterSourceCollector::GetSource(void)
{
    return CRef<CByteSource>(); // TODO: Add appropriate bytesource
}


END_NCBI_SCOPE


/*
 * ---------------------------------------------------------------------------
 * $Log$
 * Revision 1.22  2003/09/25 16:38:03  kuznets
 * + IWriter based sub source collector
 *
 * Revision 1.21  2003/09/25 13:59:40  ucko
 * Pass C(Const)Ref by value, not reference!
 *
 * Revision 1.20  2003/09/25 12:47:36  kuznets
 * Added subsource chaining
 *
 * Revision 1.19  2003/02/26 21:32:00  gouriano
 * modify C++ exceptions thrown by this library
 *
 * Revision 1.18  2002/11/27 21:08:52  lavr
 * Take advantage of CStreamUtils::Readsome() in CStreamByteSource::Read()
 *
 * Revision 1.17  2002/11/04 21:29:22  grichenk
 * Fixed usage of const CRef<> and CRef<> constructor
 *
 * Revision 1.16  2002/01/17 17:57:14  vakatov
 * [GCC > 3.0]  CStreamByteSourceReader::Read() -- use "readsome()", thus
 * avoid blocking on timeout at EOF for non-blocking input stream.
 *
 * Revision 1.15  2001/08/15 18:43:34  lavr
 * FIXED CStreamByteSourceReader::Read:
 * ios::clear() has to have no arguments in order to clear EOF state.
 *
 * Revision 1.14  2001/05/30 19:55:35  grichenk
 * Fixed one more non-blocking stream reading bug, added comments
 *
 * Revision 1.13  2001/05/29 19:35:23  grichenk
 * Fixed non-blocking stream reading for GCC
 *
 * Revision 1.12  2001/05/17 15:07:15  lavr
 * Typos corrected
 *
 * Revision 1.11  2001/05/16 17:55:40  grichenk
 * Redesigned support for non-blocking stream read operations
 *
 * Revision 1.10  2001/05/11 20:41:19  grichenk
 * Added support for non-blocking stream reading
 *
 * Revision 1.9  2001/05/11 14:00:21  grichenk
 * CStreamByteSourceReader::Read() -- use readsome() instead of read()
 *
 * Revision 1.8  2001/01/05 20:09:05  vasilche
 * Added util directory for various algorithms and utility classes.
 *
 * Revision 1.7  2000/11/01 20:38:37  vasilche
 * Removed ECanDelete enum and related constructors.
 *
 * Revision 1.6  2000/09/01 13:16:14  vasilche
 * Implemented class/container/choice iterators.
 * Implemented CObjectStreamCopier for copying data without loading into memory
 *
 * Revision 1.5  2000/07/03 18:42:42  vasilche
 * Added interface to typeinfo via CObjectInfo and CConstObjectInfo.
 * Reduced header dependency.
 *
 * Revision 1.4  2000/06/22 16:13:48  thiessen
 * change IFStreamFlags to macro
 *
 * Revision 1.3  2000/05/03 14:38:13  vasilche
 * SERIAL: added support for delayed reading to generated classes.
 * DATATOOL: added code generation for delayed reading.
 *
 * Revision 1.2  2000/04/28 19:14:37  vasilche
 * Fixed stream position and offset typedefs.
 *
 * Revision 1.1  2000/04/28 16:58:11  vasilche
 * Added classes CByteSource and CByteSourceReader for generic reading.
 * Added delayed reading of choice variants.
 *
 * ===========================================================================
 */
