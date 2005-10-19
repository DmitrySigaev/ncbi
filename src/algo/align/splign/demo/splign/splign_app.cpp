/* $Id$
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
 * Author:  Yuri Kapustin
 *
 * File Description: Splign application
 *                   
*/

#include <ncbi_pch.hpp>
#include "splign_app.hpp"
#include "splign_app_exception.hpp"

#include <corelib/ncbistd.hpp>
#include <serial/objostrasn.hpp>
#include <serial/serial.hpp>

#include <algo/align/nw/nw_spliced_aligner16.hpp>
#include <algo/align/nw/nw_spliced_aligner32.hpp>

#include <objmgr/object_manager.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/seqloc/Seq_loc.hpp>

#include <objtools/readers/fasta.hpp>
#include <objtools/lds/lds_admin.hpp>
#include <objtools/data_loaders/lds/lds_dataloader.hpp>

#include <iostream>
#include <memory>


BEGIN_NCBI_SCOPE

const char kQuality_high[] = "high";
const char kQuality_low[] = "low";

const char kStrandPlus[] = "plus";
const char kStrandMinus[] = "minus";
const char kStrandBoth[] = "both";
const char kStrandAuto[] = "auto";

void CSplignApp::Init()
{
    HideStdArgs( fHideLogfile | fHideConffile | fHideVersion);

    SetVersion(CVersionInfo(1, 19, 0, "Splign"));  
    auto_ptr<CArgDescriptions> argdescr(new CArgDescriptions);

    string program_name ("Splign v.1.19");

#ifdef GENOME_PIPELINE
    program_name += 'p';
#endif

    argdescr->SetUsageContext(GetArguments().GetProgramName(), program_name);
    
    argdescr->AddOptionalKey
        ("hits", "hits",
         "[Batch mode] Externally computed input blast hits. "
         "This file defines the set of sequences to align and "
         "is also used to guide alignments. "
         "The file must be collated by subject and query "
         "(e.g. sort -k 2,2 -k 1,1).",
         CArgDescriptions::eInputFile);

    argdescr->AddOptionalKey
        ("mklds", "mklds",
         "[Batch mode] Make LDS DB under the specified directory "
         "with cDNA and genomic FASTA files or symlinks.",
         CArgDescriptions::eString);

    argdescr->AddOptionalKey
        ("ldsdir", "ldsdir",
         "[Batch mode] Directory holding LDS subdirectory.",
         CArgDescriptions::eString);

    argdescr->AddOptionalKey
        ("query", "query",
         "[Pairwise mode] FASTA file with the spliced sequence. "
         "Must be used with -subj.",
         CArgDescriptions::eInputFile);
    
    argdescr->AddOptionalKey
        ("subj", "subj",
         "[Pairwise mode] FASTA file with the genomic sequence(s). "
         "Must be used with -query. ",
         CArgDescriptions::eInputFile);
    
#ifdef GENOME_PIPELINE
    argdescr->AddOptionalKey
        ("querydb", "querydb",
         "[Batch mode] Pathname to the blast database of query (cDNA) "
         "sequences. To create one, use formatdb -o T -p F",
         CArgDescriptions::eString);
#endif

    argdescr->AddFlag("cross", "Cross-species mode");
    
    argdescr->AddDefaultKey
        ("log", "log", "Splign log file",
         CArgDescriptions::eOutputFile,
         "splign.log");
    
    argdescr->AddOptionalKey
        ("asn", "asn", "ASN.1 output file name", 
         CArgDescriptions::eOutputFile);
    
    argdescr->AddDefaultKey
        ("strand", "strand", "Spliced sequence's strand.",
         CArgDescriptions::eString, kStrandPlus);
    
    argdescr->AddFlag ("noendgaps",
                       "Skip detection of unaligning regions at the ends.",
                       true);
    
    argdescr->AddFlag ("nopolya", "Assume no Poly(A) tail.",  true);
    
    argdescr->AddDefaultKey
        ("compartment_penalty", "compartment_penalty",
         "Penalty to open a new compartment "
         "(compartment identification parameter). "
         "Multiple compartments will only be identified if "
         "they have at least this level of coverage.",
         CArgDescriptions::eDouble,
         "0.25");
    
    argdescr->AddDefaultKey
        ("min_compartment_idty", "min_compartment_identity",
         "Minimal overall identity a compartment "
         "must have in order to be aligned.",
         CArgDescriptions::eDouble, "0.5");
    
    argdescr->AddDefaultKey
        ("max_extent", "max_extent",
         "Max genomic extent to look for exons beyond compartment boundaries "
         "as determined with Blast hits.",
         CArgDescriptions::eInteger,
         NStr::IntToString(CSplign::s_GetDefaultMaxGenomicExtent()) );
    
    argdescr->AddDefaultKey
        ("min_exon_idty", "identity",
         "Minimal exon identity. Lower identity segments "
         "will be marked as gaps.",
         CArgDescriptions::eDouble, "0.75");
    
#ifdef GENOME_PIPELINE
    
    argdescr->AddFlag ("mt","Use multiple threads (up to the number of CPUs)", 
                       true);

    argdescr->AddDefaultKey
        ("quality", "quality", "Genomic sequence quality.",
         CArgDescriptions::eString, kQuality_high);
    
    // restrictions
    CArgAllow_Strings* constrain_errlevel = new CArgAllow_Strings;
    constrain_errlevel->Allow(kQuality_low)->Allow(kQuality_high);
    argdescr->SetConstraint("quality", constrain_errlevel);
    
#endif

    argdescr->AddDefaultKey
        ("Wm", "match", "match score",
         CArgDescriptions::eInteger,
         NStr::IntToString(CNWAligner::GetDefaultWm()).c_str());
    
    argdescr->AddDefaultKey
        ("Wms", "mismatch", "mismatch score",
         CArgDescriptions::eInteger,
         NStr::IntToString(CNWAligner::GetDefaultWms()).c_str());
    
    argdescr->AddDefaultKey
        ("Wg", "gap", "gap opening score",
         CArgDescriptions::eInteger,
         NStr::IntToString(CNWAligner::GetDefaultWg()).c_str());
    
    argdescr->AddDefaultKey
        ("Ws", "space", "gap extension (space) score",
         CArgDescriptions::eInteger,
         NStr::IntToString(CNWAligner::GetDefaultWs()).c_str());
    
    argdescr->AddDefaultKey
        ("Wi0", "Wi0", "Conventional intron (GT/AG) score",
         CArgDescriptions::eInteger,
         NStr::IntToString(CSplicedAligner16::GetDefaultWi(0)).c_str());
    
    argdescr->AddDefaultKey
        ("Wi1", "Wi1", "Conventional intron (GC/AG) score",
         CArgDescriptions::eInteger,
         NStr::IntToString(CSplicedAligner16::GetDefaultWi(1)).c_str());
    
    argdescr->AddDefaultKey
        ("Wi2", "Wi2", "Conventional intron (AT/AC) score",
         CArgDescriptions::eInteger,
         NStr::IntToString(CSplicedAligner16::GetDefaultWi(2)).c_str());
    
    argdescr->AddDefaultKey
        ("Wi3", "Wi3", "Arbitrary intron score",
         CArgDescriptions::eInteger,
         NStr::IntToString(CSplicedAligner16::GetDefaultWi(3)).c_str());
    
    CArgAllow_Strings* constrain_strand = new CArgAllow_Strings;
    constrain_strand->Allow(kStrandPlus)->Allow(kStrandMinus)
        ->Allow(kStrandBoth)->Allow(kStrandAuto);
    
    argdescr->SetConstraint("strand", constrain_strand);
    
    CArgAllow* constrain01 = new CArgAllow_Doubles(0,1);
    argdescr->SetConstraint("min_compartment_idty", constrain01);
    argdescr->SetConstraint("min_exon_idty", constrain01);
    argdescr->SetConstraint("compartment_penalty", constrain01);
    
    CArgAllow* constrain_positives = new CArgAllow_Integers(1, kMax_Int);
    argdescr->SetConstraint("max_extent", constrain_positives);
    
    SetupArgDescriptions(argdescr.release());
}


