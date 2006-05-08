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
#include <corelib/ncbi_system.hpp>

#include <serial/objostrasn.hpp>
#include <serial/serial.hpp>

#include <algo/align/nw/nw_spliced_aligner16.hpp>
#include <algo/align/nw/nw_spliced_aligner32.hpp>
#include <algo/align/util/hit_comparator.hpp>
#include <algo/blast/api/seqsrc_seqdb.hpp>

#include <objmgr/object_manager.hpp>
#include <objmgr/seq_vector.hpp>

#include <objects/seq/Bioseq.hpp>
#include <objects/seqloc/Seq_loc.hpp>

#include <objtools/readers/fasta.hpp>
#include <objtools/lds/lds_admin.hpp>
#include <objtools/data_loaders/lds/lds_dataloader.hpp>
#include <objtools/data_loaders/blastdb/bdbloader.hpp>

#include <algorithm>
#include <memory>


BEGIN_NCBI_SCOPE

static const char kQuality_high[] = "high";
static const char kQuality_low[] = "low";

static const char kStrandPlus[] = "plus";
static const char kStrandMinus[] = "minus";
static const char kStrandBoth[] = "both";
static const char kStrandAuto[] = "auto";

void CSplignApp::Init()
{
    HideStdArgs( fHideLogfile | fHideConffile | fHideVersion);

    SetVersion(CVersionInfo(1, 20, 0, "Splign"));  
    auto_ptr<CArgDescriptions> argdescr(new CArgDescriptions);

    string program_name ("Splign v.1.20");

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
         "[Batch mode] "
         "Make LDS DB under the specified directory "
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
    
    argdescr->AddDefaultKey
        ("W", "mbwordsize", "[Pairwise mode] Megablast word size",
         CArgDescriptions::eInteger,
         "28");

#ifdef GENOME_PIPELINE

    argdescr->AddOptionalKey
        ("querydb", "querydb",
         "[Batch mode - no external hits] "
         "Pathname to the blast database of query (cDNA) "
         "sequences. To create one, use formatdb -o T -p F",
         CArgDescriptions::eString);

    argdescr->AddOptionalKey
        ("subjdb", "subjdb",
         "[Incremental mode - no external hits] "
         "Pathname to the blast database of subject "
         "sequences. To create one, use formatdb -o T -p F",
         CArgDescriptions::eString);
    
    argdescr->AddOptionalKey(
        "chunk", "chunk",
        "[Batch mode or incremental mode - no external hits] "
        "Slice of the blast database to work with. "
        "Must be specified as N:M where M is the total number "
        "of chunks and N is the current chunk number (one-based). "
        "Use this parameter when your blast database is large "
        "(such as all human ESTs) and you want to split your "
        "jobs into smaller chunks.",
         CArgDescriptions::eString);
#endif
    
    argdescr->AddDefaultKey
        ("log", "log", "Splign log file",
         CArgDescriptions::eOutputFile,
         "splign.log");
    
    argdescr->AddOptionalKey
        ("asn", "asn", "ASN.1 output file name", 
         CArgDescriptions::eOutputFile);

    argdescr->AddOptionalKey
        ("aln", "aln", "Pairwise alignment output file name", 
         CArgDescriptions::eOutputFile);
    
    argdescr->AddFlag("cross", "Cross-species mode");

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
         NStr::DoubleToString(CSplign::s_GetDefaultCompartmentPenalty()));
    
    argdescr->AddDefaultKey
        ("min_compartment_idty", "min_compartment_identity",
         "Minimal compartment identity to align.",
         CArgDescriptions::eDouble,
         NStr::DoubleToString(CSplign::s_GetDefaultMinCompartmentIdty()));
    
    argdescr->AddOptionalKey
        ("min_singleton_idty", "min_singleton_identity",
         "Minimal singleton compartment identity to align. Singletons are "
         "per subject and strand",
         CArgDescriptions::eDouble);
    
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
         CArgDescriptions::eDouble,
         NStr::DoubleToString(CSplign::s_GetDefaultMinExonIdty()));
    
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


