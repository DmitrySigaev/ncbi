#ifndef GBLOADER_REQUEST_RESULT__HPP_INCLUDED
#define GBLOADER_REQUEST_RESULT__HPP_INCLUDED

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
*  ===========================================================================
*
*  Author: Eugene Vasilchenko
*
*  File Description:
*   Class for storing results of reader's request and thread synchronization
*
* ===========================================================================
*/

#include <corelib/ncbiobj.hpp>
#include <corelib/ncbimtx.hpp>
#include <objects/seq/seq_id_handle.hpp>
#include <objmgr/impl/mutex_pool.hpp>
#include <objmgr/impl/tse_loadlock.hpp>
#include <objtools/data_loaders/genbank/blob_id.hpp>

#include <map>
#include <set>
#include <string>
#include <vector>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CSeq_id;
class CSeq_id_Handle;
class CID2_Blob_Id;
class CBlob_id;
class CTSE_Info;
class CTimer;

class CLoadInfo;
class CLoadInfoLock;
class CLoadLock_Base;
class CReaderRequestResult;
class CReaderRequestConn;

/////////////////////////////////////////////////////////////////////////////
// resolved information classes
/////////////////////////////////////////////////////////////////////////////

class NCBI_XREADER_EXPORT CLoadInfo : public CObject
{
public:
    CLoadInfo(void);
    ~CLoadInfo(void);

    bool IsLoaded(void) const
        {
            return m_LoadLock;
        }

private:
    friend class CLoadInfoLock;
    friend class CLoadLock_Base;

    CLoadInfo(const CLoadInfo&);
    CLoadInfo& operator=(const CLoadInfo&);

    CInitMutex<CObject> m_LoadLock;
};


class NCBI_XREADER_EXPORT CLoadInfoSeq_ids : public CLoadInfo
{
public:
    typedef CSeq_id_Handle TSeq_id;
    typedef vector<TSeq_id> TSeq_ids;
    typedef TSeq_ids::const_iterator const_iterator;

    CLoadInfoSeq_ids(void);
    ~CLoadInfoSeq_ids(void);

    const_iterator begin(void) const
        {
            return m_Seq_ids.begin();
        }
    const_iterator end(void) const
        {
            return m_Seq_ids.end();
        }
    size_t size(void) const
        {
            return m_Seq_ids.size();
        }
    bool empty(void) const
        {
            return m_Seq_ids.empty();
        }

public:
    TSeq_ids    m_Seq_ids;
};


class CBlob_Info
{
public:
    typedef TBlobContentsMask TContentsMask;

    CBlob_Info(TContentsMask contents)
        : m_Contents(contents)
        {
        }

    TContentsMask GetContentsMask(void) const
        {
            return m_Contents;
        }

private:
    TContentsMask   m_Contents;
};


class NCBI_XREADER_EXPORT CLoadInfoBlob_ids : public CLoadInfo
{
public:
    typedef CSeq_id_Handle TSeq_id;
    typedef TSeq_id TKey;
    typedef CBlob_id TBlob_id;
    typedef CBlob_Info TBlob_Info;
    typedef map<TBlob_id, TBlob_Info> TBlob_ids;
    typedef TBlob_ids::const_iterator const_iterator;

    CLoadInfoBlob_ids(const TSeq_id& id);
    ~CLoadInfoBlob_ids(void);

    const TSeq_id& GetSeq_id(void) const
        {
            return m_Seq_id;
        }

    const_iterator begin(void) const
        {
            return m_Blob_ids.begin();
        }
    const_iterator end(void) const
        {
            return m_Blob_ids.end();
        }
    bool empty(void) const
        {
            return m_Blob_ids.empty();
        }

    TBlob_Info& AddBlob_id(const TBlob_id& id, const TBlob_Info& info);

public:
    TSeq_id     m_Seq_id;
    TBlob_ids   m_Blob_ids;

    // lock/refresh support
    double      m_RefreshTime;
};


/*
class NCBI_XREADER_EXPORT CLoadInfoBlob : public CLoadInfo
{
public:
    typedef CBlob_id TBlob_id;
    typedef TBlob_id TKey;

    CLoadInfoBlob(const TBlob_id& id);
    ~CLoadInfoBlob(void);

    const TBlob_id& GetBlob_id(void) const
        {
            return m_Blob_id;
        }

    CRef<CTSE_Info> GetTSE_Info(void) const;

    enum EBlobState {
        eState_normal,
        eState_suppressed_temp,
        eState_suppressed,
        eState_withdrawn
    };

    EBlobState GetBlobState(void) const
        {
            return m_Blob_State;
        }
    void SetBlobState(EBlobState state)
        {
            m_Blob_State = state;
        }
    bool IsNotLoadable(void) const
        {
            return GetBlobState() >= eState_withdrawn;
        }

public:
    TBlob_id m_Blob_id;

    EBlobState m_Blob_State;

    // typedefs for various info
    typedef set<CSeq_id_Handle> TSeq_ids;

    // blob
    // if null -> not loaded yet
    CRef<CTSE_Info> m_TSE_Info;

    // set of Seq-ids with sequences in this blob
    TSeq_ids        m_Seq_ids;
};
*/


