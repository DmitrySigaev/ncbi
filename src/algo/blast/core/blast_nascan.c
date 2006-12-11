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

/** @file blast_nascan.c
 * Functions for scanning nucleotide BLAST lookup tables.
 */

#include <algo/blast/core/blast_nalookup.h>
#include <algo/blast/core/blast_nascan.h>
#include <algo/blast/core/blast_util.h> /* for NCBI2NA_UNPACK_BASE */

#ifndef SKIP_DOXYGEN_PROCESSING
static char const rcsid[] =
    "$Id$";
#endif                          /* SKIP_DOXYGEN_PROCESSING */

/**
* Retrieve the number of query offsets associated with this subject word.
* @param lookup The lookup table to read from. [in]
* @param index The index value of the word to retrieve. [in]
* @return The number of query offsets associated with this subject word.
*/
static NCBI_INLINE Int4 s_BlastLookupGetNumHits(BlastNaLookupTable * lookup,
                                                Int4 index)
{
    PV_ARRAY_TYPE *pv = lookup->pv;
    if (PV_TEST(pv, index, PV_ARRAY_BTS))
        return lookup->thick_backbone[index].num_used;
    else
        return 0;
}

/** 
* Copy query offsets from the lookup table to the array of offset pairs.
* @param lookup The lookup table to read from. [in]
* @param index The index value of the word to retrieve. [in]
* @param offset_pairs A pointer into the destination array. [out]
* @param s_off The subject offset to be associated with the retrieved query offset(s). [in]
*/
static NCBI_INLINE void s_BlastLookupRetrieve(BlastNaLookupTable * lookup,
                                              Int4 index,
                                              BlastOffsetPair * offset_pairs,
                                              Int4 s_off)
{
    Int4 *lookup_pos;
    Int4 num_hits = lookup->thick_backbone[index].num_used;
    Int4 i;

    /* determine if hits live in the backbone or the overflow array */

    if (num_hits <= NA_HITS_PER_CELL)
        lookup_pos = lookup->thick_backbone[index].payload.entries;
    else
        lookup_pos = lookup->overflow +
            lookup->thick_backbone[index].payload.overflow_cursor;

    /* Copy the (query,subject) offset pairs to the destination. */
    for(i=0;i<num_hits;i++) {
        offset_pairs[i].qs_offsets.q_off = lookup_pos[i];
        offset_pairs[i].qs_offsets.s_off = s_off;
    }

    return;
}

/** Scan the compressed subject sequence, returning 8-letter word hits
 * with stride 4. Assumes a standard nucleotide lookup table
 * @param lookup_wrap Pointer to the (wrapper to) lookup table [in]
 * @param subject The (compressed) sequence to be scanned for words [in]
 * @param start_offset The offset into the sequence in actual coordinates [in]
 * @param offset_pairs Array of query and subject positions where words are 
 *                found [out]
 * @param max_hits The allocated size of the above array - how many offsets 
 *        can be returned [in]
 * @param end_offset Where the scanning should stop [in], has stopped [out]
*/
static Int4 s_BlastNaScanSubject_8_4(const LookupTableWrap * lookup_wrap,
                        const BLAST_SequenceBlk * subject,
                        Int4 start_offset,
                        BlastOffsetPair * NCBI_RESTRICT offset_pairs,
                        Int4 max_hits, Int4 * end_offset)
{
    Uint1 *s;
    Uint1 *abs_start, *s_end;
    BlastNaLookupTable *lookup;
    Int4 num_hits;
    Int4 total_hits = 0;
    Int4 lut_word_length;

    ASSERT(lookup_wrap->lut_type == eNaLookupTable);
    lookup = (BlastNaLookupTable *) lookup_wrap->lut;

    lut_word_length = lookup->lut_word_length;
    ASSERT(lut_word_length == 8);

    abs_start = subject->sequence;
    s = abs_start + start_offset / COMPRESSION_RATIO;
    s_end = abs_start + (subject->length - lut_word_length) /
        COMPRESSION_RATIO;

    for (; s <= s_end; s++) {

        Int4 index = s[0] << 8 | s[1];

        num_hits = s_BlastLookupGetNumHits(lookup, index);
        if (num_hits == 0)
            continue;
        if (num_hits > (max_hits - total_hits))
            break;

        s_BlastLookupRetrieve(lookup,
                              index,
                              offset_pairs + total_hits,
                              (s - abs_start) * COMPRESSION_RATIO);
        total_hits += num_hits;
    }
    *end_offset = (s - abs_start) * COMPRESSION_RATIO;

    return total_hits;
}

/** Scan the compressed subject sequence, returning 4-to-8 letter word hits
 * at arbitrary stride. Assumes a standard nucleotide lookup table
 * @param lookup_wrap Pointer to the (wrapper to) lookup table [in]
 * @param subject The (compressed) sequence to be scanned for words [in]
 * @param start_offset The offset into the sequence in actual coordinates [in]
 * @param offset_pairs Array of query and subject positions where words are 
 *                found [out]
 * @param max_hits The allocated size of the above array - how many offsets 
 *        can be returned [in]
 * @param end_offset Where the scanning should stop [in], has stopped [out]
*/
static Int4 s_BlastNaScanSubject_Any(const LookupTableWrap * lookup_wrap,
                           const BLAST_SequenceBlk * subject,
                           Int4 start_offset,
                           BlastOffsetPair * NCBI_RESTRICT offset_pairs,
                           Int4 max_hits, Int4 * end_offset)
{
    BlastNaLookupTable *lookup;
    Uint1 *s;
    Uint1 *abs_start;
    Int4 s_off;
    Int4 num_hits;
    Int4 index;
    Int4 total_hits = 0;
    Int4 scan_step;
    Int4 mask;
    Int4 lut_word_length;
    Int4 last_offset;

    ASSERT(lookup_wrap->lut_type == eNaLookupTable);
    lookup = (BlastNaLookupTable *) lookup_wrap->lut;

    abs_start = subject->sequence;
    mask = lookup->mask;
    scan_step = lookup->scan_step;
    lut_word_length = lookup->lut_word_length;
    last_offset = subject->length - lut_word_length;

    ASSERT(lookup->scan_step > 0);

    if (lut_word_length > 5) {

        /* perform scanning for lookup tables of width 6, 7, and 8. These
           widths require two bytes of the compressed subject sequence, and
           possibly a third if the word is not aligned on a 4-base boundary */

        if (scan_step % COMPRESSION_RATIO == 0) {

            /* for strides that are a multiple of 4, words are always aligned 
               and two bytes of the subject sequence will always hold a
               complete word (plus possible extra bases that must be shifted
               away). s_end below always points to the second-to-last byte of
               subject, so we will never fetch the byte beyond the end of
               subject */

            Uint1 *s_end = abs_start + last_offset / COMPRESSION_RATIO;
            Int4 shift = 2 * (8 - lut_word_length);
            s = abs_start + start_offset / COMPRESSION_RATIO;
            scan_step = scan_step / COMPRESSION_RATIO;

            for (; s <= s_end; s += scan_step) {
                index = s[0] << 8 | s[1];
                index = index >> shift;

                num_hits = s_BlastLookupGetNumHits(lookup, index);
                if (num_hits == 0)
                    continue;
                if (num_hits > (max_hits - total_hits))
                    break;

                s_BlastLookupRetrieve(lookup,
                                      index,
                                      offset_pairs + total_hits,
                                      (s - abs_start) * COMPRESSION_RATIO);
                total_hits += num_hits;
            }
            *end_offset = (s - abs_start) * COMPRESSION_RATIO;
        } else {
            /* when the stride is not a multiple of 4, extra bases may occur
               both before and after every word read from the subject
               sequence. The portion of each 12-base region that contains the 
               actual word depends on the offset of the word and the lookup
               table width, and must be recalculated for each 12-base region

               Unlike the aligned stride case, the scanning can walk off the
               subject array for lut_word_length = 6 or 7 (length 8 may also
               do this, but only if the subject is a multiple of 4 bases in
               size, and in that case there is a sentinel byte). We avoid
               this by first handling all the cases where 12-base regions
               fit, then handling the last few offsets separately */

            Int4 last_offset3 = last_offset;
            switch (subject->length % COMPRESSION_RATIO) {
            case 2:
                if (lut_word_length == 6)
                    last_offset3--;
                break;
            case 3:
                if (lut_word_length == 6)
                    last_offset3 -= 2;
                else if (lut_word_length == 7)
                    last_offset3--;
                break;
            }

            for (s_off = start_offset; s_off <= last_offset3;
                 s_off += scan_step) {

                Int4 shift =
                    2 * (12 - (s_off % COMPRESSION_RATIO + lut_word_length));
                s = abs_start + (s_off / COMPRESSION_RATIO);

                index = s[0] << 16 | s[1] << 8 | s[2];
                index = (index >> shift) & mask;

                num_hits = s_BlastLookupGetNumHits(lookup, index);
                if (num_hits == 0)
                    continue;
                if (num_hits > (max_hits - total_hits))
                    break;

                s_BlastLookupRetrieve(lookup,
                                      index,
                                      offset_pairs + total_hits, s_off);
                total_hits += num_hits;
            }

            /* repeat the loop but only read two bytes at a time. For
               lut_word_length = 6 the loop runs at most twice, and for
               lut_word_length = 7 it runs at most once */

            for (; s_off > last_offset3 && s_off <= last_offset;
                 s_off += scan_step) {

                Int4 shift =
                    2 * (8 - (s_off % COMPRESSION_RATIO + lut_word_length));
                s = abs_start + (s_off / COMPRESSION_RATIO);

                index = s[0] << 8 | s[1];
                index = (index >> shift) & mask;

                num_hits = s_BlastLookupGetNumHits(lookup, index);
                if (num_hits == 0)
                    continue;
                if (num_hits > (max_hits - total_hits))
                    break;

                s_BlastLookupRetrieve(lookup,
                                      index,
                                      offset_pairs + total_hits, s_off);
                total_hits += num_hits;
            }
            *end_offset = s_off;
        }
    } else {
        /* perform scanning for lookup tables of width 4 and 5. Here the
           stride will never be a multiple of 4 (these tables are only used
           for very small word sizes). The last word is always two bytes from 
           the end of the sequence, unless the table width is 4 and subject is 
           a multiple of 4 bases; in that case there is a sentinel byte, so
           the following does not need to be corrected when scanning near the 
           end of the subject sequence */

        for (s_off = start_offset; s_off <= last_offset; s_off += scan_step) {

            Int4 shift =
                2 * (8 - (s_off % COMPRESSION_RATIO + lut_word_length));
            s = abs_start + (s_off / COMPRESSION_RATIO);

            index = s[0] << 8 | s[1];
            index = (index >> shift) & mask;

            num_hits = s_BlastLookupGetNumHits(lookup, index);
            if (num_hits == 0)
                continue;
            if (num_hits > (max_hits - total_hits))
                break;

            s_BlastLookupRetrieve(lookup,
                                  index,
                                  offset_pairs + total_hits,
                                  s_off);
            total_hits += num_hits;
        }
        *end_offset = s_off;
    }

    return total_hits;
}

void BlastNaChooseScanSubject(LookupTableWrap *lookup_wrap)
{
    BlastNaLookupTable *lookup = (BlastNaLookupTable *)lookup_wrap->lut;

    ASSERT(lookup_wrap->lut_type == eNaLookupTable);

    if (lookup->lut_word_length == 8 && lookup->scan_step == 0)
        lookup->scansub_callback = s_BlastNaScanSubject_8_4;
    else
        lookup->scansub_callback = s_BlastNaScanSubject_Any;
}

/** 
* Copy query offsets from a BlastSmallNaLookupTable
* @param offset_pairs A pointer into the destination array. [out]
* @param index The index value of the word to retrieve. [in]
* @param s_off The subject offset to be associated with the retrieved query offset(s). [in]
* @param total_hits The total number of query offsets save thus far [in]
* @param overflow Packed list of query offsets [in]
* @return Number of hits saved
*/
static NCBI_INLINE Int4 s_BlastSmallNaRetrieveHits(
                          BlastOffsetPair * NCBI_RESTRICT offset_pairs,
                          Int4 index, Int4 s_off,
                          Int4 total_hits, Int2 *overflow)
{
    if (index >= 0) {
        offset_pairs[total_hits].qs_offsets.q_off = index;
        offset_pairs[total_hits].qs_offsets.s_off = s_off;
        return 1;
    }
    else {
        Int4 num_hits = 0;
        Int4 src_off = -(index + 2);
        index = overflow[src_off++];
        do {
            offset_pairs[total_hits+num_hits].qs_offsets.q_off = index;
            offset_pairs[total_hits+num_hits].qs_offsets.s_off = s_off;
            num_hits++;
            index = overflow[src_off++];
        } while (index >= 0);

        return num_hits;
    }
}

