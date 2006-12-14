#ifndef OBJECTINFO__HPP
#define OBJECTINFO__HPP

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
#include <corelib/ncbiobj.hpp>
#include <serial/serialdef.hpp>
#include <serial/continfo.hpp>
#include <serial/ptrinfo.hpp>
#include <serial/stdtypes.hpp>
#include <serial/classinfo.hpp>
#include <serial/choice.hpp>
#include <vector>
#include <memory>


/** @addtogroup ObjStreamSupport
 *
 * @{
 */


BEGIN_NCBI_SCOPE

class CObjectTypeInfo;
class CConstObjectInfo;
class CObjectInfo;

class CPrimitiveTypeInfo;
class CClassTypeInfoBase;
class CClassTypeInfo;
class CChoiceTypeInfo;
class CContainerTypeInfo;
class CPointerTypeInfo;

class CMemberId;
class CItemInfo;
class CMemberInfo;
class CVariantInfo;

class CReadContainerElementHook;

class CObjectTypeInfoMI;
class CObjectTypeInfoVI;
class CObjectTypeInfoCV;
class CConstObjectInfoMI;
class CConstObjectInfoCV;
class CConstObjectInfoEI;
class CObjectInfoMI;
class CObjectInfoCV;
class CObjectInfoEI;

/// Facilitate access to the data type information
class NCBI_XSERIAL_EXPORT CObjectTypeInfo
{
public:
    typedef CObjectTypeInfoMI CMemberIterator;
    typedef CObjectTypeInfoVI CVariantIterator;
    typedef CObjectTypeInfoCV CChoiceVariant;

    CObjectTypeInfo(TTypeInfo typeinfo = 0);

    ETypeFamily GetTypeFamily(void) const;

    bool Valid(void) const
        {
            return m_TypeInfo != 0;
        }
    DECLARE_OPERATOR_BOOL_PTR(m_TypeInfo);
    
    bool operator==(const CObjectTypeInfo& type) const;
    bool operator!=(const CObjectTypeInfo& type) const;

    // primitive type interface
    // only when GetTypeFamily() == CTypeInfo::eTypePrimitive
    EPrimitiveValueType GetPrimitiveValueType(void) const;
    bool IsPrimitiveValueSigned(void) const;

    // container interface
    // only when GetTypeFamily() == CTypeInfo::eTypeContainer
    CObjectTypeInfo GetElementType(void) const;

    // class interface
    // only when GetTypeFamily() == CTypeInfo::eTypeClass
    CMemberIterator BeginMembers(void) const;
    CMemberIterator FindMember(const string& memberName) const;
    CMemberIterator FindMemberByTag(int memberTag) const;

    // choice interface
    // only when GetTypeFamily() == CTypeInfo::eTypeChoice
    CVariantIterator BeginVariants(void) const;
    CVariantIterator FindVariant(const string& memberName) const;
    CVariantIterator FindVariantByTag(int memberTag) const;

    // pointer interface
    // only when GetTypeFamily() == CTypeInfo::eTypePointer
    CObjectTypeInfo GetPointedType(void) const;

    /// Set local (for the specified stream) read hook
    /// @param stream
    ///   Input data stream reader
    /// @param hook
    ///   Pointer to hook object
    void SetLocalReadHook(CObjectIStream& stream,
                          CReadObjectHook* hook) const;

    /// Set global (for all streams) read hook
    /// @param hook
    ///   Pointer to hook object
    void SetGlobalReadHook(CReadObjectHook* hook) const;

    /// Reset local read hook
    /// @param stream
    ///   Input data stream reader
    void ResetLocalReadHook(CObjectIStream& stream) const;

    /// Reset global read hooks
    void ResetGlobalReadHook(void) const;

    /// Set local context-specific read hook
    /// @param stream
    ///   Input data stream reader
    /// @param path
    ///   Context (stack path)
    /// @param hook
    ///   Pointer to hook object
    void SetPathReadHook(CObjectIStream* stream, const string& path,
                         CReadObjectHook* hook) const;


    /// Set local (for the specified stream) write hook
    /// @param stream
    ///   Output data stream writer
    /// @param hook
    ///   Pointer to hook object
    void SetLocalWriteHook(CObjectOStream& stream,
                          CWriteObjectHook* hook) const;

    /// Set global (for all streams) write hook
    /// @param hook
    ///   Pointer to hook object
    void SetGlobalWriteHook(CWriteObjectHook* hook) const;

