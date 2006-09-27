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
 *      information in the AGP file.
 *
 *
 */

#include <ncbi_pch.hpp>
#include "SyntaxValidator.hpp"
#include "AccessionPatterns.hpp"

USING_NCBI_SCOPE;
BEGIN_NCBI_SCOPE

int agp_error_count=0;
int agp_warn_count=0;

CAgpSyntaxValidator::CAgpSyntaxValidator()
{
  //objNamePatterns = new CAccPatternCounter();

  prev_end = 0;
  prev_part_num = 0;
  // prev_line_num = 0;
  // prev_line_error_occured = false;
  componentsInLastScaffold = 0;
  prev_orientation_unknown=false;

  m_ObjCount = 0;
  m_ScaffoldCount = 0;
  m_SingletonCount = 0;
  m_CompCount = 0;
  m_CompPosCount = 0;
  m_CompNegCount = 0;
  m_CompZeroCount = 0;
  m_CompNaCount = 0;
  m_GapCount = 0;

  m_TypeGapCnt["contigyes"] = 0;
  m_TypeGapCnt["contigno"] = 0;
  m_TypeGapCnt["cloneyes"] = 0;
  m_TypeGapCnt["cloneno"] = 0;
  m_TypeGapCnt["fragmentyes"] = 0;
  m_TypeGapCnt["fragmentno"] = 0;
  m_TypeGapCnt["repeatyes"] = 0;
  m_TypeGapCnt["repeatno"] = 0;

  //m_TypeGapCnt["split_finishedyes"] = 0;
  //m_TypeGapCnt["split_finishedno"] = 0;
  m_TypeGapCnt["centromereyes"] = 0;
  m_TypeGapCnt["centromereno"] = 0;
  m_TypeGapCnt["short_armyes"] = 0;
  m_TypeGapCnt["short_armno"] = 0;
  m_TypeGapCnt["heterochromatinyes"] = 0;
  m_TypeGapCnt["heterochromatinno"] = 0;
  m_TypeGapCnt["telomereyes"] = 0;
  m_TypeGapCnt["telomereno"] = 0;

  m_TypeGapCnt["invalid gap type"]=0;

  // initialize values
  m_GapTypes["contig"         ] = GAP_contig         ;
  m_GapTypes["clone"          ] = GAP_clone          ;
  m_GapTypes["fragment"       ] = GAP_fragment       ;
  m_GapTypes["repeat"         ] = GAP_repeat         ;

  m_GapTypes["centromere"     ] = GAP_centromere     ;
  m_GapTypes["short_arm"      ] = GAP_short_arm      ;
  m_GapTypes["heterochromatin"] = GAP_heterochromatin;
  m_GapTypes["telomere"       ] = GAP_telomere       ;

  //m_GapTypes["scaffold"]=GAP_scaffold);
  //m_GapTypes["split_finished"]=GAP_split_finished;

  m_ComponentTypeValues.insert("A");
  m_ComponentTypeValues.insert("D");
  m_ComponentTypeValues.insert("F");
  m_ComponentTypeValues.insert("G");
  m_ComponentTypeValues.insert("P");
  m_ComponentTypeValues.insert("N"); // gap
  m_ComponentTypeValues.insert("O");
  m_ComponentTypeValues.insert("U"); // gap of unknown size
  m_ComponentTypeValues.insert("W");

  m_OrientaionValues.insert("+");
  m_OrientaionValues.insert("-");
  m_OrientaionValues.insert("0");
  m_OrientaionValues.insert("na");

  m_LinkageValues["no" ] = LINKAGE_no;
  m_LinkageValues["yes"] = LINKAGE_yes;
}

CAgpSyntaxValidator::~CAgpSyntaxValidator()
{
  //delete objNamePatterns;
}

void CAgpSyntaxValidator::EndOfObject()
{
  if(componentsInLastScaffold==1) m_SingletonCount++;
  componentsInLastScaffold=0;

  if( IsGapType(prev_component_type) ) agpErr.Msg(
    // The previous line was a gap at the end of a scaffold & object
    CAgpErr::W_GapObjEnd, NcbiEmptyString,
    AT_ThisLine|AT_PrevLine
  );
}

