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

USING_NCBI_SCOPE;

BEGIN_NCBI_SCOPE

// Stores minimal and maximal values for a run of up to 15 digits.
// A more precise name might be be "CRunOfDigitsStatistics"
// We preserve leading zeroes by storing min/max values as strings.
// We also count how many times min and max values occur.
class CRunOfDigits
{
public:
  double min_val, max_val;
  string min_str, max_str;
  int min_val_count, max_val_count, total_count;

  // If these lengths are not equal,
  // then the following optimization must be suppressed:
  // AC[00001..0085] -> AC00[01..85]
  int min_len, max_len;

  CRunOfDigits()
  {
    min_val=kMax_Double; max_val=0;
    min_val_count=max_val_count=total_count=0;
    min_len=100; max_len=0;
  }

  void AddString(const string& s)
  {
    total_count++;

    double d=NStr::StringToDouble(s);
    if(d<min_val) {
      min_val=d;
      min_str=s;
      min_val_count=1;
    }
    else if(d==min_val) {
      min_val_count++;
    }

    if(d>max_val) {
      max_val=d;
      max_str=s;
      max_val_count=1;
    }
    else if(d==max_val) {
      max_val_count++;
    }

    int i=s.size();
    if(i<min_len) min_len=1;
    if(i>max_len) max_len=i;
  }

  // Returns a string represeting the range of values in this run of digits.
  // Possible formats:
  //   [min_val..max_val]
  //   min_val            when there was just one value in this run;
  //
  //   [min_val,max_val]  when there were exactly 2 values in this run;
  //   prefix[from..to]   | when min_val and max_val start with a common prefix
  //   prefix[from,to]    | and all submitted strings have the same length
  string GetString() const
  {
    if(min_val==max_val) return min_str;
    int prefix_len=0;
    if(min_len==max_len) {
      // Can optimize: // [0001..0085] -> 00[01..85]
      while( prefix_len<min_len &&
        min_str[prefix_len] == max_str[prefix_len]
      ) prefix_len++;
    }

    string sep=".."; // A range
    if(min_val_count+max_val_count==total_count) {
      sep=","; // Just 2 values
    }
    return min_str.substr(0,prefix_len) + "[" +
      min_str.substr(prefix_len) + sep +
      max_str.substr(prefix_len) + "]";
  }
};

typedef vector<CRunOfDigits> CRunsOfDigits;
class CPatternStats
{
public:
  typedef vector<string> TStrVec;

  int acc_count;
  CRunsOfDigits* runs;

  // runs_count is the number of continuous digit runs,
  // e.g. 2 for "AC01234.5". runs_count is the same
  // for all accessions of the same pattern.
  CPatternStats(int runs_count)
  {
    acc_count=0;
    runs = new CRunsOfDigits(runs_count);
  }
  ~CPatternStats()
  {
    delete runs;
  }

  void AddAccRuns(TStrVec& runs_str)
  {
    acc_count++;
    for( SIZE_TYPE i=0; i<runs_str.size(); i++ ) {
      (*runs)[i].AddString(runs_str[i]);
    }
  }

  // Replace "#" in a simple pattern like BCM_Spur_v#.#_Scaffold#
  // with digits or numerical [ranges].
  string ExpandPattern(const string& pattern) const
  {
    int i = 0;
    SIZE_TYPE pos = 0;
    string res = pattern;

    for(;;) {
      pos = NStr::Find(res, "#", pos);
      if(pos==NPOS) break;

      res.replace( pos, 1, (*runs)[i].GetString() );
      i++;
    }
    return res;
  }
};


/* Display accession naming patterns.
 * Replace all groups of digits in accessions
 * with numerical ranges; keep a digit at a given place
 * if only one digit is possible there.
 */
class CObjNamePatternCounter : public map<string, CPatternStats*>
{
public:
  void addObject(const string& name)
  {
    string s;
    s.reserve(name.size());

    // Replace runs of digits with # -> s;
    // collect those runs in digrun.
    bool prev_digit=false;
    vector<string> digrun;
    for(SIZE_TYPE i=0; i<name.size(); i++) {
      if( isdigit(name[i]) ) {
        if(!prev_digit) {
          s+="#";
          prev_digit=true;
          digrun.push_back(NcbiEmptyString);
        }
        digrun.back() += name[i];
      }
      else{
        prev_digit=false;
        s+=name[i];
      }
    }

    // Using the "#pattern" (s) as a key in this map,
    // add digrun values to the statistics.
    iterator it = find(s);
    CPatternStats* ps;
    if(it==end()) {
      ps = new CPatternStats( digrun.size() );
      insert( value_type(s, ps) );
    }
    else {
      ps = it->second;
    }

    ps->AddAccRuns(digrun);
  }