bool CSplignApp::x_GetNextPair(istream& ifs, THitRefs* hitrefs)
{
    hitrefs->resize(0);

    if(!m_PendingHits.size() && !ifs ) {
        return false;
    }
    
    if(!m_PendingHits.size()) {

        CAlignShadow::TId query, subj;

        if(m_firstline.size()) {

            THitRef hitref (new CBlastTabular(m_firstline.c_str()));
            query = hitref->GetQueryId();
            subj  = hitref->GetSubjId();
            m_PendingHits.push_back(hitref);
        }

        char buf [1024];
        while(ifs) {

            buf[0] = 0;
            CT_POS_TYPE pos0 = ifs.tellg();
            ifs.getline(buf, sizeof buf, '\n');
            CT_POS_TYPE pos1 = ifs.tellg();
            if(pos1 == pos0) break; // GCC hack
            if(buf[0] == '#') continue; // skip comments
            const char* p = buf; // skip leading spaces
            while(*p == ' ' || *p == '\t') ++p;
            if(*p == 0) continue; // skip empty lines
            
            THitRef hit (new CBlastTabular(p));
            if(query.IsNull()) {
                query = hit->GetQueryId();
            }
            if(subj.IsNull()) {
                subj = hit->GetSubjId();
            }
            if(hit->GetQueryStrand() == false) {
                hit->FlipStrands();
            }
            if(hit->GetSubjStop() == hit->GetSubjStart()) {
                // skip single bases
                continue;
            }
            
            if(hit->GetQueryId()->Match(*query) == false || 
               hit->GetSubjId()->Match(*subj) == false) {

                m_firstline = p;
                break;
            }
            
            m_PendingHits.push_back(hit);
        }
    }

    const size_t pending_size = m_PendingHits.size();
    if(pending_size) {

        CAlignShadow::TId query = m_PendingHits[0]->GetQueryId();
        CAlignShadow::TId subj  = m_PendingHits[0]->GetSubjId();
        size_t i = 1;
        for(; i < pending_size; ++i) {

            THitRef h = m_PendingHits[i];
            if(h->GetQueryId()->Match(*query) == false || 
               h->GetSubjId()->Match(*subj) == false) {
                break;
            }
        }
        hitrefs->resize(i);
        copy(m_PendingHits.begin(), m_PendingHits.begin() + i, 
             hitrefs->begin());
        m_PendingHits.erase(m_PendingHits.begin(), m_PendingHits.begin() + i);
    }
    
    return hitrefs->size() > 0;
}


