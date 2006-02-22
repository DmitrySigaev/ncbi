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

#include <dbapi/driver/dblib/interfaces.hpp>
#include <dbapi/driver/dblib/interfaces_p.hpp>

#include <string.h>


#if defined(NCBI_OS_MSWIN)
#include <io.h>
inline int close(int fd)
{
    return _close(fd);
}
#endif

BEGIN_NCBI_SCOPE


CDBL_Connection::CDBL_Connection(CDBLibContext* cntx, DBPROCESS* con,
                                 bool reusable, const string& pool_name) :
    m_Link(con), m_Context(cntx), m_Pool(pool_name), m_Reusable(reusable),
    m_BCPAble(false), m_SecureLogin(false), m_ResProc(0)
{
    dbsetuserdata(m_Link, (BYTE*) this);
}


bool CDBL_Connection::IsAlive()
{
    return DBDEAD(m_Link) == FALSE;
}


CDB_LangCmd* CDBL_Connection::LangCmd(const string& lang_query,
                                      unsigned int nof_parms)
{
    CDBL_LangCmd* lcmd = new CDBL_LangCmd(this, m_Link, lang_query, nof_parms);
    m_CMDs.Add(lcmd);
    return Create_LangCmd(*lcmd);
}


CDB_RPCCmd* CDBL_Connection::RPC(const string& rpc_name, unsigned int nof_args)
{
    CDBL_RPCCmd* rcmd = new CDBL_RPCCmd(this, m_Link, rpc_name, nof_args);
    m_CMDs.Add(rcmd);
    return Create_RPCCmd(*rcmd);
}


CDB_BCPInCmd* CDBL_Connection::BCPIn(const string& tab_name,
                                     unsigned int nof_cols)
{
    CHECK_DRIVER_ERROR( !m_BCPAble, "No bcp on this connection", 210003 );
    CDBL_BCPInCmd* bcmd = new CDBL_BCPInCmd(this, m_Link, tab_name, nof_cols);
    m_CMDs.Add(bcmd);
    return Create_BCPInCmd(*bcmd);
}


CDB_CursorCmd* CDBL_Connection::Cursor(const string& cursor_name,
                                       const string& query,
                                       unsigned int nof_params,
                                       unsigned int)
{
    CDBL_CursorCmd* ccmd = new CDBL_CursorCmd(this, m_Link, cursor_name,
                                              query, nof_params);
    m_CMDs.Add(ccmd);
    return Create_CursorCmd(*ccmd);
}


CDB_SendDataCmd* CDBL_Connection::SendDataCmd(I_ITDescriptor& descr_in,
                                              size_t data_size, bool log_it)
{
    CHECK_DRIVER_FATAL( data_size < 1, "wrong (zero) data size", 210092 );

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
    

    C_ITDescriptorGuard d_guard(p_desc);

    CDBL_ITDescriptor& desc = p_desc? dynamic_cast<CDBL_ITDescriptor&> (*p_desc) : 
    dynamic_cast<CDBL_ITDescriptor&> (descr_in);

    if (dbwritetext(m_Link,
                    (char*) desc.m_ObjName.c_str(),
                    desc.m_TxtPtr_is_NULL ? 0 : desc.m_TxtPtr,
                    DBTXPLEN,
                    desc.m_TimeStamp_is_NULL ? 0 : desc.m_TimeStamp,
                    log_it ? TRUE : FALSE,
                    (DBINT) data_size, 0) != SUCCEED ||
        dbsqlok(m_Link) != SUCCEED ||
        //        dbresults(m_Link) == FAIL) {
        x_Results(m_Link) == FAIL) {
        DATABASE_DRIVER_ERROR( "dbwritetext/dbsqlok/dbresults failed", 210093 );
    }

    CDBL_SendDataCmd* sd_cmd = new CDBL_SendDataCmd(this, m_Link, data_size);
    m_CMDs.Add(sd_cmd);
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
    while (m_CMDs.NofItems()) {
        CDB_BaseEnt* pCmd = static_cast<CDB_BaseEnt*> (m_CMDs.Get(0));
        delete pCmd;
        m_CMDs.Remove((int) 0);
    }

    // cancel all pending commands
    if (dbcancel(m_Link) != CS_SUCCEED)
        return false;

    // check the connection status
    return DBDEAD(m_Link) == FALSE;
}


const string& CDBL_Connection::ServerName() const
{
    return m_Server;
}


