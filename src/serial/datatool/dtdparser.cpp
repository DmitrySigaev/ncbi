
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
*   DTD lexer
*
* ===========================================================================
*/

#include <serial/datatool/dtdparser.hpp>
#include <serial/datatool/tokens.hpp>
#include <serial/datatool/module.hpp>
#include <serial/datatool/moduleset.hpp>
#include <serial/datatool/type.hpp>
#include <serial/datatool/statictype.hpp>
#include <serial/datatool/enumtype.hpp>
#include <serial/datatool/reftype.hpp>
#include <serial/datatool/unitype.hpp>
#include <serial/datatool/blocktype.hpp>
#include <serial/datatool/choicetype.hpp>
#include <serial/datatool/value.hpp>
#include <algorithm>
#include <corelib/ncbifile.hpp>

BEGIN_NCBI_SCOPE

/////////////////////////////////////////////////////////////////////////////
// DTDElement

DTDElement::DTDElement(void)
{
    m_Type = eUnknown;
    m_Occ  = eOne;
    m_Refd = false;
    m_Embd = false;
}

DTDElement::DTDElement(const DTDElement& other)
{
    m_Name = other.m_Name;
    m_Type = other.eUnknown;
    m_Occ  = other.m_Occ;
    m_Refd = other.m_Refd;
    m_Embd = other.m_Embd;
    for (list<string>::const_iterator i = other.m_Refs.begin();
        i != other.m_Refs.end(); ++i) {
        m_Refs.push_back( *i);
    }
    for (map<string,EOccurrence>::const_iterator i = other.m_RefOcc.begin();
        i != other.m_RefOcc.end(); ++i) {
        m_RefOcc[i->first] = i->second;
    }
}

DTDElement::~DTDElement(void)
{
}


void DTDElement::SetName(const string& name)
{
    m_Name = name;
}
const string& DTDElement::GetName(void) const
{
    return m_Name;
}


void DTDElement::SetType( EType type)
{
    _ASSERT(m_Type == eUnknown || m_Type == type);
    m_Type = type;
}

void DTDElement::SetTypeIfUnknown( EType type)
{
    if (m_Type == eUnknown) {
        m_Type = type;
    }
}

DTDElement::EType DTDElement::GetType(void) const
{
    return (EType)m_Type;
}


void DTDElement::SetOccurrence( const string& ref_name, EOccurrence occ)
{
    m_RefOcc[ref_name] = occ;
}
DTDElement::EOccurrence DTDElement::GetOccurrence(
    const string& ref_name) const
{
    map<string,EOccurrence>::const_iterator i = m_RefOcc.find(ref_name);
    return (i != m_RefOcc.end()) ? i->second : eOne;
}


void DTDElement::SetOccurrence( EOccurrence occ)
{
    m_Occ = occ;
}
DTDElement::EOccurrence DTDElement::GetOccurrence(void) const
{
    return m_Occ;
}


void DTDElement::AddContent( const string& ref_name)
{
    m_Refs.push_back( ref_name);
}

const list<string>& DTDElement::GetContent(void) const
{
    return m_Refs;
}


void DTDElement::SetReferenced(void)
{
    m_Refd = true;
}
bool DTDElement::IsReferenced(void) const
{
    return m_Refd;
}


void DTDElement::SetEmbedded(void)
{
    m_Embd = true;
}
bool DTDElement::IsEmbedded(void) const
{
    return m_Embd;
}
string DTDElement::CreateEmbeddedName(int depth) const
{
    string name, tmp;
    list<string>::const_iterator i;
    for ( i = m_Refs.begin(); i != m_Refs.end(); ++i) {
        tmp = i->substr(0,depth);
        tmp[0] = toupper(tmp[0]);
        name += tmp;
    }
    return name;
}



/////////////////////////////////////////////////////////////////////////////
// DTDParser

DTDParser::DTDParser(DTDLexer& lexer)
    : AbstractParser(lexer)
{
}

DTDParser::~DTDParser(void)
{
}


