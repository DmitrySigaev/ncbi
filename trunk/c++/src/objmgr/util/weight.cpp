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
* Author:  Aaron Ucko
*
* File Description:
*   Weights for protein sequences
*/

#include <corelib/ncbistd.hpp>
#include <objects/objmgr/bioseq_handle.hpp>
#include <objects/objmgr/feat_ci.hpp>
#include <objects/objmgr/seq_vector.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/seq/Seq_inst.hpp>
#include <objects/seqfeat/Prot_ref.hpp>
#include <objects/seqfeat/SeqFeatData.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/util/weight.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

// By NCBIstdaa:
//  A  B  C  D  E  F  G  H  I  K  L  M  N  P  Q  R  S  T  V  W  X  Y  Z  U  *
static const int kNumC[] =
{0, 3, 4, 3, 4, 5, 9, 2, 6, 6, 6, 6, 5, 4, 5, 5, 6, 3, 4, 5,11, 0, 9, 5, 3, 0};
static const int kNumH[] =
{0, 5, 5, 5, 5, 7, 9, 3, 7,11,12,11, 9, 6, 7, 8,12, 5, 7, 9,10, 0, 9, 7, 5, 0};
static const int kNumN[] =
{0, 1, 1, 1, 1, 1, 1, 1, 3, 1, 2, 1, 1, 2, 1, 2, 4, 1, 1, 1, 2, 0, 1, 1, 1, 0};
static const int kNumO[] =
{0, 1, 3, 1, 3, 3, 1, 1, 1, 1, 1, 1, 1, 2, 1, 2, 1, 2, 2, 1, 1, 0, 2, 3, 1, 0};
static const int kNumS[] =
{0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static const int kNumSe[] =
{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0};
static const size_t kMaxRes = sizeof(kNumC) / sizeof(*kNumC) - 1;


double GetProteinWeight(CSeqVector& v, TSeqPos start, TSeqPos end)
    THROWS((CBadResidueException))
{
    v.SetCoding(CSeq_data::e_Ncbistdaa);

    if (start < 0) {
        start = 0;
    }
    if (end >= v.size()) {
        end = v.size() - 1;
    }

    // Start with water (H2O)
    TSeqPos c = 0, h = 2, n = 0, o = 1, s = 0, se = 0;

    for (TSeqPos i = start;  i <= end;  i++) {
        CSeqVector::TResidue res = v[i];
        if ( res >= kMaxRes  ||  !kNumC[res] ) {
            THROW1_TRACE(CBadResidueException,
                         "GetProteinWeight: bad residue");
        }
        c  += kNumC [res];
        h  += kNumH [res];
        n  += kNumN [res];
        o  += kNumO [res];
        s  += kNumS [res];
        se += kNumSe[res];
    }
    return 12.01115 * c + 1.0079 * h + 14.0067 * n + 15.9994 * o + 32.064 * s
        + 78.96 * se;
}


void GetProteinWeights(CBioseq_Handle& handle, TWeights& weights)
{
    CBioseq_Handle::TBioseqCore core = handle.GetBioseqCore();
    if (core->GetInst().GetMol() != CSeq_inst::eMol_aa) {
        THROW1_TRACE(runtime_error, "GetMolecularWeights requires a protein!");
    }
    weights.clear();

    set<CConstRef<CSeq_loc> > locations;
    CSeqVector v = handle.GetSeqVector();
    CSeq_loc* whole = new CSeq_loc;
    whole->SetWhole(*handle.GetSeqId());

    CConstRef<CSeq_feat> signal;

    // Look for explicit markers: ideally cleavage products (mature
    // peptides), but possibly just signal peptides
    for (CFeat_CI feat(handle, 0, 0, CSeqFeatData::e_not_set,
                       CFeat_CI::eResolve_TSE);
         feat;  ++feat) {
        bool is_mature = false, is_signal = false;
        const CSeqFeatData& data = feat->GetData();
        switch (data.Which()) {
        case CSeqFeatData::e_Prot:
            switch (data.GetProt().GetProcessed()) {
            case CProt_ref::eProcessed_mature:         is_mature = true; break;
            case CProt_ref::eProcessed_signal_peptide: is_signal = true; break;
            }
            break;

        case CSeqFeatData::e_Region:
            if (!NStr::CompareNocase(data.GetRegion(), "mature chain")
                ||  !NStr::CompareNocase(data.GetRegion(),
                                         "processed active peptide")) {
                is_mature = true;
            } else if (!NStr::CompareNocase(data.GetRegion(), "signal")) {
                is_signal = true;
            }
            break;

        case CSeqFeatData::e_Site:
            if (data.GetSite() == CSeqFeatData::eSite_signal_peptide) {
                is_signal = true;
            }
            break;

        default:
            break;
        }

        if (is_mature) {
            locations.insert(&feat->GetLocation());
        } else if (is_signal  &&  signal.Empty()) {
            signal = &*feat;
        }
    }

    if (locations.empty()) {
        v.SetCoding(CSeq_data::e_Ncbistdaa); // so check for Met will work
        if ( signal.NotEmpty() ) {
            // Expects to see at beginning; is this assumption safe?
            CSeq_interval& interval = whole->SetInt();
            interval.SetFrom(signal->GetLocation().GetTotalRange().GetTo() + 1);
            interval.SetTo(v.size() - 1);
            interval.SetId(*core->GetId().front());                
        } else if (v[0] == 12) { // Treat initial methionine as start codon
            CSeq_interval& interval = whole->SetInt();
            interval.SetFrom(1);
            interval.SetTo(v.size() - 1);
            interval.SetId(*core->GetId().front());
        }
        locations.insert(whole);
    }

    iterate(set<CConstRef<CSeq_loc> >, it, locations) {
        // Assumes contiguous
        CSeq_loc::TRange range = (*it)->GetTotalRange();
        try {
            weights[*it] = GetProteinWeight(v, range.GetFrom(), range.GetTo());
        } catch (CBadResidueException) {
            // Silently elide
        }
    }
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ===========================================================================
* $Log$
* Revision 1.7  2002/05/06 17:12:29  ucko
* Take advantage of new eResolve_TSE option.
*
* Revision 1.6  2002/05/06 16:11:49  ucko
* Update for new OM; move CVS log to end.
*
* Revision 1.5  2002/05/06 03:39:13  vakatov
* OM/OM1 renaming
*
* Revision 1.4  2002/05/03 21:28:20  ucko
* Introduce T(Signed)SeqPos.
*
* Revision 1.3  2002/04/09 20:58:09  ucko
* Look for "processed active peptide" in addition to "mature chain";
* SWISS-PROT changed their labels.
*
* Revision 1.2  2002/03/21 18:57:31  ucko
* Fix check for initial Met.
*
* Revision 1.1  2002/03/06 22:08:40  ucko
* Add code to calculate protein weights.
* ===========================================================================
*/
