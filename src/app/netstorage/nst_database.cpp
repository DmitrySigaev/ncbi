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
 * Author:  Sergey Satskiy
 *
 */

#include <ncbi_pch.hpp>

#include <corelib/ncbistd.hpp>
#include <connect/services/json_over_uttp.hpp>
#include <connect/services/netstorage.hpp>
#include <connect/services/netstorage_impl.hpp>
#include "nst_database.hpp"
#include "nst_exception.hpp"
#include "nst_application.hpp"
#include "nst_server.hpp"


BEGIN_NCBI_SCOPE


CNSTDatabase::CNSTDatabase(CNetStorageServer *  server)
    : m_Server(server), m_Db(NULL), m_Connected(false),
      m_RestoreConnectionThread(NULL)
{
    x_CreateDatabase();
    try {
        m_RestoreConnectionThread.Reset(new CNSTDBConnectionThread(
                                                m_Connected, m_Db));
        m_RestoreConnectionThread->Run();
    } catch (...) {
        delete m_Db;
        throw;
    }
}


void CNSTDatabase::InitialConnect(void)
{
    if (!m_Connected) {
        try {
            m_Db->Connect();
            m_Connected = true;
        } catch (...) {
            m_RestoreConnectionThread->Wakeup();
            throw;
        }
    }
}


CNSTDatabase::~CNSTDatabase(void)
{
    m_RestoreConnectionThread->Stop();
    m_RestoreConnectionThread->Join();
    m_RestoreConnectionThread.Reset(0);

    if (m_Connected)
        m_Db->Close();
    delete m_Db;
}


int
CNSTDatabase::ExecSP_GetNextObjectID(Int8 &  object_id)
{
    const string    proc_name = "GetNextObjectID";
    x_PreCheckConnection();

    int     status;
    try {
        CDatabase   db = m_Db->Clone();
        CQuery      query = db.NewQuery();

        object_id = 0;
        query.SetParameter("@next_id", 0, eSDB_Int8, eSP_InOut);
        query.ExecuteSP(proc_name);
        query.VerifyDone();
        status = x_CheckStatus(query, proc_name);

        if (status == 0)
            object_id = query.GetParameter("@next_id").AsInt8();
        else
            object_id = -1;
        return status;
    } catch (const std::exception &  ex) {
        m_Server->RegisterAlert(eDB, proc_name + " DB error: " + ex.what());
        x_PostCheckConnection();
        throw;
    } catch (...) {
        m_Server->RegisterAlert(eDB, proc_name + " unknown DB error");
        x_PostCheckConnection();
        throw;
    }
}


int
CNSTDatabase::ExecSP_CreateClient(
            const string &  client, Int8 &  client_id)
{
    const string    proc_name = "CreateClient";
    x_PreCheckConnection();

    int     status;
    try {
        CDatabase   db = m_Db->Clone();
        CQuery      query = db.NewQuery();

        client_id = -1;
        query.SetParameter("@client_name", client);
        query.SetParameter("@client_id", client_id, eSDB_Int8, eSP_InOut);

        query.ExecuteSP(proc_name);
        query.VerifyDone();
        status = x_CheckStatus(query, proc_name);

        if (status == 0)
            client_id = query.GetParameter("@client_id").AsInt8();
        else
            client_id = -1;
        return status;
    } catch (const std::exception &  ex) {
        m_Server->RegisterAlert(eDB, proc_name + " DB error: " + ex.what());
        x_PostCheckConnection();
        throw;
    } catch (...) {
        m_Server->RegisterAlert(eDB, proc_name + " unknown DB error");
        x_PostCheckConnection();
        throw;
    }
}


