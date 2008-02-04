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
 * File Description:  DBLib connection
 *
 */
#include <ncbi_pch.hpp>

#include "dblib_utils.hpp"

#include <dbapi/driver/dblib/interfaces.hpp>
#include <dbapi/driver/dblib/interfaces_p.hpp>
#include <dbapi/error_codes.hpp>

#include <string.h>

#include <algorithm>


#if defined(NCBI_OS_MSWIN)
#include <io.h>
inline int close(int fd)
{
    return _close(fd);
}
#endif


#define NCBI_USE_ERRCODE_X   Dbapi_Dblib_Conn

BEGIN_NCBI_SCOPE


CDBL_Connection::CDBL_Connection(CDBLibContext& cntx,
                                 const CDBConnParams& params) :
    impl::CConnection(cntx, true, params.IsPooled(), params.GetPoolName()),
    m_DBLibCtx(&cntx),
    m_Login(NULL),
    m_Link(NULL)
{    
    m_Login = GetDBLibCtx().Check(dblogin());
    _ASSERT(m_Login);

    const string err_str(
        "Cannot connect to the server '" + params.GetServerName() +
        "' as user '" + params.GetUserName() + "'"
        );

    if (!GetCDriverContext().GetHostName().empty())
        DBSETLHOST(m_Login, (char*) GetCDriverContext().GetHostName().c_str());
    if (GetDBLibCtx().GetPacketSize() > 0)
        DBSETLPACKET(m_Login, GetDBLibCtx().GetPacketSize());
    if (DBSETLAPP (m_Login, (char*) GetCDriverContext().GetApplicationName().c_str())
        != SUCCEED ||
        DBSETLUSER(m_Login, (char*) params.GetUserName().c_str())
        != SUCCEED ||
        DBSETLPWD (m_Login, (char*) params.GetPassword().c_str())
        != SUCCEED) 
    {
        DATABASE_DRIVER_ERROR( err_str, 200011 );
    }

#ifndef MS_DBLIB_IN_USE
    DBSETLCHARSET( 
            m_Login, 
            const_cast<char*>(
                GetCDriverContext().GetClientCharset().c_str()
                ) 
            );
#endif
    BCP_SETL(m_Login, TRUE);

#ifndef MS_DBLIB_IN_USE
    if (params.IsPasswordEncrypted())
        DBSETLENCRYPT(m_Login, TRUE);
#endif

    string server_name;

#if defined(FTDS_IN_USE)
    if (params.GetHost()) {
        server_name = impl::ConvertN2A(params.GetHost());
        if (params.GetPort()) {
            server_name += ":" + NStr::IntToString(params.GetPort());
        }
    } else {
        server_name = params.GetServerName();
    }
#else
    server_name = params.GetServerName();
#endif

    m_Link = GetDBLibCtx().Check(dbopen(m_Login, (char*) server_name.c_str()));


    /*
    DBPROCESS* dbprocess = NULL; 

    if (params.GetHost()) {
        server_name = impl::ConvertN2A(params.GetHost());
        string port_str = NStr::IntToString(params.GetPort());

        RETCODE rc = dbsetconnect(
                "query", 
                "tcp", 
                "ether", 
                (char*)server_name.c_str(), 
                (char*)port_str.c_str());

        CHECK_DRIVER_ERROR(rc != SUCCEED, "dbsetconnect failed.", 200001);

        dbprocess = GetDBLibCtx().Check(dbopen(m_Login, NULL));
    } else {
        server_name = params.GetServerName();
        dbprocess = GetDBLibCtx().Check(dbopen(m_Login, (char*) server_name.c_str()));
    }
    */


    // It doesn't work currently ...
//     if (dbprocess) {
//         CHECK_DRIVER_ERROR(
//             GetDBLibCtx().Check(dbsetopt(
//                 dbprocess,
//                 DBBUFFER,
//                 const_cast<char*>(NStr::UIntToString(GetBufferSize()).c_str()),
//                 -1)) != SUCCEED,
//             "dbsetopt failed",
//             200001
//             );
//     }

    if (!m_Link) {
        DATABASE_DRIVER_ERROR( err_str, 200011 );
    }


#ifdef MS_DBLIB_IN_USE
    GetDBLibCtx().Check(dbsetopt(GetDBLibConnection(), DBTEXTLIMIT, "0" )); // No limit
    GetDBLibCtx().Check(dbsetopt(GetDBLibConnection(), DBTEXTSIZE , "2147483647" )); // 0x7FFFFFFF
#endif

    SetServerName(params.GetServerName());
    SetUserName(params.GetUserName());
    SetPassword(params.GetPassword());
    SetSecureLogin(params.IsPasswordEncrypted());

    dbsetuserdata(GetDBLibConnection(), (BYTE*) this);
    CheckFunctCall();

    SetServerType(CalculateServerType(params));
}


