/* $Id$
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
 * ===========================================================================
 *
 * Author: Ilya Dondoshansky
 *
 */

/** @file blast_filter.c
 * All code related to query sequence masking/filtering for BLAST
 */

#ifndef SKIP_DOXYGEN_PROCESSING
static char const rcsid[] = 
    "$Id$";
#endif /* SKIP_DOXYGEN_PROCESSING */

#include <algo/blast/core/blast_def.h>
#include <algo/blast/core/blast_util.h>
#include <algo/blast/core/blast_filter.h>
#include <algo/blast/core/blast_dust.h>
#include <algo/blast/core/blast_seg.h>
#include "blast_inline.h"
#ifdef CC_FILTER_ALLOWED
#include <algo/blast/core/urkpcc.h>
#endif

/****************************************************************************/
/* Constants */
const Uint1 kNuclMask = 14;     /* N in BLASTNA */
const Uint1 kProtMask = 21;     /* X in NCBISTDAA */

BlastSeqLoc* BlastSeqLocNew(BlastSeqLoc** head, Int4 from, Int4 to)
{
   BlastSeqLoc* loc = (BlastSeqLoc*) calloc(1, sizeof(BlastSeqLoc));
   SSeqRange* seq_range = (SSeqRange*) malloc(sizeof(SSeqRange));

   seq_range->left = from;
   seq_range->right = to;
   loc->ssr = seq_range;

   if (head)
   {
       if (*head)
       {
          BlastSeqLoc* tmp = *head;
          while (tmp->next)
             tmp = tmp->next;
          tmp->next = loc;
       }
       else
       {
          *head = loc;
       }
   }
       
   return loc;
}

BlastSeqLoc* BlastSeqLocFree(BlastSeqLoc* loc)
{
   SSeqRange* seq_range;
   BlastSeqLoc* next_loc;

   while (loc) {
      next_loc = loc->next;
      seq_range = loc->ssr;
      sfree(seq_range);
      sfree(loc);
      loc = next_loc;
   }
   return NULL;
}

/** Makes a copy of the BlastSeqLoc and also a copy of the 
 * SSRange element.  Does not copy BlastSeqLoc that is pointed
 * to by "next".
 * @param from the object to be copied [in]
 * @return another BlastSeqLoc*
 */

static BlastSeqLoc* s_BlastSeqLocDup(BlastSeqLoc* from)
{
    BlastSeqLoc* to;
    SSeqRange* seq_range;

    if (from == NULL)
       return NULL;

    seq_range = from->ssr;
    ASSERT(seq_range);

    to = BlastSeqLocNew(NULL, seq_range->left, seq_range->right);

    return to;
}

BlastMaskLoc* BlastMaskLocNew(Int4 total)
{
      BlastMaskLoc* retval = (BlastMaskLoc *) calloc(1, sizeof(BlastMaskLoc));
      retval->total_size = total;
      if (total > 0)
          retval->seqloc_array = (BlastSeqLoc **) calloc(total, sizeof(BlastSeqLoc *));
      return retval;
}

BlastMaskLoc* BlastMaskLocFree(BlastMaskLoc* mask_loc)
{
   Int4 index;

   if (mask_loc == NULL)
      return NULL;

   for (index=0; index<mask_loc->total_size; index++)
   {
      if (mask_loc->seqloc_array != NULL)
         BlastSeqLocFree(mask_loc->seqloc_array[index]);
   }
   sfree(mask_loc->seqloc_array);
   sfree(mask_loc);
   return NULL;
}

/** Calculates length of the DNA query from the BlastQueryInfo structure that 
 * contains context information for translated frames for a set of queries.
 * @param Query information containing data for all contexts [in]
 * @param query_index Which query to find DNA length for?
 * @return DNA length of the query, calculated as sum of 3 protein frame lengths, 
 *         plus 2, because 2 last nucleotide residues do not have a 
 *         corresponding codon.
 */
static Int4 
s_GetTranslatedQueryDNALength(const BlastQueryInfo* query_info, Int4 query_index)
{
    Int4 start_context = NUM_FRAMES*query_index;
    Int4 dna_length = 2;
    Int4 index;
 
    /* Make sure that query index is within appropriate range, and that this is
       really a translated search */
    ASSERT(query_index < query_info->num_queries);
    ASSERT(start_context < query_info->last_context);

    /* If only reverse strand is searched, then forward strand contexts don't 
       have lengths information */
    if (query_info->contexts[start_context].query_length == 0)
        start_context += 3;

    for (index = start_context; index < start_context + 3; ++index)
        dna_length += query_info->contexts[index].query_length;
 
    return dna_length;
}