CSplign::THitRef CSplignApp::s_ReadBlastHit(const string& m8)
{
    THitRef rv (new CBlastTabular(m8.c_str()));

#ifdef SPLIGNAPP_UNDECORATED_ARE_LOCALS
    // make seq-id local if no type specified in the original m8
    string::const_iterator ie = m8.end(), i0 = m8.begin(), i1 = i0;
    while(i1 != ie && *i1 !='\t') ++i1;
    if(i1 != ie) {
        string::const_iterator i2 = ++i1;
        while(i2 != ie && *i2 !='\t') ++i2;
        if(i2 != ie) {
            if(find(i0, i1, '|') == i1) {
                const string strid = rv->GetQueryId()->GetSeqIdString(true);
                CRef<CSeq_id> seqid (new CSeq_id(CSeq_id::e_Local, strid));
                rv->SetQueryId(seqid);
            }
            if(find(i1, i2, '|') == i2) {
                const string strid = rv->GetSubjId()->GetSeqIdString(true);
                CRef<CSeq_id> seqid (new CSeq_id(CSeq_id::e_Local, strid));
                rv->SetSubjId(seqid);
            }
            return rv;
        }
    }
    const string errmsg = string("Incorrectly formatted blast hit:\n") + m8;
    NCBI_THROW(CSplignAppException, eBadData, errmsg);
#else
    return rv;
#endif
}


