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
 * Author:  Jason Papadopoulos
 *
 */

/** @file blast_args.hpp
 * Interface for converting blast-related command line
 * arguments into blast options
 */

#ifndef ALGO_BLAST_BLASTINPUT___BLAST_ARGS__HPP
#define ALGO_BLAST_BLASTINPUT___BLAST_ARGS__HPP

#include <corelib/ncbistd.hpp>
#include <corelib/ncbiargs.hpp>
#include <algo/blast/api/uniform_search.hpp>
#include <algo/blast/api/blast_options.hpp>
#include <algo/blast/api/blast_options_handle.hpp>
#include <algo/blast/api/setup_factory.hpp> // for CThreadable
#include <algo/blast/blastinput/cmdline_flags.hpp>

#include <objects/seqloc/Na_strand.hpp>
#include <objects/scoremat/PssmWithParameters.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(blast)

/**
 * BLAST Command line arguments design
 * The idea is to have several small objects (subclasses of IBlastCmdLineArgs) 
 * which can do two things:
 * 1) On creation, add flags/options/etc to a CArgs object
 * 2) When passed in a CBlastOptions object, call the appropriate methods based
 * on the CArgs options set when the NCBI application framework parsed the
 * command line. If data collected by the small object (from the command line)
 * cannot be applied to the CBlastOptions object, then it's provided to the
 * application via some other interface methods.
 *
 * Each command line application will have its own argument class (e.g.:
 * CPsiBlastAppArgs), which will contain several of the aformentioned small 
 * objects. It will create and hold a reference to a CArgs class as well as 
 * a CBlastOptionsHandle object, which will pass to each of its small objects 
 * aggregated as data members and then return it to the caller (application)
 *
 * Categories of data to extract from command line options
 * 1) BLAST algorithm options
 * 2) Input/Output files, and their modifiers (e.g.: believe query defline)
 * 3) BLAST database information (names, limitations, num db seqs)
 * 4) Formatting options (html, display formats, etc)
*/

/** Interface definition for a generic command line option for BLAST
 */
class NCBI_XBLAST_EXPORT IBlastCmdLineArgs : public CObject
{
public:
    /** Our virtual destructor */
    virtual ~IBlastCmdLineArgs() {}

    /** Sets the command line descriptions in the CArgDescriptions object
     * relevant to the subclass
     * @param arg_desc the argument descriptions object [in|out]
     */
    virtual void SetArgumentDescriptions(CArgDescriptions& arg_desc) = 0;

    /** Extracts BLAST algorithmic options from the command line arguments into
     * the CBlastOptions object
     * @param cmd_line_args Command line arguments parsed by the NCBI
     * application framework [in]
     * @param options object to which the appropriate options will be set
     * [in|out]
     */
    virtual void ExtractAlgorithmOptions(const CArgs& cmd_line_args, 
                                         CBlastOptions& options) {}
};

/** Argument class to retrieve input and output streams for a command line
 * program.
 */
class NCBI_XBLAST_EXPORT CStdCmdLineArgs : public IBlastCmdLineArgs
{
public:
    /** Default constructor */
    CStdCmdLineArgs() : m_InputStream(0), m_OutputStream(0) {};
    /** Interface method, \sa IBlastCmdLineArgs::SetArgumentDescriptions */
    virtual void SetArgumentDescriptions(CArgDescriptions& arg_desc);
    /** Interface method, \sa IBlastCmdLineArgs::SetArgumentDescriptions */
    virtual void ExtractAlgorithmOptions(const CArgs& cmd_line_args, 
                                         CBlastOptions& options);
    /** Get the input stream for a command line application */
    CNcbiIstream& GetInputStream() const;
    /** Get the output stream for a command line application */
    CNcbiOstream& GetOutputStream() const;

private:
    CNcbiIstream* m_InputStream;    ///< Application's input stream
    CNcbiOstream* m_OutputStream;   ///< Application's output stream
};

