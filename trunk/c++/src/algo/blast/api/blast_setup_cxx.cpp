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
 * ===========================================================================
 */

/// @file blast_setup_cxx.cpp
/// Auxiliary setup functions for Blast objects interface.

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/metareg.hpp>
#include <algo/blast/api/blast_options.hpp>
#include <algo/blast/core/blast_setup.h>
#include <algo/blast/core/blast_util.h>

#include "blast_setup.hpp"

/** @addtogroup AlgoBlast
 *
 * @{
 */

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
BEGIN_SCOPE(blast)

/** Set field values for one element of the context array of a
 * concatenated query.  All previous contexts should have already been
 * assigned correct values.
 * @param qinfo  Query info structure containing contexts. [in/out]
 * @param index  Index of the context to fill. [in]
 * @param length Length of this context. [in]
 */
static void
s_QueryInfo_SetContext(BlastQueryInfo*   qinfo,
                       Uint4             index,
                       Uint4             length)
{
    ASSERT(index <= static_cast<Uint4>(qinfo->last_context));
    
    if (index) {
        Uint4 prev_loc = qinfo->contexts[index-1].query_offset;
        Uint4 prev_len = qinfo->contexts[index-1].query_length;
        
        Uint4 shift = prev_len ? prev_len + 1 : 0;
        
        qinfo->contexts[index].query_offset = prev_loc + shift;
        qinfo->contexts[index].query_length = length;
    } else {
        // First context
        qinfo->contexts[0].query_offset = 0;
        qinfo->contexts[0].query_length = length;
    }
}

/// Adjust first context depending on the first query strand
static void
s_AdjustFirstContext(BlastQueryInfo* query_info, 
                     EBlastProgramType prog,
                     ENa_strand strand_opt,
                     const IBlastQuerySource& queries)
{
    ASSERT(query_info);

    bool is_na = (prog == eBlastTypeBlastn) ? true : false;
	bool translate = Blast_QueryIsTranslated(prog) ? true : false;

    ASSERT(is_na || translate);

    // Only if the strand specified by the options is NOT both or unknown,
    // it takes precedence over what is specified by the query's strand
    ENa_strand strand = (strand_opt == eNa_strand_both || 
                         strand_opt == eNa_strand_unknown) 
        ? queries.GetStrand(0)
        : strand_opt;

    // Adjust the first context if the requested strand is the minus strand
    if (strand == eNa_strand_minus) {
        query_info->first_context = translate ? 3 : 1;
    }
}

void
SetupQueryInfo_OMF(const IBlastQuerySource& queries,
                   EBlastProgramType prog,
                   ENa_strand strand_opt,
                   BlastQueryInfo** qinfo)
{
    ASSERT(qinfo);
    CBlastQueryInfo query_info(BlastQueryInfoNew(prog, queries.Size()));
    if (query_info.Get() == NULL) {
        NCBI_THROW(CBlastSystemException, eOutOfMemory, "Query info");
    }

    const unsigned int kNumContexts = GetNumberOfContexts(prog);
    bool is_na = (prog == eBlastTypeBlastn) ? true : false;
	bool translate = Blast_QueryIsTranslated(prog) ? true : false;

    if (is_na || translate) {
        s_AdjustFirstContext(query_info, prog, strand_opt, queries);
    }

    // Set up the context offsets into the sequence that will be added
    // to the sequence block structure.
    unsigned int ctx_index = 0; // index into BlastQueryInfo::contexts array
    // Longest query length, to be saved in the query info structure
    Uint4 max_length = 0;

    for(TSeqPos j = 0; j < queries.Size(); j++) {
        TSeqPos length = 0;
        try { length = queries.GetLength(j); }
        catch (const CException&) { 
            // Ignore exceptions in this function as they will be caught in
            // SetupQueries
        }

        ENa_strand strand = queries.GetStrand(j);
        
        if (strand_opt == eNa_strand_minus || strand_opt == eNa_strand_plus) {
            strand = strand_opt;
        }

        if (translate) {
            for (unsigned int i = 0; i < kNumContexts; i++) {
                unsigned int prot_length = 
                    (length == 0 ? 0 : 
                     (length - i % CODON_LENGTH) / CODON_LENGTH);
                max_length = MAX(max_length, prot_length);
                
                Uint4 ctx_len(0);
                
                switch (strand) {
                case eNa_strand_plus:
                    ctx_len = (i<3) ? prot_length : 0;
                    s_QueryInfo_SetContext(query_info, ctx_index + i, ctx_len);
                    break;

                case eNa_strand_minus:
                    ctx_len = (i<3) ? 0 : prot_length;
                    s_QueryInfo_SetContext(query_info, ctx_index + i, ctx_len);
                    break;

                case eNa_strand_both:
                case eNa_strand_unknown:
                    s_QueryInfo_SetContext(query_info, ctx_index + i, 
                                           prot_length);
                    break;

                default:
                    abort();
                }
            }
        } else {
            max_length = MAX(max_length, length);
            
            if (is_na) {
                switch (strand) {
                case eNa_strand_plus:
                    s_QueryInfo_SetContext(query_info, ctx_index, length);
                    s_QueryInfo_SetContext(query_info, ctx_index+1, 0);
                    break;

                case eNa_strand_minus:
                    s_QueryInfo_SetContext(query_info, ctx_index, 0);
                    s_QueryInfo_SetContext(query_info, ctx_index+1, length);
                    break;

                case eNa_strand_both:
                case eNa_strand_unknown:
                    s_QueryInfo_SetContext(query_info, ctx_index, length);
                    s_QueryInfo_SetContext(query_info, ctx_index+1, length);
                    break;

                default:
                    abort();
                }
            } else {    // protein
                s_QueryInfo_SetContext(query_info, ctx_index, length);
            }
        }
        ctx_index += kNumContexts;
    }
    query_info->max_length = max_length;
    *qinfo = query_info.Release();
}

static void
s_ComputeStartEndContexts(ENa_strand   strand,
                          int          num_contexts,
                          int        & start,
                          int        & end)
{
    start = end = num_contexts;
    
    switch (strand) {
    case eNa_strand_minus: 
        start = num_contexts/2; 
        end = num_contexts;
        break;
    case eNa_strand_plus: 
        start = 0; 
        end = num_contexts/2;
        break;
    case eNa_strand_both:
        start = 0;
        end = num_contexts;
        break;
    default:
        abort();
    }
}

static void
s_AddMask(EBlastProgramType prog,
          BlastMaskLoc* mask,
          int query_index,
          BlastSeqLoc* core_seqloc,
          ENa_strand strand,
          TSeqPos query_length)
{
    ASSERT(query_index < mask->total_size);
    unsigned num_contexts = GetNumberOfContexts(prog);
    
    if (Blast_QueryIsTranslated(prog)) {
        int starting_context(0), ending_context(0);
        
        s_ComputeStartEndContexts(strand,
                                  num_contexts,
                                  starting_context,
                                  ending_context);
        
        const TSeqPos dna_length = query_length;
        BlastSeqLoc** frames_seqloc = 
            &(mask->seqloc_array[query_index*num_contexts]);
        for (int i = starting_context; i < ending_context; i++) {
            BlastSeqLoc* prot_seqloc(0);
            for (BlastSeqLoc* itr = core_seqloc; itr; itr = itr->next) {
                short frame = BLAST_ContextToFrame(eBlastTypeBlastx, i);
                TSeqPos to, from;
                if (frame < 0) {
                    from = (dna_length + frame - itr->ssr->right)/CODON_LENGTH;
                    to = (dna_length + frame - itr->ssr->left)/CODON_LENGTH;
                } else {
                    from = (itr->ssr->left - frame + 1)/CODON_LENGTH;
                    to = (itr->ssr->right - frame + 1)/CODON_LENGTH;
                }
                BlastSeqLocNew(&prot_seqloc, from, to);
            }
            frames_seqloc[i] = prot_seqloc;
        }

        BlastSeqLocFree(core_seqloc);

    } else if (Blast_QueryIsNucleotide(prog) && 
               !Blast_ProgramIsPhiBlast(prog)) {
        
        switch (strand) {
        case eNa_strand_plus:
            mask->seqloc_array[query_index*num_contexts] = core_seqloc;
            break;

        case eNa_strand_minus:
            mask->seqloc_array[query_index*num_contexts+1] = core_seqloc;
            break;

        case eNa_strand_both:
            mask->seqloc_array[query_index*num_contexts] = core_seqloc;
            mask->seqloc_array[query_index*num_contexts+1] =
                BlastSeqLocListDup(core_seqloc);
            break;

        default:
            abort();
        }
    } else  {
        mask->seqloc_array[query_index] = core_seqloc;
    }
}

