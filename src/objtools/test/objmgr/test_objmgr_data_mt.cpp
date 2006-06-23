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
#include <objmgr/annot_ci.hpp>
#include <objmgr/prefetch_manager.hpp>
#include <objmgr/prefetch_actions.hpp>
#include <objtools/data_loaders/genbank/gbloader.hpp>
#include <connect/ncbi_core_cxx.hpp>
#include <connect/ncbi_util.h>

#include <algorithm>
#include <map>

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
const int g_acc_from = 1;

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

    typedef vector<CConstRef<CSeq_feat> >   TFeats;
    typedef pair<CSeq_id_Handle, bool>      TMapKey;
    typedef map<TMapKey, TFeats>            TFeatMap;
    typedef map<TMapKey, int>               TIntMap;
    typedef map<TMapKey, CBlobIdKey>        TBlobIdMap;
    typedef vector<CSeq_id_Handle>          TIds;

    CRef<CObjectManager> m_ObjMgr;

    TBlobIdMap  m_BlobIdMap;
    TIntMap     m_DescMap;
    TFeatMap    m_Feat0Map;
    TFeatMap    m_Feat1Map;

    void SetValue(TBlobIdMap& vm, const TMapKey& key, const CBlobIdKey& value);
    void SetValue(TFeatMap& vm, const TMapKey& key, const TFeats& value);
    void SetValue(TIntMap& vm, const TMapKey& key, int value);

    TIds m_Ids;

    bool m_load_only;
    bool m_no_seq_map;
    bool m_no_snp;
    bool m_no_named;
    bool m_adaptive;
    int  m_pass_count;
    bool m_no_reset;
    bool m_keep_handles;
    bool m_verbose;
    CRef<CPrefetchManager> m_prefetch_manager;

    bool failed;
};


/////////////////////////////////////////////////////////////////////////////


void CTestOM::SetValue(TBlobIdMap& vm, const TMapKey& key,
                       const CBlobIdKey& value)
{
    const CBlobIdKey* old_value;
    {{
        CFastMutexGuard guard(s_GlobalLock);
        TBlobIdMap::iterator it = vm.lower_bound(key);
        if ( it == vm.end() || it->first != key ) {
            it = vm.insert(it, TBlobIdMap::value_type(key, value));
        }
        old_value = &it->second;
    }}
    if ( *old_value != value ) {
        string name;
        if ( &vm == &m_BlobIdMap ) name = "blob-id";
        ERR_POST("Inconsistent "<<name<<" on "<<
                 key.first.AsString()<<" "<<key.second<<
                 " was "<<old_value->ToString()<<" now "<<value.ToString());
    }
    _ASSERT(*old_value == value);
}


void CTestOM::SetValue(TIntMap& vm, const TMapKey& key, int value)
{
    int old_value;
    {{
        CFastMutexGuard guard(s_GlobalLock);
        old_value = vm.insert(TIntMap::value_type(key, value)).first->second;
    }}
    if ( old_value != value ) {
        string name;
        if ( &vm == &m_DescMap ) name = "desc";
        ERR_POST("Inconsistent "<<name<<" on "<<
                 key.first.AsString()<<" "<<key.second<<
                 " was "<<old_value<<" now "<<value);
    }
    _ASSERT(old_value == value);
}


void CTestOM::SetValue(TFeatMap& vm, const TMapKey& key, const TFeats& value)
{
    const TFeats* old_value;
    {{
        CFastMutexGuard guard(s_GlobalLock);
        TFeatMap::iterator it = vm.lower_bound(key);
        if ( it == vm.end() || it->first != key ) {
            it = vm.insert(it, TFeatMap::value_type(key, value));
        }
        old_value = &it->second;
    }}
    if ( old_value->size() != value.size() ) {
        CNcbiOstrstream s;
        string name;
        if ( &vm == &m_Feat0Map ) name = "feat0";
        if ( &vm == &m_Feat1Map ) name = "feat1";
        s << "Inconsistent "<<name<<" on "<<
            key.first.AsString()<<" "<<key.second<<
            " was "<<old_value->size()<<" now "<<value.size() << NcbiEndl;
        ITERATE ( TFeats, it, *old_value ) {
            s << " old: " << MSerial_AsnText << **it;
        }
        ITERATE ( TFeats, it, value ) {
            s << " new: " << MSerial_AsnText << **it;
        }
        ERR_POST(string(CNcbiOstrstreamToString(s)));
    }
    _ASSERT(old_value->size() == value.size());
}


