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
 */

/// @file hspstream_queue.hpp
/// C++ implementation of the BlastHSPStream interface for producing results 
/// on the fly.

#ifndef ALGO_BLAST_API___HSPSTREAM_QUEUE_HPP
#define ALGO_BLAST_API___HSPSTREAM_QUEUE_HPP

#include <corelib/ncbimtx.hpp>
#include <algo/blast/core/blast_options.h>
#include <algo/blast/core/blast_hits.h>
#include <algo/blast/core/blast_seqsrc.h>
#include <algo/blast/core/blast_hspstream.h>
#include <list>

/** @addtogroup AlgoBlast
 *
 * @{
 */

USING_NCBI_SCOPE;

/// Data structure for the queue implementation of BlastHSPStream
class CBlastHSPListQueueData
{
public:
    /// Constructor
    CBlastHSPListQueueData();
    /// Destructor
    ~CBlastHSPListQueueData();
    /// Read from the queue
    int Read(BlastHSPList** hsp_list );
    /// Write to the queue
    int Write(BlastHSPList** hsp_list);
    /// Close the queue for writing
    void Close();
    /// Check if wait on semaphore is needed
    bool NeedWait();
private:
    std::list<BlastHSPList*> m_resultsQueue;
    bool m_writingDone;
    CFastMutex m_Mutex;
    CSemaphore* m_Sema;
};

BlastHSPStream* Blast_HSPListCQueueInit();

/* @} */

/*
* ===========================================================================
*
* $Log$
* Revision 1.1  2004/06/15 16:25:23  dondosha
* Implementation of an HSP list queue
*
*
* ===========================================================================
*/
#endif /* ALGO_BLAST_API___HSPSTREAM_QUEUE_HPP */
