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
 * Author:  Yuri Kapustin
 *
 * ===========================================================================
 *
 */

#include <algo/align/nw_formatter.hpp>
#include <corelib/ncbi_limits.h>
#include <objects/seqalign/Score.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/seqalign/Dense_seg.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <serial/objostrasn.hpp>
#include <serial/serial.hpp>


BEGIN_NCBI_SCOPE


CNWFormatter::CNWFormatter (const CNWAligner& aligner):
    m_aligner(&aligner)
{
}


void CNWFormatter::AsSeqAlign(CSeq_align* seqalign) const
{
    if(seqalign == 0) return;

    seqalign->Reset();

    const vector<CNWAligner::ETranscriptSymbol>& transcript = 
        *(m_aligner->GetTranscript());

    if(transcript.size() == 0) return;

    // the alignment is pairwise
    seqalign->SetDim(2);

    // this is a global alignment
    seqalign->SetType(CSeq_align::eType_global);

    // seq-ids
    CRef< CSeq_id > id1 ( new CSeq_id );
    CRef< CObject_id > local_oid1 (new CObject_id);
    local_oid1->SetStr(m_Seq1Id);
    id1->SetLocal(*local_oid1);

    CRef< CSeq_id > id2 ( new CSeq_id );
    CRef< CObject_id > local_oid2 (new CObject_id);
    local_oid2->SetStr(m_Seq2Id);
    id2->SetLocal(*local_oid2);
    
    // the score was calculated during the main process
    CRef< CScore > score (new CScore);
    CRef< CObject_id > id (new CObject_id);
    id->SetStr("score");
    score->SetId(*id);
    CRef< CScore::C_Value > val (new CScore::C_Value);
    val->SetInt(m_aligner->GetScore());
    score->SetValue(*val);
    list< CRef< CScore > >& scorelist = seqalign->SetScore();
    scorelist.push_back(score);

    // create segments and add them to this seq-align
    CRef< CSeq_align::C_Segs > segs (new CSeq_align::C_Segs);
    CDense_seg& ds = segs->SetDenseg();
    ds.SetDim(2);
    vector< CRef< CSeq_id > > &ids = ds.SetIds();
    ids.push_back( id1 );
    ids.push_back( id2 );
    vector< TSignedSeqPos > &starts  = ds.SetStarts();
    vector< TSeqPos >       &lens    = ds.SetLens();
    vector< ENa_strand >    &strands = ds.SetStrands();
    
    // iterate through transcript
    size_t seg_count = 0;
    {{ 
        const char * const S1 = m_aligner->GetSeq1();
        const char * const S2 = m_aligner->GetSeq2();
        const char *seq1 = S1, *seq2 = S2;
        const char *start1 = seq1, *start2 = seq2;

        vector<CNWAligner::ETranscriptSymbol>::const_reverse_iterator
            ib = transcript.rbegin(),
            ie = transcript.rend(),
            ii;
        
        CNWAligner::ETranscriptSymbol ts = *ib;
        bool intron = (ts == CNWAligner::eTS_Intron);
        char seg_type0 = ((ts == CNWAligner::eTS_Insert || intron )? 1:
                          (ts == CNWAligner::eTS_Delete)? 2: 0);
        size_t seg_len = 0;

        for (ii = ib;  ii != ie; ++ii) {
            ts = *ii;
            intron = (ts == CNWAligner::eTS_Intron);
            char seg_type = ((ts == CNWAligner::eTS_Insert || intron )? 1:
                             (ts == CNWAligner::eTS_Delete)? 2: 0);
            if(seg_type0 != seg_type) {
                starts.push_back( (seg_type0 == 1)? -1: start1 - S1 );
                starts.push_back( (seg_type0 == 2)? -1: start2 - S2 );
                lens.push_back(seg_len);
                strands.push_back(eNa_strand_plus);
                strands.push_back(eNa_strand_plus);
                ++seg_count;
                start1 = seq1;
                start2 = seq2;
                seg_type0 = seg_type;
                seg_len = 1;
            }
            else {
                ++seg_len;
            }

            if(seg_type != 1) ++seq1;
            if(seg_type != 2) ++seq2;
        }
        // the last one
        starts.push_back( (seg_type0 == 1)? -1: start1 - S1 );
        starts.push_back( (seg_type0 == 2)? -1: start2 - S2 );
        lens.push_back(seg_len);
        strands.push_back(eNa_strand_plus);
        strands.push_back(eNa_strand_plus);
        ++seg_count;
    }}

    ds.SetNumseg(seg_count);
    ds.SetIds();
    seqalign->SetSegs(*segs);
}



void CNWFormatter::AsText(string* output, ETextFormatType type,
                          size_t line_width) const
    throw(CAlgoAlignException)

