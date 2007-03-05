static char const rcsid[] = "$Id$";

/*
* ===========================================================================
*
*                            PUBLIC DOMAIN NOTICE
*               National Center for Biotechnology Information
*
*  This software/database is a "United States Government Work" under the
*  terms of the United States Copyright Act.  It was written as part of
*  the author's offical duties as a United States Government employee and
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
* ===========================================================================*/

/*****************************************************************************

File name: seq.cpp

Author: Jason Papadopoulos

Contents: implementation of CSequence class

******************************************************************************/

#include <ncbi_pch.hpp>
#include <objmgr/seq_vector.hpp>
#include <algo/cobalt/seq.hpp>

/// @file seq.cpp
/// Implementation of CSequence class

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(cobalt)

unsigned char
CSequence::GetPrintableLetter(int pos) const
{
    _ASSERT(pos >= 0 && pos < GetLength());

    int val;
    switch (m_Sequence[pos]) {
    case 0: val = '-'; break;
    case 1: val = 'A'; break;
    case 2: val = 'B'; break;
    case 3: val = 'C'; break;
    case 4: val = 'D'; break;
    case 5: val = 'E'; break;
    case 6: val = 'F'; break;
    case 7: val = 'G'; break;
    case 8: val = 'H'; break;
    case 9: val = 'I'; break;
    case 10: val = 'K'; break;
    case 11: val = 'L'; break;
    case 12: val = 'M'; break;
    case 13: val = 'N'; break;
    case 14: val = 'P'; break;
    case 15: val = 'Q'; break;
    case 16: val = 'R'; break;
    case 17: val = 'S'; break;
    case 18: val = 'T'; break;
    case 19: val = 'V'; break;
    case 20: val = 'W'; break;
    case 21: val = 'X'; break;
    case 22: val = 'Y'; break;
    case 23: val = 'Z'; break;
    case 24: val = 'U'; break;
    case 25: val = '*'; break;
    default: val = '?'; break;
    }

    return val;
}

void 
CSequence::Reset(const blast::SSeqLoc& seq_in)
{
    if (!seq_in.seqloc->IsWhole() && !seq_in.seqloc->IsInt()) {
        NCBI_THROW(CMultiAlignerException, eInvalidInput,
                   "Unsupported SeqLoc encountered");
    }

    objects::CSeqVector sv(*seq_in.seqloc, *seq_in.scope);

    if (!sv.IsProtein()) {
        NCBI_THROW(CMultiAlignerException, eInvalidInput,
                   "Nucleotide sequences cannot be aligned");
    }

    // make a local copy of the sequence data

    int seq_length = sv.size();
    m_Sequence.resize(seq_length);
    for (int i = 0; i < seq_length; i++) {
        m_Sequence[i] = sv[i];
    }

    // residue frequencies start off empty

    m_Freqs.Resize(seq_length, kAlphabetSize);
    m_Freqs.Set(0.0);
}


CSequence::CSequence(const blast::SSeqLoc& sl) 
{
    Reset(sl);
}


void 
CSequence::PropagateGaps(const CNWAligner::TTranscript& transcript,
                         CNWAligner::ETranscriptSymbol gap_choice)
{
    int new_size = transcript.size();

    // no gaps means nothing needs updating

    if (new_size == GetLength()) {
        return;
    }

    vector<unsigned char> new_seq(new_size);
    TFreqMatrix new_freq(new_size, kAlphabetSize, 0.0);

    // expand the sequence data and the profile columns
    // to incorporate new gaps

    for (int i = 0, j = 0; i < new_size; i++) {
        if (transcript[i] == gap_choice) {
            new_seq[i] = kGapChar;
        }
        else {
            new_seq[i] = m_Sequence[j];
            for (int k = 0; k < kAlphabetSize; k++)
                new_freq(i, k) = m_Freqs(j, k);
            _ASSERT(j < GetLength());
            j++;
        }
    }

    // replace class data

    m_Sequence.swap(new_seq);
    m_Freqs.Swap(new_freq);
}

void CSequence::CompressSequences(vector<CSequence>& seq,
                                  vector<int> index_list)
{
    int align_length = seq[index_list[0]].GetLength();
    int num_seqs = index_list.size();
    int new_length = 0;

    // for each alignment column

    for (int i = 0; i < align_length; i++) {
        int j;
        for (j = 0; j < num_seqs; j++) {
            if (seq[index_list[j]].m_Sequence[i] != kGapChar)
                break;
        }

        // if the specified list of sequences do not all 
        // have a gap character in column i, keep the column
        
        if (j < num_seqs) {
            for (j = 0; j < num_seqs; j++) {
                seq[index_list[j]].m_Sequence[new_length] =
                                 seq[index_list[j]].m_Sequence[i];
                for (int k = 0; k < kAlphabetSize; k++) {
                    seq[index_list[j]].m_Freqs(new_length, k) =
                                seq[index_list[j]].m_Freqs(i, k);
                }
            }
            new_length++;
        }
    }

    // if the length changed, shorten m_Sequence and m_Freqs

    if (new_length != align_length) {
        for (int i = 0; i < num_seqs; i++) {
            seq[index_list[i]].m_Sequence.resize(new_length);
            seq[index_list[i]].m_Freqs.Resize(new_length, kAlphabetSize);
        }
    }
}

END_SCOPE(cobalt)
END_NCBI_SCOPE
