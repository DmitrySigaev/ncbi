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
* Author:  Eugene Vasilchenko
*
* File Description:
*   Unit tests for WGS data loader
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <sra/data_loaders/wgs/wgsloader.hpp>
#include <sra/readers/sra/wgsread.hpp>
#include <sra/readers/ncbi_traces_path.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/bioseq_handle.hpp>
#include <objmgr/seq_vector.hpp>
#include <objmgr/align_ci.hpp>
#include <objmgr/graph_ci.hpp>
#include <objmgr/seqdesc_ci.hpp>
#include <objtools/data_loaders/genbank/gbloader.hpp>
#include <objects/general/Dbtag.hpp>
#include <objects/seqalign/seqalign__.hpp>
#include <objects/seq/seq__.hpp>
#include <objects/seqset/seqset__.hpp>
#include <corelib/ncbi_system.hpp>
#include <objtools/readers/idmapper.hpp>
#include <serial/iterator.hpp>
#include <objmgr/util/sequence.hpp>

#include <corelib/test_boost.hpp>

USING_NCBI_SCOPE;
USING_SCOPE(objects);

#define PILEUP_NAME_SUFFIX " pileup graphs"

#define REPORT_GENERAL_ID_ERROR 2 // 0 - never, 1 - every 5th day, 2 - always
NCBI_PARAM_DECL(int, WGS, REPORT_GENERAL_ID_ERROR);
NCBI_PARAM_DEF(int, WGS, REPORT_GENERAL_ID_ERROR, 0);

static int GetReportGeneralIdError(void)
{
    return NCBI_PARAM_TYPE(WGS, REPORT_GENERAL_ID_ERROR)().Get();
}

enum EMasterDescrType
{
    eWithoutMasterDescr,
    eWithMasterDescr
};

static EMasterDescrType s_master_descr_type = eWithoutMasterDescr;

void sx_InitGBLoader(CObjectManager& om)
{
    CGBDataLoader* gbloader = dynamic_cast<CGBDataLoader*>
        (CGBDataLoader::RegisterInObjectManager(om, 0, om.eNonDefault).GetLoader());
    _ASSERT(gbloader);
    gbloader->SetAddWGSMasterDescr(s_master_descr_type == eWithMasterDescr);
}

CRef<CObjectManager> sx_GetEmptyOM(void)
{
    SetDiagPostLevel(eDiag_Info);
    CRef<CObjectManager> om = CObjectManager::GetInstance();
    CObjectManager::TRegisteredNames names;
    om->GetRegisteredNames(names);
    ITERATE ( CObjectManager::TRegisteredNames, it, names ) {
        om->RevokeDataLoader(*it);
    }
    return om;
}

CRef<CObjectManager> sx_InitOM(EMasterDescrType master_descr_type)
{
    CRef<CObjectManager> om = sx_GetEmptyOM();
    s_master_descr_type = master_descr_type;
    CWGSDataLoader::RegisterInObjectManager(*om, CObjectManager::eDefault, 88);
    if ( master_descr_type == eWithMasterDescr ) {
        sx_InitGBLoader(*om);
    }
    return om;
}

CBioseq_Handle sx_LoadFromGB(const CBioseq_Handle& bh)
{
    CRef<CObjectManager> om = CObjectManager::GetInstance();
    sx_InitGBLoader(*om);
    CScope scope(*om);
    scope.AddDataLoader("GBLOADER");
    return scope.GetBioseqHandle(*bh.GetSeqId());
}

string sx_GetASN(const CBioseq_Handle& bh)
{
    CNcbiOstrstream str;
    if ( bh ) {
        str << MSerial_AsnText << *bh.GetCompleteBioseq();
    }
    return CNcbiOstrstreamToString(str);
}

CRef<CSeq_id> sx_ExtractGeneralId(CBioseq& seq)
{
    NON_CONST_ITERATE ( CBioseq::TId, it, seq.SetId() ) {
        CRef<CSeq_id> id = *it;
        if ( id->Which() == CSeq_id::e_General ) {
            seq.SetId().erase(it);
            return id;
        }
    }
    return null;
}

