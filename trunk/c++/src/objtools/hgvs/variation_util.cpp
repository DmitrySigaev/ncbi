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
 * File Description:
 *   Sample library
 *
 */

#include <ncbi_pch.hpp>
#include <objects/seqloc/Seq_point.hpp>
#include <objects/general/Object_id.hpp>

#include <objects/seqalign/Seq_align.hpp>
#include <objects/seqalign/Spliced_seg.hpp>
#include <objects/seqalign/Spliced_exon.hpp>
#include <objects/seqalign/Product_pos.hpp>
#include <objects/seqalign/Prot_pos.hpp>
#include <objmgr/seq_loc_mapper.hpp>


#include <objects/seq/seqport_util.hpp>

#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqfeat/Variation_ref.hpp>
#include <objects/seqfeat/Variation_inst.hpp>
#include <objects/seqfeat/Delta_item.hpp>
#include <objects/seqfeat/Ext_loc.hpp>


#include <objects/seqfeat/SeqFeatXref.hpp>

#include <objects/seq/Seq_literal.hpp>
#include <objects/seq/Seq_data.hpp>
#include <objects/seq/Numbering.hpp>
#include <objects/seq/Num_ref.hpp>
#include <objects/seq/Annot_descr.hpp>
#include <objects/seq/Annotdesc.hpp>
#include <objects/seq/Seq_descr.hpp>
#include <objects/seqfeat/BioSource.hpp>
#include <objects/general/User_object.hpp>
#include <objects/general/Object_id.hpp>

#include <objects/seqloc/Seq_loc_equiv.hpp>

#include <serial/iterator.hpp>
#include <objmgr/util/sequence.hpp>
#include <objmgr/seq_vector.hpp>
#include <objtools/hgvs/variation_util.hpp>


BEGIN_NCBI_SCOPE

CVariationUtil::ETestStatus CVariationUtil::CheckAssertedAllele(
        const CSeq_feat& variation_feat,
        string* asserted_out,
        string* actual_out)
{
    if(!variation_feat.GetData().IsVariation()) {
        return eNotApplicable;
    }

    CVariation_ref vr;
    vr.Assign(variation_feat.GetData().GetVariation());
    if(!vr.IsSetLocation()) {
        vr.SetLocation().Assign(variation_feat.GetLocation());
    }
    s_PropagateLocsInPlace(vr);


    bool have_asserted_seq = false;
    bool is_ok = true;
    for(CTypeIterator<CVariation_ref> it1(Begin(vr)); it1; ++it1) {
        const CVariation_ref& vr1 = *it1;
        if(vr1.GetData().IsInstance()
           && vr1.GetData().GetInstance().IsSetObservation()
           && vr1.GetData().GetInstance().GetObservation() == CVariation_inst::eObservation_asserted)
        {
            string asserted_seq;
            const CSeq_literal& literal = vr1.GetData().GetInstance().GetDelta().front()->GetSeq().GetLiteral();
            if(literal.GetSeq_data().IsIupacna()) {
                asserted_seq = literal.GetSeq_data().GetIupacna();
                have_asserted_seq = true;
            } else if(literal.GetSeq_data().IsNcbieaa()) {
                asserted_seq = literal.GetSeq_data().GetNcbieaa();
                have_asserted_seq = true;
            }

            //an asserted sequnece may be of the form "A..BC", where ".." is to be interpreted as a
            //gap of arbitrary length - we need to match prefix and suffix separately
            string prefix, suffix;
            string str_tmp = NStr::Replace(asserted_seq, "..", "\t"); //SplitInTwo's delimiter must be single-character
            NStr::SplitInTwo(str_tmp, "\t", prefix, suffix);

            CSeqVector v(vr1.GetLocation(), *m_scope, CBioseq_Handle::eCoding_Iupac);
            string actual_seq;
            v.GetSeqData(v.begin(), v.end(), actual_seq);

            if(   prefix.size() > 0 && !NStr::StartsWith(actual_seq, prefix)
               || suffix.size() > 0 && !NStr::EndsWith(actual_seq, suffix))
            {
                is_ok = false;
                if(asserted_out) {
                    *asserted_out = asserted_seq;
                }
                if(actual_out) {
                    *actual_out = actual_seq;
                }
                break;
            }
        }
    }

    return !have_asserted_seq ? eNotApplicable : is_ok ? ePass : eFail;
}


/*!
 * if variation-feat is not intronic, or alignment is not spliced-seg -> eNotApplicable
 * else if variation is intronic but location is not at exon boundary -> eFail
 * else -> ePass
 */
CVariationUtil::ETestStatus CVariationUtil::CheckExonBoundary(const CSeq_feat& variation_feat, const CSeq_align& aln)
{
    if(!variation_feat.GetData().IsVariation() || !aln.GetSegs().IsSpliced()) {
        return eNotApplicable;
    }

    CVariation_ref vr;
    vr.Assign(variation_feat.GetData().GetVariation());
    if(!vr.IsSetLocation()) {
        vr.SetLocation().Assign(variation_feat.GetLocation());
    }
    s_PropagateLocsInPlace(vr);

    set<TSeqPos> exon_terminal_pts;
    ITERATE(CSpliced_seg::TExons, it, aln.GetSegs().GetSpliced().GetExons()) {
        const CSpliced_exon& exon = **it;
        exon_terminal_pts.insert(exon.GetProduct_start().IsNucpos() ?
                                     exon.GetProduct_start().GetNucpos() :
                                     exon.GetProduct_start().GetProtpos().GetAmin());
        exon_terminal_pts.insert(exon.GetProduct_end().IsNucpos() ?
                                     exon.GetProduct_end().GetNucpos() :
                                     exon.GetProduct_end().GetProtpos().GetAmin());
    }

    bool is_intronic = false;
    bool is_ok = true;
    for(CTypeIterator<CVariation_ref> it1(Begin(vr)); it1; ++it1) {
        const CVariation_ref& vr1 = *it1;
        if(!vr1.GetData().IsInstance()) {
            continue;
        }
        const CSeq_id* id1 = vr1.GetLocation().GetId();
        if(!id1 || !aln.GetSeq_id(0).Equals(*id1)) {
            continue;
        }

        if(vr1.GetData().GetInstance().GetDelta().size() == 0) {
            continue;
        }

        const CDelta_item& first_delta = *vr1.GetData().GetInstance().GetDelta().front();
        const CDelta_item& last_delta = *vr1.GetData().GetInstance().GetDelta().front();

        //check intronic offsets for bio-start
        if(first_delta.IsSetAction() && first_delta.GetAction() == CDelta_item::eAction_offset) {
            is_intronic = true;
            if(exon_terminal_pts.find(vr1.GetLocation().GetStart(eExtreme_Biological)) == exon_terminal_pts.end()) {
                is_ok = false;
            }
        }

        //check intronic offsets for bio-stop
        if(last_delta.IsSetAction() && last_delta.GetAction() == CDelta_item::eAction_offset) {
            is_intronic = true;
            if(exon_terminal_pts.find(vr1.GetLocation().GetStop(eExtreme_Biological)) == exon_terminal_pts.end()) {
                is_ok = false;
            }
        }

        if(!is_ok) {
            break;
        }
    }

    return !is_intronic ? eNotApplicable : is_ok ? ePass : eFail;
}


