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
 * Author:  Anton Butanayev
 *
 * File Description:  Driver for MySQL server
 *
 */

#include <iostream>
#include <corelib/ncbimtx.hpp>
#include <dbapi/driver/mysql/interfaces.hpp>

BEGIN_NCBI_SCOPE

static EDB_Type s_GetDataType(enum_field_types type)
{
  switch (type)
  {
  case FIELD_TYPE_TINY: return eDB_TinyInt;
  case FIELD_TYPE_SHORT: return eDB_SmallInt;
  case FIELD_TYPE_LONG: return eDB_Int;
  case FIELD_TYPE_INT24: return eDB_Int;
  case FIELD_TYPE_LONGLONG: return eDB_Int;
  case FIELD_TYPE_DECIMAL: return eDB_Numeric;
  case FIELD_TYPE_FLOAT: return eDB_Float;
  case FIELD_TYPE_DOUBLE: return eDB_Double;
  case FIELD_TYPE_TIMESTAMP: return eDB_DateTime;
  case FIELD_TYPE_DATE: return eDB_SmallDateTime;
  case FIELD_TYPE_TIME: return eDB_UnsupportedType;
  case FIELD_TYPE_DATETIME: return eDB_DateTime;
  case FIELD_TYPE_YEAR: return eDB_UnsupportedType;
  case FIELD_TYPE_STRING:
  case FIELD_TYPE_VAR_STRING:
    return eDB_VarChar;
  case FIELD_TYPE_BLOB: return eDB_Image;
  case FIELD_TYPE_SET: return eDB_UnsupportedType;
  case FIELD_TYPE_ENUM: return eDB_UnsupportedType;
  case FIELD_TYPE_NULL: return eDB_UnsupportedType;
  default:
    return eDB_UnsupportedType;
  }
}

CMySQL_RowResult::CMySQL_RowResult(CMySQL_Connection *conn) :
m_Connect(conn),
m_CurrItem(0)
{
  m_Result = mysql_use_result(&m_Connect->m_MySQL);
  if(m_Result == NULL)
    throw CDB_ClientEx(eDB_Warning, 800004,
                       "CMySQL_RowResult::CMySQL_RowResult",
                       "Failed: mysql_use_result");

  m_NofCols = mysql_num_fields(m_Result);
  m_ColFmt = new SMySQL_ColDescr[m_NofCols];
  MYSQL_FIELD *fields = mysql_fetch_fields(m_Result);
  for(unsigned int n = 0; n < m_NofCols; n++)
  {
    m_ColFmt[n].max_length = fields[n].max_length;
    m_ColFmt[n].data_type = s_GetDataType(fields[n].type);
    m_ColFmt[n].col_name = fields[n].name;
  }
}

CMySQL_RowResult::~CMySQL_RowResult()
{}

EDB_ResType CMySQL_RowResult::ResultType() const
{
  return eDB_RowResult;
}

unsigned int CMySQL_RowResult::NofItems() const
{
  return m_NofCols;
}

const char *CMySQL_RowResult::ItemName (unsigned int item_num) const
{
  return item_num < m_NofCols ? m_ColFmt[item_num].col_name.c_str() : 0;
}

size_t CMySQL_RowResult::ItemMaxSize (unsigned int item_num) const
{
  return item_num < m_NofCols ? m_ColFmt[item_num].max_length : 0;
}


EDB_Type CMySQL_RowResult::ItemDataType(unsigned int item_num) const
{
  return item_num < m_NofCols ? m_ColFmt[item_num].data_type : eDB_UnsupportedType;
}

bool CMySQL_RowResult::Fetch()
{
  bool fetched = (m_Row = mysql_fetch_row(m_Result));
  if(fetched && m_Row == NULL)
    throw CDB_ClientEx(eDB_Warning, 800005,
                       "CMySQL_RowResult::Fetch",
                       "Failed: mysql_fetch_row");
  if(fetched)
  {
    m_Lengths = mysql_fetch_lengths(m_Result);
    if(m_Lengths == NULL)
      throw CDB_ClientEx(eDB_Warning, 800006,
                         "CMySQL_RowResult::Fetch",
                         "Failed: mysql_fetch_lengths");
  }
  m_CurrItem = 0;
  return fetched;
}

