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
* Author:  Christiam Camacho
*
* File Description:
*   Utility function to convert internal BLAST result structures into
*   CSeq_align_set objects
*
* ===========================================================================
*/

#include "blast_seqalign.hpp"

#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seqalign/Seq_align.hpp>
#include <objects/seqalign/Seq_align_set.hpp>
#include <objects/seqalign/Dense_seg.hpp>
#include <objects/seqalign/Dense_diag.hpp>
#include <objects/seqalign/Std_seg.hpp>
#include <objects/seqalign/Score.hpp>

#include <objects/general/Object_id.hpp>
#include <objmgr/util/sequence.hpp>
#include <serial/iterator.hpp>

#ifndef SMALLEST_EVALUE
#define SMALLEST_EVALUE 1.0e-180
#endif
#ifndef GAP_VALUE
#define GAP_VALUE -1
#endif


BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
BEGIN_SCOPE(blast)

// Converts a frame into the appropriate strand
static ENa_strand
x_Frame2Strand(short frame)
{
    if (frame > 0)
        return eNa_strand_plus;
    else if (frame < 0)
        return eNa_strand_minus;
    else
        return eNa_strand_unknown;
}

static int 
x_GetCurrPos(int& pos, int pos2advance)
{
    int retval;

    if (pos < 0)
        retval = -(pos + pos2advance - 1);
    else
        retval = pos;
    pos += pos2advance;
    return retval;
}

static TSeqPos
x_GetAlignmentStart(int& curr_pos, const GapEditScript* esp, 
        ENa_strand strand, bool translate, int length, int original_length, 
        short frame)
{
    TSeqPos retval;

    if (strand == eNa_strand_minus) {

        if (translate)
            retval = original_length - 
                CODON_LENGTH*(x_GetCurrPos(curr_pos, esp->num) + esp->num)
                + frame + 1;
        else
            retval = length - x_GetCurrPos(curr_pos, esp->num) - esp->num;

    } else {

        if (translate)
            retval = frame - 1 + CODON_LENGTH*x_GetCurrPos(curr_pos, esp->num);
        else
            retval = x_GetCurrPos(curr_pos, esp->num);

    }

    return retval;
}

/// C++ version of GXECollectDataForSeqalign
/// Note that even though the edit_block is passed in, data for seqalign is
/// collected from the esp argument for nsegs segments
static void
x_CollectSeqAlignData(const GapEditBlock* edit_block, 
        const GapEditScript* esp_head, unsigned int nsegs,
        vector<TSignedSeqPos>& starts, vector<TSeqPos>& lengths, 
        vector<ENa_strand>& strands)
{
    ASSERT(edit_block != NULL);

    GapEditScript* esp = const_cast<GapEditScript*>(esp_head);
    ENa_strand m_strand, s_strand;      // strands of alignment
    TSignedSeqPos m_start, s_start;     // running starts of alignment
    int start1 = edit_block->start1;    // start of alignment on master sequence
    int start2 = edit_block->start2;    // start of alignment on slave sequence

    m_strand = x_Frame2Strand(edit_block->frame1);
    s_strand = x_Frame2Strand(edit_block->frame2);

    for (unsigned int i = 0; esp && i < nsegs; esp = esp->next, i++) {
        switch (esp->op_type) {
        case GAPALIGN_DECLINE:
        case GAPALIGN_SUB:
            m_start = x_GetAlignmentStart(start1, esp, m_strand,
                    edit_block->translate1 != 0, edit_block->length1,
                    edit_block->original_length1, edit_block->frame1);

            s_start = x_GetAlignmentStart(start2, esp, s_strand,
                    edit_block->translate2 != 0, edit_block->length2,
                    edit_block->original_length2, edit_block->frame2);

            if (edit_block->reverse) {
                strands.push_back(s_strand);
                strands.push_back(m_strand);
                starts.push_back(s_start);
                starts.push_back(m_start);
            } else {
                strands.push_back(m_strand);
                strands.push_back(s_strand);
                starts.push_back(m_start);
                starts.push_back(s_start);
            }
            break;

        // Insertion on the master sequence (gap on slave)
        case GAPALIGN_INS:
            m_start = x_GetAlignmentStart(start1, esp, m_strand,
                    edit_block->translate1 != 0, edit_block->length1,
                    edit_block->original_length1, edit_block->frame1);

            s_start = GAP_VALUE;

            if (edit_block->reverse) {
                strands.push_back(i == 0 ? eNa_strand_unknown : s_strand);
                strands.push_back(m_strand);
                starts.push_back(s_start);
                starts.push_back(m_start);
            } else {
                strands.push_back(m_strand);
                strands.push_back(i == 0 ? eNa_strand_unknown : s_strand);
                starts.push_back(m_start);
                starts.push_back(s_start);
            }
            break;

        // Deletion on master sequence (gap; insertion on slave)
        case GAPALIGN_DEL:
            m_start = GAP_VALUE;

            s_start = x_GetAlignmentStart(start2, esp, s_strand,
                    edit_block->translate2 != 0, edit_block->length2,
                    edit_block->original_length2, edit_block->frame2);

            if (edit_block->reverse) {
                strands.push_back(s_strand);
                strands.push_back(i == 0 ? eNa_strand_unknown : m_strand);
                starts.push_back(s_start);
                starts.push_back(m_start);
            } else {
                strands.push_back(i == 0 ? eNa_strand_unknown : m_strand);
                strands.push_back(s_strand);
                starts.push_back(m_start);
                starts.push_back(s_start);
            }
            break;

        default:
            break;
        }

        lengths.push_back(esp->num);
    }

    // Make sure the vectors have the right size
    if (lengths.size() != nsegs)
        lengths.resize(nsegs);

    if (starts.size() != nsegs*2)
        starts.resize(nsegs*2);

    if (strands.size() != nsegs*2)
        strands.resize(nsegs*2);
}