/** Scan the compressed subject sequence, returning 8-letter word hits
 * with stride 4. Assumes a small-query nucleotide lookup table
 * @param lookup_wrap Pointer to the (wrapper to) lookup table [in]
 * @param subject The (compressed) sequence to be scanned for words [in]
 * @param start_offset The offset into the sequence in actual coordinates [in]
 * @param offset_pairs Array of query and subject positions where words are 
 *                found [out]
 * @param max_hits The allocated size of the above array - how many offsets 
 *        can be returned [in]
 * @param end_offset Where the scanning should stop [in], has stopped [out]
*/
static Int4 s_BlastSmallNaScanSubject_8_4(const LookupTableWrap * lookup_wrap,
                             const BLAST_SequenceBlk * subject,
                             Int4 start_offset,
                             BlastOffsetPair * NCBI_RESTRICT offset_pairs,
                             Int4 max_hits, Int4 * end_offset)
{
    BlastSmallNaLookupTable *lookup = 
                        (BlastSmallNaLookupTable *) lookup_wrap->lut;
    const Int4 kLutWordLength = 8;
    Uint1 *abs_start = subject->sequence;
    Uint1 *s = abs_start + start_offset / COMPRESSION_RATIO;
    Uint1 *s_end = abs_start + (subject->length - kLutWordLength) /
                                   COMPRESSION_RATIO;
    Int4 total_hits = 0;
    Int2 *backbone = lookup->final_backbone;
    Int2 *overflow = lookup->overflow;

    ASSERT(lookup_wrap->lut_type == eSmallNaLookupTable);
    ASSERT(lookup->lut_word_length == 8);
    max_hits -= lookup->longest_chain;

    for (; s <= s_end; s++) {

        Int4 index = s[0] << 8 | s[1];
        Int4 s_off;

        index = backbone[index];
        if (index != -1) {
            if (total_hits > max_hits)
                break;
    
            s_off = (s - abs_start) * COMPRESSION_RATIO;
            total_hits += s_BlastSmallNaRetrieveHits(offset_pairs, index,
                                            s_off, total_hits, overflow);
        }
    }

    *end_offset = (s - abs_start) * COMPRESSION_RATIO;
    return total_hits;
}

/** access the small-query lookup table */
#define SMALL_NA_ACCESS_HITS()                                  \
    if (index != -1) {                                          \
        if (total_hits > max_hits)                              \
            break;                                              \
        total_hits += s_BlastSmallNaRetrieveHits(offset_pairs,  \
                                                 index, s_off,  \
                                                 total_hits,    \
                                                 overflow);     \
    }

/** Scan the compressed subject sequence, returning 4-to-8-letter word hits
 * with arbitrary stride. Assumes a small-query nucleotide lookup table
 * @param lookup_wrap Pointer to the (wrapper to) lookup table [in]
 * @param subject The (compressed) sequence to be scanned for words [in]
 * @param start_offset The offset into the sequence in actual coordinates [in]
 * @param offset_pairs Array of query and subject positions where words are 
 *                found [out]
 * @param max_hits The allocated size of the above array - how many offsets 
 *        can be returned [in]
 * @param end_offset Where the scanning should stop [in], has stopped [out]
*/
static Int4 s_BlastSmallNaScanSubject_Any(const LookupTableWrap * lookup_wrap,
                                const BLAST_SequenceBlk * subject,
                                Int4 start_offset,
                                BlastOffsetPair * NCBI_RESTRICT offset_pairs,
                                Int4 max_hits, Int4 * end_offset)
{
    BlastSmallNaLookupTable *lookup = 
                        (BlastSmallNaLookupTable *) lookup_wrap->lut;
    Uint1 *abs_start = subject->sequence;
    Uint1 *s;
    Int4 s_off;
    Int4 index;
    Int4 total_hits = 0;
    Int4 scan_step = lookup->scan_step;
    Int4 mask = lookup->mask;
    Int4 lut_word_length = lookup->lut_word_length;
    Int4 last_offset = subject->length - lut_word_length;
    Int2 *backbone = lookup->final_backbone;
    Int2 *overflow = lookup->overflow;

    ASSERT(lookup_wrap->lut_type == eSmallNaLookupTable);
    ASSERT(scan_step > 0);
    max_hits -= lookup->longest_chain;

    if (lut_word_length > 5) {

        /* perform scanning for lookup tables of width 6, 7, and 8. These
           widths require two bytes of the compressed subject sequence, and
           possibly a third if the word is not aligned on a 4-base boundary */

        if (scan_step % COMPRESSION_RATIO == 0) {

            /* for strides that are a multiple of 4, words are always aligned 
               and two bytes of the subject sequence will always hold a
               complete word (plus possible extra bases that must be shifted
               away). s_end below always points to the second-to-last byte of
               subject, so we will never fetch the byte beyond the end of
               subject */

            Uint1 *s_end = abs_start + last_offset / COMPRESSION_RATIO;
            Int4 shift = 2 * (8 - lut_word_length);
            s = abs_start + start_offset / COMPRESSION_RATIO;
            scan_step = scan_step / COMPRESSION_RATIO;

            for (; s <= s_end; s += scan_step) {
                index = s[0] << 8 | s[1];
                index = backbone[index >> shift];
                if (index != -1) {
                    if (total_hits > max_hits)
                        break;
            
                    s_off = (s - abs_start) * COMPRESSION_RATIO;
                    total_hits += s_BlastSmallNaRetrieveHits(offset_pairs, 
                                                             index, s_off, 
                                                             total_hits, 
                                                             overflow);
                }
            }
            *end_offset = (s - abs_start) * COMPRESSION_RATIO;
        } else {
            /* when the stride is not a multiple of 4, extra bases may occur
               both before and after every word read from the subject
               sequence. The portion of each 12-base region that contains the 
               actual word depends on the offset of the word and the lookup
               table width, and must be recalculated for each 12-base region

               Unlike the aligned stride case, the scanning can walk off the
               subject array for lut_word_length = 6 or 7 (length 8 may also
               do this, but only if the subject is a multiple of 4 bases in
               size, and in that case there is a sentinel byte). We avoid
               this by first handling all the cases where 12-base regions
               fit, then handling the last few offsets separately */

            Int4 last_offset3 = last_offset;
            switch (subject->length % COMPRESSION_RATIO) {
            case 2:
                if (lut_word_length == 6)
                    last_offset3--;
                break;
            case 3:
                if (lut_word_length == 6)
                    last_offset3 -= 2;
                else if (lut_word_length == 7)
                    last_offset3--;
                break;
            }

            for (s_off = start_offset; s_off <= last_offset3;
                 s_off += scan_step) {

                Int4 shift =
                    2 * (12 - (s_off % COMPRESSION_RATIO + lut_word_length));
                s = abs_start + (s_off / COMPRESSION_RATIO);

                index = s[0] << 16 | s[1] << 8 | s[2];
                index = backbone[(index >> shift) & mask];
                SMALL_NA_ACCESS_HITS();
            }

            /* repeat the loop but only read two bytes at a time. For
               lut_word_length = 6 the loop runs at most twice, and for
               lut_word_length = 7 it runs at most once */

            for (; s_off > last_offset3 && s_off <= last_offset;
                 s_off += scan_step) {

                Int4 shift =
                    2 * (8 - (s_off % COMPRESSION_RATIO + lut_word_length));
                s = abs_start + (s_off / COMPRESSION_RATIO);

                index = s[0] << 8 | s[1];
                index = backbone[(index >> shift) & mask];
                SMALL_NA_ACCESS_HITS();
            }
            *end_offset = s_off;
        }
    } else {
        /* perform scanning for lookup tables of width 4 and 5. Here the
           stride will never be a multiple of 4 (these tables are only used
           for very small word sizes). The last word is always two bytes from 
           the end of the sequence, unless the table width is 4 and subject is 
           a multiple of 4 bases; in that case there is a sentinel byte, so
           the following does not need to be corrected when scanning near the 
           end of the subject sequence */

        for (s_off = start_offset; s_off <= last_offset; s_off += scan_step) {

            Int4 shift =
                2 * (8 - (s_off % COMPRESSION_RATIO + lut_word_length));
            s = abs_start + (s_off / COMPRESSION_RATIO);

            index = s[0] << 8 | s[1];
            index = backbone[(index >> shift) & mask];
            SMALL_NA_ACCESS_HITS();
        }
        *end_offset = s_off;
    }

    return total_hits;
}

/** Scan the compressed subject sequence, returning 4-letter word hits
 * with stride 1. Assumes a standard nucleotide lookup table
 * @param lookup_wrap Pointer to the (wrapper to) lookup table [in]
 * @param subject The (compressed) sequence to be scanned for words [in]
 * @param start_offset The offset into the sequence in actual coordinates [in]
 * @param offset_pairs Array of query and subject positions where words are 
 *                found [out]
 * @param max_hits The allocated size of the above array - how many offsets 
 *        can be returned [in]
 * @param end_offset Where the scanning should stop [in], has stopped [out]
*/
static Int4 s_BlastSmallNaScanSubject_4_1(
                                const LookupTableWrap * lookup_wrap,
                                const BLAST_SequenceBlk * subject,
                                Int4 start_offset,
                                BlastOffsetPair * NCBI_RESTRICT offset_pairs,
                                Int4 max_hits, Int4 * end_offset)
{
    BlastSmallNaLookupTable *lookup = 
                        (BlastSmallNaLookupTable *) lookup_wrap->lut;
    const Int4 kLutWordLength = 4;
    const Int4 kLutWordMask = (1 << (2 * kLutWordLength)) - 1;
    Int4 s_off = start_offset;
    Uint1 *s = subject->sequence + (s_off / COMPRESSION_RATIO);
    Int4 total_hits = 0;
    Int4 last_offset = subject->length - kLutWordLength;
    Int2 *backbone = lookup->final_backbone;
    Int2 *overflow = lookup->overflow;
    Int4 index; 
    Int4 init_index;

    max_hits -= lookup->longest_chain;
    ASSERT(lookup_wrap->lut_type == eSmallNaLookupTable);
    ASSERT(lookup->lut_word_length == 4);
    ASSERT(lookup->scan_step == 1);

    switch (s_off % COMPRESSION_RATIO) {
    case 1:
        init_index = s[0];
        goto base_1;
    case 2:
        init_index = s[0] << 8 | s[1];
        goto base_2;
    case 3:
        init_index = s[0] << 8 | s[1];
        goto base_3;
    }

    while (s_off <= last_offset) {

        init_index = s[0];
        index = backbone[init_index];
        SMALL_NA_ACCESS_HITS();
        s_off++;

base_1:
        if (s_off > last_offset)
            break;

        init_index = init_index << 8 | s[1];
        index = backbone[(init_index >> 6) & kLutWordMask];
        SMALL_NA_ACCESS_HITS();
        s_off++;

base_2:
        if (s_off > last_offset)
            break;

        index = backbone[(init_index >> 4) & kLutWordMask];
        SMALL_NA_ACCESS_HITS();
        s_off++;

base_3:
        if (s_off > last_offset)
            break;

        s++;
        index = backbone[(init_index >> 2) & kLutWordMask];
        SMALL_NA_ACCESS_HITS();
        s_off++;
    }

    *end_offset = s_off;
    return total_hits;
}

/** Scan the compressed subject sequence, returning 5-letter word hits
 * with stride 1. Assumes a standard nucleotide lookup table
 * @param lookup_wrap Pointer to the (wrapper to) lookup table [in]
 * @param subject The (compressed) sequence to be scanned for words [in]
 * @param start_offset The offset into the sequence in actual coordinates [in]
 * @param offset_pairs Array of query and subject positions where words are 
 *                found [out]
 * @param max_hits The allocated size of the above array - how many offsets 
 *        can be returned [in]
 * @param end_offset Where the scanning should stop [in], has stopped [out]
*/
static Int4 s_BlastSmallNaScanSubject_5_1(
                                const LookupTableWrap * lookup_wrap,
                                const BLAST_SequenceBlk * subject,
                                Int4 start_offset,
                                BlastOffsetPair * NCBI_RESTRICT offset_pairs,
                                Int4 max_hits, Int4 * end_offset)
{
    BlastSmallNaLookupTable *lookup = 
                        (BlastSmallNaLookupTable *) lookup_wrap->lut;
    const Int4 kLutWordLength = 5;
    const Int4 kLutWordMask = (1 << (2 * kLutWordLength)) - 1;
    Int4 s_off = start_offset;
    Uint1 *s = subject->sequence + (s_off / COMPRESSION_RATIO);
    Int4 total_hits = 0;
    Int4 last_offset = subject->length - kLutWordLength;
    Int2 *backbone = lookup->final_backbone;
    Int2 *overflow = lookup->overflow;
    Int4 index; 
    Int4 init_index;

    max_hits -= lookup->longest_chain;
    ASSERT(lookup_wrap->lut_type == eSmallNaLookupTable);
    ASSERT(lookup->lut_word_length == 5);
    ASSERT(lookup->scan_step == 1);

    switch (s_off % COMPRESSION_RATIO) {
    case 1:
        init_index = s[0] << 8 | s[1];
        goto base_1;
    case 2:
        init_index = s[0] << 8 | s[1];
        goto base_2;
    case 3:
        init_index = s[0] << 8 | s[1];
        goto base_3;
    }

    while (s_off <= last_offset) {

        init_index = s[0] << 8 | s[1];
        index = backbone[init_index >> 6];
        SMALL_NA_ACCESS_HITS();
        s_off++;

base_1:
        if (s_off > last_offset)
            break;

        index = backbone[(init_index >> 4) & kLutWordMask];
        SMALL_NA_ACCESS_HITS();
        s_off++;

base_2:
        if (s_off > last_offset)
            break;

        index = backbone[(init_index >> 2) & kLutWordMask];
        SMALL_NA_ACCESS_HITS();
        s_off++;

base_3:
        if (s_off > last_offset)
            break;

        s++;
        index = backbone[init_index & kLutWordMask];
        SMALL_NA_ACCESS_HITS();
        s_off++;
    }

    *end_offset = s_off;
    return total_hits;
}