CRef<CDelta_ext> sx_ExtractDelta(CBioseq& seq)
{
    CRef<CDelta_ext> delta;
    CSeq_inst& inst = seq.SetInst();
    if ( inst.IsSetExt() ) {
        CSeq_ext& ext = inst.SetExt();
        if ( ext.IsDelta() ) {
            delta = &ext.SetDelta();
            ext.Reset();
        }
    }
    return delta;
}

bool sx_EqualGeneralId(const CSeq_id& gen1, const CSeq_id& gen2)
{
    if ( gen1.Equals(gen2) ) {
        return true;
    }
    // allow partial match in Dbtag.db like "WGS:ABBA" vs "WGS:ABBA01"
    if ( !gen1.IsGeneral() || !gen2.IsGeneral() ) {
        return false;
    }
    const CDbtag& id1 = gen1.GetGeneral();
    const CDbtag& id2 = gen2.GetGeneral();
    if ( !id1.GetTag().Equals(id2.GetTag()) ) {
        return false;
    }
    const string& db1 = id1.GetDb();
    const string& db2 = id2.GetDb();
    size_t len = min(db1.size(), db2.size());
    if ( db1.substr(0, len) != db2.substr(0, len) ) {
        return false;
    }
    if ( db1.size() <= len+2 && db2.size() <= len+2 ) {
        return true;
    }
    return false;
}

bool sx_EqualDelta(const CDelta_ext& delta1, const CDelta_ext& delta2)
{
    if ( delta1.Equals(delta2) ) {
        return true;
    }
    // slow check for possible different representation of delta sequence
    CScope scope(*CObjectManager::GetInstance());
    CRef<CBioseq> seq1(new CBioseq);
    CRef<CBioseq> seq2(new CBioseq);
    seq1->SetId().push_back(Ref(new CSeq_id("lcl|1")));
    seq2->SetId().push_back(Ref(new CSeq_id("lcl|2")));
    seq1->SetInst().SetRepr(CSeq_inst::eRepr_delta);
    seq2->SetInst().SetRepr(CSeq_inst::eRepr_delta);
    seq1->SetInst().SetMol(CSeq_inst::eMol_na);
    seq2->SetInst().SetMol(CSeq_inst::eMol_na);
    seq1->SetInst().SetExt().SetDelta(const_cast<CDelta_ext&>(delta1));
    seq2->SetInst().SetExt().SetDelta(const_cast<CDelta_ext&>(delta2));
    CBioseq_Handle bh1 = scope.AddBioseq(*seq1);
    CBioseq_Handle bh2 = scope.AddBioseq(*seq2);
    CSeqVector sv1 = bh1.GetSeqVector(CBioseq_Handle::eCoding_Ncbi);
    CSeqVector sv2 = bh2.GetSeqVector(CBioseq_Handle::eCoding_Ncbi);
    if ( sv1.size() != sv2.size() ) {
        return false;
    }
    for ( CSeqVector_CI it1 = sv1.begin(), it2 = sv2.begin();
          it1 && it2; ++it1, ++it2 ) {
        if ( it1.IsInGap() != it2.IsInGap() ) {
            return false;
        }
        if ( *it1 != *it2 ) {
            return false;
        }
    }
    return true;
}

