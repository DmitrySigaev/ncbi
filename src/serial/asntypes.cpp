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
* Revision 1.38  2000/05/09 16:38:37  vasilche
* CObject::GetTypeInfo now moved to CObjectGetTypeInfo::GetTypeInfo to reduce possible errors.
* Added write context to CObjectOStream.
* Inlined most of methods of helping class Member, Block, ByteBlock etc.
*
* Revision 1.37  2000/03/31 21:38:20  vasilche
* Renamed First() -> FirstNode(), Next() -> NextNode() to avoid name conflict.
*
* Revision 1.36  2000/03/29 15:55:26  vasilche
* Added two versions of object info - CObjectInfo and CConstObjectInfo.
* Added generic iterators by class -
* 	CTypeIterator<class>, CTypeConstIterator<class>,
* 	CStdTypeIterator<type>, CStdTypeConstIterator<type>,
* 	CObjectsIterator and CObjectsConstIterator.
*
* Revision 1.35  2000/03/14 14:42:29  vasilche
* Fixed error reporting.
*
* Revision 1.34  2000/03/07 14:06:20  vasilche
* Added stream buffering to ASN.1 binary input.
* Optimized class loading/storing.
* Fixed bugs in processing OPTIONAL fields.
* Added generation of reference counted objects.
*
* Revision 1.33  2000/02/17 20:02:42  vasilche
* Added some standard serialization exceptions.
* Optimized text/binary ASN.1 reading.
* Fixed wrong encoding of StringStore in ASN.1 binary format.
* Optimized logic of object collection.
*
* Revision 1.32  2000/02/01 21:47:21  vasilche
* Added CGeneratedChoiceTypeInfo for generated choice classes.
* Added buffering to CObjectIStreamAsn.
* Removed CMemberInfo subclasses.
* Added support for DEFAULT/OPTIONAL members.
*
* Revision 1.31  1999/12/28 18:55:49  vasilche
* Reduced size of compiled object files:
* 1. avoid inline or implicit virtual methods (especially destructors).
* 2. avoid std::string's methods usage in inline methods.
* 3. avoid string literals ("xxx") in inline methods.
*
* Revision 1.30  1999/12/17 19:05:01  vasilche
* Simplified generation of GetTypeInfo methods.
*
* Revision 1.29  1999/10/04 19:39:45  vasilche
* Fixed bug in CObjectOStreamBinary.
* Start using of BSRead/BSWrite.
* Added ASNCALL macro for prototypes of old ASN.1 functions.
*
* Revision 1.28  1999/10/04 16:22:15  vasilche
* Fixed bug with old ASN.1 structures.
*
* Revision 1.27  1999/09/29 22:36:33  vakatov
* Dont forget to #include ncbistd.hpp before #ifdef HAVE_NCBI_C...
*
* Revision 1.26  1999/09/24 18:55:58  vasilche
* ASN.1 types will not be compiled is we don't have NCBI toolkit.
*
* Revision 1.25  1999/09/24 18:19:17  vasilche
* Removed dependency on NCBI toolkit.
*
* Revision 1.24  1999/09/23 21:16:07  vasilche
* Removed dependance on asn.h
*
* Revision 1.23  1999/09/23 20:25:03  vasilche
* Added support HAVE_NCBI_C
*
* Revision 1.22  1999/09/23 18:56:57  vasilche
* Fixed bugs with overloaded methods in objistr*.hpp & objostr*.hpp
*
* Revision 1.21  1999/09/22 20:11:54  vasilche
* Modified for compilation on IRIX native c++ compiler.
*
* Revision 1.20  1999/09/14 18:54:14  vasilche
* Fixed bugs detected by gcc & egcs.
* Removed unneeded includes.
*
* Revision 1.19  1999/08/31 17:50:08  vasilche
* Implemented several macros for specific data types.
* Added implicit members.
* Added multimap and set.
*
* Revision 1.18  1999/08/16 16:08:30  vasilche
* Added ENUMERATED type.
*
* Revision 1.17  1999/08/13 20:22:57  vasilche
* Fixed lot of bugs in datatool
*
* Revision 1.16  1999/08/13 15:53:49  vasilche
* C++ analog of asntool: datatool
*
* Revision 1.15  1999/07/21 20:02:53  vasilche
* Added embedding of ASN.1 binary output from ToolKit to our binary format.
* Fixed bugs with storing pointers into binary ASN.1
*
* Revision 1.14  1999/07/21 18:05:09  vasilche
* Fixed OPTIONAL attribute for ASN.1 structures.
*
* Revision 1.13  1999/07/20 18:23:08  vasilche
* Added interface to old ASN.1 routines.
* Added fixed choice of subclasses to use for pointers.
*
* Revision 1.12  1999/07/19 15:50:30  vasilche
* Added interface to old ASN.1 routines.
* Added naming of key/value in STL map.
*
* Revision 1.11  1999/07/15 16:54:48  vasilche
* Implemented vector<X> & vector<char> as special case.
*
* Revision 1.10  1999/07/13 20:54:05  vasilche
* Fixed minor bugs.
*
* Revision 1.9  1999/07/13 20:18:15  vasilche
* Changed types naming.
*
* Revision 1.8  1999/07/09 20:27:05  vasilche
* Fixed some bugs
*
* Revision 1.7  1999/07/09 16:32:54  vasilche
* Added OCTET STRING write/read.
*
* Revision 1.6  1999/07/07 19:59:03  vasilche
* Reduced amount of data allocated on heap
* Cleaned ASN.1 structures info
*
* Revision 1.5  1999/07/07 18:18:32  vasilche
* Fixed some bugs found by MS VC++
*
* Revision 1.4  1999/07/02 21:31:51  vasilche
* Implemented reading from ASN.1 binary format.
*
* Revision 1.3  1999/07/01 17:55:25  vasilche
* Implemented ASN.1 binary write.
*
* Revision 1.2  1999/06/30 18:54:58  vasilche
* Fixed some errors under MSVS
*
* Revision 1.1  1999/06/30 16:04:46  vasilche
* Added support for old ASN.1 structures.
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>

