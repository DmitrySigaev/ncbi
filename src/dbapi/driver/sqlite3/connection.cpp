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
 * Author:  Sergey Sikorskiy
 *
 * File Description:
 *    Driver for SQLite v3 embedded database server
 *
 */

#include <ncbi_pch.hpp>

#include <dbapi/driver/sqlite3/interfaces.hpp>
#include <memory>

#include "sqlite3_utils.hpp"


BEGIN_NCBI_SCOPE

CSL3_Connection::CSL3_Connection(CSL3Context&  cntx,
                                 const string& srv_name,
                                 const string&,
                                 const string&) :
    impl::CConnection(cntx),
    m_SQLite3(NULL),
    m_IsOpen(false)
{
    SetServerName(srv_name);
    Check(sqlite3_open(srv_name.c_str(), &m_SQLite3));

    m_IsOpen = true;
}


CSL3_Connection::~CSL3_Connection()
{
    try {
        // Close is a virtual function but it is safe to call it from a destructor
        // because it is defined in this class.
        Close();
    }
    NCBI_CATCH_ALL( kEmptyStr )
}


bool CSL3_Connection::IsAlive()
{
    return m_IsOpen;
}


CDB_SendDataCmd* CSL3_Connection::SendDataCmd(I_ITDescriptor& /*descr_in*/,
                                                size_t          /*data_size*/,
                                                bool            /*log_it*/)
{
    return NULL;
}


bool CSL3_Connection::SendData(I_ITDescriptor& /*desc*/,
                                 CDB_Image&      /*img*/,
                                 bool            /*log_it*/)
{
    return false;
}


bool CSL3_Connection::SendData(I_ITDescriptor& /*desc*/,
                                 CDB_Text&       /*txt*/,
                                 bool            /*log_it*/)
{
    return false;
}


bool CSL3_Connection::Refresh()
{
    // close all commands first
    DeleteAllCommands();

    return true;
}


I_DriverContext::TConnectionMode CSL3_Connection::ConnectMode() const
{
    return 0;
}


CDB_LangCmd* CSL3_Connection::LangCmd(const string& lang_query,
                                        unsigned int  nof_parms)
{
    CSL3_LangCmd* lcmd = new CSL3_LangCmd(this, lang_query, nof_parms);
    return Create_LangCmd(*lcmd);
}


CDB_RPCCmd *CSL3_Connection::RPC(const string& /*rpc_name*/,
                                   unsigned int  /*nof_args*/)
{
    return NULL;
}


CDB_BCPInCmd* CSL3_Connection::BCPIn(const string& /*table_name*/,
                                       unsigned int  /*nof_columns*/)
{
    return NULL;
}


CDB_CursorCmd *CSL3_Connection::Cursor(const string& /*cursor_name*/,
                                         const string& /*query*/,
                                         unsigned int  /*nof_params*/,
                                         unsigned int  /*batch_size*/)
{
    return NULL;
}

bool CSL3_Connection::Abort()
{
    return false;
}

bool CSL3_Connection::Close(void)
{
    if (m_IsOpen) {
        Refresh();
        Check(sqlite3_close(m_SQLite3));
        m_IsOpen = false;
        return true;
    }

    return false;
}


int CSL3_Connection::Check(int rc)
{
    return CheckSQLite3(m_SQLite3, GetMsgHandlers(), rc);
}

END_NCBI_SCOPE



/*
 * ===========================================================================
 * $Log$
 * Revision 1.2  2006/07/12 16:29:31  ssikorsk
 * Separated interface and implementation of CDB classes.
 *
 * Revision 1.1  2006/06/12 20:30:51  ssikorsk
 * Initial version
 *
 * ===========================================================================
 */
