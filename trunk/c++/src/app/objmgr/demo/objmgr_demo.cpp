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
* Author:  Aleksey Grichenko
*
* File Description:
*   Examples of using the C++ object manager
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbi_system.hpp>

// Objects includes
#include <objects/seq/Bioseq.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <objects/seq/Seq_inst.hpp>
#include <objects/seq/Seq_ext.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objects/seqset/Bioseq_set.hpp>
#include <objects/seqfeat/seqfeat__.hpp>
#include <objects/seqalign/Seq_align.hpp>

// Object manager includes
#include <objmgr/scope.hpp>
#include <objmgr/seq_vector.hpp>
#include <objmgr/desc_ci.hpp>
#include <objmgr/feat_ci.hpp>
#include <objmgr/graph_ci.hpp>
#include <objmgr/align_ci.hpp>
#include <objmgr/bioseq_ci.hpp>
#include <objmgr/seq_annot_ci.hpp>
#include <objmgr/impl/synonyms.hpp>

#include <objmgr/object_manager.hpp>
#include <objtools/data_loaders/genbank/gbloader.hpp>
#include <objtools/data_loaders/genbank/readers/id1/reader_id1_cache.hpp>
#include <bdb/bdb_blobcache.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);


/////////////////////////////////////////////////////////////////////////////
//
//  Demo application
//


class CDemoApp : public CNcbiApplication
{
public:
    virtual void Init(void);
    virtual int  Run (void);

    CRef<CGBDataLoader> gb_loader;
    auto_ptr<CBDB_BLOB_Cache> bdb_cache;
    auto_ptr<CBDB_Cache> blob_cache;
    auto_ptr<CBDB_Cache> id_cache;
};


void CDemoApp::Init(void)
{
    // Prepare command line descriptions
    //

    // Create
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // GI to fetch
    arg_desc->AddOptionalKey("gi", "SeqEntryID",
                             "GI id of the Seq-Entry to fetch",
                             CArgDescriptions::eInteger);
    arg_desc->AddOptionalKey("id", "SeqEntryID",
                             "Seq-id of the Seq-Entry to fetch",
                             CArgDescriptions::eString);
    arg_desc->AddOptionalKey("asn_id", "SeqEntryID",
                             "ASN.1 of Seq-id of the Seq-Entry to fetch",
                             CArgDescriptions::eString);
    arg_desc->AddOptionalKey("file", "SeqEntryFile",
                             "file with Seq-Entry to load (text ASN.1)",
                             CArgDescriptions::eInputFile);
    arg_desc->AddOptionalKey("bfile", "SeqEntryFile",
                             "file with Seq-Entry to load (binary ASN.1)",
                             CArgDescriptions::eInputFile);
    arg_desc->AddDefaultKey("count", "RepeatCount",
                            "repeat test work RepeatCount times",
                            CArgDescriptions::eInteger, "1");
    arg_desc->AddDefaultKey("pause", "Pause",
                            "pause between tests in seconds",
                            CArgDescriptions::eInteger, "0");

    arg_desc->AddDefaultKey("resolve", "ResolveMethod",
                            "Method of segments resolution",
                            CArgDescriptions::eString, "tse");
    arg_desc->SetConstraint("resolve",
                            &(*new CArgAllow_Strings,
                              "none", "tse", "all"));

    arg_desc->AddDefaultKey("loader", "Loader",
                            "Use specified loaders",
                            CArgDescriptions::eString, "");


    arg_desc->AddFlag("seq_map", "scan SeqMap on full depth");
    arg_desc->AddFlag("print_features", "print all found features");
    arg_desc->AddFlag("only_features", "do only one scan of features");
    arg_desc->AddFlag("get_mapped_location", "get mapped location");
    arg_desc->AddFlag("get_original_feature", "get original location");
    arg_desc->AddFlag("get_mapped_feature", "get mapped feature");
    arg_desc->AddFlag("print_alignments", "print all found Seq-aligns");
    arg_desc->AddFlag("reverse", "reverse order of features");
    arg_desc->AddFlag("no_sort", "do not sort features");
    arg_desc->AddDefaultKey("max_feat", "MaxFeat",
                            "Max number of features to iterate",
                            CArgDescriptions::eInteger, "0");
    arg_desc->AddDefaultKey("depth", "depth",
                            "Max depth of segments to iterate",
                            CArgDescriptions::eInteger, "100");
    arg_desc->AddFlag("adaptive", "Use adaptive depth of segments");
    arg_desc->AddFlag("unnamed",
                      "include features from unnamed Seq-annots");
    arg_desc->AddOptionalKey("named", "NamedAnnots",
                             "include features from named Seq-annots "
                             "(comma separated list)",
                             CArgDescriptions::eString);
    arg_desc->AddFlag("allnamed",
                      "include features from all named Seq-annots");
    arg_desc->AddFlag("nosnp",
                      "exclude snp features - only unnamed Seq-annots");
    arg_desc->AddOptionalKey("exclude_named", "ExcludeNamedAnnots",
                             "exclude features from named Seq-annots"
                             "(comma separated list)",
                             CArgDescriptions::eString);
    arg_desc->AddDefaultKey("feat_type", "FeatType",
                            "Type of features to select",
                            CArgDescriptions::eInteger, "-1");
    arg_desc->AddDefaultKey("feat_subtype", "FeatSubType",
                            "Subtype of features to select",
                            CArgDescriptions::eInteger, "-1");

    arg_desc->AddFlag("cache",
                      "use BDB cache");
    arg_desc->AddDefaultKey("cache_mode", "CacheMode",
                            "Cache classes to use",
                            CArgDescriptions::eString, "old");
    arg_desc->SetConstraint("cache_mode",
                            &(*new CArgAllow_Strings,
                              "old", "newid", "new"));
    arg_desc->AddDefaultKey("id_cache_days", "id_cache_days",
                            "number of days to keep gi->sat/satkey cache",
                            CArgDescriptions::eInteger, "1");

    // Program description
    string prog_description = "Example of the C++ object manager usage\n";
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              prog_description, false);

    // Pass argument descriptions to the application
    //

    SetupArgDescriptions(arg_desc.release());
}