{
    CNcbiOstrstream ss;

    const vector<CNWAligner::ETranscriptSymbol>& transcript =
        *(m_aligner->GetTranscript());

    switch (type) {

    case eFormatType1: {
        if(m_Seq1Id.size() && m_Seq2Id.size()) {
            ss << '>' << m_Seq1Id << '\t' << m_Seq2Id << endl;
        }
        vector<char> v1, v2;
        size_t aln_size = x_ApplyTranscript(&v1, &v2);
        unsigned i1 = 0, i2 = 0;
        for (size_t i = 0;  i < aln_size; ) {
            ss << i << '\t' << i1 << ':' << i2 << endl;
            int i0 = i;
            for (size_t jPos = 0;  i < aln_size  &&  jPos < line_width;
                 ++i, ++jPos) {
                char c = v1[i0 + jPos];
                ss << c;
                if(c != '-'  &&  c != '+')
                    i1++;
            }
            ss << endl;
            
            string marker_line(line_width, ' ');
            i = i0;
            for (size_t jPos = 0;  i < aln_size  &&  jPos < line_width;
                 ++i, ++jPos) {
                char c1 = v1[i0 + jPos];
                char c  = v2[i0 + jPos];
                ss << c;
                if(c != '-' && c != '+')
                    i2++;
                if(c != c1  &&  c != '-'  &&  c1 != '-'  &&  c1 != '+')
                    marker_line[jPos] = '^';
            }
            ss << endl << marker_line << endl;
        }
    }
    break;

    case eFormatType2: {
        if(m_Seq1Id.size() && m_Seq2Id.size()) {
            ss << '>' << m_Seq1Id << '\t' << m_Seq2Id << endl;
        }
        vector<char> v1, v2;
        size_t aln_size = x_ApplyTranscript(&v1, &v2);
        unsigned i1 = 0, i2 = 0;
        for (size_t i = 0;  i < aln_size; ) {
            ss << i << '\t' << i1 << ':' << i2 << endl;
            int i0 = i;
            for (size_t jPos = 0;  i < aln_size  &&  jPos < line_width;
                 ++i, ++jPos) {
                char c = v1[i0 + jPos];
                ss << c;
                if(c != '-'  &&  c != '+')
                    i1++;
            }
            ss << endl;
            
            string line2 (line_width, ' ');
            string line3 (line_width, ' ');
            i = i0;
            for (size_t jPos = 0;  i < aln_size  &&  jPos < line_width;
                 ++i, ++jPos) {
                char c1 = v1[i0 + jPos];
                char c2  = v2[i0 + jPos];
                if(c2 != '-' && c2 != '+')
                    i2++;
                if(c2 == c1)
                    line2[jPos] = '|';
                line3[jPos] = c2;
            }
            ss << line2 << endl << line3 << endl << endl;
        }
    }
    break;

    case eFormatAsn: {
        CSeq_align seq_align;
        AsSeqAlign(&seq_align);
        CObjectOStreamAsn asn_stream (ss);
        asn_stream << seq_align;
        asn_stream << Separator;
    }
    break;

    case eFormatFastA: {
        vector<char> v1, v2;
        size_t aln_size = x_ApplyTranscript(&v1, &v2);
        
        ss << '>' << m_Seq1Id << endl;
        const vector<char>* pv = &v1;
        for(size_t i = 0; i < aln_size; ++i) {
            for(size_t j = 0; j < line_width && i < aln_size; ++j, ++i) {
                ss << (*pv)[i];
            }
            ss << endl;
        }

        ss << '>' << m_Seq2Id << endl;
        pv = &v2;
        for(size_t i = 0; i < aln_size; ++i) {
            for(size_t j = 0; j < line_width && i < aln_size; ++j, ++i) {
                ss << (*pv)[i];
            }
            ss << endl;
        }
    }
    break;
    
    case eFormatExonTable:
    case eFormatExonTableEx: {
        ss.precision(3);
        bool esfL1, esfR1, esfL2, esfR2;
        m_aligner->GetEndSpaceFree(&esfL1, &esfR1, &esfL2, &esfR2);
        const size_t len2 = m_aligner->GetSeqLen2();
        const char* start1 = m_aligner->GetSeq1();
        const char* start2 = m_aligner->GetSeq2();
        const char* p1 = start1;
        const char* p2 = start2;
        int tr_idx_hi0 = transcript.size() - 1, tr_idx_hi = tr_idx_hi0;
        int tr_idx_lo0 = 0, tr_idx_lo = tr_idx_lo0;

        if(esfL1 && transcript[tr_idx_hi0] == CNWAligner::eTS_Insert) {
            while(esfL1 && transcript[tr_idx_hi] == CNWAligner::eTS_Insert) {
                --tr_idx_hi;
                ++p2;
            }
        }

        if(esfL2 && transcript[tr_idx_hi0] == CNWAligner::eTS_Delete) {
            while(esfL2 && transcript[tr_idx_hi] == CNWAligner::eTS_Delete) {
                --tr_idx_hi;
                ++p1;
            }
        }

        if(esfR1 && transcript[tr_idx_lo0] == CNWAligner::eTS_Insert) {
            while(esfR1 && transcript[tr_idx_lo] == CNWAligner::eTS_Insert) {
                ++tr_idx_lo;
            }
        }

        if(esfR2 && transcript[tr_idx_lo0] == CNWAligner::eTS_Delete) {
            while(esfR2 && transcript[tr_idx_lo] == CNWAligner::eTS_Delete) {
                ++tr_idx_lo;
            }
        }

        bool type_ex = type == eFormatExonTableEx;
        vector<char> trans_ex (tr_idx_hi - tr_idx_lo + 1);

        for(int tr_idx = tr_idx_hi; tr_idx >= tr_idx_lo;) {
            const char* p1_beg = p1;
            const char* p2_beg = p2;
            size_t matches = 0, exon_size = 0;

            vector<char>::iterator ii_ex = trans_ex.begin();
            while(tr_idx >= tr_idx_lo && 
                  transcript[tr_idx] < CNWAligner::eTS_Intron) {
                
                bool noins = transcript[tr_idx] != CNWAligner::eTS_Insert;
                bool nodel = transcript[tr_idx] != CNWAligner::eTS_Delete;
                if(noins && nodel) {
                    if(*p1++ == *p2++) {
                        ++matches;
                        if(type_ex) *ii_ex++ = 'M';
                    }
                    else {
                        if(type_ex) *ii_ex++ = 'R';
                    }
                } else if( noins ) {
                    ++p1;
                    if(type_ex) *ii_ex++ = 'D';
                } else {
                    ++p2;
                    if(type_ex) *ii_ex++ = 'I';
                }
                --tr_idx;
                ++exon_size;
            }

            if(exon_size > 0) {

                if(m_Seq1Id.size() && m_Seq2Id.size()) {
                    ss << m_Seq1Id << '\t' << m_Seq2Id << '\t';
                }
                float identity = float(matches) / exon_size;
                ss << identity << '\t' << exon_size << '\t';
                size_t beg1  = p1_beg - start1, end1 = p1 - start1 - 1;
                size_t beg2  = p2_beg - start2, end2 = p2 - start2 - 1;
                ss << beg1 << '\t'<< end1<< '\t'<< beg2 << '\t'<< end2<< '\t';
                char c1 = (p2_beg >= start2 + 2)? *(p2_beg - 2): ' ';
                char c2 = (p2_beg >= start2 + 1)? *(p2_beg - 1): ' ';
                char c3 = (p2 < start2 + len2)? *(p2): ' ';
                char c4 = (p2 < start2 + len2 - 1)? *(p2+1): ' ';
                ss << c1 << c2 << "<exon>" << c3 << c4;
                if(type == eFormatExonTableEx) {
                    ss << '\t';
                    copy(trans_ex.begin(), ii_ex, ostream_iterator<char>(ss));
                }
                ss << endl;
            }
            // find next exon
            while(tr_idx >= tr_idx_lo &&
                  (transcript[tr_idx] == CNWAligner::eTS_Intron)) {

                --tr_idx;
                ++p2;
            }
        }
    }
    break;

    default: {
        NCBI_THROW(
                   CAlgoAlignException,
                   eBadParameter,
                   "Incorrect format specified");
    }

    }

    *output = CNcbiOstrstreamToString(ss);
}



