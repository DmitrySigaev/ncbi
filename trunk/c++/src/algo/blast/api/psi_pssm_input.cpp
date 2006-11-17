#ifndef SKIP_DOXYGEN_PROCESSING
static char const rcsid[] =
    "$Id$";
#endif /* SKIP_DOXYGEN_PROCESSING */
/* ===========================================================================
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
 */

/** @file psi_pssm_input.cpp
 * Implementation of the concrete strategy to obtain PSSM input data for
 * PSI-BLAST.
 */

#include <ncbi_pch.hpp>
#include <iomanip>

// BLAST includes
#include <algo/blast/api/psi_pssm_input.hpp>
#include <algo/blast/api/blast_exception.hpp>
#include "../core/blast_psi_priv.h"

// Object includes
#include <objects/seqalign/Seq_align.hpp>
#include <objects/seqalign/Seq_align_set.hpp>
#include <objects/seqalign/Dense_seg.hpp>
#include <objects/seqalign/Score.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_interval.hpp>

// Object manager includes
#include <objmgr/scope.hpp>
#include <objmgr/seq_vector.hpp>
#include <objects/seq/Seq_data.hpp>

#include "psiblast_aux_priv.hpp"

/** @addtogroup AlgoBlast
 *
 * @{
 */

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
BEGIN_SCOPE(blast)

#ifndef GAP_IN_ALIGNMENT
    /// Representation of GAP in Seq-align
#   define GAP_IN_ALIGNMENT     ((Uint4)-1)
#endif

//////////////////////////////////////////////////////////////////////////////

CPsiBlastInputData::CPsiBlastInputData(const unsigned char* query,
                                       unsigned int query_length,
                                       CConstRef<objects::CSeq_align_set> sset,
                                       CRef<objects::CScope> scope,
                                       const PSIBlastOptions& opts,
                                       const char* matrix_name,
                                       const PSIDiagnosticsRequest* diags)
{
    if ( !query ) {
        NCBI_THROW(CBlastException, eInvalidArgument, "NULL query");
    }

    if ( !sset || sset->Get().front()->GetDim() != 2) {
        NCBI_THROW(CBlastException, eNotSupported, 
                   "Only 2-dimensional alignments are supported");
    }

    m_Query = new Uint1[query_length];
    memcpy((void*) m_Query, (void*) query, query_length);

    m_Scope.Reset(scope);
    m_SeqAlignSet.Reset(sset);
    m_Opts = opts;

    m_MsaDimensions.query_length = query_length;
    m_MsaDimensions.num_seqs = 0;
    m_Msa = NULL;

    // Default value provided by base class
    m_MatrixName = string(matrix_name ? matrix_name : "");
    m_DiagnosticsRequest = const_cast<PSIDiagnosticsRequest*>(diags);
}

CPsiBlastInputData::~CPsiBlastInputData()
{
    delete [] m_Query;
    PSIMsaFree(m_Msa);
}

void
CPsiBlastInputData::Process()
{
    _ASSERT(m_Query != NULL);

    // Update the number of aligned sequences
    m_MsaDimensions.num_seqs = x_CountAndSelectQualifyingAlignments();

    // Create multiple alignment data structure and populate with query
    // sequence
    m_Msa = PSIMsaNew(&m_MsaDimensions);
    if ( !m_Msa ) {
        NCBI_THROW(CBlastSystemException, eOutOfMemory, 
                   "Multiple alignment data structure");
    }

    x_CopyQueryToMsa();
    x_ExtractAlignmentData();
}

unsigned int
CPsiBlastInputData::x_CountAndSelectQualifyingAlignments()
{
    CPsiBlastAlignmentProcessor proc;
    CPsiBlastAlignmentProcessor::THitIdentifiers hit_ids;
    proc(*m_SeqAlignSet, m_Opts.inclusion_ethresh, hit_ids);
    return hit_ids.size();
}

