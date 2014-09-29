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
 * Author: Eugene Vasilchenko
 *
 * File Description: WGS file data loader
 *
 * ===========================================================================
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>

#include <objects/general/general__.hpp>
#include <objects/seqloc/seqloc__.hpp>
#include <objects/seq/seq__.hpp>
#include <objects/seqres/seqres__.hpp>

#include <objmgr/impl/data_source.hpp>
#include <objmgr/impl/tse_loadlock.hpp>
#include <objmgr/impl/tse_chunk_info.hpp>
#include <objmgr/impl/tse_split_info.hpp>
#include <objmgr/data_loader_factory.hpp>
#include <corelib/plugin_manager_impl.hpp>
#include <corelib/plugin_manager_store.hpp>
#include <serial/objistr.hpp>
#include <serial/serial.hpp>

#include <sra/error_codes.hpp>
#include <sra/readers/ncbi_traces_path.hpp>
#include <sra/data_loaders/wgs/wgsloader.hpp>
#include <sra/data_loaders/wgs/impl/wgsloader_impl.hpp>

#include <util/sequtil/sequtil_manip.hpp>

#include <algorithm>
#include <cmath>
#include <sra/error_codes.hpp>

BEGIN_NCBI_SCOPE

#define NCBI_USE_ERRCODE_X   WGSLoader
NCBI_DEFINE_ERR_SUBCODE_X(10);

BEGIN_SCOPE(objects)

class CDataLoader;

static const int kTSEId = 1;
static const int kMainChunkId = -1;

NCBI_PARAM_DECL(int, WGS_LOADER, DEBUG);
NCBI_PARAM_DEF_EX(int, WGS_LOADER, DEBUG, 0,
                  eParam_NoThread, WGS_LOADER_DEBUG);

static int GetDebugLevel(void)
{
    static NCBI_PARAM_TYPE(WGS_LOADER, DEBUG) s_Value;
    return s_Value.Get();
}


NCBI_PARAM_DECL(bool, WGS_LOADER, MASTER_DESCR);
NCBI_PARAM_DEF_EX(bool, WGS_LOADER, MASTER_DESCR, true,
                  eParam_NoThread, WGS_LOADER_MASTER_DESCR);

static bool GetMasterDescrParam(void)
{
    return NCBI_PARAM_TYPE(WGS_LOADER, MASTER_DESCR)().Get();
}


NCBI_PARAM_DECL(size_t, WGS_LOADER, GC_SIZE);
NCBI_PARAM_DEF_EX(size_t, WGS_LOADER, GC_SIZE, 10,
                  eParam_NoThread, WGS_LOADER_GC_SIZE);

static size_t GetGCSize(void)
{
    static NCBI_PARAM_TYPE(WGS_LOADER, GC_SIZE) s_Value;
    return s_Value.Get();
}


NCBI_PARAM_DECL(string, WGS_LOADER, VOL_PATH);
NCBI_PARAM_DEF_EX(string, WGS_LOADER, VOL_PATH, "",
                  eParam_NoThread, WGS_LOADER_VOL_PATH);

static string GetWGSVolPath(void)
{
    static NCBI_PARAM_TYPE(WGS_LOADER, VOL_PATH) s_Value;
    return s_Value.Get();
}


/////////////////////////////////////////////////////////////////////////////
// CWGSBlobId
/////////////////////////////////////////////////////////////////////////////

CWGSBlobId::CWGSBlobId(CTempString str)
{
    FromString(str);
}


CWGSBlobId::CWGSBlobId(const CWGSFileInfo::SAccFileInfo& info)
    : m_WGSPrefix(info.file->GetWGSPrefix()),
      m_SeqType(info.seq_type),
      m_RowId(info.row_id)
{
}


CWGSBlobId::~CWGSBlobId(void)
{
}


string CWGSBlobId::ToString(void) const
{
    CNcbiOstrstream out;
    out << m_WGSPrefix << '.';
    if ( m_SeqType ) {
        out << m_SeqType;
    }
    out << m_RowId;
    return CNcbiOstrstreamToString(out);
}