bool CDBL_Connection::IsAlive()
{
    return DBDEAD(GetDBLibConnection()) == FALSE;
}


void 
CDBL_Connection::SetTimeout(size_t nof_secs)
{
    // DATABASE_DRIVER_ERROR( "SetTimeout is not supported.", 100011 );
    ERR_POST_X_ONCE(3, "SetTimeout is not supported.");
}


CDB_LangCmd* CDBL_Connection::LangCmd(const string& lang_query)
{
    string extra_msg = "SQL Command: \"" + lang_query + "\"";
    SetExtraMsg(extra_msg);

    CDBL_LangCmd* lcmd = new CDBL_LangCmd(*this, GetDBLibConnection(), lang_query);
    return Create_LangCmd(*lcmd);
}


CDB_RPCCmd* CDBL_Connection::RPC(const string& rpc_name)
{
    string extra_msg = "RPC Command: " + rpc_name;
    SetExtraMsg(extra_msg);

    CDBL_RPCCmd* rcmd = new CDBL_RPCCmd(*this, GetDBLibConnection(), rpc_name);
    return Create_RPCCmd(*rcmd);
}


CDB_BCPInCmd* CDBL_Connection::BCPIn(const string& table_name)
{
    CHECK_DRIVER_ERROR( !IsBCPable(), "No bcp on this connection." + GetDbgInfo(), 210003 );

    string extra_msg = "BCP Table: " + table_name;
    SetExtraMsg(extra_msg);

    CDBL_BCPInCmd* bcmd = new CDBL_BCPInCmd(*this, GetDBLibConnection(), table_name);
    return Create_BCPInCmd(*bcmd);
}


CDB_CursorCmd* CDBL_Connection::Cursor(const string& cursor_name,
                                       const string& query,
                                       unsigned int)
{
    string extra_msg = "Cursor Name: \"" + cursor_name + "\"; SQL Command: \""+
        query + "\"";
    SetExtraMsg(extra_msg);

    CDBL_CursorCmd* ccmd = new CDBL_CursorCmd(*this, GetDBLibConnection(), cursor_name,
                                              query);
    return Create_CursorCmd(*ccmd);
}


CDB_SendDataCmd* CDBL_Connection::SendDataCmd(I_ITDescriptor& descr_in,
                                              size_t data_size, bool log_it)
{
    CHECK_DRIVER_ERROR( data_size < 1, "Wrong (zero) data size." + GetDbgInfo(), 210092 );

    I_ITDescriptor* p_desc= 0;

    // check what type of descriptor we've got
    if(descr_in.DescriptorType() !=
#ifndef MS_DBLIB_IN_USE
        CDBL_ITDESCRIPTOR_TYPE_MAGNUM
#else
        CMSDBL_ITDESCRIPTOR_TYPE_MAGNUM
#endif
        ) {
    // this is not a native descriptor
    p_desc= x_GetNativeITDescriptor(dynamic_cast<CDB_ITDescriptor&> (descr_in));
    if(p_desc == 0) return false;
    }


    auto_ptr<I_ITDescriptor> d_guard(p_desc);

    CDBL_ITDescriptor& desc = p_desc? dynamic_cast<CDBL_ITDescriptor&> (*p_desc) :
    dynamic_cast<CDBL_ITDescriptor&> (descr_in);

    if (Check(dbwritetext(GetDBLibConnection(),
                    (char*) desc.m_ObjName.c_str(),
                    desc.m_TxtPtr_is_NULL ? 0 : desc.m_TxtPtr,
                    DBTXPLEN,
                    desc.m_TimeStamp_is_NULL ? 0 : desc.m_TimeStamp,
                    log_it ? TRUE : FALSE,
                    (DBINT) data_size, 0)) != SUCCEED ||
        Check(dbsqlok(GetDBLibConnection())) != SUCCEED ||
        //        dbresults(GetDBLibConnection()) == FAIL) {
        x_Results(GetDBLibConnection()) == FAIL) {
        DATABASE_DRIVER_ERROR( "dbwritetext/dbsqlok/dbresults failed." + GetDbgInfo(), 210093 );
    }

    CDBL_SendDataCmd* sd_cmd = new CDBL_SendDataCmd(*this, GetDBLibConnection(), data_size);
    return Create_SendDataCmd(*sd_cmd);
}


