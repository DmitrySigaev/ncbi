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
 *  File Description: Base data reader interface
 *
 */

#include <ncbi_pch.hpp>
#include <objtools/data_loaders/genbank/reader.hpp>
#include <objtools/data_loaders/genbank/split_parser.hpp>
#include <objtools/data_loaders/genbank/request_result.hpp>

#include <objmgr/annot_name.hpp>
#include <objmgr/annot_type_selector.hpp>
#include <objmgr/objmgr_exception.hpp>
#include <objmgr/impl/snp_annot_info.hpp>
#include <objmgr/impl/tse_info.hpp>
#include <objmgr/impl/tse_chunk_info.hpp>
#include <objmgr/impl/seq_annot_info.hpp>
#include <objmgr/impl/handle_range_map.hpp>

#include <objects/general/Object_id.hpp>
#include <objects/general/Dbtag.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqfeat/Gb_qual.hpp>
#include <objects/seqfeat/Imp_feat.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objects/seqset/Bioseq_set.hpp>
#include <objects/id2/ID2_Blob_Id.hpp>

#include <serial/serial.hpp>
#include <serial/objistr.hpp>
#include <serial/objectinfo.hpp>
#include <serial/objectiter.hpp>
#include <serial/pack_string.hpp>

#include <corelib/ncbiapp.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

static const char* const GENBANK_SECTION = "GENBANK";
static const char* const STRING_PACK_ENV = "SNP_PACK_STRINGS";
static const char* const SNP_SPLIT_ENV = "SNP_SPLIT";
static const char* const SNP_TABLE_ENV = "SNP_TABLE";
static const char* const ENV_YES = "YES";

void SConfigIntValue::x_Initialize(void) const
{
    if ( m_Initialized ) {
        return;
    }
#ifdef _DEBUG
    static SConfigBoolValue var = { "NCBI", "CONFIG_DUMP_VARIABLES" };
    bool dump_variables = (this != &var) && var.GetBool();
#endif
    try {
        if ( m_Section ) {
            CNcbiApplication* app = CNcbiApplication::Instance();
            if ( app ) {
                const string& value =
                    app->GetConfig().Get(m_Section, m_Variable);
                if ( !value.empty() ) {
                    x_SetValue(value.c_str());
#ifdef _DEBUG
                    if ( dump_variables ) {
                        LOG_POST("NCBI_CONFIG: int variable "
                                 " [" << m_Section << "]"
                                 " " << m_Variable <<
                                 " (default: " << m_DefaultValue << ")"
                                 " = " << m_Value <<
                                 " from registry \"" << value << "\"");
                    }
#endif
                    return;
                }
            }
        }
        const char* value;
        if ( m_Section ) {
            string variable;
            variable += m_Section;
            variable += '_';
            variable += m_Variable;
            value = getenv(variable.c_str());
        }
        else {
            value = getenv(m_Variable);
        }
        if ( value ) {
            x_SetValue(value);
#ifdef _DEBUG
            if ( dump_variables ) {
                if ( m_Section ) {
                    LOG_POST("NCBI_CONFIG: int variable "
                             " [" << m_Section << "]"
                             " " << m_Variable <<
                             " (default: " << m_DefaultValue << ")"
                             " = " << m_Value <<
                             " from environment "
                             << m_Section << '_' << m_Variable <<
                             "=\"" << value << "\"");
                }
                else {
                    LOG_POST("NCBI_CONFIG: int variable "
                             " [" << m_Section << "]"
                             " " << m_Variable <<
                             " (default: " << m_DefaultValue << ")"
                             " = " << m_Value <<
                             " from environment "
                             << m_Variable <<
                             "=\"" << value << "\"");
                }
            }
#endif
            return;
        }
    }
    catch ( ... ) {
        // ignored, will use default value
    }
    m_Value = m_DefaultValue;
    m_Initialized = true;
#ifdef _DEBUG
    if ( dump_variables ) {
        LOG_POST("NCBI_CONFIG: int variable "
                 " [" << m_Section << "]"
                 " " << m_Variable <<
                 " (default: " << m_DefaultValue << ")"
                 " = " << m_Value <<
                 " by default");
    }
#endif
}