static void
s_AddMask(EBlastProgramType           prog,
          BlastMaskLoc              * mask,
          int                         query_index,
          CBlastQueryFilteredFrames & seqloc_frames,
          ENa_strand                  strand,
          TSeqPos                     query_length)
{
    ASSERT(query_index < mask->total_size);
    unsigned num_contexts = GetNumberOfContexts(prog);
    
    if (Blast_QueryIsTranslated(prog)) {
        assert(seqloc_frames.QueryIsMulti());
        
        int starting_context(0), ending_context(0);
        
        s_ComputeStartEndContexts(strand,
                                  num_contexts,
                                  starting_context,
                                  ending_context);
        
        const TSeqPos dna_length = query_length;
        
        BlastSeqLoc** frames_seqloc = 
            & (mask->seqloc_array[query_index*num_contexts]);
        
        seqloc_frames.UseProteinCoords(dna_length);
            
        for (int i = starting_context; i < ending_context; i++) {
            short frame = BLAST_ContextToFrame(eBlastTypeBlastx, i);
            frames_seqloc[i] = *seqloc_frames[frame];
            seqloc_frames.Release(frame);
        }
    } else if (Blast_QueryIsNucleotide(prog) && 
               !Blast_ProgramIsPhiBlast(prog)) {
        
        int posframe = CSeqLocInfo::eFramePlus1;
        int negframe = CSeqLocInfo::eFrameMinus1;
        
        if (*seqloc_frames[posframe]) {
            assert(*seqloc_frames[negframe]);
        }
        
        switch (strand) {
        case eNa_strand_plus:
            mask->seqloc_array[query_index*num_contexts] =
                *seqloc_frames[posframe];
            seqloc_frames.Release(posframe);
            break;
            
        case eNa_strand_minus:
            mask->seqloc_array[query_index*num_contexts+1] =
                *seqloc_frames[negframe];
            seqloc_frames.Release(negframe);
            break;
            
        case eNa_strand_both:
            mask->seqloc_array[query_index*num_contexts] =
                *seqloc_frames[posframe];
            
            mask->seqloc_array[query_index*num_contexts+1] =
                *seqloc_frames[negframe];
            
            seqloc_frames.Release(posframe);
            seqloc_frames.Release(negframe);
            break;
            
        default:
            abort();
        }
        
    } else {
        mask->seqloc_array[query_index] = *seqloc_frames[0];
        seqloc_frames.Release(0);
    }
}

void
s_RestrictSeqLocs_OneFrame(BlastSeqLoc             ** bsl,
                           const IBlastQuerySource  & queries,
                           int                        query_index,
                           const BlastQueryInfo     * qinfo)
{
    for(int ci = qinfo->first_context; ci <= qinfo->last_context; ci++) {
        if (qinfo->contexts[ci].query_index == query_index) {
            CConstRef<CSeq_loc> qseqloc = queries.GetSeqLoc(query_index);
            
            BlastSeqLoc_RestrictToInterval(bsl,
                                           qseqloc->GetStart(eExtreme_Positional),
                                           qseqloc->GetStop (eExtreme_Positional));
            
            break;
        }
    }
}

void
s_RestrictSeqLocs_Multiframe(CBlastQueryFilteredFrames & frame_to_bsl,
                             const IBlastQuerySource   & queries,
                             int                         query_index,
                             const BlastQueryInfo      * qinfo)
{
    typedef vector<CSeqLocInfo::ETranslationFrame> TFrameVec;
    
    TFrameVec frames = frame_to_bsl.ListFrames();
    
    ITERATE(TFrameVec, iter, frames) {
        int seqloc_frame = *iter;
        BlastSeqLoc ** bsl = frame_to_bsl[seqloc_frame];
        
        for(int ci = qinfo->first_context; ci <= qinfo->last_context; ci++) {
            if (qinfo->contexts[ci].query_index != query_index)
                continue;
            
            int context_frame = qinfo->contexts[ci].frame;
            
            if (context_frame == seqloc_frame) {
                CConstRef<CSeq_loc> qseqloc = queries.GetSeqLoc(query_index);
                    
                BlastSeqLoc_RestrictToInterval(bsl,
                                               qseqloc->GetStart(eExtreme_Positional),
                                               qseqloc->GetStop (eExtreme_Positional));
                    
                break;
            }
        }
    }
}

CRef<CBlastQueryFilteredFrames>
s_GetRestrictedBlastSeqLocs(IBlastQuerySource & queries,
                            int                       query_index,
                            const BlastQueryInfo    * qinfo,
                            EBlastProgramType         program)
{
    TMaskedQueryRegions mqr =
        queries.GetMaskedRegions(query_index);
    
    CRef<CBlastQueryFilteredFrames> frame_to_bsl
        (new CBlastQueryFilteredFrames(program, mqr));
    
    if (! frame_to_bsl->Empty()) {
        if (frame_to_bsl->QueryIsMulti()) {
            s_RestrictSeqLocs_Multiframe(*frame_to_bsl,
                                         queries,
                                         query_index,
                                         qinfo);
        } else {
            s_RestrictSeqLocs_OneFrame((*frame_to_bsl)[0],
                                       queries,
                                       query_index,
                                       qinfo);
        }
    }
    
    return frame_to_bsl;
}

static void
s_InvalidateQueryContexts(BlastQueryInfo* qinfo, int query_index)
{
    ASSERT(qinfo);
    for (int i = qinfo->first_context; i <= qinfo->last_context; i++) {
        if (qinfo->contexts[i].query_index == query_index) {
            qinfo->contexts[i].is_valid = FALSE;
        }
    }
}

