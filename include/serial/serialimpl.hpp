#ifndef SERIALIMPL__HPP
#define SERIALIMPL__HPP

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
*   File to be included in modules implementing GetTypeInfo methods.
*
*/

#include <serial/typeinfo.hpp>
#include <serial/member.hpp>
#include <serial/serialasn.hpp>
#include <serial/stdtypes.hpp>
#include <serial/stltypes.hpp>
#include <serial/ptrinfo.hpp>
#include <serial/autoptrinfo.hpp>
#include <serial/asntypes.hpp>
#include <serial/classinfo.hpp>
#include <serial/enumerated.hpp>
#include <serial/choice.hpp>

struct valnode;

BEGIN_NCBI_SCOPE

class CMemberInfo;

//
// define type info getter for standard classes
template<typename T>
inline
TTypeInfoGetter GetStdTypeInfoGetter(const T* )
{
    return &CStdTypeInfo<T>::GetTypeInfo;
}

// some compilers cannot resolve overloading by
// (char* const*) and (const char* const*) in template
// so we'll add explicit implemetations:

TTypeInfo GetStdTypeInfo_char_ptr(void);
TTypeInfo GetStdTypeInfo_const_char_ptr(void);

inline
TTypeInfoGetter GetStdTypeInfoGetter(char* const* )
{
    return &GetStdTypeInfo_char_ptr;
}

inline
TTypeInfoGetter GetStdTypeInfoGetter(const char* const* )
{
    return &GetStdTypeInfo_const_char_ptr;
}

#define SERIAL_TYPE(Name) NCBI_NAME2(SERIAL_TYPE_,Name)
#define SERIAL_REF(Name) NCBI_NAME2(SERIAL_REF_,Name)

#define SERIAL_TYPE_CLASS(Name) Name
#define SERIAL_REF_CLASS(Name) &Name::GetTypeInfo

#define SERIAL_TYPE_STD(Name) Name
#define SERIAL_REF_STD(Name) &NCBI_NS_NCBI::CStdTypeInfo<Name>::GetTypeInfo

#define SERIAL_TYPE_StringStore() NCBI_NS_STD::string
#define SERIAL_REF_StringStore() \
    &NCBI_NS_NCBI::CStringStoreTypeInfo::GetTypeInfo

#define SERIAL_TYPE_null() bool
#define SERIAL_REF_null() \
    &NCBI_NS_NCBI::CNullBoolTypeInfo::GetTypeInfo

#define SERIAL_TYPE_ENUM(Type, Name) Type
#define SERIAL_REF_ENUM(Type, Name) \
    NCBI_NS_NCBI::CreateEnumeratedTypeInfo(Type(0), NCBI_NAME2(GetEnumInfo_,Name)())

#define SERIAL_TYPE_ENUM2(Type, Namespace, Name) Type
#define SERIAL_REF_ENUM2(Type, Namespace, Name) \
    NCBI_NS_NCBI::CreateEnumeratedTypeInfo(Type(0), Namespace NCBI_NAME2(GetEnumInfo_,Name)())

#define SERIAL_TYPE_POINTER(Type,Args) SERIAL_TYPE(Type)Args*
#define SERIAL_REF_POINTER(Type,Args) \
    &NCBI_NS_NCBI::CPointerTypeInfo::GetTypeInfo, SERIAL_REF(Type)Args

#define SERIAL_TYPE_STL_multiset(Type,Args) \
    NCBI_NS_STD::multiset<SERIAL_TYPE(Type)Args>
#define SERIAL_REF_STL_multiset(Type,Args) \
    &NCBI_NS_NCBI::CStlClassInfo_multiset<SERIAL_TYPE(Type)Args>::GetTypeInfo,\
    SERIAL_REF(Type)Args

#define SERIAL_TYPE_STL_set(Type,Args) \
    NCBI_NS_STD::set<SERIAL_TYPE(Type)Args>
#define SERIAL_REF_STL_set(Type,Args) \
    &NCBI_NS_NCBI::CStlClassInfo_set<SERIAL_TYPE(Type)Args>::GetTypeInfo,\
    SERIAL_REF(Type)Args

