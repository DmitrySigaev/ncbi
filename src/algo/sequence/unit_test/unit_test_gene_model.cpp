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
 * Author: Mike DiCuccio
 *
 * File Description:
 *
 * ===========================================================================
 */

#include <ncbi_pch.hpp>

#include <corelib/ncbiapp.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/test_boost.hpp>

#include <objmgr/object_manager.hpp>
#include <objtools/data_loaders/genbank/gbloader.hpp>
#include <objmgr/scope.hpp>

#include <serial/serial.hpp>
#include <serial/objistr.hpp>
#include <serial/objostr.hpp>

#include <objects/seq/Seq_annot.hpp>
#include <objects/seqalign/Seq_align.hpp>

#include <algo/sequence/gene_model.hpp>

USING_NCBI_SCOPE;
USING_SCOPE(objects);

NCBITEST_INIT_CMDLINE(arg_desc)
{
    // Here we make descriptions of command line parameters that we are
    // going to use.

    arg_desc->AddKey("data-in", "InputData",
                     "Concatenated Seq-aligns used to generate gene models",
                     CArgDescriptions::eInputFile);

    arg_desc->AddKey("data-expected", "InputData",
                     "Expected Seq-annots produced from input alignments",
                     CArgDescriptions::eInputFile);
}

BOOST_AUTO_TEST_CASE(TestUsingArg)
{
    CRef<CObjectManager> om = CObjectManager::GetInstance();
    CGBDataLoader::RegisterInObjectManager(*om);
    CScope scope(*om);
    scope.AddDefaults();

    const CArgs& args = CNcbiApplication::Instance()->GetArgs();
    CNcbiIstream& align_istr = args["data-in"].AsInputFile();
    CNcbiIstream& annot_istr = args["data-expected"].AsInputFile();

    auto_ptr<CObjectIStream> align_is(CObjectIStream::Open(eSerial_AsnText,
                                                           align_istr));
    auto_ptr<CObjectIStream> annot_is(CObjectIStream::Open(eSerial_AsnText,
                                                           annot_istr));

    while (align_istr  &&  annot_istr) {

        CSeq_align align;
        CSeq_annot expected_annot;

        /// we wrap the first serialization in try/catch
        /// if this fails, we are at the end of the file, and we expect both to
        /// be at the end of the file.
        /// a failure in the second serialization is fatal
        try {
            *align_is >> align;
        }
        catch (CEofException&) {
            try {
                *annot_is >> expected_annot;
            }
            catch (CEofException&) {
            }
            break;
        }

        *annot_is >> expected_annot;

        CBioseq_set seqs;
        CSeq_annot actual_annot;
        CGeneModel::CreateGeneModelFromAlign(align, scope,
                                             actual_annot, seqs);

        BOOST_CHECK(expected_annot.Equals(actual_annot));
    }

    BOOST_CHECK(align_istr.eof());
    BOOST_CHECK(annot_istr.eof());
}


