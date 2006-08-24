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
 * Authors:  Yuri Kapustin, Boris Kiryutin, Alexander Souvorov
 *
 * File Description:  CNWAligner implementation
 *                   
 * ===========================================================================
 *
 */


#include <ncbi_pch.hpp>

#include "nw_aligner_threads.hpp"
#include "messages.hpp"

#include <corelib/ncbi_system.hpp>
#include <algo/align/nw/align_exception.hpp>
#include <algorithm>


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
      m_ScoreMatrixInvalid(true),
      m_prg_callback(0),
      m_terminate(false),
      m_Seq1(0), m_SeqLen1(0),
      m_Seq2(0), m_SeqLen2(0),
      m_PositivesAsMatches(false),
      m_score(kInfMinus),
      m_mt(false),
      m_maxthreads(1)
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
      m_ScoreMatrixInvalid(true),
      m_prg_callback(0),
      m_terminate(false),
      m_Seq1(seq1), m_SeqLen1(len1),
      m_Seq2(seq2), m_SeqLen2(len2),
      m_PositivesAsMatches(false),
      m_score(kInfMinus),
      m_mt(false),
      m_maxthreads(1)
{
    SetScoreMatrix(scoremat);
    SetSequences(seq1, len1, seq2, len2);
}


CNWAligner::CNWAligner(const string& seq1,
                       const string& seq2,
                       const SNCBIPackedScoreMatrix* scoremat)
    : m_Wm(GetDefaultWm()),
      m_Wms(GetDefaultWms()),
      m_Wg(GetDefaultWg()),
      m_Ws(GetDefaultWs()),
      m_esf_L1(false), m_esf_R1(false), m_esf_L2(false), m_esf_R2(false),
      m_abc(g_nwaligner_nucleotides),
      m_ScoreMatrixInvalid(true),
      m_prg_callback(0),
      m_terminate(false),
      m_Seq1(seq1.data()), m_SeqLen1(seq1.size()),
      m_Seq2(seq2.data()), m_SeqLen2(seq2.size()),
      m_score(kInfMinus),
      m_mt(false),
      m_maxthreads(1)
{
    SetScoreMatrix(scoremat);
    SetSequences(seq1, seq2);
};


void CNWAligner::SetSequences(const char* seq1, size_t len1,
			      const char* seq2, size_t len2,
			      bool verify)
{
    if(!seq1 || !seq2) {
        NCBI_THROW(CAlgoAlignException, eBadParameter,
                   g_msg_NullParameter);
    }

    if(verify) {
        size_t iErrPos1 = x_CheckSequence(seq1, len1);
	if(iErrPos1 < len1) {
	    ostrstream oss;
	    oss << "The first sequence is inconsistent with the current "
		<< "scoring matrix type. "
                << "Position = " << iErrPos1 
                << " Symbol = '" << seq1[iErrPos1] << "'";

	    string message = CNcbiOstrstreamToString(oss);
	    NCBI_THROW(CAlgoAlignException, eInvalidCharacter, message);
	}

	size_t iErrPos2 = x_CheckSequence(seq2, len2);
	if(iErrPos2 < len2) {
	    ostrstream oss;
	    oss << "The second sequence is inconsistent with the current "
		<< "scoring matrix type. "
                << "Position = " << iErrPos2 
                << " Symbol = '" << seq2[iErrPos2] << "'";

	    string message = CNcbiOstrstreamToString(oss);
	    NCBI_THROW(CAlgoAlignException, eInvalidCharacter, message);
	}
    }
    m_Seq1 = seq1;
    m_SeqLen1 = len1;
    m_Seq2 = seq2;
    m_SeqLen2 = len2;
    m_Transcript.clear();
}


