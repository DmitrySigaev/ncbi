
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

#include <ncbi_pch.hpp>
#include <serial/datatool/exceptions.hpp>
#include <serial/datatool/xsdparser.hpp>
#include <serial/datatool/tokens.hpp>

BEGIN_NCBI_SCOPE

/////////////////////////////////////////////////////////////////////////////
// DTDParser

XSDParser::XSDParser(XSDLexer& lexer)
    : DTDParser(lexer)
{
}

XSDParser::~XSDParser(void)
{
}

void XSDParser::BuildDocumentTree(void)
{
    Reset();
    ParseHeader();

    TToken tok;
    for (;;) {
        tok = GetNextToken();
        switch ( tok ) {
        case K_ELEMENT:
            ParseElementContent(0);
            break;
        case K_COMPLEXTYPE:
            CreateTypeDefinition();
            break;
        case K_ATTPAIR:
            break;
        case K_ENDOFTAG:
        case T_EOF:
            ProcessNamedTypes();
            return;
        default:
            ParseError("Invalid keyword", "keyword");
            return;
        }
    }
}

void XSDParser::Reset(void)
{
    m_PrefixToNamespace.clear();
    m_NamespaceToPrefix.clear();
    m_TargetNamespace.erase();
    m_ElementFormDefault = false;
}

TToken XSDParser::GetNextToken(void)
{
    TToken tok = DTDParser::GetNextToken();
    if (tok == T_EOF) {
        return tok;
    }
    string data = NextToken().GetText();
    string str1, str2, data2;

    m_Raw = data;
    m_Element.erase();
    m_ElementPrefix.erase();
    m_Attribute.erase();
    m_AttributePrefix.erase();
    m_Value.erase();
    m_ValuePrefix.erase();
    if (tok == K_ATTPAIR || tok == K_XMLNS) {
// format is
// ns:attr="ns:value"
        if (!NStr::SplitInTwo(data, "=", str1, data2)) {
            ParseError("Unexpected data", "attribute (name=\"value\")");
        }
// attribute
        data = str1;
        if (NStr::SplitInTwo(data, ":", str1, str2)) {
            m_Attribute = str2;
            m_AttributePrefix = str1;
        } else if (tok == K_XMLNS) {
            m_AttributePrefix = str1;
        } else {
            m_Attribute = str1;
        }
// value
        string::size_type first = 0, last = data2.length()-1;
        if (data2.length() < 2 || data2[first] != '\"' || data2[last] != '\"') {
            ParseError("Unexpected data", "attribute (name=\"value\")");
        }
        data = data2.substr(first+1, last - first - 1);
        if (tok == K_XMLNS) {
            if (m_PrefixToNamespace.find(m_Attribute) != m_PrefixToNamespace.end()) {
                if (!m_PrefixToNamespace[m_Attribute].empty()) {
                    ParseError("Unexpected data");
                }
            }
            m_PrefixToNamespace[m_Attribute] = data;
            m_NamespaceToPrefix[data] = m_Attribute;
            m_Value = data;
        } else {
            if (NStr::SplitInTwo(data, ":", str1, str2)) {
                if (m_PrefixToNamespace.find(str1) == m_PrefixToNamespace.end()) {
                    m_Value = data;
                } else {
                    m_Value = str2;
                    m_ValuePrefix = str1;
                }
            } else {
                m_Value = str1;
            }
        }
    } else if (tok != K_ENDOFTAG && tok != K_CLOSING) {
// format is
// ns:element
        if (NStr::SplitInTwo(data, ":", str1, str2)) {
            m_Element = str2;
            m_ElementPrefix = str1;
        } else {
            m_Element = str1;
        }
        if (m_PrefixToNamespace.find(m_ElementPrefix) == m_PrefixToNamespace.end()) {
            m_PrefixToNamespace[m_ElementPrefix] = "";
        }
    }
    ConsumeToken();
    return tok;
}

bool XSDParser::IsAttribute(const char* att) const
{
    return NStr::strcmp(m_Attribute.c_str(),att) == 0;
}

bool XSDParser::IsValue(const char* value) const
{
    return NStr::strcmp(m_Value.c_str(),value) == 0;
}

