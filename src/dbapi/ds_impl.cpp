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
* File Description:  DataSource implementation
*
*
* $Log$
* Revision 1.1  2002/01/30 14:51:21  kholodov
* User DBAPI implementation, first commit
*
*
*
*
*/

#include "ds_impl.hpp"
#include "conn_impl.hpp"
#include "dbexception.hpp"
//#include <dbapi/driver/public.hpp>
#include <dbapi/driver/ctlib/interfaces.hpp>


CDataSource::CDataSource(I_DriverContext *ctx,
			 CNcbiOstream *out)
  : m_loginTimeout(30), m_context(ctx), m_poolUsed(false)
{

  SetLogStream(out);
}

CDataSource::~CDataSource()
{
  Notify(CDbapiDeletedEvent(this));
  delete m_context;
}

void CDataSource::SetLoginTimeout(unsigned int i) 
{
  m_loginTimeout = i;
  if( m_context != 0 ) {
    m_context->SetLoginTimeout(i);
  }
}

void CDataSource::SetLogStream(CNcbiOstream* out) 
{
  if( out != 0 ) {
    CDB_UserHandler *newH = new CDB_UserHandler_Stream(out);
    CDB_UserHandler *h = CDB_UserHandler::SetDefault(newH);
    delete h;
  }
}

IConnection* CDataSource::CreateConnection()
{
  CConnection *ci = new CConnection(this);
  AddListener(ci);
  return ci;
}

void CDataSource::Action(const CDbapiEvent& e) 
{
  if( dynamic_cast<const CDbapiDeletedEvent*>(&e) != 0 ) {
    RemoveListener(dynamic_cast<IEventListener*>(e.GetSource()));
  }
}

