/* $Id$
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
 * Author: Christiam Camacho, Kevin Bealer
 *
 */

/** @file prelim_stage.hpp
 * NOTE: This file contains work in progress and the APIs are likely to change,
 * please do not rely on them until this notice is removed.
 */

#ifndef ALGO_BLAST_API___PRELIM_STAGE_HPP
#define ALGO_BLAST_API___PRELIM_STAGE_HPP

#include <algo/blast/api/setup_factory.hpp>
#include <algo/blast/api/query_data.hpp>
#include <algo/blast/api/uniform_search.hpp>

/** @addtogroup AlgoBlast
 *
 * @{
 */

BEGIN_NCBI_SCOPE

BEGIN_SCOPE(objects)
    class CPssmWithParameters;
END_SCOPE(objects)

BEGIN_SCOPE(blast)

// Forward declaration
class IBlastSeqSrcAdapter;

/// Search class to perform the preliminary stage of the BLAST search
/// NOTE: Non-const CBlastOptions is only needed for RPS-BLAST!!!
class NCBI_XBLAST_EXPORT CBlastPrelimSearch : public CObject, public CThreadable
{
public:
    // we create a BlastSeqSrc using CSeqDB as its implementation
    CBlastPrelimSearch(CRef<IQueryFactory> query_factory,
                       CRef<CBlastOptions> options,
                       const CSearchDatabase& dbinfo);
    // we don't own the BlastSeqSrc
    CBlastPrelimSearch(CRef<IQueryFactory> query_factory,
                       CRef<CBlastOptions> options,
                       IBlastSeqSrcAdapter& db);
    // we don't own the BlastSeqSrc
    CBlastPrelimSearch(CRef<IQueryFactory> query_factory,
                       CRef<CBlastOptions> options,
                       BlastSeqSrc* seqsrc,
                       CConstRef<objects::CPssmWithParameters> pssm);
    ~CBlastPrelimSearch();

    /// Borrow the internal data and results results. 
    CRef<SInternalData> Run();

    /// Return HSPs in a structure other than the HSPStream? Provide
    /// conversion? How to combine this with CBlastTracebackStage?
    BlastHSPResults* ComputeBlastHSPResults(BlastHSPStream* stream,
                                            Uint4 max_num_hsps = 0,
                                            bool* rm_hsps = NULL) const;

    /// Retrieve any error/warning messages that occurred during the search
    TSearchMessages GetSearchMessages() const;

    /// Retrieve the filtered/masked query regions
    TSeqLocInfoVector GetFilteredQueryRegions() const;

private:
    /// Prohibit copy constructor
    CBlastPrelimSearch(const CBlastPrelimSearch& rhs);
    /// Prohibit assignment operator
    CBlastPrelimSearch& operator=(const CBlastPrelimSearch& rhs);

    /// Internal initialization function
    /// Initializes internal data structures except the BlastSeqSrc
    /// @param query_factory Contains query related data [in]
    /// @param options BLAST algorithm options [in]
    /// @param pssm PSSM to initialize PSI-BLAST
    /// @param dbname required for RPS-BLAST to initialize the RPS-BLAST
    /// related data structures from the RPS-BLAST database [in]
    void x_Init(CRef<IQueryFactory> query_factory,
                CRef<CBlastOptions> options,
                CConstRef<objects::CPssmWithParameters> pssm,
                const string& dbname);

    /// Runs the preliminary search in multi-threaded mode
    int x_LaunchMultiThreadedSearch();

    /// Query factory is retained to ensure the lifetime of the data (queries)
    /// produced by it.
    CRef<IQueryFactory>             m_QueryFactory;
    CRef<SInternalData>             m_InternalData;
    const CBlastOptionsMemento*     m_OptsMemento;

    /// Warnings and error messages
    TSearchMessages                 m_Messages;

    /// Query masking information
    TSeqLocInfoVector               m_MasksForAllQueries;
};

inline TSearchMessages
CBlastPrelimSearch::GetSearchMessages() const
{
    return m_Messages;
}

inline TSeqLocInfoVector
CBlastPrelimSearch::GetFilteredQueryRegions() const
{
    return m_MasksForAllQueries;
}

END_SCOPE(BLAST)
END_NCBI_SCOPE

/* @} */

#endif /* ALGO_BLAST_API___PRELIM_STAGE__HPP */