Int2 BlastMaskLocDNAToProtein(BlastMaskLoc* mask_loc, 
                              const BlastQueryInfo* query_info)
{
    BlastSeqLoc** prot_seqloc_array;
    Uint4 seq_index;

    if (!mask_loc)
        return 0;

    /* Check that the number of sequences in BlastQueryInfo is the same as the
       size of the DNA mask locations array in the BlastMaskLoc. */
    ASSERT(mask_loc->total_size == query_info->num_queries);

    mask_loc->total_size *= NUM_FRAMES;
    prot_seqloc_array = 
        (BlastSeqLoc**) calloc(mask_loc->total_size, sizeof(BlastSeqLoc*));

    /* Loop over multiple DNA sequences */
    for (seq_index = 0; seq_index < (Uint4)query_info->num_queries; 
         ++seq_index) { 
        BlastSeqLoc** prot_seqloc = 
            &(prot_seqloc_array[NUM_FRAMES*seq_index]);
        BlastSeqLoc* dna_seqloc = mask_loc->seqloc_array[seq_index];
        Int4 dna_length = s_GetTranslatedQueryDNALength(query_info, seq_index);
        Int4 context;

        /* Reproduce this mask for all 6 frames, with translated coordinates */
        for (context = 0; context < NUM_FRAMES; ++context) {
            BlastSeqLoc* prot_head=NULL;
            BlastSeqLoc* seqloc_var;
            Int2 frame = BLAST_ContextToFrame(eBlastTypeBlastx, context);

            prot_head = NULL;
            for (seqloc_var = dna_seqloc; seqloc_var; 
                 seqloc_var = seqloc_var->next) {
                Int4 from, to;
                SSeqRange* seq_range = seqloc_var->ssr;
                if (frame < 0) {
                    from = (dna_length + frame - seq_range->right)/CODON_LENGTH;
                    to = (dna_length + frame - seq_range->left)/CODON_LENGTH;
                } else {
                    from = (seq_range->left - frame + 1)/CODON_LENGTH;
                    to = (seq_range->right - frame + 1)/CODON_LENGTH;
                }
                BlastSeqLocNew(&prot_head, from, to);
            }
            prot_seqloc[context] = prot_head;
        }
        BlastSeqLocFree(dna_seqloc);
    }
    sfree(mask_loc->seqloc_array);
    mask_loc->seqloc_array = prot_seqloc_array;

    return 0;
}


Int2 BlastMaskLocProteinToDNA(BlastMaskLoc* mask_loc, 
                              const BlastQueryInfo* query_info)
{
   Int2 status = 0;
   Int4 index;

   /* If there is not mask, there is nothing to convert to DNA coordinates,
      hence just return. */
   if (!mask_loc) 
      return 0;

   /* Check that the array size in BlastMaskLoc corresponds to the number
      of contexts in BlastQueryInfo. */
   ASSERT(mask_loc->total_size == query_info->last_context + 1);

   /* Loop over all DNA sequences */
   for (index=0; index < query_info->num_queries; ++index)
   {
       Int4 frame_start = index*NUM_FRAMES;
       Int4 frame_index;
       Int4 dna_length = s_GetTranslatedQueryDNALength(query_info, index);
       /* Loop over all frames of one DNA sequence */
       for (frame_index=frame_start; frame_index<(frame_start+NUM_FRAMES); 
            frame_index++) {
           BlastSeqLoc* loc;
           Int2 frame = 
               BLAST_ContextToFrame(eBlastTypeBlastx, frame_index % NUM_FRAMES);
           /* Loop over all mask locations for a given frame */
           for (loc = mask_loc->seqloc_array[frame_index]; loc; loc = loc->next) {
               Int4 from=0, to=0;
               SSeqRange* seq_range = loc->ssr;
               if (frame < 0) {
                   to = dna_length - CODON_LENGTH*seq_range->left + frame;
                   from = dna_length - CODON_LENGTH*seq_range->right + frame + 1;
               } else {
                   from = CODON_LENGTH*seq_range->left + frame - 1;
                   to = CODON_LENGTH*seq_range->right + frame - 1;
               }
               seq_range->left = from;
               seq_range->right = to;
           }
       }
   }
   return status;
}

/** Used for qsort, compares two SeqLoc's by starting position. */
static int s_SeqRangeSortByStartPosition(const void *vp1, const void *vp2)
{
   BlastSeqLoc* v1 = *((BlastSeqLoc**) vp1);
   BlastSeqLoc* v2 = *((BlastSeqLoc**) vp2);
   SSeqRange* loc1 = (SSeqRange*) v1->ssr;
   SSeqRange* loc2 = (SSeqRange*) v2->ssr;
   
   if (loc1->left < loc2->left)
      return -1;
   else if (loc1->left > loc2->left)
      return 1;
   else
      return 0;
}

static Int4 s_BlastSeqLocLen(BlastSeqLoc* var)
{
     Int4 count=0;
   
     while (var)
     {
         count++;
         var = var->next;
     }

     return count;
}

/** Sort a list of BlastSeqLoc structures
 * @param list List of BlastSeqLoc's [in]
 * @param compar Comparison function to use [in]
 * @return Sorted list.  
 */
static BlastSeqLoc* 
s_BlastSeqLocSort (BlastSeqLoc* list,
                 int (*compar )(const void *, const void *))
{
        BlastSeqLoc* tmp,** head;
        Int4 count, i;

        if (list == NULL) 
           return NULL;

        count = s_BlastSeqLocLen (list);
        head = (BlastSeqLoc* *) calloc (((size_t) count + 1), sizeof (BlastSeqLoc*));
        for (tmp = list, i = 0; tmp != NULL && i < count; i++) {
                head [i] = tmp;
                tmp = tmp->next;
        }

        qsort (head, (size_t) count, sizeof (BlastSeqLoc*), compar);
        for (i = 0; i < count; i++) {
                tmp = head [i];
                tmp->next = head [i + 1];
        }
        list = head [0];
        sfree (head);

        return list;
}

