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
* Author:  Kamen Todorov, NCBI
*
* File Description:
*   Alignment merger
*
* ===========================================================================
*/


#include <ncbi_pch.hpp>
#include <objtools/alnmgr/alnmerger.hpp>
#include <objtools/alnmgr/alnseq.hpp>
#include <objtools/alnmgr/alnmatch.hpp>
#include <objtools/alnmgr/alnmap.hpp>



BEGIN_NCBI_SCOPE
BEGIN_objects_SCOPE // namespace ncbi::objects::


CAlnMixMerger::CAlnMixMerger(CRef<CAlnMixMatches>& aln_mix_matches,
                             TCalcScoreMethod calc_score)
    : m_DsCnt(aln_mix_matches->m_DsCnt),
      m_AlnMixMatches(aln_mix_matches),
      m_Matches(aln_mix_matches->m_Matches),
      m_AlnMixSequences(aln_mix_matches->m_AlnMixSequences),
      m_Seqs(aln_mix_matches->m_Seqs),
      m_Rows(m_AlnMixSequences->m_Rows),
      m_ExtraRows(m_AlnMixSequences->m_ExtraRows),
      m_SingleRefseq(false),
      m_AlnMixSegments(new CAlnMixSegments(m_AlnMixSequences)),
      x_CalculateScore(calc_score)
{
}


void
CAlnMixMerger::Reset()
{
    m_SingleRefseq = false;
    if (m_DS) {
        m_DS.Reset();
    }
    if (m_Aln) {
        m_Aln.Reset();
    }
    m_AlnMixSegments->m_Segments.clear();
    m_Rows.clear();
    m_ExtraRows.clear();
    NON_CONST_ITERATE (TSeqs, seq_i, m_Seqs) {
        (*seq_i)->m_Starts.clear();
        (*seq_i)->m_ExtraRow = 0;
    }
}


void
CAlnMixMerger::Merge(TMergeFlags flags)
{
    if ( !m_DsCnt ) {
        NCBI_THROW(CAlnException, eMergeFailure,
                   "CAlnMixMerger::Merge(): "
                   "No alignments were added for merging.");
    }
    if ( !m_DS  ||  m_MergeFlags != flags) {
        Reset();
        m_MergeFlags = flags;
        if (m_MergeFlags & fTryOtherMethodOnFail) {
            try {
                x_Merge();
            } catch(...) {
                if (m_MergeFlags & fGen2EST) {
                    m_MergeFlags &= !fGen2EST;
                } else {
                    m_MergeFlags |= fGen2EST;
                }
                try {
                    x_Merge();
                } catch(...) {
                    NCBI_THROW(CAlnException, eUnknownMergeFailure,
                               "CAlnMixMerger::x_Merge(): "
                               "Both Gen2EST and Nucl2Nucl "
                               "merges failed.");
                }
            }
        } else {
            x_Merge();
        }
    }
}


