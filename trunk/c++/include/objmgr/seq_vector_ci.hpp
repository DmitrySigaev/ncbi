#ifndef SEQ_VECTOR_CI__HPP
#define SEQ_VECTOR_CI__HPP

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
* Author: Aleksey Grichenko, Michael Kimelman, Eugene Vasilchenko
*
* File Description:
*   Seq-vector iterator
*
*/

#include <objects/objmgr/seq_vector.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CSeqVector_CI
{
public:
    CSeqVector_CI(void);
    ~CSeqVector_CI(void);
    CSeqVector_CI(const CSeqVector& seq_vector, TSeqPos pos = 0);
    CSeqVector_CI(const CSeqVector_CI& sv_it);
    CSeqVector_CI& operator=(const CSeqVector_CI& sv_it);

    typedef CSeqVector::TResidue     TResidue;

    // Fill the buffer string with the sequence data for the interval
    // [start, stop).
    void GetSeqData(TSeqPos start, TSeqPos stop, string& buffer);

    CSeqVector_CI& operator++(void);
    CSeqVector_CI& operator--(void);

    TSeqPos GetPos(void) const;
    void SetPos(TSeqPos pos);

    TResidue operator*(void) const;
    operator bool(void) const;

private:
    void x_NextCacheSeg(void);
    void x_PrevCacheSeg(void);
    void x_UpdateCache(TSeqPos pos);
    void x_FillCache(TSeqPos count);

    friend class CSeqVector;

    typedef vector<char> TCacheData;
    typedef char* TCache_I;
    typedef CSeqVector::TCoding TCoding;

    void x_ResizeCache(size_t size);
    void x_UpdateCachePtr(void);
    TSeqPos x_CacheEnd(void) const;
    TSeqPos x_BackupEnd(void) const;

    CConstRef<CSeqVector>   m_Vector;
    TCoding                 m_Coding;
    CSeqMap::const_iterator m_Seg;
    // Current cache
    TSeqPos                 m_CachePos;
    TCacheData              m_CacheData;
    TCache_I                m_Cache;
    // Backup cache
    TSeqPos                 m_BackupPos;
    TCacheData              m_BackupData;
};


/////////////////////////////////////////////////////////////////////
//
//  Inline methods
//
/////////////////////////////////////////////////////////////////////

inline
CSeqVector_CI::CSeqVector_CI(void)
    : m_Vector(0),
      m_Coding(CSeq_data::e_not_set),
      m_CachePos(kInvalidSeqPos),
      m_Cache(0),
      m_BackupPos(kInvalidSeqPos)
{
    return;
}

inline
CSeqVector_CI::~CSeqVector_CI(void)
{
    return;
}

inline
CSeqVector_CI::CSeqVector_CI(const CSeqVector_CI& sv_it)
    : m_Vector(sv_it.m_Vector),
      m_Coding(sv_it.m_Coding),
      m_CachePos(sv_it.m_CachePos),
      m_CacheData(sv_it.m_CacheData),
      m_BackupPos(sv_it.m_BackupPos),
      m_BackupData(sv_it.m_BackupData)
{
    x_UpdateCachePtr();
    if ( sv_it.m_Cache ) {
        m_Cache += sv_it.m_Cache - sv_it.m_CacheData.begin();
    }
}

inline
CSeqVector_CI& CSeqVector_CI::operator=(const CSeqVector_CI& sv_it)
{
    if (this == &sv_it)
        return *this;
    m_Vector = sv_it.m_Vector;
    m_Coding = sv_it.m_Coding;
    m_CachePos = sv_it.m_CachePos;
    m_CacheData = sv_it.m_CacheData;
    m_BackupPos = sv_it.m_BackupPos;
    m_BackupData = sv_it.m_BackupData;
    x_UpdateCachePtr();
    if ( sv_it.m_Cache ) {
        m_Cache += sv_it.m_Cache - sv_it.m_CacheData.begin();
    }
    return *this;
}

inline
CSeqVector_CI& CSeqVector_CI::operator++(void)
{
    _ASSERT(bool(m_Vector)  &&  m_Cache);
    if (++m_Cache >= m_CacheData.end()) {
            x_NextCacheSeg();
    }
    return *this;
}

inline
CSeqVector_CI& CSeqVector_CI::operator--(void)
{
    _ASSERT(bool(m_Vector)  &&  m_Cache);
    if (--m_Cache < m_CacheData.begin()) {
            x_PrevCacheSeg();
    }
    return *this;
}

inline
TSeqPos CSeqVector_CI::GetPos(void) const
{
    return m_Cache ? m_Cache - m_CacheData.begin() + m_CachePos : kInvalidSeqPos;
}

inline
CSeqVector_CI::TResidue CSeqVector_CI::operator*(void) const
{
    _ASSERT(m_Cache);
    return *m_Cache;
}

inline
CSeqVector_CI::operator bool(void) const
{
    return bool(m_Vector)  &&  m_Cache;
}

inline
TSeqPos CSeqVector_CI::x_CacheEnd(void) const
{
    return m_CachePos + m_CacheData.size();
}

inline
TSeqPos CSeqVector_CI::x_BackupEnd(void) const
{
    return m_BackupPos + m_BackupData.size();
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.2  2003/05/30 18:00:26  grichenk
* Fixed caching bugs in CSeqVector_CI
*
* Revision 1.1  2003/05/27 19:42:23  grichenk
* Initial revision
*
*
* ===========================================================================
*/

#endif  // SEQ_VECTOR_CI__HPP
