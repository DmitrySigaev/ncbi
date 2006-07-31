/* $Id$
 * ===========================================================================
 *
 *                            PUBLIC DOMAIN NOTICE
 *               National Center for Biotechnology Information
 *
 *  This software/database is a "United States Government Work" under the
 *  terms of the United States Copyright Act.  It was written as part of
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
 */

/** @file blast_extend.c
 * Functions to initialize structures used for BLAST extension
 */

#ifndef SKIP_DOXYGEN_PROCESSING
static char const rcsid[] =
    "$Id$";
#endif                          /* SKIP_DOXYGEN_PROCESSING */

#include <algo/blast/core/blast_extend.h>
#include <algo/blast/core/blast_options.h>
#include <algo/blast/core/blast_lookup.h>
#include <algo/blast/core/mb_lookup.h>
#include <algo/blast/core/blast_util.h> /* for NCBI2NA_UNPACK_BASE macros */
#include <algo/blast/core/aa_ungapped.h>        /* for
                                                   BlastDiagTable{New,Free} */
#include "blast_inline.h"

/*
 * Attempt to retrieve information associated with diagonal diag.
 * Return 1 if successful, 0 if failed.
 */
static NCBI_INLINE Int4 s_BlastDiagHashRetrieve(BLAST_DiagHash * table,
                                                  Int4 diag, Int4 * level,
                                                  Int4 * hit_saved)
{
	Int4 bucket = diag & 0x1ff;
	Int4 numElements = table->size[bucket];
	Int4 index;

	for(index=0;index<numElements;index++)
	{
		if (table->backbone[bucket][index].diag == diag)
		{
			*level     = table->backbone[bucket][index].level;
			*hit_saved = table->backbone[bucket][index].hit_saved;
			return 1;
		}
	}

	return 0;
}

/*
 * Attempt to store information associated with diagonal diag.
 * Clean up defunct entries along the way or allocate more space if necessary.
 * Return 1 if successful, 0 if failed.
 */
static NCBI_INLINE Int4 s_BlastDiagHashInsert(BLAST_DiagHash * table,
                                                Int4 diag, Int4 level,
                                                Int4 hit_saved,
												Int4 s_end,
												Int4 window_size,
												Int4 min_step,
												Int4 two_hits)
{
	Int4 bucket = diag & 0x1ff;
	Int4 numElements = table->size[bucket];
	Int4 index;

	for(index=0;index<numElements;index++)
	{
		/* if we find what we're looking for, save into it */
		if (table->backbone[bucket][index].diag == diag)
		{
			table->backbone[bucket][index].level     = level;
			table->backbone[bucket][index].hit_saved = hit_saved;
			return 1;
		}
		else
		{
			Int4 step = s_end - table->backbone[bucket][index].level;
			/* if this hit is stale, save into it. */
			if ( ! (step <= (Int4)min_step || (two_hits && step <= window_size)) ) {
                table->backbone[bucket][index].diag      = diag;
				table->backbone[bucket][index].level     = level;
                table->backbone[bucket][index].hit_saved = hit_saved;
				return 1;
			}
		}
	}

	/* if we got this far, we were unable to replace any existing entries.
       add a new hit to the end, allocating more memory if necessary */

	/* if we need more space, allocate it */
	if (numElements == table->capacity[bucket]) {
        table->capacity[bucket] *= 2;
		table->backbone[bucket] = realloc(table->backbone[bucket],
												table->capacity[bucket] * sizeof(DiagHashCell));
		if (table->backbone[bucket] == NULL)
			return 0;
	}

	/* save the hit */
	table->backbone[bucket][numElements].diag      = diag;
	table->backbone[bucket][numElements].level     = level;
	table->backbone[bucket][numElements].hit_saved = hit_saved;
	table->size[bucket]++;

return 1;
}

/* Description in blast_extend.h */
BlastInitHitList *BLAST_InitHitListNew(void)
{
    BlastInitHitList *init_hitlist = (BlastInitHitList *)
        calloc(1, sizeof(BlastInitHitList));

    init_hitlist->allocated = MIN_INIT_HITLIST_SIZE;

    init_hitlist->init_hsp_array = (BlastInitHSP *)
        malloc(MIN_INIT_HITLIST_SIZE * sizeof(BlastInitHSP));

    return init_hitlist;
}

void BlastInitHitListReset(BlastInitHitList * init_hitlist)
{
    Int4 index;

    for (index = 0; index < init_hitlist->total; ++index)
        sfree(init_hitlist->init_hsp_array[index].ungapped_data);
    init_hitlist->total = 0;
}

BlastInitHitList *BLAST_InitHitListFree(BlastInitHitList * init_hitlist)
{
    if (init_hitlist == NULL)
        return NULL;

    BlastInitHitListReset(init_hitlist);
    sfree(init_hitlist->init_hsp_array);
    sfree(init_hitlist);
    return NULL;
}

/** Callback for sorting an array of initial HSP structures (not pointers to
 * structures!) by score. 
 */
static int score_compare_match(const void *v1, const void *v2)
{
    BlastInitHSP *h1, *h2;
    int result = 0;

    h1 = (BlastInitHSP *) v1;
    h2 = (BlastInitHSP *) v2;

    /* Check if ungapped_data substructures are initialized. If not, move
       those array elements to the end. In reality this should never happen. */
    if (h1->ungapped_data == NULL && h2->ungapped_data == NULL)
        return 0;
    else if (h1->ungapped_data == NULL)
        return 1;
    else if (h2->ungapped_data == NULL)
        return -1;

    if (0 == (result = BLAST_CMP(h2->ungapped_data->score,
                                 h1->ungapped_data->score)) &&
        0 == (result = BLAST_CMP(h1->ungapped_data->s_start,
                                 h2->ungapped_data->s_start)) &&
        0 == (result = BLAST_CMP(h2->ungapped_data->length,
                                 h1->ungapped_data->length)) &&
        0 == (result = BLAST_CMP(h1->ungapped_data->q_start,
                                 h2->ungapped_data->q_start))) {
        result = BLAST_CMP(h2->ungapped_data->length,
                           h1->ungapped_data->length);
    }

    return result;
}

void Blast_InitHitListSortByScore(BlastInitHitList * init_hitlist)
{
    qsort(init_hitlist->init_hsp_array, init_hitlist->total,
          sizeof(BlastInitHSP), score_compare_match);
}

