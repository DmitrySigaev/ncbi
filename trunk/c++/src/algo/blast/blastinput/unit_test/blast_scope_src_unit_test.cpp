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
 * Author:  Christiam Camacho
 *
 */

/** @file blast_scope_src_unit_test.cpp
 */
#include <ncbi_pch.hpp>
#include <boost/test/auto_unit_test.hpp>

#include <algo/blast/blastinput/blast_scope_src.hpp>

#include <objects/seqloc/Seq_id.hpp>
#include <objmgr/util/seq_loc_util.hpp>

#include <objects/entrez2/entrez2_client.hpp>
#include <objmgr/seq_vector.hpp>

USING_NCBI_SCOPE;
USING_SCOPE(blast);
USING_SCOPE(objects);
using boost::unit_test::test_suite;


BOOST_AUTO_UNIT_TEST(RetrieveFromBlastDb_TestSequenceData) 
{
    CSeq_loc seqloc;
    seqloc.SetWhole().SetGi(129295);

    const char* seq =
"QIKDLLVSSSTDLDTTLVLVNAIYFKGMWKTAFNAEDTREMPFHVTKQESKPVQMMCMNNSFNVATLPAEKMKILELPFASGDLSMLVLLPDEVSDLERIEKTINFEKLTEWTNPNTMEKRRVKVYLPQMKIEEKYNLTSVLMALGMTDLFIPSANLTGISSAESLKISQAVHGAFMELSEDGIEMAGSTGVIEDIKHSPESEQFRADHPFLFLIKHNPTNTIVYFGRYWSP";

    SDataLoaderConfig dlconfig("nr", true);
    CBlastScopeSource scope_source(dlconfig);
    CRef<CScope> scope = scope_source.NewScope();

    CBioseq_Handle bh = scope->GetBioseqHandle(seqloc);
    CSeqVector sv = bh.GetSeqVector(CBioseq_Handle::eCoding_Iupac);

    for (size_t i = 0; i < sv.size(); i++) {
        BOOST_CHECK_EQUAL((char)seq[i], (char)sv[i]);
    }
}

static 
void s_RetrieveSequenceLength(int gi, 
                              const string& dbname,
                              bool is_prot,
                              TSeqPos kExpectedLength) 
{
    const CSeq_id seqid(CSeq_id::e_Gi, gi);

    SDataLoaderConfig dlconfig(dbname, is_prot);
    CBlastScopeSource scope_source(dlconfig);

    CRef<CScope> scope = scope_source.NewScope();
    TSeqPos length = sequence::GetLength(seqid, scope);
    BOOST_CHECK_EQUAL(kExpectedLength, length);
}

BOOST_AUTO_UNIT_TEST(RetrieveFromBlastDb) {
    s_RetrieveSequenceLength(129295, "nr", true, 232);
}

// This gi has been removed from the BLAST databases
BOOST_AUTO_UNIT_TEST(RetrieveFromGenbank) {
    s_RetrieveSequenceLength(7450545, "nr", true, 443);
}

BOOST_AUTO_UNIT_TEST(RetrieveFromGenbank_NewlyAddedSequenceToGenbank) {

    // Get today's date
    CTime t;
    t.SetCurrent().SetFormat("Y/M/D");
    const string query_str(t.AsString() + "[PDAT]");

    // Search sequences publishe recently
    CEntrez2Client entrez;
    vector<int> results;
    entrez.Query(query_str, "nucleotide", results);
    if (results.empty()) {
        // No newly added sequences today :(
        return;
    }

    const CSeq_id seqid(CSeq_id::e_Gi, results.front());
    TSeqPos length = sequence::GetLength(seqid,
                                         CBlastScopeSource().NewScope());
    s_RetrieveSequenceLength(results.front(), "nt", false, length);
}

BOOST_AUTO_UNIT_TEST(RetrieveFromGenbank_NoBlastDbDataLoader) {
    const CSeq_id seqid(CSeq_id::e_Gi, 7450545);
    CBlastScopeSource scope_source;
    CRef<CScope> scope = scope_source.NewScope();
    TSeqPos length = sequence::GetLength(seqid, scope);
    const TSeqPos kExpectedLength = 443;
    BOOST_CHECK_EQUAL(kExpectedLength, length);
}

BOOST_AUTO_UNIT_TEST(RetrieveFromGenbank_IncorrectBlastDbType) {
    s_RetrieveSequenceLength(7450545, "sts", false, 443);
}

BOOST_AUTO_UNIT_TEST(InvalidBlastDatabase) {
    // Make sure to post warnings
    SetDiagPostLevel(eDiag_Warning);
    CNcbiOstream* diag_stream = GetDiagStream();

    // Redirect the output warnings
    CNcbiOstrstream error_stream;
    SetDiagStream(&error_stream); 

    s_RetrieveSequenceLength(129295, "dummy", true, 232);

    const string kWarnings = error_stream.str();
    const string kExpectedMsg("No alias or index file found");
    BOOST_CHECK(kWarnings.find(kExpectedMsg) != NPOS);

    // restore the diagnostic stream
    SetDiagStream(diag_stream);
}