/** Argument class to populate an application's name and description */
class NCBI_XBLAST_EXPORT CProgramDescriptionArgs : public IBlastCmdLineArgs
{
public:
    /** 
     * @brief Constructor
     * 
     * @param program_name application's name [in]
     * @param program_description application's description [in]
     */
    CProgramDescriptionArgs(const string& program_name, 
                            const string& program_description);
    /** Interface method, \sa IBlastCmdLineArgs::SetArgumentDescriptions */
    virtual void SetArgumentDescriptions(CArgDescriptions& arg_desc);

private:
    string m_ProgName;  ///< Application's name
    string m_ProgDesc;  ///< Application's description
};

/// Argument class to specify the supported tasks a given program
class NCBI_XBLAST_EXPORT CTaskCmdLineArgs : public IBlastCmdLineArgs
{
public:
    /** Constructor 
     * @param supported_tasks list of supported tasks [in]
     */
    CTaskCmdLineArgs(const set<string>& supported_tasks)
        : m_SupportedTasks(supported_tasks) {}
    /** Interface method, \sa IBlastCmdLineArgs::SetArgumentDescriptions */
    virtual void SetArgumentDescriptions(CArgDescriptions& arg_desc);
    /** Interface method, \sa IBlastCmdLineArgs::SetArgumentDescriptions */
    virtual void ExtractAlgorithmOptions(const CArgs& cmd_line_args, 
                                         CBlastOptions& options);
private:
    const set<string> m_SupportedTasks;
};

/** Argument class to retrieve and set the window size BLAST algorithm 
 * option */
class NCBI_XBLAST_EXPORT CWindowSizeArg : public IBlastCmdLineArgs
{
public:
    /** Interface method, \sa IBlastCmdLineArgs::SetArgumentDescriptions */
    virtual void SetArgumentDescriptions(CArgDescriptions& arg_desc);
    /** Interface method, \sa IBlastCmdLineArgs::SetArgumentDescriptions 
     * @note this depends on the matrix already being set...
     */
    virtual void ExtractAlgorithmOptions(const CArgs& cmd_line_args, 
                                         CBlastOptions& options);
};

/** Argument class to retrieve and set the word threshold BLAST algorithm 
 * option */
class NCBI_XBLAST_EXPORT CWordThresholdArg : public IBlastCmdLineArgs
{
public:
    /** Interface method, \sa IBlastCmdLineArgs::SetArgumentDescriptions */
    virtual void SetArgumentDescriptions(CArgDescriptions& arg_desc);
    /** Interface method, \sa IBlastCmdLineArgs::SetArgumentDescriptions 
     * @note this depends on the matrix already being set...
     */
    virtual void ExtractAlgorithmOptions(const CArgs& cmd_line_args, 
                                         CBlastOptions& options);
};

/** Argument class to retrieve and set the scoring matrix name BLAST algorithm
 * option */
class NCBI_XBLAST_EXPORT CMatrixNameArg : public IBlastCmdLineArgs
{
public:
    /** Interface method, \sa IBlastCmdLineArgs::SetArgumentDescriptions */
    virtual void SetArgumentDescriptions(CArgDescriptions& arg_desc);
    /** Interface method, \sa IBlastCmdLineArgs::SetArgumentDescriptions */
    virtual void ExtractAlgorithmOptions(const CArgs& cmd_line_args, 
                                         CBlastOptions& options);
};

/** Argument class for general search BLAST algorithm options: evalue, gap
 * penalties, query filter string, ungapped x-drop, initial and final gapped 
 * x-drop, word size, percent identity, and effective search space
 */
class NCBI_XBLAST_EXPORT CGenericSearchArgs : public IBlastCmdLineArgs
{
public:
    /** 
     * @brief Constructor
     * 
     * @param query_is_protein is the query sequence(s) protein?
     * @param is_rpsblast is it RPS-BLAST?
     */
    CGenericSearchArgs(bool query_is_protein = true, bool is_rpsblast = false)
        : m_QueryIsProtein(query_is_protein), m_IsRpsBlast(is_rpsblast) {}
         
