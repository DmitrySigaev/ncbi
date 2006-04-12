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
 * Author: Anatoliy Kuznetsov
 *
 * File Description:  LDS dataloader. Implementations.
 *
 */


#include <ncbi_pch.hpp>

#include <objtools/data_loaders/lds/lds_dataloader.hpp>
#include <objtools/lds/lds_reader.hpp>
#include <objtools/lds/lds_util.hpp>
#include <objtools/lds/lds.hpp>
#include <objtools/lds/lds_object.hpp>

#include <objects/general/Object_id.hpp>

#include <bdb/bdb_util.hpp>

#include <objmgr/impl/handle_range_map.hpp>
#include <objmgr/impl/tse_info.hpp>
#include <objmgr/impl/tse_loadlock.hpp>
#include <objmgr/impl/data_source.hpp>
#include <objmgr/data_loader_factory.hpp>

#include <corelib/plugin_manager.hpp>
#include <corelib/plugin_manager_impl.hpp>
#include <corelib/plugin_manager_store.hpp>

BEGIN_NCBI_SCOPE

BEGIN_SCOPE(objects)

DEFINE_STATIC_FAST_MUTEX(sx_LDS_Lock);



/// @internal
///
struct SLDS_ObjectDisposition
{
    SLDS_ObjectDisposition(int object, int parent, int tse)
      : object_id(object), parent_id(parent), tse_id(tse)
    {}

    int     object_id;
    int     parent_id;
    int     tse_id;
};

/// Objects scanner
///
/// @internal
///
class CLDS_FindSeqIdFunc
{
public:
    typedef vector<SLDS_ObjectDisposition>  TDisposition;

public:
    CLDS_FindSeqIdFunc(SLDS_TablesCollection& db,
                       const CHandleRangeMap&  hrmap)
    : m_HrMap(hrmap),
      m_db(db)
    {}

    void operator()(SLDS_ObjectDB& dbf)
    {
        if (dbf.primary_seqid.IsNull())
            return;
        int object_id = dbf.object_id;
        int parent_id = dbf.parent_object_id;
        int tse_id    = dbf.TSE_object_id;


        string seq_id_str = (const char*)dbf.primary_seqid;
        if (seq_id_str.empty())
            return;
        {{
            CRef<CSeq_id> seq_id_db;
            try {
                seq_id_db.Reset(new CSeq_id(seq_id_str));
            } catch (CSeqIdException&) {
                seq_id_db.Reset(new CSeq_id(CSeq_id::e_Local, seq_id_str));
            }

            ITERATE (CHandleRangeMap, it, m_HrMap) {
                CSeq_id_Handle seq_id_hnd = it->first;
                CConstRef<CSeq_id> seq_id = seq_id_hnd.GetSeqId();
                if (seq_id->Match(*seq_id_db)) {
                    m_Disposition.push_back(
                        SLDS_ObjectDisposition(object_id, parent_id, tse_id));

                    LOG_POST(Info << "LDS: Local object " << seq_id_str
                                  << " id=" << object_id << " matches "
                                  << seq_id->AsFastaString());

                    return;
                }
            } // ITERATE
        }}

        // Primaty seq_id scan gave no results
        // Trying supplemental aliases
/*
        m_db.object_attr_db.object_attr_id = object_id;
        if (m_db.object_attr_db.Fetch() != eBDB_Ok) {
            return;
        }

        if (m_db.object_attr_db.seq_ids.IsNull() || 
            m_db.object_attr_db.seq_ids.IsEmpty()) {
            return;
        }
*/
        if (dbf.seq_ids.IsNull() || 
            dbf.seq_ids.IsEmpty()) {
            return;
        }
        string attr_seq_ids((const char*)dbf.seq_ids);
        vector<string> seq_id_arr;
        
        NStr::Tokenize(attr_seq_ids, " ", seq_id_arr, NStr::eMergeDelims);

        ITERATE (vector<string>, it, seq_id_arr) {
            seq_id_str = *it;
            CRef<CSeq_id> seq_id_db;
            try {
                seq_id_db.Reset(new CSeq_id(seq_id_str));
            } catch (CSeqIdException&) {
                seq_id_db.Reset(new CSeq_id(CSeq_id::e_Local, seq_id_str));
            }

            {{
            
            ITERATE (CHandleRangeMap, it2, m_HrMap) {
                CSeq_id_Handle seq_id_hnd = it2->first;
                CConstRef<CSeq_id> seq_id = seq_id_hnd.GetSeqId();

                if (seq_id->Match(*seq_id_db)) {
                    m_Disposition.push_back(
                        SLDS_ObjectDisposition(object_id, parent_id, tse_id));

                    LOG_POST(Info << "LDS: Local object " << seq_id_str
                                  << " id=" << object_id << " matches "
                                  << seq_id->AsFastaString());

                    return;
                }
            } // ITERATE
            
            }}

        } // ITERATE
    }

