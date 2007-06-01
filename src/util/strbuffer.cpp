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
*   Input buffer
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbistre.hpp>
#include <corelib/ncbi_limits.hpp>
#include <util/strbuffer.hpp>
#include <util/bytesrc.hpp>
#include <algorithm>

BEGIN_NCBI_SCOPE

static const size_t KInitialBufferSize = 4096;

static inline
size_t BiggerBufferSize(size_t size) THROWS1_NONE
{
    return size * 2;
}

CIStreamBuffer::CIStreamBuffer(void)
    THROWS1((bad_alloc))
    : m_Error(0), m_BufferPos(0),
      m_BufferSize(0), m_Buffer(0),
      m_CurrentPos(0), m_DataEndPos(0),
      m_Line(1),
      m_CollectPos(0)
{
}

CIStreamBuffer::CIStreamBuffer(const char* buffer, size_t size)
    : m_Error(0), m_BufferPos(0),
      m_BufferSize(0), m_Buffer(const_cast<char*>(buffer)),
      m_CurrentPos(buffer), m_DataEndPos(buffer+size),
      m_Line(1),
      m_CollectPos(0)
{
}

CIStreamBuffer::~CIStreamBuffer(void)
{
    try {
        Close();
    }
    catch ( exception& exc ) {
        ERR_POST(Warning <<
                 "~CIStreamBuffer: exception while closing: " << exc.what());
    }
    if ( m_BufferSize ) {
        delete[] m_Buffer;
    }
}

void CIStreamBuffer::Open(CByteSourceReader& reader)
{
    Close();
    if ( !m_BufferSize ) {
        m_BufferSize = KInitialBufferSize;
        m_CurrentPos = m_DataEndPos = m_Buffer = new char[m_BufferSize];
    }
    m_Input = &reader;
    m_Error = 0;
}

void CIStreamBuffer::Open(const char* buffer, size_t size)
{
    Close();
    if ( m_BufferSize ) {
        delete[] m_Buffer;
    }
    m_BufferSize = 0;
    m_Buffer = const_cast<char*>(buffer);
    m_CurrentPos = buffer;
    m_DataEndPos = buffer + size;
    m_Error = 0;
}

void CIStreamBuffer::Close(void)
{
    if ( m_Input ) {
        size_t unused = m_DataEndPos - m_CurrentPos;
        if ( unused ) {
            m_Input->Pushback(m_CurrentPos, unused);
        }
        m_Input.Reset();
    }
    m_BufferPos = 0;
    m_CurrentPos = m_Buffer;
    m_DataEndPos = m_Buffer;
    m_Line = 1;
    m_Error = 0;
}

void CIStreamBuffer::StartSubSource(void)
{
    if ( m_Collector ) {
        // update current collector data
        _ASSERT(m_CollectPos);
        size_t count = m_CurrentPos - m_CollectPos;
        if ( count )
            m_Collector->AddChunk(m_CollectPos, count);
    }
    m_CollectPos = m_CurrentPos;
    if ( m_Input ) {
        m_Collector =
            m_Input->SubSource(m_DataEndPos - m_CurrentPos, m_Collector);
    }
    else {
        m_Collector =
            new CMemorySourceCollector(m_Collector);
    }
}

CRef<CByteSource> CIStreamBuffer::EndSubSource(void)
{
    _ASSERT(m_Collector);
    _ASSERT(m_CollectPos);

    _ASSERT(m_CollectPos <= m_CurrentPos);
    size_t count = m_CurrentPos - m_CollectPos;
    if ( count )
        m_Collector->AddChunk(m_CollectPos, count);

    CRef<CByteSource> source = m_Collector->GetSource();

    CRef<CSubSourceCollector> parent = m_Collector->GetParentCollector();
    if ( parent ) {
        m_Collector = parent;
        m_CollectPos = m_CurrentPos;
    }
    else {
        m_Collector = null;
        m_CollectPos = 0;
    }

    return source;
}

