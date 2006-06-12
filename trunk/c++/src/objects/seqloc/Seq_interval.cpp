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
 * Revision 6.12  2006/06/12 20:03:44  grichenk
 * x_IsMinusStrand() uses IsReverse().
 *
 * Revision 6.11  2006/03/16 18:58:30  grichenk
 * Indicate intervals truncated while mapping by fuzz lim tl/tr.
 *
 * Revision 6.10  2005/02/18 15:01:53  shomrat
 * Use ESeqLocExtremes to solve Left/Right ambiguity
 *
 * Revision 6.9  2004/10/25 18:01:33  shomrat
 * + FlipStrand
 *
 * Revision 6.8  2004/05/19 17:26:25  gorelenk
 * Added include of PCH - ncbi_pch.hpp
 *
 * Revision 6.7  2004/05/06 16:55:00  shomrat
 * Added methods to set partial left and right
 *
 * Revision 6.6  2002/09/12 21:17:45  kans
 * IsPartialLeft and IsPartialRight are const
 *
 * Revision 6.5  2002/09/12 20:40:42  kans
 * fixed extra IsSetFuzz_to test in IsPartialRight
 *
 * Revision 6.4  2002/09/12 20:30:49  kans
 * added member functions IsPartialLeft and IsPartialRight
 *
 * Revision 6.3  2002/06/06 20:48:16  clausen
 * Moved IsValid to objects/util/sequence.cpp
 *
 * Revision 6.2  2002/05/03 21:28:18  ucko
 * Introduce T(Signed)SeqPos.
 *
 * Revision 6.1  2002/01/10 19:07:26  clausen
 * Committing add
 *
 *
 * ===========================================================================
 */

// standard includes
#include <ncbi_pch.hpp>
#include <objects/seqloc/Seq_interval.hpp>

// generated includes
#include <objects/general/Int_fuzz.hpp>

// generated classes

BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::

// destructor
CSeq_interval::~CSeq_interval(void)
{
}


bool CSeq_interval::x_IsMinusStrand(void) const
{
    return IsSetStrand() && IsReverse(GetStrand());
}


bool CSeq_interval::IsPartialStart(ESeqLocExtremes ext) const
{
    if (ext == eExtreme_Biological  &&  x_IsMinusStrand()) {
        if (IsSetFuzz_to()) {
            const CInt_fuzz& ifp = GetFuzz_to();
            if (ifp.IsLim()  &&  ifp.GetLim() == CInt_fuzz::eLim_gt) {
                return true;
            }
        }
    } else {
        if (IsSetFuzz_from()) {
            const CInt_fuzz& ifp = GetFuzz_from();
            if (ifp.IsLim()  &&  ifp.GetLim() == CInt_fuzz::eLim_lt) {
                return true;
            }
        }
    }
    return false;
}

bool CSeq_interval::IsPartialStop(ESeqLocExtremes ext) const
{
    if (ext == eExtreme_Biological  &&  x_IsMinusStrand()) {
        if (IsSetFuzz_from()) {
            const CInt_fuzz& ifp = GetFuzz_from();
            if (ifp.IsLim()  &&  ifp.GetLim() == CInt_fuzz::eLim_lt) {
                return true;
            }
        }
    } else {
        if (IsSetFuzz_to()) {
            const CInt_fuzz& ifp = GetFuzz_to();
            if (ifp.IsLim()  &&  ifp.GetLim() == CInt_fuzz::eLim_gt) {
                return true;
            }
        }
    }
    return false;
}


// set / remove e_Lim fuzz on left (5') or right (3') end
void CSeq_interval::SetPartialStart(bool val, ESeqLocExtremes ext)
{
    if (val != IsPartialStart(ext)) {
        if (val) {
            if (ext == eExtreme_Biological  &&  x_IsMinusStrand()) {
                SetFuzz_to().SetLim(CInt_fuzz::eLim_gt);
            } else {
                SetFuzz_from().SetLim(CInt_fuzz::eLim_lt);
            }
        } else {
            if (ext == eExtreme_Biological  &&  x_IsMinusStrand()) {
                ResetFuzz_to();
            } else {
                ResetFuzz_from();
            }
        }
    }
    _ASSERT(val == IsPartialStart(ext));
}