AutoPtr<CFileModules> DTDParser::Modules(const string& fileName)
{
    AutoPtr<CFileModules> modules(new CFileModules(fileName));

    while( Next() != T_EOF ) {
        modules->AddModule(Module(CDirEntry(fileName).GetBase()));
    }
/*
    while ( Next() != T_EOF ) {
        modules->AddModule(Module());
    }
    CopyComments(modules->LastComments());
*/
    return modules;
}

AutoPtr<CDataTypeModule> DTDParser::Module(const string& name)
{
    AutoPtr<CDataTypeModule> module(new CDataTypeModule(name));

    BuildDocumentTree();

#ifdef _DEBUG
    PrintDocumentTree();
#endif

    GenerateDataTree(*module);


/*
    string moduleName = ModuleReference();
    AutoPtr<CDataTypeModule> module(new CDataTypeModule(moduleName));

    Consume(K_DEFINITIONS, "DEFINITIONS");
    Consume(T_DEFINE, "::=");
    Consume(K_BEGIN, "BEGIN");

    Next();

    CopyComments(module->Comments());

    ModuleBody(*module);
    Consume(K_END, "END");

    CopyComments(module->LastComments());

*/
    return module;
}

#ifdef _DEBUG
void DTDParser::PrintDocumentTree(void)
{
    map<string,DTDElement>::iterator i;
    for (i = m_mapElement.begin(); i != m_mapElement.end(); ++i) {
        DTDElement& node = i->second;
        DTDElement::EType type = node.GetType();
        if (((type == DTDElement::eSequence) ||
            (type == DTDElement::eChoice)) && !node.IsEmbedded()) {
            PrintDocumentNode(i->first,i->second);
        }
    }
    bool started = false;
    for (i = m_mapElement.begin(); i != m_mapElement.end(); ++i) {
        DTDElement& node = i->second;
        if (node.IsEmbedded()) {
            if (!started) {
                cout << " === embedded elements ===" << endl;
                started = true;
            }
            PrintDocumentNode(i->first,i->second);
        }
    }
    started = false;
    for (i = m_mapElement.begin(); i != m_mapElement.end(); ++i) {
        DTDElement& node = i->second;
        DTDElement::EType type = node.GetType();
        if (((type != DTDElement::eSequence) &&
            (type != DTDElement::eChoice)) && !node.IsReferenced()) {
            if (!started) {
                cout << " === UNREFERENCED elements ===" << endl;
                started = true;
            }
            PrintDocumentNode(i->first,i->second);
        }
    }
}

void DTDParser::PrintDocumentNode(const string& name, DTDElement& node)
{
    cout << name << ": ";
    switch (node.GetType()) {
    default:
    case DTDElement::eUnknown:  cout << "unknown"; break;
    case DTDElement::eString:   cout << "string";  break;
    case DTDElement::eAny:      cout << "any";     break;
    case DTDElement::eEmpty:    cout << "empty";   break;
    case DTDElement::eSequence: cout << "sequence";break;
    case DTDElement::eChoice:   cout << "choice";  break;
    }
    switch (node.GetOccurrence()) {
    default:
    case DTDElement::eOne:         cout << "(1)";    break;
    case DTDElement::eOneOrMore:   cout << "(1..*)"; break;
    case DTDElement::eZeroOrMore:  cout << "(0..*)"; break;
    case DTDElement::eZeroOrOne:   cout << "(0..1)"; break;
    }
    const list<string>& refs = node.GetContent();
    if (!refs.empty()) {
        cout << ": " << endl;
        for (list<string>::const_iterator ir= refs.begin();
            ir != refs.end(); ++ir) {
            cout << "        " << *ir;
            switch (node.GetOccurrence(*ir)) {
            default:
            case DTDElement::eOne:         cout << "(1)"; break;
            case DTDElement::eOneOrMore:   cout << "(1..*)"; break;
            case DTDElement::eZeroOrMore:  cout << "(0..*)"; break;
            case DTDElement::eZeroOrOne:   cout << "(0..1)"; break;
            }
            cout << endl;
        }
    }
    cout << endl;
}
#endif