    /** Interface method, \sa IBlastCmdLineArgs::SetArgumentDescriptions */
    virtual void SetArgumentDescriptions(CArgDescriptions& arg_desc);
    /** Interface method, \sa IBlastCmdLineArgs::SetArgumentDescriptions */
    virtual void ExtractAlgorithmOptions(const CArgs& cmd_line_args, 
                                         CBlastOptions& options);
private:
    bool m_QueryIsProtein;  /**< true if the query is protein */
    bool m_IsRpsBlast;      /**< true if the search is RPS-BLAST */
};

/** Argument class for collecting filtering options */
class NCBI_XBLAST_EXPORT CFilteringArgs : public IBlastCmdLineArgs
{
public:
    /** 
     * @brief Constructor
     * 
     * @param query_is_protein is the query sequence(s) protein?
     */
    CFilteringArgs(bool query_is_protein = true)
        : m_QueryIsProtein(query_is_protein) {}

    /** Interface method, \sa IBlastCmdLineArgs::SetArgumentDescriptions */
    virtual void SetArgumentDescriptions(CArgDescriptions& arg_desc);
    /** Interface method, \sa IBlastCmdLineArgs::SetArgumentDescriptions */
    virtual void ExtractAlgorithmOptions(const CArgs& cmd_line_args, 
                                         CBlastOptions& options);
private:
    bool m_QueryIsProtein;  /**< true if the query is protein */

    /** 
     * @brief Auxiliary method to tokenize the filtering string.
     * 
     * @param filtering_args string to tokenize [in]
     * @param output vector with tokens [in|out]
     */
    void x_TokenizeFilteringArgs(const string& filtering_args,
                                 vector<string>& output) const;
};

/// Defines values for match and mismatch in nucleotide comparisons as well as
/// non-greedy extension
class NCBI_XBLAST_EXPORT CNuclArgs : public IBlastCmdLineArgs
{
public:
    /** Interface method, \sa IBlastCmdLineArgs::SetArgumentDescriptions */
    virtual void SetArgumentDescriptions(CArgDescriptions& arg_desc);
    /** Interface method, \sa IBlastCmdLineArgs::SetArgumentDescriptions */
    virtual void ExtractAlgorithmOptions(const CArgs& cmd_line_args, 
                                         CBlastOptions& options);
};

/// Argument class to retrieve discontiguous megablast arguments
class NCBI_XBLAST_EXPORT CDiscontiguousMegablastArgs : public IBlastCmdLineArgs
{
public:
    /** Interface method, \sa IBlastCmdLineArgs::SetArgumentDescriptions */
    virtual void SetArgumentDescriptions(CArgDescriptions& arg_desc);
    /** Interface method, \sa IBlastCmdLineArgs::SetArgumentDescriptions */
    virtual void ExtractAlgorithmOptions(const CArgs& cmd_line_args, 
                                         CBlastOptions& options);

    /// Value to specify coding template type
    static const string kTemplType_Coding;
    /// Value to specify optimal template type
    static const string kTemplType_Optimal;
    /// Value to specify coding+optimal template type
    static const string kTemplType_CodingAndOptimal;
};

/** Argument class for collecting composition based statistics options */
class NCBI_XBLAST_EXPORT CCompositionBasedStatsArgs : public IBlastCmdLineArgs
{
public:
    /** Interface method, \sa IBlastCmdLineArgs::SetArgumentDescriptions */
    virtual void SetArgumentDescriptions(CArgDescriptions& arg_desc);
    /** Interface method, \sa IBlastCmdLineArgs::SetArgumentDescriptions */
    virtual void ExtractAlgorithmOptions(const CArgs& cmd_line_args, 
                                         CBlastOptions& options);
};

/** Argument class for collecting gapped options */
class NCBI_XBLAST_EXPORT CGappedArgs : public IBlastCmdLineArgs
{
public:
    /** Interface method, \sa IBlastCmdLineArgs::SetArgumentDescriptions */
    virtual void SetArgumentDescriptions(CArgDescriptions& arg_desc);
    /** Interface method, \sa IBlastCmdLineArgs::SetArgumentDescriptions */
    virtual void ExtractAlgorithmOptions(const CArgs& cmd_line_args, 
                                         CBlastOptions& options);
};

