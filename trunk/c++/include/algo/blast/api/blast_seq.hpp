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
*   C++ Wrappers to NewBlast structures
*
*/

#ifndef ALGO_BLAST_API___BLAST_SEQ__HPP
#define ALGO_BLAST_API___BLAST_SEQ__HPP

#include <algo/blast/api/blast_aux.hpp>
#include <algo/blast/api/blast_option.hpp>
// NewBlast includes
#include <algo/blast/core/blast_options.h>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(blast)

int
BLAST_SetUpQuery(EProgram program_number, 
    TSeqLocVector &query_slp, const QuerySetUpOptions* query_options, 
    BlastQueryInfo** query_info, BLAST_SequenceBlk* *query_blk);

END_SCOPE(blast)
END_NCBI_SCOPE

/*
* ===========================================================================
*
* $Log$
* Revision 1.7  2003/08/19 20:22:05  dondosha
* EProgram definition moved from CBlastOption clase to blast scope
*
* Revision 1.6  2003/08/19 13:45:21  dicuccio
* Removed 'USING_SCOPE(objects)'.  Changed #include guards to be standards
* compliant.  Added 'objects::' where necessary.
*
* Revision 1.5  2003/08/18 22:17:52  camacho
* Renaming of SSeqLoc members
*
* Revision 1.4  2003/08/18 20:58:56  camacho
* Added blast namespace, removed *__.hpp includes
*
* Revision 1.3  2003/08/14 19:08:08  dondosha
* Use enum type for program argument to BLAST_SetUpQuery
*
* Revision 1.2  2003/08/12 19:18:45  dondosha
* Use TSeqLocVector type in functions
*
* Revision 1.1  2003/08/11 15:19:59  dondosha
* Headers for preparation of database BLAST search
*
* Revision 1.5  2003/08/01 17:40:56  dondosha
* Use renamed functions and structures from local blastkar.h
*
* Revision 1.4  2003/07/31 19:45:33  camacho
* Eliminate Ptr notation
*
* Revision 1.3  2003/07/30 15:00:01  camacho
* Do not use Malloc/MemNew/MemFree
*
* Revision 1.2  2003/07/14 22:17:17  camacho
* Convert CSeq_loc to BlastMaskPtr
*
* Revision 1.1  2003/07/10 18:34:19  camacho
* Initial revision
*
*
* ===========================================================================
*/

#endif  /* ALGO_BLAST_API___BLAST_AUX__HPP */
