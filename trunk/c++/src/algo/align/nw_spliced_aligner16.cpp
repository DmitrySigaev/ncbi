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
* Authors:  Yuri Kapustin
*
* File Description:  CSplicedAligner16
*                   
* ===========================================================================
*
*/


#include <algo/align/nw_spliced_aligner16.hpp>
#include <corelib/ncbi_limits.h>

BEGIN_NCBI_SCOPE

unsigned char g_nwspl_donor[splice_type_count_16][2] = {
    {'G','T'}, {'G','C'}, {'A','T'}, {'?','?'}
};

unsigned char g_nwspl_acceptor[splice_type_count_16][2] = {
    {'A','G'}, {'A','G'}, {'A','C'}, {'?','?'}
};


CSplicedAligner16::CSplicedAligner16()
{
    for(unsigned char st = 0; st < splice_type_count_16; ++st) {
        m_Wi[st] = GetDefaultWi(st);
    }
}


CSplicedAligner16::CSplicedAligner16(const char* seq1, size_t len1,
                                       const char* seq2, size_t len2)
    : CSplicedAligner(seq1, len1, seq2, len2)
{
    for(unsigned char st = 0; st < splice_type_count_16; ++st) {
        m_Wi[st] = GetDefaultWi(st);
    }
}


CNWAligner::TScore CSplicedAligner16::GetDefaultWi(unsigned char splice_type)
{

   switch(splice_type) {
        case 0: return -10; // GT/AG
        case 1: return -15; // GC/AG
        case 2: return -15; // AT/AC
        case 3: return -25; // ??/??
        default: {
            NCBI_THROW(CAlgoAlignException,
                       eInvalidSpliceTypeIndex,
                       "Invalid splice type index");
        }
    }
}


// Bit coding (eleven bits per value) for backtrace.
// --------------------------------------------------
// [11-8] donors (bitwise OR for multiple types)
//        1000     ??    (??/??) - arbitrary pair
//        0100     AT    (AT/AC)
//        0010     GC    (GC/AG)
//        0001     GT    (GT/AG)
// [7-5]  acceptor type
//        100      ?? (??/??)
//        011      AC (AT/AC)
//        010      AG (GC/AG)
//        001      AG (GT/AG)
//        000      no acceptor
// [4]    Fc:      1 if gap in 2nd sequence was extended; 0 if it is was opened
// [3]    Ec:      1 if gap in 1st sequence was extended; 0 if it is was opened
// [2]    E:       1 if space in 1st sequence; 0 if space in 2nd sequence
// [1]    D:       1 if diagonal; 0 - otherwise

const unsigned char kMaskFc       = 0x0001;
const unsigned char kMaskEc       = 0x0002;
const unsigned char kMaskE        = 0x0004;
const unsigned char kMaskD        = 0x0008;

