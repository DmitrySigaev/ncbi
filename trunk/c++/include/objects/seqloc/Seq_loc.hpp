/* $Id$
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
 * Author:  Cliff Clausen, Eugene Vasilchenko
 *
 * File Description:
 *   .......
 *
 * Remark:
 *   This code was originally generated by application DATATOOL
 *   using specifications from the ASN data definition file
 *   'seqloc.asn'.
 *
 * ===========================================================================
 */

#ifndef OBJECTS_SEQLOC_SEQ_LOC_HPP
#define OBJECTS_SEQLOC_SEQ_LOC_HPP


// generated includes
#include <objects/seqloc/Seq_loc_.hpp>
#include <objects/seqloc/Seq_loc_mix.hpp>
#include <objects/seqloc/Packed_seqpnt.hpp>
#include <objects/seqloc/Packed_seqint.hpp>
#include <objects/seqloc/Seq_id.hpp>

#include <objects/general/Int_fuzz.hpp>

//
#include <corelib/ncbiexpt.hpp>
#include <util/range.hpp>
#include <vector>

BEGIN_NCBI_SCOPE
BEGIN_objects_SCOPE // namespace ncbi::objects::

class CSeq_id_Handle;

class NCBI_SEQLOC_EXPORT CSeq_loc : public CSeq_loc_Base
{
public:
    typedef CSeq_loc_Base Tparent;
    typedef CPacked_seqpnt_Base::TPoints TPoints;
    typedef CPacked_seqint_Base::Tdata   TIntervals;
    typedef CSeq_loc_mix_Base::Tdata     TLocations;
    typedef CSeq_id                      TId;
    typedef ENa_strand                   TStrand;
    typedef TSeqPos                      TPoint;
    typedef CPacked_seqint::TRanges      TRanges;

    // constructor
    CSeq_loc(void);
    CSeq_loc(E_Choice index);
    CSeq_loc(TId& id, TPoint point, TStrand strand = eNa_strand_unknown);
    CSeq_loc(TId& id, const TPoints& points, TStrand strand = eNa_strand_unknown);
    CSeq_loc(TId& id, TPoint from, TPoint to, TStrand strand = eNa_strand_unknown);
    CSeq_loc(TId& id, TRanges ivals, TStrand strand = eNa_strand_unknown);

    // destructor
    virtual ~CSeq_loc(void);

    // See related functions in util/sequence.hpp:
    //
    //   TSeqPos GetLength(const CSeq_loc&, CScope*)
    //   bool IsOneBioseq(const CSeq_loc&, CScope*)
    //   const CSeq_id& GetId(const CSeq_loc&, CScope*)
    //   TSeqPos GetStart(const CSeq_loc&, CScope*)
    //   sequence::ECompare Compare(const CSeq_loc&, CSeq_loc&, CScope*)
    //   sequence::SeqLocMerge(...)
    //

    typedef CRange<TSeqPos> TRange;

    TRange GetTotalRange(void) const;
    void InvalidateTotalRangeCache(void);
 
    // Return true if all ranges have reverse strand
    bool IsReverseStrand(void) const;

    // Flip the strand (e.g. plus to minus)
    void FlipStrand(void);

    // Return start and stop positions of the seq-loc.
    // End may be less than Start for circular sequences.
    TSeqPos GetStart(TSeqPos circular_length = kInvalidSeqPos) const;
    TSeqPos GetEnd(TSeqPos circular_length = kInvalidSeqPos) const;

    // Special case for circular sequences. No ID is checked for
    // circular locations. If the sequence is not circular
    // (seq_len == kInvalidSeqPos) the function works like GetTotalRange()
    TSeqPos GetCircularLength(TSeqPos seq_len) const;

    // Appends a label suitable for display (e.g., error messages)
    // label must point to an existing string object
    // Method just returns if label is null. Note this label is NOT
    // GenBank-style.
    void GetLabel(string* label) const;