/////////////////////////////////////////////////////////////////////////////
// resolved information locks
/////////////////////////////////////////////////////////////////////////////

class NCBI_XREADER_EXPORT CLoadInfoLock : public CObject
{
public:
    ~CLoadInfoLock(void);

    void SetLoaded(CObject* obj = 0);

protected:
    friend class CReaderRequestResult;

    CLoadInfoLock(CReaderRequestResult& owner,
                  const CRef<CLoadInfo>& info);

private:
    CReaderRequestResult&   m_Owner;
    CRef<CLoadInfo>         m_Info;
    CInitGuard              m_Guard;

private:
    CLoadInfoLock(const CLoadInfoLock&);
    CLoadInfoLock& operator=(const CLoadInfoLock&);
};


class NCBI_XREADER_EXPORT CLoadLock_Base
{
public:
    typedef CLoadInfo TInfo;
    typedef CLoadInfoLock TLock;
    typedef CReaderRequestResult TMutexSource;

    bool IsLoaded(void) const
        {
            return Get().IsLoaded();
        }
    void SetLoaded(CObject* obj = 0);
    operator bool(void) const
        {
            return IsLoaded();
        }
    bool operator!(void) const
        {
            return !IsLoaded();
        }

    TInfo& Get(void)
        {
            return *m_Info;
        }
    const TInfo& Get(void) const
        {
            return *m_Info;
        }

    const CRef<TLock> GetLock(void) const
        {
            return m_Lock;
        }

protected:
    void Lock(TInfo& info, TMutexSource& src);

private:
    CRef<TInfo> m_Info;
    CRef<TLock> m_Lock;
};


class NCBI_XREADER_EXPORT CLoadLockSeq_ids : public CLoadLock_Base
{
public:
    typedef CLoadInfoSeq_ids TInfo;

    CLoadLockSeq_ids(TMutexSource& src, const string& seq_id);
    CLoadLockSeq_ids(TMutexSource& src, const CSeq_id& seq_id);
    CLoadLockSeq_ids(TMutexSource& src, const CSeq_id_Handle& seq_id);

    TInfo& Get(void)
        {
            return static_cast<TInfo&>(CLoadLock_Base::Get());
        }
    const TInfo& Get(void) const
        {
            return static_cast<const TInfo&>(CLoadLock_Base::Get());
        }
    TInfo& operator*(void)
        {
            return Get();
        }
    TInfo* operator->(void)
        {
            return &Get();
        }
    const TInfo& operator*(void) const
        {
            return Get();
        }
    const TInfo* operator->(void) const
        {
            return &Get();
        }

    void AddSeq_id(const CSeq_id_Handle& seq_id);
    void AddSeq_id(const CSeq_id& seq_id);
};


class NCBI_XREADER_EXPORT CLoadLockBlob_ids : public CLoadLock_Base
{
public:
    typedef CLoadInfoBlob_ids TInfo;

    CLoadLockBlob_ids(TMutexSource& src, const CSeq_id_Handle& seq_id);
    CLoadLockBlob_ids(TMutexSource& src, const CSeq_id& seq_id);
    
    TInfo& Get(void)
        {
            return static_cast<TInfo&>(CLoadLock_Base::Get());
        }
    const TInfo& Get(void) const
        {
            return static_cast<const TInfo&>(CLoadLock_Base::Get());
        }
    TInfo& operator*(void)
        {
            return Get();
        }
    TInfo* operator->(void)
        {
            return &Get();
        }
    const TInfo& operator*(void) const
        {
            return Get();
        }
    const TInfo* operator->(void) const
        {
            return &Get();
        }

    CBlob_Info& AddBlob_id(const CBlob_id& blob_id,
                           const CBlob_Info& blob_info);

private:
    void x_Initialize(TMutexSource& src, const CSeq_id_Handle& seq_id);
};


class NCBI_XREADER_EXPORT CLoadLockBlob : public CTSE_LoadLock
{
public:
    CLoadLockBlob(CReaderRequestResult& src, const CBlob_id& blob_id);
};

/*
class NCBI_XREADER_EXPORT CLoadLockBlob : public CLoadLock_Base
{
public:
    typedef CLoadInfoBlob TInfo;

    CLoadLockBlob(TMutexSource& src, const CBlob_id& blob_id);
    
    TInfo& Get(void)
        {
            return static_cast<TInfo&>(CLoadLock_Base::Get());
        }
    const TInfo& Get(void) const
        {
            return static_cast<const TInfo&>(CLoadLock_Base::Get());
        }
    TInfo& operator*(void)
        {
            return Get();
        }
    TInfo* operator->(void)
        {
            return &Get();
        }
    const TInfo& operator*(void) const
        {
            return Get();
        }
    const TInfo* operator->(void) const
        {
            return &Get();
        }
};
*/