// Transform source sequences according to the transcript.
// Write the results to v1 and v2 leaving source sequences intact.
// Return alignment size.
size_t CNWFormatter::x_ApplyTranscript(vector<char>* pv1, vector<char>* pv2)
    const
{
    const vector<CNWAligner::ETranscriptSymbol>& transcript = 
        *(m_aligner->GetTranscript());

    vector<char>& v1 = *pv1;
    vector<char>& v2 = *pv2;

    vector<CNWAligner::ETranscriptSymbol>::const_reverse_iterator
        ib = transcript.rbegin(),
        ie = transcript.rend(),
        ii;

    const char* iv1 = m_aligner->GetSeq1();
    const char* iv2 = m_aligner->GetSeq2();
    v1.clear();
    v2.clear();

    for (ii = ib;  ii != ie;  ii++) {
        CNWAligner::ETranscriptSymbol ts = *ii;
        char c1, c2;
        switch ( ts ) {
        case CNWAligner::eTS_Insert:
            c1 = '-';
            c2 = *iv2++;
            break;
        case CNWAligner::eTS_Delete:
            c2 = '-';
            c1 = *iv1++;
            break;
        case CNWAligner::eTS_Match:
        case CNWAligner::eTS_Replace:
            c1 = *iv1++;
            c2 = *iv2++;
            break;
        case CNWAligner::eTS_Intron:
            c1 = '+';
            c2 = *iv2++;
            break;
        default:
            c1 = c2 = '?';
            break;
        }
        v1.push_back(c1);
        v2.push_back(c2);
    }

    return v1.size();
}


END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.1  2003/09/02 22:34:49  kapustin
 * Initial revision
 *
 * ===========================================================================
 */