bool sx_Equal(const CBioseq_Handle& bh1, const CBioseq_Handle& bh2)
{
    CConstRef<CBioseq> seq1 = bh1.GetCompleteBioseq();
    CConstRef<CBioseq> seq2 = bh2.GetCompleteBioseq();
    if ( seq1->Equals(*seq2) ) {
        return true;
    }
    CRef<CBioseq> nseq1(SerialClone(*seq1));
    CRef<CBioseq> nseq2(SerialClone(*seq2));
    CRef<CSeq_id> gen1 = sx_ExtractGeneralId(*nseq1);
    CRef<CSeq_id> gen2 = sx_ExtractGeneralId(*nseq2);
    CRef<CDelta_ext> delta1 = sx_ExtractDelta(*nseq1);
    CRef<CDelta_ext> delta2 = sx_ExtractDelta(*nseq2);
    if ( !nseq1->Equals(*nseq2) ) {
        NcbiCout << "Sequences do not match:\n"
                 << "Seq1: " << MSerial_AsnText << *seq1
                 << "Seq2: " << MSerial_AsnText << *seq2;
        return false;
    }
    bool has_delta_error = false;
    bool has_id_error = false;
    bool report_id_error = false;
    if ( !delta1 != !delta2 ) {
        has_delta_error = true;
    }
    else if ( delta1 && !sx_EqualDelta(*delta1, *delta2) ) {
        has_delta_error = true;
    }
    if ( !gen1 != !gen2 ) {
        // one has general id but another hasn't
        has_id_error = report_id_error = true;
    }
    else if ( gen1 && !sx_EqualGeneralId(*gen1, *gen2) ) {
        has_id_error = true;
        // report general id error only when day of month divides by 5
        int report = GetReportGeneralIdError();
        if ( (report > 1) ||
             (report == 1 && CTime(CTime::eCurrent).Day() % 5 == 0) ) {
            report_id_error = true;
        }
    }
    if ( !has_delta_error && !has_id_error ) {
        return true;
    }
    if ( has_delta_error ) {
        NcbiCout << "Delta sequences do not match:\n";
        NcbiCout << "Seq1: ";
        if ( !delta1 ) {
            NcbiCout << "null\n";
        }
        else {
            NcbiCout << MSerial_AsnText << *delta1;
        }
        NcbiCout << "Seq2: ";
        if ( !delta2 ) {
            NcbiCout << "null\n";
        }
        else {
            NcbiCout << MSerial_AsnText << *delta2;
        }
    }
    if ( has_id_error ) {
        NcbiCout << "General ids do no match:\n";
        NcbiCout << "Id1: ";
        if ( !gen1 ) {
            NcbiCout << "null\n";
        }
        else {
            NcbiCout << MSerial_AsnText << *gen1;
        }
        NcbiCout << "Id2: ";
        if ( !gen2 ) {
            NcbiCout << "null\n";
        }
        else {
            NcbiCout << MSerial_AsnText << *gen2;
        }
    }
    return !has_delta_error && !has_id_error;
}

bool sx_EqualToGB(const CBioseq_Handle& bh)
{
    BOOST_REQUIRE(bh);
    CBioseq_Handle gb_bh = sx_LoadFromGB(bh);
    BOOST_REQUIRE(gb_bh);
    return sx_Equal(bh, gb_bh);
}

void sx_CheckNames(CScope& scope, const CSeq_loc& loc,
                   const string& name)
{
    SAnnotSelector sel;
    sel.SetSearchUnresolved();
    sel.SetCollectNames();
    CAnnotTypes_CI it(CSeq_annot::C_Data::e_not_set, scope, loc, &sel);
    CAnnotTypes_CI::TAnnotNames names = it.GetAnnotNames();
    ITERATE ( CAnnotTypes_CI::TAnnotNames, i, names ) {
        if ( i->IsNamed() ) {
            NcbiCout << "Named annot: " << i->GetName()
                     << NcbiEndl;
        }
        else {
            NcbiCout << "Unnamed annot"
                     << NcbiEndl;
        }
    }
    //NcbiCout << "Checking for name: " << name << NcbiEndl;
    BOOST_CHECK_EQUAL(names.count(name), 1u);
    if ( names.size() == 2 ) {
        BOOST_CHECK_EQUAL(names.count(name+PILEUP_NAME_SUFFIX), 1u);
    }
    else {
        BOOST_CHECK_EQUAL(names.size(), 1u);
    }
}

void sx_CheckSeq(CScope& scope,
                 const CSeq_id_Handle& main_idh,
                 const CSeq_id& id)
{
    CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(id);
    if ( idh == main_idh ) {
        return;
    }
    const CBioseq_Handle::TId& ids = scope.GetIds(idh);
    BOOST_REQUIRE_EQUAL(ids.size(), 1u);
    BOOST_CHECK_EQUAL(ids[0], idh);
    CBioseq_Handle bh = scope.GetBioseqHandle(idh);
    BOOST_REQUIRE(bh);
    BOOST_CHECK_EQUAL(bh.GetState(), 0);
}