// this method is highly optimized
char CIStreamBuffer::SkipSpaces(void)
    THROWS1((CIOException))
{
    // cache pointers
    const char* pos = m_CurrentPos;
    const char* end = m_DataEndPos;
    // make sure thire is at least one char in buffer
    if ( pos == end ) {
        // fill buffer
        pos = FillBuffer(pos);
        // cache m_DataEndPos
        end = m_DataEndPos;
    }
    // main cycle
    // at the beginning:
    //     pos == m_CurrentPos
    //     end == m_DataEndPos
    //     pos < end
    for (;;) {
        // we use do{}while() cycle because
        // condition is true at the beginning ( pos < end )
        do {
            // cache current char
            char c = *pos;
            if ( c != ' ' ) { // it's not space (' ')
                // point m_CurrentPos to first non space char
                m_CurrentPos = pos;
                // return char value
                return c;
            }
            // skip space char
        } while ( ++pos < end );
        // here pos == end == m_DataEndPos
        // point m_CurrentPos to end of buffer
        m_CurrentPos = pos;
        // fill next portion
        pos = FillBuffer(pos);
        // cache m_DataEndPos
        end = m_DataEndPos;
    }
}

// this method is highly optimized
void CIStreamBuffer::FindChar(char c)
    THROWS1((CIOException))
{
    // cache pointers
    const char* pos = m_CurrentPos;
    const char* end = m_DataEndPos;
    // make sure thire is at least one char in buffer
    if ( pos == end ) {
        // fill buffer
        pos = FillBuffer(pos);
        // cache m_DataEndPos
        end = m_DataEndPos;
    }
    // main cycle
    // at the beginning:
    //     pos == m_CurrentPos
    //     end == m_DataEndPos
    //     pos < end
    for (;;) {
        const void* found = memchr(pos, c, end - pos);
        if ( found ) {
            m_CurrentPos = static_cast<const char*>(found);
            return;
        }
        // point m_CurrentPos to end of buffer
        m_CurrentPos = end;
        // fill next portion
        pos = FillBuffer(end);
        // cache m_DataEndPos
        end = m_DataEndPos;
    }
}

// this method is highly optimized
size_t CIStreamBuffer::PeekFindChar(char c, size_t limit)
    THROWS1((CIOException))
{
    _ASSERT(limit > 0);
    PeekCharNoEOF(limit - 1);
    // cache pointers
    const char* pos = m_CurrentPos;
    size_t bufferSize = m_DataEndPos - pos;
    if ( bufferSize != 0 ) {
        const void* found = memchr(pos, c, min(limit, bufferSize));
        if ( found )
            return static_cast<const char*>(found) - pos;
    }
    return limit;
}

const char* CIStreamBuffer::FillBuffer(const char* pos, bool noEOF)
    THROWS1((CIOException, bad_alloc))
{
    _ASSERT(pos >= m_DataEndPos);
    // remove unused portion of buffer at the beginning
    _ASSERT(m_CurrentPos >= m_Buffer);
    if ( m_BufferSize == 0 ) {
        // buffer is external -> no more data
        if ( noEOF ) {
            return pos;
        }
        else {
            m_Error = "end of file";
            NCBI_THROW(CEofException,eEof,m_Error);
        }
    }
    size_t newPosOffset = pos - m_Buffer;
    if ( newPosOffset >= m_BufferSize || m_DataEndPos == m_CurrentPos ) {
        // if new position is out of buffer, or if there is no data left
        // move pointers to the beginning
        size_t erase = m_CurrentPos - m_Buffer;
        if ( erase > 0 ) {
            const char* newPos = m_CurrentPos - erase;
            if ( m_Collector ) {
                _ASSERT(m_CollectPos);
                size_t count = m_CurrentPos - m_CollectPos;
                if ( count > 0 )
                    m_Collector->AddChunk(m_CollectPos, count);
                m_CollectPos = newPos;
            }
            size_t copy_count = m_DataEndPos - m_CurrentPos;
            if ( copy_count )
                memmove(const_cast<char*>(newPos), m_CurrentPos, copy_count);
            m_CurrentPos = newPos;
            m_DataEndPos -= erase;
            m_BufferPos += CT_OFF_TYPE(erase);
            pos -= erase;
            newPosOffset -= erase;
        }
    }
    size_t dataSize = m_DataEndPos - m_Buffer;
    if ( newPosOffset >= m_BufferSize ) {
        // reallocate buffer
        size_t newSize = BiggerBufferSize(m_BufferSize);
        while ( newPosOffset >= newSize ) {
            newSize = BiggerBufferSize(newSize);
        }
        char* newBuffer = new char[newSize];
        memcpy(newBuffer, m_Buffer, dataSize);
        m_CurrentPos = newBuffer + (m_CurrentPos - m_Buffer);
        if ( m_CollectPos )
            m_CollectPos = newBuffer + (m_CollectPos - m_Buffer);
        pos = newBuffer + newPosOffset;
        m_DataEndPos = newBuffer + dataSize;
        delete[] m_Buffer;
        m_Buffer = newBuffer;
        m_BufferSize = newSize;
    }
    size_t load = m_BufferSize - dataSize;
    while ( load > 0  &&  pos >= m_DataEndPos ) {
        if ( !m_Input ) {
            if ( noEOF ) {
                return pos;
            }
            m_Error = "end of file";
            NCBI_THROW(CEofException,eEof,m_Error);
        }
        size_t count = m_Input->Read(const_cast<char*>(m_DataEndPos), load);
        if ( count == 0 ) {
            if ( pos < m_DataEndPos )
                return pos;
            if ( m_Input->EndOfData() ) {
                if ( noEOF ) {
                    // ignore EOF
                    _ASSERT(m_Buffer <= m_CurrentPos);
                    _ASSERT(m_CurrentPos <= pos);
                    _ASSERT(m_DataEndPos <= m_Buffer + m_BufferSize);
                    _ASSERT(!m_CollectPos ||
                            (m_CollectPos>=m_Buffer &&
                             m_CollectPos<=m_CurrentPos));
                    return pos;
                }
                m_Error = "end of file";
//                THROW0_TRACE(CEofException());
                NCBI_THROW(CEofException,eEof,m_Error);
            }
            else {
                m_Error = "read fault";
//                THROW1_TRACE(CIOException, "read fault");
                NCBI_THROW(CIOException,eRead,m_Error);
            }
        }
        m_DataEndPos += count;
        load -= count;
    }
    _ASSERT(m_Buffer <= m_CurrentPos);
    _ASSERT(m_CurrentPos <= pos);
    _ASSERT(pos < m_DataEndPos);
    _ASSERT(m_DataEndPos <= m_Buffer + m_BufferSize);
    _ASSERT(!m_CollectPos ||
            (m_CollectPos>=m_Buffer && m_CollectPos<=m_CurrentPos));
    return pos;
}

