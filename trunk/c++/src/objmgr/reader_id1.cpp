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
 *
 * ===========================================================================
 *
 *  Author:  Anton Butanaev, Eugene Vasilchenko
 *
 *  File Description: Data reader from ID1
 *
 */

#include <objmgr/reader_id1.hpp>

#include <objmgr/impl/seqref_id1.hpp>
#include <objmgr/impl/reader_zlib.hpp>
#include <objmgr/impl/reader_snp.hpp>

#include <corelib/ncbistre.hpp>

#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objects/seqset/Bioseq_set.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <objects/id1/id1__.hpp>

#include <serial/enumvalues.hpp>
#include <serial/iterator.hpp>
#include <serial/objistrasnb.hpp>
#include <serial/objostrasn.hpp>
#include <serial/objostrasnb.hpp>
#include <serial/serial.hpp>

#include <memory>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


bool CId1Reader::RetrieveSeqrefs(TSeqrefs& srs,
                                 const CSeq_id& seqId,
                                 TConn conn)
{
    return x_RetrieveSeqrefs(srs, seqId, conn);
}


bool CId1Reader::x_RetrieveSeqrefs(TSeqrefs& srs,
                                   const CSeq_id& seqId,
                                   TConn conn)
{
    CConn_ServiceStream* server = GetService(conn);
    auto_ptr<CObjectIStream>
        server_input(CObjectIStream::Open(eSerial_AsnBinary, *server, false));
    int gi = 0;

    if (!seqId.IsGi()) {
        {{
            CID1server_request id1_request;
            id1_request.SetGetgi().Assign(seqId);
            
            CObjectOStreamAsnBinary server_output(*server);
            server_output << id1_request;
        }}

        CID1server_back id1_reply;
        *server_input >> id1_reply;

        if (id1_reply.IsGotgi()) {
            gi = id1_reply.GetGotgi();
        }
        else {
            LOG_POST("SeqId is not found");
            seqId.WriteAsFasta(cout);
        }
    }
    else {
        gi = seqId.GetGi();
    }

    if ( !gi ) {
        return true; // no data?
    }

    {{
        CID1server_request id1_request;
        CID1server_maxcomplex& blob = id1_request.SetGetblobinfo();
        blob.SetMaxplex(eEntry_complexities_entry);
        blob.SetGi(gi);

        CObjectOStreamAsnBinary server_output(*server);
        server_output << id1_request;
    }}

    CID1server_back    id1_reply;
    *server_input >> id1_reply;

    if (id1_reply.IsGotblobinfo()) {
        const CID1blob_info& info = id1_reply.GetGotblobinfo();
        CRef<CSeqref> sr;
        sr.Reset(new CId1Seqref(*this, gi, info.GetSat(), info.GetSat_key()));
        srs.push_back(sr);
        if ( TrySNPSplit() ) {
            CSeqref::TFlags flags;
            if ( !info.IsSetExtfeatmask() ) {
                flags = CSeqref::fHasExternal | CSeqref::fPossible;
            }
            else if ( info.GetExtfeatmask() != 0 ) {
                flags = CSeqref::fHasExternal;
            }
            else {
                flags = 0;
            }
            if ( flags ) {
                sr.Reset(new CId1Seqref(*this, gi, kSat_SNP, gi));
                sr->SetFlags(flags);
                srs.push_back(sr);
            }
        }
    }
    return true;
}


CId1Seqref::CId1Seqref(CId1Reader& reader, int gi, int sat, int satkey)
    : m_Reader(reader), m_Gi(gi), m_Sat(sat), m_SatKey(satkey)
{
}


CId1Seqref::~CId1Seqref(void)
{
}


CConn_ServiceStream* CId1Seqref::x_GetService(TConn conn) const
{
    return m_Reader.GetService(conn);
}


void CId1Seqref::x_Reconnect(TConn conn) const
{
    m_Reader.Reconnect(conn);
}