void CSplignApp::x_LogStatus(size_t model_id, 
                             bool query_strand,
                             const CAlignShadow::TId& query, 
                             const CAlignShadow::TId& subj, 
			     bool error, 
                             const string& msg)
{
    string error_tag (error? "Error: ": "");
    if(model_id == 0) {
        *m_logstream << '-';
    }
    else {
        *m_logstream << (query_strand? '+': '-') << model_id;
    }
    
    *m_logstream << '\t' << query->GetSeqIdString(true) 
                 << '\t' << subj->GetSeqIdString(true)
                 << '\t' << error_tag << msg << endl;
}


CRef<blast::CBlastOptionsHandle> CSplignApp::x_SetupBlastOptions(bool cross)
{
    USING_SCOPE(blast);

    m_BlastProgram = cross? eDiscMegablast: eMegablast;

    CRef<CBlastOptionsHandle> blast_options_handle
        (CBlastOptionsFactory::Create(m_BlastProgram));

    blast_options_handle->SetDefaults();
    CBlastOptions& blast_opt = blast_options_handle->SetOptions();

    blast_opt.SetGapExtensionCost(4);
    blast_opt.SetMismatchPenalty(-4);
    blast_opt.SetMatchReward(1);
    if(!cross) {
        blast_opt.SetWordSize(20);
        blast_opt.SetGapXDropoff(0);
        blast_opt.SetGapXDropoffFinal(0);
    }

    return blast_options_handle;
}


enum ERunMode {
    eNotSet,
    ePairwise, // single query vs single subj
    eBatch1,   // use external blast hits
    eBatch2,   // run blast internally using external blast db of queries
    eIncremental // run blast internally using external blastdb of subjects
};


