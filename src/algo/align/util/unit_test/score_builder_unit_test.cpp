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
* Author:  Christiam Camacho, NCBI
*
* File Description:
*   Unit tests for the CScoreBuilder class.
*
* NOTE:
*   Boost.Test reports some memory leaks when compiled in MSVC even for this
*   simple code. Maybe it's related to some toolkit static variables.
*   To avoid annoying messages about memory leaks run this program with
*   parameter --detect_memory_leak=0
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>

// This macro should be defined before inclusion of test_boost.hpp in all
// "*.cpp" files inside executable except one. It is like function main() for
// non-Boost.Test executables is defined only in one *.cpp file - other files
// should not include it. If NCBI_BOOST_NO_AUTO_TEST_MAIN will not be defined
// then test_boost.hpp will define such "main()" function for tests.
//
// Usually if your unit tests contain only one *.cpp file you should not
// care about this macro at all.
//
//#undef NCBI_BOOST_NO_AUTO_TEST_MAIN


// This header must be included before all Boost.Test headers if there are any
#include <corelib/test_boost.hpp>
#include <objects/seqalign/Seq_align.hpp>
#include <objects/seqalign/Score.hpp>
#include <objects/general/Object_id.hpp>
#include <algo/align/util/score_builder.hpp>
#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objtools/data_loaders/genbank/gbloader.hpp>

#include <common/test_assert.h>  /* This header must go last */


USING_NCBI_SCOPE;
USING_SCOPE(objects);

NCBITEST_INIT_CMDLINE(arg_desc)
{
    // Here we make descriptions of command line parameters that we are
    // going to use.

    arg_desc->AddKey("data-in", "InputData",
                     "Concatenated Seq-aligns used to generate gene models",
                     CArgDescriptions::eInputFile);
}


BOOST_AUTO_TEST_CASE(Test_Dense_seg)
{
    CRef<CObjectManager> om = CObjectManager::GetInstance();
    CGBDataLoader::RegisterInObjectManager(*om);
    CRef<CScope> scope(new CScope(*om));
    scope->AddDefaults();

    CScoreBuilder score_builder(blast::eBlastn);

    const CArgs& args = CNcbiApplication::Instance()->GetArgs();
    CNcbiIstream& istr = args["data-in"].AsInputFile();
    while (istr) {
        CSeq_align alignment;
        try {
            istr >> MSerial_AsnText >> alignment;
        }
        catch (CEofException&) {
            break;
        }

        ///
        /// we test several distinct scores
        ///

        int kExpectedIdentities = 0;
        alignment.GetNamedScore(CSeq_align::eScore_IdentityCount,
                                kExpectedIdentities);

        int kExpectedMismatches = 0;
        alignment.GetNamedScore(CSeq_align::eScore_MismatchCount,
                                kExpectedMismatches);

        int kExpectedScore = 0;
        alignment.GetNamedScore(CSeq_align::eScore_Score,
                                kExpectedScore);

        double kExpectedPctIdentity = 0;
        alignment.GetNamedScore(CSeq_align::eScore_PercentIdentity,
                                kExpectedPctIdentity);

        double kExpectedPctCoverage = 0;
        alignment.GetNamedScore(CSeq_align::eScore_PercentCoverage,
                                kExpectedPctCoverage);

        /// reset scores to avoid any taint in score generation
        alignment.ResetScore();

        /// check identity count
        {{
             int actual =
                 score_builder.GetIdentityCount(*scope, alignment);
             LOG_POST(Error << "Verifying score: num_ident: "
                      << kExpectedIdentities << " == " << actual);
             BOOST_CHECK_EQUAL(kExpectedIdentities, actual);
         }}

        /// check mismatch count
        {{
             int actual =
                 score_builder.GetMismatchCount(*scope, alignment);
             LOG_POST(Error << "Verifying score: num_mismatch: "
                      << kExpectedMismatches << " == " << actual);
             BOOST_CHECK_EQUAL(kExpectedMismatches, actual);
         }}

        /// check percent identity
        {{
             double actual =
                 score_builder.GetPercentIdentity(*scope, alignment);
             LOG_POST(Error << "Verifying score: pct_identity: "
                      << kExpectedPctIdentity << " == " << actual);

             /// machine precision is a problme here
             /// we verify to 12 digits of precision
             Uint8 int_pct_identity_expected = kExpectedPctIdentity * 1e12;
             Uint8 int_pct_identity_actual = actual * 1e12;
             BOOST_CHECK_EQUAL(int_pct_identity_expected,
                               int_pct_identity_actual);

             /**
             CScore score;
             score.SetId().SetStr("pct_identity");
             score.SetValue().SetReal(actual);
             cerr << MSerial_AsnText << score;
             **/
         }}

        /// check percent coverage
        {{
             double actual =
                 score_builder.GetPercentCoverage(*scope, alignment);
             LOG_POST(Error << "Verifying score: pct_coverage: "
                      << kExpectedPctCoverage << " == " << actual);

             /// machine precision is a problme here
             /// we verify to 12 digits of precision
             Uint8 int_pct_coverage_expected = kExpectedPctCoverage * 1e12;
             Uint8 int_pct_coverage_actual = actual * 1e12;
             BOOST_CHECK_EQUAL(int_pct_coverage_expected,
                               int_pct_coverage_actual);

             /**
             CScore score;
             score.SetId().SetStr("pct_coverage");
             score.SetValue().SetReal(actual);
             cerr << MSerial_AsnText << score;
             **/
         }}

        if (alignment.GetSegs().IsDenseg()) {
            ///
            /// our encoded dense-segs have a BLAST-style 'score'
            ///
            int actual = score_builder.GetBlastScore(*scope, alignment);
            LOG_POST(Error << "Verifying score: score: "
                     << kExpectedScore << " == " << actual);
            BOOST_CHECK_EQUAL(kExpectedScore, actual);
        }
    }
}


