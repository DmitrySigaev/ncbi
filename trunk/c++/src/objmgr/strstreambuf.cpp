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
* ===========================================================================
*/

#include "strstreambuf.hpp"

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

CT_INT_TYPE CStrStreamBuf::underflow()
{
  size_t n = CIStream::Read(*m_Stream, buffer, sizeof(buffer));
  setg(buffer, buffer, buffer + n);
  return n == 0 ? CT_EOF : CT_TO_INT_TYPE(buffer[0]);
}

END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ===========================================================================
*
* $Log$
* Revision 1.2  2002/05/03 21:28:11  ucko
* Introduce T(Signed)SeqPos.
*
* Revision 1.1  2002/04/09 16:10:58  ucko
* Split CStrStreamBuf out into a common location.
*
*
* ===========================================================================
*/