/* This will go in place of CombineSeqLocs to combine filtered locations */
Int2
CombineMaskLocations(BlastSeqLoc* mask_loc, BlastSeqLoc* *mask_loc_out,
                     Int4 link_value)
{
   Int2 status=0;		/* return value. */
   Int4 start, stop;	/* USed to merge overlapping SeqLoc's. */
   SSeqRange* ssr = NULL;
   BlastSeqLoc* loc_head=NULL,* last_loc=NULL,* loc_var=NULL;
   BlastSeqLoc* new_loc = NULL,* new_loc_last = NULL;
   
   if (!mask_loc) {
      *mask_loc_out = NULL;
      return 0;
   }

   /* Put all the SeqLoc's into one big linked list. */
   loc_var = mask_loc;
   loc_head = last_loc = s_BlastSeqLocDup(loc_var);
   while (loc_var->next)
   {
       last_loc->next = s_BlastSeqLocDup(loc_var->next);
       last_loc = last_loc->next;
       loc_var = loc_var->next;
   }
   
   /* Sort them by starting position. */
   loc_head = (BlastSeqLoc*)   
      s_BlastSeqLocSort (loc_head, s_SeqRangeSortByStartPosition);
   
   ssr = (SSeqRange*) loc_head->ssr;
   start = ssr->left;
   stop = ssr->right;
   loc_var = loc_head;
   ssr = NULL;

   while (loc_var) {
      if (loc_var->next)
         ssr = loc_var->next->ssr;
      if (ssr && ((stop + link_value) > ssr->left)) {
         stop = MAX(stop, ssr->right);
      } else {
         if (!new_loc)
            new_loc_last = BlastSeqLocNew(&new_loc, start, stop);
         else
            new_loc_last = BlastSeqLocNew(&new_loc_last, start, stop);
         if (loc_var->next) {
             start = ssr->left;
             stop = ssr->right;
         }
      }
      loc_var = loc_var->next;
      ssr = NULL;
   }

   *mask_loc_out = new_loc;
      
   /* Free memory allocated for the temporary list of SeqLocs */
   BlastSeqLocFree(loc_head);

   return status;
}

Int2 
BLAST_ComplementMaskLocations(EBlastProgramType program_number, 
   BlastQueryInfo* query_info, 
   BlastMaskLoc* mask_loc, BlastSeqLoc* *complement_mask) 
{
   Int4 context;
   BlastSeqLoc* loc,* last_loc = NULL,* start_loc = NULL;
   const Boolean kIsNucl = (program_number == eBlastTypeBlastn);

   if (complement_mask == NULL)
	return -1;

   *complement_mask = NULL;

   for (context = query_info->first_context; 
        context <= query_info->last_context; ++context) {

      Boolean first = TRUE;	/* Specifies beginning of query. */
      Boolean last_interval_open=TRUE; /* if TRUE last interval needs to be closed. */
      Boolean reverse = FALSE; /* Sequence on minus strand. */
      Int4 index; /* loop index */
      Int4 start_offset, end_offset, filter_start, filter_end;
      Int4 left=0, right; /* Used for left/right extent of a region. */

      start_offset = query_info->contexts[context].query_offset;
      end_offset = query_info->contexts[context].query_length + start_offset - 1;
      
      /* For blastn: check if this strand is not searched at all */
      if (end_offset < start_offset)
          continue;
      index = BlastGetMaskLocIndexFromContext(kIsNucl, context);
      reverse = BlastIsReverseStrand(kIsNucl, context);
     

      /* mask_loc NULL is simply the case that NULL was passed in, which we take to 
         mean that nothing on query is masked. */
      if (mask_loc == NULL || mask_loc->seqloc_array[index] == NULL)
      {
         /* No masks for this context */
         if (!last_loc)
            last_loc = BlastSeqLocNew(complement_mask, start_offset, end_offset);
         else 
            last_loc = BlastSeqLocNew(&last_loc, start_offset, end_offset);
         continue;
      }
      
      if (reverse) {
         BlastSeqLoc* prev_loc = NULL;
         /* Reverse the order of the locations */
         for (start_loc = mask_loc->seqloc_array[index]; start_loc; 
              start_loc = start_loc->next) {
            loc = s_BlastSeqLocDup(start_loc);
            loc->next = prev_loc;
            prev_loc = loc;
         }
         /* Save where this list starts, so it can be freed later */
         start_loc = loc;
      } else {
         loc = mask_loc->seqloc_array[index];
      }

      first = TRUE;
      for ( ; loc; loc = loc->next) {
         SSeqRange* seq_range = loc->ssr;
         if (reverse) {
            filter_start = end_offset - seq_range->right;
            filter_end = end_offset - seq_range->left;
         } else {
            filter_start = start_offset + seq_range->left;
            filter_end = start_offset + seq_range->right;
         }
         /* The canonical "state" at the top of this 
            while loop is that both "left" and "right" have
            been initialized to their correct values.  
            The first time this loop is entered in a call to 
            the function this is not true and the following "if" 
            statement moves everything to the canonical state. */
         if (first) {
            last_interval_open = TRUE;
            first = FALSE;
            
            if (filter_start > start_offset) {
               /* beginning of sequence not filtered */
               left = start_offset;
            } else {
               /* beginning of sequence filtered */
               left = filter_end + 1;
               continue;
            }
         }

         right = filter_start - 1;

         if (!last_loc)
            last_loc = BlastSeqLocNew(complement_mask, left, right);
         else 
            last_loc = BlastSeqLocNew(&last_loc, left, right);
         if (filter_end >= end_offset) {
            /* last masked region at end of sequence */
            last_interval_open = FALSE;
            break;
         } else {
            left = filter_end + 1;
         }
      }

      if (reverse) {
         start_loc = BlastSeqLocFree(start_loc);
      }
      
      if (last_interval_open) {
         /* Need to finish SSeqRange* for last interval. */
         right = end_offset;
         if (!last_loc)
            last_loc = BlastSeqLocNew(complement_mask, left, right);
         else 
            last_loc = BlastSeqLocNew(&last_loc, left, right);
      }
   }
   return 0;
}

