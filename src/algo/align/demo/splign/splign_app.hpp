#ifndef ALGO_ALIGN__SPLIGN__HPP
#define ALGO_ALIGN__SPLIGN__HPP

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
* File Description:  Splign application class declarations
*                   
* ===========================================================================
*/

#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbiargs.hpp>

#include "hf_hit.hpp"
#include "seq_loader.hpp"

BEGIN_NCBI_SCOPE


class CSplignApp: public CNcbiApplication
{
public:

    virtual void Init();
    virtual int  Run();

protected:

  string x_RunOnPair(vector<CHit>* hits);
  void   x_Filter(vector<CHit>* hits);
  bool   x_GetNextQuery(ifstream* ifs, vector<CHit>* hits, string* first_line);
  size_t x_TestPolyA(const vector<char>& mrna);

  // sequence loader
  CSeqLoader m_seqloader;

  // seq quality
  unsigned char m_SeqQuality; // 0 == low, 1 == high

  // RLE flag
  bool m_rle;

  // min exon identity for Splign
  double m_minidty;

  // min query hit coverage
  double m_min_query_coverage;

  // mandatory end gaps detection flag
  bool m_endgaps;

  // presume no Poly(A) tail when true
  bool m_nopolya;

  // status log
  ofstream m_logstream;
  void   x_LogStatus(const string& query, const string& subj,
		     bool error, const string& msg);

};

END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.3  2003/11/20 14:38:10  kapustin
 * Add -nopolya flag to suppress Poly(A) detection.
 *
 * Revision 1.2  2003/11/05 20:32:11  kapustin
 * Include source information into the index
 *
 * Revision 1.1  2003/10/30 19:37:20  kapustin
 * Initial toolkit revision
 *
 * ===========================================================================
 */


#endif
