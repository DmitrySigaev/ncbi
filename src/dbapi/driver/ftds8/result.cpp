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
 * File Description:   TDS Results
 *
 */

#include <ncbi_pch.hpp>
#include <dbapi/driver/ftds/interfaces.hpp>
#include <dbapi/driver/dblib/interfaces_p.hpp>
#include <dbapi/driver/util/numeric_convert.hpp>


BEGIN_NCBI_SCOPE


// Aux. for s_*GetItem()
static CDB_Object* s_GenericGetItem(EDB_Type data_type, CDB_Object* item_buff,
                                    EDB_Type b_type, BYTE* d_ptr, DBINT d_len)
{
    switch (data_type) {
    case eDB_VarBinary: {
        if (item_buff) {
            switch (b_type) {
            case eDB_VarBinary:
                ((CDB_VarBinary*) item_buff)->SetValue((const void*) d_ptr,
                                                       (size_t) d_len);
                break;
            case eDB_Binary:
                ((CDB_Binary*)    item_buff)->SetValue((const void*) d_ptr,
                                                       (size_t) d_len);
                break;
            case eDB_VarChar:
                ((CDB_VarChar*)   item_buff)->SetValue((const char*) d_ptr,
                                                       (size_t) d_len);
                break;
            case eDB_Char:
                ((CDB_Char*)      item_buff)->SetValue((const char*) d_ptr,
                                                       (size_t) d_len);
                break;
            case eDB_LongBinary:
                ((CDB_LongBinary*)item_buff)->SetValue((const void*) d_ptr,
                                                       (size_t) d_len);
                break;
            case eDB_LongChar:
                ((CDB_LongChar*)  item_buff)->SetValue((const char*) d_ptr,
                                                       (size_t) d_len);
                break;

            default:
                DATABASE_DRIVER_ERROR( "wrong type of CDB_Object", 230020 );
            }
            return item_buff;
        }

        return d_ptr ? new CDB_VarBinary((const void*) d_ptr, (size_t) d_len)
            : new CDB_VarBinary();
    }

    case eDB_Bit: {
        DBBIT* v = (DBBIT*) d_ptr;
        if (item_buff) {
            if (v) {
                switch (b_type) {
                case eDB_Bit:
                    *((CDB_Bit*)      item_buff) = (int) *v;
                    break;
                case eDB_TinyInt:
                    *((CDB_TinyInt*)  item_buff) = *v ? 1 : 0;
                    break;
                case eDB_SmallInt:
                    *((CDB_SmallInt*) item_buff) = *v ? 1 : 0;
                    break;
                case eDB_Int:
                    *((CDB_Int*)      item_buff) = *v ? 1 : 0;
                    break;
                default:
                    DATABASE_DRIVER_ERROR( "wrong type of CDB_Object", 230020 );
                }
            } else
                item_buff->AssignNULL();
            return item_buff;
        }

        return v ? new CDB_Bit((int) *v) : new CDB_Bit;
    }

    case eDB_VarChar: {
        if (item_buff) {
            switch (b_type) {
            case eDB_VarChar:
                ((CDB_VarChar*)   item_buff)->SetValue((const char*) d_ptr,
                                                       (size_t) d_len);
                break;
            case eDB_Char:
                ((CDB_Char*)      item_buff)->SetValue((const char*) d_ptr,
                                                       (size_t) d_len);
                break;
            case eDB_LongChar:
                ((CDB_LongChar*)  item_buff)->SetValue((const char*) d_ptr,
                                                       (size_t) d_len);
                break;
            case eDB_VarBinary:
                ((CDB_VarBinary*) item_buff)->SetValue((const void*) d_ptr,
                                                       (size_t) d_len);
                break;
            case eDB_Binary:
                ((CDB_Binary*)    item_buff)->SetValue((const void*) d_ptr,
                                                       (size_t) d_len);
                break;
            case eDB_LongBinary:
                ((CDB_LongBinary*)item_buff)->SetValue((const void*) d_ptr,
                                                       (size_t) d_len);
                break;
            default:
                DATABASE_DRIVER_ERROR( "wrong type of CDB_Object", 230020 );
            }
            return item_buff;
        }

        return d_ptr ? new CDB_VarChar((const char*) d_ptr, (size_t) d_len)
            : new CDB_VarChar();
    }

    case eDB_DateTime: {
        DBDATETIME* v = (DBDATETIME*) d_ptr;
        if (item_buff) {
            if (b_type != eDB_DateTime) {
                DATABASE_DRIVER_ERROR( "wrong type of CDB_Object", 230020 );
            }
            if (v)
                ((CDB_DateTime*) item_buff)->Assign(v->dtdays, v->dttime);
            else
                item_buff->AssignNULL();
            return item_buff;
        }

        return v ? new CDB_DateTime(v->dtdays, v->dttime) : new CDB_DateTime;
    }

    case eDB_SmallDateTime: {
        DBDATETIME4* v = (DBDATETIME4*)d_ptr;
        if (item_buff) {
            if (v) {
                switch (b_type) {
                case eDB_SmallDateTime:
                    ((CDB_SmallDateTime*) item_buff)->Assign
                        (v->days,             v->minutes);
                    break;
                case eDB_DateTime:
                    ((CDB_DateTime*)      item_buff)->Assign
                        ((int) v->days, (int) v->minutes*60*300);
                    break;
                default:
                    DATABASE_DRIVER_ERROR( "wrong type of CDB_Object", 230020 );
                }
            } else
                item_buff->AssignNULL();
            return item_buff;
        }

        return v ?
            new CDB_SmallDateTime(v->days, v->minutes) : new CDB_SmallDateTime;
    }

    case eDB_TinyInt: {
        DBTINYINT* v = (DBTINYINT*) d_ptr;
        if (item_buff) {
            if (v) {
                switch (b_type) {
                case eDB_TinyInt:
                    *((CDB_TinyInt*)  item_buff) = (Uint1) *v;
                    break;
                case eDB_SmallInt:
                    *((CDB_SmallInt*) item_buff) = (Int2)  *v;
                    break;
                case eDB_Int:
                    *((CDB_Int*)      item_buff) = (Int4)  *v;
                    break;
                default:
                    DATABASE_DRIVER_ERROR( "wrong type of CDB_Object", 230020 );
                }
            } else
                item_buff->AssignNULL();
            return item_buff;
        }

        return v ? new CDB_TinyInt((Uint1) *v) : new CDB_TinyInt;
    }

    case eDB_SmallInt: {
        DBSMALLINT* v = (DBSMALLINT*) d_ptr;
        if (item_buff) {
            if (v) {
                switch (b_type) {
                case eDB_SmallInt:
                    *((CDB_SmallInt*) item_buff) = (Int2) *v;
                    break;
                case eDB_Int:
                    *((CDB_Int*)      item_buff) = (Int4) *v;
                    break;
                default:
                    DATABASE_DRIVER_ERROR( "wrong type of CDB_Object", 230020 );
                }
            } else
                item_buff->AssignNULL();
            return item_buff;
        }

        return v ? new CDB_SmallInt((Int2) *v) : new CDB_SmallInt;
    }

    case eDB_Int: {
        DBINT* v = (DBINT*) d_ptr;
        if (item_buff) {
            if (v) {
                if (b_type == eDB_Int)
                    *((CDB_Int*) item_buff) = (Int4) *v;
                else {
                    DATABASE_DRIVER_ERROR( "wrong type of CDB_Object", 230020 );
                }
            } else
                item_buff->AssignNULL();
            return item_buff;
        }

        return v ? new CDB_Int((Int4) *v) : new CDB_Int;
    }

    case eDB_Double: {
        if (item_buff  &&  b_type != eDB_Double) {
            DATABASE_DRIVER_ERROR( "wrong type of CDB_Object", 130020 );
        }
        DBFLT8 v;
        if(d_ptr) memcpy((void*)&v, d_ptr, sizeof(DBFLT8));
        if (item_buff) {
            if (d_ptr)
                *((CDB_Double*) item_buff) = (double) v;
            else
                item_buff->AssignNULL();
            return item_buff;
        }

        return d_ptr ? new CDB_Double((double)v) : new CDB_Double;
    }

    case eDB_Float: {
        if (item_buff  &&  b_type != eDB_Float) {
            DATABASE_DRIVER_ERROR( "wrong type of CDB_Object", 130020 );
        }
        DBREAL* v = (DBREAL*) d_ptr;
        if (item_buff) {
            if (v)
                *((CDB_Float*) item_buff) = (float) *v;
            else
                item_buff->AssignNULL();
            return item_buff;
        }

        return v ? new CDB_Float((float) *v) : new CDB_Float;
    }

    case eDB_LongBinary: {
        if (item_buff) {
            switch (b_type) {
            case eDB_LongBinary:
                ((CDB_LongBinary*)item_buff)->SetValue((const void*) d_ptr,
                                                       (size_t) d_len);
                break;
            case eDB_LongChar:
                ((CDB_LongChar*)  item_buff)->SetValue((const char*) d_ptr,
                                                       (size_t) d_len);
                break;

            default:
                DATABASE_DRIVER_ERROR( "wrong type of CDB_Object", 230020 );
            }
            return item_buff;
        }

        return d_ptr ? new CDB_LongBinary((size_t) d_len, (const void*) d_ptr,
                                          (size_t) d_len)
            : new CDB_LongBinary();
    }

    case eDB_LongChar: {
        if (item_buff) {
            switch (b_type) {
            case eDB_LongChar:
                ((CDB_LongChar*)  item_buff)->SetValue((const char*) d_ptr,
                                                       (size_t) d_len);
                break;
            case eDB_LongBinary:
                ((CDB_LongBinary*)item_buff)->SetValue((const void*) d_ptr,
                                                       (size_t) d_len);
                break;
            case eDB_VarChar:
                if (d_len < 256) {
                    ((CDB_VarChar*)  item_buff)->SetValue((const char*) d_ptr,
                                                           (size_t) d_len);
                } else {
                    DATABASE_DRIVER_ERROR( "Invalid conversion to CDB_VarChar type", 230021 );
                }
                break;
            default:
                DATABASE_DRIVER_ERROR( "wrong type of CDB_Object", 230020 );
            }
            return item_buff;
        }

        return d_ptr ? new CDB_LongChar((size_t) d_len, (const char*) d_ptr)
            : new CDB_LongChar();
    }

    default:
        return 0;
    }
}