void CVariationUtil::s_FactorOutLocsInPlace(CVariation_ref& v)
{
    if(!v.GetData().IsSet()) {
        return;
    }

    //round-1: calculate this loc as union of the members
    CRef<CSeq_loc> aggregate_loc(new CSeq_loc(CSeq_loc::e_Mix));
    NON_CONST_ITERATE(CVariation_ref::TData::TSet::TVariations, it, v.SetData().SetSet().SetVariations()) {
        CVariation_ref& vr = **it;
        s_FactorOutLocsInPlace(vr);
        if(vr.IsSetLocation()) {
            aggregate_loc->Add(vr.GetLocation());
        }
    }
    aggregate_loc = aggregate_loc->Merge(CSeq_loc::fSortAndMerge_All, NULL);
    v.SetLocation(*aggregate_loc);

    //round-2: reset the set-member locations if they are the same as this
    NON_CONST_ITERATE(CVariation_ref::TData::TSet::TVariations, it, v.SetData().SetSet().SetVariations()) {
        CVariation_ref& vr = **it;
        if(vr.IsSetLocation() && vr.GetLocation().Equals(v.GetLocation())) {
            vr.ResetLocation();
        }
    }
}

void CVariationUtil::s_PropagateLocsInPlace(CVariation_ref& v)
{
    if(!v.GetData().IsSet()) {
        return;
    }

    NON_CONST_ITERATE(CVariation_ref::TData::TSet::TVariations, it, v.SetData().SetSet().SetVariations()) {
        CVariation_ref& vr = **it;
        if(!vr.IsSetLocation()) {
            vr.SetLocation().Assign(v.GetLocation());
        }
        s_PropagateLocsInPlace(vr);
    }
}

void CVariationUtil::s_ResolveIntronicOffsets(CVariation_ref& v, const CSeq_loc& parent_variation_loc)
{
    const CSeq_loc& variation_loc = v.IsSetLocation() ? v.GetLocation() : parent_variation_loc;

    if(v.GetData().IsSet()) {
        NON_CONST_ITERATE(CVariation_ref::TData::TSet::TVariations, it, v.SetData().SetSet().SetVariations()) {
            s_ResolveIntronicOffsets(**it, variation_loc);
        }
    } else if(v.GetData().IsInstance()) {
        const CDelta_item& delta_first = *v.GetData().GetInstance().GetDelta().front();

        if(variation_loc.IsPnt() && delta_first.IsSetAction() && delta_first.GetAction() == CDelta_item::eAction_offset) {
            if(!v.IsSetLocation()) {
                v.SetLocation().Assign(variation_loc);
            }
            int offset = delta_first.GetSeq().GetLiteral().GetLength()
                       * (delta_first.IsSetMultiplier() ? delta_first.GetMultiplier() : 1)
                       * (v.GetLocation().GetStrand() == eNa_strand_minus ? -1 : 1);
            v.SetLocation().SetPnt().SetPoint() += offset;
            v.SetData().SetInstance().SetDelta().pop_front();
        } else {
            //If the location is not a point, then the offset(s) apply to start and/or stop individually
            if(delta_first.IsSetAction() && delta_first.GetAction() == CDelta_item::eAction_offset) {
                if(!v.IsSetLocation()) {
                    v.SetLocation().Assign(variation_loc);
                }
                CRef<CSeq_loc> range_loc = sequence::Seq_loc_Merge(v.GetLocation(), CSeq_loc::fMerge_SingleRange, NULL);
                TSeqPos& bio_start = range_loc->GetStrand() == eNa_strand_minus ? range_loc->SetInt().SetTo() : range_loc->SetInt().SetFrom();
                int offset = delta_first.GetSeq().GetLiteral().GetLength()
                           * (delta_first.IsSetMultiplier() ? delta_first.GetMultiplier() : 1)
                           * (range_loc->GetStrand() == eNa_strand_minus ? -1 : 1);
                bio_start += offset;
                v.SetLocation().Assign(*range_loc);
                v.SetData().SetInstance().SetDelta().pop_front();
            }

            const CDelta_item& delta_last = *v.GetData().GetInstance().GetDelta().back();
            if(delta_last.IsSetAction() && delta_last.GetAction() == CDelta_item::eAction_offset) {
                if(!v.IsSetLocation()) {
                    v.SetLocation().Assign(variation_loc);
                }
                CRef<CSeq_loc> range_loc = sequence::Seq_loc_Merge(v.GetLocation(), CSeq_loc::fMerge_SingleRange, NULL);
                TSeqPos& bio_end = range_loc->GetStrand() == eNa_strand_minus ? range_loc->SetInt().SetFrom() : range_loc->SetInt().SetTo();
                int offset = delta_last.GetSeq().GetLiteral().GetLength()
                           * (delta_last.IsSetMultiplier() ? delta_last.GetMultiplier() : 1)
                           * (range_loc->GetStrand() == eNa_strand_minus ? -1 : 1);
                bio_end += offset;
                v.SetLocation().Assign(*range_loc);
                v.SetData().SetInstance().SetDelta().pop_back();
            }
        }
    }
}