/** Argument class for collecting the largest intron size */
class NCBI_XBLAST_EXPORT CLargestIntronSizeArgs : public IBlastCmdLineArgs
{
public:
    /** Interface method, \sa IBlastCmdLineArgs::SetArgumentDescriptions */
    virtual void SetArgumentDescriptions(CArgDescriptions& arg_desc);
    /** Interface method, \sa IBlastCmdLineArgs::SetArgumentDescriptions */
    virtual void ExtractAlgorithmOptions(const CArgs& cmd_line_args, 
                                         CBlastOptions& options);
};

/// Argument class to collect the frame shift penalty for out-of-frame searches
class NCBI_XBLAST_EXPORT CFrameShiftArgs : public IBlastCmdLineArgs
{
public:
    /** Interface method, \sa IBlastCmdLineArgs::SetArgumentDescriptions */
    virtual void SetArgumentDescriptions(CArgDescriptions& arg_desc);
    /** Interface method, \sa IBlastCmdLineArgs::SetArgumentDescriptions */
    virtual void ExtractAlgorithmOptions(const CArgs& cmd_line_args, 
                                         CBlastOptions& options);
};

/// Argument class to collect the genetic code for all queries/subjects
class NCBI_XBLAST_EXPORT CGeneticCodeArgs : public IBlastCmdLineArgs
{
public:
    /// Enumeration defining which sequences the genetic code applies to
    enum ETarget {
        eQuery,         ///< Query genetic code
        eDatabase       ///< Database genetic code
    };


    /** 
     * @brief Constructor
     * 
     * @param t genetic code target (query or database)
     */
    CGeneticCodeArgs(ETarget t) : m_Target(t) {};

    /** Interface method, \sa IBlastCmdLineArgs::SetArgumentDescriptions */
    virtual void SetArgumentDescriptions(CArgDescriptions& arg_desc);
    /** Interface method, \sa IBlastCmdLineArgs::SetArgumentDescriptions */
    virtual void ExtractAlgorithmOptions(const CArgs& cmd_line_args, 
                                         CBlastOptions& options);

private:
    ETarget m_Target; ///< Genetic code target
};

class NCBI_XBLAST_EXPORT CDecline2AlignArgs : public IBlastCmdLineArgs
{
public:
    /** 
     * @brief Constructor
     * 
     * @param default_value for this option
     */
    CDecline2AlignArgs(int default_value = 0)
        : m_DefaultValue(default_value) {}
    /** Interface method, \sa IBlastCmdLineArgs::SetArgumentDescriptions */
    virtual void SetArgumentDescriptions(CArgDescriptions& arg_desc);
    /** Interface method, \sa IBlastCmdLineArgs::SetArgumentDescriptions */
    virtual void ExtractAlgorithmOptions(const CArgs& cmd_line_args, 
                                         CBlastOptions& options);
private:
    /// Default value specified in constructor
    int m_DefaultValue;
};

/// Argument class to retrieve the gap trigger option
class NCBI_XBLAST_EXPORT CGapTriggerArgs : public IBlastCmdLineArgs
{
public:
    /** 
     * @brief Constructor
     * 
     * @param query_is_protein is the query sequence(s) protein?
     */
    CGapTriggerArgs(bool query_is_protein) 
        : m_QueryIsProtein(query_is_protein) {}
    /** Interface method, \sa IBlastCmdLineArgs::SetArgumentDescriptions */
    virtual void SetArgumentDescriptions(CArgDescriptions& arg_desc);
    /** Interface method, \sa IBlastCmdLineArgs::SetArgumentDescriptions */
    virtual void ExtractAlgorithmOptions(const CArgs& cmd_line_args, 
                                         CBlastOptions& options);
private:
    bool m_QueryIsProtein;  /**< true if the query is protein */
};

