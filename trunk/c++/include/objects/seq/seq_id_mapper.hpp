#ifndef OBJECTS_OBJMGR___SEQ_ID_MAPPER__HPP
#define OBJECTS_OBJMGR___SEQ_ID_MAPPER__HPP

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
* Author: Aleksey Grichenko
*
* File Description:
*   Seq-id mapper for Object Manager
*
*/

#include <objects/objmgr/seq_id_handle.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <corelib/ncbiobj.hpp>
#include <corelib/ncbimtx.hpp>
#include <corelib/ncbi_limits.hpp>
#include <map>
#include <set>
#include <corelib/ncbi_safe_static.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


class CSeq_id;


////////////////////////////////////////////////////////////////////
//
//  CSeq_id_***_Tree::
//
//    Seq-id sub-type specific trees
//


typedef pair< CConstRef<CSeq_id>, TSeq_id_Key > TSeq_id_Info;


// Base class for seq-id type-specific trees
class NCBI_XOBJMGR_EXPORT CSeq_id_Which_Tree : public CObject
{
public:
    // 'ctors
    CSeq_id_Which_Tree(void) {}
    virtual ~CSeq_id_Which_Tree(void) {}

    typedef list<TSeq_id_Info> TSeq_id_MatchList;

    // Find exaclty the same seq-id
    virtual TSeq_id_Info FindEqual(const CSeq_id& id) const = 0;
    // Get the list of matching seq-id.
    // Not using list of handles since CSeq_id_Handle constructor
    // is private.
    virtual void FindMatch(const CSeq_id& id,
                           TSeq_id_MatchList& id_list) const = 0;
    virtual void FindMatchStr(string sid,
                              TSeq_id_MatchList& id_list) const = 0;

    // Map new seq-id
    virtual void AddSeq_idMapping(CSeq_id_Handle& handle) = 0;

    virtual bool IsBetterVersion(const CSeq_id_Handle& h1,
                                 const CSeq_id_Handle& h2) const;

    // Remove mapping for a given keys range
    void DropKeysRange(TSeq_id_Key first, TSeq_id_Key last);

    mutable CFastMutex m_TreeMutex; // Locked by CSeq_id_Mapper

protected:
    const CSeq_id& x_GetSeq_id(const CSeq_id_Handle& handle) const;
    TSeq_id_Key    x_GetKey(const CSeq_id_Handle& handle) const;

    void x_AddToKeyMap(const CSeq_id_Handle& handle);
    void x_RemoveFromKeyMap(TSeq_id_Key key);
    CSeq_id_Handle x_GetHandleByKey(TSeq_id_Key key) const;

    // Called by DropKeysRange() for each handle to remove it from
    // all maps. After calling x_DropHandle() the key is removed
    // from the keys map.
    virtual void x_DropHandle(const CSeq_id_Handle& handle) = 0;

    // Map for reverse lookups
    typedef map<TSeq_id_Key, CSeq_id_Handle> TKeyMap;
    TKeyMap m_KeyMap;
};


////////////////////////////////////////////////////////////////////
//
//  CSeq_id_Mapper::
//
//    Allows fast convertions between CSeq_id and CSeq_id_Handle,
//    including searching for multiple matches for a given seq-id.
//


typedef set<CSeq_id_Handle>                     TSeq_id_HandleSet;


class NCBI_XOBJMGR_EXPORT CSeq_id_Mapper : public CObject
{
public:
    static  CSeq_id_Mapper& GetSeq_id_Mapper(void);

    virtual ~CSeq_id_Mapper(void);

    // Get seq-id handle. Create new handle if not found and
    // do_not_create is false. Get only the exactly equal seq-id handle.
    CSeq_id_Handle GetHandle(const CSeq_id& id, bool do_not_create = false);
    // Get the list of matching handles, do not create new handles
    void GetMatchingHandles(const CSeq_id& id, TSeq_id_HandleSet& h_set);
    // Get the list of string-matching handles, do not create new handles
    void GetMatchingHandlesStr(string sid, TSeq_id_HandleSet& h_set);
    // Get seq-id for the given handle
    static const CSeq_id& GetSeq_id(const CSeq_id_Handle& handle);

private:
    CSeq_id_Mapper(void);
    friend class CSafeStaticRef<CSeq_id_Mapper>;

