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
* Revision 1.15  1999/07/07 21:15:03  vasilche
* Cleaned processing of string types (string, char*, const char*).
*
* Revision 1.14  1999/07/07 19:59:07  vasilche
* Reduced amount of data allocated on heap
* Cleaned ASN.1 structures info
*
* Revision 1.13  1999/07/02 21:31:57  vasilche
* Implemented reading from ASN.1 binary format.
*
* Revision 1.12  1999/07/01 17:55:31  vasilche
* Implemented ASN.1 binary write.
*
* Revision 1.11  1999/06/30 16:04:57  vasilche
* Added support for old ASN.1 structures.
*
* Revision 1.10  1999/06/24 14:44:56  vasilche
* Added binary ASN.1 output.
*
* Revision 1.9  1999/06/16 20:35:32  vasilche
* Cleaned processing of blocks of data.
* Added input from ASN.1 text format.
*
* Revision 1.8  1999/06/16 13:24:54  vakatov
* tiny typo fixed
*
* Revision 1.7  1999/06/15 16:19:49  vasilche
* Added ASN.1 object output stream.
*
* Revision 1.6  1999/06/11 19:14:58  vasilche
* Working binary serialization and deserialization of first test object.
*
* Revision 1.5  1999/06/10 21:06:46  vasilche
* Working binary output and almost working binary input.
*
* Revision 1.4  1999/06/07 20:10:02  vasilche
* Avoid using of numeric_limits.
*
* Revision 1.3  1999/06/07 19:30:25  vasilche
* More bug fixes
*
* Revision 1.2  1999/06/04 20:51:45  vasilche
* First compilable version of serialization.
*
* Revision 1.1  1999/05/19 19:56:53  vasilche
* Commit just in case.
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <serial/objistrb.hpp>
#include <serial/objstrb.hpp>
#include <serial/member.hpp>

BEGIN_NCBI_SCOPE


CObjectIStreamBinary::CObjectIStreamBinary(CNcbiIstream& in)
    : m_Input(in)
{
}

unsigned char CObjectIStreamBinary::ReadByte(void)
{
    char c;
    if ( !m_Input.get(c) ) {
        THROW1_TRACE(runtime_error, "unexpected EOF");
    }
    return c;
}

typedef unsigned char TByte;

runtime_error overflow_error(const type_info& type, bool sign, 
                             const string& bytes)
{
    ERR_POST((sign? "": "un") << "signed number too big to fit in " <<
             type.name() << " bytes: " << bytes);
    return runtime_error((sign? "": "un") +
                         string("signed number too big to fit in ") +
                         type.name() + " bytes: " + bytes);
}

inline
runtime_error overflow_error(const type_info& type, bool sign,
                             TByte c0)
{
    return overflow_error(type, sign, NStr::IntToString(c0));
}

inline
runtime_error overflow_error(const type_info& type, bool sign,
                             TByte c0, TByte c1)
{
    return overflow_error(type, sign,
                          NStr::IntToString(c0) + ' ' +
                          NStr::IntToString(c1));
}

inline
runtime_error overflow_error(const type_info& type, bool sign,
                             TByte c0, TByte c1, TByte c2)
{
    return overflow_error(type, sign,
                          NStr::IntToString(c0) + ' ' +
                          NStr::IntToString(c1) + ' ' +
                          NStr::IntToString(c2));
}

inline
runtime_error overflow_error(const type_info& type, bool sign,
                             TByte c0, TByte c1, TByte c2, TByte c3)
{
    return overflow_error(type, sign,
                          NStr::IntToString(c0) + ' ' +
                          NStr::IntToString(c1) + ' ' +
                          NStr::IntToString(c2) + ' ' +
                          NStr::IntToString(c3));
}

// check whether signByte is sign expansion of highByte
inline
bool isSignExpansion(TByte signByte, TByte highByte)
{
    return (signByte + (highByte >> 7)) == 0;
}

inline
int checkSignedSign(TByte code, TByte highBit, bool sign)
{
    // check if signed number is returned as unsigned
    // mask data bits
    code &= (highBit << 1) - 1;
    if ( sign ) {
        // extend sign
        return int(code ^ highBit) - highBit;
    }
    else {
        return code;
    }
}