char CIStreamBuffer::FillBufferNoEOF(const char* pos)
{
    pos = FillBuffer(pos, true);
    if ( pos >= m_DataEndPos )
        return 0;
    else
        return *pos;
}

bool CIStreamBuffer::TryToFillBuffer(void)
{
    return FillBuffer(m_CurrentPos, true) < m_DataEndPos;
}

void CIStreamBuffer::GetChars(char* buffer, size_t count)
    THROWS1((CIOException))
{
    // cache pos
    const char* pos = m_CurrentPos;
    for ( ;; ) {
        size_t c = m_DataEndPos - pos;
        if ( c >= count ) {
            // all data is already in buffer -> copy it
            memcpy(buffer, pos, count);
            m_CurrentPos = pos + count;
            return;
        }
        else {
            memcpy(buffer, pos, c);
            buffer += c;
            count -= c;
            m_CurrentPos = pos += c;
            pos = FillBuffer(pos);
        }
    }
}

void CIStreamBuffer::GetChars(string& str, size_t count)
    THROWS1((CIOException))
{
    // cache pos
    const char* pos = m_CurrentPos;
    size_t in_buffer = m_DataEndPos - pos;
    if ( in_buffer >= count ) {
        // simplest case - plain copy
        str.assign(pos, count);
        m_CurrentPos = pos + count;
        return;
    }
    str.reserve(count);
    str.assign(pos, in_buffer);
    for ( ;; ) {
        count -= in_buffer;
        m_CurrentPos = pos += in_buffer;
        pos = FillBuffer(pos);
        in_buffer = m_DataEndPos - pos;
        if ( in_buffer >= count ) {
            // all data is already in buffer -> copy it
            str.append(pos, count);
            m_CurrentPos = pos + count;
            return;
        }
        str.append(pos, in_buffer);
    }
}

void CIStreamBuffer::GetChars(size_t count)
    THROWS1((CIOException))
{
    // cache pos
    const char* pos = m_CurrentPos;
    for ( ;; ) {
        size_t c = m_DataEndPos - pos;
        if ( c >= count ) {
            // all data is already in buffer -> skip it
            m_CurrentPos = pos + count;
            return;
        }
        else {
            count -= c;
            m_CurrentPos = pos += c;
            pos = FillBuffer(pos);
        }
    }
}

