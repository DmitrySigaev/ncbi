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
 * File Description:  DBLib Results
 *
 */

#include <ncbi_pch.hpp>
#include <dbapi/driver/odbc/interfaces.hpp>
#include <dbapi/driver/util/numeric_convert.hpp>


BEGIN_NCBI_SCOPE

/////////////////////////////////////////////////////////////////////////////
static const char* wrong_type = "Wrong type of CDB_Object";

/////////////////////////////////////////////////////////////////////////////

static EDB_Type s_GetDataType(SQLSMALLINT t, SQLSMALLINT dec_digits,
                              SQLULEN prec)
{
    switch (t) {
    case SQL_WCHAR:
    case SQL_CHAR:         return (prec < 256)? eDB_Char : eDB_LongChar;
    case SQL_WVARCHAR:
    case SQL_VARCHAR:      return (prec < 256)? eDB_VarChar : eDB_LongChar;
    case SQL_LONGVARCHAR:  return eDB_Text;
    case SQL_LONGVARBINARY:
    case SQL_WLONGVARCHAR: return eDB_Image;
    case SQL_DECIMAL:
    case SQL_NUMERIC:      if(prec > 20 || dec_digits > 0) return eDB_Numeric;
    case SQL_BIGINT:       return eDB_BigInt;
    case SQL_SMALLINT:     return eDB_SmallInt;
    case SQL_INTEGER:      return eDB_Int;
    case SQL_REAL:         return eDB_Float;
    case SQL_FLOAT:        return eDB_Double;
    case SQL_BINARY:       return (prec < 256)? eDB_Binary : eDB_LongBinary;
    case SQL_BIT:          return eDB_Bit;
    case SQL_TINYINT:      return eDB_TinyInt;
    case SQL_VARBINARY:    return (prec < 256)? eDB_VarBinary : eDB_LongBinary;
    case SQL_TYPE_TIMESTAMP:
        return (prec > 16 || dec_digits > 0)? eDB_DateTime : eDB_SmallDateTime;
    default:               return eDB_UnsupportedType;
    }
}


/////////////////////////////////////////////////////////////////////////////
//
//  CODBC_RowResult::
//


CODBC_RowResult::CODBC_RowResult(
    CStatementBase& stmt,
    SQLSMALLINT nof_cols,
    SQLLEN* row_count
    )
    : m_Stmt(stmt)
    , m_CurrItem(-1)
    , m_EOR(false)
    , m_RowCountPtr( row_count )
{
    m_NofCols = nof_cols;
    if(m_RowCountPtr) *m_RowCountPtr = 0;

    SQLSMALLINT actual_name_size, nullable;

    m_ColFmt = new SODBC_ColDescr[m_NofCols];
    for (unsigned int n = 0; n < m_NofCols; n++) {
        switch(SQLDescribeCol(GetHandle(), n+1, m_ColFmt[n].ColumnName,
                              ODBC_COLUMN_NAME_SIZE, &actual_name_size,
                              &m_ColFmt[n].DataType, &m_ColFmt[n].ColumnSize,
                              &m_ColFmt[n].DecimalDigits, &nullable)) {
        case SQL_SUCCESS_WITH_INFO:
            ReportErrors();
        case SQL_SUCCESS:
            continue;
        case SQL_ERROR:
            ReportErrors();
            {
                string err_message = "SQLDescribeCol failed" + GetDiagnosticInfo();
                DATABASE_DRIVER_ERROR( err_message, 420020 );
            }
        default:
            {
                string err_message = "SQLDescribeCol failed (memory corruption suspected)" + GetDiagnosticInfo();
                DATABASE_DRIVER_ERROR( err_message, 420021 );
            }
       }
    }
}


EDB_ResType CODBC_RowResult::ResultType() const
{
    return eDB_RowResult;
}


unsigned int CODBC_RowResult::NofItems() const
{
    return m_NofCols;
}


const char* CODBC_RowResult::ItemName(unsigned int item_num) const
{
    return item_num < m_NofCols ? (char*)m_ColFmt[item_num].ColumnName : 0;
}


size_t CODBC_RowResult::ItemMaxSize(unsigned int item_num) const
{
    return item_num < m_NofCols ? m_ColFmt[item_num].ColumnSize : 0;
}


EDB_Type CODBC_RowResult::ItemDataType(unsigned int item_num) const
{
    return item_num < m_NofCols ?
        s_GetDataType(m_ColFmt[item_num].DataType, m_ColFmt[item_num].DecimalDigits,
            m_ColFmt[item_num].ColumnSize) :  eDB_UnsupportedType;
}


bool CODBC_RowResult::Fetch()
{
    m_CurrItem= -1;
    if (!m_EOR) {
        switch (SQLFetch(GetHandle())) {
        case SQL_SUCCESS_WITH_INFO:
            ReportErrors();
        case SQL_SUCCESS:
            m_CurrItem = 0;
            if ( m_RowCountPtr != NULL ) {
                ++(*m_RowCountPtr);
            }
            return true;
        case SQL_NO_DATA:
            m_EOR = true;
            break;
        case SQL_ERROR:
            ReportErrors();
            {
                string err_message = "SQLFetch failed" + GetDiagnosticInfo();
                DATABASE_DRIVER_ERROR( err_message, 430003 );
            }
        default:
            {
                string err_message = "SQLFetch failed (memory corruption suspected)" + GetDiagnosticInfo();
                DATABASE_DRIVER_ERROR( err_message, 430004 );
            }
        }
    }
    return false;
}