void CVariationUtil::s_AddIntronicOffsets(CVariation_ref& v, const CSpliced_seg& ss, const CSeq_loc& parent_variation_loc)
{
    const CSeq_loc& vloc = v.IsSetLocation() ? v.GetLocation() : parent_variation_loc;

    if(v.GetData().IsSet()) {
        NON_CONST_ITERATE(CVariation_ref::TData::TSet::TVariations, it, v.SetData().SetSet().SetVariations()) {
            s_AddIntronicOffsets(**it, ss, vloc);
        }
    } else if(v.GetData().IsInstance()) {
        if(!vloc.GetId() || !vloc.GetId()->Equals(ss.GetGenomic_id()))
        {
            NCBI_THROW(CArgException, CArgException::eInvalidArg, "Expected genomic_id in the variation to be the same as in spliced-seg");
        }

        long start = vloc.GetStart(eExtreme_Positional);
        long stop = vloc.GetStop(eExtreme_Positional);

        long closest_start = 0; //closest-exon-boundary for bio-start of variation location
        long closest_stop = 0; //closest-exon-boundary for bio-stop of variation location

        ITERATE(CSpliced_seg::TExons, it, ss.GetExons()) {
            const CSpliced_exon& se = **it;

            if(se.GetGenomic_end() >= start && se.GetGenomic_start() <= start) {
                closest_start = start; //start is within exon - use itself.
            } else {
                if(abs((long)se.GetGenomic_end() - start) < abs(closest_start - start)) {
                    closest_start = (long)se.GetGenomic_end();
                }
                if(abs((long)se.GetGenomic_start() - start) < abs(closest_start - start)) {
                    closest_start = (long)se.GetGenomic_start();
                }
            }

            if(se.GetGenomic_end() >= stop && se.GetGenomic_start() <= stop) {
                closest_stop = stop; //end is within exon - use itself.
            } else {
                if(abs((long)se.GetGenomic_end() - stop) < abs(closest_stop - stop)) {
                    closest_stop = (long)se.GetGenomic_end();
                }
                if(abs((long)se.GetGenomic_start() - stop) < abs(closest_stop - stop)) {
                    closest_stop = (long)se.GetGenomic_start();
                }
            }
        }

        //adjust location
        if(start != closest_start || stop != closest_stop) {
            CRef<CSeq_loc> loc = sequence::Seq_loc_Merge(vloc, CSeq_loc::fMerge_SingleRange, NULL);
            loc->SetInt().SetFrom(closest_start);
            loc->SetInt().SetTo(closest_stop);
            v.SetLocation().Assign(*loc);
        }

        //add offsets
        if(start != closest_start) {
            int offset = start - closest_start;
            CRef<CDelta_item> delta(new CDelta_item);
            delta->SetAction(CDelta_item::eAction_offset);
            delta->SetSeq().SetLiteral().SetLength(abs(offset));

            int sign = (v.GetLocation().GetStrand() == eNa_strand_minus ? -1 : 1) * (offset < 0 ? -1 : 1);
            if(sign < 0) {
                delta->SetMultiplier(-1);
            }
            if(v.GetLocation().GetStrand() == eNa_strand_minus) {
                v.SetData().SetInstance().SetDelta().push_back(delta);
            } else {
                v.SetData().SetInstance().SetDelta().push_front(delta);
            }
        }

        if(stop != closest_stop && start != stop) {
            int offset = stop - closest_stop;
            CRef<CDelta_item> delta(new CDelta_item);
            delta->SetAction(CDelta_item::eAction_offset);
            delta->SetSeq().SetLiteral().SetLength(abs(offset));
            int sign = (v.GetLocation().GetStrand() == eNa_strand_minus ? -1 : 1) * (offset < 0 ? -1 : 1);
            if(sign < 0) {
                delta->SetMultiplier(-1);
            }
            if(v.GetLocation().GetStrand() == eNa_strand_minus) {
                v.SetData().SetInstance().SetDelta().push_front(delta);
            } else {
                v.SetData().SetInstance().SetDelta().push_back(delta);
            }
        }
    }
}


bool IsFirstSubsetOfSecond(const CSeq_loc& aa, const CSeq_loc& bb)
{
    CRef<CSeq_loc> a(new CSeq_loc);
    a->Assign(aa);
    a->ResetStrand();

    CRef<CSeq_loc> b(new CSeq_loc);
    b->Assign(bb);
    b->ResetStrand();

    CRef<CSeq_loc> sub_loc = a->Subtract(*b, CSeq_loc::fSortAndMerge_All, NULL, NULL);
    return !sub_loc->Which() || sequence::GetLength(*sub_loc, NULL) == 0;
}


void CVariationUtil::s_Remap(CVariation_ref& vr, CSeq_loc_Mapper& mapper, const CSeq_loc& parent_variation_loc)
{
    const CSeq_loc& variation_loc = vr.IsSetLocation() ? vr.GetLocation() : parent_variation_loc;

    if(vr.GetData().IsSet()) {
        NON_CONST_ITERATE(CVariation_ref::TData::TSet::TVariations, it, vr.SetData().SetSet().SetVariations()) {
            s_Remap(**it, mapper, variation_loc);
        }
    } else if(vr.GetData().IsInstance()) {
        //remap inst: process inst's locations in delta that are subset of the variation-loc.
        NON_CONST_ITERATE(CVariation_inst::TDelta, it, vr.SetData().SetInstance().SetDelta()) {
            CDelta_item& di = **it;
            if(!di.IsSetSeq() || !di.GetSeq().IsLoc() || !IsFirstSubsetOfSecond(di.GetSeq().GetLoc(), variation_loc)) {
                continue;
            }
            CRef<CSeq_loc> mapped_loc = mapper.Map(di.GetSeq().GetLoc());
            CRef<CSeq_loc> merged_mapped_loc = sequence::Seq_loc_Merge(*mapped_loc, CSeq_loc::fSortAndMerge_All, NULL);
            di.SetSeq().SetLoc().Assign(*merged_mapped_loc);
        }
    }

    //remap the location.
    if(vr.IsSetLocation()) {
        CRef<CSeq_loc> mapped_loc = mapper.Map(vr.GetLocation());
        CRef<CSeq_loc> merged_mapped_loc = sequence::Seq_loc_Merge(*mapped_loc, CSeq_loc::fSortAndMerge_All, NULL);
        vr.SetLocation().Assign(*merged_mapped_loc);
    }
}


