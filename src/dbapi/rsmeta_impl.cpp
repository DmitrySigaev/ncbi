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
* File Description:  Resultset metadata implementation
*
*
* $Log$
* Revision 1.3  2002/09/18 18:49:27  kholodov
* Modified: class declaration and Action method to reflect
* direct inheritance of CActiveObject from IEventListener
*
* Revision 1.2  2002/09/09 20:48:57  kholodov
* Added: Additional trace output about object life cycle
* Added: CStatement::Failed() method to check command status
*
* Revision 1.1  2002/01/30 14:51:21  kholodov
* User DBAPI implementation, first commit
*
*
*
*
*/

#include "rsmeta_impl.hpp"
#include "rs_impl.hpp"

#include <dbapi/driver/exception.hpp>
#include <dbapi/driver/public.hpp>

CResultSetMetaData::CResultSetMetaData(CDB_Result *rs)
  : m_totalColumns(rs->NofItems())
{
    SetIdent("CResultSetMetaData");

  // Fill out column metadata
  for(unsigned int i = 0; i < m_totalColumns; ++i ) {
  
    SColMetaData md(rs->ItemName(i) == 0 ? "" : rs->ItemName(i),
		    rs->ItemDataType(i),
		    rs->ItemMaxSize(i));

    m_colInfo.Add(md);
    
  }
}

CResultSetMetaData::~CResultSetMetaData()
{
  Notify(CDbapiDeletedEvent(this));
}

void CResultSetMetaData::Action(const CDbapiEvent& e) 
{
    _TRACE(GetIdent() << " " << (void*)this << ": '" << e.GetName() 
           << "' from " << e.GetSource()->GetIdent());

  if(dynamic_cast<const CDbapiDeletedEvent*>(&e) != 0 ) {
    RemoveListener(e.GetSource());
    if(dynamic_cast<CResultSet*>(e.GetSource()) != 0 ) {
        _TRACE("Deleting " << GetIdent() << " " << (void*)this); 
      delete this;
      //SetValid(false);
    }
  }
}