bool CTestOM::Thread_Run(int idx)
{
// initialize scope
    CScope scope(*m_ObjMgr);
    scope.AddDefaults();

    vector<CSeq_id_Handle> ids = m_Ids;
    
    // make them go in opposite directions
    bool rev = idx % 2;
    if ( rev ) {
        reverse(ids.begin(), ids.end());
    }

    SAnnotSelector sel(CSeqFeatData::e_not_set);
    if ( m_no_named ) {
        sel.ResetAnnotsNames().AddUnnamedAnnots();
    }
    else if ( m_no_snp ) {
        sel.ExcludeNamedAnnots("SNP");
    }
    if ( m_adaptive ) {
        sel.SetAdaptiveDepth();
    }
    if ( idx%2 == 0 ) {
        sel.SetOverlapType(sel.eOverlap_Intervals);
        sel.SetResolveMethod(sel.eResolve_All);
    }
    
    set<CBioseq_Handle> handles;

    bool ok = true;
    for ( int pass = 0; pass < m_pass_count; ++pass ) {
        CRef<CPrefetchSequence> prefetch;
        if ( m_prefetch_manager ) {
            // Initialize prefetch token;
            prefetch = new CPrefetchSequence
                (*m_prefetch_manager,
                 new CPrefetchFeat_CIActionSource(CScopeSource::New(scope),
                                                  ids, sel));
        }

        const int kMaxErrorCount = 3;
        static int error_count = 0;
        TFeats feats;
        for ( size_t i = 0; i < ids.size(); ++i ) {
            CSeq_id_Handle sih = ids[i];
            CNcbiOstrstream out;
            if ( m_verbose ) {
                out << CTime(CTime::eCurrent).AsString() << " " << i << ": "
                    << sih.AsString();
            }
            TMapKey key(sih, rev);
            try {
                // load sequence
                bool preload_ids = !prefetch && ((idx ^ i) & 2) != 0;
                vector<CSeq_id_Handle> ids1, ids2, ids3;
                if ( preload_ids ) {
                    ids1 = scope.GetIds(sih);
                    sort(ids1.begin(), ids1.end());
                }

                CPrefetchToken token;
                if ( prefetch ) {
                    token = prefetch->GetNextToken();
                }
                CBioseq_Handle handle;
                if ( token ) {
                    handle = CStdPrefetch::GetBioseqHandle(token);
                }
                else {
                    handle = scope.GetBioseqHandle(sih);
                }

                if ( !handle ) {
                    if ( preload_ids ) {
                        _ASSERT(ids1.empty());
                    }
                    LOG_POST("T" << idx << ": id = " << sih.AsString() <<
                             ": INVALID HANDLE");
                    SetValue(m_DescMap, key, -1);
                    continue;
                }
                SetValue(m_BlobIdMap, key, handle.GetTSE_Handle().GetBlobId());

                ids2 = handle.GetId();
                sort(ids2.begin(), ids2.end());
                _ASSERT(!ids2.empty());
                ids3 = handle.GetScope().GetIds(sih);
                sort(ids3.begin(), ids3.end());
                _ASSERT(ids2 == ids3);
                if ( preload_ids ) {
                    if ( 1 ) {
                        if ( ids1.empty() ) {
                            ERR_POST("Ids discrepancy for " << sih.AsString());
                        }
                    }
                    else {
                        if ( ids1 != ids2 ) {
                            ERR_POST("Ids discrepancy for " << sih.AsString());
                        }
                        //_ASSERT(ids1 == ids2);
                    }
                }

                if ( !m_load_only ) {
                    // check CSeqMap_CI
                    if ( !m_no_seq_map ) {
                        SSeqMapSelector sel(CSeqMap::fFindRef, kMax_UInt);
                        for ( CSeqMap_CI it(handle, sel); it; ++it ) {
                            _ASSERT(it.GetType() == CSeqMap::eSeqRef);
                        }
                    }

                    // enumerate descriptions
                    // Seqdesc iterator
                    int desc_count = 0;
                    for (CSeqdesc_CI desc_it(handle); desc_it;  ++desc_it) {
                        desc_count++;
                    }
                    // verify result
                    SetValue(m_DescMap, key, desc_count);

                    // enumerate features
                    CSeq_loc loc;
                    loc.SetWhole(const_cast<CSeq_id&>(*sih.GetSeqId()));

                    feats.clear();
                    set<CSeq_annot_Handle> annots;
                    if ( idx%2 == 0 ) {
                        CFeat_CI it;
                        if ( token ) {
                            it = CStdPrefetch::GetFeat_CI(token);
                        }
                        else {
                            it = CFeat_CI(scope, loc, sel);
                        }
                        for ( ; it;  ++it ) {
                            feats.push_back(ConstRef(&it->GetOriginalFeature()));
                            annots.insert(it.GetAnnot());
                        }
                        CAnnot_CI annot_it(handle.GetScope(), loc, sel);
                        if ( m_verbose ) {
                            out << " Seq-annots: " << annot_it.size()
                                << " features: " << feats.size();
                        }

#if NCBI_DEVELOPMENT_VER >= 20060627
                        // verify result
                        SetValue(m_Feat0Map, key, feats);

                        _ASSERT(annot_it.size() == annots.size());
                        set<CSeq_annot_Handle> annots2;
                        for ( ; annot_it; ++annot_it ) {
                            annots2.insert(*annot_it);
                        }
                        _ASSERT(annots.size() == annots2.size());
                        _ASSERT(annots == annots2);
#endif
                    }
                    else if ( idx%4 == 1 ) {
                        CFeat_CI it;
                        if ( token ) {
                            it = CStdPrefetch::GetFeat_CI(token);
                        }
                        else {
                            it = CFeat_CI(handle, sel);
                        }

                        for ( ; it;  ++it ) {
                            feats.push_back(ConstRef(&it->GetOriginalFeature()));
                            annots.insert(it.GetAnnot());
                        }
                        CAnnot_CI annot_it(handle, sel);
                        if ( m_verbose ) {
                            out << " Seq-annots: " << annot_it.size()
                                << " features: " << feats.size();
                        }

#if NCBI_DEVELOPMENT_VER >= 20060627
                        // verify result
                        SetValue(m_Feat1Map, key, feats);

                        _ASSERT(annot_it.size() == annots.size());
                        set<CSeq_annot_Handle> annots2;
                        for ( ; annot_it; ++annot_it ) {
                            annots2.insert(*annot_it);
                        }
                        _ASSERT(annots.size() == annots2.size());
                        _ASSERT(annots == annots2);
#endif
                    }
                    else {
                        CFeat_CI feat_it(handle.GetTopLevelEntry(), sel);
                        for ( ; feat_it; ++feat_it ) {
                            feats.push_back(ConstRef(&feat_it->GetOriginalFeature()));
                            annots.insert(feat_it.GetAnnot());
                        }
                        CAnnot_CI annot_it(feat_it);
                        if ( m_verbose ) {
                            out << " Seq-annots: " << annot_it.size()
                                << " features: " << feats.size();
                        }

#if NCBI_DEVELOPMENT_VER >= 20060627
                        _ASSERT(annot_it.size() == annots.size());
                        set<CSeq_annot_Handle> annots2;
                        for ( ; annot_it; ++annot_it ) {
                            annots2.insert(*annot_it);
                        }
                        _ASSERT(annots.size() == annots2.size());
                        _ASSERT(annots == annots2);
#endif
                    }
                }
                if ( m_verbose ) {
                    out << NcbiEndl;
                    string msg = CNcbiOstrstreamToString(out);
                    NcbiCout << msg << NcbiFlush;
                }
                if ( m_no_reset && m_keep_handles ) {
                    handles.insert(handle);
                }
            }
            catch (CLoaderException& e) {
                if ( m_verbose ) {
                    string msg = CNcbiOstrstreamToString(out);
                    LOG_POST("T" << idx << ": " << msg);
                }
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
                if ( m_verbose ) {
                    string msg = CNcbiOstrstreamToString(out);
                    LOG_POST("T" << idx << ": " << msg);
                }
                LOG_POST("T" << idx << ": id = " << sih.AsString() <<
                         ": EXCEPTION = " << e.what());
                ok = false;
                if ( ++error_count > kMaxErrorCount ) {
                    break;
                }
            }
            if ( !m_no_reset ) {
                scope.ResetHistory();
            }
        }
    }

    if ( !ok ) {
        failed = true;
    }
    return ok;
}