int CODBC_RowResult::CurrentItemNo() const
{
    return m_CurrItem;
}

int CODBC_RowResult::GetColumnNum(void) const
{
    return static_cast<int>(m_NofCols);
}

int CODBC_RowResult::xGetData(SQLSMALLINT target_type, SQLPOINTER buffer,
                              SQLINTEGER buffer_size)
{
    SQLLEN f;

    switch(SQLGetData(GetHandle(), m_CurrItem+1, target_type, buffer, buffer_size, &f)) {
    case SQL_SUCCESS_WITH_INFO:
        switch(f) {
        case SQL_NO_TOTAL:
            return buffer_size;
        case SQL_NULL_DATA:
            return 0;
        default:
            if(f < 0)
                ReportErrors();
            return (int)f;
        }
    case SQL_SUCCESS:
        if(target_type==SQL_C_CHAR) buffer_size--;
        return (f > buffer_size)? buffer_size : (int)f;
    case SQL_NO_DATA:
        return 0;
    case SQL_ERROR:
        ReportErrors();
    default:
        {
            string err_message = "SQLGetData failed " + GetDiagnosticInfo();
            DATABASE_DRIVER_ERROR( err_message, 430027 );
        }
    }
}

static void xConvert2CDB_Numeric(CDB_Numeric* d, SQL_NUMERIC_STRUCT& s)
{
    swap_numeric_endian((unsigned int)s.precision, s.val);
    d->Assign((unsigned int)s.precision, (unsigned int)s.scale,
             s.sign == 0, s.val);
}

