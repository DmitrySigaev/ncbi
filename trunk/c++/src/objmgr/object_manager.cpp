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
*
* File Description:
*           Object manager manages data objects,
*           provides them to Scopes when needed
*
*/

#include <objects/objmgr/object_manager.hpp>
#include <objects/objmgr/bioseq_handle.hpp>
#include <objects/objmgr/impl/data_source.hpp>
#include <objects/objmgr/scope.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objects/seq/Bioseq.hpp>
#include <corelib/ncbimtx.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

DEFINE_STATIC_MUTEX(s_OM_Mutex);

CObjectManager::CObjectManager(void)
{
  //LOG_POST("CObjectManager - new :" << this );
}


CObjectManager::~CObjectManager(void)
{
  //LOG_POST("~CObjectManager - delete " << this );
    // delete scopes
    if(!m_setScope.empty())
      {
        ERR_POST("Attempt to delete Object Manager with open scopes");
        while (!m_setScope.empty()) {
          // this will cause calling RegisterScope and changing m_setScope
          // be careful with data access synchronization
          (*m_setScope.begin())->x_DetachFromOM();
        }
      }
    CMutexGuard guard(s_OM_Mutex);
    // release data sources
    
    CDataSource* pSource;
    while (!m_mapLoaderToSource.empty()) {
        pSource = m_mapLoaderToSource.begin()->second;
        _VERIFY(pSource->ReferencedOnlyOnce());
        x_ReleaseDataSource(pSource);
    }
    while (!m_mapEntryToSource.empty()) {
        pSource = m_mapEntryToSource.begin()->second;
        _VERIFY(pSource->ReferencedOnlyOnce());
        x_ReleaseDataSource(pSource);
    }
    // LOG_POST("~CObjectManager - delete " << this << "  done");
}

/////////////////////////////////////////////////////////////////////////////
// configuration functions


void CObjectManager::RegisterDataLoader( CDataLoader& loader,
                                         EIsDefault   is_default)
{
    CMutexGuard guard(s_OM_Mutex);
    x_RegisterLoader(loader,is_default);
}


void CObjectManager::RegisterDataLoader(CDataLoaderFactory& factory,
                                        EIsDefault is_default)
{
    string loader_name = factory.GetName();
    _ASSERT(!loader_name.empty());
    // if already registered
    if (x_GetLoaderByName(loader_name)) {
        ERR_POST(Warning <<
            "CObjectManager::RegisterDataLoader: " <<
            "data loader " << loader_name << " already registered");
        return;
    }
    // in case factory was not put in a CRef container by the caller
    // it will be deleted here
    CRef<CDataLoaderFactory> p(&factory);
    // create loader
    CDataLoader* loader = factory.Create();
    // register
    x_RegisterLoader(*loader, is_default);
}


void CObjectManager::RegisterDataLoader(
    TFACTORY_AUTOCREATE factory, const string& loader_name,
    EIsDefault   is_default)
{
    _ASSERT(!loader_name.empty());
    // if already registered
    if (x_GetLoaderByName(loader_name)) {
        ERR_POST(Warning <<
            "CObjectManager::RegisterDataLoader: " <<
            "data loader " << loader_name << " already registered");
        return;
    }
    // create one
    CDataLoader* loader = CREATE_AUTOCREATE(CDataLoader,factory);
    loader->SetName(loader_name);
    // register
    x_RegisterLoader(*loader, is_default);
}


bool CObjectManager::RevokeDataLoader(CDataLoader& loader)
{
    CMutexGuard guard(s_OM_Mutex);
    // make sure it is registered
    string loader_name = loader.GetName();
    CDataLoader* my_loader = x_GetLoaderByName(loader_name);
    if (!my_loader || (my_loader != &loader)) {
        THROW1_TRACE(runtime_error,
            "CObjectManager::RevokeDataLoader() -- "
            "Data loader " + loader_name + " not registered");
    }
    CDataSource* source = m_mapLoaderToSource[&loader];
    if (!source->ReferencedOnlyOnce()) {
        // this means it is in use
        return false;
    }
    // remove from the maps
    m_mapLoaderToSource.erase(&loader);
    m_mapNameToLoader.erase(loader_name);
    m_setDefaultSource.erase(source);
    // release source
    source->RemoveReference();
    return true;
}