#define SERIAL_TYPE_STL_multimap(KeyType,KeyArgs,ValueType,ValueArgs) \
    NCBI_NS_STD::multimap<SERIAL_TYPE(KeyType)KeyArgs,SERIAL_TYPE(ValueType)ValueArgs>
#define SERIAL_REF_STL_multimap(KeyType,KeyArgs,ValueType,ValueArgs) \
    &NCBI_NS_NCBI::CStlClassInfo_multimap<SERIAL_TYPE(KeyType)KeyArgs,SERIAL_TYPE(ValueType)ValueArgs>::GetTypeInfo,\
    SERIAL_REF(KeyType)KeyArgs,SERIAL_REF(ValueType)ValueArgs

#define SERIAL_TYPE_STL_map(KeyType,KeyArgs,ValueType,ValueArgs) \
    NCBI_NS_STD::map<SERIAL_TYPE(KeyType)KeyArgs,SERIAL_TYPE(ValueType)ValueArgs>
#define SERIAL_REF_STL_map(KeyType,KeyArgs,ValueType,ValueArgs) \
    &NCBI_NS_NCBI::CStlClassInfo_map<SERIAL_TYPE(KeyType)KeyArgs,SERIAL_TYPE(ValueType)ValueArgs>::GetTypeInfo,\
    SERIAL_REF(KeyType)KeyArgs,SERIAL_REF(ValueType)ValueArgs

#define SERIAL_TYPE_STL_list(Type,Args) \
    NCBI_NS_STD::list<SERIAL_TYPE(Type)Args>
#define SERIAL_REF_STL_list(Type,Args) \
    &NCBI_NS_NCBI::CStlClassInfo_list<SERIAL_TYPE(Type)Args>::GetTypeInfo,\
    SERIAL_REF(Type)Args

#define SERIAL_TYPE_STL_list_set(Type,Args) \
    NCBI_NS_STD::list<SERIAL_TYPE(Type)Args>
#define SERIAL_REF_STL_list_set(Type,Args) \
    &NCBI_NS_NCBI::CStlClassInfo_list<SERIAL_TYPE(Type)Args>::GetSetTypeInfo,\
    SERIAL_REF(Type)Args

#define SERIAL_TYPE_STL_vector(Type,Args) \
    NCBI_NS_STD::vector<SERIAL_TYPE(Type)Args>
#define SERIAL_REF_STL_vector(Type,Args) \
    &NCBI_NS_NCBI::CStlClassInfo_vector<SERIAL_TYPE(Type)Args>::GetTypeInfo,\
    SERIAL_REF(Type)Args

#define SERIAL_TYPE_STL_CHAR_vector(Type) NCBI_NS_STD::vector<Type>
#define SERIAL_REF_STL_CHAR_vector(Type) \
    &NCBI_NS_NCBI::CStlClassInfoChar_vector<Type>::GetTypeInfo

#define SERIAL_TYPE_STL_auto_ptr(Type,Args) \
    NCBI_NS_STD::auto_ptr<SERIAL_TYPE(Type)Args>
#define SERIAL_REF_STL_auto_ptr(Type,Args) \
    &NCBI_NS_NCBI::CStlClassInfo_auto_ptr<SERIAL_TYPE(Type)Args>::GetTypeInfo,\
    SERIAL_REF(Type)Args

#define SERIAL_TYPE_STL_AutoPtr(Type,Args) NCBI_NS_NCBI::AutoPtr<SERIAL_TYPE(Type)Args>
#define SERIAL_REF_STL_AutoPtr(Type,Args) \
    &NCBI_NS_NCBI::CAutoPtrTypeInfo<SERIAL_TYPE(Type)Args>::GetTypeInfo,\
    SERIAL_REF(Type)Args

#define SERIAL_TYPE_STL_CRef(Type,Args) NCBI_NS_NCBI::CRef<SERIAL_TYPE(Type)Args>
#define SERIAL_REF_STL_CRef(Type,Args) \
    &NCBI_NS_NCBI::CRefTypeInfo<SERIAL_TYPE(Type)Args>::GetTypeInfo,\
    SERIAL_REF(Type)Args

