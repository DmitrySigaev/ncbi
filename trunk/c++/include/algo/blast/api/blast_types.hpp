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
* File Description:
*   Definitions of special type used in BLAST
*
*/

#ifndef ALGO_BLAST_API___BLAST_TYPE__HPP
#define ALGO_BLAST_API___BLAST_TYPE__HPP

#include <corelib/ncbistd.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqalign/Seq_align_set.hpp>
#include <objmgr/scope.hpp>

BEGIN_NCBI_SCOPE

BEGIN_SCOPE(blast)

/// Enumeration analogous to blast_type_* defines from blast_def.h
enum EProgram {
    eBlastn = 0,        //< Nucl-Nucl (traditional blastn)
    eBlastp,            //< Protein-Protein
    eBlastx,            //< Translated nucl-Protein
    eTblastn,           //< Protein-Translated nucl
    eTblastx,           //< Translated nucl-Translated nucl
	eMegablast,			//< Nucl-Nucl (traditional megablast)
	eDiscMegablast,		//< Nucl-Nucl using discontiguous megablast
    eRPSBlast,          //< protein-pssm (reverse-position-specific BLAST)
    eBlastProgramMax    //< Undefined program
};

struct SSeqLoc {
    CConstRef<objects::CSeq_loc>     seqloc;
    mutable CRef<objects::CScope>    scope;
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
typedef vector<SSeqLoc>   TSeqLocVector;
typedef vector< CRef<objects::CSeq_align_set> > TSeqAlignVector;

END_SCOPE(blast)
END_NCBI_SCOPE

/*
* ===========================================================================
*
* $Log$
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