bool XSDParser::DefineElementType(DTDElement& node)
{
    if (IsValue("string") || IsValue("token") || IsValue("normalizedString")) {
        node.SetType(DTDElement::eString);
    } else if (IsValue("double") || IsValue("float") || IsValue("decimal")) {
        node.SetType(DTDElement::eDouble);
    } else if (IsValue("integer") || IsValue("int")
             || IsValue("short") || IsValue("byte") 
             || IsValue("negativeInteger") || IsValue("nonNegativeInteger")
             || IsValue("positiveInteger") || IsValue("nonPositiveInteger")
             || IsValue("unsignedInt") || IsValue("unsignedShort")
             || IsValue("unsignedByte") ) {
        node.SetType(DTDElement::eInteger);
    } else if (IsValue("long") || IsValue("unsignedLong")) {
        node.SetType(DTDElement::eBigInt);
    } else if (IsValue("hexBinary")) {
        node.SetType(DTDElement::eOctetString);
    } else {
        return false;
    }
    return true;
}

void XSDParser::ParseHeader()
{
// xml header
    TToken tok = GetNextToken();
    if (tok != K_XML) {
        ParseError("Unexpected token", "xml");
    }
    for ( ; tok != K_ENDOFTAG; tok=GetNextToken())
        ;
// schema    
    tok = GetNextToken();
    if (tok != K_SCHEMA) {
        ParseError("Unexpected token", "schema");
    }
    for ( tok = GetNextToken(); tok == K_ATTPAIR || tok == K_XMLNS; tok = GetNextToken()) {
        if (tok == K_ATTPAIR) {
            if (IsAttribute("targetNamespace")) {
                m_TargetNamespace = m_Value;
            } else if (IsAttribute("elementFormDefault")) {
                m_ElementFormDefault = IsValue("qualified");
            }
        }
    }
    if (tok != K_CLOSING) {
        ParseError("Unexpected token");
    }
}

string XSDParser::ParseElementContent(DTDElement* owner)
{
    TToken tok;
    string name;
    bool ref=false, named_type=false;
    for ( tok = GetNextToken(); tok == K_ATTPAIR; tok = GetNextToken()) {
        if (IsAttribute("ref")) {
            name = m_Value;
            ref=true;

        } else if (IsAttribute("name")) {
            name = m_Value;
            m_MapElement[name].SetName(name);

        } else if (IsAttribute("type")) {
            if (!DefineElementType(m_MapElement[name])) {
                m_MapElement[name].SetTypeName(m_Value);
                named_type = true;
            }

        } else if (IsAttribute("minOccurs")) {
            if (!owner || name.empty()) {
                ParseError("Unexpected attribute");
            }
            int m = NStr::StringToInt(m_Value);
            DTDElement::EOccurrence occNow, occNew;
            occNew = occNow = owner->GetOccurrence(name);
            if (m == 0) {
                if (occNow == DTDElement::eOne) {
                    occNew = DTDElement::eZeroOrOne;
                } else if (occNow == DTDElement::eOneOrMore) {
                    occNew = DTDElement::eZeroOrMore;
                }
            } else if (m != 1) {
                ParseError("Unsupported attribute");
            }
            if (occNow != occNew) {
                owner->SetOccurrence(name, occNew);
            }
            
        } else if (IsAttribute("maxOccurs")) {
            if (!owner || name.empty()) {
                ParseError("Unexpected attribute");
            }
            int m = IsValue("unbounded") ? -1 : NStr::StringToInt(m_Value);
            DTDElement::EOccurrence occNow, occNew;
            occNew = occNow = owner->GetOccurrence(name);
            if (m == -1) {
                if (occNow == DTDElement::eOne) {
                    occNew = DTDElement::eOneOrMore;
                } else if (occNow == DTDElement::eZeroOrOne) {
                    occNew = DTDElement::eZeroOrMore;
                }
            } else if (m != 1) {
                ParseError("Unsupported attribute");
            }
            if (occNow != occNew) {
                owner->SetOccurrence(name, occNew);
            }
        }
    }
    if (tok != K_CLOSING && tok != K_ENDOFTAG) {
        ParseError("Unexpected token");
    }
    if (tok == K_CLOSING) {
        ParseContent(m_MapElement[name]);
        FixEmbeddedNames(m_MapElement[name]);
    }
    if (!ref && !named_type) {
        m_MapElement[name].SetTypeIfUnknown(DTDElement::eEmpty);
    }
    return name;
}

