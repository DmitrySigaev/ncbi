#ifndef COMMENTS__HPP
#define COMMENTS__HPP

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
*/

#include <corelib/ncbistd.hpp>
#include <list>

BEGIN_NCBI_SCOPE

class CComments
{
public:
    CComments(void);
    ~CComments(void);
    CComments& operator= (const CComments& other);

    void Add(const string& s);

    enum {
        eAlwaysMultiline = 1,
        eDoNotWriteBlankLine = 2,
        eNoEOL = 4,
        eOneLine = eDoNotWriteBlankLine | eNoEOL,
        eMultiline = eDoNotWriteBlankLine | eAlwaysMultiline
    };

    bool Empty(void) const;
    bool OneLine(void) const;
    
    CNcbiOstream& Print(CNcbiOstream& out, const string& before,
                        const string& between, const string& after) const;
    CNcbiOstream& PrintDTD(CNcbiOstream& out, int flags = 0) const;
    CNcbiOstream& PrintASN(CNcbiOstream& out, int indent, int flags = 0) const;

    // shortcuts
    CNcbiOstream& PrintHPPEnum(CNcbiOstream& out) const;
    CNcbiOstream& PrintHPPClass(CNcbiOstream& out) const;
    CNcbiOstream& PrintHPPMember(CNcbiOstream& out) const;

private:
    typedef list<string> TComments;

    TComments m_Comments;
};

template<class Container>
inline
bool SizeIsOne(const Container& cont)
{
    typename Container::const_iterator it = cont.begin(), end = cont.end();
    return it != end && ++it == end;
}

inline
bool CComments::Empty(void) const
{
    return m_Comments.empty();
}

inline
bool CComments::OneLine(void) const
{
    return SizeIsOne(m_Comments);
}

//#include <serial/datatool/comments.inl>

END_NCBI_SCOPE

#endif  /* COMMENTS__HPP */
/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.4  2006/10/18 13:13:02  gouriano
* Added comments into typestrings and generated code
*
* Revision 1.3  2006/07/24 18:57:13  gouriano
* Preserve comments when parsing DTD
*
* Revision 1.2  2001/05/17 15:00:42  lavr
* Typos corrected
*
* Revision 1.1  2000/11/29 17:42:29  vasilche
* Added CComment class for storing/printing ASN.1/XML module comments.
* Added srcutil.hpp file to reduce file dependency.
*
* ===========================================================================
*/
