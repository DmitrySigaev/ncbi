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
* Revision 1.7  2000/08/15 19:41:41  vasilche
* Changed refernce counter to allow detection of more errors.
*
* Revision 1.6  2000/06/16 16:29:51  vasilche
* Added SetCanDelete() method to allow to change CObject 'in heap' status immediately after creation.
*
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
#if _DEBUG
    TCounter counter = m_Counter;
    if ( ObjectStateIsInvalid(counter) ) {
        if ( counter == TCounter(eObjectDeletedValue) )
            THROW1_TRACE(runtime_error, "double deletion of CObject");
        else
            THROW1_TRACE(runtime_error, "deletion of corrupted CObject");
    }
    if ( ObjectStateReferenced(counter) )
        THROW1_TRACE(runtime_error, "deletion of referenced CObject");
#endif
    // mark object as deleted
    m_Counter = TCounter(eObjectDeletedValue);
}

void CObject::InvalidObject(void) const
{
    if ( m_Counter == TCounter(eObjectDeletedValue) )
        THROW1_TRACE(runtime_error, "using of deleted CObject");
    else
        THROW1_TRACE(runtime_error, "using of corrupted CObject");
}

void CObject::AddReferenceOverflow(void) const
{
    TCounter counter = m_Counter;
    if ( ObjectStateIsInvalid(counter) )
        InvalidObject();
    else
        THROW1_TRACE(runtime_error, "AddReferenceOverflow");
}

void CObject::CannotSetCanDelete(void) const
{
    TCounter counter = m_Counter;
    if ( ObjectStateIsInvalid(counter) )
        InvalidObject();
    else
        THROW1_TRACE(runtime_error, "SetCanDelete for used CObject");
}

void CObject::RemoveLastReference(void) const
{
    TCounter counter = m_Counter;
    if ( counter == TCounter(eObjectInHeap + eObjectCounterStep) ) {
        // last reference to heap object -> delete
        m_Counter = eObjectInHeap;
        delete const_cast<CObject*>(this);
    }
    else if ( counter == TCounter(eObjectInStack + eObjectCounterStep) ) {
        // last reference to stack object
        m_Counter = eObjectInStack;
    }
    else if ( ObjectStateIsInvalid(counter) )
        InvalidObject();
    else
        THROW1_TRACE(runtime_error, "RemoveReference of unreferenced CObject");
}

void CObject::ReleaseReference(void) const
{
    TCounter counter = m_Counter;
    if ( counter == TCounter(eObjectInHeap + eObjectCounterStep) ) {
        // release reference to object in heap
        m_Counter = eObjectInHeap;
    }
    else if ( ObjectStateIsInvalid(counter) )
        InvalidObject();
    else
        THROW1_TRACE(runtime_error, "Illegal ReleaseReference to CObject");
}

END_NCBI_SCOPE