void DTDParser::BuildDocumentTree(void)
{
    string name;
    for (;;) {
        try {
            switch ( Next() ) {
            case K_ELEMENT:
                Consume();
                // element name
                name = NextToken().GetText();
                Consume();
                ParseElementContent(name, false);
                break;
            case K_ATTLIST:
                Consume();
                break;
            case K_ENTITY:
                Consume();
                break;
            case T_EOF:
                return;
            default:
                ERR_POST("LINE " << Location() <<
                    " invalid keyword: \"") <<
                    NextToken().GetText() << "\"";
                return;
            }
        }
        catch (exception& e) {
            ERR_POST(e.what());
        }
    }
}


void DTDParser::ParseElementContent(const string& name, bool embedded)
{
    DTDElement& node = m_mapElement[ name];
    node.SetName(name);
    switch (Next()) {
    case T_IDENTIFIER:
        _ASSERT(0);
        break;
    case K_ANY:     // category
        node.SetType(DTDElement::eAny);
        Consume();
        break;
    case K_EMPTY:   // category
        node.SetType(DTDElement::eEmpty);
        Consume();
        break;
    case T_SYMBOL:     // content. the symbol must be '('
        ConsumeElementContent(node);
        if (embedded) {
            node.SetEmbedded();
            return;
        }
        break;
    default:           // ???
        break;
    }
    // element description is ended
    ConsumeSymbol('>');
}

void DTDParser::ConsumeElementContent(DTDElement& node)
{
// definition of element content:
// http://www.w3.org/TR/2000/REC-xml-20001006#sec-element-content

    string id_name;
    char symbol;
    int emb=0;
    bool skip;

    _ASSERT(NextToken().GetSymbol() == '(');

    for (skip = false; ;) {
        if (skip) {
            skip=false;
        } else {
            Consume();
        }
        switch (Next()) {
        default:
            _ASSERT(0);
            break;
        case T_IDENTIFIER:
            id_name = NextToken().GetText();
            _ASSERT(!id_name.empty());
            break;
        case K_PCDATA:
            node.SetType(DTDElement::eString);
            break;
        case T_SYMBOL:
            switch (symbol = NextToken().GetSymbol()) {
            case '(':
                // embedded content
                id_name = node.GetName();
                id_name += "__emb#__";
                id_name += NStr::IntToString(emb++);
                ParseElementContent(id_name, true);
                skip = true;
                break;
            case ')':
                // id_name could be empty if the prev token was K_PCDATA
                if (!id_name.empty()) {
                    node.AddContent(id_name);
                    node.SetTypeIfUnknown(DTDElement::eSequence);
                    m_mapElement[ id_name].SetReferenced();
                    id_name.erase();
                }
                EndElementContent( node);
                return;
            case ',':
            case '|':
                // id_name could be empty if the prev token was K_PCDATA
                if (!id_name.empty()) {
                    node.AddContent(id_name);
                    node.SetType(symbol == ',' ?
                        DTDElement::eSequence : DTDElement::eChoice);
                    m_mapElement[ id_name].SetReferenced();
                    id_name.erase();
                }
                break;
            case '+':
            case '*':
            case '?':
                _ASSERT(!id_name.empty());
                node.SetOccurrence(id_name,
                    symbol == '+' ? DTDElement::eOneOrMore :
                        (symbol == '*' ? DTDElement::eZeroOrMore :
                            DTDElement::eZeroOrOne));
                break;
            default:
                _ASSERT(0);
                break;
            }
            break;
        }
    }
}

void DTDParser::EndElementContent(DTDElement& node)
{
    _ASSERT(NextToken().GetSymbol() == ')');
    Consume();
// occurence
    char symbol;
    switch (Next()) {
    default:
        break;
    case T_SYMBOL:
        switch (symbol = NextToken().GetSymbol()) {
        default:
            break;
        case '+':
        case '*':
        case '?':
            node.SetOccurrence(
                symbol == '+' ? DTDElement::eOneOrMore :
                    (symbol == '*' ? DTDElement::eZeroOrMore :
                        DTDElement::eZeroOrOne));
            Consume();
            break;
        }
        break;
    }
    FixEmbeddedNames(node);
}