inline
int checkUnsignedSign(TByte code, TByte highBit, bool sign)
{
    // check if signed number is returned as unsigned
    if ( sign && (code & highBit) ) {
        throw overflow_error(typeid(unsigned), sign, code);
    }
    // mask data bits
    code &= (highBit << 1) - 1;
    return code;
}

template<typename T>
void ReadStdSigned(CObjectIStreamBinary& in, T& data, bool sign)
{
    TByte code = in.ReadByte();
    if ( !(code & 0x80) ) {
        // one byte: 0xxxxxxx
        data = checkSignedSign(code, 0x40, sign);
    }
    else if ( !(code & 0x40) ) {
        // two bytes: 10xxxxxx xxxxxxxx
        TByte c0 = in.ReadByte();
        T c1 = checkSignedSign(code, 0x20, sign);
        if ( sizeof(T) == 1 ) {
            // check for byte fit
            if ( !isSignExpansion(c1, c0) )
                throw overflow_error(typeid(T), sign, code, c0);
            data = T(c0);
        }
        else {
            data = T((c1 << 8) | c0);
        }
    }
    else if ( !(code & 0x20) ) {
        // three bytes: 110xxxxx xxxxxxxx xxxxxxxx
        TByte c1 = in.ReadByte();
        TByte c0 = in.ReadByte();
        T c2 = checkSignedSign(code, 0x10, sign);
        if ( sizeof(T) == 1 ) {
            // check for byte fit
            if ( !isSignExpansion(c2, c0) || !isSignExpansion(c1, c0) )
                throw overflow_error(typeid(T), sign, code, c1, c0);
            data = T(c0);
        }
        else if ( sizeof(T) == 2 ) {
            // check for two byte fit
            if ( !isSignExpansion(c2, c1) )
                throw overflow_error(typeid(T), sign, code, c1, c0);
            data = T((c1 << 8) | c0);
        }
        else {
            data = T((c2 << 16) | (c1 << 8) | c0);
        }
    }
    else if ( !(code & 0x10) ) {
        // four bytes: 1110xxxx xxxxxxxx xxxxxxxx xxxxxxxx
        TByte c2 = in.ReadByte();
        TByte c1 = in.ReadByte();
        TByte c0 = in.ReadByte();
        T c3 = checkSignedSign(code, 0x08, sign);
        if ( sizeof(T) == 1 ) {
            // check for byte fit
            if ( !isSignExpansion(c3, c0) || !isSignExpansion(c2, c0) ||
                 !isSignExpansion(c1, c0) )
                throw overflow_error(typeid(T), sign, code, c2, c1, c0);
            data = T(c0);
        }
        else if ( sizeof(T) == 2 ) {
            // check for two byte fit
            if ( !isSignExpansion(c3, c1) || !isSignExpansion(c2, c1) )
                throw overflow_error(typeid(T), sign, code, c2, c1, c0);
            data = T((c1 << 8) | c0);
        }
        else {
            data = T((c3 << 24) | (T(c2) << 16) | (c1 << 8) | c0);
        }
    }
    else {
        // 1111xxxx (xxxxxxxx)*
        size_t size = (code & 0xF) + 4;
        if ( size > 16 ) {
            THROW1_TRACE(runtime_error,
                         "reserved number code: " + NStr::IntToString(code));
        }
        // skip high bytes
        while ( size > sizeof(T) ) {
            TByte c = in.ReadByte();
            if ( c ) {
                throw overflow_error(typeid(T), sign,
                                     NStr::IntToString(code) + " ... " +
                                     NStr::IntToString(c));
            }
            --size;
        }
        // read number
        T n = 0;
        while ( size-- ) {
            n = (n << 8) | in.ReadByte();
        }
        data = n;
    }
}