    // check left (5') or right (3') end of location for e_Lim fuzz
    bool IsPartialLeft  (void) const;
    bool IsPartialRight (void) const;

    // set / remove e_Lim fuzz on left (5') or right (3') end
    void SetPartialLeft (bool val);
    void SetPartialRight(bool val);

    // set the 'id' field in all parts of this location
    void SetId(CSeq_id& id); // stores id
    void SetId(const CSeq_id& id); // stores a new copy of id

    // check that the 'id' field in all parts of the location is the same
    // as the specifies id.
    // if the id parameter is NULL will return the location's id (if unique)
    void CheckId(const CSeq_id*& id) const;
    void InvalidateIdCache(void);

    virtual void Assign(const CSerialObject& source,
                        ESerialRecursionMode how = eRecursive);
    virtual bool Equals(const CSerialObject& object,
                        ESerialRecursionMode how = eRecursive) const;

    // Compare locations if they are defined on the same single sequence
    // or throw exception.
    int Compare(const CSeq_loc& loc) const;

    // Simple adding of seq-locs.
    void Add(const CSeq_loc& other);

    void ChangeToMix(void);
    // Works only if current state is "Int".
    void ChangeToPackedInt(void);

private:
    // Prohibit copy constructor & assignment operator
    CSeq_loc(const CSeq_loc&);
    CSeq_loc& operator= (const CSeq_loc&);

    TRange x_UpdateTotalRange(void) const;
    TRange x_CalculateTotalRangeCheckId(const CSeq_id*& id) const;
    void x_CheckId(const CSeq_id*& id) const;
    void x_UpdateId(const CSeq_id*& total_id, const CSeq_id* id) const;
    void x_ChangeToMix(const CSeq_loc& other);
    void x_ChangeToPackedInt(const CSeq_interval& other);
    void x_ChangeToPackedInt(const CSeq_loc& other);
    void x_ChangeToPackedPnt(const CSeq_loc& other);
    void x_InvalidateCache(void);

    enum {
        kDirtyCache = -2,
        kSeveralIds = -3
    };

    mutable TRange m_TotalRangeCache;
    // Seq-id for the whole seq-loc or null if multiple IDs were found
    mutable const CSeq_id* m_IdCache;
};



// Seq-loc iterator class -- iterates all intervals from a seq-loc
// in the correct order.
class NCBI_SEQLOC_EXPORT CSeq_loc_CI
{
public:
    // Options for empty locations processing
    enum EEmptyFlag {
        eEmpty_Skip,    // ignore empty locations
        eEmpty_Allow    // treat empty locations as usual
    };

    CSeq_loc_CI(void);
    CSeq_loc_CI(const CSeq_loc& loc, EEmptyFlag empty_flag = eEmpty_Skip);
    ~CSeq_loc_CI(void);

    CSeq_loc_CI(const CSeq_loc_CI& iter);
    CSeq_loc_CI& operator= (const CSeq_loc_CI& iter);

    CSeq_loc_CI& operator++ (void);
    operator bool (void) const;

    typedef CRange<TSeqPos> TRange;

    // Get seq_id of the current location
    const CSeq_id& GetSeq_id(void) const;
    // Get the range
    TRange         GetRange(void) const;
    // Get strand
    bool IsSetStrand(void) const;
    ENa_strand GetStrand(void) const;
    // Get seq-loc for the current interval
    const CSeq_loc& GetSeq_loc(void) const;

    // Return null if non-fuzzy 
    const CInt_fuzz* GetFuzzFrom(void) const;
    const CInt_fuzz* GetFuzzTo  (void) const;

    // True if the current location is a whole sequence
    bool           IsWhole(void) const;
    // True if the current location is empty
    bool           IsEmpty(void) const;
    // True if the current location is a single point
    bool           IsPoint(void) const;

private:
    // Check the iterator position
    bool x_IsValid(void) const;
    // Check the position, throw exception if not valid
    void x_CheckNotValid(const char* where) const;
    void x_ThrowNotValid(const char* where) const;