/** Scan the compressed subject sequence, returning 6-letter word hits
 * with stride 1. Assumes a standard nucleotide lookup table
 * @param lookup_wrap Pointer to the (wrapper to) lookup table [in]
 * @param subject The (compressed) sequence to be scanned for words [in]
 * @param start_offset The offset into the sequence in actual coordinates [in]
 * @param offset_pairs Array of query and subject positions where words are 
 *                found [out]
 * @param max_hits The allocated size of the above array - how many offsets 
 *        can be returned [in]
 * @param end_offset Where the scanning should stop [in], has stopped [out]
*/
static Int4 s_BlastSmallNaScanSubject_6_1(
                                const LookupTableWrap * lookup_wrap,
                                const BLAST_SequenceBlk * subject,
                                Int4 start_offset,
                                BlastOffsetPair * NCBI_RESTRICT offset_pairs,
                                Int4 max_hits, Int4 * end_offset)
{
    BlastSmallNaLookupTable *lookup = 
                        (BlastSmallNaLookupTable *) lookup_wrap->lut;
    const Int4 kLutWordLength = 6;
    const Int4 kLutWordMask = (1 << (2 * kLutWordLength)) - 1;
    Int4 s_off = start_offset;
    Uint1 *s = subject->sequence + (s_off / COMPRESSION_RATIO);
    Int4 total_hits = 0;
    Int4 last_offset = subject->length - kLutWordLength;
    Int2 *backbone = lookup->final_backbone;
    Int2 *overflow = lookup->overflow;
    Int4 index; 
    Int4 init_index;

    max_hits -= lookup->longest_chain;
    ASSERT(lookup_wrap->lut_type == eSmallNaLookupTable);
    ASSERT(lookup->lut_word_length == 6);
    ASSERT(lookup->scan_step == 1);

    switch (s_off % COMPRESSION_RATIO) {
    case 1:
        init_index = s[0] << 8 | s[1];
        goto base_1;
    case 2:
        init_index = s[0] << 8 | s[1];
        goto base_2;
    case 3:
        init_index = s[0] << 8 | s[1];
        goto base_3;
    }

    while (s_off <= last_offset) {

        init_index = s[0] << 8 | s[1];
        index = backbone[init_index >> 4];
        SMALL_NA_ACCESS_HITS();
        s_off++;

base_1:
        if (s_off > last_offset)
            break;

        index = backbone[(init_index >> 2) & kLutWordMask];
        SMALL_NA_ACCESS_HITS();
        s_off++;

base_2:
        if (s_off > last_offset)
            break;

        index = backbone[init_index & kLutWordMask];
        SMALL_NA_ACCESS_HITS();
        s_off++;

base_3:
        if (s_off > last_offset)
            break;

        init_index = init_index << 8 | s[2];
        s++;
        index = backbone[(init_index >> 6) & kLutWordMask];
        SMALL_NA_ACCESS_HITS();
        s_off++;
    }

    *end_offset = s_off;
    return total_hits;
}

/** Scan the compressed subject sequence, returning 6-letter word hits
 * with stride 2. Assumes a standard nucleotide lookup table
 * @param lookup_wrap Pointer to the (wrapper to) lookup table [in]
 * @param subject The (compressed) sequence to be scanned for words [in]
 * @param start_offset The offset into the sequence in actual coordinates [in]
 * @param offset_pairs Array of query and subject positions where words are 
 *                found [out]
 * @param max_hits The allocated size of the above array - how many offsets 
 *        can be returned [in]
 * @param end_offset Where the scanning should stop [in], has stopped [out]
*/
static Int4 s_BlastSmallNaScanSubject_6_2(
                                const LookupTableWrap * lookup_wrap,
                                const BLAST_SequenceBlk * subject,
                                Int4 start_offset,
                                BlastOffsetPair * NCBI_RESTRICT offset_pairs,
                                Int4 max_hits, Int4 * end_offset)
{
    BlastSmallNaLookupTable *lookup = 
                        (BlastSmallNaLookupTable *) lookup_wrap->lut;
    const Int4 kLutWordLength = 6;
    const Int4 kLutWordMask = (1 << (2 * kLutWordLength)) - 1;
    Int4 s_off = start_offset;
    Uint1 *s = subject->sequence + (s_off / COMPRESSION_RATIO);
    Int4 total_hits = 0;
    Int4 last_offset = subject->length - kLutWordLength;
    Int2 *backbone = lookup->final_backbone;
    Int2 *overflow = lookup->overflow;
    Int4 index; 
    Int4 init_index;

    max_hits -= lookup->longest_chain;
    ASSERT(lookup_wrap->lut_type == eSmallNaLookupTable);
    ASSERT(lookup->lut_word_length == 6);
    ASSERT(lookup->scan_step == 2);

    if (s_off % COMPRESSION_RATIO == 2) {
        init_index = s[0] << 8 | s[1];
        goto base_2;
    }

    while (s_off <= last_offset) {

        init_index = s[0] << 8 | s[1];
        index = backbone[init_index >> 4];
        SMALL_NA_ACCESS_HITS();
        s_off += 2;

base_2:
        if (s_off > last_offset)
            break;

        s++;
        index = backbone[init_index & kLutWordMask];
        SMALL_NA_ACCESS_HITS();
        s_off += 2;
    }

    *end_offset = s_off;
    return total_hits;
}

/** Scan the compressed subject sequence, returning 7-letter word hits
 * with stride 1. Assumes a standard nucleotide lookup table
 * @param lookup_wrap Pointer to the (wrapper to) lookup table [in]
 * @param subject The (compressed) sequence to be scanned for words [in]
 * @param start_offset The offset into the sequence in actual coordinates [in]
 * @param offset_pairs Array of query and subject positions where words are 
 *                found [out]
 * @param max_hits The allocated size of the above array - how many offsets 
 *        can be returned [in]
 * @param end_offset Where the scanning should stop [in], has stopped [out]
*/
static Int4 s_BlastSmallNaScanSubject_7_1(
                                const LookupTableWrap * lookup_wrap,
                                const BLAST_SequenceBlk * subject,
                                Int4 start_offset,
                                BlastOffsetPair * NCBI_RESTRICT offset_pairs,
                                Int4 max_hits, Int4 * end_offset)
{
    BlastSmallNaLookupTable *lookup = 
                        (BlastSmallNaLookupTable *) lookup_wrap->lut;
    const Int4 kLutWordLength = 7;
    const Int4 kLutWordMask = (1 << (2 * kLutWordLength)) - 1;
    Int4 s_off = start_offset;
    Uint1 *s = subject->sequence + (s_off / COMPRESSION_RATIO);
    Int4 total_hits = 0;
    Int4 last_offset = subject->length - kLutWordLength;
    Int2 *backbone = lookup->final_backbone;
    Int2 *overflow = lookup->overflow;
    Int4 index; 
    Int4 init_index;

    max_hits -= lookup->longest_chain;
    ASSERT(lookup_wrap->lut_type == eSmallNaLookupTable);
    ASSERT(lookup->lut_word_length == 7);
    ASSERT(lookup->scan_step == 1);

    switch (s_off % COMPRESSION_RATIO) {
    case 1:
        init_index = s[0] << 8 | s[1];
        goto base_1;
    case 2:
        init_index = s[0] << 8 | s[1];
        goto base_2;
    case 3:
        init_index = s[0] << 16 | s[1] << 8 | s[2];
        goto base_3;
    }

    while (s_off <= last_offset) {

        init_index = s[0] << 8 | s[1];
        index = backbone[init_index >> 2];
        SMALL_NA_ACCESS_HITS();
        s_off++;

base_1:
        if (s_off > last_offset)
            break;

        index = backbone[init_index & kLutWordMask];
        SMALL_NA_ACCESS_HITS();
        s_off++;

base_2:
        if (s_off > last_offset)
            break;

        init_index = init_index << 8 | s[2];
        index = backbone[(init_index >> 6) & kLutWordMask];
        SMALL_NA_ACCESS_HITS();
        s_off++;

base_3:
        if (s_off > last_offset)
            break;

        s++;
        index = backbone[(init_index >> 4) & kLutWordMask];
        SMALL_NA_ACCESS_HITS();
        s_off++;
    }

    *end_offset = s_off;
    return total_hits;
}

/** Scan the compressed subject sequence, returning 7-letter word hits
 * with stride 2. Assumes a standard nucleotide lookup table
 * @param lookup_wrap Pointer to the (wrapper to) lookup table [in]
 * @param subject The (compressed) sequence to be scanned for words [in]
 * @param start_offset The offset into the sequence in actual coordinates [in]
 * @param offset_pairs Array of query and subject positions where words are 
 *                found [out]
 * @param max_hits The allocated size of the above array - how many offsets 
 *        can be returned [in]
 * @param end_offset Where the scanning should stop [in], has stopped [out]
*/
static Int4 s_BlastSmallNaScanSubject_7_2(
                                const LookupTableWrap * lookup_wrap,
                                const BLAST_SequenceBlk * subject,
                                Int4 start_offset,
                                BlastOffsetPair * NCBI_RESTRICT offset_pairs,
                                Int4 max_hits, Int4 * end_offset)
{
    BlastSmallNaLookupTable *lookup = 
                        (BlastSmallNaLookupTable *) lookup_wrap->lut;
    const Int4 kLutWordLength = 7;
    const Int4 kLutWordMask = (1 << (2 * kLutWordLength)) - 1;
    Int4 s_off = start_offset;
    Uint1 *s = subject->sequence + (s_off / COMPRESSION_RATIO);
    Int4 total_hits = 0;
    Int4 last_offset = subject->length - kLutWordLength;
    Int2 *backbone = lookup->final_backbone;
    Int2 *overflow = lookup->overflow;
    Int4 index; 
    Int4 init_index;

    max_hits -= lookup->longest_chain;
    ASSERT(lookup_wrap->lut_type == eSmallNaLookupTable);
    ASSERT(lookup->lut_word_length == 7);
    ASSERT(lookup->scan_step == 2);

    if (s_off % COMPRESSION_RATIO == 2) {
        init_index = s[0] << 8 | s[1];
        goto base_2;
    }

    while (s_off <= last_offset) {

        init_index = s[0] << 8 | s[1];
        index = backbone[init_index >> 2];
        SMALL_NA_ACCESS_HITS();
        s_off += 2;

base_2:
        if (s_off > last_offset)
            break;

        init_index = init_index << 8 | s[2];
        s++;
        index = backbone[(init_index >> 6) & kLutWordMask];
        SMALL_NA_ACCESS_HITS();
        s_off += 2;
    }

    *end_offset = s_off;
    return total_hits;
}