template<typename T>
void ReadStdUnsigned(CObjectIStreamBinary& in, T& data, bool sign)
{
    TByte code = in.ReadByte();
    if ( !(code & 0x80) ) {
        // one byte: 0xxxxxxx
        data = checkUnsignedSign(code, 0x40, sign);
    }
    else if ( !(code & 0x40) ) {
        // two bytes: 10xxxxxx xxxxxxxx
        TByte c0 = in.ReadByte();
        T c1 = checkUnsignedSign(code, 0x20, sign);
        if ( sizeof(T) == 1 ) {
            // check for byte fit
            if ( c1 )
                throw overflow_error(typeid(T), sign, code, c0);
            data = T(c0);
        }
        else {
            data = T((c1 << 8) | c0);
        }
    }
    else if ( !(code & 0x20) ) {
        // three bytes: 110xxxxx xxxxxxxx xxxxxxxx
        TByte c1 = in.ReadByte();
        TByte c0 = in.ReadByte();
        T c2 = checkUnsignedSign(code, 0x10, sign);
        if ( sizeof(T) == 1 ) {
            // check for byte fit
            if ( c2 || c1 )
                throw overflow_error(typeid(T), sign, code, c1, c0);
            data = T(c0);
        }
        else if ( sizeof(T) == 2 ) {
            // check for two byte fit
            if ( c2 )
                throw overflow_error(typeid(T), sign, code, c1, c0);
            data = T((c1 << 8) | c0);
        }
        else {
            data = T((c2 << 16) | (c1 << 8) | c0);
        }
    }
    else if ( !(code & 0x10) ) {
        // four bytes: 1110xxxx xxxxxxxx xxxxxxxx xxxxxxxx
        TByte c2 = in.ReadByte();
        TByte c1 = in.ReadByte();
        TByte c0 = in.ReadByte();
        T c3 = checkUnsignedSign(code, 0x08, sign);
        if ( sizeof(T) == 1 ) {
            // check for byte fit
            if ( c3 || c2 || c1 )
                throw overflow_error(typeid(T), sign, code, c2, c1, c0);
            data = T(c0);
        }
        else if ( sizeof(T) == 2 ) {
            // check for two byte fit
            if ( c3 || c2 )
                throw overflow_error(typeid(T), sign, code, c2, c1, c0);
            data = T((c1 << 8) | c0);
        }
        else {
            data = T((c3 << 24) | (T(c2) << 16) | (c1 << 8) | c0);
        }
    }
    else {
        // 1111xxxx (xxxxxxxx)*
        size_t size = (code & 0xF) + 4;
        if ( size > 16 ) {
            THROW1_TRACE(runtime_error,
                         "reserved number code: " + NStr::IntToString(code));
        }
        // skip high bytes
        while ( size > sizeof(T) ) {
            TByte c = in.ReadByte();
            if ( c ) {
                throw overflow_error(typeid(T), sign,
                                     NStr::IntToString(code) + " ... " +
                                     NStr::IntToString(c));
            }
            --size;
        }
        // read number
        T n = 0;
        while ( size-- ) {
            n = (n << 8) | in.ReadByte();
        }
        data = n;
    }
}

template<typename T>
void ReadStdSigned(CObjectIStreamBinary& in, T& data)
{
    CObjectIStreamBinary::TByte code = in.ReadByte();
    switch ( code ) {
    case CObjectStreamBinaryDefs::eNull:
        data = 0;
        return;
    case CObjectStreamBinaryDefs::eStd_sbyte:
        data = static_cast<signed char>(in.ReadByte());
    case CObjectStreamBinaryDefs::eStd_ubyte:
        {
            unsigned char value = in.ReadByte();
            if ( sizeof(T) == 1 && value > 0x7f )
                THROW1_TRACE(runtime_error,
                             "cannot read unsigned byte to signed type");
            data = value;
            return;
        }
        return;
    case CObjectStreamBinaryDefs::eStd_sordinal:
        ReadStdSigned(in, data, true);
        return;
    case CObjectStreamBinaryDefs::eStd_uordinal:
        ReadStdSigned(in, data, false);
        return;
    default:
        THROW1_TRACE(runtime_error,
                     "bad number code: " + NStr::IntToString(code));
    }
}

