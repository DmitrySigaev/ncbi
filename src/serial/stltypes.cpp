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
* Revision 1.28  2000/08/15 19:44:51  vasilche
* Added Read/Write hooks:
* CReadObjectHook/CWriteObjectHook for objects of specified type.
* CReadClassMemberHook/CWriteClassMemberHook for specified members.
* CReadChoiceVariantHook/CWriteChoiceVariant for specified choice variants.
* CReadContainerElementHook/CWriteContainerElementsHook for containers.
*
* Revision 1.27  2000/07/11 20:36:19  vasilche
* File included in all generated headers made lighter.
* Nonnecessary code moved to serialimpl.hpp.
*
* Revision 1.26  2000/07/03 18:42:47  vasilche
* Added interface to typeinfo via CObjectInfo and CConstObjectInfo.
* Reduced header dependency.
*
* Revision 1.25  2000/06/16 16:31:22  vasilche
* Changed implementation of choices and classes info to allow use of the same classes in generated and user written classes.
*
* Revision 1.24  2000/06/01 19:07:05  vasilche
* Added parsing of XML data.
*
* Revision 1.23  2000/05/24 20:08:50  vasilche
* Implemented XML dump.
*
* Revision 1.22  2000/05/09 16:38:40  vasilche
* CObject::GetTypeInfo now moved to CObjectGetTypeInfo::GetTypeInfo to reduce possible errors.
* Added write context to CObjectOStream.
* Inlined most of methods of helping class Member, Block, ByteBlock etc.
*
* Revision 1.21  2000/05/04 16:22:20  vasilche
* Cleaned and optimized blocks and members.
*
* Revision 1.20  2000/03/29 15:55:29  vasilche
* Added two versions of object info - CObjectInfo and CConstObjectInfo.
* Added generic iterators by class -
* 	CTypeIterator<class>, CTypeConstIterator<class>,
* 	CStdTypeIterator<type>, CStdTypeConstIterator<type>,
* 	CObjectsIterator and CObjectsConstIterator.
*
* Revision 1.19  2000/03/14 14:42:32  vasilche
* Fixed error reporting.
*
* Revision 1.18  2000/03/07 14:06:24  vasilche
* Added stream buffering to ASN.1 binary input.
* Optimized class loading/storing.
* Fixed bugs in processing OPTIONAL fields.
* Added generation of reference counted objects.
*
* Revision 1.17  2000/02/17 20:02:45  vasilche
* Added some standard serialization exceptions.
* Optimized text/binary ASN.1 reading.
* Fixed wrong encoding of StringStore in ASN.1 binary format.
* Optimized logic of object collection.
*
* Revision 1.16  2000/01/10 19:46:42  vasilche
* Fixed encoding/decoding of REAL type.
* Fixed encoding/decoding of StringStore.
* Fixed encoding/decoding of NULL type.
* Fixed error reporting.
* Reduced object map (only classes).
*
* Revision 1.15  2000/01/05 19:43:57  vasilche
* Fixed error messages when reading from ASN.1 binary file.
* Fixed storing of integers with enumerated values in ASN.1 binary file.
* Added TAG support to key/value of map.
* Added support of NULL variant in CHOICE.
*
* Revision 1.14  1999/12/28 21:04:27  vasilche
* Removed three more implicit virtual destructors.
*
* Revision 1.13  1999/12/17 19:05:05  vasilche
* Simplified generation of GetTypeInfo methods.
*
* Revision 1.12  1999/09/27 14:18:03  vasilche
* Fixed bug with overloaded construtors of Block.
*
* Revision 1.11  1999/09/23 20:38:01  vasilche
* Fixed ambiguity.
*
* Revision 1.10  1999/09/14 18:54:21  vasilche
* Fixed bugs detected by gcc & egcs.
* Removed unneeded includes.
*
* Revision 1.9  1999/09/01 17:38:13  vasilche
* Fixed vector<char> implementation.
* Added explicit naming of class info.
* Moved IMPLICIT attribute from member info to class info.
*
* Revision 1.8  1999/07/20 18:23:14  vasilche
* Added interface to old ASN.1 routines.
* Added fixed choice of subclasses to use for pointers.
*
* Revision 1.7  1999/07/19 15:50:39  vasilche
* Added interface to old ASN.1 routines.
* Added naming of key/value in STL map.
*
* Revision 1.6  1999/07/15 19:35:31  vasilche
* Implemented map<K, V>.
* Changed ASN.1 text formatting.
*
* Revision 1.5  1999/07/15 16:54:50  vasilche
* Implemented vector<X> & vector<char> as special case.
*
* Revision 1.4  1999/07/13 20:54:05  vasilche
* Fixed minor bugs.
*
* Revision 1.3  1999/07/13 20:18:22  vasilche
* Changed types naming.
*
* Revision 1.2  1999/06/04 20:51:50  vasilche
* First compilable version of serialization.
*
* Revision 1.1  1999/05/19 19:56:58  vasilche
* Commit just in case.
*
* ===========================================================================
*/

#include <serial/stltypes.hpp>
#include <serial/classinfo.hpp>
#include <serial/serialimpl.hpp>

