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
* Authors:  Andrei Gourianov
*
* File Description:
*   Object Manager test: multiple threads working with annotations
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.6  2003/05/06 16:52:55  vasilche
* Added 'pause' argument.
*
* Revision 1.5  2003/04/24 16:12:39  vasilche
* Object manager internal structures are splitted more straightforward.
* Removed excessive header dependencies.
*
* Revision 1.4  2003/04/15 14:23:11  vasilche
* Added missing includes.
*
* Revision 1.3  2003/03/18 21:48:33  grichenk
* Removed obsolete class CAnnot_CI
*
* Revision 1.2  2003/01/29 17:34:18  vasilche
* Added test code for negative strand CSeqVector.
*
* Revision 1.1  2002/12/26 16:39:24  vasilche
* Object manager class CSeqMap rewritten.
*
* Revision 1.4  2002/12/06 15:36:03  grichenk
* Added overlap type for annot-iterators
*
* Revision 1.3  2002/07/22 22:49:05  kimelman
* test fixes for confidential data retrieval
*
* Revision 1.2  2002/05/06 03:28:53  vakatov
* OM/OM1 renaming
*
* Revision 1.1  2002/04/30 19:04:05  gouriano
* multi-threaded data retrieval test
*
*
* ===========================================================================
*/

#include <corelib/ncbiapp.hpp>
#include <corelib/ncbi_system.hpp>
#include <connect/ncbi_util.h>

#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqloc/Seq_loc.hpp>

#include <objects/objmgr/object_manager.hpp>
#include <objects/objmgr/scope.hpp>
#include <objects/objmgr/seq_vector.hpp>
#include <objects/objmgr/seqdesc_ci.hpp>
#include <objects/objmgr/feat_ci.hpp>
#include <objects/objmgr/align_ci.hpp>
#include <objects/objmgr/gbloader.hpp>
#include <connect/ncbi_core_cxx.hpp>
#include <connect/ncbi_util.h>

#include <test/test_assert.h>  /* This header must go last */


BEGIN_NCBI_SCOPE
using namespace objects;

static CFastMutex    s_GlobalLock;

// GIs to process
#if 0
    const int g_gi_from = 156894;
    const int g_gi_to   = 156896;
#elif 0
    const int g_gi_from = 156201;
    const int g_gi_to   = 156203;
#else
    const int g_gi_from = 156000;
    const int g_gi_to   = 157000;
#endif

/////////////////////////////////////////////////////////////////////////////
//
//  Test application
//

class CTestOM : public CNcbiApplication
{
protected:
    virtual int Run(void);
    virtual void Init(void);
    virtual void Exit(void);

    virtual bool Thread_Run(int idx);
    virtual bool TestApp_Args( CArgDescriptions& args);
    virtual bool TestApp_Init(void);
    virtual bool TestApp_Exit(void);

    CRef<CScope> m_Scope;
    CRef<CObjectManager> m_ObjMgr;

    map<int, int> m_mapGiToDesc;
    map<int, int> m_mapGiToFeat0;
    map<int, int> m_mapGiToFeat1;

    int m_gi_from;
    int m_gi_to;
    int m_pause;
};


/////////////////////////////////////////////////////////////////////////////

int CTestOM::Run(void)
{
    if ( !Thread_Run(0) )
        return 1;
    else
        return 0;
}

void CTestOM::Init(void)
{
    // Prepare command line descriptions
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // Let test application add its own arguments
    TestApp_Args(*arg_desc);

    string prog_description =
        "test";
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              prog_description, false);

    SetupArgDescriptions(arg_desc.release());

    if ( !TestApp_Init() )
        THROW1_TRACE(runtime_error, "Cannot init test application");
}

void CTestOM::Exit(void)
{
    if ( !TestApp_Exit() )
        THROW1_TRACE(runtime_error, "Cannot exit test application");
}

