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
 * Authors:  Yuri Kapustin, Boris Kiryutin
 *
 * File Description:  CNWAligner implementation
 *                   
 * ===========================================================================
 *
 */


#include <algo/align/nw_aligner.hpp>
#include <corelib/ncbi_limits.h>


BEGIN_NCBI_SCOPE

// IUPACna alphabet (default)
const char g_nwaligner_nucleotides [] = "AGTCBDHKMNRSVWY";


CNWAligner::CNWAligner()
    : m_Wm(GetDefaultWm()),
      m_Wms(GetDefaultWms()),
      m_Wg(GetDefaultWg()),
      m_Ws(GetDefaultWs()),
      m_esf_L1(false), m_esf_R1(false), m_esf_L2(false), m_esf_R2(false),
      m_abc(g_nwaligner_nucleotides),
      m_prg_callback(0),
      m_Seq1(0), m_SeqLen1(0),
      m_Seq2(0), m_SeqLen2(0),
      m_score(kInfMinus)
{
    SetScoreMatrix(0);
}


CNWAligner::CNWAligner( const char* seq1, size_t len1,
                        const char* seq2, size_t len2,
                        const SNCBIPackedScoreMatrix* scoremat )

    : m_Wm(GetDefaultWm()),
      m_Wms(GetDefaultWms()),
      m_Wg(GetDefaultWg()),
      m_Ws(GetDefaultWs()),
      m_esf_L1(false), m_esf_R1(false), m_esf_L2(false), m_esf_R2(false),
      m_abc(g_nwaligner_nucleotides),
      m_prg_callback(0),
      m_Seq1(seq1), m_SeqLen1(len1),
      m_Seq2(seq2), m_SeqLen2(len2),
      m_score(kInfMinus)
{
    SetScoreMatrix(scoremat);
    SetSequences(seq1, len1, seq2, len2);
}


void CNWAligner::SetSequences(const char* seq1, size_t len1,
			      const char* seq2, size_t len2,
			      bool verify)
{
    if(!seq1 || !seq2) {
        NCBI_THROW(
                   CAlgoAlignException,
                   eBadParameter,
                   "NULL sequence pointer(s) passed");
    }

    if(verify) {
        size_t iErrPos1 = x_CheckSequence(seq1, len1);
	if(iErrPos1 < len1) {
	    ostrstream oss;
	    oss << "The first sequence is inconsistent with the current "
		<< "scoring matrix type. Symbol " << seq1[iErrPos1] << " at "
		<< iErrPos1;
	    string message = CNcbiOstrstreamToString(oss);
	    NCBI_THROW(
		       CAlgoAlignException,
		       eInvalidCharacter,
		       message );
	}

	size_t iErrPos2 = x_CheckSequence(seq2, len2);
	if(iErrPos2 < len2) {
	    ostrstream oss;
	    oss << "The second sequence is inconsistent with the current "
		<< "scoring matrix type. Symbol " << seq2[iErrPos2] << " at "
		<< iErrPos2;
	    string message = CNcbiOstrstreamToString(oss);
	    NCBI_THROW (
			CAlgoAlignException,
			eInvalidCharacter,
			message );
	}
    }
    m_Seq1 = seq1;
    m_SeqLen1 = len1;
    m_Seq2 = seq2;
    m_SeqLen2 = len2;
    m_Transcript.clear();
}



void CNWAligner::SetEndSpaceFree(bool Left1, bool Right1,
                                 bool Left2, bool Right2)
{
    m_esf_L1 = Left1;
    m_esf_R1 = Right1;
    m_esf_L2 = Left2;
    m_esf_R2 = Right2;
}

// evaluate score for each possible alignment;
// fill out backtrace matrix
// bit coding (four bits per value): D E Ec Fc
// D:  1 if diagonal; 0 - otherwise
// E:  1 if space in 1st sequence; 0 if space in 2nd sequence
// Ec: 1 if gap in 1st sequence was extended; 0 if it is was opened
// Fc: 1 if gap in 2nd sequence was extended; 0 if it is was opened
//

const unsigned char kMaskFc  = 0x0001;
const unsigned char kMaskEc  = 0x0002;
const unsigned char kMaskE   = 0x0004;
const unsigned char kMaskD   = 0x0008;

