#ifndef READER_ID1__HPP_INCLUDED
#define READER_ID1__HPP_INCLUDED

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
*
*  Author:  Anton Butanaev, Eugene Vasilchenko
*
*  File Description: Data reader from ID1
*
*/

#include <objmgr/reader.hpp>

BEGIN_NCBI_SCOPE

class CConn_ServiceStream;

BEGIN_SCOPE(objects)

class CID1server_back;
class CID1server_maxcomplex;
class CID1blob_info;

class NCBI_XOBJMGR_EXPORT CId1Reader : public CReader
{
public:
    CId1Reader(TConn noConn = 5);
    ~CId1Reader();

    virtual void RetrieveSeqrefs(TSeqrefs& sr,
                                 const CSeq_id& seqId,
                                 TConn conn);

    CRef<CTSE_Info> GetMainBlob(const CSeqref& seqref, TConn conn);
    CRef<CSeq_annot_SNP_Info> GetSNPAnnot(const CSeqref& seqref, TConn conn);

    virtual TConn GetParallelLevel(void) const;
    virtual void SetParallelLevel(TConn noConn);
    virtual void Reconnect(TConn conn);

    int GetVersion(const CSeqref& seqref,
                   TConn conn);

protected:
    CConn_ServiceStream* x_GetConnection(TConn conn);
    CConn_ServiceStream* x_NewConnection(void);

    virtual void x_ReadBlob(CID1server_back& id1_reply,
                            const CSeqref& seqref, TConn conn);
    virtual void x_ReadBlob(CID1server_back& id1_reply,
                            const CSeqref& seqref, CNcbiIstream& in);

    void x_SendRequest(const CSeqref& seqref,
                       CConn_ServiceStream* stream,
                       bool is_snp);
    void x_SetParams(const CSeqref& seqref,
                     CID1server_maxcomplex& params,
                     bool is_snp);
    void x_UpdateVersion(CSeqref& seqref,
                         const CID1blob_info& info);

    CRef<CSeq_annot_SNP_Info> x_ReceiveSNPAnnot(CConn_ServiceStream* stream);

private:

    void x_RetrieveSeqrefs(TSeqrefs& sr,
                           const CSeq_id &seqId,
                           CConn_ServiceStream* stream);
    void x_RetrieveSeqrefs(TSeqrefs& sr,
                           int gi,
                           CConn_ServiceStream* stream);
    int x_ResolveSeq_id_to_gi(const CSeq_id& seqId,
                              CConn_ServiceStream* stream);

    CRef<CTSE_Info> x_ReceiveMainBlob(CConn_ServiceStream* stream);

    vector<CConn_ServiceStream *> m_Pool;
};



END_SCOPE(objects)
END_NCBI_SCOPE


/*
* $Log$
* Revision 1.21  2003/10/08 14:16:12  vasilche
* Added version of blobs loaded from ID1.
*
* Revision 1.20  2003/10/01 18:06:58  kuznets
* x_ReceiveSNPAnnot made public
*
* Revision 1.19  2003/10/01 16:54:50  kuznets
* Relaxed scope restriction of CId1Reader::x_SendRequest() (private->protected)
*
* Revision 1.18  2003/09/30 19:38:26  vasilche
* Added support for cached id1 reader.
*
* Revision 1.17  2003/09/30 16:21:59  vasilche
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
* Revision 1.16  2003/06/02 16:01:36  dicuccio
* Rearranged include/objects/ subtree.  This includes the following shifts:
*     - include/objects/alnmgr --> include/objtools/alnmgr
*     - include/objects/cddalignview --> include/objtools/cddalignview
*     - include/objects/flat --> include/objtools/flat
*     - include/objects/objmgr/ --> include/objmgr/
*     - include/objects/util/ --> include/objmgr/util/
*     - include/objects/validator --> include/objtools/validator
*
* Revision 1.15  2003/04/24 16:12:37  vasilche
* Object manager internal structures are splitted more straightforward.
* Removed excessive header dependencies.
*
* Revision 1.14  2003/04/15 14:24:07  vasilche
* Changed CReader interface to not to use fake streams.
*
* Revision 1.13  2003/03/26 16:11:06  vasilche
* Removed redundant const modifier from integral return types.
*
* Revision 1.12  2003/03/01 22:26:07  kimelman
* performance fixes
*
* Revision 1.11  2003/02/04 16:02:22  dicuccio
* Moved headers unecessarily included here into the .cpp file
*
* Revision 1.10  2002/12/26 20:51:35  dicuccio
* Added Win32 export specifier
*
* Revision 1.9  2002/07/08 20:50:56  grichenk
* Moved log to the end of file
* Replaced static mutex (in CScope, CDataSource) with the mutex
* pool. Redesigned CDataSource data locking.
*
* Revision 1.8  2002/05/06 03:30:36  vakatov
* OM/OM1 renaming
*
* Revision 1.7  2002/05/03 21:28:02  ucko
* Introduce T(Signed)SeqPos.
*
* Revision 1.6  2002/03/27 20:22:32  butanaev
* Added connection pool.
*
* Revision 1.5  2002/03/26 18:48:31  butanaev
* Fixed bug not deleting streambuf.
*
* Revision 1.4  2002/03/21 19:14:52  kimelman
* GB related bugfixes
*
* Revision 1.3  2002/03/21 01:34:51  kimelman
* gbloader related bugfixes
*
* Revision 1.2  2002/03/20 04:50:35  kimelman
* GB loader added
*
* Revision 1.1  2002/01/11 19:04:03  gouriano
* restructured objmgr
*
* Revision 1.6  2001/12/13 00:20:55  kimelman
* bugfixes:
*
* Revision 1.5  2001/12/12 21:43:47  kimelman
* more meaningfull Compare
*
* Revision 1.4  2001/12/10 20:07:24  butanaev
* Code cleanup.
*
* Revision 1.3  2001/12/07 21:25:19  butanaev
* Interface development, code beautyfication.
*
* Revision 1.2  2001/12/07 17:02:06  butanaev
* Fixed includes.
*
* Revision 1.1  2001/12/07 16:11:45  butanaev
* Switching to new reader interfaces.
*
* Revision 1.2  2001/12/06 18:06:22  butanaev
* Ported to linux.
*
* Revision 1.1  2001/12/06 14:35:23  butanaev
* New streamable interfaces designed, ID1 reimplemented.
*
*/

#endif // READER_ID1__HPP_INCLUDED