int
CNSTDatabase::ExecSP_CreateObjectWithClientID(
            Int8  object_id, const string &  object_key,
            const string &  object_loc, Int8  size,
            Int8  client_id, const TNSTDBValue<CTimeSpan>  ttl)
{
    const string    proc_name = "CreateObjectWithClientID";
    x_PreCheckConnection();
    try {
        CNetStorageObjectLoc    object_loc_struct(m_Server->GetCompoundIDPool(),
                                                  object_loc);
        CDatabase               db = m_Db->Clone();
        CQuery                  query = db.NewQuery();

        query.SetParameter("@object_id", object_id);
        query.SetParameter("@object_key", object_key);
        query.SetParameter("@object_create_tm",
                           object_loc_struct.GetCreationTime());
        query.SetParameter("@object_loc", object_loc);
        query.SetParameter("@object_size", size);
        query.SetParameter("@client_id", client_id);

        if (ttl.m_IsNull)
            query.SetNullParameter("@object_expiration", eSDB_DateTime);
        else
            query.SetParameter("@object_expiration",
                               CTime(CTime::eCurrent) + ttl.m_Value);

        query.ExecuteSP(proc_name);
        query.VerifyDone();
        return x_CheckStatus(query, proc_name);
    } catch (const std::exception &  ex) {
        m_Server->RegisterAlert(eDB, proc_name + " DB error: " + ex.what());
        x_PostCheckConnection();
        throw;
    } catch (...) {
        m_Server->RegisterAlert(eDB, proc_name + " unknown DB error");
        x_PostCheckConnection();
        throw;
    }
}


int
CNSTDatabase::ExecSP_UpdateObjectOnWrite(
            const string &  object_key,
            const string &  object_loc, Int8  size, Int8  client_id,
            const TNSTDBValue<CTimeSpan> &  ttl,
            const CTimeSpan &  prolong_on_write,
            const TNSTDBValue<CTime> &  object_expiration)
{
    // Calculate separate expirations for two cases:
    // - record is found
    // - record is not found
    // It is easier to do in C++ than in MS SQL stored procedure
    CTime                   current_time(CTime::eCurrent);
    TNSTDBValue<CTime>      exp_record_found;
    TNSTDBValue<CTime>      exp_record_not_found;
    x_CalculateExpiration(current_time, ttl, prolong_on_write,
                          object_expiration,
                          exp_record_found, exp_record_not_found);

    const string    proc_name = "UpdateObjectOnWrite";
    x_PreCheckConnection();
    try {
        CDatabase               db = m_Db->Clone();
        CQuery                  query = db.NewQuery();

        query.SetParameter("@object_key", object_key);
        query.SetParameter("@object_loc", object_loc);
        query.SetParameter("@object_size", size);
        query.SetParameter("@client_id", client_id);
        query.SetParameter("@current_time", current_time);

        if (exp_record_found.m_IsNull)
            query.SetNullParameter("@object_exp_if_found",
                                   eSDB_DateTime);
        else
            query.SetParameter("@object_exp_if_found",
                               exp_record_found.m_Value);
        if (exp_record_not_found.m_IsNull)
            query.SetNullParameter("@object_exp_if_not_found",
                                   eSDB_DateTime);
        else
            query.SetParameter("@object_exp_if_not_found",
                               exp_record_not_found.m_Value);

        query.ExecuteSP(proc_name);
        query.VerifyDone();
        return x_CheckStatus(query, proc_name);
    } catch (const std::exception &  ex) {
        m_Server->RegisterAlert(eDB, proc_name + " DB error: " + ex.what());
        x_PostCheckConnection();
        throw;
    } catch (...) {
        m_Server->RegisterAlert(eDB, proc_name + " unknown DB error");
        x_PostCheckConnection();
        throw;
    }
}