BEGIN_NCBI_SCOPE

CStlOneArgTemplate::CStlOneArgTemplate(TTypeInfo type, bool randomOrder)
    : CParent(randomOrder), m_DataType(type)
{
}

CStlOneArgTemplate::CStlOneArgTemplate(const CTypeRef& type, bool randomOrder)
    : CParent(randomOrder), m_DataType(type)
{
}

TTypeInfo CStlOneArgTemplate::GetElementType(void) const
{
    return GetDataTypeInfo();
}

void CStlOneArgTemplate::SetDataId(const CMemberId& id)
{
    m_DataId = id;
}

CStlTwoArgsTemplate::CStlTwoArgsTemplate(TTypeInfo keyType,
                                         TTypeInfo dataType,
                                         bool randomOrder)
    : CParent(randomOrder), m_KeyType(keyType), m_ValueType(dataType)
{
}

CStlTwoArgsTemplate::CStlTwoArgsTemplate(const CTypeRef& keyType,
                                         const CTypeRef& dataType,
                                         bool randomOrder)
    : CParent(randomOrder), m_KeyType(keyType), m_ValueType(dataType)
{
}

void CStlTwoArgsTemplate::SetKeyId(const CMemberId& id)
{
    m_KeyId = id;
}

void CStlTwoArgsTemplate::SetValueId(const CMemberId& id)
{
    m_ValueId = id;
}

CStlClassInfoMapImpl::CStlClassInfoMapImpl(TTypeInfo keyType,
                                           TConstObjectPtr keyOffset,
                                           TTypeInfo valueType,
                                           TConstObjectPtr valueOffset)
    : CParent(keyType, valueType, true),
      m_KeyOffset(keyOffset), m_ValueOffset(valueOffset)
{
}

CStlClassInfoMapImpl::CStlClassInfoMapImpl(const CTypeRef& keyType,
                                           TConstObjectPtr keyOffset,
                                           const CTypeRef& valueType,
                                           TConstObjectPtr valueOffset)
    : CParent(keyType, valueType, true),
      m_KeyOffset(keyOffset), m_ValueOffset(valueOffset)
{
}

TTypeInfo CStlClassInfoMapImpl::GetElementType(void) const
{
    return GetElementClassType();
}

const CClassTypeInfo* CStlClassInfoMapImpl::GetElementClassType(void) const
{
    if ( !m_ElementType ) {
        CClassTypeInfo* classInfo =
            CClassInfoHelper<bool>::CreateAbstractClassInfo("");
        m_ElementType.reset(classInfo);
        classInfo->SetRandomOrder(false);
        classInfo->GetMembers().AddMember(GetKeyId(), m_KeyOffset,
                                          GetKeyTypeInfo());
        classInfo->GetMembers().AddMember(GetValueId(), m_ValueOffset,
                                          GetValueTypeInfo());
    }
    return m_ElementType.get();
}

void CReadStlMapContainerHook::ReadContainerElement(CObjectIStream& in,
                                                    const CObjectInfo& container)
{
    // get map type info
    _ASSERT(dynamic_cast<const CStlClassInfoMapImpl*>(container.GetTypeInfo()));
    const CStlClassInfoMapImpl* mapType =
        static_cast<const CStlClassInfoMapImpl*>(container.GetTypeInfo());
    const CClassTypeInfo* elementType = mapType->GetElementClassType();

    CObjectStackClass cls(in, elementType);
    in.BeginClass(cls, elementType);
    ReadClassElement(in, container, mapType);
    in.EndClass(cls);
}

void CStlClassInfoMapImpl::ReadKey(CObjectIStream& in,
                                   TObjectPtr keyPtr) const
{
    CObjectStackClassMember m(in);
    CObjectIStream::CClassMemberPosition pos;
    const CMembersInfo& members = GetElementClassType()->GetMembers();
    if ( in.BeginClassMember(m, members, pos) != members.FirstMemberIndex() ) {
        in.ThrowError(CObjectIStream::eFormatError, "map key expected");
    }
    in.ReadObject(keyPtr, GetKeyTypeInfo());
    in.EndClassMember(m);
    _ASSERT(pos.GetLastIndex() == members.FirstMemberIndex());
}

void CStlClassInfoMapImpl::ReadValue(CObjectIStream& in,
                                     TObjectPtr valuePtr) const
{
    CObjectStackClassMember m(in);
    CObjectIStream::CClassMemberPosition pos;
    const CMembersInfo& members = GetElementClassType()->GetMembers();
    pos.SetLastIndex(members.FirstMemberIndex());
    if ( in.BeginClassMember(m, members, pos) != members.LastMemberIndex() ) {
        in.ThrowError(CObjectIStream::eFormatError, "map value expected");
    }
    in.ReadObject(valuePtr, GetValueTypeInfo());
    in.EndClassMember(m);
    _ASSERT(pos.GetLastIndex() == members.LastMemberIndex());
    if ( in.BeginClassMember(m, members, pos) != kInvalidMember ) {
        in.ThrowError(CObjectIStream::eFormatError,
                      "end of map entry expected");
    }
}

END_NCBI_SCOPE
