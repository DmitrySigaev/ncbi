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
                         const CSearchDatabase& dbinfo)
: m_QueryFactory    (qf),
  m_Opts            (const_cast<CBlastOptions*>(&opts_handle->GetOptions())),
  m_InternalData    (0),
  m_PrelimSearch    (new CBlastPrelimSearch(qf, m_Opts, dbinfo)),
  m_TbackSearch     (0)
{}

CLocalBlast::CLocalBlast(CRef<IQueryFactory> qf,
                         CRef<CBlastOptionsHandle> opts_handle,
                         CRef<CLocalDbAdapter> db)
: m_QueryFactory    (qf),
  m_Opts            (const_cast<CBlastOptions*>(&opts_handle->GetOptions())),
  m_InternalData    (0),
  m_PrelimSearch    (new CBlastPrelimSearch(qf, m_Opts, db)),
  m_TbackSearch     (0),
  m_LocalDbAdapter  (db.GetNonNullPointer())
{}

CLocalBlast::CLocalBlast(CRef<IQueryFactory> qf,
                         CRef<CBlastOptionsHandle> opts_handle,
                         BlastSeqSrc* seqsrc)
: m_QueryFactory    (qf),
  m_Opts            (const_cast<CBlastOptions*>(&opts_handle->GetOptions())),
  m_InternalData    (0),
  m_PrelimSearch    (new CBlastPrelimSearch(qf, m_Opts, seqsrc,
                                            CRef<CPssmWithParameters>())),
  m_TbackSearch     (0)
{}

CLocalBlast::CLocalBlast(CRef<IQueryFactory> qf,
                         CRef<CBlastOptionsHandle> opts_handle,
                         BlastSeqSrc* seqsrc,
                         CRef<IBlastSeqInfoSrc> seqInfoSrc)
: m_QueryFactory    (qf),
  m_Opts            (const_cast<CBlastOptions*>(&opts_handle->GetOptions())),
  m_InternalData    (0),
  m_PrelimSearch    (new CBlastPrelimSearch(qf, m_Opts, seqsrc,
                                            CRef<CPssmWithParameters>())),
  m_TbackSearch     (0),
  m_SeqInfoSrc      (seqInfoSrc)
{}
    
CRef<CSearchResultSet>
CLocalBlast::Run()
{
    _ASSERT(m_QueryFactory);
    _ASSERT(m_PrelimSearch);
    _ASSERT(m_Opts);
    
    // Note: we need to pass the search messages ...
    // filtered query regions should be masked in the BLAST_SequenceBlk
    // already.
    
    m_PrelimSearch->SetNumberOfThreads(GetNumberOfThreads());
    m_InternalData = m_PrelimSearch->Run();
    _ASSERT(m_InternalData);
    
    TSearchMessages search_msgs = m_PrelimSearch->GetSearchMessages();
    
    CRef<IBlastSeqInfoSrc> seqinfo_src;
    
    if (m_SeqInfoSrc.NotEmpty())
    {
        // Use the SeqInfoSrc provided by the user during construction
        seqinfo_src = m_SeqInfoSrc;
    }
    else if (m_LocalDbAdapter.NotEmpty()) {
        // This path is preferred because it preserves the GI list
        // limitation if there is one.  DBs with both internal OID
        // filtering and user GI list filtering will not do complete
        // filtering during the traceback stage, which can cause
        // 'Unknown defline' errors during formatting.
        
        seqinfo_src.Reset(m_LocalDbAdapter->MakeSeqInfoSrc());
    } else {
        seqinfo_src.Reset(InitSeqInfoSrc(m_InternalData->m_SeqSrc->GetPointer()));
    }
    
    m_TbackSearch.Reset(new CBlastTracebackSearch(m_QueryFactory,
                                                  m_InternalData,
                                                  *m_Opts,
                                                  *seqinfo_src,
                                                  search_msgs));
    if (m_LocalDbAdapter.NotEmpty() && !m_LocalDbAdapter->IsBlastDb()) {
        m_TbackSearch->SetResultType(eSequenceComparison);
    }
    CRef<CSearchResultSet> retval = m_TbackSearch->Run();
    retval->SetFilteredQueryRegions(m_PrelimSearch->GetFilteredQueryRegions());
    return retval;
}

END_SCOPE(blast)
END_NCBI_SCOPE

/* @} */
