static char const rcsid[] = "$Id$";

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

File name: cobalt_app.cpp

Author: Jason Papadopoulos

Contents: C++ driver for COBALT multiple alignment algorithm

******************************************************************************/

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbifile.hpp>
#include <objmgr/object_manager.hpp>
#include <objmgr/util/sequence.hpp>
#include <objtools/data_loaders/blastdb/bdbloader.hpp>
#include <serial/iterator.hpp>
#include <objtools/readers/fasta.hpp>
#include <objtools/readers/reader_exception.hpp>
#include <objtools/alnmgr/alnvec.hpp>

#include <algo/cobalt/cobalt.hpp>

USING_NCBI_SCOPE;
USING_SCOPE(objects);
USING_SCOPE(cobalt);

class CMultiApplication : public CNcbiApplication
{
private:
    virtual void Init(void);
    virtual int Run(void);
    virtual void Exit(void);

    CRef<CObjectManager> m_ObjMgr;
};

void CMultiApplication::Init(void)
{
    HideStdArgs(fHideLogfile | fHideConffile | fHideFullVersion | fHideXmlHelp
                | fHideDryRun);

    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(), 
                             "COBALT multiple sequence alignment utility");

    // Input sequences
    arg_desc->AddKey("i", "infile", "File containing input sequences in FASTA"
                     " format", CArgDescriptions::eInputFile);


    // Conserved domain options
    arg_desc->SetCurrentGroup("Conserved domain options");
    arg_desc->AddOptionalKey("rpsdb", "database", "Conserved domain database "
                             "name\nEither database or -norps option must be "
                             "specified", CArgDescriptions::eString);
    arg_desc->AddDefaultKey("norps", "norps", "Do not perform initial "
                            "RPS-BLAST search",
                            CArgDescriptions::eBoolean, "F");
    arg_desc->AddDefaultKey("rps_evalue", "evalue", 
                            "E-value threshold for selecting conserved domains"
                            " from results of RPS-BLAST search",
                            CArgDescriptions::eDouble, "0.01");
    arg_desc->AddOptionalKey("p", "patternfile", 
                             "Filename containing regular expression patterns "
                             "for conserved domains",
                             CArgDescriptions::eString);
    arg_desc->AddDefaultKey("dfb", "domain_res_boost", 
                     "When assigning domain residue frequencies, the amount of "
                     "extra weight (0..1) to give to the actual sequence letter "
                     "at that position",
                     CArgDescriptions::eDouble, "0.5");


    // User conststraints options
    arg_desc->SetCurrentGroup("Constraints options");
    arg_desc->AddOptionalKey("c", "constraintfile", 
                     "Filename containing pairwise alignment constraints, "
                     "one per line, each represented by 6 integers:\n"
                     "  -zero-based index of sequence 1 in the input file\n"
                     "  -zero-based start position in sequence 1\n"
                     "  -zero-based stop position in sequence 1\n"
                     "  -zero-based index of sequence 2 in the input file\n"
                     "  -zero-based start position in sequence 2\n"
                     "  -zero-based stop position in sequence 2\n",
                     CArgDescriptions::eString);


    // Multiple alignment options
    arg_desc->SetCurrentGroup("Multiple alignment options");
    arg_desc->AddDefaultKey("treemethod", "method", 
                     "Method for generating progressive alignment guide tree",
                      CArgDescriptions::eString, "clust");
    arg_desc->SetConstraint("treemethod", &(*new CArgAllow_Strings,
                                      "clust", "nj", "fastme"));
    arg_desc->AddDefaultKey("iter", "iterate", 
                     "After the first iteration search for conserved columns "
                     "and realign if any are found",
                     CArgDescriptions::eBoolean, "T");
    arg_desc->AddDefaultKey("ccc", "conserved_cutoff", 
                     "Minimum average score needed for a multiple alignment "
                     "column to be considered as conserved",
                     CArgDescriptions::eDouble, "0.67");
    arg_desc->AddDefaultKey("pseudo", "pseudocount", 
                     "Pseudocount constant",
                     CArgDescriptions::eDouble, "2.0");
    arg_desc->AddDefaultKey("ffb", "filler_res_boost", 
                     "When assigning filler residue frequencies, the amount of "
                     "extra weight (0..1) to give to the actual sequence letter "
                     "at that position",
                     CArgDescriptions::eDouble, "1.0");


    // Pairwise alignment options
    arg_desc->SetCurrentGroup("Pairwise alignment options");
    arg_desc->AddDefaultKey("matrix", "matrix", 
                     "Score matrix to use",
                     CArgDescriptions::eString, "BLOSUM62");
    arg_desc->AddDefaultKey("end_gapopen", "penalty", 
                     "Gap open penalty for terminal gaps",
                     CArgDescriptions::eInteger, "5");
    arg_desc->AddDefaultKey("end_gapextend", "penalty", 
                     "Gap extend penalty for terminal gaps",
                     CArgDescriptions::eInteger, "1");
    arg_desc->AddDefaultKey("gapopen", "penalty", 
                     "Gap open penalty for internal gaps",
                     CArgDescriptions::eInteger, "11");
    arg_desc->AddDefaultKey("gapextend", "penalty", 
                     "Gap extend penalty for internal gaps",
                     CArgDescriptions::eInteger, "1");
    arg_desc->AddDefaultKey("blast_evalue", "evalue", 
                   "E-value threshold for selecting segments matched "
                   "by BLASTP",
                   CArgDescriptions::eDouble, "0.01");


    // Query clustering options
    arg_desc->SetCurrentGroup("Query clustering options");
    arg_desc->AddDefaultKey("clusters", "clusters", 
                    "Use query clustering for faster alignment",
                     CArgDescriptions::eBoolean, "T");
    arg_desc->AddDefaultKey("k", "length", 
                      "K-mer length for query clustering",
                     CArgDescriptions::eInteger, "4");
    arg_desc->AddDefaultKey("max_dist", "distance",
                     "Maximum allowed distance between sequences in a cluster"
                     " (0..1)",
                     CArgDescriptions::eDouble, "0.8");
    arg_desc->AddDefaultKey("alph", "name",
                     "Alphabet for used k-mer counting",
                     CArgDescriptions::eString, "regular");
    arg_desc->SetConstraint("alph", &(*new CArgAllow_Strings, "regular",
                                      "se-v10", "se-b15"));


    // Output options
    arg_desc->SetCurrentGroup("Output options");
    arg_desc->AddOptionalKey("seqalign", "file", 
                             "Output text seqalign to specified file",
                             CArgDescriptions::eOutputFile);
    arg_desc->AddFlag("v", "Verbose output");


    SetupArgDescriptions(arg_desc.release());
}