Boolean Blast_InitHitListIsSortedByScore(BlastInitHitList * init_hitlist)
{
    Int4 index;
    BlastInitHSP *init_hsp_array = init_hitlist->init_hsp_array;

    for (index = 0; index < init_hitlist->total - 1; ++index) {
        if (score_compare_match(&init_hsp_array[index],
                                &init_hsp_array[index + 1]) > 0)
            return FALSE;
    }
    return TRUE;
}

/* Description in blast_extend.h */
Int2 BlastExtendWordNew(const LookupTableWrap * lookup_wrap,
                        Uint4 query_length,
                        const BlastInitialWordParameters * word_params,
                        Uint4 subject_length, Blast_ExtendWord ** ewp_ptr)
{
    Blast_ExtendWord *ewp;
    Int4 index;

    *ewp_ptr = ewp = (Blast_ExtendWord *) calloc(1, sizeof(Blast_ExtendWord));

    if (!ewp) {
        return -1;
    }

    if (word_params->container_type == eDiagHash) {
        double search_space;
        Int4 capacity, num_buckets;
        BLAST_DiagHash *hash_table;

        ewp->hash_table = hash_table =
            (BLAST_DiagHash *) calloc(1, sizeof(BLAST_DiagHash));

        search_space = ((double) query_length) * subject_length;
        num_buckets = 512;
        capacity = 8;
        hash_table->size = (Int4 *) calloc(num_buckets, sizeof(Int4));
        hash_table->capacity = (Int4 *) malloc(num_buckets * sizeof(Int4));

        hash_table->backbone =
            (DiagHashCell **) malloc(num_buckets * sizeof(DiagHashCell *));
        for (index = 0; index < num_buckets; index++) {
            hash_table->backbone[index] =
                (DiagHashCell *) malloc(capacity * sizeof(DiagHashCell));
            hash_table->capacity[index] = capacity;
        }
        hash_table->num_buckets = num_buckets;
    } else {                    /* container_type == eDiagArray */

        Boolean multiple_hits = (word_params->options->window_size > 0);
        BLAST_DiagTable *diag_table;

        ewp->diag_table = diag_table =
            BlastDiagTableNew(query_length, multiple_hits,
                              word_params->options->window_size);
        /* Allocate the buffer to be used for diagonal array. */

        diag_table->hit_level_array = (DiagStruct *)
            calloc(diag_table->diag_array_length, sizeof(DiagStruct));
        if (!diag_table->hit_level_array) {
            sfree(ewp);
            return -1;
        }
    }
    *ewp_ptr = ewp;

    return 0;
}

Boolean BLAST_SaveInitialHit(BlastInitHitList * init_hitlist,
                             Int4 q_off, Int4 s_off,
                             BlastUngappedData * ungapped_data)
{
    BlastInitHSP *match_array;
    Int4 num, num_avail;

    num = init_hitlist->total;
    num_avail = init_hitlist->allocated;

    match_array = init_hitlist->init_hsp_array;
    if (num >= num_avail) {
        if (init_hitlist->do_not_reallocate)
            return FALSE;
        num_avail *= 2;
        match_array = (BlastInitHSP *)
            realloc(match_array, num_avail * sizeof(BlastInitHSP));
        if (!match_array) {
            init_hitlist->do_not_reallocate = TRUE;
            return FALSE;
        } else {
            init_hitlist->allocated = num_avail;
            init_hitlist->init_hsp_array = match_array;
        }
    }

    match_array[num].offsets.qs_offsets.q_off = q_off;
    match_array[num].offsets.qs_offsets.s_off = s_off;
    match_array[num].ungapped_data = ungapped_data;

    init_hitlist->total++;
    return TRUE;
}

/** Perform ungapped extension of a word hit, using a score
 *  matrix and extending one base at a time
 * @param query The query sequence [in]
 * @param subject The subject sequence [in]
 * @param matrix The scoring matrix [in]
 * @param q_off The offset of a word in query [in]
 * @param s_off The offset of a word in subject [in]
 * @param X The drop-off parameter for the ungapped extension [in]
 * @param ungapped_data The ungapped extension information [out]
 */
static void
s_NuclUngappedExtendExact(BLAST_SequenceBlk * query,
                          BLAST_SequenceBlk * subject, Int4 ** matrix,
                          Int4 q_off, Int4 s_off, Int4 X,
                          BlastUngappedData * ungapped_data)
{
    Uint1 *q;
    Int4 sum, score;
    Uint1 ch;
    Uint1 *subject0, *sf, *q_beg, *q_end, *s_end, *s, *start;
    Int2 remainder, base;
    Int4 q_avail, s_avail;

    base = 3 - (s_off % 4);

    subject0 = subject->sequence;
    q_avail = query->length - q_off;
    s_avail = subject->length - s_off;

    q = q_beg = q_end = query->sequence + q_off;
    s = s_end = subject0 + s_off / COMPRESSION_RATIO;
    if (q_off < s_off) {
        start = subject0 + (s_off - q_off) / COMPRESSION_RATIO;
        remainder = 3 - ((s_off - q_off) % COMPRESSION_RATIO);
    } else {
        start = subject0;
        remainder = 3;
    }

    score = 0;
    sum = 0;

    /* extend to the left */
    while ((s > start) || (s == start && base < remainder)) {
        if (base == 3) {
            s--;
            base = 0;
        } else {
            ++base;
        }
        ch = *s;
        if ((sum += matrix[*--q][NCBI2NA_UNPACK_BASE(ch, base)]) > 0) {
            q_beg = q;
            score += sum;
            sum = 0;
        } else if (sum < X) {
            break;
        }
    }

    ungapped_data->q_start = q_beg - query->sequence;
    ungapped_data->s_start = s_off - (q_off - ungapped_data->q_start);

    if (q_avail < s_avail) {
        sf = subject0 + (s_off + q_avail) / COMPRESSION_RATIO;
        remainder = 3 - ((s_off + q_avail) % COMPRESSION_RATIO);
    } else {
        sf = subject0 + (subject->length) / COMPRESSION_RATIO;
        remainder = 3 - ((subject->length) % COMPRESSION_RATIO);
    }

    /* extend to the right */
    q = query->sequence + q_off;
    s = subject0 + s_off / COMPRESSION_RATIO;
    sum = 0;
    base = 3 - (s_off % COMPRESSION_RATIO);

    while (s < sf || (s == sf && base > remainder)) {
        ch = *s;
        if ((sum += matrix[*q++][NCBI2NA_UNPACK_BASE(ch, base)]) > 0) {
            q_end = q;
            score += sum;
            sum = 0;
        } else if (sum < X)
            break;
        if (base == 0) {
            base = 3;
            s++;
        } else
            base--;
    }

    ungapped_data->length = q_end - q_beg;
    ungapped_data->score = score;
}