#define BLASTOPTIONS_BUFFER_SIZE 128

/** Parses options used for dust.
 * @param ptr buffer containing instructions. [in]
 * @param level sets level for dust. [out]
 * @param window sets window for dust [out]
 * @param cutoff sets cutoff for dust. [out] 
 * @param linker sets linker for dust. [out]
*/
static Int2
s_ParseDustOptions(const char *ptr, Int2* level, Int2* window,
	Int2* cutoff, Int2* linker)

{
	char buffer[BLASTOPTIONS_BUFFER_SIZE];
	Int4 arg, index, index1, window_pri=-1, linker_pri=-1, level_pri=-1, cutoff_pri=-1;
	long	tmplong;

	arg = 0;
	index1 = 0;
	for (index=0; index<BLASTOPTIONS_BUFFER_SIZE; index++)
	{
		if (*ptr == ' ' || *ptr == NULLB)
		{
			buffer[index1] = NULLB;
			index1 = 0;
			switch(arg) {
				case 0:
					sscanf(buffer, "%ld", &tmplong);
					level_pri = tmplong;
					break;
				case 1:
					sscanf(buffer, "%ld", &tmplong);
					window_pri = tmplong;
					break;
				case 2:
					sscanf(buffer, "%ld", &tmplong);
					cutoff_pri = tmplong;
					break;
				case 3:
					sscanf(buffer, "%ld", &tmplong);
					linker_pri = tmplong;
					break;
				default:
					break;
			}

			arg++;
			while (*ptr == ' ')
				ptr++;

			/* end of the buffer. */
			if (*ptr == NULLB)
				break;
		}
		else
		{
			buffer[index1] = *ptr; ptr++;
			index1++;
		}
	}

	*level = level_pri; 
	*window = window_pri; 
	*cutoff = cutoff_pri; 
	*linker = linker_pri; 

	return 0;
}

/** parses a string to set three seg options. 
 * @param ptr buffer containing instructions [in]
 * @param window returns "window" for seg algorithm. [out]
 * @param locut returns "locut" for seg. [out]
 * @param hicut returns "hicut" for seg. [out]
*/
static Int2
s_ParseSegOptions(const char *ptr, Int4* window, double* locut, double* hicut)

{
	char buffer[BLASTOPTIONS_BUFFER_SIZE];
	Int4 arg, index, index1; 
	long	tmplong;
	double	tmpdouble;

	arg = 0;
	index1 = 0;
	for (index=0; index<BLASTOPTIONS_BUFFER_SIZE; index++)
	{
		if (*ptr == ' ' || *ptr == NULLB)
		{
			buffer[index1] = NULLB;
			index1 = 0;
			switch(arg) {
				case 0:
					sscanf(buffer, "%ld", &tmplong);
					*window = tmplong;
					break;
				case 1:
					sscanf(buffer, "%le", &tmpdouble);
					*locut = tmpdouble;
					break;
				case 2:
					sscanf(buffer, "%le", &tmpdouble);
					*hicut = tmpdouble;
					break;
				default:
					break;
			}

			arg++;
			while (*ptr == ' ')
				ptr++;

			/* end of the buffer. */
			if (*ptr == NULLB)
				break;
		}
		else
		{
			buffer[index1] = *ptr; ptr++;
			index1++;
		}
	}

	return 0;
}

#ifdef CC_FILTER_ALLOWED
/** Coiled-coiled algorithm parameters. 
 * @param ptr buffer containing instructions. [in]
 * @param window returns window for coil-coiled algorithm. [out]
 * @param cutoff cutoff for coil-coiled algorithm [out]
 * @param linker returns linker [out]
*/
static Int2
s_ParseCoilCoilOptions(const char *ptr, Int4* window, double* cutoff, Int4* linker)

{
	char buffer[BLASTOPTIONS_BUFFER_SIZE];
	Int4 arg, index, index1;
	long	tmplong;
	double	tmpdouble;

	arg = 0;
	index1 = 0;
	for (index=0; index<BLASTOPTIONS_BUFFER_SIZE; index++)
	{
		if (*ptr == ' ' || *ptr == NULLB)
		{
			buffer[index1] = NULLB;
			index1 = 0;
			switch(arg) {
				case 0:
					sscanf(buffer, "%ld", &tmplong);
					*window = tmplong;
					break;
				case 1:
					sscanf(buffer, "%le", &tmpdouble);
					*cutoff = tmpdouble;
					break;
				case 2:
					sscanf(buffer, "%ld", &tmplong);
					*linker = tmplong;
					break;
				default:
					break;
			}

			arg++;
			while (*ptr == ' ')
				ptr++;

			/* end of the buffer. */
			if (*ptr == NULLB)
				break;
		}
		else
		{
			buffer[index1] = *ptr; ptr++;
			index1++;
		}
	}

	return 0;
}
#endif

