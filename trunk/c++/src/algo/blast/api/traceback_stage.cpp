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

/** @file traceback_stage.cpp
 * NOTE: This file contains work in progress and the APIs are likely to change,
 * please do not rely on them until this notice is removed.
 */

#include <ncbi_pch.hpp>
#include <algo/blast/api/traceback_stage.hpp>
#include <algo/blast/api/uniform_search.hpp>    // for CSearchDatabase
#include <algo/blast/api/seqinfosrc_seqdb.hpp>  // for CSeqDbSeqInfoSrc

#include "blast_memento_priv.hpp"
#include "blast_seqsrc_adapter_priv.hpp"
#include "blast_seqalign.hpp"
#include "blast_aux_priv.hpp"

// CORE BLAST includes
#include <algo/blast/core/blast_setup.h>
#include <algo/blast/core/blast_traceback.h>

/** @addtogroup AlgoBlast
 *
 * @{
 */

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
BEGIN_SCOPE(blast)

CBlastTracebackSearch::CBlastTracebackSearch(CRef<IQueryFactory>     qf,
                                             CRef<CBlastOptions>     opts,
                                             CSearchDatabase const & db,
                                             CRef<TBlastHSPStream>   hsps)
    : m_QueryFactory (qf),
      m_Options      (opts),
      m_InternalData (new SInternalData),
      m_OptsMemento  (0),
      m_HspResults   (0),
      m_SeqInfoSrc   (0),
      m_OwnSeqInfoSrc(false)
{
    x_Init(qf, opts, db.GetDatabaseName());
    BlastSeqSrc* seqsrc = CSetupFactory::CreateBlastSeqSrc(db);
    m_SeqInfoSrc = InitSeqInfoSrc(seqsrc);
    m_OwnSeqInfoSrc = true;
    m_InternalData->m_SeqSrc.Reset(new TBlastSeqSrc(seqsrc, BlastSeqSrcFree));
    m_InternalData->m_HspStream.Reset(hsps);
}

CBlastTracebackSearch::CBlastTracebackSearch(CRef<IQueryFactory>    qf,
                                             CRef<CBlastOptions>    opts,
                                             IBlastSeqSrcAdapter  & ssa,
                                             CRef<TBlastHSPStream>  hsps)
    : m_QueryFactory (qf),
      m_Options      (opts),
      m_InternalData (new SInternalData),
      m_OptsMemento  (0),
      m_HspResults   (0),
      m_SeqInfoSrc   (0),
      m_OwnSeqInfoSrc(false)
{
    BlastSeqSrc* seqsrc = ssa.GetBlastSeqSrc();
    m_SeqInfoSrc = InitSeqInfoSrc(seqsrc);
    m_OwnSeqInfoSrc = true;
    x_Init(qf, opts, BlastSeqSrcGetName(seqsrc));
    m_InternalData->m_SeqSrc.Reset(new TBlastSeqSrc(seqsrc, 0));
    m_InternalData->m_HspStream.Reset(hsps);
}

CBlastTracebackSearch::CBlastTracebackSearch(CRef<IQueryFactory>   qf,
                                             CRef<CBlastOptions>   opts,
                                             BlastSeqSrc         * seqsrc,
                                             CRef<TBlastHSPStream> hsps)
    : m_QueryFactory (qf),
      m_Options      (opts),
      m_InternalData (new SInternalData),
      m_OptsMemento  (0),
      m_HspResults   (0),
      m_SeqInfoSrc   (InitSeqInfoSrc(seqsrc)),
      m_OwnSeqInfoSrc(true)
{
    x_Init(qf, opts, BlastSeqSrcGetName(seqsrc));
    m_InternalData->m_SeqSrc.Reset(new TBlastSeqSrc(seqsrc, 0));
    m_InternalData->m_HspStream.Reset(hsps);
}

CBlastTracebackSearch::CBlastTracebackSearch(CRef<IQueryFactory> qf,
                                             CRef<SInternalData> internal_data, 
                                             const CBlastOptions& opts, 
                                             const IBlastSeqInfoSrc& seqinfosrc)
    : m_QueryFactory (qf),
      m_Options      (const_cast<CBlastOptions*>(&opts)),
      m_InternalData (internal_data),
      m_OptsMemento  (opts.CreateSnapshot()),
      m_HspResults   (0),
      m_SeqInfoSrc   (const_cast<IBlastSeqInfoSrc*>(&seqinfosrc)),
      m_OwnSeqInfoSrc(false)
{}

CBlastTracebackSearch::~CBlastTracebackSearch()
{
    delete m_OptsMemento;
    if (m_OwnSeqInfoSrc) {
        delete m_SeqInfoSrc;
    }
}