    const TDisposition& GetResultDisposition() const 
    {
        return m_Disposition;
    }
private:
    const CHandleRangeMap&  m_HrMap;       ///< Range map of seq ids to search
    SLDS_TablesCollection&  m_db;          ///< The LDS database
    TDisposition            m_Disposition; ///< Search result (found objects)
};



CLDS_DataLoader::TRegisterLoaderInfo CLDS_DataLoader::RegisterInObjectManager(
    CObjectManager& om,
    CObjectManager::EIsDefault is_default,
    CObjectManager::TPriority priority)
{
    TSimpleMaker maker;
    CDataLoader::RegisterInObjectManager(om, maker, is_default, priority);
    return maker.GetRegisterInfo();
}


string CLDS_DataLoader::GetLoaderNameFromArgs(void)
{
    return "LDS_dataloader";
}


CLDS_DataLoader::TRegisterLoaderInfo CLDS_DataLoader::RegisterInObjectManager(
    CObjectManager& om,
    CLDS_Database& lds_db,
    CObjectManager::EIsDefault is_default,
    CObjectManager::TPriority priority)
{
    TDbMaker maker(lds_db);
    CDataLoader::RegisterInObjectManager(om, maker, is_default, priority);
    return maker.GetRegisterInfo();
}


string CLDS_DataLoader::GetLoaderNameFromArgs(CLDS_Database& lds_db)
{
    const string& alias = lds_db.GetAlias();
    if ( !alias.empty() ) {
        return "LDS_dataloader_" + alias;
    }
    return "LDS_dataloader_" + lds_db.GetDirName() + "_" + lds_db.GetDbName();
}


CDataLoader*
CLDS_DataLoader::CLDS_LoaderMaker::CreateLoader(void) const
{
    CFastMutexGuard mg(sx_LDS_Lock);

    bool is_created = false;
    auto_ptr<CLDS_Database> lds_db
        (CLDS_Management::OpenCreateDB(m_Param, "lds",
                                        &is_created,
                                        m_Recurse, m_ControlSum));
    if ( !is_created ) {
        CLDS_Management mgmt(*lds_db);
        mgmt.SyncWithDir(m_Param, m_Recurse, m_ControlSum);
    }
    CLDS_DataLoader* dl = new CLDS_DataLoader(m_Name, lds_db.release());
    return dl;
}


CLDS_DataLoader::TRegisterLoaderInfo CLDS_DataLoader::RegisterInObjectManager(
    CObjectManager& om,
    const string& db_path,
    CObjectManager::EIsDefault is_default,
    CObjectManager::TPriority priority,
    CLDS_Management::ERecurse recurse,
    CLDS_Management::EComputeControlSum csum
    )
{
    CLDS_LoaderMaker maker(db_path, recurse, csum);
    //TPathMaker maker(db_path);
    CDataLoader::RegisterInObjectManager(om, maker, is_default, priority);
    return maker.GetRegisterInfo();
}


string CLDS_DataLoader::GetLoaderNameFromArgs(const string& db_path)
{
    return "LDS_dataloader_" + db_path;
}


CLDS_DataLoader::CLDS_DataLoader(void)
    : CDataLoader(GetLoaderNameFromArgs()),
      m_LDS_db(0)
{
}