int
CNSTDatabase::ExecSP_UpdateUserKeyObjectOnWrite(
            const string &  object_key,
            const string &  object_loc, Int8  size, Int8  client_id,
            const TNSTDBValue<CTimeSpan> &  ttl,
            const CTimeSpan &  prolong_on_write,
            const TNSTDBValue<CTime> &  object_expiration)
{
    // Calculate separate expirations for two cases:
    // - record is found
    // - record is not found
    // It is easier to do in C++ than in MS SQL stored procedure
    CTime                   current_time(CTime::eCurrent);
    TNSTDBValue<CTime>      exp_record_found;
    TNSTDBValue<CTime>      exp_record_not_found;
    x_CalculateExpiration(current_time, ttl, prolong_on_write, object_expiration,
                          exp_record_found, exp_record_not_found);

    const string    proc_name = "UpdateUserKeyObjectOnWrite";
    x_PreCheckConnection();
    try {
        CDatabase               db = m_Db->Clone();
        CQuery                  query = db.NewQuery();

        query.SetParameter("@object_key", object_key);
        query.SetParameter("@object_loc", object_loc);
        query.SetParameter("@object_size", size);
        query.SetParameter("@client_id", client_id);
        query.SetParameter("@current_time", current_time);

        if (exp_record_found.m_IsNull)
            query.SetNullParameter("@object_exp_if_found",
                                   eSDB_DateTime);
        else
            query.SetParameter("@object_exp_if_found",
                               exp_record_found.m_Value);
        if (exp_record_not_found.m_IsNull)
            query.SetNullParameter("@object_exp_if_not_found",
                                   eSDB_DateTime);
        else
            query.SetParameter("@object_exp_if_not_found",
                               exp_record_not_found.m_Value);

        query.ExecuteSP(proc_name);
        query.VerifyDone();
        return x_CheckStatus(query, proc_name);
    } catch (const std::exception &  ex) {
        m_Server->RegisterAlert(eDB, proc_name + " DB error: " + ex.what());
        x_PostCheckConnection();
        throw;
    } catch (...) {
        m_Server->RegisterAlert(eDB, proc_name + " unknown DB error");
        x_PostCheckConnection();
        throw;
    }
}


int
CNSTDatabase::ExecSP_UpdateObjectOnRead(
            const string &  object_key,
            const string &  object_loc, Int8  size, Int8  client_id,
            const TNSTDBValue<CTimeSpan> &  ttl,
            const CTimeSpan &  prolong_on_read,
            const TNSTDBValue<CTime> &  object_expiration)
{
    // Calculate separate expirations for two cases:
    // - record is found
    // - record is not found
    // It is easier to do in C++ than in MS SQL stored procedure
    CTime                   current_time(CTime::eCurrent);
    TNSTDBValue<CTime>      exp_record_found;
    TNSTDBValue<CTime>      exp_record_not_found;
    x_CalculateExpiration(current_time, ttl, prolong_on_read, object_expiration,
                          exp_record_found, exp_record_not_found);

    const string    proc_name = "UpdateObjectOnRead";
    x_PreCheckConnection();
    try {
        CDatabase               db = m_Db->Clone();
        CQuery                  query = db.NewQuery();

        query.SetParameter("@object_key", object_key);
        query.SetParameter("@object_loc", object_loc);
        query.SetParameter("@object_size", size);
        query.SetParameter("@client_id", client_id);
        query.SetParameter("@current_time", current_time);

        if (exp_record_found.m_IsNull)
            query.SetNullParameter("@object_exp_if_found",
                                   eSDB_DateTime);
        else
            query.SetParameter("@object_exp_if_found",
                               exp_record_found.m_Value);
        if (exp_record_not_found.m_IsNull)
            query.SetNullParameter("@object_exp_if_not_found",
                                   eSDB_DateTime);
        else
            query.SetParameter("@object_exp_if_not_found",
                               exp_record_not_found.m_Value);

        query.ExecuteSP(proc_name);
        query.VerifyDone();
        return x_CheckStatus(query, proc_name);
    } catch (const std::exception &  ex) {
        m_Server->RegisterAlert(eDB, proc_name + " DB error: " + ex.what());
        x_PostCheckConnection();
        throw;
    } catch (...) {
        m_Server->RegisterAlert(eDB, proc_name + " unknown DB error");
        x_PostCheckConnection();
        throw;
    }
}