bool CTestOM::Thread_Run(int idx)
{
// initialize scope
    CScope scope(*m_ObjMgr);
    scope.AddDefaults();

    int from, to;
    // make them go in opposite directions
    if (idx % 2 == 0) {
        from = m_gi_from;
        to = m_gi_to;
    } else {
        from = m_gi_to;
        to = m_gi_from;
    }
    int delta = (to > from) ? 1 : -1;
    int pause = m_pause;

    for ( int i = from, end = to+delta; i != end; i += delta ) {
        if ( i != from && pause ) {
            SleepSec(pause);
        }
        try {
// load sequence
            CSeq_id sid;
            sid.SetGi(i);
            CBioseq_Handle handle = scope.GetBioseqHandle(sid);
            if (!handle) {
                LOG_POST("T" << idx << ": gi = " << i << ": INVALID HANDLE");
                continue;
            }

            int count = 0, count_prev;

// check seqvector
            if ( 0 ) {{
                string buff;
                CSeqVector sv =
                    handle.GetSeqVector(CBioseq_Handle::eCoding_Iupac, 
                                        CBioseq_Handle::eStrand_Plus);

                int start = max(0, int(sv.size()-60));
                int stop  = sv.size();

                sv.GetSeqData(start, stop, buff);
                cout << "POS: " << buff << endl;

                sv = handle.GetSeqVector(CBioseq_Handle::eCoding_Iupac, 
                                         CBioseq_Handle::eStrand_Minus);
                sv.GetSeqData(sv.size()-stop, sv.size()-start, buff);
                cout << "NEG: " << buff << endl;
            }}

// enumerate descriptions
            // Seqdesc iterator
            for (CSeqdesc_CI desc_it(handle); desc_it;  ++desc_it) {
                count++;
            }
// verify result
            {
                CFastMutexGuard guard(s_GlobalLock);
                if (m_mapGiToDesc.find(i) != m_mapGiToDesc.end()) {
                    count_prev = m_mapGiToDesc[i];
                    _ASSERT( m_mapGiToDesc[i] == count);
                } else {
                    m_mapGiToDesc[i] = count;
                }
            }

// enumerate features
            CSeq_loc loc;
            loc.SetWhole(sid);
            count = 0;
            if ( idx%2 == 0 ) {
                for (CFeat_CI feat_it(scope, loc,
                                      CSeqFeatData::e_not_set,
                                      SAnnotSelector::eOverlap_Intervals,
                                      CAnnotTypes_CI::eResolve_All);
                     feat_it;  ++feat_it) {
                    count++;
                }
// verify result
                {
                    CFastMutexGuard guard(s_GlobalLock);
                    if (m_mapGiToFeat0.find(i) != m_mapGiToFeat0.end()) {
                        count_prev = m_mapGiToFeat0[i];
                        _ASSERT( m_mapGiToFeat0[i] == count);
                    } else {
                        m_mapGiToFeat0[i] = count;
                    }
                }
            }
            else {
                for (CFeat_CI feat_it(handle, 0, 0, CSeqFeatData::e_not_set);
                     feat_it;  ++feat_it) {
                    count++;
                }
// verify result
                {
                    CFastMutexGuard guard(s_GlobalLock);
                    if (m_mapGiToFeat1.find(i) != m_mapGiToFeat1.end()) {
                        count_prev = m_mapGiToFeat1[i];
                        _ASSERT( m_mapGiToFeat1[i] == count);
                    } else {
                        m_mapGiToFeat1[i] = count;
                    }
                }
            }
        } catch (exception& e) {
            LOG_POST("T" << idx << ": gi = " << i 
                     << ": EXCEPTION = " << e.what());
        }
        scope.ResetHistory();
    }
    return true;
}

bool CTestOM::TestApp_Args( CArgDescriptions& args)
{
    args.AddDefaultKey
        ("fromgi", "FromGi",
         "Process sequences in the interval FROM this Gi",
         CArgDescriptions::eInteger, NStr::IntToString(g_gi_from));
    args.AddDefaultKey
        ("togi", "ToGi",
         "Process sequences in the interval TO this Gi",
         CArgDescriptions::eInteger, NStr::IntToString(g_gi_to));
    args.AddDefaultKey
        ("pause", "Pause",
         "Pause between requests in seconds",
         CArgDescriptions::eInteger, "0");
    return true;
}

bool CTestOM::TestApp_Init(void)
{
    CORE_SetLOCK(MT_LOCK_cxx2c());
    CORE_SetLOG(LOG_cxx2c());

    const CArgs& args = GetArgs();
    m_gi_from = args["fromgi"].AsInteger();
    m_gi_to   = args["togi"].AsInteger();
    m_pause   = args["pause"].AsInteger();

    NcbiCout << "Testing ObjectManager ("
        << "gi from "
        << m_gi_from << " to " << m_gi_to << ")..." << NcbiEndl;

    m_ObjMgr = new CObjectManager;
    m_ObjMgr->RegisterDataLoader(*new CGBDataLoader("ID"),CObjectManager::eDefault);


    // Scope shared by all threads
/*
    m_Scope = new CScope(*m_ObjMgr);
    m_Scope->AddDefaults();
*/
    return true;
}

bool CTestOM::TestApp_Exit(void)
{
    NcbiCout << " Passed" << NcbiEndl << NcbiEndl;

/*
    map<int, int>::iterator it;
    for (it = m_mapGiToDesc.begin(); it != m_mapGiToDesc.end(); ++it) {
        LOG_POST(
            "gi = "         << it->first
            << ": desc = "  << it->second
            << ", feat0 = " << m_mapGiToFeat0[it->first]
            << ", feat1 = " << m_mapGiToFeat1[it->first]
            );
    }
*/
    return true;
}
END_NCBI_SCOPE


/////////////////////////////////////////////////////////////////////////////
//  MAIN

USING_NCBI_SCOPE;

int main(int argc, const char* argv[])
{
    return CTestOM().AppMain(argc, argv, 0, eDS_Default, 0);
}
