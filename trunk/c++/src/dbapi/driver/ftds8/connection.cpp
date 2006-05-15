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
 * File Description:  TDS connection
 *
 */

#include <ncbi_pch.hpp>

#include "ftds8_utils.hpp"

#include <dbapi/driver/ftds/interfaces.hpp>
#include <string.h>

#include <algorithm>

#if defined(NCBI_OS_MSWIN)
#include <io.h>
inline int close(int fd)
{
    return _close(fd);
}
#endif


BEGIN_NCBI_SCOPE


CTDS_Connection::CTDS_Connection(CTDSContext* cntx, DBPROCESS* con,
                                 bool reusable, const string& pool_name) :
    m_Link(con), m_Context(cntx), m_Pool(pool_name), m_Reusable(reusable),
    m_BCPAble(false), m_SecureLogin(false), m_ResProc(0)
{
    dbsetuserdata(m_Link, (BYTE*) this);
    CheckFunctCall();
}


bool CTDS_Connection::IsAlive()
{
    return DBDEAD(m_Link) == FALSE;
}


CDB_LangCmd* CTDS_Connection::LangCmd(const string& lang_query,
                                      unsigned int nof_parms)
{
    CTDS_LangCmd* lcmd = new CTDS_LangCmd(this, m_Link, lang_query, nof_parms);
    m_CMDs.push_back(lcmd);
    return Create_LangCmd(*lcmd);
}


CDB_RPCCmd* CTDS_Connection::RPC(const string& rpc_name, unsigned int nof_args)
{
    CTDS_RPCCmd* rcmd = new CTDS_RPCCmd(this, m_Link, rpc_name, nof_args);
    m_CMDs.push_back(rcmd);
    return Create_RPCCmd(*rcmd);
}


CDB_BCPInCmd* CTDS_Connection::BCPIn(const string& tab_name,
                                     unsigned int nof_cols)
{
    if (!m_BCPAble) {
        DATABASE_DRIVER_ERROR( "No bcp on this connection", 210003 );
    }
    CTDS_BCPInCmd* bcmd = new CTDS_BCPInCmd(this, m_Link, tab_name, nof_cols);
    m_CMDs.push_back(bcmd);
    return Create_BCPInCmd(*bcmd);
}


CDB_CursorCmd* CTDS_Connection::Cursor(const string& cursor_name,
                                       const string& query,
                                       unsigned int nof_params,
                                       unsigned int)
{
    CTDS_CursorCmd* ccmd = new CTDS_CursorCmd(this, m_Link, cursor_name,
                                              query, nof_params);
    m_CMDs.push_back(ccmd);
    return Create_CursorCmd(*ccmd);
}


CDB_SendDataCmd* CTDS_Connection::SendDataCmd(I_ITDescriptor& descr_in,
                                              size_t data_size, bool log_it)
{
    if (data_size < 1) {
        DATABASE_DRIVER_ERROR( "wrong (zero) data size", 210092 );
    }

    I_ITDescriptor* p_desc= 0;

    // check what type of descriptor we've got
    if(descr_in.DescriptorType() != CDBL_ITDESCRIPTOR_TYPE_MAGNUM) {
        // this is not a native descriptor
        p_desc= x_GetNativeITDescriptor(dynamic_cast<CDB_ITDescriptor&> (descr_in));
        if(p_desc == 0) return false;
    }
    

    C_ITDescriptorGuard d_guard(p_desc);

    CTDS_ITDescriptor& desc = p_desc? dynamic_cast<CTDS_ITDescriptor&> (*p_desc) : 
        dynamic_cast<CTDS_ITDescriptor&> (descr_in);

    if (Check(dbwritetext(m_Link,
                    (char*) desc.m_ObjName.c_str(),
                    desc.m_TxtPtr_is_NULL ? 0 : desc.m_TxtPtr,
                    DBTXPLEN,
                    desc.m_TimeStamp_is_NULL ? 0 : desc.m_TimeStamp,
                    log_it ? TRUE : FALSE,
                    static_cast<int>(data_size), 0)) != SUCCEED ||
        Check(dbsqlok(m_Link)) != SUCCEED ||
        //        dbresults(m_Link) == FAIL) {
        x_Results(m_Link) == FAIL) {
        DATABASE_DRIVER_ERROR( "dbwritetext/dbsqlok/dbresults failed", 210093 );
    }

    CTDS_SendDataCmd* sd_cmd = new CTDS_SendDataCmd(this, m_Link, data_size);
    m_CMDs.push_back(sd_cmd);
    return Create_SendDataCmd(*sd_cmd);
}


bool CTDS_Connection::SendData(I_ITDescriptor& desc,
                               CDB_Image& img, bool log_it){
    return x_SendData(desc, dynamic_cast<CDB_Stream&> (img), log_it);
}


