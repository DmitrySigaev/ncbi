#ifndef SKIP_DOXYGEN_PROCESSING
static char const rcsid[] = "$Id$";
#endif /* SKIP_DOXYGEN_PROCESSING */

/*
* ===========================================================================
*
*                            PUBLIC DOMAIN NOTICE
*               National Center for Biotechnology Information
*
*  This software/database is a "United States Government Work" under the
*  terms of the United States Copyright Act.  It was written as part of
*  the author's offical duties as a United States Government employee and
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
* ===========================================================================*/

/*****************************************************************************

File name: blast_app.cpp

Author: Ilya Dondoshansky

Contents: C++ driver for running BLAST

******************************************************************************/

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbifile.hpp>
#include <corelib/metareg.hpp>

#include <objects/seq/seq__.hpp>
#include <objects/seq/seqport_util.hpp>
#include <objects/seqfeat/Genetic_code_table.hpp>
#include <objmgr/object_manager.hpp>
#include <objmgr/util/sequence.hpp>

#include <algo/blast/api/blast_options.hpp>
#include <algo/blast/api/blast_nucl_options.hpp>
#include <algo/blast/api/disc_nucl_options.hpp>
#include <algo/blast/api/db_blast.hpp>
#include <algo/blast/api/blast_aux.hpp>
#include <algo/blast/api/hspstream_queue.hpp>
#include <algo/blast/api/seqinfosrc_seqdb.hpp>

#ifndef NCBI_C_TOOLKIT
#define NCBI_C_TOOLKIT
#endif
#include <objtools/data_loaders/blastdb/bdbloader.hpp>

#include <algo/blast/api/seqsrc_seqdb.hpp>

#include "blast_input.hpp" // From working directory
#include <objtools/alnmgr/util/blast_format.hpp>

// C include files

#include <algo/blast/core/blast_setup.h>
#include <algo/blast/core/blast_util.h>
#include <algo/blast/core/lookup_wrap.h>
#include <algo/blast/core/blast_engine.h>

// For on-the-fly tabular output
#include "blast_tabular.hpp"

// For repeats filtering
#include <algo/blast/api/repeats_filter.hpp>

USING_NCBI_SCOPE;
USING_SCOPE(blast);
USING_SCOPE(objects);

class CBlastApplication : public CNcbiApplication
{
private:
    virtual void Init(void);
    virtual int Run(void);
    virtual void Exit(void);
    void InitScope(void);
    void InitOptions(void);
    void SetOptions(const CArgs& args);
    void ProcessCommandLineArgs(CBlastOptionsHandle* opt, 
                                BlastSeqSrc* seq_src);
    int BlastSearch(void);
    void FormatResults(const CDbBlast* blaster, TSeqAlignVector& seqalignv);
    /// NB: The object manager member field must be put in front of the scope
    /// member field, to guarantee the correct order of destruction.
    CRef<CObjectManager> m_ObjMgr;
    CRef<CScope>         m_Scope;
};

