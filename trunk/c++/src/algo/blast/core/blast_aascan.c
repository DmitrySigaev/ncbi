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
 */

/** @file blast_aascan.c
 * Functions for accessing hits in the protein BLAST lookup table.
 */

#include <algo/blast/core/blast_aascan.h>
#include <algo/blast/core/blast_aalookup.h>

#ifndef SKIP_DOXYGEN_PROCESSING
static char const rcsid[] =
    "$Id$";
#endif                          /* SKIP_DOXYGEN_PROCESSING */

Int4 BlastAaScanSubject(const LookupTableWrap * lookup_wrap,
                        const BLAST_SequenceBlk * subject,
                        Int4 * offset,
                        BlastOffsetPair * NCBI_RESTRICT offset_pairs,
                        Int4 array_size)
{
    Int4 index;
    Uint1 *s;
    Uint1 *s_first;
    Uint1 *s_last;
    Int4 numhits = 0;           /* number of hits found for a given subject
                                   offset */
    Int4 totalhits = 0;         /* cumulative number of hits found */
    PV_ARRAY_TYPE *pv;
    BlastAaLookupTable *lookup;

    ASSERT(lookup_wrap->lut_type == eAaLookupTable);
    lookup = (BlastAaLookupTable *) lookup_wrap->lut;

    s_first = subject->sequence + *offset;
    s_last = subject->sequence + subject->length - lookup->word_length;
    pv = lookup->pv;

    /* prime the index */
    index = ComputeTableIndex(lookup->word_length - 1,
                              lookup->charsize, s_first);

    for (s = s_first; s <= s_last; s++) {
        /* compute the index value */
        index = ComputeTableIndexIncremental(lookup->word_length, 
                                             lookup->charsize,
                                             lookup->mask, s, index);

        /* if there are hits... */
        if (PV_TEST(pv, index, PV_ARRAY_BTS)) {
            numhits = lookup->thick_backbone[index].num_used;

            ASSERT(numhits != 0);

            /* ...and there is enough space in the destination array, */
            if (numhits <= (array_size - totalhits))
                /* ...then copy the hits to the destination */
            {
                Int4 *src;
                Int4 i;
                if (numhits <= AA_HITS_PER_CELL)
                    /* hits live in thick_backbone */
                    src = lookup->thick_backbone[index].payload.entries;
                else
                    /* hits live in overflow array */
                    src =
                        &(lookup->
                          overflow[lookup->thick_backbone[index].payload.
                                   overflow_cursor]);

                /* copy the hits. */
                for (i = 0; i < numhits; i++) {
                    offset_pairs[i + totalhits].qs_offsets.q_off = src[i];
                    offset_pairs[i + totalhits].qs_offsets.s_off =
                        s - subject->sequence;
                }

                totalhits += numhits;
            } else
                /* not enough space in the destination array; return early */
            {
                break;
            }
        } else
            /* no hits found */
        {
        }
    }

    /* if we get here, we fell off the end of the sequence */
    *offset = s - subject->sequence;

    return totalhits;
}

/** Add one query-subject pair to the list of such pairs retrieved
 *  from the RPS blast lookup table.
 * @param b the List in which the current pair will be placed [in/out]
 * @param q_off query offset [in]
 * @param s_off subject offset [in]
 */
static void s_AddToRPSBucket(RPSBucket * b, Uint4 q_off, Uint4 s_off)
{
    BlastOffsetPair *offset_pairs = b->offset_pairs;
    Int4 i = b->num_filled;
    if (i == b->num_alloc) {
        b->num_alloc *= 2;
        offset_pairs = b->offset_pairs =
            (BlastOffsetPair *) realloc(b->offset_pairs,
                                        b->num_alloc *
                                        sizeof(BlastOffsetPair));
    }
    offset_pairs[i].qs_offsets.q_off = q_off;
    offset_pairs[i].qs_offsets.s_off = s_off;
    b->num_filled++;
}

Int4 BlastRPSScanSubject(const LookupTableWrap * lookup_wrap,
                         const BLAST_SequenceBlk * sequence, Int4 * offset)
{
    Int4 index;
    Int4 table_correction;
    Uint1 *s = NULL;
    Uint1 *abs_start = sequence->sequence;
    Uint1 *s_first = NULL;
    Uint1 *s_last = NULL;
    Int4 numhits = 0;           /* number of hits found for a given subject
                                   offset */
    Int4 totalhits = 0;         /* cumulative number of hits found */
    BlastRPSLookupTable *lookup;
    RPSBackboneCell *cell;
    RPSBucket *bucket_array;
    PV_ARRAY_TYPE *pv;
    /* Buffer a large number of hits at once. The number of hits is
       independent of the search, because the structures that will contain
       them grow dynamically. A large number is needed because cache reuse
       requires that many hits to the same neighborhood of the concatenated
       database are available at any given time */
    const Int4 max_hits = 4000000;

    ASSERT(lookup_wrap->lut_type == eRPSLookupTable);
    lookup = (BlastRPSLookupTable *) lookup_wrap->lut;
    bucket_array = lookup->bucket_array;

    /* empty the previous collection of hits */

    for (index = 0; index < lookup->num_buckets; index++)
        bucket_array[index].num_filled = 0;

    s_first = abs_start + *offset;
    s_last = abs_start + sequence->length - lookup->wordsize;
    pv = lookup->pv;

    /* Calling code expects the returned sequence offsets to refer to the
       *first letter* in a word. The legacy RPS blast lookup table stores
       offsets to the *last* letter in each word, and so a correction is
       needed */

    table_correction = lookup->wordsize - 1;

    /* prime the index */
    index = ComputeTableIndex(lookup->wordsize - 1,
                          lookup->charsize, s_first);

    for (s = s_first; s <= s_last; s++) {
        /* compute the index value */
        index = ComputeTableIndexIncremental(lookup->wordsize, 
                                             lookup->charsize,
                                             lookup->mask, s, index);

        /* if there are hits... */
        if (PV_TEST(pv, index, PV_ARRAY_BTS)) {
            cell = &lookup->rps_backbone[index];
            numhits = cell->num_used;

            ASSERT(numhits != 0);

            if (numhits <= (max_hits - totalhits)) {
                Int4 *src;
                Int4 i;
                Uint4 q_off;
                Uint4 s_off = s - abs_start;
                if (numhits <= RPS_HITS_PER_CELL) {
                    for (i = 0; i < numhits; i++) {
                        q_off = cell->entries[i] - table_correction;
                        s_AddToRPSBucket(bucket_array +
                                         q_off / RPS_BUCKET_SIZE, q_off,
                                         s_off);
                    }
                } else {
                    /* hits (past the first) live in overflow array */
                    src =
                        lookup->overflow + (cell->entries[1] / sizeof(Int4));
                    q_off = cell->entries[0] - table_correction;
                    s_AddToRPSBucket(bucket_array + q_off / RPS_BUCKET_SIZE,
                                     q_off, s_off);
                    for (i = 0; i < (numhits - 1); i++) {
                        q_off = src[i] - table_correction;
                        s_AddToRPSBucket(bucket_array +
                                         q_off / RPS_BUCKET_SIZE, q_off,
                                         s_off);
                    }
                }

                totalhits += numhits;
            } else
                /* not enough space in the destination array; return early */
            {
                break;
            }
        } else
            /* no hits found */
        {
        }
    }

    /* if we get here, we fell off the end of the sequence */
    *offset = s - abs_start;

    return totalhits;
}
