#ifndef OBJLIST__HPP
#define OBJLIST__HPP

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
* Revision 1.8  2000/03/29 15:55:21  vasilche
* Added two versions of object info - CObjectInfo and CConstObjectInfo.
* Added generic iterators by class -
* 	CTypeIterator<class>, CTypeConstIterator<class>,
* 	CStdTypeIterator<type>, CStdTypeConstIterator<type>,
* 	CObjectsIterator and CObjectsConstIterator.
*
* Revision 1.7  2000/01/10 19:46:32  vasilche
* Fixed encoding/decoding of REAL type.
* Fixed encoding/decoding of StringStore.
* Fixed encoding/decoding of NULL type.
* Fixed error reporting.
* Reduced object map (only classes).
*
* Revision 1.6  1999/10/04 16:22:09  vasilche
* Fixed bug with old ASN.1 structures.
*
* Revision 1.5  1999/09/14 18:54:04  vasilche
* Fixed bugs detected by gcc & egcs.
* Removed unneeded includes.
*
* Revision 1.4  1999/06/30 16:04:30  vasilche
* Added support for old ASN.1 structures.
*
* Revision 1.3  1999/06/24 14:44:40  vasilche
* Added binary ASN.1 output.
*
* Revision 1.2  1999/06/10 21:06:39  vasilche
* Working binary output and almost working binary input.
*
* Revision 1.1  1999/06/07 19:30:16  vasilche
* More bug fixes
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <serial/serialdef.hpp>
#include <memory>
#include <map>
#include <list>
#include <functional>

BEGIN_NCBI_SCOPE

class CMemberId;
class CMemberInfo;
class COObjectList;

class CORootObjectInfo {
public:
    typedef int TIndex;

    CORootObjectInfo(TTypeInfo typeInfo = 0)
        : m_TypeInfo(typeInfo), m_Index(-1)
        {
        }

    bool IsWritten(void) const
        { return m_Index != TIndex(-1); }
    TIndex GetIndex(void) const
        { return m_Index; }

    TTypeInfo GetTypeInfo(void) const
        { return m_TypeInfo; }

private:
    friend class COObjectList;

    TTypeInfo m_TypeInfo;
    TIndex m_Index;
};

class COObjectInfo
{
public:
    typedef unsigned TIndex;

    COObjectInfo(void)
        : m_RootObject(0)
        { }
    COObjectInfo(const COObjectList& list,
                 TConstObjectPtr object, TTypeInfo typeInfo);

    TConstObjectPtr GetRootObject(void) const
        { return m_RootObject->first; }
    const CORootObjectInfo& GetRootObjectInfo(void) const
        { return m_RootObject->second; }

    bool IsMember(void) const
        {
            return false;
        }

    int GetMemberId(void) const
        {
            return -1;
        }

    void ToContainerObject(void)
        {
        }

private:
    friend class COObjectList;

    const pair<const TConstObjectPtr, CORootObjectInfo>* m_RootObject;
};

class COObjectList
{
public:
    typedef unsigned TIndex;

    COObjectList(void);
    ~COObjectList(void);

    // add object to object list
    // return true if object is new and should add all its members
    // return false if object was already added
    // may throw an exception if there is error in objects placements
    bool Add(TConstObjectPtr object, TTypeInfo typeInfo);

    void SetObject(COObjectInfo& info,
                   TConstObjectPtr object, TTypeInfo typeInfo) const;

protected:
    friend class CObjectOStream;

    void RegisterObject(const CORootObjectInfo& info);

    COObjectInfo GetObjectInfo(TConstObjectPtr object,
                               TTypeInfo typeInfo) const;


    // checks whether member:memberTypeInfo is member (direct or indirect) of
    // object:typeInfo
    // throws exception if not
    static bool CheckMember(TConstObjectPtr object, TTypeInfo typeInfo,
                            TConstObjectPtr member, TTypeInfo memberTypeInfo);

    // check that all objects marked as written
    void CheckAllWritten(void) const;

private:
    // we need reverse order map due to faster algorithm of lookup
    typedef map<TConstObjectPtr, CORootObjectInfo,
        greater<TConstObjectPtr> > TObjects;

    TObjects m_Objects;
    TIndex m_NextObjectIndex;
};

#include <serial/objlist.inl>

END_NCBI_SCOPE

#endif  /* OBJLIST__HPP */