/////////////////////////////////////////////////////////////////////////////
// whole request lock
/////////////////////////////////////////////////////////////////////////////


class NCBI_XREADER_EXPORT CReaderRequestResult
{
public:
    CReaderRequestResult(void);
    virtual ~CReaderRequestResult(void);
    
    typedef string TKeySeq_ids;
    typedef CSeq_id_Handle TKeySeq_ids2;
    typedef CLoadInfoSeq_ids TInfoSeq_ids;
    typedef CSeq_id_Handle TKeyBlob_ids;
    typedef CLoadInfoBlob_ids TInfoBlob_ids;
    typedef CBlob_id TKeyBlob;
    //typedef CLoadInfoBlob TInfoBlob;
    typedef CLoadLockBlob TLockBlob;

    virtual CRef<TInfoSeq_ids>  GetInfoSeq_ids(const TKeySeq_ids& seq_id);
    virtual CRef<TInfoSeq_ids>  GetInfoSeq_ids(const TKeySeq_ids2& seq_id);
    virtual CRef<TInfoBlob_ids> GetInfoBlob_ids(const TKeyBlob_ids& seq_id);
    //virtual CRef<TInfoBlob>     GetInfoBlob(const TKeyBlob& blob_id);
    CTSE_LoadLock GetBlobLoadLock(const TKeyBlob& blob_id);
    virtual CTSE_LoadLock GetTSE_LoadLock(const TKeyBlob& blob_id);

    // load ResultBlob
    //virtual CRef<CTSE_Info> GetTSE_Info(const TLockBlob& blob);
    //CRef<CTSE_Info> GetTSE_Info(const TKeyBlob& blob_id);
    //virtual void SetTSE_Info(TLockBlob& blob, const CRef<CTSE_Info>& tse);
    //void SetTSE_Info(const TKeyBlob& blob_id, const CRef<CTSE_Info>& tse);

    //bool AddTSE_Lock(const TLockBlob& blob);
    //bool AddTSE_Lock(const TKeyBlob& blob_id);
    void AddTSE_Lock(const TTSE_Lock& lock);
    typedef set<TTSE_Lock> TTSE_LockSet;
    const TTSE_LockSet& GetTSE_LockSet(void) const
        {
            return m_TSE_LockSet;
        }
    void SaveLocksTo(TTSE_LockSet& locks);

    //virtual TTSE_Lock AddTSE(CRef<CTSE_Info> tse, const TKeyBlob& blob_id);
    //virtual TTSE_Lock LockTSE(CRef<CTSE_Info> tse);

    virtual operator CInitMutexPool&(void);
    CRef<CLoadInfoLock> GetLoadLock(const CRef<CLoadInfo>& info);

    typedef map<CRef<CLoadInfoLock>, CRef<CObject> > TLoadedSet;

    void SetLoaded(CLoadLock_Base& lock, CObject* object = 0);
    void ResetLoadedSet(void);
    const TLoadedSet& GetLoadedSet(void)
        {
            return m_LoadedSet;
        }
    void UpdateLoadedSet(const TLoadedSet& ls);
    void UpdateLoadedSet(void);

    typedef int TConn;

protected:
    friend class CReaderRequestConn;

    virtual TConn GetConn(void);
    virtual void ReleaseConn(void);

private:
    friend class CLoadInfoLock;

    void ReleaseLoadLock(const CRef<CLoadInfo>& info);
    
    typedef map<CRef<CLoadInfo>, CLoadInfoLock*> TLockMap;
    typedef map<CBlob_id, CTSE_LoadLock> TBlobLoadLocks;
    
    TLockMap        m_LockMap;
    TTSE_LockSet    m_TSE_LockSet;
    TLoadedSet      m_LoadedSet;
    TBlobLoadLocks  m_BlobLoadLocks;

private: // hide methods
    CReaderRequestResult(const CReaderRequestResult&);
    CReaderRequestResult& operator=(const CReaderRequestResult&);
};


class CReaderRequestConn
{
public:
    typedef CReaderRequestResult::TConn TConn;

    explicit CReaderRequestConn(CReaderRequestResult& result)
        : m_Result(result)
        {
        }
    ~CReaderRequestConn(void)
        {
            m_Result.ReleaseConn();
        }

    TConn GetConn(void) const
        {
            return m_Result.GetConn();
        }
    operator TConn(void) const
        {
            return GetConn();
        }

private:
    CReaderRequestConn(const CReaderRequestConn&);
    CReaderRequestConn& operator=(const CReaderRequestConn&);

    CReaderRequestResult& m_Result;
};


END_SCOPE(objects)
END_NCBI_SCOPE

#endif//GBLOADER_REQUEST_RESULT__HPP_INCLUDED