int CMySQL_RowResult::CurrentItemNo() const
{
  return m_CurrItem;
}

static CDB_Object* s_GetItem(EDB_Type data_type, CDB_Object* item_buff,
                             EDB_Type b_type, const char *d_ptr, size_t d_len)
{
  if(! d_ptr)
    d_ptr = "";

  if(b_type == eDB_VarChar)
  {
    if(! item_buff)
      item_buff = new CDB_VarChar;
    if(d_len)
      ((CDB_VarChar*)item_buff)->SetValue(d_ptr, d_len);
    else
      item_buff->AssignNULL();
    return item_buff;
  }

  long intVal;
  double doubleVal;

  switch (data_type)
  {
  case eDB_TinyInt:
  case eDB_SmallInt:
  case eDB_Int:
    intVal = NStr::StringToInt(d_ptr);
    break;

  case eDB_Float:
  case eDB_Double:
    doubleVal = NStr::StringToDouble(d_ptr);
    break;
  }

  switch (b_type)
  {
  case eDB_TinyInt:
    {
      if(item_buff)
        *((CDB_TinyInt*)item_buff) = (Uint1)intVal;
      else
        item_buff = new CDB_TinyInt((Uint1)intVal);
      break;
    }

  case eDB_SmallInt:
    {
      if(item_buff)
        *((CDB_SmallInt*)item_buff) = (Uint2)intVal;
      else
        item_buff = new CDB_SmallInt((Uint2)intVal);
      break;
    }

  case eDB_Int:
    {
      if(item_buff)
        *((CDB_Int*)item_buff) = (Uint4)intVal;
      else
        item_buff = new CDB_Int((Uint4)intVal);
      break;
    }

  case eDB_Float:
    {
      if(item_buff)
        *((CDB_Float*)item_buff) = (float)doubleVal;
      else
        item_buff = new CDB_Float((float)doubleVal);
      break;
    }

  case eDB_Double:
    {
      if(item_buff)
        *((CDB_Double*)item_buff) = doubleVal;
      else
        item_buff = new CDB_Double(doubleVal);
      break;
    }

  case eDB_DateTime:
    {
      CTime time;
      if(d_len)
        time = CTime(d_ptr, "Y-M-D h:m:s");

      if(item_buff)
        *(CDB_DateTime*)item_buff = time;
      else
        item_buff = new CDB_DateTime(time);
      break;
    }

  case eDB_SmallDateTime:
    {
      CTime time;
      if(d_len)
        time = CTime(d_ptr, "Y-M-D");

      if(item_buff)
        *(CDB_SmallDateTime*)item_buff = time;
      else
        item_buff = new CDB_SmallDateTime(time);
      break;
    }
  }

  if(d_len == 0 && item_buff)
    item_buff->AssignNULL();

  return item_buff;
}

CDB_Object *CMySQL_RowResult::GetItem(CDB_Object* item_buf)
{
  if((unsigned int)m_CurrItem >= m_NofCols)
    return 0;

  CDB_Object *r =
    s_GetItem(m_ColFmt[m_CurrItem].data_type, item_buf,
              (item_buf ? item_buf->GetType() : eDB_UnsupportedType),
              m_Row[m_CurrItem], m_Lengths[m_CurrItem]);
  ++m_CurrItem;
  return r;
}

size_t CMySQL_RowResult::ReadItem(void* buffer, size_t buffer_size, bool *is_null = 0)
{
  return 0;
}

I_ITDescriptor *CMySQL_RowResult::GetImageOrTextDescriptor()
{
  return 0;
}

bool CMySQL_RowResult::SkipItem()
{
  return false;
}

END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.2  2002/08/28 17:18:20  butanaev
 * Improved error handling, demo app.
 *
 * Revision 1.1  2002/08/13 20:23:14  butanaev
 * The beginning.
 *
 * ===========================================================================
 */
