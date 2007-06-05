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
* File Name:  $Id$
*
* Author:  Michael Kholodov
*
* File Description:  Statement implementation
*
*
*
*
*/

#include <ncbi_pch.hpp>
#include <corelib/rwstream.hpp>
#include "conn_impl.hpp"
#include "stmt_impl.hpp"
#include "rs_impl.hpp"
#include "rw_impl.hpp"
#include <dbapi/driver/public.hpp>


BEGIN_NCBI_SCOPE

// implementation
CStatement::CStatement(CConnection* conn)
    : m_conn(conn)
    , m_cmd(0)
    , m_rowCount(-1)
    , m_failed(false)
    , m_irs(0)
    , m_wr(0)
    , m_ostr(0)
    , m_AutoClearInParams(false)
{
    SetIdent("CStatement");
}

CStatement::~CStatement()
{
    try {
        Notify(CDbapiClosedEvent(this));
        FreeResources();
        Notify(CDbapiDeletedEvent(this));
        _TRACE(GetIdent() << " " << (void*)this << " deleted.");
    }
    NCBI_CATCH_ALL( kEmptyStr )
}

IConnection* CStatement::GetParentConn()
{
    return m_conn;
}

void CStatement::CacheResultSet(CDB_Result *rs)
{
    if( m_irs != 0 ) {
        _TRACE("CStatement::CacheResultSet(): Invalidating cached CResultSet " << (void*)m_irs);
        m_irs->Invalidate();
    }
    if( rs != 0 ) {
        m_irs = new CResultSet(m_conn, rs);
        m_irs->AddListener(this);
        AddListener(m_irs);
        _TRACE("CStatement::CacheResultSet(): Created new CResultSet " << (void*)m_irs
            << " with CDB_Result " << (void*)rs);
    }
    else
        m_irs = 0;
}

IResultSet* CStatement::GetResultSet()
{
   return m_irs;
}

bool CStatement::HasMoreResults()
{
    // This method may be called even before *execute*.
    // We have to be prepared for everything.
    bool more = (GetBaseCmd() != NULL);

    if (more) {
        more = GetBaseCmd()->HasMoreResults();
        if( more ) {
            if( GetBaseCmd()->HasFailed() ) {
                SetFailed(true);
                return false;
            }
            //Notify(CDbapiNewResultEvent(this));
            CDB_Result *rs = GetBaseCmd()->Result();
            CacheResultSet(rs);
#if 0
            if( rs == 0 ) {
                m_rowCount = GetBaseCmd()->RowCount();
            }
#endif
        }
    }

    return more;
}

void CStatement::SetParam(const CVariant& v,
                          const string& name)
{

    ParamList::iterator i = m_params.find(name);
    if( i != m_params.end() ) {
        *((*i).second) = v;
    }
    else {
        m_params.insert(make_pair(name, new CVariant(v)));
    }
}


void CStatement::ClearParamList()
{
    ParamList::iterator i = m_params.begin();
    for( ; i != m_params.end(); ++i ) {
        delete (*i).second;
    }

    m_params.clear();
}

void CStatement::Execute(const string& sql)
{
    x_Send(sql);
}

void CStatement::SendSql(const string& sql)
{
    x_Send(sql);
}

void CStatement::x_Send(const string& sql)
{
    if( m_cmd != 0 ) {
        delete m_cmd;
        m_cmd = 0;
        m_rowCount = -1;
    }

    SetFailed(false);

    _TRACE("Sending SQL: " + sql);
    m_cmd = m_conn->GetCDB_Connection()->LangCmd(sql, m_params.size());

    ExecuteLast();

    if ( IsAutoClearInParams() ) {
        // Implicitely clear all parameters.
        ClearParamList();
    }
}

IResultSet* CStatement::ExecuteQuery(const string& sql)
{
    SendSql(sql);
    while( HasMoreResults() ) {
        if( HasRows() ) {
            return GetResultSet();
        }
    }
    return 0;
}
void CStatement::ExecuteUpdate(const string& sql)
{
    SendSql(sql);
    //while( HasMoreResults() );
    GetBaseCmd()->DumpResults();
}

