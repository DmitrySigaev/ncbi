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
* Revision 1.2  2000/09/18 20:00:23  vasilche
* Separated CVariantInfo and CMemberInfo.
* Implemented copy hooks.
* All hooks now are stored in CTypeInfo/CMemberInfo/CVariantInfo.
* Most type specific functions now are implemented via function pointers instead of virtual functions.
*
* Revision 1.1  2000/08/15 19:44:48  vasilche
* Added Read/Write hooks:
* CReadObjectHook/CWriteObjectHook for objects of specified type.
* CReadClassMemberHook/CWriteClassMemberHook for specified members.
* CReadChoiceVariantHook/CWriteChoiceVariant for specified choice variants.
* CReadContainerElementHook/CWriteContainerElementsHook for containers.
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <serial/objhook.hpp>
#include <serial/object.hpp>
#include <serial/objistr.hpp>
#include <serial/member.hpp>
#include <serial/memberid.hpp>

BEGIN_NCBI_SCOPE

CReadObjectHook::~CReadObjectHook(void)
{
}

CReadClassMemberHook::~CReadClassMemberHook(void)
{
}

void CReadClassMemberHook::ReadMissingClassMember(CObjectIStream& in,
                                                  const CObjectInfo& object,
                                                  TMemberIndex index)
{
    const CMemberInfo* memberInfo =
        object.GetClassTypeInfo()->GetMemberInfo(index);
    memberInfo->DefaultReadMissingMember(in, object.GetObjectPtr());
}

CReadChoiceVariantHook::~CReadChoiceVariantHook(void)
{
}

CReadContainerElementHook::~CReadContainerElementHook(void)
{
}

CWriteObjectHook::~CWriteObjectHook(void)
{
}

CWriteClassMembersHook::~CWriteClassMembersHook(void)
{
}

CWriteClassMemberHook::~CWriteClassMemberHook(void)
{
}

CWriteChoiceVariantHook::~CWriteChoiceVariantHook(void)
{
}

CWriteContainerElementsHook::~CWriteContainerElementsHook(void)
{
}

CCopyObjectHook::~CCopyObjectHook(void)
{
}

CCopyClassMemberHook::~CCopyClassMemberHook(void)
{
}

void CCopyClassMemberHook::CopyMissingClassMember(CObjectStreamCopier& copier,
                                                  const CObjectTypeInfo& type,
                                                  TMemberIndex index)
{
    const CMemberInfo* memberInfo =
        type.GetClassTypeInfo()->GetMemberInfo(index);
    memberInfo->DefaultCopyMissingMember(copier);
}

CCopyChoiceVariantHook::~CCopyChoiceVariantHook(void)
{
}

END_NCBI_SCOPE