const string kSplignLdsDb ("splign.ldsdb");

string GetLdsDbDir(const string& fasta_dir)
{
    string lds_db_dir = fasta_dir;
    const char sep = CDirEntry::GetPathSeparator();
    const size_t fds = fasta_dir.size();
    if(fds > 0 && fasta_dir[fds-1] != sep) {
        lds_db_dir += sep;
    }
    lds_db_dir += "_SplignLDS_";
    return lds_db_dir;
}

int CSplignApp::Run()
{ 
    USING_SCOPE(objects);

    const CArgs& args = GetArgs();    

    // check that modes aren't mixed

    const bool is_mklds   = args["mklds"];
    const bool is_hits    = args["hits"];
    const bool is_ldsdir  = args["ldsdir"];

    const bool is_query   = args["query"];
    const bool is_subj    = args["subj"];
#ifdef GENOME_PIPELINE
    const bool is_querydb = args["querydb"];
#else
    const bool is_querydb = false;
#endif
    const bool is_cross   = args["cross"];

    if(is_mklds) {

        // create LDS DB and exit
        string fa_dir = args["mklds"].AsString();
        if(CDirEntry::IsAbsolutePath(fa_dir) == false) {
            string curdir = CDir::GetCwd();
            const char sep = CDirEntry::GetPathSeparator();            
            const size_t curdirsize = curdir.size();
            if(curdirsize && curdir[curdirsize-1] != sep) {
                curdir += sep;
            }
            fa_dir = curdir + fa_dir;
        }
        const string lds_db_dir = GetLdsDbDir(fa_dir);
        CLDS_Database ldsdb (lds_db_dir, kSplignLdsDb, kSplignLdsDb);
        CLDS_Management ldsmgt (ldsdb);
        ldsmgt.Create();
        ldsmgt.SyncWithDir(fa_dir,
                           CLDS_Management::eRecurseSubDirs,
                           CLDS_Management::eNoControlSum);
        return 0;
    }

    // determine mode and verify arguments
    ERunMode run_mode (eNotSet);
    
    if(is_query && is_subj && !(is_hits || is_querydb || is_ldsdir)) {
        run_mode = ePairwise;
    }
    else if(is_subj && is_querydb && !(is_query || is_hits || is_ldsdir)) {
        run_mode = eBatch2;
    }
    else if(is_hits && is_ldsdir && !(is_query || is_subj || is_querydb)) {
        run_mode = eBatch1;
    }

    if(run_mode == eNotSet) {
        NCBI_THROW(CSplignAppException,
                   eBadParameter,
                   "Incomplete or inconsistent set of input parameters." );
    }   

    // open log stream
    m_logstream = & args["log"].AsOutputFile();
    
    // open asn output stream, if any
    m_AsnOut = args["asn"]? & args["asn"].AsOutputFile(): NULL;
    
    // in pairwise and batch 2 mode, setup blast options
    if(run_mode == ePairwise || run_mode == eBatch2) {
        m_BlastOptionsHandle = x_SetupBlastOptions(is_cross);
    }

    // aligner setup    
    string quality;
#ifndef GENOME_PIPELINE
    quality = kQuality_high;
#else
    quality = args["quality"].AsString();
#endif
    
    CRef<CSplicedAligner> aligner ( quality == kQuality_high ?
        static_cast<CSplicedAligner*> (new CSplicedAligner16):
        static_cast<CSplicedAligner*> (new CSplicedAligner32)  );

#if GENOME_PIPELINE
    aligner->SetWm(args["Wm"].AsInteger());
    aligner->SetWms(args["Wms"].AsInteger());
    aligner->SetWg(args["Wg"].AsInteger());
    aligner->SetWs(args["Ws"].AsInteger());
    aligner->SetScoreMatrix(NULL);
    
    for(size_t i = 0, n = aligner->GetSpliceTypeCount(); i < n; ++i) {
        string arg_name ("Wi");
        arg_name += NStr::IntToString(i);
        aligner->SetWi(i, args[arg_name.c_str()].AsInteger());
    }
    
    aligner->EnableMultipleThreads(args["mt"]);
#endif
    
    // splign and formatter setup    
    m_Splign.Reset(new CSplign);
    m_Splign->SetPolyaDetection(!args["nopolya"]);
    m_Splign->SetMinCompartmentIdentity(
                 args["min_compartment_idty"].AsDouble());
    m_Splign->SetMinExonIdentity(args["min_exon_idty"].AsDouble());
    m_Splign->SetCompartmentPenalty(args["compartment_penalty"].AsDouble());
    m_Splign->SetEndGapDetection(!(args["noendgaps"]));
    m_Splign->SetMaxGenomicExtent(args["max_extent"].AsInteger());
    m_Splign->SetAligner() = aligner;
    m_Splign->SetStartModelId(1);

    CRef<CObjectManager> objmgr = CObjectManager::GetInstance();
    CRef<CScope> scope;

    CRef<CSeq_id> seqid_query, seqid_subj;
    if(run_mode == ePairwise) {

        scope.Reset (new CScope(*objmgr));
        scope->AddDefaults();

        CRef<CSeq_entry> se (
               ReadFasta(args["query"].AsInputFile(), fReadFasta_OneSeq));
        scope->AddTopLevelSeqEntry(*se);
        {{
            const CSeq_entry::TSeq& bioseq = se->GetSeq();    
            const CSeq_entry::TSeq::TId& seqid = bioseq.GetId();
            seqid_query = seqid.back();
        }}

        se = ReadFasta(args["subj"].AsInputFile(), fReadFasta_OneSeq);
        scope->AddTopLevelSeqEntry(*se);
        {{
            const CSeq_entry::TSeq& bioseq = se->GetSeq();    
            const CSeq_entry::TSeq::TId& seqid = bioseq.GetId();
            seqid_subj = seqid.back();
        }}
    }
    else if(run_mode == eBatch1) {
        
        const string fasta_dir = args["ldsdir"].AsString();
        const string ldsdb_dir = GetLdsDbDir(fasta_dir);
        CLDS_Database* ldsdb (
              new CLDS_Database(ldsdb_dir, kSplignLdsDb, kSplignLdsDb));
        m_LDS_db.reset(ldsdb);
        m_LDS_db->Open();
        CLDS_DataLoader::RegisterInObjectManager(*objmgr, 
                                                 *ldsdb, 
                                                 CObjectManager::eDefault);
        scope.Reset (new CScope(*objmgr));
        scope->AddDefaults();
    }

    m_Splign->SetScope() = scope;    

    // splign formatter object    
    m_Formatter.Reset(new CSplignFormatter(*m_Splign));

    if(run_mode == ePairwise) {
        THitRefs hitrefs;
        x_GetBl2SeqHits(seqid_query, seqid_subj, scope, &hitrefs);
        x_ProcessPair(hitrefs, args);
    }
    else if (run_mode == eBatch1) {

        THitRefs hitrefs;
        CNcbiIstream& hit_stream = args["hits"].AsInputFile();
        while(x_GetNextPair(hit_stream, &hitrefs) ) {
            x_ProcessPair(hitrefs, args);
        }
    }
    else {
        NCBI_THROW(CSplignAppException,
                   eInternal,
                   "Batch mode not yet implemented");
    }
        
    return 0;
}


