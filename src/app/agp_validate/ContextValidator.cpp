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
 *
 */

#include <ncbi_pch.hpp>
#include "ContextValidator.hpp"
#include "AccessionPatterns.hpp"

#include <algorithm>

USING_NCBI_SCOPE;
BEGIN_NCBI_SCOPE

//// class CAgpContextValidator
CAgpContextValidator::CAgpContextValidator()
{
  prev_end = 0;
  prev_part_num = 0;
  componentsInLastScaffold = 0;
  componentsInLastObject = 0;
  prev_orientation_unknown=false;

  m_ObjCount = 0;
  m_ScaffoldCount = 0;
  m_SingleCompScaffolds = 0;
  m_SingleCompObjects = 0;
  m_CompCount = 0;
  m_GapCount = 0;

  memset(m_CompOri, 0, sizeof(m_CompOri));
  memset(m_GapTypeCnt, 0, sizeof(m_GapTypeCnt));
}

void CAgpContextValidator::EndOfObject(bool afterLastLine)
{
  if(componentsInLastObject==0) agpErr.Msg(
    CAgpErr::W_ObjNoComp, string(" ") + prev_object,
    afterLastLine ? AT_PrevLine : (AT_ThisLine|AT_PrevLine)
  );
  if(componentsInLastScaffold==1) m_SingleCompScaffolds++;
  if(componentsInLastObject  ==1) m_SingleCompObjects++;

  if( prev_line_is_gap ) {
    // The previous line was a gap at the end of a scaffold & object
    agpErr.Msg( CAgpErr::W_GapObjEnd, string(" ")+prev_object,
      AT_PrevLine);
    if(componentsInLastScaffold==0) m_ScaffoldCount--;
  }

  componentsInLastScaffold=0;
  componentsInLastObject=0;
}

void CAgpContextValidator::InvalidLine()
{
  prev_orientation_unknown=false;
}


void CAgpContextValidator::ValidateLine(
  const SDataLine& dl, const CAgpLine& cl)
{
  //// Context-sensetive code common for GAPs and components.
  //// Checks and statistics.

  // Local variable shared with x_OnGapLine(), x_OnCompLine()
  new_obj = (dl.object != prev_object);

  if(new_obj) {
    m_ObjCount++;
    m_ScaffoldCount++; // to do: detect or avoid scaffold with 0 components?

    // Do not call when we are at the first line of the first object
    if( prev_object.size() ) EndOfObject();
    prev_object = dl.object; // Must be after EndOfObject()

    TObjSetResult obj_insert_result = m_ObjIdSet.insert(dl.object);
    if (obj_insert_result.second == false) {
      agpErr.Msg(CAgpErr::E_DuplicateObj, dl.object,
        AT_ThisLine|AT_PrevLine);
    }

    // object_beg and part_num: must be 1
    if(cl.obj_begin != 1) {
      agpErr.Msg(CAgpErr::E_ObjMustBegin1,
        NcbiEmptyString,
        AT_ThisLine|AT_PrevLine|AT_SkipAfterBad
      );
    }
    if(cl.part_num != 1) {
      agpErr.Msg( CAgpErr::E_PartNumberNot1,
        NcbiEmptyString, AT_ThisLine|AT_PrevLine|AT_SkipAfterBad );
    }
  }
  else {
    // object range and part_num: compare to prev line
    if(cl.obj_begin != prev_end+1) {
      agpErr.Msg(CAgpErr::E_ObjBegNePrevEndPlus1,
        NcbiEmptyString,
        AT_ThisLine|AT_PrevLine|AT_SkipAfterBad
      );
    }
    if(cl.part_num != prev_part_num+1) {
      agpErr.Msg( CAgpErr::E_PartNumberNotPlus1,
        NcbiEmptyString, AT_ThisLine|AT_PrevLine|AT_SkipAfterBad );
    }
  }

  m_TypeCompCnt.add( dl.component_type );

  //// Gap- or component-specific code.
  if( cl.is_gap ) {
    x_OnGapLine(dl, cl.gap);
  }
  else {
    x_OnCompLine(dl, cl.compSpan);
  }

  prev_end = cl.obj_end;
  prev_part_num = cl.part_num;
  prev_line_is_gap = cl.is_gap;
}