/////////////////////////////////////////////////////////////////////////////
//
//  CTDS_RowResult::
//


CTDS_RowResult::CTDS_RowResult(CDBL_Connection& conn,
                               DBPROCESS* cmd,
                               unsigned int* res_status,
                               bool need_init) :
    CDBL_Result(conn, cmd),
    m_CurrItem(-1),
    m_EOR(false),
    m_ResStatus(res_status),
    m_Offset(0)
{
    if (!need_init)
        return;

    m_NofCols = Check(dbnumcols(cmd));
    m_CmdNum = DBCURCMD(cmd);

    m_ColFmt = new STDS_ColDescr[m_NofCols];
    for (unsigned int n = 0;  n < m_NofCols;  n++) {
        m_ColFmt[n].max_length = Check(dbcollen(GetCmd(), n + 1));
        m_ColFmt[n].data_type = GetDataType(n + 1);
        char* s = Check(dbcolname(GetCmd(), n + 1));
        m_ColFmt[n].col_name = s ? s : "";
    }
}


EDB_ResType CTDS_RowResult::ResultType() const
{
    return eDB_RowResult;
}


unsigned int CTDS_RowResult::NofItems() const
{
    return m_NofCols;
}


const char* CTDS_RowResult::ItemName(unsigned int item_num) const
{
    return item_num < m_NofCols ? m_ColFmt[item_num].col_name.c_str() : 0;
}


