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
* Authors:  Andrei Gourianov
*
* File Description:
*   Object Manager test: multiple threads working with annotations
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.6  2004/07/21 15:51:26  grichenk
* CObjectManager made singleton, GetInstance() added.
* CXXXXDataLoader constructors made private, added
* static RegisterInObjectManager() and GetLoaderNameFromArgs()
* methods.
*
* Revision 1.5  2004/07/13 14:03:08  vasilche
* Added option "idlist" to test_objmgr_data & test_objmgr_data_mt.
*
* Revision 1.4  2004/06/30 20:58:11  vasilche
* Added option to test list of Seq-ids from file.
*
* Revision 1.3  2004/05/21 21:42:56  gorelenk
* Added PCH ncbi_pch.hpp
*
* Revision 1.2  2004/02/17 21:13:20  vasilche
* Added flags to skip some tests.
*
* Revision 1.1  2003/12/16 17:51:18  kuznets
* Code reorganization
*
* Revision 1.14  2003/11/26 17:56:04  vasilche
* Implemented ID2 split in ID1 cache.
* Fixed loading of splitted annotations.
*
* Revision 1.13  2003/11/03 21:21:41  vasilche
* Limit amount of exceptions to catch while testing.
*
* Revision 1.12  2003/10/22 17:58:10  vasilche
* Do not catch CLoaderException::eNoConnection.
*
* Revision 1.11  2003/09/30 16:22:05  vasilche
* Updated internal object manager classes to be able to load ID2 data.
* SNP blobs are loaded as ID2 split blobs - readers convert them automatically.
* Scope caches results of requests for data to data loaders.
* Optimized CSeq_id_Handle for gis.
* Optimized bioseq lookup in scope.
* Reduced object allocations in annotation iterators.
* CScope is allowed to be destroyed before other objects using this scope are
* deleted (feature iterators, bioseq handles etc).
* Optimized lookup for matching Seq-ids in CSeq_id_Mapper.
* Added 'adaptive' option to objmgr_demo application.
*
* Revision 1.10  2003/06/02 16:06:39  dicuccio
* Rearranged src/objects/ subtree.  This includes the following shifts:
*     - src/objects/asn2asn --> arc/app/asn2asn
*     - src/objects/testmedline --> src/objects/ncbimime/test
*     - src/objects/objmgr --> src/objmgr
*     - src/objects/util --> src/objmgr/util
*     - src/objects/alnmgr --> src/objtools/alnmgr
*     - src/objects/flat --> src/objtools/flat
*     - src/objects/validator --> src/objtools/validator
*     - src/objects/cddalignview --> src/objtools/cddalignview
* In addition, libseq now includes six of the objects/seq... libs, and libmmdb
* replaces the three libmmdb? libs.
*
* Revision 1.9  2003/05/20 20:35:09  vasilche
* Reduced number of threads to make test time reasonably small.
*
* Revision 1.8  2003/05/20 15:44:39  vasilche
* Fixed interaction of CDataSource and CDataLoader in multithreaded app.
* Fixed some warnings on WorkShop.
* Added workaround for memory leak on WorkShop.
*
* Revision 1.7  2003/04/24 16:12:39  vasilche
* Object manager internal structures are splitted more straightforward.
* Removed excessive header dependencies.
*
* Revision 1.6  2003/04/15 14:23:11  vasilche
* Added missing includes.
*
* Revision 1.5  2003/03/18 21:48:33  grichenk
* Removed obsolete class CAnnot_CI
*
* Revision 1.4  2002/12/06 15:36:03  grichenk
* Added overlap type for annot-iterators
*
* Revision 1.3  2002/07/22 22:49:05  kimelman
* test fixes for confidential data retrieval
*
* Revision 1.2  2002/05/06 03:28:53  vakatov
* OM/OM1 renaming
*
* Revision 1.1  2002/04/30 19:04:05  gouriano
* multi-threaded data retrieval test
*
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbithr.hpp>
#include <corelib/test_mt.hpp>

#include <connect/ncbi_util.h>

#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqloc/Seq_loc.hpp>

#include <objmgr/object_manager.hpp>
#include <objmgr/objmgr_exception.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/seq_vector.hpp>
#include <objmgr/seqdesc_ci.hpp>
#include <objmgr/feat_ci.hpp>
#include <objmgr/align_ci.hpp>
#include <objtools/data_loaders/genbank/gbloader.hpp>
#include <connect/ncbi_core_cxx.hpp>
#include <connect/ncbi_util.h>