void
x_GetSeqLocFromStream(CNcbiIstream& instream, CObjectManager& objmgr,
                      vector< CRef<objects::CSeq_loc> >& seqs,
                      CRef<objects::CScope>& scope)
{
    seqs.clear();
    scope.Reset(new CScope(objmgr));
    scope->AddDefaults();

    // read one query at a time, and use a separate seq_entry,
    // scope, and lowercase mask for each query. This lets different
    // query sequences have the same ID. Later code will distinguish
    // between queries by using different elements of retval[]

    CStreamLineReader line_reader(instream);
    CFastaReader fasta_reader(line_reader, 
                              CFastaReader::fAssumeProt |
                              CFastaReader::fForceType |
                              CFastaReader::fNoParseID);

    while (!line_reader.AtEOF()) {

        CRef<CSeq_entry> entry = fasta_reader.ReadOneSeq();

        if (entry == 0) {
            NCBI_THROW(CObjReaderException, eInvalid, 
                        "Could not retrieve seq entry");
        }
        scope->AddTopLevelSeqEntry(*entry);
        CTypeConstIterator<CBioseq> itr(ConstBegin(*entry));
        CRef<CSeq_loc> seqloc(new CSeq_loc());
        seqloc->SetWhole().Assign(*itr->GetId().front());
        seqs.push_back(seqloc);
    }
}

