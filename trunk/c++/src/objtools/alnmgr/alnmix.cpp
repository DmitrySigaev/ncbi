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

#include <objtools/alnmgr/alnmix.hpp>

#include <objects/seq/Bioseq.hpp>
#include <objects/seqloc/Seq_id.hpp>

// Object Manager includes
#include <objmgr/gbloader.hpp>
#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>

#include <algorithm>

BEGIN_NCBI_SCOPE
BEGIN_objects_SCOPE // namespace ncbi::objects::


CAlnMix::CAlnMix(void)
    : m_MergeFlags(0),
      m_SingleRefseq(false)
{
    x_CreateScope();
}


CAlnMix::CAlnMix(CScope& scope)
    : m_Scope(&scope),
      m_MergeFlags(0),
      m_SingleRefseq(false)
{
}


CAlnMix::~CAlnMix(void)
{
}


inline
bool CAlnMix::x_CompareAlnSeqScores(const CAlnMixSeq* aln_seq1,
                                    const CAlnMixSeq* aln_seq2) 
{
    return aln_seq1->m_Score > aln_seq2->m_Score;
}


inline
bool CAlnMix::x_CompareAlnMatchScores(const CRef<CAlnMixMatch>& aln_match1, 
                                      const CRef<CAlnMixMatch>& aln_match2) 
{
    return aln_match1->m_Score > aln_match2->m_Score;
}


void CAlnMix::x_Reset()
{
    if (m_DS) {
        m_DS.Reset();
    }
    if (m_Aln) {
        m_Aln.Reset();
    }
    m_Segments.clear();
    m_Rows.clear();
    m_ExtraRows.clear();
    ITERATE (TSeqs, seq_i, m_Seqs) {
        (*seq_i)->m_Starts.clear();
        (*seq_i)->m_ExtraRow = 0;
    }
}


void CAlnMix::x_CreateScope()
{
    m_ObjMgr = new CObjectManager;
    
    m_ObjMgr->RegisterDataLoader(*new CGBDataLoader("ID", NULL, 2),
                                 CObjectManager::eDefault);

    m_Scope = new CScope(*m_ObjMgr);
    m_Scope->AddDefaults();
}


void CAlnMix::Merge(TMergeFlags flags)
{
    if ( !m_DS  ||  m_MergeFlags != flags) {
        if (m_InputDSs.size()) {
            x_Reset();
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
                                   "CAlnMix::x_Merge(): "
                                   "Both Gen2EST and Nucl2Nucl "
                                   "merges failed.");
                    }
                }
            } else {
                x_Merge();
            }
        }
    }
}