/** Perform ungapped extension of a word hit. Use an approximate method
 * and revert to rigorous ungapped alignment if the approximate score
 * is high enough
 * @param query The query sequence [in]
 * @param subject The subject sequence [in]
 * @param matrix The scoring matrix [in]
 * @param q_off The offset of a word in query [in]
 * @param s_match_end The first offset in the subject sequence that 
 *              is not known to exactly match the query sequence [in]
 * @param s_off The offset of a word in subject [in]
 * @param X The drop-off parameter for the ungapped extension [in]
 * @param ungapped_data The ungapped extension information [out]
 * @param score_table Array containing precompute sums of
 *                    match and mismatch scores [in]
 * @param reduced_cutoff Score beyond which a rigorous extension is used [in]
 */
static void
s_NuclUngappedExtend(BLAST_SequenceBlk * query,
                     BLAST_SequenceBlk * subject, Int4 ** matrix,
                     Int4 q_off, Int4 s_match_end, Int4 s_off,
                     Int4 X, BlastUngappedData * ungapped_data,
                     const Int4 * score_table, Int4 reduced_cutoff)
{
    Uint1 *q_start = query->sequence;
    Uint1 *s_start = subject->sequence;
    Uint1 *q;
    Uint1 *s;
    Int4 sum, score;
    Int4 i, len;
    Uint1 *new_q;
    Int4 q_ext, s_ext;

    /* The left extension begins behind (q_ext,s_ext); this is the first
       4-base boundary after s_off. */

    len = (COMPRESSION_RATIO - (s_off % COMPRESSION_RATIO)) %
        COMPRESSION_RATIO;
    q_ext = q_off + len;
    s_ext = s_off + len;
    q = q_start + q_ext;
    s = s_start + s_ext / COMPRESSION_RATIO;
    len = MIN(q_ext, s_ext) / COMPRESSION_RATIO;

    score = 0;
    sum = 0;
    new_q = q;

    for (i = 0; i < len; s--, q -= 4, i++) {
        Uint1 s_byte = s[-1];
        Uint1 q_byte = (q[-4] << 6) | (q[-3] << 4) | (q[-2] << 2) | q[-1];

        sum += score_table[q_byte ^ s_byte];
        if (sum > 0) {
            new_q = q - 4;
            score += sum;
            sum = 0;
        }
        if (sum < X) {
            break;
        }
    }

    /* record the start point of the extension */

    ungapped_data->q_start = new_q - q_start;
    ungapped_data->s_start = s_ext - (q_ext - ungapped_data->q_start);

    /* the right extension begins at the first bases not examined by the
       previous loop */

    q = q_start + q_ext;
    s = s_start + s_ext / COMPRESSION_RATIO;
    len = MIN(query->length - q_ext, subject->length - s_ext) /
        COMPRESSION_RATIO;
    sum = 0;
    new_q = q;

    for (i = 0; i < len; s++, q += 4, i++) {
        Uint1 s_byte = s[0];
        Uint1 q_byte = (q[0] << 6) | (q[1] << 4) | (q[2] << 2) | q[3];

        sum += score_table[q_byte ^ s_byte];
        if (sum > 0) {
            new_q = q + 3;
            score += sum;
            sum = 0;
        }
        if (sum < X) {
            break;
        }
    }

    if (score >= reduced_cutoff) {
        /* the current score is high enough; throw away the alignment and
           recompute it rigorously */
        s_NuclUngappedExtendExact(query, subject, matrix, q_off,
                                  s_off, X, ungapped_data);
    } else {
        /* record the length and score of the extension. Make sure the
           alignment extends at least to s_match_end */
        ungapped_data->score = score;
        ungapped_data->length = MAX(s_match_end - ungapped_data->s_start,
                                    (new_q - q_start) -
                                    ungapped_data->q_start + 1);
    }
}

/** Perform ungapped extension given an offset pair, and save the initial 
 * hit information if the hit qualifies. This function assumes that the
 * exact match has already been extended to the word size parameter. It also
 * supports a two-hit version, where extension is performed only after two hits
 * are detected within a window.
 * @param query The query sequence [in]
 * @param subject The subject sequence [in]
 * @param min_step Distance at which new word hit lies within the previously
 *                 extended hit. Non-zero only when ungapped extension is not
 *                 performed, e.g. for contiguous megablast. [in]
 * @param template_length specifies overlap of two words in two-hit model [in]
 * @param word_params The parameters related to initial word extension [in]
 * @param matrix the substitution matrix for ungapped extension [in]
 * @param diag_table Structure containing diagonal table with word extension 
 *                   information [in]
 * @param q_off The offset in the query sequence [in]
 * @param s_end The offset in the subject sequence where this hit ends [in]
 * @param s_off The offset in the subject sequence [in]
 * @param init_hitlist The structure containing information about all 
 *                     initial hits [in] [out]
 * @return Has this hit been extended? 
 */
