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
* Author:  Christiam Camacho
*
* File Description:
*   C++ Wrappers to NewBlast structures
*
*/

#ifndef ALGO_BLAST_API___BLAST_AUX__HPP
#define ALGO_BLAST_API___BLAST_AUX__HPP

#include <corelib/ncbistd.hpp>
#include <corelib/ddumpable.hpp>
#include <objmgr/scope.hpp>
#include <objects/seqalign/Seq_align_set.hpp>

#include <algo/blast/api/blast_types.hpp>
// NewBlast includes
#include <algo/blast/core/blast_util.h>
#include <algo/blast/core/blast_options.h>
#include <algo/blast/core/blast_filter.h> // Needed for BlastMask & BlastSeqLoc
#include <algo/blast/core/blast_extend.h>
#include <algo/blast/core/blast_gapalign.h>
#include <algo/blast/core/blast_hits.h>

BEGIN_NCBI_SCOPE

BEGIN_SCOPE(objects)
    class CSeq_loc;
END_SCOPE(objects)

template<>
struct CDeleter<BLAST_SequenceBlk>
{
    static void Delete(BLAST_SequenceBlk* p)
    { BlastSequenceBlkFree(p); }
};

template<>
struct CDeleter<BlastQueryInfo>
{
    static void Delete(BlastQueryInfo* p)
    { BlastQueryInfoFree(p); }
};

template<>
struct CDeleter<QuerySetUpOptions>
{
    static void Delete(QuerySetUpOptions* p)
    { BlastQuerySetUpOptionsFree(p); }
};

template<>
struct CDeleter<LookupTableOptions>
{
    static void Delete(LookupTableOptions* p)
    { LookupTableOptionsFree(p); }
};

template<>
struct CDeleter<LookupTableWrap>
{
    static void Delete(LookupTableWrap* p)
    { LookupTableWrapFree(p); }
};

template<>
struct CDeleter<BlastInitialWordOptions>
{
    static void Delete(BlastInitialWordOptions* p)
    { BlastInitialWordOptionsFree(p); }
};

template<>
struct CDeleter<BlastInitialWordParameters>
{
    static void Delete(BlastInitialWordParameters* p)
    { BlastInitialWordParametersFree(p); }
};

template<>
struct CDeleter<BLAST_ExtendWord>
{
    static void Delete(BLAST_ExtendWord* p)
    { BlastExtendWordFree(p); }
};

template<>
struct CDeleter<BlastExtensionOptions>
{
    static void Delete(BlastExtensionOptions* p)
    { BlastExtensionOptionsFree(p); }
};

template<>
struct CDeleter<BlastExtensionParameters>
{
    static void Delete(BlastExtensionParameters* p)
    { BlastExtensionParametersFree(p); }
};

template<>
struct CDeleter<BlastHitSavingOptions>
{
    static void Delete(BlastHitSavingOptions* p)
    { BlastHitSavingOptionsFree(p); }
};

template<>
struct CDeleter<BlastHitSavingParameters>
{
    static void Delete(BlastHitSavingParameters* p)
    { BlastHitSavingParametersFree(p); }
};

template<>
struct CDeleter<PSIBlastOptions>
{
    static void Delete(PSIBlastOptions* p)
    { sfree(p); }
};

template<>
struct CDeleter<BlastDatabaseOptions>
{
    static void Delete(BlastDatabaseOptions* p)
    { BlastDatabaseOptionsFree(p); }
};

template<>
struct CDeleter<BlastScoreBlk>
{
    static void Delete(BlastScoreBlk* p)
    { BlastScoreBlkFree(p); }
};

template<>
struct CDeleter<BlastScoringOptions>
{
    static void Delete(BlastScoringOptions* p)
    { BlastScoringOptionsFree(p); }
};

template<>
struct CDeleter<BlastEffectiveLengthsOptions>
{
    static void Delete(BlastEffectiveLengthsOptions* p)
    { BlastEffectiveLengthsOptionsFree(p); }
};

