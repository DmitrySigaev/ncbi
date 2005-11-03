/*  $Id$
* ===========================================================================
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
* ===========================================================================
*
*  Author:  Anton Butanaev, Eugene Vasilchenko
*
*  File Description: Data reader from Pubseq_OS
*
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbi_config_value.hpp>
#include <objtools/data_loaders/genbank/readers/pubseqos/reader_pubseq.hpp>
#include <objtools/data_loaders/genbank/readers/pubseqos/reader_pubseq_entry.hpp>
#include <objtools/data_loaders/genbank/readers/pubseqos/reader_pubseq_params.h>
#include <objtools/data_loaders/genbank/readers/readers.hpp> // for entry point
#include <objtools/data_loaders/genbank/request_result.hpp>
#include <objtools/data_loaders/genbank/dispatcher.hpp>

#include <objmgr/objmgr_exception.hpp>
#include <objmgr/impl/tse_info.hpp>

#include <dbapi/driver/exception.hpp>
#include <dbapi/driver/driver_mgr.hpp>
#include <dbapi/driver/drivers.hpp>

#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqset/Seq_entry.hpp>

#include <corelib/ncbicntr.hpp>
#include <corelib/plugin_manager_impl.hpp>

#include <serial/objistrasnb.hpp>
#include <serial/objostrasn.hpp>
#include <serial/serial.hpp>

#include <util/rwstream.hpp>
#include <util/compress/zlib.hpp>
#include <corelib/plugin_manager_store.hpp>

#define ALLOW_GZIPPED 1

#define DEFAULT_DB_SERVER   "PUBSEQ_OS"
#define DEFAULT_DB_USER     "anyone"
#define DEFAULT_DB_PASSWORD "allowed"
#define MAX_MT_CONN         5
#define DEFAULT_NUM_CONN    2
#define DEFAULT_ALLOW_GZIP  true

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

#if !defined(HAVE_SYBASE_REENTRANT) && defined(NCBI_THREADS)
// we have non MT-safe library used in MT application
static CAtomicCounter s_pubseq_readers;
#endif

#ifdef _DEBUG
static int GetDebugLevel(void)
{
    static int s_Value = -1;
    int value = s_Value;
    if ( value < 0 ) {
        value = GetConfigInt("GENBANK", "PUBSEQOS_DEBUG");
        if ( value < 0 ) {
            value = 0;
        }
        s_Value = value;
    }
    return value;
}
#else
# define GetDebugLevel() (0)
#endif

#define RPC_GET_ASN         "id_get_asn"
#define RPC_GET_BLOB_INFO   "id_get_blob_prop"

enum {
    fZipType_gzipped = 2
};

CPubseqReader::CPubseqReader(int max_connections,
                             const string& server,
                             const string& user,
                             const string& pswd)
    : m_Server(server) , m_User(user), m_Password(pswd),
      m_Context(0), m_AllowGzip(false)
{
    if ( max_connections <= 0 ) {
        max_connections = DEFAULT_NUM_CONN;
    }
    if ( m_Server.empty() ) {
        m_Server = DEFAULT_DB_SERVER;
    }
    if ( m_User.empty() ) {
        m_User = DEFAULT_DB_USER;
    }
    if ( m_Password.empty() ) {
        m_Password = DEFAULT_DB_PASSWORD;
    }

#if defined(NCBI_THREADS) && !defined(HAVE_SYBASE_REENTRANT)
    if ( s_pubseq_readers.Add(1) > 1 ) {
        s_pubseq_readers.Add(-1);
        NCBI_THROW(CLoaderException, eNoConnection,
                   "Attempt to open multiple pubseq_readers "
                   "without MT-safe DB library");
    }
#endif

    SetInitialConnections(max_connections);
}


CPubseqReader::CPubseqReader(const TPluginManagerParamTree* params,
                             const string& driver_name)
    : m_Context(0), m_AllowGzip(false)
{
    CConfig conf(params);
    TConn max_connections = conf.GetInt(
        driver_name,
        NCBI_GBLOADER_READER_PUBSEQ_PARAM_NUM_CONN,
        CConfig::eErr_NoThrow,
        DEFAULT_NUM_CONN);
    m_Server = conf.GetString(
        driver_name,
        NCBI_GBLOADER_READER_PUBSEQ_PARAM_SERVER,
        CConfig::eErr_NoThrow,
        DEFAULT_DB_SERVER);
    m_User = conf.GetString(
        driver_name,
        NCBI_GBLOADER_READER_PUBSEQ_PARAM_USER,
        CConfig::eErr_NoThrow,
        DEFAULT_DB_USER);
    m_Password = conf.GetString(
        driver_name,
        NCBI_GBLOADER_READER_PUBSEQ_PARAM_PASSWORD,
        CConfig::eErr_NoThrow,
        DEFAULT_DB_PASSWORD);
#ifdef ALLOW_GZIPPED
    m_AllowGzip = conf.GetBool(
        driver_name,
        NCBI_GBLOADER_READER_PUBSEQ_PARAM_GZIP,
        CConfig::eErr_NoThrow,
        DEFAULT_ALLOW_GZIP);
#endif

#if defined(NCBI_THREADS) && !defined(HAVE_SYBASE_REENTRANT)
    if ( s_pubseq_readers.Add(1) > 1 ) {
        s_pubseq_readers.Add(-1);
        NCBI_THROW(CLoaderException, eNoConnection,
                   "Attempt to open multiple pubseq_readers "
                   "without MT-safe DB library");
    }
#endif

    SetInitialConnections(max_connections);
}

CPubseqReader::~CPubseqReader()
{
#if !defined(HAVE_SYBASE_REENTRANT) && defined(NCBI_THREADS)
    s_pubseq_readers.Add(-1);
#endif
}


int CPubseqReader::GetMaximumConnectionsLimit(void) const
{
#if defined(HAVE_SYBASE_REENTRANT) && defined(NCBI_THREADS)
    return MAX_MT_CONN;
#else
    return 1;
#endif
}


void CPubseqReader::x_AddConnectionSlot(TConn conn)
{
    _ASSERT(!m_Connections.count(conn));
    m_Connections[conn];
}


void CPubseqReader::x_RemoveConnectionSlot(TConn conn)
{
    _VERIFY(m_Connections.erase(conn));
}


void CPubseqReader::x_DisconnectAtSlot(TConn conn)
{
    _ASSERT(m_Connections.count(conn));
    AutoPtr<CDB_Connection>& stream = m_Connections[conn];
    if ( stream ) {
        ERR_POST("CPubseqReader: PubSeqOS GenBank connection failed: "
                 "reconnecting...");
        stream.reset();
    }
}


void CPubseqReader::x_ConnectAtSlot(TConn conn)
{
    x_GetConnection(conn);
}


CDB_Connection* CPubseqReader::x_GetConnection(TConn conn)
{
    _ASSERT(m_Connections.count(conn));
    AutoPtr<CDB_Connection>& stream = m_Connections[conn];
    if ( !stream.get() ) {
        stream.reset(x_NewConnection());
    }
    return stream.get();
}


CDB_Connection* CPubseqReader::x_NewConnection(void)
{
    if ( !m_Context ) {
        C_DriverMgr drvMgr;
        //DBAPI_RegisterDriver_CTLIB(drvMgr);
        //DBAPI_RegisterDriver_DBLIB(drvMgr);
        map<string,string> args;
        args["packet"]="3584"; // 7*512
        string errmsg;
        m_Context = drvMgr.GetDriverContext("ctlib", &errmsg, &args);
        if ( !m_Context ) {
            LOG_POST(errmsg);
#if defined(NCBI_THREADS)
            NCBI_THROW(CLoaderException, eNoConnection,
                       "Cannot create dbapi context");
#else
            m_Context = drvMgr.GetDriverContext("dblib", &errmsg, &args);
            if ( !m_Context ) {
                LOG_POST(errmsg);
                NCBI_THROW(CLoaderException, eNoConnection,
                           "Cannot create dbapi context");
            }
#endif
        }
    }

    AutoPtr<CDB_Connection> conn
        (m_Context->Connect(m_Server, m_User, m_Password, 0));
    
    if ( !conn.get() ) {
        NCBI_THROW(CLoaderException, eNoConnection, "connection failed");
    }

    {{
        AutoPtr<CDB_LangCmd> cmd(conn->LangCmd("set blob_stream on"));
        if ( cmd ) {
            cmd->Send();
        }
    }}

#ifdef ALLOW_GZIPPED
    if ( m_AllowGzip ) {
        AutoPtr<CDB_LangCmd> cmd(conn->LangCmd("set accept gzip"));
        if ( cmd ) {
            cmd->Send();
        }
    }
#endif
    
    return conn.release();
}


// LoadSeq_idGi, LoadSeq_idSeq_ids, and LoadSeq_idBlob_ids
// are implemented here and call the same function because
// PubSeqOS has one RPC call that may suite all needs
// LoadSeq_idSeq_ids works like this only when Seq-id is not gi
// To prevent deadlocks these functions lock Seq-ids before Blob-ids.

bool CPubseqReader::LoadSeq_idGi(CReaderRequestResult& result,
                                 const CSeq_id_Handle& seq_id)
{
    CLoadLockSeq_ids seq_ids(result, seq_id);
    if ( seq_ids->IsLoadedGi() ) {
        return true;
    }

    if ( !GetSeq_idInfo(result, seq_id, seq_ids, seq_ids.GetBlob_ids()) ) {
        return false;
    }
    // gi is always loaded in GetSeq_idInfo()
    _ASSERT(seq_ids->IsLoadedGi());
    return true;
}


bool CPubseqReader::LoadSeq_idSeq_ids(CReaderRequestResult& result,
                                      const CSeq_id_Handle& seq_id)
{
    CLoadLockSeq_ids seq_ids(result, seq_id);
    if ( seq_ids.IsLoaded() ) {
        return true;
    }

    GetSeq_idSeq_ids(result, seq_ids, seq_id);
    SetAndSaveSeq_idSeq_ids(result, seq_id, seq_ids);
    return true;
}


bool CPubseqReader::LoadSeq_idBlob_ids(CReaderRequestResult& result,
                                       const CSeq_id_Handle& seq_id)
{
    CLoadLockSeq_ids seq_ids(result, seq_id);
    CLoadLockBlob_ids& blob_ids = seq_ids.GetBlob_ids();
    if ( blob_ids.IsLoaded() ) {
        return true;
    }
    if ( seq_ids.IsLoaded() &&
         seq_ids->GetState() & CBioseq_Handle::fState_no_data ) {
        // no such seq-id
        blob_ids->SetState(seq_ids->GetState());
        SetAndSaveSeq_idBlob_ids(result, seq_id, blob_ids);
        return true;
    }

    if ( !GetSeq_idInfo(result, seq_id, seq_ids, blob_ids) ) {
        return false;
    }
    // blob_ids are always loaded in GetSeq_idInfo()
    _ASSERT(blob_ids.IsLoaded());
    return true;
}


bool CPubseqReader::GetSeq_idInfo(CReaderRequestResult& result,
                                  const CSeq_id_Handle& seq_id,
                                  CLoadLockSeq_ids& seq_ids,
                                  CLoadLockBlob_ids& blob_ids)
{
    // Get gi by seq-id
    _TRACE("ResolveSeq_id to gi/sat/satkey: " << seq_id.AsString());

    CDB_VarChar asnIn;
    {{
        CNcbiOstrstream oss;
        if ( seq_id.IsGi() ) {
            oss << "Seq-id ::= gi " << seq_id.GetGi();
        }
        else {
            CObjectOStreamAsn ooss(oss);
            ooss << *seq_id.GetSeqId();
        }
        asnIn = CNcbiOstrstreamToString(oss);
    }}

    int result_count = 0;
    CConn conn(this);
    {{
        CDB_Connection* db_conn = x_GetConnection(conn);

        AutoPtr<CDB_RPCCmd> cmd(db_conn->RPC("id_gi_by_seqid_asn", 1));
        cmd->SetParam("@asnin", &asnIn);
        cmd->Send();
    
        while(cmd->HasMoreResults()) {
            AutoPtr<CDB_Result> dbr(cmd->Result());
            if ( !dbr.get()  ||  dbr->ResultType() != eDB_RowResult) {
                continue;
            }
        
            while ( dbr->Fetch() ) {
                CDB_Int giGot;
                CDB_Int satGot;
                CDB_Int satKeyGot;
                CDB_Int extFeatGot;

                _TRACE("next fetch: " << dbr->NofItems() << " items");
                ++result_count;
                for ( unsigned pos = 0; pos < dbr->NofItems(); ++pos ) {
                    const string& name = dbr->ItemName(pos);
                    _TRACE("next item: " << name);
                    if (name == "gi") {
                        dbr->GetItem(&giGot);
                        _TRACE("gi: "<<giGot.Value());
                    }
                    else if (name == "sat" ) {
                        dbr->GetItem(&satGot);
                        _TRACE("sat: "<<satGot.Value());
                    }
                    else if(name == "sat_key") {
                        dbr->GetItem(&satKeyGot);
                        _TRACE("sat_key: "<<satKeyGot.Value());
                    }
                    else if(name == "extra_feat" || name == "ext_feat") {
                        dbr->GetItem(&extFeatGot);
#ifdef _DEBUG
                        if ( extFeatGot.IsNULL() ) {
                            _TRACE("ext_feat = NULL");
                        }
                        else {
                            _TRACE("ext_feat = "<<extFeatGot.Value());
                        }
#endif
                    }
                    else {
                        dbr->SkipItem();
                    }
                }

                int gi = giGot.Value();
                int sat = satGot.Value();
                int sat_key = satKeyGot.Value();
                
                if ( GetDebugLevel() >= 5 ) {
                    NcbiCout << "CPubseqReader::ResolveSeq_id"
                        "(" << seq_id.AsString() << ")"
                        " gi=" << gi <<
                        " sat=" << sat <<
                        " satkey=" << sat_key <<
                        " extfeat=";
                    if ( extFeatGot.IsNULL() ) {
                        NcbiCout << "NULL";
                    }
                    else {
                        NcbiCout << extFeatGot.Value();
                    }
                    NcbiCout << NcbiEndl;
                }

                if ( !blob_ids.IsLoaded() ) {
                    if ( CProcessor::TrySNPSplit() && sat != eSat_ANNOT ) {
                        // main blob
                        CBlob_id blob_id;
                        blob_id.SetSat(sat);
                        blob_id.SetSatKey(sat_key);
                        blob_ids.AddBlob_id(blob_id, fBlobHasAllLocal);
                        if ( !extFeatGot.IsNULL() ) {
                            int ext_feat = extFeatGot.Value();
                            while ( ext_feat ) {
                                int bit = ext_feat & ~(ext_feat-1);
                                ext_feat -= bit;
                                blob_id.SetSat(eSat_ANNOT);
                                blob_id.SetSatKey(gi);
                                blob_id.SetSubSat(bit);
                                blob_ids.AddBlob_id(blob_id, fBlobHasExtAnnot);
                            }
                        }
                    }
                    else {
                        // whole blob
                        CBlob_id blob_id;
                        blob_id.SetSat(sat);
                        blob_id.SetSatKey(sat_key);
                        if ( !extFeatGot.IsNULL() ) {
                            blob_id.SetSubSat(extFeatGot.Value());
                        }
                        blob_ids.AddBlob_id(blob_id, fBlobHasAllLocal);
                    }
                    SetAndSaveSeq_idBlob_ids(result, seq_id, blob_ids);
                }

                if ( giGot.IsNULL() || gi == 0 ) {
                    // no gi -> only one Seq-id - the one used as argument
                    if ( !seq_ids.IsLoaded() ) {
                        seq_ids.AddSeq_id(seq_id);
                        SetAndSaveSeq_idSeq_ids(result, seq_id, seq_ids);
                    }
                }
                else {
                    // we've got gi
                    if ( !seq_ids->IsLoadedGi() ) {
                        SetAndSaveSeq_idGi(result, seq_id, seq_ids,
                                           giGot.Value());
                    }
                }
            }
        }
    }}
    conn.Release();
    return result_count > 0;
}


void CPubseqReader::GetSeq_idBlob_ids(CReaderRequestResult& result,
                                      CLoadLockBlob_ids& ids,
                                      const CSeq_id_Handle& seq_id)
{
    NCBI_THROW(CLoaderException, eLoaderFailed, "invalid call");
}


void CPubseqReader::GetSeq_idSeq_ids(CReaderRequestResult& result,
                                     CLoadLockSeq_ids& ids,
                                     const CSeq_id_Handle& seq_id)
{
    if ( ids.IsLoaded() ) {
        return;
    }

    if ( seq_id.Which() == CSeq_id::e_Gi ) {
        GetGiSeq_ids(result, seq_id, ids);
        return;
    }

    m_Dispatcher->LoadSeq_idGi(result, seq_id);
    if ( ids.IsLoaded() ) { // may be loaded as extra information for gi
        return;
    }
    int gi = ids->GetGi();
    if ( !gi ) {
        // no gi -> no Seq-ids
        return;
    }

    CSeq_id_Handle gi_handle = CSeq_id_Handle::GetGiHandle(gi);
    CLoadLockSeq_ids gi_ids(result, gi_handle);
    m_Dispatcher->LoadSeq_idSeq_ids(result, gi_handle);
    
    // copy Seq-id list from gi to original seq-id
    ids->m_Seq_ids = gi_ids->m_Seq_ids;
    ids->SetState(gi_ids->GetState());
}


namespace {
    I_BaseCmd* x_SendRequest2(const CBlob_id& blob_id,
                              CDB_Connection* db_conn,
                              const char* rpc)
    {
        string str = rpc;
        str += " ";
        str += NStr::IntToString(blob_id.GetSatKey());
        str += ",";
        str += NStr::IntToString(blob_id.GetSat());
        str += ",";
        str += NStr::IntToString(blob_id.GetSubSat());
        AutoPtr<I_BaseCmd> cmd(db_conn->LangCmd(str));
        cmd->Send();
        return cmd.release();
    }
    

    class CDB_Result_Reader : public IReader
    {
    public:
        CDB_Result_Reader(CDB_Result* db_result)
            : m_DB_Result(db_result)
            {
            }
    
        ERW_Result Read(void*   buf,
                        size_t  count,
                        size_t* bytes_read)
            {
                if ( !count ) {
                    if ( bytes_read ) {
                        *bytes_read = 0;
                    }
                    return eRW_Success;
                }
                size_t ret;
                while ( (ret = m_DB_Result->ReadItem(buf, count)) == 0 ) {
                    if ( !m_DB_Result->Fetch() )
                        break;
                }
                if ( bytes_read ) {
                    *bytes_read = ret;
                }
                return ret? eRW_Success: eRW_Eof;
            }
        ERW_Result PendingCount(size_t* /*count*/)
            {
                return eRW_NotImplemented;
            }

    private:
        CDB_Result* m_DB_Result;
    };
}