CDB_Object* CODBC_RowResult::xLoadItem(CDB_Object* item_buf)
{
    char buffer[8*1024];
    int outlen;

    switch(m_ColFmt[m_CurrItem].DataType) {
    case SQL_WCHAR:
    case SQL_CHAR:
    case SQL_VARCHAR:
    case SQL_WVARCHAR: {
        switch (item_buf->GetType()) {
        case eDB_VarBinary:
            outlen= xGetData(SQL_C_BINARY, buffer, sizeof(buffer));
            if ( outlen <= 0) item_buf->AssignNULL();
            else ((CDB_VarBinary*) item_buf)->SetValue(buffer, outlen);
            break;
        case eDB_Binary:
            outlen= xGetData(SQL_C_BINARY, buffer, sizeof(buffer));
            if ( outlen <= 0) item_buf->AssignNULL();
            else ((CDB_Binary*) item_buf)->SetValue(buffer, outlen);
            break;
        case eDB_LongBinary:
            outlen= xGetData(SQL_C_BINARY, buffer, sizeof(buffer));
            if ( outlen <= 0) item_buf->AssignNULL();
            else ((CDB_LongBinary*) item_buf)->SetValue(buffer, outlen);
            break;
        case eDB_VarChar:
            outlen= xGetData(SQL_C_CHAR, buffer, sizeof(buffer));
            if ( outlen <= 0) item_buf->AssignNULL();
            else *((CDB_VarChar*)  item_buf) = buffer;
            break;
        case eDB_Char:
            outlen= xGetData(SQL_C_CHAR, buffer, sizeof(buffer));
            if ( outlen <= 0) item_buf->AssignNULL();
            else *((CDB_Char*)     item_buf) = buffer;
            break;
        case eDB_LongChar:
            outlen= xGetData(SQL_C_CHAR, buffer, sizeof(buffer));
            if ( outlen <= 0) item_buf->AssignNULL();
            else *((CDB_LongChar*)     item_buf) = buffer;
            break;
        default:
            {
                string err_message = wrong_type + GetDiagnosticInfo();
                DATABASE_DRIVER_ERROR( err_message, 430020 );
            }
        }
        break;
    }

    case SQL_BINARY:
    case SQL_VARBINARY: {
        switch ( item_buf->GetType() ) {
        case eDB_VarBinary:
            outlen= xGetData(SQL_C_BINARY, buffer, sizeof(buffer));
            if ( outlen <= 0) item_buf->AssignNULL();
            else ((CDB_VarBinary*) item_buf)->SetValue(buffer, outlen);
            break;
        case eDB_Binary:
            outlen= xGetData(SQL_C_BINARY, buffer, sizeof(buffer));
            if ( outlen <= 0) item_buf->AssignNULL();
            else ((CDB_Binary*) item_buf)->SetValue(buffer, outlen);
            break;
        case eDB_LongBinary:
            outlen= xGetData(SQL_C_BINARY, buffer, sizeof(buffer));
            if ( outlen <= 0) item_buf->AssignNULL();
            else ((CDB_LongBinary*) item_buf)->SetValue(buffer, outlen);
            break;
        case eDB_VarChar:
            outlen= xGetData(SQL_C_CHAR, buffer, sizeof(buffer));
            if (outlen <= 0) item_buf->AssignNULL();
            else *((CDB_VarChar*)  item_buf) = buffer;
            break;
        case eDB_Char:
            outlen= xGetData(SQL_C_CHAR, buffer, sizeof(buffer));
            if (outlen <= 0) item_buf->AssignNULL();
            else *((CDB_Char*) item_buf) = buffer;
            break;
        case eDB_LongChar:
            outlen= xGetData(SQL_C_CHAR, buffer, sizeof(buffer));
            if (outlen <= 0) item_buf->AssignNULL();
            else *((CDB_LongChar*) item_buf) = buffer;
            break;
        default:
            {
                string err_message = wrong_type + GetDiagnosticInfo();
                DATABASE_DRIVER_ERROR( err_message, 430020 );
            }
        }

        break;
    }

    case SQL_BIT: {
        SQLCHAR v;
        switch (  item_buf->GetType()  ) {
        case eDB_Bit:
            outlen= xGetData(SQL_C_BIT, &v, sizeof(SQLCHAR));
            if (outlen <= 0) item_buf->AssignNULL();
            else *((CDB_Bit*) item_buf) = (int) v;
            break;
        case eDB_TinyInt:
            outlen= xGetData(SQL_C_BIT, &v, sizeof(SQLCHAR));
            if (outlen <= 0) item_buf->AssignNULL();
            else *((CDB_TinyInt*)  item_buf) = v ? 1 : 0;
            break;
        case eDB_SmallInt:
            outlen= xGetData(SQL_C_BIT, &v, sizeof(SQLCHAR));
            if (outlen <= 0) item_buf->AssignNULL();
            else *((CDB_SmallInt*) item_buf) = v ? 1 : 0;
            break;
        case eDB_Int:
            outlen= xGetData(SQL_C_BIT, &v, sizeof(SQLCHAR));
            if (outlen <= 0) item_buf->AssignNULL();
            else *((CDB_Int*)      item_buf) = v ? 1 : 0;
            break;
        default:
            {
                string err_message = wrong_type + GetDiagnosticInfo();
                DATABASE_DRIVER_ERROR( err_message, 430020 );
            }
        }
        break;
    }

    case SQL_TYPE_TIMESTAMP: {
        SQL_TIMESTAMP_STRUCT v;
        switch ( item_buf->GetType() ) {
        case eDB_SmallDateTime: {
            outlen= xGetData(SQL_C_TYPE_TIMESTAMP, &v, sizeof(SQL_TIMESTAMP_STRUCT));
            if (outlen <= 0) item_buf->AssignNULL();
            else {
                CTime t((int)v.year, (int)v.month, (int)v.day,
                        (int)v.hour, (int)v.minute, (int)v.second,
                        (long)v.fraction);

                *((CDB_SmallDateTime*) item_buf)= t;
            }
            break;
        }
        case eDB_DateTime: {
            outlen= xGetData(SQL_C_TYPE_TIMESTAMP, &v, sizeof(SQL_TIMESTAMP_STRUCT));
            if (outlen <= 0) item_buf->AssignNULL();
            else {
                CTime t((int)v.year, (int)v.month, (int)v.day,
                        (int)v.hour, (int)v.minute, (int)v.second,
                        (long)v.fraction);

                *((CDB_DateTime*) item_buf)= t;
            }
            break;
        }
        default:
            {
                string err_message = wrong_type + GetDiagnosticInfo();
                DATABASE_DRIVER_ERROR( err_message, 430020 );
            }
        }
        break;
    }

    case SQL_TINYINT: {
        SQLCHAR v;
        switch (  item_buf->GetType()  ) {
        case eDB_TinyInt:
            outlen= xGetData(SQL_C_UTINYINT, &v, sizeof(SQLCHAR));
            if (outlen <= 0) item_buf->AssignNULL();
            else *((CDB_TinyInt*)  item_buf) = (Uint1) v;
            break;
        case eDB_SmallInt:
            outlen= xGetData(SQL_C_UTINYINT, &v, sizeof(SQLCHAR));
            if (outlen <= 0) item_buf->AssignNULL();
            else *((CDB_SmallInt*) item_buf) = (Int2) v;
            break;
        case eDB_Int:
            outlen= xGetData(SQL_C_UTINYINT, &v, sizeof(SQLCHAR));
            if (outlen <= 0) item_buf->AssignNULL();
            else *((CDB_Int*)      item_buf) = (Int4) v;
            break;
        default:
            {
                string err_message = wrong_type + GetDiagnosticInfo();
                DATABASE_DRIVER_ERROR( err_message, 430020 );
            }
        }
        break;
    }

    case SQL_SMALLINT: {
        SQLSMALLINT v;
        switch (  item_buf->GetType()  ) {
        case eDB_SmallInt:
            outlen= xGetData(SQL_C_SSHORT, &v, sizeof(SQLSMALLINT));
            if (outlen <= 0) item_buf->AssignNULL();
            else *((CDB_SmallInt*) item_buf) = (Int2) v;
            break;
        case eDB_Int:
            outlen= xGetData(SQL_C_SSHORT, &v, sizeof(SQLSMALLINT));
            if (outlen <= 0) item_buf->AssignNULL();
            else *((CDB_Int*) item_buf) = (Int4) v;
            break;
        default:
            {
                string err_message = wrong_type + GetDiagnosticInfo();
                DATABASE_DRIVER_ERROR( err_message, 430020 );
            }
        }
        break;
    }

    case SQL_INTEGER: {
        SQLINTEGER v;
        switch (  item_buf->GetType()  ) {
        case eDB_Int:
            outlen= xGetData(SQL_C_SLONG, &v, sizeof(SQLINTEGER));
            if (outlen <= 0) item_buf->AssignNULL();
            else *((CDB_Int*) item_buf) = (Int4) v;
            break;
        default:
            {
                string err_message = wrong_type + GetDiagnosticInfo();
                DATABASE_DRIVER_ERROR( err_message, 430020 );
            }
        }
        break;
    }

    case SQL_DOUBLE:
    case SQL_FLOAT: {
        SQLDOUBLE v;
        switch (  item_buf->GetType()  ) {
        case eDB_Double:
            outlen= xGetData(SQL_C_DOUBLE, &v, sizeof(SQLDOUBLE));
            if (outlen <= 0) item_buf->AssignNULL();
            else *((CDB_Double*)      item_buf) = v;
            break;
        default:
            {
                string err_message = wrong_type + GetDiagnosticInfo();
                DATABASE_DRIVER_ERROR( err_message, 430020 );
            }
        }
        break;
    }

    case SQL_REAL: {
        SQLREAL v;
        switch (  item_buf->GetType()  ) {
        case eDB_Float:
            outlen= xGetData(SQL_C_FLOAT, &v, sizeof(SQLREAL));
            if (outlen <= 0) item_buf->AssignNULL();
            else *((CDB_Float*)      item_buf) = v;
            break;
        default:
            {
                string err_message = wrong_type + GetDiagnosticInfo();
                DATABASE_DRIVER_ERROR( err_message, 430020 );
            }
        }
        break;
    }

    case SQL_BIGINT:
    case SQL_DECIMAL:
    case SQL_NUMERIC: {
        switch (  item_buf->GetType()  ) {
        case eDB_Numeric: {
            SQL_NUMERIC_STRUCT v;
            SQLHDESC hdesc;
            SQLGetStmtAttr(GetHandle(), SQL_ATTR_APP_ROW_DESC,&hdesc, 0, NULL);
            SQLSetDescField(hdesc,m_CurrItem+1,SQL_DESC_TYPE,(VOID*)SQL_C_NUMERIC,0);
            SQLSetDescField(hdesc,m_CurrItem+1,SQL_DESC_PRECISION,
                    (VOID*)(m_ColFmt[m_CurrItem].ColumnSize),0);
            SQLSetDescField(hdesc,m_CurrItem+1,SQL_DESC_SCALE,
                    (VOID*)(m_ColFmt[m_CurrItem].DecimalDigits),0);

            outlen= xGetData(SQL_ARD_TYPE, &v, sizeof(SQL_NUMERIC_STRUCT));
            if (outlen <= 0) item_buf->AssignNULL();
            else xConvert2CDB_Numeric((CDB_Numeric*)item_buf, v);
            break;
        }
        case eDB_BigInt: {
            SQLBIGINT v;
            outlen= xGetData(SQL_C_SBIGINT, &v, sizeof(SQLBIGINT));
            if (outlen <= 0) item_buf->AssignNULL();
            else *((CDB_BigInt*) item_buf) = (Int8) v;
            break;
        }
        default:
            {
                string err_message = wrong_type + GetDiagnosticInfo();
                DATABASE_DRIVER_ERROR( err_message, 430020 );
            }
        }
        break;
    }

    case SQL_WLONGVARCHAR:
    case SQL_LONGVARBINARY:
    case SQL_LONGVARCHAR: {
        SQLLEN f;
        switch(item_buf->GetType()) {
        case eDB_Text: {
            CDB_Stream* val = (CDB_Stream*) item_buf;
            for(;;) {
                switch(SQLGetData(GetHandle(), m_CurrItem+1, SQL_C_CHAR, buffer, sizeof(buffer), &f)) {
                case SQL_SUCCESS_WITH_INFO:
                    if(f == SQL_NO_TOTAL) f= sizeof(buffer) - 1;
                    else if(f < 0) ReportErrors();
                case SQL_SUCCESS:
                    if(f > 0) {
                        if(f > sizeof(buffer)-1) f= sizeof(buffer)-1;
                        val->Append(buffer, f);
                    }
                    continue;
                case SQL_NO_DATA:
                    break;
                case SQL_ERROR:
                    ReportErrors();
                default:
                    {
                        string err_message = "SQLGetData failed while retrieving text/image into CDB_Text" + 
                            GetDiagnosticInfo();
                        DATABASE_DRIVER_ERROR( err_message, 430021 );
                    }
                }
                break;
            }
            break;
        }
        case eDB_Image: {
            CDB_Stream* val = (CDB_Stream*) item_buf;
            for(;;) {
                switch(SQLGetData(GetHandle(), m_CurrItem+1, SQL_C_BINARY, buffer, sizeof(buffer), &f)) {
                case SQL_SUCCESS_WITH_INFO:
                    if(f == SQL_NO_TOTAL || f > sizeof(buffer)) f= sizeof(buffer);
                    else ReportErrors();
                case SQL_SUCCESS:
                    if(f > 0) {
                        if(f > sizeof(buffer)) f= sizeof(buffer);
                        val->Append(buffer, f);
                    }
                    continue;
                case SQL_NO_DATA:
                    break;
                case SQL_ERROR:
                    ReportErrors();
                default:
                    {
                        string err_message = "SQLGetData failed while retrieving text/image into CDB_Image" + 
                            GetDiagnosticInfo();
                        DATABASE_DRIVER_ERROR( err_message, 430022 );
                    }
                }
                break;
            }
            break;
        }
        default:
            {
                string err_message = wrong_type + GetDiagnosticInfo();
                DATABASE_DRIVER_ERROR( err_message, 430020 );
            }
        }
        break;
    }
    default:
        {
            string err_message = "Unsupported column type" + GetDiagnosticInfo();
            DATABASE_DRIVER_ERROR( err_message, 430025 );
        }

    }
    return item_buf;
}