CRef<CSeq_feat> CVariationUtil::Remap(const CSeq_feat& variation_feat, const CSeq_align& aln)
{
    CRef<CSeq_feat> feat(new CSeq_feat);
    feat->Assign(variation_feat);

    if(!feat->GetData().IsVariation()) {
        NCBI_THROW(CArgException, CArgException::eInvalidArg, "feature must be of variation-feat type");
    }

    CVariation_ref& vr = feat->SetData().SetVariation();

    //copy the feature's location to root variation's for remapping (will move back when done)
    vr.SetLocation().Assign(feat->GetLocation());
    if(!vr.GetLocation().GetId()) {
        NCBI_THROW(CArgException, CArgException::eInvalidArg, "Can't get unique seq-id for location");
    }

    if(aln.GetSegs().IsSpliced() && aln.GetSegs().GetSpliced().GetGenomic_id().Equals(*vr.GetLocation().GetId())) {
        s_AddIntronicOffsets(vr, aln.GetSegs().GetSpliced(), vr.GetLocation());
    }

    CSeq_align::TDim target_row = -1;
    for(int i = 0; i < 2; i++) {
        if(sequence::IsSameBioseq(*vr.GetLocation().GetId(), aln.GetSeq_id(i), m_scope )) {
            target_row = 1 - i;
        }
    }
    if(target_row == -1) {
        NCBI_THROW(CException, eUnknown, "The alignment has no row for seq-id " + vr.GetLocation().GetId()->AsFastaString());
    }

    CRef<CSeq_loc_Mapper> mapper(new CSeq_loc_Mapper(aln, target_row, m_scope));

    //save the original in ext-locs (for root variation only)
    typedef CVariation_ref::TExt_locs::value_type TExtLoc;
    TExtLoc ext_loc(new TExtLoc::TObjectType);
    ext_loc->SetId().SetStr("mapped-from");
    ext_loc->SetLocation().Assign(vr.GetLocation());
    vr.SetExt_locs().push_back(ext_loc);


    s_Remap(vr, *mapper, vr.GetLocation());

    if(aln.GetSegs().IsSpliced() && aln.GetSegs().GetSpliced().GetGenomic_id().Equals(*vr.GetLocation().GetId())) {
        s_ResolveIntronicOffsets(vr, vr.GetLocation());
    }

    //transfer the root location of the variation back to feat.location
    feat->SetLocation(feat->SetData().SetVariation().SetLocation());
    feat->SetData().SetVariation().ResetLocation();

    return feat;
}



CVariationUtil::SFlankLocs CVariationUtil::CreateFlankLocs(const CSeq_loc& loc, TSeqPos len)
{
    TSignedSeqPos start = sequence::GetStart(loc, m_scope, eExtreme_Positional);
    TSignedSeqPos stop = sequence::GetStop(loc, m_scope, eExtreme_Positional);

    CBioseq_Handle bsh = m_scope->GetBioseqHandle(loc);
    TSignedSeqPos max_pos = bsh.GetInst_Length() - 1;

    SFlankLocs flanks;
    flanks.upstream.Reset(new CSeq_loc);
    flanks.upstream->SetInt().SetId().Assign(sequence::GetId(loc, NULL));
    flanks.upstream->SetInt().SetStrand(sequence::GetStrand(loc, NULL));
    flanks.upstream->SetInt().SetTo(min(max_pos, stop + (TSignedSeqPos)len));
    flanks.upstream->SetInt().SetFrom(max((TSignedSeqPos)0, start - (TSignedSeqPos)len));

    flanks.downstream.Reset(new CSeq_loc);
    flanks.downstream->Assign(*flanks.upstream);

    CSeq_loc& second = sequence::GetStrand(loc, NULL) == eNa_strand_minus ? *flanks.upstream : *flanks.downstream;
    CSeq_loc& first = sequence::GetStrand(loc, NULL) == eNa_strand_minus ? *flanks.downstream : *flanks.upstream;

    if(start == 0) {
        first.SetNull();
    } else {
        first.SetInt().SetTo(start - 1);
    }

    if(stop == max_pos) {
        second.SetNull();
    } else {
        second.SetInt().SetFrom(stop + 1);
    }

    return flanks;
}



///////////////////////////////////////////////////////////////////////////////
//
// Methods and functions pertaining to converting protein variation in precursor coords
//
///////////////////////////////////////////////////////////////////////////////
void CVariationUtil::s_UntranslateProt(const string& prot_str, vector<string>& codons)
{
    if(prot_str.size() != 1) {
        NCBI_THROW(CException, eUnknown, "Expected prot_str of length 1");
    }

    static const string alphabet = "ACGT";
    string codon = "AAA";
    CSeqTranslator translator;
    for(size_t i0 = 0; i0 < 4; i0++) {
        codon[0] = alphabet[i0];
        for(size_t i1 = 0; i1 < 4; i1++) {
            codon[1] = alphabet[i1];
            for(size_t i2 = 0; i2 < 4; i2++) {
                codon[2] = alphabet[i2];
                string prot("");
                translator.Translate(codon, prot, CSeqTranslator::fIs5PrimePartial);
                NStr::ReplaceInPlace(prot, "*", "X"); //Conversion to IUPAC produces "X", but Translate produces "*"

                //LOG_POST(">>>" << codon << " " << prot << " " << prot_str);
                if(prot == prot_str) {
                    codons.push_back(codon);
                }
            }
        }
    }
}