void CPubseqReader::GetGiSeq_ids(CReaderRequestResult& /*result*/,
                                 const CSeq_id_Handle& seq_id,
                                 CLoadLockSeq_ids& ids)
{
    _ASSERT(seq_id.Which() == CSeq_id::e_Gi);
    int gi;
    if ( seq_id.IsGi() ) {
        gi = seq_id.GetGi();
    }
    else {
        gi = seq_id.GetSeqId()->GetGi();
    }
    if ( gi == 0 ) {
        return;
    }

    _TRACE("ResolveGi to Seq-ids: " << gi);

    CConn conn(this);
    {{
        CDB_Connection* db_conn = x_GetConnection(conn);
    
        AutoPtr<CDB_RPCCmd> cmd(db_conn->RPC("id_seqid4gi", 2));
        CDB_Int giIn = gi;
        CDB_TinyInt binIn = 1;
        cmd->SetParam("@gi", &giIn);
        cmd->SetParam("@bin", &binIn);
        cmd->Send();
    
        bool not_found = false;
        int id_count = 0;
        while ( cmd->HasMoreResults() ) {
            AutoPtr<CDB_Result> dbr(cmd->Result());
            if ( !dbr.get() ) {
                continue;
            }
            
            if ( dbr->ResultType() == eDB_StatusResult ) {
                dbr->Fetch();
                CDB_Int v;
                dbr->GetItem(&v);
                int status = v.Value();
                if ( status == 100 ) {
                    // gi does not exist
                    not_found = true;
                }
                break;
            }
        
            if ( dbr->ResultType() != eDB_RowResult ) {
                continue;
            }

            while ( dbr->Fetch() ) {
                _TRACE("next fetch: " << dbr->NofItems() << " items");
                for ( unsigned pos = 0; pos < dbr->NofItems(); ++pos ) {
                    const string& name = dbr->ItemName(pos);
                    _TRACE("next item: " << name);
                    if ( name == "seqid" ) {
                        CDB_Result_Reader reader(dbr.get());
                        CRStream stream(&reader);
                        CObjectIStreamAsnBinary in(stream);
                        CSeq_id id;
                        while ( in.HaveMoreData() ) {
                            in >> id;
                            ids.AddSeq_id(id);
                            ++id_count;
                        }
                    }
                    else {
                        dbr->SkipItem();
                    }
                }
            }
        }
        if ( id_count == 0 && !not_found ) {
            // artificially add argument Seq-id if empty set was received
            ids.AddSeq_id(seq_id);
        }
    }}
    conn.Release();
}


