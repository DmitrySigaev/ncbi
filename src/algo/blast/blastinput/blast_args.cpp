/* $Id$
* ===========================================================================
*
*                            PUBLIC DOMAIN NOTICE
*               National Center for Biotechnology Information
*
*  This software/database is a "United States Government Work" under the
*  terms of the United States Copyright Act.  It was written as part of
*  the author's offical duties as a United States Government employee and
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
* ===========================================================================*/

/*****************************************************************************

File name: blast_args.cpp

Author: Jason Papadopoulos

******************************************************************************/

/** @file blast_args.cpp
 * convert blast-related command line
 * arguments into blast options
*/

#ifndef SKIP_DOXYGEN_PROCESSING
static char const rcsid[] = "$Id$";
#endif

#include <ncbi_pch.hpp>
#include <algo/blast/api/version.hpp>
#include <algo/blast/blastinput/blast_args.hpp>
#include <algo/blast/api/blast_exception.hpp>
#include <algo/blast/core/blast_nalookup.h>
#include <objtools/blast_format/blastfmtutil.hpp>
#include <objects/scoremat/PssmWithParameters.hpp>
#include <util/format_guess.hpp>
#include <objtools/readers/seqdb/seqdb.hpp>
#include "blast_input_aux.hpp"
#include <sstream>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(blast)
USING_SCOPE(objects);

/// Class to constrain the values of an argument to those greater than or equal
/// to the value specified in the constructor
class CArgAllowValuesGreaterThanOrEqual : public CArgAllow
{
public:
    /// Constructor taking an integer
    CArgAllowValuesGreaterThanOrEqual(int min) : m_MinValue(min) {}
    /// Constructor taking a double
    CArgAllowValuesGreaterThanOrEqual(double min) : m_MinValue(min) {}

protected:
    /// Overloaded method from CArgAllow
    virtual bool Verify(const string& value) const {
        return NStr::StringToDouble(value) >= m_MinValue;
    }

    /// Overloaded method from CArgAllow
    virtual string GetUsage(void) const {
        return ">=" + NStr::DoubleToString(m_MinValue);
    }
    
private:
    double m_MinValue;  /**< Minimum value for this object */
};

/// Class to constrain the values of an argument to those less than or equal
/// to the value specified in the constructor
class CArgAllowValuesLessThanOrEqual : public CArgAllow
{
public:
    /// Constructor taking an integer
    CArgAllowValuesLessThanOrEqual(int max) : m_MaxValue(max) {}
    /// Constructor taking a double
    CArgAllowValuesLessThanOrEqual(double max) : m_MaxValue(max) {}

protected:
    /// Overloaded method from CArgAllow
    virtual bool Verify(const string& value) const {
        return NStr::StringToDouble(value) <= m_MaxValue;
    }

    /// Overloaded method from CArgAllow
    virtual string GetUsage(void) const {
        return "<=" + NStr::DoubleToString(m_MaxValue);
    }
    
private:
    double m_MaxValue;  /**< Maximum value for this object */
};

/// Class to constrain the values of an argument to in the set provided in the
/// constructor
class CArgAllowIntegerSet : public CArgAllow
{
public:
    /// Constructor
    CArgAllowIntegerSet(const set<int>& values) : m_AllowedValues(values) {
        if (values.empty()) {
            throw runtime_error("Allowed values set must not be empty");
        }
    }

protected:
    /// Overloaded method from CArgAllow
    virtual bool Verify(const string& value) const {
        int value2check = NStr::StringToInt(value);
        ITERATE(set<int>, itr, m_AllowedValues) {
            if (*itr == value2check) {
                return true;
            }
        }
        return false;
    }

    /// Overloaded method from CArgAllow
    virtual string GetUsage(void) const {
        ostringstream os;
        os << "Permissible values: ";
        ITERATE(set<int>, itr, m_AllowedValues) {
            os << *itr << " ";
        }

        return os.str();
    }
    
private:
    /// Set containing the permissible values
    set<int> m_AllowedValues;
};

CProgramDescriptionArgs::CProgramDescriptionArgs(const string& program_name, 
                                                 const string& program_desc)
    : m_ProgName(program_name), m_ProgDesc(program_desc)
{}

void
CProgramDescriptionArgs::SetArgumentDescriptions(CArgDescriptions& arg_desc)
{
    // program description
    arg_desc.SetUsageContext(m_ProgName, m_ProgDesc + " v. " + 
                             blast::Version.Print() + " released " + 
                             blast::Version.GetReleaseDate());
}