/** Scan the compressed subject sequence, returning 7-letter word hits
 * with stride 3. Assumes a standard nucleotide lookup table
 * @param lookup_wrap Pointer to the (wrapper to) lookup table [in]
 * @param subject The (compressed) sequence to be scanned for words [in]
 * @param start_offset The offset into the sequence in actual coordinates [in]
 * @param offset_pairs Array of query and subject positions where words are 
 *                found [out]
 * @param max_hits The allocated size of the above array - how many offsets 
 *        can be returned [in]
 * @param end_offset Where the scanning should stop [in], has stopped [out]
*/
static Int4 s_BlastSmallNaScanSubject_7_3(
                                const LookupTableWrap * lookup_wrap,
                                const BLAST_SequenceBlk * subject,
                                Int4 start_offset,
                                BlastOffsetPair * NCBI_RESTRICT offset_pairs,
                                Int4 max_hits, Int4 * end_offset)
{
    BlastSmallNaLookupTable *lookup = 
                        (BlastSmallNaLookupTable *) lookup_wrap->lut;
    const Int4 kLutWordLength = 7;
    const Int4 kLutWordMask = (1 << (2 * kLutWordLength)) - 1;
    Int4 s_off = start_offset;
    Uint1 *s = subject->sequence + (s_off / COMPRESSION_RATIO);
    Int4 total_hits = 0;
    Int4 last_offset = subject->length - kLutWordLength;
    Int2 *backbone = lookup->final_backbone;
    Int2 *overflow = lookup->overflow;
    Int4 index; 
    Int4 init_index;

    max_hits -= lookup->longest_chain;
    ASSERT(lookup_wrap->lut_type == eSmallNaLookupTable);
    ASSERT(lookup->lut_word_length == 7);
    ASSERT(lookup->scan_step == 3);

    switch (s_off % COMPRESSION_RATIO) {
    case 1:
        init_index = s[0] << 8 | s[1];
        s -= 2;
        goto base_3;
    case 2:
        init_index = s[0] << 8 | s[1];
        s--;
        goto base_2;
    case 3:
        init_index = s[0] << 8 | s[1];
        goto base_1;
    }

    while (s_off <= last_offset) {

        init_index = s[0] << 8 | s[1];
        index = backbone[(init_index >> 2) & kLutWordMask];
        SMALL_NA_ACCESS_HITS();
        s_off += 3;

base_1:
        if (s_off > last_offset)
            break;

        init_index = init_index << 8 | s[2];
        index = backbone[(init_index >> 4) & kLutWordMask];
        SMALL_NA_ACCESS_HITS();
        s_off += 3;

base_2:
        if (s_off > last_offset)
            break;

        init_index = init_index << 8 | s[3];
        index = backbone[(init_index >> 6) & kLutWordMask];
        SMALL_NA_ACCESS_HITS();
        s_off += 3;

base_3:
        if (s_off > last_offset)
            break;

        s += 3;
        index = backbone[init_index & kLutWordMask];
        SMALL_NA_ACCESS_HITS();
        s_off += 3;
    }

    *end_offset = s_off;
    return total_hits;
}

/** Scan the compressed subject sequence, returning 8-letter word hits
 * with stride of one plus a multiple of four. Assumes a standard 
 * nucleotide lookup table
 * @param lookup_wrap Pointer to the (wrapper to) lookup table [in]
 * @param subject The (compressed) sequence to be scanned for words [in]
 * @param start_offset The offset into the sequence in actual coordinates [in]
 * @param offset_pairs Array of query and subject positions where words are 
 *                found [out]
 * @param max_hits The allocated size of the above array - how many offsets 
 *        can be returned [in]
 * @param end_offset Where the scanning should stop [in], has stopped [out]
*/
static Int4 s_BlastSmallNaScanSubject_8_1Mod4(
                                const LookupTableWrap * lookup_wrap,
                                const BLAST_SequenceBlk * subject,
                                Int4 start_offset,
                                BlastOffsetPair * NCBI_RESTRICT offset_pairs,
                                Int4 max_hits, Int4 * end_offset)
{
    BlastSmallNaLookupTable *lookup = 
                        (BlastSmallNaLookupTable *) lookup_wrap->lut;
    const Int4 kLutWordLength = 8;
    const Int4 kLutWordMask = (1 << (2 * kLutWordLength)) - 1;
    Int4 s_off = start_offset;
    Int4 scan_step = lookup->scan_step;
    Int4 scan_step_byte = scan_step / COMPRESSION_RATIO;
    Uint1 *s = subject->sequence + s_off / COMPRESSION_RATIO;
    Int4 total_hits = 0;
    Int4 last_offset = subject->length - kLutWordLength;
    Int2 *backbone = lookup->final_backbone;
    Int2 *overflow = lookup->overflow;
    Int4 index; 

    max_hits -= lookup->longest_chain;
    ASSERT(lookup_wrap->lut_type == eSmallNaLookupTable);
    ASSERT(lookup->lut_word_length == 8);
    ASSERT(lookup->scan_step % COMPRESSION_RATIO == 1);

    switch (s_off % COMPRESSION_RATIO) {
    case 1: goto base_1;
    case 2: goto base_2;
    case 3: goto base_3;
    }

    while (s_off <= last_offset) {

        index = s[0] << 8 | s[1];
        s += scan_step_byte;
        index = backbone[index];
        SMALL_NA_ACCESS_HITS();
        s_off += scan_step;

base_1:
        if (s_off > last_offset)
            break;

        index = s[0] << 16 | s[1] << 8 | s[2];
        s += scan_step_byte;
        index = backbone[(index >> 6) & kLutWordMask];
        SMALL_NA_ACCESS_HITS();
        s_off += scan_step;

base_2:
        if (s_off > last_offset)
            break;

        index = s[0] << 16 | s[1] << 8 | s[2];
        s += scan_step_byte;
        index = backbone[(index >> 4) & kLutWordMask];
        SMALL_NA_ACCESS_HITS();
        s_off += scan_step;

base_3:
        if (s_off > last_offset)
            break;

        index = s[0] << 16 | s[1] << 8 | s[2];
        s += scan_step_byte + 1;
        index = backbone[(index >> 2) & kLutWordMask];
        SMALL_NA_ACCESS_HITS();
        s_off += scan_step;
    }

    *end_offset = s_off;
    return total_hits;
}

/** Scan the compressed subject sequence, returning 8-letter word hits
 * with stride of two plus a multiple of four. Assumes a standard 
 * nucleotide lookup table
 * @param lookup_wrap Pointer to the (wrapper to) lookup table [in]
 * @param subject The (compressed) sequence to be scanned for words [in]
 * @param start_offset The offset into the sequence in actual coordinates [in]
 * @param offset_pairs Array of query and subject positions where words are 
 *                found [out]
 * @param max_hits The allocated size of the above array - how many offsets 
 *        can be returned [in]
 * @param end_offset Where the scanning should stop [in], has stopped [out]
*/
static Int4 s_BlastSmallNaScanSubject_8_2Mod4(
                                const LookupTableWrap * lookup_wrap,
                                const BLAST_SequenceBlk * subject,
                                Int4 start_offset,
                                BlastOffsetPair * NCBI_RESTRICT offset_pairs,
                                Int4 max_hits, Int4 * end_offset)
{
    BlastSmallNaLookupTable *lookup = 
                        (BlastSmallNaLookupTable *) lookup_wrap->lut;
    const Int4 kLutWordLength = 8;
    const Int4 kLutWordMask = (1 << (2 * kLutWordLength)) - 1;
    Int4 s_off = start_offset;
    Int4 scan_step = lookup->scan_step;
    Int4 scan_step_byte = scan_step / COMPRESSION_RATIO;
    Uint1 *s = subject->sequence + s_off / COMPRESSION_RATIO;
    Int4 total_hits = 0;
    Int4 last_offset = subject->length - kLutWordLength;
    Int2 *backbone = lookup->final_backbone;
    Int2 *overflow = lookup->overflow;
    Int4 index; 

    max_hits -= lookup->longest_chain;
    ASSERT(lookup_wrap->lut_type == eSmallNaLookupTable);
    ASSERT(lookup->lut_word_length == 8);
    ASSERT(lookup->scan_step % COMPRESSION_RATIO == 2);

    if (s_off % COMPRESSION_RATIO == 2)
        goto base_2;

    while (s_off <= last_offset) {

        index = s[0] << 8 | s[1];
        s += scan_step_byte;
        index = backbone[index];
        SMALL_NA_ACCESS_HITS();
        s_off += scan_step;

base_2:
        if (s_off > last_offset)
            break;

        index = s[0] << 16 | s[1] << 8 | s[2];
        s += scan_step_byte + 1;
        index = backbone[(index >> 4) & kLutWordMask];
        SMALL_NA_ACCESS_HITS();
        s_off += scan_step;
    }

    *end_offset = s_off;
    return total_hits;
}

/** Scan the compressed subject sequence, returning 8-letter word hits
 * with stride of three plus a multiple of four. Assumes a standard 
 * nucleotide lookup table
 * @param lookup_wrap Pointer to the (wrapper to) lookup table [in]
 * @param subject The (compressed) sequence to be scanned for words [in]
 * @param start_offset The offset into the sequence in actual coordinates [in]
 * @param offset_pairs Array of query and subject positions where words are 
 *                found [out]
 * @param max_hits The allocated size of the above array - how many offsets 
 *        can be returned [in]
 * @param end_offset Where the scanning should stop [in], has stopped [out]
*/
static Int4 s_BlastSmallNaScanSubject_8_3Mod4(
                                const LookupTableWrap * lookup_wrap,
                                const BLAST_SequenceBlk * subject,
                                Int4 start_offset,
                                BlastOffsetPair * NCBI_RESTRICT offset_pairs,
                                Int4 max_hits, Int4 * end_offset)
{
    BlastSmallNaLookupTable *lookup = 
                        (BlastSmallNaLookupTable *) lookup_wrap->lut;
    const Int4 kLutWordLength = 8;
    const Int4 kLutWordMask = (1 << (2 * kLutWordLength)) - 1;
    Int4 s_off = start_offset;
    Int4 scan_step = lookup->scan_step;
    Int4 scan_step_byte = scan_step / COMPRESSION_RATIO;
    Uint1 *s = subject->sequence + s_off / COMPRESSION_RATIO;
    Int4 total_hits = 0;
    Int4 last_offset = subject->length - kLutWordLength;
    Int2 *backbone = lookup->final_backbone;
    Int2 *overflow = lookup->overflow;
    Int4 index; 

    max_hits -= lookup->longest_chain;
    ASSERT(lookup_wrap->lut_type == eSmallNaLookupTable);
    ASSERT(lookup->lut_word_length == 8);
    ASSERT(lookup->scan_step % COMPRESSION_RATIO == 3);

    switch (s_off % COMPRESSION_RATIO) {
    case 1: 
        s -= 2;
        goto base_3;
    case 2: 
        s--;
        goto base_2;
    case 3: 
        goto base_1;
    }

    while (s_off <= last_offset) {

        index = s[0] << 8 | s[1];
        s += scan_step_byte;
        index = backbone[index];
        SMALL_NA_ACCESS_HITS();
        s_off += scan_step;

base_1:
        if (s_off > last_offset)
            break;

        index = s[0] << 16 | s[1] << 8 | s[2];
        s += scan_step_byte;
        index = backbone[(index >> 2) & kLutWordMask];
        SMALL_NA_ACCESS_HITS();
        s_off += scan_step;

base_2:
        if (s_off > last_offset)
            break;

        index = s[1] << 16 | s[2] << 8 | s[3];
        s += scan_step_byte;
        index = backbone[(index >> 4) & kLutWordMask];
        SMALL_NA_ACCESS_HITS();
        s_off += scan_step;

base_3:
        if (s_off > last_offset)
            break;

        index = s[2] << 16 | s[3] << 8 | s[4];
        s += scan_step_byte + 3;
        index = backbone[(index >> 6) & kLutWordMask];
        SMALL_NA_ACCESS_HITS();
        s_off += scan_step;
    }

    *end_offset = s_off;
    return total_hits;
}

void BlastSmallNaChooseScanSubject(LookupTableWrap *lookup_wrap)
{
    /* the specialized scanning routines below account for
       anything the nucleotide lookup table construction
       currently chooses. If a specialized routine is not 
       available, revert to a slower general-purpose routine.
     
       Note that when the stride is a multiple of 4, the 
       general-purpose routine is about as efficient as 
       possible, except when the stride is exactly 4; in
       that case there is a specialized routine for word
       size 8 (the most common) */

    BlastSmallNaLookupTable *lookup = 
                        (BlastSmallNaLookupTable *) lookup_wrap->lut;
    Int4 scan_step = lookup->scan_step;

    ASSERT(lookup_wrap->lut_type == eSmallNaLookupTable);

    switch (lookup->lut_word_length) {
    case 4:
        if (scan_step == 1)
            lookup->scansub_callback = s_BlastSmallNaScanSubject_4_1;
        else
            lookup->scansub_callback = s_BlastSmallNaScanSubject_Any;
        break;

    case 5:
        if (scan_step == 1)
            lookup->scansub_callback = s_BlastSmallNaScanSubject_5_1;
        else
            lookup->scansub_callback = s_BlastSmallNaScanSubject_Any;
        break;

    case 6:
        if (scan_step == 1)
            lookup->scansub_callback = s_BlastSmallNaScanSubject_6_1;
        else if (scan_step == 2)
            lookup->scansub_callback = s_BlastSmallNaScanSubject_6_2;
        else
            lookup->scansub_callback = s_BlastSmallNaScanSubject_Any;
        break;

    case 7:
        if (scan_step == 1)
            lookup->scansub_callback = s_BlastSmallNaScanSubject_7_1;
        else if (scan_step == 2)
            lookup->scansub_callback = s_BlastSmallNaScanSubject_7_2;
        else if (scan_step == 3)
            lookup->scansub_callback = s_BlastSmallNaScanSubject_7_3;
        else
            lookup->scansub_callback = s_BlastSmallNaScanSubject_Any;
        break;

    case 8:
        if (scan_step == 0 || scan_step == 4) {
            lookup->scansub_callback = s_BlastSmallNaScanSubject_8_4;
        }
        else {
            switch (scan_step % COMPRESSION_RATIO) {
            case 0:
                lookup->scansub_callback = s_BlastSmallNaScanSubject_Any;
                break;
            case 1:
                lookup->scansub_callback = s_BlastSmallNaScanSubject_8_1Mod4;
                break;
            case 2:
                lookup->scansub_callback = s_BlastSmallNaScanSubject_8_2Mod4;
                break;
            case 3:
                lookup->scansub_callback = s_BlastSmallNaScanSubject_8_3Mod4;
                break;
            }
        }
        break;
    }
}