    // Process the location, fill the list
    void x_ProcessLocation(const CSeq_loc& loc);

    // Simple location structure: id/from/to
    struct SLoc_Info {
        SLoc_Info(void);
        void SetStrand(ENa_strand strand);

        CConstRef<CSeq_id>  m_Id;
        TRange              m_Range;
        bool                m_IsSetStrand;
        ENa_strand          m_Strand;
        // The original seq-loc for the interval
        CConstRef<CSeq_loc> m_Loc;
        CConstRef<CInt_fuzz> m_Fuzz[2];
    };

    typedef list<SLoc_Info> TLocList;

    // Prevent seq-loc destruction
    CConstRef<CSeq_loc>      m_Location;
    // List of intervals
    TLocList                 m_LocList;
    // Current interval
    TLocList::const_iterator m_CurLoc;
    // Empty locations processing option
    EEmptyFlag               m_EmptyFlag;
};



/////////////////// CSeq_loc inline methods

inline
void CSeq_loc::InvalidateTotalRangeCache(void)
{
    m_TotalRangeCache.SetFrom(TSeqPos(kDirtyCache));
}


inline 
void CSeq_loc::InvalidateIdCache(void)
{
    m_IdCache = 0;
}


inline 
void CSeq_loc::x_InvalidateCache(void)
{
    InvalidateTotalRangeCache();
    InvalidateIdCache();
}


// constructor
inline
CSeq_loc::CSeq_loc(void)
{
    x_InvalidateCache();
}


inline
CSeq_loc::TRange CSeq_loc::GetTotalRange(void) const
{
    TRange range = m_TotalRangeCache;
    if ( range.GetFrom() == TSeqPos(kDirtyCache) )
        range = x_UpdateTotalRange();
    return range;
}


inline
void CSeq_loc::CheckId(const CSeq_id*& id) const
{
    const CSeq_id* my_id = m_IdCache;
    if ( my_id == 0 ) {
        x_CheckId(my_id);
        m_IdCache = my_id;
    }
    x_UpdateId(id, my_id);
}


inline
void CSeq_loc::SetId(const CSeq_id& id)
{
    InvalidateIdCache();
    CRef<CSeq_id> nc_id(new CSeq_id);
    nc_id->Assign(id);
    SetId(*nc_id);
    m_IdCache = nc_id.GetPointer();
}


inline
int CSeq_loc::Compare(const CSeq_loc& loc) const
{
    CSeq_loc::TRange range1 = GetTotalRange();
    CSeq_loc::TRange range2 = loc.GetTotalRange();
    // smallest left extreme first
    if ( range1.GetFrom() != range2.GetFrom() ) {
        return range1.GetFrom() < range2.GetFrom()? -1: 1;
    }

    // longest first
    if ( range1.GetToOpen() != range2.GetToOpen() ) {
        return range1.GetToOpen() < range2.GetToOpen()? 1: -1;
    }
    return 0;
}


/////////////////// end of CSeq_loc inline methods

/////////////////// CSeq_loc_CI inline methods

inline
CSeq_loc_CI::SLoc_Info::SLoc_Info(void)
    : m_Id(0),
      m_IsSetStrand(false),
      m_Strand(eNa_strand_unknown),
      m_Loc(0)
{
    return;
}

inline
void CSeq_loc_CI::SLoc_Info::SetStrand(ENa_strand strand)
{
    m_IsSetStrand = true;
    m_Strand = strand;
}

inline
CSeq_loc_CI& CSeq_loc_CI::operator++ (void)
{
    ++m_CurLoc;
    return *this;
}

inline
bool CSeq_loc_CI::x_IsValid(void) const
{
    return m_CurLoc != m_LocList.end();
}

inline
CSeq_loc_CI::operator bool (void) const
{
    return x_IsValid();
}

inline
void CSeq_loc_CI::x_CheckNotValid(const char* where) const
{
    if ( !x_IsValid() )
        x_ThrowNotValid(where);
}