void XSDParser::ParseContent(DTDElement& node)
{
    int emb=0;
    TToken tok;
    for ( tok=GetNextToken(); tok != K_ENDOFTAG; tok=GetNextToken()) {
        switch (tok) {
        case T_EOF:
            return;
        case K_COMPLEXTYPE:
            ParseComplexType(node);
            break;
        case K_SIMPLECONTENT:
            ParseSimpleContent(node);
            break;
        case K_EXTENSION:
            ParseExtension(node);
            break;
        case K_RESTRICTION:
            ParseRestriction(node);
            break;
        case K_ATTRIBUTE:
            ParseAttribute(node);
            break;
        case K_SEQUENCE:
            if (node.GetType() == DTDElement::eUnknown) {
                node.SetType(DTDElement::eSequence);
                ParseContainer(node);
            } else {
                string name = node.GetName();
                name += "__emb#__";
                name += NStr::IntToString(emb++);
                m_MapElement[name].SetName(name);
                m_MapElement[name].SetEmbedded();
                m_MapElement[name].SetType(DTDElement::eSequence);
                ParseContainer(m_MapElement[name]);
                AddElementContent(node,name);
            }
            break;
        case K_CHOICE:
            if (node.GetType() == DTDElement::eUnknown) {
                node.SetType(DTDElement::eChoice);
                ParseContainer(node);
            } else {
                string name = node.GetName();
                name += "__emb#__";
                name += NStr::IntToString(emb++);
                m_MapElement[name].SetName(name);
                m_MapElement[name].SetEmbedded();
                m_MapElement[name].SetType(DTDElement::eChoice);
                ParseContainer(m_MapElement[name]);
                AddElementContent(node,name);
            }
            break;
        case K_ELEMENT:
            {
	        string name = ParseElementContent(&node);
	        AddElementContent(node,name);
            }
            break;
        default:
            for ( tok = GetNextToken(); tok == K_ATTPAIR; tok = GetNextToken())
                ;
            if (tok == K_CLOSING) {
                ParseContent(node);
            }
            break;
        }
    }
}

void XSDParser::ParseContainer(DTDElement& node)
{
    TToken tok;
    for ( tok = GetNextToken(); tok == K_ATTPAIR; tok = GetNextToken()) {
        if (IsAttribute("minOccurs")) {
            int m = NStr::StringToInt(m_Value);
            DTDElement::EOccurrence occNow, occNew;
            occNew = occNow = node.GetOccurrence();
            if (m == 0) {
                if (occNow == DTDElement::eOne) {
                    occNew = DTDElement::eZeroOrOne;
                } else if (occNow == DTDElement::eOneOrMore) {
                    occNew = DTDElement::eZeroOrMore;
                }
            } else if (m != 1) {
                ParseError("Unsupported attribute");
            }
            if (occNow != occNew) {
                node.SetOccurrence(occNew);
            }
            
        } else if (IsAttribute("maxOccurs")) {
            int m = IsValue("unbounded") ? -1 : NStr::StringToInt(m_Value);
            DTDElement::EOccurrence occNow, occNew;
            occNew = occNow = node.GetOccurrence();
            if (m == -1) {
                if (occNow == DTDElement::eOne) {
                    occNew = DTDElement::eOneOrMore;
                } else if (occNow == DTDElement::eZeroOrOne) {
                    occNew = DTDElement::eZeroOrMore;
                }
            } else if (m != 1) {
                ParseError("Unsupported attribute");
            }
            if (occNow != occNew) {
                node.SetOccurrence(occNew);
            }
        }
    }
    if (tok != K_CLOSING) {
        ParseError("Unexpected token");
    }
    ParseContent(node);
}

void XSDParser::ParseComplexType(DTDElement& node)
{
    TToken tok;
    for ( tok = GetNextToken(); tok == K_ATTPAIR; tok = GetNextToken())
        ;
    if (tok != K_CLOSING) {
        ParseError("Unexpected token");
    }
    ParseContent(node);
}

void XSDParser::ParseSimpleContent(DTDElement& node)
{
    TToken tok;
    for ( tok = GetNextToken(); tok == K_ATTPAIR; tok = GetNextToken())
        ;
    if (tok != K_CLOSING) {
        ParseError("Unexpected token");
    }
    ParseContent(node);
}

void XSDParser::ParseExtension(DTDElement& node)
{
    TToken tok;
    string name;
    for ( tok = GetNextToken(); tok == K_ATTPAIR; tok = GetNextToken()) {
        if (IsAttribute("base")) {
            if (!DefineElementType(node)) {
                name = m_Value;
            }
        }
    }
    if (tok != K_CLOSING) {
        ParseError("Unexpected token");
    }
    if (!name.empty() && (m_MapEntity.find(name) != m_MapEntity.end())) {
        PushEntityLexer(name);
        ParseContent(node);
    }
    ParseContent(node);
}