void CSeq_interval::SetPartialStop(bool val, ESeqLocExtremes ext)
{
    if (val != IsPartialStop(ext)) {
        if (val) {
            if (ext == eExtreme_Biological  &&  x_IsMinusStrand()) {
                SetFuzz_from().SetLim(CInt_fuzz::eLim_lt);
            } else {
                SetFuzz_to().SetLim(CInt_fuzz::eLim_gt);
            }
        } else {
            if (ext == eExtreme_Biological  &&  x_IsMinusStrand()) {
                ResetFuzz_from();
            } else {
                ResetFuzz_to();
            }
        }
    }
    _ASSERT(val == IsPartialStop(ext));
}


bool CSeq_interval::IsTruncatedStart(ESeqLocExtremes ext) const
{
    if (ext == eExtreme_Biological  &&  x_IsMinusStrand()) {
        if (IsSetFuzz_to()) {
            const CInt_fuzz& ifp = GetFuzz_to();
            if (ifp.IsLim()  &&  ifp.GetLim() == CInt_fuzz::eLim_tr) {
                return true;
            }
        }
    } else {
        if (IsSetFuzz_from()) {
            const CInt_fuzz& ifp = GetFuzz_from();
            if (ifp.IsLim()  &&  ifp.GetLim() == CInt_fuzz::eLim_tl) {
                return true;
            }
        }
    }
    return false;
}


bool CSeq_interval::IsTruncatedStop(ESeqLocExtremes ext) const
{
    if (ext == eExtreme_Biological  &&  x_IsMinusStrand()) {
        if (IsSetFuzz_from()) {
            const CInt_fuzz& ifp = GetFuzz_from();
            if (ifp.IsLim()  &&  ifp.GetLim() == CInt_fuzz::eLim_tl) {
                return true;
            }
        }
    } else {
        if (IsSetFuzz_to()) {
            const CInt_fuzz& ifp = GetFuzz_to();
            if (ifp.IsLim()  &&  ifp.GetLim() == CInt_fuzz::eLim_tr) {
                return true;
            }
        }
    }
    return false;
}


void CSeq_interval::SetTruncatedStart(bool val, ESeqLocExtremes ext)
{
    if (val != IsTruncatedStart(ext)) {
        if (val) {
            if (ext == eExtreme_Biological  &&  x_IsMinusStrand()) {
                SetFuzz_to().SetLim(CInt_fuzz::eLim_tr);
            } else {
                SetFuzz_from().SetLim(CInt_fuzz::eLim_tl);
            }
        } else {
            if (ext == eExtreme_Biological  &&  x_IsMinusStrand()) {
                ResetFuzz_to();
            } else {
                ResetFuzz_from();
            }
        }
    }
    _ASSERT(val == IsTruncatedStart(ext));
}


void CSeq_interval::SetTruncatedStop(bool val, ESeqLocExtremes ext)
{
    if (val != IsTruncatedStop(ext)) {
        if (val) {
            if (ext == eExtreme_Biological  &&  x_IsMinusStrand()) {
                SetFuzz_from().SetLim(CInt_fuzz::eLim_tl);
            } else {
                SetFuzz_to().SetLim(CInt_fuzz::eLim_tr);
            }
        } else {
            if (ext == eExtreme_Biological  &&  x_IsMinusStrand()) {
                ResetFuzz_from();
            } else {
                ResetFuzz_to();
            }
        }
    }
    _ASSERT(val == IsTruncatedStop(ext));
}


void CSeq_interval::FlipStrand(void)
{
    if (IsSetStrand()) {
        SetStrand(Reverse(GetStrand()));
    }
}


END_objects_SCOPE // namespace ncbi::objects::

END_NCBI_SCOPE

/* Original file checksum: lines: 61, chars: 1895, CRC32: 81b5bfaa */
