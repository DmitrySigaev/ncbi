#ifndef MISC_GRID_CGI___CGI_SESSION_NETCACHE__HPP
#define MISC_GRID_CGI___CGI_SESSION_NETCACHE__HPP

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
*/

#include <stddef.h>
#include <corelib/ncbiexpt.hpp>
#include <corelib/blob_storage.hpp>

#include <cgi/cgi_session.hpp>

#include <map>
#include <string>


BEGIN_NCBI_SCOPE

class IRegistry;

///////////////////////////////////////////////////////////////////////////////
//
//  CCgiSession_NetCache -- 
//  
//  Implementaion of ICgiSessionStorage interface based on NetCache service
//
//  @note
//    This implementation can work only with one attribute at a time. This
//    means that if a user requested a stream to the first attribute and then
//    requests a stream or reading/writing the second one, the 
//    stream to the fist attribute is getting closed.
//
class NCBI_XGRIDCGI_EXPORT CCgiSession_NetCache : public ICgiSessionStorage
{
public:
    /// Create Session Storage from the registry
    CCgiSession_NetCache(const IRegistry&);
    /*
    /// Create Session Storage 
    /// @param[in] nc_client
    ///  NetCache client. Session Storage will delete it when
    ///  it goes out of scope.
    /// @param[in] flags
    ///  Specifies if blobs should be cached on a local fs
    ///  before/after they are accessed for read/write.
    /// @param[in[ temp_dir
    ///  Specifies where on a local fs those blobs will be cached
    CCgiSession_NetCache(CNetCacheClient* nc_client, 
                         CBlobStorage_NetCache::TCacheFlags flags = 0x0,
                         const string& temp_dir = ".");
    */
    virtual ~CCgiSession_NetCache();

    /// Create a new empty session. 
    /// @return ID of the new session
    virtual string CreateNewSession();

    /// Load the session
    /// @param[in] sesseionid
    ///  ID of the session
    /// @return true if the session was loaded, false otherwise
    virtual bool LoadSession(const string& sessionid);

    /// Retrieve names of all attributes attached to this session.
    virtual TNames GetAttributeNames(void) const;

    /// Get input stream to read an attribute's data from.
    /// @param[in] name
    ///  Name of the attribute
    /// @param[out] size
    ///  Size of the attribute's data
    /// @return 
    ///  Stream to read attribute's data from.If the attribute does not exist, 
    ///  then return an empty stream.
    virtual CNcbiIstream& GetAttrIStream(const string& name,
                                         size_t* size = 0);

    /// Get output stream to write an attribute's data to.
    /// If the attribute does not exist it will be created and added 
    /// to the session. If the attribute exists its content will be
    /// overwritten.
    /// @param[in] name
    ///  Name of the attribute
    virtual CNcbiOstream& GetAttrOStream(const string& name);

    /// Set attribute data as a string.
    /// @param[in] name
    ///  Name of the attribute to set
    /// @param[in] value
    ///  Value to set the attribute data to
    virtual void SetAttribute(const string& name, const string& value);

    /// Get attribute data as string.
    /// @param[in] name
    ///  Name of the attribute to retrieve
    /// @return
    ///  Attribute's data, or empty string if the attribute does not exist.
    virtual string GetAttribute(const string& name) const;  

    /// Remove attribute from the session.
    /// @param[in] name
    ///  Name of the attribute to remove
    virtual void RemoveAttribute(const string& name);

    /// Delete current session
    virtual void DeleteSession();

    /// Close all open connections
    virtual void Reset();

private:
    typedef map<string,string> TBlobs;

    string m_SessionId;    
    auto_ptr<IBlobStorage> m_Storage;
    
    TBlobs m_Blobs;    
    bool m_Dirty;

    bool m_Loaded;
    
    void x_CheckStatus() const;

    CCgiSession_NetCache(const CCgiSession_NetCache&);
    CCgiSession_NetCache& operator=(const CCgiSession_NetCache&);
};


END_NCBI_SCOPE


/* @} */


/*
 * ===========================================================================
 * $Log$
 * Revision 1.9  2006/03/28 17:01:13  didenko
 * - CCgiSessionNCException
 * Throw en exception when a requested attribute is not found in the session
 *
 * Revision 1.8  2006/02/27 14:50:21  didenko
 * Redone an implementation of IBlobStorage interface based on NetCache as a plugin
 *
 * Revision 1.7  2006/01/09 16:17:28  vakatov
 * Removed extraneous comma
 *
 * Revision 1.6  2005/12/23 15:02:44  didenko
 * Added comments
 * Added one more constructor
 *
 * Revision 1.5  2005/12/23 14:25:26  didenko
 * Renamed CCgiSession_Netcache to CCgiSession_NetCache
 *
 * Revision 1.4  2005/12/20 20:36:02  didenko
 * Comments cosmetics
 * Small interace changes
 *
 * Revision 1.3  2005/12/20 17:26:22  didenko
 * Reorganized netschedule storage facility.
 * renamed INetScheduleStorage to IBlobStorage and moved it to corelib
 * renamed INetScheduleStorageFactory to IBlobStorageFactory and moved it to corelib
 * renamed CNetScheduleNSStorage_NetCache to CBlobStorage_NetCache and moved it
 * to separate files
 * Moved CNetScheduleClientFactory to separate files
 *
 * Revision 1.2  2005/12/19 16:55:04  didenko
 * Improved CGI Session implementation
 *
 * Revision 1.1  2005/12/15 18:21:15  didenko
 * Added CGI session support
 *
 * ===========================================================================
 */

#endif  /* MISC_GRID_CGI___CGI_SESSION_NETCACHE__HPP */