void CIStreamBuffer::SkipEndOfLine(char lastChar)
    THROWS1((CIOException))
{
    _ASSERT(lastChar == '\n' || lastChar == '\r');
    _ASSERT(m_CurrentPos > m_Buffer && m_CurrentPos[-1] == lastChar);
    m_Line++;
    char nextChar = PeekCharNoEOF();
    // lastChar either '\r' or \n'
    // if nextChar is compliment, skip it
    if ( (lastChar + nextChar) == ('\r' + '\n') )
        SkipChar();
}

size_t CIStreamBuffer::ReadLine(char* buff, size_t size)
    THROWS1((CIOException))
{
    size_t count = 0;
    try {
        while ( size > 0 ) {
            char c = *buff++ = GetChar();
            count++;
            size--;
            switch ( c ) {
            case '\r':
                // replace leading '\r' by '\n'
                buff[-1] = '\n';
                if ( PeekChar() == '\n' )
                    SkipChar();
                return count;
            case '\n':
                if ( PeekChar() == '\r' )
                    SkipChar();
                return count;
            }
        }
        return count;
    }
    catch ( CEofException& /*ignored*/ ) {
        return count;
    }
}

void CIStreamBuffer::BadNumber(void)
{
    m_Error = "bad number";
//    THROW1_TRACE(runtime_error, "bad number in line " + NStr::UIntToString(GetLine()));
    NCBI_THROW(CIOException,eRead,
        "bad number in line " + NStr::UIntToString(GetLine()));
}

void CIStreamBuffer::SetStreamOffset(CNcbiStreampos pos)
{
    SetStreamPos(pos);
}

void CIStreamBuffer::SetStreamPos(CNcbiStreampos pos)
{
    if ( m_Input ) {
        m_Input->Seekg(pos);
        m_BufferPos = pos;
        m_DataEndPos = m_Buffer;
        m_CurrentPos = m_Buffer;
        m_Line = 1;
    }
    else {
        if ( pos < 0 || pos > m_DataEndPos - m_Buffer ) {
            NCBI_THROW(CIOException,eRead,"stream position is out of buffer");
        }
        m_BufferPos = pos;
        m_CurrentPos = m_Buffer + pos;
        m_Line = 1;
    }
}

Int4 CIStreamBuffer::GetInt4(void)
{
    bool sign;
    char c = GetChar();
    switch ( c ) {
    case '-':
        sign = true;
        c = GetChar();
        break;
    case '+':
        sign = false;
        c = GetChar();
        break;
    default:
        sign = false;
        break;
    }
    if ( c < '0' || c > '9' )
        BadNumber();

    Int4 n = c - '0';
    for ( ;; ) {
        c = PeekCharNoEOF();
        if  ( c < '0' || c > '9' )
            break;

        SkipChar();
        // TODO: check overflow
        n = n * 10 + (c - '0');
    }
    if ( sign )
        return -n;
    else
        return n;
}

Uint4 CIStreamBuffer::GetUint4(void)
{
    char c = GetChar();
    if ( c == '+' )
        c = GetChar();
    if ( c < '0' || c > '9' )
        BadNumber();

    Uint4 n = c - '0';
    for ( ;; ) {
        c = PeekCharNoEOF();
        if  ( c < '0' || c > '9' )
            break;

        SkipChar();
        // TODO: check overflow
        n = n * 10 + (c - '0');
    }
    return n;
}

Int8 CIStreamBuffer::GetInt8(void)
    THROWS1((CIOException))
{
    bool sign;
    char c = GetChar();
    switch ( c ) {
    case '-':
        sign = true;
        c = GetChar();
        break;
    case '+':
        sign = false;
        c = GetChar();
        break;
    default:
        sign = false;
        break;
    }
    if ( c < '0' || c > '9' )
        BadNumber();

    Int8 n = c - '0';
    for ( ;; ) {
        c = PeekCharNoEOF();
        if  ( c < '0' || c > '9' )
            break;

        SkipChar();
        // TODO: check overflow
        n = n * 10 + (c - '0');
    }
    if ( sign )
        return -n;
    else
        return n;
}

Uint8 CIStreamBuffer::GetUint8(void)
    THROWS1((CIOException))
{
    char c = GetChar();
    if ( c == '+' )
        c = GetChar();
    if ( c < '0' || c > '9' )
        BadNumber();

    Uint8 n = c - '0';
    for ( ;; ) {
        c = PeekCharNoEOF();
        if  ( c < '0' || c > '9' )
            break;

        SkipChar();
        // TODO: check overflow
        n = n * 10 + (c - '0');
    }
    return n;
}

