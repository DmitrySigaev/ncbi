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
 * Authors:  Christiam Camacho
 *
 * File Description:
 *   Main driver for blast2sequences C++ interface
 *
 */

#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbiargs.hpp>

#include <serial/iterator.hpp>

#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/seq_vector.hpp>
#include <objmgr/gbloader.hpp>
#include <objmgr/util/sequence.hpp>

#include <corelib/ncbitime.hpp>
#include <objtools/readers/fasta.hpp>

#include <algo/blast/api/bl2seq.hpp>
#include "blast_input.hpp"

#include <objects/seqalign/Seq_align_set.hpp>

#ifdef WRITE_SEQALIGNS
#include <ctools/asn_converter.hpp>
#include <objalign.h>
#endif

#ifdef CPP_FORMATTING
#include "blast_format.hpp"
#endif

#if 0
// C includes for C formatter
#include <sqnutils.h>
#include <txalign.h>
#include <accid1.h>
#endif

USING_NCBI_SCOPE;
USING_SCOPE(blast);
USING_SCOPE(objects);


/////////////////////////////////////////////////////////////////////////////
//  CBlast2seqApplication::


class CBlast2seqApplication : public CNcbiApplication
{
private:
    virtual void Init(void);
    virtual int  Run(void);
    virtual void Exit(void);

    void InitScope(void);
    EProgram GetBlastProgramNum(const string& prog);
    void ProcessCommandLineArgs(CBlastOptions& opt);

    // needed for debugging only
    FILE* GetOutputFilePtr(void);

    CRef<CObjectManager>    m_ObjMgr;
    CRef<CScope>            m_Scope;
};

void CBlast2seqApplication::Init(void)
{
    HideStdArgs(fHideLogfile | fHideConffile | fHideVersion);
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "Compares 2 sequence using the BLAST algorithm");

    // Program type
    arg_desc->AddKey("program", "p", "Type of BLAST program",
            CArgDescriptions::eString);
    arg_desc->SetConstraint
        ("program", &(*new CArgAllow_Strings, 
                "blastp", "blastn", "blastx", "tblastn", "tblastx"));

    // Query sequence
    arg_desc->AddDefaultKey("query", "q", "Query file name",
            CArgDescriptions::eInputFile, "-", CArgDescriptions::fPreOpen);

    // Subject(s) sequence(s)
    arg_desc->AddKey("subject", "s", "Subject(s) file name",
            CArgDescriptions::eInputFile, CArgDescriptions::fPreOpen);

    // Copied from blast_app
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
    arg_desc->AddDefaultKey("templtype", "templtype", 
        "Discontiguous word template type",
        CArgDescriptions::eInteger, "0");
    arg_desc->AddDefaultKey("thresh", "threshold", 
        "Score threshold for neighboring words",
        CArgDescriptions::eInteger, "0");
    arg_desc->AddDefaultKey("window","window", "Window size for two-hit extension",
                            CArgDescriptions::eInteger, "0");
    arg_desc->AddDefaultKey("ag", "ag", 
        "Should AG method be used for scanning the database?",
        CArgDescriptions::eBoolean, "T");
    arg_desc->AddDefaultKey("varword", "varword", 
        "Should variable word size be used?",
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
        "Use greedy algorithm for gapped extensions?",
        CArgDescriptions::eBoolean, "F");
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
    arg_desc->AddDefaultKey("descr", "descriptions",
        "How many matching sequence descriptions to show?",
        CArgDescriptions::eInteger, "500");
    arg_desc->AddDefaultKey("align", "alignments", 
        "How many matching sequence alignments to show?",
        CArgDescriptions::eInteger, "250");
    arg_desc->AddDefaultKey("out", "out", "File name for writing output",
        CArgDescriptions::eOutputFile, "-", CArgDescriptions::fPreOpen);
    arg_desc->AddDefaultKey("format", "format", 
        "How to format the results?",
        CArgDescriptions::eInteger, "0");
    arg_desc->AddDefaultKey("html", "html", "Produce HTML output?",
                            CArgDescriptions::eBoolean, "F");
    arg_desc->AddDefaultKey("gencode", "gencode", "Query genetic code",
                            CArgDescriptions::eInteger, "0");
    arg_desc->AddDefaultKey("dbgencode", "dbgencode", "Database genetic code",
                            CArgDescriptions::eInteger, "0");
    arg_desc->AddDefaultKey("maxintron", "maxintron", 
                            "Longest allowed intron length for linking HSPs",
                            CArgDescriptions::eInteger, "0");
    arg_desc->AddDefaultKey("frameshift", "frameshift",
                            "Frame shift penalty (blastx only)",
                            CArgDescriptions::eInteger, "0");
    arg_desc->AddOptionalKey("asnout", "seqalignasn", 
        "File name for writing the seqalign results in ASN.1 form",
        CArgDescriptions::eOutputFile);

    // Debug parameters
    arg_desc->AddFlag("trace", "Tracing enabled?", true);

    SetupArgDescriptions(arg_desc.release());
}

