#ifndef SERIALBASE__HPP
#define SERIALBASE__HPP

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
*   File to be included in all headers generated by datatool
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.11  2002/05/15 20:22:02  grichenk
* Added CSerialObject -- base class for all generated ASN.1 classes
*
* Revision 1.10  2001/07/25 19:15:27  grichenk
* Added comments. Added type checking before dynamic cast.
*
* Revision 1.9  2001/07/16 16:22:47  grichenk
* Added CSerialUserOp class to create Assign() and Equals() methods for
* user-defind classes.
* Added SerialAssign<>() and SerialEquals<>() functions.
*
* Revision 1.8  2000/12/15 21:28:49  vasilche
* Moved some typedefs/enums from corelib/ncbistd.hpp.
* Added flags to CObjectIStream/CObjectOStream: eFlagAllowNonAsciiChars.
* TByte typedef replaced by Uint1.
*
* Revision 1.7  2000/07/12 13:30:56  vasilche
* Typo in function prototype.
*
* Revision 1.6  2000/07/11 20:34:51  vasilche
* File included in all generated headers made lighter.
* Nonnecessary code moved to serialimpl.hpp.
*
* Revision 1.5  2000/07/10 17:59:30  vasilche
* Moved macros needed in headers to serialbase.hpp.
* Use DECLARE_ENUM_INFO in generated code.
*
* Revision 1.4  2000/06/16 16:31:07  vasilche
* Changed implementation of choices and classes info to allow use of the same classes in generated and user written classes.
*
* Revision 1.3  2000/05/04 16:21:36  vasilche
* Fixed bug in choice reset.
*
* Revision 1.2  2000/04/28 16:58:03  vasilche
* Added classes CByteSource and CByteSourceReader for generic reading.
* Added delayed reading of choice variants.
*
* Revision 1.1  2000/04/03 18:47:09  vasilche
* Added main include file for generated headers.
* serialimpl.hpp is included in generated sources with GetTypeInfo methods
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <corelib/ncbiobj.hpp>
#include <typeinfo>

BEGIN_NCBI_SCOPE

class CTypeInfo;
class CClassTypeInfo;
class CChoiceTypeInfo;
class CEnumeratedTypeValues;

// enum for choice classes generated by datatool
enum EResetVariant {
    eDoResetVariant,
    eDoNotResetVariant
};

typedef void (*TPostReadFunction)(const CTypeInfo* info, void* object);
typedef void (*TPreWriteFunction)(const CTypeInfo* info, const void* object);

void SetPostRead(CClassTypeInfo*  info, TPostReadFunction function);
void SetPostRead(CChoiceTypeInfo* info, TPostReadFunction function);
void SetPreWrite(CClassTypeInfo*  info, TPreWriteFunction function);
void SetPreWrite(CChoiceTypeInfo* info, TPreWriteFunction function);

template<class Class>
class CClassPostReadPreWrite
{
public:
    static void PostRead(const CTypeInfo* /*info*/, void* object)
        {
            static_cast<Class*>(object)->PostRead();
        }
    static void PreWrite(const CTypeInfo* /*info*/, const void* object)
        {
            static_cast<const Class*>(object)->PreWrite();
        }
};


// Base class for all serializable objects
class CSerialObject : public CObject
{
public:
    virtual const CTypeInfo* GetThisTypeInfo(void) const = 0;
    virtual void Assign(const CSerialObject& source);
    virtual bool Equals(const CSerialObject& object) const;
};


// Base class for user-defined serializable classes
// to allow for objects assignment and comparison.
// EXAMPLE:
//   class CSeq_entry : public CSeq_entry_Base, CSerialUserOp
//
class CSerialUserOp
{
    friend class CClassTypeInfo;
    friend class CChoiceTypeInfo;
protected:
    // will be called after copying the datatool-generated members
    virtual void Assign(const CSerialUserOp& source) = 0;
    // will be called after comparing the datatool-generated members
    virtual bool Equals(const CSerialUserOp& object) const = 0;
};


