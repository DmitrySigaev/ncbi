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

/** @file setup_factory.cpp
 * NOTE: This file contains work in progress and the APIs are likely to change,
 * please do not rely on them until this notice is removed.
 */

#include <ncbi_pch.hpp>
#include <algo/blast/api/setup_factory.hpp>
#include <algo/blast/api/uniform_search.hpp>    // for CSearchDatabase
#include <algo/blast/api/blast_options.hpp>
#include <algo/blast/api/seqsrc_seqdb.hpp>      // for SeqDbBlastSeqSrcInit
#include <algo/blast/api/blast_mtlock.hpp>      // for Blast_DiagnosticsInitMT
#include <algo/blast/api/blast_dbindex.hpp>

#include "blast_aux_priv.hpp"
#include "blast_memento_priv.hpp"
#include "blast_setup.hpp"

// SeqAlignVector building
#include "blast_seqalign.hpp"
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seqalign/seqalign__.hpp>
#include <serial/iterator.hpp>

// CORE BLAST includes
#include <algo/blast/core/blast_setup.h>
#include <algo/blast/core/hspstream_collector.h>

/** @addtogroup AlgoBlast
 *
 * @{
 */

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
BEGIN_SCOPE(blast)

CRef<CBlastRPSInfo>
CSetupFactory::CreateRpsStructures(const string& rps_dbname,
                                   CRef<CBlastOptions> options)
{
    CRef<CBlastRPSInfo> retval(new CBlastRPSInfo(rps_dbname));
    options->SetMatrixName(retval->GetMatrixName());
    options->SetGapOpeningCost(retval->GetGapOpeningCost());
    options->SetGapExtensionCost(retval->GetGapExtensionCost());
    return retval;
}

static
CRef<CPacked_seqint> s_LocalQueryData2Packed_seqint(ILocalQueryData& query_data)
{
    const int kNumQueries = query_data.GetNumQueries();
    if (kNumQueries == 0) {
        return CRef<CPacked_seqint>();
    }

    CRef<CPacked_seqint> retval(new CPacked_seqint);
    for (int i = 0; i < kNumQueries; i++) {

        CConstRef<CSeq_id> id(query_data.GetSeq_loc(i)->GetId());
        if (query_data.IsValidQuery(i)) {

            if (query_data.GetSeq_loc(i)->IsInt()) {
                retval->AddInterval(query_data.GetSeq_loc(i)->GetInt());
            } else if (id.NotEmpty()) {
                retval->AddInterval(*id, 0, query_data.GetSeqLength(i));
            }

        } else {
            retval->AddInterval(*id, 0, 0);
        }

    }

    return retval;
}

BlastScoreBlk*
CSetupFactory::CreateScoreBlock(const CBlastOptionsMemento* opts_memento,
                                CRef<ILocalQueryData> query_data,
                                BlastSeqLoc** lookup_segments,
                                TSearchMessages& search_messages,
                                TSeqLocInfoVector* masked_query_regions,
                                const CBlastRPSInfo* rps_info)
{
    _ASSERT(opts_memento);

    double rps_scale_factor(1.0);

    if (rps_info) {
        _ASSERT(Blast_ProgramIsRpsBlast(opts_memento->m_ProgramType));
        rps_scale_factor = rps_info->GetScalingFactor();
    }

    CBlast_Message blast_msg;
    CBlastMaskLoc core_masked_query_regions;

    BlastQueryInfo* query_info = query_data->GetQueryInfo();
    BLAST_SequenceBlk* queries = query_data->GetSequenceBlk();

    BlastScoreBlk* retval(0);
    Int2 status = BLAST_MainSetUp(opts_memento->m_ProgramType,
                                  opts_memento->m_QueryOpts,
                                  opts_memento->m_ScoringOpts,
                                  queries,
                                  query_info,
                                  rps_scale_factor,
                                  lookup_segments,
                                  &core_masked_query_regions,
                                  &retval,
                                  &blast_msg,
                                  &BlastFindMatrixPath);

    Blast_Message2TSearchMessages(blast_msg.Get(), query_info, search_messages);
    if (status != 0) {
        string msg;
        if (search_messages.HasMessages()) {
            msg = search_messages.ToString();
        } else {
            msg = "BLAST_MainSetUp failed (" + NStr::IntToString(status) + 
            " error code)";
        }
        NCBI_THROW(CBlastException, eCoreBlastError, msg);
    }

    if (masked_query_regions) {
        CRef<CPacked_seqint> query_locations = 
            s_LocalQueryData2Packed_seqint(*query_data);
        Blast_GetSeqLocInfoVector(opts_memento->m_ProgramType,
                                  *query_locations,
                                  core_masked_query_regions,
                                  *masked_query_regions);
    }

    return retval;
}