void
SetupQueries_OMF(IBlastQuerySource& queries, 
                 BlastQueryInfo* qinfo, 
                 BLAST_SequenceBlk** seqblk,
                 EBlastProgramType prog,
                 ENa_strand strand_opt,
                 const Uint1* genetic_code,
                 TSearchMessages& messages)
{
    ASSERT(seqblk);
    ASSERT( !queries.Empty() );
    if (messages.size() != (size_t)queries.Size()) {
        messages.resize(queries.Size());
    }

    EBlastEncoding encoding = GetQueryEncoding(prog);
    
    int buflen = QueryInfo_GetSeqBufLen(qinfo);
    TAutoUint1Ptr buf((Uint1*) calloc(buflen+1, sizeof(Uint1)));
    
    if ( !buf ) {
        NCBI_THROW(CBlastSystemException, eOutOfMemory, 
                   "Query sequence buffer");
    }

    bool is_na = (prog == eBlastTypeBlastn) ? true : false;
	bool translate = Blast_QueryIsTranslated(prog) ? true : false;

    unsigned int ctx_index = 0;      // index into context_offsets array
    const unsigned int kNumContexts = GetNumberOfContexts(prog);

    CBlastMaskLoc mask(BlastMaskLocNew(qinfo->num_queries*kNumContexts));

    for(TSeqPos index = 0; index < queries.Size(); index++) {
        ENa_strand strand = eNa_strand_unknown;
        
        try {

            if ((is_na || translate) &&
                (strand_opt == eNa_strand_unknown || 
                 strand_opt == eNa_strand_both)) 
            {
                strand = queries.GetStrand(index);
                // The default for nucleotide queries is both strands
                // FIXME: this should be handled inside the GetStrand() call
                // above
                if (strand == eNa_strand_unknown) {
                    strand = eNa_strand_both;
                }
            } else {
                strand = strand_opt;
            }
            
            CRef<CBlastQueryFilteredFrames> frame_to_bsl = 
                s_GetRestrictedBlastSeqLocs(queries, index, qinfo, prog);
            
            // Set the id if this is possible
            if (const CSeq_id* id = queries.GetSeqId(index)) {
                messages[index].SetQueryId(id->AsFastaString());
            }

            SBlastSequence sequence;
            
            if (translate) {
                ASSERT(strand == eNa_strand_both ||
                       strand == eNa_strand_plus ||
                       strand == eNa_strand_minus);
                ASSERT(Blast_QueryIsTranslated(prog));

                // Get both strands of the original nucleotide sequence with
                // sentinels
                sequence = queries.GetBlastSequence(index, encoding, strand, 
                                                    eSentinels);
                
                int na_length = queries.GetLength(index);
                Uint1* seqbuf_rev = NULL;  // negative strand
                if (strand == eNa_strand_both)
                   seqbuf_rev = sequence.data.get() + na_length + 1;
                else if (strand == eNa_strand_minus)
                   seqbuf_rev = sequence.data.get();

                // Populate the sequence buffer
                for (unsigned int i = 0; i < kNumContexts; i++) {
                    if (qinfo->contexts[ctx_index + i].query_length <= 0) {
                        continue;
                    }
                    
                    int offset = qinfo->contexts[ctx_index + i].query_offset;
                    BLAST_GetTranslation(sequence.data.get() + 1,
                                         seqbuf_rev,
                                         na_length,
                                         qinfo->contexts[ctx_index + i].frame,
                                         & buf.get()[offset],
                                         genetic_code);
                }

            } else if (is_na) {

                ASSERT(strand == eNa_strand_both ||
                       strand == eNa_strand_plus ||
                       strand == eNa_strand_minus);
                
                sequence = queries.GetBlastSequence(index, encoding, strand, 
                                                    eSentinels);
                
                int idx = (strand == eNa_strand_minus) ? 
                    ctx_index + 1 : ctx_index;

                int offset = qinfo->contexts[idx].query_offset;
                memcpy(&buf.get()[offset], sequence.data.get(), 
                       sequence.length);

            } else {

                string warnings;
                sequence = queries.GetBlastSequence(index,
                                                    encoding,
                                                    eNa_strand_unknown,
                                                    eSentinels,
                                                    &warnings);
                
                int offset = qinfo->contexts[ctx_index].query_offset;
                memcpy(&buf.get()[offset], sequence.data.get(), 
                       sequence.length);
                if ( !warnings.empty() ) {
                    // FIXME: is index this the right value for the 2nd arg?
                    CRef<CSearchMessage> m
                        (new CSearchMessage(eBlastSevWarning, index, warnings));
                    messages[index].push_back(m);
                }
            }

            TSeqPos qlen = BlastQueryInfoGetQueryLength(qinfo, prog, index);
            
            s_AddMask(prog, mask, index, *frame_to_bsl, strand, qlen);
            
            // AddMask releases the elements of frame_to_bsl that it uses;
            // the rest are freed by frame_to_bsl in the destructor.
        
        } catch (const CException& e) {
            // FIXME: is index this the right value for the 2nd arg?
            CRef<CSearchMessage> m
                (new CSearchMessage(eBlastSevWarning, index, 
                                    e.ReportThis(eDPF_ErrCodeExplanation)));
            messages[index].push_back(m);
            s_InvalidateQueryContexts(qinfo, index);
        }
        
        ctx_index += kNumContexts;
    }
    
    if (BlastSeqBlkNew(seqblk) < 0) {
        NCBI_THROW(CBlastSystemException, eOutOfMemory, "Query sequence block");
    }
    
    BlastSeqBlkSetSequence(*seqblk, buf.release(), buflen - 2);
    
    (*seqblk)->lcase_mask = mask.Release();
    (*seqblk)->lcase_mask_allocated = TRUE;
}

void
SetupSubjects_OMF(IBlastQuerySource& subjects,
                  EBlastProgramType prog,
                  vector<BLAST_SequenceBlk*>* seqblk_vec,
                  unsigned int* max_subjlen)
{
    ASSERT(seqblk_vec);
    ASSERT(max_subjlen);
    ASSERT(!subjects.Empty());

    // Nucleotide subject sequences are stored in ncbi2na format, but the
    // uncompressed format (ncbi4na/blastna) is also kept to re-evaluate with
    // the ambiguities
	bool subj_is_na = Blast_SubjectIsNucleotide(prog) ? true : false;

    ESentinelType sentinels = eSentinels;
    if (prog == eBlastTypeTblastn || prog == eBlastTypeTblastx) {
        sentinels = eNoSentinels;
    }

    EBlastEncoding encoding = GetSubjectEncoding(prog);
       
    // TODO: Should strand selection on the subject sequences be allowed?
    //ENa_strand strand = options->GetStrandOption(); 

    *max_subjlen = 0;

    for (TSeqPos i = 0; i < subjects.Size(); i++) {
        BLAST_SequenceBlk* subj = NULL;

        SBlastSequence sequence =
            subjects.GetBlastSequence(i, encoding, 
                                      eNa_strand_plus, sentinels);

        if (BlastSeqBlkNew(&subj) < 0) {
            NCBI_THROW(CBlastSystemException, eOutOfMemory, 
                       "Subject sequence block");
        }

        /* Set the lower case mask, if it exists */
        if (subjects.GetMask(i).NotEmpty()) {
            // N.B.: only one BlastMaskLoc structure is needed per subject
            CBlastMaskLoc mask(BlastMaskLocNew(GetNumberOfContexts(prog)));
            const int kSubjectMaskIndex(0);
            const ENa_strand kSubjectStrands2Search(eNa_strand_both);
            
            // Note: this could, and probably will, be modified to use
            // the types introduced with CBlastQueryVector, as done in
            // SetupQueries; it would allow removal of the older
            // version of s_AddMask(), but would not affect output
            // unless CBl2Seq is modified to use the CBlastQueryVector
            // code.
            
            s_AddMask(prog, mask,
                      kSubjectMaskIndex,
                      CSeqLoc2BlastSeqLoc(subjects.GetMask(i)),
                      kSubjectStrands2Search,
                      subjects.GetLength(i));
            
            subj->lcase_mask = mask.Release();
            subj->lcase_mask_allocated = TRUE;
        }

        if (subj_is_na) {
            BlastSeqBlkSetSequence(subj, sequence.data.release(), 
               ((sentinels == eSentinels) ? sequence.length - 2 :
                sequence.length));

            try {
                // Get the compressed sequence
                SBlastSequence compressed_seq =
                    subjects.GetBlastSequence(i, eBlastEncodingNcbi2na, 
                                              eNa_strand_plus, eNoSentinels);
                BlastSeqBlkSetCompressedSequence(subj, 
                                          compressed_seq.data.release());
            } catch (CException& e) {
                BlastSequenceBlkFree(subj);
                NCBI_RETHROW_SAME(e, 
                      "Failed to get compressed nucleotide sequence");
            }
        } else {
            BlastSeqBlkSetSequence(subj, sequence.data.release(), 
                                   sequence.length - 2);
        }

        seqblk_vec->push_back(subj);
        (*max_subjlen) = MAX((*max_subjlen), subjects.GetLength(i));

    }
}

/// Tests if a number represents a valid residue
/// @param res Value to test [in]
/// @return TRUE if value is a valid residue ( < 26)
static inline bool s_IsValidResidue(Uint1 res) { return res < 26; }