size_t CVariationUtil::s_CountMatches(const string& a, const string& b)
{
    size_t count(0);
    for(size_t i = 0; i <  min(a.size(), b.size()); i++ ) {
        if(a[i] == b[i]) {
            count++;
        }
    }
    return count;
}

void CVariationUtil::s_CalcPrecursorVariationCodon(
        const string& codon_from, //codon on cDNA
        const string& prot_to,    //missense/nonsense AA
        vector<string>& codons_to)  //calculated variation-codons
{
    vector<string> candidates1;
    size_t max_matches(0);
    s_UntranslateProt(prot_to, candidates1);
    codons_to.clear();

    ITERATE(vector<string>, it1, candidates1) {
        size_t matches = s_CountMatches(codon_from, *it1);

//        LOG_POST("CalcPrecursorVariationCodon:" << codon_from << " " << prot_to << " " << *it1 << " " << matches);
        if(matches == 3) {
            //all three bases in a codon matched - we must be processing a silent mutation.
            //in this case we want to consider candidate codons other than itself.
            continue;
        }

        if(matches >= max_matches) {
            if(matches > max_matches) {
                codons_to.clear();
            }
            codons_to.push_back(*it1);
            max_matches = matches;
        }
    }
}

string CVariationUtil::s_CollapseAmbiguities(const vector<string>& seqs)
{
    string collapsed_seq;

    vector<int> bits; //4-bit bitmask denoting whether a nucleotide occurs at this pos at any seq

    typedef const vector<string> TConstStrs;
    ITERATE(TConstStrs, it, seqs) {
        const string& seq = *it;
        if(seq.size() > bits.size()) {
            bits.resize(seq.size());
        }

        for(size_t i= 0; i < seq.size(); i++) {
            char nt = seq[i];
            int m =    (nt == 'T' ? 1
                      : nt == 'G' ? 2
                      : nt == 'C' ? 4
                      : nt == 'A' ? 8 : 0);
            if(!m) {
                NCBI_THROW(CException, eUnknown, "Expected [ACGT] alphabet");
            }

            bits[i] |= m;
        }
    }

    static const string iupac_bases = "NTGKCYSBAWRDMHVN";
    collapsed_seq.resize(bits.size());
    for(size_t i = 0; i < collapsed_seq.size(); i++) {
        collapsed_seq[i] = iupac_bases[bits[i]];
    }
    return collapsed_seq;
}


//vr must be a prot missense or nonsense (inst) with location set; inst must not have offsets.
CRef<CSeq_feat> CVariationUtil::ProtToPrecursor(const CSeq_feat& prot_variation_feat)
{
    if(!prot_variation_feat.GetData().IsVariation()) {
        NCBI_THROW(CArgException, CArgException::eInvalidArg, "Expected variation-feature");
    }

    const CVariation_ref& vr = prot_variation_feat.GetData().GetVariation();

    if(!vr.GetData().IsInstance()) {
        NCBI_THROW(CArgException, CArgException::eInvalidArg, "Expected variation.inst");
    }

    if(!vr.GetData().GetInstance().GetDelta().size() == 1) {
        NCBI_THROW(CArgException, CArgException::eInvalidArg, "Expected single-element delta");
    }

    const CDelta_item& delta = *vr.GetData().GetInstance().GetDelta().front();
    if(delta.IsSetAction() && delta.GetAction() != CDelta_item::eAction_morph) {
        NCBI_THROW(CArgException, CArgException::eInvalidArg, "Expected morph action");
    }

    if(!delta.IsSetSeq() || !delta.GetSeq().IsLiteral() || delta.GetSeq().GetLiteral().GetLength() != 1) {
        NCBI_THROW(CArgException, CArgException::eInvalidArg, "Expected literal of length 1 in inst.");
    }

    CSeq_data variant_prot_seq;
    CSeqportUtil::Convert(delta.GetSeq().GetLiteral().GetSeq_data(), &variant_prot_seq, CSeq_data::e_Iupacaa);


    if(sequence::GetLength(prot_variation_feat.GetLocation(), NULL) != 1) {
        NCBI_THROW(CArgException, CArgException::eInvalidArg, "Expected single-aa location");
    }

    SAnnotSelector sel(CSeqFeatData::e_Cdregion, true);
       //note: sel by product; location is prot; found feature is mrna having this prot as product
    CRef<CSeq_loc_Mapper> prot2precursor_mapper;
    for(CFeat_CI ci(*m_scope, prot_variation_feat.GetLocation(), sel); ci; ++ci) {
        prot2precursor_mapper.Reset(new CSeq_loc_Mapper(ci->GetMappedFeature(), CSeq_loc_Mapper::eProductToLocation, m_scope));
        break;
    }

    if(!prot2precursor_mapper) {
        NCBI_THROW(CException, eUnknown, "Can't create prot2precursor mapper. Is this a prot?");
    }

    CRef<CSeq_loc> nuc_loc = prot2precursor_mapper->Map(prot_variation_feat.GetLocation());
    if(!nuc_loc->IsInt() || sequence::GetLength(*nuc_loc, NULL) != 3) {
        NCBI_THROW(CException, eUnknown, "AA does not remap to a single codon.");
    }


    CSeqVector v(*nuc_loc, m_scope, CBioseq_Handle::eCoding_Iupac);

    string original_allele_codon; //nucleotide allele on the sequence
    v.GetSeqData(v.begin(), v.end(), original_allele_codon);


    vector<string> variant_codons;
    s_CalcPrecursorVariationCodon(original_allele_codon, variant_prot_seq.GetIupacaa(), variant_codons);

    string variant_codon = s_CollapseAmbiguities(variant_codons);

    //If the original and variant codons have terminal bases shared, we can truncate the variant codon and location accordingly.
    while(variant_codon.length() > 1 && variant_codon.at(0) == original_allele_codon.at(0)) {
        variant_codon = variant_codon.substr(1);
        original_allele_codon = variant_codon.substr(1);
        if(nuc_loc->GetStrand() == eNa_strand_minus) {
            nuc_loc->SetInt().SetTo()--;
        } else {
            nuc_loc->SetInt().SetFrom()++;
        }
    }
    while(variant_codon.length() > 1 &&
          variant_codon.at(variant_codon.length() - 1) == original_allele_codon.at(original_allele_codon.length() - 1))
    {
        variant_codon.resize(variant_codon.length() - 1);
        original_allele_codon.resize(variant_codon.length() - 1);
        //Note: normally given a protein, the parent will be a mRNA and the CDS location
        //will have plus strand; however, the parent could be MT, so we can't assume plus strand
        if(nuc_loc->GetStrand() == eNa_strand_minus) {
            nuc_loc->SetInt().SetFrom()++;
        } else {
            nuc_loc->SetInt().SetTo()--;
        }
    }

    CRef<CDelta_item> delta2(new CDelta_item);
    delta2->SetSeq().SetLiteral().SetLength(variant_codon.length());
    delta2->SetSeq().SetLiteral().SetSeq_data().SetIupacna().Set(variant_codon);

    CRef<CSeq_feat> feat(new CSeq_feat);

    //merge loc to convert int of length 1 to a pnt as necessary
    feat->SetLocation(*sequence::Seq_loc_Merge(*nuc_loc, CSeq_loc::fSortAndMerge_All, NULL));
    CVariation_inst& inst = feat->SetData().SetVariation().SetData().SetInstance();
    inst.SetType(variant_codon.length() == 1 ? CVariation_inst::eType_snv : CVariation_inst::eType_mnp);
    inst.SetDelta().push_back(delta2);

    return feat;
}