void
CGenericSearchArgs::SetArgumentDescriptions(CArgDescriptions& arg_desc)
{
    arg_desc.SetCurrentGroup("General search options");

    // evalue cutoff
    arg_desc.AddDefaultKey(kArgEvalue, "evalue", 
                     "Expectation value (E) threshold for saving hits ",
                     CArgDescriptions::eDouble,
                     NStr::DoubleToString(BLAST_EXPECT_VALUE));

    // gap open penalty
    const int kGOpen = m_QueryIsProtein 
        ? BLAST_GAP_OPEN_PROT : BLAST_GAP_OPEN_MEGABLAST;
    arg_desc.AddDefaultKey(kArgGapOpen, "open_penalty", "Cost to open a gap", 
                           CArgDescriptions::eInteger, 
                           NStr::IntToString(kGOpen));

    // gap extend penalty
    const int kGExtn = m_QueryIsProtein 
        ? BLAST_GAP_EXTN_PROT : BLAST_GAP_EXTN_MEGABLAST;
    arg_desc.AddDefaultKey(kArgGapExtend, "extend_penalty",
                           "Cost to extend a gap",
                           CArgDescriptions::eInteger,
                           NStr::IntToString(kGExtn));

    // ungapped X-drop
    // Default values: blastn=20, megablast=10, others=7
    arg_desc.AddOptionalKey(kArgUngappedXDropoff, "float_value", 
                            "X dropoff value (in bits) for ungapped extensions",
                            CArgDescriptions::eDouble);

    // initial gapped X-drop
    // Default values: blastn=30, megablast=20, tblastx=0, others=15
    arg_desc.AddOptionalKey(kArgGappedXDropoff, "float_value", 
                 "X-dropoff value (in bits) for preliminary gapped extensions",
                 CArgDescriptions::eDouble);

    // final gapped X-drop
    // Default values: blastn/megablast=50, tblastx=0, others=25
    arg_desc.AddOptionalKey(kArgFinalGappedXDropoff, "float_value", 
                         "X dropoff value (in bits) for final gapped alignment",
                         CArgDescriptions::eDouble);

    // word size
    // Default values: blastn=11, megablast=28, others=3
    const string description = m_QueryIsProtein 
        ? "Word size for wordfinder algorithm"
        : "Word size for wordfinder algorithm (length of best perfect match)";
    arg_desc.AddOptionalKey(kArgWordSize, "int_value", description,
                            CArgDescriptions::eInteger);
    arg_desc.SetConstraint(kArgWordSize, m_QueryIsProtein 
                           ? new CArgAllowValuesGreaterThanOrEqual(2)
                           : new CArgAllowValuesGreaterThanOrEqual(4));

    arg_desc.AddOptionalKey(kArgTargetPercentIdentity, "float_value",
                            "Target percent identity",
                            CArgDescriptions::eDouble);
    arg_desc.SetConstraint(kArgTargetPercentIdentity,
                           new CArgAllow_Doubles(0.0, 1.0));

    // effective search space
    // Default value is the real size
    arg_desc.AddOptionalKey(kArgEffSearchSpace, "float_value", 
                            "Effective length of the search space",
                            CArgDescriptions::eInt8);
    arg_desc.SetConstraint(kArgEffSearchSpace, 
                           new CArgAllowValuesGreaterThanOrEqual(0));

    arg_desc.AddDefaultKey(kArgMaxHSPsPerSubject, "int_value",
                           "Maximum number of HPSs per subject to save "
                           "( " + NStr::IntToString(kDfltArgMaxHSPsPerSubject)
                           + " means infinite)",
                           CArgDescriptions::eInteger,
                           NStr::IntToString(kDfltArgMaxHSPsPerSubject));
    arg_desc.SetConstraint(kArgMaxHSPsPerSubject,
                           new CArgAllowValuesGreaterThanOrEqual(0));

    arg_desc.SetCurrentGroup("");
}

void
CGenericSearchArgs::ExtractAlgorithmOptions(const CArgs& args, 
                                            CBlastOptions& opt)
{
    if (args[kArgEvalue]) {
        opt.SetEvalueThreshold(args[kArgEvalue].AsDouble());
    }

    if (args[kArgGapOpen]) {
        opt.SetGapOpeningCost(args[kArgGapOpen].AsInteger());
    }

    if (args[kArgGapExtend]) {
        opt.SetGapExtensionCost(args[kArgGapExtend].AsInteger());
    }

    if (args[kArgUngappedXDropoff]) {
        opt.SetXDropoff(args[kArgUngappedXDropoff].AsDouble());
    }

    if (args[kArgGappedXDropoff]) {
        opt.SetGapXDropoff(args[kArgGappedXDropoff].AsDouble());
    }

    if (args[kArgFinalGappedXDropoff]) {
        opt.SetGapXDropoffFinal(args[kArgFinalGappedXDropoff].AsDouble());
    }

    if (args[kArgWordSize]) {
        opt.SetWordSize(args[kArgWordSize].AsInteger());
    }

    if (args[kArgEffSearchSpace]) {
        opt.SetEffectiveSearchSpace(args[kArgEffSearchSpace].AsInt8());
    }

    if (args[kArgTargetPercentIdentity]) {
        opt.SetPercentIdentity(args[kArgTargetPercentIdentity].AsDouble());
    }

    if (args[kArgMaxHSPsPerSubject]) {
        const int value = args[kArgMaxHSPsPerSubject].AsInteger();
        if (value != kDfltArgMaxHSPsPerSubject) {
            opt.SetMaxNumHspPerSequence(value);
        }
    }
}

void
CFilteringArgs::SetArgumentDescriptions(CArgDescriptions& arg_desc)
{
    arg_desc.SetCurrentGroup("Query filtering options");

    if (m_QueryIsProtein) {
        arg_desc.AddDefaultKey(kArgSegFiltering, "SEG_options",
                        "Filter query sequence with SEG "
                        "(Format: 'window locut hicut', or 'no' to disable)",
                        CArgDescriptions::eString, kDfltArgSegFiltering);
    } else {
        arg_desc.AddDefaultKey(kArgDustFiltering, "DUST_options",
                        "Filter query sequence with DUST "
                        "(Format: 'level window linker', or 'no' to disable)",
                        CArgDescriptions::eString, kDfltArgDustFiltering);
    }

    arg_desc.AddFlag(kArgLookupTableMaskingOnly,
                     "Apply filtering locations as soft masks?", true);
    arg_desc.AddOptionalKey(kArgFilteringDb, "filtering_database",
            "BLAST database containing filtering elements (i.e.: repeats)",
            CArgDescriptions::eString);

    arg_desc.SetCurrentGroup("");
}

void 
CFilteringArgs::x_TokenizeFilteringArgs(const string& filtering_args, 
                                        vector<string>& output) const
{
    output.clear();
    NStr::Tokenize(filtering_args, " ", output);
}

void
CFilteringArgs::ExtractAlgorithmOptions(const CArgs& args, CBlastOptions& opt)
{
    if (args[kArgLookupTableMaskingOnly]) {
        opt.SetMaskAtHash(true);
    }

    vector<string> tokens;

    if (m_QueryIsProtein && args[kArgSegFiltering]) {
        const string& seg_opts = args[kArgSegFiltering].AsString();
        if (seg_opts == "no") {
            opt.SetSegFiltering(false);
        } else {
            x_TokenizeFilteringArgs(seg_opts, tokens);
            opt.SetSegFilteringWindow(NStr::StringToInt(tokens[0]));
            opt.SetSegFilteringLocut(NStr::StringToDouble(tokens[1]));
            opt.SetSegFilteringHicut(NStr::StringToDouble(tokens[2]));
        }
    }

    if ( !m_QueryIsProtein && args[kArgDustFiltering]) {
        const string& dust_opts = args[kArgDustFiltering].AsString();
        if (dust_opts == "no") {
            opt.SetDustFiltering(false);
        } else {
            x_TokenizeFilteringArgs(dust_opts, tokens);
            opt.SetDustFilteringLevel(NStr::StringToInt(tokens[0]));
            opt.SetDustFilteringWindow(NStr::StringToInt(tokens[1]));
            opt.SetDustFilteringLinker(NStr::StringToInt(tokens[2]));
        }
    }

    if (args[kArgFilteringDb]) {
        opt.SetRepeatFilteringDB(args[kArgFilteringDb].AsString().c_str());
    }
}

