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
* Author:  Aleksey Grichenko
*
* File Description:
*   Test for CSeqVector_CI
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbi_system.hpp>
#include <corelib/ncbi_limits.hpp>

// Objects includes
#include <objects/seqloc/Seq_id.hpp>

// Object manager includes
#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/seq_vector.hpp>
#include <objmgr/seq_vector_ci.hpp>
#include <objmgr/gbloader.hpp>

#include <objmgr/bioseq_handle.hpp>

BEGIN_NCBI_SCOPE
using namespace objects;


class CTestApp : public CNcbiApplication
{
public:
    virtual void Init(void);
    virtual int  Run (void);

private:
    void x_TestIterate(CSeqVector_CI& vit,
                       TSeqPos start,
                       TSeqPos stop);
    void x_TestGetData(CSeqVector_CI& vit,
                       TSeqPos start,
                       TSeqPos stop);
    void x_TestVector(TSeqPos start,
                      TSeqPos stop);

    CSeqVector m_Vect;
    string m_RefBuf;
};


void CTestApp::Init(void)
{
    // Prepare command line descriptions
    //

    // Create
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // GI to fetch
    arg_desc->AddDefaultKey("gi", "SeqEntryID",
                            "GI id of the Seq-Entry to fetch",
                            CArgDescriptions::eInteger,
                            "29791621");
    arg_desc->AddDefaultKey("cycles", "RandomCycles",
                            "repeat random test 'cycles' times",
                            CArgDescriptions::eInteger, "20");
    arg_desc->AddDefaultKey("seed", "RandomSeed",
                            "Force random seed",
                            CArgDescriptions::eInteger, "0");

    // Program description
    string prog_description = "Test for CSeqVector_CI\n";
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              prog_description, false);

    // Pass argument descriptions to the application
    //

    SetupArgDescriptions(arg_desc.release());
}


void CTestApp::x_TestIterate(CSeqVector_CI& vit,
                             TSeqPos start,
                             TSeqPos stop)
{
    if (start != kInvalidSeqPos) {
        vit.SetPos(start);
    }
    else {
        start = vit.GetPos();
    }

    // cout << "Testing iterator, " << start << " - " << stop << "... ";

    if (start > stop) {
        // Moving down
        if ( !vit ) {
            --vit;
        }
        for ( ; vit.GetPos() > stop; --vit) {
            if (*vit != m_RefBuf[vit.GetPos()]) {
                cout << endl << "ERROR: Test failed at position "
                    << vit.GetPos() << endl;
                throw runtime_error("Test failed");
            }
        }
    }
    else {
        // Moving up
        if ( !vit )
            ++vit;
        if (stop > m_Vect.size()) {
            stop = m_Vect.size();
        }
        for ( ; vit.GetPos() < stop; ++vit) {
            if (*vit != m_RefBuf[vit.GetPos()]) {
                cout << endl << "ERROR: Test failed at position "
                    << vit.GetPos() << endl;
                throw runtime_error("Test failed");
            }
        }
    }

    // cout << "OK" << endl;
}


void CTestApp::x_TestGetData(CSeqVector_CI& vit,
                             TSeqPos start,
                             TSeqPos stop)
{
    if (start == kInvalidSeqPos) {
        start = vit.GetPos();
    }

    if (start > stop)
        swap(start, stop);

    // cout << "Testing GetSeqData(" << start << ", " << stop << ")... " << endl;

    string buf;
    vit.GetSeqData(start, stop, buf);
    if (start >= stop  &&  buf.size() > 0) {
        cout << endl << "ERROR: Test failed -- invalid data length" << endl;
        throw runtime_error("Test failed");
    }

    if (stop > m_Vect.size())
        stop = m_Vect.size();

    if (buf != m_RefBuf.substr(start, stop - start)) {
        cout << endl << "ERROR: Test failed -- invalid data" << endl;
        throw runtime_error("Test failed");
    }
    // cout << "OK" << endl;
}


void CTestApp::x_TestVector(TSeqPos start,
                            TSeqPos stop)
{
    if (start >= m_Vect.size()) {
        start = m_Vect.size() - 1;
    }
    if (stop >= m_Vect.size()) {
        stop = m_Vect.size() - 1;
    }
    // cout << "Testing vector[], " << start << " - " << stop << "... ";

    if (start > stop) {
        // Moving down
        for (TSeqPos i = start; i > stop; --i) {
            if (m_Vect[i] != m_RefBuf[i]) {
                cout << endl << "ERROR: Test failed at position " << i << endl;
                throw runtime_error("Test failed");
            }
        }
    }
    else {
        // Moving up
        if (stop > m_Vect.size()) {
            stop = m_Vect.size();
        }
        for (TSeqPos i = start; i < stop; ++i) {
            if (m_Vect[i] != m_RefBuf[i]) {
                cout << endl << "ERROR: Test failed at position " << i << endl;
                throw runtime_error("Test failed");
            }
        }
    }

    // cout << "OK" << endl;
}