void CAgpContextValidator::x_OnGapLine(
  const SDataLine& dl, const CGapVal& gap)
{
  int i = gap.type;
  if(gap.linkage==CGapVal::LINKAGE_yes) i+= CGapVal::GAP_count;
  NCBI_ASSERT( i < (int)sizeof(m_GapTypeCnt)/sizeof(m_GapTypeCnt[0]),
    "m_GapTypeCnt[] index out of bounds" );
  m_GapTypeCnt[i]++;
  m_GapCount++;

  //// Check the gap context: is it a start of a new object,
  //// does it follow another gap, is it the end of a scaffold.
  if(new_obj) {
    agpErr.Msg(CAgpErr::W_GapObjBegin,
      NcbiEmptyString, AT_ThisLine|AT_SkipAfterBad);
  }
  else if( prev_line_is_gap ) {
    agpErr.Msg( CAgpErr::W_ConseqGaps, NcbiEmptyString,
      AT_ThisLine|AT_PrevLine|AT_SkipAfterBad );
  }
  else if( gap.endsScaffold() ) {
    // A breaking gap after a component.
    // Previous line is the last component of a scaffold
    m_ScaffoldCount++; // to do: detect or do not count scaffold with 0 components?
    if(componentsInLastScaffold==1) {
      m_SingleCompScaffolds++;
    }
  }
  else {
    // a non-breaking gap after a component
    if(prev_orientation_unknown && componentsInLastScaffold==1) {
                             // ^^^can probably ASSERT this^^^
      agpErr.Msg(CAgpErr::E_UnknownOrientation,
        NcbiEmptyString, AT_PrevLine);
      prev_orientation_unknown=false;
    }
  }
  if( gap.endsScaffold() ) {
    componentsInLastScaffold=0;
  }
}

void CAgpContextValidator::x_OnCompLine(
  const SDataLine& dl, const CCompVal& comp )
{
  //// Statistics
  m_CompCount++;
  componentsInLastScaffold++;
  componentsInLastObject++;
  m_CompOri[comp.ori]++;

  //// Orientation "0" or "na" only for singletons
  // A saved potential error
  if(prev_orientation_unknown) {
    // Make sure that prev_orientation_unknown
    // is not a leftover from the preceding singleton.
    if( componentsInLastScaffold==2 ) {
      agpErr.Msg(CAgpErr::E_UnknownOrientation,
        NcbiEmptyString, AT_PrevLine);
    }
    prev_orientation_unknown=false;
  }

  if( comp.ori != CCompVal::ORI_plus && comp.ori != CCompVal::ORI_minus ) {
    if(componentsInLastScaffold==1) {
      // Report an error later if the current scaffold
      // turns out not a singleton. Only singletons can have
      // components with unknown orientation.
      //
      // Note: previous component != previous line
      // if there was a non-breaking gap; thus, we check
      // prev_orientation_unknown for such gaps, too.
      prev_orientation_unknown=true;
    }
    else {
      // This error is real, not "potential"; report it now.
      agpErr.Msg(CAgpErr::E_UnknownOrientation);
    }
  }

  //// Check: component spans do not overlap and are in correct order

  // Try to insert to the span as a new entry
  TCompIdSpansPair value_pair( dl.component_id, CCompSpans(comp) );
  pair<TCompId2Spans::iterator, bool> id_insert_result =
      m_CompId2Spans.insert(value_pair);

  if(id_insert_result.second == false) {
    // Not inserted - the key already exists.
    CCompSpans& spans = (id_insert_result.first)->second;

    // Issue at most 1 warning
    CCompSpans::TCheckSpan check_sp = spans.CheckSpan(
      comp.beg, comp.end, comp.ori!=CCompVal::ORI_minus
    );
    if( check_sp.second == CAgpErr::W_SpansOverlap  ) {
      agpErr.Msg(CAgpErr::W_SpansOverlap,
        string(": ")+ check_sp.first->ToString()
      );
    }
    else if(
      dl.component_type!="A" && // Active Finishing
      dl.component_type!="D" && // Draft HTG
      dl.component_type!="P"    // Pre Draft
    ) {
      // Non-draft sequence
      agpErr.Msg(check_sp.second, // W_SpansOrder or W_DuplicateComp
        string("; preceding span: ")+ check_sp.first->ToString()
      );
    }

    // Add the span to the existing entry
    spans.AddSpan(comp);
  }
}