bool CDBL_Connection::SendData(I_ITDescriptor& desc,
                               CDB_Image& img, bool log_it){
    return x_SendData(desc, dynamic_cast<CDB_Stream&> (img), log_it);
}


bool CDBL_Connection::SendData(I_ITDescriptor& desc,
                               CDB_Text&  txt, bool log_it) {
    return x_SendData(desc, dynamic_cast<CDB_Stream&> (txt), log_it);
}


bool CDBL_Connection::Refresh()
{
    // close all commands first
    DeleteAllCommands();

    // cancel all pending commands
    if (Check(dbcancel(GetDBLibConnection())) != CS_SUCCEED)
        return false;

    // check the connection status
    return DBDEAD(GetDBLibConnection()) == FALSE;
}


I_DriverContext::TConnectionMode CDBL_Connection::ConnectMode() const
{
    I_DriverContext::TConnectionMode mode = 0;
    if ( IsBCPable() ) {
        mode |= I_DriverContext::fBcpIn;
    }
    if ( HasSecureLogin() ) {
        mode |= I_DriverContext::fPasswordEncrypted;
    }
    return mode;
}

CDBL_Connection::~CDBL_Connection()
{
    try {
        // Close is a virtual function but it is safe to call it from a destructor
        // because it is defined in this class.
        Close();
    }
    NCBI_CATCH_ALL_X( 1, NCBI_CURRENT_FUNCTION )
}


bool CDBL_Connection::Abort()
{
    int fdr= DBIORDESC(GetDBLibConnection());
    int fdw= DBIOWDESC(GetDBLibConnection());
    if(fdr >= 0) {
        close(fdr);
    }
    if(fdw != fdr && fdw >= 0) {
        close(fdw);
    }

    if (fdr >= 0 || fdw >= 0) {
        MarkClosed();
        return true;
    }

    return false;
}

bool CDBL_Connection::Close(void)
{
    if (GetDBLibConnection()) {
        Refresh();

        // Clean user data ...
        dbsetuserdata(GetDBLibConnection(), (BYTE*) NULL);

        dbclose(GetDBLibConnection());
        CheckFunctCall();

        MarkClosed();

#ifdef MS_DBLIB_IN_USE
                dbfreelogin(m_Login);
                CheckFunctCall();
#else
                dbloginfree(m_Login);
                CheckFunctCall();
#endif

        m_Link = NULL;

        return true;
    }

    return false;
}

