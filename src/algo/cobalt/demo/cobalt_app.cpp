static char const rcsid[] = "$Id";

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
#include <objtools/data_loaders/blastdb/bdbloader.hpp>

#include <serial/iterator.hpp>
#include <objtools/readers/fasta.hpp>
#include <objtools/readers/reader_exception.hpp>

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
    HideStdArgs(fHideLogfile | fHideConffile | fHideVersion);
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(), 
                              "COBALT multiple alignment utility");

    arg_desc->AddKey("i", "infile", "Query file name",
                     CArgDescriptions::eInputFile);
    arg_desc->AddOptionalKey("p", "patternfile", 
                     "filename containing search patterns",
                     CArgDescriptions::eString);
    arg_desc->AddOptionalKey("db", "database", "domain database name",
                     CArgDescriptions::eString);
    arg_desc->AddOptionalKey("b", "blockfile", 
                     "filename containing conserved blocks",
                     CArgDescriptions::eString);
    arg_desc->AddOptionalKey("f", "freqfile", 
                     "filename containing residue frequencies",
                     CArgDescriptions::eString);
    arg_desc->AddOptionalKey("seqalign", "file", 
                     "destination filename for text seqalign",
                     CArgDescriptions::eOutputFile);
    arg_desc->AddDefaultKey("evalue", "evalue", 
                     "E-value threshold for selecting conserved domains",
                     CArgDescriptions::eDouble, "0.01");
    arg_desc->AddDefaultKey("evalue2", "evalue2", 
                     "E-value threshold for aligning filler segments",
                     CArgDescriptions::eDouble, "10.0");
    arg_desc->AddDefaultKey("g0", "penalty", 
                     "gap open penalty for initial/terminal gaps",
                     CArgDescriptions::eInteger, "5");
    arg_desc->AddDefaultKey("e0", "penalty", 
                     "gap extend penalty for initial/terminal gaps",
                     CArgDescriptions::eInteger, "1");
    arg_desc->AddDefaultKey("g1", "penalty", 
                     "gap open penalty for middle gaps",
                     CArgDescriptions::eInteger, "11");
    arg_desc->AddDefaultKey("e1", "penalty", 
                     "gap extend penalty for middle gaps",
                     CArgDescriptions::eInteger, "1");
    arg_desc->AddDefaultKey("ccc", "conserved_cutoff", 
                     "Minimum average score needed for a multiple alignment "
                     "column to be considered as conserved",
                     CArgDescriptions::eDouble, "2.0");
    arg_desc->AddDefaultKey("dfb", "domain_res_boost", 
                     "When assigning domain residue frequencies, the amount of "
                     "extra weight (0..1) to give the actual sequence letter "
                     "at that position",
                     CArgDescriptions::eDouble, "0.5");
    arg_desc->AddDefaultKey("ffb", "filler_res_boost", 
                     "When assigning filler residue frequencies, the amount of "
                     "extra weight (0..1) to give the actual sequence letter "
                     "at that position",
                     CArgDescriptions::eDouble, "1.0");
    arg_desc->AddDefaultKey("norps", "norps", 
                     "do not perform initial RPS blast search",
                     CArgDescriptions::eBoolean, "F");
    arg_desc->AddDefaultKey("v", "verbose", 
                     "verbose output",
                     CArgDescriptions::eBoolean, "F");
    arg_desc->AddDefaultKey("matrix", "matrix", 
                     "score matrix to use",
                     CArgDescriptions::eString, "BLOSUM62");

    SetupArgDescriptions(arg_desc.release());
}

TSeqLocVector
GetSeqLocFromStream(CNcbiIstream& instream, CObjectManager& objmgr)
{
    TSeqLocVector retval;

    // read one query at a time, and use a separate seq_entry,
    // scope, and lowercase mask for each query. This lets different
    // query sequences have the same ID. Later code will distinguish
    // between queries by using different elements of retval[]

    while (!instream.eof()) {
        CRef<CScope> scope(new CScope(objmgr));

        scope->AddDefaults();
        CRef<CSeq_entry> entry = ReadFasta(instream, 
                                    fReadFasta_AssumeProt |
                                    fReadFasta_ForceType |
                                    fReadFasta_OneSeq,
                                    0,
                                    0);
        if (entry == 0) {
            NCBI_THROW(CObjReaderException, eInvalid, 
                        "Could not retrieve seq entry");
        }
        scope->AddTopLevelSeqEntry(*entry);
        CTypeConstIterator<CBioseq> itr(ConstBegin(*entry));
        CRef<CSeq_loc> seqloc(new CSeq_loc());
        seqloc->SetWhole().Assign(*itr->GetId().front());
        SSeqLoc sl(seqloc, scope);
        retval.push_back(sl);
    }
    return retval;
}


int CMultiApplication::Run(void)
{
    SetDiagPostLevel(eDiag_Warning);

    // Process command line args
    const CArgs& args = GetArgs();
    
    // Set up data loaders
    m_ObjMgr = CObjectManager::GetInstance();

    CMultiAligner aligner(args["matrix"].AsString().c_str(),
                          -args["g1"].AsInteger(),
                          -args["e1"].AsInteger(),
                          -args["g0"].AsInteger(),
                          -args["e0"].AsInteger(),
                          args["evalue2"].AsDouble(),
                          args["ccc"].AsDouble(),
                          args["ffb"].AsDouble());

    TSeqLocVector queries = GetSeqLocFromStream(args["i"].AsInputFile(), 
                                                *m_ObjMgr);

    aligner.SetQueries(queries);

    if (args["norps"].AsBoolean() == false) {
        if (!args["db"] || !args["b"] || !args["f"]) {
            printf("RPS database, frequency file and block file "
                   "must be specified\n");
            exit(-1);
        }
        aligner.SetDomainInfo(args["db"].AsString(),
                              args["b"].AsString(),
                              args["f"].AsString(),
                              args["evalue"].AsDouble(),
                              args["dfb"].AsDouble());
    }

    if (args["p"]) {
        aligner.SetPatternInfo(args["p"].AsString());
    }

    aligner.SetVerbose(args["v"].AsBoolean());

    aligner.Run();

    const vector<CSequence>& results(aligner.GetResults());
    for (int i = 0; i < (int)results.size(); i++) {
        printf(">%s\n", 
               queries[i].seqloc->GetWhole().GetSeqIdString().c_str());
        for (int j = 0; j < results[i].GetLength(); j++) {
            printf("%c", results[i].GetPrintableLetter(j));
        }
        printf("\n");
    }
    if (args["seqalign"]) {
        CRef<CSeq_align> sa = aligner.GetSeqalignResults();
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

/*-----------------------------------------------------------------------
  $Log$
  Revision 1.1  2005/11/07 18:14:01  papadopo
  Initial revision

-----------------------------------------------------------------------*/
