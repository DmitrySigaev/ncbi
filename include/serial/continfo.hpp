#ifndef CONTINFO__HPP
#define CONTINFO__HPP

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
*/

#include <corelib/ncbistd.hpp>
#include <corelib/ncbiutil.hpp>
#include <serial/typeinfo.hpp>
#include <serial/typeref.hpp>
#include <memory>


/** @addtogroup TypeInfoCPP
 *
 * @{
 */


BEGIN_NCBI_SCOPE

class CConstContainerElementIterator;
class CContainerElementIterator;

class NCBI_XSERIAL_EXPORT CContainerTypeInfo : public CTypeInfo
{
    typedef CTypeInfo CParent;
public:
    CContainerTypeInfo(size_t size,
                       TTypeInfo elementType, bool randomOrder);
    CContainerTypeInfo(size_t size,
                       const CTypeRef& elementType, bool randomOrder);
    CContainerTypeInfo(size_t size, const char* name,
                       TTypeInfo elementType, bool randomOrder);
    CContainerTypeInfo(size_t size, const char* name,
                       const CTypeRef& elementType, bool randomOrder);
    CContainerTypeInfo(size_t size, const string& name,
                       TTypeInfo elementType, bool randomOrder);
    CContainerTypeInfo(size_t size, const string& name,
                       const CTypeRef& elementType, bool randomOrder);

    TTypeInfo GetElementType(void) const;

    bool RandomElementsOrder(void) const;
    
    virtual bool MayContainType(TTypeInfo type) const;

    void Assign(TObjectPtr dst, TConstObjectPtr src,
                ESerialRecursionMode how = eRecursive) const;
    bool Equals(TConstObjectPtr object1, TConstObjectPtr object2,
                ESerialRecursionMode how = eRecursive) const;

    // iterators methods (private)
    class CConstIterator
    {
    public:
        CConstIterator(void);
        ~CConstIterator(void);

        typedef NCBI_NS_NCBI::TConstObjectPtr TObjectPtr;

        const CContainerTypeInfo* GetContainerType(void) const;
        TObjectPtr GetContainerPtr(void) const;
        void Reset(void);

        const CContainerTypeInfo* m_ContainerType;
        TObjectPtr                m_ContainerPtr;
        void*                     m_IteratorData;
    };
    
    class CIterator
    {
    public:
        CIterator(void);
        ~CIterator(void);

        typedef NCBI_NS_NCBI::TObjectPtr TObjectPtr;

        const CContainerTypeInfo* GetContainerType(void) const;
        TObjectPtr GetContainerPtr(void) const;
        void Reset(void);

        const CContainerTypeInfo* m_ContainerType;
        TObjectPtr                m_ContainerPtr;
        void*                     m_IteratorData;
    };

    bool InitIterator(CConstIterator& it, TConstObjectPtr containerPtr) const;
    void ReleaseIterator(CConstIterator& it) const;
    void CopyIterator(CConstIterator& dst, const CConstIterator& src) const;
    bool NextElement(CConstIterator& it) const;
    TConstObjectPtr GetElementPtr(const CConstIterator& it) const;

    bool InitIterator(CIterator& it, TObjectPtr containerPtr) const;
    void ReleaseIterator(CIterator& it) const;
    void CopyIterator(CIterator& dst, const CIterator& src) const;
    bool NextElement(CIterator& it) const;
    TObjectPtr GetElementPtr(const CIterator& it) const;
    bool EraseElement(CIterator& it) const;
    void EraseAllElements(CIterator& it) const;

    void AddElement(TObjectPtr containerPtr, TConstObjectPtr elementPtr,
                    ESerialRecursionMode how = eRecursive) const;
    void AddElement(TObjectPtr containerPtr, CObjectIStream& in) const;

    typedef bool (*TInitIteratorConst)(CConstIterator&);
    typedef void (*TReleaseIteratorConst)(CConstIterator&);
    typedef void (*TCopyIteratorConst)(CConstIterator&, const CConstIterator&);
    typedef bool (*TNextElementConst)(CConstIterator&);
    typedef TConstObjectPtr (*TGetElementPtrConst)(const CConstIterator&);

    typedef bool (*TInitIterator)(CIterator&);
    typedef void (*TReleaseIterator)(CIterator&);
    typedef void (*TCopyIterator)(CIterator&, const CIterator&);
    typedef bool (*TNextElement)(CIterator&);
    typedef TObjectPtr (*TGetElementPtr)(const CIterator&);
    typedef bool (*TEraseElement)(CIterator&);
    typedef void (*TEraseAllElements)(CIterator&);

    typedef void (*TAddElement)(const CContainerTypeInfo* cType,
                                TObjectPtr cPtr, TConstObjectPtr ePtr,
                                ESerialRecursionMode how);
    typedef void (*TAddElementIn)(const CContainerTypeInfo* cType,
                                  TObjectPtr cPtr, CObjectIStream& in);

