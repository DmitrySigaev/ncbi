/*  $Id$
* ===========================================================================
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
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <iostream>
#include <serial/serial.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqloc/Textseq_id.hpp>
#include <serial/objistrasnb.hpp>
#include <serial/objostrasn.hpp>
#include <objtools/data_loaders/genbank/readers/id1/reader_id1.hpp>
#include <objtools/data_loaders/genbank/request_result.hpp>
#include <objmgr/impl/tse_info.hpp>

#include <connect/ncbi_util.h>
#include <connect/ncbi_core_cxx.hpp>

USING_NCBI_SCOPE;
USING_SCOPE(objects);
using namespace std;

int main()
{
    //export CONN_DEBUG_PRINTOUT=data
    //CORE_SetLOG(LOG_cxx2c());
    //SetDiagTrace(eDT_Enable);
    //SetDiagPostLevel(eDiag_Info);
    //SetDiagPostFlag(eDPF_All);

    //test_id1_calls();

    int gi = 5;

    CId1Reader reader;
    for(int k = 0; k < 500; k++) {
        CSeq_id seq_id;
        seq_id.SetGi(gi);
        CReaderRequestResult request;
        CLoadLockBlob_ids ids(request, seq_id);
        reader.ResolveSeq_id(ids, seq_id, 0);
        ITERATE ( CLoadInfoBlob_ids, i, *ids ) {
            const CBlob_id& blob_id = i->first;
            cout << "K: " << k << " " << gi << endl;

            CLoadLockBlob blob(request, blob_id);
            reader.LoadBlob(request, blob_id);
            if ( !blob.IsLoaded() ) {
                cout << "blob is not available\n";
                continue;
            }
            cout << "K: " <<k<<" " <<gi<<" "<<1<<" blobs" << endl;
        }
    }
    return 0;
}

/*
* $Log$
* Revision 1.4  2004/08/04 14:55:18  vasilche
* Changed TSE locking scheme.
* TSE cache is maintained by CDataSource.
* Added ID2 reader.
* CSeqref is replaced by CBlobId.
*
* Revision 1.3  2004/05/21 21:42:52  gorelenk
* Added PCH ncbi_pch.hpp
*
* Revision 1.2  2003/12/30 22:14:45  vasilche
* Updated genbank loader and readers plugins.
*
* Revision 1.1  2003/12/16 17:51:22  kuznets
* Code reorganization
*
* Revision 1.16  2003/11/26 17:56:04  vasilche
* Implemented ID2 split in ID1 cache.
* Fixed loading of splitted annotations.
*
* Revision 1.15  2003/09/30 16:22:05  vasilche
* Updated internal object manager classes to be able to load ID2 data.
* SNP blobs are loaded as ID2 split blobs - readers convert them automatically.
* Scope caches results of requests for data to data loaders.
* Optimized CSeq_id_Handle for gis.
* Optimized bioseq lookup in scope.
* Reduced object allocations in annotation iterators.
* CScope is allowed to be destroyed before other objects using this scope are
* deleted (feature iterators, bioseq handles etc).
* Optimized lookup for matching Seq-ids in CSeq_id_Mapper.
* Added 'adaptive' option to objmgr_demo application.
*
* Revision 1.14  2003/08/14 20:05:20  vasilche
* Simple SNP features are stored as table internally.
* They are recreated when needed using CFeat_CI.
*
* Revision 1.13  2003/06/02 16:06:39  dicuccio
* Rearranged src/objects/ subtree.  This includes the following shifts:
*     - src/objects/asn2asn --> arc/app/asn2asn
*     - src/objects/testmedline --> src/objects/ncbimime/test
*     - src/objects/objmgr --> src/objmgr
*     - src/objects/util --> src/objmgr/util
*     - src/objects/alnmgr --> src/objtools/alnmgr
*     - src/objects/flat --> src/objtools/flat
*     - src/objects/validator --> src/objtools/validator
*     - src/objects/cddalignview --> src/objtools/cddalignview
* In addition, libseq now includes six of the objects/seq... libs, and libmmdb
* replaces the three libmmdb? libs.
*
* Revision 1.12  2003/04/15 14:23:11  vasilche
* Added missing includes.
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
* Revision 1.8  2002/05/06 03:28:52  vakatov
* OM/OM1 renaming
*
* Revision 1.7  2002/03/27 20:23:50  butanaev
* Added connection pool.
*
* Revision 1.6  2002/03/26 18:48:59  butanaev
* Fixed bug not deleting streambuf.
*
* Revision 1.5  2002/03/26 17:17:04  kimelman
* reader stream fixes
*
* Revision 1.4  2002/03/25 17:49:13  kimelman
* ID1 failure handling
*
* Revision 1.3  2002/03/21 19:14:55  kimelman
* GB related bugfixes
*
* Revision 1.2  2002/01/23 21:59:34  grichenk
* Redesigned seq-id handles and mapper
*
* Revision 1.1  2002/01/11 19:06:27  gouriano
* restructured objmgr
*
* Revision 1.3  2001/12/10 20:08:02  butanaev
* Code cleanup.
*
* Revision 1.2  2001/12/07 21:25:00  butanaev
* Interface development, code beautyfication.
*
* Revision 1.1  2001/12/07 16:43:35  butanaev
* Switching to new reader interfaces.
*
* Revision 1.6  2001/12/06 20:37:05  butanaev
* Fixed timeout problem.
*
* Revision 1.5  2001/12/06 18:06:22  butanaev
* Ported to linux.
*
* Revision 1.4  2001/12/06 14:35:22  butanaev
* New streamable interfaces designed, ID1 reimplemented.
*
* Revision 1.3  2001/11/29 21:24:03  butanaev
* Classes working with PUBSEQ transferred to separate lib. Code cleanup.
* Test app redesigned.
*
* Revision 1.2  2001/10/11 15:47:34  butanaev
* Loader from ID1 implemented.
*
* Revision 1.1  2001/09/10 20:03:04  butanaev
* Initial checkin.
*/