bool CObjectManager::RevokeDataLoader(const string& loader_name)
{
    CDataLoader* loader = x_GetLoaderByName(loader_name);
    // if not registered
    if (!loader)
    {
        THROW1_TRACE(runtime_error,
            "CObjectManager::RevokeDataLoader() -- "
            "Data loader " + loader_name + " not registered");
    }
    return RevokeDataLoader(*loader);
}


void CObjectManager::RegisterTopLevelSeqEntry(CSeq_entry& top_entry)
{
    CMutexGuard guard(s_OM_Mutex);
    x_RegisterTSE(top_entry);
}

/////////////////////////////////////////////////////////////////////////////
// functions for scopes

void CObjectManager::RegisterScope(CScope& scope)
{
    CMutexGuard guard(s_OM_Mutex);
    m_setScope.insert(&scope);
}


void CObjectManager::RevokeScope(CScope& scope)
{
    CMutexGuard guard(s_OM_Mutex);
    m_setScope.erase(&scope);
}


void CObjectManager::AcquireDefaultDataSources(
    CPriorityNode& sources,
    CPriorityNode::TPriority priority)
{
    CMutexGuard guard(s_OM_Mutex);
    set<CDataSource*>::const_iterator itSource;
    // for each source in defaults
    for (itSource = m_setDefaultSource.begin();
        itSource != m_setDefaultSource.end(); ++itSource) {
        x_AddDataSource(sources, *itSource, priority);
    }
}


void CObjectManager::AddDataLoader(
    CPriorityNode& sources, CDataLoader& loader,
    CPriorityNode::TPriority priority)
{
    CMutexGuard guard(s_OM_Mutex);
    x_RegisterLoader(loader);
    x_AddDataSource(sources, m_mapLoaderToSource[&loader], priority);
}


void CObjectManager::AddDataLoader(
    CPriorityNode& sources, const string& loader_name,
    CPriorityNode::TPriority priority)
{
    CDataLoader* loader = x_GetLoaderByName(loader_name);
    if (loader) {
        AddDataLoader(sources, *loader, priority);
    }
    else {
        THROW1_TRACE(runtime_error,
            "CObjectManager::AddDataLoader() -- "
            "Data loader " + loader_name + " not found");
    }
}

void CObjectManager::AddTopLevelSeqEntry(
    CPriorityNode& sources, CSeq_entry& top_entry,
    CPriorityNode::TPriority priority)
{
    CMutexGuard guard(s_OM_Mutex);
    x_RegisterTSE(top_entry);
    x_AddDataSource(sources, m_mapEntryToSource[ &top_entry], priority);
}


void CObjectManager::AddScope(
    CPriorityNode& sources, CScope& scope,
    CPriorityNode::TPriority priority)
{
    CMutexGuard guard(s_OM_Mutex);
    sources.Insert(scope.m_setDataSrc, priority);
    for (CPriority_I it(scope.m_setDataSrc); it; ++it) {
        it->AddReference();
    }
}


void CObjectManager::RemoveTopLevelSeqEntry(
    CPriorityNode& sources, CSeq_entry& top_entry)
{
    CMutexGuard guard(s_OM_Mutex);
    if (m_mapEntryToSource.find(&top_entry) != m_mapEntryToSource.end()) {
        CDataSource* source = m_mapEntryToSource[ &top_entry];

        source->DropTSE(top_entry);

        // Do not destroy datasource if it's not empty
        if ( !source->IsEmpty() )
            return;

        if ( sources.Erase(*source) ) {
            x_ReleaseDataSource(source);
        }
    }
}


void CObjectManager::ReleaseDataSources(CPriorityNode& sources)
{
    CMutexGuard guard(s_OM_Mutex);
    CPriority_I itSet(sources);
    for (; itSet; ++itSet) {
        x_ReleaseDataSource(&(*itSet));
    }
    sources.Clear();
}