void CNWAligner::SetSequences(const string& seq1,
                              const string& seq2,
                              bool verify)
{
    SetSequences(seq1.data(), seq1.size(), seq2.data(), seq2.size(), verify);
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

const unsigned char kMaskFc  = 0x01;
const unsigned char kMaskEc  = 0x02;
const unsigned char kMaskE   = 0x04;
const unsigned char kMaskD   = 0x08;

CNWAligner::TScore CNWAligner::x_Align(SAlignInOut* data)
{
    const size_t N1 = data->m_len1 + 1;
    const size_t N2 = data->m_len2 + 1;

    vector<TScore> stl_rowV (N2), stl_rowF(N2);

    TScore* rowV    = &stl_rowV[0];
    TScore* rowF    = &stl_rowF[0];

    TScore* pV = rowV - 1;

    const char* seq1 = m_Seq1 + data->m_offset1 - 1;
    const char* seq2 = m_Seq2 + data->m_offset2 - 1;

    const TNCBIScore (*sm) [NCBI_FSM_DIM] = m_ScoreMatrix.s;

    m_terminate = false;

    if(m_prg_callback) {
        m_prg_info.m_iter_total = N1*N2;
        m_prg_info.m_iter_done = 0;
        if(m_terminate = m_prg_callback(&m_prg_info)) {
	  return 0;
	}
    }

    bool bFreeGapLeft1  = data->m_esf_L1 && data->m_offset1 == 0;
    bool bFreeGapRight1 = data->m_esf_R1 &&
                          m_SeqLen1 == data->m_offset1 + data->m_len1; 

    bool bFreeGapLeft2  = data->m_esf_L2 && data->m_offset2 == 0;
    bool bFreeGapRight2 = data->m_esf_R2 &&
                          m_SeqLen2 == data->m_offset2 + data->m_len2; 

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
        x_DoBackTrace(backtrace_matrix, data);
    }

    return V;
}


CNWAligner::TScore CNWAligner::Run()
{
    if(m_ScoreMatrixInvalid) {

        NCBI_THROW(CAlgoAlignException, eInvalidMatrix,
                   "CNWAligner::SetScoreMatrix(NULL) must be called "
                   "after changing match/mismatch scores "
                   "to make sure that the new parameters are engaged.");
    }

    if(!m_Seq1 || !m_Seq2) {
        NCBI_THROW(CAlgoAlignException, eNoData,
                   g_msg_DataNotAvailable);
    }

    if(!x_CheckMemoryLimit()) {
        NCBI_THROW(CAlgoAlignException, eMemoryLimit, g_msg_OutOfSpace);
    }

    m_score = x_Run();

    return m_score;
}