unsigned int
CPsiBlastInputData::GetNumAlignedSequences() const
{
    // Process() should result in this field being assigned a non-zero value
    _ASSERT(m_MsaDimensions.num_seqs != 0);
    return m_MsaDimensions.num_seqs;
}

inline PSIMsa* 
CPsiBlastInputData::GetData()
{ 
    return m_Msa;
}

inline unsigned char*
CPsiBlastInputData::GetQuery()
{
    return m_Query;
}

inline unsigned int
CPsiBlastInputData::GetQueryLength()
{
    return m_MsaDimensions.query_length;
}

inline const PSIBlastOptions*
CPsiBlastInputData::GetOptions()
{
    return &m_Opts;
}

inline const char*
CPsiBlastInputData::GetMatrixName()
{
    if (m_MatrixName.length() != 0) {
        return m_MatrixName.c_str();
    } else {
        return IPssmInputData::GetMatrixName();
    }
}

inline const PSIDiagnosticsRequest*
CPsiBlastInputData::GetDiagnosticsRequest()
{
    return m_DiagnosticsRequest;
}

#if 0
void
CPsiBlastInputData::x_ExtractAlignmentDataUseBestAlign()
{
    TSeqPos seq_index = 1; // Query sequence already processed

    // Note that in this implementation the m_ProcessHit vector is irrelevant
    // because we assume the Seq-align contains only those sequences selected
    // by the user (www-psi-blast). This could also be implemented by letting
    // the calling code populate a vector like m_ProcessHit or specifying a
    // vector of Seq-ids.
    ITERATE(list< CRef<CSeq_align> >, itr, m_SeqAlignSet->Get()) {

        const CSeq_align::C_Segs::TDisc::Tdata& hsp_list =
            (*itr)->GetSegs().GetDisc().Get();
        CSeq_align::C_Segs::TDisc::Tdata::const_iterator best_alignment;
        double min_evalue = numeric_limits<double>::max();

        // Search for the best alignment among all HSPs corresponding to this
        // query-subject pair (hit)
        ITERATE(CSeq_align::C_Segs::TDisc::Tdata, hsp_itr, hsp_list) {

            // Note: Std-seg can be converted to Denseg, will need 
            // conversion from Dendiag to Denseg too
            if ( !(*hsp_itr)->GetSegs().IsDenseg() ) {
                NCBI_THROW(CBlastException, eNotSupported, 
                           "Segment type not supported");
            }

            double evalue = s_GetLowestEvalue((*hsp_itr)->GetScore());
            if (evalue < min_evalue) {
                best_alignment = hsp_itr;
                min_evalue = evalue;
            }
        }
        _ASSERT(best_alignment != hsp_list.end());

        x_ProcessDenseg((*best_alignment)->GetSegs().GetDenseg(), 
                        seq_index, min_evalue);

        seq_index++;

    }

    _ASSERT(seq_index == GetNumAlignedSequences()+1);
}
#endif

void
CPsiBlastInputData::x_CopyQueryToMsa()
{
    _ASSERT(m_Msa);

    for (unsigned int i = 0; i < GetQueryLength(); i++) {
        m_Msa->data[kQueryIndex][i].letter = m_Query[i];
        m_Msa->data[kQueryIndex][i].is_aligned = true;
    }
}

