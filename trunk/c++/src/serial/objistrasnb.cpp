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
* Revision 1.36  2000/06/07 19:45:59  vasilche
* Some code cleaning.
* Macros renaming in more clear way.
* BEGIN_NAMED_*_INFO, ADD_*_MEMBER, ADD_NAMED_*_MEMBER.
*
* Revision 1.35  2000/06/01 19:07:04  vasilche
* Added parsing of XML data.
*
* Revision 1.34  2000/05/24 20:08:47  vasilche
* Implemented XML dump.
*
* Revision 1.33  2000/05/09 16:38:39  vasilche
* CObject::GetTypeInfo now moved to CObjectGetTypeInfo::GetTypeInfo to reduce possible errors.
* Added write context to CObjectOStream.
* Inlined most of methods of helping class Member, Block, ByteBlock etc.
*
* Revision 1.32  2000/04/28 16:58:13  vasilche
* Added classes CByteSource and CByteSourceReader for generic reading.
* Added delayed reading of choice variants.
*
* Revision 1.31  2000/04/13 14:50:27  vasilche
* Added CObjectIStream::Open() and CObjectOStream::Open() for easier use.
*
* Revision 1.30  2000/03/29 15:55:28  vasilche
* Added two versions of object info - CObjectInfo and CConstObjectInfo.
* Added generic iterators by class -
* 	CTypeIterator<class>, CTypeConstIterator<class>,
* 	CStdTypeIterator<type>, CStdTypeConstIterator<type>,
* 	CObjectsIterator and CObjectsConstIterator.
*
* Revision 1.29  2000/03/14 14:42:31  vasilche
* Fixed error reporting.
*
* Revision 1.28  2000/03/10 17:59:20  vasilche
* Fixed error reporting.
* Added EOF bug workaround on MIPSpro compiler (not finished).
*
* Revision 1.27  2000/03/07 14:41:34  vasilche
* Removed default argument.
*
* Revision 1.26  2000/03/07 14:06:23  vasilche
* Added stream buffering to ASN.1 binary input.
* Optimized class loading/storing.
* Fixed bugs in processing OPTIONAL fields.
* Added generation of reference counted objects.
*
* Revision 1.25  2000/02/17 20:02:44  vasilche
* Added some standard serialization exceptions.
* Optimized text/binary ASN.1 reading.
* Fixed wrong encoding of StringStore in ASN.1 binary format.
* Optimized logic of object collection.
*
* Revision 1.24  2000/01/10 19:46:40  vasilche
* Fixed encoding/decoding of REAL type.
* Fixed encoding/decoding of StringStore.
* Fixed encoding/decoding of NULL type.
* Fixed error reporting.
* Reduced object map (only classes).
*
* Revision 1.23  2000/01/05 19:43:54  vasilche
* Fixed error messages when reading from ASN.1 binary file.
* Fixed storing of integers with enumerated values in ASN.1 binary file.
* Added TAG support to key/value of map.
* Added support of NULL variant in CHOICE.
*
* Revision 1.22  1999/12/17 19:05:03  vasilche
* Simplified generation of GetTypeInfo methods.
*
* Revision 1.21  1999/10/19 15:41:03  vasilche
* Fixed reference to IOS_BASE
*
* Revision 1.20  1999/10/18 20:21:41  vasilche
* Enum values now have long type.
* Fixed template generation for enums.
*
* Revision 1.19  1999/10/08 21:00:43  vasilche
* Implemented automatic generation of unnamed ASN.1 types.
*
* Revision 1.18  1999/10/04 16:22:17  vasilche
* Fixed bug with old ASN.1 structures.
*
* Revision 1.17  1999/09/24 18:19:18  vasilche
* Removed dependency on NCBI toolkit.
*
* Revision 1.16  1999/09/23 21:16:08  vasilche
* Removed dependance on asn.h
*
* Revision 1.15  1999/09/23 18:56:59  vasilche
* Fixed bugs with overloaded methods in objistr*.hpp & objostr*.hpp
*
* Revision 1.14  1999/09/22 20:11:55  vasilche
* Modified for compilation on IRIX native c++ compiler.
*
* Revision 1.13  1999/09/14 18:54:18  vasilche
* Fixed bugs detected by gcc & egcs.
* Removed unneeded includes.
*
* Revision 1.12  1999/08/16 16:08:31  vasilche
* Added ENUMERATED type.
*
* Revision 1.11  1999/08/13 20:22:57  vasilche
* Fixed lot of bugs in datatool
*
* Revision 1.10  1999/08/13 15:53:51  vasilche
* C++ analog of asntool: datatool
*
* Revision 1.9  1999/07/26 18:31:37  vasilche
* Implemented skipping of unused values.
* Added more useful error report.
*
* Revision 1.8  1999/07/22 20:36:38  vasilche
* Fixed 'using namespace' declaration for MSVC.
*
* Revision 1.7  1999/07/22 19:40:56  vasilche
* Fixed bug with complex object graphs (pointers to members of other objects).
*
* Revision 1.6  1999/07/22 17:33:52  vasilche
* Unified reading/writing of objects in all three formats.
*
* Revision 1.5  1999/07/21 20:02:54  vasilche
* Added embedding of ASN.1 binary output from ToolKit to our binary format.
* Fixed bugs with storing pointers into binary ASN.1
*
* Revision 1.4  1999/07/21 14:20:05  vasilche
* Added serialization of bool.
*
* Revision 1.3  1999/07/09 16:32:54  vasilche
* Added OCTET STRING write/read.
*
* Revision 1.2  1999/07/07 21:15:02  vasilche
* Cleaned processing of string types (string, char*, const char*).
*
* Revision 1.1  1999/07/02 21:31:56  vasilche
* Implemented reading from ASN.1 binary format.
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <serial/objistrasnb.hpp>
#include <serial/classinfo.hpp>
#include <serial/member.hpp>
#include <serial/memberid.hpp>
#include <serial/enumvalues.hpp>
#include <serial/memberlist.hpp>
#if HAVE_NCBI_C
# include <asn.h>
#endif