void
CWindowSizeArg::SetArgumentDescriptions(CArgDescriptions& arg_desc)
{
    // 2-hit wordfinder window size
    arg_desc.AddOptionalKey(kArgWindowSize, "int_value", 
                            "Multiple hits window size, use 0 to specify "
                            "1-hit algorithm",
                            CArgDescriptions::eInteger);
    arg_desc.SetConstraint(kArgWindowSize, 
                           new CArgAllowValuesGreaterThanOrEqual(0));
}

void
CWindowSizeArg::ExtractAlgorithmOptions(const CArgs& args, CBlastOptions& opt)
{
    if (args[kArgWindowSize]) {
        opt.SetWindowSize(args[kArgWindowSize].AsInteger());
    } else {
        int window = -1;
        BLAST_GetSuggestedWindowSize(opt.GetProgramType(), 
                                     opt.GetMatrixName(), 
                                     &window);
        if (window != -1) {
            opt.SetWindowSize(window);
        }
    }
}

void
CWordThresholdArg::SetArgumentDescriptions(CArgDescriptions& arg_desc)
{
    // lookup table word score threshold
    arg_desc.AddOptionalKey(kArgWordScoreThreshold, "float_value", 
                 "Minimum word score such that the word is added to the "
                 "BLAST lookup table",
                 CArgDescriptions::eDouble);
    arg_desc.SetConstraint(kArgWordScoreThreshold, 
                           new CArgAllowValuesGreaterThanOrEqual(0));
}

void
CWordThresholdArg::ExtractAlgorithmOptions(const CArgs& args, 
                                           CBlastOptions& opt)
{
    if (args[kArgWordScoreThreshold]) {
        opt.SetWordThreshold(args[kArgWordScoreThreshold].AsDouble());
    } else {
        double threshold = -1;
        BLAST_GetSuggestedThreshold(opt.GetProgramType(),
                                    opt.GetMatrixName(),
                                    &threshold);
        if (threshold != -1) {
            opt.SetWordThreshold(threshold);
        }
    }
}

void
CMatrixNameArg::SetArgumentDescriptions(CArgDescriptions& arg_desc)
{
    arg_desc.AddDefaultKey(kArgMatrixName, "matrix_name", 
                           "Scoring matrix name",
                           CArgDescriptions::eString, 
                           string(BLAST_DEFAULT_MATRIX));
}

void
CMatrixNameArg::ExtractAlgorithmOptions(const CArgs& args, CBlastOptions& opt)
{
    if (args[kArgMatrixName]) {
        opt.SetMatrixName(args[kArgMatrixName].AsString().c_str());
    }
}

void
CNuclArgs::SetArgumentDescriptions(CArgDescriptions& arg_desc)
{
    arg_desc.SetCurrentGroup("Nucleotide scoring options");

    // blastn mismatch penalty
    arg_desc.AddDefaultKey(kArgMismatch, "penalty", 
                           "Penalty for a nucleotide mismatch", 
                           CArgDescriptions::eInteger,
                           NStr::IntToString(BLAST_PENALTY));
    arg_desc.SetConstraint(kArgMismatch, 
                           new CArgAllowValuesLessThanOrEqual(0));

    // blastn match reward
    arg_desc.AddDefaultKey(kArgMatch, "reward", 
                           "Reward for a nucleotide match", 
                           CArgDescriptions::eInteger, 
                           NStr::IntToString(BLAST_REWARD));
    arg_desc.SetConstraint(kArgMatch, 
                           new CArgAllowValuesGreaterThanOrEqual(0));

    arg_desc.AddFlag(kArgNoGreedyExtension,
                     "Use non-greedy dynamic programming extension",
                     true);

    arg_desc.SetCurrentGroup("");
}

void
CNuclArgs::ExtractAlgorithmOptions(const CArgs& cmd_line_args,
                                   CBlastOptions& options)
{
    if (cmd_line_args[kArgMismatch])
        options.SetMismatchPenalty(cmd_line_args[kArgMismatch].AsInteger());
    if (cmd_line_args[kArgMatch])
        options.SetMatchReward(cmd_line_args[kArgMatch].AsInteger());

    if (cmd_line_args[kArgNoGreedyExtension]) {
        options.SetGapExtnAlgorithm(eDynProgScoreOnly);
        options.SetGapTracebackAlgorithm(eDynProgTbck);
    }
}

const string CDiscontinuousMegablastArgs::kTemplType_Coding("coding");
const string CDiscontinuousMegablastArgs::kTemplType_Optimal("optimal");
const string 
CDiscontinuousMegablastArgs::kTemplType_CodingAndOptimal("coding_and_optimal");

void
CDiscontinuousMegablastArgs::SetArgumentDescriptions(CArgDescriptions& arg_desc)
{
    // FIXME: this can be applied to any program, but since it was only offered
    // in megablast, we're putting it here 
    arg_desc.AddOptionalKey(kArgMinRawGappedScore, "int_value",
                            "Minimum raw gapped score to keep an alignment "
                            "in the preliminary gapped and traceback stages",
                            CArgDescriptions::eInteger);

    arg_desc.SetCurrentGroup("Discontinuous Megablast options");

    arg_desc.AddOptionalKey(kArgDMBTemplateType, "type", 
                 "Discontinuous megablast template type",
                 CArgDescriptions::eString);
    arg_desc.SetConstraint(kArgDMBTemplateType, &(*new CArgAllow_Strings, 
                                                  kTemplType_Coding,
                                                  kTemplType_Optimal,
                                                  kTemplType_CodingAndOptimal));
    arg_desc.SetDependency(kArgDMBTemplateType,
                           CArgDescriptions::eRequires,
                           kArgDMBTemplateLength);

    arg_desc.AddOptionalKey(kArgDMBTemplateLength, "int_value", 
                 "Discontinuous megablast template length",
                 CArgDescriptions::eInteger);
    set<int> allowed_values;
    allowed_values.insert(16);
    allowed_values.insert(18);
    allowed_values.insert(21);
    arg_desc.SetConstraint(kArgDMBTemplateLength, 
                           new CArgAllowIntegerSet(allowed_values));
    arg_desc.SetDependency(kArgDMBTemplateLength,
                           CArgDescriptions::eRequires,
                           kArgDMBTemplateType);

    arg_desc.SetCurrentGroup("");
}

