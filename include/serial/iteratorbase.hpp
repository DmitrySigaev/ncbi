#ifndef ITERATORBASE__HPP
#define ITERATORBASE__HPP

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
* Revision 1.1  2000/04/10 21:01:38  vasilche
* Fixed Erase for map/set.
* Added iteratorbase.hpp header for basic internal classes.
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <serial/object.hpp>

BEGIN_NCBI_SCOPE

class CChildIndex {
public:
    size_t m_Index;
    CRef<CObject> m_AnyIterator;
};

class CChildrenTypesIterator {
public:
    CChildrenTypesIterator(TTypeInfo parentType);

    TTypeInfo GetTypeInfo(void) const
        {
            return m_Parent;
        }
    
    const CChildIndex& GetIndex(void) const
        {
            return m_Index;
        }
    CChildIndex& GetIndex(void)
        {
            return m_Index;
        }

    operator bool(void) const;
    void Next(void);
    TTypeInfo GetChildType(void) const;

private:
    CChildIndex m_Index;
    TTypeInfo m_Parent;
};

class CConstChildrenIterator {
public:
    typedef CConstObjectInfo TObjectInfo;

    CConstChildrenIterator(const TObjectInfo& parent);

    const TObjectInfo& GetParent(void) const
        {
            return m_Parent;
        }
    TConstObjectPtr GetParentPtr(void) const
        {
            return m_Parent.GetObjectPtr();
        }

    const CChildIndex& GetIndex(void) const
        {
            return m_Index;
        }
    CChildIndex& GetIndex(void)
        {
            return m_Index;
        }

    operator bool(void) const;
    void Next(void);
    void GetChild(TObjectInfo& child) const;


private:
    TTypeInfo GetTypeInfo(void) const
        {
            return GetParent().GetTypeInfo();
        }

    // m_Index must be declared before m_Parent to ensure
    // proper destruction order
    CChildIndex m_Index;
    TObjectInfo m_Parent;
};

class CChildrenIterator {
public:
    typedef CObjectInfo TObjectInfo;

    CChildrenIterator(const TObjectInfo& parent);

    const TObjectInfo& GetParent(void) const
        {
            return m_Parent;
        }
    TObjectPtr GetParentPtr(void) const
        {
            return const_cast<TObjectPtr>(m_Parent.GetObjectPtr());
        }

    const CChildIndex& GetIndex(void) const
        {
            return m_Index;
        }
    CChildIndex& GetIndex(void)
        {
            return m_Index;
        }

    operator bool(void) const;
    void Next(void);
    void GetChild(TObjectInfo& child) const;
    void Erase(void);

private:
    TTypeInfo GetTypeInfo(void) const
        {
            return GetParent().GetTypeInfo();
        }

    // m_Index must be declared before m_Parent to ensure
    // proper destruction order
    CChildIndex m_Index;
    TObjectInfo m_Parent;
};

END_NCBI_SCOPE

#include <serial/iteratorbase.inl>

#endif  /* ITERATORBASE__HPP */
