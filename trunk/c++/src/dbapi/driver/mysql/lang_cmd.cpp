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
 * File Description
 *    Driver for MySQL server
 *
 */

#include <ncbi_pch.hpp>
#include <dbapi/driver/mysql/interfaces.hpp>


BEGIN_NCBI_SCOPE


CMySQL_LangCmd::CMySQL_LangCmd(CMySQL_Connection* conn,
                               const string&      lang_query,
                               unsigned int       nof_params) :
    impl::CBaseCmd(lang_query, nof_params),
    m_Connect(conn),
    m_HasResults(false)
{
}


bool CMySQL_LangCmd::Send()
{
    if (mysql_real_query
        (&m_Connect->m_MySQL, GetQuery().c_str(), GetQuery().length()) != 0) {
        DATABASE_DRIVER_WARNING( "Failed: mysql_real_query", 800003 );
    }

    my_ulonglong nof_Rows = mysql_affected_rows(&this->m_Connect->m_MySQL);
    // There is not too much sence in comparing unsigned value with -1.
    // m_HasResults = nof_Rows == -1 || nof_Rows > 0;
    m_HasResults = nof_Rows > 0;
    return true;
}


bool CMySQL_LangCmd::WasSent() const
{
    return false;
}


bool CMySQL_LangCmd::Cancel()
{
    return false;
}


bool CMySQL_LangCmd::WasCanceled() const
{
    return false;
}


CDB_Result *CMySQL_LangCmd::Result()
{
    m_HasResults = false;
    return Create_Result(*new CMySQL_RowResult(m_Connect));
}


bool CMySQL_LangCmd::HasMoreResults() const
{
    return m_HasResults;
}

void CMySQL_LangCmd::DumpResults()
{
    CDB_Result* dbres;
    while(m_HasResults) {
        dbres= Result();
        if(dbres) {
            if(m_Connect->GetResultProcessor()) {
                m_Connect->GetResultProcessor()->ProcessResult(*dbres);
            }
            else {
                while(dbres->Fetch());
            }
            delete dbres;
        }
    }
}


bool CMySQL_LangCmd::HasFailed() const
{
    if(mysql_errno(&m_Connect->m_MySQL) == 0)
      return false;
    return true;
}

CMySQL_LangCmd::~CMySQL_LangCmd()
{
    try {
        DetachInterface();

        GetConnection().DropCmd(*this);

        Cancel();
    }
    NCBI_CATCH_ALL( NCBI_CURRENT_FUNCTION )
}

int CMySQL_LangCmd::LastInsertId() const
{
  return static_cast<int>( mysql_insert_id(&this->m_Connect->m_MySQL) );
}

int CMySQL_LangCmd::RowCount() const
{
  return static_cast<int>( mysql_affected_rows(&this->m_Connect->m_MySQL) );
}

string CMySQL_LangCmd::EscapeString(const char* str, unsigned long len)
{
    std::auto_ptr<char> buff( new char[len*2 + 1]);
    mysql_real_escape_string(&this->m_Connect->m_MySQL, buff.get(), str, len);
    return buff.get();
}


END_NCBI_SCOPE