void
CAlnMixMerger::x_Merge()
{
    bool first_refseq = true; // mark the first loop

    // Find the refseq (if such exists)
    {{
        m_SingleRefseq = false;
        m_IndependentDSs = m_DsCnt > 1;

        unsigned int ds_cnt;
        NON_CONST_ITERATE (TSeqs, it, m_Seqs){
            ds_cnt = (*it)->m_DsCnt;
            if (ds_cnt > 1) {
                m_IndependentDSs = false;
            }
            if (ds_cnt == m_DsCnt) {
                m_SingleRefseq = true;
                if ( !first_refseq ) {
                    CRef<CAlnMixSeq> refseq = *it;
                    m_Seqs.erase(it);
                    m_Seqs.insert(m_Seqs.begin(), refseq);
                }
                break;
            }
            first_refseq = false;
        }
    }}

    // Index the sequences
    {{
        int seq_idx=0;
        NON_CONST_ITERATE (TSeqs, seq_i, m_Seqs) {
            (*seq_i)->m_SeqIdx = seq_idx++;
        }
    }}


    // Set the widths if the mix contains both AA & NA
    // or in case we force translation
    if (m_AlnMixMatches->m_ContainsNA  &&  m_AlnMixMatches->m_ContainsAA  ||
        m_AlnMixMatches->m_AddFlags & CAlnMixMatches::fForceTranslation) {
        NON_CONST_ITERATE (TSeqs, seq_i, m_Seqs) {
            (*seq_i)->m_Width = (*seq_i)->m_IsAA ? 1 : 3;
        }
    }



    CAlnMixSeq * refseq = 0, * seq1 = 0, * seq2 = 0;
    TSeqPos start, start1, start2, len, curr_len;
    int width1, width2;
    CAlnMixMatch * match;
    CAlnMixSeq::TMatchList::iterator match_list_iter1, match_list_iter2;
    CAlnMixSeq::TMatchList::iterator match_list_i;
    TSecondRowFits second_row_fits;

    refseq = *(m_Seqs.begin());
    TMatches::iterator match_i = m_Matches.begin();
    m_MatchIdx = 0;
    
    CRef<CAlnMixSegment> seg;
    CAlnMixSeq::TStarts::iterator start_i, lo_start_i, hi_start_i, tmp_start_i;

    first_refseq = true; // mark the first loop

    while (true) {
        // reached the end?
        if (first_refseq ?
            match_i == m_Matches.end()  &&  m_Matches.size() :
            match_list_i == refseq->m_MatchList.end()) {

            // move on to the next refseq
            refseq->m_RefBy = 0;

            // try to find the best scoring 'connected' candidate
            NON_CONST_ITERATE (TSeqs, it, m_Seqs){
                if ( !((*it)->m_MatchList.empty())  &&
                     (*it)->m_RefBy == refseq) {
                    refseq = *it;
                    break;
                }
            }
            if (refseq->m_RefBy == 0) {
                // no candidate was found 'connected' to the refseq
                // continue with the highest scoring candidate
                NON_CONST_ITERATE (TSeqs, it, m_Seqs){
                    if ( !((*it)->m_MatchList.empty()) ) {
                        refseq = *it;
                        break;
                    }
                }
            }

            if (refseq->m_MatchList.empty()) {
                break; // done
            } else {
                first_refseq = false;
                match_list_i = refseq->m_MatchList.begin();
            }
            continue;
        } else {
            // iterate
            match = first_refseq ? *(match_i++) : *(match_list_i++);
        }

        curr_len = len = match->m_Len;

        // is it a match with this refseq?
        if (match->m_AlnSeq1 == refseq) {
            seq1 = match->m_AlnSeq1;
            start1 = match->m_Start1;
            match_list_iter1 = match->m_MatchIter1;
            seq2 = match->m_AlnSeq2;
            start2 = match->m_Start2;
            match_list_iter2 = match->m_MatchIter2;
        } else if (match->m_AlnSeq2 == refseq) {
            seq1 = match->m_AlnSeq2;
            start1 = match->m_Start2;
            match_list_iter1 = match->m_MatchIter2;
            seq2 = match->m_AlnSeq1;
            start2 = match->m_Start1;
            match_list_iter2 = match->m_MatchIter1;
        } else {
            seq1 = match->m_AlnSeq1;
            seq2 = match->m_AlnSeq2;

            // mark the two refseqs, they are candidates for next refseq(s)
            seq1->m_MatchList.push_back(match);
            (match->m_MatchIter1 = seq1->m_MatchList.end())--;
            if (seq2) {
                seq2->m_MatchList.push_back(match);
                (match->m_MatchIter2 = seq2->m_MatchList.end())--;
            }

            // mark that there's no match with this refseq
            seq1 = 0;
        }

        // save the match info into the segments map
        if (seq1) {
            m_MatchIdx++;

            // order the match
            match->m_AlnSeq1 = seq1;
            match->m_Start1 = start1;
            match->m_AlnSeq2 = seq2;
            match->m_Start2 = start2;

            width1 = seq1->m_Width;
            if (seq2) {
                width2 = seq2->m_Width;
            }

            // in case of translated refseq
            if (width1 == 3) {
                x_SetSeqFrame(match, match->m_AlnSeq1);
                {{
                    // reset the ones below,
                    // since match may have been modified
                    seq1 = match->m_AlnSeq1;
                    start1 = match->m_Start1;
                    match_list_iter1 = match->m_MatchIter1;
                    seq2 = match->m_AlnSeq2;
                    start2 = match->m_Start2;
                    match_list_iter2 = match->m_MatchIter2;
                    curr_len = len = match->m_Len;
                }}
            }

            // this match is used, erase from seq1 list
            if ( !first_refseq ) {
                if ( !refseq->m_MatchList.empty() ) {
                    refseq->m_MatchList.erase(match_list_iter1);
                }
            }

            // if a subject sequence place it in the proper row
            if ( !first_refseq  &&  m_MergeFlags & fQuerySeqMergeOnly) {
                bool proper_row_found = false;
                while (true) {
                    if (seq1->m_DsIdx == match->m_DsIdx) {
                        proper_row_found = true;
                        break;
                    } else {
                        if (seq1->m_ExtraRow) {
                            seq1 = match->m_AlnSeq1 = seq1->m_ExtraRow;
                        } else {
                            break;
                        }
                    }
                }
                if ( !proper_row_found   &&
                     !(m_MergeFlags & fTruncateOverlaps)) {
                    NCBI_THROW(CAlnException, eMergeFailure,
                               "CAlnMixMerger::x_Merge(): "
                               "Proper row not found for the match. "
                               "Cannot use fQuerySeqMergeOnly?");
                }
            }
            
            CAlnMixSeq::TStarts& starts = seq1->m_Starts;
            if (seq2) {
                // mark it, it is linked to the refseq
                seq2->m_RefBy = refseq;

                // this match is used erase from seq2 list
                if ( !first_refseq ) {
                    if ( !seq2->m_MatchList.empty() ) {
                        seq2->m_MatchList.erase(match_list_iter2);
                    }
                }
            }

            start_i = starts.end();
            lo_start_i = starts.end();
            hi_start_i = starts.end();


            if (seq2) {
                if (width2 == 3) {
                    // Set the frame if necessary
                    x_SetSeqFrame(match, match->m_AlnSeq2);
                }
                // check if the second row fits
                // this will truncate the match if 
                // there's an inconsistent overlap
                // and truncation was requested
                second_row_fits = x_SecondRowFits(match);
                if (second_row_fits == eIgnoreMatch) {
                    continue;
                }
                {{
                    // reset the ones below,
                    // since match may have been modified
                    seq1 = match->m_AlnSeq1;
                    start1 = match->m_Start1;
                    match_list_iter1 = match->m_MatchIter1;
                    seq2 = match->m_AlnSeq2;
                    start2 = match->m_Start2;
                    match_list_iter2 = match->m_MatchIter2;
                    curr_len = len = match->m_Len;
                }}
            }


            if (!starts.size()) {
                // no starts exist yet

                if ( !m_IndependentDSs ) {
                    // TEMPORARY, for the single refseq version of mix,
                    // clear all MatchLists and exit
                    if ( !(m_SingleRefseq  ||  first_refseq) ) {
                        NON_CONST_ITERATE (TSeqs, it, m_Seqs){
                            if ( !((*it)->m_MatchList.empty()) ) {
                                (*it)->m_MatchList.clear();
                            }
                        }
                        break; 
                    }
                }

                // this seq has not yet been used, set the strand
                if (m_AlnMixMatches->m_AddFlags & CAlnMixMatches::fForceTranslation) {
                    seq1->m_PositiveStrand = (seq1->m_StrandScore >= 0);
                } else {
                    seq1->m_PositiveStrand = ! (m_MergeFlags & fNegativeStrand);
                }

                //create the first one
                seg = new CAlnMixSegment;
                seg->m_Len = len;
                seg->m_DsIdx = match->m_DsIdx;
                starts[start1] = seg;
                seg->m_StartIts[seq1] = 
                    lo_start_i = hi_start_i = starts.begin();

                if (m_MergeFlags & fQuerySeqMergeOnly) {
                    seq2->m_DsIdx = match->m_DsIdx;
                }
                // DONE!
            } else {
                // some starts exist already

                // look ahead
                if ((lo_start_i = start_i = starts.lower_bound(start1))
                    == starts.end()  ||
                    start1 < start_i->first) {
                    // the start position does not exist
                    if (lo_start_i != starts.begin()) {
                        --lo_start_i;
                    }
                }

                // look back
                if (hi_start_i == starts.end()  &&  start_i != lo_start_i) {
                    CAlnMixSegment * prev_seg = lo_start_i->second;
                    if (lo_start_i->first + prev_seg->m_Len * width1 >
                        start1) {
                        // x----..   becomes  x-x--..
                        //   x--..
                        
                        TSeqPos len1 = (start1 - lo_start_i->first) / width1;
                        TSeqPos len2 = prev_seg->m_Len - len1;
                        
                        // create the second seg
                        seg = new CAlnMixSegment;
                        seg->m_Len = len2;
                        seg->m_DsIdx = match->m_DsIdx;
                        starts[start1] = seg;
                        
                        // create rows info
                        ITERATE (CAlnMixSegment::TStartIterators, it, 
                                 prev_seg->m_StartIts) {
                            CAlnMixSeq * seq = it->first;
                            tmp_start_i = it->second;
                            if (seq->m_PositiveStrand ==
                                seq1->m_PositiveStrand) {
                                seq->m_Starts
                                    [tmp_start_i->first + len1 * seq->m_Width]
                                    = seg;
                                if (tmp_start_i != seq->m_Starts.end()) {
                                    seg->m_StartIts[seq] = ++tmp_start_i;
                                } else {
                                    NCBI_THROW(CAlnException, eMergeFailure,
                                               "CAlnMixMerger::x_Merge(): "
                                               "Internal error: tmp_start_i == seq->m_Starts.end()");
                                }
                            } else {
                                seq->m_Starts
                                    [tmp_start_i->first + len2 * seq->m_Width]
                                    = prev_seg;
                                seq->m_Starts[tmp_start_i->first] = seg;
                                seg->m_StartIts[seq] = tmp_start_i;
                                if (tmp_start_i != seq->m_Starts.end()) {
                                    prev_seg->m_StartIts[seq] = ++tmp_start_i;
                                } else {
                                    NCBI_THROW(CAlnException, eMergeFailure,
                                               "CAlnMixMerger::x_Merge(): "
                                               "Internal error: tmp_start_i == seq->m_Starts.end()");
                                }
                            }
                        }
                        
                        // truncate the first seg
                        prev_seg->m_Len = len1;
                        
                        if (start_i != starts.begin()) {
                            start_i--; // point to the newly created start
                        }
                    }
                    if (lo_start_i != starts.end()) {
                        lo_start_i++;
                    }
                }
            }

            // loop through overlapping segments
            start = start1;
            while (hi_start_i == starts.end()) {
                if (start_i != starts.end()  &&  start_i->first == start) {
                    CAlnMixSegment * prev_seg = start_i->second;
                    if (prev_seg->m_Len > curr_len) {
                        // x-------)  becomes  x----)x--)
                        // x----)
                        
                        // create the second seg
                        seg = new CAlnMixSegment;
                        TSeqPos len1 = 
                            seg->m_Len = prev_seg->m_Len - curr_len;
                        start += curr_len * width1;

                        // truncate the first seg
                        prev_seg->m_Len = curr_len;
                        
                        // create rows info
                        ITERATE (CAlnMixSegment::TStartIterators, it, 
                                prev_seg->m_StartIts) {
                            CAlnMixSeq * seq = it->first;
                            tmp_start_i = it->second;
                            if (seq->m_PositiveStrand ==
                                seq1->m_PositiveStrand) {
                                seq->m_Starts[tmp_start_i->first +
                                             curr_len * seq->m_Width]
                                    = seg;
                                if (tmp_start_i != seq->m_Starts.end()) {
                                    seg->m_StartIts[seq] = ++tmp_start_i;
                                } else {
                                    NCBI_THROW(CAlnException, eMergeFailure,
                                               "CAlnMixMerger::x_Merge(): "
                                               "Internal error: tmp_start_i == seq->m_Starts.end()");
                                }
                            } else{
                                seq->m_Starts[tmp_start_i->first +
                                             len1 * seq->m_Width]
                                    = prev_seg;
                                seq->m_Starts[tmp_start_i->first] = seg;
                                seg->m_StartIts[seq] = tmp_start_i;
                                if (tmp_start_i != seq->m_Starts.end()) {
                                    prev_seg->m_StartIts[seq] = ++tmp_start_i;
                                } else {
                                    NCBI_THROW(CAlnException, eMergeFailure,
                                               "CAlnMixMerger::x_Merge(): "
                                               "Internal error: tmp_start_i == seq->m_Starts.end()");
                                }
                            }
                        }
#if _DEBUG && _ALNMGR_DEBUG                        
                        prev_seg->StartItsConsistencyCheck(*seq1,
                                                           start,
                                                           m_MatchIdx);
#endif


                        hi_start_i = start_i; // DONE!
                    } else if (curr_len == prev_seg->m_Len) {
                        // x----)
                        // x----)
                        hi_start_i = start_i; // DONE!
                    } else {
                        // x----)     becomes  x----)x--)
                        // x-------)
                        start += prev_seg->m_Len * width1;
                        curr_len -= prev_seg->m_Len;
                        if (start_i != starts.end()) {
                            start_i++;
                        }
                    }
                } else {
                    seg = new CAlnMixSegment;
                    starts[start] = seg;
                    tmp_start_i = start_i;
                    if (tmp_start_i != starts.begin()) {
                        tmp_start_i--;
                    }
                    seg->m_StartIts[seq1] = tmp_start_i;
                    if (start_i != starts.end()  &&
                        start + curr_len * width1 > start_i->first) {
                        //       x--..
                        // x--------..
                        seg->m_Len = (start_i->first - start) / width1;
                        seg->m_DsIdx = match->m_DsIdx;
                    } else {
                        //       x-----)
                        // x---)
                        seg->m_Len = curr_len;
                        seg->m_DsIdx = match->m_DsIdx;
                        hi_start_i = start_i;
                        if (hi_start_i != starts.begin()) {
                            hi_start_i--; // DONE!
                        }
                    }
                    start += seg->m_Len * width1;
                    curr_len -= seg->m_Len;
                    if (lo_start_i == start_i) {
                        if (lo_start_i != starts.begin()) {
                            lo_start_i--;
                        }
                    }
                }
            }
                 
            // try to resolve the second row
            if (seq2) {
                // create a copy of the match,
                // which we could work with temporarily
                // without modifying the original
                CAlnMixMatch tmp_match = *match;
                match = &tmp_match; // point to the new tmp_match

                if (second_row_fits == eFirstRowOverlapBelow  ||
                    second_row_fits == eFirstRowOverlapAbove) {
                    // try it again, it may fit this time
                    // since the second row may have been
                    // cut into smaller segments
                    // or new frame was set
                    second_row_fits = x_SecondRowFits(match);
                }
                while (second_row_fits != eSecondRowFitsOk  &&
                       second_row_fits != eIgnoreMatch) {
                    if (!seq2->m_ExtraRow) {
                        // create an extra row
                        CRef<CAlnMixSeq> row (new CAlnMixSeq);
                        row->m_BioseqHandle = seq2->m_BioseqHandle;
                        row->m_SeqId = seq2->m_SeqId;
                        row->m_Width = seq2->m_Width;
                        row->m_Frame = start2 % 3;
                        row->m_SeqIdx = seq2->m_SeqIdx;
                        if (m_MergeFlags & fQuerySeqMergeOnly) {
                            row->m_DsIdx = match->m_DsIdx;
                        }
                        m_ExtraRows.push_back(row);
                        row->m_ExtraRowIdx = seq2->m_ExtraRowIdx + 1;
                        seq2 = match->m_AlnSeq2 = seq2->m_ExtraRow = row;
                        break;
                    }
                    seq2 = match->m_AlnSeq2 = seq2->m_ExtraRow;

                    second_row_fits = x_SecondRowFits(match);
                    {{
                        // reset the ones below,
                        // since match may have been modified
                        seq1 = match->m_AlnSeq1;
                        start1 = match->m_Start1;
                        match_list_iter1 = match->m_MatchIter1;
                        seq2 = match->m_AlnSeq2;
                        start2 = match->m_Start2;
                        match_list_iter2 = match->m_MatchIter2;
                        curr_len = len = match->m_Len;
                    }}
                }
                if (second_row_fits == eIgnoreMatch) {
                    continue;
                }

                if (m_MergeFlags & fTruncateOverlaps) {
                    // we need to reset these shtorcuts
                    // in case the match was truncated
                    start1 = match->m_Start1;
                    start2 = match->m_Start2;
                    len = match->m_Len;
                }

                // set the strand if first time
                if (seq2->m_Starts.empty()) {
                    seq2->m_PositiveStrand = 
                        (seq1->m_PositiveStrand ?
                         !match->m_StrandsDiffer :
                         match->m_StrandsDiffer);
                }

                // create row info
                CAlnMixSeq::TStarts& starts2 = match->m_AlnSeq2->m_Starts;
                start = start2;
                CAlnMixSeq::TStarts::iterator start2_i
                    = starts2.lower_bound(start2);
                start_i = match->m_StrandsDiffer ? hi_start_i : lo_start_i;

                while(start < start2 + len * width2) {
                    if (start2_i != starts2.end() &&
                        start2_i->first == start) {
                        // this position already exists

                        if (start2_i->second != start_i->second) {
                            // merge the two segments

                            // store the seg in a CRef to delay its deletion
                            // until after the iteration on it is finished
                            CRef<CAlnMixSegment> tmp_seg = start2_i->second;

                            ITERATE (CAlnMixSegment::TStartIterators,
                                     it, 
                                     tmp_seg->m_StartIts) {
                                CAlnMixSeq * tmp_seq = it->first;
                                tmp_start_i = it->second;
                                tmp_start_i->second = start_i->second;
                                start_i->second->m_StartIts[tmp_seq] =
                                    tmp_start_i;
                            }
#if _DEBUG && _ALNMGR_DEBUG                        
                            start_i->second->StartItsConsistencyCheck(*seq2,
                                                                      start,
                                                                      m_MatchIdx);
#endif
                        }
                    } else {
                        // this position does not exist, create it
                        seq2->m_Starts[start] = start_i->second;

                        // start2_i != starts.begin() because we just 
                        // made an insertion, so decrement is ok
                        start2_i--;
                        
                        // point this segment's row start iterator
                        start_i->second->m_StartIts[seq2] = start2_i;
#if _DEBUG && _ALNMGR_DEBUG                        
                        start_i->second->StartItsConsistencyCheck(*seq2,
                                                                  start,
                                                                  m_MatchIdx);
#endif
                    }

                    // increment values
                    start += start_i->second->m_Len * width2;
                    if (start2_i != starts2.end()) {
                        start2_i++;
                    }
                    if (match->m_StrandsDiffer) {
                        if (start_i != starts.begin()) {
                            start_i--;
                        }
                    } else {
                        if (start_i != starts.end()) {
                            start_i++;
                        }
                    }
                }
            }
        }
    }
    m_AlnMixSequences->BuildRows();
    m_AlnMixSegments->Build(m_MergeFlags & fGapJoin,
                            m_MergeFlags & fMinGap);
    if (m_MergeFlags & fFillUnalignedRegions) {
        m_AlnMixSegments->FillUnalignedRegions();
    }
    x_CreateDenseg();
}


