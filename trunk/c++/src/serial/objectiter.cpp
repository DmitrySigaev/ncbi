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
* Revision 1.8  2003/08/11 15:25:52  grichenk
* Added possibility to reset an object member from
* a read hook (including non-optional members).
*
* Revision 1.7  2003/07/29 18:47:47  vasilche
* Fixed thread safeness of object stream hooks.
*
* Revision 1.6  2003/07/17 22:49:48  vasilche
* Added missing methods.
*
* Revision 1.5  2003/07/17 20:02:51  vasilche
* Added update of 'set' flag for non const class member dereference.
* Added choice switch for non const choice variant dereference.
*
* Revision 1.4  2003/04/29 18:30:37  gouriano
* object data member initialization verification
*
* Revision 1.3  2003/03/10 18:54:26  gouriano
* use new structured exceptions (based on CException)
*
* Revision 1.2  2001/05/17 15:07:08  lavr
* Typos corrected
*
* Revision 1.1  2000/10/20 15:51:40  vasilche
* Fixed data error processing.
* Added interface for constructing container objects directly into output stream.
* object.hpp, object.inl and object.cpp were split to
* objectinfo.*, objecttype.*, objectiter.* and objectio.*.
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <serial/exception.hpp>
#include <serial/objectiter.hpp>
#include <serial/delaybuf.hpp>

BEGIN_NCBI_SCOPE

void CConstObjectInfoEI::ReportNonValid(void) const
{
    ERR_POST("CElementIterator was used without checking its validity");
}

void CObjectInfoEI::ReportNonValid(void) const
{
    ERR_POST("CElementIterator was used without checking its validity");
}

void CObjectTypeInfoII::ReportNonValid(void) const
{
    ERR_POST("CTypeMemberIterator is used without validity check");
}

// container iterators

CConstObjectInfoEI::CConstObjectInfoEI(const CConstObjectInfo& object)
    : m_Iterator(object.GetObjectPtr(), object.GetContainerTypeInfo())
{
    _DEBUG_ARG(m_LastCall = eNone);
}

CConstObjectInfoEI& CConstObjectInfoEI::operator=(const CConstObjectInfo& object)
{
    m_Iterator.Init(object.GetObjectPtr(), object.GetContainerTypeInfo());
    _DEBUG_ARG(m_LastCall = eNone);
    return *this;
}

CObjectInfoEI::CObjectInfoEI(const CObjectInfo& object)
    : m_Iterator(object.GetObjectPtr(), object.GetContainerTypeInfo())
{
    _DEBUG_ARG(m_LastCall = eNone);
}

CObjectInfoEI& CObjectInfoEI::operator=(const CObjectInfo& object)
{
    m_Iterator.Init(object.GetObjectPtr(), object.GetContainerTypeInfo());
    _DEBUG_ARG(m_LastCall = eNone);
    return *this;
}

// class iterators

bool CObjectTypeInfoMI::IsSet(const CConstObjectInfo& object) const
{
    const CMemberInfo* memberInfo = GetMemberInfo();
    if ( memberInfo->HaveSetFlag() )
        return memberInfo->GetSetFlag(object.GetObjectPtr()) != CMemberInfo::eSetNo;
    
    if ( memberInfo->CanBeDelayed() &&
         memberInfo->GetDelayBuffer(object.GetObjectPtr()).Delayed() )
        return true;

    if ( memberInfo->Optional() ) {
        TConstObjectPtr defaultPtr = memberInfo->GetDefault();
        TConstObjectPtr memberPtr =
            memberInfo->GetMemberPtr(object.GetObjectPtr());
        TTypeInfo memberType = memberInfo->GetTypeInfo();
        if ( !defaultPtr ) {
            if ( memberType->IsDefault(memberPtr) )
                return false; // DEFAULT
        }
        else {
            if ( memberType->Equals(memberPtr, defaultPtr) )
                return false; // OPTIONAL
        }
    }
    return true;
}

void CObjectTypeInfoMI::SetLocalReadHook(CObjectIStream& stream,
                                         CReadClassMemberHook* hook) const
{
    GetNCMemberInfo()->SetLocalReadHook(stream, hook);
}

void CObjectTypeInfoMI::SetGlobalReadHook(CReadClassMemberHook* hook) const
{
    GetNCMemberInfo()->SetGlobalReadHook(hook);
}

void CObjectTypeInfoMI::ResetLocalReadHook(CObjectIStream& stream) const
{
    GetNCMemberInfo()->ResetLocalReadHook(stream);
}

void CObjectTypeInfoMI::ResetGlobalReadHook(void) const
{
    GetNCMemberInfo()->ResetGlobalReadHook();
}

void CObjectTypeInfoMI::SetLocalWriteHook(CObjectOStream& stream,
                                          CWriteClassMemberHook* hook) const
{
    GetNCMemberInfo()->SetLocalWriteHook(stream, hook);
}

void CObjectTypeInfoMI::SetGlobalWriteHook(CWriteClassMemberHook* hook) const
{
    GetNCMemberInfo()->SetGlobalWriteHook(hook);
}