void CPubseqReader::GetBlobVersion(CReaderRequestResult& result, 
                                   const CBlob_id& blob_id)
{
    try {
        CConn conn(this);
        {{
            CDB_Connection* db_conn = x_GetConnection(conn);
            AutoPtr<I_BaseCmd> cmd
                (x_SendRequest2(blob_id, db_conn, RPC_GET_BLOB_INFO));
            x_ReceiveData(result, blob_id, *cmd, false);
        }}
        conn.Release();
        if ( !blob_id.IsMainBlob() ) {
            CLoadLockBlob blob(result, blob_id);
            if ( !blob.IsSetBlobVersion() ) {
                SetAndSaveBlobVersion(result, blob_id, 0);
            }
        }
    }
    catch ( ... ) {
        if ( !blob_id.IsMainBlob() ) {
            SetAndSaveBlobVersion(result, blob_id, 0);
            return;
        }
        throw;
    }
}


void CPubseqReader::GetBlob(CReaderRequestResult& result,
                            const TBlobId& blob_id,
                            TChunkId chunk_id)
{
    CConn conn(this);
    {{
        CDB_Connection* db_conn = x_GetConnection(conn);
        AutoPtr<I_BaseCmd> cmd(x_SendRequest(blob_id, db_conn, RPC_GET_ASN));
        pair<AutoPtr<CDB_Result>, int> dbr
            (x_ReceiveData(result, blob_id, *cmd, true));
        if ( dbr.first ) {
            CDB_Result_Reader reader(dbr.first.get());
            CRStream stream(&reader);
            CProcessor::EType processor_type;
            if ( blob_id.GetSubSat() == eSubSat_SNP ) {
                processor_type = CProcessor::eType_Seq_entry_SNP;
            }
            else {
                processor_type = CProcessor::eType_Seq_entry;
            }
            if ( dbr.second & fZipType_gzipped ) {
                //LOG_POST("Compressed blob: " << blob_id.ToString());
                CCompressionIStream unzip(stream,
                                          new CZipStreamDecompressor,
                                          CCompressionIStream::fOwnProcessor);
                m_Dispatcher->GetProcessor(processor_type)
                    .ProcessStream(result, blob_id, chunk_id, unzip);
                //LOG_POST("Compressed blob: " << blob_id.ToString() << " read.");
            }
            else {
                //LOG_POST("Non-compressed blob: " << blob_id.ToString());
                m_Dispatcher->GetProcessor(processor_type)
                    .ProcessStream(result, blob_id, chunk_id, stream);
                //LOG_POST("Non-compressed blob: " << blob_id.ToString() << " read.");
            }
        }
        else {
            SetAndSaveNoBlob(result, blob_id, chunk_id);
        }
    }}
    conn.Release();
}


