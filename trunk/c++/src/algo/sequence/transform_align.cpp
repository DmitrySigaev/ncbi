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
 * Authors:  Vyacheslav Chetvernin
 *
 * File Description: Alignment transformations
 *
 */
#include <ncbi_pch.hpp>
#include <algo/sequence/gene_model.hpp>
#include <objects/seqalign/seqalign__.hpp>
#include <objects/seqloc/Na_strand.hpp>
#include <objects/general/User_object.hpp>

#include <objmgr/bioseq_handle.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/feat_ci.hpp>

#include "feature_generator.hpp"

BEGIN_NCBI_SCOPE

USING_SCOPE(objects);


namespace {

void GetExonStructure(const CSpliced_seg& spliced_seg, vector<SExon>& exons)
{
    bool is_protein = (spliced_seg.GetProduct_type()==CSpliced_seg::eProduct_type_protein);

    ENa_strand product_strand = spliced_seg.IsSetProduct_strand() ? spliced_seg.GetProduct_strand() : eNa_strand_unknown;
    ENa_strand genomic_strand = spliced_seg.IsSetGenomic_strand() ? spliced_seg.GetGenomic_strand() : eNa_strand_unknown;

    exons.resize(spliced_seg.GetExons().size());
    int i = 0;
    ITERATE(CSpliced_seg::TExons, it, spliced_seg.GetExons()) {
        const CSpliced_exon& exon = **it;
        SExon& exon_struct = exons[i++];

        const CProduct_pos& prod_from = exon.GetProduct_start();
        const CProduct_pos& prod_to = exon.GetProduct_end();

        if (is_protein) {
            const CProt_pos& prot_from = prod_from.GetProtpos();
            exon_struct.prod_from = prot_from.GetAmin()*3;
            if (prot_from.IsSetFrame())
                exon_struct.prod_from += prot_from.GetFrame()-1;

            const CProt_pos& prot_to = prod_to.GetProtpos();
            exon_struct.prod_to = prot_to.GetAmin()*3;
                if (prot_to.IsSetFrame())
                exon_struct.prod_to += prot_to.GetFrame()-1;
        } else {
            exon_struct.prod_from = prod_from.GetNucpos();
            exon_struct.prod_to = prod_to.GetNucpos();
            if (product_strand == eNa_strand_minus) {
                swap(exon_struct.prod_from, exon_struct.prod_to);
                exon_struct.prod_from = -exon_struct.prod_from;
                exon_struct.prod_to = -exon_struct.prod_to;
            }
        }

        exon_struct.genomic_from = exon.GetGenomic_start();
        exon_struct.genomic_to = exon.GetGenomic_end();
        
        if (genomic_strand == eNa_strand_minus) {
            swap(exon_struct.genomic_from, exon_struct.genomic_to);
            exon_struct.genomic_from = -exon_struct.genomic_from;
            exon_struct.genomic_to = -exon_struct.genomic_to;
        }
    }
}

}

