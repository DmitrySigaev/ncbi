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
* Author:
*           Andrei Gourianov
*
* File Description:
*           1.1 Basic tests
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.13  2003/04/29 19:51:14  vasilche
* Fixed interaction of Data Loader garbage collector and TSE locking mechanism.
* Made some typedefs more consistent.
*
* Revision 1.12  2003/04/24 20:48:47  vasilche
* Added missing header.
*
* Revision 1.11  2003/03/27 21:54:58  grichenk
* Renamed test applications and makefiles, updated references
*
* Revision 1.10  2002/12/26 16:39:24  vasilche
* Object manager class CSeqMap rewritten.
*
* Revision 1.9  2002/11/04 21:29:14  grichenk
* Fixed usage of const CRef<> and CRef<> constructor
*
* Revision 1.8  2002/10/23 12:43:31  gouriano
* comment out unused parameters
*
* Revision 1.7  2002/06/04 17:18:33  kimelman
* memory cleanup :  new/delete/Cref rearrangements
*
* Revision 1.6  2002/05/14 20:06:28  grichenk
* Improved CTSE_Info locking by CDataSource and CDataLoader
*
* Revision 1.5  2002/05/06 03:28:53  vakatov
* OM/OM1 renaming
*
* Revision 1.4  2002/04/02 14:51:52  gouriano
* *** empty log message ***
*
* Revision 1.3  2002/03/20 17:03:51  gouriano
* *** empty log message ***
*
* Revision 1.2  2002/03/01 20:12:17  gouriano
* *** empty log message ***
*
* Revision 1.1  2002/03/01 19:41:35  gouriano
* *** empty log message ***
*
*
* ===========================================================================
*/

#include <corelib/ncbiapp.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objects/seq/Bioseq.hpp>

#include <objects/objmgr/object_manager.hpp>
#include <objects/objmgr/scope.hpp>

BEGIN_NCBI_SCOPE
using namespace objects;


//===========================================================================
// CTestDataLoader

class CTestDataLoader : public CDataLoader
{
public:
    CTestDataLoader(const string& loader_name) : CDataLoader( loader_name) {}
    virtual bool GetRecords(const CHandleRangeMap& /*hrmap*/,
        const EChoice /*choice*/) { return false; }
    virtual bool DropTSE(const CSeq_entry* /*sep*/)  {return false;}
    virtual void GC(void) {return;}
};

//===========================================================================
// CTestApplication

class CTestApplication : public CNcbiApplication
{
public:
    CRef<CSeq_entry> CreateTestEntry(void);
    virtual int Run( void);
};

CRef<CSeq_entry> CTestApplication::CreateTestEntry(void)
//---------------------------------------------------------------------------
{
    CRef<CSeq_entry> entry(new CSeq_entry);
    entry->SetSeq();
    return entry;
}

int CTestApplication::Run()
//---------------------------------------------------------------------------
{
    string name1("DL_1"), name2("DL_2");

NcbiCout << "1.1.1 Creating CScope ==============================" << NcbiEndl;
{
    {
        CRef< CObjectManager> pOm(new CObjectManager);
        {
            CTestDataLoader *pLoader1 = new CTestDataLoader( name1);
            pOm->RegisterDataLoader( *pLoader1, CObjectManager::eNonDefault);
            CTestDataLoader *pLoader2 = new CTestDataLoader( name2);
            pOm->RegisterDataLoader( *pLoader2, CObjectManager::eDefault);

            // scope in CRef container
            CRef< CScope> pScope1(new CScope(*pOm));
            pScope1->AddDefaults(); // loader 2 added
            pScope1->AddDataLoader( name1);
            // scope on the stack
            CScope scope2(*pOm);
            scope2.AddDefaults(); // loader 2 added
            // scope on the heap
            CScope* pScope3 = new CScope(*pOm);
            pScope3->AddDefaults(); // loader 2 added
            delete pScope3; //  loader 2 alive
        }
        // scopes deleted, all dataloaders alive
    }
// objmgr deleted, all dataloaders deleted
}
NcbiCout << "1.1.2 Adding Seq_entry to the scope=================" << NcbiEndl;
{
    {
        CRef< CObjectManager> pOm(new CObjectManager);
        {
            // 3 scopes
            CRef< CScope> pScope1(new CScope(*pOm));
            CScope scope2(*pOm);
            CScope* pScope3 = new CScope(*pOm);
            CRef< CSeq_entry> pEntry = CreateTestEntry();
            // add entry to all scopes
            pScope1->AddTopLevelSeqEntry( *pEntry);
            scope2.AddTopLevelSeqEntry( *pEntry);
            pScope3->AddTopLevelSeqEntry( *pEntry);

            delete pScope3; // data source and seq_entry alive
        }
        // scopes deleted, seq_entry and data source deleted
    }
// objmgr deleted
}
NcbiCout << "1.1.3 Handling Data loader==========================" << NcbiEndl;
{
    {
        CRef< CObjectManager> pOm(new CObjectManager);
        {
            CTestDataLoader *pLoader1 = new CTestDataLoader( name1);
            CScope* pScope1 = new CScope(*pOm);
            pScope1->AddDefaults(); // nothing added
            // must throw an exception: dataloader1 not found
            try {
                pScope1->AddDataLoader( name1);
            }
            catch (exception& e) {
                NcbiCout << e.what() << NcbiEndl;
            }
            pOm->RegisterDataLoader( *pLoader1, CObjectManager::eNonDefault);
            pScope1->AddDefaults(); // nothing added
            pScope1->AddDataLoader( name1); // ok
            // must fail - dataloader1 is in use
            if (pOm->RevokeDataLoader( name1))
            {
                NcbiCout << "ERROR: RevokeDataLoader has succeeded" << NcbiEndl;
            }
            delete pScope1; // loader1 alive
            // delete dataloader1
            pOm->RevokeDataLoader( name1); // ok
            // must throw an exception - dataloader1 not registered
            try {
                pOm->RevokeDataLoader( name1);
            }
            catch (exception& e) {
                NcbiCout << e.what() << NcbiEndl;
            }
        }
    }
}
NcbiCout << "==================================================" << NcbiEndl;
NcbiCout << "Test completed" << NcbiEndl;
    return 0;
}

END_NCBI_SCOPE

USING_NCBI_SCOPE;

//===========================================================================
// entry point

int main( int argc, const char* argv[])
{
    return CTestApplication().AppMain( argc, argv, 0, eDS_Default, 0);
}