/// Protein sequences are always encoded in eBlastEncodingProtein and always 
/// have sentinel bytes around sequence data
static SBlastSequence 
GetSequenceProtein(IBlastSeqVector& sv, string* warnings = 0)
{
    Uint1* buf = NULL;          // buffer to write sequence
    Uint1* buf_var = NULL;      // temporary pointer to buffer
    TSeqPos buflen;             // length of buffer allocated
    TSeqPos i;                  // loop index of original sequence
    TAutoUint1Ptr safe_buf;     // contains buf to ensure exception safety
    vector<TSeqPos> replaced_selenocysteins; // Selenocystein residue positions
    vector<TSeqPos> invalid_residues;        // Invalid residue positions

    sv.SetCoding(CSeq_data::e_Ncbistdaa);
    buflen = CalculateSeqBufferLength(sv.size(), eBlastEncodingProtein);
    ASSERT(buflen != 0);
    buf = buf_var = (Uint1*) malloc(sizeof(Uint1)*buflen);
    if ( !buf ) {
        NCBI_THROW(CBlastSystemException, eOutOfMemory, 
                   "Failed to allocate " + NStr::IntToString(buflen) + "bytes");
    }
    safe_buf.reset(buf);
    *buf_var++ = GetSentinelByte(eBlastEncodingProtein);
    for (i = 0; i < sv.size(); i++) {
        // Change Selenocysteine to X
        if (sv[i] == AMINOACID_TO_NCBISTDAA[(int)'U']) {
            replaced_selenocysteins.push_back(i);
            *buf_var++ = AMINOACID_TO_NCBISTDAA[(int)'X'];
        } else if (!s_IsValidResidue(sv[i])) {
            invalid_residues.push_back(i);
        } else {
            *buf_var++ = sv[i];
        }
    }
    if (invalid_residues.size() > 0) {
        string error("Invalid residues found at positions ");
        error += NStr::IntToString(invalid_residues[0]);
        for (i = 1; i < invalid_residues.size(); i++) {
            error += ", " + NStr::IntToString(invalid_residues[i]);
        }
        NCBI_THROW(CBlastException, eInvalidCharacter, error);
    }

    *buf_var++ = GetSentinelByte(eBlastEncodingProtein);
    if (warnings && replaced_selenocysteins.size() > 0) {
        *warnings += "Selenocysteine (U) replaced by X at positions ";
        *warnings += NStr::IntToString(replaced_selenocysteins[0]);
        for (i = 1; i < replaced_selenocysteins.size(); i++) {
            *warnings += ", " + NStr::IntToString(replaced_selenocysteins[i]);
        }
    }
    return SBlastSequence(safe_buf.release(), buflen);
}

static SBlastSequence
GetSequenceCompressedNucleotide(IBlastSeqVector& sv)
{
    sv.SetCoding(CSeq_data::e_Ncbi4na);
    return CompressNcbi2na(sv.GetCompressedPlusStrand());
}

static SBlastSequence
GetSequenceSingleNucleotideStrand(IBlastSeqVector& sv,
                                  EBlastEncoding encoding,
                                  objects::ENa_strand strand, 
                                  ESentinelType sentinel)
{
    ASSERT(strand == eNa_strand_plus || strand == eNa_strand_minus);
    
    if (strand == eNa_strand_plus) {
        sv.SetPlusStrand();
    } else {
        sv.SetMinusStrand();
    }
    
    Uint1* buf = NULL;          // buffer to write sequence
    Uint1* buf_var = NULL;      // temporary pointer to buffer
    TSeqPos buflen;             // length of buffer allocated
    TSeqPos i;                  // loop index of original sequence
    TAutoUint1Ptr safe_buf;     // contains buf to ensure exception safety
    
    // We assume that this packs one base per byte in the requested encoding
    sv.SetCoding(CSeq_data::e_Ncbi4na);
    buflen = CalculateSeqBufferLength(sv.size(), encoding,
                                      strand, sentinel);
    ASSERT(buflen != 0);
    buf = buf_var = (Uint1*) malloc(sizeof(Uint1)*buflen);
    if ( !buf ) {
        NCBI_THROW(CBlastSystemException, eOutOfMemory, 
                   "Failed to allocate " + NStr::IntToString(buflen) + "bytes");
    }
    safe_buf.reset(buf);
    if (sentinel == eSentinels)
        *buf_var++ = GetSentinelByte(encoding);

    if (encoding == eBlastEncodingNucleotide) {
        for (i = 0; i < sv.size(); i++) {
            ASSERT(sv[i] < BLASTNA_SIZE);
            *buf_var++ = NCBI4NA_TO_BLASTNA[sv[i]];
        }
    } else {
        for (i = 0; i < sv.size(); i++) {
            *buf_var++ = sv[i];
        }
    }
    
    if (sentinel == eSentinels)
        *buf_var++ = GetSentinelByte(encoding);
    
    return SBlastSequence(safe_buf.release(), buflen);
}

static SBlastSequence
GetSequenceNucleotideBothStrands(IBlastSeqVector& sv, 
                                 EBlastEncoding encoding, 
                                 ESentinelType sentinel)
{
    SBlastSequence plus =
        GetSequenceSingleNucleotideStrand(sv,
                                          encoding,
                                          eNa_strand_plus,
                                          eNoSentinels);
    
    SBlastSequence minus =
        GetSequenceSingleNucleotideStrand(sv,
                                          encoding,
                                          eNa_strand_minus,
                                          eNoSentinels);
    
    // Stitch the two together
    TSeqPos buflen = CalculateSeqBufferLength(sv.size(), encoding, 
                                              eNa_strand_both, sentinel);
    Uint1* buf_ptr = (Uint1*) malloc(sizeof(Uint1) * buflen);
    if ( !buf_ptr ) {
        NCBI_THROW(CBlastSystemException, eOutOfMemory, 
                   "Failed to allocate " + NStr::IntToString(buflen) + "bytes");
    }
    SBlastSequence retval(buf_ptr, buflen);

    if (sentinel == eSentinels) {
        *buf_ptr++ = GetSentinelByte(encoding);
    }
    memcpy(buf_ptr, plus.data.get(), plus.length);
    buf_ptr += plus.length;
    if (sentinel == eSentinels) {
        *buf_ptr++ = GetSentinelByte(encoding);
    }
    memcpy(buf_ptr, minus.data.get(), minus.length);
    buf_ptr += minus.length;
    if (sentinel == eSentinels) {
        *buf_ptr++ = GetSentinelByte(encoding);
    }

    return retval;
}


SBlastSequence
GetSequence_OMF(IBlastSeqVector& sv, EBlastEncoding encoding, 
            objects::ENa_strand strand, 
            ESentinelType sentinel,
            std::string* warnings) 
{
    switch (encoding) {
    case eBlastEncodingProtein:
        return GetSequenceProtein(sv, warnings);

    case eBlastEncodingNcbi4na:
    case eBlastEncodingNucleotide: // Used for nucleotide blastn queries
        if (strand == eNa_strand_both) {
            return GetSequenceNucleotideBothStrands(sv, encoding, sentinel);
        } else {
            return GetSequenceSingleNucleotideStrand(sv,
                                                     encoding,
                                                     strand,
                                                     sentinel);
        }

    case eBlastEncodingNcbi2na:
        ASSERT(sentinel == eNoSentinels);
        return GetSequenceCompressedNucleotide(sv);

    default:
        NCBI_THROW(CBlastException, eNotSupported, "Unsupported encoding");
    }
}

EBlastEncoding
GetQueryEncoding(EBlastProgramType program)
{
    EBlastEncoding retval = eBlastEncodingError;

    switch (program) {
    case eBlastTypeBlastn:
    case eBlastTypePhiBlastn: 
        retval = eBlastEncodingNucleotide; 
        break;

    case eBlastTypeBlastp: 
    case eBlastTypeTblastn:
    case eBlastTypeRpsBlast: 
    case eBlastTypePsiBlast:
    case eBlastTypePhiBlastp:
        retval = eBlastEncodingProtein; 
        break;

    case eBlastTypeBlastx:
    case eBlastTypeTblastx:
    case eBlastTypeRpsTblastn:
        retval = eBlastEncodingNcbi4na;
        break;

    default:
        abort();    // should never happen
    }

    return retval;
}

EBlastEncoding
GetSubjectEncoding(EBlastProgramType program)
{
    EBlastEncoding retval = eBlastEncodingError;

    switch (program) {
    case eBlastTypeBlastn: 
        retval = eBlastEncodingNucleotide; 
        break;

    case eBlastTypeBlastp: 
    case eBlastTypeBlastx:
    case eBlastTypePsiBlast:
        retval = eBlastEncodingProtein; 
        break;

    case eBlastTypeTblastn:
    case eBlastTypeTblastx:
        retval = eBlastEncodingNcbi4na;
        break;

    default:
        abort();        // should never happen
    }

    return retval;
}