void CFeatureGenerator::SImplementation::StitchSmallHoles(CSeq_align& align)
{
    CSpliced_seg& spliced_seg = align.SetSegs().SetSpliced();

    if (!spliced_seg.CanGetExons() || spliced_seg.GetExons().size() < 2)
        return;

    vector<SExon> exons;
    GetExonStructure(spliced_seg, exons);

    bool is_protein = (spliced_seg.GetProduct_type()==CSpliced_seg::eProduct_type_protein);

    ENa_strand product_strand = spliced_seg.IsSetProduct_strand() ? spliced_seg.GetProduct_strand() : eNa_strand_unknown;
    ENa_strand genomic_strand = spliced_seg.IsSetGenomic_strand() ? spliced_seg.GetGenomic_strand() : eNa_strand_unknown;

    int product_min_pos;
    int product_max_pos;
    if (product_strand != eNa_strand_minus) {
        product_min_pos = 0;
        if (spliced_seg.IsSetPoly_a()) {
            product_max_pos = spliced_seg.GetPoly_a()-1;
        } if (spliced_seg.IsSetProduct_length()) {
            product_max_pos = spliced_seg.GetProduct_length()-1;
            if (is_protein)
                product_max_pos *= 3;
        } else {
            product_max_pos = exons.back().prod_to;
        }
    } else {
        if (spliced_seg.IsSetProduct_length()) {
            product_min_pos = -int(spliced_seg.GetProduct_length())+1;
            if (is_protein)
                product_min_pos = product_min_pos*3-2;
        } else {
            product_min_pos = exons[0].prod_from;
        }
        if (spliced_seg.IsSetPoly_a()) {
            product_max_pos = -int(spliced_seg.GetPoly_a())+1;
        } else {
            product_max_pos = 0;
        }
    }

    CSpliced_seg::TExons::iterator it = spliced_seg.SetExons().begin();
    CRef<CSpliced_exon> prev_exon = *it;
    int i = 1;
    for (++it; it != spliced_seg.SetExons().end();  ++i, prev_exon = *it++) {
        CSpliced_exon& exon = **it;

        bool donor_set = prev_exon->IsSetDonor_after_exon();
        bool acceptor_set = exon.IsSetAcceptor_before_exon();

        if(donor_set && acceptor_set && exons[i-1].prod_to + 1 == exons[i].prod_from) {
            continue;
        }

        _ASSERT( exons[i].prod_from > exons[i-1].prod_to );
        TSeqPos prod_hole_len = exons[i].prod_from - exons[i-1].prod_to -1;
        _ASSERT( exons[i].genomic_from > exons[i-1].genomic_to );
        TSeqPos genomic_hole_len = exons[i].genomic_from - exons[i-1].genomic_to -1;

        if (prod_hole_len >= m_min_intron || genomic_hole_len >= m_min_intron)
            continue;

        if (is_protein || product_strand != eNa_strand_minus) {
            prev_exon->SetProduct_end().Assign( exon.GetProduct_end() );
        } else {
            prev_exon->SetProduct_start().Assign( exon.GetProduct_start() );
        }
        
        if (genomic_strand != eNa_strand_minus) {
            prev_exon->SetGenomic_end() = exon.GetGenomic_end();
        } else {
            prev_exon->SetGenomic_start() = exon.GetGenomic_start();
        }

        if (!prev_exon->IsSetParts() || prev_exon->GetParts().empty()) {
            CRef< CSpliced_exon_chunk > part(new CSpliced_exon_chunk);
            part->SetMatch(exons[i-1].prod_to-exons[i-1].prod_from+1);
            prev_exon->SetParts().push_back(part);
        }

        if (prod_hole_len == genomic_hole_len) {
            CRef< CSpliced_exon_chunk > part(new CSpliced_exon_chunk);
            part->SetMismatch(prod_hole_len);
            prev_exon->SetParts().push_back(part);
        } else {
            int max_hole_len = max(prod_hole_len, genomic_hole_len);
            int min_hole_len = min(prod_hole_len, genomic_hole_len);
            if (min_hole_len > 1) {
                CRef< CSpliced_exon_chunk > part(new CSpliced_exon_chunk);
                part->SetMismatch(min_hole_len/2);
                prev_exon->SetParts().push_back(part);
            }
            {
                CRef< CSpliced_exon_chunk > part(new CSpliced_exon_chunk);
                if (prod_hole_len < genomic_hole_len) {
                    part->SetGenomic_ins(max_hole_len - min_hole_len);
                } else {
                    part->SetProduct_ins(max_hole_len - min_hole_len);
                }
                prev_exon->SetParts().push_back(part);
            }
            if (min_hole_len > 0) {
                CRef< CSpliced_exon_chunk > part(new CSpliced_exon_chunk);
                part->SetMismatch(min_hole_len-min_hole_len/2);
                prev_exon->SetParts().push_back(part);
            }
        }
        if (!exon.IsSetParts() || exon.GetParts().empty()) {
            CRef< CSpliced_exon_chunk > part(new CSpliced_exon_chunk);
            part->SetMatch(exons[i].prod_to-exons[i].prod_from+1);
            prev_exon->SetParts().push_back(part);
        } else {
            prev_exon->SetParts().splice(prev_exon->SetParts().end(), exon.SetParts());
        }

        // TO DO: Recalculate or approximate scores
        prev_exon->SetScores().Set().splice(prev_exon->SetScores().Set().end(), exon.SetScores().Set());
        if (exon.IsSetDonor_after_exon()) {
            prev_exon->SetDonor_after_exon().Assign( exon.GetDonor_after_exon() );
        } else {
            prev_exon->ResetDonor_after_exon();
        }

        exons[i].prod_from = exons[i-1].prod_from;
        exons[i].genomic_from = exons[i-1].genomic_from;

        prev_exon->SetPartial(
                             (product_min_pos < exons[i-1].prod_from  && !prev_exon->IsSetAcceptor_before_exon()) ||
                             (exons[i].prod_to < product_max_pos  && !prev_exon->IsSetDonor_after_exon()));

        if (exon.IsSetExt()) {
            prev_exon->SetExt().splice(prev_exon->SetExt().end(), exon.SetExt());
        }

        CSpliced_seg::TExons::iterator save_it = it;
        --save_it;
        spliced_seg.SetExons().erase(it);
        it = save_it;

    }
}

