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
* Revision 1.23  2000/11/01 20:38:59  vasilche
* OPTIONAL and DEFAULT are not permitted in CHOICE.
* Fixed code generation for DEFAULT.
*
* Revision 1.22  2000/09/26 17:38:26  vasilche
* Fixed incomplete choiceptr implementation.
* Removed temporary comments.
*
* Revision 1.21  2000/08/25 15:59:23  vasilche
* Renamed directory tool -> datatool.
*
* Revision 1.20  2000/04/07 19:26:32  vasilche
* Added namespace support to datatool.
* By default with argument -oR datatool will generate objects in namespace
* NCBI_NS_NCBI::objects (aka ncbi::objects).
* Datatool's classes also moved to NCBI namespace.
*
* Revision 1.19  2000/02/01 21:48:04  vasilche
* Added CGeneratedChoiceTypeInfo for generated choice classes.
* Removed CMemberInfo subclasses.
* Added support for DEFAULT/OPTIONAL members.
* Changed class generation.
* Moved datatool headers to include/internal/serial/tool.
*
* Revision 1.18  2000/01/10 19:46:45  vasilche
* Fixed encoding/decoding of REAL type.
* Fixed encoding/decoding of StringStore.
* Fixed encoding/decoding of NULL type.
* Fixed error reporting.
* Reduced object map (only classes).
*
* Revision 1.17  1999/12/28 18:55:59  vasilche
* Reduced size of compiled object files:
* 1. avoid inline or implicit virtual methods (especially destructors).
* 2. avoid std::string's methods usage in inline methods.
* 3. avoid string literals ("xxx") in inline methods.
*
* Revision 1.16  1999/12/03 21:42:12  vasilche
* Fixed conflict of enums in choices.
*
* Revision 1.15  1999/11/19 15:48:10  vasilche
* Modified AutoPtr template to allow its use in STL containers (map, vector etc.)
*
* Revision 1.14  1999/11/15 19:36:18  vasilche
* Fixed warnings on GCC
*
* ===========================================================================
*/

#include <serial/datatool/parser.hpp>
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

BEGIN_NCBI_SCOPE

AutoPtr<CFileModules> ASNParser::Modules(const string& fileName)
{
    AutoPtr<CFileModules> modules(new CFileModules(fileName));
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

CDataType* ASNParser::x_Type(void)
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
            return TypesBlock(new CDataSequenceType(), true);
    case K_SET:
        Consume();
        if ( ConsumeIf(K_OF) )
            return new CUniSetDataType(Type());
        else
            return TypesBlock(new CDataSetType(), true);
    case K_CHOICE:
        Consume();
        return TypesBlock(new CChoiceDataType(), false);
    case K_VisibleString:
        Consume();
        return new CStringDataType();
    case K_StringStore:
        Consume();
        return new CStringStoreDataType();
    case T_IDENTIFIER:
    case T_TYPE_REFERENCE:
        return new CReferenceDataType(TypeReference());
    }
    ParseError("type");
	return 0;
}

CDataType* ASNParser::TypesBlock(CDataMemberContainerType* containerType,
                                 bool allowDefaults)
{
    AutoPtr<CDataMemberContainerType> container(containerType);
    ConsumeSymbol('{');
    do {
        container->AddMember(NamedDataType(allowDefaults));
    } while ( ConsumeIfSymbol(',') );
    ConsumeSymbol('}');
    return container.release();
}

AutoPtr<CDataMember> ASNParser::NamedDataType(bool allowDefaults)
{
    string name;
    if ( Next() == T_IDENTIFIER )
        name = Identifier();
    AutoPtr<CDataType> type(Type());

    AutoPtr<CDataMember> member(new CDataMember(name, type));
    if ( allowDefaults ) {
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
    }
    return member;
}

CEnumDataType* ASNParser::EnumeratedBlock(CEnumDataType* enumType)
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
        return AutoPtr<CDataValue>(new CIntDataValue(Number()));
    case T_STRING:
        return AutoPtr<CDataValue>(new CStringDataValue(String()));
    case K_NULL:
        Consume();
        return AutoPtr<CDataValue>(new CNullDataValue());
    case K_FALSE:
        Consume();
        return AutoPtr<CDataValue>(new CBoolDataValue(false));
    case K_TRUE:
        Consume();
        return AutoPtr<CDataValue>(new CBoolDataValue(true));
    case T_IDENTIFIER:
        {
            string id = Identifier();
            if ( CheckSymbols(',', '}') )
                return AutoPtr<CDataValue>(new CIdDataValue(id));
            else
                return AutoPtr<CDataValue>(new CNamedDataValue(id, Value()));
        }
    case T_BINARY_STRING:
    case T_HEXADECIMAL_STRING:
        return AutoPtr<CDataValue>(new CBitStringDataValue(ConsumeAndValue()));
    case T_SYMBOL:
        switch ( NextToken().GetSymbol() ) {
        case '-':
            return AutoPtr<CDataValue>(new CIntDataValue(Number()));
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
                return AutoPtr<CDataValue>(b.release());
            }
        }
		break;
    }
    ParseError("value");
	return AutoPtr<CDataValue>(0);
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

END_NCBI_SCOPE