CDB_Object* CODBC_RowResult::xMakeItem()
{
    char buffer[8*1024];
    int outlen;

    switch(m_ColFmt[m_CurrItem].DataType) {
    case SQL_WCHAR:
    case SQL_CHAR:
    case SQL_VARCHAR:
    case SQL_WVARCHAR: {
        outlen= xGetData(SQL_C_CHAR, buffer, sizeof(buffer));
        if(m_ColFmt[m_CurrItem].ColumnSize < 256) {
            CDB_VarChar* val = (outlen < 0)
                ? new CDB_VarChar() : new CDB_VarChar(buffer, (size_t) outlen);

            return val;
        }
        else {
            CDB_LongChar* val = (outlen < 0)
                ? new CDB_LongChar(m_ColFmt[m_CurrItem].ColumnSize) :
                new CDB_LongChar(m_ColFmt[m_CurrItem].ColumnSize,
                        buffer);

            return val;
        }

    }

    case SQL_BINARY:
    case SQL_VARBINARY: {
        outlen= xGetData(SQL_C_BINARY, buffer, sizeof(buffer));
        if(m_ColFmt[m_CurrItem].ColumnSize < 256) {
            CDB_VarBinary* val = (outlen <= 0)
                ? new CDB_VarBinary() : new CDB_VarBinary(buffer, (size_t)outlen);

            return val;
        }
        else {
            CDB_LongBinary* val = (outlen < 0)
                ? new CDB_LongBinary(m_ColFmt[m_CurrItem].ColumnSize) :
                new CDB_LongBinary(m_ColFmt[m_CurrItem].ColumnSize,
                        buffer, (size_t) outlen);

            return val;
        }
    }

    case SQL_BIT: {
        SQLCHAR v;
        outlen= xGetData(SQL_C_BIT, &v, sizeof(SQLCHAR));
        return (outlen <= 0) ? new CDB_Bit() : new CDB_Bit((int) v);
    }

    case SQL_TYPE_TIMESTAMP: {
        SQL_TIMESTAMP_STRUCT v;
        outlen= xGetData(SQL_C_TYPE_TIMESTAMP, &v, sizeof(SQL_TIMESTAMP_STRUCT));
        if (outlen <= 0) {
            return (m_ColFmt[m_CurrItem].ColumnSize > 16 ||
                m_ColFmt[m_CurrItem].DecimalDigits > 0)? (CDB_Object*)(new CDB_DateTime()) :
                (CDB_Object*)(new CDB_SmallDateTime());
        }
        else {
            CTime t((int)v.year, (int)v.month, (int)v.day,
                    (int)v.hour, (int)v.minute, (int)v.second,
                    (long)v.fraction);
            return (m_ColFmt[m_CurrItem].ColumnSize > 16 ||
                m_ColFmt[m_CurrItem].DecimalDigits > 0)? (CDB_Object*)(new CDB_DateTime(t)) :
                (CDB_Object*)(new CDB_SmallDateTime(t));
        }
    }

    case SQL_TINYINT: {
        SQLCHAR v;
        outlen= xGetData(SQL_C_UTINYINT, &v, sizeof(SQLCHAR));
        return (outlen <= 0) ? new CDB_TinyInt() : new CDB_TinyInt((Uint1) v);
    }

    case SQL_SMALLINT: {
        SQLSMALLINT v;
        outlen= xGetData(SQL_C_SSHORT, &v, sizeof(SQLSMALLINT));
        return (outlen <= 0) ? new CDB_SmallInt() : new CDB_SmallInt((Int2) v);
    }

    case SQL_INTEGER: {
        SQLINTEGER v;
        outlen= xGetData(SQL_C_SLONG, &v, sizeof(SQLINTEGER));
        return (outlen <= 0) ? new CDB_Int() : new CDB_Int((Int4) v);
    }

    case SQL_DOUBLE:
    case SQL_FLOAT: {
        SQLDOUBLE v;
        outlen= xGetData(SQL_C_DOUBLE, &v, sizeof(SQLDOUBLE));
        return (outlen <= 0) ? new CDB_Double() : new CDB_Double(v);
    }
    case SQL_REAL: {
        SQLREAL v;
        outlen= xGetData(SQL_C_FLOAT, &v, sizeof(SQLREAL));
        return (outlen <= 0) ? new CDB_Float() : new CDB_Float(v);
    }

    case SQL_DECIMAL:
    case SQL_NUMERIC: {
        if((m_ColFmt[m_CurrItem].DecimalDigits > 0) ||
           (m_ColFmt[m_CurrItem].ColumnSize > 20)) { // It should be numeric
            SQL_NUMERIC_STRUCT v;
            outlen= xGetData(SQL_C_NUMERIC, &v, sizeof(SQL_NUMERIC_STRUCT));
            CDB_Numeric* r= new CDB_Numeric;
            if(outlen > 0) {
                xConvert2CDB_Numeric(r, v);
            }
                return r;
        }
        else { // It should be bigint
            SQLBIGINT v;
            outlen= xGetData(SQL_C_SBIGINT, &v, sizeof(SQLBIGINT));
            return (outlen <= 0) ? new CDB_BigInt() : new CDB_BigInt((Int8) v);
        }
    }

    case SQL_WLONGVARCHAR:
    case SQL_LONGVARCHAR: {
        CDB_Text* val = new CDB_Text;
        SQLLEN f;

        for(;;) {
            switch(SQLGetData(GetHandle(), m_CurrItem+1, SQL_C_CHAR, buffer, sizeof(buffer), &f)) {
            case SQL_SUCCESS_WITH_INFO:
                if(f == SQL_NO_TOTAL) f= sizeof(buffer) - 1;
                else if(f < 0) ReportErrors();
            case SQL_SUCCESS:
                if(f > 0) {
                    if(f > sizeof(buffer)-1) f= sizeof(buffer)-1;
                    val->Append(buffer, f);
                }
                continue;
            case SQL_NO_DATA:
                break;
            case SQL_ERROR:
                ReportErrors();
            default:
                {
                    string err_message = "SQLGetData failed while retrieving text into CDB_Text" + 
                        GetDiagnosticInfo();
                    DATABASE_DRIVER_ERROR( err_message, 430023 );
                }
            }
        }
        return val;
    }

    case SQL_LONGVARBINARY: {
        CDB_Image* val = new CDB_Image;
        SQLLEN f;
        for(;;) {
            switch(SQLGetData(GetHandle(), m_CurrItem+1, SQL_C_BINARY, buffer, sizeof(buffer), &f)) {
            case SQL_SUCCESS_WITH_INFO:
                if(f == SQL_NO_TOTAL) f= sizeof(buffer);
                else if(f < 0) ReportErrors();
            case SQL_SUCCESS:
                if(f > 0) {
                    if(f > sizeof(buffer)) f= sizeof(buffer);
                    val->Append(buffer, f);
                }
                continue;
            case SQL_NO_DATA:
                break;
            case SQL_ERROR:
                ReportErrors();
            default:
                {
                    string err_message = "SQLGetData failed while retrieving text into CDB_Image" + 
                        GetDiagnosticInfo();
                    DATABASE_DRIVER_ERROR( err_message, 430024 );
                }
            }
        }
        return val;
    }
    default:
        {
            string err_message = "Unsupported column type" + GetDiagnosticInfo();
            DATABASE_DRIVER_ERROR( err_message, 430025 );
        }

    }
}