#include <test/test_assert.h>  /* This header must go last */


BEGIN_NCBI_SCOPE
using namespace objects;

DEFINE_STATIC_FAST_MUTEX(s_GlobalLock);

// GIs to process
#if 0
    const int g_gi_from = 156894;
    const int g_gi_to   = 156896;
#elif 0
    const int g_gi_from = 156201;
    const int g_gi_to   = 156203;
#else
    const int g_gi_from = 156000;
    const int g_gi_to   = 157000;
#endif

/////////////////////////////////////////////////////////////////////////////
//
//  Test application
//

class CTestOM : public CThreadedApp
{
protected:
    virtual bool Thread_Run(int idx);
    virtual bool TestApp_Args( CArgDescriptions& args);
    virtual bool TestApp_Init(void);
    virtual bool TestApp_Exit(void);

    typedef map<CSeq_id_Handle, int> TValueMap;
    typedef vector<CSeq_id_Handle> TIds;

    CRef<CScope> m_Scope;
    CRef<CObjectManager> m_ObjMgr;

    TValueMap m_mapGiToDesc;
    TValueMap m_mapGiToFeat0;
    TValueMap m_mapGiToFeat1;

    void SetValue(TValueMap& vm, const CSeq_id_Handle& id, int value);

    TIds m_Ids;

    bool m_load_only;
    bool m_no_snp;

    bool failed;
};


/////////////////////////////////////////////////////////////////////////////


void CTestOM::SetValue(TValueMap& vm, const CSeq_id_Handle& id, int value)
{
    int old_value;
    {{
        CFastMutexGuard guard(s_GlobalLock);
        old_value = vm.insert(TValueMap::value_type(id, value)).first->second;
    }}
    if ( old_value != value ) {
        string name;
        if ( &vm == &m_mapGiToDesc ) name = "desc";
        if ( &vm == &m_mapGiToFeat0 ) name = "feat0";
        if ( &vm == &m_mapGiToFeat1 ) name = "feat1";
        ERR_POST("Inconsistent "<<name<<" on "<<id.AsString()<<
                 " was "<<old_value<<" now "<<value);
    }
    _ASSERT(old_value == value);
}


bool CTestOM::Thread_Run(int idx)
{
// initialize scope
    CScope scope(*m_ObjMgr);
    scope.AddDefaults();

    int from, to;
    // make them go in opposite directions
    if (idx % 2 == 0) {
        from = 0;
        to = m_Ids.size()-1;
    } else {
        from = m_Ids.size()-1;
        to = 0;
    }
    int delta = (to > from) ? 1 : -1;

    bool ok = true;
    const int kMaxErrorCount = 3;
    static int error_count = 0;
    for (int i = from;
         ((delta > 0) && (i <= to)) || ((delta < 0) && (i >= to)); i += delta) {
        CSeq_id_Handle sih = m_Ids[i];
        try {
            // load sequence
            CBioseq_Handle handle = scope.GetBioseqHandle(sih);
            if (!handle) {
                LOG_POST("T" << idx << ": id = " << sih.AsString() <<
                         ": INVALID HANDLE");
                SetValue(m_mapGiToDesc, sih, -1);
                continue;
            }

            if ( !m_load_only ) {
                int count = 0;

                // enumerate descriptions
                // Seqdesc iterator
                for (CSeqdesc_CI desc_it(handle); desc_it;  ++desc_it) {
                    count++;
                }
                // verify result
                SetValue(m_mapGiToDesc, sih, count);

                // enumerate features
                CSeq_loc loc;
                loc.SetWhole(const_cast<CSeq_id&>(*sih.GetSeqId()));
                SAnnotSelector sel(CSeqFeatData::e_not_set);
                if ( idx%2 == 0 ) {
                    sel.SetOverlapType(sel.eOverlap_Intervals);
                    sel.SetResolveMethod(sel.eResolve_All);
                }
                if ( m_no_snp ) {
                    sel.ExcludedAnnotName("SNP");
                }

                count = 0;
                if ( idx%2 == 0 ) {
                    for ( CFeat_CI it(scope, loc, sel); it;  ++it ) {
                        count++;
                    }
                    // verify result
                    SetValue(m_mapGiToFeat0, sih, count);
                }
                else {
                    for ( CFeat_CI it(handle, 0, 0, sel); it;  ++it ) {
                        count++;
                    }
                    // verify result
                    SetValue(m_mapGiToFeat1, sih, count);
                }
            }
        }
        catch (CLoaderException& e) {
            LOG_POST("T" << idx << ": id = " << sih.AsString() <<
                     ": EXCEPTION = " << e.what());
            ok = false;
            if ( e.GetErrCode() == CLoaderException::eNoConnection ) {
                break;
            }
            if ( ++error_count > kMaxErrorCount ) {
                break;
            }
        }
        catch (exception& e) {
            LOG_POST("T" << idx << ": id = " << sih.AsString() <<
                     ": EXCEPTION = " << e.what());
            ok = false;
            if ( ++error_count > kMaxErrorCount ) {
                break;
            }
        }
        scope.ResetHistory();
    }
    if ( !ok ) {
        failed = true;
    }
    return ok;
}

