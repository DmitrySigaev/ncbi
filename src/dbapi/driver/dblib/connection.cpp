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

#ifndef USE_MS_DBLIB
#  include <dbapi/driver/dblib/interfaces.hpp>
#  include <dbapi/driver/dblib/interfaces_p.hpp>
#else
#  include <dbapi/driver/msdblib/interfaces.hpp>
#  include <dbapi/driver/msdblib/interfaces_p.hpp>
#endif
#include <string.h>


BEGIN_NCBI_SCOPE


CDBL_Connection::CDBL_Connection(CDBLibContext* cntx, DBPROCESS* con,
                                 bool reusable, const string& pool_name) :
    m_Link(con), m_Context(cntx), m_Pool(pool_name), m_Reusable(reusable),
    m_BCPAble(false), m_SecureLogin(false)
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
    if (!m_BCPAble) {
        throw CDB_ClientEx(eDB_Error, 210003, "CDBL_Connection::BCPIn",
                           "No bcp on this connection");
    }
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
    if (data_size < 1) {
        throw CDB_ClientEx(eDB_Fatal, 210092,
                           "CDBL_Connection::SendDataCmd",
                           "wrong (zero) data size");
    }

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
        dbresults(m_Link) == FAIL) {
        throw CDB_ClientEx(eDB_Error, 210093, "CDBL_Connection::SendDataCmd",
                           "dbwritetext/dbsqlok/dbresults failed");
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
    if (!Refresh())
        dbclose(m_Link);
}


void CDBL_Connection::DropCmd(CDB_BaseEnt& cmd)
{
    m_CMDs.Remove(static_cast<TPotItem> (&cmd));
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
            throw CDB_ClientEx(eDB_Error, 210030, "CDBL_Connection::SendData",
                               "dbwritetext failed");
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
        dbresults(m_Link) == FAIL) {
        throw CDB_ClientEx(eDB_Error, 210031, "CDBL_Connection::SendData",
                           "dbwritetext/dbsqlok/dbresults failed");
    }

    while (size > 0) {
        size_t s = stream.Read(buff, sizeof(buff));
        if (s < 1) {
            dbcancel(m_Link);
            throw CDB_ClientEx(eDB_Fatal, 210032, "CDBL_Connection::SendData",
                               "Text/Image data corrupted");
        }
        if (dbmoretext(m_Link, (DBINT) s, (BYTE*) buff) != SUCCEED) {
            dbcancel(m_Link);
            throw CDB_ClientEx(eDB_Error, 210033, "CDBL_Connection::SendData",
                               "dbmoretext failed");
        }
        size -= s;
    }

    if (dbsqlok(m_Link) != SUCCEED || dbresults(m_Link) == FAIL) {
        throw CDB_ClientEx(eDB_Error, 210034, "CDBL_Connection::SendData",
                           "dbsqlok/dbresults failed");
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
	throw CDB_ClientEx(eDB_Error, 210035, "CDBL_Connection::x_GetNativeITDescriptor",
			   "can not send the language command");
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
		
		    descr= new CDBL_ITDescriptor(m_Link, descr_in);
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
    if (!pChunk  ||  !nof_bytes) {
        throw CDB_ClientEx(eDB_Fatal, 290000, "CDBL_SendDataCmd::SendChunk",
                           "wrong (zero) arguments");
    }
    if (!m_Bytes2go)
        return 0;

    if (nof_bytes > m_Bytes2go)
        nof_bytes = m_Bytes2go;

    if (dbmoretext(m_Cmd, (DBINT) nof_bytes, (BYTE*) pChunk) != SUCCEED) {
        dbcancel(m_Cmd);
        throw CDB_ClientEx(eDB_Error, 290001, "CDBL_SendDataCmd::SendChunk",
                           "dbmoretext failed");
    }

    m_Bytes2go -= nof_bytes;

    if (m_Bytes2go <= 0) {
        if (dbsqlok(m_Cmd) != SUCCEED || dbresults(m_Cmd) == FAIL) {
            throw CDB_ClientEx(eDB_Error, 290002,
                               "CDBL_SendDataCmd::SendChunk",
                               "dbsqlok/results failed");
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
    if (m_Bytes2go > 0)
        dbcancel(m_Cmd);
    if (m_BR)
        *m_BR = 0;
}


END_NCBI_SCOPE



/*
 * ===========================================================================
 * $Log$
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
