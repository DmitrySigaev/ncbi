#ifndef OBJHOOK__HPP
#define OBJHOOK__HPP

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
* Revision 1.9  2002/09/17 22:26:16  grichenk
* Type<> -> CType<>
*
* Revision 1.8  2002/09/09 18:44:53  grichenk
* Fixed CObjectHookGuard methods for GCC
*
* Revision 1.7  2002/09/09 18:20:18  grichenk
* Fixed includes (to declare Type<>)
*
* Revision 1.6  2002/09/09 18:13:59  grichenk
* Added CObjectHookGuard class.
* Added methods to be used by hooks for data
* reading and skipping.
*
* Revision 1.5  2000/11/01 20:35:27  vasilche
* Removed ECanDelete enum and related constructors.
*
* Revision 1.4  2000/10/03 17:22:34  vasilche
* Reduced header dependency.
* Reduced size of debug libraries on WorkShop by 3 times.
* Fixed tag allocation for parent classes.
* Fixed CObject allocation/deallocation in streams.
* Moved instantiation of several templates in separate source file.
*
* Revision 1.3  2000/09/26 17:38:07  vasilche
* Fixed incomplete choiceptr implementation.
* Removed temporary comments.
*
* Revision 1.2  2000/09/18 20:00:04  vasilche
* Separated CVariantInfo and CMemberInfo.
* Implemented copy hooks.
* All hooks now are stored in CTypeInfo/CMemberInfo/CVariantInfo.
* Most type specific functions now are implemented via function pointers instead of virtual functions.
*
* Revision 1.1  2000/08/15 19:44:39  vasilche
* Added Read/Write hooks:
* CReadObjectHook/CWriteObjectHook for objects of specified type.
* CReadClassMemberHook/CWriteClassMemberHook for specified members.
* CReadChoiceVariantHook/CWriteChoiceVariant for specified choice variants.
* CReadContainerElementHook/CWriteContainerElementsHook for containers.
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <corelib/ncbiobj.hpp>
#include <serial/serialdef.hpp>
#include <serial/objecttype.hpp>
#include <serial/objstack.hpp>

BEGIN_NCBI_SCOPE

class CObjectIStream;
class CObjectOStream;
class CObjectStreamCopier;
class CObjectInfo;
class CConstObjectInfo;
class CObjectTypeInfo;
class CObjectInfoMI;
class CConstObjectInfoMI;
class CObjectTypeInfoMI;
class CObjectInfoCV;
class CConstObjectInfoCV;
class CObjectTypeInfoCV;

class CReadObjectHook : public CObject
{
public:
    virtual ~CReadObjectHook(void);
    
    virtual void ReadObject(CObjectIStream& in,
                            const CObjectInfo& object) = 0;
};

class CReadClassMemberHook : public CObject
{
public:
    virtual ~CReadClassMemberHook(void);

    virtual void ReadClassMember(CObjectIStream& in,
                                 const CObjectInfoMI& member) = 0;
    virtual void ReadMissingClassMember(CObjectIStream& in,
                                        const CObjectInfoMI& member);
};

class CReadChoiceVariantHook : public CObject
{
public:
    virtual ~CReadChoiceVariantHook(void);

    virtual void ReadChoiceVariant(CObjectIStream& in,
                                   const CObjectInfoCV& variant) = 0;
};

class CReadContainerElementHook : public CObject
{
public:
    virtual ~CReadContainerElementHook(void);

    virtual void ReadContainerElement(CObjectIStream& in,
                                      const CObjectInfo& container) = 0;
};

class CWriteObjectHook : public CObject
{
public:
    virtual ~CWriteObjectHook(void);
    
    virtual void WriteObject(CObjectOStream& out,
                             const CConstObjectInfo& object) = 0;
};

class CWriteClassMemberHook : public CObject
{
public:
    virtual ~CWriteClassMemberHook(void);
    
    virtual void WriteClassMember(CObjectOStream& out,
                                  const CConstObjectInfoMI& member) = 0;
};

class CWriteChoiceVariantHook : public CObject
{
public:
    virtual ~CWriteChoiceVariantHook(void);

    virtual void WriteChoiceVariant(CObjectOStream& in,
                                    const CConstObjectInfoCV& variant) = 0;
};

class CCopyObjectHook : public CObject
{
public:
    virtual ~CCopyObjectHook(void);
    
    virtual void CopyObject(CObjectStreamCopier& copier,
                            const CObjectTypeInfo& type) = 0;
};