#if HAVE_NCBI_C
#include <corelib/ncbiutil.hpp>
#include <serial/asntypes.hpp>
#include <serial/autoptrinfo.hpp>
#include <serial/objostr.hpp>
#include <serial/objistr.hpp>
#include <serial/classinfo.hpp>
#include <serial/typemap.hpp>
#include <asn.h>

BEGIN_NCBI_SCOPE

static inline
void* Alloc(size_t size)
{
	return NotNull(calloc(size, 1));
}

template<class T>
static inline
T* Alloc(T*& ptr)
{
	return ptr = static_cast<T*>(Alloc(sizeof(T)));
}

static CTypeInfoMap<CSequenceOfTypeInfo> CSequenceOfTypeInfo_map;

TTypeInfo CSequenceOfTypeInfo::GetTypeInfo(TTypeInfo base)
{
    return CSequenceOfTypeInfo_map.GetTypeInfo(base);
}

CSequenceOfTypeInfo::CSequenceOfTypeInfo(TTypeInfo type)
    : CParent("SEQUENCE OF " + type->GetName()), m_DataType(type)
{
	Init();
}

CSequenceOfTypeInfo::CSequenceOfTypeInfo(const string& name, TTypeInfo type)
    : CParent(name), m_DataType(type)
{
	Init();
}

CSequenceOfTypeInfo::CSequenceOfTypeInfo(const char* name, TTypeInfo type)
    : CParent(name), m_DataType(type)
{
	Init();
}

CSequenceOfTypeInfo::~CSequenceOfTypeInfo(void)
{
}

void CSequenceOfTypeInfo::Init(void)
{
	TTypeInfo type = m_DataType;
    _TRACE("SequenceOf(" << type->GetName() << ") " << typeid(*type).name());
    const CAutoPointerTypeInfo* ptrInfo =
        dynamic_cast<const CAutoPointerTypeInfo*>(type);
    if ( ptrInfo != 0 ) {
        // data type is auto_ptr
        TTypeInfo asnType = ptrInfo->GetDataTypeInfo();
        if ( dynamic_cast<const CChoiceTypeInfo*>(asnType) != 0 ) {
            // CHOICE
            _TRACE("SequenceOf(" << type->GetName() << ") AUTO CHOICE");
            SetChoiceNext();
            m_DataType = asnType;
        }
        else if ( dynamic_cast<const CClassInfoTmpl*>(asnType) != 0 ) {
            // user types
            if ( dynamic_cast<const CClassInfoTmpl*>(asnType)->
                 GetMembers().GetFirstMemberOffset() < sizeof(void*) ) {
                THROW1_TRACE(runtime_error,
                             "CSequenceOfTypeInfo: incompatible type: " +
                             type->GetName() + ": " + typeid(*type).name() +
                             " size: " + NStr::IntToString(type->GetSize()));
            }
            m_NextOffset = 0;
            m_DataOffset = 0;
            m_DataType = asnType;
            _TRACE("SequenceOf(" << type->GetName() << ") SEQUENCE");
        }
        else if ( asnType->GetSize() <= sizeof(dataval) ) {
            // statndard types and SET/SEQUENCE OF
            _TRACE("SequenceOf(" << type->GetName() << ") AUTO VALNODE");
            SetValNodeNext();
			m_DataType = asnType;
        }
		else {
/*
			_ASSERT(type->GetSize() <= sizeof(dataval));
            _TRACE("SequenceOf(" << type->GetName() << ") VALNODE");
            SetValNodeNext();
*/
            THROW1_TRACE(runtime_error,
                         "CSequenceOfTypeInfo: incompatible type: " +
                         type->GetName() + ": " + typeid(*type).name() +
                         " size: " + NStr::IntToString(type->GetSize()));
		}
    }
    else if ( type->GetSize() <= sizeof(dataval) ) {
        // SEQUENCE OF, SET OF or primitive types
        _TRACE("SequenceOf(" << type->GetName() << ") VALNODE");
        SetValNodeNext();
    }
	else {
		THROW1_TRACE(runtime_error,
                     "CSequenceOfTypeInfo: incompatible type: " +
                     type->GetName() + ": " + typeid(*type).name() +
                     " size: " + NStr::IntToString(type->GetSize()));
	}
}