void SConfigIntValue::x_SetValue(const char* value) const
{
    if ( NStr::CompareNocase(value,"YES") == 0 ||
         NStr::CompareNocase(value,"Y") == 0 ) {
        m_Value = 1;
    }
    else {
        m_Value = NStr::StringToInt(value);
    }
    m_Initialized = true;
}


CReader::CReader(void)
{
}


CReader::~CReader(void)
{
}


int CReader::GetConst(const string& ) const
{
    return 0;
}


bool CReader::TryStringPack(void)
{
    static SConfigBoolValue var = { GENBANK_SECTION, STRING_PACK_ENV, true };
    return CPackString::TryStringPack() && var.GetBool();
}


void CReader::SetSeqEntryReadHooks(CObjectIStream& in)
{
    if ( !TryStringPack() ) {
        return;
    }

    CObjectTypeInfo type;

    type = CObjectTypeInfo(CType<CObject_id>());
    type.FindVariant("str").SetLocalReadHook(in, new CPackStringChoiceHook);

    type = CObjectTypeInfo(CType<CImp_feat>());
    type.FindMember("key").SetLocalReadHook(in,
                                            new CPackStringClassHook(32, 128));

    type = CObjectTypeInfo(CType<CDbtag>());
    type.FindMember("db").SetLocalReadHook(in, new CPackStringClassHook);

    type = CType<CGb_qual>();
    type.FindMember("qual").SetLocalReadHook(in, new CPackStringClassHook);
}


void CReader::SetSNPReadHooks(CObjectIStream& in)
{
    if ( !TryStringPack() ) {
        return;
    }

    CObjectTypeInfo type;

    type = CType<CGb_qual>();
    type.FindMember("qual").SetLocalReadHook(in, new CPackStringClassHook);
    type.FindMember("val").SetLocalReadHook(in,
                                            new CPackStringClassHook(4, 128));

    type = CObjectTypeInfo(CType<CImp_feat>());
    type.FindMember("key").SetLocalReadHook(in,
                                            new CPackStringClassHook(32, 128));

    type = CObjectTypeInfo(CType<CObject_id>());
    type.FindVariant("str").SetLocalReadHook(in, new CPackStringChoiceHook);

    type = CObjectTypeInfo(CType<CDbtag>());
    type.FindMember("db").SetLocalReadHook(in, new CPackStringClassHook);

    type = CObjectTypeInfo(CType<CSeq_feat>());
    type.FindMember("comment").SetLocalReadHook(in, new CPackStringClassHook);
}


void CReader::LoadBlobs(CReaderRequestResult& result,
                        const string& seq_id,
                        TContentsMask mask)
{
    CLoadLockSeq_ids ids(result, seq_id);
    if ( !ids ) {
        ResolveString(result, seq_id);
        if ( !ids ) {
            return;
        }
    }
    if ( ids->size() == 1 ) {
        LoadBlobs(result, *ids->begin(), mask);
    }
}


void CReader::LoadBlobs(CReaderRequestResult& result,
                        const CSeq_id_Handle& seq_id,
                        TContentsMask mask)
{
    CLoadLockBlob_ids ids(result, seq_id);
    if ( !ids ) {
        ResolveSeq_id(result, seq_id);
        if ( !ids ) {
            return;
        }
    }
    LoadBlobs(result, ids, mask);
}


void CReader::LoadBlobs(CReaderRequestResult& result,
                        CLoadLockBlob_ids blobs,
                        TContentsMask mask)
{
    ITERATE ( CLoadInfoBlob_ids, it, *blobs ) {
        const CBlob_Info& info = it->second;
        if ( (info.GetContentsMask() & mask) != 0 ) {
            LoadBlob(result, blobs, it);
        }
    }
}


void CReader::LoadChunk(CReaderRequestResult& /*result*/,
                        const TBlob_id& /*blob_id*/,
                        TChunk_id /*chunk_id*/)
{
    NCBI_THROW(CLoaderException, eLoaderFailed,
               "unexpected chunk load request");
}


/////////////////////////////////////////////////////////////////////////////
// CId1ReaderBase
/////////////////////////////////////////////////////////////////////////////


CId1ReaderBase::CId1ReaderBase(void)
{
}


CId1ReaderBase::~CId1ReaderBase()
{
}