int
CNSTDatabase::ExecSP_UpdateObjectOnRelocate(
            const string &  object_key,
            const string &  object_loc, Int8  client_id)
{
    const string    proc_name = "UpdateObjectOnRelocate";
    x_PreCheckConnection();
    try {
        CDatabase               db = m_Db->Clone();
        CQuery                  query = db.NewQuery();

        query.SetParameter("@object_key", object_key);
        query.SetParameter("@object_loc", object_loc);
        query.SetParameter("@client_id", client_id);

        query.ExecuteSP(proc_name);
        query.VerifyDone();
        return x_CheckStatus(query, proc_name);
    } catch (const std::exception &  ex) {
        m_Server->RegisterAlert(eDB, proc_name + " DB error: " + ex.what());
        x_PostCheckConnection();
        throw;
    } catch (...) {
        m_Server->RegisterAlert(eDB, proc_name + " unknown DB error");
        x_PostCheckConnection();
        throw;
    }
}


int
CNSTDatabase::ExecSP_RemoveObject(const string &  object_key)
{
    const string    proc_name = "RemoveObject";
    x_PreCheckConnection();
    try {
        CDatabase               db = m_Db->Clone();
        CQuery                  query = db.NewQuery();

        query.SetParameter("@object_key", object_key);

        query.ExecuteSP(proc_name);
        query.VerifyDone();
        return x_CheckStatus(query, proc_name);
    } catch (const std::exception &  ex) {
        m_Server->RegisterAlert(eDB, proc_name + " DB error: " + ex.what());
        x_PostCheckConnection();
        throw;
    } catch (...) {
        m_Server->RegisterAlert(eDB, proc_name + " unknown DB error");
        x_PostCheckConnection();
        throw;
    }
}


int
CNSTDatabase::ExecSP_SetExpiration(const string &  object_key,
                                   const TNSTDBValue<CTimeSpan> &  ttl)
{
    const string    proc_name = "SetObjectExpiration";
    x_PreCheckConnection();
    try {
        CDatabase               db = m_Db->Clone();
        CQuery                  query = db.NewQuery();

        query.SetParameter("@object_key", object_key);

        if (ttl.m_IsNull)
            query.SetNullParameter("@expiration", eSDB_DateTime);
        else
            query.SetParameter("@expiration", CTime(CTime::eCurrent) +
                                              ttl.m_Value);

        query.ExecuteSP(proc_name);
        query.VerifyDone();
        return x_CheckStatus(query, proc_name);
    } catch (const std::exception &  ex) {
        m_Server->RegisterAlert(eDB, proc_name + " DB error: " + ex.what());
        x_PostCheckConnection();
        throw;
    } catch (...) {
        m_Server->RegisterAlert(eDB, proc_name + " unknown DB error");
        x_PostCheckConnection();
        throw;
    }
}


int
CNSTDatabase::ExecSP_AddAttribute(const string &  object_key,
                                  const string &  attr_name,
                                  const string &  attr_value,
                                  Int8  client_id)
{
    const string    proc_name = "AddAttribute";
    x_PreCheckConnection();
    try {
        CDatabase               db = m_Db->Clone();
        CQuery                  query = db.NewQuery();

        query.SetParameter("@object_key", object_key);
        query.SetParameter("@attr_name", attr_name);
        query.SetParameter("@attr_value", attr_value);
        query.SetParameter("@client_id", client_id);

        query.ExecuteSP(proc_name);
        query.VerifyDone();
        return x_CheckStatus(query, proc_name);
    } catch (const std::exception &  ex) {
        m_Server->RegisterAlert(eDB, proc_name + " DB error: " + ex.what());
        x_PostCheckConnection();
        throw;
    } catch (...) {
        m_Server->RegisterAlert(eDB, proc_name + " unknown DB error");
        x_PostCheckConnection();
        throw;
    }
}


