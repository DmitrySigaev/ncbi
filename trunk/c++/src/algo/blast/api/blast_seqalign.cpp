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
* ===========================================================================
*/

/// @file blast_seqalign.cpp
/// Utility function to convert internal BLAST result structures into
/// CSeq_align_set objects.

#include <ncbi_pch.hpp>
#include "blast_seqalign.hpp"

#include <algo/blast/api/blast_aux.hpp>
#include <algo/blast/api/query_data.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seqalign/seqalign__.hpp>
#include <objects/general/Object_id.hpp>
#include <serial/iterator.hpp>
#include <objmgr/util/seq_align_util.hpp>

#include <algorithm>

/** @addtogroup AlgoBlast
 *
 * @{
 */

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
BEGIN_SCOPE(blast)

#ifndef SMALLEST_EVALUE
/// Threshold below which e-values are saved as 0
#define SMALLEST_EVALUE 1.0e-180
#endif
#ifndef GAP_VALUE
/// Value in the Dense-seg indicating a gap
#define GAP_VALUE -1
#endif

/// Duplicate any serial object.
template<class TObj>
CRef<TObj> s_DuplicateObject(const TObj & id)
{
    CRef<TObj> id2(new TObj);
    SerialAssign(*id2, id);
    return id2;
}

/// Converts a frame into the appropriate strand
static ENa_strand
s_Frame2Strand(short frame)
{
    if (frame > 0)
        return eNa_strand_plus;
    else if (frame < 0)
        return eNa_strand_minus;
    else
        return eNa_strand_unknown;
}

/// Advances position in a sequence, according to an edit script instruction.
/// @param pos Current position on input, next position on output  [in] [out]
/// @param pos2advance How much the position should be advanced? [in]
/// @return Current position.
static int 
s_GetCurrPos(int& pos, int pos2advance)
{
    int retval;

    if (pos < 0) /// @todo FIXME: is this condition possible? 
        retval = -(pos + pos2advance - 1);
    else
        retval = pos;
    pos += pos2advance;
    return retval;
}

/// Finds the starting position of a sequence segment in an alignment, given an 
/// editing script.
/// @param curr_pos Current position on input, modified to next position on 
///                 output [in] [out]
/// @param num number of letters specified by traceback edit [in]
/// @param strand Sequence strand [in]
/// @param translate Is sequence translated? [in]
/// @param length Sequence length [in]
/// @param original_length Original (nucleotide) sequence length, if it is 
///                        translated [in]
/// @param frame Translating frame [in]
/// @return Start position of the current alignment segment.
static TSeqPos
s_GetAlignmentStart(int& curr_pos, int num,
        ENa_strand strand, bool translate, int length, int original_length, 
        short frame)
{
    TSeqPos retval;

    if (strand == eNa_strand_minus) {

        if (translate)
            retval = original_length - 
                CODON_LENGTH*(s_GetCurrPos(curr_pos, num) + num)
                + frame + 1;
        else
            retval = length - s_GetCurrPos(curr_pos, num) - num;

    } else {

        if (translate)
            retval = frame - 1 + CODON_LENGTH*s_GetCurrPos(curr_pos, num);
        else
            retval = s_GetCurrPos(curr_pos, num);

    }

    return retval;
}

/// Finds length of a protein frame given a nucleotide length and a frame 
/// number.
/// @param nuc_length Nucleotide sequence length [in]
/// @param frame Translation frame [in]
/// @return Length of the translated sequence.
static Int4
s_GetProteinFrameLength(Int4 nuc_length, Int2 frame)
{
    return (nuc_length - (ABS(frame)-1)%CODON_LENGTH) / CODON_LENGTH;
}

/// Fills vectors of start positions, lengths and strands for all alignment segments.
/// Note that even though the edit_block is passed in, data for seqalign is
/// collected from the esp_head argument for nsegs segments. This editing script may
/// not be the full editing scripg if a discontinuous alignment is being built.
/// @param hsp HSP structure containing traceback information. [in]
/// @param esp Traceback edit script [in]
/// @param first first element of GapEditScript to use [in]
/// @param nsegs Number of alignment segments [in]
/// @param starts Vector of starting positions to fill [out]
/// @param lengths Vector of segment lengths to fill [out]
/// @param strands Vector of segment strands to fill [out]
/// @param query_length Length of query sequence [in]
/// @param subject_length Length of subject sequence [in]
/// @param translate1 Is query translated? [in]
/// @param translate2 Is subject translated? [in]
static void
s_CollectSeqAlignData(const BlastHSP* hsp, const GapEditScript* esp, 
                      unsigned int first, unsigned int nsegs, vector<TSignedSeqPos>& starts, 
                      vector<TSeqPos>& lengths, vector<ENa_strand>& strands,
                      Int4 query_length, Int4 subject_length,
                      bool translate1, bool translate2)
{
    _ASSERT(hsp != NULL);

    ENa_strand m_strand, s_strand;    // strands of alignment
    TSignedSeqPos m_start, s_start;   // running starts of alignment
    int start1 = hsp->query.offset;   // start of alignment on master sequence
    int start2 = hsp->subject.offset; // start of alignment on slave sequence
    int length1 = query_length;
    int length2 = subject_length;

    if (translate1)
        length1 = s_GetProteinFrameLength(length1, hsp->query.frame);
    if (translate2)
        length2 = s_GetProteinFrameLength(length2, hsp->subject.frame);

    m_strand = s_Frame2Strand(hsp->query.frame);
    s_strand = s_Frame2Strand(hsp->subject.frame);

    for (unsigned int esp_index = first; esp_index< (unsigned int)esp->size && esp_index < (unsigned int)(first+nsegs); esp_index++) {
        switch (esp->op_type[esp_index]) {
        case eGapAlignDecline:
        case eGapAlignSub:
            m_start = 
                s_GetAlignmentStart(start1, esp->num[esp_index], m_strand, translate1, length1,
                                    query_length, hsp->query.frame);

            s_start = 
                s_GetAlignmentStart(start2, esp->num[esp_index], s_strand, translate2, length2,
                                    subject_length, hsp->subject.frame);

            strands.push_back(m_strand);
            strands.push_back(s_strand);
            starts.push_back(m_start);
            starts.push_back(s_start);
            break;

        // Insertion on the master sequence (gap on slave)
        case eGapAlignIns:
            m_start = 
                s_GetAlignmentStart(start1, esp->num[esp_index], m_strand, translate1, length1,
                                    query_length, hsp->query.frame);

            s_start = GAP_VALUE;

            strands.push_back(m_strand);
            strands.push_back(esp_index == 0 ? eNa_strand_unknown : s_strand);
            starts.push_back(m_start);
            starts.push_back(s_start);
            break;

        // Deletion on master sequence (gap; insertion on slave)
        case eGapAlignDel:
            m_start = GAP_VALUE;

            s_start = 
                s_GetAlignmentStart(start2, esp->num[esp_index], s_strand, translate2, length2,
                                    subject_length, hsp->subject.frame);

            strands.push_back(esp_index == 0 ? eNa_strand_unknown : m_strand);
            strands.push_back(s_strand);
            starts.push_back(m_start);
            starts.push_back(s_start);
            break;

        default:
            break;
        }

        lengths.push_back(esp->num[esp_index]);
    }

    // Make sure the vectors have the right size
    if (lengths.size() != nsegs)
        lengths.resize(nsegs);

    if (starts.size() != nsegs*2)
        starts.resize(nsegs*2);

    if (strands.size() != nsegs*2)
        strands.resize(nsegs*2);
}