static CRef<CDense_seg>
x_CreateDenseg(const CSeq_id* master, const CSeq_id* slave,
        vector<TSignedSeqPos>& starts, vector<TSeqPos>& lengths, 
        vector<ENa_strand>& strands)
{
    ASSERT(master);
    ASSERT(slave);

    CRef<CDense_seg> dense_seg(new CDense_seg());

    // Pairwise alignment is 2 dimensional
    dense_seg->SetDim(2);

    // Set the sequence ids
    CDense_seg::TIds& ids = dense_seg->SetIds();
    CRef<CSeq_id> tmp(new CSeq_id(master->AsFastaString()));
    ids.push_back(tmp);
    tmp.Reset(new CSeq_id(slave->AsFastaString()));
    ids.push_back(tmp);
    ids.resize(dense_seg->GetDim());

    dense_seg->SetLens() = lengths;
    dense_seg->SetStrands() = strands;
    dense_seg->SetStarts() = starts;
    dense_seg->SetNumseg(lengths.size());

    return dense_seg;
}

static CSeq_align::C_Segs::TStd
x_CreateStdSegs(const CSeq_id* master, const CSeq_id* slave, 
        vector<TSignedSeqPos>& starts, vector<TSeqPos>& lengths, 
        vector<ENa_strand>& strands, bool reverse, bool translate_master, bool
        translate_slave)
{
    ASSERT(master);
    ASSERT(slave);

    CSeq_align::C_Segs::TStd retval;
    int nsegs = lengths.size();         // number of segments in alignment
    TSignedSeqPos m_start, m_stop;      // start and stop for master sequence
    TSignedSeqPos s_start, s_stop;      // start and stop for slave sequence

    CRef<CSeq_id> master_id(new CSeq_id(master->AsFastaString()));
    CRef<CSeq_id> slave_id(new CSeq_id(slave->AsFastaString()));
    
    for (int i = 0; i < nsegs; i++) {
        CRef<CStd_seg> std_seg(new CStd_seg());
        CRef<CSeq_loc> master_loc(new CSeq_loc());
        CRef<CSeq_loc> slave_loc(new CSeq_loc());

        // Pairwise alignment is 2 dimensional
        std_seg->SetDim(2);

        // Set master seqloc
        if ( (m_start = starts[2*i]) != GAP_VALUE) {
            master_loc->SetInt().SetId(*master_id);
            master_loc->SetInt().SetFrom(m_start);
            if (translate_master || (reverse && translate_slave))
                m_stop = m_start + CODON_LENGTH*lengths[i] - 1;
            else
                m_stop = m_start + lengths[i] - 1;
            master_loc->SetInt().SetTo(m_stop);
            master_loc->SetInt().SetStrand(strands[2*i]);
        } else {
            master_loc->SetEmpty(*master_id);
        }

        // Set slave seqloc
        if ( (s_start = starts[2*i+1]) != GAP_VALUE) {
            slave_loc->SetInt().SetId(*slave_id);
            slave_loc->SetInt().SetFrom(s_start);
            if (translate_slave || (reverse && translate_master))
                s_stop = s_start + CODON_LENGTH*lengths[i] - 1;
            else
                s_stop = s_start + lengths[i] - 1;
            slave_loc->SetInt().SetTo(s_stop);
            slave_loc->SetInt().SetStrand(strands[2*i+1]);
        } else {
            slave_loc->SetEmpty(*slave_id);
        }

        if (reverse) {
            std_seg->SetIds().push_back(slave_id);
            std_seg->SetIds().push_back(master_id);
            std_seg->SetLoc().push_back(slave_loc);
            std_seg->SetLoc().push_back(master_loc);
        } else {
            std_seg->SetIds().push_back(master_id);
            std_seg->SetIds().push_back(slave_id);
            std_seg->SetLoc().push_back(master_loc);
            std_seg->SetLoc().push_back(slave_loc);
        }

        retval.push_back(std_seg);
    }

    return retval;
}

/// Clone of GXECorrectUASequence (tools/gapxdrop.c)
/// Assumes GAPALIGN_DECLINE regions are to the right of GAPALIGN_{INS,DEL}.
/// This function swaps them (GAPALIGN_DECLINE ends to the right of the gap)
static void 
x_CorrectUASequence(GapEditBlock* edit_block)
{
    GapEditScript* curr = NULL,* curr_last = NULL;
    GapEditScript* indel_prev = NULL; // pointer to node before the last
            // insertion or deletion followed immediately by GAPALIGN_DECLINE
    bool last_indel = false;    // last operation was an insertion or deletion

    for (curr = edit_block->esp; curr; curr = curr->next) {

        // if GAPALIGN_DECLINE immediately follows an insertion or deletion
        if (curr->op_type == GAPALIGN_DECLINE && last_indel) {
            /* This is invalid condition and regions should be
               exchanged */

            if (indel_prev != NULL)
                indel_prev->next = curr;
            else
                edit_block->esp = curr; /* First element in a list */

            // CC: If flaking gaps are allowed, curr_last could be NULL and the
            // following statement would core dump...
            curr_last->next = curr->next;
            curr->next = curr_last;
        }

        last_indel = false;

        if (curr->op_type == GAPALIGN_INS || curr->op_type == GAPALIGN_DEL) {
            last_indel = true;
            indel_prev = curr_last;
        }

        curr_last = curr;
    }
    return;
}

/// C++ version of GXEMakeSeqAlign (tools/gapxdrop.c)
/// Creates a Seq-align for a single HSP
static CRef<CSeq_align>
x_CreateSeqAlign(const CSeq_id* master, const CSeq_id* slave,
    vector<TSignedSeqPos> starts, vector<TSeqPos> lengths,
    vector<ENa_strand> strands, bool translate_master, bool translate_slave,
    bool reverse)
{
    CRef<CSeq_align> sar(new CSeq_align());
    sar->SetType(CSeq_align::eType_partial);
    sar->SetDim(2);         // BLAST only creates pairwise alignments

    if (translate_master || translate_slave) {
        sar->SetSegs().SetStd() =
            x_CreateStdSegs(master, slave, starts, lengths, strands,
                    reverse, translate_master, translate_slave);
    } else {
        CRef<CDense_seg> dense_seg = 
            x_CreateDenseg(master, slave, starts, lengths, strands);
        sar->SetSegs().SetDenseg(*dense_seg);
    }

    return sar;
}

