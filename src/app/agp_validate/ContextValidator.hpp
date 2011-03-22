#ifndef AGP_VALIDATE_ContextValidator
#define AGP_VALIDATE_ContextValidator

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
 *      Victor Sapojnikov
 *
 * File Description:
 *      AGP context-sensetive validation (i.e. information from several lines).
 *
 */

#include <corelib/ncbistd.hpp>
#include <iostream>
#include <objtools/readers/agp_util.hpp>

// #include "LineValidator.hpp"
#include <set>
#include "MapCompLen.hpp"

BEGIN_NCBI_SCOPE

 // Determines accession naming patterns, counts accessions.
class CAccPatternCounter;

// Count how many times a atring value appears;
// report values and counts ordered by count.
class CValuesCount : public map<string, int>
{
public:
  void add(const string& c);

  // >pointer to >value_type vector for sorting
  typedef vector<value_type*> TValPtrVec;
  void GetSortedValues(TValPtrVec& out);

private:
  // For sorting by value count
  static int x_byCount( value_type* a, value_type* b );
};

extern CAgpErrEx agpErr;
class CCompVal
{
public:
  int beg, end;
  char ori;
  int file_num, line_num;

  enum { ORI_plus, ORI_minus, ORI_zero, ORI_na,  ORI_count };

  void init(const CAgpRow& row, int line_num_arg)
  {
    beg=row.component_beg;
    end=row.component_end;
    ori=row.orientation;

    line_num=line_num_arg;
    file_num=agpErr.GetFileNum();
  }
  int getLen() const { return end - beg + 1; }

  string ToString() const
  {
    string s;
    s += NStr::IntToString(beg);
    s += "..";
    s += NStr::IntToString(end);
    s += " at ";
    if(file_num) {
      s += agpErr.GetFile(file_num);
      s += ":";
    }
    else {
      s += "line ";
    }
    s += NStr::IntToString(line_num);
    return s;
  }
};

// To save memory, this is a vector instead of a map.
// Multiple spans on one component are uncommon.
class CCompSpans : public vector<CCompVal>
{
public:
  // Construct a vector with one element
  CCompSpans(const CCompVal& src)
  {
    push_back(src);
  }

  // Returns the first overlapping span and CAgpErr::W_SpansOverlap,
  // or the first span out of order and CAgpErr::W_SpansOrder,
  // or begin() and CAgpErr::W_DuplicateComp.
  // The caller can ignore the last 2 warnings for draft seqs.
  typedef pair<iterator, int> TCheckSpan;
  TCheckSpan CheckSpan(int span_beg, int span_end, bool isPlus);
  void AddSpan(const CCompVal& span); // CCompSpans::iterator it,

};


class CAgpValidateReader : public CAgpReader
{
public:
  CAgpValidateReader(CAgpErrEx& agpErr, CMapCompLen& comp2len); // , bool checkCompNames=false);
  //virtual ~CAgpValidateReader();
  void PrintTotals();
  bool m_CheckCompNames;
  bool m_CheckObjLen; // false: check compoment lengths

protected:
  void x_PrintTotals(); // without comment counts
  // true: a suspicious mix of ids - some look like GenBank accessions, some do not.
  static bool x_PrintPatterns(CAccPatternCounter& namePatterns, const string& strHeader);

  int m_CommentLineCount;
  int m_EolComments;

  // Callbacks from CAgpReader
  virtual void OnScaffoldEnd();
  virtual void OnObjectChange();
  virtual void OnGapOrComponent();
  virtual bool OnError();
  virtual void OnComment()
  {
    // Line that starts with "#".
    // No other callbacks invoked for this line.
    m_CommentLineCount++;
  }

  // for W_ObjOrderNotNumerical (JIRA: GP-773)
  string m_obj_id_pattern; // object_id with each run of conseq digits replaced with '#'
  // >0 number of sorted literally so far - 1 (for the same current m_obj_id_pattern)
  int m_obj_id_sorted; // 0 not established yet <0 sort order violated
  CAccPatternCounter::TDoubleVec* m_obj_id_digits;
  CAccPatternCounter::TDoubleVec* m_prev_id_digits;

  CMapCompLen& m_comp2len; // for optional check of component lengths (or maybe object lengths)
  int m_expected_obj_len;
  int m_comp_name_matches;
  int m_obj_name_matches;

  int m_componentsInLastScaffold;
  int m_componentsInLastObject;
  int m_gapsInLastScaffold;
  int m_gapsInLastObject;
  bool m_prev_orientation_unknown;

  string m_prev_component_id; // for W_BreakingGapSameCompId: only set when encountering a breaking gap

  int m_ObjCount;
  int m_ScaffoldCount;
  int m_SingleCompScaffolds;
  int m_SingleCompObjects;
  int m_SingleCompScaffolds_withGaps;
  int m_SingleCompObjects_withGaps;
  int m_NoCompScaffolds;
  // add m_NoCompObjects?

  int m_CompCount;
  int m_GapCount;
  int m_CompOri[4];

  //int m_GapTypeCnt[CGapVal::GAP_count+CGapVal::GAP_yes_count];
  int m_GapTypeCnt[CAgpRow::eGapCount+CAgpRow::eGapYes_count];
  // Count component types and N/U gap types.
  CValuesCount m_TypeCompCnt; // column 5: A, D, F, ..., N, U

  // keep track of the object ids to detect duplicates.
  typedef set<string> TObjSet;
  typedef pair<TObjSet::iterator, bool> TObjSetResult;
  TObjSet m_ObjIdSet;

  CAccPatternCounter m_objNamePatterns;

  // keep track of the component and object ids used
  //  in the AGP. Used to detect duplicates and
  //  duplicates with seq range intersections.
  typedef map<string, CCompSpans> TCompId2Spans;
  typedef pair<string, CCompSpans> TCompIdSpansPair;
  TCompId2Spans m_CompId2Spans;

};

END_NCBI_SCOPE

#endif /* AGP_VALIDATE_ContextValidator */

