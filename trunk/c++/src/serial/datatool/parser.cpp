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
*   ASN.1 parser
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.14  1999/11/15 19:36:18  vasilche
* Fixed warnings on GCC
*
* ===========================================================================
*/

#include "parser.hpp"
#include "tokens.hpp"
#include "module.hpp"
#include "moduleset.hpp"
#include "type.hpp"
#include "statictype.hpp"
#include "enumtype.hpp"
#include "reftype.hpp"
#include "unitype.hpp"
#include "blocktype.hpp"
#include "value.hpp"

AutoPtr<CModuleSet> ASNParser::Modules(const string& fileName)
{
    AutoPtr<CModuleSet> modules(new CModuleSet(fileName));
    while ( Next() != T_EOF ) {
        modules->AddModule(Module());
    }
    return modules;
}

AutoPtr<CDataTypeModule> ASNParser::Module(void)
{
    string moduleName = ModuleReference();
    AutoPtr<CDataTypeModule> module(new CDataTypeModule(moduleName));

    Consume(K_DEFINITIONS, "DEFINITIONS");
    Consume(T_DEFINE, "::=");
    Consume(K_BEGIN, "BEGIN");
    ModuleBody(*module);
    Consume(K_END, "END");
    return module;
}

void ASNParser::Imports(CDataTypeModule& module)
{
    do {
        list<string> types;
        TypeList(types);
        Consume(K_FROM, "FROM");
        module.AddImports(ModuleReference(), types);
    } while ( !ConsumeIfSymbol(';') );
}

void ASNParser::Exports(CDataTypeModule& module)
{
    list<string> types;
    TypeList(types);
    module.AddExports(types);
    ConsumeSymbol(';');
}

void ASNParser::ModuleBody(CDataTypeModule& module)
{
    string name;
    while ( true ) {
        try {
            switch ( Next() ) {
            case K_EXPORTS:
                Consume();
                Exports(module);
                break;
            case K_IMPORTS:
                Consume();
                Imports(module);
                break;
            case T_TYPE_REFERENCE:
            case T_IDENTIFIER:
                name = TypeReference();
                Consume(T_DEFINE, "::=");
                module.AddDefinition(name, Type());
                break;
            case T_DEFINE:
                ERR_POST(Location() << "type name omitted");
                Consume();
                module.AddDefinition("unnamed type", Type());
                break;
            case K_END:
                return;
            default:
                ERR_POST(Location() << "type definition expected");
                return;
            }
        }
        catch (runtime_error e) {
            ERR_POST(e.what());
        }
    }
}

AutoPtr<CDataType> ASNParser::Type(void)
{
    int line = NextToken().GetLine();
    AutoPtr<CDataType> type(x_Type());
    type->SetSourceLine(line);
    return type;
}

AutoPtr<CDataType> ASNParser::x_Type(void)
{
    switch ( Next() ) {
    case K_BOOLEAN:
        Consume();
        return new CBoolDataType();
    case K_INTEGER:
        Consume();
        if ( CheckSymbol('{') )
            return EnumeratedBlock(new CIntEnumDataType());
        else
            return new CIntDataType();
    case K_ENUMERATED:
        Consume();
        return EnumeratedBlock(new CEnumDataType());
    case K_REAL:
        Consume();
        return new CRealDataType();
    case K_BIT:
        Consume();
        Consume(K_STRING, "STRING");
        return new CBitStringDataType();
    case K_OCTET:
        Consume();
        Consume(K_STRING, "STRING");
        return new COctetStringDataType();
    case K_NULL:
        Consume();
        return new CNullDataType();
    case K_SEQUENCE:
        Consume();
        if ( ConsumeIf(K_OF) )
            return new CUniSequenceDataType(Type());
        else
            return TypesBlock(new CDataSequenceType());
    case K_SET:
        Consume();
        if ( ConsumeIf(K_OF) )
            return new CUniSetDataType(Type());
        else
            return TypesBlock(new CDataSetType());
    case K_CHOICE:
        Consume();
        return TypesBlock(new CChoiceDataType());
    case K_VisibleString:
        Consume();
        return new CStringDataType();
    case K_StringStore:
        Consume();
        return new CStringDataType("StringStore");
    case T_IDENTIFIER:
    case T_TYPE_REFERENCE:
        return new CReferenceDataType(TypeReference());
    }
    ParseError("type");
	return 0;
}