void
CDiscontinuousMegablastArgs::ExtractAlgorithmOptions(const CArgs& args,
                                                     CBlastOptions& options)
{
    if (args[kArgMinRawGappedScore]) {
        options.SetCutoffScore(args[kArgMinRawGappedScore].AsInteger());
    }

    if (args[kArgDMBTemplateType]) {
        const string& type = args[kArgDMBTemplateType].AsString();
        EDiscWordType temp_type = eMBWordCoding;

        if (type == kTemplType_Coding) {
            temp_type = eMBWordCoding;
        } else if (type == kTemplType_Optimal) {
            temp_type = eMBWordOptimal;
        } else if (type == kTemplType_CodingAndOptimal) {
            temp_type = eMBWordTwoTemplates;
        } else {
            abort();
        }
        options.SetMBTemplateType(static_cast<unsigned char>(temp_type));
    }

    if (args[kArgDMBTemplateLength]) {
        unsigned char tlen = 
            static_cast<unsigned char>(args[kArgDMBTemplateLength].AsInteger());
        options.SetMBTemplateLength(tlen);
    }

    // FIXME: should the window size be adjusted if this is set?
}

void
CCompositionBasedStatsArgs::SetArgumentDescriptions(CArgDescriptions& arg_desc)
{
    // composition based statistics
    arg_desc.AddOptionalKey(kArgCompBasedStats, "compo", 
                      "Use composition-based statistics for blastp / tblastn:\n"
                      "    D or d: default (equivalent to F)\n"
                      "    0 or F or f: no composition-based statistics\n"
                      "    1 or T or t: Composition-based statistics "
                                      "as in NAR 29:2994-3005, 2001\n"
                      "    2: Composition-based score adjustment as in "
                                      "Bioinformatics 21:902-911,\n"
                      "    2005, conditioned on sequence properties\n"
                      "    3: Composition-based score adjustment as in "
                                      "Bioinformatics 21:902-911,\n"
                      "    2005, unconditionally\n"
                      "For programs other than tblastn, must either be "
                      "absent or be D, F or 0",
                      CArgDescriptions::eString);

    // Use Smith-Waterman algorithm in traceback stage
    // FIXME: available only for gapped blastp/tblastn, and with
    // composition-based statistics
    arg_desc.AddFlag(kArgUseSWTraceback, 
                     "Compute locally optimal Smith-Waterman alignments?",
                     true);
}

/** 
 * @brief Auxiliary function to set the composition based statistics and smith
 * waterman options
 * 
 * @param opt BLAST options object [in|out]
 * @param comp_stat_string command line value for composition based statistics
 * [in]
 * @param smith_waterman_value command line value for determining the use of
 * the smith-waterman algorithm [in]
 */
static void
s_SetCompositionBasedStats(CBlastOptions& opt,
                           const string& comp_stat_string,
                           bool smith_waterman_value)
{
    const EProgram program = opt.GetProgram();
    if (program == eBlastp || program == eTblastn || program == ePSIBlast) {

        ECompoAdjustModes compo_mode = eNoCompositionBasedStats;;
    
        switch (comp_stat_string[0]) {
            case 'D': case 'd':
            case '0': case 'F': case 'f':
                compo_mode = eNoCompositionBasedStats;
                break;
            case '1': case 'T': case 't':
                compo_mode = eCompositionBasedStats;
                break;
            case '2':
                compo_mode = eCompositionMatrixAdjust;
                break;
            case '3':
                compo_mode = eCompoForceFullMatrixAdjust;
                break;
        }
        opt.SetCompositionBasedStats(compo_mode);
        if (program == eBlastp &&
            compo_mode != eNoCompositionBasedStats &&
            tolower(comp_stat_string[1]) == 'u') {
            opt.SetUnifiedP(1);
        }
        opt.SetSmithWatermanMode(smith_waterman_value);
    }
}

void
CCompositionBasedStatsArgs::ExtractAlgorithmOptions(const CArgs& args,
                                                    CBlastOptions& opt)
{
    if (args[kArgCompBasedStats]) {
        s_SetCompositionBasedStats(opt, 
                                   args[kArgCompBasedStats].AsString(),
                                   args[kArgUseSWTraceback]);
    }

}

void
CGappedArgs::SetArgumentDescriptions(CArgDescriptions& arg_desc)
{
    // perform gapped search
#if 0
    arg_desc.AddOptionalKey(ARG_GAPPED, "gapped", 
                 "Perform gapped alignment (default T, but "
                 "not available for tblastx)",
                 CArgDescriptions::eBoolean,
                 CArgDescriptions::fOptionalSeparator);
    arg_desc.AddAlias("-gapped", ARG_GAPPED);
#endif
    arg_desc.AddFlag(kArgUngapped, "Perform ungapped alignment only?", true);
}

void
CGappedArgs::ExtractAlgorithmOptions(const CArgs& args, CBlastOptions& options)
{
#if 0
    if (args[ARG_GAPPED] && options.GetProgram() != eTblastx) {
        options.SetGappedMode(args[ARG_GAPPED].AsBoolean());
    }
#endif
    options.SetGappedMode( !args[kArgUngapped] );
}

void
CLargestIntronSizeArgs::SetArgumentDescriptions(CArgDescriptions& arg_desc)
{
    // largest intron length
    arg_desc.AddDefaultKey(kArgMaxIntronLength, "length", 
                    "Length of the largest intron allowed in a translated "
                    "nucleotide sequence when linking multiple distinct "
                    "alignments (a negative value disables linking)",
                    CArgDescriptions::eInteger,
                    NStr::IntToString(kDfltArgMaxIntronLength));
}

void
CLargestIntronSizeArgs::ExtractAlgorithmOptions(const CArgs& args,
                                                CBlastOptions& opt)
{
    if ( !args[kArgMaxIntronLength] ) {
        return;
    }

    if (args[kArgMaxIntronLength].AsInteger() < 0) {
        opt.SetSumStatisticsMode(false);
    } else {
        opt.SetSumStatisticsMode();
        opt.SetLongestIntronLength(args[kArgMaxIntronLength].AsInteger());
    }
}

