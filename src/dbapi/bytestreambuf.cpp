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
* Author: Michael Kholodov
*
* File Description: streambuf implementation for BLOBs
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.1  2002/01/30 14:51:21  kholodov
* User DBAPI implementation, first commit
*
*
*
* ===========================================================================
*/

#include <exception>
#include <algorithm>
#include "bytestreambuf.hpp"
#include <dbapi/driver/exception.hpp>
#include <dbapi/driver/interfaces.hpp>
#include <dbapi/driver/public.hpp>


CByteStreamBuf::CByteStreamBuf(streamsize bufsize)
  : m_buf(0), 
    m_size(bufsize > 0 ? bufsize : DEF_BUFSIZE), 
    m_len(0), m_rs(0), m_cmd(0)
{ 
  m_buf = new CT_CHAR_TYPE[m_size * 2]; // read and write buffer in one
  setg(0, 0, 0); // call underflow on the first read

  setp(getPBuf(), getPBuf() + m_size);
}

CByteStreamBuf::~CByteStreamBuf()
{
  if( m_rs != 0 && m_len > 0 )
    m_rs->SkipItem();

  delete[] m_buf;
  delete m_cmd;
}

CT_CHAR_TYPE* CByteStreamBuf::getGBuf()
{
  return m_buf;
}

CT_CHAR_TYPE* CByteStreamBuf::getPBuf() 
{
  return m_buf + m_size;
}

void CByteStreamBuf::SetCmd(CDB_SendDataCmd* cmd) {
  delete m_cmd;
  m_cmd = cmd;
}

void CByteStreamBuf::SetRs(CDB_Result* rs) {
  delete m_rs;
  m_rs = rs;
}

CT_INT_TYPE CByteStreamBuf::underflow()
{
  if( m_rs == 0 )
    throw runtime_error("CByteStreamBuf::underflow(): CDB_Result* is null");
  
  static size_t total = 0;

  m_len = m_rs->ReadItem(getGBuf(), m_size);
  total += m_len;
  if( m_len == 0 ) {
    NcbiCout << "Total read from readItem: " << total << endl;
    return CT_EOF;
  }
  else {
    setg(getGBuf(), getGBuf(), getGBuf() + m_len);
    return CT_TO_INT_TYPE(*getGBuf());
  }
    
}

CT_INT_TYPE CByteStreamBuf::overflow(CT_INT_TYPE c)
{
  if( m_cmd == 0 )
    throw runtime_error("CByteStreamBuf::overflow(): CDB_SendDataCmd* is null");

  static size_t total = 0;
  size_t put = m_cmd->SendChunk(pbase(), pptr() - pbase());
  total += put;
  if( put > 0 ) {
    setp(getPBuf(), getPBuf() + m_size );

    if( ! CT_EQ_INT_TYPE(c, CT_EOF) )
      sputc(CT_TO_CHAR_TYPE(c));

    return c;
  }
  else {
    NcbiCout << "Total sent: " << total << endl;
    return CT_EOF;
  }
    
}

int CByteStreamBuf::sync()
{
  overflow(CT_EOF);
  return 0;
}

streambuf* 
CByteStreamBuf::setbuf(CT_CHAR_TYPE* /*p*/, streamsize /*n*/)
{
  throw runtime_error("CByteStreamBuf::setbuf(): not allowed");
  return this;
}


streamsize CByteStreamBuf::showmanyc() 
{
  streamsize left = egptr() - gptr();
  return left > 1 ? left : -1;
}
//======================================================