CId1BlobSource::CId1BlobSource(const CId1Seqref& seqId, TConn conn)
    : m_Seqref(seqId), m_Conn(conn), m_Count(0)
{
    CRef<CID1server_maxcomplex> params(new CID1server_maxcomplex);
    bool is_external = seqId.Sat() == CReader::kSat_SNP;
    bool skip_extfeat = !is_external && CReader::TrySNPSplit();
    enum {
        kNoExtFeat = 1<<4
    };
    EEntry_complexities maxplex = eEntry_complexities_entry;
    if ( skip_extfeat ) {
        maxplex = EEntry_complexities(maxplex | kNoExtFeat);
    }
    params->SetMaxplex(maxplex);
    params->SetGi(seqId.Gi());
    params->SetEnt(seqId.SatKey());
    params->SetSat(NStr::IntToString(seqId.Sat()));

    CID1server_request id1_request;
    id1_request.SetGetsefromgi(*params);

    {{
        CObjectOStreamAsnBinary server_output(*x_GetService());
        server_output << id1_request;
        server_output.Flush();
        m_Count = 1;
    }}
}


CId1BlobSource::~CId1BlobSource(void)
{
}


CConn_ServiceStream* CId1BlobSource::x_GetService(void) const
{
    return m_Seqref.x_GetService(m_Conn);
}


void CId1BlobSource::x_Reconnect(void)
{
    m_Seqref.x_Reconnect(m_Conn);
    m_Count = 0;
}


bool CId1BlobSource::HaveMoreBlobs(void)
{
    return m_Count > 0;
}


CBlob* CId1BlobSource::RetrieveBlob(void)
{
    --m_Count;
    bool is_snp = m_Seqref.Sat() == CReader::kSat_SNP;
    return new CId1Blob(*this, is_snp);
}


CBlobSource* CId1Seqref::GetBlobSource(TPos , TPos ,
                                       TBlobClass ,
                                       TConn conn) const
{
    return new CId1BlobSource(*this, conn);
}


CId1Blob::CId1Blob(CId1BlobSource& source, bool is_snp)
    : CBlob(is_snp), m_Source(source)
{
}


CId1Blob::~CId1Blob(void)
{
}


static void s_SkipBytes(CByteSourceReader& reader, size_t to_skip)
{
    // skip 2 bytes of hacked header
    const size_t kBufferSize = 128;
    char buffer[kBufferSize];
    while ( to_skip ) {
        size_t cnt = reader.Read(buffer, min(to_skip, sizeof(buffer)));
        if ( cnt == 0 ) {
            NCBI_THROW(CEofException, eEof,
                       "unexpected EOF while skipping ID1 SNP wrapper bytes");
        }
        to_skip -= cnt;
    }
}


void CId1Blob::ReadSeq_entry(void)
{
    if ( IsSnp() ) {
        const size_t kSkipHeader = 2, kSkipFooter = 2;

        CStreamByteSourceReader src(0, m_Source.x_GetService());

        s_SkipBytes(src, kSkipHeader);

        CResultZBtSrc src2(&src);
        
        auto_ptr<CObjectIStream> in;
        in.reset(CObjectIStream::Create(eSerial_AsnBinary, src2));

        m_SNP_annot_Info.Reset(new CSeq_annot_SNP_Info);
        m_Seq_entry = m_SNP_annot_Info->Read(*in);
        if ( m_SNP_annot_Info->empty() ) {
            m_SNP_annot_Info.Reset();
        }

        s_SkipBytes(src, kSkipFooter);
    }
    else {
        CID1server_back id1_reply;

        auto_ptr<CObjectIStream> in;
        in.reset(CObjectIStream::Open(eSerial_AsnBinary,
                                      *m_Source.x_GetService(), false));

        CReader::SetSeqEntryReadHooks(*in);

        *in >> id1_reply;

        if (id1_reply.IsGotseqentry())
            m_Seq_entry.Reset(&id1_reply.SetGotseqentry());
        else if (id1_reply.IsGotdeadseqentry())
            m_Seq_entry.Reset(&id1_reply.SetGotdeadseqentry());
    }
}


const string CId1Seqref::print(void) const
{
    CNcbiOstrstream ostr;
    ostr << "SeqRef(" << Sat() << "," << SatKey () << "," << Gi() << ")";
    return CNcbiOstrstreamToString(ostr);
}


const string CId1Seqref::printTSE(void) const
{
    CNcbiOstrstream ostr;
    ostr << "TSE(" << Sat() << "," << SatKey () << ")";
    return CNcbiOstrstreamToString(ostr);
}


