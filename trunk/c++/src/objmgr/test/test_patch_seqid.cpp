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
* Authors:  Maxim Didenko
*
* File Description:
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>

#include <corelib/ncbiapp.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbistre.hpp>

#include <objects/seq/seq_id_handle.hpp>
#include <objmgr/seq_id_translator.hpp>
#include <objmgr/impl/tse_assigner.hpp>

#include <boost/version.hpp>
#include <boost/test/unit_test.hpp>
#include <boost/test/unit_test_log.hpp>
// #include <boost/test/unit_test_result.hpp>

#include <test/test_assert.h>  /* This header must go last */

using boost::unit_test_framework::test_suite;


BEGIN_NCBI_SCOPE
using namespace objects;

// Forward declarations


//////////////////////////////////////////////////////////////////////////////
///
static CSeq_id_Handle id_orig = CSeq_id_Handle::GetGiHandle(32324);
static CSeq_id_Handle id_patched = CSeq_id_Handle::GetGiHandle(52256);   
static CSeq_id_Handle id_empty;

const ISeq_id_Translator& GetSeqIdTranslator()
{
    static auto_ptr<ISeq_id_Translator> s_Translator;
    if (!s_Translator.get()) {
        CSeq_id_Translator_Simple* translator = new CSeq_id_Translator_Simple;
        s_Translator.reset(translator);
        translator->AddMapEntry(id_orig,id_patched);
    }
    return *s_Translator;
}


void Test_SeqIdTranslator()
{
    const ISeq_id_Translator& translator = GetSeqIdTranslator();
    
    BOOST_CHECK( translator.TranslateToOrig(id_patched) == id_orig );
    BOOST_CHECK( translator.TranslateToPatched(id_orig) == id_patched);
    BOOST_CHECK( translator.TranslateToPatched(id_patched) == id_empty );
}

void Test_PatchSeqId_SeqId()
{
    const ISeq_id_Translator& translator = GetSeqIdTranslator();

    CConstRef<CSeq_id> patched = id_patched.GetSeqId();
    CRef<CSeq_id> orig = PatchSeqId_Copy(*patched, translator);
    BOOST_CHECK(orig->Equals(*id_orig.GetSeqId()));
}

void Test_PatchSeqId_SeqIds()
{
    const ISeq_id_Translator& translator = GetSeqIdTranslator();
    typedef vector<CRef<CSeq_id> > TSeqIds;
    TSeqIds ids;
    ids.push_back(Ref(new CSeq_id(id_patched.AsString())));
    ids.push_back(Ref(new CSeq_id("3"))); 
    ids.push_back(Ref(new CSeq_id("4"))); 

    PatchSeqIds(ids, translator);

    BOOST_CHECK(ids[0]->Equals(*id_orig.GetSeqId()));
    BOOST_CHECK(ids[1]->Equals(*CSeq_id_Handle::GetGiHandle(3).GetSeqId()));
    BOOST_CHECK(ids[2]->Equals(*CSeq_id_Handle::GetGiHandle(4).GetSeqId()));
}

END_NCBI_SCOPE

/////////////////////////////////////////////////////////////////////////////
//  MAIN

USING_NCBI_SCOPE;

CNcbiOfstream out("res.xml"); //TODO

test_suite* init_unit_test_suite(int argc, char * argv[])
{    
#if BOOST_VERSION >= 103300
    typedef boost::unit_test_framework::unit_test_log_t TLog; 
    TLog& log = boost::unit_test_framework::unit_test_log;
    log.set_stream(out);    
    log.set_format(boost::unit_test_framework::XML);
    log.set_threshold_level(boost::unit_test_framework::log_test_suites);
#else
    typedef boost::unit_test_framework::unit_test_log TLog; 
    TLog& log = TLog::instance();
    log.set_log_stream(out);    
    log.set_log_format("XML");
    log.set_log_threshold_level(boost::unit_test_framework::log_test_suites);
#endif

    //boost::unit_test_framework::unit_test_result::set_report_format("XML");
    test_suite* test = BOOST_TEST_SUITE("Seq id patcher Unit Test.");

    test_suite* idtr_suite = BOOST_TEST_SUITE("Simple SeqIdTranslator");
    idtr_suite->add(BOOST_TEST_CASE(ncbi::Test_SeqIdTranslator));
    test->add(idtr_suite);

    test_suite* patch_suite = BOOST_TEST_SUITE("PatchSeqId");
    idtr_suite->add(BOOST_TEST_CASE(ncbi::Test_PatchSeqId_SeqId));
    idtr_suite->add(BOOST_TEST_CASE(ncbi::Test_PatchSeqId_SeqIds));
    test->add(patch_suite);

    return test;
}

/*
* ===========================================================================
* $Log$
* Revision 1.4  2005/12/05 17:07:49  ucko
* Add support for Boost 1.33.x, which is now installed on Solaris 10.
*
* Revision 1.3  2005/09/01 15:28:23  didenko
* Added a test for patching a container of CSeq_id
*
* Revision 1.2  2005/08/31 19:36:44  didenko
* Reduced the number of objects copies which are being created while doing PatchSeqIds
*
* Revision 1.1  2005/08/25 14:55:54  didenko
* Added test for seqid patcher
*
*
* ===========================================================================
*/