class CCopyClassMemberHook : public CObject
{
public:
    virtual ~CCopyClassMemberHook(void);
    
    virtual void CopyClassMember(CObjectStreamCopier& copier,
                                 const CObjectTypeInfoMI& member) = 0;
    virtual void CopyMissingClassMember(CObjectStreamCopier& copier,
                                        const CObjectTypeInfoMI& member);
};

class CCopyChoiceVariantHook : public CObject
{
public:
    virtual ~CCopyChoiceVariantHook(void);

    virtual void CopyChoiceVariant(CObjectStreamCopier& copier,
                                   const CObjectTypeInfoCV& variant) = 0;
};


enum EDefaultHookAction {
    eDefault_Normal,        // read or write data
    eDefault_Skip           // skip data
};


template <class T>
class CObjectHookGuard
{
public:
    // object read hook
    CObjectHookGuard(CReadObjectHook& hook, CObjectIStream* in = 0);
    // object write hook
    CObjectHookGuard(CWriteObjectHook& hook, CObjectOStream* out = 0);

    // member read hook
    CObjectHookGuard(string id, CReadClassMemberHook& hook, CObjectIStream* in = 0);
    // member write hook
    CObjectHookGuard(string id, CWriteClassMemberHook& hook, CObjectOStream* out = 0);

    // choice variant read hook
    CObjectHookGuard(string id, CReadChoiceVariantHook& hook, CObjectIStream* in = 0);
    // choice variant write hook
    CObjectHookGuard(string id, CWriteChoiceVariantHook& hook, CObjectOStream* out = 0);

    // container element read hook
    // CObjectHookGuard(CReadContainerElementHook& hook, CObjectIStream* in = 0);

    // container element write hook
    // CObjectHookGuard(CWriteContainerElementHook& hook, CObjectOStream* out = 0);

    ~CObjectHookGuard(void);

private:
    enum EHookMode {
        eHook_Read,
        eHook_Write,
        eHook_Copy
    };
    enum EHookType {
        eHook_Object,       // object hook
        eHook_Member,       // class member hook
        eHook_Variant,      // choice variant hook
        eHook_Element       // container element hook
    };

    CObjectStack* m_Stream;
    CRef<CObject> m_Hook;
    EHookMode m_HookMode;
    EHookType m_HookType;
    string m_Id;            // member or variant id
};


template <class T>
CObjectHookGuard<T>::CObjectHookGuard(CReadObjectHook& hook,
                                      CObjectIStream* in)
    : m_Stream(in),
      m_Hook(&hook),
      m_HookMode(eHook_Read),
      m_HookType(eHook_Object)
{
    CType<T> type;
    if ( m_Stream ) {
        CObjectTypeInfo(type).SetLocalReadHook(*in, &hook);
    }
    else {
        CObjectTypeInfo(type).SetGlobalReadHook(&hook);
    }
}

template <class T>
CObjectHookGuard<T>::CObjectHookGuard(CWriteObjectHook& hook,
                                      CObjectOStream* out)
    : m_Stream(out),
      m_Hook(&hook),
      m_HookMode(eHook_Write),
      m_HookType(eHook_Object)
{
    CType<T> type;
    if ( m_Stream ) {
        CObjectTypeInfo(type).SetLocalWriteHook(*out, &hook);
    }
    else {
        CObjectTypeInfo(type).SetGlobalWriteHook(&hook);
    }
}

template <class T>
CObjectHookGuard<T>::CObjectHookGuard(string id,
                                      CReadClassMemberHook& hook,
                                      CObjectIStream* in)
    : m_Stream(in),
      m_Hook(&hook),
      m_HookMode(eHook_Read),
      m_HookType(eHook_Member),
      m_Id(id)
{
    CType<T> type;
    if ( m_Stream ) {
        CObjectTypeInfo(type).FindMember(m_Id).SetLocalReadHook(*in, &hook);
    }
    else {
        CObjectTypeInfo(type).FindMember(m_Id).SetGlobalReadHook(&hook);
    }
}

template <class T>
CObjectHookGuard<T>::CObjectHookGuard(string id,
                                      CWriteClassMemberHook& hook,
                                      CObjectOStream* out)
    : m_Stream(out),
      m_Hook(&hook),
      m_HookMode(eHook_Write),
      m_HookType(eHook_Member),
      m_Id(id)
{
    CType<T> type;
    if ( m_Stream ) {
        CObjectTypeInfo(type).FindMember(m_Id).SetLocalWriteHook(*out, &hook);
    }
    else {
        CObjectTypeInfo(type).FindMember(m_Id).SetGlobalWriteHook(&hook);
    }
}