CNWAligner::TScore CNWAligner::x_Align(const char* seg1, size_t len1,
                                       const char* seg2, size_t len2,
                                       vector<ETranscriptSymbol>* transcript)
{
    const size_t N1 = len1 + 1;
    const size_t N2 = len2 + 1;

    vector<TScore> stl_rowV (N2), stl_rowF(N2);

    TScore* rowV    = &stl_rowV[0];
    TScore* rowF    = &stl_rowF[0];

    TScore* pV = rowV - 1;

    const char* seq1 = seg1 - 1;
    const char* seq2 = seg2 - 1;

    const TNCBIScore (*sm) [NCBI_FSM_DIM] = m_ScoreMatrix.s;

    m_terminate = false;

    if(m_prg_callback) {
        m_prg_info.m_iter_total = N1*N2;
        m_prg_info.m_iter_done = 0;
        if(m_terminate = m_prg_callback(&m_prg_info)) {
	  return 0;
	}
    }

    bool bFreeGapLeft1  = m_esf_L1 && seg1 == m_Seq1;
    bool bFreeGapRight1 = m_esf_R1 && m_Seq1 + m_SeqLen1 - len1 == seg1;
    bool bFreeGapLeft2  = m_esf_L2 && seg2 == m_Seq2;
    bool bFreeGapRight2 = m_esf_R2 && m_Seq2 + m_SeqLen2 - len2 == seg2;

    TScore wgleft1   = bFreeGapLeft1? 0: m_Wg;
    TScore wsleft1   = bFreeGapLeft1? 0: m_Ws;
    TScore wg1 = m_Wg, ws1 = m_Ws;

    // index calculation: [i,j] = i*n2 + j
    vector<unsigned char> stl_bm (N1*N2);
    unsigned char* backtrace_matrix = &stl_bm[0];

    // first row
    size_t k;
    rowV[0] = wgleft1;
    for (k = 1; k < N2; k++) {
        rowV[k] = pV[k] + wsleft1;
        rowF[k] = kInfMinus;
        backtrace_matrix[k] = kMaskE | kMaskEc;
    }
    rowV[0] = 0;
	
    if(m_prg_callback) {
        m_prg_info.m_iter_done = k;
        m_terminate = m_prg_callback(&m_prg_info);
    }

    // recurrences
    TScore wgleft2   = bFreeGapLeft2? 0: m_Wg;
    TScore wsleft2   = bFreeGapLeft2? 0: m_Ws;
    TScore V  = rowV[N2 - 1];
    TScore V0 = wgleft2;
    TScore E, G, n0;
    unsigned char tracer;

    size_t i, j;
    for(i = 1;  i < N1 && !m_terminate;  ++i) {
        
        V = V0 += wsleft2;
        E = kInfMinus;
        backtrace_matrix[k++] = kMaskFc;
        unsigned char ci = seq1[i];

        if(i == N1 - 1 && bFreeGapRight1) {
                wg1 = ws1 = 0;
        }

        TScore wg2 = m_Wg, ws2 = m_Ws;

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
            backtrace_matrix[k] = tracer;
        }

        pV[j] = V;

        if(m_prg_callback) {
            m_prg_info.m_iter_done = k;
            if(m_terminate = m_prg_callback(&m_prg_info)) {
                break;
            }
        }
       
    }

    if(!m_terminate) {
        x_DoBackTrace(backtrace_matrix, N1, N2, transcript);
    }

    return V;
}


CNWAligner::TScore CNWAligner::Run()
{
    if(!m_Seq1 || !m_Seq2) {
        NCBI_THROW(
		   CAlgoAlignException,
		   eNoData,
		   "Sequence data not available for one or both sequences");
    }

    if(!x_CheckMemoryLimit()) {

        NCBI_THROW(
                   CAlgoAlignException,
                   eMemoryLimit,
                   "Memory limit exceeded");
    }

    m_score = x_Run();

    return m_score;
}