static Boolean
s_BlastnDiagTableExtendInitialHit(BLAST_SequenceBlk * query,
                             BLAST_SequenceBlk * subject, Uint4 min_step,
                             Uint1 template_length,
                             const BlastInitialWordParameters * word_params,
                             Int4 ** matrix, BLAST_DiagTable * diag_table,
                             Int4 q_off, Int4 s_end, Int4 s_off,
                             BlastInitHitList * init_hitlist)
{
    Int4 diag, real_diag;
    Int4 s_pos;
    BlastUngappedData *ungapped_data;
    BlastUngappedData dummy_ungapped_data;
    Int4 window_size = word_params->options->window_size;
    Boolean hit_ready;
    Boolean new_hit = FALSE, second_hit = FALSE;
    Int4 step;
    Uint4 last_hit;
    Uint4 hit_saved;
    DiagStruct *hit_level_array;

    hit_level_array = diag_table->hit_level_array;
    ASSERT(hit_level_array);

    diag = s_off + diag_table->diag_array_length - q_off;
    real_diag = diag & diag_table->diag_mask;
    last_hit = hit_level_array[real_diag].last_hit;
    hit_saved = hit_level_array[real_diag].flag;
    s_pos = s_end + diag_table->offset;
    step = s_pos - last_hit;

    if (step <= 0)
        /* This is a hit on a diagonal that has already been explored
           further down */
        return 0;

    if (window_size == 0 || hit_saved) {
        /* Single hit version or previous hit was already a second hit */
        new_hit = (step > (Int4) min_step);
    } else {
        /* Previous hit was the first hit */
        new_hit = (step > window_size);
        second_hit = (step >= template_length && step < window_size);
    }

    hit_ready = ((window_size == 0) && new_hit) || second_hit;

    if (hit_ready) {
        if (word_params->options->ungapped_extension) {
            /* Perform ungapped extension */
            ungapped_data = &dummy_ungapped_data;
            s_NuclUngappedExtend(query, subject, matrix, q_off, s_end, s_off,
                                 -word_params->x_dropoff, ungapped_data,
                                 word_params->nucl_score_table,
                                 word_params->reduced_nucl_cutoff_score);

            last_hit = ungapped_data->length + ungapped_data->s_start
                + diag_table->offset;
        } else {
            ungapped_data = NULL;
            last_hit = s_pos;
        }
        if (ungapped_data == NULL) {
            BLAST_SaveInitialHit(init_hitlist, q_off, s_off, ungapped_data);
            /* Set the "saved" flag for this hit */
            hit_saved = 1;
        } else if (ungapped_data->score >= word_params->cutoff_score) {
            BlastUngappedData *final_data =
                (BlastUngappedData *) malloc(sizeof(BlastUngappedData));
            *final_data = *ungapped_data;
            BLAST_SaveInitialHit(init_hitlist, q_off, s_off, final_data);
            /* Set the "saved" flag for this hit */
            hit_saved = 1;
        } else {
            /* Unset the "saved" flag for this hit */
            hit_saved = 0;
        }
    } else {
        /* First hit in the 2-hit case or a direct extension of the previous
           hit - update the last hit information only */
        if (step > window_size)
            last_hit = s_pos;
        if (new_hit)
            hit_saved = 0;
    }

    hit_level_array[real_diag].last_hit = last_hit;
    hit_level_array[real_diag].flag = hit_saved;

    return hit_ready;
}

/** Discontiguous megablast initial word extension using diagonal array.
 * @param query Query sequence block [in]
 * @param subject Subject sequence block [in]
 * @param lookup Lookup table [in]
 * @param word_params Parameters for finding and extending initial words [in]
 * @param matrix Matrix for ungapped extension [in]
 * @param diag_table Structure containing diagonal array [in]
 * @param num_hits Number of lookup table hits [in]
 * @param offset_pairs Array of query and subject offsets of lookup table 
 *                     hits [in]
 * @param init_hitlist Structure where to save hits when they are 
 *                     ready [in] [out]
 * @return Number of hits extended.
 */
static Int4
s_DiscMbDiagTableExtendInitialHits(BLAST_SequenceBlk * query,
                              BLAST_SequenceBlk * subject,
                              LookupTableWrap * lookup,
                              const BlastInitialWordParameters * word_params,
                              Int4 ** matrix, BLAST_DiagTable * diag_table,
                              Int4 num_hits,
                              const BlastOffsetPair * offset_pairs,
                              BlastInitHitList * init_hitlist)
{
    BlastMBLookupTable *mb_lt = (BlastMBLookupTable *) lookup->lut;
    Int4 index;
    Int4 hits_extended = 0;

    for (index = 0; index < num_hits; ++index) {
        if (s_BlastnDiagTableExtendInitialHit
            (query, subject, mb_lt->scan_step, mb_lt->template_length,
             word_params, matrix, diag_table,
             offset_pairs[index].qs_offsets.q_off,
             offset_pairs[index].qs_offsets.s_off,
             offset_pairs[index].qs_offsets.s_off, init_hitlist))
            ++hits_extended;
    }

    return hits_extended;

}

/** Perform ungapped extension given an offset pair, and save the initial 
 * hit information if the hit qualifies. This function assumes that the
 * exact match has already been extended to the word size parameter.
 * @param query The query sequence [in]
 * @param subject The subject sequence [in]
 * @param min_step Distance at which new word hit lies within the previously
 *                 extended hit. Non-zero only when ungapped extension is not
 *                 performed, e.g. for contiguous megablast. [in]
 * @param template_length specifies overlap of two words in two-hit model [in]
 * @param word_params The parameters related to initial word extension [in]
 * @param matrix the substitution matrix for ungapped extension [in]
 * @param hash_table Structure containing initial hits [in] [out]
 * @param q_off The offset in the query sequence [in]
 * @param s_end The offset in the subject sequence where this hit ends [in]
 * @param s_off The offset in the subject sequence [in]
 * @param init_hitlist The structure containing information about all 
 *                     initial hits [in] [out]
 * @return Has this hit been extended? 
 */