using namespace NCBI_NS_NCBI::CObjectStreamAsnBinaryDefs;

BEGIN_NCBI_SCOPE


CObjectIStream* CreateObjectIStreamAsnBinary(void)
{
    return new CObjectIStreamAsnBinary();
}


static inline
TByte MakeTagByte(EClass c, bool constructed, ETag tag)
{
    return (c << 6) | (constructed? 0x20: 0) | tag;
}

static inline
TByte MakeTagByte(EClass c, bool constructed)
{
    return (c << 6) | (constructed? 0x20: 0);
}

static const TByte KEndOfContentsByte = 0;

static inline
ETag ExtractTag(TByte byte)
{
    return ETag(byte & 0x1f);
}

static inline
bool IsLongTag(TByte byte)
{
    return ExtractTag(byte) == eLongTag;
}

static inline
bool ExtractConstructed(TByte byte)
{
    return (byte & 0x20) != 0;
}

static inline
TByte ExtractClassAndConstructed(TByte byte)
{
    return byte & 0xE0;
}

CObjectIStreamAsnBinary::CObjectIStreamAsnBinary(void)
{
#if CHECK_STREAM_INTEGRITY
    m_CurrentTagState = eTagStart;
    m_CurrentTagLimit = INT_MAX;
#endif
    m_CurrentTagLength = 0;
}

CObjectIStreamAsnBinary::CObjectIStreamAsnBinary(CNcbiIstream& in)
{
#if CHECK_STREAM_INTEGRITY
    m_CurrentTagState = eTagStart;
    m_CurrentTagLimit = INT_MAX;
#endif
    m_CurrentTagLength = 0;
    Open(in);
}

CObjectIStreamAsnBinary::CObjectIStreamAsnBinary(CNcbiIstream& in,
                                                 bool deleteIn)
{
#if CHECK_STREAM_INTEGRITY
    m_CurrentTagState = eTagStart;
    m_CurrentTagLimit = INT_MAX;
#endif
    m_CurrentTagLength = 0;
    Open(in, deleteIn);
}

ESerialDataFormat CObjectIStreamAsnBinary::GetDataFormat(void) const
{
    return eSerial_AsnBinary;
}

string CObjectIStreamAsnBinary::GetPosition(void) const
{
    return "byte "+NStr::UIntToString(m_Input.GetStreamOffset());
}

#define CATCH_READ_ERROR \
    catch (CSerialEofException& ) { \
        SetFailFlags(eEOF); \
        throw; \
    } \
    catch (...) { \
        SetFailFlags(eReadError); \
        throw; \
    }

#if !CHECK_STREAM_INTEGRITY
inline
#endif
TByte CObjectIStreamAsnBinary::PeekTagByte(size_t index)
{
#if CHECK_STREAM_INTEGRITY
    if ( m_CurrentTagState != eTagStart )
        THROW1_TRACE(runtime_error, "bad PeekTagByte call");
#endif
    //try {
    return m_Input.PeekChar(index);
    //}
    //CATCH_READ_ERROR;
}

#if !CHECK_STREAM_INTEGRITY
inline
#endif
TByte CObjectIStreamAsnBinary::StartTag(void)
{
#if CHECK_STREAM_INTEGRITY
    if ( m_CurrentTagLength != 0 )
        THROW1_TRACE(runtime_error, "bad StartTag call");
#endif
    return PeekTagByte();
}

