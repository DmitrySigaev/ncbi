#ifndef SERIALDEF__HPP
#define SERIALDEF__HPP

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
* Revision 1.12  2000/09/18 20:00:09  vasilche
* Separated CVariantInfo and CMemberInfo.
* Implemented copy hooks.
* All hooks now are stored in CTypeInfo/CMemberInfo/CVariantInfo.
* Most type specific functions now are implemented via function pointers instead of virtual functions.
*
* Revision 1.11  2000/08/15 19:44:42  vasilche
* Added Read/Write hooks:
* CReadObjectHook/CWriteObjectHook for objects of specified type.
* CReadClassMemberHook/CWriteClassMemberHook for specified members.
* CReadChoiceVariantHook/CWriteChoiceVariant for specified choice variants.
* CReadContainerElementHook/CWriteContainerElementsHook for containers.
*
* Revision 1.10  2000/07/03 18:42:37  vasilche
* Added interface to typeinfo via CObjectInfo and CConstObjectInfo.
* Reduced header dependency.
*
* Revision 1.9  2000/06/01 19:06:58  vasilche
* Added parsing of XML data.
*
* Revision 1.8  2000/04/28 16:58:03  vasilche
* Added classes CByteSource and CByteSourceReader for generic reading.
* Added delayed reading of choice variants.
*
* Revision 1.7  2000/04/13 14:50:18  vasilche
* Added CObjectIStream::Open() and CObjectOStream::Open() for easier use.
*
* Revision 1.6  2000/04/06 16:10:52  vasilche
* Fixed bug with iterators in choices.
* Removed unneeded calls to ReadExternalObject/WriteExternalObject.
* Added output buffering to text ASN.1 data.
*
* Revision 1.5  2000/04/03 18:47:10  vasilche
* Added main include file for generated headers.
* serialimpl.hpp is included in generated sources with GetTypeInfo methods
*
* Revision 1.4  2000/01/10 19:46:33  vasilche
* Fixed encoding/decoding of REAL type.
* Fixed encoding/decoding of StringStore.
* Fixed encoding/decoding of NULL type.
* Fixed error reporting.
* Reduced object map (only classes).
*
* Revision 1.3  1999/12/17 19:04:54  vasilche
* Simplified generation of GetTypeInfo methods.
*
* Revision 1.2  1999/09/22 20:11:51  vasilche
* Modified for compilation on IRIX native c++ compiler.
*
* Revision 1.1  1999/06/24 14:44:44  vasilche
* Added binary ASN.1 output.
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>

BEGIN_NCBI_SCOPE

// forward declaration of two main classes
class CTypeRef;
class CTypeInfo;

class CObjectIStream;
class CObjectOStream;

class CObjectStreamCopier;

// typedef for object references (constant and nonconstant)
typedef void* TObjectPtr;
typedef const void* TConstObjectPtr;

// shortcut typedef: almost everywhere in code we have pointer to const CTypeInfo
typedef const CTypeInfo* TTypeInfo;
typedef TTypeInfo (*TTypeInfoGetter)(void);
typedef TTypeInfo (*TTypeInfoGetter1)(TTypeInfo);
typedef TTypeInfo (*TTypeInfoGetter2)(TTypeInfo, TTypeInfo);

#define NCBISER_ALLOW_CYCLES 1

enum ESerialDataFormat {
    eSerial_None         = 0,
    eSerial_AsnText      = 1,      // open ASN.1 text format
    eSerial_AsnBinary    = 2,      // open ASN.1 binary format
    eSerial_Xml          = 3       // open XML format
};

enum ESerialOpenFlags {
    eSerial_StdWhenEmpty = 1 << 0, // use std stream when filename is empty
    eSerial_StdWhenDash  = 1 << 1, // use std stream when filename is "-"
    eSerial_StdWhenStd   = 1 << 2, // use std when filename is "stdin"/"stdout"
    eSerial_StdWhenMask  = 15,
    eSerial_StdWhenAny   = eSerial_StdWhenMask,
    eSerial_UseFileForReread = 1 << 4
};

// type family
enum ETypeFamily {
    eTypeFamilyPrimitive,
    eTypeFamilyClass,
    eTypeFamilyChoice,
    eTypeFamilyContainer,
    eTypeFamilyPointer
};

enum EPrimitiveValueType {
    ePrimitiveValueSpecial,        // null, void
    ePrimitiveValueBool,           // bool
    ePrimitiveValueChar,           // char
    ePrimitiveValueInteger,        // (signed|unsigned) (char|short|int|long)
    ePrimitiveValueReal,           // float|double
    ePrimitiveValueString,         // string|char*|const char*
    ePrimitiveValueEnum,           // enum
    ePrimitiveValueOctetString     // vector<(signed|unsigned)? char>
};

enum EContainerType {
    eContainerVector,              // allows indexing & access to size
    eContainerList,                // only sequential access
    eContainerSet,
    eContainerMap
};

//type used for indexing class members and choice variants
typedef size_t TMemberIndex;

// start if member indexing
const TMemberIndex kFirstMemberIndex = 1;
// special value returned from FindMember
const TMemberIndex kInvalidMember = kFirstMemberIndex - 1;
// special value for marking empty choice
const TMemberIndex kEmptyChoice = kInvalidMember;

//#include <serial/serialdef.inl>

END_NCBI_SCOPE

#endif  /* SERIALDEF__HPP */
