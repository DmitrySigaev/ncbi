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
* Author: Eugene Vasilchenko
*
* File Description:
*   !!! PUT YOUR DESCRIPTION HERE !!!
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.1  1999/06/04 20:51:46  vasilche
* First compilable version of serialization.
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <serial/objlist.hpp>
#include <serial/classinfo.hpp>

BEGIN_NCBI_SCOPE

bool COObjectList::Add(TConstObjectPtr object, TTypeInfo typeInfo)
{
    // note that TObject have reverse sort order
    // just in case typedef in header file will be redefined:
    typedef map<TConstObjectPtr, CORootObjectInfo, greater<TConstObjectPtr> > TObject;

    TConstObjectPtr endOfObject = typeInfo->EndOf(object);

    TObjects::iterator before = m_Objects.lower_bound(object);
    // before->first <= object, (before-1)->first > object
    if ( before == m_Objects.end() ) {
        // there is not objects before then new one
        if ( m_Objects.empty() ) {
            // first object - just insert it
            m_Objects[object] = CORootObjectInfo(typeInfo);
            return true;
        }
    }
    else {
        TTypeInfo beforeTypeInfo = before->second.GetTypeInfo();
        TConstObjectPtr beforeObject = before->first;
        TConstObjectPtr beforeEndOfObject = beforeTypeInfo->EndOf(beforeObject);
        if ( object < beforeEndOfObject ) {
            // object and beforeObject may overlap
            if ( object > beforeObject || endOfObject < beforeEndOfObject ) {
                // object must be inside beforeObject
                if ( !CheckMember(beforeObject, beforeTypeInfo,
                                  object, typeInfo) ) {
                    ERR_POST("overlapping objects");
                    throw runtime_error("overlapping objects");
                }
                // in this case object completely inside beforeObject so
                // we'll not check anymore
                return false;
            }
            if ( endOfObject == beforeEndOfObject ) {
                // special case - object and beforeObject use the same memory
                // trivial check if types are the same
                if ( typeInfo == beforeTypeInfo ) {
                    // it's ok this object is already in map
                    return false;
                }
                // check who is owner
                if ( CheckMember(beforeObject, beforeTypeInfo,
                                 object, typeInfo) )
                    // good beforeObject is owner of object
                    return false;
                if ( !CheckMember(object, typeInfo,
                                  beforeObject, beforeTypeInfo) ) {
                    ERR_POST("overlapping objects");
                    throw runtime_error("overlapping objects");
                }
                // ok object is owner of beforeObject
                if ( before->second.IsWritten() ) {
                    ERR_POST("member already written");
                    throw runtime_error("member already written");
                }
                // lets replace it
                before->second = CORootObjectInfo(typeInfo);
                return true;
            }
            // here object == beforeObject && endOfObject > beforeEndOfObject
            // so beforeObject must be inside object
            // increase before to include it in following check
            ++before;
        }
    }
    // our object is smallest -> check for overlapping with first
    TObjects::iterator after = m_Objects.upper_bound(endOfObject);
    // after->first < endOfObject, (after-1) >= endOfObject
    for ( TObjects::iterator i = after; i != before; ++i ) {
        if ( !CheckMember(object, typeInfo, i->first, i->second.m_TypeInfo) ) {
            ERR_POST("overlapping objects");
            throw runtime_error("overlapping objects");
        }
        if ( i->second.IsWritten() ) {
            ERR_POST("member already written");
            throw runtime_error("member already written");
        }
    }
    // ok all objects from after to before are inside object
    // so replace them by object
    m_Objects.erase(after, before);
    m_Objects[object] = CORootObjectInfo(typeInfo);
    return true;
}

bool COObjectList::CheckMember(TConstObjectPtr owner, TTypeInfo ownerTypeInfo,
                               TConstObjectPtr member, TTypeInfo memberTypeInfo)
{
    if ( owner == member && ownerTypeInfo == memberTypeInfo )
        return true;
    ERR_POST("check is not complete");
    return false;
}

void COObjectList::CheckAllWritten(void) const
{
    for ( TObjects::const_iterator i = m_Objects.begin();
          i != m_Objects.end();
          ++i ) {
        if ( !i->second.IsWritten() ) {
            ERR_POST("object not written");
            throw runtime_error("object not written");
        }
    }
}

void COObjectList::SetObject(COObjectInfo& info,
                             TConstObjectPtr object, TTypeInfo typeInfo) const
{
    TObjects::const_iterator i = m_Objects.find(object);
    if ( i != m_Objects.end() && i->second.GetTypeInfo() == typeInfo ) {
        info.m_RootObject = &*i;
    }
    throw runtime_error("members not supported yet");
}

END_NCBI_SCOPE