TTag CObjectIStreamAsnBinary::PeekTag(void)
{
    TByte byte = StartTag();
    ETag sysTag = ExtractTag(byte);
    if ( sysTag != eLongTag ) {
        m_CurrentTagLength = 1;
#if CHECK_STREAM_INTEGRITY
        m_CurrentTagState = eTagParsed;
#endif
        return sysTag;
    }
    TTag tag = 0;
    size_t i = 1;
    const size_t KBitsInByte = 8; // ?
    const size_t KTagBits = sizeof(tag) * KBitsInByte - 1;
    do {
        if ( tag >= (1 << (KTagBits - 7)) ) {
            ThrowError(eOverflow, "tag number is too big");
        }
        byte = PeekTagByte(i++);
        tag = (tag << 7) | (byte & 0x7f);
    } while ( (byte & 0x80) == 0 );
    m_CurrentTagLength = i;
#if CHECK_STREAM_INTEGRITY
    m_CurrentTagState = eTagParsed;
#endif
    return tag;
}

TTag CObjectIStreamAsnBinary::PeekTag(EClass cls, bool constructed)
{
    if ( ExtractClassAndConstructed(PeekTagByte()) !=
         MakeTagByte(cls, constructed) ) {
        ThrowError(eFormatError, "unexpected tag class/constructed");
    }
    return PeekTag();
}

string CObjectIStreamAsnBinary::PeekClassTag(void)
{
    TByte byte = StartTag();
    if ( ExtractTag(byte) != eLongTag ) {
        ThrowError(eFormatError, "long tag expected");
    }
    string name;
    size_t i = 1;
    TByte c;
    while ( ((c = PeekTagByte(i++)) & 0x80) == 0 ) {
        name += char(c);
        if ( i > 1024 ) {
            ThrowError(eOverflow, "tag number is too big");
        }
    }
    m_CurrentTagLength = i;
#if CHECK_STREAM_INTEGRITY
    m_CurrentTagState = eTagParsed;
#endif
    name += char(c & 0x7f);
    return name;
}

TByte CObjectIStreamAsnBinary::PeekAnyTag(void)
{
    TByte fByte = StartTag();
    if ( ExtractTag(fByte) != eLongTag ) {
        m_CurrentTagLength = 1;
#if CHECK_STREAM_INTEGRITY
        m_CurrentTagState = eTagParsed;
#endif
        return fByte;
    }
    size_t i = 1;
    TByte byte;
    do {
        if ( i > 1024 ) {
            ThrowError(eOverflow, "tag number is too big");
        }
        byte = PeekTagByte(i++);
    } while ( (byte & 0x80) == 0 );
    m_CurrentTagLength = i;
#if CHECK_STREAM_INTEGRITY
    m_CurrentTagState = eTagParsed;
#endif
    return fByte;
}

inline
void CObjectIStreamAsnBinary::ExpectSysTag(EClass c, bool constructed,
                                           ETag tag)
{
    _ASSERT(tag != eLongTag);
    if ( StartTag() != MakeTagByte(c, constructed, tag) )
        UnexpectedTag(tag);
    m_CurrentTagLength = 1;
#if CHECK_STREAM_INTEGRITY
    m_CurrentTagState = eTagParsed;
#endif
}

void CObjectIStreamAsnBinary::UnexpectedTag(TTag tag)
{
    ThrowError(eFormatError,
               "unexpected tag: " + NStr::IntToString(m_Input.PeekChar()) +
               ", should be: " + NStr::IntToString(tag));
}

void CObjectIStreamAsnBinary::UnexpectedByte(TByte byte)
{
    ThrowError(eFormatError,
               "byte " + NStr::IntToString(byte) + " expected");
}

inline
void CObjectIStreamAsnBinary::ExpectSysTag(ETag tag)
{
    ExpectSysTag(eUniversal, false, tag);
}

#if !CHECK_STREAM_INTEGRITY
inline
#endif
TByte CObjectIStreamAsnBinary::FlushTag(void)
{
#if CHECK_STREAM_INTEGRITY
    if ( m_CurrentTagState != eTagParsed || m_CurrentTagLength == 0 )
        THROW1_TRACE(runtime_error, "illegal FlushTag call");
#endif
    m_Input.SkipChars(m_CurrentTagLength);
#if CHECK_STREAM_INTEGRITY
    m_CurrentTagState = eLengthValue;
#endif
    //try {
    return m_Input.GetChar();
    //}
    //CATCH_READ_ERROR;
}

#if !CHECK_STREAM_INTEGRITY
inline
#endif
bool CObjectIStreamAsnBinary::PeekIndefiniteLength(void)
{
#if CHECK_STREAM_INTEGRITY
    if ( m_CurrentTagState != eTagParsed )
        THROW1_TRACE(runtime_error, "illegal PeekIndefiniteLength call");
#endif
    //try {
    return TByte(m_Input.PeekChar(m_CurrentTagLength)) == 0x80;
    //}
    //CATCH_READ_ERROR;
}

