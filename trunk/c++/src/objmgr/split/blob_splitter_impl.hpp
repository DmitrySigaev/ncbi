#ifndef NCBI_OBJMGR_SPLIT_BLOB_SPLITTER_IMPL__HPP
#define NCBI_OBJMGR_SPLIT_BLOB_SPLITTER_IMPL__HPP

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
*   Application for splitting blobs withing ID1 cache
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <corelib/ncbiobj.hpp>

#include <memory>
#include <map>
#include <vector>

#include "blob_splitter_params.hpp"
#include "split_blob.hpp"
#include "chunk_info.hpp"
#include "object_splitinfo.hpp"
#include "size.hpp"

BEGIN_NCBI_SCOPE

class CObjectOStream;

BEGIN_SCOPE(objects)

class CSeq_entry;
class CBioseq;
class CBioseq_set;
class CSeq_annot;
class CSeq_feat;
class CSeq_align;
class CSeq_graph;
class CID2S_Split_Info;
class CID2S_Chunk_Id;
class CID2S_Chunk;
class CBlobSplitter;
class CBlobSplitterImpl;
class CAnnotObject_SplitInfo;
class CLocObjects_SplitInfo;
class CSeq_annot_SplitInfo;
class CBioseq_SplitInfo;

struct SAnnotPiece;
struct SIdAnnotPieces;
class CAnnotPieces;
struct SChunkInfo;

class CBlobSplitterImpl
{
public:
    CBlobSplitterImpl(const SSplitterParams& params);
    ~CBlobSplitterImpl(void);

    typedef map<int, CBioseq_SplitInfo> TBioseqs;
    typedef map<int, SChunkInfo> TChunks;
    typedef map<CID2S_Chunk_Id, CRef<CID2S_Chunk> > TID2Chunks;

    bool Split(const CSeq_entry& entry);

    const CSplitBlob& GetBlob(void) const
        {
            return m_SplitBlob;
        }

    void Reset(void);

    void CopySkeleton(CSeq_entry& dst, const CSeq_entry& src);
    void CopySkeleton(CBioseq_set& dst, const CBioseq_set& src);
    void CopySkeleton(CBioseq& dst, const CBioseq& src);
    bool CopyAnnot(CBioseq_SplitInfo& bioseq_info, const CSeq_annot& annot);

    void CollectPieces(void);
    void SplitPieces(void);
    void MakeID2SObjects(void);
    void AttachToSkeleton(const SChunkInfo& info);

    static size_t CountAnnotObjects(const CSeq_annot& annot);
    static size_t CountAnnotObjects(const CSeq_entry& entry);
    static size_t CountAnnotObjects(const CID2S_Chunk& chunk);
    static size_t CountAnnotObjects(const TID2Chunks& chunks);

    typedef vector<CAnnotObject_SplitInfo> TAnnotObjects;
    CRef<CSeq_annot> MakeSeq_annot(const CSeq_annot& src,
                                   const TAnnotObjects& objs);
    void MakeID2Chunk(int id, const SChunkInfo& info);

    SChunkInfo* NextChunk(SChunkInfo* chunk, const CSize& size);

private:
    // params
    SSplitterParams m_Params;

    // split result
    CSplitBlob m_SplitBlob;

    // split state
    CRef<CSeq_entry> m_Skeleton;
    CRef<CID2S_Split_Info> m_Split_Info;
    TID2Chunks m_ID2_Chunks;

    int m_NextBioseq_set_Id;

    TBioseqs m_Bioseqs;

    AutoPtr<CAnnotPieces> m_Pieces;

    TChunks m_Chunks;
};


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.5  2003/12/03 19:40:57  kuznets
* Minor file rename
*
* Revision 1.4  2003/12/03 19:30:44  kuznets
* Misprint fixed
*
* Revision 1.3  2003/12/02 19:12:23  vasilche
* Fixed compilation on MSVC.
*
* Revision 1.2  2003/11/26 23:04:58  vasilche
* Removed extra semicolons after BEGIN_SCOPE and END_SCOPE.
*
* Revision 1.1  2003/11/12 16:18:27  vasilche
* First implementation of ID2 blob splitter withing cache.
*
* ===========================================================================
*/
#endif//NCBI_OBJMGR_SPLIT_BLOB_SPLITTER_IMPL__HPP