static void
x_LoadConstraints(string constraintfile,
                  vector<CMultiAlignerOptions::SConstraint>& constr)
{
    CNcbiIfstream f(constraintfile.c_str());
    if (f.bad() || f.fail())
        NCBI_THROW(CMultiAlignerException, eInvalidInput,
                   "Cannot open file with pairwise constraints");

    int seq1, seq1_start, seq1_end;
    int seq2, seq2_start, seq2_end;

    constr.clear();

    f >> seq1 >> seq1_start >> seq1_end;
    f >> seq2 >> seq2_start >> seq2_end;
    CMultiAlignerOptions::SConstraint 
        c(seq1, seq1_start, seq1_end, seq2, seq2_start, seq2_end);
    constr.push_back(c);

    while (!f.eof()) {
        seq1 = -1;

        f >> seq1 >> seq1_start >> seq1_end;
        f >> seq2 >> seq2_start >> seq2_end;
        if (seq1 >= 0) {
            constr.push_back(CMultiAlignerOptions::SConstraint(seq1,
                          seq1_start, seq1_end, seq2, seq2_start, seq2_end));
        }
    }
}


static void
x_LoadPatterns(string patternsfile,
               vector<CMultiAlignerOptions::CPattern>& patterns)
{
    CNcbiIfstream f(patternsfile.c_str());
    if (f.bad() || f.fail())
        NCBI_THROW(CMultiAlignerException, eInvalidInput,
                   "Cannot open patterns file");

    patterns.clear();

    while (!f.eof()) {
        string single_pattern;

        f >> single_pattern;

        if (!single_pattern.empty()) {
            patterns.push_back(single_pattern);
        }
    }
}


