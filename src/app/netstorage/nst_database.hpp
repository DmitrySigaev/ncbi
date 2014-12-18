#ifndef NETSTORAGE_DATABASE__HPP
#define NETSTORAGE_DATABASE__HPP

/* $Id$
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
 * Author: Sergey Satskiy
 *
 */

#include <corelib/ncbiapp.hpp>
#include <dbapi/simple/sdbapi.hpp>

#include "nst_dbconnection_thread.hpp"



BEGIN_NCBI_SCOPE

// Forward declarations
struct SCommonRequestArguments;
class CJsonNode;
class CNetStorageServer;
class CNSTDBConnectionThread;


struct SDbAccessInfo
{
    string    m_Service;
    string    m_UserName;
    string    m_Password;
    string    m_Database;
};


template <typename ValueType> struct TNSTDBValue
{
    bool        m_IsNull;
    ValueType   m_Value;

    bool operator==(const TNSTDBValue<ValueType> &  other) const
    { return (m_IsNull == other.m_IsNull) && (m_Value == other.m_Value); }
    bool operator!=(const TNSTDBValue<ValueType> &  other) const
    { return !this->operator==(other); }
};


class CNSTDatabase
{
public:
    CNSTDatabase(CNetStorageServer *  server);
    ~CNSTDatabase(void);

    void InitialConnect(void);

    int  ExecSP_GetNextObjectID(Int8 &  object_id);
    int  ExecSP_CreateClient(const string &  client,
                             Int8 &  client_id);
    int  ExecSP_CreateObjectWithClientID(
            Int8  object_id, const string &  object_key,
            const string &  object_loc, Int8  size,
            Int8  client_id, const TNSTDBValue<CTimeSpan>  ttl);
    int  ExecSP_UpdateObjectOnWrite(
            const string &  object_key,
            const string &  object_loc, Int8  size, Int8  client_id,
            const TNSTDBValue<CTimeSpan> &  ttl,
            const CTimeSpan &  prolong_on_write,
            const TNSTDBValue<CTime> &  object_expiration);
    int  ExecSP_UpdateUserKeyObjectOnWrite(
            const string &  object_key,
            const string &  object_loc, Int8  size, Int8  client_id,
            const TNSTDBValue<CTimeSpan> &  ttl,
            const CTimeSpan &  prolong_on_write,
            const TNSTDBValue<CTime> &  object_expiration);
    int  ExecSP_UpdateObjectOnRead(
            const string &  object_key,
            const string &  object_loc, Int8  size, Int8  client_id,
            const TNSTDBValue<CTimeSpan> &  ttl,
            const CTimeSpan &  prolong_on_read,
            const TNSTDBValue<CTime> &  object_expiration);
    int  ExecSP_UpdateObjectOnRelocate(
            const string &  object_key,
            const string &  object_loc, Int8  client_id);
    int  ExecSP_RemoveObject(const string &  object_key);
    int  ExecSP_SetExpiration(const string &  object_key,
                              const TNSTDBValue<CTimeSpan> &  ttl);
    int  ExecSP_AddAttribute(const string &  object_key,
                             const string &  attr_name,
                             const string &  attr_value,
                             Int8  client_id);
    int  ExecSP_GetAttribute(const string &  object_key,
                             const string &  attr_name,
                             bool            need_update,
                             string &        value);
    int  ExecSP_DelAttribute(const string &  object_key,
                             const string &  attr_name);
    int  ExecSP_GetAttributeNames(const string &  object_key,
                                  vector<string> &  attr_names);
    int  ExecSP_GetObjectFixedAttributes(const string &        object_key,
                                         TNSTDBValue<CTime> &  expiration,
                                         TNSTDBValue<CTime> &  creation,
                                         TNSTDBValue<CTime> &  obj_read,
                                         TNSTDBValue<CTime> &  obj_write,
                                         TNSTDBValue<CTime> &  attr_read,
                                         TNSTDBValue<CTime> &  attr_write,
                                         TNSTDBValue<Int8> &   read_count,
                                         TNSTDBValue<Int8> &   write_count);
    int  ExecSP_GetObjectExpiration(const string &        object_key,
                                    TNSTDBValue<CTime> &  expiration);

private:
    void x_ReadDbAccessInfo(void);
    void x_CreateDatabase(void);
    CQuery x_NewQuery(void);
    void x_PreCheckConnection(void);
    void x_PostCheckConnection(void);
    int  x_CheckStatus(CQuery &  query,
                       const string &  procedure);
    void x_CalculateExpiration(const CTime &  current_time,
                               const TNSTDBValue<CTimeSpan> &  ttl,
                               const CTimeSpan &  prolong,
                               const TNSTDBValue<CTime> &  object_expiration,
                               TNSTDBValue<CTime> &  exp_record_found,
                               TNSTDBValue<CTime> &  exp_record_not_found);

private:
    SDbAccessInfo                   m_DbAccessInfo;
    CNetStorageServer *             m_Server;
    CDatabase *                     m_Db;
    bool                            m_Connected;
    CRef<CNSTDBConnectionThread>    m_RestoreConnectionThread;

    CNSTDatabase(const CNSTDatabase &  conn);
    CNSTDatabase & operator= (const CNSTDatabase &  conn);
};


END_NCBI_SCOPE

#endif /* NETSTORAGE_DBAPP__HPP */

