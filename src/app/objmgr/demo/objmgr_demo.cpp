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
* Author:  Aleksey Grichenko, Eugene Vasilchenko
*
* File Description:
*   Examples of using the C++ object manager
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbi_system.hpp>
#include <connect/ncbi_core_cxx.hpp>
#include <util/random_gen.hpp>

// Objects includes
#include <objects/seq/seq__.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seqset/seqset__.hpp>
#include <objects/seqfeat/seqfeat__.hpp>
#include <objects/seqalign/Seq_align.hpp>
#include <objects/general/Name_std.hpp>
#include <objects/general/Dbtag.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/general/User_field.hpp>
#include <objects/general/User_object.hpp>

// Object manager includes
#include <objmgr/scope.hpp>
#include <objmgr/seq_vector.hpp>
#include <objmgr/seqdesc_ci.hpp>
#include <objmgr/feat_ci.hpp>
#include <objmgr/annot_ci.hpp>
#include <objmgr/seq_loc_mapper.hpp>
#include <objmgr/graph_ci.hpp>
#include <objmgr/align_ci.hpp>
#include <objmgr/bioseq_ci.hpp>
#include <objmgr/seq_annot_ci.hpp>
#include <objmgr/util/seq_loc_util.hpp>
#include <objmgr/util/feature.hpp>
#include <objmgr/util/sequence.hpp>
#include <objmgr/impl/synonyms.hpp>
#include <objmgr/object_manager.hpp>
#include <objmgr/prefetch_manager.hpp>
#include <objmgr/prefetch_actions.hpp>
#include <objmgr/table_field.hpp>

#include <objtools/data_loaders/genbank/gbloader.hpp>
#include <objtools/data_loaders/genbank/readers.hpp>
#include <dbapi/driver/drivers.hpp>

#include <objtools/data_loaders/blastdb/bdbloader.hpp>

#ifdef HAVE_BDB
#  define HAVE_LDS 1
#elif defined(HAVE_LDS)
#  undef HAVE_LDS
#endif

#ifdef HAVE_LDS
#  include <objtools/data_loaders/lds/lds_dataloader.hpp>
#  include <objtools/lds/lds_manager.hpp>
#endif

#include <serial/iterator.hpp>

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
};


void CDemoApp::Init(void)
{
    CONNECT_Init(&GetConfig());

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
                             "file with Seq-entry to load (text ASN.1)",
                             CArgDescriptions::eInputFile);
    arg_desc->AddOptionalKey("bfile", "SeqEntryFile",
                             "file with Seq-entry to load (binary ASN.1)",
                             CArgDescriptions::eInputFile,
                             CArgDescriptions::fBinary);
    arg_desc->AddOptionalKey("annot_file", "SeqAnnotFile",
                             "file with Seq-annot to load (text ASN.1)",
                             CArgDescriptions::eInputFile);
    arg_desc->AddOptionalKey("annot_bfile", "SeqAnnotFile",
                             "file with Seq-annot to load (binary ASN.1)",
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
    arg_desc->AddDefaultKey("missing", "UnresolvableIdMethod",
                            "Method of treating unresolvable ids",
                            CArgDescriptions::eString, "ignore");
    arg_desc->SetConstraint("missing",
                            &(*new CArgAllow_Strings,
                            "ignore", "search", "fail"));
    arg_desc->AddFlag("limit_tse", "Limit annotations from sequence TSE only");
    arg_desc->AddFlag("externals", "Search for external features only");

    arg_desc->AddOptionalKey("loader", "Loader",
                             "Use specified GenBank loader readers (\"-\" means no GenBank",
                             CArgDescriptions::eString);
#ifdef HAVE_LDS
    arg_desc->AddOptionalKey("lds_dir", "LDSDir",
                             "Use local data storage loader from the specified firectory",
                             CArgDescriptions::eString);
#endif
    arg_desc->AddOptionalKey("blast", "Blast",
                             "Use BLAST data loader from the specified DB",
                             CArgDescriptions::eString);
    arg_desc->AddOptionalKey("blast_type", "BlastType",
                             "Use BLAST data loader type (default: eUnknown)",
                             CArgDescriptions::eString);
    arg_desc->SetConstraint("blast_type",
                            &(*new CArgAllow_Strings,
                              "protein", "p", "nucleotide", "n"));
    arg_desc->AddFlag("sra", "Use SRA data loader as a plugin");

    arg_desc->AddFlag("get_ids", "Get sequence ids");
    arg_desc->AddFlag("get_gi", "Get sequence gi");
    arg_desc->AddFlag("get_acc", "Get sequence accession");

    arg_desc->AddFlag("seq_map", "scan SeqMap on full depth");
    arg_desc->AddFlag("seg_labels", "get labels of all segments in Delta");
    arg_desc->AddFlag("whole_sequence", "load whole sequence");
    arg_desc->AddFlag("scan_whole_sequence", "scan whole sequence");
    arg_desc->AddFlag("whole_tse", "perform some checks on whole TSE");
    arg_desc->AddFlag("print_tse", "print TSE with sequence");
    arg_desc->AddFlag("print_seq", "print sequence");
    arg_desc->AddOptionalKey("desc_type", "DescType",
                             "look only descriptors of specified type",
                             CArgDescriptions::eString);
    arg_desc->AddFlag("print_descr", "print all found descriptors");
    arg_desc->AddFlag("print_cds", "print CDS");
    arg_desc->AddFlag("print_features", "print all found features");
    arg_desc->AddFlag("print_mapper",
                      "print retult of CSeq_loc_Mapper "
                      "(when -print_features is set)");
    arg_desc->AddFlag("only_features", "do only one scan of features");
    arg_desc->AddFlag("by_product", "Search features by their product");
    arg_desc->AddFlag("count_types",
                      "print counts of different feature types");
    arg_desc->AddFlag("count_subtypes",
                      "print counts of different feature subtypes");
    arg_desc->AddFlag("get_types",
                      "print only types of features found");
    arg_desc->AddFlag("get_names",
                      "print only Seq-annot names of features found");
    arg_desc->AddOptionalKey("range_from", "RangeFrom",
                             "features starting at this point on the sequence",
                             CArgDescriptions::eInteger);
    arg_desc->AddOptionalKey("range_to", "RangeTo",
                             "features ending at this point on the sequence",
                             CArgDescriptions::eInteger);
    arg_desc->AddFlag("minus_strand",
                      "use minus strand of the sequence");
    arg_desc->AddOptionalKey("range_loc", "RangeLoc",
                             "features on this Seq-loc in ASN.1 text format",
                             CArgDescriptions::eString);
    arg_desc->AddDefaultKey("overlap", "Overlap",
                            "Method of overlap location check",
                            CArgDescriptions::eString, "totalrange");
    arg_desc->SetConstraint("overlap",
                            &(*new CArgAllow_Strings,
                              "totalrange", "intervals"));
                             
    arg_desc->AddFlag("get_mapped_location", "get mapped location");
    arg_desc->AddFlag("get_original_feature", "get original location");
    arg_desc->AddFlag("get_mapped_feature", "get mapped feature");
    arg_desc->AddFlag("get_feat_handle", "reverse lookup of feature handle");
    arg_desc->AddFlag("check_cds", "check correctness cds");
    arg_desc->AddFlag("check_seq_data", "check availability of seq_data");
    arg_desc->AddFlag("seq_vector_tse", "use TSE as a base for CSeqVector");
    arg_desc->AddFlag("skip_alignments", "do not search for alignments");
    arg_desc->AddFlag("print_alignments", "print all found Seq-aligns");
    arg_desc->AddFlag("get_mapped_alignments", "get mapped alignments");
    arg_desc->AddFlag("reverse", "reverse order of features");
    arg_desc->AddFlag("labels", "compare features by labels too");
    arg_desc->AddFlag("no_sort", "do not sort features");
    arg_desc->AddDefaultKey("max_feat", "MaxFeat",
                            "Max number of features to iterate",
                            CArgDescriptions::eInteger, "0");
    arg_desc->AddDefaultKey("depth", "depth",
                            "Max depth of segments to iterate",
                            CArgDescriptions::eInteger, "100");
    arg_desc->AddFlag("adaptive", "Use adaptive depth of segments");
    arg_desc->AddFlag("no-feat-policy", "Ignore feature fetch policy");
    arg_desc->AddFlag("exact_depth", "Use exact depth of segments");
    arg_desc->AddFlag("unnamed",
                      "include features from unnamed Seq-annots");
    arg_desc->AddOptionalKey("named", "NamedAnnots",
                             "include features from named Seq-annots "
                             "(comma separated list)",
                             CArgDescriptions::eString);
    arg_desc->AddOptionalKey("named_acc", "NamedAnnotAccession",
                             "include features with named annot accession "
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
    arg_desc->AddFlag("noexternal",
                      "include external annotations");
    arg_desc->AddOptionalKey("feat_type", "FeatType",
                             "Type of features to select",
                             CArgDescriptions::eString);
    arg_desc->AddDefaultKey("feat_subtype", "FeatSubType",
                            "Subtype of features to select",
                            CArgDescriptions::eInteger,
                            NStr::IntToString(CSeqFeatData::eSubtype_any));
    arg_desc->AddOptionalKey("feat_id", "FeatId",
                             "Feat-id of features to search",
                             CArgDescriptions::eInteger);
    arg_desc->AddFlag("make_tree", "make feature tree");
    arg_desc->AddFlag("print_tree", "print feature tree");
    arg_desc->AddFlag("used_memory_check", "exit(0) after loading sequence");
    arg_desc->AddFlag("reset_scope", "reset scope before exiting");
    arg_desc->AddFlag("modify", "try to modify Bioseq object");
    arg_desc->AddOptionalKey("table_field_name", "table_field_name",
                             "Table Seq-feat field name to retrieve",
                             CArgDescriptions::eString);
    arg_desc->AddOptionalKey("table_field_id", "table_field_id",
                             "Table Seq-feat field id to retrieve",
                             CArgDescriptions::eInteger);

    // Program description
    string prog_description = "Example of the C++ object manager usage\n";
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              prog_description, false);

    // Pass argument descriptions to the application
    //

    SetupArgDescriptions(arg_desc.release());
}