void
CFrameShiftArgs::SetArgumentDescriptions(CArgDescriptions& arg_desc)
{
    // applicable in blastx/tblastn, off by default
    arg_desc.AddOptionalKey(kArgFrameShiftPenalty, "frameshift",
                            "Frame shift penalty (for use with out-of-frame "
                            "gapped alignment in blastx or tblastn, default "
                            "ignored)",
                            CArgDescriptions::eInteger);
    arg_desc.SetConstraint(kArgFrameShiftPenalty, 
                           new CArgAllowValuesGreaterThanOrEqual(1));
}

void
CFrameShiftArgs::ExtractAlgorithmOptions(const CArgs& args,
                                         CBlastOptions& opt)
{
    if (args[kArgFrameShiftPenalty]) {
        opt.SetOutOfFrameMode();
        opt.SetFrameShiftPenalty(args[kArgFrameShiftPenalty].AsInteger());
    }
}

void
CGeneticCodeArgs::SetArgumentDescriptions(CArgDescriptions& arg_desc)
{
    if (m_Target == eQuery) {
        // query genetic code
        arg_desc.AddDefaultKey(kArgQueryGeneticCode, "int_value", 
                               "Genetic code to use to translate query",
                               CArgDescriptions::eInteger,
                               NStr::IntToString(BLAST_GENETIC_CODE));
    } else {
        // DB genetic code
        arg_desc.AddDefaultKey(kArgDbGeneticCode, "int_value", 
                               "Genetic code to use to translate database",
                               CArgDescriptions::eInteger,
                               NStr::IntToString(BLAST_GENETIC_CODE));
    }
}

void
CGeneticCodeArgs::ExtractAlgorithmOptions(const CArgs& args,
                                          CBlastOptions& opt)
{
    const EProgram program = opt.GetProgram();

    if (m_Target == eQuery && args[kArgQueryGeneticCode]) {
        opt.SetQueryGeneticCode(args[kArgQueryGeneticCode].AsInteger());
    }
  
    if (m_Target == eDatabase && args[kArgDbGeneticCode] &&
        (program == eTblastn || program == eTblastx) ) {
        opt.SetDbGeneticCode(args[kArgDbGeneticCode].AsInteger());
    }
}

void
CDecline2AlignArgs::SetArgumentDescriptions(CArgDescriptions& arg_desc)
{
    arg_desc.AddDefaultKey(kArgDecline2Align, "int_value", 
                           "Cost to decline alignment",
                           CArgDescriptions::eInteger,
                           NStr::IntToString(m_DefaultValue));
}

void
CDecline2AlignArgs::ExtractAlgorithmOptions(const CArgs& args,
                                            CBlastOptions& opt)
{
    if (args[kArgDecline2Align]) {
        opt.SetDecline2AlignPenalty(args[kArgDecline2Align].AsInteger());
    }
}


void
CGapTriggerArgs::SetArgumentDescriptions(CArgDescriptions& arg_desc)
{
    const double default_value = m_QueryIsProtein
        ? BLAST_GAP_TRIGGER_PROT : BLAST_GAP_TRIGGER_NUCL;
    arg_desc.AddDefaultKey(kArgGapTrigger, "float_value", 
                           "Number of bits to trigger gapping",
                           CArgDescriptions::eDouble,
                           NStr::DoubleToString(default_value));
}

void
CGapTriggerArgs::ExtractAlgorithmOptions(const CArgs& args,
                                         CBlastOptions& opt)
{
    if (args[kArgGapTrigger]) {
        opt.SetGapTrigger(args[kArgGapTrigger].AsDouble());
    }
}

void
CPssmEngineArgs::SetArgumentDescriptions(CArgDescriptions& arg_desc)
{
    arg_desc.SetCurrentGroup("PSSM engine options");

    // Pseudo count
    arg_desc.AddDefaultKey(kArgPSIPseudocount, "pseudocount",
                           "Pseudo-count value used when constructing PSSM",
                           CArgDescriptions::eInteger,
                           NStr::IntToString(PSI_PSEUDO_COUNT_CONST));

    // Evalue inclusion threshold
    arg_desc.AddDefaultKey(kArgPSIInclusionEThreshold, "ethresh", 
                   "E-value inclusion threshold for pairwise alignments", 
                   CArgDescriptions::eDouble,
                   NStr::DoubleToString(PSI_INCLUSION_ETHRESH));

    arg_desc.SetCurrentGroup("");
}

void
CPssmEngineArgs::ExtractAlgorithmOptions(const CArgs& args,
                                         CBlastOptions& opt)
{
    if (args[kArgPSIPseudocount]) {
        opt.SetPseudoCount(args[kArgPSIPseudocount].AsInteger());
    }

    if (args[kArgPSIInclusionEThreshold]) {
        opt.SetInclusionThreshold(args[kArgPSIInclusionEThreshold].AsDouble());
    }
}

void
CPsiBlastArgs::SetArgumentDescriptions(CArgDescriptions& arg_desc)
{

    if (m_DbTarget == eNucleotideDb) {
        arg_desc.SetCurrentGroup("PSI-TBLASTN options");

        // PSI-tblastn checkpoint
        arg_desc.AddOptionalKey(kArgPSIInputChkPntFile, "psi_chkpt_file", 
                                "PSI-TBLASTN checkpoint file",
                                CArgDescriptions::eInputFile);
    } else {
        arg_desc.SetCurrentGroup("PSI-BLAST options");

        // Number of iterations
        arg_desc.AddDefaultKey(kArgPSINumIterations, "int_value",
                               "Number of iterations to perform",
                               CArgDescriptions::eInteger,
                               NStr::IntToString(1));
        arg_desc.SetConstraint(kArgPSINumIterations, 
                               new CArgAllowValuesGreaterThanOrEqual(1));
        // checkpoint file
        arg_desc.AddOptionalKey(kArgPSIOutputChkPntFile, "checkpoint_file",
                                "File name to store checkpoint file",
                                CArgDescriptions::eOutputFile);
        // ASCII matrix file
        arg_desc.AddOptionalKey(ARG_ASCII_MATRIX, "ascii_mtx_file",
                                "File name to store ASCII version of PSSM",
                                CArgDescriptions::eOutputFile,
                                CArgDescriptions::fOptionalSeparator);
        // MSA restart file
        arg_desc.AddOptionalKey(ARG_MSA_RESTART, "align_restart",
                                "File name of multiple sequence alignment to "
                                "restart PSI-BLAST",
                                CArgDescriptions::eInputFile,
                                CArgDescriptions::fOptionalSeparator);
        // PSI-BLAST checkpoint
        arg_desc.AddOptionalKey(kArgPSIInputChkPntFile, "psi_chkpt_file", 
                                "PSI-BLAST checkpoint file",
                                CArgDescriptions::eInputFile);
    }

    arg_desc.SetCurrentGroup("");
}