bool CDBL_Connection::x_SendData(I_ITDescriptor& descr_in,
                                 CDB_Stream& stream, bool log_it)
{
    size_t size = stream.Size();
    if (size < 1)
        return false;

    I_ITDescriptor* p_desc= 0;

    // check what type of descriptor we've got
    if(descr_in.DescriptorType() !=
#ifndef MS_DBLIB_IN_USE
        CDBL_ITDESCRIPTOR_TYPE_MAGNUM
#else
        CMSDBL_ITDESCRIPTOR_TYPE_MAGNUM
#endif
        ){
    // this is not a native descriptor
    p_desc= x_GetNativeITDescriptor(dynamic_cast<CDB_ITDescriptor&> (descr_in));
    if(p_desc == 0) return false;
    }


    auto_ptr<I_ITDescriptor> d_guard(p_desc);

    CDBL_ITDescriptor& desc = p_desc? dynamic_cast<CDBL_ITDescriptor&> (*p_desc) :
    dynamic_cast<CDBL_ITDescriptor&> (descr_in);
    char buff[1800]; // maximal page size

    if (size <= sizeof(buff)) { // we could write a blob in one chunk
        size_t s = stream.Read(buff, sizeof(buff));
        if (Check(dbwritetext(GetDBLibConnection(), (char*) desc.m_ObjName.c_str(),
                        desc.m_TxtPtr_is_NULL ? 0 : desc.m_TxtPtr,
                        DBTXPLEN,
                        desc.m_TimeStamp_is_NULL ? 0 : desc.m_TimeStamp,
                        log_it ? TRUE : FALSE, (DBINT) s, (BYTE*) buff))
            != SUCCEED) {
            DATABASE_DRIVER_ERROR( "dbwritetext failed." + GetDbgInfo(), 210030 );
        }
        return true;
    }

    // write it in chunks
    if (Check(dbwritetext(GetDBLibConnection(), (char*) desc.m_ObjName.c_str(),
                    desc.m_TxtPtr_is_NULL ? 0 : desc.m_TxtPtr,
                    DBTXPLEN,
                    desc.m_TimeStamp_is_NULL ? 0 : desc.m_TimeStamp,
                    log_it ? TRUE : FALSE, (DBINT) size, 0)) != SUCCEED ||
        Check(dbsqlok(GetDBLibConnection())) != SUCCEED ||
        //        dbresults(GetDBLibConnection()) == FAIL) {
        x_Results(GetDBLibConnection()) == FAIL) {
        DATABASE_DRIVER_ERROR( "dbwritetext/dbsqlok/dbresults failed." + GetDbgInfo(), 210031 );
    }

    while (size > 0) {
        size_t s = stream.Read(buff, sizeof(buff));
        if (s < 1) {
            Check(dbcancel(GetDBLibConnection()));
            DATABASE_DRIVER_ERROR( "Text/Image data corrupted." + GetDbgInfo(), 210032 );
        }
        if (Check(dbmoretext(GetDBLibConnection(), (DBINT) s, (BYTE*) buff)) != SUCCEED) {
            Check(dbcancel(GetDBLibConnection()));
            DATABASE_DRIVER_ERROR( "dbmoretext failed." + GetDbgInfo(), 210033 );
        }
        size -= s;
    }

    //    if (dbsqlok(GetDBLibConnection()) != SUCCEED || dbresults(GetDBLibConnection()) == FAIL) {
    if (Check(dbsqlok(GetDBLibConnection())) != SUCCEED || x_Results(GetDBLibConnection()) == FAIL) {
        DATABASE_DRIVER_ERROR( "dbsqlok/dbresults failed." + GetDbgInfo(), 210034 );
    }

    return true;
}

I_ITDescriptor* CDBL_Connection::x_GetNativeITDescriptor(const CDB_ITDescriptor& descr_in)
{
    string q= "set rowcount 1\nupdate ";
    q+= descr_in.TableName();
    q+= " set ";
    q+= descr_in.ColumnName();
    q+= "=NULL where ";
    q+= descr_in.SearchConditions();
    q+= " \nselect ";
    q+= descr_in.ColumnName();
    q+= " from ";
    q+= descr_in.TableName();
    q+= " where ";
    q+= descr_in.SearchConditions();
    q+= " \nset rowcount 0";

    CDB_LangCmd* lcmd= LangCmd(q);
    if(!lcmd->Send()) {
        DATABASE_DRIVER_ERROR( "Cannot send the language command." + GetDbgInfo(), 210035 );
    }

    I_ITDescriptor* descr = NULL;
    bool i;

    while(lcmd->HasMoreResults()) {
        auto_ptr<CDB_Result> res( lcmd->Result() );
        if(res.get() == 0) continue;
        if((res->ResultType() == eDB_RowResult) && (descr == 0)) {
            EDB_Type tt= res->ItemDataType(0);
            if(tt == eDB_Text || tt == eDB_Image) {
                while(res->Fetch()) {
                    res->ReadItem(&i, 1);

                    descr= new CDBL_ITDescriptor(*this, GetDBLibConnection(), descr_in);
                    // descr= res->GetImageOrTextDescriptor();
                    if(descr) {
                        break;
                    }
                }
            }
        }
    }
    delete lcmd;

    return descr;
}

