#ifndef SCOPE_INFO__HPP
#define SCOPE_INFO__HPP

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
*     Structures used by CScope
*
*/

#include <corelib/ncbiobj.hpp>
#include <corelib/ncbimtx.hpp>

#include <objmgr/seq_id_handle.hpp>
#include <objmgr/impl/mutex_pool.hpp>

#include <set>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CDataSource;
class CDataLoader;
class CSeqMap;
class CScope;
class CBioseq_Info;
class CTSE_Info;
class CSynonymsSet;

struct NCBI_XOBJMGR_EXPORT CDataSource_ScopeInfo : public CObject
{
    typedef CRef<CDataSource> TDataSourceLock;

    CDataSource_ScopeInfo(CDataSource& ds);
    ~CDataSource_ScopeInfo(void);

    typedef CConstRef<CTSE_Info>                     TTSE_Lock;
    typedef set<TTSE_Lock>                           TTSE_LockSet;

    const TTSE_LockSet& GetTSESet(void) const;

    CDataSource& GetDataSource(void);
    const CDataSource& GetDataSource(void) const;
    CDataLoader* GetDataLoader(void);

protected:
    friend class CScope;

    CFastMutex& GetMutex(void);

    void AddTSE(const CTSE_Info& tse);
    void Reset(void);

private:
    CDataSource_ScopeInfo(const CDataSource_ScopeInfo&);
    const CDataSource_ScopeInfo& operator=(const CDataSource_ScopeInfo&);

    TDataSourceLock m_DataSource;
    TTSE_LockSet    m_TSE_LockSet;
    CFastMutex      m_Leaf_Mtx; // for m_TSE_LockSet
};


class NCBI_XOBJMGR_EXPORT CBioseq_ScopeInfo : public CObject
{
public:
    CBioseq_ScopeInfo(CScope* scope); // no sequence
    CBioseq_ScopeInfo(CScope* scope, const CConstRef<CBioseq_Info>& bioseq);

    CScope& GetScope(void) const;

    bool HasBioseq(void) const;
    const CBioseq_Info& GetBioseq_Info(void) const;

    const CTSE_Info& GetTSE_Info(void) const;
    CDataSource& GetDataSource(void) const;

private:
    friend class CScope;
    friend class CSeq_id_ScopeInfo;

    // owner scope
    CScope*                  m_Scope;

    // bioseq object if any
    // if none -> no bioseq for this Seq_id, but there might be annotations
    // if buiseq exists, m_TSE_Lock holds lock for TSE containing bioseq
    CConstRef<CBioseq_Info>  m_Bioseq_Info;
    CConstRef<CTSE_Info>     m_TSE_Lock;

    // caches synonyms of bioseq if any
    // all synonyms share the same CBioseq_ScopeInfo object
    CInitMutex<CSynonymsSet> m_SynCache;

private:
    CBioseq_ScopeInfo(const CBioseq_ScopeInfo& info);
    const CBioseq_ScopeInfo& operator=(const CBioseq_ScopeInfo& info);
};


struct NCBI_XOBJMGR_EXPORT SSeq_id_ScopeInfo
{
    typedef CConstRef<CTSE_Info>                     TTSE_Lock;
    typedef set<TTSE_Lock>                           TTSE_LockSet;
    typedef CObjectFor<TTSE_LockSet>                 TAnnotRefSet;

    // caches and locks other (not main) TSEs with annotations on this Seq-id
    CInitMutex<CBioseq_ScopeInfo> m_Bioseq_Info;

    // caches and locks other (not main) TSEs with annotations on this Seq-id
    CInitMutex<TAnnotRefSet>      m_AnnotRef_Info;
};


typedef CRef<CObject> TBlob_ID;

class NCBI_XOBJMGR_EXPORT CBlob_Info
{
public:
    CBlob_Info(void)
        : m_Blob_ID(0), m_Source(0) {}

    CBlob_Info(TBlob_ID bid, CDataSource& src)
        : m_Blob_ID(bid), m_Source(&src) {}

    ~CBlob_Info(void) {}

    CBlob_Info(const CBlob_Info& info)
        : m_Blob_ID(info.m_Blob_ID), m_Source(info.m_Source) {}

    CBlob_Info& operator =(const CBlob_Info& info)
        {
            if (&info != this) {
                m_Blob_ID = info.m_Blob_ID;
                m_Source = info.m_Source;
            }
            return *this;
        }

    operator bool(void)
        { return bool(m_Blob_ID)  &&  m_Source; }

    bool operator !(void)
        { return !m_Blob_ID  ||  !m_Source; }

    TBlob_ID     m_Blob_ID;
    CDataSource* m_Source;
};

/////////////////////////////////////////////////////////////////////////////
// Inline methods
/////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////
// CDataSource_ScopeInfo
/////////////////////////////////////////////////////////////////////////////


inline
CDataSource& CDataSource_ScopeInfo::GetDataSource(void)
{
    return *m_DataSource;
}


inline
const CDataSource& CDataSource_ScopeInfo::GetDataSource(void) const
{
    return *m_DataSource;
}


inline
CFastMutex& CDataSource_ScopeInfo::GetMutex(void)
{
    return m_Leaf_Mtx;
}

/////////////////////////////////////////////////////////////////////////////
// CBioseq_ScopeInfo
/////////////////////////////////////////////////////////////////////////////


inline
CScope& CBioseq_ScopeInfo::GetScope(void) const
{
    return *m_Scope;
}


inline
bool CBioseq_ScopeInfo::HasBioseq(void) const
{
    return m_Bioseq_Info;
}


inline
const CBioseq_Info& CBioseq_ScopeInfo::GetBioseq_Info(void) const
{
    return *m_Bioseq_Info;
}



END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.1  2003/06/19 18:23:45  vasilche
* Added several CXxx_ScopeInfo classes for CScope related information.
* CBioseq_Handle now uses reference to CBioseq_ScopeInfo.
* Some fine tuning of locking in CScope.
*
*
* ===========================================================================
*/

#endif  // SCOPE_INFO__HPP
