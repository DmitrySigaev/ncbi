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
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.18  2000/12/15 15:38:42  vasilche
* Added support of Int8 and long double.
* Enum values now have type Int4 instead of long.
*
* Revision 1.17  2000/11/07 17:25:40  vasilche
* Fixed encoding of XML:
*     removed unnecessary apostrophes in OCTET STRING
*     removed unnecessary content in NULL
* Added module names to CTypeInfo and CEnumeratedTypeValues
*
* Revision 1.16  2000/10/20 15:51:38  vasilche
* Fixed data error processing.
* Added interface for costructing container objects directly into output stream.
* object.hpp, object.inl and object.cpp were split to
* objectinfo.*, objecttype.*, objectiter.* and objectio.*.
*
* Revision 1.15  2000/10/17 18:45:33  vasilche
* Added possibility to turn off object cross reference detection in
* CObjectIStream and CObjectOStream.
*
* Revision 1.14  2000/09/26 18:09:48  vasilche
* Fixed some warnings.
*
* Revision 1.13  2000/09/18 20:00:21  vasilche
* Separated CVariantInfo and CMemberInfo.
* Implemented copy hooks.
* All hooks now are stored in CTypeInfo/CMemberInfo/CVariantInfo.
* Most type specific functions now are implemented via function pointers instead of virtual functions.
*
* Revision 1.12  2000/09/01 13:16:15  vasilche
* Implemented class/container/choice iterators.
* Implemented CObjectStreamCopier for copying data without loading into memory.
*
* Revision 1.11  2000/08/15 19:44:47  vasilche
* Added Read/Write hooks:
* CReadObjectHook/CWriteObjectHook for objects of specified type.
* CReadClassMemberHook/CWriteClassMemberHook for specified members.
* CReadChoiceVariantHook/CWriteChoiceVariant for specified choice variants.
* CReadContainerElementHook/CWriteContainerElementsHook for containers.
*
* Revision 1.10  2000/07/03 18:42:43  vasilche
* Added interface to typeinfo via CObjectInfo and CConstObjectInfo.
* Reduced header dependency.
*
* Revision 1.9  2000/06/07 19:45:58  vasilche
* Some code cleaning.
* Macros renaming in more clear way.
* BEGIN_NAMED_*_INFO, ADD_*_MEMBER, ADD_NAMED_*_MEMBER.
*
* Revision 1.8  2000/06/01 19:07:02  vasilche
* Added parsing of XML data.
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <corelib/ncbiutil.hpp>
#include <serial/enumvalues.hpp>
#include <serial/enumerated.hpp>
#include <serial/serialutil.hpp>
#include <serial/objistr.hpp>
#include <serial/objostr.hpp>
#include <serial/objcopy.hpp>

BEGIN_NCBI_SCOPE

CEnumeratedTypeValues::CEnumeratedTypeValues(const char* name,
                                             bool isInteger)
    : m_Name(name), m_Integer(isInteger)
{
}

CEnumeratedTypeValues::CEnumeratedTypeValues(const string& name,
                                             bool isInteger)
    : m_Name(name), m_Integer(isInteger)
{
}

CEnumeratedTypeValues::~CEnumeratedTypeValues(void)
{
}

void CEnumeratedTypeValues::SetModuleName(const string& name)
{
    if ( !m_ModuleName.empty() )
        THROW1_TRACE(runtime_error, "cannot change module name");
    m_ModuleName = name;
}

TEnumValueType CEnumeratedTypeValues::FindValue(const CLightString& name) const
{
    const TNameToValue& m = NameToValue();
    TNameToValue::const_iterator i = m.find(name);
    if ( i == m.end() ) {
        THROW1_TRACE(runtime_error, "invalid value of enumerated type");
    }
    return i->second;
}

const string& CEnumeratedTypeValues::FindName(TEnumValueType value,
                                              bool allowBadValue) const
{
    const TValueToName& m = ValueToName();
    TValueToName::const_iterator i = m.find(value);
    if ( i == m.end() ) {
        if ( allowBadValue ) {
            return NcbiEmptyString;
        }
        else {
            THROW1_TRACE(runtime_error, "invalid value of enumerated type");
        }
    }
    return *i->second;
}

