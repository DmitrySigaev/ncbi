#ifndef BLOB_ID__HPP_INCLUDED
#define BLOB_ID__HPP_INCLUDED
/* */

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
*  Author:  Eugene Vasilchenko
*
*  File Description: Loaded Blob ID
*
*/

#include <corelib/ncbiobj.hpp>
#include <utility>
#include <string>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class NCBI_XREADER_EXPORT CBlob_id : public CObject
{
public:
    CBlob_id(void)
        : m_Sat(-1), m_SubSat(0), m_SatKey(0)
        {
        }
    
    int GetSat() const
        {
            return m_Sat;
        }
    int GetSubSat() const
        {
            return m_SubSat;
        }
    int GetSatKey() const
        {
            return m_SatKey;
        }

    const string ToString(void) const;

    bool operator<(const CBlob_id& blob_id) const
        {
            if ( m_Sat != blob_id.m_Sat )
                return m_Sat < blob_id.m_Sat;
            if ( m_SubSat != blob_id.m_SubSat )
                return m_SubSat < blob_id.m_SubSat;
            return m_SatKey < blob_id.m_SatKey;
        }
    bool operator==(const CBlob_id& blob_id) const
        {
            return m_SatKey == blob_id.m_SatKey &&
                m_Sat == blob_id.m_Sat && m_SubSat == blob_id.m_SubSat;
        }
    bool operator!=(const CBlob_id& blob_id) const
        {
            return m_SatKey != blob_id.m_SatKey ||
                m_Sat != blob_id.m_Sat || m_SubSat != blob_id.m_SubSat;
        }

    bool IsEmpty(void) const
        {
            return m_Sat < 0;
        }

    bool IsMainBlob(void) const
        {
            return m_SubSat == 0;
        }

    void SetSat(int v)
        {
            m_Sat = v;
        }
    void SetSubSat(int v)
        {
            m_SubSat = v;
        }
    void SetSatKey(int v)
        {
            m_SatKey = v;
        }

protected:
    int m_Sat;
    int m_SubSat;
    int m_SatKey;
};


enum EBlobContentsMask
{
    fBlobHasCore     = 1 << 0,
    fBlobHasDescr    = 1 << 1,
    fBlobHasSeqMap   = 1 << 2,
    fBlobHasSeqData  = 1 << 3,
    fBlobHasFeatures = 1 << 4,
    fBlobHasExternal = 1 << 5,
    fBlobHasAlign    = 1 << 6,
    fBlobHasGraph    = 1 << 7,
    
    fBlobHasAllLocal = fBlobHasCore | fBlobHasDescr | fBlobHasSeqMap | fBlobHasSeqData,

    fBlobHasAll      = (1 << 16)-1
};
typedef int TBlobContentsMask;


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* $Log$
* Revision 1.1  2004/08/04 14:55:17  vasilche
* Changed TSE locking scheme.
* TSE cache is maintained by CDataSource.
* Added ID2 reader.
* CSeqref is replaced by CBlobId.
*
*/

#endif//BLOB_ID__HPP_INCLUDED
