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
 * File Description:  DBLib bcp-in command
 *
 */

#include <dbapi/driver/dblib/interfaces.hpp>
#include <dbapi/driver/util/numeric_convert.hpp>
#include <string.h>


BEGIN_NCBI_SCOPE


/////////////////////////////////////////////////////////////////////////////
//
//  CDBL_BCPInCmd::
//

CDBL_BCPInCmd::CDBL_BCPInCmd(CDBL_Connection* con,
                             DBPROCESS*       cmd,
                             const string&    table_name,
                             unsigned int     nof_columns) :
    m_Connect(con), m_Cmd(cmd), m_Params(nof_columns),
    m_WasSent(false), m_HasFailed(false),
    m_HasTextImage(false), m_WasBound(false)
{
    if (bcp_init(cmd, (char*) table_name.c_str(), 0, 0, DB_IN) != SUCCEED) {
        throw CDB_ClientEx(eDB_Fatal, 223001,
                           "CDBL_BCPInCmd::CDBL_BCPInCmd", "bcp_init failed");
    }
}


bool CDBL_BCPInCmd::Bind(unsigned int column_num, CDB_Object* param_ptr)
{
    return m_Params.BindParam(column_num,  kEmptyStr, param_ptr);
}


bool CDBL_BCPInCmd::x_AssignParams(void* pb)
{
    RETCODE r;

    if (!m_WasBound) {
        for (unsigned int i = 0; i < m_Params.NofParams(); i++) {
            if (m_Params.GetParamStatus(i) == 0)
                continue;

            CDB_Object& param = *m_Params.GetParam(i);
            
            switch ( param.GetType() ) {
            case eDB_Int: {
                CDB_Int& val = dynamic_cast<CDB_Int&> (param);
                // DBINT v = (DBINT) val.Value();
                r = bcp_bind(m_Cmd, (BYTE*) val.BindVal(), 0,
                             val.IsNULL() ? 0 : -1, 0, 0, SYBINT4, i + 1);
            }
            break;
            case eDB_SmallInt: {
                CDB_SmallInt& val = dynamic_cast<CDB_SmallInt&> (param);
                // DBSMALLINT v = (DBSMALLINT) val.Value();
                r = bcp_bind(m_Cmd, (BYTE*) val.BindVal(), 0,
                             val.IsNULL() ? 0 : -1, 0, 0, SYBINT2, i + 1);
            }
            break;
            case eDB_TinyInt: {
                CDB_TinyInt& val = dynamic_cast<CDB_TinyInt&> (param);
                // DBTINYINT v = (DBTINYINT) val.Value();
                r = bcp_bind(m_Cmd, (BYTE*) val.BindVal(), 0,
                             val.IsNULL() ? 0 : -1, 0, 0, SYBINT1, i + 1);
            }
            break;
            case eDB_BigInt: {
                CDB_BigInt& val = dynamic_cast<CDB_BigInt&> (param);
                DBNUMERIC* v = reinterpret_cast<DBNUMERIC*> (pb);
                Int8 v8 = val.Value();
                if (longlong_to_numeric(v8, 18, v->array) == 0)
                    return false;
                r = bcp_bind(m_Cmd, (BYTE*) v, 0,
                             val.IsNULL() ? 0 : -1, 0, 0, SYBNUMERIC, i + 1);
                pb = (void*) (v + 1);
            }
            break;
            case eDB_Char: {
                CDB_Char& val = dynamic_cast<CDB_Char&> (param);
                r = bcp_bind(m_Cmd, (BYTE*) val.Value(), 0,
                             val.IsNULL() ? 0 : -1,
                             (BYTE*) "", 1, SYBCHAR, i + 1);
            }
            break;
            case eDB_VarChar: {
                CDB_VarChar& val = dynamic_cast<CDB_VarChar&> (param);
                r = bcp_bind(m_Cmd, (BYTE*) val.Value(), 0,
                             val.IsNULL() ? 0 : -1,
                             (BYTE*) "", 1, SYBCHAR, i + 1);
            }
            break;
            case eDB_Binary: {
                CDB_Binary& val = dynamic_cast<CDB_Binary&> (param);
                r = bcp_bind(m_Cmd, (BYTE*) val.Value(), 0,
                             val.IsNULL() ? 0 : (DBINT) val.Size(),
                             0, 0, SYBBINARY, i + 1);
            }
            break;
            case eDB_VarBinary: {
                CDB_VarBinary& val = dynamic_cast<CDB_VarBinary&> (param);
                r = bcp_bind(m_Cmd, (BYTE*) val.Value(), 0,
                             val.IsNULL() ? 0 : (DBINT) val.Size(),
                             0, 0, SYBBINARY, i + 1);
            }
            break;
            case eDB_Float: {
                CDB_Float& val = dynamic_cast<CDB_Float&> (param);
                // DBREAL v = (DBREAL) val.Value();
                r = bcp_bind(m_Cmd, (BYTE*) val.BindVal(), 0,
                             val.IsNULL() ? 0 : -1, 0, 0, SYBREAL, i + 1);
            }
            break;
            case eDB_Double: {
                CDB_Double& val = dynamic_cast<CDB_Double&> (param);
                // DBFLT8 v = (DBFLT8) val.Value();
                r = bcp_bind(m_Cmd, (BYTE*) val.BindVal(), 0,
                             val.IsNULL() ? 0 : -1, 0, 0, SYBFLT8, i + 1);
            }
            break;
            case eDB_SmallDateTime: {
                CDB_SmallDateTime& val =
                    dynamic_cast<CDB_SmallDateTime&> (param);
                DBDATETIME4* dt = (DBDATETIME4*) pb;
                dt->days        = val.GetDays();
                dt->minutes     = val.GetMinutes();
                r = bcp_bind(m_Cmd, (BYTE*) dt, 0, val.IsNULL() ? 0 : -1,
                             0, 0, SYBDATETIME4,  i + 1);
                pb = (void*) (dt + 1);
            }
            break;
            case eDB_DateTime: {
                CDB_DateTime& val = dynamic_cast<CDB_DateTime&> (param);
                DBDATETIME* dt = (DBDATETIME*) pb;
                dt->dtdays     = val.GetDays();
                dt->dttime     = val.Get300Secs();
                r = bcp_bind(m_Cmd, (BYTE*) dt, 0, val.IsNULL() ? 0 : -1,
                             0, 0, SYBDATETIME, i + 1);
                pb = (void*) (dt + 1);
            }
            break;
            case eDB_Text: {
                CDB_Text& val = dynamic_cast<CDB_Text&> (param);
                r = bcp_bind(m_Cmd, 0, 0,
                             val.IsNULL() ? 0 : (DBINT) val.Size(),
                             0, 0, SYBTEXT, i + 1);
                m_HasTextImage = true;
            }
            break;
            case eDB_Image: {
                CDB_Image& val = dynamic_cast<CDB_Image&> (param);
                r = bcp_bind(m_Cmd, 0, 0,
                             val.IsNULL() ? 0 : (DBINT) val.Size(),
                             0, 0, SYBIMAGE, i + 1);
                m_HasTextImage = true;
            }
            break;
            default:
                return false;
            }
            if (r != CS_SUCCEED)
                return false;
        }
        m_WasBound = true;
    } else {
        for (unsigned int i = 0; i < m_Params.NofParams(); i++) {
            if (m_Params.GetParamStatus(i) == 0)
                continue;

            CDB_Object& param = *m_Params.GetParam(i);

            switch ( param.GetType() ) {
            case eDB_Int: {
                CDB_Int& val = dynamic_cast<CDB_Int&> (param);
                // DBINT v = (DBINT) val.Value();
                r = bcp_colptr(m_Cmd, (BYTE*) val.BindVal(), i + 1)
                    == SUCCEED &&
                    bcp_collen(m_Cmd,  val.IsNULL() ? 0 : -1, i + 1)
                    == SUCCEED ? SUCCEED : FAIL;
            }
            break;
            case eDB_SmallInt: {
                CDB_SmallInt& val = dynamic_cast<CDB_SmallInt&> (param);
                // DBSMALLINT v = (DBSMALLINT) val.Value();
                r = bcp_colptr(m_Cmd, (BYTE*) val.BindVal(), i + 1)
                    == SUCCEED &&
                    bcp_collen(m_Cmd,  val.IsNULL() ? 0 : -1, i + 1)
                    == SUCCEED ? SUCCEED : FAIL;
            }
            break;
            case eDB_TinyInt: {
                CDB_TinyInt& val = dynamic_cast<CDB_TinyInt&> (param);
                // DBTINYINT v = (DBTINYINT) val.Value();
                r = bcp_colptr(m_Cmd, (BYTE*) val.BindVal(), i + 1)
                    == SUCCEED &&
                    bcp_collen(m_Cmd, val.IsNULL() ? 0 : -1, i + 1)
                    == SUCCEED ? SUCCEED : FAIL;
            }
            break;
            case eDB_BigInt: {
                CDB_BigInt& val = dynamic_cast<CDB_BigInt&> (param);
                DBNUMERIC* v = (DBNUMERIC*) pb;
                Int8 v8 = val.Value();
                if (longlong_to_numeric(v8, 18, v->array) == 0)
                    return false;
                r = bcp_colptr(m_Cmd, (BYTE*) v, i + 1)
                    == SUCCEED &&
                    bcp_collen(m_Cmd,  val.IsNULL() ? 0 : -1, i + 1)
                    == SUCCEED ? SUCCEED : FAIL;
                pb = (void*) (v + 1);
            }
            break;
            case eDB_Char: {
                CDB_Char& val = dynamic_cast<CDB_Char&> (param);
                r = bcp_colptr(m_Cmd, (BYTE*) val.Value(), i + 1)
                    == SUCCEED &&
                    bcp_collen(m_Cmd, val.IsNULL() ? 0 : -1, i + 1)
                    == SUCCEED ? SUCCEED : FAIL;
            }
            break;
            case eDB_VarChar: {
                CDB_VarChar& val = dynamic_cast<CDB_VarChar&> (param);
                r = bcp_colptr(m_Cmd, (BYTE*) val.Value(), i + 1)
                    == SUCCEED &&
                    bcp_collen(m_Cmd, val.IsNULL() ? 0 : -1, i + 1)
                    == SUCCEED ? SUCCEED : FAIL;
            }
            break;
            case eDB_Binary: {
                CDB_Binary& val = dynamic_cast<CDB_Binary&> (param);
                r = bcp_colptr(m_Cmd, (BYTE*) val.Value(), i + 1)
                    == SUCCEED &&
                    bcp_collen(m_Cmd,
                               val.IsNULL() ? 0 : (DBINT) val.Size(), i + 1)
                    == SUCCEED ? SUCCEED : FAIL;
            }
            break;
            case eDB_VarBinary: {
                CDB_VarBinary& val = dynamic_cast<CDB_VarBinary&> (param);
                r = bcp_colptr(m_Cmd, (BYTE*) val.Value(), i + 1)
                    == SUCCEED &&
                    bcp_collen(m_Cmd,
                               val.IsNULL() ? 0 : (DBINT) val.Size(), i + 1)
                    == SUCCEED ? SUCCEED : FAIL;
            }
            break;
            case eDB_Float: {
                CDB_Float& val = dynamic_cast<CDB_Float&> (param);
                //DBREAL v = (DBREAL) val.Value();
                r = bcp_colptr(m_Cmd, (BYTE*) val.BindVal(), i + 1)
                    == SUCCEED &&
                    bcp_collen(m_Cmd,  val.IsNULL() ? 0 : -1, i + 1)
                    == SUCCEED ? SUCCEED : FAIL;
            }
            break;
            case eDB_Double: {
                CDB_Double& val = dynamic_cast<CDB_Double&> (param);
                //DBFLT8 v = (DBFLT8) val.Value();
                r = bcp_bind(m_Cmd, (BYTE*) val.BindVal(), 0,
                             val.IsNULL() ? 0 : -1, 0, 0, SYBFLT8, i + 1);
            }
            break;
            case eDB_SmallDateTime: {
                CDB_SmallDateTime& val =
                    dynamic_cast<CDB_SmallDateTime&> (param);
                DBDATETIME4* dt = (DBDATETIME4*) pb;
                dt->days        = val.GetDays();
                dt->minutes     = val.GetMinutes();
                r = bcp_colptr(m_Cmd, (BYTE*) dt, i + 1)
                    == SUCCEED &&
                    bcp_collen(m_Cmd, val.IsNULL() ? 0 : -1, i + 1)
                    == SUCCEED ? SUCCEED : FAIL;
                pb = (void*) (dt + 1);
            }
            break;
            case eDB_DateTime: {
                CDB_DateTime& val = dynamic_cast<CDB_DateTime&> (param);
                DBDATETIME* dt = (DBDATETIME*) pb;
                dt->dtdays     = val.GetDays();
                dt->dttime     = val.Get300Secs();
                r = bcp_colptr(m_Cmd, (BYTE*) dt, i + 1)
                    == SUCCEED &&
                    bcp_collen(m_Cmd, val.IsNULL() ? 0 : -1, i + 1)
                    == SUCCEED ? SUCCEED : FAIL;
                pb = (void*) (dt + 1);
            }
            break;
            case eDB_Text: {
                CDB_Text& val = dynamic_cast<CDB_Text&> (param);
                r = bcp_collen(m_Cmd, (DBINT) val.Size(), i + 1);
            }
            break;
            case eDB_Image: {
                CDB_Image& val = dynamic_cast<CDB_Image&> (param);
                r = bcp_collen(m_Cmd, (DBINT) val.Size(), i + 1);
            }
            break;
            default:
                return false;
            }
            if (r != CS_SUCCEED)
                return false;
        }
    }
    return true;
}