CNWAligner::TScore CNWAligner::x_Run()
{
    try {
    
    if(m_guides.size() == 0) {
      m_score = x_Align(m_Seq1,m_SeqLen1, m_Seq2,m_SeqLen2, &m_Transcript);
    }
    else {

      // run the algorithm for every segment between hits
      m_Transcript.clear();
      size_t guides_dim = m_guides.size() / 4;
      size_t q1 = m_SeqLen1, q0, s1 = m_SeqLen2, s0;
      vector<ETranscriptSymbol> trans;
      
      // save original esf settings
      bool esf_L1 = m_esf_L1, esf_R1 = m_esf_R1, esf_L2 = m_esf_L2, esf_R2 = m_esf_R2;
      SetEndSpaceFree(false, esf_R1, false, esf_R2); // last region
      for(size_t istart = 4*guides_dim, i = istart; i != 0; i -= 4) {
	q0 = m_guides[i - 3] + 1;
	s0 = m_guides[i - 1] + 1;
	size_t dim_query = q1 - q0, dim_subj = s1 - s0;
	x_Align(m_Seq1 + q0, dim_query, m_Seq2 + s0, dim_subj, &trans);
	copy(trans.begin(), trans.end(), back_inserter(m_Transcript));
	size_t dim_hit = m_guides[i - 3] - m_guides[i - 4] + 1;
	for(size_t k = 0; k < dim_hit; ++k) {
	  m_Transcript.push_back(eTS_Match);
	}
	q1 = m_guides[i - 4];
	s1 = m_guides[i - 2];
	trans.clear();
	if(i == istart) {
	  SetEndSpaceFree(false, false, false, false); // middle regions
	}
      }
      SetEndSpaceFree(esf_L1, false, esf_L2, false); // first region
      x_Align(m_Seq1, q1, m_Seq2, s1, &trans);
      SetEndSpaceFree(esf_L1, esf_R1, esf_L2, esf_R2); // restore
      copy(trans.begin(), trans.end(), back_inserter(m_Transcript));
      m_score = x_ScoreByTranscript();
    }

  }
  
  catch( bad_alloc& ) {
      
    NCBI_THROW(
	       CAlgoAlignException,
	       eMemoryLimit,
	       "Memory limit exceeded");
  }

  return m_score;
}


// perform backtrace step;
void CNWAligner::x_DoBackTrace(const unsigned char* backtrace,
                               size_t N1, size_t N2,
                               vector<ETranscriptSymbol>* transcript)
{
    transcript->clear();
    transcript->reserve(N1 + N2);

    size_t k = N1*N2 - 1;
    while (k != 0) {
        unsigned char Key = backtrace[k];
        if (Key & kMaskD) {
            transcript->push_back(eTS_Match);
            k -= N2 + 1;
        }
        else if (Key & kMaskE) {
            transcript->push_back(eTS_Insert); --k;
            while(k > 0 && (Key & kMaskEc)) {
                transcript->push_back(eTS_Insert);
                Key = backtrace[k--];
            }
        }
        else {
            transcript->push_back(eTS_Delete);
            k -= N2;
            while(k > 0 && (Key & kMaskFc)) {
                transcript->push_back(eTS_Delete);
                Key = backtrace[k];
                k -= N2;
            }
        }
    }
}


void  CNWAligner::SetPattern(const vector<size_t>& guides)
{
    size_t dim = guides.size();
    const char* err = 0;
    if(dim % 4 == 0) {
        for(size_t i = 0; i < dim; i += 4) {

            if( guides[i] > guides[i+1] || guides[i+2] > guides[i+3] ) {
                err = "Pattern hits must be specified in plus strand";
		break;
            }

            if(i > 4) {
                if(guides[i] <= guides[i-3] || guides[i+2] <= guides[i-2]){
                    err = "Pattern hits coordinates must be sorted";
                    break;
                }
            }

            size_t dim1 = guides[i + 1] - guides[i];
            size_t dim2 = guides[i + 3] - guides[i + 2];
            if( dim1 != dim2) {
                err = "Pattern hits must have equal length on both sequences";
                break;
            }

            if(guides[i+1] >= m_SeqLen1 || guides[i+3] >= m_SeqLen2) {
                err = "One or several pattern hits are out of range";
                break;
            }
        }
    }
    else {
        err = "Pattern must have a dimension multiple of four";
    }

    if(err) {
        NCBI_THROW( CAlgoAlignException, eBadParameter, err );
    }
    else {
        m_guides = guides;
    }
}


void CNWAligner::GetEndSpaceFree(bool* L1, bool* R1, bool* L2, bool* R2) const
{
    if(L1) *L1 = m_esf_L1;
    if(R1) *R1 = m_esf_R1;
    if(L2) *L2 = m_esf_L2;
    if(R2) *R2 = m_esf_R2;
}