void CWGSBlobId::FromString(CTempString str)
{
    SIZE_TYPE dot = str.rfind('.');
    if ( dot == NPOS ) {
        NCBI_THROW_FMT(CSraException, eOtherError,
                       "Bad CWGSBlobId: "<<str);
    }
    m_WGSPrefix = str.substr(0, dot);
    SIZE_TYPE pos = dot+1;
    if ( str[pos] == 'S' || str[pos] == 'P' ) {
        m_SeqType = str[pos++];
    }
    else {
        m_SeqType = '\0';
    }
    m_RowId = NStr::StringToNumeric<Uint8>(str.substr(dot+1));
}


bool CWGSBlobId::operator<(const CBlobId& id) const
{
    const CWGSBlobId& wgs2 = dynamic_cast<const CWGSBlobId&>(id);
    if ( int diff = NStr::CompareNocase(m_WGSPrefix, wgs2.m_WGSPrefix) ) {
        return diff < 0;
    }
    if ( m_SeqType != wgs2.m_SeqType ) {
        return m_SeqType < wgs2.m_SeqType;
    }
    return m_RowId < wgs2.m_RowId;
}


bool CWGSBlobId::operator==(const CBlobId& id) const
{
    const CWGSBlobId& wgs2 = dynamic_cast<const CWGSBlobId&>(id);
    return m_RowId == wgs2.m_RowId &&
        m_SeqType == wgs2.m_SeqType &&
        m_WGSPrefix == wgs2.m_WGSPrefix;
}


/////////////////////////////////////////////////////////////////////////////
// CWGSDataLoader_Impl
/////////////////////////////////////////////////////////////////////////////


CWGSDataLoader_Impl::CWGSDataLoader_Impl(
    const CWGSDataLoader::SLoaderParams& params)
    : m_WGSVolPath(params.m_WGSVolPath),
      m_FoundFiles(GetGCSize()),
      m_AddWGSMasterDescr(GetMasterDescrParam())
{
    if ( m_WGSVolPath.empty() && params.m_WGSFiles.empty() ) {
        m_WGSVolPath = GetWGSVolPath();
    }
    ITERATE (vector<string>, it, params.m_WGSFiles) {
        CRef<CWGSFileInfo> info(new CWGSFileInfo(*this, *it));
        if ( !m_FixedFiles.insert(TFixedFiles::value_type(info->GetWGSPrefix(),
                                                          info)).second ) {
            NCBI_THROW_FMT(CSraException, eOtherError,
                           "Duplicated fixed WGS prefix: "<<
                           info->GetWGSPrefix());
        }
    }
}


CWGSDataLoader_Impl::~CWGSDataLoader_Impl(void)
{
}


CRef<CWGSFileInfo> CWGSDataLoader_Impl::GetWGSFile(const string& prefix)
{
    if ( !m_FixedFiles.empty() ) {
        // no dynamic WGS accessions
        TFixedFiles::iterator it = m_FixedFiles.find(prefix);
        if ( it != m_FixedFiles.end() ) {
            return it->second;
        }
        return null;
    }
    CMutexGuard guard(m_Mutex);
    TFoundFiles::iterator it = m_FoundFiles.find(prefix);
    if ( it != m_FoundFiles.end() ) {
        return it->second;
    }
    try {
        CRef<CWGSFileInfo> info(new CWGSFileInfo(*this, prefix));
        m_FoundFiles[prefix] = info;
        return info;
    }
    catch ( CSraException& exc ) {
        if ( exc.GetErrCode() == CSraException::eNotFoundDb ) {
            // no such WGS table
            return null;
        }
        throw;
    }
}


