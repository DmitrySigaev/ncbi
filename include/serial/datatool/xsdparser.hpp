#ifndef XSDPARSER_HPP
#define XSDPARSER_HPP

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
* Author: Andrei Gourianov
*
* File Description:
*   XML Schema parser
*
* ===========================================================================
*/

#include <corelib/ncbiutil.hpp>
#include <serial/datatool/dtdparser.hpp>
#include <serial/datatool/xsdlexer.hpp>

BEGIN_NCBI_SCOPE


/////////////////////////////////////////////////////////////////////////////
// XSDParser

class XSDParser : public DTDParser
{
public:
    XSDParser( XSDLexer& lexer);
    virtual ~XSDParser(void);

protected:
    virtual void BuildDocumentTree(CDataTypeModule& module);
    void Reset(void);
    TToken GetNextToken(void);

    bool IsAttribute(const char* att) const;
    bool IsValue(const char* value) const;
    bool DefineElementType(DTDElement& node);
    bool DefineAttributeType(DTDAttribute& att);

    void ParseHeader(void);
    void ParseInclude(void);
    TToken GetRawAttributeSet(void);
    bool GetAttribute(const string& att);

    string ParseElementContent(DTDElement* owner, int& emb);
    void ParseContent(DTDElement& node);
    void ParseDocumentation(void);
    void ParseContainer(DTDElement& node);

    void ParseComplexType(DTDElement& node);
    void ParseSimpleContent(DTDElement& node);
    void ParseExtension(DTDElement& node);
    void ParseRestriction(DTDElement& node);
    void ParseAttribute(DTDElement& node);

    string ParseAttributeContent(void);
    void ParseContent(DTDAttribute& att);
    void ParseRestriction(DTDAttribute& att);
    void ParseEnumeration(DTDAttribute& att);

    void CreateTypeDefinition(void);
    void ParseTypeDefinition(DTDEntity& ent);
    void ProcessNamedTypes(void);

    virtual void PushEntityLexer(const string& name);
    virtual bool PopEntityLexer(void);
    virtual AbstractLexer* CreateEntityLexer(
        CNcbiIstream& in, const string& name, bool autoDelete=true);

#if defined(NCBI_DTDPARSER_TRACE)
    virtual void PrintDocumentTree(void);
#endif

protected:
    string m_Raw;
    string m_Element;
    string m_ElementPrefix;
    string m_Attribute;
    string m_AttributePrefix;
    string m_Value;
    string m_ValuePrefix;

    map<string,string> m_RawAttributes;

    map<string,string> m_PrefixToNamespace;
    map<string,string> m_NamespaceToPrefix;
    map<string,DTDAttribute> m_MapAttribute;
    string m_TargetNamespace;
    bool m_ElementFormDefault;

private:
    stack< map<string,string> > m_StackPrefixToNamespace;
    stack< map<string,string> > m_StackNamespaceToPrefix;
    stack<string> m_StackTargetNamespace;
    stack<bool> m_StackElementFormDefault;
};

END_NCBI_SCOPE

#endif // XSDPARSER_HPP


/*
 * ==========================================================================
 * $Log$
 * Revision 1.6  2006/08/03 17:23:02  gouriano
 * Preserve comments when parsing schema
 *
 * Revision 1.5  2006/06/27 17:58:24  gouriano
 * Parse attributes as SET
 *
 * Revision 1.4  2006/06/05 15:33:32  gouriano
 * Implemented local elements when parsing XML schema
 *
 * Revision 1.3  2006/05/10 18:48:52  gouriano
 * Added documentation parsing
 *
 * Revision 1.2  2006/05/03 14:37:38  gouriano
 * Added parsing attribute definition and include
 *
 * Revision 1.1  2006/04/20 14:00:56  gouriano
 * Added XML schema parsing
 *
 *
 * ==========================================================================
 */
