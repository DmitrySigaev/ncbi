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
*  Author: Christiam Camacho
*
*  File Description:
*   Data loader implementation that uses the blast databases
*
* ===========================================================================
*/
#include <ncbi_pch.hpp>
#include <util/rangemap.hpp>
#include <objtools/data_loaders/blastdb/bdbloader.hpp>
#include <objmgr/impl/handle_range_map.hpp>
#include <objmgr/data_loader_factory.hpp>

#include <ctools/asn_converter.hpp>
#include <objects/seq/seq_id_handle.hpp>
#include <objmgr/impl/data_source.hpp>

#include <objects/seq/Bioseq.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objmgr/util/sequence.hpp>
#include <corelib/plugin_manager_impl.hpp>

//=======================================================================
// BlastDbDataLoader Public interface 
//

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

CBlastDbDataLoader::TRegisterLoaderInfo CBlastDbDataLoader::RegisterInObjectManager(
    CObjectManager& om,
    const string& dbname,
    const EDbType dbtype,
    CObjectManager::EIsDefault is_default,
    CObjectManager::TPriority priority)
{
    SBlastDbParam param(dbname, dbtype);
    TMaker maker(param);
    CDataLoader::RegisterInObjectManager(om, maker, is_default, priority);
    return maker.GetRegisterInfo();
}


string CBlastDbDataLoader::GetLoaderNameFromArgs(const SBlastDbParam& /*param*/)
{
    return "BLASTDB";
}


CBlastDbDataLoader::CBlastDbDataLoader(const string&        loader_name,
                                       const SBlastDbParam& param)
    : CDataLoader(loader_name),
      m_dbname(param.m_DbName),
      m_dbtype(param.m_DbType),
      m_rdfp(0)
{
    m_mutex = new CFastMutex();
}

CBlastDbDataLoader::~CBlastDbDataLoader(void)
{
    if (m_rdfp) {
        CFastMutexGuard mtx(*m_mutex);
        if (m_rdfp)
            m_rdfp = readdb_destruct(m_rdfp);
    }
    delete m_mutex;
}


// TODO Note that the ranges are ignored for right now
// How to handle other choices?
CDataLoader::TTSE_LockSet
CBlastDbDataLoader::GetRecords(const CSeq_id_Handle& idh, 
        const EChoice choice)
{
    TTSE_LockSet locks;
    //LOG_POST("***CBlastDbDataLoader::GetRecords***");
    // only eBioseq and eBioseqCore are supported
    switch (choice) {
        case eBlob:
        case eCore:
        case eSequence:
        case eFeatures:
        case eGraph:
        case eAll:
        default:
            LOG_POST("Invalid choice: " + NStr::IntToString(choice));
            return locks;
        case eBioseq:
        case eBioseqCore:
            break;
    }

    // Open the blast database if it hasn't been accessed yet
    if (!m_rdfp) {
        CFastMutexGuard mtx(*m_mutex);
        if (!m_rdfp) {
            char* tmp = strdup(m_dbname.c_str());
            m_rdfp = readdb_new_ex2(tmp, (int)m_dbtype, READDB_NEW_INDEX |
                    READDB_NEW_DO_TAXDB, NULL, NULL);
            free(tmp);
        }
    }

    // for each seqid in hrmap, look them up in db and add them to the data
    // source
    {{

        DECLARE_ASN_CONVERTER(CSeq_id, SeqId, sic);
        DECLARE_ASN_CONVERTER(CBioseq, Bioseq, bc);
        SeqIdPtr sip = NULL, sip_tmp = NULL, sip_itr = NULL, all_seqids = NULL;
        BioseqPtr bsp = NULL;
        int oid = -1;
        unsigned int index = 0;
        TOid2Bioseq::iterator found;

        CConstRef<CSeq_id> seq_id = idh.GetSeqId();
        if ( !(sip = sic.ToC(*seq_id)) )
            return locks;

        if ( (oid = SeqId2OrdinalId(m_rdfp, sip)) < 0)
            return locks;

        // If we've already retrieved this particular ordinal id, ignore it
        if ( (found = m_cache.find(oid)) != m_cache.end())
            return locks;

        {{  // protect access to the blast database
            CFastMutexGuard mtx(*m_mutex);
            bsp = readdb_get_bioseq(m_rdfp, oid);

            // Retrieve multiple seqids if there are any
            while (readdb_get_header(m_rdfp, oid, &index, &sip_tmp, NULL)) {
                if (!all_seqids) {
                    all_seqids = sip_tmp;
                } else {
                    sip_itr = all_seqids;
                    while (sip_itr->next)
                        sip_itr = sip_itr->next;
                    sip_itr->next = sip_tmp;
                }
            }
        }}

        if (all_seqids) {
            SeqIdSetFree(bsp->id);
            bsp->id = all_seqids;
        }

        CRef<CBioseq> bsr(bc.FromC(bsp));

        CRef<CSeq_entry> ser(new CSeq_entry());
        ser->Select(CSeq_entry::e_Seq);
        ser->SetSeq(*bsr);

        locks.insert(GetDataSource()->AddTSE(*ser));
        m_cache[oid] = bsr;

        bsp = BioseqFree(bsp);
        sip = SeqIdFree(sip);
    }}
    return locks;
}

void
CBlastDbDataLoader::DebugDump(CDebugDumpContext ddc, unsigned int depth) const
{
    //LOG_POST("CBlastDbDataLoader::DebugDump\n");
    ddc.SetFrame("CGBLoader");
    // CObject::DebugDump( ddc, depth);
    DebugDumpValue(ddc,"m_dbname", m_dbname);
    DebugDumpValue(ddc,"m_dbtype", m_dbtype);
    DebugDumpValue(ddc,"m_rdfp", m_rdfp);
}