I_BaseCmd* CPubseqReader::x_SendRequest(const CBlob_id& blob_id,
                                        CDB_Connection* db_conn,
                                        const char* rpc)
{
    AutoPtr<CDB_RPCCmd> cmd(db_conn->RPC(rpc, 3));
    CDB_SmallInt satIn(blob_id.GetSat());
    CDB_Int satKeyIn(blob_id.GetSatKey());
    CDB_Int ext_feat(blob_id.GetSubSat());

    _TRACE("x_SendRequest: "<<blob_id.ToString());

    cmd->SetParam("@sat_key", &satKeyIn);
    cmd->SetParam("@sat", &satIn);
    cmd->SetParam("@ext_feat", &ext_feat);
    cmd->Send();
    return cmd.release();
}


pair<AutoPtr<CDB_Result>, int>
CPubseqReader::x_ReceiveData(CReaderRequestResult& result,
                             const TBlobId& blob_id,
                             I_BaseCmd& cmd,
                             bool force_blob)
{
    pair<AutoPtr<CDB_Result>, int> ret;

    enum {
        kState_dead = 125
    };
    TBlobState blob_state = 0;

    CLoadLockBlob blob(result, blob_id);

    // new row
    while( !ret.first && cmd.HasMoreResults() ) {
        _TRACE("next result");
        if ( cmd.HasFailed() ) {
            break;
        }
        
        AutoPtr<CDB_Result> dbr(cmd.Result());
        if ( !dbr.get() || dbr->ResultType() != eDB_RowResult ) {
            continue;
        }
        
        while ( !ret.first && dbr->Fetch() ) {
            _TRACE("next fetch: " << dbr->NofItems() << " items");
            for ( unsigned pos = 0; pos < dbr->NofItems(); ++pos ) {
                const string& name = dbr->ItemName(pos);
                _TRACE("next item: " << name);
                if ( name == "confidential" ) {
                    CDB_Int v;
                    dbr->GetItem(&v);
                    _TRACE("confidential: "<<v.Value());
                    if ( v.Value() ) {
                        blob_state |=
                            CBioseq_Handle::fState_confidential |
                            CBioseq_Handle::fState_no_data;
                    }
                }
                else if ( name == "suppress" ) {
                    CDB_Int v;
                    dbr->GetItem(&v);
                    _TRACE("suppress: "<<v.Value());
                    if ( v.Value() ) {
                        blob_state |= (v.Value() & 4)
                            ? CBioseq_Handle::fState_suppress_temp
                            : CBioseq_Handle::fState_suppress_perm;
                    }
                }
                else if ( name == "override" ) {
                    CDB_Int v;
                    dbr->GetItem(&v);
                    _TRACE("withdrawn: "<<v.Value());
                    if ( v.Value() ) {
                        blob_state |=
                            CBioseq_Handle::fState_withdrawn |
                            CBioseq_Handle::fState_no_data;
                    }
                }
                else if ( name == "last_touched_m" ) {
                    CDB_Int v;
                    dbr->GetItem(&v);
                    _TRACE("version: " << v.Value());
                    m_Dispatcher->SetAndSaveBlobVersion(result, blob_id,
                                                        v.Value());
                }
                else if ( name == "state" ) {
                    CDB_Int v;
                    dbr->GetItem(&v);
                    _TRACE("state: "<<v.Value());
                    if ( v.Value() == kState_dead ) {
                        blob_state |= CBioseq_Handle::fState_dead;
                    }
                }
                else if ( name == "zip_type" ) {
                    CDB_Int v;
                    dbr->GetItem(&v);
                    _TRACE("zip_type: "<<v.Value());
                    ret.second = v.Value();
                }
                else if ( name == "asn1" ) {
                    ret.first.reset(dbr.release());
                    break;
                }
                else {
#ifdef _DEBUG
                    AutoPtr<CDB_Object> item(dbr->GetItem(0));
                    _TRACE("item type: " << item->GetType());
                    switch ( item->GetType() ) {
                    case eDB_Int:
                    case eDB_SmallInt:
                    case eDB_TinyInt:
                    {
                        CDB_Int v;
                        v.AssignValue(*item);
                        _TRACE("item value: " << v.Value());
                        break;
                    }
                    case eDB_VarChar:
                    {
                        CDB_VarChar v;
                        v.AssignValue(*item);
                        _TRACE("item value: " << v.Value());
                        break;
                    }
                    default:
                        break;
                    }
#else
                    dbr->SkipItem();
#endif
                }
            }
        }
    }
    if ( !ret.first && force_blob ) {
        // no data
        blob_state |= CBioseq_Handle::fState_no_data;
    }
    m_Dispatcher->SetAndSaveBlobState(result, blob_id, blob, blob_state);
    return ret;
}