size_t CTDS_RowResult::ItemMaxSize(unsigned int item_num) const
{
    return item_num < m_NofCols ? m_ColFmt[item_num].max_length : 0;
}


EDB_Type CTDS_RowResult::ItemDataType(unsigned int item_num) const
{
    return item_num < m_NofCols
        ? m_ColFmt[item_num].data_type : eDB_UnsupportedType;
}


bool CTDS_RowResult::Fetch()
{
    m_CurrItem = -1;
    if ( m_EOR )
        return false;

    switch ( Check(dbnextrow(GetCmd())) ) {
    case REG_ROW:
        m_CurrItem = 0;
        m_Offset = 0;
        return true;
    case NO_MORE_ROWS:
        m_EOR = true;
        break;
    case FAIL:
        DATABASE_DRIVER_ERROR( "error in fetching row", 230003 );
    case BUF_FULL:
        DATABASE_DRIVER_ERROR( "buffer is full", 230006 );
    default:
        *m_ResStatus |= 0x10;
        m_EOR = true;
        break;
    }
    return false;
}


int CTDS_RowResult::CurrentItemNo() const
{
    return m_CurrItem;
}


int CTDS_RowResult::GetColumnNum(void) const
{
    return m_NofCols;
}


CDB_Object* CTDS_RowResult::GetItem(CDB_Object* item_buff)
{
    if ((unsigned int) m_CurrItem >= m_NofCols) {
        return 0;
    }
    CDB_Object* r = GetItemInternal(m_CurrItem + 1,
                              &m_ColFmt[m_CurrItem], item_buff);
    ++m_CurrItem;
    m_Offset = 0;
    return r;
}


size_t CTDS_RowResult::ReadItem(void* buffer, size_t buffer_size,bool* is_null)
{
    if ((unsigned int) m_CurrItem >= m_NofCols) {
        if (is_null)
            *is_null = true;
        return 0;
    }

    BYTE* d_ptr = Check(dbdata  (GetCmd(), m_CurrItem + 1));
    DBINT d_len = Check(dbdatlen(GetCmd(), m_CurrItem + 1));

    if (d_ptr == 0  ||  d_len < 1) { // NULL value
        ++m_CurrItem;
        m_Offset = 0;
        if (is_null)
            *is_null = true;
        return 0;
    }

    if (is_null)
        *is_null = false;
    if ((size_t) (d_len - m_Offset) < buffer_size) {
        buffer_size = d_len - m_Offset;
    }

    if (buffer)
        memcpy(buffer, d_ptr + m_Offset, buffer_size);
    m_Offset += buffer_size;
    if (m_Offset >= static_cast<size_t>(d_len)) {
        m_Offset = 0;
        ++m_CurrItem;
    }

    return buffer_size;
}


I_ITDescriptor* CTDS_RowResult::GetImageOrTextDescriptor()
{
    if ((unsigned int) m_CurrItem >= m_NofCols)
        return 0;
    return new CTDS_ITDescriptor(GetConnection(), GetCmd(), m_CurrItem + 1);
}


bool CTDS_RowResult::SkipItem()
{
    if ((unsigned int) m_CurrItem < m_NofCols) {
        ++m_CurrItem;
        m_Offset = 0;
        return true;
    }
    return false;
}


CTDS_RowResult::~CTDS_RowResult()
{
    try {
        if (m_ColFmt) {
            delete[] m_ColFmt;
            m_ColFmt = 0;
        }
        if (!m_EOR)
            Check(dbcanquery(GetCmd()));
    }
    NCBI_CATCH_ALL( kEmptyStr )
}



/////////////////////////////////////////////////////////////////////////////
//
//  CTDS_BlobResult::


CTDS_BlobResult::CTDS_BlobResult(CDBL_Connection& conn, DBPROCESS* cmd) :
    CDBL_Result(conn, cmd),
    m_CurrItem(-1),
    m_EOR(false)
{
    m_CmdNum = DBCURCMD(cmd);

    m_ColFmt.max_length = Check(dbcollen(GetCmd(), 1));
    m_ColFmt.data_type = GetDataType(1);
    char* s = Check(dbcolname(GetCmd(), 1));
    m_ColFmt.col_name = s ? s : "";
}

EDB_ResType CTDS_BlobResult::ResultType() const
{
    return eDB_RowResult;
}


unsigned int CTDS_BlobResult::NofItems() const
{
    return 1;
}


const char* CTDS_BlobResult::ItemName(unsigned int item_num) const
{
    return item_num ? 0 : m_ColFmt.col_name.c_str();
}


size_t CTDS_BlobResult::ItemMaxSize(unsigned int item_num) const
{
    return item_num ? 0 : m_ColFmt.max_length;
}


EDB_Type CTDS_BlobResult::ItemDataType(unsigned int) const
{
    return m_ColFmt.data_type;
}


bool CTDS_BlobResult::Fetch()
{
    if (m_EOR)
        return false;

    STATUS s;
    if (m_CurrItem == 0) {
        while ((s = Check(dbreadtext(GetCmd(), m_Buff, (DBINT) sizeof(m_Buff)))) > 0)
            ;
        switch (s) {
        case 0:
            break;
        case NO_MORE_ROWS:
            m_EOR = true;
            return false;
        default:
            DATABASE_DRIVER_ERROR( "error in fetching row", 280003 );
        }
    }
    else {
        m_CurrItem = 0;
    }
    s = Check(dbreadtext(GetCmd(), m_Buff, (DBINT) sizeof(m_Buff)));
    if (s == NO_MORE_ROWS)
        return false;
    if (s < 0) {
        DATABASE_DRIVER_ERROR( "error in fetching row", 280003 );
    }
    m_BytesInBuffer = s;
    m_ReadedBytes = 0;
    return true;
}


int CTDS_BlobResult::CurrentItemNo() const
{
    return m_CurrItem;
}


