#ifndef ABSTRACT_LEXER_HPP
#define ABSTRACT_LEXER_HPP

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
*   Abstract lexer
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.6  1999/11/19 15:48:09  vasilche
* Modified AutoPtr template to allow its use in STL containers (map, vector etc.)
*
* Revision 1.5  1999/11/15 19:36:12  vasilche
* Fixed warnings on GCC
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <corelib/ncbiutil.hpp>
#include <corelib/ncbistre.hpp>
#include <vector>
#include "atoken.hpp"

USING_NCBI_SCOPE;

class AbstractLexer {
public:
    AbstractLexer(CNcbiIstream& in);
    virtual ~AbstractLexer(void);
    
    const AbstractToken& NextToken(void)
        {
            if ( TokenStarted() )
                return m_NextToken;
            else
                return FillNextToken();
        }

    void Consume(void)
        {
            if ( !TokenStarted() )
                LexerError("illegal call: Consume() without NextToken()");
            m_TokenStart = 0;
        }
    string ConsumeAndValue(void);

    string CurrentTokenText(void) const
        {
            _ASSERT(TokenStarted());
            return string(CurrentTokenStart(), CurrentTokenEnd());
        }
    const string& CurrentTokenValue(void) const
        {
            _ASSERT(TokenStarted());
            return m_TokenValue;
        }

    virtual void LexerError(const char* error);
    virtual void LexerWarning(const char* error);

protected:
    virtual TToken LookupToken(void) = 0;

    void NextLine(void)
        {
            m_Line++;
        }
    void StartToken(void)
        {
            _ASSERT(!TokenStarted());
            m_TokenStart = m_Position;
            m_NextToken.line = m_Line;
            m_TokenValue.erase();
        }
    void AddChars(int count)
        {
            _ASSERT(TokenStarted());
            m_Position += count;
            _ASSERT(m_Position <= m_DataEnd);
        }
    void AddChar(void)
        {
            AddChars(1);
        }
    void AddValueChar(char c)
        {
            m_TokenValue += c;
        }
    void SkipChars(int count)
        {
            _ASSERT(!TokenStarted());
            m_Position += count;
            _ASSERT(m_Position <= m_DataEnd);
        }
    void SkipChar(void)
        {
            SkipChars(1);
        }
    char Char(int index)
        {
            char* pos = m_Position + index;
            if ( pos < m_DataEnd )
                return *pos;
            else
                return FillChar(index);
        }
    char Char(void)
        {
            return Char(0);
        }
    bool Eof(void)
        {
            return !m_Input;
        }
    const char* CurrentTokenStart(void) const
        {
            return m_TokenStart;
        }
    const char* CurrentTokenEnd(void) const
        {
            return m_Position;
        }
    int CurrentTokenLength(void) const
        {
            return CurrentTokenEnd() - CurrentTokenStart();
        }

private:
    bool TokenStarted(void) const
        {
            return m_TokenStart != 0;
        }
    const AbstractToken& FillNextToken(void);
    char FillChar(int index);

    CNcbiIstream& m_Input;
    int m_Line;  // current line in source
    char* m_Buffer;
    char* m_AllocEnd;
    char* m_Position; // current position in buffer
    char* m_DataEnd; // end of read data in buffer
    char* m_TokenStart; // token start in buffer (0: not parsed yet)
    AbstractToken m_NextToken;
    string m_TokenValue;
};

#endif