bool CDBL_BCPInCmd::SendRow()
{
    char param_buff[2048]; // maximal row size, assured of buffer overruns

    if (!x_AssignParams(param_buff)) {
        m_HasFailed = true;
        throw CDB_ClientEx(eDB_Error, 223004,
                           "CDBL_BCPInCmd::SendRow", "cannot assign params");
    }

    if (bcp_sendrow(m_Cmd) != SUCCEED) {
        m_HasFailed = true;
        throw CDB_ClientEx(eDB_Error, 223005,
                           "CDBL_BCPInCmd::SendRow", "bcp_sendrow failed");
    }
    m_WasSent = true;

    if (m_HasTextImage) { // send text/image data
        char buff[1800]; // text/image page size

        for (unsigned int i = 0; i < m_Params.NofParams(); i++) {
            if (m_Params.GetParamStatus(i) == 0)
                continue;

            CDB_Object& param = *m_Params.GetParam(i);

            if (param.GetType() != eDB_Text &&
                param.GetType() != eDB_Image)
                continue;

            CDB_Stream& val = dynamic_cast<CDB_Stream&> (param);
            size_t s = val.Size();
            do {
                size_t l = val.Read(buff, sizeof(buff));
                if (l > s)
                    l = s;
                if (bcp_moretext(m_Cmd, (DBINT) l, (BYTE*) buff) != SUCCEED) {
                    m_HasFailed = true;
                    throw CDB_ClientEx(eDB_Error, 223006,
                                       "CDBL_BCPInCmd::SendRow",
                                       param.GetType() == eDB_Text ?
                                       "bcp_moretext for text failed" :
                                       "bcp_moretext for image failed");
                }
                if (!l)
                    break;
                s -= l;
            } while (s);
        }
    }

    return true;
}


