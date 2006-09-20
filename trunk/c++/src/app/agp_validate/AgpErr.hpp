#ifndef AGP_VALIDATE_AgpErr
#define AGP_VALIDATE_AgpErr

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
 *      Error and warning messages produced by AGP validator.
 *      CAgpErr: print errors and related AGP lines;
 *      supress repetitive messages and the ones
 *      that user requested to suppress.
 *
 */

#include <corelib/ncbistd.hpp>
#include <iostream>

BEGIN_NCBI_SCOPE

// Print errors for this and previous lines in the format:
//   [filename:]linenum:agp lline
//           WARNING: text
//           ERROR: text
// Skip selected and repetitive errors; count skipped errors.

/* Basic usage:
   CAgpErr agpErr;

   // Apply options (normally specified in the command-line)
   agpErr.m_MaxRepeat = 0; // Print all
   agpErr.SkipMsg("warn"); // Skip warnings

   // Loop files
   for(...) {
     // Do not call if reading stdin, or only one file
     agpErr.StartFile(filename);
     // Loop lines
     for(...) {
       . . .
       // The order of the calls below is not very significant:
       if(error_in_the_previous_line) agpErr.Msg(code, "", CAgpErr::AT_PrevLine);
       if(error_in_this_line        ) agpErr.Msg(code, "", CAgpErr::AT_ThisLine);
       if(error_in_the_last_pair_of_lines) // E.g. 2 conseq. gap lines
         agpErr.Msg(code, "", CAgpErr::AT_ThisLine|CAgpErr::AT_PrevLine);
       . . .
     }
   }
   agpErr.StartFile(""); // Done (make it a separate function?)
*/

class CAgpErr
{
public:
  enum TCode {
    E_DuplicateObj  , E_ObjMustBegin1 , E_PartNumberNotPlus1,
    E_ObjRangeNeGap , E_ObjRangeNeComp, E_UnknownOrientation,
    E_MustBePositive, E_Overlaps      , E_ObjBeginLtEnd     ,
    E_CompStartLtEnd, E_InvalidValue  ,
    E_Last, E_First=0,

    W_GapObjEnd=20 , W_GapObjBegin , W_ConseqGaps ,
    W_InvalidYes   , W_SpansOverlap, W_SpansOrder ,
    W_DuplicateComp, W_LooksLikeGap, W_LooksLikeComp,
    W_Last, W_First = 20
  };

  static const char* GetMsg(TCode code);
  static string GetPrintableCode(int code); // Returns a string like e01 w12
  static void PrintAllMessages(CNcbiOstream& out);


  // The max number of times to print a given error message.
  int m_MaxRepeat;

  // Print the source line along with filename and line number.
  static void PrintLine   (CNcbiOstream& ostr,
    const string& filename, int linenum, const string& content);

  // Print the message by code, prepended by \tERROR: or \tWARNING:
  // details: append at the end of the message
  // substX : substitute it instead of the word "X" in msg[code]
  static void PrintMessage(CNcbiOstream& ostr, TCode code,
    const string& details=NcbiEmptyString,
    const string& substX=NcbiEmptyString);

  // Construct a readable message on total error & warning counts
  static void PrintErrorCounts(CNcbiOstream& ostr, int e_count, int w_count);

  CAgpErr();

  enum TAppliesTo{
    //AT_Unknown=0, -- just print the message without the content line
    AT_ThisLine=1, // Accumulate messages
    AT_PrevLine=2  // Print the previous line if it was not printed;
                   // print the message now
   // AT_ThisLine|AT_PrevLine: both lines are involved;
   // print the previous line now (if it was not printed already)
  };

  // Can skip unwanted messages, record a message for printing (AT_ThisLine),
  // print it immediately if it applies to the previous line (AT_PrevLine),
  // print the previous line and record the message
  // if it applies to the 2 last lines (AT_PrevLine|AT_ThisLine).
  //
  // The resulting output format shines when:
  // 1)there are multiple errors in one line:
  //   file:linenum:agp content
  //        MSG1
  //        MSG2
  // 2)there is an error that involves 2 lines:
  //   file1:linenum1:agp content1
  //   file2:linenum2:agp content2
  //        MSG
  // It should still be acceptable if both 1 and 2 are true.
  //
  // When the message applies to 2 non-consequitive lines
  // (e.g. intersecting component spans):
  //   // Print the first line involved
  //   if( !agpErr.MustSkip(code) ) CAgpErr::PrintLine(...);
  //   // Print the current line, then the message
  //   agpErr.Msg(..AT_ThisLine..);
  //
  // The worst mixup could happen when there are 2 errors involving
  // different pairs of lines, typically: (N-1, N) (N-M, N)
  // To do: check if this is possible in the current SyntaxValidator

  void Msg(TCode code, const string& details=NcbiEmptyString,
    int appliesTo=AT_ThisLine, const string& substX=NcbiEmptyString);

  // Print any accumulated messages.
  void LineDone(const string& s, int line_num);

  // Invoke with "" when done reading all files or cin.
  // NO other invokations are needed when reading from cin,
  // or when reading only one file. For multiple files,
  // invoke with the next filename prior to reading it.
  void StartFile(const string& s);

  // 'fgrep' errors out, or keep just the given errors (skip_other=true)
  // Can also include/exclude by code -- see GetPrintableCode().
  // Returns a message detailing what would be skipped
  // (or saying that no matches were found).
  // Note: call SkipMsg("all") nefore SkipMsg(smth, true)
  string SkipMsg(const string& str, bool skip_other=false);

  bool MustSkip(TCode code);

  // To do: some function to print individual error counts
  //        (important if we skipped some errors)

  // One argument:
  //   E_Last: count errors  W_Last: count warnings
  //   other: errors/warnings of one given type
  // Two arguments: range of TCode-s
  int CountErrors(TCode from, TCode to=E_First);

private:
  typedef const char* TStr;
  static const TStr msg[];

  // Total # of errors for each type, including skipped ones.
  int m_MsgCount[W_Last];
  bool m_MustSkip[W_Last];

  string m_filename_prev;
  // Not m_line_num-1 when the previous line:
  // - was in the different file;
  // - was a skipped comment line.
  string m_line_prev;
  int  m_line_num_prev;
  bool m_prev_printed;    // true: previous line was already printed
                          // (probably had another error);
                          // no need to-reprint "fname:linenum:content"

  // a stream to Accumulate messages for the current line.
  // (We print right away messages that apply only to the previous line.)
  CNcbiOstrstream* m_messages;
  string m_filename;
  int m_line_num;

};

END_NCBI_SCOPE

#endif /* AGP_VALIDATE_AgpErr */

