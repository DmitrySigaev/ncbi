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
*   Type description for compound types: SET, SEQUENCE and CHOICE
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.23  2000/09/18 20:00:28  vasilche
* Separated CVariantInfo and CMemberInfo.
* Implemented copy hooks.
* All hooks now are stored in CTypeInfo/CMemberInfo/CVariantInfo.
* Most type specific functions now are implemented via function pointers instead of virtual functions.
*
* Revision 1.22  2000/08/25 15:59:19  vasilche
* Renamed directory tool -> datatool.
*
* Revision 1.21  2000/08/15 19:45:27  vasilche
* Added Read/Write hooks:
* CReadObjectHook/CWriteObjectHook for objects of specified type.
* CReadClassMemberHook/CWriteClassMemberHook for specified members.
* CReadChoiceVariantHook/CWriteChoiceVariant for specified choice variants.
* CReadContainerElementHook/CWriteContainerElementsHook for containers.
*
* Revision 1.20  2000/07/03 18:42:57  vasilche
* Added interface to typeinfo via CObjectInfo and CConstObjectInfo.
*
* Revision 1.19  2000/06/16 16:31:37  vasilche
* Changed implementation of choices and classes info to allow use of the same classes in generated and user written classes.
*
* Revision 1.18  2000/05/24 20:57:14  vasilche
* Use new macro _DEBUG_ARG to avoid warning about unused argument.
*
* Revision 1.17  2000/05/24 20:09:27  vasilche
* Implemented DTD generation.
*
* Revision 1.16  2000/05/03 14:38:17  vasilche
* SERIAL: added support for delayed reading to generated classes.
* DATATOOL: added code generation for delayed reading.
*
* Revision 1.15  2000/04/17 19:11:07  vasilche
* Fixed failed assertion.
* Removed redundant namespace specifications.
*
* Revision 1.14  2000/04/12 15:36:48  vasilche
* Added -on <namespace> argument to datatool.
* Removed unnecessary namespace specifications in generated files.
*
* Revision 1.13  2000/04/07 19:26:23  vasilche
* Added namespace support to datatool.
* By default with argument -oR datatool will generate objects in namespace
* NCBI_NS_NCBI::objects (aka ncbi::objects).
* Datatool's classes also moved to NCBI namespace.
*
* Revision 1.12  2000/03/14 14:43:10  vasilche
* All OPTIONAL members implemented via CRef<> by default.
*
* Revision 1.11  2000/03/10 15:00:45  vasilche
* Fixed OPTIONAL members reading.
*
* Revision 1.10  2000/03/07 14:06:30  vasilche
* Added generation of reference counted objects.
*
* Revision 1.9  2000/02/01 21:47:53  vasilche
* Added CGeneratedChoiceTypeInfo for generated choice classes.
* Removed CMemberInfo subclasses.
* Added support for DEFAULT/OPTIONAL members.
* Changed class generation.
* Moved datatool headers to include/internal/serial/tool.
*
* Revision 1.8  1999/12/21 17:18:33  vasilche
* Added CDelayedFostream class which rewrites file only if contents is changed.
*
* Revision 1.7  1999/12/03 21:42:10  vasilche
* Fixed conflict of enums in choices.
*
* Revision 1.6  1999/12/01 17:36:24  vasilche
* Fixed CHOICE processing.
*
* Revision 1.5  1999/11/18 17:13:05  vasilche
* Fixed generation of ENUMERATED CHOICE and VisibleString.
* Added generation of initializers to zero for primitive types and pointers.
*
* Revision 1.4  1999/11/16 15:41:16  vasilche
* Added plain pointer choice.
* By default we use C pointer instead of auto_ptr.
* Start adding initializers.
*
* Revision 1.3  1999/11/15 20:31:37  vasilche
* Fixed error on GCC
*
* Revision 1.2  1999/11/15 19:36:13  vasilche
* Fixed warnings on GCC
*
* ===========================================================================
*/

#include <serial/datatool/blocktype.hpp>
#include <serial/autoptrinfo.hpp>
#include <serial/datatool/value.hpp>
#include <serial/datatool/classstr.hpp>
#include <serial/classinfo.hpp>
#include <serial/member.hpp>

BEGIN_NCBI_SCOPE

class CAnyTypeClassInfo : public CClassTypeInfo
{
public:
    CAnyTypeClassInfo(const string& name, size_t count)
        : CClassTypeInfo(sizeof(AnyType) * count, name,
                         TObjectPtr(0), &CreateAnyTypeClass,
                         typeid(void), &GetAnyTypeClassId)
        {
        }