void CAgpSyntaxValidator::ValidateLine( const SDataLine& dl,
  const string& text_line)
{
  new_obj = false;
  obj_range_len = 0;

  int obj_begin = 0;
  int obj_end = 0;
  int part_num = 0;

  // for compatibility with AGP_WARNING() AGP_ERROR()
  m_LineErrorOccured = false;

  //// Common code for GAPs and components
  if(dl.object != prev_object) {
    new_obj = true;

    prev_end = 0;
    prev_part_num = 0;
    prev_object = dl.object;
    m_ObjCount++;

    m_ScaffoldCount++; // to do: detect or avoid scaffold with 0 components?
    EndOfObject();

    TObjSetResult obj_insert_result = m_ObjIdSet.insert(dl.object);
    if (obj_insert_result.second == false) {
      agpErr.Msg(CAgpErr::E_DuplicateObj, dl.object,
        AT_ThisLine|AT_PrevLine);
      //AGP_ERROR("Duplicate object: " << );
    }

    //objNamePatterns->AddName(dl.object);
  }

  obj_begin = x_CheckIntField(
    dl.begin, "object_begin (column 2)" );
  if( obj_begin && ( obj_end = x_CheckIntField(
        dl.end, "object_end (column 3)"
  ))){
    if(new_obj && obj_begin != 1) {
      agpErr.Msg(CAgpErr::E_ObjMustBegin1,
        NcbiEmptyString,
        AT_ThisLine|AT_PrevLine
      );
      //AGP_ERROR("First line of an object must have object_begin=1");
    }

    obj_range_len = x_CheckRange(
      prev_end, obj_begin, obj_end,
      "object_begin", "object_end", CAgpErr::E_ObjBeginLtEnd);
    prev_end = obj_end;
  }

  if (part_num = x_CheckIntField(
    dl.part_num, "part_num (column 4)"
  )) {
    if(part_num != prev_part_num+1) {
      agpErr.Msg( CAgpErr::E_PartNumberNotPlus1, NcbiEmptyString,
        AT_ThisLine|AT_PrevLine );
      // post_prev=true;
      // AGP_ERROR("Part number (column 4) != previous part number +1");
    }
    prev_part_num = part_num;
  }

  if(x_CheckValues(
    m_ComponentTypeValues,
    dl.component_type, "component_type (column 5)"
  )) {
    m_TypeCompCnt.add( dl.component_type );
  }
  else {
    m_TypeCompCnt.add( "invalid type" );
  }

  //// Gap- or component-specific code.
  if( IsGapType(dl.component_type) ) {
    x_OnGapLine(dl, text_line);
  }
  else {
    x_OnComponentLine(dl, text_line);
    // prev_component_line = text_line;
    // prev_component_line_num = dl.line_num;
  }

  ////
  prev_component_type = dl.component_type;
  // prev_line = text_line;
  // prev_line_error_occured = m_LineErrorOccured;
}

