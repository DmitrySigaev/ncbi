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
 * Authors: Dmitry Kazimirov, Rafael Sadyrov
 *
 * File Description:
 *   Implementation of the unified network blob storage API.
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/resource_info.hpp>
#include "netstorage_direct_object.hpp"


BEGIN_NCBI_SCOPE


using namespace NDirectNetStorageImpl;


template <class Derived, class Base>
Derived* Impl(CRef<Base, CNetComponentCounterLocker<Base> >& base_ref)
{
    return static_cast<Derived*>(base_ref.GetNonNullPointer());
}


CDirectNetStorageObject::CDirectNetStorageObject(EVoid)
    : CNetStorageObject(eVoid)
{
}


string CDirectNetStorageObject::Relocate(TNetStorageFlags flags)
{
    return Impl<CObj>(m_Impl)->Relocate(flags);
}


bool CDirectNetStorageObject::Exists()
{
    return Impl<CObj>(m_Impl)->Exists();
}


void CDirectNetStorageObject::Remove()
{
    Impl<CObj>(m_Impl)->Remove();
}


const CNetStorageObjectLoc& CDirectNetStorageObject::Locator()
{
    return Impl<CObj>(m_Impl)->Locator();
}


CDirectNetStorageObject::CDirectNetStorageObject(SNetStorageObjectImpl* impl)
    : CNetStorageObject(impl)
{}


struct SDirectNetStorageImpl : public SNetStorageImpl
{
    SDirectNetStorageImpl(const string& app_domain,
            TNetStorageFlags default_flags,
            CNetICacheClient::TInstance icache_client,
            CCompoundIDPool::TInstance compound_id_pool,
            const SFileTrackConfig& ft_config)
        : m_Context(new SContext(app_domain, icache_client, default_flags,
                    compound_id_pool, ft_config))
    {
    }

    CObj* OpenImpl(const string&);

    CNetStorageObject Create(TNetStorageFlags = 0);
    CNetStorageObject Open(const string&);
    string Relocate(const string&, TNetStorageFlags);
    bool Exists(const string&);
    void Remove(const string&);

    CObj* Create(TNetStorageFlags, const string&, Int8 = 0);

private:
#ifdef NCBI_GRID_XSITE_CONN_SUPPORT
    void AllowXSiteConnections() { m_Context->AllowXSiteConnections(); }
#endif

    CRef<SContext> m_Context;
};


CObj* SDirectNetStorageImpl::OpenImpl(const string& object_loc)
{
    ISelector::Ptr selector(ISelector::Create(m_Context, object_loc));
    return new CObj(selector);
}


CNetStorageObject SDirectNetStorageImpl::Create(TNetStorageFlags flags)
{
    ISelector::Ptr selector(ISelector::Create(m_Context, flags));
    return new CObj(selector);
}


CNetStorageObject SDirectNetStorageImpl::Open(const string& object_loc)
{
    return OpenImpl(object_loc);
}


string SDirectNetStorageImpl::Relocate(const string& object_loc,
        TNetStorageFlags flags)
{
    ISelector::Ptr selector(ISelector::Create(m_Context, object_loc));
    CRef<CObj> file(new CObj(selector));
    return file->Relocate(flags);
}


bool SDirectNetStorageImpl::Exists(const string& object_loc)
{
    ISelector::Ptr selector(ISelector::Create(m_Context, object_loc));
    CRef<CObj> net_file(new CObj(selector));
    return net_file->Exists();
}


void SDirectNetStorageImpl::Remove(const string& object_loc)
{
    ISelector::Ptr selector(ISelector::Create(m_Context, object_loc));
    CRef<CObj> net_file(new CObj(selector));
    net_file->Remove();
}


CObj* SDirectNetStorageImpl::Create(TNetStorageFlags flags,
        const string& service, Int8 id)
{
    ISelector::Ptr selector(ISelector::Create(m_Context, flags, service, id));

    // Server reports locator to the client before writing anything
    // So, object must choose location for writing here to make locator valid
    selector->SetLocator();

    return new CObj(selector);
}


struct SDirectNetStorageByKeyImpl : public SNetStorageByKeyImpl
{
    SDirectNetStorageByKeyImpl(const string& app_domain,
            TNetStorageFlags default_flags,
            CNetICacheClient::TInstance icache_client,
            CCompoundIDPool::TInstance compound_id_pool,
            const SFileTrackConfig& ft_config)
        : m_Context(new SContext(app_domain, icache_client, default_flags,
                    compound_id_pool, ft_config))
    {
        if (app_domain.empty()) {
            NCBI_THROW_FMT(CNetStorageException, eInvalidArg,
                    "Domain name cannot be empty.");
        }
    }

    CObj* OpenImpl(const string&, TNetStorageFlags = 0);

