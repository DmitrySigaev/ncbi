#ifndef ALGO_BLAST_API___BLAST_RPS_OPTIONS__HPP
#define ALGO_BLAST_API___BLAST_RPS_OPTIONS__HPP

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
 * Authors:  Tom Madden
 *
 */

/// @file blast_rps_options.hpp
/// Declares the CBlastRPSOptionsHandle class.


#include <algo/blast/api/blast_prot_options.hpp>

/** @addtogroup AlgoBlast
 *
 * @{
 */

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(blast)

/// Handle to the rpsblast options to the BLAST algorithm.
///
/// Adapter class for rpsblast BLAST comparisons.
/// Exposes an interface to allow manipulation the options that are relevant to
/// this type of search.

class NCBI_XBLAST_EXPORT CBlastRPSOptionsHandle : public CBlastOptionsHandle
{
public:
    
    /// Creates object with default options set
    CBlastRPSOptionsHandle(EAPILocality locality = CBlastOptions::eLocal);
    ~CBlastRPSOptionsHandle() {}

    /******************* Lookup table options ***********************/
    /// Returns WordThreshold
    int GetWordThreshold() const { return m_Opts->GetWordThreshold(); }
    /// Returns WordSize
    short GetWordSize() const { return m_Opts->GetWordSize(); }

    /******************* Initial word options ***********************/
    /// Returns WindowSize
    int GetWindowSize() const { return m_Opts->GetWindowSize(); }
    /// Sets WindowSize
    /// @param ws WindowSize [in]
    void SetWindowSize(int ws) { m_Opts->SetWindowSize(ws); }

    /// Returns XDropoff
    double GetXDropoff() const { return m_Opts->GetXDropoff(); } 
    /// Sets XDropoff
    /// @param x XDropoff [in]
    void SetXDropoff(double x) { m_Opts->SetXDropoff(x); }

    /******************* Gapped extension options *******************/
    /// Returns GapXDropoffFinal
    double GetGapXDropoffFinal() const { 
        return m_Opts->GetGapXDropoffFinal(); 
    }
    /// Sets GapXDropoffFinal
    /// @param x GapXDropoffFinal [in]
    void SetGapXDropoffFinal(double x) { m_Opts->SetGapXDropoffFinal(x); }

    /************************ Scoring options ************************/
    /// Returns GapOpeningCost
    int GetGapOpeningCost() const { return m_Opts->GetGapOpeningCost(); }
    /// Returns GapExtensionCost
    int GetGapExtensionCost() const { return m_Opts->GetGapExtensionCost(); }

    /// Returns EffectiveSearchSpace
    Int8 GetEffectiveSearchSpace() const { 
        return m_Opts->GetEffectiveSearchSpace(); 
    }
    /// Sets EffectiveSearchSpace
    /// @param eff EffectiveSearchSpace [in]
    void SetEffectiveSearchSpace(Int8 eff) {
        m_Opts->SetEffectiveSearchSpace(eff);
    }

    /// Returns UseRealDbSize
    bool GetUseRealDbSize() const { return m_Opts->GetUseRealDbSize(); }
    /// Sets UseRealDbSize
    /// @param u UseRealDbSize [in]
    void SetUseRealDbSize(bool u = true) { m_Opts->SetUseRealDbSize(u); }

protected:
    /// Overrides LookupTableDefaults for RPS-BLAST options
    virtual void SetLookupTableDefaults();
    /// Overrides QueryOptionDefaults for RPS-BLAST options
    virtual void SetQueryOptionDefaults();
    /// Overrides InitialWordOptionsDefaults for RPS-BLAST options
    virtual void SetInitialWordOptionsDefaults();
    /// Overrides GappedExtensionDefaults for RPS-BLAST options
    virtual void SetGappedExtensionDefaults();
    /// Overrides ScoringOptionsDefaults for RPS-BLAST options
    virtual void SetScoringOptionsDefaults();
    /// Overrides HitSavingOptionsDefaults for RPS-BLAST options
    virtual void SetHitSavingOptionsDefaults();
    /// Overrides EffectiveLengthsOptionsDefaults for RPS-BLAST options
    virtual void SetEffectiveLengthsOptionsDefaults();
    /// Overrides SubjectSequenceOptionsDefaults for RPS-BLAST options
    virtual void SetSubjectSequenceOptionsDefaults(); 

private:
    /// Disallow copy constructor
    CBlastRPSOptionsHandle(const CBlastRPSOptionsHandle& rhs);
    /// Disallow assignment operator
    CBlastRPSOptionsHandle& operator=(const CBlastRPSOptionsHandle& rhs);
};

END_SCOPE(blast)
END_NCBI_SCOPE


/* @} */


/*
 * ===========================================================================
 * $Log$
 * Revision 1.6  2004/06/08 22:41:04  camacho
 * Add missing doxygen comments
 *
 * Revision 1.5  2004/05/04 13:09:20  camacho
 * Made copy-ctor & assignment operator private
 *
 * Revision 1.4  2004/04/23 13:55:29  papadopo
 * derived BlastRPSOptionsHandle from BlastOptions (again)
 *
 * Revision 1.3  2004/04/16 14:31:49  papadopo
 * make this class a derived class of CBlastProteinOptionsHandle, that corresponds to the eRPSBlast program
 *
 * Revision 1.2  2004/03/19 14:53:24  camacho
 * Move to doxygen group AlgoBlast
 *
 * Revision 1.1  2004/03/10 14:52:34  madden
 * Options handle for RPSBlast searches
 *
 *
 * ===========================================================================
 */

#endif  /* ALGO_BLAST_API___BLAST_RPS_OPTIONS__HPP */