CLDS_DataLoader::CLDS_DataLoader(const string& dl_name)
    : CDataLoader(dl_name),
      m_LDS_db(0)
{
}



CLDS_DataLoader::CLDS_DataLoader(const string& dl_name,
                                 CLDS_Database& lds_db)
 : CDataLoader(dl_name),
   m_LDS_db(&lds_db),
   m_OwnDatabase(false)
{
}

CLDS_DataLoader::CLDS_DataLoader(const string& dl_name,
                                 CLDS_Database* lds_db)
 : CDataLoader(dl_name),
   m_LDS_db(lds_db),
   m_OwnDatabase(true)
{
}

CLDS_DataLoader::CLDS_DataLoader(const string& dl_name,
                                 const string& db_path)
 : CDataLoader(dl_name),
   m_LDS_db(new CLDS_Database(db_path, kEmptyStr)),
   m_OwnDatabase(true)
{
    try {
       CFastMutexGuard mg(sx_LDS_Lock);

        m_LDS_db->Open();
    } 
    catch(...)
    {
        delete m_LDS_db;
        throw;
    }
}

CLDS_DataLoader::~CLDS_DataLoader()
{
    if (m_OwnDatabase)
        delete m_LDS_db;
}


void CLDS_DataLoader::SetDatabase(CLDS_Database& lds_db,
                                  const string&  dl_name)
{
    CFastMutexGuard mg(sx_LDS_Lock);

    if (m_LDS_db && m_OwnDatabase) {
        delete m_LDS_db;
    }
    m_LDS_db = &lds_db;
    SetName(dl_name);
}

CDataLoader::TTSE_LockSet
CLDS_DataLoader::GetRecords(const CSeq_id_Handle& idh,
                            EChoice /* choice */)
{
    unsigned db_recovery_count = 0;
    while (true) {
    try {

        TTSE_LockSet locks;
        CHandleRangeMap hrmap;
        hrmap.AddRange(idh, CRange<TSeqPos>::GetWhole(), eNa_strand_unknown);

        CLDS_FindSeqIdFunc search_func(m_LDS_db->GetTables(), hrmap);

        {{
        CFastMutexGuard mg(sx_LDS_Lock);

            SLDS_TablesCollection& db = m_LDS_db->GetTables();
            CLDS_Query lds_query(*m_LDS_db);

            // index screening
            CLDS_Set       cand_set;
            {{
            CBDB_FileCursor cur_int_idx(db.obj_seqid_int_idx);
            cur_int_idx.SetCondition(CBDB_FileCursor::eEQ);

            CBDB_FileCursor cur_txt_idx(db.obj_seqid_txt_idx);
            cur_txt_idx.SetCondition(CBDB_FileCursor::eEQ);

            SLDS_SeqIdBase sbase;

            ITERATE (CHandleRangeMap, it, hrmap) {
                CSeq_id_Handle seq_id_hnd = it->first;
                CConstRef<CSeq_id> seq_id = seq_id_hnd.GetSeqId();

                LDS_GetSequenceBase(*seq_id, &sbase);

                if (!sbase.str_id.empty()) {
                    NStr::ToUpper(sbase.str_id);
                }

                lds_query.ScreenSequence(sbase, 
                                        &cand_set, 
                                        cur_int_idx, 
                                        cur_txt_idx);


            } // ITER

            }}

            // finding matching sequences using pre-screened result set

            if (cand_set.any()) {

                BDB_iterate_file(db.object_db, 
                                cand_set.first(), cand_set.end(), 
                                search_func);        
            }

        }}

        // load objects

        const CLDS_FindSeqIdFunc::TDisposition& disposition = 
                                            search_func.GetResultDisposition();

        CDataSource* data_source = GetDataSource();
        _ASSERT(data_source);

        CLDS_FindSeqIdFunc::TDisposition::const_iterator it;
        for (it = disposition.begin(); it != disposition.end(); ++it) {
            const SLDS_ObjectDisposition& obj_disp = *it;
            int object_id = 
                obj_disp.tse_id ? obj_disp.tse_id : obj_disp.object_id;

            // check if we can extract seq-entry out of binary bioseq-set file
            //
            //   (this trick has been added by kuznets (Jan-12-2005) to read 
            //    molecules out of huge refseq files)
            {
                CLDS_Query query(*m_LDS_db);
                CLDS_Query::SObjectDescr obj_descr = 
                    query.GetObjectDescr(
                                    m_LDS_db->GetObjTypeMap(),
                                    obj_disp.tse_id, false/*do not trace to top*/);
                if ((obj_descr.is_object && obj_descr.id > 0)      &&
                    (obj_descr.format == CFormatGuess::eBinaryASN) &&
                    (obj_descr.type_str == "Bioseq-set")
                ) {
                obj_descr = 
                        query.GetTopSeqEntry(m_LDS_db->GetObjTypeMap(),
                                            obj_disp.object_id);
                object_id = obj_descr.id;
                }
            }

            TBlobId blob_id(new CBlobIdInt(object_id));
            CTSE_LoadLock load_lock = data_source->GetTSE_LoadLock(blob_id);
            if ( !load_lock.IsLoaded() ) {
                CRef<CSeq_entry> seq_entry = 
                    LDS_LoadTSE(*m_LDS_db, object_id, false/*dont trace to top*/);
                if ( !seq_entry ) {
                    NCBI_THROW2(CBlobStateException, eBlobStateError,
                                "cannot load blob",
                                CBioseq_Handle::fState_no_data);
                }
                load_lock->SetSeq_entry(*seq_entry);
                load_lock.SetLoaded();
            }
            locks.insert(load_lock);
        }


        return locks;
    } 
    catch (CBDB_ErrnoException& ex)
    {
        if (ex.IsRecovery()) {
            ++db_recovery_count;
            if (db_recovery_count > 10) {
                throw;
            }
            ERR_POST("LDS Database returned db recovery error... Reopening.");
            CFastMutexGuard mg(sx_LDS_Lock);
            m_LDS_db->ReOpen();
        }
    }
    } // while
}


