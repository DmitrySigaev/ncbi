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

/** @file blast_types.hpp
 * Definitions of special type used in BLAST
 */

#ifndef ALGO_BLAST_API___BLAST_TYPE__HPP
#define ALGO_BLAST_API___BLAST_TYPE__HPP

#include <corelib/ncbistd.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqalign/Seq_align_set.hpp>
#include <objmgr/scope.hpp>

BEGIN_NCBI_SCOPE

BEGIN_SCOPE(blast)

/// This enumeration is to evolve into a task/program specific list that 
/// specifies sets of default parameters to easily conduct searches using BLAST
enum EProgram {
    eBlastn = 0,        ///< Nucl-Nucl (traditional blastn)
    eBlastp,            ///< Protein-Protein
    eBlastx,            ///< Translated nucl-Protein
    eTblastn,           ///< Protein-Translated nucl
    eTblastx,           ///< Translated nucl-Translated nucl
    eRPSBlast,          ///< protein-pssm (reverse-position-specific BLAST)
    eRPSTblastn,        ///< nucleotide-pssm (RPS blast with translated query)
    eMegablast,         ///< Nucl-Nucl (traditional megablast)
    eDiscMegablast,	   ///< Nucl-Nucl using discontiguous megablast
    ePSIBlast,          ///< PSI Blast
    eBlastProgramMax    ///< Undefined program
};

/// Structure combining a Seq-loc, scope and masking locations for one sequence
struct SSeqLoc {
    /// Seq-loc describing the sequence to use as query/subject to BLAST
    /// The types of Seq-loc currently supported are: whole, seq-interval...
    /// @todo FIXME Complete the list of supported seq-locs
    CConstRef<objects::CSeq_loc>     seqloc;

    /// Scope where the sequence referenced can be found by the toolkit's
    /// object manager
    mutable CRef<objects::CScope>    scope;

    /// Seq-loc describing regions to mask in the seqloc field
    CConstRef<objects::CSeq_loc>     mask;

    SSeqLoc()
        : seqloc(), scope(), mask() {}
    SSeqLoc(const objects::CSeq_loc* sl, objects::CScope* s)
        : seqloc(sl), scope(s), mask(0) {}
    SSeqLoc(const objects::CSeq_loc& sl, objects::CScope& s)
        : seqloc(&sl), scope(&s), mask(0) {}
    SSeqLoc(const objects::CSeq_loc* sl, objects::CScope* s,
            const objects::CSeq_loc* m)
        : seqloc(sl), scope(s), mask(m) {}
    SSeqLoc(const objects::CSeq_loc& sl, objects::CScope& s,
            const objects::CSeq_loc& m)
        : seqloc(&sl), scope(&s), mask(&m) {}
};

/// Vector of sequence locations
typedef vector<SSeqLoc>   TSeqLocVector;
/// Vector of Seq-align-sets
typedef vector< CRef<objects::CSeq_align_set> > TSeqAlignVector;

END_SCOPE(blast)
END_NCBI_SCOPE

/*
* ===========================================================================
*
* $Log$
* Revision 1.15  2004/07/06 15:46:09  dondosha
* Added doxygen comments
*
* Revision 1.14  2004/06/23 15:09:51  camacho
* Added doxygen comments for SSeqLoc
*
* Revision 1.13  2004/06/14 15:41:44  dondosha
* "Doxygenized" some comments.
*
* Revision 1.12  2004/05/19 14:52:00  camacho
* 1. Added doxygen tags to enable doxygen processing of algo/blast/core
* 2. Standardized copyright, CVS $Id string, $Log and rcsid formatting and i
*    location
* 3. Added use of @todo doxygen keyword
*
* Revision 1.11  2004/05/17 18:07:19  bealer
* - Add PSI Blast support.
*
* Revision 1.10  2004/04/16 14:32:54  papadopo
* add eRPSBlast and eRPSTblastn, moved the megablast enums so that the list of enums still corresponds to the list in blast_def.h
*
* Revision 1.9  2004/03/10 14:55:19  madden
* Added enum type for eRPSBlast
*
* Revision 1.8  2003/12/09 17:07:58  camacho
* Added missing initializer
*
* Revision 1.7  2003/11/26 18:22:16  camacho
* +Blast Option Handle classes
*
* Revision 1.6  2003/10/07 17:27:38  dondosha
* Lower case mask removed from options, added to the SSeqLoc structure
*
* Revision 1.5  2003/09/05 01:48:38  ucko
* Use full headers rather than forward declarations, which are
* insufficient for arguments of C(Const)Ref<>.
*
* Revision 1.4  2003/08/25 17:21:12  camacho
* Use forward declarations whenever possible
*
* Revision 1.3  2003/08/21 12:12:37  dicuccio
* Changed constructors of SSeqLoc to take pointers insted of CConstRef<>/CRef<>.
* Added constructors to take C++ references
*
* Revision 1.2  2003/08/20 14:45:26  dondosha
* All references to CDisplaySeqalign moved to blast_format.hpp
*
* Revision 1.1  2003/08/19 22:10:10  dondosha
* Special types definitions for use in BLAST
*
*
* ===========================================================================
*/

#endif  /* ALGO_BLAST_API___BLAST_TYPE__HPP */