/**
* Determine if this subject word occurs in the query.
* @param lookup The lookup table to read from. [in]
* @param index The index value of the word to retrieve. [in]
* @return 1 if there are hits, 0 otherwise.
*/
static NCBI_INLINE Int4 s_BlastMBLookupHasHits(BlastMBLookupTable * lookup,
                                               Int4 index)
{
    PV_ARRAY_TYPE *pv = lookup->pv_array;
    Int4 pv_array_bts = lookup->pv_array_bts;

    if (PV_TEST(pv, index, pv_array_bts))
        return 1;
    else
        return 0;
}

/** 
* Copy query offsets from the lookup table to the array of offset pairs.
* @param lookup The lookup table to read from. [in]
* @param index The index value of the word to retrieve. [in]
* @param offset_pairs A pointer into the destination array. [out]
* @param s_off The subject offset to be associated with the retrieved query offset(s). [in]
* @return The number of hits copied.
*/
static NCBI_INLINE Int4 s_BlastMBLookupRetrieve(BlastMBLookupTable * lookup,
                                                Int4 index,
                                                BlastOffsetPair * offset_pairs,
                                                Int4 s_off)
{
    Int4 i=0;
    Int4 q_off = lookup->hashtable[index];

    while (q_off) {
        offset_pairs[i].qs_offsets.q_off   = q_off - 1;
        offset_pairs[i++].qs_offsets.s_off = s_off;
        q_off = lookup->next_pos[q_off];
    }
    return i;
}

/** access the megablast lookup table */
#define MB_ACCESS_HITS()                                \
   if (s_BlastMBLookupHasHits(mb_lt, index)) {          \
       if (total_hits >= max_hits)                      \
           break;                                       \
       total_hits += s_BlastMBLookupRetrieve(mb_lt,     \
                  index, offset_pairs + total_hits,     \
                  s_off);                               \
   }

/** Scan the compressed subject sequence, returning 9-to-12 letter word hits
 * with arbitrary stride. Assumes a megablast lookup table
 * @param lookup_wrap Pointer to the (wrapper to) lookup table [in]
 * @param subject The (compressed) sequence to be scanned for words [in]
 * @param start_offset The offset into the sequence in actual coordinates [in]
 * @param offset_pairs Array of query and subject positions where words are 
 *                found [out]
 * @param max_hits The allocated size of the above array - how many offsets 
 *        can be returned [in]
 * @param end_offset Where the scanning should stop [in], has stopped [out]
*/
static Int4 s_MBScanSubject_Any(const LookupTableWrap* lookup_wrap,
       const BLAST_SequenceBlk* subject, Int4 start_offset,
       BlastOffsetPair* NCBI_RESTRICT offset_pairs, Int4 max_hits,  
       Int4* end_offset)
{
   BlastMBLookupTable* mb_lt = (BlastMBLookupTable*) lookup_wrap->lut;
   Uint1* s;
   Uint1* abs_start = subject->sequence;
   Int4 s_off;
   Int4 index;
   Int4 mask = mb_lt->hashsize - 1;
   Int4 total_hits = 0;
   Int4 lut_word_length = mb_lt->lut_word_length;
   Int4 last_offset = subject->length - lut_word_length;
   Int4 scan_step = mb_lt->scan_step;
   
   ASSERT(lookup_wrap->lut_type == eMBLookupTable);
   ASSERT(lut_word_length == 9 || 
          lut_word_length == 10 ||
          lut_word_length == 11 ||
          lut_word_length == 12);

   /* Since the test for number of hits here is done after adding them, 
      subtract the longest chain length from the allowed offset array size. */
   max_hits -= mb_lt->longest_chain;

   if (scan_step % COMPRESSION_RATIO == 0) {

      /* for strides that are a multiple of 4, words are
         always aligned and three bytes of the subject sequence 
         will always hold a complete word (plus possible extra bases 
         that must be shifted away). s_end below always points to
         the third-to-last byte of the subject sequence, so we will
         never fetch the byte beyond the end of subject */

      Uint1* s_end = abs_start + (subject->length - lut_word_length) /
                                                COMPRESSION_RATIO;
      Int4 shift = 2 * (12 - lut_word_length);
      s = abs_start + start_offset / COMPRESSION_RATIO;
      scan_step = scan_step / COMPRESSION_RATIO;

      for ( ; s <= s_end; s += scan_step) {
         
         index = s[0] << 16 | s[1] << 8 | s[2];
         index = index >> shift;
   
         if (s_BlastMBLookupHasHits(mb_lt, index)) {
             if (total_hits >= max_hits)
                 break;
             s_off = (s - abs_start)*COMPRESSION_RATIO;
             total_hits += s_BlastMBLookupRetrieve(mb_lt,
                 index,
                 offset_pairs + total_hits,
                 s_off);
         }
      }
      *end_offset = (s - abs_start)*COMPRESSION_RATIO;
   }
   else if (lut_word_length > 9) {

      /* when the stride is not a multiple of 4, extra bases
         may occur both before and after every word read from
         the subject sequence. The portion of each 16-base
         region that contains the actual word depends on the
         offset of the word and the lookup table width, and
         must be recalculated for each 16-base region
          
         Unlike the aligned stride case, the scanning can
         walk off the subject array for lut_word_length = 10 or 11
         (length 12 may also do this, but only if the subject is a
         multiple of 4 bases in size, and in that case there is 
         a sentinel byte). We avoid this by first handling all 
         the cases where 16-base regions fit, then handling the 
         last few offsets separately */

      Int4 last_offset4 = last_offset;
      switch (subject->length % COMPRESSION_RATIO) {
      case 2:
          if (lut_word_length == 10)
              last_offset4--;
          break;
      case 3:
          if (lut_word_length == 10)
              last_offset4 -= 2;
          else if (lut_word_length == 11)
              last_offset4--;
          break;
      default:
          break;
      }

      for (s_off = start_offset; s_off <= last_offset4; s_off += scan_step) {
   
         Int4 shift = 2*(16 - (s_off % COMPRESSION_RATIO + lut_word_length));
         s = abs_start + (s_off / COMPRESSION_RATIO);
   
         index = s[0] << 24 | s[1] << 16 | s[2] << 8 | s[3];
         index = (index >> shift) & mask;
         MB_ACCESS_HITS();
      }

      /* repeat the loop but only read three bytes at a time. For
         lut_word_length = 10 the loop runs at most twice, and for
         lut_word_length = 11 it runs at most once */

      for (; s_off > last_offset4 && s_off <= last_offset; 
                                             s_off += scan_step) {
   
         Int4 shift = 2*(12 - (s_off % COMPRESSION_RATIO + lut_word_length));
         s = abs_start + (s_off / COMPRESSION_RATIO);
   
         index = s[0] << 16 | s[1] << 8 | s[2];
         index = (index >> shift) & mask;
         MB_ACCESS_HITS();
      }
      *end_offset = s_off;
   }
   else {
      /* perform scanning for a lookup table of width 9 with
         stride not a multiple of 4. The last word is always 
         three bytes from the end of the sequence, so the 
         following does not need to be corrected when scanning 
         near the end of the subject sequence */

      for (s_off = start_offset; s_off <= last_offset; s_off += scan_step) {
   
         Int4 shift = 2*(12 - (s_off % COMPRESSION_RATIO + lut_word_length));
         s = abs_start + (s_off / COMPRESSION_RATIO);
   
         index = s[0] << 16 | s[1] << 8 | s[2];
         index = (index >> shift) & mask;
         MB_ACCESS_HITS();
      }
      *end_offset = s_off;
   }

   return total_hits;
}

/** Scan the compressed subject sequence, returning 9-letter word hits
 * with stride 1. Assumes a megablast lookup table
 * @param lookup_wrap Pointer to the (wrapper to) lookup table [in]
 * @param subject The (compressed) sequence to be scanned for words [in]
 * @param start_offset The offset into the sequence in actual coordinates [in]
 * @param offset_pairs Array of query and subject positions where words are 
 *                found [out]
 * @param max_hits The allocated size of the above array - how many offsets 
 *        can be returned [in]
 * @param end_offset Where the scanning should stop [in], has stopped [out]
*/
static Int4 s_MBScanSubject_9_1(const LookupTableWrap* lookup_wrap,
       const BLAST_SequenceBlk* subject, Int4 start_offset,
       BlastOffsetPair* NCBI_RESTRICT offset_pairs, Int4 max_hits,  
       Int4* end_offset)
{
    BlastMBLookupTable* mb_lt = (BlastMBLookupTable*) lookup_wrap->lut;
    const Int4 kLutWordLength = 9;
    const Int4 kLutWordMask = (1 << (2 * kLutWordLength)) - 1;
    Int4 index;
    Int4 init_index;
    Int4 total_hits = 0;
    Uint1* s = subject->sequence + start_offset / COMPRESSION_RATIO;
    Int4 s_off = start_offset;
    Int4 last_offset = subject->length - kLutWordLength;
    
    max_hits -= mb_lt->longest_chain;
    ASSERT(lookup_wrap->lut_type == eMBLookupTable);
    ASSERT(mb_lt->lut_word_length == 9);
    ASSERT(mb_lt->scan_step == 1);
 
    switch (s_off % COMPRESSION_RATIO) {
    case 1: 
        init_index = s[0] << 16 | s[1] << 8 | s[2];
        goto base_1;
    case 2: 
        init_index = s[0] << 16 | s[1] << 8 | s[2];
        goto base_2;
    case 3: 
        init_index = s[0] << 16 | s[1] << 8 | s[2];
        goto base_3;
    }

    while (s_off <= last_offset) {

        init_index = s[0] << 16 | s[1] << 8 | s[2];
        index = init_index >> 6;
        MB_ACCESS_HITS();
        s_off++;

base_1:
        if (s_off > last_offset)
            break;

        index = (init_index >> 4) & kLutWordMask;
        MB_ACCESS_HITS();
        s_off++;

base_2:
        if (s_off > last_offset)
            break;

        index = (init_index >> 2) & kLutWordMask;
        MB_ACCESS_HITS();
        s_off++;

base_3:
        if (s_off > last_offset)
            break;

        index = init_index & kLutWordMask;
        s++;
        MB_ACCESS_HITS();
        s_off++;
    }

   *end_offset = s_off;
   return total_hits;
}

/** Scan the compressed subject sequence, returning 9-letter word hits
 * with stride 2. Assumes a megablast lookup table
 * @param lookup_wrap Pointer to the (wrapper to) lookup table [in]
 * @param subject The (compressed) sequence to be scanned for words [in]
 * @param start_offset The offset into the sequence in actual coordinates [in]
 * @param offset_pairs Array of query and subject positions where words are 
 *                found [out]
 * @param max_hits The allocated size of the above array - how many offsets 
 *        can be returned [in]
 * @param end_offset Where the scanning should stop [in], has stopped [out]
*/
static Int4 s_MBScanSubject_9_2(const LookupTableWrap* lookup_wrap,
       const BLAST_SequenceBlk* subject, Int4 start_offset,
       BlastOffsetPair* NCBI_RESTRICT offset_pairs, Int4 max_hits,  
       Int4* end_offset)
{
    BlastMBLookupTable* mb_lt = (BlastMBLookupTable*) lookup_wrap->lut;
    const Int4 kLutWordLength = 9;
    const Int4 kLutWordMask = (1 << (2 * kLutWordLength)) - 1;
    Int4 index;
    Int4 init_index;
    Int4 total_hits = 0;
    Uint1* s = subject->sequence + start_offset / COMPRESSION_RATIO;
    Int4 s_off = start_offset;
    Int4 last_offset = subject->length - kLutWordLength;
    
    max_hits -= mb_lt->longest_chain;
    ASSERT(lookup_wrap->lut_type == eMBLookupTable);
    ASSERT(mb_lt->lut_word_length == 9);
    ASSERT(mb_lt->scan_step == 2);
 
    if (s_off % COMPRESSION_RATIO == 2) {
        init_index = s[0] << 16 | s[1] << 8 | s[2];
        goto base_2;
    }

    while (s_off <= last_offset) {

        init_index = s[0] << 16 | s[1] << 8 | s[2];
        index = init_index >> 6;
        MB_ACCESS_HITS();
        s_off += 2;

base_2:
        if (s_off > last_offset)
            break;

        index = (init_index >> 2) & kLutWordMask;
        s++;
        MB_ACCESS_HITS();
        s_off += 2;
    }

   *end_offset = s_off;
   return total_hits;
}