    /// Reset local write hook
    /// @param stream
    ///   Output data stream writer
    void ResetLocalWriteHook(CObjectOStream& stream) const;

    /// Reset global write hooks
    void ResetGlobalWriteHook(void) const;

    /// Set local context-specific write hook
    /// @param stream
    ///   Output data stream writer
    /// @param path
    ///   Context (stack path)
    /// @param hook
    ///   Pointer to hook object
    void SetPathWriteHook(CObjectOStream* stream, const string& path,
                          CWriteObjectHook* hook) const;


    /// Set local (for the specified stream) skip hook
    /// @param stream
    ///   Input data stream reader
    /// @param hook
    ///   Pointer to hook object
    void SetLocalSkipHook(CObjectIStream& stream,
                          CSkipObjectHook* hook) const;

    /// Set global (for all streams) skip hook
    /// @param hook
    ///   Pointer to hook object
    void SetGlobalSkipHook(CSkipObjectHook* hook) const;

    /// Reset local skip hook
    /// @param stream
    ///   Input data stream reader
    void ResetLocalSkipHook(CObjectIStream& stream) const;

    /// Reset global skip hooks
    void ResetGlobalSkipHook(void) const;

    /// Set local context-specific skip hook
    /// @param stream
    ///   Input data stream reader
    /// @param path
    ///   Context (stack path)
    /// @param hook
    ///   Pointer to hook object
    void SetPathSkipHook(CObjectIStream* stream, const string& path,
                         CSkipObjectHook* hook) const;


    /// Set local (for the specified stream) copy hook
    /// @param stream
    ///   Data copier
    /// @param hook
    ///   Pointer to hook object
    void SetLocalCopyHook(CObjectStreamCopier& stream,
                          CCopyObjectHook* hook) const;

    /// Set global (for all streams) copy hook
    /// @param hook
    ///   Pointer to hook object
    void SetGlobalCopyHook(CCopyObjectHook* hook) const;

    /// Reset local copy hook
    /// @param stream
    ///   Data copier
    void ResetLocalCopyHook(CObjectStreamCopier& stream) const;

    /// Reset global read hooks
    void ResetGlobalCopyHook(void) const;

    /// Set local context-specific copy hook
    /// @param stream
    ///   Data copier
    /// @param path
    ///   Context (stack path)
    /// @param hook
    ///   Pointer to hook object
    void SetPathCopyHook(CObjectStreamCopier* stream, const string& path,
                         CCopyObjectHook* hook) const;

public: // mostly for internal use
    TTypeInfo GetTypeInfo(void) const;
    const CPrimitiveTypeInfo* GetPrimitiveTypeInfo(void) const;
    const CClassTypeInfo* GetClassTypeInfo(void) const;
    const CChoiceTypeInfo* GetChoiceTypeInfo(void) const;
    const CContainerTypeInfo* GetContainerTypeInfo(void) const;
    const CPointerTypeInfo* GetPointerTypeInfo(void) const;

    TMemberIndex FindMemberIndex(const string& name) const;
    TMemberIndex FindMemberIndex(int tag) const;
    TMemberIndex FindVariantIndex(const string& name) const;
    TMemberIndex FindVariantIndex(int tag) const;

    CMemberIterator GetMemberIterator(TMemberIndex index) const;
    CVariantIterator GetVariantIterator(TMemberIndex index) const;

protected:
    void ResetTypeInfo(void);
    void SetTypeInfo(TTypeInfo typeinfo);

    void CheckTypeFamily(ETypeFamily family) const;
    void WrongTypeFamily(ETypeFamily needFamily) const;

private:
    TTypeInfo m_TypeInfo;

private:
    CTypeInfo* GetNCTypeInfo(void) const;
};


/// Facilitate read access to a particular instance of an object
/// of the specified type.
class NCBI_XSERIAL_EXPORT CConstObjectInfo : public CObjectTypeInfo
{
public:
    typedef TConstObjectPtr TObjectPtrType;
    typedef CConstObjectInfoEI CElementIterator;
    typedef CConstObjectInfoMI CMemberIterator;
    typedef CConstObjectInfoCV CChoiceVariant;
    
    enum ENonCObject {
        eNonCObject
    };