bool CSplignApp::x_GetNextPair(istream& ifs, THitRefs* hitrefs)
{
    hitrefs->resize(0);

    if(!m_PendingHits.size() && !ifs ) {
        return false;
    }
    
    if(!m_PendingHits.size()) {

        CSplign::THit::TId query, subj;

        if(m_firstline.size()) {

            THitRef hitref (s_ReadBlastHit(m_firstline));
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
            
            THitRef hit (s_ReadBlastHit(p));
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

        CSplign::THit::TId query = m_PendingHits[0]->GetQueryId();
        CSplign::THit::TId subj  = m_PendingHits[0]->GetSubjId();
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


bool CSplignApp::x_GetNextPair(const THitRefs& hitrefs, THitRefs* hitrefs_pair)
{
    USING_SCOPE(objects);

    hitrefs_pair->resize(0);

    const size_t dim = hitrefs.size();
    if(dim == 0) {
        return false;
    }

    if(m_CurHitRef == dim) {
        m_CurHitRef = numeric_limits<size_t>::max();
        return false;
    }

    if(m_CurHitRef == numeric_limits<size_t>::max()) {
        m_CurHitRef = 0;
    }

    CConstRef<CSeq_id> query (hitrefs[m_CurHitRef]->GetQueryId());
    CConstRef<CSeq_id> subj  (hitrefs[m_CurHitRef]->GetSubjId());
    while(m_CurHitRef < dim 
          && hitrefs[m_CurHitRef]->GetQueryId()->Match(*query)
          && hitrefs[m_CurHitRef]->GetSubjId()->Match(*subj)  ) 
    {
        hitrefs_pair->push_back(hitrefs[m_CurHitRef++]);
    }
    return true;
}


void CSplignApp::x_LogStatus(size_t model_id, 
                             bool query_strand,
                             const CSplign::THit::TId& query, 
                             const CSplign::THit::TId& subj, 
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


CRef<blast::CBlastOptionsHandle> 
CSplignApp::x_SetupBlastOptions(bool cross)
{
    USING_SCOPE(blast);

    m_BlastProgram = cross? eDiscMegablast: eMegablast;

    CRef<CBlastOptionsHandle> blast_options_handle
        (CBlastOptionsFactory::Create(m_BlastProgram));

    blast_options_handle->SetDefaults();

    CBlastOptions& blast_opt = blast_options_handle->SetOptions();

    if(!cross) {

        const CArgs& args = GetArgs();
        blast_opt.SetWordSize(args["W"].AsInteger());
        blast_opt.SetMaskAtHash(true);
        blast_opt.SetGapXDropoff(1);
        blast_opt.SetGapXDropoffFinal(1);
    }

    if(blast_options_handle->Validate() == false) {
        NCBI_THROW(CSplignAppException,
                   eInternal,
                   "Incorrect blast setup");
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


void CSplignApp::x_GetDbBlastHits(CSeqDB& seqdb,
                                  blast::TSeqLocVector& queries,
                                  THitRefs* phitrefs,
                                  size_t chunk, size_t total_chunks)
{
    USING_SCOPE(objects);
    USING_SCOPE(blast);

    // run blast and print ASN output
    CBlastSeqSrc seq_src(SeqDbBlastSeqSrcInit(&seqdb));
    CDbBlast blast (queries, seq_src, *m_BlastOptionsHandle);
    TSeqAlignVector sav = blast.Run();
    phitrefs->resize(0);
    ITERATE(TSeqAlignVector, ii, sav) {
        if((*ii)->IsSet()) {
            const CSeq_align_set::Tdata &sas0 = (*ii)->Get();
            ITERATE(CSeq_align_set::Tdata, sa_iter0, sas0) {
                const CSeq_align_set::Tdata &sas = 
                    (*sa_iter0)->GetSegs().GetDisc().Get();
                ITERATE(CSeq_align_set::Tdata, sa_iter, sas) {

                    THitRef hitref (new CBlastTabular(**sa_iter, false));
                    phitrefs->push_back(hitref);

                    CSplign::THit::TId id (hitref->GetSubjId());
                    int oid = -1;
                    seqdb.SeqidToOid(*id, oid);
                    id = seqdb.GetSeqIDs(oid).back();
                    hitref->SetSubjId(id);
                }
            }
        }
    }
}


void CSplignApp::x_DoIncremental(void)
{
    USING_SCOPE(objects);
    USING_SCOPE(blast);

    const CArgs& args = GetArgs();
    const string dbname = args["subjdb"].AsString();

    CRef<CObjectManager> objmgr = CObjectManager::GetInstance();
    CBlastDbDataLoader::RegisterInObjectManager(
        *objmgr, dbname, CBlastDbDataLoader::eNucleotide, 
        CObjectManager::eDefault);

    CNcbiIstream& ifa = args["query"].AsInputFile();
    for(CRef<CSeq_entry> se (ReadFasta(ifa, fReadFasta_OneSeq));
        se.NotEmpty(); se = ReadFasta(ifa, fReadFasta_OneSeq)) 
    {
        CRef<CScope> scope (new CScope(*objmgr));
        scope->AddDefaults();
        scope->AddTopLevelSeqEntry(*se);

        m_Splign->SetScope() = scope;

        const CSeq_entry::TSeq& bioseq = se->GetSeq();    
        const CSeq_entry::TSeq::TId& qids = bioseq.GetId();
        CRef<CSeq_id> seqid_query (qids.back());

        TSeqLocVector queries;
        CRef<CSeq_loc> sl (new CSeq_loc);
        sl->SetWhole().Assign(*seqid_query);
        queries.push_back(SSeqLoc(*sl, *scope));
            
        THitRefs hitrefs;
        CSeqDB seqdb(dbname, CSeqDB::eNucleotide);
        x_GetDbBlastHits(seqdb, queries, &hitrefs, 0, 0);
        typedef CHitComparator<CBlastTabular> THitComparator;
        THitComparator hc (THitComparator::eSubjIdQueryId);
        stable_sort(hitrefs.begin(), hitrefs.end(), hc);

        THitRefs hitrefs_pair;
        m_CurHitRef = numeric_limits<size_t>::max();
        while(x_GetNextPair(hitrefs, &hitrefs_pair)) {

            x_ProcessPair(hitrefs_pair, args); 
        }

        if(ifa.eof()) { break; }
    }
}


void CSplignApp::x_DoBatch2(void)
{
    USING_SCOPE(objects);
    USING_SCOPE(blast);
    
    const CArgs& args = GetArgs();
    const string dbname = args["querydb"].AsString();
    const size_t W = args["W"].AsInteger();
    
    CRef<CObjectManager> objmgr = CObjectManager::GetInstance();
    CBlastDbDataLoader::RegisterInObjectManager(
        *objmgr, dbname, CBlastDbDataLoader::eNucleotide, 
        CObjectManager::eDefault);

    CNcbiIstream& ifa = args["subj"].AsInputFile();

    for(CRef<CSeq_entry> se (ReadFasta(ifa, fReadFasta_OneSeq));
        se.NotEmpty(); se = ReadFasta(ifa, fReadFasta_OneSeq)) 
    {
        CRef<CScope> scope (new CScope(*objmgr));
        scope->AddDefaults();
        scope->AddTopLevelSeqEntry(*se);
        m_Splign->SetScope() = scope;

        const CSeq_entry::TSeq& bioseq = se->GetSeq();    
        const CSeq_entry::TSeq::TId& sids = bioseq.GetId();
        CRef<CSeq_id> seqid_dna (sids.back());
        if(bioseq.IsNa() == false) {
            string errmsg;
            errmsg = "Subject sequence not nucleotide: " 
                + seqid_dna->GetSeqIdString(true);
            NCBI_THROW(CSplignAppException, eBadData, errmsg);
        }

        TSeqPos offset = 0, len = bioseq.GetInst().GetLength();
        const TSeqPos step1m = 1024*1024;
        const TSeqPos step500k = 500*1024;

        THitRefs pending;
        while(offset < len || pending.size() > 0) {

            TSeqPos from = offset, to = offset + step500k;

            THitRefs hitrefs;
            if(from < len) {

                if(to >= len) {
                    to = len - 1;
                }

                CRef<CSeq_loc> sl (new CSeq_loc (*seqid_dna, from, to));
                TSeqLocVector queries;
                queries.push_back(SSeqLoc(*sl, *scope));

                CSeqDB seqdb(dbname, CSeqDB::eNucleotide);
                x_GetDbBlastHits(seqdb, queries, &hitrefs, 0, 0);
                NON_CONST_ITERATE(THitRefs, ii, hitrefs) {
                
                    THitRef h = *ii;
                    h->SwapQS();
                    if(h->GetQueryStrand() == false) {
                        h->FlipStrands();
                    }
                }
                copy(pending.begin(), pending.end(), back_inserter(hitrefs));
                pending.resize(0);
                
                set<string> pending_ids;
                NON_CONST_ITERATE(THitRefs, ii, hitrefs) {
                    
                    const string id = (*ii)->GetQueryId()->AsFastaString();
                    bool bp = pending_ids.find(id) != pending_ids.end();
                    if(bp || (*ii)->GetSubjMin() + step1m > to) {
                        pending.push_back(*ii);
                        ii->Reset(NULL);
                        if(!bp) {
                            pending_ids.insert(id);
                        }
                    }
                }

                NON_CONST_ITERATE(THitRefs, ii, hitrefs) {
                    if(ii->NotNull()) {
                        const string id = (*ii)->GetQueryId()->AsFastaString();
                        if(pending_ids.find(id) != pending_ids.end()) {
                            pending.push_back(*ii);
                            ii->Reset(NULL);
                        }
                    }
                }

                size_t i = 0, n = hitrefs.size();
                for(size_t j = 0; j < n; ++j) {
                    if(hitrefs[j].NotNull()) {
                        if(i < j) {
                            hitrefs[i++] = hitrefs[j];
                        }
                        else {
                            ++i;
                        }
                    }
                }
                if(i < n) {
                    hitrefs.erase(hitrefs.begin() + i, hitrefs.end());
                }

            }
            else {
                hitrefs = pending;
                pending.resize(0);
            }

            typedef CHitComparator<CBlastTabular> THitComparator;
            THitComparator hc (THitComparator::eQueryId);
            stable_sort(hitrefs.begin(), hitrefs.end(), hc);            

            THitRefs hitrefs_pair;
            m_CurHitRef = numeric_limits<size_t>::max();
            CSeqDB seqdb(dbname, CSeqDB::eNucleotide);
            while(x_GetNextPair(hitrefs, &hitrefs_pair)) {
                
                x_ProcessPair(hitrefs_pair, args); 
            }

            offset = (to + 1 >= len)? len: (to - 2*W);
        }

        if(ifa.eof()) { break; }
    }
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
    const bool is_subjdb = args["subjdb"];

#else
    const bool is_querydb = false;
    const bool is_subjdb = false;
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

#ifdef GENOME_PIPELINE
    if(is_mkidx) {

        // create mer index and exit
        const Uint4 max_vol_mem = args["M"].AsInteger()*1024*1024;
        const string filename_base (args["mkidx"].AsString());
        CSeqDB seqdb (args["subjdb"].AsString(), CSeqDB::eNucleotide);

        size_t q = 0;
        for(CSeqDBIter db_iter (seqdb.Begin()); db_iter; ++q) 
        {
            cout << "Indexing vol " << q << " ... " << flush;
            CMerMatcherIndex mmidx;
            size_t rv = mmidx.Create(db_iter, max_vol_mem, 16);
            cout << "Done (" << rv << ") entries" << endl << flush;
            const string filename = args["mkidx"].AsString() + ".v" +
                NStr::IntToString(q) + ".idx";
            CNcbiOfstream ofstr (filename.c_str(), IOS_BASE::binary);
            mmidx.Dump(ofstr);
        }

        return 0;
    }
#endif

    // determine mode and verify arguments
    ERunMode run_mode (eNotSet);
    
    if(is_query && is_subj && !(is_hits || is_querydb || is_ldsdir)) {
        run_mode = ePairwise;
    }
    else if(is_subj && is_querydb
            && !(is_query || is_hits || is_ldsdir)) 
    {
        run_mode = eBatch2;
    }
    else if(is_query && is_subjdb && !(is_subj || is_hits || is_ldsdir)) {
        run_mode = eIncremental;
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
    
    // open paiwise alignment output stream, if any
    m_AlnOut = args["aln"]? & args["aln"].AsOutputFile(): NULL;
    
    // in pairwise, batch 2 or incremental mode, setup blast options
    if(run_mode != eBatch1) {
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
    m_Splign->SetMinExonIdentity(args["min_exon_idty"].AsDouble());

    m_Splign->SetCompartmentPenalty(args["compartment_penalty"].AsDouble());
    m_Splign->SetMinCompartmentIdentity(args["min_compartment_idty"].AsDouble());
    if(args["min_singleton_idty"]) {
        m_Splign->SetMinSingletonIdentity(args["min_singleton_idty"].AsDouble());
    }
    else {
        m_Splign->SetMinSingletonIdentity(m_Splign->GetMinCompartmentIdentity());
    }

    m_Splign->SetEndGapDetection(!(args["noendgaps"]));
    m_Splign->SetMaxGenomicExtent(args["max_extent"].AsInteger());
    m_Splign->SetAligner() = aligner;
    m_Splign->SetStartModelId(1);

    // splign formatter object    
    m_Formatter.Reset(new CSplignFormatter(*m_Splign));

    // do mode-specific preparations
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
        CLDS_DataLoader::RegisterInObjectManager(
            *objmgr, *ldsdb, CObjectManager::eDefault);
        scope.Reset (new CScope(*objmgr));
        scope->AddDefaults();
    }
    else if(run_mode == eIncremental) {
        x_DoIncremental();
    }
    else if(run_mode == eBatch2) {
        x_DoBatch2();
    }
    else {
        NCBI_THROW(CSplignAppException,
                   eGeneral,
                   "Requested mode not implemented." );
    }

    m_Splign->SetScope() = scope;

    // run splign in selected mode 
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
    else if (run_mode == eIncremental || run_mode == eBatch2) {
        // done at the preparation step
    }
    else {
        NCBI_THROW(CSplignAppException,
                   eInternal,
                   "Mode not implemented");
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
    /*
    ITERATE(THitRefs, ii, hitrefs) {
        cerr << (**ii) << endl;
    }
    */

    const CSplignFormatter::EFlags flags =
#ifdef GENOME_PIPELINE
        CSplignFormatter::fNone;
#else
        CSplignFormatter::fNoExonScores;
#endif

    if(hitrefs.size() == 0) {
        return;
    }

    CSplign::THit::TId query = hitrefs.front()->GetQueryId();
    CSplign::THit::TId subj  = hitrefs.front()->GetSubjId();
    
    m_Formatter->SetSeqIds(query, subj);
    
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
            copy(results.begin(), results.end(), back_inserter(splign_results));
            mid_plus = m_Splign->GetNextModelId();
        }}
        {{
            m_Splign->SetStrand(false);
            m_Splign->SetStartModelId(mid);
            m_Splign->Run(&hits0);
            const CSplign::TResults& results = m_Splign->GetResult();
            copy(results.begin(), results.end(), back_inserter(splign_results));
            mid_minus = m_Splign->GetNextModelId();
        }}
        mid = max(mid_plus, mid_minus);
    }
    else {
        NCBI_THROW(CSplignAppException, eGeneral, 
                   "Auto strand not yet implemented");
    }
    
    cout << m_Formatter->AsExonTable(&splign_results, flags);
        
    if(m_AsnOut) {
        
        *m_AsnOut << MSerial_AsnText 
                  << *(m_Formatter->AsSeqAlignSet(&splign_results))
                  << endl;
    }
    
    if(m_AlnOut) {
        
        *m_AlnOut << m_Formatter->AsAlignmentText(m_Splign->GetScope(),
                                                  &splign_results);
    }
        
    ITERATE(CSplign::TResults, ii, splign_results) {
        x_LogStatus(ii->m_id, ii->m_QueryStrand, query, subj,
                    ii->m_error, ii->m_msg);
    }
    
    if(splign_results.size() == 0) {
        //x_LogStatus(0, true, query, subj, false, "No compartment found");
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

    // pre-scan for mkidx0
    for(int i = 1; i < argc; ++i) {
        if(0 == strcmp(argv[i], "-mkidx0")) {
            
            if(i + 1 == argc) {
                char err_msg [] = 
                    "ERROR: No FastA files specified to index. "
                    "Please specify one or more FastA files after -mkidx0. "
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
 * Revision 1.66  2006/05/08 15:19:02  kapustin
 * Code and file cleanup
 *
 * Revision 1.65  2006/03/31 19:11:20  kapustin
 * Refine common argument set
 *
 * Revision 1.64  2006/03/28 16:17:17  kapustin
 * Specify min signleton idty as an optional rather than default argument
 *
 * Revision 1.63  2006/03/21 16:20:50  kapustin
 * Various changes, mainly adjust the code with  other libs
 *
 * Revision 1.62  2006/03/17 15:58:16  kapustin
 * Fix megablast word size option
 *
 * Revision 1.61  2006/03/15 15:15:54  ucko
 * +<algorithm> for sort() et al.; -<iostream>, already included via ncbistd.hpp.
 *
 * Revision 1.60  2006/02/14 02:21:35  ucko
 * Use our IOS_BASE macro rather than ios_base for compatibility with GCC 2.95.
 *
 * Revision 1.59  2006/02/13 20:03:35  kapustin
 * Intermediate update
 *
 * Revision 1.58  2006/02/13 19:31:54  kapustin
 * Do not pre-load mRNA
 *
 * Revision 1.57  2006/01/04 13:28:00  kapustin
 * Fix typo in x_GetNextPair
 *
 * Revision 1.56  2005/12/07 15:51:34  kapustin
 * +CSplignApp::s_ReadBlastHit()
 *
 * Revision 1.55  2005/11/21 16:06:38  kapustin
 * Move gpipe-sensitive items to the app level
 *
 * Revision 1.54  2005/11/21 14:16:10  kapustin
 * Fix ASN output stream typo
 *
 * Revision 1.53  2005/10/31 16:29:58  kapustin
 * Support traditional pairwise alignment text output
 *
 * Revision 1.52  2005/10/24 17:44:06  kapustin
 * Intermediate update
 *
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
