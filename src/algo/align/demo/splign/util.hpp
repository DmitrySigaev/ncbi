#ifndef ALGO_ALIGN_DEMO_SPLIGN_UTIL__HPP
#define ALGO_ALIGN_DEMO_SPLIGN_UTIL__HPP

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
* File Description:  Helper functions
*                   
* ===========================================================================
*/


#include <vector>
#include "splign.hpp"
#include "hf_hit.hpp"


BEGIN_NCBI_SCOPE

size_t TestPolyA(const vector<char>& mrna);
void   CleaveOffByTail(vector<CHit>* hits, size_t polya_start);
void   DoFilter(vector<CHit>* hits);
void   GetHitsMinMax(const vector<CHit>& hits,
		     size_t* qmin, size_t* qmax,
		     size_t* smin, size_t* smax);
void   SetPatternFromHits(CSplign& splign, vector<CHit>* hits);
string RLE(const string& in);
void MakeIDX( istream* inp_istr, const size_t file_index, ostream* out_ostr );


struct SCompliment
{
    char operator() (char c) {
        switch(c) {
        case 'A': return 'T';
        case 'G': return 'C';
        case 'T': return 'A';
        case 'C': return 'G';
        }
        return c;
    }
};


END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.3  2003/11/05 20:24:21  kapustin
 * Update fasta indexing routine
 *
 * Revision 1.2  2003/10/31 19:43:15  kapustin
 * Format and compatibility update
 *
 * ===========================================================================
 */


#endif