int CTDS_BlobResult::GetColumnNum(void) const
{
    return -1;
}


CDB_Object* CTDS_BlobResult::GetItem(CDB_Object* item_buff)
{
    if (m_CurrItem)
        return 0;

    EDB_Type b_type = item_buff ? item_buff->GetType() : eDB_UnsupportedType;

    if (item_buff  &&  b_type != eDB_Text  &&  b_type != eDB_Image) {
        DATABASE_DRIVER_ERROR( "wrong type of CDB_Object", 230020 );
    }

    if (item_buff == 0) {
        item_buff = m_ColFmt.data_type == eDB_Text
            ? (CDB_Object*) new CDB_Text : (CDB_Object*) new CDB_Image;
    }

    CDB_Text* val = (CDB_Text*) item_buff;

    // check if we do have something in buffer
    if (m_ReadedBytes < m_BytesInBuffer) {
        val->Append(m_Buff + m_ReadedBytes, m_BytesInBuffer - m_ReadedBytes);
        m_ReadedBytes = m_BytesInBuffer;
    }

    if (m_BytesInBuffer == 0) {
        return item_buff;
    }


    STATUS s;
    while ((s = Check(dbreadtext(GetCmd(), m_Buff, (DBINT) sizeof(m_Buff)))) > 0) {
        val->Append(m_Buff, (size_t(s) < sizeof(m_Buff))? size_t(s) : sizeof(m_Buff));
    }

    switch (s) {
    case NO_MORE_ROWS:
        m_EOR = true;
    case 0:
        m_CurrItem = 1;
        break;
    default:
        DATABASE_DRIVER_ERROR( "dbreadtext failed", 280003 );
    }

    return item_buff;
}


size_t CTDS_BlobResult::ReadItem(void* buffer, size_t buffer_size,
                                 bool* is_null)
{
    if (m_BytesInBuffer == 0)
        m_CurrItem = 1;

    if (m_CurrItem != 0) {
        if (is_null)
            *is_null = true;
        return 0;
    }

    size_t l = 0;
    // check if we do have something in buffer
    if (m_ReadedBytes < m_BytesInBuffer) {
        l = m_BytesInBuffer - m_ReadedBytes;
        if (l >= buffer_size) {
            memcpy(buffer, m_Buff + m_ReadedBytes, buffer_size);
            m_ReadedBytes += buffer_size;
            if (is_null)
                *is_null = false;
            return buffer_size;
        }
        memcpy(buffer, m_Buff + m_ReadedBytes, l);
    }

    STATUS s = Check(dbreadtext(GetCmd(),
                          (void*)((char*)buffer + l), (DBINT)(buffer_size-l)));
    switch (s) {
    case NO_MORE_ROWS:
        m_EOR = true;
    case 0:
        m_CurrItem = 1;
        break;
    case -1:
        DATABASE_DRIVER_ERROR( "dbreadtext failed", 280003 );
    default:
        break;
    }

    if (is_null) {
        *is_null = (m_BytesInBuffer == 0  &&  s <= 0);
    }
    return (size_t) s + l;
}


I_ITDescriptor* CTDS_BlobResult::GetImageOrTextDescriptor()
{
    if (m_CurrItem != 0)
        return 0;
    return new CTDS_ITDescriptor(GetConnection(), GetCmd(), 1);
}


bool CTDS_BlobResult::SkipItem()
{
    if (m_EOR  ||  m_CurrItem)
        return false;

    STATUS s;
    while ((s = Check(dbreadtext(GetCmd(), m_Buff, sizeof(m_Buff)))) > 0)
        continue;

    switch (s) {
    case NO_MORE_ROWS:
        m_EOR = true;
    case 0:
        m_CurrItem = 1;
        break;
    default:
        DATABASE_DRIVER_ERROR( "dbreadtext failed", 280003 );
    }
    return true;
}


CTDS_BlobResult::~CTDS_BlobResult()
{
    try {
        if (!m_EOR) {
            Check(dbcanquery(GetCmd()));
        }
    }
    NCBI_CATCH_ALL( kEmptyStr )
}


/////////////////////////////////////////////////////////////////////////////
//
//  CTDS_ParamResult::
//

CTDS_ParamResult::CTDS_ParamResult(CDBL_Connection& conn,
                                   DBPROCESS* cmd,
                                   int nof_params) :
    CTDS_RowResult(conn, cmd, 0, false)
{

    m_NofCols = nof_params;
    m_CmdNum = DBCURCMD(cmd);

    m_ColFmt = new STDS_ColDescr[m_NofCols];
    for (unsigned int n = 0;  n < m_NofCols;  n++) {
        m_ColFmt[n].max_length = 255;
        m_ColFmt[n].data_type = RetGetDataType(n + 1);
        const char* s = Check(dbretname(GetCmd(), n + 1));
        m_ColFmt[n].col_name = s ? s : "";
    }
    m_1stFetch = true;
}


EDB_ResType CTDS_ParamResult::ResultType() const
{
    return eDB_ParamResult;
}


bool CTDS_ParamResult::Fetch()
{
    if (m_1stFetch) { // we didn't get the items yet;
        m_1stFetch = false;
    m_CurrItem= 0;
        return true;
    }
    return false;
}


CDB_Object* CTDS_ParamResult::GetItem(CDB_Object* item_buff)
{
    if ((unsigned int) m_CurrItem >= m_NofCols) {
        return 0;
    }

    CDB_Object* r = RetGetItem(m_CurrItem + 1,
                                 &m_ColFmt[m_CurrItem], item_buff);
    ++m_CurrItem;
    m_Offset = 0;
    return r;
}