bool CTestOM::TestApp_Args( CArgDescriptions& args)
{
    args.AddDefaultKey
        ("fromgi", "FromGi",
         "Process sequences in the interval FROM this Gi",
         CArgDescriptions::eInteger, NStr::IntToString(g_gi_from));
    args.AddDefaultKey
        ("togi", "ToGi",
         "Process sequences in the interval TO this Gi",
         CArgDescriptions::eInteger, NStr::IntToString(g_gi_to));
    args.AddOptionalKey
        ("idlist", "IdList",
         "File with list of Seq-ids to test",
         CArgDescriptions::eInputFile);
    args.AddFlag("load_only", "Do not work with sequences - only load them");
    args.AddFlag("no_snp", "Exclude SNP features from processing");
    return true;
}

bool CTestOM::TestApp_Init(void)
{
    failed = false;
    CORE_SetLOCK(MT_LOCK_cxx2c());
    CORE_SetLOG(LOG_cxx2c());

    const CArgs& args = GetArgs();
    if ( args["idlist"] ) {
        CNcbiIstream& file = args["idlist"].AsInputFile();
        string line;
        while ( getline(file, line) ) {
            size_t comment = line.find('#');
            if ( comment != NPOS ) {
                line.erase(comment);
            }
            line = NStr::TruncateSpaces(line);
            if ( line.empty() ) {
                continue;
            }
            
            CSeq_id sid(line);
            CSeq_id_Handle sih = CSeq_id_Handle::GetHandle(sid);
            if ( !sih ) {
                continue;
            }
            m_Ids.push_back(sih);
        }
        
        NcbiCout << "Testing ObjectManager (" <<
            m_Ids.size() << " Seq-ids from file)..." << NcbiEndl;
    }
    if ( m_Ids.empty() ) {
        int gi_from  = args["fromgi"].AsInteger();
        int gi_to    = args["togi"].AsInteger();
        int delta = gi_to > gi_from? 1: -1;
        for ( int gi = gi_from; gi != gi_to+delta; gi += delta ) {
            m_Ids.push_back(CSeq_id_Handle::GetGiHandle(gi));
        }
        NcbiCout << "Testing ObjectManager ("
            "gi from " << gi_from << " to " << gi_to << ")..." << NcbiEndl;
    }
    m_load_only = args["load_only"];
    m_no_snp = args["no_snp"];

    m_ObjMgr = CObjectManager::GetInstance();
    CGBDataLoader::RegisterInObjectManager(*m_ObjMgr);

    // Scope shared by all threads
/*
    m_Scope = new CScope(*m_ObjMgr);
    m_Scope->AddDefaults();
*/
    return true;
}

bool CTestOM::TestApp_Exit(void)
{
    if ( failed ) {
        NcbiCout << " Failed" << NcbiEndl << NcbiEndl;
    }
    else {
        NcbiCout << " Passed" << NcbiEndl << NcbiEndl;
    }

/*
    map<int, int>::iterator it;
    for (it = m_mapGiToDesc.begin(); it != m_mapGiToDesc.end(); ++it) {
        LOG_POST(
            "gi = "         << it->first
            << ": desc = "  << it->second
            << ", feat0 = " << m_mapGiToFeat0[it->first]
            << ", feat1 = " << m_mapGiToFeat1[it->first]
            );
    }
*/
    return true;
}
END_NCBI_SCOPE


/////////////////////////////////////////////////////////////////////////////
//  MAIN

USING_NCBI_SCOPE;

int main(int argc, const char* argv[])
{
    s_NumThreads = 12;
    return CTestOM().AppMain(argc, argv, 0, eDS_Default, 0);
}
