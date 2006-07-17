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
 * Author:  Maxim Didenko
 *
 *
 */


#include <ncbi_pch.hpp>
#include <corelib/blob_storage.hpp>
#include <corelib/ncbi_config.hpp>
#include <corelib/plugin_manager_store.hpp>


BEGIN_NCBI_SCOPE

IBlobStorage::~IBlobStorage() 
{
}
void IBlobStorage::DeleteStorage(void)
{
    NCBI_THROW(CBlobStorageException, eNotImplemented,
               "DeleteStorage operation is not implemented.");
}


IBlobStorageFactory::~IBlobStorageFactory()
{
}


CBlobStorageFactory::CBlobStorageFactory(const IRegistry& reg)
    : m_Params(CConfig::ConvertRegToTree(reg)), m_Owner(eTakeOwnership)
{
}
CBlobStorageFactory::CBlobStorageFactory(const TPluginManagerParamTree* params,
                                         EOwnership own)
    : m_Params(params), m_Owner(own)
{
}

CBlobStorageFactory::~CBlobStorageFactory()
{
    if (m_Owner == eTakeOwnership)
        delete m_Params;
    m_Params = NULL;
}

IBlobStorage* CBlobStorageFactory::CreateInstance()
{
    typedef CPluginManager<IBlobStorage> TCacheManager; 
    typedef CPluginManagerGetter<IBlobStorage> TCacheManagerStore;
 
    CRef<TCacheManager> cache_manager( TCacheManagerStore::Get() );
    //auto_ptr<TPluginManagerParamTree> params( MakeParamTree() );
    IBlobStorage* drv = NULL;
    
    _ASSERT( cache_manager );

    const TPluginManagerParamTree* storage_tree = 
            m_Params->FindSubNode("blob_storage");
    
    string driver_name = "netcache";
    if (storage_tree) {
        const TPluginManagerParamTree* driver_tree = 
            storage_tree->FindSubNode("driver");
        if (driver_tree  && !driver_tree->GetValue().value.empty()) {
            driver_name = driver_tree->GetValue().value;
            storage_tree = m_Params->FindSubNode(driver_name);
        }
    } else
        storage_tree = m_Params->FindSubNode("netcache_client");
        
    drv = cache_manager->CreateInstance(
            driver_name,
            NCBI_INTERFACE_VERSION(IBlobStorage),
            storage_tree
            );

    if (!drv)
        drv = new CBlobStorage_Null;
    
    return drv;
}




END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.3  2006/07/17 17:56:26  didenko
 * + DeleteStorage method
 *
 * Revision 1.2  2006/02/27 14:50:21  didenko
 * Redone an implementation of IBlobStorage interface based on NetCache as a plugin
 *
 * Revision 1.1  2005/12/20 17:13:34  didenko
 * Added new IBlobStorage interface
 *
 * ===========================================================================
 */