/// Creates a Dense-seg object from the starts, lengths and strands vectors and two 
/// Seq-ids.
/// @param master Query Seq-id [in]
/// @param slave Subject Seq-ids [in]
/// @param starts Vector of start positions for alignment segments [in]
/// @param lengths Vector of alignment segments lengths [in]
/// @param strands Vector of alignment segments strands [in]
/// @return The Dense-seg object.
static CRef<CDense_seg>
s_CreateDenseg(const CSeq_id* master, const CSeq_id* slave,
               vector<TSignedSeqPos>& starts, vector<TSeqPos>& lengths, 
               vector<ENa_strand>& strands)
{
    _ASSERT(master);
    _ASSERT(slave);

    CRef<CDense_seg> dense_seg(new CDense_seg());

    // Pairwise alignment is 2 dimensional
    dense_seg->SetDim(2);

    // Set the sequence ids
    CDense_seg::TIds & ids = dense_seg->SetIds();
    ids.push_back(s_DuplicateObject(*master));
    ids.push_back(s_DuplicateObject(*slave));
    ids.resize(dense_seg->GetDim());
    
    dense_seg->SetLens() = lengths;
    dense_seg->SetStrands() = strands;
    dense_seg->SetStarts() = starts;
    dense_seg->SetNumseg((int) lengths.size());

    return dense_seg;
}

/// Creates a Std-seg object from the starts, lengths and strands vectors and two 
/// Seq-ids for a translated search.
/// @param master Query Seq-id [in]
/// @param slave Subject Seq-ids [in]
/// @param starts Vector of start positions for alignment segments [in]
/// @param lengths Vector of alignment segments lengths [in]
/// @param strands Vector of alignment segments strands [in]
/// @param translate_master Is query sequence translated? [in]
/// @param translate_slave Is subject sequenec translated? [in]
/// @return The Std-seg object.
static CSeq_align::C_Segs::TStd
s_CreateStdSegs(const CSeq_id* master, const CSeq_id* slave, 
                vector<TSignedSeqPos>& starts, vector<TSeqPos>& lengths, 
                vector<ENa_strand>& strands, bool translate_master, 
                bool translate_slave)
{
    _ASSERT(master);
    _ASSERT(slave);

    CSeq_align::C_Segs::TStd retval;
    int nsegs = (int) lengths.size();   // number of segments in alignment
    TSignedSeqPos m_start, m_stop;      // start and stop for master sequence
    TSignedSeqPos s_start, s_stop;      // start and stop for slave sequence
    
    CRef<CSeq_id> master_id = s_DuplicateObject(*master);
    CRef<CSeq_id> slave_id = s_DuplicateObject(*slave);
    
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
            if (translate_master)
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
            if (translate_slave)
                s_stop = s_start + CODON_LENGTH*lengths[i] - 1;
            else
                s_stop = s_start + lengths[i] - 1;
            slave_loc->SetInt().SetTo(s_stop);
            slave_loc->SetInt().SetStrand(strands[2*i+1]);
        } else {
            slave_loc->SetEmpty(*slave_id);
        }

        std_seg->SetIds().push_back(master_id);
        std_seg->SetIds().push_back(slave_id);
        std_seg->SetLoc().push_back(master_loc);
        std_seg->SetLoc().push_back(slave_loc);

        retval.push_back(std_seg);
    }

    return retval;
}

/// Checks if any decline-to-align segments immediately follow an insertion or 
/// deletion, and swaps any such segments so indels are always to the right of 
/// the decline-to-align segments.
/// @param hsp HSP structure, containint traceback [in] [out]
static void 
s_CorrectUASequence(BlastHSP* hsp)
{
    GapEditScript* esp = hsp->gap_info;
    for (int index=0; index<esp->size; index++)
    {
        // if GAPALIGN_DECLINE immediately follows an insertion or deletion
        if (index > 0 && esp->op_type[index] == eGapAlignDecline && 
           (esp->op_type[index-1] == eGapAlignIns || esp->op_type[index-1] == eGapAlignDel)) 
        {
            /* This is invalid condition and regions should be
               exchanged */
            int temp_num = esp->num[index];
            EGapAlignOpType temp_op = esp->op_type[index];

            esp->num[index] = esp->num[index-1];
            esp->op_type[index] = esp->op_type[index-1];
            esp->num[index-1] = temp_num;
            esp->op_type[index-1] = temp_op;
        }
    }
    return;
}

/// Creates a Seq-align for a single HSP from precalculated vectors of start 
/// positions, lengths and strands of segments, sequence identifiers and other 
/// information.
/// @param master Query sequence identifier [in]
/// @param slave Subject sequence identifier [in]
/// @param starts Start positions of alignment segments [in]
/// @param lengths Lengths of alignment segments [in]
/// @param strands Strands of alignment segments [in]
/// @param translate_master Is query translated? [in]
/// @param translate_slave Is subject translated? [in]
/// @return Resulting Seq-align object.
static CRef<CSeq_align>
s_CreateSeqAlign(const CSeq_id* master, const CSeq_id* slave,
    vector<TSignedSeqPos> starts, vector<TSeqPos> lengths,
    vector<ENa_strand> strands, bool translate_master, bool translate_slave)
{
    CRef<CSeq_align> sar(new CSeq_align());
    sar->SetType(CSeq_align::eType_partial);
    sar->SetDim(2);         // BLAST only creates pairwise alignments

    if (translate_master || translate_slave) {
        sar->SetSegs().SetStd() =
            s_CreateStdSegs(master, slave, starts, lengths, strands,
                            translate_master, translate_slave);
    } else {
        CRef<CDense_seg> dense_seg = 
            s_CreateDenseg(master, slave, starts, lengths, strands);
        sar->SetSegs().SetDenseg(*dense_seg);
    }

    return sar;
}