void CBlastApplication::Init(void)
{
    HideStdArgs(fHideLogfile | fHideConffile | fHideVersion);
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(), "basic local alignment search tool");

    arg_desc->AddKey("program", "program", "Type of BLAST program",
        CArgDescriptions::eString);
    arg_desc->SetConstraint
        ("program", &(*new CArgAllow_Strings, 
                "blastp", "blastn", "blastx", "tblastn", "tblastx", 
                "rpsblast", "rpstblastn"));
    arg_desc->AddDefaultKey("query", "query", "Query file name",
                     CArgDescriptions::eInputFile, "stdin");
    arg_desc->AddKey("db", "database", "BLAST database name",
                     CArgDescriptions::eString);
    arg_desc->AddDefaultKey("strand", "strand", 
        "Query strands to search: 1 forward, 2 reverse, 0,3 both",
        CArgDescriptions::eInteger, "0");
    arg_desc->SetConstraint("strand", new CArgAllow_Integers(0,3));

    arg_desc->AddDefaultKey("filter", "filter", "Filtering option",
                            CArgDescriptions::eString, "T");
    arg_desc->AddDefaultKey("lcase", "lcase", "Should lower case be masked?",
                            CArgDescriptions::eBoolean, "F");
    arg_desc->AddDefaultKey("lookup", "lookup", 
        "Type of lookup table: 0 default, 1 megablast",
        CArgDescriptions::eInteger, "0");
    arg_desc->AddDefaultKey("matrix", "matrix", "Scoring matrix name",
                            CArgDescriptions::eString, "BLOSUM62");
    arg_desc->AddDefaultKey("mismatch", "penalty", "Penalty score for a mismatch",
                            CArgDescriptions::eInteger, "0");
    arg_desc->AddDefaultKey("match", "reward", "Reward score for a match",
                            CArgDescriptions::eInteger, "0");
    arg_desc->AddDefaultKey("word", "wordsize", "Word size",
                            CArgDescriptions::eInteger, "0");
    arg_desc->AddDefaultKey("templen", "templen", 
        "Discontiguous word template length",
        CArgDescriptions::eInteger, "0");
    arg_desc->SetConstraint("templen", 
                            &(*new CArgAllow_Strings, "0", "16", "18", "21"));

    arg_desc->AddDefaultKey("templtype", "templtype", 
        "Discontiguous word template type",
        CArgDescriptions::eInteger, "0");
    arg_desc->SetConstraint("templtype", new CArgAllow_Integers(0,2));

    arg_desc->AddDefaultKey("thresh", "threshold", 
        "Score threshold for neighboring words",
        CArgDescriptions::eInteger, "0");
    arg_desc->AddDefaultKey("window","window", "Window size for two-hit extension",
                            CArgDescriptions::eInteger, "0");
    arg_desc->AddDefaultKey("scantype", "scantype", 
        "Method for checking initial words length:\n"
        "0 default depending on other options values,\n"
        "1 AG - extension in both directions,\n"
        "2 traditional - extension to the right,\n"
        "3 Update of word length on diagonal entries.",
        CArgDescriptions::eInteger, "0");
    arg_desc->SetConstraint("scantype", new CArgAllow_Integers(0,3));

    arg_desc->AddDefaultKey("stack", "stack",
        "Use stacks instead of diagonal array for initial hits information",
        CArgDescriptions::eBoolean, "F");

    arg_desc->AddDefaultKey("varword", "varword", 
        "Should variable word size be used, i.e. no partial byte extensions"
        "when checking initial word length?",
        CArgDescriptions::eBoolean, "F");
    arg_desc->AddDefaultKey("stride","stride", "Database scanning stride",
                            CArgDescriptions::eInteger, "0");
    arg_desc->AddDefaultKey("xungap", "xungapped", 
        "X-dropoff value for ungapped extensions",
        CArgDescriptions::eDouble, "0");
    arg_desc->AddDefaultKey("ungapped", "ungapped", 
        "Perform only an ungapped alignment search?",
        CArgDescriptions::eBoolean, "F");
    arg_desc->AddDefaultKey("greedy", "greedy", 
        "Use greedy algorithm for gapped extensions:\n"
        "0 default, -1 no, 1 one-step, 2 two-step, 3 two-step with ungapped",
        CArgDescriptions::eInteger, "0");
    arg_desc->AddDefaultKey("gopen", "gapopen", "Penalty for opening a gap",
                            CArgDescriptions::eInteger, "0");
    arg_desc->AddDefaultKey("gext", "gapext", "Penalty for extending a gap",
                            CArgDescriptions::eInteger, "0");
    arg_desc->AddDefaultKey("xgap", "xdrop", 
        "X-dropoff value for preliminary gapped extensions",
        CArgDescriptions::eDouble, "0");
    arg_desc->AddDefaultKey("xfinal", "xfinal", 
        "X-dropoff value for final gapped extensions with traceback",
        CArgDescriptions::eDouble, "0");
    arg_desc->AddDefaultKey("evalue", "evalue", 
        "E-value threshold for saving hits",
        CArgDescriptions::eDouble, "0");
    arg_desc->AddDefaultKey("searchsp", "searchsp", 
        "Virtual search space to be used for statistical calculations",
        CArgDescriptions::eDouble, "0");
    arg_desc->AddDefaultKey("perc", "percident", 
        "Percentage of identities cutoff for saving hits",
        CArgDescriptions::eDouble, "0");
    arg_desc->AddDefaultKey("hitlist", "hitlist_size",
        "How many best matching sequences to find?",
        CArgDescriptions::eInteger, "500");
    arg_desc->AddDefaultKey("descr", "descriptions",
        "How many matching sequence descriptions to show?",
        CArgDescriptions::eInteger, "500");
    arg_desc->AddDefaultKey("align", "alignments", 
        "How many matching sequence alignments to show?",
        CArgDescriptions::eInteger, "250");
    arg_desc->AddDefaultKey("out", "outfile", 
        "File name for writing output",
        CArgDescriptions::eOutputFile, "-", CArgDescriptions::fPreOpen);
    arg_desc->AddDefaultKey("format", "format", 
        "How to format the results?",
        CArgDescriptions::eInteger, "0");
    arg_desc->AddDefaultKey("html", "html", "Produce HTML output?",
                            CArgDescriptions::eBoolean, "F");
    arg_desc->AddDefaultKey("gencode", "gencode", "Query genetic code",
                            CArgDescriptions::eInteger, "1");
    arg_desc->AddDefaultKey("dbgencode", "dbgencode", "Database genetic code",
                            CArgDescriptions::eInteger, "1");
    arg_desc->AddDefaultKey("maxintron", "maxintron", 
                            "Longest allowed intron length for linking HSPs",
                            CArgDescriptions::eInteger, "0");
    arg_desc->AddDefaultKey("frameshift", "frameshift",
                            "Frame shift penalty (blastx only)",
                            CArgDescriptions::eInteger, "0");
    arg_desc->AddOptionalKey("asnout", "seqalignasn", 
        "File name for writing the seqalign results in ASN.1 form",
        CArgDescriptions::eOutputFile);

    arg_desc->AddDefaultKey("qstart", "query_start",
                            "Starting offset in query location",
                            CArgDescriptions::eInteger, "0");
    arg_desc->AddDefaultKey("qend", "query_end",
                            "Ending offset in query location",
                            CArgDescriptions::eInteger, "0");

    arg_desc->AddOptionalKey("pattern", "phipattern",
                            "Pattern for PHI-BLAST",
                             CArgDescriptions::eString);

    arg_desc->AddOptionalKey("pssm", "pssm", 
        "File name for uploading a PSSM for PSI BLAST seach",
        CArgDescriptions::eInputFile);

    arg_desc->AddOptionalKey("dbrange", "databaserange",
                            "Range of ordinal ids in the BLAST database.\n"
                             "Format: \"oid1 oid2\"",
                             CArgDescriptions::eString);

    arg_desc->AddDefaultKey("tabular", "tabular", 
                             "On the fly tabular output", 
                             CArgDescriptions::eBoolean, "F");

    arg_desc->AddDefaultKey("threads", "num_threads",
                            "Number of threads to use in preliminary stage"
                            " of the search",
                            CArgDescriptions::eInteger, "1");

    SetupArgDescriptions(arg_desc.release());
}