void CSequenceOfTypeInfo::SetChoiceNext(void)
{
    m_NextOffset = offsetof(valnode, next);
    m_DataOffset = 0;
}

void CSequenceOfTypeInfo::SetValNodeNext(void)
{
    m_NextOffset = offsetof(valnode, next);
    m_DataOffset = offsetof(valnode, data);
}

bool CSequenceOfTypeInfo::RandomOrder(void) const
{
    return false;
}

size_t CSequenceOfTypeInfo::GetSize(void) const
{
    return TType::GetSize();
}

TObjectPtr CSequenceOfTypeInfo::CreateData(void) const
{
    if ( m_DataOffset == 0 ) {
        _ASSERT(m_NextOffset == 0 || m_NextOffset == offsetof(valnode, next));
        return GetDataTypeInfo()->Create();
	}
    else {
        _ASSERT(m_NextOffset == offsetof(valnode, next));
		_ASSERT(m_DataOffset == offsetof(valnode, data));
        return Alloc(sizeof(valnode));
	}
}

bool CSequenceOfTypeInfo::IsDefault(TConstObjectPtr object) const
{
    return FirstNode(object) == 0;
}

bool CSequenceOfTypeInfo::Equals(TConstObjectPtr object1,
                                 TConstObjectPtr object2) const
{
    for ( object1 = FirstNode(object1), object2 = FirstNode(object2);
          object1 != 0;
          object1 = NextNode(object1), object2 = NextNode(object2) ) {
        if ( object2 == 0 )
            return false;
        if ( !GetDataTypeInfo()->Equals(Data(object1), Data(object2)) )
            return false;
    }
    return object2 == 0;
}

void CSequenceOfTypeInfo::SetDefault(TObjectPtr dst) const
{
    FirstNode(dst) = 0;
}

void CSequenceOfTypeInfo::Assign(TObjectPtr dst, TConstObjectPtr src) const
{
    src = FirstNode(src);
    if ( src == 0 ) {
        FirstNode(dst) = 0;
        return;
    }

    TTypeInfo dataType = GetDataTypeInfo();
    dst = FirstNode(dst) = CreateData();
    dataType->Assign(Data(dst), Data(src));
    while ( (src = NextNode(src)) != 0 ) {
        dst = NextNode(dst) = CreateData();
        dataType->Assign(Data(dst), Data(src));
    }
}

void CSequenceOfTypeInfo::WriteData(CObjectOStream& out,
                                    TConstObjectPtr object) const
{
    CObjectOStream::Block block(out, RandomOrder());
    TTypeInfo dataType = GetDataTypeInfo();
    for ( object = FirstNode(object); object; object = NextNode(object) ) {
        block.Next();
        dataType->WriteData(out, Data(object));
    }
}

void CSequenceOfTypeInfo::ReadData(CObjectIStream& in,
                                   TObjectPtr object) const
{
    _TRACE("SequenceOf<" << GetDataTypeInfo()->GetName() << ">::ReadData(" <<
           NStr::PtrToString(object) << ")");
    CObjectIStream::Block block(in, RandomOrder());
    if ( !block.Next() ) {
        FirstNode(object) = 0;
        return;
    }
    TTypeInfo dataType = GetDataTypeInfo();

    TObjectPtr next = FirstNode(object);
    if ( next == 0 ) {
        ERR_POST(Warning << "null sequence pointer"); 
        next = FirstNode(object) = CreateData();
        _TRACE("new " << dataType->GetName() << ": " <<
               NStr::PtrToString(next));
    }
    object = next;

    dataType->ReadData(in, Data(object));

    while ( block.Next() ) {

        next = NextNode(object);
        if ( next == 0 ) {
            ERR_POST(Warning << "null sequence pointer"); 
            next = NextNode(object) = CreateData();
            _TRACE("new " << dataType->GetName() << ": " <<
                   NStr::PtrToString(next));
        }
        object = next;

        dataType->ReadData(in, Data(object));
    }
}

