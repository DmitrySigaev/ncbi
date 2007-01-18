/*  $Id$
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
*/

#include <ncbi_pch.hpp>

#include <dbapi/driver/impl/dbapi_impl_connection.hpp>
#include <dbapi/driver/impl/dbapi_impl_context.hpp>
#include <dbapi/driver/impl/dbapi_impl_cmd.hpp>

#include <algorithm>


BEGIN_NCBI_SCOPE

BEGIN_SCOPE(impl)

///////////////////////////////////////////////////////////////////////////
//  CConnection::
//

CDB_LangCmd* CConnection::Create_LangCmd(CBaseCmd& lang_cmd)
{
    m_CMDs.push_back(&lang_cmd);

    return new CDB_LangCmd(&lang_cmd);
}

CDB_RPCCmd* CConnection::Create_RPCCmd(CBaseCmd& rpc_cmd)
{
    m_CMDs.push_back(&rpc_cmd);

    return new CDB_RPCCmd(&rpc_cmd);
}

CDB_BCPInCmd* CConnection::Create_BCPInCmd(CBaseCmd& bcpin_cmd)
{
    m_CMDs.push_back(&bcpin_cmd);

    return new CDB_BCPInCmd(&bcpin_cmd);
}

CDB_CursorCmd* CConnection::Create_CursorCmd(CCursorCmd& cursor_cmd)
{
    m_CMDs.push_back(&cursor_cmd);

    return new CDB_CursorCmd(&cursor_cmd);
}

CDB_SendDataCmd* CConnection::Create_SendDataCmd(CSendDataCmd& senddata_cmd)
{
    m_CMDs.push_back(&senddata_cmd);

    return new CDB_SendDataCmd(&senddata_cmd);
}


CConnection::CConnection(CDriverContext& dc,
                         bool isBCPable,
                         bool reusable,
                         const string& pool_name,
                         bool hasSecureLogin
                         ) :
    m_DriverContext(&dc),
    m_MsgHandlers(dc.GetConnHandlerStack()),
    m_Interface(NULL),
    m_ResProc(NULL),
    m_Pool(pool_name),
    m_Reusable(reusable),
    m_BCPable(isBCPable),
    m_SecureLogin(hasSecureLogin)
{
}

CConnection::~CConnection(void)
{
    DetachResultProcessor();
//         DetachInterface();
}


void CConnection::PushMsgHandler(CDB_UserHandler* h,
                                    EOwnership ownership)
{
    m_MsgHandlers.Push(h, ownership);
}


void CConnection::PopMsgHandler(CDB_UserHandler* h)
{
    m_MsgHandlers.Pop(h);
}

void CConnection::DropCmd(impl::CCommand& cmd)
{
    TCommandList::iterator it = find(m_CMDs.begin(), m_CMDs.end(), &cmd);

    if (it != m_CMDs.end()) {
        m_CMDs.erase(it);
    }
}

void CConnection::DeleteAllCommands(void)
{
    while (!m_CMDs.empty()) {
        // Destructor will remove an entity from a container ...
        delete m_CMDs.back();
    }
}

void CConnection::Release(void)
{
    // close all commands first
    DeleteAllCommands();
    GetCDriverContext().DestroyConnImpl(this);
}

I_DriverContext* CConnection::Context(void) const
{
    _ASSERT(m_DriverContext);
    return m_DriverContext;
}

void CConnection::DetachResultProcessor(void)
{
    if (m_ResProc) {
        m_ResProc->ReleaseConn();
        m_ResProc = NULL;
    }
}

CDB_ResultProcessor* CConnection::SetResultProcessor(CDB_ResultProcessor* rp)
{
    CDB_ResultProcessor* r = m_ResProc;
    m_ResProc = rp;
    return r;
}

CDB_Result* CConnection::Create_Result(impl::CResult& result)
{
    return new CDB_Result(&result);
}

const string& CConnection::ServerName(void) const
{
    return m_Server;
}


const string& CConnection::UserName(void) const
{
    return m_User;
}


const string& CConnection::Password(void) const
{
    return m_Passwd;
}

const string& CConnection::PoolName(void) const
{
    return m_Pool;
}

bool CConnection::IsReusable(void) const
{
    return m_Reusable;
}

void CConnection::AttachTo(CDB_Connection* interface)
{
    m_Interface = interface;
}

void CConnection::ReleaseInterface(void)
{
    m_Interface = NULL;
}


void CConnection::SetTimeout(size_t nof_secs)
{
}


void CConnection::SetTextImageSize(size_t nof_bytes)
{
}


bool
CConnection::IsMultibyteClientEncoding(void) const
{
    return GetCDriverContext().IsMultibyteClientEncoding();
}


EEncoding
CConnection::GetClientEncoding(void) const
{
    return GetCDriverContext().GetClientEncoding();
}


void
CConnection::SetExtraMsg(const string& msg)
{
    GetMsgHandlers().SetExtraMsg(msg);
}


END_SCOPE(impl)

END_NCBI_SCOPE


