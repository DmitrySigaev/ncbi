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

#include <objects/alnmgr/alnmix.hpp>

// Object Manager includes
#include <objects/objmgr/gbloader.hpp>
#include <objects/objmgr/object_manager.hpp>
#include <objects/objmgr/reader_id1.hpp>
#include <objects/objmgr/scope.hpp>

#include <algorithm>

BEGIN_NCBI_SCOPE
BEGIN_objects_SCOPE // namespace ncbi::objects::


CAlnMix::CAlnMix(void)
    : m_SingleRefseq(false)
{
    x_CreateScope();
    x_InitBlosum62Map();
}


CAlnMix::CAlnMix(CScope& scope)
    : m_Scope(&scope),
      m_SingleRefseq(false)
{
    x_InitBlosum62Map();
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
bool CAlnMix::x_CompareAlnMatchScores(const CAlnMixMatch* aln_match1, 
                                      const CAlnMixMatch* aln_match2) 
{
    return aln_match1->m_Score > aln_match2->m_Score;
}


inline
bool CAlnMix::x_CompareAlnSegIndexes(const CAlnMixSegment* aln_seg1,
                                     const CAlnMixSegment* aln_seg2) 
{
    return
        aln_seg1->m_Index1 < aln_seg2->m_Index1 ||
        aln_seg1->m_Index1 == aln_seg2->m_Index1 &&
        aln_seg1->m_Index2 < aln_seg2->m_Index2;
        
}


void CAlnMix::x_Reset()
{
    if (m_DS) {
        m_DS->Reset();
    }
    if (m_Aln) {
        m_Aln->Reset();
    }
    m_Segments.clear();
    m_Rows.clear();
    m_ExtraRows.clear();
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
            m_DS = null;
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

#define BLOSUMSIZE 24

static const char Blosum62Fields[BLOSUMSIZE] =
    { 'A', 'R', 'N', 'D', 'C', 'Q', 'E', 'G', 'H', 'I', 'L', 'K',
      'M', 'F', 'P', 'S', 'T', 'W', 'Y', 'V', 'B', 'Z', 'X', '*' };

static const char Blosum62Matrix[BLOSUMSIZE][BLOSUMSIZE] = {
    /*       A,  R,  N,  D,  C,  Q,  E,  G,  H,  I,  L,  K,
             M,  F,  P,  S,  T,  W,  Y,  V,  B,  Z,  X,  * */
    /*A*/ {  4, -1, -2, -2,  0, -1, -1,  0, -2, -1, -1, -1,
            -1, -2, -1,  1,  0, -3, -2,  0, -2, -1,  0, -4 },
    /*R*/ { -1,  5,  0, -2, -3,  1,  0, -2,  0, -3, -2,  2,
            -1, -3, -2, -1, -1, -3, -2, -3, -1,  0, -1, -4 },
    /*N*/ { -2,  0,  6,  1, -3,  0,  0,  0,  1, -3, -3,  0,
            -2, -3, -2,  1,  0, -4, -2, -3,  3,  0, -1, -4 },
    /*D*/ { -2, -2,  1,  6, -3,  0,  2, -1, -1, -3, -4, -1,
            -3, -3, -1,  0, -1, -4, -3, -3,  4,  1, -1, -4 },
    /*C*/ {  0, -3, -3, -3,  9, -3, -4, -3, -3, -1, -1, -3,
            -1, -2, -3, -1, -1, -2, -2, -1, -3, -3, -2, -4 },
    /*Q*/ { -1,  1,  0,  0, -3,  5,  2, -2,  0, -3, -2,  1,
             0, -3, -1,  0, -1, -2, -1, -2,  0,  3, -1, -4 },
    /*E*/ { -1,  0,  0,  2, -4,  2,  5, -2,  0, -3, -3,  1,
            -2, -3, -1,  0, -1, -3, -2, -2,  1,  4, -1, -4 },
    /*G*/ {  0, -2,  0, -1, -3, -2, -2,  6, -2, -4, -4, -2,
            -3, -3, -2,  0, -2, -2, -3, -3, -1, -2, -1, -4 },
    /*H*/ { -2,  0,  1, -1, -3,  0,  0, -2,  8, -3, -3, -1,
            -2, -1, -2, -1, -2, -2,  2, -3,  0,  0, -1, -4 },
    /*I*/ { -1, -3, -3, -3, -1, -3, -3, -4, -3,  4,  2, -3,
             1,  0, -3, -2, -1, -3, -1,  3, -3, -3, -1, -4 },
    /*L*/ { -1, -2, -3, -4, -1, -2, -3, -4, -3,  2,  4, -2,
             2,  0, -3, -2, -1, -2, -1,  1, -4, -3, -1, -4 },
    /*K*/ { -1,  2,  0, -1, -3,  1,  1, -2, -1, -3, -2,  5,
            -1, -3, -1,  0, -1, -3, -2, -2,  0,  1, -1, -4 },
    /*M*/ { -1, -1, -2, -3, -1,  0, -2, -3, -2,  1,  2, -1,
             5,  0, -2, -1, -1, -1, -1,  1, -3, -1, -1, -4 },
    /*F*/ { -2, -3, -3, -3, -2, -3, -3, -3, -1,  0,  0, -3,
             0,  6, -4, -2, -2,  1,  3, -1, -3, -3, -1, -4 },
    /*P*/ { -1, -2, -2, -1, -3, -1, -1, -2, -2, -3, -3, -1,
            -2, -4,  7, -1, -1, -4, -3, -2, -2, -1, -2, -4 },
    /*S*/ {  1, -1,  1,  0, -1,  0,  0,  0, -1, -2, -2,  0,
            -1, -2, -1,  4,  1, -3, -2, -2,  0,  0,  0, -4 },
    /*T*/ {  0, -1,  0, -1, -1, -1, -1, -2, -2, -1, -1, -1,
            -1, -2, -1,  1,  5, -2, -2,  0, -1, -1,  0, -4 },
    /*W*/ { -3, -3, -4, -4, -2, -2, -3, -2, -2, -3, -2, -3,
            -1,  1, -4, -3, -2, 11,  2, -3, -4, -3, -2, -4 },
    /*Y*/ { -2, -2, -2, -3, -2, -1, -2, -3,  2, -1, -1, -2,
            -1,  3, -3, -2, -2,  2,  7, -1, -3, -2, -1, -4 },
    /*V*/ {  0, -3, -3, -3, -1, -2, -2, -3, -3,  3,  1, -2,
             1, -1, -2, -2,  0, -3, -1,  4, -3, -2, -1, -4 },
    /*B*/ { -2, -1,  3,  4, -3,  0,  1, -1,  0, -3, -4,  0,
            -3, -3, -2,  0, -1, -4, -3, -3,  4,  1, -1, -4 },
    /*Z*/ { -1,  0,  0,  1, -3,  3,  4, -2,  0, -3, -3,  1,
            -1, -3, -1,  0, -1, -3, -2, -2,  1,  4, -1, -4 },
    /*X*/ {  0, -1, -1, -1, -2, -1, -1, -1, -1, -1, -1, -1,
            -1, -1, -2,  0,  0, -2, -1, -1, -1, -1, -1, -4 },
    /***/ { -4, -4, -4, -4, -4, -4, -4, -4, -4, -4, -4, -4,
            -4, -4, -4, -4, -4, -4, -4, -4, -4, -4, -4,  1 }
};

void CAlnMix::x_InitBlosum62Map()
{
    for (int row=0; row<BLOSUMSIZE; row++) {
        for (int col=0; col<BLOSUMSIZE; col++) {
            m_Blosum62Map[Blosum62Fields[row]][Blosum62Fields[col]] =
                Blosum62Matrix[row][col];
        }
    }
}

void CAlnMix::Add(const CDense_seg &ds)
{
    if (m_InputDSsMap.find((void *)&ds) != m_InputDSsMap.end()) {
        return; // it has already been added
    }
    x_Reset();

    m_InputDSsMap[(void *)&ds] = &ds;
    m_InputDSs.push_back(CConstRef<CDense_seg>(&ds));

    vector<CRef<CAlnMixSeq> > ds_seq;

    //store the bioseq handles
    for (CAlnMap::TNumrow row = 0;  row < ds.GetDim();  row++) {

        CBioseq_Handle bioseq_handle = 
            GetScope().GetBioseqHandle(*(ds.GetIds())[row]);

        CRef<CAlnMixSeq> aln_seq;

        TBioseqHandleMap::iterator it = m_BioseqHandles.find(bioseq_handle);
        if (it == m_BioseqHandles.end()) {
            // add this bioseq handle
            aln_seq = new CAlnMixSeq();
            m_BioseqHandles[bioseq_handle] = aln_seq;
            aln_seq->m_BioseqHandle = 
                &m_BioseqHandles.find(bioseq_handle)->first;
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
                        {{
                            // calc the score by seq comp
                            string s1, s2;

                            aln_seq1->m_BioseqHandle->GetSeqVector
                                (CBioseq_Handle::eCoding_Iupac,
                                 strand1 == eNa_strand_minus ?
                                 CBioseq_Handle::eStrand_Minus :
                                 CBioseq_Handle::eStrand_Plus).
                                GetSeqData(start1, start1 + len, s1);
                            
                            aln_seq2->m_BioseqHandle->GetSeqVector
                                (CBioseq_Handle::eCoding_Iupac,
                                 strand2 == eNa_strand_minus ?
                                 CBioseq_Handle::eStrand_Minus :
                                 CBioseq_Handle::eStrand_Plus).
                                GetSeqData(start2, start2 + len, s2);


                            match->m_Score = 
                                x_CalculateScore
                                (s1, s2, aln_seq1->m_IsAA, aln_seq2->m_IsAA);
                        }}
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
                    m_Matches.push_back(match);
                }
            }
        }
        seg_off += ds.GetDim();
    }
}