void CSequenceOfTypeInfo::SkipData(CObjectIStream& in) const
{
    _TRACE("SequenceOf<" << GetDataTypeInfo()->GetName() << ">::SkipData()");
    CObjectIStream::Block block(in, RandomOrder());
    TTypeInfo dataType = GetDataTypeInfo();
    while ( block.Next() ) {
        dataType->SkipData(in);
    }
}

static CTypeInfoMap<CSetOfTypeInfo> CSetOfTypeInfo_map;

TTypeInfo CSetOfTypeInfo::GetTypeInfo(TTypeInfo base)
{
    return CSetOfTypeInfo_map.GetTypeInfo(base);
}

CSetOfTypeInfo::CSetOfTypeInfo(TTypeInfo type)
    : CParent("SET OF " + type->GetName(), type)
{
}

CSetOfTypeInfo::CSetOfTypeInfo(const string& name, TTypeInfo type)
    : CParent(name, type)
{
}

CSetOfTypeInfo::CSetOfTypeInfo(const char* name, TTypeInfo type)
    : CParent(name, type)
{
}

CSetOfTypeInfo::~CSetOfTypeInfo(void)
{
}

bool CSetOfTypeInfo::RandomOrder(void) const
{
    return true;
}

CChoiceTypeInfo::CChoiceTypeInfo(const string& name)
    : CParent(name)
{
}

CChoiceTypeInfo::CChoiceTypeInfo(const char* name)
    : CParent(name)
{
}

CChoiceTypeInfo::~CChoiceTypeInfo(void)
{
}

size_t CChoiceTypeInfo::GetSize(void) const
{
    return sizeof(TObjectType);
}

TObjectPtr CChoiceTypeInfo::Create(void) const
{
    return Alloc(sizeof(TObjectType));
}

typedef CChoiceTypeInfo::TMemberIndex TMemberIndex;

TMemberIndex CChoiceTypeInfo::GetIndex(TConstObjectPtr object) const
{
    return Get(object).choice - 1;
}

void CChoiceTypeInfo::SetIndex(TObjectPtr object, TMemberIndex index) const
{
    Get(object).choice = index + 1;
}

TObjectPtr CChoiceTypeInfo::x_GetData(TObjectPtr object, TMemberIndex ) const
{
    return &Get(object).data;
}

COctetStringTypeInfo::COctetStringTypeInfo(void)
    : CParent("OCTET STRING")
{
}

COctetStringTypeInfo::~COctetStringTypeInfo(void)
{
}

size_t COctetStringTypeInfo::GetSize(void) const
{
    return TType::GetSize();
}

bool COctetStringTypeInfo::IsDefault(TConstObjectPtr object) const
{
    return Get(object)->totlen == 0;
}

bool COctetStringTypeInfo::Equals(TConstObjectPtr obj1,
                                  TConstObjectPtr obj2) const
{
    bytestore* bs1 = Get(obj1);
    bytestore* bs2 = Get(obj2);
    if ( bs1 == 0 || bs2 == 0 )
		return bs1 == bs2;

	Int4 len = BSLen(bs1);
    if ( len != BSLen(bs2) )
        return false;
    
	BSSeek(bs1, 0, SEEK_SET);
	BSSeek(bs2, 0, SEEK_SET);
	char buff1[1024], buff2[1024];
	while ( len > 0 ) {
		Int4 chunk = sizeof(buff1);
		if ( chunk > len )
			chunk = len;
		BSRead(bs1, buff1, chunk);
		BSRead(bs2, buff2, chunk);
		if ( memcmp(buff1, buff2, chunk) != 0 )
			return false;
		len -= chunk;
	}
	return true;
}

void COctetStringTypeInfo::SetDefault(TObjectPtr dst) const
{
    BSFree(Get(dst));
    Get(dst) = BSNew(0);
}

void COctetStringTypeInfo::Assign(TObjectPtr dst, TConstObjectPtr src) const
{
	if ( Get(src) == 0 )
		THROW1_TRACE(runtime_error, "null bytestore pointer");
	BSFree(Get(dst));
	Get(dst) = BSDup(Get(src));
}