extern CAtomicCounter newCObjects;

int CDemoApp::Run(void)
{
    // Process command line args: get GI to load
    const CArgs& args = GetArgs();

    // Create seq-id, set it to GI specified on the command line
    CRef<CSeq_id> id;
    if ( args["gi"] ) {
        int gi = args["gi"].AsInteger();
        id.Reset(new CSeq_id);
        id->SetGi(gi);
    }
    else if ( args["id"] ) {
        id.Reset(new CSeq_id(args["id"].AsString()));
    }
    else if ( args["asn_id"] ) {
        id.Reset(new CSeq_id);
        string text = args["asn_id"].AsString();
        if ( text.find("::=") == NPOS ) {
            text = "Seq-id ::= " + text;
        }
        CNcbiIstrstream ss(text.c_str());
        ss >> MSerial_AsnText >> *id;
    }
    else {
        ERR_POST(Fatal << "One of -gi, -id or -asn_id arguments is required");
    }

    CFeat_CI::EResolveMethod resolve = CFeat_CI::eResolve_TSE;
    if ( args["resolve"].AsString() == "all" )
        resolve = CFeat_CI::eResolve_All;
    if ( args["resolve"].AsString() == "none" )
        resolve = CFeat_CI::eResolve_None;
    if ( args["resolve"].AsString() == "tse" )
        resolve = CFeat_CI::eResolve_TSE;
    if ( !args["loader"].AsString().empty() ) {
        string env = "GENBANK_LOADER_METHOD="+args["loader"].AsString();
        ::putenv(::strdup(env.c_str()));
    }

    int repeat_count = args["count"].AsInteger();
    int pause = args["pause"].AsInteger();
    bool only_features = args["only_features"];
    bool print_features = args["print_features"];
    bool get_mapped_location = args["get_mapped_location"];
    bool get_original_feature = args["get_original_feature"];
    bool get_mapped_feature = args["get_mapped_feature"];
    bool print_alignments = args["print_alignments"];
    SAnnotSelector::ESortOrder order =
        args["reverse"] ?
        SAnnotSelector::eSortOrder_Reverse : SAnnotSelector::eSortOrder_Normal;
    if ( args["no_sort"] )
        order = SAnnotSelector::eSortOrder_None;
    int max_feat = args["max_feat"].AsInteger();
    int depth = args["depth"].AsInteger();
    bool adaptive = args["adaptive"];
    int feat_type = args["feat_type"].AsInteger();
    int feat_subtype = args["feat_subtype"].AsInteger();
    bool nosnp = args["nosnp"];
    bool include_unnamed = args["unnamed"];
    bool include_allnamed = args["allnamed"];
    set<string> include_named;
    if ( args["named"] ) {
        string names = args["named"].AsString();
        size_t comma_pos;
        while ( (comma_pos = names.find(',')) != NPOS ) {
            include_named.insert(names.substr(0, comma_pos));
            names.erase(0, comma_pos+1);
        }
        include_named.insert(names);
    }
    set<string> exclude_named;
    if ( args["exclude_named"] ) {
        string names = args["exclude_named"].AsString();
        size_t comma_pos;
        while ( (comma_pos = names.find(',')) != NPOS ) {
            exclude_named.insert(names.substr(0, comma_pos));
            names.erase(0, comma_pos+1);
        }
        exclude_named.insert(names);
    }
    bool scan_seq_map = args["seq_map"];

    // Create object manager. Use CRef<> to delete the OM on exit.
    CRef<CObjectManager> pOm(new CObjectManager);

    if ( args["cache"] ) {
        // caching options
        CNcbiRegistry& reg = GetConfig();

        string cache_path = ".genbank_cache";
        //CSystemPath::ResolvePath("<home>", "cache");
        cache_path    = reg.GetString("LOCAL_CACHE", "Path", cache_path,
                                      CNcbiRegistry::eErrPost);
        int cache_age = reg.GetInt("LOCAL_CACHE", "Age", 5,
                                   CNcbiRegistry::eErrPost);
        if (cache_age == 0) {
            cache_age = 5;   // keep objects for 5 days (default)
        }

        auto_ptr<CCachedId1Reader> id1_reader;

        if (!cache_path.empty()) {
            try {
                {{
                    // make sure our cache directory exists first
                    CDir dir(cache_path);
                    if ( !dir.Exists() ) {
                        dir.Create();
                    }
                }}

                // setup blob cache
                CCachedId1Reader* rdr;
                string cache_mode = args["cache_mode"].AsString();
                if ( cache_mode == "new" ) {
                    blob_cache.reset(new CBDB_Cache());

                    ICache::TTimeStampFlags flags =
                        ICache::fTimeStampOnRead |
                        ICache::fExpireLeastFrequentlyUsed |
                        ICache::fPurgeOnStartup;
                    blob_cache->SetTimeStampPolicy(flags, cache_age*24*60*60);

                    blob_cache->Open(cache_path.c_str(), "blobs");

                    rdr = new CCachedId1Reader(5, blob_cache.get());
                }
                else {
                    bdb_cache.reset(new CBDB_BLOB_Cache());

                    bdb_cache->Open(cache_path.c_str());
                    
                    // Cache cleaning
                    // Objects age should be assigned in days, negative value
                    // means cleaning is disabled
                    if (cache_age) {
                        CTime time_stamp(CTime::eCurrent);
                        time_t age = time_stamp.GetTimeT();
                        
                        age -= 60 * 60 * 24 * cache_age;
                        
                        bdb_cache->Purge(age);
                    }

                    rdr = new CCachedId1Reader(5, bdb_cache.get());
                }
                id1_reader.reset(rdr);

                int id_days = args["id_cache_days"].AsInteger();
                if ( cache_mode != "old" ) {
                    id_cache.reset(new CBDB_Cache());

                    ICache::TTimeStampFlags flags =
                        ICache::fTimeStampOnCreate|
                        ICache::fCheckExpirationAlways;
                    id_cache->SetTimeStampPolicy(flags, id_days*24*60*60);

                    id_cache->Open((cache_path+"/id").c_str(), "ids");

                    rdr->SetIdCache(id_cache.get());
                }
                else {
                    bdb_cache->GetIntCache()->
                        SetExpirationTime(id_days*24*60*60);

                    rdr->SetIdCache(bdb_cache->GetIntCache());
                }
                LOG_POST(Info << "ID1 cache enabled in " << cache_path);
            }
            catch(CException& e) {
                LOG_POST(Error << "ID1 cache initialization failed in "
                         << cache_path << ": "
                         << e.what());
            }
#ifndef _DEBUG
            catch(...) {
                LOG_POST(Error << "ID1 cache initialization failed in "
                         << cache_path);
            }
#endif
        } else {
            LOG_POST(Info << "ID1 cache disabled.");
        }

        gb_loader = new CGBDataLoader("GenBank", id1_reader.release(), 2);
    }
    else {
        // Create genbank data loader and register it with the OM.
        // The last argument "eDefault" informs the OM that the loader
        // must be included in scopes during the CScope::AddDefaults() call
        gb_loader = new CGBDataLoader("ID", 0, 2);
    }
    pOm->RegisterDataLoader(*gb_loader, CObjectManager::eDefault);

    // Create a new scope.
    CScope scope(*pOm);
    // Add default loaders (GB loader in this demo) to the scope.
    scope.AddDefaults();

    if ( args["file"] ) {
        CRef<CSeq_entry> entry(new CSeq_entry);
        args["file"].AsInputFile() >> MSerial_AsnText >> *entry;
        scope.AddTopLevelSeqEntry(*entry);
    }
    if ( args["bfile"] ) {
        CRef<CSeq_entry> entry(new CSeq_entry);
        args["bfile"].AsInputFile() >> MSerial_AsnBinary >> *entry;
        scope.AddTopLevelSeqEntry(*entry);
    }

    {{
        CConstRef<CSeqref> sr = gb_loader->GetSatSatkey(*id);
        if ( !sr ) {
            ERR_POST("Cannot resolve Seq-id "<<id->AsFastaString());
        }
        else {
            NcbiCout << "Resolved: "<<id->AsFastaString()<<
                " gi="<<sr->GetGi()<<
                " sat="<<sr->GetSat()<<" satkey="<<sr->GetSatKey()<<NcbiEndl;
        }
    }}

    // Get bioseq handle for the seq-id. Most of requests will use this handle.
    CBioseq_Handle handle = scope.GetBioseqHandle(*id);
    // Check if the handle is valid
    if ( !handle ) {
        ERR_POST(Fatal << "Bioseq not found");
    }
    if ( !id->IsGi() ) {
        CConstRef<CSynonymsSet> syns = scope.GetSynonyms(handle);
        ITERATE ( CSynonymsSet, it, *syns ) {
            CConstRef<CSeq_id> seq_id =
                CSynonymsSet::GetSeq_id_Handle(it).GetSeqId();
            if ( seq_id->IsGi() ) {
                int gi = seq_id->GetGi();
                NcbiCout << "Sequence gi is "<<gi<<NcbiEndl;
                break;
            }
        }
    }

    CSeq_id_Handle master_id = CSeq_id_Handle::GetHandle(*id);

    for ( int c = 0; c < repeat_count; ++c ) {
        if ( c && pause ) {
            SleepSec(pause);
        }

        if ( scan_seq_map ) {
            TSeqRange range(0, handle.GetBioseqLength()-1);
            for (size_t levels = 0;  levels < 4;  ++levels) {
                CConstRef<CSeqMap> seq_map(&handle.GetSeqMap());
                CSeqMap::const_iterator seg =
                    seq_map->ResolvedRangeIterator(&handle.GetScope(),
                                                   range.GetFrom(),
                                                   range.GetLength(),
                                                   eNa_strand_plus, levels);
                
                for ( ;  seg;  ++seg ) {
                    switch (seg.GetType()) {
                    case CSeqMap::eSeqRef:
                        break;
                    case CSeqMap::eSeqData:
                    case CSeqMap::eSeqGap:
                        break;
                    case CSeqMap::eSeqEnd:
                        _ASSERT("Unexpected END segment" && 0);
                        break;
                    default:
                        _ASSERT("Unexpected segment type" && 0);
                        break;
                    }
                }
            }
        }

        string sout;
        int count;
        if ( !only_features ) {
            // List other sequences in the same TSE
            NcbiCout << "TSE sequences:" << NcbiEndl;
            CBioseq_CI bit;
            bit = CBioseq_CI(scope, handle.GetTopLevelSeqEntry());
            for ( ; bit; ++bit) {
                NcbiCout << "    "<<bit->GetSeqId()->DumpAsFasta()<<NcbiEndl;
            }

            // Get the bioseq
            CConstRef<CBioseq> bioseq(&handle.GetBioseq());
            // -- use the bioseq: print the first seq-id
            NcbiCout << "First ID = " <<
                (*bioseq->GetId().begin())->DumpAsFasta() << NcbiEndl;

            // Get the sequence using CSeqVector. Use default encoding:
            // CSeq_data::e_Iupacna or CSeq_data::e_Iupacaa.
            CSeqVector seq_vect = handle.GetSeqVector(CBioseq_Handle::eCoding_Iupac);
            // -- use the vector: print length and the first 10 symbols
            NcbiCout << "Sequence: length=" << seq_vect.size();
            if (seq_vect.CanGetRange(0, seq_vect.size() - 1)) {
                NcbiCout << " data=";
                sout = "";
                for (TSeqPos i = 0; (i < seq_vect.size()) && (i < 10); i++) {
                    // Convert sequence symbols to printable form
                    sout += seq_vect[i];
                }
                NcbiCout << NStr::PrintableString(sout) << NcbiEndl;
            }
            else {
                NcbiCout << " data unavailable" << NcbiEndl;
            }
            // CSeq_descr iterator: iterates all descriptors starting
            // from the bioseq and going the seq-entries tree up to the
            // top-level seq-entry.
            count = 0;
            for (CDesc_CI desc_it(handle);
                 desc_it;  ++desc_it) {
                count++;
            }
            NcbiCout << "Desc count: " << count << NcbiEndl;

            count = 0;
            for (CSeq_annot_CI ai(scope, handle.GetTopLevelSeqEntry());
                 ai; ++ai) {
                ++count;
            }
            NcbiCout << "Seq_annot count (recursive): " << count << NcbiEndl;

            count = 0;
            for (CSeq_annot_CI ai(scope,
                                  handle.GetTopLevelSeqEntry(),
                                  CSeq_annot_CI::eSearch_entry);
                 ai; ++ai) {
                ++count;
            }
            NcbiCout << "Seq_annot count (non-recursive): "
                     << count << NcbiEndl;
        }

        // CSeq_feat iterator: iterates all features which can be found in the
        // current scope including features from all TSEs.
        // Construct seq-loc to get features for.
        CSeq_loc loc;
        // No region restrictions -- the whole bioseq is used:
        loc.SetWhole(*id);
        count = 0;
        // Create CFeat_CI using the current scope and location.
        // No feature type restrictions.
        SAnnotSelector base_sel;
        base_sel
            .SetResolveMethod(resolve)
            .SetSortOrder(order)
            .SetMaxSize(max_feat)
            .SetResolveDepth(depth)
            .SetAdaptiveDepth(adaptive);
        if ( include_allnamed ) {
            base_sel.SetAllNamedAnnots();
        }
        if ( include_unnamed ) {
            base_sel.AddUnnamedAnnots();
        }
        ITERATE ( set<string>, it, include_named ) {
            base_sel.AddNamedAnnots(*it);
        }
        if ( nosnp ) {
            base_sel.ExcludeNamedAnnots("SNP");
        }
        ITERATE ( set<string>, it, exclude_named ) {
            base_sel.ExcludeNamedAnnots(*it);
        }

        {{
            SAnnotSelector sel = base_sel;
            if ( feat_type >= 0 ) {
                sel.SetFeatChoice(SAnnotSelector::TFeatChoice(feat_type));
            }
            if ( feat_subtype >= 0 ) {
                sel.SetFeatSubtype(SAnnotSelector::TFeatSubtype(feat_subtype));
            }
            //int cnt0 = newCObjects.Get();
            CFeat_CI it(scope, loc, sel);
            //int cnt1 = newCObjects.Get();
            for ( ; it;  ++it) {
                count++;
                if ( get_mapped_location )
                    it->GetLocation();
                if ( get_original_feature )
                    it->GetOriginalFeature();
                if ( get_mapped_feature )
                    it->GetMappedFeature();
                
                // Get seq-annot containing the feature
                if ( print_features ) {
                    NcbiCout << "Feature:";
                    if ( it->IsSetPartial() ) {
                        NcbiCout << " partial = " << it->GetPartial();
                    }
                    NcbiCout << "\n" <<
                        MSerial_AsnText << it->GetMappedFeature();
                    if ( 1 ) {
                        NcbiCout << "Original location:\n" <<
                            MSerial_AsnText <<
                            it->GetOriginalFeature().GetLocation();
                    }
                    else {
                        NcbiCout << "Location:\n" <<
                            MSerial_AsnText << it->GetLocation();
                    }
                }
                CConstRef<CSeq_annot> annot(&it.GetSeq_annot());
                /*
                const CSeq_id* mapped_id = 0;
                it->GetLocation().CheckId(mapped_id);
                _ASSERT(mapped_id);
                _ASSERT(CSeq_id_Handle::GetHandle(*mapped_id) == master_id);
                */
            }
            //int cnt2 = newCObjects.Get();
            if ( feat_type >= 0 || feat_subtype >= 0 ) {
                NcbiCout << "Feat count (whole, requested): ";
            }
            else {
                NcbiCout << "Feat count (whole, any):       ";
            }
            NcbiCout << count << NcbiEndl;
            //NcbiCout << "Init new: " << (cnt1 - cnt0) << " iteratr new: " << (cnt2-cnt1) << NcbiEndl;
            _ASSERT(count == (int)it.GetSize());
        }}

        if ( only_features )
            continue;

        {
            count = 0;
            // The same region (whole sequence), but restricted feature type:
            // searching for e_Cdregion features only. If the sequence is
            // segmented (constructed), search for features on the referenced
            // sequences in the same top level seq-entry, ignore far pointers.
            SAnnotSelector sel = base_sel;
            sel.SetFeatChoice(CSeqFeatData::e_Cdregion);
            for ( CFeat_CI it(scope, loc, sel); it;  ++it ) {
                count++;
                // Get seq vector filtered with the current feature location.
                // e_ViewMerged flag forces each residue to be shown only once.
                CSeqVector cds_vect = handle.GetSequenceView
                    (it->GetLocation(), CBioseq_Handle::eViewMerged,
                     CBioseq_Handle::eCoding_Iupac);
                // Print first 10 characters of each cd-region
                NcbiCout << "cds" << count << " len=" << cds_vect.size() << " data=";
                if ( cds_vect.size() == 0 ) {
                    NcbiCout << "Zero size from: " << MSerial_AsnText <<
                        it->GetOriginalFeature().GetLocation();
                    NcbiCout << "Zero size to: " << MSerial_AsnText <<
                        it->GetMappedFeature().GetLocation();
                    NcbiCout << "Zero size to: " << MSerial_AsnText <<
                        it->GetLocation();

                    CSeqVector v2 = handle.GetSequenceView
                        (it->GetLocation(), CBioseq_Handle::eViewMerged,
                         CBioseq_Handle::eCoding_Iupac);
                    NcbiCout << v2.size() << NcbiEndl;

                    const CSeq_id* mapped_id = 0;
                    it->GetMappedFeature().GetLocation().CheckId(mapped_id);
                    _ASSERT(mapped_id);
                    _ASSERT(CSeq_id_Handle::GetHandle(*mapped_id)==master_id);
                }
                sout = "";
                for (TSeqPos i = 0; (i < cds_vect.size()) && (i < 10); i++) {
                    // Convert sequence symbols to printable form
                    sout += cds_vect[i];
                }
                NcbiCout << NStr::PrintableString(sout) << NcbiEndl;
            }
            NcbiCout << "Feat count (whole, cds):      " << count << NcbiEndl;
        }

        // Region set to interval 0..9 on the bioseq. Any feature
        // intersecting with the region should be selected.
        loc.SetInt().SetId(*id);
        loc.SetInt().SetFrom(0);
        loc.SetInt().SetTo(9);
        count = 0;
        // Iterate features. No feature type restrictions.
        for (CFeat_CI it(scope, loc, base_sel); it;  ++it) {
            count++;
        }
        NcbiCout << "Feat count (int. 0..9, any):   " << count << NcbiEndl;

        // Search features only in the TSE containing the target bioseq.
        // Since only one seq-id may be used as the target bioseq, the
        // iterator is constructed not from a seq-loc, but from a bioseq handle
        // and start/stop points on the bioseq. If both start and stop are 0 the
        // whole bioseq is used. The last parameter may be used for type filtering.
        count = 0;
        for ( CFeat_CI it(handle, 0, 999, base_sel); it;  ++it ) {
            count++;
            if ( print_features ) {
                NcbiCout << MSerial_AsnText <<
                    it->GetMappedFeature() << it->GetLocation();
            }
        }
        NcbiCout << "Feat count (bh, 0..999, any):  " << count << NcbiEndl;

        // The same way may be used to iterate aligns and graphs,
        // except that there is no type filter for both of them.
        // No region restrictions -- the whole bioseq is used:
        loc.SetWhole(*id);
        count = 0;
        for (CGraph_CI it(scope, loc,
                          SAnnotSelector()
                          .SetResolveMethod(resolve)
                          .SetSortOrder(order)); it;  ++it) {
            count++;
            // Get seq-annot containing the feature
            if ( print_features ) {
                NcbiCout << MSerial_AsnText <<
                    it->GetMappedGraph() << it->GetLoc();
            }
            CConstRef<CSeq_annot> annot(&it.GetSeq_annot());
        }
        NcbiCout << "Graph count (whole, any):       " << count << NcbiEndl;

        count = 0;
        // Create CAlign_CI using the current scope and location.
        for (CAlign_CI it(scope, loc); it;  ++it) {
            count++;
            if ( print_alignments ) {
                NcbiCout << MSerial_AsnText << *it;
            }
        }
        NcbiCout << "Align count (whole, any):       " << count << NcbiEndl;
    }

    NcbiCout << "Done" << NcbiEndl;
    return 0;
}