bool CDBL_BCPInCmd::Cancel()
{
    DBINT outrow = bcp_done(m_Cmd);
    return outrow == 0;
}


bool CDBL_BCPInCmd::CompleteBatch()
{
    CS_INT outrow = bcp_batch(m_Cmd);
    return outrow != -1;
}


bool CDBL_BCPInCmd::CompleteBCP()
{
    DBINT outrow = bcp_done(m_Cmd);
    return outrow != -1;
}


void CDBL_BCPInCmd::Release()
{
    m_BR = 0;
    if (m_WasSent) {
        Cancel();
        m_WasSent = false;
    }
    m_Connect->DropCmd(*this);
    delete this;
}


CDBL_BCPInCmd::~CDBL_BCPInCmd()
{
    if (m_BR)
        *m_BR = 0;
    if (m_WasSent)
        Cancel();
}


END_NCBI_SCOPE



/*
 * ===========================================================================
 * $Log$
 * Revision 1.3  2001/10/24 16:38:53  lavr
 * Explicit casts (where necessary) to eliminate 64->32 bit compiler warnings
 *
 * Revision 1.2  2001/10/22 18:38:49  soussov
 * sending NULL instead of emty string fixed
 *
 * Revision 1.1  2001/10/22 15:19:55  lavr
 * This is a major revamp (by Anton Lavrentiev, with help from Vladimir
 * Soussov and Denis Vakatov) of the DBAPI "driver" libs originally
 * written by Vladimir Soussov. The revamp follows the one of CTLib
 * driver, and it involved massive code shuffling and grooming, numerous
 * local API redesigns, adding comments and incorporating DBAPI to
 * the C++ Toolkit.
 *
 * ===========================================================================
 */