inline
const CSeq_id& CSeq_loc_CI::GetSeq_id(void) const
{
    x_CheckNotValid("GetSeq_id()");
    return *m_CurLoc->m_Id;
}

inline
CSeq_loc_CI::TRange CSeq_loc_CI::GetRange(void) const
{
    x_CheckNotValid("GetRange()");
    return m_CurLoc->m_Range;
}

inline
bool CSeq_loc_CI::IsSetStrand(void) const
{
    x_CheckNotValid("IsSetStrand()");
    return m_CurLoc->m_IsSetStrand;
}

inline
ENa_strand CSeq_loc_CI::GetStrand(void) const
{
    x_CheckNotValid("GetStrand()");
    return m_CurLoc->m_Strand;
}

inline
const CSeq_loc& CSeq_loc_CI::GetSeq_loc(void) const
{
    x_CheckNotValid("GetSeq_loc()");
    if ( !m_CurLoc->m_Loc ) {
        NCBI_THROW(CException, eUnknown,
            "CSeq_loc_CI::GetSeq_loc() -- NULL seq-loc");
    }
    return *m_CurLoc->m_Loc;
}

inline
const CInt_fuzz* CSeq_loc_CI::GetFuzzFrom(void) const
{
    x_CheckNotValid("GetFuzzFrom()");
    return m_CurLoc->m_Fuzz[0];
}

inline
const CInt_fuzz* CSeq_loc_CI::GetFuzzTo(void) const
{
    x_CheckNotValid("GetFuzzTo()");
    return m_CurLoc->m_Fuzz[1];
}

inline
bool CSeq_loc_CI::IsWhole(void) const
{
    x_CheckNotValid("IsWhole()");
    return m_CurLoc->m_Range.IsWhole();
}

inline
bool CSeq_loc_CI::IsEmpty(void) const
{
    x_CheckNotValid("IsEmpty()");
    return m_CurLoc->m_Range.Empty();
}

inline
bool CSeq_loc_CI::IsPoint(void) const
{
    x_CheckNotValid("IsPoint()");
    return m_CurLoc->m_Range.GetLength() == 1;
}

/////////////////// end of CSeq_loc_CI inline methods