SBlastSequence CompressNcbi2na(const SBlastSequence& source)
{
    ASSERT(source.data.get());

    TSeqPos i;                  // loop index of original sequence
    TSeqPos ci;                 // loop index for compressed sequence

    // Allocate the return value
    SBlastSequence retval(CalculateSeqBufferLength(source.length,
                                                   eBlastEncodingNcbi2na,
                                                   eNa_strand_plus,
                                                   eNoSentinels));
    Uint1* source_ptr = source.data.get();

    // Populate the compressed sequence up to the last byte
    for (ci = 0, i = 0; ci < retval.length-1; ci++, i+= COMPRESSION_RATIO) {
        Uint1 a, b, c, d;
        a = ((*source_ptr & NCBI2NA_MASK)<<6); ++source_ptr;
        b = ((*source_ptr & NCBI2NA_MASK)<<4); ++source_ptr;
        c = ((*source_ptr & NCBI2NA_MASK)<<2); ++source_ptr;
        d = ((*source_ptr & NCBI2NA_MASK)<<0); ++source_ptr;
        retval.data.get()[ci] = a | b | c | d;
    }

    // Set the last byte in the compressed sequence
    retval.data.get()[ci] = 0;
    for (; i < source.length; i++) {
            Uint1 bit_shift = 0;
            switch (i%COMPRESSION_RATIO) {
               case 0: bit_shift = 6; break;
               case 1: bit_shift = 4; break;
               case 2: bit_shift = 2; break;
               default: abort();   // should never happen
            }
            retval.data.get()[ci] |= ((*source_ptr & NCBI2NA_MASK)<<bit_shift);
            ++source_ptr;
    }
    // Set the number of bases in the last 2 bits of the last byte in the
    // compressed sequence
    retval.data.get()[ci] |= source.length%COMPRESSION_RATIO;
    return retval;
}

TSeqPos CalculateSeqBufferLength(TSeqPos sequence_length, 
                                 EBlastEncoding encoding,
                                 objects::ENa_strand strand, 
                                 ESentinelType sentinel)
                                 THROWS((CBlastException))
{
    TSeqPos retval = 0;

    if (sequence_length == 0) {
        return retval;
    }

    switch (encoding) {
    // Strand and sentinels are irrelevant in this encoding.
    // Strand is always plus and sentinels cannot be represented
    case eBlastEncodingNcbi2na:
        ASSERT(sentinel == eNoSentinels);
        ASSERT(strand == eNa_strand_plus);
        retval = sequence_length / COMPRESSION_RATIO + 1;
        break;

    case eBlastEncodingNcbi4na:
    case eBlastEncodingNucleotide: // Used for nucleotide blastn queries
        if (sentinel == eSentinels) {
            if (strand == eNa_strand_both) {
                retval = sequence_length * 2;
                retval += 3;
            } else {
                retval = sequence_length + 2;
            }
        } else {
            if (strand == eNa_strand_both) {
                retval = sequence_length * 2;
            } else {
                retval = sequence_length;
            }
        }
        break;

    case eBlastEncodingProtein:
        ASSERT(sentinel == eSentinels);
        ASSERT(strand == eNa_strand_unknown);
        retval = sequence_length + 2;
        break;

    default:
        NCBI_THROW(CBlastException, eNotSupported, "Unsupported encoding");
    }

    return retval;
}

Uint1 GetSentinelByte(EBlastEncoding encoding) THROWS((CBlastException))
{
    switch (encoding) {
    case eBlastEncodingProtein:
        return kProtSentinel;

    case eBlastEncodingNcbi4na:
    case eBlastEncodingNucleotide:
        return kNuclSentinel;

    default:
        NCBI_THROW(CBlastException, eNotSupported, "Unsupported encoding");
    }
}

#if 0
// Not used right now, need to complete implementation
void
BLASTGetTranslation(const Uint1* seq, const Uint1* seq_rev,
        const int nucl_length, const short frame, Uint1* translation)
{
    TSeqPos ni = 0;     // index into nucleotide sequence
    TSeqPos pi = 0;     // index into protein sequence

    const Uint1* nucl_seq = frame >= 0 ? seq : seq_rev;
    translation[0] = NULLB;
    for (ni = ABS(frame)-1; ni < (TSeqPos) nucl_length-2; ni += CODON_LENGTH) {
        Uint1 residue = CGen_code_table::CodonToIndex(nucl_seq[ni+0], 
                                                      nucl_seq[ni+1],
                                                      nucl_seq[ni+2]);
        if (IS_residue(residue))
            translation[pi++] = residue;
    }
    translation[pi++] = NULLB;

    return;
}
#endif

string
FindMatrixPath(const char* matrix_name, bool is_prot)
{
    string retval;
    string full_path;       // full path to matrix file

    if (!matrix_name)
        return retval;

    string mtx(matrix_name);
    mtx = NStr::ToUpper(mtx);

    // Look for matrix file in local directory
    full_path = mtx;
    if (CFile(full_path).Exists()) {
        return retval;
    }
    
    string path("");

    // Obtain the matrix path from the ncbi configuration file
    try {
        CMetaRegistry::SEntry sentry;
        sentry = CMetaRegistry::Load("ncbi", CMetaRegistry::eName_RcOrIni);
        path = sentry.registry ? sentry.registry->Get("NCBI", "Data") : "";
    } catch (const CRegistryException&) { /* ignore */ }
    
    full_path = CFile::MakePath(path, mtx);
    if (CFile(full_path).Exists()) {
        retval = full_path;
        retval.erase(retval.size() - mtx.size());
        return retval;
    }

    // Try appending "aa" or "nt" 
    full_path = path;
    full_path += CFile::AddTrailingPathSeparator(full_path);
    full_path += is_prot ? "aa" : "nt";
    full_path += CFile::AddTrailingPathSeparator(full_path);
    full_path += mtx;
    if (CFile(full_path).Exists()) {
        retval = full_path;
        retval.erase(retval.size() - mtx.size());
        return retval;
    }

    // Try using local "data" directory
    full_path = "data";
    full_path += CFile::AddTrailingPathSeparator(full_path);
    full_path += mtx;
    if (CFile(full_path).Exists()) {
        retval = full_path;
        retval.erase(retval.size() - mtx.size());
        return retval;
    }

    CNcbiApplication* app = CNcbiApplication::Instance();
    if (!app)
        return retval;

    const string& blastmat_env = app->GetEnvironment().Get("BLASTMAT");
    if (CFile(blastmat_env).Exists()) {
        full_path = blastmat_env;
        full_path += CFile::AddTrailingPathSeparator(full_path);
        full_path += is_prot ? "aa" : "nt";
        full_path += CFile::AddTrailingPathSeparator(full_path);
        full_path += mtx;
        if (CFile(full_path).Exists()) {
            retval = full_path;
            retval.erase(retval.size() - mtx.size());
            return retval;
        }
    }

#ifdef OS_UNIX
    full_path = BLASTMAT_DIR;
    full_path += CFile::AddTrailingPathSeparator(full_path);
    full_path += is_prot ? "aa" : "nt";
    full_path += CFile::AddTrailingPathSeparator(full_path);
    full_path += mtx;
    if (CFile(full_path).Exists()) {
        retval = full_path;
        retval.erase(retval.size() - mtx.size());
        return retval;
    }
#endif

    // Try again without the "aa" or "nt"
    if (CFile(blastmat_env).Exists()) {
        full_path = blastmat_env;
        full_path += CFile::AddTrailingPathSeparator(full_path);
        full_path += mtx;
        if (CFile(full_path).Exists()) {
            retval = full_path;
            retval.erase(retval.size() - mtx.size());
            return retval;
        }
    }

#ifdef OS_UNIX
    full_path = BLASTMAT_DIR;
    full_path += CFile::AddTrailingPathSeparator(full_path);
    full_path += mtx;
    if (CFile(full_path).Exists()) {
        retval = full_path;
        retval.erase(retval.size() - mtx.size());
        return retval;
    }
#endif

    return retval;
}