void DTDParser::FixEmbeddedNames(DTDElement& node)
{
    const list<string>& refs = node.GetContent();
    for (list<string>::const_iterator i= refs.begin(); i != refs.end(); ++i) {
        DTDElement& refNode = m_mapElement[*i];
        if (refNode.IsEmbedded()) {
            for ( int depth=1; depth<100; ++depth) {
                string testName = refNode.CreateEmbeddedName(depth);
                if (find(refs.begin(),refs.end(),testName) == refs.end()) {
                    refNode.SetName(testName);
                    break;
                }
            }
        }
    }
 }

/////////////////////////////////////////////////////////////////////////////
// model generation

void DTDParser::GenerateDataTree(CDataTypeModule& module)
{
    map<string,DTDElement>::iterator i;
    for (i = m_mapElement.begin(); i != m_mapElement.end(); ++i) {
//        if (!i->second.IsReferenced()) {
        DTDElement::EType type = i->second.GetType();
        if (((type == DTDElement::eSequence) ||
            (type == DTDElement::eChoice)) && !i->second.IsEmbedded()) {
            ModuleType(module, i->second);
        }
    }
}

void DTDParser::ModuleType(CDataTypeModule& module, const DTDElement& node)
{
    AutoPtr<CDataType> type = Type(node, node.GetOccurrence(), false);
    module.AddDefinition(node.GetName(), type);
}


AutoPtr<CDataType> DTDParser::Type(
    const DTDElement& node, DTDElement::EOccurrence occ, bool in_elem)
{
    AutoPtr<CDataType> type(x_Type(node, occ, in_elem));
    return type;
}


CDataType* DTDParser::x_Type(
    const DTDElement& node, DTDElement::EOccurrence occ, bool in_elem)
{
    CDataType* type;
    switch (node.GetType()) {
    case DTDElement::eSequence:
        if (in_elem && !node.IsEmbedded()) {
            type = new CReferenceDataType(node.GetName());
        } else {
            type = TypesBlock(new CDataSequenceType(), node);
        }
        break;
    case DTDElement::eChoice:
        if (in_elem && !node.IsEmbedded()) {
            type = new CReferenceDataType(node.GetName());
        } else {
            type = TypesBlock(new CChoiceDataType(), node);
        }
        break;
    case DTDElement::eString:
        type = new CStringDataType();
        break;
    case DTDElement::eAny:
        type = new CStringStoreDataType();  // ????
        break;
    case DTDElement::eEmpty:
        type = new CNullDataType();  // ????
        break;
    default:
        _ASSERT(0);
        break;
    }
    if ((occ == DTDElement::eOneOrMore) ||
        (occ == DTDElement::eZeroOrMore)) {
        type = new CUniSequenceDataType(type);
    }
    return type;
}

CDataType* DTDParser::TypesBlock(
    CDataMemberContainerType* containerType,const DTDElement& node)
{
    AutoPtr<CDataMemberContainerType> container(containerType);
    const list<string>& refs = node.GetContent();
    for (list<string>::const_iterator i= refs.begin(); i != refs.end(); ++i) {
        DTDElement& refNode = m_mapElement[*i];
        DTDElement::EOccurrence occ = node.GetOccurrence(*i);
        AutoPtr<CDataType> type(Type(refNode, occ, true));
        AutoPtr<CDataMember> member(new CDataMember(refNode.GetName(), type));
        if ((occ == DTDElement::eZeroOrOne) ||
            (occ == DTDElement::eZeroOrMore)) {
            member->SetOptional();
        }
        member->SetNoPrefix();
        container->AddMember(member);
    }
    return container.release();
}

END_NCBI_SCOPE


/*
 * ==========================================================================
 * $Log$
 * Revision 1.1  2002/10/15 13:54:01  gouriano
 * DTD lexer and parser, first version
 *
 *
 * ==========================================================================
 */
