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

#ifndef BLAST_AUX__HPP
#define BLAST_AUX__HPP

#include <corelib/ncbistd.hpp>
#include <corelib/ddumpable.hpp>

#include <objects/seqloc/Seq_loc.hpp>

// NewBlast includes
#include <blast_def.h>
#include <blast_options.h>
#include <blast_hits.h>
#include <blast_filter.h>       // Needed for BlastMask & BlastSeqLoc
#include <blast_util.h>

USING_NCBI_SCOPE;
USING_SCOPE(objects);

BEGIN_NCBI_SCOPE

/** Converts a CSeq_loc into a BlastMaskPtr structure used in NewBlast
 * @param sl CSeq_loc to convert [in]
 * @param index Number of frame/query number? this CSeq_loc applies to [in]
 * @return Linked list of BlastMask structures
 */
BlastMaskPtr
BLASTSeqLoc2BlastMask(const CSeq_loc& sl, int index);

CRef<CSeq_loc>
BLASTBlastMask2SeqLoc(BlastMaskPtr mask);



/** Declares class to handle deallocating of the structure using the appropriate
 * function
 */

#define DECLARE_AUTO_CLASS_WRAPPER(struct_name, free_func) \
class C##struct_name : public CDebugDumpable \
{ \
public: \
    C##struct_name() : m_Ptr(NULL) {} \
    C##struct_name(struct_name p) : m_Ptr(p) {} \
    void Reset(struct_name p) { if (m_Ptr) { free_func(m_Ptr); } m_Ptr = p; } \
    ~C##struct_name() { if (m_Ptr) { free_func(m_Ptr); m_Ptr = NULL; } } \
    operator struct_name() { return m_Ptr; } \
    operator struct_name() const { return m_Ptr; } \
    struct_name operator->() { return m_Ptr; } \
    struct_name operator->() const { return m_Ptr; } \
    struct_name* operator&() { return &m_Ptr; } \
    void DebugDump(CDebugDumpContext ddc, unsigned int depth) const; \
private: \
    struct_name m_Ptr; \
}

DECLARE_AUTO_CLASS_WRAPPER(BLAST_SequenceBlkPtr, BlastSequenceBlkFree);

DECLARE_AUTO_CLASS_WRAPPER(BlastQueryInfoPtr, BlastQueryInfoFree);
DECLARE_AUTO_CLASS_WRAPPER(QuerySetUpOptionsPtr, BlastQuerySetUpOptionsFree);

DECLARE_AUTO_CLASS_WRAPPER(LookupTableOptionsPtr, LookupTableOptionsFree);
DECLARE_AUTO_CLASS_WRAPPER(LookupTableWrapPtr, BlastLookupTableDestruct);

DECLARE_AUTO_CLASS_WRAPPER(BlastInitialWordOptionsPtr,
        BlastInitialWordOptionsFree);
DECLARE_AUTO_CLASS_WRAPPER(BlastInitialWordParametersPtr,
        BlastInitialWordParametersFree);

DECLARE_AUTO_CLASS_WRAPPER(BLAST_ExtendWordPtr, BlastExtendWordFree);
DECLARE_AUTO_CLASS_WRAPPER(BlastExtensionOptionsPtr, BlastExtensionOptionsFree);
DECLARE_AUTO_CLASS_WRAPPER(BlastExtensionParametersPtr,
        BlastExtensionParametersFree);

DECLARE_AUTO_CLASS_WRAPPER(BlastHitSavingOptionsPtr, BlastHitSavingOptionsFree);
DECLARE_AUTO_CLASS_WRAPPER(BlastHitSavingParametersPtr,
        BlastHitSavingParametersFree);

DECLARE_AUTO_CLASS_WRAPPER(PSIBlastOptionsPtr, MemFree);
DECLARE_AUTO_CLASS_WRAPPER(BlastDatabaseOptionsPtr, MemFree);

DECLARE_AUTO_CLASS_WRAPPER(BLAST_ScoreBlkPtr, BLAST_ScoreBlkDestruct);
DECLARE_AUTO_CLASS_WRAPPER(BlastScoringOptionsPtr, BlastScoringOptionsFree);

DECLARE_AUTO_CLASS_WRAPPER(BlastEffectiveLengthsOptionsPtr,
        BlastEffectiveLengthsOptionsFree);

DECLARE_AUTO_CLASS_WRAPPER(BlastGapAlignStructPtr, BLAST_GapAlignStructFree);
DECLARE_AUTO_CLASS_WRAPPER(BlastResultsPtr, BLAST_ResultsFree);

END_NCBI_SCOPE

/*
* ===========================================================================
*
* $Log$
* Revision 1.2  2003/07/14 22:17:17  camacho
* Convert CSeq_loc to BlastMaskPtr
*
* Revision 1.1  2003/07/10 18:34:19  camacho
* Initial revision
*
*
* ===========================================================================
*/

#endif  /* BLAST_AUX__HPP */