/// Argument class to collect PSSM engine options
class NCBI_XBLAST_EXPORT CPssmEngineArgs : public IBlastCmdLineArgs
{
public:
    /** Interface method, \sa IBlastCmdLineArgs::SetArgumentDescriptions */
    virtual void SetArgumentDescriptions(CArgDescriptions& arg_desc);
    /** Interface method, \sa IBlastCmdLineArgs::SetArgumentDescriptions */
    virtual void ExtractAlgorithmOptions(const CArgs& cmd_line_args, 
                                         CBlastOptions& options);
};

/// Argument class to import/export the search strategy
class NCBI_XBLAST_EXPORT CSearchStrategyArgs : public IBlastCmdLineArgs
{
public:
    /** Interface method, \sa IBlastCmdLineArgs::SetArgumentDescriptions */
    virtual void SetArgumentDescriptions(CArgDescriptions& arg_desc);
    /** Interface method, \sa IBlastCmdLineArgs::SetArgumentDescriptions */
    virtual void ExtractAlgorithmOptions(const CArgs& cmd_line_args, 
                                         CBlastOptions& options);
};

/// Argument class to collect options specific to PSI-BLAST
class NCBI_XBLAST_EXPORT CPsiBlastArgs : public IBlastCmdLineArgs
{
public:
    /// Enumeration to determine the molecule type of the database
    enum ETargetDatabase {
        eProteinDb,         ///< Traditional, iterated PSI-BLAST
        eNucleotideDb       ///< PSI-Tblastn, non-iterated
    };

    /** 
     * @brief Constructor
     * 
     * @param db_target Molecule type of the database
     */
    CPsiBlastArgs(ETargetDatabase db_target = eProteinDb) 
        : m_DbTarget(db_target), m_NumIterations(1),
        m_CheckPointOutputStream(0), m_AsciiMatrixOutputStream(0),
        m_CheckPointInputStream(0), m_MSAInputStream(0)
    {};

    /// Our virtual destructor
    virtual ~CPsiBlastArgs() {
        if (m_CheckPointOutputStream) {
            delete m_CheckPointOutputStream;
        }
        if (m_AsciiMatrixOutputStream) {
            delete m_AsciiMatrixOutputStream;
        }
        if (m_CheckPointInputStream) {
            delete m_CheckPointInputStream;
        }
        if (m_MSAInputStream) {
            delete m_MSAInputStream;
        }
    }

    /** Interface method, \sa IBlastCmdLineArgs::SetArgumentDescriptions */
    virtual void SetArgumentDescriptions(CArgDescriptions& arg_desc);
    /** Interface method, \sa IBlastCmdLineArgs::SetArgumentDescriptions */
    virtual void ExtractAlgorithmOptions(const CArgs& cmd_line_args, 
                                         CBlastOptions& options);

    /// Retrieve the number of iterations to perform
    size_t GetNumberOfIterations() const { 
        return m_NumIterations; 
    }
    /// Get the checkpoint file output stream
    CNcbiOstream* GetCheckPointOutputStream() const { 
        return m_CheckPointOutputStream; 
    }
    /// Get the ASCII matrix output stream
    CNcbiOstream* GetAsciiMatrixFile() const { 
        return m_AsciiMatrixOutputStream; 
    }
    /// Get the multiple sequence alignment input stream
    CNcbiIstream* GetMSAInputFile() const { 
        return m_MSAInputStream; 
    }
    /// Get the checkpoint file input stream
    /// @note this is valid in both eProteinDb and eNucleotideDb
    CNcbiIstream* GetCheckPointInputStream() const { 
        return m_CheckPointInputStream; 
    }

    /// Get the PSSM read from checkpoint file
    CRef<objects::CPssmWithParameters> GetPssm() const {
        return m_Pssm;
    }

private:
    /// Molecule of the database
    ETargetDatabase m_DbTarget;
    /// number of iterations to perform
    size_t m_NumIterations;
    /// checkpoint output file
    CNcbiOstream* m_CheckPointOutputStream;
    /// ASCII matrix output file
    CNcbiOstream* m_AsciiMatrixOutputStream;
    /// checkpoint input file
    CNcbiIstream* m_CheckPointInputStream;
    /// multiple sequence alignment input file
    CNcbiIstream* m_MSAInputStream;
    /// PSSM
    CRef<objects::CPssmWithParameters> m_Pssm;
};