// Evaluate dynamic programming matrix. Create transcript.
CNWAligner::TScore CSplicedAligner16::x_Align (
                         const char* seg1, size_t len1,
                         const char* seg2, size_t len2,
                         vector<ETranscriptSymbol>* transcript )
{
    const size_t N1 = len1 + 1;
    const size_t N2 = len2 + 1;

    vector<TScore> stl_rowV (N2), stl_rowF (N2);
    TScore* rowV    = &stl_rowV[0];
    TScore* rowF    = &stl_rowF[0];

    // index calculation: [i,j] = i*n2 + j
    vector<Uint2> stl_bm (N1*N2);
    Uint2* backtrace_matrix = &stl_bm[0];

    TScore* pV = rowV - 1;

    const char* seq1   = seg1 - 1;
    const char* seq2   = seg2 - 1;

    const TNCBIScore (*sm) [NCBI_FSM_DIM] = m_ScoreMatrix.s;

    bool bFreeGapLeft1  = m_esf_L1 && seg1 == m_Seq1;
    bool bFreeGapRight1 = m_esf_R1 && m_Seq1 + m_SeqLen1 - len1 == seg1;
    bool bFreeGapLeft2  = m_esf_L2 && seg2 == m_Seq2;
    bool bFreeGapRight2 = m_esf_R2 && m_Seq2 + m_SeqLen2 - len2 == seg2;

    TScore wgleft1   = bFreeGapLeft1? 0: m_Wg;
    TScore wsleft1   = bFreeGapLeft1? 0: m_Ws;
    TScore wg1 = wgleft1, ws1 = wsleft1;

    // recurrences
    TScore wgleft2   = bFreeGapLeft2? 0: m_Wg;
    TScore wsleft2   = bFreeGapLeft2? 0: m_Ws;
    TScore V  = 0, V_max, vAcc;
    TScore V0 = 0;
    TScore E, G, n0;
    Uint2 tracer;

    // store candidate donors
    size_t* jAllDonors [splice_type_count_16];
    TScore* vAllDonors [splice_type_count_16];
    vector<size_t> stl_jAllDonors (splice_type_count_16 * N2);
    vector<TScore> stl_vAllDonors (splice_type_count_16 * N2);
    for(unsigned char st = 0; st < splice_type_count_16; ++st) {
        jAllDonors[st] = &stl_jAllDonors[st*N2];
        vAllDonors[st] = &stl_vAllDonors[st*N2];
    }
    size_t  jTail[splice_type_count_16], jHead[splice_type_count_16];
    TScore  vBestDonor [splice_type_count_16];
    size_t  jBestDonor [splice_type_count_16];

    // fake row
    rowV[0] = kInfMinus;
    size_t k;
    for (k = 0; k < N2; k++) {
        rowV[k] = rowF[k] = kInfMinus;
    }
    k = 0;

    size_t i, j = 0, k0;
    unsigned char ci;
    for(i = 0;  i < N1;  ++i, j = 0) {
       
        V = i > 0? (V0 += wsleft2) : 0;
        E = kInfMinus;
        k0 = k;
        backtrace_matrix[k++] = kMaskFc;
        ci = i > 0? seq1[i]: 'N';

        for(unsigned char st = 0; st < splice_type_count_16; ++st) {
            jTail[st] = jHead[st] = 0;
            vBestDonor[st] = kInfMinus;
        }

        if(i == N1 - 1 && bFreeGapRight1) {
                wg1 = ws1 = 0;
        }

        TScore wg2 = m_Wg, ws2 = m_Ws;
            
        // detect donor candidate
        if(N2 > 2) {
            for( char st = splice_type_count_16 - 2; st >= 0; --st ) {
                if( seq2[1] == g_nwspl_donor[st][0] && seq2[2] == g_nwspl_donor[st][1]) {
                    jAllDonors[st][jTail[st]] = j;
                    vAllDonors[st][jTail[st]] = V;
                    ++(jTail[st]);
                }
            }
        }
        // the first max value
        jAllDonors[3][jTail[3]] = j;
        vAllDonors[3][jTail[3]] = V_max = V;
        ++(jTail[3]);

        for (j = 1; j < N2; ++j, ++k) {
            
            G = pV[j] + sm[ci][(unsigned char)seq2[j]];
            pV[j] = V;

            n0 = V + wg1;
            if(E >= n0) {
                E += ws1;      // continue the gap
                tracer = kMaskEc;
            }
            else {
                E = n0 + ws1;  // open a new gap
                tracer = 0;
            }

            if(j == N2 - 1 && bFreeGapRight2) {
                wg2 = ws2 = 0;
            }
            n0 = rowV[j] + wg2;
            if(rowF[j] >= n0) {
                rowF[j] += ws2;
                tracer |= kMaskFc;
            }
            else {
                rowF[j] = n0 + ws2;
            }

            // evaluate the score (V)
            if (E >= rowF[j]) {
                if(E >= G) {
                    V = E;
                    tracer |= kMaskE;
                }
                else {
                    V = G;
                    tracer |= kMaskD;
                }
            } else {
                if(rowF[j] >= G) {
                    V = rowF[j];
                }
                else {
                    V = G;
                    tracer |= kMaskD;
                }
            }

            // find out if there are new donors
            for(unsigned char st = 0; st < splice_type_count_16; ++st) {

                if(jTail[st] > jHead[st])  {
                    if(j - jAllDonors[st][jHead[st]] >= m_IntronMinSize) {
                        if(vAllDonors[st][jHead[st]] > vBestDonor[st]) {
                            vBestDonor[st] = vAllDonors[st][jHead[st]];
                            jBestDonor[st] = jAllDonors[st][jHead[st]];
                        }
                        ++(jHead[st]);
                    }
                }
            }
                
            // check splice signal
            size_t dnr_pos = 0;
            Uint2 tracer_dnr = 0xFFFF;
            Uint2 tracer_acc = 0;
            for(char st = splice_type_count_16 - 1; st >= 0; --st) {
                if(seq2[j-1] == g_nwspl_acceptor[st][0] && seq2[j] == g_nwspl_acceptor[st][1]
                   && vBestDonor[st] > kInfMinus || st == 3) {
                    vAcc = vBestDonor[st] + m_Wi[st];
                    if(vAcc > V) {
                        V = vAcc;
                        tracer_acc = (st+1) << 4;
                        dnr_pos = k0 + jBestDonor[st];
                        tracer_dnr = 0x0080 << st;
                    }
                }
            }

            if(tracer_dnr != 0xFFFF) {
                backtrace_matrix[dnr_pos] |= tracer_dnr;
                tracer |= tracer_acc;
            }

            backtrace_matrix[k] = tracer;

            // detect donor candidate
            if(j < N2 - 2) {
                for( char st = splice_type_count_16 - 1; st >= 0; --st ) {
                    
                    if( seq2[j+1] == g_nwspl_donor[st][0] &&
                        seq2[j+2] == g_nwspl_donor[st][1] &&
                        V > vBestDonor[st]  ) {
                        
                        jAllDonors[st][jTail[st]] = j;
                        vAllDonors[st][jTail[st]] = V;
                        ++(jTail[st]);
                    }
                }
            }
            
            // detect new best value
            if(V > V_max) {
                jAllDonors[3][jTail[3]] = j;
                vAllDonors[3][jTail[3]] = V_max = V;
                ++(jTail[3]);
            }
        }

        pV[j] = V;

        if(i == 0) {
            V0 = wgleft2;
            wg1 = m_Wg;
            ws1 = m_Ws;
        }

    }

    try {
    x_DoBackTrace(backtrace_matrix, N1, N2, transcript);
    }
    catch(exception&) { // GCC hack
      throw;
    }

    return V;
}


