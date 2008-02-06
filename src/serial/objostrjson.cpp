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
* Author: Andrei Gourianov
*
* File Description:
*   JSON object output stream
*
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbi_limits.h>

#include <serial/objostrjson.hpp>
#include <serial/objistr.hpp>
#include <serial/objcopy.hpp>
#include <serial/impl/memberid.hpp>
#include <serial/impl/memberlist.hpp>
#include <serial/enumvalues.hpp>
#include <serial/objhook.hpp>
#include <serial/impl/classinfo.hpp>
#include <serial/impl/choice.hpp>
#include <serial/impl/continfo.hpp>
#include <serial/delaybuf.hpp>
#include <serial/impl/ptrinfo.hpp>
#include <serial/error_codes.hpp>

#include <stdio.h>
#include <math.h>


#define NCBI_USE_ERRCODE_X   Serial_OStream

BEGIN_NCBI_SCOPE


CObjectOStream* CObjectOStream::OpenObjectOStreamJson(CNcbiOstream& out,
                                                     bool deleteOut)
{
    return new CObjectOStreamJson(out, deleteOut);
}



CObjectOStreamJson::CObjectOStreamJson(CNcbiOstream& out, bool deleteOut)
    : CObjectOStream(eSerial_Json, out, deleteOut),
    m_BlockStart(false),
    m_ExpectValue(false),
    m_StringEncoding(eEncoding_Unknown)
{
}

CObjectOStreamJson::~CObjectOStreamJson(void)
{
}

void CObjectOStreamJson::SetDefaultStringEncoding(EEncoding enc)
{
    m_StringEncoding = enc;
}

EEncoding CObjectOStreamJson::GetDefaultStringEncoding(void) const
{
    return m_StringEncoding;
}

string CObjectOStreamJson::GetPosition(void) const
{
    return "line "+NStr::UIntToString(m_Output.GetLine());
}

void CObjectOStreamJson::WriteFileHeader(TTypeInfo type)
{
    StartBlock();
    m_Output.PutEol();
    x_WriteString(type->GetName());
    NameSeparator();
}

void CObjectOStreamJson::EndOfWrite(void)
{
    EndBlock();
    m_Output.PutEol();
    CObjectOStream::EndOfWrite();
}

void CObjectOStreamJson::WriteBool(bool data)
{
    WriteKeywordValue( data ? "true" : "false");
}

void CObjectOStreamJson::WriteChar(char data)
{
    string s;
    s += data;
    WriteString(s);
}

void CObjectOStreamJson::WriteInt4(Int4 data)
{
    WriteKeywordValue(NStr::IntToString(data));
}

void CObjectOStreamJson::WriteUint4(Uint4 data)
{
    WriteKeywordValue(NStr::UIntToString(data));
}

void CObjectOStreamJson::WriteInt8(Int8 data)
{
    WriteKeywordValue(NStr::Int8ToString(data));
}

void CObjectOStreamJson::WriteUint8(Uint8 data)
{
    WriteKeywordValue(NStr::UInt8ToString(data));
}

void CObjectOStreamJson::WriteFloat(float data)
{
    WriteKeywordValue(NStr::DoubleToString(data,FLT_DIG));
}

void CObjectOStreamJson::WriteDouble(double data)
{
    WriteKeywordValue(NStr::DoubleToString(data,DBL_DIG));
}

void CObjectOStreamJson::WriteCString(const char* str)
{
    WriteValue(str);
}

void CObjectOStreamJson::WriteString(const string& str,
                            EStringType type)
{
    WriteValue(str,type);
}

void CObjectOStreamJson::WriteStringStore(const string& s)
{
    WriteString(s);
}

void CObjectOStreamJson::CopyString(CObjectIStream& in)
{
    string s;
    in.ReadStd(s);
    WriteString(s);
}

void CObjectOStreamJson::CopyStringStore(CObjectIStream& in)
{
    string s;
    in.ReadStringStore(s);
    WriteStringStore(s);
}

void CObjectOStreamJson::WriteNullPointer(void)
{
}

void CObjectOStreamJson::WriteObjectReference(TObjectIndex /*index*/)
{
    ThrowError(fNotImplemented, "Not Implemented");
}

void CObjectOStreamJson::WriteOtherBegin(TTypeInfo /*typeInfo*/)
{
    ThrowError(fNotImplemented, "Not Implemented");
}

void CObjectOStreamJson::WriteOtherEnd(TTypeInfo /*typeInfo*/)
{
    ThrowError(fNotImplemented, "Not Implemented");
}

void CObjectOStreamJson::WriteOther(TConstObjectPtr /*object*/, TTypeInfo /*typeInfo*/)
{
    ThrowError(fNotImplemented, "Not Implemented");
}

void CObjectOStreamJson::WriteNull(void)
{
    if (m_ExpectValue) {
        WriteKeywordValue("null");
    }
}