CDB_Object* CODBC_RowResult::GetItem(CDB_Object* item_buf)
{
    if ((unsigned int) m_CurrItem >= m_NofCols  ||  m_CurrItem == -1) {
        return 0;
    }

    CDB_Object* item = item_buf? xLoadItem(item_buf) : xMakeItem();

    ++m_CurrItem;
    return item;
}


size_t CODBC_RowResult::ReadItem(void* buffer,size_t buffer_size,bool* is_null)
{
    if ((unsigned int) m_CurrItem >= m_NofCols  ||  m_CurrItem == -1 ||
        buffer == 0 || buffer_size == 0) {
        return 0;
    }

    SQLLEN f = 0;

    if(is_null) *is_null= false;

    switch(SQLGetData(GetHandle(), m_CurrItem+1, SQL_C_BINARY, buffer, buffer_size, &f)) {
    case SQL_SUCCESS_WITH_INFO:
        switch(f) {
        case SQL_NO_TOTAL:
            return buffer_size;
        case SQL_NULL_DATA:
            ++m_CurrItem;
            if(is_null) *is_null= true;
            return 0;
        default:
            if ( f >= 0 ) {
                return (static_cast<size_t>(f) <= buffer_size) ? 
                    static_cast<size_t>(f) : buffer_size;
            }
            ReportErrors();
            return 0;
        }
    case SQL_SUCCESS:
        ++m_CurrItem;
        if(f == SQL_NULL_DATA) {
            if(is_null) *is_null= true;
            return 0;
        }
        return (f >= 0)? ((size_t)f) : 0;
    case SQL_NO_DATA:
        ++m_CurrItem;
        if(f == SQL_NULL_DATA) {
            if(is_null) *is_null= true;
        }
        return 0;
    case SQL_ERROR:
        ReportErrors();
    default:
        {
            string err_message = "SQLGetData failed " + GetDiagnosticInfo();
            DATABASE_DRIVER_ERROR( err_message, 430026 );
        }
    }
}