CRef<CSeq_literal> CVariationUtil::x_GetLiteralAtLoc(const CSeq_loc& loc)
{
    CSeqVector v(loc, *m_scope, CBioseq_Handle::eCoding_Iupac);
    string seq;
    v.GetSeqData(v.begin(), v.end(), seq);
    CRef<CSeq_literal> literal(new CSeq_literal);
    literal->SetLength(seq.length());
    if(v.IsProtein()) {
        literal->SetSeq_data().SetNcbieaa().Set(seq);
    } else if (v.IsNucleotide()) {
        literal->SetSeq_data().SetIupacna().Set(seq);
    }
    return literal;
}

CRef<CSeq_literal> CVariationUtil::s_CatLiterals(const CSeq_literal& a, const CSeq_literal& b)
{
    CRef<CSeq_literal> c(new CSeq_literal);

    if(b.GetLength() == 0) {
        c->Assign(a);
    } else if(a.GetLength() == 0) {
        c->Assign(b);
    } else {
        CSeqportUtil::Append(&(c->SetSeq_data()),
                             a.GetSeq_data(), 0, a.GetLength(),
                             b.GetSeq_data(), 0, b.GetLength());

        c->SetLength(a.GetLength() + b.GetLength());

        if(a.IsSetFuzz() || b.IsSetFuzz()) {
            c->SetFuzz().SetLim(CInt_fuzz::eLim_unk);
        }
    }
    return c;
}



/*!
 * Convert any simple nucleotide variation to delins form, if possible; throw if not.
 * Precondition: location must be set.
 */
void CVariationUtil::ChangeToDelins(CVariation_ref& v)
{
    s_PropagateLocsInPlace(v);
    if(v.GetData().IsSet()) {
        NON_CONST_ITERATE(CVariation_ref::TData::TSet::TVariations, it, v.SetData().SetSet().SetVariations()) {
            ChangeToDelins(**it);
        }
    } else if(v.GetData().IsInstance()) {
        CVariation_inst& inst = v.SetData().SetInstance();
        inst.SetType(CVariation_inst::eType_delins);

        if(inst.GetDelta().size() == 0) {
            CRef<CDelta_item> di(new CDelta_item);
            di->SetSeq().SetLiteral().SetLength(0);
            di->SetSeq().SetLiteral().SetSeq_data().SetIupacna().Set("");
            inst.SetDelta().push_back(di);
        } else if(inst.GetDelta().size() > 1) {
            NCBI_THROW(CArgException, CArgException::eInvalidArg, "Deltas of length >1 are not supported");
        } else {
            CDelta_item& di = *inst.SetDelta().front();


            //convert 'del' to 'replace-with-empty-literal'
            if(di.GetAction() == CDelta_item::eAction_del_at) {
                di.ResetAction();
                di.SetSeq().SetLiteral().SetLength(0);
                di.SetSeq().SetLiteral().SetSeq_data().SetIupacna().Set("");
            }

            //convert 'loc' or 'this'-based deltas to literals
            if(di.GetSeq().IsLoc()) {
                CRef<CSeq_literal> literal = x_GetLiteralAtLoc(di.GetSeq().GetLoc());
                di.SetSeq().SetLiteral(*literal);
            } else if(di.GetSeq().IsThis()) {
                CRef<CSeq_literal> literal = x_GetLiteralAtLoc(v.GetLocation());
                di.SetSeq().SetLiteral(*literal);
            }

            //expand multipliers.
            if(di.IsSetMultiplier()) {
                if(di.GetMultiplier() < 0) {
                    NCBI_THROW(CArgException, CArgException::eInvalidArg, "Encountered negative multiplier");
                } else {
                    CSeq_literal& literal = di.SetSeq().SetLiteral();
                    string str_kernel = literal.GetSeq_data().GetIupacna().Get();
                    literal.SetSeq_data().SetIupacna().Set("");
                    for(int i = 0; i < di.GetMultiplier(); i++) {
                        literal.SetSeq_data().SetIupacna().Set() += str_kernel;
                    }
                    literal.SetLength(literal.GetSeq_data().GetIupacna().Get().size());
                    if(literal.IsSetFuzz()) {
                        literal.SetFuzz().SetLim(CInt_fuzz::eLim_unk);
                    }

                    di.ResetMultiplier();
                    if(di.IsSetMultiplier_fuzz()) {
                        di.SetMultiplier_fuzz().SetLim(CInt_fuzz::eLim_unk);
                    }
                }
            }

            //Convert ins-X-before-loc to 'replace seq@loc with X + seq@loc'
            if(!di.IsSetAction() || di.GetAction() == CDelta_item::eAction_morph) {
                ;  //already done
            } else if(di.GetAction() == CDelta_item::eAction_ins_before) {
                di.ResetAction();
                CRef<CSeq_literal> suffix_literal = x_GetLiteralAtLoc(v.GetLocation());
                CRef<CSeq_literal> cat_literal = s_CatLiterals(di.GetSeq().GetLiteral(), *suffix_literal);
                di.SetSeq().SetLiteral(*cat_literal);
            }
        }
    }
}