static CRef<CSeq_align>
x_GapEditBlock2SeqAlign(GapEditBlock* edit_block, 
        const CSeq_id* id1, const CSeq_id* id2)
{
    ASSERT(edit_block != NULL);

    vector<TSignedSeqPos> starts;
    vector<TSeqPos> lengths;
    vector<ENa_strand> strands;
    bool is_disc_align = false;
    int nsegs = 0;      // number of segments in edit_block->esp

    for (GapEditScript* t = edit_block->esp; t; t = t->next, nsegs++) {
        if (t->op_type == GAPALIGN_DECLINE)
            is_disc_align = true;
    }

    if (is_disc_align) {

        /* By request of Steven Altschul - we need to have 
           the unaligned part being to the left if it is adjacent to the
           gap (insertion or deletion) - so this function will do
           shuffeling */
        x_CorrectUASequence(edit_block);

        CRef<CSeq_align> seqalign(new CSeq_align());
        seqalign->SetType(CSeq_align::eType_partial);
        seqalign->SetDim(2);         // BLAST only creates pairwise alignments

        bool skip_region;
        GapEditScript* curr = NULL,* curr_head = edit_block->esp;

        while (curr_head) {
            skip_region = false;

            for (nsegs = 0, curr = curr_head; curr; curr = curr->next, nsegs++){
                if (curr->op_type == GAPALIGN_DECLINE) {
                    if (nsegs != 0) { // end of aligned region
                        break;
                    } else {
                        while (curr && curr->op_type == GAPALIGN_DECLINE) {
                            nsegs++;
                            curr = curr->next;
                        }
                        skip_region = true;
                        break;
                    }
                }
            }

            // build seqalign for required regions only
            if (!skip_region) {

                x_CollectSeqAlignData(edit_block, curr_head, nsegs, starts,
                        lengths, strands);

                CRef<CSeq_align> sa_tmp = x_CreateSeqAlign(id1, id2, starts,
                        lengths, strands, edit_block->translate1 !=0,
                        edit_block->translate2 != 0, edit_block->reverse != 0);

                // Add this seqalign to the list
                if (sa_tmp)
                    seqalign->SetSegs().SetDisc().Set().push_back(sa_tmp);
            }
            curr_head = curr;
        }

        return seqalign;

    } else {

        x_CollectSeqAlignData(edit_block, edit_block->esp, nsegs, 
                starts, lengths, strands);

        return x_CreateSeqAlign(id1, id2, starts, lengths, strands,
                edit_block->translate1 != 0, edit_block->translate2 != 0,
                edit_block->reverse != 0);
    }
}

/** Get the current position. */
static Int4 get_current_pos(Int4* pos, Int4 length)
{
    Int4 val;
    if(*pos < 0)
        val = -(*pos + length -1);
    else
        val = *pos;
    *pos += length;
    return val;
}