void COctetStringTypeInfo::WriteData(CObjectOStream& out,
                                     TConstObjectPtr object) const
{
	bytestore* bs = const_cast<bytestore*>(Get(object));
	if ( bs == 0 )
		THROW1_TRACE(runtime_error, "null bytestore pointer");
	Int4 len = BSLen(bs);
	CObjectOStream::ByteBlock block(out, len);
	BSSeek(bs, 0, SEEK_SET);
	char buff[1024];
	while ( len > 0 ) {
		Int4 chunk = sizeof(buff);
		if ( chunk > len )
			chunk = len;
		BSRead(bs, buff, chunk);
		block.Write(buff, chunk);
		len -= chunk;
	}
}

void COctetStringTypeInfo::ReadData(CObjectIStream& in, TObjectPtr object) const
{
	CObjectIStream::ByteBlock block(in);
	BSFree(Get(object));
    char buffer[1024];
    size_t count = block.Read(buffer, sizeof(buffer));
    bytestore* bs = Get(object) = BSNew(count);
    BSWrite(bs, buffer, count);
    while ( (count = block.Read(buffer, sizeof(buffer))) != 0 ) {
        BSWrite(bs, buffer, count);
    }
}

void COctetStringTypeInfo::SkipData(CObjectIStream& in) const
{
    in.SkipByteBlock();
}

TTypeInfo COctetStringTypeInfo::GetTypeInfo(void)
{
    static TTypeInfo typeInfo = 0;
    if ( !typeInfo )
        typeInfo = new COctetStringTypeInfo();
    return typeInfo;
}

//map<COldAsnTypeInfo::TNewProc, COldAsnTypeInfo*> COldAsnTypeInfo::m_Types;

COldAsnTypeInfo::COldAsnTypeInfo(const string& name,
                                 TNewProc newProc, TFreeProc freeProc,
                                 TReadProc readProc, TWriteProc writeProc)
    : CParent(name),
      m_NewProc(newProc), m_FreeProc(freeProc),
      m_ReadProc(readProc), m_WriteProc(writeProc)
{
}

COldAsnTypeInfo::COldAsnTypeInfo(const char* name,
                                 TNewProc newProc, TFreeProc freeProc,
                                 TReadProc readProc, TWriteProc writeProc)
    : CParent(name),
      m_NewProc(newProc), m_FreeProc(freeProc),
      m_ReadProc(readProc), m_WriteProc(writeProc)
{
}

COldAsnTypeInfo::~COldAsnTypeInfo(void)
{
}

/*
TTypeInfo COldAsnTypeInfo::GetTypeInfo(TNewProc newProc, TFreeProc freeProc,
                                       TReadProc readProc, TWriteProc writeProc)
{
    COldAsnTypeInfo*& info = m_Types[newProc];
    if ( info ) {
        if ( info->m_NewProc != newProc || info->m_FreeProc != freeProc ||
             info->m_ReadProc != readProc || info->m_WriteProc != writeProc )
            THROW1_TRACE(runtime_error, "changed ASN.1 procedures pointers");
    }
    else {
        info = new COldAsnTypeInfo(newProc, freeProc, readProc, writeProc);
    }
    return info;
}
*/

size_t COldAsnTypeInfo::GetSize(void) const
{
    return TType::GetSize();
}

bool COldAsnTypeInfo::IsDefault(TConstObjectPtr object) const
{
    return Get(object) == 0;
}

bool COldAsnTypeInfo::Equals(TConstObjectPtr object1,
                             TConstObjectPtr object2) const
{
    return Get(object1) == 0 && Get(object2) == 0;
}

void COldAsnTypeInfo::SetDefault(TObjectPtr dst) const
{
    Get(dst) = 0;
}

void COldAsnTypeInfo::Assign(TObjectPtr , TConstObjectPtr ) const
{
    THROW1_TRACE(runtime_error, "cannot assign non default value");
}

void COldAsnTypeInfo::WriteData(CObjectOStream& out,
                                TConstObjectPtr object) const
{
    if ( !m_WriteProc(Get(object), CObjectOStream::AsnIo(out, GetName()), 0) )
        THROW1_TRACE(runtime_error, "write fault");
}

void COldAsnTypeInfo::ReadData(CObjectIStream& in, TObjectPtr object) const
{
    CObjectIStream::AsnIo io(in, GetName());
    if ( (Get(object) = m_ReadProc(io, 0)) == 0 )
        THROW1_TRACE(runtime_error, "read fault");
}

void COldAsnTypeInfo::SkipData(CObjectIStream& /*in*/) const
{
    THROW1_TRACE(runtime_error, "cannot skip COldAsnTypeInfo");
}

END_NCBI_SCOPE

#endif