/** Copies filtering commands for one filtering algorithm from "instructions" to
 * "buffer". 
 * ";" is a delimiter for the commands for different algorithms, so it stops
 * copying when a ";" is found. 
 * Example filtering string: "m L; R -d rodents.lib"
 * @param instructions filtering commands [in] 
 * @param buffer filled with filtering commands for one algorithm. [out]
*/
static const char *
s_LoadOptionsToBuffer(const char *instructions, char* buffer)
{
	Boolean not_started=TRUE;
	char* buffer_ptr;
	const char *ptr;
	Int4 index;

	ptr = instructions;
	buffer_ptr = buffer;
	for (index=0; index<BLASTOPTIONS_BUFFER_SIZE && *ptr != NULLB; index++)
	{
		if (*ptr == ';')
		{	/* ";" is a delimiter for different filtering algorithms. */
			ptr++;
			break;
		}
		/* Remove blanks at the beginning. */
		if (not_started && *ptr == ' ')
		{
			ptr++;
		}
		else
		{
			not_started = FALSE;
			*buffer_ptr = *ptr;
			buffer_ptr++; ptr++;
		}
	}

	*buffer_ptr = NULLB;

	if (not_started == FALSE)
	{	/* Remove trailing blanks. */
		buffer_ptr--;
		while (*buffer_ptr == ' ' && buffer_ptr > buffer)
		{
			*buffer_ptr = NULLB;
			buffer_ptr--;
		}
	}

	return ptr;
}

#define CC_WINDOW 22
#define CC_CUTOFF 40.0
#define CC_LINKER 32