void 
CBlastApplication::InitScope(void)
{
    if (m_Scope.Empty()) {
        m_ObjMgr = CObjectManager::GetInstance();
        if (!m_ObjMgr) {
            throw std::runtime_error("Could not initialize object manager");
        }
        m_Scope.Reset(new CScope(*m_ObjMgr));
        m_Scope->AddDefaults();
        _TRACE("BlastApp: Initializing scope");
    }
}

void
CBlastApplication::ProcessCommandLineArgs(CBlastOptionsHandle* opts_handle, 
                                          BlastSeqSrc* seq_src)
{
    CArgs args = GetArgs();
    CBlastOptions& opt = opts_handle->SetOptions();

    if (args["strand"].AsInteger()) {
        switch (args["strand"].AsInteger()) {
        case 1: opt.SetStrandOption(eNa_strand_plus); break;
        case 2: opt.SetStrandOption(eNa_strand_minus); break;
        case 3: opt.SetStrandOption(eNa_strand_both); break;
        default: abort();
        }
    }

    opt.SetFilterString(args["filter"].AsString().c_str());

    // If lookup table type argument value is 0, the type will be set correctly
    // automatically. Value 1 corresponds to megablast lookup table;
    switch (args["lookup"].AsInteger()) {
    case 1:
        opt.SetLookupTableType(MB_LOOKUP_TABLE);
        break;
    default:
        break;
    }

    if (args["matrix"]) {
        opt.SetMatrixName(args["matrix"].AsString().c_str());
    }
    if (args["gopen"].AsInteger() || args["greedy"].AsInteger() > 0) {
        opt.SetGapOpeningCost(args["gopen"].AsInteger());
    }
    if (args["gext"].AsInteger() || args["greedy"].AsInteger() > 0) {
        opt.SetGapExtensionCost(args["gext"].AsInteger());
    }

    if (args["mismatch"].AsInteger()) {
        opt.SetMismatchPenalty(args["mismatch"].AsInteger());
    }
    if (args["match"].AsInteger()) {
        opt.SetMatchReward(args["match"].AsInteger());
    }
    if (args["thresh"].AsInteger()) {
        opt.SetWordThreshold(args["thresh"].AsInteger());
    }
    if (args["window"].AsInteger()) {
        opt.SetWindowSize(args["window"].AsInteger());
    }

    // The next 3 apply to nucleotide searches only
    string program = args["program"].AsString();
    if (args["word"].AsInteger()) {
        if (program == "blastn") {
            // Setting word size for blastn involves changing the scanning 
            // stride as well, which is handled in the derived 
            // CBlastNucleotideOptionsHandle class, but not in the base
            // CBlastOptionsHandle class.
            CBlastNucleotideOptionsHandle* nucl_handle = 
                dynamic_cast<CBlastNucleotideOptionsHandle*>(opts_handle);
            nucl_handle->SetWordSize(args["word"].AsInteger());
        } else {
            opt.SetWordSize(args["word"].AsInteger());
        }
    }

    if (program == "blastn") {
        if (args["templen"].AsInteger()) {
            // Setting template length involves changing the scanning 
            // stride as well, which is handled in the derived 
            // CDiscNucleotideOptionsHandle class, but not in the base
            // CBlastOptionsHandle class.
            CDiscNucleotideOptionsHandle* disc_nucl_handle = 
                dynamic_cast<CDiscNucleotideOptionsHandle*>(opts_handle);
        
            disc_nucl_handle->SetTemplateLength(args["templen"].AsInteger());
        }
        if (args["templtype"].AsInteger()) {
            opt.SetMBTemplateType(args["templtype"].AsInteger());
        }
        // Setting seed extension method involves changing the scanning 
        // stride as well, which is handled in the derived 
        // CBlastNucleotideOptionsHandle class, but not in the base
        // CBlastOptionsHandle class.
        CBlastNucleotideOptionsHandle* nucl_handle = 
            dynamic_cast<CBlastNucleotideOptionsHandle*>(opts_handle);
        switch(args["scantype"].AsInteger()) {
        case 1:
            nucl_handle->SetSeedExtensionMethod(eRightAndLeft);
            break;
        case 2:
            nucl_handle->SetSeedExtensionMethod(eRight);
            break;
        case 3:
            nucl_handle->SetSeedExtensionMethod(eUpdateDiag);
            break;
        default:
            break;
        }
        nucl_handle->SetSeedContainerType(eDiagArray);
        
        if (args["stack"].AsBoolean())
            nucl_handle->SetSeedContainerType(eWordStacks);

        opt.SetVariableWordSize(args["varword"].AsBoolean());

        // Override the scan step value if it is set by user
        if (args["stride"].AsInteger()) {
            opt.SetScanStep(args["stride"].AsInteger());
        }
    }

    if (args["xungap"].AsDouble()) {
        opt.SetXDropoff(args["xungap"].AsDouble());
    }

    if (args["ungapped"].AsBoolean()) {
        opt.SetGappedMode(false);
    }

    switch (args["greedy"].AsInteger()) {
    case 1: /* Immediate greedy gapped extension with traceback */
        opt.SetGapExtnAlgorithm(eGreedyWithTracebackExt);
        opt.SetGapTracebackAlgorithm(eSkipTbck);
        opt.SetUngappedExtension(false);
        break;
    case 2: /* Two-step greedy extension, no ungapped extension */
        opt.SetGapExtnAlgorithm(eGreedyExt);
        opt.SetGapTracebackAlgorithm(eGreedyTbck);
        opt.SetUngappedExtension(false);
        break;
    case 3: /* Two-step greedy extension after ungapped extension*/
        opt.SetGapExtnAlgorithm(eGreedyExt);
        opt.SetGapTracebackAlgorithm(eGreedyTbck);
        opt.SetUngappedExtension(true);
        break;
    case -1: /* Force non-greedy extension */
        opt.SetGapExtnAlgorithm(eDynProgExt);
        opt.SetGapTracebackAlgorithm(eDynProgTbck);
        opt.SetUngappedExtension(true);
        break;
    default: break;
    }


    if (args["xgap"].AsDouble()) {
        opt.SetGapXDropoff(args["xgap"].AsDouble());
    }
    if (args["xfinal"].AsDouble()) {
        opt.SetGapXDropoffFinal(args["xfinal"].AsDouble());
    }

    if (args["evalue"].AsDouble()) {
        opt.SetEvalueThreshold(args["evalue"].AsDouble());
    }

    if (args["searchsp"].AsDouble()) {
        opt.SetEffectiveSearchSpace((Int8) args["searchsp"].AsDouble());
    } else if (seq_src) {
        opt.SetDbLength(BLASTSeqSrcGetTotLen(seq_src));
        opt.SetDbSeqNum(BLASTSeqSrcGetNumSeqs(seq_src));
    }

    if (args["hitlist"].AsInteger()) {
        opt.SetHitlistSize(args["hitlist"].AsInteger());
        /* Hitlist size for preliminary alignments is increased, unless 
           no traceback is performed. */
        if (args["ungapped"].AsBoolean() || args["greedy"].AsInteger() == 1) {
            opt.SetPrelimHitlistSize(args["hitlist"].AsInteger());
        } else {
            opt.SetPrelimHitlistSize(MIN(2*args["hitlist"].AsInteger(),
                                         args["hitlist"].AsInteger() + 50));
        }
    }

    if (args["perc"].AsDouble()) {
        opt.SetPercentIdentity(args["perc"].AsDouble());
    }

    if (args["gencode"].AsInteger()) {
        opt.SetQueryGeneticCode(args["gencode"].AsInteger());
    }
  
    if ((program == "tblastn" || program == "tblastx") &&
        args["dbgencode"].AsInteger() != BLAST_GENETIC_CODE) {
        opt.SetDbGeneticCode(args["dbgencode"].AsInteger());
    }

    if (args["maxintron"].AsInteger()) {
        opt.SetLongestIntronLength(args["maxintron"].AsInteger());
    }

    if (args["frameshift"].AsInteger()) {
        opt.SetFrameShiftPenalty(args["frameshift"].AsInteger());
        opt.SetOutOfFrameMode();
    }

    if (args["pattern"]) {
        opt.SetPHIPattern(args["pattern"].AsString().c_str(),
                                 (program == "blastn"));
    }

    if (args["pssm"]) {
#if 0
        FILE* pssmfp = fopen(args["pssm"].AsInputFile(), "r");
        // Read the pssm string and fill the posMatrix
        bool use_pssm = TRUE;
        fclose(pssmfp);
#endif
    }

    return;
}