#if !CHECK_STREAM_INTEGRITY
inline
#endif
void CObjectIStreamAsnBinary::ExpectIndefiniteLength(void)
{
    // indefinite length allowed only for constructed tags
#if CHECK_STREAM_INTEGRITY
    if ( !ExtractConstructed(m_Input.PeekChar()) )
        THROW1_TRACE(runtime_error, "illegal ExpectIndefiniteLength call");
#endif
    if ( FlushTag() != 0x80 ) {
        ThrowError(eFormatError, "indefinite length is expected");
    }
#if CHECK_STREAM_INTEGRITY
    _ASSERT(m_CurrentTagLimit == INT_MAX);
    // save tag limit
    // tag limit is not changed
    m_Limits.push(m_CurrentTagLimit);
    m_CurrentTagState = eTagStart;
    m_CurrentTagLength = 0;
#endif
}

#if !CHECK_STREAM_INTEGRITY
inline
#endif
size_t CObjectIStreamAsnBinary::StartTagData(size_t length)
{
#if CHECK_STREAM_INTEGRITY
    size_t newLimit = m_Input.GetStreamOffset() + length;
    size_t currentLimit = m_CurrentTagLimit;
    _ASSERT(m_CurrentTagLimit == INT_MAX);
    m_Limits.push(currentLimit);
    m_CurrentTagLimit = newLimit;
    m_CurrentTagState = eData;
#endif
    return length;
}

size_t CObjectIStreamAsnBinary::ReadShortLength(void)
{
    TByte byte = FlushTag();
    if ( byte >= 0x80 ) {
        ThrowError(eFormatError, "short length expected");
    }
    return StartTagData(byte);
}

size_t CObjectIStreamAsnBinary::ReadLength(void)
{
    TByte byte = FlushTag();
    if ( byte < 0x80 ) {
        return StartTagData(byte);
    }
    size_t lengthLength = byte - 0x80;
    if ( lengthLength == 0 ) {
        ThrowError(eFormatError, "unexpected indefinite length");
    }
    if ( lengthLength > sizeof(size_t) ) {
        ThrowError(eOverflow, "length overflow");
    }
    //try {
    byte = m_Input.GetChar();
    //}
    //CATCH_READ_ERROR;
    if ( byte == 0 ) {
        ThrowError(eFormatError, "illegal length start");
    }
    if ( size_t(-1) < size_t(0) ) {        // size_t is signed
        // check for sign overflow
        if ( lengthLength == sizeof(size_t) && (byte & 0x80) != 0 ) {
            ThrowError(eOverflow, "length overflow");
        }
    }
    lengthLength--;
    size_t length = byte;
    //try {
    while ( lengthLength-- > 0 )
        length = (length << 8) | TByte(m_Input.GetChar());
    //}
    //CATCH_READ_ERROR;
    return StartTagData(length);
}

void CObjectIStreamAsnBinary::ExpectShortLength(size_t length)
{
    if ( ReadShortLength() != length ) {
        ThrowError(eFormatError, "length expected");
    }
}

#if !CHECK_STREAM_INTEGRITY
inline
#endif
void CObjectIStreamAsnBinary::EndOfTag(void)
{
#if CHECK_STREAM_INTEGRITY
    if ( m_CurrentTagState != eData )
        THROW1_TRACE(runtime_error, "illegal EndOfTag call");
    // check for all bytes read
    if ( m_CurrentTagLimit != INT_MAX ) {
        if ( m_Input.GetStreamOffset() != m_CurrentTagLimit )
            THROW1_TRACE(runtime_error,
                         "illegal EndOfTag call: not all data bytes read");
    }
    m_CurrentTagState = eTagStart;
    m_CurrentTagLength = 0;
    // restore tag limit from stack
    m_CurrentTagLimit = m_Limits.top();
    m_Limits.pop();
    _ASSERT(m_CurrentTagLimit == INT_MAX);
#endif
}

#if !CHECK_STREAM_INTEGRITY
inline
#endif
void CObjectIStreamAsnBinary::ExpectEndOfContent(void)
{
#if CHECK_STREAM_INTEGRITY
    if ( m_CurrentTagState != eTagStart )
        THROW1_TRACE(runtime_error, "illegal ExpectEndOfContent call");
#endif
    ExpectSysTag(eNone);
    if ( FlushTag() != 0 ) {
        ThrowError(eFormatError, "zero length expected");
    }
#if CHECK_STREAM_INTEGRITY
    _ASSERT(m_CurrentTagLimit == INT_MAX);
    // restore tag limit from stack
    m_CurrentTagLimit = m_Limits.top();
    m_Limits.pop();
    _ASSERT(m_CurrentTagLimit == INT_MAX);
    m_CurrentTagState = eTagStart;
    m_CurrentTagLength = 0;
#endif
}

#if !CHECK_STREAM_INTEGRITY
inline
#endif
TByte CObjectIStreamAsnBinary::ReadByte(void)
{
#if CHECK_STREAM_INTEGRITY
    if ( m_CurrentTagState != eData )
        THROW1_TRACE(runtime_error, "illegal ReadByte call");
    if ( m_Input.GetStreamOffset() >= m_CurrentTagLimit )
        ThrowError(eOverflow, "tag size overflow");
#endif
    //try {
    return m_Input.GetChar();
    //}
    //CATCH_READ_ERROR;
}