void CObjectTypeInfoMI::ResetLocalWriteHook(CObjectOStream& stream) const
{
    GetNCMemberInfo()->ResetLocalWriteHook(stream);
}

void CObjectTypeInfoMI::ResetGlobalWriteHook(void) const
{
    GetNCMemberInfo()->ResetGlobalWriteHook();
}

void CObjectTypeInfoMI::SetLocalSkipHook(CObjectIStream& stream,
                                         CSkipClassMemberHook* hook) const
{
    GetNCMemberInfo()->SetLocalSkipHook(stream, hook);
}

void CObjectTypeInfoMI::SetGlobalSkipHook(CSkipClassMemberHook* hook) const
{
    GetNCMemberInfo()->SetGlobalSkipHook(hook);
}

void CObjectTypeInfoMI::ResetLocalSkipHook(CObjectIStream& stream) const
{
    GetNCMemberInfo()->ResetLocalSkipHook(stream);
}

void CObjectTypeInfoMI::ResetGlobalSkipHook(void) const
{
    GetNCMemberInfo()->ResetGlobalSkipHook();
}

void CObjectTypeInfoMI::SetLocalCopyHook(CObjectStreamCopier& stream,
                                         CCopyClassMemberHook* hook) const
{
    GetNCMemberInfo()->SetLocalCopyHook(stream, hook);
}

void CObjectTypeInfoMI::SetGlobalCopyHook(CCopyClassMemberHook* hook) const
{
    GetNCMemberInfo()->SetGlobalCopyHook(hook);
}

void CObjectTypeInfoMI::ResetLocalCopyHook(CObjectStreamCopier& stream) const
{
    GetNCMemberInfo()->ResetLocalCopyHook(stream);
}

void CObjectTypeInfoMI::ResetGlobalCopyHook(void) const
{
    GetNCMemberInfo()->ResetGlobalCopyHook();
}

pair<TConstObjectPtr, TTypeInfo> CConstObjectInfoMI::GetMemberPair(void) const
{
    const CMemberInfo* memberInfo = GetMemberInfo();
    return make_pair(memberInfo->GetMemberPtr(m_Object.GetObjectPtr()),
                     memberInfo->GetTypeInfo());
}

pair<TObjectPtr, TTypeInfo> CObjectInfoMI::GetMemberPair(void) const
{
    TObjectPtr objectPtr = m_Object.GetObjectPtr();
    const CMemberInfo* memberInfo = GetMemberInfo();
    memberInfo->UpdateSetFlag(objectPtr, CMemberInfo::eSetYes);
    return make_pair(memberInfo->GetMemberPtr(objectPtr),
                     memberInfo->GetTypeInfo());
}

void CObjectInfoMI::Erase(EEraseFlag flag)
{
    const CMemberInfo* mInfo = GetMemberInfo();
    if ( !(mInfo->Optional() || flag == eErase_Mandatory)
        || mInfo->GetDefault() )
        NCBI_THROW(CSerialException,eIllegalCall, "cannot reset non OPTIONAL member");
    
    TObjectPtr objectPtr = m_Object.GetObjectPtr();
    // check 'set' flag
    bool haveSetFlag = mInfo->HaveSetFlag();
    if ( haveSetFlag && mInfo->GetSetFlag(objectPtr) == CMemberInfo::eSetNo ) {
        // member not set
        return;
    }

    // reset member
    mInfo->GetTypeInfo()->SetDefault(mInfo->GetMemberPtr(objectPtr));

    // update 'set' flag
    if ( haveSetFlag )
        mInfo->UpdateSetFlag(objectPtr,CMemberInfo::eSetNo);
}

// choice iterators

void CObjectTypeInfoVI::SetLocalReadHook(CObjectIStream& stream,
                                         CReadChoiceVariantHook* hook) const
{
    GetNCVariantInfo()->SetLocalReadHook(stream, hook);
}

void CObjectTypeInfoVI::SetGlobalReadHook(CReadChoiceVariantHook* hook) const
{
    GetNCVariantInfo()->SetGlobalReadHook(hook);
}

void CObjectTypeInfoVI::ResetLocalReadHook(CObjectIStream& stream) const
{
    GetNCVariantInfo()->ResetLocalReadHook(stream);
}

void CObjectTypeInfoVI::ResetGlobalReadHook(void) const
{
    GetNCVariantInfo()->ResetGlobalReadHook();
}

void CObjectTypeInfoVI::SetLocalWriteHook(CObjectOStream& stream,
                                          CWriteChoiceVariantHook* hook) const
{
    GetNCVariantInfo()->SetLocalWriteHook(stream, hook);
}

void CObjectTypeInfoVI::SetGlobalWriteHook(CWriteChoiceVariantHook* hook) const
{
    GetNCVariantInfo()->SetGlobalWriteHook(hook);
}

void CObjectTypeInfoVI::ResetLocalWriteHook(CObjectOStream& stream) const
{
    GetNCVariantInfo()->ResetLocalWriteHook(stream);
}

void CObjectTypeInfoVI::ResetGlobalWriteHook(void) const
{
    GetNCVariantInfo()->ResetGlobalWriteHook();
}