template<>
struct CDeleter<BlastGapAlignStruct>
{
    static void Delete(BlastGapAlignStruct* p)
    { BLAST_GapAlignStructFree(p); }
};

template<>
struct CDeleter<BlastResults>
{
    static void Delete(BlastResults* p)
    { BLAST_ResultsFree(p); }
};

BEGIN_SCOPE(blast)

/** Converts a CSeq_loc into a BlastMask structure used in NewBlast
 * @param sl CSeq_loc to convert [in]
 * @param index Number of frame/query number? this CSeq_loc applies to [in]
 * @return Linked list of BlastMask structures
 */
NCBI_XBLAST_EXPORT
BlastMask*
CSeqLoc2BlastMask(const objects::CSeq_loc &slp, int index);

/** Convert coordinates in masking locations for one sequence from DNA to 
 * protein, creating mask locations for each of the 6 translation frames.
 * @param mask Pointer to masking locations structure [in] [out]
 * @param seqloc DNA sequence data [in]
 * @param scope Which scope this sequence belongs to? [in]
 */
void BlastMaskDNAToProtein(BlastMask** mask, 
         const objects::CSeq_loc &seqloc, objects::CScope* scope);

/** Convert coordinates in masking locations for a set of sequences from
 * protein to DNA.
 * @param mask Pointer to masking locations for all frames of a set 
 *            of translated sequences [in] [out]
 * @param slp Vector of DNA sequence data [in]
 */
void BlastMaskProteinToDNA(BlastMask** mask, TSeqLocVector &slp);


/** Declares class to handle deallocating of the structure using the appropriate
 * function
 */

#define DECLARE_AUTO_CLASS_WRAPPER(struct_name, free_func) \
class C##struct_name : public CDebugDumpable \
{ \
public: \
    C##struct_name() : m_Ptr(NULL) {} \
    C##struct_name(struct_name* p) : m_Ptr(p) {} \
    void Reset(struct_name* p) { if (m_Ptr) { free_func(m_Ptr); } m_Ptr = p; } \
    ~C##struct_name() { if (m_Ptr) { free_func(m_Ptr); m_Ptr = NULL; } } \
    operator struct_name *() { return m_Ptr; } \
    operator struct_name *() const { return m_Ptr; } \
    struct_name* operator->() { return m_Ptr; } \
    struct_name* operator->() const { return m_Ptr; } \
    struct_name** operator&() { return &m_Ptr; } \
    void DebugDump(CDebugDumpContext ddc, unsigned int depth) const; \
private: \
    struct_name* m_Ptr; \
}

DECLARE_AUTO_CLASS_WRAPPER(BLAST_SequenceBlk, BlastSequenceBlkFree);

DECLARE_AUTO_CLASS_WRAPPER(BlastQueryInfo, BlastQueryInfoFree);
DECLARE_AUTO_CLASS_WRAPPER(QuerySetUpOptions, BlastQuerySetUpOptionsFree);

DECLARE_AUTO_CLASS_WRAPPER(LookupTableOptions, LookupTableOptionsFree);
DECLARE_AUTO_CLASS_WRAPPER(LookupTableWrap, LookupTableWrapFree);

DECLARE_AUTO_CLASS_WRAPPER(BlastInitialWordOptions,
                           BlastInitialWordOptionsFree);
DECLARE_AUTO_CLASS_WRAPPER(BlastInitialWordParameters,
                           BlastInitialWordParametersFree);

DECLARE_AUTO_CLASS_WRAPPER(BLAST_ExtendWord, BlastExtendWordFree);
DECLARE_AUTO_CLASS_WRAPPER(BlastExtensionOptions, BlastExtensionOptionsFree);
DECLARE_AUTO_CLASS_WRAPPER(BlastExtensionParameters,
                           BlastExtensionParametersFree);

DECLARE_AUTO_CLASS_WRAPPER(BlastHitSavingOptions, BlastHitSavingOptionsFree);
DECLARE_AUTO_CLASS_WRAPPER(BlastHitSavingParameters,
                           BlastHitSavingParametersFree);