END_SCOPE(objects)

// ===========================================================================

USING_SCOPE(objects);

void DataLoaders_Register_LDS(void)
{
    RegisterEntryPoint<CDataLoader>(NCBI_EntryPoint_DataLoader_LDS);
}


const string kDataLoader_LDS_DriverName("lds");

class CLDS_DataLoaderCF : public CDataLoaderFactory
{
public:
    CLDS_DataLoaderCF(void)
        : CDataLoaderFactory(kDataLoader_LDS_DriverName) {}
    virtual ~CLDS_DataLoaderCF(void) {}

protected:
    virtual CDataLoader* CreateAndRegister(
        CObjectManager& om,
        const TPluginManagerParamTree* params) const;
};


CDataLoader* CLDS_DataLoaderCF::CreateAndRegister(
    CObjectManager& om,
    const TPluginManagerParamTree* params) const
{
    if ( !ValidParams(params) ) {
        // Use constructor without arguments
        return CLDS_DataLoader::RegisterInObjectManager(om).GetLoader();
    }
    // Parse params, select constructor
/*    
    const string& database_str =
        GetParam(GetDriverName(), params,
                 kCFParam_LDS_Database, false, kEmptyStr);
*/
    const string& db_path =
        GetParam(GetDriverName(), params,
                 kCFParam_LDS_DbPath, false, kEmptyStr);
    const string& recurse_str =
        GetParam(GetDriverName(), params,
                 kCFParam_LDS_RecurseSubDir, false, "true");
    bool recurse = NStr::StringToBool(recurse_str);
    CLDS_Management::ERecurse recurse_subdir = 
        recurse ? 
            CLDS_Management::eRecurseSubDirs : CLDS_Management::eDontRecurse;

    const string& control_sum_str =
        GetParam(GetDriverName(), params,
                 kCFParam_LDS_ControlSum, false, "true");
    bool csum = NStr::StringToBool(control_sum_str);
    CLDS_Management::EComputeControlSum control_sum =
       csum ? 
         CLDS_Management::eComputeControlSum : CLDS_Management::eNoControlSum;
              
// commented out...
// suspicious code, expected somebody will pass database as a stringified pointer
// (code has a bug in casts which should always give NULL
/*
    if ( !database_str.empty() ) {
        // Use existing database
        CLDS_Database* db = dynamic_cast<CLDS_Database*>(
            static_cast<CDataLoader*>(
            const_cast<void*>(NStr::StringToPtr(database_str))));
        if ( db ) {
            return CLDS_DataLoader::RegisterInObjectManager(
                om,
                *db,
                GetIsDefault(params),
                GetPriority(params)).GetLoader();
        }
    }
*/
    if ( !db_path.empty() ) {
        // Use db path
        return CLDS_DataLoader::RegisterInObjectManager(
            om,
            db_path,
            GetIsDefault(params),
            GetPriority(params),
            recurse_subdir,
            control_sum).GetLoader();
    }
    // IsDefault and Priority arguments may be specified
    return CLDS_DataLoader::RegisterInObjectManager(
        om,
        GetIsDefault(params),
        GetPriority(params)).GetLoader();
}