CAlnMixMerger::TSecondRowFits
CAlnMixMerger::x_SecondRowFits(CAlnMixMatch * match) const
{
    CAlnMixSeq*&                  seq1    = match->m_AlnSeq1;
    CAlnMixSeq*&                  seq2    = match->m_AlnSeq2;
    TSeqPos&                      start1  = match->m_Start1;
    TSeqPos&                      start2  = match->m_Start2;
    TSeqPos&                      len     = match->m_Len;
    const int&                    width1  = seq1->m_Width;
    const int&                    width2  = seq2->m_Width;
    CAlnMixSeq::TStarts::iterator start_i;
    TSignedSeqPos                 delta, delta1, delta2;

    // subject sequences go on separate rows if requested
    if (m_MergeFlags & fQuerySeqMergeOnly) {
        if (seq2->m_DsIdx) {
            if ( !(m_MergeFlags & fTruncateOverlaps) ) {
                if (seq2->m_DsIdx == match->m_DsIdx) {
                    return eSecondRowFitsOk;
                } else {
                    return eForceSeparateRow;
                }
            }
        } else {
            seq2->m_DsIdx = match->m_DsIdx;
            if ( !(m_MergeFlags & fTruncateOverlaps) ) {
                return eSecondRowFitsOk;
            }
        }
    }

    if ( !seq2->m_Starts.empty() ) {

        // check strand
        while (true) {
            if (seq2->m_PositiveStrand !=
                (seq1->m_PositiveStrand ?
                 !match->m_StrandsDiffer :
                 match->m_StrandsDiffer)) {
                if (seq2->m_ExtraRow) {
                    seq2 = seq2->m_ExtraRow;
                } else {
                    return eInconsistentStrand;
                }
            } else {
                break;
            }
        }

        // check frame
        if (seq2->m_Width == 3  &&  seq2->m_Frame != start2 % 3) {
            return eInconsistentFrame;
        }

        start_i = seq2->m_Starts.lower_bound(start2);

        // check below
        if (start_i != seq2->m_Starts.begin()) {
            start_i--;
            
            // check for inconsistency on the first row
            if ( !m_IndependentDSs ) {
                CAlnMixSegment::TStartIterators::iterator start_it_i =
                    start_i->second->m_StartIts.find(seq1);
                if (start_it_i != start_i->second->m_StartIts.end()) {
                    if (match->m_StrandsDiffer) {
                        delta = start1 + len * width1 - start_it_i->second->first;
                        if (delta > 0) {
                            // target above
                            // x----- x-)-)
                            // (----x (---x
                            // target below
                            if (m_MergeFlags & fTruncateOverlaps) {
                                delta /= width1;
                                if (len > delta) {
                                    len -= delta;
                                    start2 += delta * width2;
                                } else {
                                    return eIgnoreMatch;
                                }
                            } else {
                                return eFirstRowOverlapAbove;
                            }
                        }
                    } else {
                        delta = start_it_i->second->first
                            + start_i->second->m_Len * width1
                            - start1;
                        if (delta > 0) {
                            // below target
                            // x---- x-)--)
                            // x---) x----)
                            // below target
                            if (m_MergeFlags & fTruncateOverlaps) {
                                delta /= width1;
                                if (len > delta) {
                                    len -= delta;
                                    start1 += delta * width1;
                                    start2 += delta * width2;
                                } else {
                                    return eIgnoreMatch;
                                }
                            } else {
                                return eFirstRowOverlapBelow;
                            }
                        }
                    }
                }
            }

            // check for overlap with the segment below on second row
            if ((delta = start_i->first + start_i->second->m_Len * width2
                 - start2) > 0) {
                //       target
                // ----- ------
                // x---- x-)--)
                // below target
                if (m_MergeFlags & fTruncateOverlaps) {
                    delta /= width2;
                    if (len > delta) {
                        len -= delta;
                        start2 += delta * width2;
                        if ( !match->m_StrandsDiffer ) {
                            start1 += delta * width1;
                        }
                    } else {
                        return eIgnoreMatch;
                    }
                } else {
                    return eSecondRowOverlap;
                }
            }
            if (start_i != seq2->m_Starts.end()) {
                start_i++;
            }
        }

        // check the overlapping area for consistency
        while (start_i != seq2->m_Starts.end()  &&  
               start_i->first < start2 + len * width2) {
            if ( !m_IndependentDSs ) {
                CAlnMixSegment::TStartIterators::iterator start_it_i =
                    start_i->second->m_StartIts.find(seq1);
                if (start_it_i != start_i->second->m_StartIts.end()) {
                    if (match->m_StrandsDiffer) {
                        // x---..- x---..--)
                        // (---..- (--x..--x
                        delta1 = (start1 - start_it_i->second->first) / width1 +
                            len - start_i->second->m_Len;
                    } else {
                        // x--..- x---...-)
                        // x--..- x---...-)
                        delta1 = (start_it_i->second->first - start1) / width1;
                    }
                    delta2 = (start_i->first - start2) / width2;
                    if (delta1 != delta2) {
                        if (m_MergeFlags & fTruncateOverlaps) {
                            delta = (delta1 < delta2 ? delta1 : delta2);
                            if (delta > 0) {
                                if (match->m_StrandsDiffer) {
                                    start1 += (len - delta) * width1;
                                }
                                len = delta;
                            } else {
                                return eIgnoreMatch;
                            }
                        } else {
                            return eInconsistentOverlap;
                        }
                    }
                }
            }
            start_i++;
        }

        // check above for consistency
        if (start_i != seq2->m_Starts.end()) {
            if ( !m_IndependentDSs ) {
                CAlnMixSegment::TStartIterators::iterator start_it_i =
                    start_i->second->m_StartIts.find(seq1);
                if (start_it_i != start_i->second->m_StartIts.end()) {
                    if (match->m_StrandsDiffer) {
                        delta = start_it_i->second->first + 
                            start_i->second->m_Len * width1 - start1;
                        if (delta > 0) {
                            // below target
                            // x---- x-)--)
                            // (---x (----x
                            // above target
                            if (m_MergeFlags & fTruncateOverlaps) {
                                if (len > delta / width1) {
                                    len -= delta / width1;
                                    start1 += delta;
                                } else {
                                    return eIgnoreMatch;
                                }
                            } else {
                                return eFirstRowOverlapBelow;
                            }
                        }
                    } else {
                        delta = start1 + len * width1 - start_it_i->second->first;
                        if (delta > 0) {
                            // target above
                            // x--x-) ----)
                            // x----) x---)
                            // target above
                            if (m_MergeFlags & fTruncateOverlaps) {
                                if (len <= delta / width1) {
                                    return eIgnoreMatch;
                                } else {
                                    len -= delta / width1;
                                }
                            } else {
                                return eFirstRowOverlapAbove;
                            }
                        }
                    }
                }
            }
        }

        // check for inconsistent matches
        if ((start_i = seq1->m_Starts.find(start1)) == seq1->m_Starts.end() ||
            start_i->first != start1) {
            // commenting out for now, since moved the function call ahead            
//             NCBI_THROW(CAlnException, eMergeFailure,
//                        "CAlnMixMerger::x_SecondRowFits(): "
//                        "Internal error: seq1->m_Starts do not match");
        } else {
            CAlnMixSegment::TStartIterators::iterator it;
            TSeqPos tmp_start =
                match->m_StrandsDiffer ? start2 + len * width2 : start2;
            while (start_i != seq1->m_Starts.end()  &&
                   start_i->first < start1 + len * width1) {

                CAlnMixSegment::TStartIterators& its = 
                    start_i->second->m_StartIts;

                if (match->m_StrandsDiffer) {
                    tmp_start -= start_i->second->m_Len * width2;
                }

                if ((it = its.find(seq2)) != its.end()) {
                    if (it->second->first != tmp_start) {
                        // found an inconsistent prev match
                        return eSecondRowInconsistency;
                    }
                }

                if ( !match->m_StrandsDiffer ) {
                    tmp_start += start_i->second->m_Len * width2;
                }

                start_i++;
            }
        }
    }
    if (m_MergeFlags & fQuerySeqMergeOnly) {
        _ASSERT(m_MergeFlags & fTruncateOverlaps);
        if (seq2->m_DsIdx == match->m_DsIdx) {
            return eSecondRowFitsOk;
        } else {
            return eForceSeparateRow;
        }
    }
    return eSecondRowFitsOk;
}


