#ifndef ANNOT_CI__HPP
#define ANNOT_CI__HPP

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
* Author: Aleksey Grichenko, Michael Kimelman
*
* File Description:
*   Object manager iterators
*
*/


#include <corelib/ncbistd.hpp>
//#include <objects/objmgr/bioseq_handle.hpp>
#include <objects/objmgr/seq_id_handle.hpp>
#include <objects/objmgr/annot_selector.hpp>
#include <objects/objmgr/impl/tse_info.hpp>
#include <util/rangemap.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

struct SAnnotObject_Index;
class CAnnotObject_Info;
class CHandleRangeMap;
class CTSE_Info;


// General annotation iterator. Public interfaces should use
// CFeat_CI, CGraph_CI and CAlign_CI instead.
class NCBI_XOBJMGR_EXPORT CAnnot_CI : public SAnnotSelector
{
public:
    typedef CRange<TSeqPos>                                          TRange;
    typedef CRangeMultimap<SAnnotObject_Index,TRange::position_type> TRangeMap;
    typedef map<SAnnotSelector, TRangeMap>                   TAnnotSelectorMap;
    typedef map<CSeq_id_Handle, TAnnotSelectorMap>                   TAnnotMap;

    CAnnot_CI(void);
    CAnnot_CI(CTSE_Info& tse,
              CHandleRangeMap& loc,
              const SAnnotSelector& selector);
    CAnnot_CI(CTSE_Info& tse,
              CHandleRangeMap& loc,
              SAnnotSelector selector,
              EOverlapType overlap_type);
    ~CAnnot_CI(void);

    CAnnot_CI& operator++ (void);
    operator bool (void) const;
    const SAnnotObject_Index& GetIndex(void) const;
    const CAnnotObject_Info& operator* (void) const;
    const CAnnotObject_Info* operator-> (void) const;

private:

    typedef TRangeMap::const_iterator     TRangeIter;

    // Check if the location intersects with the current m_Location.
    bool x_ValidLocation(const CHandleRangeMap& loc) const;

    // FALSE if the selected item does not fit m_Selector and m_Location
    // TRUE will also be returned at the end of annotation list
    bool x_IsValid(void) const;

    // Move to the next valid seq-annot
    void x_Walk(void);

    // The TSEInfo is locked by CAnnotTypes_CI, not here.
    CTSE_Lock         m_TSEInfo;
    const TRangeMap*  m_RangeMap;
    TRange            m_CoreRange;
    TRangeIter        m_Current;
    CHandleRangeMap*  m_HandleRangeMap;
    CSeq_id_Handle    m_CurrentHandle;
};

inline
CAnnot_CI& CAnnot_CI::operator++(void)
{
    x_Walk();
    return *this;
}

inline
CAnnot_CI::operator bool (void) const
{
    return bool(m_TSEInfo)  &&  bool(m_Current);
}

inline
const SAnnotObject_Index& CAnnot_CI::GetIndex(void) const
{
    _ASSERT(bool(m_TSEInfo)  &&  bool(m_Current));
    return m_Current->second;
}

inline
const CAnnotObject_Info& CAnnot_CI::operator* (void) const
{
    _ASSERT(bool(m_TSEInfo)  &&  bool(m_Current));
    return *m_Current->second.m_AnnotObject;
}

inline
const CAnnotObject_Info* CAnnot_CI::operator-> (void) const
{
    _ASSERT(bool(m_TSEInfo)  &&  bool(m_Current));
    return m_Current->second.m_AnnotObject.GetPointer();
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.18  2003/03/05 20:56:42  vasilche
* SAnnotSelector now holds all parameters of annotation iterators.
*
* Revision 1.17  2003/02/24 18:57:20  vasilche
* Make feature gathering in one linear pass using CSeqMap iterator.
* Do not use feture index by sub locations.
* Sort features at the end of gathering in one vector.
* Extracted some internal structures and classes in separate header.
* Delay creation of mapped features.
*
* Revision 1.16  2003/02/13 14:34:31  grichenk
* Renamed CAnnotObject -> CAnnotObject_Info
* + CSeq_annot_Info and CAnnotObject_Ref
* Moved some members of CAnnotObject to CSeq_annot_Info
* and CAnnotObject_Ref.
* Added feat/align/graph to CAnnotObject_Info map
* to CDataSource.
*
* Revision 1.15  2003/01/29 17:45:04  vasilche
* Annotaions index is split by annotation/feature type.
*
* Revision 1.14  2002/12/26 20:42:55  dicuccio
* Added Win32 export specifier.  Removed unimplemented (private) operator++(int)
*
* Revision 1.13  2002/12/20 20:54:23  grichenk
* Added optional location/product switch to CFeat_CI
*
* Revision 1.12  2002/12/06 15:35:57  grichenk
* Added overlap type for annot-iterators
*
* Revision 1.11  2002/12/05 19:28:29  grichenk
* Prohibited postfix operator ++()
*
* Revision 1.10  2002/07/08 20:50:56  grichenk
* Moved log to the end of file
* Replaced static mutex (in CScope, CDataSource) with the mutex
* pool. Redesigned CDataSource data locking.
*
* Revision 1.9  2002/05/06 03:30:35  vakatov
* OM/OM1 renaming
*
* Revision 1.8  2002/05/03 21:28:01  ucko
* Introduce T(Signed)SeqPos.
*
* Revision 1.7  2002/03/04 15:08:43  grichenk
* Improved CTSE_Info locks
*
* Revision 1.6  2002/02/21 19:27:00  grichenk
* Rearranged includes. Added scope history. Added searching for the
* best seq-id match in data sources and scopes. Updated tests.
*
* Revision 1.5  2002/02/15 20:36:29  gouriano
* changed implementation of HandleRangeMap
*
* Revision 1.4  2002/02/07 21:27:33  grichenk
* Redesigned CDataSource indexing: seq-id handle -> TSE -> seq/annot
*
* Revision 1.3  2002/01/23 21:59:28  grichenk
* Redesigned seq-id handles and mapper
*
* Revision 1.2  2002/01/16 16:26:36  gouriano
* restructured objmgr
*
* Revision 1.1  2002/01/11 19:03:59  gouriano
* restructured objmgr
*
*
* ===========================================================================
*/

#endif  // ANNOT_CI__HPP