template<typename T>
void ReadStdUnsigned(CObjectIStreamBinary& in, T& data)
{
    CObjectIStreamBinary::TByte code = in.ReadByte();
    switch ( code ) {
    case CObjectStreamBinaryDefs::eNull:
        data = 0;
        return;
    case CObjectStreamBinaryDefs::eStd_sbyte:
        {
            signed char value = in.ReadByte();
            if ( value < 0 )
                THROW1_TRACE(runtime_error,
                             "cannot read signed byte to unsigned type");
            data = value;
            return;
        }
    case CObjectStreamBinaryDefs::eStd_ubyte:
        data = in.ReadByte();
        return;
    case CObjectStreamBinaryDefs::eStd_sordinal:
        ReadStdUnsigned(in, data, true);
        return;
    case CObjectStreamBinaryDefs::eStd_uordinal:
        ReadStdUnsigned(in, data, false);
        return;
    default:
        THROW1_TRACE(runtime_error,
                     "bad number code: " + NStr::IntToString(code));
    }
}

template<typename T>
inline
void ReadStdNumber(CObjectIStreamBinary& in, T& data)
{
    if ( T(-1) < T(0) )
        ReadStdSigned(in, data);
    else
        ReadStdUnsigned(in, data);
}

void CObjectIStreamBinary::ReadStd(char& data)
{
    CObjectIStreamBinary::TByte code = ReadByte();
    switch ( code ) {
    case CObjectStreamBinaryDefs::eNull:
        data = 0;
        return;
    case CObjectStreamBinaryDefs::eStd_char:
        data = ReadByte();
        return;
    default:
        THROW1_TRACE(runtime_error,
                     "bad number code: " + NStr::UIntToString(code));
    }
}

void CObjectIStreamBinary::ReadStd(signed char& data)
{
    CObjectIStreamBinary::TByte code = ReadByte();
    switch ( code ) {
    case CObjectStreamBinaryDefs::eNull:
        data = 0;
        return;
    case CObjectStreamBinaryDefs::eStd_sbyte:
        data = ReadByte();
        return;
    case CObjectStreamBinaryDefs::eStd_ubyte:
        if ( (data = ReadByte()) & 0x80 ) {
            // unsigned -> signed overflow
            THROW1_TRACE(runtime_error,
                         "unsigned char doesn't fit in signed char");
        }
        return;
    default:
        THROW1_TRACE(runtime_error,
                     "bad number code: " + NStr::UIntToString(code));
    }
}

void CObjectIStreamBinary::ReadStd(unsigned char& data)
{
    CObjectIStreamBinary::TByte code = ReadByte();
    switch ( code ) {
    case CObjectStreamBinaryDefs::eNull:
        data = 0;
        return;
    case CObjectStreamBinaryDefs::eStd_ubyte:
        data = ReadByte();
        return;
    case CObjectStreamBinaryDefs::eStd_sbyte:
        if ( (data = ReadByte()) & 0x80 ) {
            // signed -> unsigned overflow
            THROW1_TRACE(runtime_error,
                         "signed char doesn't fit in unsigned char");
        }
        return;
    default:
        THROW1_TRACE(runtime_error, 
                     "bad number code: " + NStr::UIntToString(code));
    }
}

void CObjectIStreamBinary::ReadStd(short& data)
{
    ReadStdNumber(*this, data);
}

void CObjectIStreamBinary::ReadStd(unsigned short& data)
{
    ReadStdNumber(*this, data);
}

void CObjectIStreamBinary::ReadStd(int& data)
{
    ReadStdNumber(*this, data);
}

void CObjectIStreamBinary::ReadStd(unsigned int& data)
{
    ReadStdNumber(*this, data);
}

void CObjectIStreamBinary::ReadStd(long& data)
{
    ReadStdNumber(*this, data);
}

void CObjectIStreamBinary::ReadStd(unsigned long& data)
{
    ReadStdNumber(*this, data);
}

void CObjectIStreamBinary::ReadStd(float& )
{
    throw runtime_error("float is not supported");
}

void CObjectIStreamBinary::ReadStd(double& )
{
    throw runtime_error("double is not supported");
}

CObjectIStreamBinary::TIndex CObjectIStreamBinary::ReadIndex(void)
{
    TIndex index;
    ReadStdUnsigned(*this, index, false);
    return index;
}

unsigned CObjectIStreamBinary::ReadSize(void)
{
    unsigned size;
    ReadStdUnsigned(*this, size, false);
    return size;
}