template <class T>
CObjectHookGuard<T>::CObjectHookGuard(string id,
                                      CReadChoiceVariantHook& hook,
                                      CObjectIStream* in)
    : m_Stream(in),
      m_Hook(&hook),
      m_HookMode(eHook_Read),
      m_HookType(eHook_Variant),
      m_Id(id)
{
    CType<T> type;
    if ( m_Stream ) {
        CObjectTypeInfo(type).FindVariant(m_Id).SetLocalReadHook(*in, &hook);
    }
    else {
        CObjectTypeInfo(type).FindVariant(m_Id).SetGlobalReadHook(&hook);
    }
}

template <class T>
CObjectHookGuard<T>::CObjectHookGuard(string id,
                                      CWriteChoiceVariantHook& hook,
                                      CObjectOStream* out)
    : m_Stream(out),
      m_Hook(&hook),
      m_HookMode(eHook_Write),
      m_HookType(eHook_Variant),
      m_Id(id)
{
    CType<T> type;
    if ( m_Stream ) {
        CObjectTypeInfo(type).FindVariant(m_Id).SetLocalWriteHook(*out, &hook);
    }
    else {
        CObjectTypeInfo(type).FindVariant(m_Id).SetGlobalWriteHook(&hook);
    }
}

/*
template <class T>
CObjectHookGuard<T>::CObjectHookGuard(CReadContainerElementHook& hook)
    : m_Stream(0),
      m_Hook(&hook),
      m_HookMode(eHook_Read),
      m_HookType(eHook_Element),
{
    CType<T> type;
    if (m_Stream) {
        CObjectTypeInfo(type).GetElementType().GetContainerTypeInfo()->SetLocalReadHook(*in, hook);
    }
    else {
        CObjectTypeInfo(type).GetElementType().SetGlobalReadHook(hook);
    }
}
*/

template <class T>
CObjectHookGuard<T>::~CObjectHookGuard(void)
{
    CType<T> type;
    switch (m_HookType) {
    case eHook_Object:
        switch (m_HookMode) {
        case eHook_Read:
            if ( m_Stream ) {
                CObjectTypeInfo(type).ResetLocalReadHook(*static_cast<CObjectIStream*>(m_Stream));
            }
            else {
                CObjectTypeInfo(type).ResetGlobalReadHook();
            }
            break;
        case eHook_Write:
            if ( m_Stream ) {
                CObjectTypeInfo(type).ResetLocalWriteHook(*static_cast<CObjectOStream*>(m_Stream));
            }
            else {
                CObjectTypeInfo(type).ResetGlobalWriteHook();
            }
            break;
        case eHook_Copy:
            break;
        }
        break;
    case eHook_Member:
        switch (m_HookMode) {
        case eHook_Read:
            if ( m_Stream ) {
                CObjectTypeInfo(type).FindMember(m_Id).ResetLocalReadHook
                    (*static_cast<CObjectIStream*>(m_Stream));
            }
            else {
                CObjectTypeInfo(type).FindMember(m_Id).ResetGlobalReadHook();
            }
            break;
        case eHook_Write:
            if ( m_Stream ) {
                CObjectTypeInfo(type).FindMember(m_Id).ResetLocalWriteHook
                    (*static_cast<CObjectOStream*>(m_Stream));
            }
            else {
                CObjectTypeInfo(type).FindMember(m_Id).ResetGlobalWriteHook();
            }
            break;
        case eHook_Copy:
            break;
        }
        break;
    case eHook_Variant:
        switch (m_HookMode) {
        case eHook_Read:
            if ( m_Stream ) {
                CObjectTypeInfo(type).FindVariant(m_Id).ResetLocalReadHook
                    (*static_cast<CObjectIStream*>(m_Stream));
            }
            else {
                CObjectTypeInfo(type).FindVariant(m_Id).ResetGlobalReadHook();
            }
            break;
        case eHook_Write:
            if ( m_Stream ) {
                CObjectTypeInfo(type).FindVariant(m_Id).ResetLocalWriteHook
                    (*static_cast<CObjectOStream*>(m_Stream));
            }
            else {
                CObjectTypeInfo(type).FindVariant(m_Id).ResetGlobalWriteHook();
            }
            break;
        case eHook_Copy:
            break;
        }
        break;
    case eHook_Element:
        break;
    }
}



//#include <serial/objhook.inl>

END_NCBI_SCOPE

#endif  /* OBJHOOK__HPP */
