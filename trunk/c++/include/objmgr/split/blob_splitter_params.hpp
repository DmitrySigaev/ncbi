#ifndef NCBI_OBJMGR_SPLIT_BLOB_SPLITTER_PARAMS__HPP
#define NCBI_OBJMGR_SPLIT_BLOB_SPLITTER_PARAMS__HPP

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

#include <vector>

BEGIN_NCBI_SCOPE

class CObjectOStream;

BEGIN_SCOPE(objects)

struct SSplitterParams
{
    enum {
        kDefaultChunkSize = 20 * 1024
    };

    enum ECompression
    {
        eCompression_none,
        eCompression_nlm_zip,
        eCompression_gzip
    };

    SSplitterParams(void)
        {
            SetChunkSize(kDefaultChunkSize);
            m_Compression = eCompression_none;
        }


    void SetChunkSize(size_t size)
        {
            m_ChunkSize = size;
            m_MinChunkSize = size_t(double(size) * 0.8);
            m_MaxChunkSize = size_t(double(size) * 1.2);
        }

    void Compress(vector<char>& dst, const char* data, size_t size) const;

    // parameters
    size_t       m_ChunkSize;
    size_t       m_MinChunkSize;
    size_t       m_MaxChunkSize;
    ECompression m_Compression;
};


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.4  2003/12/18 21:15:35  vasilche
* Fixed long -> double conversion warning.
*
* Revision 1.3  2003/11/26 23:04:58  vasilche
* Removed extra semicolons after BEGIN_SCOPE and END_SCOPE.
*
* Revision 1.2  2003/11/26 17:56:02  vasilche
* Implemented ID2 split in ID1 cache.
* Fixed loading of splitted annotations.
*
* Revision 1.1  2003/11/12 16:18:29  vasilche
* First implementation of ID2 blob splitter withing cache.
*
* ===========================================================================
*/
#endif//NCBI_OBJMGR_SPLIT_BLOB_SPLITTER_PARAMS__HPP
