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
 * Author:  .......
 *
 * File Description:
 *   .......
 *
 * Remark:
 *   This code was originally generated by application DATATOOL
 *   using specifications from the ASN data definition file
 *   'seqloc.asn'.
 *
 * ---------------------------------------------------------------------------
 * $Log$
 * Revision 6.7  2004/05/19 17:26:25  gorelenk
 * Added include of PCH - ncbi_pch.hpp
 *
 * Revision 6.6  2004/05/06 16:55:00  shomrat
 * Added methods to set partial left and right
 *
 * Revision 6.5  2002/09/12 21:17:45  kans
 * IsPartialLeft and IsPartialRight are const
 *
 * Revision 6.4  2002/09/12 20:39:30  kans
 * added member functions IsPartialLeft and IsPartialRight
 *
 * Revision 6.3  2002/06/06 20:49:00  clausen
 * Moved IsValid to objects/util/sequence.cpp
 *
 * Revision 6.2  2002/05/03 21:28:19  ucko
 * Introduce T(Signed)SeqPos.
 *
 * Revision 6.1  2002/01/10 19:12:12  clausen
 * Committing add
 *
 *
 * ===========================================================================
 */
#include <ncbi_pch.hpp>
#include <objects/seqloc/Seq_point.hpp>

// generated classes

BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::


// destructor
CSeq_point::~CSeq_point(void)
{
}


bool CSeq_point::x_IsMinusStrand(void) const
{
    ENa_strand strand = eNa_strand_unknown;
    if ( IsSetStrand() ) {
        strand = GetStrand();
    }
    return (strand == eNa_strand_minus)  ||  (strand == eNa_strand_both_rev);
}


bool CSeq_point::IsPartialLeft (void) const
{
    if (x_IsMinusStrand()) {
        if (IsSetFuzz ()) {
            const CInt_fuzz & ifp = GetFuzz ();
            if (ifp.IsLim ()  &&  ifp.GetLim () == CInt_fuzz::eLim_gt) {
                return true;
            }
        }
    } else {
        if (IsSetFuzz ()) {
            const CInt_fuzz & ifp = GetFuzz ();
            if (ifp.IsLim ()  &&  ifp.GetLim () == CInt_fuzz::eLim_lt) {
                return true;
            }
        }
    }
    return false;
}

bool CSeq_point::IsPartialRight (void) const
{
    if (x_IsMinusStrand()) {
        if (IsSetFuzz ()) {
            const CInt_fuzz & ifp = GetFuzz ();
            if (ifp.IsLim ()  &&  ifp.GetLim () == CInt_fuzz::eLim_lt) {
                return true;
            }
        }
    } else {
        if (IsSetFuzz ()) {
            const CInt_fuzz & ifp = GetFuzz ();
            if (ifp.IsLim ()  &&  ifp.GetLim () == CInt_fuzz::eLim_gt) {
                return true;
            }
        }
    }
    return false;
}


// set / remove e_Lim fuzz on left (5') or right (3') end
void CSeq_point::SetPartialLeft (bool val)
{
    if (val == IsPartialLeft()) {
        return;
    }

    if (val) {
        SetFuzz().SetLim(x_IsMinusStrand() ? CInt_fuzz::eLim_gt : CInt_fuzz::eLim_lt);
    } else {
        ResetFuzz();
    }
    _ASSERT(val == IsPartialLeft());
}


void CSeq_point::SetPartialRight(bool val)
{
    if (val == IsPartialRight()) {
        return;
    }

    if ( val ) {
        SetFuzz().SetLim(x_IsMinusStrand() ? CInt_fuzz::eLim_lt : CInt_fuzz::eLim_gt);
    } else {
        ResetFuzz();
    }
    _ASSERT(val == IsPartialRight());
}


END_objects_SCOPE // namespace ncbi::objects::

END_NCBI_SCOPE

/* Original file checksum: lines: 61, chars: 1886, CRC32: 5f7b1244 */