int
CNSTDatabase::ExecSP_GetAttributeNames(const string &  object_key,
                                       vector<string> &  attr_names)
{
    const string    proc_name = "GetAttributeNames";
    x_PreCheckConnection();

    try {
        CDatabase               db = m_Db->Clone();
        CQuery                  query = db.NewQuery();

        query.SetParameter("@object_key", object_key);

        query.ExecuteSP(proc_name);

        // NOTE: reading result recordset must be done before getting the
        //       status code
        ITERATE(CQuery, qit, query.SingleSet()) {
            attr_names.push_back(qit["name"].AsString());
        }

        query.VerifyDone();
        return x_CheckStatus(query, proc_name);
    } catch (const std::exception &  ex) {
        m_Server->RegisterAlert(eDB, proc_name + " DB error: " + ex.what());
        x_PostCheckConnection();
        throw;
    } catch (...) {
        m_Server->RegisterAlert(eDB, proc_name + " unknown DB error");
        x_PostCheckConnection();
        throw;
    }
}


int
CNSTDatabase::ExecSP_GetAttribute(const string &  object_key,
                                  const string &  attr_name,
                                  bool            need_update,
                                  string &        value)
{
    const string    proc_name = "GetAttribute";
    x_PreCheckConnection();

    int     status;
    try {
        CDatabase               db = m_Db->Clone();
        CQuery                  query = db.NewQuery();

        query.SetParameter("@object_key", object_key);
        query.SetParameter("@attr_name", attr_name);
        query.SetParameter("@need_update", need_update);
        query.SetParameter("@attr_value", "", eSDB_String, eSP_InOut);

        query.ExecuteSP(proc_name);
        query.VerifyDone();
        status = x_CheckStatus(query, proc_name);

        if (status == 0)
            value = query.GetParameter("@attr_value").AsString();
        else
            value = "";
        return status;
    } catch (const std::exception &  ex) {
        m_Server->RegisterAlert(eDB, proc_name + " DB error: " + ex.what());
        x_PostCheckConnection();
        throw;
    } catch (...) {
        m_Server->RegisterAlert(eDB, proc_name + " unknown DB error");
        x_PostCheckConnection();
        throw;
    }
}


int
CNSTDatabase::ExecSP_DelAttribute(const string &  object_key,
                                  const string &  attr_name)
{
    const string    proc_name = "DelAttribute";
    x_PreCheckConnection();
    try {
        CDatabase               db = m_Db->Clone();
        CQuery                  query = db.NewQuery();

        query.SetParameter("@object_key", object_key);
        query.SetParameter("@attr_name", attr_name);

        query.ExecuteSP(proc_name);
        query.VerifyDone();
        return x_CheckStatus(query, proc_name);
    } catch (const std::exception &  ex) {
        m_Server->RegisterAlert(eDB, proc_name + " DB error: " + ex.what());
        x_PostCheckConnection();
        throw;
    } catch (...) {
        m_Server->RegisterAlert(eDB, proc_name + " unknown DB error");
        x_PostCheckConnection();
        throw;
    }
}