BOOST_AUTO_TEST_CASE(FetchSeq1)
{
    CRef<CObjectManager> om = sx_InitOM(eWithoutMasterDescr);

    string id = "AAAA01000102";
    CScope scope(*om);
    scope.AddDefaults();

    CRef<CSeq_id> seqid(new CSeq_id(id));
    CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(*seqid);
    CBioseq_Handle bh = scope.GetBioseqHandle(idh);
    BOOST_REQUIRE(bh);
    BOOST_CHECK_EQUAL(bh.GetState(), CBioseq_Handle::fState_suppress_perm);
    if ( 0 ) {
        NcbiCout << MSerial_AsnText << *bh.GetCompleteObject();
    }
    BOOST_CHECK(CSeqdesc_CI(bh, CSeqdesc::e_Molinfo, 1));
    BOOST_CHECK(!CSeqdesc_CI(bh, CSeqdesc::e_Pub, 1));
    BOOST_CHECK(sx_EqualToGB(bh));
}

BOOST_AUTO_TEST_CASE(FetchSeq2)
{
    CRef<CObjectManager> om = sx_InitOM(eWithoutMasterDescr);

    string id = "AAAA02000102.1";
    CScope scope(*om);
    scope.AddDefaults();

    CRef<CSeq_id> seqid(new CSeq_id(id));
    CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(*seqid);
    CBioseq_Handle bh = scope.GetBioseqHandle(idh);
    BOOST_REQUIRE(bh);
    BOOST_CHECK_EQUAL(bh.GetState(), 0);
    if ( 0 ) {
        NcbiCout << MSerial_AsnText << *bh.GetCompleteObject();
    }
    BOOST_CHECK(CSeqdesc_CI(bh, CSeqdesc::e_Molinfo, 1));
    BOOST_CHECK(!CSeqdesc_CI(bh, CSeqdesc::e_Pub, 1));
    BOOST_CHECK(sx_EqualToGB(bh));
}

BOOST_AUTO_TEST_CASE(FetchSeq3)
{
    CRef<CObjectManager> om = sx_InitOM(eWithoutMasterDescr);

    string id = "ref|NZ_AAAA01000102";
    CScope scope(*om);
    scope.AddDefaults();

    CRef<CSeq_id> seqid(new CSeq_id(id));
    CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(*seqid);
    CBioseq_Handle bh = scope.GetBioseqHandle(idh);
    BOOST_REQUIRE(bh);
    BOOST_CHECK_EQUAL(bh.GetState(), CBioseq_Handle::fState_dead);
    if ( 0 ) {
        NcbiCout << MSerial_AsnText << *bh.GetCompleteObject();
    }
    BOOST_CHECK(CSeqdesc_CI(bh, CSeqdesc::e_Molinfo, 1));
    BOOST_CHECK(!CSeqdesc_CI(bh, CSeqdesc::e_Pub, 1));
}

BOOST_AUTO_TEST_CASE(FetchSeq4)
{
    CRef<CObjectManager> om = sx_InitOM(eWithoutMasterDescr);

    string id = "ref|NZ_AAAA010000001";
    CScope scope(*om);
    scope.AddDefaults();

    CRef<CSeq_id> seqid(new CSeq_id(id));
    CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(*seqid);
    CBioseq_Handle bh = scope.GetBioseqHandle(idh);
    BOOST_CHECK(!bh);
}

BOOST_AUTO_TEST_CASE(FetchSeq5)
{
    CRef<CObjectManager> om = sx_InitOM(eWithoutMasterDescr);

    string id = "ref|NZ_AAAA0100000001";
    CScope scope(*om);
    scope.AddDefaults();

    CRef<CSeq_id> seqid(new CSeq_id(id));
    CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(*seqid);
    CBioseq_Handle bh = scope.GetBioseqHandle(idh);
    BOOST_CHECK(!bh);
}