extern CAtomicCounter newCObjects;


template<class C>
typename C::E_Choice GetVariant(const CArgValue& value)
{
    typedef typename C::E_Choice E_Choice;
    if ( !value ) {
        return C::e_not_set;
    }
    for ( int e = C::e_not_set; e < C::e_MaxChoice; ++e ) {
        if ( C::SelectionName(E_Choice(e)) == value.AsString() ) {
            return E_Choice(e);
        }
    }
    return E_Choice(NStr::StringToInt(value.AsString()));
}


CNcbiOstream& operator<<(CNcbiOstream& out, const vector<char>& v)
{
    out << '\'';
    ITERATE ( vector<char>, i, v ) {
        int c = *i & 255;
        for ( int j = 0; j < 2; ++j ) {
            out << "0123456789ABCDEF"[(c>>4)&15];
            c <<= 4;
        }
    }
    out << "\'H";
    return out;
}


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
        NcbiCout << MSerial_AsnText << *id;
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

    SAnnotSelector::EResolveMethod resolve = SAnnotSelector::eResolve_TSE;
    if ( args["resolve"].AsString() == "all" )
        resolve = SAnnotSelector::eResolve_All;
    if ( args["resolve"].AsString() == "none" )
        resolve = SAnnotSelector::eResolve_None;
    if ( args["resolve"].AsString() == "tse" )
        resolve = SAnnotSelector::eResolve_TSE;
    SAnnotSelector::EUnresolvedFlag missing = SAnnotSelector::eIgnoreUnresolved;
    if ( args["missing"].AsString() == "ignore" )
        missing = SAnnotSelector::eIgnoreUnresolved;
    if ( args["missing"].AsString() == "search" )
        missing = SAnnotSelector::eSearchUnresolved;
    if ( args["missing"].AsString() == "fail" )
        missing = SAnnotSelector::eFailUnresolved;
    bool externals_only = args["externals"];
    bool limit_tse = args["limit_tse"];

    int repeat_count = args["count"].AsInteger();
    int pause = args["pause"].AsInteger();
    bool only_features = args["only_features"];
    bool by_product = args["by_product"];
    bool count_types = args["count_types"];
    bool count_subtypes = args["count_subtypes"];
    bool get_types = args["get_types"];
    bool get_names = args["get_names"];
    if ( get_types || get_names ) {
        only_features = true;
    }
    if ( count_types || count_subtypes ) {
        only_features = true;
    }
    bool print_tse = args["print_tse"];
    bool print_seq = args["print_seq"];
    bool print_descr = args["print_descr"];
    CSeqdesc::E_Choice desc_type =
        GetVariant<CSeqdesc>(args["desc_type"]);
    bool print_cds = args["print_cds"];
    bool print_features = args["print_features"];
    bool print_mapper = args["print_mapper"];
    bool get_mapped_location = args["get_mapped_location"];
    bool get_original_feature = args["get_original_feature"];
    bool get_mapped_feature = args["get_mapped_feature"];
    bool get_feat_handle = args["get_feat_handle"];
    bool print_alignments = args["print_alignments"];
    bool check_cds = args["check_cds"];
    bool check_seq_data = args["check_seq_data"];
    bool seq_vector_tse = args["seq_vector_tse"];
    bool skip_alignments = args["skip_alignments"];
    bool get_mapped_alignments = args["get_mapped_alignments"];
    SAnnotSelector::ESortOrder order =
        args["reverse"] ?
        SAnnotSelector::eSortOrder_Reverse : SAnnotSelector::eSortOrder_Normal;
    if ( args["no_sort"] )
        order = SAnnotSelector::eSortOrder_None;
    bool labels = args["labels"];
    int max_feat = args["max_feat"].AsInteger();
    int depth = args["depth"].AsInteger();
    bool adaptive = args["adaptive"];
    bool no_feat_policy = args["no-feat-policy"];
    bool exact_depth = args["exact_depth"];
    CSeqFeatData::E_Choice feat_type =
        GetVariant<CSeqFeatData>(args["feat_type"]);
    CSeqFeatData::ESubtype feat_subtype =
        CSeqFeatData::ESubtype(args["feat_subtype"].AsInteger());
    bool nosnp = args["nosnp"];
    bool include_unnamed = args["unnamed"];
    bool include_allnamed = args["allnamed"];
    bool noexternal = args["noexternal"];
    bool whole_tse = args["whole_tse"];
    bool whole_sequence = args["whole_sequence"];
    bool scan_whole_sequence = args["scan_whole_sequence"];
    bool used_memory_check = args["used_memory_check"];
    bool get_synonyms = false;
    bool get_ids = args["get_ids"];
    bool get_blob_id = true;
    bool make_tree = args["make_tree"];
    bool print_tree = args["print_tree"];
    list<string> include_named;
    if ( args["named"] ) {
        NStr::Split(args["named"].AsString(), ",", include_named);
    }
    list<string> exclude_named;
    if ( args["exclude_named"] ) {
        NStr::Split(args["exclude_named"].AsString(), ",", exclude_named);
    }
    list<string> include_named_accs;
    if ( args["named_acc"] ) {
        NStr::Split(args["named_acc"].AsString(), ",", include_named_accs);
    }
    bool scan_seq_map = args["seq_map"];
    bool get_seg_labels = args["seg_labels"];

    vector<int> types_counts, subtypes_counts;

    // Create object manager. Use CRef<> to delete the OM on exit.
    CRef<CObjectManager> pOm = CObjectManager::GetInstance();

    CRef<CGBDataLoader> gb_loader;
    vector<string> other_loaders;
    if ( args["loader"] ) {
        string genbank_readers = args["loader"].AsString();
        if ( genbank_readers != "-" ) {
            // Create genbank data loader and register it with the OM.
            // The last argument "eDefault" informs the OM that the loader
            // must be included in scopes during the CScope::AddDefaults() call
#ifdef HAVE_PUBSEQ_OS
            DBAPI_RegisterDriver_FTDS();
            GenBankReaders_Register_Pubseq();
#endif
            gb_loader = CGBDataLoader::RegisterInObjectManager
                (*pOm, genbank_readers).GetLoader();
        }
    }
    else {
        gb_loader = CGBDataLoader::RegisterInObjectManager(*pOm).GetLoader();
    }
