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
* Author: 
*   Eugene Vasilchenko
*
* File Description:
*   Standard CObject and CRef classes for reference counter based GC
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.5  2000/06/07 19:44:22  vasilche
* Removed unneeded THROWS declaration - they lead to encreased code size.
*
* Revision 1.4  2000/03/29 15:50:41  vasilche
* Added const version of CRef - CConstRef.
* CRef and CConstRef now accept classes inherited from CObject.
*
* Revision 1.3  2000/03/08 17:47:30  vasilche
* Simplified error check.
*
* Revision 1.2  2000/03/08 14:18:22  vasilche
* Fixed throws instructions.
*
* Revision 1.1  2000/03/07 14:03:15  vasilche
* Added CObject class as base for reference counted objects.
* Added CRef templace for reference to CObject descendant.
*
* 
* ===========================================================================
*/

#include <corelib/ncbiobj.hpp>

BEGIN_NCBI_SCOPE

CNullPointerError::CNullPointerError(void)
    THROWS_NONE
{
}

CNullPointerError::~CNullPointerError(void)
    THROWS_NONE
{
}

const char* CNullPointerError::what() const
    THROWS_NONE
{
    return "null pointer error";
}

CObject::~CObject(void)
{
    if ( Referenced() )
        throw runtime_error("delete referenced CObject object");
}

void CObject::RemoveLastReference(void) const
{
    switch ( m_Counter ) {
    case 0:
        // last reference removed
        delete const_cast<CObject*>(this);
        break;
    case eDoNotDelete:
        // last reference to static object removed -> do nothing
        break;
    default:
        m_Counter++;
        throw runtime_error("RemoveReference() without AddReference()");
    }
}

void CObject::ReleaseReference(void) const
{
    if ( m_Counter == 1 ) {
        m_Counter = 0;
    }
    else {
        throw runtime_error("Illegal CObject::ReleaseReference() call");
    }
}

END_NCBI_SCOPE