/// Checks if a BLAST database exists at a given file path: looks for 
/// an alias file first, then for an index file
static bool BlastDbFileExists(string& path, bool is_prot)
{
    // Check for alias file first
    string full_path = path + (is_prot ? ".pal" : ".nal");
    if (CFile(full_path).Exists())
        return true;
    // Check for an index file
    full_path = path + (is_prot ? ".pin" : ".nin");
    if (CFile(full_path).Exists())
        return true;
    return false;
}

string
FindBlastDbPath(const char* dbname, bool is_prot)
{
    string retval;
    string full_path;       // full path to matrix file

    if (!dbname)
        return retval;

    string database(dbname);

    // Look for matrix file in local directory
    full_path = database;
    if (BlastDbFileExists(full_path, is_prot)) {
        return retval;
    }

    CNcbiApplication* app = CNcbiApplication::Instance();
    if (app) {
        const string& blastdb_env = app->GetEnvironment().Get("BLASTDB");
        if (CFile(blastdb_env).Exists()) {
            full_path = blastdb_env;
            full_path += CFile::AddTrailingPathSeparator(full_path);
            full_path += database;
            if (BlastDbFileExists(full_path, is_prot)) {
                retval = full_path;
                retval.erase(retval.size() - database.size());
                return retval;
            }
        }
    }

    // Obtain the matrix path from the ncbi configuration file
    CMetaRegistry::SEntry sentry;
    sentry = CMetaRegistry::Load("ncbi", CMetaRegistry::eName_RcOrIni);
    string path = 
        sentry.registry ? sentry.registry->Get("BLAST", "BLASTDB") : "";

    full_path = CFile::MakePath(path, database);
    if (BlastDbFileExists(full_path, is_prot)) {
        retval = full_path;
        retval.erase(retval.size() - database.size());
        return retval;
    }

    return retval;
}

unsigned int
GetNumberOfContexts(EBlastProgramType p)
{
    unsigned int retval = 0;
    if ( (retval = BLAST_GetNumberOfContexts(p)) == 0) {
        int debug_value = static_cast<int>(p);
        string prog_name(Blast_ProgramNameFromType(p));
        string msg = "Cannot get number of contexts for invalid program ";
        msg += "type: " + prog_name + " (" + NStr::IntToString(debug_value);
        msg += ")";
        NCBI_THROW(CBlastException, eNotSupported, msg);
    }

    return retval;
}

/////////////////////////////////////////////////////////////////////////////

BLAST_SequenceBlk*
SafeSetupQueries(IBlastQuerySource& queries,
                 const CBlastOptions* options,
                 BlastQueryInfo* query_info,
                 TSearchMessages& messages)
{
    ASSERT(options);
    ASSERT(query_info);
    ASSERT( !queries.Empty() );

    CBLAST_SequenceBlk retval;
    TAutoUint1ArrayPtr gc = FindGeneticCode(options->GetQueryGeneticCode());

    SetupQueries_OMF(queries, query_info, &retval, options->GetProgramType(), 
                     options->GetStrandOption(), gc.get(), messages);

    return retval.Release();
}

BlastQueryInfo*
SafeSetupQueryInfo(const IBlastQuerySource& queries,
                   const CBlastOptions* options)
{
    ASSERT(!queries.Empty());
    ASSERT(options);

    CBlastQueryInfo retval;
    SetupQueryInfo_OMF(queries, options->GetProgramType(),
                       options->GetStrandOption(), &retval);

    if (retval.Get() == NULL) {
        NCBI_THROW(CBlastException, eInvalidArgument, 
                   "blast::SetupQueryInfo failed");
    }
    return retval.Release();
}


bool CBlastQueryFilteredFrames::x_NeedsTrans()
{
    switch(m_Program) {
    case eBlastTypeBlastx:
    case eBlastTypeTblastx:
    case eBlastTypeRpsTblastn:
        return true;
        break;
            
    default:
        return false;
    }
}

CBlastQueryFilteredFrames::
CBlastQueryFilteredFrames(EBlastProgramType program)
    : m_Program(program)
{
    m_TranslateCoords = x_NeedsTrans();
}

CBlastQueryFilteredFrames::
CBlastQueryFilteredFrames(EBlastProgramType           program,
                          const TMaskedQueryRegions & mqr)
    : m_Program(program)
{
    m_TranslateCoords = x_NeedsTrans();
    
    if (mqr.empty()) {
        return;
    }
    
    ITERATE(TMaskedQueryRegions, itr, mqr) {
        const CSeq_interval & intv = (**itr).GetInterval();
        
        ETranslationFrame frame =
            (ETranslationFrame) (**itr).GetFrame();
        
        AddSeqLoc(intv, frame);
    }
}

CBlastQueryFilteredFrames::~CBlastQueryFilteredFrames()
{
    ITERATE(TFrameSet, iter, m_Seqlocs) {
        if ((*iter).second != 0) {
            BlastSeqLocFree((*iter).second);
        }
    }
}

void CBlastQueryFilteredFrames::Release(int frame)
{
    m_Seqlocs.erase((ETranslationFrame)frame);
}

void CBlastQueryFilteredFrames::UseProteinCoords(TSeqPos dna_length)
{
    if (m_TranslateCoords) {
        m_TranslateCoords = false;
        
        ITERATE(TFrameSet, iter, m_Seqlocs) {
            short frame = iter->first;
            BlastSeqLoc * bsl = iter->second;
            
            for (BlastSeqLoc* itr = bsl; itr; itr = itr->next) {
                TSeqPos to(0), from(0);
                
                if (frame < 0) {
                    from = (dna_length + frame - itr->ssr->right) / CODON_LENGTH;
                    to = (dna_length + frame - itr->ssr->left) / CODON_LENGTH;
                } else {
                    from = (itr->ssr->left - frame + 1) / CODON_LENGTH;
                    to = (itr->ssr->right - frame + 1) / CODON_LENGTH;
                }
                
                itr->ssr->left  = from;
                itr->ssr->right = to;
            }
        }
    }
}

vector<CBlastQueryFilteredFrames::ETranslationFrame>
CBlastQueryFilteredFrames::ListFrames() const
{
    vector<ETranslationFrame> rv;
    
    ITERATE(TFrameSet, iter, m_Seqlocs) {
        if ((*iter).second != 0) {
            rv.push_back((*iter).first);
        }
    }
    
    return rv;
}

bool CBlastQueryFilteredFrames::Empty() const
{
    vector<ETranslationFrame> rv = ListFrames();
    
    return rv.empty();
}

void CBlastQueryFilteredFrames::x_VerifyFrame(int frame)
{
    bool okay = true;
    
    switch(m_Program) {
    case eBlastTypeBlastp:
    case eBlastTypeTblastn:
    case eBlastTypeRpsBlast:
    case eBlastTypePsiBlast:
        if (frame != 0) {
            okay = false;
        }
        break;
        
    case eBlastTypeBlastn:
        if ((frame != CSeqLocInfo::eFramePlus1) &&
            (frame != CSeqLocInfo::eFrameMinus1)) {
            okay = false;
        }
        break;
        
    case eBlastTypeBlastx:
    case eBlastTypeTblastx:
    case eBlastTypeRpsTblastn:
        switch(frame) {
        case 1:
        case 2:
        case 3:
        case -1:
        case -2:
        case -3:
            break;
            
        default:
            okay = false;
        }
        break;
        
    default:
        okay = false;
    }
    
    if (! okay) {
        NCBI_THROW(CBlastException, eNotSupported, 
                   "Frame and program values are incompatible.");
    }
}

bool CBlastQueryFilteredFrames::QueryIsMulti() const
{
    switch(m_Program) {
    case eBlastTypeBlastp:
    case eBlastTypeTblastn:
    case eBlastTypeRpsBlast:
    case eBlastTypePsiBlast:
        return false;
        
    case eBlastTypeBlastn:
    case eBlastTypeBlastx:
    case eBlastTypeTblastx:
    case eBlastTypeRpsTblastn:
        return true;
        
    default:
        NCBI_THROW(CBlastException, eNotSupported, 
                   "IsMulti: unsupported program");
    }
    
    return false;
}