void 
CBlast2seqApplication::InitScope(void)
{
    if (m_Scope.Empty()) {
        m_ObjMgr.Reset(new CObjectManager());
        m_ObjMgr->RegisterDataLoader(*new CGBDataLoader("ID", 0, 2),
                CObjectManager::eDefault);

        m_Scope.Reset(new CScope(*m_ObjMgr));
        m_Scope->AddDefaults();
        _TRACE("Blast2seqApp: Initializing scope");
    }
}

EProgram
CBlast2seqApplication::GetBlastProgramNum(const string& prog)
{
    if (prog == "blastp")
        return eBlastp;
    if (prog == "blastn")
        return eBlastn;
    if (prog == "blastx")
        return eBlastx;
    if (prog == "tblastn")
        return eTblastn;
    if (prog == "tblastx")
        return eTblastx;
    return eBlastUndef;
}

void
CBlast2seqApplication::ProcessCommandLineArgs(CBlastOptions& opt)
{
    CArgs args = GetArgs();

    if (args["strand"].AsInteger()) {
        switch (args["strand"].AsInteger()) {
        case 1: opt.SetStrandOption(eNa_strand_plus); break;
        case 2: opt.SetStrandOption(eNa_strand_minus); break;
        case 3: opt.SetStrandOption(eNa_strand_both); break;
        default: abort();
        }
    }

    opt.SetFilterString(args["filter"].AsString().c_str());
    // FIXME: Handle lcase masking

    if (args["lookup"].AsInteger()) {
        opt.SetLookupTableType(args["lookup"].AsInteger());
    }
    if (args["matrix"]) {
        opt.SetMatrixName(args["matrix"].AsString().c_str());
    }
    if (args["mismatch"].AsInteger()) {
        opt.SetMismatchPenalty(args["mismatch"].AsInteger());
    }
    if (args["match"].AsInteger()) {
        opt.SetMatchReward(args["match"].AsInteger());
    }
    if (args["word"].AsInteger()) {
        opt.SetWordSize(args["word"].AsInteger());
    }
    if (args["templen"].AsInteger()) {
        opt.SetMBTemplateLength(args["templen"].AsInteger());
    }
    if (args["templtype"].AsInteger()) {
        opt.SetMBTemplateType(args["templtype"].AsInteger());
    }
    if (args["thresh"].AsInteger()) {
        opt.SetWordThreshold(args["thresh"].AsInteger());
    }
    if (args["window"].AsInteger()) {
        opt.SetWindowSize(args["window"].AsInteger());
    }

    // The next 3 apply to nucleotide searches only
    string program = args["program"].AsString();
    if (program == "blastn") {
        opt.SetSeedExtensionMethod(args["ag"].AsBoolean());
        opt.SetVariableWordsize(args["varword"].AsBoolean());
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

    if (args["gopen"].AsInteger()) {
        opt.SetGapOpeningCost(args["gopen"].AsInteger());
    }
    if (args["gext"].AsInteger()) {
        opt.SetGapExtensionCost(args["gext"].AsInteger());
    }

    if (args["greedy"].AsBoolean()) {
        opt.SetGapExtnAlgorithm(EXTEND_GREEDY);
        opt.SetUngappedExtension(false);
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
    }
    if (args["perc"].AsDouble()) {
        opt.SetPercentIdentity(args["perc"].AsDouble());
    }

    if (args["gencode"].AsInteger()) {
        opt.SetQueryGeneticCode(args["gencode"].AsInteger());
    }
    if (args["dbgencode"].AsInteger()) {
        opt.SetDbGeneticCode(args["dbgencode"].AsInteger());
    }

    if (args["maxintron"].AsInteger()) {
        opt.SetLongestIntronLength(args["maxintron"].AsInteger());
    }
    if (args["frameshift"].AsInteger()) {
        opt.SetFrameShiftPenalty(args["frameshift"].AsInteger());
    }

    return;
}

FILE*
CBlast2seqApplication::GetOutputFilePtr(void)
{
    FILE *retval = NULL;

    if (GetArgs()["out"].AsString() == "-")
        retval = stdout;    
    else
        retval = fopen((char *)GetArgs()["out"].AsString().c_str(), "a");

    ASSERT(retval);
    return retval;
}

/*****************************************************************************/

int CBlast2seqApplication::Run(void)
{
    CStopWatch sw;
    InitScope();
    CArgs args = GetArgs();
    if (args["trace"])
        SetDiagTrace(eDT_Enable);

    CNcbiOstream& out = args["out"].AsOutputFile();
    int counter = 0;

    // Retrieve input sequences
    TSeqLocVector query_loc = 
        BLASTGetSeqLocFromStream(args["query"].AsInputFile(), m_Scope,
          eNa_strand_unknown, 0, 0, &counter, args["lcase"].AsBoolean());

    TSeqLocVector subject_loc = 
        BLASTGetSeqLocFromStream(args["subject"].AsInputFile(), m_Scope,
          eNa_strand_unknown, 0, 0, &counter);

    // Get program name
    EProgram prog =
        GetBlastProgramNum(args["program"].AsString());

    sw.Start();
    CBl2Seq blaster(query_loc, subject_loc, prog);
    ProcessCommandLineArgs(blaster.SetOptions());
    TSeqAlignVector seqalignv = blaster.MultiQRun();

    double t = sw.Elapsed();
    cerr << "CBl2seq run took " << t << " seconds" << endl;
    if (seqalignv.size() == 0) {
        cerr << "Returned NULL SeqAlign!" << endl;
        exit(1);
    }

    if (args["asnout"]) {
        auto_ptr<CObjectOStream> asnout(
            CObjectOStream::Open(args["asnout"].AsString(), eSerial_AsnText));
        int index;
        for (index = 0; index < seqalignv.size(); ++index)
            *asnout << *seqalignv[index];
    }
#ifdef WRITE_SEQALIGNS
        // Convert CSeq_align_set to linked list of SeqAlign structs
        DECLARE_ASN_CONVERTER(CSeq_align, SeqAlign, converter);
        SeqAlignPtr salp = NULL, tmp = NULL, tail = NULL;

        int query_index;

        for (query_index = 0; query_index < seqalign->size(); ++query_index)
        {
            ITERATE(list< CRef<CSeq_align> >, itr, 
                    (*seqalign)[query_index]->Get()) {
                tmp = converter.ToC(**itr);

                if (!salp)
                    salp = tail = tmp;
                else {
                    tail->next = tmp;
                    while (tail->next)
                        tail = tail->next;
                }
            }
        }

        if (args["asnout"]) {
            AsnIoPtr aip = AsnIoOpen(
                    (char*)args["asnout"].AsString().c_str(), (char*)"w");
            GenericSeqAlignSetAsnWrite(salp, aip);
            AsnIoReset(aip);
            AsnIoClose(aip);
        }
#endif

#ifdef CPP_FORMATTING
        // Display with C++ formatter
        TSeqLocInfoVector maskv;
        int index;
        for (index=0; index < query_loc.size(); ++index)
            maskv.push_back(0);

        CBlastFormatOptions* format_options = 
            new CBlastFormatOptions(prog, args["out"].AsOutputFile());

        Int2 status = Bl2seq_FormatResults(seqalignv, 
                 prog, query_loc, subject_loc, maskv, 
                 format_options, 
                 args["frameshift"].AsBoolean());
#endif
        
#if 0
        // Display w/ C formatter
        UseLocalAsnloadDataAndErrMsg();
        if (!SeqEntryLoad()) return 1;
        if (!ID1BioseqFetchEnable(const_cast<char*>("bl2seq"), true)) {
            cerr << "could not initialize entrez sequence retrieval" << endl;
            return 1;
        }

        SeqAnnotPtr seqannot = SeqAnnotNew();
        seqannot->type = 2;
        AddAlignInfoToSeqAnnot(seqannot, (Uint1)prog);
        seqannot->data = salp;

        Uint4 align_opts = TXALIGN_MATRIX_VAL | TXALIGN_SHOW_QS
            | TXALIGN_COMPRESS | TXALIGN_END_NUM;
        if (prog == eBlastx) align_opts |= TXALIGN_BLASTX_SPECIAL;

        FILE *fp = GetOutputFilePtr();
        Uint1 feats[255] = { 0 };   // dummy argument
        ShowTextAlignFromAnnot(seqannot, 60, fp, feats, feats, align_opts, 
                NULL, NULL, FormatScoreFunc);
        seqannot = SeqAnnotFree(seqannot);

#endif

    return 0;
}

void CBlast2seqApplication::Exit(void)
{
    SetDiagStream(0);
}


int main(int argc, const char* argv[])
{
    return CBlast2seqApplication().AppMain(argc, argv, 0, eDS_Default, 0);
}


/*
 * ===========================================================================
 * $Log$
 * Revision 1.24  2003/10/21 17:34:34  camacho
 * Renaming of gap open/extension accessors/mutators
 *
 * Revision 1.23  2003/10/17 18:22:28  dondosha
 * Use separate variables for different initial word extension options
 *
 * Revision 1.22  2003/10/08 15:27:02  camacho
 * Remove unnecessary conditional
 *
 * Revision 1.21  2003/10/07 17:37:10  dondosha
 * Lower case mask is now a boolean argument in call to BLASTGetSeqLocFromStream
 *
 * Revision 1.20  2003/09/26 21:36:29  dondosha
 * Show results for all queries in multi-query case
 *
 * Revision 1.19  2003/09/26 15:42:23  dondosha
 * Added second argument to SetExtendWordMethod, so bit can be set or unset
 *
 * Revision 1.18  2003/09/11 17:46:16  camacho
 * Changed CBlastOption -> CBlastOptions
 *
 * Revision 1.17  2003/09/09 15:43:43  ucko
 * Fix #include directive for blast_input.hpp.
 *
 * Revision 1.16  2003/09/05 18:24:28  camacho
 * Restoring printing of SeqAlign, fix setting of default word extension method
 *
 * Revision 1.15  2003/08/28 23:17:20  camacho
 * Add processing of command-line options
 *
 * Revision 1.14  2003/08/19 20:36:44  dondosha
 * EProgram enum type is no longer part of CBlastOptions class
 *
 * Revision 1.13  2003/08/18 20:58:57  camacho
 * Added blast namespace, removed *__.hpp includes
 *
 * Revision 1.12  2003/08/18 17:07:42  camacho
 * Introduce new SSeqLoc structure (replaces pair<CSeq_loc, CScope>).
 * Change in function to read seqlocs from files.
 *
 * Revision 1.11  2003/08/15 16:03:00  dondosha
 * TSeqLoc and TSeqLocVector types no longer belong to class CBl2Seq, but are common to all BLAST applications
 *
 * Revision 1.10  2003/08/11 20:16:43  camacho
 * Change return type of BLASTGetSeqLocFromStream and fix namespaces
 *
 * Revision 1.9  2003/08/11 15:26:30  dondosha
 * BLASTGetSeqLocFromStream function moved to blast_input.cpp
 *
 * Revision 1.8  2003/08/08 20:46:08  camacho
 * Fix to use new ReadFasta arguments
 *
 * Revision 1.7  2003/08/08 20:24:31  dicuccio
 * Adjustments for Unix build: rename 'ncmimath' -> 'ncbi_math'; fix #include in demo app
 *
 * Revision 1.6  2003/08/01 22:38:31  camacho
 * Added conditional compilation to write seqaligns
 *
 * Revision 1.5  2003/07/30 16:33:31  madden
 * Remove C toolkit dependencies
 *
 * Revision 1.4  2003/07/16 20:25:34  camacho
 * Added dummy features argument to C formatter
 *
 * Revision 1.3  2003/07/14 21:53:32  camacho
 * Minor
 *
 * Revision 1.2  2003/07/11 21:22:57  camacho
 * Use same command line option as blast to display seqalign
 *
 * Revision 1.1  2003/07/10 18:35:58  camacho
 * Initial revision
 *
 * ===========================================================================
 */