void
CPsiBlastInputData::x_ExtractAlignmentData()
{
    // Index into multiple sequence alignment structure, query sequence 
    // already processed
    unsigned int msa_index = kQueryIndex + 1;  
    
    // For each hit...
    ITERATE(CSeq_align_set::Tdata, itr, m_SeqAlignSet->Get()) {
        _ASSERT((*itr)->GetSegs().IsDisc());
        bool qualifying_seqid = false;

        // for each HSP...
        ITERATE(CSeq_align::C_Segs::TDisc::Tdata, hsp_itr,
                (*itr)->GetSegs().GetDisc().Get()) {

            // Note: Std-seg (for tblastn results) can be converted to 
            // Dense-seg, but this will need conversion from Dense-diag to 
            // Dense-seg too
            if ( !(*hsp_itr)->GetSegs().IsDenseg() ) {
                string msg("Segment type ");
                msg += NStr::IntToString((*hsp_itr)->GetSegs().Which());
                msg += string(" not supported");
                NCBI_THROW(CBlastException, eNotSupported, msg);
            }

            double evalue = GetLowestEvalue((*hsp_itr)->GetScore());
            // ... below the e-value inclusion threshold
            if (evalue < m_Opts.inclusion_ethresh) {
                _ASSERT(msa_index < GetNumAlignedSequences() + 1);
                x_ProcessDenseg((*hsp_itr)->GetSegs().GetDenseg(), msa_index);
                qualifying_seqid = true;
            }
        }
        if (qualifying_seqid) {
            msa_index++;
        }
    }
}

void
CPsiBlastInputData::x_ProcessDenseg(const objects::CDense_seg& denseg, 
                                    unsigned int msa_index)
{
    _ASSERT(denseg.GetDim() == 2);

    const Uint1 GAP = AMINOACID_TO_NCBISTDAA[(Uint1)'-'];
    const vector<TSignedSeqPos>& starts = denseg.GetStarts();
    const vector<TSeqPos>& lengths = denseg.GetLens();
    const int kNumSegments = denseg.GetNumseg();
    const TSeqPos kDimensions = denseg.GetDim();
    TSeqPos query_index = 0;        // index into starts vector
    TSeqPos subj_index = 1;         // index into starts vector
    TSeqPos subj_seq_idx = 0;       // index into subject sequence buffer
    string seq;                     // the sequence data

    // Get the portion of the subject sequence corresponding to this Dense-seg
    x_GetSubjectSequence(denseg, *m_Scope, seq);

    // if this isn't available, set its corresponding row in the multiple
    // sequence alignment to the query sequence so that it can be purged in
    // PSIPurgeMatrix -> This is a hack, it should withdraw the sequence from
    // the multiple sequence alignment structure!
    if (seq.size() == 0) {
        for (unsigned int i = 0; i < GetQueryLength(); i++) {
            m_Msa->data[msa_index][i].letter = m_Query[i];
            m_Msa->data[msa_index][i].is_aligned = true;
        }
        return;
    }

    // Iterate over all segments
    for (int segmt_idx = 0; segmt_idx < kNumSegments; segmt_idx++) {

        TSeqPos query_offset = starts[query_index];
        TSeqPos subject_offset = starts[subj_index];

        // advance the query and subject indices for next iteration
        query_index += kDimensions;
        subj_index += kDimensions;

        if (query_offset == GAP_IN_ALIGNMENT) {

            // gap in query, just skip residues on subject sequence
            subj_seq_idx += lengths[segmt_idx];
            continue;

        } else if (subject_offset == GAP_IN_ALIGNMENT) {

            // gap in subject, initialize appropriately
            for (TSeqPos i = 0; i < lengths[segmt_idx]; i++) {
                PSIMsaCell& msa_cell = m_Msa->data[msa_index][query_offset++];
                if ( !msa_cell.is_aligned ) {
                    msa_cell.letter = GAP;
                    msa_cell.is_aligned = true;
                }
            }

        } else {

            // Aligned segments without any gaps
            for (TSeqPos i = 0; i < lengths[segmt_idx]; i++, subj_seq_idx++) {
                PSIMsaCell& msa_cell =
                    m_Msa->data[msa_index][query_offset++];
                if ( !msa_cell.is_aligned ) {
                    msa_cell.letter = static_cast<Uint1>(seq[subj_seq_idx]);
                    msa_cell.is_aligned = true;
                }
            }
        }

    }

}