void CEnumeratedTypeValues::AddValue(const string& name, TEnumValueType value)
{
    if ( name.empty() )
        THROW1_TRACE(runtime_error, "empty enum value name");
    m_Values.push_back(make_pair(name, value));
    m_ValueToName.reset(0);
    m_NameToValue.reset(0);
}

const CEnumeratedTypeValues::TValueToName&
CEnumeratedTypeValues::ValueToName(void) const
{
    TValueToName* m = m_ValueToName.get();
    if ( !m ) {
        m_ValueToName.reset(m = new TValueToName);
        iterate ( TValues, i, m_Values ) {
            (*m)[i->second] = &i->first;
        }
    }
    return *m;
}

const CEnumeratedTypeValues::TNameToValue&
CEnumeratedTypeValues::NameToValue(void) const
{
    TNameToValue* m = m_NameToValue.get();
    if ( !m ) {
        m_NameToValue.reset(m = new TNameToValue);
        iterate ( TValues, i, m_Values ) {
            const string& s = i->first;
            pair<TNameToValue::iterator, bool> p =
                m->insert(TNameToValue::value_type(s, i->second));
            if ( !p.second ) {
                THROW1_TRACE(runtime_error, "duplicated enum value name");
            }
        }
    }
    return *m;
}

void CEnumeratedTypeValues::AddValue(const char* name, TEnumValueType value)
{
    AddValue(string(name), value);
}

CEnumeratedTypeInfo::CEnumeratedTypeInfo(size_t size,
                                         const CEnumeratedTypeValues* values)
    : CParent(size, values->GetName(), ePrimitiveValueEnum),
      m_ValueType(CPrimitiveTypeInfo::GetIntegerTypeInfo(size)),
      m_Values(*values)
{
    _ASSERT(m_ValueType->GetPrimitiveValueType() == ePrimitiveValueInteger);
    if ( !values->GetModuleName().empty() )
        SetModuleName(values->GetModuleName());
    SetCreateFunction(&CreateEnum);
    SetReadFunction(&ReadEnum);
    SetWriteFunction(&WriteEnum);
    SetCopyFunction(&CopyEnum);
    SetSkipFunction(&SkipEnum);
}

bool CEnumeratedTypeInfo::IsDefault(TConstObjectPtr object) const
{
    return m_ValueType->IsDefault(object);
}

bool CEnumeratedTypeInfo::Equals(TConstObjectPtr object1,
                                 TConstObjectPtr object2) const
{
    return m_ValueType->Equals(object1, object2);
}

void CEnumeratedTypeInfo::SetDefault(TObjectPtr dst) const
{
    m_ValueType->SetDefault(dst);
}

void CEnumeratedTypeInfo::Assign(TObjectPtr dst, TConstObjectPtr src) const
{
    m_ValueType->Assign(dst, src);
}

bool CEnumeratedTypeInfo::IsSigned(void) const
{
    return m_ValueType->IsSigned();
}

Int4 CEnumeratedTypeInfo::GetValueInt4(TConstObjectPtr objectPtr) const
{
    return m_ValueType->GetValueInt4(objectPtr);
}

Uint4 CEnumeratedTypeInfo::GetValueUint4(TConstObjectPtr objectPtr) const
{
    return m_ValueType->GetValueUint4(objectPtr);
}

void CEnumeratedTypeInfo::SetValueInt4(TObjectPtr objectPtr, Int4 value) const
{
    if ( !Values().IsInteger() ) {
        // check value for acceptance
        _ASSERT(sizeof(TEnumValueType) == sizeof(value));
        TEnumValueType v = TEnumValueType(value);
        Values().FindName(v, false);
    }
    m_ValueType->SetValueInt4(objectPtr, value);
}

void CEnumeratedTypeInfo::SetValueUint4(TObjectPtr objectPtr,
                                        Uint4 value) const
{
    if ( !Values().IsInteger() ) {
        // check value for acceptance
        _ASSERT(sizeof(TEnumValueType) == sizeof(value));
        TEnumValueType v = TEnumValueType(value);
        if ( v < 0 )
            THROW1_TRACE(runtime_error, "overflow error");
        Values().FindName(v, false);
    }
    m_ValueType->SetValueUint4(objectPtr, value);
}

