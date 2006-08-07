/* $Id$
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
 * Author:  Vladimir Soussov
 *
 * File Description:  Type conversions
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbitime.hpp>

#include <dbapi/driver/exception.hpp>
#include <dbapi/driver/util/numeric_convert.hpp>
#include "memory_store.hpp"

#include <dbapi/driver/types.hpp>

#include <string.h>


BEGIN_NCBI_SCOPE


/////////////////////////////////////////////////////////////////////////////
//  CDB_Object::
//

CDB_Object::~CDB_Object()
{
    return;
}

void CDB_Object::AssignNULL()
{
    m_Null = true;
}


CDB_Object* CDB_Object::Create(EDB_Type type, size_t size)
{
    switch ( type ) {
    case eDB_Int             : return new CDB_Int           ();
    case eDB_SmallInt        : return new CDB_SmallInt      ();
    case eDB_TinyInt         : return new CDB_TinyInt       ();
    case eDB_BigInt          : return new CDB_BigInt        ();
    case eDB_VarChar         : return new CDB_VarChar       ();
    case eDB_Char            : return new CDB_Char      (size);
    case eDB_VarBinary       : return new CDB_VarBinary     ();
    case eDB_Binary          : return new CDB_Binary    (size);
    case eDB_Float           : return new CDB_Float         ();
    case eDB_Double          : return new CDB_Double        ();
    case eDB_DateTime        : return new CDB_DateTime      ();
    case eDB_SmallDateTime   : return new CDB_SmallDateTime ();
    case eDB_Text            : return new CDB_Text          ();
    case eDB_Image           : return new CDB_Image         ();
    case eDB_Bit             : return new CDB_Bit           ();
    case eDB_Numeric         : return new CDB_Numeric       ();
    case eDB_LongBinary      : return new CDB_LongBinary(size);
    case eDB_LongChar        : return new CDB_LongChar  (size);
    case eDB_UnsupportedType : break;
    }
    DATABASE_DRIVER_ERROR( "unknown type", 2 );
}


const char* CDB_Object::GetTypeName(EDB_Type db_type)
{
    switch ( db_type ) {
    case eDB_Int             : return "DB_Int";
    case eDB_SmallInt        : return "DB_SmallInt";
    case eDB_TinyInt         : return "DB_TinyInt";
    case eDB_BigInt          : return "DB_BigInt";
    case eDB_VarChar         : return "DB_VarChar";
    case eDB_Char            : return "DB_Char";
    case eDB_VarBinary       : return "DB_VarBinary";
    case eDB_Binary          : return "DB_Binary";
    case eDB_Float           : return "DB_Float";
    case eDB_Double          : return "DB_Double";
    case eDB_DateTime        : return "DB_DateTime";
    case eDB_SmallDateTime   : return "DB_SmallDateTime";
    case eDB_Text            : return "DB_Text";
    case eDB_Image           : return "DB_Image";
    case eDB_Bit             : return "DB_Bit";
    case eDB_Numeric         : return "DB_Numeric";
    case eDB_LongBinary      : return "DB_LongBinary";
    case eDB_LongChar        : return "DB_LongChar";
    case eDB_UnsupportedType : return "DB_UnsupportedType";
    }

    DATABASE_DRIVER_ERROR( "unknown type", 2 );

    return NULL;
}

/////////////////////////////////////////////////////////////////////////////
//  CDB_Int::
//

EDB_Type CDB_Int::GetType() const
{
    return eDB_Int;
}

CDB_Object* CDB_Int::Clone() const
{
    return m_Null ? new CDB_Int : new CDB_Int(m_Val);
}

void CDB_Int::AssignValue(CDB_Object& v)
{
    switch( v.GetType() ) {
        case eDB_Int     : *this= (CDB_Int&)v; break;
        case eDB_SmallInt: *this= ((CDB_SmallInt&)v).Value(); break;
        case eDB_TinyInt : *this= ((CDB_TinyInt &)v).Value(); break;
        default:
            DATABASE_DRIVER_ERROR( "wrong type of CDB_Object", 2 );
    }

}


/////////////////////////////////////////////////////////////////////////////
//  CDB_SmallInt::
//

EDB_Type CDB_SmallInt::GetType() const
{
    return eDB_SmallInt;
}

CDB_Object* CDB_SmallInt::Clone() const
{
    return m_Null ? new CDB_SmallInt : new CDB_SmallInt(m_Val);
}

void CDB_SmallInt::AssignValue(CDB_Object& v)
{
    switch( v.GetType() ) {
        case eDB_SmallInt: *this= (CDB_SmallInt&)v; break;
        case eDB_TinyInt : *this= ((CDB_TinyInt &)v).Value(); break;
        default:
            DATABASE_DRIVER_ERROR( "wrong type of CDB_Object", 2 );
    }
    *this= (CDB_SmallInt&)v;
}

/////////////////////////////////////////////////////////////////////////////
//  CDB_TinyInt::
//

EDB_Type CDB_TinyInt::GetType() const
{
    return eDB_TinyInt;
}

CDB_Object* CDB_TinyInt::Clone() const
{
    return m_Null ? new CDB_TinyInt : new CDB_TinyInt(m_Val);
}

void CDB_TinyInt::AssignValue(CDB_Object& v)
{
    CHECK_DRIVER_ERROR( v.GetType() != eDB_TinyInt, "wrong type of CDB_Object", 2 );

    *this= (CDB_TinyInt&)v;
}


/////////////////////////////////////////////////////////////////////////////
//  CDB_BigInt::
//

EDB_Type CDB_BigInt::GetType() const
{
    return eDB_BigInt;
}

CDB_Object* CDB_BigInt::Clone() const
{
    return m_Null ? new CDB_BigInt : new CDB_BigInt(m_Val);
}

void CDB_BigInt::AssignValue(CDB_Object& v)
{
    switch( v.GetType() ) {
        case eDB_BigInt  : *this= (CDB_BigInt&)v; break;
        case eDB_Int     : *this= ((CDB_Int     &)v).Value(); break;
        case eDB_SmallInt: *this= ((CDB_SmallInt&)v).Value(); break;
        case eDB_TinyInt : *this= ((CDB_TinyInt &)v).Value(); break;
        default:
            DATABASE_DRIVER_ERROR( "wrong type of CDB_Object", 2 );
    }
}

/////////////////////////////////////////////////////////////////////////////
//  CDB_VarChar::
//

CDB_VarChar& CDB_VarChar::SetValue(const string& s)
{
    m_Null = false;
    m_Size = s.copy(m_Val, sizeof(m_Val) - 1);
    m_Val[m_Size] = '\0';
    return *this;
}


CDB_VarChar& CDB_VarChar::SetValue(const char* s)
{
    if ( s ) {
        for (m_Size = 0;  (m_Size < sizeof(m_Val) - 1)  &&  (*s != '\0');
             ++s) {
            m_Val[m_Size++] = *s;
        }
        m_Val[m_Size] = '\0';
        m_Null = false;
    }
    else {
        m_Null = true;
    }
    return *this;
}


CDB_VarChar& CDB_VarChar::SetValue(const char* s, size_t l)
{
    if ( s ) {
        m_Size = l < sizeof(m_Val) ? l : sizeof(m_Val) - 1;
        if ( m_Size ) {
            memcpy(m_Val, s, m_Size);
        }
        m_Val[m_Size] = '\0';
        m_Null = false;
    }
    else {
        m_Null = true;
    }
    return *this;
}


EDB_Type CDB_VarChar::GetType() const
{
    return eDB_VarChar;
}


CDB_Object* CDB_VarChar::Clone() const
{
    return m_Null ? new CDB_VarChar : new CDB_VarChar(m_Val);
}


void CDB_VarChar::AssignValue(CDB_Object& v)
{
    CHECK_DRIVER_ERROR( v.GetType() != eDB_VarChar, "wrong type of CDB_Object", 2 );

    *this= (CDB_VarChar&)v;
}


/////////////////////////////////////////////////////////////////////////////
//  CDB_Char::
//


CDB_Char::CDB_Char(size_t s) : CDB_Object(true)
{
    m_Size = (s < 1) ? 1 : (s > kMaxCharSize ? kMaxCharSize : s);
    m_Val  = new char[m_Size + 1];
    memset(m_Val, ' ', m_Size);
    m_Val[m_Size] = '\0';
}


CDB_Char::CDB_Char(size_t s, const string& v) :  CDB_Object(false)
{
    m_Size = (s < 1) ? 1 : (s > kMaxCharSize ? kMaxCharSize : s);
    m_Val = new char[m_Size + 1];
    size_t l = v.copy(m_Val, m_Size);
    if (l < m_Size) {
        memset(m_Val + l, ' ', m_Size - l);
    }
    m_Val[m_Size] = '\0';
}


CDB_Char::CDB_Char(size_t len, const char* str) :  CDB_Object(str == 0)
{
    m_Size = (len < 1) ? 1 : (len > kMaxCharSize ? kMaxCharSize : len);
    m_Val = new char[m_Size + 1];

    if ( str ) {
        size_t l;
        for (l = 0;  (l < m_Size)  &&  (*str != '\0');  ++str) {
            m_Val[l++] = *str;
        }
        if (l < m_Size) {
            memset(m_Val + l, ' ', m_Size - l);
        }
    } else {
        memset(m_Val, ' ', m_Size);
    }

    m_Val[m_Size] = '\0';
}


CDB_Char::CDB_Char(const CDB_Char& v)
{
    m_Null = v.m_Null;
    m_Size = v.m_Size;
    m_Val = new char[m_Size + 1];
    memcpy(m_Val, v.m_Val, m_Size + 1);
}


CDB_Char& CDB_Char::operator= (const CDB_Char& v)
{
    m_Null = v.m_Null;
    size_t l = (m_Size > v.m_Size) ? v.m_Size : m_Size;
    memmove(m_Val, v.m_Val, l);
    if (l < m_Size)
        memset(m_Val + l, ' ', m_Size - l);
    return *this;
}


CDB_Char& CDB_Char::operator= (const string& v)
{
    m_Null = false;
    size_t l = v.copy(m_Val, m_Size);
    if (l < m_Size)
        memset(m_Val + l, ' ', m_Size - l);
    return *this;
}


CDB_Char& CDB_Char::operator= (const char* v)
{
    if (v == 0) {
        m_Null = true;
    }
    else {
        m_Null = false;
        size_t l;

        for (l = 0;  (l < m_Size)  &&  (*v != '\0');  ++v) {
            m_Val[l++] = *v;
        }
        if (l < m_Size)
            memset(m_Val + l, ' ', m_Size - l);
    }
    return *this;
}


void CDB_Char::SetValue(const char* str, size_t len)
{
    if ( str ) {
        if (len >= m_Size) {
            memcpy(m_Val, str, m_Size);
        }
        else {
            if ( len ) {
                memcpy(m_Val, str, len);
            }
            memset(m_Val + len, ' ', m_Size - len);
        }
        m_Null = false;
    }
    else {
        m_Null = true;
    }
}


EDB_Type CDB_Char::GetType() const
{
    return eDB_Char;
}

CDB_Object* CDB_Char::Clone() const
{
    return new CDB_Char(*this);
}

void CDB_Char::AssignValue(CDB_Object& v)
{
    CHECK_DRIVER_ERROR( v.GetType() != eDB_Char, "wrong type of CDB_Object", 2 );

    register CDB_Char& cv= (CDB_Char&)v;
    if(m_Size < cv.m_Size) {
        delete [] m_Val;
        m_Val= new char[cv.m_Size+1];
    }
    m_Size= cv.m_Size;
    if(cv.IsNULL()) {
        m_Null= true;
    }
    else {
        m_Null= false;
        memcpy(m_Val, cv.m_Val, m_Size+1);
    }
}

CDB_Char::~CDB_Char()
{
    try {
        delete [] m_Val;
    }
    NCBI_CATCH_ALL( NCBI_CURRENT_FUNCTION )
}

/////////////////////////////////////////////////////////////////////////////
//  CDB_LongChar::
//


CDB_LongChar::CDB_LongChar(size_t s) : CDB_Object(true)
{
    m_Size = (s < 1) ? 1 : s;
    m_Val  = new char[m_Size + 1];
}


CDB_LongChar::CDB_LongChar(size_t s, const string& v) :  CDB_Object(false)
{
    m_Size = (s < 1) ? K8_1 : s;
    m_Val = new char[m_Size + 1];
    size_t l = v.copy(m_Val, m_Size);
    m_Val[l] = '\0';
}


CDB_LongChar::CDB_LongChar(size_t len, const char* str) :  CDB_Object(str == 0)
{
    m_Size = (len < 1) ? K8_1 : len;
    m_Val = new char[m_Size + 1];

    if(str) strncpy(m_Val, str, m_Size);
    m_Val[m_Size] = '\0';
}


CDB_LongChar::CDB_LongChar(const CDB_LongChar& v)
{
    m_Null = v.m_Null;
    m_Size = v.m_Size;
    m_Val = new char[m_Size + 1];
    memcpy(m_Val, v.m_Val, m_Size + 1);
}


CDB_LongChar& CDB_LongChar::operator= (const CDB_LongChar& v)
{
    m_Null = v.m_Null;
    size_t l = (m_Size > v.m_Size) ? v.m_Size : m_Size;
    memmove(m_Val, v.m_Val, l);
    m_Val[l]= '\0';
    return *this;
}


CDB_LongChar& CDB_LongChar::operator= (const string& v)
{
    m_Null = false;
    size_t l = v.copy(m_Val, m_Size);
    m_Val[l]= '\0';
    return *this;
}


CDB_LongChar& CDB_LongChar::operator= (const char* v)
{
    if (v == 0) {
        m_Null = true;
    }
    else {
        m_Null = false;
        size_t l;

        for (l = 0;  (l < m_Size)  &&  (*v != '\0');  ++v) {
            m_Val[l++] = *v;
        }
        m_Val[l]= '\0';
    }
    return *this;
}


void CDB_LongChar::SetValue(const char* str, size_t len)
{
    if ( str ) {
        if (len >= m_Size) {
            memcpy(m_Val, str, m_Size);
            m_Val[m_Size]= '\0';
        }
        else {
            if ( len ) {
                memcpy(m_Val, str, len);
            }
            m_Val[len]= '\0';
        }
        m_Null = false;
    }
    else {
        m_Null = true;
    }
}


EDB_Type CDB_LongChar::GetType() const
{
    return eDB_LongChar;
}


CDB_Object* CDB_LongChar::Clone() const
{
    return new CDB_LongChar(*this);
}


void CDB_LongChar::AssignValue(CDB_Object& v)
{
    CHECK_DRIVER_ERROR( v.GetType() != eDB_LongChar, "wrong type of CDB_Object", 2 );

    register CDB_LongChar& cv= (CDB_LongChar&)v;
    if(m_Size < cv.m_Size) {
        delete [] m_Val;
        m_Val= new char[cv.m_Size+1];
    }
    m_Size= cv.m_Size;
    if(cv.IsNULL()) {
        m_Null= true;
    }
    else {
        m_Null= false;
        memcpy(m_Val, cv.m_Val, m_Size+1);
    }
}


CDB_LongChar::~CDB_LongChar()
{
    try {
        delete [] m_Val;
    }
    NCBI_CATCH_ALL( NCBI_CURRENT_FUNCTION )
}


/////////////////////////////////////////////////////////////////////////////
//  CDB_VarBinary::
//


void CDB_VarBinary::SetValue(const void* v, size_t l)
{
    if (v  &&  l) {
        m_Size = l > sizeof(m_Val) ? sizeof(m_Val) : l;
        memcpy(m_Val, v, m_Size);
        m_Null = false;
    }
    else {
        m_Null = true;
    }
}


EDB_Type CDB_VarBinary::GetType() const
{
    return eDB_VarBinary;
}


CDB_Object* CDB_VarBinary::Clone() const
{
    return m_Null ? new CDB_VarBinary : new CDB_VarBinary(m_Val, m_Size);
}


void CDB_VarBinary::AssignValue(CDB_Object& v)
{
    CHECK_DRIVER_ERROR( v.GetType() != eDB_VarBinary, "wrong type of CDB_Object", 2 );

    *this= (CDB_VarBinary&)v;
}

/////////////////////////////////////////////////////////////////////////////
//  CDB_Binary::
//


CDB_Binary::CDB_Binary(size_t s) : CDB_Object(true)
{
    m_Size = (s < 1) ? 1 : (s > kMaxBinSize ? kMaxBinSize : s);
    m_Val = new unsigned char[m_Size];
    memset(m_Val, 0, m_Size);
}


CDB_Binary::CDB_Binary(size_t s, const void* v, size_t v_size)
{
    m_Size = (s == 0) ? 1 : (s > kMaxBinSize ? kMaxBinSize : s);
    m_Val  = new unsigned char[m_Size];
    SetValue(v, v_size);
}


CDB_Binary::CDB_Binary(const CDB_Binary& v)
{
    m_Null = v.m_Null;
    m_Size = v.m_Size;
    m_Val = new unsigned char[m_Size];
    memcpy(m_Val, v.m_Val, m_Size);
}


void CDB_Binary::SetValue(const void* v, size_t v_size)
{
    if (v  &&  v_size) {
        memcpy(m_Val, v, (v_size > m_Size) ? m_Size : v_size);
        if (v_size < m_Size) {
            memset(m_Val + v_size, 0, m_Size - v_size);
        }
        m_Null = false;
    } else {
        m_Null = true;
    }
}


CDB_Binary& CDB_Binary::operator= (const CDB_Binary& v)
{
    m_Null = v.m_Null;
    size_t l = (m_Size > v.m_Size) ? v.m_Size : m_Size;
    memmove(m_Val, v.m_Val, l);
    if (l < m_Size) {
        memset(m_Val+l, 0, m_Size - l);
    }
    return *this;
}


EDB_Type CDB_Binary::GetType() const
{
    return eDB_Binary;
}


CDB_Object* CDB_Binary::Clone() const
{
    return m_Null ? new CDB_Binary(m_Size) : new CDB_Binary(*this);
}


void CDB_Binary::AssignValue(CDB_Object& v)
{
    CHECK_DRIVER_ERROR( v.GetType() != eDB_Binary, "wrong type of CDB_Object", 2 );

    register CDB_Binary& cv= (CDB_Binary&)v;
    if(m_Size < cv.m_Size) {
        delete [] m_Val;
        m_Val= new unsigned char[cv.m_Size];
    }
    m_Size= cv.m_Size;
    if(cv.IsNULL()) {
        m_Null= true;
    }
    else {
        m_Null= false;
        memcpy(m_Val, cv.m_Val, m_Size);
    }
}


CDB_Binary::~CDB_Binary()
{
    try {
        delete [] m_Val;
    }
    NCBI_CATCH_ALL( NCBI_CURRENT_FUNCTION )
}

/////////////////////////////////////////////////////////////////////////////
//  CDB_LongBinary::
//


CDB_LongBinary::CDB_LongBinary(size_t s) : CDB_Object(true)
{
    m_Size = (s < 1) ? 1 : s;
    m_Val = new unsigned char[m_Size];
    m_DataSize= 0;
}


CDB_LongBinary::CDB_LongBinary(size_t s, const void* v, size_t v_size)
{
    m_Size = (s == 0) ? K8_1 : s;
    m_Val  = new unsigned char[m_Size];
    SetValue(v, v_size);
}


CDB_LongBinary::CDB_LongBinary(const CDB_LongBinary& v)
{
    m_Null = v.m_Null;
    m_Size = v.m_Size;
    m_DataSize= v.m_DataSize;
    m_Val = new unsigned char[m_Size];
    memcpy(m_Val, v.m_Val, m_DataSize);
}


void CDB_LongBinary::SetValue(const void* v, size_t v_size)
{
    if (v  &&  v_size) {
        m_DataSize= (v_size > m_Size) ? m_Size : v_size;
        memcpy(m_Val, v, m_DataSize);
        m_Null = false;
    } else {
        m_Null = true;
        m_DataSize= 0;
    }
}


CDB_LongBinary& CDB_LongBinary::operator= (const CDB_LongBinary& v)
{
    m_Null = v.m_Null;
    m_DataSize = (m_Size > v.m_DataSize) ? v.m_DataSize : m_Size;
    if(m_DataSize) {
        memmove(m_Val, v.m_Val, m_DataSize);
    }
    return *this;
}


EDB_Type CDB_LongBinary::GetType() const
{
    return eDB_LongBinary;
}


CDB_Object* CDB_LongBinary::Clone() const
{
    return new CDB_LongBinary(*this);
}


void CDB_LongBinary::AssignValue(CDB_Object& v)
{
    CHECK_DRIVER_ERROR(
        v.GetType() != eDB_LongBinary,
        "wrong type of CDB_Object",
        2 );
    register CDB_LongBinary& cv= (CDB_LongBinary&)v;
    if(cv.IsNULL()) {
        m_Null= true;
    }
    else {
        m_Null= false;
        m_DataSize= (m_Size < cv.m_DataSize)? m_Size : cv.m_DataSize;
        memcpy(m_Val, cv.m_Val, m_DataSize);
    }
}


CDB_LongBinary::~CDB_LongBinary()
{
    try {
        delete [] m_Val;
    }
    NCBI_CATCH_ALL( NCBI_CURRENT_FUNCTION )
}


/////////////////////////////////////////////////////////////////////////////
//  CDB_Float::
//


CDB_Float& CDB_Float::operator= (const float& i)
{
    m_Null = false;
    m_Val  = i;
    return *this;
}


EDB_Type CDB_Float::GetType() const
{
    return eDB_Float;
}

CDB_Object* CDB_Float::Clone() const
{
    return m_Null ? new CDB_Float : new CDB_Float(m_Val);
}

void CDB_Float::AssignValue(CDB_Object& v)
{
    switch( v.GetType() ) {
        case eDB_Float   : *this= (CDB_Float&)v; break;
        case eDB_SmallInt: *this= ((CDB_SmallInt&)v).Value(); break;
        case eDB_TinyInt : *this= ((CDB_TinyInt &)v).Value(); break;
        default:
            DATABASE_DRIVER_ERROR( "wrong type of CDB_Object", 2 );
    }
}

/////////////////////////////////////////////////////////////////////////////
//  CDB_Double::
//


CDB_Double& CDB_Double::operator= (const double& i)
{
    m_Null = false;
    m_Val  = i;
    return *this;
}

EDB_Type CDB_Double::GetType() const
{
    return eDB_Double;
}

CDB_Object* CDB_Double::Clone() const
{
    return m_Null ? new CDB_Double : new CDB_Double(m_Val);
}

void CDB_Double::AssignValue(CDB_Object& v)
{
    switch( v.GetType() ) {
        case eDB_Double  : *this= (CDB_Double&)v; break;
        case eDB_Float   : *this= ((CDB_Float   &)v).Value(); break;
        case eDB_Int     : *this= ((CDB_Int     &)v).Value(); break;
        case eDB_SmallInt: *this= ((CDB_SmallInt&)v).Value(); break;
        case eDB_TinyInt : *this= ((CDB_TinyInt &)v).Value(); break;
        default:
            DATABASE_DRIVER_ERROR( "wrong type of CDB_Object", 2 );
    }
}

/////////////////////////////////////////////////////////////////////////////
//  CDB_Stream::
//

CDB_Stream::CDB_Stream()
    : CDB_Object(true)
{
    m_Store = new CMemStore;
}

CDB_Stream& CDB_Stream::Assign(const CDB_Stream& v)
{
    m_Null = v.m_Null;
    m_Store->Truncate();
    if ( !m_Null ) {
        char buff[1024];
        CMemStore* s = (CMemStore*) &v.m_Store;
        size_t pos = s->Tell();
        for (size_t n = s->Read((void*) buff, sizeof(buff));
             n > 0;
             n = s->Read((void*) buff, sizeof(buff))) {
            Append((void*) buff, n);
        }
        s->Seek((long) pos, C_RA_Storage::eHead);
    }
    return *this;
}

void CDB_Stream::AssignNULL()
{
    CDB_Object::AssignNULL();
    Truncate();
}

size_t CDB_Stream::Read(void* buff, size_t nof_bytes)
{
    return m_Store->Read(buff, nof_bytes);
}

size_t CDB_Stream::Append(const void* buff, size_t nof_bytes)
{
    if(buff && (nof_bytes > 0)) m_Null = false;
    return m_Store->Append(buff, nof_bytes);
}

bool CDB_Stream::MoveTo(size_t byte_number)
{
    return m_Store->Seek((long) byte_number, C_RA_Storage::eHead)
        == (long) byte_number;
}

size_t CDB_Stream::Size() const
{
    return m_Store->GetDataSize();
}

void CDB_Stream::Truncate(size_t nof_bytes)
{
    m_Store->Truncate(nof_bytes);
    m_Null = (m_Store->GetDataSize() <= 0);
}

void CDB_Stream::AssignValue(CDB_Object& v)
{
    CHECK_DRIVER_ERROR(
        (v.GetType() != eDB_Image) && (v.GetType() != eDB_Text),
        "wrong type of CDB_Object",
        2 );
    Assign((CDB_Stream&)v);
}

CDB_Stream::~CDB_Stream()
{
    try {
        delete m_Store;
    }
    NCBI_CATCH_ALL( NCBI_CURRENT_FUNCTION )
}


/////////////////////////////////////////////////////////////////////////////
//  CDB_Image::
//

CDB_Image& CDB_Image::operator= (const CDB_Image& image)
{
    return dynamic_cast<CDB_Image&> (Assign(image));
}

EDB_Type CDB_Image::GetType() const
{
    return eDB_Image;
}

CDB_Object* CDB_Image::Clone() const
{
    CHECK_DRIVER_ERROR(
        !m_Null,
        "Clone for the non NULL image is not supported",
        1 );

    return new CDB_Image;
}


/////////////////////////////////////////////////////////////////////////////
//  CDB_Text::
//

size_t CDB_Text::Append(const void* buff, size_t nof_bytes)
{
    if(!buff) return 0;
    return CDB_Stream::Append
        (buff, nof_bytes ? nof_bytes : strlen((const char*) buff));
}

size_t CDB_Text::Append(const string& s)
{
    return CDB_Stream::Append(s.data(), s.size());
}

CDB_Text& CDB_Text::operator= (const CDB_Text& text)
{
    return dynamic_cast<CDB_Text&> (Assign(text));
}

EDB_Type CDB_Text::GetType() const
{
    return eDB_Text;
}

CDB_Object* CDB_Text::Clone() const
{
    CHECK_DRIVER_ERROR(
        !m_Null,
        "Clone for the non-NULL text is not supported",
        1 );

    return new CDB_Text;
}


/////////////////////////////////////////////////////////////////////////////
//  CDB_SmallDateTime::
//


CDB_SmallDateTime::CDB_SmallDateTime(CTime::EInitMode mode)
: m_NCBITime(mode)
, m_Status( 0x1 )
{
    m_DBTime.days = 0;
    m_DBTime.time = 0;
    m_Null = (mode == CTime::eEmpty);
}


CDB_SmallDateTime::CDB_SmallDateTime(const CTime& t)
: m_NCBITime( t )
, m_Status( 0x1 )
{
    m_DBTime.days = 0;
    m_DBTime.time = 0;
    m_Null = t.IsEmpty();
}


CDB_SmallDateTime::CDB_SmallDateTime(Uint2 days, Uint2 minutes)
: m_Status( 0x2 )
{
    m_DBTime.days = days;
    m_DBTime.time = minutes;
    m_Null        = false;
}


CDB_SmallDateTime& CDB_SmallDateTime::Assign(Uint2 days, Uint2 minutes)
{
    m_DBTime.days = days;
    m_DBTime.time = minutes;
    m_Status      = 0x2;
    m_Null        = false;
    return *this;
}


CDB_SmallDateTime& CDB_SmallDateTime::operator= (const CTime& t)
{
    m_NCBITime = t;
    m_DBTime.days = 0;
    m_DBTime.time = 0;
    m_Status = 0x1;
    m_Null = t.IsEmpty();
    return *this;
}


const CTime& CDB_SmallDateTime::Value(void) const
{
    if((m_Status & 0x1) == 0) {
        m_NCBITime.SetTimeDBU(m_DBTime);
        m_Status |= 0x1;
    }
    return m_NCBITime;
}


Uint2 CDB_SmallDateTime::GetDays(void) const
{
    if((m_Status & 0x2) == 0) {
        m_DBTime = m_NCBITime.GetTimeDBU();
        m_Status |= 0x2;
    }
    return m_DBTime.days;
}


Uint2 CDB_SmallDateTime::GetMinutes(void) const
{
    if((m_Status & 0x2) == 0) {
        m_DBTime = m_NCBITime.GetTimeDBU();
        m_Status |= 0x2;
    }
    return m_DBTime.time;
}


EDB_Type CDB_SmallDateTime::GetType() const
{
    return eDB_SmallDateTime;
}

CDB_Object* CDB_SmallDateTime::Clone() const
{
    return m_Null ? new CDB_SmallDateTime : new CDB_SmallDateTime(Value());
}

void CDB_SmallDateTime::AssignValue(CDB_Object& v)
{
    CHECK_DRIVER_ERROR(
        v.GetType() != eDB_SmallDateTime,
        "wrong type of CDB_Object",
        2 );
    *this= (CDB_SmallDateTime&)v;
}


/////////////////////////////////////////////////////////////////////////////
//  CDB_DateTime::
//


CDB_DateTime::CDB_DateTime(CTime::EInitMode mode)
: m_NCBITime(mode)
, m_Status(0x1)
{
    m_DBTime.days = 0;
    m_DBTime.time = 0;
    m_Null = (mode == CTime::eEmpty);
}


CDB_DateTime::CDB_DateTime(const CTime& t)
: m_NCBITime( t )
, m_Status( 0x1 )
{
    m_DBTime.days = 0;
    m_DBTime.time = 0;
    m_Null = t.IsEmpty();
}


CDB_DateTime::CDB_DateTime(Int4 d, Int4 s300)
: m_Status( 0x2 )
{
    m_DBTime.days = d;
    m_DBTime.time = s300;
    m_Null = false;
}


CDB_DateTime& CDB_DateTime::operator= (const CTime& t)
{
    m_NCBITime = t;
    m_DBTime.days = 0;
    m_DBTime.time = 0;
    m_Status = 0x1;
    m_Null = t.IsEmpty();
    return *this;
}


CDB_DateTime& CDB_DateTime::Assign(Int4 d, Int4 s300)
{
    m_DBTime.days = d;
    m_DBTime.time = s300;
    m_Status = 0x2;
    m_Null = false;
    return *this;
}


const CTime& CDB_DateTime::Value(void) const
{
    if((m_Status & 0x1) == 0) {
        m_NCBITime.SetTimeDBI(m_DBTime);
        m_Status |= 0x1;
    }
    return m_NCBITime;
}


Int4 CDB_DateTime::GetDays(void) const
{
    if((m_Status & 0x2) == 0) {
        m_DBTime = m_NCBITime.GetTimeDBI();
        m_Status |= 0x2;
    }
    return m_DBTime.days;
}


Int4 CDB_DateTime::Get300Secs(void) const
{
    if((m_Status & 0x2) == 0) {
        m_DBTime = m_NCBITime.GetTimeDBI();
        m_Status |= 0x2;
    }
    return m_DBTime.time;
}


EDB_Type CDB_DateTime::GetType() const
{
    return eDB_DateTime;
}

CDB_Object* CDB_DateTime::Clone() const
{
    return m_Null ? new CDB_DateTime : new CDB_DateTime(Value());
}

void CDB_DateTime::AssignValue(CDB_Object& v)
{
    CHECK_DRIVER_ERROR(
        v.GetType() != eDB_DateTime,
        "wrong type of CDB_Object",
        2 );
    *this= (CDB_DateTime&)v;
}

/////////////////////////////////////////////////////////////////////////////
//  CDB_Bit::
//


CDB_Bit& CDB_Bit::operator= (const int& i)
{
    m_Null = false;
    m_Val = i ? 1 : 0;
    return *this;
}


CDB_Bit& CDB_Bit::operator= (const bool& i)
{
    m_Null = false;
    m_Val = i ? 1 : 0;
    return *this;
}


EDB_Type CDB_Bit::GetType() const
{
    return eDB_Bit;
}

CDB_Object* CDB_Bit::Clone() const
{
    return m_Null ? new CDB_Bit : new CDB_Bit(m_Val ? 1 : 0);
}

void CDB_Bit::AssignValue(CDB_Object& v)
{
    CHECK_DRIVER_ERROR(
        v.GetType() != eDB_Bit,
        "wrong type of CDB_Object",
        2 );
    *this= (CDB_Bit&)v;
}


/////////////////////////////////////////////////////////////////////////////
//
//  CDB_Numeric::
//


CDB_Numeric::CDB_Numeric() : CDB_Object(true)
{
    m_Precision= m_Scale= 0;
    return;
}


CDB_Numeric::CDB_Numeric(unsigned int precision, unsigned int scale)
    : CDB_Object(false)
{
    m_Precision = precision;
    m_Scale     = scale;
    memset(m_Body, 0, sizeof(m_Body));
}


CDB_Numeric::CDB_Numeric(unsigned int precision,
                         unsigned int scale,
                         const unsigned char* arr) : CDB_Object(false)
{
    m_Precision = precision;
    m_Scale     = scale;
    memcpy(m_Body, arr, sizeof(m_Body));
}


CDB_Numeric::CDB_Numeric(unsigned int precision,
                         unsigned int scale,
                         bool is_negative,
                         const unsigned char* arr) : CDB_Object(false)
{
    m_Precision = precision;
    m_Scale     = scale;
    m_Body[0]= is_negative? 1 : 0;
    memcpy(m_Body+1, arr, sizeof(m_Body)-1);
}


CDB_Numeric::CDB_Numeric(unsigned int precision, unsigned int scale, const char* val)
{
    x_MakeFromString(precision, scale, val);
}


CDB_Numeric::CDB_Numeric(unsigned int precision, unsigned int scale, const string& val)
{
    x_MakeFromString(precision, scale, val.c_str());
}


CDB_Numeric& CDB_Numeric::Assign(unsigned int precision,
                                 unsigned int scale,
                                 const unsigned char* arr)
{
    m_Precision = precision;
    m_Scale     = scale;
    m_Null      = false;
    memcpy(m_Body, arr, sizeof(m_Body));
    return *this;
}


CDB_Numeric& CDB_Numeric::Assign(unsigned int precision,
                                 unsigned int scale,
                                 bool is_negative,
                                 const unsigned char* arr)
{
    m_Precision = precision;
    m_Scale     = scale;
    m_Null      = false;
    m_Body[0]= is_negative? 1 : 0;
    memcpy(m_Body + 1, arr, sizeof(m_Body) - 1);
    return *this;
}


CDB_Numeric& CDB_Numeric::operator= (const char* val)
{
    x_MakeFromString(m_Precision, m_Scale, val);
    return *this;
}


CDB_Numeric& CDB_Numeric::operator= (const string& val)
{
    x_MakeFromString(m_Precision, m_Scale, val.c_str());
    return *this;
}


EDB_Type CDB_Numeric::GetType() const
{
    return eDB_Numeric;
}


CDB_Object* CDB_Numeric::Clone() const
{
    return new CDB_Numeric((unsigned int) m_Precision,
                           (unsigned int) m_Scale, m_Body);
}


static int s_NumericBytesPerPrec[] =
{
    2, 2, 3, 3, 4, 4, 4, 5, 5, 6, 6, 6, 7, 7, 8, 8, 9, 9, 9,
    10, 10, 11, 11, 11, 12, 12, 13, 13, 14, 14, 14, 15, 15,
    16, 16, 16, 17, 17, 18, 18, 19, 19, 19, 20, 20, 21, 21, 21,
    22, 22, 23, 23, 24, 24, 24, 25, 25, 26, 26, 26
};


static const unsigned int kMaxPrecision = 50;


static void s_DoCarry(unsigned char* product)
{
    for (unsigned int j = 0;  j < kMaxPrecision;  j++) {
        if (product[j] > 9) {
            product[j + 1] += product[j] / 10;
            product[j]     %= 10;
        }
    }
}


static void s_MultiplyByte(unsigned char* product, int num,
                           const unsigned char* multiplier)
{
    unsigned char number[3];
    number[0] =  num        % 10;
    number[1] = (num /  10) % 10;
    number[2] = (num / 100) % 10;

    int top;
    for (top = kMaxPrecision - 1;  top >= 0  &&  !multiplier[top];  top--)
        continue;

    int start = 0;
    for (int i = 0;  i <= top;  i++) {
        for (int j =0;  j < 3;  j++) {
            product[j + start] += multiplier[i] * number[j];
        }
        s_DoCarry(product);
        start++;
    }
}


static char* s_ArrayToString(const unsigned char* array, int scale, char* s)
{
    int top;

    for (top = kMaxPrecision - 1;  top >= 0  &&  top > scale  &&  !array[top];
         top--)
        continue;

    if (top == -1) {
        s[0] = '0';
        s[1] = '\0';
        return s;
    }

    int j = 0;
    for (int i = top;  i >= 0;  i--) {
        if (top + 1 - j == scale)
            s[j++] = '.';
        s[j++] = array[i] + '0';
    }
    s[j] = '\0';

    return s;
}


string CDB_Numeric::Value() const
{
    unsigned char multiplier[kMaxPrecision];
    unsigned char temp[kMaxPrecision];
    unsigned char product[kMaxPrecision];
    char result[kMaxPrecision + 1];
    char* s = result;
    int num_bytes;

    memset(multiplier, 0, kMaxPrecision);
    memset(product,    0, kMaxPrecision);
    multiplier[0] = 1;
    num_bytes = s_NumericBytesPerPrec[m_Precision-1];

    if (m_Body[0] == 1) {
        *s++ = '-';
    }

    for (int pos = num_bytes - 1;  pos > 0;  pos--) {
        s_MultiplyByte(product, m_Body[pos], multiplier);

        memcpy(temp, multiplier, kMaxPrecision);
        memset(multiplier, 0, kMaxPrecision);
        s_MultiplyByte(multiplier, 256, temp);
    }

    s_ArrayToString(product, m_Scale, s);
    return result;
}



static int s_NextValue(const char** s, int rem, int base)
{
    while (**s < base) {
        rem = rem * base + (int) **s;
        (*s)++;
        if (rem >= 256)
            break;
    }
    return rem;
}


static int s_Div256(const char* value, char* product, int base)
{
    int res = 0;
    int n;

    while (*value < base) {
        n = s_NextValue(&value, res, base);
        *product = (char) (n / 256);
        res = n % 256;
        ++product;
    }
    *product = base;
    return res;
}


void CDB_Numeric::x_MakeFromString(unsigned int precision, unsigned int scale,
                                   const char* val)
{

    if (m_Precision == 0  &&  precision == 0  &&  val) {
        precision= (unsigned int) strlen(val);
        if (scale == 0) {
            scale= precision - (Uint1) strcspn(val, ".");
            if (scale > 1)
                --scale;
        }
    }

    CHECK_DRIVER_ERROR(
        !precision  ||  precision > kMaxPrecision,
        "illegal precision",
        100 );
    CHECK_DRIVER_ERROR(
        scale > precision,
        "scale cannot be more than precision",
        101 );

    bool is_negative= false;
    if(*val == '-') {
        is_negative= true;
        ++val;
    }

    while (*val == '0') {
        ++val;
    }

    char buff1[kMaxPrecision + 1];
    unsigned int n = 0;
    while (*val  &&  n < precision) {
        if (*val >= '0'  &&  *val <= '9') {
            buff1[n++] = *val - '0';
        } else if (*val == '.') {
            break;
        } else {
            DATABASE_DRIVER_ERROR( "string cannot be converted", 102 );
        }
        ++val;
    }

    CHECK_DRIVER_ERROR(
        precision - n < scale,
        "string cannot be converted because of overflow",
        103 );

    unsigned int dec = 0;
    if (*val == '.') {
        ++val;
        while (*val  &&  dec < scale) {
            if (*val >= '0'  &&  *val <= '9') {
                buff1[n++] = *val - '0';
            } else {
                DATABASE_DRIVER_ERROR( "string cannot be converted", 102 );
            }
            ++dec;
            ++val;
        }
    }

    while (dec++ < scale) {
        buff1[n++] = 0;
    }
    if (n == 0) {
        buff1[n++] = 0;
    }
    buff1[n] = 10;

    char  buff2[kMaxPrecision + 1];
    char* p[2];
    p[0] = buff1;
    p[1] = buff2;

    // Setup everything now
    memset(m_Body, 0, sizeof(m_Body));
    if (is_negative) {
        m_Body[0] = 1/*sign*/;
    }
    unsigned char* num = m_Body + s_NumericBytesPerPrec[precision - 1] - 1;
    for (int i = 0;  *p[i];  i = 1 - i) {
        *num = s_Div256(p[i], p[1-i], 10);
        --num;
    }

    m_Precision = precision;
    m_Scale     = scale;
    m_Null      = false;
}