COStreamBuffer::COStreamBuffer(CNcbiOstream& out, bool deleteOut)
    THROWS1((bad_alloc))
    : m_Output(out), m_DeleteOutput(deleteOut), m_Error(0),
      m_IndentLevel(0), m_BufferPos(0),
      m_Buffer(new char[KInitialBufferSize]),
      m_CurrentPos(m_Buffer),
      m_BufferEnd(m_Buffer + KInitialBufferSize),
      m_Line(1), m_LineLength(0),
      m_BackLimit(0), m_UseIndentation(true)
{
}

COStreamBuffer::~COStreamBuffer(void)
{
    try {
        Close();
    }
    catch ( exception& exc ) {
        ERR_POST(Warning <<
                 "~COStreamBuffer: exception while closing: " << exc.what());
    }
    delete[] m_Buffer;
}

void COStreamBuffer::Close(void)
{
    if ( m_Output ) {
        FlushBuffer();
        if ( m_DeleteOutput ) {
            Flush();
            delete &m_Output;
            m_DeleteOutput = false;
        }
    }
    m_Error = 0;
    m_IndentLevel = 0;
    m_CurrentPos = m_Buffer;
    m_Line = 1;
    m_LineLength = 0;
}

void COStreamBuffer::FlushBuffer(bool fullBuffer)
    THROWS1((CIOException))
{
    size_t used = GetUsedSpace();
    size_t count;
    size_t leave;
    if ( fullBuffer ) {
        count = used;
        leave = 0;
    }
    else {
        leave = m_BackLimit;
        if ( used < leave )
            return; // none to flush
        count = used - leave;
    }
    if ( count != 0 ) {
        if ( !m_Output.write(m_Buffer, count) ) {
            m_Error = "write fault";
//            THROW1_TRACE(CIOException, "write fault");
            NCBI_THROW(CIOException,eWrite,m_Error);
        }
        if ( leave != 0 ) {
            memmove(m_Buffer, m_Buffer + count, leave);
            m_CurrentPos -= count;
        }
        else {
            m_CurrentPos = m_Buffer;
        }
        m_BufferPos += CT_OFF_TYPE(count);
    }
}


void COStreamBuffer::Flush(void)
    THROWS1((CIOException))
{
    FlushBuffer();
    IOS_BASE::iostate state = m_Output.rdstate();
    m_Output.clear();
    try {
        if ( !m_Output.flush() ) {
            NCBI_THROW(CIOException,eFlush,"COStreamBuffer::Flush: failed");
        }
    }
    catch (...) {
        m_Output.clear(state);
        throw;
    }
    m_Output.clear(state);
}


char* COStreamBuffer::DoReserve(size_t count)
    THROWS1((CIOException, bad_alloc))
{
    FlushBuffer(false);
    size_t usedSize = m_CurrentPos - m_Buffer;
    size_t needSize = usedSize + count;
    size_t bufferSize = m_BufferEnd - m_Buffer;
    if ( bufferSize < needSize ) {
        // realloc too small buffer
        do {
            bufferSize = BiggerBufferSize(bufferSize);
        } while ( bufferSize < needSize );
        if ( usedSize == 0 ) {
            delete[] m_Buffer;
            m_CurrentPos = m_Buffer = new char[bufferSize];
            m_BufferEnd = m_Buffer + bufferSize;
        }
        else {
            char* oldBuffer = m_Buffer;
            m_Buffer = new char[bufferSize];
            m_BufferEnd = m_Buffer + bufferSize;
            memcpy(m_Buffer, oldBuffer, usedSize);
            delete[] oldBuffer;
            m_CurrentPos = m_Buffer + usedSize;
        }
    }
    return m_CurrentPos;
}

void COStreamBuffer::PutInt4(Int4 v)
    THROWS1((CIOException, bad_alloc))
{
    const size_t BSIZE = (sizeof(v)*CHAR_BIT) / 3 + 2;
    char b[BSIZE];
    char* pos = b + BSIZE;
    if ( v == 0 ) {
        *--pos = '0';
    }
    else {
        Uint4 uv = v;
        bool sign = v < 0;
        if ( sign )
            uv = -v;
        
        do {
            *--pos = char('0' + (uv % 10));
            uv /= 10;
        } while ( uv );
        
        if ( sign )
            *--pos = '-';
    }
    PutString(pos, b + BSIZE - pos);
}