BOOST_AUTO_TEST_CASE(FetchSeq6)
{
    CRef<CObjectManager> om = sx_InitOM(eWithoutMasterDescr);

    string id = "ref|ZZZZ01000001";
    CScope scope(*om);
    scope.AddDefaults();

    CRef<CSeq_id> seqid(new CSeq_id(id));
    CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(*seqid);
    CBioseq_Handle bh = scope.GetBioseqHandle(idh);
    BOOST_CHECK(!bh);
}


BOOST_AUTO_TEST_CASE(FetchSeq7)
{
    CRef<CObjectManager> om = sx_InitOM(eWithMasterDescr);

    string id = "AAAA01010001";
    CScope scope(*om);
    scope.AddDefaults();

    CRef<CSeq_id> seqid(new CSeq_id(id));
    CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(*seqid);
    CBioseq_Handle bh = scope.GetBioseqHandle(idh);
    BOOST_REQUIRE(bh);
    BOOST_CHECK_EQUAL(bh.GetState(), CBioseq_Handle::fState_suppress_perm);
    if ( 0 ) {
        NcbiCout << MSerial_AsnText << *bh.GetCompleteObject();
    }
    BOOST_CHECK(CSeqdesc_CI(bh, CSeqdesc::e_Molinfo, 1));
    BOOST_CHECK(CSeqdesc_CI(bh, CSeqdesc::e_Pub, 1));
}


BOOST_AUTO_TEST_CASE(FetchSeq8)
{
    CRef<CObjectManager> om = sx_InitOM(eWithMasterDescr);

    string id = "ALWZ01S31652451";
    CScope scope(*om);
    scope.AddDefaults();

    CRef<CSeq_id> seqid(new CSeq_id(id));
    CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(*seqid);
    CBioseq_Handle bh = scope.GetBioseqHandle(idh);
    BOOST_REQUIRE(bh);
    BOOST_CHECK_EQUAL(bh.GetState(), 0);
    if ( 0 ) {
        NcbiCout << MSerial_AsnText << *bh.GetCompleteObject();
    }
    BOOST_CHECK(!CSeqdesc_CI(bh, CSeqdesc::e_Title, 1));
    BOOST_CHECK(!CSeqdesc_CI(bh, CSeqdesc::e_Pub, 1));
}

BOOST_AUTO_TEST_CASE(FetchSeq9)
{
    CRef<CObjectManager> om = sx_InitOM(eWithMasterDescr);

    string id = "ALWZ0100000001";
    CScope scope(*om);
    scope.AddDefaults();

    CRef<CSeq_id> seqid(new CSeq_id(id));
    CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(*seqid);
    CBioseq_Handle bh = scope.GetBioseqHandle(idh);
    BOOST_REQUIRE(bh);
    BOOST_CHECK_EQUAL(bh.GetState(), 0);
    if ( 0 ) {
        NcbiCout << MSerial_AsnText << *bh.GetCompleteObject();
    }
    BOOST_CHECK(!CSeqdesc_CI(bh, CSeqdesc::e_Title, 1));
    BOOST_CHECK(!CSeqdesc_CI(bh, CSeqdesc::e_Pub, 1));
}

BOOST_AUTO_TEST_CASE(FetchSeq10)
{
    CRef<CObjectManager> om = sx_InitOM(eWithMasterDescr);

    string id = "ALWZ010000001";
    CScope scope(*om);
    scope.AddDefaults();

    CRef<CSeq_id> seqid(new CSeq_id(id));
    CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(*seqid);
    CBioseq_Handle bh = scope.GetBioseqHandle(idh);
    BOOST_CHECK(!bh);
}