void CDB_Numeric::AssignValue(CDB_Object& v)
{
    CHECK_DRIVER_ERROR(
        v.GetType() != eDB_Numeric,
        "wrong type of CDB_Object",
        2 );
    *this= (CDB_Numeric&)v;
}

END_NCBI_SCOPE



/*
 * ===========================================================================
 * $Log$
 * Revision 1.23  2006/08/07 15:55:01  ssikorsk
 * Moved definitions of big inline functions into cpp.
 *
 * Revision 1.22  2006/06/02 19:26:38  ssikorsk
 * + NCBI_CATCH_ALL( NCBI_CURRENT_FUNCTION )
 *
 * Revision 1.21  2005/11/02 14:32:39  ssikorsk
 * Use NCBI_CATCH_ALL macro instead of catch(...)
 *
 * Revision 1.20  2005/09/15 11:00:01  ssikorsk
 * Destructors do not throw exceptions any more.
 *
 * Revision 1.19  2005/05/31 21:02:37  ssikorsk
 * Added GetTypeName method to the CDB_Object class
 *
 * Revision 1.18  2005/04/04 13:03:56  ssikorsk
 * Revamp of DBAPI exception class CDB_Exception
 *
 * Revision 1.17  2004/05/17 21:11:38  gorelenk
 * Added include of PCH ncbi_pch.hpp
 *
 * Revision 1.16  2004/01/21 18:07:35  sapojnik
 * AssignValue() for misc. int, float, double accepts less capable types; only no loss type conversions allowed
 *
 * Revision 1.15  2003/12/10 18:50:22  soussov
 * fixes bug with null value flag in AssignValue methods for CDB_Char, CDB_Binary, CDB_LongChar, CDB_LongBinary
 *
 * Revision 1.14  2003/08/13 18:02:11  soussov
 * fixes bug in Clone() for [Small]DateTime
 *
 * Revision 1.13  2003/05/13 16:54:40  sapojnik
 * CDB_Object::Create() - support for LongChar, LongBinary
 *
 * Revision 1.12  2003/05/05 15:58:22  soussov
 * fixed bug in Clone() method for CDB_Char, CDB_LongChar and CDB_LongBinary when NULL value is cloned
 *
 * Revision 1.11  2003/04/29 21:13:57  soussov
 * new datatypes CDB_LongChar and CDB_LongBinary added
 *
 * Revision 1.10  2002/09/25 21:47:51  soussov
 * adds check for not-empty append before dropping the Null flag in CDB_Stream
 *
 * Revision 1.9  2002/07/19 15:31:43  soussov
 * add check for NULL pointer in CDB_Text::Append
 *
 * Revision 1.8  2002/05/16 21:29:38  soussov
 * AssignValue methods added
 *
 * Revision 1.7  2002/02/14 00:59:42  vakatov
 * Clean-up: warning elimination, fool-proofing, fine-tuning, identation, etc.
 *
 * Revision 1.6  2002/02/13 22:37:26  sapojnik
 * new_CDB_Object() renamed to CDB_Object::create()
 *
 * Revision 1.5  2002/02/13 22:14:50  sapojnik
 * new_CDB_Object() (needed for rdblib)
 *
 * Revision 1.4  2002/02/06 22:22:54  soussov
 * fixes the string to numeric assignment
 *
 * Revision 1.3  2001/11/06 17:59:54  lavr
 * Formatted uniformly as the rest of the library
 *
 * Revision 1.2  2001/09/24 20:38:52  soussov
 * fixed bug in sign processing
 *
 * Revision 1.1  2001/09/21 23:40:00  vakatov
 * -----  Initial (draft) revision.  -----
 * This is a major revamp (by Denis Vakatov, with help from Vladimir Soussov)
 * of the DBAPI "driver" libs originally written by Vladimir Soussov.
 * The revamp involved massive code shuffling and grooming, numerous local
 * API redesigns, adding comments and incorporating DBAPI to the C++ Toolkit.
 *
 * ===========================================================================
 */
