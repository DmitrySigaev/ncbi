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


#include <objmgr/seq_map.hpp>
#include <objects/seq/Seq_data.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


class CSeqVector;


class NCBI_XOBJMGR_EXPORT CSeqVector_CI
{
public:
    CSeqVector_CI(void);
    ~CSeqVector_CI(void);
    CSeqVector_CI(const CSeqVector& seq_vector, TSeqPos pos = 0);
    CSeqVector_CI(const CSeqVector_CI& sv_it);
    CSeqVector_CI& operator=(const CSeqVector_CI& sv_it);

    typedef unsigned char TResidue;

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
    void x_SetPos(TSeqPos pos);
    void x_InitializeCache(void);
    void x_DestroyCache(void);
    void x_ClearCache(void);
    void x_ResizeCache(size_t size);
    void x_SwapCache(void);
    void x_UpdateCache(TSeqPos pos);
    void x_FillCache(TSeqPos start, TSeqPos count);

    void x_NextCacheSeg(void);
    void x_PrevCacheSeg(void);

    TSeqPos x_CacheEnd(void) const;
    TSeqPos x_BackupEnd(void) const;

    friend class CSeqVector;

    typedef char* TCacheData;
    typedef char* TCache_I;
    typedef CSeq_data::E_Choice   TCoding;

    CConstRef<CSeqVector>   m_Vector;
    TCoding                 m_Coding;
    CSeqMap::const_iterator m_Seg;
    // Current cache
    TSeqPos                 m_CachePos;
    TCacheData              m_CacheData;
    TCache_I                m_CacheEnd;
    TCache_I                m_Cache;
    // Backup cache
    TSeqPos                 m_BackupPos;
    TCacheData              m_BackupData;
    TCache_I                m_BackupEnd;
};


/////////////////////////////////////////////////////////////////////
//
//  Inline methods
//
/////////////////////////////////////////////////////////////////////


inline
CSeqVector_CI& CSeqVector_CI::operator++(void)
{
    _ASSERT(m_Cache != m_CacheEnd);
    if (++m_Cache >= m_CacheEnd) {
        x_NextCacheSeg();
    }
    return *this;
}

inline
CSeqVector_CI& CSeqVector_CI::operator--(void)
{
    _ASSERT(m_Cache >= m_CacheData);
    if (m_Cache == m_CacheData) {
        x_PrevCacheSeg();
    }
    else {
        --m_Cache;
    }
    return *this;
}

inline
TSeqPos CSeqVector_CI::GetPos(void) const
{
    return (m_Cache - m_CacheData) + m_CachePos;
}

inline
void CSeqVector_CI::SetPos(TSeqPos pos)
{
    TCache_I data = m_CacheData;
    TSeqPos offset = pos - m_CachePos;
    TSeqPos size = m_CacheEnd - data;
    if ( offset >= size ) {
        x_SetPos(pos);
    }
    else {
        m_Cache = data + offset;
    }
}

inline
CSeqVector_CI::TResidue CSeqVector_CI::operator*(void) const
{
    _ASSERT(m_Cache < m_CacheEnd);
    return *m_Cache;
}

inline
CSeqVector_CI::operator bool(void) const
{
    return m_Cache < m_CacheEnd;
}

inline
TSeqPos CSeqVector_CI::x_CacheEnd(void) const
{
    return m_CachePos + (m_CacheEnd - m_CacheData);
}

inline
TSeqPos CSeqVector_CI::x_BackupEnd(void) const
{
    return m_BackupPos + (m_BackupEnd - m_BackupData);
}

inline
void CSeqVector_CI::x_ClearCache(void)
{
    m_CachePos = kInvalidSeqPos;
    m_CacheEnd = m_CacheData;
    m_Cache = m_CacheEnd;
}

inline
void CSeqVector_CI::x_SwapCache(void)
{
    swap(m_CacheData, m_BackupData);
    swap(m_CacheEnd, m_BackupEnd);
    swap(m_CachePos, m_BackupPos);
    m_Cache = m_CacheData;
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.13  2003/08/21 13:32:04  vasilche
* Optimized CSeqVector iteration.
* Set some CSeqVector values (mol type, coding) in constructor instead of detecting them while iteration.
* Remove unsafe bit manipulations with coding.
*
* Revision 1.12  2003/08/19 18:34:11  vasilche
* Buffer length constant moved to *.cpp file for easier modification.
*
* Revision 1.11  2003/08/14 20:05:18  vasilche
* Simple SNP features are stored as table internally.
* They are recreated when needed using CFeat_CI.
*
* Revision 1.10  2003/07/18 19:42:42  vasilche
* Added x_DestroyCache() method.
*
* Revision 1.9  2003/06/24 19:46:41  grichenk
* Changed cache from vector<char> to char*. Made
* CSeqVector::operator[] inline.
*
* Revision 1.8  2003/06/17 22:03:37  dicuccio
* Added missing export specifier
*
* Revision 1.7  2003/06/05 20:20:21  grichenk
* Fixed bugs in assignment functions,
* fixed minor problems with coordinates.
*
* Revision 1.6  2003/06/04 13:48:53  grichenk
* Improved double-caching, fixed problem with strands.
*
* Revision 1.5  2003/06/02 16:01:36  dicuccio
* Rearranged include/objects/ subtree.  This includes the following shifts:
*     - include/objects/alnmgr --> include/objtools/alnmgr
*     - include/objects/cddalignview --> include/objtools/cddalignview
*     - include/objects/flat --> include/objtools/flat
*     - include/objects/objmgr/ --> include/objmgr/
*     - include/objects/util/ --> include/objmgr/util/
*     - include/objects/validator --> include/objtools/validator
*
* Revision 1.4  2003/05/30 20:43:54  vasilche
* Fixed CSeqVector_CI::operator--().
*
* Revision 1.3  2003/05/30 19:30:08  vasilche
* Fixed compilation on GCC 3.
*
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