    const AnyType* GetAnyTypePtr(size_t index) const
        {
            return &static_cast<AnyType*>(0)[index];
        }
    const bool* GetSetFlagPtr(size_t index)
        {
            return &GetAnyTypePtr(index)->booleanValue;
        }

protected:
    static TObjectPtr CreateAnyTypeClass(TTypeInfo objectType)
        {
            size_t size = objectType->GetSize();
            TObjectPtr obj = new char[size];
            memset(obj, 0, size);
            return obj;
        }
    static const type_info* GetAnyTypeClassId(TConstObjectPtr /*objectPtr*/)
        {
            return 0;
        }
};

void CDataMemberContainerType::AddMember(const AutoPtr<CDataMember>& member)
{
    m_Members.push_back(member);
}

void CDataMemberContainerType::PrintASN(CNcbiOstream& out, int indent) const
{
    out << GetASNKeyword() << " {";
    indent++;
    for ( TMembers::const_iterator i = m_Members.begin();
          i != m_Members.end(); ++i ) {
        if ( i != m_Members.begin() )
            out << ',';
        NewLine(out, indent);
        (*i)->PrintASN(out, indent);
    }
    NewLine(out, indent - 1);
    out << "}";
}

void CDataMemberContainerType::PrintDTD(CNcbiOstream& out) const
{
    string tag = XmlTagName();
    out <<
        "<!ELEMENT "<<tag<<" (\n";
    const char* separator = XmlMemberSeparator();
    iterate ( TMembers, i, m_Members ) {
        if ( i != m_Members.begin() )
            out << ' '<<separator<<'\n';
        out << "               "<<tag<<'_'<<(*i)->GetName();
        if ( (*i)->Optional() )
            out << '?';
    }
    out << " )>\n";
    if ( GetParentType() == 0 )
        out << '\n';
    iterate ( TMembers, j, m_Members ) {
        (*j)->GetType()->PrintDTD(out);
    }
}

void CDataMemberContainerType::FixTypeTree(void) const
{
    CParent::FixTypeTree();
    iterate ( TMembers, i, m_Members ) {
        (*i)->GetType()->SetParent(this, (*i)->GetName());
    }
}

bool CDataMemberContainerType::CheckType(void) const
{
    bool ok = true;
    iterate ( TMembers, i, m_Members ) {
        if ( !(*i)->Check() )
            ok = false;
    }
    return ok;
}

TObjectPtr CDataMemberContainerType::CreateDefault(const CDataValue& ) const
{
    THROW1_TRACE(runtime_error,
                 GetASNKeyword() + string(" default not implemented"));
}

const char* CDataContainerType::XmlMemberSeparator(void) const
{
    return ",";
}

CTypeInfo* CDataContainerType::CreateTypeInfo(void)
{
    return CreateClassInfo();
}

CClassTypeInfo* CDataContainerType::CreateClassInfo(void)
{
    size_t count = GetMembers().size();
    // add place for 'isSet' flags
    for ( TMembers::const_iterator i = GetMembers().begin();
          i != GetMembers().end(); ++i ) {
        CDataMember* mem = i->get();
        if ( mem->Optional() )
            ++count;
    }
    auto_ptr<CAnyTypeClassInfo> typeInfo(new CAnyTypeClassInfo(GlobalName(),
                                                               count));
    size_t index = 0;
    for ( TMembers::const_iterator i = GetMembers().begin();
          i != GetMembers().end(); ++i ) {
        CDataMember* mem = i->get();
        CDataType* memType = mem->GetType();
        TConstObjectPtr memberPtr = typeInfo->GetAnyTypePtr(index++);
        CMemberInfo* memInfo =
            typeInfo->AddMember(mem->GetName(), memberPtr,
                                memType->GetTypeInfo());
        if ( mem->Optional() ) {
            if ( mem->GetDefault() ) {
                TObjectPtr defPtr = memType->CreateDefault(*mem->GetDefault());
                memInfo->SetDefault(defPtr);
            }
            else {
                memInfo->SetOptional();
            }
            memInfo->SetSetFlag(typeInfo->GetSetFlagPtr(index++));
        }
    }
    return typeInfo.release();
}

AutoPtr<CTypeStrings> CDataContainerType::GenerateCode(void) const
{
    return GetFullCType();
}

AutoPtr<CTypeStrings> CDataContainerType::GetFullCType(void) const
{
    bool isRootClass = GetParentType() == 0;
    AutoPtr<CClassTypeStrings> code(new CClassTypeStrings(GlobalName(),
                                                          ClassName()));

    bool haveUserClass = isRootClass;
/*
    bool isObject;
    if ( haveUserClass ) {
        isObject = true;
    }
    else {
        isObject = !GetVar("Object").empty();
    }
*/
    code->SetHaveUserClass(haveUserClass);
    code->SetObject(true /*isObject*/ );
    iterate ( TMembers, i, GetMembers() ) {
        string defaultCode;
        bool optional = (*i)->Optional();
        const CDataValue* defaultValue = (*i)->GetDefault();
        if ( defaultValue ) {
            defaultCode = (*i)->GetType()->GetDefaultString(*defaultValue);
            _ASSERT(!defaultCode.empty());
        }

        bool delayed = !GetVar((*i)->GetName()+".Delay").empty();
        AutoPtr<CTypeStrings> memberType = (*i)->GetType()->GetFullCType();
        code->AddMember((*i)->GetName(), memberType,
                        (*i)->GetType()->GetVar("Pointer"),
                        optional, defaultCode, delayed);
    }
    SetParentClassTo(*code);
    return AutoPtr<CTypeStrings>(code.release());
}