BOOST_AUTO_TEST_CASE(FetchSeq11)
{
    CRef<CObjectManager> om = sx_InitOM(eWithoutMasterDescr);

    string id = "NZ_ACUJ01000001";
    CScope scope(*om);
    scope.AddDefaults();

    CRef<CSeq_id> seqid(new CSeq_id(id));
    CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(*seqid);
    CBioseq_Handle bh = scope.GetBioseqHandle(idh);
    BOOST_REQUIRE(bh);
    BOOST_CHECK_EQUAL(bh.GetState(), 0);
    if ( 0 ) {
        NcbiCout << MSerial_AsnText << *bh.GetCompleteObject();
    }
    BOOST_CHECK(CSeqdesc_CI(bh, CSeqdesc::e_Title, 1));
    BOOST_CHECK(!CSeqdesc_CI(bh, CSeqdesc::e_Pub, 1));
}


BOOST_AUTO_TEST_CASE(FetchSeq12)
{
    CRef<CObjectManager> om = sx_InitOM(eWithMasterDescr);

    string id = "NZ_ACUJ01000001";
    CScope scope(*om);
    scope.AddDefaults();

    CRef<CSeq_id> seqid(new CSeq_id(id));
    CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(*seqid);
    CBioseq_Handle bh = scope.GetBioseqHandle(idh);
    BOOST_REQUIRE(bh);
    BOOST_CHECK_EQUAL(bh.GetState(), 0);
    if ( 0 ) {
        NcbiCout << MSerial_AsnText << *bh.GetCompleteObject();
    }
    BOOST_CHECK(CSeqdesc_CI(bh, CSeqdesc::e_Title, 1));
    BOOST_CHECK(CSeqdesc_CI(bh, CSeqdesc::e_Pub, 1));
}


BOOST_AUTO_TEST_CASE(FetchSeq13)
{
    CRef<CObjectManager> om = sx_InitOM(eWithMasterDescr);

    string id = "NZ_ACUJ01000001.3";
    CScope scope(*om);
    scope.AddDefaults();

    CRef<CSeq_id> seqid(new CSeq_id(id));
    CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(*seqid);
    CBioseq_Handle bh = scope.GetBioseqHandle(idh);
    BOOST_REQUIRE(bh);
    BOOST_CHECK_EQUAL(bh.GetState(), 0);
    if ( 0 ) {
        NcbiCout << MSerial_AsnText << *bh.GetCompleteObject();
    }
    BOOST_CHECK(CSeqdesc_CI(bh, CSeqdesc::e_Title, 1));
    BOOST_CHECK(CSeqdesc_CI(bh, CSeqdesc::e_Pub, 1));
}


BOOST_AUTO_TEST_CASE(FetchSeq14)
{
    CRef<CObjectManager> om = sx_InitOM(eWithMasterDescr);

    string id = "NZ_ACUJ01000001.1";
    CScope scope(*om);
    scope.AddDefaults();

    CRef<CSeq_id> seqid(new CSeq_id(id));
    CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(*seqid);
    CBioseq_Handle bh = scope.GetBioseqHandle(idh);
    BOOST_CHECK(!bh);
}


BOOST_AUTO_TEST_CASE(FetchSeq15)
{
    CRef<CObjectManager> om = sx_InitOM(eWithMasterDescr);

    CRef<CScope> scope(new CScope(*om));
    scope->AddDefaults();
    scope->AddDataLoader("GBLOADER");

    CSeq_id_Handle idh = CSeq_id_Handle::GetHandle("gb|ABBA01000001.1|");
    CBioseq_Handle bsh = scope->GetBioseqHandle(idh);
    if ( !bsh ) {
        cerr << "failed to find sequence: " << idh << endl;
        CBioseq_Handle::TBioseqStateFlags flags = bsh.GetState();
        if (flags & CBioseq_Handle::fState_suppress_temp) {
            cerr << "  suppressed temporarily" << endl;
        }
        if (flags & CBioseq_Handle::fState_suppress_perm) {
            cerr << "  suppressed permanently" << endl;
        }
        if (flags & CBioseq_Handle::fState_suppress) {
            cerr << "  suppressed" << endl;
        }
        if (flags & CBioseq_Handle::fState_dead) {
            cerr << "  dead" << endl;
        }
        if (flags & CBioseq_Handle::fState_confidential) {
            cerr << "  confidential" << endl;
        }
        if (flags & CBioseq_Handle::fState_withdrawn) {
            cerr << "  withdrawn" << endl;
        }
        if (flags & CBioseq_Handle::fState_no_data) {
            cerr << "  no data" << endl;
        }
        if (flags & CBioseq_Handle::fState_conflict) {
            cerr << "  conflict" << endl;
        }
        if (flags & CBioseq_Handle::fState_not_found) {
            cerr << "  not found" << endl;
        }
        if (flags & CBioseq_Handle::fState_other_error) {
            cerr << "  other error" << endl;
        }
    }
    else {
        cerr << "found sequence: " << idh << endl;
        //cerr << MSerial_AsnText << *bsh.GetCompleteBioseq();
    }
    BOOST_REQUIRE(bsh);
    BOOST_CHECK(sx_EqualToGB(bsh));
}