void
CPsiBlastArgs::ExtractAlgorithmOptions(const CArgs& args,
                                       CBlastOptions& /* opt */)
{
    if (m_DbTarget == eProteinDb) {
        if (args[kArgPSINumIterations]) {
            m_NumIterations = args[kArgPSINumIterations].AsInteger();
        }
        if (args[kArgPSIOutputChkPntFile]) {
            m_CheckPointOutputStream = NULL;    /* FIXME */
        }
        if (args[ARG_ASCII_MATRIX]) {
            cerr << "Warning: ASCII MATRIX NOT HANDLED\n";
        }
        if (args[ARG_MSA_RESTART]) {
            cerr << "Warning: INPUT ALIGNMENT FILE NOT HANDLED\n";
        }
    }

    if (args.Exist(kArgPSIInputChkPntFile) && args[kArgPSIInputChkPntFile]) {
        CNcbiIstream& in = args[kArgPSIInputChkPntFile].AsInputFile();
        m_Pssm.Reset(new CPssmWithParameters);
        switch (CFormatGuess().Format(in)) {
        case CFormatGuess::eBinaryASN:
            in >> MSerial_AsnBinary >> *m_Pssm;
            break;
        case CFormatGuess::eTextASN:
            in >> MSerial_AsnText >> *m_Pssm;
            break;
        case CFormatGuess::eXml:
            in >> MSerial_Xml >> *m_Pssm;
            break;
        default:
            NCBI_THROW(CBlastException, eInvalidArgument, 
                       "Unsupported format for PSSM");
            break;
        }
        _ASSERT(m_Pssm.NotEmpty());
    }
}

void
CPhiBlastArgs::SetArgumentDescriptions(CArgDescriptions& arg_desc)
{
    arg_desc.SetCurrentGroup("PHI-BLAST options");

    arg_desc.AddOptionalKey(kArgPHIPatternFile, "file",
                            "File name containing pattern to search",
                            CArgDescriptions::eInputFile);
    arg_desc.SetDependency(kArgPHIPatternFile,
                           CArgDescriptions::eExcludes,
                           kArgPSIInputChkPntFile);

    arg_desc.SetCurrentGroup("");
}

void
CPhiBlastArgs::ExtractAlgorithmOptions(const CArgs& args,
                                       CBlastOptions& opt)
{
    if (args.Exist(kArgPHIPatternFile) && args[kArgPHIPatternFile]) {
        //CNcbiIstream& in = args[kArgPHIPatternFile].AsInputFile();
        string pattern("FIXME - NOT VALID");
        // FIXME: need to port code from pssed3.c get_pat function
        opt.SetPHIPattern(pattern.c_str(), 
                          Blast_QueryIsNucleotide(opt.GetProgramType()));
        throw runtime_error("Reading of pattern file not implemented");
    }
}

void
CQueryOptionsArgs::SetArgumentDescriptions(CArgDescriptions& arg_desc)
{
    arg_desc.SetCurrentGroup("Input query options");

    // lowercase masking
    arg_desc.AddFlag(kArgUseLCaseMasking, 
                     "Use lower case filtering in query sequence(s)?", true);

    // query location
    arg_desc.AddOptionalKey(kArgQueryLocation, "range", 
                            "Location on the query sequence "
                            "(Format: start-stop)",
                            CArgDescriptions::eString);

    // believe query ID
    arg_desc.AddFlag(kArgParseQueryDefline,
                     "Should the query defline(s) be parsed?", true);

    if ( !m_QueryCannotBeNucl ) {
        // search strands
        arg_desc.AddDefaultKey(kArgStrand, "strand", 
                         "Query strand(s) to search against database",
                         CArgDescriptions::eString, kDfltArgStrand);
        arg_desc.SetConstraint(kArgStrand, &(*new CArgAllow_Strings, 
                                             kDfltArgStrand, "plus", "minus"));
    }

    arg_desc.SetCurrentGroup("");
}

void
CQueryOptionsArgs::ExtractAlgorithmOptions(const CArgs& args, 
                                           CBlastOptions& opt)
{
    // Get the strand
    {
        m_Strand = eNa_strand_unknown;

        if (!Blast_QueryIsProtein(opt.GetProgramType()) && args[kArgStrand]) {
            const string& kStrand = args[kArgStrand].AsString();
            if (kStrand == "both") {
                m_Strand = eNa_strand_both;
            } else if (kStrand == "plus") {
                m_Strand = eNa_strand_plus;
            } else if (kStrand == "minus") {
                m_Strand = eNa_strand_minus;
            } else {
                abort();
            }
        }
    }

    // set the sequence range
    if (args[kArgQueryLocation]) {
        const string delimiters("-");
        vector<string> tokens;
        NStr::Tokenize(args[kArgQueryLocation].AsString(), delimiters, tokens);
        if (tokens.size() != 2) {
            NCBI_THROW(CBlastException, eInvalidArgument, 
                       "Invalid specification of query location");
        }
        m_Range.SetFrom(NStr::StringToInt(tokens.front()));
        m_Range.SetToOpen(NStr::StringToInt(tokens.back()));
    }

    m_UseLCaseMask = static_cast<bool>(args[kArgUseLCaseMasking]);
    m_BelieveQueryDefline = static_cast<bool>(args[kArgParseQueryDefline]);
}

CBlastDatabaseArgs::CBlastDatabaseArgs(bool request_mol_type /* = false */)
    : m_RequestMoleculeType(request_mol_type)
{}

