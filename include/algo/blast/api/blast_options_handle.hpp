#ifndef ALGO_BLAST_API___BLAST_OPTIONS_HANDLE__HPP
#define ALGO_BLAST_API___BLAST_OPTIONS_HANDLE__HPP

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
 * Authors:  Christiam Camacho
 *
 */

/// @file blast_options_handle.hpp
/// Declares the CBlastOptionsHandle and CBlastOptionsFactory classes.

#include <algo/blast/api/blast_options.hpp>

/** @addtogroup Miscellaneous
 *
 * @{
 */


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(blast)
class CBlastException;

class CBlastOptionsHandle;

/*!
* Creates BlastOptionsHandle objects with default values for the 
* programs/tasks requested.
*
* Concrete factory to create various properly configured BLAST options 
* objects with default values, given a program (or task). 
*
* Example:
* \code
* ...
* CBlastOptionsHandle* opts = CBlastOptionsFactory::Create(eBlastn);
* CBl2Seq blaster(query, subject, opts);
* TSeqAlignVector results = blaster.Run();
* ...
* blaster.SetOptionsHandle(CBlastOptionsFactory::Create(eMegablast));
* results = blaster.Run();
* ...
* blaster.SetOptionsHandle(CBlastOptionsFactory::Create(eDiscMegablast));
* results = blaster.Run();
* ...
* \endcode
*/

class NCBI_XBLAST_EXPORT CBlastOptionsFactory
{
public:
    static CBlastOptionsHandle* Create(EProgram program) 
        THROWS((CBlastException));

private:
    CBlastOptionsFactory();
};

/// Handle to the options to the BLAST algorithm.
///
/// This abstract base class only defines those options that are truly 
/// "universal" BLAST options (they apply to all flavors of BLAST).
/// Derived classes define options that are applicable only to those programs
/// whose options they manipulate.

class NCBI_XBLAST_EXPORT CBlastOptionsHandle : public CObject
{
public:
    CBlastOptionsHandle();

    /// Return the object which this object is a handle for.
    ///
    /// Assumes user knows exactly how to set the individual options 
    /// correctly.
    const CBlastOptions& GetOptions() const { return *m_Opts; }
    CBlastOptions& SetOptions() { return *m_Opts; }

    /// Resets the state of the object to all default values.
    /// This is a template method (design pattern).
    virtual void SetDefaults();

    /******************* Lookup table options ***********************/
    int GetAlphabetSize() const { return m_Opts->GetAlphabetSize(); }
    void SetAlphabetSize(int asz) { m_Opts->SetAlphabetSize(asz); }

    /******************* Query setup options ************************/
    const char* GetFilterString() const { return m_Opts->GetFilterString(); }
    void SetFilterString(const char* f) { m_Opts->SetFilterString(f); }

    /******************* Gapped extension options *******************/
    double GetGapXDropoff() const { return m_Opts->GetGapXDropoff(); }
    void SetGapXDropoff(double x) { m_Opts->SetGapXDropoff(x); }

    double GetGapTrigger() const { return m_Opts->GetGapTrigger(); }
    void SetGapTrigger(double g) { m_Opts->SetGapTrigger(g); }

    /******************* Hit saving options *************************/
    int GetHitlistSize() const { return m_Opts->GetHitlistSize(); }
    void SetHitlistSize(int s) { m_Opts->SetHitlistSize(s); }

    int GetMaxNumHspPerSequence() const { 
        return m_Opts->GetMaxNumHspPerSequence();
    }
    void SetMaxNumHspPerSequence(int m) { m_Opts->SetMaxNumHspPerSequence(m); }

    // These 2 are never set in core... should they be removed?
    //int GetTotalHspLimit() const { return m_Opts->GetTotalHspLimit(); }
    //void SetTotalHspLimit(int l) { m_Opts->SetTotalHspLimit(l); }

    double GetEvalueThreshold() const { return m_Opts->GetEvalueThreshold(); }
    void SetEvalueThreshold(double eval) { m_Opts->SetEvalueThreshold(eval); } 
    int GetCutoffScore() const { return m_Opts->GetCutoffScore(); }
    void SetCutoffScore(int s) { m_Opts->SetCutoffScore(s); }

    double GetPercentIdentity() const { return m_Opts->GetPercentIdentity(); }
    void SetPercentIdentity(double p) { m_Opts->SetPercentIdentity(p); }

    bool GetGappedMode() const { return m_Opts->GetGappedMode(); }
    void SetGappedMode(bool m = true) { m_Opts->SetGappedMode(m); }

    /************************ Scoring options ************************/
    const char* GetMatrixName() const { return m_Opts->GetMatrixName(); }
    void SetMatrixName(const char* matrix) { m_Opts->SetMatrixName(matrix); }

    const char* GetMatrixPath() const { return m_Opts->GetMatrixPath(); }
    void SetMatrixPath(const char* path) { m_Opts->SetMatrixPath(path); }

    int GetGapOpeningCost() const { return m_Opts->GetGapOpeningCost(); }
    void SetGapOpeningCost(int g) { m_Opts->SetGapOpeningCost(g); }

    int GetGapExtensionCost() const { return m_Opts->GetGapExtensionCost(); }
    void SetGapExtensionCost(int e) { m_Opts->SetGapExtensionCost(e); }

protected:

    /// Data type this class controls access to
    CRef<CBlastOptions> m_Opts;

    // These methods make up the template method
    virtual void SetLookupTableDefaults() = 0;
    virtual void SetQueryOptionDefaults() = 0;
    virtual void SetInitialWordOptionsDefaults() = 0;
    virtual void SetGappedExtensionDefaults() = 0;
    virtual void SetScoringOptionsDefaults() = 0;
    virtual void SetHitSavingOptionsDefaults() = 0;
    virtual void SetEffectiveLengthsOptionsDefaults() = 0;
    virtual void SetSubjectSequenceOptionsDefaults() = 0;
};

END_SCOPE(blast)
END_NCBI_SCOPE


/* @} */


/*
 * ===========================================================================
 * $Log$
 * Revision 1.3  2003/12/09 12:40:22  camacho
 * Added windows export specifiers
 *
 * Revision 1.2  2003/11/26 18:36:44  camacho
 * Renaming blast_option*pp -> blast_options*pp
 *
 * Revision 1.1  2003/11/26 18:22:15  camacho
 * +Blast Option Handle classes
 *
 * ===========================================================================
 */

#endif  /* ALGO_BLAST_API___BLAST_OPTIONS_HANDLE__HPP */