/////////////////////////////////////////////////////////////////////////////
// private functions


bool CObjectManager::x_RegisterLoader( CDataLoader& loader,EIsDefault   is_default)
{
    string loader_name = loader.GetName();
    _ASSERT(!loader_name.empty());

    // if already registered
    CDataLoader* my_loader = x_GetLoaderByName(loader_name);
    if (my_loader) {
        // must be the same object
        // there should be NO different loaders with the same name
        if (my_loader != &loader) {
            THROW1_TRACE(runtime_error,
                "CObjectManager::RegisterDataLoader() -- "
                "Attempt to register different data loaders with the same name");
        }
        ERR_POST(Warning <<
            "CObjectManager::RegisterDataLoader() -- data loader " <<
            loader_name << " already registered");
        return false;
    }
    m_mapNameToLoader[ loader_name] = &loader;
    // create data source
    CDataSource* source = new CDataSource(loader, *this);
    source->DoDeleteThisObject();
    source->AddReference();
    m_mapLoaderToSource[&loader] = source;
    if (is_default == eDefault) {
        m_setDefaultSource.insert(source);
    }
    return true;
}

void CObjectManager::x_RegisterTSE(CSeq_entry& top_entry)
{
    if (m_mapEntryToSource.find(&top_entry) == m_mapEntryToSource.end())
      {
        CDataSource *source = new CDataSource(top_entry, *this);
        source->DoDeleteThisObject();
        source->AddReference();
        m_mapEntryToSource[ &top_entry] = source ;
      }
}

CDataLoader* CObjectManager::x_GetLoaderByName(
    const string& loader_name) const
{
    map< string, CDataLoader* >::const_iterator itMap;
    itMap = m_mapNameToLoader.find(loader_name);
    if (itMap != m_mapNameToLoader.end()) {
        return (*itMap).second;
    }
    return 0;
}


void CObjectManager::x_AddDataSource(
    CPriorityNode& setSources, CDataSource* pSource,
    CPriorityNode::TPriority priority) const
{
    _ASSERT(pSource  &&  setSources.IsTree());
    if (setSources.Insert(CPriorityNode(*pSource), priority))
        pSource->AddReference();
}


void CObjectManager::x_ReleaseDataSource(CDataSource* pSource)
{
    if (pSource->ReferencedOnlyOnce()) {
      // LOG_POST("CObjectManager - trying to remove DS " << pSource);
        CDataLoader* pLoader = pSource->GetDataLoader();
        if (pLoader) {
            m_mapLoaderToSource.erase(pLoader);
            m_mapNameToLoader.erase(pLoader->GetName());
        }
        CSeq_entry* pEntry = pSource->GetTopEntry();
        if (pEntry) {
            m_mapEntryToSource.erase(pEntry);
        }
        m_setDefaultSource.erase(pSource);
        pSource->RemoveReference();
    }
    else {
        pSource->RemoveReference();
        // Destroy data source if it's linked to an entry and is not
        // referenced by any scope.
        if ( pSource->ReferencedOnlyOnce()  &&  !pSource->GetDataLoader() ) {
            // Release the last reference (kept by this OM) and destroy the
            // data source.
            x_ReleaseDataSource(pSource);
        }
    }
}


CConstRef<CBioseq> CObjectManager::GetBioseq(const CSeq_id& id)
{
    CScope* pScope = *(m_setScope.begin());
    return CConstRef<CBioseq>(&((pScope->GetBioseqHandle(id)).GetBioseq()));
}


