#ifndef STRSTREAMBUF__HPP
#define STRSTREAMBUF__HPP

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
* Author:  !!! PUT YOUR NAME(s) HERE !!!
*
* File Description:
*   CStrStreamBuf
*
*/

#include <objects/objmgr/reader.hpp>
#include <connect/ncbi_core_cxx.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

struct CStrStreamBuf : public streambuf
{
  CStrStreamBuf(strstream *s) : m_Stream(s) {}
  ~CStrStreamBuf() {}

  CT_INT_TYPE underflow();

  CT_CHAR_TYPE buffer[1024];
  auto_ptr<strstream> m_Stream;
};

END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ===========================================================================
*
* $Log$
* Revision 1.3  2002/05/06 03:28:48  vakatov
* OM/OM1 renaming
*
* Revision 1.2  2002/04/09 18:48:16  kimelman
* portability bugfixes: to compile on IRIX, sparc gcc
*
* Revision 1.1  2002/04/09 16:11:00  ucko
* Split CStrStreamBuf out into a common location.
*
*
* ===========================================================================
*/

#endif  /* STRSTREAMBUF__HPP */