void XSDParser::ParseRestriction(DTDElement& node)
{
    TToken tok;
    for ( tok = GetNextToken(); tok == K_ATTPAIR; tok = GetNextToken()) {
        if (IsAttribute("base")) {
            DefineElementType(node);
        }
    }
    if (tok != K_CLOSING) {
        ParseError("Unexpected token");
    }
    ParseContent(node);
}

void XSDParser::ParseAttribute(DTDElement& node)
{
    TToken tok;
    DTDAttribute att;
    for ( tok = GetNextToken(); tok == K_ATTPAIR; tok = GetNextToken()) {
        if (IsAttribute("name")) {
            att.SetName(m_Value);
        } else if (IsAttribute("type")) {
            if (IsValue("string")) {
                att.SetType(DTDAttribute::eString);
            }
        } else if (IsAttribute("use")) {
            if (IsValue("required")) {
                att.SetValueType(DTDAttribute::eRequired);
            } else if (IsValue("optional")) {
                att.SetValueType(DTDAttribute::eImplied);
            }
        } else if (IsAttribute("default")) {
            att.SetValue(m_Value);
        }
    }
    if (tok == K_CLOSING) {
        ParseContent(att);
    }
    node.AddAttribute(att);
}

void XSDParser::ParseContent(DTDAttribute& att)
{
    TToken tok;
    for ( tok=GetNextToken(); tok != K_ENDOFTAG; tok=GetNextToken()) {
        switch (tok) {
        case K_ENUMERATION:
            ParseEnumeration(att);
            break;
        case K_SIMPLETYPE:
        case K_RESTRICTION:
        default:
            for ( tok = GetNextToken(); tok == K_ATTPAIR; tok = GetNextToken())
                ;
            if (tok == K_CLOSING) {
                ParseContent(att);
            }
            break;
        }
    }
}

void XSDParser::ParseEnumeration(DTDAttribute& att)
{
    TToken tok;
    att.SetType(DTDAttribute::eEnum);
    for ( tok = GetNextToken(); tok == K_ATTPAIR; tok = GetNextToken()) {
        if (IsAttribute("value")) {
            att.AddEnumValue(m_Value);
        }
    }
}

void XSDParser::CreateTypeDefinition(void)
{
    string name, data;
    TToken tok;
    data += "<" + m_Raw;
    for ( tok = GetNextToken(); tok == K_ATTPAIR; tok = GetNextToken()) {
        data += " " + m_Raw;
        if (IsAttribute("name")) {
            name = m_Value;
            m_MapEntity[name].SetName(name);
        }
    }
    data += m_Raw;
    m_MapEntity[name].SetData(data);
    if (tok != K_CLOSING) {
        ParseError("Unexpected token");
    }
    ParseTypeDefinition(m_MapEntity[name]);
}

void XSDParser::ParseTypeDefinition(DTDEntity& ent)
{
    string data = ent.GetData();
    TToken tok;
    for ( tok=GetNextToken(); tok != K_ENDOFTAG; tok=GetNextToken()) {
        data += "<" + m_Raw;
        for ( tok = GetNextToken(); tok == K_ATTPAIR; tok = GetNextToken()) {
            data += " " + m_Raw;
        }
        data += m_Raw;
        if (tok == K_CLOSING) {
            ent.SetData(data);
            ParseTypeDefinition(ent);
            data = ent.GetData();
        }
    }
    data += m_Raw;
    ent.SetData(data);
}

void XSDParser::ProcessNamedTypes(void)
{
    bool found;
    do {
        found = false;
        map<string,DTDElement>::iterator i;
        for (i = m_MapElement.begin(); i != m_MapElement.end(); ++i) {

            DTDElement& node = i->second;
            if (node.GetType() == DTDElement::eUnknown && !node.GetTypeName().empty()) {
                found = true;
                PushEntityLexer(node.GetTypeName());
                ParseContent(node);
                FixEmbeddedNames(node);
            }
        }
    } while (found);
}

AbstractLexer* XSDParser::CreateEntityLexer(
    CNcbiIstream& in, const string& name, bool autoDelete /*=true*/)
{
    return new XSDEntityLexer(in,name);
}

END_NCBI_SCOPE


/*
 * ==========================================================================
 * $Log$
 * Revision 1.1  2006/04/20 14:00:11  gouriano
 * Added XML schema parsing
 *
 *
 * ==========================================================================
 */