#define ALIGN_W(x) setw(w) << resetiosflags(IOS_BASE::left) << (x)
void CAgpContextValidator::PrintTotals()
{
  //// Counts of errors and warnings
  int e_count=agpErr.CountTotals(CAgpErr::E_Last);
  int w_count=agpErr.CountTotals(CAgpErr::W_Last);
  cout << "\n";
  agpErr.PrintTotals(cout, e_count, w_count, agpErr.m_msg_skipped);
  if(agpErr.m_MaxRepeatTopped) {
    cout << " (to print all: -limit 0)";
  }
  cout << ".";
  if(agpErr.m_MaxRepeat && (e_count+w_count) ) {
    cout << "\n";
    agpErr.PrintMessageCounts(cout, CAgpErr::CODE_First, CAgpErr::CODE_Last);
  }
  if(m_ObjCount==0) {
    cout << "\nNo valid AGP lines.\n";
    return;
  }

  cout << "\n";
  if(agpErr.m_lines_skipped) {
    cout << "\nNOTE: " << agpErr.m_lines_skipped << " invalid lines were skipped.\n"
      "Some counts below may be smaller than expected by " << agpErr.m_lines_skipped
      << " or less.\n";
  }

  //// Prepare component/gap types and counts for later printing
  string s_comp, s_gap;

  CValuesCount::pv_vector comp_cnt;
  m_TypeCompCnt.GetSortedValues(comp_cnt);

  for(CValuesCount::pv_vector::iterator
    it = comp_cnt.begin();
    it != comp_cnt.end();
    ++it
  ) {
    string *s = CAgpLine::IsGapType((*it)->first) ? &s_gap : &s_comp;

    if( s->size() ) *s+= ", ";
    *s+= (*it)->first;
    *s+= ":";
    *s+= NStr::IntToString((*it)->second);
  }

  //// Various counts of AGP elements

  // w: width for right alignment
  int w = NStr::IntToString(m_CompId2Spans.size()).size(); // +1;

  cout << "\n"
    "Objects                : "<<ALIGN_W(m_ObjCount           )<<"\n"
    "- with single component: "<<ALIGN_W(m_SingleCompObjects  )<<"\n"
    "\n"
    "Scaffolds              : "<<ALIGN_W(m_ScaffoldCount      )<<"\n"
    "- with single component: "<<ALIGN_W(m_SingleCompScaffolds)<<"\n"
    "\n";


  cout<<
    "Components             : "<< ALIGN_W(m_CompCount);
  if(m_CompCount) {
    if( s_comp.size() ) {
      if( NStr::Find(s_comp, ",")!=NPOS ) {
        // (W: 1234, D: 5678)
        cout << " (" << s_comp << ")";
      }
      else {
        // One type of components: (W) or (invalid type)
        cout << " (" << s_comp.substr( 0, NStr::Find(s_comp, ":") ) << ")";
      }
    }
    cout << "\n"
      "\torientation +  : " << ALIGN_W(m_CompOri[CCompVal::ORI_plus ]) << "\n"
      "\torientation -  : " << ALIGN_W(m_CompOri[CCompVal::ORI_minus]) << "\n"
      "\torientation 0  : " << ALIGN_W(m_CompOri[CCompVal::ORI_zero ]) << "\n"
      "\torientation na : " << ALIGN_W(m_CompOri[CCompVal::ORI_na   ]) << "\n";
  }

  cout << "\n" << "Gaps                   : " << ALIGN_W(m_GapCount);
  if(m_GapCount) {
    // Print (N) if all components are of one type,
    //        or (N: 1234, U: 5678)
    if( s_gap.size() ) {
      if( NStr::Find(s_gap, ",")!=NPOS ) {
        // (N: 1234, U: 5678)
        cout << " (" << s_gap << ")";
      }
      else {
        // One type of gaps: (N)
        cout << " (" << s_gap.substr( 0, NStr::Find(s_gap, ":") ) << ")";
      }
    }

    int linkageYesCnt =
      m_GapTypeCnt[CGapVal::GAP_count+CGapVal::GAP_clone   ]+
      m_GapTypeCnt[CGapVal::GAP_count+CGapVal::GAP_fragment]+
      m_GapTypeCnt[CGapVal::GAP_count+CGapVal::GAP_repeat  ];
    int linkageNoCnt = m_GapCount - linkageYesCnt;
    /*
    cout << "\n- linkage yes -------- : "<<ALIGN_W(linkageYesCnt);
    if(linkageYesCnt) {
      for(int i=0; i<CGapVal::GAP_yes_count; i++) {
        cout<< "\n\t"
            << setw(15) << setiosflags(IOS_BASE::left) << CGapVal::typeIntToStr[i]
            << ": " << ALIGN_W( m_GapTypeCnt[CGapVal::GAP_count+i] );
      }
    }

    cout << "\n- linkage no  -------- : "<<ALIGN_W(linkageNoCnt);
    if(linkageNoCnt) {
      for(int i=0; i<CGapVal::GAP_count; i++) {
        cout<< "\n\t"
            << setw(15) << setiosflags(IOS_BASE::left) << CGapVal::typeIntToStr[i]
            << ": " << ALIGN_W( m_GapTypeCnt[i] );
      }
    }
    */

    int doNotBreakCnt= linkageYesCnt + m_GapTypeCnt[CGapVal::GAP_fragment];
    int breakCnt     = linkageNoCnt  - m_GapTypeCnt[CGapVal::GAP_fragment];

    cout<< "\n- do not break scaffold: "<<ALIGN_W(doNotBreakCnt);
    if(doNotBreakCnt) {
      cout<< "\n  clone   , linkage yes: "<<
            ALIGN_W(m_GapTypeCnt[CGapVal::GAP_count+CGapVal::GAP_clone   ]);
      cout<< "\n  fragment, linkage yes: "<<
            ALIGN_W(m_GapTypeCnt[CGapVal::GAP_count+CGapVal::GAP_fragment]);
      cout<< "\n  fragment, linkage no : "<<
            ALIGN_W(m_GapTypeCnt[                   CGapVal::GAP_fragment]);
      cout<< "\n  repeat  , linkage yes: "<<
            ALIGN_W(m_GapTypeCnt[CGapVal::GAP_count+CGapVal::GAP_repeat  ]);
    }

    cout<< "\n- break it, linkage no : "<<ALIGN_W(breakCnt);
    if(breakCnt) {
      for(int i=0; i<CGapVal::GAP_count; i++) {
        if(i==CGapVal::GAP_fragment) continue;
        cout<< "\n\t"
            << setw(15) << setiosflags(IOS_BASE::left) << CGapVal::typeIntToStr[i]
            << ": " << ALIGN_W( m_GapTypeCnt[i] );
      }
    }

  }
  cout << "\n";

  if(m_ObjCount) {
    {
      CAccPatternCounter objNamePatterns;
      objNamePatterns.AddNames(m_ObjIdSet);
      x_PrintPatterns(objNamePatterns, "Object names");
    }
  }

  if(m_CompId2Spans.size()) {
    {
      CAccPatternCounter compNamePatterns;
      for(TCompId2Spans::iterator it = m_CompId2Spans.begin();
        it != m_CompId2Spans.end(); ++it)
      {
        compNamePatterns.AddName(it->first);
      }
      x_PrintPatterns(compNamePatterns, "Component names");
    }
  }
}