/** Scan the compressed subject sequence, returning 10-letter word hits
 * with stride 1. Assumes a megablast lookup table
 * @param lookup_wrap Pointer to the (wrapper to) lookup table [in]
 * @param subject The (compressed) sequence to be scanned for words [in]
 * @param start_offset The offset into the sequence in actual coordinates [in]
 * @param offset_pairs Array of query and subject positions where words are 
 *                found [out]
 * @param max_hits The allocated size of the above array - how many offsets 
 *        can be returned [in]
 * @param end_offset Where the scanning should stop [in], has stopped [out]
*/
static Int4 s_MBScanSubject_10_1(const LookupTableWrap* lookup_wrap,
       const BLAST_SequenceBlk* subject, Int4 start_offset,
       BlastOffsetPair* NCBI_RESTRICT offset_pairs, Int4 max_hits,  
       Int4* end_offset)
{
    BlastMBLookupTable* mb_lt = (BlastMBLookupTable*) lookup_wrap->lut;
    const Int4 kLutWordLength = 10;
    const Int4 kLutWordMask = (1 << (2 * kLutWordLength)) - 1;
    Int4 index;
    Int4 init_index;
    Int4 total_hits = 0;
    Uint1* s = subject->sequence + start_offset / COMPRESSION_RATIO;
    Int4 s_off = start_offset;
    Int4 last_offset = subject->length - kLutWordLength;
    
    max_hits -= mb_lt->longest_chain;
    ASSERT(lookup_wrap->lut_type == eMBLookupTable);
    ASSERT(mb_lt->lut_word_length == 10);
    ASSERT(mb_lt->scan_step == 1);
 
    switch (s_off % COMPRESSION_RATIO) {
    case 1: 
        init_index = s[0] << 16 | s[1] << 8 | s[2];
        goto base_1;
    case 2: 
        init_index = s[0] << 16 | s[1] << 8 | s[2];
        goto base_2;
    case 3: 
        init_index = s[0] << 16 | s[1] << 8 | s[2];
        goto base_3;
    }

    while (s_off <= last_offset) {

        init_index = s[0] << 16 | s[1] << 8 | s[2];
        index = init_index >> 4;
        MB_ACCESS_HITS();
        s_off++;

base_1:
        if (s_off > last_offset)
            break;

        index = (init_index >> 2) & kLutWordMask;
        MB_ACCESS_HITS();
        s_off++;

base_2:
        if (s_off > last_offset)
            break;

        index = init_index & kLutWordMask;
        MB_ACCESS_HITS();
        s_off++;

base_3:
        if (s_off > last_offset)
            break;

        init_index = init_index << 8 | s[3];
        index = (init_index >> 6) & kLutWordMask;
        s++;
        MB_ACCESS_HITS();
        s_off++;
    }

   *end_offset = s_off;
   return total_hits;
}

/** Scan the compressed subject sequence, returning 10-letter word hits
 * with stride 2. Assumes a megablast lookup table
 * @param lookup_wrap Pointer to the (wrapper to) lookup table [in]
 * @param subject The (compressed) sequence to be scanned for words [in]
 * @param start_offset The offset into the sequence in actual coordinates [in]
 * @param offset_pairs Array of query and subject positions where words are 
 *                found [out]
 * @param max_hits The allocated size of the above array - how many offsets 
 *        can be returned [in]
 * @param end_offset Where the scanning should stop [in], has stopped [out]
*/
static Int4 s_MBScanSubject_10_2(const LookupTableWrap* lookup_wrap,
       const BLAST_SequenceBlk* subject, Int4 start_offset,
       BlastOffsetPair* NCBI_RESTRICT offset_pairs, Int4 max_hits,  
       Int4* end_offset)
{
    BlastMBLookupTable* mb_lt = (BlastMBLookupTable*) lookup_wrap->lut;
    const Int4 kLutWordLength = 10;
    const Int4 kLutWordMask = (1 << (2 * kLutWordLength)) - 1;
    Int4 index;
    Int4 init_index;
    Int4 total_hits = 0;
    Uint1* s = subject->sequence + start_offset / COMPRESSION_RATIO;
    Int4 s_off = start_offset;
    Int4 last_offset = subject->length - kLutWordLength;
    
    max_hits -= mb_lt->longest_chain;
    ASSERT(lookup_wrap->lut_type == eMBLookupTable);
    ASSERT(mb_lt->lut_word_length == 10);
    ASSERT(mb_lt->scan_step == 2);
 
    if (s_off % COMPRESSION_RATIO == 2) {
        init_index = s[0] << 16 | s[1] << 8 | s[2];
        goto base_2;
    }

    while (s_off <= last_offset) {

        init_index = s[0] << 16 | s[1] << 8 | s[2];
        index = init_index >> 4;
        MB_ACCESS_HITS();
        s_off += 2;

base_2:
        if (s_off > last_offset)
            break;

        index = init_index & kLutWordMask;
        s++;
        MB_ACCESS_HITS();
        s_off += 2;
    }

   *end_offset = s_off;
   return total_hits;
}

/** Scan the compressed subject sequence, returning 10-letter word hits
 * with stride 3. Assumes a megablast lookup table
 * @param lookup_wrap Pointer to the (wrapper to) lookup table [in]
 * @param subject The (compressed) sequence to be scanned for words [in]
 * @param start_offset The offset into the sequence in actual coordinates [in]
 * @param offset_pairs Array of query and subject positions where words are 
 *                found [out]
 * @param max_hits The allocated size of the above array - how many offsets 
 *        can be returned [in]
 * @param end_offset Where the scanning should stop [in], has stopped [out]
*/
static Int4 s_MBScanSubject_10_3(const LookupTableWrap* lookup_wrap,
       const BLAST_SequenceBlk* subject, Int4 start_offset,
       BlastOffsetPair* NCBI_RESTRICT offset_pairs, Int4 max_hits,  
       Int4* end_offset)
{
    BlastMBLookupTable* mb_lt = (BlastMBLookupTable*) lookup_wrap->lut;
    const Int4 kLutWordLength = 10;
    const Int4 kLutWordMask = (1 << (2 * kLutWordLength)) - 1;
    Int4 index;
    Int4 init_index;
    Int4 total_hits = 0;
    Uint1* s = subject->sequence + start_offset / COMPRESSION_RATIO;
    Int4 s_off = start_offset;
    Int4 last_offset = subject->length - kLutWordLength;
    
    max_hits -= mb_lt->longest_chain;
    ASSERT(lookup_wrap->lut_type == eMBLookupTable);
    ASSERT(mb_lt->lut_word_length == 10);
    ASSERT(mb_lt->scan_step == 3);
 
    switch (s_off % COMPRESSION_RATIO) {
    case 1: 
       init_index = s[0] << 8 | s[1];
       s -= 2;
       goto base_3;
    case 2: 
       init_index = s[0] << 16 | s[1] << 8 | s[2];
       s--;
       goto base_2;
    case 3: 
       init_index = s[0] << 16 | s[1] << 8 | s[2];
       goto base_1;
    }

    while (s_off <= last_offset) {

        init_index = s[0] << 16 | s[1] << 8 | s[2];
        index = init_index >> 4;
        MB_ACCESS_HITS();
        s_off += 3;

base_1:
        if (s_off > last_offset)
            break;

        init_index = init_index << 8 | s[3];
        index = (init_index >> 6) & kLutWordMask;
        MB_ACCESS_HITS();
        s_off += 3;

base_2:
        if (s_off > last_offset)
            break;

        index = init_index & kLutWordMask;
        MB_ACCESS_HITS();
        s_off += 3;

base_3:
        if (s_off > last_offset)
            break;

        init_index = init_index << 8 | s[4];
        index = (init_index >> 2) & kLutWordMask;
        s += 3;
        MB_ACCESS_HITS();
        s_off += 3;
    }

   *end_offset = s_off;
   return total_hits;
}

/** Scan the compressed subject sequence, returning 11-letter word hits
 * with stride one plus a multiple of four. Assumes a megablast lookup table
 * @param lookup_wrap Pointer to the (wrapper to) lookup table [in]
 * @param subject The (compressed) sequence to be scanned for words [in]
 * @param start_offset The offset into the sequence in actual coordinates [in]
 * @param offset_pairs Array of query and subject positions where words are 
 *                found [out]
 * @param max_hits The allocated size of the above array - how many offsets 
 *        can be returned [in]
 * @param end_offset Where the scanning should stop [in], has stopped [out]
*/
static Int4 s_MBScanSubject_11_1Mod4(const LookupTableWrap* lookup_wrap,
       const BLAST_SequenceBlk* subject, Int4 start_offset,
       BlastOffsetPair* NCBI_RESTRICT offset_pairs, Int4 max_hits,  
       Int4* end_offset)
{
    BlastMBLookupTable* mb_lt = (BlastMBLookupTable*) lookup_wrap->lut;
    const Int4 kLutWordLength = 11;
    const Int4 kLutWordMask = (1 << (2 * kLutWordLength)) - 1;
    Int4 index;
    Int4 total_hits = 0;
    Uint1* s = subject->sequence + start_offset / COMPRESSION_RATIO;
    Int4 s_off = start_offset;
    Int4 last_offset = subject->length - kLutWordLength;
    Int4 scan_step = mb_lt->scan_step;
    Int4 scan_step_byte = scan_step / COMPRESSION_RATIO;
    
    max_hits -= mb_lt->longest_chain;
    ASSERT(lookup_wrap->lut_type == eMBLookupTable);
    ASSERT(mb_lt->lut_word_length == 11);
    ASSERT(scan_step % COMPRESSION_RATIO == 1);
 
    switch (s_off % COMPRESSION_RATIO) {
    case 1: goto base_1;
    case 2: goto base_2;
    case 3: goto base_3;
    }

    while (s_off <= last_offset) {

        index = s[0] << 16 | s[1] << 8 | s[2];
        index = index >> 2;
        s += scan_step_byte;
        MB_ACCESS_HITS();
        s_off += scan_step;

base_1:
        if (s_off > last_offset)
            break;

        index = s[0] << 16 | s[1] << 8 | s[2];
        index = index & kLutWordMask;
        s += scan_step_byte;
        MB_ACCESS_HITS();
        s_off += scan_step;

base_2:
        if (s_off > last_offset)
            break;

        index = s[0] << 24 | s[1] << 16 | s[2] << 8 | s[3];
        index = (index >> 6) & kLutWordMask;
        s += scan_step_byte;
        MB_ACCESS_HITS();
        s_off += scan_step;

base_3:
        if (s_off > last_offset)
            break;

        index = s[0] << 24 | s[1] << 16 | s[2] << 8 | s[3];
        index = (index >> 4) & kLutWordMask;
        s += scan_step_byte + 1;
        MB_ACCESS_HITS();
        s_off += scan_step;
    }

   *end_offset = s_off;
   return total_hits;
}