bool CTestOM::TestApp_Args( CArgDescriptions& args)
{
    args.AddOptionalKey
        ("fromgi", "FromGi",
         "Process sequences in the interval FROM this Gi",
         CArgDescriptions::eInteger);
    args.AddOptionalKey
        ("togi", "ToGi",
         "Process sequences in the interval TO this Gi",
         CArgDescriptions::eInteger);
    args.AddOptionalKey
        ("idlist", "IdList",
         "File with list of Seq-ids to test",
         CArgDescriptions::eInputFile);
    args.AddFlag("load_only", "Do not work with sequences - only load them");
    args.AddFlag("no_seq_map", "Do not scan CSeqMap on the sequence");
    args.AddFlag("no_snp", "Exclude SNP features from processing");
    args.AddFlag("no_named", "Exclude features from named Seq-annots");
    args.AddFlag("adaptive", "Use adaptive depth for feature iteration");
    args.AddDefaultKey("pass_count", "PassCount",
                       "Run test several times",
                       CArgDescriptions::eInteger, "1");
    args.AddFlag("no_reset", "Do not reset scope history after each id");
    args.AddFlag("keep_handles",
                 "Remember bioseq handles if not resetting scope history");
    args.AddFlag("verbose", "Print each Seq-id before processing");
    args.AddFlag("prefetch", "Use prefetching");
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
    if ( m_Ids.empty() && (args["fromgi"] || args["togi"]) ) {
        int gi_from  = args["fromgi"]? args["fromgi"].AsInteger(): g_gi_from;
        int gi_to    = args["togi"]? args["togi"].AsInteger(): g_gi_to;
        int delta = gi_to > gi_from? 1: -1;
        for ( int gi = gi_from; gi != gi_to+delta; gi += delta ) {
            m_Ids.push_back(CSeq_id_Handle::GetGiHandle(gi));
        }
        NcbiCout << "Testing ObjectManager ("
            "gi from " << gi_from << " to " << gi_to << ")..." << NcbiEndl;
    }
    if ( m_Ids.empty() ) {
        int count = g_gi_to-g_gi_from+1;
        for ( int i = 0; i < count; ++i ) {
            if ( i % 3 != 0 ) {
                m_Ids.push_back(CSeq_id_Handle::GetGiHandle(i+g_gi_from));
            }
            else {
                CNcbiOstrstream str;
                str << "AA" << setfill('0') << setw(6) << (i/3+g_acc_from);
                string acc = CNcbiOstrstreamToString(str);
                CSeq_id seq_id(acc);
                m_Ids.push_back(CSeq_id_Handle::GetHandle(seq_id));
            }
        }
        NcbiCout << "Testing ObjectManager ("
            "accessions and gi from " <<
            g_gi_from << " to " << g_gi_to << ")..." << NcbiEndl;
    }
    m_load_only = args["load_only"];
    m_no_seq_map = args["no_seq_map"];
    m_no_snp = args["no_snp"];
    m_no_named = args["no_named"];
    m_adaptive = args["adaptive"];
    m_pass_count = args["pass_count"].AsInteger();
    m_no_reset = args["no_reset"];
    m_keep_handles = args["keep_handles"];
    m_verbose = args["verbose"];

    m_ObjMgr = CObjectManager::GetInstance();
    CGBDataLoader::RegisterInObjectManager(*m_ObjMgr);

    if ( args["prefetch"] ) {
        LOG_POST("Using prefetch");
        m_prefetch_manager = new CPrefetchManager();
    }

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
    for (it = m_DescMap.begin(); it != m_DescMap.end(); ++it) {
        LOG_POST(
            "gi = "         << it->first
            << ": desc = "  << it->second
            << ", feat0 = " << m_Feat0Map[it->first]
            << ", feat1 = " << m_Feat1Map[it->first]
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
    return CTestOM().AppMain(argc, argv);
}

/*
* ===========================================================================
*
* $Log$
* Revision 1.19  2006/06/23 21:31:21  vasilche
* Temporarily disable some tests until GenBank data will be fixed.
*
* Revision 1.18  2006/06/05 15:28:35  vasilche
* Use real limited prefetch with separate scopes.
*
* Revision 1.17  2006/03/15 14:48:18  ucko
* +<algorithm> for sort(); -<vector>, already included via ncbistd.hpp.
* Move CVS log to end.
*
* Revision 1.16  2005/09/12 16:05:25  vasilche
* Report major GetIds inconsistency instead of abort.
*
* Revision 1.15  2005/09/12 14:33:19  vasilche
* Disable comprehensive test of GetIds().
*
* Revision 1.14  2005/09/07 19:16:35  vasilche
* Add test for accession fetching.
* Add test for scope.GetIds().
*
* Revision 1.13  2005/08/05 15:50:10  vasilche
* Reordered calls to make error messages more informative.
*
* Revision 1.12  2005/06/22 14:35:47  vasilche
* Added test of non-location feature iterators.
* Added test of CAnnot_CI.
*
* Revision 1.11  2005/03/15 19:16:55  vasilche
* Removed unnecessary includes.
*
* Revision 1.10  2005/02/28 17:11:54  vasilche
* Use correct AppMain() arguments.
*
* Revision 1.9  2004/12/22 15:56:43  vasilche
* Add options -pass_count and -no_reset.
*
* Revision 1.8  2004/11/01 19:33:10  grichenk
* Removed deprecated methods
*
* Revision 1.7  2004/08/12 22:28:41  vasilche
* More details about inconsistency between threads.
*
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
* ===========================================================================
*/