string CObjectIStreamBinary::ReadString(void)
{
    switch ( ReadByte() ) {
    case CObjectStreamBinaryDefs::eNull:
        return string();
    case CObjectStreamBinaryDefs::eStd_string:
        return ReadStringValue();
    default:
        THROW1_TRACE(runtime_error, "invalid string code");
    }
}

const string& CObjectIStreamBinary::ReadStringValue(void)
{
    TIndex index = ReadIndex();
    if ( index == m_Strings.size() ) {
        // new string
        m_Strings.push_back(string());
        string& s = m_Strings.back();
        unsigned length = ReadSize();
        s.reserve(length);
        for ( unsigned i = 0; i < length; ++i )
            s += char(ReadByte());
        return s;
    }
    else if ( index < m_Strings.size() ) {
        return m_Strings[index];
    }
    else {
        ERR_POST(index);
        THROW1_TRACE(runtime_error,
                     "invalid string index: " + NStr::UIntToString(index));
    }
}

void CObjectIStreamBinary::FBegin(Block& block)
{
    TByte code = ReadByte();
    switch ( code ) {
    case CObjectStreamBinaryDefs::eBlock:
        SetFixed(block, ReadSize());
        break;
    case CObjectStreamBinaryDefs::eElement:
    case CObjectStreamBinaryDefs::eEndOfElements:
        m_Input.unget();
        SetNonFixed(block);
        break;
    default:
        THROW1_TRACE(runtime_error, "bad block start byte");
    }
}

bool CObjectIStreamBinary::VNext(const Block& )
{
    switch ( ReadByte() ) {
    case CObjectStreamBinaryDefs::eElement:
        return true;
    case CObjectStreamBinaryDefs::eEndOfElements:
        return false;
    default:
        THROW1_TRACE(runtime_error, "invalid element type");
    }
}

void CObjectIStreamBinary::StartMember(Member& member)
{
    member.SetName(ReadStringValue());
}

TObjectPtr CObjectIStreamBinary::ReadPointer(TTypeInfo declaredType)
{
    _TRACE("CObjectIStreamBinary::ReadPointer(" << declaredType->GetName() << ")");
    switch ( ReadByte() ) {
    case CObjectStreamBinaryDefs::eNull:
        _TRACE("CObjectIStreamBinary::ReadPointer: null");
        return 0;
    case CObjectStreamBinaryDefs::eMemberReference:
        {
            const string& memberName = ReadStringValue();
            _TRACE("CObjectIStreamBinary::ReadPointer: member " << memberName);
            CIObjectInfo info = ReadObjectPointer();
            CTypeInfo::TMemberIndex index =
                info.GetTypeInfo()->FindMember(memberName);
            if ( index < 0 ) {
                THROW1_TRACE(runtime_error, "member not found: " +
                             info.GetTypeInfo()->GetName() + "." + memberName);
            }
            const CMemberInfo* memberInfo =
                info.GetTypeInfo()->GetMemberInfo(index);
            if ( memberInfo->GetTypeInfo() != declaredType ) {
                THROW1_TRACE(runtime_error, "incompatible member type");
            }
            return memberInfo->GetMember(info.GetObject());
        }
    case CObjectStreamBinaryDefs::eObjectReference:
        {
            TIndex index = ReadIndex();
            _TRACE("CObjectIStreamBinary::ReadPointer: @" << index);
            const CIObjectInfo& info = GetRegisteredObject(index);
            if ( info.GetTypeInfo() != declaredType ) {
                THROW1_TRACE(runtime_error, "incompatible object type");
            }
            return info.GetObject();
        }
    case CObjectStreamBinaryDefs::eThisClass:
        {
            _TRACE("CObjectIStreamBinary::ReadPointer: new");
            TObjectPtr object = declaredType->Create();
            RegisterObject(object, declaredType);
            Read(object, declaredType);
            return object;
        }
    default:
        THROW1_TRACE(runtime_error, "invalid object reference code");
    }
}