void
CBlastTracebackSearch::x_Init(CRef<IQueryFactory> qf,
                              CRef<CBlastOptions> opts,
                              const string& dbname)
{
    opts->Validate();

    // 1. Initialize the query data (borrow it from the factory)
    CRef<ILocalQueryData> query_data(qf->MakeLocalQueryData(&*opts));
    m_InternalData->m_Queries = query_data->GetSequenceBlk();
    m_InternalData->m_QueryInfo = query_data->GetQueryInfo();

    // 2. Take care of any rps information
    if (Blast_ProgramIsRpsBlast(opts->GetProgramType())) {
        m_InternalData->m_RpsData =
            CSetupFactory::CreateRpsStructures(dbname, opts);
    }

    // 3. Create the options memento
    m_OptsMemento = opts->CreateSnapshot();

    const bool kIsPhiBlast =
		Blast_ProgramIsPhiBlast(m_OptsMemento->m_ProgramType) ? true : false;

    // 4. Create the BlastScoreBlk
    BlastSeqLoc* lookup_segments(0);
    BlastScoreBlk* sbp =
        CSetupFactory::CreateScoreBlock(m_OptsMemento, query_data, 
                                        kIsPhiBlast ? &lookup_segments : 0, 
                                        /* FIXME: masked locations */ 0,
                                        m_InternalData->m_RpsData);
    m_InternalData->m_ScoreBlk.Reset
        (new TBlastScoreBlk(sbp, BlastScoreBlkFree));

    // N.B.: Only PHI BLAST pseudo lookup table is necessary
    if (kIsPhiBlast) {
        ASSERT(lookup_segments);
        ASSERT(m_InternalData->m_RpsData == NULL);
        LookupTableWrap* lut =
            CSetupFactory::CreateLookupTable(query_data, m_OptsMemento,
                 m_InternalData->m_ScoreBlk->GetPointer(), lookup_segments);
        m_InternalData->m_LookupTable.Reset
            (new TLookupTableWrap(lut, LookupTableWrapFree));
        lookup_segments = BlastSeqLocFree(lookup_segments);
    }

    // 5. Create diagnostics
    BlastDiagnostics* diags = CSetupFactory::CreateDiagnosticsStructure();
    m_InternalData->m_Diagnostics.Reset
        (new TBlastDiagnostics(diags, Blast_DiagnosticsFree));

    // 6. Create HSP stream
    BlastHSPStream* hsp_stream =
        CSetupFactory::CreateHspStream(m_OptsMemento, 
                                       query_data->GetNumQueries());
    m_InternalData->m_HspStream.Reset
        (new TBlastHSPStream(hsp_stream, BlastHSPStreamFree));
}

CSearchResultSet
CBlastTracebackSearch::Run()
{
    ASSERT(m_OptsMemento);
    SPHIPatternSearchBlk* phi_lookup_table(0);

    // For PHI BLAST we need to pass the pattern search items structure to the
    // traceback code
    if (Blast_ProgramIsPhiBlast(m_OptsMemento->m_ProgramType)) {
        // The following call modifies the effective search space fields in the
        // BlastQueryInfo structure
        PHIPatternSpaceCalc(m_InternalData->m_QueryInfo,
                            m_InternalData->m_Diagnostics->GetPointer());
        ASSERT(m_InternalData->m_LookupTable);
        phi_lookup_table = (SPHIPatternSearchBlk*) 
            m_InternalData->m_LookupTable->GetPointer()->lut;
    }
    
    BlastHSPResults * hsp_results(0);
    int status =
        Blast_RunTracebackSearch(m_OptsMemento->m_ProgramType,
                                 m_InternalData->m_Queries,
                                 m_InternalData->m_QueryInfo,
                                 m_InternalData->m_SeqSrc->GetPointer(),
                                 m_OptsMemento->m_ScoringOpts,
                                 m_OptsMemento->m_ExtnOpts,
                                 m_OptsMemento->m_HitSaveOpts,
                                 m_OptsMemento->m_EffLenOpts,
                                 m_OptsMemento->m_DbOpts,
                                 m_OptsMemento->m_PSIBlastOpts,
                                 m_InternalData->m_ScoreBlk->GetPointer(),
                                 m_InternalData->m_HspStream->GetPointer(),
                                 m_InternalData->m_RpsData ?
                                 (*m_InternalData->m_RpsData)() : 0,
                                 phi_lookup_table,
                                 & hsp_results);
    
    if (status) {
        NCBI_THROW(CBlastException, eCoreBlastError, "Traceback failed"); 
    }
    
    m_HspResults.Reset(WrapStruct(hsp_results, Blast_HSPResultsFree));
    
    ASSERT(m_SeqInfoSrc);
    ASSERT(m_QueryFactory);

    TSeqAlignVector aligns =
        LocalBlastResults2SeqAlign(hsp_results,
           *m_QueryFactory->MakeLocalQueryData(m_Options),
           *m_SeqInfoSrc,
           m_OptsMemento->m_ProgramType,
           m_Options->GetGappedMode(),
           m_Options->GetOutOfFrameMode());
    
    // The code should probably capture and return errors here; the
    // traceback stage does not seem to produce messages, but the
    // preliminary stage does; they should be saved and returned here
    // if they have not been returned or reported yet.
    
    CSearchResultSet::TMessages empty_msgs(aligns.size());
    
    return CSearchResultSet(aligns, empty_msgs);
}

END_SCOPE(blast)
END_NCBI_SCOPE

/* @} */