/*!
 * Extend or truncate delins to specified location.
 * truncate or attach suffixes/prefixes to seq-literals as necessary).
 *
 * Precondition:
 *    -variation must be a normalized delins (via x_ChangeToDelins)
 *    -loc must be a superset of variation's location.
 */
void CVariationUtil::AdjustDelinsToInterval(CVariation_ref& v, const CSeq_loc& loc)
{
    if(!loc.IsInt()) {
        NCBI_THROW(CArgException, CArgException::eInvalidArg, "Expected Int location");
    }

    if(v.GetData().IsSet()) {
        NON_CONST_ITERATE(CVariation_ref::TData::TSet::TVariations, it, v.SetData().SetSet().SetVariations()) {
            AdjustDelinsToInterval(**it, loc);
        }
    } else if(v.GetData().IsInstance()) {
        CVariation_inst& inst = v.SetData().SetInstance();
        inst.SetType(CVariation_inst::eType_delins);

        CRef<CSeq_loc> sub_loc = v.GetLocation().Subtract(loc, 0, NULL, NULL);
        if(sub_loc->Which() && sequence::GetLength(*sub_loc, NULL) > 0) {
            NCBI_THROW(CArgException, CArgException::eInvalidArg, "Location must be a superset of the variation's loc");
        }

        if(!inst.GetDelta().size() == 1) {
            NCBI_THROW(CArgException, CArgException::eInvalidArg, "Expected single-element delta");
        }

        CDelta_item& delta = *inst.SetDelta().front();

        if(!delta.IsSetSeq() || !delta.GetSeq().IsLiteral()) {
            NCBI_THROW(CArgException, CArgException::eInvalidArg, "Expected literal");
        }

        CRef<CSeq_loc> tmp_loc = sequence::Seq_loc_Merge(loc, CSeq_loc::fMerge_SingleRange, NULL);
        tmp_loc->SetInt().SetFrom(sequence::GetStart(v.GetLocation(), NULL, eExtreme_Positional));
        CRef<CSeq_loc> suffix_loc = sequence::Seq_loc_Subtract(*tmp_loc, v.GetLocation(), CSeq_loc::fSortAndMerge_All, NULL);

        tmp_loc = sequence::Seq_loc_Merge(loc, CSeq_loc::fMerge_SingleRange, NULL);
        tmp_loc->SetInt().SetTo(sequence::GetStop(v.GetLocation(), NULL, eExtreme_Positional));
        CRef<CSeq_loc> prefix_loc = sequence::Seq_loc_Subtract(*tmp_loc, v.GetLocation(), CSeq_loc::fSortAndMerge_All, NULL);

        if(sequence::GetStrand(loc, NULL) == eNa_strand_minus) {
            swap(prefix_loc, suffix_loc);
        }

        CRef<CSeq_literal> prefix_literal = x_GetLiteralAtLoc(*prefix_loc);
        CRef<CSeq_literal> suffix_literal = x_GetLiteralAtLoc(*suffix_loc);

        CRef<CSeq_literal> tmp_literal1 = s_CatLiterals(*prefix_literal, delta.SetSeq().SetLiteral());
        CRef<CSeq_literal> tmp_literal2 = s_CatLiterals(*tmp_literal1, *suffix_literal);
        delta.SetSeq().SetLiteral(*tmp_literal2);
        v.SetLocation().Assign(loc);
    }
}