// Sort by accession count, print not more than MaxPatterns or 2*MaxPatterns
void CAgpContextValidator::x_PrintPatterns(
  CAccPatternCounter& namePatterns, const string& strHeader)
{
  const int MaxPatterns=10;

  // Get a vector with sorted pointers to map values
  CAccPatternCounter::pv_vector pat_cnt;
  namePatterns.GetSortedValues(pat_cnt);

  /*
  if(pat_cnt.size()==1) {
    // Continue on the same line, do not print counts
    cout<< " " << CAccPatternCounter::GetExpandedPattern( pat_cnt[0] )
        << "\n";
    return;
  }
  */
  cout << "\n";


  // Calculate width of columns 1 (pattern) and 2 (count)
  int w = NStr::IntToString(
      CAccPatternCounter::GetCount(pat_cnt[0])
    ).size()+1;

  int wPattern=strHeader.size()-2;
  int totalCount=0;
  int patternsPrinted=0;
  for(CAccPatternCounter::pv_vector::iterator it =
      pat_cnt.begin(); it != pat_cnt.end(); ++it
  ) {
    if( ++patternsPrinted<=MaxPatterns || pat_cnt.size()<=2*MaxPatterns ) {
      int i = CAccPatternCounter::GetExpandedPattern(*it).size();
      if( i > wPattern ) wPattern = i;
    }
    else {
      if(w+15>wPattern) wPattern = w+15;
    }
    totalCount+=CAccPatternCounter::GetCount(*it);
  }

  // Print the total
  cout<< setw(wPattern+2) << setiosflags(IOS_BASE::left)
      << strHeader << ": " << ALIGN_W(totalCount) << "\n";

  // Print the patterns
  patternsPrinted=0;
  int accessionsSkipped=0;
  for(CAccPatternCounter::pv_vector::iterator it =
      pat_cnt.begin(); it != pat_cnt.end(); ++it
  ) {
    // Limit the number of lines to MaxPatterns or 2*MaxPatterns
    if( ++patternsPrinted<=MaxPatterns || pat_cnt.size()<=2*MaxPatterns ) {
      cout<< "  " << setw(wPattern) << setiosflags(IOS_BASE::left)
          << CAccPatternCounter::GetExpandedPattern(*it)
          << ": " << ALIGN_W( CAccPatternCounter::GetCount(*it) )
          << "\n";
    }
    else {
      accessionsSkipped += CAccPatternCounter::GetCount(*it);
    }
  }

  if(accessionsSkipped) {
    string s = "other ";
    s+=NStr::IntToString(pat_cnt.size() - 10);
    s+=" patterns";
    cout<< "  " << setw(wPattern) << setiosflags(IOS_BASE::left) << s
        << ": " << ALIGN_W( accessionsSkipped )
        << "\n";

  }
}

