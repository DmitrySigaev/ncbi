#ifndef ALGO_ALIGN_SPLIGN_FORMATTER__HPP
#define ALGO_ALIGN_SPLIGN_FORMATTER__HPP

/* $Id$
* ===========================================================================
*
*                            public DOMAIN NOTICE                          
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
* Author:  Yuri Kapustin
*
* File Description:
*   Splign formatter
*/

#include <algo/align/splign/splign.hpp>
#include <objects/seqalign/Seq_align_set.hpp>
#include <objects/seqalign/Dense_seg.hpp>


BEGIN_NCBI_SCOPE

class NCBI_XALGOALIGN_EXPORT CSplignFormatter: public CObject
{
public:

    CSplignFormatter(const CSplign& splign);

    // setters
    void SetSeqIds(const string& id1, const string& id2) {
        m_QueryId = id1;
        m_SubjId = id2;
    }

    string AsText(const CSplign::TResults* results = 0) const;
    CRef<objects::CSeq_align_set> AsSeqAlignSet(const CSplign::TResults*
                                                results = 0) const;

private:

    const CSplign*    m_splign;
    string            m_QueryId, m_SubjId;

    CRef<objects::CSeq_align> x_Compartment2SeqAlign(
                               const vector<size_t>& boxes,
                               const vector<string>& transcripts,
                               const vector<CNWAligner::TScore>& scores) const;
    void x_Exon2DS(const size_t* box, const string& trans,
                   objects::CDense_seg* pds) const;
};


END_NCBI_SCOPE


/*
 * ===========================================================================
 *
 * $Log$
 * Revision 1.8  2004/06/21 17:44:46  kapustin
 * Add result param to AsText and AsSeqAlignSet with zero default
 *
 * Revision 1.7  2004/05/04 15:23:44  ucko
 * Split splign code out of xalgoalign into new xalgosplign.
 *
 * Revision 1.6  2004/04/30 15:00:32  kapustin
 * Support ASN formatting
 *
 * Revision 1.5  2004/04/23 14:36:24  kapustin
 * Initial revision
 *
 *
 * ===========================================================================
 */

#endif
