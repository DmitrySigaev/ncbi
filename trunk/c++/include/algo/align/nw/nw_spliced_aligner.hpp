#ifndef ALGO_ALIGN___NW_SPLICED_ALIGNER__HPP
#define ALGO_ALIGN___NW_SPLICED_ALIGNER__HPP

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
* Author:  Yuri Kapustin
*
* File Description:
*   Base class for spliced aligners.
*
*/

#include "nw_aligner.hpp"


BEGIN_NCBI_SCOPE

class NCBI_XALGOALIGN_EXPORT CSplicedAligner: public CNWAligner
{
public:

    // Setters
    virtual void SetWi( unsigned char splice_type, TScore value );
    void SetIntronMinSize( size_t s )  { m_IntronMinSize  = s; }

    // Getters
    static TScore GetDefaultWi  (unsigned char splice_type)
        throw(CAlgoAlignException);
    static size_t GetDefaultIntronMinSize () {
        return 50;
    }

    // Guides
    size_t MakeGuides(const size_t guide_size = 30);

protected:

    CSplicedAligner();
    CSplicedAligner( const char* seq1, size_t len1,
                     const char* seq2, size_t len2) throw(CAlgoAlignException);

    size_t  m_IntronMinSize;
    virtual size_t  x_GetSpliceTypeCount()  = 0;
    virtual TScore* x_GetSpliceScores() = 0;
    virtual TScore  x_Align ( const char* seg1, size_t len1,
                              const char* seg2, size_t len2,
                              vector<ETranscriptSymbol>* transcript) = 0;
    // Guides
    unsigned char   x_CalcFingerPrint64( const char* beg,
                                         const char* end,
                                         size_t& err_index );
    const char*     x_FindFingerPrint64( const char* beg, 
                                         const char* end,
                                         unsigned char fingerprint,
                                         size_t size,
                                         size_t& err_index );
};


END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.1  2003/09/02 22:27:44  kapustin
 * Initial revision
 *
 * ===========================================================================
 */

#endif  /* ALGO_ALIGN___SPLICED_ALIGNER__HPP */
