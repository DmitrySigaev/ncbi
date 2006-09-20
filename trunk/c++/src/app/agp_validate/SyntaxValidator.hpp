#ifndef AGP_VALIDATE_SyntaxValidator
#define AGP_VALIDATE_SyntaxValidator

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
 *      Syntactic validation tests that can be preformed solely by the
 *      information that in the AGP file.
 *
 */

#include <corelib/ncbistd.hpp>
#include <corelib/ncbimisc.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbidiag.hpp>
#include <corelib/ncbistr.hpp>
#include <connect/ncbi_core_cxx.hpp>
#include <util/range_coll.hpp>

#include <iostream>
#include "AgpErr.hpp"

#define NO_LOG false

BEGIN_NCBI_SCOPE

extern CAgpErr agpErr;

extern int agp_error_count;
extern int agp_warn_count;

struct SDataLine {
  int     line_num;
  string  object;
  string  begin;
  string  end;
  string  part_num;
  string  component_type;
  string  component_id;
  string  component_start;
  string  component_end;
  string  orientation;
  string  gap_length;
  string  gap_type;
  string  linkage;
};

 // Determines accession naming patterns, count accessions.
class CAccPatternCounter;

// Count how many times a atring value appears;
// report values and counts ordered by count.
class CValuesCount : public map<string, int>
{
public:
  void add(const string& c);

  // >pointer to >value_type vector for sorting
  typedef vector<value_type*> pv_vector;
  void GetSortedValues(pv_vector& out);

private:
  // For sorting by value count
  static int x_byCount( value_type* a, value_type* b );
};

class CAgpSyntaxValidator
{
public:
  CAgpSyntaxValidator();
  ~CAgpSyntaxValidator();
  // former CAgpValidateApplication::x_ValidateSyntaxLine
  void ValidateLine( const SDataLine& dl,
    const string& text_line, bool last_validation=false);

  // Note: resets post_prev
  //const string& PreviousLineToPrint();

  // static bool GapBreaksScaffold(int type, int linkage);
  static bool IsGapType(const string& type);
  void PrintTotals();

  // Temporarily public;
  // prev_line_filename usage is questionable anyway...
  // string prev_line_filename;

protected:
  // Vars assigned in ValidateLine(),
  // further validated in x_OnGapLine() x_OnComponentLine()
  int obj_range_len;
  bool new_obj;

  // Private subroutines for ValidateLine()
  void x_OnGapLine(
    const SDataLine& dl, const string& text_line);
  void x_OnComponentLine(
    const SDataLine& dl, const string& text_line);

  int m_ObjCount;
  int m_ScaffoldCount;
  int m_SingletonCount;
  int m_CompCount;
  int m_CompPosCount;
  int m_CompNegCount;
  int m_CompZeroCount;
  int m_CompNaCount;
  int m_GapCount;


  CValuesCount m_TypeGapCnt;  // column 7: fragment, clone, ...
  CValuesCount m_TypeCompCnt; // column 5: A, D, F, ..., N, U

  // Count component types and N/U gap types.
  int  m_LineTypes;
  int* m_LineTypeCnt;

  // keep track of the object ids to detect duplicates.
  typedef set<string> TObjSet;
  typedef pair<TObjSet::iterator, bool> TObjSetResult;
  TObjSet m_ObjIdSet;

  // keep track of the component and object ids used
  //  in the AGP. Used to detect duplicates and
  //  duplicates with seq range intersections.
  typedef CRangeCollection<TSeqPos> TCompSpans;
  typedef map<string, TCompSpans> TCompId2Spans;
  typedef pair<string, TCompSpans> TCompIdSpansPair;
  //typedef pair<TCompId2Spans::iterator, bool> TIdMapResult;

  TCompId2Spans m_CompId2Spans;

  // proper values for the different fields in the AGP
  typedef set<string> TValuesSet;
  TValuesSet m_ComponentTypeValues;
  TValuesSet m_OrientaionValues;
  //TValuesSet m_GapTypes;
  //TValuesSet m_LinkageValues;

  // Compared to TValuesSet. provides code optimization by avoiding
  // repetetive string comparisons.
  typedef map<string, int> TValuesMap;
  enum {
    GAP_contig         ,
    GAP_clone          ,
    GAP_fragment       ,
    GAP_repeat         ,

    GAP_centromere     ,
    GAP_short_arm      ,
    GAP_heterochromatin,
    GAP_telomere       ,

    //GAP_scaffold       ,
    //GAP_split_finished ,

    GAP_count
  };
  enum {
    LINKAGE_no ,
    LINKAGE_yes ,
    LINKAGE_count
  };
  TValuesMap m_GapTypes;
  TValuesMap m_LinkageValues;


  bool x_CheckValues(int line_num, const TValuesSet& values,
    const string& value, const string& field_name,
    bool log_error = true);
  // Returns an integer constant mapped to the allowed text value,
  // -1 if the value is unknowm
  int x_CheckValues(int line_num, const TValuesMap& values,
    const string& value, const string& field_name,
    bool log_error = true);

  int x_CheckRange(int line_num, int start, int begin,
    int end, string begin_name, string end_name,
    CAgpErr::TCode ltCode); // // "Less Than" error Code
  int x_CheckIntField(int line_num, const string& field,
    const string& field_name, bool log_error = true);

  bool m_LineErrorOccured;
  CNcbiOstrstream* m_ValidateMsg;

  string prev_object;
  string prev_component_type;
  int prev_gap_type;
  string prev_line;

  bool prev_orientation_unknown;
  //string prev_component_id;
  //string prev_component_line;
  //int prev_component_line_num;

  int  prev_end;
  int  prev_part_num;
  int  prev_line_num;
  bool prev_line_error_occured;
  int  componentsInLastScaffold;

  // true: print the previous line as well, to illustrate the error better
  // bool post_prev;

  CAccPatternCounter* objNamePatterns;
};

END_NCBI_SCOPE

#endif /* AGP_VALIDATE_SyntaxValidator */

