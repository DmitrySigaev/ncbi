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
    case eDB_Int             : return new CDB_Int          ();
    case eDB_SmallInt        : return new CDB_SmallInt     ();
    case eDB_TinyInt         : return new CDB_TinyInt      ();
    case eDB_BigInt          : return new CDB_BigInt       ();
    case eDB_VarChar         : return new CDB_VarChar      ();
    case eDB_Char            : return new CDB_Char     (size);
    case eDB_VarBinary       : return new CDB_VarBinary    ();
    case eDB_Binary          : return new CDB_Binary   (size);
    case eDB_Float           : return new CDB_Float        ();
    case eDB_Double          : return new CDB_Double       ();
    case eDB_DateTime        : return new CDB_DateTime     ();
    case eDB_SmallDateTime   : return new CDB_SmallDateTime();
    case eDB_Text            : return new CDB_Text         ();
    case eDB_Image           : return new CDB_Image        ();
    case eDB_Bit             : return new CDB_Bit          ();
    case eDB_Numeric         : return new CDB_Numeric      ();
    case eDB_UnsupportedType : break;
    }
    throw CDB_ClientEx(eDB_Error, 2, "CDB_Object::Create", "unknown type");
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


/////////////////////////////////////////////////////////////////////////////
//  CDB_VarChar::
//

EDB_Type CDB_VarChar::GetType() const
{
    return eDB_VarChar;
}

CDB_Object* CDB_VarChar::Clone() const
{
    return m_Null ? new CDB_VarChar : new CDB_VarChar(m_Val);
}


/////////////////////////////////////////////////////////////////////////////
//  CDB_Char::
//

EDB_Type CDB_Char::GetType() const
{
    return eDB_Char;
}

CDB_Object* CDB_Char::Clone() const
{
    return m_Null ? new CDB_Char : new CDB_Char(*this);
}

CDB_Char::~CDB_Char()
{
    delete [] m_Val;
}


/////////////////////////////////////////////////////////////////////////////
//  CDB_VarBinary::
//

EDB_Type CDB_VarBinary::GetType() const
{
    return eDB_VarBinary;
}

CDB_Object* CDB_VarBinary::Clone() const
{
    return m_Null ? new CDB_VarBinary : new CDB_VarBinary(m_Val, m_Size);
}


/////////////////////////////////////////////////////////////////////////////
//  CDB_Binary::
//

EDB_Type CDB_Binary::GetType() const
{
    return eDB_Binary;
}

CDB_Object* CDB_Binary::Clone() const
{
    return m_Null ? new CDB_Binary : new CDB_Binary(*this);
}

CDB_Binary::~CDB_Binary()
{
    delete[] m_Val;
}


/////////////////////////////////////////////////////////////////////////////
//  CDB_Float::
//

EDB_Type CDB_Float::GetType() const
{
    return eDB_Float;
}

CDB_Object* CDB_Float::Clone() const
{
    return m_Null ? new CDB_Float : new CDB_Float(m_Val);
}



/////////////////////////////////////////////////////////////////////////////
//  CDB_Double::
//

EDB_Type CDB_Double::GetType() const
{
    return eDB_Double;
}

CDB_Object* CDB_Double::Clone() const
{
    return m_Null ? new CDB_Double : new CDB_Double(m_Val);
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
    m_Null = false;
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

CDB_Stream::~CDB_Stream()
{
    delete m_Store;
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
    if ( !m_Null ) {
        throw CDB_ClientEx(eDB_Error, 1, "CDB_Image::Clone",
                           "Clone for the non NULL image is not supported");
    }
    return new CDB_Image;
}


/////////////////////////////////////////////////////////////////////////////
//  CDB_Text::
//

size_t CDB_Text::Append(const void* buff, size_t nof_bytes)
{
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
    if ( !m_Null ) {
        throw CDB_ClientEx(eDB_Error, 1, "CDB_Text::Clone",
                           "Clone for the non-NULL text is not supported");
    }
    return new CDB_Text;
}


/////////////////////////////////////////////////////////////////////////////
//  CDB_SmallDateTime::
//

EDB_Type CDB_SmallDateTime::GetType() const
{
    return eDB_SmallDateTime;
}

CDB_Object* CDB_SmallDateTime::Clone() const
{
    return new CDB_SmallDateTime(Value());
}


/////////////////////////////////////////////////////////////////////////////
//  CDB_DateTime::
//

EDB_Type CDB_DateTime::GetType() const
{
    return eDB_DateTime;
}

CDB_Object* CDB_DateTime::Clone() const
{
    return new CDB_DateTime(Value());
}


/////////////////////////////////////////////////////////////////////////////
//  CDB_Bit::
//

EDB_Type CDB_Bit::GetType() const
{
    return eDB_Bit;
}

CDB_Object* CDB_Bit::Clone() const
{
    return m_Null ? new CDB_Bit : new CDB_Bit(m_Val ? 1 : 0);
}



/////////////////////////////////////////////////////////////////////////////
//
//  CDB_Numeric::
//


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

    if (!precision  ||  precision > kMaxPrecision) {
        throw CDB_ClientEx(eDB_Error, 100, "CDB_Numeric::x_MakeFromString",
                           "illegal precision");
    }
    if (scale > precision) {
        throw CDB_ClientEx(eDB_Error, 101, "CDB_Numeric::x_MakeFromString",
                           "scale can not be more than precision");
    }

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
            throw CDB_ClientEx(eDB_Error, 102, "CDB_Numeric::x_MakeFromString",
                               "string can not be converted");
        }
        ++val;
    }

    if (precision - n < scale) {
        throw CDB_ClientEx(eDB_Error, 103, "CDB_Numeric::x_MakeFromString",
                           "string can not be converted because of overflow");
    }

    unsigned int dec = 0;
    if (*val == '.') {
        ++val;
        while (*val  &&  dec < scale) {
            if (*val >= '0'  &&  *val <= '9') {
                buff1[n++] = *val - '0';
            } else {
                throw CDB_ClientEx(eDB_Error, 102,
                                   "CDB_Numeric::x_MakeFromString",
                                   "string can not be converted");
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


END_NCBI_SCOPE



/*
 * ===========================================================================
 * $Log$
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