CNWAligner::TScore CNWAligner::x_Run()
{
    try {
    
        if(m_guides.size() == 0) {

            SAlignInOut data (0, m_SeqLen1, m_esf_L1, m_esf_R1,
                              0, m_SeqLen2, m_esf_L2, m_esf_R2);
            m_score = x_Align(&data);
            m_Transcript = data.m_transcript;
        }
        // run the algorithm for every segment between hits
        else if(m_mt && m_maxthreads > 1) {

            size_t guides_dim = m_guides.size() / 4;

            // setup inputs
            typedef vector<SAlignInOut> TDataVector;
            TDataVector vdata;
            vector<size_t> seed_dims;
            {{
                vdata.reserve(guides_dim + 1);
                seed_dims.reserve(guides_dim + 1);
                size_t q1 = m_SeqLen1, q0, s1 = m_SeqLen2, s0;
                for(size_t istart = 4*guides_dim, i = istart; i != 0; i -= 4) {
                    
                    q0 = m_guides[i - 3] + 1;
                    s0 = m_guides[i - 1] + 1;
                    size_t dim_query = q1 - q0, dim_subj = s1 - s0;
                    
                    bool esf_L1 = false, esf_R1 = false,
                        esf_L2 = false, esf_R2 = false;
                    if(i == istart) {
                        esf_R1 = m_esf_R1;
                        esf_R2 = m_esf_R2;
                    }
                    
                    SAlignInOut data (q0, dim_query, esf_L1, esf_R1,
                                      s0, dim_subj, esf_L2, esf_R2);
                    
                    vdata.push_back(data);
                    seed_dims.push_back(m_guides[i-3] - m_guides[i-4] + 1);

                    q1 = m_guides[i - 4];
                    s1 = m_guides[i - 2];
                }
                SAlignInOut data(0, q1, m_esf_L1, false,
                                 0, s1, m_esf_L2, false);
                vdata.push_back(data);
            }}

            // rearrange vdata so that the largest chunks come first
            typedef vector<SAlignInOut*> TDataPtrVector;
            TDataPtrVector vdata_p (vdata.size());
            {{
                TDataPtrVector::iterator jj = vdata_p.begin();
                NON_CONST_ITERATE(TDataVector, ii, vdata) {
                    *jj++ = &(*ii);
                }
                stable_sort(vdata_p.begin(), vdata_p.end(),
                            SAlignInOut::PSpace);
            }}
            
            // align over the segments
            {{
                m_Transcript.clear();
                size_t idim = vdata.size();

                typedef vector<CNWAlignerThread_Align*> TThreadVector;
                TThreadVector threads;
                threads.reserve(idim);

                ITERATE(TDataPtrVector, ii, vdata_p) {
                    
                    SAlignInOut& data = **ii;

                    if(data.GetSpace() >= 10000000 &&
                       NW_RequestNewThread(m_maxthreads)) {
                        
                        CNWAlignerThread_Align* thread = 
                            new CNWAlignerThread_Align(this, &data);
                        threads.push_back(thread);
                        thread->Run();
                    }
                    else {
                        x_Align(&data);
                    }
                }

                auto_ptr<CException> e;
                ITERATE(TThreadVector, ii, threads) {

                    if(e.get() == 0) {
                        CException* pe = 0;
                        (*ii)->Join(reinterpret_cast<void**>(&pe));
                        if(pe) {
                            e.reset(new CException (*pe));
                        }
                    }
                    else {
                        (*ii)->Join(0);
                    }
                }
                if(e.get()) {
                    throw *e;
                }

                for(size_t idata = 0; idata < idim; ++idata) {

                    SAlignInOut& data = vdata[idata];
                    copy(data.m_transcript.begin(), data.m_transcript.end(),
                         back_inserter(m_Transcript));
                    if(idata + 1 < idim) {
                        for(size_t k = 0; k < seed_dims[idata]; ++k) {
                            m_Transcript.push_back(eTS_Match);
                        }
                    }
                }
                
                m_score = ScoreFromTranscript(GetTranscript(false), 0, 0);
            }}
        }
        else {            

            m_Transcript.clear();
            size_t guides_dim = m_guides.size() / 4;
            size_t q1 = m_SeqLen1, q0, s1 = m_SeqLen2, s0;
            for(size_t istart = 4*guides_dim, i = istart; i != 0; i -= 4) {

                q0 = m_guides[i - 3] + 1;
                s0 = m_guides[i - 1] + 1;
                size_t dim_query = q1 - q0, dim_subj = s1 - s0;
                
                bool esf_L1 = false, esf_R1 = false,
                     esf_L2 = false, esf_R2 = false;
                if(i == istart) {
                    esf_R1 = m_esf_R1;
                    esf_R2 = m_esf_R2;
                }

                SAlignInOut data (q0, dim_query, esf_L1, esf_R1,
                                  s0, dim_subj, esf_L2, esf_R2);

                x_Align(&data);
                copy(data.m_transcript.begin(), data.m_transcript.end(),
                     back_inserter(m_Transcript));

                size_t dim_hit = m_guides[i - 3] - m_guides[i - 4] + 1;
                for(size_t k = 0; k < dim_hit; ++k) {
                    m_Transcript.push_back(eTS_Match);
                }
                q1 = m_guides[i - 4];
                s1 = m_guides[i - 2];
            }
            SAlignInOut data(0, q1, m_esf_L1, false,
                             0, s1, m_esf_L2, false);
            x_Align(&data);
            copy(data.m_transcript.begin(), data.m_transcript.end(),
                 back_inserter(m_Transcript));

            m_score = ScoreFromTranscript(GetTranscript(false), 0, 0);
        }
    }
    
    catch( bad_alloc& ) {
        
        NCBI_THROW(CAlgoAlignException, eMemoryLimit,
                   g_msg_OutOfSpace);
    }
    
    return m_score;
}


