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
* Author: Aleksey Grichenko
*
* File Description:
*   Alias type info
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.4  2003/11/24 14:10:05  grichenk
* Changed base class for CAliasTypeInfo to CPointerTypeInfo
*
* Revision 1.3  2003/11/18 18:11:48  grichenk
* Resolve aliased type info before using it in CObjectTypeInfo
*
* Revision 1.2  2003/10/21 21:08:46  grichenk
* Fixed aliases-related bug in XML stream
*
* Revision 1.1  2003/10/21 13:45:23  grichenk
* Initial revision
*
*
* ===========================================================================
*/

#include <serial/aliasinfo.hpp>
#include <serial/serialbase.hpp>
#include <serial/serialutil.hpp>
#include <serial/objistr.hpp>
#include <serial/objostr.hpp>

BEGIN_NCBI_SCOPE


class CAliasTypeInfoFunctions
{
public:
    static void ReadAliasDefault(CObjectIStream& in,
                                 TTypeInfo objectType,
                                 TObjectPtr objectPtr);
    static void WriteAliasDefault(CObjectOStream& out,
                                  TTypeInfo objectType,
                                  TConstObjectPtr objectPtr);
    static void SkipAliasDefault(CObjectIStream& in,
                                 TTypeInfo objectType);
    static void CopyAliasDefault(CObjectStreamCopier& copier,
                                 TTypeInfo objectType);
};

typedef CAliasTypeInfoFunctions TFunc;


CAliasTypeInfo::CAliasTypeInfo(const string& name, TTypeInfo type)
    : CParent(name, type->GetSize(), type),
      m_DataOffset(0)
{
    InitAliasTypeInfoFunctions();
}


TObjectPtr CAliasTypeInfo::GetDataPointer(const CPointerTypeInfo* objectType,
                                          TObjectPtr objectPtr)
{
    const CAliasTypeInfo* alias = static_cast<const CAliasTypeInfo*>(objectType);
    return alias->GetDataPtr(objectPtr);
}

void CAliasTypeInfo::SetDataPointer(const CPointerTypeInfo* objectType,
                                    TObjectPtr objectPtr,
                                    TObjectPtr dataPtr)
{
    const CAliasTypeInfo* alias = static_cast<const CAliasTypeInfo*>(objectType);
    alias->Assign(objectPtr, dataPtr);
}

void CAliasTypeInfo::InitAliasTypeInfoFunctions(void)
{
    SetReadFunction(&TFunc::ReadAliasDefault);
    SetWriteFunction(&TFunc::WriteAliasDefault);
    SetCopyFunction(&TFunc::CopyAliasDefault);
    SetSkipFunction(&TFunc::SkipAliasDefault);
    SetFunctions(&GetDataPointer, &SetDataPointer);
}


bool CAliasTypeInfo::IsDefault(TConstObjectPtr object) const
{
    return GetPointedType()->IsDefault(object);
}


bool CAliasTypeInfo::Equals(TConstObjectPtr object1,
                            TConstObjectPtr object2) const
{
    return GetPointedType()->Equals(object1, object2);
}


void CAliasTypeInfo::SetDefault(TObjectPtr dst) const
{
    GetPointedType()->SetDefault(dst);
}


void CAliasTypeInfo::Assign(TObjectPtr dst, TConstObjectPtr src) const
{
    GetPointedType()->Assign(dst, src);
}


void CAliasTypeInfo::Delete(TObjectPtr object) const
{
    GetPointedType()->Delete(object);
}


void CAliasTypeInfo::DeleteExternalObjects(TObjectPtr object) const
{
    GetPointedType()->DeleteExternalObjects(object);
}


const CObject* CAliasTypeInfo::GetCObjectPtr(TConstObjectPtr objectPtr) const
{
    return GetPointedType()->GetCObjectPtr(objectPtr);
}


TTypeInfo CAliasTypeInfo::GetRealTypeInfo(TConstObjectPtr object) const
{
    return GetPointedType()->GetRealTypeInfo(object);
}


bool CAliasTypeInfo::IsParentClassOf(const CClassTypeInfo* classInfo) const
{
    return GetPointedType()->IsParentClassOf(classInfo);
}


bool CAliasTypeInfo::IsType(TTypeInfo type) const
{
    return GetPointedType()->IsType(type);
}


void CAliasTypeInfo::SetDataOffset(TPointerOffsetType offset)
{
    m_DataOffset = offset;
}


TObjectPtr CAliasTypeInfo::GetDataPtr(TObjectPtr objectPtr) const
{
    return static_cast<char*>(objectPtr) + m_DataOffset;
}


TConstObjectPtr CAliasTypeInfo::GetDataPtr(TConstObjectPtr objectPtr) const
{
    return static_cast<const char*>(objectPtr) + m_DataOffset;
}


void CAliasTypeInfoFunctions::ReadAliasDefault(CObjectIStream& in,
                                               TTypeInfo objectType,
                                               TObjectPtr objectPtr)
{
    const CAliasTypeInfo* aliasType =
        CTypeConverter<CAliasTypeInfo>::SafeCast(objectType);
    in.ReadAlias(aliasType, objectPtr);
}

void CAliasTypeInfoFunctions::WriteAliasDefault(CObjectOStream& out,
                                                TTypeInfo objectType,
                                                TConstObjectPtr objectPtr)
{
    const CAliasTypeInfo* aliasType =
        CTypeConverter<CAliasTypeInfo>::SafeCast(objectType);
    out.WriteAlias(aliasType, objectPtr);
}

void CAliasTypeInfoFunctions::CopyAliasDefault(CObjectStreamCopier& copier,
                                               TTypeInfo objectType)
{
    const CAliasTypeInfo* aliasType =
        CTypeConverter<CAliasTypeInfo>::SafeCast(objectType);
    aliasType->GetPointedType()->DefaultCopyData(copier);
}

void CAliasTypeInfoFunctions::SkipAliasDefault(CObjectIStream& in,
                                               TTypeInfo objectType)
{
    const CAliasTypeInfo* aliasType =
        CTypeConverter<CAliasTypeInfo>::SafeCast(objectType);
    in.SkipAlias(aliasType);
}


END_NCBI_SCOPE