CDB_ITDescriptor* CODBC_RowResult::GetImageOrTextDescriptor(int item_no,
                                                            const string& cond)
{
    char* buffer[128];
    SQLSMALLINT slp;

    switch(SQLColAttribute(GetHandle(), item_no+1,
                           SQL_DESC_BASE_TABLE_NAME,
                           (SQLPOINTER)buffer, sizeof(buffer),
                           &slp, 0)) {
    case SQL_SUCCESS_WITH_INFO:
        ReportErrors();
    case SQL_SUCCESS:
        break;
    case SQL_ERROR:
        ReportErrors();
        return 0;
    default:
        {
            string err_message = "SQLColAttribute failed" + GetDiagnosticInfo();
            DATABASE_DRIVER_ERROR( err_message, 430027 );
        }
    }
    string base_table=(const char*)buffer;

    switch(SQLColAttribute(GetHandle(), item_no+1,
                           SQL_DESC_BASE_COLUMN_NAME,
                           (SQLPOINTER)buffer, sizeof(buffer),
                           &slp, 0)) {
    case SQL_SUCCESS_WITH_INFO:
        ReportErrors();
    case SQL_SUCCESS:
        break;
    case SQL_ERROR:
        ReportErrors();
        return 0;
    default:
        {
            string err_message = "SQLColAttribute failed" + GetDiagnosticInfo();
            DATABASE_DRIVER_ERROR( err_message, 430027 );
        }
    }
    string base_column=(const char*)buffer;

    return new CDB_ITDescriptor(base_table, base_column, cond);
}