static CDisplaySeqalign::TranslatedFrames 
Context2TranslatedFrame(int context)
{
    switch (context) {
    case 1: return CDisplaySeqalign::ePlusStrand1;
    case 2: return CDisplaySeqalign::ePlusStrand2;
    case 3: return CDisplaySeqalign::ePlusStrand3;
    case 4: return CDisplaySeqalign::eMinusStrand1;
    case 5: return CDisplaySeqalign::eMinusStrand2;
    case 6: return CDisplaySeqalign::eMinusStrand3;
    default: return CDisplaySeqalign::eFrameNotSet;
    }
}

static TSeqLocInfoVector
BlastMaskLoc2CSeqLoc(const BlastMaskLoc* mask, const TSeqLocVector& slp,
    EProgram program)
{
    TSeqLocInfoVector retval;
    int frame, num_frames;
    bool translated_query;
    int index;

    translated_query = (program == eBlastx ||
                        program == eTblastx);

    num_frames = (translated_query ? NUM_FRAMES : 1);

    TSeqLocInfo mask_info_list;

    for (index = 0; index < (int)slp.size(); ++index) {
        mask_info_list.clear();

/*   FIXME???
        BlastSeqLoc* loc = mask->seqloc_array[index*num_frames];
        if (!loc) {
            retval.push_back(mask_info_list);
            continue;
        }
*/
        CDisplaySeqalign::SeqlocInfo* seqloc_info =
            new CDisplaySeqalign::SeqlocInfo;

        for (int tmp_index=0; tmp_index<num_frames; tmp_index++)
        {
            frame = (translated_query ? (tmp_index+1) : 0);
            BlastSeqLoc* loc = mask->seqloc_array[index*num_frames+tmp_index];
           
            for ( ; loc; loc = loc->next) {
                seqloc_info->frame = Context2TranslatedFrame(frame);
                CRef<CSeq_loc> seqloc(new CSeq_loc());
                seqloc->SetInt().SetFrom((loc->ssr)->left);
                seqloc->SetInt().SetTo((loc->ssr)->right);
                seqloc->SetInt().SetId(*(const_cast<CSeq_id*>(&sequence::GetId(*
slp[index].seqloc, slp[index].scope))));

                seqloc_info->seqloc = seqloc;
                mask_info_list.push_back(seqloc_info);
            }
        }
        retval.push_back(mask_info_list);
    }

    return retval;
}