void CObjectManager::DebugDump(CDebugDumpContext ddc, unsigned int depth) const
{
    ddc.SetFrame("CObjectManager");
    CObject::DebugDump( ddc, depth);

    if (depth == 0) {
        DebugDumpValue(ddc,"m_setDefaultSource.size()", m_setDefaultSource.size());
        DebugDumpValue(ddc,"m_mapNameToLoader.size()",  m_mapNameToLoader.size());
        DebugDumpValue(ddc,"m_mapLoaderToSource.size()",m_mapLoaderToSource.size());
        DebugDumpValue(ddc,"m_mapEntryToSource.size()", m_mapEntryToSource.size());
        DebugDumpValue(ddc,"m_setScope.size()",         m_setScope.size());
    } else {
        
        DebugDumpRangePtr(ddc,"m_setDefaultSource",
            m_setDefaultSource.begin(), m_setDefaultSource.end(), depth);
        DebugDumpPairsValuePtr(ddc,"m_mapNameToLoader",
            m_mapNameToLoader.begin(), m_mapNameToLoader.end(), depth);
        DebugDumpPairsPtrPtr(ddc,"m_mapLoaderToSource",
            m_mapLoaderToSource.begin(), m_mapLoaderToSource.end(), depth);
        DebugDumpPairsPtrPtr(ddc,"m_mapEntryToSource",
            m_mapEntryToSource.begin(), m_mapEntryToSource.end(), depth);
        DebugDumpRangePtr(ddc,"m_setScope",
            m_setScope.begin(), m_setScope.end(), depth);
    }
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.23  2003/05/20 15:44:37  vasilche
* Fixed interaction of CDataSource and CDataLoader in multithreaded app.
* Fixed some warnings on WorkShop.
* Added workaround for memory leak on WorkShop.
*
* Revision 1.22  2003/04/24 16:12:38  vasilche
* Object manager internal structures are splitted more straightforward.
* Removed excessive header dependencies.
*
* Revision 1.21  2003/04/09 16:04:32  grichenk
* SDataSourceRec replaced with CPriorityNode
* Added CScope::AddScope(scope, priority) to allow scope nesting
*
* Revision 1.20  2003/03/11 14:15:52  grichenk
* +Data-source priority
*
* Revision 1.19  2003/02/05 17:59:17  dicuccio
* Moved formerly private headers into include/objects/objmgr/impl
*
* Revision 1.18  2003/01/29 22:03:46  grichenk
* Use single static CSeq_id_Mapper instead of per-OM model.
*
* Revision 1.17  2002/11/08 21:07:15  ucko
* CConstRef<> now requires an explicit constructor.
*
* Revision 1.16  2002/11/04 21:29:12  grichenk
* Fixed usage of const CRef<> and CRef<> constructor
*
* Revision 1.15  2002/09/19 20:05:44  vasilche
* Safe initialization of static mutexes
*
* Revision 1.14  2002/07/12 18:33:23  grichenk
* Fixed the bug with querying destroyed datasources
*
* Revision 1.13  2002/07/10 21:11:36  grichenk
* Destroy any data source not linked to a loader
* and not used by any scope.
*
* Revision 1.12  2002/07/08 20:51:02  grichenk
* Moved log to the end of file
* Replaced static mutex (in CScope, CDataSource) with the mutex
* pool. Redesigned CDataSource data locking.
*
* Revision 1.11  2002/06/04 17:18:33  kimelman
* memory cleanup :  new/delete/Cref rearrangements
*
* Revision 1.10  2002/05/28 18:00:43  gouriano
* DebugDump added
*
* Revision 1.9  2002/05/06 03:28:47  vakatov
* OM/OM1 renaming
*
* Revision 1.8  2002/05/02 20:42:37  grichenk
* throw -> THROW1_TRACE
*
* Revision 1.7  2002/04/22 20:04:11  grichenk
* Fixed TSE dropping
*
* Revision 1.6  2002/02/21 19:27:06  grichenk
* Rearranged includes. Added scope history. Added searching for the
* best seq-id match in data sources and scopes. Updated tests.
*
* Revision 1.5  2002/01/29 17:45:21  grichenk
* Removed debug output
*
* Revision 1.4  2002/01/23 21:59:31  grichenk
* Redesigned seq-id handles and mapper
*
* Revision 1.3  2002/01/18 15:53:29  gouriano
* implemented RegisterTopLevelSeqEntry
*
* Revision 1.2  2002/01/16 16:25:58  gouriano
* restructured objmgr
*
* Revision 1.1  2002/01/11 19:06:21  gouriano
* restructured objmgr
*
*
* ===========================================================================
*/