void
CAlnMixMerger::x_CreateDenseg()
{
    int numrow  = 0,
        numrows = m_Rows.size();
    int numseg  = 0,
        numsegs = m_AlnMixSegments->m_Segments.size();
    int num     = numrows * numsegs;

    m_DS = new CDense_seg();
    m_DS->SetDim(numrows);
    m_DS->SetNumseg(numsegs);

    m_Aln = new CSeq_align();
    m_Aln->SetType(CSeq_align::eType_not_set);
    m_Aln->SetSegs().SetDenseg(*m_DS);
    m_Aln->SetDim(numrows);

    CDense_seg::TIds&     ids     = m_DS->SetIds();
    CDense_seg::TStarts&  starts  = m_DS->SetStarts();
    CDense_seg::TStrands& strands = m_DS->SetStrands();
    CDense_seg::TLens&    lens    = m_DS->SetLens();

    ids.resize(numrows);
    lens.resize(numsegs);
    starts.resize(num, -1);
    strands.resize(num, eNa_strand_minus);

    // ids
    numrow = 0;
    ITERATE(TSeqs, row_i, m_Rows) {
        ids[numrow++] = (*row_i)->m_SeqId;
    }

    int offset = 0;
    numseg = 0;
    ITERATE(CAlnMixSegments::TSegments,
            seg_i,
            m_AlnMixSegments->m_Segments) {

        // lens
        lens[numseg] = (*seg_i)->m_Len;

        // starts
        ITERATE (CAlnMixSegment::TStartIterators, start_its_i,
                (*seg_i)->m_StartIts) {
            starts[offset + start_its_i->first->m_RowIdx] =
                start_its_i->second->first;
        }

        // strands
        numrow = 0;
        ITERATE(TSeqs, row_i, m_Rows) {
            if ((*row_i)->m_PositiveStrand) {
                strands[offset + numrow] = eNa_strand_plus;
            }
            numrow++;
        }

        // next segment
        numseg++; offset += numrows;
    }

    // widths
    if (m_AlnMixMatches->m_ContainsNA  &&  m_AlnMixMatches->m_ContainsAA  || 
        m_AlnMixMatches->m_AddFlags & CAlnMixMatches::fForceTranslation) {
        CDense_seg::TWidths&  widths  = m_DS->SetWidths();
        widths.resize(numrows);
        numrow = 0;
        ITERATE (TSeqs, row_i, m_Rows) {
            widths[numrow++] = (*row_i)->m_Width;
        }
    }
#if _DEBUG
    m_DS->Validate(true);
#endif    
}


