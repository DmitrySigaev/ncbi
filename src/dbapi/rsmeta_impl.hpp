#ifndef _RSMETA_IMPL_HPP_
#define _RSMETA_IMPL_HPP_

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
* Revision 1.3  2002/06/11 18:21:52  kholodov
* Fixed: column numbers are one-based for metadata
*
* Revision 1.2  2002/02/05 17:24:02  kholodov
* Put into NCBI scope
*
* Revision 1.1  2002/01/30 14:51:23  kholodov
* User DBAPI implementation, first commit
*
*
*
*/

#include <dbapi/dbapi.hpp>
#include "dbexception.hpp"
#include "array.hpp"
#include "active_obj.hpp"

BEGIN_NCBI_SCOPE

class CDB_Result;

class CResultSetMetaData : public CActiveObject, 
			   public IEventListener,
			   public IResultSetMetaData
{

public:
  CResultSetMetaData(CDB_Result *rs);

  virtual ~CResultSetMetaData();
  
  virtual unsigned int GetTotalColumns() const;
  virtual EDB_Type GetType(unsigned int idx) const;
  virtual int GetMaxSize(unsigned int idx) const;
  virtual string GetName(unsigned int idx) const;

  // Interface IEventListener implementation
  virtual void Action(const CDbapiEvent& e);

private:

  struct SColMetaData 
  {
    SColMetaData(const string& name,
		 EDB_Type type,
		 int maxSize)
      : m_name(name), m_type(type), m_maxSize(maxSize) {}

    string m_name;
    EDB_Type m_type;
    int m_maxSize;
  };
  
  unsigned int m_totalColumns;
  CDynArray<SColMetaData> m_colInfo;

};

//====================================================================
inline
unsigned int CResultSetMetaData::GetTotalColumns() const 
{
  CheckValid();
  return m_totalColumns;
}

inline
EDB_Type CResultSetMetaData::GetType(unsigned int idx) const 
{
  CheckValid();
  return m_colInfo[idx-1].m_type;
}

inline
int CResultSetMetaData::GetMaxSize(unsigned int idx) const 
{
  CheckValid();
  return m_colInfo[idx-1].m_maxSize;
}

inline
string CResultSetMetaData::GetName(unsigned int idx) const 
{
  CheckValid();
  return m_colInfo[idx-1].m_name;
}
//====================================================================

END_NCBI_SCOPE

#endif // _RSMETA_IMPL_HPP_