void CSplignApp::x_GetBl2SeqHits(
    CRef<objects::CSeq_id> seqid_query,  
    CRef<objects::CSeq_id> seqid_subj, 
    CRef<objects::CScope>  scope,  
    THitRefs* phitrefs)
{
    USING_SCOPE(blast);
    USING_SCOPE(objects);

    phitrefs->resize(0);
    phitrefs->reserve(100);

    CRef<CSeq_loc> seqloc_query (new CSeq_loc);
    seqloc_query->SetWhole().Assign(*seqid_query);
    CRef<CSeq_loc> seqloc_subj (new CSeq_loc);
    seqloc_subj->SetWhole().Assign(*seqid_subj);

    CBl2Seq Blast( SSeqLoc(seqloc_query.GetNonNullPointer(),
                           scope.GetNonNullPointer()),
                   SSeqLoc(seqloc_subj.GetNonNullPointer(),
                           scope.GetNonNullPointer()),
                   m_BlastProgram);
    
    Blast.SetOptionsHandle() = *m_BlastOptionsHandle;

    TSeqAlignVector blast_output (Blast.Run());

    ITERATE(TSeqAlignVector, ii, blast_output) {
        if((*ii)->IsSet()) {
            const CSeq_align_set::Tdata &sas0 = (*ii)->Get();
            ITERATE(CSeq_align_set::Tdata, sa_iter0, sas0) {
                const CSeq_align_set::Tdata &sas = 
                    (*sa_iter0)->GetSegs().GetDisc().Get();
                ITERATE(CSeq_align_set::Tdata, sa_iter, sas) {
                    CRef<CBlastTabular> hitref (new CBlastTabular(**sa_iter));
                    if(hitref->GetQueryStrand() == false) {
                        hitref->FlipStrands();
                    }
                    phitrefs->push_back(hitref);
                }
            }
        }
    }
}