void CObjectOStreamJson::WriteAnyContentObject(const CAnyContentObject& obj)
{
    string obj_name = obj.GetName();
    if (obj_name.empty()) {
        if (!StackIsEmpty() && TopFrame().HasMemberId()) {
            obj_name = TopFrame().GetMemberId().GetName();
        }
        if (obj_name.empty()) {
            ThrowError(fInvalidData, "AnyContent object must have name");
        }
    } else {
        NextElement();
        x_WriteString(obj.GetName());
        NameSeparator();
    }
    const vector<CSerialAttribInfoItem>& attlist = obj.GetAttributes();
    if (attlist.empty()) {
        WriteValue(obj.GetValue());
        return;
    }
    StartBlock();
    vector<CSerialAttribInfoItem>::const_iterator it;
    for ( it = attlist.begin(); it != attlist.end(); ++it) {
        NextElement();
        x_WriteString(it->GetName());
        NameSeparator();
        WriteValue(it->GetValue());
    }
    m_SkippedMemberId = obj_name;
    WriteValue(obj.GetValue());
    EndBlock();
}

void CObjectOStreamJson::CopyAnyContentObject(CObjectIStream& in)
{
    CAnyContentObject obj;
    in.ReadAnyContentObject(obj);
    WriteAnyContentObject(obj);
}


void CObjectOStreamJson::WriteBitString(const CBitString& /*obj*/)
{
    ThrowError(fNotImplemented, "Not Implemented");
}

void CObjectOStreamJson::CopyBitString(CObjectIStream& /*in*/)
{
    ThrowError(fNotImplemented, "Not Implemented");
}

void CObjectOStreamJson::WriteEnum(const CEnumeratedTypeValues& values,
                        TEnumValueType value)
{
    string value_str;
    if (values.IsInteger()) {
        value_str = NStr::IntToString(value);
        const string& name = values.FindName(value, values.IsInteger());
        if (name.empty() || GetWriteNamedIntegersByValue()) {
            WriteKeywordValue(value_str);
        } else {
            WriteValue(name);
        }
    } else {
        value_str = values.FindName(value, values.IsInteger());
        WriteValue(value_str);
    }
}

void CObjectOStreamJson::CopyEnum(const CEnumeratedTypeValues& values,
                        CObjectIStream& in)
{
    TEnumValueType value = in.ReadEnum(values);
    WriteEnum(values, value);
}

void CObjectOStreamJson::WriteClassMember(const CMemberId& memberId,
                                          TTypeInfo memberType,
                                          TConstObjectPtr memberPtr)
{
    if (memberType->GetTypeFamily() == eTypeFamilyContainer) {
        const CContainerTypeInfo* cont =
            dynamic_cast<const CContainerTypeInfo*>(memberType);
        if (cont && cont->GetElementCount(memberPtr) == 0) {
            return;
        }
    }
    CObjectOStream::WriteClassMember(memberId,memberType,memberPtr);
}

bool CObjectOStreamJson::WriteClassMember(const CMemberId& memberId,
                                          const CDelayBuffer& buffer)
{
    return CObjectOStream::WriteClassMember(memberId,buffer);
}


void CObjectOStreamJson::BeginNamedType(TTypeInfo namedTypeInfo)
{
    CObjectOStream::BeginNamedType(namedTypeInfo);
}

void CObjectOStreamJson::EndNamedType(void)
{
    CObjectOStream::EndNamedType();
}


void CObjectOStreamJson::BeginContainer(const CContainerTypeInfo* /*containerType*/)
{
    BeginArray();
}

void CObjectOStreamJson::EndContainer(void)
{
    EndArray();
}

void CObjectOStreamJson::BeginContainerElement(TTypeInfo /*elementType*/)
{
    NextElement();
}

void CObjectOStreamJson::EndContainerElement(void)
{
}


void CObjectOStreamJson::BeginClass(const CClassTypeInfo* /*classInfo*/)
{
    if (FetchFrameFromTop(1).GetNotag()) {
        return;
    }
    StartBlock();
}


void CObjectOStreamJson::EndClass(void)
{
    if (FetchFrameFromTop(1).GetNotag()) {
        return;
    }
    EndBlock();
}

void CObjectOStreamJson::BeginClassMember(const CMemberId& id)
{
    if (id.HasNotag() || id.IsAttlist()) {
        m_SkippedMemberId = id.GetName();
        TopFrame().SetNotag();
        return;
    }
    NextElement();
    WriteMemberId(id);
}

void CObjectOStreamJson::EndClassMember(void)
{
    if (TopFrame().GetNotag()) {
        TopFrame().SetNotag(false);
    }
}


void CObjectOStreamJson::BeginChoice(const CChoiceTypeInfo* /*choiceType*/)
{
    if (FetchFrameFromTop(1).GetNotag()) {
        return;
    }
    StartBlock();
}

void CObjectOStreamJson::EndChoice(void)
{
    if (FetchFrameFromTop(1).GetNotag()) {
        return;
    }
    EndBlock();
}