bool CTDS_Connection::SendData(I_ITDescriptor& desc,
                               CDB_Text&  txt, bool log_it) {
    return x_SendData(desc, dynamic_cast<CDB_Stream&> (txt), log_it);
}


bool CTDS_Connection::Refresh()
{
    // close all commands first
    ITERATE(deque<CDB_BaseEnt*>, it, m_CMDs) {
        try {
            delete *it;
        } catch (CDB_Exception& ) {
            _ASSERT(false);
        }
    }
    m_CMDs.clear();

    // cancel all pending commands
    if (Check(dbcancel(m_Link)) != CS_SUCCEED)
        return false;

    // check the connection status
    return DBDEAD(m_Link) == FALSE;
}


const string& CTDS_Connection::ServerName() const
{
    return m_Server;
}


const string& CTDS_Connection::UserName() const
{
    return m_User;
}


const string& CTDS_Connection::Password() const
{
    return m_Passwd;
}


I_DriverContext::TConnectionMode CTDS_Connection::ConnectMode() const
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

bool CTDS_Connection::IsReusable() const
{
    return m_Reusable;
}


const string& CTDS_Connection::PoolName() const
{
    return m_Pool;
}


I_DriverContext* CTDS_Connection::Context() const
{
    return const_cast<CTDSContext*> (m_Context);
}


void CTDS_Connection::PushMsgHandler(CDB_UserHandler* h,
                                     EOwnership ownership)
{
    m_MsgHandlers.Push(h, ownership);
}


void CTDS_Connection::PopMsgHandler(CDB_UserHandler* h)
{
    m_MsgHandlers.Pop(h);
}

CDB_ResultProcessor* CTDS_Connection::SetResultProcessor(CDB_ResultProcessor* rp)
{
    CDB_ResultProcessor* r= m_ResProc;
    m_ResProc= rp;
    return r;
}

void CTDS_Connection::Release()
{
    m_BR = 0;
    // close all commands first
    ITERATE(deque<CDB_BaseEnt*>, it, m_CMDs) {
        try {
            delete *it;
        } catch (CDB_Exception& ) {
            _ASSERT(false);
        }
    }
    m_CMDs.clear();
}


CTDS_Connection::~CTDS_Connection()
{
    try {
        Close();
    }
    NCBI_CATCH_ALL( kEmptyStr )
}


void CTDS_Connection::DropCmd(CDB_BaseEnt& cmd)
{
    deque<CDB_BaseEnt*>::iterator it = find(m_CMDs.begin(), m_CMDs.end(), &cmd);

    if (it != m_CMDs.end()) {
        m_CMDs.erase(it);
    }
}

bool CTDS_Connection::Abort()
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

bool CTDS_Connection::Close(void)
{
    if (m_Link) {
        try {
            Refresh();
            dbclose(m_Link);
            CheckFunctCall();
            m_Link = NULL;
            return true;
        }
        NCBI_CATCH_ALL( kEmptyStr )
    }

    return false;
}

bool CTDS_Connection::x_SendData(I_ITDescriptor& descr_in,
                                 CDB_Stream& stream, bool log_it)
{
    size_t size = stream.Size();
    if (size < 1)
        return false;

    I_ITDescriptor* p_desc= 0;

    // check what type of descriptor we've got
    if(descr_in.DescriptorType() != CDBL_ITDESCRIPTOR_TYPE_MAGNUM) {
        // this is not a native descriptor
        p_desc= x_GetNativeITDescriptor(dynamic_cast<CDB_ITDescriptor&> (descr_in));
        if(p_desc == 0) return false;
    }
    

    C_ITDescriptorGuard d_guard(p_desc);

    CTDS_ITDescriptor& desc = p_desc? dynamic_cast<CTDS_ITDescriptor&> (*p_desc) : 
        dynamic_cast<CTDS_ITDescriptor&> (descr_in);
    // CTDS_ITDescriptor& desc = dynamic_cast<CTDS_ITDescriptor&> (descr_in);
    char buff[1800]; // maximal page size

    if (size <= sizeof(buff)) { // we could write a blob in one chunk
        size_t s = stream.Read(buff, sizeof(buff));
        if (Check(dbwritetext(m_Link, (char*) desc.m_ObjName.c_str(),
                        desc.m_TxtPtr_is_NULL ? 0 : desc.m_TxtPtr,
                        DBTXPLEN,
                        desc.m_TimeStamp_is_NULL ? 0 : desc.m_TimeStamp,
                        log_it ? TRUE : FALSE, (DBINT) s, (BYTE*) buff))
            != SUCCEED) {
            DATABASE_DRIVER_ERROR( "dbwritetext failed", 210030 );
        }
        return true;
    }

    // write it in chunks
    if (Check(dbwritetext(m_Link, (char*) desc.m_ObjName.c_str(),
                    desc.m_TxtPtr_is_NULL ? 0 : desc.m_TxtPtr,
                    DBTXPLEN,
                    desc.m_TimeStamp_is_NULL ? 0 : desc.m_TimeStamp,
                    log_it ? TRUE : FALSE, (DBINT) size, 0)) != SUCCEED ||
        Check(dbsqlok(m_Link)) != SUCCEED ||
        //        dbresults(m_Link) == FAIL) {
        x_Results(m_Link) == FAIL) {
        DATABASE_DRIVER_ERROR( "dbwritetext/dbsqlok/dbresults failed", 210031 );
    }

    while (size > 0) {
        size_t s = stream.Read(buff, sizeof(buff));
        if (s < 1) {
            Check(dbcancel(m_Link));
            DATABASE_DRIVER_ERROR( "Text/Image data corrupted", 210032 );
        }
        if (Check(dbmoretext(m_Link, (DBINT) s, (BYTE*) buff)) != SUCCEED) {
            Check(dbcancel(m_Link));
            DATABASE_DRIVER_ERROR( "dbmoretext failed", 210033 );
        }
        size -= s;
    }

    //    if (dbsqlok(m_Link) != SUCCEED || dbresults(m_Link) == FAIL) {
    if (Check(dbsqlok(m_Link)) != SUCCEED || x_Results(m_Link) == FAIL) {
        DATABASE_DRIVER_ERROR( "dbsqlok/dbresults failed", 110034 );
    }

    return true;
}