    /// Create empty CObjectInfo
    CConstObjectInfo(void);
    /// Initialize CObjectInfo
    CConstObjectInfo(TConstObjectPtr objectPtr, TTypeInfo typeInfo);
    CConstObjectInfo(pair<TConstObjectPtr, TTypeInfo> object);
    CConstObjectInfo(pair<TObjectPtr, TTypeInfo> object);
    /// Initialize CObjectInfo when we are sure that object 
    /// is not inherited from CObject (for efficiency)
    CConstObjectInfo(TConstObjectPtr objectPtr, TTypeInfo typeInfo,
                     ENonCObject nonCObject);

    /// Reset CObjectInfo to empty state
    void Reset(void);
    /// Set CObjectInfo
    CConstObjectInfo& operator=(pair<TConstObjectPtr, TTypeInfo> object);
    CConstObjectInfo& operator=(pair<TObjectPtr, TTypeInfo> object);

    /// Check if CObjectInfo initialized with valid object
    bool Valid(void) const
        {
            return m_ObjectPtr != 0;
        }
    DECLARE_OPERATOR_BOOL_PTR(m_ObjectPtr);

    bool operator==(const CConstObjectInfo& obj) const
    {
        return m_ObjectPtr == obj.m_ObjectPtr;
    }
    bool operator!=(const CConstObjectInfo& obj) const
    {
        return m_ObjectPtr != obj.m_ObjectPtr;
    }

    void ResetObjectPtr(void);

    /// Get pointer to object
    TConstObjectPtr GetObjectPtr(void) const;
    pair<TConstObjectPtr, TTypeInfo> GetPair(void) const;

    // primitive type interface
    bool GetPrimitiveValueBool(void) const;
    char GetPrimitiveValueChar(void) const;

    Int4 GetPrimitiveValueInt4(void) const;
    Uint4 GetPrimitiveValueUint4(void) const;
    Int8 GetPrimitiveValueInt8(void) const;
    Uint8 GetPrimitiveValueUint8(void) const;
    int GetPrimitiveValueInt(void) const;
    unsigned GetPrimitiveValueUInt(void) const;
    long GetPrimitiveValueLong(void) const;
    unsigned long GetPrimitiveValueULong(void) const;

    double GetPrimitiveValueDouble(void) const;

    void GetPrimitiveValueString(string& value) const;
    string GetPrimitiveValueString(void) const;

    void GetPrimitiveValueOctetString(vector<char>& value) const;

    void GetPrimitiveValueBitString(CBitString& value) const;

    void GetPrimitiveValueAnyContent(CAnyContentObject& value) const;

    // class interface
    CMemberIterator GetMember(CObjectTypeInfo::CMemberIterator m) const;
    CMemberIterator BeginMembers(void) const;
    CMemberIterator GetClassMemberIterator(TMemberIndex index) const;
    CMemberIterator FindClassMember(const string& memberName) const;
    CMemberIterator FindClassMemberByTag(int memberTag) const;

    // choice interface
    TMemberIndex GetCurrentChoiceVariantIndex(void) const;
    CChoiceVariant GetCurrentChoiceVariant(void) const;

    // pointer interface
    CConstObjectInfo GetPointedObject(void) const;
    
    // container interface
    CElementIterator BeginElements(void) const;

protected:
    void Set(TConstObjectPtr objectPtr, TTypeInfo typeInfo);
    
private:
    TConstObjectPtr m_ObjectPtr; // object pointer
    CConstRef<CObject> m_Ref; // hold reference to CObject for correct removal
};

/// Facilitate read/write access to a particular instance of an object
/// of the specified type.
class NCBI_XSERIAL_EXPORT CObjectInfo : public CConstObjectInfo
{
    typedef CConstObjectInfo CParent;
public:
    typedef TObjectPtr TObjectPtrType;
    typedef CObjectInfoEI CElementIterator;
    typedef CObjectInfoMI CMemberIterator;
    typedef CObjectInfoCV CChoiceVariant;

    /// Create empty CObjectInfo
    CObjectInfo(void);
    /// Initialize CObjectInfo
    CObjectInfo(TObjectPtr objectPtr, TTypeInfo typeInfo);
    CObjectInfo(pair<TObjectPtr, TTypeInfo> object);
    /// Initialize CObjectInfo when we are sure that object 
    /// is not inherited from CObject (for efficiency)
    CObjectInfo(TObjectPtr objectPtr, TTypeInfo typeInfo,
                ENonCObject nonCObject);
    /// Create CObjectInfo with new object
    explicit CObjectInfo(TTypeInfo typeInfo);