Int2
BlastSetUp_Filter(EBlastProgramType program_number, Uint1* sequence, Int4 length, 
   Int4 offset, const char* instructions, Boolean *mask_at_hash, 
   BlastSeqLoc* *seqloc_retval)
{
	Boolean do_default=FALSE, do_seg=FALSE, do_dust=FALSE; 
	char* buffer=NULL;
   const char *ptr;
	Int2 seqloc_num;
	Int2 status=0;		/* return value. */
	Int2 window_dust, level_dust, minwin_dust, linker_dust;
   BlastSeqLoc* dust_loc = NULL,* seg_loc = NULL;
	SegParameters* sparamsp=NULL;
#ifdef CC_FILTER_ALLOWED
   Boolean do_coil_coil = FALSE;
   BlastSeqLoc* cc_loc = NULL;
	PccDatPtr pccp;
   Int4 window_cc, linker_cc;
	double cutoff_cc;
#endif
#ifdef TEMP_BLAST_OPTIONS
   /* TEMP_BLAST_OPTIONS is set to zero until these are implemented. */
   BlastSeqLoc* vs_loc = NULL,* repeat_loc = NULL;
	BLAST_OptionsBlkPtr repeat_options, vs_options;
	Boolean do_repeats=FALSE; 	/* screen for orgn. specific repeats. */
	Boolean do_vecscreen=FALSE;	/* screen for vector contamination. */
	Boolean myslp_allocated;
	char* repeat_database=NULL,* vs_database=NULL,* error_msg;
	SeqLocPtr repeat_slp=NULL, vs_slp=NULL;
	SeqAlignPtr seqalign;
	SeqLocPtr myslp, seqloc_var, seqloc_tmp;
	ListNode* vnp=NULL,* vnp_var;
#endif

#ifdef CC_FILTER_ALLOWED
	cutoff_cc = CC_CUTOFF;
#endif

   if (!seqloc_retval) 
      return -1;

	/* FALSE is the default right now. */
	if (mask_at_hash)
      *mask_at_hash = FALSE;
   
   *seqloc_retval = NULL;

	if (instructions == NULL || strcasecmp(instructions, "F") == 0)
		return status;

	/* parameters for dust. */
	/* -1 indicates defaults. */
	level_dust = -1;
	window_dust = -1;
	linker_dust = -1;
	if (strcasecmp(instructions, "T") == 0)
	{ /* do_default actually means seg for proteins and dust for nt. */
		do_default = TRUE;
	}
	else
	{
		buffer = (char*) calloc(strlen(instructions), sizeof(char));
		ptr = instructions;
		/* allow old-style filters when m cannot be followed by the ';' */
		if (*ptr == 'm' && ptr[1] == ' ')
		{
			if (mask_at_hash)
				*mask_at_hash = TRUE;
			ptr += 2;
		}
		while (*ptr != NULLB)
		{
			if (*ptr == 'S')
			{
				sparamsp = SegParametersNewAa();
				sparamsp->overlaps = TRUE;	/* merge overlapping segments. */
				ptr = s_LoadOptionsToBuffer(ptr+1, buffer);
				if (buffer[0] != NULLB)
				{
					s_ParseSegOptions(buffer, &sparamsp->window, &sparamsp->locut, &sparamsp->hicut);
				}
				do_seg = TRUE;
			}
			else if (*ptr == 'C')
			{
#ifdef CC_FILTER_ALLOWED
				ptr = s_LoadOptionsToBuffer(ptr+1, buffer);
				window_cc = CC_WINDOW;
				cutoff_cc = CC_CUTOFF;
				linker_cc = CC_LINKER;
				if (buffer[0] != NULLB)
					s_ParseCoilCoilOptions(buffer, &window_cc, &cutoff_cc, &linker_cc);
				do_coil_coil = TRUE;
#endif
			}
			else if (*ptr == 'D')
			{
				ptr = s_LoadOptionsToBuffer(ptr+1, buffer);
				if (buffer[0] != NULLB)
					s_ParseDustOptions(buffer, &level_dust, &window_dust, &minwin_dust, &linker_dust);
				do_dust = TRUE;
			}
#ifdef TEMP_BLAST_OPTIONS
			else if (*ptr == 'R')
			{
				repeat_options = BLASTOptionNew("blastn", TRUE);
				repeat_options->expect_value = 0.1;
				repeat_options->penalty = -1;
				repeat_options->wordsize = 11;
				repeat_options->gap_x_dropoff_final = 90;
				repeat_options->dropoff_2nd_pass = 40;
				repeat_options->gap_open = 2;
				repeat_options->gap_extend = 1;
				ptr = s_LoadOptionsToBuffer(ptr+1, buffer);
				if (buffer[0] != NULLB)
                                   parse_blast_options(repeat_options,
                                      buffer, &error_msg, &repeat_database,
                                      NULL, NULL);
				if (repeat_database == NULL)
                                   repeat_database = strdup("humlines.lib humsines.lib retrovir.lib");
				do_repeats = TRUE;
			}
			else if (*ptr == 'V')
			{
				vs_options = VSBlastOptionNew();
				ptr = s_LoadOptionsToBuffer(ptr+1, buffer);
				if (buffer[0] != NULLB)
                                   parse_blast_options(vs_options, buffer,
                                      &error_msg, &vs_database, NULL, NULL); 
				vs_options = BLASTOptionDelete(vs_options);
				if (vs_database == NULL)
                                   vs_database = strdup("UniVec_Core");
				do_vecscreen = TRUE;
			}
#endif
			else if (*ptr == 'L')
			{ /* do low-complexity filtering; dust for blastn, otherwise seg.*/
				do_default = TRUE;
				ptr++;
			}
			else if (*ptr == 'm')
			{
				if (mask_at_hash)
					*mask_at_hash = TRUE;
				ptr++;
			}
			else
			{	/* Nothing applied. */
				ptr++;
			}
		}
	        sfree(buffer);
	}

	seqloc_num = 0;
	if (program_number != eBlastTypeBlastn)
	{
		if (do_default || do_seg)
		{
			SeqBufferSeg(sequence, length, offset, sparamsp, &seg_loc);
			SegParametersFree(sparamsp);
			sparamsp = NULL;
			seqloc_num++;
		}
#ifdef CC_FILTER_ALLOWED
		if (do_coil_coil)
		{
			pccp = PccDatNew ();
			pccp->window = window_cc;
			ReadPccData (pccp);
			scores = PredictCCSeqLoc(slp, pccp);
			cc_slp = FilterCC(scores, cutoff_cc, length, linker_cc,
                                          SeqIdDup(sip), FALSE);
			sfree(scores);
			PccDatFree (pccp);
			seqloc_num++;
		}
#endif
	}
	else
	{
		if (do_default || do_dust)
		{
         SeqBufferDust(sequence, length, offset, level_dust, window_dust,
                       linker_dust, &dust_loc);
			seqloc_num++;
		}
#ifdef TEMP_BLAST_OPTIONS
		if (do_repeats)
		{
		/* Either the SeqLocPtr is SEQLOC_WHOLE (both strands) or SEQLOC_INT (probably 
one strand).  In that case we make up a double-stranded one as we wish to look at both strands. */
			myslp_allocated = FALSE;
			if (slp->choice == SEQLOC_INT)
			{
				myslp = SeqLocIntNew(SeqLocStart(slp), SeqLocStop(slp), Seq_strand_both, SeqLocId(slp));
				myslp_allocated = TRUE;
			}
			else
			{
				myslp = slp;
			}
start_timer;
			repeat_slp = BioseqHitRangeEngineByLoc(myslp, "blastn", repeat_database, repeat_options, NULL, NULL, NULL, NULL, NULL, 0);
stop_timer("after repeat filtering");
			repeat_options = BLASTOptionDelete(repeat_options);
			sfree(repeat_database);
			if (myslp_allocated)
				SeqLocFree(myslp);
			seqloc_num++;
		}
		if (do_vecscreen)
		{
		/* Either the SeqLocPtr is SEQLOC_WHOLE (both strands) or SEQLOC_INT (probably 
one strand).  In that case we make up a double-stranded one as we wish to look at both strands. */
			myslp_allocated = FALSE;
			if (slp->choice == SEQLOC_INT)
			{
				myslp = SeqLocIntNew(SeqLocStart(slp), SeqLocStop(slp), Seq_strand_both, SeqLocId(slp));
				myslp_allocated = TRUE;
			}
			else
			{
				myslp = slp;
			}
			VSScreenSequenceByLoc(myslp, NULL, vs_database, &seqalign, &vnp, NULL, NULL);
			vnp_var = vnp;
			while (vnp_var)
			{
				seqloc_tmp = vnp_var->ptr;
				if (vs_slp == NULL)
				{
					vs_slp = seqloc_tmp;
				}
				else
				{
					seqloc_var = vs_slp;
					while (seqloc_var->next)
						seqloc_var = seqloc_var->next;
					seqloc_var->next = seqloc_tmp;
				}
				vnp_var->ptr = NULL;
				vnp_var = vnp_var->next;
			}
			vnp = ListNodeFree(vnp);
			seqalign = SeqAlignSetFree(seqalign);
			sfree(vs_database);
			if (myslp_allocated)
				SeqLocFree(myslp);
			seqloc_num++;
		}
#endif
	}

	if (seqloc_num)
	{ 
		BlastSeqLoc* seqloc_list=NULL;  /* Holds all SeqLoc's for
                                                      return. */
#if 0
		if (seg_slp)
			ListNodeAddPointer(&seqloc_list, SEQLOC_MIX, seg_slp);
		if (cc_slp)
			ListNodeAddPointer(&seqloc_list, SEQLOC_MIX, cc_slp);
		if (dust_slp)
			ListNodeAddPointer(&seqloc_list, SEQLOC_MIX, dust_slp);
		if (repeat_slp)
			ListNodeAddPointer(&seqloc_list, SEQLOC_MIX, repeat_slp);
		if (vs_slp)
			ListNodeAddPointer(&seqloc_list, SEQLOC_MIX, vs_slp);
#endif
      if (dust_loc)
         seqloc_list = dust_loc;
      if (seg_loc)
         seqloc_list = seg_loc;

		*seqloc_retval = seqloc_list;
	}

	return status;
}