LookupTableWrap*
CSetupFactory::CreateLookupTable(CRef<ILocalQueryData> query_data,
                                 const CBlastOptionsMemento* opts_memento,
                                 BlastScoreBlk* score_blk,
                                 BlastSeqLoc* lookup_segments,
                                 const CBlastRPSInfo* rps_info,
                                 BlastSeqSrc* seqsrc)
{
    BLAST_SequenceBlk* queries = query_data->GetSequenceBlk();
    CBlast_Message blast_msg;
    LookupTableWrap* retval(0);

    Int2 status = LookupTableWrapInit(queries,
                                      opts_memento->m_LutOpts,
                                      lookup_segments,
                                      score_blk,
                                      &retval,
                                      rps_info ? (*rps_info)() : 0,
                                      &blast_msg);
    if (status != 0) {
         TSearchMessages search_messages;
         Blast_Message2TSearchMessages(blast_msg.Get(), 
                                           query_data->GetQueryInfo(), 
                                           search_messages);
         string msg;
         if (search_messages.HasMessages()) {
              msg = search_messages.ToString();
         } else {
              msg = "LookupTableWrapInit failed (" + 
                   NStr::IntToString(status) + " error code)";
         }
         NCBI_THROW(CBlastException, eCoreBlastError, msg);
    }

    // For PHI BLAST, save information about pattern occurrences in query in
    // the BlastQueryInfo structure
    if (Blast_ProgramIsPhiBlast(opts_memento->m_ProgramType)) {
        SPHIPatternSearchBlk* phi_lookup_table
            = (SPHIPatternSearchBlk*) retval->lut;
        status = Blast_SetPHIPatternInfo(opts_memento->m_ProgramType,
                                phi_lookup_table,
                                queries,
                                lookup_segments,
                                query_data->GetQueryInfo(), 
                                &blast_msg);
        if (status != 0) {  
             TSearchMessages search_messages;
             Blast_Message2TSearchMessages(blast_msg.Get(), 
                                           query_data->GetQueryInfo(), 
                                           search_messages);
             string msg;
             if (search_messages.HasMessages()) {
                 msg = search_messages.ToString();
             } else {
                 msg = "Blast_SetPHIPatternInfo failed (" + 
                     NStr::IntToString(status) + " error code)";
             }
             NCBI_THROW(CBlastException, eCoreBlastError, msg);
        }
    }

    if (seqsrc) {
        GetDbIndexPreSearchFn()(
                seqsrc,
                retval,
                queries,
                lookup_segments,
                opts_memento->m_LutOpts,
                opts_memento->m_InitWordOpts
        );
    }

    return retval;
}

BlastDiagnostics*
CSetupFactory::CreateDiagnosticsStructure()
{
    return Blast_DiagnosticsInit();
}

BlastDiagnostics*
CSetupFactory::CreateDiagnosticsStructureMT()
{
    return Blast_DiagnosticsInitMT(Blast_CMT_LOCKInit());
}

BlastHSPStream*
CSetupFactory::CreateHspStream(const CBlastOptionsMemento* opts_memento,
                               size_t number_of_queries)
{
    return x_CreateHspStream(opts_memento, number_of_queries, false);
}