/** Scan the compressed subject sequence, returning 11-letter word hits
 * with stride two plus a multiple of four. Assumes a megablast lookup table
 * @param lookup_wrap Pointer to the (wrapper to) lookup table [in]
 * @param subject The (compressed) sequence to be scanned for words [in]
 * @param start_offset The offset into the sequence in actual coordinates [in]
 * @param offset_pairs Array of query and subject positions where words are 
 *                found [out]
 * @param max_hits The allocated size of the above array - how many offsets 
 *        can be returned [in]
 * @param end_offset Where the scanning should stop [in], has stopped [out]
*/
static Int4 s_MBScanSubject_11_2Mod4(const LookupTableWrap* lookup_wrap,
       const BLAST_SequenceBlk* subject, Int4 start_offset,
       BlastOffsetPair* NCBI_RESTRICT offset_pairs, Int4 max_hits,  
       Int4* end_offset)
{
    BlastMBLookupTable* mb_lt = (BlastMBLookupTable*) lookup_wrap->lut;
    const Int4 kLutWordLength = 11;
    const Int4 kLutWordMask = (1 << (2 * kLutWordLength)) - 1;
    Int4 index;
    Int4 total_hits = 0;
    Uint1* s = subject->sequence + start_offset / COMPRESSION_RATIO;
    Int4 s_off = start_offset;
    Int4 last_offset = subject->length - kLutWordLength;
    Int4 scan_step = mb_lt->scan_step;
    Int4 scan_step_byte = scan_step / COMPRESSION_RATIO;
    
    max_hits -= mb_lt->longest_chain;
    ASSERT(lookup_wrap->lut_type == eMBLookupTable);
    ASSERT(mb_lt->lut_word_length == 11);
    ASSERT(scan_step % COMPRESSION_RATIO == 2);
 
    if (s_off % COMPRESSION_RATIO == 2)
        goto base_2;

    while (s_off <= last_offset) {

        index = s[0] << 16 | s[1] << 8 | s[2];
        index = index >> 2;
        s += scan_step_byte;
        MB_ACCESS_HITS();
        s_off += scan_step;

base_2:
        if (s_off > last_offset)
            break;

        index = s[0] << 24 | s[1] << 16 | s[2] << 8 | s[3];
        index = (index >> 6) & kLutWordMask;
        s += scan_step_byte + 1;
        MB_ACCESS_HITS();
        s_off += scan_step;
    }

   *end_offset = s_off;
   return total_hits;
}

/** Scan the compressed subject sequence, returning 11-letter word hits
 * with stride three plus a multiple of four. Assumes a megablast lookup table
 * @param lookup_wrap Pointer to the (wrapper to) lookup table [in]
 * @param subject The (compressed) sequence to be scanned for words [in]
 * @param start_offset The offset into the sequence in actual coordinates [in]
 * @param offset_pairs Array of query and subject positions where words are 
 *                found [out]
 * @param max_hits The allocated size of the above array - how many offsets 
 *        can be returned [in]
 * @param end_offset Where the scanning should stop [in], has stopped [out]
*/
static Int4 s_MBScanSubject_11_3Mod4(const LookupTableWrap* lookup_wrap,
       const BLAST_SequenceBlk* subject, Int4 start_offset,
       BlastOffsetPair* NCBI_RESTRICT offset_pairs, Int4 max_hits,  
       Int4* end_offset)
{
    BlastMBLookupTable* mb_lt = (BlastMBLookupTable*) lookup_wrap->lut;
    const Int4 kLutWordLength = 11;
    const Int4 kLutWordMask = (1 << (2 * kLutWordLength)) - 1;
    Int4 index;
    Int4 total_hits = 0;
    Uint1* s = subject->sequence + start_offset / COMPRESSION_RATIO;
    Int4 s_off = start_offset;
    Int4 last_offset = subject->length - kLutWordLength;
    Int4 scan_step = mb_lt->scan_step;
    Int4 scan_step_byte = scan_step / COMPRESSION_RATIO;
    
    max_hits -= mb_lt->longest_chain;
    ASSERT(lookup_wrap->lut_type == eMBLookupTable);
    ASSERT(mb_lt->lut_word_length == 11);
    ASSERT(scan_step % COMPRESSION_RATIO == 3);
 
    switch (s_off % COMPRESSION_RATIO) {
    case 1: 
       s -= 2;
       goto base_3;
    case 2: 
       s--;
       goto base_2;
    case 3: 
       goto base_1;
    }

    while (s_off <= last_offset) {

        index = s[0] << 16 | s[1] << 8 | s[2];
        index = index >> 2;
        s += scan_step_byte;
        MB_ACCESS_HITS();
        s_off += scan_step;

base_1:
        if (s_off > last_offset)
            break;

        index = s[0] << 24 | s[1] << 16 | s[2] << 8 | s[3];
        index = (index >> 4) & kLutWordMask;
        s += scan_step_byte;
        MB_ACCESS_HITS();
        s_off += scan_step;

base_2:
        if (s_off > last_offset)
            break;

        index = s[1] << 24 | s[2] << 16 | s[3] << 8 | s[4];
        index = (index >> 6) & kLutWordMask;
        s += scan_step_byte;
        MB_ACCESS_HITS();
        s_off += scan_step;

base_3:
        if (s_off > last_offset)
            break;

        index = s[2] << 16 | s[3] << 8 | s[4];
        index = index & kLutWordMask;
        s += scan_step_byte + 3;
        MB_ACCESS_HITS();
        s_off += scan_step;
    }

   *end_offset = s_off;
   return total_hits;
}

/** Scan the compressed subject sequence, returning 11- or 12-letter 
 * discontiguous words with stride 1 or stride 4. Assumes a megablast 
 * lookup table
 * @param lookup_wrap Pointer to the (wrapper to) lookup table [in]
 * @param subject The (compressed) sequence to be scanned for words [in]
 * @param start_offset The offset into the sequence in actual coordinates [in]
 * @param offset_pairs Array of query and subject positions where words are 
 *                found [out]
 * @param max_hits The allocated size of the above array - how many offsets 
 *        can be returned [in]
 * @param end_offset Where the scanning should stop [in], has stopped [out]
*/
static Int4 s_MB_DiscWordScanSubject_Any(const LookupTableWrap* lookup, 
       const BLAST_SequenceBlk* subject, Int4 start_offset,
       BlastOffsetPair* NCBI_RESTRICT offset_pairs, Int4 max_hits, 
       Int4* end_offset)
{
   BlastMBLookupTable* mb_lt = (BlastMBLookupTable*) lookup->lut;
   Uint1* s = subject->sequence + start_offset / COMPRESSION_RATIO;
   Int4 total_hits = 0;
   Uint4 q_off, s_off;
   Int4 index, index2=0;
   Uint8 accum;
   Boolean two_templates = mb_lt->two_templates;
   EDiscTemplateType template_type = mb_lt->template_type;
   EDiscTemplateType second_template_type = mb_lt->second_template_type;
   PV_ARRAY_TYPE *pv_array = mb_lt->pv_array;
   Uint1 pv_array_bts = mb_lt->pv_array_bts;
   Uint4 template_length = mb_lt->template_length;
   Uint4 curr_base = COMPRESSION_RATIO - (start_offset % COMPRESSION_RATIO);
   Uint4 last_base = *end_offset;

   ASSERT(lookup->lut_type == eMBLookupTable);

   /* Since the test for number of hits here is done after adding them, 
      subtract the longest chain length from the allowed offset array size. */
   max_hits -= mb_lt->longest_chain;

   accum = (Uint8)(*s++);

   if (mb_lt->full_byte_scan) {

      /* The accumulator is filled with the first (template_length -
         COMPRESSION_RATIO) bases to scan, 's' points to the first byte
         of 'sequence' that contains bases not in the accumulator,
         and curr_base is the offset of the next base to add */

      while (curr_base < template_length - COMPRESSION_RATIO) {
         accum = accum << 8 | *s++;
         curr_base += COMPRESSION_RATIO;
      }
      if (curr_base > template_length - COMPRESSION_RATIO) {
         accum = accum >> (2 * (curr_base - 
                    (template_length - COMPRESSION_RATIO)));
         s--;
      }
      curr_base = start_offset + template_length;

      while (curr_base <= last_base) {
          
         /* add the next 4 bases to the accumulator. These are
            shifted into the low-order 8 bits. curr_base was already
            incremented, but that doesn't change how the next bases
            are shifted in */

         accum = (accum << 8);
         switch (curr_base % COMPRESSION_RATIO) {
         case 0: accum |= s[0]; break;
         case 1: accum |= (s[0] << 2 | s[1] >> 6); break;
         case 2: accum |= (s[0] << 4 | s[1] >> 4); break;
         case 3: accum |= (s[0] << 6 | s[1] >> 2); break;
         }
         s++;

         index = ComputeDiscontiguousIndex(accum, template_type);
  
         if (PV_TEST(pv_array, index, pv_array_bts)) {
            q_off = mb_lt->hashtable[index];
            s_off = curr_base - template_length;
            while (q_off) {
               offset_pairs[total_hits].qs_offsets.q_off = q_off - 1;
               offset_pairs[total_hits++].qs_offsets.s_off = s_off;
               q_off = mb_lt->next_pos[q_off];
            }
         }
         if (two_templates) {
            index2 = ComputeDiscontiguousIndex(accum, second_template_type);

            if (PV_TEST(pv_array, index2, pv_array_bts)) {
               q_off = mb_lt->hashtable2[index2];
               s_off = curr_base - template_length;
               while (q_off) {
                  offset_pairs[total_hits].qs_offsets.q_off = q_off - 1;
                  offset_pairs[total_hits++].qs_offsets.s_off = s_off;
                  q_off = mb_lt->next_pos2[q_off];
               }
            }
         }

         curr_base += COMPRESSION_RATIO;

         /* test for buffer full. This test counts 
            for both templates (they both add hits 
            or neither one does) */
         if (total_hits >= max_hits)
            break;

      }
   } else {
      const Int4 kScanStep = 1; /* scan one letter at a time. */
      const Int4 kScanShift = 2; /* scan 2 bits (i.e. one base) at a time*/

      /* The accumulator is filled with the first (template_length -
         kScanStep) bases to scan, 's' points to the first byte
         of 'sequence' that contains bases not in the accumulator,
         and curr_base is the offset of the next base to add */

      while (curr_base < template_length - kScanStep) {
         accum = accum << 8 | *s++;
         curr_base += COMPRESSION_RATIO;
      }
      if (curr_base > template_length - kScanStep) {
         accum = accum >> (2 * (curr_base - (template_length - kScanStep)));
         s--;
      }
      curr_base = start_offset + template_length;

      while (curr_base <= last_base) {

         /* shift the next base into the low-order bits of the accumulator */
         accum = (accum << kScanShift) | 
                 NCBI2NA_UNPACK_BASE(s[0], 
                         3 - (curr_base - kScanStep) % COMPRESSION_RATIO);
         if (curr_base % COMPRESSION_RATIO == 0)
            s++;

         index = ComputeDiscontiguousIndex(accum, template_type);
       
         if (PV_TEST(pv_array, index, pv_array_bts)) {
            q_off = mb_lt->hashtable[index];
            s_off = curr_base - template_length;
            while (q_off) {
               offset_pairs[total_hits].qs_offsets.q_off = q_off - 1;
               offset_pairs[total_hits++].qs_offsets.s_off = s_off;
               q_off = mb_lt->next_pos[q_off];
            }
         }
         if (two_templates) {
            index2 = ComputeDiscontiguousIndex(accum, second_template_type);

            if (PV_TEST(pv_array, index2, pv_array_bts)) {
               q_off = mb_lt->hashtable2[index2];
               s_off = curr_base - template_length;
               while (q_off) {
                  offset_pairs[total_hits].qs_offsets.q_off = q_off - 1;
                  offset_pairs[total_hits++].qs_offsets.s_off = s_off;
                  q_off = mb_lt->next_pos2[q_off];
               }
            }
         }

         curr_base += kScanStep;

         /* test for buffer full. This test counts 
            for both templates (they both add hits 
            or neither one does) */
         if (total_hits >= max_hits)
            break;
      }
   }
   *end_offset = curr_base - template_length;

   return total_hits;
}

/** Scan the compressed subject sequence, returning 11- or 12-letter 
 * discontiguous words with stride 1. Assumes a megablast lookup table
 * @param lookup_wrap Pointer to the (wrapper to) lookup table [in]
 * @param subject The (compressed) sequence to be scanned for words [in]
 * @param start_offset The offset into the sequence in actual coordinates [in]
 * @param offset_pairs Array of query and subject positions where words are 
 *                found [out]
 * @param max_hits The allocated size of the above array - how many offsets 
 *        can be returned [in]
 * @param end_offset Where the scanning should stop [in], has stopped [out]
*/
static Int4 s_MB_DiscWordScanSubject_1(const LookupTableWrap* lookup, 
       const BLAST_SequenceBlk* subject, Int4 start_offset,
       BlastOffsetPair* NCBI_RESTRICT offset_pairs, Int4 max_hits, 
       Int4* end_offset)
{
   BlastMBLookupTable* mb_lt = (BlastMBLookupTable*) lookup->lut;
   Uint1* s = subject->sequence + start_offset / COMPRESSION_RATIO;
   Int4 total_hits = 0;
   Uint4 s_off = start_offset;
   Int4 index;
   Uint8 accum = 0;
   EDiscTemplateType template_type = mb_lt->template_type;
   Uint4 template_length = mb_lt->template_length;
   Uint4 last_offset = *end_offset - template_length;

   ASSERT(lookup->lut_type == eMBLookupTable);
   max_hits -= mb_lt->longest_chain;

   /* fill the accumulator */
   index = s_off - (s_off % COMPRESSION_RATIO);
   while(index < s_off + template_length) {
      accum = accum << 8 | *s++;
      index += COMPRESSION_RATIO;
   }

   /* note that the part of the loop we jump to will
      depend on the number of extra bases (0-3) in the 
      accumulator, and not on the value of s_off */
   switch (index - (s_off + template_length)) {
   case 1: 
       goto base_3;
   case 2: 
       goto base_2;
   case 3: 
       /* this branch of the main loop adds another
          byte from s[], which is not needed in the
          initial value of the accumulator */
       accum = accum >> 8;
       s--;
       goto base_1;
   }

   while (s_off <= last_offset) {

      index = ComputeDiscontiguousIndex(accum, template_type);
      MB_ACCESS_HITS();
      s_off++;

base_1:
      if (s_off > last_offset)
         break;

      accum = accum << 8 | *s++;
      index = ComputeDiscontiguousIndex(accum >> 6, template_type);
      MB_ACCESS_HITS();
      s_off++;

base_2:
      if (s_off > last_offset)
         break;

      index = ComputeDiscontiguousIndex(accum >> 4, template_type);
      MB_ACCESS_HITS();
      s_off++;

base_3:
      if (s_off > last_offset)
         break;

      index = ComputeDiscontiguousIndex(accum >> 2, template_type);
      MB_ACCESS_HITS();
      s_off++;
   }

   *end_offset = s_off;
   return total_hits;
}