bool CId1ReaderBase::TrySNPSplit(void)
{
    static SConfigBoolValue var = { GENBANK_SECTION, SNP_SPLIT_ENV, true };
    return var.GetBool();
}


bool CId1ReaderBase::TrySNPTable(void)
{
    static SConfigBoolValue var = { GENBANK_SECTION, SNP_TABLE_ENV, true };
    return var.GetBool();
}


void CId1ReaderBase::ResolveString(CReaderRequestResult& result,
                                   const string& seq_id)
{
}


void CId1ReaderBase::ResolveSeq_id(CReaderRequestResult& result,
                                   const CSeq_id_Handle& seq_id)
{
    CLoadLockBlob_ids ids(result, seq_id);
    if ( !ids.IsLoaded() ) {
        CReaderRequestConn conn(result);
        try {
            ResolveSeq_id(ids, *seq_id.GetSeqId(), conn);
            ids.SetLoaded();
        }
        catch ( CLoaderException& exc ) {
            if ( exc.GetErrCode() == exc.ePrivateData ||
                 exc.GetErrCode() == exc.eNoData ) {
                // leave ids empty
                ids.SetLoaded();
            }
            else if ( exc.GetErrCode() == exc.eNoConnection ) {
                throw;
            }
            else {
                // any other error -> reconnect
                LOG_POST(exc.what());
                LOG_POST("GenBank connection failed: Reconnecting....");
                Reconnect(conn);
            }
        }
        catch ( const exception& exc ) {
            LOG_POST(exc.what());
            LOG_POST("GenBank connection failed: Reconnecting....");
            Reconnect(conn);
        }
    }
}


void CId1ReaderBase::ResolveSeq_ids(CReaderRequestResult& result,
                                    const CSeq_id_Handle& seq_id)
{
    CLoadLockSeq_ids ids(result, seq_id);
    if ( !ids.IsLoaded() ) {
        CReaderRequestConn conn(result);
        try {
            ResolveSeq_id(ids, *seq_id.GetSeqId(), conn);
            ids.SetLoaded();
        }
        catch ( CLoaderException& exc ) {
            if ( exc.GetErrCode() == exc.ePrivateData ||
                 exc.GetErrCode() == exc.eNoData ) {
                // leave ids empty
                ids.SetLoaded();
            }
            else if ( exc.GetErrCode() == exc.eNoConnection ) {
                throw;
            }
            else {
                // any other error -> reconnect
                LOG_POST(exc.what());
                LOG_POST("GenBank connection failed: Reconnecting....");
                Reconnect(conn);
            }
        }
        catch ( const exception& exc ) {
            LOG_POST(exc.what());
            LOG_POST("GenBank connection failed: Reconnecting....");
            Reconnect(conn);
        }
    }
}