CNWAligner::ETranscriptSymbol CNWAligner::x_GetDiagTS(size_t i1, size_t i2)
const
{
    const unsigned char c1 = m_Seq1[i1--];
    const unsigned char c2 = m_Seq2[i2--];
    
    ETranscriptSymbol ts;
    if(m_PositivesAsMatches) {
        ts = m_ScoreMatrix.s[c1][c2] > 0? eTS_Match: eTS_Replace;
    }
    else {
        ts = (c1 == c2)? eTS_Match: eTS_Replace;
    }
    return ts;
}


// perform backtrace step;
void CNWAligner::x_DoBackTrace(const unsigned char* backtrace,
                               SAlignInOut* data)
{
    const size_t N1 = data->m_len1 + 1;
    const size_t N2 = data->m_len2 + 1;

    data->m_transcript.clear();
    data->m_transcript.reserve(N1 + N2);

    size_t k = N1*N2 - 1;
    size_t i1 = data->m_offset1 + data->m_len1 - 1;
    size_t i2 = data->m_offset2 + data->m_len2 - 1;
    while (k != 0) {

        unsigned char Key = backtrace[k];
        if (Key & kMaskD) {

            data->m_transcript.push_back(x_GetDiagTS(i1--, i2--));
            k -= N2 + 1;
        }
        else if (Key & kMaskE) {

            data->m_transcript.push_back(eTS_Insert);
            --k;
            --i2;
            while(k > 0 && (Key & kMaskEc)) {

                data->m_transcript.push_back(eTS_Insert);
                Key = backtrace[k--];
                --i2;
            }
        }
        else {

            data->m_transcript.push_back(eTS_Delete);
            k -= N2;
            --i1;
            while(k > 0 && (Key & kMaskFc)) {

                data->m_transcript.push_back(eTS_Delete);
                Key = backtrace[k];
                k -= N2;
                --i1;
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
        NCBI_THROW(CAlgoAlignException, eBadParameter, err);
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


// return raw transcript
CNWAligner::TTranscript CNWAligner::GetTranscript(bool reversed) const
{
    TTranscript rv (m_Transcript.size());
    if(reversed) {
        copy(m_Transcript.begin(), m_Transcript.end(), rv.begin());
    }
    else {
        copy(m_Transcript.rbegin(), m_Transcript.rend(), rv.begin());
    }
    return rv;
}


// alignment 'restore' - set raw transcript
void CNWAligner::SetTranscript(const TTranscript& transcript)
{
    m_Transcript = transcript;
    m_score = ScoreFromTranscript(transcript);
}


// Return transcript as a readable string
string CNWAligner::GetTranscriptString(void) const
{
    const size_t dim = m_Transcript.size();   
    string s;
    s.resize(dim);
    size_t i1 = 0, i2 = 0, i = 0;

    for (int k = dim - 1; k >= 0; --k) {

        ETranscriptSymbol c0 = m_Transcript[k];
        char c = 0;
        switch ( c0 ) {

            case eTS_Match: {

                if(m_Seq1 && m_Seq2) {
                    ETranscriptSymbol ts = x_GetDiagTS(i1++, i2++);
                    c = ts == eTS_Match? 'M': 'R';
                }
                else {
                    c = 'M';
                }
            }
            break;

            case eTS_Replace: {

                if(m_Seq1 && m_Seq2) {
                    ETranscriptSymbol ts = x_GetDiagTS(i1++, i2++);
                    c = ts == eTS_Match? 'M': 'R';
                }
                else {
                    c = 'R';
                }
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

	    default: {
	      NCBI_THROW(CAlgoAlignException, eInternal,
                         g_msg_InvalidTranscriptSymbol);
	    }	  
        }

        s[i++] = c;
    }

    if(i < s.size()) {
        s.resize(i + 1);
    }

    return s;
}


void CNWAligner::SetProgressCallback ( FProgressCallback prg_callback,
				       void* data )
{
    m_prg_callback = prg_callback;
    m_prg_info.m_data = data;
}


void CNWAligner::SetWms(TScore val)
{
    m_Wms = val;
    m_ScoreMatrixInvalid = true;
}


void CNWAligner::SetWm(TScore val)
{
    m_Wm = val;
    m_ScoreMatrixInvalid = true;
}

 
void CNWAligner::SetScoreMatrix(const SNCBIPackedScoreMatrix* psm)
{
    if(psm) {

        m_abc = psm->symbols;
	NCBISM_Unpack(psm, &m_ScoreMatrix);
    }
    else { // assume IUPACna

        m_abc = g_nwaligner_nucleotides;
        const size_t dim = strlen(m_abc);
        vector<TNCBIScore> iupacna (dim*dim, m_Wms);
        iupacna[0] = iupacna[dim+1] = iupacna[2*(dim+1)] = 
            iupacna[3*(dim+1)] = m_Wm;
        SNCBIPackedScoreMatrix iupacna_psm;
        iupacna_psm.symbols = g_nwaligner_nucleotides;
        iupacna_psm.scores = &iupacna.front();
        iupacna_psm.defscore = m_Wms;
        NCBISM_Unpack(&iupacna_psm, &m_ScoreMatrix);        
    }
    m_ScoreMatrixInvalid = false;
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
        Flags[unsigned(toupper((unsigned char) m_abc[k]))] = 1;
        Flags[unsigned(tolower((unsigned char) m_abc[k]))] = 1;
        Flags[unsigned(k)] = 1;
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
    NCBI_THROW(CAlgoAlignException, eNoData,
               g_msg_NoAlignment);
  }
}


bool CNWAligner::x_CheckMemoryLimit()
{
    const size_t elem_size = GetElemSize();
    const size_t gdim = m_guides.size();
    const size_t max_mem = kMax_UInt / 2;
    if(gdim) {
        size_t dim1 = m_guides[0], dim2 = m_guides[2];
        double mem = double(dim1)*dim2*elem_size;
        if(mem >= max_mem) {
            return false;
        }
        for(size_t i = 4; i < gdim; i += 4) {
            dim1 = m_guides[i] - m_guides[i-3] + 1;
            dim2 = m_guides[i + 2] - m_guides[i-1] + 1;
            mem = double(dim1)*dim2*elem_size;
            if(mem >= max_mem) {
                return false;
            }
        }
        dim1 = m_SeqLen1 - m_guides[gdim-3];
        dim2 = m_SeqLen2 - m_guides[gdim-1];
        mem = double(dim1)*dim2*elem_size;
        if(mem >= max_mem) {
            return false;
        }

        return true;
    }
    else {
        double mem = double(m_SeqLen1 + 1)*(m_SeqLen2 + 1)*elem_size;
        return mem < max_mem;
    }
}


void CNWAligner::EnableMultipleThreads(bool enable)
{
    m_maxthreads = (m_mt = enable)? GetCpuCount(): 1;
}


CNWAligner::TScore CNWAligner::ScoreFromTranscript(
                       const TTranscript& transcript,
                       size_t start1, size_t start2) const
{
    bool nucl_mode;
    if(start1 == kMax_UInt && start2 == kMax_UInt) {
        nucl_mode = true;
    }
    else if(start1 != kMax_UInt && start2 != kMax_UInt) {
        nucl_mode = false;
    }
    else {
        NCBI_THROW(CAlgoAlignException, eInternal,
                   g_msg_InconsistentArguments);
    }

    const size_t dim = transcript.size();
    if(dim == 0) {
        return 0;
    }

    TScore score = 0;

    const char* p1 = m_Seq1 + start1;
    const char* p2 = m_Seq2 + start2;

    int state1;   // 0 = normal, 1 = gap, 2 = intron
    int state2;   // 0 = normal, 1 = gap

    switch(transcript[0]) {
    case eTS_Match:
    case eTS_Replace:  state1 = state2 = 0; break;
    case eTS_Insert:   state1 = 1; state2 = 0; score += m_Wg; break;
    case eTS_Delete:   state1 = 0; state2 = 1; score += m_Wg; break;
    default: {
        NCBI_THROW( CAlgoAlignException, eInternal,
                    g_msg_InvalidTranscriptSymbol );
        }
    }

    const TNCBIScore (*sm) [NCBI_FSM_DIM] = m_ScoreMatrix.s;

    for(size_t i = 0; i < dim; ++i) {

        ETranscriptSymbol ts = transcript[i];

        switch(ts) {

        case eTS_Match:
        case eTS_Replace: {
            if(nucl_mode) {
                score += (ts == eTS_Match)? m_Wm: m_Wms;
            }
            else {
                unsigned char c1 = *p1;
                unsigned char c2 = *p2;
                score += sm[c1][c2];
                ++p1; ++p2;
            }
            state1 = state2 = 0;
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

            NCBI_THROW(CAlgoAlignException, eInternal,
                       g_msg_InvalidTranscriptSymbol);
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
                NCBI_THROW( CAlgoAlignException, eInternal,
                            g_msg_InvalidTranscriptSymbol );
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

/////////////////////////////////////////////////
/// naive pattern generator (a la Rabin-Karp)

struct nwaln_mrnaseg {
    nwaln_mrnaseg(size_t i1, size_t i2, unsigned char fp0):
        a(i1), b(i2), fp(fp0) {}
    size_t a, b;
    unsigned char fp;
};

struct nwaln_mrnaguide {
    nwaln_mrnaguide(size_t i1, size_t i2, size_t i3, size_t i4):
        q0(i1), q1(i2), s0(i3), s1(i4) {}
    size_t q0, q1, s0, s1;
};


unsigned char CNWAligner::x_CalcFingerPrint64(
    const char* beg, const char* end, size_t& err_index)
{
    if(beg >= end) {
        return 0xFF;
    }

    unsigned char fp = 0, code;
    for(const char* p = beg; p < end; ++p) {
        switch(*p) {
        case 'A': code = 0;    break;
        case 'G': code = 0x01; break;
        case 'T': code = 0x02; break;
        case 'C': code = 0x03; break;
        default:  err_index = p - beg; return 0x40; // incorrect char
        }
        fp = 0x3F & ((fp << 2) | code);
    }

    return fp;
}


const char* CNWAligner::x_FindFingerPrint64(
    const char* beg, const char* end,
    unsigned char fingerprint, size_t size,
    size_t& err_index)
{

    if(beg + size > end) {
        err_index = 0;
        return 0;
    }

    const char* p0 = beg;

    size_t err_idx = 0; --p0;
    unsigned char fp = 0x40;
    while(fp == 0x40 && p0 < end) {
        p0 += err_idx + 1;
        fp = x_CalcFingerPrint64(p0, p0 + size, err_idx);
    }

    if(p0 >= end) {
        return end;  // not found
    }
    
    unsigned char code;
    while(fp != fingerprint && ++p0 < end) {

        switch(*(p0 + size - 1)) {
        case 'A': code = 0;    break;
        case 'G': code = 0x01; break;
        case 'T': code = 0x02; break;
        case 'C': code = 0x03; break;
        default:  err_index = p0 + size - 1 - beg;
                  return 0;
        }
        
        fp = 0x3F & ((fp << 2) | code );
    }

    return p0;
}


size_t CNWAligner::MakePattern(const size_t guide_size,
                               const size_t guide_core)
{
    if(guide_core > guide_size) {
        NCBI_THROW(CAlgoAlignException,
                   eBadParameter,
                   g_msg_NullParameter);
    }

    vector<nwaln_mrnaseg> segs;

    size_t err_idx;
    for(size_t i = 0; i + guide_size <= m_SeqLen1; ) {
        const char* beg = m_Seq1 + i;
        const char* end = m_Seq1 + i + guide_size;
        unsigned char fp = x_CalcFingerPrint64(beg, end, err_idx);
        if(fp != 0x40) {
            segs.push_back(nwaln_mrnaseg(i, i + guide_size - 1, fp));
            i += guide_size;
        }
        else {
            i += err_idx + 1;
        }
    }

    vector<nwaln_mrnaguide> guides;
    size_t idx = 0;
    const char* beg = m_Seq2 + idx;
    const char* end = m_Seq2 + m_SeqLen2;
    for(size_t i = 0, seg_count = segs.size();
        beg + guide_size <= end && i < seg_count; ++i) {

        const char* p = 0;
        const char* beg0 = beg;
        while( p == 0 && beg + guide_size <= end ) {

            p = x_FindFingerPrint64( beg, end, segs[i].fp,
                                     guide_size, err_idx );
            if(p == 0) { // incorrect char
                beg += err_idx + 1; 
            }
            else if (p < end) {// fingerprints match but check actual sequences
                const char* seq1 = m_Seq1 + segs[i].a;
                const char* seq2 = p;
                size_t k;
                for(k = 0; k < guide_size; ++k) {
                    if(seq1[k] != seq2[k]) break;
                }
                if(k == guide_size) { // real match
                    size_t i1 = segs[i].a;
                    size_t i2 = segs[i].b;
                    size_t i3 = seq2 - m_Seq2;
                    size_t i4 = i3 + guide_size - 1;
                    size_t guides_dim = guides.size();
                    if( guides_dim == 0 ||
                        i1 - 1 > guides[guides_dim - 1].q1 ||
                        i3 - 1 > guides[guides_dim - 1].s1    ) {
                        guides.push_back(nwaln_mrnaguide(i1, i2, i3, i4));
                    }
                    else { // expand the last guide
                        guides[guides_dim - 1].q1 = i2;
                        guides[guides_dim - 1].s1 = i4;
                    }
                    beg0 = p + guide_size;
                }
                else {  // spurious match
                    beg = p + 1;
                    p = 0;
                }
            }
        }
        beg = beg0; // restore start pos in genomic sequence
    }

    // initialize m_guides
    size_t guides_dim = guides.size();
    m_guides.clear();
    m_guides.resize(4*guides_dim);
    const size_t offs = guide_core/2 - 1;
    for(size_t k = 0; k < guides_dim; ++k) {
        size_t q0 = (guides[k].q0 + guides[k].q1) / 2;
        size_t s0 = (guides[k].s0 + guides[k].s1) / 2;
        m_guides[4*k]         = q0 - offs;
        m_guides[4*k + 1]     = q0 + offs;
        m_guides[4*k + 2]     = s0 - offs;
        m_guides[4*k + 3]     = s0 + offs;
    }
 
    return m_guides.size();   
}

//////////////////////////////////////////////
/////////////////////////////////////////////

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
                NCBI_THROW( CAlgoAlignException, eInternal,
                            g_msg_InvalidTranscriptSymbol );
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
                NCBI_THROW( CAlgoAlignException, eInternal,
                            g_msg_InvalidTranscriptSymbol );
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
 * Revision 1.71  2006/08/24 13:43:06  kapustin
 * Set initial value for CNWAligner::m_terminate
 *
 * Revision 1.70  2006/03/21 16:21:51  kapustin
 * Move RLE code to xalgoalignutil
 *
 * Revision 1.69  2005/07/26 16:43:29  kapustin
 * Move MakePattern() to CNWAligner
 *
 * Revision 1.68  2005/06/03 16:22:01  lavr
 * Explicit (unsigned char) casts in ctype routines
 *
 * Revision 1.67  2005/06/02 15:01:52  kapustin
 * Use explicit flag to invalidate score matrix
 *
 * Revision 1.66  2005/06/02 14:18:02  kapustin
 * Invalidate score matrix after setting match and mismatch scores
 *
 * Revision 1.65  2005/05/24 19:35:36  kapustin
 * +CNWAligner::s_RunLength{En|De}Code()
 *
 * Revision 1.64  2005/04/04 16:34:13  kapustin
 * Specify precise type of diags in raw alignment transcripts where feasible
 *
 * Revision 1.63  2005/03/16 15:48:26  jcherry
 * Allow use of std::string for specifying sequences
 *
 * Revision 1.62  2005/02/23 16:59:38  kapustin
 * +CNWAligner::SetTranscript. Use CSeq_id's instead of strings in 
 * CNWFormatter. Modify CNWFormatter::AsSeqAlign to allow specification of 
 * alignment's starts and strands.
 *
 * Revision 1.61  2004/12/16 22:42:22  kapustin
 * Move to algo/align/nw
 *
 * Revision 1.60  2004/12/06 22:13:36  kapustin
 * File header update
 *
 * Revision 1.59  2004/11/29 14:37:15  kapustin
 * CNWAligner::GetTranscript now returns TTranscript and direction can be specified. x_ScoreByTanscript renamed to ScoreFromTranscript with two additional parameters to specify starting coordinates.
 *
 * Revision 1.58  2004/09/23 15:31:24  kapustin
 * Eliminate reading past sequence ends in x_ScoreByTranscript()
 *
 * Revision 1.57  2004/08/31 16:17:21  papadopo
 * make SAlignInOut work with sequence offsets rather than char pointers
 *
 * Revision 1.56  2004/08/30 18:27:31  kapustin
 * Avoid dynamic double-dimension for iupacna to make msvc happy
 *
 * Revision 1.55  2004/08/30 16:29:30  kapustin
 * Use the NCBI packed score matrix to initialize the unpacked
 * matrix for nucleotides
 *
 * Revision 1.54  2004/08/19 20:33:45  papadopo
 * mark lower-case and raw numerical characters as valid sequence data
 *
 * Revision 1.53  2004/08/18 21:48:56  kapustin
 * *** empty log message ***
 *
 * Revision 1.52  2004/07/07 21:38:30  kapustin
 * Check space before starting new thread
 *
 * Revision 1.51  2004/06/29 21:08:17  kapustin
 * +#include <algorithm>
 *
 * Revision 1.50  2004/06/29 20:51:21  kapustin
 * Support simultaneous segment computing
 *
 * Revision 1.49  2004/06/23 19:59:09  kapustin
 * Report incorrect sym position before the symbol
 *
 * Revision 1.48  2004/05/21 21:41:02  gorelenk
 * Added PCH ncbi_pch.hpp
 *
 * Revision 1.47  2004/05/19 13:37:47  kapustin
 * Remove test dumping code
 *
 * Revision 1.46  2004/05/18 21:43:40  kapustin
 * Code cleanup
 *
 * Revision 1.45  2004/05/17 14:50:56  kapustin
 * Add/remove/rearrange some includes and object declarations
 *
 * Revision 1.44  2004/04/23 14:39:47  kapustin
 * Add Splign library and other changes
 *
 * Revision 1.42  2003/12/29 13:03:48  kapustin
 * Return string from GetTranscriptString().
 *
 * Revision 1.41  2003/10/31 19:40:13  kapustin
 * Get rid of some WS and GCC complains
 *
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