CRef<CSeq_feat> CVariationUtil::PrecursorToProt(const CSeq_feat& nuc_variation_feat)
{
    /*
     * Method:
     * 1. Normalize variation into a delins form :
     *    location: what is being replaced;     del := seq-literal(location)
     *    delta: what replaces the location;    ins := seq-literal(delta)
     *    E.g. "ins 'AC' before loc" is expressed as "replace seq@loc with 'AC'+seq@loc".
     *         "del@loc" is expressed as "replace seq@loc with ''".
     *
     * 2. Throw if location not completely within CDS.
     *    In the future, might truncate to CDS location:
     *    1. If the location crosses CDS edge (cds-start, or splice-junction):
     *       1. If |ins| == 0 or |ins| == |del| - can truncate the variation to part covered by CDS
     *       2. Else - bail, because we don't know base-for-base correspondence in ins vs del.
     *
     * 3. Extend the location and append suffix/prefix literal up to nearest codon boundaries.
     *  Note that the suffix and prefix for ins and del might be different. Account for the fact
     *  that if part of a codon is deleted, the bases from the downstream codon will replace it.
     *
     * 3. Translate modified ins and del literals and remap location to prot.
     *
     * 4. If translated sequences are the same -> silent variation.
     *    Else, truncate common prefixes and suffixes and adjust location accordingy
     *    (make sure to leave at least one pnt).
     *
     * 5. Attach frameshift based on (|ins|-|del|) % 3
     */

    bool verbose = false;

    if(!nuc_variation_feat.GetData().IsVariation()) {
        NCBI_THROW(CArgException, CArgException::eInvalidArg, "Expected variation-feature");
    }

    const CVariation_ref& nuc_v = nuc_variation_feat.GetData().GetVariation();
    if(!nuc_v.GetData().IsInstance()) {
        NCBI_THROW(CArgException, CArgException::eInvalidArg, "Expected variation.inst");
    }

    if(!nuc_v.GetData().GetInstance().GetDelta().size() == 1) {
        //can't process intronic, etc.
        NCBI_THROW(CArgException, CArgException::eInvalidArg, "Expected single-element delta");
    }

    CRef<CSeq_loc_Mapper> nuc2prot_mapper;
    CRef<CSeq_loc_Mapper> prot2nuc_mapper;

    SAnnotSelector sel(CSeqFeatData::e_Cdregion);
    for(CFeat_CI ci(*m_scope, nuc_variation_feat.GetLocation(), sel); ci; ++ci) {
        nuc2prot_mapper.Reset(new CSeq_loc_Mapper(ci->GetMappedFeature(), CSeq_loc_Mapper::eLocationToProduct, m_scope));
        prot2nuc_mapper.Reset(new CSeq_loc_Mapper(ci->GetMappedFeature(), CSeq_loc_Mapper::eProductToLocation, m_scope));
        break;
    }

    if(!prot2nuc_mapper) {
        NCBI_THROW(CException, eUnknown, "Could not create prot->nuc mapper");
    }

    CRef<CVariation_ref> v(new CVariation_ref);
    v->Assign(nuc_variation_feat.GetData().GetVariation());
    if(!v->IsSetLocation()) {
        v->SetLocation().Assign(nuc_variation_feat.GetLocation());
        if(!v->GetLocation().GetId()) {
            NCBI_THROW(CArgException, CArgException::eInvalidArg, "Expected variation's location to have unique seq-id");
        }
    }

    if(verbose) NcbiCerr << "Original variation: " << MSerial_AsnText << *v;

    ChangeToDelins(*v);

    if(verbose) NcbiCerr << "Normalized variation: " << MSerial_AsnText << *v;


    const CDelta_item& delta = *v->GetData().GetInstance().GetDelta().front();

    bool have_frameshift = ((long)sequence::GetLength(v->GetLocation(), NULL) - (long)delta.GetSeq().GetLiteral().GetLength()) % 3 != 0;

    CRef<CSeq_loc> prot_loc = nuc2prot_mapper->Map(v->GetLocation());
    CRef<CSeq_loc> codons_loc = prot2nuc_mapper->Map(*prot_loc);
    codons_loc->SetId(*v->GetLocation().GetId()); //restore the original id, as mapping forward and back may have changed the type

    if(verbose) NcbiCerr << "Prot-loc: " << MSerial_AsnText << *prot_loc;

    if(verbose) NcbiCerr << "Codons-loc: " << MSerial_AsnText << *codons_loc;

    //extend codons-loc by two bases downstream, since a part of the next
    //codon may become part of the variation (e.g. 1-base deletion in a codon
    //results in first base of the downstream codon becoming part of modified one)
    //If, on the other hand, the downstream codon does not participate, there's
    //only two bases if it, so it won't get translated.
    SFlankLocs flocs = CreateFlankLocs(*codons_loc, 2);
    CRef<CSeq_loc> codons_loc_ext = sequence::Seq_loc_Add(*codons_loc, *flocs.downstream, CSeq_loc::fSortAndMerge_All, NULL);

    if(verbose) NcbiCerr << "Codons-loc-ext: " << MSerial_AsnText << *codons_loc_ext;

    AdjustDelinsToInterval(*v, *codons_loc_ext);

    CSeq_literal& literal = v->SetData().SetInstance().SetDelta().front()->SetSeq().SetLiteral();
    int prot_literal_len = literal.GetLength() / 3;
    literal.SetLength(prot_literal_len * 3); //divide by 3 and multiply by 3 to truncate to codon boundary.
    literal.SetSeq_data().SetIupacna().Set().resize(literal.GetLength());

    if(verbose) NcbiCerr << "Adjusted variation: " << MSerial_AsnText << *v;

    string prot_delta_str("");

    CSeqTranslator translator;
    translator.Translate(
            delta.GetSeq().GetLiteral().GetSeq_data().GetIupacna(),
            prot_delta_str,
            CSeqTranslator::fIs5PrimePartial);
    NStr::ReplaceInPlace(prot_delta_str, "*", "X"); //Conversion to IUPAC produces "X", but Translate produces "*"

    literal.SetLength(prot_delta_str.size());
    literal.SetSeq_data().SetNcbieaa().Set(prot_delta_str);

    CRef<CSeq_literal> ref_prot_literal = x_GetLiteralAtLoc(*prot_loc);

    if(literal.GetLength() == 0) {
        v->SetData().SetInstance().SetType(CVariation_inst::eType_del);
        v->SetData().SetInstance().SetDelta().clear();
    } else if(ref_prot_literal->GetLength() != literal.GetLength()) {
        v->SetData().SetInstance().SetType(CVariation_inst::eType_prot_other);
    } else {
        //sequence of the same length
        if(ref_prot_literal->GetSeq_data().GetNcbieaa().Get() == prot_delta_str) {
            v->SetData().SetInstance().SetType(CVariation_inst::eType_prot_silent);
        } else if(NStr::Find(prot_delta_str, "X") != NPOS) {
            v->SetData().SetInstance().SetType(CVariation_inst::eType_prot_nonsense);
        } else {
            v->SetData().SetInstance().SetType(CVariation_inst::eType_prot_missense);
        }
    }

    CRef<CSeq_feat> prot_variation_feat(new CSeq_feat);
    prot_variation_feat->SetLocation(*prot_loc);
    prot_variation_feat->SetData().SetVariation(*v);
    v->ResetLocation();

    if(have_frameshift) {
        CVariation_ref::TConsequence::value_type consequence(new CVariation_ref::TConsequence::value_type::TObjectType);
        consequence->SetFrameshift();
        v->SetConsequence().push_back(consequence);
    }

    CRef<CUser_object> uo(new CUser_object);
    uo->SetType().SetStr("HGVS");
    uo->AddField("reference_sequence", ref_prot_literal->GetSeq_data().GetNcbieaa());
    v->SetExt(*uo);

    return prot_variation_feat;
}


END_NCBI_SCOPE