void CSplignApp::x_ProcessPair(THitRefs& hitrefs, const CArgs& args)
{
    if(hitrefs.size() == 0) {
        return;
    }
    
    CAlignShadow::TId query = hitrefs.front()->GetQueryId();
    CAlignShadow::TId subj  = hitrefs.front()->GetSubjId();
    
    m_Formatter->SetSeqIds( query, subj );
    
    const string strand = args["strand"].AsString();
    CSplign::TResults splign_results;
    if(strand == kStrandPlus) {
        m_Splign->SetStrand(true);
        m_Splign->Run(&hitrefs);
        const CSplign::TResults& results = m_Splign->GetResult();
        copy(results.begin(), results.end(), back_inserter(splign_results));
    }
    else if(strand == kStrandMinus) {
            
        m_Splign->SetStrand(false);
        m_Splign->Run(&hitrefs);
        const CSplign::TResults& results = m_Splign->GetResult();
        copy(results.begin(), results.end(), back_inserter(splign_results));
    }
    else if(strand == kStrandBoth) {
        
        THitRefs hits0 (hitrefs.begin(), hitrefs.end());
        static size_t mid = 1;
        size_t mid_plus, mid_minus;
        {{
            m_Splign->SetStrand(true);
            m_Splign->SetStartModelId(mid);
            m_Splign->Run(&hitrefs);
            const CSplign::TResults& results = m_Splign->GetResult();
            copy(results.begin(), results.end(), 
                 back_inserter(splign_results));
            mid_plus = m_Splign->GetNextModelId();
        }}
        {{
            m_Splign->SetStrand(false);
            m_Splign->SetStartModelId(mid);
            m_Splign->Run(&hits0);
            const CSplign::TResults& results = m_Splign->GetResult();
            copy(results.begin(), results.end(), 
                 back_inserter(splign_results));
            mid_minus = m_Splign->GetNextModelId();
        }}
        mid = max(mid_plus, mid_minus);
    }
    else {
        NCBI_THROW(CSplignAppException, eGeneral, 
                   "Auto strand not yet implemented");
    }
        
    cout << m_Formatter->AsText(&splign_results) << flush;
        
    if(m_AsnOut) {

        *m_AsnOut << MSerial_AsnText 
                  << m_Formatter->AsSeqAlignSet(&splign_results)
                  << endl;
    }
        
    ITERATE(CSplign::TResults, ii, splign_results) {
        x_LogStatus(ii->m_id, ii->m_QueryStrand, query, subj,
                    ii->m_error, ii->m_msg);
    }
    
    if(splign_results.size() == 0) {
        x_LogStatus(0, true, query, subj, true, "No compartment found");
    }
}


END_NCBI_SCOPE

/////////////////////////////////////

USING_NCBI_SCOPE;

#ifdef GENOME_PIPELINE