RETCODE CDBL_Connection::x_Results(DBPROCESS* pLink)
{
    unsigned int x_Status= 0x1;
    CDB_Result* dbres;
    impl::CResult* res= 0;

    while ((x_Status & 0x1) != 0) {
#if !defined(FTDS_LOGIC)
        if ((x_Status & 0x20) != 0) { // check for return parameters from exec
            x_Status ^= 0x20;
            int n;
            if (GetResultProcessor() && (n = Check(dbnumrets(pLink))) > 0) {
                res = new CDBL_ParamResult(*this, pLink, n);
                dbres= Create_Result(*res);
                GetResultProcessor()->ProcessResult(*dbres);
                delete dbres;
                delete res;
            }
            continue;
        }

        if ((x_Status & 0x40) != 0) { // check for ret status
            x_Status ^= 0x40;
            if (GetResultProcessor() && Check(dbhasretstat(pLink))) {
                res = new CDBL_StatusResult(*this, pLink);
                dbres= Create_Result(*res);
                GetResultProcessor()->ProcessResult(*dbres);
                delete dbres;
                delete res;
            }
            continue;
        }
        if ((x_Status & 0x10) != 0) { // we do have a compute result
            res = new CDBL_ComputeResult(*this, pLink, &x_Status);
            dbres= Create_Result(*res);
            if(GetResultProcessor()) {
                GetResultProcessor()->ProcessResult(*dbres);
            }
            else {
                while(dbres->Fetch())
                    continue;
            }
            delete dbres;
            delete res;
        }
#endif

        RETCODE rc = Check(dbresults(pLink));

        switch (rc) {
        case SUCCEED:
#if !defined(FTDS_LOGIC)
            x_Status |= 0x60;
#endif
            if (DBCMDROW(pLink) == SUCCEED) { // we could get rows in result
                if(!GetResultProcessor()) {
                    while(1) {
                        switch(Check(dbnextrow(pLink))) {
                        case NO_MORE_ROWS:
                        case FAIL:
                        case BUF_FULL: break;
                        default: continue;
                        }
                        break;
                    }
                    continue;
                }

#if defined(FTDS_LOGIC)
                res = new CTDS_RowResult(*this, pLink, &x_Status);
                if(res) {
                    dbres= Create_Result(*res);
                    GetResultProcessor()->ProcessResult(*dbres);
                    delete dbres;
                    delete res;
                }
                if ((x_Status & 0x10) != 0) { // we do have a compute result
                    res = new CTDS_ComputeResult(*this, pLink, &x_Status);
                    if(res) {
                        dbres= Create_Result(*res);
                        GetResultProcessor()->ProcessResult(*dbres);
                        delete dbres;
                        delete res;
                    }
                }
            }
            continue;
#else
// This optimization is currently unavailable for MS dblib...
#ifndef MS_DBLIB_IN_USE /*Text,Image*/
                if (Check(dbnumcols(pLink)) == 1) {
                    int ct = Check(dbcoltype(pLink, 1));
                    if ((ct == SYBTEXT) || (ct == SYBIMAGE)) {
                        res = new CDBL_BlobResult(*this, pLink);
                    }
                }
#endif // MS_DBLIB_IN_USE
                if (!res)
                    res = new CDBL_RowResult(*this, pLink, &x_Status);
                dbres= Create_Result(*res);
                GetResultProcessor()->ProcessResult(*dbres);
                delete dbres;
                delete res;
            } else {
                continue;
            }
#endif // FTDS_IN_USE
        case NO_MORE_RESULTS:
            x_Status = 2;
            break;
        default:
            return FAIL;
        }
        break;
    }