CWGSFileInfo::SAccFileInfo
CWGSDataLoader_Impl::GetFileInfo(const string& acc)
{
    CWGSFileInfo::SAccFileInfo ret;
    CSeq_id::EAccessionInfo type = CSeq_id::IdentifyAccession(acc);
    switch ( type & CSeq_id::eAcc_division_mask ) {
        // accepted accession types
    case CSeq_id::eAcc_wgs:
    case CSeq_id::eAcc_wgs_intermed:
    case CSeq_id::eAcc_tsa:
        break;
    default:
        return ret;
    }
    SIZE_TYPE prefix_len = NStr::StartsWith(acc, "NZ_")? 9: 6;
    if ( acc.size() <= prefix_len ) {
        return ret;
    }
    string prefix = acc.substr(0, prefix_len);
    for ( SIZE_TYPE i = prefix_len-6; i < prefix_len-2; ++i ) {
        if ( !isalpha(acc[i]&0xff) ) {
            return ret;
        }
    }
    for ( SIZE_TYPE i = prefix_len-2; i < prefix_len; ++i ) {
        if ( !isdigit(acc[i]&0xff) ) {
            return ret;
        }
    }
    SIZE_TYPE row_pos = prefix_len;
    if ( acc[row_pos] == 'S' || acc[row_pos] == 'P' ) {
        ret.seq_type = acc[row_pos++];
    }
    if ( acc.size() <= row_pos ) {
        return ret;
    }
    ret.row_id = NStr::StringToNumeric<Uint8>(acc.substr(row_pos),
                                              NStr::fConvErr_NoThrow);
    if ( !ret.row_id && errno ) {
        return ret;
    }
    if ( CRef<CWGSFileInfo> info = GetWGSFile(prefix) ) {
        SIZE_TYPE row_digits = acc.size() - row_pos;
        if ( info->IsValidRowId(ret, row_digits) ) {
            ret.file = info;
            return ret;
        }
    }
    return ret;
}


CWGSFileInfo::SAccFileInfo
CWGSDataLoader_Impl::GetFileInfo(const CSeq_id_Handle& idh)
{
    CWGSFileInfo::SAccFileInfo ret;
    if ( idh.IsGi() ) {
        TGi gi = idh.GetGi();
        if ( !m_FoundFiles.empty() ) {
            ITERATE ( TFixedFiles, it, m_FixedFiles ) {
                CWGSGiIterator gi_it(it->second->m_WGSDb, gi);
                if ( gi_it ) {
                    ret.file = it->second;
                    ret.row_id = gi_it.GetRowId();
                    ret.seq_type = gi_it.GetSeqType() == gi_it.eProt? 'P': '\0';
                    ret.has_version = false;
                    return ret;
                }
            }
        }
        else {
            if ( m_GiResolver.IsValid() ) {
                CWGSGiResolver::TAccessionList accs = m_GiResolver.FindAll(gi);
                ITERATE ( CWGSGiResolver::TAccessionList, it, accs ) {
                    CRef<CWGSFileInfo> file = GetWGSFile(*it);
                    if ( !file ) {
                        continue;
                    }
                    CWGSGiIterator gi_it(file->m_WGSDb, gi);
                    if ( gi_it ) {
                        ret.file = file;
                        ret.row_id = gi_it.GetRowId();
                        ret.seq_type = gi_it.GetSeqType() == gi_it.eProt? 'P': '\0';
                        ret.has_version = false;
                        return ret;
                    }
                }
            }
            else {
                ITERATE ( TFoundFiles, it, m_FoundFiles ) {
                    CWGSGiIterator gi_it(it->second->m_WGSDb, gi);
                    if ( gi_it ) {
                        ret.file = it->second;
                        ret.row_id = gi_it.GetRowId();
                        ret.seq_type = gi_it.GetSeqType() == gi_it.eProt? 'P': '\0';
                        ret.has_version = false;
                        return ret;
                    }
                }
            }
        }
        return ret;
    }
    switch ( idh.Which() ) { // shortcut
    case CSeq_id::e_not_set:
    case CSeq_id::e_Local:
    case CSeq_id::e_Gibbsq:
    case CSeq_id::e_Gibbmt:
    case CSeq_id::e_Giim:
    case CSeq_id::e_Patent:
    case CSeq_id::e_General:
    case CSeq_id::e_Pdb:
        return ret;
    default:
        break;
    }
    CConstRef<CSeq_id> id = idh.GetSeqId();
    const CTextseq_id* text_id = id->GetTextseq_Id();
    if ( !text_id ) {
        return ret;
    }
    ret = GetFileInfo(text_id->GetAccession());
    if ( !ret ) {
        return ret;
    }
    if ( text_id->IsSetVersion() ) {
        ret.has_version = true;
        ret.version = text_id->GetVersion();
        if ( !ret.file->IsCorrectVersion(ret) ) {
            ret.file = null;
        }
    }
    return ret;
}


CRef<CWGSFileInfo> CWGSDataLoader_Impl::GetFileInfo(const CWGSBlobId& blob_id)
{
    return GetWGSFile(blob_id.m_WGSPrefix);
}