/** This function is used for Out-Of-Frame traceback conversion
 * Convert an OOF EditScript chain to a SeqAlign of type StdSeg.
 * Used for a non-simple interval (i.e., one without subs. or 
 * deletions).  
 * The first SeqIdPtr in the chain of subject_id and query_id is 
 * duplicated for the SeqAlign.
*/
static CRef<CSeq_align>
x_OOFEditBlock2SeqAlign(EProgram program, 
    GapEditBlock* edit_block, 
    const CSeq_id* query_id, const CSeq_id* subject_id)
{
    ASSERT(edit_block != NULL);

    CRef<CSeq_align> seqalign(new CSeq_align());

    Boolean reverse = FALSE;
    GapEditScript* curr,* esp;
    Int2 frame1, frame2;
    Int4 start1, start2;
    Int4 original_length1, original_length2;
    CRef<CSeq_interval> seq_int1_last;
    CRef<CSeq_interval> seq_int2_last;
    CConstRef<CSeq_id> id1;
    CConstRef<CSeq_id> id2;
    CRef<CSeq_loc> slp1, slp2;
    ENa_strand strand1, strand2;
    bool first_shift;
    Int4 from1, from2, to1, to2;
    CRef<CSeq_id> tmp;

    if (program == eBlastx) {
       reverse = TRUE;
       start1 = edit_block->start2;
       start2 = edit_block->start1;
       frame1 = edit_block->frame2;
       frame2 = edit_block->frame1;
       original_length1 = edit_block->original_length2;
       original_length2 = edit_block->original_length1;
       id1.Reset(subject_id);
       id2.Reset(query_id);
    } else { 
       start1 = edit_block->start1;
       start2 = edit_block->start2;
       frame1 = edit_block->frame1;
       frame2 = edit_block->frame2;
       original_length1 = edit_block->original_length1;
       original_length2 = edit_block->original_length2;
       id1.Reset(query_id);
       id2.Reset(subject_id);
    }
 
    strand1 = x_Frame2Strand(frame1);
    strand2 = x_Frame2Strand(frame2);
    
    esp = edit_block->esp;
    
    seqalign->SetDim(2); /**only two dimention alignment**/
    
    seqalign->SetType(CSeq_align::eType_partial); /**partial for gapped translating search. */

    esp = edit_block->esp;

    first_shift = false;

    for (curr=esp; curr; curr=curr->next) {

        slp1.Reset(new CSeq_loc());
        slp2.Reset(new CSeq_loc());
        
        switch (curr->op_type) {
        case 0: /* deletion of three nucleotides. */
            
            first_shift = false;

            slp1->SetInt().SetFrom(get_current_pos(&start1, curr->num));
            slp1->SetInt().SetTo(MIN(start1,original_length1) - 1);
            tmp.Reset(new CSeq_id(id1->AsFastaString()));
            slp1->SetInt().SetId(*tmp);
            slp1->SetInt().SetStrand(strand1);
            
            /* Empty nucleotide piece */
            tmp.Reset(new CSeq_id(id2->AsFastaString()));
            slp2->SetEmpty(*tmp);

            seq_int1_last.Reset(&slp1->SetInt());
            /* Keep previous seq_int2_last, in case there is a frame shift
               immediately after this gap */
            
            break;

        case 6: /* insertion of three nucleotides. */
            /* If gap is followed after frameshift - we have to
               add this element for the alignment to be correct */
            
            if(first_shift) { /* Second frameshift in a row */
                /* Protein coordinates */
                slp1->SetInt().SetFrom(get_current_pos(&start1, 1));
                to1 = MIN(start1,original_length1) - 1;
                slp1->SetInt().SetTo(to1);
                tmp.Reset(new CSeq_id(id1->AsFastaString()));
                slp1->SetInt().SetId(*tmp);
                slp1->SetInt().SetStrand(strand1);
                
                /* Nucleotide scale shifted by op_type */
                from2 = get_current_pos(&start2, 3);
                to2 = MIN(start2,original_length2) - 1;
                slp2->SetInt().SetFrom(from2);
                slp2->SetInt().SetTo(to2);
                if (start2 > original_length2)
                    slp1->SetInt().SetTo(to1 - 1);
                
                /* Transfer to DNA minus strand coordinates */
                if(strand2 == eNa_strand_minus) {
                    slp2->SetInt().SetTo(original_length2 - from2 - 1);
                    slp2->SetInt().SetFrom(original_length2 - to2 - 1);
                }
                
                tmp.Reset(new CSeq_id(id2->AsFastaString()));
                slp2->SetInt().SetId(*tmp);
                slp2->SetInt().SetStrand(strand2);

                CRef<CStd_seg> seg(new CStd_seg());
                seg->SetDim(2);

                CStd_seg::TIds& ids = seg->SetIds();

                if (reverse) {
                    seg->SetLoc().push_back(slp2);
                    seg->SetLoc().push_back(slp1);
                    tmp.Reset(new CSeq_id(id2->AsFastaString()));
                    ids.push_back(tmp);
                    tmp.Reset(new CSeq_id(id1->AsFastaString()));
                    ids.push_back(tmp);
                } else {
                    seg->SetLoc().push_back(slp1);
                    seg->SetLoc().push_back(slp2);
                    tmp.Reset(new CSeq_id(id1->AsFastaString()));
                    ids.push_back(tmp);
                    tmp.Reset(new CSeq_id(id2->AsFastaString()));
                    ids.push_back(tmp);
                }
                ids.resize(seg->GetDim());
                
                seqalign->SetSegs().SetStd().push_back(seg);
            }

            first_shift = false;

            /* Protein piece is empty */
            tmp.Reset(new CSeq_id(id1->AsFastaString()));
            slp1->SetEmpty(*tmp);
            
            /* Nucleotide scale shifted by 3, protein gapped */
            from2 = get_current_pos(&start2, curr->num*3);
            to2 = MIN(start2,original_length2) - 1;
            slp2->SetInt().SetFrom(from2);
            slp2->SetInt().SetTo(to2);

            /* Transfer to DNA minus strand coordinates */
            if(strand2 == eNa_strand_minus) {
                slp2->SetInt().SetTo(original_length2 - from2 - 1);
                slp2->SetInt().SetFrom(original_length2 - to2 - 1);
            }
            tmp.Reset(new CSeq_id(id2->AsFastaString()));
            slp2->SetInt().SetId(*tmp);
            slp2->SetInt().SetStrand(strand2);
            
            seq_int1_last.Reset(NULL);
            seq_int2_last.Reset(&slp2->SetInt()); /* Will be used to adjust "to" value */
            
            break;

        case 3: /* Substitution. */

            first_shift = false;

            /* Protein coordinates */
            from1 = get_current_pos(&start1, curr->num);
            to1 = MIN(start1, original_length1) - 1;
            /* Adjusting last segment and new start point in
               nucleotide coordinates */
            from2 = get_current_pos(&start2, curr->num*curr->op_type);
            to2 = start2 - 1;
            /* Chop off three bases and one residue at a time.
               Why does this happen, seems like a bug?
            */
            while (to2 >= original_length2) {
                to2 -= 3;
                to1--;
            }
            /* Transfer to DNA minus strand coordinates */
            if(strand2 == eNa_strand_minus) {
                int tmp_int;
                tmp_int = to2;
                to2 = original_length2 - from2 - 1;
                from2 = original_length2 - tmp_int - 1;
            }

            slp1->SetInt().SetFrom(from1);
            slp1->SetInt().SetTo(to1);
            tmp.Reset(new CSeq_id(id1->AsFastaString()));
            slp1->SetInt().SetId(*tmp);
            slp1->SetInt().SetStrand(strand1);
            slp2->SetInt().SetFrom(from2);
            slp2->SetInt().SetTo(to2);
            tmp.Reset(new CSeq_id(id2->AsFastaString()));
            slp2->SetInt().SetId(*tmp);
            slp2->SetInt().SetStrand(strand2);
           

            seq_int1_last.Reset(&slp1->SetInt()); /* Will be used to adjust "to" value */
            seq_int2_last.Reset(&slp2->SetInt()); /* Will be used to adjust "to" value */
            
            break;
        case 1:	/* gap of two nucleotides. */
        case 2: /* Gap of one nucleotide. */
        case 4: /* Insertion of one nucleotide. */
        case 5: /* Insertion of two nucleotides. */

            if(first_shift) { /* Second frameshift in a row */
                /* Protein coordinates */
                from1 = get_current_pos(&start1, 1);
                to1 = MIN(start1,original_length1) - 1;

                /* Nucleotide scale shifted by op_type */
                from2 = get_current_pos(&start2, curr->op_type);
                to2 = start2 - 1;
                if (to2 >= original_length2) {
                    to2 = original_length2 -1;
                    to1--;
                }
                /* Transfer to DNA minus strand coordinates */
                if(strand2 == eNa_strand_minus) {
                    int tmp_int;
                    tmp_int = to2;
                    to2 = original_length2 - from2 - 1;
                    from2 = original_length2 - tmp_int - 1;
                }

                slp1->SetInt().SetFrom(from1);
                slp1->SetInt().SetTo(to1);
                tmp.Reset(new CSeq_id(id1->AsFastaString()));
                slp1->SetInt().SetId(*tmp);
                slp1->SetInt().SetStrand(strand1);
                slp2->SetInt().SetFrom(from2);
                slp2->SetInt().SetTo(to2);
                tmp.Reset(new CSeq_id(id2->AsFastaString()));
                slp2->SetInt().SetId(*tmp);
                slp2->SetInt().SetStrand(strand2);

                seq_int1_last.Reset(&slp1->SetInt()); 
                seq_int2_last.Reset(&slp2->SetInt()); 

                break;
            }
            
            first_shift = true;

            /* If this substitution is following simple frameshift
               we do not need to start new segment, but may continue
               old one */

            if(seq_int2_last) {
                get_current_pos(&start2, curr->num*(curr->op_type-3));
                if(strand2 != eNa_strand_minus) {
                    seq_int2_last->SetTo(start2 - 1);
                } else {
                    /* Transfer to DNA minus strand coordinates */
                    seq_int2_last->SetFrom(original_length2 - start2);
                }

                /* Adjustment for multiple shifts - theoretically possible,
                   but very improbable */
                if(seq_int2_last->GetFrom() > seq_int2_last->GetTo()) {
                    
                    if(strand2 != eNa_strand_minus) {
                        seq_int2_last->SetTo(seq_int2_last->GetTo() + 3);
                    } else {
                        seq_int2_last->SetFrom(seq_int2_last->GetFrom() - 3);
                    }
                    
                    if (seq_int1_last.GetPointer() &&
						seq_int1_last->GetTo() != 0)
                        seq_int1_last->SetTo(seq_int1_last->GetTo() + 1);
                }

            } else if (curr->op_type > 3) {
                /* Protein piece is empty */
                tmp.Reset(new CSeq_id(id1->AsFastaString()));
                slp1->SetEmpty(*tmp);
                /* Simulating insertion of nucleotides */
                from2 = get_current_pos(&start2, 
                                        curr->num*(curr->op_type-3));
                to2 = MIN(start2,original_length2) - 1;

                /* Transfer to DNA minus strand coordinates */
                if(strand2 == eNa_strand_minus) {
                    int tmp_int;
                    tmp_int = to2;
                    to2 = original_length2 - from2 - 1;
                    from2 = original_length2 - tmp_int - 1;
                }
                slp2->SetInt().SetFrom(from2);
                slp2->SetInt().SetTo(to2);
                
                tmp.Reset(new CSeq_id(id2->AsFastaString()));
                slp2->SetInt().SetId(*tmp);

                seq_int1_last.Reset(NULL);
                seq_int2_last.Reset(&slp2->SetInt()); /* Will be used to adjust "to" value */
                break;
            } else {
                continue;       /* Main loop */
            }
            continue;       /* Main loop */
            /* break; */
        default:
            continue;       /* Main loop */
            /* break; */
        } 

        CRef<CStd_seg> seg(new CStd_seg());
        seg->SetDim(2);
        
        CStd_seg::TIds& ids = seg->SetIds();

        if (reverse) {
            seg->SetLoc().push_back(slp2);
            seg->SetLoc().push_back(slp1);
            tmp.Reset(new CSeq_id(id2->AsFastaString()));
            ids.push_back(tmp);
            tmp.Reset(new CSeq_id(id1->AsFastaString()));
            ids.push_back(tmp);
        } else {
            seg->SetLoc().push_back(slp1);
            seg->SetLoc().push_back(slp2);
            tmp.Reset(new CSeq_id(id1->AsFastaString()));
            ids.push_back(tmp);
            tmp.Reset(new CSeq_id(id2->AsFastaString()));
            ids.push_back(tmp);
        }
        ids.resize(seg->GetDim());
        
        seqalign->SetSegs().SetStd().push_back(seg);
    }
    
    return seqalign;
}