static Int2
s_GetReversedLocation(BlastSeqLoc** filter_out, BlastSeqLoc* filter_in, Int4 query_length)
{
   BlastSeqLoc* mask_loc = NULL;
   BlastSeqLoc* last_filter_per_context = NULL;

   ASSERT(filter_out);

   *filter_out = NULL;

   for (mask_loc=filter_in; mask_loc; mask_loc = mask_loc->next) {
        Int4 start, stop;
        SSeqRange* loc = mask_loc->ssr;
        
        start = query_length - 1 - loc->right;
        stop = query_length - 1 - loc->left;
        if (last_filter_per_context == NULL)
              last_filter_per_context = BlastSeqLocNew(filter_out, start, stop);
        else
              last_filter_per_context = BlastSeqLocNew(&last_filter_per_context, start, stop);
   }

   return 0;
}

static Int2
s_GetFilteringLocationsForOneContext(BLAST_SequenceBlk* query_blk, BlastQueryInfo* query_info, Int2 context, EBlastProgramType program_number, const char* filter_string, BlastSeqLoc* *filter_out, Boolean* mask_at_hash)
{
        Int2 status = 0;
        Int4 query_length = 0;      /* Length of query described by SeqLocPtr. */
        Int4 context_offset;
        BlastSeqLoc *lcase_mask_slp = NULL; /* Auxiliary locations for lower-case masking  */
        BlastSeqLoc *filter_slp = NULL;     /* SeqLocPtr computed for filtering. */
        BlastSeqLoc *filter_slp_rev = NULL;     /* reversed SeqLocPtr (used only if minus strand) */
        BlastSeqLoc *filter_slp_combined;   /* Used to hold combined SeqLoc's */
        Uint1 *buffer;              /* holds sequence for plus strand or protein. */

        const Boolean kIsNucl = (program_number == eBlastTypeBlastn);
        Int2 index = BlastGetMaskLocIndexFromContext(kIsNucl, context);
        
        context_offset = query_info->contexts[context].query_offset;
        buffer = &query_blk->sequence[context_offset];

        if ((query_length = query_info->contexts[context].query_length) <= 0)
           return 0;


        if ((status = BlastSetUp_Filter(program_number, buffer,
                       query_length, 0, filter_string,
                             mask_at_hash, &filter_slp))) 
             return status;

        if (BlastIsReverseStrand(kIsNucl, context) == TRUE)
        {  /* Reverse this as it's on minus strand. */
              s_GetReversedLocation(&filter_slp_rev, filter_slp, query_length);
              filter_slp = BlastSeqLocFree(filter_slp);
              filter_slp = filter_slp_rev;
        }

        /* Extract the mask locations corresponding to this query 
               (frame, strand), detach it from other masks.
               NB: for translated search the mask locations are expected in 
               protein coordinates. The nucleotide locations must be converted
               to protein coordinates prior to the call to BLAST_MainSetUp.
        */
        lcase_mask_slp = NULL;
        if (query_blk->lcase_mask && query_blk->lcase_mask->seqloc_array)
        {
             lcase_mask_slp = query_blk->lcase_mask->seqloc_array[index];
             /* Set location list to NULL, to allow safe memory deallocation, 
               ownership transferred to filter_slp below. */
             query_blk->lcase_mask->seqloc_array[index] = NULL;
        }

        /* Attach the lower case mask locations to the filter locations and combine them */
        if (lcase_mask_slp) {
             if (filter_slp) {
                  BlastSeqLoc *loc;           /* Iterator variable */
                  for (loc = filter_slp; loc->next; loc = loc->next);
                    loc->next = lcase_mask_slp;
             } else {
                   filter_slp = lcase_mask_slp;
             }
        }

        filter_slp_combined = NULL;
        CombineMaskLocations(filter_slp, &filter_slp_combined, 0);
        *filter_out = filter_slp_combined;

        filter_slp = BlastSeqLocFree(filter_slp);

	return 0;
}