AutoPtr<CDataType>
ASNParser::TypesBlock(CDataMemberContainerType* containerType)
{
    AutoPtr<CDataMemberContainerType> container(containerType);
    ConsumeSymbol('{');
    do {
        container->AddMember(NamedDataType());
    } while ( ConsumeIfSymbol(',') );
    ConsumeSymbol('}');
    return container.release();
}

AutoPtr<CDataMember> ASNParser::NamedDataType(void)
{
    string name;
    if ( Next() == T_IDENTIFIER )
        name = Identifier();

    AutoPtr<CDataMember> member(new CDataMember(name, Type()));
    switch ( Next() ) {
    case K_OPTIONAL:
        Consume();
        member->SetOptional();
        break;
    case K_DEFAULT:
        Consume();
        member->SetDefault(Value());
        break;
    }
    return member;
}

AutoPtr<CDataType> ASNParser::EnumeratedBlock(CEnumDataType* enumType)
{
    AutoPtr<CEnumDataType> e(enumType);
    ConsumeSymbol('{');
    do {
        EnumeratedValue(*e);
    } while ( ConsumeIfSymbol(',') );
    ConsumeSymbol('}');
    return e.release();
}

void ASNParser::EnumeratedValue(CEnumDataType& t)
{
    string id = Identifier();
    ConsumeSymbol('(');
    long value = Number();
    ConsumeSymbol(')');
    t.AddValue(id, value);
}

void ASNParser::TypeList(list<string>& ids)
{
    do {
        ids.push_back(TypeReference());
    } while ( ConsumeIfSymbol(',') );
}

AutoPtr<CDataValue> ASNParser::Value(void)
{
    int line = NextToken().GetLine();
    AutoPtr<CDataValue> value(x_Value());
    value->SetSourceLine(line);
    return value;
}

AutoPtr<CDataValue> ASNParser::x_Value(void)
{
    switch ( Next() ) {
    case T_NUMBER:
        return new CIntDataValue(Number());
    case T_STRING:
        return new CStringDataValue(String());
    case K_NULL:
        Consume();
        return new CNullDataValue();
    case K_FALSE:
        Consume();
        return new CBoolDataValue(false);
    case K_TRUE:
        Consume();
        return new CBoolDataValue(true);
    case T_IDENTIFIER:
        {
            string id = Identifier();
            if ( CheckSymbols(',', '}') )
                return new CIdDataValue(id);
            else
                return new CNamedDataValue(id, Value());
        }
    case T_BINARY_STRING:
    case T_HEXADECIMAL_STRING:
        return new CBitStringDataValue(ConsumeAndValue());
    case T_SYMBOL:
        switch ( NextToken().GetSymbol() ) {
        case '-':
            return new CIntDataValue(Number());
        case '{':
            {
                Consume();
                AutoPtr<CBlockDataValue> b(new CBlockDataValue());
                if ( !CheckSymbol('}') ) {
                    do {
                        b->GetValues().push_back(Value());
                    } while ( ConsumeIfSymbol(',') );
                }
                ConsumeSymbol('}');
                return b.release();
            }
        }
		break;
    }
    ParseError("value");
	return 0;
}

long ASNParser::Number(void)
{
    bool minus = ConsumeIfSymbol('-');
    long value = NStr::StringToUInt(ValueOf(T_NUMBER, "number"));
    if ( minus )
        value = -value;
    return value;
}

string ASNParser::String(void)
{
    Expect(T_STRING, "string");
    const string& ret = Lexer().CurrentTokenValue();
    Consume();
    return ret;
}

string ASNParser::Identifier(void)
{
    switch ( Next() ) {
    case T_TYPE_REFERENCE:
        ERR_POST(Location() << "identifier must begin with lowercase letter");
        return ConsumeAndValue();
    case T_IDENTIFIER:
        return ConsumeAndValue();
    }
    ParseError("identifier");
	return NcbiEmptyString;
}

string ASNParser::TypeReference(void)
{
    switch ( Next() ) {
    case T_TYPE_REFERENCE:
        return ConsumeAndValue();
    case T_IDENTIFIER:
        ERR_POST(Location() << "type name must begin with uppercase letter");
        return ConsumeAndValue();
    }
    ParseError("type name");
	return NcbiEmptyString;
}

string ASNParser::ModuleReference(void)
{
    switch ( Next() ) {
    case T_TYPE_REFERENCE:
        return ConsumeAndValue();
    case T_IDENTIFIER:
        ERR_POST(Location() << "module name must begin with uppercase letter");
        return ConsumeAndValue();
    }
    ParseError("module name");
	return NcbiEmptyString;
}