END_NCBI_SCOPE



/////////////////////////////////////////////////////////////////////////////
//  MAIN


USING_NCBI_SCOPE;

int main(int argc, const char* argv[])
{
    return CDemoApp().AppMain(argc, argv);
}

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.56  2004/01/30 19:18:49  vasilche
* Added -seq_map flag.
*
* Revision 1.55  2004/01/29 20:38:47  vasilche
* Removed loading of whole sequence by CSeqMap_CI.
*
* Revision 1.54  2004/01/26 18:06:35  vasilche
* Add option for printing Seq-align.
* Use MSerial_Asn* manipulators.
* Removed unused includes.
*
* Revision 1.53  2004/01/07 17:38:02  vasilche
* Fixed include path to genbank loader.
*
* Revision 1.52  2004/01/05 18:14:03  vasilche
* Fixed name of project and path to header.
*
* Revision 1.51  2003/12/30 20:00:41  vasilche
* Added test for CGBDataLoader::GetSatSatKey().
*
* Revision 1.50  2003/12/30 19:51:54  vasilche
* Test CGBDataLoader::GetSatSatkey() method.
*
* Revision 1.49  2003/12/30 16:01:41  vasilche
* Added possibility to specify type of cache to use: -cache_mode (old|newid|new).
*
* Revision 1.48  2003/12/19 19:50:22  vasilche
* Removed obsolete split code.
* Do not use intemediate gi.
*
* Revision 1.47  2003/12/02 23:20:22  vasilche
* Allow to specify any Seq-id via ASN.1 text.
*
* Revision 1.46  2003/11/26 17:56:01  vasilche
* Implemented ID2 split in ID1 cache.
* Fixed loading of splitted annotations.
*
* Revision 1.45  2003/10/27 15:06:31  vasilche
* Added option to set ID1 cache keep time.
*
* Revision 1.44  2003/10/21 14:27:35  vasilche
* Added caching of gi -> sat,satkey,version resolution.
* SNP blobs are stored in cache in preprocessed format (platform dependent).
* Limit number of connections to GenBank servers.
* Added collection of ID1 loader statistics.
*
* Revision 1.43  2003/10/14 18:29:05  vasilche
* Added -exclude_named option.
*
* Revision 1.42  2003/10/09 20:20:58  vasilche
* Added possibility to include and exclude Seq-annot names to annot iterator.
* Fixed adaptive search. It looked only on selected set of annot names before.
*
* Revision 1.41  2003/10/08 17:55:32  vasilche
* Print sequence gi when -id option is used.
*
* Revision 1.40  2003/10/07 13:43:23  vasilche
* Added proper handling of named Seq-annots.
* Added feature search from named Seq-annots.
* Added configurable adaptive annotation search (default: gene, cds, mrna).
* Fixed selection of blobs for loading from GenBank.
* Added debug checks to CSeq_id_Mapper for easier finding lost CSeq_id_Handles.
* Fixed leaked split chunks annotation stubs.
* Moved some classes definitions in separate *.cpp files.
*
* Revision 1.39  2003/09/30 20:13:38  vasilche
* Print original feature location if requested.
*
* Revision 1.38  2003/09/30 16:22:05  vasilche
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
* Revision 1.37  2003/08/27 14:22:01  vasilche
* Added options get_mapped_location, get_mapped_feature and get_original_feature
* to test feature iterator speed.
*
* Revision 1.36  2003/08/15 19:19:16  vasilche
* Fixed memory leak in string packing hooks.
* Fixed processing of 'partial' flag of features.
* Allow table packing of non-point SNP.
* Allow table packing of SNP with long alleles.
*
* Revision 1.35  2003/08/14 20:05:20  vasilche
* Simple SNP features are stored as table internally.
* They are recreated when needed using CFeat_CI.
*
* Revision 1.34  2003/07/25 21:41:32  grichenk
* Implemented non-recursive mode for CSeq_annot_CI,
* fixed friend declaration in CSeq_entry_Info.
*
* Revision 1.33  2003/07/25 15:25:27  grichenk
* Added CSeq_annot_CI class
*
* Revision 1.32  2003/07/24 20:36:17  vasilche
* Added arguemnt to choose ID1<->PUBSEQOS on Windows easier.
*
* Revision 1.31  2003/07/17 20:06:18  vasilche
* Added OBJMGR_LIBS definition.
*
* Revision 1.30  2003/07/17 19:10:30  grichenk
* Added methods for seq-map and seq-vector validation,
* updated demo.
*
* Revision 1.29  2003/07/08 15:09:44  vasilche
* Added argument to test depth of segment resolution.
*
* Revision 1.28  2003/06/25 20:56:32  grichenk
* Added max number of annotations to annot-selector, updated demo.
*
* Revision 1.27  2003/06/02 16:06:38  dicuccio
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
* Revision 1.26  2003/05/27 20:54:52  grichenk
* Fixed CRef<> to bool convertion
*
* Revision 1.25  2003/05/06 16:52:54  vasilche
* Added 'pause' argument.
*
* Revision 1.24  2003/04/29 19:51:14  vasilche
* Fixed interaction of Data Loader garbage collector and TSE locking mechanism.
* Made some typedefs more consistent.
*
* Revision 1.23  2003/04/24 16:12:38  vasilche
* Object manager internal structures are splitted more straightforward.
* Removed excessive header dependencies.
*
* Revision 1.22  2003/04/15 14:24:08  vasilche
* Changed CReader interface to not to use fake streams.
*
* Revision 1.21  2003/04/03 14:17:43  vasilche
* Allow using data loaded from file.
*
* Revision 1.20  2003/03/27 19:40:11  vasilche
* Implemented sorting in CGraph_CI.
* Added Rewind() method to feature/graph/align iterators.
*
* Revision 1.19  2003/03/26 17:27:04  vasilche
* Added optinal reverse feature traversal.
*
* Revision 1.18  2003/03/21 14:51:41  vasilche
* Added debug printing of features collected.
*
* Revision 1.17  2003/03/18 21:48:31  grichenk
* Removed obsolete class CAnnot_CI
*
* Revision 1.16  2003/03/10 16:33:44  vasilche
* Added dump of features if errors were detected.
*
* Revision 1.15  2003/02/27 20:57:36  vasilche
* Addef some options for better performance testing.
*
* Revision 1.14  2003/02/24 19:02:01  vasilche
* Reverted testing shortcut.
*
* Revision 1.13  2003/02/24 18:57:22  vasilche
* Make feature gathering in one linear pass using CSeqMap iterator.
* Do not use feture index by sub locations.
* Sort features at the end of gathering in one vector.
* Extracted some internal structures and classes in separate header.
* Delay creation of mapped features.
*
* Revision 1.12  2002/12/06 15:36:01  grichenk
* Added overlap type for annot-iterators
*
* Revision 1.11  2002/12/05 19:28:33  grichenk
* Prohibited postfix operator ++()
*
* Revision 1.10  2002/11/08 19:43:36  grichenk
* CConstRef<> constructor made explicit
*
* Revision 1.9  2002/11/04 21:29:13  grichenk
* Fixed usage of const CRef<> and CRef<> constructor
*
* Revision 1.8  2002/10/02 17:58:41  grichenk
* Added CBioseq_CI sample code
*
* Revision 1.7  2002/09/03 21:27:03  grichenk
* Replaced bool arguments in CSeqVector constructor and getters
* with enums.
*
* Revision 1.6  2002/06/12 14:39:03  grichenk
* Renamed enumerators
*
* Revision 1.5  2002/05/06 03:28:49  vakatov
* OM/OM1 renaming
*
* Revision 1.4  2002/05/03 21:28:11  ucko
* Introduce T(Signed)SeqPos.
*
* Revision 1.3  2002/05/03 18:37:34  grichenk
* Added more examples of using CFeat_CI and GetSequenceView()
*
* Revision 1.2  2002/03/28 14:32:58  grichenk
* Minor fixes
*
* Revision 1.1  2002/03/28 14:07:25  grichenk
* Initial revision
*
*
* ===========================================================================
*/

