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
* Authors:
*           Andrei Gourianov
*           Aleksey Grichenko
*           Michael Kimelman
*           Denis Vakatov
*           Eugene Vasilchenko
*
* File Description:
*           Scope is top-level object available to a client.
*           Its purpose is to define a scope of visibility and reference
*           resolution and provide access to the bio sequence data
*
*/

#include <ncbi_pch.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/bioseq_handle.hpp>
#include <objmgr/seq_entry_handle.hpp>
#include <objmgr/seq_annot_handle.hpp>
#include <objmgr/bioseq_set_handle.hpp>
#include <objmgr/object_manager.hpp>
#include <objmgr/objmgr_exception.hpp>

#include <objmgr/impl/data_source.hpp>
#include <objmgr/impl/tse_info.hpp>
#include <objmgr/impl/scope_info.hpp>
#include <objmgr/impl/bioseq_info.hpp>
#include <objmgr/impl/bioseq_set_info.hpp>
#include <objmgr/impl/seq_annot_info.hpp>
#include <objmgr/impl/priority.hpp>
#include <objmgr/impl/synonyms.hpp>

#include <objects/seq/Bioseq.hpp>
#include <objects/seq/Delta_seq.hpp>
#include <objects/seq/Seq_literal.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Textseq_id.hpp>
#include <objects/seqset/Seq_entry.hpp>

#include <objmgr/impl/scope_impl.hpp>
#include <objmgr/impl/scope_transaction_impl.hpp>

#include <objmgr/seq_annot_ci.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


/////////////////////////////////////////////////////////////////////////////
//
//  CScope_Impl
//
/////////////////////////////////////////////////////////////////////////////


CScope_Impl::CScope_Impl(CObjectManager& objmgr)
    : m_HeapScope(0), m_ObjMgr(0), m_Transaction(NULL)
{
    TWriteLockGuard guard(m_ConfLock);
    x_AttachToOM(objmgr);
}


CScope_Impl::~CScope_Impl(void)
{
    TWriteLockGuard guard(m_ConfLock);
    x_DetachFromOM();
}


CScope& CScope_Impl::GetScope(void)
{
    _ASSERT(m_HeapScope);
    return *m_HeapScope;
}


void CScope_Impl::x_AttachToOM(CObjectManager& objmgr)
{
    _ASSERT(!m_ObjMgr);
    m_ObjMgr.Reset(&objmgr);
    m_ObjMgr->RegisterScope(*this);
}


void CScope_Impl::x_DetachFromOM(void)
{
    _ASSERT(m_ObjMgr);
    // Drop and release all TSEs
    ResetScope();
    m_ObjMgr->RevokeScope(*this);
    m_ObjMgr.Reset();
}


void CScope_Impl::AddDefaults(TPriority priority)
{
    CObjectManager::TDataSourcesLock ds_set;
    m_ObjMgr->AcquireDefaultDataSources(ds_set);

    TWriteLockGuard guard(m_ConfLock);
    NON_CONST_ITERATE( CObjectManager::TDataSourcesLock, it, ds_set ) {
        m_setDataSrc.Insert(*x_GetDSInfo(const_cast<CDataSource&>(**it)),
                            (priority == kPriority_NotSet) ?
                            (*it)->GetDefaultPriority() : priority);
    }
    x_ClearCacheOnNewData();
}


void CScope_Impl::AddDataLoader(const string& loader_name, TPriority priority)
{
    CRef<CDataSource> ds = m_ObjMgr->AcquireDataLoader(loader_name);

    TWriteLockGuard guard(m_ConfLock);
    m_setDataSrc.Insert(*x_GetDSInfo(*ds),
                        (priority == kPriority_NotSet) ?
                        ds->GetDefaultPriority() : priority);
    x_ClearCacheOnNewData();
}


void CScope_Impl::AddScope(CScope_Impl& scope, TPriority priority)
{
    TReadLockGuard src_guard(scope.m_ConfLock);
    CPriorityTree tree(*this, scope.m_setDataSrc);
    src_guard.Release();
    
    TWriteLockGuard guard(m_ConfLock);
    m_setDataSrc.Insert(tree, (priority == kPriority_NotSet) ? 9 : priority);
    x_ClearCacheOnNewData();
}


CSeq_entry_Handle CScope_Impl::AddSeq_entry(CSeq_entry& entry,
                                            TPriority priority)
{
    TWriteLockGuard guard(m_ConfLock);
    CRef<CDataSource_ScopeInfo> ds_info = GetNonSharedDS(priority);
    CTSE_Lock tse_lock = ds_info->GetDataSource().AddStaticTSE(entry);
    x_ClearCacheOnNewData();

    return CSeq_entry_Handle(*tse_lock, *ds_info->GetTSE_Lock(tse_lock));
}


CSeq_entry_Handle CScope_Impl::AddSharedSeq_entry(const CSeq_entry& entry,
                                                 TPriority priority)
{
    TWriteLockGuard guard(m_ConfLock);
    CRef<CDataSource> ds = m_ObjMgr->AcquireSharedSeq_entry(entry);
    CRef<CDataSource_ScopeInfo> ds_info = AddDS(ds, priority);
    CTSE_Lock tse_lock = ds->GetSharedTSE();
    _ASSERT(tse_lock->GetTSECore() == &entry);

    return CSeq_entry_Handle(*tse_lock, *ds_info->GetTSE_Lock(tse_lock));
}


CBioseq_Handle CScope_Impl::AddBioseq(CBioseq& bioseq,
                                      TPriority priority)
{
    TWriteLockGuard guard(m_ConfLock);
    CRef<CDataSource_ScopeInfo> ds_info = GetNonSharedDS(priority);
    CRef<CSeq_entry> entry(new CSeq_entry);
    entry->SetSeq(bioseq);
    CTSE_Lock tse_lock = ds_info->GetDataSource().AddStaticTSE(*entry);
    x_ClearCacheOnNewData();

    return x_GetBioseqHandle(tse_lock->GetSeq(),
                             *ds_info->GetTSE_Lock(tse_lock));
}


CBioseq_Handle CScope_Impl::AddSharedBioseq(const CBioseq& bioseq,
                                            TPriority priority)
{
    TWriteLockGuard guard(m_ConfLock);
    CRef<CDataSource> ds = m_ObjMgr->AcquireSharedBioseq(bioseq);
    CRef<CDataSource_ScopeInfo> ds_info = AddDS(ds, priority);
    CTSE_Lock tse_lock = ds->GetSharedTSE();
    _ASSERT(tse_lock->IsSeq() &&
            tse_lock->GetSeq().GetBioseqCore() == &bioseq);

    return x_GetBioseqHandle(tse_lock->GetSeq(),
                             *ds_info->GetTSE_Lock(tse_lock));
}


CSeq_annot_Handle CScope_Impl::AddSeq_annot(CSeq_annot& annot,
                                            TPriority priority)
{
    TWriteLockGuard guard(m_ConfLock);
    CRef<CDataSource_ScopeInfo> ds_info = GetNonSharedDS(priority);
    CRef<CSeq_entry> entry(new CSeq_entry);
    entry->SetSet().SetSeq_set(); // it's not optional
    entry->SetSet().SetAnnot().push_back(Ref(&annot));
    CTSE_Lock tse_lock = ds_info->GetDataSource().AddStaticTSE(*entry);
    x_ClearCacheOnNewData();

    return CSeq_annot_Handle(*tse_lock->GetSet().GetAnnot()[0],
                             *ds_info->GetTSE_Lock(tse_lock));
}


CSeq_annot_Handle CScope_Impl::AddSharedSeq_annot(const CSeq_annot& annot,
                                                  TPriority priority)
{
    TWriteLockGuard guard(m_ConfLock);
    CRef<CDataSource> ds = m_ObjMgr->AcquireSharedSeq_annot(annot);
    CRef<CDataSource_ScopeInfo> ds_info = AddDS(ds, priority);
    CTSE_Lock tse_lock = ds->GetSharedTSE();
    _ASSERT(tse_lock->IsSet() &&
            tse_lock->GetSet().IsSetAnnot() &&
            tse_lock->GetSet().GetAnnot().size() == 1 &&
            tse_lock->GetSet().GetAnnot()[0]->GetSeq_annotCore() == &annot);

    return CSeq_annot_Handle(*tse_lock->GetSet().GetAnnot()[0],
                             *ds_info->GetTSE_Lock(tse_lock));
}


void CScope_Impl::RemoveDataLoader(const string& name,
                                   EActionIfLocked action)
{
    CRef<CDataSource> ds(m_ObjMgr->AcquireDataLoader(name));
    TWriteLockGuard guard(m_ConfLock);
    TDSMap::iterator ds_it = m_DSMap.find(ds);
    if ( ds_it == m_DSMap.end() ) {
        NCBI_THROW(CObjMgrException, eFindFailed,
                   "CScope_Impl::RemoveDataLoader: "
                   "data loader not found in the scope");
    }
    CRef<CDataSource_ScopeInfo> ds_info = ds_it->second;
    if ( action != eRemoveIfLocked ) {
        // we need to process each TSE individually checking if it's unlocked
        CDataSource_ScopeInfo::TTSE_InfoMap tse_map;
        {{
            CDataSource_ScopeInfo::TTSE_InfoMapMutex::TReadLockGuard guard2
                (ds_info->GetTSE_InfoMapMutex());
            tse_map = ds_info->GetTSE_InfoMap();
        }}
        ITERATE( CDataSource_ScopeInfo::TTSE_InfoMap, tse_it, tse_map ) {
            try {
                tse_it->second.GetNCObject().RemoveFromHistory(eThrowIfLocked);
            }
            catch ( ... ) {
                x_ClearCacheOnRemoveData();
                throw;
            }
        }
    }
    _VERIFY(m_setDataSrc.Erase(*ds_info));
    _VERIFY(m_DSMap.erase(ds));
    ds.Reset();
    ds_info->DetachScope();
    x_ClearCacheOnRemoveData();
}