void CStatement::ExecuteLast()
{
    ParamList::iterator i = m_params.begin();
    for( ; i != m_params.end(); ++i ) {
        GetLangCmd()->SetParam((*i).first, (*i).second->GetData());
    }
    m_cmd->Send();
}

bool CStatement::HasRows()
{
    return m_irs != 0;
}

IWriter* CStatement::GetBlobWriter(I_ITDescriptor &d, size_t blob_size, EAllowLog log_it)
{
    delete m_wr;
    m_wr = new CxBlobWriter(GetConnection()->GetCDB_Connection(),
        d, blob_size, log_it == eEnableLog, false);
    return m_wr;
}

CNcbiOstream& CStatement::GetBlobOStream(I_ITDescriptor &d, size_t blob_size,
                                         EAllowLog log_it, size_t buf_size)
{
    delete m_ostr;
    m_ostr = new CWStream(new CxBlobWriter(GetConnection()->GetCDB_Connection(),
        d, blob_size, log_it == eEnableLog, false), buf_size, 0, CRWStreambuf::fOwnWriter);
    return *m_ostr;
}

CDB_Result* CStatement::GetCDB_Result() {
    return m_irs == 0 ? 0 : m_irs->GetCDB_Result();
}

bool CStatement::Failed()
{
    return m_failed;
}

int CStatement::GetRowCount()
{
    int v;
    if( (v = GetBaseCmd()->RowCount()) >= 0 ) {
        m_rowCount = v;
    }
    return m_rowCount;
}

void CStatement::Close()
{
    Notify(CDbapiClosedEvent(this));
    FreeResources();
}

void CStatement::FreeResources()
{
    delete m_cmd;
    m_cmd = 0;
    m_rowCount = -1;
    if( m_conn != 0 && m_conn->IsAux() ) {
        delete m_conn;
        m_conn = 0;
        Notify(CDbapiAuxDeletedEvent(this));
    }

    delete m_wr;
    m_wr = 0;
    delete m_ostr;
    m_ostr = 0;

    ClearParamList();
}

void CStatement::PurgeResults()
{
    if( GetBaseCmd() != 0 )
        GetBaseCmd()->DumpResults();
}

void CStatement::Cancel()
{
    if( GetBaseCmd() != 0 )
        GetBaseCmd()->Cancel();
    m_rowCount = -1;
}

CDB_LangCmd* CStatement::GetLangCmd()
{
    //if( m_cmd == 0 )
    //throw CDbException("CStatementImpl::GetLangCmd(): no cmd structure");
    return (CDB_LangCmd*)m_cmd;
}

void CStatement::Action(const CDbapiEvent& e)
{
    _TRACE(GetIdent() << " " << (void*)this << ": '" << e.GetName()
           << "' received from " << e.GetSource()->GetIdent());

    CResultSet *rs;

    if(dynamic_cast<const CDbapiFetchCompletedEvent*>(&e) != 0 ) {
        if( m_irs != 0 && (rs = dynamic_cast<CResultSet*>(e.GetSource())) != 0 ) {
            if( rs == m_irs ) {
                m_rowCount = rs->GetTotalRows();
                _TRACE("Rowcount from the last resultset: " << m_rowCount);
            }
        }
    }

    if(dynamic_cast<const CDbapiDeletedEvent*>(&e) != 0 ) {
        RemoveListener(e.GetSource());
        if(dynamic_cast<CConnection*>(e.GetSource()) != 0 ) {
            _TRACE("Deleting " << GetIdent() << " " << (void*)this);
            delete this;
        }
        else if( m_irs != 0 && (rs = dynamic_cast<CResultSet*>(e.GetSource())) != 0 ) {
            if( rs == m_irs ) {
                _TRACE("Clearing cached CResultSet " << (void*)m_irs);
                m_irs = 0;
            }
        }
    }
}

END_NCBI_SCOPE