void CObjectTypeInfoVI::SetLocalSkipHook(CObjectIStream& stream,
                                         CSkipChoiceVariantHook* hook) const
{
    GetNCVariantInfo()->SetLocalSkipHook(stream, hook);
}

void CObjectTypeInfoVI::SetGlobalSkipHook(CSkipChoiceVariantHook* hook) const
{
    GetNCVariantInfo()->SetGlobalSkipHook(hook);
}

void CObjectTypeInfoVI::ResetLocalSkipHook(CObjectIStream& stream) const
{
    GetNCVariantInfo()->ResetLocalSkipHook(stream);
}

void CObjectTypeInfoVI::ResetGlobalSkipHook(void) const
{
    GetNCVariantInfo()->ResetGlobalSkipHook();
}

void CObjectTypeInfoVI::SetLocalCopyHook(CObjectStreamCopier& stream,
                                         CCopyChoiceVariantHook* hook) const
{
    GetNCVariantInfo()->SetLocalCopyHook(stream, hook);
}

void CObjectTypeInfoVI::SetGlobalCopyHook(CCopyChoiceVariantHook* hook) const
{
    GetNCVariantInfo()->SetGlobalCopyHook(hook);
}

void CObjectTypeInfoVI::ResetLocalCopyHook(CObjectStreamCopier& stream) const
{
    GetNCVariantInfo()->ResetLocalCopyHook(stream);
}

void CObjectTypeInfoVI::ResetGlobalCopyHook(void) const
{
    GetNCVariantInfo()->ResetGlobalCopyHook();
}

void CObjectTypeInfoCV::SetLocalReadHook(CObjectIStream& stream,
                                         CReadChoiceVariantHook* hook) const
{
    GetNCVariantInfo()->SetLocalReadHook(stream, hook);
}

void CObjectTypeInfoCV::SetGlobalReadHook(CReadChoiceVariantHook* hook) const
{
    GetNCVariantInfo()->SetGlobalReadHook(hook);
}

void CObjectTypeInfoCV::ResetLocalReadHook(CObjectIStream& stream) const
{
    GetNCVariantInfo()->ResetLocalReadHook(stream);
}

void CObjectTypeInfoCV::ResetGlobalReadHook(void) const
{
    GetNCVariantInfo()->ResetGlobalReadHook();
}

void CObjectTypeInfoCV::SetLocalWriteHook(CObjectOStream& stream,
                                          CWriteChoiceVariantHook* hook) const
{
    GetNCVariantInfo()->SetLocalWriteHook(stream, hook);
}

void CObjectTypeInfoCV::SetGlobalWriteHook(CWriteChoiceVariantHook* hook) const
{
    GetNCVariantInfo()->SetGlobalWriteHook(hook);
}

void CObjectTypeInfoCV::ResetLocalWriteHook(CObjectOStream& stream) const
{
    GetNCVariantInfo()->ResetLocalWriteHook(stream);
}

void CObjectTypeInfoCV::ResetGlobalWriteHook(void) const
{
    GetNCVariantInfo()->ResetGlobalWriteHook();
}

void CObjectTypeInfoCV::SetLocalCopyHook(CObjectStreamCopier& stream,
                                         CCopyChoiceVariantHook* hook) const
{
    GetNCVariantInfo()->SetLocalCopyHook(stream, hook);
}

void CObjectTypeInfoCV::SetGlobalCopyHook(CCopyChoiceVariantHook* hook) const
{
    GetNCVariantInfo()->SetGlobalCopyHook(hook);
}

void CObjectTypeInfoCV::ResetLocalCopyHook(CObjectStreamCopier& stream) const
{
    GetNCVariantInfo()->ResetLocalCopyHook(stream);
}

void CObjectTypeInfoCV::ResetGlobalCopyHook(void) const
{
    GetNCVariantInfo()->ResetGlobalCopyHook();
}

void CObjectTypeInfoCV::Init(const CConstObjectInfo& object)
{
    m_ChoiceTypeInfo = object.GetChoiceTypeInfo();
    m_VariantIndex = object.GetCurrentChoiceVariantIndex();
}

pair<TConstObjectPtr, TTypeInfo> CConstObjectInfoCV::GetVariantPair(void) const
{
    const CVariantInfo* variantInfo = GetVariantInfo();
    return make_pair(variantInfo->GetVariantPtr(m_Object.GetObjectPtr()),
                     variantInfo->GetTypeInfo());
}

pair<TObjectPtr, TTypeInfo> CObjectInfoCV::GetVariantPair(void) const
{
    TObjectPtr choicePtr = m_Object.GetObjectPtr();
    const CChoiceTypeInfo* choiceTypeInfo = m_Object.GetChoiceTypeInfo();
    TMemberIndex index = GetVariantIndex();
    choiceTypeInfo->SetIndex(choicePtr, index);
    const CVariantInfo* variantInfo = choiceTypeInfo->GetVariantInfo(index);
    return make_pair(variantInfo->GetVariantPtr(choicePtr),
                     variantInfo->GetTypeInfo());
}

END_NCBI_SCOPE
