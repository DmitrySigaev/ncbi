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
 * Author: Christiam Camacho
 *
 */

/** @file blast_input_aux.hpp
 *  Auxiliary classes/functions for BLAST input library
 */

#ifndef ALGO_BLAST_BLASTINPUT__BLAST_INPUT_AUX__HPP
#define ALGO_BLAST_BLASTINPUT__BLAST_INPUT_AUX__HPP

#include <algo/blast/api/sseqloc.hpp>   /* for CBlastQueryVector */
#include <algo/blast/blastinput/blast_args.hpp>
#include <sstream>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(blast)

/// Class to constrain the values of an argument to those greater than or equal
/// to the value specified in the constructor
class NCBI_XBLAST_EXPORT CArgAllowValuesGreaterThanOrEqual : public CArgAllow
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
class NCBI_XBLAST_EXPORT CArgAllowValuesLessThanOrEqual : public CArgAllow
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

/** 
 * @brief Macro to create a subclass of CArgAllow that allows the specification
 * of sets of data
 * 
 * @param ClassName Name of the class to be created [in]
 * @param DataType data type of the allowed arguments [in]
 * @param String2DataTypeFn Conversion function from a string to DataType [in]
 */
#define DEFINE_CARGALLOW_SET_CLASS(ClassName, DataType, String2DataTypeFn)  \
class NCBI_XBLAST_EXPORT ClassName : public CArgAllow                       \
{                                                                           \
public:                                                                     \
    ClassName(const set<DataType>& values)                                  \
        : m_AllowedValues(values)                                           \
    {                                                                       \
        if (values.empty()) {                                               \
            throw runtime_error("Allowed values set must not be empty");    \
        }                                                                   \
    }                                                                       \
                                                                            \
protected:                                                                  \
    virtual bool Verify(const string& value) const {                        \
        DataType value2check = String2DataTypeFn(value);                    \
        ITERATE(set<DataType>, itr, m_AllowedValues) {                      \
            if (*itr == value2check) {                                      \
                return true;                                                \
            }                                                               \
        }                                                                   \
        return false;                                                       \
    }                                                                       \
                                                                            \
    virtual string GetUsage(void) const {                                   \
        ostringstream os;                                                   \
        os << "Permissible values: ";                                       \
        ITERATE(set<DataType>, itr, m_AllowedValues) {                      \
            os << "'" << *itr << "' ";                                      \
        }                                                                   \
        return os.str();                                                    \
    }                                                                       \
                                                                            \
private:                                                                    \
    /* Set containing the permissible values */                             \
    set<DataType> m_AllowedValues;                                          \
}

#ifndef SKIP_DOXYGEN_PROCESSING
DEFINE_CARGALLOW_SET_CLASS(CArgAllowIntegerSet, int, NStr::StringToInt);
DEFINE_CARGALLOW_SET_CLASS(CArgAllowStringSet, string, string);
#endif /* SKIP_DOXYGEN_PROCESSING */

/** Parse and extract a sequence range from argument provided to this function
 * @param range_str string to extract the range from [in]
 * @param error_msg error message to encode in the exception thrown in case of
 * error (if NULL a default message will be used) [in]
 * @return properly constructed range if parsing succeeded
 * @throw CStringException or CBlastException with error code eInvalidArgument
 * if parsing fails */
NCBI_XBLAST_EXPORT
TSeqRange
ParseSequenceRange(const string& range_str, const char* error_msg = NULL);

/** 
 * @brief Create a CArgDescriptions object and invoke SetArgumentDescriptions
 * for each of the TBlastCmdLineArgs in its argument list
 * 
 * @param args arguments to configure the return value [in]
 * 
 * @return a CArgDescriptions object with the command line options set
 */
NCBI_XBLAST_EXPORT
CArgDescriptions* 
SetUpCommandLineArguments(TBlastCmdLineArgs& args);

/** Retrieve the appropriate batch size for the specified task 
 * @program BLAST task [in]
 */
NCBI_XBLAST_EXPORT
int
GetQueryBatchSize(EProgram program);

/** Read sequence input for BLAST 
 * @param in input stream from which to read [in]
 * @param read_proteins expect proteins or nucleotides as input [in]
 * @param range range restriction to apply to sequences read [in]
 * @param parse_deflines true if the subject deflines should be parsed [in]
 * @param sequences output will be placed here [in|out]
 * @return CScope object which contains all the sequences read
 */
NCBI_XBLAST_EXPORT
CRef<objects::CScope>
ReadSequencesToBlast(CNcbiIstream& in, 
                     bool read_proteins, 
                     const TSeqRange& range, 
                     bool parse_deflines,
                     CRef<CBlastQueryVector>& sequences);

/// Calculates the formatting parameters based on the maximum number of target
/// sequences selected (a.k.a.: hitlist size).
/// @param max_target_seqs the hitlist size [in]
/// @param num_descriptions the number of one-line descriptions to show [out]
/// @param num_alignments the number of alignments to show [out]
/// @param num_overview the number of sequences to show in the overview image
/// displayed in the BLAST report on the web [out]
NCBI_XBLAST_EXPORT
void
CalculateFormattingParams(TSeqPos max_target_seqs, 
                          TSeqPos* num_descriptions, 
                          TSeqPos* num_alignments, 
                          TSeqPos* num_overview = NULL);

/// Returns true if the Bioseq passed as argument has the full, raw sequence
/// data in its Seq-inst field
/// @param bioseq Bioseq to examine [in]
NCBI_XBLAST_EXPORT 
bool HasRawSequenceData(const objects::CBioseq& bioseq);

END_SCOPE(blast)
END_NCBI_SCOPE

#endif /* ALGO_BLAST_BLASTINPUT__BLAST_INPUT_AUX__HPP */