void CId1ReaderBase::LoadBlob(CReaderRequestResult& result,
                              CLoadLockBlob_ids& blobs,
                              CLoadInfoBlob_ids::const_iterator blob_iter)
{
    const TBlob_id& blob_id = blob_iter->first;
    CLoadLockBlob blob(result, blob_id);
    if ( !blob.IsLoaded() ) {
        if ( IsAnnotBlob_id(blob_id) ) {
            // create special external annotations blob
            CAnnotName name;
            SAnnotTypeSelector type;
            string db_name;
            if ( blob_id.GetSubSat() == eSubSat_SNP ) {
                blob->SetName("SNP");
                name.SetNamed("SNP");
                type.SetFeatSubtype(CSeqFeatData::eSubtype_variation);
                db_name = "Annot:SNP";
            }
            else if ( blob_id.GetSubSat() == eSubSat_CDD ) {
                name.SetNamed("CDDSearch");
                type.SetFeatSubtype(CSeqFeatData::eSubtype_region);
                db_name = "Annot:CDD";
            }
            else if ( blob_id.GetSubSat() == eSubSat_MGC ) {
                type.SetFeatSubtype(CSeqFeatData::eSubtype_misc_difference);
                db_name = "Annot:MGC";
            }
            if ( !db_name.empty() &&
                 type.GetFeatSubtype() != CSeqFeatData::eSubtype_any ) {
                int gi = blob_id.GetSatKey();
                CSeq_id seq_id;
                seq_id.SetGeneral().SetDb(db_name);
                seq_id.SetGeneral().SetTag().SetId(gi);

                CRef<CTSE_Chunk_Info> chunk
                    (new CTSE_Chunk_Info(kSkeleton_ChunkId));
                chunk->x_AddAnnotType(name, type,
                                      CSeq_id_Handle::GetGiHandle(gi),
                                      CTSE_Chunk_Info::TLocationRange::GetWhole());
                chunk->x_AddBioseqPlace(0);
                chunk->x_AddBioseqId(CSeq_id_Handle::GetHandle(seq_id));
                chunk->x_TSEAttach(*blob);
                blob.SetLoaded();
                result.AddTSE_Lock(blob);
                result.UpdateLoadedSet();
                return;
            }
        }
        CReaderRequestConn conn(result);
        try {
            GetBlob(*blob, blob_id, conn);
            blob.SetLoaded();
            if ( !blob->IsUnavailable() ) {
                result.AddTSE_Lock(blob);
                result.UpdateLoadedSet();
            }
        }
        catch ( CLoaderException& exc ) {
            if ( exc.GetErrCode() == exc.ePrivateData ) {
                // leave tse empty
                blob->SetSuppressionLevel(CTSE_Info::eSuppression_private);
                blob.SetLoaded();
            }
            else if ( exc.GetErrCode() == exc.eNoData ) {
                // leave tse empty
                blob->SetSuppressionLevel(CTSE_Info::eSuppression_no_data);
                blob.SetLoaded();
            }
            else if ( exc.GetErrCode() == exc.eNoConnection ) {
                throw;
            }
            else {
                // any other error -> reconnect
                LOG_POST(exc.what());
                LOG_POST("GenBank connection failed: Reconnecting....");
                Reconnect(conn);
            }
        }
        catch ( const exception &exc ) {
            LOG_POST(exc.what());
            LOG_POST("GenBank connection failed: Reconnecting....");
            Reconnect(conn);
        }
    }
}


CReader::TBlobVersion
CId1ReaderBase::GetBlobVersion(CReaderRequestResult& result,
                               const TBlob_id& blob_id)
{
    CLoadLockBlob blob(result, blob_id);
    TBlobVersion version = blob->GetBlobVersion();
    if ( version < 0 ) {
        for ( int retry = 0; version < 0 && retry < 3; ++retry ) {
            CReaderRequestConn conn(result);
            try {
                version = GetVersion(blob_id, conn);
            }
            catch ( CLoaderException& exc ) {
                if ( exc.GetErrCode() == exc.ePrivateData ||
                     exc.GetErrCode() == exc.eNoData ) {
                    // leave version zero
                    version = 0;
                    break;
                }
                else if ( exc.GetErrCode() == exc.eNoConnection ) {
                    throw;
                }
                else {
                    // any other error -> reconnect
                    LOG_POST(exc.what());
                    LOG_POST("GenBank connection failed: Reconnecting....");
                    Reconnect(conn);
                }
            }
            catch ( const exception &exc ) {
                LOG_POST(exc.what());
                LOG_POST("GenBank connection failed: Reconnecting....");
                Reconnect(conn);
            }
        }
        blob->SetBlobVersion(version);
    }
    return version;
}


void CId1ReaderBase::LoadChunk(CReaderRequestResult& result,
                               const TBlob_id& blob_id, TChunk_id chunk_id)
{
    CLoadLockBlob blob(result, blob_id);
    _ASSERT(blob);
    _ASSERT(blob.IsLoaded());
    CTSE_Chunk_Info& chunk_info = blob->GetChunk(chunk_id);
    if ( chunk_info.NotLoaded() ) {
        CInitGuard init(chunk_info, result);
        if ( init ) {
            CReaderRequestConn conn(result);
            try {
                if ( chunk_info.GetChunkId() == kSkeleton_ChunkId && 
                     IsAnnotBlob_id(blob_id) ) {
                    GetBlob(*blob, blob_id, conn);
                }
                else {
                    GetChunk(chunk_info, blob_id, conn);
                }
                chunk_info.SetLoaded();
            }
            catch ( CLoaderException& exc ) {
                if ( exc.GetErrCode() == exc.eNoConnection ) {
                    throw;
                }
                else {
                    // any other error -> reconnect
                    LOG_POST(exc.what());
                    LOG_POST("GenBank connection failed: Reconnecting....");
                    Reconnect(conn);
                }
            }
            catch ( const exception& exc ) {
                LOG_POST(exc.what());
                LOG_POST("GenBank connection failed: Reconnecting....");
                Reconnect(conn);
            }
        }
    }
}


