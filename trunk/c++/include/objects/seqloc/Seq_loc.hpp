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

//
#include <util/range.hpp>
#include <vector>

BEGIN_NCBI_SCOPE
BEGIN_objects_SCOPE // namespace ncbi::objects::

class CSeq_loc : public CSeq_loc_Base
{
public:
    typedef CSeq_loc_Base Tparent;
    typedef CPacked_seqpnt_Base::TPoints TPoints;
    typedef CPacked_seqint_Base::Tdata   TIntervals;
    typedef CSeq_loc_mix_Base::Tdata     TLocations;

    // constructor
    CSeq_loc(void);
    // destructor
    virtual ~CSeq_loc(void);

    // See related functions in util/sequence.hpp:
    //
    //   TSeqPos GetLength(const CSeq_loc&, CScope*)
    //   bool IsOneBioseq(const CSeq_loc&, CScope*)
    //   const CSeq_id& GetId(const CSeq_loc&, CScope*)
    //   TSeqPos GetStart(const CSeq_loc&, CScope*)
    //   sequence::ECompare Compare(const CSeq_loc&, CSeq_loc&, CScope*)
    //

    typedef CRange<TSeqPos> TRange;

    TRange GetTotalRange(void) const;

    // Appends a label suitable for display (e.g., error messages)
    // label must point to an existing string object
    // Method just returns if label is null. Note this label is NOT
    // GenBank-style.
    void GetLabel(string* label) const;

    // check left (5') or right (3') end of location for e_Lim fuzz
    bool IsPartialLeft  (void) const;
    bool IsPartialRight (void) const;

private:
    // Prohibit copy constructor & assignment operator
    CSeq_loc(const CSeq_loc&);
    CSeq_loc& operator= (const CSeq_loc&);
};



// Seq-loc iterator class -- iterates all intervals from a seq-loc
// in the correct order.
class CSeq_loc_CI
{
public:
    CSeq_loc_CI(void);
    CSeq_loc_CI(const CSeq_loc& loc);
    virtual ~CSeq_loc_CI(void);

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
    ENa_strand GetStrand(void) const;
    // Get seq-loc for the current interval
    const CSeq_loc& GetSeq_loc(void) const;

    // True if the current location is a whole sequence
    bool           IsWhole(void) const;
    // True if the current location is empty
    bool           IsEmpty(void) const;
    // True if the current location is a single point
    bool           IsPoint(void) const;

private:
    // Check the iterator position
    bool x_IsValid(void) const;
    // Check the position, throw runtime_error if not valid
    void x_ThrowNotValid(string where) const;

    // Process the location, fill the list
    void x_ProcessLocation(const CSeq_loc& loc);

    // Simple location structure: id/from/to
    struct SLoc_Info {
        SLoc_Info(void);
        SLoc_Info(const SLoc_Info& loc_info);
        SLoc_Info& operator= (const SLoc_Info& loc_info);

        CConstRef<CSeq_id>  m_Id;
        TRange              m_Range;
        ENa_strand          m_Strand;
        // The original seq-loc for the interval
        CConstRef<CSeq_loc> m_Loc;
    };

    typedef list<SLoc_Info> TLocList;

    // Prevent seq-loc destruction
    CConstRef<CSeq_loc>      m_Location;
    // List of intervals
    TLocList                 m_LocList;
    // Current interval
    TLocList::const_iterator m_CurLoc;
};



/////////////////// CSeq_loc inline methods

// constructor
inline
CSeq_loc::CSeq_loc(void)
{
}


/////////////////// end of CSeq_loc inline methods

/////////////////// CSeq_loc_CI inline methods

inline
CSeq_loc_CI::SLoc_Info::SLoc_Info(void)
    : m_Id(0), m_Strand(eNa_strand_unknown), m_Loc(0)
{
    return;
}

inline
CSeq_loc_CI::SLoc_Info::SLoc_Info(const SLoc_Info& loc_info)
    : m_Id(loc_info.m_Id),
      m_Range(loc_info.m_Range),
      m_Strand(loc_info.m_Strand),
      m_Loc(loc_info.m_Loc)
{
    return;
}

inline
CSeq_loc_CI::SLoc_Info&
CSeq_loc_CI::SLoc_Info::operator= (const SLoc_Info& loc_info)
{
    if (this == &loc_info)
        return *this;
    m_Id = loc_info.m_Id;
    m_Range = loc_info.m_Range;
    m_Strand = loc_info.m_Strand;
    m_Loc = loc_info.m_Loc;
    return *this;
}

inline
CSeq_loc_CI& CSeq_loc_CI::operator++ (void)
{
    m_CurLoc++;
    return *this;
}

inline
CSeq_loc_CI::operator bool (void) const
{
    return x_IsValid();
}

inline
const CSeq_id& CSeq_loc_CI::GetSeq_id(void) const
{
    x_ThrowNotValid("GetSeq_id()");
    return *m_CurLoc->m_Id;
}

inline
CSeq_loc_CI::TRange CSeq_loc_CI::GetRange(void) const
{
    x_ThrowNotValid("GetRange()");
    return m_CurLoc->m_Range;
}

inline
ENa_strand CSeq_loc_CI::GetStrand(void) const
{
    x_ThrowNotValid("GetStrand()");
    return m_CurLoc->m_Strand;
}

inline
const CSeq_loc& CSeq_loc_CI::GetSeq_loc(void) const
{
    x_ThrowNotValid("GetSeq_loc()");
    if ( !m_CurLoc->m_Loc ) {
        throw runtime_error(
            "CSeq_loc_CI::GetSeq_loc() -- NULL seq-loc");
    }
    return *m_CurLoc->m_Loc;
}

inline
bool CSeq_loc_CI::IsWhole(void) const
{
    x_ThrowNotValid("IsWhole()");
    return m_CurLoc->m_Range.IsWholeFrom()  &&
        m_CurLoc->m_Range.IsWholeTo();
}

inline
bool CSeq_loc_CI::IsEmpty(void) const
{
    x_ThrowNotValid("IsEmpty()");
    return m_CurLoc->m_Range.IsEmptyFrom()  &&
        m_CurLoc->m_Range.IsEmptyTo();
}

inline
bool CSeq_loc_CI::IsPoint(void) const
{
    x_ThrowNotValid("IsPoint()");
    return m_CurLoc->m_Range.GetLength() == 1;
}

inline
bool CSeq_loc_CI::x_IsValid(void) const
{
    return m_CurLoc != m_LocList.end();
}

inline
void CSeq_loc_CI::x_ThrowNotValid(string where) const
{
    if ( x_IsValid() )
        return;
    throw runtime_error("CSeq_loc_CI::" + where + " -- iterator is not valid");
}

/////////////////// end of CSeq_loc_CI inline methods


END_objects_SCOPE // namespace ncbi::objects::
END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
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