// make old-style Splign index file
void MakeIDX( istream* inp_istr, const size_t file_index, ostream* out_ostr )
{
    istream * inp = inp_istr? inp_istr: &cin;
    ostream * out = out_ostr? out_ostr: &cout;
    inp->unsetf(IOS_BASE::skipws);
    char c0 = '\n', c;
    while(inp->good()) {
        c = inp->get();
        if(c0 == '\n' && c == '>') {
            CT_OFF_TYPE pos = inp->tellg() - CT_POS_TYPE(1);
            string s;
            *inp >> s;
            *out << s << '\t' << file_index << '\t' << pos << endl;
        }
        c0 = c;
    }
}
#endif

int main(int argc, const char* argv[]) 
{

#ifdef GENOME_PIPELINE

    // pre-scan for mkidx
    for(int i = 1; i < argc; ++i) {
        if(0 == strcmp(argv[i], "-mkidx")) {
            
            if(i + 1 == argc) {
                char err_msg [] = 
                    "ERROR: No FastA files specified to index. "
                    "Please specify one or more FastA files after -mkidx. "
                    "Your system may support wildcards "
                    "to specify multiple files.";
                cerr << err_msg << endl;
                return 1;
            }
            else {
                ++i;
            }
            vector<string> fasta_filenames;
            for(; i < argc; ++i) {
                fasta_filenames.push_back(argv[i]);
                // test
                ifstream ifs (argv[i]);
                if(ifs.fail()) {
                    cerr << "ERROR: Unable to open " << argv[i] << endl;
                    return 1;
                }
            }
            
            // write the list of files
            const size_t files_count = fasta_filenames.size();
            cout << "# This file was generated by Splign." << endl;
            cout << "$$$FI" << endl;
            for(size_t k = 0; k < files_count; ++k) {
                cout << fasta_filenames[k] << '\t' << k << endl;
            }
            cout << "$$$SI" << endl;
            for(size_t k = 0; k < files_count; ++k) {
                ifstream ifs (fasta_filenames[k].c_str());
                MakeIDX(&ifs, k, &cout);
            }
            
            return 0;
        }
    }
    

#endif

    return CSplignApp().AppMain(argc, argv, 0, eDS_Default, 0);
}