CRef<CWGSBlobId> CWGSDataLoader_Impl::GetBlobId(const CSeq_id_Handle& idh)
{
    // return blob-id of blob with sequence
    if ( CWGSFileInfo::SAccFileInfo info = GetFileInfo(idh) ) {
        return Ref(new CWGSBlobId(info));
    }
    return null;
}


CWGSSeqIterator
CWGSDataLoader_Impl::GetSeqIterator(const CSeq_id_Handle& idh,
                                    CWGSScaffoldIterator* iter2_ptr)
{
    // return blob-id of blob with sequence
    if ( CWGSFileInfo::SAccFileInfo info = GetFileInfo(idh) ) {
        if ( info.seq_type == 'S' ) {
            if ( iter2_ptr ) {
                *iter2_ptr = CWGSScaffoldIterator(info.file->GetDb(),
                                                  info.row_id);
            }
            return CWGSSeqIterator();
        }
        else if ( info.seq_type == 'P' ) {
            ERR_POST("not supported");
        }
        else {
            return CWGSSeqIterator(info.file->GetDb(), info.row_id,
                                   CWGSSeqIterator::eIncludeWithdrawn);
        }
    }
    return CWGSSeqIterator();
}


CTSE_LoadLock CWGSDataLoader_Impl::GetBlobById(CDataSource* data_source,
                                               const CWGSBlobId& blob_id)
{
    CDataLoader::TBlobId loader_blob_id(&blob_id);
    CTSE_LoadLock load_lock = data_source->GetTSE_LoadLock(loader_blob_id);
    if ( !load_lock.IsLoaded() ) {
        LoadBlob(blob_id, load_lock);
        load_lock.SetLoaded();
    }
    return load_lock;
}


CDataLoader::TTSE_LockSet
CWGSDataLoader_Impl::GetRecords(CDataSource* data_source,
                                const CSeq_id_Handle& idh,
                                CDataLoader::EChoice choice)
{
    CDataLoader::TTSE_LockSet locks;
    if ( choice == CDataLoader::eExtAnnot ||
         choice == CDataLoader::eExtFeatures ||
         choice == CDataLoader::eExtAlign ||
         choice == CDataLoader::eExtGraph ||
         choice == CDataLoader::eOrphanAnnot ) {
        // WGS loader doesn't provide external annotations
        return locks;
    }
    // return blob-id of blob with annotations and possibly with sequence

    if ( CRef<CWGSBlobId> blob_id = GetBlobId(idh) ) {
        CDataLoader::TTSE_Lock lock = GetBlobById(data_source, *blob_id);
        if ( (lock->GetBlobState() & CBioseq_Handle::fState_no_data) &&
             (lock->GetBlobState() != CBioseq_Handle::fState_no_data) ) {
            NCBI_THROW2(CBlobStateException, eBlobStateError,
                        "blob state error for "+idh.AsString(),
                        lock->GetBlobState());
        }
        locks.insert(lock);
    }

    return locks;
}


void CWGSDataLoader_Impl::LoadBlob(const CWGSBlobId& blob_id,
                                   CTSE_LoadLock& load_lock)
{
    GetFileInfo(blob_id)->LoadBlob(blob_id, load_lock);
}


void CWGSDataLoader_Impl::LoadChunk(const CWGSBlobId& blob_id,
                                    CTSE_Chunk_Info& chunk_info)
{
    GetFileInfo(blob_id)->LoadChunk(blob_id, chunk_info);
}


void CWGSDataLoader_Impl::GetIds(const CSeq_id_Handle& idh, TIds& ids)
{
    CBioseq::TId ids2;
    CWGSScaffoldIterator it2;
    if ( CWGSSeqIterator it = GetSeqIterator(idh, &it2) ) {
        it.GetIds(ids2);
    }
    else if ( it2 ) {
        it2.GetIds(ids2);
    }
    ITERATE ( CBioseq::TId, it2, ids2 ) {
        ids.push_back(CSeq_id_Handle::GetHandle(**it2));
    }
}


CSeq_id_Handle CWGSDataLoader_Impl::GetAccVer(const CSeq_id_Handle& idh)
{
    CWGSScaffoldIterator it2;
    if ( CWGSSeqIterator it = GetSeqIterator(idh, &it2) ) {
        if ( CRef<CSeq_id> acc_id = it.GetAccSeq_id() ) {
            return CSeq_id_Handle::GetHandle(*acc_id);
        }
    }
    else if ( it2 ) {
        if ( CRef<CSeq_id> acc_id = it2.GetAccSeq_id() ) {
            return CSeq_id_Handle::GetHandle(*acc_id);
        }
    }
    return CSeq_id_Handle();
}


