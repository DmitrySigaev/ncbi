#ifndef ASNLEXER_HPP
#define ASNLEXER_HPP

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
*   ASN.1 lexer
*
*/

#include <serial/datatool/alexer.hpp>
#include <list>

BEGIN_NCBI_SCOPE

class ASNLexer : public AbstractLexer
{
    typedef AbstractLexer CParent;
public:
    ASNLexer(CNcbiIstream& in, const string& name);
    virtual ~ASNLexer();

    const string& StringValue(void) const
        {
            return m_StringValue;
        }

    bool AllowIDsEndingWithMinus(void) const
        {
            return m_AllowIDsEndingWithMinus;
        }
    void AllowIDsEndingWithMinus(bool allow)
        {
            m_AllowIDsEndingWithMinus = allow;
        }

protected:
    TToken LookupToken(void);
    void LookupComments(void);

    void StartString(void);
    void AddStringChar(char c);

    void SkipComment(void);
    TToken LookupNumber(void);
    void LookupIdentifier(void);
    void LookupString(void);
    void LookupTag(void);
    TToken LookupBinHexString(void);
    TToken LookupKeyword(void);

    string m_StringValue;
    bool m_AllowIDsEndingWithMinus;
};

END_NCBI_SCOPE

#endif
/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.10  2006/10/18 13:04:26  gouriano
* Moved Log to bottom
*
* Revision 1.9  2005/01/06 20:21:13  gouriano
* Added name property to lexers - for better diagnostics
*
* Revision 1.8  2004/02/25 19:45:47  gouriano
* Made it possible to define DEFAULT for data members of type REAL
*
* Revision 1.7  2002/09/26 16:57:31  vasilche
* Added flag for compatibility with asntool
*
* Revision 1.6  2001/06/11 14:34:58  grichenk
* Added support for numeric tags in ASN.1 specifications and data streams.
*
* Revision 1.5  2000/11/15 20:34:43  vasilche
* Added user comments to ENUMERATED types.
* Added storing of user comments to ASN.1 module definition.
*
* Revision 1.4  2000/11/14 21:41:13  vasilche
* Added preserving of ASN.1 definition comments.
*
* Revision 1.3  2000/08/25 15:58:46  vasilche
* Renamed directory tool -> datatool.
*
* Revision 1.2  2000/04/07 19:26:09  vasilche
* Added namespace support to datatool.
* By default with argument -oR datatool will generate objects in namespace
* NCBI_NS_NCBI::objects (aka ncbi::objects).
* Datatool's classes also moved to NCBI namespace.
*
* Revision 1.1  2000/02/01 21:46:19  vasilche
* Added CGeneratedChoiceTypeInfo for generated choice classes.
* Removed CMemberInfo subclasses.
* Added support for DEFAULT/OPTIONAL members.
* Changed class generation.
* Moved datatool headers to include/internal/serial/tool.
*
* Revision 1.3  1999/11/15 19:36:16  vasilche
* Fixed warnings on GCC
*
* ===========================================================================
*/
