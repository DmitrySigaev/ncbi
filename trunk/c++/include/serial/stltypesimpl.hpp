#ifndef STLTYPESIMPL__HPP
#define STLTYPESIMPL__HPP

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
* Revision 1.1  2000/10/13 16:28:33  vasilche
* Reduced header dependency.
* Avoid use of templates with virtual methods.
* Reduced amount of different maps used.
* All this lead to smaller compiled code size (libraries and programs).
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <serial/serialdef.hpp>
#include <serial/typeref.hpp>
#include <serial/continfo.hpp>
#include <serial/memberid.hpp>

BEGIN_NCBI_SCOPE

class CStlClassInfoUtil
{
public:
    static TTypeInfo Get_auto_ptr(TTypeInfo arg, TTypeInfoGetter1 f);
    static TTypeInfo Get_CRef(TTypeInfo arg, TTypeInfoGetter1 f);
    static TTypeInfo Get_AutoPtr(TTypeInfo arg, TTypeInfoGetter1 f);
    static TTypeInfo Get_list(TTypeInfo arg, TTypeInfoGetter1 f);
    static TTypeInfo GetSet_list(TTypeInfo arg, TTypeInfoGetter1 f);
    static TTypeInfo Get_vector(TTypeInfo arg, TTypeInfoGetter1 f);
    static TTypeInfo Get_set(TTypeInfo arg, TTypeInfoGetter1 f);
    static TTypeInfo Get_multiset(TTypeInfo arg, TTypeInfoGetter1 f);
    static TTypeInfo Get_map(TTypeInfo arg1, TTypeInfo arg2,
                             TTypeInfoGetter2 f);
    static TTypeInfo Get_multimap(TTypeInfo arg1, TTypeInfo arg2,
                                  TTypeInfoGetter2 f);

    // throw exceptions
    static void ThrowDuplicateElementError(void);
    static void CannotGetElementOfSet(void);
};

class CStlOneArgTemplate : public CContainerTypeInfo
{
    typedef CContainerTypeInfo CParent;
public:
    CStlOneArgTemplate(size_t size, TTypeInfo dataType,
                       bool randomOrder);
    CStlOneArgTemplate(size_t size, const CTypeRef& dataType,
                       bool randomOrder);

    const CMemberId& GetDataId(void) const
        {
            return m_DataId;
        }
    void SetDataId(const CMemberId& id);

    bool IsDefault(TConstObjectPtr objectPtr) const;
    void SetDefault(TObjectPtr objectPtr) const;

    // private use
    typedef bool (*TIsDefaultFunction)(TConstObjectPtr objectPtr);
    typedef void (*TSetDefaultFunction)(TObjectPtr objectPtr);

    void SetMemFunctions(TTypeCreate create,
                         TIsDefaultFunction isDefault,
                         TSetDefaultFunction setDefault);

private:
    TIsDefaultFunction m_IsDefault;
    TSetDefaultFunction m_SetDefault;

    CMemberId m_DataId;
};

class CStlTwoArgsTemplate : public CStlOneArgTemplate
{
    typedef CStlOneArgTemplate CParent;
public:
	typedef size_t TOffset;

    CStlTwoArgsTemplate(size_t size,
                        TTypeInfo keyType, TOffset keyOffset,
                        TTypeInfo valueType, TOffset valueOffset,
                        bool randomOrder);
    CStlTwoArgsTemplate(size_t size,
                        const CTypeRef& keyType, TOffset keyOffset,
                        const CTypeRef& valueType, TOffset valueOffset,
                        bool randomOrder);

    const CMemberId& GetKeyId(void) const
        {
            return m_KeyId;
        }
    const CMemberId& GetValueId(void) const
        {
            return m_ValueId;
        }
    void SetKeyId(const CMemberId& id);
    void SetValueId(const CMemberId& id);

private:
    CMemberId m_KeyId;
    CTypeRef m_KeyType;
    TOffset m_KeyOffset;

    CMemberId m_ValueId;
    CTypeRef m_ValueType;
    TOffset m_ValueOffset;

    static TTypeInfo CreateElementTypeInfo(TTypeInfo info);
};

//#include <serial/stltypesimpl.inl>

END_NCBI_SCOPE

#endif  /* STLTYPESIMPL__HPP */