void CObjectOStreamJson::BeginChoiceVariant(const CChoiceTypeInfo* /*choiceType*/,
                                const CMemberId& id)
{
    if (id.HasNotag() || id.IsAttlist()) {
        m_SkippedMemberId = id.GetName();
        TopFrame().SetNotag();
        return;
    }
    NextElement();
    WriteMemberId(id);
}

void CObjectOStreamJson::EndChoiceVariant(void)
{
    if (TopFrame().GetNotag()) {
        TopFrame().SetNotag(false);
    }
}


void CObjectOStreamJson::WriteBytes(const ByteBlock& /*block*/,
                        const char* /*bytes*/, size_t /*length*/)
{
    ThrowError(fNotImplemented, "Not Implemented");
}

void CObjectOStreamJson::WriteChars(const CharBlock& /*block*/,
                        const char* /*chars*/, size_t /*length*/)
{
    ThrowError(fNotImplemented, "Not Implemented");
}


void CObjectOStreamJson::WriteSeparator(void)
{
}

void CObjectOStreamJson::WriteMemberId(const CMemberId& id)
{
    string name = id.GetName();
    NStr::ReplaceInPlace(name,"-","_");
    x_WriteString(name);
    NameSeparator();
    m_SkippedMemberId.erase();
}

void CObjectOStreamJson::WriteSkippedMember(void)
{
    string name("#");
    name += m_SkippedMemberId;
    NextElement();
    x_WriteString(name);
    NameSeparator();
    m_SkippedMemberId.erase();
}


void CObjectOStreamJson::WriteEscapedChar(char c)
{
    switch ( c ) {
    case '"':
        m_Output.PutString("\\\"");
        break;
    case '\\':
        m_Output.PutString("\\\\");
        break;
    default:
        if ((unsigned int)c < 0x20 || (unsigned int)c >= 0x80) {
            m_Output.PutString("\\u00");
            Uint1 ch = c;
            unsigned hi = ch >> 4;
            unsigned lo = ch & 0xF;
            m_Output.PutChar("0123456789ABCDEF"[hi]);
            m_Output.PutChar("0123456789ABCDEF"[lo]);
        } else {
            m_Output.PutChar(c);
        }
        break;
    }
}

void CObjectOStreamJson::WriteEncodedChar(const char*& src, EStringType type)
{
    EEncoding enc_in( type == eStringTypeUTF8 ? eEncoding_UTF8 : m_StringEncoding);
    EEncoding enc_out(eEncoding_UTF8);

    if (enc_in == enc_out || enc_in == eEncoding_Unknown || (*src & 0x80) == 0) {
        WriteEscapedChar(*src);
    } else {
        CStringUTF8 tmp;
        tmp.Assign(*src,enc_in);
        for ( string::const_iterator t = tmp.begin(); t != tmp.end(); ++t ) {
            m_Output.PutChar(*t);
        }
    }
}

void CObjectOStreamJson::x_WriteString(const string& value, EStringType type)
{
    m_Output.PutChar('\"');
    for (const char* src = value.c_str(); *src; ++src) {
        WriteEncodedChar(src,type);
    }
    m_Output.PutChar('\"');
}

void CObjectOStreamJson::WriteValue(const string& value, EStringType type)
{
    if (!m_ExpectValue && !m_SkippedMemberId.empty()) {
        WriteSkippedMember();
    }
    x_WriteString(value,type);
    m_ExpectValue = false;
}

void CObjectOStreamJson::WriteKeywordValue(const string& value)
{
    m_Output.PutString(value);
    m_ExpectValue = false;
}

void CObjectOStreamJson::StartBlock(void)
{
    if (!m_ExpectValue && !m_SkippedMemberId.empty()) {
        WriteSkippedMember();
    }
    m_Output.PutChar('{');
    m_Output.IncIndentLevel();
    m_BlockStart = true;
    m_ExpectValue = false;
}

void CObjectOStreamJson::EndBlock(void)
{
    m_Output.DecIndentLevel();
    m_Output.PutEol();
    m_Output.PutChar('}');
    m_BlockStart = false;
    m_ExpectValue = false;
}

void CObjectOStreamJson::NextElement(void)
{
    if ( m_BlockStart ) {
        m_BlockStart = false;
    } else {
        m_Output.PutChar(',');
    }
    m_Output.PutEol();
    m_ExpectValue = false;
}

void CObjectOStreamJson::BeginArray(void)
{
    if (!m_ExpectValue && !m_SkippedMemberId.empty()) {
        WriteSkippedMember();
    }
    m_Output.PutChar('[');
    m_Output.IncIndentLevel();
    m_BlockStart = true;
    m_ExpectValue = false;
}

void CObjectOStreamJson::EndArray(void)
{
    m_Output.DecIndentLevel();
    m_Output.PutEol();
    m_Output.PutChar(']');
    m_BlockStart = false;
    m_ExpectValue = false;
}

void CObjectOStreamJson::NameSeparator(void)
{
    m_Output.PutChar(':');
    m_Output.PutChar(' ');
    m_ExpectValue = true;
}

END_NCBI_SCOPE