// Return transcript as a readable string
void CNWAligner::GetTranscriptString(vector<char>* out) const
{
    const size_t dim = m_Transcript.size();   
    const size_t line_size = 100;
    const size_t lines_est = dim / line_size + 1;
    out->resize(dim + lines_est);
    size_t i1 = 0, i2 = 0, i = 0;

    for (int k = dim - 1; k >= 0;  --k) {

        ETranscriptSymbol c0 = m_Transcript[k];
        char c = 0;
        switch ( c0 ) {

            case eTS_Match: 
            case eTS_Replace: {
                c = (m_Seq1[i1++] == m_Seq2[i2++])? 'M': 'R';
            }
            break;

            case eTS_Insert: {
                c = 'I';
                ++i2;
            }
            break;

            case eTS_Delete: {
                c = 'D';
                ++i1;
            }
            break;

            case eTS_Intron: {
                c = '+';
                ++i2;
            }
            break;
        }

        (*out)[i++] = c;
        if((dim - k) % line_size == 0) {
            (*out)[i++] = '\n';
        }
    }
    if((*out)[i-1] != '\n') {
        (*out)[i] = '\n';
    }
    if(i < out->size()) {
        out->resize(i + 1);
    }
}


void CNWAligner::SetProgressCallback ( FProgressCallback prg_callback,
				       void* data )
{
    m_prg_callback = prg_callback;
    m_prg_info.m_data = data;
}

 
void CNWAligner::SetScoreMatrix(const SNCBIPackedScoreMatrix* psm)
{
    if(psm) {
        m_abc = psm->symbols;
	NCBISM_Unpack(psm, &m_ScoreMatrix);
    }
    else { // assume IUPACna
        m_abc = g_nwaligner_nucleotides;
	memset(&m_ScoreMatrix.s, m_Wms, sizeof m_ScoreMatrix.s);
	// set diag to Wm except ambiguity symbols
        unsigned char c1, c2;
	const size_t kNuclSize = strlen(g_nwaligner_nucleotides);
	for(size_t i = 0; i < kNuclSize; ++i) {
	    c1 = g_nwaligner_nucleotides[i];
	    for(size_t j = 0; j < i; ++j) {
	        c2 = g_nwaligner_nucleotides[j];
		m_ScoreMatrix.s[c1][c2] = m_ScoreMatrix.s[c2][c1] = m_Wms;
	    }
	    m_ScoreMatrix.s[c1][c1] = i < 4? m_Wm: m_Wms;
	}
    }
}


// Check that all characters in sequence are valid for 
// the current sequence type.
// Return an index to the first invalid character
// or len if all characters are valid.
size_t CNWAligner::x_CheckSequence(const char* seq, size_t len) const
{
    char Flags [256];
    memset(Flags, 0, sizeof Flags);
    const size_t abc_size = strlen(m_abc);
    
    size_t k;
    for(k = 0; k < abc_size; ++k) {
        Flags[unsigned(m_abc[k])] = 1;
    }

    for(k = 0; k < len; ++k) {
        if(Flags[unsigned(seq[k])] == 0)
            break;
    }

    return k;
}


CNWAligner::TScore CNWAligner::GetScore() const 
{
  if(m_Transcript.size()) {
      return m_score;
  }
  else {
    NCBI_THROW(
	       CAlgoAlignException,
	       eNoData,
	       "Sequences not aligned yet");
  }
}


bool CNWAligner::x_CheckMemoryLimit()
{
    size_t gdim = m_guides.size();
    if(gdim) {
        size_t dim1 = m_guides[0], dim2 = m_guides[2];
	const size_t elem_size = x_GetElemSize();
        if(double(dim1)*dim2*elem_size >= kMax_UInt) {
            return false;
        }
        for(size_t i = 4; i < gdim; i += 4) {
            size_t dim1 = m_guides[i] - m_guides[i-3] + 1;
            size_t dim2 = m_guides[i + 2] - m_guides[i-1] + 1;
            if(double(dim1)*dim2*elem_size >= kMax_UInt) {
                return false;
            }
        }
        dim1 = m_SeqLen1 - m_guides[gdim-3];
        dim2 = m_SeqLen2 - m_guides[gdim-1];
        if(double(dim1)*dim2*elem_size >= kMax_UInt) {
            return false;
        }

        return true;
    }
    else {
        return double(m_SeqLen1 + 1)*(m_SeqLen2 + 1) < kMax_UInt;
    }
}


