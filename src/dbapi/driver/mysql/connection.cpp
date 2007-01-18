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

#include <ncbi_pch.hpp>
#include <dbapi/driver/mysql/interfaces.hpp>
#include <memory>


BEGIN_NCBI_SCOPE


CMySQL_Connection::CMySQL_Connection(CMySQLContext& cntx,
                                     const string&  srv_name,
                                     const string&  user_name,
                                     const string&  passwd) :
    impl::CConnection(cntx),
    m_IsOpen(false)
{
    if ( !mysql_init(&m_MySQL) ) {
        DATABASE_DRIVER_WARNING( "Failed: mysql_init", 800001 );
    }

    if ( !mysql_real_connect(&m_MySQL,
                             srv_name.c_str(),
                             user_name.c_str(), passwd.c_str(),
                             NULL, 0, NULL, 0)) {
        DATABASE_DRIVER_WARNING( "Failed: mysql_real_connect", 800002 );
    }

    m_IsOpen = true;
}


CMySQL_Connection::~CMySQL_Connection()
{
    try {
        // Close is a virtual function but it is safe to call it from a destructor
        // because it is defined in this class.
        Close();
    }
    NCBI_CATCH_ALL( NCBI_CURRENT_FUNCTION )
}


bool CMySQL_Connection::IsAlive()
{
    return false;
}


CDB_SendDataCmd* CMySQL_Connection::SendDataCmd(I_ITDescriptor& /*descr_in*/,
                                                size_t          /*data_size*/,
                                                bool            /*log_it*/)
{
    return 0;
}


bool CMySQL_Connection::SendData(I_ITDescriptor& /*desc*/,
                                 CDB_Image&      /*img*/,
                                 bool            /*log_it*/)
{
    return true;
}


bool CMySQL_Connection::SendData(I_ITDescriptor& /*desc*/,
                                 CDB_Text&       /*txt*/,
                                 bool            /*log_it*/)
{
    return true;
}


bool CMySQL_Connection::Refresh()
{
    // close all commands first
    DeleteAllCommands();

    return true;
}


I_DriverContext::TConnectionMode CMySQL_Connection::ConnectMode() const
{
    return 0;
}


CDB_LangCmd* CMySQL_Connection::LangCmd(const string& lang_query,
                                        unsigned int  nof_parms)
{
    return Create_LangCmd(*new CMySQL_LangCmd(this, lang_query, nof_parms));

}


CDB_RPCCmd *CMySQL_Connection::RPC(const string& /*rpc_name*/,
                                   unsigned int  /*nof_args*/)
{
    return 0;
}


CDB_BCPInCmd* CMySQL_Connection::BCPIn(const string& /*table_name*/,
                                       unsigned int  /*nof_columns*/)
{
    return 0;
}


CDB_CursorCmd *CMySQL_Connection::Cursor(const string& /*cursor_name*/,
                                         const string& /*query*/,
                                         unsigned int  /*nof_params*/,
                                         unsigned int  /*batch_size*/)
{
    return 0;
}

bool CMySQL_Connection::Abort()
{
    return false;
}

bool CMySQL_Connection::Close(void)
{
    if (m_IsOpen) {
        Refresh();

        mysql_close(&m_MySQL);

        m_IsOpen = false;

        return true;
    }

    return false;
}

END_NCBI_SCOPE