static Boolean
s_BlastnDiagHashExtendInitialHit(BLAST_SequenceBlk * query,
                               BLAST_SequenceBlk * subject, Uint4 min_step,
                               Uint1 template_length,
                               const BlastInitialWordParameters * word_params,
                               Int4 ** matrix, BLAST_DiagHash * hash_table,
                               Int4 q_off, Int4 s_end, Int4 s_off,
                               BlastInitHitList * init_hitlist)
{
    Int4 index, diag;
    Int4 window_size;
    Boolean hit_ready = FALSE, two_hits;
    BlastUngappedData dummy_ungapped_data;
    BlastUngappedData *ungapped_data = NULL;
    Int4 rc;
    Int4 last_hit, hit_saved;

    window_size = word_params->options->window_size;
    two_hits = (window_size > 0);
    diag = s_off - q_off;

    rc = s_BlastDiagHashRetrieve(hash_table, diag, &last_hit, &hit_saved);

    /* if we found a previous hit on this diagonal */
    if (rc) {
        Int4 step = s_end - last_hit;
        Boolean new_hit = FALSE;
        Boolean second_hit = FALSE;

        /* This is a hit on a diagonal that has already been explored
           further down */
        if (step <= 0)
            return 0;

        if (!two_hits || hit_saved) {
            /* Single hit version or previous hit was already a second hit */
            new_hit = (step > (Int4) min_step);
        } else {
            /* Previous hit was the first hit */
            new_hit = (step > window_size);
            second_hit = (step >= template_length && step < window_size);
        }

        hit_ready = (!two_hits && new_hit) || second_hit;

        if (hit_ready) {
            if (word_params->options->ungapped_extension) {
                /* Perform ungapped extension */
                ungapped_data = &dummy_ungapped_data;
                s_NuclUngappedExtend(query, subject, matrix, q_off, s_end,
                                     s_off, -word_params->x_dropoff,
                                     ungapped_data,
                                     word_params->nucl_score_table,
                                     word_params->reduced_nucl_cutoff_score);

                last_hit = ungapped_data->length + ungapped_data->s_start;
            } else {
                ungapped_data = NULL;
                last_hit = s_end;
            }
            if (ungapped_data == NULL) {
                BLAST_SaveInitialHit(init_hitlist, q_off, s_off,
                                     ungapped_data);
                /* Set the "saved" flag for this hit */
                hit_saved = 1;
            } else if (ungapped_data->score >= word_params->cutoff_score) {
                BlastUngappedData *final_data =
                    (BlastUngappedData *) malloc(sizeof(BlastUngappedData));
                *final_data = *ungapped_data;
                BLAST_SaveInitialHit(init_hitlist, q_off, s_off, final_data);
                /* Set the "saved" flag for this hit */
                hit_saved = 1;
            } else {
                /* Unset the "saved" flag for this hit */
                hit_saved = 0;
            }
        } else {
            /* First hit in the 2-hit case or a direct extension of the
               previous hit - update the last hit information only */
            if (step > window_size)
                last_hit = s_end;
            if (new_hit)
                hit_saved = 0;
        }

        s_BlastDiagHashInsert(hash_table, diag, last_hit, hit_saved, s_end, window_size, min_step,two_hits);

        return hit_ready;
    } else {                    /* we did not find a previous hit on this
                                   diagonal */

        Int4 level = s_end;

        /* Save the hit if it already qualifies */
        if (!two_hits) {
            hit_ready = TRUE;
            if (word_params->options->ungapped_extension) {
                /* Perform ungapped extension */
                ungapped_data = &dummy_ungapped_data;
                s_NuclUngappedExtend(query, subject, matrix, q_off, s_end,
                                     s_off, -word_params->x_dropoff,
                                     ungapped_data,
                                     word_params->nucl_score_table,
                                     word_params->reduced_nucl_cutoff_score);
                level = (ungapped_data->length + ungapped_data->s_start);
            } else {
                ungapped_data = NULL;
            }
            if (ungapped_data == NULL) {
                BLAST_SaveInitialHit(init_hitlist, q_off, s_off,
                                     ungapped_data);
                hit_saved = 1;
            } else if (ungapped_data->score >= word_params->cutoff_score) {
                BlastUngappedData *final_data =
                    (BlastUngappedData *) malloc(sizeof(BlastUngappedData));
                *final_data = *ungapped_data;
                BLAST_SaveInitialHit(init_hitlist, q_off, s_off, final_data);
                hit_saved = 1;
            } else {
                /* Set hit length back to 0 after ungapped extension failure 
                 */
                hit_saved = 0;
            }
        }

        s_BlastDiagHashInsert(hash_table, diag, level, hit_saved, s_end, window_size,min_step,two_hits);

        return hit_ready;
    }
}

/** Discontiguous megablast initial word extension using a hash table.
 * Finds previous hit on this hit's diagonal among the hits saved in the 
 * hash table and updates that hit information, or adds a new hit to the hash table.
 * Saves hit if it has reached the word size and/or it is a second
 * hit in case of multiple-hit option.
 * @param query Query sequence block [in]
 * @param subject Subject sequence block [in]
 * @param lookup Lookup table [in]
 * @param word_params Parameters for finding and extending initial words [in]
 * @param matrix Matrix for ungapped extension [in]
 * @param hash_table Structure containing word hits [in]
 * @param num_hits Number of lookup table hits [in]
 * @param offset_pairs Array of query and subject offsets of lookup table 
 *                     hits [in]
 * @param init_hitlist Structure where to save hits when they are 
 *                     ready [in] [out]
 * @return Number of hits extended.
 */
static Int4
s_DiscMbDiagHashExtendInitialHits(BLAST_SequenceBlk * query,
                                BLAST_SequenceBlk * subject,
                                LookupTableWrap * lookup,
                                const BlastInitialWordParameters *
                                word_params, Int4 ** matrix,
                                BLAST_DiagHash * hash_table, Int4 num_hits,
                                const BlastOffsetPair * offset_pairs,
                                BlastInitHitList * init_hitlist)
{
    BlastMBLookupTable *mb_lt = (BlastMBLookupTable *) lookup->lut;
    Int4 index;
    Int4 hits_extended = 0;

    for (index = 0; index < num_hits; ++index) {
        if (s_BlastnDiagHashExtendInitialHit
            (query, subject, mb_lt->scan_step, mb_lt->template_length,
             word_params, matrix, hash_table,
             offset_pairs[index].qs_offsets.q_off,
             offset_pairs[index].qs_offsets.s_off,
             offset_pairs[index].qs_offsets.s_off, init_hitlist))
            ++hits_extended;
    }

    return hits_extended;
}

Boolean
DiscMB_ExtendInitialHits(const BlastOffsetPair * offset_pairs,
                         Int4 num_hits, BLAST_SequenceBlk * query,
                         BLAST_SequenceBlk * subject,
                         LookupTableWrap * lookup,
                         const BlastInitialWordParameters * word_params,
                         Int4 ** matrix, Blast_ExtendWord * ewp,
                         BlastInitHitList * init_hitlist)
{
    ASSERT(lookup->lut_type == MB_LOOKUP_TABLE &&
           ((BlastMBLookupTable *) lookup->lut)->discontiguous);

    if (word_params->container_type == eDiagArray) {
        return s_DiscMbDiagTableExtendInitialHits(query, subject, lookup,
                                             word_params, matrix,
                                             ewp->diag_table, num_hits,
                                             offset_pairs, init_hitlist);
    } else if (word_params->container_type == eDiagHash) {
        return s_DiscMbDiagHashExtendInitialHits(query, subject, lookup,
                                               word_params, matrix,
                                               ewp->hash_table, num_hits,
                                               offset_pairs, init_hitlist);
    }

    return FALSE;
}

/** Update the word extension structure after scanning of each subject sequence
 * @param ewp The structure holding word extension information [in] [out]
 * @param subject_length The length of the subject sequence that has just been
 *        processed [in]
 */