int
CNSTDatabase::ExecSP_GetObjectFixedAttributes(const string &        object_key,
                                              TNSTDBValue<CTime> &  expiration,
                                              TNSTDBValue<CTime> &  creation,
                                              TNSTDBValue<CTime> &  obj_read,
                                              TNSTDBValue<CTime> &  obj_write,
                                              TNSTDBValue<CTime> &  attr_read,
                                              TNSTDBValue<CTime> &  attr_write,
                                              TNSTDBValue<Int8> &   read_count,
                                              TNSTDBValue<Int8> &   write_count,
                                              TNSTDBValue<string> & client_name
                                              )
{
    const string    proc_name = "GetObjectFixedAttributes";
    x_PreCheckConnection();

    int     status;
    try {
        CDatabase               db = m_Db->Clone();
        CQuery                  query = db.NewQuery();

        query.SetParameter("@object_key", object_key);
        query.SetParameter("@expiration", expiration.m_Value,
                                          eSDB_DateTime, eSP_InOut);
        query.SetParameter("@creation", creation.m_Value,
                                        eSDB_DateTime, eSP_InOut);
        query.SetParameter("@obj_read", obj_read.m_Value,
                                        eSDB_DateTime, eSP_InOut);
        query.SetParameter("@obj_write", obj_write.m_Value,
                                         eSDB_DateTime, eSP_InOut);
        query.SetParameter("@attr_read", attr_read.m_Value,
                                         eSDB_DateTime, eSP_InOut);
        query.SetParameter("@attr_write", attr_write.m_Value,
                                          eSDB_DateTime, eSP_InOut);
        query.SetParameter("@read_cnt", read_count.m_Value,
                                        eSDB_Int8, eSP_InOut);
        query.SetParameter("@write_cnt", write_count.m_Value,
                                         eSDB_Int8, eSP_InOut);
        query.SetParameter("@client_name", client_name.m_Value,
                                           eSDB_String, eSP_InOut);

        query.ExecuteSP(proc_name);
        query.VerifyDone();
        status = x_CheckStatus(query, proc_name);

        if (status == 0) {
            expiration.m_IsNull = query.GetParameter("@expiration").IsNull();
            if (!expiration.m_IsNull)
                expiration.m_Value = query.GetParameter("@expiration").
                                                                AsDateTime();
            creation.m_IsNull = query.GetParameter("@creation").IsNull();
            if (!creation.m_IsNull)
                creation.m_Value = query.GetParameter("@creation").
                                                                AsDateTime();
            obj_read.m_IsNull = query.GetParameter("@obj_read").IsNull();
            if (!obj_read.m_IsNull)
                obj_read.m_Value = query.GetParameter("@obj_read").
                                                                AsDateTime();
            obj_write.m_IsNull = query.GetParameter("@obj_write").IsNull();
            if (!obj_write.m_IsNull)
                obj_write.m_Value = query.GetParameter("@obj_write").
                                                                AsDateTime();
            attr_read.m_IsNull = query.GetParameter("@attr_read").IsNull();
            if (!attr_read.m_IsNull)
                attr_read.m_Value = query.GetParameter("@attr_read").
                                                                AsDateTime();
            attr_write.m_IsNull = query.GetParameter("@attr_write").IsNull();
            if (!attr_write.m_IsNull)
                attr_write.m_Value = query.GetParameter("@attr_write").
                                                                AsDateTime();
            read_count.m_IsNull = query.GetParameter("@read_cnt").IsNull();
            if (!read_count.m_IsNull)
                read_count.m_Value = query.GetParameter("@read_cnt").
                                                                AsInt8();
            write_count.m_IsNull = query.GetParameter("@write_cnt").IsNull();
            if (!write_count.m_IsNull)
                write_count.m_Value = query.GetParameter("@write_cnt").
                                                                AsInt8();
            client_name.m_IsNull = query.GetParameter("@client_name").IsNull();
            if (!client_name.m_IsNull)
                client_name.m_Value = query.GetParameter("@client_name").
                                                                AsString();
        }
        return status;
    } catch (const std::exception &  ex) {
        m_Server->RegisterAlert(eDB, proc_name + " DB error: " + ex.what());
        x_PostCheckConnection();
        throw;
    } catch (...) {
        m_Server->RegisterAlert(eDB, proc_name + " unknown DB error");
        x_PostCheckConnection();
        throw;
    }
}


int
CNSTDatabase::ExecSP_GetObjectExpiration(const string &        object_key,
                                         TNSTDBValue<CTime> &  expiration)
{
    const string    proc_name = "GetObjectExpiration";
    x_PreCheckConnection();

    int     status;
    try {
        CDatabase               db = m_Db->Clone();
        CQuery                  query = db.NewQuery();

        query.SetParameter("@object_key", object_key);
        query.SetParameter("@expiration", expiration.m_Value,
                                          eSDB_DateTime, eSP_InOut);

        query.ExecuteSP(proc_name);
        query.VerifyDone();
        status = x_CheckStatus(query, proc_name);

        if (status == 0) {
            expiration.m_IsNull = query.GetParameter("@expiration").IsNull();
            if (!expiration.m_IsNull)
                expiration.m_Value = query.GetParameter("@expiration").
                                                                AsDateTime();
        }
        return status;
    } catch (const std::exception &  ex) {
        m_Server->RegisterAlert(eDB, proc_name + " DB error: " + ex.what());
        x_PostCheckConnection();
        throw;
    } catch (...) {
        m_Server->RegisterAlert(eDB, proc_name + " unknown DB error");
        x_PostCheckConnection();
        throw;
    }
}