// perform backtrace step;
void CSplicedAligner16::x_DoBackTrace ( const Uint2* backtrace_matrix,
                                         size_t N1, size_t N2,
                                         vector<ETranscriptSymbol>* transcript)
{
    transcript->clear();
    transcript->reserve(N1 + N2);

    size_t k = N1*N2 - 1;
    while (k != 0) {
        Uint2 Key = backtrace_matrix[k];
        while(Key & 0x0070) {  // acceptor

            unsigned char acc_type = (Key & 0x0070) >> 4;
            Uint2 dnr_mask = 0x0040 << acc_type;
            ETranscriptSymbol ets = eTS_Intron;
            do {
                transcript->push_back(ets);
                Key = backtrace_matrix[--k];
            }
            while(!(Key & dnr_mask));

            if(Key & 0x0070) {
                NCBI_THROW(CAlgoAlignException,
                           eInternal,
                           "Adjacent splices encountered during backtrace");
            }
        }

        if(k == 0) continue;
        
        if (Key & kMaskD) {
            transcript->push_back(eTS_Match);  // or eTS_Replace
            k -= N2 + 1;
        }
        else if (Key & kMaskE) {
            transcript->push_back(eTS_Insert); --k;
            while(k > 0 && (Key & kMaskEc)) {
                transcript->push_back(eTS_Insert);
                Key = backtrace_matrix[k--];
            }
        }
        else {
            transcript->push_back(eTS_Delete);
            k -= N2;
            while(k > 0 && (Key & kMaskFc)) {
                transcript->push_back(eTS_Delete);
                Key = backtrace_matrix[k];
                k -= N2;
            }
        }
    }
}