#ifdef HAVE_LDS
    if ( args["lds_dir"] ) {
        string lds_dir = args["lds_dir"].AsString();
        CLDS_Manager::ERecurse recurse = CLDS_Manager::eRecurseSubDirs;
        CLDS_Manager::EComputeControlSum control_sum =
            CLDS_Manager::eComputeControlSum;
        auto_ptr<CLDS_Database> lds_db;
        CLDS_Manager manager(lds_dir);

        try {
            lds_db.reset(manager.ReleaseDB());
        } catch(CBDB_ErrnoException&) {
            manager.Index(recurse, control_sum);
            lds_db.reset(manager.ReleaseDB());
        }

        other_loaders.push_back(CLDS_DataLoader::RegisterInObjectManager(*pOm, *lds_db).GetLoader()->GetName());
        lds_db.release();
    }
#endif
    if ( args["blast"] || args["blast_type"] ) {
        string db;
        if ( args["blast"] ) {
            db = args["blast"].AsString();
        }
        else {
            db = "nr";
        }
        CBlastDbDataLoader::EDbType type = CBlastDbDataLoader::eUnknown;
        if ( args["blast_type"] ) {
            string s = args["blast_type"].AsString();
            if ( s.size() > 0 && s[0] == 'p' ) {
                type = CBlastDbDataLoader::eProtein;
            }
            else if ( s.size() > 0 && s[0] == 'n' ) {
                type = CBlastDbDataLoader::eNucleotide;
            }
        }
        other_loaders.push_back(CBlastDbDataLoader::RegisterInObjectManager(*pOm, db, type).GetLoader()->GetName());
    }
    if ( args["sra"] ) {
        other_loaders.push_back(pOm->RegisterDataLoader(0, "sra")->GetName());
    }

    // Create a new scope.
    CScope scope(*pOm);
    // Add default loaders (GB loader in this demo) to the scope.
    scope.AddDefaults();
    ITERATE ( vector<string>, it, other_loaders ) {
        scope.AddDataLoader(*it, 88);
    }

    CSeq_entry_Handle added_entry;
    CSeq_annot_Handle added_annot;
    if ( args["file"] ) {
        CRef<CSeq_entry> entry(new CSeq_entry);
        args["file"].AsInputFile() >> MSerial_AsnText >> *entry;
        if ( used_memory_check ) {
            exit(0);
        }
        added_entry = scope.AddTopLevelSeqEntry(const_cast<CSeq_entry&>(*entry));
    }
    if ( args["bfile"] ) {
        CRef<CSeq_entry> entry(new CSeq_entry);
        args["bfile"].AsInputFile() >> MSerial_AsnBinary >> *entry;
        added_entry = scope.AddTopLevelSeqEntry(*entry);
    }
    if ( args["annot_file"] ) {
        CRef<CSeq_annot> annot(new CSeq_annot);
        args["annot_file"].AsInputFile() >> MSerial_AsnText >> *annot;
        added_annot = scope.AddSeq_annot(*annot);
    }
    if ( args["annot_bfile"] ) {
        CRef<CSeq_annot> annot(new CSeq_annot);
        args["annot_bfile"].AsInputFile() >> MSerial_AsnBinary >> *annot;
        added_annot = scope.AddSeq_annot(*annot);
    }

    CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(*id);
    if ( get_ids ) {
        if ( args["get_gi"] ) {
            NcbiCout << "Gi: "
                     << sequence::GetId(idh, scope, sequence::eGetId_ForceGi)
                     << NcbiEndl;
        }
        if ( args["get_acc"] ) {
            if ( args["gi"] ) {
                int gi = args["gi"].AsInteger();
                NcbiCout << "Acc: "
                         << sequence::GetAccessionForGi(gi, scope, sequence::eWithoutAccessionVersion)
                         << NcbiEndl;
            }
        }
        NcbiCout << "Best id: "
                 << sequence::GetId(idh, scope, sequence::eGetId_Best)
                 << NcbiEndl;
        NcbiCout << "Ids:" << NcbiEndl;
        //scope.GetBioseqHandle(idh);
        vector<CSeq_id_Handle> ids = scope.GetIds(idh);
        ITERATE ( vector<CSeq_id_Handle>, it, ids ) {
            NcbiCout << "    " << it->AsString() << NcbiEndl;
        }
    }
    if ( get_blob_id && gb_loader && other_loaders.empty() ) {
        try {
            CDataLoader::TBlobId blob_id = gb_loader->GetBlobId(idh);
            if ( !blob_id ) {
                ERR_POST("Cannot find blob id of "<<id->AsFastaString());
            }
            else {
                NcbiCout << "Resolved: "<<id->AsFastaString()<<
                    " -> "<<blob_id.ToString()<<NcbiEndl;
            }
        }
        catch ( CException& exc ) {
            ERR_POST("Cannot blob id of "<<id->AsFastaString()<<": "<<
                     exc.GetMsg());
        }
    }

    // Get bioseq handle for the seq-id. Most of requests will use this handle.
    CBioseq_Handle handle = scope.GetBioseqHandle(idh);
    if ( handle.GetState() ) {
        // print blob state:
        NcbiCout << "Bioseq state: 0x" << hex << handle.GetState() << dec
                 << NcbiEndl;
    }
    // Check if the handle is valid
    if ( !handle ) {
        ERR_POST(Error << "Bioseq not found.");
    }
    if ( handle && get_synonyms ) {
        NcbiCout << "Synonyms:" << NcbiEndl;
        CConstRef<CSynonymsSet> syns = scope.GetSynonyms(handle);
        ITERATE ( CSynonymsSet, it, *syns ) {
            CSeq_id_Handle idh = CSynonymsSet::GetSeq_id_Handle(it);
            NcbiCout << "    " << idh.AsString() << NcbiEndl;
        }
    }

    if ( handle && print_tse ) {
        CConstRef<CSeq_entry> entry =
            handle.GetTopLevelEntry().GetCompleteSeq_entry();
        NcbiCout << "-------------------- TSE --------------------\n";
        NcbiCout << MSerial_AsnText << *entry << '\n';
        NcbiCout << "-------------------- END --------------------\n";
    }
    if ( handle && print_seq ) {
        NcbiCout << "-------------------- SEQ --------------------\n";
        NcbiCout << MSerial_AsnText << *handle.GetCompleteObject() << '\n';
        NcbiCout << "-------------------- END --------------------\n";
    }

    CRef<CSeq_id> search_id = id;
    CRef<CSeq_loc> whole_loc(new CSeq_loc);
    // No region restrictions -- the whole bioseq is used:
    whole_loc->SetWhole(*search_id);
    CRef<CSeq_loc> range_loc;
    bool minus_strand = args["minus_strand"];
    TSeqPos range_from, range_to;
    if ( minus_strand || args["range_from"] || args["range_to"] ) {
        if ( args["range_from"] ) {
            range_from = args["range_from"].AsInteger();
        }
        else {
            range_from = 0;
        }
        if ( args["range_to"] ) {
            range_to = args["range_to"].AsInteger();
        }
        else {
            range_to = handle? handle.GetBioseqLength()-1: kInvalidSeqPos;
        }
        range_loc.Reset(new CSeq_loc);
        range_loc->SetInt().SetId(*search_id);
        range_loc->SetInt().SetFrom(range_from);
        range_loc->SetInt().SetTo(range_to);
        if ( minus_strand ) {
            range_loc->SetInt().SetStrand(eNa_strand_minus);
        }
    }
    else {
        range_from = range_to = 0;
        range_loc = whole_loc;
    }
    if ( args["range_loc"] ) {
        CNcbiIstrstream in(args["range_loc"].AsString().c_str());
        in >> MSerial_AsnText >> *range_loc;
    }
    SAnnotSelector::EOverlapType overlap = SAnnotSelector::eOverlap_TotalRange;
    if ( args["overlap"].AsString() == "totalrange" )
        overlap = SAnnotSelector::eOverlap_TotalRange;
    if ( args["overlap"].AsString() == "intervals" )
        overlap = SAnnotSelector::eOverlap_Intervals;

    string table_field_name;
    if ( args["table_field_name"] )
        table_field_name = args["table_field_name"].AsString();
    int table_field_id = -1;
    if ( args["table_field_id"] )
        table_field_id = args["table_field_id"].AsInteger();
    bool modify = args["modify"];

    handle.Reset();
    for ( int c = 0; c < repeat_count; ++c ) {
        if ( c && pause ) {
            SleepSec(pause);
        }
        
        // get handle again, check for scope TSE locking
        handle = scope.GetBioseqHandle(idh);
        if ( !handle ) {
            ERR_POST(Error << "Cannot resolve "<<idh.AsString());
            //continue;
        }

        if ( handle && get_seg_labels ) {
            TSeqPos range_length =
                range_to == 0? kInvalidSeqPos: range_to - range_from + 1;
            CSeqMap::TFlags flags = CSeqMap::fDefaultFlags;
            if ( exact_depth ) {
                flags |= CSeqMap::fFindExactLevel;
            }
            const CSeqMap& seq_map = handle.GetSeqMap();
            CSeqMap::const_iterator seg =
                seq_map.ResolvedRangeIterator(&scope,
                                              range_from,
                                              range_length,
                                              eNa_strand_plus,
                                              1,
                                              flags);
            for ( ; seg; ++seg ) {
                if ( seg.GetType() == CSeqMap::eSeqRef ) {
                    string label = scope.GetLabel(seg.GetRefSeqid());
                    NcbiCout << "Label(" << seg.GetRefSeqid().AsString()
                             << ") = " << label << NcbiEndl;
                }
            }
        }

        string sout;
        int count;
        if ( handle && !only_features ) {
            // List other sequences in the same TSE
            if ( whole_tse ) {
                NcbiCout << "TSE sequences:" << NcbiEndl;
                for ( CBioseq_CI bit(handle.GetTopLevelEntry()); bit; ++bit) {
                    NcbiCout << "    "<<bit->GetSeqId()->DumpAsFasta()<<
                        NcbiEndl;
                }
            }

            // Get the bioseq
            CConstRef<CBioseq> bioseq(handle.GetBioseqCore());
            // -- use the bioseq: print the first seq-id
            NcbiCout << "First ID = " <<
                (*bioseq->GetId().begin())->DumpAsFasta() << NcbiEndl;

            // Get the sequence using CSeqVector. Use default encoding:
            // CSeq_data::e_Iupacna or CSeq_data::e_Iupacaa.
            CSeqVector seq_vect(*range_loc, scope,
                                CBioseq_Handle::eCoding_Iupac);
            if ( seq_vector_tse ) {
                seq_vect = CSeqVector(*range_loc, handle.GetTSE_Handle(),
                                      CBioseq_Handle::eCoding_Iupac);
            }
            //handle.GetSeqVector(CBioseq_Handle::eCoding_Iupac);
            // -- use the vector: print length and the first 10 symbols
            NcbiCout << "Sequence: length=" << seq_vect.size() << NcbiFlush;
            if ( check_seq_data ) {
                CStopWatch sw(CStopWatch::eStart);
                if ( seq_vect.CanGetRange(0, seq_vect.size()) ) {
                    NcbiCout << " data=";
                    sout.erase();
                    TSeqPos size = min(seq_vect.size(), 10u);
                    for ( TSeqPos i=0; i < size; ++i ) {
                        // Convert sequence symbols to printable form
                        sout += seq_vect[i];
                    }
                    NcbiCout << NStr::PrintableString(sout)
                             << " in " << sw;
                }
                else {
                    NcbiCout << " data unavailable"
                             << " in " << sw;
                }
            }
            else {
                try {
                    seq_vect[seq_vect.size()-1];
                    NcbiCout << " got last byte";
                }
                catch ( CException& exc ) {
                    ERR_POST(" cannot get last byte: Exception: "<<exc.what());
                }
            }
            NcbiCout << NcbiEndl;
            if ( whole_sequence ) {
                CStopWatch sw(CStopWatch::eStart);
                TSeqPos size = seq_vect.size();
                NcbiCout << "Whole seq data["<<size<<"] = " << NcbiFlush;
                seq_vect.GetSeqData(0, size, sout);
                if ( size <= 20u ) {
                    NcbiCout << NStr::PrintableString(sout);
                }
                else {
                    NcbiCout << NStr::PrintableString(sout.substr(0, 10));
                    NcbiCout << "..";
                    NcbiCout << NStr::PrintableString(sout.substr(size-10));
                }
                NcbiCout << " in " << sw << NcbiEndl;
            }
            if ( scan_whole_sequence ) {
                CStopWatch sw(CStopWatch::eStart);
                NcbiCout << "Scanning sequence..." << NcbiFlush;
                size_t pos = 0;
                string buffer;
                for ( CSeqVector_CI it(seq_vect); it; ) {
                    _ASSERT(it.GetPos() == pos);
                    if ( (pos & 255) == 0 ) {
                        size_t cnt = min(size_t(99), seq_vect.size()-pos);
                        it.GetSeqData(buffer, cnt);
                        pos += cnt;
                    }
                    else {
                        ++it;
                        ++pos;
                    }
                    _ASSERT(it.GetPos() == pos);
                }
                _ASSERT(pos == seq_vect.size());
                NcbiCout << "done" << " in " << sw << NcbiEndl;
            }
            // CSeq_descr iterator: iterates all descriptors starting
            // from the bioseq and going the seq-entries tree up to the
            // top-level seq-entry.
            count = 0;
            for (CSeqdesc_CI desc_it(handle, desc_type); desc_it; ++desc_it) {
                if ( print_descr ) {
                    NcbiCout << "\n" << MSerial_AsnText << *desc_it;
                }
                count++;
            }
            cout << "\n";
            NcbiCout << "Seqdesc count (sequence):\t" << count << NcbiEndl;

            count = 0;
            for ( CSeq_annot_CI ai(handle.GetParentEntry()); ai; ++ai) {
                ++count;
            }
            NcbiCout << "Seq_annot count (recursive):\t"
                     << count << NcbiEndl;
            
            count = 0;
            for ( CSeq_annot_CI ai(handle.GetParentEntry(),
                                   CSeq_annot_CI::eSearch_entry);
                  ai; ++ai) {
                ++count;
            }
            NcbiCout << "Seq_annot count (non-recurs):\t"
                     << count << NcbiEndl;

            if ( whole_tse ) {
                count = 0;
                for ( CSeq_annot_CI ai(handle); ai; ++ai) {
                    ++count;
                }
                NcbiCout << "Seq_annot count (up to TSE):\t"
                         << count << NcbiEndl;

                count = 0;
                for (CSeq_annot_CI ai(handle.GetTopLevelEntry()); ai; ++ai) {
                    ++count;
                }
                NcbiCout << "Seq_annot count (TSE, recursive):\t"
                         << count << NcbiEndl;
                
                count = 0;
                for (CSeq_annot_CI ai(handle.GetTopLevelEntry(),
                                      CSeq_annot_CI::eSearch_entry);
                     ai; ++ai) {
                    ++count;
                }
                NcbiCout << "Seq_annot count (TSE, non-recurs):\t"
                         << count << NcbiEndl;
            }
        }

        // CSeq_feat iterator: iterates all features which can be found in the
        // current scope including features from all TSEs.
        count = 0;
        // Create CFeat_CI using the current scope and location.
        // No feature type restrictions.
        SAnnotSelector base_sel;
        base_sel
            .SetResolveMethod(resolve)
            .SetOverlapType(overlap)
            .SetSortOrder(order)
            .SetMaxSize(max_feat)
            .SetResolveDepth(depth)
            .SetAdaptiveDepth(adaptive)
            .SetExactDepth(exact_depth)
            .SetUnresolvedFlag(missing);
        if ( no_feat_policy ) {
            base_sel.SetAdaptiveDepthFlags(base_sel.GetAdaptiveDepthFlags()&
                                           ~SAnnotSelector::fAdaptive_ByPolicy);
        }
        if ( labels ) {
            base_sel.SetFeatComparator(new feature::CFeatComparatorByLabel());
        }
        if ( handle && externals_only ) {
            base_sel.SetSearchExternal(handle);
        }
        if ( limit_tse ) {
            if ( added_annot ) {
                base_sel.SetLimitSeqAnnot(added_annot);
            }
            else if ( added_entry ) {
                base_sel.SetLimitSeqEntry(added_entry);
            }
            else if ( handle ) {
                base_sel.SetLimitTSE(handle.GetTopLevelEntry());
            }
        }
        if ( include_allnamed ) {
            base_sel.SetAllNamedAnnots();
        }
        if ( include_unnamed ) {
            base_sel.AddUnnamedAnnots();
        }
        ITERATE ( list<string>, it, include_named ) {
            base_sel.AddNamedAnnots(*it);
        }
        ITERATE ( list<string>, it, include_named_accs ) {
            base_sel.IncludeNamedAnnotAccession(*it);
        }
        if ( nosnp ) {
            base_sel.ExcludeNamedAnnots("SNP");
        }
        ITERATE ( list<string>, it, exclude_named ) {
            base_sel.ExcludeNamedAnnots(*it);
        }
        if ( noexternal ) {
            base_sel.SetExcludeExternal();
        }
        string sel_msg = "any";
        if ( feat_type != CSeqFeatData::e_not_set ) {
            base_sel.SetFeatType(feat_type);
            sel_msg = "req";
        }
        if ( feat_subtype != CSeqFeatData::eSubtype_any ) {
            base_sel.SetFeatSubtype(feat_subtype);
            sel_msg = "req";
        }
        base_sel.SetByProduct(by_product);

        typedef int TTableField;
        auto_ptr< CTableFieldHandle<TTableField> > table_field;
        if ( table_field_id >= 0 ) {
            table_field.reset(new CTableFieldHandle<TTableField>(CSeqTable_column_info::EField_id(table_field_id)));
        }
        else if ( !table_field_name.empty() ) {
            table_field.reset(new CTableFieldHandle<TTableField>(table_field_name));
        }

        if ( get_types || get_names ) {
            if ( get_types ) {
                CFeat_CI it(scope, *range_loc, base_sel.SetCollectTypes());
                ITERATE ( CFeat_CI::TAnnotTypes, i, it.GetAnnotTypes() ) {
                    SAnnotSelector::TFeatType t = i->GetFeatType();
                    SAnnotSelector::TFeatSubtype st = i->GetFeatSubtype();
                    NcbiCout << "Feat type: "
                             << setw(10) << CSeqFeatData::SelectionName(t)
                             << " (" << setw(2) << t << ") "
                             << " subtype: "
                             << setw(3) << st
                             << NcbiEndl;
                }
            }
            if ( get_names ) {
                {{
                    NcbiCout << "Annot names:" << NcbiEndl;
                    set<string> annot_names =
                        gb_loader->GetNamedAnnotAccessions(idh);
                    ITERATE ( set<string>, i, annot_names ) {
                        NcbiCout << "Named annot: " << *i
                                 << NcbiEndl;
                    }
                }}
                {{
                    NcbiCout << "Feature names:" << NcbiEndl;
                    SAnnotSelector sel = base_sel;
                    sel.SetCollectNames();
                    if ( !sel.IsIncludedAnyNamedAnnotAccession() ) {
                        sel.IncludeNamedAnnotAccession("NA*");
                    }
                    CFeat_CI it(scope, *range_loc, sel);
                    ITERATE ( CFeat_CI::TAnnotNames, i, it.GetAnnotNames() ) {
                        if ( i->IsNamed() ) {
                            NcbiCout << "Named annot: " << i->GetName()
                                     << NcbiEndl;
                        }
                        else {
                            NcbiCout << "Unnamed annot"
                                     << NcbiEndl;
                        }
                    }
                }}
            }
            continue;
        }

        {{
            if ( count_types ) {
                types_counts.assign(CSeqFeatData::e_MaxChoice, 0);
            }
            if ( count_subtypes ) {
                subtypes_counts.assign(CSeqFeatData::eSubtype_max+1, 0);
            }
            CRef<CSeq_loc_Mapper> mapper;
            if ( handle && print_features && print_mapper ) {
                mapper.Reset(new CSeq_loc_Mapper(handle,
                                                 CSeq_loc_Mapper::eSeqMap_Up));
            }
            if ( handle && args["feat_id"] ) {
                int feat_id = args["feat_id"].AsInteger();
                vector<CSeq_feat_Handle> feats;
                CTSE_Handle tse = handle.GetTSE_Handle();
                for ( int t = 0; t < 4; ++t ) {
                    switch ( t ) {
                    case 0:
                        NcbiCout << "Features with id "
                                 << feat_id << " +type:";
                        feats = tse.GetFeaturesWithId(feat_type, feat_id);
                        break;
                    case 1:
                        NcbiCout << "Features with id "
                                 << feat_id << " +subtype:";
                        feats = tse.GetFeaturesWithId(feat_subtype, feat_id);
                        break;
                    case 2:
                        NcbiCout << "Features with xref "
                                 << feat_id << " +type:";
                        feats = tse.GetFeaturesWithXref(feat_type, feat_id);
                        break;
                    case 3:
                        NcbiCout << "Features with xref "
                                 << feat_id << " +subtype:";
                        feats = tse.GetFeaturesWithXref(feat_subtype, feat_id);
                        break;
                    }
                    if ( print_features ) {
                        NcbiCout << "\n";
                        ITERATE ( vector<CSeq_feat_Handle>, it, feats ) {
                            NcbiCout << MSerial_AsnText << *it->GetSeq_feat();
                        }
                    }
                    else {
                        NcbiCout << " " << feats.size() << NcbiEndl;
                    }
                }
            }
            for ( CFeat_CI it(scope, *range_loc, base_sel); it;  ++it) {
                if ( count_types ) {
                    ++types_counts[it->GetFeatType()];
                }
                if ( count_subtypes ) {
                    ++subtypes_counts[it->GetFeatSubtype()];
                }
                ++count;
                if ( get_mapped_location )
                    it->GetLocation();
                if ( get_original_feature )
                    it->GetOriginalFeature();
                if ( get_mapped_feature ) {
                    if ( it->IsSetId() )
                        NcbiCout << MSerial_AsnText << it->GetId();
                    NcbiCout << MSerial_AsnText << it->GetData();
                    if ( it->IsSetPartial() )
                        NcbiCout << "Partial: " << it->GetPartial() << '\n';
                    if ( it->IsSetExcept() )
                        NcbiCout << "Except: " << it->GetExcept() << '\n';
                    if ( it->IsSetComment() )
                        NcbiCout << "Commend: " << it->GetComment() << '\n';
                    if ( it->IsSetProduct() )
                        NcbiCout << MSerial_AsnText << it->GetProduct();
                    NcbiCout << MSerial_AsnText << it->GetLocation();
                    if ( it->IsSetQual() )
                        ITERATE ( CSeq_feat::TQual, it2, it->GetQual() )
                            NcbiCout << MSerial_AsnText << **it2;
                    if ( it->IsSetTitle() )
                        NcbiCout << "Title: " << it->GetTitle() << '\n';
                    if ( it->IsSetExt() )
                        NcbiCout << MSerial_AsnText << it->GetExt();
                    //if ( it->IsSetCit() ) NcbiCout << MSerial_AsnText << it->GetCit();
                    if ( it->IsSetExp_ev() )
                        NcbiCout << "Exp-ev: " << it->GetExp_ev() << '\n';
                    if ( it->IsSetXref() )
                        ITERATE ( CSeq_feat::TXref, it2, it->GetXref() )
                            NcbiCout << MSerial_AsnText << **it2;
                    if ( it->IsSetDbxref() )
                        ITERATE ( CSeq_feat::TDbxref, it2, it->GetDbxref() )
                            NcbiCout << MSerial_AsnText << **it2;
                    if ( it->IsSetPseudo() )
                        NcbiCout << "Pseudo: " << it->GetPseudo() << '\n';
                    if ( it->IsSetExcept_text() )
                        NcbiCout << "Except-text: "<< it->GetExcept_text() << '\n';
                    /*
                      if ( it->IsSetIds() )
                      ITERATE ( CSeq_feat::TIds, it2, it->GetIds() )
                      NcbiCout << MSerial_AsnText << **it2;
                      if ( it->IsSetExts() )
                      ITERATE ( CSeq_feat::TExts, it2, it->GetExts() )
                      NcbiCout << MSerial_AsnText << **it2;
                    */
                    it->GetMappedFeature();
                }

                if ( table_field.get() &&
                     it->GetSeq_feat_Handle().IsTableFeat() ) {
                    TTableField value;
                    if ( table_field->TryGet(it, value) ) {
                        NcbiCout << "table field: " << value << NcbiEndl;
                    }
                }
                
                // Get seq-annot containing the feature
                if ( print_features ) {
                    NcbiCout << "Feature:";
                    if ( it->IsSetPartial() ) {
                        NcbiCout << " partial = " << it->GetPartial();
                    }
                    try {
                        NcbiCout << "\n" <<
                            MSerial_AsnText << it->GetMappedFeature();
                    }
                    catch ( CException& exc ) {
                        ERR_POST("Exception: "<<exc);
                    }
                    if ( 1 ) {
                        NcbiCout << "Original location:";
                        if ( it->GetOriginalFeature().IsSetPartial() ) {
                            NcbiCout << " partial = " <<
                                it->GetOriginalFeature().GetPartial();
                        }
                        NcbiCout << "\n" <<
                            MSerial_AsnText <<
                            it->GetOriginalFeature().GetLocation();
                        if ( mapper ) {
                            NcbiCout << "Mapped orig location:\n" <<
                                MSerial_AsnText <<
                                *mapper->Map(it->GetOriginalFeature()
                                             .GetLocation());
                            NcbiCout << "Mapped iter location:\n"<<
                                MSerial_AsnText <<
                                *mapper->Map(it->GetLocation());
                        }
                    }
                    else {
                        NcbiCout << "Location:\n" <<
                            MSerial_AsnText << it->GetLocation();
                    }
                }

                if ( modify ) {
                    it.GetAnnot().GetEditHandle();
                }
                if ( handle && print_features &&
                     it->GetFeatSubtype() == CSeqFeatData::eSubtype_mRNA &&
                     it->IsSetProduct() ) {
                    using namespace sequence;
                    if ( modify ) {
                        handle.GetEditHandle();
                    }
                    CSeq_id_Handle prod_idh =
                        GetIdHandle(it->GetProduct(), NULL);
                    NcbiCout << "mRNA product: " << prod_idh.AsString()
                             << NcbiEndl;
                    CBioseq_Handle bsh =
                        scope.GetBioseqHandleFromTSE(prod_idh, handle);
                    if ( bsh ) {
                        NcbiCout << "GetBestXxxForMrna: "
                                 << MSerial_AsnText
                                 << it->GetOriginalFeature()
                                 << NcbiEndl;
                        
                        CConstRef<CSeq_feat> gene =
                            GetBestGeneForMrna(it->GetOriginalFeature(),
                                               scope);
                        NcbiCout << "GetBestGeneForMrna: ";
                        if ( gene ) {
                            NcbiCout << MSerial_AsnText << *gene;
                        }
                        else {
                            NcbiCout << "null";
                        }
                        NcbiCout << NcbiEndl;
                        CConstRef<CSeq_feat> cds =
                            GetBestCdsForMrna(it->GetOriginalFeature(),
                                              scope);
                        NcbiCout << "GetBestCdsForMrna: ";
                        if ( cds ) {
                            NcbiCout << MSerial_AsnText << *cds;
                        }
                        else {
                            NcbiCout << "null";
                        }
                        NcbiCout << NcbiEndl;
                    }
                }
                if ( print_features &&
                     it->GetFeatType() == CSeqFeatData::e_Cdregion ) {
                    using namespace sequence;
                    CConstRef<CSeq_feat> gene =
                        GetBestOverlappingFeat(it->GetLocation(),
                                               CSeqFeatData::e_Gene,
                                               eOverlap_Contains,
                                               scope);
                    NcbiCout << "GetBestGeneForCds: "<<it->GetLocation();
                    if ( gene ) {
                        NcbiCout << MSerial_AsnText << *gene;
                        NcbiCout << " compare: " <<
                            MSerial_AsnText << gene->GetLocation() <<
                            "\n with: "<< it->GetOriginalFeature().GetLocation() <<
                            "\n = " << sequence::Compare(gene->GetLocation(),
                                                         it->GetOriginalFeature().GetLocation(),
                                                         &scope);
                    }
                    else {
                        NcbiCout << "null";
                    }
                    NcbiCout << NcbiEndl;
                }

                CSeq_annot_Handle annot = it.GetAnnot();
                if ( get_feat_handle && it->IsPlainFeat() ) {
                    CSeq_feat_Handle fh =
                        scope.GetSeq_featHandle(it->GetOriginalFeature());
                    if ( !fh ) {
                        NcbiCout << "Reverse CSeq_feat_Handle lookup failed."
                                 << NcbiEndl;
                    }
                    else if ( fh.GetOriginalSeq_feat() !=
                              &it->GetOriginalFeature() ) {
                        NcbiCout << "Reverse CSeq_feat_Handle differs: "
                                 << MSerial_AsnText<<*fh.GetOriginalSeq_feat()
                                 << NcbiEndl;
                    }
                }
                if ( 0 ) {
                    CMappedFeat mf1 = *it;
                    CMappedFeat mf2 =
                        feature::MapSeq_feat(mf1.GetSeq_feat_Handle(), handle);
                    _ASSERT(mf1 == mf2);
                }
            }
            NcbiCout << "Feat count (loc range, " << sel_msg << "):\t"
                     << count << NcbiEndl;
            if ( count_types ) {
                ITERATE ( vector<int>, it, types_counts ) {
                    if ( *it ) {
                        CSeqFeatData::E_Choice type =
                            CSeqFeatData::E_Choice(it-types_counts.begin());
                        NcbiCout << "  type " <<
                            setw(2) << type <<
                            setw(10) << CSeqFeatData::SelectionName(type) <<
                            " : " << *it << NcbiEndl;
                    }
                }
            }
            if ( count_subtypes ) {
                ITERATE ( vector<int>, it, subtypes_counts ) {
                    if ( *it ) {
                        CSeqFeatData::ESubtype subtype =
                            CSeqFeatData::ESubtype(it-subtypes_counts.begin());
                        CSeqFeatData::E_Choice type =
                            CSeqFeatData::GetTypeFromSubtype(subtype);
                        NcbiCout << "  subtype " <<
                            setw(3) << subtype <<
                            setw(10) << CSeqFeatData::SelectionName(type) <<
                            " : " << *it << NcbiEndl;
                    }
                }
            }
            if ( make_tree ) {
                feature::CFeatTree feat_tree;
                {{
                    CFeat_CI it(scope, *range_loc, base_sel);
                    feat_tree.AddFeatures(it);
                    NcbiCout << "Added "<<it.GetSize()<<" features."
                             << NcbiEndl;
                }}
                NcbiCout << " Root features: "
                         << feat_tree.GetChildren(CMappedFeat()).size()
                         << NcbiEndl;
                if ( print_tree ) {
                    typedef pair<string, CMappedFeat> TFeatureKey;
                    typedef set<TFeatureKey> TOrderedFeatures;
                    typedef map<CMappedFeat, TOrderedFeatures> TOrderedTree;
                    TOrderedTree tree;
                    TOrderedFeatures all;
                    list<CMappedFeat> q;
                    q.push_back(CMappedFeat());
                    ITERATE ( list<CMappedFeat>, pit, q ) {
                        CMappedFeat parent = *pit;
                        vector<CMappedFeat> cc = 
                            feat_tree.GetChildren(parent);
                        TOrderedFeatures& dst = tree[parent];
                        ITERATE ( vector<CMappedFeat>, cit, cc ) {
                            CMappedFeat child = *cit;
                            CNcbiOstrstream str;
                            str << MSerial_AsnText << child.GetMappedFeature();
                            string s = CNcbiOstrstreamToString(str);
                            dst.insert(make_pair(s, child));
                            all.insert(make_pair(s, child));
                            q.push_back(child);
                        }
                    }
                    size_t cnt = 0;
                    map<TFeatureKey, size_t> index;
                    ITERATE ( TOrderedFeatures, fit, all ) {
                        index[*fit] = cnt;
                        NcbiCout << "Feature "<<cnt<<": " << fit->first;
                        ++cnt;
                    }
                    NcbiCout << "Tree:\n";
                    ITERATE ( TOrderedFeatures, fit, all ) {
                        NcbiCout << "Children of "<<index[*fit] << ": ";
                        const TOrderedFeatures& cc = tree[fit->second];
                        ITERATE ( TOrderedFeatures, cit, cc ) {
                            NcbiCout << " " << index[*cit];
                        }
                        NcbiCout << "\n";
                    }
                    NcbiCout << NcbiEndl;
                }
            }
        }}

        if ( !only_features && check_cds ) {
            count = 0;
            // The same region, but restricted feature type:
            // searching for e_Cdregion features only. If the sequence is
            // segmented (constructed), search for features on the referenced
            // sequences in the same top level seq-entry, ignore far pointers.
            SAnnotSelector sel = base_sel;
            sel.SetFeatType(CSeqFeatData::e_Cdregion);
            size_t no_product_count = 0;
            for ( CFeat_CI it(scope, *range_loc, sel); it;  ++it ) {
                count++;
                // Get seq vector filtered with the current feature location.
                // e_ViewMerged flag forces each residue to be shown only once.
                CSeqVector cds_vect;
                if ( by_product ) {
                    cds_vect = CSeqVector(it->GetLocation(), scope,
                                          CBioseq_Handle::eCoding_Iupac);
                }
                else {
                    if ( it->IsSetProduct() ) {
                        cds_vect = CSeqVector(it->GetProduct(), scope,
                                              CBioseq_Handle::eCoding_Iupac);
                    }
                    else {
                        ++no_product_count;
                        continue;
                    }
                }
                // Print first 10 characters of each cd-region
                if ( print_cds ) {
                    NcbiCout << "cds" << count <<
                        " len=" << cds_vect.size() << " data=";
                }
                if ( cds_vect.size() == 0 ) {
                    NcbiCout << "Zero size from: " << MSerial_AsnText <<
                        it->GetOriginalFeature().GetLocation();
                    NcbiCout << "Zero size to: " << MSerial_AsnText <<
                        it->GetMappedFeature().GetLocation();
                    NcbiCout << "Zero size to: " << MSerial_AsnText <<
                        it->GetLocation();

                    CSeqVector v2(it->GetLocation(), scope,
                                  CBioseq_Handle::eCoding_Iupac);
                    NcbiCout << v2.size() << NcbiEndl;
                    
                    const CSeq_id* mapped_id = 0;
                    it->GetMappedFeature().GetLocation().CheckId(mapped_id);
                    _ASSERT(mapped_id);
                    _ASSERT(by_product ||
                            CSeq_id_Handle::GetHandle(*mapped_id)==idh);
                }
                
                sout = "";
                for (TSeqPos i = 0; (i < cds_vect.size()) && (i < 10); i++) {
                    // Convert sequence symbols to printable form
                    sout += cds_vect[i];
                }
                if ( print_cds ) {
                    NcbiCout << NStr::PrintableString(sout) << NcbiEndl;
                }
            }
            NcbiCout << "Feat count (loc range, cds):\t" << count << NcbiEndl;
            if ( no_product_count ) {
                NcbiCout << "*** no product on " << no_product_count << " cds"
                         << NcbiEndl;
            }
        }

        // Search features only in the TSE containing the target bioseq.
        // Since only one seq-id may be used as the target bioseq, the
        // iterator is constructed not from a seq-loc, but from a bioseq handle
        // and start/stop points on the bioseq.
        // If both start and stop are 0 the whole bioseq is used.
        // The last parameter may be used for type filtering.
        count = 0;
        for ( CFeat_CI it(scope, *range_loc, base_sel); it; ++it ) {
            count++;
        }
        NcbiCout << "Feat count (bh range, " << sel_msg << "):\t"
                 << count << NcbiEndl;

        if ( !only_features ) {
            if ( handle && whole_tse ) {
                count = 0;
                for (CFeat_CI it(handle.GetTopLevelEntry(), base_sel);
                     it; ++it) {
                    count++;
                }
                NcbiCout << "Feat count        (TSE):\t" << count << NcbiEndl;
            }

            // The same way may be used to iterate aligns and graphs,
            // except that there is no type filter for both of them.
            count = 0;
            for ( CGraph_CI it(scope, *range_loc, base_sel); it;  ++it) {
                count++;
                // Get seq-annot containing the feature
                if ( get_mapped_location )
                    it->GetLoc();
                if ( get_original_feature )
                    it->GetOriginalGraph();
                if ( get_mapped_feature )
                    it->GetMappedGraph();
                if ( print_features ) {
                    NcbiCout << MSerial_AsnText <<
                        it->GetMappedGraph() << it->GetLoc();
                }
                CSeq_annot_Handle annot = it.GetAnnot();
            }
            NcbiCout << "Graph count (loc range):\t" << count << NcbiEndl;

            if ( !skip_alignments ) {
                count = 0;
                // Create CAlign_CI using the current scope and location.
                for (CAlign_CI it(scope, *range_loc, base_sel); it;  ++it) {
                    count++;
                    if ( get_mapped_alignments ) {
                        *it;
                    }
                    if ( print_alignments ) {
                        NcbiCout << MSerial_AsnText << *it;
                        NcbiCout << "Original Seq-align: "
                                 << MSerial_AsnText 
                                 << it.GetOriginalSeq_align();
                    }
                }
                NcbiCout << "Align count (loc range):\t" <<count<<NcbiEndl;
            }

            if ( true ) {
                count = 0;
                // Create CAlign_CI using the current scope and location.
                SAnnotSelector sel = base_sel;
                sel.SetAnnotType(CSeq_annot::C_Data::e_Seq_table);
                for (CAnnot_CI it(scope, *range_loc, sel); it;  ++it) {
                    count++;
                    if ( true ) {
                        CSeq_annot_Handle annot = *it;
                        size_t rows = annot.GetSeq_tableNumRows();
                        NcbiCout << "Seq-table with " << rows << " rows."
                                 << NcbiEndl;
                        if ( table_field.get() ) {
                            for ( size_t row = 0; row < rows; ++row ) {
                                TTableField value;
                                if ( table_field->TryGet(annot, row, value) ) {
                                    NcbiCout << "table field["<<row<<"]: "
                                             << value << NcbiEndl;
                                }
                            }
                        }
                    }
                }
                NcbiCout << "Table count (loc range):\t" <<count<<NcbiEndl;
            }
        }

        if ( handle && scan_seq_map ) {
            TSeqPos range_length =
                range_to == 0? kInvalidSeqPos: range_to - range_from + 1;
            TSeqPos actual_end =
                range_to == 0? handle.GetBioseqLength(): range_to + 1;
            TSeqPos actual_length = actual_end; actual_length -= range_from;
            const CSeqMap& seq_map = handle.GetSeqMap();
            NcbiCout << "Mol type: " << seq_map.GetMol() << NcbiEndl;
            for (size_t level = 0;  level < 5;  ++level) {
                NcbiCout << "Level " << level << NcbiEndl;
                TSeqPos total_length = 0;
                CSeqMap::TFlags flags = CSeqMap::fDefaultFlags;
                if ( exact_depth ) {
                    flags |= CSeqMap::fFindExactLevel;
                }
                CSeqMap::const_iterator seg =
                    seq_map.ResolvedRangeIterator(&scope,
                                                  range_from,
                                                  range_length,
                                                  eNa_strand_plus,
                                                  level,
                                                  flags);
                _ASSERT(level || seg.GetPosition() == range_from);
                for ( ;  seg;  ++seg ) {
                    NcbiCout << " @" << seg.GetPosition() << "-" <<
                        seg.GetEndPosition() << " +" <<
                        seg.GetLength() << ": ";
                    _ASSERT(seg.GetEndPosition()-seg.GetPosition() == seg.GetLength());
                    switch (seg.GetType()) {
                    case CSeqMap::eSeqRef:
                        NcbiCout << "ref: " <<
                            seg.GetRefSeqid().AsString() << " " <<
                            (seg.GetRefMinusStrand()? "minus ": "") <<
                            seg.GetRefPosition() << "-" <<
                            seg.GetRefEndPosition();
                        _ASSERT(seg.GetRefEndPosition()-seg.GetRefPosition() == seg.GetLength());
                        break;
                    case CSeqMap::eSeqData:
                        NcbiCout << "data: ";
                        break;
                    case CSeqMap::eSeqGap:
                        NcbiCout << "gap: ";
                        break;
                    case CSeqMap::eSeqEnd:
                        NcbiCout << "end: ";
                        _ASSERT("Unexpected END segment" && 0);
                        break;
                    default:
                        NcbiCout << "?: ";
                        _ASSERT("Unexpected segment type" && 0);
                        break;
                    }
                    total_length += seg.GetLength();
                    NcbiCout << NcbiEndl;
                }
                _ASSERT(level || total_length == actual_length);
                _ASSERT(seg.GetPosition() == actual_end);
                _ASSERT(seg.GetLength() == 0);
                TSeqPos new_length = 0;
                for ( --seg; seg; --seg ) {
                    _ASSERT(seg.GetType() != CSeqMap::eSeqEnd);
                    new_length += seg.GetLength();
                }
                _ASSERT(total_length == new_length);
                _ASSERT(level || seg.GetPosition() == range_from);
                _ASSERT(seg.GetLength() == 0);
                new_length = 0;
                for ( ++seg; seg; ++seg ) {
                    _ASSERT(seg.GetType() != CSeqMap::eSeqEnd);
                    new_length += seg.GetLength();
                }
                _ASSERT(total_length == new_length);
                _ASSERT(seg.GetPosition() == actual_end);
                _ASSERT(seg.GetLength() == 0);
            }
            CSeqMap::const_iterator begin = seq_map.begin(0);
            _ASSERT(begin.GetPosition() == 0);
            CSeqMap::const_iterator end = seq_map.end(0);
            _ASSERT(end.GetType() == CSeqMap::eSeqEnd);
            _ASSERT(end.GetPosition() == handle.GetBioseqLength());
            TSeqPos total_length = 0;
            for ( CSeqMap::const_iterator iter = begin; iter != end; ++iter ) {
                _ASSERT(iter.GetType() != CSeqMap::eSeqEnd);
                total_length += iter.GetLength();
            }
            _ASSERT(total_length == handle.GetBioseqLength());
            total_length = 0;
            for ( CSeqMap::const_iterator iter = end; iter != begin; ) {
                --iter;
                _ASSERT(iter.GetType() != CSeqMap::eSeqEnd);
                total_length += iter.GetLength();
            }
            _ASSERT(total_length == handle.GetBioseqLength());
        }

        if ( handle && modify ) {
            //CTSE_Handle tse = handle.GetTSE_Handle();
            //CBioseq_EditHandle ebh = handle.GetEditHandle();
            CRef<CBioseq> newseq(new CBioseq);
            newseq->Assign(*handle.GetCompleteObject());
            CSeq_entry_Handle seh = handle.GetParentEntry();
            seh.GetEditHandle().SelectNone();
            seh.GetEditHandle().SelectSeq(*newseq);
        }
        if ( used_memory_check ) {
            if ( args["reset_scope"] ) {
                scope.ResetHistory();
                handle.Reset();
            }
            exit(0);
        }

        if ( handle ) {
            scope.RemoveFromHistory(handle);
            _ASSERT(!handle);
        }
        
        handle.Reset();
        scope.ResetHistory();
    }
    if ( modify ) {
        handle = scope.GetBioseqHandle(idh);
        CBioseq_EditHandle ebh = handle.GetEditHandle();
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
    int ret = CDemoApp().AppMain(argc, argv);
    NcbiCout << NcbiEndl;
    return ret;
}