static Int2
s_BlastNaExtendWordExit(Blast_ExtendWord * ewp, Int4 subject_length)
{
    if (!ewp)
        return -1;

    if (ewp->diag_table) {
        BLAST_DiagTable *diag_table;
        Int4 diag_array_length;

        diag_table = ewp->diag_table;

        if (diag_table->offset >= INT4_MAX / 2) {
            diag_array_length = diag_table->diag_array_length;
            if (diag_table->hit_level_array) {
                memset(diag_table->hit_level_array, 0,
                       diag_array_length * sizeof(DiagStruct));
            }
        }

        if (diag_table->offset < INT4_MAX / 2) {
            diag_table->offset += subject_length + diag_table->window;
        } else {
            diag_table->offset = diag_table->window;
        }
    } else if (ewp->hash_table) {
        memset(ewp->hash_table->size, 0,
               ewp->hash_table->num_buckets * sizeof(Int4));
    }

    return 0;
}

Int4
BlastNaExtendRight(const BlastOffsetPair * offset_pairs, Int4 num_hits,
                   const BlastInitialWordParameters * word_params,
                   LookupTableWrap * lookup_wrap,
                   BLAST_SequenceBlk * query, BLAST_SequenceBlk * subject,
                   Int4 ** matrix, Blast_ExtendWord * ewp,
                   BlastInitHitList * init_hitlist)
{
    Int4 index;
    Uint4 query_length = query->length;
    Uint4 subject_length = subject->length;
    Uint1 *q_start = query->sequence;
    Uint1 *s_start = subject->sequence;
    Uint4 word_length, lut_word_length;
    Uint4 q_off, s_off;
    Uint4 max_bases_left, max_bases_right;
    Uint4 extended_left, extended_right;
    Uint4 extra_bases;
    Uint1 *q, *s;
    Uint4 min_step = 0;
    Int4 hits_extended = 0;
    Uint1 template_length = 0;
    Boolean hit_ready;
    BlastLookupTable *lut;

    ASSERT(lookup_wrap->lut_type != MB_LOOKUP_TABLE);
    lut = (BlastLookupTable *) lookup_wrap->lut;
    word_length = lut->word_length;
    lut_word_length = lut->lut_word_length;
    extra_bases = word_length - lut_word_length;

    for (index = 0; index < num_hits; ++index) {
        Uint4 s_offset = offset_pairs[index].qs_offsets.s_off;
        Uint4 q_offset = offset_pairs[index].qs_offsets.q_off;

        /* begin with the left extension; the initialization is slightly
           faster. q below points to the first base of the lookup table hit
           and s points to the first four bases of the hit (which is
           guaranteed to be aligned on a byte boundary) */

        max_bases_left = MIN(extra_bases, MIN(q_offset, s_offset));
        q = q_start + q_offset;
        s = s_start + s_offset / COMPRESSION_RATIO;

        for (extended_left = 0; extended_left < max_bases_left;
             s--, q -= 4, extended_left++) {
            Uint1 byte = s[-1];

            if ((byte & 3) != q[-1] || ++extended_left == max_bases_left)
                break;
            if (((byte >> 2) & 3) != q[-2]
                || ++extended_left == max_bases_left)
                break;
            if (((byte >> 4) & 3) != q[-3]
                || ++extended_left == max_bases_left)
                break;
            if ((byte >> 6) != q[-4])
                break;
        }

        /* do the right extension if the left extension did not find all the
           bases required. Begin at the first base past the lookup table hit 
           and move forwards */

        s_off = s_offset + lut_word_length;
        extended_right = 0;

        if (extended_left < extra_bases) {
            q_off = q_offset + lut_word_length;
            q = q_start + q_off;
            s = s_start + s_off / COMPRESSION_RATIO;
            max_bases_right = MIN(extra_bases - extended_left,
                                  MIN(query_length - q_off,
                                      subject_length - s_off));

            for (; extended_right < max_bases_right;
                 s++, q += 4, extended_right++) {
                Uint1 byte = s[0];

                if ((byte >> 6) != q[0]
                    || ++extended_right == max_bases_right)
                    break;
                if (((byte >> 4) & 3) != q[1]
                    || ++extended_right == max_bases_right)
                    break;
                if (((byte >> 2) & 3) != q[2]
                    || ++extended_right == max_bases_right)
                    break;
                if ((byte & 3) != q[3])
                    break;
            }

            /* check if enough extra matches were found */

            if (extended_left + extended_right < extra_bases)
                continue;
        }

        /* check the diagonal on which the hit lies. The boundaries extend
           from the first match of the hit to one beyond the last match */

        if (word_params->container_type == eDiagHash) {
            hit_ready =
                s_BlastnDiagHashExtendInitialHit(query, subject, min_step,
                                               template_length, word_params,
                                               matrix, ewp->hash_table,
                                               q_offset - extended_left,
                                               s_off + extended_right,
                                               s_offset - extended_left,
                                               init_hitlist);
        } else {
            hit_ready = s_BlastnDiagTableExtendInitialHit(query, subject, min_step,
                                                     template_length,
                                                     word_params, matrix,
                                                     ewp->diag_table,
                                                     q_offset - extended_left,
                                                     s_off + extended_right,
                                                     s_offset - extended_left,
                                                     init_hitlist);
        }
        if (hit_ready)
            ++hits_extended;
    }
    return hits_extended;
}

/* Description in blast_extend.h */
Int2 BlastNaWordFinder(BLAST_SequenceBlk * subject,
                       BLAST_SequenceBlk * query,
                       LookupTableWrap * lookup_wrap,
                       Int4 ** matrix,
                       const BlastInitialWordParameters * word_params,
                       Blast_ExtendWord * ewp,
                       BlastOffsetPair * offset_pairs,
                       Int4 max_hits,
                       BlastInitHitList * init_hitlist,
                       BlastUngappedStats * ungapped_stats)
{
    BlastLookupTable *lookup = (BlastLookupTable *) lookup_wrap->lut;
    Int4 hitsfound, total_hits = 0;
    Int4 hits_extended = 0;
    Int4 start_offset, last_start, next_start;

    last_start = subject->length - lookup->lut_word_length;
    start_offset = 0;

    while (start_offset <= last_start) {
        /* Pass the last word ending offset */
        next_start = last_start;
        hitsfound = BlastNaScanSubject(lookup_wrap, subject, start_offset,
                                       offset_pairs, max_hits, &next_start);

        total_hits += hitsfound;

        hits_extended +=
            BlastNaExtendRight(offset_pairs, hitsfound, word_params,
                               lookup_wrap, query, subject, matrix, ewp,
                               init_hitlist);

        start_offset = next_start;
    }

    s_BlastNaExtendWordExit(ewp, subject->length);

    Blast_UngappedStatsUpdate(ungapped_stats, total_hits, hits_extended,
                              init_hitlist->total);

    if (word_params->options->ungapped_extension)
        Blast_InitHitListSortByScore(init_hitlist);

    return 0;
}