void COStreamBuffer::PutUint4(Uint4 v)
    THROWS1((CIOException, bad_alloc))
{
    const size_t BSIZE = (sizeof(v)*CHAR_BIT) / 3 + 2;
    char b[BSIZE];
    char* pos = b + BSIZE;
    if ( v == 0 ) {
        *--pos = '0';
    }
    else {
        do {
            *--pos = char('0' + (v % 10));
            v /= 10;
        } while ( v );
    }
    PutString(pos, b + BSIZE - pos);
}

void COStreamBuffer::PutInt8(Int8 v)
    THROWS1((CIOException, bad_alloc))
{
    const size_t BSIZE = (sizeof(v)*CHAR_BIT) / 3 + 2;
    char b[BSIZE];
    char* pos = b + BSIZE;
    if ( v == 0 ) {
        *--pos = '0';
    }
    else {
        bool sign = v < 0;
        if ( sign )
            v = -v;
        
        do {
            *--pos = char('0' + (v % 10));
            v /= 10;
        } while ( v );
        
        if ( sign )
            *--pos = '-';
    }
    PutString(pos, b + BSIZE - pos);
}

void COStreamBuffer::PutUint8(Uint8 v)
    THROWS1((CIOException, bad_alloc))
{
    const size_t BSIZE = (sizeof(v)*CHAR_BIT) / 3 + 2;
    char b[BSIZE];
    char* pos = b + BSIZE;
    if ( v == 0 ) {
        *--pos = '0';
    }
    else {
        do {
            *--pos = char('0' + (v % 10));
            v /= 10;
        } while ( v );
    }
    PutString(pos, b + BSIZE - pos);
}

void COStreamBuffer::PutEolAtWordEnd(size_t lineLength)
    THROWS1((CIOException, bad_alloc))
{
    Reserve(1);
    size_t linePos = m_LineLength;
    char* pos = m_CurrentPos;
    bool goodPlace = false;
    while ( pos > m_Buffer && linePos > 0 ) {
        --pos;
        --linePos;
        if ( linePos <= lineLength && (isspace((unsigned char) (*pos)) ||
                                       *pos == '\'') ) {
            goodPlace = true;
            break;
        }
        else if ( *pos == '\n' || *pos == '"' ) {
            // no suitable space found
            break;
        }
    }

    // Prevent insertion of more than one '\n'
    if (pos > m_Buffer  &&  *(pos-1) == '\n') {
        goodPlace = false;
    }

    if ( !goodPlace ) {
        // no suitable space found
        if ( linePos < lineLength ) {
            pos += lineLength - linePos;
            linePos = lineLength;
        }
        // assure we will not break double ""
        while ( pos > m_Buffer && *(pos - 1) == '"' ) {
            --pos;
            --linePos;
        }
        if ( pos == m_Buffer ) {
            // it's possible that we already put some " before...
            while ( pos < m_CurrentPos && *pos == '"' ) {
                ++pos;
                ++linePos;
            }
        }
    }
    // split there
    // insert '\n'
    size_t count = m_CurrentPos - pos;
    memmove(pos + 1, pos, count);
    m_LineLength = count;
    ++m_CurrentPos;
    *pos = '\n';
    ++m_Line;
}

void COStreamBuffer::Write(const char* data, size_t dataLength)
    THROWS1((CIOException, bad_alloc))
{
    while ( dataLength > 0 ) {
        size_t available = GetAvailableSpace();
        if ( available == 0 ) {
            FlushBuffer(false);
            available = GetAvailableSpace();
        }
        if ( available >= dataLength )
            break; // current chunk will fit in buffer
        memcpy(m_CurrentPos, data, available);
        m_CurrentPos += available;
        data += available;
        dataLength -= available;
    }
    memcpy(m_CurrentPos, data, dataLength);
    m_CurrentPos += dataLength;
}

void COStreamBuffer::Write(CByteSourceReader& reader)
    THROWS1((CIOException, bad_alloc))
{
    for ( ;; ) {
        size_t available = GetAvailableSpace();
        if ( available == 0 ) {
            FlushBuffer(false);
            available = GetAvailableSpace();
        }
        size_t count = reader.Read(m_CurrentPos, available);
        if ( count == 0 ) {
            if ( reader.EndOfData() )
                return;
            else
//                THROW1_TRACE(CIOException, "buffer read fault");
                NCBI_THROW(CIOException,eRead,"buffer read fault");
        }
        m_CurrentPos += count;
    }
}

END_NCBI_SCOPE