const string& CDBL_Connection::UserName() const
{
    return m_User;
}


const string& CDBL_Connection::Password() const
{
    return m_Passwd;
}


I_DriverContext::TConnectionMode CDBL_Connection::ConnectMode() const
{
    I_DriverContext::TConnectionMode mode = 0;
    if ( m_BCPAble ) {
        mode |= I_DriverContext::fBcpIn;
    }
    if ( m_SecureLogin ) {
        mode |= I_DriverContext::fPasswordEncrypted;
    }
    return mode;
}

bool CDBL_Connection::IsReusable() const
{
    return m_Reusable;
}


const string& CDBL_Connection::PoolName() const
{
    return m_Pool;
}


I_DriverContext* CDBL_Connection::Context() const
{
    return const_cast<CDBLibContext*> (m_Context);
}


void CDBL_Connection::PushMsgHandler(CDB_UserHandler* h)
{
    m_MsgHandlers.Push(h);
}


void CDBL_Connection::PopMsgHandler(CDB_UserHandler* h)
{
    m_MsgHandlers.Pop(h);
}

CDB_ResultProcessor* CDBL_Connection::SetResultProcessor(CDB_ResultProcessor* rp)
{
    CDB_ResultProcessor* r= m_ResProc;
    m_ResProc= rp;
    return r;
}

void CDBL_Connection::Release()
{
    m_BR = 0;
    // close all commands first
    while(m_CMDs.NofItems() > 0) {
        CDB_BaseEnt* pCmd = static_cast<CDB_BaseEnt*> (m_CMDs.Get(0));
        delete pCmd;
        m_CMDs.Remove((int) 0);
    }
}


CDBL_Connection::~CDBL_Connection()
{
    try {
        Refresh();
        dbclose(m_Link);
        m_Link = NULL;
    }
    NCBI_CATCH_ALL( kEmptyStr )
}


void CDBL_Connection::DropCmd(CDB_BaseEnt& cmd)
{
    m_CMDs.Remove(static_cast<TPotItem> (&cmd));
}