I_ITDescriptor* CTDS_Connection::x_GetNativeITDescriptor(const CDB_ITDescriptor& descr_in)
{
    string q= "set rowcount 1\nupdate ";
    q+= descr_in.TableName();
    q+= " set ";
    q+= descr_in.ColumnName();
    q+= "='0x0' where ";
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

    CDB_Result* res;
    I_ITDescriptor* descr= 0;
    bool i;

    while(lcmd->HasMoreResults()) {
        res= lcmd->Result();
        if(res == 0) continue;
        if((res->ResultType() == eDB_RowResult) && (descr == 0)) {
            EDB_Type tt= res->ItemDataType(0);
            if(tt == eDB_Text || tt == eDB_Image) {
                while(res->Fetch()) {
                    res->ReadItem(&i, 1);
        
                    descr= new CTDS_ITDescriptor(*this, m_Link, descr_in);
                    // descr= res->GetImageOrTextDescriptor();
                    if(descr) break;
                }
            }
        }
        delete res;
    }
    delete lcmd;
        
    return descr;
}


RETCODE CTDS_Connection::x_Results(DBPROCESS* pLink)
{
    unsigned int x_Status= 0x1;
    CDB_Result* dbres;
    I_Result* res= 0;

    while ((x_Status & 0x1) != 0) {
        switch (Check(dbresults(pLink))) {
        case SUCCEED:
            if (DBCMDROW(pLink) == SUCCEED) { // we may get rows in this result
                if(!m_ResProc) {
                    for(;;) {
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
                res = new CTDS_RowResult(*this, pLink, &x_Status);
                if(res) {
                    dbres= Create_Result(*res);
                    m_ResProc->ProcessResult(*dbres);
                    delete dbres;
                    delete res;
                }
                if ((x_Status & 0x10) != 0) { // we do have a compute result
                    res = new CTDS_ComputeResult(*this, pLink, &x_Status);
                    if(res) {
                        dbres= Create_Result(*res);
                        m_ResProc->ProcessResult(*dbres);
                        delete dbres;
                        delete res;
                    }
                }
            } 
            continue;
            
        case NO_MORE_RESULTS:
            x_Status = 2;
            break;
        default:
            return FAIL;
        }
        break;
    }
    
    // we've done with the row results at this point
    // let's look at return parameters and ret status
    if (m_ResProc && x_Status == 2) {
        x_Status = 4;
        int n = Check(dbnumrets(pLink));
        if (n > 0) {
            res = new CTDS_ParamResult(*this, pLink, n);
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
            res = new CTDS_StatusResult(*this, pLink);
            if(res) {
                dbres= Create_Result(*res);
                m_ResProc->ProcessResult(*dbres);
                delete dbres;
                delete res;
            }
        }
    }
    return SUCCEED;
}

void CTDS_Connection::TDS_SetTimeout(void)
{        
    m_Link->tds_socket->timeout= (TDS_INT)(m_Context->TDS_GetTimeout());
}

void CTDS_Connection::CheckFunctCall(void)
{
    GetFTDS8ExceptionStorage().Handle(m_MsgHandlers);
}


/////////////////////////////////////////////////////////////////////////////
//
//  CTDS_SendDataCmd::
//


CTDS_SendDataCmd::CTDS_SendDataCmd(CTDS_Connection* conn, DBPROCESS* cmd,
                                   size_t nof_bytes)
: CDBL_Cmd( conn, cmd )
{
    m_Bytes2go = nof_bytes;
}


size_t CTDS_SendDataCmd::SendChunk(const void* pChunk, size_t nof_bytes)
{
    if (!pChunk  ||  !nof_bytes) {
        DATABASE_DRIVER_ERROR( "wrong (zero) arguments", 290000 );
    }
    if (!m_Bytes2go)
        return 0;

    if (nof_bytes > m_Bytes2go)
        nof_bytes = m_Bytes2go;

    if (Check(dbmoretext(GetCmd(), static_cast<DBINT>(nof_bytes), (BYTE*) pChunk)) 
        != SUCCEED) {
        Check(dbcancel(GetCmd()));
        DATABASE_DRIVER_ERROR( "dbmoretext failed", 290001 );
    }

    m_Bytes2go -= nof_bytes;

    if (m_Bytes2go <= 0) {
        //        if (dbsqlok(GetCmd()) != SUCCEED || dbresults(GetCmd()) == FAIL) {
        if (Check(dbsqlok(GetCmd())) != SUCCEED || GetConnection().x_Results(GetCmd()) == FAIL) {
            DATABASE_DRIVER_ERROR( "dbsqlok/results failed", 290002 );
        }
    }

    return nof_bytes;
}


void CTDS_SendDataCmd::Release()
{
    m_BR = 0;
    if (m_Bytes2go > 0) {
        Check(dbcancel(GetCmd()));
        m_Bytes2go = 0;
    }
    GetConnection().DropCmd(*this);
    delete this;
}


CTDS_SendDataCmd::~CTDS_SendDataCmd()
{
    try {
        if (m_Bytes2go > 0)
            Check(dbcancel(GetCmd()));
        if (m_BR)
            *m_BR = 0;
    }
    NCBI_CATCH_ALL( kEmptyStr )
}


END_NCBI_SCOPE



/*
 * ===========================================================================
 * $Log$
 * Revision 1.22  2006/05/15 19:42:18  ssikorsk
 * Added EOwnership argument to method PushMsgHandler.
 *
 * Revision 1.21  2006/05/11 18:14:12  ssikorsk
 * Fixed compilation issues
 *
 * Revision 1.20  2006/05/11 18:07:19  ssikorsk
 * Utilized new exception storage
 *
 * Revision 1.19  2006/05/10 13:56:27  ucko
 * +<algorithm> for find()
 *
 * Revision 1.18  2006/05/08 17:49:28  ssikorsk
 * Replaced type of  CDBL_Connection::m_CMDs from CPointerPot to deque<CDB_BaseEnt*>
 *
 * Revision 1.17  2006/05/04 20:12:47  ssikorsk
 * Implemented classs CDBL_Cmd, CDBL_Result and CDBLExceptions;
 * Surrounded each native dblib call with Check;
 *
 * Revision 1.16  2006/04/05 14:31:52  ssikorsk
 * Implemented CTDS_Connection::Close
 *
 * Revision 1.15  2006/02/22 15:15:51  ssikorsk
 * *** empty log message ***
 *
 * Revision 1.14  2005/09/19 14:19:05  ssikorsk
 * Use NCBI_CATCH_ALL macro instead of catch(...)
 *
 * Revision 1.13  2005/09/15 11:00:02  ssikorsk
 * Destructors do not throw exceptions any more.
 *
 * Revision 1.12  2005/08/29 17:10:38  ssikorsk
 * Get rid of warnings on 64-bit platforms
 *
 * Revision 1.11  2005/07/20 12:33:05  ssikorsk
 * Merged ftds/interfaces.hpp into dblib/interfaces.hpp
 *
 * Revision 1.10  2005/04/04 13:03:57  ssikorsk
 * Revamp of DBAPI exception class CDB_Exception
 *
 * Revision 1.9  2005/02/25 16:09:26  soussov
 * adds wrapper for close to make windows happy
 *
 * Revision 1.8  2005/02/23 21:39:08  soussov
 * Adds Abort() method to connection
 *
 * Revision 1.7  2004/05/17 21:13:37  gorelenk
 * Added include of PCH ncbi_pch.hpp
 *
 * Revision 1.6  2003/06/05 16:01:40  soussov
 * adds code for DumpResults and for the dumped results processing
 *
 * Revision 1.5  2002/12/03 19:21:24  soussov
 * formatting
 *
 * Revision 1.4  2002/08/23 16:32:11  soussov
 * fixes bug in ~CTDS_Connection()
 *
 * Revision 1.3  2002/03/26 15:35:10  soussov
 * new image/text operations added
 *
 * Revision 1.2  2001/11/06 18:00:02  lavr
 * Formatted uniformly as the rest of the library
 *
 * Revision 1.1  2001/10/25 00:39:22  vakatov
 * Initial revision
 *
 * ===========================================================================
 */