    /// Set CObjectInfo to point to another object
    CObjectInfo& operator=(pair<TObjectPtr, TTypeInfo> object);
    
    /// Get pointer to object
    TObjectPtr GetObjectPtr(void) const;
    pair<TObjectPtr, TTypeInfo> GetPair(void) const;

    // primitive type interface
    void SetPrimitiveValueBool(bool value);
    void SetPrimitiveValueChar(char value);

    void SetPrimitiveValueInt4(Int4 value);
    void SetPrimitiveValueUint4(Uint4 value);
    void SetPrimitiveValueInt8(Int8 value);
    void SetPrimitiveValueUint8(Uint8 value);
    void SetPrimitiveValueInt(int value);
    void SetPrimitiveValueUInt(unsigned value);
    void SetPrimitiveValueLong(long value);
    void SetPrimitiveValueULong(unsigned long value);

    void SetPrimitiveValueDouble(double value);

    void SetPrimitiveValueString(const string& value);

    void SetPrimitiveValueOctetString(const vector<char>& value);

    void SetPrimitiveValueBitString(const CBitString& value);

    void SetPrimitiveValueAnyContent(const CAnyContentObject& value);

    // class interface
    CMemberIterator GetMember(CObjectTypeInfo::CMemberIterator m) const;
    CMemberIterator BeginMembers(void) const;
    CMemberIterator GetClassMemberIterator(TMemberIndex index) const;
    CMemberIterator FindClassMember(const string& memberName) const;
    CMemberIterator FindClassMemberByTag(int memberTag) const;

    // choice interface
    CChoiceVariant GetCurrentChoiceVariant(void) const;

    // pointer interface
    CObjectInfo GetPointedObject(void) const;

    // container interface
    CElementIterator BeginElements(void) const;
    void ReadContainer(CObjectIStream& in, CReadContainerElementHook& hook);
};


/* @} */


#include <serial/impl/objectinfo.inl>

END_NCBI_SCOPE

#endif  /* OBJECTINFO__HPP */



/* ---------------------------------------------------------------------------
* $Log$
* Revision 1.17  2006/12/14 19:31:00  gouriano
* Added documentation
*
* Revision 1.16  2006/12/07 18:59:30  gouriano
* Reviewed doxygen groupping, added documentation
*
* Revision 1.15  2006/10/12 15:08:24  gouriano
* Some header files moved into impl
*
* Revision 1.14  2006/10/05 19:23:04  gouriano
* Some headers moved into impl
*
* Revision 1.13  2006/07/26 19:06:30  ucko
* Add reflective accessors for BitString and AnyContent values.
*
* Revision 1.12  2005/01/26 13:10:07  rsmith
* delete unnecessary and illegal class specifier.
*
* Revision 1.11  2005/01/24 17:05:48  vasilche
* Safe boolean operators.
*
* Revision 1.10  2004/07/27 15:00:57  ucko
* Restore (and define) removed SetPrimitiveValueInt variants.
*
* Revision 1.9  2004/04/30 13:29:09  gouriano
* Remove obsolete function declarations
*
* Revision 1.8  2004/01/05 14:24:08  gouriano
* Added possibility to set serialization hooks by stack path
*
* Revision 1.7  2003/07/29 18:47:46  vasilche
* Fixed thread safeness of object stream hooks.
*
* Revision 1.6  2003/04/15 16:18:11  siyan
* Added doxygen support
*
* Revision 1.5  2002/12/23 18:38:51  dicuccio
* Added WIn32 export specifier: NCBI_XSERIAL_EXPORT.
* Moved all CVS logs to the end.
*
* Revision 1.4  2001/05/17 14:57:17  lavr
* Typos corrected
*
* Revision 1.3  2001/01/22 23:20:30  vakatov
* + CObjectInfo::GetMember(), CConstObjectInfo::GetMember()
*
* Revision 1.2  2000/12/15 15:37:59  vasilche
* Added support of Int8 and long double.
* Enum values now have type Int4 instead of long.
*
* Revision 1.1  2000/10/20 15:51:24  vasilche
* Fixed data error processing.
* Added interface for constructing container objects directly into output stream.
* object.hpp, object.inl and object.cpp were split to
* objectinfo.*, objecttype.*, objectiter.* and objectio.*.
*
* ===========================================================================
*/