void CAlnMix::Add(const CDense_seg &ds, TAddFlags flags)
{
    if (m_InputDSsMap.find((void *)&ds) != m_InputDSsMap.end()) {
        return; // it has already been added
    }
    x_Reset();

    if (flags & fDontUseObjMgr  &&  flags & fCalcScore) {
        NCBI_THROW(CAlnException, eMergeFailure, "CAlnMix::Add(): "
                   "fCalcScore cannot be used together with fDontUseObjMgr");
    }
    m_AddFlags = flags;

    m_InputDSsMap[(void *)&ds] = &ds;
    m_InputDSs.push_back(CConstRef<CDense_seg>(&ds));

    vector<CRef<CAlnMixSeq> > ds_seq;

    //store the bioseq handles
    for (CAlnMap::TNumrow row = 0;  row < ds.GetDim();  row++) {

        CRef<CAlnMixSeq> aln_seq;

        if (m_AddFlags & fDontUseObjMgr) {
            // identify sequences by their seq ids as provided by
            // the dense seg (not as reliable as with OM, but faster)
            CRef<CSeq_id> seq_id(new CSeq_id);
            seq_id->Assign(*ds.GetIds()[row]);

            TSeqIdMap::iterator it = m_SeqIds.find(seq_id);
            if (it == m_SeqIds.end()) {
                // add this seq id
                aln_seq = new CAlnMixSeq();
                m_SeqIds[seq_id] = aln_seq;
                aln_seq->m_SeqId = seq_id;
                aln_seq->m_DS_Count = 1;

                // add this sequence
                m_Seqs.push_back(aln_seq);
            
                // mark if protein sequence
                aln_seq->m_IsAA = false;

            } else {
                aln_seq = it->second;
                aln_seq->m_DS_Count++;
            }
            
        } else {
            // uniquely identify the bioseq
            CBioseq_Handle bioseq_handle = 
                GetScope().GetBioseqHandle(*(ds.GetIds())[row]);

            if ( !bioseq_handle ) {
                string errstr = string("CAlnMix::Add(): ") 
                    + "Seq-id cannot be resolved: "
                    + (ds.GetIds())[row]->AsFastaString();
                
                NCBI_THROW(CAlnException, eMergeFailure, errstr);
            }

            TBioseqHandleMap::iterator it = m_BioseqHandles.find(bioseq_handle);
            if (it == m_BioseqHandles.end()) {
                // add this bioseq handle
                aln_seq = new CAlnMixSeq();
                m_BioseqHandles[bioseq_handle] = aln_seq;
                aln_seq->m_BioseqHandle = 
                    &m_BioseqHandles.find(bioseq_handle)->first;
                
                CRef<CSeq_id> seq_id(new CSeq_id);
                seq_id->Assign(*aln_seq->m_BioseqHandle->GetSeqId());
                m_SeqIds[seq_id] = aln_seq;
                aln_seq->m_SeqId = seq_id;
                aln_seq->m_DS_Count = 1;

                // add this sequence
                m_Seqs.push_back(aln_seq);
            
                // mark if protein sequence
                aln_seq->m_IsAA = aln_seq->m_BioseqHandle->GetBioseqCore()
                    ->GetInst().GetMol() == CSeq_inst::eMol_aa;

            } else {
                aln_seq = it->second;
                aln_seq->m_DS_Count++;
            }
        }
        ds_seq.push_back(aln_seq);
    }

    //record all alignment relations
    int              seg_off = 0;
    TSignedSeqPos    start1, start2;
    TSeqPos          len;
    bool             single_chunk;
    CAlnMap::TNumrow first_non_gapped_row_found;
    bool             strands_exist = 
        ds.GetStrands().size() == ds.GetNumseg() * ds.GetDim();

    for (CAlnMap::TNumseg seg =0;  seg < ds.GetNumseg();  seg++) {
        len = ds.GetLens()[seg];
        single_chunk = true;

        for (CAlnMap::TNumrow row1 = 0;  row1 < ds.GetDim();  row1++) {
            if ((start1 = ds.GetStarts()[seg_off + row1]) >= 0) {
                //search for a match for the piece of seq on row1

                CRef<CAlnMixSeq> aln_seq1 = ds_seq[row1];

                for (CAlnMap::TNumrow row2 = row1+1;
                     row2 < ds.GetDim();  row2++) {
                    if ((start2 = ds.GetStarts()[seg_off + row2]) >= 0) {
                        //match found
                        if (single_chunk) {
                            single_chunk = false;
                            first_non_gapped_row_found = row1;
                        }
                        
                        //create the match
                        CRef<CAlnMixMatch> match(new CAlnMixMatch);

                        //add only pairs with the first_non_gapped_row_found
                        //still, calc the score to be added to the seqs' scores
                        if (row1 == first_non_gapped_row_found) {
                            m_Matches.push_back(match);
                        }

                        CRef<CAlnMixSeq> aln_seq2 = ds_seq[row2];


                        match->m_AlnSeq1 = aln_seq1;
                        match->m_Start1 = start1;
                        match->m_AlnSeq2 = aln_seq2;
                        match->m_Start2 = start2;
                        match->m_Len = len;
                        match->m_DSIndex = m_InputDSs.size();

                        // determine the strand
                        match->m_StrandsDiffer = false;
                        ENa_strand strand1 = eNa_strand_plus;
                        ENa_strand strand2 = eNa_strand_plus;
                        if (strands_exist) {
                            if (ds.GetStrands()[seg_off + row1] 
                                == eNa_strand_minus) {
                                strand1 = eNa_strand_minus;
                            }
                            if (ds.GetStrands()[seg_off + row2] 
                                == eNa_strand_minus) {
                                strand2 = eNa_strand_minus;
                            }

                            if (strand1 == eNa_strand_minus  &&
                                strand2 != eNa_strand_minus  ||
                                strand1 != eNa_strand_minus  &&
                                strand2 == eNa_strand_minus) {

                                match->m_StrandsDiffer = true;

                            }
                        }


                        //Determine the score
                        if (m_AddFlags & fCalcScore) {
                            // calc the score by seq comp
                            string s1, s2;

                            if (strand1 == eNa_strand_minus) {
                                CSeqVector seq_vec = 
                                    aln_seq1->m_BioseqHandle->GetSeqVector
                                    (CBioseq_Handle::eCoding_Iupac,
                                     CBioseq_Handle::eStrand_Minus);
                                TSeqPos size = seq_vec.size();
                                seq_vec.GetSeqData(size - (start1 + len),
                                                   size - start1, 
                                                   s1);
                            } else {
                                aln_seq1->m_BioseqHandle->GetSeqVector
                                    (CBioseq_Handle::eCoding_Iupac,
                                     CBioseq_Handle::eStrand_Plus).
                                    GetSeqData(start1, start1 + len, s1);
                            }                                
                            if (strand2 ==  eNa_strand_minus) {
                                CSeqVector seq_vec = 
                                    aln_seq2->m_BioseqHandle->GetSeqVector
                                    (CBioseq_Handle::eCoding_Iupac,
                                     CBioseq_Handle::eStrand_Minus);
                                TSeqPos size = seq_vec.size();
                                seq_vec.GetSeqData(size - (start2 + len),
                                                   size - start2, 
                                                   s2);
                            } else {
                                aln_seq2->m_BioseqHandle->GetSeqVector
                                    (CBioseq_Handle::eCoding_Iupac,
                                     CBioseq_Handle::eStrand_Plus).
                                    GetSeqData(start2, start2 + len, s2);
                            }

                            //verify that we were able to load all data
                            if (s1.length() != len || s2.length() != len) {
                                string errstr = string("CAlnMix::Add(): ")
                                    + "Unable to load data for segment "
                                    + NStr::IntToString(seg) +
                                    ", rows " + NStr::IntToString(row1) +
                                    " and " + NStr::IntToString(row2);
                                NCBI_THROW(CAlnException, eInvalidSegment,
                                           errstr);
                            }

                            match->m_Score = 
                                CAlnVec::CalculateScore
                                (s1, s2, aln_seq1->m_IsAA, aln_seq2->m_IsAA);
                        } else {
                            match->m_Score = len;
                        }
                        
                        aln_seq1->m_Score += match->m_Score;
                        aln_seq2->m_Score += match->m_Score;

                    }
                }
                if (single_chunk) {
                    //record it
                    CRef<CAlnMixMatch> match(new CAlnMixMatch);
                    match->m_Score = 0;
                    match->m_AlnSeq1 = aln_seq1;
                    match->m_Start1 = start1;
                    match->m_AlnSeq2 = 0;
                    match->m_Start2 = 0;
                    match->m_Len = len;
                    match->m_StrandsDiffer = false;
                    match->m_DSIndex = m_InputDSs.size();
                    m_Matches.push_back(match);
                }
            }
        }
        seg_off += ds.GetDim();
    }
}