CIObjectInfo CObjectIStreamBinary::ReadObjectPointer(void)
{
    _TRACE("CObjectIStreamBinary::ReadObjectPointer()");
    switch ( ReadByte() ) {
    case CObjectStreamBinaryDefs::eMemberReference:
        {
            const string& memberName = ReadStringValue();
            _TRACE("CObjectIStreamBinary::ReadObjectPointer: member " <<
                   memberName);
            CIObjectInfo info = ReadObjectPointer();
            CTypeInfo::TMemberIndex index =
                info.GetTypeInfo()->FindMember(memberName);
            if ( index < 0 ) {
                THROW1_TRACE(runtime_error, "member not found: " +
                             info.GetTypeInfo()->GetName() + "." + memberName);
            }
            const CMemberInfo* memberInfo =
                info.GetTypeInfo()->GetMemberInfo(index);
            return CIObjectInfo(memberInfo->GetMember(info.GetObject()),
                                memberInfo->GetTypeInfo());
        }
    case CObjectStreamBinaryDefs::eObjectReference:
        {
            TIndex index = ReadIndex();
            _TRACE("CObjectIStreamBinary::ReadObjectPointer: @" << index);
            return GetRegisteredObject(index);
        }
    case CObjectStreamBinaryDefs::eOtherClass:
        {
            const string& className = ReadStringValue();
            _TRACE("CObjectIStreamBinary::ReadPointer: new " << className);
            TTypeInfo typeInfo = CTypeInfo::GetTypeInfoByName(className);
            TObjectPtr object = typeInfo->Create();
            RegisterObject(object, typeInfo);
            Read(object, typeInfo);
            return CIObjectInfo(object, typeInfo);
        }
    default:
        THROW1_TRACE(runtime_error, "invalid object reference code");
    }
}

void CObjectIStreamBinary::SkipValue()
{
    switch ( ReadByte() ) {
    case CObjectStreamBinaryDefs::eNull:
        return;
    case CObjectStreamBinaryDefs::eStd_char:
    case CObjectStreamBinaryDefs::eStd_ubyte:
    case CObjectStreamBinaryDefs::eStd_sbyte:
        ReadByte();
        return;
    case CObjectStreamBinaryDefs::eStd_sordinal:
        {
            long dummy;
            ReadStdSigned(*this, dummy, true);
        }
        return;
    case CObjectStreamBinaryDefs::eStd_uordinal:
        {
            unsigned long dummy;
            ReadStdUnsigned(*this, dummy, false);
        }
        return;
    case CObjectStreamBinaryDefs::eStd_float:
        THROW1_TRACE(runtime_error, "floating point numbers are not supported");
        return;
    case CObjectStreamBinaryDefs::eObjectReference:
        ReadIndex();
        return;
    case CObjectStreamBinaryDefs::eMemberReference:
        ReadStringValue();
        SkipObjectPointer();
        return;
    case CObjectStreamBinaryDefs::eThisClass:
        RegisterInvalidObject();
        SkipObjectData();
        return;
    case CObjectStreamBinaryDefs::eOtherClass:
        ReadStringValue();
        RegisterInvalidObject();
        SkipObjectData();
        return;
    case CObjectStreamBinaryDefs::eBlock:
        SkipBlock();
        return;
    default:
        THROW1_TRACE(runtime_error, "invalid value code");
    }
}

void CObjectIStreamBinary::SkipObjectPointer(void)
{
    switch ( ReadByte() ) {
    case CObjectStreamBinaryDefs::eObjectReference:
        ReadIndex();
        return;
    case CObjectStreamBinaryDefs::eMemberReference:
        ReadStringValue();
        SkipObjectPointer();
        return;
    case CObjectStreamBinaryDefs::eOtherClass:
        ReadStringValue();
        RegisterInvalidObject();
        SkipObjectData();
        return;
    default:
        THROW1_TRACE(runtime_error, "invalid value code");
    }
}

void CObjectIStreamBinary::SkipObjectData(void)
{
    Block block(*this);
    while ( block.Next() ) {
        Member m(*this);
        SkipValue();
    }
}

void CObjectIStreamBinary::SkipBlock(void)
{
    unsigned count = ReadSize();
    for ( unsigned i = 0; i < count; ++i )
        SkipValue();
}

END_NCBI_SCOPE