END_SCOPE(objects)

void GenBankReaders_Register_Pubseq(void)
{
    RegisterEntryPoint<objects::CReader>(NCBI_EntryPoint_ReaderPubseqos);
}


/// Class factory for Pubseq reader
///
/// @internal
///
class CPubseqReaderCF : 
    public CSimpleClassFactoryImpl<objects::CReader,
                                   objects::CPubseqReader>
{
public:
    typedef CSimpleClassFactoryImpl<objects::CReader,
                                    objects::CPubseqReader> TParent;
public:
    CPubseqReaderCF()
        : TParent(NCBI_GBLOADER_READER_PUBSEQ_DRIVER_NAME, 0) {}

    ~CPubseqReaderCF() {}

    objects::CReader* 
    CreateInstance(const string& driver  = kEmptyStr,
                   CVersionInfo version =
                   NCBI_INTERFACE_VERSION(objects::CReader),
                   const TPluginManagerParamTree* params = 0) const
    {
        objects::CReader* drv = 0;
        if ( !driver.empty()  &&  driver != m_DriverName ) {
            return 0;
        }
        if (version.Match(NCBI_INTERFACE_VERSION(objects::CReader)) 
                            != CVersionInfo::eNonCompatible) {
            drv = new objects::CPubseqReader(params, driver);
        }
        return drv;
    }
};


void NCBI_EntryPoint_ReaderPubseqos(
     CPluginManager<objects::CReader>::TDriverInfoList&   info_list,
     CPluginManager<objects::CReader>::EEntryPointRequest method)
{
    CHostEntryPointImpl<CPubseqReaderCF>::
        NCBI_EntryPointImpl(info_list, method);
}


void NCBI_EntryPoint_xreader_pubseqos(
     CPluginManager<objects::CReader>::TDriverInfoList&   info_list,
     CPluginManager<objects::CReader>::EEntryPointRequest method)
{
    NCBI_EntryPoint_ReaderPubseqos(info_list, method);
}


END_NCBI_SCOPE
