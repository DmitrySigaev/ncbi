#ifndef AGP_VALIDATE_AltValidator
#define AGP_VALIDATE_AltValidator

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
 * Authors:
 *      Lou Friedman, Victor Sapojnikov
 *
 * File Description:
 *      Checks that use component Accession, Length and Taxid info from the GenBank.
 *
 */

#include "LineValidator.hpp"

BEGIN_NCBI_SCOPE

class CAltValidator
{
public:
  bool m_check_len_taxid;
  CAltValidator(bool check_len_taxid)
  {
    m_check_len_taxid=check_len_taxid;
  }

  bool m_SpeciesLevelTaxonCheck;

  // The next few structures are used to keep track
  //  of the taxids used in the AGP files. A map of
  //  taxid to a vector of AGP file lines is maintained.
  struct SAgpLineInfo {
      int    file_num;
      int    line_num;
      string component_id;
  };

  typedef vector<SAgpLineInfo> TAgpInfoList;
  typedef map<int, TAgpInfoList> TTaxidMap;
  typedef pair<TTaxidMap::iterator, bool> TTaxidMapRes;
  TTaxidMap m_TaxidMap;
  int m_TaxidComponentTotal;

  typedef map<int, int> TTaxidSpeciesMap;
  TTaxidSpeciesMap m_TaxidSpeciesMap;
  int m_GenBankCompLineCount;

  // Methods
  void Init();
  void ValidateLine(
    const string& comp_id,
    int line_num, int comp_end);
  void CheckTaxids();
  void PrintTotals();

  static int ValidateAccession( const string& acc );
  static void ValidateLength(const string& comp_id, int comp_end, int comp_len);

  void x_AddToTaxidMap(int taxid, const string& comp_id, int line_num);

  // searches m_TaxidSpeciesMap first, calls x_GetTaxonSpecies() if necessary
  int x_GetSpecies(int taxid);
  int x_GetTaxonSpecies(int taxid);

  struct SLineData
  {
    string orig_line;
    string comp_id;
    int line_num;
    int comp_end;

    CNcbiOstrstream* messages;
    int gi;

    SLineData()
    {
      messages=NULL;
      gi=0;
    }
  };

  typedef vector<SLineData> TLineQueue;
  TLineQueue lineQueue;

  void QueueLine(
    const string& orig_line, const string& comp_id,
    int line_num, int comp_end);
  int QueueSize()
  {
    return lineQueue.size();
  }
  void ProcessQueue();
};


END_NCBI_SCOPE

#endif /* AGP_VALIDATE_AltValidator */