map<string, string>
CNSTDatabase::ExecSP_GetGeneralDBInfo(void)
{
    map<string, string>     result;
    const string            proc_name = "sp_spaceused";
    x_PreCheckConnection();

    try {
        CDatabase               db = m_Db->Clone();
        CQuery                  query = db.NewQuery();

        query.ExecuteSP(proc_name);

        // sp_spaceused provides two recordsets 1 record each, e.g.
        // NETSTORAGE   224.88 MB   98.34 MB
        // 1696 KB   752 KB  736 KB  208 KB
        ITERATE(CQuery, qit, query.SingleSet()) {
            for (int  k = 1; k <= qit.GetTotalColumns(); ++k) {
                string  columnName = qit.GetColumnName(k);
                result[columnName] = qit[k].AsString();
            }
        }
        query.VerifyDone();
    } catch (const std::exception &  ex) {
        m_Server->RegisterAlert(eDB, proc_name + " DB error: " + ex.what());
        x_PostCheckConnection();
        throw;
    } catch (...) {
        m_Server->RegisterAlert(eDB, proc_name + " unknown DB error");
        x_PostCheckConnection();
        throw;
    }
    return result;
}


map<string, string>
CNSTDatabase::ExecSP_GetStatDBInfo(void)
{
    const string            proc_name = "GetStatInfo";
    map<string, string>     result;
    x_PreCheckConnection();

    try {
        CDatabase               db = m_Db->Clone();
        CQuery                  query = db.NewQuery();

        query.ExecuteSP(proc_name);

        ITERATE(CQuery, qit, query.SingleSet()) {
            for (int  k = 1; k <= qit.GetTotalColumns(); ++k) {
                string  columnName = qit.GetColumnName(k);
                result[columnName] = qit[k].AsString();
            }
        }
        query.VerifyDone();
    } catch (const std::exception &  ex) {
        m_Server->RegisterAlert(eDB, proc_name + " DB error: " + ex.what());
        x_PostCheckConnection();
        throw;
    } catch (...) {
        m_Server->RegisterAlert(eDB, proc_name + " unknown DB error");
        x_PostCheckConnection();
        throw;
    }
    return result;
}


int
CNSTDatabase::ExecSP_GetClientObjects(const string &  client_name,
                                      TNSTDBValue<Int8>  limit,
                                      Int8 &  total,
                                      vector<string> &  locators)
{
    const string    proc_name = "GetClientObjects";
    x_PreCheckConnection();

    try {
        CDatabase               db = m_Db->Clone();
        CQuery                  query = db.NewQuery();

        query.SetParameter("@client_name", client_name);
        if (limit.m_IsNull)
            query.SetNullParameter("@limit", eSDB_Int8);
        else
            query.SetParameter("@limit", limit.m_Value);
        query.SetParameter("@total_object_cnt", total, eSDB_Int8, eSP_InOut);

        query.ExecuteSP(proc_name);

        // NOTE: reading result recordset must be done before getting the
        //       status code
        ITERATE(CQuery, qit, query.SingleSet()) {
            locators.push_back(qit["object_loc"].AsString());
        }

        total = query.GetParameter("@total_object_cnt").AsInt8();
        query.VerifyDone();
        return x_CheckStatus(query, proc_name);
    } catch (const std::exception &  ex) {
        m_Server->RegisterAlert(eDB, proc_name + " DB error: " + ex.what());
        x_PostCheckConnection();
        throw;
    } catch (...) {
        m_Server->RegisterAlert(eDB, proc_name + " unknown DB error");
        x_PostCheckConnection();
        throw;
    }
}