#define SERIAL_REF_CHOICE_POINTER(Type,Args) \
    &NCBI_NS_NCBI::CChoicePointerTypeInfo::GetTypeInfo,\
    SERIAL_REF(Type)Args

#define SERIAL_REF_CHOICE_STL_auto_ptr(Type,Args) \
    &NCBI_NS_NCBI::CChoiceStlClassInfo_auto_ptr<SERIAL_TYPE(Type)Args>::GetTypeInfo,\
    SERIAL_REF(Type)Args

#define SERIAL_REF_CHOICE_STL_AutoPtr(Type,Args) \
    &NCBI_NS_NCBI::CChoiceAutoPtrTypeInfo<SERIAL_TYPE(Type)Args>::GetTypeInfo,\
    SERIAL_REF(Type)Args

#define SERIAL_TYPE_CHOICE(Type,Args) SERIAL_TYPE(Type)Args
#define SERIAL_REF_CHOICE(Type,Args) SERIAL_REF(NCBI_NAME2(CHOICE_,Type))Args

template<typename T>
struct Check
{
    static CMemberInfo* Member(const T* member, const CTypeRef& type)
        {
            return new CRealMemberInfo(size_t(member), type);
        }
    static const void* Ptr(const T* member)
        {
            return member;
        }
private:
    Check(void);
    ~Check(void);
    Check(const Check<T>&);
    Check<T>& operator=(const Check<T>&);
};

inline
CMemberInfo* Member(const void* member, const CTypeRef& typeRef)
{
    return new CRealMemberInfo(size_t(member), typeRef);
}

template<typename T>
inline
CMemberInfo* StdMember(const T* member)
{
    return new CRealMemberInfo(size_t(member), GetStdTypeInfoGetter(member));
}

template<typename T>
inline
CMemberInfo* ClassMember(const T* member)
{
    return new CRealMemberInfo(size_t(member), &T::GetTypeInfo);
}

template<typename T>
inline
CMemberInfo* EnumMember(const T* member, const CEnumeratedTypeValues* enumInfo)
{
    return new CRealMemberInfo(size_t(member),
                               CreateEnumeratedTypeInfo(*member, enumInfo));
}

#define DECLARE_BASE_OBJECT(Class) 
#define BASE_OBJECT() static_cast<const CClass_Base*>(static_cast<const CClass*>(0))
#define MEMBER_PTR(Name) &BASE_OBJECT()->Name
#define CLASS_PTR(Class) static_cast<const Class*>(BASE_OBJECT())

#define BEGIN_BASE_TYPE_INFO(Class, Class_Base, Method, Info, Args) \
const NCBI_NS_NCBI::CTypeInfo* Method(void) \
{ \
    typedef Class CClass; \
	typedef Class_Base CClass_Base; \
    static Info* info = 0; \
    if ( info == 0 ) { \
        DECLARE_BASE_OBJECT(CClass); \
        info = new Info Args;
#define BEGIN_TYPE_INFO(Class, Method, Info, Args) \
	BEGIN_BASE_TYPE_INFO(Class, Class, Method, Info, Args)

#define END_TYPE_INFO \
    } \
    return info; \
}

#define SERIAL_MEMBER(Name,Type,Args) \
    NCBI_NS_NCBI::Check<SERIAL_TYPE(Type)Args>::Ptr(MEMBER_PTR(Name)),\
    SERIAL_REF(Type)Args
#define SERIAL_STD_MEMBER(Name) \
    MEMBER_PTR(Name),NCBI_NS_NCBI::GetStdTypeInfoGetter(MEMBER_PTR(Name))
#define SERIAL_CLASS_MEMBER(Name) \
    MEMBER_PTR(Name),&Name::GetTypeInfo)
#define SERIAL_ENUM_MEMBER(Name,Type) \
    NCBI_NS_NCBI::EnumMember(MEMBER_PTR(Name),\
    NCBI_NAME2(GetEnumInfo_, Type)())
    
#define ADD_NAMED_MEMBER(Name,Mem,Type,Args) \
    NCBI_NS_NCBI::AddMember(info->GetMembers(),Name, \
                            SERIAL_MEMBER(Mem,Type,Args))