Int2
BlastSetUp_GetFilteringLocations(BLAST_SequenceBlk* query_blk, BlastQueryInfo* query_info, EBlastProgramType program_number, const char* filter_string, BlastMaskLoc** filter_maskloc, Boolean* mask_at_hash, Blast_Message * *blast_message)
{

    Int2 status = 0;
    Int2 context = 0; /* loop variable. */
    const Boolean kIsNucl = (program_number == eBlastTypeBlastn);
    Boolean no_forward_strand = (query_info->first_context > 0);  /* filtering needed on reverse strand. */

    ASSERT(query_info && query_blk && filter_maskloc);

    *filter_maskloc = BlastMaskLocNew(query_info->last_context+1);

    for (context = query_info->first_context;
         context <= query_info->last_context; ++context) {
      
        BlastSeqLoc *filter_per_context = NULL;   /* Used to hold combined SeqLoc's */
        Boolean reverse = BlastIsReverseStrand(kIsNucl, context);
        Int4 query_length = 0;

        /* For each query, check if forward strand is present */
        if ((query_length = query_info->contexts[context].query_length) <= 0)
        {
            if (kIsNucl && (context & 1) == 0)  /* Needed only for blastn, or does this not apply FIXME */
               no_forward_strand = TRUE;  /* No plus strand, we cannot simply infer locations by going from plus to minus */
            continue;
        }
        else if (!reverse)  /* This is a plus strand, safe to set no_forward_strand to FALSE as clearly there is one. */
               no_forward_strand = FALSE;

        if (!reverse || no_forward_strand)
        {
            Int4 filter_index = BlastGetMaskLocIndexFromContext(kIsNucl, context);
            if ((status=s_GetFilteringLocationsForOneContext(query_blk, query_info, context, program_number, filter_string, &filter_per_context, mask_at_hash)))
            {
               Blast_MessageWrite(blast_message, BLAST_SEV_ERROR, 2, 1, 
                  "Failure at filtering");
               return status;
            }

        /* NB: for translated searches filter locations are returned in 
               protein coordinates, because the DNA lengths of sequences are 
               not available here. The caller must take care of converting 
               them back to nucleotide coordinates. */
             (*filter_maskloc)->seqloc_array[filter_index] = filter_per_context;
         }
    }

    return 0;
}

Int2
Blast_MaskTheResidues(Uint1 * buffer, Int4 length, Boolean is_na,
                           BlastSeqLoc* mask_loc, Boolean reverse, Int4 offset)
{
    SSeqRange *loc = NULL;
    Int2 status = 0;
    Int4 index, start, stop;
    const Uint1 kMaskingLetter = is_na ? kNuclMask : kProtMask;

    for (; mask_loc; mask_loc = mask_loc->next) {
        loc = (SSeqRange *) mask_loc->ssr;
        if (reverse) {
            start = length - 1 - loc->right;
            stop = length - 1 - loc->left;
        } else {
            start = loc->left;
            stop = loc->right;
        }

        start -= offset;
        stop -= offset;

        for (index = start; index <= stop; index++)
            buffer[index] = kMaskingLetter;
    }

    return status;
}

Int2 
BlastSetUp_MaskQuery(BLAST_SequenceBlk* query_blk, BlastQueryInfo* query_info, BlastMaskLoc *filter_maskloc, EBlastProgramType program_number)
{
    const Boolean kIsNucl = (program_number == eBlastTypeBlastn);
    Int4 context; /* loop variable. */
    Int2 status=0;

    for (context = query_info->first_context;
         context <= query_info->last_context; ++context) {
      
        BlastSeqLoc *filter_per_context = NULL;   /* Used to hold combined SeqLoc's */
        Boolean reverse = BlastIsReverseStrand(kIsNucl, context);
        Int4 query_length;
        Int4 context_offset;
        Int4 maskloc_index;
        Uint1 *buffer;              /* holds sequence */

        /* For each query, check if forward strand is present */
        if ((query_length = query_info->contexts[context].query_length) <= 0)
            continue;

        context_offset = query_info->contexts[context].query_offset;
        buffer = &query_blk->sequence[context_offset];

        maskloc_index = BlastGetMaskLocIndexFromContext(kIsNucl, context);
	filter_per_context = filter_maskloc->seqloc_array[maskloc_index];
        
        if (buffer) {

            if ((status =
                     Blast_MaskTheResidues(buffer, query_length, kIsNucl,
                                                filter_per_context, reverse, 0)))
            {
                    return status;
            }
        }
    }

    return 0;
}

char* Blast_GetRepeatsFilterOption(const char* filter_string)
{
    char* repeat_filter_string = NULL;
    const char* kPtr;
    
    if (!filter_string)
        return NULL;

    kPtr = strchr(filter_string, 'R');

    if (kPtr) {
        const char* kEnd = strstr(kPtr, ";");
        unsigned int length;
        if (kEnd)
            length = (unsigned int) (kEnd - kPtr);
        else
            length = strlen(kPtr);
        repeat_filter_string = (char*) malloc(length+1);
        strncpy(repeat_filter_string, kPtr, length);
        repeat_filter_string[length] = NULLB;
    }
    
    return repeat_filter_string;
}

void 
Blast_ParseRepeatOptions(const char* repeat_options, char** dbname)
{
    char* ptr;

    if (!dbname)
        return;
    if (!repeat_options || (*repeat_options != 'R')) {
        *dbname = NULL;
        return;
    }
    
    ptr = strstr(repeat_options, "-d");
    if (!ptr) {
        *dbname = strdup("humrep");
    } else {
        ptr += 2;
        while (*ptr == ' ' || *ptr == '\t')
            ++ptr;
        *dbname = strdup(ptr);
    }
}