void CNSTDatabase::x_PreCheckConnection(void)
{
    if (!m_Connected)
        NCBI_THROW(CNetStorageServerException, eDatabaseError,
                   "There is no connection to metadata information database");

    // It is possible that a connection has been lost and restored while there
    // were no activities
    if (!m_Db->IsConnected(CDatabase::eFastCheck)) {
        try {
            m_Db->Close();
            m_Db->Connect();
        } catch (...) {
            // To avoid interfering the connection restoring thread nothing is
            // done here. Basically the fact that we are here means there is no
            // connection anymore. The exception is suppressed however a stored
            // procedure execution exception will be generated a few moments
            // later and a normal procedure of the connection restoration will
            // be activated.
        }
    }
}


void CNSTDatabase::x_PostCheckConnection(void)
{
    if (!m_Db->IsConnected(CDatabase::eFastCheck)) {
        m_Connected = false;
        ERR_POST(Critical << "Database connection has been lost");
        m_RestoreConnectionThread->Wakeup();
    }
}


int  CNSTDatabase::x_CheckStatus(CQuery &  query,
                                 const string &  procedure)
{
    int     status = query.GetStatus();
    if (status > 0)
        NCBI_THROW(CNetStorageServerException, eDatabaseError,
                   "Error executing " + procedure + " stored "
                   "procedure (return code " + NStr::NumericToString(status) +
                   "). See MS SQL log for details.");
    return status;
}


void  CNSTDatabase::x_ReadDbAccessInfo(void)
{
    CNetStorageDApp *       app = dynamic_cast<CNetStorageDApp*>
                                        (CNcbiApplication::Instance());
    const CNcbiRegistry &   reg  = app->GetConfig();

    m_DbAccessInfo.m_Service = reg.GetString("database", "service", "");
    m_DbAccessInfo.m_UserName = reg.GetString("database", "user_name", "");
    m_DbAccessInfo.m_Password = reg.GetString("database", "password", "");
    m_DbAccessInfo.m_Database = reg.GetString("database", "database", "");
}


void  CNSTDatabase::x_CreateDatabase(void)
{
    x_ReadDbAccessInfo();

    string  uri = "dbapi://" + m_DbAccessInfo.m_UserName +
                  ":" + m_DbAccessInfo.m_Password +
                  "@" + m_DbAccessInfo.m_Service +
                  "/" + m_DbAccessInfo.m_Database;

    CSDB_ConnectionParam    db_params(uri);
    m_Db = new CDatabase(db_params);
}


// Calculates the new object expiration for two cases:
// - object is found in the DB
// - object is not found in the DB (and thus should be created)
void
CNSTDatabase::x_CalculateExpiration(
                            const CTime &  current_time,
                            const TNSTDBValue<CTimeSpan> &  ttl,
                            const CTimeSpan &  prolong,
                            const TNSTDBValue<CTime> &  object_expiration,
                            TNSTDBValue<CTime> &  exp_record_found,
                            TNSTDBValue<CTime> &  exp_record_not_found)
{
    if (prolong.IsEmpty()) {
        if (object_expiration.m_IsNull) {
            exp_record_found.m_IsNull = true;
            if (ttl.m_IsNull)
                exp_record_not_found.m_IsNull = true;
            else {
                exp_record_not_found.m_IsNull = false;
                exp_record_not_found.m_Value = current_time + ttl.m_Value;
            }
        } else {
            exp_record_found.m_IsNull = false;
            exp_record_found.m_Value = object_expiration.m_Value;
        }
    } else {
        if (object_expiration.m_IsNull) {
            exp_record_found.m_IsNull = true;
            if (ttl.m_IsNull)
                exp_record_not_found.m_IsNull = true;
            else {
                exp_record_not_found.m_IsNull = false;
                if (ttl.m_Value > prolong)
                    exp_record_not_found.m_Value = current_time + ttl.m_Value;
                else
                    exp_record_not_found.m_Value = current_time + prolong;
            }
        } else {
            exp_record_found.m_IsNull = false;
            if (object_expiration.m_Value > current_time + prolong)
                exp_record_found.m_Value = object_expiration.m_Value;
            else
                exp_record_found.m_Value = current_time + prolong;
        }
    }
}

END_NCBI_SCOPE

