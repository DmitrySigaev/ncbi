#ifndef CLASSINFOB__HPP
#define CLASSINFOB__HPP

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
* Revision 1.6  2000/09/18 20:00:00  vasilche
* Separated CVariantInfo and CMemberInfo.
* Implemented copy hooks.
* All hooks now are stored in CTypeInfo/CMemberInfo/CVariantInfo.
* Most type specific functions now are implemented via function pointers instead of virtual functions.
*
* Revision 1.5  2000/09/01 13:15:58  vasilche
* Implemented class/container/choice iterators.
* Implemented CObjectStreamCopier for copying data without loading into memory.
*
* Revision 1.4  2000/08/15 19:44:38  vasilche
* Added Read/Write hooks:
* CReadObjectHook/CWriteObjectHook for objects of specified type.
* CReadClassMemberHook/CWriteClassMemberHook for specified members.
* CReadChoiceVariantHook/CWriteChoiceVariant for specified choice variants.
* CReadContainerElementHook/CWriteContainerElementsHook for containers.
*
* Revision 1.3  2000/07/11 20:34:51  vasilche
* File included in all generated headers made lighter.
* Nonnecessary code moved to serialimpl.hpp.
*
* Revision 1.2  2000/07/03 18:42:33  vasilche
* Added interface to typeinfo via CObjectInfo and CConstObjectInfo.
* Reduced header dependency.
*
* Revision 1.1  2000/06/16 16:31:04  vasilche
* Changed implementation of choices and classes info to allow use of the same classes in generated and user written classes.
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <corelib/ncbiobj.hpp>
#include <serial/stdtypeinfo.hpp>
#include <serial/typeref.hpp>
#include <serial/typeinfo.hpp>
#include <serial/memberlist.hpp>
#include <serial/memberid.hpp>
#include <map>
#include <set>
#include <vector>
#include <memory>

BEGIN_NCBI_SCOPE

class CClassTypeInfoBase : public CTypeInfo {
    typedef CTypeInfo CParent;
public:
    typedef vector<pair<CMemberId, CTypeRef> > TSubClasses;
    typedef map<TTypeInfo, bool> TContainedTypes;

protected:
    CClassTypeInfoBase(ETypeFamily typeFamily, size_t size, const char* name,
                       const void* nonCObject, TTypeCreate createFunc,
                       const type_info& ti);
    CClassTypeInfoBase(ETypeFamily typeFamily, size_t size, const char* name,
                       const CObject* cObject, TTypeCreate createFunc,
                       const type_info& ti);
    CClassTypeInfoBase(ETypeFamily typeFamily, size_t size, const string& name,
                       const void* nonCObject, TTypeCreate createFunc,
                       const type_info& ti);
    CClassTypeInfoBase(ETypeFamily typeFamily, size_t size, const string& name,
                       const CObject* cObject, TTypeCreate createFunc,
                       const type_info& ti);
    
public:
    virtual ~CClassTypeInfoBase(void);

    const CMembersInfo& GetItems(void) const;

    const type_info& GetId(void) const;

    // PostRead/PreWrite
    typedef void (*TPostReadFunction)(TTypeInfo info, TObjectPtr object);
    typedef void (*TPreWriteFunction)(TTypeInfo info, TConstObjectPtr object);

    void SetPostReadFunction(TPostReadFunction func);
    void SetPreWriteFunction(TPreWriteFunction func);

public:
    // finds type info (throws runtime_error if absent)
    static TTypeInfo GetClassInfoByName(const string& name);
    static TTypeInfo GetClassInfoById(const type_info& id);
    static TTypeInfo GetClassInfoBy(const type_info& id,
                                    void (*creator)(void));

    bool IsCObject(void) const;
    const CObject* GetCObjectPtr(TConstObjectPtr objectPtr) const;

    // iterators interface
    virtual bool MayContainType(TTypeInfo type) const;

    // helping member iterator class (internal use)
    class CIterator
    {
    public:
        CIterator(const CClassTypeInfoBase* type);
        CIterator(const CClassTypeInfoBase* type, TMemberIndex index);
        
        CIterator& operator=(TMemberIndex index);

        bool Valid(void) const;

        operator bool(void) const;
        bool operator!(void) const;

        void Next(void);
        void operator++(void);

        TMemberIndex GetIndex(void) const;
        TMemberIndex operator*(void) const;

    private:
        TMemberIndex m_CurrentIndex;
        TMemberIndex m_LastIndex;
    };

protected:
    friend class CIterator;
    CMembersInfo& GetItems(void);

    virtual bool CalcMayContainType(TTypeInfo typeInfo) const;

private:
    const type_info* m_Id;
    bool m_IsCObject;

    CMembersInfo m_Items;

    mutable auto_ptr<TContainedTypes> m_ContainedTypes;

    // class mapping
    typedef set<CClassTypeInfoBase*> TClasses;
    typedef map<const type_info*, const CClassTypeInfoBase*,
        CLessTypeInfo> TClassesById;
    typedef map<string, const CClassTypeInfoBase*> TClassesByName;

    static TClasses* sm_Classes;
    static TClassesById* sm_ClassesById;
    static TClassesByName* sm_ClassesByName;

    void InitClassTypeInfoBase(const type_info& id);
    void Register(void);
    void Deregister(void);
    static TClasses& Classes(void);
    static TClassesById& ClassesById(void);
    static TClassesByName& ClassesByName(void);
};

#include <serial/classinfob.inl>

END_NCBI_SCOPE

#endif  /* CLASSINFOB__HPP */
