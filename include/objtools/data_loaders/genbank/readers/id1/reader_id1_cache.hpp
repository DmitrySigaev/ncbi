#ifndef READER_ID1_CACHE__HPP_INCLUDED
#define READER_ID1_CACHE__HPP_INCLUDED

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
*  Author:  Eugene Vasilchenko, Anatoliy Kuznetsov
*
*  File Description: Cached extension of data reader from ID1
*
*/

#include <objmgr/reader_id1.hpp>

BEGIN_NCBI_SCOPE

class IBLOB_Cache;
class IIntCache;
class IReader;
class IWriter;

BEGIN_SCOPE(objects);

class CID2_Reply_Data;

/// ID1 reader with caching capabilities
///
class NCBI_XOBJMGR_EXPORT CCachedId1Reader : public CId1Reader
{
public:
    CCachedId1Reader(TConn noConn = 5, 
                     IBLOB_Cache* blob_cache = 0,
                     IIntCache* id_cache = 0);
    ~CCachedId1Reader();

    void SetBlobCache(IBLOB_Cache* blob_cache);
    void SetIdCache(IIntCache* id_cache);

    void RetrieveSeqrefs(TSeqrefs& sr, int gi, TConn conn);
    void PurgeSeqrefs(const TSeqrefs& srs, const CSeq_id& id);

    /// Return BLOB key string based on CSeqref Sat() and SatKey()
    /// @sa CSeqref::Sat(), CSeqref::SatKey()
    string GetBlobKey(const CSeqref& seqref);

    int GetSNPBlobVersion(int gi);

    void GetTSEChunk(const CSeqref& seqref, CTSE_Chunk_Info& chunk_info,
                     TConn conn);

protected:
    
    void PrintStatistics(void) const;

    int x_GetVersion(const CSeqref& seqref, TConn conn);

    void x_GetTSEBlob(CID1server_back& id1_reply,
                      CRef<CID2S_Split_Info>& split_info,
                      const CSeqref& seqref,
                      TConn conn);
    void x_GetSNPAnnot(CSeq_annot_SNP_Info& snp_info,
                       const CSeqref& seqref,
                       TConn conn);

    void x_ReadTSEBlob(CID1server_back& id1_reply,
                       const CSeqref&   seqref,
                       CNcbiIstream&    stream);

    bool GetBlobInfo(int gi, TSeqrefs& sr);
    void StoreBlobInfo(int gi, const TSeqrefs& sr);

    void StoreSNPBlobVersion(int gi, int version);

    bool LoadBlob(CID1server_back& id1_reply,
                  CRef<CID2S_Split_Info>& split_info,
                  const CSeqref& seqref);
    bool LoadWholeBlob(CID1server_back& id1_reply,
                       const CSeqref& seqref);
    bool LoadSplitBlob(CID1server_back& id1_reply,
                       CRef<CID2S_Split_Info>& split_info,
                       const CSeqref& seqref);

    bool LoadSNPTable(CSeq_annot_SNP_Info& snp_info,
                      const CSeqref& seqref);
    void StoreSNPTable(const CSeq_annot_SNP_Info& snp_info,
                       const CSeqref& seqref);

    bool LoadData(const string& key, const char* suffix,
                  int version, CID2_Reply_Data& data);

    enum ECompression
    {
        eCompression_none,
        eCompression_nlm_zip,
        eCompression_gzip
    };

    enum EDataType
    {
        eDataType_MainBlob = 0,
        eDataType_SplitInfo = 1,
        eDataType_Chunk = 2
    };

    CRef<CByteSourceReader> GetReader(CID2_Reply_Data& data,
                                      EDataType data_type);
    AutoPtr<CObjectIStream> OpenData(CID2_Reply_Data& data,
                                     CByteSourceReader& reader);

private:

    CCachedId1Reader(const CCachedId1Reader& );
    CCachedId1Reader& operator=(const CCachedId1Reader&);

private:
    IBLOB_Cache*   m_BlobCache;
    IIntCache*     m_IdCache;
};


END_SCOPE(objects)
END_NCBI_SCOPE


/*
* $Log$
* Revision 1.10  2003/11/26 17:55:53  vasilche
* Implemented ID2 split in ID1 cache.
* Fixed loading of splitted annotations.
*
* Revision 1.9  2003/10/27 15:05:41  vasilche
* Added correct recovery of cached ID1 loader if gi->sat/satkey cache is invalid.
* Added recognition of ID1 error codes: private, etc.
* Some formatting of old code.
*
* Revision 1.8  2003/10/21 16:32:50  vasilche
* Cleaned ID1 statistics messages.
* Now by setting GENBANK_ID1_STATS=1 CId1Reader collects and displays stats.
* And by setting GENBANK_ID1_STATS=2 CId1Reader logs all activities.
*
* Revision 1.7  2003/10/21 14:27:34  vasilche
* Added caching of gi -> sat,satkey,version resolution.
* SNP blobs are stored in cache in preprocessed format (platform dependent).
* Limit number of connections to GenBank servers.
* Added collection of ID1 loader statistics.
*
* Revision 1.6  2003/10/14 18:31:53  vasilche
* Added caching support for SNP blobs.
* Added statistics collection of ID1 connection.
*
* Revision 1.5  2003/10/08 18:57:49  kuznets
* Implemeneted correct ID1 BLOB versions.
*
* Revision 1.4  2003/10/03 17:41:33  kuznets
* Added an option, that cache is owned by the ID1 reader.
* Cache destroyed with the reader.
*
* Revision 1.3  2003/10/02 19:28:34  kuznets
* First working revision
*
* Revision 1.2  2003/10/01 19:32:01  kuznets
* Work in progress
*
* Revision 1.1  2003/09/30 19:38:26  vasilche
* Added support for cached id1 reader.
*
*/

#endif // READER_ID1_CACHE__HPP_INCLUDED