TSeqPos random(TSeqPos size)
{
    double r = rand();
    r /= RAND_MAX;
    r *= size;
    return r;
}


int CTestApp::Run(void)
{
    // Process command line args: get GI to load
    const CArgs& args = GetArgs();

    // GI with many segments of different sizes.
    int gi = args["gi"].AsInteger(); // 29791621;

    CRef<CObjectManager> pOm(new CObjectManager);
    pOm->RegisterDataLoader(
        *new CGBDataLoader("ID", 0, 2),
        CObjectManager::eDefault);

    CScope scope(*pOm);
    scope.AddDefaults();

    CSeq_id id;
    id.SetGi(gi);
    CBioseq_Handle handle = scope.GetBioseqHandle(id);

    // Check if the handle is valid
    if ( !handle ) {
        cout << "Can not find gi " << gi << endl;
        return 0;
    }

    // Create seq-vector
    m_Vect = handle.GetSeqVector(CBioseq_Handle::eCoding_Iupac);
    // Prepare reference data
    m_Vect.GetSeqData(0, m_Vect.size(), m_RefBuf);

    CSeqVector_CI vit = CSeqVector_CI(m_Vect);

    string buf;

    // start > stop test
    cout << "Testing empty range (start > stop)... ";
    vit.GetSeqData(m_Vect.size(), 0, buf);
    if (buf.size() != 0) {
        cout << endl << "ERROR: Test failed -- got non-empty data" << endl;
        throw runtime_error("Empty range test failed");
    }
    cout << "OK" << endl;

    // stop > length test
    cout << "Testing past-end read (stop > size)... ";
    vit.GetSeqData(
        max<TSeqPos>(m_Vect.size(), 100) - 100, m_Vect.size() + 1, buf);
    if (buf != m_RefBuf.substr(max<TSeqPos>(m_Vect.size(), 100) - 100, 100)) {
        cout << "ERROR: GetSeqData() failed -- invalid data" << endl;
        throw runtime_error("Past-end read test failed");
    }
    cout << "OK" << endl;

    buf = ""; // .erase();

    // Compare iterator with GetSeqData()
    // Not using x_TestIterate() to also check operator bool()
    cout << "Testing basic iterator... ";
    string::iterator c = m_RefBuf.begin();
    for (vit.SetPos(0); vit; ++vit, ++c) {
        if (c == m_RefBuf.end()  ||  *vit != *c) {
            cout << "ERROR: Invalid data at " << vit.GetPos() << endl;
            throw runtime_error("Basic iterator test failed");
        }
    }
    if (c != m_RefBuf.end()) {
        cout << "ERROR: Invalid data length" << endl;
        throw runtime_error("Basic iterator test failed");
    }
    cout << "OK" << endl;

    // Partial retrieval with GetSeqData() test, limit to 2000 bases
    cout << "Testing partial retrieval... ";
    for (int i = max<int>(1, m_Vect.size() / 2 - 2000);
        i < m_Vect.size() / 2; ++i) {
        x_TestGetData(vit, i, m_Vect.size() - i);
    }
    cout << "OK" << endl;

    // Butterfly test
    cout << "Testing butterfly reading... ";
    for (int i = 1; i < m_Vect.size() / 2; ++i) {
        if (m_Vect[i] != m_RefBuf[i]) {
            cout << "ERROR: Butterfly test failed at position " << i << endl;
            throw runtime_error("Butterfly read test failed");
        }
    }
    cout << "OK" << endl;

    TSeqPos pos1 = 0;
    TSeqPos pos2 = 0;
    TSeqPos pos3 = 0;
    TSeqPos pos4 = 0;

    {{
        const CSeqMap& smap = handle.GetSeqMap();
        CSeqMap_CI map_it = smap.begin_resolved(&scope);
        if ( map_it ) {
            // Should happen for any non-empty sequence
            pos2 = map_it.GetEndPosition();
            ++map_it;
            if ( map_it ) {
                // Should happen for most segmented sequences
                pos3 = map_it.GetEndPosition();
                ++map_it;
                if ( map_it ) {
                    // May happen for some segmented sequences
                    pos4 = map_it.GetEndPosition();
                }
            }
        }
    }}

    // ========= Iterator tests ==========
    cout << "Testing iterator - single segment... ";
    // Single segment test, start from the middle of the first segment
    vit = CSeqVector_CI(m_Vect, (pos1 + pos2) / 2);
    // Back to the first segment start
    x_TestIterate(vit, kInvalidSeqPos, pos1);
    // Forward to the first segment end
    x_TestIterate(vit, kInvalidSeqPos, pos2);
    // Back to the first segment start again
    x_TestIterate(vit, kInvalidSeqPos, pos1);
    cout << "OK" << endl;

    // Try to run multi-segment tests
    if ( pos3 ) {
        cout << "Testing iterator - multiple segments... ";
        // Start from the middle of the second segment
        vit = CSeqVector_CI(m_Vect, (pos2 + pos3) / 2);
        // Back to the first segment start
        x_TestIterate(vit, kInvalidSeqPos, pos1);
        // Forward to the second or third segment end
        x_TestIterate(vit, kInvalidSeqPos, pos4 ? pos4 : pos3);
        // Back to the first segment start again
        x_TestIterate(vit, kInvalidSeqPos, pos1);
        cout << "OK" << endl;
    }

    // ========= GetSeqData() tests ==========
    cout << "Testing GetSeqData() - single segment... ";
    // Single segment test, start from the middle of the first segment
    vit = CSeqVector_CI(m_Vect, (pos1 + pos2) / 2);
    // Back to the first segment start
    x_TestGetData(vit, kInvalidSeqPos, pos1);
    // Forward to the first segment end
    x_TestGetData(vit, kInvalidSeqPos, pos2);
    // Back to the first segment start again
    x_TestGetData(vit, kInvalidSeqPos, pos1);
    cout << "OK" << endl;

    // Try to run multi-segment tests
    if ( pos3 ) {
        cout << "Testing GetSeqData() - multiple segments... ";
        // Start from the middle of the second segment
        vit = CSeqVector_CI(m_Vect, (pos2 + pos3) / 2);
        // Back to the first segment start
        x_TestGetData(vit, kInvalidSeqPos, pos1);
        // Forward to the second or third segment end
        x_TestGetData(vit, kInvalidSeqPos, pos4 ? pos4 : pos3);
        // Back to the first segment start again
        x_TestGetData(vit, kInvalidSeqPos, pos1);
        cout << "OK" << endl;
    }

    // ========= CSeqVector[] tests ==========
    cout << "Testing operator[] - single segment... ";
    // Single segment test, start from the middle of the first segment
    // Back to the first segment start
    x_TestVector((pos1 + pos2) / 2, pos1);
    // Forward to the first segment end
    x_TestVector(pos1, pos2);
    // Back to the first segment start again
    x_TestVector(pos2, pos1);
    cout << "OK" << endl;

    // Try to run multi-segment tests
    if ( pos3 ) {
        cout << "Testing operator[] - multiple segments... ";
        // Start from the middle of the second segment
        // Back to the first segment start
        x_TestVector((pos2 + pos3) / 2, pos1);
        // Forward to the second or third segment end
        x_TestVector(pos1, pos4 ? pos4 : pos3);
        // Back to the first segment start again
        x_TestVector(pos4 ? pos4 : pos3, pos1);
        cout << "OK" << endl;
    }

    // Random access tests
    cout << "Testing random access" << endl;
    unsigned int rseed = args["seed"].AsInteger();
    int cycles = args["cycles"].AsInteger();
    if (rseed == 0) {
        rseed = (unsigned)time( NULL );
    }
    cout << "Testing random reading (seed: " << rseed
         << ", cycles: " << cycles << ")... " << endl;
    srand(rseed);
    vit = CSeqVector_CI(m_Vect);
    for (int i = 0; i < cycles; ++i) {
        TSeqPos start = random(m_Vect.size());
        TSeqPos stop = random(m_Vect.size());
        switch (i % 3) {
        case 0:
            x_TestIterate(vit, start, stop);
            break;
        case 1:
            x_TestGetData(vit, start, stop);
            break;
        case 2:
            x_TestVector(start, stop);
            break;
        }
    }
    cout << "OK" << endl;

    NcbiCout << "All tests passed" << NcbiEndl;
    return 0;
}


END_NCBI_SCOPE



/////////////////////////////////////////////////////////////////////////////
//  MAIN


USING_NCBI_SCOPE;

int main(int argc, const char* argv[])
{
    return CTestApp().AppMain(argc, argv);
}

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.3  2003/07/09 18:49:33  grichenk
* Added arguments (seed and cycles), default random cycles set to 20.
*
* Revision 1.2  2003/06/26 17:02:52  grichenk
* Simplified output, decreased number of cycles.
*
* Revision 1.1  2003/06/24 19:50:02  grichenk
* Added test for CSeqVector_CI
*
*
* ===========================================================================
*/