    CNetStorageObject Open(const string&, TNetStorageFlags = 0);
    string Relocate(const string&, TNetStorageFlags, TNetStorageFlags = 0);
    bool Exists(const string&, TNetStorageFlags = 0);
    void Remove(const string&, TNetStorageFlags = 0);

private:
    CRef<SContext> m_Context;
};


CNetStorageObject SDirectNetStorageByKeyImpl::Open(const string& key,
        TNetStorageFlags flags)
{
    return OpenImpl(key, flags);
}


CObj* SDirectNetStorageByKeyImpl::OpenImpl(const string& key,
        TNetStorageFlags flags)
{
    ISelector::Ptr selector(ISelector::Create(m_Context, flags, key));
    return new CObj(selector);
}


string SDirectNetStorageByKeyImpl::Relocate(const string& key,
        TNetStorageFlags flags, TNetStorageFlags old_flags)
{
    ISelector::Ptr selector(ISelector::Create(m_Context, old_flags, key));
    CRef<CObj> file(new CObj(selector));
    return file->Relocate(flags);
}


bool SDirectNetStorageByKeyImpl::Exists(const string& key, TNetStorageFlags flags)
{
    ISelector::Ptr selector(ISelector::Create(m_Context, flags, key));
    CRef<CObj> net_file(new CObj(selector));
    return net_file->Exists();
}


void SDirectNetStorageByKeyImpl::Remove(const string& key, TNetStorageFlags flags)
{
    ISelector::Ptr selector(ISelector::Create(m_Context, flags, key));
    CRef<CObj> net_file(new CObj(selector));
    net_file->Remove();
}

CDirectNetStorage::CDirectNetStorage(
        const SFileTrackConfig& ft_config,
        CNetICacheClient::TInstance icache_client,
        CCompoundIDPool::TInstance compound_id_pool,
        const string& app_domain,
        TNetStorageFlags default_flags)
    : CNetStorage(
            new SDirectNetStorageImpl(app_domain, default_flags, icache_client,
                compound_id_pool, ft_config))
{}


CDirectNetStorageObject CDirectNetStorage::Create(
        const string& service_name,
        Int8 object_id,
        TNetStorageFlags flags)
{
    return Impl<SDirectNetStorageImpl>(m_Impl)->Create(flags, service_name, object_id);
}


CDirectNetStorageObject CDirectNetStorage::Open(const string& object_loc)
{
    return Impl<SDirectNetStorageImpl>(m_Impl)->OpenImpl(object_loc);
}


CDirectNetStorageByKey::CDirectNetStorageByKey(
        const SFileTrackConfig& ft_config,
        CNetICacheClient::TInstance icache_client,
        CCompoundIDPool::TInstance compound_id_pool,
        const string& app_domain,
        TNetStorageFlags default_flags)
    : CNetStorageByKey(
            new SDirectNetStorageByKeyImpl(app_domain, default_flags, icache_client,
                compound_id_pool, ft_config))
{
}


CDirectNetStorageObject CDirectNetStorageByKey::Open(const string& key,
        TNetStorageFlags flags)
{
    return Impl<SDirectNetStorageByKeyImpl>(m_Impl)->OpenImpl(key, flags);
}


const string s_GetSection(const string& section)
{
    return section.empty() ? "filetrack" : section;
}


const STimeout s_GetDefaultTimeout()
{
    STimeout result;
    result.sec = 30;
    result.usec = 0;
    return result;
}


const string s_GetDecryptedKey(const string& key)
{
    if (CNcbiEncrypt::IsEncrypted(key)) {
        try {
            return CNcbiEncrypt::Decrypt(key);
        } catch (CException& e) {
            NCBI_RETHROW2(e, CRegistryException, eDecryptionFailed,
                    "Decryption failed for configuration value '" + key + "'.", 0);
        }
    }

    return key;
}


SFileTrackConfig::SFileTrackConfig(EVoid) :
    read_timeout(s_GetDefaultTimeout()),
    write_timeout(s_GetDefaultTimeout())
{
}


SFileTrackConfig::SFileTrackConfig(const IRegistry& reg, const string& section) :
    site(reg.GetString(s_GetSection(section), "site", "prod")),
    key(reg.GetEncryptedString(s_GetSection(section), "api_key",
                IRegistry::fPlaintextAllowed)),
    read_timeout(s_GetDefaultTimeout()),
    write_timeout(s_GetDefaultTimeout())
{
}


SFileTrackConfig::SFileTrackConfig(const string& s, const string& k) :
    site(s),
    key(s_GetDecryptedKey(k)),
    read_timeout(s_GetDefaultTimeout()),
    write_timeout(s_GetDefaultTimeout())
{
}


END_NCBI_SCOPE