inline
signed char CObjectIStreamAsnBinary::ReadSByte(void)
{
    return ReadByte();
}

inline
void CObjectIStreamAsnBinary::ExpectByte(TByte byte)
{
    if ( ReadByte() != byte )
        UnexpectedByte(byte);
}

void CObjectIStreamAsnBinary::ReadBytes(char* buffer, size_t count)
{
#if CHECK_STREAM_INTEGRITY
    if ( m_CurrentTagState != eData ) {
        THROW1_TRACE(runtime_error, "illegal ReadBytes call");
    }
#endif
    if ( count == 0 )
        return;
#if CHECK_STREAM_INTEGRITY
    if ( m_Input.GetStreamOffset() + count > m_CurrentTagLimit )
        ThrowError(eOverflow, "tag size overflow");
#endif
    //try {
    m_Input.GetChars(buffer, count);
    //}
    //CATCH_READ_ERROR;
}

void CObjectIStreamAsnBinary::SkipBytes(size_t count)
{
#if CHECK_STREAM_INTEGRITY
    if ( m_CurrentTagState != eData ) {
        THROW1_TRACE(runtime_error, "illegal ReadBytes call");
    }
#endif
    if ( count == 0 )
        return;
#if CHECK_STREAM_INTEGRITY
    if ( m_Input.GetStreamOffset() + count > m_CurrentTagLimit )
        ThrowError(eOverflow, "tag size overflow");
#endif
    //try {
    m_Input.GetChars(count);
    //}
    //CATCH_READ_ERROR;
}

template<typename T>
void ReadStdSigned(CObjectIStreamAsnBinary& in, T& data)
{
    size_t length = in.ReadShortLength();
    if ( length == 0 ) {
        in.ThrowError(in.eFormatError, "zero length of number");
    }
    T n;
    if ( length > sizeof(data) ) {
        // skip
        signed char c = in.ReadSByte();
        if ( c != 0 || c != -1 ) {
            in.ThrowError(in.eOverflow, "overflow error");
        }
        length--;
        while ( length-- > sizeof(data) )
            in.ExpectByte(c);
        length--;
        n = in.ReadSByte();
        if ( ((n ^ c) & 0x80) != 0 ) {
            in.ThrowError(in.eOverflow, "overflow error");
        }
    }
    else {
        length--;
        n = in.ReadSByte();
    }
    while ( length-- > 0 ) {
        n = (n << 8) | in.ReadByte();
    }
    data = n;
    in.EndOfTag();
}

template<typename T>
void ReadStdUnsigned(CObjectIStreamAsnBinary& in, T& data)
{
    size_t length = in.ReadShortLength();
    if ( length == 0 ) {
        in.ThrowError(in.eFormatError, "zero length of number");
    }
    T n;
    if ( length > sizeof(data) ) {
        // skip
        while ( length-- > sizeof(data) )
            in.ExpectByte(0);
        length--;
        n = in.ReadByte();
        if ( (n & 0x80) != 0 ) {
            in.ThrowError(in.eOverflow, "overflow error");
        }
    }
    else if ( length == sizeof(data) ) {
        length--;
        n = in.ReadByte();
        if ( (n & 0x80) != 0 ) {
            in.ThrowError(in.eOverflow, "overflow error");
        }
    }
    else {
        n = 0;
    }
    while ( length-- > 0 ) {
        n = (n << 8) | in.ReadByte();
    }
    data = n;
    in.EndOfTag();
}

bool CObjectIStreamAsnBinary::ReadBool(void)
{
    ExpectSysTag(eBoolean);
    ExpectShortLength(1);
    bool ret = ReadByte() != 0;
    EndOfTag();
    return ret;
}

char CObjectIStreamAsnBinary::ReadChar(void)
{
    ExpectSysTag(eGeneralString);
    ExpectShortLength(1);
    char ret = ReadByte();
    EndOfTag();
    return ret;
}

int CObjectIStreamAsnBinary::ReadInt(void)
{
    ExpectSysTag(eInteger);
    int data;
    ReadStdSigned(*this, data);
    return data;
}

unsigned CObjectIStreamAsnBinary::ReadUInt(void)
{
    ExpectSysTag(eInteger);
    unsigned data;
    ReadStdUnsigned(*this, data);
    return data;
}

long CObjectIStreamAsnBinary::ReadLong(void)
{
#if LONG_MIN == INT_MIN && LONG_MAX == INT_MAX
    return ReadInt();
#else
    ExpectSysTag(eInteger);
    long data;
    ReadStdSigned(*this, data);
    return data;
#endif
}