  // Replace "#" in a simple pattern like BCM_Spur_v#.#_Scaffold#
  // with digits or numerical [ranges].
  static string GetExpandedPattern(value_type* p)
  {
    return p->second->ExpandPattern(p->first);
  }

  static int GetCount(value_type* p)
  {
    return p->second->acc_count;
  }

  // >pointer to >value_type vector for sorting
  typedef vector<value_type*> pv_vector;

private:
  // For sorting by accession count
  static int x_byCount( value_type* a, value_type* b )
  {
    if( GetCount(a) != GetCount(b) ){
      return GetCount(a) < GetCount(b);
    }
    return a->first < b->first;
  }

public:
  void GetSortedValues(pv_vector& out)
  {
    out.clear(); out.reserve( size() );
    for(iterator it = begin();  it != end(); ++it) {
      out.push_back(&*it);
    }
    std::sort( out.begin(), out.end(), x_byCount );
  }

};

CAgpSyntaxValidator::CAgpSyntaxValidator()
{
  objNamePatterns = new CObjNamePatternCounter();

  prev_end = 0;
  prev_part_num = 0;
  prev_line_num = 0;
  prev_line_error_occured = false;
  componentsInLastScaffold = 0;
  post_prev = false;

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
  // m_ComponentTypeValues.insert("U"); // gap of unknown size
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
  delete objNamePatterns;
}

bool CAgpSyntaxValidator::ValidateLine(
  CNcbiOstrstream* msgStream, const SDataLine& dl,
  const string& text_line, bool last_validation)
{
  new_obj = false;
  obj_range_len = 0;

  int obj_begin = 0;
  int obj_end = 0;
  int part_num = 0;

  // for compatibility with AGP_WARNING() AGP_ERROR()
  m_LineErrorOccured = false;
  m_ValidateMsg=msgStream;

  //// Common code for GAPs, components, and one last check at EOF.
  if(last_validation) {
    new_obj = true;  // to check the prev_gap_type below
    if(componentsInLastScaffold==1) m_SingletonCount++;
  }
  else if(dl.object != prev_object) {
    new_obj = true;

    prev_end = 0;
    prev_part_num = 0;
    prev_object = dl.object;
    m_ObjCount++;

    // 2006/08/28
    m_ScaffoldCount++;
    if(componentsInLastScaffold==1) m_SingletonCount++;
    componentsInLastScaffold=0;
  }

  if( new_obj && prev_component_type == "N" ) {
    // A new scafold.
    // Previous line is a gap at the end of a scaffold

    // special error reporting mechanism since the
    //  error is on the previous line. Directly Log
    //  the error messages.
    if(!prev_line_error_occured) {
      AGP_POST( prev_line_num << ": " << prev_line);
    }
    AGP_POST("\tWARNING: gap at the end of an object");
  }
  if(last_validation) return m_LineErrorOccured;

  //// Common code for GAPs and components
  if(new_obj) {
    TObjSetResult obj_insert_result = m_ObjIdSet.insert(dl.object);
    if (obj_insert_result.second == false) {
      AGP_ERROR("Duplicate object: " << dl.object);
    }

    objNamePatterns->addObject(dl.object);
  }

  obj_begin = x_CheckIntField(
    dl.line_num, dl.begin, "object_begin"
  );
  if( obj_begin && ( obj_end = x_CheckIntField(
        dl.line_num, dl.end, "object_end"
  ))){
    if(new_obj && obj_begin != 1) {
      AGP_ERROR("First line of an object must have "
        "object_begin=1");
    }

    obj_range_len = x_CheckRange(
      dl.line_num, prev_end, obj_begin, obj_end,
      "object_begin", "object_end");
    prev_end = obj_end;
  }

  if (part_num = x_CheckIntField(
    dl.line_num, dl.part_num, "part_num"
  )) {
    if(part_num != prev_part_num+1) {
      post_prev=true;
      AGP_ERROR("Part number (column 4) != previous "
        "part number +1");
    }
    prev_part_num = part_num;
  }

  x_CheckValues( dl.line_num, m_ComponentTypeValues,
    dl.component_type,"component_type");

  //// Gap- or component-specific code.
  if (dl.component_type == "N") {
    x_OnGapLine(dl, text_line);
  }
  else {
    x_OnComponentLine(dl, text_line);
  }

  ////
  prev_component_type = dl.component_type;
  prev_line = text_line;
  prev_line_num = dl.line_num;
  //prev_line_filename = m_app->m_CurrentFileName;
  prev_line_error_occured = m_LineErrorOccured;
  //prev_component_id = dl.component_id;

  return m_LineErrorOccured; // may be set in AGP_WARNING, AGP_ERROR
}