TGi CWGSDataLoader_Impl::GetGi(const CSeq_id_Handle& idh)
{
    if ( CWGSSeqIterator it = GetSeqIterator(idh) ) {
        if ( it.HasGi() ) {
            return it.GetGi();
        }
    }
    return ZERO_GI;
}


int CWGSDataLoader_Impl::GetTaxId(const CSeq_id_Handle& idh)
{
    CWGSScaffoldIterator it2;
    if ( CWGSSeqIterator it = GetSeqIterator(idh, &it2) ) {
        if ( it.HasTaxId() ) {
            return it.GetTaxId();
        }
        return 0; // taxid is not defined
    }
    else if ( it2 ) {
        return 0; // taxid is not defined
    }
    return -1; // sequence is unknown
}


TSeqPos CWGSDataLoader_Impl::GetSequenceLength(const CSeq_id_Handle& idh)
{
    CWGSScaffoldIterator it2;
    if ( CWGSSeqIterator it = GetSeqIterator(idh, &it2) ) {
        return it.GetSeqLength();
    }
    else if ( it2 ) {
        return it2.GetSeqLength();
    }
    return kInvalidSeqPos;
}


CSeq_inst::TMol CWGSDataLoader_Impl::GetSequenceType(const CSeq_id_Handle& idh)
{
    if ( GetBlobId(idh) ) {
        return CSeq_inst::eMol_na;
    }
    return CSeq_inst::eMol_not_set;
}


/////////////////////////////////////////////////////////////////////////////
// CWGSFileInfo
/////////////////////////////////////////////////////////////////////////////


CWGSFileInfo::CWGSFileInfo(CWGSDataLoader_Impl& impl,
                           CTempString prefix)
{
    try {
        x_Initialize(impl, prefix);
    }
    catch ( CSraException& exc ) {
        if ( GetDebugLevel() >= 1 ) {
            ERR_POST_X(1, "Exception while opening WGS DB "<<prefix<<": "<<exc);
        }
        if ( exc.GetParam().find(prefix) == NPOS ) {
            exc.SetParam(exc.GetParam()+" acc="+string(prefix));
        }
        throw exc;
    }
    catch ( CException& exc ) {
        if ( GetDebugLevel() >= 1 ) {
            ERR_POST_X(1, "Exception while opening WGS DB "<<prefix<<": "<<exc);
        }
        NCBI_RETHROW_FMT(exc, CSraException, eOtherError,
                         "CWGSDataLoader: exception while opening WGS DB "<<prefix);
    }
}


void CWGSFileInfo::x_Initialize(CWGSDataLoader_Impl& impl,
                                CTempString prefix)
{
    m_WGSDb = CWGSDb(impl.m_Mgr, prefix, impl.m_WGSVolPath);
    m_WGSPrefix = m_WGSDb->GetIdPrefixWithVersion();
    m_AddWGSMasterDescr = impl.GetAddWGSMasterDescr();
    m_FirstBadRowId = m_FirstBadScaffoldRowId = m_FirstBadProteinRowId = 0;
    if ( GetDebugLevel() >= 1 ) {
        ERR_POST_X(1, "Opened WGS DB "<<prefix<<" -> "<<
                   GetWGSPrefix()<<" "<<m_WGSDb.GetWGSPath());
    }
    x_InitMasterDescr();
}


void CWGSFileInfo::x_InitMasterDescr(void)
{
    if ( !m_AddWGSMasterDescr ) {
        // master descriptors are disabled
        return;
    }
    if ( m_WGSDb->LoadMasterDescr() ) {
        // loaded descriptors from metadata
        return;
    }
    CRef<CSeq_id> id = m_WGSDb->GetMasterSeq_id();
    if ( !id ) {
        // no master sequence id
        return;
    }
    CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(*id);
    CDataLoader* gb_loader =
        CObjectManager::GetInstance()->FindDataLoader("GBLOADER");
    if ( !gb_loader ) {
        // no GenBank loader found -> no way to load master record
        return;
    }
    CDataLoader::TTSE_LockSet locks =
        gb_loader->GetRecordsNoBlobState(idh, CDataLoader::eBioseqCore);
    ITERATE ( CDataLoader::TTSE_LockSet, it, locks ) {
        CConstRef<CBioseq_Info> bs_info = (*it)->FindMatchingBioseq(idh);
        if ( !bs_info ) {
            continue;
        }
        if ( bs_info->IsSetDescr() ) {
            m_WGSDb->SetMasterDescr(bs_info->GetDescr().Get());
        }
        break;
    }
}