/// Converts a traceback editing block to a Seq-align, provided the 2 sequence 
/// identifiers.
/// @param program Type of BLAST program [in]
/// @param hsp Internal HSP structure [in]
/// @param id1 Query sequence identifier [in]
/// @param id2 Subject sequence identifier [in]
/// @param query_length Length of query sequence [in]
/// @param subject_length Length of subject sequence [in]
/// @return Resulting Seq-align object.
static CRef<CSeq_align>
s_BlastHSP2SeqAlign(EBlastProgramType program, BlastHSP* hsp, 
                    const CSeq_id* id1, const CSeq_id* id2,
                    Int4 query_length, Int4 subject_length)
{
    _ASSERT(hsp != NULL);

    vector<TSignedSeqPos> starts;
    vector<TSeqPos> lengths;
    vector<ENa_strand> strands;
    bool translate1, translate2;
    bool is_disc_align = false;

    GapEditScript* t = hsp->gap_info;
    for (int i=0; i<t->size; i++) {
        if (t->op_type[i] == eGapAlignDecline)
        {
            is_disc_align = true;
            break;
        }
    }

    translate1 = (program == eBlastTypeBlastx || program == eBlastTypeTblastx ||
                  program == eBlastTypeRpsTblastn);
    translate2 = (program == eBlastTypeTblastn || program == eBlastTypeTblastx);

    if (is_disc_align) {

        /* By request of Steven Altschul - we need to have 
           the unaligned part being to the left if it is adjacent to the
           gap (insertion or deletion) - so this function will do
           shuffeling */
        s_CorrectUASequence(hsp);

        CRef<CSeq_align> seqalign(new CSeq_align());
        seqalign->SetType(CSeq_align::eType_partial);
        seqalign->SetDim(2);         // BLAST only creates pairwise alignments

        bool skip_region;
        GapEditScript* esp=hsp->gap_info;
        int nsegs = 0;

        for (int index=0; index< esp->size; index++)
        {
            skip_region = false;
            int index2 = index;
            int first = index;
            for (index2=first; index2<esp->size; index2++, nsegs++){
                if (esp->op_type[index2] == eGapAlignDecline) {
                    if (nsegs != 0) { // end of aligned region
                        break;
                    } else {
                        while (index2<esp->size && esp->op_type[index2] == eGapAlignDecline) {
                            nsegs++;
                            index2++;
                        }
                        skip_region = true;
                        break;
                    }
                }
            }

            // build seqalign for required regions only
            if (!skip_region) {

                s_CollectSeqAlignData(hsp, esp, 0, nsegs, starts, lengths, 
                                      strands, query_length, subject_length,
                                      translate1, translate2);

                CRef<CSeq_align> sa_tmp = 
                    s_CreateSeqAlign(id1, id2, starts, lengths, strands, 
                                     translate1, translate2);

                // Add this seqalign to the list
                if (sa_tmp)
                    seqalign->SetSegs().SetDisc().Set().push_back(sa_tmp);
            }
        }

        return seqalign;

    } else {

        s_CollectSeqAlignData(hsp, hsp->gap_info, 0, hsp->gap_info->size, starts, lengths, 
                              strands, query_length, subject_length,
                              translate1, translate2);

        return s_CreateSeqAlign(id1, id2, starts, lengths, strands,
                                translate1, translate2);
    }
}