/// Argument class to collect options specific to PHI-BLAST
class NCBI_XBLAST_EXPORT CPhiBlastArgs : public IBlastCmdLineArgs
{
public:
    /** Interface method, \sa IBlastCmdLineArgs::SetArgumentDescriptions */
    virtual void SetArgumentDescriptions(CArgDescriptions& arg_desc);
    /** Interface method, \sa IBlastCmdLineArgs::SetArgumentDescriptions */
    virtual void ExtractAlgorithmOptions(const CArgs& cmd_line_args, 
                                         CBlastOptions& options);
};

/*****************************************************************************/
// Input options

/// Argument class to collect query options
class NCBI_XBLAST_EXPORT CQueryOptionsArgs : public IBlastCmdLineArgs
{
public:
    /** 
     * @brief Constructor
     * 
     * @param query_cannot_be_nucl can the query not be nucleotide?
     */
    CQueryOptionsArgs(bool query_cannot_be_nucl = false)
        : m_Strand(objects::eNa_strand_unknown), m_Range(),
        m_UseLCaseMask(false), m_BelieveQueryDefline(false),
        m_QueryCannotBeNucl(query_cannot_be_nucl)
    {};

    /** Interface method, \sa IBlastCmdLineArgs::SetArgumentDescriptions */
    virtual void SetArgumentDescriptions(CArgDescriptions& arg_desc);
    /** Interface method, \sa IBlastCmdLineArgs::SetArgumentDescriptions */
    virtual void ExtractAlgorithmOptions(const CArgs& cmd_line_args, 
                                         CBlastOptions& options);

    /// Get query sequence range restriction
    TSeqRange GetRange() const { return m_Range; }
    /// Get strand to search in query sequence(s)
    objects::ENa_strand GetStrand() const { return m_Strand; }
    /// Use lowercase masking in FASTA input?
    bool UseLowercaseMasks() const { return m_UseLCaseMask; }
    /// Should the defline be parsed?
    bool BelieveQueryDefline() const { return m_BelieveQueryDefline; }

    // Is the query sequence protein?
    bool QueryIsProtein() const { return m_QueryCannotBeNucl; }
private:
    /// Strand(s) to search
    objects::ENa_strand m_Strand;
    /// range to restrict the query sequence(s)
    TSeqRange m_Range;
    /// use lowercase masking in FASTA input
    bool m_UseLCaseMask;
    /// Should the defline be parsed?
    bool m_BelieveQueryDefline;

    /// only false for blast[xn], and tblastx
    /// true in case of PSI-BLAST
    bool m_QueryCannotBeNucl;  
};

/// Argument class to collect database/subject arguments
class NCBI_XBLAST_EXPORT CBlastDatabaseArgs : public IBlastCmdLineArgs
{
public:
    /// alias for a list of gis
    typedef CSearchDatabase::TGiList TGiList;
    /// alias for the database molecule type
    typedef CSearchDatabase::EMoleculeType EMoleculeType;

    /// Constructor
    /// @param request_mol_type If true, the command line arguments will
    /// include a mandatory option to disambiguate whether a protein or a
    /// nucleotide database is searched
    /// @param is_rpsblast is it RPS-BLAST?
    CBlastDatabaseArgs(bool request_mol_type = false, bool is_rpsblast = false);
    /** Interface method, \sa IBlastCmdLineArgs::SetArgumentDescriptions */
    virtual void SetArgumentDescriptions(CArgDescriptions& arg_desc);
    /** Interface method, \sa IBlastCmdLineArgs::SetArgumentDescriptions */
    virtual void ExtractAlgorithmOptions(const CArgs& args, 
                                         CBlastOptions& opts);

    /// Is the database/subject protein?
    bool IsProtein() const;
    /// Get the gi list file name
    /// The return value of this should be set in the CSeqDb ctor
    string GetGiListFileName() const { return m_GiListFileName; }
    /// Get the BLAST database name
    string GetDatabaseName() const { return m_SearchDb->GetDatabaseName(); }