#define ADD_NAMED_STD_MEMBER(Name,Mem) \
    NCBI_NS_NCBI::AddMember(info->GetMembers(),Name, \
                            SERIAL_STD_MEMBER(Mem))
#define ADD_NAMED_CLASS_MEMBER(Name,Mem) \
    NCBI_NS_NCBI::AddMember(info->GetMembers(),Name, \
                            SERIAL_CLASS_MEMBER(Mem))
#define ADD_NAMED_ENUM_MEMBER(Name,Mem,Type) \
    NCBI_NS_NCBI::AddMember(info->GetMembers(),Name, \
                            SERIAL_ENUM_MEMBER(Mem,Type))
#define ADD_MEMBER(Name,Type,Args) ADD_NAMED_MEMBER(#Name,Name,Type,Args)
#define ADD_STD_MEMBER(Name) ADD_NAMED_STD_MEMBER(#Name,Name)
#define ADD_CLASS_MEMBER(Name) ADD_NAMED_CLASS_MEMBER(#Name,Name)
#define ADD_ENUM_MEMBER(Name,Type) ADD_NAMED_ENUM_MEMBER(#Name,Name,Type)

// member types
template<typename T>
inline
CMemberInfo* MemberInfo(const T* member, const CTypeRef typeRef)
{
	return new CRealMemberInfo(size_t(member), typeRef);
}

#if HAVE_NCBI_C
template<typename T>
inline
CMemberInfo* SetMemberInfo(const T* const* member)
{
	return MemberInfo(member, GetSetTypeRef(member));
}

template<typename T>
inline
CMemberInfo* SequenceMemberInfo(const T* const* member)
{
	return MemberInfo(member, GetSequenceTypeRef(member));
}

template<typename T>
inline
CMemberInfo* SetOfMemberInfo(const T* const* member)
{
	return MemberInfo(member, GetSetOfTypeRef(member));
}

template<typename T>
inline
CMemberInfo* SequenceOfMemberInfo(const T* const* member)
{
	return MemberInfo(member, GetSequenceOfTypeRef(member));
}

inline
CMemberInfo* OctetStringMemberInfo(const void* const* member)
{
	return MemberInfo(member, GetOctetStringTypeRef(member));
}

inline
CMemberInfo* ChoiceMemberInfo(const valnode* const* member,
                              TTypeInfo (*func)(void))
{
	return MemberInfo(member, GetChoiceTypeRef(func));
}

template<typename T>
inline
CMemberInfo* OldAsnMemberInfo(const T* const* member, const string& name,
                              T* (ASNCALL*newProc)(void),
							  T* (ASNCALL*freeProc)(T*),
                              T* (ASNCALL*readProc)(asnio*, asntype*),
                              unsigned char (ASNCALL*writeProc)(T*, asnio*, asntype*))
{
    return MemberInfo(member,
                      GetOldAsnTypeRef(name, newProc, freeProc,
                                       readProc, writeProc));
}

#endif

// type info definition
#define BEGIN_NAMED_CLASS_INFO(Name, Class) \
	BEGIN_TYPE_INFO(Class, Class::GetTypeInfo, \
					NCBI_NS_NCBI::CClassInfo<CClass>, (Name))
