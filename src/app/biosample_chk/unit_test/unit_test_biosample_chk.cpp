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
* Author:  Colleen Bollin, NCBI
*
* File Description:
*   Unit tests for biosample_chk.
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>

#include <corelib/ncbi_system.hpp>

// This macro should be defined before inclusion of test_boost.hpp in all
// "*.cpp" files inside executable except one. It is like function main() for
// non-Boost.Test executables is defined only in one *.cpp file - other files
// should not include it. If NCBI_BOOST_NO_AUTO_TEST_MAIN will not be defined
// then test_boost.hpp will define such "main()" function for tests.
//
// Usually if your unit tests contain only one *.cpp file you should not
// care about this macro at all.
//
//#define NCBI_BOOST_NO_AUTO_TEST_MAIN

// This header must be included before all Boost.Test headers if there are any
#include <corelib/test_boost.hpp>

#include <objects/seqfeat/BioSource.hpp>
#include <objects/seqfeat/Org_ref.hpp>
#include <objects/seqfeat/OrgName.hpp>
#include <objects/seqfeat/OrgMod.hpp>
#include <objects/seqfeat/SubSource.hpp>
#include <common/ncbi_export.h>
#include <objtools/unit_test_util/unit_test_util.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


void CheckDiffs(const TFieldDiffList& expected, const TFieldDiffList& observed)
{
    BOOST_CHECK_EQUAL(expected.size(), observed.size());
    TFieldDiffList::const_iterator ex = expected.begin();
    TFieldDiffList::const_iterator ob = observed.begin();
    while (ex != expected.end() && ob != observed.end()) {
        string ex_str = (*ex)->GetFieldName() + ":" 
                        + (*ex)->GetSrcVal() + ":"
                        + (*ex)->GetSampleVal();
        string ob_str = (*ob)->GetFieldName() + ":" 
                        + (*ob)->GetSrcVal() + ":"
                        + (*ob)->GetSampleVal();
        BOOST_CHECK_EQUAL(ex_str, ob_str);
        ex++;
        ob++;
    }
    while (ex != expected.end()) {
        string ex_str = (*ex)->GetFieldName() + ":" 
                        + (*ex)->GetSrcVal() + ":"
                        + (*ex)->GetSampleVal();
        BOOST_CHECK_EQUAL(ex_str, "");
        ex++;
    }
    while (ob != observed.end()) {
        string ob_str = (*ob)->GetFieldName() + ":" 
                        + (*ob)->GetSrcVal() + ":"
                        + (*ob)->GetSampleVal();
        BOOST_CHECK_EQUAL("", ob_str);
        ob++;
    }
}


BOOST_AUTO_TEST_CASE(Test_GetBiosampleDiffs)
{
    CRef<CBioSource> test_src(new CBioSource());
    CRef<CBioSource> test_sample(new CBioSource());

    test_src->SetOrg().SetTaxname("A");
    test_sample->SetOrg().SetTaxname("B");

    TFieldDiffList expected;
    expected.push_back(CRef<CFieldDiff>(new CFieldDiff("Organism Name", "A", "B")));
    TFieldDiffList diff_list = test_src->GetBiosampleDiffs(*test_sample);
    CheckDiffs(expected, diff_list);

    // ignore differences that can be autofixed
    unit_test_util::SetSubSource(*test_src, CSubSource::eSubtype_sex, "male");
    unit_test_util::SetSubSource(*test_sample, CSubSource::eSubtype_sex, "m");
    diff_list = test_src->GetBiosampleDiffs(*test_sample);
    CheckDiffs(expected, diff_list);

    // ignore differences that are allowed if BioSample is missing a value
    unit_test_util::SetSubSource(*test_src, CSubSource::eSubtype_chromosome, "1");
    diff_list = test_src->GetBiosampleDiffs(*test_sample);
    CheckDiffs(expected, diff_list);

    // ignore certain diffs in Org-ref
    unit_test_util::SetOrgMod(*test_sample, COrgMod::eSubtype_pathovar, "path");
    unit_test_util::SetOrgMod(*test_sample, COrgMod::eSubtype_specimen_voucher, "spec");
    expected.push_back(CRef<CFieldDiff>(new CFieldDiff("specimen-voucher", "", "spec")));
    diff_list = test_src->GetBiosampleDiffs(*test_sample);
    CheckDiffs(expected, diff_list);


    try {
        test_src->UpdateWithBioSample(*test_sample, false);
        BOOST_CHECK_EQUAL("Expected exception to be thrown", "Exception not thrown!");
    } catch (CException& e) {
        BOOST_CHECK_EQUAL(e.GetMsg(), "Conflicts found");
    }

    try {
        test_src->UpdateWithBioSample(*test_sample, true);
        BOOST_CHECK_EQUAL(test_src->GetOrg().GetTaxname(), "B");
        BOOST_CHECK_EQUAL(test_src->GetSubtype().front()->GetName(), "1");
        BOOST_CHECK_EQUAL(test_src->GetSubtype().back()->GetName(), "male");
        BOOST_CHECK_EQUAL(test_src->GetOrg().GetOrgname().GetMod().front()->GetSubname(), "path");
        BOOST_CHECK_EQUAL(test_src->GetOrg().GetOrgname().GetMod().back()->GetSubname(), "spec");
    } catch (CException& e) {
        BOOST_CHECK_EQUAL("Unexpected exception", e.GetMsg());
    }

}


END_SCOPE(objects)
END_NCBI_SCOPE