    // References to each handle must be tracked to re-use their values
    // Each CSeq_id_Handle locks itself in the constructor and
    // releases in the destructor.
    void AddHandleReference(const CSeq_id_Handle& handle);
    void ReleaseHandleReference(const CSeq_id_Handle& handle);
    bool IsBetter(const CSeq_id_Handle& h1, const CSeq_id_Handle& h2) const;
    const CSeq_id* x_GetSeq_id(TSeq_id_Key key) const;
    friend class CSeq_id_Handle;

    // Hide copy constructor and operator
    CSeq_id_Mapper(const CSeq_id_Mapper&);
    CSeq_id_Mapper& operator= (const CSeq_id_Mapper&);

    // Get next available key for CSeq_id_Handle
    TSeq_id_Key GetNextKey(void);

    // Table for id reference counters. The keys are grouped by
    // kKeyUsageTableSize elements. When a group is completely
    // unreferenced, it may be deleted and its keys re-used.
    typedef map<size_t, size_t> TKeyUsageTable;
    TKeyUsageTable m_KeyUsageTable;

    // Next available key value -- must be read through GetNextKey() only.
    TSeq_id_Key m_NextKey;

    // Some map entries may point to the same subtree (e.g. gb, dbj, emb).
    typedef map<CSeq_id::E_Choice, CRef<CSeq_id_Which_Tree> > TIdMap;
    typedef map<TSeq_id_Key, CRef<CSeq_id> >                  TKeyToIdMap;
    TIdMap      m_IdMap;
    TKeyToIdMap m_KeyMap;
    mutable CMutex  m_IdMapMutex;
};


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.16  2003/05/20 15:44:37  vasilche
* Fixed interaction of CDataSource and CDataLoader in multithreaded app.
* Fixed some warnings on WorkShop.
* Added workaround for memory leak on WorkShop.
*
* Revision 1.15  2003/04/24 16:12:37  vasilche
* Object manager internal structures are splitted more straightforward.
* Removed excessive header dependencies.
*
* Revision 1.14  2003/03/10 16:31:29  vasilche
* Moved implementation constant to .cpp file.
*
* Revision 1.13  2003/02/25 14:48:06  vasilche
* Added Win32 export modifier to object manager classes.
*
* Revision 1.12  2003/01/29 22:03:43  grichenk
* Use single static CSeq_id_Mapper instead of per-OM model.
*
* Revision 1.11  2002/10/03 01:58:27  ucko
* Move the definition of TSeq_id_Info above the declaration of
* CSeq_id_Which_Tree, which uses it.
*
* Revision 1.10  2002/10/02 21:26:53  ivanov
* A CSeq_id_Which_Tree class declaration moved from .cpp to .hpp to make
* KCC happy
*
* Revision 1.9  2002/09/19 20:05:44  vasilche
* Safe initialization of static mutexes
*
* Revision 1.8  2002/07/08 20:51:02  grichenk
* Moved log to the end of file
* Replaced static mutex (in CScope, CDataSource) with the mutex
* pool. Redesigned CDataSource data locking.
*
* Revision 1.7  2002/05/06 03:28:47  vakatov
* OM/OM1 renaming
*
* Revision 1.6  2002/04/22 20:03:48  grichenk
* Redesigned keys usage table to work in 64-bit mode
*
* Revision 1.5  2002/03/15 18:10:09  grichenk
* Removed CRef<CSeq_id> from CSeq_id_Handle, added
* key to seq-id map th CSeq_id_Mapper
*
* Revision 1.4  2002/02/21 19:27:06  grichenk
* Rearranged includes. Added scope history. Added searching for the
* best seq-id match in data sources and scopes. Updated tests.
*
* Revision 1.3  2002/02/12 19:41:42  grichenk
* Seq-id handles lock/unlock moved to CSeq_id_Handle 'ctors.
*
* Revision 1.2  2002/01/23 21:59:32  grichenk
* Redesigned seq-id handles and mapper
*
*
* ===========================================================================
*/

#endif  /* OBJECTS_OBJMGR___SEQ_ID_MAPPER__HPP */
