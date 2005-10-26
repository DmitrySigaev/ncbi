#ifndef CONNECT_SERVICES__NETSCHEDULE_STORAGE_HPP
#define CONNECT_SERVICES__NETSCHEDULE_STORAGE_HPP

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
 * Authors:  Maxim Didneko, Anatoliy Kuznetsov
 *
 * File Description:  NetSchedule shared storage interface
 *
 */

#include <connect/connect_export.h>
#include <corelib/ncbistr.hpp>
#include <corelib/ncbistre.hpp>
#include <corelib/ncbiexpt.hpp>

BEGIN_NCBI_SCOPE


/** @addtogroup NetScheduleClient
 *
 * @{
 */


/// NetSchedule Storage interface
///
/// This interface is used by Worker Node and Worker 
/// Node client to retrive and store data to/from external storage
//
class NCBI_XCONNECT_EXPORT INetScheduleStorage
{
public:
    enum ELockMode {
        eLockWait,   ///< waits for BLOB to become available
        eLockNoWait  ///< throws an exception immediately if BLOB locked
    };

    virtual ~INetScheduleStorage() {}

    virtual string        GetBlobAsString(const string& data_id) = 0;
    virtual CNcbiIstream& GetIStream(const string& data_id, 
                                     size_t* blob_size = 0,
                                     ELockMode lockMode = eLockWait) = 0;
    virtual CNcbiOstream& CreateOStream(string& data_id) = 0;

    virtual string CreateEmptyBlob() = 0;
    virtual void DeleteBlob(const string& data_id) = 0;

    virtual void Reset() = 0;

};


/// NetSchedule Storage Factory interafce
///
/// @sa INetScheduleStorage
///
class INetScheduleStorageFactory
{
public:
    virtual ~INetScheduleStorageFactory() {}
    /// Create a NetSchedule storage
    ///
    virtual INetScheduleStorage* CreateInstance(void) = 0;
};

class CNetScheduleClient;
/// NetSchedule Client Factory interface
///
/// @sa CNetScheduleClient
///
class INetScheduleClientFactory
{
public:
    virtual ~INetScheduleClientFactory() {}
    /// Create a NetSchedule client
    ///
    virtual CNetScheduleClient* CreateInstance(void) = 0;
};

class NCBI_XCONNECT_EXPORT CNetScheduleStorageException : public CException
{
public:
    enum EErrCode {
        eReader,
        eWriter,
        eBlocked
    };

    virtual const char* GetErrCodeString(void) const
    {
        switch (GetErrCode())
        {
        case eReader:  return "eReaderError";
        case eWriter:  return "eWriterError";
        case eBlocked: return "eBlocked";
        default:       return CException::GetErrCodeString();
        }
    }

    NCBI_EXCEPTION_DEFAULT(CNetScheduleStorageException, CException);
};

class NCBI_XCONNECT_EXPORT CNullStorage : public INetScheduleStorage
{
public:
    virtual ~CNullStorage() {}

    virtual string        GetBlobAsString( const string& data_id)
    {
        return data_id;
    }

    virtual CNcbiIstream& GetIStream(const string&,
                                     size_t* blob_size = 0,
                                     ELockMode lockMode = eLockWait)
    {
        if (blob_size) *blob_size = 0;
        NCBI_THROW(CNetScheduleStorageException,
                   eReader, "Empty Storage reader.");
    }
    virtual CNcbiOstream& CreateOStream(string&)
    {
        NCBI_THROW(CNetScheduleStorageException,
                   eWriter, "Empty Storage writer.");
    }

    virtual string CreateEmptyBlob() { return kEmptyStr; };
    virtual void DeleteBlob(const string& data_id) {}
    virtual void Reset() {};
};


/* @} */

END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.10  2005/10/26 16:37:44  didenko
 * Added for non-blocking read for netschedule storage
 *
 * Revision 1.9  2005/05/10 14:11:22  didenko
 * Added blob caching
 *
 * Revision 1.8  2005/05/05 15:18:51  didenko
 * Added debugging facility to worker nodes
 *
 * Revision 1.7  2005/04/20 19:23:47  didenko
 * Added GetBlobAsString, GreateEmptyBlob methods
 * Remave RemoveData to DeleteBlob
 *
 * Revision 1.6  2005/03/29 14:10:16  didenko
 * + removing a date from the storage
 *
 * Revision 1.5  2005/03/28 16:49:00  didenko
 * Added virtual desturctors to all new interfaces to prevent memory leaks
 *
 * Revision 1.4  2005/03/25 16:24:58  didenko
 * Classes restructure
 *
 * Revision 1.3  2005/03/23 13:10:32  kuznets
 * documented and doxygenized
 *
 * Revision 1.2  2005/03/22 21:42:50  didenko
 * Got rid of warnning on Sun WorkShop
 *
 * Revision 1.1  2005/03/22 20:17:55  didenko
 * Initial version
 *
 * ===========================================================================
 */


#endif // CONNECT_SERVICES__NETSCHEDULE_STORAGE_HPP