int CAlnMix::x_CalculateScore(const string& s1, const string& s2,
                              bool s1_is_prot, bool s2_is_prot)
{
    int score = 0;

    string::const_iterator res1 = s1.begin();
    string::const_iterator res2 = s2.begin();

    if (s1_is_prot  &&  s2_is_prot) {
        // use BLOSUM62 matrix
        for ( ;  res1 != s1.end();  res1++, res2++) {
            score += m_Blosum62Map[*res1][*res2];
        }
    } else if ( !s1_is_prot  &&  !s2_is_prot ) {
        // use match score/mismatch penalty
        for ( ; res1 != s1.end();  res1++, res2++) {
            if (*res1 && *res2) {
                if (*res1 == *res2) {
                    score += 1;
                } else {
                    score -= 3;
                }
            }
        }
    } else {
        NCBI_THROW(CAlnException, eMergeFailure,
                   "CAlnMix::x_CalculateScore(): "
                   "Mixing prot and nucl not implemented yet.");
    }
    return score;
}


void CAlnMix::x_Merge()
{
    // Find the refseq (if such exists)
    {{
        TSeqs::iterator refseq_it;
        non_const_iterate (TSeqs, it, m_Seqs){
            CAlnMixSeq * aln_seq = *it;

            if (aln_seq->m_DS_Count == m_InputDSs.size()) {
                // found a potential reference sequence
                if ( !m_SingleRefseq  ||
                     m_SingleRefseq  &&  
                     aln_seq->m_Score > (*refseq_it)->m_Score) {
                    // it has the best score (so far)
                    m_SingleRefseq = true;
                    refseq_it = it;
                }
            }
        }
        if (m_SingleRefseq  &&  refseq_it != m_Seqs.begin()) {
            // move to the front
            CAlnMixSeq * refseq = *refseq_it;
            m_Seqs.erase(refseq_it);
            m_Seqs.insert(m_Seqs.begin(), refseq);
        }
    }}

    if (m_MergeFlags & fSortSeqsByScore) {
        // Sort sequences by score,
        // leaving the reference sequence (if such exists) on
        stable_sort(m_Seqs.begin() + (m_SingleRefseq ? 1 : 0),
                    m_Seqs.end(),
                    x_CompareAlnSeqScores);
    }

    // Index the sequences
    {{
        int seq_idx=0;
        iterate (TSeqs, seq_i, m_Seqs) {
            (*seq_i)->m_SeqIndex = seq_idx++;
        }
    }}

    // Sort matches by score
    stable_sort(m_Matches.begin(), m_Matches.end(), x_CompareAlnMatchScores);

    CAlnMixSeq * refseq = 0, * seq1 = 0, * seq2 = 0;
    TSeqPos start, start1, start2, len, curr_len;
    CAlnMixMatch * match;

    refseq = *(m_Seqs.begin());
    refseq->m_PositiveStrand = ! (m_MergeFlags & fNegativeStrand);
    TMatches::iterator match_i = m_Matches.begin();

    CRef<CAlnMixSegment> seg;
    CAlnMixSeq::TStarts::iterator start_i, lo_start_i, hi_start_i, tmp_start_i;

    while (m_Matches.size()) {
        match = *match_i;

        curr_len = len = match->m_Len;

        // is it a match_i with this refseq?
        if (match->m_AlnSeq1 == refseq) {
            seq1 = match->m_AlnSeq1;
            start1 = match->m_Start1;
            seq2 = match->m_AlnSeq2;
            start2 = match->m_Start2;
        } else if (match->m_AlnSeq2 == refseq) {
            seq1 = match->m_AlnSeq2;
            start1 = match->m_Start2;
            seq2 = match->m_AlnSeq1;
            start2 = match->m_Start1;
        } else {
            seq1 = match->m_AlnSeq1;
            seq2 = match->m_AlnSeq2;
            // mark the two refseqs, they are candidates for next refseq(s)
            seq1->m_RefseqCandidate = true;
            if (seq2) {
                seq2->m_RefseqCandidate = true;
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

            CAlnMixSeq::TStarts& starts = refseq->m_Starts;
            if (seq2) {
                // mark it, it is linked to the refseq
                seq2->m_RefBy = refseq;
            }

            start_i = starts.end();
            lo_start_i = starts.end();
            hi_start_i = starts.end();

            if (!starts.size()) {
                // no starts exist yet, create the first one
                seg = new CAlnMixSegment;
                seg->m_Len = len;
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

                // look back
                if (start_i != lo_start_i) {
                    CAlnMixSegment * prev_seg = lo_start_i->second;
                    if (lo_start_i->first + prev_seg->m_Len > start1) {
                        // x----..   becomes  x-x--..
                        //   x--..
                            
                        TSeqPos len1 = start1 - lo_start_i->first;
                        TSeqPos len2 = prev_seg->m_Len - len1;

                        // create the second seg
                        seg = new CAlnMixSegment;
                        seg->m_Len = len2;
                        starts[start1] = seg;

                        // create rows info
                        iterate (CAlnMixSegment::TStartIterators, it, 
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
                        iterate (CAlnMixSegment::TStartIterators, it, 
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
                    } else {
                        //       x-----)
                        // x---)
                        seg->m_Len = curr_len;
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
                while (!x_SecondRowFits(match)) {
                    if (!seq2->m_ExtraRow) {
                        // create an extra row
                        CRef<CAlnMixSeq> row (new CAlnMixSeq);
                        row->m_BioseqHandle = seq2->m_BioseqHandle;
                        row->m_Factor = seq2->m_Factor;
                        row->m_PositiveStrand = seq2->m_PositiveStrand;
                        row->m_SeqIndex = seq2->m_SeqIndex;
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
                            NCBI_THROW(CAlnException, eMergeFailure,
                                       "CAlnMix::Merge(): "
                                       "Problem mixing second row");
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
            }

            // match was used, remove it
            match_i = m_Matches.erase(match_i, match_i+1);
        } else {
            match_i++;
        }
        
        if (match_i == m_Matches.end()  &&  m_Matches.size()) {
            //////////////////
            // TEMPORARY, for the single refseq version
            if (!m_SingleRefseq) {
                m_Matches.clear();
            }
            //////////////////

            // rewind matches
            match_i = m_Matches.begin();

            // and move on to the next refseq
            refseq->m_RefBy = 0;
            refseq->m_RefseqCandidate = false;

            // try to find the best scoring 'connected' candidate
            iterate (TSeqs, it, m_Seqs){
                if ((*it)->m_RefseqCandidate  &&
                    (*it)->m_RefBy == refseq) {
                    refseq = *it;
                    break;
                }
            }
            if (refseq->m_RefBy == 0) {
                // no candidate was found 'connected' to the refseq
                // continue with the highest scoring candidate
                iterate (TSeqs, it, m_Seqs){
                    if ((*it)->m_RefseqCandidate) {
                        refseq = *it;
                        break;
                    }
                }
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
            if (match->m_StrandsDiffer) {
                if (start_i->second->m_StartIts[seq1]->first <
                    start1 + len) {
                    return false;
                }
            } else {
                if (start_i->second->m_StartIts[seq1]->first + 
                    start_i->second->m_Len > start1) {
                    return false;
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
            if (start_i->second->m_StartIts[seq1]->first != start1 + 
                (match->m_StrandsDiffer ?
                 start2 + len - start_i->first :
                 start_i->first - start2)) {
                return false;
            }
            start_i++;
        }

        // check above for consistency
        if (start_i != starts2.end()) {
            if (match->m_StrandsDiffer) {
                if (start_i->second->m_StartIts[seq1]->first + 
                    start_i->second->m_Len > start1) {
                    return false;
                }
            } else {
                if (start_i->second->m_StartIts[seq1]->first < 
                    start1 + len) {
                    return false;
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
    iterate (TSeqs, i, m_Seqs) {
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
    TSegments gapped_segs;

    // init the start iterators for each row
    non_const_iterate (TSeqs, row_i, m_Rows) {
        CAlnMixSeq * row = *row_i;
        if (row->m_Starts.size()) {
            if (row->m_PositiveStrand) {
                row->m_StartIt = row->m_Starts.begin();
            } else {
                row->m_StartIt = row->m_Starts.end();
                row->m_StartIt--;
            }
        } else {
#if OBJECTS_ALNMGR___ALNMIX__DBG
            NCBI_THROW(CAlnException, eMergeFailure,
                       "CAlnMix::x_CreateSegmentsVector(): "
                       "Internal error: no starts for this row. ");
        }
#endif
    }

    // init the start iterators for each extra row
    non_const_iterate (list<CRef<CAlnMixSeq> >, row_i, m_ExtraRows) {
        CAlnMixSeq * row = *row_i;
        if (row->m_PositiveStrand) {
            row->m_StartIt = row->m_Starts.begin();
        } else {
            row->m_StartIt = row->m_Starts.end();
            row->m_StartIt--;
        }
    }

    CAlnMixSeq * refseq = (*m_Rows.begin()); 
    // for each refseq segment
    CAlnMixSeq::TStarts::iterator refseq_start_i;
    if (refseq->m_PositiveStrand) {
        refseq_start_i = refseq->m_Starts.begin();
    } else {
        refseq_start_i = refseq->m_Starts.end();
        refseq_start_i--;
    }
    while (refseq_start_i != refseq->m_Starts.end()) {
        CAlnMixSegment * refseq_seg = refseq_start_i->second;

        // check the gapped segments on the left
        iterate (CAlnMixSegment::TStartIterators, start_its_i,
                refseq_seg->m_StartIts) {
            CAlnMixSeq * row = start_its_i->first;
            if (row == refseq) {
                continue;
            }
            int index = 0;
            while (row->m_StartIt != start_its_i->second) {
#if OBJECTS_ALNMGR___ALNMIX__DBG
                if (row->m_PositiveStrand  &&
                    row->m_StartIt->first > start_its_i->second->first  ||
                    !row->m_PositiveStrand  &&
                    row->m_StartIt->first < start_its_i->second->first) {
                    NCBI_THROW(CAlnException, eMergeFailure,
                               "CAlnMix::x_CreateSegmentsVector(): "
                               "Internal error: Integrity broken");
                }
#endif
                // index the segment
                CAlnMixSegment * seg = row->m_StartIt->second;
                seg->m_Index1 = index++;
                seg->m_Index2 = row->m_RowIndex;
                gapped_segs.push_back(seg);

                if (row->m_PositiveStrand) {
                    row->m_StartIt++;
                } else {
                    row->m_StartIt--;
                }
            }
            if (row->m_PositiveStrand) {
                row->m_StartIt++;
            } else {
                row->m_StartIt--;
            }
        }

        // add the gapped segments if any
        if (gapped_segs.size()) {
            sort(gapped_segs.begin(), gapped_segs.end(),
                 x_CompareAlnSegIndexes);

            if (m_MergeFlags & fGapJoin) {
                // request for trying to align gapped segments
                x_ConsolidateGaps(gapped_segs);
            }
            iterate (TSegments, seg_i, gapped_segs) {
                m_Segments.push_back(*seg_i);
            }
            gapped_segs.clear();
        }

        // add the refseq segment
        m_Segments.push_back(refseq_seg);

        if (refseq->m_PositiveStrand) {
            refseq_start_i++;
        } else {
            refseq_start_i--;
        }
    }
}


void CAlnMix::x_ConsolidateGaps(TSegments& gapped_segs)
{
    TSegments::iterator seg1_i, seg2_i;

    seg2_i = seg1_i = gapped_segs.begin();
    seg2_i++;

    bool         cache = false;
    string       s1;
    TSeqPos      start1;
    int          score1;
    CAlnMixSeq * seq1;

    while (seg2_i != gapped_segs.end()) {

        CAlnMixSegment * seg1 = *seg1_i;
        CAlnMixSegment * seg2 = *seg2_i;

        // check if this seg possibly aligns with the previous one
        bool possible = true;
            
        if (seg2->m_Len == seg1->m_Len  && 
            seg2->m_StartIts.size() == 1) {

            CAlnMixSeq * seq2 = (seg2->m_StartIts.begin())->first;

            // check if this seq was already used
            iterate (CAlnMixSegment::TStartIterators,
                     st_it,
                     (*seg1_i)->m_StartIts) {
                if (st_it->first == seq2) {
                    possible = false;
                    break;
                }
            }

            // check if score is sufficient
            if (possible) {
                if (!cache) {

                    seq1 = ((*seg1_i)->m_StartIts.begin())->first;
                
                    start1 = seg1->m_StartIts[seq1]->first;

                    seq1->m_BioseqHandle->GetSeqVector
                        (CBioseq_Handle::eCoding_Iupac,
                         seq1->m_PositiveStrand ? 
                         eNa_strand_plus : eNa_strand_minus).
                        GetSeqData(start1, start1 + seg1->m_Len, s1);

                    score1 = 
                        x_CalculateScore(s1, s1,
                                         seq1->m_IsAA, seq1->m_IsAA);

                    cache = true;
                }
                
                string s2;

                TSeqPos start2 = seg2->m_StartIts[seq2]->first;
                            
                seq2->m_BioseqHandle->GetSeqVector
                    (CBioseq_Handle::eCoding_Iupac,
                     seq2->m_PositiveStrand ? 
                     eNa_strand_plus : eNa_strand_minus).
                    GetSeqData(start2, start2 + seg2->m_Len, s2);

                int score2 = 
                    x_CalculateScore(s1, s2, seq1->m_IsAA, seq2->m_IsAA);

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
            seg1->m_StartIts[seg2->m_StartIts.begin()->first] =
                seg2->m_StartIts.begin()->second;
            
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
        CRef<CSeq_id> seq_id(new CSeq_id);
        SerialAssign(*seq_id,
                     *(m_Rows[numrow]->m_BioseqHandle->GetSeqId()));
        ids[numrow] = seq_id;
    }

    int offset = 0;
    for (numseg = 0;  numseg < numsegs;  numseg++, offset += numrows) {
        // lens
        lens[numseg] = m_Segments[numseg]->m_Len;

        // starts
        iterate (CAlnMixSegment::TStartIterators, start_its_i,
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

        if (strands[numrow] == eNa_strand_plus) {
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
                        " Seg=" + NStr::IntToString(numseg) +
                        " MaxStart=" + NStr::IntToString(max_start) +
                        " Start=" + NStr::IntToString(start);
                    
                    NCBI_THROW(CAlnException, eMergeFailure, errstr);
                }
                max_start = start + 
                  lens[strands[numrow] == eNa_strand_plus ?
                      numseg : numsegs - 1 - numseg];
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