#define BEGIN_CLASS_INFO(Class) \
	BEGIN_NAMED_CLASS_INFO(#Class, Class)
#define BEGIN_NAMED_BASE_CLASS_INFO(Name, Class) \
	BEGIN_BASE_TYPE_INFO(Class, NCBI_NAME2(Class, _Base), \
						 NCBI_NAME2(Class, _Base)::GetTypeInfo, \
						 NCBI_NS_NCBI::CClassInfo<CClass>, (Name))

#define END_CLASS_INFO END_TYPE_INFO

#define BEGIN_NAMED_ABSTRACT_CLASS_INFO(Name, Class) \
	BEGIN_TYPE_INFO(Class, Class::GetTypeInfo, \
					NCBI_NS_NCBI::CAbstractClassInfo<CClass>, (Name))
#define BEGIN_ABSTRACT_CLASS_INFO(Class) \
	BEGIN_NAMED_ABSTRACT_CLASS_INFO(#Class, Class)
#define BEGIN_NAMED_ABSTRACT_BASE_CLASS_INFO(Name, Class) \
	BEGIN_BASE_TYPE_INFO(Class, NCBI_NAME2(Class, _Base), \
						 NCBI_NAME2(Class, _Base)::GetTypeInfo, \
						 NCBI_NS_NCBI::CAbstractClassInfo<CClass>, (Name))

#define END_ABSTRACT_CLASS_INFO END_TYPE_INFO

#define BEGIN_NAMED_DERIVED_CLASS_INFO(Name, Class, BaseClass) \
	BEGIN_TYPE_INFO(Class, Class::GetTypeInfo, \
        NCBI_NS_NCBI::CClassInfo<CClass>, (Name)) \
    SET_PARENT_CLASS(BaseClass);
#define BEGIN_DERIVED_CLASS_INFO(Class, BaseClass) \
	BEGIN_NAMED_DERIVED_CLASS_INFO(#Class, Class, BaseClass)

#define END_DERIVED_CLASS_INFO END_TYPE_INFO

#define SET_PARENT_CLASS(BaseClass) \
    info->SetParentClass(BaseClass::GetTypeInfo())

#define BEGIN_NAMED_STRUCT_INFO(Name, Class) \
    BEGIN_TYPE_INFO(NCBI_NAME2(struct_, Class), \
        NCBI_NAME2(GetTypeInfo_struct_, Class), \
        NCBI_NS_NCBI::CStructInfo<CClass>, (Name))
#define BEGIN_STRUCT_INFO(Class) BEGIN_NAMED_STRUCT_INFO(#Class, Class)

#define END_STRUCT_INFO END_TYPE_INFO

#define BEGIN_NAMED_CHOICE_INFO(Name, Class) \
BEGIN_TYPE_INFO(valnode, NCBI_NAME2(GetTypeInfo_struct_, Class), \
                NCBI_NS_NCBI::CChoiceTypeInfo, (Name))
#define BEGIN_CHOICE_INFO(Class) BEGIN_NAMED_CHOICE_INFO(#Class, Class)

#define END_CHOICE_INFO END_TYPE_INFO

#define BEGIN_NAMED_ENUM_INFO(Name, Method, Enum, IsInteger) \
const NCBI_NS_NCBI::CEnumeratedTypeValues* Method(void) \
{ static NCBI_NS_NCBI::CEnumeratedTypeValues* enumInfo = 0; if ( !enumInfo ) { \
    enumInfo = new NCBI_NS_NCBI::CEnumeratedTypeValues(Name, IsInteger); \
    Enum enumValue;
#define BEGIN_ENUM_INFO(Method, Enum, IsInteger) \
    BEGIN_NAMED_ENUM_INFO(#Enum, Method, Enum, IsInteger)

#define ADD_ENUM_VALUE(Name, Value) enumInfo->AddValue(Name, enumValue = Value)

#define END_ENUM_INFO } return enumInfo; }

// adding old ASN members
#define ASN_MEMBER(Member, Type) \
	NCBI_NAME2(Type, MemberInfo)(MEMBER_PTR(Member))
#define ADD_NAMED_ASN_MEMBER(Name, Member, Type) \
	info->GetMembers().AddMember(Name, ASN_MEMBER(Member, Type))
#define ADD_ASN_MEMBER(Member, Type) \
    ADD_NAMED_ASN_MEMBER(#Member, Member, Type)

#define OLD_ASN_MEMBER(Member, Name, Type) \
    NCBI_NS_NCBI::OldAsnMemberInfo(MEMBER_PTR(Member), Name, \
                     &NCBI_NAME2(Type, New), &NCBI_NAME2(Type, Free), \
                     &NCBI_NAME2(Type, AsnRead), &NCBI_NAME2(Type, AsnWrite))
#define ADD_NAMED_OLD_ASN_MEMBER(Name, Member, TypeName, Type) \
	info->GetMembers().AddMember(Name, OLD_ASN_MEMBER(Member, TypeName, Type))
#define ADD_OLD_ASN_MEMBER(Member, Type) \
    ADD_NAMED_OLD_ASN_MEMBER(#Member, Member, #Type, Type)

#define CHOICE_MEMBER(Member, Choices) \
    NCBI_NS_NCBI::ChoiceMemberInfo(MEMBER_PTR(Member), \
                     NCBI_NAME2(GetTypeInfo_struct_, Choices))
#define ADD_NAMED_CHOICE_MEMBER(Name, Member, Choices) \
    info->GetMembers().AddMember(Name, CHOICE_MEMBER(Member, Choices))
#define ADD_CHOICE_MEMBER(Member, Choices) \
    ADD_NAMED_CHOICE_MEMBER(#Member, Member, Choices)

#define ADD_CHOICE_STD_VARIANT(Name, Member) \
    info->AddVariant(#Name, GetStdTypeInfoGetter(MEMBER_PTR(data.NCBI_NAME2(Member, value))))
#define ADD_CHOICE_VARIANT(Name, Type, Struct) \
    info->AddVariant(#Name, NCBI_NAME3(Get, Type, TypeRef)(reinterpret_cast<const NCBI_NAME2(struct_, Struct)* const*>(MEMBER_PTR(data.ptrvalue))))

#define ADD_NAMED_NULL_SUB_CLASS(Name) \
    info->AddSubClassNull(Name)
#define ADD_NAMED_SUB_CLASS(Name, SubClass) \
    info->AddSubClass(Name, &SubClass::GetTypeInfo)
#define ADD_SUB_CLASS(Class) \
    ADD_NAMED_SUB_CLASS(#Class, Class)

#define CHOICE_VARIANT_METHODS(BaseClass,VariantClass,Name) \
bool NCBI_NAME2(BaseClass,_Base)::NCBI_NAME2(Is,Name)(void) const \
{ \
    return dynamic_cast<const VariantClass*>(this) != 0; \
} \
VariantClass* NCBI_NAME2(BaseClass,_Base)::NCBI_NAME2(Get,Name)(void) \
{ \
    return dynamic_cast<VariantClass*>(this); \
} \
const VariantClass* NCBI_NAME2(BaseClass,_Base)::NCBI_NAME2(Get,Name)(void) const \
{ \
    return dynamic_cast<const VariantClass*>(this); \
}

inline
CMemberInfo*
AddMember(CMembersInfo& info, const char* name, const void* member,
          TTypeInfo typeInfo)
{
    return info.AddMember(name, member, typeInfo);
}

// one argument:
CMemberInfo*
AddMember(CMembersInfo& info, const char* name, const void* member,
          TTypeInfoGetter f);
// two arguments:
CMemberInfo*
AddMember(CMembersInfo& info, const char* name, const void* member,
          TTypeInfoGetter1 f, TTypeInfo t);
CMemberInfo*
AddMember(CMembersInfo& info, const char* name, const void* member,
          TTypeInfoGetter1 f, TTypeInfoGetter f1);
// three arguments:
CMemberInfo*
AddMember(CMembersInfo& info, const char* name, const void* member,
          TTypeInfoGetter1 f, TTypeInfoGetter1 f1, TTypeInfo t1);
CMemberInfo*
AddMember(CMembersInfo& info, const char* name, const void* member,
          TTypeInfoGetter1 f, TTypeInfoGetter1 f1, TTypeInfoGetter f11);
CMemberInfo*
AddMember(CMembersInfo& info, const char* name, const void* member,
          TTypeInfoGetter2 f, TTypeInfo t1, TTypeInfo t2);
CMemberInfo*
AddMember(CMembersInfo& info, const char* name, const void* member,
          TTypeInfoGetter2 f, TTypeInfoGetter f1, TTypeInfo t2);
CMemberInfo*
AddMember(CMembersInfo& info, const char* name, const void* member,
          TTypeInfoGetter2 f, TTypeInfo t1, TTypeInfoGetter f2);
CMemberInfo*
AddMember(CMembersInfo& info, const char* name, const void* member,
          TTypeInfoGetter2 f, TTypeInfoGetter f1, TTypeInfoGetter f2);

END_NCBI_SCOPE

#endif