void
CBlastDatabaseArgs::SetArgumentDescriptions(CArgDescriptions& arg_desc)
{
    arg_desc.SetCurrentGroup("BLAST database options");

    // database filename
    arg_desc.AddOptionalKey(kArgDb, "database_name", "BLAST database name", 
                            CArgDescriptions::eString);

    if (m_RequestMoleculeType) {
        arg_desc.AddKey(kArgDbType, "database_type", 
                        "BLAST database molecule type",
                        CArgDescriptions::eString);
        arg_desc.SetConstraint(kArgDbType, 
                               &(*new CArgAllow_Strings, "prot", "nucl"));
    }

    // DB size
    arg_desc.AddOptionalKey(kArgDbSize, "num_letters", 
                            "Effective length of the database ",
                            CArgDescriptions::eInt8);

    // GI list
    arg_desc.AddOptionalKey(kArgGiList, "filename", 
                            "Restrict search of database to list of GI's",
                            CArgDescriptions::eString);

    arg_desc.SetCurrentGroup("BLAST-2-Sequences options");
    // subject sequence input (for bl2seq)
    arg_desc.AddOptionalKey(kArgSubject, "subject_input_file",
                            "Subject sequence(s) to search",
                            CArgDescriptions::eInputFile);
    arg_desc.SetDependency(kArgSubject, CArgDescriptions::eExcludes, kArgDb);
    arg_desc.SetDependency(kArgSubject, CArgDescriptions::eExcludes, 
                           kArgGiList);
    // subject location
    arg_desc.AddOptionalKey(kArgSubjectLocation, "range", 
                            "Location on the subject sequence "
                            "(Format: start-stop)",
                            CArgDescriptions::eString);
    arg_desc.SetDependency(kArgSubjectLocation, 
                           CArgDescriptions::eExcludes, kArgDb);
    arg_desc.SetDependency(kArgSubjectLocation, CArgDescriptions::eExcludes, 
                           kArgGiList);

    arg_desc.SetCurrentGroup("");
}

void
CBlastDatabaseArgs::ExtractAlgorithmOptions(const CArgs& args,
                                            CBlastOptions& opts)
{
    EMoleculeType mol_type = Blast_SubjectIsNucleotide(opts.GetProgramType())
        ? CSearchDatabase::eBlastDbIsNucleotide
        : CSearchDatabase::eBlastDbIsProtein;
    
    if (args.Exist(kArgDb) && args[kArgDb]) {

        m_SearchDb.Reset(new CSearchDatabase(args[kArgDb].AsString(), 
                                             mol_type));
        if (args.Exist(kArgGiList) && args[kArgGiList]) {
            m_GiListFileName.assign(args[kArgGiList].AsString());
            /// This is only needed if the gi list is to be submitted remotely
            if (args.Exist(kArgRemote) && args[kArgRemote] && 
                CFile(m_GiListFileName).Exists()) {
                vector<int> gis;
                SeqDB_ReadGiList(m_GiListFileName, gis);
                m_SearchDb->SetGiListLimitation(gis);
            }
        }
    } else if (args.Exist(kArgSubject) && args[kArgSubject]) {
        if (args.Exist(kArgRemote) && args[kArgRemote]) {
            NCBI_THROW(CBlastException, eInvalidArgument,
               "Submission of remote BLAST-2-Sequences is not supported\n"
               "Please visit the NCBI web site to submit your search");
        }
        throw runtime_error("Setting of subject sequences unimplemented");
    } else {
        NCBI_THROW(CBlastException, eInvalidArgument,
           "Either a BLAST database or subject sequence(s) must be specified");
    }

    if (opts.GetEffectiveSearchSpace() != 0) {
        // no need to set any other options, as this trumps them
        return;
    }

    if (args.Exist(kArgSubjectLocation) && args[kArgSubjectLocation]) {
        throw runtime_error("Setting of subject sequence location unimplemented");
    }

    if (args[kArgDbSize]) {
        opts.SetDbLength(args[kArgDbSize].AsInt8());
    }

}

bool
CBlastDatabaseArgs::IsProtein() const
{
    return m_SearchDb->GetMoleculeType() == CSearchDatabase::eBlastDbIsProtein;
}

void
CFormattingArgs::SetArgumentDescriptions(CArgDescriptions& arg_desc)
{
    arg_desc.SetCurrentGroup("Formatting options");

    // alignment view
    arg_desc.AddDefaultKey(kArgOutputFormat, "format", 
                    "alignment view options:\n"
                    "  0 = pairwise,\n"
                    "  1 = query-anchored showing identities,\n"
                    "  2 = query-anchored no identities,\n"
                    "  3 = flat query-anchored, show identities,\n"
                    "  4 = flat query-anchored, no identities,\n"
                    "  5 = query-anchored no identities and blunt ends,\n"
                    "  6 = flat query-anchored, no identities and blunt ends,\n"
                    "  7 = XML Blast output,\n"
                    "  8 = tabular, \n"
                    "  9 tabular with comment lines,\n"
                    "  10 ASN, text,\n"
                    "  11 ASN, binary\n",
                   CArgDescriptions::eInteger,
                   NStr::IntToString(kDfltArgOutputFormat));
    arg_desc.SetConstraint(kArgOutputFormat, new CArgAllow_Integers(0,11));

    // show GIs in deflines
    arg_desc.AddFlag(kArgShowGIs, "Show NCBI GIs in deflines?", true);

    // number of one-line descriptions to display
    arg_desc.AddDefaultKey(kArgNumDescriptions, "int_value",
                 "Number of database sequences to show one-line "
                 "descriptions for",
                 CArgDescriptions::eInteger,
                 NStr::IntToString(kDfltArgNumDescriptions));
    arg_desc.SetConstraint(kArgNumDescriptions, 
                           new CArgAllowValuesGreaterThanOrEqual(1));

    // number of alignments per DB sequence
    arg_desc.AddDefaultKey(kArgNumAlignments, "int_value",
                 "Number of database sequences to show alignments for",
                 CArgDescriptions::eInteger, 
                 NStr::IntToString(kDfltArgNumAlignments));
    arg_desc.SetConstraint(kArgNumAlignments, 
                           new CArgAllowValuesGreaterThanOrEqual(1));

# if 0
    // Deprecate this?
    // HTML output
    arg_desc.AddOptionalKey(ARG_HTML, "html", 
                            "Produce HTML output (default F)",
                            CArgDescriptions::eBoolean,
                            CArgDescriptions::fOptionalSeparator);
    arg_desc.AddAlias("-html", ARG_HTML);
#endif

    arg_desc.SetCurrentGroup("");
}

void
CFormattingArgs::ExtractAlgorithmOptions(const CArgs& args,
                                         CBlastOptions& /* opt */)
{
    if (args[kArgOutputFormat]) {
        m_FormattedOutputChoice = args[kArgOutputFormat].AsInteger();
    }

    m_ShowGis = static_cast<bool>(args[kArgShowGIs]);

    if (args[kArgNumDescriptions]) {
        m_NumDescriptions = args[kArgNumDescriptions].AsInteger();
    } 

    if (args[kArgNumAlignments]) {
        m_NumAlignments = args[kArgNumAlignments].AsInteger();
    }

#if 0
    if (args[ARG_HTML]) {
        m_Html = args[ARG_HTML].AsBoolean();
    }
#endif
}