I_ITDescriptor* CODBC_RowResult::GetImageOrTextDescriptor()
{
    return (I_ITDescriptor*) GetImageOrTextDescriptor(m_CurrItem,
                                                      "don't use me");
}

bool CODBC_RowResult::SkipItem()
{
    if ((unsigned int) m_CurrItem < m_NofCols) {
        ++m_CurrItem;
        return true;
    }
    return false;
}


CODBC_RowResult::~CODBC_RowResult()
{
    try {
        if (m_ColFmt) {
            delete[] m_ColFmt;
            m_ColFmt = 0;
        }
        if (!m_EOR) {
            Close();
        }
    }
    NCBI_CATCH_ALL( kEmptyStr )
}


/////////////////////////////////////////////////////////////////////////////
//
//  CTL_ParamResult::
//  CTL_StatusResult::
//  CTL_CursorResult::
//

CODBC_StatusResult::CODBC_StatusResult(CStatementBase& stmt)
: CODBC_RowResult(stmt, 1, NULL)
{
}

CODBC_StatusResult::~CODBC_StatusResult()
{
}

EDB_ResType CODBC_StatusResult::ResultType() const
{
    return eDB_StatusResult;
}

/////////////////////////////////////////////////////////////////////////////
CODBC_ParamResult::CODBC_ParamResult(
    CStatementBase& stmt,
    SQLSMALLINT nof_cols)
: CODBC_RowResult(stmt, nof_cols, NULL)
{
}

CODBC_ParamResult::~CODBC_ParamResult()
{
}

EDB_ResType CODBC_ParamResult::ResultType() const
{
    return eDB_ParamResult;
}


/////////////////////////////////////////////////////////////////////////////
//
//  CODBC_CursorResult::
//

CODBC_CursorResult::CODBC_CursorResult(CODBC_LangCmd* cmd) 
: m_Cmd(cmd)
, m_Res(NULL)
, m_EOR(false)
{
    try {
        m_Cmd->Send();
        m_EOR = true;

        while (m_Cmd->HasMoreResults()) {
            m_Res = m_Cmd->Result();

            if (m_Res && m_Res->ResultType() == eDB_RowResult) {
                m_EOR = false;
                return;
            }

            if (m_Res) {
                while (m_Res->Fetch())
                    ;
                delete m_Res;
                m_Res = 0;
            }
        }
    } catch (const CDB_Exception& e) {
        string err_message = "failed to get the results" + GetDiagnosticInfo();
        DATABASE_DRIVER_ERROR_EX( e, err_message, 422010 );
    }
}


EDB_ResType CODBC_CursorResult::ResultType() const
{
    return eDB_CursorResult;
}


unsigned int CODBC_CursorResult::NofItems() const
{
    return m_Res ? m_Res->NofItems() : 0;
}


const char* CODBC_CursorResult::ItemName(unsigned int item_num) const
{
    return m_Res ? m_Res->ItemName(item_num) : 0;
}


size_t CODBC_CursorResult::ItemMaxSize(unsigned int item_num) const
{
    return m_Res ? m_Res->ItemMaxSize(item_num) : 0;
}


EDB_Type CODBC_CursorResult::ItemDataType(unsigned int item_num) const
{
    return m_Res ? m_Res->ItemDataType(item_num) : eDB_UnsupportedType;
}


bool CODBC_CursorResult::Fetch()
{

    if( m_EOR ) {
        return false;
    }

    try {
        if (m_Res && m_Res->Fetch()) {
            return true;
        }
    } catch ( const CDB_Exception& ) {
        delete m_Res;
        m_Res = 0;
    }

    try {
        // finish this command
        m_EOR = true;
        if( m_Res ) {
            delete m_Res;
            m_Res = 0;
            while (m_Cmd->HasMoreResults()) {
                m_Res = m_Cmd->Result();
                if (m_Res) {
                    while (m_Res->Fetch())
                        ;
                    delete m_Res;
                    m_Res = 0;
                }
            }
        }

        // send the another "fetch cursor_name" command
        m_Cmd->Send();
        while (m_Cmd->HasMoreResults()) {
            m_Res = m_Cmd->Result();
            if (m_Res && m_Res->ResultType() == eDB_RowResult) {
                m_EOR = false;
                return m_Res->Fetch();
            }
            if ( m_Res ) {
                while (m_Res->Fetch())
                    ;
                delete m_Res;
                m_Res = 0;
            }
        }
    } catch (const CDB_Exception& e) {
        string err_message = "Failed to fetch the results" + GetDiagnosticInfo();
        DATABASE_DRIVER_ERROR_EX( e, err_message, 422011 );
    }
    return false;
}