#if defined(FTDS_LOGIC)
    // we've done with the row results at this point
    // let's look at return parameters and ret status
    if (m_ResProc && x_Status == 2) {
        x_Status = 4;
        int n = Check(dbnumrets(pLink));
        if (n > 0) {
            res = new CTDS_ParamResult(pLink, n);
            if(res) {
                dbres= Create_Result(*res);
                m_ResProc->ProcessResult(*dbres);
                delete dbres;
                delete res;
            }
        }
    }

    if (m_ResProc && x_Status == 4) {
        if (Check(dbhasretstat(pLink))) {
            res = new CTDS_StatusResult(pLink);
            if(res) {
                dbres= Create_Result(*res);
                m_ResProc->ProcessResult(*dbres);
                delete dbres;
                delete res;
            }
        }
    }
#endif

    return SUCCEED;
}

#ifdef FTDS_IN_USE
void CTDS_Connection::TDS_SetTimeout(void)
{
    // This is a private function. It is not supposed to be called outside.
    _ASSERT(false);
}
#endif

RETCODE CDBL_Connection::CheckDead(RETCODE rc)
{
    if (rc == FAIL) {
        if (DBDEAD(GetDBLibConnection()) == TRUE) {
            CDB_ClientEx ex(DIAG_COMPILE_INFO,
                            0,
                            "Database connection is closed",
                            eDiag_Error,
                            220000);

            GetDBLExceptionStorage().Accept(ex);
        } else {
            CDB_ClientEx ex(DIAG_COMPILE_INFO,
                            0,
                            "dblib function call failed",
                            eDiag_Error,
                            220001);

            GetDBLExceptionStorage().Accept(ex);
        }
    }

    CheckFunctCall();

    return rc;
}


void CDBL_Connection::CheckFunctCall(void)
{
    GetDBLExceptionStorage().Handle(GetMsgHandlers());
}


void CDBL_Connection::CheckFunctCall(const string& extra_msg)
{
    GetDBLExceptionStorage().Handle(GetMsgHandlers(), extra_msg);
}


/////////////////////////////////////////////////////////////////////////////
//
//  CDBL_SendDataCmd::
//


CDBL_SendDataCmd::CDBL_SendDataCmd(CDBL_Connection& conn,
                                   DBPROCESS* cmd,
                                   size_t nof_bytes) :
    CDBL_Cmd(conn, cmd),
    impl::CSendDataCmd(conn, nof_bytes)
{
}


size_t CDBL_SendDataCmd::SendChunk(const void* pChunk, size_t nof_bytes)
{
    CHECK_DRIVER_ERROR(
        !pChunk  ||  !nof_bytes,
        "Wrong (zero) arguments." + GetDbgInfo(),
        290000 );

    if (!GetBytes2Go())
        return 0;

    if (nof_bytes > GetBytes2Go())
        nof_bytes = GetBytes2Go();

    if (Check(dbmoretext(GetCmd(), (DBINT) nof_bytes, (BYTE*) pChunk)) != SUCCEED) {
        Check(dbcancel(GetCmd()));
        DATABASE_DRIVER_ERROR( "dbmoretext failed." + GetDbgInfo(), 290001 );
    }

    SetBytes2Go(GetBytes2Go() - nof_bytes);

    if (GetBytes2Go() <= 0) {
        //        if (dbsqlok(m_Cmd) != SUCCEED || dbresults(m_Cmd) == FAIL) {
        if (Check(dbsqlok(GetCmd())) != SUCCEED || GetConnection().x_Results(GetCmd()) == FAIL) {
            DATABASE_DRIVER_ERROR( "dbsqlok/results failed." + GetDbgInfo(), 290002 );
        }
    }

    return nof_bytes;
}


bool CDBL_SendDataCmd::Cancel(void)
{
    if (GetBytes2Go() > 0) {
        Check(dbcancel(GetCmd()));
        SetBytes2Go(0);
        return true;
    }

    return false;
}


CDBL_SendDataCmd::~CDBL_SendDataCmd()
{
    try {
        DetachInterface();

        GetConnection().DropCmd(*this);

        Cancel();
    }
    NCBI_CATCH_ALL_X( 2, NCBI_CURRENT_FUNCTION )
}


END_NCBI_SCOPE