/// Creates and initializes CScore with i (if it's non-zero) or d
static CRef<CScore> 
x_MakeScore(const string& ident_string, double d = 0.0, int i = 0)
{
    CRef<CScore> retval(new CScore());

    CRef<CObject_id> id(new CObject_id());
    id->SetStr(ident_string);
    retval->SetId(*id);

    CRef<CScore::C_Value> val(new CScore::C_Value());
    if (i)
        val->SetInt(i);
    else
        val->SetReal(d);
    retval->SetValue(*val);

    return retval;
}

/// C++ version of GetScoreSetFromBlastHsp (tools/blastutl.c)
static void
x_BuildScoreList(const BlastHSP* hsp, const BlastScoreBlk* sbp, const
        BlastScoringOptions* score_options, CSeq_align::TScore& scores,
        EProgram program)
{
    string score_type;
    BLAST_KarlinBlk* kbp = NULL;

    if (!hsp)
        return;

    score_type = "score";
    if (hsp->score)
        scores.push_back(x_MakeScore(score_type, 0.0, hsp->score));

    score_type = "sum_n";
    if (hsp->num > 1)
        scores.push_back(x_MakeScore(score_type, 0.0, hsp->num));

    // Set the E-Value
    double evalue = (hsp->evalue < SMALLEST_EVALUE) ? 0.0 : hsp->evalue;
    score_type = (hsp->num == 1) ? "e_value" : "sum_e";
    if (evalue >= 0.0)
        scores.push_back(x_MakeScore(score_type, evalue));

    // Calculate the bit score from the raw score
    score_type = "bit_score";
    if (program == eBlastn || !score_options->gapped_calculation)
        kbp = sbp->kbp[hsp->context];
    else
        kbp = sbp->kbp_gap[hsp->context];

    double bit_score = ((hsp->score*kbp->Lambda) - kbp->logK)/NCBIMATH_LN2;
    if (bit_score >= 0.0)
        scores.push_back(x_MakeScore(score_type, bit_score));

    // Set the identity score
    score_type = "num_ident";
    if (hsp->num_ident > 0)
        scores.push_back(x_MakeScore(score_type, 0.0, hsp->num_ident));

    if (hsp->num > 1 && hsp->ordering_method == 0/*BLAST_SMALL_GAPS*/) {
        score_type = "small_gap";
        scores.push_back(x_MakeScore(score_type, 0.0, 1));
    } else if (hsp->ordering_method > 3) {
        // In new tblastn this means splice junction was found
        score_type = "splice_junction";
        scores.push_back(x_MakeScore(score_type, 0.0, 1));
    }

    return;
}