bool CDBL_Connection::Abort()
{
    int fdr= DBIORDESC(m_Link);
    int fdw= DBIOWDESC(m_Link);
    if(fdr >= 0) {
        close(fdr);
    }
    if(fdw != fdr && fdw >= 0) {
        close(fdw);
    }
    return (fdr >= 0 || fdw >= 0);
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
    

    C_ITDescriptorGuard d_guard(p_desc);

    CDBL_ITDescriptor& desc = p_desc? dynamic_cast<CDBL_ITDescriptor&> (*p_desc) : 
    dynamic_cast<CDBL_ITDescriptor&> (descr_in);
    char buff[1800]; // maximal page size

    if (size <= sizeof(buff)) { // we could write a blob in one chunk
        size_t s = stream.Read(buff, sizeof(buff));
        if (dbwritetext(m_Link, (char*) desc.m_ObjName.c_str(),
                        desc.m_TxtPtr_is_NULL ? 0 : desc.m_TxtPtr,
                        DBTXPLEN,
                        desc.m_TimeStamp_is_NULL ? 0 : desc.m_TimeStamp,
                        log_it ? TRUE : FALSE, (DBINT) s, (BYTE*) buff)
            != SUCCEED) {
            DATABASE_DRIVER_ERROR( "dbwritetext failed", 210030 );
        }
        return true;
    }

    // write it in chunks
    if (dbwritetext(m_Link, (char*) desc.m_ObjName.c_str(),
                    desc.m_TxtPtr_is_NULL ? 0 : desc.m_TxtPtr,
                    DBTXPLEN,
                    desc.m_TimeStamp_is_NULL ? 0 : desc.m_TimeStamp,
                    log_it ? TRUE : FALSE, (DBINT) size, 0) != SUCCEED ||
        dbsqlok(m_Link) != SUCCEED ||
        //        dbresults(m_Link) == FAIL) {
        x_Results(m_Link) == FAIL) {
        DATABASE_DRIVER_ERROR( "dbwritetext/dbsqlok/dbresults failed", 210031 );
    }

    while (size > 0) {
        size_t s = stream.Read(buff, sizeof(buff));
        if (s < 1) {
            dbcancel(m_Link);
            DATABASE_DRIVER_ERROR( "Text/Image data corrupted", 210032 );
        }
        if (dbmoretext(m_Link, (DBINT) s, (BYTE*) buff) != SUCCEED) {
            dbcancel(m_Link);
            DATABASE_DRIVER_ERROR( "dbmoretext failed", 210033 );
        }
        size -= s;
    }

    //    if (dbsqlok(m_Link) != SUCCEED || dbresults(m_Link) == FAIL) {
    if (dbsqlok(m_Link) != SUCCEED || x_Results(m_Link) == FAIL) {
        DATABASE_DRIVER_ERROR( "dbsqlok/dbresults failed", 210034 );
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
    
    CDB_LangCmd* lcmd= LangCmd(q, 0);
    if(!lcmd->Send()) {
        DATABASE_DRIVER_ERROR( "Cannot send the language command", 210035 );
    }

    I_ITDescriptor* descr= 0;
    bool i;

    while(lcmd->HasMoreResults()) {
        auto_ptr<CDB_Result> res( lcmd->Result() );
        if(res.get() == 0) continue;
        if((res->ResultType() == eDB_RowResult) && (descr == 0)) {
            EDB_Type tt= res->ItemDataType(0);
            if(tt == eDB_Text || tt == eDB_Image) {
                while(res->Fetch()) {
                    res->ReadItem(&i, 1);
                
                    descr= new CDBL_ITDescriptor(m_Link, descr_in);
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
    I_Result* res= 0;

    while ((x_Status & 0x1) != 0) {
#if !defined(FTDS_LOGIC)
        if ((x_Status & 0x20) != 0) { // check for return parameters from exec
            x_Status ^= 0x20;
            int n;
            if (m_ResProc && (n = dbnumrets(pLink)) > 0) {
                res = new CDBL_ParamResult(pLink, n);
                dbres= Create_Result(*res);
                m_ResProc->ProcessResult(*dbres);
                delete dbres;
                delete res;
            }
            continue;
        }

        if ((x_Status & 0x40) != 0) { // check for ret status
            x_Status ^= 0x40;
            if (m_ResProc && dbhasretstat(pLink)) {
                res = new CDBL_StatusResult(pLink);
                dbres= Create_Result(*res);
                m_ResProc->ProcessResult(*dbres);
                delete dbres;
                delete res;
            }
            continue;
        }
        if ((x_Status & 0x10) != 0) { // we do have a compute result
            res = new CDBL_ComputeResult(pLink, &x_Status);
            dbres= Create_Result(*res);
            if(m_ResProc) {
                m_ResProc->ProcessResult(*dbres);
            }
            else {
                while(dbres->Fetch())
                    continue;
            }
            delete dbres;
            delete res;
        }
#endif
        switch (dbresults(pLink)) {
        case SUCCEED:
#if !defined(FTDS_LOGIC)
            x_Status |= 0x60;
#endif
            if (DBCMDROW(pLink) == SUCCEED) { // we could get rows in result
                if(!m_ResProc) {
                    while(1) {
                        switch(dbnextrow(pLink)) {
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
                res = new CTDS_RowResult(pLink, &x_Status);
                if(res) {
                    dbres= Create_Result(*res);
                    m_ResProc->ProcessResult(*dbres);
                    delete dbres;
                    delete res;
                }
                if ((x_Status & 0x10) != 0) { // we do have a compute result
                    res = new CTDS_ComputeResult(pLink, &x_Status);
                    if(res) {
                        dbres= Create_Result(*res);
                        m_ResProc->ProcessResult(*dbres);
                        delete dbres;
                        delete res;
                    }
                }
            } 
            continue;
#else
// This optimization is currently unavailable for MS dblib...
#ifndef MS_DBLIB_IN_USE /*Text,Image*/
                if (dbnumcols(pLink) == 1) {
                    int ct = dbcoltype(pLink, 1);
                    if ((ct == SYBTEXT) || (ct == SYBIMAGE)) {
                        res = new CDBL_BlobResult(pLink);
                    }
                }
#endif // MS_DBLIB_IN_USE
                if (!res)
                    res = new CDBL_RowResult(pLink, &x_Status);
                dbres= Create_Result(*res);
                m_ResProc->ProcessResult(*dbres);
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
        int n = dbnumrets(pLink);
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
        if (dbhasretstat(pLink)) {
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

/////////////////////////////////////////////////////////////////////////////
//
//  CDBL_SendDataCmd::
//


CDBL_SendDataCmd::CDBL_SendDataCmd(CDBL_Connection* con, DBPROCESS* cmd,
                                   size_t nof_bytes)
{
    m_Connect  = con;
    m_Cmd      = cmd;
    m_Bytes2go = nof_bytes;
}


size_t CDBL_SendDataCmd::SendChunk(const void* pChunk, size_t nof_bytes)
{
    CHECK_DRIVER_FATAL( 
        !pChunk  ||  !nof_bytes, 
        "wrong (zero) arguments", 
        290000 );

    if (!m_Bytes2go)
        return 0;

    if (nof_bytes > m_Bytes2go)
        nof_bytes = m_Bytes2go;

    if (dbmoretext(m_Cmd, (DBINT) nof_bytes, (BYTE*) pChunk) != SUCCEED) {
        dbcancel(m_Cmd);
        DATABASE_DRIVER_ERROR( "dbmoretext failed", 290001 );
    }

    m_Bytes2go -= nof_bytes;

    if (m_Bytes2go <= 0) {
        //        if (dbsqlok(m_Cmd) != SUCCEED || dbresults(m_Cmd) == FAIL) {
        if (dbsqlok(m_Cmd) != SUCCEED || m_Connect->x_Results(m_Cmd) == FAIL) {
            DATABASE_DRIVER_ERROR( "dbsqlok/results failed", 290002 );
        }
    }

    return nof_bytes;
}


void CDBL_SendDataCmd::Release()
{
    m_BR = 0;
    if (m_Bytes2go > 0) {
        dbcancel(m_Cmd);
        m_Bytes2go = 0;
    }
    m_Connect->DropCmd(*this);
    delete this;
}


CDBL_SendDataCmd::~CDBL_SendDataCmd()
{
    try {
        if (m_Bytes2go > 0)
            dbcancel(m_Cmd);
        if (m_BR)
            *m_BR = 0;
    }
    NCBI_CATCH_ALL( kEmptyStr )
}


END_NCBI_SCOPE



/*
 * ===========================================================================
 * $Log$
 * Revision 1.21  2006/02/22 15:15:50  ssikorsk
 * *** empty log message ***
 *
 * Revision 1.20  2005/10/31 12:19:58  ssikorsk
 * Do not use separate include files for msdblib.
 *
 * Revision 1.19  2005/09/19 14:19:05  ssikorsk
 * Use NCBI_CATCH_ALL macro instead of catch(...)
 *
 * Revision 1.18  2005/09/15 11:00:01  ssikorsk
 * Destructors do not throw exceptions any more.
 *
 * Revision 1.17  2005/08/09 14:54:37  ssikorsk
 * Stylistic changes.
 *
 * Revision 1.16  2005/07/20 12:33:05  ssikorsk
 * Merged ftds/interfaces.hpp into dblib/interfaces.hpp
 *
 * Revision 1.15  2005/07/07 19:12:55  ssikorsk
 * Improved to support a ftds driver
 *
 * Revision 1.14  2005/04/04 13:03:57  ssikorsk
 * Revamp of DBAPI exception class CDB_Exception
 *
 * Revision 1.13  2005/02/25 16:09:33  soussov
 * adds wrapper for close to make windows happy
 *
 * Revision 1.12  2005/02/23 21:39:46  soussov
 * Adds Abort() method to connection
 *
 * Revision 1.11  2005/01/21 13:12:10  dicuccio
 * Set link to NULL after closing in destructor
 *
 * Revision 1.10  2004/05/18 18:30:36  gorelenk
 * PCH <ncbi_pch.hpp> moved to correct place .
 *
 * Revision 1.9  2004/05/17 21:12:41  gorelenk
 * Added include of PCH ncbi_pch.hpp
 *
 * Revision 1.8  2003/06/05 16:01:13  soussov
 * adds code for DumpResults and for the dumped results processing
 *
 * Revision 1.7  2002/08/23 16:31:47  soussov
 * fixes bug in ~CDBL_Connection()
 *
 * Revision 1.6  2002/07/02 16:05:49  soussov
 * splitting Sybase dblib and MS dblib
 *
 * Revision 1.5  2002/03/26 15:37:52  soussov
 * new image/text operations added
 *
 * Revision 1.4  2002/01/08 18:10:18  sapojnik
 * Syabse to MSSQL name translations moved to interface_p.hpp
 *
 * Revision 1.3  2001/10/24 16:39:01  lavr
 * Explicit casts (where necessary) to eliminate 64->32 bit compiler warnings
 *
 * Revision 1.2  2001/10/22 16:28:01  lavr
 * Default argument values removed
 * (mistakenly left while moving code from header files)
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