static const bool s_MakeFasta = 0;


BOOST_AUTO_TEST_CASE(Scaffold2Fasta)
{
    CStopWatch sw(CStopWatch::eStart);
    CVDBMgr mgr;
    CWGSDb wgs_db(mgr, "ALWZ01");
    size_t limit_count = 30000, start_row = 1, count = 0;

    CRef<CObjectManager> om = sx_InitOM(eWithoutMasterDescr);
    CScope scope(*om);
    scope.AddDefaults();

    auto_ptr<CNcbiOstream> out;
    auto_ptr<CFastaOstream> fasta;

    if ( s_MakeFasta ) {
        string outfile_name = "out.fsa";
        out.reset(new CNcbiOfstream(outfile_name.c_str()));
        fasta.reset(new CFastaOstream(*out));
    }

    for ( CWGSScaffoldIterator it(wgs_db, start_row); it; ++it ) {
        ++count;
        if (limit_count > 0 && count > limit_count)
            break;

        //scope.ResetHistory();
        CBioseq_Handle scaffold = scope.GetBioseqHandle(*it.GetAccSeq_id());
        if ( fasta.get() ) {
            fasta->Write(scaffold);
        }
    }
    NcbiCout << "Scanned in "<<sw.Elapsed() << NcbiEndl;
}


BOOST_AUTO_TEST_CASE(Scaffold2Fasta2)
{
    CStopWatch sw(CStopWatch::eStart);
    CVDBMgr mgr;
    CWGSDb wgs_db(mgr, "ALWZ01");
    size_t limit_count = 30000, start_row = 1, count = 0;

    CRef<CObjectManager> om = sx_InitOM(eWithoutMasterDescr);
    CScope scope(*om);
    scope.AddDefaults();

    auto_ptr<CNcbiOstream> out;
    auto_ptr<CFastaOstream> fasta;

    if ( s_MakeFasta ) {
        string outfile_name = "out.fsa";
        out.reset(new CNcbiOfstream(outfile_name.c_str()));
        fasta.reset(new CFastaOstream(*out));
    }

    CScope::TIds ids;
    for ( CWGSScaffoldIterator it(wgs_db, start_row); it; ++it ) {
        ++count;
        if (limit_count > 0 && count > limit_count)
            break;

        ids.push_back(CSeq_id_Handle::GetHandle(*it.GetAccSeq_id()));
    }

    CScope::TBioseqHandles handles = scope.GetBioseqHandles(ids);
    ITERATE ( CScope::TBioseqHandles, it, handles ) {
        CBioseq_Handle scaffold = *it;
        if ( fasta.get() ) {
            fasta->Write(scaffold);
        }
    }

    NcbiCout << "Scanned in "<<sw.Elapsed() << NcbiEndl;
}