/*
 * ===========================================================================
 * $Log$
 * Revision 1.51  2005/10/19 17:56:35  kapustin
 * Switch to using ObjMgr+LDS to load sequence data
 *
 * Revision 1.50  2005/09/21 13:46:45  kapustin
 * Use gapless blast when computing hits internally
 *
 * Revision 1.49  2005/09/13 16:02:04  kapustin
 * Flip hit strands when query is in minus strand
 *
 * Revision 1.48  2005/09/12 16:24:01  kapustin
 * Move compartmentization to xalgoalignutil.
 *
 * Revision 1.47  2005/09/06 17:52:52  kapustin
 * Add interface to max_extent member
 *
 * Revision 1.46  2005/08/29 14:14:49  kapustin
 * Retain last subject sequence in memory when in batch mode.
 *
 * Revision 1.45  2005/08/08 17:43:15  kapustin
 * Bug fix: keep external stream buf as long as the stream
 *
 * Revision 1.44  2005/08/02 15:57:13  kapustin
 * Adjust max genomic extent
 *
 * Revision 1.43  2005/07/05 16:50:47  kapustin
 * Adjust compartmentization and term genomic extent. 
 * Introduce min overall identity required for compartments to align.
 *
 * Revision 1.42  2005/07/01 16:40:36  ucko
 * Adjust for CSeq_id's use of CSeqIdException to report bad input.
 *
 * Revision 1.41  2005/06/02 15:13:36  kapustin
 * Call SetScoreMatrix(NULL) after changing diag scores
 *
 * Revision 1.40  2005/05/10 18:02:01  kapustin
 * Advance version number
 *
 * Revision 1.39  2005/04/28 19:26:56  kapustin
 * Note in parameter description that input hit files must be sorted.
 *
 * Revision 1.38  2005/04/28 19:17:13  kapustin
 * Add cross-species mode flag
 *
 * Revision 1.37  2005/04/14 15:29:05  kapustin
 * Advance version number
 *
 * Revision 1.36  2005/03/23 20:31:17  kapustin
 * Always set local type for not recognized SeqIds
 *
 * Revision 1.35  2005/02/23 22:14:18  kapustin
 * Remove outdated code. Shae scope object
 *
 * Revision 1.34  2005/01/31 13:45:19  kapustin
 * Enforce local seq-id if type not recognized
 *
 * Revision 1.33  2005/01/03 22:47:35  kapustin
 * Implement seq-ids with CSeq_id instead of generic strings
 *
 * Revision 1.32  2004/12/16 23:12:26  kapustin
 * algo/align rearrangement
 *
 * Revision 1.31  2004/07/21 15:51:24  grichenk
 * CObjectManager made singleton, GetInstance() added.
 * CXXXXDataLoader constructors made private, added
 * static RegisterInObjectManager() and GetLoaderNameFromArgs()
 * methods.
 *
 * Revision 1.30  2004/06/29 20:51:21  kapustin
 * Support simultaneous segment computing
 *
 * Revision 1.29  2004/06/23 19:30:44  kapustin
 * Tweak model id substitution when doing both strands
 *
 * Revision 1.28  2004/06/23 19:24:59  ucko
 * GetLastModelID() --> GetNextModelID()
 *
 * Revision 1.27  2004/06/21 18:16:45  kapustin
 * Support computation on both query strands
 *
 * Revision 1.26  2004/06/07 13:47:37  kapustin
 * Throw when no hits returned from Blast
 *
 * Revision 1.25  2004/06/03 19:30:22  kapustin
 * Specify max genomic extension ta the app level
 *
 * Revision 1.24  2004/05/21 21:41:02  gorelenk
 * Added PCH ncbi_pch.hpp
 *
 * Revision 1.23  2004/05/18 21:43:40  kapustin
 * Code cleanup
 *
 * Revision 1.22  2004/05/10 16:40:12  kapustin
 * Support a pairwise mode
 *
 * Revision 1.21  2004/05/04 15:23:45  ucko
 * Split splign code out of xalgoalign into new xalgosplign.
 *
 * Revision 1.20  2004/04/30 15:00:47  kapustin
 * Support ASN formatting
 *
 * Revision 1.19  2004/04/26 19:26:22  kapustin
 * Count models from one
 *
 * Revision 1.18  2004/04/26 15:38:46  kapustin
 * Add model_id as a CSplign member
 *
 * Revision 1.17  2004/04/23 14:33:32  kapustin
 * *** empty log message ***
 *
 * Revision 1.15  2004/02/19 22:57:55  ucko
 * Accommodate stricter implementations of CT_POS_TYPE.
 *
 * Revision 1.14  2004/02/18 16:04:53  kapustin
 * Do not print PolyA and gap contents
 *
 * Revision 1.13  2003/12/15 20:16:58  kapustin
 * GetNextQuery() ->GetNextPair()
 *
 * Revision 1.12  2003/12/09 13:21:47  ucko
 * +<memory> for auto_ptr
 *
 * Revision 1.11  2003/12/04 20:08:22  kapustin
 * Remove endgaps argument
 *
 * Revision 1.10  2003/12/04 19:26:37  kapustin
 * Account for zero-length segment vector
 *
 * Revision 1.9  2003/12/03 19:47:02  kapustin
 * Increase exon search scope at the ends
 *
 * Revision 1.8  2003/11/26 16:13:21  kapustin
 * Determine subject after filtering for more accurate log reporting
 *
 * Revision 1.7  2003/11/21 16:04:18  kapustin
 * Enable RLE for Poly(A) tail.
 *
 * Revision 1.6  2003/11/20 17:58:20  kapustin
 * Make the code msvc6.0-friendly
 *
 * Revision 1.5  2003/11/20 14:38:10  kapustin
 * Add -nopolya flag to suppress Poly(A) detection.
 *
 * Revision 1.4  2003/11/10 19:20:26  kapustin
 * Generate encoded alignment transcript by default
 *
 * Revision 1.3  2003/11/05 20:32:11  kapustin
 * Include source information into the index
 *
 * Revision 1.2  2003/10/31 19:43:15  kapustin
 * Format and compatibility update
 *
 * Revision 1.1  2003/10/30 19:37:20  kapustin
 * Initial toolkit revision
 *
 * ===========================================================================
 */