void CBlastApplication::FormatResults(const CDbBlast* blaster, 
                                      TSeqAlignVector& seqalignv)
{
    if (seqalignv.size() == 0)
        return;
    
    CArgs args = GetArgs();

    if (args["asnout"]) {
        auto_ptr<CObjectOStream> asnout(
            CObjectOStream::Open(args["asnout"].AsString(), eSerial_AsnText));
        unsigned int query_index;
        for (query_index = 0; query_index < seqalignv.size(); ++query_index)
        {
            if (!seqalignv[query_index]->IsSet())
                continue;
            *asnout << *seqalignv[query_index];
        }
    }

    if (args["out"]) {
        EProgram program = blaster->GetOptions().GetProgram();

        /* Revert RPS program names to their conventional
           counterparts, to avoid confusing the C toolkit
           formatter */

        if (program == eRPSBlast)
            program = eBlastp;
        if (program == eRPSTblastn)
            program = eTblastn;

        CBlastFormatOptions format_options(program, args["out"].AsOutputFile());
        
        format_options.SetAlignments(args["align"].AsInteger());
        format_options.SetDescriptions(args["descr"].AsInteger());
        format_options.SetAlignView(args["format"].AsInteger());
        format_options.SetHtml(args["html"].AsBoolean());
        
        bool db_is_na = (program == eBlastn || program == eTblastn || 
                         program == eTblastx);

        /// @todo Print output header here

        CBlastDbDataLoader::RegisterInObjectManager(*m_ObjMgr,
            args["db"].AsString(),
            db_is_na ? CBlastDbDataLoader::eNucleotide :
                       CBlastDbDataLoader::eProtein,
            CObjectManager::eDefault);

        /* Format the results */
        TSeqLocInfoVector maskv =
            BlastMaskLoc2CSeqLoc(blaster->GetFilteredQueryRegions(), 
                              blaster->GetQueries(), program);
        
        if (BLAST_FormatResults(seqalignv, program, blaster->GetQueries(), 
                maskv, &format_options, 
                blaster->GetOptions().GetOutOfFrameMode())) {
            ERR_POST_EX(CBlastException::eInternal, 2,
                       "Error in formatting results");
            exit(CBlastException::eInternal);
        }
        
        /// @todo Print output footer here
    }
}

