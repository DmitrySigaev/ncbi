#if defined(STRBUFFER__HPP)  &&  !defined(STRBUFFER__INL)
#define STRBUFFER__INL

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
*   !!! PUT YOUR DESCRIPTION HERE !!!
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.1  2000/12/26 22:23:45  vasilche
* Fixed errors of compilation on Mac.
*
* ===========================================================================
*/

inline
bool CIStreamBuffer::fail(void) const
{
    return m_Error != 0;
}

inline
void CIStreamBuffer::ResetFail(void)
{
    m_Error = 0;
}

inline
const char* CIStreamBuffer::GetError(void) const
{
    return m_Error;
}

inline
char CIStreamBuffer::PeekChar(size_t offset)
    THROWS1((CSerialIOException, bad_alloc))
{
    char* pos = m_CurrentPos + offset;
    if ( pos >= m_DataEndPos )
        pos = FillBuffer(pos);
    return *pos;
}

inline
char CIStreamBuffer::PeekCharNoEOF(size_t offset)
    THROWS1((CSerialIOException, bad_alloc))
{
    char* pos = m_CurrentPos + offset;
    if ( pos >= m_DataEndPos )
        return FillBufferNoEOF(pos);
    return *pos;
}

inline
char CIStreamBuffer::GetChar(void)
    THROWS1((CSerialIOException, bad_alloc))
{
    char* pos = m_CurrentPos;
    if ( pos >= m_DataEndPos )
        pos = FillBuffer(pos);
    m_CurrentPos = pos + 1;
    return *pos;
}

inline
void CIStreamBuffer::UngetChar(char _DEBUG_ARG(c))
{
    char* pos = m_CurrentPos;
    _ASSERT(pos > m_Buffer);
    _ASSERT(pos[-1] == c);
    m_CurrentPos = pos - 1;
}

inline
void CIStreamBuffer::SkipChars(size_t count)
{
    _ASSERT(m_CurrentPos + count > m_CurrentPos);
    _ASSERT(m_CurrentPos + count <= m_DataEndPos);
    m_CurrentPos += count;
}

inline
void CIStreamBuffer::SkipChar(void)
{
    SkipChars(1);
}

inline
const char* CIStreamBuffer::GetCurrentPos(void) const
    THROWS1_NONE
{
    return m_CurrentPos;
}

inline
size_t CIStreamBuffer::GetLine(void) const
    THROWS1_NONE
{
    return m_Line;
}

inline
size_t CIStreamBuffer::GetStreamOffset(void) const
    THROWS1_NONE
{
    return m_BufferOffset + (m_CurrentPos - m_Buffer);
}

inline
bool COStreamBuffer::fail(void) const
{
    return m_Error != 0;
}

inline
void COStreamBuffer::ResetFail(void)
{
    m_Error = 0;
}

inline
const char* COStreamBuffer::GetError(void) const
{
    return m_Error;
}

inline
size_t COStreamBuffer::GetLine(void) const
    THROWS1_NONE
{
    return m_Line;
}

inline
size_t COStreamBuffer::GetStreamOffset(void) const
    THROWS1_NONE
{
    return m_BufferOffset + (m_CurrentPos - m_Buffer);
}

inline
size_t COStreamBuffer::GetCurrentLineLength(void) const
    THROWS1_NONE
{
    return m_LineLength;
}

inline
bool COStreamBuffer::ZeroIndentLevel(void) const
    THROWS1_NONE
{
    return m_IndentLevel == 0;
}

inline
size_t COStreamBuffer::GetIndentLevel(size_t step) const
    THROWS1_NONE
{
    return m_IndentLevel / step;
}

inline
void COStreamBuffer::IncIndentLevel(size_t step)
    THROWS1_NONE
{
    m_IndentLevel += step;
}

inline
void COStreamBuffer::DecIndentLevel(size_t step)
    THROWS1_NONE
{
    _ASSERT(m_IndentLevel >= step);
    m_IndentLevel -= step;
}

inline
void COStreamBuffer::SetBackLimit(size_t limit)
{
    m_BackLimit = limit;
}

inline
char* COStreamBuffer::DoSkip(size_t reserve)
    THROWS1((CSerialIOException, bad_alloc))
{
    char* pos = DoReserve(reserve);
    m_CurrentPos = pos + reserve;
    m_LineLength += reserve;
    return pos;
}

inline
char* COStreamBuffer::Skip(size_t count)
    THROWS1((CSerialIOException, bad_alloc))
{
    char* pos = m_CurrentPos;
    char* end = pos + count;
    if ( end <= m_BufferEnd ) {
        // enough space in buffer
        m_CurrentPos = end;
        m_LineLength += count;
        return pos;
    }
    else {
        return DoSkip(count);
    }
}

inline
char* COStreamBuffer::Reserve(size_t count)
    THROWS1((CSerialIOException, bad_alloc))
{
    char* pos = m_CurrentPos;
    char* end = pos + count;
    if ( end <= m_BufferEnd ) {
        // enough space in buffer
        return pos;
    }
    else {
        return DoReserve(count);
    }
}

inline
void COStreamBuffer::PutChar(char c)
    THROWS1((CSerialIOException))
{
    *Skip(1) = c;
}

inline
void COStreamBuffer::BackChar(char _DEBUG_ARG(c))
{
    _ASSERT(m_CurrentPos > m_Buffer);
    --m_CurrentPos;
    _ASSERT(*m_CurrentPos == c);
}

inline
void COStreamBuffer::PutString(const char* str, size_t length)
    THROWS1((CSerialIOException, bad_alloc))
{
    if ( length < 1024 ) {
        memcpy(Skip(length), str, length);
    }
    else {
        Write(str, length);
    }
}

inline
void COStreamBuffer::PutString(const char* str)
    THROWS1((CSerialIOException, bad_alloc))
{
    PutString(str, strlen(str));
}

inline
void COStreamBuffer::PutString(const string& str)
    THROWS1((CSerialIOException, bad_alloc))
{
    PutString(str.data(), str.size());
}

inline
void COStreamBuffer::PutIndent(void)
    THROWS1((CSerialIOException, bad_alloc))
{
    size_t count = m_IndentLevel;
    memset(Skip(count), ' ', count);
}

inline
void COStreamBuffer::PutEol(bool indent)
    THROWS1((CSerialIOException, bad_alloc))
{
    char* pos = Reserve(1);
    *pos = '\n';
    m_CurrentPos = pos + 1;
    ++m_Line;
    m_LineLength = 0;
    if ( indent )
        PutIndent();
}

inline
void COStreamBuffer::WrapAt(size_t lineLength, bool keepWord)
    THROWS1((CSerialIOException, bad_alloc))
{
    if ( keepWord ) {
        if ( GetCurrentLineLength() > lineLength )
            PutEolAtWordEnd(lineLength);
    }
    else {
        if ( GetCurrentLineLength() >= lineLength )
            PutEol(false);
    }
}

inline
size_t COStreamBuffer::GetUsedSpace(void) const
{
    return m_CurrentPos - m_Buffer;
}

inline
size_t COStreamBuffer::GetFreeSpace(void) const
{
    return m_BufferEnd - m_CurrentPos;
}

inline
size_t COStreamBuffer::GetBufferSize(void) const
{
    return m_BufferEnd - m_Buffer;
}

#endif /* def STRBUFFER__HPP  &&  ndef STRBUFFER__INL */