int CMultiApplication::Run(void)
{
    // Allow the fasta reader to complain on 
    // invalid sequence input
    SetDiagPostLevel(eDiag_Warning);

    // Process command line args
    const CArgs& args = GetArgs();
    

    if (args["rpsdb"] && args["norps"].AsBoolean()) {
        NCBI_THROW(CMultiAlignerException, eInvalidInput,
                   "The options -rpsdb and -norps T are mutually exclusive");
    }

    if (!args["rpsdb"] && !args["norps"].AsBoolean()) {
        NCBI_THROW(CMultiAlignerException, eInvalidInput,
                   "RPS dababase not specified");
    }


    // Set up data loaders
    m_ObjMgr = CObjectManager::GetInstance();

    CRef<CMultiAlignerOptions> opts(new CMultiAlignerOptions(
                                      CMultiAlignerOptions::fNoRpsBlast
                                      | CMultiAlignerOptions::fNoPatterns));

    // PSSM aligner parameters
    opts->SetGapOpenPenalty(-args["gapopen"].AsInteger());
    opts->SetGapExtendPenalty(-args["gapextend"].AsInteger());
    opts->SetEndGapOpenPenalty(-args["end_gapopen"].AsInteger());
    opts->SetEndGapExtendPenalty(-args["end_gapextend"].AsInteger());
    opts->SetScoreMatrixName(args["matrix"].AsString());

    // RPS Blast parameters
    if (args["rpsdb"]) {
        opts->SetRpsDb(args["rpsdb"].AsString());

        // Check whether RPS database and auxialry files exist
        const string dbname = args["rpsdb"].AsString();
        CFile rps(dbname + ".rps");
        if (!rps.Exists()) {
            NcbiCerr << "Error: RPS database file: " << dbname + ".rps"
                     << " is missing" << NcbiEndl;
            return 1;
        }

        CFile blocks(dbname + ".blocks");
        if (!blocks.Exists()) {
            NcbiCerr << "Error: RPS block file: " << dbname + ".blocks"
                     <<  " is missing" << NcbiEndl;
            return 1;
        }

        CFile freq(dbname + ".freq");
        if (!freq.Exists()) {
            NcbiCerr << "Error: RPS frequencies file: " << dbname + ".freq"
                     << " is missing" << NcbiEndl;
            return 1;
        }
    }
    opts->SetRpsEvalue(args["rps_evalue"].AsDouble());
    opts->SetDomainResFreqBoost(args["dfb"].AsDouble());

    // Blastp parameters
    opts->SetBlastpEvalue(args["blast_evalue"].AsDouble());
    opts->SetLocalResFreqBoost(args["ffb"].AsDouble());

    // Patterns
    if (args["p"]) {
        x_LoadPatterns(args["p"].AsString(), opts->SetCddPatterns());
    }

    // User constraints
    if (args["c"]) {
        x_LoadConstraints(args["c"].AsString(), opts->SetUserConstraints());
    }

    // Progressive alignmenet params
    CMultiAlignerOptions::ETreeMethod tree_method;
    if (args["treemethod"].AsString() == "clust") {
        tree_method = CMultiAlignerOptions::eClusters;
    }
    else if (args["treemethod"].AsString() == "nj") {
        tree_method = CMultiAlignerOptions::eNJ;
    }
    else if (args["treemethod"].AsString() == "fastme") {
        tree_method = CMultiAlignerOptions::eFastME;
    }
    else {
        NcbiCerr << "Error: Incorrect tree method";
        return 1;
    }
    opts->SetTreeMethod(tree_method);

    // Iterative alignment params
    opts->SetIterate(args["iter"].AsBoolean());
    opts->SetConservedCutoffScore(args["ccc"].AsDouble());
    opts->SetPseudocount(args["pseudo"].AsDouble());

    // Query clustering params
    opts->SetUseQueryClusters(args["clusters"].AsBoolean());
    opts->SetKmerLength(args["k"].AsInteger());
    opts->SetMaxInClusterDist(args["max_dist"].AsDouble());

    CMultiAligner::TKMethods::ECompressedAlphabet alph 
                                 = CMultiAligner::TKMethods::eRegular;
    if (args["alph"]) {
        if (args["alph"].AsString() == "regular") {
            alph = CMultiAligner::TKMethods::eRegular;
        }
        else if (args["comp_alph"].AsString() == "se-v10") {
            alph = CMultiAligner::TKMethods::eSE_V10;
        } 
        else if (args["comp_alph"].AsString() == "se-b15") {
            alph = CMultiAligner::TKMethods::eSE_B15;
        }
    }
    opts->SetKmerAlphabet(alph);

    // not option of the application
    opts->SetInClustAlnMethod(args["clusters"].AsBoolean() 
                              ? CMultiAlignerOptions::eMulti
                              : CMultiAlignerOptions::eNone);


    // Verbose level
    opts->SetVerbose(args["v"]);

    // Validate options and print warning messages if any
    if (!opts->Validate()) {
        ITERATE(vector<string>, it, opts->GetMessages()) {
            NcbiCerr << "Warning: " << *it << NcbiEndl;
        }
    }

    CMultiAligner aligner(opts);

    vector< CRef<objects::CSeq_loc> > queries;
    CRef<objects::CScope> scope;
    x_GetSeqLocFromStream(args["i"].AsInputFile(), *m_ObjMgr, queries, scope);
    _ASSERT(!scope.Empty());

    aligner.SetQueries(queries, scope);
    CMultiAligner::TStatus status = aligner.Run();

    // If aligner returns with error status then write messages and exit
    if (status > CMultiAligner::eWarnings) {
        ITERATE(vector<string>, it, aligner.GetMessages()) {
            NcbiCerr << "Error: " << *it << NcbiEndl;
        }

        return 1;
    }

    // If aligner returns with warning status then write messeges and proceed
    if (status == CMultiAligner::eWarnings) {
        ITERATE(vector<string>, it, aligner.GetMessages()) {
            NcbiCerr << "Warning: " << *it << NcbiEndl;
        }
    }

    const vector<CSequence>& results(aligner.GetSeqResults());
    for (int i = 0; i < (int)results.size(); i++) {
        CBioseq_Handle bhandle = scope->GetBioseqHandle(queries[i]->GetWhole(),
                                                       CScope::eGetBioseq_All);

        printf(">%s\n", sequence::GetTitle(bhandle).c_str());
        for (int j = 0; j < results[i].GetLength(); j++) {
            printf("%c", results[i].GetPrintableLetter(j));
        }
        printf("\n");
    }

    if (args["seqalign"]) {
        CRef<CSeq_align> sa = aligner.GetResults();
        CNcbiOstream& out = args["seqalign"].AsOutputFile();
        out << MSerial_AsnText << *sa;
    }

    return 0;
}

void CMultiApplication::Exit(void)
{
    SetDiagStream(0);
}

int main(int argc, const char* argv[])
{
    return CMultiApplication().AppMain(argc, argv, 0, eDS_Default, 0);
}