int CId1Seqref::Compare(const CSeqref& seqRef, EMatchLevel ml) const
{
    const CId1Seqref* p = dynamic_cast<const CId1Seqref*> (&seqRef);
    if ( !p ) {
        THROW1_TRACE(runtime_error,
                     "Attempt to compare seqrefs from different sources");
    }
    if ( ml == eContext )
        return 0;
    //cout << "Compare" ; print(); cout << " vs "; p->print(); cout << endl;

    if (Sat() < p->Sat())
        return -1;
    if (Sat() > p->Sat())
        return 1;

    // Sat() == p->Sat()
    if (SatKey() < p->SatKey())
        return -1;
    if (SatKey() > p->SatKey())
        return 1;

    // blob == p->blob
    //cout << "Same TSE" << endl;
    if (ml == eTSE)
        return 0;

    if (Gi() < p->Gi())
        return -1;
    if (Gi() > p->Gi())
        return 1;

    //cout << "Same GI" << endl;
    return 0;
}


CId1Reader::CId1Reader(unsigned noConn)
{
#if !defined(NCBI_THREADS)
    noConn=1;
#endif
    SetParallelLevel(noConn);
}


CId1Reader::~CId1Reader()
{
    SetParallelLevel(0);
}


CReader::TConn CId1Reader::GetParallelLevel(void) const
{
    return m_Pool.size();
}


void CId1Reader::SetParallelLevel(TConn size)
{
    size_t oldSize = m_Pool.size();
    for (size_t i = size; i < oldSize; ++i)
        delete m_Pool[i];

    m_Pool.resize(size);

    for (size_t i = oldSize; i < size; ++i)
        m_Pool[i] = NewID1Service();
}


CConn_ServiceStream* CId1Reader::NewID1Service(void)
{
    _TRACE("CId1Reader(" << this << ")->NewID1Service()");
    STimeout tmout;
    tmout.sec = 20;
    tmout.usec = 0;
    return new CConn_ServiceStream("ID1", fSERV_Any, 0, 0, &tmout);
}


CConn_ServiceStream* CId1Reader::GetService(TConn conn)
{
    return m_Pool[conn % m_Pool.size()];
}


void CId1Reader::Reconnect(TConn conn)
{
    _TRACE("Reconnect(" << conn << ")");
    conn = conn % m_Pool.size();
    delete m_Pool[conn];
    m_Pool[conn] = NewID1Service();
}


END_SCOPE(objects)
END_NCBI_SCOPE