END_SCOPE(objects)

// ===========================================================================

USING_SCOPE(objects);

const string kDataLoader_BlastDb_DriverName("blastdb");

class CBlastDb_DataLoaderCF : public CDataLoaderFactory
{
public:
    CBlastDb_DataLoaderCF(void)
        : CDataLoaderFactory(kDataLoader_BlastDb_DriverName) {}
    virtual ~CBlastDb_DataLoaderCF(void) {}

protected:
    virtual CDataLoader* CreateAndRegister(
        CObjectManager& om,
        const TPluginManagerParamTree* params) const;
};


CDataLoader* CBlastDb_DataLoaderCF::CreateAndRegister(
    CObjectManager& om,
    const TPluginManagerParamTree* params) const
{
    if ( !ValidParams(params) ) {
        // Use constructor without arguments
        return CBlastDbDataLoader::RegisterInObjectManager(om).GetLoader();
    }
    // Parse params, select constructor
    const string& dbname =
        GetParam(GetDriverName(), params,
        kCFParam_BlastDb_DbName, false, kEmptyStr);
    const string& dbtype_str =
        GetParam(GetDriverName(), params,
        kCFParam_BlastDb_DbType, false, kEmptyStr);
    if ( !dbname.empty() ) {
        // Use database name
        CBlastDbDataLoader::EDbType dbtype = CBlastDbDataLoader::eUnknown;
        if ( !dbtype_str.empty() ) {
            if (NStr::CompareNocase(dbtype_str, "Nucleotide") == 0) {
                dbtype = CBlastDbDataLoader::eNucleotide;
            }
            else if (NStr::CompareNocase(dbtype_str, "Protein") == 0) {
                dbtype = CBlastDbDataLoader::eProtein;
            }
        }
        return CBlastDbDataLoader::RegisterInObjectManager(
            om,
            dbname,
            dbtype,
            GetIsDefault(params),
            GetPriority(params)).GetLoader();
    }
    // IsDefault and Priority arguments may be specified
    return CBlastDbDataLoader::RegisterInObjectManager(om).GetLoader();
}


void NCBI_EntryPoint_DataLoader_BlastDb(
    CPluginManager<CDataLoader>::TDriverInfoList&   info_list,
    CPluginManager<CDataLoader>::EEntryPointRequest method)
{
    CHostEntryPointImpl<CBlastDb_DataLoaderCF>::
        NCBI_EntryPointImpl(info_list, method);
}


void NCBI_EntryPoint_xloader_blastdb(
    CPluginManager<objects::CDataLoader>::TDriverInfoList&   info_list,
    CPluginManager<objects::CDataLoader>::EEntryPointRequest method)
{
    NCBI_EntryPoint_DataLoader_BlastDb(info_list, method);
}


END_NCBI_SCOPE


/* ========================================================================== 
 *
 * $Log$
 * Revision 1.12  2004/08/10 16:56:11  grichenk
 * Fixed dll export declarations, moved entry points to cpp.
 *
 * Revision 1.11  2004/08/04 14:56:35  vasilche
 * Updated to changes in TSE locking scheme.
 *
 * Revision 1.10  2004/08/02 21:33:07  ucko
 * Preemptively include <util/rangemap.hpp> before anything can define
 * stat() as a macro.
 *
 * Revision 1.9  2004/08/02 17:34:43  grichenk
 * Added data_loader_factory.cpp.
 * Renamed xloader_cdd to ncbi_xloader_cdd.
 * Implemented data loader factories for all loaders.
 *
 * Revision 1.8  2004/07/28 14:02:57  grichenk
 * Improved MT-safety of RegisterInObjectManager(), simplified the code.
 *
 * Revision 1.7  2004/07/26 14:13:32  grichenk
 * RegisterInObjectManager() return structure instead of pointer.
 * Added CObjectManager methods to manipuilate loaders.
 *
 * Revision 1.6  2004/07/21 15:51:26  grichenk
 * CObjectManager made singleton, GetInstance() added.
 * CXXXXDataLoader constructors made private, added
 * static RegisterInObjectManager() and GetLoaderNameFromArgs()
 * methods.
 *
 * Revision 1.5  2004/07/12 15:05:32  grichenk
 * Moved seq-id mapper from xobjmgr to seq library
 *
 * Revision 1.4  2004/05/21 21:42:51  gorelenk
 * Added PCH ncbi_pch.hpp
 *
 * Revision 1.3  2003/09/30 17:22:06  vasilche
 * Fixed for new CDataLoader interface.
 *
 * Revision 1.2  2003/09/30 16:36:36  vasilche
 * Updated CDataLoader interface.
 *
 * Revision 1.1  2003/08/06 16:15:18  jianye
 * Add BLAST DB loader.
 *
 * Revision 1.7  2003/05/19 21:11:46  camacho
 * Added caching
 *
 * Revision 1.6  2003/05/16 14:27:48  camacho
 * Proper use of namespaces
 *
 * Revision 1.5  2003/05/15 15:58:28  camacho
 * Minor changes
 *
 * Revision 1.4  2003/05/08 15:11:43  camacho
 * Changed prototype for GetRecords in base class
 *
 * Revision 1.3  2003/03/21 17:42:54  camacho
 * Added loading of taxonomy info
 *
 * Revision 1.2  2003/03/18 21:19:26  camacho
 * Retrieve all seqids if available
 *
 * Revision 1.1  2003/03/14 22:37:26  camacho
 * Initial revision
 *
 *
 * ========================================================================== */
