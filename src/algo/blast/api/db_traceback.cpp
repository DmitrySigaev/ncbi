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
 * ===========================================================================
 */

/// @file db_traceback.cpp
/// Implementation of a CDbBlastTraceback class for running BLAST traceback 
/// only, given the precomputed preliminary HSP results.

#include <ncbi_pch.hpp>
#include <algo/blast/api/db_traceback.hpp>
#include "blast_setup.hpp"
#include <algo/blast/core/blast_traceback.h>
#include <algo/blast/core/blast_setup.h>
#include <algo/blast/core/blast_message.h>

/** @addtogroup AlgoBlast
 *
 * @{
 */

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
BEGIN_SCOPE(blast)

CDbBlastTraceback::CDbBlastTraceback(const TSeqLocVector& queries, 
                       BlastSeqSrc* seq_src, EProgram p, 
                       BlastHSPStream* hsp_stream)
    : CDbBlast(queries, seq_src, p)
{
    m_ipHspStream = hsp_stream;
}

CDbBlastTraceback::CDbBlastTraceback(const TSeqLocVector& queries, 
                       BlastSeqSrc* seq_src, CBlastOptionsHandle& opts,
                       BlastHSPStream* hsp_stream)
    : CDbBlast(queries, seq_src, opts)
{
    m_ipHspStream = hsp_stream;
}

int CDbBlastTraceback::SetupSearch()
{
    int status = 0;
    EProgram x_eProgram = GetOptionsHandle().GetOptions().GetProgram();
    
    if ( !m_ibQuerySetUpDone ) {
        double scale_factor;

        x_ResetQueryDs();
        
        SetupQueryInfo(GetQueries(), GetOptionsHandle().GetOptions(), 
                       &m_iclsQueryInfo);
        SetupQueries(GetQueries(), GetOptionsHandle().GetOptions(), 
                     m_iclsQueryInfo, &m_iclsQueries);

        m_ipScoreBlock = 0;
        
        if (x_eProgram == eRPSBlast || x_eProgram == eRPSTblastn)
            scale_factor = GetRPSInfo()->aux_info.scale_factor;
        else
            scale_factor = 1.0;

        Blast_Message* blast_message = NULL;
        
        /* Pass NULL lookup segments and output filtering locations pointers
           in the next call, to indicate that we don't need them here. */
        BLAST_MainSetUp(x_eProgram, GetOptions().GetQueryOpts(), 
            GetOptions().GetScoringOpts(), GetOptions().GetHitSaveOpts(), 
            m_iclsQueries, m_iclsQueryInfo, scale_factor, NULL, NULL, 
            &m_ipScoreBlock, &blast_message);

        if (blast_message)
            GetErrorMessage().push_back(blast_message);

        BLAST_GapAlignSetUp(x_eProgram, GetSeqSrc(), 
            GetOptions().GetScoringOpts(), GetOptions().GetEffLenOpts(), 
            GetOptions().GetExtnOpts(), GetOptions().GetHitSaveOpts(),
            m_iclsQueryInfo, m_ipScoreBlock, &m_ipScoringParams,
            &m_ipExtParams, &m_ipHitParams, &m_ipEffLenParams, &m_ipGapAlign);

    }
    return status;
}

void
CDbBlastTraceback::RunSearchEngine()
{
    Int2 status;

    status = 
        BLAST_ComputeTraceback(GetOptionsHandle().GetOptions().GetProgram(), 
            m_ipHspStream, m_iclsQueries, m_iclsQueryInfo,
            GetSeqSrc(), m_ipGapAlign, m_ipScoringParams, m_ipExtParams, 
            m_ipHitParams, m_ipEffLenParams, GetOptions().GetDbOpts(), NULL, 
            &m_ipResults);
}

/// Resets query data structures; does only part of the work in the base 
/// CDbBlast class
void
CDbBlastTraceback::x_ResetQueryDs()
{
    m_ibQuerySetUpDone = false;
    // should be changed if derived classes are created
    m_iclsQueries.Reset(NULL);
    m_iclsQueryInfo.Reset(NULL);
    m_ipScoreBlock = BlastScoreBlkFree(m_ipScoreBlock);
    m_ipScoringParams = BlastScoringParametersFree(m_ipScoringParams);
    m_ipExtParams = BlastExtensionParametersFree(m_ipExtParams);
    m_ipHitParams = BlastHitSavingParametersFree(m_ipHitParams);
    m_ipEffLenParams = BlastEffectiveLengthsParametersFree(m_ipEffLenParams);
    m_ipGapAlign = BLAST_GapAlignStructFree(m_ipGapAlign);

    m_ipResults = Blast_HSPResultsFree(m_ipResults);

    sfree(m_ipDiagnostics);
    NON_CONST_ITERATE(TBlastError, itr, m_ivErrors) {
        *itr = Blast_MessageFree(*itr);
    }
}


END_SCOPE(blast)
END_NCBI_SCOPE

/* @} */

/*
 * ===========================================================================
 *
 * $Log$
 * Revision 1.13  2004/06/24 15:54:59  dondosha
 * Added doxygen file description
 *
 * Revision 1.12  2004/06/15 18:46:38  dondosha
 * Access individual options structures through CBlastOptions class methods
 *
 * Revision 1.11  2004/06/08 15:20:44  dondosha
 * Use BlastHSPStream interface
 *
 * Revision 1.10  2004/05/21 21:41:02  gorelenk
 * Added PCH ncbi_pch.hpp
 *
 * Revision 1.9  2004/05/14 17:16:32  dondosha
 * BlastReturnStat structure changed to BlastDiagnostics and refactored
 *
 * Revision 1.8  2004/05/12 12:19:33  madden
 * Add (NULL) psi_options to call to BLAST_ComputeTraceback
 *
 * Revision 1.7  2004/05/07 15:30:09  papadopo
 * use BlastScoringParameters instead of BlastScoringOptions
 *
 * Revision 1.6  2004/05/05 15:28:56  dondosha
 * Renamed functions in blast_hits.h accordance with new convention Blast_[StructName][Task]
 *
 * Revision 1.5  2004/04/30 16:53:06  dondosha
 * Changed a number of function names to have the same conventional Blast_ prefix
 *
 * Revision 1.4  2004/03/16 23:30:59  dondosha
 * Changed mi_ to m_i in member field names
 *
 * Revision 1.3  2004/03/15 19:57:00  dondosha
 * Merged TwoSequences and Database engines
 *
 * Revision 1.2  2004/03/09 18:54:06  dondosha
 * Setup now needs the effective lengths parameters structure
 *
 * Revision 1.1  2004/02/24 18:20:42  dondosha
 * Class derived from CDbBlast to do only traceback, given precomputed HSP results
 *
 *
 * ===========================================================================
 */