/////////////////////////////////////////////////////////////////////
//
//  Assignment and comparison for serializable objects
//

template <class C>
C& SerialAssign(C& dest, const C& src)
{
    if ( typeid(src) != typeid(dest) ) {
        ERR_POST(Fatal <<
                 "SerialAssign() -- Assignment of incompatible types: " <<
                 typeid(dest).name() << " = " << typeid(src).name());
    }
    C::GetTypeInfo()->Assign(&dest, &src);
    return dest;
}


template <class C>
bool SerialEquals(const C& object1, const C& object2)
{
    if ( typeid(object1) != typeid(object2) ) {
        ERR_POST(Fatal <<
                 "SerialAssign() -- Can not compare types: " <<
                 typeid(object1).name() << " == " << typeid(object2).name());
    }
    return C::GetTypeInfo()->Equals(&object1, &object2);
}


END_NCBI_SCOPE

// these methods must be defined in root namespace so they have prefix NCBISER

// default functions do nothing
template<class CInfo>
inline
void NCBISERSetPostRead(const void* /*object*/, CInfo* /*info*/)
{
}

template<class CInfo>
inline
void NCBISERSetPreWrite(const void* /*object*/, CInfo* /*info*/)
{
}

// define for declaring specific function
#define NCBISER_HAVE_POST_READ(Class) \
template<class CInfo> \
inline \
void NCBISERSetPostRead(const Class* /*object*/, CInfo* info) \
{ \
    NCBI_NS_NCBI::SetPostRead(info, &CClassPostReadPreWrite<Class>::PostRead);\
}

#define NCBISER_HAVE_PRE_WRITE(Class) \
template<class CInfo> \
inline \
void NCBISERSetPreWrite(const Class* /*object*/, CInfo* info) \
{ \
    NCBI_NS_NCBI::SetPreWrite(info, &CClassPostReadPreWrite<Class>::PreWrite);\
}

#define DECLARE_INTERNAL_TYPE_INFO() \
    virtual const NCBI_NS_NCBI::CTypeInfo* GetThisTypeInfo(void) const \
    { return GetTypeInfo(); } \
    static const NCBI_NS_NCBI::CTypeInfo* GetTypeInfo(void)

#define ENUM_METHOD_NAME(EnumName) \
    NCBI_NAME2(GetTypeInfo_enum_,EnumName)
#define DECLARE_ENUM_INFO(EnumName) \
    const NCBI_NS_NCBI::CEnumeratedTypeValues* ENUM_METHOD_NAME(EnumName)(void)
#define DECLARE_INTERNAL_ENUM_INFO(EnumName) \
    static DECLARE_ENUM_INFO(EnumName)

#if HAVE_NCBI_C

#define ASN_STRUCT_NAME(AsnStructName) NCBI_NAME2(struct_, AsnStructName)
#define ASN_STRUCT_METHOD_NAME(AsnStructName) \
    NCBI_NAME2(GetTypeInfo_struct_,AsnStructName)

#define DECLARE_ASN_TYPE_INFO(AsnStructName) \
    const NCBI_NS_NCBI::CTypeInfo* ASN_STRUCT_METHOD_NAME(AsnStructName)(void)
#define DECLARE_ASN_STRUCT_INFO(AsnStructName) \
    struct ASN_STRUCT_NAME(AsnStructName); \
    DECLARE_ASN_TYPE_INFO(AsnStructName); \
    inline \
    const NCBI_NS_NCBI::CTypeInfo* \
    GetAsnStructTypeInfo(const ASN_STRUCT_NAME(AsnStructName)* ) \
    { \
        return ASN_STRUCT_METHOD_NAME(AsnStructName)(); \
    } \
    struct ASN_STRUCT_NAME(AsnStructName)

#define DECLARE_ASN_CHOICE_INFO(AsnChoiceName) \
    DECLARE_ASN_TYPE_INFO(AsnChoiceName)

#endif

#endif  /* SERIALBASE__HPP */