/// This function is used for out-of-frame traceback conversion
/// Converts an OOF editing script chain to a Seq-align of type Std-seg.
/// @param program BLAST program: blastx or tblastn.
/// @param hsp HSP structure containing traceback produced by an out-of-frame 
///                   gapped extension [in]
/// @param query_id Query sequence identifier [in]
/// @param subject_id Subject sequence identifier [in]
/// @param query_length Length of query sequence [in]
/// @param subject_length Length of subject sequence [in]
static CRef<CSeq_align>
s_OOFBlastHSP2SeqAlign(EBlastProgramType program, BlastHSP* hsp,
                       const CSeq_id* query_id, const CSeq_id* subject_id,
                       Int4 query_length, Int4 subject_length)
{
    _ASSERT(hsp != NULL);

    CRef<CSeq_align> seqalign(new CSeq_align());

    Boolean reverse = FALSE;
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

    if (program == eBlastTypeBlastx) {
       reverse = TRUE;
       start1 = hsp->subject.offset;
       start2 = hsp->query.offset;
       frame1 = hsp->subject.frame;
       frame2 = hsp->query.frame;
       original_length1 = subject_length;
       original_length2 = query_length;
       id1.Reset(subject_id);
       id2.Reset(query_id);
    } else { 
       start1 = hsp->query.offset;
       start2 = hsp->subject.offset;
       frame1 = hsp->query.frame;
       frame2 = hsp->subject.frame;
       original_length1 = query_length;
       original_length2 = subject_length;
       id1.Reset(query_id);
       id2.Reset(subject_id);
    }
 
    strand1 = s_Frame2Strand(frame1);
    strand2 = s_Frame2Strand(frame2);
    
    seqalign->SetDim(2); /**only two dimensional alignment**/
    
    seqalign->SetType(CSeq_align::eType_partial); /**partial for gapped translating search. */

    first_shift = false;

    GapEditScript* esp = hsp->gap_info;

    for (int index=0; index<esp->size; index++)
    {
        slp1.Reset(new CSeq_loc());
        slp2.Reset(new CSeq_loc());
        
        switch (esp->op_type[index]) {
        case eGapAlignDel: /* deletion of three nucleotides. */
            
            first_shift = false;

            slp1->SetInt().SetFrom(s_GetCurrPos(start1, esp->num[index]));
            slp1->SetInt().SetTo(MIN(start1,original_length1) - 1);
            tmp = s_DuplicateObject(*id1);
            slp1->SetInt().SetId(*tmp);
            slp1->SetInt().SetStrand(strand1);
            
            /* Empty nucleotide piece */
            tmp = s_DuplicateObject(*id2);
            slp2->SetEmpty(*tmp);
            
            seq_int1_last.Reset(&slp1->SetInt());
            /* Keep previous seq_int2_last, in case there is a frame shift
               immediately after this gap */
            
            break;

        case eGapAlignIns: /* insertion of three nucleotides. */
            /* If gap is followed after frameshift - we have to
               add this element for the alignment to be correct */
            
            if(first_shift) { /* Second frameshift in a row */
                /* Protein coordinates */
                slp1->SetInt().SetFrom(s_GetCurrPos(start1, 1));
                to1 = MIN(start1,original_length1) - 1;
                slp1->SetInt().SetTo(to1);
                tmp = s_DuplicateObject(*id1);
                slp1->SetInt().SetId(*tmp);
                slp1->SetInt().SetStrand(strand1);
                
                /* Nucleotide scale shifted by op_type */
                from2 = s_GetCurrPos(start2, 3);
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
                
                tmp = s_DuplicateObject(*id2);
                slp2->SetInt().SetId(*tmp);
                slp2->SetInt().SetStrand(strand2);

                CRef<CStd_seg> seg(new CStd_seg());
                seg->SetDim(2);

                CStd_seg::TIds& ids = seg->SetIds();

                if (reverse) {
                    seg->SetLoc().push_back(slp2);
                    seg->SetLoc().push_back(slp1);
                    tmp = s_DuplicateObject(*id2);
                    ids.push_back(tmp);
                    tmp = s_DuplicateObject(*id1);
                    ids.push_back(tmp);
                } else {
                    seg->SetLoc().push_back(slp1);
                    seg->SetLoc().push_back(slp2);
                    tmp = s_DuplicateObject(*id1);
                    ids.push_back(tmp);
                    tmp = s_DuplicateObject(*id2);
                    ids.push_back(tmp);
                }
                ids.resize(seg->GetDim());
                
                seqalign->SetSegs().SetStd().push_back(seg);
            }

            first_shift = false;

            /* Protein piece is empty */
            tmp = s_DuplicateObject(*id1);
            slp1->SetEmpty(*tmp);
            
            /* Nucleotide scale shifted by 3, protein gapped */
            from2 = s_GetCurrPos(start2, esp->num[index]*3);
            to2 = MIN(start2,original_length2) - 1;
            slp2->SetInt().SetFrom(from2);
            slp2->SetInt().SetTo(to2);

            /* Transfer to DNA minus strand coordinates */
            if(strand2 == eNa_strand_minus) {
                slp2->SetInt().SetTo(original_length2 - from2 - 1);
                slp2->SetInt().SetFrom(original_length2 - to2 - 1);
            }
            tmp = s_DuplicateObject(*id2);
            slp2->SetInt().SetId(*tmp);
            slp2->SetInt().SetStrand(strand2);
            
            seq_int1_last.Reset(NULL);
            seq_int2_last.Reset(&slp2->SetInt()); /* Will be used to adjust "to" value */
            
            break;

        case eGapAlignSub: /* Substitution. */

            first_shift = false;

            /* Protein coordinates */
            from1 = s_GetCurrPos(start1, esp->num[index]);
            to1 = MIN(start1, original_length1) - 1;
            /* Adjusting last segment and new start point in
               nucleotide coordinates */
            from2 = s_GetCurrPos(start2, esp->num[index]*((Uint1)esp->op_type[index]));
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
            tmp = s_DuplicateObject(*id1);
            slp1->SetInt().SetId(*tmp);
            slp1->SetInt().SetStrand(strand1);
            slp2->SetInt().SetFrom(from2);
            slp2->SetInt().SetTo(to2);
            tmp = s_DuplicateObject(*id2);
            slp2->SetInt().SetId(*tmp);
            slp2->SetInt().SetStrand(strand2);
           

            seq_int1_last.Reset(&slp1->SetInt()); /* Will be used to adjust "to" value */
            seq_int2_last.Reset(&slp2->SetInt()); /* Will be used to adjust "to" value */
            
            break;
        case eGapAlignDel2:	/* gap of two nucleotides. */
        case eGapAlignDel1: /* Gap of one nucleotide. */
        case eGapAlignIns1: /* Insertion of one nucleotide. */
        case eGapAlignIns2: /* Insertion of two nucleotides. */

            if(first_shift) { /* Second frameshift in a row */
                /* Protein coordinates */
                from1 = s_GetCurrPos(start1, 1);
                to1 = MIN(start1,original_length1) - 1;

                /* Nucleotide scale shifted by op_type */
                from2 = s_GetCurrPos(start2, (Uint1)esp->op_type[index]);
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
                tmp = s_DuplicateObject(*id1);
                slp1->SetInt().SetId(*tmp);
                slp1->SetInt().SetStrand(strand1);
                slp2->SetInt().SetFrom(from2);
                slp2->SetInt().SetTo(to2);
                tmp = s_DuplicateObject(*id2);
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
                s_GetCurrPos(start2, esp->num[index]*((Uint1)esp->op_type[index]-3));
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

            } else if ((Uint1)esp->op_type[index] > 3) {
                /* Protein piece is empty */
                tmp = s_DuplicateObject(*id1);
                slp1->SetEmpty(*tmp);
                /* Simulating insertion of nucleotides */
                from2 = s_GetCurrPos(start2, 
                                     esp->num[index]*((Uint1)esp->op_type[index]-3));
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
                
                tmp = s_DuplicateObject(*id2);
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
            tmp = s_DuplicateObject(*id2);
            ids.push_back(tmp);
            tmp = s_DuplicateObject(*id1);
            ids.push_back(tmp);
        } else {
            seg->SetLoc().push_back(slp1);
            seg->SetLoc().push_back(slp2);
            tmp = s_DuplicateObject(*id1);
            ids.push_back(tmp);
            tmp = s_DuplicateObject(*id2);
            ids.push_back(tmp);
        }
        ids.resize(seg->GetDim());
        
        seqalign->SetSegs().SetStd().push_back(seg);
    }
    
    return seqalign;
}

/// Creates and initializes CScore with a given name, and with integer or 
/// double value. Integer value is used if it is not zero, otherwise 
/// double value is assigned.
/// @param ident_string Score type name [in]
/// @param d Real value of the score [in]
/// @param i Integer value of the score. [in]
/// @return Resulting CScore object.
static CRef<CScore> 
s_MakeScore(const string& ident_string, double d = 0.0, int i = 0)
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

/// Creates a list of score objects for a Seq-align, given an HSP structure.
/// @param hsp Structure containing HSP information [in]
/// @param scores Linked list of score objects to put into a Seq-align [out]
/// @param gi_list List of GIs for the subject sequence.
static void
s_BuildScoreList(const BlastHSP     * hsp,
                 CSeq_align::TScore & scores,
                 const vector<int>  & gi_list)
{
    string score_type;

    if (!hsp)
        return;

    score_type = "score";
    if (hsp->score)
        scores.push_back(s_MakeScore(score_type, 0.0, hsp->score));

    score_type = "sum_n";
    if (hsp->num > 1)
        scores.push_back(s_MakeScore(score_type, 0.0, hsp->num));

    // Set the E-Value
    double evalue = (hsp->evalue < SMALLEST_EVALUE) ? 0.0 : hsp->evalue;
    score_type = (hsp->num <= 1) ? "e_value" : "sum_e";
    if (evalue >= 0.0)
        scores.push_back(s_MakeScore(score_type, evalue));

    // Calculate the bit score from the raw score
    score_type = "bit_score";

    if (hsp->bit_score >= 0.0)
        scores.push_back(s_MakeScore(score_type, hsp->bit_score));

    // Set the identity score
    score_type = "num_ident";
    if (hsp->num_ident > 0)
        scores.push_back(s_MakeScore(score_type, 0.0, hsp->num_ident));

    score_type = "comp_adjustment_method";
    if (hsp->comp_adjustment_method > 0) {
        scores.push_back(s_MakeScore(score_type, 0.0,
                                     hsp->comp_adjustment_method));
    }
    
    score_type = "use_this_gi";
    
    ITERATE(vector<int>, gi, gi_list) {
        scores.push_back(s_MakeScore(score_type, 0.0, *gi));
    }
    
    return;
}


/// Given an HSP structure, creates a list of scores and inserts them into 
/// a Seq-align.
/// @param seqalign Seq-align object to fill [in] [out]
/// @param hsp An HSP structure [in]
/// @param gi_list List of GIs for the subject sequence.
static void
s_AddScoresToSeqAlign(CRef<CSeq_align>  & seqalign,
                      const BlastHSP    * hsp,
                      const vector<int> & gi_list)
{
    // Add the scores for this HSP
    CSeq_align::TScore& score_list = seqalign->SetScore();
    s_BuildScoreList(hsp, score_list, gi_list);
}


/// Creates a Dense-diag object from HSP information and sequence identifiers
/// for a non-translated ungapped search.
/// @param hsp An HSP structure [in]
/// @param query_id Query sequence identifier [in]
/// @param subject_id Subject sequence identifier [in]
/// @param query_length Length of the query [in]
/// @param subject_length Length of the subject [in]
/// @param gi_list List of GIs for the subject sequence.
/// @return Resulting Dense-diag object.
CRef<CDense_diag>
x_UngappedHSPToDenseDiag(BlastHSP* hsp, const CSeq_id *query_id, 
                         const CSeq_id *subject_id,
                         Int4 query_length, Int4 subject_length,
                         const vector<int> & gi_list)
{
    CRef<CDense_diag> retval(new CDense_diag());
    
    // Pairwise alignment is 2 dimensional
    retval->SetDim(2);

    // Set the sequence ids
    CDense_diag::TIds& ids = retval->SetIds();
    ids.push_back(s_DuplicateObject(*query_id));
    ids.push_back(s_DuplicateObject(*subject_id));
    ids.resize(retval->GetDim());
    
    retval->SetLen(hsp->query.end - hsp->query.offset);
    
    CDense_diag::TStrands& strands = retval->SetStrands();
    strands.push_back(s_Frame2Strand(hsp->query.frame));
    strands.push_back(s_Frame2Strand(hsp->subject.frame));
    CDense_diag::TStarts& starts = retval->SetStarts();
    if (hsp->query.frame >= 0) {
       starts.push_back(hsp->query.offset);
    } else {
       starts.push_back(query_length - hsp->query.end);
    }
    if (hsp->subject.frame >= 0) {
       starts.push_back(hsp->subject.offset);
    } else {
       starts.push_back(subject_length - hsp->subject.end);
    }

    CSeq_align::TScore& score_list = retval->SetScores();
    s_BuildScoreList(hsp, score_list, gi_list);

    return retval;
}

/// Creates a Std-seg object from HSP information and sequence identifiers
/// for a translated ungapped search.
/// @param hsp An HSP structure [in]
/// @param query_id Query sequence identifier [in]
/// @param subject_id Subject sequence identifier [in]
/// @param query_length Length of the query [in]
/// @param subject_length Length of the subject [in]
/// @param gi_list List of GIs for the subject sequence.
/// @return Resulting Std-seg object.
CRef<CStd_seg>
x_UngappedHSPToStdSeg(BlastHSP* hsp, const CSeq_id *query_id, 
                      const CSeq_id *subject_id,
                      Int4 query_length, Int4 subject_length,
                      const vector<int> & gi_list)
{
    CRef<CStd_seg> retval(new CStd_seg());

    // Pairwise alignment is 2 dimensional
    retval->SetDim(2);

    CRef<CSeq_loc> query_loc(new CSeq_loc());
    CRef<CSeq_loc> subject_loc(new CSeq_loc());

    // Set the sequence ids
    CStd_seg::TIds& ids = retval->SetIds();
    CRef<CSeq_id> tmp = s_DuplicateObject(*query_id);
    query_loc->SetInt().SetId(*tmp);
    ids.push_back(tmp);
    tmp = s_DuplicateObject(*subject_id);
    subject_loc->SetInt().SetId(*tmp);
    ids.push_back(tmp);
    ids.resize(retval->GetDim());

    query_loc->SetInt().SetStrand(s_Frame2Strand(hsp->query.frame));
    subject_loc->SetInt().SetStrand(s_Frame2Strand(hsp->subject.frame));

    if (hsp->query.frame == 0) {
       query_loc->SetInt().SetFrom(hsp->query.offset);
       query_loc->SetInt().SetTo(hsp->query.end - 1);
    } else if (hsp->query.frame > 0) {
       query_loc->SetInt().SetFrom(CODON_LENGTH*(hsp->query.offset) + 
                                   hsp->query.frame - 1);
       query_loc->SetInt().SetTo(CODON_LENGTH*(hsp->query.end) +
                                 hsp->query.frame - 2);
    } else {
       query_loc->SetInt().SetFrom(query_length -
           CODON_LENGTH*(hsp->query.end) + hsp->query.frame + 1);
       query_loc->SetInt().SetTo(query_length - CODON_LENGTH*hsp->query.offset
                                 + hsp->query.frame);
    }

    if (hsp->subject.frame == 0) {
       subject_loc->SetInt().SetFrom(hsp->subject.offset);
       subject_loc->SetInt().SetTo(hsp->subject.end - 1);
    } else if (hsp->subject.frame > 0) {
       subject_loc->SetInt().SetFrom(CODON_LENGTH*(hsp->subject.offset) + 
                                   hsp->subject.frame - 1);
       subject_loc->SetInt().SetTo(CODON_LENGTH*(hsp->subject.end) +
                                   hsp->subject.frame - 2);

    } else {
       subject_loc->SetInt().SetFrom(subject_length -
           CODON_LENGTH*(hsp->subject.end) + hsp->subject.frame + 1);
       subject_loc->SetInt().SetTo(subject_length - 
           CODON_LENGTH*hsp->subject.offset + hsp->subject.frame);
    }

    retval->SetLoc().push_back(query_loc);
    retval->SetLoc().push_back(subject_loc);

    CSeq_align::TScore& score_list = retval->SetScores();
    s_BuildScoreList(hsp, score_list, gi_list);

    return retval;
}

/// Creates a Seq-align from an HSP list for an ungapped search. 
/// @param program BLAST program [in]
/// @param hsp_list HSP list structure [in]
/// @param query_id Query sequence identifier [in]
/// @param subject_id Subject sequence identifier [in]
/// @param query_length Length of the query [in]
/// @param subject_length Length of the subject [in]
/// @param gi_list List of GIs for the subject sequence.
void
BLASTUngappedHspListToSeqAlign(EBlastProgramType program, 
                               BlastHSPList* hsp_list,
                               const CSeq_id *query_id, 
                               const CSeq_id *subject_id,
                               Int4 query_length,
                               Int4 subject_length,
                               const vector<int> & gi_list,
                               vector<CRef<CSeq_align > > & sa_vector)
{
    CRef<CSeq_align> seqalign(new CSeq_align()); 
    BlastHSP** hsp_array;
    int index;

    seqalign->SetType(CSeq_align::eType_diags);

    sa_vector.clear();

    hsp_array = hsp_list->hsp_array;

    /* All HSPs are put in one seqalign, containing a list of 
     * DenseDiag for same molecule search, or StdSeg for translated searches.
     */
    if (program == eBlastTypeBlastn || 
        program == eBlastTypeBlastp ||
        program == eBlastTypeRpsBlast) {
        for (index=0; index<hsp_list->hspcnt; index++) { 
            BlastHSP* hsp = hsp_array[index];
            seqalign->SetSegs().SetDendiag().push_back(
                x_UngappedHSPToDenseDiag(hsp,
                                         query_id,
                                         subject_id, 
					 query_length,
                                         subject_length,
                                         gi_list));
        }
    } else { // Translated search
        for (index=0; index<hsp_list->hspcnt; index++) { 
            BlastHSP* hsp = hsp_array[index];
            seqalign->SetSegs().SetStd().push_back(
                x_UngappedHSPToStdSeg(hsp,
                                      query_id,
                                      subject_id, 
				      query_length,
                                      subject_length,
                                      gi_list));
        }
    }
    sa_vector.push_back(seqalign);
    return;
}

/// This is called for each query and each subject in a BLAST search.
/// @param program BLAST program [in]
/// @param hsp_list HSP list structure [in]
/// @param query_id Query sequence identifier [in]
/// @param subject_id Subject sequence identifier [in]
/// @param query_length Length of query sequence [in]
/// @param subject_length Length of subject sequence [in]
/// @param is_ooframe Was this a search with out-of-frame gapping? [in]
/// @param gi_list List of GIs for the subject sequence [in]
/// @param sa_vector Resulting Seq-align object [in|out] 
void
BLASTHspListToSeqAlign(EBlastProgramType program, BlastHSPList* hsp_list, 
                       const CSeq_id *query_id, const CSeq_id *subject_id, 
                       Int4 query_length, Int4 subject_length, bool is_ooframe,
                       const vector<int> & gi_list,
                       vector<CRef<CSeq_align > > & sa_vector)
{
    // Process the list of HSPs corresponding to one subject sequence and
    // create one seq-align for each list of HSPs (use disc seqalign when
    // multiple HSPs are found).
    BlastHSP** hsp_array = hsp_list->hsp_array;

    sa_vector.clear();

    for (int index = 0; index < hsp_list->hspcnt; index++) { 
        BlastHSP* hsp = hsp_array[index];
        CRef<CSeq_align> seqalign;

        if (is_ooframe) {
            seqalign = 
                s_OOFBlastHSP2SeqAlign(program, hsp, query_id, subject_id,
                                       query_length, subject_length);
        } else {
            seqalign = 
                s_BlastHSP2SeqAlign(program, hsp, query_id, subject_id, 
                                    query_length, subject_length);
        }
        
        s_AddScoresToSeqAlign(seqalign, hsp, gi_list);
        sa_vector.push_back(seqalign);
    }
    
    return;
}

CSeq_align_set*
CreateEmptySeq_align_set(CSeq_align_set* sas)
{
    CSeq_align_set* retval = NULL;

    if (!sas) {
        retval = new CSeq_align_set;
    } else {
        retval = sas;
    }

    return retval;
}

void RemapToQueryLoc(CRef<CSeq_align> sar, const CSeq_loc & query)
{
    _ASSERT(sar);
    const int query_row = 0;
    
    TSeqPos q_shift = 0;
    
    if (query.IsInt()) {
        q_shift = query.GetInt().GetFrom();
    }
    
    if (q_shift > 0) {
        sar->OffsetRow(query_row, q_shift);
    }
}

/// Remap subject alignment if its location specified the reverse strand or
/// a starting location other than the beginning of the sequence.
/// @param subj_aligns Seq-align containing HSPs for a given 
/// query-subject alignment [in|out]
/// @param subj_loc Location of the subject sequence searched. [in]
static void 
s_RemapToSubjectLoc(CRef<CSeq_align> & subj_aligns, const CSeq_loc& subj_loc)
{
    const int kSubjDimension = 1;
    _ASSERT(subj_loc.IsInt() || subj_loc.IsWhole());
    subj_aligns.Reset(RemapAlignToLoc(*subj_aligns, kSubjDimension, subj_loc));
}

/// Retrieve the minimum and maximum subject offsets that span all HSPs in the
/// hitlist.
/// @param hsp_list HSP list to examine [in]
/// @return empty TSeqRange if hsp_list is NULL, otherwise the advertised
/// return value
static TSeqRange s_GetSubjRanges(const BlastHSPList* hsp_list)
{
    TSeqRange retval;

    if ( ! hsp_list ) {
        return retval;
    }

    TSeqPos min = numeric_limits<TSeqPos>::max();
    TSeqPos max = numeric_limits<TSeqPos>::min();

    for (int i = 0; i < hsp_list->hspcnt; i++) {
        const BlastHSP* hsp = hsp_list->hsp_array[i];
        min = MIN(min, static_cast<TSeqPos>(hsp->subject.offset));
        max = MAX(max, static_cast<TSeqPos>(hsp->subject.end));
    }

    retval.SetFrom(min);
    retval.SetTo(max);
    return retval;
}

CSeq_align_set*
BlastHitList2SeqAlign_OMF(const BlastHitList     * hit_list,
                          EBlastProgramType        prog,
                          const CSeq_loc         & query_loc,
                          TSeqPos                  query_length,
                          const IBlastSeqInfoSrc * seqinfo_src,
                          bool                     is_gapped,
                          bool                     is_ooframe,
                          TSeqLocInfoVector      & subj_masks)
{
    CSeq_align_set* seq_aligns = new CSeq_align_set();
    
    if (!hit_list) {
        return CreateEmptySeq_align_set(seq_aligns);
    }
    
    CConstRef<CSeq_id> query_id(& CSeq_loc_CI(query_loc).GetSeq_id());
    _ASSERT(query_id);
    
    for (int index = 0; index < hit_list->hsplist_count; index++) {
        BlastHSPList* hsp_list = hit_list->hsplist_array[index];
        if (!hsp_list)
            continue;
        
        // Sort HSPs with e-values as first priority and scores as 
        // tie-breakers, since that is the order we want to see them in 
        // in Seq-aligns.
        Blast_HSPListSortByEvalue(hsp_list);
        
        const Uint4 kOid = hsp_list->oid;
        TSeqPos subj_length = 0;
        CConstRef<CSeq_id> subject_id;
        GetSequenceLengthAndId(seqinfo_src, kOid, subject_id, &subj_length);
        
        // Extract subject masks
        TMaskedSubjRegions masks;
        if (seqinfo_src->GetMasks(kOid, s_GetSubjRanges(hsp_list), masks)) {
            subj_masks.push_back(masks);
        }

        // Get GIs for entrez query restriction.
        vector<int> gi_list;
        GetFilteredRedundantGis(*seqinfo_src, hsp_list->oid, gi_list);
        
        // stores a CSeq_align for each matching sequence
        vector<CRef<CSeq_align > > hit_align;
        if (is_gapped) {
                BLASTHspListToSeqAlign(prog,
                                       hsp_list,
                                       query_id,
                                       subject_id,
                                       query_length,
                                       subj_length,
                                       is_ooframe,
                                       gi_list,
                                       hit_align);
        } else {
                BLASTUngappedHspListToSeqAlign(prog,
                                               hsp_list,
                                               query_id,
                                               subject_id,
                                               query_length,
                                               subj_length,
                                               gi_list,
                                               hit_align);
        }
        
        ITERATE(vector<CRef<CSeq_align > >, iter, hit_align) {
           RemapToQueryLoc(*iter, query_loc);
           seq_aligns->Set().push_back(*iter);
        }
    }
    return seq_aligns;
}

TSeqAlignVector 
PhiBlastResults2SeqAlign_OMF(const BlastHSPResults  * results, 
                             EBlastProgramType        prog,
                             class ILocalQueryData  & query,
                             const IBlastSeqInfoSrc * seqinfo_src,
                             const SPHIQueryInfo    * pattern_info,
                             vector<TSeqLocInfoVector>&    subj_masks)
{
    TSeqAlignVector retval;

    /* Split results into an array of BlastHSPResults structures corresponding
       to different pattern occurrences. */
    BlastHSPResults* *phi_results = 
        PHIBlast_HSPResultsSplit(results, pattern_info);

    subj_masks.clear();
    subj_masks.resize(pattern_info->num_patterns);

    if (phi_results) {
        for (int pattern_index = 0; pattern_index < pattern_info->num_patterns;
             ++pattern_index) {
            CBlastHSPResults one_phi_results(phi_results[pattern_index]);

            if (one_phi_results) {
                // PHI BLAST does not work with multiple queries, so we only 
                // need to look at the first hit list.
                BlastHitList* hit_list = one_phi_results->hitlist_array[0];

                // PHI BLAST is always gapped, and never out-of-frame, hence
                // true and false values for the respective booleans in the next
                // call.
                CRef<CSeq_align_set> seq_aligns(
                    BlastHitList2SeqAlign_OMF(hit_list,
                                              prog,
                                              *query.GetSeq_loc(0),
                                              query.GetSeqLength(0),
                                              seqinfo_src,
                                              true,
                                              false,
                                              subj_masks[pattern_index]));
                
                retval.push_back(seq_aligns);

            }
            else
            {   // Makes an empty SeqAlign set as this pattern had no hits.
                CRef<CSeq_align_set> seq_aligns(
                    BlastHitList2SeqAlign_OMF(NULL,
                                              prog,
                                              *query.GetSeq_loc(0),
                                              query.GetSeqLength(0),
                                              seqinfo_src,
                                              true,
                                              false,
                                              subj_masks[pattern_index]));
                retval.push_back(seq_aligns);
                
            }
        }
        sfree(phi_results);
    }
    
    return retval;
}

/** Extracts results from the BlastHSPResults structure for only one subject 
 * sequence, identified by its index, and converts them into a vector of 
 * CSeq_align_set objects. Returns one vector element per query sequence; 
 * The CSeq_align_set consists of as many CSeq_align-s as there are HSPs in the
 * BlastHSPList for each query-subject pair
 * @param results results from running the BLAST algorithm [in]
 * @param query_data All query sequences [in]
 * @param seqinfo_src Source of subject sequences information [in]
 * @param prog type of BLAST program [in]
 * @param subj_index Index of this subject sequence in a set [in]
 * @param is_gapped Is this a gapped search? [in]
 * @param is_ooframe Is it a search with out-of-frame gapping? [in]
 * @return Vector of seqalign sets (one set per query sequence).
 */
static TSeqAlignVector
s_BLAST_OneSubjectResults2CSeqAlign(const BlastHSPResults* results,
                                    ILocalQueryData& query_data,
                                    const IBlastSeqInfoSrc& seqinfo_src,
                                    EBlastProgramType prog, 
                                    Uint4 subj_index, 
                                    bool is_gapped, 
                                    bool is_ooframe,
                                    vector<TSeqLocInfoVector>& subj_masks)
{
    _ASSERT(results->num_queries == (int)query_data.GetNumQueries());

    TSeqAlignVector retval;
    CConstRef<CSeq_id> subject_id;
    TSeqPos subj_length = 0;

    // Subject is the same for all queries, so retrieve its id right away
    GetSequenceLengthAndId(&seqinfo_src, subj_index, subject_id, &subj_length);

    vector<CRef<CSeq_align > > hit_align;

    // Process each query's hit list
    for (int qindex = 0; qindex < results->num_queries; qindex++) {
        CRef<CSeq_align_set> seq_aligns;
        BlastHitList* hit_list = results->hitlist_array[qindex];
        BlastHSPList* hsp_list = NULL;

        // Find the HSP list corresponding to this subject, if it exists
        if (hit_list) {
            int sindex;
            for (sindex = 0; sindex < hit_list->hsplist_count; ++sindex) {
                hsp_list = hit_list->hsplist_array[sindex];
                if (hsp_list->oid == static_cast<Int4>(subj_index))
                    break;
            }
            /* If hsp_list for this subject is not found, set it to NULL */
            if (sindex == hit_list->hsplist_count)
                hsp_list = NULL;
        }

        if (hsp_list) {
            // Sort HSPs with e-values as first priority and scores as 
            // tie-breakers, since that is the order we want to see them in 
            // in Seq-aligns.
            Blast_HSPListSortByEvalue(hsp_list);

            CConstRef<CSeq_loc> seqloc = query_data.GetSeq_loc(qindex);
            CConstRef<CSeq_id> query_id(seqloc->GetId());
            TSeqPos query_length = query_data.GetSeqLength(qindex); 
            
            vector<int> gi_list;
            GetFilteredRedundantGis(seqinfo_src, hsp_list->oid, gi_list);

            TMaskedSubjRegions masks;
            if (seqinfo_src.GetMasks(subj_index, 
                                      s_GetSubjRanges(hsp_list), masks)) {
                subj_masks[qindex].push_back(masks);
            }
            
            hit_align.clear();
            if (is_gapped) {
                    BLASTHspListToSeqAlign(prog,
                                           hsp_list,
                                           query_id,
                                           subject_id,
                                           query_length, 
                                           subj_length,
                                           is_ooframe,
                                           gi_list,
                                           hit_align);
            } else {
                    BLASTUngappedHspListToSeqAlign(prog,
                                                   hsp_list,
                                                   query_id,
                                                   subject_id,
                                                   query_length,
                                                   subj_length,
                                                   gi_list,
                                                   hit_align);
            }
            seq_aligns.Reset(new CSeq_align_set());
            CConstRef<CSeq_loc> subj_loc = seqinfo_src.GetSeqLoc(subj_index);
            NON_CONST_ITERATE(vector<CRef<CSeq_align > >, iter, hit_align) {
                RemapToQueryLoc(*iter, *seqloc);
                if ( !is_ooframe )
                    s_RemapToSubjectLoc(*iter, *subj_loc); 
                seq_aligns->Set().push_back(*iter);
            }
        } else {
            seq_aligns.Reset(CreateEmptySeq_align_set(NULL));
        }
        retval.push_back(seq_aligns);
    }

    return retval;
}

/// Transpose the (linearly organized) seqalign set matrix from
/// (q1 s1 q2 s1 ... qN s1, ..., q1 sM q2 sM ... qN sM) to
/// (q1 s1 q1 s2 ... q1 sM, ..., qN s1 qN s2 ... qN sM)
/// this method only reorganizes the seqalign sets, does not copy them.
/// @param alnvec data structure to reorganize [in]
/// @param num_queries number of queries [in]
/// @param num_subjects number of subjects [in]
/// @retval transposed data structure
static TSeqAlignVector 
s_TransposeSeqAlignVector(const TSeqAlignVector& alnvec,
                          const size_t num_queries,
                          const size_t num_subjects)
{
    TSeqAlignVector result_alnvec;

    for (size_t iQuery = 0; iQuery < num_queries; iQuery++)
    {
        for (size_t iSubject = 0; iSubject < num_subjects; iSubject++)
        {
            size_t iLinearIndex = iSubject * num_queries + iQuery;
            CRef<CSeq_align_set> aln_set = alnvec[iLinearIndex];
            result_alnvec.push_back(aln_set);
        }
    }

    _ASSERT(result_alnvec.size() == alnvec.size());
    return result_alnvec;
}

static TSeqAlignVector
s_BlastResults2SeqAlignSequenceCmp_OMF(const BlastHSPResults* results,
                                       EBlastProgramType prog,
                                       class ILocalQueryData& query_data,
                                       const IBlastSeqInfoSrc* seqinfo_src,
                                       bool is_gapped,
                                       bool is_ooframe,
                                       vector<TSeqLocInfoVector>& subj_masks)
{
    TSeqAlignVector retval;
    retval.reserve(query_data.GetNumQueries() * seqinfo_src->Size());

    _ASSERT(results->num_queries == (int)query_data.GetNumQueries());

    // Compute the subject masks
    subj_masks.clear();
    subj_masks.resize(results->num_queries);

    for (Uint4 index = 0; index < seqinfo_src->Size(); index++) {
        TSeqAlignVector seqalign =
            s_BLAST_OneSubjectResults2CSeqAlign(results, query_data, 
                                                *seqinfo_src, prog, index, 
                                                is_gapped, is_ooframe, 
                                                subj_masks);

        /* Merge the new vector with the current. Assume that both vectors
           contain CSeq_align_sets for all queries, i.e. have the same 
           size. */
        _ASSERT(seqalign.size() == query_data.GetNumQueries());

        if (retval.size() == 0) {
            // First time around, just fill the empty vector with the 
            // seqaligns from the first subject.
            retval.swap(seqalign);
        } else {

            for (TSeqAlignVector::size_type i = 0; i < seqalign.size(); ++i) {
                retval.push_back(seqalign[i]);
            }

        }
    }
    _ASSERT(retval.size() == query_data.GetNumQueries() * seqinfo_src->Size());
    return s_TransposeSeqAlignVector(retval, query_data.GetNumQueries(),
                                     seqinfo_src->Size());
}

static TSeqAlignVector
s_BlastResults2SeqAlignDatabaseSearch_OMF(const BlastHSPResults  * results,
                                          EBlastProgramType        prog,
                                          class ILocalQueryData  & query,
                                          const IBlastSeqInfoSrc * seqinfo_src,
                                          bool                     is_gapped,
                                          bool                     is_ooframe,
                                          vector<TSeqLocInfoVector>&  subj_masks)
{
    _ASSERT(results->num_queries == (int)query.GetNumQueries());
    
    TSeqAlignVector retval;
    CConstRef<CSeq_id> query_id;
    
    subj_masks.clear();
    subj_masks.resize(results->num_queries);

    // Process each query's hit list
    for (int index = 0; index < results->num_queries; index++) {
       BlastHitList* hit_list = results->hitlist_array[index];

       CRef<CSeq_align_set>
           seq_aligns(BlastHitList2SeqAlign_OMF(hit_list,
                                                prog,
                                                *query.GetSeq_loc(index),
                                                query.GetSeqLength(index),
                                                seqinfo_src,
                                                is_gapped,
                                                is_ooframe,
                                                subj_masks[index]));
       
       retval.push_back(seq_aligns);
       _TRACE("Query " << index << ": " << seq_aligns->Get().size()
              << " seqaligns");

    }

    return retval;
}

TSeqAlignVector
LocalBlastResults2SeqAlign(BlastHSPResults   * hsp_results,
                   ILocalQueryData   & local_data,
                   const IBlastSeqInfoSrc& seqinfo_src,
                   EBlastProgramType   program,
                   bool                gapped,
                   bool                oof_mode,
                   vector<TSeqLocInfoVector>& subj_masks,
                   EResultType         result_type /* = eDatabaseSearch*/)
{
    TSeqAlignVector retval;
    
    if (! hsp_results)
        return retval;
    
    // For PHI BLAST, results need to be split by query pattern
    // occurrence, which is done in a separate function. Results for
    // different pattern occurrences are put in separate discontinuous
    // Seq_aligns, and linked in a Seq_align_set.
    
    BlastQueryInfo * query_info = local_data.GetQueryInfo();
    
    if (Blast_ProgramIsPhiBlast(program)) {
        retval = PhiBlastResults2SeqAlign_OMF(hsp_results,
                                              program,
                                              local_data,
                                              & seqinfo_src,
                                              query_info->pattern_info,
                                              subj_masks);
    } else {
        if (result_type == eSequenceComparison) {
            retval = 
                s_BlastResults2SeqAlignSequenceCmp_OMF(hsp_results, program, 
                                                       local_data, &seqinfo_src,
                                                       gapped, oof_mode,
                                                       subj_masks);
        } else {
            retval = 
                s_BlastResults2SeqAlignDatabaseSearch_OMF(hsp_results, program, 
                                                          local_data, 
                                                          &seqinfo_src, gapped,
                                                          oof_mode, subj_masks);
        }
    }
    
    return retval;
}

END_SCOPE(blast)
END_NCBI_SCOPE

/* @} */