    void SetConstIteratorFunctions(TInitIteratorConst, TReleaseIteratorConst,
                                   TCopyIteratorConst, TNextElementConst,
                                   TGetElementPtrConst);
    void SetIteratorFunctions(TInitIterator, TReleaseIterator,
                              TCopyIterator, TNextElement,
                              TGetElementPtr,
                              TEraseElement, TEraseAllElements);
    void SetAddElementFunctions(TAddElement, TAddElementIn);

protected:
    static void ReadContainer(CObjectIStream& in,
                              TTypeInfo objectType,
                              TObjectPtr objectPtr);
    static void WriteContainer(CObjectOStream& out,
                               TTypeInfo objectType,
                               TConstObjectPtr objectPtr);
    static void SkipContainer(CObjectIStream& in,
                              TTypeInfo objectType);
    static void CopyContainer(CObjectStreamCopier& copier,
                              TTypeInfo objectType);

protected:
    CTypeRef m_ElementType;
    bool m_RandomOrder;

private:
    void InitContainerTypeInfoFunctions(void);

    // iterator functions
    TInitIteratorConst m_InitIteratorConst;
    TReleaseIteratorConst m_ReleaseIteratorConst;
    TCopyIteratorConst m_CopyIteratorConst;
    TNextElementConst m_NextElementConst;
    TGetElementPtrConst m_GetElementPtrConst;

    TInitIterator m_InitIterator;
    TReleaseIterator m_ReleaseIterator;
    TCopyIterator m_CopyIterator;
    TNextElement m_NextElement;
    TGetElementPtr m_GetElementPtr;
    TEraseElement m_EraseElement;
    TEraseAllElements m_EraseAllElements;

    TAddElement m_AddElement;
    TAddElementIn m_AddElementIn;
};

class NCBI_XSERIAL_EXPORT CConstContainerElementIterator
{
public:
    typedef CContainerTypeInfo::CConstIterator TIterator;

    CConstContainerElementIterator(void);
    CConstContainerElementIterator(TConstObjectPtr containerPtr,
                                   const CContainerTypeInfo* containerType);
    CConstContainerElementIterator(const CConstContainerElementIterator& src);

    CConstContainerElementIterator&
    operator=(const CConstContainerElementIterator& src);
    
    void Init(TConstObjectPtr containerPtr,
              const CContainerTypeInfo* containerType);

    TTypeInfo GetElementType(void) const;
    
    bool Valid(void) const;
    void Next(void);

    pair<TConstObjectPtr, TTypeInfo> Get(void) const;

private:
    TTypeInfo m_ElementType;
    TIterator m_Iterator;
    bool m_Valid;
};

class NCBI_XSERIAL_EXPORT CContainerElementIterator
{
public:
    typedef CContainerTypeInfo::CIterator TIterator;

    CContainerElementIterator(void);
    CContainerElementIterator(TObjectPtr containerPtr,
                              const CContainerTypeInfo* containerType);
    CContainerElementIterator(const CContainerElementIterator& src);
    CContainerElementIterator& operator=(const CContainerElementIterator& src);
    void Init(TObjectPtr containerPtr,
              const CContainerTypeInfo* containerType);

    TTypeInfo GetElementType(void) const;
    
    bool Valid(void) const;
    void Next(void);
    void Erase(void);
    void EraseAll(void);

    pair<TObjectPtr, TTypeInfo> Get(void) const;

private:
    TTypeInfo m_ElementType;
    TIterator m_Iterator;
    bool m_Valid;
};


/* @} */


#include <serial/continfo.inl>

END_NCBI_SCOPE

#endif  /* CONTINFO__HPP */



/* ---------------------------------------------------------------------------
* $Log$
* Revision 1.10  2004/03/25 15:56:27  gouriano
* Added possibility to copy and compare serial object non-recursively
*
* Revision 1.9  2003/08/14 20:03:57  vasilche
* Avoid memory reallocation when reading over preallocated object.
* Simplified CContainerTypeInfo iterators interface.
*
* Revision 1.8  2003/04/15 14:15:06  siyan
* Added doxygen support
*
* Revision 1.7  2002/12/23 18:38:50  dicuccio
* Added WIn32 export specifier: NCBI_XSERIAL_EXPORT.
* Moved all CVS logs to the end.
*
* Revision 1.6  2000/10/13 16:28:30  vasilche
* Reduced header dependency.
* Avoid use of templates with virtual methods.
* Reduced amount of different maps used.
* All this lead to smaller compiled code size (libraries and programs).
*
* Revision 1.5  2000/09/18 20:00:00  vasilche
* Separated CVariantInfo and CMemberInfo.
* Implemented copy hooks.
* All hooks now are stored in CTypeInfo/CMemberInfo/CVariantInfo.
* Most type specific functions now are implemented via function pointers instead of virtual functions.
*
* Revision 1.4  2000/09/13 15:10:13  vasilche
* Fixed type detection in type iterators.
*
* Revision 1.3  2000/09/01 13:15:58  vasilche
* Implemented class/container/choice iterators.
* Implemented CObjectStreamCopier for copying data without loading into memory.
*
* Revision 1.2  2000/08/15 19:44:38  vasilche
* Added Read/Write hooks:
* CReadObjectHook/CWriteObjectHook for objects of specified type.
* CReadClassMemberHook/CWriteClassMemberHook for specified members.
* CReadChoiceVariantHook/CWriteChoiceVariant for specified choice variants.
* CReadContainerElementHook/CWriteContainerElementsHook for containers.
*
* Revision 1.1  2000/07/03 18:42:33  vasilche
* Added interface to typeinfo via CObjectInfo and CConstObjectInfo.
* Reduced header dependency.
*
* ===========================================================================
*/