static void
x_AddScoresToSeqAlign(CRef<CSeq_align>& seqalign, const BlastHSP* hsp, 
        const BlastScoreBlk* sbp, EProgram program,
        const BlastScoringOptions* score_options)
{
    // Add the scores for this HSP
    CSeq_align::TScore& score_list = seqalign->SetScore();
    x_BuildScoreList(hsp, sbp, score_options, score_list, program);

    // Add scores for special cases: stdseg and disc alignments
#if 0 /* We need to add scores to individual segs only in case
         of ungapped alignment; but this case is handled separately. */
    if (seqalign->GetSegs().IsStd()) {
        CSeq_align::C_Segs::TStd& stdseg_list = seqalign->SetSegs().SetStd();

        NON_CONST_ITERATE(CSeq_align::C_Segs::TStd, itr, stdseg_list) {
            CStd_seg::TScores& scores = (**itr).SetScores();
            x_BuildScoreList(hsp, sbp, score_options, scores, program);
        }
    } else if (seqalign->GetSegs().IsDisc()) {
        CSeq_align_set& seqalign_set = seqalign->SetSegs().SetDisc();

        NON_CONST_ITERATE(list< CRef<CSeq_align> >, itr, seqalign_set.Set()) {
            CSeq_align::TScore& score = (**itr).SetScore();
            x_BuildScoreList(hsp, sbp, score_options, score, program);
        }
    }
#endif
}


CRef<CDense_diag>
x_UngappedHSPToDenseDiag(BlastHSP* hsp, const CSeq_id *query_id, 
    const CSeq_id *subject_id, const BlastScoreBlk* sbp,
    const BlastScoringOptions* score_options, EProgram program, 
    Int4 query_length, Int4 subject_length)
{
    CRef<CDense_diag> retval(new CDense_diag());
    
    // Pairwise alignment is 2 dimensional
    retval->SetDim(2);

    // Set the sequence ids
    CDense_diag::TIds& ids = retval->SetIds();
    CRef<CSeq_id> tmp(new CSeq_id(query_id->AsFastaString()));
    ids.push_back(tmp);
    tmp.Reset(new CSeq_id(subject_id->AsFastaString()));
    ids.push_back(tmp);
    ids.resize(retval->GetDim());

    retval->SetLen(hsp->query.length);

    CDense_diag::TStrands& strands = retval->SetStrands();
    strands.push_back(x_Frame2Strand(hsp->query.frame));
    strands.push_back(x_Frame2Strand(hsp->subject.frame));
    CDense_diag::TStarts& starts = retval->SetStarts();
    if (hsp->query.frame >= 0) {
       starts.push_back(hsp->query.offset);
    } else {
       starts.push_back(query_length - hsp->query.offset - hsp->query.length);
    }
    if (hsp->subject.frame >= 0) {
       starts.push_back(hsp->subject.offset);
    } else {
       starts.push_back(subject_length - hsp->subject.offset - 
                        hsp->subject.length);
    }

    CSeq_align::TScore& score_list = retval->SetScores();
    x_BuildScoreList(hsp, sbp, score_options, score_list, program);

    return retval;
}

CRef<CStd_seg>
x_UngappedHSPToStdSeg(BlastHSP* hsp, const CSeq_id *query_id, 
    const CSeq_id *subject_id, const BlastScoreBlk* sbp,
    const BlastScoringOptions* score_options, EProgram program, 
    Int4 query_length, Int4 subject_length)
{
    CRef<CStd_seg> retval(new CStd_seg());

    // Pairwise alignment is 2 dimensional
    retval->SetDim(2);

    CRef<CSeq_loc> query_loc(new CSeq_loc());
    CRef<CSeq_loc> subject_loc(new CSeq_loc());

    // Set the sequence ids
    CStd_seg::TIds& ids = retval->SetIds();
    CRef<CSeq_id> tmp(new CSeq_id(query_id->AsFastaString()));
    query_loc->SetInt().SetId(*tmp);
    ids.push_back(tmp);
    tmp.Reset(new CSeq_id(subject_id->AsFastaString()));
    subject_loc->SetInt().SetId(*tmp);
    ids.push_back(tmp);
    ids.resize(retval->GetDim());

    query_loc->SetInt().SetStrand(x_Frame2Strand(hsp->query.frame));
    subject_loc->SetInt().SetStrand(x_Frame2Strand(hsp->subject.frame));

    if (hsp->query.frame == 0) {
       query_loc->SetInt().SetFrom(hsp->query.offset);
       query_loc->SetInt().SetTo(hsp->query.offset + hsp->query.length - 1);
    } else if (hsp->query.frame > 0) {
       query_loc->SetInt().SetFrom(CODON_LENGTH*(hsp->query.offset) + 
                                   hsp->query.frame - 1);
       query_loc->SetInt().SetTo(CODON_LENGTH*(hsp->query.offset+
                                               hsp->query.length)
                                 + hsp->query.frame - 2);
    } else {
       query_loc->SetInt().SetFrom(query_length -
           CODON_LENGTH*(hsp->query.offset+hsp->query.length) +
           hsp->query.frame + 1);
       query_loc->SetInt().SetTo(query_length - CODON_LENGTH*hsp->query.offset
                                 + hsp->query.frame);
    }

    if (hsp->subject.frame == 0) {
       subject_loc->SetInt().SetFrom(hsp->subject.offset);
       subject_loc->SetInt().SetTo(hsp->subject.offset + 
                                   hsp->subject.length - 1);
    } else if (hsp->subject.frame > 0) {
       subject_loc->SetInt().SetFrom(CODON_LENGTH*(hsp->subject.offset) + 
                                   hsp->subject.frame - 1);
       subject_loc->SetInt().SetTo(CODON_LENGTH*(hsp->subject.offset+
                                               hsp->subject.length) +
                                   hsp->subject.frame - 2);

    } else {
       subject_loc->SetInt().SetFrom(subject_length -
           CODON_LENGTH*(hsp->subject.offset+hsp->subject.length) +
           hsp->subject.frame + 1);
       subject_loc->SetInt().SetTo(subject_length - 
           CODON_LENGTH*hsp->subject.offset + hsp->subject.frame);
    }

    retval->SetLoc().push_back(query_loc);
    retval->SetLoc().push_back(subject_loc);

    CSeq_align::TScore& score_list = retval->SetScores();
    x_BuildScoreList(hsp, sbp, score_options, score_list, program);

    return retval;
}