bool CWGSFileInfo::IsValidRowId(const SAccFileInfo& info,
                                SIZE_TYPE row_digits)
{
    if ( info.row_id == 0 || m_WGSDb->GetIdRowDigits() != row_digits ) {
        return false;
    }
    Uint8 first_bad_row_id;
    {{
        CMutexGuard guard(GetMutex());
        if ( info.seq_type == 'S' ) {
            Uint8& c_first_bad_row_id = m_FirstBadScaffoldRowId;
            first_bad_row_id = c_first_bad_row_id;
            if ( !first_bad_row_id ) {
                CWGSScaffoldIterator it(m_WGSDb);
                c_first_bad_row_id = first_bad_row_id = it.GetFirstBadRowId();
            }
        }
        else if ( info.seq_type == 'P' ) {
            ERR_POST("not supported");
            return false;
        }
        else {
            Uint8& c_first_bad_row_id = m_FirstBadRowId;
            first_bad_row_id = c_first_bad_row_id;
            if ( !first_bad_row_id ) {
                CWGSSeqIterator it(m_WGSDb,
                                   CWGSSeqIterator::eIncludeWithdrawn);
                c_first_bad_row_id = first_bad_row_id = it.GetFirstBadRowId();
            }
        }
    }}
    return info.row_id < first_bad_row_id;
}


bool CWGSFileInfo::IsCorrectVersion(const SAccFileInfo& info)
{
    if ( info.row_id == 0 ) {
        return false;
    }
    if ( info.seq_type == 'S' ) {
        // scaffolds can have only version 1
        return info.version == 1;
    }
    else if ( info.seq_type == 'P' ) {
        ERR_POST("not supported");
        return false;
    }
    else {
        CWGSSeqIterator iter(m_WGSDb, info.row_id,
                             CWGSSeqIterator::eIncludeWithdrawn);
        return iter && info.version == iter.GetAccVersion();
    }
}


void CWGSFileInfo::LoadBlob(const CWGSBlobId& blob_id,
                            CTSE_LoadLock& load_lock)
{
    if ( !load_lock.IsLoaded() ) {
        CRef<CBioseq> seq;
        if ( blob_id.m_SeqType == 'S' ) {
            CWGSScaffoldIterator it(GetDb(), blob_id.m_RowId);
            seq = it.GetBioseq();
        }
        else if ( blob_id.m_SeqType == 'P' ) {
            ERR_POST("not supported");
        }
        else {
            CWGSSeqIterator it(GetDb(), blob_id.m_RowId,
                               CWGSSeqIterator::eIncludeWithdrawn);
            if ( it ) {
                CBioseq_Handle::TBioseqStateFlags state = 0;
                if ( NCBI_gb_state gb_state = it.GetGBState() ) {
                    if ( gb_state == 1 ) {
                        state |= CBioseq_Handle::fState_suppress_perm;
                    }
                    if ( gb_state == 2 ) {
                        state |= CBioseq_Handle::fState_dead;
                    }
                    if ( gb_state == 3 ) {
                        state |= CBioseq_Handle::fState_withdrawn;
                        state |= CBioseq_Handle::fState_no_data;
                    }
                }
                load_lock->SetBlobState(state);
                if ( !(state & CBioseq_Handle::fState_no_data) ) {
                    seq = it.GetBioseq();
                }
            }
            else {
                load_lock->SetBlobState(CBioseq_Handle::fState_no_data);
            }
        }
        if ( seq ) {
            CRef<CSeq_entry> entry(new CSeq_entry);
            entry->SetSeq(*seq);
            load_lock->SetSeq_entry(*entry);
        }
    }
}


void CWGSFileInfo::LoadChunk(const CWGSBlobId& blob_id,
                             CTSE_Chunk_Info& chunk_info)
{
}


END_SCOPE(objects)
END_NCBI_SCOPE
