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
* Author:  Alex Astashyn
*
* File Description:
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbi_system.hpp>
#include <corelib/test_boost.hpp>
#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objtools/data_loaders/genbank/gbloader.hpp>
#include <objtools/hgvs/hgvs_parser.hpp>

#include <boost/test/parameterized_test.hpp>

#include <objects/seqalign/Seq_align.hpp>
#include <objects/seqalign/Seq_align_set.hpp>

USING_NCBI_SCOPE;

/**
 * Either test that an hgvs expression is parsed with no exception, or
 * or check that the hgvs-name of an existing variation-feature, when parsed,
 * results in exact same variation-feature.
 */
class CTestCase
{
public:
    CTestCase(const string& line, CRef<CHgvsParser> parser, CNcbiOstream& out)
      : m_feat(NULL)
      , m_parser(parser)
      , m_out(out)
    {
        LOG_POST("Creating test-case for expression");
        string s1("");
        string s2("");
        //line = "hgvs_expression [#comment [#_THROWS:exception_string]]"
        //where exception-string is error-code of CHgvsParserException,
        //or any arbitrary text if we are expecting CException
        NStr::SplitInTwo(line, "#", m_expr, s1);
        NStr::SplitInTwo(s1, "#", m_comment,s2);

        NStr::SplitInTwo(s2, ":", s1, m_throw_str);
        if(s1 != "_THROWS") {
            m_throw_str = "";
        }
        NStr::TruncateSpacesInPlace(m_expr);
        NStr::TruncateSpacesInPlace(m_comment);
        NStr::TruncateSpacesInPlace(m_throw_str);

    }

    CTestCase(const CSeq_feat& feat, CRef<CHgvsParser> parser, CNcbiOstream& out)
      : m_expr("")
      , m_feat(&feat)
      , m_parser(parser)
      , m_out(out)
    {
        LOG_POST("Creating test-case for feature");
        m_expr = m_feat->GetData().GetVariation().GetName();
        m_comment = m_feat->GetComment();
        m_throw_str = "";
    }

    void operator()()
    {
        CRef<CSeq_feat> feat;
        string exception_str;
        string err_code;

        try {
            LOG_POST("Parsing " << m_expr);
            feat = m_parser->AsVariationFeat(m_expr);
            LOG_POST("Parsed " << m_expr);
            LOG_POST(m_expr << "\t" << m_comment << "\t" << m_throw_str << "\t" << "OK");

            if(m_throw_str != "") {
                BOOST_REQUIRE_THROW(feat.Reset(), CException); //this will not throw
            }


        } catch(CException& e) {
            LOG_POST(m_expr << "\t" << m_comment << "\t" << m_throw_str << "\t" << e.GetMsg());


            if(m_throw_str != "") {
                if(e.GetErrCodeString() == m_throw_str) {
                    BOOST_REQUIRE_THROW(NCBI_RETHROW_SAME(e, ""), CException);
                } else {
                    BOOST_CHECK_MESSAGE(false, "Caught wrong exception");
                }
            } else {
                BOOST_REQUIRE_NO_THROW(NCBI_RETHROW_SAME(e, ""));
            }
        }

        if(!feat.IsNull()) {
            feat->SetComment(m_comment);
        }

        if(!m_feat.IsNull()) {
            //if we are checking against reference variation-feats, compare the one
            //that we just created with the reference one
            if(!m_feat->Equals(*feat)) {
                NcbiCerr << "Expected\n" << MSerial_AsnText << *m_feat;
                NcbiCerr << "Produced\n" << MSerial_AsnText << *feat;
                NCBITEST_CHECK(false);
            }
        } else if(!feat.IsNull()) {
            //we are creating new reference features - just dump it
            LOG_POST("Dumping feat");
            m_out << MSerial_AsnText << *feat;
            LOG_POST("Dumped feat");
        }
    }

    string m_expr;
    string m_comment;
    string m_throw_str;
    bool m_do_remap;

    CConstRef<CSeq_feat> m_feat;
    CRef<CScope> m_scope;
    CRef<CHgvsParser> m_parser;
    CNcbiOstream& m_out;
};

CRef<CSeq_loc> CreateLoc(const string& id, TSeqPos from = kInvalidSeqPos, TSeqPos to = kInvalidSeqPos, ENa_strand strand = eNa_strand_unknown)
{
    CRef<CSeq_loc> loc(new CSeq_loc);
    if(from == kInvalidSeqPos) {
        loc->SetWhole().Set(id);
    } else {
        loc->SetInt().SetId().Set(id);
        loc->SetInt().SetFrom(from);
        loc->SetInt().SetTo(to);
        loc->SetStrand(strand);
    }
    return loc;
}

NCBITEST_INIT_TREE()
{
    const CArgs& args = CNcbiApplication::Instance()->GetArgs();
    CRef<CObjectManager> obj_mgr = CObjectManager::GetInstance();
    CGBDataLoader::RegisterInObjectManager(*obj_mgr);
    CRef<CScope> scope(new CScope(*obj_mgr));
    scope->AddDefaults();
    CRef<CHgvsParser> parser(new CHgvsParser(*scope));
    CException::SetStackTraceLevel(eDiag_Trace);

    LOG_POST("Initialized...");


    CNcbiIstream& istr = args["in"].AsInputFile();
    CNcbiOstream& ostr = args["out"].AsOutputFile();


    if(!args["asn"]) {
        LOG_POST("Creating reference ASN.1 variation feats");
        bool off = false;
        string line_str;
        while(NcbiGetlineEOL(istr, line_str)) {
            if(NStr::StartsWith(line_str, "#") || line_str.empty()) {
                if(line_str == "#OFF") {
                    off = true;
                } else if (line_str == "#ON") {
                    off = false;
                } else if(line_str == "#EXIT") {
                    break;
                } else {
                    ;
                }
                continue;
            }
            if(off) {
                continue;
            }

            boost::unit_test::framework::master_test_suite().add(
                    BOOST_TEST_CASE(CTestCase(line_str, parser, ostr)));
        }
    } else {
        LOG_POST("Testing reference ASN.1");
        CNcbiIstream& istr = args["in"].AsInputFile();
        while(1) {
            CRef<CSeq_feat> feat(new CSeq_feat);
            try {
                istr >> MSerial_AsnText >> *feat;
            } catch (CEofException e) {
                break;
            }
            boost::unit_test::framework::master_test_suite().add(
                    BOOST_TEST_CASE(CTestCase(*feat, parser, ostr)));

        }
    }
}


NCBITEST_AUTO_INIT()
{

}

NCBITEST_AUTO_FINI()
{

}


NCBITEST_INIT_CMDLINE(arg_desc)
{
    arg_desc->AddFlag
        ("asn",
         "Input is the text ASN.1 of reference Variation-feats; "
         "otherwise assumed to be text-file of hgvs expressions");

    arg_desc->AddDefaultKey
        ("in",
         "input",
         "Text file of hgvs expressions",
         CArgDescriptions::eInputFile,
         "-",
         CArgDescriptions::fPreOpen);

    arg_desc->AddDefaultKey
        ("out",
         "output",
         "Text file of hgvs expressions; used in conjunction with -in",
         CArgDescriptions::eOutputFile,
         "-",
         CArgDescriptions::fPreOpen);
}