void CAgpSyntaxValidator::x_OnGapLine(
  const SDataLine& dl, const string& text_line)
{
  int gap_len = 0;
  bool error= false;
  m_GapCount++;

  //// Check the line: gap length, values in gap_type and linkage
  if(gap_len = x_CheckIntField(
    dl.line_num, dl.gap_length, "gap_length"
  )) {
    if (obj_range_len && obj_range_len != gap_len) {
      AGP_ERROR("Object range length not equal to gap length"
        ": " << obj_range_len << " != " << gap_len);
      error = true;
    }
  }
  else {
    error = true;
  }

  int gap_type = x_CheckValues(
    dl.line_num, m_GapTypes, dl.gap_type, "gap_type");
  int linkage = -1;
  bool endsScaffold = true;
  if( gap_type>=0 ) {
    linkage = x_CheckValues(
        dl.line_num, m_LinkageValues, dl.linkage, "linkage");

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
          AGP_WARNING("Invalid linkage=yes for gap_type="<<dl.gap_type);
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

  //// Check gap context: is it a start of a new object,
  //// does it follow another gap, is it the end of a scaffold
  if(new_obj) { AGP_WARNING("Object begins with a gap"); }
  if(prev_component_type == "N") {
    // Previous line a gap.
    //AGP_POST( prev_line_num << ": " << prev_line);
    post_prev=true;
    AGP_WARNING("Two consequtive gap lines. There may be a gap at the end of "
    "a scaffold, two non scaffold-breaking gaps, etc");

    /*
    // Check the gap_type.
    if(prev_gap_type == "fragment") {
      // two gaps in a row

      if (!new_obj) {
        if (dl.gap_type == "fragment") {
          AGP_WARNING("Two fragment gaps in a row.");
        }
        else {
          // Current gap type is a scaffold boundary

          // special error reporting mechanism since
          // the error is on the previous line.
          // Directly Log the error messages.
          if(!prev_line_error_occured) {
            AGP_POST( prev_line_num << ": " << prev_line);
          }
          AGP_POST("\tWARNING: "
            "Next line is a scaffold-ending gap. "
            "Current line is a fragment gap. "
            "A Scaffold should not end with a gap."
          );
        }
      }
    }
    else {
      if (!new_obj) {
        // Previous line is a a scafold gap
        // This line is the start of a new scaffold
        if (dl.gap_type == "fragment") {
          // Scaffold starts with a fragment gap.
          AGP_WARNING("Scaffold should not begin with a gap."
            "(Previous line was a scaffold-ending gap.)" );
        }
        else {
          // Current gap type is a scaffold boundary.
          //  Two scaffold gaps in a row.
          AGP_WARNING( "Two scaffold-ending gaps in a row.");
        }
      }
    }
    */
  }
  else if( endsScaffold ) {
    // 2006/08/28: Previous line is the last component of a scaffold
    m_ScaffoldCount++;
    if(componentsInLastScaffold==1) m_SingletonCount++;
  }
  if( endsScaffold ) {
    componentsInLastScaffold=0;
  }

  //// Check if the line looks more like a component
  //// (i.e. dl.component_type should be != "N")
  if (error) {
    // A component line has integers in column 7
    // (component start) and column 8 (component end);
    // +, - or 0 in column 9 (orientation).
    if( x_CheckIntField(
          dl.line_num, dl.component_start,
          "component_start", NO_LOG
        ) && x_CheckIntField(
          dl.line_num, dl.component_end,
          "component_end", NO_LOG
        ) && x_CheckValues(
          dl.line_num, m_OrientaionValues,
          dl.orientation, "orientation", NO_LOG
    ) ) {
      AGP_WARNING( "Line with component_type=N appears to be"
        " a component line and not a gap line.");
    }
  }

  prev_gap_type = gap_type;
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

  //// Check that component begin & end are integers,
  //// begin < end, component span length == that of the object
  if( (comp_start = x_CheckIntField(
        dl.line_num,dl.component_start,"component_start"
      )) &&
      (comp_end   = x_CheckIntField(
        dl.line_num, dl.component_end ,"component_end"
      ))
  ) {
    comp_len = x_CheckRange(
      dl.line_num, 0, comp_start, comp_end,
      "component_start", "component_end"
    );
    if( comp_len && obj_range_len &&
        comp_len != obj_range_len
    ) {
      AGP_ERROR( "Object range length not equal to component"
        " range length");
      error = true;
    }
  } else {
    error = true;
  }

  //// Orientation: + - 0 na
  if (x_CheckValues(
    dl.line_num, m_OrientaionValues, dl.orientation,
    "orientation"
  )) {
    // todo: avoid additional string comparisons;
    // use array for counting; count invalid orientations

    if( dl.orientation == "0") {
      AGP_ERROR( "Component cannot have an unknown orientation"
      );
      m_CompZeroCount++;
      error = true;
    }
    else if (dl.orientation == "+") {
      m_CompPosCount++;
    }
    else if (dl.orientation == "-") {
      m_CompNegCount++;
    }
    else {
      m_CompNaCount++;
    }
  } else {
    error = true;
  }

  //// Check that component spans do not overlap
  //// and are in correct order
  if( comp_start >= comp_end ) {
    int tmp = comp_end; comp_end = comp_start; comp_start = tmp;
  }
  CRange<TSeqPos>  component_range(comp_start, comp_end);
  TCompIdSpansPair value_pair(
    dl.component_id, TCompSpans(component_range)
  );
  pair<TCompId2Spans::iterator, bool> id_insert_result =
     m_CompId2Spans.insert(value_pair);
  if (id_insert_result.second == false) {
    TCompSpans& collection = (id_insert_result.first)->second;

    // Need not check the span order for these:
    bool isDraft =
      dl.component_type=="A" || // Active Finishing
      dl.component_type=="D" || // Draft HTG
      dl.component_type=="P";   // Pre Draft
    string str_details="";

    if(collection.IntersectingWith(component_range)) {
      // To do: (try to) print both lines with overlapping spans
      str_details = "The span overlaps "
        "a previous span for this component.";
    }
    else if ( !isDraft) {
      if( ( dl.orientation=="+" &&
            comp_start > (int)collection.GetToOpen() ) ||
          ( dl.orientation=="-" &&
            comp_end  < (int)collection.GetFrom() )
      ) {
        // A correct order.
      }
      else {
        str_details = "Component span is out of order.";;
      }
    }

    if(!isDraft) {
      /* Need better means of finding a probable offending line(s).
      if(prev_component_id==dl.component_id) {
        post_prev=true;
      }
      */
      AGP_WARNING("Duplicate component id found.");
    }
    if( str_details.size() ) {
      AGP_WARNING(str_details);
    }

    collection.CombineWith(component_range);
  }

  //// Check if the line looks more like a gap
  //// (i.e. dl.component_type should be "N")
  if(error) {
    // Gap line has integer (gap len) in column 6,
    // gap type value in column 7,
    // a yes/no in column 8.
    if(x_CheckIntField(
        dl.line_num, dl.gap_length, "gap_length",
      NO_LOG) && x_CheckValues(
        dl.line_num, m_GapTypes, dl.gap_type, "gap_type",
      NO_LOG) && x_CheckValues(
        dl.line_num, m_LinkageValues, dl.linkage, "linkage",
      NO_LOG)
    ) {
      AGP_WARNING( "Line with component_type="
        << dl.component_type <<" appears to be a gap line "
        "and not a component line");
    }
  }
}

int CAgpSyntaxValidator::x_CheckIntField(
  int line_num, const string& field,
  const string& field_name, bool log_error)
{
  int field_value = 0;
  try {
      field_value = NStr::StringToInt(field);
  } catch (...) {
  }

  if (field_value <= 0  &&  log_error) {
    AGP_ERROR( field_name << " field must be a positive "
    "integer");
  }
  return field_value;
}

int CAgpSyntaxValidator::x_CheckRange(
  int line_num, int start, int begin, int end,
  string begin_name, string end_name)
{
  int length = 0;
  if(begin <= start){
    /* Need better means of finding a probable offending line(s).
    post_prev=true;
    */
    AGP_ERROR( begin_name << " field overlaps the previous "
      "line");
  }
  else if (end < begin) {
    AGP_ERROR(end_name << " is less than " << begin_name );
  }
  else {
    length = end - begin + 1;
  }

  return length;
}

bool CAgpSyntaxValidator::x_CheckValues(
  int line_num,
  const TValuesSet& values,
  const string& value,
  const string& field_name,
  bool log_error)
{
  if (values.count(value) == 0) {
    if (log_error)
      AGP_ERROR("Invalid value for " << field_name);
    return false;
  }
  return true;
}

int CAgpSyntaxValidator::x_CheckValues(
  int line_num,
  const TValuesMap& values,
  const string& value,
  const string& field_name,
  bool log_error)
{
  TValuesMap::const_iterator it = values.find(value);
  if( it==values.end() ) {
    if (log_error)
      AGP_ERROR("Invalid value for " << field_name);
    return -1;
  }
  return it->second;
}

void CAgpSyntaxValidator::PrintTotals()
{
  AGP_POST("\n"
    "Objects     : " << m_ObjCount << "\n"
    "Scaffolds   : " << m_ScaffoldCount   << "\n"
    "  singletons: " << m_SingletonCount << "\n\n"
    "Unique Component Accessions: "<< m_CompId2Spans.size() <<"\n"
    "Lines with Components      : " << m_CompCount        << "\n"
    "\torientation +      : " << m_CompPosCount   << "\n"
    "\torientation -      : " << m_CompNegCount   << "\n"
    "\torientation 0      : " << m_CompZeroCount  << "\n"
    "\torientation na     : "<< m_CompNaCount    << "\n\n"

 << "Gaps: " << m_GapCount
 << "\n\t   with linkage: yes\tno"
 << "\n\tclone          : "<<m_TypeGapCnt["cloneyes"          ]
 << "\t"                   <<m_TypeGapCnt["cloneno"           ]
 << "\n\tfragment       : "<<m_TypeGapCnt["fragmentyes"       ]
 << "\t"                   <<m_TypeGapCnt["fragmentno"        ]
 << "\n\trepeat         : "<<m_TypeGapCnt["repeatyes"         ]
 << "\t"                   <<m_TypeGapCnt["repeatno"          ]
 << "\n"
 << "\n\tcontig         : "<<m_TypeGapCnt["contigyes"         ]
 << "\t"                   <<m_TypeGapCnt["contigno"          ]
 << "\n\tcentromere     : "<<m_TypeGapCnt["centromereyes"     ]
 << "\t"                   <<m_TypeGapCnt["centromereno"      ]
 << "\n\tshort_arm      : "<<m_TypeGapCnt["short_armyes"      ]
 << "\t"                   <<m_TypeGapCnt["short_armno"       ]
 << "\n\theterochromatin: "<<m_TypeGapCnt["heterochromatinyes"]
 << "\t"                   <<m_TypeGapCnt["heterochromatinno" ]
 << "\n\ttelomere       : "<<m_TypeGapCnt["telomereyes"       ]
 << "\t"                   <<m_TypeGapCnt["telomereno"        ]
 //<< "\n\tSplit_finished : "<<m_TypeGapCnt["split_finishedyes" ]
 //<< "\t"                   <<m_TypeGapCnt["split_finishedno"  ]
  );

  AGP_POST("\nObject names and counts:");
  // TO DO:
  //   print not more than 50, not more than 10 with acc.count==1;
  //   instead of the omiited patterns, print:
  //     how many patterns were omitted,
  //     how many accessions they contain.
  /*
  for(CObjNamePatternCounter::iterator it = objNamePatterns->begin();
      it != objNamePatterns->end(); ++it
  ) {
    AGP_POST( "\t"
      << it->second->ExpandPattern( it->first)
      << "\t" << it->second->acc_count << "\n"
    );
  }
  */

  // Get a vector with sorted map values
  CObjNamePatternCounter::pv_vector pat_cnt;
  objNamePatterns->GetSortedValues(pat_cnt);

  for(CObjNamePatternCounter::pv_vector::iterator it =
      pat_cnt.begin(); it != pat_cnt.end(); ++it
  ) {
    AGP_POST( "\t"
      << CObjNamePatternCounter::GetExpandedPattern(*it) << "\t"
      << CObjNamePatternCounter::GetCount(*it) << "\n"
    );
  }
}

const string& CAgpSyntaxValidator::PreviousLineToPrint()
{
  if(post_prev) {
    post_prev=false;
    return prev_line;
  }
  return NcbiEmptyString;
}

END_NCBI_SCOPE

