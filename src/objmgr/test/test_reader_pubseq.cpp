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

#include <iostream>
#include <serial/serial.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqloc/Textseq_id.hpp>
#include <serial/objistrasnb.hpp>
#include <serial/objostrasn.hpp>
#include <objects/objmgr/reader_pubseq.hpp>
#include <objects/objmgr/impl/seqref_pubseq.hpp>

#include <connect/ncbi_util.h>
#include <connect/ncbi_core_cxx.hpp>

USING_NCBI_SCOPE;
USING_SCOPE(objects);
using namespace std;

int main()
{
    for(int k = 0; k < 10; ++k) {
        cout << "K: " << k << endl;

        CPubseqReader reader;
        CSeq_id seqId;
        seqId.SetGi(156895+k-1);

        vector< CRef<CSeqref> > sr;
        reader.RetrieveSeqrefs(sr, seqId);
        ITERATE ( vector< CRef<CSeqref> >, i, sr ) {
            const CPubseqSeqref& seqRef =
                dynamic_cast<const CPubseqSeqref&>(**i);

            cout << "gi: " << seqId.GetGi() << " SatKey=" << seqRef.SatKey() << " Flag=" << seqRef.Flag() << endl;
      
            CPubseqSeqref::TBlobClass cl = 0;
            int count = 0;
            for ( CRef<CBlobSource> bs(seqRef.GetBlobSource(0, 0, cl));
                  bs->HaveMoreBlobs(); ++count) {
                CRef<CBlob> blob(bs->RetrieveBlob());
                cout << "Class=" << blob->Class() << " Descr=" << blob->Descr() << endl;
                CRef<CSeq_entry> se(blob->Seq_entry());
                if (!se) {
                    cout << "blob is not available\n";
                    continue;
                }
                if(0){
                    ofstream ofs("/dev/null");
                    //ostream &o = ofs;
                    ostream &o = cout;
                    CObjectOStreamAsn oos(o);
                    oos << *se;
                    o << endl;
                }
            }
            cout << "gi: " << seqId.GetGi() << " " << count << " blobs" << endl;
        }
    }
    return 0;
}

/*
* $Log$
* Revision 1.5  2003/04/15 14:23:11  vasilche
* Added missing includes.
*
* Revision 1.4  2002/07/22 22:49:05  kimelman
* test fixes for confidential data retrieval
*
* Revision 1.3  2002/05/06 03:28:52  vakatov
* OM/OM1 renaming
*
* Revision 1.2  2002/04/12 14:52:34  butanaev
* Typos fixed, code cleanup.
*
* Revision 1.1  2002/04/08 21:45:58  butanaev
* Added test for pubseq reader.
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