unsigned long CObjectIStreamAsnBinary::ReadULong(void)
{
#if ULONG_MAX == UINT_MAX
    return ReadUInt();
#else
    ExpectSysTag(eInteger);
    unsigned long data;
    ReadStdUnsigned(*this, data);
    return data;
#endif
}

double CObjectIStreamAsnBinary::ReadDouble(void)
{
    ExpectSysTag(eReal);
    size_t length = ReadShortLength();
    if ( length < 2 ) {
        ThrowError(eFormatError, "too short REAL data");
    }
    if ( length > 32 ) {
        ThrowError(eFormatError, "too long REAL data");
    }

    ExpectByte(eDecimal);
    length--;
    char buffer[32];
    ReadBytes(buffer, length);
    EndOfTag();
    buffer[length] = 0;
    char* endptr;
    double data = strtod(buffer, &endptr);
    if ( *endptr != 0 ) {
        ThrowError(eFormatError, "bad REAL data string");
    }
    return data;
}

void CObjectIStreamAsnBinary::ReadString(string& s)
{
    ExpectSysTag(eVisibleString);
    ReadStringValue(s);
}

void CObjectIStreamAsnBinary::ReadStringStore(string& s)
{
    ExpectSysTag(eApplication, false, eStringStore);
    ReadStringValue(s);
}

void CObjectIStreamAsnBinary::ReadStringValue(string& s)
{
    s.erase();
    size_t length = ReadLength();
    s.reserve(length);
    while ( length > 0 ) {
        char buffer[128];
        size_t count = length;
        if ( count > sizeof(buffer) )
            count = sizeof(buffer);
        ReadBytes(buffer, count);
        s.append(buffer, count);
        length -= count;
    }
    EndOfTag();
}

char* CObjectIStreamAsnBinary::ReadCString(void)
{
    ExpectSysTag(eVisibleString);
    size_t length = ReadLength();
	char* s = static_cast<char*>(malloc(length + 1));
	ReadBytes(s, length);
    EndOfTag();
	s[length] = 0;
    return s;
}

inline
bool CObjectIStreamAsnBinary::HaveMoreElements(void)
{
    return PeekTagByte() != KEndOfContentsByte;
}

void CObjectIStreamAsnBinary::ReadArray(CObjectArrayReader& reader,
                                        TTypeInfo /*arrayType*/,
                                        bool randomOrder,
                                        TTypeInfo /*elementType*/)
{
    ExpectSysTag(eUniversal, true, randomOrder? eSet: eSequence);
    ExpectIndefiniteLength();

    {
        CObjectStackArrayElement e(*this, false);
        
        while ( HaveMoreElements() ) {
            reader.ReadElement(*this);
        }
        
        e.End();
    }

    ExpectEndOfContent();
}

void CObjectIStreamAsnBinary::BeginClass(CObjectStackClass& cls)
{
    ExpectSysTag(eUniversal, true, cls.RandomOrder()? eSet: eSequence);
    ExpectIndefiniteLength();
}

void CObjectIStreamAsnBinary::EndClass(CObjectStackClass& cls)
{
    ExpectEndOfContent();
    cls.End();
}

void CObjectIStreamAsnBinary::UnexpectedMember(TTag tag)
{
    ThrowError(eFormatError,
               "unexpected member: ["+NStr::IntToString(tag)+"]");
}

CObjectIStreamAsnBinary::TMemberIndex
CObjectIStreamAsnBinary::BeginClassMember(CObjectStackClassMember& m,
                                          const CMembers& members)
{
    if ( !HaveMoreElements() )
        return -1;

    TTag tag = PeekTag(eContextSpecific, true);
    ExpectIndefiniteLength();
    TMemberIndex index = members.FindMember(tag);
    if ( index < 0 )
        UnexpectedMember(tag);
    m.SetName(members.GetMemberId(index));
    m.Begin();
    return index;
}

CObjectIStreamAsnBinary::TMemberIndex
CObjectIStreamAsnBinary::BeginClassMember(CObjectStackClassMember& m,
                                          const CMembers& members,
                                          CClassMemberPosition& pos)
{
    if ( !HaveMoreElements() )
        return -1;

    TTag tag = PeekTag(eContextSpecific, true);
    ExpectIndefiniteLength();
    TMemberIndex index = members.FindMember(tag, pos.GetLastIndex());
    if ( index < 0 )
        UnexpectedMember(tag);
    m.SetName(members.GetMemberId(index));
    m.Begin();
    pos.SetLastIndex(index);
    return index;
}

void CObjectIStreamAsnBinary::EndClassMember(CObjectStackClassMember& m)
{
    ExpectEndOfContent();
    m.End();
}

