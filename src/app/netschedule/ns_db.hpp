#ifndef NETSCHEDULE_DB__HPP
#define NETSCHEDULE_DB__HPP

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
 * Authors:  Anatoliy Kuznetsov, Victor Joukov
 *
 * File Description:
 *   NetSchedule database structure.
 *
 */

/// @file ns_db.hpp
/// NetSchedule database structure.
///
/// This file collects all BDB data files and index files used in netschedule
///
/// @internal

#include <corelib/ncbimtx.hpp>
#include <corelib/ncbicntr.hpp>

#include <util/bitset/ncbi_bitset.hpp>

#include <bdb/bdb_file.hpp>
#include <bdb/bdb_env.hpp>
#include <bdb/bdb_cursor.hpp>
#include <bdb/bdb_bv_store.hpp>

#include <connect/services/netschedule_api.hpp>


BEGIN_NCBI_SCOPE

const unsigned kNetScheduleSplitSize = 64;

/// BDB table to store queue
///
/// @internal
///
struct SQueueDB : public CBDB_File
{
    CBDB_FieldUint4        id;              ///< Job id

    CBDB_FieldInt4         status;          ///< Current job status
    CBDB_FieldUint4        time_submit;     ///< Job submit time
    CBDB_FieldUint4        time_run;        ///<     run time
    CBDB_FieldUint4        time_done;       ///<     result submission time
    CBDB_FieldUint4        timeout;         ///<     individual timeout
    CBDB_FieldUint4        run_timeout;     ///<     job run timeout

    CBDB_FieldUint4        subm_addr;       ///< netw BO (for notification)
    CBDB_FieldUint4        subm_port;       ///< notification port
    CBDB_FieldUint4        subm_timeout;    ///< notification timeout

    CBDB_FieldUint4        worker_node1;    ///< IP of wnode 1 (netw BO)
    CBDB_FieldUint4        worker_node2;    ///<       wnode 2
    CBDB_FieldUint4        worker_node3;
    CBDB_FieldUint4        worker_node4;
    CBDB_FieldUint4        worker_node5;

    CBDB_FieldUint4        run_counter;     ///< Number of execution attempts
    CBDB_FieldInt4         ret_code;        ///< Return code

    /// Affinity token id (refers to the affinity dictionary DB)
    CBDB_FieldUint4        aff_id;
    CBDB_FieldUint4        mask;

    CBDB_FieldLString      input;           ///< Input data
    CBDB_FieldLString      output;          ///< Result data

    CBDB_FieldString       err_msg;         ///< Error message (exception::what())
    CBDB_FieldString       progress_msg;    ///< Progress report message

    SQueueDB()
    {
        DisableNull(); 

        BindKey("id", &id);

        BindData("status",       &status);
        BindData("time_submit",  &time_submit);
        BindData("time_run",     &time_run);
        BindData("time_done",    &time_done);
        BindData("timeout",      &timeout);
        BindData("run_timeout",  &run_timeout);

        BindData("subm_addr",    &subm_addr);
        BindData("subm_port",    &subm_port);
        BindData("subm_timeout", &subm_timeout);

        BindData("worker_node1", &worker_node1);
        BindData("worker_node2", &worker_node2);
        BindData("worker_node3", &worker_node3);
        BindData("worker_node4", &worker_node4);
        BindData("worker_node5", &worker_node5);

        BindData("run_counter",        &run_counter);
        BindData("ret_code",           &ret_code);

        BindData("aff_id", &aff_id);
        BindData("mask",   &mask);

        BindData("input",  &input,  kNetScheduleMaxDBDataSize); // kNetScheduleSplitSize
        BindData("output", &output, kNetScheduleMaxDBDataSize); // kNetScheduleSplitSize

        BindData("err_msg", &err_msg, kNetScheduleMaxDBErrSize);
        BindData("progress_msg", &progress_msg, kNetScheduleMaxDBDataSize);
    }
};


const unsigned kNetScheduleMaxOverflowSize = 1024*1024;
/// BDB table to store infrequently needed job info
///
/// @internal
///
struct SJobInfoDB : public CBDB_File
{
    CBDB_FieldUint4        id;              ///< Job id
    CBDB_FieldString       tags;            ///< Tags for the job
    CBDB_FieldLString      input;           ///< Job input overflow
    CBDB_FieldLString      output;          ///< Job output overflow

    SJobInfoDB()
    {
        BindKey("id", &id);
        BindData("tags",   &tags,   kNetScheduleMaxOverflowSize);
        BindData("input",  &input,  kNetScheduleMaxOverflowSize);
        BindData("output", &output, kNetScheduleMaxOverflowSize);
    }
};


/// Database of vectors of jobs to be deleted. Because deletion
/// occurs in background, with different pace for different tables,
/// and even in different manner (blocks of jobs from main and aux job
/// tables, and whole vectors from several records from affinity and tag
/// tables), we need to maintain several bitvectors with deleted jobs.
///
/// @internal
///
struct SDeletedJobsDB : public CBDB_BvStore<TNSBitVector>
{
    ///< Vector ID, 0 - job table, 1 - tag table, 2 - affinities
    CBDB_FieldUint4 id;

    typedef CBDB_BvStore<TNSBitVector> TParent;

    SDeletedJobsDB()
    {
        DisableNull(); 
        BindKey("id", &id);
    }
};

/// Index of queue database (affinity to jobs)
///
/// @internal
///
struct SQueueAffinityIdx : public CBDB_BvStore<TNSBitVector>
{
    CBDB_FieldUint4 aff_id;

    typedef CBDB_BvStore<TNSBitVector> TParent;

    SQueueAffinityIdx()
    {
        DisableNull(); 
        BindKey("aff_id", &aff_id);
    }
};


/// BDB table to store affinity
///
/// @internal
///
struct SAffinityDictDB : public CBDB_File
{
    CBDB_FieldUint4  aff_id;        ///< Affinity token id
    CBDB_FieldString token;         ///< Affinity token

    SAffinityDictDB()
    {
        DisableNull(); 
        BindKey("aff_id", &aff_id);
        BindData("token", &token, kNetScheduleMaxDBDataSize);
    }
};

/// BDB affinity token index
///
/// @internal
///
struct SAffinityDictTokenIdx : public CBDB_File
{
    CBDB_FieldString       token;
    CBDB_FieldUint4        aff_id;

    SAffinityDictTokenIdx()
    {
        DisableNull(); 
        BindKey("token", &token, kNetScheduleMaxDBDataSize);
        BindData("aff_id", &aff_id);
    }
};


/// BDB tag storage
///
/// @internal
///
struct STagDB : public CBDB_BvStore<TNSBitVector>
{
    CBDB_FieldString key;
    CBDB_FieldString val;

    typedef CBDB_BvStore<TNSBitVector> TParent;

    STagDB()
    {
        DisableNull(); 
        BindKey("key", &key);
        BindKey("val", &val);
    }
};


/// BDB table for storing queue descriptions
///
/// @internal
///
struct SQueueDescriptionDB : public CBDB_File
{
    CBDB_FieldString queue;
    CBDB_FieldUint4  kind; // static - 0 or dynamic - 1
    CBDB_FieldString qclass;
    CBDB_FieldString comment;
    SQueueDescriptionDB()
    {
        DisableNull();
        BindKey("queue", &queue);
        BindData("kind", &kind);
        BindData("qclass", &qclass);
        BindData("comment", &comment);
    }
};


END_NCBI_SCOPE

#endif /* NETSCHEDULE_DB__HPP */
