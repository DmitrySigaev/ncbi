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
* ---------------------------------------------------------------------------
* $Log$
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

#include <objects/objmgr/object_manager.hpp>
#include "data_source.hpp"
#include <objects/objmgr/scope.hpp>
#include <corelib/ncbithr.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

static CMutex s_OM_Mutex;

CObjectManager::CObjectManager(void)
    : m_IdMapper(new CSeq_id_Mapper)
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
    CRef<CDataLoaderFactory> p = &factory;
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
        return false;
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
        return false;
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
    set< CDataSource* >& sources)
{
    CMutexGuard guard(s_OM_Mutex);
    set< CDataSource* >::const_iterator itSource;
    // for each source in defaults
    for (itSource = m_setDefaultSource.begin();
        itSource != m_setDefaultSource.end(); ++itSource) {
        x_AddDataSource(sources, *itSource);
    }
}


void CObjectManager::AddDataLoader(
    set< CDataSource* >& sources, CDataLoader& loader)
{
    CMutexGuard guard(s_OM_Mutex);
    x_RegisterLoader(loader);
    x_AddDataSource(sources, m_mapLoaderToSource[&loader]);
}


void CObjectManager::AddDataLoader(
    set< CDataSource* >& sources, const string& loader_name)
{
    CDataLoader* loader = x_GetLoaderByName(loader_name);
    if (loader) {
        AddDataLoader(sources, *loader);
    }
    else {
        THROW1_TRACE(runtime_error,
            "CObjectManager::AddDataLoader() -- "
            "Data loader " + loader_name + " not found");
    }
}

void CObjectManager::AddTopLevelSeqEntry(
    set< CDataSource* >& sources, CSeq_entry& top_entry)
{
    CMutexGuard guard(s_OM_Mutex);
    x_RegisterTSE(top_entry);
    x_AddDataSource(sources, m_mapEntryToSource[ &top_entry]);
}


void CObjectManager::RemoveTopLevelSeqEntry(
    set< CDataSource* >& sources, CSeq_entry& top_entry)
{
    CMutexGuard guard(s_OM_Mutex);
    if (m_mapEntryToSource.find(&top_entry) != m_mapEntryToSource.end()) {
        CDataSource* source = m_mapEntryToSource[ &top_entry];

        source->DropTSE(top_entry);

        // Do not destroy datasource if it's not empty
        if ( !source->IsEmpty() )
            return;

        if (sources.find(source) != sources.end()) {
            sources.erase(source);
            x_ReleaseDataSource(source);
        }
    }
}


void CObjectManager::ReleaseDataSources(set< CDataSource* >& sources)
{
    CMutexGuard guard(s_OM_Mutex);
    set< CDataSource* >::iterator itSet;
    for (itSet = sources.begin(); itSet != sources.end(); ++itSet) {
        x_ReleaseDataSource(*itSet);
    }
    sources.clear();
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
    set< CDataSource* >& setSources, CDataSource* pSource) const
{
    _ASSERT(pSource);
    set< CDataSource* >::iterator itSrc = setSources.find(pSource);
    // if it is not there
    if (itSrc == setSources.end()) {
        // add
        pSource->AddReference();
        setSources.insert(pSource);
    }
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
    }
    pSource->RemoveReference();
}


CConstRef<CBioseq> CObjectManager::GetBioseq(const CSeq_id& id)
{
    CScope* pScope = *(m_setScope.begin());
    return &((pScope->GetBioseqHandle(id)).GetBioseq());
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
    ddc.Log("m_IdMapper", m_IdMapper.GetPointer(), depth);
}


END_SCOPE(objects)
END_NCBI_SCOPE