CNWAligner::TScore CNWAligner::x_ScoreByTranscript() const
{
    const size_t dim = m_Transcript.size();
    if(dim == 0) return 0;

    vector<ETranscriptSymbol> transcript (dim);
    for(size_t i = 0; i < dim; ++i) {
        transcript[i] = m_Transcript[dim - i - 1];
    }

    TScore score = 0;

    const char* p1 = m_Seq1;
    const char* p2 = m_Seq2;

    int state1;   // 0 = normal, 1 = gap, 2 = intron
    int state2;   // 0 = normal, 1 = gap

    switch(transcript[0]) {
    case eTS_Match:    state1 = state2 = 0; break;
    case eTS_Insert:   state1 = 1; state2 = 0; score += m_Wg; break;
    case eTS_Delete:   state1 = 0; state2 = 1; score += m_Wg; break;
    default: {
        NCBI_THROW(
                   CAlgoAlignException,
                   eInternal,
                   "Invalid transcript symbol");
        }
    }

    const TNCBIScore (*sm) [NCBI_FSM_DIM] = m_ScoreMatrix.s;

    for(size_t i = 0; i < dim; ++i) {

        unsigned char c1 = m_Seq1? *p1: 'N';
        unsigned char c2 = m_Seq2? *p2: 'N';
        switch(transcript[i]) {

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
        for(size_t i = 0; i < dim; ++i) {
            if(transcript[i] == eTS_Insert) ++g; else break;
        }
        if(g > 0) {
            score -= (m_Wg + g*m_Ws);
        }
    }

    if(m_esf_L2) {
        size_t g = 0;
        for(size_t i = 0; i < dim; ++i) {
            if(transcript[i] == eTS_Delete) ++g; else break;
        }
        if(g > 0) {
            score -= (m_Wg + g*m_Ws);
        }
    }

    if(m_esf_R1) {
        size_t g = 0;
        for(int i = dim - 1; i >= 0; --i) {
            if(transcript[i] == eTS_Insert) ++g; else break;
        }
        if(g > 0) {
            score -= (m_Wg + g*m_Ws);
        }
    }

    if(m_esf_R2) {
        size_t g = 0;
        for(int i = dim - 1; i >= 0; --i) {
            if(transcript[i] == eTS_Delete) ++g; else break;
        }
        if(g > 0) {
            score -= (m_Wg + g*m_Ws);
        }
    }

    return score;
}


size_t CNWAligner::GetLeftSeg(size_t* q0, size_t* q1,
                              size_t* s0, size_t* s1,
                              size_t min_size) const
{
    size_t trdim = m_Transcript.size();
    size_t cur = 0, maxseg = 0;
    const char* p1 = m_Seq1;
    const char* p2 = m_Seq2;
    size_t i0 = 0, j0 = 0, imax = i0, jmax = j0;

    for(int k = trdim - 1; k >= 0; --k) {

        switch(m_Transcript[k]) {

            case eTS_Insert: {
                ++p2;
                if(cur > maxseg) {
                    maxseg = cur;
                    imax = i0;
                    jmax = j0;
                    if(maxseg >= min_size) goto ret_point;
                }
                cur = 0;
            }
            break;

            case eTS_Delete: {
            ++p1;
            if(cur > maxseg) {
                maxseg = cur;
                imax = i0;
                jmax = j0;
                if(maxseg >= min_size) goto ret_point;
            }
            cur = 0;
            }
            break;

            case eTS_Match:
            case eTS_Replace: {
                if(*p1 == *p2) {
                    if(cur == 0) {
                        i0 = p1 - m_Seq1;
                        j0 = p2 - m_Seq2;
                    }
                    ++cur;
                }
                else {
                    if(cur > maxseg) {
                        maxseg = cur;
                        imax = i0;
                        jmax = j0;
                        if(maxseg >= min_size) goto ret_point;
                    }
                    cur = 0;
                }
                ++p1;
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

    if(cur > maxseg) {
      maxseg = cur;
      imax = i0;
      jmax = j0;
    }

 ret_point:

    *q0 = imax; *s0 = jmax;
    *q1 = *q0 + maxseg - 1;
    *s1 = *s0 + maxseg - 1;

    return maxseg;
}


size_t CNWAligner::GetRightSeg(size_t* q0, size_t* q1,
                               size_t* s0, size_t* s1,
                               size_t min_size) const
{
    size_t trdim = m_Transcript.size();
    size_t cur = 0, maxseg = 0;
    const char* seq1_end = m_Seq1 + m_SeqLen1;
    const char* seq2_end = m_Seq2 + m_SeqLen2;
    const char* p1 = seq1_end - 1;
    const char* p2 = seq2_end - 1;
    size_t i0 = m_SeqLen1 - 1, j0 = m_SeqLen2 - 1,
           imax = i0, jmax = j0;

    for(size_t k = 0; k < trdim; ++k) {

        switch(m_Transcript[k]) {

            case eTS_Insert: {
                --p2;
                if(cur > maxseg) {
                    maxseg = cur;
                    imax = i0;
                    jmax = j0;
                    if(maxseg >= min_size) goto ret_point;
                }
                cur = 0;
            }
            break;

            case eTS_Delete: {
                --p1;
                if(cur > maxseg) {
                    maxseg = cur;
                    imax = i0;
                    jmax = j0;
                    if(maxseg >= min_size) goto ret_point;
		}
                cur = 0;
            }
            break;

            case eTS_Match:
            case eTS_Replace: {
                if(*p1 == *p2) {
                    if(cur == 0) {
                        i0 = p1 - m_Seq1;
                        j0 = p2 - m_Seq2;
                    }
                    ++cur;
                }
                else {
                    if(cur > maxseg) {
                        maxseg = cur;
                        imax = i0;
                        jmax = j0;
                        if(maxseg >= min_size) goto ret_point;
                    }
                    cur = 0;
                }
                --p1;
                --p2;
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

    if(cur > maxseg) {
      maxseg = cur;
      imax = i0;
      jmax = j0;
    }

 ret_point:

    *q1 = imax; *s1 = jmax;
    *q0 = imax - maxseg + 1;
    *s0 = jmax - maxseg + 1;

    return maxseg;
}


size_t CNWAligner::GetLongestSeg(size_t* q0, size_t* q1,
                                 size_t* s0, size_t* s1) const
{
    size_t trdim = m_Transcript.size();
    size_t cur = 0, maxseg = 0;
    const char* p1 = m_Seq1;
    const char* p2 = m_Seq2;
    size_t i0 = 0, j0 = 0, imax = i0, jmax = j0;

    for(int k = trdim-1; k >= 0; --k) {

        switch(m_Transcript[k]) {

            case eTS_Insert: {
                ++p2;
                if(cur > maxseg) {
                    maxseg = cur;
                    imax = i0;
                    jmax = j0;
                }
                cur = 0;
            }
            break;

            case eTS_Delete: {
            ++p1;
            if(cur > maxseg) {
                maxseg = cur;
                imax = i0;
                jmax = j0;
            }
            cur = 0;
            }
            break;

            case eTS_Match:
            case eTS_Replace: {
                if(*p1 == *p2) {
                    if(cur == 0) {
                        i0 = p1 - m_Seq1;
                        j0 = p2 - m_Seq2;
                    }
                    ++cur;
                }
                else {
		    if(cur > maxseg) {
                        maxseg = cur;
                        imax = i0;
                        jmax = j0;
                    }
                    cur = 0;
                }
                ++p1;
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
    
    if(cur > maxseg) {
        maxseg = cur;
        imax = i0;
        jmax = j0;
    }
    
    *q0 = imax; *s0 = jmax;
    *q1 = *q0 + maxseg - 1;
    *s1 = *s0 + maxseg - 1;

    return maxseg;
}


END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.40  2003/10/27 20:47:35  kapustin
 * Minor code cleanup
 *
 * Revision 1.39  2003/10/14 18:44:46  kapustin
 * Adjust esf flags during pattern-guided alignment
 *
 * Revision 1.38  2003/09/30 19:50:04  kapustin
 * Make use of standard score matrix interface
 *
 * Revision 1.37  2003/09/26 14:43:18  kapustin
 * Remove exception specifications
 *
 * Revision 1.36  2003/09/15 20:49:11  kapustin
 * Clear the transcript when setting new sequences
 *
 * Revision 1.35  2003/09/04 16:07:38  kapustin
 * Use STL vectors for exception-safe dynamic arrays and matrices
 *
 * Revision 1.34  2003/09/03 17:28:40  kapustin
 * Clean the list of includes
 *
 * Revision 1.33  2003/09/02 22:38:18  kapustin
 * Move format functionality out of the class
 *
 * Revision 1.32  2003/07/09 15:11:22  kapustin
 * Update plain text output with seq ids and use inequalities instead of
 * binary operation to verify intron boundaries
 *
 * Revision 1.31  2003/06/26 20:39:39  kapustin
 * Rename formal parameters in SetEndSpaceFree() to avoid conflict with macro
 * under some configurations
 *
 * Revision 1.30  2003/06/17 17:20:44  kapustin
 * CNWAlignerException -> CAlgoAlignException
 *
 * Revision 1.29  2003/06/17 16:06:13  kapustin
 * Detect all variety of splices in Seq-Align formatter (restored from 1.27)
 *
 * Revision 1.28  2003/06/17 14:51:04  dicuccio
 * Fixed after algo/ rearragnement
 *
 * Revision 1.26  2003/06/02 14:04:49  kapustin
 * Progress indication-related updates
 *
 * Revision 1.25  2003/05/23 18:27:02  kapustin
 * Use weak comparisons in core recurrences. Adjust for new
 * transcript identifiers.
 *
 * Revision 1.24  2003/04/17 14:44:40  kapustin
 * A few changes to eliminate gcc warnings
 *
 * Revision 1.23  2003/04/14 19:00:55  kapustin
 * Add guide creation facility.  x_Run() -> x_Align()
 *
 * Revision 1.22  2003/04/10 19:15:16  kapustin
 * Introduce guiding hits approach
 *
 * Revision 1.21  2003/04/02 20:52:55  kapustin
 * Make FormatAsText virtual. Pass output string as a parameter.
 *
 * Revision 1.20  2003/03/31 15:32:05  kapustin
 * Calculate score independently from transcript
 *
 * Revision 1.19  2003/03/19 16:36:39  kapustin
 * Reverse memory limit condition
 *
 * Revision 1.18  2003/03/18 15:15:51  kapustin
 * Implement virtual memory checking function. Allow separate free end
 * gap specification
 *
 * Revision 1.17  2003/03/17 15:31:46  kapustin
 * Forced conversion to double in memory limit checking to avoid integer
 * overflow
 *
 * Revision 1.16  2003/03/14 19:18:50  kapustin
 * Add memory limit checking. Fix incorrect seq index references when
 * reporting about incorrect input seq character. Support all characters
 * within IUPACna coding
 *
 * Revision 1.15  2003/03/07 13:51:11  kapustin
 * Use zero-based indices to specify seq coordinates in ASN
 *
 * Revision 1.14  2003/03/05 20:13:48  kapustin
 * Simplify FormatAsText(). Fix FormatAsSeqAlign(). Convert sequence
 * alphabets to capitals
 *
 * Revision 1.13  2003/02/11 16:06:54  kapustin
 * Add end-space free alignment support
 *
 * Revision 1.12  2003/02/04 23:04:50  kapustin
 * Add progress callback support
 *
 * Revision 1.11  2003/01/31 02:55:48  lavr
 * User proper argument for ::time()
 *
 * Revision 1.10  2003/01/30 20:32:36  kapustin
 * Add EstiamteRunningTime()
 *
 * Revision 1.9  2003/01/28 12:39:03  kapustin
 * Implement ASN.1 and SeqAlign output
 *
 * Revision 1.8  2003/01/24 16:48:50  kapustin
 * Support more output formats - type 2 and gapped FastA
 *
 * Revision 1.7  2003/01/21 16:34:21  kapustin
 * Get rid of reverse() and reverse_copy() not supported by MSVC and/or ICC
 *
 * Revision 1.6  2003/01/21 12:41:37  kapustin
 * Use class neg infinity constant to specify least possible score
 *
 * Revision 1.5  2003/01/08 15:42:59  kapustin
 * Fix initialization for the first column of the backtrace matrix
 *
 * Revision 1.4  2003/01/06 18:04:10  kapustin
 * CNWAligner: add sequence size verification
 *
 * Revision 1.3  2002/12/31 13:53:13  kapustin
 * CNWAligner::Format() -- use CNcbiOstrstreamToString
 *
 * Revision 1.2  2002/12/12 17:59:28  kapustin
 * Enable spliced alignments
 *
 * Revision 1.1  2002/12/06 17:41:21  ivanov
 * Initial revision
 *
 * ===========================================================================
 */