void
CAlnMixMerger::x_SetSeqFrame(CAlnMixMatch* match, CAlnMixSeq*& seq)
{
    unsigned frame;
    if (seq == match->m_AlnSeq1) {
        frame = match->m_Start1 % 3;
    } else {
        frame = match->m_Start2 % 3;
    }
    if (seq->m_Starts.empty()) {
        seq->m_Frame = frame;
    } else {
        while (seq->m_Frame != frame) {
            if (!seq->m_ExtraRow) {
                // create an extra frame
                CRef<CAlnMixSeq> new_seq (new CAlnMixSeq);
                new_seq->m_BioseqHandle = seq->m_BioseqHandle;
                new_seq->m_SeqId = seq->m_SeqId;
                new_seq->m_PositiveStrand = seq->m_PositiveStrand;
                new_seq->m_Width = seq->m_Width;
                new_seq->m_Frame = frame;
                new_seq->m_SeqIdx = seq->m_SeqIdx;
                if (m_MergeFlags & fQuerySeqMergeOnly) {
                    new_seq->m_DsIdx = match->m_DsIdx;
                }
                m_ExtraRows.push_back(new_seq);
                new_seq->m_ExtraRowIdx = seq->m_ExtraRowIdx + 1;
                seq = seq->m_ExtraRow = new_seq;
                break;
            }
            seq = seq->m_ExtraRow;
        }
    }
}


END_objects_SCOPE // namespace ncbi::objects::
END_NCBI_SCOPE


/*
* ===========================================================================
*
* $Log$
* Revision 1.2  2005/03/10 19:33:00  todorov
* Moved a few routines out of the merger to their corresponding classes
*
* Revision 1.1  2005/03/01 17:28:49  todorov
* Rearranged CAlnMix classes
*
* ===========================================================================
*/