/** Scan the compressed subject sequence, returning discontiguous
 * words corresponding to the 11-of-18 coding template and stride 1.
 * Assumes a megablast lookup table
 * @param lookup_wrap Pointer to the (wrapper to) lookup table [in]
 * @param subject The (compressed) sequence to be scanned for words [in]
 * @param start_offset The offset into the sequence in actual coordinates [in]
 * @param offset_pairs Array of query and subject positions where words are 
 *                found [out]
 * @param max_hits The allocated size of the above array - how many offsets 
 *        can be returned [in]
 * @param end_offset Where the scanning should stop [in], has stopped [out]
*/
static Int4 s_MB_DiscWordScanSubject_11_18_1(
       const LookupTableWrap* lookup, 
       const BLAST_SequenceBlk* subject, Int4 start_offset,
       BlastOffsetPair* NCBI_RESTRICT offset_pairs, Int4 max_hits, 
       Int4* end_offset)
{
   BlastMBLookupTable* mb_lt = (BlastMBLookupTable*) lookup->lut;
   Uint1* s = subject->sequence + start_offset / COMPRESSION_RATIO;
   Int4 total_hits = 0;
   Uint4 s_off = start_offset;
   Int4 index;
   const Uint4 kTemplateLength = 18;
   Uint4 last_offset = *end_offset - kTemplateLength;
   Uint4 lo = 0; 
   Uint4 hi = 0;

   ASSERT(lookup->lut_type == eMBLookupTable);
   ASSERT(mb_lt->lut_word_length == 11);
   ASSERT(mb_lt->template_length == 18);
   ASSERT(mb_lt->template_type == eDiscTemplate_11_18_Coding);
   max_hits -= mb_lt->longest_chain;

   /* fill the accumulator */
   index = s_off - (s_off % COMPRESSION_RATIO);
   while(index < s_off + kTemplateLength) {
      hi = (hi << 8) | (lo >> 24);
      lo = lo << 8 | *s++;
      index += COMPRESSION_RATIO;
   }

   switch (index - (s_off + kTemplateLength)) {
   case 1: 
       goto base_3;
   case 2: 
       goto base_2;
   case 3: 
       s--;
       lo = (lo >> 8) | (hi << 24);
       hi = hi >> 8;
       goto base_1;
   }

   while (s_off <= last_offset) {

      index = ((lo & 0x00000003)      ) |
              ((lo & 0x000000f0) >>  2) |
              ((lo & 0x00003c00) >>  4) |
              ((lo & 0x00030000) >>  6) |
              ((lo & 0x03c00000) >> 10) |
              ((lo & 0xf0000000) >> 12) |
              ((hi & 0x0000000c) << 18);
      MB_ACCESS_HITS();
      s_off++;

base_1:
      if (s_off > last_offset)
         break;

      hi = (hi << 8) | (lo >> 24);
      lo = lo << 8 | *s++;

      index = ((lo & 0x000000c0) >>  6) |
              ((lo & 0x00003c00) >>  8) |
              ((lo & 0x000f0000) >> 10) |
              ((lo & 0x00c00000) >> 12) |
              ((lo & 0xf0000000) >> 16) |
              ((hi & 0x0000003c) << 14) |
              ((hi & 0x00000300) << 12);
      MB_ACCESS_HITS();
      s_off++;

base_2:
      if (s_off > last_offset)
         break;

      index = ((lo & 0x00000030) >>  4) |
              ((lo & 0x00000f00) >>  6) |
              ((lo & 0x0003c000) >>  8) |
              ((lo & 0x00300000) >> 10) |
              ((lo & 0x3c000000) >> 14) |
              ((hi & 0x0000000f) << 16) |
              ((hi & 0x000000c0) << 14);
      MB_ACCESS_HITS();
      s_off++;

base_3:
      if (s_off > last_offset)
         break;

      index = ((lo & 0x0000000c) >>  2) |
              ((lo & 0x000003c0) >>  4) |
              ((lo & 0x0000f000) >>  6) |
              ((lo & 0x000c0000) >>  8) |
              ((lo & 0x0f000000) >> 12) |
              ((lo & 0xc0000000) >> 14) |
              ((hi & 0x00000003) << 18) |
              ((hi & 0x00000030) << 16);
      MB_ACCESS_HITS();
      s_off++;
   }

   *end_offset = s_off;
   return total_hits;
}

/** Scan the compressed subject sequence, returning discontiguous
 * words corresponding to the 11-of-21 coding template and stride 1.
 * Assumes a megablast lookup table
 * @param lookup_wrap Pointer to the (wrapper to) lookup table [in]
 * @param subject The (compressed) sequence to be scanned for words [in]
 * @param start_offset The offset into the sequence in actual coordinates [in]
 * @param offset_pairs Array of query and subject positions where words are 
 *                found [out]
 * @param max_hits The allocated size of the above array - how many offsets 
 *        can be returned [in]
 * @param end_offset Where the scanning should stop [in], has stopped [out]
*/
static Int4 s_MB_DiscWordScanSubject_11_21_1(
       const LookupTableWrap* lookup, 
       const BLAST_SequenceBlk* subject, Int4 start_offset,
       BlastOffsetPair* NCBI_RESTRICT offset_pairs, Int4 max_hits, 
       Int4* end_offset)
{
   BlastMBLookupTable* mb_lt = (BlastMBLookupTable*) lookup->lut;
   Uint1* s = subject->sequence + start_offset / COMPRESSION_RATIO;
   Int4 total_hits = 0;
   Uint4 s_off = start_offset;
   Int4 index;
   const Uint4 kTemplateLength = 21;
   Uint4 last_offset = *end_offset - kTemplateLength;
   Uint4 lo = 0; 
   Uint4 hi = 0;

   ASSERT(lookup->lut_type == eMBLookupTable);
   ASSERT(mb_lt->lut_word_length == 11);
   ASSERT(mb_lt->template_length == 21);
   ASSERT(mb_lt->template_type == eDiscTemplate_11_21_Coding);
   max_hits -= mb_lt->longest_chain;

   index = s_off - (s_off % COMPRESSION_RATIO);
   while(index < s_off + kTemplateLength) {
      hi = (hi << 8) | (lo >> 24);
      lo = lo << 8 | *s++;
      index += COMPRESSION_RATIO;
   }

   switch (index - (s_off + kTemplateLength)) {
   case 1: 
       goto base_3;
   case 2: 
       goto base_2;
   case 3: 
       s--;
       lo = (lo >> 8) | (hi << 24);
       hi = hi >> 8;
       goto base_1;
   }

   while (s_off <= last_offset) {

      index = ((lo & 0x00000003)      ) |
              ((lo & 0x000000f0) >>  2) |
              ((lo & 0x00000c00) >>  4) |
              ((lo & 0x000f0000) >>  8) |
              ((lo & 0x00c00000) >> 10) |
              ((lo & 0xf0000000) >> 14) |
              ((hi & 0x0000000c) << 16) |
              ((hi & 0x00000300) << 12);
      MB_ACCESS_HITS();
      s_off++;

base_1:
      if (s_off > last_offset)
         break;

      hi = (hi << 8) | (lo >> 24);
      lo = lo << 8 | *s++;

      index = ((lo & 0x000000c0) >>  6) |
              ((lo & 0x00003c00) >>  8) |
              ((lo & 0x00030000) >> 10) |
              ((lo & 0x03c00000) >> 14) |
              ((lo & 0x30000000) >> 16) |
              ((hi & 0x0000003c) << 12) |
              ((hi & 0x00000300) << 10) |
              ((hi & 0x0000c000) <<  6);
      MB_ACCESS_HITS();
      s_off++;

base_2:
      if (s_off > last_offset)
         break;

      index = ((lo & 0x00000030) >>  4) |
              ((lo & 0x00000f00) >>  6) |
              ((lo & 0x0000c000) >>  8) |
              ((lo & 0x00f00000) >> 12) |
              ((lo & 0x0c000000) >> 14) |
              ((hi & 0x0000000f) << 14) |
              ((hi & 0x000000c0) << 12) |
              ((hi & 0x00003000) <<  8);
      MB_ACCESS_HITS();
      s_off++;

base_3:
      if (s_off > last_offset)
         break;

      index = ((lo & 0x0000000c) >>  2) |
              ((lo & 0x000003c0) >>  4) |
              ((lo & 0x00003000) >>  6) |
              ((lo & 0x003c0000) >> 10) |
              ((lo & 0x03000000) >> 12) |
              ((lo & 0xc0000000) >> 16) |
              ((hi & 0x00000003) << 16) |
              ((hi & 0x00000030) << 14) |
              ((hi & 0x00000c00) << 10);
      MB_ACCESS_HITS();
      s_off++;
   }

   *end_offset = s_off;
   return total_hits;
}

void BlastMBChooseScanSubject(LookupTableWrap *lookup_wrap)
{
    /* the specialized scanning routines below account for
       anything the nucleotide lookup table construction
       currently chooses. If a specialized routine is not 
       available, revert to a slower general-purpose routine.
     
       Note that when the stride is a multiple of 4, the 
       general-purpose routine is about as efficient as 
       possible */

    BlastMBLookupTable* mb_lt = (BlastMBLookupTable*) lookup_wrap->lut;

    ASSERT(lookup_wrap->lut_type == eMBLookupTable);

    if (mb_lt->discontiguous) {
        if (!mb_lt->full_byte_scan && !mb_lt->two_templates) {
            if (mb_lt->template_type == eDiscTemplate_11_18_Coding)
                mb_lt->scansub_callback = s_MB_DiscWordScanSubject_11_18_1;
            else if (mb_lt->template_type == eDiscTemplate_11_21_Coding)
                mb_lt->scansub_callback = s_MB_DiscWordScanSubject_11_21_1;
            else
                mb_lt->scansub_callback = s_MB_DiscWordScanSubject_1;
        }
        else {
            mb_lt->scansub_callback = s_MB_DiscWordScanSubject_Any;
        }
    }
    else {
        Int4 scan_step = mb_lt->scan_step;

        switch (mb_lt->lut_word_length) {
        case 9:
            if (scan_step == 1)
                mb_lt->scansub_callback = s_MBScanSubject_9_1;
            if (scan_step == 2)
                mb_lt->scansub_callback = s_MBScanSubject_9_2;
            else
                mb_lt->scansub_callback = s_MBScanSubject_Any;
            break;

        case 10:
            if (scan_step == 1)
                mb_lt->scansub_callback = s_MBScanSubject_10_1;
            else if (scan_step == 2)
                mb_lt->scansub_callback = s_MBScanSubject_10_2;
            else if (scan_step == 3)
                mb_lt->scansub_callback = s_MBScanSubject_10_3;
            else
                mb_lt->scansub_callback = s_MBScanSubject_Any;
            break;
    
        case 11:
            switch (scan_step % COMPRESSION_RATIO) {
            case 0:
                mb_lt->scansub_callback = s_MBScanSubject_Any;
                break;
            case 1:
                mb_lt->scansub_callback = s_MBScanSubject_11_1Mod4;
                break;
            case 2:
                mb_lt->scansub_callback = s_MBScanSubject_11_2Mod4;
                break;
            case 3:
                mb_lt->scansub_callback = s_MBScanSubject_11_3Mod4;
                break;
            }
            break;

        case 12:
            /* lookup tables of width 12 are only used
               for very large queries, and the latency of
               cache misses dominates the runtime in that
               case. Thus the extra arithmetic in the generic
               routine isn't performance-critical */
            mb_lt->scansub_callback = s_MBScanSubject_Any;
            break;
        }
    }
}