DECLARE_AUTO_CLASS_WRAPPER(PSIBlastOptions, sfree);
DECLARE_AUTO_CLASS_WRAPPER(BlastDatabaseOptions, BlastDatabaseOptionsFree);

DECLARE_AUTO_CLASS_WRAPPER(BlastScoreBlk, BlastScoreBlkFree);
DECLARE_AUTO_CLASS_WRAPPER(BlastScoringOptions, BlastScoringOptionsFree);

DECLARE_AUTO_CLASS_WRAPPER(BlastEffectiveLengthsOptions,
                           BlastEffectiveLengthsOptionsFree);

DECLARE_AUTO_CLASS_WRAPPER(BlastGapAlignStruct, BLAST_GapAlignStructFree);
DECLARE_AUTO_CLASS_WRAPPER(BlastResults, BLAST_ResultsFree);

END_SCOPE(blast)
END_NCBI_SCOPE

/*
* ===========================================================================
*
* $Log$
* Revision 1.26  2003/11/26 18:22:13  camacho
* +Blast Option Handle classes
*
* Revision 1.25  2003/10/07 17:27:37  dondosha
* Lower case mask removed from options, added to the SSeqLoc structure
*
* Revision 1.24  2003/09/11 17:44:39  camacho
* Changed CBlastOption -> CBlastOptions
*
* Revision 1.23  2003/09/10 20:00:49  dondosha
* BlastLookupTableDestruct renamed to LookupTableWrapFree
*
* Revision 1.22  2003/08/27 21:27:58  camacho
* Fix to previous commit
*
* Revision 1.21  2003/08/27 18:40:02  camacho
* Change free function for blast db options struct
*
* Revision 1.20  2003/08/20 15:23:47  ucko
* DECLARE_AUTO_CLASS_WRAPPER: Remove occurrences of ## adjacent to punctuation.
*
* Revision 1.19  2003/08/20 14:45:26  dondosha
* All references to CDisplaySeqalign moved to blast_format.hpp
*
* Revision 1.18  2003/08/19 22:11:49  dondosha
* Major types definitions moved to blast_types.h
*
* Revision 1.17  2003/08/19 20:22:05  dondosha
* EProgram definition moved from CBlastOptions clase to blast scope
*
* Revision 1.16  2003/08/19 13:45:21  dicuccio
* Removed 'USING_SCOPE(objects)'.  Changed #include guards to be standards
* compliant.  Added 'objects::' where necessary.
*
* Revision 1.15  2003/08/18 22:17:52  camacho
* Renaming of SSeqLoc members
*
* Revision 1.14  2003/08/18 20:58:56  camacho
* Added blast namespace, removed *__.hpp includes
*
* Revision 1.13  2003/08/18 17:07:41  camacho
* Introduce new SSeqLoc structure (replaces pair<CSeq_loc, CScope>).
* Change in function to read seqlocs from files.
*
* Revision 1.12  2003/08/14 19:08:45  dondosha
* Use CRef instead of pointer to CSeq_loc in the TSeqLoc type definition
*
* Revision 1.11  2003/08/12 19:17:58  dondosha
* Added TSeqLocVector typedef so it can be used from all sources; removed scope argument from functions
*
* Revision 1.10  2003/08/11 19:55:04  camacho
* Early commit to support query concatenation and the use of multiple scopes.
* Compiles, but still needs work.
*
* Revision 1.9  2003/08/11 17:12:10  dondosha
* Do not use CConstRef as argument to CSeqLoc2BlastMask
*
* Revision 1.8  2003/08/11 16:09:53  dondosha
* Pass CConstRef by value in CSeqLoc2BlastMask
*
* Revision 1.7  2003/08/11 15:23:23  dondosha
* Renamed conversion functions between BlastMask and CSeqLoc; added algo/blast/core to headers from core BLAST library
*
* Revision 1.6  2003/08/11 13:58:51  dicuccio
* Added export specifiers.  Fixed problem with unimplemented private copy ctor
* (truly make unimplemented)
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