Int8 CEnumeratedTypeInfo::GetValueInt8(TConstObjectPtr objectPtr) const
{
    return m_ValueType->GetValueInt8(objectPtr);
}

Uint8 CEnumeratedTypeInfo::GetValueUint8(TConstObjectPtr objectPtr) const
{
    return m_ValueType->GetValueUint8(objectPtr);
}

void CEnumeratedTypeInfo::SetValueInt8(TObjectPtr objectPtr, Int8 value) const
{
    if ( !Values().IsInteger() ) {
        // check value for acceptance
        _ASSERT(sizeof(TEnumValueType) < sizeof(value));
        TEnumValueType v = TEnumValueType(value);
        if ( v != value )
            THROW1_TRACE(runtime_error, "overflow error");
        Values().FindName(v, false);
    }
    m_ValueType->SetValueInt8(objectPtr, value);
}

void CEnumeratedTypeInfo::SetValueUint8(TObjectPtr objectPtr,
                                        Uint8 value) const
{
    if ( !Values().IsInteger() ) {
        // check value for acceptance
        _ASSERT(sizeof(TEnumValueType) < sizeof(value));
        TEnumValueType v = TEnumValueType(value);
        if ( v < 0 || Uint8(v) != value )
            THROW1_TRACE(runtime_error, "overflow error");
        Values().FindName(v, false);
    }
    m_ValueType->SetValueUint8(objectPtr, value);
}

void CEnumeratedTypeInfo::GetValueString(TConstObjectPtr objectPtr,
                                         string& value) const
{
    value = Values().FindName(m_ValueType->GetValueInt(objectPtr), false);
}

void CEnumeratedTypeInfo::SetValueString(TObjectPtr objectPtr,
                                         const string& value) const
{
    m_ValueType->SetValueInt(objectPtr, Values().FindValue(value));
}

TObjectPtr CEnumeratedTypeInfo::CreateEnum(TTypeInfo objectType)
{
    const CEnumeratedTypeInfo* enumType =
        CTypeConverter<CEnumeratedTypeInfo>::SafeCast(objectType);
    return enumType->m_ValueType->Create();
}

void CEnumeratedTypeInfo::ReadEnum(CObjectIStream& in,
                                   TTypeInfo objectType,
                                   TObjectPtr objectPtr)
{
    const CEnumeratedTypeInfo* enumType =
        CTypeConverter<CEnumeratedTypeInfo>::SafeCast(objectType);
    try {
        enumType->m_ValueType->SetValueInt(objectPtr,
                                           in.ReadEnum(enumType->Values()));
    }
    catch ( ... ) {
        in.ThrowError(in.eFormatError, "invalid enum value");
        throw;
    }
}

void CEnumeratedTypeInfo::WriteEnum(CObjectOStream& out,
                                    TTypeInfo objectType,
                                    TConstObjectPtr objectPtr)
{
    const CEnumeratedTypeInfo* enumType =
        CTypeConverter<CEnumeratedTypeInfo>::SafeCast(objectType);
    try {
        out.WriteEnum(enumType->Values(),
                      enumType->m_ValueType->GetValueInt(objectPtr));
    }
    catch ( ... ) {
        out.ThrowError(out.eInvalidData, "invalid enum value");
        throw;
    }
}

void CEnumeratedTypeInfo::CopyEnum(CObjectStreamCopier& copier,
                                   TTypeInfo objectType)
{
    const CEnumeratedTypeInfo* enumType =
        CTypeConverter<CEnumeratedTypeInfo>::SafeCast(objectType);
    try {
        copier.Out().CopyEnum(enumType->Values(), copier.In());
    }
    catch ( ... ) {
        copier.ThrowError(CObjectIStream::eFormatError, "invalid enum value");
        throw;
    }
}

void CEnumeratedTypeInfo::SkipEnum(CObjectIStream& in,
                                   TTypeInfo objectType)
{
    const CEnumeratedTypeInfo* enumType =
        CTypeConverter<CEnumeratedTypeInfo>::SafeCast(objectType);
    try {
        in.ReadEnum(enumType->Values());
    }
    catch ( ... ) {
        in.ThrowError(in.eFormatError, "invalid enum value");
        throw;
    }
}

END_NCBI_SCOPE
