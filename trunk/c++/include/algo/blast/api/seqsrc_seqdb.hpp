#ifndef ALGO_BLAST_API___SEQSRC_SEQDB__HPP
#define ALGO_BLAST_API___SEQSRC_SEQDB__HPP

/*  $Id$
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
 * Author:  Ilya Dondoshansky
 *
 */

/// @file seqsrc_seqdb.hpp
/// Implementation of the BlastSeqSrc interface using the C++ BLAST databases
/// API


#include <algo/blast/core/blast_seqsrc.h>
#include <algo/blast/core/blast_def.h>
#include <algo/blast/api/blast_types.hpp>

/** @addtogroup AlgoBlast
 *
 * @{
 */

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(blast)

/// Encapsulates the arguments needed to initialize multi-sequence source.
struct SSeqDbSrcNewArgs {
    char* dbname;     /**< Database name */
    bool is_protein; /**< Is this database protein? */
    Int4 first_db_seq; /**< Ordinal id of the first sequence to search */
    Int4 final_db_seq; /**< Ordinal id of the last sequence to search */
};

extern "C" {

/** SeqDb sequence source constructor 
 * @param seq_src BlastSeqSrc structure (already allocated) to populate [in]
 * @param args Pointer to SSeqDbSrcNewArgs structure above [in]
 * @return Updated seq_src structure (with all function pointers initialized
 */
NCBI_XBLAST_EXPORT
BlastSeqSrc* SeqDbSrcNew(BlastSeqSrc* seq_src, void* args);

/** SeqDb sequence source destructor: frees its internal data structure and the
 * BlastSeqSrc structure itself.
 * @param seq_src BlastSeqSrc structure to free [in]
 * @return NULL
 */
NCBI_XBLAST_EXPORT
BlastSeqSrc* SeqDbSrcFree(BlastSeqSrc* seq_src);

/** SeqDb sequence source copier: creates a new reference to the CSeqDB object
 * and copies the rest of the BlastSeqSrc structure.
 * @param seq_src BlastSeqSrc structure to copy [in]
 * @return Pointer to the new BlastSeqSrc.
 */
NCBI_XBLAST_EXPORT
BlastSeqSrc* SeqDbSrcCopy(BlastSeqSrc* seq_src);

}

/** Initialize the sequence source structure.
 * @param dbname BLAST database name [in]
 * @param is_prot Is this a protein or nucleotide database? [in]
 * @param first_seq First ordinal id in the database to search [in]
 * @param last_seq Last ordinal id in the database to search 
 *                 (full database if 0) [in]
 * @param extra_arg Reserved for the future implementation of other database
 *                  restrictions [in]
 */
NCBI_XBLAST_EXPORT
BlastSeqSrc* 
SeqDbSrcInit(const char* dbname, Boolean is_prot, 
             int first_seq = 0, int last_seq = 0, void* extra_arg = NULL);

END_SCOPE(blast)
END_NCBI_SCOPE

/* @} */

/*
 * ===========================================================================
 * $Log$
 * Revision 1.8  2004/10/27 21:12:07  camacho
 * Add default parameters to SeqDbSrcInit for convenience
 *
 * Revision 1.7  2004/10/06 14:49:55  dondosha
 * Cosmetic change in parameter name
 *
 * Revision 1.6  2004/08/11 11:59:07  ivanov
 * Added export specifier NCBI_XBLAST_EXPORT
 *
 * Revision 1.5  2004/07/19 14:57:57  dondosha
 * Corrected file name
 *
 * Revision 1.4  2004/06/15 16:23:10  dondosha
 * Added SeqDbSrcCopy function, to implement BlastSeqSrcCopy interface
 *
 * Revision 1.3  2004/05/19 14:52:00  camacho
 * 1. Added doxygen tags to enable doxygen processing of algo/blast/core
 * 2. Standardized copyright, CVS $Id string, $Log and rcsid formatting and i
 *    location
 * 3. Added use of @todo doxygen keyword
 *
 * Revision 1.2  2004/04/08 14:46:06  camacho
 * Added missing extern "C" declarations"
 *
 * Revision 1.1  2004/04/06 20:43:54  dondosha
 * Sequence source for CSeqDB BLAST database interface
 *
 * ===========================================================================
 */

#endif /* ALGO_BLAST_API___SEQSRC_SEQDB__HPP */