void CScope_Impl::RemoveTopLevelSeqEntry(CTSE_Handle tse)
{
    TWriteLockGuard guard(m_ConfLock);
    if ( !tse ) {
        NCBI_THROW(CObjMgrException, eInvalidHandle,
                "CScope_Impl::RemoveTopLevelSeqEntry: "
                "TSE not found in the scope");
    }
    CRef<CTSE_ScopeInfo> tse_info(&tse.x_GetScopeInfo());
    CRef<CDataSource_ScopeInfo> ds_info(&tse_info->GetDSInfo());
    CTSE_Lock tse_lock(tse_info->GetTSE_Lock());
    _ASSERT(tse_lock);
    if ( &ds_info->GetScopeImpl() != this ) {
        NCBI_THROW(CObjMgrException, eInvalidHandle,
                "CScope_Impl::RemoveTopLevelSeqEntry: "
                "TSE doesn't belong to the scope");
    }
    if ( ds_info->GetDataLoader() ) {
        NCBI_THROW(CObjMgrException, eInvalidHandle,
                "CScope_Impl::RemoveTopLevelSeqEntry: "
                "can not remove a loaded TSE");
    }
    tse_info->RemoveFromHistory(eRemoveIfLocked);
    _ASSERT(!tse_info->IsAttached());
    _ASSERT(!tse);
    if ( !ds_info->CanBeEdited() ) { // shared -> remove whole DS
        CRef<CDataSource> ds(&ds_info->GetDataSource());
        _VERIFY(m_setDataSrc.Erase(*ds_info));
        _VERIFY(m_DSMap.erase(ds));
        ds.Reset();
        ds_info->DetachScope();
    }
    else { // private -> remove TSE only
        ds_info->GetDataSource().DropStaticTSE
            (const_cast<CTSE_Info&>(*tse_lock));
        x_ClearCacheOnRemoveData();
    }
}


CSeq_entry_EditHandle
CScope_Impl::x_AttachEntry(const CBioseq_set_EditHandle& seqset,
                           CRef<CSeq_entry_Info> entry,
                           int index)
{
    TWriteLockGuard guard(m_ConfLock);

    _ASSERT(seqset);
    _ASSERT(entry);

    seqset.x_GetInfo().AddEntry(entry, index);

    x_ClearCacheOnNewData();

    return CSeq_entry_EditHandle(*entry, seqset.GetTSE_Handle());
}


void CScope_Impl::x_AttachEntry(const CBioseq_set_EditHandle& seqset,
                                const CSeq_entry_EditHandle& entry,
                                int index)
{
    TWriteLockGuard guard(m_ConfLock);

    _ASSERT(seqset);
    _ASSERT(entry.IsRemoved());
    _ASSERT(!entry);

    seqset.GetTSE_Handle().x_GetScopeInfo()
        .AddEntry(seqset.x_GetScopeInfo(), entry.x_GetScopeInfo(), index);

    x_ClearCacheOnNewData();

    _ASSERT(entry);
}


CBioseq_EditHandle
CScope_Impl::x_SelectSeq(const CSeq_entry_EditHandle& entry,
                         CRef<CBioseq_Info> bioseq)
{
    CBioseq_EditHandle ret;

    TWriteLockGuard guard(m_ConfLock);

    _ASSERT(entry);
    _ASSERT(entry.Which() == CSeq_entry::e_not_set);
    _ASSERT(bioseq);

    // duplicate bioseq info
    entry.x_GetInfo().SelectSeq(*bioseq);

    x_ClearCacheOnNewData();

    ret.m_Info = entry.x_GetScopeInfo().x_GetTSE_ScopeInfo()
        .GetBioseqLock(null, bioseq);
    x_UpdateHandleSeq_id(ret);
    return ret;
}


CBioseq_set_EditHandle
CScope_Impl::x_SelectSet(const CSeq_entry_EditHandle& entry,
                         CRef<CBioseq_set_Info> seqset)
{
    TWriteLockGuard guard(m_ConfLock);

    _ASSERT(entry);
    _ASSERT(entry.Which() == CSeq_entry::e_not_set);
    _ASSERT(seqset);

    // duplicate bioseq info
    entry.x_GetInfo().SelectSet(*seqset);

    x_ClearCacheOnNewData();

    return CBioseq_set_EditHandle(*seqset, entry.GetTSE_Handle());
}


void CScope_Impl::x_SelectSeq(const CSeq_entry_EditHandle& entry,
                              const CBioseq_EditHandle& bioseq)
{
    TWriteLockGuard guard(m_ConfLock);

    _ASSERT(entry);
    _ASSERT(entry.Which() == CSeq_entry::e_not_set);
    _ASSERT(bioseq.IsRemoved());
    _ASSERT(!bioseq);

    entry.GetTSE_Handle().x_GetScopeInfo()
        .SelectSeq(entry.x_GetScopeInfo(), bioseq.x_GetScopeInfo());

    x_ClearCacheOnNewData();

    _ASSERT(bioseq);
}


void CScope_Impl::x_SelectSet(const CSeq_entry_EditHandle& entry,
                              const CBioseq_set_EditHandle& seqset)
{
    TWriteLockGuard guard(m_ConfLock);

    _ASSERT(entry);
    _ASSERT(entry.Which() == CSeq_entry::e_not_set);
    _ASSERT(seqset.IsRemoved());
    _ASSERT(!seqset);

    entry.GetTSE_Handle().x_GetScopeInfo()
        .SelectSet(entry.x_GetScopeInfo(), seqset.x_GetScopeInfo());

    x_ClearCacheOnNewData();

    _ASSERT(seqset);
}


CSeq_annot_EditHandle
CScope_Impl::x_AttachAnnot(const CSeq_entry_EditHandle& entry,
                           CRef<CSeq_annot_Info> annot)
{
    TWriteLockGuard guard(m_ConfLock);

    _ASSERT(entry);
    _ASSERT(annot);

    entry.x_GetInfo().AddAnnot(annot);

    x_ClearAnnotCache();

    return CSeq_annot_EditHandle(*annot, entry.GetTSE_Handle());
}


void CScope_Impl::x_AttachAnnot(const CSeq_entry_EditHandle& entry,
                                const CSeq_annot_EditHandle& annot)
{
    TWriteLockGuard guard(m_ConfLock);

    _ASSERT(entry);
    _ASSERT(annot.IsRemoved());
    _ASSERT(!annot);

    entry.GetTSE_Handle().x_GetScopeInfo()
        .AddAnnot(entry.x_GetScopeInfo(), annot.x_GetScopeInfo());

    x_ClearAnnotCache();

    _ASSERT(annot);
}