int CBlastApplication::Run(void)
{
    int status = 0;

    // Process command line args
    const CArgs& args = GetArgs();
    
    EProgram program = ProgramNameToEnum(args["program"].AsString());

    Int4 strand_number = args["strand"].AsInteger();
    ENa_strand strand;
    Int4 from = args["qstart"].AsInteger();
    Int4 to = args["qend"].AsInteger();
    Int4 first_oid = 0;
    Int4 last_oid = 0;
    bool query_is_aa;

    query_is_aa = 
        (program == eBlastp || program == eTblastn || program == eRPSBlast);

    if (query_is_aa) {
        strand = eNa_strand_unknown;
    } else {
        if (strand_number == 1)
            strand = eNa_strand_plus;
        else if (strand_number == 2)
            strand = eNa_strand_minus;
        else
            strand = eNa_strand_both;
    }

    InitScope();

    int id_counter = 0;
    // Read the query(ies) from input file; perform the setup
    TSeqLocVector query_loc = 
        BLASTGetSeqLocFromStream(args["query"].AsInputFile(),
            *m_ObjMgr, strand, from, to, &id_counter,
            args["lcase"].AsBoolean());

    if (args["dbrange"]) {
        const char* delimiters = " ,:;";
        char* range_str = strdup(args["dbrange"].AsString().c_str());
        first_oid = atoi(strtok(range_str, delimiters));
        last_oid = atoi(strtok(NULL, delimiters));
        sfree(range_str);
    }
    
    bool db_is_aa = (program == eBlastp || program == eBlastx || 
                     program == eRPSBlast || program == eRPSTblastn);

    BlastSeqSrc* seq_src = 
        SeqDbBlastSeqSrcInit(args["db"].AsString(), db_is_aa,
                             first_oid, last_oid);
    char* error_str = BlastSeqSrcGetInitError(seq_src);
    if (error_str) {
        string msg(error_str);
        sfree(error_str);
        NCBI_THROW(CBlastException, eSeqSrc, msg);
    }

    /* If megablast lookup table is used, change default program to 
       eMegablast, facilitating use of megablast defaults. */
    if (args["lookup"].AsInteger() == 1) {
        program = eMegablast;
        if (args["templen"].AsInteger() > 0) 
            program = eDiscMegablast;
    }
    if (args["lookup"].AsInteger() != 1 && args["templen"].AsInteger() > 0) {
        ERR_POST_EX(CBlastException::eBadParameter, 2, 
        "\"-lookup 1\" option must be used if \"-templen\" option is not 0");
        exit(CBlastException::eBadParameter);
    }

    CBlastOptionsHandle* opts = CBlastOptionsFactory::Create(program);

    ProcessCommandLineArgs(opts, seq_src);

    // Perform repeats filtering if required
    char* repeat_filter_string = 
        GetRepeatsFilterOption(opts->GetOptions().GetFilterString());
    if (repeat_filter_string) {
        FindRepeatFilterLoc(query_loc, repeat_filter_string);
        sfree(repeat_filter_string);
    }

    BlastHSPStream* hsp_stream = NULL;
    bool tabular_output = args["tabular"].AsBoolean();

    int num_threads = args["threads"].AsInteger();

    TSeqAlignVector seqalignv;

    try {

       if (!tabular_output) {
	  CDbBlast blaster(query_loc, seq_src, *opts, 
                           hsp_stream, num_threads);
	  seqalignv = blaster.Run();
	  FormatResults(&blaster, seqalignv);
       } else {
	  hsp_stream = Blast_HSPListCQueueInit();
	  
	  CDbBlast blaster(query_loc, seq_src, *opts, 
                           hsp_stream, num_threads);
	  blaster.SetupSearch();
	  
          CSeqDbSeqInfoSrc seqinfo_src(args["db"].AsString(), db_is_aa);

	  // Start the on-the-fly formatting thread
          CBlastTabularFormatThread* tab_thread = 
              new CBlastTabularFormatThread(&blaster,
                      (args["out"] ? args["out"].AsOutputFile() : cout),
                      &seqinfo_src);

	  tab_thread->Run();
	  
	  blaster.RunPreliminarySearch();
	  // Close the HSP stream for writing, allowing the formatting thread
	  // to exit.
	  BlastHSPStreamClose(hsp_stream);
	  // Join the on-the-fly formatting thead
	  void *exit_data;
	  tab_thread->Join(&exit_data);
	  hsp_stream = BlastHSPStreamFree(hsp_stream);
       }

    } catch (const CBlastException& exptn) {
       cerr << exptn.GetErrCodeString() << endl;
       exptn.ReportAll();
       status = exptn.GetErrCode();
    }

    BlastSeqSrcFree(seq_src);

    return status;
}

void CBlastApplication::Exit(void)
{
    SetDiagStream(0);
}


int main(int argc, const char* argv[] /*, const char* envp[]*/)
{
    return CBlastApplication().AppMain(argc, argv, 0, eDS_Default, 0);
}