void CAlnMix::x_Merge()
{
    bool first_refseq = true; // mark the first loop

    if (m_MergeFlags & fSortSeqsByScore) {
        stable_sort(m_Seqs.begin(), m_Seqs.end(), x_CompareAlnSeqScores);
    }

    // Find the refseq (if such exists)
    {{
        m_SingleRefseq = false;
        m_IndependentDSs = m_InputDSs.size() > 1;

        unsigned int ds_cnt;
        NON_CONST_ITERATE (TSeqs, it, m_Seqs){
            ds_cnt = (*it)->m_DS_Count;
            if (ds_cnt > 1) {
                m_IndependentDSs = false;
            }
            if (ds_cnt == m_InputDSs.size()) {
                m_SingleRefseq = true;
                if ( !first_refseq ) {
                    CAlnMixSeq * refseq = *it;
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
        ITERATE (TSeqs, seq_i, m_Seqs) {
            (*seq_i)->m_SeqIndex = seq_idx++;
        }
    }}

    // Sort matches by score
    stable_sort(m_Matches.begin(), m_Matches.end(), x_CompareAlnMatchScores);

    CAlnMixSeq * refseq = 0, * seq1 = 0, * seq2 = 0;
    TSeqPos start, start1, start2, len, curr_len;
    CAlnMixMatch * match;
    CAlnMixSeq::TMatchList::iterator match_list_iter1, match_list_iter2;
    CAlnMixSeq::TMatchList::iterator match_list_i;

    refseq = *(m_Seqs.begin());
    TMatches::iterator match_i = m_Matches.begin();

    CRef<CAlnMixSegment> seg;
    CAlnMixSeq::TStarts::iterator start_i, lo_start_i, hi_start_i, tmp_start_i;

    first_refseq = true; // mark the first loop

    while (true) {
        if (first_refseq) {
            match = *(match_i++);
        } else {
            match = *(match_list_i++);
        }

        curr_len = len = match->m_Len;

        // is it a match_i with this refseq?
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
            // order the match
            match->m_AlnSeq1 = seq1;
            match->m_Start1 = start1;
            match->m_AlnSeq2 = seq2;
            match->m_Start2 = start2;

            // this match is used erase from seq1 list
            if ( !first_refseq ) {
                seq1->m_MatchList.erase(match_list_iter1);
            }

            // if a subect sequence place it in the proper row
            if ( !first_refseq  &&  m_MergeFlags & fQuerySeqMergeOnly) {
                bool proper_row_found = false;
                while (true) {
                    if (seq1->m_DSIndex == match->m_DSIndex) {
                        proper_row_found = true;
                        break;
                    } else {
                        if (seq1->m_ExtraRow) {
                            seq1 = match->m_AlnSeq2 = seq1->m_ExtraRow;
                        } else {
                            break;
                        }
                    }
                }
                if ( !proper_row_found ) {
                    NCBI_THROW(CAlnException, eMergeFailure,
                               "CAlnMix::x_Merge(): "
                               "Proper row not found for the match. "
                               "Cannot use fQuerySeqMergeOnly?");
                }
            }
            
            if (m_MergeFlags & fTruncateOverlaps) {
                // check if this seg is marked for truncation or deletion

                CAlnMix::TTruncateDSIndexMap::iterator ds_i =
                    m_TruncateMap.find(match->m_DSIndex);
                if (ds_i != m_TruncateMap.end()) {
                    CAlnMix::TTruncateSeqPosMap::iterator pos_i =
                        ds_i->second.find(start1);
                    if (pos_i != ds_i->second.end()) {
                        CAlnMixMatch * truncated_match = pos_i->second;
#if OBJECTS_ALNMGR___ALNMIX__DBG
                        if (truncated_match->m_AlnSeq1 !=
                            match->m_AlnSeq1) {
                            NCBI_THROW(CAlnException, eMergeFailure,
                                       "CAlnMix::x_Merge(): "
                                       "Truncated refseq not consistent");
                        }
                        if (truncated_match->m_Len >= match->m_Len) {
                            NCBI_THROW(CAlnException, eMergeFailure,
                                       "CAlnMix::x_Merge(): "
                                       "truncated_match->m_Len >= match->m_Len");
                        }                            
#endif
                        // this match needs to be truncated
                        TSeqPos left_diff = 
                            truncated_match->m_Start1 - match->m_Start1;
                        TSeqPos right_diff =
                            match->m_Len - truncated_match->m_Len - left_diff;
                        match->m_Len = curr_len =
                            len = truncated_match->m_Len;
                        match->m_Start1 = start1 = 
                            truncated_match->m_Start1;
                        match->m_Start2 = start2 += match->m_StrandsDiffer ?
                            right_diff : left_diff;
                    }
                }
                
            }

            CAlnMixSeq::TStarts& starts = seq1->m_Starts;
            if (seq2) {
                // mark it, it is linked to the refseq
                seq2->m_RefBy = refseq;

                // this match is used erase from seq2 list
                if ( !first_refseq ) {
                    seq2->m_MatchList.erase(match_list_iter2);
                }
            }

            start_i = starts.end();
            lo_start_i = starts.end();
            hi_start_i = starts.end();

            if (!starts.size()) {
                // no starts exist yet

                if ( !m_IndependentDSs ) {
                    // TEMPORARY, for the single refseq version of mix,
                    // clear all MatchLists and exit
                    if ( !(m_SingleRefseq  ||  first_refseq) ) {
                        ITERATE (TSeqs, it, m_Seqs){
                            if ( !((*it)->m_MatchList.empty()) ) {
                                (*it)->m_MatchList.clear();
                            }
                        }
                        break; 
                    }
                }

                // this seq has not yet been used, set the strand
                seq1->m_PositiveStrand = ! (m_MergeFlags & fNegativeStrand);

                //create the first one
                seg = new CAlnMixSegment;
                seg->m_Len = len;
                seg->m_DSIndex = match->m_DSIndex;
                starts[start1] = seg;
                seg->m_StartIts[seq1] = 
                    lo_start_i = hi_start_i = starts.begin();
                // DONE!
            } else {
                // look ahead
                if ((lo_start_i = start_i = starts.lower_bound(start1))
                    == starts.end()  ||
                    start1 < start_i->first) {
                    // the start position does not exist
                    if (lo_start_i != starts.begin()) {
                        --lo_start_i;
                    }
                }

                if (m_MergeFlags & fTruncateOverlaps  &&
                    !(start1 == lo_start_i->first  &&
                      len == lo_start_i->second->m_Len  &&
                      match->m_DSIndex == lo_start_i->second->m_DSIndex)) {

                    TSignedSeqPos left_diff = 0;
                    if (start1 >= start_i->first) {
                        left_diff = lo_start_i->first + 
                            lo_start_i->second->m_Len -
                            start1;
                        if (left_diff < 0) {
                            left_diff = 0;
                        }
                    }

                    TSignedSeqPos right_diff
                        = (start_i == starts.end() ? 0 :
                           len - (start_i->first - start1));
                    if (right_diff < 0) {
                        right_diff = 0;
                    }
                    TSeqPos diff = left_diff + right_diff;
                    
                    if (diff) {
                        if (len - diff > 0) {
                            // Truncation
                            // x----)  x---.. becomes  x----x---x---..
                            //   x-------)
                            m_TruncateMap[match->m_DSIndex][start1] 
                                = match;
                            match->m_Start1 = start1 += left_diff;
                            if (match->m_StrandsDiffer) {
                                match->m_Start2 = start2 += right_diff;
                            } else {
                                match->m_Start2 = start2 += left_diff;
                            }
                            match->m_Len = len -= diff;
                            
                            // create the new truncated seg
                            seg = new CAlnMixSegment;
                            seg->m_Len = len;
                            seg->m_DSIndex = match->m_DSIndex;
                            starts[start1] = seg;
                            // point to the newly created start
                            seg->m_StartIts[seq1] = hi_start_i = ++lo_start_i;
                            // DONE!
                            
                        } else {
                            // Ignoring
                            // x----x---.. becomes  x----x---..
                            //   x-------)
                            m_DeleteMap[match->m_DSIndex][start1] 
                                = match;
                            break;
                        }
                    }
                }

                // look back
                if (hi_start_i == starts.end()  &&  start_i != lo_start_i) {
                    CAlnMixSegment * prev_seg = lo_start_i->second;
                    if (lo_start_i->first + prev_seg->m_Len > start1) {
                        // x----..   becomes  x-x--..
                        //   x--..
                        
                        TSeqPos len1 = start1 - lo_start_i->first;
                        TSeqPos len2 = prev_seg->m_Len - len1;
                        
                        // create the second seg
                        seg = new CAlnMixSegment;
                        seg->m_Len = len2;
                        seg->m_DSIndex = match->m_DSIndex;
                        starts[start1] = seg;
                        
                        // create rows info
                        ITERATE (CAlnMixSegment::TStartIterators, it, 
                                 prev_seg->m_StartIts) {
                            CAlnMixSeq * seq = it->first;
                            tmp_start_i = it->second;
                            if (seq->m_PositiveStrand ==
                                seq1->m_PositiveStrand) {
                                seq->m_Starts[tmp_start_i->first + len1]
                                    = seg;
                                seg->m_StartIts[seq] = ++tmp_start_i;
                            } else {
                                seq->m_Starts[tmp_start_i->first + len2]
                                    = prev_seg;
                                seq->m_Starts[tmp_start_i->first] = seg;
                                seg->m_StartIts[seq] = tmp_start_i;
                                prev_seg->m_StartIts[seq] = ++tmp_start_i;
                            }
                        }
                        
                        // truncate the first seg
                        prev_seg->m_Len = len1;
                        
                        start_i--; // point to the newly created start
                    }
                    lo_start_i++;
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
                        start += curr_len;

                        // truncate the first seg
                        prev_seg->m_Len = curr_len;
                        
                        // create rows info
                        ITERATE (CAlnMixSegment::TStartIterators, it, 
                                prev_seg->m_StartIts) {
                            CAlnMixSeq * seq = it->first;
                            tmp_start_i = it->second;
                            if (seq->m_PositiveStrand ==
                                seq1->m_PositiveStrand) {
                                seq->m_Starts[tmp_start_i->first + curr_len]
                                    = seg;
                                seg->m_StartIts[seq] = ++tmp_start_i;
                            } else{
                                seq->m_Starts[tmp_start_i->first + len1]
                                    = prev_seg;
                                seq->m_Starts[tmp_start_i->first] = seg;
                                seg->m_StartIts[seq] = tmp_start_i;
                                prev_seg->m_StartIts[seq] = ++tmp_start_i;
                            }
                        }

                        hi_start_i = start_i; // DONE!
                    } else if (curr_len == prev_seg->m_Len) {
                        // x----)
                        // x----)
                        hi_start_i = start_i; // DONE!
                    } else {
                        // x----)     becomes  x----)x--)
                        // x-------)
                        start += prev_seg->m_Len;
                        curr_len -= prev_seg->m_Len;
                        start_i++;
                    }
                } else {
                    seg = new CAlnMixSegment;
                    starts[start] = seg;
                    tmp_start_i = start_i;
                    tmp_start_i--;
                    seg->m_StartIts[seq1] = tmp_start_i;
                    if (start_i != starts.end()  &&
                        start + curr_len > start_i->first) {
                        //       x--..
                        // x--------..
                        seg->m_Len = start_i->first - start;
                        seg->m_DSIndex = match->m_DSIndex;
                    } else {
                        //       x-----)
                        // x---)
                        seg->m_Len = curr_len;
                        seg->m_DSIndex = match->m_DSIndex;
                        hi_start_i = start_i;
                        hi_start_i--; // DONE!
                    }
                    start += seg->m_Len;
                    curr_len -= seg->m_Len;
                    if (lo_start_i == start_i) {
                        lo_start_i--;
                    }
                }
            }
                 
            // try to resolve the second row
            if (seq2) {
                // save the orig seq2, since we may be modifying match below
                CAlnMixSeq * orig_seq2 = seq2;
                while (!x_SecondRowFits(match)) {
                    if (!seq2->m_ExtraRow) {
                        // create an extra row
                        CRef<CAlnMixSeq> row (new CAlnMixSeq);
                        row->m_BioseqHandle = seq2->m_BioseqHandle;
                        row->m_SeqId = seq2->m_SeqId;
                        row->m_Factor = seq2->m_Factor;
                        row->m_SeqIndex = seq2->m_SeqIndex;
                        if (m_MergeFlags & fQuerySeqMergeOnly) {
                            row->m_DSIndex = match->m_DSIndex;
                        }
                        m_ExtraRows.push_back(row);
                        seq2->m_ExtraRow = row;
                        seq2 = match->m_AlnSeq2 = seq2->m_ExtraRow;
                        break;
                    }
                    seq2 = match->m_AlnSeq2 = seq2->m_ExtraRow;
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

                while(start < start2 + len) {
                    if (start2_i != starts2.end() &&
                        start2_i->first == start) {
                        // this seg already exists
                        if (start2_i->second != start_i->second) {
                            // merge the two segments
                            ITERATE (CAlnMixSegment::TStartIterators,
                                     it, 
                                     start2_i->second->m_StartIts) {
                                CAlnMixSeq * tmp_seq = it->first;
                                tmp_start_i = it->second;
                                tmp_start_i->second = start_i->second;
                                start_i->second->m_StartIts[tmp_seq] =
                                    tmp_start_i;
                            }
                        }
                    } else {
                        seq2->m_Starts[start] = start_i->second;
                        start2_i--;
                        start_i->second->m_StartIts[seq2] = start2_i;
                    }
                    start += start_i->second->m_Len;
                    start2_i++;
                    if (match->m_StrandsDiffer) {
                        start_i--;
                    } else {
                        start_i++;
                    }
                }
                // done with this match -- restore the original seq2; 
                // (to be reused in case of multiple mixes)
                match->m_AlnSeq2 = orig_seq2;
            }
        }
        
        if (first_refseq  &&  
            match_i == m_Matches.end()  &&  m_Matches.size()  ||
            !first_refseq  &&  match_list_i == refseq->m_MatchList.end()) {

            // and move on to the next refseq
            refseq->m_RefBy = 0;

            // try to find the best scoring 'connected' candidate
            ITERATE (TSeqs, it, m_Seqs){
                if ( !((*it)->m_MatchList.empty())  &&
                     (*it)->m_RefBy == refseq) {
                    refseq = *it;
                    break;
                }
            }
            if (refseq->m_RefBy == 0) {
                // no candidate was found 'connected' to the refseq
                // continue with the highest scoring candidate
                ITERATE (TSeqs, it, m_Seqs){
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
        }
    }
    x_CreateRowsVector();
    x_CreateSegmentsVector();
    x_CreateDenseg();
}

bool CAlnMix::x_SecondRowFits(const CAlnMixMatch * match) const
{
    CAlnMixSeq::TStarts&          starts1 = match->m_AlnSeq1->m_Starts;
    CAlnMixSeq::TStarts&          starts2 = match->m_AlnSeq2->m_Starts;
    CAlnMixSeq                  * seq1    = match->m_AlnSeq1,
                                * seq2    = match->m_AlnSeq2;
    const TSeqPos&                start1  = match->m_Start1;
    const TSeqPos&                start2  = match->m_Start2;
    const TSeqPos&                len     = match->m_Len;
    CAlnMixSeq::TStarts::iterator start_i;

    // subject sequences go on separate rows if requested
    if (m_MergeFlags & fQuerySeqMergeOnly) {
        if (seq2->m_DSIndex) {
            if (seq2->m_DSIndex == match->m_DSIndex) {
                return true;
            } else {
                return false;
            }
        } else {
            seq2->m_DSIndex = match->m_DSIndex;
            return true;
        }
    }

    if ( !starts2.empty() ) {

        // check strand
        if (seq2->m_PositiveStrand !=
            (seq1->m_PositiveStrand ?
             !match->m_StrandsDiffer :
             match->m_StrandsDiffer)) {
            return false;
        }

        start_i = starts2.lower_bound(start2);

        // check below
        if (start_i != starts2.begin()) {
            start_i--;
            
            // no overlap, check for consistency
            if ( !m_IndependentDSs ) {
                CAlnMixSegment::TStartIterators::iterator start_it_i =
                    start_i->second->m_StartIts.find(seq1);
                if (start_it_i != start_i->second->m_StartIts.end()) {
                    if (match->m_StrandsDiffer) {
                        if (start_it_i->second->first < start1 + len) {
                            return false;
                        }
                    } else {
                        if (start_it_i->second->first + 
                            start_i->second->m_Len > start1) {
                            return false;
                        }
                    }
                }
            }

            // check for overlap
            if (start_i->first + start_i->second->m_Len > start2) {
                return false;
            }
            start_i++;
        }

        // check the overlap for consistency
        while (start_i != starts2.end()  &&  
               start_i->first < start2 + len) {
            if ( !m_IndependentDSs ) {
                CAlnMixSegment::TStartIterators::iterator start_it_i =
                    start_i->second->m_StartIts.find(seq1);
                if (start_it_i != start_i->second->m_StartIts.end()) {
                    if (start_it_i->second->first != start1 + 
                        (match->m_StrandsDiffer ?
                         start2 + len - 
                         (start_i->first + start_i->second->m_Len) :
                         start_i->first - start2)) {
                        return false;
                    }
                }
            }
            start_i++;
        }

        // check above for consistency
        if (start_i != starts2.end()) {
            if ( !m_IndependentDSs ) {
                CAlnMixSegment::TStartIterators::iterator start_it_i =
                    start_i->second->m_StartIts.find(seq1);
                if (start_it_i != start_i->second->m_StartIts.end()) {
                    if (match->m_StrandsDiffer) {
                        if (start_it_i->second->first + 
                            start_i->second->m_Len > start1) {
                            return false;
                        }
                    } else {
                        if (start_it_i->second->first < start1 + len) {
                            return false;
                        }
                    }
                }
            }
        }

        // check for inconsistent matches
        if ((start_i = starts1.find(start1)) == starts1.end() ||
            start_i->first != start1) {
            NCBI_THROW(CAlnException, eMergeFailure,
                       "CAlnMix::x_SecondRowFits(): "
                       "Internal error: starts1 do not match");
        } else {
            CAlnMixSegment::TStartIterators::iterator it;
            TSeqPos tmp_start =
                match->m_StrandsDiffer ? start2 + len : start2;
            while (start_i != starts1.end()  &&
                   start_i->first < start1 + len) {

                CAlnMixSegment::TStartIterators& its = 
                    start_i->second->m_StartIts;

                if (match->m_StrandsDiffer) {
                    tmp_start -= start_i->second->m_Len;
                }

                if ((it = its.find(seq2)) != its.end()) {
                    if (it->second->first != tmp_start) {
                        // found an inconsistent prev match
                        return false;
                    }
                }

                if ( !match->m_StrandsDiffer ) {
                    tmp_start += start_i->second->m_Len;
                }

                start_i++;
            }
        }
    }
    return true;
}

void CAlnMix::x_CreateRowsVector()
{
    m_Rows.clear();

    int count = 0;
    ITERATE (TSeqs, i, m_Seqs) {
        CAlnMixSeq * seq = *i;
        m_Rows.push_back(seq);
        seq->m_RowIndex = count++;
        while ( (seq = seq->m_ExtraRow) != NULL ) {
            seq->m_RowIndex = count++;
            m_Rows.push_back(seq);
        }
    }
}


void CAlnMix::x_CreateSegmentsVector()
{
    TSegmentsContainer gapped_segs;

    // init the start iterators for each row
    NON_CONST_ITERATE (TSeqs, row_i, m_Rows) {
        CAlnMixSeq * row = *row_i;
        if ( !row->m_Starts.empty() ) {
            if (row->m_PositiveStrand) {
                row->m_StartIt = row->m_Starts.begin();
            } else {
                row->m_StartIt = row->m_Starts.end();
                row->m_StartIt--;
            }
        } else {
            row->m_StartIt = row->m_Starts.end();
#if OBJECTS_ALNMGR___ALNMIX__DBG
            NCBI_THROW(CAlnException, eMergeFailure,
                       "CAlnMix::x_CreateSegmentsVector(): "
                       "Internal error: no starts for this row. ");
#endif
        }
    }

    // init the start iterators for each extra row
    NON_CONST_ITERATE (list<CRef<CAlnMixSeq> >, row_i, m_ExtraRows) {
        CAlnMixSeq * row = *row_i;
        if ( !row->m_Starts.empty() ) {
            if (row->m_PositiveStrand) {
                row->m_StartIt = row->m_Starts.begin();
            } else {
                row->m_StartIt = row->m_Starts.end();
                row->m_StartIt--;
            }
        } else {
            row->m_StartIt = row->m_Starts.end();
#if OBJECTS_ALNMGR___ALNMIX__DBG
            NCBI_THROW(CAlnException, eMergeFailure,
                       "CAlnMix::x_CreateSegmentsVector(): "
                       "Internal error: no starts for this row. ");
#endif
        }
    }

    TSeqs::iterator refseq_it = m_Rows.begin(); 
    while (true) {
        CAlnMixSeq * refseq = 0;
        while (refseq_it != m_Rows.end()) {
            refseq = *refseq_it;
            if (refseq->m_StartIt != refseq->m_Starts.end()) {
                refseq_it++;
                break;
            } else {
                refseq = 0;
            }
            refseq_it++;
        }
        if ( !refseq ) {
            break; // done
        }
        // for each refseq segment
        while (refseq->m_StartIt != refseq->m_Starts.end()) {
            stack< CRef<CAlnMixSegment> > seg_stack;
            seg_stack.push(refseq->m_StartIt->second);
            
            while ( !seg_stack.empty() ) {
                
                bool pop_seg = true;
                // check the gapped segments on the left
                ITERATE (CAlnMixSegment::TStartIterators, start_its_i,
                         seg_stack.top()->m_StartIts) {

                    CAlnMixSeq * row = start_its_i->first;

                    if (row->m_StartIt != start_its_i->second) {
                        seg_stack.push(row->m_StartIt->second);
                        pop_seg = false;
                        break;
                    }
                }

                if (pop_seg) {

                    // inc/dec iterators for each row of the seg
                    ITERATE (CAlnMixSegment::TStartIterators, start_its_i,
                             seg_stack.top()->m_StartIts) {
                        CAlnMixSeq * row = start_its_i->first;

#if OBJECTS_ALNMGR___ALNMIX__DBG
                        if (row->m_PositiveStrand  &&
                            row->m_StartIt->first > 
                            start_its_i->second->first  ||
                            !row->m_PositiveStrand  &&
                            row->m_StartIt->first <
                            start_its_i->second->first) {
                            string errstr =
                                string("CAlnMix::x_CreateSegmentsVector():")
                                + " Internal error: Integrity broken" +
                                " row=" + NStr::IntToString(row->m_RowIndex)
                                + " row->m_StartIt->first="
                                + NStr::IntToString(row->m_StartIt->first)
                                + " start_its_i->second->first=" +
                                NStr::IntToString(start_its_i->second->first)
                                + " strand=" +
                                (row->m_PositiveStrand ? "plus" : "minus");
                            NCBI_THROW(CAlnException, eMergeFailure, errstr);
                        }
#endif

                        if (row->m_PositiveStrand) {
                            row->m_StartIt++;
                        } else {
                            if (row->m_StartIt == row->m_Starts.begin()) {
                                row->m_StartIt = row->m_Starts.end();
                            } else {
                                row->m_StartIt--;
                            }
                        }
                    }

                    if (seg_stack.size() > 1) {
                        // add to the gapped segments
                        gapped_segs.push_back(seg_stack.top());
                        seg_stack.pop();
                    } else {
                        // add the gapped segments if any
                        if (gapped_segs.size()) {
                            if (m_MergeFlags & fGapJoin) {
                                // request to try to align
                                // gapped segments w/ equal len
                                x_ConsolidateGaps(gapped_segs);
                            } else if (m_MergeFlags & fMinGap) {
                                // request to try to align 
                                // all gapped segments
                                x_MinimizeGaps(gapped_segs);
                            }
                            NON_CONST_ITERATE (TSegmentsContainer,
                                               seg_i, gapped_segs) {
                                m_Segments.push_back(&**seg_i);
                            }
                            gapped_segs.clear();
                        }
                        // add the refseq segment
                        m_Segments.push_back(seg_stack.top());
                        seg_stack.pop();
                    }
                }
            }
        }
    }
}


void CAlnMix::x_ConsolidateGaps(TSegmentsContainer& gapped_segs)
{
    TSegmentsContainer::iterator seg1_i, seg2_i;

    seg2_i = seg1_i = gapped_segs.begin();
    seg2_i++;

    bool         cache = false;
    string       s1;
    TSeqPos      start1;
    int          score1;
    CAlnMixSeq * seq1;
    CAlnMixSeq * seq2;

    while (seg2_i != gapped_segs.end()) {

        CAlnMixSegment * seg1 = *seg1_i;
        CAlnMixSegment * seg2 = *seg2_i;

        // check if this seg possibly aligns with the previous one
        bool possible = true;
            
        if (seg2->m_Len == seg1->m_Len  && 
            seg2->m_StartIts.size() == 1) {

            seq2 = seg2->m_StartIts.begin()->first;

            // check if this seq was already used
            ITERATE (CAlnMixSegment::TStartIterators,
                     st_it,
                     (*seg1_i)->m_StartIts) {
                if (st_it->first == seq2) {
                    possible = false;
                    break;
                }
            }

            // check if score is sufficient
            if (possible  &&  m_AddFlags & fCalcScore) {
                if (!cache) {

                    seq1 = seg1->m_StartIts.begin()->first;
                    
                    start1 = seg1->m_StartIts[seq1]->first;
                        
                    if (seq1->m_PositiveStrand) {
                        seq1->m_BioseqHandle->GetSeqVector
                            (CBioseq_Handle::eCoding_Iupac,
                             CBioseq_Handle::eStrand_Plus).
                            GetSeqData(start1, start1 + seg1->m_Len, s1);
                    } else {
                        CSeqVector seq_vec = 
                            seq1->m_BioseqHandle->GetSeqVector
                            (CBioseq_Handle::eCoding_Iupac,
                             CBioseq_Handle::eStrand_Minus);
                        TSeqPos size = seq_vec.size();
                        seq_vec.GetSeqData(size - (start1 + seg1->m_Len),
                                           size - start1, 
                                           s1);
                    }                                

                    score1 = 
                        CAlnVec::CalculateScore(s1, s1,
                                                seq1->m_IsAA,
                                                seq1->m_IsAA);
                    cache = true;
                }
                
                string s2;

                TSeqPos start2 = seg2->m_StartIts[seq2]->first;
                            
                if (seq2->m_PositiveStrand) {
                    seq2->m_BioseqHandle->GetSeqVector
                        (CBioseq_Handle::eCoding_Iupac,
                         CBioseq_Handle::eStrand_Plus).
                        GetSeqData(start2, start2 + seg2->m_Len, s2);
                } else {
                    CSeqVector seq_vec = 
                        seq2->m_BioseqHandle->GetSeqVector
                        (CBioseq_Handle::eCoding_Iupac,
                         CBioseq_Handle::eStrand_Minus);
                    TSeqPos size = seq_vec.size();
                    seq_vec.GetSeqData(size - (start2 + seg2->m_Len),
                                       size - start2, 
                                       s2);
                }                                

                int score2 = 
                    CAlnVec::CalculateScore(s1, s2, seq1->m_IsAA, seq2->m_IsAA);

                if (score2 < 75 * score1 / 100) {
                    possible = false;
                }
            }
            
        } else {
            possible = false;
        }

        if (possible) {
            // consolidate the ones so far
            
            // add the new row
            seg1->m_StartIts[seq2] = seg2->m_StartIts.begin()->second;
            
            // point the row's start position to the beginning seg
            seg2->m_StartIts.begin()->second->second = seg1;
            
            seg2_i = gapped_segs.erase(seg2_i);
        } else {
            cache = false;
            seg1_i++;
            seg2_i++;
        }
    }
}


void CAlnMix::x_MinimizeGaps(TSegmentsContainer& gapped_segs)
{
    TSegmentsContainer::iterator  seg_i, seg_i_end, seg_i_begin;
    CAlnMixSegment       * seg1, * seg2;
    CRef<CAlnMixSegment> seg;
    CAlnMixSeq           * seq;
    TSegmentsContainer            new_segs;

    seg_i_begin = seg_i_end = seg_i = gapped_segs.begin();

    typedef map<TSeqPos, CRef<CAlnMixSegment> > TLenMap;
    TLenMap len_map;

    while (seg_i_end != gapped_segs.end()) {

        len_map[(*seg_i_end)->m_Len];
        
        // check if this seg can possibly be minimized
        bool possible = true;

        seg_i = seg_i_begin;
        while (seg_i != seg_i_end) {
            seg1 = *seg_i;
            seg2 = *seg_i_end;
            
            ITERATE (CAlnMixSegment::TStartIterators,
                     st_it,
                     seg2->m_StartIts) {
                seq = st_it->first;
                // check if this seq was already used
                if (seg1->m_StartIts.find(seq) != seg1->m_StartIts.end()) {
                    possible = false;
                    break;
                }
            }
            if ( !possible ) {
                break;
            }
            seg_i++;
        }
        seg_i_end++;

        if ( !possible  ||  seg_i_end == gapped_segs.end()) {
            // use the accumulated len info to create the new segs

            // create new segs with appropriate len
            TSeqPos len_so_far = 0;
            TLenMap::iterator len_i = len_map.begin();
            while (len_i != len_map.end()) {
                len_i->second = new CAlnMixSegment();
                len_i->second->m_Len = len_i->first - len_so_far;
                len_so_far += len_i->second->m_Len;
                len_i++;
            }
                
            // loop through the accumulated orig segs.
            // copy info from them into the new segs
            TLenMap::iterator len_i_end;
            seg_i = seg_i_begin;
            while (seg_i != seg_i_end) {
                TSeqPos orig_len = (*seg_i)->m_Len;

                // determine the span of the current seg
                len_i_end = len_map.find(orig_len);
                len_i_end++;

                // loop through its sequences
                NON_CONST_ITERATE (CAlnMixSegment::TStartIterators,
                                   st_it,
                                   (*seg_i)->m_StartIts) {

                    seq = st_it->first;
                    TSeqPos orig_start = st_it->second->first;

                    len_i = len_map.begin();
                    len_so_far = 0;
                    // loop through the new segs
                    while (len_i != len_i_end) {
                        seg = len_i->second;
                    
                        // calc the start
                        TSeqPos this_start = orig_start + 
                            (seq->m_PositiveStrand ? 
                             len_so_far :
                             orig_len - len_so_far - seg->m_Len);

                        // create the bindings:
                        seq->m_Starts[this_start] = seg;
                        seg->m_StartIts[seq] = seq->m_Starts.find(this_start);
                        len_i++;
                        len_so_far += seg->m_Len;
                    }
                }
                seg_i++;
            }
            NON_CONST_ITERATE (TLenMap, len_it, len_map) {
                new_segs.push_back(len_it->second);
            }
            len_map.clear();
            seg_i_begin = seg_i_end;
        }
    }
    gapped_segs.clear();
    ITERATE (TSegmentsContainer, new_seg_i, new_segs) {
        gapped_segs.push_back(*new_seg_i);
    }
}


void CAlnMix::x_CreateDenseg()
{
    int numrow  = 0,
        numrows = m_Rows.size();
    int numseg  = 0,
        numsegs = m_Segments.size();
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
    for (numrow = 0;  numrow < numrows;  numrow++) {
        ids[numrow] = m_Rows[numrow]->m_SeqId;
    }

    int offset = 0;
    for (numseg = 0;  numseg < numsegs;  numseg++, offset += numrows) {
        // lens
        lens[numseg] = m_Segments[numseg]->m_Len;

        // starts
        ITERATE (CAlnMixSegment::TStartIterators, start_its_i,
                m_Segments[numseg]->m_StartIts) {
            starts[offset + start_its_i->first->m_RowIndex] =
                start_its_i->second->first;
        }

        // strands
        for (numrow = 0;  numrow < numrows;  numrow++) {
            if (m_Rows[numrow]->m_PositiveStrand) {
                strands[offset + numrow] = eNa_strand_plus;
            }
        }
    }
#if OBJECTS_ALNMGR___ALNMIX__DBG
    offset = 0;
    for (numrow = 0;  numrow < numrows;  numrow++) {
        TSignedSeqPos max_start = -1, start;
        bool plus = strands[numrow] == eNa_strand_plus;

        if (plus) {
            offset = 0;
        } else {
            offset = (numsegs -1) * numrows;
        }

        for (numseg = 0;  numseg < numsegs;  numseg++) {
            start = starts[offset + numrow];
            if (start >= 0) {
                if (start < max_start) {
                    string errstr = string("CAlnMix::x_CreateDenseg():")
                        + " Starts are not consistent!"
                        + " Row=" + NStr::IntToString(numrow) +
                        " Seg=" + NStr::IntToString(plus ? numseg :
                                                    numsegs - 1 - numseg) +
                        " MaxStart=" + NStr::IntToString(max_start) +
                        " Start=" + NStr::IntToString(start);
                    
                    NCBI_THROW(CAlnException, eMergeFailure, errstr);
                }
                max_start = start + 
                  lens[plus ? numseg : numsegs - 1 - numseg];
            }
            if (strands[numrow] == eNa_strand_plus) {
                offset += numrows;
            } else {
                offset -= numrows;
            }
        }
    }
#endif
}


END_objects_SCOPE // namespace ncbi::objects::
END_NCBI_SCOPE

/*
* ===========================================================================
*
* $Log$
* Revision 1.59  2003/06/25 15:17:31  todorov
* truncation consistent for the whole segment now
*
* Revision 1.58  2003/06/24 15:24:14  todorov
* added optional truncation of overlaps
*
* Revision 1.57  2003/06/20 03:06:39  todorov
* Setting the seqalign type
*
* Revision 1.56  2003/06/19 18:37:19  todorov
* typo fixed
*
* Revision 1.55  2003/06/09 20:54:20  todorov
* Use of ObjMgr is now optional
*
* Revision 1.54  2003/06/03 20:56:52  todorov
* Bioseq handle validation
*
* Revision 1.53  2003/06/03 16:07:05  todorov
* Fixed overlap consistency check in case strands differ
*
* Revision 1.52  2003/06/03 14:38:26  todorov
* warning fixed
*
* Revision 1.51  2003/06/02 17:39:40  todorov
* Changes in rev 1.49 were lost. Reapplying them
*
* Revision 1.50  2003/06/02 16:06:40  dicuccio
* Rearranged src/objects/ subtree.  This includes the following shifts:
*     - src/objects/asn2asn --> arc/app/asn2asn
*     - src/objects/testmedline --> src/objects/ncbimime/test
*     - src/objects/objmgr --> src/objmgr
*     - src/objects/util --> src/objmgr/util
*     - src/objects/alnmgr --> src/objtools/alnmgr
*     - src/objects/flat --> src/objtools/flat
*     - src/objects/validator --> src/objtools/validator
*     - src/objects/cddalignview --> src/objtools/cddalignview
* In addition, libseq now includes six of the objects/seq... libs, and libmmdb
* replaces the three libmmdb? libs.
*
* Revision 1.49  2003/05/30 17:42:03  todorov
* 1) Bug fix in x_SecondRowFits
* 2) x_CreateSegmentsVector now uses a stack to order the segs
*
* Revision 1.48  2003/05/23 18:11:42  todorov
* More detailed exception txt
*
* Revision 1.47  2003/05/20 21:20:59  todorov
* mingap minus strand multiple segs bug fix
*
* Revision 1.46  2003/05/15 23:31:26  todorov
* Minimize gaps bug fix
*
* Revision 1.45  2003/05/09 16:41:27  todorov
* Optional mixing of the query sequence only
*
* Revision 1.44  2003/04/24 16:15:57  vasilche
* Added missing includes and forward class declarations.
*
* Revision 1.43  2003/04/15 14:21:12  vasilche
* Fixed order of member initializers.
*
* Revision 1.42  2003/04/14 18:03:19  todorov
* reuse of matches bug fix
*
* Revision 1.41  2003/04/01 20:25:31  todorov
* fixed iterator-- behaviour at the begin()
*
* Revision 1.40  2003/03/28 16:47:28  todorov
* Introduced TAddFlags (fCalcScore for now)
*
* Revision 1.39  2003/03/26 16:38:24  todorov
* mix independent densegs
*
* Revision 1.38  2003/03/18 17:16:05  todorov
* multirow inserts bug fix
*
* Revision 1.37  2003/03/12 15:39:47  kuznets
* iterate -> ITERATE
*
* Revision 1.36  2003/03/10 22:12:02  todorov
* fixed x_CompareAlnMatchScores callback
*
* Revision 1.35  2003/03/06 21:42:38  todorov
* bug fix in x_Reset()
*
* Revision 1.34  2003/03/05 21:53:24  todorov
* clean up miltiple mix case
*
* Revision 1.33  2003/03/05 17:42:41  todorov
* Allowing multiple mixes + general case speed optimization
*
* Revision 1.32  2003/03/05 16:18:17  todorov
* + str len err check
*
* Revision 1.31  2003/02/24 19:01:31  vasilche
* Use faster version of CSeq_id::Assign().
*
* Revision 1.30  2003/02/24 18:46:38  todorov
* Fixed a - strand row info creation bug
*
* Revision 1.29  2003/02/11 21:32:44  todorov
* fMinGap optional merging algorithm
*
* Revision 1.28  2003/02/04 00:05:16  todorov
* GetSeqData neg strand range bug
*
* Revision 1.27  2003/01/27 22:30:30  todorov
* Attune to seq_vector interface change
*
* Revision 1.26  2003/01/23 16:30:18  todorov
* Moved calc score to alnvec
*
* Revision 1.25  2003/01/22 21:00:21  dicuccio
* Fixed compile error - transposed #endif and }
*
* Revision 1.24  2003/01/22 19:39:13  todorov
* 1) Matches w/ the 1st non-gapped row added only; the rest used to calc seqs'
* score only
* 2) Fixed a couple of potential problems
*
* Revision 1.23  2003/01/17 18:56:26  todorov
* Perform merge algorithm even if only one input denseg
*
* Revision 1.22  2003/01/16 17:40:19  todorov
* Fixed strand param when calling GetSeqVector
*
* Revision 1.21  2003/01/10 17:37:28  todorov
* Fixed a bug in fNegativeStrand
*
* Revision 1.20  2003/01/10 15:12:13  dicuccio
* Ooops.  Methinks I should read my e-mail more thoroughly - thats '!= NULL', not
* '== NULL'.
*
* Revision 1.19  2003/01/10 15:11:12  dicuccio
* Small bug fixes: changed 'iterate' -> 'non_const_iterate' in x_Merge(); changed
* logic of while() in x_CreateRowsVector() to avoid compiler warning.
*
* Revision 1.18  2003/01/10 00:42:53  todorov
* Optional sorting of seqs by score
*
* Revision 1.17  2003/01/10 00:11:37  todorov
* Implemented fNegativeStrand
*
* Revision 1.16  2003/01/07 17:23:49  todorov
* Added conditionally compiled validation code
*
* Revision 1.15  2003/01/02 20:03:48  todorov
* Row strand init & check
*
* Revision 1.14  2003/01/02 16:40:11  todorov
* Added accessors to the input data
*
* Revision 1.13  2003/01/02 15:30:17  todorov
* Fixed the order of checks in x_SecondRowFits
*
* Revision 1.12  2002/12/30 20:55:47  todorov
* Added range fitting validation to x_SecondRowFits
*
* Revision 1.11  2002/12/30 18:08:39  todorov
* 1) Initialized extra rows' m_StartIt
* 2) Fixed a bug in x_SecondRowFits
*
* Revision 1.10  2002/12/27 23:09:56  todorov
* Additional inconsistency checks in x_SecondRowFits
*
* Revision 1.9  2002/12/27 17:27:13  todorov
* Force positive strand in all cases but negative
*
* Revision 1.8  2002/12/27 16:39:13  todorov
* Fixed a bug in the single Dense-seg case.
*
* Revision 1.7  2002/12/23 18:03:51  todorov
* Support for putting in and getting out Seq-aligns
*
* Revision 1.6  2002/12/19 00:09:23  todorov
* Added optional consolidation of segments that are gapped on the query.
*
* Revision 1.5  2002/12/18 18:58:17  ucko
* Tweak syntax to avoid confusing MSVC.
*
* Revision 1.4  2002/12/18 03:46:00  todorov
* created an algorithm for mixing alignments that share a single sequence.
*
* Revision 1.3  2002/10/25 20:02:41  todorov
* new fTryOtherMethodOnFail flag
*
* Revision 1.2  2002/10/24 21:29:13  todorov
* adding Dense-segs instead of Seq-aligns
*
* Revision 1.1  2002/08/23 14:43:52  ucko
* Add the new C++ alignment manager to the public tree (thanks, Kamen!)
*
*
* ===========================================================================
*/
