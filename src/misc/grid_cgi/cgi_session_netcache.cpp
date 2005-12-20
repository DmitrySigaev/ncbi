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
 * Author: Maxim Didenko
 *
 *
 */
#include <ncbi_pch.hpp>

#include <corelib/ncbireg.hpp>
#include <corelib/ncbistre.hpp>
#include <corelib/ncbistr.hpp>

#include <cgi/ncbicgi.hpp>

#include <misc/grid_cgi/cgi_session_netcache.hpp>

#include <connect/services/blob_storage_netcache.hpp>

BEGIN_NCBI_SCOPE

CCgiSession_Netcache::CCgiSession_Netcache(const IRegistry& conf) 
    : m_Dirty(false), m_Loaded(false)
{
    CBlobStorageFactory_NetCache factory(conf);
    m_Storage.reset(factory.CreateInstance());
}

CCgiSession_Netcache::~CCgiSession_Netcache()
{
    try {
        Reset();
    } catch (...) {}
}

string CCgiSession_Netcache::CreateNewSession()
{
    m_Blobs.clear();
    m_SessionId.erase();
    Reset();
    m_SessionId = m_Storage->CreateEmptyBlob();
    m_Loaded = true;
    return m_SessionId;
}


bool CCgiSession_Netcache::LoadSession(const string& sessionid)
{
    m_Blobs.clear();
    m_SessionId.erase();
    Reset();
    m_Loaded = false;
    string master_value;
    try {
        master_value = m_Storage->GetBlobAsString(sessionid);
    }
    catch(...) {
        return false;
    }
    m_SessionId = sessionid;
    list<string> pairs;
    NStr::Split(master_value, ";", pairs);
    ITERATE(list<string>, it, pairs) {
        string blobid, blobname;
        NStr::SplitInTwo(*it, "|", blobname, blobid);
        m_Blobs[blobname] = blobid;          
    }
    m_Loaded = true;
    return m_Loaded;
}
CCgiSession::TNames CCgiSession_Netcache::GetAttributeNames() const
{
    TNames names;
    x_CheckStatus();
    ITERATE(TBlobs, it, m_Blobs) {
        names.push_back(it->first);
    }
    return names;
}

CNcbiIstream& CCgiSession_Netcache::GetAttrIStream(const string& name, 
                                                   size_t* size)
{
    x_CheckStatus();

    Reset();
    TBlobs::const_iterator i = m_Blobs.find(name);
    if (i == m_Blobs.end()) {
        static CNcbiIstrstream sEmptyStream("",0);
        if (size) *size = 0;
        return sEmptyStream;
    }
    return m_Storage->GetIStream(i->second, size);
}
CNcbiOstream& CCgiSession_Netcache::GetAttrOStream(const string& name)
{
    x_CheckStatus();
    Reset();
    m_Dirty = true;
    string& blobid = m_Blobs[name];
    return m_Storage->CreateOStream(blobid);
}

void CCgiSession_Netcache::SetAttribute(const string& name, const string& value)
{
    x_CheckStatus();
    Reset();
    m_Dirty = true;
    string& blobid = m_Blobs[name];
    CNcbiOstream& os = m_Storage->CreateOStream(blobid);
    os << value;
    Reset();
}
string CCgiSession_Netcache::GetAttribute(const string& name) const
{
    x_CheckStatus();
    const_cast<CCgiSession_Netcache*>(this)->Reset();
    TBlobs::const_iterator i = m_Blobs.find(name);
    if (i == m_Blobs.end()) {
        return "";
    }
    return m_Storage->GetBlobAsString(i->second);
}

void CCgiSession_Netcache::RemoveAttribute(const string& name)
{
     x_CheckStatus();
    TBlobs::iterator i = m_Blobs.find(name);
    if (i == m_Blobs.end())
        return;           
    Reset(); 
    string blobid = i->second;
    m_Blobs.erase(i);
    m_Storage->DeleteBlob(blobid);
    m_Dirty = true;
    Reset();
}
void CCgiSession_Netcache::DeleteSession()
{
    x_CheckStatus();
    Reset();
    ITERATE(TBlobs, it, m_Blobs) {
        m_Storage->DeleteBlob(it->second);
    }
    m_Storage->DeleteBlob(m_SessionId);
    m_Loaded = false;
}


void CCgiSession_Netcache::Reset()
{
    if (!m_Loaded) return;
    m_Storage->Reset();
    if (m_Dirty) {
        string master_value;
        ITERATE(TBlobs, it, m_Blobs) {
            if (it != m_Blobs.begin())
                master_value += ";";
            master_value += it->first + "|" + it->second;
        }
        CNcbiOstream& os = m_Storage->CreateOStream(m_SessionId);
        os << master_value;
        m_Storage->Reset();
        m_Dirty = false;
    }
}


void CCgiSession_Netcache::x_CheckStatus() const
{
    if (!m_Loaded)
        NCBI_THROW(CCgiSessionNCException,
                   eNotLoaded, "The Session is not loaded.");
}

END_NCBI_SCOPE

/* @} */


/*
 * ===========================================================================
 * $Log$
 * Revision 1.5  2005/12/20 20:36:02  didenko
 * Comments cosmetics
 * Small interace changes
 *
 * Revision 1.4  2005/12/20 17:26:22  didenko
 * Reorganized netschedule storage facility.
 * renamed INetScheduleStorage to IBlobStorage and moved it to corelib
 * renamed INetScheduleStorageFactory to IBlobStorageFactory and moved it to corelib
 * renamed CNetScheduleNSStorage_NetCache to CBlobStorage_NetCache and moved it
 * to separate files
 * Moved CNetScheduleClientFactory to separate files
 *
 * Revision 1.3  2005/12/19 16:55:04  didenko
 * Improved CGI Session implementation
 *
 * Revision 1.2  2005/12/15 21:56:16  ucko
 * Use string::erase rather than string::clear for GCC 2.95 compatibility.
 *
 * Revision 1.1  2005/12/15 18:21:16  didenko
 * Added CGI session support
 *
 * ===========================================================================
 */
