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
*   Type description for enumerated types
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.6  2000/02/01 21:47:57  vasilche
* Added CGeneratedChoiceTypeInfo for generated choice classes.
* Removed CMemberInfo subclasses.
* Added support for DEFAULT/OPTIONAL members.
* Changed class generation.
* Moved datatool headers to include/internal/serial/tool.
*
* Revision 1.5  1999/12/21 17:18:33  vasilche
* Added CDelayedFostream class which rewrites file only if contents is changed.
*
* Revision 1.4  1999/12/03 21:42:11  vasilche
* Fixed conflict of enums in choices.
*
* Revision 1.3  1999/12/01 17:36:25  vasilche
* Fixed CHOICE processing.
*
* Revision 1.2  1999/11/15 19:36:14  vasilche
* Fixed warnings on GCC
*
* ===========================================================================
*/

#include <serial/tool/enumtype.hpp>
#include <serial/tool/value.hpp>
#include <serial/tool/enumstr.hpp>
#include <serial/tool/fileutil.hpp>
#include <serial/enumerated.hpp>

const char* CEnumDataType::GetASNKeyword(void) const
{
    return "ENUMERATED";
}

bool CEnumDataType::IsInteger(void) const
{
    return false;
}

void CEnumDataType::AddValue(const string& valueName, long value)
{
    m_Values.push_back(TValues::value_type(valueName, value));
}

void CEnumDataType::PrintASN(CNcbiOstream& out, int indent) const
{
    out << GetASNKeyword() << " {";
    indent++;
    for ( TValues::const_iterator i = m_Values.begin();
          i != m_Values.end(); ++i ) {
        if ( i != m_Values.begin() )
            out << ',';
        NewLine(out, indent);
        out << i->first << " (" << i->second << ")";
    }
    NewLine(out, indent - 1);
    out << "}";
}

bool CEnumDataType::CheckValue(const CDataValue& value) const
{
    const CIdDataValue* id = dynamic_cast<const CIdDataValue*>(&value);
    if ( id ) {
        for ( TValues::const_iterator i = m_Values.begin();
              i != m_Values.end(); ++i ) {
            if ( i->first == id->GetValue() )
                return true;
        }
        value.Warning("illegal ENUMERATED value: " + id->GetValue());
        return false;
    }

    const CIntDataValue* intValue =
        dynamic_cast<const CIntDataValue*>(&value);
    if ( !intValue ) {
        value.Warning("ENUMERATED or INTEGER value expected");
        return false;
    }

    if ( !IsInteger() ) {
        for ( TValues::const_iterator i = m_Values.begin();
              i != m_Values.end(); ++i ) {
            if ( i->second == intValue->GetValue() )
                return true;
        }
        value.Warning("illegal INTEGER value: " + intValue->GetValue());
        return false;
    }

    return true;
}

TObjectPtr CEnumDataType::CreateDefault(const CDataValue& value) const
{
    const CIdDataValue* id = dynamic_cast<const CIdDataValue*>(&value);
    if ( id == 0 ) {
        return new int(dynamic_cast<const CIntDataValue&>(value).GetValue());
    }
    for ( TValues::const_iterator i = m_Values.begin();
          i != m_Values.end(); ++i ) {
        if ( i->first == id->GetValue() )
            return new int(i->second);
    }
    value.Warning("illegal ENUMERATED value: " + id->GetValue());
    return 0;
}

string CEnumDataType::GetDefaultString(const CDataValue& value) const
{
    const CIdDataValue* id = dynamic_cast<const CIdDataValue*>(&value);
    if ( id ) {
        return GetEnumCInfo().valuePrefix + Identifier(id->GetValue());
    }
    else {
        const CIntDataValue* intValue =
            dynamic_cast<const CIntDataValue*>(&value);
        return NStr::IntToString(intValue->GetValue());
    }
}

CTypeInfo* CEnumDataType::CreateTypeInfo(void)
{
    AutoPtr<CEnumeratedTypeValues> info(new CEnumeratedTypeValues(IdName(),
                                                                  IsInteger()));
    for ( TValues::const_iterator i = m_Values.begin();
          i != m_Values.end(); ++i ) {
        info->AddValue(i->first, i->second);
    }
    return new CEnumeratedTypeInfo(info.release());
}

string CEnumDataType::DefaultEnumName(void) const
{
    // generate enum name from ASN type or field name
    if ( !GetParentType() ) {
        // root enum
        return 'E' + ClassName();
    }
    else {
        // internal enum
        return 'E' + Identifier(GetKeyPrefix());
    }
}

CEnumDataType::SEnumCInfo CEnumDataType::GetEnumCInfo(void) const
{
    string typeName = GetVar("_type");
    string enumName;
    if ( !typeName.empty() && typeName[0] == 'E' ) {
        enumName = typeName;
    }
    else {
        // make C++ type name
        enumName = DefaultEnumName();
        if ( typeName.empty() ) {
            if ( IsInteger() )
                typeName = "int";
            else
                typeName = enumName;
        }
    }
    string prefix = GetVar("_prefix");
    if ( prefix.empty() ) {
        prefix = char(tolower(enumName[0])) + enumName.substr(1) + '_';
    }
    return SEnumCInfo(enumName, typeName, prefix);
}

AutoPtr<CTypeStrings> CEnumDataType::GetRefCType(void) const
{
    SEnumCInfo enumInfo = GetEnumCInfo();
    return AutoPtr<CTypeStrings>(new CEnumRefTypeStrings(enumInfo.enumName,
                                                         enumInfo.cType,
                                                         Namespace(),
                                                         FileName()));
}

AutoPtr<CTypeStrings> CEnumDataType::GetFullCType(void) const
{
    SEnumCInfo enumInfo = GetEnumCInfo();
    return AutoPtr<CTypeStrings>(new CEnumTypeStrings(enumInfo.enumName,
                                                      enumInfo.cType,
                                                      IsInteger(),
                                                      m_Values,
                                                      enumInfo.valuePrefix));
}

AutoPtr<CTypeStrings> CEnumDataType::GenerateCode(void) const
{
    return GetFullCType();
}

const char* CIntEnumDataType::GetASNKeyword(void) const
{
    return "INTEGER";
}

bool CIntEnumDataType::IsInteger(void) const
{
    return true;
}