void CObjectIStreamAsnBinary::ReadClassRandom(CObjectClassReader& reader,
                                              TTypeInfo classInfo,
                                              const CMembersInfo& members)
{
    ExpectSysTag(eUniversal, true, eSet);
    ExpectIndefiniteLength();

    TTypeInfo parentClassInfo = classInfo->GetParentTypeInfo();
    if ( parentClassInfo &&
         parentClassInfo != CObjectGetTypeInfo::GetTypeInfo() ) {
        reader.ReadParentClass(*this, parentClassInfo);
    }

    size_t memberCount = members.GetSize();

    if ( HaveMoreElements() ) {
        vector<bool> read(memberCount);
        CObjectStackClassMember m(*this, false);

        do {
            TTag tag = PeekTag(eContextSpecific, true);
            ExpectIndefiniteLength();
            TMemberIndex index = members.FindMember(tag);
            if ( index < 0 )
                UnexpectedMember(tag);
            m.SetName(members.GetMemberId(index));
            
            if ( read[index] )
                DuplicatedMember(members, index);
            read[index] = true;
            reader.ReadMember(*this, members, index);
            
            ExpectEndOfContent();
        } while ( HaveMoreElements() );
        
        m.End();
        // init all absent members
        for ( size_t index = 0; index < memberCount; ++index ) {
            if ( !read[index] ) {
                reader.AssignMemberDefault(*this, members, index);
            }
        }
    }
    else {
        // init all members
        for ( size_t index = 0; index < memberCount; ++index ) {
            reader.AssignMemberDefault(*this, members, index);
        }
    }

    ExpectEndOfContent();
}

void CObjectIStreamAsnBinary::ReadClassSequential(CObjectClassReader& reader,
                                                  TTypeInfo classInfo,
                                                  const CMembersInfo& members)
{
    ExpectSysTag(eUniversal, true, eSequence);
    ExpectIndefiniteLength();

    TTypeInfo parentClassInfo = classInfo->GetParentTypeInfo();
    if ( parentClassInfo &&
         parentClassInfo != CObjectGetTypeInfo::GetTypeInfo() ) {
        reader.ReadParentClass(*this, parentClassInfo);
    }
    
    TMemberIndex pos = -1;
    if ( HaveMoreElements() ) {
        CObjectStackClassMember m(*this, false);

        do {
            TTag tag = PeekTag(eContextSpecific, true);
            ExpectIndefiniteLength();
            TMemberIndex index = members.FindMember(tag, pos);
            if ( index < 0 )
                UnexpectedMember(tag);
            m.SetName(members.GetMemberId(index));

            size_t end = index;
            for ( size_t i = pos + 1; i < end; ++i ) {
                // init missing member
                reader.AssignMemberDefault(*this, members, i);
            }
            pos = index;

            reader.ReadMember(*this, members, index);

            ExpectEndOfContent();
        } while ( HaveMoreElements() );

        m.End();
    }

    // init all absent members
    size_t end = members.GetSize();
    for ( size_t i = pos + 1; i < end; ++i ) {
        reader.AssignMemberDefault(*this, members, i);
    }

    ExpectEndOfContent();
}

#if 0
CObjectIStreamAsnBinary::TMemberIndex
CObjectIStreamAsnBinary::BeginChoiceVariant(CObjectStackChoiceVariant& v,
                                            const CMembers& variants)
{
    TTag tag = PeekTag(eContextSpecific, true);
    ExpectIndefiniteLength();
    TMemberIndex index = variants.FindMember(tag);
    if ( index < 0 )
        UnexpectedMember(tag);
    v.SetName(variants.GetMemberId(index));
    return index;
}

void CObjectIStreamAsnBinary::EndChoiceVariant(CObjectStackChoiceVariant& v)
{
    ExpectEndOfContent();
    v.End();
    _ASSERT(v.GetPrevous());
    v.GetPrevous()->End();
}
#endif

void CObjectIStreamAsnBinary::ReadChoice(CObjectChoiceReader& reader,
                                         TTypeInfo /*choiceInfo*/,
                                         const CMembersInfo& variants)
{
    CObjectStackChoiceVariant v(*this);
    TTag tag = PeekTag(eContextSpecific, true);
    ExpectIndefiniteLength();
    TMemberIndex index = variants.FindMember(tag);
    if ( index < 0 )
        UnexpectedMember(tag);
    v.SetName(variants.GetMemberId(index));
    reader.ReadChoiceVariant(*this, variants, index);
    ExpectEndOfContent();
    v.End();
}

void CObjectIStreamAsnBinary::BeginBytes(ByteBlock& block)
{
	ExpectSysTag(eOctetString);
	block.SetLength(ReadLength());
}

size_t CObjectIStreamAsnBinary::ReadBytes(ByteBlock& ,
                                          char* dst, size_t length)
{
	ReadBytes(dst, length);
	return length;
}

void CObjectIStreamAsnBinary::EndBytes(const ByteBlock& )
{
    EndOfTag();
}

void CObjectIStreamAsnBinary::ReadNull(void)
{
    ExpectSysTag(eNull);
    ExpectShortLength(0);
    EndOfTag();
}