    /// Retrieve the search database information
    CRef<CSearchDatabase> GetSearchDatabase() const { return m_SearchDb; }

private:
    CRef<CSearchDatabase> m_SearchDb;/**< Description of the BLAST database */
    string m_GiListFileName;        /**< File name of gi list DB restriction */
    bool m_RequestMoleculeType;     /**< Determines whether the database's
                                      molecule type should be requested in the
                                      command line, true in case of PSI-BLAST
                                      */
    bool m_IsRpsBlast;              /**< true if the search is RPS-BLAST */
};

/// Argument class to collect formatting options, use this to create a 
/// CBlastFormat object
class NCBI_XBLAST_EXPORT CFormattingArgs : public IBlastCmdLineArgs
{
public:
    /// Default constructor
    CFormattingArgs()
        : m_FormattedOutputChoice(0), m_ShowGis(false), 
        m_NumDescriptions(kDfltArgNumDescriptions),
        m_NumAlignments(kDfltArgNumAlignments), m_Html(false)
    {};

    /** Interface method, \sa IBlastCmdLineArgs::SetArgumentDescriptions */
    virtual void SetArgumentDescriptions(CArgDescriptions& arg_desc);
    /** Interface method, \sa IBlastCmdLineArgs::SetArgumentDescriptions */
    virtual void ExtractAlgorithmOptions(const CArgs& args, 
                                         CBlastOptions& opts);

    /// Get the choice of formatted output
    int GetFormattedOutputChoice() const {
        return m_FormattedOutputChoice;
    }
    /// Display the NCBI GIs in formatted output?
    bool ShowGis() const {
        return m_ShowGis;
    }
    /// Number of one-line descriptions to show in traditional BLAST output
    size_t GetNumDescriptions() const {
        return m_NumDescriptions;
    }
    /// Number of alignments to show in traditional BLAST output
    size_t GetNumAlignments() const {
        return m_NumAlignments;
    }
    /// Display HTML output?
    bool DisplayHtmlOutput() const {
        return m_Html;
    }

private:
    int m_FormattedOutputChoice;    ///< Choice of formatting output
    bool m_ShowGis;                 ///< Display NCBI GIs?
    size_t m_NumDescriptions;       ///< Number of 1-line descr. to show
    size_t m_NumAlignments;         ///< Number of alignments to show
    bool m_Html;                    ///< Display HTML output?
};

/// Argument class to collect multi-threaded arguments
class NCBI_XBLAST_EXPORT CMTArgs : public IBlastCmdLineArgs
{
public:
    /// Default Constructor
    CMTArgs() : m_NumThreads(CThreadable::kMinNumThreads) {}
    /** Interface method, \sa IBlastCmdLineArgs::SetArgumentDescriptions */
    virtual void SetArgumentDescriptions(CArgDescriptions& arg_desc);
    /** Interface method, \sa IBlastCmdLineArgs::SetArgumentDescriptions */
    virtual void ExtractAlgorithmOptions(const CArgs& cmd_line_args, 
                                         CBlastOptions& options);

    /// Get the number of threads to spawn
    size_t GetNumThreads() const { return m_NumThreads; }
private:
    size_t m_NumThreads;        ///< Number of threads to spawn
};

/// Argument class to collect remote vs. local execution
class NCBI_XBLAST_EXPORT CRemoteArgs : public IBlastCmdLineArgs
{
public:
    CRemoteArgs() : m_IsRemote(false), m_DebugOutput(false) {}
    /** Interface method, \sa IBlastCmdLineArgs::SetArgumentDescriptions */
    virtual void SetArgumentDescriptions(CArgDescriptions& arg_desc);
    /** Interface method, \sa IBlastCmdLineArgs::SetArgumentDescriptions */
    virtual void ExtractAlgorithmOptions(const CArgs& cmd_line_args, 
                                         CBlastOptions& options);

    /// Return whether the search should be executed remotely or not
    bool ExecuteRemotely() const { return m_IsRemote; }
    /// Return whether debug (verbose) output should be produced on remote
    /// searches (only available when compiled with _DEBUG)
    bool ProduceDebugRemoteOutput() const { return m_DebugOutput; }
private:

    /// Should the search be executed remotely?
    bool m_IsRemote;
    /// Should debugging (verbose) output be printed
    bool m_DebugOutput;
};

/// Argument class to retrieve calling options
class NCBI_XBLAST_EXPORT CCullingArgs : public IBlastCmdLineArgs
{
public:
    /** Interface method, \sa IBlastCmdLineArgs::SetArgumentDescriptions */
    virtual void SetArgumentDescriptions(CArgDescriptions& arg_desc);
    /** Interface method, \sa IBlastCmdLineArgs::SetArgumentDescriptions */
    virtual void ExtractAlgorithmOptions(const CArgs& args, 
                                         CBlastOptions& opts);
};

/// Type definition of a container of IBlastCmdLineArgs
typedef vector< CRef<IBlastCmdLineArgs> > TBlastCmdLineArgs;


/// Base command line argument class for a generic BLAST command line binary
class NCBI_XBLAST_EXPORT CBlastAppArgs : public CObject
{
public:
    /// Default constructor
    CBlastAppArgs();
    /// Our virtual destructor
    virtual ~CBlastAppArgs() {}

    /// Set the command line arguments
    CArgDescriptions* SetCommandLine();

    /// Extract the command line arguments into a CBlastOptionsHandle object
    /// @param args Commad line arguments [in]
    CRef<CBlastOptionsHandle> SetOptions(const CArgs& args);

    /// Get the BLAST database arguments
    CRef<CBlastDatabaseArgs> GetBlastDatabaseArgs() const {
        return m_BlastDbArgs;
    }

    /// Get the options for the query sequence(s)
    CRef<CQueryOptionsArgs> GetQueryOptionsArgs() const {
        return m_QueryOptsArgs;
    }

    /// Get the formatting options
    CRef<CFormattingArgs> GetFormattingArgs() const {
        return m_FormattingArgs;
    }

    /// Get the number of threads to spawn
    size_t GetNumThreads() const {
        return m_MTArgs->GetNumThreads();
    }

    /// Get the input stream
    CNcbiIstream& GetInputStream() const {
        return m_StdCmdLineArgs->GetInputStream();
    }
    /// Get the output stream
    CNcbiOstream& GetOutputStream() const {
        return m_StdCmdLineArgs->GetOutputStream();
    }

    /// Determine whether the search should be executed remotely or not
    bool ExecuteRemotely() const {
        return m_RemoteArgs->ExecuteRemotely();
    }

    /// Return whether debug (verbose) output should be produced on remote
    /// searches (only available when compiled with _DEBUG)
    bool ProduceDebugRemoteOutput() const { 
        return m_RemoteArgs->ProduceDebugRemoteOutput();
    }

    /// Get the query batch size
    virtual int GetQueryBatchSize() const = 0;

protected:
    /// Set of command line argument objects
    TBlastCmdLineArgs m_Args;
    /// query options object
    CRef<CQueryOptionsArgs> m_QueryOptsArgs;
    /// database/subject object
    CRef<CBlastDatabaseArgs> m_BlastDbArgs;
    /// formatting options
    CRef<CFormattingArgs> m_FormattingArgs;
    /// multi-threaded options
    CRef<CMTArgs> m_MTArgs;
    /// remote vs. local execution options
    CRef<CRemoteArgs> m_RemoteArgs;
    /// standard command line arguments class
    CRef<CStdCmdLineArgs> m_StdCmdLineArgs;

    /// Create the options handle based on the command line arguments
    /// @param locality whether the search will be executed locally or remotely
    /// [in]
    /// @param args command line arguments [in]
    virtual CRef<CBlastOptionsHandle>
    x_CreateOptionsHandle(CBlastOptions::EAPILocality locality,
                          const CArgs& args) = 0;
};

END_SCOPE(blast)
END_NCBI_SCOPE

#endif  /* ALGO_BLAST_BLASTINPUT___BLAST_ARGS__HPP */