CNWAligner::TScore CSplicedAligner16::x_ScoreByTranscript() const
{
    const size_t dim = m_Transcript.size();
    if(dim == 0) return 0;

    TScore score = 0;

    const char* p1 = m_Seq1;
    const char* p2 = m_Seq2;

    const TNCBIScore (*sm) [NCBI_FSM_DIM] = m_ScoreMatrix.s;

    char state1;   // 0 = normal, 1 = gap, 2 = intron
    char state2;   // 0 = normal, 1 = gap

    switch( m_Transcript[dim - 1] ) {

    case eTS_Match:
        state1 = state2 = 0;
        break;

    case eTS_Insert:
        state1 = 1; state2 = 0; score += m_Wg;
        break;

    case eTS_Intron:
        state1 = 0; state2 = 0;
        break; // intron flag set later

    case eTS_Delete:
        state1 = 0; state2 = 1; score += m_Wg;
        break;

    default: {
        NCBI_THROW(
                   CAlgoAlignException,
                   eInternal,
                   "Invalid transcript symbol");
        }
    }

    for(int i = dim - 1; i >= 0; --i) {

        unsigned char c1 = m_Seq1? *p1: 0;
        unsigned char c2 = m_Seq2? *p2: 0;
        switch(m_Transcript[i]) {

        case eTS_Match: {
            state1 = state2 = 0;
            score += sm[c1][c2];
            ++p1; ++p2;
        }
        break;

        case eTS_Insert: {
            if(state1 != 1) score += m_Wg;
            state1 = 1; state2 = 0;
            score += m_Ws;
            ++p2;
        }
        break;

        case eTS_Delete: {
            if(state2 != 1) score += m_Wg;
            state1 = 0; state2 = 1;
            score += m_Ws;
            ++p1;
        }
        break;

        case eTS_Intron: {

            if(state1 != 2) {
                for(unsigned char i = 0; i < splice_type_count_16; ++i) {
                    if(*p2 == g_nwspl_donor[i][0] && *(p2 + 1) == g_nwspl_donor[i][1]
                       || i == splice_type_count_16 - 1) {
                        score += m_Wi[i];
                        break;
                    }
                }
            }
            state1 = 2; state2 = 0;
            ++p2;
        }
        break;

        default: {
        NCBI_THROW(
                   CAlgoAlignException,
                   eInternal,
                   "Invalid transcript symbol");
        }
        }
    }

    if(m_esf_L1) {
        size_t g = 0;
        for(int i = dim - 1; i >= 0; --i) {
            if(m_Transcript[i] == eTS_Insert) ++g; else break;
        }
        if(g > 0) {
            score -= (m_Wg + g*m_Ws);
        }
    }

    if(m_esf_L2) {
        size_t g = 0;
        for(int i = dim - 1; i >= 0; --i) {
            if(m_Transcript[i] == eTS_Delete) ++g; else break;
        }
        if(g > 0) {
            score -= (m_Wg + g*m_Ws);
        }
    }

    if(m_esf_R1) {
        size_t g = 0;
        for(size_t i = 0; i < dim; ++i) {
            if(m_Transcript[i] == eTS_Insert) ++g; else break;
        }
        if(g > 0) {
            score -= (m_Wg + g*m_Ws);
        }
    }

    if(m_esf_R2) {
        size_t g = 0;
        for(size_t i = 0; i < dim; ++i) {
            if(m_Transcript[i] == eTS_Delete) ++g; else break;
        }
        if(g > 0) {
            score -= (m_Wg + g*m_Ws);
        }
    }

    return score;
}


END_NCBI_SCOPE



/*
 * ===========================================================================
 * $Log$
 * Revision 1.6  2003/10/27 21:00:17  kapustin
 * Set intron penalty defaults differently for 16- and 32-bit versions according to the expected quality of sequences those variants are supposed to be used with.
 *
 * Revision 1.5  2003/10/14 19:29:24  kapustin
 * Dismiss static keyword as a local-to-compilation-unit flag. Use longer name since unnamed namespaces are not everywhere supported
 *
 * Revision 1.4  2003/09/30 19:50:04  kapustin
 * Make use of standard score matrix interface
 *
 * Revision 1.3  2003/09/26 14:43:18  kapustin
 * Remove exception specifications
 *
 * Revision 1.2  2003/09/04 16:07:38  kapustin
 * Use STL vectors for exception-safe dynamic arrays and matrices
 *
 * Revision 1.1  2003/09/02 22:34:49  kapustin
 * Initial revision
 *
 * ===========================================================================
 */