void CBlastQueryFilteredFrames::AddSeqLoc(const objects::CSeq_interval & intv, 
                                          int frame)
{
    if ((frame == 0) && (m_Program == eBlastTypeBlastn)) {
        x_VerifyFrame(CSeqLocInfo::eFramePlus1);
        x_VerifyFrame(CSeqLocInfo::eFrameMinus1);
        
        BlastSeqLocNew(& m_Seqlocs[CSeqLocInfo::eFramePlus1],
                       intv.GetFrom(),
                       intv.GetTo());
        
        BlastSeqLocNew(& m_Seqlocs[CSeqLocInfo::eFrameMinus1],
                       intv.GetFrom(),
                       intv.GetTo());
    } else {
        x_VerifyFrame(frame);
        
        BlastSeqLocNew(& m_Seqlocs[(ETranslationFrame) frame],
                       intv.GetFrom(),
                       intv.GetTo());
    }
}

BlastSeqLoc ** CBlastQueryFilteredFrames::operator[](int frame)
{
    // Asking for a frame verifies that it is a valid vaue for the
    // type of search you are running.
    
    x_VerifyFrame(frame);
    return & m_Seqlocs[(ETranslationFrame) frame];
}


END_SCOPE(blast)
END_NCBI_SCOPE

/* @} */

