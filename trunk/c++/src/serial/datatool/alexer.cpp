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
*   Abstract lexer class
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.8  2000/04/07 19:26:22  vasilche
* Added namespace support to datatool.
* By default with argument -oR datatool will generate objects in namespace
* NCBI_NS_NCBI::objects (aka ncbi::objects).
* Datatool's classes also moved to NCBI namespace.
*
* Revision 1.7  2000/02/01 21:47:51  vasilche
* Added CGeneratedChoiceTypeInfo for generated choice classes.
* Removed CMemberInfo subclasses.
* Added support for DEFAULT/OPTIONAL members.
* Changed class generation.
* Moved datatool headers to include/internal/serial/tool.
*
* Revision 1.6  1999/11/15 19:36:11  vasilche
* Fixed warnings on GCC
*
* ===========================================================================
*/

#include <serial/tool/alexer.hpp>
#include <serial/tool/atoken.hpp>

BEGIN_NCBI_SCOPE

#define READ_AHEAD 1024

AbstractLexer::AbstractLexer(CNcbiIstream& in)
    : m_Input(in), m_Line(1),
      m_Buffer(new char[READ_AHEAD]), m_AllocEnd(m_Buffer + READ_AHEAD),
      m_Position(m_Buffer), m_DataEnd(m_Position),
      m_TokenStart(0)
{
}

AbstractLexer::~AbstractLexer(void)
{
    delete []m_Buffer;
}

void AbstractLexer::LexerError(const char* error)
{
    throw runtime_error(NStr::IntToString(m_Line) + ": lexer error: " + error);
}

void AbstractLexer::LexerWarning(const char* error)
{
    NcbiCerr << m_Line << ": lexer error: " << error << NcbiEndl;
}

string AbstractLexer::ConsumeAndValue(void)
{
    if ( !TokenStarted() )
        LexerError("illegal call: Consume() without NextToken()");
    const char* token = CurrentTokenStart();
    const char* tokenEnd = CurrentTokenEnd();
    m_TokenStart = 0;
    return string(token, tokenEnd);
}

const AbstractToken& AbstractLexer::FillNextToken(void)
{
    _ASSERT(!TokenStarted());
    if ( (m_NextToken.token = LookupToken()) == T_SYMBOL ) {
        m_TokenStart = m_Position;
        m_NextToken.line = m_Line;
        if ( m_Position == m_DataEnd ) {
            // no more data read -> EOF
            m_NextToken.token = T_EOF;
        }
        else if ( CurrentTokenLength() == 0 ) {
            AddChar();
        }
        else {
            _ASSERT(CurrentTokenLength() == 1);
        }
    }
    m_NextToken.start = CurrentTokenStart();
    m_NextToken.length = CurrentTokenLength();
    return m_NextToken;
}

char AbstractLexer::FillChar(int index)
{
    char* pos = m_Position + index;
    if ( pos >= m_AllocEnd ) {
        // char will lay outside of buffer
        // first try to remove unused chars
        char* used = m_Position;
        if ( m_TokenStart != 0 && m_TokenStart < used )
            used = m_TokenStart;
        // now used if the beginning of needed data in buffer
        if ( used > m_Buffer ) {
            // skip nonused data at the beginning of buffer
            size_t dataSize = m_DataEnd - used;
            if ( dataSize > 0 ) {
                //                _TRACE("memmove(" << dataSize << ")");
                memmove(m_Buffer, used, dataSize);
            }
            size_t skip = used - m_Buffer;
            m_Position -= skip;
            m_DataEnd -= skip;
            pos -= skip;
            if ( m_TokenStart != 0 )
                m_TokenStart -= skip;
        }
        if ( pos >= m_AllocEnd ) {
            // we still need longer buffer: reallocate it
            // save old offsets
            size_t position = m_Position - m_Buffer;
            size_t dataEnd = m_DataEnd - m_Buffer;
            size_t tokenStart = m_TokenStart == 0? 0: m_TokenStart - m_Buffer;
            // new buffer size
            size_t bufferSize = pos - m_Buffer + READ_AHEAD + 1;
            // new buffer
            char* buffer = new char[bufferSize];
            // copy old data
            //            _TRACE("memcpy(" << dataEnd << ")");
            memcpy(buffer, m_Buffer, dataEnd);
            // delete old buffer
            delete []m_Buffer;
            // restore offsets
            m_Buffer = buffer;
            m_AllocEnd = buffer + bufferSize;
            m_Position = buffer + position;
            m_DataEnd = buffer + dataEnd;
            if ( m_TokenStart != 0 )
                m_TokenStart = buffer + tokenStart;
            pos = m_Position + index;
        }
    }
    while ( pos >= m_DataEnd ) {
        size_t space = m_AllocEnd - m_DataEnd;
        m_Input.read(m_DataEnd, space);
        size_t read = m_Input.gcount();
        //        _TRACE("read(" << space << ") = " << read);
        if ( read == 0 )
            return 0;
        m_DataEnd += read;
    }
    return *pos;
}

END_NCBI_SCOPE
