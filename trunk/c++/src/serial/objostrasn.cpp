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
* Revision 1.1  1999/06/15 16:19:51  vasilche
* Added ASN.1 object output stream.
*
* Revision 1.5  1999/06/10 21:06:49  vasilche
* Working binary output and almost working binary input.
*
* Revision 1.4  1999/06/07 20:10:03  vasilche
* Avoid using of numeric_limits.
*
* Revision 1.3  1999/06/07 19:30:27  vasilche
* More bug fixes
*
* Revision 1.2  1999/06/04 20:51:47  vasilche
* First compilable version of serialization.
*
* Revision 1.1  1999/05/19 19:56:55  vasilche
* Commit just in case.
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <serial/objostrasn.hpp>

BEGIN_NCBI_SCOPE

CObjectOStreamAsn::CObjectOStreamAsn(CNcbiOstream& out)
    : m_Output(out), m_Ident(0)
{
}

CObjectOStreamAsn::~CObjectOStreamAsn(void)
{
}

void CObjectOStreamAsn::WriteStd(const char& data)
{
    m_Output << '\'';
    WriteChar(data);
    m_Output << '\'';
}

void CObjectOStreamAsn::WriteChar(char c)
{
    switch ( c ) {
    case '\'':
    case '\"':
    case '\\':
        m_Output << '\\' << c;
        break;
    case '\r':
        m_Output << "\\r";
        break;
    case '\n':
        m_Output << "\\n";
        break;
    default:
        if ( c >= ' ' && c < 0x7f ) {
            m_Output << c;
        }
        else {
            // octal escape sequence
            m_Output << '\\'
                     << ('0' + ((c >> 6) & 3))
                     << ('0' + ((c >> 3) & 7))
                     << ('0' + (c & 7));
        }
        break;
    }
}

void CObjectOStreamAsn::WriteStd(const unsigned char& data)
{
    m_Output << unsigned(data);
}

void CObjectOStreamAsn::WriteStd(const signed char& data)
{
    m_Output << int(data);
}

void CObjectOStreamAsn::WriteStd(const short& data)
{
    m_Output << data;
}

void CObjectOStreamAsn::WriteStd(const unsigned short& data)
{
    m_Output << data;
}

void CObjectOStreamAsn::WriteStd(const int& data)
{
    m_Output << data;
}

void CObjectOStreamAsn::WriteStd(const unsigned int& data)
{
    m_Output << data;
}

void CObjectOStreamAsn::WriteStd(const long& data)
{
    m_Output << data;
}

void CObjectOStreamAsn::WriteStd(const unsigned long& data)
{
    m_Output << data;
}

void CObjectOStreamAsn::WriteStd(const float& data)
{
    m_Output << data;
}

void CObjectOStreamAsn::WriteStd(const double& data)
{
    m_Output << data;
}

void CObjectOStreamAsn::WriteStd(const string& data)
{
    WriteString(data);
}

void CObjectOStreamAsn::WriteStd(char* const& data)
{
    if ( data == 0 ) {
        WriteNull();
    }
    else {
        WriteString(data);
    }
}

void CObjectOStreamAsn::WriteStd(const char* const& data)
{
    if ( data == 0 ) {
        WriteNull();
    }
    else {
        WriteString(data);
    }
}

void CObjectOStreamAsn::WriteNull(void)
{
    m_Output << "null";
}

void CObjectOStreamAsn::WriteIndex(TIndex index)
{
    m_Output << index;
}

void CObjectOStreamAsn::WriteSize(unsigned size)
{
    m_Output << size;
}

void CObjectOStreamAsn::WriteString(const string& str)
{
    m_Output << '\"';
    for ( string::const_iterator i = str.begin(); i != str.end(); ++i ) {
        WriteChar(*i);
    }
    m_Output << '\"';
}

void CObjectOStreamAsn::WriteId(const string& str)
{
    m_Output << str;
}

void CObjectOStreamAsn::WriteMemberName(const string& str)
{
    m_Output << str << ' ';
}

void CObjectOStreamAsn::WritePointer(TConstObjectPtr object,
                                        TTypeInfo typeInfo)
{
    _TRACE("CObjectOStreamAsn::WritePointer(" << unsigned(object) << ", " << typeInfo->GetName() << ")");
    if ( object == 0 ) {
        _TRACE("CObjectOStreamAsn::WritePointer: " << unsigned(object) << ": null");
        WriteNull();
        return;
    }

    COObjectInfo info(m_Objects, object, typeInfo);

    // find if this object is part of another object
    string memberSuffix;
    while ( info.IsMember() ) {
        // member of object
        _TRACE("CObjectOStreamAsn::WritePointer: " << unsigned(object) << ": member " << info.GetMemberInfo().GetName() << " of...");
        memberSuffix = '.' + info.GetMemberInfo().GetName() + memberSuffix;
        info.ToContainerObject();
    }

    const CORootObjectInfo& root = info.GetRootObjectInfo();
    if ( root.IsWritten() ) {
        // put reference on it
        _TRACE("CObjectOStreamAsn::WritePointer: " << unsigned(object) << ": @" << root.GetIndex());
        m_Output << '@';
        WriteIndex(root.GetIndex());
    }
    else {
        // new object
        TTypeInfo realTypeInfo = root.GetTypeInfo();
        if ( typeInfo == realTypeInfo ) {
            _TRACE("CObjectOStreamAsn::WritePointer: " << unsigned(object) << ": new");
        }
        else {
            _TRACE("CObjectOStreamAsn::WritePointer: " << unsigned(object) << ": new " << realTypeInfo->GetName());
            WriteId(realTypeInfo->GetName());
            m_Output << "::=";
        }
        Write(info.GetRootObject(), realTypeInfo);
    }
    m_Output << memberSuffix;
}

void CObjectOStreamAsn::WriteNewLine(void)
{
    m_Output << endl;
    for ( int i = 0; i < m_Ident; ++i )
        m_Output << "  ";
}

void CObjectOStreamAsn::WriteBlockBegin(void)
{
    m_Output << '{';
    ++m_Ident;
    WriteNewLine();
}

void CObjectOStreamAsn::WriteBlockSeparator(const Block& block)
{
    if ( block.GetNextIndex() != 0 ) {
        m_Output << " ,";
        WriteNewLine();
    }
}

void CObjectOStreamAsn::WriteBlockEnd(void)
{
    --m_Ident;
    m_Output << " }";
}

void CObjectOStreamAsn::Begin(FixedBlock& , unsigned )
{
    WriteBlockBegin();
}

void CObjectOStreamAsn::Begin(VarBlock& )
{
    WriteBlockBegin();
}

void CObjectOStreamAsn::Next(FixedBlock& block)
{
    WriteBlockSeparator(block);
}

void CObjectOStreamAsn::Next(VarBlock& block)
{
    WriteBlockSeparator(block);
}

void CObjectOStreamAsn::End(FixedBlock& )
{
    WriteBlockEnd();
}

void CObjectOStreamAsn::End(VarBlock& )
{
    WriteBlockEnd();
}

END_NCBI_SCOPE