void CAgpSyntaxValidator::x_OnGapLine(
  const SDataLine& dl, const string& text_line)
{
  int gap_len = 0;
  bool error= false;
  m_GapCount++;

  //// Check the line: gap length, values in gap_type and linkage
  if(gap_len = x_CheckIntField(
    dl.gap_length, "gap_length (column 6)"
  )) {
    if (obj_range_len && obj_range_len != gap_len) {
      agpErr.Msg(CAgpErr::E_ObjRangeNeGap, string(": ") +
        NStr::IntToString(obj_range_len) + " != " +
        NStr::IntToString(gap_len      ) );
      // AGP_ERROR("Object range length not equal to gap length"
      //  ": " << obj_range_len << " != " << gap_len);
      error = true;
    }
  }
  else {
    error = true;
  }

  int gap_type = x_CheckValues(
    m_GapTypes, dl.gap_type, "gap_type (column 7)");
  int linkage = -1;
  bool endsScaffold = true;
  if( gap_type>=0 ) {
    linkage = x_CheckValues(
        m_LinkageValues, dl.linkage, "linkage (column 8)");

    if(linkage>=0) {
      string key = dl.gap_type + dl.linkage;
      m_TypeGapCnt[key]++;

      //// Check whether  gap_type and linkage combination is valid,
      //// and if this combination ends the scaffold.
      if(gap_type==GAP_fragment) {
        endsScaffold=false;
      }
      else if(linkage==LINKAGE_yes) {
        endsScaffold=false;
        if( gap_type==GAP_clone || gap_type==GAP_repeat ) {
          // Ok
        }
        else {
          agpErr.Msg(CAgpErr::W_InvalidYes, dl.gap_type);
          //AGP_WARNING("Invalid linkage=yes for gap_type="<<dl.gap_type);
          error=true;
        }
      }
      // else: endsScaffold remains true
    }
  }
  else {
    m_TypeGapCnt["invalid gap type"]++;
    error = true;
  }

  //// Check the gap context: is it a start of a new object,
  //// does it follow another gap, is it the end of a scaffold.
  if(new_obj) {
    agpErr.Msg(CAgpErr::W_GapObjBegin, NcbiEmptyString,
      AT_ThisLine|AT_PrevLine);
    //AGP_WARNING("Object begins with a gap");
  }
  else if(IsGapType(prev_component_type)) {
    agpErr.Msg( CAgpErr::W_ConseqGaps, NcbiEmptyString,
      AT_ThisLine|AT_PrevLine );
    //post_prev=true;
    //AGP_WARNING("Two consequtive gap lines. There may be a gap at the end of "
    //"a scaffold, two non scaffold-breaking gaps, etc");
  }
  else if( endsScaffold ) {
    // A breaking gap after a component.
    // Previous line is the last component of a scaffold
    m_ScaffoldCount++; // to do: detect or do not count scaffold with 0 components?
    if(componentsInLastScaffold==1) {
      m_SingletonCount++;
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
  if( endsScaffold ) {
    componentsInLastScaffold=0;
  }

  //// Check if the line looks more like a component
  //// (i.e. dl.component_type should be != "N" or "U")
  if (error) {
    // A component line would have integers in column 7
    // (component start) and column 8 (component end);
    // +, - or 0 in column 9 (orientation).
    if( x_CheckIntField(
          dl.component_start,
          "component_start", NO_LOG
        ) && x_CheckIntField(
          dl.component_end,
          "component_end", NO_LOG
        ) && x_CheckValues(
          m_OrientaionValues,
          dl.orientation, "orientation", NO_LOG
    ) ) {
      agpErr.Msg(CAgpErr::W_LooksLikeComp,
        NcbiEmptyString, AT_ThisLine, // defaults
        dl.component_type );
      // AGP_WARNING( "Line with component_type=" << dl.component_type
      //  << " appears to be a component line and not a gap line.");
    }
  }

  //prev_gap_type = gap_type;
}

void CAgpSyntaxValidator::x_OnComponentLine(
  const SDataLine& dl, const string& text_line)
{
  bool error = false;
  int comp_start = 0;
  int comp_end = 0;
  int comp_len = 0;

  m_CompCount++;
  componentsInLastScaffold++;

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

  //// Check that component begin & end are integers,
  //// begin < end, component span length == that of the object
  if( (comp_start = x_CheckIntField(
        dl.component_start,
        "component_start (column 7)"
      )) &&
      (comp_end   = x_CheckIntField(
        dl.component_end,
        "component_end (column 8)"
      ))
  ) {
    comp_len = x_CheckRange(
      0, comp_start, comp_end,
      "component_start", "component_end", CAgpErr::E_CompStartLtEnd
    );
    if( comp_len && obj_range_len &&
        comp_len != obj_range_len
    ) {
      agpErr.Msg(CAgpErr::E_ObjRangeNeComp);
      // AGP_ERROR( "Object range length not equal to component range length");
      error = true;
    }
  } else {
    error = true;
  }

  //// Orientation: + - 0 na
  if (x_CheckValues(
    m_OrientaionValues, dl.orientation,
    "orientation (column 9)"
  )) {
    // todo: avoid additional string comparisons;
    // use array for counting; count invalid orientations

    if (dl.orientation == "+") {
      m_CompPosCount++;
    }
    else if (dl.orientation == "-") {
      m_CompNegCount++;
    }
    else {
      if( dl.orientation == "0") {
        m_CompZeroCount++;
        //AGP_ERROR( "Component cannot have an unknown orientation");
        //error = true;
      }
      else {
        m_CompNaCount++;
      }

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

  } else {
    error = true;
  }

  //// Check that component spans do not overlap
  //// and are in correct order
  if( comp_start >= comp_end ) {
    int tmp = comp_end; comp_end = comp_start; comp_start = tmp;
  }
  TSeqRange component_range(comp_start, comp_end);
  TCompIdSpansPair value_pair(
    dl.component_id, TCompSpans(component_range)
  );
  pair<TCompId2Spans::iterator, bool> id_insert_result =
     m_CompId2Spans.insert(value_pair);
  if (id_insert_result.second == false) {
    TCompSpans& spans = (id_insert_result.first)->second;

    // Do not check the span order for these:
    bool isDraft =
      dl.component_type=="A" || // Active Finishing
      dl.component_type=="D" || // Draft HTG
      dl.component_type=="P";   // Pre Draft
    string str_details="";

    if(spans.IntersectingWith(component_range)) {
      // To do: print both lines with overlapping spans
      agpErr.Msg(CAgpErr::W_SpansOverlap);
      //str_details = "The span overlaps "
      //  "a previous span for this component.";
    }
    else if ( !isDraft) {
      if( ( dl.orientation=="+" &&
            comp_start > (int)spans.GetToOpen() ) ||
          ( dl.orientation=="-" &&
            comp_end  < (int)spans.GetFrom() )
      ) {
        // A correct order.
      }
      else {
        agpErr.Msg(CAgpErr::W_SpansOrder);
        //str_details = "Component span appears out of order.";;
      }
    }

    if(!isDraft) {
      /* Need better means of finding a probable offending line(s).
      if(prev_component_id==dl.component_id) {
        post_prev=true;
      }
      */
      agpErr.Msg(CAgpErr::W_DuplicateComp);
      //AGP_WARNING("Duplicate component id with non-draft type");
    }
    // if( str_details.size() ) { AGP_WARNING(str_details); }

    spans.CombineWith(component_range);
  }

  //// Check if the line looks more like a gap
  //// (i.e. dl.component_type should be "N" or "U")
  if(error) {
    // Gap line has integer (gap len) in column 6,
    // gap type value in column 7,
    // a yes/no in column 8.
    if( x_CheckIntField(
        dl.gap_length, "gap_length", NO_LOG
      ) && x_CheckValues(
        m_GapTypes, dl.gap_type, "gap_type", NO_LOG
      ) && x_CheckValues(
        m_LinkageValues, dl.linkage, "linkage", NO_LOG
    ) ) {
      agpErr.Msg(CAgpErr::W_LooksLikeGap,
        NcbiEmptyString, AT_ThisLine, // defaults
        dl.component_type );
      //AGP_WARNING( "Line with component_type="
      //  << dl.component_type <<" appears to be a gap line "
      //  "and not a component line");
    }
  }
}

int CAgpSyntaxValidator::x_CheckIntField( const string& field,
  const string& field_name, bool log_error)
{
  int field_value = 0;
  try {
      field_value = NStr::StringToInt(field);
  } catch (...) {
  }

  if (field_value <= 0  &&  log_error) {
    agpErr.Msg(CAgpErr::E_MustBePositive,
        NcbiEmptyString, AT_ThisLine, // defaults
        field_name );
    //AGP_ERROR( field_name << " field must be a positive integer");
  }
  return field_value;
}

int CAgpSyntaxValidator::x_CheckRange(
  int start, int begin, int end,
  string begin_name, string end_name, CAgpErr::TCode ltCode)
{
  int length = 0;
  if(begin <= start){
    agpErr.Msg( CAgpErr::E_Overlaps,
      NcbiEmptyString,
      AT_ThisLine|AT_PrevLine,
      begin_name );
    //post_prev=true;
    //AGP_ERROR( begin_name << " field overlaps a previous line");
  }
  else if (end < begin) {
    agpErr.Msg(ltCode); // "Less Than" error Code
    //AGP_ERROR(end_name << " is less than " << begin_name );
  }
  else {
    length = end - begin + 1;
  }

  return length;
}

bool CAgpSyntaxValidator::x_CheckValues(const TValuesSet& values,
  const string& value, const string& field_name, bool log_error)
{
  if(values.count(value) == 0) {
    if(log_error) {
      agpErr.Msg(CAgpErr::E_InvalidValue,
        NcbiEmptyString, AT_ThisLine, // defaults
        field_name );
      //AGP_ERROR("Invalid value for " << field_name);
    }
    return false;
  }
  return true;
}

int CAgpSyntaxValidator::x_CheckValues(const TValuesMap& values,
  const string& value, const string& field_name, bool log_error)
{
  TValuesMap::const_iterator it = values.find(value);
  if( it==values.end() ) {
    if(log_error) {
      agpErr.Msg(CAgpErr::E_InvalidValue,
        NcbiEmptyString, AT_ThisLine, // defaults
        field_name );
      // AGP_ERROR("Invalid value for " << field_name);
    }
    return -1;
  }
  return it->second;
}

#define ALIGN_W(x) setw(w) << (x)
void CAgpSyntaxValidator::PrintTotals()
{
  //// Counts of errors and warnings
  int e_count=agpErr.CountTotals(CAgpErr::E_Last);
  int w_count=agpErr.CountTotals(CAgpErr::W_Last);
  cout << "\n";
  agpErr.PrintTotals(cout, e_count, w_count, agpErr.m_skipped_count);
  if(agpErr.m_MaxRepeatTopped) {
    cout << " (to print all: -limit 0)";
  }
  cout << ".";
  if(agpErr.m_MaxRepeat && (e_count+w_count) ) {
    cout << "\n";
    agpErr.PrintMessageCounts(cout, CAgpErr::E_First, CAgpErr::W_Last);
  }
  cout << "\n";

  //// Prepare component/gap types and counts for later printing
  string s_comp, s_gap;

  CValuesCount::pv_vector comp_cnt;
  m_TypeCompCnt.GetSortedValues(comp_cnt);

  for(CValuesCount::pv_vector::iterator
    it = comp_cnt.begin();
    it != comp_cnt.end();
    ++it
  ) {
    string *s = IsGapType((*it)->first) ? &s_gap : &s_comp;

    if( s->size() ) *s+= ", ";
    *s+= (*it)->first;
    *s+= ":";
    *s+= NStr::IntToString((*it)->second);
  }

  //// Various counts of AGP elements

  // w: width for right alignment
  int w = NStr::IntToString(m_CompId2Spans.size()).size();

  cout << "\n"
    "Objects     : " << ALIGN_W(m_ObjCount)       << "\n"
    "Scaffolds   : " << ALIGN_W(m_ScaffoldCount)  << "\n"
    "Singletons  : " << ALIGN_W(m_SingletonCount) << "\n\n";


  cout<<
    "Unique Component Accessions: "<< ALIGN_W(m_CompId2Spans.size()) <<"\n";

  cout<<
    "Lines with Components      : "<< ALIGN_W(m_CompCount);
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
    "\torientation +      : " << ALIGN_W(m_CompPosCount ) << "\n"
    "\torientation -      : " << ALIGN_W(m_CompNegCount ) << "\n"
    "\torientation 0      : " << ALIGN_W(m_CompZeroCount) << "\n"
    "\torientation na     : " << ALIGN_W(m_CompNaCount  ) << "\n";

  cout << "\n" << "Gaps: " << m_GapCount;
  // To do: print (N) if all components are of one type,
  //        or (N: 1234, U: 5678)
  if( s_gap.size() ) {
    if( NStr::Find(s_gap, ",")!=NPOS ) {
       // (N: 1234, N: 5678)
      cout << " (" << s_gap << ")";
    }
    else {
      // One type of gaps: (N)
      cout << " (" << s_gap.substr( 0, NStr::Find(s_gap, ":") ) << ")";
    }
  }

  cout
  << "\n\t   with linkage: "<<ALIGN_W("yes") << "\t" << ALIGN_W("no")
  << "\n\tclone          : "<<ALIGN_W(m_TypeGapCnt["cloneyes"          ])
  << "\t"                   <<ALIGN_W(m_TypeGapCnt["cloneno"           ])
  << "\n\tfragment       : "<<ALIGN_W(m_TypeGapCnt["fragmentyes"       ])
  << "\t"                   <<ALIGN_W(m_TypeGapCnt["fragmentno"        ])
  << "\n\trepeat         : "<<ALIGN_W(m_TypeGapCnt["repeatyes"         ])
  << "\t"                   <<ALIGN_W(m_TypeGapCnt["repeatno"          ])
  << "\n"
  << "\n\tcontig         : "<<ALIGN_W(m_TypeGapCnt["contigyes"         ])
  << "\t"                   <<ALIGN_W(m_TypeGapCnt["contigno"          ])
  << "\n\tcentromere     : "<<ALIGN_W(m_TypeGapCnt["centromereyes"     ])
  << "\t"                   <<ALIGN_W(m_TypeGapCnt["centromereno"      ])
  << "\n\tshort_arm      : "<<ALIGN_W(m_TypeGapCnt["short_armyes"      ])
  << "\t"                   <<ALIGN_W(m_TypeGapCnt["short_armno"       ])
  << "\n\theterochromatin: "<<ALIGN_W(m_TypeGapCnt["heterochromatinyes"])
  << "\t"                   <<ALIGN_W(m_TypeGapCnt["heterochromatinno" ])
  << "\n\ttelomere       : "<<ALIGN_W(m_TypeGapCnt["telomereyes"       ])
  << "\t"                   <<ALIGN_W(m_TypeGapCnt["telomereno"        ])
  //<< "\n\tSplit_finished : "<<m_TypeGapCnt["split_finishedyes" ]
  //<< "\t"                   <<m_TypeGapCnt["split_finishedno"  ]
  << "\n";

  cout << "\nObject names   :";
  // Patterns in object names
  {
    CAccPatternCounter objNamePatterns;
    objNamePatterns.AddNames(m_ObjIdSet);
    x_PrintPatterns(objNamePatterns, w);
  }

  cout << "Component names:";
  {
    CAccPatternCounter compNamePatterns;
    for(TCompId2Spans::iterator it = m_CompId2Spans.begin();
      it != m_CompId2Spans.end(); ++it)
    {
      compNamePatterns.AddName(it->first);
    }
    x_PrintPatterns(compNamePatterns, w);
  }

}

// Sort by accession count, print not more than MaxPatterns or 2*MaxPatterns
void CAgpSyntaxValidator::x_PrintPatterns(CAccPatternCounter& namePatterns, int w)
{
  const int MaxPatterns=10;

  // Get a vector with sorted pointers to map values
  CAccPatternCounter::pv_vector pat_cnt;
  namePatterns.GetSortedValues(pat_cnt);

  if(pat_cnt.size()==1) {
    // Continue on the same line, do not print counts
    cout<< " " << CAccPatternCounter::GetExpandedPattern( pat_cnt[0] )
        << "\n";
    return;
  }
  cout << "\n";

  // Print the patterns
  int patternsPrinted=0;
  int accessionsSkipped=0;
  for(CAccPatternCounter::pv_vector::iterator it =
      pat_cnt.begin(); it != pat_cnt.end(); ++it
  ) {
    // Limit the number of lines to MaxPatterns or 2*MaxPatterns
    if( ++patternsPrinted<=MaxPatterns || pat_cnt.size()<=2*MaxPatterns ) {
      cout<<  "\t"
          << ALIGN_W(CAccPatternCounter::GetCount(*it))
          << "  "
          << CAccPatternCounter::GetExpandedPattern(*it)
          << "\n";
    }
    else {
      accessionsSkipped += CAccPatternCounter::GetCount(*it);
    }
  }

  if(accessionsSkipped) {
    cout<<  "\t"
        << ALIGN_W(accessionsSkipped)
        << "  accessions between other " << pat_cnt.size() - 10
        << " patterns\n";
  }
}

bool CAgpSyntaxValidator::IsGapType(const string& type)
{
  return type=="N" || type=="U";
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

END_NCBI_SCOPE