void CFeatureGenerator::SImplementation::TrimHolesToCodons(CSeq_align& align)
{
    CSpliced_seg& spliced_seg = align.SetSegs().SetSpliced();

    if (!spliced_seg.CanGetExons() || spliced_seg.GetExons().size() < 2)
        return;

    bool is_protein = (spliced_seg.GetProduct_type()==CSpliced_seg::eProduct_type_protein);

    ENa_strand product_strand = spliced_seg.IsSetProduct_strand() ? spliced_seg.GetProduct_strand() : eNa_strand_unknown;
    ENa_strand genomic_strand = spliced_seg.IsSetGenomic_strand() ? spliced_seg.GetGenomic_strand() : eNa_strand_unknown;

    TSignedSeqPos cds_start = 0;
    if (!is_protein) {
        if (!spliced_seg.CanGetProduct_id())
            return;
        cds_start = GetCdsStart(spliced_seg.GetProduct_id());
        if (cds_start == -1)
            return;
        if (product_strand == eNa_strand_minus) {
            NCBI_THROW(CException, eUnknown,
                       "TrimHolesToCodons(): "
                       "Reversed mRNA with CDS");
        }
    }

    vector<SExon> exons;
    GetExonStructure(spliced_seg, exons);
    _ASSERT( exons.size() == spliced_seg.GetExons().size() );

    int frame_offset = (exons.back().prod_to/3+1)*3+cds_start; // to make modulo operands always positive

    vector<SExon>::iterator right_exon_it = exons.begin();
    CSpliced_seg::TExons::iterator right_spl_exon_it = spliced_seg.SetExons().begin();

    while (++right_exon_it     !=exons.end() &&
           ++right_spl_exon_it != spliced_seg.SetExons().end()) {

        vector<SExon>::reverse_iterator left_exon_it(right_exon_it); 
        CSpliced_seg::TExons::reverse_iterator left_spl_exon_it(right_spl_exon_it);

        bool donor_set = (*left_spl_exon_it)->IsSetDonor_after_exon();
        bool acceptor_set = (*right_spl_exon_it)->IsSetAcceptor_before_exon();

        if(donor_set && acceptor_set && left_exon_it->prod_to + 1 == right_exon_it->prod_from) {
            continue;
        }

        TrimLeftExon((left_exon_it->prod_to - cds_start + 1) % 3,
                     exons.rend(), left_exon_it, left_spl_exon_it,
                     product_strand, genomic_strand);
        TrimRightExon((frame_offset-right_exon_it->prod_from) % 3,
                      right_exon_it, exons.end(), right_spl_exon_it,
                      product_strand, genomic_strand);

        if (left_exon_it.base() != right_exon_it) {
            right_exon_it = exons.erase(left_exon_it.base(), right_exon_it);
            right_spl_exon_it = spliced_seg.SetExons().erase(left_spl_exon_it.base(), right_spl_exon_it);
        }
    }
}

TSignedSeqPos CFeatureGenerator::SImplementation::GetCdsStart(const objects::CSeq_id& rna_id)
{
    TSignedSeqPos cds_start = -1;

    CBioseq_Handle handle = m_scope->GetBioseqHandle(rna_id);
    CFeat_CI feat_iter(handle, CSeqFeatData::eSubtype_cdregion);
    if (feat_iter  &&  feat_iter.GetSize()) {
        cds_start = feat_iter->GetTotalRange().GetFrom();
    }

    return cds_start;
}