#define CHECK_HANDLE(func, handle)                                     \
    if ( !handle ) {                                                   \
        NCBI_THROW(CObjMgrException, eInvalidHandle,                   \
                   "CScope_Impl::" #func ": null " #handle " handle"); \
    }

#define CHECK_REMOVED_HANDLE(func, handle)                             \
    if ( !handle.IsRemoved() ) {                                       \
        NCBI_THROW(CObjMgrException, eInvalidHandle,                   \
                   "CScope_Impl::" #func ": "                          \
                   #handle " handle is not removed");                  \
    }


CSeq_entry_EditHandle
CScope_Impl::AttachEntry(const CBioseq_set_EditHandle& seqset,
                         CSeq_entry& entry,
                         int index)
{
    return AttachEntry(seqset, Ref(new CSeq_entry_Info(entry)), index);
}

CSeq_entry_EditHandle
CScope_Impl::AttachEntry(const CBioseq_set_EditHandle& seqset,
                         CRef<CSeq_entry_Info> entry,
                         int index)
{
    CHECK_HANDLE(AttachEntry, seqset);
    _ASSERT(seqset);
    return x_AttachEntry(seqset,entry, index);
}

/*
CSeq_entry_EditHandle
CScope_Impl::CopyEntry(const CBioseq_set_EditHandle& seqset,
                       const CSeq_entry_Handle& entry,
                       int index)
{
    CHECK_HANDLE(CopyEntry, seqset);
    CHECK_HANDLE(CopyEntry, entry);
    _ASSERT(seqset);
    _ASSERT(entry);
    return x_AttachEntry(seqset,
                         Ref(new CSeq_entry_Info(entry.x_GetInfo(), 0)),
                         index);
}


CSeq_entry_EditHandle
CScope_Impl::TakeEntry(const CBioseq_set_EditHandle& seqset,
                       const CSeq_entry_EditHandle& entry,
                       int index)
{
    CHECK_HANDLE(TakeEntry, seqset);
    CHECK_HANDLE(TakeEntry, entry);
    _ASSERT(seqset);
    _ASSERT(entry);
    entry.Remove();
    return AttachEntry(seqset, entry, index);
}
*/

CSeq_entry_EditHandle
CScope_Impl::AttachEntry(const CBioseq_set_EditHandle& seqset,
                         const CSeq_entry_EditHandle& entry,
                         int index)
{
    CHECK_HANDLE(AttachEntry, seqset);
    CHECK_REMOVED_HANDLE(AttachEntry, entry);
    _ASSERT(seqset);
    _ASSERT(!entry);
    _ASSERT(entry.IsRemoved());
    x_AttachEntry(seqset, entry, index);
    _ASSERT(!entry.IsRemoved());
    _ASSERT(entry);
    return entry;
}


CBioseq_EditHandle CScope_Impl::SelectSeq(const CSeq_entry_EditHandle& entry,
                                          CBioseq& seq)
{
    return SelectSeq(entry, Ref(new CBioseq_Info(seq)));
    /*CHECK_HANDLE(SelectSeq, entry);
    _ASSERT(entry);
    return x_SelectSeq(entry, Ref(new CBioseq_Info(seq)));
    */
}
CBioseq_EditHandle CScope_Impl::SelectSeq(const CSeq_entry_EditHandle& entry,
                                          CRef<CBioseq_Info> seq)
{
    CHECK_HANDLE(SelectSeq, entry);
    _ASSERT(entry);
    return x_SelectSeq(entry, seq);
}

/*
CBioseq_EditHandle CScope_Impl::CopySeq(const CSeq_entry_EditHandle& entry,
                                        const CBioseq_Handle& seq)
{
    CHECK_HANDLE(CopySeq, entry);
    CHECK_HANDLE(CopySeq, seq);
    _ASSERT(entry);
    _ASSERT(seq);
    return x_SelectSeq(entry,
                       Ref(new CBioseq_Info(seq.x_GetInfo(), 0)));
}


CBioseq_EditHandle CScope_Impl::TakeSeq(const CSeq_entry_EditHandle& entry,
                                        const CBioseq_EditHandle& seq)
{
    CHECK_HANDLE(TakeSeq, entry);
    CHECK_HANDLE(TakeSeq, seq);
    _ASSERT(entry);
    _ASSERT(seq);
    seq.Remove();
    return SelectSeq(entry, seq);
}
*/

CBioseq_EditHandle CScope_Impl::SelectSeq(const CSeq_entry_EditHandle& entry,
                                          const CBioseq_EditHandle& seq)
{
    CHECK_HANDLE(SelectSeq, entry);
    CHECK_REMOVED_HANDLE(SelectSeq, seq);
    _ASSERT(entry);
    _ASSERT(seq.IsRemoved());
    _ASSERT(!seq);
    x_SelectSeq(entry, seq);
    _ASSERT(seq);
    return seq;
}


CBioseq_set_EditHandle
CScope_Impl::SelectSet(const CSeq_entry_EditHandle& entry,
                       CBioseq_set& seqset)
{
    return SelectSet(entry, Ref(new CBioseq_set_Info(seqset)));
    /*    CHECK_HANDLE(SelectSet, entry);
    _ASSERT(entry);
    return x_SelectSet(entry, Ref(new CBioseq_set_Info(seqset)));*/
}

CBioseq_set_EditHandle
CScope_Impl::SelectSet(const CSeq_entry_EditHandle& entry,
                       CRef<CBioseq_set_Info> seqset)
{
    CHECK_HANDLE(SelectSet, entry);
    _ASSERT(entry);
    return x_SelectSet(entry, seqset);
}

/*
CBioseq_set_EditHandle
CScope_Impl::CopySet(const CSeq_entry_EditHandle& entry,
                     const CBioseq_set_Handle& seqset)
{
    CHECK_HANDLE(CopySet, entry);
    CHECK_HANDLE(CopySet, seqset);
    _ASSERT(entry);
    _ASSERT(seqset);
    return x_SelectSet(entry,
                       Ref(new CBioseq_set_Info(seqset.x_GetInfo(), 0)));
}


CBioseq_set_EditHandle
CScope_Impl::TakeSet(const CSeq_entry_EditHandle& entry,
                     const CBioseq_set_EditHandle& seqset)
{
    CHECK_HANDLE(TakeSet, entry);
    CHECK_HANDLE(TakeSet, seqset);
    _ASSERT(entry);
    _ASSERT(seqset);
    seqset.Remove();
    return SelectSet(entry, seqset);
}
*/

CBioseq_set_EditHandle
CScope_Impl::SelectSet(const CSeq_entry_EditHandle& entry,
                       const CBioseq_set_EditHandle& seqset)
{
    CHECK_HANDLE(SelectSet, entry);
    CHECK_REMOVED_HANDLE(SelectSet, seqset);
    _ASSERT(entry);
    _ASSERT(seqset.IsRemoved());
    _ASSERT(!seqset);
    x_SelectSet(entry, seqset);
    _ASSERT(seqset);
    return seqset;
}


CSeq_annot_EditHandle
CScope_Impl::AttachAnnot(const CSeq_entry_EditHandle& entry,
                         CSeq_annot& annot)
{
    return AttachAnnot(entry, Ref(new CSeq_annot_Info(annot)));
    /*CHECK_HANDLE(AttachAnnot, entry);
    _ASSERT(entry);
    return x_AttachAnnot(entry, Ref(new CSeq_annot_Info(annot)));
    */
}

CSeq_annot_EditHandle
CScope_Impl::AttachAnnot(const CSeq_entry_EditHandle& entry,
                         CRef<CSeq_annot_Info> annot)
{
    CHECK_HANDLE(AttachAnnot, entry);
    _ASSERT(entry);
    return x_AttachAnnot(entry, annot);
}

/*
CSeq_annot_EditHandle
CScope_Impl::CopyAnnot(const CSeq_entry_EditHandle& entry,
                       const CSeq_annot_Handle& annot)
{
    CHECK_HANDLE(CopyAnnot, entry);
    CHECK_HANDLE(CopyAnnot, annot);
    _ASSERT(entry);
    _ASSERT(annot);
    return x_AttachAnnot(entry,
                         Ref(new CSeq_annot_Info(annot.x_GetInfo(), 0)));
}


CSeq_annot_EditHandle
CScope_Impl::TakeAnnot(const CSeq_entry_EditHandle& entry,
                       const CSeq_annot_EditHandle& annot)
{
    CHECK_HANDLE(TakeAnnot, entry);
    CHECK_HANDLE(TakeAnnot, annot);
    _ASSERT(entry);
    _ASSERT(annot);
    annot.Remove();
    return AttachAnnot(entry, annot);
}
*/

CSeq_annot_EditHandle
CScope_Impl::AttachAnnot(const CSeq_entry_EditHandle& entry,
                         const CSeq_annot_EditHandle& annot)
{
    CHECK_HANDLE(AttachAnnot, entry);
    CHECK_REMOVED_HANDLE(AttachAnnot, annot);
    _ASSERT(entry);
    _ASSERT(annot.IsRemoved());
    _ASSERT(!annot);
    x_AttachAnnot(entry, annot);
    _ASSERT(annot);
    return annot;
}


void CScope_Impl::x_ClearCacheOnNewData(const CTSE_ScopeInfo* replaced_tse)
{
    // Clear unresolved bioseq handles
    // Clear annot cache
    if ( !replaced_tse && !m_Seq_idMap.empty() ) {
        LOG_POST(Info <<
            "CScope_Impl: -- "
            "adding new data to a scope with non-empty history "
            "may cause the data to become inconsistent");
    }
    for ( TSeq_idMap::iterator it = m_Seq_idMap.begin();
          it != m_Seq_idMap.end(); ) {
        if ( it->second.m_Bioseq_Info ) {
            CBioseq_ScopeInfo& binfo = *it->second.m_Bioseq_Info;
            if ( binfo.HasBioseq() ) {
                if ( &binfo.x_GetTSE_ScopeInfo() == replaced_tse ) {
                    binfo.m_SynCache.Reset(); // break circular link
                    m_Seq_idMap.erase(it++);
                    continue;
                }
                binfo.m_BioseqAnnotRef_Info.Reset();
            }
            else {
                binfo.m_SynCache.Reset(); // break circular link
                it->second.m_Bioseq_Info.Reset(); // try to resolve again
            }
        }
        it->second.m_AllAnnotRef_Info.Reset();
        ++it;
    }
}


void CScope_Impl::x_ClearCacheOnRemoveData(void)
{
    // Clear removed bioseq handles
    for ( TSeq_idMap::iterator it = m_Seq_idMap.begin();
          it != m_Seq_idMap.end(); ) {
        it->second.m_AllAnnotRef_Info.Reset();
        if ( it->second.m_Bioseq_Info ) {
            CBioseq_ScopeInfo& binfo = *it->second.m_Bioseq_Info;
            binfo.m_BioseqAnnotRef_Info.Reset();
            if ( binfo.IsDetached() ) {
                binfo.m_SynCache.Reset();
                m_Seq_idMap.erase(it++);
                continue;
            }
        }
        ++it;
    }
}


void CScope_Impl::x_ClearAnnotCache(void)
{
    // Clear annot cache
    NON_CONST_ITERATE ( TSeq_idMap, it, m_Seq_idMap ) {
        if ( it->second.m_Bioseq_Info ) {
            CBioseq_ScopeInfo& binfo = *it->second.m_Bioseq_Info;
            binfo.m_BioseqAnnotRef_Info.Reset();
        }
        it->second.m_AllAnnotRef_Info.Reset();
    }
}


CBioseq_set_Handle CScope_Impl::GetBioseq_setHandle(const CBioseq_set& seqset)
{
    TReadLockGuard guard(m_ConfLock);
    TBioseq_set_Lock lock = x_GetBioseq_set_Lock(seqset);
    return CBioseq_set_Handle(*lock.first, *lock.second);
}


CSeq_entry_Handle CScope_Impl::GetSeq_entryHandle(const CSeq_entry& entry)
{
    TReadLockGuard guard(m_ConfLock);
    TSeq_entry_Lock lock = x_GetSeq_entry_Lock(entry);
    return CSeq_entry_Handle(*lock.first, *lock.second);
}


CSeq_entry_Handle CScope_Impl::GetSeq_entryHandle(const CTSE_Handle& tse)
{
    return CSeq_entry_Handle(tse.x_GetTSE_Info(), tse);
}


CSeq_annot_Handle CScope_Impl::GetSeq_annotHandle(const CSeq_annot& annot)
{
    TReadLockGuard guard(m_ConfLock);
    TSeq_annot_Lock lock = x_GetSeq_annot_Lock(annot);
    return CSeq_annot_Handle(*lock.first, *lock.second);
}


CBioseq_Handle CScope_Impl::GetBioseqHandle(const CBioseq& seq)
{
    CBioseq_Handle ret;
    {{
        TReadLockGuard guard(m_ConfLock);
        ret.m_Info = x_GetBioseq_Lock(seq);
        x_UpdateHandleSeq_id(ret);
    }}
    return ret;
}


CRef<CDataSource_ScopeInfo> CScope_Impl::AddDS(CRef<CDataSource> ds,
                                               TPriority priority)
{
    TWriteLockGuard guard(m_ConfLock);
    CRef<CDataSource_ScopeInfo> ds_info = x_GetDSInfo(*ds);
    m_setDataSrc.Insert(*ds_info,
                        (priority == kPriority_NotSet) ?
                        ds->GetDefaultPriority() : priority);
    x_ClearCacheOnNewData();
    return ds_info;
}


CRef<CDataSource_ScopeInfo>
CScope_Impl::GetNonSharedDS(TPriority priority)
{
    TWriteLockGuard guard(m_ConfLock);
    typedef CPriorityTree::TPriorityMap TMap;
    TMap& pmap = m_setDataSrc.GetTree();
    TMap::iterator iter = pmap.lower_bound(priority);
    while ( iter != pmap.end() && iter->first == priority ) {
        if ( iter->second.IsLeaf() && iter->second.GetLeaf().CanBeEdited() ) {
            return Ref(&iter->second.GetLeaf());
        }
        ++iter;
    }
    CRef<CDataSource> ds(new CDataSource);
    _ASSERT(ds->CanBeEdited());
    CRef<CDataSource_ScopeInfo> ds_info = x_GetDSInfo(*ds);
    _ASSERT(ds_info->CanBeEdited());
    pmap.insert(iter, TMap::value_type(priority, CPriorityNode(*ds_info)));
    return ds_info;
}


CRef<CDataSource_ScopeInfo>
CScope_Impl::AddDSBefore(CRef<CDataSource> ds,
                         CRef<CDataSource_ScopeInfo> ds2)
{
    TWriteLockGuard guard(m_ConfLock);
    CRef<CDataSource_ScopeInfo> ds_info = x_GetDSInfo(*ds);
    for (CPriority_I it(m_setDataSrc); it; ++it) {
        if ( &*it == ds2 ) {
            it.InsertBefore(*ds_info);
            x_ClearCacheOnNewData();
            return ds_info;
        }
    }
    NCBI_THROW(CObjMgrException, eOtherError,
               "CScope_Impl::AddDSBefore: ds2 is not attached");
}


CRef<CDataSource_ScopeInfo> CScope_Impl::x_GetDSInfo(CDataSource& ds)
{
    CRef<CDataSource_ScopeInfo>& slot = m_DSMap[Ref(&ds)];
    if ( !slot ) {
        slot = new CDataSource_ScopeInfo(*this, ds);
    }
    return slot;
}


CScope_Impl::TTSE_Lock CScope_Impl::x_GetTSE_Lock(const CSeq_entry& tse)
{
    for (CPriority_I it(m_setDataSrc); it; ++it) {
        TTSE_Lock lock = it->FindTSE_Lock(tse);
        if ( lock ) {
            return lock;
        }
    }
    NCBI_THROW(CObjMgrException, eFindFailed,
               "CScope_Impl::x_GetTSE_Lock: entry is not attached");
}


CScope_Impl::TSeq_entry_Lock
CScope_Impl::x_GetSeq_entry_Lock(const CSeq_entry& entry)
{
    for (CPriority_I it(m_setDataSrc); it; ++it) {
        TSeq_entry_Lock lock = it->FindSeq_entry_Lock(entry);
        if ( lock.first ) {
            return lock;
        }
    }
    NCBI_THROW(CObjMgrException, eFindFailed,
               "CScope_Impl::x_GetSeq_entry_Lock: entry is not attached");
}


CScope_Impl::TSeq_annot_Lock
CScope_Impl::x_GetSeq_annot_Lock(const CSeq_annot& annot)
{
    for (CPriority_I it(m_setDataSrc); it; ++it) {
        TSeq_annot_Lock lock = it->FindSeq_annot_Lock(annot);
        if ( lock.first ) {
            return lock;
        }
    }
    NCBI_THROW(CObjMgrException, eFindFailed,
               "CScope_Impl::x_GetSeq_annot_Lock: annot is not attached");
}


CScope_Impl::TBioseq_set_Lock
CScope_Impl::x_GetBioseq_set_Lock(const CBioseq_set& seqset)
{
    for (CPriority_I it(m_setDataSrc); it; ++it) {
        TBioseq_set_Lock lock = it->FindBioseq_set_Lock(seqset);
        if ( lock.first ) {
            return lock;
        }
    }
    NCBI_THROW(CObjMgrException, eFindFailed,
               "CScope_Impl::x_GetBioseq_set_Lock: "
               "bioseq set is not attached");
}


CScope_Impl::TBioseq_Lock
CScope_Impl::x_GetBioseq_Lock(const CBioseq& bioseq)
{
    for (CPriority_I it(m_setDataSrc); it; ++it) {
        TBioseq_Lock lock = it->FindBioseq_Lock(bioseq);
        if ( lock ) {
            return lock;
        }
    }
    NCBI_THROW(CObjMgrException, eFindFailed,
               "CScope_Impl::x_GetBioseq_Lock: bioseq is not attached");
}


CScope_Impl::TTSE_Lock
CScope_Impl::x_GetTSE_Lock(const CTSE_Lock& lock, CDataSource_ScopeInfo& ds)
{
    _ASSERT(&ds.GetScopeImpl() == this);
    return ds.GetTSE_Lock(lock);
}


CScope_Impl::TTSE_Lock
CScope_Impl::x_GetTSE_Lock(const CTSE_ScopeInfo& tse)
{
    _ASSERT(&tse.GetScopeImpl() == this);
    return CTSE_ScopeUserLock(const_cast<CTSE_ScopeInfo*>(&tse));
}


void CScope_Impl::RemoveEntry(const CSeq_entry_EditHandle& entry)
{
    entry.GetCompleteSeq_entry();
    if ( !entry.GetParentEntry() ) {
        CTSE_Handle tse = entry.GetTSE_Handle();
        // TODO entry.Reset();
        RemoveTopLevelSeqEntry(tse);
        return;
    }
    TWriteLockGuard guard(m_ConfLock);

    entry.GetTSE_Handle().x_GetScopeInfo().RemoveEntry(entry.x_GetScopeInfo());

    x_ClearCacheOnRemoveData();
}


void CScope_Impl::RemoveAnnot(const CSeq_annot_EditHandle& annot)
{
    TWriteLockGuard guard(m_ConfLock);

    annot.GetTSE_Handle().x_GetScopeInfo().RemoveAnnot(annot.x_GetScopeInfo());

    x_ClearAnnotCache();
}


void CScope_Impl::SelectNone(const CSeq_entry_EditHandle& entry)
{
    _ASSERT(entry);
    entry.GetCompleteSeq_entry();

    TWriteLockGuard guard(m_ConfLock);
    entry.GetTSE_Handle().x_GetScopeInfo().ResetEntry(entry.x_GetScopeInfo());

    x_ClearCacheOnRemoveData();
}


void CScope_Impl::RemoveBioseq(const CBioseq_EditHandle& seq)
{
    SelectNone(seq.GetParentEntry());
}


void CScope_Impl::RemoveBioseq_set(const CBioseq_set_EditHandle& seqset)
{
    SelectNone(seqset.GetParentEntry());
}


CScope_Impl::TSeq_idMapValue&
CScope_Impl::x_GetSeq_id_Info(const CSeq_id_Handle& id)
{
    TSeq_idMapLock::TWriteLockGuard guard(m_Seq_idMapLock);
    TSeq_idMap::iterator it = m_Seq_idMap.lower_bound(id);
    if ( it == m_Seq_idMap.end() || it->first != id ) {
        it = m_Seq_idMap.insert(it, TSeq_idMapValue(id, SSeq_id_ScopeInfo()));
    }
    return *it;
/*
    TSeq_idMap::iterator it;
    {{
        TSeq_idMapLock::TReadLockGuard guard(m_Seq_idMapLock);
        it = m_Seq_idMap.lower_bound(id);
        if ( it != m_Seq_idMap.end() && it->first == id ) {
            return *it;
        }
    }}
    {{
        TSeq_idMapLock::TWriteLockGuard guard(m_Seq_idMapLock);
        it = m_Seq_idMap.insert(it, TSeq_idMapValue(id, SSeq_id_ScopeInfo()));
        return *it;
    }}
*/
/*
    {{
        TWriteLockGuard guard(m_Seq_idMapLock);
        TSeq_idMap::iterator it = m_Seq_idMap.lower_bound(id);
        if ( it == m_Seq_idMap.end() || it->first != id ) {
            it = m_Seq_idMap.insert(it,
                                    TSeq_idMapValue(id, SSeq_id_ScopeInfo()));
        }
        return *it;
    }}
*/
/*
    {{
        TSeq_idMapLock::TReadLockGuard guard(m_Seq_idMapLock);
        TSeq_idMap::iterator it = m_Seq_idMap.lower_bound(id);
        if ( it != m_Seq_idMap.end() && it->first == id )
            return *it;
    }}
    {{
        TSeq_idMapLock::TWriteLockGuard guard(m_Seq_idMapLock);
        return *m_Seq_idMap.insert(
            TSeq_idMapValue(id, SSeq_id_ScopeInfo())).first;
    }}
*/
}


CScope_Impl::TSeq_idMapValue*
CScope_Impl::x_FindSeq_id_Info(const CSeq_id_Handle& id)
{
    TSeq_idMapLock::TReadLockGuard guard(m_Seq_idMapLock);
    TSeq_idMap::iterator it = m_Seq_idMap.lower_bound(id);
    if ( it != m_Seq_idMap.end() && it->first == id )
        return &*it;
    return 0;
}


CRef<CBioseq_ScopeInfo>
CScope_Impl::x_InitBioseq_Info(TSeq_idMapValue& info,
                               int get_flag,
                               SSeqMatch_Scope& match)
{
    if (get_flag != CScope::eGetBioseq_Resolved) {
        // Resolve only if the flag allows
        CInitGuard init(info.second.m_Bioseq_Info, m_MutexPool);
        if ( init ) {
            x_ResolveSeq_id(info, get_flag, match);
        }
    }
    if ( get_flag == CScope::eGetBioseq_All ) {
        _ASSERT(info.second.m_Bioseq_Info);
        _ASSERT(!info.second.m_Bioseq_Info->HasBioseq() ||
                &info.second.m_Bioseq_Info->x_GetScopeImpl() == this);
    }
    return info.second.m_Bioseq_Info;
}


bool CScope_Impl::x_InitBioseq_Info(TSeq_idMapValue& info,
                                    CBioseq_ScopeInfo& bioseq_info)
{
    _ASSERT(&bioseq_info.x_GetScopeImpl() == this);
    {{
        CInitGuard init(info.second.m_Bioseq_Info, m_MutexPool);
        if ( init ) {
            _ASSERT(!info.second.m_Bioseq_Info);
            info.second.m_Bioseq_Info.Reset(&bioseq_info);
            return true;
        }
    }}
    return info.second.m_Bioseq_Info.GetPointerOrNull() == &bioseq_info;
}


CRef<CBioseq_ScopeInfo>
CScope_Impl::x_GetBioseq_Info(const CSeq_id_Handle& id,
                              int get_flag,
                              SSeqMatch_Scope& match)
{
    return x_InitBioseq_Info(x_GetSeq_id_Info(id), get_flag, match);
}


CRef<CBioseq_ScopeInfo>
CScope_Impl::x_FindBioseq_Info(const CSeq_id_Handle& id,
                               int get_flag,
                               SSeqMatch_Scope& match)
{
    CRef<CBioseq_ScopeInfo> ret;
    TSeq_idMapValue* info = x_FindSeq_id_Info(id);
    if ( info ) {
        ret = x_InitBioseq_Info(*info, get_flag, match);
        if ( ret ) {
            _ASSERT(!ret->HasBioseq() || &ret->x_GetScopeImpl() == this);
        }
    }
    return ret;
}


CBioseq_Handle CScope_Impl::x_GetBioseqHandleFromTSE(const CSeq_id_Handle& id,
                                                     const CTSE_Handle& tse)
{
    CBioseq_Handle ret;
    TReadLockGuard rguard(m_ConfLock);
    SSeqMatch_Scope match;
    CRef<CBioseq_ScopeInfo> info =
        x_FindBioseq_Info(id, CScope::eGetBioseq_Loaded, match);
    if ( info ) {
        if ( info->HasBioseq() &&
             &info->x_GetTSE_ScopeInfo() == &tse.x_GetScopeInfo() ) {
            ret = CBioseq_Handle(id, *info);
        }
    }
    else {
        // new bioseq - try to find it in source TSE
        if ( tse.x_GetScopeInfo().ContainsMatchingBioseq(id) ) {
            ret = GetBioseqHandle(id, CScope::eGetBioseq_Loaded);
            if ( ret.GetTSE_Handle() != tse ) {
                ret.Reset();
            }
        }
    }
    return ret;
}


CBioseq_Handle CScope_Impl::GetBioseqHandle(const CSeq_id_Handle& id,
                                            int get_flag)
{
    CBioseq_Handle ret;
    if ( id )  {
        SSeqMatch_Scope match;
        CRef<CBioseq_ScopeInfo> info;
        TReadLockGuard rguard(m_ConfLock);
        info = x_GetBioseq_Info(id, get_flag, match);
        if ( info ) {
            ret.m_Handle_Seq_id = id;
            if ( info->HasBioseq() ) {
                ret.m_Info = info->GetLock(match.m_Bioseq);
            }
            else {
                ret.m_Info.Reset(info);
            }
        }
    }
    return ret;
}


CScope_Impl::TBioseqHandles CScope_Impl::GetBioseqHandles(const TIds& ids)
{
    // Keep locks to prevent cleanup of the loaded TSEs.
    typedef CDataSource_ScopeInfo::TSeqMatchMap TSeqMatchMap;
    TSeqMatchMap match_map;
    ITERATE(TIds, id, ids) {
        match_map[*id];
    }
    for (CPriority_I it(m_setDataSrc); it; ++it) {
        it->GetBlobs(match_map);
    }
    TBioseqHandles ret;
    ITERATE(TIds, id, ids) {
        TSeqMatchMap::iterator match = match_map.find(*id);
        if (match != match_map.end()  &&  match->second) {
            ret.push_back(GetBioseqHandle(match->first,
                CScope::eGetBioseq_Loaded));
        }
        else {
            TReadLockGuard rguard(m_ConfLock);
            TSeq_idMapValue& id_info = x_GetSeq_id_Info(*id);
            CInitGuard init(id_info.second.m_Bioseq_Info, m_MutexPool);
            if ( init ) {
                _ASSERT(!id_info.second.m_Bioseq_Info);
                id_info.second.m_Bioseq_Info.Reset(new CBioseq_ScopeInfo(
                    CBioseq_Handle::fState_no_data |
                    CBioseq_Handle::fState_not_found));
            }
            CBioseq_Handle handle;
            handle.m_Handle_Seq_id = *id;
            CRef<CBioseq_ScopeInfo> info = id_info.second.m_Bioseq_Info;
            handle.m_Info.Reset(info);
            ret.push_back(handle);
        }
    }
    return ret;
}


CRef<CDataSource_ScopeInfo>
CScope_Impl::GetEditDataSource(CDataSource_ScopeInfo& src_ds)
{
    if ( !src_ds.m_EditDS ) {
        TWriteLockGuard guard(m_ConfLock);
        if ( !src_ds.m_EditDS ) {
            CRef<CDataSource> ds(new CDataSource);
            _ASSERT(ds->CanBeEdited());
            src_ds.m_EditDS = AddDSBefore(ds, Ref(&src_ds));
            _ASSERT(src_ds.m_EditDS);
            _ASSERT(src_ds.m_EditDS->CanBeEdited());
        }
    }
    return src_ds.m_EditDS;
}


#define EDITHANDLE_IS_NEW 0

CTSE_Handle CScope_Impl::GetEditHandle(const CTSE_Handle& handle)
{
    _ASSERT(handle);
#if EDITHANDLE_IS_NEW

    _ASSERT(!handle.CanBeEdited());
    CTSE_ScopeInfo& src_scope_info = handle.x_GetScopeInfo();
    if ( src_scope_info.m_EditLock ) {
        return *src_scope_info.m_EditLock;
    }
    TWriteLockGuard guard(m_ConfLock);
    if ( src_scope_info.m_EditLock ) {
        return *src_scope_info.m_EditLock;
    }
    CDataSource_ScopeInfo& src_ds = src_scope_info.GetDSInfo();
    CRef<CDataSource_ScopeInfo> edit_ds = GetEditDataSource(src_ds);
    // load all missing information if split
    src_scope_info.m_TSE_Lock->GetCompleteSeq_entry();
    CRef<CTSE_Info> new_tse(new CTSE_Info(src_scope_info.m_TSE_Lock));
#if 0 && defined(_DEBUG)
    LOG_POST("CTSE_Info is copied, map.size()="<<
             new_tse->m_BaseTSE->m_ObjectCopyMap.size());
    LOG_POST(typeid(*new_tse->m_BaseTSE->m_BaseTSE).name() <<
             "(" << &*new_tse->m_BaseTSE->m_BaseTSE << ")" <<
             " -> " <<
             typeid(*new_tse).name() <<
             "(" << new_tse << ")");
    ITERATE ( TEditInfoMap, it, new_tse->m_BaseTSE->m_ObjectCopyMap ) {
        LOG_POST(typeid(*it->first).name() <<
                 "(" << it->first << ")" <<
                 " -> " <<
                 typeid(*it->second).name() <<
                 "(" << it->second << ")");
    }
#endif
    CTSE_Lock edit_tse_lock = edit_ds->GetDataSource().AddStaticTSE(new_tse);
    src_scope_info.m_EditLock = edit_ds->GetTSE_Lock(edit_tse_lock);
    x_ClearCacheOnNewData(&src_scope_info);
    return *src_scope_info.m_EditLock;

#else

    if ( handle.CanBeEdited() ) {
        return handle;
    }
    TWriteLockGuard guard(m_ConfLock);
    if ( handle.CanBeEdited() ) {
        return handle;
    }
    CTSE_ScopeInfo& scope_info = handle.x_GetScopeInfo();
    CRef<CDataSource_ScopeInfo> old_ds(&scope_info.GetDSInfo());
    CRef<CDataSource_ScopeInfo> new_ds = GetEditDataSource(*old_ds);
    // load all missing information if split
    //scope_info.m_TSE_Lock->GetCompleteSeq_entry();
    CRef<CTSE_Info> new_tse(new CTSE_Info(scope_info.m_TSE_Lock));
    CTSE_Lock new_tse_lock = new_ds->GetDataSource().AddStaticTSE(new_tse);
    scope_info.SetEditTSE(new_tse_lock, *new_ds,
                          new_tse_lock->m_BaseTSE->m_ObjectCopyMap);
    const_cast<CTSE_Info&>(*new_tse_lock).m_BaseTSE.reset();
    _ASSERT(handle.CanBeEdited());
    _ASSERT(!old_ds->CanBeEdited());

    CRef<CDataSource> ds(&old_ds->GetDataSource());
    if ( ds->GetSharedObject() ) {
        // remove old shared object
        _ASSERT(!ds->GetDataLoader());
        _VERIFY(m_setDataSrc.Erase(*old_ds));
        _VERIFY(m_DSMap.erase(ds));
        ds.Reset();
        old_ds->DetachScope();
    }
    return handle;

#endif
}


CBioseq_EditHandle CScope_Impl::GetEditHandle(const CBioseq_Handle& h)
{
    CHECK_HANDLE(GetEditHandle, h);

#if EDITHANDLE_IS_NEW

    if ( h.GetTSE_Handle().CanBeEdited() ) {
        // use original TSE
        return CBioseq_EditHandle(h);
    }
    
    CBioseq_EditHandle ret;

    CTSE_Handle edit_tse(GetEditHandle(h.GetTSE_Handle()));
    _ASSERT(edit_tse && edit_tse != h.GetTSE_Handle());
    const CTSE_Info& edit_tse_info = edit_tse.x_GetTSE_Info();

    // find corresponding info in edit_tse
    const TEditInfoMap& info_map = edit_tse_info.m_BaseTSE->m_ObjectCopyMap;
    TEditInfoMap::const_iterator iter =
        info_map.find(CConstRef<CObject>(&h.x_GetInfo()));
    CConstRef<CBioseq_Info> edit_info;
    if ( iter != info_map.end() ) {
        edit_info = dynamic_cast<const CBioseq_Info*>(&*iter->second);
    }

    if ( !edit_info || !edit_info->BelongsToTSE_Info(edit_tse_info) ) {
        NCBI_THROW(CObjMgrException, eModifyDataError,
                   "CScope::GetEditHandle: "
                   "Bioseq already removed from TSE");
    }
    
    ret.m_Handle_Seq_id = h.m_Handle_Seq_id;
    ret.m_Info = edit_tse.x_GetScopeInfo().GetBioseqLock(null, edit_info);
    x_UpdateHandleSeq_id(ret);
    return ret;

#else
    
    _VERIFY(GetEditHandle(h.GetTSE_Handle()) == h.GetTSE_Handle());
    _ASSERT(h.GetTSE_Handle().CanBeEdited());
    return CBioseq_EditHandle(h);

#endif
}


CSeq_entry_EditHandle CScope_Impl::GetEditHandle(const CSeq_entry_Handle& h)
{
    CHECK_HANDLE(GetEditHandle, h);

#if EDITHANDLE_IS_NEW

    if ( h.GetTSE_Handle().CanBeEdited() ) {
        // use original TSE
        return CSeq_entry_EditHandle(h);
    }
    
    CTSE_Handle edit_tse(GetEditHandle(h.GetTSE_Handle()));
    _ASSERT(edit_tse && edit_tse != h.GetTSE_Handle());
    const CTSE_Info& edit_tse_info = edit_tse.x_GetTSE_Info();

    // find corresponding info in edit_tse
    const TEditInfoMap& info_map = edit_tse_info.m_BaseTSE->m_ObjectCopyMap;
    TEditInfoMap::const_iterator iter =
        info_map.find(CConstRef<CObject>(&h.x_GetInfo()));
    CRef<CSeq_entry_Info> edit_info;
    if ( iter != info_map.end() ) {
        edit_info =
            dynamic_cast<CSeq_entry_Info*>(iter->second.GetNCPointer());
    }

    if ( !edit_info || !edit_info->BelongsToTSE_Info(edit_tse_info) ) {
        NCBI_THROW(CObjMgrException, eModifyDataError,
                   "CScope::GetEditHandle: "
                   "Seq-entry already removed from TSE");
    }
    
    return CSeq_entry_EditHandle(*edit_info, edit_tse);

#else
    
    _VERIFY(GetEditHandle(h.GetTSE_Handle()) == h.GetTSE_Handle());
    _ASSERT(h.GetTSE_Handle().CanBeEdited());
    return CSeq_entry_EditHandle(h);

#endif
}


CSeq_annot_EditHandle CScope_Impl::GetEditHandle(const CSeq_annot_Handle& h)
{
    CHECK_HANDLE(GetEditHandle, h);

#if EDITHANDLE_IS_NEW

    if ( h.GetTSE_Handle().CanBeEdited() ) {
        // use original TSE
        return CSeq_annot_EditHandle(h);
    }
    
    CTSE_Handle edit_tse(GetEditHandle(h.GetTSE_Handle()));
    _ASSERT(edit_tse && edit_tse != h.GetTSE_Handle());
    const CTSE_Info& edit_tse_info = edit_tse.x_GetTSE_Info();

    // find corresponding info in edit_tse
    const TEditInfoMap& info_map = edit_tse_info.m_BaseTSE->m_ObjectCopyMap;
    TEditInfoMap::const_iterator iter =
        info_map.find(CConstRef<CObject>(&h.x_GetInfo()));
    CRef<CSeq_annot_Info> edit_info;
    if ( iter != info_map.end() ) {
        edit_info =
            dynamic_cast<CSeq_annot_Info*>(iter->second.GetNCPointer());
    }

    if ( !edit_info || !edit_info->BelongsToTSE_Info(edit_tse_info) ) {
        NCBI_THROW(CObjMgrException, eModifyDataError,
                   "CScope::GetEditHandle: "
                   "Seq-annot already removed from TSE");
    }
    
    return CSeq_annot_EditHandle(*edit_info, edit_tse);

#else
    
    _VERIFY(GetEditHandle(h.GetTSE_Handle()) == h.GetTSE_Handle());
    _ASSERT(h.GetTSE_Handle().CanBeEdited());
    return CSeq_annot_EditHandle(h);

#endif
}


CBioseq_set_EditHandle CScope_Impl::GetEditHandle(const CBioseq_set_Handle& h)
{
    CHECK_HANDLE(GetEditHandle, h);

#if EDITHANDLE_IS_NEW

    if ( h.GetTSE_Handle().CanBeEdited() ) {
        // use original TSE
        return CBioseq_set_EditHandle(h);
    }
    
    CTSE_Handle edit_tse(GetEditHandle(h.GetTSE_Handle()));
    _ASSERT(edit_tse && edit_tse != h.GetTSE_Handle());
    const CTSE_Info& edit_tse_info = edit_tse.x_GetTSE_Info();

    // find corresponding info in edit_tse
    const TEditInfoMap& info_map = edit_tse_info.m_BaseTSE->m_ObjectCopyMap;
    TEditInfoMap::const_iterator iter =
        info_map.find(CConstRef<CObject>(&h.x_GetInfo()));
    CRef<CBioseq_set_Info> edit_info;
    if ( iter != info_map.end() ) {
        edit_info =
            dynamic_cast<CBioseq_set_Info*>(iter->second.GetNCPointer());
    }

    if ( !edit_info || !edit_info->BelongsToTSE_Info(edit_tse_info) ) {
        NCBI_THROW(CObjMgrException, eModifyDataError,
                   "CScope::GetEditHandle: "
                   "Bioseq-set already removed from TSE");
    }
    
    return CBioseq_set_EditHandle(*edit_info, edit_tse);

#else
    
    _VERIFY(GetEditHandle(h.GetTSE_Handle()) == h.GetTSE_Handle());
    _ASSERT(h.GetTSE_Handle().CanBeEdited());
    return CBioseq_set_EditHandle(h);

#endif
}


CBioseq_Handle
CScope_Impl::GetBioseqHandleFromTSE(const CSeq_id_Handle& id,
                                    const CTSE_Handle& tse)
{
    CBioseq_Handle ret;
    if ( tse ) {
        ret = x_GetBioseqHandleFromTSE(id, tse);
    }
    return ret;
}


CBioseq_Handle CScope_Impl::GetBioseqHandle(const CSeq_loc& loc, int get_flag)
{
    CBioseq_Handle bh;
    for ( CSeq_loc_CI citer (loc); citer; ++citer) {
        bh = GetBioseqHandle(CSeq_id_Handle::GetHandle(citer.GetSeq_id()),
                             get_flag);
        if ( bh ) {
            break;
        }
    }
    return bh;
}


CBioseq_Handle CScope_Impl::GetBioseqHandle(const CBioseq_Info& seq,
                                            const CTSE_Handle& tse)
{
    CBioseq_Handle ret;
    {{
        TReadLockGuard guard(m_ConfLock);
        ret = x_GetBioseqHandle(seq, tse);
    }}
    return ret;
}


CBioseq_Handle CScope_Impl::x_GetBioseqHandle(const CBioseq_Info& seq,
                                              const CTSE_Handle& tse)
{
    CBioseq_Handle ret;
    ret.m_Info = tse.x_GetScopeInfo().GetBioseqLock(null, ConstRef(&seq));
    x_UpdateHandleSeq_id(ret);
    return ret;
}


void CScope_Impl::x_UpdateHandleSeq_id(CBioseq_Handle& bh)
{
    if ( bh.m_Handle_Seq_id ) {
        return;
    }
    ITERATE ( CBioseq_Handle::TId, id, bh.GetId() ) {
        CBioseq_Handle bh2 = x_GetBioseqHandleFromTSE(*id, bh.GetTSE_Handle());
        if ( bh2 && &bh2.x_GetInfo() == &bh.x_GetInfo() ) {
            bh.m_Handle_Seq_id = *id;
            return;
        }
    }
}


SSeqMatch_Scope CScope_Impl::x_FindBioseqInfo(const CPriorityTree& tree,
                                              const CSeq_id_Handle& idh,
                                              int get_flag)
{
    SSeqMatch_Scope ret;
    // Process sub-tree
    TPriority last_priority = 0;
    ITERATE( CPriorityTree::TPriorityMap, mit, tree.GetTree() ) {
        // Search in all nodes of the same priority regardless
        // of previous results
        TPriority new_priority = mit->first;
        if ( new_priority != last_priority ) {
            // Don't process lower priority nodes if something
            // was found
            if ( ret ) {
                break;
            }
            last_priority = new_priority;
        }
        SSeqMatch_Scope new_ret = x_FindBioseqInfo(mit->second, idh, get_flag);
        if ( new_ret ) {
            _ASSERT(&new_ret.m_TSE_Lock->GetScopeImpl() == this);
            if ( ret && ret.m_Bioseq != new_ret.m_Bioseq ) {
                ret.m_BlobState = CBioseq_Handle::fState_conflict;
                ret.m_Bioseq.Reset();
                return ret;
            }
            ret = new_ret;
        }
        else if (new_ret.m_BlobState != 0) {
            // Remember first blob state
            if (!ret  &&  ret.m_BlobState == 0) {
                ret = new_ret;
            }
        }
    }
    return ret;
}


SSeqMatch_Scope CScope_Impl::x_FindBioseqInfo(CDataSource_ScopeInfo& ds_info,
                                              const CSeq_id_Handle& idh,
                                              int get_flag)
{
    _ASSERT(&ds_info.GetScopeImpl() == this);
    try {
        return ds_info.BestResolve(idh, get_flag);
    }
    catch (CBlobStateException& e) {
        SSeqMatch_Scope ret;
        ret.m_BlobState = e.GetBlobState();
        return ret;
    }
}


SSeqMatch_Scope CScope_Impl::x_FindBioseqInfo(const CPriorityNode& node,
                                              const CSeq_id_Handle& idh,
                                              int get_flag)
{
    SSeqMatch_Scope ret;
    if ( node.IsTree() ) {
        // Process sub-tree
        ret = x_FindBioseqInfo(node.GetTree(), idh, get_flag);
    }
    else if ( node.IsLeaf() ) {
        CDataSource_ScopeInfo& ds_info = 
            const_cast<CDataSource_ScopeInfo&>(node.GetLeaf());
        ret = x_FindBioseqInfo(ds_info, idh, get_flag);
    }
    return ret;
}


void CScope_Impl::x_ResolveSeq_id(TSeq_idMapValue& id_info,
                                  int get_flag,
                                  SSeqMatch_Scope& match)
{
    // Use priority, do not scan all DSs - find the first one.
    // Protected by m_ConfLock in upper-level functions
    match = x_FindBioseqInfo(m_setDataSrc, id_info.first, get_flag);
    if ( !match ) {
        // Map unresoved ids only if loading was requested
        _ASSERT(!id_info.second.m_Bioseq_Info);
        if (get_flag == CScope::eGetBioseq_All) {
            id_info.second.m_Bioseq_Info.Reset
                (new CBioseq_ScopeInfo(match.m_BlobState |
                                       CBioseq_Handle::fState_no_data));
        }
    }
    else {
        CTSE_ScopeInfo& tse_info = *match.m_TSE_Lock;
        _ASSERT(&tse_info.GetScopeImpl() == this);
        CRef<CBioseq_ScopeInfo> bioseq = tse_info.GetBioseqInfo(match);
        _ASSERT(!id_info.second.m_Bioseq_Info);
        _ASSERT(&bioseq->x_GetScopeImpl() == this);
        id_info.second.m_Bioseq_Info = bioseq;
    }
}


CScope_Impl::TTSE_LockMatchSet
CScope_Impl::GetTSESetWithAnnots(const CSeq_id_Handle& idh)
{
    TTSE_LockMatchSet lock;

    {{
        TReadLockGuard rguard(m_ConfLock);
        TSeq_idMapValue& info = x_GetSeq_id_Info(idh);
        SSeqMatch_Scope seq_match;
        CRef<CBioseq_ScopeInfo> binfo =
            x_InitBioseq_Info(info, CScope::eGetBioseq_All, seq_match);
        if ( binfo->HasBioseq() ) {
            CInitGuard init(binfo->m_BioseqAnnotRef_Info, m_MutexPool);
            if ( init ) {
                CRef<CBioseq_ScopeInfo::TTSE_MatchSetObject> match
                    (new CBioseq_ScopeInfo::TTSE_MatchSetObject);
                x_GetTSESetWithBioseqAnnots(lock, match->GetData(), *binfo);
                binfo->m_BioseqAnnotRef_Info = match;
            }
            else {
                x_LockMatchSet(lock, *binfo->m_BioseqAnnotRef_Info);
            }
        }
        else {
            CInitGuard init(info.second.m_AllAnnotRef_Info, m_MutexPool);
            if ( init ) {
                CRef<CBioseq_ScopeInfo::TTSE_MatchSetObject> match
                    (new CBioseq_ScopeInfo::TTSE_MatchSetObject);
                CSeq_id_Handle::TMatches ids;
                idh.GetReverseMatchingHandles(ids);
                x_GetTSESetWithOrphanAnnots(lock, match->GetData(), ids, 0);
                info.second.m_AllAnnotRef_Info = match;
            }
            else {
                x_LockMatchSet(lock, *info.second.m_AllAnnotRef_Info);
            }
        }
    }}

    return lock;
}


CScope_Impl::TTSE_LockMatchSet
CScope_Impl::GetTSESetWithAnnots(const CBioseq_Handle& bh)
{
    TTSE_LockMatchSet lock;
    if ( bh ) {
        TReadLockGuard rguard(m_ConfLock);
        CRef<CBioseq_ScopeInfo> binfo
            (&const_cast<CBioseq_ScopeInfo&>(bh.x_GetScopeInfo()));
        
        _ASSERT(binfo->HasBioseq());
        
        CInitGuard init(binfo->m_BioseqAnnotRef_Info, m_MutexPool);
        if ( init ) {
            CRef<CBioseq_ScopeInfo::TTSE_MatchSetObject> match
                (new CBioseq_ScopeInfo::TTSE_MatchSetObject);
            x_GetTSESetWithBioseqAnnots(lock, match->GetData(), *binfo);
            binfo->m_BioseqAnnotRef_Info = match;
        }
        else {
            x_LockMatchSet(lock, *binfo->m_BioseqAnnotRef_Info);
        }
    }
    return lock;
}


void CScope_Impl::x_AddTSESetWithAnnots(TTSE_LockMatchSet& lock,
                                        TTSE_MatchSet& match,
                                        const TTSE_LockMatchSet_DS& add,
                                        CDataSource_ScopeInfo& ds_info)
{
    ITERATE( TTSE_LockMatchSet_DS, it, add ) {
        CTSE_Handle tse(*x_GetTSE_Lock(it->first, ds_info));
        CTSE_ScopeInfo& tse_info = tse.x_GetScopeInfo();
        match[Ref(&tse_info)].insert(it->second.begin(), it->second.end());
        lock[tse].insert(it->second.begin(), it->second.end());
    }
}


void CScope_Impl::x_GetTSESetWithOrphanAnnots(TTSE_LockMatchSet& lock,
                                              TTSE_MatchSet& match,
                                              const TSeq_idSet& ids,
                                              CDataSource_ScopeInfo* excl_ds)
{
    for (CPriority_I it(m_setDataSrc); it; ++it) {
        if ( &*it == excl_ds ) {
            // skip non-orphan annotations
            continue;
        }
        CDataSource& ds = it->GetDataSource();
        TTSE_LockMatchSet_DS ds_lock = ds.GetTSESetWithOrphanAnnots(ids);
        x_AddTSESetWithAnnots(lock, match, ds_lock, *it);
    }
}


void CScope_Impl::x_GetTSESetWithBioseqAnnots(TTSE_LockMatchSet& lock,
                                              TTSE_MatchSet& match,
                                              CBioseq_ScopeInfo& binfo)
{
    CDataSource_ScopeInfo& ds_info = binfo.x_GetTSE_ScopeInfo().GetDSInfo();
    CDataSource& ds = ds_info.GetDataSource();

    if ( m_setDataSrc.HasSeveralNodes() ) {
        // orphan annotations on all synonyms of Bioseq
        TSeq_idSet ids;
        // collect ids
        CConstRef<CSynonymsSet> syns = x_GetSynonyms(binfo);
        ITERATE ( CSynonymsSet, syn_it, *syns ) {
            // CSynonymsSet already contains all matching ids
            ids.insert(syns->GetSeq_id_Handle(syn_it));
        }
        // add orphan annots
        x_GetTSESetWithOrphanAnnots(lock, match, ids, &ds_info);
    }

    // datasource annotations on all ids of Bioseq
    // add external annots
    TBioseq_Lock bioseq = binfo.GetLock(null);
    TTSE_LockMatchSet_DS ds_lock =
        ds.GetTSESetWithBioseqAnnots(bioseq->GetObjectInfo(),
                                     bioseq->x_GetTSE_ScopeInfo().m_TSE_Lock);
    x_AddTSESetWithAnnots(lock, match, ds_lock, ds_info);
}


void CScope_Impl::x_LockMatchSet(TTSE_LockMatchSet& lock,
                                 const TTSE_MatchSet& match)
{
    ITERATE ( TTSE_MatchSet, it, match ) {
        lock[*x_GetTSE_Lock(*it->first)].insert(it->second.begin(),
                                                it->second.end());
    }
}

namespace {
    inline
    string sx_GetDSName(const SSeqMatch_Scope& match)
    {
        return match.m_TSE_Lock->GetDSInfo().GetDataSource().GetName();
    }
}


void CScope_Impl::RemoveFromHistory(CTSE_Handle tse)
{
    TWriteLockGuard guard(m_ConfLock);
    x_RemoveFromHistory(Ref(&tse.x_GetScopeInfo()), eRemoveIfLocked);
    _ASSERT(!tse);
}


void CScope_Impl::x_RemoveFromHistory(CRef<CTSE_ScopeInfo> tse_info,
                                      EActionIfLocked action)
{
    _ASSERT(tse_info->IsAttached());
    tse_info->RemoveFromHistory(action);
    if ( !tse_info->IsAttached() ) {
        // removed
        x_ClearCacheOnRemoveData();
    }
}


void CScope_Impl::ResetHistory(EActionIfLocked action)
{
    TWriteLockGuard guard(m_ConfLock);
    NON_CONST_ITERATE ( TDSMap, it, m_DSMap ) {
        it->second->ResetHistory(action);
    }
    x_ClearCacheOnRemoveData();
    //m_Seq_idMap.clear();
}


void CScope_Impl::ResetScope(void)
{
    TWriteLockGuard guard(m_ConfLock);

    while ( !m_DSMap.empty() ) {
        TDSMap::iterator iter = m_DSMap.begin();
        CRef<CDataSource_ScopeInfo> ds_info(iter->second);
        m_DSMap.erase(iter);
        ds_info->DetachScope();
    }
    m_setDataSrc.Clear();
    m_Seq_idMap.clear();
}


void CScope_Impl::x_PopulateBioseq_HandleSet(const CSeq_entry_Handle& seh,
                                             TBioseq_HandleSet& handles,
                                             CSeq_inst::EMol filter,
                                             TBioseqLevelFlag level)
{
    if ( seh ) {
        TReadLockGuard rguard(m_ConfLock);
        const CSeq_entry_Info& info = seh.x_GetInfo();
        CDataSource::TBioseq_InfoSet info_set;
        info.GetDataSource().GetBioseqs(info, info_set, filter, level);
        // Convert each bioseq info into bioseq handle
        ITERATE (CDataSource::TBioseq_InfoSet, iit, info_set) {
            CBioseq_Handle bh = x_GetBioseqHandle(**iit, seh.GetTSE_Handle());
            if ( bh ) {
                handles.push_back(bh);
            }
        }
    }
}


CScope_Impl::TIds CScope_Impl::GetIds(const CSeq_id_Handle& idh)
{
    TIds ret;
    TReadLockGuard rguard(m_ConfLock);
    SSeqMatch_Scope match;
    CRef<CBioseq_ScopeInfo> info =
        x_FindBioseq_Info(idh, CScope::eGetBioseq_Resolved, match);
    if ( info ) {
        if ( info->HasBioseq() ) {
            ret = info->GetIds();
        }
    }
    else {
        // Unknown bioseq, try to find in data sources
        for (CPriority_I it(m_setDataSrc); it; ++it) {
            it->GetDataSource().GetIds(idh, ret);
            if ( !ret.empty() ) {
                break;
            }
        }
    }
    return ret;
}


CConstRef<CSynonymsSet> CScope_Impl::GetSynonyms(const CSeq_id_Handle& id,
                                                 int get_flag)
{
    _ASSERT(id);
    TReadLockGuard rguard(m_ConfLock);
    SSeqMatch_Scope match;
    CRef<CBioseq_ScopeInfo> info = x_GetBioseq_Info(id, get_flag, match);
    if ( !info ) {
        return CConstRef<CSynonymsSet>(0);
    }
    return x_GetSynonyms(*info);
}


CConstRef<CSynonymsSet> CScope_Impl::GetSynonyms(const CBioseq_Handle& bh)
{
    if ( !bh ) {
        return CConstRef<CSynonymsSet>();
    }
    TReadLockGuard rguard(m_ConfLock);
    return x_GetSynonyms(const_cast<CBioseq_ScopeInfo&>(bh.x_GetScopeInfo()));
}


void CScope_Impl::x_AddSynonym(const CSeq_id_Handle& idh,
                               CSynonymsSet& syn_set,
                               CBioseq_ScopeInfo& info)
{
    // Check current ID for conflicts, add to the set.
    TSeq_idMapValue& seq_id_info = x_GetSeq_id_Info(idh);
    if ( x_InitBioseq_Info(seq_id_info, info) ) {
        // the same bioseq - add synonym
        if ( !syn_set.ContainsSynonym(seq_id_info.first) ) {
            syn_set.AddSynonym(&seq_id_info);
        }
    }
    else {
        CRef<CBioseq_ScopeInfo> info2 = seq_id_info.second.m_Bioseq_Info;
        _ASSERT(info2 != &info);
        LOG_POST(Warning << "CScope::GetSynonyms: "
                 "Bioseq["<<info.IdString()<<"]: "
                 "id "<<idh.AsString()<<" is resolved to another "
                 "Bioseq["<<info2->IdString()<<"]");
    }
}


CConstRef<CSynonymsSet>
CScope_Impl::x_GetSynonyms(CBioseq_ScopeInfo& info)
{
    {{
        CInitGuard init(info.m_SynCache, m_MutexPool);
        if ( init ) {
            // It's OK to use CRef, at least one copy should be kept
            // alive by the id cache (for the ID requested).
            CRef<CSynonymsSet> syn_set(new CSynonymsSet);
            //syn_set->AddSynonym(id);
            if ( info.HasBioseq() ) {
                ITERATE ( CBioseq_ScopeInfo::TIds, it, info.GetIds() ) {
                    if ( it->HaveReverseMatch() ) {
                        CSeq_id_Handle::TMatches hset;
                        it->GetReverseMatchingHandles(hset);
                        ITERATE ( CSeq_id_Handle::TMatches, mit, hset ) {
                            x_AddSynonym(*mit, *syn_set, info);
                        }
                    }
                    else {
                        x_AddSynonym(*it, *syn_set, info);
                    }
                }
            }
            info.m_SynCache = syn_set;
        }
    }}
    return info.m_SynCache;
}


void CScope_Impl::GetAllTSEs(TTSE_Handles& tses, int kind)
{
    TReadLockGuard rguard(m_ConfLock);
    for (CPriority_I it(m_setDataSrc); it; ++it) {
        if (it->GetDataLoader() &&  kind == CScope::eManualTSEs) {
            // Skip data sources with loaders
            continue;
        }
        CDataSource_ScopeInfo::TTSE_InfoMapMutex::TReadLockGuard
            guard(it->GetTSE_InfoMapMutex());
        ITERATE(CDataSource_ScopeInfo::TTSE_InfoMap, j, it->GetTSE_InfoMap()) {
            tses.push_back(CTSE_Handle(*x_GetTSE_Lock(*j->second)));
        }
    }
}


CDataSource* CScope_Impl::GetFirstLoaderSource(void)
{
    TReadLockGuard rguard(m_ConfLock);
    for (CPriority_I it(m_setDataSrc); it; ++it) {
        if ( it->GetDataLoader() ) {
            return &it->GetDataSource();
        }
    }
    return 0;
}


IScopeTransaction_Impl& CScope_Impl::GetTransaction()
{
    if( !m_Transaction )
        m_Transaction = CreateTransaction();
    return *m_Transaction;   
}


IScopeTransaction_Impl* CScope_Impl::CreateTransaction()
{
    /*    if ( m_Transaction ) {
        m_Transaction = new CScopeSubTransaction_Impl(*this);
    } else {
        m_Transaction = new CScopeTransaction_Impl(*this);
        }*/
    m_Transaction = new CScopeTransaction_Impl(*this, m_Transaction);
    return m_Transaction;   
}

void CScope_Impl::SetActiveTransaction(IScopeTransaction_Impl* transaction)
{
    if (m_Transaction && (transaction && !transaction->HasScope(*this))) {
        NCBI_THROW(CObjMgrException, eModifyDataError,
                   "CScope_Impl::AttachToTransaction: already attached to another transaction");
    }
    if (transaction)
        transaction->AddScope(*this);
    m_Transaction = transaction;
}

bool CScope_Impl::IsTransactionActive() const
{
    return m_Transaction;
}


END_SCOPE(objects)
END_NCBI_SCOPE