static CRef<CSeq_align>
BLASTUngappedHspListToSeqAlign(EProgram program, 
    BlastHSPList* hsp_list, const CSeq_id *query_id, 
    const CSeq_id *subject_id, Int4 query_length, Int4 subject_length,
    const BlastScoringOptions* score_options, const BlastScoreBlk* sbp)
{
    CRef<CSeq_align> retval(new CSeq_align()); 
    BlastHSP** hsp_array;
    int index;

    retval->SetType(CSeq_align::eType_diags);

    hsp_array = hsp_list->hsp_array;

    /* All HSPs are put in one seqalign, containing a list of 
     * DenseDiag for same molecule search, or StdSeg for translated searches.
    */
    if (program == eBlastn || program == eBlastp) {
        for (index=0; index<hsp_list->hspcnt; index++) { 
            BlastHSP* hsp = hsp_array[index];
            retval->SetSegs().SetDendiag().push_back(
                x_UngappedHSPToDenseDiag(hsp, query_id, subject_id, sbp, 
                    score_options, program, query_length, subject_length));
        }
    } else { // Translated search
        for (index=0; index<hsp_list->hspcnt; index++) { 
            BlastHSP* hsp = hsp_array[index];
            retval->SetSegs().SetStd().push_back(
                x_UngappedHSPToStdSeg(hsp, query_id, subject_id, sbp, 
                    score_options, program, query_length, subject_length));
        }
    }

    return retval;
}

// This is called for each query and each subject in the BLAST search
// We always return CSeq_aligns of type disc to allow multiple HSPs
// corresponding to the same query-subject pair to be grouped in one CSeq_align
static CRef<CSeq_align>
BLASTHspListToSeqAlign(EProgram program, 
    BlastHSPList* hsp_list, const CSeq_id *query_id, 
    const CSeq_id *subject_id,
    const BlastScoringOptions* score_options, const BlastScoreBlk* sbp)
{
    CRef<CSeq_align> retval(new CSeq_align()); 
    retval->SetType(CSeq_align::eType_disc);
    retval->SetDim(2);         // BLAST only creates pairwise alignments

    // Process the list of HSPs corresponding to one subject sequence and
    // create one seq-align for each list of HSPs (use disc seqalign when
    // multiple HSPs are found).
    BlastHSP** hsp_array = hsp_list->hsp_array;

    for (int index = 0; index < hsp_list->hspcnt; index++) { 
        BlastHSP* hsp = hsp_array[index];
        CRef<CSeq_align> seqalign;

        if (score_options->is_ooframe) {
            seqalign = 
                x_OOFEditBlock2SeqAlign(program, hsp->gap_info, 
                    query_id, subject_id);
        } else {
            seqalign = 
                x_GapEditBlock2SeqAlign(hsp->gap_info, 
                    query_id, subject_id);
        }
        
        x_AddScoresToSeqAlign(seqalign, hsp, sbp, program, 
                              score_options);
        retval->SetSegs().SetDisc().Set().push_back(seqalign);
    }
    
    return retval;
}

static void
x_GetSequenceLengthAndId(const SSeqLoc* ss,          // [in]
                         const BlastSeqSrc* bssp,    // [in]
                         int oid,                    // [in] 
                         bool is_gapped,             // [in]
                         CSeq_id** seqid,            // [out]
                         TSeqPos* length)            // [out]
{
    ASSERT(ss || bssp);
    ASSERT(seqid);
    ASSERT(length);

    if ( !bssp ) {
        *seqid = const_cast<CSeq_id*>(&sequence::GetId(*ss->seqloc,
                                                       ss->scope));

        if ( !is_gapped ) {
            *length = sequence::GetLength(*ss->seqloc, ss->scope);
        }
    } else {
        char* id = BLASTSeqSrcGetSeqIdStr(bssp, (void*) &oid);
        string id_str(id);
        *seqid = new CSeq_id(id_str);
        sfree(id);

        if ( !is_gapped ) {
            *length = BLASTSeqSrcGetSeqLen(bssp, (void*) &oid);
        }
    }
    return;
}

/// Constructs an empty seq-align-set containing an empty discontinuous
/// seq-align.
static CSeq_align_set*
x_CreateEmptySeq_align_set(CSeq_align_set* sas)
{
    CSeq_align_set* retval = NULL;

    if (!sas) {
        retval = new CSeq_align_set;
    } else {
        retval = sas;
    }

    CRef<CSeq_align> empty_seqalign(new CSeq_align);
    empty_seqalign->SetType(CSeq_align::eType_disc);
    empty_seqalign->SetSegs().SetDisc(*new CSeq_align_set);

    retval->Set().push_back(empty_seqalign);
    return retval;
}

// Always remap the query, the subject is remapped if it's given (assumes
// alignment created by BLAST 2 Sequences API)
static void
x_RemapAlignmentCoordinates(CRef<CSeq_align> sar, 
                            const SSeqLoc* query,
                            const SSeqLoc* subject = NULL)
{
    ASSERT(sar);
    ASSERT(query);
    const int query_dimension = 0;
    const int subject_dimension = 1;

    for (CTypeIterator<CDense_seg> itr(Begin(*sar)); itr; ++itr) {
        itr->RemapToLoc(query_dimension, *query->seqloc);
        if (subject) {
            itr->RemapToLoc(subject_dimension, *subject->seqloc);
        }
    }
}