void CFeatureGenerator::SImplementation::TrimLeftExon(int trim_amount,
                                                      vector<SExon>::reverse_iterator left_edge,
                                                      vector<SExon>::reverse_iterator& exon_it,
                                                      CSpliced_seg::TExons::reverse_iterator& spl_exon_it,
                                                      ENa_strand product_strand,
                                                      ENa_strand genomic_strand)
{
    bool is_protein = (*spl_exon_it)->GetProduct_start().IsProtpos();

    while (trim_amount > 0) {
        int exon_len = exon_it->prod_to - exon_it->prod_from + 1;
        if (exon_len <= trim_amount) {
            ++exon_it;
            ++spl_exon_it;
            trim_amount -= exon_len;
            if (exon_it == left_edge)
                break;
        } else {
            if (is_protein) {
                CProt_pos& prot_pos = (*spl_exon_it)->SetProduct_end().SetProtpos();
                _ASSERT( prot_pos.GetFrame() - trim_amount == 0 );
                --prot_pos.SetAmin();
                prot_pos.SetFrame(3);
            } else {
                if (product_strand != eNa_strand_minus) {
                    (*spl_exon_it)->SetProduct_end().SetNucpos() -= trim_amount;
                } else {
                    (*spl_exon_it)->SetProduct_start().SetNucpos() += trim_amount;
                }
            }

            (*spl_exon_it)->SetPartial(true);
            (*spl_exon_it)->ResetDonor_after_exon();

            int genomic_trim_amount = 0;

            if ((*spl_exon_it)->CanGetParts() && !(*spl_exon_it)->GetParts().empty()) {
                CSpliced_exon::TParts& parts = (*spl_exon_it)->SetParts();
                CSpliced_exon_Base::TParts::iterator chunk = parts.end();
                while (--chunk, (trim_amount>0 || (*chunk)->IsGenomic_ins())) {
                    int product_chunk_len = 0;
                    int genomic_chunk_len = 0;
                    switch((*chunk)->Which()) {
                    case CSpliced_exon_chunk::e_Match:
                        product_chunk_len = (*chunk)->GetMatch();
                        genomic_chunk_len = product_chunk_len;
                        if (product_chunk_len > trim_amount) {
                            (*chunk)->SetMatch(product_chunk_len - trim_amount);
                        }
                        break;
                    case CSpliced_exon_chunk::e_Mismatch:
                        product_chunk_len = (*chunk)->GetMismatch();
                        genomic_chunk_len = product_chunk_len;
                        if (product_chunk_len > trim_amount) {
                            (*chunk)->SetMismatch(product_chunk_len - trim_amount);
                        }
                        break;
                    case CSpliced_exon_chunk::e_Diag:
                        product_chunk_len = (*chunk)->GetDiag();
                        genomic_chunk_len = product_chunk_len;
                        if (product_chunk_len > trim_amount) {
                            (*chunk)->SetDiag(product_chunk_len - trim_amount);
                        }
                        break;
                        
                    case CSpliced_exon_chunk::e_Product_ins:
                        product_chunk_len = (*chunk)->GetProduct_ins();
                        if (product_chunk_len > trim_amount) {
                            (*chunk)->SetProduct_ins(product_chunk_len - trim_amount);
                        }
                        break;
                    case CSpliced_exon_chunk::e_Genomic_ins:
                        genomic_chunk_len = (*chunk)->GetGenomic_ins();
                        break;
                    default:
                        _ASSERT(false);
                        break;
                    }
                    
                    if (product_chunk_len <= trim_amount) {
                        genomic_trim_amount += genomic_chunk_len;
                        trim_amount -= product_chunk_len;
                        chunk = parts.erase(chunk);
                    } else {
                        genomic_trim_amount += min(trim_amount, genomic_chunk_len);
                        trim_amount = 0;
                        break;
                    }
                }
                
            } else {
                genomic_trim_amount += trim_amount;
                trim_amount = 0;
            }
            
            if (genomic_strand != eNa_strand_minus) {
                (*spl_exon_it)->SetGenomic_end() -= genomic_trim_amount;
            } else {
                (*spl_exon_it)->SetGenomic_start() += genomic_trim_amount;
            }
        }
    }
}
void CFeatureGenerator::SImplementation::TrimRightExon(int trim_amount,
                                                       vector<SExon>::iterator& exon_it,
                                                       vector<SExon>::iterator right_edge,
                                                       CSpliced_seg::TExons::iterator& spl_exon_it,
                                                       ENa_strand product_strand,
                                                       ENa_strand genomic_strand)
{
    bool is_protein = (*spl_exon_it)->GetProduct_start().IsProtpos();

    while (trim_amount > 0) {
        int exon_len = exon_it->prod_to - exon_it->prod_from + 1;
        if (exon_len <= trim_amount) {
            ++exon_it;
            ++spl_exon_it;
            trim_amount -= exon_len;
            if (exon_it == right_edge)
                break;
        } else {
            if (is_protein) {
                CProt_pos& prot_pos = (*spl_exon_it)->SetProduct_start().SetProtpos();
                _ASSERT( prot_pos.GetFrame() + trim_amount == 3 );
                ++prot_pos.SetAmin();
                prot_pos.SetFrame(1);
            } else {
                if (product_strand != eNa_strand_minus) {
                    (*spl_exon_it)->SetProduct_start().SetNucpos() += trim_amount;
                } else {
                    (*spl_exon_it)->SetProduct_end().SetNucpos() -= trim_amount;
                }
            }

            (*spl_exon_it)->SetPartial(true);
            (*spl_exon_it)->ResetAcceptor_before_exon();

            int genomic_trim_amount = 0;

            if ((*spl_exon_it)->CanGetParts() && !(*spl_exon_it)->GetParts().empty()) {
                CSpliced_exon::TParts& parts = (*spl_exon_it)->SetParts();
                CSpliced_exon_Base::TParts::iterator chunk = parts.begin();
                for (; trim_amount>0 || (*chunk)->IsGenomic_ins(); chunk = parts.erase(chunk)) {
                    int product_chunk_len = 0;
                    int genomic_chunk_len = 0;
                    switch((*chunk)->Which()) {
                    case CSpliced_exon_chunk::e_Match:
                        product_chunk_len = (*chunk)->GetMatch();
                        genomic_chunk_len = product_chunk_len;
                        if (product_chunk_len > trim_amount) {
                            (*chunk)->SetMatch(product_chunk_len - trim_amount);
                        }
                        break;
                    case CSpliced_exon_chunk::e_Mismatch:
                        product_chunk_len = (*chunk)->GetMismatch();
                        genomic_chunk_len = product_chunk_len;
                        if (product_chunk_len > trim_amount) {
                            (*chunk)->SetMismatch(product_chunk_len - trim_amount);
                        }
                        break;
                    case CSpliced_exon_chunk::e_Diag:
                        product_chunk_len = (*chunk)->GetDiag();
                        genomic_chunk_len = product_chunk_len;
                        if (product_chunk_len > trim_amount) {
                            (*chunk)->SetDiag(product_chunk_len - trim_amount);
                        }
                        break;
                        
                    case CSpliced_exon_chunk::e_Product_ins:
                        product_chunk_len = (*chunk)->GetProduct_ins();
                        if (product_chunk_len > trim_amount) {
                            (*chunk)->SetProduct_ins(product_chunk_len - trim_amount);
                        }
                        break;
                    case CSpliced_exon_chunk::e_Genomic_ins:
                        genomic_chunk_len = (*chunk)->GetGenomic_ins();
                        break;
                    default:
                        _ASSERT(false);
                        break;
                    }
                    
                    if (product_chunk_len <= trim_amount) {
                        genomic_trim_amount += genomic_chunk_len;
                        trim_amount -= product_chunk_len;
                    } else {
                        genomic_trim_amount += min(trim_amount, genomic_chunk_len);
                        trim_amount = 0;
                        break;
                    }
                }
                
            } else {
                genomic_trim_amount += trim_amount;
                trim_amount = 0;
            }
            
            if (genomic_strand != eNa_strand_minus) {
                (*spl_exon_it)->SetGenomic_start() += genomic_trim_amount;
            } else {
                (*spl_exon_it)->SetGenomic_end() -= genomic_trim_amount;
            }
        }
    }
}
END_NCBI_SCOPE
