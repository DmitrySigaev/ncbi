#ifndef MISC_NETSTORAGE___NETSTORAGEIMPL__HPP
#define MISC_NETSTORAGE___NETSTORAGEIMPL__HPP

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
 * Author: Rafael Sadyrov
 *
 */

#include <connect/services/compound_id.hpp>
#include <connect/services/neticache_client.hpp>
#include <connect/services/netstorage.hpp>
#include "filetrack.hpp"


BEGIN_NCBI_SCOPE


struct SCombinedNetStorageConfig : SNetStorage::SConfig
{
    enum EMode {
        eDefault,
        eServerless,
    };

    EMode mode;
    SFileTrackConfig ft;

    SCombinedNetStorageConfig() : mode(eDefault) {}
    void ParseArg(const string&, const string&);
    void Validate(const string&);

    static SCombinedNetStorageConfig Build(const string& init_string)
    {
        return BuildImpl<SCombinedNetStorageConfig>(init_string);
    }

private:
    static EMode GetMode(const string&);
};


namespace NDirectNetStorageImpl
{

typedef CNetStorageObjectLoc TObjLoc;

class IState
{
public:
    virtual ~IState() {}

    virtual ERW_Result ReadImpl(void*, size_t, size_t*) = 0;
    virtual bool EofImpl() = 0;
    virtual ERW_Result WriteImpl(const void*, size_t, size_t*) = 0;
    virtual void CloseImpl() {}
    virtual void AbortImpl() { CloseImpl(); }
};

class ILocation
{
public:
    virtual ~ILocation() {}

    virtual IState* StartRead(void*, size_t, size_t*, ERW_Result*) = 0;
    virtual IState* StartWrite(const void*, size_t, size_t*, ERW_Result*) = 0;
    virtual Uint8 GetSizeImpl() = 0;
    virtual CNetStorageObjectInfo GetInfoImpl() = 0;
    virtual bool ExistsImpl() = 0;
    virtual ENetStorageRemoveResult RemoveImpl() = 0;
    virtual void SetExpirationImpl(const CTimeout&) = 0;

    virtual string FileTrack_PathImpl() = 0;

    typedef pair<string, string> TUserInfo;
    virtual TUserInfo GetUserInfoImpl() = 0;

    virtual bool IsSame(const ILocation* other) const = 0;

protected:
    template <class TLocation>
    static const TLocation* To(const ILocation* location)
    {
        return dynamic_cast<const TLocation*>(location);
    }
};

class ISelector
{
public:
    typedef auto_ptr<ISelector> Ptr;

    virtual ~ISelector() {}

    virtual ILocation* First() = 0;
    virtual ILocation* Next() = 0;
    virtual const TObjLoc& Locator() = 0;
    virtual void SetLocator() = 0;

    virtual ISelector* Clone(TNetStorageFlags) = 0;
};

struct SContext : CObject
{
    CNetICacheClientExt icache_client;
    SFileTrackAPI filetrack_api;
    CCompoundIDPool compound_id_pool;
    TNetStorageFlags default_flags = 0;
    string app_domain;

    SContext(const SCombinedNetStorageConfig&, TNetStorageFlags);
    SContext(const string&, CNetICacheClient::TInstance,
            CCompoundIDPool::TInstance, const SFileTrackConfig&);

    TNetStorageFlags DefaultFlags(TNetStorageFlags flags) const
    {
        return flags ? flags : default_flags;
    }

    ISelector* Create(const string&);
    ISelector* Create(TNetStorageFlags);
    ISelector* Create(TNetStorageFlags, const string&);
    ISelector* Create(TNetStorageFlags, const string&, Int8);
    ISelector* Create(TNetStorageFlags, const string&, const string&);

#ifdef NCBI_GRID_XSITE_CONN_SUPPORT
    void AllowXSiteConnections()
    {
        if (icache_client) icache_client.GetService().AllowXSiteConnections();
    }
#endif

private:
    void Init();

    CRandom m_Random;
};

}

END_NCBI_SCOPE

#endif