END_objects_SCOPE // namespace ncbi::objects::
END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.40  2004/10/25 18:00:42  shomrat
 * + FlipStrand
 *
 * Revision 1.39  2004/10/20 18:11:40  grichenk
 * Added CSeq_loc::ChangeToMix, ChangeToPackedInt and CSeq_loc_CI::IsSetStrand.
 *
 * Revision 1.38  2004/09/01 15:33:44  grichenk
 * Check strand in GetStart and GetEnd. Circular length argument
 * made optional.
 *
 * Revision 1.37  2004/05/06 16:54:41  shomrat
 * Added methods to set partial left and right
 *
 * Revision 1.36  2004/03/25 15:58:41  gouriano
 * Added possibility to copy and compare serial object non-recursively
 *
 * Revision 1.35  2004/03/16 18:08:56  vasilche
 * Use GetPointer() to avoid ambiguity
 *
 * Revision 1.34  2004/02/17 21:10:58  vasilche
 * Fixed possible race condition in CSeq_loc::CheckId().
 *
 * Revision 1.33  2004/01/29 21:07:08  shomrat
 * Made cache invalidation methods public
 *
 * Revision 1.32  2004/01/28 17:16:31  shomrat
 * Added methods to ease the construction of objects
 *
 * Revision 1.31  2003/12/31 15:36:07  grichenk
 * Moved CompareLocations() from CSeq_feat to CSeq_loc,
 * renamed it to Compare().
 *
 * Revision 1.30  2003/11/21 14:45:00  grichenk
 * Replaced runtime_error with CException
 *
 * Revision 1.29  2003/10/15 15:50:21  ucko
 * CSeq_loc::SetId: add a version that takes a const ID and stores a new copy.
 * CSeq_loc_CI: expose fuzz (if present).
 *
 * Revision 1.28  2003/10/14 16:49:53  dicuccio
 * Added SetId()
 *
 * Revision 1.27  2003/09/22 18:38:13  grichenk
 * Fixed circular seq-locs processing by TestForOverlap()
 *
 * Revision 1.26  2003/09/17 18:39:01  grichenk
 * + GetStart(), GetEnd(), GetCircularLength()
 *
 * Revision 1.25  2003/06/18 16:00:07  vasilche
 * Fixed GetTotalRange() in multithreaded app.
 *
 * Revision 1.24  2003/06/13 17:21:18  grichenk
 * Added seq-id caching for single-id seq-locs
 *
 * Revision 1.23  2003/04/02 15:17:45  grichenk
 * Added flag for CSeq_loc_CI to skip/include empty locations.
 *
 * Revision 1.22  2003/02/06 22:23:29  vasilche
 * Added CSeq_id::Assign(), CSeq_loc::Assign().
 * Added int CSeq_id::Compare() (not safe).
 * Added caching of CSeq_loc::GetTotalRange().
 *
 * Revision 1.21  2003/02/04 16:04:12  dicuccio
 * Changed postfix to prefix operator in op++() - marginally faster
 *
 * Revision 1.20  2003/02/04 15:15:11  grichenk
 * Overrided Assign() for CSeq_loc and CSeq_id
 *
 * Revision 1.19  2003/01/22 20:13:57  vasilche
 * Use more effective COpenRange<> methods.
 *
 * Revision 1.18  2002/12/30 19:37:02  vasilche
 * Rewrote CSeq_loc::GetTotalRange() to avoid using CSeq_loc_CI -
 * it's too expensive.
 *
 * Revision 1.17  2002/12/26 12:43:42  dicuccio
 * Added Win32 export specifiers
 *
 * Revision 1.16  2002/12/23 17:19:26  grichenk
 * Added GetSeq_loc() to CSeq_loc_CI
 *
 * Revision 1.15  2002/12/19 20:21:10  dicuccio
 * Remove post-increment operator
 *
 * Revision 1.14  2002/12/19 20:11:20  grichenk
 * Fixed error message
 *
 * Revision 1.13  2002/10/03 18:49:05  clausen
 * Removed extra whitespace
 *
 * Revision 1.12  2002/10/03 16:37:39  clausen
 * Added GetLabel()
 *
 * Revision 1.11  2002/09/12 21:16:14  kans
 * added IsPartialLeft and IsPartialRight
 *
 * Revision 1.10  2002/06/07 11:54:34  clausen
 * Added related functions comment
 *
 * Revision 1.9  2002/06/06 20:40:51  clausen
 * Moved methods using object manager to objects/util
 *
 * Revision 1.8  2002/05/03 21:28:04  ucko
 * Introduce T(Signed)SeqPos.
 *
 * Revision 1.7  2002/04/17 15:39:06  grichenk
 * Moved CSeq_loc_CI to the seq-loc library
 *
 * Revision 1.6  2002/01/10 18:20:48  clausen
 * Added IsOneBioseq, GetStart, and GetId
 *
 * Revision 1.5  2001/10/22 11:39:49  clausen
 * Added Compare()
 *
 * Revision 1.4  2001/06/25 18:52:02  grichenk
 * Prohibited copy constructor and assignment operator
 *
 * Revision 1.3  2001/01/05 20:11:41  vasilche
 * CRange, CRangeMap were moved to util.
 *
 * Revision 1.2  2001/01/03 16:38:58  vasilche
 * Added CAbstractObjectManager - stub for object manager.
 * CRange extracted to separate file.
 *
 * Revision 1.1  2000/11/17 21:35:02  vasilche
 * Added GetLength() method to CSeq_loc class.
 *
 *
 * ===========================================================================
*/

#endif // OBJECTS_SEQLOC_SEQ_LOC_HPP