size_t CTDS_ParamResult::ReadItem(void* buffer, size_t buffer_size,
                                  bool* is_null)
{
    if ((unsigned int) m_CurrItem >= m_NofCols) {
        if (is_null)
            *is_null = true;
        return 0;
    }

    BYTE* d_ptr = Check(dbretdata(GetCmd(), m_CurrItem + 1));
    DBINT d_len = Check(dbretlen (GetCmd(), m_CurrItem + 1));

    if (d_ptr == 0  ||  d_len < 1) { // NULL value
        ++m_CurrItem;
        m_Offset = 0;
        if (is_null)
            *is_null = true;
        return 0;
    }

    if (is_null)
        *is_null = false;
    if ((size_t) (d_len - m_Offset) < buffer_size) {
        buffer_size = d_len - m_Offset;
    }

    memcpy(buffer, d_ptr + m_Offset, buffer_size);
    m_Offset += buffer_size;
    if ( m_Offset >= static_cast<size_t>(d_len) ) {
        m_Offset = 0;
        ++m_CurrItem;
    }
    return buffer_size;
}


I_ITDescriptor* CTDS_ParamResult::GetImageOrTextDescriptor()
{
    return 0;
}


CTDS_ParamResult::~CTDS_ParamResult()
{
    try {
        if (m_ColFmt) {
            delete[] m_ColFmt;
            m_ColFmt = 0;
        }
    }
    NCBI_CATCH_ALL( kEmptyStr )
}


/////////////////////////////////////////////////////////////////////////////
//
//  CTL_ComputeResult::
//

CTDS_ComputeResult::CTDS_ComputeResult(CDBL_Connection& conn,
                                       DBPROCESS* cmd,
                                       unsigned int* res_stat) :
    CTDS_RowResult(conn, cmd, res_stat, false)
{
    DATABASE_DRIVER_ERROR( "The compute results do not implemented in Free TDS", 270000 );
}


EDB_ResType CTDS_ComputeResult::ResultType() const
{
    return eDB_ComputeResult;
}


bool CTDS_ComputeResult::Fetch()
{
    return false; // do not implemented in Free TDS
}


int CTDS_ComputeResult::CurrentItemNo() const
{
    return m_CurrItem;
}


CDB_Object* CTDS_ComputeResult::GetItem(CDB_Object* /*item_buff*/)
{
    return 0; // not implemented in Free TDS
}


size_t CTDS_ComputeResult::ReadItem(void* /*buffer*/, size_t /*buffer_size*/,
                                    bool* /*is_null*/)
{
    return 0; // not implemented in Free TDS
}


I_ITDescriptor* CTDS_ComputeResult::GetImageOrTextDescriptor()
{
    return 0;
}


CTDS_ComputeResult::~CTDS_ComputeResult()
{
    return;
}



/////////////////////////////////////////////////////////////////////////////
//
//  CTL_StatusResult::
//


CTDS_StatusResult::CTDS_StatusResult(CDBL_Connection& conn, DBPROCESS* cmd) :
    CDBL_Result(conn, cmd),
    m_Offset(0),
    m_1stFetch(true)
{
    m_Val = Check(dbretstatus(cmd));
}

EDB_ResType CTDS_StatusResult::ResultType() const
{
    return eDB_StatusResult;
}


unsigned int CTDS_StatusResult::NofItems() const
{
    return 1;
}


const char* CTDS_StatusResult::ItemName(unsigned int) const
{
    return 0;
}


size_t CTDS_StatusResult::ItemMaxSize(unsigned int) const
{
    return sizeof(DBINT);
}


EDB_Type CTDS_StatusResult::ItemDataType(unsigned int) const
{
    return eDB_Int;
}


bool CTDS_StatusResult::Fetch()
{
    if (m_1stFetch) {
        m_1stFetch = false;
        return true;
    }
    return false;
}


int CTDS_StatusResult::CurrentItemNo() const
{
    return m_1stFetch? -1 : 0;
}


int CTDS_StatusResult::GetColumnNum(void) const
{
    return 1;
}


CDB_Object* CTDS_StatusResult::GetItem(CDB_Object* item_buff)
{
    if (!item_buff)
        return new CDB_Int(m_Val);

    if (item_buff->GetType() != eDB_Int) {
        DATABASE_DRIVER_ERROR( "wrong type of CDB_Object", 230020 );
    }

    CDB_Int* i = (CDB_Int*) item_buff;
    *i = m_Val;
    return item_buff;
}


size_t CTDS_StatusResult::ReadItem(void* buffer, size_t buffer_size,
                                   bool* is_null)
{
    if (is_null)
        *is_null = false;


    if (sizeof(int) <= (size_t) m_Offset)
        return 0;

    size_t l = sizeof(int) - m_Offset;

    char* p = (char*) &m_Val;

    if (buffer_size > l)
        buffer_size = l;
    memcpy(buffer, p + m_Offset, buffer_size);
    m_Offset += buffer_size;
    return buffer_size;
}


I_ITDescriptor* CTDS_StatusResult::GetImageOrTextDescriptor()
{
    return 0;
}


bool CTDS_StatusResult::SkipItem()
{
    return false;
}


CTDS_StatusResult::~CTDS_StatusResult()
{
    return;
}



/////////////////////////////////////////////////////////////////////////////
//
//  CTL_CursorResult::
//

CTDS_CursorResult::CTDS_CursorResult(CDBL_Connection& conn, CDB_LangCmd* cmd) :
    CDBL_Result(conn, NULL),
    m_Cmd(cmd),
    m_Res(0)
{
    try {
        GetCmd().Send();
        while (GetCmd().HasMoreResults()) {
            m_Res = GetCmd().Result();
            if (m_Res  &&  m_Res->ResultType() == eDB_RowResult) {
                return;
            }
            if (m_Res) {
                while (m_Res->Fetch()) {
                    continue;
                }
                delete m_Res;
                m_Res = 0;
            }
        }
    } catch ( const CDB_Exception& e ) {
        DATABASE_DRIVER_ERROR_EX( e, "failed to get the results", 222010 );
    }
}


EDB_ResType CTDS_CursorResult::ResultType() const
{
    return eDB_CursorResult;
}