void NCBI_EntryPoint_DataLoader_LDS(
    CPluginManager<CDataLoader>::TDriverInfoList&   info_list,
    CPluginManager<CDataLoader>::EEntryPointRequest method)
{
    CHostEntryPointImpl<CLDS_DataLoaderCF>::NCBI_EntryPointImpl(info_list, method);
}


void NCBI_EntryPoint_xloader_lds(
    CPluginManager<objects::CDataLoader>::TDriverInfoList&   info_list,
    CPluginManager<objects::CDataLoader>::EEntryPointRequest method)
{
    NCBI_EntryPoint_DataLoader_LDS(info_list, method);
}


END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.39  2006/04/12 13:31:41  kuznets
 * Reopen if BDB throws recovery exception
 *
 * Revision 1.38  2006/02/22 17:18:10  kuznets
 * Added mutex to protect LDS database from parallel calls from OM
 *
 * Revision 1.37  2005/12/21 15:27:17  kuznets
 * Fixed recursion and control sum parameters
 *
 * Revision 1.36  2005/11/15 16:51:48  kuznets
 * Bug fix: convert search string to upper case when index screening
 *
 * Revision 1.35  2005/11/15 14:41:15  kuznets
 * Added dignostics
 *
 * Revision 1.34  2005/10/26 14:36:44  vasilche
 * Updated for new CBlobId interface. Fixed load lock logic.
 *
 * Revision 1.33  2005/10/20 15:35:03  kuznets
 * Code cleanup
 *
 * Revision 1.32  2005/10/06 16:23:55  kuznets
 * Search using LDS SeqId index
 *
 * Revision 1.31  2005/09/26 15:27:39  kuznets
 * Fixed a bug in searching in alternative ids
 *
 * Revision 1.30  2005/09/21 13:33:51  kuznets
 * Implemented database initialization in class factory
 *
 * Revision 1.29  2005/07/15 19:52:25  vasilche
 * Use blob_id map from CDataSource.
 *
 * Revision 1.28  2005/07/01 16:40:37  ucko
 * Adjust for CSeq_id's use of CSeqIdException to report bad input.
 *
 * Revision 1.27  2005/02/02 19:49:55  grichenk
 * Fixed more warnings
 *
 * Revision 1.26  2005/01/26 16:25:21  grichenk
 * Added state flags to CBioseq_Handle.
 *
 * Revision 1.25  2005/01/13 17:52:16  kuznets
 * Tweak dataloader to read seq entries out of large binary ASN bioseq-sets
 *
 * Revision 1.24  2005/01/12 15:55:38  vasilche
 * Use correct constructor of CTSE_Info (detected by new bool operator).
 *
 * Revision 1.23  2005/01/11 19:30:29  kuznets
 * Fixed problem with loaded flag
 *
 * Revision 1.22  2004/12/22 20:42:53  grichenk
 * Added entry points registration funcitons
 *
 * Revision 1.21  2004/08/10 16:56:11  grichenk
 * Fixed dll export declarations, moved entry points to cpp.
 *
 * Revision 1.20  2004/08/04 14:56:35  vasilche
 * Updated to changes in TSE locking scheme.
 *
 *
 * ===========================================================================
 */

