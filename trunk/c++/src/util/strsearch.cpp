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
* Author:  Mati Shomrat
*
* File Description:
*   String search utilities.
*
*   Currently there are two search utilities:
*   1. An implementation of the Boyer-Moore algorithm.
*   2. A finite state automaton.
*
*/

#include <util/strsearch.hpp>
#include <algorithm>
#include <vector>
#include <ctype.h>

NCBI_USING_NAMESPACE_STD;

BEGIN_NCBI_SCOPE

//==============================================================================
//                            CBoyerMooreMatcher
//==============================================================================

// Public:
// =======

CBoyerMooreMatcher::CBoyerMooreMatcher(const string& pattern, 
                                       NStr::ECase   case_sensitive,
                                       unsigned int  whole_word)
: m_Pattern(pattern), 
  m_PatLen(pattern.length()), 
  m_CaseSensitive(case_sensitive), 
  m_WholeWord(whole_word),
  m_LastOccurance(sm_AlphabetSize),
  m_WordDelimiters(sm_AlphabetSize)
{
    x_InitPattern();
    // Init the word deimiting alphabet
    if (m_WholeWord) {
        for (int i = 0; i < sm_AlphabetSize; ++i) {
            m_WordDelimiters[i] = (isspace(i) != 0);
        }
    }
}

CBoyerMooreMatcher::CBoyerMooreMatcher(const string& pattern,
                                       const string& word_delimeters,
                                       NStr::ECase   case_sensitive,
                                       bool          invert_delimiters)
: m_Pattern(pattern), 
  m_PatLen(pattern.length()), 
  m_CaseSensitive(case_sensitive), 
  m_WholeWord(true),
  m_LastOccurance(sm_AlphabetSize),
  m_WordDelimiters(sm_AlphabetSize)
{
    x_InitPattern();
    SetWordDelimiters(word_delimeters, invert_delimiters);
}

void CBoyerMooreMatcher::SetWordDelimiters(const string& word_delimeters,
                                           bool          invert_delimiters)
{
    m_WholeWord = eWholeWordMatch;

    string word_d = word_delimeters;
    if (m_CaseSensitive == NStr::eNocase) {
        NStr::ToUpper(word_d);
    }

    // Init the word delimiting alphabet
    for (int i = 0; i < sm_AlphabetSize; ++i) {
        char ch = m_CaseSensitive ? i : toupper(i);
        string::size_type n = word_d.find_first_of(ch);
        m_WordDelimiters[i] = (!invert_delimiters) == (n != string::npos);
    }
}


void CBoyerMooreMatcher::x_InitPattern(void)
{
    if ( m_CaseSensitive == NStr::eNocase) {
        NStr::ToUpper(m_Pattern);
    }
    
    // For each character in the alpahbet compute its last occurance in 
    // the pattern.
    
    // Initilalize vector
    size_t size = m_LastOccurance.size();
    for ( size_t i = 0;  i < size;  ++i ) {
        m_LastOccurance[i] = m_PatLen;
    }
    
    // compute right-most occurance
    for ( int j = 0;  j < (int)m_PatLen - 1;  ++j ) {
        m_LastOccurance[(int)m_Pattern[j]] = m_PatLen - j - 1;
    }
}


int CBoyerMooreMatcher::Search(const char*  text, 
                               unsigned int shift,
                               unsigned int text_len) const
{
    // Implementation note.
    // Case sensitivity check has been taken out of loop. 
    // Code size for performance optimization. (We generally choose speed).
    // (Anatoliy)
    if (m_CaseSensitive == NStr::eCase) {
        while (shift + m_PatLen <= text_len) {
            int j = (int)m_PatLen - 1;

            for (char text_char = text[shift + j];
                 j >= 0  &&  m_Pattern[j] == text_char;
                 text_char = text[shift + j] ) { --j; }

            if ( (j == -1)  &&  IsWholeWord(text, shift, text_len) ) {
                return  shift;
            } else {
                shift += (unsigned int)m_LastOccurance[text[shift + j]];
            }
        }
    } else { // case insensitive NStr::eNocase
        while (shift + m_PatLen <= text_len) {
            int j = (int)m_PatLen - 1;

            for (char text_char = toupper(text[shift + j]);
                 j >= 0  &&  m_Pattern[j] == text_char;
                 text_char = toupper(text[shift + j]) ) { --j; }

            if ( (j == -1)  &&  IsWholeWord(text, shift, text_len) ) {
                return  shift;
            } else {
                shift += (unsigned int)m_LastOccurance[text[shift + j]];
            }
        }
    }
    return -1;
}


// Private:
// ========

// Constants
const int CBoyerMooreMatcher::sm_AlphabetSize = 256;     // assuming ASCII


// Member Functions
bool CBoyerMooreMatcher::IsWholeWord(const char*  text, 
                                     unsigned int pos,
                                     unsigned int text_len) const
{
    int left, right;
    left = right = 1;

    // Words at the begging and end of text are also considered "whole"

    // check on the left  
    if (m_WholeWord & ePrefixMatch) {
        left = (pos == 0) ||
               ((pos > 0) && m_WordDelimiters[text[pos - 1]]);
    }

    // check on the right
    if (m_WholeWord & eSuffixMatch) {
        pos += (unsigned int)m_PatLen;
        right = (pos == text_len) || 
                ((pos < text_len) && m_WordDelimiters[text[pos]]);
    }


    return (left && right);
}


END_NCBI_SCOPE


/*
* ===========================================================================
*
* $Log$
* Revision 1.8  2004/03/03 17:56:02  kuznets
* Code cleane up (CBoyerMooreMatcher) to use enums instead of bools,
* better coverage or different types of whole word matchers
*
* Revision 1.7  2004/03/03 14:37:14  kuznets
* bug fix: CBoyerMooreMatcher::IsWholeWord added case when
* the tested token is at the very beggining or end of the text.
*
* Revision 1.6  2004/03/02 20:00:45  kuznets
* Changes in CBoyerMooreMatcher:
*   - added work with memory areas
*   - alternative word delimiters
*   - performance optimizations
*
* Revision 1.5  2003/11/07 17:16:23  ivanov
* Fixed  warnings on 64-bit Workshop compiler
*
* Revision 1.4  2003/02/04 20:16:15  shomrat
* Change signed to unsigned to eliminate compilation warning
*
* Revision 1.3  2002/11/05 23:01:13  shomrat
* Coding style changes
*
* Revision 1.2  2002/11/03 21:59:14  kans
* BoyerMoore takes caseSensitive, wholeWord parameters (MS)
*
* Revision 1.1  2002/10/29 16:33:11  kans
* initial checkin - Boyer-Moore string search and templated text search finite state machine (MS)
*
*
* ===========================================================================
*/