unsigned int CTDS_CursorResult::NofItems() const
{
    return m_Res? m_Res->NofItems() : 0;
}


const char* CTDS_CursorResult::ItemName(unsigned int item_num) const
{
    return m_Res ? m_Res->ItemName(item_num) : 0;
}


size_t CTDS_CursorResult::ItemMaxSize(unsigned int item_num) const
{
    return m_Res ? m_Res->ItemMaxSize(item_num) : 0;
}


EDB_Type CTDS_CursorResult::ItemDataType(unsigned int item_num) const
{
    return m_Res ? m_Res->ItemDataType(item_num) : eDB_UnsupportedType;
}


bool CTDS_CursorResult::Fetch()
{
    if (!m_Res) {
        return false;
    }

    try {
        if (m_Res->Fetch()) {
            return true;
        }
    }
    catch (CDB_ClientEx& ex) {
        if (ex.GetDBErrCode() == 200003) {
            m_Res = 0;
        } else {
            DATABASE_DRIVER_ERROR( "Failed to fetch the results", 222011 );
        }
    }

    // try to get next cursor result
    try {
        // finish this command
        if (m_Res) {
            delete m_Res;
            m_Res = NULL;
        }

        while (GetCmd().HasMoreResults()) {
            auto_ptr<CDB_Result> res( GetCmd().Result() );
            if (res.get()) {
                while (res->Fetch())
                    continue;
            }
        }
        // send the another "fetch cursor_name" command
        GetCmd().Send();
        while (GetCmd().HasMoreResults()) {
            m_Res = GetCmd().Result();
            if (m_Res  &&  m_Res->ResultType() == eDB_RowResult) {
                return m_Res->Fetch();
            }
            if (m_Res) {
                while (m_Res->Fetch()) {
                    continue;
                }

                delete m_Res;
                m_Res = NULL;
            }
        }
    } catch ( const CDB_Exception& e ) {
        DATABASE_DRIVER_ERROR_EX( e, "Failed to fetch the results", 222011 );
    }
    return false;
}


int CTDS_CursorResult::CurrentItemNo() const
{
    return m_Res ? m_Res->CurrentItemNo() : -1;
}


int CTDS_CursorResult::GetColumnNum(void) const
{
    return m_Res ? m_Res->GetColumnNum() : -1;
}


CDB_Object* CTDS_CursorResult::GetItem(CDB_Object* item_buff)
{
    return m_Res ? m_Res->GetItem(item_buff) : 0;
}


size_t CTDS_CursorResult::ReadItem(void* buffer, size_t buffer_size,
                                   bool* is_null)
{
    if (m_Res) {
        return m_Res->ReadItem(buffer, buffer_size, is_null);
    }

    if (is_null) {
        *is_null = true;
    }

    return 0;
}


I_ITDescriptor* CTDS_CursorResult::GetImageOrTextDescriptor()
{
    return m_Res ? m_Res->GetImageOrTextDescriptor() : 0;
}


bool CTDS_CursorResult::SkipItem()
{
    return m_Res ? m_Res->SkipItem() : false;
}


CTDS_CursorResult::~CTDS_CursorResult()
{
    try {
        delete m_Res;
    }
    NCBI_CATCH_ALL( kEmptyStr )
}



/////////////////////////////////////////////////////////////////////////////
//
//  CTDS_ITDescriptor::
//

CTDS_ITDescriptor::CTDS_ITDescriptor(CDBL_Connection& conn,
                                     DBPROCESS* dblink,
                                     int col_num) :
    CDBL_Result(conn, dblink)
{

    m_ObjName = Check(dbcolsource(dblink, col_num));
    DBBINARY* p = Check(dbtxptr(dblink, col_num));
    if (p) {
        memcpy(m_TxtPtr, p, DBTXPLEN);
        m_TxtPtr_is_NULL = false;
    } else
        m_TxtPtr_is_NULL = true;

    p = Check(dbtxtimestamp(dblink, col_num));
    if (p) {
        memcpy(m_TimeStamp, p, DBTXTSLEN);
        m_TimeStamp_is_NULL = false;
    } else
        m_TimeStamp_is_NULL = true;
}


CTDS_ITDescriptor::CTDS_ITDescriptor(CDBL_Connection& conn,
                                     DBPROCESS* dblink,
                                     const CDB_ITDescriptor& inp_d) :
CDBL_Result(conn, dblink)
{
    m_ObjName = inp_d.TableName();
    m_ObjName += ".";
    m_ObjName += inp_d.ColumnName();


    DBBINARY* p = Check(dbtxptr(dblink, 1));
    if (p) {
        memcpy(m_TxtPtr, p, DBTXPLEN);
        m_TxtPtr_is_NULL = false;
    } else
        m_TxtPtr_is_NULL = true;

    p = Check(dbtxtimestamp(dblink, 1));
    if (p) {
        memcpy(m_TimeStamp, p, DBTXTSLEN);
        m_TimeStamp_is_NULL = false;
    } else
        m_TimeStamp_is_NULL = true;
}


int CTDS_ITDescriptor::DescriptorType() const
{
    return CDBL_ITDESCRIPTOR_TYPE_MAGNUM;
}


CTDS_ITDescriptor::~CTDS_ITDescriptor()
{
    return;
}