void
CMTArgs::SetArgumentDescriptions(CArgDescriptions& arg_desc)
{
    const int kMinValue = static_cast<int>(CThreadable::kMinNumThreads);

    // number of threads
    arg_desc.AddDefaultKey(kArgNumThreads, "int_value",
                           "Number of threads to use in preliminary stage "
                           "of the search", CArgDescriptions::eInteger, 
                           NStr::IntToString(kMinValue));
    arg_desc.SetConstraint(kArgNumThreads, 
                           new CArgAllowValuesGreaterThanOrEqual(kMinValue));
}

void
CMTArgs::ExtractAlgorithmOptions(const CArgs& args, CBlastOptions& /* opts */)
{
    if (args.Exist(kArgNumThreads)) {
        m_NumThreads = args[kArgNumThreads].AsInteger();
    }
}

void
CRemoteArgs::SetArgumentDescriptions(CArgDescriptions& arg_desc)
{
    arg_desc.AddFlag(kArgRemote, "Execute search remotely?", true);
    arg_desc.SetDependency(kArgRemote,
                           CArgDescriptions::eExcludes,
                           kArgNumThreads);

#if _DEBUG
    arg_desc.AddFlag("remote_verbose", 
                     "Produce verbose output for remote searches?", true);
#endif /* DEBUG */
}

void
CRemoteArgs::ExtractAlgorithmOptions(const CArgs& args, CBlastOptions& /* opts */)
{
    if (args.Exist(kArgRemote)) {
        m_IsRemote = static_cast<bool>(args[kArgRemote]);
    }

#if _DEBUG
    m_DebugOutput = static_cast<bool>(args["remote_verbose"]);
#endif /* DEBUG */
}

void
CCullingArgs::SetArgumentDescriptions(CArgDescriptions& arg_desc)
{
    // culling limit
    arg_desc.AddDefaultKey(kArgCullingLimit, "int_value",
                     "If the query range of a hit is enveloped by that of at "
                     "least this many higher-scoring hits, delete the hit",
                     CArgDescriptions::eInteger,
                     NStr::IntToString(kDfltArgCullingLimit));
    arg_desc.SetConstraint(kArgCullingLimit, 
                           new CArgAllowValuesGreaterThanOrEqual(0));
}

void
CCullingArgs::ExtractAlgorithmOptions(const CArgs& args,
                                      CBlastOptions& opts)
{
    if (args[kArgCullingLimit]) {
        opts.SetCullingLimit(args[kArgCullingLimit].AsInteger());
    }
}

#if 0
CBlastArgs::CBlastArgs(string prog_name,
                       string prog_desc)
    : m_ProgName(prog_name),
      m_ProgDesc(prog_desc)
{
}

void
CBlastArgs::x_SetArgDescriptions(CArgDescriptions * arg_desc)
{
    if ( !arg_desc ) {
        NCBI_THROW(CBlastException, eInvalidArgument, "NULL description");
    }
}
#endif

void
CStdCmdLineArgs::SetArgumentDescriptions(CArgDescriptions& arg_desc)
{
    // query filename
    arg_desc.AddDefaultKey(kArgQuery, "input_file", 
                     "Input file name",
                     CArgDescriptions::eInputFile, "-");

    // report output file
    arg_desc.AddDefaultKey(kArgOutput, "output_file", 
                   "Output file name",
                   CArgDescriptions::eOutputFile, "-");
}

void
CStdCmdLineArgs::ExtractAlgorithmOptions(const CArgs& args,
                                         CBlastOptions& /* opt */)
{
    m_InputStream = &args[kArgQuery].AsInputFile();
    m_OutputStream = &args[kArgOutput].AsOutputFile();
}

CNcbiIstream&
CStdCmdLineArgs::GetInputStream() const
{
    // programmer must ensure the ExtractAlgorithmOptions method is called
    // before this method is invoked
    _ASSERT(m_InputStream); 
    return *m_InputStream;
}

CNcbiOstream&
CStdCmdLineArgs::GetOutputStream() const
{
    // programmer must ensure the ExtractAlgorithmOptions method is called
    // before this method is invoked
    _ASSERT(m_OutputStream);
    return *m_OutputStream;
}

void
CSearchStrategyArgs::SetArgumentDescriptions(CArgDescriptions& arg_desc)
{
    arg_desc.SetCurrentGroup("Search strategy options");

    arg_desc.AddOptionalKey(kArgInputSearchStrategy,
                            "filename",
                            "Search strategy to use", 
                            CArgDescriptions::eInputFile);
    arg_desc.AddOptionalKey(kArgOutputSearchStrategy,
                            "filename",
                            "File name to record the search strategy used", 
                            CArgDescriptions::eOutputFile);
    arg_desc.SetDependency(kArgInputSearchStrategy,
                           CArgDescriptions::eExcludes,
                           kArgOutputSearchStrategy);

    arg_desc.SetCurrentGroup("");
}

void
CSearchStrategyArgs::ExtractAlgorithmOptions(const CArgs& cmd_line_args,
                                             CBlastOptions& /* options */)
{
}

CBlastAppArgs::CBlastAppArgs()
{
    m_Args.push_back(CRef<IBlastCmdLineArgs>(new CSearchStrategyArgs));
}

CArgDescriptions*
CBlastAppArgs::SetCommandLine()
{
    return SetUpCommandLineArguments(m_Args);
}

CRef<CBlastOptionsHandle>
CBlastAppArgs::SetOptions(const CArgs& args)
{
    const CBlastOptions::EAPILocality locality = 
        (args.Exist(kArgRemote) && args[kArgRemote]) 
        ? CBlastOptions::eRemote 
        : CBlastOptions::eLocal;
    CRef<CBlastOptionsHandle> handle(x_CreateOptionsHandle(locality, args));
    CBlastOptions& opts = handle->SetOptions();
    NON_CONST_ITERATE(TBlastCmdLineArgs, arg, m_Args) {
        (*arg)->ExtractAlgorithmOptions(args, opts);
    }
    return handle;
}

END_SCOPE(blast)
END_NCBI_SCOPE