void
CPsiBlastInputData::x_GetSubjectSequence(const objects::CDense_seg& ds, 
                                         objects::CScope& scope,
                                         string& sequence_data) 
{
    _ASSERT(ds.GetDim() == 2);
    TSeqPos subjlen = 0;                    // length of the return value
    TSeqPos subj_start = kInvalidSeqPos;    // start of subject alignment
    bool subj_start_found = false;
    const int kNumSegments = ds.GetNumseg();
    const TSeqPos kDimensions = ds.GetDim();
    TSeqPos subj_index = 1;                 // index into starts vector

    const vector<TSignedSeqPos>& starts = ds.GetStarts();
    const vector<TSeqPos>& lengths = ds.GetLens();

    for (int i = 0; i < kNumSegments; i++) {

        if (starts[subj_index] != (TSignedSeqPos)GAP_IN_ALIGNMENT) {
            if ( !subj_start_found ) {
                subj_start = starts[subj_index];
                subj_start_found = true;
            }
            subjlen += lengths[i];
        }

        subj_index += kDimensions;
    }
    _ASSERT(subj_start_found);

    CSeq_loc seqloc(const_cast<CSeq_id&>(*ds.GetIds().back()), subj_start, 
                    subj_start+subjlen-1);

    try {
        CSeqVector sv(seqloc, scope);
        sv.SetCoding(CSeq_data::e_Ncbistdaa);
        sv.GetSeqData(0, kInvalidSeqPos, sequence_data);
    } catch (const CException&) {
        sequence_data.clear();
        ERR_POST(Warning << "Failed to retrieve sequence " <<
                 seqloc.GetInt().GetId().AsFastaString());
    }
}

END_SCOPE(blast)
END_NCBI_SCOPE

/* @} */

/*
 * ===========================================================================
 *
 * $Log$
 * Revision 1.17  2006/11/17 17:22:02  camacho
 * Return sequence data in a std::string
 *
 * Revision 1.16  2006/11/14 15:20:22  camacho
 * m_ProcessHit no longer needed, some optimizations
 *
 * Revision 1.15  2006/06/14 15:58:54  camacho
 * Replace ASSERT (defined in CORE) for _ASSERT (defined by C++ toolkit)
 *
 * Revision 1.14  2005/11/14 15:24:48  camacho
 * Implemented alignment processor to extract relevant sequences for PSSM generation
 *
 * Revision 1.13  2005/07/07 16:32:12  camacho
 * Revamping of BLAST exception classes and error codes
 *
 * Revision 1.12  2005/01/26 14:04:05  camacho
 * Fix compiler warning
 *
 * Revision 1.11  2004/12/28 16:47:43  camacho
 * 1. Use typedefs to AutoPtr consistently
 * 2. Remove exception specification from blast::SetupQueries
 * 3. Use SBlastSequence structure instead of std::pair as return value to
 *    blast::GetSequence
 *
 * Revision 1.10  2004/12/06 17:54:09  grichenk
 * Replaced calls to deprecated methods
 *
 * Revision 1.9  2004/11/02 20:37:34  camacho
 * Doxygen fixes
 *
 * Revision 1.8  2004/10/13 20:49:00  camacho
 * + support for requesting diagnostics information and specifying underlying matrix
 *
 * Revision 1.7  2004/08/13 22:33:06  camacho
 * Change to be backwards compatible with old pssm engine
 *
 * Revision 1.6  2004/08/04 21:49:01  camacho
 * Minor change to avoid compiler warning
 *
 * Revision 1.5  2004/08/04 21:12:47  camacho
 * Removed dead code, unused variable
 *
 * Revision 1.4  2004/08/04 20:24:53  camacho
 * Renaming of core PSSM structures and removal of unneeded fields.
 *
 * Revision 1.3  2004/08/02 13:29:53  camacho
 * + implementation query sequence methods
 *
 * Revision 1.2  2004/07/29 21:00:22  ucko
 * Tweak call to assign() in constructor to avoid triggering an
 * inappropriate template (that takes two iterators) on some compilers.
 *
 * Revision 1.1  2004/07/29 17:55:05  camacho
 * Initial revision
 *
 * ===========================================================================
 */