///////////////////////////////////////////////////////////////////////////////
EDB_Type CTDS_Result::GetDataType(int n)
{
    switch (Check(dbcoltype(GetCmd(), n))) {
    case SYBBINARY:    return (Check(dbcollen(GetCmd(), n)) > 255) ? eDB_LongBinary :
                                                         eDB_VarBinary;
    case SYBBITN:
    case SYBBIT:       return eDB_Bit;
    case SYBCHAR:      return (Check(dbcollen(GetCmd(), n)) > 255) ? eDB_LongChar :
                                                         eDB_VarChar;
    case SYBDATETIME:  return eDB_DateTime;
    case SYBDATETIME4: return eDB_SmallDateTime;
    case SYBINT1:      return eDB_TinyInt;
    case SYBINT2:      return eDB_SmallInt;
    case SYBINT4:      return eDB_Int;
    case SYBINT8:      return eDB_BigInt;
    case SYBDECIMAL:
    case SYBNUMERIC:   break;
    case SYBFLT8:      return eDB_Double;
    case SYBREAL:      return eDB_Float;
    case SYBTEXT:      return eDB_Text;
    case SYBIMAGE:     return eDB_Image;
    case SYBUNIQUE:    return eDB_VarBinary;
    default:           return eDB_UnsupportedType;
    }
    DBTYPEINFO* t = Check(dbcoltypeinfo(GetCmd(), n));
    return (t->scale == 0  &&  t->precision < 20) ? eDB_BigInt : eDB_Numeric;
}

CDB_Object* CTDS_Result::GetItemInternal(int item_no,
                            SDBL_ColDescr* fmt,
                            CDB_Object* item_buff)
{
    EDB_Type b_type = item_buff ? item_buff->GetType() : eDB_UnsupportedType;
    BYTE* d_ptr = Check(dbdata  (GetCmd(), item_no));
    DBINT d_len = Check(dbdatlen(GetCmd(), item_no));

    CDB_Object* val = s_GenericGetItem(fmt->data_type, item_buff,
                                       b_type, d_ptr, d_len);
    if (val)
        return val;

    switch (fmt->data_type) {
    case eDB_BigInt: {
        if(Check(dbcoltype(GetCmd(), item_no)) == SYBINT8) {
          Int8* v= (Int8*) d_ptr;
          if(item_buff) {
            if(v) {
              if(b_type == eDB_BigInt) {
                *((CDB_BigInt*) item_buff)= *v;
              }
              else {
                DATABASE_DRIVER_ERROR( "wrong type of CDB_Object", 230020 );
              }
            }
            else
                item_buff->AssignNULL();
            return item_buff;
          }

          return v ?
            new CDB_BigInt(*v) : new CDB_BigInt;
        }
        DBNUMERIC* v = (DBNUMERIC*) d_ptr;
        if (item_buff) {
            if (v) {
                if (b_type == eDB_Numeric) {
                    ((CDB_Numeric*) item_buff)->Assign
                        ((unsigned int)   v->precision,
                         (unsigned int)   v->scale,
                         (unsigned char*) v->array);
                } else if (b_type == eDB_BigInt) {
                    *((CDB_BigInt*) item_buff) = numeric_to_longlong
                        ((unsigned int) v->precision, v->array);
                } else {
                    DATABASE_DRIVER_ERROR( "wrong type of CDB_Object", 230020 );
                }
            } else
                item_buff->AssignNULL();
            return item_buff;
        }

        return v ?
            new CDB_BigInt(numeric_to_longlong((unsigned int) v->precision,
                                               v->array)) : new CDB_BigInt;
    }

    case eDB_Numeric: {
        DBNUMERIC* v = (DBNUMERIC*) d_ptr;
        if (item_buff) {
            if (v) {
                if (b_type == eDB_Numeric) {
                    ((CDB_Numeric*) item_buff)->Assign
                        ((unsigned int)   v->precision,
                         (unsigned int)   v->scale,
                         (unsigned char*) v->array);
                } else {
                    DATABASE_DRIVER_ERROR( "wrong type of CDB_Object", 230020 );
                }
            } else
                item_buff->AssignNULL();
            return item_buff;
        }

        return v ?
            new CDB_Numeric((unsigned int)   v->precision,
                            (unsigned int)   v->scale,
                            (unsigned char*) v->array) : new CDB_Numeric;
    }

    case eDB_Text: {
        if (item_buff  &&  b_type != eDB_Text  &&  b_type != eDB_Image) {
            DATABASE_DRIVER_ERROR( "wrong type of CDB_Object", 130020 );
        }
        CDB_Text* v = item_buff ? (CDB_Text*) item_buff : new CDB_Text;
        v->Append((char*) d_ptr, (int) d_len);
        return v;
    }

    case eDB_Image: {
        if (item_buff  &&  b_type != eDB_Text  &&  b_type != eDB_Image) {
            DATABASE_DRIVER_ERROR( "wrong type of CDB_Object", 130020 );
        }
        CDB_Image* v = item_buff ? (CDB_Image*) item_buff : new CDB_Image;
        v->Append((void*) d_ptr, (int) d_len);
        return v;
    }

    default:
        DATABASE_DRIVER_ERROR( "unexpected result type", 130004 );
    }
}

EDB_Type CTDS_Result::RetGetDataType(int n)
{
    switch (Check(dbrettype(GetCmd(), n))) {
    case SYBBINARY:    return (Check(dbretlen(GetCmd(), n) > 255))? eDB_LongBinary :
                                                        eDB_VarBinary;
    case SYBBIT:       return eDB_Bit;
    case SYBCHAR:      return (Check(dbretlen(GetCmd(), n)) > 255)? eDB_LongChar :
                                                        eDB_VarChar;
    case SYBDATETIME:  return eDB_DateTime;
    case SYBDATETIME4: return eDB_SmallDateTime;
    case SYBINT1:      return eDB_TinyInt;
    case SYBINT2:      return eDB_SmallInt;
    case SYBINT4:      return eDB_Int;
    case SYBFLT8:      return eDB_Double;
    case SYBREAL:      return eDB_Float;
    case SYBUNIQUE:    return eDB_VarBinary;
    default:           return eDB_UnsupportedType;
    }

    return eDB_UnsupportedType;
}

CDB_Object* CTDS_Result::RetGetItem(int item_no,
                       SDBL_ColDescr* fmt,
                       CDB_Object* item_buff)
{
    EDB_Type b_type = item_buff ? item_buff->GetType() : eDB_UnsupportedType;
    BYTE* d_ptr = Check(dbretdata(GetCmd(), item_no));
    DBINT d_len = Check(dbretlen (GetCmd(), item_no));
    CDB_Object* val = s_GenericGetItem(fmt->data_type, item_buff,
                                       b_type, d_ptr, d_len);
    if (!val) {
        DATABASE_DRIVER_ERROR( "unexpected result type", 230004 );
    }
    return val;
}