Int4
BlastNaExtendRightAndLeft(const BlastOffsetPair * offset_pairs, Int4 num_hits,
                          const BlastInitialWordParameters * word_params,
                          LookupTableWrap * lookup_wrap,
                          BLAST_SequenceBlk * query,
                          BLAST_SequenceBlk * subject, Int4 ** matrix,
                          Blast_ExtendWord * ewp,
                          BlastInitHitList * init_hitlist)
{
    Int4 index;
    Uint4 query_length = query->length;
    Uint4 subject_length = subject->length;
    Uint1 *q_start = query->sequence;
    Uint1 *s_start = subject->sequence;
    Uint4 word_length, lut_word_length;
    Uint4 q_off, s_off;
    Uint4 max_bases_left, max_bases_right;
    Uint4 extended_left, extended_right;
    Uint4 extra_bases;
    Uint1 *q, *s;
    Uint4 min_step = 0;
    Int4 hits_extended = 0;
    Uint1 template_length = 0;
    Boolean hit_ready;

    if (lookup_wrap->lut_type == MB_LOOKUP_TABLE) {
        BlastMBLookupTable *lut = (BlastMBLookupTable *) lookup_wrap->lut;
        word_length = lut->word_length;
        lut_word_length = lut->lut_word_length;
        /* When ungapped extension is not performed, the hit will be new only 
           if it more than scan_step away from the previous hit. */
        if (!word_params->options->ungapped_extension)
            min_step = lut->scan_step;
        template_length = lut->template_length;
    } else {
        BlastLookupTable *lut = (BlastLookupTable *) lookup_wrap->lut;
        word_length = lut->word_length;
        lut_word_length = lut->lut_word_length;
        if (!word_params->options->ungapped_extension)
            min_step = lut->scan_step;
    }
    extra_bases = word_length - lut_word_length;

    if (extra_bases == 0) {
        /* if the lookup table is the width of a full word, no extensions are 
           needed. Immediately check if the diagonal has been explored */

        for (index = 0; index < num_hits; ++index) {
            Uint4 s_offset = offset_pairs[index].qs_offsets.s_off;
            Uint4 q_offset = offset_pairs[index].qs_offsets.q_off;
            if (word_params->container_type == eDiagHash) {
                hit_ready =
                    s_BlastnDiagHashExtendInitialHit(query, subject, min_step,
                                                   template_length,
                                                   word_params, matrix,
                                                   ewp->hash_table, q_offset,
                                                   s_offset + lut_word_length,
                                                   s_offset, init_hitlist);
            } else {
                hit_ready =
                    s_BlastnDiagTableExtendInitialHit(query, subject, min_step,
                                                 template_length, word_params,
                                                 matrix, ewp->diag_table,
                                                 q_offset,
                                                 s_offset + lut_word_length,
                                                 s_offset, init_hitlist);
            }
            if (hit_ready)
                ++hits_extended;
        }
    } else {
        /* The initial lookup table hit must be extended. We trust that the
           bases of the hit itself are exact matches, and look only for exact 
           matches before and after the hit.

           Most of the time, the lookup table width is close to the word size 
           so only a few bases need examining. Also, most of the time (for
           random sequences) extensions will fail almost immediately (the
           first base examined will not match about 3/4 of the time). Thus it 
           is critical to reduce as much as possible all work that is not the 
           actual examination of sequence data */

        for (index = 0; index < num_hits; ++index) {
            Uint4 s_offset = offset_pairs[index].qs_offsets.s_off;
            Uint4 q_offset = offset_pairs[index].qs_offsets.q_off;

            /* begin with the left extension; the initialization is slightly
               faster. Point to the first base of the lookup table hit and
               work backwards */

            max_bases_left = MIN(extra_bases, MIN(q_offset, s_offset));
            q = q_start + q_offset;
            s = s_start + s_offset / COMPRESSION_RATIO;
            s_off = s_offset;

            for (extended_left = 0; extended_left < max_bases_left;
                 extended_left++) {
                s_off--;
                q--;
                if (s_off % COMPRESSION_RATIO == 3)
                    s--;
                if (((Uint1) (*s << (2 * (s_off % COMPRESSION_RATIO))) >> 6)
                    != *q)
                    break;
            }

            /* do the right extension if the left extension did not find all
               the bases required. Begin at the first base beyond the lookup
               table hit and move forwards */

            s_off = s_offset + lut_word_length;

            if (extended_left < extra_bases) {
                q_off = q_offset + lut_word_length;
                q = q_start + q_off;
                s = s_start + s_off / COMPRESSION_RATIO;
                max_bases_right = MIN(extra_bases - extended_left,
                                      MIN(query_length - q_off,
                                          subject_length - s_off));

                for (extended_right = 0; extended_right < max_bases_right;
                     extended_right++) {
                    if (((Uint1) (*s << (2 * (s_off % COMPRESSION_RATIO))) >>
                         6) != *q)
                        break;
                    s_off++;
                    q++;
                    if (s_off % COMPRESSION_RATIO == 0)
                        s++;
                }

                /* check if enough extra matches were found */

                if (extended_left + extended_right < extra_bases)
                    continue;
            }

            /* check the diagonal on which the hit lies. The boundaries
               extend from the first match of the hit to one beyond the last
               match */

            if (word_params->container_type == eDiagHash) {
                hit_ready =
                    s_BlastnDiagHashExtendInitialHit(query, subject, min_step,
                                                   template_length,
                                                   word_params, matrix,
                                                   ewp->hash_table,
                                                   q_offset - extended_left,
                                                   s_off,
                                                   s_offset - extended_left,
                                                   init_hitlist);
            } else {
                hit_ready =
                    s_BlastnDiagTableExtendInitialHit(query, subject, min_step,
                                                 template_length, word_params,
                                                 matrix, ewp->diag_table,
                                                 q_offset - extended_left,
                                                 s_off,
                                                 s_offset - extended_left,
                                                 init_hitlist);
            }
            if (hit_ready)
                ++hits_extended;
        }
    }
    return hits_extended;
}