CSeq_align_set*
BLAST_HitList2CSeqAlign(const BlastHitList* hit_list, 
    EProgram prog, SSeqLoc &query,
    const BlastSeqSrc* bssp, const SSeqLoc *subject,
    const BlastScoringOptions* score_options, 
    const BlastScoreBlk* sbp)
{
    CSeq_align_set* seq_aligns = new CSeq_align_set();
    bool is_gapped = static_cast<bool>(score_options->gapped_calculation);

    if (!hit_list) {
        return x_CreateEmptySeq_align_set(seq_aligns);
    }

    TSeqPos query_length = 0;
    CSeq_id* qid = NULL;
    CConstRef<CSeq_id> query_id;
    x_GetSequenceLengthAndId(&query, NULL, 0, is_gapped, &qid, &query_length);
    query_id.Reset(qid);

    TSeqPos subj_length = 0;
    CSeq_id* sid = NULL;
    CConstRef<CSeq_id> subject_id;
    if ( !bssp ) {
        x_GetSequenceLengthAndId(subject, NULL, 0, is_gapped, &sid, 
                               &subj_length);
        subject_id.Reset(sid);
    }

    for (int index = 0; index < hit_list->hsplist_count; index++) {
        BlastHSPList* hsp_list = hit_list->hsplist_array[index];
        if (!hsp_list)
            continue;

        if (bssp) {
            x_GetSequenceLengthAndId(NULL, bssp, hsp_list->oid, is_gapped, 
                                   &sid, &subj_length);
            subject_id.Reset(sid);
        }
        
        // Create a CSeq_align for each matching sequence
        CRef<CSeq_align> hit_align;
        if (is_gapped) {
            hit_align = 
                BLASTHspListToSeqAlign(prog, hsp_list, query_id, 
                                       subject_id, score_options, sbp);
        } else {
            hit_align = 
                BLASTUngappedHspListToSeqAlign(prog, hsp_list, query_id, 
                    subject_id, query_length, subj_length, score_options, sbp);
        }
        x_RemapAlignmentCoordinates(hit_align, &query, subject);
        seq_aligns->Set().push_back(hit_align);
    }
    return seq_aligns;
}

TSeqAlignVector
BLAST_Results2CSeqAlign(const BlastResults* results, 
        EProgram prog,
        TSeqLocVector &query,
        const BlastSeqSrc* bssp,
        const SSeqLoc *subject,
        const BlastScoringOptions* score_options, 
        const BlastScoreBlk* sbp)
{
    ASSERT(results->num_queries == (int)query.size());
    ASSERT(bssp || subject);

    TSeqAlignVector retval;
    CConstRef<CSeq_id> query_id;

    // Process each query's hit list
    for (int index = 0; index < results->num_queries; index++) {
       BlastHitList* hit_list = results->hitlist_array[index];

       CRef<CSeq_align_set> seq_aligns(BLAST_HitList2CSeqAlign(hit_list, prog,
                                           query[index], bssp, subject,
                                           score_options, sbp));

       retval.push_back(seq_aligns);
       _TRACE("Query " << index << ": " << seq_aligns->Get().size() 
              << " seqaligns");

    }
    
    return retval;
}


END_SCOPE(blast)
END_NCBI_SCOPE

/*
* ===========================================================================
*
* $Log$
* Revision 1.25  2003/11/24 17:14:58  camacho
* Remap alignment coordinates to original Seq-locs
*
* Revision 1.24  2003/11/04 18:37:36  dicuccio
* Fix for brain-dead MSVC (operator && is ambiguous)
*
* Revision 1.23  2003/11/04 17:13:31  dondosha
* Implemented conversion of results to seqalign for out-of-frame search
*
* Revision 1.22  2003/10/31 22:08:39  dondosha
* Implemented conversion of BLAST results to seqalign for ungapped search
*
* Revision 1.21  2003/10/31 00:05:15  camacho
* Changes to return discontinuous seq-aligns for each query-subject pair
*
* Revision 1.20  2003/10/30 21:40:57  dondosha
* Removed unneeded extra argument from BLAST_Results2CSeqAlign
*
* Revision 1.19  2003/10/28 20:53:59  camacho
* Temporarily use exception for unimplemented function
*
* Revision 1.18  2003/10/15 16:59:42  coulouri
* type correctness fixes
*
* Revision 1.17  2003/09/09 15:18:02  camacho
* Fix includes
*
* Revision 1.16  2003/08/19 20:27:51  dondosha
* Rewrote the results-to-seqalign conversion slightly
*
* Revision 1.15  2003/08/19 13:46:13  dicuccio
* Added 'USING_SCOPE(objects)' to .cpp files for ease of reading implementation.
*
* Revision 1.14  2003/08/18 22:17:36  camacho
* Renaming of SSeqLoc members
*
* Revision 1.13  2003/08/18 20:58:57  camacho
* Added blast namespace, removed *__.hpp includes
*
* Revision 1.12  2003/08/18 17:07:41  camacho
* Introduce new SSeqLoc structure (replaces pair<CSeq_loc, CScope>).
* Change in function to read seqlocs from files.
*
* Revision 1.11  2003/08/15 15:56:32  dondosha
* Corrections in implementation of results-to-seqalign function
*
* Revision 1.10  2003/08/12 19:19:34  dondosha
* Use TSeqLocVector type
*
* Revision 1.9  2003/08/11 14:00:41  dicuccio
* Indenting changes.  Fixed use of C++ namespaces (USING_SCOPE(objects) inside of
* BEGIN_NCBI_SCOPE block)
*
* Revision 1.8  2003/08/08 19:43:07  dicuccio
* Compilation fixes: #include file rearrangement; fixed use of 'list' and
* 'vector' as variable names; fixed missing ostrea<< for __int64
*
* Revision 1.7  2003/08/01 17:40:56  dondosha
* Use renamed functions and structures from local blastkar.h
*
* Revision 1.6  2003/07/31 19:45:33  camacho
* Eliminate Ptr notation
*
* Revision 1.5  2003/07/25 22:12:46  camacho
* Use BLAST Sequence Source to retrieve sequence identifier
*
* Revision 1.4  2003/07/25 13:55:58  camacho
* Removed unnecessary #includes
*
* Revision 1.3  2003/07/23 21:28:23  camacho
* Use new local gapinfo structures
*
* Revision 1.2  2003/07/14 21:40:22  camacho
* Pacify compiler warnings
*
* Revision 1.1  2003/07/10 18:34:19  camacho
* Initial revision
*
*
* ===========================================================================
*/