EDB_Type CTDS_Result::AltGetDataType(int id, int n)
{
    switch (Check(dbalttype(GetCmd(), id, n))) {
    case SYBBINARY:    return eDB_VarBinary;
    case SYBBIT:       return eDB_Bit;
    case SYBCHAR:      return eDB_VarChar;
    case SYBDATETIME:  return eDB_DateTime;
    case SYBDATETIME4: return eDB_SmallDateTime;
    case SYBINT1:      return eDB_TinyInt;
    case SYBINT2:      return eDB_SmallInt;
    case SYBINT4:      return eDB_Int;
    case SYBFLT8:      return eDB_Double;
    case SYBREAL:      return eDB_Float;
    case SYBUNIQUE:    return eDB_VarBinary;
    default:           return eDB_UnsupportedType;
    }

    return eDB_UnsupportedType;
}

CDB_Object* CTDS_Result::AltGetItem(int id,
                       int item_no,
                       SDBL_ColDescr* fmt,
                       CDB_Object* item_buff)
{
    EDB_Type b_type = item_buff ? item_buff->GetType() : eDB_UnsupportedType;
    BYTE* d_ptr = Check(dbadata(GetCmd(), id, item_no));
    DBINT d_len = Check(dbadlen(GetCmd(), id, item_no));
    CDB_Object* val = s_GenericGetItem(fmt->data_type, item_buff,
                                       b_type, d_ptr, d_len);
    if (!val) {
        DATABASE_DRIVER_ERROR( "unexpected result type", 130004 );
    }
    return val;
}

END_NCBI_SCOPE



/*
 * ===========================================================================
 * $Log$
 * Revision 1.29  2006/08/21 19:56:52  ssikorsk
 * Allow to convert from CDB_LongChar to CDB_VarChar when size of
 * data is less than 256 bytes.
 *
 * Revision 1.28  2006/08/21 18:04:08  ssikorsk
 * Minor code cleanup.
 *
 * Revision 1.27  2006/07/12 16:29:31  ssikorsk
 * Separated interface and implementation of CDB classes.
 *
 * Revision 1.26  2006/05/04 20:12:47  ssikorsk
 * Implemented classs CDBL_Cmd, CDBL_Result and CDBLExceptions;
 * Surrounded each native dblib call with Check;
 *
 * Revision 1.25  2006/02/16 19:37:42  ssikorsk
 * Get rid of compilation warnings
 *
 * Revision 1.24  2005/11/02 14:16:59  ssikorsk
 * Rethrow catched CDB_Exception to preserve useful information.
 *
 * Revision 1.23  2005/10/31 12:25:56  ssikorsk
 * Get rid of warnings.
 *
 * Revision 1.22  2005/09/19 14:19:05  ssikorsk
 * Use NCBI_CATCH_ALL macro instead of catch(...)
 *
 * Revision 1.21  2005/09/16 16:58:17  ssikorsk
 * Handle SYBUNIQUE data type as eDB_VarBinary
 *
 * Revision 1.20  2005/09/15 11:00:02  ssikorsk
 * Destructors do not throw exceptions any more.
 *
 * Revision 1.19  2005/09/14 14:16:25  ssikorsk
 * Small improvement
 *
 * Revision 1.18  2005/09/07 11:06:32  ssikorsk
 * Added a GetColumnNum implementation
 *
 * Revision 1.17  2005/07/20 12:33:05  ssikorsk
 * Merged ftds/interfaces.hpp into dblib/interfaces.hpp
 *
 * Revision 1.16  2005/04/04 13:03:57  ssikorsk
 * Revamp of DBAPI exception class CDB_Exception
 *
 * Revision 1.15  2004/12/10 15:26:11  ssikorsk
 * FreeTDS is ported on windows
 *
 * Revision 1.14  2004/05/17 21:13:37  gorelenk
 * Added include of PCH ncbi_pch.hpp
 *
 * Revision 1.13  2004/02/24 20:22:12  soussov
 * SYBBITN processing added
 *
 * Revision 1.12  2003/04/29 21:15:03  soussov
 * new datatypes CDB_LongChar and CDB_LongBinary added
 *
 * Revision 1.11  2003/02/06 16:26:52  soussov
 * adding support for bigint datatype
 *
 * Revision 1.10  2003/01/06 16:59:31  soussov
 * sets m_CurrItem = -1 for all result types if no fetch was called
 *
 * Revision 1.9  2003/01/03 21:48:18  soussov
 * set m_CurrItem = -1 if fetch failes
 *
 * Revision 1.8  2002/07/18 14:59:31  soussov
 * fixes bug in blob result
 *
 * Revision 1.7  2002/06/13 21:10:18  soussov
 * freeTDS does not place doubles properly in memory, patch added
 *
 * Revision 1.6  2002/06/08 06:18:43  vakatov
 * Ran through 64-bit compilation (tests were successful on Solaris/Forte6u2).
 * Fixed a return type in CTDS_CursorResult::ItemMaxSize, eliminated a couple
 * of warnings. Formally formatted the code to fit the C++ Toolkit style.
 *
 * Revision 1.5  2002/05/29 22:04:29  soussov
 * Makes BlobResult read ahead
 *
 * Revision 1.4  2002/03/26 15:35:10  soussov
 * new image/text operations added
 *
 * Revision 1.3  2002/02/06 22:30:56  soussov
 * fixes the arguments order in numeric assign
 *
 * Revision 1.2  2001/11/06 18:00:02  lavr
 * Formatted uniformly as the rest of the library
 *
 * Revision 1.1  2001/10/25 00:39:22  vakatov
 * Initial revision
 *
 * ===========================================================================
 */