/* Description in blast_extend.h */
Int2 MB_WordFinder(BLAST_SequenceBlk * subject,
                   BLAST_SequenceBlk * query,
                   LookupTableWrap * lookup_wrap,
                   Int4 ** matrix,
                   const BlastInitialWordParameters * word_params,
                   Blast_ExtendWord * ewp,
                   BlastOffsetPair * offset_pairs,
                   Int4 max_hits,
                   BlastInitHitList * init_hitlist,
                   BlastUngappedStats * ungapped_stats)
{
    /* Pointer to the beginning of the first word of the subject sequence */
    BlastMBLookupTable *mb_lt = (BlastMBLookupTable *) lookup_wrap->lut;
    Int4 hitsfound = 0;
    Int4 total_hits = 0;
    Int4 start_offset, next_start, last_start = 0, last_end = 0;
    Int4 subject_length = subject->length;
    Int4 hits_extended = 0;

    ASSERT(mb_lt->discontiguous ||
           word_params->extension_method == eRightAndLeft);

    start_offset = 0;
    if (mb_lt->discontiguous) {
        last_start = subject_length - mb_lt->template_length;
        last_end = subject_length;
    } else {
        last_end = subject_length;
        last_start = last_end - mb_lt->lut_word_length;
    }

    /* start_offset points to the beginning of the word */
    while (start_offset <= last_start) {

        /* Set the last argument's value to the end of the last word, without 
           the extra bases for the discontiguous case */
        next_start = last_end;
        if (mb_lt->discontiguous) {
            hitsfound =
                MB_DiscWordScanSubject(lookup_wrap, subject, start_offset,
                                       offset_pairs, max_hits, &next_start);
        } else {
            hitsfound = MB_AG_ScanSubject(lookup_wrap, subject, start_offset,
                                          offset_pairs, max_hits,
                                          &next_start);
        }

        switch (word_params->extension_method) {
        case eRightAndLeft:
            hits_extended +=
                BlastNaExtendRightAndLeft(offset_pairs, hitsfound,
                                          word_params, lookup_wrap, query,
                                          subject, matrix, ewp, init_hitlist);
            break;
        case eUpdateDiag:
            hits_extended +=
                DiscMB_ExtendInitialHits(offset_pairs, hitsfound, query,
                                         subject, lookup_wrap, word_params,
                                         matrix, ewp, init_hitlist);
            break;
        default:
            break;
        }
        /* next_start returned from the ScanSubject points to the beginning
           of the word */
        start_offset = next_start;
        total_hits += hitsfound;
    }

    s_BlastNaExtendWordExit(ewp, subject_length);

    Blast_UngappedStatsUpdate(ungapped_stats, total_hits, hits_extended,
                              init_hitlist->total);

    if (word_params->options->ungapped_extension)
        Blast_InitHitListSortByScore(init_hitlist);


    return 0;
}

/* Description in blast_extend.h */
Int2 BlastNaWordFinder_AG(BLAST_SequenceBlk * subject,
                          BLAST_SequenceBlk * query,
                          LookupTableWrap * lookup_wrap,
                          Int4 ** matrix,
                          const BlastInitialWordParameters * word_params,
                          Blast_ExtendWord * ewp,
                          BlastOffsetPair * offset_pairs,
                          Int4 max_hits,
                          BlastInitHitList * init_hitlist,
                          BlastUngappedStats * ungapped_stats)
{
    Int4 hitsfound, total_hits = 0;
    Int4 start_offset, end_offset, next_start;
    BlastLookupTable *lookup = (BlastLookupTable *) lookup_wrap->lut;
    Int4 hits_extended = 0;

    start_offset = 0;
    end_offset = subject->length - lookup->lut_word_length;

    /* start_offset points to the beginning of the word; end_offset is the
       beginning of the last word */
    while (start_offset <= end_offset) {
        hitsfound = BlastNaScanSubject_AG(lookup_wrap, subject, start_offset,
                                          offset_pairs, max_hits,
                                          &next_start);

        total_hits += hitsfound;

        hits_extended +=
            BlastNaExtendRightAndLeft(offset_pairs, hitsfound, word_params,
                                      lookup_wrap, query, subject,
                                      matrix, ewp, init_hitlist);

        start_offset = next_start;
    }

    s_BlastNaExtendWordExit(ewp, subject->length);

    Blast_UngappedStatsUpdate(ungapped_stats, total_hits, hits_extended,
                              init_hitlist->total);

    if (word_params->options->ungapped_extension)
        Blast_InitHitListSortByScore(init_hitlist);

    return 0;
}


/** Deallocate memory for the hash table structure.
 * @param hash_table The hash table structure to free. [in]
 * @return NULL.
 */
static BLAST_DiagHash *s_BlastDiagHashFree(BLAST_DiagHash * hash_table)
{
    Int4 index;

    if (!hash_table)
        return NULL;

    if (hash_table->backbone) {
        for (index = 0; index < hash_table->num_buckets; ++index)
            sfree(hash_table->backbone[index]);
        sfree(hash_table->backbone);
    }
    sfree(hash_table->size);
    sfree(hash_table->capacity);
    sfree(hash_table);
    return NULL;
}

Blast_ExtendWord *BlastExtendWordFree(Blast_ExtendWord * ewp)
{

    if (ewp == NULL)
        return NULL;

    BlastDiagTableFree(ewp->diag_table);
    s_BlastDiagHashFree(ewp->hash_table);
    sfree(ewp);
    return NULL;
}

void
BlastSaveInitHsp(BlastInitHitList * ungapped_hsps, Int4 q_start, Int4 s_start,
                 Int4 q_off, Int4 s_off, Int4 len, Int4 score)
{
    BlastUngappedData *ungapped_data = NULL;

    ungapped_data = (BlastUngappedData *) malloc(sizeof(BlastUngappedData));

    ungapped_data->q_start = q_start;
    ungapped_data->s_start = s_start;
    ungapped_data->length = len;
    ungapped_data->score = score;

    BLAST_SaveInitialHit(ungapped_hsps, q_off, s_off, ungapped_data);

    return;
}