AutoPtr<CTypeStrings> CDataContainerType::GetRefCType(void) const
{
    return AutoPtr<CTypeStrings>(new CClassRefTypeStrings(ClassName(),
                                                          Namespace(),
                                                          FileName()));
}

const char* CDataSetType::GetASNKeyword(void) const
{
    return "SET";
}

bool CDataSetType::CheckValue(const CDataValue& value) const
{
    const CBlockDataValue* block =
        dynamic_cast<const CBlockDataValue*>(&value);
    if ( !block ) {
        value.Warning("block of values expected");
        return false;
    }

    typedef map<string, const CDataMember*> TReadValues;
    TReadValues mms;
    for ( TMembers::const_iterator m = GetMembers().begin();
          m != GetMembers().end(); ++m ) {
        mms[m->get()->GetName()] = m->get();
    }

    iterate ( CBlockDataValue::TValues, v, block->GetValues() ) {
        const CNamedDataValue* currvalue =
            dynamic_cast<const CNamedDataValue*>(v->get());
        if ( !currvalue ) {
            v->get()->Warning("named value expected");
            return false;
        }
        TReadValues::iterator member = mms.find(currvalue->GetName());
        if ( member == mms.end() ) {
            currvalue->Warning("unexpected member");
            return false;
        }
        if ( !member->second->GetType()->CheckValue(currvalue->GetValue()) ) {
            return false;
        }
        mms.erase(member);
    }
    
    for ( TReadValues::const_iterator member = mms.begin();
          member != mms.end(); ++member ) {
        if ( !member->second->Optional() ) {
            value.Warning(member->first + " member expected");
            return false;
        }
    }
    return true;
}

CClassTypeInfo* CDataSetType::CreateClassInfo(void)
{
    return CParent::CreateClassInfo()->SetRandomOrder();
}

const char* CDataSequenceType::GetASNKeyword(void) const
{
    return "SEQUENCE";
}

bool CDataSequenceType::CheckValue(const CDataValue& value) const
{
    const CBlockDataValue* block =
        dynamic_cast<const CBlockDataValue*>(&value);
    if ( !block ) {
        value.Warning("block of values expected");
        return false;
    }
    TMembers::const_iterator member = GetMembers().begin();
    CBlockDataValue::TValues::const_iterator cvalue =
        block->GetValues().begin();
    while ( cvalue != block->GetValues().end() ) {
        const CNamedDataValue* currvalue =
            dynamic_cast<const CNamedDataValue*>(cvalue->get());
        if ( !currvalue ) {
            cvalue->get()->Warning("named value expected");
            return false;
        }
        for (;;) {
            if ( member == GetMembers().end() ) {
                currvalue->Warning("unexpected value");
                return false;
            }
            if ( (*member)->GetName() == currvalue->GetName() )
                break;
            if ( !(*member)->Optional() ) {
                currvalue->GetValue().Warning((*member)->GetName() +
                                              " member expected");
                return false;
            }
            ++member;
        }
        if ( !(*member)->GetType()->CheckValue(currvalue->GetValue()) ) {
            return false;
        }
        ++member;
        ++cvalue;
    }
    while ( member != GetMembers().end() ) {
        if ( !(*member)->Optional() ) {
            value.Warning((*member)->GetName() + " member expected");
            return false;
        }
    }
    return true;
}

CDataMember::CDataMember(const string& name, const AutoPtr<CDataType>& type)
    : m_Name(name), m_Type(type), m_Optional(false)
{
}

CDataMember::~CDataMember(void)
{
}

void CDataMember::PrintASN(CNcbiOstream& out, int indent) const
{
    out << GetName() << ' ';
    GetType()->PrintASN(out, indent);
    if ( GetDefault() ) {
        GetDefault()->PrintASN(out << " DEFAULT ", indent + 1);
    }
    else if ( Optional() ) {
        out << " OPTIONAL";
    }
}

bool CDataMember::Check(void) const
{
    if ( !m_Type->Check() )
        return false;
    if ( !m_Default )
        return true;
    return GetType()->CheckValue(*m_Default);
}

void CDataMember::SetDefault(const AutoPtr<CDataValue>& value)
{
    m_Default = value;
}

void CDataMember::SetOptional(void)
{
    m_Optional = true;
}

END_NCBI_SCOPE