/*
 * $Log$
 * Revision 1.42  2003/08/19 18:34:40  vasilche
 * Fixed warning about enum conversion.
 *
 * Revision 1.41  2003/08/14 20:05:19  vasilche
 * Simple SNP features are stored as table internally.
 * They are recreated when needed using CFeat_CI.
 *
 * Revision 1.40  2003/07/24 19:28:09  vasilche
 * Implemented SNP split for ID1 loader.
 *
 * Revision 1.39  2003/07/17 20:07:56  vasilche
 * Reduced memory usage by feature indexes.
 * SNP data is loaded separately through PUBSEQ_OS.
 * String compression for SNP data.
 *
 * Revision 1.38  2003/06/02 16:06:38  dicuccio
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
 * Revision 1.37  2003/05/13 20:14:40  vasilche
 * Catching exceptions and reconnection were moved from readers to genbank loader.
 *
 * Revision 1.36  2003/04/24 16:12:38  vasilche
 * Object manager internal structures are splitted more straightforward.
 * Removed excessive header dependencies.
 *
 * Revision 1.35  2003/04/15 15:30:15  vasilche
 * Added include <memory> when needed.
 * Removed buggy buffer in printing methods.
 * Removed unnecessary include of stream_util.hpp.
 *
 * Revision 1.34  2003/04/15 14:24:08  vasilche
 * Changed CReader interface to not to use fake streams.
 *
 * Revision 1.33  2003/04/07 16:56:57  vasilche
 * Fixed unassigned member in ID1 request.
 *
 * Revision 1.32  2003/03/31 17:02:03  lavr
 * Some code reformatting to [more closely] meet coding requirements
 *
 * Revision 1.31  2003/03/30 07:00:29  lavr
 * MIPS-specific workaround for lame-designed stream read ops
 *
 * Revision 1.30  2003/03/28 03:28:14  lavr
 * CId1Reader::xsgetn() added; code heavily reformatted
 *
 * Revision 1.29  2003/03/27 19:38:13  vasilche
 * Use safe NStr::IntToString() instead of sprintf().
 *
 * Revision 1.28  2003/03/03 20:34:51  vasilche
 * Added NCBI_THREADS macro - it's opposite to NCBI_NO_THREADS.
 * Avoid using _REENTRANT macro - use NCBI_THREADS instead.
 *
 * Revision 1.27  2003/03/01 22:26:56  kimelman
 * performance fixes
 *
 * Revision 1.26  2003/02/04 18:53:36  dicuccio
 * Include file clean-up
 *
 * Revision 1.25  2002/12/26 20:53:25  dicuccio
 * Minor tweaks to relieve compiler warnings in MSVC
 *
 * Revision 1.24  2002/12/26 16:39:24  vasilche
 * Object manager class CSeqMap rewritten.
 *
 * Revision 1.23  2002/11/18 19:48:43  grichenk
 * Removed "const" from datatool-generated setters
 *
 * Revision 1.22  2002/07/25 15:01:51  grichenk
 * Replaced non-const GetXXX() with SetXXX()
 *
 * Revision 1.21  2002/05/09 21:40:59  kimelman
 * MT tuning
 *
 * Revision 1.20  2002/05/06 03:28:47  vakatov
 * OM/OM1 renaming
 *
 * Revision 1.19  2002/05/03 21:28:10  ucko
 * Introduce T(Signed)SeqPos.
 *
 * Revision 1.18  2002/04/17 19:52:02  kimelman
 * fix: no gi found
 *
 * Revision 1.17  2002/04/09 16:10:56  ucko
 * Split CStrStreamBuf out into a common location.
 *
 * Revision 1.16  2002/04/08 20:52:26  butanaev
 * Added PUBSEQ reader.
 *
 * Revision 1.15  2002/03/29 02:47:05  kimelman
 * gbloader: MT scalability fixes
 *
 * Revision 1.14  2002/03/27 20:23:50  butanaev
 * Added connection pool.
 *
 * Revision 1.13  2002/03/26 23:31:08  gouriano
 * memory leaks and garbage collector fix
 *
 * Revision 1.12  2002/03/26 18:48:58  butanaev
 * Fixed bug not deleting streambuf.
 *
 * Revision 1.11  2002/03/26 17:17:02  kimelman
 * reader stream fixes
 *
 * Revision 1.10  2002/03/26 15:39:25  kimelman
 * GC fixes
 *
 * Revision 1.9  2002/03/25 18:12:48  grichenk
 * Fixed bool convertion
 *
 * Revision 1.8  2002/03/25 17:49:13  kimelman
 * ID1 failure handling
 *
 * Revision 1.7  2002/03/25 15:44:47  kimelman
 * proper logging and exception handling
 *
 * Revision 1.6  2002/03/22 21:50:21  kimelman
 * bugfix: avoid history rtequest for nonexistent sequence
 *
 * Revision 1.5  2002/03/21 19:14:54  kimelman
 * GB related bugfixes
 *
 * Revision 1.4  2002/03/21 01:34:55  kimelman
 * gbloader related bugfixes
 *
 * Revision 1.3  2002/03/20 04:50:13  kimelman
 * GB loader added
 *
 * Revision 1.2  2002/01/16 18:56:28  grichenk
 * Removed CRef<> argument from choice variant setter, updated sources to
 * use references instead of CRef<>s
 *
 * Revision 1.1  2002/01/11 19:06:22  gouriano
 * restructured objmgr
 *
 * Revision 1.7  2001/12/17 21:38:24  butanaev
 * Code cleanup.
 *
 * Revision 1.6  2001/12/13 00:19:25  kimelman
 * bugfixes:
 *
 * Revision 1.5  2001/12/12 21:46:40  kimelman
 * Compare interface fix
 *
 * Revision 1.4  2001/12/10 20:08:02  butanaev
 * Code cleanup.
 *
 * Revision 1.3  2001/12/07 21:24:59  butanaev
 * Interface development, code beautyfication.
 *
 * Revision 1.2  2001/12/07 16:43:58  butanaev
 * Fixed includes.
 *
 * Revision 1.1  2001/12/07 16:10:23  butanaev
 * Switching to new reader interfaces.
 *
 * Revision 1.3  2001/12/06 20:37:05  butanaev
 * Fixed timeout problem.
 *
 * Revision 1.2  2001/12/06 18:06:22  butanaev
 * Ported to linux.
 *
 * Revision 1.1  2001/12/06 14:35:23  butanaev
 * New streamable interfaces designed, ID1 reimplemented.
 *
 */