BOOST_AUTO_TEST_CASE(WithdrawnCheck)
{
    CRef<CObjectManager> om = sx_InitOM(eWithMasterDescr);
    CScope scope(*om);
    scope.AddDefaults();

    CBioseq_Handle bh;

    bh = scope.GetBioseqHandle(CSeq_id_Handle::GetHandle("AFFP02000011.1"));
    BOOST_CHECK(bh);
    BOOST_CHECK_EQUAL(bh.GetState(), 0);
    BOOST_CHECK(sx_Equal(bh, sx_LoadFromGB(bh)));

    bh = scope.GetBioseqHandle(CSeq_id_Handle::GetHandle("AFFP01000011.1"));
    BOOST_CHECK(bh);
    BOOST_CHECK_EQUAL(bh.GetState(),
                      CBioseq_Handle::fState_suppress_perm);
    BOOST_CHECK(sx_Equal(bh, sx_LoadFromGB(bh)));

    bh = scope.GetBioseqHandle(CSeq_id_Handle::GetHandle("AFFP01000012.1"));
    BOOST_CHECK(!bh);
    BOOST_CHECK_EQUAL(bh.GetState(),
                      CBioseq_Handle::fState_no_data |
                      CBioseq_Handle::fState_withdrawn);
}


#ifdef NCBI_OS_DARWIN
# define PAN1_PATH "/net/pan1"
#else
# define PAN1_PATH "//panfs/pan1"
#endif

BOOST_AUTO_TEST_CASE(TPGTest)
{
    CRef<CObjectManager> om = sx_GetEmptyOM();
    CScope scope(*om);

    string wgs_root = PAN1_PATH "/id_dumps/WGS/tmp";
    CWGSDataLoader::RegisterInObjectManager(*om,  wgs_root, vector<string>(), CObjectManager::eDefault);
    scope.AddDefaults();

    CBioseq_Handle bh;

    bh = scope.GetBioseqHandle(CSeq_id_Handle::GetHandle("DAAH01000001.1"));
    BOOST_CHECK(bh);
    bh = scope.GetBioseqHandle(CSeq_id_Handle::GetHandle("DAAG01000001.1"));
    BOOST_CHECK(bh);
    //bh = scope.GetBioseqHandle(CSeq_id_Handle::GetHandle("AGKB01000001.1"));
    //BOOST_CHECK(!bh);
    //bh = scope.GetBioseqHandle(CSeq_id_Handle::GetHandle("AAAA01000001.1"));
    //BOOST_CHECK(!bh);
}


BOOST_AUTO_TEST_CASE(FixedFileTest)
{
    CRef<CObjectManager> om = sx_GetEmptyOM();
    CScope scope(*om);

    vector<string> files;
    string wgs_root = PAN1_PATH "/id_dumps/WGS/tmp";
    files.push_back(wgs_root+"/DAAH01");
    CWGSDataLoader::RegisterInObjectManager(*om,  "", files, CObjectManager::eDefault);
    scope.AddDefaults();

    CBioseq_Handle bh;

    bh = scope.GetBioseqHandle(CSeq_id_Handle::GetHandle("DAAH01000001.1"));
    BOOST_CHECK(bh);
    bh = scope.GetBioseqHandle(CSeq_id_Handle::GetHandle("DAAG01000001.1"));
    BOOST_CHECK(!bh);
    bh = scope.GetBioseqHandle(CSeq_id_Handle::GetHandle("AGKB01000001.1"));
    BOOST_CHECK(!bh);
    bh = scope.GetBioseqHandle(CSeq_id_Handle::GetHandle("AAAA01000001.1"));
    BOOST_CHECK(!bh);
}


BOOST_AUTO_TEST_CASE(QualityTest)
{
    CRef<CObjectManager> om = sx_InitOM(eWithoutMasterDescr);
    CScope scope(*om);

    scope.AddDefaults();

    CBioseq_Handle bh;

    bh = scope.GetBioseqHandle(CSeq_id_Handle::GetHandle("ABYI02000001.1"));
    BOOST_REQUIRE(bh);
    CGraph_CI graph_it(bh, SAnnotSelector().AddNamedAnnots("Phrap Graph"));
    BOOST_CHECK_EQUAL(graph_it.GetSize(), 1u);
    CFeat_CI feat_it(bh);
    BOOST_CHECK_EQUAL(feat_it.GetSize(), 116u);
}