//// class CValuesCount

void CValuesCount::GetSortedValues(pv_vector& out)
{
  out.clear(); out.reserve( size() );
  for(iterator it = begin();  it != end(); ++it) {
    out.push_back(&*it);
  }
  std::sort( out.begin(), out.end(), x_byCount );
}

void CValuesCount::add(const string& c)
{
  iterator it = find(c);
  if(it==end()) {
    (*this)[c]=1;
  }
  else{
    it->second++;
  }
}

int CValuesCount::x_byCount( value_type* a, value_type* b )
{
  if( a->second != b->second ){
    return a->second > b->second; // by count, largest first
  }
  return a->first < b->first; // by name
}

//// class CCompSpans - stores data for all preceding components
CCompSpans::TCheckSpan CCompSpans::CheckSpan(int span_beg, int span_end, bool isPlus)
{
  TCheckSpan res( begin(), CAgpErr::W_DuplicateComp );

  for(iterator it = begin();  it != end(); ++it) {
    // A high priority warning
    if( it->beg <= span_beg && span_beg <= it->end )
      return TCheckSpan(it, CAgpErr::W_SpansOverlap);

    // A lower priority error (would even be ignored for draft seqs)
    if( ( isPlus && span_beg < it->beg) ||
        (!isPlus && span_end > it->end)
    ) {
      res.first  = it;
      res.second = CAgpErr::W_SpansOrder;
    }
  }

  return res;
}

void CCompSpans::AddSpan(const CCompVal& span)
{
  push_back(span);
  //if(span.beg < beg) beg = span.beg;
  //if(end < span.end) end = span.end;
}


END_NCBI_SCOPE