BlastHSPStream*
CSetupFactory::CreateHspStreamMT(const CBlastOptionsMemento* opts_memento,
                                 size_t number_of_queries)
{
    return x_CreateHspStream(opts_memento, number_of_queries, true);
}

BlastHSPStream*
CSetupFactory::x_CreateHspStream(const CBlastOptionsMemento* opts_memento,
                                 size_t number_of_queries,
                                 bool is_multi_threaded)
{
    _ASSERT(opts_memento);

    SBlastHitsParameters* blast_hit_params(0);
    SBlastHitsParametersNew(opts_memento->m_HitSaveOpts,
                            opts_memento->m_ExtnOpts,
                            opts_memento->m_ScoringOpts,
                            &blast_hit_params);

    if (is_multi_threaded) {
        return Blast_HSPListCollectorInitMT(opts_memento->m_ProgramType,
                                            blast_hit_params,
                                            number_of_queries,
                                            true,
                                            Blast_CMT_LOCKInit());
    } else {
        return Blast_HSPListCollectorInit(opts_memento->m_ProgramType,
                                          blast_hit_params,
                                          number_of_queries,
                                          true);
    }
}

BlastSeqSrc*
CSetupFactory::CreateBlastSeqSrc(const CSearchDatabase& db)
{
    bool prot = (db.GetMoleculeType() == CSearchDatabase::eBlastDbIsProtein);

    BlastSeqSrc* retval = SeqDbBlastSeqSrcInit(db.GetDatabaseName(), prot);
    char* error_str = BlastSeqSrcGetInitError(retval);
    if (error_str) {
        string msg(error_str);
        sfree(error_str);
        retval = BlastSeqSrcFree(retval);
        NCBI_THROW(CBlastException, eSeqSrcInit, msg);
    }
    return retval;
}

BlastSeqSrc*
CSetupFactory::CreateBlastSeqSrc(CSeqDB * db)
{
    BlastSeqSrc* retval = SeqDbBlastSeqSrcInit(db);
    char* error_str = BlastSeqSrcGetInitError(retval);
    if (error_str) {
        string msg(error_str);
        sfree(error_str);
        retval = BlastSeqSrcFree(retval);
        NCBI_THROW(CBlastException, eSeqSrcInit, msg);
    }
    return retval;
}

void
CSetupFactory::InitializeMegablastDbIndex(BlastSeqSrc* seqsrc,
                                          CRef<CBlastOptions> options)
{
    _ASSERT(options->GetUseIndex());

    if (options->GetMBIndexLoaded()) {
        return;
    }

    _ASSERT(seqsrc);

    BlastSeqSrc * new_seqsrc = CloneSeqSrcInit( seqsrc );

    if( new_seqsrc == 0 ) {
        ERR_POST( Error << "Allocation of new BlastSeqSrc structure failed."
                          << " Database index will not be used." );
        options->SetUseIndex( false );
        return;
    }

    BlastSeqSrc * ind_seqsrc = DbIndexSeqSrcInit(
            options->GetIndexName(), new_seqsrc );

    if( ind_seqsrc == 0 ) {
        ERR_POST( Error << "Allocation of BlastSeqSrc structure for index failed."
                          << " Database index will not be used." );
        free( new_seqsrc );
        options->SetUseIndex( false );
        return;
    }

    CloneSeqSrc( seqsrc, ind_seqsrc );
    free( ind_seqsrc );
    options->SetMBIndexLoaded();
}

SInternalData::SInternalData()
{
    m_Queries = 0;
    m_QueryInfo = 0;
}

SDatabaseScanData::SDatabaseScanData()
   : kNoPhiBlastPattern(-1)
{
   m_NumPatOccurInDB = kNoPhiBlastPattern; 
}

END_SCOPE(blast)
END_NCBI_SCOPE

/* @} */