bool CId1ReaderBase::IsSNPBlob_id(const CBlob_id& blob_id)
{
    return blob_id.GetSat() == eSat_SNP &&
        blob_id.GetSubSat() == CID2_Blob_Id::eSub_sat_snp;
}


bool CId1ReaderBase::IsAnnotBlob_id(const CBlob_id& blob_id)
{
    return blob_id.GetSat() == eSat_ANNOT;
}


void CId1ReaderBase::AddSNPBlob_id(CLoadLockBlob_ids& ids, int gi)
{
    CBlob_id blob_id;
    blob_id.SetSat(eSat_SNP);
    blob_id.SetSubSat(CID2_Blob_Id::eSub_sat_snp);
    blob_id.SetSatKey(gi);
    //blob_id.SetVersion(0);
    ids.AddBlob_id(blob_id, fBlobHasExternal);
}


void CId1ReaderBase::GetBlob(CTSE_Info& tse_info,
                             const CBlob_id& blob_id,
                             TConn conn)
{
    if ( IsSNPBlob_id(blob_id) ) {
        GetSNPBlob(tse_info, blob_id, conn);
    }
    else {
        GetTSEBlob(tse_info, blob_id, conn);
    }
}


void CId1ReaderBase::GetChunk(CTSE_Chunk_Info& chunk_info,
                              const CBlob_id& blob_id,
                              TConn conn)
{
    if ( chunk_info.GetChunkId() == kSNP_ChunkId && IsSNPBlob_id(blob_id) ) {
        GetSNPChunk(chunk_info, blob_id, conn);
    }
    else {
        GetTSEChunk(chunk_info, blob_id, conn);
    }
}


void CId1ReaderBase::GetSNPBlob(CTSE_Info& tse_info,
                                const CBlob_id& blob_id,
                                TConn /*conn*/)
{
    CRef<CSeq_entry> seq_entry(new CSeq_entry);
    seq_entry->SetSet().SetSeq_set();
    seq_entry->SetSet().SetId().SetId(kSNP_EntryId);

    // create CTSE_Info
    tse_info.SetSeq_entry(*seq_entry);
    tse_info.SetName("SNP");

    CRef<CTSE_Chunk_Info> info(new CTSE_Chunk_Info(kSNP_ChunkId));

    info->x_AddAnnotPlace(CTSE_Chunk_Info::eBioseq_set, kSNP_EntryId);

    info->x_AddAnnotType(CAnnotName("SNP"),
                         SAnnotTypeSelector(CSeqFeatData::eSubtype_variation),
                         CSeq_id_Handle::GetGiHandle(blob_id.GetSatKey()),
                         CTSE_Chunk_Info::TLocationRange::GetWhole());

    info->x_TSEAttach(tse_info);
}


void CId1ReaderBase::GetTSEChunk(CTSE_Chunk_Info& /*chunk_info*/,
                                 const CBlob_id& /*blob_id*/,
                                 TConn /*conn*/)
{
    NCBI_THROW(CLoaderException, eLoaderFailed,
               "unexpected chunk load request");
}


void CId1ReaderBase::GetSNPChunk(CTSE_Chunk_Info& chunk,
                                 const CBlob_id& blob_id,
                                 TConn conn)
{
    _ASSERT(IsSNPBlob_id(blob_id));
    _ASSERT(chunk.GetChunkId() == kSNP_ChunkId);
    CRef<CSeq_annot_SNP_Info> snp_annot =
        GetSNPAnnot(chunk.GetTSE_Info(), blob_id, conn);
    if ( !chunk.GetTSE_Info().IsUnavailable() ) {
        CRef<CSeq_annot_Info> annot_info(new CSeq_annot_Info(*snp_annot));
        CTSE_Chunk_Info::TPlace place(chunk.eBioseq_set, kSNP_EntryId);
        chunk.x_LoadAnnot(place, annot_info);
    }
}


END_SCOPE(objects)
END_NCBI_SCOPE
