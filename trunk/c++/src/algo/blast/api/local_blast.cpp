#ifndef SKIP_DOXYGEN_PROCESSING
static char const rcsid[] =
    "$Id$";
#endif /* SKIP_DOXYGEN_PROCESSING */
/* ===========================================================================
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
 */

/** @file local_blast.cpp
 * NOTE: This file contains work in progress and the APIs are likely to change,
 * please do not rely on them until this notice is removed.
 */

#include <ncbi_pch.hpp>
#include <algo/blast/api/local_blast.hpp>
#include <algo/blast/api/uniform_search.hpp>
#include <algo/blast/api/blast_seqinfosrc.hpp>
#include "blast_aux_priv.hpp"
#include <objects/scoremat/PssmWithParameters.hpp>

/** @addtogroup AlgoBlast
 *
 * @{
 */

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
BEGIN_SCOPE(blast)

CLocalBlast::CLocalBlast(CRef<IQueryFactory> qf,
                         CRef<CBlastOptionsHandle> opts_handle,
                         //CConstRef<CBlastOptionsHandle> opts_handle,
                         const CSearchDatabase& dbinfo)
: m_QueryFactory    (qf),
  m_Opts            (const_cast<CBlastOptions*>(&opts_handle->GetOptions())),
  m_InternalData    (0),
  m_PrelimSearch    (new CBlastPrelimSearch(qf, m_Opts, dbinfo)),
  m_TbackSearch     (0)
{}

CLocalBlast::CLocalBlast(CRef<IQueryFactory> qf,
                         CRef<CBlastOptionsHandle> opts_handle,
                         //CConstRef<CBlastOptionsHandle> opts_handle,
                         IBlastSeqSrcAdapter& db)
: m_QueryFactory    (qf),
  m_Opts            (const_cast<CBlastOptions*>(&opts_handle->GetOptions())),
  m_InternalData    (0),
  m_PrelimSearch    (new CBlastPrelimSearch(qf, m_Opts, db)),
  m_TbackSearch     (0)
{}

CLocalBlast::CLocalBlast(CRef<IQueryFactory> qf,
                         CRef<CBlastOptionsHandle> opts_handle,
                         //CConstRef<CBlastOptionsHandle> opts_handle,
                         BlastSeqSrc* seqsrc)
: m_QueryFactory    (qf),
  m_Opts            (const_cast<CBlastOptions*>(&opts_handle->GetOptions())),
  m_InternalData    (0),
  m_PrelimSearch    (new CBlastPrelimSearch(qf, m_Opts, seqsrc,
                                            CRef<CPssmWithParameters>())),
  m_TbackSearch     (0)
{}

CSearchResultSet
CLocalBlast::Run()
{
    ASSERT(m_QueryFactory);
    ASSERT(m_PrelimSearch);
    ASSERT(m_Opts);

    // Note: we need to pass the search messages ...
    // filtered query regions should be masked in the BLAST_SequenceBlk
    // already.
    
    m_InternalData = m_PrelimSearch->Run();
    ASSERT(m_InternalData);

    TSearchMessages search_msgs = m_PrelimSearch->GetSearchMessages();

    auto_ptr<IBlastSeqInfoSrc>
        seqinfo_src(InitSeqInfoSrc(m_InternalData->m_SeqSrc->GetPointer()));

    m_TbackSearch.Reset(new CBlastTracebackSearch(m_QueryFactory,
                                                  m_InternalData,
                                                  *m_Opts,
                                                  *seqinfo_src,
                                                  search_msgs));
    CSearchResultSet retval = m_TbackSearch->Run();

    // Add the filtered query regions, if any
    TSeqLocInfoVector masks = m_PrelimSearch->GetFilteredQueryRegions();
    if ( !masks.empty() ) {
        _ASSERT(masks.size() == (size_t)retval.GetNumResults());
        for (int i = 0; i < retval.GetNumResults(); i++) {
            retval[i].SetMaskedQueryRegions(masks[i]);
        }
    }
    return retval;
}

END_SCOPE(blast)
END_NCBI_SCOPE

/* @} */