int CODBC_CursorResult::CurrentItemNo() const
{
    return m_Res ? m_Res->CurrentItemNo() : -1;
}


int CODBC_CursorResult::GetColumnNum(void) const
{
    return m_Res ? m_Res->GetColumnNum() : -1;
}


CDB_Object* CODBC_CursorResult::GetItem(CDB_Object* item_buff)
{
    return m_Res ? m_Res->GetItem(item_buff) : 0;
}


size_t CODBC_CursorResult::ReadItem(void* buffer, size_t buffer_size,
                                   bool* is_null)
{
    if (m_Res) {
        return m_Res->ReadItem(buffer, buffer_size, is_null);
    }
    if (is_null)
        *is_null = true;
    return 0;
}


I_ITDescriptor* CODBC_CursorResult::GetImageOrTextDescriptor()
{
    return m_Res ? m_Res->GetImageOrTextDescriptor() : 0;
}


bool CODBC_CursorResult::SkipItem()
{
    return m_Res ? m_Res->SkipItem() : false;
}


CODBC_CursorResult::~CODBC_CursorResult()
{
    try {
        if (m_Res) {
            delete m_Res;
            m_Res = 0;
        }
    }
    NCBI_CATCH_ALL( kEmptyStr )
}


END_NCBI_SCOPE



/*
 * ===========================================================================
 * $Log$
 * Revision 1.28  2006/02/28 15:00:45  ssikorsk
 * Use larger type (SQLLEN) instead of SQLINTEGER where it needs to be converted to a pointer.
 *
 * Revision 1.27  2006/02/28 14:27:30  ssikorsk
 * Replaced int/SQLINTEGER variables with SQLLEN where needed.
 *
 * Revision 1.26  2006/02/28 14:00:47  ssikorsk
 * Fixed argument type misuse (like SQLINTEGER and SQLLEN) for vc8-x64 sake.
 *
 * Revision 1.25  2005/11/28 13:22:59  ssikorsk
 * Report SQL statement and database connection parameters in case
 * of an error in addition to a server error message.
 *
 * Revision 1.24  2005/11/02 16:46:21  ssikorsk
 * Pass context information with an error message of a database exception.
 *
 * Revision 1.23  2005/11/02 13:51:05  ssikorsk
 * Rethrow catched CDB_Exception to preserve useful information.
 *
 * Revision 1.22  2005/11/02 12:58:38  ssikorsk
 * Report extra information in exceptions and error messages.
 *
 * Revision 1.21  2005/10/31 12:25:21  ssikorsk
 * Get rid of warnings.
 *
 * Revision 1.20  2005/09/19 14:19:05  ssikorsk
 * Use NCBI_CATCH_ALL macro instead of catch(...)
 *
 * Revision 1.19  2005/09/15 11:00:02  ssikorsk
 * Destructors do not throw exceptions any more.
 *
 * Revision 1.18  2005/09/07 11:06:32  ssikorsk
 * Added a GetColumnNum implementation
 *
 * Revision 1.17  2005/04/07 18:09:04  soussov
 * fixes bug with RowCount in RowResult
 *
 * Revision 1.16  2005/04/04 13:03:57  ssikorsk
 * Revamp of DBAPI exception class CDB_Exception
 *
 * Revision 1.15  2005/02/15 16:07:51  ssikorsk
 * Fixed a bug with GetRowCount plus SELECT statement
 *
 * Revision 1.14  2005/02/01 18:01:20  soussov
 * fixes copy-paste bug
 *
 * Revision 1.13  2005/01/31 21:48:27  soussov
 * fixes bug in xMakeItem. It should return CDB_SmallDateTime if DecimalDigits == 0
 *
 * Revision 1.12  2004/05/17 21:16:06  gorelenk
 * Added include of PCH ncbi_pch.hpp
 *
 * Revision 1.11  2004/01/30 20:00:39  soussov
 * fixes GetItem for CDB_Numeric
 *
 * Revision 1.10  2003/12/09 17:35:29  sapojnik
 * data sizes returned by SQLGetData() adjusted to fit within the buffer
 *
 * Revision 1.9  2003/12/09 15:42:15  sapojnik
 * CODBC_RowResult::ReadItem(): * was missing in *is_null=false; corrected
 *
 * Revision 1.8  2003/11/25 20:09:06  soussov
 * fixes bug in ReadItem: it did return the text/image size instead of number of bytes in buffer in some cases
 *
 * Revision 1.7  2003/05/08 20:30:24  soussov
 * CDB_LongChar CDB_LongBinary added
 *
 * Revision 1.6  2003/01/31 16:51:03  lavr
 * Remove unused variable "e" from catch() clause
 *
 * Revision 1.5  2003/01/06 16:59:20  soussov
 * sets m_CurrItem = -1 for all result types if no fetch was called
 *
 * Revision 1.4  2003/01/03 21:48:37  soussov
 * set m_CurrItem = -1 if fetch failes
 *
 * Revision 1.3  2003/01/02 21:05:35  soussov
 * SQL_BIGINT added in CODBC_RowResult::xLoadItem
 *
 * Revision 1.2  2002/07/05 15:15:10  soussov
 * fixes bug in ReadItem
 *
 *
 * ===========================================================================
 */