/*
 * ===========================================================================
 *
 * $Log$
 * Revision 1.112  2006/04/05 21:00:58  camacho
 * Remove unused argument from s_QueryInfo_SetContext
 *
 * Revision 1.111  2006/04/05 20:48:15  camacho
 * Move initialization of BlastContextInfo::{query_index,frame} to
 * BlastQueryInfoNew.
 *
 * Revision 1.110  2006/03/31 17:32:24  camacho
 * Use constants for sentinel values
 *
 * Revision 1.109  2006/03/15 22:39:54  camacho
 * Minor change
 *
 * Revision 1.108  2006/03/03 17:15:21  camacho
 * Doxygen fixes
 *
 * Revision 1.107  2006/02/28 17:14:32  camacho
 * Reduce the scope of frame_to_bsl so that a non-NULL object is always passed to
 * s_AddMask. The object might be NULL in case of an invalid gi.
 *
 * Revision 1.106  2006/02/27 15:43:47  camacho
 * Fixed bug in CBlastQuerySourceOM::GetMaskedRegions.
 * Made IBlastQuerySource::GetMaskedRegions a non-const method.
 *
 * Revision 1.105  2006/02/22 18:34:17  bealer
 * - Blastx filtering support, CBlastQueryVector class.
 *
 * Revision 1.104  2006/01/24 15:25:40  camacho
 * Remove const type qualifier for IBlastQuerySoruce to SetupQueries_OMF and SetupSubjects_OMF
 *
 * Revision 1.103  2006/01/12 20:39:22  camacho
 * Removed const from BlastQueryInfo argument to functions (to allow setting of context-validity flag)
 *
 * Revision 1.102  2005/12/16 20:51:18  camacho
 * Diffuse the use of CSearchMessage, TQueryMessages, and TSearchMessages
 *
 * Revision 1.101  2005/12/01 15:41:21  bealer
 * - Fix blastn strand handling error.
 *
 * Revision 1.100  2005/11/10 14:48:34  madden
 * Fix to SetupQueries_OMF if zero length query was included
 *
 * Revision 1.99  2005/09/28 18:23:08  camacho
 * Rearrangement of headers/functions to segregate object manager dependencies.
 *
 * Revision 1.98  2005/09/26 15:36:18  camacho
 * Eliminate msvc compiler warnings
 *
 * Revision 1.97  2005/09/23 19:01:08  camacho
 * Fix non-functional subject sequence filtering
 *
 * Revision 1.96  2005/09/16 17:03:42  camacho
 * Refactoring of filtering locations code.
 *
 * Revision 1.95  2005/09/02 15:58:15  camacho
 * Rename GetNumberOfFrames -> GetNumberOfContexts, delegate to CORE function
 *
 * Revision 1.94  2005/08/30 20:22:14  camacho
 * + psitblastn to GetNumberOfFrames
 *
 * Revision 1.93  2005/07/07 16:32:12  camacho
 * Revamping of BLAST exception classes and error codes
 *
 * Revision 1.92  2005/06/20 18:55:37  ucko
 * Replace non-portable __func__ with the relevant hardcoded strings.
 *
 * Revision 1.91  2005/06/20 17:32:47  camacho
 * Add blast::GetSequence object manager-free interface
 *
 * Revision 1.90  2005/06/20 13:10:11  madden
 * Rename BlastSeverity enums in line with C++ tookit convention
 *
 * Revision 1.89  2005/06/15 21:48:53  camacho
 * Fix out of date comment
 *
 * Revision 1.88  2005/06/10 18:07:01  camacho
 * Use default argument for GetBlastSequence
 *
 * Revision 1.87  2005/06/10 15:20:48  ucko
 * Use consistent parameter names in SetupSubjects(_OMF).
 *
 * Revision 1.86  2005/06/10 14:56:18  camacho
 * Implement SetupSubjects_OMF
 *
 * Revision 1.85  2005/06/09 20:34:52  camacho
 * Object manager dependent functions reorganization
 *
 * Revision 1.84  2005/05/26 14:36:47  dondosha
 * Added PHI BLAST cases in switch statements
 *
 * Revision 1.83  2005/05/10 21:23:59  camacho
 * Fix to prior commit
 *
 * Revision 1.82  2005/05/10 16:08:39  camacho
 * Changed *_ENCODING #defines to EBlastEncoding enumeration
 *
 * Revision 1.81  2005/04/06 21:06:18  dondosha
 * Use EBlastProgramType instead of EProgram in non-user-exposed functions
 *
 * Revision 1.80  2005/03/04 16:53:27  camacho
 * more doxygen fixes
 *
 * Revision 1.79  2005/01/21 17:38:57  camacho
 * Handle zero-length sequences
 *
 * Revision 1.78  2004/12/29 17:15:42  camacho
 * Use NStr utility functions
 *
 * Revision 1.77  2004/12/20 20:17:00  camacho
 * + PSI-BLAST
 *
 * Revision 1.76  2004/11/12 16:43:34  camacho
 * Add handling of missing EProgram values to GetNumberOfFrames
 *
 * Revision 1.75  2004/09/21 13:50:38  dondosha
 * GetNumberOfFrames now returns 6 for RPS tblastn
 *
 * Revision 1.74  2004/08/17 20:01:57  camacho
 * Handle misconfigured NCBI configuration file
 *
 * Revision 1.73  2004/08/17 15:13:00  ivanov
 * Moved GetProgramFromBlastProgramType() from blast_setup_cxx.cpp
 * to blast_options_cxx.cpp
 *
 * Revision 1.72  2004/08/11 14:24:50  camacho
 * Move FindGeneticCode
 *
 * Revision 1.71  2004/08/02 13:28:28  camacho
 * Minor
 *
 * Revision 1.70  2004/07/06 15:49:30  dondosha
 * Added GetProgramFromBlastProgramType function
 *
 * Revision 1.69  2004/06/02 15:59:14  bealer
 * - Isolate objmgr dependent code.
 *
 * Revision 1.68  2004/05/21 21:41:02  gorelenk
 * Added PCH ncbi_pch.hpp
 *
 * Revision 1.67  2004/04/19 19:52:02  papadopo
 * correct off-by-one error in sequence size computation
 *
 * Revision 1.66  2004/04/16 14:28:49  papadopo
 * add use of eRPSBlast and eRPSTblastn programs
 *
 * Revision 1.65  2004/04/07 03:06:15  camacho
 * Added blast_encoding.[hc], refactoring blast_stat.[hc]
 *
 * Revision 1.64  2004/04/06 20:45:28  dondosha
 * Added function FindBlastDbPath: should be moved to seqdb library
 *
 * Revision 1.63  2004/03/24 19:14:48  dondosha
 * Fixed memory leaks
 *
 * Revision 1.62  2004/03/19 19:22:55  camacho
 * Move to doxygen group AlgoBlast, add missing CVS logs at EOF
 *
 * Revision 1.61  2004/03/15 19:57:52  dondosha
 * SetupSubjects takes just program argument instead of CBlastOptions*
 *
 * Revision 1.60  2004/03/11 17:26:46  dicuccio
 * Use NStr::ToUpper() instead of transform
 *
 * Revision 1.59  2004/03/09 18:53:25  dondosha
 * Do not set db length and number of sequences options to real values - these are calculated and assigned to parameters structure fields
 *
 * Revision 1.58  2004/03/06 00:39:47  camacho
 * Some refactorings, changed boolen parameter to enum in GetSequence
 *
 * Revision 1.57  2004/02/24 18:14:56  dondosha
 * Set the maximal length in the set of queries, when filling BlastQueryInfo structure
 *
 * Revision 1.56  2004/02/18 15:16:28  camacho
 * Consolidated ncbi2na mask definition
 *
 * Revision 1.55  2004/01/07 17:39:27  vasilche
 * Fixed include path to genbank loader.
 *
 * Revision 1.54  2003/12/29 17:00:57  camacho
 * Update comment
 *
 * Revision 1.53  2003/12/15 19:55:14  camacho
 * Minor fix to ensure exception safety
 *
 * Revision 1.52  2003/12/03 16:43:47  dondosha
 * Renamed BlastMask to BlastMaskLoc, BlastResults to BlastHSPResults
 *
 * Revision 1.51  2003/11/26 18:36:45  camacho
 * Renaming blast_option*pp -> blast_options*pp
 *
 * Revision 1.50  2003/11/26 18:24:00  camacho
 * +Blast Option Handle classes
 *
 * Revision 1.49  2003/11/24 17:12:44  dondosha
 * Query info structure does not have a total_length member any more; use last context offset
 *
 * Revision 1.48  2003/11/06 21:25:37  camacho
 * Revert previous change, add assertions
 *
 * Revision 1.47  2003/11/04 17:14:22  dondosha
 * Length in subject sequence block C structure should not include sentinels
 *
 * Revision 1.46  2003/10/31 19:45:03  camacho
 * Fix setting of subject sequence length
 *
 * Revision 1.45  2003/10/31 16:53:32  dondosha
 * Set length in query sequence block correctly
 *
 * Revision 1.44  2003/10/29 04:46:16  camacho
 * Use fixed AutoPtr for GetSequence return value
 *
 * Revision 1.43  2003/10/27 21:27:36  camacho
 * Remove extra argument to GetSequenceView, minor refactorings
 *
 * Revision 1.42  2003/10/22 14:21:55  camacho
 * Added sanity checking assertions
 *
 * Revision 1.41  2003/10/21 13:04:54  camacho
 * Fix bug in SetupSubjects, use sequence blk set functions
 *
 * Revision 1.40  2003/10/16 13:38:54  coulouri
 * use anonymous exceptions to fix unreferenced variable compiler warning
 *
 * Revision 1.39  2003/10/15 18:18:00  camacho
 * Fix to setup query info structure for proteins
 *
 * Revision 1.38  2003/10/15 15:09:32  camacho
 * Changes from Mike DiCuccio to use GetSequenceView to retrieve sequences.
 *
 * Revision 1.37  2003/10/08 15:13:56  dondosha
 * Test if subject mask is not NULL before converting to a C structure
 *
 * Revision 1.36  2003/10/08 15:05:47  dondosha
 * Test if mask is not NULL before converting to a C structure
 *
 * Revision 1.35  2003/10/07 17:34:05  dondosha
 * Add lower case masks to SSeqLocs forming the vector of sequence locations
 *
 * Revision 1.34  2003/10/03 16:12:18  dondosha
 * Fix in previous change for plus strand search
 *
 * Revision 1.33  2003/10/02 22:10:46  dondosha
 * Corrections for one-strand translated searches
 *
 * Revision 1.32  2003/09/30 03:23:18  camacho
 * Fixes to FindMatrixPath
 *
 * Revision 1.31  2003/09/29 21:38:29  camacho
 * Assign retval only when successfully found path to matrix
 *
 * Revision 1.30  2003/09/29 20:35:03  camacho
 * Replace abort() with exception in GetNumberOfFrames
 *
 * Revision 1.29  2003/09/16 16:48:13  dondosha
 * Use BLAST_PackDNA and BlastSetUp_SeqBlkNew from the core blast library for setting up subject sequences
 *
 * Revision 1.28  2003/09/12 17:52:42  camacho
 * Stop using pair<> as return value from GetSequence
 *
 * Revision 1.27  2003/09/11 20:55:01  camacho
 * Temporary fix for AutoPtr return value
 *
 * Revision 1.26  2003/09/11 17:45:03  camacho
 * Changed CBlastOption -> CBlastOptions
 *
 * Revision 1.25  2003/09/10 04:27:43  camacho
 * 1) Minor change to return type of GetSequence
 * 2) Fix to previous revision
 *
 * Revision 1.24  2003/09/09 22:15:02  dondosha
 * Added cast in return statement in GetSequence method, fixing compiler error
 *
 * Revision 1.23  2003/09/09 15:57:23  camacho
 * Fix indentation
 *
 * Revision 1.22  2003/09/09 14:21:39  coulouri
 * change blastkar.h to blast_stat.h
 *
 * Revision 1.21  2003/09/09 12:57:15  camacho
 * + internal setup functions, use smart pointers to handle memory mgmt
 *
 * Revision 1.20  2003/09/05 19:06:31  camacho
 * Use regular new to allocate genetic code string
 *
 * Revision 1.19  2003/09/03 19:36:27  camacho
 * Fix include path for blast_setup.hpp
 *
 * Revision 1.18  2003/08/28 22:42:54  camacho
 * Change BLASTGetSequence signature
 *
 * Revision 1.17  2003/08/28 15:49:02  madden
 * Fix for packing DNA as well as correct buflen
 *
 * Revision 1.16  2003/08/25 16:24:14  camacho
 * Updated BLASTGetMatrixPath
 *
 * Revision 1.15  2003/08/19 17:39:07  camacho
 * Minor fix to use of metaregistry class
 *
 * Revision 1.14  2003/08/18 20:58:57  camacho
 * Added blast namespace, removed *__.hpp includes
 *
 * Revision 1.13  2003/08/14 13:51:24  camacho
 * Use CMetaRegistry class to load the ncbi config file
 *
 * Revision 1.12  2003/08/11 15:17:39  dondosha
 * Added algo/blast/core to all #included headers
 *
 * Revision 1.11  2003/08/11 14:00:41  dicuccio
 * Indenting changes.  Fixed use of C++ namespaces (USING_SCOPE(objects) inside of
 * BEGIN_NCBI_SCOPE block)
 *
 * Revision 1.10  2003/08/08 19:43:07  dicuccio
 * Compilation fixes: #include file rearrangement; fixed use of 'list' and
 * 'vector' as variable names; fixed missing ostrea<< for __int64
 *
 * Revision 1.9  2003/08/04 15:18:23  camacho
 * Minor fixes
 *
 * Revision 1.8  2003/08/01 22:35:02  camacho
 * Added function to get matrix path (fixme)
 *
 * Revision 1.7  2003/08/01 17:40:56  dondosha
 * Use renamed functions and structures from local blastkar.h
 *
 * Revision 1.6  2003/07/31 19:45:33  camacho
 * Eliminate Ptr notation
 *
 * Revision 1.5  2003/07/30 15:00:01  camacho
 * Do not use Malloc/MemNew/MemFree
 *
 * Revision 1.4  2003/07/25 13:55:58  camacho
 * Removed unnecessary #includes
 *
 * Revision 1.3  2003/07/24 18:22:50  camacho
 * #include blastkar.h
 *
 * Revision 1.2  2003/07/23 21:29:06  camacho
 * Update BLASTFindGeneticCode to get genetic code string with C++ toolkit
 *
 * Revision 1.1  2003/07/10 18:34:19  camacho
 * Initial revision
 *
 *
 * ===========================================================================
 */