CObjectIStream::EPointerType CObjectIStreamAsnBinary::ReadPointerType(void)
{
    TByte byte = PeekTagByte();
    // variants:
    //    eUniversal,   !constructed, eNull             -> NULL
    //    eApplication,  constructed, eMemberReference  -> member reference
    //    eApplication,  constructed, eLongTag          -> other class
    //    eApplication, !constructed, eObjectReference  -> object reference
    // any other -> this class
    if ( byte == KEndOfContentsByte ) {
        ExpectShortLength(0);
        EndOfTag();
        return eNullPointer;
    }
    else if ( byte == MakeTagByte(eApplication, true, eLongTag) ) {
        return eOtherPointer;
    }
    else if ( byte == MakeTagByte(eApplication, false, eObjectReference) ) {
        return eObjectPointer;
    }
    // by default: try this class
    return eThisPointer;
}

pair<long, bool>
CObjectIStreamAsnBinary::ReadEnum(const CEnumeratedTypeValues& values)
{
    if ( values.IsInteger() ) {
        // allow any integer
        return make_pair(0l, false);
    }
    
    // enum element by value
    ExpectSysTag(eEnumerated);
    long value;
#if LONG_MIN == INT_MIN && LONG_MAX == INT_MAX
    int data;
    ReadStdSigned(*this, data);
    value = data;
#else
    ReadStdSigned(*this, value);
#endif
    values.FindName(value, false); // check value
    return make_pair(value, true);
}

CObjectIStream::TIndex CObjectIStreamAsnBinary::ReadObjectPointer(void)
{
    unsigned data;
    ReadStdUnsigned(*this, data);
    return data;
}

string CObjectIStreamAsnBinary::ReadOtherPointer(void)
{
    string className = PeekClassTag();
    ExpectIndefiniteLength();
    return className;
}

void CObjectIStreamAsnBinary::ReadOtherPointerEnd(void)
{
    ExpectEndOfContent();
}

bool CObjectIStreamAsnBinary::SkipRealValue(void)
{
    if ( PeekTagByte() == 0 && PeekTagByte(1) == 0 )
        return false;
    TByte byte = PeekAnyTag();
    if ( ExtractConstructed(byte) ) {
        // constructed
        ExpectIndefiniteLength();
        while ( SkipRealValue() )
            ;
        ExpectEndOfContent();
    }
    else {
        SkipTagData();
    }
    return true;
}

void CObjectIStreamAsnBinary::SkipValue()
{
    if ( !SkipRealValue() ) {
        ThrowError(eFormatError, "unexpected end of contents");
    }
}

#if HAVE_NCBI_C
unsigned CObjectIStreamAsnBinary::GetAsnFlags(void)
{
    return ASNIO_BIN;
}

void CObjectIStreamAsnBinary::AsnOpen(AsnIo& )
{
#if CHECK_STREAM_INTEGRITY
    if ( m_CurrentTagState != eTagStart ) {
        ThrowError(eIllegalCall, "double tag read");
    }
#endif
}

size_t CObjectIStreamAsnBinary::AsnRead(AsnIo& , char* data, size_t )
{
    //try {
    *data = m_Input.GetChar();
    //}
    //CATCH_READ_ERROR;
    return 1;
}
#endif

void CObjectIStreamAsnBinary::SkipBool(void)
{
    ExpectSysTag(eBoolean);
    ExpectShortLength(1);
    ReadByte();
    EndOfTag();
}

void CObjectIStreamAsnBinary::SkipChar(void)
{
    ExpectSysTag(eGeneralString);
    ExpectShortLength(1);
    ReadByte();
    EndOfTag();
}

void CObjectIStreamAsnBinary::SkipSNumber(void)
{
    ExpectSysTag(eInteger);
    SkipTagData();
}

void CObjectIStreamAsnBinary::SkipFNumber(void)
{
    ExpectSysTag(eReal);
    size_t length = ReadShortLength();
    if ( length < 2 ) {
        ThrowError(eFormatError, "too short REAL data");
    }
    if ( length > 32 ) {
        ThrowError(eFormatError, "too long REAL data");
    }

    ExpectByte(eDecimal);
    length--;
    SkipBytes(length);
    EndOfTag();
}

void CObjectIStreamAsnBinary::SkipString(void)
{
    ExpectSysTag(eVisibleString);
    SkipTagData();
}

void CObjectIStreamAsnBinary::SkipStringStore(void)
{
    ExpectSysTag(eApplication, false, eStringStore);
    SkipTagData();
}

void CObjectIStreamAsnBinary::SkipNull(void)
{
    ExpectSysTag(eNull);
    ExpectShortLength(0);
    EndOfTag();
}

void CObjectIStreamAsnBinary::SkipByteBlock(void)
{
	ExpectSysTag(eOctetString);
    SkipTagData();
}

void CObjectIStreamAsnBinary::SkipTagData(void)
{
    SkipBytes(ReadLength());
    EndOfTag();
}

END_NCBI_SCOPE
