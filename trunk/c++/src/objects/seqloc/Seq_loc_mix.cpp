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
 * Author:  Eugene Vasilchenko
 *
 * File Description:
 *   .......
 *
 * Remark:
 *   This code was originally generated by application DATATOOL
 *   using specifications from the ASN data definition file
 *   'seqloc.asn'.
 */

#include <ncbi_pch.hpp>
#include <objects/seqloc/Seq_loc_mix.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seqloc/Seq_point.hpp>


BEGIN_NCBI_SCOPE
BEGIN_objects_SCOPE // namespace ncbi::objects::

CSeq_loc_mix::CSeq_loc_mix(void)
{
    return;
}


CSeq_loc_mix::~CSeq_loc_mix(void)
{
}

bool CSeq_loc_mix::IsPartialLeft (void) const

{
    if ( !Get().empty() ) {
        return Get ().front ()->IsPartialLeft ();
    }
    return false;
}

bool CSeq_loc_mix::IsPartialRight (void) const

{
    if ( !Get().empty() ) {
        return Get ().back ()->IsPartialRight ();
    }
    return false;
}


void CSeq_loc_mix::SetPartialLeft (bool val)
{
    if ( !Set().empty() ) {
        Set().front()->SetPartialLeft(val);
    }
    _ASSERT(val == IsPartialLeft());
}


void CSeq_loc_mix::SetPartialRight(bool val)
{
    if ( !Set().empty() ) {
        Set().back()->SetPartialRight(val);
    }
    _ASSERT(val == IsPartialRight());
}


bool CSeq_loc_mix::IsReverseStrand(void) const
{
    CSeq_loc_mix::Tdata::const_iterator li = Get().begin();
    while ( li != Get().end()  &&  (*li)->IsNull() ) {
        ++li;
    }
    if ( li == Get().end() ) {
        return false;
    }
    bool res = (*li)->IsReverseStrand();
    if ( res ) {
        for ( ++li; li != Get().end(); ++li) {
            if ( (*li)->IsNull() ) {
                continue;
            }
            if (res != (*li)->IsReverseStrand()) {
                return false;
            }
        }
    }
    return res;
}


TSeqPos CSeq_loc_mix::GetStart(TSeqPos /*circular_length*/) const
{
    if ( IsReverseStrand() ) {
        return Get().back()->GetStart();
    }
    return Get().front()->GetStart();
}


TSeqPos CSeq_loc_mix::GetEnd(TSeqPos /*circular_length*/) const
{
    if ( IsReverseStrand() ) {
        return Get().front()->GetEnd();
    }
    return Get().back()->GetEnd();
}


void CSeq_loc_mix::AddSeqLoc(const CSeq_loc& other)
{
    if ( !other.IsMix() ) {
        CRef<CSeq_loc> loc(new CSeq_loc);
        loc->Assign(other);
        Set().push_back(loc);
    } else {
        // "flatten" the other seq-loc
        ITERATE(CSeq_loc_mix::Tdata, li, other.GetMix().Get()) {
            AddSeqLoc(**li);
        }
    }
}


void CSeq_loc_mix::AddSeqLoc(CSeq_loc& other)
{
    if ( !other.IsMix() ) {
        CRef<CSeq_loc> loc(&other);
        Set().push_back(loc);
    } else {
        // "flatten" the other seq-loc
        NON_CONST_ITERATE(CSeq_loc_mix::Tdata, li, other.SetMix().Set()) {
            AddSeqLoc(**li);
        }
    }
}


void CSeq_loc_mix::AddInterval(const CSeq_id& id, TSeqPos from, TSeqPos to,
                               ENa_strand strand)
{
    CRef<CSeq_loc> loc(new CSeq_loc);
    if (from == to) {
        CSeq_point& pnt = loc->SetPnt();
        pnt.SetPoint(from);
        pnt.SetId().Assign(id);
        if (strand != eNa_strand_unknown) {
            pnt.SetStrand(strand);
        }
    } else {
        CSeq_interval& ival = loc->SetInt();
        ival.SetFrom(from);
        ival.SetTo(to);
        ival.SetId().Assign(id);
        if (strand != eNa_strand_unknown) {
            ival.SetStrand(strand);
        }
    }
    Set().push_back(loc);
}


void CSeq_loc_mix::SetStrand(ENa_strand strand)
{
    NON_CONST_ITERATE (Tdata, it, Set()) {
        (*it)->SetStrand(strand);
    }
}

void CSeq_loc_mix::FlipStrand(void)
{
    NON_CONST_ITERATE (Tdata, it, Set()) {
        (*it)->FlipStrand();
    }
}

END_objects_SCOPE // namespace ncbi::objects::
END_NCBI_SCOPE

/*
 * ===========================================================================
 *
 * $Log$
 * Revision 6.19  2005/01/05 18:29:59  shomrat
 * Fixed IsReverseStrand
 *
 * Revision 6.18  2004/12/15 16:26:56  grichenk
 * Ignore NULLs in IsReverseStrand()
 *
 * Revision 6.17  2004/11/19 15:42:33  shomrat
 * + SetStrand
 *
 * Revision 6.16  2004/10/25 18:01:33  shomrat
 * + FlipStrand
 *
 * Revision 6.15  2004/10/22 16:01:20  kans
 * protect IsPartialXXX with Get empty test
 *
 * Revision 6.14  2004/10/21 21:45:07  kans
 * CSeq_loc_mix::SetPartialRight was incorrectly using front instead of back
 *
 * Revision 6.13  2004/09/01 15:33:44  grichenk
 * Check strand in GetStart and GetEnd. Circular length argument
 * made optional.
 *
 * Revision 6.12  2004/05/19 17:26:25  gorelenk
 * Added include of PCH - ncbi_pch.hpp
 *
 * Revision 6.11  2004/05/06 16:55:00  shomrat
 * Added methods to set partial left and right
 *
 * Revision 6.10  2004/01/28 17:18:19  shomrat
 * Added methods to ease the construction of objects
 *
 * Revision 6.9  2003/05/23 20:24:56  ucko
 * +AddInterval (also handles points automatically); CVS log -> end;
 * drop workaround for old WorkShop bug
 *
 * Revision 6.8  2002/09/12 21:19:02  kans
 * added IsPartialLeft and IsPartialRight
 *
 * Revision 6.7  2002/06/06 20:51:51  clausen
 * Moved GetLength to objects/util/sequence.cpp
 *
 * Revision 6.6  2002/05/03 21:28:19  ucko
 * Introduce T(Signed)SeqPos.
 *
 * Revision 6.5  2002/04/22 20:09:08  grichenk
 * -GetTotalRange(), GetRangeMap(), ResetRangeMap()
 *
 * Revision 6.4  2002/01/24 23:29:48  vakatov
 * Note for ourselves that the bug workaround "BW_010" is not needed
 * anymore, and we should get rid of it in about half a year
 *
 * Revision 6.3  2002/01/07 05:20:03  vakatov
 * Workaround for the SUN Forte 6 Update 1,2 compiler's internal bug.
 *
 * Revision 6.2  2001/01/03 16:39:05  vasilche
 * Added CAbstractObjectManager - stub for object manager.
 * CRange extracted to separate file.
 *
 * Revision 6.1  2000/11/17 21:35:10  vasilche
 * Added GetLength() method to CSeq_loc class.
 *
 * ===========================================================================
 */
